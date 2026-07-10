# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 in progress: type chart + species (roster + base stats) + full move table landed.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` clean via `zenith build Zenithmon`. D3D12_False link proof runs in CI.

## Tests

- Unit (T0, category `ZM_Data`): **1111 ran, 1110 passed, 0 failed, 1 skipped** at boot (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 9 type-chart + 16 species + **16 move** + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- CI runs the unit suite via `run_unit_gate.ps1` (ZM-D-019). **Baseline is now 1111** in `zm-tests.yml`.

## What landed

- **PR #147:** type chart (`ZM_Types` + `ZM_TypeChart`) + CI unit gate (ZM-D-018/019).
- **PR #148 / #149:** `ZM_SpeciesData` structural roster (152) + derived base stats (ZM-D-020/021).
- **PR #150 (this iteration):** `ZM_MoveData` -- 218-move compiled table over a 57-kind `ZM_MOVE_EFFECT` enum + `ZM_MOVE_ID`/`ZM_MOVE_CATEGORY`/`ZM_MOVE_TARGET` schema + accessors + `ZM_MoveCategory/TargetToString`; 16 `ZM_Data` integrity tests (ZM-D-022). **Data + schema only -- the `ZM_MoveExecutor` that interprets the effect enum is S2.**

## Current task

None in flight (once PR #150 merges). **Next Roadmap task: return to the `ZM_SpeciesData` box (`[~]`) to add per-species LEARNSETS** -- now UNBLOCKED (they reference `ZM_MOVE_ID`, which exists). Add a learnset table (level-up moves per species; TMs later with `ZM_ItemData`), a registry-integrity test that every learnset entry resolves to a real move, then tick the species box `[x]`. After that the first unchecked S1 box is **`ZM_ItemData` (~80 + TMs)** (Roadmap box 4).

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever you add/remove `ZM_*` unit tests. Currently **1111**. Forgetting it reddens zm-tests with a count mismatch.
- Verify unit tests via a boot, not `zenith test` (which skips them): `Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N` prints `Unit tests complete: N ran ... M failed`. See Q-2026-07-10-004.
- **Move data is INERT** (ZM-D-022): rows name an effect kind + magnitude + chance; nothing interprets them until S2's `ZM_MoveExecutor` switch. Every `ZM_MOVE_EFFECT` value is used by >=1 row (locked by `Moves_EveryEffectKindUsed`) so S2 per-effect scenarios all have a subject. `m_uAccuracy==0` means never-miss; `m_uPower==0` means status or fixed-damage; `m_uEffectChance==0` iff effect is NONE.
- **Big-table authoring tip:** the 218 rows were generated + validated offline by a scratchpad Python script (enum body + table body emitted in lockstep, all invariants checked before building) -- NOT committed; the C++ table is the source of truth thereafter (ZM-D-009). Hand-edit the `.cpp` for future tuning.
- **Base stats are DERIVED, not stored** (ZM-D-021) -- a placeholder for S11 balance.
- **Working-dir gotcha:** the Bash and PowerShell tools SHARE a cwd here -- a `cd` in Bash moves PowerShell too. Avoid `cd`; use `git -C` / absolute paths / `Set-Location C:\dev\Zenith` before `regen`/`build`.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- Branch fresh off master (`git pull` first).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
