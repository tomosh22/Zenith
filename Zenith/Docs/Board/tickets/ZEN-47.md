---
id: "5946f7a3-b4f9-4000-a789-775dbb783380"
key: "ZEN-47"
type: "TASK"
title: "Vulkan surface recreation across TERM_WINDOW and INIT_WINDOW: verify on the emulator, then fix"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 8
position: 47
labels: ["engine-gap","needs-human"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 acceptance includes suspend/resume, which is the surface-recreation result."}]
updatedAt: "2026-09-19T17:42:47.977Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #5 (P0, verify) and Hearth P0 exit criterion "suspend/resume". The TDD row: *"Vulkan surface recreation across APP_CMD_TERM_WINDOW -> INIT_WINDOW: verify on device, then fix. VkSurfaceKHR is created exactly once and there is no vkDestroySurfaceKHR anywhere in the repo."*

Verified against HEAD: `grep vkDestroySurfaceKHR Zenith` returns zero hits, and `Zenith_Android_Main.cpp` logs `APP_CMD_TERM_WINDOW` without tearing the surface down. Android process backgrounding routinely destroys the window; if the surface is not recreated the app renders to a dead surface after resume.

**Scope.** Two halves. VERIFY (`needs-human`: an emulator or device session): background and foreground TilePuzzle on the x86_64 emulator and record whether rendering resumes, with the validation-layer output attached to this ticket. FIX if it fails: destroy and recreate the surface and swapchain on TERM/INIT_WINDOW, re-acquire the present queue, and rebuild size-dependent render targets through the existing resize path (the `860e12b3` resize-deadlock fix is the precedent); a Null-safe unit over the recreation state machine and a Windows analogue (minimise/restore) that exercises the same path. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] The emulator background/foreground result is recorded on this ticket with validation output
- [ ] If broken: the surface and swapchain are recreated on TERM/INIT_WINDOW and rendering resumes, proven on the emulator and by a unit over the state machine
- [ ] Windows minimise/restore exercises the same path and stays green
- [ ] Every game's pinned baseline bumped from an OBSERVED run
