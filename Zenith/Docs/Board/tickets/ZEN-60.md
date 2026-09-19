---
id: "a1e6d17d-f189-42e6-b6a7-e7bc51e8c27f"
key: "ZEN-60"
type: "TASK"
title: "Android CI leg: an AGDE compile-and-link gate plus APK assembly on engine changes"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 60
labels: ["engine-gap","needs-human"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance requires the Android CI leg green."}]
updatedAt: "2026-09-19T17:43:43.976Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #15 (P1); every Android engine change. The TDD row: *"Android CI leg: arm64 AGDE compile+link gate + APK assembly job. Android has zero CI with a documented history of silent bit-rot (MSVC-isms, a UAF found only at device time)."*

The 2026-08-03 emulator bring-up found six engine bugs that had rotted silently because no workflow compiles the AGDE configuration. `needs-human`: `.github/**` is a protected path, and a new CI leg is a budget decision.

**Scope.** A workflow that runs `Build\regen.ps1` and builds `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` for one game with a `.zproj` that has `android: true` (TilePuzzle or Zenithmon) and assembles the APK, as a required check on engine changes; the `Null_` unit suite already covers logic host-side, so no device run is needed for the gate. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A workflow compiles and links an AGDE configuration and assembles an APK on every push touching `Zenith/**`
- [ ] A deliberately introduced MSVC-only call fails it (recorded once, then reverted)
- [ ] `Docs/BuildSystem.md` §8 no longer says Android has zero CI
- [ ] Every game's pinned baseline bumped from an OBSERVED run
