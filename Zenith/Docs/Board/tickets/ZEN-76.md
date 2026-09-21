---
id: "2fcd9111-a111-4dde-a8a5-a232316a52dc"
key: "ZEN-76"
type: "TASK"
title: "zagent check: the contract verdict (exit 4) wins over repo-busy (exit 5); claim and next unchanged"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 76
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T06:48:21.299Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "e12226de-9c88-4c19-95c5-fe59bc19ada3"
---
## Goal
Suggestions row 2026-08-25 (cli): *"For `zagent check` specifically, let the DURABLE contract verdict (exit 4) win over the TRANSIENT I5 repo-busy verdict (exit 5). Keep 5 winning for claim/next, where the lock is the whole point."* Today a `check` run while another ticket is in flight answers 5 for every ticket, so a malformed ticket cannot be found until the repo is idle.

In `Tools/Vault/cli/queries.mjs` (or wherever `check` composes its verdict), evaluate the contract first and return 4 when it fails regardless of the lock; return 5 only for a contract-valid ticket that the lock would refuse. `claim` and `next` unchanged. Vitest over both orders.

## Definition of Done
- [ ] `check` returns 4 for a contract-invalid ticket while the repo is busy and 5 only for a valid one; `claim`/`next` unchanged; tests cover both
- [ ] CLAUDE.md's exit-code paragraph says so
- [ ] The Suggestions row is resolved with this key

