---
id: "0d7f807e-128f-4a3b-aa24-9c1085bb164c"
key: "ZEN-58"
type: "STORY"
title: "Text field and engine-drawn on-screen keyboard, headless-testable through the input simulator"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 58
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance exercises text entry on the emulator."}]
updatedAt: "2026-09-19T17:43:35.972Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-12 (P1); naming a sim or a household. Distinct from FD-135 (IME), which Hearth rejects for MVP. The TDD row: *"Text field + on-screen keyboard: Zenith_UITextField (caret, max length, filter) + Zenith_UIKeyboard (engine-drawn QWERTY overlay, Latin set), zero JNI, headless-testable via InputSimulator."*

Verified against HEAD: `Zenith/UI/` has no text-entry widget; the only way to name anything on Android would be the platform IME, which needs the JNI seam (ZEN-63) and is post-1.0 for Foundry (FD-135).

**Scope.** A `Zenith_UITextField` (caret, selection-free editing, max length, character filter, submit and cancel actions) and an engine-drawn `Zenith_UIKeyboard` overlay (Latin set, shift, backspace, space, done) built from existing widgets and quads, fed by keyboard input on Windows and by the overlay on touch, all through `Zenith_InputSimulator`; units type, edit and submit a string headlessly on both input sources; a windowed capture on the emulator. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A text field and an on-screen keyboard exist, are headless-testable through the simulator on both input sources, and honour max length and filter
- [ ] A windowed emulator capture shows the overlay typing into the field
- [ ] `Zenith/UI/CLAUDE.md` lists both widgets
- [ ] Every game's pinned baseline bumped from an OBSERVED run
