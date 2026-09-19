---
id: "b121f253-fb0d-4960-be49-eb13763aea69"
key: "ZEN-55"
type: "TASK"
title: "Animation events from the state-machine path with a crossfade contract"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 55
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:34:52.220Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-11 (P1); footsteps and interaction beats from state-machine-driven clips. The TDD row: *"Animation events from the state-machine path: route SM-driven clip time through the same event scan as direct playback (Flux_AnimationController.cpp), with a defined crossfade contract (events fire from the dominant state only)."*

Direct clip playback scans clip events; a clip driven through `Flux_AnimationStateMachine` does not reach that scan, so a footstep event fires for a directly played walk and not for the same walk chosen by a state machine.

**Scope.** One event-scan path over the controller's evaluated clip time for both drivers, with the crossfade rule (events from the dominant state only, no double fire across a blend) stated in the header and pinned; units drive a two-state machine through a transition and assert each event fires exactly once at its time; the animation editor's event track (the r5 program) displays the same events. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] State-machine-driven clips fire events through the same scan as direct playback; the crossfade rule is pinned by a unit over a transition
- [ ] No existing direct-playback event timing moves (golden)
- [ ] `Zenith/Flux/MeshAnimation/CLAUDE.md` states the contract
- [ ] Every game's pinned baseline bumped from an OBSERVED run
