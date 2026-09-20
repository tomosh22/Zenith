---
id: "9780d773-2301-4b69-bd25-3931d0dee0d9"
key: "ZEN-78"
type: "TASK"
title: "tick_gates measure phase reports deferred gate lines and the automated-test registry count"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 78
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T23:31:42.812Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Suggestions row 2026-08-25 (gates): *"`tick_gates.ps1 -Phase measure` should report the gate lines it deferred, and should print the registry count as well as the unit pin."* A measure phase that defers a gate line silently leaves the verify phase to discover it, and the automated-test registry count (the LIVE PIN block's second number) is read by hand from the harness log today.

`Tools/tick_gates.ps1`: in `-Phase measure`, print every deferred line with its reason and write them into `tick_gates.json` as `deferred: [...]` beside `skipped`; parse the automated-test registry count from the harness summary and print it beside the unit `ran` count. Assert-script coverage in the `Tools/Test_*` harness over a fixture run log.

## Definition of Done
- [ ] Measure reports deferred lines and the registry count, both also in `tick_gates.json`, with assert-script coverage
- [ ] The Suggestions row is resolved with this key

