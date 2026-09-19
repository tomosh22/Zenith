---
id: "6b74693b-5003-40ae-a209-15d68dc37999"
key: "ZEN-71"
type: "STORY"
title: "Motion blur and depth of field over the existing velocity MRT as optional HDR-chain passes"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 71
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:52.073Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"No depth of field, no motion blur: the velocity MRT already exists and is populated, so motion blur is nearly free whenever it is wanted."* The TAA program populated per-pixel velocity for geometry, sky and primitives (translucency and particles deliberately excluded, `Flux/TAA/CLAUDE.md`).

Add a motion-blur pass over the velocity MRT (tile max, neighbour max, per-pixel gather with a sample budget, clamped by the shutter fraction option) and a depth-of-field pass (circle-of-confusion from the camera's focus distance and aperture, near and far layers, bokeh gather at half resolution), both after TAA and before bloom in the HDR chain, both off by default and exposed through `Zenith_GraphicsOptions` and the camera component. Units over the CoC math, the tile-max reduction and the option plumbing; a photo-tour A/B with a moving camera and a focused subject.

## Definition of Done
- [ ] Motion blur and depth of field exist as optional HDR-chain passes with units over CoC, tile reduction and options
- [ ] A photo-tour A/B shows correct blur with both on and byte-identical frames with both off
- [ ] Both are off on the LOW tier (ZEN-46)
- [ ] Every game's pinned baseline bumped from an OBSERVED run
