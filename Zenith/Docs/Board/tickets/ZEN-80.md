---
id: "e1b77653-1817-4069-9c65-9a8a7af165c7"
key: "ZEN-80"
type: "TASK"
title: "Vault web app: the status menu offers exactly the workflow's transitions"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 80
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:26.313Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
ZEN-23's work log: *"Status-menu catalog correction is a separate usability follow-up."* The Vault web app's status menu offers transitions from a catalog that does not match the workflow file (`Docs/Board/workflow.md` per project) in at least one case observed during the production verification.

Make the web app's status menu read the project's workflow transitions (the same `transitionAllowed` rule the CLI enforces in `write.mjs`) so the menu never offers a move the CLI would refuse; Playwright coverage for one allowed and one refused transition.

## Definition of Done
- [ ] The status menu offers exactly the workflow's transitions for the ticket's current status; Playwright covers an allowed and a refused move

