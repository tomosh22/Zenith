---
id: "56f9165a-9387-4839-be36-1e7c92d54160"
key: "ZEN-48"
type: "TASK"
title: "Per-instance tint from Zenith_ModelComponent for static and skinned draws"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 48
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:34:26.113Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-09 (P0); also DevilsPlayground's archetype tints and Zenithmon's shiny variants. The TDD row: *"Per-instance tint for static + skinned draws: feed the existing m_uColorTintPacked lane from a component (Zenith_ModelComponent::SetTint) instead of hardcoded WHITE (Flux_GPUSceneBuilder.cpp)."*

The GPU scene already carries a packed RGBA8 tint per draw item and the instanced path already uses its tint lane; the unified static and skinned path writes WHITE unconditionally in `Flux_GPUSceneBuilder.cpp`, so a game that wants a tinted villager must clone a material.

**Scope.** `Zenith_ModelComponent::SetTint(RGBA8)` stored on the component (runtime-only or serialised; decide with the same reasoning `SetModelSpaceOffset` used and record it), read by the GPU scene builder into the tint lane; the material shader multiplies it exactly where the instanced path does. Units: the lane round-trips through the builder for static and skinned items, WHITE by default so every existing frame is byte-identical (a golden over the built GPU scene), and the serialisation decision is pinned. DevilsPlayground's `DPMaterials::GetOrCreateColouredVariant` is the first consumer to migrate, proven by its existing tint tests. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A model-component tint reaches the GPU scene tint lane for static and skinned draws; the default is WHITE and existing frames are identical
- [ ] Units cover the builder for both paths and the serialisation decision
- [ ] DevilsPlayground's coloured-variant materials are replaced by the tint with its tests green
- [ ] Every game's pinned baseline bumped from an OBSERVED run
