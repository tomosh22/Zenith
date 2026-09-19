---
id: "012fbb5e-8bee-4973-96e7-f1849abede6e"
key: "ZEN-66"
type: "TASK"
title: "Input program hand-off: run the Android on-device and gamepad checklists"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 66
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:33.100Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** The input program (WP0..WP6, 2026-08) left two HUMAN checklists undelivered. The TDD row: *"Android on-device checklist (touch layout on a real phone) and the gamepad checklist (every action bound and reachable on a pad)."*

The input program shipped its engine and game work with unit and simulator coverage; two verifications need hands on hardware and were delivered as checklists. `needs-human`: no runner produces them.

**Scope.** Run both checklists from the program's ledger, record pass/fail per row on this ticket, and file a ticket per failure under this epic. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Both checklists are recorded row by row on this ticket
- [ ] Every failure has its own ticket
- [ ] Every game's pinned baseline bumped from an OBSERVED run
