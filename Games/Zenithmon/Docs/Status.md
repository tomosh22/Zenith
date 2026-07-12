# Zenithmon Status

**Last updated:** 2026-07-12 -- **S2 box 4 (`ZM_ExpAndLevel`) is COMPLETE and locally GREEN.** Deterministic EXP curves, modern party-share awards, EV accumulation/caps, current cumulative EXP, mid-battle level/stat/move learning, and terminal level-evolution settlement are implemented under ZM-D-043. Next: `ZM_BattleAI` tiers RANDOM / GREEDY / SMART / CHAMPION.

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. Box 4's progression contract is **ZM-D-043** in [DecisionLog.md](DecisionLog.md); box 3 remains ZM-D-036/039/040/041/042.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Orchestrated-session model (this session)

Development is running under [OrchestratorPlaybook.md](OrchestratorPlaybook.md): the primary Claude instance is the ORCHESTRATOR -- it dispatches subagents for spec-extraction / code / test / review authoring but **owns ALL builds/tests/runs itself** (subagents never build; their "it compiles" claims are worthless), and it **alone writes the living docs** and owns the direct-master commit/push. SC3, SC4, and SC5 all landed this way. Per-SC pattern that worked well: (1) a spec-extraction subagent reads the design archive + `ZM_AbilityData` masks + the seams and writes a complete self-contained `<sc>_spec.md` to the orchestrator's scratchpad; (2) an Implementer and a Test Author author in PARALLEL on DISJOINT files from that one spec; (3) an adversarial Reviewer traces the diff; (4) the orchestrator builds, runs the unit gate + headless, applies doc updates, commits + pushes. The adversarial review caught a real test-fixture bug in SC5 (see below) -- keep the review step.

## Build / Tests (all GREEN this iteration)

- Regen GREEN (required for two new production files and one new test TU).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit boot gate: **1577 ran, 1576 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). Box 4 added **67** tests (45 `ZM_Data` + 22 `ZM_Battle`); workflow baseline bumped **1510 -> 1577**.
- Automated P1: **1/1** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- No windowed or visual check applies to this headless box.
- Adversarial production and test re-reviews GREEN: party sharing, current EXP, faint-source ordering, exact-once credit, terminal evolution, zero-perturbation, and independent literal coverage all pass review.

## Completed locally this iteration -- S2 box 4 (`ZM_ExpAndLevel`)

`Source/Battle/ZM_ExpAndLevel.{h,cpp}` supplies four integer curves and their inverse, derived growth/base-EXP/EV/evolution accessors, capped EV helpers, current-EXP level mutation, move learning, and pure one-edge evolution. `ZM_BattleEngine` adds default-off, side-mask-configurable progression: each defeated opponent tracks its own participants; living participants receive full gross EXP, living nonparticipants half, and every living recipient receives full EV yield. Direct and end-of-turn faint sources award exactly once. EXP/LEVEL/MOVE events occur mid-battle; one level-evolution edge queues only at terminal settlement. Full contract: ZM-D-043.

## Current task

**Next per [Roadmap.md](Roadmap.md) S2: `ZM_BattleAI` tiers RANDOM / GREEDY / SMART / CHAMPION**, a pure function of battle state + `ZM_BattleRNG`. Start with a Planner/spec extraction pass that pins legal-action enumeration, expected-damage scoring, SMART switching/healing rules, CHAMPION two-ply state cloning, and the TestPlan property fixtures; then use disjoint Implementer/Test Author scopes, adversarial review, and the orchestrator-owned serial gate.

After `ZM_BattleAI`, S2 has `ZM_Breeding` / `ZM_Daycare` / `ZM_BattleTower`, then the automated S2 gate. Roadmap S2 lists no windowed or visual check, so no `GATE-WAIT` is expected for this box or stage unless the binding gate docs change.

## Notes for the next agent

- **Box 4 is DONE:** ZM-D-043 locks default-off, player-default progression; per-opponent participation; full participant/half nonparticipant EXP; full EV yield; current cumulative EXP; exact-once direct/EOT settlement; and terminal one-edge level evolution.
- **Box 3 is DONE:** all 50 ability rows realize their declared `m_uHookMask` (live pfns + documented engine-side exceptions: the 12 weather-coupled WEATHER-as-condition, and QUICKDRAW/DAUNTINGROAR/BLOODRUSH MODIFY_STAT engine-side). QUICKDRAW(46) is the sole all-null-pfn row. The all-50 coverage gate lives in `ZM_Tests_Abilities.cpp` (`Abilities_HookTableAll50*`) -- if you touch the ability table, that gate is your guardrail. The box-3 design archive (all 50 bodies) was at `C:\Users\tomos\AppData\Local\Temp\claude\c--dev-Zenith\6a05a4f2-a8e4-41e0-bb7e-b300ad0dc3ce\scratchpad\zm_s2_box3_design.md` (session-temp, may be gone); the SC3/SC4/SC5 orchestrator specs are in this session's scratchpad (`sc3`/`sc4`/`sc5_spec.md`).
- **Open follow-up (Q-2026-07-12-001):** `ZM_ValidateEventStream` rule 7 doesn't recognize `WEATHER_DAMAGE`->`FAINT` or ability-chip(`ABILITY_TRIGGER`)->`FAINT` as valid pre-FAINT damage sources -- a PRE-EXISTING test-helper gap. Full-battle tests with weather-chip or ability-chip KOs must bypass the validator until it's widened (a small standalone commit). Non-blocking.
- **Test-coverage gate is a hard merge bar (user mandate):** every new system/method gets dedicated behavioral tests (for stochastic battle logic, paired identical-seed positive+control), not just a smoke test. The adversarial review is what catches gaps + false-confidence tests -- keep it for logic-heavy work.
- **BUMP the zm-tests baseline** in the SAME commit whenever `ZM_*` unit tests change (now **1577**, in `.github/workflows/zm-tests.yml`; exact-match gate).
- **DLL-heal gotcha:** Zenithmon's FIRST build in a fresh config leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, no "Unit tests complete" line). Fix: `Import-Module Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>`. `zenith test` heals automatically; the bare unit gate does NOT. (Config was warm all session.)
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (looks like the DLL crash). Set the bit AND its counter together.
- **LOCKED battle contracts (ZM-D-032..043):** `ZM_BattleEvent` POD is append-only (new kinds append before COUNT, new fields default 0); RNG draw order fixed; inactive mechanics draw/emits/mutate ZERO; EoT order remains weather -> countdown -> PLAYER-then-ENEMY status -> ability -> progression sweep -> TURN_END. When progression is enabled, recipient order and terminal evolution order are locked by ZM-D-043. New systems must APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrator invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author on DISJOINT file scopes and never claim "builds clean"; the orchestrator alone writes the living docs and owns the direct-master commit/push; parallel authoring is fine on non-overlapping files, but integrate + build SERIALLY.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
