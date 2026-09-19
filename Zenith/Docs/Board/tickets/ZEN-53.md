---
id: "f78cc331-537b-4fab-8363-1bec52311976"
key: "ZEN-53"
type: "TASK"
title: "UI canvas scaling mode: match width, shorter or longer side, not height only"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p1-before-an-android-ship"
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 53
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-88","reason":"P1 acceptance reviews every P1 item."}]
updatedAt: "2026-09-19T17:43:16.040Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #18 (P2) and Hearth HE-ENG-03 (P0); complements FD-56, which applies the scale factor. The TDD row: *"UI canvas scaling mode: width-aware / match-shorter-side (today height-only: m_fScaleFactor = size.y / reference.y)."*

Verified against HEAD: `Zenith_UICanvas.cpp` computes `m_fScaleFactor = m_xSize.y / m_xReferenceResolution.y`, height-only. FD-56 makes the factor actually reach layout, hit-testing and fonts; this ticket adds the MODE so a 20:9 phone and a 4:3 tablet both anchor correctly. Do it after FD-56 lands so there is one consumer.

**Scope.** A per-canvas scaling mode (MATCH_HEIGHT as today, MATCH_WIDTH, MATCH_SHORTER, MATCH_LONGER) with the factor derived accordingly; units over the factor per mode for a set of aspect ratios; TilePuzzle and Zenithmon canvases unchanged at MATCH_HEIGHT (golden over their computed factors). Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A scaling mode exists per canvas, defaulting to today's height-only behaviour, with units over every mode and aspect
- [ ] Existing canvases compute the same factor as before (golden)
- [ ] Related to FD-56; the register rows cite both keys
- [ ] Every game's pinned baseline bumped from an OBSERVED run
