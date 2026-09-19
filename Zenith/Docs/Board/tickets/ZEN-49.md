---
id: "51e373b3-1ae7-4e8b-b7d3-f0f0d47240e5"
key: "ZEN-49"
type: "STORY"
title: "Cutaway rendering: per-item camera-hide and height-clip flags on the draw item"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 49
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 exit lists cutaway among the P0 items."}]
updatedAt: "2026-09-19T17:40:20.335Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-10 (P0); Hearth's build and live modes. The TDD row: *"Cutaway: per-item camera-hide + height-clip. Two mechanisms on the existing draw-item flag word (Flux_GPUScene.h low-16 reserved for item flags; high-16 view mask already proven by material preview): (a) camera-view hide, (b) a per-item clip height."*

A Sims-style interior needs walls hidden or clipped when the camera looks in; the draw-item flag word already has reserved item bits and a proven view mask, so this is a flag and a clip plane in the existing GPU scene, not a new pass.

**Scope.** Two draw-item flags: HIDE_IN_MAIN_VIEW (the cull drops the item for the main view while shadow cascades keep it, so hidden walls still cast) and HEIGHT_CLIP with a per-item clip height read by the material shader (`clip()` above the plane in world space); `Zenith_ModelComponent` setters; units over the flag word layout (no collision with the view mask), the cull decision per view, and a raw-mirror of the clip test; a windowed check in RenderTest's interior showcase. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Camera-hide and height-clip flags exist on the draw item, are settable from the model component, and shadow cascades still see hidden items
- [ ] Units pin the flag layout and the per-view cull; a windowed capture shows a clipped wall with intact shadows
- [ ] `Zenith/Flux/CLAUDE.md` documents the two flags
- [ ] Every game's pinned baseline bumped from an OBSERVED run
