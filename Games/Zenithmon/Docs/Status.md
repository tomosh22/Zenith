# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box-2 SC4 (ZM_StatusLogic majors) COMPLETE. Box 2 is 6 ordered sub-commits (ZM-D-033); SC1-SC4 done, next: SC5 (volatiles + SWAGGER + FORCE_SWITCH + DoSwitch primitive).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`). D3D12_False link proof in CI.
- Unit (T0): **1327 ran, 1326 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 102 `ZM_Data` + **155 `ZM_Battle`** (14 box-1 + 5 SC1 + 43 SC2 + 45 SC3 + 48 SC4) + 1068 engine + 2 boot. **Baseline 1327** in `zm-tests.yml`.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.

## What landed this session -- S2 box 1 (the battle-engine keystone)

`Source/Battle/`: `ZM_BattleTypes.h` (SIDE/status/volatile/stat/weather/screen enums + `ZM_BattleAction` + `ZM_BattleConfig`) / `ZM_BattleMonster.{h,cpp}` (`ZM_BattleMonster` + `ZM_BattleMonsterSpec` w/ base-stat override + `ZM_BuildBattleMonster`) / `ZM_BattleEvent.h` (flat 7-field POD + `ZM_MakeEvent` + full kind enum incl. reserved) / `ZM_DamageCalc.{h,cpp}` (real minimal Gen-V pipeline + `ZM_ApplyStatStage` + `ZM_EffectivenessPercent`) / `ZM_BattleState.{h,cpp}` (sides+field+RNG) / `ZM_BattleEngine.{h,cpp}` (Begin/SubmitAction/ResolveTurn turn loop). `Tests/ZM_Tests_Battle.cpp` (14 `ZM_Battle` tests). Arch decision = **ZM-D-032**. Design doc archived in session scratchpad `S2_Box1_Design.md`; offline oracle `scratchpad/zm_battle_ref.py` (PCG32+stat+damage, validated vs S1 goldens) derived the scenario golden.

## Current task

None -- SC4 committed + pushed; tree CLEAN. **PARKED 2026-07-11: SC5 authoring was dispatched but both authoring subagents died on an API session limit (resets ~5:50am Europe/London) before writing any files -- no partial work on disk, master green at SC4. Resume SC5 FRESH next firing (it is fully specified below + in ZM-D-033 + plan S2_Box2_Plan.md).** **Box 2 = 6 ordered sub-commits (full plan + LOCKED draw order in DecisionLog ZM-D-033).** Done: **SC1** (executor seam) + **SC2** (stat-stage effects + STATUS-category dispatch) + **SC3** (delivery variants + field/screen/hazard state-only + `bScreen`) + **SC4** (`ZM_StatusLogic` 6 majors: apply/block/PHASE-G-gate/turn-end-chip/cure + REST/CURE_STATUS/HEAL_BELL + `bBurnedPhysical` halve-physical + paralysis speed/4; appended `int m_iTag=0` to `ZM_BattleEvent` for STATUS_DAMAGE source [Q1]; lit `STATUS_APPLIED/STATUS_DAMAGE/STATUS_CURED` + MOVE_FAILED FROZEN/ASLEEP/FULLY_PARALYZED/STATUS_BLOCKED; reviewer clean). **Next: SC5** -- volatiles (all 10: CONFUSE/FLINCH/LEECH_SEED/TRAP/TAUNT/PROTECT/ENDURE/CHARGE_TURN/SEMI_INVULN/RECHARGE/LOCK_IN) via the SC4 gate framework's reserved G1/G4/G5 slots + end-of-turn leech/trap ticks + SWAGGER + FORCE_SWITCH + a `DoSwitch` primitive (emit SWITCH_IN, clear outgoing volatiles+stages+counters, keep major); lights `FLINCH/VOLATILE_APPLIED/VOLATILE_ENDED`. Then SC6 (pre-move actions SWITCH/ITEM/RUN + `ZM_CatchCalc` + the 2000-battle soak). **Pinned conventions:** event sides -- MULTI_HIT/RECOIL/DRAIN/HEAL=ATTACKER, DAMAGE_DEALT/CRIT/eff/FAINT/STATUS_*=affected mon; end-of-turn chip order = **fixed player-then-enemy** (NOT speed-ordered -- resolves ZM-D-033 3b wording); sleep=decrement-then-check, toxic=use-then-increment (locked by SC4 tests). **Deferred to SC6:** an `IsFainted` assert in `ZM_StatusLogic::ApplyMajor` (safe until switching lands); both-sides-chip ordering + REST-while-statused coverage; SC3 recoil-self-KO/HEAL-at-full/mid-run edges. Two mechanics deferrals in Shortfalls.md 1.2. **S2 has NO visual gate**, so the loop runs autonomously through S2.

## Notes for the next agent

- **LOCKED contracts box 2 must NOT break (ZM-D-032):** the `ZM_BattleEvent` POD field set + emit order per hit (`MOVE_USED -> {MISSED | IMMUNE | ([CRIT][SUPER|NOT] DAMAGE_DEALT [FAINT])}`, neutral silent), and the RNG draw order (`accuracy-if-can-miss -> crit RandBelow(24) -> roll RandRange(85,100)`, attacker-then-defender, one `RandBelow(2)` tie-break only on exact effective-speed tie). Later boxes APPEND event kinds/fields (default 0) so box-1 goldens never shift. Secondary-effect procs draw AFTER the damage roll (append, never interleave).
- **`DAMAGE_DEALT.m_iAmount` is RAW rolled damage (pre-clamp), NOT HP lost** -- box 4 exp/UI must read `m_iAux` (remaining HP) to compute HP lost. Comment fixed in `ZM_BattleEvent.h`.
- **Golden-vector discipline (reuse for box 2):** prototype expected event streams in a scratchpad Python oracle validated against S1 golden vectors BEFORE trusting the engine; the 2000-battle fuzz soak (real target) lands with box-2 move variety (box 1 shipped a 50-battle 1v1 smoke scaffold + `ZM_ValidateEventStream`).
- **Orchestration that worked (this session):** design via a 3-architect Workflow panel -> synthesize; parallel Implementer (Source/Battle) + Test Author (tests + oracle) with the design's section-2 header sketches as the NORMATIVE api so they converge without naming drift; orchestrator owns regen/build/test/oracle-run/reviewer/commit. `Zenith_Vector` has `.Get(idx)` (NO `operator[]`).
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now 1186).
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
