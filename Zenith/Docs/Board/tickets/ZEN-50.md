---
id: "7e985da4-f5b7-483c-957d-ad07787c3090"
key: "ZEN-50"
type: "TASK"
title: "Android config plumbing: an APK args file and intent extras reach Zenith_CommandLine"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 50
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 acceptance sets flags on the emulator through the config plumbing."}]
updatedAt: "2026-09-19T17:40:24.441Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #12 (P0); every Android QA run. The TDD row: *"Android config plumbing: asset-packaged config file (or intent extras) parsed into the Zenith_CommandLine surface at boot. No way to set any flag on device; every CLI-gated knob is inert on Android."*

Verified against HEAD: `Zenith_Android_Main.cpp` never constructs a `Zenith_CommandLine` from anything the device provides, so `--fixed-dt`, `--automated-test`, render-scale and tier overrides cannot be set on a phone or the emulator.

**Scope.** Read an optional `zenith_args.txt` from the APK assets (and intent extras when present, `adb shell am start -e args "..."`) and feed it through the same `Zenith_CommandLine` parser Windows uses, so every flag has one meaning on both platforms; units over the parser given the asset form (quoting, empty file, unknown flag refusal identical to Windows); an emulator run proving `--automated-test` selects a test on Android. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] An APK-packaged args file and intent extras reach `Zenith_CommandLine` through the shared parser, proven by units
- [ ] On the x86_64 emulator `--automated-test <T>` runs that test, recorded on this ticket
- [ ] `Zenith/Android/CLAUDE.md` documents the two sources and their precedence
- [ ] Every game's pinned baseline bumped from an OBSERVED run
