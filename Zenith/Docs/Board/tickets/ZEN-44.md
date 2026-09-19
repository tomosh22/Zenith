---
id: "3219e1a3-f55b-4855-811d-9a369d95babf"
key: "ZEN-44"
type: "TASK"
title: "Zenith_PriorityQueue: a binary heap in Zenith/Collections, first consumer Zenith_Pathfinding"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 44
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 exit lists the priority queue among the P0 items."}]
updatedAt: "2026-09-19T17:42:36.118Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Undervault #3 (P0), the A* open list; also Hearth's per-storey router. The TDD row: *"Zenith_PriorityQueue<T>: binary heap over Zenith_Vector. Zenith/Collections/ has Vector/HashMap/HashSet/CircularQueue/MemoryPool and no heap; std::priority_queue is banned."*

Verified against HEAD: `Zenith/Collections/` holds `Zenith_CircularQueue.h`, `Zenith_HashMap.h`, `Zenith_HashSet.h`, `Zenith_MemoryPool.h`, `Zenith_Vector.h` and no heap. `Zenith_Pathfinding` and the navmesh generator hand-roll their own open-list handling.

**Scope.** A header-only `Zenith_PriorityQueue<T, Compare>` binary heap over `Zenith_Vector` in the collection idiom (no exceptions, allocation through the engine allocator, `Push`/`Pop`/`Top`/`Empty`/`Reserve`, optional decrease-key by handle if `Zenith_Pathfinding` benefits); units over ordering, stability under equal keys, growth, the empty-pop refusal, and a randomised comparison against a sorted reference; migrate `Zenith_Pathfinding::FindPath`'s open list onto it as the first consumer with its existing units as the guard. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] `Zenith_PriorityQueue` exists under `Zenith/Collections/` with units covering ordering, equal keys, growth and refusals
- [ ] `Zenith_Pathfinding` uses it and its path units are unchanged
- [ ] `Zenith/Collections/CLAUDE.md` lists the new container
- [ ] Every game's pinned baseline bumped from an OBSERVED run
