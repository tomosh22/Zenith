---
id: "59dd5e1e-8655-40de-a90e-65540cb1da8a"
key: "ZEN-81"
type: "TASK"
title: "Add a non-tools (_False) build line to every category's gate list"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 81
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:30.186Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Suggestions row 2026-08-25 (gates): *"Add a non-tools (_False) build line to each category's gate list, copied verbatim from the CI step that already exists."* Every category gate builds `--headless` (a `Null_*_True` config); the `_False` runtime-only configs have a recorded history of rotting (the Build-oddities note: `_False` runs and pre-existing-red aux tools). `needs-human`: `zagent.project.json` is a protected path.

Add `pwsh -NoProfile -File Tools\zenith.ps1 build <Game> --config Null_vs2022_Debug_Win64_False` to each game category's gate list and `build Combat --config Null_vs2022_Debug_Win64_False` to Engine, copied from `release-build.yml` where it exists; run `Test-ZagentClient.ps1` after.

## Definition of Done
- [ ] Every category's gate list carries a `_False` build line and `Test-ZagentClient.ps1` passes
- [ ] The Suggestions row is resolved with this key

