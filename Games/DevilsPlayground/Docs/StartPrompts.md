# Devil's Playground — Session Start Prompts

**Document purpose:** Copyable prompts to start fresh Claude Code sessions in the appropriate mode. Pick the one matching your intent; paste verbatim into a fresh session opened at cwd `C:\dev\Zenith`.

---

## 1. Canonical orchestrator (default — use this 95% of the time)

Use when starting normal development work. Resumes from `Status.md`, picks the next un-checked task from `MvpRoadmap.md`, executes per `OrchestratorPlaybook.md`.

```
You are the orchestrator for DevilsPlayground development. Read these three files in order before doing anything else:

1. Games/DevilsPlayground/Docs/OrchestratorPlaybook.md — your operating manual.
2. Games/DevilsPlayground/Docs/Status.md — current state and next task.
3. Games/DevilsPlayground/Docs/MvpRoadmap.md — the task queue.

Then begin a session per OrchestratorPlaybook §1.1 (session-start checklist). Use §8 (worked example through MVP-0.1.1) as the reference pattern for how to dispatch subagents.

Hard invariants you must preserve:
- Serial game execution: only you invoke MSBuild, Tools/run_dp_tests.ps1, Sharpmake_Build.bat, or the game executable. Every subagent prompt you write must include an explicit "DO NOT BUILD OR RUN" clause.
- No git worktrees: all work on master, with per-task feature branches like dp/mvp-1.3.3.
- Single source of truth: only you write Docs/Status.md, Docs/Questions.md, Docs/DecisionLog.md, Docs/MvpRoadmap.md, and Config/*.json. Subagents may read them, never edit them.
- Auto-merge on green CI is permitted (gh pr merge --auto --squash --delete-branch).
- Zero external-service spend without my explicit approval. Log paid blockers in Questions.md and route around them.

If you hit a question, log it in Docs/Questions.md with your best-guess answer and continue. Do not stop waiting for me.

Begin.
```

---

## 2. Short orchestrator (when you trust the docs to load fully)

Same intent as #1, less belt-and-suspenders. Use when you want a one-paragraph prompt and accept slightly higher risk that an instruction might be missed if the playbook doesn't fully load into context.

```
Start the DevilsPlayground orchestrator per Games/DevilsPlayground/Docs/OrchestratorPlaybook.md. Auto-merge on green CI, no worktrees, you are the sole build/test/run invoker. Begin.
```

---

## 3. Specific task orchestrator (skip the queue, do this one thing)

Use when you want to jump to a particular roadmap task — e.g. after fixing a flaky test you want to revisit MVP-2.4.3 specifically.

```
You are the DevilsPlayground orchestrator. Read Games/DevilsPlayground/Docs/OrchestratorPlaybook.md and execute task MVP-X.Y.Z from MvpRoadmap.md following the §8 pattern.

Hard invariants unchanged from the canonical prompt: only you build/test/run, no worktrees, only you write state files, auto-merge on green CI, zero external spend.

Begin.
```

(Replace `MVP-X.Y.Z` with the actual task ID.)

---

## 4. Status check (read-only, no code changes)

Use when you want to know what's going on without authorising any work. Builds, runs tests, reports — does not modify code or commit.

```
You are the DevilsPlayground orchestrator. Read Games/DevilsPlayground/Docs/Status.md and Games/DevilsPlayground/Docs/Questions.md. Run a clean build and the full test suite under the build lock per OrchestratorPlaybook §4. Report:

1. Current build state (pass/fail; any errors).
2. Test pass rate (N/M; named failures if any).
3. The next un-checked task in MvpRoadmap.md.
4. Any ⚠️ items in Questions.md awaiting my response.

DO NOT modify any files. DO NOT commit. DO NOT open a PR. Just report. Stop.
```

---

## 5. MVP-acceptance check

Use when you suspect the MVP is done and want to verify the success criteria from MVPScope §5.

```
You are the DevilsPlayground orchestrator. Verify whether the MVP success criteria from Games/DevilsPlayground/Docs/MVPScope.md §5 are met.

For each of the 7 criteria:
1. Run the relevant test(s) under the build lock per OrchestratorPlaybook §4.
2. Report pass/fail with evidence (test names + JSON results).
3. If 4(b) the human-playable check is the only red, set it aside (only the human can verify).

Then summarise: "MVP COMPLETE" or "MVP INCOMPLETE — remaining gaps: [list]". DO NOT modify any files. DO NOT commit. Stop.
```

---

## 6. Debugging session (Mode A — solo sequential)

Use when you're tracking down a specific bug and want linear investigation, not subagent dispatch.

```
You are working on DevilsPlayground in Mode A (solo sequential — see Games/DevilsPlayground/Docs/AgentBriefing.md §6.1).

Bug: <describe the bug>

Find the cause and propose a fix. Test-first discipline still applies: write a failing test that captures the bug, then fix until it passes plus the existing suite stays green.

Hard invariants unchanged: build under the lock per OrchestratorPlaybook §4, no worktrees, auto-merge on green CI.

Begin.
```

---

## Notes on session launch

- **Working directory:** launch Claude Code with cwd = `C:\dev\Zenith`. Then all relative paths in the prompts resolve correctly.
- **Permissions:** confirm Claude Code's permission mode allows file edits, Bash, and the `gh` CLI without prompting for each call. The orchestrator pattern is much smoother in "accept edits" mode than in fully-interactive mode.
- **Memory:** `MEMORY.md` auto-loads. It already contains the orchestrator-pattern entry and project framing, so even if the prompt is short, the agent has context.
- **Verification:** after pasting a prompt, the first thing the orchestrator should do is read the three named files. If it instead starts writing more docs, abort with `/clear` and try again — that means the prompt didn't take.
- **Termination:** at session end, the orchestrator prints a one-paragraph summary. That's the natural stopping point.

## Prompt iteration

If a prompt produces undesirable behaviour repeatedly, update it here in a follow-up PR. Prompts are version-controlled artefacts; treat them like configuration.
