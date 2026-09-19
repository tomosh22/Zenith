---
id: "cbf96fc9-c63c-472e-b496-f451c8780df7"
key: "ZEN-87"
type: "TASK"
title: "Mobile P0 acceptance: the emulator renders a test lot at 30 fps with suspend/resume"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 87
labels: ["human-gate"]
fixVersions: []
blocks: [{"key":"ZEN-52","reason":"P1 starts after P0 is accepted (Hearth TDD phase order)."}]
updatedAt: "2026-09-19T17:43:07.917Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The sprint's designated milestone-acceptance ticket for Mobile readiness P0. Hearth TDD §17 P0 exit: *"Emulator and one real device render the test lot at 30 fps (ASTC); suspend/resume."* Run TilePuzzle or Zenithmon on the x86_64 emulator with the P0 items enabled (ortho or gesture camera, tiers, tint, cutaway), record frame time, background/foreground once, and attach captures. Stops at In Review.

## Definition of Done
- [ ] Emulator frame time and a suspend/resume result are recorded with captures
- [ ] Every P0 child is Done or carried to P1 with a reason
- [ ] The Hearth and Undervault registers cite the landed keys
