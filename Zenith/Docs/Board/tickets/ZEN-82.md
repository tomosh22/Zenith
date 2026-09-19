---
id: "c3f4b646-a042-45c8-a199-b4a7841caaba"
key: "ZEN-82"
type: "TASK"
title: "Give the Engine category a decision log (Docs/EngineDecisionLog.md, ZEN-D- prefix) and make zagent decide warn without one"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 82
labels: ["needs-human","docs"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:34.078Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Suggestions row 2026-08-26 (config): *"Give the Engine category a `decisionLog` in zagent.project.json, or make the project-level decisionLog a fallback for categories that declare none."* ZEN-6's DoD asked for a ruling "recorded in the decision log" and the Engine category had none, so the reasoning went to the board's Decisions database only, invisible to a loop worker that reads files off disk. `needs-human`: `zagent.project.json` is a protected path.

Create `Docs/EngineDecisionLog.md` with the ZEN-6 ruling backfilled from the Decisions row, declare it as the Engine category's `decisionLog` with prefix `ZEN-D-`, and make `zagent decide` warn when a ticket's category declares none.

## Definition of Done
- [ ] `Docs/EngineDecisionLog.md` exists with ZEN-6 backfilled and is declared as Engine's `decisionLog`
- [ ] `zagent decide` warns for a category without one
- [ ] The Suggestions row is resolved with this key

