#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // ZM_SetInstantBattlesForTests
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_BattleDirector -- the S5 item-4 (SC3) windowed gate. ONE test,
// m_bRequiresGraphics = true: ZM_BattleDirectorRoundTrip_Test proves that the
// live ZM_BattleDirector component -- NOT the test harness -- drives the headless
// AI-vs-AI battle to completion and calls ZM_BattleTransition::RequestBattleEnd()
// exactly once, and that every item-3 additive-load / pause / camera-switch /
// exact-resume invariant still holds when the director owns the exit.
//
// This CLONES ZM_BattleRoundTrip_Test in ZM_AutoTests_BattleTransition.cpp (the
// shipped item-3 SC5 gate) with two deliberate differences:
//   1. Setup flips zm_instant_battles ON (ZM_SetInstantBattlesForTests) so the
//      director's battle collapses to zero-duration ops and resolves well inside
//      the frame budget.
//   2. Step does NOT call RequestBattleEnd() itself. It walks the player onto
//      grass -> forced wild encounter -> captures the battle-side invariants at
//      the first IN_BATTLE frame -> then POLLS the transition until
//      GetCompletedBattleCount() == 1 (i.e. the DIRECTOR ended it). Verify then
//      asserts the director ended it (completed==1, aborted==0) plus the same
//      battle-side + exact-resume invariants the shipped round trip asserts.
//
// Like the shipped gate it constructs NO Zenith_EventDispatcher::ScopedTestIsolation
// (that guard would delete the game's own ZM_OnWildEncounter subscriber, the
// subject under test) and adds no subscription of its own. The shared entity-view
// / asset-guard / direction-search / arena-finder helpers are file-local (internal
// linkage) in the shipped TU and cannot be reused across TUs, so they are
// re-declared verbatim here.
//
// GATING mirrors the shipped gate (m_bRequiresGraphics = true + the RequestSkip
// guards on the git-ignored Dawnmere terrain, the authored Battle.zscen, and the
// baked PROP family). Creature model bundles are deliberately NOT gated on --
// under the director they are cosmetic and non-blocking. A fresh CI checkout has
// no baked Assets tree, so it skips rather than fails and the unit baseline is
// unchanged. Only a windowed *_True run bakes + loads + drives the slice.
// ============================================================================

namespace
{
	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (re-declared from ZM_AutoTests_Battle
	// Transition.cpp -- those copies are file-local and not linkable across TUs).
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
				bReady = bReady || xGrass.IsGrassApplied();
			});
		return bReady;
	}

	bool FindActiveTerrainGrassEntity(Zenith_EntityID& xOut)
	{
		xOut = INVALID_ENTITY_ID;
		g_xEngine.Scenes().QueryActiveScene<ZM_TerrainGrass>().ForEach(
			[&xOut](Zenith_EntityID xID, ZM_TerrainGrass&)
			{
				if (xOut == INVALID_ENTITY_ID)
				{
					xOut = xID;
				}
			});
		return xOut != INVALID_ENTITY_ID;
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

	// The persistent ZM_BattleTransition singleton, resolved FRESH each frame. The
	// component pool relocates entries on swap-and-pop, so this pointer must never
	// be cached across frames -- every caller re-resolves through the generation-
	// bearing ID.
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

	// -------------------------------------------------------------------------
	// Direction search (re-declared from ZM_AutoTests_BattleTransition.cpp)
	// -------------------------------------------------------------------------

	constexpr float fBD_SEARCH_MIN_DIST = 1.5f;   // >= 1 tile so the destination is a genuine transition
	constexpr float fBD_SEARCH_MAX_DIST = 24.0f;
	constexpr float fBD_SEARCH_STEP     = 0.5f;

	struct WalkChoice
	{
		Zenith_KeyCode m_eKey         = ZENITH_KEY_W;
		float          m_fHitDistance = 0.0f;
		float          m_fHitDensity  = 0.0f;
		bool           m_bFound       = false;
	};

	WalkChoice ChooseWalkDirection(float fPX, float fPZ, const ZM_GrassDensityMap& xMap)
	{
		struct Candidate
		{
			Zenith_KeyCode m_eKey;
			float          m_fDx;
			float          m_fDz;
		};
		// Priority order only breaks ties on equal distance; the nearest hit wins
		// across all four (shortest walk = least follow-camera drift = most robust).
		const Candidate axCandidates[4] = {
			{ ZENITH_KEY_W,  0.0f,  1.0f },   // camera-forward +Z at spawn
			{ ZENITH_KEY_S,  0.0f, -1.0f },
			{ ZENITH_KEY_D,  1.0f,  0.0f },
			{ ZENITH_KEY_A, -1.0f,  0.0f },
		};

		const float fThreshold = ZM_TallGrassSystem::fGRASS_DENSITY_THRESHOLD;
		WalkChoice xBest;
		for (const Candidate& xCand : axCandidates)
		{
			for (float fDist = fBD_SEARCH_MIN_DIST;
				fDist <= fBD_SEARCH_MAX_DIST;
				fDist += fBD_SEARCH_STEP)
			{
				const float fSampleX = fPX + xCand.m_fDx * fDist;
				const float fSampleZ = fPZ + xCand.m_fDz * fDist;
				const float fDensity = xMap.SampleWorld(fSampleX, fSampleZ);
				if (fDensity >= fThreshold)
				{
					if (!xBest.m_bFound || fDist < xBest.m_fHitDistance)
					{
						xBest.m_eKey = xCand.m_eKey;
						xBest.m_fHitDistance = fDist;
						xBest.m_fHitDensity = fDensity;
						xBest.m_bFound = true;
					}
					break;   // nearest hit for this cardinal
				}
			}
		}
		return xBest;
	}

	// -------------------------------------------------------------------------
	// ZM_BattleDirectorRoundTrip_Test
	// -------------------------------------------------------------------------

	// Coarser than 1/60 (mirrors the shipped round trip): the trip must fit the
	// additive load + arena build + the director's headless battle + grass regen
	// poll chains into the frame budget, and 1/60 would be too tight.
	constexpr float fBD_FIXED_DT = 1.0f / 30.0f;

	enum class BDPhase
	{
		AwaitReady,
		Baseline,
		Walk,
		AwaitInBattle,
		AwaitResume,
		Done,
	};

	constexpr int iBD_READY_DEADLINE       = 420;   // Dawnmere first-load ready window (round-trip parity)
	constexpr int iBD_BASELINE_FRAMES      = 4;     // let OnUpdate record its baseline tile before we drive
	constexpr int iBD_WALK_DEADLINE        = 460;   // ample budget to reach a <= 24 m grass tile at walk speed
	constexpr int iBD_INBATTLE_DEADLINE    = 600;   // fade-out + additive load + arena build + fade-in
	constexpr int iBD_RESUME_DEADLINE      = 600;   // director resolves the battle + fade-to-overworld + unload + regrow
	constexpr int iBD_RESUME_SETTLE_FRAMES = 8;     // let the resume settle before sampling the exact state
	// 420 + 4 + 460 + 600 + 600 = 2084 < the 2200-frame cap, with headroom: the
	// harness cuts to Verify on the FRAME BUDGET, so 2200 leaves margin over 2084.

	// Find the unique ZM_BattleArena across every loaded scene (the additive Battle
	// scene owns it). Mirrors FindUniqueArenaRT in ZM_AutoTests_BattleTransition.cpp.
	struct BDArenaView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_BattleArena* m_pxArena   = nullptr;
		u_int           m_uCount    = 0u;
	};

	bool FindUniqueArenaBD(BDArenaView& xOut)
	{
		xOut = BDArenaView{};
		g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().ForEach(
			[&xOut](Zenith_EntityID xEntityID, ZM_BattleArena& xArena)
			{
				++xOut.m_uCount;
				if (xOut.m_uCount == 1u)
				{
					xOut.m_xEntityID = xEntityID;
					xOut.m_pxArena = &xArena;
				}
			});
		return xOut.m_uCount == 1u && xOut.m_pxArena != nullptr;
	}

	// ---- Control state (all reset in Setup; batch mode reuses the process) ----
	BDPhase        g_eBDPhase             = BDPhase::Done;
	int            g_iBDPhaseFrames       = 0;
	int            g_iBDResumeSettle      = 0;
	bool           g_bBDResumeReached     = false;
	bool           g_bBDInBattleCaptured  = false;
	bool           g_bBDPrereqsPresent    = false;
	bool           g_bBDActive            = false;
	bool           g_bBDFailed            = false;
	const char*    g_szBDFailure          = "test did not reach verification";
	Zenith_KeyCode g_eBDWalkKey           = ZENITH_KEY_W;

	// ---- Entry captures (before the encounter) ----
	Zenith_Maths::Vector3 g_xBDEntryPlayerPos       = Zenith_Maths::Vector3(0.0f);  // DIAGNOSTIC ONLY
	Zenith_Scene          g_xBDEntryOverworldScene;
	u_int                 g_uBDEntryGrassBlades      = 0u;

	// ---- Walk captures (the latched encounter payload) ----
	ZM_SPECIES_ID g_eBDSeenSpecies     = ZM_SPECIES_NONE;
	u_int         g_uBDSeenLevel       = 0u;
	ZM_SCENE_ID   g_eBDSeenSourceScene = ZM_SCENE_NONE;

	// ---- IN_BATTLE captures ----
	bool                  g_bBDSeenFadeOpaque             = false;
	Zenith_Maths::Vector3 g_xBDParkedPos                  = Zenith_Maths::Vector3(0.0f);  // THE drift baseline
	bool                  g_bBDBattleSceneValid           = false;
	int                   g_iBDActiveBuildIndexInBattle   = -1;
	bool                  g_bBDOverworldPausedInBattle    = false;
	u_int                 g_uBDGrassInBattle              = 0u;
	bool                  g_bBDBattleCameraActive         = false;
	bool                  g_bBDArenaFullyBuilt            = false;
	ZM_BATTLE_BIOME       g_eBDArenaBiome                 = ZM_BATTLE_BIOME_COUNT;
	u_int                 g_uBDArenaChildrenInBattleScene = 0u;
	u_int                 g_uBDIssuedLoads                = 0u;

	// ---- Resume captures (exact restore) ----
	int                   g_iBDActiveBuildIndexAfter  = -1;
	bool                  g_bBDOverworldPausedAfter   = true;   // want false; default fails if uncaptured
	bool                  g_bBDBattleSceneUnloaded    = false;
	u_int                 g_uBDGrassAfter             = 0u;
	Zenith_Maths::Vector3 g_xBDResumePlayerPos        = Zenith_Maths::Vector3(0.0f);
	bool                  g_bBDPlayerMovementEnabled  = false;
	bool                  g_bBDPlayerResolved         = false;
	u_int                 g_uBDCompletedAfter         = 0u;   // the DIRECTOR's end request lands here
	u_int                 g_uBDAbortedAfter           = 0u;
	u_int                 g_uBDArenaCountAfter        = 0u;

	void FailBD(const char* szReason)
	{
		g_szBDFailure = szReason;
		g_bBDFailed = true;
		g_eBDPhase = BDPhase::Done;
		Zenith_InputSimulator::SetKeyHeld(g_eBDWalkKey, false);
	}

	void Setup_ZMBattleDirectorRoundTrip()
	{
		g_eBDPhase                      = BDPhase::Done;
		g_iBDPhaseFrames                = 0;
		g_iBDResumeSettle               = 0;
		g_bBDResumeReached              = false;
		g_bBDInBattleCaptured           = false;
		g_bBDPrereqsPresent             = false;
		g_bBDActive                     = false;
		g_bBDFailed                     = false;
		g_szBDFailure                   = "test did not reach verification";
		g_eBDWalkKey                    = ZENITH_KEY_W;

		g_xBDEntryPlayerPos             = Zenith_Maths::Vector3(0.0f);
		g_xBDEntryOverworldScene        = Zenith_Scene();
		g_uBDEntryGrassBlades           = 0u;

		g_eBDSeenSpecies                = ZM_SPECIES_NONE;
		g_uBDSeenLevel                  = 0u;
		g_eBDSeenSourceScene            = ZM_SCENE_NONE;

		g_bBDSeenFadeOpaque             = false;
		g_xBDParkedPos                  = Zenith_Maths::Vector3(0.0f);
		g_bBDBattleSceneValid           = false;
		g_iBDActiveBuildIndexInBattle   = -1;
		g_bBDOverworldPausedInBattle    = false;
		g_uBDGrassInBattle              = 0u;
		g_bBDBattleCameraActive         = false;
		g_bBDArenaFullyBuilt            = false;
		g_eBDArenaBiome                 = ZM_BATTLE_BIOME_COUNT;
		g_uBDArenaChildrenInBattleScene = 0u;
		g_uBDIssuedLoads                = 0u;

		g_iBDActiveBuildIndexAfter      = -1;
		g_bBDOverworldPausedAfter       = true;
		g_bBDBattleSceneUnloaded        = false;
		g_uBDGrassAfter                 = 0u;
		g_xBDResumePlayerPos            = Zenith_Maths::Vector3(0.0f);
		g_bBDPlayerMovementEnabled      = false;
		g_bBDPlayerResolved             = false;
		g_uBDCompletedAfter             = 0u;
		g_uBDAbortedAfter               = 0u;
		g_uBDArenaCountAfter            = 0u;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, instant-battles flag, scene load) until EVERY
		// git-ignored input is confirmed present -- the Dawnmere terrain/scene, the
		// authored Battle scene, and the baked PROP family (the arena's dressing
		// sets). Creature model bundles are deliberately NOT gated: under the
		// director they are cosmetic. A tools build bakes stale families and stamps
		// them; a non-tools build only checks the prop manifest. CI has no baked
		// Assets tree -> skip rather than fail.
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bBDPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bBDPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// Clear the transition's ownerless statics so an earlier batched test cannot
		// bleed a pending latch in. Does NOT touch the live subscription.
		ZM_BattleTransition::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fBD_FIXED_DT);

		// Collapse every presentation op to zero duration so the director's headless
		// battle resolves within the frame budget (the SOLE reason this gate needs it).
		ZM_SetInstantBattlesForTests(true);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eBDPhase = BDPhase::AwaitReady;
		g_bBDActive = true;
	}

	bool Step_ZMBattleDirectorRoundTrip(int)
	{
		if (!g_bBDActive || g_bBDFailed || g_eBDPhase == BDPhase::Done)
		{
			return false;
		}

		++g_iBDPhaseFrames;
		switch (g_eBDPhase)
		{
		case BDPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iBDPhaseFrames > iBD_READY_DEADLINE)
				{
					FailBD("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}

			// The battle machine is a persistent-scene singleton -- if it does not
			// resolve here, the subscription under test cannot exist either.
			if (ResolveSingletonBattleTransition() == nullptr)
			{
				FailBD("no unique ZM_BattleTransition singleton");
				return false;
			}

			// Resolve the terrain entity and attach the gameplay tall-grass system.
			Zenith_EntityID xTerrainID = INVALID_ENTITY_ID;
			if (!FindActiveTerrainGrassEntity(xTerrainID))
			{
				FailBD("no ZM_TerrainGrass entity in the active Dawnmere scene");
				return false;
			}
			Zenith_Entity xTerrain = g_xEngine.Scenes().ResolveEntity(xTerrainID);
			if (!xTerrain.IsValid())
			{
				FailBD("terrain entity did not resolve to a live handle");
				return false;
			}

			// A runtime AddComponent returns a reference valid only until the next
			// ZM_TallGrassSystem pool mutation, so call OnAwake + the arm seams
			// IMMEDIATELY, with no intervening component add/remove.
			ZM_TallGrassSystem* pxSystem = xTerrain.TryGetComponent<ZM_TallGrassSystem>();
			if (pxSystem == nullptr)
			{
				pxSystem = &xTerrain.AddComponent<ZM_TallGrassSystem>();
			}
			pxSystem->OnAwake();
			if (!pxSystem->HasDensityMap())
			{
				FailBD("tall-grass density map did not load after manual OnAwake");
				return false;
			}
			pxSystem->SetRngSeedForTests(0xABCull);
			pxSystem->ForceEncounterOnNextTransitionForTests(ZM_SPECIES_FERNFAWN, 5u);

			// Data-driven direction pick from the SAME density map the system reads.
			ZM_TerrainGrass* pxGrass = xTerrain.TryGetComponent<ZM_TerrainGrass>();
			if (pxGrass == nullptr || !pxGrass->HasCPUMap())
			{
				FailBD("terrain density map is not available for direction sampling");
				return false;
			}
			const WalkChoice xChoice = ChooseWalkDirection(
				xPlayer.m_xPosition.x, xPlayer.m_xPosition.z, pxGrass->GetDensityMap());
			if (!xChoice.m_bFound)
			{
				FailBD("no cardinal direction from the spawn reaches a grass tile");
				return false;
			}
			g_eBDWalkKey = xChoice.m_eKey;

			// Entry captures. The entry position is a LOGGED DIAGNOSTIC ONLY -- the
			// drift baseline is the parked position latched at the first IN_BATTLE
			// frame, since the player must move to trigger the encounter at all.
			g_xBDEntryPlayerPos      = xPlayer.m_xPosition;
			g_xBDEntryOverworldScene = g_xEngine.Scenes().GetActiveScene();
			g_uBDEntryGrassBlades    = g_xEngine.Grass().GetGeneratedInstanceCount();

			g_eBDPhase = BDPhase::Baseline;
			g_iBDPhaseFrames = 0;
			return true;
		}

		case BDPhase::Baseline:
			// Let the system's OnUpdate establish its baseline tile (the first update
			// after OnAwake only records the tile; it never transitions).
			if (g_iBDPhaseFrames < iBD_BASELINE_FRAMES)
			{
				return true;
			}
			Zenith_InputSimulator::SetKeyHeld(g_eBDWalkKey, true);
			g_eBDPhase = BDPhase::Walk;
			g_iBDPhaseFrames = 0;
			return true;

		case BDPhase::Walk:
		{
			// Re-resolve EVERY frame: the pool relocates components on swap-and-pop.
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBD("the ZM_BattleTransition singleton stopped resolving mid-walk");
				return false;
			}
			if (pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
			{
				// The encounter latched and the machine started; release the key now.
				Zenith_InputSimulator::SetKeyHeld(g_eBDWalkKey, false);
				g_eBDSeenSpecies     = pxTransition->GetBattleSpecies();
				g_uBDSeenLevel       = pxTransition->GetBattleLevel();
				g_eBDSeenSourceScene = pxTransition->GetSourceScene();
				g_eBDPhase = BDPhase::AwaitInBattle;
				g_iBDPhaseFrames = 0;
				return true;
			}
			if (g_iBDPhaseFrames > iBD_WALK_DEADLINE)
			{
				FailBD("walk deadline: never left IDLE");
				return false;
			}
			return true;
		}

		case BDPhase::AwaitInBattle:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBD("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
				return false;
			}

			// Latch the opaque-fade observation EVERY frame: the additive load must
			// only issue behind a fully-opaque screen, and by IN_BATTLE the fade has
			// already returned to transparent, so the endpoint alone proves nothing.
			g_bBDSeenFadeOpaque = g_bBDSeenFadeOpaque
				|| (pxTransition->GetFadeAlpha() >= 1.0f);

			if (!g_bBDInBattleCaptured
				&& pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
			{
				g_xBDParkedPos        = pxTransition->GetParkedPlayerPosition();
				g_bBDBattleSceneValid = pxTransition->GetBattleScene().IsValid();
				g_iBDActiveBuildIndexInBattle = g_xEngine.Scenes().GetSceneInfo(
					g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
				g_bBDOverworldPausedInBattle =
					g_xEngine.Scenes().IsScenePaused(g_xBDEntryOverworldScene);
				g_uBDGrassInBattle = g_xEngine.Grass().GetGeneratedInstanceCount();

				// The camera switch: the active main camera across scenes is now the
				// battle scene's own authored main camera.
				Zenith_SceneData* pxBattleData =
					g_xEngine.Scenes().GetSceneData(pxTransition->GetBattleScene());
				g_bBDBattleCameraActive = (pxBattleData != nullptr
					&& g_xEngine.Scenes().FindMainCameraEntityAcrossScenes()
						== pxBattleData->GetMainCameraEntity());

				// The arena: built + biome + every child owned by the battle scene.
				// It may not resolve on the very first IN_BATTLE frame; leave the
				// diagnostics at their failing defaults rather than aborting, so the
				// test stays diagnostic instead of hanging.
				BDArenaView xArena;
				if (FindUniqueArenaBD(xArena))
				{
					g_bBDArenaFullyBuilt = xArena.m_pxArena->IsFullyBuilt();
					g_eBDArenaBiome      = xArena.m_pxArena->GetActiveBiome();
					if (pxBattleData != nullptr)
					{
						for (u_int i = 0; i < ZM_BattleArena::uCHILD_COUNT; ++i)
						{
							const Zenith_EntityID xChildID = xArena.m_pxArena->GetChildEntityID(i);
							if (g_xEngine.Scenes().GetSceneDataForEntity(xChildID) == pxBattleData)
							{
								++g_uBDArenaChildrenInBattleScene;
							}
						}
					}
				}

				g_uBDIssuedLoads = pxTransition->GetIssuedLoadRequestCount();

				// The crucial difference from the item-3 round trip: this test does
				// NOT call RequestBattleEnd(). The live ZM_BattleDirector component in
				// the Battle scene drives its headless AI-vs-AI battle (collapsed by
				// zm_instant_battles) and calls the SOLE exit itself; AwaitResume just
				// waits for its GetCompletedBattleCount() to tick to 1.
				g_bBDInBattleCaptured = true;
				g_eBDPhase = BDPhase::AwaitResume;
				g_iBDPhaseFrames = 0;
				return true;
			}

			if (g_iBDPhaseFrames > iBD_INBATTLE_DEADLINE)
			{
				FailBD("never reached IN_BATTLE before deadline");
				return false;
			}
			return true;
		}

		case BDPhase::AwaitResume:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBD("the ZM_BattleTransition singleton stopped resolving during resume");
				return false;
			}

			if (!g_bBDResumeReached)
			{
				// The DIRECTOR must be the one that ends the battle: we never called
				// RequestBattleEnd, so completed==1 here proves the component drove
				// the AI-vs-AI battle to resolution and requested the exit itself.
				if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
					&& pxTransition->GetCompletedBattleCount() == 1u)
				{
					g_bBDResumeReached = true;
					g_iBDResumeSettle = 0;
					return true;
				}
				if (g_iBDPhaseFrames > iBD_RESUME_DEADLINE)
				{
					FailBD("resume deadline: the director never ended the battle "
						"(never returned to IDLE with completed==1)");
					return false;
				}
				return true;
			}

			// Let the resume settle before sampling the exact-restore invariants.
			++g_iBDResumeSettle;
			if (g_iBDResumeSettle < iBD_RESUME_SETTLE_FRAMES)
			{
				return true;
			}

			g_iBDActiveBuildIndexAfter = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
			g_bBDOverworldPausedAfter =
				g_xEngine.Scenes().IsScenePaused(g_xBDEntryOverworldScene);
			g_bBDBattleSceneUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
				std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();
			g_uBDGrassAfter = g_xEngine.Grass().GetGeneratedInstanceCount();

			PlayerView xPlayer2;
			if (FindActivePlayer(xPlayer2))
			{
				g_xBDResumePlayerPos       = xPlayer2.m_xPosition;
				g_bBDPlayerMovementEnabled = xPlayer2.m_pxController->IsMovementEnabled();
				g_bBDPlayerResolved        = true;
			}

			g_uBDCompletedAfter  = pxTransition->GetCompletedBattleCount();
			g_uBDAbortedAfter    = pxTransition->GetAbortedTransitionCount();
			g_uBDArenaCountAfter = g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().Count();

			g_eBDPhase = BDPhase::Done;
			return false;
		}

		case BDPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMBattleDirectorRoundTrip()
	{
		bool bPassed = true;

		if (g_bBDActive)
		{
			const float fDrift = glm::length(g_xBDResumePlayerPos - g_xBDParkedPos);

			// One line dumping EVERY captured value so a failure is fully localisable
			// from the log alone. drift is a MEASURED number (not a cliff) so a
			// regression shows as a trend; entryPos is a diagnostic only.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleDirectorRoundTrip] captured: failed=%s (%s) fadeOpaque=%s species=%d "
				"(want FERNFAWN=%d) level=%u (want 5) source=%d (want DAWNMERE=%d) "
				"issuedLoads=%u (want 1) battleSceneValid=%s buildInBattle=%d (want 1) "
				"battleCamActive=%s overworldPausedInBattle=%s grassInBattle=%u (want 0) "
				"entryGrass=%u arenaFullyBuilt=%s arenaChildren=%u (want %u) biome=%d "
				"(want MEADOW=%d) completedAfter=%u (want 1) abortedAfter=%u (want 0) "
				"buildAfter=%d (want 2) overworldPausedAfter=%s battleUnloaded=%s "
				"arenaCountAfter=%u (want 0) playerResolved=%s movementEnabled=%s "
				"grassAfter=%u (want %u) drift=%f (want <0.05) entryPos=(%f,%f,%f) "
				"parkedPos=(%f,%f,%f) resumePos=(%f,%f,%f)",
				g_bBDFailed ? "true" : "false", g_szBDFailure,
				g_bBDSeenFadeOpaque ? "true" : "false",
				(int)g_eBDSeenSpecies, (int)ZM_SPECIES_FERNFAWN,
				g_uBDSeenLevel,
				(int)g_eBDSeenSourceScene, (int)ZM_SCENE_DAWNMERE,
				g_uBDIssuedLoads,
				g_bBDBattleSceneValid ? "true" : "false",
				g_iBDActiveBuildIndexInBattle,
				g_bBDBattleCameraActive ? "true" : "false",
				g_bBDOverworldPausedInBattle ? "true" : "false",
				g_uBDGrassInBattle,
				g_uBDEntryGrassBlades,
				g_bBDArenaFullyBuilt ? "true" : "false",
				g_uBDArenaChildrenInBattleScene, ZM_BattleArena::uCHILD_COUNT,
				(int)g_eBDArenaBiome, (int)ZM_BATTLE_BIOME_MEADOW,
				g_uBDCompletedAfter,
				g_uBDAbortedAfter,
				g_iBDActiveBuildIndexAfter,
				g_bBDOverworldPausedAfter ? "true" : "false",
				g_bBDBattleSceneUnloaded ? "true" : "false",
				g_uBDArenaCountAfter,
				g_bBDPlayerResolved ? "true" : "false",
				g_bBDPlayerMovementEnabled ? "true" : "false",
				g_uBDGrassAfter, g_uBDEntryGrassBlades,
				(double)fDrift,
				(double)g_xBDEntryPlayerPos.x, (double)g_xBDEntryPlayerPos.y, (double)g_xBDEntryPlayerPos.z,
				(double)g_xBDParkedPos.x, (double)g_xBDParkedPos.y, (double)g_xBDParkedPos.z,
				(double)g_xBDResumePlayerPos.x, (double)g_xBDResumePlayerPos.y, (double)g_xBDResumePlayerPos.z);

			if (g_bBDFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_BattleDirectorRoundTrip] %s", g_szBDFailure);
				bPassed = false;
			}

			// --- the DIRECTOR ended it (the whole point of this gate) ---
			if (g_uBDCompletedAfter != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] completed battle count was %u, expected exactly 1 "
					"(the test never calls RequestBattleEnd -- the ZM_BattleDirector component must "
					"end the battle itself)", g_uBDCompletedAfter);
				bPassed = false;
			}
			if (g_uBDAbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the round trip recorded %u aborts, expected 0 (a "
					"clean director-ended round trip records zero aborts)", g_uBDAbortedAfter);
				bPassed = false;
			}

			// --- the additive load must only issue behind an opaque screen ---
			if (!g_bBDSeenFadeOpaque)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the fade never reached opaque -- the additive Battle "
					"load must only issue behind an opaque screen");
				bPassed = false;
			}

			// --- the latched encounter payload ---
			if (g_eBDSeenSpecies != ZM_SPECIES_FERNFAWN)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] latched species was %d, expected FERNFAWN %d",
					(int)g_eBDSeenSpecies, (int)ZM_SPECIES_FERNFAWN);
				bPassed = false;
			}
			if (g_uBDSeenLevel != 5u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] latched level was %u, expected 5", g_uBDSeenLevel);
				bPassed = false;
			}
			if (g_eBDSeenSourceScene != ZM_SCENE_DAWNMERE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] latched source scene was %d, expected DAWNMERE %d",
					(int)g_eBDSeenSourceScene, (int)ZM_SCENE_DAWNMERE);
				bPassed = false;
			}

			// --- the additive load ---
			if (g_uBDIssuedLoads != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] issued %u additive Battle loads, expected exactly 1",
					g_uBDIssuedLoads);
				bPassed = false;
			}
			if (!g_bBDBattleSceneValid)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the Battle scene handle was invalid at IN_BATTLE");
				bPassed = false;
			}

			// --- the camera switch ---
			if (g_iBDActiveBuildIndexInBattle != 1)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] active scene build index at IN_BATTLE was %d, expected "
					"1 (the Battle scene)", g_iBDActiveBuildIndexInBattle);
				bPassed = false;
			}
			if (!g_bBDBattleCameraActive)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the active main camera at IN_BATTLE was not the battle "
					"scene's own camera -- the camera switch did not happen");
				bPassed = false;
			}

			// --- the overworld is paused in IN_BATTLE (observed AND per the predicate) ---
			if (!g_bBDOverworldPausedInBattle)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the overworld scene was not paused at IN_BATTLE");
				bPassed = false;
			}
			if (!ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_IN_BATTLE))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the pure predicate IsOverworldPausedInState(IN_BATTLE) "
					"disagrees -- it must report the overworld paused in IN_BATTLE");
				bPassed = false;
			}

			// --- grass cleared entering battle ---
			if (g_uBDGrassInBattle != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] %u grass blades remained at IN_BATTLE, expected the "
					"overworld grass to be cleared (0)", g_uBDGrassInBattle);
				bPassed = false;
			}
			if (g_uBDEntryGrassBlades == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the overworld had 0 grass blades before the encounter "
					"-- the grass-clear/restore invariants are vacuous");
				bPassed = false;
			}

			// --- the arena ---
			if (!g_bBDArenaFullyBuilt)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the arena was not IsFullyBuilt() at IN_BATTLE");
				bPassed = false;
			}
			if (g_uBDArenaChildrenInBattleScene != ZM_BattleArena::uCHILD_COUNT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] only %u of %u arena children were owned by the Battle "
					"scene at IN_BATTLE", g_uBDArenaChildrenInBattleScene,
					ZM_BattleArena::uCHILD_COUNT);
				bPassed = false;
			}
			if (g_eBDArenaBiome != ZM_BATTLE_BIOME_MEADOW)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] arena biome was %d, expected MEADOW %d (the biome for a "
					"Dawnmere-launched battle)", (int)g_eBDArenaBiome, (int)ZM_BATTLE_BIOME_MEADOW);
				bPassed = false;
			}

			// --- EXACT resume ---
			if (g_iBDActiveBuildIndexAfter != 2)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] active scene build index after resume was %d, expected "
					"2 (Dawnmere)", g_iBDActiveBuildIndexAfter);
				bPassed = false;
			}
			if (g_bBDOverworldPausedAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the overworld scene was still paused after resume");
				bPassed = false;
			}
			if (!g_bBDBattleSceneUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the Battle scene was still loaded after resume");
				bPassed = false;
			}
			if (g_uBDArenaCountAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] %u ZM_BattleArena instances remained after resume, "
					"expected 0", g_uBDArenaCountAfter);
				bPassed = false;
			}
			if (!g_bBDPlayerResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the overworld player did not resolve after resume");
				bPassed = false;
			}
			if (!g_bBDPlayerMovementEnabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] player movement was not re-enabled after resume");
				bPassed = false;
			}
			if (fDrift >= 0.05f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] the resumed player drifted %f m from its parked "
					"position, expected < 0.05 (the parked body must not drift while the overworld is "
					"paused)", (double)fDrift);
				bPassed = false;
			}
			if (g_uBDGrassAfter != g_uBDEntryGrassBlades)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleDirectorRoundTrip] resumed grass blade count was %u, expected %u (resume "
					"must restore the same deterministic blade count)",
					g_uBDGrassAfter, g_uBDEntryGrassBlades);
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure:
		// release the key, drop the fixed timestep, clear the instant-battles flag,
		// clear the transition's ownerless statics, force-unload any lingering Battle
		// scene, restore FrontEnd, then wipe input.
		Zenith_InputSimulator::SetKeyHeld(g_eBDWalkKey, false);
		Zenith_InputSimulator::ClearFixedDt();
		ZM_SetInstantBattlesForTests(false);
		ZM_BattleTransition::ResetRuntimeStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bBDActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bBDActive = false;

		return bPassed || !g_bBDPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMBattleDirectorRoundTripTest = {
	"ZM_BattleDirectorRoundTrip_Test",
	&Setup_ZMBattleDirectorRoundTrip,
	&Step_ZMBattleDirectorRoundTrip,
	&Verify_ZMBattleDirectorRoundTrip,
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleDirectorRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
