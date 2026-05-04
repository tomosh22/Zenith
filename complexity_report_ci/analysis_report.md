# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `tree-sitter`)
- High-priority files (score >= 70): **8**
- P90 file priority: **65.7**
- Include cycles: **0**
- Duplicate clusters: **4**
- Include edges resolved: 2125 relative-path (high confidence), 20 basename-only (low confidence, 1%)
- Worst function: `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:118` `Flux_AnimatedMeshes::ExecuteGBuffer` (score 52.4)
- Worst file: `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp` (score 74.5)

## Summary

- Files: **448**, SLOC: **82,938**, Functions: **5,274**, Classes: **1,166**
- Avg Cyclomatic: **19.3** (max 302), Avg Cognitive: **24.6** (max 432)
- Priority score distribution: **8** high (>=70, 1.8%), **131** medium, **309** low. P50=11.7, P90=65.7.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:118` — `Flux_AnimatedMeshes::ExecuteGBuffer` (score: 52.4)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=29, nesting=4, LOC=97, params=2, returns=1
Score breakdown: cognitive=42%, cyclomatic=21%, nesting=19%, length=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 2. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:262` — `RenderCategoryTab` (score: 51.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 3. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1172` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 51.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=94, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 4. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:297` — `Flux_MeshGeometry::Combine` (score: 51.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 5. `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp:844` — `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` (score: 50.0)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=25, nesting=3, LOC=172, params=0, returns=1
Score breakdown: cognitive=38%, length=26%, cyclomatic=22%, nesting=15%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 6. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1589` — `Flux_RenderGraph::AssignClearFlags` (score: 49.5)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=18, cognitive=32, nesting=3, LOC=40, params=0, returns=0
Score breakdown: cognitive=48%, cyclomatic=30%, nesting=15%, length=6%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 7. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:714` — `Zenith_NavMeshGenerator::EmitQuadsFromSpans` (score: 48.6)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=9, cognitive=32, nesting=4, LOC=61, params=2, returns=1
Score breakdown: cognitive=49%, nesting=21%, cyclomatic=15%, length=9%, params=5%

Suggested approach: break up into smaller, named steps.

### 8. `Zenith\Editor\Zenith_Editor.cpp:368` — `Zenith_Editor::Update` (score: 48.1)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=25, nesting=3, LOC=114, params=0, returns=4
Score breakdown: cognitive=39%, cyclomatic=28%, length=18%, nesting=16%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 9. `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp:551` — `Zenith_TerrainComponent::LoadAndCombineLowLODChunks` (score: 47.7)
Dominant signal: **cognitive**
Tags: `long-function`
CC=12, cognitive=22, nesting=3, LOC=116, params=4, returns=1
Score breakdown: cognitive=35%, cyclomatic=21%, length=18%, nesting=16%, params=10%

Suggested approach: extract cohesive blocks into helpers.

### 10. `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp:311` — `Zenith_SceneOperationQueue::ProcessPendingAsyncLoads` (score: 47.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=30, nesting=3, LOC=91, params=0, returns=0
Score breakdown: cognitive=47%, cyclomatic=23%, nesting=16%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 11. `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp:199` — `Flux_SkeletonPose::SampleFromClip` (score: 47.3)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=11, cognitive=29, nesting=3, LOC=68, params=3, returns=0
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=16%, length=11%, params=8%

Suggested approach: break up into smaller, named steps.

### 12. `Zenith\Editor\Zenith_Editor_Menu.cpp:50` — `Zenith_Editor::RenderFileMenu` (score: 47.1)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=12, cognitive=27, nesting=4, LOC=91, params=0, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=21%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 13. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:419` — `Flux_MeshGeometry::GenerateLayoutAndVertexData` (score: 47.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=25, nesting=2, LOC=110, params=0, returns=0
Score breakdown: cognitive=40%, cyclomatic=32%, length=18%, nesting=11%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 14. `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp:388` — `Flux_TerrainStreamingManager::RequestNearbyHighLOD` (score: 46.9)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=12, cognitive=27, nesting=3, LOC=72, params=3, returns=0
Score breakdown: cognitive=43%, cyclomatic=21%, nesting=16%, length=12%, params=8%

Suggested approach: break up into smaller, named steps.

### 15. `Zenith\Physics\Zenith_Physics.cpp:584` — `Zenith_Physics::LockRotation` (score: 46.8)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=13, cognitive=27, nesting=3, LOC=43, params=4, returns=2
Score breakdown: cognitive=43%, cyclomatic=23%, nesting=16%, params=11%, length=7%

Suggested approach: break up into smaller, named steps.

### 16. `Zenith\UI\Zenith_UILayoutGroup.cpp:268` — `Zenith_UILayoutGroup::PlaceChild` (score: 46.8)
Dominant signal: **cognitive**
Tags: `complex-branching`
CC=18, cognitive=22, nesting=3, LOC=71, params=2, returns=1
Score breakdown: cognitive=35%, cyclomatic=32%, nesting=16%, length=11%, params=5%

Suggested approach: extract branches into named helpers or strategy objects.

### 17. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:871` — `Zenith_NavMeshGenerator::MergeOverlappingSpans` (score: 46.2)
Dominant signal: **cognitive**
Tags: `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=11, cognitive=26, nesting=5, LOC=51, params=1, returns=0
Score breakdown: cognitive=42%, nesting=27%, cyclomatic=20%, length=8%, params=3%

Extract candidates:
- Lines 900-914 (depth 5+ for 15 lines)

Suggested approach: extract `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:900-914` (the depth-5+ block of 15 lines) into a helper.

### 18. `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:270` — `Zenith_PerceptionSystem::UpdateSightPerception` (score: 46.1)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=26, nesting=3, LOC=93, params=1, returns=0
Score breakdown: cognitive=42%, cyclomatic=24%, nesting=16%, length=15%, params=3%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 19. `Zenith\UI\Zenith_UIButton.cpp:233` — `Zenith_UIButton::CalculateIconTextPositions` (score: 45.9)
Dominant signal: **cognitive**
Tags: `complex-branching`
CC=17, cognitive=23, nesting=3, LOC=76, params=1, returns=3
Score breakdown: cognitive=38%, cyclomatic=31%, nesting=16%, length=12%, params=3%

Suggested approach: extract branches into named helpers or strategy objects.

### 20. `Zenith\EntityComponent\Components\Zenith_ScriptComponent.cpp:499` — `Zenith_ScriptComponent::ReadFromDataStream` (score: 45.6)
Dominant signal: **cognitive**
Tags: `long-function`
CC=13, cognitive=22, nesting=3, LOC=127, params=1, returns=1
Score breakdown: cognitive=36%, cyclomatic=24%, length=21%, nesting=16%, params=3%

Suggested approach: extract cohesive blocks into helpers.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp` | `low-mi` | 74.5 | 104 | 183 | 6 | 640 | 26 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 72.3 | 117 | 222 | 5 | 779 | 28 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 68 | 136 | 4 | 543 | 13 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.4 | 302 | 432 | 4 | 1476 | 91 |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 71.2 | 96 | 149 | 5 | 704 | 30 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 59 | 96 | 4 | 588 | 24 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 82 | 130 | 4 | 470 | 36 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.1 | 153 | 264 | 4 | 819 | 38 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 63 | 106 | 4 | 499 | 28 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 69.5 | 95 | 143 | 4 | 527 | 19 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 524 | 17 |
| `Zenith\EntityComponent\Internal\Zenith_SceneRegistry.cpp` | `low-mi` | 69.4 | 102 | 148 | 4 | 445 | 30 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 450 | 35 |
| `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` | `low-mi` | 69.2 | 144 | 164 | 4 | 658 | 26 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 69.1 | 71 | 102 | 4 | 590 | 35 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi` | 68.8 | 79 | 135 | 4 | 313 | 16 |
| `Zenith\Editor\Zenith_Editor.cpp` | `low-mi`, `god-file` | 68.7 | 148 | 204 | 3 | 1247 | 52 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 68.4 | 28 | 48 | 4 | 210 | 9 |
| `Zenith\EntityComponent\Zenith_Entity.cpp` | `low-mi` | 68.3 | 45 | 69 | 4 | 301 | 28 |
| `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` | `low-mi` | 68.3 | 58 | 66 | 4 | 316 | 12 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 68.3 | 70 | 103 | 3 | 580 | 41 |
| `Zenith\UI\Zenith_UILayoutGroup.cpp` | `low-mi` | 68.3 | 71 | 114 | 3 | 393 | 24 |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | `low-mi` | 68.0 | 32 | 56 | 4 | 321 | 13 |
| `Zenith\UI\Zenith_UIButton.cpp` | `low-mi` | 68.0 | 66 | 76 | 3 | 430 | 16 |
| `Zenith\EntityComponent\Components\Zenith_ScriptComponent.cpp` | `low-mi` | 67.9 | 76 | 100 | 3 | 549 | 32 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate. **Caveat: header-inline only.** Method bodies that live in separate `.cpp` files are invisible at this scope, so this is a header-inline cohesion signal, not a full-class score.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_Vulkan` | `Zenith\Vulkan\Zenith_Vulkan.h:103` | 1.00 | 12 | 4 |
| `Zenith_Vulkan_MemoryManager` | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.h:34` | 1.00 | 12 | 31 |
| `Flux_Gizmos` | `Zenith\Flux\Gizmos\Flux_Gizmos.h:36` | 1.00 | 4 | 5 |
| `Flux_TerrainStreamingManager` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.h:183` | 1.00 | 4 | 12 |
| `Zenith_SceneOperationQueue` | `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.h:36` | 1.00 | 3 | 25 |
| `Zenith_AssetRegistry` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:81` | 0.98 | 10 | 5 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:293` | 0.98 | 6 | 35 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:196` | 0.97 | 3 | 11 |
| `Flux_RenderGraph_Pass` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:130` | 0.96 | 3 | 18 |
| `Zenith_SceneData` | `Zenith\EntityComponent\Zenith_SceneData.h:51` | 0.96 | 51 | 36 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Zenith_MeshAsset` | `Zenith\AssetHandling\Zenith_MeshAsset.h:29` | 0.95 | 15 | 25 |
| `Zenith_TerrainComponent` | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.h:56` | 0.95 | 15 | 19 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:39` | 0.95 | 20 | 20 |
| `Flux_AnimationController` | `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:40` | 0.95 | 20 | 20 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 198 | 11 |  |
| `Zenith\Flux\Flux.h` | 72 | 7 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 67 | 0 |  |
| `Zenith\DataStream\Zenith_DataStream.h` | 65 | 1 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 61 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 56 | 0 |  |
| `Zenith\EntityComponent\Zenith_Scene.h` | 48 | 4 |  |
| `Zenith\Flux\Flux_Buffers.h` | 48 | 1 |  |
| `Zenith\Flux\Flux_Graphics.h` | 47 | 2 |  |
| `Zenith\Collections\Zenith_Vector.h` | 46 | 3 |  |
| `Zenith\EntityComponent\Zenith_SceneManager.h` | 45 | 10 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_SceneData.h` | 40 | 7 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 39 | 4 |  |
| `Zenith\EntityComponent\Zenith_Entity.h` | 36 | 1 |  |

## Near-duplicate functions (4 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:185` — `Zenith_TestAssertEq` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:213` — `Zenith_TestAssertGt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:227` — `Zenith_TestAssertLt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:241` — `Zenith_TestAssertGe` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:255` — `Zenith_TestAssertLe` (score 15.8, LOC 12)

### Cluster 2 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1446` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1535` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 3 (2 functions)
- `Zenith\Core\Zenith_Tween.cpp:39` — `Zenith_GetEasingTypeName` (score 23.1, LOC 26)
- `Zenith\Flux\Slang\Flux_CodeGenerator.cpp:64` — `ResourceKindName` (score 16.8, LOC 19)

### Cluster 4 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:35` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:56` — `IsWriteAccess` (score 15.2, LOC 20)

## Tech-debt markers (28 total; showing first 30)

- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:14` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:15` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:176` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:177` **TODO** Replace with engine hash map
- `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:162` **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- `Zenith\Editor\Zenith_UndoSystem.cpp:352` **TODO** Serialize full entity state (all components)
- `Zenith\Editor\Zenith_UndoSystem.cpp:404` **TODO** Deserialize full entity state
- `Zenith\EntityComponent\Zenith_SceneData.cpp:20` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.cpp:179` **TODO** auto-promote same-scene self-unload to async.");
- `Zenith\EntityComponent\Zenith_SceneData.h:407` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.h:602` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:154` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:262` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1009` **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- `Zenith\Flux\Flux.h:105` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:114` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:89` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:543` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:178` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:293` **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:92` **TODO** Replace with engine hash map
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:309` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash set
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:154` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:13` **TODO** Replace with engine hash set
- `Zenith\Flux\Slang\Flux_SlangCompiler.h:111` **TODO** Replace with engine hash map. Map stores indices into
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1388` **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:411` **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\EntityComponent\Internal` | 11 | 2,109 | 74.5 | 7.6 | 27.6 | `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp` |
| `Zenith\AI\Navigation` | 8 | 2,417 | 72.3 | 37.3 | 36.6 | `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` |
| `Zenith\Editor\Panels` | 21 | 3,071 | 71.5 | 2.9 | 22.7 | `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,330 | 71.4 | 67.7 | 116.8 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,660 | 70.3 | 56.2 | 29.5 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` |
| `Zenith\Flux\MeshAnimation` | 18 | 5,491 | 70.2 | 27.2 | 35.8 | `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` |
| `Zenith\Flux\Terrain` | 5 | 1,464 | 70.1 | 11.7 | 36.4 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\AI\Perception` | 2 | 621 | 69.8 | 38.4 | 32.0 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,157 | 69.5 | 49.8 | 34.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |
| `Zenith\Flux\Slang` | 10 | 2,163 | 69.2 | 13.1 | 26.1 | `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` |
| `Zenith\Editor` | 15 | 4,828 | 68.7 | 45.7 | 32.3 | `Zenith\Editor\Zenith_Editor.cpp` |
| `Zenith\EntityComponent` | 24 | 4,620 | 68.3 | 19.5 | 23.1 | `Zenith\EntityComponent\Zenith_Entity.cpp` |
| `Zenith\Physics` | 5 | 1,413 | 68.3 | 8.5 | 32.8 | `Zenith\Physics\Zenith_Physics.cpp` |
| `Zenith\UI` | 25 | 4,002 | 68.3 | 13.7 | 21.3 | `Zenith\UI\Zenith_UILayoutGroup.cpp` |
| `Zenith\Profiling` | 2 | 901 | 67.7 | 39.4 | 42.0 | `Zenith\Profiling\Zenith_Profiling.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple.
