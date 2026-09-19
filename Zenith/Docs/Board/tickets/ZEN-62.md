---
id: "9f146e1c-b30b-4a33-81a1-ead9a5817b75"
key: "ZEN-62"
type: "STORY"
title: "VAT character path hardening: animated normals and a two-clip crossfade"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 62
labels: ["engine-gap","deferred"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:18.109Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-15 (P2); street ambience crowds. The TDD row: *"VAT character path hardening: the VAT instancing path (Flux_AnimationTexture.h, 8 B/instance, tint lane live) gains animated normals + 2-clip crossfade (Flux_InstanceAnimData spare bytes; 8 -> 12 B)."*

The vertex-animation-texture path renders many cheap animated instances but bakes positions only, so lighting on a VAT character is static, and a clip change pops.

**Scope.** Bake normals alongside positions in the VAT (a second texture or interleaved rows), sample both in the instanced vertex path, and add a two-clip crossfade weight to the instance animation record (8 to 12 bytes); units over the bake layout and the record packing; a windowed capture of a VAT crowd lit under the CSM with a mid-crossfade frame. `deferred` until a game presents a crowd. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] VAT instances carry animated normals and a two-clip crossfade, pinned by bake and packing units
- [ ] A windowed capture shows correct lighting and a smooth crossfade
- [ ] The instance record growth is documented in `Zenith/Flux/InstancedMeshes/CLAUDE.md`
- [ ] Every game's pinned baseline bumped from an OBSERVED run
