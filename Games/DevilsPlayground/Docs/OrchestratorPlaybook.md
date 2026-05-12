# Devil's Playground — Orchestrator Playbook

**Document purpose:** The operating manual for the **orchestrator** role. When the user starts a new DP session, the primary Claude Code instance reads this file and acts as orchestrator — dispatching subagents for research/code/review/lint work while owning the serial build/test gate itself.

**Audience:** The orchestrating Claude instance (you, on session start). Subagents do NOT read this file — they read scoped briefings from the orchestrator.

**Companion docs:** [AgentBriefing.md](AgentBriefing.md) (general conventions; you inherit them) · [MvpRoadmap.md](MvpRoadmap.md) (your task queue).

---

## 0. The Three Invariants

Every action in an orchestrated session must preserve these three properties. Violations break the user's stated constraints; do not break them.

### Invariant 1 — Serial game execution
**At most one entity invokes `MSBuild` or `Tools/run_dp_tests.ps1` or runs the game executable at any time.**

The user explicitly forbade concurrent game execution as the basis for skipping git worktrees. The orchestrator is the **only** entity permitted to build or run. Subagents author files; the orchestrator builds.

Mechanically enforced by:
- Foreground subagent calls block the orchestrator (one Claude thread).
- Background subagent prompts include an explicit "DO NOT BUILD OR RUN" clause.
- A defensive file lock at `Build/.build_lock` catches any accidental violation.

### Invariant 2 — No git worktrees
**All work happens on the main repo's checkout at `C:\dev\Zenith\` on `master` (with per-task feature branches).**

Worktrees were the alternative path for parallel work; the user rejected it. We instead serialise game execution and live with a single working tree.

### Invariant 3 — Single source of truth
**`Docs/Status.md`, `Docs/MvpRoadmap.md`, `Docs/DecisionLog.md`, `Docs/Questions.md` are written by the orchestrator only.** Subagents may read them but never write them.

Reason: race-free write semantics. Subagents return summaries; the orchestrator integrates and writes once.

---

## 1. Session Start

### 1.1 First five minutes

1. **Read `Games/DevilsPlayground/Docs/ManualSetupChecklist.md`.** If ANY box is unticked, STOP. Write a Questions.md entry asking the user to complete it. Do not attempt any Phase 0.0 task until every box is ticked. The orchestrator cannot install Vulkan SDK, configure GitHub branch protection, or set up `gh` auth — these are human-only one-time tasks.
2. Read `Games/DevilsPlayground/Docs/Status.md` — current task, build state, notes for next agent.
3. Read `Games/DevilsPlayground/Docs/Questions.md` — surface ⚠️ items the user may have answered between sessions.
4. Read the next un-checked task in `Games/DevilsPlayground/Docs/MvpRoadmap.md`.
5. Decide which subagents the task needs (see §3 catalogue).
6. Acquire the build lock (see §4) if you'll be building.
7. **Verify the baseline.** Quick `git status` to confirm clean tree on master. If dirty, investigate before doing anything.

### 1.2 The orchestrator's mental model

You are a tech lead. You don't write all the code yourself; you scope tasks for team members, integrate their work, run the build, open the PR.

- Read enough to understand the task; don't over-research.
- Brief subagents tightly with explicit *write* scope (which files they may edit).
- Run the build/tests yourself; subagents return code, not test results.
- Update state files yourself; subagents return content, not commits.
- Open the PR yourself; subagents never invoke `gh`.

---

## 2. The Standard Task Loop

For a typical MvpRoadmap task, the orchestrator runs this loop. Adapt for special cases (research-only, doc-only, etc.).

```
┌─────────────────────────────────────────────────────┐
│ READ STATE        (Status.md, Questions.md, task)   │
│       ↓                                              │
│ PLAN              (Plan subagent if non-trivial)    │
│       ↓                                              │
│ RESEARCH          (Explore subagent if needed)      │
│       ↓                                              │
│ AUTHOR TEST       (Implementer subagent — test first)│
│       ↓                                              │
│ ORCHESTRATOR runs the failing test  (lock + run)    │
│       ↓                                              │
│ AUTHOR CODE       (Implementer subagent)            │
│       ↓                                              │
│ ORCHESTRATOR runs full test suite  (lock + run)     │
│       ↓                                              │
│ REVIEW            (Reviewer subagent for §5.8 check) │
│       ↓                                              │
│ ORCHESTRATOR commits, pushes, opens PR              │
│       ↓                                              │
│ ORCHESTRATOR updates Status.md, DecisionLog.md      │
│       ↓                                              │
│ ORCHESTRATOR ticks MvpRoadmap.md, queues next task  │
└─────────────────────────────────────────────────────┘
```

The orchestrator is in the loop on every step; subagents fill specific boxes.

---

## 3. Subagent Role Catalogue

Each role is a specific way to invoke the `Agent` tool. Pick the role that matches the work. **Brief tightly** — every subagent needs the goal, the scope, the success criterion, and the *no-build-or-run* clause.

### 3.1 Researcher (subagent_type: `Explore`)

**Use when:** you need to find something in the codebase across multiple files / patterns.

**Tool restrictions:** Explore is read-only by tool definition (no Edit/Write).

**Don't use for:** a known single-file read (use `Read` directly). One Grep (use `Grep` directly). Explore is for *multi-step* exploration.

**Example prompt:**

> *"I'm implementing the demon-scent system for MVP-1.6.1. Find every place in `Games/DevilsPlayground/` where `DP_Player::SetPossessedVillager` is called or where possession-switch events are dispatched. Report file:line and a one-line context for each. ~200 words. No code changes."*

### 3.2 Planner (subagent_type: `Plan`)

**Use when:** the task involves multi-file refactoring or new architecture and you want a step-by-step plan with risks called out before committing.

**Tool restrictions:** Plan is read-only by tool definition.

**Example prompt:**

> *"Design the implementation plan for MVP-1.3.3 — adding an Apprehend BT branch to Priest_Behaviour. Constraints: must integrate with existing Selector→[Pursue, Investigate, Patrol] structure; must dispatch a new DP_OnRunLost event; must support voluntary-switch interruption. Existing code is in `Games/DevilsPlayground/Components/Priest_Behaviour.h` and `DP_BT_Nodes.h`. Return: step list, files to touch, risks, tests required. Under 400 words."*

### 3.3 Implementer (subagent_type: `general-purpose`)

**Use when:** you need code or tests written and the scope is bounded.

**Tool restrictions:** none — but the prompt MUST include the "DO NOT BUILD OR RUN THE GAME" clause.

**Mandatory prompt clauses:**

- *"Files you may edit: [exhaustive list]."* — scope can include paths under `Games/DevilsPlayground/` and/or `Zenith/` (engine code is in-scope per 2026-05-12 user direction); be specific.
- *"DO NOT run `MSBuild`, `Tools/run_dp_tests.ps1`, `Sharpmake_Build.bat`, or the game executable. The orchestrator handles all build/test/run operations."*
- *"DO NOT modify Docs/Status.md, Docs/MvpRoadmap.md, Docs/DecisionLog.md, Docs/Questions.md, or anything in `Config/`. The orchestrator manages these."*
- *"Follow Zenith conventions: no std::function / std::vector / std::mutex. Use Zenith_Vector, Zenith_Mutex, function pointers. All .cpp files start with `#include \"Zenith.h\"`. Tests wrapped in `#ifdef ZENITH_INPUT_SIMULATOR`."*
- *"For engine-code changes (paths under `Zenith/`): the orchestrator will spawn a Reviewer subagent (mandatory) and run a Combat smoke build before merging. Surface any cross-game concerns in your report."*
- *"Report back: list of files changed + a one-paragraph summary."*

**Example prompt:**

> *"Implement the Apprehend BT branch for MVP-1.3.3. Files you may edit: only `Games/DevilsPlayground/Components/Priest_Behaviour.h` (add the branch construction in OnStart), `Games/DevilsPlayground/Components/DP_BT_Nodes.h` (add `DP_BTCondition_InApprehendRange` and `DP_BTAction_ApprehendChannel`), `Games/DevilsPlayground/Source/PublicInterfaces.h` (add the `DP_OnRunLost` event struct), and `Games/DevilsPlayground/Source/PublicInterfaces.cpp` (event implementation if needed). [Plus the no-build-or-run clauses.] Report: changed files + summary."*

### 3.4 Test Author (subagent_type: `general-purpose`)

**Use when:** you need a specific test authored. Often spawned BEFORE the implementer, so the failing test guides the implementation.

**Same restrictions as Implementer.** The "files you may edit" list typically includes only one path under `Games/DevilsPlayground/Tests/`.

**Example prompt:**

> *"Author `Test_P1Apprehend_PriestCatchesPlayer`. Files you may edit: only `Games/DevilsPlayground/Tests/Test_P1Apprehend_PriestCatchesPlayer.cpp` (create). The test pattern is the 7-phase state machine from `Games/DevilsPlayground/Docs/TestPlan.md` §2.3. Setup an authored test scene with priest at (0,0,0) and villager at (1,0,0); subscribe to `DP_OnRunLost` (which is being added in MVP-1.3.2 — your test depends on that PR landing first); possess the villager, pin it (no input), wait 180+ frames; assert `DP_OnRunLost` dispatched exactly once. Max frames 240. [Plus the no-build-or-run clauses.]"*

**Note:** `DP_OnRunLost` is **not** yet in `PublicInterfaces.h` (validated 2026-05-11). MVP-1.3.2 creates it. Sequence the test author to either land alongside MVP-1.3.2 or after.

### 3.5 Reviewer (subagent_type: `general-purpose`, prompt-enforced read-only)

**Use when:** you've completed implementation and want a second-pass check before opening the PR.

**Mandatory prompt clauses:**

- *"You are reviewing, not implementing. Do not edit any files."*
- *"DO NOT build or run the game."*

**Example prompt:**

> *"Review the staged changes for MVP-1.3.3 against the §5.8 Pre-Flight Checklist in AgentBriefing.md. Specifically check: (a) test exists and matches the implementation, (b) no std::function/vector/mutex, (c) ZENITH_INPUT_SIMULATOR guard on test code, (d) no post-MVP scope creep. The git diff against master is: [paste diff]. Report: any failed checklist items + one-paragraph quality assessment. No edits."*

### 3.6 Linter (subagent_type: `general-purpose`)

**Use when:** mechanical fixups after implementation — formatting drift, naming consistency, dead-code removal — that the implementer might have missed.

**Same Implementer restrictions** + an extra: *"Make only mechanical changes. If you'd need to alter logic to fix something, leave it and report it."*

### 3.7 Doc Maintainer (subagent_type: `general-purpose`, doc-only scope)

**Use when:** ancillary doc changes after a task — updating GDD if scope shifted, refreshing TestPlan with new tests, etc.

**Files-may-edit list:** *only specific doc files*. Never includes `Source/`, `Components/`, `Tests/`, `Config/`. Never includes Status/Questions/DecisionLog/MvpRoadmap (orchestrator-owned).

---

## 4. The Build/Test Gate

The orchestrator is the sole invoker of MSBuild and the test runner. This section is the recipe.

### 4.1 Acquire the lock (atomic with PID + liveness)

The Test-Path-then-New-Item pattern below is a TOCTOU race: two agents that both see the file absent both create it. **Use atomic FileStream creation instead.** The PID is written into the lock; before stealing a stale lock, the orchestrator confirms the prior PID is no longer alive.

```powershell
# C:\dev\Zenith\Tools\build_lock.ps1 — call from every build/test wrapper.
function Acquire-BuildLock {
    param([int]$TimeoutSeconds = 60)
    $lockFile = "C:\dev\Zenith\Build\.build_lock"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ($true) {
        try {
            # Atomic create-or-fail. CreateNew throws IOException if the file exists.
            $fs = [System.IO.File]::Open($lockFile, 'CreateNew', 'Write', 'None')
            try {
                $stamp = "{0}|{1}|{2}" -f $PID, $env:COMPUTERNAME, (Get-Date -Format o)
                $bytes = [System.Text.Encoding]::UTF8.GetBytes($stamp)
                $fs.Write($bytes, 0, $bytes.Length)
            } finally {
                $fs.Close()
            }
            return $true
        }
        catch [System.IO.IOException] {
            # Lock held. Check whether the holder is still alive.
            try {
                $existing = Get-Content $lockFile -Raw -ErrorAction Stop
                $parts = $existing.Trim().Split('|')
                $holderPid = [int]$parts[0]
                $holderHost = $parts[1]
                if ($holderHost -eq $env:COMPUTERNAME) {
                    $proc = Get-Process -Id $holderPid -ErrorAction SilentlyContinue
                    if (-not $proc) {
                        # Holder is dead. Steal the lock atomically by deleting and retrying.
                        Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
                        continue
                    }
                }
            } catch {
                # Lock file is malformed; treat as stale.
                Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
                continue
            }

            if ((Get-Date) -gt $deadline) {
                throw "Build lock held for > $TimeoutSeconds s by PID $holderPid on $holderHost. Abort. Manual recovery: Remove-Item '$lockFile' if you've verified no build is in progress."
            }
            Start-Sleep -Seconds 2
        }
    }
}

function Release-BuildLock {
    $lockFile = "C:\dev\Zenith\Build\.build_lock"
    Remove-Item $lockFile -Force -ErrorAction SilentlyContinue
}
```

**Usage from every build/test wrapper:**

```powershell
. C:\dev\Zenith\Tools\build_lock.ps1
Acquire-BuildLock
try {
    # MSBuild / Tools/run_dp_tests.ps1 / Sharpmake / asset import / scene export
    # …
} finally {
    Release-BuildLock
}
```

**Why this matters:** the previous Test-Path-then-New-Item approach failed two ways. (a) Two agents see the file absent within the same 100ms window and both create it; both proceed to invoke MSBuild; mspdbsrv thrashes per memory `feedback_parallel_agents_msbuild_thrash.md`. (b) An orchestrator crash leaves a stale lock that the 30-min age cleanup can't tell apart from a real long-running build.

The atomic + PID-liveness pattern fixes both. The only remaining failure mode is the same machine reusing a PID for a different process; the safety net is the timeout, which converts that case into a manual recovery prompt.

### 4.2 Build

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
    Build\zenith_win64.sln `
    /p:Configuration=vs2022_Debug_Win64_True `
    /p:Platform=x64 `
    -maxCpuCount:1
```

If a new `.cpp` file was added by a subagent, run Sharpmake first:

```powershell
cd Build
cmd /c '.\Sharpmake_Build.bat < nul'
cd ..
```

### 4.3 Test

```powershell
pwsh.exe -File Tools/run_dp_tests.ps1 -Headless
```

Filter to the current task's tests during iteration:

```powershell
pwsh.exe -File Tools/run_dp_tests.ps1 -Filter "Apprehend" -Headless
```

### 4.4 Release the lock

`Release-BuildLock` (defined in `Tools/build_lock.ps1` above) is the canonical release. Always call it inside a `finally` block.

### 4.5 Stale lock cleanup

Stale-lock recovery is now built into `Acquire-BuildLock` (PID-liveness check above). No separate session-start cleanup is needed — the atomic acquire will steal a dead-process lock on first try. If a lock is held by a *live* process and that process is genuinely stuck, the timeout in `Acquire-BuildLock` raises an explicit error rather than silently overwriting.

### 4.6 What else the lock must cover

The lock protects more than `MSBuild` and `Tools/run_dp_tests.ps1`. Every operation that touches build output, generated source, or the game's runtime state needs it:

- `MSBuild zenith_win64.sln ...`
- `Sharpmake_Build.bat`
- `Tools/run_dp_tests.ps1`
- `ZenithTools.exe import ...` (asset import — may regenerate scene files and headers)
- `ZenithTools.exe lint ...` (asset linter; reads build outputs)
- `devilsplayground.exe` (any direct run of the game)
- Editor-automation scene save / export

Subagents that author *source files only* (header + cpp text) don't need the lock. Anything that *invokes* the build chain does.

---

## 5. Dispatch Patterns

Five common patterns. Pick the one that matches the task.

### 5.1 The single-implementer pattern (most tasks)

```
1. Orchestrator reads roadmap → identifies a small bounded task.
2. Orchestrator spawns Test Author (foreground): authors failing test.
3. Orchestrator builds + runs test, confirms it fails for the right reason.
4. Orchestrator spawns Implementer (foreground): authors code.
5. Orchestrator builds + runs full suite, confirms green.
6. **MANDATORY for any PR that changes game logic** (anything under `Source/`, `Components/`, `Config/`): orchestrator spawns Reviewer subagent per §5.4 before commit. Plumbing PRs (workflow files, doc-only changes, Sharpmake regen, build scripts) may skip the reviewer.
7. Orchestrator commits, pushes, PRs, updates state files.
```

Best for: well-scoped MvpRoadmap tasks. Most MVP work fits here.

**Why mandatory review:** auto-merge on green CI removes the human gate. The reviewer subagent IS the human-equivalent gate. Skipping it for logic changes means bugs ship to master with only test coverage as the safety net, and tests don't catch design errors or contract violations.

### 5.2 The plan-first pattern (multi-file refactors)

```
1. Orchestrator spawns Planner (foreground): returns step list.
2. Orchestrator reviews plan, optionally asks user via Questions.md.
3. For each step of the plan: §5.1 single-implementer pattern.
4. After all steps: integration commit.
```

Best for: tasks marked in the roadmap as touching >3 files, or where the existing code shape is unclear.

### 5.3 The parallel-author pattern (independent files)

```
1. Orchestrator identifies N independent files needing changes.
2. Orchestrator spawns N Implementers (background): each told its file scope.
3. Orchestrator waits (you'll be notified when each completes).
4. Orchestrator integrates: builds + runs tests once.
```

Best for: e.g. authoring 4 archetype test cases that touch 4 distinct test files. The build/test step still serialises, but authoring time parallelises.

**Crucial:** the file scopes must NOT overlap. If two background subagents both write `DevilsPlayground.cpp`, you'll get a corrupted file. The orchestrator's job is to partition write surfaces correctly.

### 5.4 The review-loop pattern (high-stakes changes)

```
1. Orchestrator runs §5.1 to completion (test, impl, build green).
2. Orchestrator spawns Reviewer (foreground): returns checklist verdict.
3. If clean: orchestrator commits and PRs.
4. If issues: orchestrator either fixes inline OR spawns another Implementer.
```

Best for: changes to core systems (Priest_Behaviour, possession state machine, fog pass, navmesh). Self-review is cheap insurance.

### 5.5 The research-only pattern (planning sessions)

```
1. Orchestrator spawns Researcher (foreground or background): broad scan.
2. Orchestrator integrates findings into a Questions.md entry or a roadmap update.
3. No code changes this session; preserves state for the next.
```

Best for: session-end when a task is bigger than expected and needs to be split.

---

## 6. Integrating Subagent Output

Subagents return summaries; their file edits are *already on disk*. Your job is to verify, integrate, and commit.

### 6.1 After a subagent reports complete

1. **Verify the diff.** `git diff` — the orchestrator should personally read what changed. Don't blindly trust "I implemented MVP-1.3.3" — read the actual code.
2. **Check the scope.** Did the subagent edit only the files you scoped them to? If not: revert the out-of-scope edits and brief them again. (`git restore <path>` is your friend.)
3. **Build.** Subagents don't build; you do. If it fails, the subagent's code was wrong — either fix yourself if trivial or re-dispatch with the compile error in the prompt.
4. **Run tests.** Full suite. Any failure means more work.

### 6.2 When subagents conflict on a file

Despite scoping discipline, two background subagents may accidentally touch the same file. Symptoms: garbled file content, undefined symbols, lost edits.

Recovery:
1. `git checkout HEAD -- <file>` to restore the pre-subagent version.
2. Re-dispatch one subagent at a time, foreground.

Prevention: scope writes by *directory*, not just *file*. E.g. "you may edit anything under `Games/DevilsPlayground/Tests/`" but never "you may edit `Tests/foo.cpp` and `Tests/bar.cpp`" if another agent is in the same dir.

### 6.3 Orchestrator-owned files

These files are *only* edited by the orchestrator. Never by a subagent.

- `Docs/Status.md`
- `Docs/Questions.md`
- `Docs/DecisionLog.md`
- `Docs/MvpRoadmap.md`
- `Config/*.json`
- Anything else listed under [AgentBriefing.md](AgentBriefing.md) §1 "Always-loaded state files"

A subagent that wants a roadmap update or a decision logged returns text to the orchestrator; the orchestrator writes it.

### 6.4 Subagent write-scope: game AND engine code

**Clarified 2026-05-12 (user direction):** subagents may edit files anywhere under `Games/DevilsPlayground/` **and** anywhere under `Zenith/` (engine code). The earlier round-2 framing limited subagent scope to game code only; the user has authorised engine work as in-scope for the autonomous orchestrator.

When dispatching subagents for engine work, the file-scope clause in the prompt must still be tight. Example:

> *"Files you may edit: ONLY `Zenith/Core/Zenith_AudioBus.h` (create), `Zenith/Core/Zenith_AudioBus.cpp` (create), `Zenith/Core/Zenith.h` (include the new header). Do NOT touch any other file under `Zenith/`. Do NOT touch any file under `Games/`."*

**Engine-PR safeguards** (mandatory, not optional):

1. **Reviewer subagent** dispatched per §5.4 — non-negotiable for engine PRs.
2. **Full test suite** run after build (`run_dp_tests.ps1 -Headless`), not just filtered tests.
3. **Combat smoke build** added as a CI gate for engine-touching PRs: build the Combat game's project to confirm shared engine code didn't regress. The CI workflow at `.github/workflows/dp-pr.yml` should include this as a conditional step that runs when the PR touches `Zenith/` paths.
4. **Design rationale logged** in `DecisionLog.md` before opening the PR for any net-new engine namespace or non-trivial algorithm.

---

## 7. Anti-patterns

Common orchestrator mistakes. Recognise and avoid.

### 7.1 Forgetting the no-build clause in a subagent prompt

Symptom: subagent reports "I built and ran the tests, they all pass" — meaning *they* invoked MSBuild, violating Invariant 1. The lock would have caught a true concurrent invocation, but the discipline is broken either way.

Fix: re-brief that subagent — and every subagent — with the explicit no-build-or-run clause. Make it the first paragraph of every Implementer/Linter/Doc-Maintainer prompt.

### 7.2 Spawning a subagent for a trivial read

Symptom: you spent 30 seconds writing an Explore prompt to read one file you could have just `Read`-ed.

Fix: subagents have a launch tax (~20s for context propagation). Use direct tools for known-target operations.

### 7.3 Letting subagents write state files

Symptom: a subagent edited `Status.md` with their progress; now Status.md says something contradictory to what the orchestrator thinks.

Fix: every subagent prompt forbids editing the orchestrator-owned files (§6.3). If a subagent reports useful state info, the orchestrator writes it.

### 7.4 Over-parallelising background subagents

Symptom: you spawned 5 background subagents for "speed"; they wrote conflicting files; build is broken; you've spent 30 minutes debugging the merge mess.

Fix: parallelism is for *independent* work surfaces. Default to 1-2 background subagents at a time. If a task can't be cleanly partitioned, run sequential.

### 7.5 Not verifying subagent output

Symptom: subagent reported "MVP-1.3.3 implemented"; you committed; CI failed; the subagent had hallucinated the wrong file paths.

Fix: always `git diff` before commit. Trust but verify. The orchestrator is the gatekeeper to master.

### 7.6 Running the game from within the orchestrator without acquiring the lock

Symptom: a future Mode-C parallel session steps on your build because the lock wasn't acquired.

Fix: even though Mode C isn't active for MVP, build defensive habits. Wrap every build/test command in try/finally with the lock. This costs nothing and protects against Mode C transition later.

---

## 8. Worked Example — Orchestrating the bootstrap + first gameplay task

**Updated 2026-05-12 (round-2 peer review):** the previous worked example walked through MVP-0.1.1 as if it were the first task and as if it were an implementation task. After the round-1 reconciliation, MVP-0.0.x is the first phase (bootstrap) and MVP-0.1.1 was reordered to be the failing test (not the implementation). This example now matches.

### Session 1: Bootstrap (MVP-0.0.1)

Before any gameplay code runs, Phase 0.0 must complete. This session does one bootstrap task end-to-end as a template.

```
Orchestrator reads:
  - Docs/Status.md → "Current task: MVP-0.0.1"
  - Docs/Questions.md → check for ⚠️ items
  - Docs/ManualSetupChecklist.md → confirm all items ticked by user
    (if not: STOP. Surface in Questions.md. Phase 0.0 cannot run until human has done the one-time setup.)
  - Docs/MvpRoadmap.md → MVP-0.0.1 = Author Tools/verify_build_env.ps1

Orchestrator runs (PowerShell):
  - Stale lock cleanup (atomic — see §4.1)
  - git checkout master && git pull
  - git checkout -b dp/mvp-0.0.1
```

The task: author a PowerShell script that checks for Visual Studio, Windows SDK, Vulkan SDK, .NET, pwsh/powershell, gh CLI authentication, and prints a pass/fail summary. Then run it locally; commit the script and a "ran cleanly on Tomos's machine 2026-MM-DD" note in DecisionLog.md.

No subagents needed — this is a single-file script authoring + verification task. The orchestrator does it directly:

1. Author `Tools/verify_build_env.ps1` per BuildEnvironment.md §1 contract.
2. Run it locally; capture output.
3. If anything fails, surface in Questions.md (likely a one-time user setup item).
4. Commit, push, PR with `gh pr create`, auto-merge.

### Session 2: First gameplay task (MVP-0.1.1)

After Phase 0.0 completes (the smoke PR at MVP-0.0.7 merges green), the orchestrator starts gameplay work. **MVP-0.1.1 lands the DP_Tuning API + the failing test + the implementation in a single PR.** Test-first discipline lives within the agent's local session (write test, watch it fail locally, implement, watch it pass), not across PR boundaries. The PR itself opens with green CI per the auto-merge contract.

```
Orchestrator reads:
  - Docs/Status.md → "Current task: MVP-0.1.1"
  - Docs/MvpRoadmap.md → MVP-0.1.1 = "Land DP_Tuning API + test + impl in one PR.
    Test-first within the session, not across PRs."
```

### Plan (subagent dispatch)

```
Orchestrator: spawn Planner (foreground).

Prompt:
"Design the implementation plan for MVP-0.1.1 — DP_Tuning API + test + impl in a single green PR (per the corrected test-first ordering from round-3 reconciliation: test-first lives within the session, not across PRs).
Goal: load Config/Tuning.json at startup, expose DP_Tuning::Get<T>(key)
accessor. **Verify whether the engine has an existing JSON utility first** — search Zenith/Core/, Zenith/AssetHandling/, Zenith/FileAccess/ for any *json* file. If found, use it; if not, write a minimal hand-coded flat-dotted-key reader (no recursive descent needed for our Tuning.json structure). Constraints: no std::function/vector/mutex (production code). Files likely
needed: new Source/DP_Tuning.h, new Source/DP_Tuning.cpp,
modification to DevilsPlayground.cpp's InitializeResources hook, new test file.
Return: step list (max 8 steps), files to touch, risks, test ideas.
~400 words."

Planner returns (summary):
  1. Search engine for JSON utility (Zenith/Core, Zenith/AssetHandling,
     Zenith/FileAccess). At time of authoring this example: ROUND-4 VERIFIED
     no Zenith_JsonReader exists. Use a minimal hand-coded parser.
  2. Author DP_Tuning.h declaring namespace and Get<T>(key) accessor.
  3. Author DP_Tuning.cpp implementing parse + cache.
  4. Hook DP_Tuning::Initialize() into InitializeResources.
  5. Run Sharpmake (new .cpp file).
  6. Author Test_P1Tuning_LoadsAndValuesInBand alongside.
  7. Build + run; iterate until test passes.
  8. Risk: minor — Tuning.json has _comment_ keys; parser must skip them.
```

### Research (one quick subagent — confirm or refute the JSON utility assumption)

```
Orchestrator: spawn Researcher (foreground).

Prompt:
"Find any existing JSON-parsing utility under C:\dev\Zenith\Zenith\
(NOTE: the path is Zenith/Zenith/, not Zenith/Core/ — round-4 reviewer
flagged that earlier example used the wrong path). Look in:
- Zenith/Core/
- Zenith/AssetHandling/
- Zenith/FileAccess/
Return file paths and a sentence on their API. If none found, report
'none found' — that's the actual state per round-4 validation (2026-05-12).
~100 words. No code changes."

Researcher returns (as of 2026-05-12 validation): "None found.
Zenith/Core/, Zenith/AssetHandling/, Zenith/FileAccess/ contain no
Zenith_Json* files. Engine reads .zscen via Zenith_DataStream binary
serialisation, not text JSON. DP_Tuning must include a minimal
hand-coded JSON parser (~80-120 LOC for the flat-dotted-key shape of
Tuning.json)."
```

### Test author (subagent dispatch)

```
Orchestrator: spawn Test Author (foreground).

Prompt:
"Author Test_P1Tuning_LoadsAndValuesInBand.

Files you may edit: ONLY Games/DevilsPlayground/Tests/Test_P1Tuning_LoadsAndValuesInBand.cpp (create new).

DO NOT run MSBuild, Tools/run_dp_tests.ps1, Sharpmake_Build.bat, or
the game executable. DO NOT edit Docs/* or Config/*. The orchestrator
handles all build/test operations.

Test contract:
- Setup: none.
- Step: phase 0 — query Get<float>('possession.life_timer_default_s'),
  capture. Phase 1 — query a sample of 10 keys spanning all top-level
  sections; capture each. Return false at phase 2.
- Verify: life_timer_default_s == 30.0 ±0.01, and each captured value
  is within a sanity-range table (provide table in test file).

Follow Zenith conventions: no std::function/vector/mutex, all .cpp
starts with #include \"Zenith.h\", wrapped in #ifdef ZENITH_INPUT_SIMULATOR.

Use Zenith_AUTOMATED_TEST_REGISTER macro.

Report: changed files + one-paragraph summary."

Test Author returns:
  Files changed: Tests/Test_P1Tuning_LoadsAndValuesInBand.cpp (new).
  Summary: implemented the 3-phase state machine; sanity table covers
  10 keys including life_timer, scent_decay, sight_range, fog memory
  durations.

Orchestrator: git diff Tests/Test_P1Tuning_LoadsAndValuesInBand.cpp
(visually verify ~80 lines, conventions followed, file looks sane).
```

### Implementer (subagent dispatch — same PR as the test)

**Important (round-3 correction):** the test author and implementer dispatch happens in the same agent session, before any commit. The PR opens with API + test + impl all green together. Test-first discipline = write the test, watch it fail *locally*, implement, watch it pass *locally*, then commit and PR. CI sees one green PR.

```
Orchestrator: spawn Implementer (foreground).

Prompt:
"Implement DP_Tuning for MVP-0.1.1 in the same PR as Test_P1Tuning_LoadsAndValuesInBand.

Files you may edit:
  - Games/DevilsPlayground/Source/DP_Tuning.h (create)
  - Games/DevilsPlayground/Source/DP_Tuning.cpp (create)
  - Games/DevilsPlayground/DevilsPlayground.cpp (insert call to
    DP_Tuning::Initialize() in InitializeResources hook;
    insert call to DP_Tuning::Shutdown() in CleanupResources)

DO NOT run MSBuild, Tools/run_dp_tests.ps1, Sharpmake_Build.bat, or
the game. DO NOT edit Docs/* or Config/*. The orchestrator handles
all build/test operations.

API spec:
  namespace DP_Tuning {
    void Initialize();   // loads Config/Tuning.json, caches values
    void Shutdown();
    template<typename T> T Get(const char* szKey);  // specialise for float/int/bool/string
    // Errors: assert if key missing; that's the failure mode we want.
  }

**No engine JSON utility exists** (verified by Researcher subagent above —
no Zenith_JsonReader, no Zenith_Json* file anywhere under Zenith/).
Write a minimal hand-coded parser: read the file as text, strip
//-style comments, walk tokens, build a flat dotted-key map keyed on
strings. ~80-120 LOC. Skip `_comment` keys (Tuning.json uses
underscore-prefixed comment fields).

Follow Zenith conventions: no std::function/vector/mutex; use
Zenith_Vector for any vector needs; all .cpp starts with
#include \"Zenith.h\".

Report: changed files + one-paragraph summary."

Implementer returns:
  Files changed: Source/DP_Tuning.h, Source/DP_Tuning.cpp,
                 DevilsPlayground.cpp.
  Summary: header declares the namespace; .cpp parses Config/Tuning.json
  on Initialize via minimal hand-coded parser (~90 LOC), walks the tree,
  flattens to dotted-key map cached in a Zenith_Vector<KVPair>; Get<float>
  dispatches via linear search (small N).

Orchestrator: git diff. All three files. Conventions followed; no
std::function. Linear search is fine for our size (~80 keys).
```

### Verify green

```
Orchestrator acquires lock. Sharpmake. MSBuild → success.
Tools/run_dp_tests.ps1 -Filter Tuning -Headless → 1 test, pass.
Tools/run_dp_tests.ps1 -Headless → 35/35 tests, all pass (the 34 existing
+ the new Test_P1Tuning_LoadsAndValuesInBand).
Release lock.
```

### Review (subagent dispatch)

```
Orchestrator: spawn Reviewer (foreground).

Prompt:
"Review the staged changes for MVP-0.1.1 against the §5.8 pre-flight
checklist in AgentBriefing.md.

Files in scope: git diff master --stat shows:
  Source/DP_Tuning.h (new, ~30 lines)
  Source/DP_Tuning.cpp (new, ~120 lines)
  DevilsPlayground.cpp (+4 lines)
  Tests/Test_P1Tuning_LoadsAndValuesInBand.cpp (new, ~80 lines)

DO NOT edit any files. DO NOT build or run the game.

Read the diff and check:
  (a) No std::function/vector/mutex.
  (b) All .cpp files start with #include \"Zenith.h\".
  (c) Test code wrapped in #ifdef ZENITH_INPUT_SIMULATOR.
  (d) No post-MVP scope (e.g., no archetype/reagent/audio code).
  (e) No obvious bugs (e.g., uninitialised data, missing null checks).

Report: any failed items + one-paragraph quality assessment."

Reviewer returns: "All five checks pass. Code is tight. One nit: the
linear search in Get<T> could use binary search if keys were sorted
at Initialize time, but at N=80 it's irrelevant. No blockers."

Orchestrator: nit acknowledged but post-MVP optimisation; no change.
```

### Commit, PR, merge

```
Orchestrator:
  git add -A
  git commit -m "feat(dp): MVP-0.1.1 — DP_Tuning system + first tuning test"
  git push -u origin dp/mvp-0.1.1
  gh pr create --title ... --body "$(cat <<'EOF' ... EOF)"  (per §3.4 template)
  gh pr merge --auto --squash --delete-branch
```

### Update state

```
Orchestrator updates Docs/Status.md:
  Current task: MVP-0.1.2 (migrate DPVillager_Behaviour to read from
  DP_Tuning).

Orchestrator appends to Docs/DecisionLog.md:
  2026-05-13 — Wrote a minimal hand-coded JSON parser (~90 LOC) — no engine
  JSON utility exists (verified via directory scan of Zenith/Core,
  Zenith/AssetHandling, Zenith/FileAccess). Linear-search Get<T> chosen
  for simplicity; binary-search optional post-MVP.
  Test_P1Tuning_LoadsAndValuesInBand is the regression guard.

Orchestrator ticks MvpRoadmap.md task MVP-0.1.1.

Orchestrator commits these doc updates in a follow-up small commit:
  chore(dp): MVP-0.1.1 — update Status / DecisionLog / Roadmap
```

### Total session

- Wall time: ~3.5 hours (within plumbing-task budget §4.8 of AgentBriefing).
- Subagents spawned: 5 (1 Planner, 1 Researcher, 1 Test Author, 1 Implementer, 1 Reviewer).
- Build invocations: 2 (red, then green).
- Test invocations: 2 (filtered, full).
- Build lock acquired/released: 2 paired pairs.
- PRs opened/merged: 1 main + 1 doc-update.

Next session starts by reading Status.md → continues at MVP-0.1.2.

---

## 9. End of Session

When the orchestrator winds down:

1. **Release the build lock** (defensive cleanup even if you released after each build).
2. **Make sure all branches are pushed or cleaned up.** No orphaned local branches.
3. **Status.md should reflect the next session's starting state.**
4. **Questions.md ⚠️ items addressed or moved to Resolved.**
5. **DecisionLog.md updated for every non-trivial decision.**
6. **Print a one-paragraph user-facing summary** of the session's accomplishments. The user reads this when they next look at their terminal.

---

## 10. Quick Reference Card

| What | Who does it |
|---|---|
| Read Status.md / Questions.md / MvpRoadmap.md | Orchestrator |
| Codebase research (multi-file) | Researcher subagent |
| Multi-file refactor planning | Planner subagent |
| Write code | Implementer subagent |
| Write tests | Test Author subagent |
| Review PR before merge | Reviewer subagent |
| Mechanical fixups | Linter subagent |
| Doc body changes (GDD, TestPlan, etc.) | Doc Maintainer subagent |
| **Build (MSBuild)** | **Orchestrator only** |
| **Run tests (run_dp_tests.ps1)** | **Orchestrator only** |
| **Run the game (devilsplayground.exe)** | **Orchestrator only** |
| Acquire / release build lock | Orchestrator |
| Sharpmake regen after .cpp adds | Orchestrator |
| Run `gh pr create` / `gh pr merge` | Orchestrator |
| Write Docs/Status.md / Questions.md / DecisionLog.md / MvpRoadmap.md | Orchestrator |
| Write Config/*.json | Orchestrator |

---

## 11. When to Break the Pattern

The orchestrator pattern is the default but not the only option. Consider falling back to **solo sequential** (Mode A in [AgentBriefing.md §6.1](AgentBriefing.md)) when:

- The task is trivial (typo fix, single-line config tweak). Subagent dispatch overhead exceeds the work.
- You're debugging a flaky test. Single-agent linear investigation is faster than coordination.
- The user is actively present and prefers direct dialogue. Subagent prompts add latency.

When in doubt: default to the orchestrator pattern; it scales better, and the small-task overhead is acceptable.
