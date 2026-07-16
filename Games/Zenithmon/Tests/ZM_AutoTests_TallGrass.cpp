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
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_TallGrass -- the S5 item-2 SC4 windowed integration gate. Two
// tests, both m_bRequiresGraphics = true, prove the tall-grass encounter slice
// end-to-end against the baked Dawnmere terrain/grass:
//
//   1. ZM_TallGrassEncounter_Test -- drives the authored player onto a real
//      grass tile and asserts ZM_TallGrassSystem (order 109) dispatches exactly
//      the forced ZM_OnWildEncounter{FERNFAWN, 5, DAWNMERE} through the live
//      Zenith_EventDispatcher. Uses the explicit-species force seam so a
//      slot-less TOWN (Dawnmere) can drive an integration encounter.
//   2. ZM_TallGrassInteriorClear_Test -- proves engine E5 clears all grass on a
//      SINGLE load, now for an INTERIOR target (PlayerHome, build index 40).
//      Complementary to ZM_GrassRegeneration_Test, which covers the FrontEnd
//      target.
//
// GATING (C4/C6): both require graphics, so the headless CI batch skips them
// (no GPU) and the unit baseline is unchanged. Setup RequestSkip()s when the
// baked Dawnmere terrain (and, for test 2, PlayerHome.zscen) is absent -- all
// git-ignored, so a fresh CI checkout skips rather than fails. Only a windowed
// *_True run bakes + loads + drives the slice.
// ============================================================================

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;

	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (mirrors ZM_AutoTests_Overworld.cpp)
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

	// Locate the unique ZM_TerrainGrass entity in the active scene (Dawnmere's
	// terrain). It shares its entity with Zenith_TerrainComponent, so it is the
	// entity onto which the gameplay ZM_TallGrassSystem is attached at runtime.
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

	// -------------------------------------------------------------------------
	// Test 1 -- ZM_TallGrassEncounter_Test
	// -------------------------------------------------------------------------

	// Direction search: probe the terrain density map (the SAME map, sampled the
	// SAME way -- ZM_GrassDensityMap::SampleWorld -- the tall-grass system reads)
	// outward from the player along each cardinal, and pick the nearest one that
	// crosses the grass threshold. This makes the walk DATA-DRIVEN rather than a
	// blind guess: whichever cardinal actually has reachable grass is chosen.
	// World<->key mapping assumes the follow-camera forward is ~+Z at spawn, which
	// ZM_DawnmerePlayerCamera_Test proves (holding W moves the yaw-zero player +Z).
	constexpr float fENC_SEARCH_MIN_DIST = 1.5f;   // >= 1 tile so the destination is a genuine transition
	constexpr float fENC_SEARCH_MAX_DIST = 24.0f;
	constexpr float fENC_SEARCH_STEP     = 0.5f;

	struct WalkChoice
	{
		Zenith_KeyCode m_eKey        = ZENITH_KEY_W;
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
			for (float fDist = fENC_SEARCH_MIN_DIST;
				fDist <= fENC_SEARCH_MAX_DIST;
				fDist += fENC_SEARCH_STEP)
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

	enum class EncPhase
	{
		AwaitReady,
		Baseline,
		Walk,
		Done,
	};

	constexpr int iENC_READY_DEADLINE    = 420;   // Dawnmere first-load ready window (Overworld parity)
	constexpr int iENC_BASELINE_FRAMES   = 4;     // let OnUpdate set its baseline tile before we drive
	constexpr int iENC_WALK_DEADLINE     = 460;   // ample budget to reach a <= 24 m grass tile at walk speed

	EncPhase        g_eEncPhase          = EncPhase::Done;
	int             g_iEncPhaseFrames    = 0;
	bool            g_bEncPrereqsPresent = false;
	bool            g_bEncActive         = false;  // Setup installed isolation + subscription
	bool            g_bEncFailed         = false;
	const char*     g_szEncFailure       = "test did not reach verification";

	// Captured from the dispatched event (written by the subscription callback).
	bool            g_bEncFired          = false;
	ZM_SPECIES_ID   g_eEncSpecies        = ZM_SPECIES_NONE;
	u_int           g_uEncLevel          = 0u;
	ZM_SCENE_ID     g_eEncScene          = ZM_SCENE_NONE;

	// Chosen walk direction (data-driven from the density map).
	Zenith_KeyCode  g_eEncWalkKey        = ZENITH_KEY_W;
	float           g_fEncHitDistance    = 0.0f;
	float           g_fEncHitDensity     = 0.0f;

	// Heap-owned so it spans Setup -> Step -> Verify. Snapshots the live game
	// subscriptions on construct and restores them on destruct, so this test's
	// subscription cannot leak into a later test (or vice versa).
	Zenith_EventDispatcher::ScopedTestIsolation* g_pxEncIsolation = nullptr;
	Zenith_EventHandle g_uEncSubHandle = INVALID_EVENT_HANDLE;

	void FailEnc(const char* szReason)
	{
		g_szEncFailure = szReason;
		g_bEncFailed = true;
		g_eEncPhase = EncPhase::Done;
		Zenith_InputSimulator::SetKeyHeld(g_eEncWalkKey, false);
	}

	void Setup_ZMTallGrassEncounter()
	{
		g_eEncPhase          = EncPhase::Done;
		g_iEncPhaseFrames    = 0;
		g_bEncPrereqsPresent = RequiredDawnmereAssetsPresent();
		g_bEncActive         = false;
		g_bEncFailed         = false;
		g_szEncFailure       = "test did not reach verification";
		g_bEncFired          = false;
		g_eEncSpecies        = ZM_SPECIES_NONE;
		g_uEncLevel          = 0u;
		g_eEncScene          = ZM_SCENE_NONE;
		g_eEncWalkKey        = ZENITH_KEY_W;
		g_fEncHitDistance    = 0.0f;
		g_fEncHitDensity     = 0.0f;
		g_pxEncIsolation     = nullptr;
		g_uEncSubHandle      = INVALID_EVENT_HANDLE;

		// Asset guard FIRST -- RequestSkip bypasses Verify, so no fixed-dt / scene /
		// subscription state is installed until the bake is known present.
		if (!g_bEncPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere scene/terrain bake is absent or incomplete");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		// Install the event isolation + our subscription AFTER the scene load, so the
		// snapshot captures the live (game + Dawnmere) subscriptions and our test
		// subscription is the only extra one on the live dispatcher for the run.
		g_pxEncIsolation = new Zenith_EventDispatcher::ScopedTestIsolation();
		g_uEncSubHandle = Zenith_EventDispatcher::Get().Subscribe<ZM_OnWildEncounter>(
			[](const ZM_OnWildEncounter& xEvent)
			{
				g_bEncFired   = true;
				g_eEncSpecies = xEvent.m_eSpecies;
				g_uEncLevel   = xEvent.m_uLevel;
				g_eEncScene   = xEvent.m_eSourceScene;
			});

		g_eEncPhase = EncPhase::AwaitReady;
		g_bEncActive = true;
	}

	bool Step_ZMTallGrassEncounter(int)
	{
		if (!g_bEncActive || g_bEncFailed || g_eEncPhase == EncPhase::Done)
		{
			return false;
		}

		++g_iEncPhaseFrames;
		switch (g_eEncPhase)
		{
		case EncPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iEncPhaseFrames > iENC_READY_DEADLINE)
				{
					FailEnc("Dawnmere did not become runtime-ready (player + collider + "
						"grounded + follow-camera + grass) in time");
					return false;
				}
				return true;
			}

			// Resolve the terrain entity and attach the gameplay tall-grass system.
			// A runtime AddComponent does NOT fire OnAwake, so we call it manually --
			// it loads the density map from the terrain sibling and seeds the RNG.
			Zenith_EntityID xTerrainID = INVALID_ENTITY_ID;
			if (!FindActiveTerrainGrassEntity(xTerrainID))
			{
				FailEnc("no ZM_TerrainGrass entity found in the active Dawnmere scene");
				return false;
			}
			Zenith_Entity xTerrain = g_xEngine.Scenes().ResolveEntity(xTerrainID);
			if (!xTerrain.IsValid())
			{
				FailEnc("terrain entity did not resolve to a live handle");
				return false;
			}

			// The returned reference is valid only until the next ZM_TallGrassSystem
			// pool mutation, so call OnAwake + the arm seams IMMEDIATELY, with no
			// intervening component add/remove.
			ZM_TallGrassSystem* pxSystem = xTerrain.TryGetComponent<ZM_TallGrassSystem>();
			if (pxSystem == nullptr)
			{
				pxSystem = &xTerrain.AddComponent<ZM_TallGrassSystem>();
			}
			pxSystem->OnAwake();
			if (!pxSystem->HasDensityMap())
			{
				FailEnc("tall-grass density map did not load after manual OnAwake");
				return false;
			}
			pxSystem->SetRngSeedForTests(0xABCull);
			pxSystem->ForceEncounterOnNextTransitionForTests(ZM_SPECIES_FERNFAWN, 5u);

			// Data-driven direction pick from the SAME density map the system reads.
			ZM_TerrainGrass* pxGrass = xTerrain.TryGetComponent<ZM_TerrainGrass>();
			if (pxGrass == nullptr || !pxGrass->HasCPUMap())
			{
				FailEnc("terrain density map is not available for direction sampling");
				return false;
			}
			const WalkChoice xChoice = ChooseWalkDirection(
				xPlayer.m_xPosition.x, xPlayer.m_xPosition.z, pxGrass->GetDensityMap());
			if (!xChoice.m_bFound)
			{
				FailEnc("no cardinal direction from the spawn reaches a grass tile within "
					"the search radius (spawn neighbourhood has no reachable grass)");
				return false;
			}
			g_eEncWalkKey     = xChoice.m_eKey;
			g_fEncHitDistance = xChoice.m_fHitDistance;
			g_fEncHitDensity  = xChoice.m_fHitDensity;

			g_eEncPhase = EncPhase::Baseline;
			g_iEncPhaseFrames = 0;
			return true;
		}

		case EncPhase::Baseline:
			// Let the system's per-frame OnUpdate establish its baseline tile (the
			// first update after OnAwake only records the tile; it never transitions).
			if (g_iEncPhaseFrames < iENC_BASELINE_FRAMES)
			{
				return true;
			}
			Zenith_InputSimulator::SetKeyHeld(g_eEncWalkKey, true);
			g_eEncPhase = EncPhase::Walk;
			g_iEncPhaseFrames = 0;
			return true;

		case EncPhase::Walk:
			// Dispatch is synchronous inside the system's OnUpdate, so g_bEncFired is
			// visible within the same frame's Step. The explicit force stays armed
			// across off-grass tiles, so the FIRST on-grass transition fires it.
			if (g_bEncFired)
			{
				Zenith_InputSimulator::SetKeyHeld(g_eEncWalkKey, false);
				g_eEncPhase = EncPhase::Done;
				return false;
			}
			if (g_iEncPhaseFrames > iENC_WALK_DEADLINE)
			{
				FailEnc("held the chosen direction but no ZM_OnWildEncounter fired "
					"before the walk deadline");
				return false;
			}
			return true;

		case EncPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMTallGrassEncounter()
	{
		bool bPassed = true;

		if (g_bEncActive)
		{
			// Log EVERY captured value so a failure is fully localisable from the log.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TallGrassEncounter] captured: fired=%s species=%d (want FERNFAWN=%d) "
				"level=%u (want 5) scene=%d (want DAWNMERE=%d) walkKey=%d hitDist=%f hitDensity=%f",
				g_bEncFired ? "true" : "false",
				(int)g_eEncSpecies, (int)ZM_SPECIES_FERNFAWN,
				g_uEncLevel,
				(int)g_eEncScene, (int)ZM_SCENE_DAWNMERE,
				(int)g_eEncWalkKey, (double)g_fEncHitDistance, (double)g_fEncHitDensity);

			if (g_bEncFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_TallGrassEncounter] %s", g_szEncFailure);
				bPassed = false;
			}
			if (!g_bEncFired)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TallGrassEncounter] no ZM_OnWildEncounter was dispatched");
				bPassed = false;
			}
			else
			{
				if (g_eEncSpecies != ZM_SPECIES_FERNFAWN)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TallGrassEncounter] encounter species was %d, expected FERNFAWN %d",
						(int)g_eEncSpecies, (int)ZM_SPECIES_FERNFAWN);
					bPassed = false;
				}
				if (g_uEncLevel != 5u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TallGrassEncounter] encounter level was %u, expected 5", g_uEncLevel);
					bPassed = false;
				}
				if (g_eEncScene != ZM_SCENE_DAWNMERE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_TallGrassEncounter] encounter source scene was %d, expected DAWNMERE %d",
						(int)g_eEncScene, (int)ZM_SCENE_DAWNMERE);
					bPassed = false;
				}
			}
		}

		// Always tear down (all guarded). Delete the isolation FIRST -- while Dawnmere
		// is still loaded -- so its snapshot restores onto a live dispatcher whose
		// Dawnmere subscriptions are still valid; only then swap back to FrontEnd so
		// its OnStart subscriptions land in the properly-restored dispatcher.
		Zenith_InputSimulator::SetKeyHeld(g_eEncWalkKey, false);
		Zenith_InputSimulator::ClearFixedDt();
		if (g_pxEncIsolation != nullptr)
		{
			delete g_pxEncIsolation;   // dtor removes our test sub + restores game subs
			g_pxEncIsolation = nullptr;
		}
		g_uEncSubHandle = INVALID_EVENT_HANDLE;
		if (g_bEncActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bEncActive = false;

		return bPassed || !g_bEncPrereqsPresent;
	}

	// -------------------------------------------------------------------------
	// Test 2 -- ZM_TallGrassInteriorClear_Test
	// -------------------------------------------------------------------------
	// Shares the SINGLE-load grass-reset semantics with ZM_GrassRegeneration_Test
	// (which covers the FrontEnd target); this case exercises an INTERIOR target
	// (PlayerHome, build index 40) instead.

	enum class InteriorPhase
	{
		AwaitDawnmereGrass,
		AwaitInteriorSettle,
		Done,
	};

	constexpr int iINT_GRASS_DEADLINE   = 420;   // Dawnmere grass generation window
	constexpr int iINT_SETTLE_FRAMES    = 8;     // >= the 5-frame SINGLE-load settle the grass test uses

	InteriorPhase g_eIntPhase            = InteriorPhase::Done;
	int           g_iIntPhaseFrames      = 0;
	bool          g_bIntPrereqsPresent   = false;
	bool          g_bIntActive           = false;
	bool          g_bIntFailed           = false;
	const char*   g_szIntFailure         = "test did not reach verification";
	u_int         g_uIntDawnmereGrass    = 0u;   // grass generated on Dawnmere (> 0 expected)
	u_int         g_uIntInteriorGrass    = 0u;   // grass after the interior SINGLE load (0 expected)

	void FailInt(const char* szReason)
	{
		g_szIntFailure = szReason;
		g_bIntFailed = true;
		g_eIntPhase = InteriorPhase::Done;
	}

	void Setup_ZMTallGrassInteriorClear()
	{
		g_eIntPhase          = InteriorPhase::Done;
		g_iIntPhaseFrames    = 0;
		g_bIntActive         = false;
		g_bIntFailed         = false;
		g_szIntFailure       = "test did not reach verification";
		g_uIntDawnmereGrass  = 0u;
		g_uIntInteriorGrass  = 0u;

		const std::string strPlayerHome =
			std::string(GAME_ASSETS_DIR) + "Scenes/PlayerHome" + ZENITH_SCENE_EXT;
		g_bIntPrereqsPresent = RequiredDawnmereAssetsPresent() && DiskFilePresent(strPlayerHome);

		// Guard FIRST -- RequestSkip bypasses Verify.
		if (!g_bIntPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere or PlayerHome bake is absent or incomplete");
			return;
		}

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

		// Idempotent same-path re-register (the game boot already registers index 40).
		g_xEngine.Scenes().RegisterSceneBuildIndex(
			40, GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere
		g_eIntPhase = InteriorPhase::AwaitDawnmereGrass;
		g_bIntActive = true;
	}

	bool Step_ZMTallGrassInteriorClear(int)
	{
		if (!g_bIntActive || g_bIntFailed || g_eIntPhase == InteriorPhase::Done)
		{
			return false;
		}

		++g_iIntPhaseFrames;
		switch (g_eIntPhase)
		{
		case InteriorPhase::AwaitDawnmereGrass:
			if (g_xEngine.Grass().GetGeneratedInstanceCount() > 0u)
			{
				g_uIntDawnmereGrass = g_xEngine.Grass().GetGeneratedInstanceCount();
				g_xEngine.Scenes().LoadSceneByIndex(40, SCENE_LOAD_SINGLE);   // PlayerHome interior
				g_eIntPhase = InteriorPhase::AwaitInteriorSettle;
				g_iIntPhaseFrames = 0;
				return true;
			}
			if (g_iIntPhaseFrames > iINT_GRASS_DEADLINE)
			{
				FailInt("Dawnmere never generated any grass instances within the deadline");
				return false;
			}
			return true;

		case InteriorPhase::AwaitInteriorSettle:
			// SINGLE loads are deferred; let the outgoing grass state be torn down and
			// the interior (no terrain/grass) settle before capturing the count.
			if (g_iIntPhaseFrames < iINT_SETTLE_FRAMES)
			{
				return true;
			}
			g_uIntInteriorGrass = g_xEngine.Grass().GetGeneratedInstanceCount();
			g_eIntPhase = InteriorPhase::Done;
			return false;

		case InteriorPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMTallGrassInteriorClear()
	{
		bool bPassed = true;

		if (g_bIntActive)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_TallGrassInteriorClear] captured: dawnmereGrass=%u (want > 0) "
				"interiorGrass=%u (want 0)",
				g_uIntDawnmereGrass, g_uIntInteriorGrass);

			if (g_bIntFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TallGrassInteriorClear] %s", g_szIntFailure);
				bPassed = false;
			}
			if (g_uIntDawnmereGrass == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TallGrassInteriorClear] Dawnmere produced zero grass instances "
					"(cannot prove the clear)");
				bPassed = false;
			}
			if (g_uIntInteriorGrass != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_TallGrassInteriorClear] interior SINGLE load retained %u grass "
					"instances, expected 0 (E5 reset lock)", g_uIntInteriorGrass);
				bPassed = false;
			}
		}

		// Restore FrontEnd + clear the fixed timestep (all guarded).
		if (g_bIntActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ClearFixedDt();
		Zenith_InputSimulator::ResetAllInputState();
		g_bIntActive = false;

		return bPassed || !g_bIntPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMTallGrassEncounterTest = {
	"ZM_TallGrassEncounter_Test",
	&Setup_ZMTallGrassEncounter,
	&Step_ZMTallGrassEncounter,
	&Verify_ZMTallGrassEncounter,
	/* maxFrames */ 900,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTallGrassEncounterTest);

static const Zenith_AutomatedTest g_xZMTallGrassInteriorClearTest = {
	"ZM_TallGrassInteriorClear_Test",
	&Setup_ZMTallGrassInteriorClear,
	&Step_ZMTallGrassInteriorClear,
	&Verify_ZMTallGrassInteriorClear,
	/* maxFrames */ 600,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTallGrassInteriorClearTest);

#endif // ZENITH_INPUT_SIMULATOR
