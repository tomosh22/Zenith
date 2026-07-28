#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleDirector.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"                // ZM_AI_TIER
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"      // ZM_SetInstantBattlesForTests, GetWinner
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"             // ZM_SIDE_PLAYER / ZM_SIDE_COUNT
#include "Zenithmon/Source/Battle/ZM_TrainerBattle.h"           // uZM_TRAINER_BATTLEABLE_PARTY
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"                // ZM_IsStoryFlagSet
#include "Zenithmon/Source/Data/ZM_TrainerData.h"               // the authored rival row
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"                  // ZM_BuildMonsterRecord
#include "Zenithmon/Source/Party/ZM_Party.h"
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"          // ZM_OnTrainerEncounter

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_TrainerBattle -- the windowed/headless gate for S7 item 3 SC5:
// the SECOND (trainer) battle-entry channel end to end.
//
// ONE test, ZM_TrainerBattle_Test, m_bRequiresGraphics = FALSE. It asserts
// nothing about pixels or grass blades, so it runs FOR REAL on the Null backend
// instead of skipping as a pass -- which is the whole point of a CI gate for the
// vertical's core.
//
// It drives the REAL entry channel rather than any test seam: it dispatches a
// ZM_OnTrainerEncounter through the process-global Zenith_EventDispatcher and
// lets the SHIPPED subscriber latch it, ZM_BattleTransition fade/load/park,
// ZM_BattleDirector take its trainer arm, the player's ENTER presses resolve the
// battle, and the resume land the prize + defeat flag on the persistent
// ZM_GameState.
//
// It does that TWICE, back to back on the still-loaded overworld:
//   trip 1 -- the RIVAL (a flagged row): the prize + defeat-flag payout, the
//             no-flee/no-catch config, and the clean round trip.
//   trip 2 -- the ROUTE-1 RAMBLER: the AI-tier proof (the rival's authored tier is
//             ZM_AI_TIER_GREEDY, which is ALSO the wild arm's literal and the
//             core's pre-Begin default, so trip 1 alone cannot distinguish the
//             trainer arm's call site), the battleable-party CLAMP read straight
//             off the engine's own state, and the ZM_STORY_FLAG_NONE payout arm.
// See the eTB_TRAINER2 comment for the mutations each clause kills.
//
// NO Zenith_EventDispatcher::ScopedTestIsolation -- deliberately, and this is
// load-bearing. That guard STEALS the live subscription tables and leaves the
// dispatcher EMPTY for its lifetime, so constructing one here would delete the
// very subscriber under test and the trainer battle would never start. This test
// adds NO subscription of its own; it reads the GAME's own component. (The same
// ruling as ZM_AutoTests_BattleTransition.cpp and ZM_AutoTests_BattleDirector.cpp.)
//
// Per-phase driver functions, never one monolithic Step: the ZM-D-141 stack rule.
// This test never holds a ZM_GameState BY VALUE (it only reads through the live
// pointer), so no phase frame carries one.
// ============================================================================

namespace
{
	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (re-declared from ZM_AutoTests_Battle
	// Menu.cpp -- those copies are file-local and not linkable across TUs).
	// -------------------------------------------------------------------------

	struct PlayerView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		ZM_PlayerController* m_pxController = nullptr;
		Zenith_ColliderComponent* m_pxCollider = nullptr;
	};

	struct CameraView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_FollowCamera* m_pxFollow = nullptr;
		Zenith_CameraComponent* m_pxCamera = nullptr;
	};

	bool FindActivePlayer(PlayerView& xOut)
	{
		xOut = PlayerView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_PlayerController,
			Zenith_ColliderComponent,
			Zenith_TransformComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_PlayerController& xController,
				Zenith_ColliderComponent& xCollider,
				Zenith_TransformComponent& xTransform)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxController = &xController;
				xOut.m_pxCollider = &xCollider;
				xTransform.GetPosition(xOut.m_xPosition);
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool FindActiveCamera(CameraView& xOut)
	{
		xOut = CameraView{};
		g_xEngine.Scenes().QueryActiveScene<
			ZM_FollowCamera,
			Zenith_CameraComponent>().ForEach(
			[&xOut](Zenith_EntityID xID,
				ZM_FollowCamera& xFollow,
				Zenith_CameraComponent& xCamera)
			{
				if (xOut.m_xEntityID != INVALID_ENTITY_ID)
				{
					return;
				}
				xOut.m_xEntityID = xID;
				xOut.m_pxFollow = &xFollow;
				xOut.m_pxCamera = &xCamera;
			});
		return xOut.m_xEntityID != INVALID_ENTITY_ID;
	}

	bool ActiveGrassIsReady()
	{
		bool bReady = false;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&bReady](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				// A Null (headless) build never APPLIES grass: the blades are GPU-only
				// content the backend deliberately does not author. The component still
				// exists and has reached its terminal inert state, which is what this
				// gate is really asking: "has the overworld finished coming up?".
				bReady = bReady || xGrass.IsGrassApplied() || Zenith_IsNullRenderer();
			});
		return bReady;
	}

	bool DawnmereRuntimeReady(PlayerView& xPlayer, CameraView& xCamera)
	{
		return FindActivePlayer(xPlayer)
			&& FindActiveCamera(xCamera)
			&& xPlayer.m_pxCollider->HasValidBody()
			&& xPlayer.m_pxController->IsGrounded()
			&& xCamera.m_pxFollow->GetTargetEntityID() == xPlayer.m_xEntityID
			&& xCamera.m_pxFollow->GetCurrentArmDistance() > 0.0f
			&& ActiveGrassIsReady();
	}

	bool DiskFilePresent(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	bool RequiredDawnmereAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" + ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" + ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			if (!DiskFilePresent(strPath))
			{
				return false;
			}
		}
		return true;
	}

	// Both singletons are resolved FRESH each frame: component pools relocate
	// entries on swap-and-pop, so these pointers must never be cached.
	ZM_BattleTransition* ResolveSingletonBattleTransition()
	{
		Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
		if (!ZM_BattleTransition::TryGetUniqueSingletonEntityID(xEntityID))
		{
			return nullptr;
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_BattleTransition>()
			: nullptr;
	}

	ZM_BattleDirector* ResolveLiveBattleDirector()
	{
		Zenith_EntityID xDirectorID = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleDirector>().ForEach(
			[&xDirectorID](Zenith_EntityID xID, ZM_BattleDirector&)
			{
				if (xDirectorID == INVALID_ENTITY_ID)
				{
					xDirectorID = xID;
				}
			});
		if (xDirectorID == INVALID_ENTITY_ID)
		{
			return nullptr;   // Battle scene not loaded
		}
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xDirectorID);
		return xEntity.IsValid()
			? xEntity.TryGetComponent<ZM_BattleDirector>()
			: nullptr;
	}

	// -------------------------------------------------------------------------
	// Constants
	// -------------------------------------------------------------------------

	// THE SINGLE DIFFICULTY KNOB. Vesper's authored party is one KINDLET at L5
	// (ZM_TrainerData.cpp) and KINDLET is the FIRE counterpart to the GRASS
	// starter, so the type matchup favours the ENEMY. The installed lead is a
	// FERNFAWN at L60: the 55-level gap is what makes the win reliable, exactly as
	// ZM_BattleMenuWhiteout_Test relies on the same asymmetry in reverse. If the
	// player ever fails to win reliably, RAISE this level -- never weaken the
	// assertions.
	constexpr ZM_SPECIES_ID eTB_PLAYER_LEAD_SPECIES = ZM_SPECIES_FERNFAWN;
	constexpr u_int         uTB_PLAYER_LEAD_LEVEL   = 60u;

	// The trainer this gate forces, and the scene it is forced from.
	constexpr ZM_TRAINER_ID eTB_TRAINER      = ZM_TRAINER_RIVAL_VESPER;
	constexpr ZM_SCENE_ID   eTB_SOURCE_SCENE = ZM_SCENE_DAWNMERE;
	constexpr ZM_SPECIES_ID eTB_ENEMY_LEAD   = ZM_SPECIES_KINDLET;   // Vesper's authored lead
	constexpr u_int         uTB_EXPECTED_PRIZE = 500u;               // Vesper's authored prize

	// -- THE SECOND ROUND TRIP, and why it exists --------------------------------
	//
	// (1) It makes the AI-tier proof NON-VACUOUS. Vesper authors
	//     ZM_AI_TIER_GREEDY, which is ALSO the wild arm's hard-coded literal
	//     (ZM_BattleDirector::RunSetup) AND ZM_BattleDirectorCore's pre-Begin member
	//     default -- so a tier assertion driven by Vesper ALONE survives replacing
	//     the trainer arm's `xSetup.m_eEnemyTier` with a literal GREEDY, and survives
	//     deleting Begin's tier assignment outright. The rambler authors
	//     ZM_AI_TIER_RANDOM, which NEITHER of those can produce, so both mutations
	//     die here. Verify guards the property rather than trusting this comment.
	//
	// (2) It drives the row whose AUTHORED party is MULTI-member, proving the
	//     battleable clamp (uZM_TRAINER_BATTLEABLE_PARTY) end to end: an unclamped
	//     party would not end when its active fainted, and the next enemy
	//     SubmitAction would hit Zenith_Assert(!xActive.IsFainted(), ...) -- a
	//     process break in EVERY configuration. The observed enemy party SIZE is
	//     asserted directly so the proof does not rest on "the run did not crash".
	//
	// (3) It exercises the ZM_STORY_FLAG_NONE arm of the payout on a REAL round trip:
	//     the prize is credited and no flag is written.
	constexpr ZM_TRAINER_ID eTB_TRAINER2        = ZM_TRAINER_ROUTE1_RAMBLER;
	constexpr ZM_SPECIES_ID eTB_ENEMY_LEAD2     = ZM_SPECIES_NIBBIN;   // the rambler's authored FIRST member
	constexpr u_int         uTB_EXPECTED_PRIZE2 = 120u;                // the rambler's authored prize

	// Coarser than 1/60 (mirrors the shipped round trip): the trip must fit the
	// additive load + arena build + the battle + the resume poll chains into the
	// frame budget, and 1/60 would be too tight.
	constexpr float fTB_FIXED_DT = 1.0f / 30.0f;

	constexpr int iTB_READY_DEADLINE    = 420;
	constexpr int iTB_INBATTLE_DEADLINE = 600;
	constexpr int iTB_RESUME_DEADLINE   = 900;
	constexpr int iTB_SETTLE_FRAMES     = 8;

	enum class TBPhase
	{
		AwaitReady,
		InstallAndDispatch,
		AwaitInBattle,
		DriveMenu,
		Settle,
		DispatchSecond,
		AwaitInBattle2,
		DriveMenu2,
		Settle2,
		Done,
	};

	// ---- Control state (ALL reset in Setup; batch mode reuses the process) ----
	TBPhase     g_eTBPhase          = TBPhase::Done;
	int         g_iTBPhaseFrames    = 0;
	bool        g_bTBActive         = false;
	bool        g_bTBFailed         = false;
	bool        g_bTBPrereqsPresent = false;
	const char* g_szTBFailure       = "test did not reach verification";

	// ---- Baselines captured BEFORE the encounter. The "after" money defaults to
	// 0xffffffff so an unsampled run FAILS rather than passing on 0 == 0. ----
	bool  g_bTBBaselineCaptured = false;
	u_int g_uTBMoneyBefore      = 0u;
	bool  g_bTBFlagBefore       = true;    // want false; a true here is anti-vacuity's failure
	bool  g_bTBLeadInstalled    = false;

	// ---- Channel + config captures (latched while the Battle scene is loaded) ----
	bool          g_bTBDirectorSeen    = false;
	ZM_TRAINER_ID g_eTBChannelTrainer  = ZM_TRAINER_NONE;
	bool          g_bTBChannelCaptured = false;
	bool          g_bTBConfigCaptured  = false;
	bool          g_bTBFleeAllowed     = true;    // want false (a trainer battle)
	bool          g_bTBCatchAllowed    = true;    // want false
	ZM_AI_TIER    g_eTBTier            = ZM_AI_TIER_NONE;
	ZM_SPECIES_ID g_eTBEnemySpecies    = ZM_SPECIES_NONE;

	// ---- Outcome captures ----
	bool    g_bTBWinnerCaptured = false;
	ZM_SIDE g_eTBWinner         = ZM_SIDE_COUNT;

	// ---- Post-resume samples ----
	bool  g_bTBAfterCaptured    = false;
	u_int g_uTBMoneyAfter       = 0xffffffffu;
	bool  g_bTBFlagAfter        = false;
	bool  g_bTBWhiteoutAfter    = true;    // want false (a WIN never latches a whiteout)
	u_int g_uTBCompletedAfter   = 0xffffffffu;
	u_int g_uTBAbortedAfter     = 0xffffffffu;
	u_int g_uTBStateAfter       = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;   // want IDLE
	bool  g_bTBBattleUnloaded   = false;

	// ---- SECOND ROUND TRIP (the rambler): the non-vacuous tier proof, the clamp
	//      proof, and the ZM_STORY_FLAG_NONE payout arm. ----
	ZM_TRAINER_ID g_eTB2ChannelTrainer  = ZM_TRAINER_NONE;
	bool          g_bTB2ChannelCaptured = false;
	bool          g_bTB2ConfigCaptured  = false;
	ZM_AI_TIER    g_eTB2Tier            = ZM_AI_TIER_NONE;
	ZM_SPECIES_ID g_eTB2EnemySpecies    = ZM_SPECIES_NONE;
	u_int         g_uTB2EnemyPartySize  = 0xffffffffu;   // want exactly uZM_TRAINER_BATTLEABLE_PARTY
	bool          g_bTB2WinnerCaptured  = false;
	ZM_SIDE       g_eTB2Winner          = ZM_SIDE_COUNT;
	u_int         g_uTB2MoneyBefore     = 0u;
	bool          g_bTB2AfterCaptured   = false;
	u_int         g_uTB2MoneyAfter      = 0xffffffffu;
	u_int         g_uTB2CompletedAfter  = 0xffffffffu;   // want 2 (both round trips)
	u_int         g_uTB2AbortedAfter    = 0xffffffffu;
	u_int         g_uTB2StateAfter      = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;

	void FailTB(const char* szReason)
	{
		g_szTBFailure = szReason;
		g_bTBFailed = true;
		g_eTBPhase = TBPhase::Done;
	}

	// Latch everything that is only readable while the Battle scene is loaded.
	// Called every frame from the await/drive phases of BOTH round trips; the
	// director pointer is re-resolved inside, never cached. bSecondRun selects which
	// set of captures is written -- the two round trips share no capture slot, so a
	// value from the rival's battle can never satisfy the rambler's assertion.
	void CaptureLiveDirector(bool bSecondRun)
	{
		ZM_BattleDirector* pxDirector = ResolveLiveBattleDirector();
		if (pxDirector == nullptr)
		{
			return;
		}
		g_bTBDirectorSeen = true;

		// The core's read-through accessors index the engine's party vectors, so
		// nothing may be sampled before Begin ran (phase RUNNING onward).
		const ZM_BATTLE_DIRECTOR_PHASE ePhase = pxDirector->GetPhase();
		if (ePhase != ZM_BD_RUNNING && ePhase != ZM_BD_RESOLVED && ePhase != ZM_BD_DONE)
		{
			return;
		}

		const ZM_BattleDirectorCore& xCore = pxDirector->GetCore();
		if (!bSecondRun)
		{
			if (!g_bTBConfigCaptured)
			{
				g_bTBFleeAllowed    = xCore.IsFleeAllowed();
				g_bTBCatchAllowed   = xCore.IsCatchAllowed();
				g_eTBTier           = xCore.GetEnemyTier();
				g_eTBEnemySpecies   = xCore.SideActiveSpecies(ZM_SIDE_ENEMY);
				g_bTBConfigCaptured = true;
			}
			if (xCore.IsOver())
			{
				g_eTBWinner         = xCore.GetWinner();   // final + stable once over
				g_bTBWinnerCaptured = true;
			}
			return;
		}

		if (!g_bTB2ConfigCaptured)
		{
			g_eTB2Tier          = xCore.GetEnemyTier();
			g_eTB2EnemySpecies  = xCore.SideActiveSpecies(ZM_SIDE_ENEMY);
			// The party the engine was actually HANDED -- the direct end-to-end proof
			// of the battleable clamp, read off the battle's own deep-owned state
			// rather than inferred from "the process did not break".
			g_uTB2EnemyPartySize =
				xCore.GetEngine().GetState().Side(ZM_SIDE_ENEMY).m_xParty.GetSize();
			g_bTB2ConfigCaptured = true;
		}
		if (xCore.IsOver())
		{
			g_eTB2Winner         = xCore.GetWinner();
			g_bTB2WinnerCaptured = true;
		}
	}

	// ---- Per-phase drivers. Each returns true to keep stepping, false to stop. ----

	bool TBPhaseAwaitReady()
	{
		PlayerView xPlayer;
		CameraView xCamera;
		if (!DawnmereRuntimeReady(xPlayer, xCamera))
		{
			if (g_iTBPhaseFrames > iTB_READY_DEADLINE)
			{
				FailTB("Dawnmere did not become runtime-ready in time");
				return false;
			}
			return true;
		}

		// The battle machine is a persistent-scene singleton -- if it does not
		// resolve here, the subscription under test cannot exist either.
		if (ResolveSingletonBattleTransition() == nullptr)
		{
			FailTB("no unique ZM_BattleTransition singleton");
			return false;
		}

		g_eTBPhase = TBPhase::InstallAndDispatch;
		g_iTBPhaseFrames = 0;
		return true;
	}

	bool TBPhaseInstallAndDispatch()
	{
		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
		{
			FailTB("no persistent ZM_GameState resolved -- the prize/flag payout has no target");
			return false;
		}

		g_uTBMoneyBefore = pxGameState->m_uMoney;
		g_bTBFlagBefore  = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
		g_bTBBaselineCaptured = true;

		// Install the deterministic-win lead (see the difficulty-knob comment).
		pxGameState->m_xParty = ZM_Party{};
		g_bTBLeadInstalled = pxGameState->m_xParty.Add(
			ZM_BuildMonsterRecord(eTB_PLAYER_LEAD_SPECIES, uTB_PLAYER_LEAD_LEVEL));
		if (!g_bTBLeadInstalled)
		{
			FailTB("could not install the deterministic-win party lead");
			return false;
		}

		// THE ENTRY CHANNEL under test: a real dispatch through the process-global
		// dispatcher into the GAME's own subscriber. SC6 will raise this from the
		// overworld sight FSM; nothing in production raises it yet.
		Zenith_EventDispatcher::Get().Dispatch(
			ZM_OnTrainerEncounter{ eTB_TRAINER, eTB_SOURCE_SCENE });

		g_eTBPhase = TBPhase::AwaitInBattle;
		g_iTBPhaseFrames = 0;
		return true;
	}

	bool TBPhaseAwaitInBattle()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
			return false;
		}

		// The channel discriminator: the accepted round trip must carry the id the
		// event named. Latched the first frame the transition leaves IDLE.
		if (!g_bTBChannelCaptured
			&& pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
		{
			g_eTBChannelTrainer  = pxTransition->GetBattleTrainer();
			g_bTBChannelCaptured = true;
		}

		CaptureLiveDirector(false);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
		{
			g_eTBPhase = TBPhase::DriveMenu;
			g_iTBPhaseFrames = 0;
			return true;
		}

		if (g_iTBPhaseFrames > iTB_INBATTLE_DEADLINE)
		{
			FailTB("the trainer encounter never reached IN_BATTLE before the deadline");
			return false;
		}
		return true;
	}

	bool TBPhaseDriveMenu()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving during the battle");
			return false;
		}

		// The default menu is ACTION_ROOT with cursor 0 = Fight: one ENTER opens the
		// move list, the next ENTER submits move 0, so pressing ENTER every frame
		// picks Fight->move0 each turn (a HIDDEN-menu press is inert). This is the
		// SAME drive ZM_BattleMenuWin_Test uses -- and it is safe in a trainer battle
		// only because SC4's Run-gate refuses a gated root entry at the CONFIRM
		// boundary, so no RUN action can reach ZM_BattleEngine's Zenith_Assert.
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		CaptureLiveDirector(false);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
			&& pxTransition->GetCompletedBattleCount() == 1u)
		{
			g_eTBPhase = TBPhase::Settle;
			g_iTBPhaseFrames = 0;
			return true;
		}

		if (g_iTBPhaseFrames > iTB_RESUME_DEADLINE)
		{
			FailTB("the player-driven trainer battle never ended "
				"(never returned to IDLE with completed == 1)");
			return false;
		}
		return true;
	}

	bool TBPhaseSettle()
	{
		if (g_iTBPhaseFrames < iTB_SETTLE_FRAMES)
		{
			return true;
		}

		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving after the resume");
			return false;
		}
		g_uTBCompletedAfter = pxTransition->GetCompletedBattleCount();
		g_uTBAbortedAfter   = pxTransition->GetAbortedTransitionCount();
		g_uTBStateAfter     = (u_int)pxTransition->GetTransitionState();
		g_bTBBattleUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();

		// Re-resolve the persistent GameState FRESH (never cached across the additive
		// battle + unload) and sample the payout.
		ZM_GameState* pxGameState = nullptr;
		if (ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr)
		{
			g_uTBMoneyAfter    = pxGameState->m_uMoney;
			g_bTBFlagAfter     = ZM_IsStoryFlagSet(*pxGameState, ZM_STORY_FLAG_RIVAL1_DEFEATED);
			g_bTBWhiteoutAfter = pxGameState->m_bPendingWhiteout;
			g_bTBAfterCaptured = true;
		}

		g_eTBPhase = TBPhase::DispatchSecond;
		g_iTBPhaseFrames = 0;
		return true;
	}

	// ---- SECOND ROUND TRIP (the rambler) ------------------------------------
	// The overworld is still loaded and the transition is back at IDLE, so this
	// costs one additive Battle load and a handful of menu frames -- no scene
	// reload, no re-ready wait. Everything the first trip sampled was captured in
	// TBPhaseSettle BEFORE this runs, so nothing here can perturb it.

	bool TBPhaseDispatchSecond()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving before the second trip");
			return false;
		}
		if (pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
		{
			FailTB("the transition was not IDLE before the second trainer dispatch");
			return false;
		}

		ZM_GameState* pxGameState = nullptr;
		if (!ZM_GameStateManager::TryGetGameState(pxGameState) || pxGameState == nullptr)
		{
			FailTB("no persistent ZM_GameState before the second trip -- its payout has no target");
			return false;
		}
		g_uTB2MoneyBefore = pxGameState->m_uMoney;

		// The party is NOT reinstalled: the lead written back from the first battle
		// is deliberately the one that fights again, so a real post-write-back record
		// is what enters the second Begin.
		Zenith_EventDispatcher::Get().Dispatch(
			ZM_OnTrainerEncounter{ eTB_TRAINER2, eTB_SOURCE_SCENE });

		g_eTBPhase = TBPhase::AwaitInBattle2;
		g_iTBPhaseFrames = 0;
		return true;
	}

	bool TBPhaseAwaitInBattle2()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE (2)");
			return false;
		}

		if (!g_bTB2ChannelCaptured
			&& pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
		{
			g_eTB2ChannelTrainer  = pxTransition->GetBattleTrainer();
			g_bTB2ChannelCaptured = true;
		}

		CaptureLiveDirector(true);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
		{
			g_eTBPhase = TBPhase::DriveMenu2;
			g_iTBPhaseFrames = 0;
			return true;
		}

		if (g_iTBPhaseFrames > iTB_INBATTLE_DEADLINE)
		{
			FailTB("the SECOND trainer encounter never reached IN_BATTLE before the deadline");
			return false;
		}
		return true;
	}

	bool TBPhaseDriveMenu2()
	{
		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving during the second battle");
			return false;
		}

		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ENTER);
		CaptureLiveDirector(true);

		if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
			&& pxTransition->GetCompletedBattleCount() == 2u)
		{
			g_eTBPhase = TBPhase::Settle2;
			g_iTBPhaseFrames = 0;
			return true;
		}

		if (g_iTBPhaseFrames > iTB_RESUME_DEADLINE)
		{
			FailTB("the SECOND trainer battle never ended "
				"(never returned to IDLE with completed == 2)");
			return false;
		}
		return true;
	}

	bool TBPhaseSettle2()
	{
		if (g_iTBPhaseFrames < iTB_SETTLE_FRAMES)
		{
			return true;
		}

		ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
		if (pxTransition == nullptr)
		{
			FailTB("the ZM_BattleTransition singleton stopped resolving after the second resume");
			return false;
		}
		g_uTB2CompletedAfter = pxTransition->GetCompletedBattleCount();
		g_uTB2AbortedAfter   = pxTransition->GetAbortedTransitionCount();
		g_uTB2StateAfter     = (u_int)pxTransition->GetTransitionState();

		ZM_GameState* pxGameState = nullptr;
		if (ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr)
		{
			g_uTB2MoneyAfter   = pxGameState->m_uMoney;
			g_bTB2AfterCaptured = true;
		}

		g_eTBPhase = TBPhase::Done;
		return false;
	}

	// -------------------------------------------------------------------------
	// Harness entry points
	// -------------------------------------------------------------------------

	void Setup_ZMTrainerBattle()
	{
		g_eTBPhase          = TBPhase::Done;
		g_iTBPhaseFrames    = 0;
		g_bTBActive         = false;
		g_bTBFailed         = false;
		g_bTBPrereqsPresent = false;
		g_szTBFailure       = "test did not reach verification";

		g_bTBBaselineCaptured = false;
		g_uTBMoneyBefore      = 0u;
		g_bTBFlagBefore       = true;
		g_bTBLeadInstalled    = false;

		g_bTBDirectorSeen    = false;
		g_eTBChannelTrainer  = ZM_TRAINER_NONE;
		g_bTBChannelCaptured = false;
		g_bTBConfigCaptured  = false;
		g_bTBFleeAllowed     = true;
		g_bTBCatchAllowed    = true;
		g_eTBTier            = ZM_AI_TIER_NONE;
		g_eTBEnemySpecies    = ZM_SPECIES_NONE;

		g_bTBWinnerCaptured = false;
		g_eTBWinner         = ZM_SIDE_COUNT;

		g_bTBAfterCaptured  = false;
		g_uTBMoneyAfter     = 0xffffffffu;
		g_bTBFlagAfter      = false;
		g_bTBWhiteoutAfter  = true;
		g_uTBCompletedAfter = 0xffffffffu;
		g_uTBAbortedAfter   = 0xffffffffu;
		g_uTBStateAfter     = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;
		g_bTBBattleUnloaded = false;

		g_eTB2ChannelTrainer  = ZM_TRAINER_NONE;
		g_bTB2ChannelCaptured = false;
		g_bTB2ConfigCaptured  = false;
		g_eTB2Tier            = ZM_AI_TIER_NONE;
		g_eTB2EnemySpecies    = ZM_SPECIES_NONE;
		g_uTB2EnemyPartySize  = 0xffffffffu;
		g_bTB2WinnerCaptured  = false;
		g_eTB2Winner          = ZM_SIDE_COUNT;
		g_uTB2MoneyBefore     = 0u;
		g_bTB2AfterCaptured   = false;
		g_uTB2MoneyAfter      = 0xffffffffu;
		g_uTB2CompletedAfter  = 0xffffffffu;
		g_uTB2AbortedAfter    = 0xffffffffu;
		g_uTB2StateAfter      = (u_int)ZM_BATTLE_TRANSITION_FADING_OUT;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, instant-battles flag, scene load) until EVERY
		// git-ignored input is confirmed present -- the Dawnmere terrain, the
		// authored Battle scene, and the baked PROP family (the arena's dressing).
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bTBPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bTBPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// Clear the transition's ownerless statics (BOTH channels' latches) so an
		// earlier batched test cannot bleed one in. Does NOT touch the subscriptions.
		ZM_BattleTransition::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fTB_FIXED_DT);

		// Collapse every presentation op to zero duration so a turn drains in a single
		// Tick; the AWAIT_INPUT gate still needs one real frame per menu press.
		ZM_SetInstantBattlesForTests(true);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eTBPhase = TBPhase::AwaitReady;
		g_bTBActive = true;
	}

	bool Step_ZMTrainerBattle(int)
	{
		if (!g_bTBActive || g_bTBFailed || g_eTBPhase == TBPhase::Done)
		{
			return false;
		}

		++g_iTBPhaseFrames;
		switch (g_eTBPhase)
		{
		case TBPhase::AwaitReady:         return TBPhaseAwaitReady();
		case TBPhase::InstallAndDispatch: return TBPhaseInstallAndDispatch();
		case TBPhase::AwaitInBattle:      return TBPhaseAwaitInBattle();
		case TBPhase::DriveMenu:          return TBPhaseDriveMenu();
		case TBPhase::Settle:             return TBPhaseSettle();
		case TBPhase::DispatchSecond:     return TBPhaseDispatchSecond();
		case TBPhase::AwaitInBattle2:     return TBPhaseAwaitInBattle2();
		case TBPhase::DriveMenu2:         return TBPhaseDriveMenu2();
		case TBPhase::Settle2:            return TBPhaseSettle2();
		case TBPhase::Done:               return false;
		}
		return false;
	}

	bool Verify_ZMTrainerBattle()
	{
		bool bPassed = true;

		if (g_bTBActive)
		{
			const ZM_TrainerData& xRow = ZM_GetTrainerData(eTB_TRAINER);

			// Everything captured, on ONE line, so a failure is fully localisable from
			// the log alone without a rebuild.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerBattle] captured: failed=%s (%s) directorSeen=%s channelCaptured=%s "
				"channelTrainer=%u (want %u) configCaptured=%s fleeAllowed=%s (want false) "
				"catchAllowed=%s (want false) tier=%u (want %u) enemySpecies=%u (want %u) "
				"winnerCaptured=%s winner=%d (want PLAYER=%d) baselineCaptured=%s leadInstalled=%s "
				"moneyBefore=%u flagBefore=%s (want false) afterCaptured=%s moneyAfter=%u "
				"(want before+%u) flagAfter=%s (want true) whiteoutAfter=%s (want false) "
				"completed=%u (want 1) aborted=%u (want 0) stateAfter=%u (want %u=IDLE) "
				"battleUnloaded=%s",
				g_bTBFailed ? "true" : "false", g_szTBFailure,
				g_bTBDirectorSeen ? "true" : "false",
				g_bTBChannelCaptured ? "true" : "false",
				(u_int)g_eTBChannelTrainer, (u_int)eTB_TRAINER,
				g_bTBConfigCaptured ? "true" : "false",
				g_bTBFleeAllowed ? "true" : "false",
				g_bTBCatchAllowed ? "true" : "false",
				(u_int)g_eTBTier, (u_int)xRow.m_eAITier,
				(u_int)g_eTBEnemySpecies, (u_int)eTB_ENEMY_LEAD,
				g_bTBWinnerCaptured ? "true" : "false",
				(int)g_eTBWinner, (int)ZM_SIDE_PLAYER,
				g_bTBBaselineCaptured ? "true" : "false",
				g_bTBLeadInstalled ? "true" : "false",
				g_uTBMoneyBefore, g_bTBFlagBefore ? "true" : "false",
				g_bTBAfterCaptured ? "true" : "false",
				g_uTBMoneyAfter, uTB_EXPECTED_PRIZE,
				g_bTBFlagAfter ? "true" : "false",
				g_bTBWhiteoutAfter ? "true" : "false",
				g_uTBCompletedAfter, g_uTBAbortedAfter,
				g_uTBStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE,
				g_bTBBattleUnloaded ? "true" : "false");

			if (g_bTBFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_TrainerBattle] %s", g_szTBFailure);
				bPassed = false;
			}

			// --- the ENTRY CHANNEL carried the trainer id ---
			if (!g_bTBChannelCaptured || g_eTBChannelTrainer != eTB_TRAINER)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the accepted round trip carried trainer %u, expected %u -- "
					"the second entry channel did not reach ZM_BattleTransition",
					(u_int)g_eTBChannelTrainer, (u_int)eTB_TRAINER);
				bPassed = false;
			}

			// --- it really was a TRAINER battle (the config the director Begun with) ---
			if (!g_bTBDirectorSeen || !g_bTBConfigCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the BattleDirector never reached a Begun phase -- the "
					"trainer arm was never observed");
				bPassed = false;
			}
			else
			{
				if (g_bTBFleeAllowed)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the battle allowed FLEEING -- the wild config was used, "
						"not BuildTrainerBattleConfig()");
					bPassed = false;
				}
				if (g_bTBCatchAllowed)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the battle allowed CATCHING -- another trainer's monster "
						"can never be caught");
					bPassed = false;
				}
				// The ROW's tier reached Begin's 7th argument, and it is a real tier.
				if ((u_int)g_eTBTier != (u_int)xRow.m_eAITier)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the battle Begun with AI tier %u, the roster row authors %u "
						"-- the row's tier never reached ZM_BattleDirectorCore::Begin",
						(u_int)g_eTBTier, (u_int)xRow.m_eAITier);
					bPassed = false;
				}
				if ((u_int)g_eTBTier >= (u_int)ZM_AI_TIER_COUNT)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the battle Begun with the ZM_AI_TIER_NONE sentinel -- an "
						"opponent with no action policy");
					bPassed = false;
				}
				if (g_eTBEnemySpecies != eTB_ENEMY_LEAD)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the enemy active species was %u, expected the roster row's "
						"lead %u -- the fixed party was not built from the row",
						(u_int)g_eTBEnemySpecies, (u_int)eTB_ENEMY_LEAD);
					bPassed = false;
				}
			}

			// --- the player WON (non-vacuity for the payout below) ---
			if (!g_bTBWinnerCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the core never reached OVER while the Battle scene was loaded "
					"-- no winner was captured");
				bPassed = false;
			}
			else if (g_eTBWinner != ZM_SIDE_PLAYER)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the battle winner was side %d, expected PLAYER %d -- raise "
					"uTB_PLAYER_LEAD_LEVEL (the single difficulty knob), never weaken this assert",
					(int)g_eTBWinner, (int)ZM_SIDE_PLAYER);
				bPassed = false;
			}

			// --- ANTI-VACUITY on the payout ---
			if (!g_bTBBaselineCaptured || !g_bTBLeadInstalled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] no pre-encounter baseline was captured -- the payout deltas "
					"below are vacuous");
				bPassed = false;
			}
			else
			{
				if (g_bTBFlagBefore)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] ZM_STORY_FLAG_RIVAL1_DEFEATED was ALREADY set before the "
						"encounter -- the flag transition is vacuous");
					bPassed = false;
				}
				if (g_uTBMoneyBefore + uTB_EXPECTED_PRIZE >= uZM_MONEY_CAP)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the purse (%u) plus the prize is at/over the cap -- the "
						"credit delta would be a saturation artefact", g_uTBMoneyBefore);
					bPassed = false;
				}
			}

			// --- THE PAYOUT ---
			if (!g_bTBAfterCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] no ZM_GameState resolved after the resume -- the payout was "
					"never sampled");
				bPassed = false;
			}
			else
			{
				if (g_uTBMoneyAfter != g_uTBMoneyBefore + uTB_EXPECTED_PRIZE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the purse went %u -> %u, expected exactly +%u (the rival's "
						"authored prize)", g_uTBMoneyBefore, g_uTBMoneyAfter, uTB_EXPECTED_PRIZE);
					bPassed = false;
				}
				if (!g_bTBFlagAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] ZM_STORY_FLAG_RIVAL1_DEFEATED was NOT set after beating the "
						"rival -- the defeat-flag write never reached the persistent GameState");
					bPassed = false;
				}
				if (g_bTBWhiteoutAfter)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] a WIN latched m_bPendingWhiteout -- the loss path fired on "
						"a won battle");
					bPassed = false;
				}
			}

			// --- the round trip closed cleanly ---
			if (g_uTBCompletedAfter != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] completed battle count was %u, expected exactly 1 (the test "
					"never calls RequestBattleEnd -- the director must end it)", g_uTBCompletedAfter);
				bPassed = false;
			}
			if (g_uTBAbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] aborted transition count was %u, expected 0", g_uTBAbortedAfter);
				bPassed = false;
			}
			if (g_uTBStateAfter != (u_int)ZM_BATTLE_TRANSITION_IDLE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the transition settled in state %u, expected IDLE (%u)",
					g_uTBStateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE);
				bPassed = false;
			}
			if (!g_bTBBattleUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the Battle scene was still loaded after the resume");
				bPassed = false;
			}

			// ================= THE SECOND ROUND TRIP (the rambler) =================
			const ZM_TrainerData& xRow2 = ZM_GetTrainerData(eTB_TRAINER2);

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TrainerBattle] trip2 captured: channelCaptured=%s channelTrainer=%u (want %u) "
				"configCaptured=%s tier=%u (want %u, must NOT be GREEDY=%u) enemySpecies=%u (want %u) "
				"enemyPartySize=%u (want %u) winnerCaptured=%s winner=%d (want PLAYER=%d) "
				"moneyBefore=%u afterCaptured=%s moneyAfter=%u (want before+%u) completed=%u (want 2) "
				"aborted=%u (want 0) stateAfter=%u (want %u=IDLE)",
				g_bTB2ChannelCaptured ? "true" : "false",
				(u_int)g_eTB2ChannelTrainer, (u_int)eTB_TRAINER2,
				g_bTB2ConfigCaptured ? "true" : "false",
				(u_int)g_eTB2Tier, (u_int)xRow2.m_eAITier, (u_int)ZM_AI_TIER_GREEDY,
				(u_int)g_eTB2EnemySpecies, (u_int)eTB_ENEMY_LEAD2,
				g_uTB2EnemyPartySize, uZM_TRAINER_BATTLEABLE_PARTY,
				g_bTB2WinnerCaptured ? "true" : "false",
				(int)g_eTB2Winner, (int)ZM_SIDE_PLAYER,
				g_uTB2MoneyBefore,
				g_bTB2AfterCaptured ? "true" : "false",
				g_uTB2MoneyAfter, uTB_EXPECTED_PRIZE2,
				g_uTB2CompletedAfter, g_uTB2AbortedAfter,
				g_uTB2StateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE);

			// ANTI-VACUITY ON THE TIER PROOF, stated as a HARD assertion so it can
			// never silently degenerate again: ZM_AI_TIER_GREEDY is BOTH the wild arm's
			// hard-coded literal AND ZM_BattleDirectorCore's pre-Begin member default,
			// so a tier observation equal to GREEDY proves NOTHING about the trainer
			// arm's call site. The second trip exists specifically to observe a tier
			// neither of those can produce.
			if (xRow2.m_eAITier == ZM_AI_TIER_GREEDY)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] trainer %u now authors ZM_AI_TIER_GREEDY -- that is the wild "
					"arm's constant AND the core's pre-Begin default, so this gate can no longer "
					"distinguish the trainer arm's call site. Point eTB_TRAINER2 at a row with a "
					"different tier, do not weaken the assertion below",
					(u_int)eTB_TRAINER2);
				bPassed = false;
			}
			if (xRow2.m_eAITier == ZM_GetTrainerData(eTB_TRAINER).m_eAITier)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] both driven rows now author the SAME AI tier -- a "
					"constant-passing trainer arm would satisfy both trips");
				bPassed = false;
			}

			if (!g_bTB2ChannelCaptured || g_eTB2ChannelTrainer != eTB_TRAINER2)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the SECOND accepted round trip carried trainer %u, expected %u",
					(u_int)g_eTB2ChannelTrainer, (u_int)eTB_TRAINER2);
				bPassed = false;
			}

			if (!g_bTB2ConfigCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the SECOND battle never reached a Begun phase -- the tier and "
					"clamp proofs were never sampled");
				bPassed = false;
			}
			else
			{
				if ((u_int)g_eTB2Tier != (u_int)xRow2.m_eAITier)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the SECOND battle Begun with AI tier %u, the roster row "
						"authors %u -- the row's tier never reached ZM_BattleDirectorCore::Begin",
						(u_int)g_eTB2Tier, (u_int)xRow2.m_eAITier);
					bPassed = false;
				}
				// THE CLAMP, end to end: the engine was handed exactly the number of
				// enemy monsters it can resolve. It has no forced replacement on faint,
				// so a bigger party faint-locks it into
				// Zenith_Assert(!xActive.IsFainted(), ...) -- a break in EVERY config.
				if (g_uTB2EnemyPartySize != uZM_TRAINER_BATTLEABLE_PARTY)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the SECOND battle fielded %u enemy monsters, expected %u -- "
						"this row AUTHORS more, so the battleable clamp did not reach Begin",
						g_uTB2EnemyPartySize, uZM_TRAINER_BATTLEABLE_PARTY);
					bPassed = false;
				}
				if (g_eTB2EnemySpecies != eTB_ENEMY_LEAD2)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TrainerBattle] the SECOND battle's enemy active was %u, expected the row's "
						"FIRST member %u -- the clamp dropped the front of the party, not the tail",
						(u_int)g_eTB2EnemySpecies, (u_int)eTB_ENEMY_LEAD2);
					bPassed = false;
				}
			}

			// The multi-member row RESOLVES: reaching a winner at all is what proves the
			// engine was never faint-locked.
			if (!g_bTB2WinnerCaptured || g_eTB2Winner != ZM_SIDE_PLAYER)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the SECOND battle's winner was side %d (captured=%s), expected "
					"PLAYER %d", (int)g_eTB2Winner, g_bTB2WinnerCaptured ? "true" : "false",
					(int)ZM_SIDE_PLAYER);
				bPassed = false;
			}

			// The ZM_STORY_FLAG_NONE payout arm on a REAL round trip: the prize is
			// credited even though the row writes no flag.
			if (!g_bTB2AfterCaptured)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] no ZM_GameState resolved after the second resume");
				bPassed = false;
			}
			else if (g_uTB2MoneyAfter != g_uTB2MoneyBefore + uTB_EXPECTED_PRIZE2)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the purse went %u -> %u over the second battle, expected "
					"exactly +%u (the rambler's authored prize, on a row that writes NO flag)",
					g_uTB2MoneyBefore, g_uTB2MoneyAfter, uTB_EXPECTED_PRIZE2);
				bPassed = false;
			}

			if (g_uTB2CompletedAfter != 2u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] completed battle count was %u after both trips, expected 2",
					g_uTB2CompletedAfter);
				bPassed = false;
			}
			if (g_uTB2AbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] aborted transition count was %u after both trips, expected 0",
					g_uTB2AbortedAfter);
				bPassed = false;
			}
			if (g_uTB2StateAfter != (u_int)ZM_BATTLE_TRANSITION_IDLE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TrainerBattle] the transition settled in state %u after both trips, expected "
					"IDLE (%u)", g_uTB2StateAfter, (u_int)ZM_BATTLE_TRANSITION_IDLE);
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure: drop
		// the fixed timestep, clear the instant-battles flag, clear BOTH of the
		// transition's ownerless channel latches, re-seed the persistent GameState
		// (this test replaced the party), force-unload any lingering Battle scene,
		// restore FrontEnd, then wipe input.
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetGameStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bTBActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bTBActive = false;

		return bPassed || !g_bTBPrereqsPresent;
	}
}

// m_bRequiresGraphics = FALSE: this test asserts nothing about pixels or grass
// blades, so it runs FOR REAL on the Null (GPU-less) backend instead of skipping
// as a pass -- which is exactly the CI signal the vertical's core needs.
static const Zenith_AutomatedTest g_xZMTrainerBattleTest = {
	"ZM_TrainerBattle_Test",
	&Setup_ZMTrainerBattle,
	&Step_ZMTrainerBattle,
	&Verify_ZMTrainerBattle,
	// Two round trips: the ready wait (<=420) plus TWO x (in-battle <=600 + drive
	// <=900 + settle 8). The harness jumps straight to Verify when maxFrames is hit,
	// so this must exceed the sum of the phase deadlines or a slow-but-valid run
	// would be cut off mid-battle and read as a failure rather than a timeout.
	/* maxFrames */ 4200,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTrainerBattleTest);

#endif // ZENITH_INPUT_SIMULATOR
