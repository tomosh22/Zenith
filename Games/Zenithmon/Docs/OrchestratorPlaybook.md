# Zenithmon -- Orchestrator Playbook

**Document purpose:** The operating manual for the **orchestrator** role in
multi-agent Zenithmon sessions. The primary Claude Code instance reads this
file and acts as orchestrator -- dispatching subagents for research / code /
test / review work while owning the serial build/test gate and the living docs
itself.

**Audience:** The orchestrating Claude instance. Subagents do NOT read this
file -- they receive scoped, self-contained briefings from the orchestrator.

**Companion docs:** [AgentBriefing.md](AgentBriefing.md) (general conventions;
you and your subagents inherit them) - [Roadmap.md](Roadmap.md) (your task
queue) - [Status.md](Status.md) (session state).

These are the DevilsPlayground governance invariants carried over verbatim in
spirit; DP proved them over ~150 PRs. Do not relitigate them.

---

## 0. The Invariants

Every action in an orchestrated session must preserve these. They are hard
rules, not preferences.

### Invariant 1 -- Serial MSBuild; the orchestrator owns all builds

**Only the orchestrator invokes MSBuild, `zenith build`, `zenith test`,
`zenith regen` / `Build\regen.ps1`, or the game executable. Subagents author
code and tests but NEVER build, test, or run.**

Why: parallel agents thrash MSBuild -- mspdbsrv and output-dir locks force
sequential dispatch. Two concurrent builds corrupt each other's PDBs and leave
hanging `cl.exe` processes that lock files for every later build.

Mechanically enforced by:
- Every subagent prompt includes an explicit **"DO NOT BUILD, TEST, OR RUN"**
  clause as its first constraint.
- The orchestrator is a single thread; foreground subagents block it, and
  background subagents are barred from the build chain by their prompts.
- If a build wedges anyway: `zenith clean Zenithmon` kills hung
  cl.exe/mspdbsrv/link processes and wipes output/obj.

### Invariant 2 -- No git worktrees

**All work happens on the main checkout at `C:\dev\Zenith`, on per-task
feature branches (`zenithmon/s<stage>-<slug>`).**

Sharpmake bakes the cwd's absolute path into generated vcxprojs
(`GAME_ASSETS_DIR`, `SHADER_SOURCE_ROOT`, post-build copy steps), so a regen
run inside a worktree produces projects that break every other checkout.
If the harness places you in `.claude/worktrees/<name>/`, treat it as a
transient sandbox: never run Sharpmake/regen there and never commit generated
output from there.

### Invariant 3 -- Single source of truth for the living docs

**[Status.md](Status.md), [Roadmap.md](Roadmap.md),
[DecisionLog.md](DecisionLog.md), [Questions.md](Questions.md),
[Shortfalls.md](Shortfalls.md) are written by the orchestrator only.**
Subagents may read them; they never edit them. A subagent that wants a
decision logged or a checkbox ticked **returns the proposed text** in its
report; the orchestrator applies it. This gives race-free write semantics and
one consistent narrative per session.

### Invariant 4 -- Never trust "builds clean"

**A subagent's claim that its code compiles, tests pass, or "builds clean" is
worthless -- subagents cannot build (Invariant 1), so any such claim is
fabricated.** After integrating any subagent's output the orchestrator itself
runs `zenith build Zenithmon` and the relevant `zenith test Zenithmon` gates
before committing. This holds even when the claim is plausible.

### Invariant 5 -- Inline item data into subagent prompts

**Subagent prompts are self-contained.** Paste the actual spec, data rows,
formulas, code snippets, and file paths into the prompt; do not say "see the
plan" or "read Docs/X.md section 3" as the primary carrier of requirements.
Subagents start with no session context; external references get skimmed,
misread, or ignored, and a doc pointer costs the subagent an unbounded
exploration detour. Pointers are fine as *supplements* for background --
never as the load-bearing task definition.

---

## 1. Session Start

### 1.1 First five minutes

1. Read [ManualSetupChecklist.md](ManualSetupChecklist.md). If any box is
   unticked (e.g. the `zm-tests` required-check registration, which is a
   manual GitHub-UI step), STOP work that depends on it, log a
   [Questions.md](Questions.md) entry, and pick a task that does not.
2. Read [Status.md](Status.md) -- build state, current task, notes.
3. Read [Questions.md](Questions.md) -- the user may have answered items
   between sessions.
4. Read the current stage in [Roadmap.md](Roadmap.md); pick the first
   un-checked task (or trust Status.md's current task if one is in flight).
5. `git status` -- confirm a clean tree; investigate if dirty.
   `git checkout master && git pull`, then branch `zenithmon/s<stage>-<slug>`.
6. If any `.zproj` / `Sharpmake_*.cs` changed since the last regen:
   `Build\regen.ps1`.

### 1.2 The orchestrator's mental model

You are a tech lead. You do not write all the code yourself; you scope tasks
for subagents, integrate their work, run the build/test gate, open the PR,
and keep the docs truthful.

- Read enough to understand the task; do not over-research.
- Brief subagents tightly, with an explicit *files-you-may-edit* list.
- Build and test yourself; subagents return code, not results.
- Write the living docs yourself; subagents return content, not commits.
- Open PRs yourself; subagents never invoke `gh`.

---

## 2. The Standard Task Loop

```
READ STATE      (Status.md, Questions.md, Roadmap task)
     |
PLAN            (Planner subagent if multi-file / unclear shape)
     |
RESEARCH        (Explore subagent if the codebase question is multi-step)
     |
AUTHOR TESTS    (Test Author subagent -- TestPlan.md specs, same PR as the system)
     |
AUTHOR CODE     (Implementer subagent)
     |
ORCHESTRATOR:   zenith regen (if files added) -> zenith build Zenithmon
     |          -> zenith test Zenithmon --headless (+ scoped windowed runs)
     |
REVIEW          (Reviewer subagent for logic/engine changes)
     |
ORCHESTRATOR:   commit, push, gh pr create, auto-merge on green
     |
ORCHESTRATOR:   update Status.md / DecisionLog.md / tick Roadmap.md
```

The orchestrator is in the loop at every step; subagents fill specific boxes.
Tests and implementation land in the same PR -- test-first discipline lives
*within* the session (author test, watch it fail, implement, watch it pass),
not across PR boundaries.

---

## 3. Subagent Role Catalogue

Brief every subagent like a new colleague: the goal, the relevant context
(inlined -- Invariant 5), the exact write scope, the success criterion, and
the standard clauses below.

**Mandatory clauses in every code-authoring subagent prompt:**

- "Files you may edit: [exhaustive list]." Scope may include
  `Games/Zenithmon/` and, for approved engine tasks, specific `Zenith/` paths.
- "DO NOT run MSBuild, zenith build/test/regen, Sharpmake, or any game
  executable. The orchestrator handles all build/test/run operations."
- "DO NOT edit Docs/Status.md, Docs/Roadmap.md, Docs/DecisionLog.md,
  Docs/Questions.md, or Docs/Shortfalls.md. Return proposed doc text in your
  report instead."
- "Conventions: ZM_ prefix for all game code; no std:: containers
  (Zenith_Vector / Zenith_HashMap / Zenith_Mutex; std::function -> function
  pointers); every .cpp starts with #include \"Zenith.h\"; tabs;
  braces on new lines; tools code in #ifdef ZENITH_TOOLS; automated tests in
  #ifdef ZENITH_INPUT_SIMULATOR; Zenith_Assert (camelCase) is the real assert."
- "Report back: list of files changed + a one-paragraph summary + any proposed
  doc updates."

### 3.1 Researcher (`Explore`)

Multi-file / multi-pass codebase questions. Read-only by tool definition.
Not for a single known-file read -- use Read/Grep directly (subagent launch
tax is real).

### 3.2 Planner (`Plan`)

Multi-file refactors or new architecture where you want a step list + risks
before committing. Read-only.

### 3.3 Implementer (`general-purpose`)

Bounded code authoring. Full mandatory clauses. Inline the spec: for a data
task, paste the table schema and example rows; for a battle mechanic, paste
the exact formula and the TestPlan vectors it must satisfy.

### 3.4 Test Author (`general-purpose`)

Authors the tests for a system, usually dispatched before or alongside the
Implementer. Write scope is typically one new TU under
`Games/Zenithmon/Tests/`. Inline the test contract (setup, steps, asserts,
max frames, RequestSkip conditions) from [TestPlan.md](TestPlan.md) -- do not
just cite the section.

### 3.5 Reviewer (`general-purpose`, prompt-enforced read-only)

"You are reviewing, not implementing. Do not edit any files. Do not build or
run." Paste the diff into the prompt. Mandatory for PRs that change game
logic and for **every** engine (`Zenith/`) change. Checks: tests match the
change, conventions (3.x clauses), no scope creep vs [Scope.md](Scope.md),
no baked assets staged, no obvious bugs.

### 3.6 Doc Maintainer (`general-purpose`, doc-only scope)

Reference-doc bodies only ([GameDesignDocument.md](GameDesignDocument.md),
[TestPlan.md](TestPlan.md), [AssetManifest.md](AssetManifest.md),
[Glossary.md](Glossary.md), ...). Never the orchestrator-owned living docs,
never source code.

---

## 4. The Build/Test Gate (orchestrator only)

All from repo root, PowerShell:

```
zenith regen                                 # only if files were added/removed
zenith build Zenithmon                       # Vulkan_vs2022_Debug_Win64_True
zenith test Zenithmon --headless             # full batch gate
zenith test Zenithmon --filter <Test>        # scoped / windowed iteration
zenith clean Zenithmon                       # recovery: hung cl.exe / locked pdbs
```

Fallback: `pwsh -File Tools\zenith.ps1 <verb> ...`. msbuild direct:
`/t:Zenithmon` on `Games\Zenithmon\zenithmon_win64.sln`, never the whole sln.
Test exit codes: 0 OK / 1 usage / 2 validation / 3 generation /
4 build-or-test / 5 not-found. The old `Tools/run_*_tests.ps1` runners were
deleted (`c29e28f8`); `zenith test` is the only harness.

Gate discipline:

- **Rebuild after every subagent integration** (Invariant 4), then run at
  least the tests scoped to the change; run the full headless batch before
  any commit.
- **Stage gates** additionally run the 4-config matrix
  (Vulkan Debug/Release x True/False) + the `D3D12_vs2022_Debug_Win64_False`
  link proof, the stage's windowed `--filter` list, and the visual check --
  see [Roadmap.md](Roadmap.md) per stage.
- **Engine changes**: unit tests for the new surface + RenderTest boot
  regression + DP and CityBuilder suites green before the PR.
- One orchestrated session per machine at a time. There is no cross-session
  build lock; do not start a second building session.

---

## 5. Dispatch Patterns

### 5.1 Single-implementer (most tasks)

1. Orchestrator reads the Roadmap task; inlines the spec.
2. Test Author subagent authors the failing tests.
3. Orchestrator builds + runs them -- confirms they fail for the right reason.
4. Implementer subagent authors the code.
5. Orchestrator builds + runs the full gate.
6. Reviewer subagent for logic/engine changes.
7. Orchestrator commits, PRs, updates docs.

### 5.2 Plan-first (multi-file / unclear shape)

Planner subagent returns a step list + risks; the orchestrator reviews (and
logs the design in [DecisionLog.md](DecisionLog.md)); then 5.1 per step.

### 5.3 Parallel-author (independent files, authoring only)

N subagents author N **non-overlapping** file sets in the background; the
orchestrator waits, integrates, then builds and tests ONCE, serially.
Parallelism is code-authoring, never builds. Partition write surfaces by
directory or file; two background agents writing the same file corrupts it.
Default to 1-2 background subagents; when in doubt, sequential.

### 5.4 Review-loop (engine + high-stakes changes)

5.1 to green, then a mandatory Reviewer pass, then fix-or-commit. Engine PRs
never skip this.

### 5.5 Content waves (S9/S10 world buildout)

The world-buildout stages are content-heavy: dozens of scenes defined as
`ZM_WorldSpec` rows + terrain/scene recipes + traversal tests, not bespoke
code. The dispatch pattern:

1. **Partition by region/system** -- one subagent per region (e.g. "Town 4 +
   Routes 6-7 + Gym 3") or per cross-cutting system (e.g. weather zones).
   Region partitions must not share files.
2. **Inline the region brief** (Invariant 5): paste the region's world-table
   entries from the GDD (scene names, build indices, connections, spawn tags,
   encounter species/rates, trainers, shops), the recipe conventions, and the
   traversal-test contract into each prompt.
3. **Each subagent authors:** its WorldSpec rows, its terrain/scene recipe
   code, and its traversal/encounter tests -- in its own files where possible.
4. **The shared WorldSpec table stays single-writer:** subagents return their
   rows for any shared TU as text blocks; the orchestrator splices them in
   itself (same rule as the living docs).
5. **Integrate serially:** apply one region's output -> `zenith build
   Zenithmon` -> run the WorldSpec integrity unit tests (every warp target,
   spawn tag, species, trainer resolves) -> run that region's traversal tests
   -> commit -> next region. Never integrate two regions between builds; when
   a region breaks integrity you want the diff that did it to be one region
   wide.
6. **Per-wave gate:** after all regions in the wave: full headless batch,
   bake-determinism check (re-run the tools boot -> zero diffs), stage-gate
   docs.

---

## 6. Integrating Subagent Output

Subagent edits are already on disk when they report. Your job: verify,
integrate, gate, commit.

1. **`git diff` and actually read it.** Do not trust the summary -- read the
   code (Invariant 4 applies to correctness claims, this step applies to
   content claims).
2. **Check scope.** Only the files you listed? If not, `git restore` the
   out-of-scope edits and re-brief.
3. **Regen if files were added**, build, test -- yourself.
4. **Apply proposed doc text** to the living docs yourself (Invariant 3).
5. **On file conflicts** (two background agents touched the same file):
   `git checkout HEAD -- <file>`, then re-dispatch one at a time, foreground.

---

## 7. Anti-patterns

- **Forgetting the no-build clause.** A subagent that reports "tests pass"
  either fabricated it or violated Invariant 1. Re-brief; the clause is the
  first paragraph of every prompt.
- **Trusting "builds clean".** The commit that follows an unverified claim is
  the one CI bounces -- or worse, the one auto-merge lands red-adjacent.
  Always rebuild yourself.
- **Doc-pointer prompts.** "Implement encounter rolls per the plan" produces
  a subagent that invents its own plan. Paste the formula, the rates table,
  the test vectors.
- **Letting a subagent write Status/Roadmap/DecisionLog.** Now the docs
  disagree with the orchestrator's view and the next session inherits the
  confusion.
- **Over-parallelising.** Five background authors, one corrupted file, thirty
  minutes of merge archaeology. 1-2 at a time, disjoint scopes.
- **Spawning a subagent for a single Read/Grep.** Launch tax exceeds the work.
- **Building the whole solution or running Sharpmake in a worktree.** Both are
  standing engine-repo rules (see [AgentBriefing.md](AgentBriefing.md) section 6).
- **Committing baked assets or generated build files.** Both are git-ignored
  by design; if `git status` shows them staged, a scope leak happened.

---

## 8. End of Session

1. All branches pushed or cleaned up; no orphaned local WIP.
2. [Status.md](Status.md) reflects the next session's true starting state.
3. [Questions.md](Questions.md) items addressed or updated.
4. [DecisionLog.md](DecisionLog.md) has an entry per non-trivial decision.
5. Roadmap checkboxes match merged reality.
6. Print a one-paragraph user-facing summary.

---

## 9. Quick Reference Card

| What | Who |
|---|---|
| Read Status / Questions / Roadmap | Orchestrator |
| Multi-file codebase research | Researcher subagent |
| Refactor / architecture planning | Planner subagent |
| Write code | Implementer subagent |
| Write tests | Test Author subagent |
| Review diffs (logic + all engine changes) | Reviewer subagent |
| Reference-doc bodies (GDD, TestPlan, ...) | Doc Maintainer subagent |
| **zenith build / test / regen / run** | **Orchestrator only** |
| **msbuild / game executable** | **Orchestrator only** |
| Splice shared-table rows (ZM_WorldSpec) | Orchestrator |
| Write Status / Roadmap / DecisionLog / Questions / Shortfalls | Orchestrator |
| gh pr create / merge | Orchestrator |
| git worktrees | Nobody |

When the session is trivial (typo, one-line fix), skip the subagent machinery
and work solo -- the invariants still hold (you are then both roles, and you
still build serially, on the main checkout, with the docs updated).

## The lifecycle loop (unattended operation)

StartPrompts.md prompt 0 turns this playbook into an unattended driver: each
firing is ONE idempotent iteration (derive state from Status.md + git; resume
in-flight work, else take the next Roadmap task; land it through PR -> CI ->
merge -> docs). The invariants above hold unchanged inside the loop -- the
iteration IS the orchestrator.

Loop-specific rules:

1. **Idempotence over memory.** A firing may crash or be interrupted at any
   point; the next firing must reconstruct from Status.md, the Roadmap, and
   git state alone. That is why Status.md is refreshed at every iteration end
   and why branches are named `zenithmon/s<stage>-<slug>` (discoverable).
2. **Hard-stop at visual gates** (user's standing order, 2026-07-10): when the
   next item is a stage's visual check (incl. S4 gallery, S8 go/no-go), run the
   automated gate items, capture screenshot evidence, set `GATE-WAIT: S<n>` in
   Status.md, and end. Every subsequent firing reports "waiting" and does
   nothing until the user's sign-off (StartPrompts prompt 4) lands in
   DecisionLog.md. The loop never signs its own gates.
3. **Merge authorization is carried by the prompt.** The user issues it by
   starting the loop; it covers merging the loop's OWN green PRs only. Anything
   else outward-facing (branch-protection changes, deleting others' branches,
   force-pushing shared refs) still stops for the user.
4. **Fix-forward budget:** 3 attempts on a red gate, then park it in
   Questions.md + Status.md and end the iteration. A parked blocker is a
   better outcome than a thrashing loop.
5. **Process hygiene:** each iteration sweeps stray zenithmon.exe processes
   (Get-Process zenithmon) before ending -- orphaned editor instances from
   crashed runs otherwise accumulate and lock build outputs.
6. **Budget sanity:** one Roadmap task per iteration is the healthy cadence;
   chain a second small task only when the first merged green with budget to
   spare. Content-wave stages (S9/S10) fan out subagents per region for
   AUTHORING only -- builds remain serial on the orchestrator.
