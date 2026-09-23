---
id: "3219e1a3-f55b-4855-811d-9a369d95babf"
key: "ZEN-44"
type: "TASK"
title: "Zenith_PriorityQueue: a binary heap in Zenith/Collections, first consumer Zenith_Pathfinding"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "Done"
assignee: "agent@zenith.invalid"
priority: "HIGH"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 44
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-87","reason":"P0 exit lists the priority queue among the P0 items."}]
updatedAt: "2026-09-23T20:04:44.234Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
harness: "claude"
lane: null
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

## Comments

ran on claude:sonnet/low via SIMPLE+LOW · category Engine

ZEN-44 landed via lane ZEN (`integrate` merge `9d038100`): header-only `Zenith_PriorityQueue`, +6 Core units, `Zenith_Pathfinding`'s open list migrated. Verify green on 19 lines (Zenithmon category unioned); pins Combat 2737 / Zenithmon 4594 OBSERVED, RenderTest 2840 inferred. Caveat: equal-cost tie-break order may differ from the old STL heap; no unit asserts exact routes.

<!-- agent:worklog:start ZEN-44 -->
ran on claude:sonnet/low via SIMPLE+LOW · category Engine

## Work Log — 2026-09-23

**Outcome**: merged to `master` as `9d038100` (lane ZEN, `zagent integrate` LANDED — board-only gap) · ran on claude:sonnet/low via SIMPLE+LOW

**What changed**

- `Zenith/Collections/Zenith_PriorityQueue.h` (new) — header-only `Zenith_PriorityQueue<T, Compare>` binary heap over `Zenith_Vector`, plus a `Zenith_Less<T>` default trait. `std::priority_queue`-identical Compare semantics (default max-heap; an `operator>` comparator gives a min-heap); asserts on empty `Pop()`/`Top()`, no exceptions; no decrease-key (FindPath's lazy deletion does not need it — documented).
- `Zenith/AI/Navigation/Zenith_Pathfinding.cpp` — A* open list migrated off `std::priority_queue<AStarNode, std::vector, std::greater>` onto the new queue via `AStarNodeCompare` (wraps the unchanged `AStarNode::operator>`); `<queue>` dropped.
- `Zenith/AI/Navigation/Zenith_Pathfinding.h` — stale comment (it still named `<queue>`) corrected by the orchestrator from the review NIT.
- `Zenith/EngineComposition/Zenith_UnitTests.Tests.inl` — +6 `ZENITH_TEST(Core, PriorityQueue*)` units: ordering, custom min-heap comparator, equal keys (multiset reconciliation + monotonic order), growth across reallocation, empty refusal via `Zenith_AssertCaptureScope`, randomised vs an independent insertion-sort reference.
- `Zenith/Collections/CLAUDE.md` — lists the container.
- `Tools/unit_baselines.json`, `Games/Zenithmon/Docs/Status.md` — pins + LIVE PIN + narration (orchestrator).

**Files** (`git diff --stat`)
```
 Games/Zenithmon/Docs/Status.md                     |   8 +-
 Tools/unit_baselines.json                          |   6 +-
 Zenith/AI/Navigation/Zenith_Pathfinding.cpp        |  30 ++-
 Zenith/AI/Navigation/Zenith_Pathfinding.h          |   4 +-
 Zenith/Collections/CLAUDE.md                       |  12 +
 Zenith/Collections/Zenith_PriorityQueue.h          | (new)
 Zenith/EngineComposition/Zenith_UnitTests.Tests.inl| 248 +++
```

**Gates** (lane ZEN)
| Command | Result |
|---|---|
| measure #1 | exit 4 — `build engine` (now the WHOLE engine sln) failed: the new header's block comment said `A*/Dijkstra`, and that `*/` closed the comment at line 30. Fix-forward 1 (worker): reworded |
| measure #2 | exit 10 — all 12 build lines green (whole engine sln linked fluxcompiler.exe + zenithhub.exe; `build TilePuzzle --config Vulkan_vs2022_Debug_Win64_True --whole` linked tilepuzzlelevelgen.exe + tilepuzzleregistryviewer.exe); Combat OBSERVED 2737 vs pin 2731 |
| verify (19 lines, Zenithmon category unioned by the Status.md edit) | exit 0 — incl. `zenith test Combat` 17/0, Combat unit 2737/0 failed, `zenith test Zenithmon` 116/0, Zenithmon unit 4594 ran / 4592 passed / 0 failed / 2 skipped in 482 s under `-TimeoutSec 1200`, doc_lint ×3 |
| verify (re-run after correcting the pin narration) | exit 0, same counts |
| `guard` | ok, workerMovedNothing clean |

**Coverage**
| Changed file | What executes the changed line |
|---|---|
| `Zenith_PriorityQueue.h` | the 6 new Core units (every game's boot suite), and `Zenith_Pathfinding` via its units |
| `Zenith_Pathfinding.cpp` | `Zenith_Pathfinding.Tests.inl` (unchanged) + the navmesh units — but see the tie-break caveat below |
| `Zenith_UnitTests.Tests.inl` | itself (the unit gates) |
| `Zenith_Pathfinding.h`, `CLAUDE.md` | comments/docs only — nothing executes them |

**Definition of Done**

- [x] `Zenith_PriorityQueue` exists under `Zenith/Collections/` with units covering ordering, equal keys, growth and refusals
- [x] `Zenith_Pathfinding` uses it and its path units are unchanged — the units file is untouched and passes; see the caveat for what "unchanged" does NOT cover
- [x] `Zenith/Collections/CLAUDE.md` lists the new container
- [x] Every game's pinned baseline bumped from an OBSERVED run — Combat 2731→2737 and Zenithmon 4588→4594 OBSERVED; RenderTest 2834→2840 INFERRED (no gate line boots RenderTest's unit suite)

**Durable findings**: none

**Deviations / follow-ups**

- Review (LOW): no blocking findings. SHOULD-FIX 1: the ordering KEY is provably identical to the old `std::greater` min-heap, but TIE-BREAK order on equal f-cost is not (a different sift pattern), and no existing pathfinding unit asserts an exact route — so an equal-cost path choice could change with every gate green. A* optimality is preserved. Recorded here rather than claimed as "unchanged behaviour". SHOULD-FIX 2: the Goal said "stability under equal keys"; a binary heap is not stable and the unit deliberately does not claim it (it checks multiset + order).
- The Android/x86_64-emulator half named in the Goal was NOT run; the headless Null suite is the verification.
- Lane test run 2: this ticket verified fix 2 (whole-sln engine gate + TilePuzzle `--whole`) and fix 3 (1200 s Zenithmon unit gate) live; the first measure's red was caught by the widened engine gate.
<!-- agent:worklog:end -->
