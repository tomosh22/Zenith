# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 data core: boxes 1-6 of 8 done. Box 7 (WorldSpec skeleton) next, then box 8 (DataRegistry) closes S1.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1152 ran, 1151 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 82 `ZM_*` (9 type + 24 species + 16 move + 11 item + 6 nature + 6 ability + 4 statcalc + 6 rng) + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1152** in `zm-tests.yml`.

## What landed (S1 data core: 6/8 boxes)

- Box 1 type chart (#147); Box 2 species roster+stats+learnsets (#148/#149/#151, DONE); Box 3 moves (#150); Box 4 items (#152); Box 5 abilities+natures (#153).
- **Box 6 (PR #154, this iteration):** `ZM_StatCalc` (Gen-III+ integer stat formulas + nature %) + `ZM_BattleRNG` (PCG32 seeded RNG). 10 `ZM_Data` tests incl. stat golden vectors + a golden PCG32 stream (ZM-D-027).

## Current task

None in flight (once PR #154 merges). **Next Roadmap task: `ZM_WorldSpec` skeleton** -- Roadmap S1 box 7, the KEYSTONE declarative world table (ZM-D-005). For S1 this is the SCHEMA + a small proving set, NOT the full ~40 scenes (that's S9/S10):
- A `ZM_WorldSpec` row per scene: name, build index, scene kind (FrontEnd/Town/Route/Interior/Gym/Battle/Tower), terrain-set name, connections (warp edges w/ target build index + spawn tag), optional encounter-table ref + rate, trainer/shop/gym refs, story-flag gate. Define the enums + struct + a compiled table with the handful of scenes that exist conceptually now (FrontEnd idx 0, Battle idx 1, Dawnmere, Route 1, a lab interior -- enough to exercise every column).
- Referential-integrity tests: every warp target build index resolves to a real WorldSpec row, every spawn tag referenced exists, every encounter species is a real `ZM_SPECIES_ID`, build indices unique. This is the schema enforcer that keeps the world honest before S3+ author real scenes.
Then box 8 `ZM_DataRegistry` (name->ID indices across all tables + a cross-table `ZM_Tests_Data` schema-enforcer suite) CLOSES S1. **S1 gate:** ~90 unit tests (currently 82 `ZM_*`), no visual check.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1152**.
- Verify unit tests via a boot: `Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`. See Q-2026-07-10-004.
- **`ZM_BattleRNG` is the ONLY sanctioned RNG** in game logic (ZM-D-027, PCG32, header-only, deterministic from seed). `ZM_StatCalc` is the integer stat math (no floats). S2's battle engine builds on both. The golden PCG32 stream pins the exact algorithm -- do not change the constants/seeding without re-deriving the golden vectors.
- **S1 patterns:** big data tables + golden vectors are prototyped/validated in scratchpad Python BEFORE building (moves/items/abilities rosters; stat + PCG32 golden values) -- committed C++ is source of truth (ZM-D-009). "Data now, executor later" for move/item/ability behaviour (S2). `ZM_STAT` lives in `ZM_SpeciesData.h`.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- `Set-Location C:\dev\Zenith` before regen/build; avoid `cd`. Tracked `Tools/**/__pycache__/*.pyc` drift on build -- `git checkout --` them; never stage them.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`). Branch fresh off master.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
