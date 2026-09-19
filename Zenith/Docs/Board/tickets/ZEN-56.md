---
id: "8310aa9d-4bf2-4a20-8331-33ecafe6d184"
key: "ZEN-56"
type: "STORY"
title: "Skinned-path headroom: skip culled instances, size palettes to the rig, no per-frame heap churn"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 56
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:34:55.929Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-13 (P1); eight to twelve sims plus visitors at 30 fps on a phone. The TDD row: *"Skinned-path headroom (targeted): (a) skip skinning for models failing every view frustum; (b) size palette uploads to actual bone count instead of MAX_BONES; (c) fix the per-skeleton per-frame heap churn."*

The unified compute-skinning path (Stage 5) skins every animated instance every frame; `FLUX_MAX_BONES` sizes the palette regardless of the rig (StickFigure has 51 bones, most rigs fewer), and the per-frame pose gather allocates.

**Scope.** Three measured steps with the profiler as the instrument: skip skin jobs for instances culled in every active view (the cull result already exists per view), pack palettes at actual bone count with the bone-palette builder's dedup, and remove the per-skeleton per-frame allocation with an owned arena; a headless unit per step (job count with a culled instance, palette byte size for a 16-bone rig, allocation counter zero across a frame) and a windowed frame-time A/B on Zenithmon's Dawnmere roster. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Culled instances are not skinned, palettes are sized to the rig, and the pose gather allocates nothing per frame, each pinned by a unit
- [ ] A windowed A/B on Zenithmon records the skinning-time saving with identical frames
- [ ] Every game's animated tests pass
- [ ] Every game's pinned baseline bumped from an OBSERVED run
