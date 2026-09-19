---
id: "c54c949a-cf6c-4a75-898a-8953e21223ba"
key: "HE-7"
type: "TASK"
title: "Seed the Hearth living docs: Status, TestPlan, SaveFormat, Shortfalls, Questions, DecisionLog (HE-D-), AgentBriefing, Board"
category: "Hearth"
parent: "HE-1"
sprint: "he-u2exlj3764t6lnl5ehdwdwea"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 7
labels: ["docs"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T19:19:32.567Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Seed the Hearth living-doc set in the shape Zenithmon, Foundry and Undervault use: `Status.md` (LIVE PIN line, committed-asset table, current task), `TestPlan.md` (TDD §13 suites as the plan), `SaveFormat.md` (§11 chunk table and the forward-tolerant rule), `Shortfalls.md`, `Questions.md` (seeded with TDD §16's four parked questions and owners), `DecisionLog.md` with prefix `HE-D-`, `AgentBriefing.md` (binding conventions for a loop worker) and `Board.md` (epics, sprints, versions, markers, the FD `Board.md` model). `Tools/doc_lint.ps1 -Game Hearth` passes.

## Definition of Done
- [ ] The eight documents exist under `Games/Hearth/Docs/` with the content each is the authority for
- [ ] `doc_lint.ps1 -Game Hearth` passes
- [ ] `Roadmap.md` and `Board.md` agree with the board (`zagent board status --project HE --roadmap Hearth/Docs/Roadmap.md` clean)
- [ ] Hearth pinned baseline bumped from an OBSERVED Null run (the row is added by HE-5; until then this line is inert)
