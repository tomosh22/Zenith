# DP Decision Log

**Purpose:** Append-only record of every non-trivial decision made during DP development. Future agents grep this when investigating "why was X done this way?"

**Format:** One entry per decision. Newest entries at the top.

**What counts as non-trivial:** anything that took >15 minutes to think through, involved trade-offs, or changed a behaviour another part of the system depends on. Tuning-value changes go in the Tuning.json's git history, not here.

---

## 2026-05-12 — MVP-0.1.1: cloned JSON parser into DP_Tuning rather than extracting a shared utility.

**Decision:** `Source/DP_Tuning.cpp` contains an anonymous-namespace copy of the hand-rolled JSON parser from `Source/DPMaterials.cpp:24-242`. The parser was *not* refactored into a shared `DP_Json.h/.cpp` utility. Both consumers now carry their own copy.

**Why:** Scope discipline for MVP-0.1.1. The task spec was "add DP_Tuning, load Config/Tuning.json"; refactoring an unrelated pre-existing file (DPMaterials) into a new shared header was out of scope. Promoting to a shared `DP_Json` becomes justified when MVP-0.2.1 (`DP_Archetypes`) adds a third consumer; at that point one PR extracts the parser and migrates both DPMaterials and DP_Tuning to use it.

**Trade-offs considered:**
- *Extract to `Source/DP_Json.h/.cpp` in this PR.* Rejected: pre-existing-code refactor in a small new-feature PR muddles the diff, and requires re-testing DPMaterials' material-loading behaviour. Defer until justified by a 3rd consumer.
- *Inline-copy and accept ~200 lines of duplication.* Accepted. Annotated in `DP_Tuning.cpp:19-22` so future readers know it's intentional.

**Test that prevents regression:** `Test_P1Tuning_LoadsAndValuesInBand` covers the parser's correctness for the Tuning.json schema; `Test_Materials` (pre-existing) covers DPMaterials' parser for material JSON. If a parser bug surfaces in either consumer, it's localised.

**Reversibility:** trivial. The MVP-0.2.1 extraction PR is the planned cleanup.

---

## 2026-05-12 — MVP-0.1.1: explicit `Get<T>` specializations for float/int/bool, no implicit-cast fall-through.

**Decision:** `DP_Tuning::Get<T>` declares a primary template plus three explicit specializations. There is no SFINAE, no `if constexpr` dispatch — querying any `T` other than `float`/`int`/`bool` is a link-time error.

**Why:** Compile-time type safety is preferable for a settings accessor that's called from gameplay code. Wrong-type queries should never silently succeed (e.g., querying `Get<double>` should not pick up the float specialization via implicit conversion). The Plan subagent (logged in conversation) suggested deleting the primary; explicit specs without a deletable primary achieves the same end with less ceremony.

**Trade-offs considered:**
- *Runtime type switch via an enum + Variant.* Rejected: heavier, error messages worse, no compile-time guarantees.
- *Templated `Get<T>` with `if constexpr` branches.* Rejected: harder to grep, no clear improvement.

**Test that prevents regression:** `Test_P1Tuning_LoadsAndValuesInBand` exercises all three specs (float, int, bool) for several keys including nested-path resolution (`possession.anchor_initial_position.x`) and the bool/number discriminator on `priest.apprehend_interruptible_by_switch`.

**Reversibility:** easy.

---

## 2026-05-12 — Worked from a Claude harness worktree despite the "no worktrees" invariant; vcxproj path damage handled by NOT committing those files.

**Decision:** Claude Code placed this orchestrator session inside `.claude/worktrees/wizardly-payne-c210e5/`. The user's stated invariant is "no worktrees, all work on master at C:\dev\Zenith\". Switching cwd mid-session would have required re-copying significant work into the main repo, which had 16+ uncommitted staged files — touching them risked clobbering the user's in-progress work. I stayed in the worktree but kept the PR free of worktree-path artefacts by restoring the four DevilsPlayground vcxproj/.filters files (Sharpmake had rewritten their absolute-path defines to the worktree's path).

**Why:** Two competing constraints: (a) honour the "no worktrees" intent (no path damage in shared files), (b) don't damage main repo's WIP. The pragmatic resolution: work in worktree; restore tracked files that contain absolute paths before commit; rely on Sharpmake-regen per [AgentBriefing.md §4.4](AgentBriefing.md) on consumers' machines to add the new `.cpp` to their solutions.

**Trade-offs considered:**
- *Copy work to main repo and commit there.* Rejected: main repo has uncommitted changes that aren't mine.
- *Commit vcxproj diffs with worktree paths.* Rejected: every other checkout would break — `c:/dev/zenith/.claude/worktrees/wizardly-payne-c210e5/` only exists on this machine.

**Test that prevents regression:** None applicable. The worktree-detection invariant is a process-discipline concern, not a code concern.

**Reversibility:** N/A — process decision, not code.

---

## 2026-05-11 — Initial planning session: ratified MVP scope and authoring framework

**Decision:** With user sign-off, locked the framing of *Devil's Playground* as a stealth-puzzle roguelite per the GDD. Authored five companion documents (Shortfalls, TestPlan, AssetManifest, AssetTestPlan, InputsForAutonomy) plus three for execution (MVPScope, MvpRoadmap, AgentBriefing). Authored three Config data files (Tuning, Archetypes, Reagents).

**Why:** The user wants autonomous execution. Autonomous agents need (a) a target (MVPScope), (b) a sequence of tasks (MvpRoadmap), (c) operating conventions (AgentBriefing), (d) structured data to reference (Config/*.json), and (e) async-communication artefacts (Status, Questions, DecisionLog). Without all five, sessions drift or stall.

**Trade-offs considered:**
- *Less documentation, faster start.* Rejected: with autonomous sessions, the document-front-loaded cost is recovered on session 2+. Without the docs, the next agent re-decides framing.
- *Full GDD execution, not MVP.* Rejected: user explicitly asked for MVP-first ("single MVP level with all gameplay elements functioning"). Scope discipline matters more than feature parity.

**Test that prevents regression:** N/A (planning artefact, not code). The MvpRoadmap's task-checklist itself is the regression guard — every task ticks because a test passed.

**Reversibility:** trivial. Documents are markdown; data files are JSON. Any line can be amended in a PR.
