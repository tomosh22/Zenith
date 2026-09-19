---
id: "34b25d56-f3c4-4322-997a-ad92b55fb127"
key: "DP-2"
type: "EPIC"
title: "DP M2 — Playtest instrumentation, bot and balance"
category: "DevilsPlayground"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: null
position: 2
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:50:57.471Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Post-MVP milestone 2, from `MvpRoadmap.md` "Post-MVP backlog" item 1 (the autonomous playtest bot, *"highest-value engineering investment post-MVP"*), `Docs/Status.md` "What's still open" (the personality-matrix questions, replay-debug UI, single-cell analysis, P1 timer amortisation, the six-gap follow-ups) and `Shortfalls.md` §3.11 (bot pathfinding duplicates the engine navmesh). The theme: turn the 8-personality seed matrix and the telemetry recorder into an instrument that runs unattended and answers balance questions with numbers, then fix what it finds.

## Definition of Done
- [ ] Every child issue Done or carried with a reason
- [ ] The matrix runs unattended nightly and its report is the balance authority in `Docs/Status.md`
