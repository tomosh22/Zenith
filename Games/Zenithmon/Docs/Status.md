# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box-2 SC5 (all 10 volatiles + Endure + SWAGGER + FORCE_SWITCH + engine-owned DoSwitch) COMPLETE and GREEN. Box 2 is 6 ordered sub-commits (ZM-D-033/034); SC1-SC5 done, next: SC6 (`ZM_CatchCalc` + voluntary SWITCH/ITEM/RUN + 2,000-battle soak).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Regen check GREEN (generated project state current after the SC5 source/test edits).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit (T0): **1379 ran, 1378 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 102 `ZM_Data` + **207 `ZM_Battle`** (14 box-1 + 5 SC1 + 43 SC2 + 45 SC3 + 48 SC4 + 52 SC5) + 1068 engine + 2 boot. **Baseline 1379** in `zm-tests.yml`.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.

## What landed this session -- S2 box-2 SC5

`ZM_StatusLogic` now owns the complete ten-bit volatile lifecycle and the locked G1/G4/G5 gates; `ZM_MoveExecutor` dispatches volatile primaries/secondaries, Endure, Swagger, forced switching, charge/semi-invulnerable/recharge/lock-in state, and the M0/M2/M3/M4 intercepts. `ZM_BattleMonster` carries the independent volatile counters/metadata and separate one-turn Endure flag. `ZM_BattleEngine::DoSwitch` validates the destination, silently clears outgoing battle-local state while preserving HP/PP/major status, emits `SWITCH_IN`, and is shared by forced switching now and voluntary switching in SC6. End-of-turn processing is fixed PLAYER-then-ENEMY, with per-side major chip -> Leech Seed -> Trap -> cleanup. The 52 SC5 tests lock the event encodings, durations, guarded draw order, gate/intercept precedence, exact streams, switching reset, forced-switch queued-action skip, and volatile-free compatibility. Decision = **ZM-D-034**.

## Current task

**Next: SC6, the final box-2 sub-commit.** Add `ZM_CatchCalc` (exact Gen-III/IV four-shake calculation with the ZM-D-033 rarity/status modifiers), implement voluntary SWITCH/ITEM/RUN in `ResolvePreMovePhase` with Trap restrictions and guarded RNG/event semantics, and replace/promote the 50-battle smoke target with the deterministic 2,000-battle soak (termination `< 500` turns plus HP/PP/stage/event invariants). Keep every move-only golden byte-identical. The Roadmap box-2 checkbox remains open until SC6 is complete and green. **S2 has no visual gate**, so work may continue autonomously.

## Notes for the next agent

- **LOCKED contracts box 2 must NOT break (ZM-D-032):** the `ZM_BattleEvent` POD field set + emit order per hit (`MOVE_USED -> {MISSED | IMMUNE | ([CRIT][SUPER|NOT] DAMAGE_DEALT [FAINT])}`, neutral silent), and the RNG draw order (`accuracy-if-can-miss -> crit RandBelow(24) -> roll RandRange(85,100)`, attacker-then-defender, one `RandBelow(2)` tie-break only on exact effective-speed tie). Later boxes APPEND event kinds/fields (default 0) so box-1 goldens never shift. Secondary-effect procs draw AFTER the damage roll (append, never interleave).
- **`DAMAGE_DEALT.m_iAmount` is RAW rolled damage (pre-clamp), NOT HP lost** -- box 4 exp/UI must read `m_iAux` (remaining HP) to compute HP lost. Comment fixed in `ZM_BattleEvent.h`.
- **Golden-vector discipline (reuse for box 2):** prototype expected event streams in a scratchpad Python oracle validated against S1 golden vectors BEFORE trusting the engine; the 2000-battle fuzz soak (real target) lands with box-2 move variety (box 1 shipped a 50-battle 1v1 smoke scaffold + `ZM_ValidateEventStream`).
- **Orchestration that worked (this session):** design via a 3-architect Workflow panel -> synthesize; parallel Implementer (Source/Battle) + Test Author (tests + oracle) with the design's section-2 header sketches as the NORMATIVE api so they converge without naming drift; orchestrator owns regen/build/test/oracle-run/reviewer/commit. `Zenith_Vector` has `.Get(idx)` (NO `operator[]`).
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now 1379).
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
