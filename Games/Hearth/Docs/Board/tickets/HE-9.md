---
id: "f09920bd-ac32-47fe-b22e-a2dbc3381a0c"
key: "HE-9"
type: "STORY"
title: "HE_LotGrid with walls on cell edges and HE_RoomGraph flood fill"
category: "Hearth"
parent: "HE-1"
sprint: "he-u2exlj3764t6lnl5ehdwdwea"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 9
labels: []
fixVersions: []
blocks: [{"key":"HE-10","reason":"the router reads the edge lattice","raw":{"key":"HE-10","reason":"the router reads the edge lattice","raw":{"key":"HE-10","reason":"the router reads the edge lattice","raw":{"key":"HE-10","reason":"the router reads the edge lattice"},"invalid":false},"invalid":false},"invalid":false},{"key":"HE-12","reason":"the shell is generated from the grid","raw":{"key":"HE-12","reason":"the shell is generated from the grid","raw":{"key":"HE-12","reason":"the shell is generated from the grid"},"invalid":false},"invalid":false},{"key":"HE-19","reason":"objects occupy grid cells and slots","raw":{"key":"HE-19","reason":"objects occupy grid cells and slots"},"invalid":false},{"key":"HE-20","reason":"build commands mutate the grid"}]
updatedAt: "2026-09-19T19:25:16.967Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
TDD §5 data model, walls on cell edges (decided). `Source/Grid/HE_LotGrid`: per storey (cap 64x64, 3 storeys; MVP 24x20x1) cells `{u8 floor, u8 roomId, u16 occupancy, u8 reservation}` plus two edge lattices (north-south `(W+1)xD`, east-west `Wx(D+1)`) of `u16 {type:4, cutout:4, paintA:4, paintB:4}`; footprint = cell mask + 4-way rotation; interaction slots as data `(cellOffset, facing)` pairs. `Source/Grid/HE_RoomGraph`: flood fill bounded by wall edges, recomputed per dirty region, Environment score = light + decor - mess. Units from §13: grid and edge legality (footprint clear, wall-edge validity, rotation), room flood on hand-built lots including a door splitting a room, dirty-region recompute equal to a full flood under mutation fuzz, occupancy and reservation exclusivity.

## Definition of Done
- [ ] The cell and edge model exists with legality checks and 4-way footprints, unit-pinned
- [ ] Rooms flood from edges and dirty-region recompute matches a full flood under fuzz
- [ ] Interaction slots resolve to walkable cells or a named reject reason
- [ ] Hearth pinned baseline bumped from an OBSERVED Null run (the row is added by HE-5; until then this line is inert)
