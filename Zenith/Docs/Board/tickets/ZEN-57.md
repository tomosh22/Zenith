---
id: "f5eec759-fa2e-42a1-b772-5587bbf68dcd"
key: "ZEN-57"
type: "TASK"
title: "SaveData slot metadata: ReadHeader and EnumerateSlots as engine surface"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 57
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance exercises save metadata."}]
updatedAt: "2026-09-19T17:43:32.040Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-14 (P1); save-slot pickers in Hearth, Undervault and Zenithmon. The TDD row: *"SaveData slot metadata: ReadHeader(slot) -> {gameVersion, timestamp, payload size} + EnumerateSlots(prefix). The header already stores these; nothing exposes them. ZM hand-rolled its own layer; make it engine surface."*

Verified against HEAD: `Zenith/SaveData/` exposes no header read or slot enumeration; Zenithmon's `ZM_UI_SaveSlots` reads whole blobs to show a slot list.

**Scope.** `Zenith_SaveData::ReadHeader(slot)` returning version, timestamp and payload size without reading the payload, and `EnumerateSlots(prefix)`; units over an empty slot, a damaged header (fail closed with a named error, "damaged is not empty" per FD-18's rule), and enumeration order; migrate Zenithmon's slot screen onto it with its tests as the guard. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Header read and slot enumeration exist with units over empty, damaged and populated slots
- [ ] Zenithmon's save-slot screen uses them and its tests pass
- [ ] `Zenith/SaveData/CLAUDE.md` documents the surface
- [ ] Every game's pinned baseline bumped from an OBSERVED run
