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
