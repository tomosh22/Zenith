---
id: "9cdf3367-8c29-4256-9043-d1ca0de36d60"
key: "ZEN-72"
type: "STORY"
title: "Local IBL occlusion for interiors: baked sky-visibility volumes or per-room probes"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: 13
position: 72
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:55.850Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"The IBL cube is a single distant probe with no local occlusion, so an interior receives full open-sky irradiance modulated only by a 0.5 m-radius SSAO."* Zenithmon's interiors are lit by their lamps plus that sky irradiance, which is why a windowless room still reads bright.

Give interiors a local irradiance term: either baked per-scene ambient occlusion volumes (a low-resolution 3D texture of sky visibility authored by the tools boot from the physics geometry, sampled in `DeferredShading` to attenuate the probe) or per-room local probes captured at authoring time. Choose by measurement on PlayerHome and RenderTest's interior and record why. Units over the bake (a closed box reads near-zero sky visibility inside, full outside), the sampler, and scene serialisation (a new asset, gitignored and regenerated like every bake); a photo-tour A/B on both interiors plus an exterior pose proving no regression outdoors.

## Definition of Done
- [ ] Interiors receive occluded sky irradiance from a baked volume or local probes, pinned by bake and sampler units
- [ ] A photo-tour A/B shows a closed room darker by the measured factor and exteriors unchanged
- [ ] `Docs/design/Photorealism.md` §2 records the chosen mechanism and its calibration
- [ ] Every game's pinned baseline bumped from an OBSERVED run
