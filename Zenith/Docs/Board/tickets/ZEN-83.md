---
id: "d46514c4-2bc2-4f5c-805e-b8de92d26e83"
key: "ZEN-83"
type: "TASK"
title: "Apply the six recorded tick.md changes (paste the WHY, grep the subject, durable findings to the log, retire docs-sync prose, two allowed-tools gaps)"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 83
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:37.898Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Two Suggestions rows (2026-08-26 tick, 2026-08-24 other) name changes to `.claude/commands/tick.md` that only a human may make (`.claude/**` is a protected path): (1) paste a ruling's WHY, not only its Decision, under I3; (2) grep the SUBJECT rather than the prose on a reversal ticket; (3) a durable worker finding becomes a DecisionLog entry rather than a work-log line; (4) retire the docs-sync timeout prose, which rests on a disproved premise; (5) step 5's wedged-worker guardrail mandates `TaskStop`, which is not in the allowed tools; (6) the second allowed-tools gap the same row names. `needs-human`.

Apply the six edits, re-run `Tools/zagent/Test-BuildPrompt.ps1` (the prompt assembler reads tick.md), and resolve both rows.

## Definition of Done
- [ ] The six edits are in `tick.md`, `Test-BuildPrompt.ps1` passes, and both Suggestions rows are resolved with this key

