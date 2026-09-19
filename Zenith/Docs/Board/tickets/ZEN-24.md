---
id: "243a6c6a-88ef-4eef-9485-eeb19aad8a51"
key: "ZEN-24"
type: "EPIC"
title: "Engine architecture: de-globalisation, leaf libraries and module layout"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: null
position: 24
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:32:26.252Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The engine's structural debt, as one epic: the de-globalisation program that `Docs/design/EngineRefactor_Phase-1_Inventory.md` inventoried (`g_xEngine` is reached from 1,569 sites in 227 engine files and 3,651 game sites today; `Tools/engine_singleton_allowlist.txt` is the shrink-only per-file ratchet the complexity gate enforces), the leaf-library layout the Sharpmake build has to work around (ZEN-14), the render-backend directory move (ZEN-16), the SceneManager/SceneData include cycle the junior exploration deferred, and the async asset loader that is documented as NOT IMPLEMENTED. Each child is a self-contained refactor with the cross-game sweep as its gate: the Engine category builds every game with a `.zproj`, and any change that moves a backend-neutral unit moves every game's pinned baseline.

## Definition of Done
- [ ] Every child issue Done or explicitly deferred with a recorded reason
- [ ] `Tools/engine_singleton_allowlist.txt` reads zero for every file outside the composition root, or the residue is listed with its reason
