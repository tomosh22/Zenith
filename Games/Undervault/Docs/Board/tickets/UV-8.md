---
id: "79619a60-026f-411b-b3e2-7a329d1fd118"
key: "UV-8"
type: "TASK"
title: "Seed the Undervault living docs: Status, TestPlan, SaveFormat, Shortfalls, Questions, DecisionLog (UV-D-), AgentBriefing, Board"
category: "Undervault"
parent: "UV-1"
sprint: "uv-wv005sc30zutg2g8hpg9tfd8"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 8
labels: ["docs"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T19:06:06.773Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Seed the Undervault living-doc set in the shape Zenithmon and Foundry use (TDD §1.1 "grows the ZM living-doc skeleton"): `Status.md` (LIVE PIN line, committed-asset table, current task), `TestPlan.md` (§16 suites as the plan), `SaveFormat.md` (§11 modules and the validate-all-then-publish rule), `Shortfalls.md`, `Questions.md`, `DecisionLog.md` with prefix `UV-D-` (declared in the category once UV-7 lands), `AgentBriefing.md` (binding conventions for a loop worker) and `Board.md` (this board's epics, sprints, versions and markers, the FD `Board.md` model). `Tools/doc_lint.ps1 -Game Undervault` passes.

## Definition of Done
- [ ] The eight documents exist under `Games/Undervault/Docs/` with the content each is the authority for
- [ ] `doc_lint.ps1 -Game Undervault` passes
- [ ] `Roadmap.md` and `Board.md` agree with the board (`zagent board status --project UV --roadmap Undervault/Docs/Roadmap.md` clean)
- [ ] Undervault pinned baseline bumped from an OBSERVED Null run (the row is added by UV-6; until then this line is inert)
