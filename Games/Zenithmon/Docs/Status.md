# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 in progress: type chart + species roster + species base stats landed.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1095 ran, 1094 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 9 type-chart + 16 species (11 structural + 5 base-stat) + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1095** in `zm-tests.yml`.

## What landed

- **PR #147:** type chart (`ZM_Types` + `ZM_TypeChart`) + CI unit gate (ZM-D-018/019).
- **PR #148:** `ZM_SpeciesData` structural roster (152 species) + 11 integrity tests (ZM-D-020).
- **PR #149 (this iteration):** `ZM_SpeciesData` **base stats** -- `ZM_STAT` enum + `ZM_BaseStats` + `ZM_GetSpeciesBaseStats/BaseStatTotal`, systematically derived (per-archetype profile x stage/rarity x per-family emphasis; ZM-D-021) + 5 `BaseStats_*` tests.

## Current task

None in flight (once PR #149 merges). **The `ZM_SpeciesData` box (`[~]`) now has only LEARNSETS remaining, and learnsets are BLOCKED on `ZM_MoveData`.** So the next task is **`ZM_MoveData` (Roadmap box 3)** -- ~220 moves as table rows over a ~60-kind `ZM_MOVE_EFFECT` enum (GDD section 7 has the effect list / formula spec). After MoveData lands, return to `ZM_SpeciesData` to add per-species learnsets (referencing `ZM_MOVE_ID`), then the box can be ticked `[x]`.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1095**. Forgetting it reddens zm-tests with a count mismatch.
- Verify unit tests via a boot, not `zenith test` (which skips them): `zenithmon.exe --list-automated-tests --headless` prints `Unit tests complete: N ran … M failed`, or `Tools/run_unit_gate.ps1`. See Q-2026-07-10-004.
- **Base stats are DERIVED, not stored** (ZM-D-021) -- a placeholder for S11 balance. If you add real per-species base-stat tuning, replace the `ZM_GetSpeciesBaseStats` body with a stored table; the signature stays.
- **Working-dir gotcha:** the Bash and PowerShell tools SHARE a cwd here -- a `cd` in Bash moves PowerShell too. Avoid `cd`; use `git -C` / absolute paths / `Set-Location C:\dev\Zenith` before `regen`/`build`.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- Branch fresh off master (`git pull` first).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
