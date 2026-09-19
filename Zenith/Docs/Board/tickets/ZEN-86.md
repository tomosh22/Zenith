---
id: "77c57064-1eff-41c3-ac53-14dac8807565"
key: "ZEN-86"
type: "TASK"
title: "De-globalisation acceptance: allowlist at zero outside the composition root, every game green"
category: "Engine"
parent: "ZEN-24"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 86
labels: ["human-gate"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:49.471Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The sprint's designated milestone-acceptance ticket for the de-globalisation program. Collect the evidence: the ZEN-18 reporting unit's total, `Tools/engine_singleton_allowlist.txt` at zero outside the composition root, every game's boot suite green, and the architecture gates green with no allowlist growth across the program. Stops at In Review for a person to confirm the injection shape is one they want to keep building on.

## Definition of Done
- [ ] The allowlist total is recorded as zero outside the composition root and every game boots green
- [ ] `Docs/design/EngineRefactor_Phase-1_Inventory.md` records the finished state per phase
- [ ] A person has reviewed the injection shape and recorded GO or a list of follow-ups
