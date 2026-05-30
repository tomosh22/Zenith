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
// WS12 parallel-sim bench + soak: Tween-only collider-free entities driven
// through the OnUpdate chokepoint with the parallel-sim flag OFF then ON.
#include "EntityComponent/Components/Zenith_TweenComponent.h"
// The field-by-field state hash serializes each Transform via its own
// WriteToDataStream into a Zenith_DataStream, so we need the full type here
// (the ECS headers only forward-use it in declarations).
#include "DataStream/Zenith_DataStream.h"

#include <chrono>
#include <cstdint>
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

// ============================================================================
// WS12 parallel-simulation bench + determinism soak
// ============================================================================
namespace
{
	// Fixed-seed xorshift32 PRNG (std::rand forbidden + non-reproducible).
	struct Zenith_SimRng
	{
		uint32_t m_uState;
		explicit Zenith_SimRng(uint32_t uSeed) : m_uState(uSeed ? uSeed : 0x1234567u) {}
		uint32_t Next()
		{
			uint32_t x = m_uState;
			x ^= x << 13; x ^= x >> 17; x ^= x << 5;
			m_uState = x;
			return x;
		}
		float NextFloat(float fLo, float fHi)
		{
			const float f01 = static_cast<float>(Next() & 0xFFFFFFu) / static_cast<float>(0xFFFFFF);
			return fLo + (fHi - fLo) * f01;
		}
	};

	// FNV-1a over the FIELD-BY-FIELD serialization of every entity's Transform
	// (position/rotation/scale/parent via WriteToDataStream — never a struct
	// memcpy, so padding never feeds the hash). Same methodology as the unit
	// smoke test and Test_ProcLevel_DeterminismCheck.
	uint64_t HashSceneTransforms(Zenith_SceneData* pxSceneData,
	                             const Zenith_Vector<Zenith_EntityID>& xIDs)
	{
		uint64_t h = 0xcbf29ce484222325ull;
		auto Bytes = [&h](const void* p, size_t n)
		{
			const uint8_t* b = static_cast<const uint8_t*>(p);
			for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ull; }
		};

		for (u_int u = 0; u < xIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xIDs.Get(u);
			if (!pxSceneData->EntityExists(uID)) { const uint8_t z = 0xAA; Bytes(&z, 1); continue; }
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			if (!xEntity.HasComponent<Zenith_TransformComponent>()) { const uint8_t z = 0xBB; Bytes(&z, 1); continue; }

			// Hash the WRITTEN bytes (GetCursor), NOT the buffer capacity
			// (GetSize) — the uninitialised buffer tail must never feed the hash.
			Zenith_DataStream xStream(256);
			xEntity.GetComponent<Zenith_TransformComponent>().WriteToDataStream(xStream);
			Bytes(xStream.GetData(), static_cast<size_t>(xStream.GetCursor()));
		}
		return h;
	}

	// Build uCount collider-free / script-free entities each with a looping
	// ping-pong POSITION tween (every 3rd also a SCALE tween). Seeded purely off
	// the entity index so every call produces a bit-identical scene. Returns the
	// active-entity ID list in xIDsOut.
	void BuildTweenScene(Zenith_SceneData* pxSceneData, u_int uCount,
	                     Zenith_Vector<Zenith_EntityID>& xIDsOut)
	{
		Zenith_SimRng xRng(0xC0FFEEu);
		xIDsOut.Reserve(uCount);

		for (u_int u = 0; u < uCount; ++u)
		{
			char acName[64];
			std::snprintf(acName, sizeof(acName), "SimTween_%u", u);
			Zenith_Entity xEntity(pxSceneData, acName);

			const Zenith_Maths::Vector3 xStart(
				xRng.NextFloat(-5.0f, 5.0f), xRng.NextFloat(-5.0f, 5.0f), xRng.NextFloat(-5.0f, 5.0f));
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xStart);

			Zenith_TweenComponent& xTween = xEntity.AddComponent<Zenith_TweenComponent>();
			const Zenith_Maths::Vector3 xTo(
				xRng.NextFloat(-5.0f, 5.0f), xRng.NextFloat(-5.0f, 5.0f), xRng.NextFloat(-5.0f, 5.0f));
			xTween.TweenPositionFromTo(xStart, xTo, xRng.NextFloat(0.2f, 1.0f), EASING_QUAD_OUT);
			xTween.SetLoop(true, /*bPingPong=*/true);

			if ((u % 3u) == 0u)
			{
				const Zenith_Maths::Vector3 xScaleTo(
					xRng.NextFloat(0.5f, 2.0f), xRng.NextFloat(0.5f, 2.0f), xRng.NextFloat(0.5f, 2.0f));
				xTween.TweenScaleFromTo(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f), xScaleTo,
					xRng.NextFloat(0.2f, 1.0f), EASING_QUAD_OUT);
				xTween.SetLoop(true, /*bPingPong=*/true);
			}

			xIDsOut.PushBack(xEntity.GetEntityID());
		}
	}

	// Run uFrames fixed-dt OnUpdate passes through the gated chokepoint
	// (Bench_DispatchOnUpdatePass forwards to DispatchOnUpdateForEntities) with
	// the parallel-sim flag pinned to bParallel, returning the elapsed ms.
	double RunSimFramesTimed(Zenith_SceneData* pxSceneData,
	                         const Zenith_Vector<Zenith_EntityID>& xIDs,
	                         u_int uFrames, bool bParallel)
	{
		const bool bPrev = g_xEngine.Scenes().AreParallelSimEnabled();
		g_xEngine.Scenes().SetParallelSim(bParallel);

		const float fFixedDt = 1.0f / 60.0f;
		const std::chrono::steady_clock::time_point xStart = std::chrono::steady_clock::now();
		for (u_int f = 0; f < uFrames; ++f)
		{
			pxSceneData->Bench_DispatchOnUpdatePass(xIDs, fFixedDt);
		}
		const std::chrono::steady_clock::time_point xEnd = std::chrono::steady_clock::now();

		g_xEngine.Scenes().SetParallelSim(bPrev);
		return std::chrono::duration<double, std::milli>(xEnd - xStart).count();
	}
}

void Zenith_BenchSim_Run()
{
	static constexpr u_int uBENCH_FRAMES = 200;
	static const u_int auEntityCounts[] = { 256u, 1024u, 4096u };

	std::printf("BENCH sim.begin frames=%u\n", uBENCH_FRAMES);
	std::fflush(stdout);

	for (u_int uCountIndex = 0; uCountIndex < (sizeof(auEntityCounts) / sizeof(auEntityCounts[0])); ++uCountIndex)
	{
		const u_int uNumEntities = auEntityCounts[uCountIndex];

		char acSceneName[128];

		// --- serial pass ---
		std::snprintf(acSceneName, sizeof(acSceneName), "BenchSim_%u_serial", uNumEntities);
		Zenith_Scene xScene = g_xEngine.Scenes().CreateEmptyScene(acSceneName);
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		Zenith_Assert(pxSceneData != nullptr, "Zenith_BenchSim_Run: empty scene has no scene data");

		Zenith_Vector<Zenith_EntityID> xIDs;
		BuildTweenScene(pxSceneData, uNumEntities, xIDs);

		const double fSerialMs = RunSimFramesTimed(pxSceneData, xIDs, uBENCH_FRAMES, /*bParallel=*/false);
		const uint64_t uSerialHash = HashSceneTransforms(pxSceneData, xIDs);
		std::printf("BENCH sim.update path=serial N=%u iters=%u ms=%.3f hash=0x%016llx\n",
			uNumEntities, uBENCH_FRAMES, fSerialMs, static_cast<unsigned long long>(uSerialHash));
		std::fflush(stdout);
		g_xEngine.Scenes().UnloadScene(xScene);

		// Rebuild an identical scene (fresh name) for the parallel pass so both
		// passes start from the SAME deterministic initial state (the serial pass
		// advanced the tweens). Same seed -> bit-identical construction.
		std::snprintf(acSceneName, sizeof(acSceneName), "BenchSim_%u_parallel", uNumEntities);
		xScene = g_xEngine.Scenes().CreateEmptyScene(acSceneName);
		pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		xIDs.Clear();
		BuildTweenScene(pxSceneData, uNumEntities, xIDs);

		// --- parallel pass ---
		const double fParallelMs = RunSimFramesTimed(pxSceneData, xIDs, uBENCH_FRAMES, /*bParallel=*/true);
		const uint64_t uParallelHash = HashSceneTransforms(pxSceneData, xIDs);
		std::printf("BENCH sim.update path=parallel N=%u iters=%u ms=%.3f hash=0x%016llx\n",
			uNumEntities, uBENCH_FRAMES, fParallelMs, static_cast<unsigned long long>(uParallelHash));
		std::fflush(stdout);

		// IN-BENCH correctness self-check: the field-by-field state hash MUST be
		// identical across the serial and parallel passes (default-off byte-
		// identity is the whole guarantee). A mismatch is a hard regression.
		Zenith_Assert(uSerialHash == uParallelHash,
			"BenchSim hash mismatch at N=%u: serial=0x%016llx parallel=0x%016llx",
			uNumEntities,
			static_cast<unsigned long long>(uSerialHash),
			static_cast<unsigned long long>(uParallelHash));

		// Honest ratio (serial/parallel). >1 = parallel faster; expect ~1 (nil)
		// today because almost everything is conservatively serial.
		const double fRatio = (fParallelMs > 0.0) ? (fSerialMs / fParallelMs) : 0.0;
		std::printf("BENCH sim.update.ratio N=%u serial_over_parallel=%.3f\n", uNumEntities, fRatio);
		std::fflush(stdout);

		g_xEngine.Scenes().UnloadScene(xScene);
	}

	std::printf("BENCH sim.end\n");
	std::fflush(stdout);
}

bool Zenith_CheckSimDeterminism_Run()
{
	static constexpr u_int uNUM_ENTITIES   = 256;
	static constexpr u_int uNUM_FRAMES     = 300;
	static constexpr u_int uPARALLEL_RUNS  = 16;

	std::printf("[check-sim-determinism] entities=%u frames=%u parallel_runs=%u\n",
		uNUM_ENTITIES, uNUM_FRAMES, uPARALLEL_RUNS);
	std::fflush(stdout);

	// Serial reference hash.
	Zenith_Scene xSerialScene = g_xEngine.Scenes().CreateEmptyScene("CheckSimDet_Serial");
	Zenith_SceneData* pxSerial = g_xEngine.Scenes().GetSceneData(xSerialScene);
	Zenith_Vector<Zenith_EntityID> xSerialIDs;
	BuildTweenScene(pxSerial, uNUM_ENTITIES, xSerialIDs);
	(void)RunSimFramesTimed(pxSerial, xSerialIDs, uNUM_FRAMES, /*bParallel=*/false);
	const uint64_t uSerialHash = HashSceneTransforms(pxSerial, xSerialIDs);
	g_xEngine.Scenes().UnloadScene(xSerialScene);

	std::printf("[check-sim-determinism] serial hash=0x%016llx\n",
		static_cast<unsigned long long>(uSerialHash));
	std::fflush(stdout);

	bool bAllMatch = true;
	for (u_int uRun = 0; uRun < uPARALLEL_RUNS; ++uRun)
	{
		char acName[64];
		std::snprintf(acName, sizeof(acName), "CheckSimDet_Parallel_%u", uRun);
		Zenith_Scene xScene = g_xEngine.Scenes().CreateEmptyScene(acName);
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
		Zenith_Vector<Zenith_EntityID> xIDs;
		BuildTweenScene(pxData, uNUM_ENTITIES, xIDs);
		(void)RunSimFramesTimed(pxData, xIDs, uNUM_FRAMES, /*bParallel=*/true);
		const uint64_t uParallelHash = HashSceneTransforms(pxData, xIDs);
		g_xEngine.Scenes().UnloadScene(xScene);

		if (uParallelHash != uSerialHash)
		{
			bAllMatch = false;
			std::printf("[check-sim-determinism] MISMATCH run=%u parallel hash=0x%016llx != serial 0x%016llx\n",
				uRun,
				static_cast<unsigned long long>(uParallelHash),
				static_cast<unsigned long long>(uSerialHash));
			std::fflush(stdout);
		}
	}

	if (bAllMatch)
	{
		std::printf("[check-sim-determinism] PASS: all %u parallel runs byte-identical to serial (hash=0x%016llx)\n",
			uPARALLEL_RUNS, static_cast<unsigned long long>(uSerialHash));
	}
	else
	{
		std::printf("[check-sim-determinism] FAIL: parallel sim diverged from serial\n");
	}
	std::fflush(stdout);
	return bAllMatch;
}
