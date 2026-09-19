---
id: "583ff1eb-6eb4-45a8-b4af-e7da8d0aba3d"
key: "ZEN-25"
type: "EPIC"
title: "Flux: view neutrality and renderer cleanup"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "COMPLEX"
risk: "MEDIUM"
storyPoints: null
position: 25
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:32:29.891Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Flux cleanup that the games do not block on but that every renderer change pays for: a frame must not know whether it renders to the swapchain, the editor viewport or a preview (ZEN-12), the shadow matrices belong to the shadow system (ZEN-19), the render-submit gate in the frame loop should not be a boolean threaded through the tick (ZEN-20), the by-name shader-binder overloads are the pre-reflection API and must go (ZEN-22), the unified GPU mesh path's deferred Stage 6 (crowd batching via `DrawIndexedIndirectCount`), the teardown scratch overflow that makes every windowed run exit non-zero, and the FluxCompiler lint that closes the last texture-decode gap `Docs/design/Photorealism.md` §4 names.

## Definition of Done
- [ ] Every child issue Done or explicitly deferred with a recorded reason
- [ ] `Zenith/Flux/CLAUDE.md` describes the shipped state after each child, in the same commit as the change
