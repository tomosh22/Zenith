#pragma once

// ============================================================================
// Zenith_BenchECS
//
// A deterministic, GPU-free ECS micro-benchmark. It exercises ONLY the public
// ECS API (empty additive scene + entities + Transform/Light components +
// Query<...>().ForEach + Add/Remove churn) and prints parseable timing lines:
//
//   BENCH ecs.query_foreach N=<n> iters=<m> ms=<elapsed>
//
// This is the before/after measurement backstop for the future sparse-set
// query rework. It performs NO Vulkan / Flux / GPU work, so it runs cleanly in
// a headless process. Wire it up via the --bench-ecs command-line flag (see
// Zenith_Main.cpp): the flag runs Zenith_BenchECS_Run() after engine init and
// then exits cleanly.
// ============================================================================

// Run the full benchmark sweep: a fixed iteration count for each of the
// canonical entity counts (1000, 10000, 50000), printing one BENCH line per
// count. Cleans up every scene it creates. Safe to call after engine init.
void Zenith_BenchECS_Run();

// Test/measurement helper: run a SINGLE benchmark pass for the given entity
// count and iteration count. Creates an empty additive scene, populates it,
// runs the Query<...>().ForEach + Add/Remove churn, tears the scene down, and
// returns the total number of component instances visited across all
// iterations (the "processed count"). Used both by Zenith_BenchECS_Run (for
// the printed sweep) and by the Core/BenchECSSmoke unit test (tiny N/iters).
//
// WS10: bUseSparse selects the Query READ path for the hot ForEach loops. The
// flag is pinned via g_xEngine.Scenes().SetSparseQueryReads(bUseSparse) around
// the hot loops and the prior value restored before return. The Add/Remove
// churn always dual-writes the sparse index regardless of path. Defaults to
// true (sparse) so the existing Core/BenchECSSmoke call site is unchanged.
u_int64 Zenith_BenchECS_RunOnce(u_int uNumEntities, u_int uIters, bool bUseSparse = true);

// ============================================================================
// WS12 parallel-simulation bench + determinism soak (siblings of the ECS bench;
// share this TU to avoid a Sharpmake regen). Both build N collider-free Tween
// entities via the public ECS API and drive the per-frame OnUpdate chokepoint
// (Zenith_SceneData::DispatchOnUpdateForEntities) with the parallel-sim flag
// OFF then ON. Both are GPU-free, so they run cleanly in a headless process.
// ============================================================================

// --bench-sim entry point. For each canonical N (256, 1024, 4096): build the
// scene, time M update passes with parallel OFF (serial) then ON (parallel),
// print one BENCH line each + a ratio, and IN-BENCH Zenith_Assert that the
// field-by-field state hash is identical across the two passes. Restores the
// flag. Honest expectation: ~nil/modest speedup — the bench proves NO
// REGRESSION when off + measures the wave-building overhead when on.
void Zenith_BenchSim_Run();

// --check-sim-determinism entry point (the heavy soak the orchestrator runs
// explicitly). Builds a fixed scene of ~256 collider-free Tween entities, runs
// ~300 fixed-dt frames serial to get a reference hash, then repeats the
// parallel run ~16x asserting ALL parallel hashes equal the serial hash. Prints
// a single clear PASS/FAIL line. Returns true on PASS.
bool Zenith_CheckSimDeterminism_Run();
