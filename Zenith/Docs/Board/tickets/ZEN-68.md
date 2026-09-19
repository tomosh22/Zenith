---
id: "75d1f129-7b8f-44a6-911f-69974bb1d2fa"
key: "ZEN-68"
type: "STORY"
title: "Local light shadows: budgeted spot and point shadow maps through the render-graph shadow passes"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 68
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:40.660Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Implement the local-light-shadow decision from ZEN-67 (option a). Scope if taken: shadow maps for spot lights and cube-faced maps for point lights allocated from a per-frame budget (`Zenith_GraphicsOptions::m_uMaxDynamicLights` from ZEN-46 bounds it), rendered through the existing render-graph shadow passes (`Flux/Shadows/`, the cascade view machinery in `Flux_RenderViews`), sampled in `DeferredShading` with the same PCF the sun uses, off on the LOW device tier; `m_bCastShadows` becomes the per-light opt-in. Units over budget selection (which lights get maps when over budget, stable frame to frame), view construction for the six cube faces, and the shading-side sample count; a photo-tour A/B on Zenithmon's PlayerHome and RenderTest's interior. If ZEN-67 chose option b, close this WONT_DO.

## Definition of Done
- [ ] Spot and point lights cast shadows within a frame budget on desktop and are off on the LOW tier, pinned by units over budget, views and sampling
- [ ] A photo-tour A/B shows no lamp light leaking through a wall in both interiors
- [ ] `m_bCastShadows` is the opt-in and no dead field remains
- [ ] Every game's pinned baseline bumped from an OBSERVED run
