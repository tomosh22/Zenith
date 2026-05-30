#include "Zenith.h"

#include "Core/Zenith_BenchECS.h"

#include "Core/Zenith_Engine.h"
// Zenith_Scene.h pulls in Zenith_SceneData.h AND Zenith_Entity.inl (the
// AddComponent / GetComponent / HasComponent / RemoveComponent template
// bodies), which is exactly what this TU needs to call those templates.
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"

#include <chrono>
#include <cstdio>

// ============================================================================
// Zenith_BenchECS_RunOnce
//
// One self-contained benchmark pass. Uses ONLY the public ECS API:
//   - g_xEngine.Scenes().CreateEmptyScene(...) for an empty in-memory scene
//     (the documented procedural empty-scene path; no disk read, no GPU work).
//   - Zenith_Entity(pxSceneData, name) to create entities (each auto-gets a
//     Zenith_TransformComponent).
//   - A second component (Zenith_LightComponent) added on a deterministic
//     subset to give Query<...> a real multi-component combination to filter.
//   - Zenith_Query<...>(*pxSceneData).ForEach(...) for the hot read loop.
//   - Add/Remove churn on Zenith_LightComponent each iteration.
//
// Deterministic: every choice is keyed off the loop index, so the processed
// count is reproducible across runs and machines.
// ============================================================================
u_int64 Zenith_BenchECS_RunOnce(u_int uNumEntities, u_int uIters, bool bUseSparse)
{
	// WS10 A/B: pin the Query read path (sparse vs legacy) for this whole pass
	// and restore the engine's prior toggle value before returning. The
	// Add/Remove churn below dual-writes the sparse index regardless of path, so
	// flipping the read path mid-bench is safe.
	const bool bPrevSparse = g_xEngine.Scenes().AreSparseQueryReadsEnabled();
	g_xEngine.Scenes().SetSparseQueryReads(bUseSparse);

	// Distinct scene name per entity count so repeated calls don't collide.
	char acSceneName[128];
	std::snprintf(acSceneName, sizeof(acSceneName), "BenchECS_%u", uNumEntities);

	Zenith_Scene xScene = g_xEngine.Scenes().CreateEmptyScene(acSceneName);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Assert(pxSceneData != nullptr, "Zenith_BenchECS_RunOnce: empty scene has no scene data");

	// Create the entities. Every entity has a Transform automatically; add a
	// Light to every 4th entity so the Query<Transform, Light> combination
	// matches a deterministic, non-trivial subset.
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(uNumEntities);
	for (u_int u = 0; u < uNumEntities; ++u)
	{
		char acEntityName[64];
		std::snprintf(acEntityName, sizeof(acEntityName), "BenchEnt_%u", u);
		Zenith_Entity xEntity(pxSceneData, acEntityName);

		// Touch the Transform so the work isn't optimised away and the data is
		// deterministic.
		xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(static_cast<float>(u), 0.0f, 0.0f));

		if ((u & 3u) == 0u)
		{
			xEntity.AddComponent<Zenith_LightComponent>();
		}

		xEntityIDs.PushBack(xEntity.GetEntityID());
	}

	// Total component instances visited across all iterations. Returned to the
	// caller as the "processed count" so callers (and the smoke test) can prove
	// the benchmark actually did work.
	u_int64 ulProcessed = 0;

	for (u_int uIter = 0; uIter < uIters; ++uIter)
	{
		// Hot read loop: iterate every entity that has a Transform (i.e. all of
		// them). Read the position so the compiler cannot elide the ForEach body,
		// and count each visit.
		Zenith_Query<Zenith_TransformComponent>(*pxSceneData).ForEach(
			[&ulProcessed](Zenith_EntityID, Zenith_TransformComponent& xTransform)
			{
				Zenith_Maths::Vector3 xPos;
				xTransform.GetPosition(xPos);
				ulProcessed += 1u + (xPos.x < 0.0f ? 1u : 0u); // position-dependent so the read isn't dead
			});

		// Multi-component read loop: entities with BOTH Transform and Light.
		Zenith_Query<Zenith_TransformComponent, Zenith_LightComponent>(*pxSceneData).ForEach(
			[&ulProcessed](Zenith_EntityID, Zenith_TransformComponent&, Zenith_LightComponent&)
			{
				++ulProcessed;
			});

		// Add/Remove churn: on even iterations strip the Light from every 4th
		// entity, on odd iterations put it back. This exercises the swap-and-pop
		// component-pool path that the future query rework must not regress.
		const bool bRemovePhase = ((uIter & 1u) == 0u);
		for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
		{
			if ((u & 3u) != 0u)
			{
				continue;
			}

			Zenith_Entity xEntity(pxSceneData, xEntityIDs.Get(u));
			if (bRemovePhase)
			{
				if (xEntity.HasComponent<Zenith_LightComponent>())
				{
					xEntity.RemoveComponent<Zenith_LightComponent>();
				}
			}
			else
			{
				if (!xEntity.HasComponent<Zenith_LightComponent>())
				{
					xEntity.AddComponent<Zenith_LightComponent>();
				}
			}
		}
	}

	g_xEngine.Scenes().UnloadScene(xScene);

	// Restore the engine's prior read-path toggle (leave no side effect).
	g_xEngine.Scenes().SetSparseQueryReads(bPrevSparse);

	return ulProcessed;
}

// ============================================================================
// Zenith_BenchECS_Run
//
// The --bench-ecs entry point. Runs a fixed iteration count for each canonical
// entity count and prints one parseable BENCH line per count.
// ============================================================================
void Zenith_BenchECS_Run()
{
	static constexpr u_int uBENCH_ITERS = 100;
	static const u_int auEntityCounts[] = { 1000u, 10000u, 50000u };

	std::printf("BENCH ecs.begin iters=%u\n", uBENCH_ITERS);
	std::fflush(stdout);

	for (u_int uCountIndex = 0; uCountIndex < (sizeof(auEntityCounts) / sizeof(auEntityCounts[0])); ++uCountIndex)
	{
		const u_int uNumEntities = auEntityCounts[uCountIndex];

		// WS10 A/B: run the SAME workload twice per N — once with the legacy read
		// path, once with the sparse fast path — and print one tagged BENCH line
		// each. The Add/Remove churn is identical (it dual-writes the sparse index
		// regardless), so the two passes are over the same logical workload.

		// --- legacy ---
		const std::chrono::steady_clock::time_point xStartLegacy = std::chrono::steady_clock::now();
		const u_int64 ulProcessedLegacy = Zenith_BenchECS_RunOnce(uNumEntities, uBENCH_ITERS, /*bUseSparse=*/false);
		const std::chrono::steady_clock::time_point xEndLegacy = std::chrono::steady_clock::now();
		const double fLegacyMs = std::chrono::duration<double, std::milli>(xEndLegacy - xStartLegacy).count();

		std::printf("BENCH ecs.query_foreach path=legacy N=%u iters=%u ms=%.3f processed=%llu\n",
			uNumEntities, uBENCH_ITERS, fLegacyMs,
			static_cast<unsigned long long>(ulProcessedLegacy));
		std::fflush(stdout);

		// --- sparse ---
		const std::chrono::steady_clock::time_point xStartSparse = std::chrono::steady_clock::now();
		const u_int64 ulProcessedSparse = Zenith_BenchECS_RunOnce(uNumEntities, uBENCH_ITERS, /*bUseSparse=*/true);
		const std::chrono::steady_clock::time_point xEndSparse = std::chrono::steady_clock::now();
		const double fSparseMs = std::chrono::duration<double, std::milli>(xEndSparse - xStartSparse).count();

		std::printf("BENCH ecs.query_foreach path=sparse N=%u iters=%u ms=%.3f processed=%llu\n",
			uNumEntities, uBENCH_ITERS, fSparseMs,
			static_cast<unsigned long long>(ulProcessedSparse));
		std::fflush(stdout);

		// Correctness self-check INSIDE the bench: both read paths must visit the
		// same number of component instances for the identical workload. A
		// mismatch means the sparse index diverged from the legacy map — a hard
		// regression — so assert it loudly.
		Zenith_Assert(ulProcessedLegacy == ulProcessedSparse,
			"BenchECS A/B mismatch at N=%u: legacy processed=%llu, sparse processed=%llu",
			uNumEntities,
			static_cast<unsigned long long>(ulProcessedLegacy),
			static_cast<unsigned long long>(ulProcessedSparse));

		// Optional human-readable ratio line (legacy/sparse). Guard the divide.
		const double fRatio = (fSparseMs > 0.0) ? (fLegacyMs / fSparseMs) : 0.0;
		std::printf("BENCH ecs.query_foreach.ratio N=%u legacy_over_sparse=%.3f\n",
			uNumEntities, fRatio);
		std::fflush(stdout);
	}

	std::printf("BENCH ecs.end\n");
	std::fflush(stdout);
}
