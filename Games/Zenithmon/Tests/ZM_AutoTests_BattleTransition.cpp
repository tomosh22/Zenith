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
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
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

	// -------------------------------------------------------------------------
	// ZM_BattleRoundTrip_Test -- the S5 item-3 (SC5) end-to-end round-trip GATE.
	//
	// This is the whole slice, driven once through the shipped machinery: walk the
	// authored Dawnmere player onto a real grass tile -> forced wild encounter ->
	// ZM_BattleTransition fades to opaque, additively loads the Battle scene (build
	// index 1) BEHIND the opaque fade, parks + pauses the overworld, switches the
	// camera to the battle scene, clears the overworld grass, builds the arena ->
	// sits IN_BATTLE -> the test calls RequestBattleEnd() (the SOLE exit) -> the
	// machine unloads Battle, reactivates + unpauses the overworld, restores the
	// player, regrows grass -> back to IDLE. It asserts the battle-side invariants
	// at IN_BATTLE and the EXACT-resume invariants after.
	//
	// Like the latch test above it constructs NO Zenith_EventDispatcher::Scoped
	// TestIsolation (that guard would delete the game's own ZM_OnWildEncounter
	// subscriber, the subject under test) and adds no subscription of its own.
	//
	// The drift baseline is GetParkedPlayerPosition() -- the body position at the
	// instant it was PARKED -- NOT the entry position: the player walks >= 1 m to
	// trigger the encounter and keeps moving through the 0.20 s fade, so
	// entry->resume displacement is metres BY DESIGN. g_xRTEntryPlayerPos is a
	// LOGGED DIAGNOSTIC ONLY, never asserted.
	//
	// GATING mirrors the latch test (m_bRequiresGraphics = true + the RequestSkip
	// guards). It additionally needs the authored Battle.zscen and the baked PROP
	// family (the arena's dressing sets) -- both git-ignored, so a fresh CI
	// checkout skips rather than fails and the unit baseline is unchanged.
	// -------------------------------------------------------------------------

	// Coarser than the latch test's 1/60: the round trip must fit the additive
	// load + arena build + grass regen poll chains into the frame budget, and 1/60
	// would be too tight.
	constexpr float fRT_FIXED_DT = 1.0f / 30.0f;

	enum class RTPhase
	{
		AwaitReady,
		Baseline,
		Walk,
		AwaitInBattle,
		AwaitResume,
		Done,
	};

	constexpr int iRT_READY_DEADLINE       = 420;   // Dawnmere first-load ready window (latch parity)
	constexpr int iRT_BASELINE_FRAMES      = 4;     // let OnUpdate record its baseline tile before we drive
	constexpr int iRT_WALK_DEADLINE        = 460;   // ample budget to reach a <= 24 m grass tile at walk speed
	constexpr int iRT_INBATTLE_DEADLINE    = 600;   // fade-out + additive load + arena build + fade-in
	constexpr int iRT_RESUME_DEADLINE      = 600;   // fade-to-overworld + unload + reactivate + regrow
	constexpr int iRT_RESUME_SETTLE_FRAMES = 8;     // let the resume settle before sampling the exact state
	// 420 + 4 + 460 + 600 + 600 = 2084 < the 2200-frame cap, with headroom: the
	// harness cuts to Verify on the FRAME BUDGET, so 2200 leaves margin over 2084.

	// Find the unique ZM_BattleArena across every loaded scene (the additive Battle
	// scene owns it). Mirrors FindUniqueArena in ZM_AutoTests_BattleArena.cpp.
	struct RTArenaView
	{
		Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
		ZM_BattleArena* m_pxArena   = nullptr;
		u_int           m_uCount    = 0u;
	};

	bool FindUniqueArenaRT(RTArenaView& xOut)
	{
		xOut = RTArenaView{};
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
	RTPhase        g_eRTPhase             = RTPhase::Done;
	int            g_iRTPhaseFrames       = 0;
	int            g_iRTResumeSettle      = 0;
	bool           g_bRTResumeReached     = false;
	bool           g_bRTInBattleCaptured  = false;
	bool           g_bRTPrereqsPresent    = false;
	bool           g_bRTActive            = false;
	bool           g_bRTFailed            = false;
	const char*    g_szRTFailure          = "test did not reach verification";
	Zenith_KeyCode g_eRTWalkKey           = ZENITH_KEY_W;

	// ---- Entry captures (before the encounter) ----
	Zenith_Maths::Vector3 g_xRTEntryPlayerPos      = Zenith_Maths::Vector3(0.0f);  // DIAGNOSTIC ONLY
	Zenith_Scene          g_xRTEntryOverworldScene;
	u_int                 g_uRTEntryGrassBlades     = 0u;

	// ---- Walk captures (the latched encounter payload) ----
	ZM_SPECIES_ID g_eRTSeenSpecies     = ZM_SPECIES_NONE;
	u_int         g_uRTSeenLevel       = 0u;
	ZM_SCENE_ID   g_eRTSeenSourceScene = ZM_SCENE_NONE;

	// ---- IN_BATTLE captures ----
	bool                  g_bRTSeenFadeOpaque             = false;
	Zenith_Maths::Vector3 g_xRTParkedPos                  = Zenith_Maths::Vector3(0.0f);  // THE drift baseline
	bool                  g_bRTBattleSceneValid           = false;
	int                   g_iRTActiveBuildIndexInBattle   = -1;
	bool                  g_bRTOverworldPausedInBattle    = false;
	u_int                 g_uRTGrassInBattle              = 0u;
	bool                  g_bRTBattleCameraActive         = false;
	bool                  g_bRTArenaFullyBuilt            = false;
	ZM_BATTLE_BIOME       g_eRTArenaBiome                 = ZM_BATTLE_BIOME_COUNT;
	u_int                 g_uRTArenaChildrenInBattleScene = 0u;
	u_int                 g_uRTIssuedLoads                = 0u;
	bool                  g_bRTBattleEndAccepted          = false;

	// ---- Resume captures (exact restore) ----
	int                   g_iRTActiveBuildIndexAfter  = -1;
	bool                  g_bRTOverworldPausedAfter   = true;   // want false; default fails if uncaptured
	bool                  g_bRTBattleSceneUnloaded    = false;
	u_int                 g_uRTGrassAfter             = 0u;
	Zenith_Maths::Vector3 g_xRTResumePlayerPos        = Zenith_Maths::Vector3(0.0f);
	bool                  g_bRTPlayerMovementEnabled  = false;
	bool                  g_bRTPlayerResolved         = false;
	u_int                 g_uRTAbortedAfter           = 0u;
	u_int                 g_uRTArenaCountAfter        = 0u;

	void FailRT(const char* szReason)
	{
		g_szRTFailure = szReason;
		g_bRTFailed = true;
		g_eRTPhase = RTPhase::Done;
		Zenith_InputSimulator::SetKeyHeld(g_eRTWalkKey, false);
	}

	void Setup_ZMBattleRoundTrip()
	{
		g_eRTPhase                      = RTPhase::Done;
		g_iRTPhaseFrames                = 0;
		g_iRTResumeSettle               = 0;
		g_bRTResumeReached              = false;
		g_bRTInBattleCaptured           = false;
		g_bRTPrereqsPresent             = false;
		g_bRTActive                     = false;
		g_bRTFailed                     = false;
		g_szRTFailure                   = "test did not reach verification";
		g_eRTWalkKey                    = ZENITH_KEY_W;

		g_xRTEntryPlayerPos             = Zenith_Maths::Vector3(0.0f);
		g_xRTEntryOverworldScene        = Zenith_Scene();
		g_uRTEntryGrassBlades           = 0u;

		g_eRTSeenSpecies                = ZM_SPECIES_NONE;
		g_uRTSeenLevel                  = 0u;
		g_eRTSeenSourceScene            = ZM_SCENE_NONE;

		g_bRTSeenFadeOpaque             = false;
		g_xRTParkedPos                  = Zenith_Maths::Vector3(0.0f);
		g_bRTBattleSceneValid           = false;
		g_iRTActiveBuildIndexInBattle   = -1;
		g_bRTOverworldPausedInBattle    = false;
		g_uRTGrassInBattle              = 0u;
		g_bRTBattleCameraActive         = false;
		g_bRTArenaFullyBuilt            = false;
		g_eRTArenaBiome                 = ZM_BATTLE_BIOME_COUNT;
		g_uRTArenaChildrenInBattleScene = 0u;
		g_uRTIssuedLoads                = 0u;
		g_bRTBattleEndAccepted          = false;

		g_iRTActiveBuildIndexAfter      = -1;
		g_bRTOverworldPausedAfter       = true;
		g_bRTBattleSceneUnloaded        = false;
		g_uRTGrassAfter                 = 0u;
		g_xRTResumePlayerPos            = Zenith_Maths::Vector3(0.0f);
		g_bRTPlayerMovementEnabled      = false;
		g_bRTPlayerResolved             = false;
		g_uRTAbortedAfter               = 0u;
		g_uRTArenaCountAfter            = 0u;

		// Guard order is MANDATORY: RequestSkip bypasses Verify, so install NO
		// process state (fixed dt, scene load) until EVERY git-ignored input is
		// confirmed present -- the Dawnmere terrain/scene, the authored Battle
		// scene, and the baked PROP family (the arena's dressing sets). A tools
		// build bakes stale families and stamps them; a non-tools build only checks
		// the prop manifest. CI has no baked Assets tree -> skip rather than fail.
		const std::string strBattlePath =
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT;
#ifdef ZENITH_TOOLS
		const bool bWarm = ZM_BakeAllAssets();
#else
		const bool bWarm = ZM_BakeManifestCheck(
			ZM_ASSET_FAMILY_PROPS, std::filesystem::path(GAME_ASSETS_DIR));
#endif
		g_bRTPrereqsPresent = RequiredDawnmereAssetsPresent()
			&& DiskFilePresent(strBattlePath)
			&& bWarm;
		if (!g_bRTPrereqsPresent)
		{
			Zenith_AutomatedTestRunner::RequestSkip(
				"Dawnmere / Battle / prop bake absent -- run a *_True build");
			return;
		}

		// Clear the transition's ownerless statics so an earlier batched test cannot
		// bleed a pending latch in. Does NOT touch the live subscription.
		ZM_BattleTransition::ResetRuntimeStateForTests();

		Zenith_InputSimulator::ResetAllInputState();
		Zenith_InputSimulator::SetFixedDt(fRT_FIXED_DT);

		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere

		g_eRTPhase = RTPhase::AwaitReady;
		g_bRTActive = true;
	}

	bool Step_ZMBattleRoundTrip(int)
	{
		if (!g_bRTActive || g_bRTFailed || g_eRTPhase == RTPhase::Done)
		{
			return false;
		}

		++g_iRTPhaseFrames;
		switch (g_eRTPhase)
		{
		case RTPhase::AwaitReady:
		{
			PlayerView xPlayer;
			CameraView xCamera;
			if (!DawnmereRuntimeReady(xPlayer, xCamera))
			{
				if (g_iRTPhaseFrames > iRT_READY_DEADLINE)
				{
					FailRT("Dawnmere did not become runtime-ready in time");
					return false;
				}
				return true;
			}

			// The battle machine is a persistent-scene singleton -- if it does not
			// resolve here, the subscription under test cannot exist either.
			if (ResolveSingletonBattleTransition() == nullptr)
			{
				FailRT("no unique ZM_BattleTransition singleton");
				return false;
			}

			// Resolve the terrain entity and attach the gameplay tall-grass system.
			Zenith_EntityID xTerrainID = INVALID_ENTITY_ID;
			if (!FindActiveTerrainGrassEntity(xTerrainID))
			{
				FailRT("no ZM_TerrainGrass entity in the active Dawnmere scene");
				return false;
			}
			Zenith_Entity xTerrain = g_xEngine.Scenes().ResolveEntity(xTerrainID);
			if (!xTerrain.IsValid())
			{
				FailRT("terrain entity did not resolve to a live handle");
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
				FailRT("tall-grass density map did not load after manual OnAwake");
				return false;
			}
			pxSystem->SetRngSeedForTests(0xABCull);
			pxSystem->ForceEncounterOnNextTransitionForTests(ZM_SPECIES_FERNFAWN, 5u);

			// Data-driven direction pick from the SAME density map the system reads.
			ZM_TerrainGrass* pxGrass = xTerrain.TryGetComponent<ZM_TerrainGrass>();
			if (pxGrass == nullptr || !pxGrass->HasCPUMap())
			{
				FailRT("terrain density map is not available for direction sampling");
				return false;
			}
			const WalkChoice xChoice = ChooseWalkDirection(
				xPlayer.m_xPosition.x, xPlayer.m_xPosition.z, pxGrass->GetDensityMap());
			if (!xChoice.m_bFound)
			{
				FailRT("no cardinal direction from the spawn reaches a grass tile");
				return false;
			}
			g_eRTWalkKey = xChoice.m_eKey;

			// Entry captures. The entry position is a LOGGED DIAGNOSTIC ONLY -- the
			// drift baseline is the parked position latched at the first IN_BATTLE
			// frame, since the player must move to trigger the encounter at all.
			g_xRTEntryPlayerPos      = xPlayer.m_xPosition;
			g_xRTEntryOverworldScene = g_xEngine.Scenes().GetActiveScene();
			g_uRTEntryGrassBlades    = g_xEngine.Grass().GetGeneratedInstanceCount();

			g_eRTPhase = RTPhase::Baseline;
			g_iRTPhaseFrames = 0;
			return true;
		}

		case RTPhase::Baseline:
			// Let the system's OnUpdate establish its baseline tile (the first update
			// after OnAwake only records the tile; it never transitions).
			if (g_iRTPhaseFrames < iRT_BASELINE_FRAMES)
			{
				return true;
			}
			Zenith_InputSimulator::SetKeyHeld(g_eRTWalkKey, true);
			g_eRTPhase = RTPhase::Walk;
			g_iRTPhaseFrames = 0;
			return true;

		case RTPhase::Walk:
		{
			// Re-resolve EVERY frame: the pool relocates components on swap-and-pop.
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailRT("the ZM_BattleTransition singleton stopped resolving mid-walk");
				return false;
			}
			if (pxTransition->GetTransitionState() != ZM_BATTLE_TRANSITION_IDLE)
			{
				// The encounter latched and the machine started; release the key now.
				Zenith_InputSimulator::SetKeyHeld(g_eRTWalkKey, false);
				g_eRTSeenSpecies     = pxTransition->GetBattleSpecies();
				g_uRTSeenLevel       = pxTransition->GetBattleLevel();
				g_eRTSeenSourceScene = pxTransition->GetSourceScene();
				g_eRTPhase = RTPhase::AwaitInBattle;
				g_iRTPhaseFrames = 0;
				return true;
			}
			if (g_iRTPhaseFrames > iRT_WALK_DEADLINE)
			{
				FailRT("walk deadline: never left IDLE");
				return false;
			}
			return true;
		}

		case RTPhase::AwaitInBattle:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailRT("the ZM_BattleTransition singleton stopped resolving before IN_BATTLE");
				return false;
			}

			// Latch the opaque-fade observation EVERY frame: the additive load must
			// only issue behind a fully-opaque screen, and by IN_BATTLE the fade has
			// already returned to transparent, so the endpoint alone proves nothing.
			g_bRTSeenFadeOpaque = g_bRTSeenFadeOpaque
				|| (pxTransition->GetFadeAlpha() >= 1.0f);

			if (!g_bRTInBattleCaptured
				&& pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IN_BATTLE)
			{
				g_xRTParkedPos        = pxTransition->GetParkedPlayerPosition();
				g_bRTBattleSceneValid = pxTransition->GetBattleScene().IsValid();
				g_iRTActiveBuildIndexInBattle = g_xEngine.Scenes().GetSceneInfo(
					g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
				g_bRTOverworldPausedInBattle =
					g_xEngine.Scenes().IsScenePaused(g_xRTEntryOverworldScene);
				g_uRTGrassInBattle = g_xEngine.Grass().GetGeneratedInstanceCount();

				// The camera switch: the active main camera across scenes is now the
				// battle scene's own authored main camera.
				Zenith_SceneData* pxBattleData =
					g_xEngine.Scenes().GetSceneData(pxTransition->GetBattleScene());
				g_bRTBattleCameraActive = (pxBattleData != nullptr
					&& g_xEngine.Scenes().FindMainCameraEntityAcrossScenes()
						== pxBattleData->GetMainCameraEntity());

				// The arena: built + biome + every child owned by the battle scene.
				// It may not resolve on the very first IN_BATTLE frame; leave the
				// diagnostics at their failing defaults rather than aborting, so the
				// test stays diagnostic instead of hanging.
				RTArenaView xArena;
				if (FindUniqueArenaRT(xArena))
				{
					g_bRTArenaFullyBuilt = xArena.m_pxArena->IsFullyBuilt();
					g_eRTArenaBiome      = xArena.m_pxArena->GetActiveBiome();
					if (pxBattleData != nullptr)
					{
						for (u_int i = 0; i < ZM_BattleArena::uCHILD_COUNT; ++i)
						{
							const Zenith_EntityID xChildID = xArena.m_pxArena->GetChildEntityID(i);
							if (g_xEngine.Scenes().GetSceneDataForEntity(xChildID) == pxBattleData)
							{
								++g_uRTArenaChildrenInBattleScene;
							}
						}
					}
				}

				g_uRTIssuedLoads = pxTransition->GetIssuedLoadRequestCount();

				// The SOLE exit from IN_BATTLE. Without this the machine sits in
				// IN_BATTLE until the frame budget expires and the test fails.
				g_bRTBattleEndAccepted = ZM_BattleTransition::RequestBattleEnd();

				g_bRTInBattleCaptured = true;
				g_eRTPhase = RTPhase::AwaitResume;
				g_iRTPhaseFrames = 0;
				return true;
			}

			if (g_iRTPhaseFrames > iRT_INBATTLE_DEADLINE)
			{
				FailRT("never reached IN_BATTLE before deadline");
				return false;
			}
			return true;
		}

		case RTPhase::AwaitResume:
		{
			ZM_BattleTransition* pxTransition = ResolveSingletonBattleTransition();
			if (pxTransition == nullptr)
			{
				FailRT("the ZM_BattleTransition singleton stopped resolving during resume");
				return false;
			}

			if (!g_bRTResumeReached)
			{
				if (pxTransition->GetTransitionState() == ZM_BATTLE_TRANSITION_IDLE
					&& pxTransition->GetCompletedBattleCount() == 1u)
				{
					g_bRTResumeReached = true;
					g_iRTResumeSettle = 0;
					return true;
				}
				if (g_iRTPhaseFrames > iRT_RESUME_DEADLINE)
				{
					FailRT("resume deadline: never returned to IDLE with completed==1");
					return false;
				}
				return true;
			}

			// Let the resume settle before sampling the exact-restore invariants.
			++g_iRTResumeSettle;
			if (g_iRTResumeSettle < iRT_RESUME_SETTLE_FRAMES)
			{
				return true;
			}

			g_iRTActiveBuildIndexAfter = g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex;
			g_bRTOverworldPausedAfter =
				g_xEngine.Scenes().IsScenePaused(g_xRTEntryOverworldScene);
			g_bRTBattleSceneUnloaded = !g_xEngine.Scenes().FindLoadedSceneByPath(
				std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT).IsValid();
			g_uRTGrassAfter = g_xEngine.Grass().GetGeneratedInstanceCount();

			PlayerView xPlayer2;
			if (FindActivePlayer(xPlayer2))
			{
				g_xRTResumePlayerPos       = xPlayer2.m_xPosition;
				g_bRTPlayerMovementEnabled = xPlayer2.m_pxController->IsMovementEnabled();
				g_bRTPlayerResolved        = true;
			}

			g_uRTAbortedAfter    = pxTransition->GetAbortedTransitionCount();
			g_uRTArenaCountAfter = g_xEngine.Scenes().QueryAllScenes<ZM_BattleArena>().Count();

			g_eRTPhase = RTPhase::Done;
			return false;
		}

		case RTPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_ZMBattleRoundTrip()
	{
		bool bPassed = true;

		if (g_bRTActive)
		{
			const float fDrift = glm::length(g_xRTResumePlayerPos - g_xRTParkedPos);

			// One line dumping EVERY captured value so a failure is fully localisable
			// from the log alone. drift is a MEASURED number (not a cliff) so a
			// regression shows as a trend; entryPos is a diagnostic only.
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_BattleRoundTrip] captured: failed=%s (%s) fadeOpaque=%s species=%d "
				"(want FERNFAWN=%d) level=%u (want 5) source=%d (want DAWNMERE=%d) "
				"issuedLoads=%u (want 1) battleSceneValid=%s buildInBattle=%d (want 1) "
				"battleCamActive=%s overworldPausedInBattle=%s grassInBattle=%u (want 0) "
				"entryGrass=%u arenaFullyBuilt=%s arenaChildren=%u (want %u) biome=%d "
				"(want MEADOW=%d) battleEndAccepted=%s abortedAfter=%u (want 0) "
				"buildAfter=%d (want 2) overworldPausedAfter=%s battleUnloaded=%s "
				"arenaCountAfter=%u (want 0) playerResolved=%s movementEnabled=%s "
				"grassAfter=%u (want %u) drift=%f (want <0.05) entryPos=(%f,%f,%f) "
				"parkedPos=(%f,%f,%f) resumePos=(%f,%f,%f)",
				g_bRTFailed ? "true" : "false", g_szRTFailure,
				g_bRTSeenFadeOpaque ? "true" : "false",
				(int)g_eRTSeenSpecies, (int)ZM_SPECIES_FERNFAWN,
				g_uRTSeenLevel,
				(int)g_eRTSeenSourceScene, (int)ZM_SCENE_DAWNMERE,
				g_uRTIssuedLoads,
				g_bRTBattleSceneValid ? "true" : "false",
				g_iRTActiveBuildIndexInBattle,
				g_bRTBattleCameraActive ? "true" : "false",
				g_bRTOverworldPausedInBattle ? "true" : "false",
				g_uRTGrassInBattle,
				g_uRTEntryGrassBlades,
				g_bRTArenaFullyBuilt ? "true" : "false",
				g_uRTArenaChildrenInBattleScene, ZM_BattleArena::uCHILD_COUNT,
				(int)g_eRTArenaBiome, (int)ZM_BATTLE_BIOME_MEADOW,
				g_bRTBattleEndAccepted ? "true" : "false",
				g_uRTAbortedAfter,
				g_iRTActiveBuildIndexAfter,
				g_bRTOverworldPausedAfter ? "true" : "false",
				g_bRTBattleSceneUnloaded ? "true" : "false",
				g_uRTArenaCountAfter,
				g_bRTPlayerResolved ? "true" : "false",
				g_bRTPlayerMovementEnabled ? "true" : "false",
				g_uRTGrassAfter, g_uRTEntryGrassBlades,
				(double)fDrift,
				(double)g_xRTEntryPlayerPos.x, (double)g_xRTEntryPlayerPos.y, (double)g_xRTEntryPlayerPos.z,
				(double)g_xRTParkedPos.x, (double)g_xRTParkedPos.y, (double)g_xRTParkedPos.z,
				(double)g_xRTResumePlayerPos.x, (double)g_xRTResumePlayerPos.y, (double)g_xRTResumePlayerPos.z);

			if (g_bRTFailed)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_BattleRoundTrip] %s", g_szRTFailure);
				bPassed = false;
			}

			// --- the additive load must only issue behind an opaque screen ---
			if (!g_bRTSeenFadeOpaque)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the fade never reached opaque -- the additive Battle "
					"load must only issue behind an opaque screen");
				bPassed = false;
			}

			// --- the latched encounter payload ---
			if (g_eRTSeenSpecies != ZM_SPECIES_FERNFAWN)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] latched species was %d, expected FERNFAWN %d",
					(int)g_eRTSeenSpecies, (int)ZM_SPECIES_FERNFAWN);
				bPassed = false;
			}
			if (g_uRTSeenLevel != 5u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] latched level was %u, expected 5", g_uRTSeenLevel);
				bPassed = false;
			}
			if (g_eRTSeenSourceScene != ZM_SCENE_DAWNMERE)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] latched source scene was %d, expected DAWNMERE %d",
					(int)g_eRTSeenSourceScene, (int)ZM_SCENE_DAWNMERE);
				bPassed = false;
			}

			// --- the additive load ---
			if (g_uRTIssuedLoads != 1u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] issued %u additive Battle loads, expected exactly 1",
					g_uRTIssuedLoads);
				bPassed = false;
			}
			if (!g_bRTBattleSceneValid)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the Battle scene handle was invalid at IN_BATTLE");
				bPassed = false;
			}

			// --- the camera switch ---
			if (g_iRTActiveBuildIndexInBattle != 1)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] active scene build index at IN_BATTLE was %d, expected "
					"1 (the Battle scene)", g_iRTActiveBuildIndexInBattle);
				bPassed = false;
			}
			if (!g_bRTBattleCameraActive)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the active main camera at IN_BATTLE was not the battle "
					"scene's own camera -- the camera switch did not happen");
				bPassed = false;
			}

			// --- the overworld is paused in IN_BATTLE (observed AND per the predicate) ---
			if (!g_bRTOverworldPausedInBattle)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the overworld scene was not paused at IN_BATTLE");
				bPassed = false;
			}
			if (!ZM_BattleTransition::IsOverworldPausedInState(ZM_BATTLE_TRANSITION_IN_BATTLE))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the pure predicate IsOverworldPausedInState(IN_BATTLE) "
					"disagrees -- it must report the overworld paused in IN_BATTLE");
				bPassed = false;
			}

			// --- grass cleared entering battle ---
			if (g_uRTGrassInBattle != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] %u grass blades remained at IN_BATTLE, expected the "
					"overworld grass to be cleared (0)", g_uRTGrassInBattle);
				bPassed = false;
			}
			if (g_uRTEntryGrassBlades == 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the overworld had 0 grass blades before the encounter -- "
					"the grass-clear/restore invariants are vacuous");
				bPassed = false;
			}

			// --- the arena ---
			if (!g_bRTArenaFullyBuilt)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the arena was not IsFullyBuilt() at IN_BATTLE");
				bPassed = false;
			}
			if (g_uRTArenaChildrenInBattleScene != ZM_BattleArena::uCHILD_COUNT)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] only %u of %u arena children were owned by the Battle "
					"scene at IN_BATTLE", g_uRTArenaChildrenInBattleScene,
					ZM_BattleArena::uCHILD_COUNT);
				bPassed = false;
			}
			if (g_eRTArenaBiome != ZM_BATTLE_BIOME_MEADOW)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] arena biome was %d, expected MEADOW %d (the biome for a "
					"Dawnmere-launched battle)", (int)g_eRTArenaBiome, (int)ZM_BATTLE_BIOME_MEADOW);
				bPassed = false;
			}

			// --- the exit was accepted ---
			if (!g_bRTBattleEndAccepted)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] RequestBattleEnd() was rejected at IN_BATTLE (the sole "
					"exit must be accepted while IN_BATTLE)");
				bPassed = false;
			}
			if (g_uRTAbortedAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the round trip recorded %u aborts, expected 0 (a clean "
					"round trip records zero aborts)", g_uRTAbortedAfter);
				bPassed = false;
			}

			// --- EXACT resume ---
			if (g_iRTActiveBuildIndexAfter != 2)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] active scene build index after resume was %d, expected "
					"2 (Dawnmere)", g_iRTActiveBuildIndexAfter);
				bPassed = false;
			}
			if (g_bRTOverworldPausedAfter)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the overworld scene was still paused after resume");
				bPassed = false;
			}
			if (!g_bRTBattleSceneUnloaded)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the Battle scene was still loaded after resume");
				bPassed = false;
			}
			if (g_uRTArenaCountAfter != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] %u ZM_BattleArena instances remained after resume, "
					"expected 0", g_uRTArenaCountAfter);
				bPassed = false;
			}
			if (!g_bRTPlayerResolved)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the overworld player did not resolve after resume");
				bPassed = false;
			}
			if (!g_bRTPlayerMovementEnabled)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] player movement was not re-enabled after resume");
				bPassed = false;
			}
			if (fDrift >= 0.05f)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] the resumed player drifted %f m from its parked position, "
					"expected < 0.05 (the parked body must not drift while the overworld is paused)",
					(double)fDrift);
				bPassed = false;
			}
			if (g_uRTGrassAfter != g_uRTEntryGrassBlades)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_BattleRoundTrip] resumed grass blade count was %u, expected %u (resume must "
					"restore the same deterministic blade count)",
					g_uRTGrassAfter, g_uRTEntryGrassBlades);
				bPassed = false;
			}
		}

		// Always tear down, in order (all guarded), even on a terminal failure:
		// release the key, drop the fixed timestep, clear the transition's ownerless
		// statics, force-unload any lingering Battle scene, restore FrontEnd, then
		// wipe input.
		Zenith_InputSimulator::SetKeyHeld(g_eRTWalkKey, false);
		Zenith_InputSimulator::ClearFixedDt();
		ZM_BattleTransition::ResetRuntimeStateForTests();
		Zenith_Scene xBattle = g_xEngine.Scenes().FindLoadedSceneByPath(
			std::string(GAME_ASSETS_DIR) + "Scenes/Battle" ZENITH_SCENE_EXT);
		if (xBattle.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(xBattle);
		}
		if (g_bRTActive)
		{
			g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
		}
		Zenith_InputSimulator::ResetAllInputState();
		g_bRTActive = false;

		return bPassed || !g_bRTPrereqsPresent;
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

static const Zenith_AutomatedTest g_xZMBattleRoundTripTest = {
	"ZM_BattleRoundTrip_Test",
	&Setup_ZMBattleRoundTrip,
	&Step_ZMBattleRoundTrip,
	&Verify_ZMBattleRoundTrip,
	/* maxFrames */ 2200,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBattleRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
