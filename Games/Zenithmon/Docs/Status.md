# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 in progress: type chart + species structural roster landed.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1090 ran, 1089 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 9 type-chart + 11 species + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1090** in `zm-tests.yml`.

## What landed

- **PR #147 (merged):** S1 type chart -- `ZM_Types` + `ZM_TypeChart` (golden-locked 18x18) + CI unit gate (ZM-D-018/019).
- **PR #148 (this iteration):** `ZM_SpeciesData` **structural roster** -- schema + enums (`ZM_ARCHETYPE`/`ZM_RARITY`/`ZM_SIZE_CLASS`/`ZM_SPECIES_ID`) + full 152-species table (id/name/types/archetype/evo/family/rarity, transcribed from GDD 5) + derived size/seed accessors + 11 `ZM_Data` integrity tests. DecisionLog ZM-D-020.

## Current task

None in flight (once PR #148 merges). **The `ZM_SpeciesData` Roadmap box is `[~]` WIP:** structural roster done; **REMAINING on the box = per-species base stats (a design pass) + learnsets (need `ZM_MoveData`).** So the next task is either continue that box with **base stats** (increment 2 -- add a base-stats field to `ZM_SpeciesData` + values + range/BST tests; bump baseline) or start `ZM_MoveData` (box 3). Base stats have no upstream dependency; learnsets must wait for moves.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1090**. Forgetting it reddens zm-tests with a count mismatch (that is the ratchet working).
- Verify unit tests via a boot, not `zenith test` (which skips them): `zenithmon.exe --list-automated-tests --headless` prints `Unit tests complete: N ran … M failed`, or `Tools/run_unit_gate.ps1`. See Q-2026-07-10-004.
- **Working-dir gotcha:** the Bash and PowerShell tools SHARE a cwd here -- a `cd` in Bash moves PowerShell too. Avoid `cd`; use absolute paths / `Set-Location C:\dev\Zenith` before `regen`/`build`.
- `Source/Data/` is globbed by Sharpmake; run `Build\regen.ps1` after adding files.
- Branch fresh off master (`git pull` first).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
