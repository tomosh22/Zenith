# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `auto`)
- High-priority files (score >= 70): **20**
- P90 file priority: **65.9**
- Include cycles: **0**
- Duplicate clusters: **5**
- Include edges resolved: 2596 relative-path + 0 unique-basename (high confidence, gated); 2 ambiguous-basename (low confidence, suppressed from gates); 2 basename total (0%)
- External includes (middleware/third-party): 115
- Include redundancy: score **242** (52 duplicate, 190 transitive, 0 unresolved, 0 conflicting)
- Worst function: `Zenith\AI\Navigation\Zenith_NavMesh.cpp:783` `Zenith_NavMesh::GetRandomReachablePointInRadius` (score 82.5)
- Worst file: `Zenith\AI\Navigation\Zenith_NavMesh.cpp` (score 82.5)

## Architecture

All gated architecture/lint findings are allow-listed (ratchet green).

Report-only signals (not gated by default):
- max-module-cycles: 1 — e.g. `module-cycle: AssetHandling|Collections|Core|DataStream|DebugVariables|Editor|EntityComponent|FileAccess|Flux|Input|Maths|Physics|Prefab|Profiling|Scripting|TaskSystem|UI|Vulkan|Windows|ZenithECS`
- max-zone-of-pain-modules: 3 — e.g. `zone-of-pain: Maths`

- Modules: **25**, unmapped files: **4**, module-level cycles (report-only): **1**, hub files: **0**

| Module | Layer | Files | Ca | Ce | Instability | Abstractness | Distance | Zone |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Android | 0 | 12 | 1 | 4 | 0.80 | 0.00 | 0.20 | main-sequence |
| Collections | 0 | 5 | 16 | 2 | 0.11 | 0.00 | 0.89 | pain |
| Core | 0 | 56 | 24 | 8 | 0.25 | 0.00 | 0.75 | - |
| DataStream | 0 | 4 | 10 | 2 | 0.17 | 0.00 | 0.83 | pain |
| DebugVariables | 0 | 2 | 9 | 3 | 0.25 | 1.00 | 0.25 | - |
| FileAccess | 0 | 2 | 6 | 1 | 0.14 | n/a | 0.00 | - |
| Maths | 0 | 4 | 12 | 1 | 0.08 | 0.00 | 0.92 | pain |
| Profiling | 0 | 2 | 8 | 3 | 0.27 | 0.00 | 0.73 | - |
| Windows | 0 | 11 | 3 | 4 | 0.57 | 0.00 | 0.43 | - |
| Input | 1 | 7 | 7 | 2 | 0.22 | 0.00 | 0.78 | - |
| SaveData | 1 | 2 | 0 | 3 | 1.00 | 0.00 | 0.00 | main-sequence |
| TaskSystem | 1 | 2 | 4 | 4 | 0.50 | 0.00 | 0.50 | - |
| Telemetry | 1 | 2 | 0 | 5 | 1.00 | 0.00 | 0.00 | main-sequence |
| ZenithECS | 1 | 28 | 9 | 3 | 0.25 | 0.08 | 0.67 | - |
| AI | 2 | 35 | 0 | 10 | 1.00 | 0.05 | 0.05 | main-sequence |
| AssetHandling | 2 | 32 | 7 | 7 | 0.50 | 0.00 | 0.50 | - |
| EntityComponent | 2 | 38 | 5 | 13 | 0.72 | 0.00 | 0.28 | - |
| Flux | 2 | 180 | 9 | 11 | 0.55 | 0.01 | 0.44 | - |
| Physics | 2 | 5 | 4 | 7 | 0.64 | 0.00 | 0.36 | - |
| Prefab | 2 | 2 | 2 | 6 | 0.75 | 0.00 | 0.25 | - |
| Scripting | 2 | 7 | 4 | 5 | 0.56 | 0.25 | 0.19 | main-sequence |
| Vulkan | 2 | 27 | 1 | 6 | 0.86 | 0.00 | 0.14 | main-sequence |
| UI | 3 | 25 | 2 | 6 | 0.75 | 0.00 | 0.25 | - |
| Editor | 4 | 53 | 3 | 17 | 0.85 | 0.04 | 0.10 | main-sequence |
| EngineComposition | 5 | 5 | 0 | 13 | 1.00 | 0.00 | 0.00 | main-sequence |

_A/D are a regex proxy and report-only. module-cycles and zone-of-pain are reported but not gated by default; module-scope statics, forbidden tokens, missing PCH/`#pragma once`, leaf `g_xEngine`, layering, leaf and encapsulation findings ARE gated (threshold 0, ratcheted via allowlists)._

## Summary

- Files: **552**, SLOC: **96,417**, Functions: **6,062**, Classes: **1,472**
- Avg Cyclomatic: **18.8** (max 235), Avg Cognitive: **23.6** (max 431)
- Priority score distribution: **20** high (>=70, 3.6%), **142** medium, **390** low. P50=10.8, P90=65.9.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\AI\Navigation\Zenith_NavMesh.cpp:783` — `Zenith_NavMesh::GetRandomReachablePointInRadius` (score: 82.5)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=30, cognitive=53, nesting=3, LOC=226, params=4, returns=10
Score breakdown: cognitive=36%, cyclomatic=30%, length=18%, nesting=9%, params=6%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; check whether returns can be consolidated via result-holding variable.

### 2. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp:425` — `Zenith_TerrainEditor::HandleViewportInput` (score: 70.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=40, cognitive=48, nesting=2, LOC=123, params=1, returns=2
Score breakdown: cognitive=43%, cyclomatic=36%, length=13%, nesting=7%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 3. `Zenith\Core\Zenith_AutomatedTest.cpp:389` — `Zenith_AutomatedTestRunner::Tick` (score: 70.2)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`, `big-switch`
CC=52, cognitive=37, nesting=1, LOC=288, params=0, returns=18
Score breakdown: cognitive=40%, cyclomatic=36%, length=21%, nesting=4%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

### 4. `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp:290` — `Flux_IKSolver::SolveChain` (score: 69.8)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=40, nesting=4, LOC=119, params=4, returns=1
Score breakdown: cognitive=43%, cyclomatic=23%, nesting=14%, length=13%, params=7%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 5. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:192` — `ExecuteGBuffer` (score: 65.8)
Dominant signal: **cognitive**
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=15, cognitive=45, nesting=5, LOC=110, params=2, returns=1
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=19%, length=12%, params=4%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 6. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp:243` — `Zenith_TerrainEditor::ApplySplatDab` (score: 63.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=35, nesting=4, LOC=93, params=5, returns=2
Score breakdown: cognitive=41%, cyclomatic=22%, nesting=16%, length=11%, params=10%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 7. `Zenith\UI\Zenith_UIText.cpp:200` — `Zenith_UIText::Render` (score: 61.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=22, cognitive=37, nesting=2, LOC=124, params=1, returns=2
Score breakdown: cognitive=45%, cyclomatic=30%, length=15%, nesting=8%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 8. `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:165` — `Zenith_Vulkan_CommandBuffer::BuildDescriptorWritesForSet` (score: 60.4)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `many-params`
CC=22, cognitive=24, nesting=2, LOC=138, params=7, returns=0
Score breakdown: cyclomatic=30%, cognitive=30%, length=17%, params=14%, nesting=8%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 9. `Zenith\Core\Zenith_AutomatedTest.cpp:190` — `Zenith_AutomatedTestRunner::ParseCommandLine` (score: 59.3)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=39, nesting=2, LOC=89, params=2, returns=1
Score breakdown: cognitive=49%, cyclomatic=27%, length=11%, nesting=8%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 10. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp:82` — `Zenith_TerrainEditor::ApplyHeightDab` (score: 57.9)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `many-params`
CC=20, cognitive=19, nesting=3, LOC=160, params=6, returns=2
Score breakdown: cyclomatic=29%, cognitive=25%, length=21%, nesting=13%, params=13%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 11. `Zenith\Editor\Panels\Zenith_EditorPanel_TerrainEditor.cpp:49` — `RenderBrushSection` (score: 57.6)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=25, cognitive=26, nesting=3, LOC=113, params=1, returns=1
Score breakdown: cyclomatic=36%, cognitive=34%, length=15%, nesting=13%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 12. `Zenith\Telemetry\Zenith_Telemetry.cpp:459` — `Reader::ExportJson` (score: 57.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=30, nesting=3, LOC=156, params=2, returns=2
Score breakdown: cognitive=39%, cyclomatic=23%, length=20%, nesting=13%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 13. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Trees.cpp:112` — `Zenith_TerrainEditor::ApplyTreeDab` (score: 55.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=16, cognitive=24, nesting=3, LOC=135, params=5, returns=4
Score breakdown: cognitive=33%, cyclomatic=24%, length=18%, nesting=14%, params=11%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 14. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp:709` — `Zenith_TerrainEditor::RaycastHeightfield` (score: 53.5)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`, `many-returns`
CC=14, cognitive=29, nesting=4, LOC=85, params=3, returns=6
Score breakdown: cognitive=41%, cyclomatic=22%, nesting=19%, length=12%, params=7%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 15. `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp:109` — `Zenith_NavMeshAgent::Update` (score: 52.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=16, cognitive=23, nesting=3, LOC=138, params=3, returns=3
Score breakdown: cognitive=33%, cyclomatic=26%, length=20%, nesting=14%, params=7%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 16. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Erosion.cpp:107` — `Zenith_TerrainEditor::RunHydraulicDroplets` (score: 52.0)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=14, cognitive=27, nesting=3, LOC=135, params=2, returns=1
Score breakdown: cognitive=39%, cyclomatic=22%, length=20%, nesting=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 17. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:263` — `RenderCategoryTab` (score: 51.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 18. `Zenith\Editor\Zenith_EditorAutomation.cpp:1510` — `Zenith_EditorAutomation::ExecuteAction` (score: 51.2)
Dominant signal: **cyclomatic**
Tags: `very-long-function`, `complex-branching`, `big-switch`
CC=59, cognitive=10, nesting=1, LOC=472, params=1, returns=3
Score breakdown: cyclomatic=49%, length=29%, cognitive=15%, nesting=5%, params=2%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

### 19. `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp:691` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 51.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=94, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 20. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:295` — `Flux_MeshGeometry::Combine` (score: 51.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 82.5 | 139 | 235 | 5 | 936 | 37 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp` | `low-mi`, `god-file` | 77.2 | 198 | 240 | 4 | 1079 | 39 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 76.9 | 83 | 124 | 4 | 648 | 35 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp` | `low-mi` | 75.1 | 53 | 69 | 4 | 316 | 6 |
| `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` | `low-mi` | 74.5 | 22 | 69 | 6 | 216 | 7 |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | `low-mi` | 73.6 | 113 | 124 | 2 | 477 | 14 |
| `Zenith\Telemetry\Zenith_Telemetry.cpp` | `low-mi` | 73.2 | 72 | 143 | 4 | 600 | 26 |
| `Zenith\UI\Zenith_UIText.cpp` | `low-mi` | 72.7 | 56 | 87 | 3 | 309 | 12 |
| `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` | `low-mi`, `god-file` | 72.4 | 107 | 112 | 3 | 802 | 43 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 72.3 | 116 | 215 | 5 | 849 | 28 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 71 | 140 | 4 | 553 | 13 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_TerrainEditor.cpp` | `low-mi` | 71.5 | 59 | 63 | 3 | 330 | 7 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi`, `god-file` | 71.4 | 235 | 431 | 4 | 1050 | 51 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 71.0 | 137 | 235 | 4 | 616 | 38 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 70.3 | 65 | 83 | 4 | 461 | 35 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 64 | 107 | 4 | 628 | 35 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Execution.cpp` | `low-mi` | 70.3 | 55 | 105 | 4 | 255 | 16 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 88 | 139 | 4 | 493 | 37 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.1 | 153 | 266 | 4 | 881 | 40 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 70.0 | 81 | 126 | 4 | 644 | 46 |
| `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp` | `low-mi` | 69.9 | 45 | 70 | 3 | 286 | 11 |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | `low-mi` | 69.9 | 44 | 74 | 4 | 374 | 15 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 68 | 113 | 4 | 523 | 29 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Erosion.cpp` | `low-mi` | 69.8 | 38 | 61 | 3 | 270 | 4 |
| `Zenith\AssetHandling\Zenith_MaterialAsset.cpp` | `low-mi` | 69.5 | 52 | 72 | 3 | 437 | 22 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate. **Caveat: header-inline only.** Method bodies that live in separate `.cpp` files are invisible at this scope, so this is a header-inline cohesion signal, not a full-class score.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_EditorAutomation` | `Zenith\Editor\Zenith_EditorAutomation.h:266` | 1.00 | 6 | 4 |
| `Flux_BindingSlot` | `Zenith\Flux\Flux_Types.h:60` | 1.00 | 3 | 3 |
| `Flux_TerrainStreamingManagerImpl` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManagerImpl.h:227` | 0.98 | 4 | 17 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:131` | 0.98 | 3 | 20 |
| `Zenith_AssetRegistry` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:53` | 0.98 | 10 | 5 |
| `Zenith_TerrainEditor` | `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.h:154` | 0.98 | 19 | 48 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:302` | 0.98 | 7 | 36 |
| `Zenith_SceneSystem` | `Zenith\ZenithECS\Zenith_SceneSystem.h:82` | 0.97 | 11 | 30 |
| `Zenith_MaterialAsset` | `Zenith\AssetHandling\Zenith_MaterialAsset.h:56` | 0.96 | 76 | 32 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Flux_IBLImpl` | `Zenith\Flux\IBL\Flux_IBLImpl.h:42` | 0.96 | 3 | 24 |
| `Zenith_D3D12_MemoryManager` | `Zenith\D3D12\Zenith_D3D12_MemoryManager.h:28` | 0.95 | 46 | 11 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:40` | 0.95 | 20 | 22 |
| `Flux_GizmosImpl` | `Zenith\Flux\Gizmos\Flux_GizmosImpl.h:55` | 0.95 | 4 | 22 |
| `Zenith_MeshAsset` | `Zenith\AssetHandling\Zenith_MeshAsset.h:28` | 0.95 | 15 | 25 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 252 | 12 |  |
| `Zenith\Core\Zenith_Engine.h` | 146 | 1 |  |
| `Zenith\Collections\Zenith_Vector.h` | 76 | 3 |  |
| `Zenith\Maths\Zenith_Maths.h` | 76 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 64 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 61 | 0 |  |
| `Zenith\Flux\Flux.h` | 60 | 4 |  |
| `Zenith\Flux\Flux_GraphicsImpl.h` | 59 | 4 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 47 | 3 |  |
| `Zenith\ZenithECS\Zenith_SceneSystem.h` | 46 | 6 |  |
| `Zenith\Flux\Flux_Buffers.h` | 38 | 2 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 36 | 4 |  |
| `Zenith\ZenithECS\Zenith_Entity.h` | 36 | 1 |  |
| `Zenith\Collections\Zenith_HashMap.h` | 35 | 3 |  |
| `Zenith\Flux\Slang\Flux_SlangCompiler.h` | 33 | 3 |  |

## Near-duplicate functions (5 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:196` — `Zenith_TestAssertEq` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:224` — `Zenith_TestAssertGt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:238` — `Zenith_TestAssertLt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:252` — `Zenith_TestAssertGe` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:266` — `Zenith_TestAssertLe` (score 15.8, LOC 12)

### Cluster 2 (2 functions)
- `Zenith\Editor\Zenith_Editor.cpp:1362` — `Zenith_Editor::HandlePendingSceneLoadDeferred` (score 37.1, LOC 84)
- `Zenith\Editor\Zenith_Editor.cpp:744` — `Zenith_Editor::HandlePendingSceneLoad` (score 36.8, LOC 101)

### Cluster 3 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1453` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1542` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 4 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan_Shader.cpp:266` — `Zenith_Vulkan_Shader::MergeReflection` (score 22.8, LOC 31)
- `Zenith\D3D12\Zenith_D3D12_Pipeline.h:106` — `MergeReflection` (score 22.5, LOC 26)

### Cluster 5 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:32` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:53` — `IsWriteAccess` (score 15.2, LOC 20)

## Include redundancy

Per-file `#include` findings: duplicates, transitive coverage, and same-basename conflicts. Each row is a concrete cleanup candidate.

### Duplicate includes (52 total)
| File | Line | Include | First seen |
| --- | ---: | --- | ---: |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 8 | `Core/Multithreading/Zenith_Multithreading.h` | 6 |
| `Zenith\AssetHandling\Zenith_TextureAsset.cpp` | 7 | `Flux/Flux_GraphicsImpl.h` | 6 |
| `Zenith\Core\Zenith_Engine.cpp` | 67 | `Flux/Flux_GraphicsImpl.h` | 33 |
| `Zenith\Core\Zenith_Engine.cpp` | 70 | `Editor/Zenith_Editor.h` | 69 |
| `Zenith\Core\Zenith_Engine.cpp` | 72 | `Editor/Zenith_EditorAutomation.h` | 71 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp` | 21 | `Flux/Flux_GraphicsImpl.h` | 20 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Viewport.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Editor\Zenith_Editor.cpp` | 83 | `Core/Zenith_CommandLine.h` | 15 |
| `Zenith\Editor\Zenith_Editor_MaterialUI.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Editor\Zenith_Editor_Menu.cpp` | 7 | `Zenith_Editor.h` | 6 |
| `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 10 | `Flux/Flux_GraphicsImpl.h` | 9 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 13 | `Flux/IBL/Flux_IBLImpl.h` | 12 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 5 | `Flux/DynamicLights/Flux_LightClusteringImpl.h` | 4 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Flux\Flux_Graphics.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Flux\Fog\Flux_Fog.cpp` | 5 | `Flux/Fog/Flux_FogImpl.h` | 4 |
| `Zenith\Flux\Fog\Flux_Fog.cpp` | 17 | `Flux/Flux_GraphicsImpl.h` | 16 |
| `Zenith\Flux\Fog\Flux_FroxelFog.cpp` | 6 | `Flux/Fog/Flux_FroxelFogImpl.h` | 5 |
| `Zenith\Flux\Fog\Flux_FroxelFog.cpp` | 11 | `Flux/Flux_GraphicsImpl.h` | 10 |
| `Zenith\Flux\Fog\Flux_GodRaysFog.cpp` | 5 | `Flux/Fog/Flux_GodRaysFogImpl.h` | 4 |
| `Zenith\Flux\Fog\Flux_GodRaysFog.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Flux\Fog\Flux_RaymarchFog.cpp` | 7 | `Flux/Fog/Flux_RaymarchFogImpl.h` | 6 |
| `Zenith\Flux\Fog\Flux_RaymarchFog.cpp` | 12 | `Flux/Flux_GraphicsImpl.h` | 11 |
| `Zenith\Flux\Fog\Flux_VolumeFog.cpp` | 5 | `Flux/Fog/Flux_VolumeFogImpl.h` | 4 |

### Transitively-covered includes (190 total)
| File | Line | Include | Covered by |
| --- | ---: | --- | --- |
| `FluxCompiler\FluxCompiler.cpp` | 3 | `Core/Memory/Zenith_MemoryManagement_Disabled.h` | `Zenith\Core\Zenith.h:1` |
| `FluxCompiler\FluxCompiler.cpp` | 6 | `DataStream/Zenith_DataStream.h` | `Zenith\Flux\Slang\Flux_SlangCompiler.h:4` |
| `Tests\SentinelECS\main.cpp` | 39 | `ZenithECS/Zenith_Scene.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 40 | `ZenithECS/Zenith_Entity.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 41 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 47 | `DataStream/Zenith_DataStream.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | 7 | `Collections/Zenith_HashMap.h` | `Zenith\Profiling\Zenith_Profiling.h:5` |
| `Zenith\Android\Callstack\Zenith_Android_Callstack.cpp` | 3 | `Zenith_Android_Callstack.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 4 | `Zenith_Android_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Zenith_Android_Window.cpp` | 3 | `Zenith_Android_Window.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.cpp` | 14 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:2` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 5 | `Core/Multithreading/Zenith_Multithreading.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 7 | `Collections/Zenith_HashMap.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 3 | `AssetHandling/Zenith_AssetRegistry.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 5 | `DataStream/Zenith_DataStream.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 6 | `Core/Zenith_Engine.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.h` | 6 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetHandle.h:4` |
| `Zenith\AssetHandling\Zenith_MaterialAsset.h` | 6 | `Maths/Zenith_Maths.h` | `Zenith\AssetHandling\Zenith_MaterialParamTable.h:5` |
| `Zenith\AssetHandling\Zenith_MeshAsset.h` | 5 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:3` |
| `Zenith\AssetHandling\Zenith_PropertyTuning.cpp` | 3 | `DataStream/Zenith_DataStream.h` | `Zenith\AssetHandling\Zenith_PropertyTuning.h:2` |
| `Zenith\Core\Memory\Zenith_MemoryManagement.cpp` | 15 | `Zenith_MemoryManagement.h` | `Zenith\Core\Zenith.h:13` |
| `Zenith\Core\Zenith.h` | 284 | `ZenithConfig.h` | `Zenith\Core\FrameContext.h:42` |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | 6 | `Core/Zenith_Core.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_BenchECS.cpp` | 11 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:10` |

## External includes (115 resolved to 35 middleware/third-party header(s))

These resolve into excluded directories (Middleware/, ThirdParty/, External/, …). Listed for visibility only — not flagged as issues.

| Count | Header |
| ---: | --- |
| 45 | `imgui.h` |
| 24 | `glm/glm.hpp` |
| 7 | `vulkan/vulkan.hpp` |
| 4 | `backends/imgui_impl_vulkan.h` |
| 4 | `vma/vk_mem_alloc.h` |
| 2 | `GLFW/glfw3.h` |
| 1 | `AI/BehaviorTree/Zenith_BehaviorTree.Tests.inl` |
| 1 | `AI/BehaviorTree/Zenith_Blackboard.Tests.inl` |
| 1 | `AI/Navigation/Zenith_NavMesh.Tests.inl` |
| 1 | `AI/Navigation/Zenith_NavMeshAgent.Tests.inl` |
| 1 | `AI/Navigation/Zenith_NavMeshGenerator.Tests.inl` |
| 1 | `AI/Navigation/Zenith_Pathfinding.Tests.inl` |
| 1 | `AI/Perception/Zenith_PerceptionSystem.Tests.inl` |
| 1 | `AI/Squad/Zenith_Formation.Tests.inl` |
| 1 | `AI/Squad/Zenith_Squad.Tests.inl` |
| 1 | `AI/Squad/Zenith_TacticalPoint.Tests.inl` |
| 1 | `AI/Zenith_AIDebugVariables.Tests.inl` |
| 1 | `AssetHandling/Zenith_AssetRegistry.Tests.inl` |
| 1 | `AssetHandling/Zenith_MaterialAsset.Tests.inl` |
| 1 | `AssetHandling/Zenith_PropertyTuning.Tests.inl` |
| 1 | `Core/Zenith_UnitTests.Tests.inl` |
| 1 | `UnitTests/Zenith_UnitTests.h` |
| 1 | `Core/Zenith_PropertySystem.Tests.inl` |
| 1 | `../../../Tools/Zenith_Tools_TextureExport.h` |
| 1 | `Editor/TerrainEditor/Zenith_TerrainEditor.Tests.inl` |
| 1 | `imgui_internal.h` |
| 1 | `Editor/Zenith_Editor.Tests.inl` |
| 1 | `Editor/Zenith_EditorAutomation.Tests.inl` |
| 1 | `EntityComponent/Components/Zenith_GraphComponent.Tests.inl` |
| 1 | `EntityComponent/Components/Zenith_LightComponent.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_AnimationClip.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_BlendTree.Tests.inl` |
| 1 | `Physics/Zenith_Physics.Tests.inl` |
| 1 | `Scripting/Zenith_Scripting.Tests.inl` |
| 1 | `backends/imgui_impl_glfw.h` |

## Tech-debt markers (10 total; showing first 30)

- `Zenith\Flux\Flux.h:79` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:88` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:87` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:524` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:173` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash set
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:160` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:14` **TODO** Replace with engine hash set
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1387` **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:444` **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\AI\Navigation` | 9 | 2,859 | 82.5 | 13.4 | 39.8 | `Zenith\AI\Navigation\Zenith_NavMesh.cpp` |
| `Zenith\Editor\TerrainEditor` | 10 | 2,555 | 77.2 | 33.5 | 36.2 | `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp` |
| `Zenith\Flux\MeshAnimation` | 20 | 5,748 | 76.9 | 27.3 | 34.0 | `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` |
| `Zenith\Flux\AnimatedMeshes` | 2 | 251 | 74.5 | 38.6 | 11.5 | `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` |
| `Zenith\Core` | 40 | 3,686 | 73.6 | 3.0 | 10.2 | `Zenith\Core\Zenith_AutomatedTest.cpp` |
| `Zenith\Telemetry` | 2 | 746 | 73.2 | 41.7 | 37.0 | `Zenith\Telemetry\Zenith_Telemetry.cpp` |
| `Zenith\UI` | 25 | 4,078 | 72.7 | 13.7 | 22.0 | `Zenith\UI\Zenith_UIText.cpp` |
| `Zenith\Vulkan` | 25 | 6,283 | 72.4 | 17.4 | 24.2 | `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` |
| `Zenith\Editor\Panels` | 24 | 4,857 | 71.5 | 13.0 | 34.5 | `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,360 | 71.4 | 66.5 | 118.2 | `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,525 | 70.3 | 56.6 | 28.5 | `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` |
| `Zenith\Flux\Terrain` | 6 | 1,590 | 70.1 | 9.8 | 30.7 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Physics` | 5 | 1,447 | 70.0 | 8.6 | 33.4 | `Zenith\Physics\Zenith_Physics.cpp` |
| `Zenith\AI\Perception` | 2 | 653 | 69.8 | 38.6 | 34.5 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\AssetHandling` | 32 | 5,215 | 69.5 | 13.4 | 17.3 | `Zenith\AssetHandling\Zenith_MaterialAsset.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple. Architecture: module Abstractness (A) and Distance (D) are a regex proxy (pure-virtual presence per header) and are reported, never gated; a std::function/pimpl-free engine tends to show uniformly low A. Direction/encapsulation gates exclude only AMBIGUOUS basename-resolved edges (a basename shared by 2+ files); a unique basename is high-confidence and IS gated, like a relative-path match.
