---
id: "b700418d-1b50-4f43-a8a2-ee054ef5f4e6"
key: "ZEN-75"
type: "TASK"
title: "zagent writes .zagent/last.exit on every exit path"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 75
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T05:45:24.313Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "fc49639f-31ec-409b-b911-cf5b71b07487"
---
## Goal
Suggestions row 2026-08-26 (cli): *"Write zagent's exit code to .zagent/last.exit on every exit path, so the protocol's most-broken rule becomes a file read like every other rule."* The client's usage line already names `.zagent/last.exit` as scratch, and the tick protocol branches on exit codes (3, 4, 5, 6, 7), so a path that exits without writing the file leaves the tick reading a stale code.

Audit every exit path in `Tools/Vault/cli/client.mjs` and `zagent.mjs` (success, usage error, refusal, thrown error, unreachable board 7) and write `last.exit` in one place (a `finally` around dispatch); Vitest asserts the file for each exit class including a thrown error.

## Definition of Done
- [ ] Every exit path writes `.zagent/last.exit`, proven by tests for each exit class
- [ ] The Suggestions row is resolved with this key

