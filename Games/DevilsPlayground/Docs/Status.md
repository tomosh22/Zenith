# DP Status

**Last updated:** 2026-05-11 by initial-planning-session
**Build:** ⚠️ unknown — prototype build state not verified in this session. First task is to run a baseline build + test.
**Tests:** ?/28 — the existing skeleton-grade port has 28 tests per CLAUDE.md; pass rate unverified in this session.

## Current task

**MVP-0.1.1** — Add `Source/DP_Tuning.h/.cpp`. Load `Config/Tuning.json` at startup; expose `DP_Tuning::Get<T>(key)` accessor.

This is the first concrete code task. Prerequisites: the [Config/Tuning.json](../Config/Tuning.json) file is authored and committed; the build infrastructure (Sharpmake, MSBuild) is in place.

## Last completed

Initial-planning session — six design documents + three config files landed:

- [GameDesignDocument.md](GameDesignDocument.md)
- [Shortfalls.md](Shortfalls.md)
- [TestPlan.md](TestPlan.md)
- [AssetManifest.md](AssetManifest.md)
- [AssetTestPlan.md](AssetTestPlan.md)
- [InputsForAutonomy.md](InputsForAutonomy.md)
- [MVPScope.md](MVPScope.md)
- [MvpRoadmap.md](MvpRoadmap.md)
- [AgentBriefing.md](AgentBriefing.md)
- [Config/Tuning.json](../Config/Tuning.json)
- [Config/Archetypes.json](../Config/Archetypes.json)
- [Config/Reagents.json](../Config/Reagents.json)

User signed off on the GDD framing and gave execution mandate.

## Notes for next agent

1. **Operating mode for fresh sessions:** orchestrator + subagents (Mode B). Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) before doing anything else if you're the orchestrator of a fresh user-initiated session. The user explicitly chose this on 2026-05-11 and explicitly forbade git worktrees.
2. **Hard invariants:** (a) only the orchestrator invokes MSBuild / Tools/run_dp_tests.ps1 / the game executable; (b) no worktrees, all work on `master` with per-task feature branches; (c) only the orchestrator writes Status.md / Questions.md / DecisionLog.md / MvpRoadmap.md / Config/*.json.
3. **First action:** verify the baseline. Acquire build lock per OrchestratorPlaybook §4. Build the prototype, run the existing 28 tests. If anything is red, fix before starting MVP-0.1.1.
4. **Then** start MVP-0.1.1. See the worked example in OrchestratorPlaybook §8 for the exact pattern.
5. **Branching:** all work on `master`. Branch as `dp/mvp-<task-id>`. The worktree at `.claude/worktrees/flamboyant-mirzakhani-9186f1/` is irrelevant to DP.
6. **No external spend** unless you find a paid blocker (and even then, log it in Questions.md and pick a different task to unblock yourself).
