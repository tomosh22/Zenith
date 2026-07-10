# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 started: type chart landed; ZM_* unit tests now gate in CI.**

**Read this first each session.** Replaced at every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the honest gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon` (per-game sln, `/t:Zenithmon`). D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1079 ran, 1078 passed, 0 failed, 1 skipped** at boot (the 1 skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). +9 over the S0 baseline = the new type-chart suite.
- Automated (P1): **1 / 1 passed** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- **CI now runs the unit suite** (this iteration's fix, ZM-D-019): `zm-tests.yml` boots `zenithmon.exe` via `Tools/run_unit_gate.ps1 -Baseline 1079`. Before this, BOTH `zenith test` and the zm-tests steps passed `--skip-unit-tests`, so no ZM unit test ran in CI.

## What landed (this iteration -- PR #147)

- **S1 first Roadmap task:** `Source/Data/ZM_Types.h` (`enum ZM_TYPE : u_int`, 18 GDD types, `ZM_TypeToString`) + `ZM_TypeChart` (const 18x18 effectiveness table + `GetEffectiveness` + `GetDualTypeEffectiveness`) + 9 `Tests/ZM_Tests_Data.cpp` cases (golden-matrix two-place lock, GDD design-intent spot checks, dual-type products, ToString). DecisionLog ZM-D-018.
- **CI unit-test gate** wired into `zm-tests.yml` (DecisionLog ZM-D-019) so the S1/S2 unit backbone actually runs on every PR. Docs updated: CIPolicy §1/§6, TestPlan §4, Questions Q-2026-07-10-004.

## Current task

None in flight (once PR #147 merges). **Next per [Roadmap.md](Roadmap.md): S1 second box -- `ZM_SpeciesData` (~150 species: archetype + evo stage + size class + family seed + stats/learnsets).** Then MoveData / ItemData / AbilityData+NatureData / StatCalc+BattleRNG / WorldSpec skeleton / DataRegistry. S3 (overworld, engine E1/E2) is the parallel track.

## Notes for the next agent

- **NEW: every PR that adds/removes `ZM_*` unit tests must bump `-Baseline` in `.github/workflows/zm-tests.yml`** (currently 1079). An engine PR that changes the engine unit count also bumps it. This is the ratchet that makes unit tests gate; forget it and zm-tests reddens with a count mismatch (that's the point).
- **Verify unit tests via a boot, not `zenith test`** (which skips them): `Tools/run_unit_gate.ps1 -Exe <zenithmon.exe> -Baseline <N>` or `zenithmon.exe --list-automated-tests --headless` (no `--skip-unit-tests`). See Q-2026-07-10-004.
- New game code lives under `Source/Data/`; Sharpmake globs the whole game tree, so `Build\regen.ps1` after adding files (done this PR).
- Branch fresh off master (`git pull` first; master tip after this = the #147 squash).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored.
- Session discipline: replace this file at session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
