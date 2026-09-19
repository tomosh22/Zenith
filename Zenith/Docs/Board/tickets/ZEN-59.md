---
id: "873c647f-ede2-402d-bdf8-8885bee156c4"
key: "ZEN-59"
type: "STORY"
title: "AAB and install-time Play Asset Delivery packaging: zenith package grows an Android target"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 8
position: 59
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance installs the AAB the packaging target produces.","raw":{"key":"ZEN-88","reason":"P1 acceptance installs the AAB the packaging target produces."},"invalid":false},{"key":"UV-53","reason":"AAB packaging is engine work"}]
updatedAt: "2026-09-19T19:17:45.614Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-18 (P3) and Undervault #16 (P1 store gate); complements FD-125 (icons, versionCode, signing plumbing). The TDD row: *"AAB + install-time Play Asset Delivery (packs stay local/offline without a 400 MB base fight), R8 config, versionCode automation. zenith package grows an Android target producing a signed AAB."*

Store submission needs an AAB; today the pipeline stops at an APK and `zenith package` is Windows-only. FD-125 covers the per-game icon, version fields and signing CONFIG plumbing; this ticket is the AAB build and the asset-pack split.

**Scope.** `zenith package <Game> --android` producing an AAB from the AGDE build with the game's assets as an install-time asset pack, R8 rules for the tiny Java shell, and versionCode from the `.zproj`; a `bundletool` local install path so the result is testable on the emulator without the store; units over the packaging script (`Tools/Test_*` harness) for the manifest and pack layout; the release keystore itself stays a human step recorded in `Docs/BuildSystem.md`. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] `zenith package <Game> --android` emits an AAB with an install-time asset pack and installs on the emulator through bundletool
- [ ] Script units cover manifest and pack layout; `Docs/BuildSystem.md` documents the keystore step a human performs
- [ ] Related to FD-125; the register rows cite both keys
- [ ] Every game's pinned baseline bumped from an OBSERVED run
