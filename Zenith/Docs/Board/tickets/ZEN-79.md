---
id: "96b2443f-8e50-444e-8a6f-aa582252d57f"
key: "ZEN-79"
type: "BUG"
title: "A refused zagent claim must change nothing: reproduce the mutate-before-validate defect against Vault and fix it"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "HIGH"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 5
position: 79
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T02:49:27.496Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "f083cb4a-cde6-4e2b-862b-a4d0bcc0b15a"
links: [{"key":"ZEN-76","type":"RELATES_TO","reason":"Same observability gap from opposite ends. ZEN-76 is 'the contract verdict (exit 4) wins over repo-busy (exit 5)' for zagent check; that short-circuit is exactly why ZEN-79's defect cannot be reproduced live from inside a tick - the tick already holds a ticket, so any second claim returns 5 before the contract check is ever reached. ZEN-79's proof had to drive the write layer directly through dispatchWrite instead."}]
---
## Goal
Suggestions row 2026-08-24 (CRITICAL): *"zagent claim MUTATES BEFORE IT VALIDATES. A targeted claim on a needs-human ticket (reproduced on ZM-64) writes status To Do -> In Progress and assignee none -> the agent, THEN returns exit 4 refusing it."* That was the SQL board; the Vault CLI's `claim` is a new implementation (`Tools/Vault/cli/write.mjs`), and eight ZEN tickets were found assigned to the agent while still To Do on 2026-09-19, which is the same shape.

Reproduce against Vault: `zagent claim` on a `needs-human` ticket, on an EPIC, and on a ticket blocked by an open predecessor; assert that on exit 4 no field changed and no `board:` commit was made. Fix any path that writes before the contract check (validation must run inside `runOperation`'s prepare before any change is produced), and add Vitest coverage for all three refusals asserting zero changes.

## Definition of Done
- [ ] A refused claim (needs-human, epic, blocked) changes no field and makes no commit, proven by tests
- [ ] The eight stale assignments found on 2026-09-19 have a recorded cause (this defect or another) on this ticket
- [ ] The Suggestions row is resolved with this key

