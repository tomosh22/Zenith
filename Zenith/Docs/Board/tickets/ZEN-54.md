---
id: "21c000de-afca-491d-8845-16d7cde3b9a9"
key: "ZEN-54"
type: "STORY"
title: "Render scale decoupled from TAA and reachable on Android"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 54
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance exercises render scale on the emulator."}]
updatedAt: "2026-09-19T17:40:45.157Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #10 (P1); the mobile GPU relief valve. The TDD row: *"Render scale decoupled from TAA + reachable on Android (or: evaluate TAA-on-mobile deliberately). Today render scale exists only as the TAA-coupled path."*

Under 3D-in-ortho the deferred pipeline stays on, so a G-buffer at 0.7 to 0.8 of native is the main GPU saving on a phone; today the internal resolution is tied to the TAA upscale so turning TAA off (ZEN-46 tiers) loses render scale with it.

**Scope.** Make the internal render resolution an independent option (`Zenith_GraphicsOptions::m_fRenderScale`) consumed by the render-target allocation and the final upsample regardless of TAA state, with TAA's own upscale composing on top when enabled; reachable on Android through ZEN-50's config; units over target sizes for scale and TAA combinations and over the upsample pass selection; a windowed A/B on RenderTest at 0.75 with TAA off recording frame time and a capture. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Render scale is independent of TAA, reachable on both platforms, and target sizes are pinned by units for every combination
- [ ] A windowed A/B at 0.75 with TAA off records the frame-time saving and a capture
- [ ] ZEN-46's tiers can set it
- [ ] Every game's pinned baseline bumped from an OBSERVED run
