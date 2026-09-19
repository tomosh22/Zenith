---
id: "a61717e2-836c-4c65-a17e-b886d39d5cfe"
key: "ZEN-42"
type: "TASK"
title: "Ortho camera: public InitialiseOrthographic, plane setters and the missing Vulkan Y-flip"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 42
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:34:04.311Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #1 (P0), consumed by every Undervault frame and by any 2D-in-3D UI. The TDD row: *"Ortho camera: public InitialiseOrthographic() + type/plane setters; add the missing Vulkan Y-flip to the ortho branch; unit tests."*

Verified against HEAD: `Zenith_CameraComponent.cpp` applies `xOut[1][1] *= -1` in the PERSPECTIVE branch only; the `CAMERA_TYPE_ORTHOGRAPHIC` branch returns `Zenith_Maths::OrthographicProjection(...)` unflipped, so an orthographic camera renders upside down on Vulkan, and there is no public initialiser for the orthographic type or setters for its planes.

**Scope.** Add `InitialiseOrthographic(left, right, bottom, top, near, far)` and plane setters mirroring the perspective API; apply the same Y-flip in the ortho branch (transcribed through `Zenith_Maths::Authoring*` if it is ever reached from the authoring path); units over the projection matrix (a known point maps to the expected NDC, Y inverted exactly once) and a Null-safe automated test that renders a quad through an ortho camera and reads back its position via the primitive readback path. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] `InitialiseOrthographic` and plane setters exist and are serialised like the perspective fields (schema bump with a tolerant reader if the component stream grows)
- [ ] The ortho branch applies the Vulkan Y-flip exactly once, pinned by a matrix unit and a readback test
- [ ] RenderTest and every existing perspective camera are byte-identical in their projection (a golden unit over the perspective matrix)
- [ ] Every game's pinned baseline bumped from an OBSERVED run
