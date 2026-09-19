---
id: "10ed29cd-ea68-4351-8425-105d29d455ce"
key: "ZEN-27"
type: "EPIC"
title: "Rendering quality: the photorealism known gaps"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: null
position: 27
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:32:36.941Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4 "Known gaps", the structural items the 2026-09 photorealism program deliberately left open after calibrating the sky, sun, exposure, bloom, terrain shadows and albedo against measured distributions: no local light shadows (every interior lamp leaks through walls, the largest remaining structural gap for interiors), SSGI off by default because it replaces sky irradiance rather than adding occlusion and colour bleed, no ozone absorption so twilight lacks the Chappuis band, no depth of field or motion blur although the velocity MRT already exists, and a single distant IBL probe with no local occlusion. Each child is a rendering feature with its own headless units where the maths allows and a `RT_PhotoTour` / `ZM_PhotoTour_Test` capture comparison where only pixels can judge (§3 "How to verify a change"). The program's lesson binds every child: calibrate against the MEASURED distribution, never the nominal range.

## Definition of Done
- [ ] Every child issue Done or explicitly deferred with a recorded reason
- [ ] `Docs/design/Photorealism.md` §4 lists only what is still open, and §2 records the calibration of each landed feature
