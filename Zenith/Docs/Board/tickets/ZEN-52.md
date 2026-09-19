---
id: "944efbbb-7f6f-4ea1-b393-6265fc859b49"
key: "ZEN-52"
type: "TASK"
title: "9-slice UI widget for panel and frame art"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 52
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance reviews every P1 item."}]
updatedAt: "2026-09-19T17:43:12.017Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #14 (P1); panel art in every mobile UI. The TDD row: *"9-slice UI widget: panel/frame art without stretch artifacts; the UI framework's widgets have no sliced image."*

Verified against HEAD: `Zenith/UI/` provides Button, GridLayoutGroup, Image, LayoutGroup, Overlay, Rect, ScrollView, Text, Toggle, VirtualButton and VirtualStick; `Zenith_UIImage` stretches its texture, so a bordered panel deforms with its size.

**Scope.** A `Zenith_UISlicedImage` (or a slicing mode on `Zenith_UIImage`) with four border insets in texels, emitting nine quads with corners unscaled and edges stretched on one axis, honouring the canvas scale factor and the quad budget; units over the nine rectangles for a set of sizes including degenerate ones (smaller than the borders), UV correctness, and quad count; a windowed check in TilePuzzle's menu. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A sliced image widget exists with border insets, pinned by units over geometry, UVs and quad count
- [ ] The engine gallery or TilePuzzle shows a panel at three sizes with undistorted corners (capture attached)
- [ ] `Zenith/UI/CLAUDE.md` lists the widget
- [ ] Every game's pinned baseline bumped from an OBSERVED run
