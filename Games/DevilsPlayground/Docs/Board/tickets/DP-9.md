---
id: "d64a8a85-681f-45b8-acb8-e0a50811039c"
key: "DP-9"
type: "STORY"
title: "MVP-3.2 Aelfric mesh: hat silhouette, dark robe, hand-parented lantern light"
category: "DevilsPlayground"
parent: "DP-1"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 9
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:51:27.965Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
MvpRoadmap §3.2 (MVP-3.2.1 to 3.2.3): Pursuivant Aelfric Crowe stops being a red-tinted capsule. Same rig route as the villagers (DP-7), with a distinct silhouette (a stove-pipe hat and dark robe: a hat attachment on the shared rig's head bone, the robe as a body variant or tint), a lantern point light parented to the right-hand bone so the fog pass's light hole follows him (GDD §4.5, §4.6: Aelfric does NOT carve a fog hole himself; his lantern is a light, and `Test_P2Fog_AelfricNotRevealed` must stay green), and his pursuit and apprehend animations mapped from the shared clip set.

Tests: `Test_AssetSkel_AelfricFaceBlendshapes` is deferred until VO per the roadmap and lands as "mesh loads and instantiates"; the fog contract test and the apprehend playthroughs are the regression guard. DP gates: `zenith build DevilsPlayground --headless`, the D3D12 link proof and `zenith test DevilsPlayground --headless`; the DP suite has no pinned unit-count row, so no pin bump applies unless `Zenith/**` is touched.

## Definition of Done
- [ ] Aelfric renders as a skinned figure with a hat silhouette and a hand-parented lantern light; the fog-hole contract and every apprehend test stay green
- [ ] A load-and-instantiate asset test exists for his mesh
- [ ] `MvpRoadmap.md` §3.2 ticked
