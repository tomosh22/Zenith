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
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_GrassDensityMap.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_BattleTransition -- the S5 item-3 (SC3b) windowed gate. ONE test,
// m_bRequiresGraphics = true: ZM_BattleEncounterLatch_Test drives the authored
// Dawnmere player onto a real grass tile and proves the persistent
// ZM_BattleTransition singleton (order 110) OBSERVED the ZM_OnWildEncounter that
// ZM_TallGrassSystem dispatched -- i.e. that its OnStart subscription is really
// bound to the LIVE Zenith_EventDispatcher, end to end, with no test-authored
// subscriber standing in for the game's.
//
// It also pins the HANDOFF: accepting an encounter must leave the battle machine
// owning the screen (OwnsFade, i.e. no longer IDLE). Until SC4 this asserted the
// opposite -- that the state stayed IDLE -- which pinned SC3b's deliberate
// scene-inertness; SC4 made the machine live and inverted that contract, so the
// assertion moved with the behaviour. The precise state is deliberately NOT
// asserted: AcceptPendingEncounter enters FADING_OUT in the same OnUpdate that
// makes the count visible, but the 0.20 s fade means the exact state sampled is a
// race, whereas "no longer IDLE" is not.
//
// NO Zenith_EventDispatcher::ScopedTestIsolation -- deliberately, and this is
// load-bearing. That guard STEALS the live subscription tables and leaves the
// dispatcher EMPTY for its lifetime (see its contract + ctor in
// ZenithECS/Zenith_EventSystem.h). Constructing one here would delete the very
// subscriber under test, and GetObservedEncounterCount() would sit at 0 forever.
// This test adds NO subscription of its own -- it reads the game's own component
// -- so there is nothing to isolate and nothing that can leak.
//
// GATING (C4/C6): requires graphics, so the headless CI batch skips it (no GPU)
// and the unit baseline is unchanged. Setup RequestSkip()s when the baked
// Dawnmere terrain is absent -- all git-ignored, so a fresh CI checkout skips
// rather than fails. Only a windowed *_True run bakes + loads + drives the slice.
// ============================================================================

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;

	// -------------------------------------------------------------------------
	// Shared asset guards + entity views (mirrors ZM_AutoTests_TallGrass.cpp)
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

	// The unique ZM_TerrainGrass entity in the active scene (Dawnmere's terrain).
	// It shares its entity with Zenith_TerrainComponent, so it is the entity onto
	// which the gameplay ZM_TallGrassSystem is attached at runtime.
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

	// The persistent ZM_BattleTransition singleton, resolved FRESH. The component
	// pool relocates entries on swap-and-pop, so this pointer must never be cached
	// across frames -- every caller re-resolves through the generation-bearing ID.
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
	// Direction search (mirrors ZM_AutoTests_TallGrass.cpp)
	// -------------------------------------------------------------------------

	// Probe the terrain density map (the SAME map, sampled the SAME way --
	// ZM_GrassDensityMap::SampleWorld -- the tall-grass system reads) outward from
	// the player along each cardinal, and pick the nearest one that crosses the
	// grass threshold. World<->key mapping assumes the follow-camera forward is ~+Z
	// at spawn, which ZM_DawnmerePlayerCamera_Test proves.
	constexpr float fBL_SEARCH_MIN_DIST = 1.5f;   // >= 1 tile so the destination is a genuine transition
	constexpr float fBL_SEARCH_MAX_DIST = 24.0f;
	constexpr float fBL_SEARCH_STEP     = 0.5f;

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
			for (float fDist = fBL_SEARCH_MIN_DIST;
				fDist <= fBL_SEARCH_MAX_DIST;
				fDist += fBL_SEARCH_STEP)
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
	// ZM_BattleEncounterLatch_Test
	// -------------------------------------------------------------------------

	enum class LatchPhase
	{
		AwaitReady,
		Baseline,
		Walk,
		Done,
	};

	constexpr int iBL_READY_DEADLINE  = 420;   // Dawnmere first-load ready window (TallGrass parity)
	constexpr int iBL_BASELINE_FRAMES = 4;     // let OnUpdate set its baseline tile before we drive
	constexpr int iBL_WALK_DEADLINE   = 460;   // ample budget to reach a <= 24 m grass tile at walk speed
	// 420 + 4 + 460 = 884 < the 900-frame cap, so every phase owns a real deadline.

	LatchPhase  g_eBLPhase          = LatchPhase::Done;
	int         g_iBLPhaseFrames    = 0;
	bool        g_bBLPrereqsPresent = false;
	bool        g_bBLActive         = false;
	bool        g_bBLFailed         = false;
	const char* g_szBLFailure       = "test did not reach verification";

	// Captured from the singleton ZM_BattleTransition on the frame it latched.
	u_int                      g_uBLObservedCount = 0u;
	ZM_SPECIES_ID              g_eBLSpecies       = ZM_SPECIES_NONE;
	u_int                      g_uBLLevel         = 0u;
	ZM_SCENE_ID                g_eBLScene         = ZM_SCENE_NONE;
	ZM_BATTLE_TRANSITION_STATE g_eBLState         = ZM_BATTLE_TRANSITION_IDLE;

	// Chosen walk direction (data-driven from the density map).
	Zenith_KeyCode g_eBLWalkKey     = ZENITH_KEY_W;
	float          g_fBLHitDistance = 0.0f;
	float          g_fBLHitDensity  = 0.0f;

	void FailBL(const char* szReason)
	{
		g_szBLFailure = szReason;
		g_bBLFailed = true;
		g_eBLPhase = LatchPhase::Done;
		Zenith_InputSimulator::SetKeyHeld(g_eBLWalkKey, false);
	}

	void Setup_ZMBattleEncounterLatch()
	{
		g_eBLPhase          = LatchPhase::Done;
		g_iBLPhaseFrames    = 0;
		g_bBLPrereqsPresent = RequiredDawnmereAssetsPresent();
		g_bBLActive         = false;
		g_bBLFailed         = false;
		g_szBLFailure       = "test did not reach verification";
		g_uBLObservedCount  = 0u;
		g_eBLSpecies        = ZM_SPECIES_NONE;
		g_uBLLevel          = 0u;
		g_eBLScene          = ZM_SCENE_NONE;
		g_eBLState          = ZM_BATTLE_TRANSITION_IDLE;
		g_eBLWalkKey        = ZENITH_KEY_W;
		g_fBLHitDistance    = 0.0f;
		g_fBLHitDensity     = 0.0f;

		// Asset guard FIRST -- RequestSkip bypasses Verify, so no fixed-dt / scene
		// state is installed until the bake is known present.
		if (!g_bBLPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere scene/terrain bake is absent or incomplete");
			return;
		}

		// The transition singleton owns ownerless statics (the pending-encounter
		// latch); clear them so an earlier batched test cannot bleed a latch in.
		// This deliberately does NOT touch the live subscription.
		ZM_BattleTransition::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eBLPhase = LatchPhase::AwaitReady;
		g_bBLActive = true;
	}

	bool Step_ZMBattleEncounterLatch(int)
	{
		if (!g_bBLActive || g_bBLFailed || g_eBLPhase == LatchPhase::Done)
		{
			return false;
		}

		++g_iBLPhaseFrames;
		switch (g_eBLPhase)
		{
		case LatchPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iBLPhaseFrames > iBL_READY_DEADLINE)
				{
					FailBL("Dawnmere did not become runtime-ready (player + collider + "
						"grounded + follow-camera + grass) in time");
					return false;
				}
				return true;
			}

			// The battle machine is a persistent-scene singleton authored on the
			// FrontEnd root, so it must already exist here -- if it does not, the
			// subscription under test cannot exist either.
			if (ResolveSingletonBattleTransition() == nullptr)
			{
				FailBL("no unique ZM_BattleTransition singleton resolved (the persistent "
					"ZM_BattleTransitionRoot is missing -- re-bake FrontEnd.zscen)");
				return false;
			}

			// Resolve the terrain entity and attach the gameplay tall-grass system.
			// A runtime AddComponent does NOT fire OnAwake, so we call it manually --
			// it loads the density map from the terrain sibling and seeds the RNG.
			Zenith_EntityID xTerrainID = INVALID_ENTITY_ID;
			if (!FindActiveTerrainGrassEntity(xTerrainID))
			{
				FailBL("no ZM_TerrainGrass entity found in the active Dawnmere scene");
				return false;
			}
			Zenith_Entity xTerrain = g_xEngine.Scenes().ResolveEntity(xTerrainID);
			if (!xTerrain.IsValid())
			{
				FailBL("terrain entity did not resolve to a live handle");
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
				FailBL("tall-grass density map did not load after manual OnAwake");
				return false;
			}
			pxSystem->SetRngSeedForTests(0xABCull);
			pxSystem->ForceEncounterOnNextTransitionForTests(ZM_SPECIES_FERNFAWN, 5u);

			// Data-driven direction pick from the SAME density map the system reads.
			ZM_TerrainGrass* pxGrass = xTerrain.TryGetComponent<ZM_TerrainGrass>();
			if (pxGrass == nullptr || !pxGrass->HasCPUMap())
			{
				FailBL("terrain density map is not available for direction sampling");
				return false;
			}
			const WalkChoice xChoice = ChooseWalkDirection(
				xPlayer.m_xPosition.x, xPlayer.m_xPosition.z, pxGrass->GetDensityMap());
			if (!xChoice.m_bFound)
			{
				FailBL("no cardinal direction from the spawn reaches a grass tile within "
					"the search radius (spawn neighbourhood has no reachable grass)");
				return false;
			}
			g_eBLWalkKey     = xChoice.m_eKey;
			g_fBLHitDistance = xChoice.m_fHitDistance;
			g_fBLHitDensity  = xChoice.m_fHitDensity;

			g_eBLPhase = LatchPhase::Baseline;
			g_iBLPhaseFrames = 0;
			return true;
		}

		case LatchPhase::Baseline:
			// Let the system's per-frame OnUpdate establish its baseline tile (the
			// first update after OnAwake only records the tile; it never transitions).
			if (g_iBLPhaseFrames < iBL_BASELINE_FRAMES)
			{
				return true;
			}
			Zenith_InputSimulator::SetKeyHeld(g_eBLWalkKey, true);
			g_eBLPhase = LatchPhase::Walk;
			g_iBLPhaseFrames = 0;
			return true;

		case LatchPhase::Walk:
		{
			// Re-resolve EVERY frame: the pool relocates components, so a cached
			// pointer would dangle across any add/remove the frame performs.
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailBL("the ZM_BattleTransition singleton stopped resolving mid-walk");
				return false;
			}

			// The system dispatches synchronously inside its OnUpdate and the
			// transition drains the latch in its own OnUpdate, so the observed count
			// rises within a frame or two of the first on-grass transition.
			if (pxTransition->GetObservedEncounterCount() > 0u)
			{
				g_uBLObservedCount = pxTransition->GetObservedEncounterCount();
				g_eBLSpecies       = pxTransition->GetBattleSpecies();
				g_uBLLevel         = pxTransition->GetBattleLevel();
				g_eBLScene         = pxTransition->GetSourceScene();
				g_eBLState         = pxTransition->GetTransitionState();
				Zenith_InputSimulator::SetKeyHeld(g_eBLWalkKey, false);
				g_eBLPhase = LatchPhase::Done;
				return false;
			}
			if (g_iBLPhaseFrames > iBL_WALK_DEADLINE)
			{
				FailBL("held the chosen direction but the ZM_BattleTransition singleton "
					"never observed a ZM_OnWildEncounter before the walk deadline");
				return false;
			}
			return true;
		}

		case LatchPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMBattleEncounterLatch()
	{
		bool bPassed = true;

		if (g_bBLActive)
		{
			// Log EVERY captured value so a failure is fully localisable from the log.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleEncounterLatch] captured: observed=%u (want 1) species=%d "
				"(want FERNFAWN=%d) level=%u (want 5) scene=%d (want DAWNMERE=%d) "
				"state=%d (want non-IDLE; IDLE=%d) walkKey=%d hitDist=%f hitDensity=%f",
				g_uBLObservedCount,
				(int)g_eBLSpecies, (int)ZM_SPECIES_FERNFAWN,
				g_uBLLevel,
				(int)g_eBLScene, (int)ZM_SCENE_DAWNMERE,
				(int)g_eBLState, (int)ZM_BATTLE_TRANSITION_IDLE,
				(int)g_eBLWalkKey, (double)g_fBLHitDistance, (double)g_fBLHitDensity);

			if (g_bBLFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleEncounterLatch] %s", g_szBLFailure);
				bPassed = false;
			}
			if (g_uBLObservedCount != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleEncounterLatch] the singleton observed %u encounters, "
					"expected exactly 1 (0 = the game's live subscription is not wired)",
					g_uBLObservedCount);
				bPassed = false;
			}
			else
			{
				if (g_eBLSpecies != ZM_SPECIES_FERNFAWN)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleEncounterLatch] latched species was %d, expected FERNFAWN %d",
						(int)g_eBLSpecies, (int)ZM_SPECIES_FERNFAWN);
					bPassed = false;
				}
				if (g_uBLLevel != 5u)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleEncounterLatch] latched level was %u, expected 5", g_uBLLevel);
					bPassed = false;
				}
				if (g_eBLScene != ZM_SCENE_DAWNMERE)
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleEncounterLatch] latched source scene was %d, expected DAWNMERE %d",
						(int)g_eBLScene, (int)ZM_SCENE_DAWNMERE);
					bPassed = false;
				}
				// SC4 made the machine live: AcceptPendingEncounter increments the
				// observed count and enters FADING_OUT in the SAME OnUpdate, so by
				// the time the count is visible the machine already owns the screen.
				// Assert the OWNERSHIP invariant via the shipped predicate rather
				// than a specific fade frame -- the exact state is a race (the fade
				// runs 0.20 s), but "no longer IDLE" is not.
				if (!ZM_BattleTransition::OwnsFade(g_eBLState))
				{
					Zenith_Error(LOG_CATEGORY_UNITTEST,
						"[ZM_BattleEncounterLatch] transition state was %d (IDLE=%d): accepting an "
						"encounter must hand the screen to the battle machine",
						(int)g_eBLState, (int)ZM_BATTLE_TRANSITION_IDLE);
					bPassed = false;
				}
			}
		}

		// Always tear down, in order (all guarded). Release the key, drop the fixed
		// timestep, clear the transition's ownerless statics, restore FrontEnd, then
		// wipe input.
		Zenith_InputSimulator::SetKeyHeld(g_eBLWalkKey, false);
		Zenith_InputSimulator::ClearFixedDt();
		ZM_BattleTransition::ResetRuntimeStateForTests();
		if (g_bBLActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bBLActive = false;

		return bPassed || !g_bBLPrereqsPresent;
	}
}

static const Zenith_AutomatedTest g_xZMBattleEncounterLatchTest = {
	"ZM_BattleEncounterLatch_Test",
	&Setup_ZMBattleEncounterLatch,
	&Step_ZMBattleEncounterLatch,
	&Verify_ZMBattleEncounterLatch,
	/* maxFrames */ 900,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleEncounterLatchTest);

#endif // ZENITH_INPUT_SIMULATOR
