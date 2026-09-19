---
id: "ce5585fd-15e6-4516-bbd7-37b361b13eb9"
key: "HE-6"
type: "TASK"
title: "Human edits after the scaffold: Hearth unit-gate line, he-tests.yml, doc-lint, baseline row leaves advisory"
category: "Hearth"
parent: "HE-1"
sprint: "he-u2exlj3764t6lnl5ehdwdwea"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 6
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T19:19:28.070Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The edits a landed scaffold needs that the loop may not make (`zagent.project.json` and `.github/**` are protected): the Hearth category gate list gains `run_unit_gate.ps1 -Game Hearth` against the `Null_` exe (TDD §14 "Gates"; the Engine list already builds Hearth), `.github/workflows/he-tests.yml` modelled on `zm-tests.yml` (regen, `Null_` build, unit gate by game name, headless batch, results artifact), the `doc-lint.yml` game array gains Hearth once HE-7 seeds the docs, and the `Hearth` row leaves `advisory` in `Tools/unit_baselines.json` with `Test_UnitBaselineManifest.ps1` green. `needs-human`.

## Definition of Done
- [ ] The Hearth gate list carries the unit-gate line and `Test-ZagentClient.ps1` passes
- [ ] A `he-tests.yml` workflow runs the Null build, unit gate and headless batch; doc-lint covers Hearth
- [ ] The `Hearth` baseline row is gated, not advisory
