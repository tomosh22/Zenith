# Zenithmon -- Session Start Prompts

**Document purpose:** Copy-paste prompts to start Claude Code sessions on
Zenithmon. Pick the one matching your intent; paste verbatim into a fresh
session opened at cwd `C:\dev\Zenith`. **Prompt 0 is the whole-lifecycle
driver** -- the only human input the project needs between visual gates.

Every prompt anchors on the same reading order: `Docs/Status.md` (current
state) -> `Docs/Roadmap.md` (what's next) -> `Docs/AgentBriefing.md` (how to
work) -- with **`Docs/MasterPlan.md` as the full approved program plan** (the
engine-fact survey, per-stage detail, design rationale, and risks behind the
Roadmap's terse checklists). Consult MasterPlan whenever a Roadmap line is not
self-explanatory; Scope.md stays binding; DecisionLog.md before re-deciding.

---

## 0. Lifecycle loop (the default -- drives the project start to finish)

When to use: any time nothing needs a human. Fire it as `/loop` (self-paced)
with the prompt below, or paste it repeatedly -- **each firing is one
idempotent iteration** that trusts Status.md + git state, so re-firing is
always safe. The loop hard-stops only at visual gates (your standing order)
and on unresolvable blockers.

```
You are the autonomous development loop for Zenithmon (Games/Zenithmon), executing one iteration. Read in order: Games/Zenithmon/Docs/Status.md, Games/Zenithmon/Docs/Roadmap.md, Games/Zenithmon/Docs/AgentBriefing.md. Games/Zenithmon/Docs/MasterPlan.md is the approved program plan -- consult it for the detail behind any Roadmap line. Scope.md is binding; grep DecisionLog.md before re-deciding anything.

ITERATION PROTOCOL (idempotent -- always derive state, never assume):
1. If Status.md contains a "GATE-WAIT" marker: check DecisionLog.md for a matching sign-off entry. If none exists, report "waiting for S<n> visual sign-off" with the evidence paths and END this iteration -- do not proceed past a gate the user has not signed off. (The user unblocks with StartPrompts.md prompt 4.)
2. Else if git state shows an in-flight feature branch (zenithmon/s<stage>-<slug>) with unmerged work: resume it -- drive it through build, tests, PR, CI, merge, docs.
3. Else: verify master is green (git pull; zenith test Zenithmon --headless as the baseline), then pick the FIRST unchecked task in the current stage of Roadmap.md and execute it end-to-end.

EXECUTION RULES: test-first per Docs/TestPlan.md (tests land WITH the system, same PR); feature branch zenithmon/s<stage>-<slug>; build via `pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon` (never the whole sln); `pwsh -NoProfile -File Build\regen.ps1` after adding files; serial MSBuild only (subagents never build); never commit baked assets; asset-dependent tests exists-guard + RequestSkip; ZM_ prefix; zero Nintendo IP; no audio / networking / Dynamax-analog; singles battles; data tables are compiled C arrays. Engine changes additionally need engine unit tests + a RenderTest boot regression check. VERIFY LOCALLY -- this is the authoritative quality bar for new behaviour (do NOT lean on CI to catch it): run ALL of (a) `pwsh -NoProfile -File Tools\zenith.ps1 test Zenithmon --headless` (automated suite), (b) the boot unit gate `pwsh -NoProfile -File Tools\run_unit_gate.ps1 -Exe <exe> -Baseline N` -- this actually runs the ZM_* unit tests, which `zenith test` SKIPS; when you add/remove ZM_* unit tests, bump N here AND in `.github/workflows/zm-tests.yml` in the SAME PR -- and (c) the stage's windowed --filter runs. All green before you push.

PR + MERGE (standing authorization; LOCAL-GATE-FIRST, do NOT wait on CI -- user policy 2026-07-10, DecisionLog ZM-D-028): the local gate above is the real verification; CI is a backstop, not something to sit and watch. Open the PR with Tools\zenith_gh.ps1 (self-bootstraps gh auth; e.g. `pwsh -NoProfile -File Tools\zenith_gh.ps1 pr create ...`), then IMMEDIATELY enable auto-merge: `pwsh -NoProfile -File Tools\zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch` (use `--rebase` instead of `--squash` to preserve a deliberate multi-commit PR). Auto-merge lands the PR unattended the moment the required `zm-tests` check passes -- do NOT idle-watch checks, do NOT wait for the slower discipline gates (dp-tests/cb-tests/engine-gate/etc.), and do NOT launch a blocking `pr checks --watch`. `--admin`/bypass merges are NOT permitted (the harness blocks them), so zm-tests must actually go green -- auto-merge is how you honour that without waiting. Never let the loop idle on CI: use the CI window to design + prototype the NEXT task (schema, golden vectors in a scratchpad Python model), then implement it off fresh master once the current PR has merged (`git pull`). If zm-tests goes red: fix forward on the same branch (a push re-triggers; NEVER `gh run rerun` -- it reuses the run's ORIGINAL merge commit; rebase onto master and push instead). After 3 failed fix-forward attempts, log the blocker in Questions.md, refresh Status.md, and end the iteration.

VISUAL GATES (hard-stop policy, user's standing order): when the next Roadmap item is a stage gate with a visual check (or S4's gallery / S8's go-no-go), run every AUTOMATED gate item, capture windowed screenshot evidence (AgentBriefing.md has the recipe), write the evidence paths into Status.md under a "GATE-WAIT: S<n> visual sign-off" marker, append the gate's automated results to DecisionLog.md, and END the iteration. Do not tick the gate or start the next stage.

DOCS (same PR as the change): tick Roadmap.md boxes only when merged AND green; append decisions to DecisionLog.md; log blockers in Questions.md with best-guess + cost-if-wrong and keep moving where possible; refresh Status.md at iteration end (it must let the NEXT firing resume cold). Kill any stray zenithmon.exe processes before ending (Get-Process zenithmon).

END OF ITERATION: report in one short paragraph what landed (PR #, merged or not), what is next, and whether the loop should keep firing (yes unless GATE-WAIT or an unresolvable blocker).
```

---

## 1. Fresh start (single supervised session)

When to use: normal supervised development with no in-flight task.

```
You are working on Zenithmon (Games/Zenithmon). Read these files in order before doing anything else:

1. Games/Zenithmon/Docs/Status.md -- current state.
2. Games/Zenithmon/Docs/Roadmap.md -- the S0-S12 stage plan; pick the FIRST un-checked task in the current stage.
3. Games/Zenithmon/Docs/AgentBriefing.md -- conventions and operating rules.

Games/Zenithmon/Docs/MasterPlan.md is the full approved program plan -- consult it for the detail and rationale behind any Roadmap line. Games/Zenithmon/Docs/Scope.md is binding -- do not add anything outside it. Grep Games/Zenithmon/Docs/DecisionLog.md before re-deciding anything already decided. If you will dispatch subagents, also read Games/Zenithmon/Docs/OrchestratorPlaybook.md and obey its invariants (only you build/test/run; subagents never build; no git worktrees; you alone write the living docs).

Execute the task test-first per Docs/TestPlan.md: tests land WITH the system, in the same PR. Work on a feature branch named zenithmon/s<stage>-<slug>. Build with `pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon` (never the whole sln). Verify LOCALLY -- the authoritative bar -- with BOTH `pwsh -NoProfile -File Tools\zenith.ps1 test Zenithmon --headless` AND the boot unit gate `pwsh -NoProfile -File Tools\run_unit_gate.ps1 -Exe <exe> -Baseline N` (it runs the ZM_* unit tests that `zenith test` skips; bump N here and in `.github/workflows/zm-tests.yml` in the same PR). Do NOT wait on CI: after opening the PR, enable auto-merge (`Tools\zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`) so it lands when the required `zm-tests` check passes; nothing merges red and `--admin`-bypass is blocked. Regenerate solutions with `pwsh -NoProfile -File Build\regen.ps1` after adding files. Use Tools\zenith_gh.ps1 for all gh calls (it self-bootstraps auth). Baked assets are git-ignored -- never commit them; asset-dependent tests RequestSkip when assets are absent.

Hard scope rules: ZM_ prefix for all game code; ~150 original species and original names everywhere (zero Nintendo IP -- mainline mechanics only); no audio; no networking/multiplayer/trading; no Dynamax-analog; battles are singles only; data tables are compiled C arrays.

Before the session ends: refresh Docs/Status.md, tick the Roadmap.md checkbox, and append non-trivial decisions to Docs/DecisionLog.md -- in the same PR as the change. If you hit a blocker, log it in Docs/Questions.md with your best-guess action and cost-if-wrong, then continue. Do not stop to wait for me.

Begin.
```

---

## 2. Continue from checkpoint

When to use: resuming a task a previous session left in flight; trust
Status.md's "Current task" rather than re-deriving from the Roadmap.

```
You are resuming in-flight Zenithmon work (Games/Zenithmon). Read these files in order before doing anything else:

1. Games/Zenithmon/Docs/Status.md -- TRUST its "Current task" and "Notes for next agent" sections; that is where the last session stopped. Do not re-pick a task from the Roadmap.
2. Games/Zenithmon/Docs/Roadmap.md -- surrounding context for the current task only.
3. Games/Zenithmon/Docs/AgentBriefing.md -- conventions and operating rules.

Games/Zenithmon/Docs/MasterPlan.md is the full approved program plan behind the Roadmap. Check `git status` and `git branch` first: the in-flight feature branch (zenithmon/s<stage>-<slug>) may already exist with uncommitted or unpushed work -- continue on it rather than branching fresh. Docs/Scope.md is binding; grep Docs/DecisionLog.md before re-deciding.

Same rules as a fresh session: test-first per Docs/TestPlan.md (tests land WITH the system, same PR); build only via `pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon`; verify LOCALLY with `... test Zenithmon --headless` AND the boot unit gate `Tools\run_unit_gate.ps1 -Exe <exe> -Baseline N` (runs the ZM_* unit tests `zenith test` skips; bump N + the workflow baseline in-PR); do NOT wait on CI -- enable auto-merge (`Tools\zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`), which lands the PR when zm-tests goes green (nothing merges red; `--admin` is blocked); regen after file adds; Tools\zenith_gh.ps1 for gh; never commit baked assets; asset-dependent tests RequestSkip when assets are absent; ZM_ prefix; zero Nintendo IP; no audio / networking / Dynamax-analog; singles battles only; data tables are compiled C arrays.

Finish the current task through PR, then update Docs/Status.md, tick the Roadmap.md box, and append decisions to Docs/DecisionLog.md in the same PR. Blockers go to Docs/Questions.md with a best guess; keep moving.

Begin.
```

---

## 3. Stage-gate review

When to use: a stage (S<n>) looks complete and you want the full gate checklist
run and the gate docs updated. Replace `S<N>` with the actual stage.

```
You are running the S<N> stage-gate review for Zenithmon (Games/Zenithmon). Read Games/Zenithmon/Docs/Status.md, then Games/Zenithmon/Docs/Roadmap.md -- the S<N> section defines the gate checklist (Games/Zenithmon/Docs/MasterPlan.md has the full gate rationale). Docs/AgentBriefing.md has the commands and conventions.

Run every gate item and record evidence for each:

1. `pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon` (Vulkan_vs2022_Debug_Win64_True) -- green.
2. The 4-config matrix (major gates): Vulkan Debug/Release x True/False all build, plus the D3D12_vs2022_Debug_Win64_False null-backend link proof.
3. `pwsh -NoProfile -File Tools\zenith.ps1 test Zenithmon --headless` -- 0 failures; capture pass counts and the slowest-10 tests against the suite-runtime budget.
4. The stage-scoped windowed runs listed in the S<N> gate (`... test Zenithmon --filter <Test>` per item).
5. CI: the zm-tests check is green on the gate PR (Tools\zenith_gh.ps1 pr checks <n>).
6. The stage's listed visual check: capture windowed screenshot evidence, then STOP -- write a "GATE-WAIT: S<N> visual sign-off" marker into Status.md with the evidence paths. The gate is NOT passed until I sign off (prompt 4). Do not tick the gate or start the next stage.
7. Docs current: every S<N> checkbox in Roadmap.md ticked or explicitly deferred with a reason.

Update the automated-results docs IN THE SAME PR: refresh Docs/Shortfalls.md (honest gap audit vs the GDD -- what shipped vs what MasterPlan.md promised), update any affected format docs (Docs/SaveFormat.md, Docs/AssetManifest.md, Docs/TestPlan.md test counts), append a gate-results entry to Docs/DecisionLog.md, refresh Docs/Status.md.

Report PASS or FAIL per automated item with evidence, then the GATE-WAIT status. If an automated item fails, do not set GATE-WAIT: log the failure in Status.md as the current task and either fix it (if small) or stop and report.

Begin.
```

---

## 4. Gate sign-off (user unblocks a GATE-WAIT)

When to use: you have reviewed the screenshot evidence a GATE-WAIT iteration
captured and are approving (or rejecting) the stage's visual check. Replace
`S<N>` and the verdict.

```
Zenithmon S<N> visual gate verdict: APPROVED. (If rejecting, replace with: REJECTED -- <what to rework>.)

Read Games/Zenithmon/Docs/Status.md (the GATE-WAIT marker + evidence paths). If APPROVED: append the sign-off to Docs/DecisionLog.md (date, gate, evidence reviewed, verdict), tick the S<N> gate line in Docs/Roadmap.md, clear the GATE-WAIT marker from Status.md, land those doc updates as a PR, and resume the lifecycle loop (StartPrompts.md prompt 0 protocol) from the next Roadmap item. If REJECTED: log the rework as the current task in Status.md, append the verdict to DecisionLog.md, and execute the rework through the normal PR flow.

Begin.
```

---

## Notes on session launch

- **Working directory:** open Claude Code at `C:\dev\Zenith` so relative paths
  in the prompts resolve.
- **Unattended runs:** the checked-in `.claude/settings.json` allowlists the
  loop's build/test/git/gh commands so iterations never stall on permission
  prompts; the merge authorization is carried by prompt 0 itself (you issue it
  by pasting/looping the prompt). Both were user-approved 2026-07-10
  (DecisionLog ZM-D-017).
- **CI policy -- do NOT wait (user policy 2026-07-10, DecisionLog ZM-D-028):**
  the full LOCAL gate is the authoritative quality bar and must pass before you
  push -- `zenith build`, the boot unit gate
  (`Tools\run_unit_gate.ps1 -Exe <exe> -Baseline N`, which runs the ZM_* unit
  tests `zenith test` skips), and `zenith test --headless`. After opening a PR,
  enable auto-merge (`Tools\zenith_gh.ps1 pr merge <n> --auto --squash`) so it
  lands the instant the required `zm-tests` check is green; NEVER idle-watch CI,
  wait for the slower discipline gates, or launch a blocking `pr checks --watch`.
  `--admin`/bypass merges are blocked by the harness, so zm-tests must genuinely
  go green -- auto-merge honours that without you waiting. Fill the CI window by
  designing/prototyping the next task (e.g. golden vectors in a scratchpad model).
- **gh auth:** all gh calls go through `Tools\zenith_gh.ps1`, which reuses the
  git credential manager when `gh auth login` is absent (sandboxed sessions).
- **Verification:** the first thing the agent should do is read the named docs.
  If it starts doing anything else first, abort and re-paste.
- **Multi-agent sessions:** the orchestrator invariants live in
  [OrchestratorPlaybook.md](OrchestratorPlaybook.md); the prompts point the
  agent there when it dispatches subagents.
- **Termination:** iterations end with a one-paragraph report and a refreshed
  [Status.md](Status.md). A GATE-WAIT report means the loop is parked on you.

## Prompt iteration

If a prompt repeatedly produces bad behaviour, update it here in a follow-up
PR. Prompts are version-controlled artefacts; treat them like configuration.
