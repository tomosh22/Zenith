# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box-2 SC2 (stat-stage effects) COMPLETE. Box 2 is 6 ordered sub-commits (ZM-D-033); SC1+SC2 done, next: SC3 (delivery variants + field/screen setters + screen damage reduction).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`). D3D12_False link proof in CI.
- Unit (T0): **1234 ran, 1233 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 102 `ZM_Data` + **62 `ZM_Battle`** (14 box-1 + 5 SC1 + 43 SC2) + 1068 engine + 2 boot. **Baseline 1234** in `zm-tests.yml`.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.

## What landed this session -- S2 box 1 (the battle-engine keystone)

`Source/Battle/`: `ZM_BattleTypes.h` (SIDE/status/volatile/stat/weather/screen enums + `ZM_BattleAction` + `ZM_BattleConfig`) / `ZM_BattleMonster.{h,cpp}` (`ZM_BattleMonster` + `ZM_BattleMonsterSpec` w/ base-stat override + `ZM_BuildBattleMonster`) / `ZM_BattleEvent.h` (flat 7-field POD + `ZM_MakeEvent` + full kind enum incl. reserved) / `ZM_DamageCalc.{h,cpp}` (real minimal Gen-V pipeline + `ZM_ApplyStatStage` + `ZM_EffectivenessPercent`) / `ZM_BattleState.{h,cpp}` (sides+field+RNG) / `ZM_BattleEngine.{h,cpp}` (Begin/SubmitAction/ResolveTurn turn loop). `Tests/ZM_Tests_Battle.cpp` (14 `ZM_Battle` tests). Arch decision = **ZM-D-032**. Design doc archived in session scratchpad `S2_Box1_Design.md`; offline oracle `scratchpad/zm_battle_ref.py` (PCG32+stat+damage, validated vs S1 goldens) derived the scenario golden.

## Current task

None -- SC2 committed + pushed. **Box 2 = 6 ordered sub-commits (full plan + LOCKED draw order in DecisionLog ZM-D-033).** Done: **SC1** (executor seam) + **SC2** (all stat-stage effects LOWER_*/RAISE_* incl. multi-stat + RAISE_CRIT counter, accuracy/evasion stages, STATUS-category-vs-damaging-secondary dispatch; lights `STAT_STAGE_CHANGED` + `MOVE_FAILED(STAT_MAXED)`; reviewer-fixed RAISE_CRIT magnitude + multi-stat MOVE_FAILED semantics + acc/eva clamp). **Next: SC3** -- delivery variants (MULTI_HIT/DOUBLE_HIT/RECOIL/DRAIN/HEAL_HALF/FIXED_LEVEL/HALVE_HP/OHKO) + field/screen/hazard SETTERS (state-only, NO event -- box 3 owns weather/screen events) + `bScreen` damage reduction; lights `MULTI_HIT/RECOIL/DRAIN/HEAL`. Then SC4 (status majors + burn/para, appends `m_iTag` to ZM_BattleEvent) -> SC5 (volatiles + switch) -> SC6 (pre-move actions + `ZM_CatchCalc` + the 2000-battle soak). Two tracked mechanics deferrals in Shortfalls.md 1.2 (high-crit flag fold; self-buff-secondary KO edge). **S2 has NO visual gate**, so the loop runs autonomously through S2.

## Notes for the next agent

- **LOCKED contracts box 2 must NOT break (ZM-D-032):** the `ZM_BattleEvent` POD field set + emit order per hit (`MOVE_USED -> {MISSED | IMMUNE | ([CRIT][SUPER|NOT] DAMAGE_DEALT [FAINT])}`, neutral silent), and the RNG draw order (`accuracy-if-can-miss -> crit RandBelow(24) -> roll RandRange(85,100)`, attacker-then-defender, one `RandBelow(2)` tie-break only on exact effective-speed tie). Later boxes APPEND event kinds/fields (default 0) so box-1 goldens never shift. Secondary-effect procs draw AFTER the damage roll (append, never interleave).
- **`DAMAGE_DEALT.m_iAmount` is RAW rolled damage (pre-clamp), NOT HP lost** -- box 4 exp/UI must read `m_iAux` (remaining HP) to compute HP lost. Comment fixed in `ZM_BattleEvent.h`.
- **Golden-vector discipline (reuse for box 2):** prototype expected event streams in a scratchpad Python oracle validated against S1 golden vectors BEFORE trusting the engine; the 2000-battle fuzz soak (real target) lands with box-2 move variety (box 1 shipped a 50-battle 1v1 smoke scaffold + `ZM_ValidateEventStream`).
- **Orchestration that worked (this session):** design via a 3-architect Workflow panel -> synthesize; parallel Implementer (Source/Battle) + Test Author (tests + oracle) with the design's section-2 header sketches as the NORMATIVE api so they converge without naming drift; orchestrator owns regen/build/test/oracle-run/reviewer/commit. `Zenith_Vector` has `.Get(idx)` (NO `operator[]`).
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now 1186).
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
