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
u_int64 Zenith_BenchECS_RunOnce(u_int uNumEntities, u_int uIters)
{
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

		const std::chrono::steady_clock::time_point xStart = std::chrono::steady_clock::now();
		const u_int64 ulProcessed = Zenith_BenchECS_RunOnce(uNumEntities, uBENCH_ITERS);
		const std::chrono::steady_clock::time_point xEnd = std::chrono::steady_clock::now();

		const double fElapsedMs =
			std::chrono::duration<double, std::milli>(xEnd - xStart).count();

		std::printf("BENCH ecs.query_foreach N=%u iters=%u ms=%.3f processed=%llu\n",
			uNumEntities, uBENCH_ITERS, fElapsedMs,
			static_cast<unsigned long long>(ulProcessed));
		std::fflush(stdout);
	}

	std::printf("BENCH ecs.end\n");
	std::fflush(stdout);
}
