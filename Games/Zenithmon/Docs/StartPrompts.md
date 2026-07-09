# Zenithmon -- Session Start Prompts

**Document purpose:** Copy-paste prompts to start fresh Claude Code sessions on
Zenithmon. Pick the one matching your intent; paste verbatim into a fresh
session opened at cwd `C:\dev\Zenith`.

---

## 1. Fresh start (default -- use this most of the time)

When to use: starting normal development work with no in-flight task; the agent
picks the next Roadmap item itself.

```
You are working on Zenithmon (Games/Zenithmon). Read these files in order before doing anything else:

1. Games/Zenithmon/Docs/Status.md -- current state.
2. Games/Zenithmon/Docs/Roadmap.md -- the S0-S12 stage plan; pick the FIRST un-checked task in the current stage.
3. Games/Zenithmon/Docs/AgentBriefing.md -- conventions and operating rules.

Games/Zenithmon/Docs/Scope.md is binding -- do not add anything outside it. Grep Games/Zenithmon/Docs/DecisionLog.md before re-deciding anything already decided. If you will dispatch subagents, also read Games/Zenithmon/Docs/OrchestratorPlaybook.md and obey its invariants (only you build/test/run; subagents never build; no git worktrees; you alone write the living docs).

Execute the task test-first per Docs/TestPlan.md: tests land WITH the system, in the same PR. Work on a feature branch named zenithmon/s<stage>-<slug>. Build with `zenith build Zenithmon` (never the whole sln; msbuild fallback is /t:Zenithmon on Games/Zenithmon/zenithmon_win64.sln). Verify with `zenith test Zenithmon --headless`. Nothing merges red (required CI check: zm-tests). Regenerate solutions with `zenith regen` after adding files. Baked assets are git-ignored -- never commit them; asset-dependent tests RequestSkip when assets are absent.

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

Check `git status` and `git branch` first: the in-flight feature branch (zenithmon/s<stage>-<slug>) may already exist with uncommitted or unpushed work -- continue on it rather than branching fresh. Docs/Scope.md is binding; grep Docs/DecisionLog.md before re-deciding anything.

Same rules as a fresh session: test-first per Docs/TestPlan.md (tests land WITH the system, same PR); build only via `zenith build Zenithmon` / msbuild /t:Zenithmon; verify with `zenith test Zenithmon --headless`; nothing merges red (required check: zm-tests); `zenith regen` after file adds; never commit baked assets; asset-dependent tests RequestSkip when assets are absent; ZM_ prefix; zero Nintendo IP; no audio / networking / Dynamax-analog; singles battles only; data tables are compiled C arrays.

Finish the current task through PR, then update Docs/Status.md, tick the Roadmap.md box, and append decisions to Docs/DecisionLog.md in the same PR. Blockers go to Docs/Questions.md with a best guess; keep moving.

Begin.
```

---

## 3. Stage-gate review

When to use: a stage (S<n>) looks complete and you want the full gate checklist
run and the gate docs updated. Replace `S<N>` with the actual stage.

```
You are running the S<N> stage-gate review for Zenithmon (Games/Zenithmon). Read Games/Zenithmon/Docs/Status.md, then Games/Zenithmon/Docs/Roadmap.md -- the S<N> section defines the gate checklist. Docs/AgentBriefing.md has the commands and conventions.

Run every gate item and record evidence for each:

1. `zenith build Zenithmon` (Vulkan_vs2022_Debug_Win64_True) -- green.
2. The 4-config matrix (major gates): Vulkan Debug/Release x True/False all build, plus the D3D12_vs2022_Debug_Win64_False null-backend link proof.
3. `zenith test Zenithmon --headless` -- 0 failures; capture pass counts and the slowest-10 tests against the suite-runtime budget.
4. The stage-scoped windowed runs listed in the S<N> gate (`zenith test Zenithmon --filter <Test>` per item).
5. CI: the zm-tests check is green on the gate PR.
6. The stage's listed visual check (windowed run; screenshot evidence).
7. Docs current: every S<N> checkbox in Roadmap.md ticked or explicitly deferred with a reason.

Then update the gate docs IN THE SAME PR: tick the S<N> gate line in Docs/Roadmap.md, refresh Docs/Shortfalls.md (honest gap audit vs the GDD -- what shipped vs what the plan promised), and update any affected format docs (Docs/SaveFormat.md, Docs/AssetManifest.md, Docs/TestPlan.md test counts). Append a gate-results entry to Docs/DecisionLog.md and refresh Docs/Status.md.

Report PASS or FAIL per item with evidence. The gate is not passed until every item is green AND the docs are updated. If an item fails, do not tick the gate: log the failure in Status.md as the current task and either fix it (if small) or stop and report.

Begin.
```

---

## Notes on session launch

- **Working directory:** open Claude Code at `C:\dev\Zenith` so relative paths
  in the prompts resolve.
- **Verification:** the first thing the agent should do is read the named docs.
  If it starts doing anything else first, abort and re-paste.
- **Multi-agent sessions:** the orchestrator invariants live in
  [OrchestratorPlaybook.md](OrchestratorPlaybook.md); prompt 1 and 2 already
  point the agent there when it dispatches subagents.
- **Termination:** sessions end with a one-paragraph summary and a refreshed
  [Status.md](Status.md). That is the natural stopping point.

## Prompt iteration

If a prompt repeatedly produces bad behaviour, update it here in a follow-up
PR. Prompts are version-controlled artefacts; treat them like configuration.
