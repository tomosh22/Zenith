---
id: "b8d2f6a1-3c7e-4e9b-a054-6f1e8c2d9a47"
key: "HE-2"
type: "EPIC"
title: "HE P1 — Vertical slice: MVP, \"one sim, one house\""
category: null
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: null
position: 2
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T18:00:00.000Z"
flagged: false
severity: null
reproducibility: null
resolution: null
resolvedAt: null
---
## Goal

TDD §17 P1 (~10 to 12 weeks), engine WPs HE-ENG-11 to 14 plus 07 finishing: *"Interaction FSM + 25 interactions / 40 objects; build/buy (walls, floors, doors, paint, 1 roof, undo); CAS-lite + naming; save/load + autosave; full touch UX incl. pie menu; audio v1; needs/mood HUD."* Exit: *"A stranger plays 15 min on a phone unprompted; autosave survives process death; 30 fps sustained on the reference device."* The TDD's critical path puts ASTC and first-device bring-up as THE P1 gate, never later.

**BACKFILLED 2026-09-19.** This epic was missing from the board: HE issue numbers ran 1, 3, 4 with exactly one gap, at 2, between the P0 and P2 epics, and the sprint of this name already existed with no epic to own its tickets. Filed by hand because a `zagent create` can only ever append the next number.

## Definition of Done

- [ ] Every child issue Done
- [ ] A stranger has played 15 minutes on a phone unprompted and the session is recorded (HE-33)
- [ ] Autosave survives process death on a device and 30 fps is sustained on the reference device (measured)
