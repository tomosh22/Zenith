# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 data core: boxes 1-5 of 8 done (types, species, moves, items, abilities+natures). Box 6 (StatCalc+RNG) next.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1142 ran, 1141 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 72 `ZM_*` (9 type + 24 species + 16 move + 11 item + 6 nature + 6 ability) + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1142** in `zm-tests.yml`.

## What landed (S1 data core)

- **PR #147:** type chart (ZM-D-018).
- **PR #148/#149/#151:** `ZM_SpeciesData` -- roster + derived base stats + derived learnsets (box DONE).
- **PR #150:** `ZM_MoveData` (218 moves / 57-kind effect enum).
- **PR #152:** `ZM_ItemData` (90 items / 34-kind effect enum / 25 TMs).
- **PR #153 (this iteration):** `ZM_NatureData` (25, exact 5x5 grid; ZM-D-025) + `ZM_AbilityData` (50 abilities: roster + metadata + `ZM_ABILITY_HOOK` surface bitmask; hook bodies are S2; ZM-D-026). Box 5 DONE.

## Current task

None in flight (once PR #153 merges). **Next Roadmap task: `ZM_StatCalc` (pure formulas) + `ZM_BattleRNG` (PCG32)** -- Roadmap S1 box 6. This is LOGIC, not a data table:
- `ZM_StatCalc`: Gen-III+ integer stat formulas -- HP = `((2*base + IV + EV/4) * level)/100 + level + 10`; other five = `(((2*base + IV + EV/4) * level)/100 + 5) * naturePercent / 100` using `ZM_GetNatureStatPercent` (ZM-D-025). Golden-vector tests (level 1/50/100 x IV 0/31 x EV 0/252 x nature up/down/neutral, HP vs non-HP).
- `ZM_BattleRNG`: PCG32 (seeded, deterministic) -- `Next()` / `RandRange(n)` / `Chance(percentOrNum, den)`; tests for determinism (same seed -> same stream), range bounds, and a distribution sanity check.
Then box 7 `ZM_WorldSpec` (the keystone world table), box 8 `ZM_DataRegistry` (name->ID indices + cross-table validation) closes S1.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1142**.
- Verify unit tests via a boot: `Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`. See Q-2026-07-10-004.
- **S1 patterns established:** (1) big data tables authored + validated in a scratchpad Python generator before building, coverage-locked (moves/items/abilities); committed C++ is the source of truth (ZM-D-009). (2) "Data now, executor later": move/item effect enums + the ability hook bitmask all name behaviour the S2 battle engine will implement -- every kind/hook is coverage-tested so S2 has subjects. (3) Derived placeholders (base stats ZM-D-021, learnsets ZM-D-023) vs exact real tables (natures ZM-D-025) -- pick per whether the set is closed.
- **`ZM_STAT` lives in `ZM_SpeciesData.h`**; natures + StatCalc reference it (include that header). `ZM_GetNatureStatPercent` (ZM_NatureData.h) is the nature multiplier StatCalc consumes.
- **Working-dir gotcha:** Bash and PowerShell SHARE a cwd here -- use `Set-Location C:\dev\Zenith` before regen/build; avoid `cd`. Tracked `Tools/**/__pycache__/*.pyc` drift on build -- `git checkout --` them; never stage them.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`). Branch fresh off master.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
