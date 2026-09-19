---
id: "c3c41348-51fa-4ff4-82dd-4f1980dc08cf"
key: "ZEN-33"
type: "STORY"
title: "De-globalisation phase 6: Flux_Graphics and the Vulkan backend statics"
category: "Engine"
parent: "ZEN-24"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 33
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:33:31.565Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
De-globalisation program (ZEN-18 spine; `Docs/design/EngineRefactor_Phase-1_Inventory.md`), **phase 6: Flux_Graphics and the Vulkan backend.** Inventory surface: the Flux namespace and Flux_Graphics (about 12 statics) with a Flux_Renderer shell, then the Zenith_Vulkan backend, MemoryManager, VRAM and PerFrame statics. Replace each `g_xEngine.X()` reach and each module-scope mutable static in this surface with the injection shape ZEN-18 records (constructor or explicit `Initialise(...)` wiring from the composition root; per-frame state on a passed context), keeping behaviour identical.

Rules for every phase: the per-file ratchet in `Tools/engine_singleton_allowlist.txt` is SHRUNK for each touched file in the same commit (never grown; the gate refuses growth, and a file left at its old count is a phase that did not finish); `max-static-violations` stays green because the statics are removed, not renamed; the reporting unit from ZEN-18 shows the total falling from the count ZEN-18 records for this surface toward the phase target. Where a subsystem is reached from game code, the game call sites move in the same commit so every game still builds (the Engine gates compile every `.zproj` game). Null and D3D12 must keep signatures matching Vulkan's, and every RenderBackend prefix (Vulkan_, Null_, D3D12_) must build; the D3D12 link proof is the neutrality gate.

Verification: the engine boot suite and every game's boot suite are the behavioural guard (the change is wiring, not logic); the architecture gates (`Tools/complexity_profiles.json` `architecture,lints` group) are the structural guard. No test is deleted or loosened to make a count pass.

## Definition of Done
- [ ] Every site in this phase's surface reaches its dependency by injection; the phase's files read lower in `Tools/engine_singleton_allowlist.txt` and none higher
- [ ] The static-violation and singleton ratchets are green without any allowlist growth, and the ZEN-18 reporting unit shows the total at or below the phase target
- [ ] Every game builds and every boot suite passes with no behaviour change
- [ ] Every game's pinned baseline bumped from an OBSERVED run
