#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "../Components/DPProcLevelBootstrap_Component.h"
#include "../Components/Priest_Component.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"

#include <cstdio>
#include <cstdlib>

// ============================================================================
// Test_ProcLevel_PriestReachability
//
// Standing CI gate for the "priest gets stuck in a building" class of
// procgen regressions. For each seed in the canonical 10-seed set
// (ratified 2026-05-22; seed 0 excluded as unsolvable) it:
//
//   1. Resets the level navmesh (seed changes invalidate the build-index
//      cache — same call the between-tests hook and R-restart rely on),
//      sets DP_PROCGEN_SEED, and reloads ProcLevel through the production
//      path (bootstrap Generate + spawn + door OnStart stitches +
//      DP_AI::GetOrBuildLevelNavMesh).
//   2. Asserts DP_AI::GetUnstitchedDoorCount() == 0 — no door may leave
//      its two rooms in different navmesh components (the verified root
//      cause: a silently-failed stitch seals the room for pathfinding
//      regardless of the door's open/closed state).
//   3. Asserts every room centre is path-connected to the priest's spawn
//      position via DP_AI::ArePositionsConnected (BFS over the polygon-
//      neighbour graph; door stitches count, dynamic BLOCKED state is
//      ignored because the priest opens unlocked doors on approach).
//
// The env var is cleared at the end (set to "" — the bootstrap treats
// empty as unset) so later tests in the same process get the default seed.
// ============================================================================

namespace
{
	const uint64_t g_aulSeeds[] = {
		1ull, 5ull, 7ull, 42ull, 100ull, 12345ull, 55555ull, 99999ull,
		250000ull, 4276994270ull
	};
	constexpr int kSeedCount = static_cast<int>(sizeof(g_aulSeeds) / sizeof(g_aulSeeds[0]));
	// Post-load settle only. The variable wait for the deferred SINGLE-load to
	// commit is now handled by the seed-matched readiness gate in Step: a fixed
	// countdown expired mid-load under batch timing and validated a half-built
	// scene (partial navmesh -> phantom sealed doors). This small fixed tail runs
	// AFTER the fresh bootstrap's OnAwake spawn wave, to let each DPDoor's OnStart
	// portal-stitch run before the navmesh is generated in ValidateCurrentSeed.
	constexpr int kPostLoadSettleFrames = 6;

	int  g_iSeedIdx = -1;
	int  g_iSettle = 0;
	bool g_bLoadCommitted = false;
	bool g_bReachFailed = false;
	bool g_bReachDone = false;
	char g_szReachWhy[256] = {};

	void BeginSeed(int iIdx)
	{
		char szSeed[32];
		std::snprintf(szSeed, sizeof(szSeed), "%llu",
			static_cast<unsigned long long>(g_aulSeeds[iIdx]));
		_putenv_s("DP_PROCGEN_SEED", szSeed);
		// Seed changes invalidate the build-index navmesh cache; production
		// covers this via fresh processes (seed matrix) / the between-tests
		// hook — an in-test seed swap needs the same explicit reset.
		DP_AI::ResetLevelNavMesh();
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		// The load is deferred; Step holds (seed-matched) for the fresh bootstrap
		// to finish its OnAwake spawn wave before starting the post-load settle.
		g_bLoadCommitted = false;
		g_iSettle = 0;
	}

	bool ValidateCurrentSeed(int iIdx)
	{
		const unsigned long long ulSeed =
			static_cast<unsigned long long>(g_aulSeeds[iIdx]);

		// Gate 0: the PRODUCTION navmesh must be real. The synthetic
		// flat-quad fallback (one polygon) would make BOTH gates below pass
		// vacuously — door stitching early-outs under 16 polygons (so the
		// sealed-door count stays 0 by construction) and every probe lands
		// in the same polygon (so connectivity is trivially true).
		const Zenith_NavMesh* pxNav = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNav == nullptr || pxNav->GetPolygonCount() < 16u)
		{
			std::snprintf(g_szReachWhy, sizeof(g_szReachWhy),
				"seed %llu: navmesh generation fell back to the synthetic quad (%u polys)",
				ulSeed, pxNav != nullptr ? pxNav->GetPolygonCount() : 0u);
			return false;
		}

		// Gate 1: no sealed doors.
		const uint32_t uUnstitched = DP_AI::GetUnstitchedDoorCount();
		if (uUnstitched != 0)
		{
			std::snprintf(g_szReachWhy, sizeof(g_szReachWhy),
				"seed %llu: %u door(s) left their rooms sealed for pathfinding",
				ulSeed, uUnstitched);
			return false;
		}

		// Layout + spawned priest via the production singletons/queries.
		DPProcLevelBootstrap_Component* pxBootstrap = DPProcLevelBootstrap_Component::Instance();
		if (pxBootstrap == nullptr || pxBootstrap->GetLayout().axRooms.GetSize() == 0)
		{
			std::snprintf(g_szReachWhy, sizeof(g_szReachWhy),
				"seed %llu: bootstrap missing or empty layout", ulSeed);
			return false;
		}
		const DPProcLevel::LevelLayout& xLayout = pxBootstrap->GetLayout();

		Zenith_Maths::Vector3 xPriestPos(0.0f, 0.0f, 0.0f);
		bool bFoundPriest = false;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&](Zenith_EntityID xId, Priest_Component&)
			{
				if (bFoundPriest) return;
				Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
				if (!xEnt.IsValid()) return;
				if (Zenith_TransformComponent* pxT = xEnt.TryGetComponent<Zenith_TransformComponent>())
				{
					pxT->GetPosition(xPriestPos);
					bFoundPriest = true;
				}
			});
		if (!bFoundPriest)
		{
			std::snprintf(g_szReachWhy, sizeof(g_szReachWhy),
				"seed %llu: no spawned priest found", ulSeed);
			return false;
		}

		// Gate 2: every room centre reachable from the priest spawn. A room
		// centre can sit inside a spawned prop's navmesh hole, so probe a
		// small offset ring before declaring the room unreachable.
		const uint32_t uRooms = xLayout.axRooms.GetSize();
		for (uint32_t uRoom = 0; uRoom < uRooms; ++uRoom)
		{
			const auto& xRoom = xLayout.axRooms.Get(uRoom);
			const Zenith_Maths::Vector3 axProbes[5] = {
				{ xRoom.fCentreX,        1.0f, xRoom.fCentreZ        },
				{ xRoom.fCentreX + 1.5f, 1.0f, xRoom.fCentreZ        },
				{ xRoom.fCentreX - 1.5f, 1.0f, xRoom.fCentreZ        },
				{ xRoom.fCentreX,        1.0f, xRoom.fCentreZ + 1.5f },
				{ xRoom.fCentreX,        1.0f, xRoom.fCentreZ - 1.5f },
			};
			bool bConnected = false;
			for (int iP = 0; iP < 5 && !bConnected; ++iP)
			{
				bConnected = DP_AI::ArePositionsConnected(xPriestPos, axProbes[iP]);
			}
			if (!bConnected)
			{
				std::snprintf(g_szReachWhy, sizeof(g_szReachWhy),
					"seed %llu: room %u centre (%.1f, %.1f) unreachable from priest spawn (%.1f, %.1f)",
					ulSeed, uRoom, xRoom.fCentreX, xRoom.fCentreZ,
					xPriestPos.x, xPriestPos.z);
				return false;
			}
		}

		std::printf("[PriestReachability] seed %llu ok (%u rooms, 0 sealed doors)\n",
			ulSeed, uRooms);
		std::fflush(stdout);
		return true;
	}
}

static void Setup_PriestReachability()
{
	g_iSeedIdx = -1;
	g_iSettle = 0;
	g_bLoadCommitted = false;
	g_bReachFailed = false;
	g_bReachDone = false;
	g_szReachWhy[0] = '\0';
}

static bool Step_PriestReachability(int /*iFrame*/)
{
	if (g_bReachFailed || g_bReachDone) return false;

	if (g_iSeedIdx < 0)
	{
		g_iSeedIdx = 0;
		BeginSeed(g_iSeedIdx);
		return true;
	}

	// Load-commit gate. Hold until the deferred SINGLE-load has swapped in THIS
	// seed's freshly-generated scene: the bootstrap singleton must report the
	// matching seed (a lingering old-seed bootstrap reports the previous value)
	// AND have finished its OnAwake spawn wave (GetSpawnedPriest is set by the
	// final spawn). Validating before this reads a half-built navmesh and invents
	// sealed doors -- the batch-timing failure this test used to hit. Bounded by
	// the test's maxFrames watchdog if the load never commits.
	if (!g_bLoadCommitted)
	{
		DPProcLevelBootstrap_Component* pxBootstrap =
			DPProcLevelBootstrap_Component::Instance();
		const bool bFreshSceneReady = pxBootstrap != nullptr
			&& pxBootstrap->GetSeed() == g_aulSeeds[g_iSeedIdx]
			&& pxBootstrap->GetSpawnedPriest();
		if (!bFreshSceneReady)
		{
			return true;
		}
		g_bLoadCommitted = true;
		g_iSettle = kPostLoadSettleFrames;
		return true;
	}

	if (g_iSettle > 0)
	{
		--g_iSettle;
		return true;
	}

	if (!ValidateCurrentSeed(g_iSeedIdx))
	{
		g_bReachFailed = true;
		return false;
	}

	++g_iSeedIdx;
	if (g_iSeedIdx >= kSeedCount)
	{
		g_bReachDone = true;
		return false;
	}
	BeginSeed(g_iSeedIdx);
	return true;
}

static bool Verify_PriestReachability()
{
	// Always clear the seed override — later tests in the same process must
	// get the default seed (empty string == unset for the bootstrap).
	_putenv_s("DP_PROCGEN_SEED", "");

	if (!g_bReachDone || g_bReachFailed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "PriestReachability failed: %s",
			g_szReachWhy[0] != '\0' ? g_szReachWhy : "did not complete all seeds");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPriestReachabilityTest = {
	"Test_ProcLevel_PriestReachability",
	&Setup_PriestReachability,
	&Step_PriestReachability,
	&Verify_PriestReachability,
	// 10 seeds x (deferred load-wait + post-load settle + validate). The load-wait
	// is variable under batch timing, so keep a generous watchdog; a genuinely
	// wedged load still fails out here rather than hanging.
	/*maxFrames*/ 600
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPriestReachabilityTest);

#endif // ZENITH_INPUT_SIMULATOR
