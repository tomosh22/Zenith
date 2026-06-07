# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `tree-sitter`)
- High-priority files (score >= 70): **12**
- P90 file priority: **65.9**
- Include cycles: **0**
- Duplicate clusters: **5**
- Include edges resolved: 2085 relative-path + 0 unique-basename (high confidence, gated); 2 ambiguous-basename (low confidence, suppressed from gates); 2 basename total (0%)
- External includes (middleware/third-party): 104
- Include redundancy: score **219** (76 duplicate, 141 transitive, 0 unresolved, 2 conflicting)
- Worst function: `Zenith\AI\Navigation\Zenith_NavMesh.cpp:782` `Zenith_NavMesh::GetRandomReachablePointInRadius` (score 82.5)
- Worst file: `Zenith\AI\Navigation\Zenith_NavMesh.cpp` (score 82.5)

## Architecture

All gated architecture/lint findings are allow-listed (ratchet green).

Report-only signals (not gated by default):
- max-module-cycles: 1 — e.g. `module-cycle: AssetHandling|Collections|Core|DataStream|DebugVariables|Editor|EntityComponent|FileAccess|Flux|Input|Maths|Physics|Prefab|Profiling|TaskSystem|UI|Vulkan|Windows|ZenithECS`
- max-zone-of-pain-modules: 2 — e.g. `zone-of-pain: Maths`

- Modules: **24**, unmapped files: **4**, module-level cycles (report-only): **1**, hub files: **0**

| Module | Layer | Files | Ca | Ce | Instability | Abstractness | Distance | Zone |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Android | 0 | 13 | 1 | 5 | 0.83 | 0.00 | 0.17 | main-sequence |
| Collections | 0 | 5 | 13 | 2 | 0.13 | 0.00 | 0.87 | pain |
| Core | 0 | 38 | 23 | 7 | 0.23 | 0.00 | 0.77 | - |
| DataStream | 0 | 4 | 7 | 2 | 0.22 | 0.00 | 0.78 | - |
| DebugVariables | 0 | 2 | 8 | 2 | 0.20 | 1.00 | 0.20 | main-sequence |
| FileAccess | 0 | 2 | 6 | 1 | 0.14 | n/a | 0.00 | - |
| Maths | 0 | 4 | 11 | 1 | 0.08 | 0.00 | 0.92 | pain |
| Profiling | 0 | 2 | 7 | 4 | 0.36 | 0.00 | 0.64 | - |
| Windows | 0 | 12 | 3 | 5 | 0.62 | 0.00 | 0.38 | - |
| Input | 1 | 7 | 7 | 3 | 0.30 | 0.00 | 0.70 | - |
| SaveData | 1 | 2 | 0 | 3 | 1.00 | 0.00 | 0.00 | main-sequence |
| TaskSystem | 1 | 2 | 4 | 4 | 0.50 | 0.00 | 0.50 | - |
| Telemetry | 1 | 2 | 0 | 5 | 1.00 | 0.00 | 0.00 | main-sequence |
| ZenithECS | 1 | 28 | 9 | 3 | 0.25 | 0.08 | 0.67 | - |
| AI | 2 | 35 | 0 | 10 | 1.00 | 0.05 | 0.05 | main-sequence |
| AssetHandling | 2 | 26 | 7 | 7 | 0.50 | 0.00 | 0.50 | - |
| EntityComponent | 2 | 31 | 7 | 10 | 0.59 | 0.08 | 0.34 | - |
| Flux | 2 | 170 | 10 | 13 | 0.56 | 0.01 | 0.43 | - |
| Physics | 2 | 5 | 4 | 8 | 0.67 | 0.00 | 0.33 | - |
| Prefab | 2 | 2 | 2 | 6 | 0.75 | 0.00 | 0.25 | - |
| Vulkan | 2 | 20 | 5 | 6 | 0.55 | 0.00 | 0.46 | - |
| UI | 3 | 25 | 4 | 7 | 0.64 | 0.00 | 0.36 | - |
| Editor | 4 | 36 | 4 | 15 | 0.79 | 0.07 | 0.14 | main-sequence |
| EngineComposition | 5 | 5 | 0 | 13 | 1.00 | 0.00 | 0.00 | main-sequence |

_A/D are a regex proxy and report-only. module-cycles and zone-of-pain are reported but not gated by default; module-scope statics, forbidden tokens, missing PCH/`#pragma once`, leaf `g_xEngine`, layering, leaf and encapsulation findings ARE gated (threshold 0, ratcheted via allowlists)._

## Summary

- Files: **482**, SLOC: **86,726**, Functions: **5,381**, Classes: **1,297**
- Avg Cyclomatic: **18.7** (max 291), Avg Cognitive: **23.8** (max 406)
- Priority score distribution: **12** high (>=70, 2.5%), **133** medium, **337** low. P50=11.1, P90=65.9.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\AI\Navigation\Zenith_NavMesh.cpp:782` — `Zenith_NavMesh::GetRandomReachablePointInRadius` (score: 82.5)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=30, cognitive=53, nesting=3, LOC=226, params=4, returns=10
Score breakdown: cognitive=36%, cyclomatic=30%, length=18%, nesting=9%, params=6%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; check whether returns can be consolidated via result-holding variable.

### 2. `Zenith\Core\Zenith_AutomatedTest.cpp:388` — `Zenith_AutomatedTestRunner::Tick` (score: 70.2)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`, `big-switch`
CC=52, cognitive=37, nesting=1, LOC=288, params=0, returns=18
Score breakdown: cognitive=40%, cyclomatic=36%, length=21%, nesting=4%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

### 3. `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp:287` — `Flux_IKSolver::SolveChain` (score: 67.9)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=39, nesting=4, LOC=115, params=4, returns=1
Score breakdown: cognitive=43%, cyclomatic=22%, nesting=15%, length=13%, params=7%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 4. `Zenith\UI\Zenith_UIText.cpp:200` — `Zenith_UIText::Render` (score: 61.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=22, cognitive=37, nesting=2, LOC=124, params=1, returns=2
Score breakdown: cognitive=45%, cyclomatic=30%, length=15%, nesting=8%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 5. `Zenith\Core\Zenith_AutomatedTest.cpp:189` — `Zenith_AutomatedTestRunner::ParseCommandLine` (score: 59.3)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=39, nesting=2, LOC=89, params=2, returns=1
Score breakdown: cognitive=49%, cyclomatic=27%, length=11%, nesting=8%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 6. `Zenith\Telemetry\Zenith_Telemetry.cpp:459` — `Reader::ExportJson` (score: 57.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=30, nesting=3, LOC=156, params=2, returns=2
Score breakdown: cognitive=39%, cyclomatic=23%, length=20%, nesting=13%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 7. `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp:108` — `Zenith_NavMeshAgent::Update` (score: 52.1)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=16, cognitive=23, nesting=3, LOC=137, params=3, returns=3
Score breakdown: cognitive=33%, cyclomatic=26%, length=20%, nesting=14%, params=7%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 8. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:262` — `RenderCategoryTab` (score: 51.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 9. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1170` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 51.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=94, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 10. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:294` — `Flux_MeshGeometry::Combine` (score: 51.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 11. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1587` — `Flux_RenderGraph::AssignClearFlags` (score: 49.5)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=18, cognitive=32, nesting=3, LOC=40, params=0, returns=0
Score breakdown: cognitive=48%, cyclomatic=30%, nesting=15%, length=6%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 12. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:845` — `Zenith_NavMeshGenerator::EmitQuadsFromSpans` (score: 48.6)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=9, cognitive=32, nesting=4, LOC=62, params=2, returns=1
Score breakdown: cognitive=49%, nesting=21%, cyclomatic=15%, length=10%, params=5%

Suggested approach: break up into smaller, named steps.

### 13. `Zenith\Editor\Zenith_Editor.cpp:334` — `Zenith_Editor::Update` (score: 48.1)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=25, nesting=3, LOC=114, params=0, returns=4
Score breakdown: cognitive=39%, cyclomatic=28%, length=18%, nesting=16%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 14. `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp:516` — `Zenith_TerrainComponent::LoadAndCombineLowLODChunks` (score: 47.7)
Dominant signal: **cognitive**
Tags: `long-function`
CC=12, cognitive=22, nesting=3, LOC=116, params=4, returns=1
Score breakdown: cognitive=35%, cyclomatic=21%, length=18%, nesting=16%, params=10%

Suggested approach: extract cohesive blocks into helpers.

### 15. `Zenith\Core\Zenith_Engine.cpp:310` — `Zenith_Engine::Initialise` (score: 47.5)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`
CC=15, cognitive=20, nesting=2, LOC=384, params=0, returns=1
Score breakdown: cognitive=32%, length=32%, cyclomatic=26%, nesting=10%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; extract branches into named helpers or strategy objects.

### 16. `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp:196` — `Flux_SkeletonPose::SampleFromClip` (score: 47.3)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=11, cognitive=29, nesting=3, LOC=68, params=3, returns=0
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=16%, length=11%, params=8%

Suggested approach: break up into smaller, named steps.

### 17. `Zenith\Editor\Zenith_Editor_Menu.cpp:52` — `Zenith_Editor::RenderFileMenu` (score: 47.1)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=12, cognitive=27, nesting=4, LOC=91, params=0, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=21%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 18. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:416` — `Flux_MeshGeometry::GenerateLayoutAndVertexData` (score: 47.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=25, nesting=2, LOC=110, params=0, returns=0
Score breakdown: cognitive=40%, cyclomatic=32%, length=18%, nesting=11%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 19. `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp:386` — `Flux_TerrainStreamingManagerImpl::RequestNearbyHighLOD` (score: 46.9)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=12, cognitive=27, nesting=3, LOC=72, params=3, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=16%, length=12%, params=8%

Suggested approach: break up into smaller, named steps.

### 20. `Zenith\Editor\Zenith_EditorAutomation.cpp:508` — `Zenith_EditorAutomation::ExecuteAction` (score: 46.8)
Dominant signal: **cyclomatic**
Tags: `very-long-function`, `complex-branching`, `big-switch`
CC=102, cognitive=4, nesting=1, LOC=1093, params=1, returns=0
Score breakdown: cyclomatic=54%, length=32%, cognitive=6%, nesting=5%, params=3%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 82.5 | 139 | 235 | 5 | 935 | 37 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 76.4 | 85 | 123 | 4 | 636 | 35 |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | `low-mi` | 73.6 | 113 | 124 | 2 | 476 | 14 |
| `Zenith\Telemetry\Zenith_Telemetry.cpp` | `low-mi` | 73.2 | 72 | 143 | 4 | 600 | 26 |
| `Zenith\UI\Zenith_UIText.cpp` | `low-mi` | 72.7 | 56 | 87 | 3 | 309 | 12 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 72.3 | 116 | 215 | 5 | 849 | 28 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 68 | 136 | 4 | 543 | 13 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.4 | 291 | 406 | 4 | 1433 | 89 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 68 | 109 | 4 | 657 | 34 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 87 | 138 | 4 | 487 | 37 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.1 | 154 | 264 | 4 | 820 | 38 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 70.0 | 80 | 124 | 4 | 620 | 44 |
| `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp` | `low-mi` | 69.9 | 45 | 70 | 3 | 283 | 11 |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | `low-mi` | 69.9 | 44 | 74 | 4 | 373 | 15 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 68 | 113 | 4 | 521 | 29 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 69.5 | 96 | 146 | 4 | 534 | 19 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 521 | 17 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 450 | 35 |
| `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` | `low-mi` | 69.2 | 144 | 164 | 4 | 656 | 26 |
| `Zenith\EntityComponent\Components\Zenith_ColliderComponent.cpp` | `low-mi` | 68.8 | 91 | 107 | 4 | 727 | 26 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi` | 68.8 | 76 | 128 | 4 | 303 | 16 |
| `Zenith\Editor\Zenith_Editor.cpp` | `low-mi`, `god-file` | 68.7 | 150 | 208 | 3 | 1235 | 64 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 68.4 | 28 | 48 | 4 | 210 | 9 |
| `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` | `low-mi` | 68.3 | 58 | 66 | 4 | 314 | 12 |
| `Zenith\UI\Zenith_UILayoutGroup.cpp` | `low-mi` | 68.3 | 71 | 114 | 3 | 393 | 24 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate. **Caveat: header-inline only.** Method bodies that live in separate `.cpp` files are invisible at this scope, so this is a header-inline cohesion signal, not a full-class score.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_EditorAutomation` | `Zenith\Editor\Zenith_EditorAutomation.h:215` | 1.00 | 6 | 4 |
| `Flux_TerrainStreamingManagerImpl` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManagerImpl.h:195` | 0.98 | 4 | 15 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:230` | 0.98 | 3 | 19 |
| `Zenith_AssetRegistry` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:54` | 0.98 | 10 | 5 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:293` | 0.97 | 6 | 33 |
| `Zenith_SceneSystem` | `Zenith\ZenithECS\Zenith_SceneSystem.h:82` | 0.97 | 11 | 31 |
| `Flux_RenderGraph_Pass` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:130` | 0.96 | 3 | 18 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Flux_GizmosImpl` | `Zenith\Flux\Gizmos\Flux_GizmosImpl.h:56` | 0.95 | 4 | 22 |
| `Flux_TerrainImpl` | `Zenith\Flux\Terrain\Flux_TerrainImpl.h:13` | 0.95 | 3 | 22 |
| `Zenith_MeshAsset` | `Zenith\AssetHandling\Zenith_MeshAsset.h:28` | 0.95 | 15 | 25 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:38` | 0.95 | 20 | 20 |
| `Flux_AnimationController` | `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:37` | 0.95 | 20 | 20 |
| `Flux_SkyboxImpl` | `Zenith\Flux\Skybox\Flux_SkyboxImpl.h:71` | 0.95 | 10 | 20 |
| `Flux_IBLImpl` | `Zenith\Flux\IBL\Flux_IBLImpl.h:42` | 0.95 | 3 | 20 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 215 | 13 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 61 | 0 |  |
| `Zenith\Flux\Flux.h` | 60 | 4 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 58 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 57 | 0 |  |
| `Zenith\Flux\Flux_GraphicsImpl.h` | 49 | 4 |  |
| `Zenith\Collections\Zenith_Vector.h` | 47 | 3 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 42 | 3 |  |
| `Zenith\Core\Zenith_Engine.h` | 41 | 0 |  |
| `Zenith\ZenithECS\Zenith_SceneSystem.h` | 41 | 6 |  |
| `Zenith\ZenithECS\Zenith_Entity.h` | 34 | 1 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 33 | 4 |  |
| `Zenith\Flux\Flux_Buffers.h` | 32 | 1 |  |
| `Zenith\Flux\Slang\Flux_SlangCompiler.h` | 32 | 2 |  |
| `Zenith\ZenithECS\Zenith_Scene.h` | 31 | 0 |  |

## Near-duplicate functions (5 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:185` — `Zenith_TestAssertEq` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:213` — `Zenith_TestAssertGt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:227` — `Zenith_TestAssertLt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:241` — `Zenith_TestAssertGe` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:255` — `Zenith_TestAssertLe` (score 15.8, LOC 12)

### Cluster 2 (2 functions)
- `Zenith\Editor\Zenith_Editor.cpp:581` — `Zenith_Editor::HandlePendingSceneLoad` (score 39.5, LOC 116)
- `Zenith\Editor\Zenith_Editor.cpp:1240` — `Zenith_Editor::HandlePendingSceneLoadDeferred` (score 37.1, LOC 84)

### Cluster 3 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1443` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1532` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 4 (2 functions)
- `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:287` — `Flux_AnimatedMeshesImpl::RenderToShadowMap` (score 21.3, LOC 46)
- `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp:273` — `Flux_StaticMeshesImpl::RenderToShadowMap` (score 17.8, LOC 33)

### Cluster 5 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:33` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:54` — `IsWriteAccess` (score 15.2, LOC 20)

## Include redundancy

Per-file `#include` findings: duplicates, transitive coverage, and same-basename conflicts. Each row is a concrete cleanup candidate.

### Duplicate includes (76 total)
| File | Line | Include | First seen |
| --- | ---: | --- | ---: |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 7 | `Core/Multithreading/Zenith_Multithreading.h` | 5 |
| `Zenith\AssetHandling\Zenith_TextureAsset.cpp` | 6 | `Flux/Flux_GraphicsImpl.h` | 5 |
| `Zenith\Core\Zenith_Core.cpp` | 15 | `DebugVariables/Zenith_DebugVariables.h` | 14 |
| `Zenith\Core\Zenith_Core.cpp` | 24 | `Flux/Flux_GraphicsImpl.h` | 23 |
| `Zenith\Core\Zenith_Engine.cpp` | 65 | `Flux/Flux_GraphicsImpl.h` | 30 |
| `Zenith\Core\Zenith_Engine.cpp` | 68 | `Editor/Zenith_Editor.h` | 67 |
| `Zenith\Core\Zenith_Engine.cpp` | 70 | `Editor/Zenith_EditorAutomation.h` | 69 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp` | 18 | `Flux/Flux_GraphicsImpl.h` | 17 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Viewport.cpp` | 7 | `Flux/Flux_GraphicsImpl.h` | 6 |
| `Zenith\Editor\Zenith_Editor_MaterialUI.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Editor\Zenith_Editor_Menu.cpp` | 6 | `Zenith_Editor.h` | 5 |
| `Zenith\Editor\Zenith_EditorAutomation.cpp` | 8 | `Editor/Zenith_EditorAutomation.h` | 4 |
| `Zenith\Editor\Zenith_EditorCamera.cpp` | 7 | `Zenith_Editor.h` | 6 |
| `Zenith\Editor\Zenith_SelectionSystem.cpp` | 6 | `Zenith_SelectionSystem.h` | 5 |
| `Zenith\Editor\Zenith_UndoSystem.cpp` | 6 | `Zenith_UndoSystem.h` | 5 |
| `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Flux\Decals\Flux_Decals.cpp` | 5 | `Flux/Decals/Flux_DecalsImpl.h` | 4 |
| `Zenith\Flux\Decals\Flux_Decals.cpp` | 7 | `Flux/Flux_GraphicsImpl.h` | 6 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 10 | `Flux/Flux_GraphicsImpl.h` | 9 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 13 | `Flux/IBL/Flux_IBLImpl.h` | 12 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | 5 | `Flux/DynamicLights/Flux_DynamicLightsImpl.h` | 4 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | 7 | `Flux/Flux_GraphicsImpl.h` | 6 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 5 | `Flux/DynamicLights/Flux_LightClusteringImpl.h` | 4 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Flux\Flux_Graphics.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |

### Transitively-covered includes (141 total)
| File | Line | Include | Covered by |
| --- | ---: | --- | --- |
| `FluxCompiler\FluxCompiler.cpp` | 3 | `Core/Memory/Zenith_MemoryManagement_Disabled.h` | `Zenith\Core\Zenith.h:1` |
| `FluxCompiler\FluxCompiler.cpp` | 6 | `DataStream/Zenith_DataStream.h` | `Zenith\Flux\Slang\Flux_SlangCompiler.h:4` |
| `Tests\SentinelECS\main.cpp` | 39 | `ZenithECS/Zenith_Scene.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 40 | `ZenithECS/Zenith_Entity.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 41 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 47 | `DataStream/Zenith_DataStream.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AI\Navigation\Zenith_NavMeshAgent.cpp` | 8 | `Physics/Zenith_Physics.h` | `Zenith\EntityComponent\Components\Zenith_ColliderComponent.h:7` |
| `Zenith\Android\Callstack\Zenith_Android_Callstack.cpp` | 3 | `Zenith_Android_Callstack.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 3 | `Zenith_Android_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Zenith_Android_Window.cpp` | 2 | `Zenith_Android_Window.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 5 | `Core/Multithreading/Zenith_Multithreading.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 6 | `Core/Zenith_Result.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 3 | `AssetHandling/Zenith_AssetRegistry.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 5 | `DataStream/Zenith_DataStream.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 6 | `Core/Zenith_Engine.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AssetHandling\Zenith_FontAsset.h` | 6 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetHandle.h:4` |
| `Zenith\AssetHandling\Zenith_MeshAsset.h` | 5 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:3` |
| `Zenith\Core\Memory\Zenith_MemoryManagement.cpp` | 14 | `Zenith_MemoryManagement.h` | `Zenith\Core\Zenith.h:13` |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | 5 | `Core/Zenith_Core.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_BenchECS.cpp` | 5 | `Core/Zenith_Engine.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_BenchECS.cpp` | 11 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:10` |
| `Zenith\Core\Zenith_Core.cpp` | 2 | `Zenith_Core.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_Core.cpp` | 3 | `Core/FrameContext.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Core\Zenith_Core.cpp` | 5 | `Core/Zenith_Engine.h` | `Zenith\Core\Zenith.h:1` |

### Conflicting basename includes (2 total)
| File | Line | Include | Conflicts with | Same target |
| --- | ---: | --- | --- | :---: |
| `Zenith\Editor\Zenith_Editor.cpp` | 9 | `Zenith_Editor.h` | `Editor/Zenith_Editor.h` (line 4) | yes |
| `Zenith\Editor\Zenith_Gizmo.cpp` | 15 | `Editor/Zenith_Gizmo.h` | `Zenith_Gizmo.h` (line 5) | yes |

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

## Tech-debt markers (24 total; showing first 30)

- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:13` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:14` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:200` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:201` **TODO** Replace with engine hash map
- `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:162` **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- `Zenith\Editor\Zenith_UndoSystem.cpp:356` **TODO** Serialize full entity state (all components)
- `Zenith\Editor\Zenith_UndoSystem.cpp:408` **TODO** Deserialize full entity state
- `Zenith\Flux\Flux.h:79` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:88` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:88` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:517` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:172` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:295` **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:90` **TODO** Replace with engine hash map
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:307` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash set
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:152` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:13` **TODO** Replace with engine hash set
- `Zenith\Flux\Slang\Flux_SlangCompiler.h:111` **TODO** Replace with engine hash map. Map stores indices into
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1377` **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:411` **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that
- `Zenith\ZenithECS\Internal\Zenith_SceneData_Serialization.cpp:127` **TODO** Replace with engine hash map
- `Zenith\ZenithECS\Internal\Zenith_SceneData_Serialization.cpp:313` **TODO** Replace with engine hash map
- `Zenith\ZenithECS\Zenith_SceneData.h:607` **TODO** Replace with engine hash map

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\AI\Navigation` | 9 | 2,854 | 82.5 | 13.4 | 39.8 | `Zenith\AI\Navigation\Zenith_NavMesh.cpp` |
| `Zenith\Flux\MeshAnimation` | 20 | 5,706 | 76.4 | 27.2 | 34.1 | `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` |
| `Zenith\Core` | 29 | 2,760 | 73.6 | 5.7 | 10.0 | `Zenith\Core\Zenith_AutomatedTest.cpp` |
| `Zenith\Telemetry` | 2 | 746 | 73.2 | 41.7 | 37.0 | `Zenith\Telemetry\Zenith_Telemetry.cpp` |
| `Zenith\UI` | 25 | 4,050 | 72.7 | 13.7 | 22.0 | `Zenith\UI\Zenith_UIText.cpp` |
| `Zenith\Editor\Panels` | 20 | 3,049 | 71.5 | 12.9 | 23.9 | `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,270 | 71.4 | 67.7 | 113.0 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,620 | 70.3 | 56.2 | 28.8 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` |
| `Zenith\Flux\Terrain` | 6 | 1,515 | 70.1 | 9.6 | 30.7 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Physics` | 5 | 1,457 | 70.0 | 8.9 | 34.8 | `Zenith\Physics\Zenith_Physics.cpp` |
| `Zenith\AI\Perception` | 2 | 651 | 69.8 | 38.6 | 34.5 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,151 | 69.5 | 49.8 | 34.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |
| `Zenith\Flux\Slang` | 10 | 2,199 | 69.2 | 13.1 | 26.1 | `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` |
| `Zenith\Editor` | 16 | 5,002 | 68.7 | 29.2 | 31.2 | `Zenith\Editor\Zenith_Editor.cpp` |
| `Zenith\ZenithECS\Internal` | 21 | 3,547 | 68.3 | 16.0 | 26.0 | `Zenith\ZenithECS\Internal\Zenith_Entity.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple. Architecture: module Abstractness (A) and Distance (D) are a regex proxy (pure-virtual presence per header) and are reported, never gated; a std::function/pimpl-free engine tends to show uniformly low A. Direction/encapsulation gates exclude only AMBIGUOUS basename-resolved edges (a basename shared by 2+ files); a unique basename is high-confidence and IS gated, like a relative-path match.
