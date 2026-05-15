# Devil's Playground — Autonomous Agent Briefing

**Document purpose:** The onboarding doc every autonomous Claude Code agent reads at the start of every DP session. Self-contained. If you've never touched this project before, read this and you'll know how to act.

**Audience:** Future Claude Code instances (you, in two days, will not remember this conversation).
**Status:** Living document — update it whenever conventions evolve.

---

## 0. TL;DR

You are working on **Devil's Playground**, a stealth-puzzle roguelite. The user (a full-time solo developer named Tomos) ratified the production plan on 2026-05-11 and wants **autonomous execution**.

### What to do first (every session)

1. `cd C:\dev\Zenith` and `git checkout master && git pull` (work on master; per OrchestratorPlaybook Invariant 2 no worktrees — if the harness drops you into `.claude/worktrees/<name>/`, treat it as a transient sandbox and remember Sharpmake bakes the cwd absolute path into generated vcxprojs).
2. Read `Games/DevilsPlayground/Docs/Status.md` — tells you the current build state and the in-flight task.
3. Read `Games/DevilsPlayground/Docs/Questions.md` — surface any blockers; address them or skip them if the user hasn't responded.
4. Read `Games/DevilsPlayground/Docs/MvpRoadmap.md` — pick the **first un-checked task**.
5. Execute. Test-first.
6. End the session by updating Status.md, appending to DecisionLog.md, and ticking the task in MvpRoadmap.md.

### Never do

- Synchronously block on user input. Surface in Questions.md; continue with a different task.
- Spend money. Default: zero external service spend. Anything that costs cash goes in Questions.md.
- Touch master directly. Always branch (`dp/mvp-<task-id>`), PR, auto-merge on green.
- Skip the test-first step. The test suite is the quality contract.
- Add post-MVP scope. If tempted, stop. Log the temptation in DecisionLog.md as a post-MVP candidate.

---

## 1. The Project in One Page

- **Game:** Devil's Playground. Top-down stealth-puzzle roguelite. You play a bodiless demon possessing villagers in a 1670s English village to collect 5 reagents and complete a ritual at a pentagram while a witch-finder hunts you. Each possessed body has a 30-second life timer.
- **Engine:** Zenith (custom C++20, Vulkan-based). Located at `C:\dev\Zenith\`. The DP project lives at `C:\dev\Zenith\Games\DevilsPlayground\`.
- **Status (2026-05-15):** Phase 1 + Phase 2 substantively complete; Phase 4 loss-state UI shipped (PR #75). ~110 automated tests registered, full headless suite 0 failures. Loss states, navmesh, pause, archetype/reagent variety, HUD upgrades, and most engine instrumentation all landed. The live frontier is Phase 3 assets (Mixamo spike — HUMAN_GATE) and Phase 4.3.x bot-driven playthrough. See [Status.md](Status.md) for the live wave-by-wave breakdown.

### Document map (read in this order)

1. **[GameDesignDocument.md](GameDesignDocument.md)** — what the game is.
2. **[MVPScope.md](MVPScope.md)** — what we're building first. **The hard target.**
3. **[MvpRoadmap.md](MvpRoadmap.md)** — sequenced task list. **Your daily orders.**
4. **[Shortfalls.md](Shortfalls.md)** — the gaps. Reference when implementing a feature.
5. **[TestPlan.md](TestPlan.md)** — every gameplay test in the suite. Reference when authoring tests.
6. **[AssetManifest.md](AssetManifest.md)** — what art deliverables look like.
7. **[AssetTestPlan.md](AssetTestPlan.md)** — how to verify placeholder assets.
8. **[OrchestratorPlaybook.md](OrchestratorPlaybook.md)** — *if you are the orchestrator of a fresh session, this is your primary operating manual. Read it instead of (well, in addition to) this one.*
9. **[ManualSetupChecklist.md](ManualSetupChecklist.md)** — one-time human pre-flight steps; orchestrator stops if any box unticked.
10. **[Glossary.md](Glossary.md)** — authoritative term definitions; consult when terminology is ambiguous.
11. **[BuildEnvironment.md](BuildEnvironment.md)** — required software, one-time setup, troubleshooting.
12. **[PeerReviews/](PeerReviews/)** — historical critiques of the planning pack. Read if you're investigating "why was X structured this way?"
13. **This file** — general conventions inherited by orchestrator and subagents alike.

### Always-loaded state files

- **`Docs/Status.md`** — read at session start.
- **`Docs/Questions.md`** — read at session start.
- **`Docs/DecisionLog.md`** — append-only; never read top-to-bottom unless investigating a specific past decision.
- **`Config/Tuning.json`** — every numerical value. Tests reference it.
- **`Config/Archetypes.json`** — villager designs.
- **`Config/Reagents.json`** — reagent designs.

### 1.1 Operating mode (read this before deciding *how* to work)

**The default operating mode for DP is Mode B — orchestrator + subagent team within a single Claude session.** The user explicitly chose this on 2026-05-11.

If you are reading this file as the *orchestrator* of a fresh session: **stop here and read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) instead.** That doc is the operating manual for your role. This file (AgentBriefing.md) covers general conventions you inherit; the playbook covers the orchestrator-specific protocols.

If you are reading this file as a *subagent* spawned by the orchestrator: stay here. This file's §3–§5 are your conventions. You do not invoke MSBuild, run tests, or run the game — the orchestrator handles all of those. You author files in the scope your spawner gave you, return a summary, and exit.

**Hard constraints (Invariants from [OrchestratorPlaybook.md](OrchestratorPlaybook.md) §0):**

1. **Serial game execution.** Only the orchestrator invokes `MSBuild` / `Tools/run_dp_tests.ps1` / the game executable. Subagents never do.
2. **No git worktrees.** All work on the main repo's checkout at `C:\dev\Zenith\` on `master` (with per-task feature branches).
3. **Single source of truth.** Only the orchestrator writes to `Docs/Status.md`, `Docs/Questions.md`, `Docs/DecisionLog.md`, `Docs/MvpRoadmap.md`, or `Config/*.json`.

§6 of this doc covers the three operating modes (A: solo sequential, B: orchestrator+subagents *(default)*, C: parallel multi-session *(post-MVP)*) in more depth.

---

## 2. The Session Loop

### 2.1 Start

```
1. cd C:\dev\Zenith
2. git checkout master
3. git pull
4. Read Docs/Status.md (look for "current task" line)
5. Read Docs/Questions.md (skim for ⚠️ items)
6. Open Docs/MvpRoadmap.md, find the first un-checked task
7. git checkout -b dp/mvp-<task-id>
```

### 2.2 Execute a task

A typical task in MvpRoadmap.md looks like:

```
- [ ] MVP-1.3.1 — Test_P1Apprehend_PriestCatchesPlayer (failing).
- [ ] MVP-1.3.2 — Add DP_OnRunLost event + cause enum.
- [ ] MVP-1.3.3 — Add Apprehend BT branch...
```

For each sub-task:

1. **Read** the surrounding tasks for context.
2. **Reference** the matching section in TestPlan.md, GDD, etc.
3. **Write the test first** if the task is "Test_X" or has an implicit test. Confirm it fails by running it.
4. **Implement** the smallest change that makes the new test pass.
5. **Run the full test batch:** `pwsh.exe -File Tools/run_dp_tests.ps1 -Headless`. All must pass.
6. **Commit** with a Conventional-Commits-style message: `feat(dp): MVP-1.3.1 — Test_P1Apprehend_PriestCatchesPlayer`.
7. **Open PR.** Use `gh pr create`. PR title = commit subject. Body = brief rationale + "Closes MVP-1.3.1".
8. **Auto-merge.** Set with `gh pr merge --auto --squash`. CI will merge when green.
9. **Tick the box** in MvpRoadmap.md as part of the PR (or as a follow-up).

### 2.3 End

```
1. Update Docs/Status.md (one-paragraph replace; mention current task)
2. Append to Docs/DecisionLog.md (one entry per non-trivial decision; see §4)
3. (If you raised one) Append to Docs/Questions.md
4. Commit those doc changes in the same PR if practical
5. Print a one-paragraph summary of the session to the user
```

---

## 3. Communication Conventions

### 3.1 Status.md

A single short living document. Format:

```markdown
# DP Status

**Last updated:** YYYY-MM-DD by agent <session-id>
**Build:** ✅ passing / ❌ broken (link to most recent CI run)
**Tests:** N/M passing

## Current task
MVP-1.3.3 — Add Apprehend BT branch to Priest_Behaviour.

## Last completed
MVP-1.3.2 — DP_OnRunLost event landed in PR #42.

## Notes for next agent
Briefly: anything not obvious from the diff. Empty if nothing.
```

Keep it to ~25 lines. Replace, don't append.

### 3.2 Questions.md

Append-only. For each blocker:

```markdown
## ⚠️ Q-2026-05-13-001 — Should we ship Mixamo placeholders as part of the game's repo or via a download script?

**Context:** AssetManifest.md §2.1 says we use Mixamo Y-Bot for villager placeholders. Two options:
- (A) Commit the FBX + imported .zmodel/.zskel/.zanim files to the DP repo. Pro: one-clone build. Con: ~80 MB repo bloat per character.
- (B) Add Tools/fetch_placeholders.ps1 that downloads from Mixamo via the user's credentials. Pro: keeps repo lean. Con: requires the user to log into Mixamo once.

**My best guess if you don't reply:** I'll go with (A) for now since it eliminates a setup step for fresh clones. I'll add a `Vendor/Mixamo/` directory and gitignore-exempt it. Will revisit at the 1 GB mark.

**Cost of getting it wrong:** moderate; we'd have to re-import + re-test all character assets. ~2 days of agent time to switch later.

**Status:** asked 2026-05-13. Acting on my best guess until you respond.
```

If the user replies, the next agent reads the answer and acts. Either move to a Resolved section or delete the entry.

### 3.3 DecisionLog.md

Append-only. Every non-trivial decision:

```markdown
## 2026-05-13 — MVP-1.3.3: chose collider-overlap for apprehend, not raycast.

**Decision:** Apprehend triggers on Jolt's collider-overlap event between priest's capsule and possessed villager's capsule. (Alternative considered: ray-cast from priest's eye every frame.)

**Why:** Collider-overlap is event-driven, doesn't burn frame budget when priest is far. Raycasts would add a per-frame physics query for the priest entity. We have one priest in MVP, so either works, but the collider path scales to hounds + multiple priests post-MVP.

**Test that prevents regression:** Test_P1Apprehend_PriestCatchesPlayer. Asserts apprehension within 4 frames of overlap.

**Reversibility:** easy. Both paths are localised in Priest_Behaviour::Update.
```

These accumulate. Future agents grep them when investigating "why is X this way?" Don't read the whole log every session.

### 3.4 Pull-request template

Every PR body uses this template (paste into `gh pr create --body "$(cat <<'EOF' ... EOF)"`):

```markdown
## What
<one-line summary>

## Why
Closes MVP-X.Y.Z (see [MvpRoadmap.md](MvpRoadmap.md)).
<link to the GDD / Shortfalls / TestPlan section that justifies the change, if relevant>

## Tests
- Added: `Test_<Name>` ([why it proves the change](#))
- All existing tests pass: `Tools/run_dp_tests.ps1 -Headless`
- New test runtime: <N> seconds at fixed-dt 60 Hz

## Decision log
<link to DecisionLog.md entry/entries added>

## Reviewer checklist (for self-review before auto-merge)
- [ ] No `std::function` / `std::vector` / `std::mutex` (use Zenith primitives)
- [ ] All new `.cpp` files start with `#include "Zenith.h"`
- [ ] Test-build code wrapped in `#ifdef ZENITH_INPUT_SIMULATOR`
- [ ] Tools-build code wrapped in `#ifdef ZENITH_TOOLS`
- [ ] Sharpmake regen committed if `.cpp` files were added
- [ ] No post-MVP scope crept in
```

PR titles use Conventional Commits: `feat(dp): MVP-X.Y.Z — short summary`. Types: `feat`, `fix`, `test`, `chore`, `docs`, `refactor`.

### 3.5 CI & auto-merge specifics

**CI provider:** GitHub Actions. Workflows live in `.github/workflows/`. **Phase 0.0 of the MvpRoadmap authored them** (MVP-0.0.2 + MVP-0.0.3). Required checks for auto-merge (verified live via `gh api repos/.../branches/master/protection`):

- `dp-build` — clean MSBuild of `vs2022_Debug_Win64_True` (MVP-0.0.2).
- `dp-tests` — `Tools/run_dp_tests.ps1 -Headless` exit code 0 (MVP-0.0.3; re-added in PR #15 after PR #14 unblocked SET_MODEL_MATERIAL).
- `complexity-gate` — Cyclomatic-complexity threshold.
- `doc-lint` — runs `Tools/doc_lint.ps1` (MVP-0.3.2, PR #24); not blocking auto-merge but always required to pass.
- `dp-asset-lint` — once MVP-3.5.1 lands, the asset linter pass.

**Auto-merge command:**

```bash
gh pr merge --auto --squash --delete-branch
```

(`--auto` queues; CI green triggers actual merge. `--squash` keeps history linear. `--delete-branch` cleans up.)

**If GitHub is unavailable** (rare; offline mode): commit on local feature branch, run `Tools/run_dp_tests.ps1 -Headless` to verify, leave a note in `Docs/Status.md` describing the un-pushed branch. Do *not* merge to master locally — wait for connectivity.

**If a required check name changes:** update `.github/workflows/` AND this section in the same PR. Drift here is silent and blocks all future PRs.

---

## 4. Operating Rules

### 4.1 Test-first is non-negotiable

The TestPlan.md TDD discipline is the **autonomous quality gate.** No human is reviewing your code mid-session. The test suite is.

For every functional change:
1. Test exists (write if not).
2. Test fails before your change.
3. Test passes after your change.
4. All other tests still pass.

If a PR changes behaviour without changing a test, **reject it yourself** — open a follow-up to add the test, then revert.

### 4.2 Scope discipline

The MVP target is **single playable level + all gameplay systems functioning**. Anything beyond that is post-MVP.

Common temptations to resist:
- "I should add the 5th archetype while I'm here." → No. 4 archetypes in MVP.
- "Let me write the audio emission code now." → No. The instrumentation hook is in MVP, the audio system itself is not.
- "Let me make the priest pursue more intelligently." → Only if a test demands it.
- "Let me add hounds to make this richer." → No. Hounds are post-MVP.

If you find yourself opening a file unrelated to the current task, stop. Open a follow-up issue in Questions.md or DecisionLog.md and return to the task.

### 4.3 Build & test commands

Always run from `C:\dev\Zenith` in PowerShell:

```powershell
# Build (use -maxCpuCount:1 per memory: parallel builds thrash MSBuild)
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Build\zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount:1

# Test (headless, fixed-dt)
pwsh.exe -File Tools/run_dp_tests.ps1 -Headless

# Test with filter (faster during dev)
pwsh.exe -File Tools/run_dp_tests.ps1 -Filter "Apprehend" -Headless

# Test single in per-process mode (for stubborn flakes)
pwsh.exe -File Tools/run_dp_tests.ps1 -Filter "Apprehend_PriestCatches" -PerProcess -Headless
```

### 4.4 Project hygiene

- After adding new `.cpp` files: run Sharpmake to regen the solution. `cd Build; cmd /c '.\Sharpmake_Build.bat < nul'`.
- Coding conventions: see CLAUDE.md root + memory/MEMORY.md. **No `std::function`, no `std::vector`, no `std::mutex`** — use Zenith primitives.
- All `.cpp` files start with `#include "Zenith.h"`.
- All tools-only code in `#ifdef ZENITH_TOOLS`.
- All test code in `#ifdef ZENITH_INPUT_SIMULATOR`.

### 4.5 Git hygiene

- One PR per task. Branch from latest master.
- Conventional Commits subject format: `feat(dp): MVP-X.Y.Z — short summary` / `test(dp): ...` / `fix(dp): ...` / `chore(dp): ...` / `docs(dp): ...`.
- Squash-merge via `gh pr merge --auto --squash`.
- Never force-push to master. Never bypass hooks. If hooks fail, fix the root cause.
- The Claude Code harness may place sessions in a `.claude/worktrees/<name>/` checkout. Per OrchestratorPlaybook Invariant 2 (no worktrees), this is treated as transient — work happens on `master` with feature branches, regardless of which directory the shell starts in. Be aware that Sharpmake-regenerated vcxprojs bake the cwd's absolute path into `ENGINE_ASSETS_DIR`/`GAME_ASSETS_DIR`/`SHADER_SOURCE_ROOT`/`ZENITH_ROOT` and post-build event xcopy commands, so committing vcxprojs from a worktree would break checkouts elsewhere.

### 4.6 Parallel agents

Per the engine-knowledge note `feedback_parallel_agents_msbuild_thrash` (Claude user-memory): **default sequential.** mspdbsrv + output-dir locks force one MSBuild at a time. Parallel works only when:
- Tasks touch different files entirely.
- One agent finishes its build before another starts.
- You're running asset linter (no MSBuild) or test runner (single build artefact reused).

When in doubt: sequential.

### 4.7 Subagent delegation

You have access to the `Agent` tool. Use it when:

| Situation | Subagent type | Why |
|---|---|---|
| Codebase research that may take > 3 Grep/Read passes | `Explore` | Keeps your main context clean; the subagent reads and summarises rather than dumping raw output. |
| Multi-file refactor with unclear dependencies | `Plan` | Returns a step-by-step plan and identifies risks; lets you approve before executing. |
| Independent implementation work that can run while you do something else | `general-purpose` with `run_in_background: true` | True parallelism (one MSBuild at a time, but background research while you write code). |
| Loading deferred tool schemas | (not a subagent) — use `ToolSearch` | Cheaper than re-launching. |

**Anti-pattern:** spawning a subagent for a single Grep or Read. Direct tools are faster and don't pay the agent-launch tax. Subagents are only worth it for *multi-step* operations.

**Discipline:** brief subagents like new colleagues. State the goal, the relevant context, what you already know, and what success looks like. A vague subagent prompt produces vague subagent output.

### 4.8 Time budgets per task category

If you're significantly over budget without convergent progress, **stop and escalate to Questions.md**.

| Task category | Target session length | Hard escalate at |
|---|---|---|
| Plumbing (tuning, registries, workflow scripts) | 2–4 h | 8 h |
| Foundation gameplay (single-mechanic feature) | 4–8 h | 16 h |
| Depth feature (multi-system) | 4–12 h | 24 h |
| Asset substitution (per asset class) | 4–8 h | 16 h |
| Playthrough test authoring | 8–16 h | 32 h |
| Engine extension (e.g. real navmesh generator) | 16–40 h | 60 h |

These are not deadlines — they're triggers to ask *"is this task ill-defined, or am I taking a wrong approach?"* Tasks that genuinely need more time get broken into sub-tasks and added to the roadmap; never grind in silence.

### 4.9 Project-specific pitfalls

Read these before you touch code; they're permanent footguns:

- **`std::function` / `std::vector` / `std::mutex` are forbidden in NEW production code** (Claude user-memory `MEMORY.md`). Use `Zenith_Vector`, `Zenith_Mutex`, function pointers. **Existing prototype code that already uses STL is grandfathered** — `Source/PublicInterfaces.cpp` uses `std::unordered_map` at lines 39, 42, 50; `Source/DPMaterials.cpp` uses it too. These were ported from UE5 and remain until a dedicated cleanup task replaces them with `Zenith_HashMap` (which exists at `Zenith/Collections/Zenith_HashMap.h`). **Do not introduce new STL containers**; if you need to read from or extend grandfathered code, the surrounding STL stays for now. **Test code (wrapped in `#ifdef ZENITH_INPUT_SIMULATOR`) may use STL freely** — `Test_HumanPlaythrough.cpp` uses `std::vector`. Production headers and runtime `.cpp` files added going forward remain strict.
- **`Zenith_Vector` has no STL iterators.** Index-based loops only.
- **Non-tools builds were fixed 2026-05-10** (Claude user-memory `project_nontools_build_broken`). Easy to regress. *Verify* `*_False` configurations build green before declaring a foundation feature done.
- **Sharpmake regen required after adding `.cpp` files** — `cd Build && cmd /c '.\Sharpmake_Build.bat < nul'`. Forgetting this means your new file isn't in the solution and tests fail with "not found."
- **`ZENITH_REGISTER_COMPONENT` doesn't fire if the `.obj` is dead-stripped** (Claude user-memory `feedback_msvc_static_init_dead_strip`). Fix by `AddComponent` at runtime from a script that *is* referenced.
- **Parallel MSBuild thrashes** (Claude user-memory `feedback_parallel_agents_msbuild_thrash`). Use `-maxCpuCount:1`.
- **Hanging compiler processes** lock build files. If a build fails on "cannot access file," run `Build/CleanBuild.bat` (kills cl.exe/mspdbsrv.exe).
- **`SimulateClickOnUIElement` asserts on missing element.** Only call after the element is known to exist in the active scene's canvas.
- **Tests must be wrapped in `#ifdef ZENITH_INPUT_SIMULATOR`** or they won't compile in non-tools builds.
- **Scene authoring lives in `Project_RegisterEditorAutomationSteps`** (Claude user-memory `feedback_scene_setup_via_editor_automation`). Don't write imperative scene construction; use `AddStep_*` calls. Marble.cpp is the canonical reference.
- **DevilsPlayground lives at `C:\dev\Zenith\Games\DevilsPlayground\` in the main repo.** Always work on `master` branch with per-task feature branches. If the harness places you in a `.claude/worktrees/<name>/` checkout, treat it as transient — the no-worktrees Invariant from OrchestratorPlaybook still holds; in particular don't commit Sharpmake-regenerated vcxprojs from inside a worktree (they bake the worktree's absolute path).
- **`DP_Player::ResetForNewRun()` is the canonical per-run reset.** Used by `DPPauseMenuController::HandleRestart`/`HandleQuit` AND the harness between-tests hook. `ResetForTest` exists only as a backward-compat alias under `#ifdef ZENITH_INPUT_SIMULATOR`; prefer the new name.
- **Perception system clamps hearing range at `min(emit_radius, agent_max_range)`.** A 200m emit doesn't reach a priest 100m away if the priest's `hearing_range_m` is 30m. For "map-wide" stimuli (BellSoul-class) use `DP_AI::NotifyAllPriestsOfInvestigatePos` to bypass the clamp via direct BB write.
- **Run tests through `Tools/run_dp_tests.ps1`, not direct `devilsplayground.exe --automated-test`.** The runner adds `--skip-tool-exports --skip-unit-tests` which the engine's automated-test driver requires for reliable batched behaviour; the direct exe path skips them and tests can fail spuriously.
- **`Sharpmake_Build.bat` has a `pause` directive** that hangs non-interactively. Invoke `Sharpmake/Sharpmake.Application.exe` directly with the same `/sources(...)` args.
- **Engine work is in-scope for autonomous agents** (per user direction 2026-05-12). Subagents may edit anywhere under `Zenith/` for tasks like MVP-0.4 (instrumentation hooks) and MVP-1.2 (navmesh generator). Engine PRs trigger mandatory Reviewer-subagent dispatch (OrchestratorPlaybook §5.4) and a Combat smoke build (catches cross-game regressions). Design rationale logged in `DecisionLog.md` before opening the PR for any net-new engine namespace.

### 4.10 When the user appears mid-session

The user may interrupt synchronously. When they do:

1. **Do not reset the loop.** Don't start over, don't re-summarise the whole project. They want a brief, not an essay.
2. **Brief them from `Status.md`** — read it back in 2–3 sentences. "Currently on MVP-1.3.3, just landed the BT branch, running tests now."
3. **Surface any ⚠️ items from `Questions.md`** they haven't answered.
4. **Accept new instructions.** If they assign a new task or modify scope: log in `DecisionLog.md`, update `MvpRoadmap.md` if it's a permanent change, then either pivot or schedule.
5. **Don't break stride mid-PR.** If you're 80% through a task, finish it before pivoting. Tell them: "Mid-task; will finish in ~30 minutes then pivot." They expect this.
6. **Treat their words like a Resolved Q.** Move the question from Open → Resolved if applicable, and update memory if the answer should persist across sessions.

The user is not the director of your loop. They are a visitor who hands you new orders. Adapt; don't restart.

### 4.11 Memory hygiene

You have a project-scoped memory system at `C:\Users\tomos\.claude\projects\C--dev-Zenith\memory\`. Save sparingly. **Default to not saving.**

**Do save when:**
- User states a preference that should persist (feedback type).
- A project fact emerges that future sessions can't derive from code (project type).
- A pointer to an external resource is established (reference type).
- A surprising or counter-intuitive convention is confirmed (feedback type).

**Don't save:**
- Code patterns derivable from reading the codebase.
- Transient state (current task, recent commits — `git log` is authoritative).
- Debugging fix recipes — the commit message has the context.
- Things already in CLAUDE.md, GDD, or any Docs/*.md.

**Format:** every memory file has frontmatter (`name`, `description`, `type`). Add a one-line pointer to `MEMORY.md`. Keep memory file bodies short (≤ 30 lines). When in doubt, write it in `DecisionLog.md` instead — that's lower-stakes.

---

## 5. Edge Cases

### 5.1 Test was flaky

If a test passes locally but fails CI (or vice-versa):
1. Re-run with `-PerProcess` to rule out batch state leak.
2. Re-run with `-FixedDt 0.01666` to rule out timing.
3. Read [TestPlan §0.2](TestPlan.md) — every test must be deterministic.
4. If you can't make it deterministic, add a comment, raise in Questions.md, mark the task complete only if the underlying behaviour is correct.

### 5.2 You broke an existing test

If a refactor or feature change breaks an existing test:
1. **First**, verify the test was correctly written. (TestPlan tests are well-vetted, but tests for new features may be over-fitted.)
2. If the test is correct and your change broke real behaviour: fix your change.
3. If the test was over-fitted and your change is correct: amend the test. **Always** document this in DecisionLog.md — over-fitted-test changes are a yellow flag.
4. If you can't decide: surface in Questions.md, revert your change, move on.

### 5.3 You're stuck

After ~2 hours of trying:
1. Write what you tried in DecisionLog.md.
2. Open a Q in Questions.md with full context.
3. Make your best guess and proceed; mark the task as "best-effort done" if your guess held.
4. Move to a different roadmap task. Don't sit blocked.

### 5.4 The build is broken on master

Don't make it worse. Open Questions.md with the failure. Try the previous green commit. If you can fix it in <30 min, do; if not, surface and move on.

### 5.5 You're out of MVP roadmap tasks

You shouldn't be. The roadmap has ~80 tasks for a 4-month MVP at full-time pace. If you genuinely run out:
1. Verify MVP done-criteria from MVPScope §5 are met.
2. If yes: update Status.md to "MVP COMPLETE", surface in Questions.md, stop.
3. If no: find the gap, add the missing roadmap task, then execute it.

### 5.6 The task is too big for one session

Some tasks (real navmesh generator, audio system, full playthrough authoring) genuinely exceed a single session. Handle them with:

1. **Branch but don't PR.** Keep work on `dp/mvp-<task-id>` across sessions.
2. **Add a `Tasks/<task-id>/notes.md`** under `Games/DevilsPlayground/` that captures incremental progress. Next session reads it cold and continues.
3. **WIP commits** are allowed on the feature branch — prefix with `wip(dp): MVP-X.Y.Z partial — what's done / what's left`. Squash them out before opening the PR.
4. **Open the PR only when the full task is done.** Don't PR partial work; it complicates the auto-merge contract.
5. **If the task is genuinely too big** (consistently exceeds budget across multiple sessions): break it into sub-tasks in MvpRoadmap.md, retire the parent task, work the children.

### 5.7 Reverting a bad merge

If a merged PR causes regressions another session/agent surfaces:

1. **Don't `git reset --hard` master.** Auto-merge means others may have committed on top.
2. Use `gh pr revert <PR-number>` (or the equivalent `git revert <commit-sha>`) — creates a new revert PR. Auto-merge that.
3. Document the revert in `DecisionLog.md` with: what broke, what test would have caught it, link to the new test you're adding to the next PR.
4. The new test is non-optional. Reverts without test coverage repeat themselves.

### 5.8 Pre-flight checklist (before opening a PR)

A 60-second self-review:

1. ✅ Test exists for the change. Test fails on master, passes on this branch.
2. ✅ `Tools/run_dp_tests.ps1 -Headless` shows 100% green locally.
3. ✅ No new `std::function` / `std::vector` / `std::mutex`.
4. ✅ New `.cpp` files start with `#include "Zenith.h"`.
5. ✅ If you added a `.cpp`, you ran Sharpmake.
6. ✅ `Docs/Status.md` updated.
7. ✅ `Docs/DecisionLog.md` updated if the decision was non-trivial.
8. ✅ Task box ticked in `MvpRoadmap.md` (in this PR).
9. ✅ PR title and body use the §3.4 template.
10. ✅ No post-MVP scope in this PR.

If any are red, fix before pushing. The auto-merge contract assumes these are true.

---

## 6. Operating Modes — Single, Subagent, Multi-Session

This section answers: *can multiple agents work on this project at once?*

The short answer: **yes, three modes are supported, in increasing order of coordination complexity.** Start with Mode A. Move to B as needed. Hold off on C until the MVP ships.

### 6.1 Mode A — Single agent, sequential sessions *(legacy fallback)*

One Claude Code instance at a time. Sessions chain via `Status.md` and `DecisionLog.md`. The roadmap is the queue; tasks are picked in order.

**When to use:** small / trivial tasks where subagent dispatch overhead isn't worth it. Debugging a flaky test where linear investigation is fastest. Direct-dialogue sessions with the user where subagent latency would slow things down.

**Pros:** simplest. No coordination overhead. No risk of agents stepping on each other. Build-system serialised by default.

**Cons:** throughput capped by single-session pace. Most MVP tasks scale better in Mode B.

### 6.2 Mode B — Orchestrator + subagent team in a single session *(default for MVP)*

One Claude Code instance acts as orchestrator; it spawns subagents via the `Agent` tool for scoped work (research, planning, code authoring, review, lint, doc maintenance). The orchestrator integrates subagent outputs, runs the build/test gate, opens PRs, and updates state files.

**When to use:** the standard for fresh sessions. The user ratified this on 2026-05-11.

**Pros:** parallelism for authoring work (background subagents); specialisation (Explore for research, Plan for design, general-purpose for code); the build/test gate is naturally serialised because only the orchestrator owns it.

**Cons:** subagent dispatch has a launch tax (~20s/subagent). Bad subagent prompts produce bad subagent output. Two background subagents writing the same file corrupts it; scope discipline matters.

**See [OrchestratorPlaybook.md](OrchestratorPlaybook.md) for the operating manual.** It covers role catalogue, dispatch patterns, the build-lock recipe, and a worked example through MVP-0.1.1.

### 6.3 Mode C — Parallel sessions, multiple agents *(post-MVP only)*

Multiple Claude Code instances on different machines / terminals / users work the project simultaneously. Each picks a different roadmap task; they coordinate via file-based locks and PRs.

**When to use:** *not yet.* This mode is over-engineered for solo MVP work. Consider it post-MVP when:
- Content-volume work dominates (e.g. importing 20 archetypes' anim sets).
- Asset substitution sweeps (each asset class can be a parallel session).
- Localisation passes (each language pair is independent).

**Pros:** real wall-clock parallelism. Specialised agents per role.

**Cons:** coordination overhead. Risk of build-system thrash (per memory). Risk of merge conflicts on `Docs/Status.md`, `MvpRoadmap.md`, `DecisionLog.md`. Auto-merge means agents could approve each other's work without human review — fine in principle, but requires *more* test discipline, not less.

**Required conventions if you enable Mode C** *(none of these exist yet; landing them is its own roadmap task)*:

1. **Task-claim file.** `Docs/Locks/<task-id>.lock` contains `{session_id, agent_name, claimed_at, expires_at}`. Stale locks (> 6 h) auto-expire. Before starting a task, an agent writes the lock; before exiting, they release it.
2. **Status.md becomes per-session.** Replace the single "current task" line with a per-session table: `| session_id | agent | current_task | last_heartbeat |`. Each session updates its own row.
3. **MvpRoadmap.md uses three states:** `[ ]` open → `[~]` in-progress (with session-id in comment) → `[x]` done. Conflict resolution: first commit wins; second agent picks a different task.
4. **DecisionLog.md is append-only with per-entry session attribution** in the header. Concurrent writes are git-resolved by ordering.
5. **MSBuild serialisation.** Agents acquire a global build-lock before invoking MSBuild. File-based: `Build/.msbuild_lock`. Hold time bounded to N minutes.
6. **PR review-by-peer-agent.** Optional but recommended: before auto-merge, another live agent reviews the PR using the §3.4 checklist. This is the failsafe for "auto-merge but with human-in-the-loop replaced by another agent."

**Role specialisation, if applicable in Mode C:** *(see Appendix A for fuller definitions; for now, sketch:)*

- **Implementer.** Picks roadmap tasks; writes code + tests.
- **Reviewer.** Reads incoming PRs; runs the §5.8 checklist; comments or approves.
- **Researcher.** Spawns Explore agents; maintains a Notes/ directory of distilled findings.
- **Asset Wrangler.** Handles only asset pipeline tasks; never touches gameplay code.
- **Tuner.** *(Post-autonomous-playtest-bot.)* Runs the bot, analyses telemetry, proposes tuning-JSON deltas.
- **Doc Maintainer.** Keeps GDD / Shortfalls / TestPlan / AssetManifest / etc. in sync with reality.

For the MVP: ignore this entire section beyond knowing it exists. **You are mode B** (orchestrator + subagents in a single session — see §6.2). The earlier draft of this paragraph said "mode A" which contradicted §1.1 and §6.2; round-4 peer review caught the inconsistency. Mode B is the operational default for the MVP per the user's 2026-05-11 directive.

### 6.4 Choosing your mode at session start

Decide before you write any code:

| Symptom | Mode |
|---|---|
| Default for a fresh user-initiated session. | **B — orchestrator + subagents.** Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md). |
| Single small task (typo, config tweak, doc edit). | A — solo sequential. |
| Debugging a flaky test or investigating an obscure bug. | A — solo linear investigation. |
| User is actively present and prefers direct dialogue. | A — minimise subagent latency. |
| Multiple machines / terminals are actively working. | C — only if coordination conventions are in place. Otherwise pick a different task. |

When in doubt for a fresh session: **B**. The orchestrator playbook is designed to handle most MVP tasks well; the small-task overhead of Mode B is acceptable.

---

## 7. The "Why" of This Setup

The user is one person, full-time, working with autonomous agents over months. Two failure modes to avoid:

1. **Drift.** Multiple sessions, no shared memory → inconsistent style, duplicated work, contradicted decisions. **Mitigation:** Status.md (current state), DecisionLog.md (rationale), MvpRoadmap.md (next task). Single source of truth per concern.
2. **Stalling.** Agent hits a question, blocks, user is asleep. **Mitigation:** "best guess + log + proceed" pattern. The user can correct in batch.

Together these mean every autonomous session is a small, complete unit of work that leaves the project in a strictly-better state. Even if the very next session contradicts a decision, the contradiction is auditable.

---

## 8. The "Done" Test

You finish a session well when:

- ✅ The current task is one box closer to ticked.
- ✅ CI is green on the merged PR.
- ✅ Status.md, DecisionLog.md, Questions.md are updated.
- ✅ The user, reading your one-paragraph summary, knows exactly what changed and what's next.
- ✅ The next agent, reading Status.md cold, can resume in <5 minutes.

If any of these are false, finish them before you stop.

---

## 9. Worked Example

**Note:** The detailed worked example previously lived here. It's been moved to **[OrchestratorPlaybook.md §8](OrchestratorPlaybook.md)** (the orchestrator's primary doc) and updated to match the current roadmap structure (where MVP-0.0.x is the bootstrap phase and MVP-0.1.1 is the failing test, not the implementation).

The version that used to be here showed MVP-0.1.1 as an implementer task — that was correct in the round-0 plan, but round-1 reconciliation flipped the order (test first, implementation second), so the duplicate example here would have misled an agent. One source of truth is better than two near-duplicates.

If you are a subagent reading this file: you don't need a worked example; your spawner provides scoped instructions. If you are the orchestrator: read OrchestratorPlaybook §8.

---

## Appendix A — Agent Role Specialisations *(for Mode C, post-MVP)*

When parallel sessions become useful (post-MVP, see §6.3), explicit role definitions help prevent overlap. Each role has a **scope filter** — the categories of MvpRoadmap tasks it works on — and a **discipline** — what it does well and what it avoids.

### A.1 Implementer

- **Scope:** any MVP-X.Y.Z task tagged `feat:` or `fix:`.
- **Strengths:** writes feature code, writes the matching tests, follows the test-first discipline rigorously.
- **Avoids:** asset pipeline work, doc-only PRs, balance tuning.

### A.2 Reviewer

- **Scope:** any open PR not authored by self.
- **Loop:** read PR diff → run §5.8 checklist → comment if anything's off → approve if clean. After 2 successful reviews from peer agents, the PR is human-equivalent reviewed and auto-merges.
- **Discipline:** never approve own PRs. Never merge without running the local test suite against the PR branch.

### A.3 Researcher

- **Scope:** Questions.md ⚠️ items that need deeper investigation. Engine-level queries from other roles.
- **Loop:** read the question → spawn Explore subagents as needed → write a Notes/ directory file with findings → update Questions.md with answer.
- **Discipline:** Notes files are append-only and dated. Findings reference file:line.

### A.4 Asset Wrangler

- **Scope:** MVP-3.X.X tasks (Mixamo / Kenney imports), AssetTestPlan tests, asset linter work.
- **Strengths:** runs `ZenithTools.exe import`, validates with the linter, updates the asset manifest.
- **Avoids:** gameplay code. Never touches anything in `Components/` or `Source/` beyond reading.

### A.5 Tuner *(post-autonomous-playtest-bot)*

- **Scope:** `Config/Tuning.json`, balance-related tests, telemetry analysis.
- **Loop:** run the bot → aggregate telemetry → propose JSON delta → PR — never code changes, only data.
- **Discipline:** all tuning PRs include the bot-aggregate JSON that justified the change.

### A.6 Doc Maintainer

- **Scope:** Docs/*.md, especially Status, Questions, DecisionLog, MvpRoadmap.
- **Loop:** read all recently-merged PRs → update docs to match reality → PR.
- **Discipline:** never edits Config/*.json or Source/*. Watches for stale doc claims (the GDD says X, but the code says Y — flag, don't decide).

### A.7 Producer / Coordinator

- **Scope:** MvpRoadmap.md task breakdown. Questions.md triage.
- **Loop:** monitor progress → split tasks that exceed time budgets → ensure no agent is blocked.
- **Discipline:** does no implementation work. Pure coordination.

Role assignment for Mode C: at most one agent per role per project at any time. Roles are claimed in `Docs/Locks/role_<name>.lock` with the same expiry / heartbeat as task locks (§6.3.1).

For the MVP: **all of these roles are played by a single sequential agent.** Specialisation is a Mode C luxury.
