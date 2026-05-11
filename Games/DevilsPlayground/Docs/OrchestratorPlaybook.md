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

1. Read `Games/DevilsPlayground/Docs/Status.md` — current task, build state, notes for next agent.
2. Read `Games/DevilsPlayground/Docs/Questions.md` — surface ⚠️ items the user may have answered between sessions.
3. Read the next un-checked task in `Games/DevilsPlayground/Docs/MvpRoadmap.md`.
4. Decide which subagents the task needs (see §3 catalogue).
5. Acquire the build lock (see §4) if you'll be building.
6. **Verify the baseline.** Quick `git status` to confirm clean tree on master. If dirty, investigate before doing anything.

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

- *"Files you may edit: [exhaustive list]."*
- *"DO NOT run `MSBuild`, `Tools/run_dp_tests.ps1`, `Sharpmake_Build.bat`, or the game executable. The orchestrator handles all build/test/run operations."*
- *"DO NOT modify Docs/Status.md, Docs/MvpRoadmap.md, Docs/DecisionLog.md, Docs/Questions.md, or anything in `Config/`. The orchestrator manages these."*
- *"Follow Zenith conventions: no std::function / std::vector / std::mutex. Use Zenith_Vector, Zenith_Mutex, function pointers. All .cpp files start with `#include \"Zenith.h\"`. Tests wrapped in `#ifdef ZENITH_INPUT_SIMULATOR`."*
- *"Report back: list of files changed + a one-paragraph summary."*

**Example prompt:**

> *"Implement the Apprehend BT branch for MVP-1.3.3. Files you may edit: only `Games/DevilsPlayground/Components/Priest_Behaviour.h` (add the branch construction in OnStart), `Games/DevilsPlayground/Components/DP_BT_Nodes.h` (add `DP_BTCondition_InApprehendRange` and `DP_BTAction_ApprehendChannel`), `Games/DevilsPlayground/Source/PublicInterfaces.h` (add the `DP_OnRunLost` event struct), and `Games/DevilsPlayground/Source/PublicInterfaces.cpp` (event implementation if needed). [Plus the no-build-or-run clauses.] Report: changed files + summary."*

### 3.4 Test Author (subagent_type: `general-purpose`)

**Use when:** you need a specific test authored. Often spawned BEFORE the implementer, so the failing test guides the implementation.

**Same restrictions as Implementer.** The "files you may edit" list typically includes only one path under `Games/DevilsPlayground/Tests/`.

**Example prompt:**

> *"Author `Test_P1Apprehend_PriestCatchesPlayer`. Files you may edit: only `Games/DevilsPlayground/Tests/Test_P1Apprehend_PriestCatchesPlayer.cpp` (create). The test pattern is the 7-phase state machine from `Games/DevilsPlayground/Docs/TestPlan.md` §2.3. Setup an authored test scene with priest at (0,0,0) and villager at (1,0,0); subscribe to a new `DP_OnRunLost` event (struct already exists in PublicInterfaces.h); possess the villager, pin it (no input), wait 180+ frames; assert `DP_OnRunLost` dispatched exactly once. Max frames 240. [Plus the no-build-or-run clauses.]"*

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

### 4.1 Acquire the lock

```powershell
# PowerShell — wait up to 60 s for the lock; abort if held longer.
$lockFile = "C:\dev\Zenith\Build\.build_lock"
$timeout = 60
$waited = 0
while (Test-Path $lockFile) {
    if ($waited -ge $timeout) {
        Write-Error "Build lock held for > $timeout s; abort. Check for runaway subagent."
        exit 1
    }
    Start-Sleep -Seconds 2
    $waited += 2
}
New-Item -Path $lockFile -Value "$PID-$(Get-Date -Format o)" -Force | Out-Null
```

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

```powershell
Remove-Item "C:\dev\Zenith\Build\.build_lock" -ErrorAction SilentlyContinue
```

**Always release in a finally-equivalent.** Use try/finally in PowerShell:

```powershell
try {
    # build/test commands here
} finally {
    Remove-Item "C:\dev\Zenith\Build\.build_lock" -ErrorAction SilentlyContinue
}
```

### 4.5 Stale lock cleanup

If the orchestrator crashes mid-build, the lock leaks. Defensively at session start:

```powershell
# Drop locks older than 30 minutes — assume crashed orchestrator.
$lockFile = "C:\dev\Zenith\Build\.build_lock"
if (Test-Path $lockFile) {
    $age = (Get-Date) - (Get-Item $lockFile).LastWriteTime
    if ($age.TotalMinutes -gt 30) {
        Remove-Item $lockFile -Force
        Write-Warning "Stale build lock removed (age: $($age.TotalMinutes) min)."
    }
}
```

The orchestrator runs this at the start of every session.

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
6. Orchestrator commits, pushes, PRs, updates state files.
```

Best for: well-scoped MvpRoadmap tasks. Most MVP work fits here.

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

## 8. Worked Example — Orchestrating MVP-0.1.1

Concrete reference for the first MVP task, executed through the orchestrator pattern.

### Setup

```
Orchestrator reads:
  - Docs/Status.md → "Current task: MVP-0.1.1"
  - Docs/Questions.md → no open items
  - Docs/MvpRoadmap.md → MVP-0.1.1 detail

Orchestrator runs (PowerShell):
  - Stale lock cleanup (§4.5)
  - git checkout master && git pull
  - git checkout -b dp/mvp-0.1.1
```

### Plan (subagent dispatch)

```
Orchestrator: spawn Planner (foreground).

Prompt:
"Design the implementation plan for MVP-0.1.1 — DP_Tuning system.
Goal: load Config/Tuning.json at startup, expose DP_Tuning::Get<T>(key)
accessor. Existing JSON-parsing utilities should be checked first in
Core/. Constraints: no std::function/vector/mutex. Files likely
needed: new Source/DP_Tuning.h, new Source/DP_Tuning.cpp,
modification to DevilsPlayground.cpp's InitializeResources hook.
Return: step list (max 8 steps), files to touch, risks, test ideas.
~400 words."

Planner returns (summary):
  1. Verify Core/Zenith_JsonReader exists; if not, use minimal parser.
  2. Author DP_Tuning.h declaring namespace and Get<T>(key) accessor.
  3. Author DP_Tuning.cpp implementing parse + cache.
  4. Hook DP_Tuning::Initialize() into InitializeResources.
  5. Run Sharpmake (new .cpp file).
  6. Author Test_P1Tuning_LoadsAndValuesInBand.
  7. Build + run.
  8. Risk: JSON parser may not exist. Fall back to hand-coded parser
     (we only need a flat-dotted-key reader).
```

### Research (one quick subagent)

```
Orchestrator: spawn Researcher (foreground).

Prompt:
"Find any existing JSON-parsing utility in C:\dev\Zenith\Core\ or
adjacent. Return the API and a file:line pointer if found; report
'none found' if not. ~100 words. No code changes."

Researcher returns: "Core/Zenith_JsonReader.h provides ParseFromFile
returning a tree. Sufficient for our needs."
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

### Verify red

```
Orchestrator acquires lock. Sharpmake. MSBuild. Compile fails:
"'DP_Tuning' was not declared in this scope" — expected (we haven't
implemented it yet). Release lock. Red confirmed.
```

### Implementer (subagent dispatch)

```
Orchestrator: spawn Implementer (foreground).

Prompt:
"Implement DP_Tuning for MVP-0.1.1.

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

Reference Core/Zenith_JsonReader for parsing.

Follow Zenith conventions: no std::function/vector/mutex; use
Zenith_Vector for any vector needs; all .cpp starts with
#include \"Zenith.h\".

Report: changed files + one-paragraph summary."

Implementer returns:
  Files changed: Source/DP_Tuning.h, Source/DP_Tuning.cpp,
                 DevilsPlayground.cpp.
  Summary: header declares the namespace; .cpp parses Config/Tuning.json
  on Initialize, walks the tree, flattens to dotted-key map cached in
  a Zenith_Vector<KVPair>; Get<float> dispatches via linear search
  (small N).

Orchestrator: git diff. All three files. Conventions followed; no
std::function. Linear search is fine for our size (~80 keys).
```

### Verify green

```
Orchestrator acquires lock. Sharpmake. MSBuild → success.
Tools/run_dp_tests.ps1 -Filter Tuning -Headless → 1 test, pass.
Tools/run_dp_tests.ps1 -Headless → 29/29 tests, all pass.
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
  2026-05-13 — Used Core/Zenith_JsonReader rather than adding a JSON
  dependency. Rationale: existing infra; no new third-party code.
  Linear-search Get<T> chosen for simplicity; binary-search optional
  post-MVP. Test_P1Tuning_LoadsAndValuesInBand is the regression
  guard.

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
