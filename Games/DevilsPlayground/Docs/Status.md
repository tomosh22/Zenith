# DP Status

**Last updated:** 2026-05-12 after round-2 peer-review reconciliation; MVP-0.1.1 landed out-of-sequence in PR #3.
**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True`, 0 warnings, 0 errors) as of PR #3 (commit e2b10e3a).
**Tests:** 34/35 passing as of PR #3 (Test_P1Tuning_LoadsAndValuesInBand green). The single fail is `HumanPlaythrough_Test`, a pre-existing skeleton-state issue (`m_iMaxFrames=6000` vs runner default `--exit-after-frames 600`).

## Manual setup checklist gating

**⚠️ BEFORE STARTING THE FIRST AGENT SESSION,** the user (Tomos) must tick every box in [Docs/ManualSetupChecklist.md](ManualSetupChecklist.md). The orchestrator pattern depends on human-only setup (branch protection rules, Vulkan SDK install, pwsh.exe install, etc.) that no agent can perform. The orchestrator's session-start ritual checks this file and STOPS if any box is unchecked.

## Current task

**MVP-0.0.6** — Repo-protect master. Scope reduced: `dp-build` only (NOT `dp-tests`; the latter is the skeleton from MVP-0.0.3, blocked on Q-2026-05-12-007). HUMAN_GATE if the gh token lacks `admin:repo_hook`. Document in `Docs/CIPolicy.md`.

**Phase 0.0 progress:** MVP-0.0.1, MVP-0.0.2, MVP-0.0.3-skeleton, MVP-0.0.4, MVP-0.0.5 all shipped. Remaining: MVP-0.0.6 (branch protection, scope-reduced) and MVP-0.0.7 (smoke PR for the `dp-build` half of the loop). **MVP-0.0.3 reactivation as a real gate** is still blocked on Q-2026-05-12-007 (GPU on CI).

**MVP-0.1.1 (DP_Tuning) is DONE** out-of-sequence in PR #3 (commit e2b10e3a). See [DecisionLog.md 2026-05-12 (PR #3 out-of-sequence)](DecisionLog.md).

## Ownership

**Per user direction 2026-05-12: engine work is in-scope for autonomous orchestrator + subagents.** No parallel human work track. The orchestrator drives both game-side tasks (`Games/DevilsPlayground/...`) and engine-side tasks (`Zenith/...`) sequentially. Mitigations on engine PRs:

- **Mandatory Reviewer subagent** on every engine PR (per OrchestratorPlaybook §5.4).
- **Full test suite + Combat smoke build** on every engine PR (catches cross-game regressions from shared `Zenith/` changes).
- **Design rationale in `DecisionLog.md` before the PR** for any net-new engine namespace or non-trivial algorithm choice.

Tomos's role remains: tick `ManualSetupChecklist.md` boxes once before the first session; review high-stakes design rationale via `DecisionLog.md`; final eye-test on S2 art when it arrives.

## Last completed

- **MVP-0.0.5** (this PR) — Installed `pwsh.exe` 7.6.1 via `winget install Microsoft.PowerShell` (no admin elevation required for current-user install). `verify_build_env.ps1` now reports 8/8 PASS, 0 warnings. Also fixed the long-standing slang DLL post-build copy gap (CLAUDE.md note): changed the Sharpmake-emitted xcopy from `slang.dll` to `*.dll` across all 4 game/tool Sharpmake configs (Games, FluxCompiler, TilePuzzleLevelGen, TilePuzzleRegistryViewer). Resolves the "copy DLLs from Combat output to DP output if you see DLL_NOT_FOUND" workaround documented in CLAUDE.md.
- **MVP-0.0.4** (PR #8) — Extended `Tools/run_dp_tests.ps1` with `-Tier <0..4>`, `-FailFast`, `-AssertionsLog <path>` flags. Tier filters by `Test_T0` / `Test_P<N>` prefix per TestPlan naming. FailFast forces per-process mode (batch mode runs all regardless). AssertionsLog appends a one-line FAIL summary per failure. **Also fixed Q-2026-05-12-005**: replaced em-dashes (and any other non-ASCII characters) so the script parses cleanly under Windows PowerShell 5.1 (no more `pwsh.exe`-mandatory). Added `Tools/Test_T0Harness_RunnerFlagsExist.ps1` as a smoke test that proves the runner declares all three flags.
- **MVP-0.0.3 skeleton** (PR #7) — `.github/workflows/dp-tests.yml` landed as a `workflow_dispatch`-only skeleton. Workflow body works through Build step (verified iteration 2): build green, vcpkg + Vulkan SDK + Slang v2026.1 + OpenCV 4.10.0 DLL provisioning all green. **The exe itself deadlocks** at `--list-automated-tests` on a GPU-less runner. Trigger disabled to avoid burning CI minutes; reactivation gated on Q-2026-05-12-007. Companion DecisionLog entry explains the option-A choice.
- **MVP-0.0.2** (PR #5) — `.github/workflows/dp-pr.yml`. Builds the DevilsPlayground target on every PR / push-to-master using `vs2022_Debug_Win64_True`. Check name `dp-build`. Runs alongside the (now-renamed) `complexity.yml` (the old `msbuild.yml`, see PR #6). Five CI iterations to get green.
- **MVP-0.0.1** (PR #4) — `Tools/verify_build_env.ps1`. Audits the eight prerequisites from BuildEnvironment.md section 1 (VS 2022 + C++ workload, Windows SDK >= 22000, Vulkan SDK, .NET 6+ runtime/SDK, PowerShell, Git 2.30+, gh CLI authenticated, repo master+clean). Exits 0 on green, 1 on any required failure. Accepts `-SkipRepoState` for use during feature-branch sessions, and `-WarningsAreErrors` for CI-grade strictness. Run locally on the wizardly-payne worktree: 7 PASS / 1 WARN (`pwsh.exe` not installed — Q-2026-05-12-005) / 0 FAIL with `-SkipRepoState`; FAILs on `branch!=master / dirty` without the flag, as designed.
- **MVP-0.1.1** (out of sequence, PR #3 e2b10e3a, 2026-05-12) — `DP_Tuning` system + `Test_P1Tuning_LoadsAndValuesInBand`. The orchestrator that ran was working from the pre-reconciliation roadmap; it jumped straight to MVP-0.1.1 instead of starting at MVP-0.0.1. The work itself is correct and matches the round-3 reconciled spec for MVP-0.1.1 (single PR bundling API + test + implementation). The Phase 0.0 bootstrap tasks remain due before further coding-style PRs land. See [DecisionLog.md 2026-05-12 entries](DecisionLog.md).
- **Round-2 peer-review reconciliation** (staged 2026-05-12, committed alongside this status update) — introduced Phase 0.0 Bootstrap, ManualSetupChecklist.md, BuildEnvironment.md, Glossary.md, PeerReviews/, expanded DecisionLog/MvpRoadmap/OrchestratorPlaybook, dropped InputsForAutonomy.md.
- **Initial-planning session** (2026-05-11) — six design documents + three config files landed: [GameDesignDocument.md](GameDesignDocument.md), [Shortfalls.md](Shortfalls.md), [TestPlan.md](TestPlan.md), [AssetManifest.md](AssetManifest.md), [AssetTestPlan.md](AssetTestPlan.md), [MVPScope.md](MVPScope.md), [MvpRoadmap.md](MvpRoadmap.md), [AgentBriefing.md](AgentBriefing.md), [Config/Tuning.json](../Config/Tuning.json), [Config/Archetypes.json](../Config/Archetypes.json), [Config/Reagents.json](../Config/Reagents.json). User signed off on the GDD framing and gave execution mandate.

## Notes for next agent

1. **Operating mode for fresh sessions:** orchestrator + subagents (Mode B). Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) before doing anything else if you're the orchestrator of a fresh user-initiated session. The user explicitly chose this on 2026-05-11 and explicitly forbade git worktrees.
2. **Hard invariants:** (a) only the orchestrator invokes MSBuild / Tools/run_dp_tests.ps1 / the game executable; (b) no worktrees, all work on `master` with per-task feature branches; (c) only the orchestrator writes Status.md / Questions.md / DecisionLog.md / MvpRoadmap.md / Config/*.json.
3. **First action:** verify the baseline. Acquire build lock per OrchestratorPlaybook §4. Build the prototype (`vs2022_Debug_Win64_True`, target DevilsPlayground), run the existing 35 tests (PR #3 added one). Expect 34/35 green — `HumanPlaythrough_Test` is the persistent pre-existing fail (Questions.md Q-2026-05-12-002). If any OTHER test goes red, fix before starting MVP-0.0.1.
4. **Then** start MVP-0.0.4 (runner script flags). MVP-0.0.3 landed as a skeleton; reactivation is blocked on Q-2026-05-12-007 (GPU on CI). MVP-0.0.6 (branch protection) cannot land until that's resolved. MVP-0.0.5 (pwsh.exe install gate) and MVP-0.0.7 (smoke PR) remain workable. MVP-0.1.2 onwards stays gated on Phase 0.0 being complete-or-officially-deferred.
5. **Branching:** all work on `master`. Branch as `dp/mvp-<task-id>`. The worktree at `.claude/worktrees/flamboyant-mirzakhani-9186f1/` is irrelevant to DP.
6. **No external spend** unless you find a paid blocker (and even then, log it in Questions.md and pick a different task to unblock yourself).
