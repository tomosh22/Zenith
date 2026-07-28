#include "Zenith.h"

#include "Zenithmon/Components/ZM_BattleDirector.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleArena.h"          // ZM_BattleArena (arena resolve + platform ids)
#include "Zenithmon/Components/ZM_BattleTransition.h"      // ZM_BattleTransition (payload + RequestBattleEnd)
#include "Zenithmon/Components/ZM_GameStateManager.h"      // TryGetGameState (real party lead + write-back target)
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"           // ZM_AI_TIER_GREEDY
#include "Zenithmon/Source/Battle/ZM_TrainerBattle.h"      // ZM_BuildTrainerBattleSetup (SC5 trainer arm)
#include "Zenithmon/Source/Data/ZM_ItemData.h"             // ZM_ITEM_ID / ZM_ITEM_CATCHORB (SC4 catch-ball seam)
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"           // ZM_CreatureAssetPath, ZM_CREATURE_ASSET_MODEL
#include "Zenithmon/Source/Party/ZM_GameState.h"           // ZM_GameState (party lead read + write-back)
#include "Zenithmon/Source/Party/ZM_Monster.h"             // ZM_MonsterToBattleSpec (real lead -> battle seed)
#include "Zenithmon/Source/Party/ZM_BattleWriteBack.h"     // ZM_ApplyBattleResultToParty (SC3 write-back)

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_BattleDirector (S5 item 4 SC3, order 111). The ECS presenter-driver that
// binds the pure ZM_BattleDirectorCore into the additively-loaded Battle scene:
// watch the persistent transition, one-shot Begin a deterministic AI-vs-AI wild
// battle, place two creature models on the arena platforms, drive the core turn
// by turn, and end the round-trip via ZM_BattleTransition::RequestBattleEnd()
// exactly once; it also owns and drives the ZM_UI_BattleHUD (SC4). ZM-D-102/103.
// ============================================================================

// A fixed valid starter (dex row 0). BuildPlaceholderPlayerSpec pins this so the
// player spec is deterministic and reproducible; a starter at level 5 always has a
// level-1 same-type move (ZM_GetSpeciesLearnset guarantees a level-1 STAB pick), so
// m_aeMoves[0] != ZM_MOVE_NONE.
static const ZM_SPECIES_ID s_ePLACEHOLDER_PLAYER_SPECIES = ZM_SPECIES_FERNFAWN;
static const u_int         s_uPLACEHOLDER_PLAYER_LEVEL   = 5u;

// Test-only catch-ball override (S5 item 5 SC4). Mirrors ZM_SetInstantBattlesForTests: a
// process-lifetime static the RUNNING drive substitutes onto any ITEM (catch) action the
// player submits. Default ZM_ITEM_CATCHORB -> the production path is an identity no-op; the
// windowed catch test sets ZM_ITEM_PRIMEORB (guaranteed capture) for a deterministic catch.
static ZM_ITEM_ID g_eCatchBallForTests = ZM_ITEM_CATCHORB;
void ZM_SetCatchBallForTests(ZM_ITEM_ID eBall) { g_eCatchBallForTests = eBall; }

ZM_BattleDirector::ZM_BattleDirector(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_BattleDirector::OnStart()
{
	// Dormant until the persistent transition reaches IN_BATTLE. Start from a clean
	// baseline (defensive; a fresh instance already defaults here).
	m_ePhase            = ZM_BD_WAIT_FOR_IN_BATTLE;
	m_bEndRequested     = false;
	m_bWriteBackToLead  = false;
	m_eTrainer          = ZM_TRAINER_NONE;   // SC5: WILD until a trainer arm latches it
	m_fRunningSeconds   = 0.0f;
}

void ZM_BattleDirector::OnUpdate(float fDeltaSeconds)
{
	// -- Locate the persistent transition singleton FRESH every frame. The component
	// pool relocates entries on swap-and-pop, so this pointer must never be cached. --
	Zenith_EntityID xTransitionID = INVALID_ENTITY_ID;
	if (!ZM_BattleTransition::TryGetUniqueSingletonEntityID(xTransitionID))
	{
		return;
	}
	Zenith_Entity xTransitionEntity = g_xEngine.Scenes().ResolveEntity(xTransitionID);
	ZM_BattleTransition* pxTransition =
		xTransitionEntity.IsValid() ? xTransitionEntity.TryGetComponent<ZM_BattleTransition>() : nullptr;
	if (pxTransition == nullptr)
	{
		return;
	}

	const bool bInBattle = (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE);

	// -- 1. One-shot setup when the transition first reaches IN_BATTLE. --
	const bool bAlreadySetUp = (m_ePhase != ZM_BD_WAIT_FOR_IN_BATTLE);
	if (ShouldRunSetup(m_ePhase, bInBattle, bAlreadySetUp))
	{
		RunSetup(*pxTransition);   // -> ZM_BD_RUNNING
	}

	// -- 2. Drive the running battle turn by turn (instant flag drains a whole turn
	//       per Tick; otherwise ops accrue wall-clock time inside the core). --
	if (m_ePhase == ZM_BD_RUNNING)
	{
		m_fRunningSeconds += fDeltaSeconds;

		if (m_xCore.IsAwaitingInput())
		{
			// SC5: the player drives the Fight/Run menu. UpdateMenu returns true exactly
			// when a full action was chosen this frame (MOVE slot or RUN); the core's own
			// AI rng picks the enemy reply (non-perturbing). RUN submits {ZM_ACTION_RUN}
			// and the engine resolves the flee (-> FLEE/FLEE_FAILED -> OVER on success).
			ZM_BattleAction xAction;
			if (m_xHud.UpdateMenu(m_xParentEntity, m_xCore, xAction))
			{
				// SC4 CATCH: swap in the configured catch ball on an ITEM action so the windowed
				// test can force a guaranteed capture (ZM_ITEM_PRIMEORB). In production
				// g_eCatchBallForTests == ZM_ITEM_CATCHORB, so this is an identity no-op.
				if (xAction.m_eKind == ZM_ACTION_ITEM) { xAction.m_eItem = g_eCatchBallForTests; }
				m_xCore.SubmitPlayerAction(xAction);   // MOVE / CATCH item / RUN (engine resolves each)
			}
			// else: wait for the player -- NO auto-submit
		}
		else
		{
			m_xHud.HideMenu(m_xParentEntity);   // the menu only shows while awaiting input
		}
		m_xCore.Tick(fDeltaSeconds);
		m_xHud.Update(m_xParentEntity, m_xCore, fDeltaSeconds);

		if (ShouldRequestEndNow(m_ePhase, m_xCore.ShouldRequestEnd(), m_bEndRequested))
		{
			// The battle resolved (incl. a successful flee reaching OVER): RequestBattleEnd
			// is the SOLE exit from IN_BATTLE. Hide the HUD + menu first so the end-fade
			// never shows them over black. Latch RESOLVED regardless of the return value --
			// a false means the transition already left IN_BATTLE, which is still terminal
			// for us (never re-fire).
			m_xHud.Hide(m_xParentEntity);
			m_xHud.HideMenu(m_xParentEntity);
			// SC3 write-back: the battle LEGITIMATELY resolved. If the player side was
			// built from the real party lead, persist the result (win-only inside; a
			// loss/flee/draw is a no-op). Re-resolve the GameState fresh (never cache).
			if (m_bWriteBackToLead)
			{
				ZM_GameState* pxGS = nullptr;
				if (ZM_GameStateManager::TryGetGameState(pxGS) && pxGS != nullptr)
				{
					ZM_ApplyBattleResultToParty(*pxGS, m_xCore);   // win-only inside; loss/flee no-op
				}
			}
			// SC5 TRAINER PAYOUT: prize money + defeat flag. WIN-ONLY inside the pure
			// helper, which routes through the SAME ZM_ClassifyBattleResult the block
			// above uses -- so a draw, a double-KO or any other third outcome pays
			// nothing. The LOSS half is UNCHANGED: the shipped write-back above is
			// still the single owner of m_bPendingWhiteout.
			//
			// Deliberately NOT gated on m_bWriteBackToLead: money and story flags are
			// GAME-STATE level, not lead level, so a placeholder-player trainer battle
			// still legitimately beat the trainer. A wild battle carries
			// ZM_TRAINER_NONE, so this is a strict no-op on the wild path.
			//
			// It is also deliberately absent from the 30-second deadline arm below: a
			// wedged battle never resolved, so it pays nothing. Fail closed.
			if (ZM_IsRegisteredTrainer(m_eTrainer))
			{
				ZM_GameState* pxTrainerGS = nullptr;
				if (ZM_GameStateManager::TryGetGameState(pxTrainerGS) && pxTrainerGS != nullptr)
				{
					ZM_ApplyTrainerResultToGameState(*pxTrainerGS, m_eTrainer, m_xCore);
				}
			}
			const bool bAccepted = ZM_BattleTransition::RequestBattleEnd();
			m_bEndRequested = true;
			m_ePhase        = ZM_BD_RESOLVED;
			if (!bAccepted)
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"ZM_BattleDirector: RequestBattleEnd returned false (already ended/not IN_BATTLE); latching.");
			}
		}
		else if (m_fRunningSeconds >= fRUNNING_DEADLINE_SECONDS && !m_bEndRequested)
		{
			// Wall-clock safety abort: never softlock the round-trip if the core wedges.
			// (Under zm_instant_battles a battle resolves in a couple of Ticks, so this
			// path is unreachable there.)
			m_xHud.Hide(m_xParentEntity);
			m_xHud.HideMenu(m_xParentEntity);
			const bool bAccepted = ZM_BattleTransition::RequestBattleEnd();
			m_bEndRequested = true;
			m_ePhase        = ZM_BD_RESOLVED;
			if (!bAccepted)
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"ZM_BattleDirector: RequestBattleEnd returned false (already ended/not IN_BATTLE); latching.");
			}
		}
	}

	// -- 3/5. Settle to DONE once the transition has left IN_BATTLE (scene about to
	//         unload). bInBattle is the top-of-frame value, so the frame that fired
	//         RequestBattleEnd still observes RESOLVED; DONE follows next frame. A
	//         RUNNING director whose battle was torn down externally also settles. --
	if (!bInBattle && (m_ePhase == ZM_BD_RESOLVED || m_ePhase == ZM_BD_RUNNING))
	{
		m_xHud.Hide(m_xParentEntity);       // defensive: ensure the HUD is down as the scene unloads
		m_xHud.HideMenu(m_xParentEntity);   // ...and the SC5 menu too
		m_ePhase = ZM_BD_DONE;
	}
}

void ZM_BattleDirector::RunSetup(const ZM_BattleTransition& xTransition)
{
	m_ePhase = ZM_BD_SETUP;

	// SC5 TRAINER ARM. A trainer round trip carries a REGISTERED id on the
	// transition; a wild one carries ZM_TRAINER_NONE, so this branch is not taken on
	// the wild path and NOTHING below it moves (the ~380 battle goldens depend on
	// that body staying byte-identical).
	if (ZM_IsRegisteredTrainer(xTransition.GetBattleTrainer()))
	{
		RunTrainerSetup(xTransition.GetBattleTrainer());
		return;
	}

	// Read the accepted encounter payload (valid: bInBattle gates on the transition's
	// own IsEncounterPayloadValid acceptance).
	const ZM_SPECIES_ID eEnemySpecies = xTransition.GetBattleSpecies();
	const u_int         uEnemyLevel   = xTransition.GetBattleLevel();

	// Build the player side from the REAL persistent party lead when one exists (SC3);
	// fall back to the deterministic placeholder otherwise (headless windowed drives /
	// no GameStateManager). Only a real lead awards exp + is written back on resolve.
	ZM_BattleMonsterSpec xPlayerSpec;
	ZM_GameState* pxGS = nullptr;
	m_bWriteBackToLead = false;
	if (ZM_GameStateManager::TryGetGameState(pxGS) && pxGS != nullptr && !pxGS->m_xParty.IsEmpty())
	{
		xPlayerSpec = ZM_MonsterToBattleSpec(pxGS->m_xParty.Lead());
		m_bWriteBackToLead = true;
	}
	else
	{
		xPlayerSpec = BuildPlaceholderPlayerSpec();
	}

	const ZM_BattleMonsterSpec xEnemySpec  = ZM_BuildWildEnemySpec(eEnemySpecies, uEnemyLevel);
	const u_int64              ulSeed      = DeriveBattleSeed(eEnemySpecies, uEnemyLevel);

	// BuildBattleConfig() itself stays exp-OFF (a shipped unit asserts it); flip exp ON
	// only for a real-lead battle so the engine mutates the lead in place (SC3).
	ZM_BattleConfig xConfig = BuildBattleConfig();
	xConfig.m_bAwardExp = m_bWriteBackToLead;

	// Begin enters PLAYING_EVENTS (intro); the drive loop Ticks it to AWAIT_INPUT.
	m_xCore.Begin(&xPlayerSpec, 1u, &xEnemySpec, 1u, xConfig, ulSeed, ZM_AI_TIER_GREEDY);

	// Best-effort visuals: a missing arena / creature bundle must NOT abort the battle.
	PlaceCreatureModels(xPlayerSpec.m_eSpecies, eEnemySpecies);

	// Reveal + seed the HUD onto this entity's own UI component (best-effort: a
	// missing UI component skips gracefully).
	m_xHud.Setup(m_xParentEntity, m_xCore);

	m_fRunningSeconds = 0.0f;
	m_ePhase          = ZM_BD_RUNNING;
}

void ZM_BattleDirector::RunTrainerSetup(ZM_TRAINER_ID eTrainer)
{
	const ZM_TrainerBattleSetup xSetup = ZM_BuildTrainerBattleSetup(eTrainer);
	if (!xSetup.m_bValid)
	{
		// FAIL CLOSED, and NEVER Begin: ZM_BattleEngine::Begin Zenith_Asserts on an
		// empty enemy party, and Zenith_Assert calls Zenith_DebugBreak() in EVERY
		// configuration. End the round trip instead -- the transition resumes the
		// overworld and the player loses nothing.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_BattleDirector] trainer %u (%s) has no buildable party -- ending the round trip",
			(u_int)eTrainer, ZM_GetTrainerName(eTrainer));
		m_xHud.Hide(m_xParentEntity);
		m_xHud.HideMenu(m_xParentEntity);
		ZM_BattleTransition::RequestBattleEnd();
		m_bEndRequested = true;
		m_ePhase        = ZM_BD_RESOLVED;
		return;
	}

	m_eTrainer = eTrainer;   // the payout gate, latched only once a battle really begins

	// The player side follows the SAME rule as the wild arm and is DUPLICATED rather
	// than extracted: the wild arm feeds ~380 frozen battle goldens, and SC5 is
	// forbidden from re-routing or refactoring it. Unifying the two is a later
	// commit's decision, not this one's.
	ZM_BattleMonsterSpec xPlayerSpec;
	ZM_GameState* pxGS = nullptr;
	m_bWriteBackToLead = false;
	if (ZM_GameStateManager::TryGetGameState(pxGS) && pxGS != nullptr && !pxGS->m_xParty.IsEmpty())
	{
		xPlayerSpec = ZM_MonsterToBattleSpec(pxGS->m_xParty.Lead());
		m_bWriteBackToLead = true;
	}
	else
	{
		xPlayerSpec = BuildPlaceholderPlayerSpec();
	}

	// BuildTrainerBattleConfig() stays exp-OFF (a shipped unit pins it); flip exp on a
	// LOCAL COPY only for a real-lead battle, exactly as the wild arm does. This is
	// what makes m_bIsTrainerBattle's gross-exp x1.5 live.
	ZM_BattleConfig xConfig = BuildTrainerBattleConfig();
	xConfig.m_bAwardExp = m_bWriteBackToLead;

	// The row's AI tier goes STRAIGHT into Begin's 7th argument -- the wild arm's
	// hard-coded ZM_AI_TIER_GREEDY is a wild-path constant and is not reused here.
	// m_uEnemyCount is already bounded by uZM_TRAINER_BATTLEABLE_PARTY (the builder
	// clamps it): the engine has NO forced replacement on faint, so handing it a
	// bench would faint-lock the battle into Zenith_Assert(!xActive.IsFainted(), ...)
	// -- a break in EVERY configuration -- rather than end it. Do not "restore" the
	// authored count here; raise the constant in ZM_TrainerBattle.h, and only in the
	// commit that adds forced-switch-on-faint.
	m_xCore.Begin(&xPlayerSpec, 1u, xSetup.m_axEnemyParty, xSetup.m_uEnemyCount,
		xConfig, xSetup.m_ulBattleSeed, xSetup.m_eEnemyTier);

	// Best-effort visuals: the trainer's LEAD is the monster on the enemy platform.
	PlaceCreatureModels(xPlayerSpec.m_eSpecies, xSetup.m_eLeadSpecies);
	m_xHud.Setup(m_xParentEntity, m_xCore);

	m_fRunningSeconds = 0.0f;
	m_ePhase          = ZM_BD_RUNNING;
}

void ZM_BattleDirector::PlaceCreatureModels(ZM_SPECIES_ID ePlayerSpecies, ZM_SPECIES_ID eEnemySpecies)
{
	// The Battle scene (this component's OWN scene, NOT the active scene -- ZM-D-089:
	// the battle is loaded additively over a still-active overworld).
	Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
	if (pxSceneData == nullptr)
	{
		return;
	}

	// Resolve the unique arena and capture its platform ids WHILE the pointer is live
	// (component pools swap-and-pop; the pointer is valid only within this call).
	Zenith_EntityID xPlayerPlatformID = INVALID_ENTITY_ID;
	Zenith_EntityID xEnemyPlatformID  = INVALID_ENTITY_ID;
	u_int uArenaCount = 0u;
	g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().ForEach(
		[&](Zenith_EntityID, ZM_BattleArena& xArena)
		{
			++uArenaCount;
			if (uArenaCount == 1u)
			{
				xPlayerPlatformID = xArena.GetChildEntityID(1u);   // 1 = player platform
				xEnemyPlatformID  = xArena.GetChildEntityID(2u);   // 2 = enemy platform
			}
		});
	if (uArenaCount != 1u)
	{
		return;   // no unique arena to place onto; skip (best-effort)
	}

	// A platform position with a small +Y lift so the model sits on the slab, falling
	// back to the arena world plane when the platform can't be resolved.
	auto fnResolvePlacePos = [](Zenith_EntityID xPlatformID) -> Zenith_Maths::Vector3
	{
		Zenith_Maths::Vector3 xPos(0.0f, ZM_BattleArena::fARENA_WORLD_Y + fCREATURE_Y_LIFT, 0.0f);
		Zenith_Entity xPlatform = g_xEngine.Scenes().ResolveEntity(xPlatformID);
		if (xPlatform.IsValid())
		{
			Zenith_TransformComponent& xPlatformTransform = xPlatform.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xPlatformPos;
			xPlatformTransform.GetPosition(xPlatformPos);
			xPos = xPlatformPos;
			xPos.y += fCREATURE_Y_LIFT;
		}
		return xPos;
	};

	auto fnPlaceOne = [&](ZM_SPECIES_ID eSpecies, const Zenith_Maths::Vector3& xPos)
	{
		if (eSpecies >= ZM_SPECIES_COUNT)   // ZM_SPECIES_NONE / out of range
		{
			return;
		}
		char szRef[256];
		if (!ZM_CreatureAssetPath(eSpecies, ZM_CREATURE_ASSET_MODEL, szRef, sizeof(szRef)))
		{
			return;   // ref overflow; skip (best-effort)
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, ZM_GetSpeciesName(eSpecies));
		if (!xEntity.IsValid())
		{
			return;
		}
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		// A missing/unbaked .zmodel loads model-less (mirrors ZM_BattleArena dressing).
		xEntity.AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
	};

	fnPlaceOne(ePlayerSpecies, fnResolvePlacePos(xPlayerPlatformID));
	fnPlaceOne(eEnemySpecies,  fnResolvePlacePos(xEnemyPlatformID));
}

ZM_BattleMonsterSpec ZM_BattleDirector::BuildPlaceholderPlayerSpec()
{
	// Deterministic (same bytes every call): a fixed valid starter at a fixed level.
	return ZM_BuildWildEnemySpec(s_ePLACEHOLDER_PLAYER_SPECIES, s_uPLACEHOLDER_PLAYER_LEVEL);
}

ZM_BattleConfig ZM_BattleDirector::BuildBattleConfig()
{
	ZM_BattleConfig xConfig;      // every other field takes its struct default
	xConfig.m_bIsWild   = true;
	xConfig.m_bAwardExp = false;
	xConfig.m_bCanCatch = true;   // SC4 CATCH: the engine asserts on a catch unless catch is allowed
	xConfig.m_bCanFlee  = true;   // SC5 RUN: the engine asserts on a flee unless flee is allowed
	return xConfig;
}

ZM_BattleConfig ZM_BattleDirector::BuildTrainerBattleConfig()
{
	ZM_BattleConfig xConfig;                 // every unnamed field keeps its struct default
	xConfig.m_bIsWild          = false;      // a trainer battle is not a wild encounter
	xConfig.m_bCanCatch        = false;      // another trainer's monster can never be caught
	xConfig.m_bCanFlee         = false;      // and there is no running from a trainer
	xConfig.m_bIsTrainerBattle = true;       // gross exp x1.5 -- inert until awards are on
	xConfig.m_uLevelCap        = 0u;         // 0 == none; only the flat-50 tower facility caps
	xConfig.m_bAwardExp        = false;      // OFF by construction, exactly like the wild helper
	return xConfig;
}

u_int64 ZM_BattleDirector::DeriveBattleSeed(ZM_SPECIES_ID eSpecies, u_int uLevel)
{
	// Pure deterministic FNV-1a fold of (species, level). Same inputs -> same seed, so
	// a windowed battle drive is reproducible. Distinct from the AI-rng seed the core
	// derives from this value (ZM_DeriveAiRngSeed), so the two streams never coincide.
	u_int64 ulHash = 0xCBF29CE484222325ull;                      // FNV-1a 64-bit offset basis
	ulHash = (ulHash ^ static_cast<u_int64>(eSpecies)) * 0x100000001B3ull;   // FNV prime
	ulHash = (ulHash ^ static_cast<u_int64>(uLevel))   * 0x100000001B3ull;
	return ulHash;
}

bool ZM_BattleDirector::ShouldRunSetup(ZM_BATTLE_DIRECTOR_PHASE ePhase, bool bTransitionInBattle, bool bAlreadySetUp)
{
	return ePhase == ZM_BD_WAIT_FOR_IN_BATTLE && bTransitionInBattle && !bAlreadySetUp;
}

bool ZM_BattleDirector::ShouldRequestEndNow(ZM_BATTLE_DIRECTOR_PHASE ePhase, bool bCoreShouldEnd, bool bAlreadyRequested)
{
	return ePhase == ZM_BD_RUNNING && bCoreShouldEnd && !bAlreadyRequested;
}

void ZM_BattleDirector::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// The director carries NO persisted live state -- a bare version stamp is enough
	// (it rebuilds from the transition + arena on the next OnUpdate).
	xStream << uSERIALIZATION_VERSION;
}

void ZM_BattleDirector::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Reset every live field to defaults BEFORE the version gate (the reset-first
	// idiom, mirroring ZM_BattleArena): never retain stale runtime state from a reused
	// instance, and rebuild on the next OnUpdate regardless of version.
	m_ePhase           = ZM_BD_WAIT_FOR_IN_BATTLE;
	m_bEndRequested    = false;
	m_bWriteBackToLead = false;
	m_eTrainer         = ZM_TRAINER_NONE;
	m_fRunningSeconds  = 0.0f;
	m_xCore            = ZM_BattleDirectorCore{};   // fresh, un-begun core

	if (uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}
	// No further persisted fields.
}

#ifdef ZENITH_TOOLS
void ZM_BattleDirector::RenderPropertiesPanel()
{
	ImGui::Text("Battle director - phase %u (endRequested=%s, running=%.2fs, coreState=%u)",
		static_cast<u_int>(m_ePhase), m_bEndRequested ? "true" : "false", m_fRunningSeconds,
		static_cast<u_int>(m_xCore.GetState()));
}
#endif
