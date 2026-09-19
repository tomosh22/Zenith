---
id: "7f120932-e9a3-4056-a2f9-75b24ea1e5e1"
key: "ZEN-43"
type: "STORY"
title: "Zenith_Gestures: tap, long-press, drag, flick, pinch and two-finger pan over the pointer table and claims"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: 8
position: 43
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 acceptance drives the camera by gesture on the emulator."}]
updatedAt: "2026-09-19T17:39:55.174Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-02 (P0) and Undervault #2 (P0); every touch game's camera and selection. The TDD row: *"Zenith_Gestures on g_xEngine, built over the pointer table + claims: long-press, drag-threshold, double-tap, 2-finger pinch (scale + focus point), 2-finger pan. Runs after UI claim capture so widgets always win."*

Verified against HEAD: the engine's only gesture primitive is `Zenith_Pointers::WasTapThisFrame` (`Zenith/Input/Zenith_Pointers.h`); there is no long-press, drag, pinch or flick recogniser anywhere, so every touch game would re-derive them.

**Scope.** A `Zenith_Gestures` subsystem over the input program's pointer table and claim system (WP1/WP2, `Zenith/Input/`): recognisers for tap, double-tap, long-press, one-finger drag with threshold, flick, two-finger pinch (scale and focus point) and two-finger pan, each emitting through the action layer so games bind them like any action; UI claims win (a pointer a widget captured never starts a gesture). Fully `Zenith_InputSimulator`-driven headless units for every recogniser including the ambiguity windows (tap vs drag, tap vs double-tap) and multi-pointer sequences; a windowed check on the emulator with real touch. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] Every listed recogniser exists on `Zenith_Gestures`, emits through the action layer, and yields to UI claims
- [ ] Headless units drive each recogniser and the ambiguity windows through `Zenith_InputSimulator`, including two-pointer sequences
- [ ] TilePuzzle or Zenithmon's touch layout consumes at least one gesture on the emulator as the integration proof
- [ ] `Zenith/Input/CLAUDE.md` documents the recogniser contract and the claim ordering
- [ ] Every game's pinned baseline bumped from an OBSERVED run
