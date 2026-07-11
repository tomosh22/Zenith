# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box 2 COMPLETE (all 6 sub-commits SC1-SC6). SC6 adds `ZM_CatchCalc` (Gen-III/IV four-shake capture + wild flee odds) and the engine pre-move SWITCH/ITEM(catch)/RUN actions, and promotes the smoke to the 2,000-battle soak. Next: S2 box 3 (abilities via per-hook fn-pointer structs + weather rain/sun/sand/snow).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Regen check GREEN (new `Source/Battle/ZM_CatchCalc.{h,cpp}` are in the solution).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit (T0): **1400 ran, 1399 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 102 `ZM_Data` + **228 `ZM_Battle`** (14 box-1 + 5 SC1 + 43 SC2 + 45 SC3 + 48 SC4 + 52 SC5 + 21 SC6) + 1068 engine + 2 boot. **Baseline 1400** in `zm-tests.yml`.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.

## What landed this session -- S2 box-2 SC6 (box 2 complete)

First landed the previously-uncommitted SC5 (all 10 volatiles + Endure/Swagger/forced-switch + `DoSwitch`) that a prior session authored but never committed -- verified its gate green and pushed it as `34adcb62`. Then SC6: `ZM_CatchCalc` (new files) implements the Gen-III/IV four-shake capture (rarity base rate + Gen-IV status bonus + ball param, integer-exact, PRIMEORB = guaranteed sentinel) and the paired wild-flee odds. `ZM_BattleEngine::ResolvePreMovePhase` now resolves voluntary SWITCH / ITEM(catch) / RUN before any move, in fixed PLAYER-then-ENEMY order; a capture/flee ends the battle (winner = catcher / none) by closing the turn directly, a trapped or illegal switch reports `MOVE_FAILED`, and the move phase draws its speed tie-break only when both sides submit MOVE (so move-only turns stay byte-identical). The 50-battle smoke is promoted to the deterministic **2,000-battle soak** (half wild, periodic catch/run). 21 SC6 tests + the soak, all derived against an independent PCG32 oracle. Decision = **ZM-D-035**.

## Current task

**S2 box 2 is DONE.** Next per [Roadmap.md](Roadmap.md) S2: **box 3 -- abilities via per-hook fn-pointer structs (~50 shipped) + weather (rain/sun/sand/snow)**. Box 3 owns the weather DAMAGE multiplier, the end-of-turn weather chip, weather/screen countdown + expiry, and the `WEATHER_*`/`SCREEN_*`/`ABILITY_TRIGGER` events (all reserved + declared; box 2 only SET field/screen state). **S2 has no visual gate**, so work continues autonomously.

## Notes for the next agent

- **DLL-heal gotcha (paid for this session):** Zenithmon's FIRST build in a given config leaves the middleware DLLs (`assimp-vc143-mtd`, `zlibd1`, `minizip`, `pugixml`, `poly2tri`) absent, so the exe dies at load with `STATUS_DLL_NOT_FOUND` (exit `0xC0000135`, EMPTY boot log) and `run_unit_gate.ps1` reports "no 'Unit tests complete' line". Fix: `Import-Module Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>` (copies from `Middleware/slang/bin` + sibling game outputs). `zenith test` heals automatically; the bare unit gate does NOT.
- **Invalid-volatile-state gotcha:** setting a volatile BIT in a test without its counter (e.g. `ZM_VOLATILE_TRAP` without `m_uTrapTurns`) trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (no "Unit tests complete" line -> looks like the DLL crash). Set the bit AND its counter together.
- **Catch/flee determinism:** offline oracle `scratchpad/zm_catch_ref.py` (independent PCG32 + integer math) derives every expected `a`/`b`/shake/flee value. `RandBelow(65536) == Next() % 65536` (rejection threshold is 0); guaranteed catch/flee draw NOTHING; the catch is the FIRST gameplay draw after `Begin` (Begin seeds but never draws).
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now **1400**).
- **Box-2 LOCKED contracts (ZM-D-032/033/034/035):** the `ZM_BattleEvent` POD + per-hit emit order, the augmented RNG draw order (PHASE G/M/E), the catch/flee formulas + guaranteed-ball sentinel, the pre-move PLAYER-then-ENEMY side order, and the catch/flee winner semantics + CATCH/FLEE event encodings. Later boxes APPEND event kinds/fields (default 0) so box-1..2 goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`).
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
