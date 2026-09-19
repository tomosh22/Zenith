---
id: "1e3402c6-e052-4e38-a6d1-f8ca0c04367e"
key: "ZEN-84"
type: "TASK"
title: "RenderTest tennis integration units must not depend on an asset a prior tools boot wrote"
category: "Engine"
parent: "ZEN-1"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 84
labels: ["flaky"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:41.770Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
A durable finding from the Behaviour Graph parity program (2026-09): *"Deleting on-disk `.bgraph` before a gate reds `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}`: those units load an asset authored by a prior tools boot."* A boot unit that depends on a file a PREVIOUS run wrote is order-dependent: green on a warm tree, red on a fresh clone or after a clean, and the gate cannot tell which. The pin (`ran == baseline`) counts it either way.

Make the two units self-sufficient: author the graph they load inside the unit from the same builder the tools boot uses (or skip with a NAMED reason that the harness reports as a skip, never as a pass), and add a lint over the boot suite that reds when a unit opens a path under a gitignored asset root without first producing it. Run the RenderTest suite on a clean tree as the proof.

## Definition of Done
- [ ] `RenderTestTennis::Integration{LoadWiring,BlackboardPublish}` pass on a clean tree with no prior boot
- [ ] A lint reds a boot unit that reads a gitignored asset it did not produce
- [ ] Every game's pinned baseline bumped from an OBSERVED run
