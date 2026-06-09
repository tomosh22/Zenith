# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `tree-sitter`)
- High-priority files (score >= 70): **13**
- P90 file priority: **65.8**
- Include cycles: **0**
- Duplicate clusters: **5**
- Include edges resolved: 2296 relative-path + 0 unique-basename (high confidence, gated); 2 ambiguous-basename (low confidence, suppressed from gates); 2 basename total (0%)
- External includes (middleware/third-party): 104
- Include redundancy: score **237** (68 duplicate, 167 transitive, 0 unresolved, 2 conflicting)
- Worst function: `Zenith\AI\Navigation\Zenith_NavMesh.cpp:783` `Zenith_NavMesh::GetRandomReachablePointInRadius` (score 82.5)
- Worst file: `Zenith\AI\Navigation\Zenith_NavMesh.cpp` (score 82.5)

## Architecture

All gated architecture/lint findings are allow-listed (ratchet green).

Report-only signals (not gated by default):
- max-module-cycles: 1 — e.g. `module-cycle: AssetHandling|Collections|Core|DataStream|DebugVariables|Editor|EntityComponent|FileAccess|Flux|Input|Maths|Physics|Prefab|Profiling|TaskSystem|UI|Vulkan|Windows|ZenithECS`
- max-zone-of-pain-modules: 3 — e.g. `zone-of-pain: Maths`

- Modules: **24**, unmapped files: **4**, module-level cycles (report-only): **1**, hub files: **0**

| Module | Layer | Files | Ca | Ce | Instability | Abstractness | Distance | Zone |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Android | 0 | 12 | 1 | 4 | 0.80 | 0.00 | 0.20 | main-sequence |
| Collections | 0 | 5 | 15 | 2 | 0.12 | 0.00 | 0.88 | pain |
| Core | 0 | 51 | 23 | 8 | 0.26 | 0.00 | 0.74 | - |
| DataStream | 0 | 4 | 8 | 2 | 0.20 | 0.00 | 0.80 | pain |
| DebugVariables | 0 | 2 | 8 | 3 | 0.27 | 1.00 | 0.27 | - |
| FileAccess | 0 | 2 | 6 | 1 | 0.14 | n/a | 0.00 | - |
| Maths | 0 | 4 | 11 | 1 | 0.08 | 0.00 | 0.92 | pain |
| Profiling | 0 | 2 | 7 | 3 | 0.30 | 0.00 | 0.70 | - |
| Windows | 0 | 11 | 3 | 4 | 0.57 | 0.00 | 0.43 | - |
| Input | 1 | 7 | 7 | 2 | 0.22 | 0.00 | 0.78 | - |
| SaveData | 1 | 2 | 0 | 3 | 1.00 | 0.00 | 0.00 | main-sequence |
| TaskSystem | 1 | 2 | 4 | 4 | 0.50 | 0.00 | 0.50 | - |
| Telemetry | 1 | 2 | 0 | 5 | 1.00 | 0.00 | 0.00 | main-sequence |
| ZenithECS | 1 | 28 | 8 | 3 | 0.27 | 0.08 | 0.65 | - |
| AI | 2 | 35 | 0 | 10 | 1.00 | 0.05 | 0.05 | main-sequence |
| AssetHandling | 2 | 26 | 7 | 7 | 0.50 | 0.00 | 0.50 | - |
| EntityComponent | 2 | 33 | 6 | 11 | 0.65 | 0.07 | 0.29 | - |
| Flux | 2 | 174 | 9 | 11 | 0.55 | 0.01 | 0.44 | - |
| Physics | 2 | 5 | 4 | 7 | 0.64 | 0.00 | 0.36 | - |
| Prefab | 2 | 2 | 2 | 6 | 0.75 | 0.00 | 0.25 | - |
| Vulkan | 2 | 22 | 1 | 6 | 0.86 | 0.00 | 0.14 | main-sequence |
| UI | 3 | 25 | 2 | 6 | 0.75 | 0.00 | 0.25 | - |
| Editor | 4 | 37 | 3 | 14 | 0.82 | 0.07 | 0.10 | main-sequence |
| EngineComposition | 5 | 5 | 0 | 12 | 1.00 | 0.00 | 0.00 | main-sequence |

_A/D are a regex proxy and report-only. module-cycles and zone-of-pain are reported but not gated by default; module-scope statics, forbidden tokens, missing PCH/`#pragma once`, leaf `g_xEngine`, layering, leaf and encapsulation findings ARE gated (threshold 0, ratcheted via allowlists)._

## Summary

- Files: **502**, SLOC: **88,658**, Functions: **5,589**, Classes: **1,423**
- Avg Cyclomatic: **18.1** (max 291), Avg Cognitive: **23.0** (max 406)
- Priority score distribution: **13** high (>=70, 2.6%), **133** medium, **356** low. P50=10.5, P90=65.8.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\AI\Navigation\Zenith_NavMesh.cpp:783` — `Zenith_NavMesh::GetRandomReachablePointInRadius` (score: 82.5)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=30, cognitive=53, nesting=3, LOC=226, params=4, returns=10
Score breakdown: cognitive=36%, cyclomatic=30%, length=18%, nesting=9%, params=6%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; check whether returns can be consolidated via result-holding variable.

### 2. `Zenith\Core\Zenith_AutomatedTest.cpp:389` — `Zenith_AutomatedTestRunner::Tick` (score: 70.2)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`, `big-switch`
CC=52, cognitive=37, nesting=1, LOC=288, params=0, returns=18
Score breakdown: cognitive=40%, cyclomatic=36%, length=21%, nesting=4%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

### 3. `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp:290` — `Flux_IKSolver::SolveChain` (score: 69.8)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=40, nesting=4, LOC=119, params=4, returns=1
Score breakdown: cognitive=43%, cyclomatic=23%, nesting=14%, length=13%, params=7%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 4. `Zenith\UI\Zenith_UIText.cpp:200` — `Zenith_UIText::Render` (score: 61.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=22, cognitive=37, nesting=2, LOC=124, params=1, returns=2
Score breakdown: cognitive=45%, cyclomatic=30%, length=15%, nesting=8%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 5. `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:167` — `Zenith_Vulkan_CommandBuffer::BuildDescriptorWritesForSet` (score: 60.4)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `many-params`
CC=22, cognitive=24, nesting=2, LOC=138, params=7, returns=0
Score breakdown: cyclomatic=30%, cognitive=30%, length=17%, params=14%, nesting=8%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 6. `Zenith\Core\Zenith_AutomatedTest.cpp:190` — `Zenith_AutomatedTestRunner::ParseCommandLine` (score: 59.3)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=39, nesting=2, LOC=89, params=2, returns=1
Score breakdown: cognitive=49%, cyclomatic=27%, length=11%, nesting=8%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 7. `Zenith\Telemetry\Zenith_Telemetry.cpp:459` — `Reader::ExportJson` (score: 57.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=30, nesting=3, LOC=156, params=2, returns=2
Score breakdown: cognitive=39%, cyclomatic=23%, length=20%, nesting=13%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 8. `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp:109` — `Zenith_NavMeshAgent::Update` (score: 52.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=16, cognitive=23, nesting=3, LOC=138, params=3, returns=3
Score breakdown: cognitive=33%, cyclomatic=26%, length=20%, nesting=14%, params=7%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 9. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:262` — `RenderCategoryTab` (score: 51.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 10. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1169` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 51.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=94, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 11. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:295` — `Flux_MeshGeometry::Combine` (score: 51.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 12. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1586` — `Flux_RenderGraph::AssignClearFlags` (score: 49.5)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=18, cognitive=32, nesting=3, LOC=40, params=0, returns=0
Score breakdown: cognitive=48%, cyclomatic=30%, nesting=15%, length=6%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 13. `Zenith\Core\Zenith_Engine.cpp:308` — `Zenith_Engine::Initialise` (score: 49.1)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`
CC=16, cognitive=21, nesting=2, LOC=410, params=0, returns=1
Score breakdown: cognitive=32%, length=31%, cyclomatic=27%, nesting=10%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; extract branches into named helpers or strategy objects.

### 14. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:845` — `Zenith_NavMeshGenerator::EmitQuadsFromSpans` (score: 48.6)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=9, cognitive=32, nesting=4, LOC=62, params=2, returns=1
Score breakdown: cognitive=49%, nesting=21%, cyclomatic=15%, length=10%, params=5%

Suggested approach: break up into smaller, named steps.

### 15. `Zenith\Editor\Zenith_Editor.cpp:335` — `Zenith_Editor::Update` (score: 48.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=25, nesting=3, LOC=120, params=0, returns=4
Score breakdown: cognitive=39%, cyclomatic=27%, length=18%, nesting=15%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 16. `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp:526` — `Zenith_TerrainComponent::LoadAndCombineLowLODChunks` (score: 47.7)
Dominant signal: **cognitive**
Tags: `long-function`
CC=12, cognitive=22, nesting=3, LOC=116, params=4, returns=1
Score breakdown: cognitive=35%, cyclomatic=21%, length=18%, nesting=16%, params=10%

Suggested approach: extract cohesive blocks into helpers.

### 17. `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp:196` — `Flux_SkeletonPose::SampleFromClip` (score: 47.3)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=11, cognitive=29, nesting=3, LOC=68, params=3, returns=0
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=16%, length=11%, params=8%

Suggested approach: break up into smaller, named steps.

### 18. `Zenith\Editor\Zenith_Editor_Menu.cpp:53` — `Zenith_Editor::RenderFileMenu` (score: 47.1)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=12, cognitive=27, nesting=4, LOC=91, params=0, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=21%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 19. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:417` — `Flux_MeshGeometry::GenerateLayoutAndVertexData` (score: 47.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=25, nesting=2, LOC=110, params=0, returns=0
Score breakdown: cognitive=40%, cyclomatic=32%, length=18%, nesting=11%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 20. `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp:401` — `Flux_TerrainStreamingManagerImpl::RequestNearbyHighLOD` (score: 46.9)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=12, cognitive=27, nesting=3, LOC=72, params=3, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=16%, length=12%, params=8%

Suggested approach: break up into smaller, named steps.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 82.5 | 139 | 235 | 5 | 936 | 37 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 76.9 | 83 | 124 | 4 | 648 | 35 |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | `low-mi` | 73.6 | 113 | 124 | 2 | 477 | 14 |
| `Zenith\Telemetry\Zenith_Telemetry.cpp` | `low-mi` | 73.2 | 72 | 143 | 4 | 600 | 26 |
| `Zenith\UI\Zenith_UIText.cpp` | `low-mi` | 72.7 | 56 | 87 | 3 | 309 | 12 |
| `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` | `low-mi`, `god-file` | 72.4 | 107 | 112 | 3 | 801 | 43 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 72.3 | 116 | 215 | 5 | 849 | 28 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 71 | 140 | 4 | 552 | 13 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.4 | 291 | 406 | 4 | 1432 | 89 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 64 | 107 | 4 | 628 | 35 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 88 | 139 | 4 | 493 | 37 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.1 | 151 | 264 | 4 | 873 | 40 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 70.0 | 80 | 124 | 4 | 625 | 44 |
| `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp` | `low-mi` | 69.9 | 45 | 70 | 3 | 286 | 11 |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | `low-mi` | 69.9 | 44 | 74 | 4 | 374 | 15 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 68 | 113 | 4 | 523 | 29 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 69.5 | 96 | 146 | 4 | 535 | 19 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 522 | 17 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 451 | 35 |
| `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` | `low-mi` | 69.2 | 148 | 171 | 4 | 656 | 26 |
| `Zenith\Editor\Zenith_Editor.cpp` | `low-mi`, `god-file` | 68.8 | 150 | 208 | 3 | 1240 | 64 |
| `Zenith\EntityComponent\Components\Zenith_ColliderComponent.cpp` | `low-mi` | 68.8 | 91 | 107 | 4 | 733 | 26 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi` | 68.8 | 76 | 128 | 4 | 303 | 16 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 68.4 | 28 | 48 | 4 | 211 | 9 |
| `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` | `low-mi` | 68.3 | 58 | 66 | 4 | 314 | 12 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate. **Caveat: header-inline only.** Method bodies that live in separate `.cpp` files are invisible at this scope, so this is a header-inline cohesion signal, not a full-class score.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_EditorAutomation` | `Zenith\Editor\Zenith_EditorAutomation.h:215` | 1.00 | 6 | 4 |
| `Flux_BindingSlot` | `Zenith\Flux\Flux_Types.h:60` | 1.00 | 3 | 3 |
| `Flux_TerrainStreamingManagerImpl` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManagerImpl.h:222` | 0.98 | 4 | 16 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:235` | 0.98 | 3 | 20 |
| `Zenith_AssetRegistry` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:54` | 0.98 | 10 | 5 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:293` | 0.97 | 6 | 33 |
| `Zenith_SceneSystem` | `Zenith\ZenithECS\Zenith_SceneSystem.h:82` | 0.97 | 11 | 31 |
| `Flux_RenderGraph_Pass` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:130` | 0.96 | 3 | 18 |
| `Flux_TerrainImpl` | `Zenith\Flux\Terrain\Flux_TerrainImpl.h:16` | 0.96 | 3 | 26 |
| `Flux_GizmosImpl` | `Zenith\Flux\Gizmos\Flux_GizmosImpl.h:57` | 0.96 | 4 | 25 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Flux_SkyboxImpl` | `Zenith\Flux\Skybox\Flux_SkyboxImpl.h:108` | 0.96 | 10 | 24 |
| `Flux_IBLImpl` | `Zenith\Flux\IBL\Flux_IBLImpl.h:42` | 0.96 | 3 | 24 |
| `Zenith_D3D12_MemoryManager` | `Zenith\D3D12\Zenith_D3D12_MemoryManager.h:29` | 0.95 | 46 | 11 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:40` | 0.95 | 20 | 22 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 224 | 12 |  |
| `Zenith\Core\Zenith_Engine.h` | 129 | 1 |  |
| `Zenith\Collections\Zenith_Vector.h` | 72 | 3 |  |
| `Zenith\Maths\Zenith_Maths.h` | 68 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 61 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 58 | 0 |  |
| `Zenith\Flux\Flux.h` | 58 | 4 |  |
| `Zenith\Flux\Flux_GraphicsImpl.h` | 50 | 4 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 42 | 3 |  |
| `Zenith\ZenithECS\Zenith_SceneSystem.h` | 40 | 6 |  |
| `Zenith\ZenithECS\Zenith_Entity.h` | 35 | 1 |  |
| `Zenith\Flux\Flux_Buffers.h` | 33 | 2 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 33 | 4 |  |
| `Zenith\Flux\Slang\Flux_SlangCompiler.h` | 33 | 3 |  |
| `Zenith\ZenithECS\Zenith_Scene.h` | 31 | 0 |  |

## Near-duplicate functions (5 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:196` — `Zenith_TestAssertEq` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:224` — `Zenith_TestAssertGt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:238` — `Zenith_TestAssertLt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:252` — `Zenith_TestAssertGe` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:266` — `Zenith_TestAssertLe` (score 15.8, LOC 12)

### Cluster 2 (2 functions)
- `Zenith\Editor\Zenith_Editor.cpp:588` — `Zenith_Editor::HandlePendingSceneLoad` (score 39.5, LOC 116)
- `Zenith\Editor\Zenith_Editor.cpp:1247` — `Zenith_Editor::HandlePendingSceneLoadDeferred` (score 37.1, LOC 84)

### Cluster 3 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1458` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1547` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 4 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan_Shader.cpp:266` — `Zenith_Vulkan_Shader::MergeReflection` (score 22.8, LOC 31)
- `Zenith\D3D12\Zenith_D3D12_Pipeline.h:106` — `MergeReflection` (score 22.5, LOC 26)

### Cluster 5 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:34` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:55` — `IsWriteAccess` (score 15.2, LOC 20)

## Include redundancy

Per-file `#include` findings: duplicates, transitive coverage, and same-basename conflicts. Each row is a concrete cleanup candidate.

### Duplicate includes (68 total)
| File | Line | Include | First seen |
| --- | ---: | --- | ---: |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 8 | `Core/Multithreading/Zenith_Multithreading.h` | 6 |
| `Zenith\AssetHandling\Zenith_TextureAsset.cpp` | 7 | `Flux/Flux_GraphicsImpl.h` | 6 |
| `Zenith\Core\Zenith_Core.cpp` | 15 | `DebugVariables/Zenith_DebugVariables.h` | 14 |
| `Zenith\Core\Zenith_Core.cpp` | 24 | `Flux/Flux_GraphicsImpl.h` | 23 |
| `Zenith\Core\Zenith_Engine.cpp` | 63 | `Flux/Flux_GraphicsImpl.h` | 30 |
| `Zenith\Core\Zenith_Engine.cpp` | 66 | `Editor/Zenith_Editor.h` | 65 |
| `Zenith\Core\Zenith_Engine.cpp` | 68 | `Editor/Zenith_EditorAutomation.h` | 67 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp` | 19 | `Flux/Flux_GraphicsImpl.h` | 18 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Viewport.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Editor\Zenith_Editor_MaterialUI.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Editor\Zenith_Editor_Menu.cpp` | 7 | `Zenith_Editor.h` | 6 |
| `Zenith\Editor\Zenith_EditorAutomation.cpp` | 9 | `Editor/Zenith_EditorAutomation.h` | 5 |
| `Zenith\Editor\Zenith_EditorCamera.cpp` | 8 | `Zenith_Editor.h` | 7 |
| `Zenith\Editor\Zenith_SelectionSystem.cpp` | 7 | `Zenith_SelectionSystem.h` | 6 |
| `Zenith\Editor\Zenith_UndoSystem.cpp` | 7 | `Zenith_UndoSystem.h` | 6 |
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

### Transitively-covered includes (167 total)
| File | Line | Include | Covered by |
| --- | ---: | --- | --- |
| `FluxCompiler\FluxCompiler.cpp` | 3 | `Core/Memory/Zenith_MemoryManagement_Disabled.h` | `Zenith\Core\Zenith.h:1` |
| `FluxCompiler\FluxCompiler.cpp` | 6 | `DataStream/Zenith_DataStream.h` | `Zenith\Flux\Slang\Flux_SlangCompiler.h:4` |
| `Tests\SentinelECS\main.cpp` | 39 | `ZenithECS/Zenith_Scene.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 40 | `ZenithECS/Zenith_Entity.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 41 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 47 | `DataStream/Zenith_DataStream.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp` | 9 | `Physics/Zenith_Physics.h` | `Zenith\EntityComponent\Components\Zenith_ColliderComponent.h:8` |
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
| `Zenith\AssetHandling\Zenith_MeshAsset.h` | 5 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:3` |
| `Zenith\Core\Memory\Zenith_MemoryManagement.cpp` | 15 | `Zenith_MemoryManagement.h` | `Zenith\Core\Zenith.h:13` |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | 6 | `Core/Zenith_Core.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_BenchECS.cpp` | 11 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:10` |
| `Zenith\Core\Zenith_Core.cpp` | 2 | `Zenith_Core.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_Core.cpp` | 3 | `Core/FrameContext.h` | `Zenith\Core\Zenith.h:1` |

### Conflicting basename includes (2 total)
| File | Line | Include | Conflicts with | Same target |
| --- | ---: | --- | --- | :---: |
| `Zenith\Editor\Zenith_Editor.cpp` | 10 | `Zenith_Editor.h` | `Editor/Zenith_Editor.h` (line 5) | yes |
| `Zenith\Editor\Zenith_Gizmo.cpp` | 16 | `Editor/Zenith_Gizmo.h` | `Zenith_Gizmo.h` (line 6) | yes |

## External includes (104 resolved to 27 middleware/third-party header(s))

These resolve into excluded directories (Middleware/, ThirdParty/, External/, …). Listed for visibility only — not flagged as issues.

| Count | Header |
| ---: | --- |
| 42 | `imgui.h` |
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
| 1 | `Core/Zenith_UnitTests.Tests.inl` |
| 1 | `UnitTests/Zenith_UnitTests.h` |
| 1 | `../../../Tools/Zenith_Tools_TextureExport.h` |
| 1 | `Editor/Zenith_Editor.Tests.inl` |
| 1 | `Editor/Zenith_EditorAutomation.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_AnimationClip.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_BlendTree.Tests.inl` |
| 1 | `Physics/Zenith_Physics.Tests.inl` |
| 1 | `backends/imgui_impl_glfw.h` |

## Tech-debt markers (13 total; showing first 30)

- `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:163` **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- `Zenith\Editor\Zenith_UndoSystem.cpp:357` **TODO** Serialize full entity state (all components)
- `Zenith\Editor\Zenith_UndoSystem.cpp:409` **TODO** Deserialize full entity state
- `Zenith\Flux\Flux.h:79` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:88` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:87` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:524` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:173` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash set
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:168` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:14` **TODO** Replace with engine hash set
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1392` **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:446` **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\AI\Navigation` | 9 | 2,859 | 82.5 | 13.4 | 39.8 | `Zenith\AI\Navigation\Zenith_NavMesh.cpp` |
| `Zenith\Flux\MeshAnimation` | 20 | 5,748 | 76.9 | 27.3 | 34.0 | `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` |
| `Zenith\Core` | 35 | 2,941 | 73.6 | 3.0 | 8.7 | `Zenith\Core\Zenith_AutomatedTest.cpp` |
| `Zenith\Telemetry` | 2 | 746 | 73.2 | 41.7 | 37.0 | `Zenith\Telemetry\Zenith_Telemetry.cpp` |
| `Zenith\UI` | 25 | 4,062 | 72.7 | 13.7 | 22.0 | `Zenith\UI\Zenith_UIText.cpp` |
| `Zenith\Vulkan` | 20 | 6,158 | 72.4 | 11.1 | 29.1 | `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` |
| `Zenith\Editor\Panels` | 20 | 3,071 | 71.5 | 12.9 | 24.1 | `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,270 | 71.4 | 67.7 | 113.0 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,706 | 70.3 | 56.6 | 29.1 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` |
| `Zenith\Flux\Terrain` | 6 | 1,594 | 70.1 | 9.8 | 30.3 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Physics` | 5 | 1,423 | 70.0 | 8.9 | 33.2 | `Zenith\Physics\Zenith_Physics.cpp` |
| `Zenith\AI\Perception` | 2 | 653 | 69.8 | 38.6 | 34.5 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,159 | 69.5 | 50.0 | 34.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |
| `Zenith\Flux\Slang` | 10 | 2,193 | 69.2 | 13.1 | 26.4 | `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` |
| `Zenith\Editor` | 17 | 5,030 | 68.8 | 11.3 | 29.4 | `Zenith\Editor\Zenith_Editor.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple. Architecture: module Abstractness (A) and Distance (D) are a regex proxy (pure-virtual presence per header) and are reported, never gated; a std::function/pimpl-free engine tends to show uniformly low A. Direction/encapsulation gates exclude only AMBIGUOUS basename-resolved edges (a basename shared by 2+ files); a unique basename is high-confidence and IS gated, like a relative-path match.
