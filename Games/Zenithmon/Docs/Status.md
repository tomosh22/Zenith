# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 in progress: type chart + species (roster + stats + learnsets, box DONE) + move table landed.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1119 ran, 1118 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 9 type-chart + 24 species (16 dex + 8 learnset) + 16 move + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1119** in `zm-tests.yml`.

## What landed

- **PR #147:** type chart + CI unit gate (ZM-D-018/019).
- **PR #148 / #149:** `ZM_SpeciesData` structural roster (152) + derived base stats (ZM-D-020/021).
- **PR #150:** `ZM_MoveData` -- 218-move table over a 57-kind `ZM_MOVE_EFFECT` enum (data + schema only; executor is S2; ZM-D-022).
- **PR #151 (this iteration):** `ZM_Learnsets` -- derived per-species level-up learnsets (`ZM_GetSpeciesLearnset`; ZM-D-023) + 8 `ZM_Data` tests. **Completes the `ZM_SpeciesData` box (now `[x]`).**

## Current task

None in flight (once PR #151 merges). **Next Roadmap task: `ZM_ItemData` (~80 items + TMs)** -- Roadmap S1 box 4. Model item categories (balls, medicine, held items, TMs, key items, berries-analog), a `ZM_ITEM_ID` + `ZM_ItemData` table (compiled C array), and TMs as items that map to a `ZM_MOVE_ID` (this is where TM/tutor learnset compatibility can be introduced). Then `ZM_AbilityData` + `ZM_NatureData` (box 5), `ZM_StatCalc` + `ZM_BattleRNG` (box 6), `ZM_WorldSpec` (box 7), `ZM_DataRegistry` (box 8).

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1119**. Forgetting it reddens zm-tests with a count mismatch.
- Verify unit tests via a boot, not `zenith test` (which skips them): `Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N` prints `Unit tests complete: N ran ... M failed`. See Q-2026-07-10-004.
- **Base stats AND learnsets are DERIVED placeholders** (ZM-D-021 / ZM-D-023): `ZM_GetSpeciesBaseStats` and `ZM_GetSpeciesLearnset` compute from the tables; replace the accessor bodies with stored tables in the S11 balance pass (signatures are the stable seam). Learnsets draw only from the species' type(s) + NORMAL, teach a STAB move at L1, are damaging-dominant, and grow with evo stage.
- **Move data is INERT** (ZM-D-022): the `ZM_MoveExecutor` that interprets `ZM_MOVE_EFFECT` is S2. Every effect kind is used by >=1 move (locked). TMs (`ZM_ItemData`) will reference `ZM_MOVE_ID`.
- **Big-table / derivation authoring tip:** move rows + the learnset derivation were both prototyped + fully validated offline in a scratchpad Python script (parsing the committed `.cpp` tables) BEFORE building -- catches design/coverage bugs in seconds, not build cycles. Not committed; the C++ is the source of truth (ZM-D-009).
- **Working-dir gotcha:** the Bash and PowerShell tools SHARE a cwd here -- a `cd` in Bash moves PowerShell too. Avoid `cd`; use `git -C` / absolute paths / `Set-Location C:\dev\Zenith` before `regen`/`build`.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- Branch fresh off master (`git pull` first).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
