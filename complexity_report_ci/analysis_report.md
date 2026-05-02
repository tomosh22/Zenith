# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `auto`)
- High-priority files (score >= 70): **18**
- P90 file priority: **66.1**
- Include cycles: **2**
- Duplicate clusters: **12**
- Include edges resolved: 2112 relative-path (high confidence), 20 basename-only (low confidence, 1%)
- Worst function: `Zenith\Flux\Slang\Flux_CodeGenerator.cpp:170` `Flux_CodeGenerator::BuildSubsystemHeaderContent` (score 79.7)
- Worst file: `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` (score 81.7)

## Summary

- Files: **446**, SLOC: **82,771**, Functions: **5,218**, Classes: **1,161**
- Avg Cyclomatic: **19.5** (max 302), Avg Cognitive: **25.2** (max 432)
- Priority score distribution: **18** high (>=70, 4.0%), **121** medium, **307** low. P50=11.7, P90=66.1.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\Flux\Slang\Flux_CodeGenerator.cpp:170` — `Flux_CodeGenerator::BuildSubsystemHeaderContent` (score: 79.7)
Dominant signal: **cognitive**
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=23, cognitive=67, nesting=5, LOC=190, params=3, returns=1
Score breakdown: cognitive=38%, cyclomatic=24%, length=18%, nesting=16%, params=5%

Extract candidates:
- Lines 238-301 (depth 5+ for 64 lines)
- Lines 328-348 (depth 5+ for 21 lines)

Suggested approach: extract `Zenith\Flux\Slang\Flux_CodeGenerator.cpp:238-301` (the depth-5+ block of 64 lines) into a helper.

### 2. `Zenith\Flux\Slang\Flux_SlangCompiler.cpp:581` — `Flux_SlangCompiler::CompileProgram` (score: 78.4)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=38, cognitive=41, nesting=3, LOC=179, params=2, returns=16
Score breakdown: cognitive=38%, cyclomatic=32%, length=17%, nesting=10%, params=3%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 3. `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp:305` — `Flux_DynamicLights::GatherLightsFromScene` (score: 74.7)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=26, cognitive=60, nesting=4, LOC=174, params=0, returns=6
Score breakdown: cognitive=40%, cyclomatic=29%, length=18%, nesting=13%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 4. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:86` — `RenderEntityContextMenu` (score: 63.8)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=38, nesting=4, LOC=88, params=3, returns=1
Score breakdown: cognitive=45%, cyclomatic=24%, nesting=16%, length=10%, params=6%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 5. `Zenith\EntityComponent\Zenith_SceneManager.cpp:1212` — `Zenith_SceneManager::UnloadAllNonPersistent` (score: 62.3)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=35, nesting=4, LOC=131, params=1, returns=0
Score breakdown: cognitive=42%, cyclomatic=24%, nesting=16%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 6. `Zenith\Editor\Zenith_SelectionSystem.cpp:329` — `Zenith_SelectionSystem::CalculateBoundingBox` (score: 58.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=32, nesting=4, LOC=114, params=1, returns=4
Score breakdown: cognitive=41%, cyclomatic=24%, nesting=17%, length=15%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 7. `Zenith\EntityComponent\Components\Zenith_ScriptComponent.cpp:651` — `Zenith_ScriptComponent::RenderPropertiesPanel` (score: 54.9)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=21, cognitive=29, nesting=3, LOC=108, params=0, returns=1
Score breakdown: cognitive=40%, cyclomatic=32%, length=15%, nesting=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 8. `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp:1042` — `Flux_TerrainStreamingManager::BuildChunkDataForGPU_Internal` (score: 54.9)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=36, nesting=4, LOC=83, params=2, returns=0
Score breakdown: cognitive=49%, nesting=18%, cyclomatic=17%, length=11%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 9. `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp:290` — `Zenith_TerrainComponent::RenderTerrainRegenerationSection` (score: 53.9)
Dominant signal: **cognitive**
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=14, cognitive=30, nesting=5, LOC=96, params=0, returns=1
Score breakdown: cognitive=42%, nesting=23%, cyclomatic=22%, length=13%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 10. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:433` — `RenderSceneHeaderAndBody` (score: 53.6)
Dominant signal: **cognitive**
Tags: `long-function`, `many-params`
CC=13, cognitive=21, nesting=4, LOC=94, params=9, returns=1
Score breakdown: cognitive=29%, cyclomatic=20%, nesting=19%, params=19%, length=13%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 11. `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1012` — `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` (score: 53.0)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=14, cognitive=26, nesting=3, LOC=141, params=3, returns=3
Score breakdown: cognitive=37%, cyclomatic=22%, length=20%, nesting=14%, params=7%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 12. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:687` — `Zenith_NavMeshGenerator::BuildPolygonMesh` (score: 52.6)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=10, cognitive=33, nesting=4, LOC=110, params=1, returns=3
Score breakdown: cognitive=47%, nesting=19%, cyclomatic=16%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 13. `Zenith\Windows\Zenith_Windows_FileWatcher.cpp:166` — `Zenith_FileWatcher::UpdatePlatform` (score: 52.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=18, cognitive=23, nesting=4, LOC=137, params=0, returns=3
Score breakdown: cognitive=33%, cyclomatic=29%, length=20%, nesting=19%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 14. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:118` — `Flux_AnimatedMeshes::ExecuteGBuffer` (score: 52.4)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=29, nesting=4, LOC=97, params=2, returns=1
Score breakdown: cognitive=42%, cyclomatic=21%, nesting=19%, length=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 15. `Zenith\UI\Zenith_UICanvas.cpp:203` — `Zenith_UICanvas::Update` (score: 52.0)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=15, cognitive=45, nesting=2, LOC=43, params=1, returns=0
Score breakdown: cognitive=58%, cyclomatic=24%, nesting=10%, length=6%, params=2%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 16. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:262` — `RenderCategoryTab` (score: 51.7)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 17. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1165` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 51.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=94, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 18. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:297` — `Flux_MeshGeometry::Combine` (score: 51.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 19. `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp:844` — `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` (score: 50.0)
Dominant signal: **cognitive**
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=25, nesting=3, LOC=172, params=0, returns=1
Score breakdown: cognitive=38%, length=26%, cyclomatic=22%, nesting=15%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 20. `Zenith\AI\Squad\Zenith_Squad.cpp:719` — `Zenith_Squad::AssignFormationSlots` (score: 49.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=28, nesting=3, LOC=81, params=0, returns=1
Score breakdown: cognitive=42%, cyclomatic=30%, nesting=15%, length=12%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` | `low-mi` | 81.7 | 58 | 97 | 5 | 305 | 9 |
| `Zenith\Flux\Slang\Flux_SlangCompiler.cpp` | `low-mi` | 79.5 | 138 | 146 | 4 | 611 | 20 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | `low-mi` | 78.4 | 41 | 76 | 4 | 340 | 17 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` | `low-mi` | 75.7 | 55 | 107 | 6 | 409 | 12 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 75.3 | 117 | 230 | 6 | 771 | 25 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 75.1 | 95 | 149 | 4 | 502 | 16 |
| `Zenith\EntityComponent\Zenith_SceneManager.cpp` | `low-mi`, `god-file` | 74.7 | 106 | 143 | 4 | 902 | 129 |
| `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp` | `low-mi` | 74.5 | 104 | 183 | 6 | 640 | 26 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 74.2 | 153 | 296 | 5 | 817 | 36 |
| `Zenith\Editor\Zenith_SelectionSystem.cpp` | `low-mi` | 73.4 | 55 | 81 | 4 | 317 | 15 |
| `Zenith\EntityComponent\Zenith_SceneData.cpp` | `low-mi`, `god-file` | 72.3 | 138 | 250 | 5 | 746 | 46 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 68 | 136 | 4 | 543 | 13 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.4 | 302 | 432 | 4 | 1475 | 91 |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 71.2 | 96 | 149 | 5 | 704 | 30 |
| `Zenith\EntityComponent\Components\Zenith_ScriptComponent.cpp` | `low-mi` | 70.7 | 76 | 108 | 3 | 542 | 29 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 59 | 96 | 4 | 588 | 24 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 82 | 130 | 4 | 470 | 36 |
| `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` | `low-mi`, `god-file` | 70.2 | 179 | 236 | 3 | 1595 | 70 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 63 | 106 | 4 | 499 | 28 |
| `Zenith\UI\Zenith_UICanvas.cpp` | `low-mi` | 69.8 | 110 | 174 | 3 | 569 | 34 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 524 | 17 |
| `Zenith\EntityComponent\Internal\Zenith_SceneRegistry.cpp` | `low-mi` | 69.4 | 102 | 148 | 4 | 445 | 30 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 450 | 35 |
| `Zenith\AI\Squad\Zenith_Squad.cpp` | `low-mi`, `god-file` | 69.1 | 122 | 152 | 3 | 788 | 59 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 69.1 | 71 | 102 | 4 | 590 | 35 |

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
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:294` | 0.98 | 6 | 35 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:196` | 0.97 | 3 | 11 |
| `Flux_RenderGraph_Pass` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:131` | 0.96 | 3 | 18 |
| `Zenith_SceneData` | `Zenith\EntityComponent\Zenith_SceneData.h:229` | 0.96 | 49 | 36 |
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
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 66 | 0 |  |
| `Zenith\DataStream\Zenith_DataStream.h` | 65 | 1 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 60 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 56 | 0 |  |
| `Zenith\EntityComponent\Zenith_Scene.h` | 48 | 4 | **yes** |
| `Zenith\Flux\Flux_Buffers.h` | 48 | 1 |  |
| `Zenith\Flux\Flux_Graphics.h` | 47 | 2 |  |
| `Zenith\EntityComponent\Zenith_SceneManager.h` | 46 | 9 | **yes** |
| `Zenith\Collections\Zenith_Vector.h` | 45 | 3 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_SceneData.h` | 40 | 6 | **yes** |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 39 | 4 |  |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 35 | 1 |  |

## Include cycles (2 total)

Strongly-connected components in the `#include` graph. Any cycle means the files in it are mutually dependent — the clearest architectural red flag in a C++ codebase.

### Cycle 1 (2 files)
- `Zenith\EntityComponent\Internal\Zenith_SceneRegistry.h`
- `Zenith\EntityComponent\Zenith_Scene.h`

### Cycle 2 (2 files)
- `Zenith\EntityComponent\Zenith_SceneData.h`
- `Zenith\EntityComponent\Zenith_SceneManager.h`

## Near-duplicate functions (12 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:164` — `Zenith_TestAssertEq` (score 16.2, LOC 17)
- `Zenith\Core\Zenith_TestFramework.h:202` — `Zenith_TestAssertGt` (score 16.2, LOC 17)
- `Zenith\Core\Zenith_TestFramework.h:221` — `Zenith_TestAssertLt` (score 16.2, LOC 17)
- `Zenith\Core\Zenith_TestFramework.h:240` — `Zenith_TestAssertGe` (score 16.2, LOC 17)
- `Zenith\Core\Zenith_TestFramework.h:259` — `Zenith_TestAssertLe` (score 16.2, LOC 17)

### Cluster 2 (3 functions)
- `Zenith\UI\Zenith_UIButton.cpp:151` — `Zenith_UIButton::ComputeMousePosition` (score 17.1, LOC 23)
- `Zenith\UI\Zenith_UIScrollView.cpp:59` — `Zenith_UIScrollView::GetTransformedMousePosition` (score 17.1, LOC 23)
- `Zenith\UI\Zenith_UIToggle.cpp:96` — `Zenith_UIToggle::GetInteractionMousePosition` (score 17.1, LOC 23)

### Cluster 3 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1446` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1535` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 4 (2 functions)
- `Zenith\Core\Zenith_Tween.cpp:39` — `Zenith_GetEasingTypeName` (score 23.1, LOC 26)
- `Zenith\Flux\Slang\Flux_CodeGenerator.cpp:64` — `ResourceKindName` (score 16.8, LOC 19)

### Cluster 5 (2 functions)
- `Zenith\Flux\Flux_RenderTargets.cpp:267` — `Flux_RenderAttachmentBuilder::BuildColourFromAliasedVRAM` (score 22.6, LOC 58)
- `Zenith\Flux\Flux_RenderTargets.cpp:202` — `Flux_RenderAttachmentBuilder::BuildColour` (score 21.8, LOC 64)

### Cluster 6 (2 functions)
- `Zenith\AI\BehaviorTree\Zenith_BTComposites.cpp:7` — `Zenith_BTSequence::Execute` (score 21.2, LOC 40)
- `Zenith\AI\BehaviorTree\Zenith_BTComposites.cpp:50` — `Zenith_BTSelector::Execute` (score 21.2, LOC 40)

### Cluster 7 (2 functions)
- `Zenith\Flux\MeshAnimation\Flux_AnimationClip.cpp:63` — `Flux_RootMotion::SamplePositionDelta` (score 20.6, LOC 24)
- `Zenith\Flux\MeshAnimation\Flux_AnimationClip.cpp:88` — `Flux_RootMotion::SampleRotationDelta` (score 20.6, LOC 24)

### Cluster 8 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:797` — `Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer` (score 16.6, LOC 41)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:715` — `Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer` (score 15.5, LOC 27)

### Cluster 9 (2 functions)
- `Zenith\EntityComponent\Zenith_SceneData.cpp:696` — `Zenith_SceneData::DispatchOnUpdateForEntities` (score 15.5, LOC 15)
- `Zenith\EntityComponent\Zenith_SceneData.cpp:712` — `Zenith_SceneData::DispatchOnLateUpdateForEntities` (score 15.5, LOC 15)

### Cluster 10 (2 functions)
- `Zenith\AssetHandling\Zenith_AssetRegistry.cpp:183` — `Zenith_AssetRegistry::SetGameAssetsDir` (score 15.4, LOC 17)
- `Zenith\AssetHandling\Zenith_AssetRegistry.cpp:201` — `Zenith_AssetRegistry::SetEngineAssetsDir` (score 15.4, LOC 17)

### Cluster 11 (2 functions)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.cpp:318` — `Flux_BlendTreeNode_BlendSpace1D::GetNormalizedTime` (score 15.4, LOC 22)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.cpp:587` — `Flux_BlendTreeNode_BlendSpace2D::GetNormalizedTime` (score 15.4, LOC 22)

### Cluster 12 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:29` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:50` — `IsWriteAccess` (score 15.2, LOC 20)

## Tech-debt markers (33 total; showing first 30)

- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:14` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:15` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:176` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:177` **TODO** Replace with engine hash map
- `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:162` **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- `Zenith\Editor\Zenith_UndoSystem.cpp:352` **TODO** Serialize full entity state (all components)
- `Zenith\Editor\Zenith_UndoSystem.cpp:404` **TODO** Deserialize full entity state
- `Zenith\EntityComponent\Zenith_SceneData.cpp:20` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.cpp:179` **TODO** auto-promote same-scene self-unload to async.");
- `Zenith\EntityComponent\Zenith_SceneData.h:585` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.h:768` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:154` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:262` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1007` **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- `Zenith\Flux\Flux.h:105` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:114` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:89` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:543` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:178` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:293` **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:92` **TODO** Replace with engine hash map
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:309` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1786` **TODO** Replace std::unordered_map with engine hash map.
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:8` **TODO** Replace with engine hash set
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:512` **TODO** Replace std::unordered_map with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:561` **TODO** Replace std::unordered_map with engine hash map
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:154` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:13` **TODO** Replace with engine hash set
- `Zenith\Flux\Slang\Flux_SlangCompiler.h:111` **TODO** Replace with engine hash map. Map stores indices into

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\Flux\Slang` | 10 | 2,105 | 81.7 | 13.1 | 25.5 | `Zenith\Flux\Slang\Flux_CodeGenerator.cpp` |
| `Zenith\Flux\DynamicLights` | 4 | 497 | 78.4 | 10.6 | 12.5 | `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,644 | 75.7 | 56.2 | 29.5 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` |
| `Zenith\AI\Navigation` | 8 | 2,395 | 75.3 | 37.3 | 36.6 | `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` |
| `Zenith\Editor\Panels` | 21 | 3,046 | 75.1 | 2.9 | 22.7 | `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` |
| `Zenith\EntityComponent` | 22 | 4,576 | 74.7 | 19.5 | 25.3 | `Zenith\EntityComponent\Zenith_SceneManager.cpp` |
| `Zenith\EntityComponent\Internal` | 11 | 2,109 | 74.5 | 7.6 | 27.6 | `Zenith\EntityComponent\Internal\Zenith_SceneOperationQueue.cpp` |
| `Zenith\Flux\Terrain` | 5 | 1,462 | 74.2 | 11.7 | 36.4 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Editor` | 15 | 4,826 | 73.4 | 45.7 | 32.3 | `Zenith\Editor\Zenith_SelectionSystem.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,326 | 71.4 | 67.7 | 116.8 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\Flux\MeshAnimation` | 18 | 5,500 | 70.2 | 27.2 | 36.3 | `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` |
| `Zenith\Vulkan` | 20 | 5,805 | 70.2 | 10.8 | 26.4 | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` |
| `Zenith\AI\Perception` | 2 | 621 | 69.8 | 38.4 | 32.0 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\UI` | 25 | 4,038 | 69.8 | 13.7 | 21.6 | `Zenith\UI\Zenith_UICanvas.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,157 | 69.5 | 49.8 | 34.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple.
