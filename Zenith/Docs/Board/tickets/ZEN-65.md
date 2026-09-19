---
id: "76d821a5-db68-40ff-acb5-616d044a01f1"
key: "ZEN-65"
type: "STORY"
title: "Morph targets: vertex lanes, pre-skin compute accumulation, glb import and sliders"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: 13
position: 65
labels: ["engine-gap","deferred"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:29.334Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-21 (post-1.0, data-gated). The TDD row: *"Morph targets (CAS depth): morph lanes in the skin vertex + compute accumulation pre-skin + Assimp import + CAS sliders. Post-1.0, data-gated: taken only if CAS-depth retention data demands it."*

Character creation depth beyond bone-scale presets and texture synthesis needs blend shapes. `deferred`: the TDD itself defers it until retention data exists.

**Scope.** Morph lanes in the vertex codec (`Flux_VertexCodec` reflection-driven layouts make the addition a declared stream), accumulation in the compute-skinning kernel before the palette skin, `.glb` import of morph targets through the existing importer, and a slider surface; units over codec layout, the accumulation kernel via the raw-mirror technique, and an imported test mesh. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Morph lanes exist in the codec and are accumulated pre-skin, pinned by units
- [ ] A `.glb` with morph targets imports and renders a blended pose (capture attached)
- [ ] Hearth's register cites this key
- [ ] Every game's pinned baseline bumped from an OBSERVED run
