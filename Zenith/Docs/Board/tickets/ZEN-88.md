---
id: "ce364b89-37b6-41b6-b27b-46e27fb6a017"
key: "ZEN-88"
type: "TASK"
title: "Mobile P1 acceptance: an AAB installs and runs on the emulator; Android CI green"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 88
labels: ["human-gate"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:43:51.964Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The sprint's designated milestone-acceptance ticket for Mobile readiness P1. Exit: an AAB with an install-time asset pack installs and runs on the emulator through bundletool, the Android CI leg is green, and render scale, text entry, save metadata and slim instances are exercised by at least one game. Stops at In Review.

## Definition of Done
- [ ] An AAB install on the emulator is recorded with a capture and the CI leg is green
- [ ] Every P1 child is Done or carried with a reason
- [ ] `Docs/BuildSystem.md` §8 describes the Android CI and packaging state
