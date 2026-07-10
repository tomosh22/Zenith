# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 in progress: type chart + species (done) + moves + items landed. Boxes 1-4 of 8 done.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1130 ran, 1129 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 9 type-chart + 24 species + 16 move + 11 item + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1130** in `zm-tests.yml`.

## What landed (S1 data core so far)

- **PR #147:** type chart + CI unit gate (ZM-D-018/019).
- **PR #148/#149/#151:** `ZM_SpeciesData` -- roster (152) + derived base stats + derived learnsets (box DONE; ZM-D-020/021/023).
- **PR #150:** `ZM_MoveData` -- 218 moves / 57-kind effect enum (data+schema; ZM-D-022).
- **PR #152 (this iteration):** `ZM_ItemData` -- 90 items / 34-kind effect enum / 9 categories, 25 TMs -> `ZM_MOVE_ID`; 11 `ZM_Data` tests (ZM-D-024). Data + schema only; bag/battle executor is S2/S5.

## Current task

None in flight (once PR #152 merges). **Next Roadmap task: `ZM_AbilityData` stubs + `ZM_NatureData`** -- Roadmap S1 box 5. Abilities: ~50 as fn-pointer hook structs (OnSwitchIn/OnModifyStat/OnModifyDamage/OnStatusTry/OnContact/OnTurnEnd/OnFaint/OnAccuracy...) -- for S1, define the `ZM_ABILITY_ID` enum + `ZM_AbilityData` metadata table (id/name/description) + the hook-struct SHAPE (stub fn-pointers, null-defaulted); the real hook bodies are wired in S2. Natures: 25 `ZM_NATURE` with the +10%/-10% stat pair (neutral natures raise==lower). Then box 6 `ZM_StatCalc` + `ZM_BattleRNG` (PCG32), box 7 `ZM_WorldSpec`, box 8 `ZM_DataRegistry`.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1130**.
- Verify unit tests via a boot, not `zenith test` (which skips them): `Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`. See Q-2026-07-10-004.
- **Data tables are INERT + executor comes later** is the established S1 pattern: MoveData effect enum -> S2 `ZM_MoveExecutor`; ItemData effect enum -> S2/S5 bag/battle logic. Every effect kind is coverage-locked to >=1 row. Follow this for abilities (hook structs stubbed in S1, bodies in S2).
- **Derived placeholders** (base stats ZM-D-021, learnsets ZM-D-023): computed accessors, replaced by stored tables in the S11 balance pass. Natures likely a small real table (25 rows); abilities metadata real + hooks stubbed.
- **Big-table authoring:** move + item rows were authored + fully validated in a scratchpad Python generator (coverage, cross-rules, TM->move resolution) BEFORE building; committed C++ is the source of truth (ZM-D-009). Reuse the pattern for large tables.
- **Working-dir gotcha:** Bash and PowerShell SHARE a cwd here -- a `cd` in Bash moves PowerShell too. Use `Set-Location C:\dev\Zenith` before regen/build; avoid `cd`.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`). Branch fresh off master (`git pull` first).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
