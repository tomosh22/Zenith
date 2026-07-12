# Zenithmon Status

**Last updated:** 2026-07-12 -- **S2 box 5 (`ZM_BattleAI`) is COMPLETE, committed, and pushed to master.** A pure, side-effect-free four-tier opponent AI (`ZM_ChooseAction`: RANDOM / GREEDY / SMART / CHAMPION) that reads battle state `const`, draws only its own caller-supplied RNG (RANDOM tier only), and perturbs no battle RNG/state/events -- box-1..4 goldens stay byte-identical. Contract: **ZM-D-044**. Next: S2 box 6 -- `ZM_Breeding` + `ZM_Daycare` + `ZM_BattleTower` logic.

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. Box 5's AI contract is **ZM-D-044**; box 4 is ZM-D-043; box 3 is ZM-D-036/039/040/041/042.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Orchestrated-session model (this session)

Development is running under [OrchestratorPlaybook.md](OrchestratorPlaybook.md): the primary Claude instance is the ORCHESTRATOR -- it dispatches subagents for spec-extraction / code / test / review authoring but **owns ALL builds/tests/runs itself** (subagents never build; their "it compiles" claims are worthless), and it **alone writes the living docs** and owns the direct-master commit/push. Box 4 and box 5 both landed this way. Per-box pattern that keeps working: (1) a spec-extraction subagent reads the seams + design docs and writes a complete self-contained `<box>_spec.md` to the orchestrator's scratchpad, returning a tight digest; (2) an Implementer and a Test Author author in PARALLEL on DISJOINT files from that one spec (the pinned public API in the spec keeps the parallel seam consistent); (3) an adversarial Reviewer traces the diff; (4) the orchestrator builds, runs the unit gate + headless, applies doc updates, commits + pushes. The adversarial review earns its keep -- on box 5 it flagged a CHAMPION reply-model false-confidence gap (all test opponents had one move), which was closed with +2 discriminating tests BEFORE commit.

## Build / Tests (all GREEN this iteration)

- Regen GREEN (required for two new production files `ZM_BattleAI.{h,cpp}` + one new test TU `ZM_Tests_BattleAI.cpp`).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit boot gate: **1605 ran, 1604 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). Box 5 added **28** `ZM_Battle` tests; workflow baseline bumped **1577 -> 1605**.
- Automated P1: **1/1** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- No windowed or visual check applies to this headless box.
- Adversarial review GREEN (SAFE-TO-COMMIT): non-perturbation (const-in-state, no clone, battle RNG never touched), GREEDY/SMART/CHAMPION correctness, and genuine test discrimination all verified. The reviewer's 4 coverage gaps (CHAMPION reply-model, SMART type premises, CHAMPION exact-speed-tie, a comment nit) were CLOSED with +2 tests + self-guarding asserts before commit.

## Completed locally this iteration -- S2 box 5 (`ZM_BattleAI`)

`Source/Battle/ZM_BattleAI.{h,cpp}` supplies `ZM_AI_TIER` (RANDOM/GREEDY/SMART/CHAMPION) and one pure chooser `ZM_ChooseAction(const ZM_BattleState&, ZM_SIDE, ZM_AI_TIER, ZM_BattleRNG&)`. It enumerates the legal set (moves-with-PP, then SWITCH-to-living-bench, deterministic order), reads state `const`, and draws only its own `xAIRng` (RANDOM only). GREEDY = argmax deterministic expected-damage (roll 92, no crit, STAB + type effectiveness) x accuracy, lowest-slot tie-break. SMART = KO -> hopeless-switch -> heal -> GREEDY cascade with pinned thresholds. CHAMPION = deterministic 2-ply scalar-HP lookahead with a modeled GREEDY opponent reply, priority/effective-speed ordering (exact tie = opponent-first), eval `V = ±100000·faint + hpMe − hpOpp`. Never calls Submit/Resolve/DoSwitch/MoveExecutor, emits no events, mutates nothing -- box-1..4 goldens + RNG byte-identical. Full contract: ZM-D-044.

## Current task

**Next per [Roadmap.md](Roadmap.md) S2: box 6 -- `ZM_Breeding` + `ZM_Daycare` logic; `ZM_BattleTower` logic (level-50 clamp, rentals, streak scaling, AI escalation).** `ZM_BattleTower` can consume `ZM_BattleAI` (its AI-escalation tiers map to `ZM_AI_TIER`). Suggested approach: a spec-extraction pass pinning the breeding inheritance rules (IV/nature/egg-move/ability inheritance vs the documented cuts), the daycare level/step model, and the Battle Tower rental/streak/AI-escalation schedule against Scope.md + the GDD; then disjoint Implementer/Test Author scopes, adversarial review, and the orchestrator-owned serial gate.

After box 6, S2 has its automated stage gate (~370+ unit tests incl. seeded scenario battles with exact expected event streams + a 2,000-battle fuzz soak). Roadmap S2 lists no windowed or visual check, so no `GATE-WAIT` is expected for box 6 or the S2 gate unless the binding gate docs change.

## Notes for the next agent

- **Box 5 is DONE + PUSHED:** ZM-D-044 locks the pure, standalone, non-perturbing four-tier chooser. The engine does NOT yet call it -- wiring the opponent side to `ZM_ChooseAction` is a later change (battle integration, S5+). Three in-scope rulings are logged in Q-2026-07-12-003 (file location `Source/Battle/` vs MasterPlan `Source/AI/`; no Struggle fallback; S11-tunable SMART thresholds + rolls) -- best-guessed and proceeded; all additive if overridden.
- **Box 4 is DONE:** ZM-D-043 locks default-off, player-default progression; per-opponent participation; full participant/half nonparticipant EXP; full EV yield; current cumulative EXP; exact-once direct/EOT settlement; terminal one-edge level evolution.
- **Box 3 is DONE:** all 50 ability rows realize their declared `m_uHookMask`; the all-50 coverage gate lives in `ZM_Tests_Abilities.cpp` (`Abilities_HookTableAll50*`).
- **Open follow-up (Q-2026-07-12-001):** `ZM_ValidateEventStream` rule 7 doesn't recognize `WEATHER_DAMAGE`->`FAINT` or ability-chip(`ABILITY_TRIGGER`)->`FAINT` as valid pre-FAINT damage sources -- a PRE-EXISTING test-helper gap. Full-battle tests with weather-chip or ability-chip KOs must bypass the validator until it's widened (a small standalone commit). Non-blocking; a good small task before the content-heavy stages.
- **Test-coverage gate is a hard merge bar (user mandate):** every new system/method gets dedicated behavioral tests (for stochastic logic, paired identical-seed positive+control), not just a smoke test. The adversarial review is what catches gaps + false-confidence tests -- keep it for logic-heavy work (it caught the box-5 CHAMPION reply-model gap).
- **BUMP the zm-tests baseline** in the SAME commit whenever `ZM_*` unit tests change (now **1605**, in `.github/workflows/zm-tests.yml`; exact-match gate).
- **DLL-heal gotcha:** Zenithmon's FIRST build/run in a fresh config (or a fresh session) leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, no "Unit tests complete" line -- looks like a crash). `zenith test` heals automatically; the BARE unit gate does NOT. Fix: run `zenith test ... --headless` once first (it heals), OR `Import-Module <abs>\Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>` (use the ABSOLUTE module path -- a bare relative `Build\...` fails to resolve).
- **Unit-gate timeout is normal:** the gate boots the exe, logs "Unit tests complete: N ran...", then watchdog-kills the windowed idle after 180s. `PASS`/exit 0 with the units line present = green; the "timeout after 180s; killing" line is expected, not a failure.
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (looks like the DLL crash). Set the bit AND its counter together.
- **LOCKED battle contracts (ZM-D-032..044):** `ZM_BattleEvent` POD is append-only (new kinds append before COUNT, new fields default 0); RNG draw order fixed; inactive mechanics draw/emit/mutate ZERO; EoT order fixed; progression recipient order + terminal evolution order locked (ZM-D-043); `ZM_BattleAI` is a pure non-perturbing chooser (ZM-D-044). New systems must APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrator invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author on DISJOINT file scopes and never claim "builds clean"; the orchestrator alone writes the living docs and owns the direct-master commit/push; parallel authoring is fine on non-overlapping files, but integrate + build SERIALLY.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
