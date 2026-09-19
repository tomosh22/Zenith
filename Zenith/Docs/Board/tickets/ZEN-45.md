---
id: "b06bac1c-6025-4f2d-bc99-7d6412194f3e"
key: "ZEN-45"
type: "TASK"
title: "Quad budget: per-game configurable FLUX_MAX_QUADS_PER_FRAME, owned arrays, high-water debug variable"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 45
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 exit lists the quad budget among the P0 items."}]
updatedAt: "2026-09-19T17:42:40.123Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-04 (P0) and Undervault #4 (P0); the catalog grid, build menus and rosters. The TDD row: *"FLUX_MAX_QUADS_PER_FRAME 1024 -> 4096 (Flux_QuadsImpl.h); stack arrays -> owned allocation; high-water debugvar."*

Verified against HEAD: `Zenith/Flux/Quads/Flux_QuadsImpl.h` defines `FLUX_MAX_QUADS_PER_FRAME 1024` and holds `Quad m_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME]` as a fixed array; every UI canvas batches into the same budget, and the engine drops the newest submissions beyond it.

**Scope.** Make the budget per-game data (a `.zproj`-carried or `Project_*` hook value with 1024 as the default so no existing game changes), move the fixed arrays to owned allocation sized at init, add a high-water `DebugVariable` and a warning at the first drop; units over the budget plumbing, the drop policy at the limit and the high-water counter; Zenithmon's bag and dex grids and TilePuzzle's UI as the regression check (their quad counts recorded). Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] The quad budget is per-game configurable with the existing default, the arrays are owned allocations, and a high-water debug variable exists
- [ ] Units cover the limit, the drop policy and the counter; every game's UI tests pass unchanged
- [ ] The Hearth and Undervault registers cite this key
- [ ] Every game's pinned baseline bumped from an OBSERVED run
