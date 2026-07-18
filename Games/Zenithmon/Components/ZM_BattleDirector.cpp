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
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"           // ZM_AI_TIER_GREEDY
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"           // ZM_CreatureAssetPath, ZM_CREATURE_ASSET_MODEL

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_BattleDirector (S5 item 4 SC3, order 111). The ECS presenter-driver that
// binds the pure ZM_BattleDirectorCore into the additively-loaded Battle scene:
// watch the persistent transition, one-shot Begin a deterministic AI-vs-AI wild
// battle, place two creature models on the arena platforms, drive the core turn
// by turn, and end the round-trip via ZM_BattleTransition::RequestBattleEnd()
// exactly once. NO HUD (later SC). ZM-D-102.
// ============================================================================

// A fixed valid starter (dex row 0). BuildPlaceholderPlayerSpec pins this so the
// player spec is deterministic and reproducible; a starter at level 5 always has a
// level-1 same-type move (ZM_GetSpeciesLearnset guarantees a level-1 STAB pick), so
// m_aeMoves[0] != ZM_MOVE_NONE.
static const ZM_SPECIES_ID s_ePLACEHOLDER_PLAYER_SPECIES = ZM_SPECIES_FERNFAWN;
static const u_int         s_uPLACEHOLDER_PLAYER_LEVEL   = 5u;

ZM_BattleDirector::ZM_BattleDirector(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_BattleDirector::OnStart()
{
	// Dormant until the persistent transition reaches IN_BATTLE. Start from a clean
	// baseline (defensive; a fresh instance already defaults here).
	m_ePhase          = ZM_BD_WAIT_FOR_IN_BATTLE;
	m_bEndRequested   = false;
	m_fRunningSeconds = 0.0f;
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
			// Singles wild battle: the player side always uses move slot 0. The core
			// picks the enemy reply through its own AI rng (non-perturbing).
			ZM_BattleAction xPlayerMove;
			xPlayerMove.m_eKind     = ZM_ACTION_MOVE;
			xPlayerMove.m_uMoveSlot = 0u;
			m_xCore.SubmitPlayerAction(xPlayerMove);
		}
		m_xCore.Tick(fDeltaSeconds);

		if (ShouldRequestEndNow(m_ePhase, m_xCore.ShouldRequestEnd(), m_bEndRequested))
		{
			// The battle resolved: RequestBattleEnd is the SOLE exit from IN_BATTLE.
			ZM_BattleTransition::RequestBattleEnd();
			m_bEndRequested = true;
			m_ePhase        = ZM_BD_RESOLVED;
		}
		else if (m_fRunningSeconds >= fRUNNING_DEADLINE_SECONDS && !m_bEndRequested)
		{
			// Wall-clock safety abort: never softlock the round-trip if the core wedges.
			// (Under zm_instant_battles a battle resolves in a couple of Ticks, so this
			// path is unreachable there.)
			ZM_BattleTransition::RequestBattleEnd();
			m_bEndRequested = true;
			m_ePhase        = ZM_BD_RESOLVED;
		}
	}

	// -- 3/5. Settle to DONE once the transition has left IN_BATTLE (scene about to
	//         unload). bInBattle is the top-of-frame value, so the frame that fired
	//         RequestBattleEnd still observes RESOLVED; DONE follows next frame. A
	//         RUNNING director whose battle was torn down externally also settles. --
	if (!bInBattle && (m_ePhase == ZM_BD_RESOLVED || m_ePhase == ZM_BD_RUNNING))
	{
		m_ePhase = ZM_BD_DONE;
	}
}

void ZM_BattleDirector::RunSetup(const ZM_BattleTransition& xTransition)
{
	m_ePhase = ZM_BD_SETUP;

	// Read the accepted encounter payload (valid: bInBattle gates on the transition's
	// own IsEncounterPayloadValid acceptance).
	const ZM_SPECIES_ID eEnemySpecies = xTransition.GetBattleSpecies();
	const u_int         uEnemyLevel   = xTransition.GetBattleLevel();

	const ZM_BattleMonsterSpec xPlayerSpec = BuildPlaceholderPlayerSpec();
	const ZM_BattleMonsterSpec xEnemySpec  = ZM_BuildWildEnemySpec(eEnemySpecies, uEnemyLevel);
	const ZM_BattleConfig      xConfig     = BuildBattleConfig();
	const u_int64              ulSeed      = DeriveBattleSeed(eEnemySpecies, uEnemyLevel);

	// Begin enters PLAYING_EVENTS (intro); the drive loop Ticks it to AWAIT_INPUT.
	m_xCore.Begin(&xPlayerSpec, 1u, &xEnemySpec, 1u, xConfig, ulSeed, ZM_AI_TIER_GREEDY);

	// Best-effort visuals: a missing arena / creature bundle must NOT abort the battle.
	PlaceCreatureModels(xPlayerSpec.m_eSpecies, eEnemySpecies);

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
	m_ePhase          = ZM_BD_WAIT_FOR_IN_BATTLE;
	m_bEndRequested   = false;
	m_fRunningSeconds = 0.0f;
	m_xCore           = ZM_BattleDirectorCore{};   // fresh, un-begun core

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
