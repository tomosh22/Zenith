---
id: "e9b1f74b-0dfc-4454-a906-ce47dbe4f427"
key: "ZEN-69"
type: "STORY"
title: "SSGI as occlusion plus colour bleed on top of IBL irradiance, energy-conserving, on by default"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 69
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:44.473Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"SSGI remains off by default, and correctly so: it replaces the sky irradiance with a single screen-space albedo bounce rather than adding indirect on top of it, so turning it on makes shadowed geometry darker and flatter. It should become occlusion + colour bleed before it ships on."*

Re-derive `Flux/SSGI/` as an ADDITIVE term: the screen-space trace supplies (1) directional occlusion of the sky irradiance the IBL already provides and (2) a colour-bleed bounce added on top, never replacing the probe; energy is conserved (a fully unoccluded pixel is unchanged). Calibrate against the measured distribution of the trace result on the photo-tour poses, not a nominal range (the program's lesson). Units over the composition math (unoccluded input passes through unchanged; a hand-built occluder darkens by the expected fraction), the existing SSGI toggle tests, and a photo-tour A/B on Dawnmere's shaded facades and RenderTest.

## Definition of Done
- [ ] SSGI adds occlusion and colour bleed on top of IBL irradiance with energy conservation pinned by units
- [ ] A photo-tour A/B shows shadowed geometry no darker with SSGI on than off, and visible bleed near coloured surfaces
- [ ] SSGI is on by default on the HIGH tier and `Docs/design/Photorealism.md` §2 records its calibration
- [ ] Every game's pinned baseline bumped from an OBSERVED run
