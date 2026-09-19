---
id: "038aaf94-9211-4ff8-a10b-f5a7da0b8e71"
key: "UV-7"
type: "TASK"
title: "Human edits after the scaffold: Undervault unit-gate line, uv-tests.yml, doc-lint, baseline row leaves advisory"
category: "Undervault"
parent: "UV-1"
sprint: "uv-wv005sc30zutg2g8hpg9tfd8"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 7
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T19:06:02.792Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The two edits a landed scaffold needs that the loop may not make (`zagent.project.json` and `.github/**` are protected paths), the FD-26 shape: (1) the Undervault category gate list gains `run_unit_gate.ps1 -Game Undervault` against the `Null_` exe (the Engine list already builds Undervault); (2) `.github/workflows/uv-tests.yml` modelled on `zm-tests.yml` (regen, `Null_` build, unit gate by game name, headless test batch, results artifact) and the `doc-lint.yml` game array gains Undervault once UV-8 seeds the docs. Then the `Undervault` row leaves `advisory` in `Tools/unit_baselines.json` and `Test_UnitBaselineManifest.ps1` stays green. `needs-human`.

## Definition of Done
- [ ] The Undervault gate list carries the unit-gate line and `Test-ZagentClient.ps1` passes
- [ ] A `uv-tests.yml` workflow runs the Null build, unit gate and headless batch; doc-lint covers Undervault
- [ ] The `Undervault` baseline row is gated, not advisory
