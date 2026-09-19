---
id: "cfe2d33c-84f9-4c99-b13f-5bb8504b7034"
key: "ZEN-28"
type: "EPIC"
title: "Agent loop, Vault and documentation tooling"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "LOW"
storyPoints: null
position: 28
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:32:40.554Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The loop's own tooling: `zagent` and the Vault CLI (`Tools/Vault/cli/`), `Tools/doc_lint.ps1`, `Tools/tick_gates.ps1` and the `/tick` protocol. The Suggestions database (`zagent suggestions --all`) holds the friction every tick since 2026-08-20 recorded; eleven rows are still `new`, and each becomes a child here so it can be scheduled instead of re-discovered. Three of them touch `protectedPaths` (`.claude/**`, `zagent.project.json`) and are `needs-human` by construction; the rest are ordinary work under `Tools/**`, which no category's `paths` claims, so their gates are the Engine list.

## Definition of Done
- [ ] Every child issue Done, and its Suggestions row resolved with `--reason` naming the ticket
- [ ] `Tools/zagent/README.md` and `CLAUDE.md`'s `zagent` table describe the shipped behaviour
