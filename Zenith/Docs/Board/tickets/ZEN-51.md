---
id: "c51827d0-8079-41df-9f98-13bdafbd4f95"
key: "ZEN-51"
type: "TASK"
title: "Device-gate relaxation: core plus per-enabled-feature requirements declared per project"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 51
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 acceptance boots on the emulator, which the relaxed device gate must admit."}]
updatedAt: "2026-09-19T17:43:03.909Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #13 (P0); low-end Mali targets. The TDD row: *"Device-gate relaxation: drop tessellationShader + fillModeNonSolid from the hard gate for games that don't use them (make the gate per-project data); review the bindless UAB floor against low-end Mali."*

Verified against HEAD: `s_bIsPhysicalDeviceHardSuitable` in `Zenith_Vulkan.cpp` requires the compressed-vertex pipeline's `samplerAnisotropy` / `depthBiasClamp` / `fillModeNonSolid` / `tessellationShader` features; a phone GPU lacking tessellation is refused outright even by a game that never tessellates.

**Scope.** Split the gate into a REQUIRED core (what every Flux frame needs) and per-feature requirements declared by the features a project enables (tessellation only if terrain tessellation is on, non-solid fill only with the wireframe debug view), carried as project data through the `Project_*` hooks or the `.zproj`; review the bindless descriptor floor against published Mali limits and record the number chosen; a unit over the gate given synthetic device-feature sets (each requirement drops exactly when its feature is off), and the failure message names the missing feature. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] The hard gate is core plus per-enabled-feature, declared per project, with a unit over synthetic feature sets
- [ ] The bindless floor decision is recorded with its source
- [ ] Every game still boots on the desktop GPU and the emulator
- [ ] Every game's pinned baseline bumped from an OBSERVED run
