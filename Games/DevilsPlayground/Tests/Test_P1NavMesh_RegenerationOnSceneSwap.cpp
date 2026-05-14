#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "AI/Navigation/Zenith_NavMesh.h"

#include "Source/PublicInterfaces.h"

#include <cstdio>

// ============================================================================
// Test_P1NavMesh_RegenerationOnSceneSwap (MVP-1.2.4)
//
// Verifies DP_AI::GetOrBuildLevelNavMesh's cache invalidation on scene swap.
// The cache is keyed by Zenith_SceneData::GetBuildIndex(); when the active
// scene changes to one with a different build-index, the cached navmesh
// must be discarded and a fresh one generated against the new scene's
// collider geometry.
//
// Procedure:
//   1. LoadSceneByIndex(4) -- Gym_Doors (small scene with door entities
//      whose colliders carry the navmesh floor; produces a small real
//      navmesh, ~45 polygons in practice).
//   2. Snapshot navmesh pointer P1 + polygon count N1.
//   3. LoadSceneByIndex(1) -- GameLevel (140 static-deco placements + the
//      authored ground plane -> Zenith_NavMeshGenerator emits the real
//      navmesh, polygon count in the hundreds of thousands).
//   4. Snapshot navmesh pointer P2 + polygon count N2.
//   5. Assert N1 < kMIN_GAMELEVEL_POLYS <= N2 (so the test fails noisily
//      if a future scene-author change causes Gym_Doors to balloon to
//      GameLevel-scale geometry, or if GameLevel shrinks below 1000
//      polygons -- both indicate a regression worth investigating).
//   6. Assert N1 != N2 (different polygon counts prove regeneration).
//
// Note on pointer equality: P1 and P2 are also recorded for diagnostic
// logging, but the test does NOT assert they differ. CI's heap allocator
// has been observed to reuse the freed Gym_Doors navmesh address for the
// GameLevel allocation (run 25850815534, 2026-05-14), which made an
// earlier "pointers differ" gate fail spuriously on the same workload
// that passed locally every time. The polygon-count delta is unambiguous
// proof of regeneration regardless of where the new navmesh lands.
//
// Why this matters: every gameplay BT branch that calls
// DP_AI::GetOrBuildLevelNavMesh (priest pursue / patrol / investigate)
// receives the cached pointer. If the cache stayed stale across scene
// swaps, the priest would pathfind on the previous scene's navmesh,
// landing him at random positions (or worse, dereferencing freed
// memory when the previous scene's polygon array gets destroyed).
// ============================================================================

namespace
{
	enum Phase : int { kRS_Start, kRS_WaitGymLoaded, kRS_RecordGym,
	                   kRS_LoadGameLevel, kRS_WaitGameLevelLoaded,
	                   kRS_RecordGameLevel, kRS_Verify, kRS_Done };

	int                     g_iPhase = kRS_Start;
	const Zenith_NavMesh*   g_pxNavMeshGym = nullptr;
	const Zenith_NavMesh*   g_pxNavMeshGameLevel = nullptr;
	uint32_t                g_uPolyCountGym = 0;
	uint32_t                g_uPolyCountGameLevel = 0;
	int                     g_iWaitFrames = 0;
	bool                    g_bPassed = false;

	// Frames to wait after LoadSceneByIndex for entities + OnAwake/OnStart
	// to drain. The 0.4 navmesh generator pass on GameLevel takes ~850 ms,
	// which is ~51 frames at 60 Hz; we add slack so the cache is settled
	// before snapshotting.
	constexpr int kWAIT_FRAMES_GYM       = 8;
	constexpr int kWAIT_FRAMES_GAMELEVEL = 8;

	// Per-scene polygon-count thresholds. Gym_Doors in practice emits ~45
	// polygons (varies a bit with collider geometry tweaks); GameLevel in
	// practice emits ~248k. The gap is wide enough that even significant
	// future scene-author changes won't flip the comparison.
	constexpr uint32_t kMAX_GYM_POLYS       = 500;
	constexpr uint32_t kMIN_GAMELEVEL_POLYS = 1000;
}

static void Setup_P1NavMeshRegenerationOnSceneSwap()
{
	g_iPhase = kRS_Start;
	g_pxNavMeshGym = nullptr;
	g_pxNavMeshGameLevel = nullptr;
	g_uPolyCountGym = 0;
	g_uPolyCountGameLevel = 0;
	g_iWaitFrames = 0;
	g_bPassed = false;
}

static bool Step_P1NavMeshRegenerationOnSceneSwap(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kRS_Start:
		// Gym_Doors (build index 4) has no authored floor -- DP_AI will
		// fall back to the synthetic flat-quad navmesh (1 polygon).
		Zenith_SceneManager::LoadSceneByIndex(4, SCENE_LOAD_SINGLE);
		g_iWaitFrames = 0;
		g_iPhase = kRS_WaitGymLoaded;
		return true;

	case kRS_WaitGymLoaded:
		if (++g_iWaitFrames < kWAIT_FRAMES_GYM) return true;
		g_iPhase = kRS_RecordGym;
		return true;

	case kRS_RecordGym:
	{
		g_pxNavMeshGym = DP_AI::GetOrBuildLevelNavMesh();
		g_uPolyCountGym = g_pxNavMeshGym ? g_pxNavMeshGym->GetPolygonCount() : 0u;
		std::printf("[P1NavMeshRegen] After Gym_Doors load: navmesh=%p polys=%u\n",
			(const void*)g_pxNavMeshGym, g_uPolyCountGym);
		std::fflush(stdout);
		g_iPhase = kRS_LoadGameLevel;
		return true;
	}

	case kRS_LoadGameLevel:
		// GameLevel (build index 1) has the authored ground plane + 140
		// static-deco placements -- the real generator runs and produces
		// >1000 polygons.
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iWaitFrames = 0;
		g_iPhase = kRS_WaitGameLevelLoaded;
		return true;

	case kRS_WaitGameLevelLoaded:
		if (++g_iWaitFrames < kWAIT_FRAMES_GAMELEVEL) return true;
		g_iPhase = kRS_RecordGameLevel;
		return true;

	case kRS_RecordGameLevel:
	{
		g_pxNavMeshGameLevel = DP_AI::GetOrBuildLevelNavMesh();
		g_uPolyCountGameLevel = g_pxNavMeshGameLevel
			? g_pxNavMeshGameLevel->GetPolygonCount() : 0u;
		std::printf("[P1NavMeshRegen] After GameLevel load: navmesh=%p polys=%u\n",
			(const void*)g_pxNavMeshGameLevel, g_uPolyCountGameLevel);
		std::fflush(stdout);
		g_iPhase = kRS_Verify;
		return true;
	}

	case kRS_Verify:
	{
		const bool bGymSmall         = (g_uPolyCountGym > 0)
		                            && (g_uPolyCountGym <= kMAX_GYM_POLYS);
		const bool bGameLevelLarge   = (g_uPolyCountGameLevel >= kMIN_GAMELEVEL_POLYS);
		const bool bPolyCountsDiffer = (g_uPolyCountGym != g_uPolyCountGameLevel);
		// Pointer equality is recorded for diagnostic purposes ONLY -- we
		// do NOT assert that the pointers differ. CI's heap allocator was
		// observed to reuse the freed Gym_Doors navmesh address for the
		// GameLevel allocation (run 25850815534), which would have made a
		// "pointers differ" gate fail spuriously on the same workload that
		// passes every time locally. The 45 -> 248413 polygon count delta
		// is unambiguous proof of regeneration regardless of where the new
		// navmesh lands in heap.
		const bool bPointersDiffer   = (g_pxNavMeshGym != g_pxNavMeshGameLevel);
		g_bPassed = bGymSmall
		         && bGameLevelLarge
		         && bPolyCountsDiffer
		         && g_pxNavMeshGym != nullptr
		         && g_pxNavMeshGameLevel != nullptr;
		std::printf("[P1NavMeshRegen] verify: gymSmall=%d gamelevelLarge=%d polyDiff=%d ptrDiff=%d (diagnostic only) passed=%d\n",
			(int)bGymSmall, (int)bGameLevelLarge,
			(int)bPolyCountsDiffer, (int)bPointersDiffer,
			(int)g_bPassed);
		std::fflush(stdout);
		g_iPhase = kRS_Done;
		return false;
	}

	case kRS_Done:
	default:
		return false;
	}
}

static bool Verify_P1NavMeshRegenerationOnSceneSwap()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xP1NavMeshRegenTest = {
	"Test_P1NavMesh_RegenerationOnSceneSwap",
	&Setup_P1NavMeshRegenerationOnSceneSwap,
	&Step_P1NavMeshRegenerationOnSceneSwap,
	&Verify_P1NavMeshRegenerationOnSceneSwap,
	120,
	false // m_bRequiresGraphics: pure scene-manager + navmesh
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1NavMeshRegenTest);

#endif // ZENITH_INPUT_SIMULATOR
