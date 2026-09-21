---
id: "898f20e5-7a58-482c-a6c5-07d2d9aab4fd"
key: "ZEN-77"
type: "TASK"
title: "Warn at file time when a Definition of Done's identifiers have hits outside the declared file list"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 77
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T07:25:23.564Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "d500041d-4e73-45df-b91a-a60f07a0a2da"
---
## Goal
Suggestions row 2026-08-25 (contract): *"At file time, grep the repo for the identifiers a Definition of Done proposes to change and warn when hits land outside the ticket's declared file list."* ZM-69's first attempt blocked because its file list was two files short, and the identifiers its DoD named (`ZM_SCENE_GYM1`, `ZM_IsSceneRegisteredForLoad`) would have found them.

In `create` and `update --dod`: extract backtick-quoted identifiers from the DoD, grep the repo (source and tests, not docs) for word-bounded hits, and when the ticket declares a file list (a `## Files` section or the claim payload's list) warn with the hits outside it; without a declared list, print the hit files as information. Never refuse; this is a warning. Vitest with a fixture repo.

## Definition of Done
- [ ] `create` and `update --dod` report identifier hits outside a declared file list, and list hit files when no list is declared; tests cover both
- [ ] The Suggestions row is resolved with this key

