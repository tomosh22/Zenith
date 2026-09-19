---
id: "7c41e0b2-9a3d-4f6e-8b15-2d9c0a7e5f31"
key: "UV-2"
type: "EPIC"
title: "UV Vertical Slice"
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

TDD §20 Vertical Slice (6 to 8 weeks, Windows-primary plus an Android boot demo): *"CellGrid -> SimClock -> FluidSim (O2/CO2 + water) -> TileInstances + CellRenderFeature -> dig/mutations -> Regions/A* -> JobBoard -> VaulterSim (breath/calories) -> camera+gestures (game-side minimum ok) -> save/load -> e2e test."* Exit: GDD §20's hands-on gesture test passes on a device, a 30-minute playable loop, the determinism and conservation suites green, the `Undervault` baseline row pinned.

**BACKFILLED 2026-09-19.** This epic was missing from the board: UV issue numbers ran 1, 3, 4, 5 with exactly one gap, at 2, between the Phase 0 and Alpha epics, and the sprint `UV Vertical Slice` already existed with no epic to own its tickets. Filed by hand because a `zagent create` can only ever append the next number.

## Definition of Done

- [ ] Every child issue Done
- [ ] The GDD §20 gesture test is passed on a device and recorded (UV-31)
- [ ] Determinism master digest and conservation oracles are green and the `Undervault` row in `Tools/unit_baselines.json` is observed
