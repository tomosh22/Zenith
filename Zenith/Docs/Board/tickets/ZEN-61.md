---
id: "835b0922-f253-4266-ba04-e02ddc53fce2"
key: "ZEN-61"
type: "STORY"
title: "Slim instance path for static tiles: position-plus-tint records in their own bucket"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 61
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance reviews every P1 item."}]
updatedAt: "2026-09-19T17:43:47.919Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #17 (P1); 100k static cells; complements FD-110 (dirty-range upload and static/dynamic split). The TDD row: *"Slim instance path for static tiles: position+tint (~16 B) records instead of full 4x4 matrix + anim + bounds (~96 B) with per-frame re-upload."*

`Flux_InstanceGroup::UpdateGPUBuffers` uploads full transform and animation records every frame. FD-110 makes the upload dirty-range and splits static from dynamic buckets; a static tile still pays a 96-byte record. This adds a SLIM record kind for instances that never rotate or animate.

**Scope.** A slim instance record (position, packed tint, mesh id) with its own bucket and vertex fetch in the unified path, chosen per group at creation; units over record packing, bucket selection and the cull using a group-level bound; a windowed A/B on a 100k-tile synthetic scene recording upload bytes and frame time against the full record. Do after FD-110 so the upload path is the dirty-range one. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A slim record kind exists with its own bucket and fetch, pinned by units over packing, bucket choice and cull
- [ ] A 100k-tile A/B records the upload-byte and frame-time saving with identical frames
- [ ] Related to FD-110; the register rows cite both keys
- [ ] Every game's pinned baseline bumped from an OBSERVED run
