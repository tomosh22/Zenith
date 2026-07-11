# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box 3 progressed to SC3/5 DONE. SC3 (20 damage-dealt/taken + modify-stat + type-interaction/absorb abilities) is COMPLETE and GREEN, landing in this commit with its full behavioral coverage. A previous iteration authored SC3 production + partial tests and left it uncommitted; this iteration finished it -- the Reviewer found the production 20/20 correct (no bugs) but 10 of 20 abilities lacked behavioral tests, so 10 were authored (all passing) and the dead absorb-test scaffold was wired up before landing. Next: S2 box 3 SC4 (status-try + contact + stat-drop-veto + accuracy abilities).**

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. The box-3 execution plan + design rulings are **ZM-D-036** (with SC2 = ZM-D-039, SC3 = **ZM-D-040**) in [DecisionLog.md](DecisionLog.md).

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests (all GREEN this iteration)

- Regen: not needed this iteration (SC3 edits existing files only; no new TUs).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit (boot gate): **1476 ran, 1475 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). SC3 added **30** `ZM_*` unit tests (1446 -> 1476). **Baseline 1476** in `zm-tests.yml` (bumped this commit).
- Automated (P1): **1/1** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- Reviews GREEN: independent production review (20/20 abilities correct, no bugs; flee unaffected by ability speed; damage order + absorb/immunity + zero-RNG contract all verified) + a test-coverage review that caught the missing-behavioral-test gap, which was then closed.

## What landed this session -- S2 box-3 SC3 (damage / stat / type-interaction abilities)

SC3 installs the next **20** ability rows as live hook function-pointers, reusing SC2's `ZM_AbilityHooks` aggregate, `ZM_AbilityContext`, and 50-row table unchanged (no infra churn). The four surges (VERDANTSURGE/EMBERSURGE/TIDALSURGE/HIVESURGE) boost own-type damage dealt x3/2 at <=1/3 HP; SKYWARDGRACE/AQUIFER/DYNAMO/GRAZER give type-immunity/absorb (heal maxHP/4); CINDERDRINK both absorbs fire and boosts own fire x3/2; BEDROCK/BLUBBER/SOLIDCORE/HEAVYPLATE/GOSSAMER/DOWNDRAFT reduce damage taken; SUNCHASER/STREAMLINE/GRITSTRIDE/RIMESTRIDE/FERVOR modify a stat under a weather/status condition. Dispatch is at the existing stat/damage/immunity seams in `ZM_MoveExecutor.cpp` (ability mods apply AFTER `ZM_CalcDamage`, dealt-then-taken, >=1 floor; type-interaction resolves on the defender before the chart-immunity gate) and the move-order-ONLY speed seam in `ZM_BattleEngine.cpp` (`g_EffectiveSpeedForMoveOrder`; flee keeps raw `g_EffectiveSpeed`). No RNG draw is added; the NONE-ability stream stays byte-identical; the `ZM_BattleEvent` POD, RNG order, and EoT order are untouched. 30 tests lock it: the SC3 mask-realization + installation-state invariants plus 20 paired identical-seed behavioral tests (positive + control). Full detail in ZM-D-040.

## Current task

**Next per [Roadmap.md](Roadmap.md) S2 box 3: SC4 -- status-try + contact + stat-drop-veto + accuracy abilities.** Reuse the SC2/SC3 hook/context/table; add dispatch at the status-application, contact, and accuracy seams. The SC4 roster (from ZM-D-036 + the design archive) is:

- **STATUS_TRY (`pfnPreventMajor` / `pfnPreventVolatile`):** WAKEFUL (block SLEEP), PUREBLOOD (block POISON/TOXIC), THAWHEART (block FREEZE), LIMBERLITHE (block PARALYSIS), COLDBLOOD (major), OWNPACE (volatile). **On a successful block emit `ABILITY_TRIGGER` (blocked status in `m_iAmount`) and NO `STATUS_APPLIED`** (ZM-D-036 §4.5).
- **CONTACT (`pfnOnContact`):** STATICVEIL (30% paralyze attacker), CINDERSKIN (30% burn), BARBSKIN (30% poison), THORNMAIL (no draw: chip attacker maxHP/8 min 1 + `ABILITY_TRIGGER` [+FAINT]), LASTSPITE, AFTERSHOCK. Contact proc chance = **30%**.
- **Stat-drop-veto / ACCURACY (`pfnPreventStatDrop` / `pfnBypassAccuracy`):** IRONWILL (veto any foe-sourced stage DROP), KEENEYE (veto ACCURACY drop only), DEADAIM + TRUESHOT (`pfnBypassAccuracy` -> never miss). Veto uses `bFromFoe = (eTgt != xCtx.m_eAtk)` so box-2 stat goldens stay equal (no ability => no veto).
- **FAINT (`pfnOnDealtFaint` / `pfnOnContact` self-KO'd branch):** BLOODRUSH (self downs foe), LASTSPITE/AFTERSHOCK (self KO'd by contact).

These are edits to existing files -- no regen unless a new TU is genuinely added. The Roadmap box-3 line stays UNCHECKED until SC5. SC4 EXTENDS the installed set; the all-50 complete-realization gate belongs to **SC5** (which also adds turn-end/faint/quickdraw abilities + reuses the `DoSwitch` primitive).

## Notes for the next agent

- **Design archive (bonus, may be transient):** the full box-3 design (all 50 ability bodies + constants + per-SC test lists) was authored at `C:\Users\tomos\AppData\Local\Temp\claude\c--dev-Zenith\6a05a4f2-a8e4-41e0-bb7e-b300ad0dc3ce\scratchpad\zm_s2_box3_design.md`. It's a session-temp file and may be gone; the AUTHORITATIVE sources are ZM-D-036/039/040 + the Roadmap S2 line + this file, and per-SC detail re-derives from `ZM_AbilityData.{h,cpp}` (the 50-row table + `m_uHookMask`: SC4 = the STATUS_TRY/CONTACT/stat-veto/ACCURACY-bit rows not yet installed) + `GameDesignDocument.md`.
- **Coverage invariant (staged through SC5):** every installed ability row (SC1 weather + SC2 six switch-ins + SC3 twenty) completely realizes its declared `m_uHookMask` via live pfns and documented engine-side exceptions; every live slot maps to a declared bit; every not-yet-installed slot stays null. SC4 extends the installed set; the all-50 complete-realization check lands at SC5. The 12 weather-coupled abilities realize WEATHER as a condition read (not a pfn); QUICKDRAW/DAUNTINGROAR/BLOODRUSH realize MODIFY_STAT engine-side.
- **Test-coverage gate is a hard merge bar (user mandate):** every new ability gets a behavioral test (positive + paired identical-seed control), not just a mask-realization assertion. This iteration a whole SC needed its behavioral half authored before it could land -- do not commit an SC whose abilities lack behavioral tests.
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now **1476**, in `.github/workflows/zm-tests.yml`).
- **DLL-heal gotcha:** Zenithmon's FIRST build in a fresh config leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, "no 'Unit tests complete' line"). Fix: `Import-Module Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>`. `zenith test` heals automatically; the bare unit gate does NOT. (Config was warm this iteration.)
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (looks like the DLL crash). Set the bit AND its counter together.
- **Box-2/3 LOCKED contracts (ZM-D-032..036, 039, 040):** `ZM_BattleEvent` POD is append-only (new kinds append before COUNT, new fields default 0); RNG draw order fixed (PHASE G/M/E); a NONE-ability/weather-NONE/status-free actor draws ZERO and emits ZERO; EoT order = weather chip FIRST -> weather/screen countdown -> PLAYER-then-ENEMY status ticks -> (SC5) ability TURN_END heals (skip fainted) -> TURN_END. Later SCs APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrated-session invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author code/tests only and never claim "builds clean"; the orchestrator alone writes Status/Roadmap/DecisionLog/Questions/Shortfalls and owns the direct-master commit/push.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
