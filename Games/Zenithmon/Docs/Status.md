# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box 3 IN PROGRESS at SC2/5. SC2 (ability-hook infrastructure + SWITCH_IN abilities) is complete and GREEN: the 12-pfn/50-row hook table, safe accessor, Begin/DoSwitch dispatch, four weather callers, Daunting Roar, Pressure Aura, shared stat-change seam, and 18 tests are landing in this commit. Box 3 and the full ability roster remain incomplete. Next: S2 box 3 SC3 (damage/stat/type-interaction abilities).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. The box-3 execution plan + design rulings are **ZM-D-036** in [DecisionLog.md](DecisionLog.md).

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Regen GREEN (the new `ZM_AbilityHooks.cpp` translation unit is present in the generated game project).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit (T0): **1446 registered, 1445 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = **107 `ZM_Data`** + **269 `ZM_Battle`** + 1068 engine + 2 boot. **Baseline 1446** in `zm-tests.yml` (bumped this commit).
- Automated (P1): **1/1** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- Reviews GREEN: independent implementation + test-quality passes completed; the explicit 12-ability WEATHER whitelist and Pressure Aura SELF/FIELD/NONE-defender negative cases were added, then the full SC2 review reran GREEN.
- Landing set is clean of review findings and **commit-ready**; the expected uncommitted SC2 source/test/docs/workflow changes remain for the orchestrator to commit together.

## What landed this session -- S2 box-3 SC2 (hook infra + SWITCH_IN abilities)

SC2 adds `ZM_AbilityHooks.h/.cpp`: a non-owning `ZM_AbilityContext`, the documented 12 plain-function-pointer slots over 11 metadata bits, a stable `const` 50-row table, and a safe `ZM_GetAbilityHooks` sentinel/out-of-range contract. Only six rows are live in this ordered slice: DAUNTINGROAR, RAINCALLER, SUNCALLER, SANDCALLER, SNOWCALLER, and PRESSUREAURA. `Begin` dispatches each lead immediately after its SWITCH_IN in PLAYER-then-ENEMY order; `DoSwitch` uses the same seam. The four callers set five-turn weather, overwrite a different weather, and strictly no-op on identical weather. Daunting Roar uses the promoted shared `ZM_StatusLogic::ApplyStatChange`; Pressure Aura taxes only opponent-target moves against its holder (2 PP, zero-clamped), leaving SELF/FIELD/NONE-defender costs at 1. The slice adds no RNG draw, preserves the exact NONE stream, and grows coverage incrementally without fake future hooks. Eighteen tests and two independent reviews lock the behavior (ZM-D-039). SC1 weather remains complete; box 3 stays unchecked until SC5.

## Current task

**Next per [Roadmap.md](Roadmap.md) S2 box 3: SC3 -- damage-dealt/taken, modify-stat, and type-interaction abilities.** Reuse the SC2 hook/context table in `ZM_AbilityHooks.cpp`; add dispatch at the existing stat/damage/immunity seams in `ZM_MoveExecutor.cpp` and the move-order-only speed seam in `ZM_BattleEngine.cpp` (ability speed must not affect flee). The 20 SC3 rows are VERDANTSURGE, EMBERSURGE, TIDALSURGE, HIVESURGE, SKYWARDGRACE, BEDROCK, SUNCHASER, STREAMLINE, GRITSTRIDE, RIMESTRIDE, FERVOR, BLUBBER, AQUIFER, DYNAMO, CINDERDRINK, GRAZER, SOLIDCORE, HEAVYPLATE, GOSSAMER, and DOWNDRAFT. These are edits to existing files, so no regen is needed unless the implementation genuinely adds a file. The Roadmap box-3 line stays UNCHECKED until SC5 lands.

## Notes for the next agent

- **Design archive:** the full box-3 design (all 50 ability bodies + constants, hook struct, dispatch points, per-SC test lists) lives in the session scratchpad `zm_s2_box3_design.md` -- if starting cold and it's gone, ZM-D-036 + the Roadmap S2 line + this file carry enough to resume; re-derive per-SC detail from `ZM_AbilityData.{h,cpp}` (the 50-row table + `m_uHookMask`) + `GameDesignDocument.md`.
- **Coverage invariant (staged through SC5):** at SC2, the six installed rows completely realize their declared masks via live pfns and the explicit engine-side exceptions, every live slot maps to a declared bit, and every other executable slot remains null until its ordered slice. SC3/SC4 extend that installed set; the all-50 complete-realization check belongs to SC5. QUICKDRAW/DAUNTINGROAR/BLOODRUSH realize MODIFY_STAT engine-side; the exact 12 weather-coupled abilities realize WEATHER as a condition, not a pfn (ZM-D-036/039).
- **DLL-heal gotcha:** Zenithmon's FIRST build in a fresh config leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, "no 'Unit tests complete' line"). Fix: `Import-Module Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>`. `zenith test` heals automatically; the bare unit gate does NOT. (Did not recur this session -- config was warm.)
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (looks like the DLL crash). Set the bit AND its counter together.
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now **1446**).
- **Box-2/3 LOCKED contracts (ZM-D-032..036):** `ZM_BattleEvent` POD is append-only (new kinds append before COUNT, new fields default 0); RNG draw order fixed (PHASE G/M/E); a NONE-ability/weather-NONE/status-free actor draws ZERO and emits ZERO; EoT order = weather chip FIRST -> weather/screen countdown -> PLAYER-then-ENEMY status ticks -> (SC5) ability TURN_END heals (skip fainted) -> TURN_END. Later SCs APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
