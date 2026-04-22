# Code Complexity Report

Root: `C:\dev\Zenith`


## Summary

- Files: **396**, SLOC: **75,308**, Functions: **4,382**, Classes: **992**
- Avg Cyclomatic: **20.6** (max 261), Avg Cognitive: **26.7** (max 380)
- Priority score distribution: **13** high (>=70, 3.3%), **118** medium, **265** low. P50=14.1, P90=66.2.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp:289` — `Zenith_TerrainComponent::RenderTerrainRegenerationSection` (score: 53.9)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=14, cognitive=30, nesting=5, LOC=97, params=0, returns=1
Score breakdown: cognitive=42%, nesting=23%, cyclomatic=22%, length=14%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 2. `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp:946` — `ExecuteDynamicLights` (score: 53.9)
Tags: `long-function`, `complex-branching`
CC=16, cognitive=23, nesting=3, LOC=178, params=2, returns=3
Score breakdown: cognitive=32%, length=25%, cyclomatic=25%, nesting=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 3. `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp:230` — `Flux_StaticMeshes::RenderToShadowMap` (score: 53.9)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=12, cognitive=30, nesting=5, LOC=85, params=2, returns=0
Score breakdown: cognitive=42%, nesting=23%, cyclomatic=19%, length=12%, params=5%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 4. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:419` — `RenderSceneHeaderAndBody` (score: 53.7)
Tags: `long-function`, `many-params`
CC=13, cognitive=21, nesting=4, LOC=95, params=9, returns=1
Score breakdown: cognitive=29%, cyclomatic=20%, nesting=19%, params=19%, length=13%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 5. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:85` — `RenderEntityContextMenu` (score: 52.9)
Tags: `complex-branching`, `hard-to-follow`
CC=15, cognitive=28, nesting=4, LOC=76, params=3, returns=1
Score breakdown: cognitive=40%, cyclomatic=24%, nesting=19%, length=11%, params=7%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 6. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:628` — `Zenith_NavMeshGenerator::BuildPolygonMesh` (score: 52.7)
Tags: `long-function`, `hard-to-follow`
CC=10, cognitive=33, nesting=4, LOC=111, params=1, returns=3
Score breakdown: cognitive=47%, nesting=19%, cyclomatic=16%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 7. `Zenith\Windows\Zenith_Windows_FileWatcher.cpp:165` — `Zenith_FileWatcher::UpdatePlatform` (score: 52.6)
Tags: `long-function`, `complex-branching`
CC=18, cognitive=23, nesting=4, LOC=138, params=0, returns=3
Score breakdown: cognitive=33%, cyclomatic=28%, length=20%, nesting=19%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 8. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:104` — `Flux_AnimatedMeshes::ExecuteGBuffer` (score: 52.4)
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=29, nesting=4, LOC=98, params=2, returns=1
Score breakdown: cognitive=42%, cyclomatic=21%, nesting=19%, length=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 9. `FluxCompiler\FluxCompiler.cpp:66` — `main` (score: 52.0)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=15, cognitive=27, nesting=3, LOC=157, params=0, returns=2
Score breakdown: cognitive=39%, cyclomatic=24%, length=23%, nesting=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 10. `Zenith\UI\Zenith_UICanvas.cpp:202` — `Zenith_UICanvas::Update` (score: 52.0)
Tags: `complex-branching`, `hard-to-follow`
CC=15, cognitive=45, nesting=2, LOC=44, params=1, returns=0
Score breakdown: cognitive=58%, cyclomatic=24%, nesting=10%, length=6%, params=2%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 11. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:261` — `RenderCategoryTab` (score: 51.8)
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=95, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 12. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:296` — `Flux_MeshGeometry::Combine` (score: 51.0)
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=85, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 13. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1086` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 50.8)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=88, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=13%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 14. `Zenith\EntityComponent\Zenith_SceneManager.cpp:1797` — `Zenith_SceneManager::UnloadAllNonPersistent` (score: 50.4)
Tags: `long-function`, `hard-to-follow`
CC=14, cognitive=26, nesting=4, LOC=106, params=1, returns=0
Score breakdown: cognitive=39%, cyclomatic=23%, nesting=20%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 15. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1484` — `Flux_RenderGraph::AssignClearFlags` (score: 49.7)
Tags: `complex-branching`, `hard-to-follow`
CC=18, cognitive=32, nesting=3, LOC=43, params=0, returns=0
Score breakdown: cognitive=48%, cyclomatic=30%, nesting=15%, length=6%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 16. `Zenith\AI\Squad\Zenith_Squad.cpp:718` — `Zenith_Squad::AssignFormationSlots` (score: 49.6)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=28, nesting=3, LOC=82, params=0, returns=1
Score breakdown: cognitive=42%, cyclomatic=30%, nesting=15%, length=12%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 17. `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:936` — `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` (score: 49.5)
Tags: `long-function`
CC=13, cognitive=23, nesting=3, LOC=136, params=3, returns=3
Score breakdown: cognitive=35%, cyclomatic=22%, length=21%, nesting=15%, params=8%

Suggested approach: extract cohesive blocks into helpers.

### 18. `Zenith\EntityComponent\Zenith_SceneData.cpp:824` — `Zenith_SceneData::DispatchAwakeForNewScene` (score: 48.5)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=11, cognitive=26, nesting=5, LOC=98, params=0, returns=0
Score breakdown: cognitive=40%, nesting=26%, cyclomatic=19%, length=15%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 19. `Zenith\Editor\Zenith_Editor.cpp:376` — `Zenith_Editor::Update` (score: 48.2)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=25, nesting=3, LOC=115, params=0, returns=4
Score breakdown: cognitive=39%, cyclomatic=28%, length=18%, nesting=16%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 20. `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp:198` — `Flux_SkeletonPose::SampleFromClip` (score: 47.3)
Tags: `hard-to-follow`
CC=11, cognitive=29, nesting=3, LOC=69, params=3, returns=0
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=16%, length=11%, params=8%

Suggested approach: break up into smaller, named steps.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` | `low-mi` | 75.7 | 54 | 107 | 6 | 407 | 12 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 75.3 | 117 | 230 | 6 | 768 | 25 |
| `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` | `low-mi` | 74.5 | 28 | 70 | 6 | 239 | 9 |
| `Zenith\EntityComponent\Zenith_SceneData.cpp` | `low-mi`, `god-file` | 72.3 | 138 | 250 | 5 | 746 | 45 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | `low-mi` | 72.2 | 72 | 115 | 4 | 781 | 25 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 72.1 | 92 | 139 | 4 | 493 | 16 |
| `Zenith\EntityComponent\Zenith_SceneManager_AsyncLoad.cpp` | `low-mi` | 71.9 | 69 | 124 | 6 | 449 | 17 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 68 | 136 | 4 | 543 | 14 |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 71.2 | 96 | 149 | 5 | 701 | 30 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.2 | 261 | 380 | 4 | 1381 | 73 |
| `Zenith\EntityComponent\Zenith_SceneManager.cpp` | `low-mi`, `god-file` | 71.1 | 210 | 269 | 4 | 1214 | 69 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.6 | 101 | 197 | 5 | 582 | 25 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 82 | 130 | 4 | 470 | 33 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 63 | 106 | 4 | 496 | 27 |
| `Zenith\UI\Zenith_UICanvas.cpp` | `low-mi` | 69.8 | 110 | 174 | 3 | 569 | 34 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 524 | 16 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 450 | 34 |
| `Zenith\AI\Squad\Zenith_Squad.cpp` | `low-mi`, `god-file` | 69.1 | 122 | 152 | 3 | 785 | 56 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 69.1 | 71 | 102 | 4 | 590 | 33 |
| `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` | `low-mi`, `god-file` | 69.1 | 172 | 211 | 3 | 1505 | 68 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi` | 68.8 | 73 | 128 | 4 | 297 | 15 |
| `Zenith\Editor\Zenith_Editor.cpp` | `low-mi`, `god-file` | 68.7 | 150 | 207 | 3 | 1264 | 52 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 68.4 | 28 | 48 | 4 | 210 | 9 |
| `Zenith\EntityComponent\Zenith_Entity.cpp` | `low-mi` | 68.3 | 45 | 69 | 4 | 301 | 26 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi` | 68.3 | 70 | 103 | 3 | 578 | 40 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_Vulkan_MemoryManager` | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.h:38` | 1.00 | 10 | 25 |
| `Flux_TerrainStreamingManager` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.h:91` | 1.00 | 6 | 11 |
| `Flux_Gizmos` | `Zenith\Flux\Gizmos\Flux_Gizmos.h:36` | 1.00 | 4 | 5 |
| `Zenith_Vulkan` | `Zenith\Vulkan\Zenith_Vulkan.h:103` | 1.00 | 3 | 4 |
| `Zenith_SceneManager` | `Zenith\EntityComponent\Zenith_SceneManager.h:134` | 0.99 | 11 | 26 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:281` | 0.98 | 3 | 33 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:194` | 0.97 | 3 | 11 |
| `Zenith_SceneData` | `Zenith\EntityComponent\Zenith_SceneData.h:229` | 0.96 | 48 | 36 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 49 | 29 |
| `Zenith_TerrainComponent` | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.h:55` | 0.95 | 10 | 17 |
| `Flux_AnimationController` | `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:40` | 0.95 | 14 | 20 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:39` | 0.95 | 12 | 20 |
| `Zenith_Window` | `Zenith\Android\Zenith_Android_Window.h:9` | 0.95 | 10 | 8 |
| `Zenith_UILayoutGroup` | `Zenith\UI\Zenith_UILayoutGroup.h:26` | 0.95 | 14 | 18 |
| `Zenith_Vulkan_CommandBuffer` | `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.h:57` | 0.95 | 5 | 15 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 190 | 10 |  |
| `Zenith\Flux\Flux.h` | 71 | 6 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 62 | 0 |  |
| `Zenith\DataStream\Zenith_DataStream.h` | 60 | 1 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 57 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 54 | 0 |  |
| `Zenith\Flux\Flux_Graphics.h` | 46 | 2 |  |
| `Zenith\EntityComponent\Zenith_SceneManager.h` | 44 | 4 | **yes** |
| `Zenith\Flux\Flux_Buffers.h` | 44 | 1 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_Scene.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_SceneData.h` | 39 | 6 | **yes** |
| `Zenith\Collections\Zenith_Vector.h` | 37 | 3 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 37 | 2 |  |
| `Zenith\EntityComponent\Components\Zenith_TransformComponent.h` | 34 | 7 |  |

## Include cycles (1 total)

Strongly-connected components in the `#include` graph. Any cycle means the files in it are mutually dependent — the clearest architectural red flag in a C++ codebase.

### Cycle 1 (2 files)
- `Zenith\EntityComponent\Zenith_SceneData.h`
- `Zenith\EntityComponent\Zenith_SceneManager.h`

## Near-duplicate functions (188 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (40 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:313` — `Zenith_EditorAutomation::AddStep_SetUIColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:389` — `Zenith_EditorAutomation::AddStep_SetUILayoutPadding` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:440` — `Zenith_EditorAutomation::AddStep_SetUIToggleOnColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:452` — `Zenith_EditorAutomation::AddStep_SetUIToggleOffColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:474` — `Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:532` — `Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:544` — `Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:556` — `Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:607` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:628` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:651` — `Zenith_EditorAutomation::AddStep_SetUIGradientColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:663` — `Zenith_EditorAutomation::AddStep_SetUIShadow` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:675` — `Zenith_EditorAutomation::AddStep_SetUIShadowColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:687` — `Zenith_EditorAutomation::AddStep_SetUIRectBorder` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:712` — `Zenith_EditorAutomation::AddStep_SetUITextShadowColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:735` — `Zenith_EditorAutomation::AddStep_SetUIButtonShadow` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:747` — `Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:759` — `Zenith_EditorAutomation::AddStep_SetUIButtonGradientColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:771` — `Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:812` — `Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor` (score 8.0, LOC 12)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:701` — `Zenith_EditorAutomation::AddStep_SetUITextShadow` (score 6.7, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:801` — `Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow` (score 6.7, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:284` — `Zenith_EditorAutomation::AddStep_SetUIPosition` (score 5.3, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:294` — `Zenith_EditorAutomation::AddStep_SetUISize` (score 5.3, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:486` — `Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize` (score 5.3, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:520` — `Zenith_EditorAutomation::AddStep_SetUIScrollViewContentSize` (score 5.3, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:586` — `Zenith_EditorAutomation::AddStep_SetUIButtonIconSize` (score 5.3, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:275` — `Zenith_EditorAutomation::AddStep_SetUIAnchor` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:304` — `Zenith_EditorAutomation::AddStep_SetUIFontSize` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:325` — `Zenith_EditorAutomation::AddStep_SetUIAlignment` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:362` — `Zenith_EditorAutomation::AddStep_SetUILayoutDirection` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:371` — `Zenith_EditorAutomation::AddStep_SetUILayoutSpacing` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:380` — `Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:568` — `Zenith_EditorAutomation::AddStep_SetUIButtonFontSize` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:596` — `Zenith_EditorAutomation::AddStep_SetUIButtonIconPlacement` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:619` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:642` — `Zenith_EditorAutomation::AddStep_SetUICornerRadius` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:726` — `Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:783` — `Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:792` — `Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration` (score 4.0, LOC 9)

### Cluster 2 (19 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:232` — `Zenith_EditorAutomation::AddStep_CreateUIText` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:241` — `Zenith_EditorAutomation::AddStep_CreateUIButton` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:266` — `Zenith_EditorAutomation::AddStep_SetUIImageTexturePath` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:353` — `Zenith_EditorAutomation::AddStep_AddUIChild` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:431` — `Zenith_EditorAutomation::AddStep_CreateUIToggle` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:577` — `Zenith_EditorAutomation::AddStep_SetUIButtonIcon` (score 4.0, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:82` — `Zenith_EditorAutomation::AddStep_CreateScene` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:90` — `Zenith_EditorAutomation::AddStep_SaveScene` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:107` — `Zenith_EditorAutomation::AddStep_CreateEntity` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:115` — `Zenith_EditorAutomation::AddStep_SelectEntity` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:133` — `Zenith_EditorAutomation::AddStep_AddComponent` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:250` — `Zenith_EditorAutomation::AddStep_CreateUIRect` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:258` — `Zenith_EditorAutomation::AddStep_CreateUIImage` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:345` — `Zenith_EditorAutomation::AddStep_CreateUILayoutGroup` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:466` — `Zenith_EditorAutomation::AddStep_CreateUIOverlay` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:512` — `Zenith_EditorAutomation::AddStep_CreateUIScrollView` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:826` — `Zenith_EditorAutomation::AddStep_SetBehaviour` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:834` — `Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:852` — `Zenith_EditorAutomation::AddStep_SetParticleConfigByName` (score 2.7, LOC 8)

### Cluster 3 (11 functions)
- `Zenith\UI\Zenith_UIButton.h:84` — `SetTransitionDuration` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIElement.h:205` — `SetBackgroundBorderColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIElement.h:206` — `SetBackgroundBorderThickness` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIImage.h:61` — `SetShadowColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIImage.h:63` — `SetShadowSpread` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:54` — `SetBorderColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:55` — `SetBorderThickness` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:56` — `SetGradientColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:57` — `SetShadowEnabled` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:58` — `SetShadowColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIRect.h:60` — `SetShadowSpread` (score 2.2, LOC 2)

### Cluster 4 (10 functions)
- `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp:1124` — `Flux_DynamicLights::SetupRenderGraph` (score 3.1, LOC 14)
- `Zenith\Flux\Vegetation\Flux_Grass.cpp:179` — `Flux_Grass::SetupRenderGraph` (score 3.1, LOC 13)
- `Zenith\Flux\Skybox\Flux_Skybox.cpp:345` — `Flux_Skybox::SetupAerialPerspectiveRenderGraph` (score 3.0, LOC 12)
- `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:95` — `Flux_AnimatedMeshes::SetupRenderGraph` (score 2.8, LOC 9)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:580` — `Flux_Primitives::SetupRenderGraph` (score 2.8, LOC 9)
- `Zenith\Flux\SDFs\Flux_SDFs.cpp:116` — `Flux_SDFs::SetupRenderGraph` (score 2.8, LOC 9)
- `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp:92` — `Flux_StaticMeshes::SetupRenderGraph` (score 2.8, LOC 9)
- `Zenith\Flux\Gizmos\Flux_Gizmos.cpp:331` — `Flux_Gizmos::SetupRenderGraph` (score 2.5, LOC 6)
- `Zenith\Flux\Quads\Flux_Quads.cpp:109` — `Flux_Quads::SetupRenderGraph` (score 2.5, LOC 6)
- `Zenith\Flux\Text\Flux_Text.cpp:281` — `Flux_Text::SetupRenderGraph` (score 2.5, LOC 6)

### Cluster 5 (8 functions)
- `Zenith\AI\BehaviorTree\Zenith_BTNode.h:68` — `SetNodeName` (score 2.2, LOC 2)
- `Zenith\AI\Squad\Zenith_Squad.h:133` — `SetName` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:98` — `SetPositionOffset` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:104` — `SetDirectionOffset` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationLayer.h:37` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:167` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:280` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.h:233` — `SetParameter` (score 2.2, LOC 2)

### Cluster 6 (6 functions)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:660` — `Flux_Primitives::AddTriangle` (score 6.8, LOC 13)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:636` — `Flux_Primitives::AddCapsule` (score 6.7, LOC 12)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:648` — `Flux_Primitives::AddCylinder` (score 6.7, LOC 12)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:600` — `Flux_Primitives::AddCube` (score 5.5, LOC 12)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:612` — `Flux_Primitives::AddWireframeCube` (score 5.5, LOC 12)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:589` — `Flux_Primitives::AddSphere` (score 5.4, LOC 11)

### Cluster 7 (6 functions)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:57` — `Flux_AnimationParameters::AddFloat` (score 4.0, LOC 9)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:66` — `Flux_AnimationParameters::AddInt` (score 4.0, LOC 9)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:75` — `Flux_AnimationParameters::AddBool` (score 4.0, LOC 9)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:6` — `Zenith_Blackboard::SetFloat` (score 3.9, LOC 8)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:14` — `Zenith_Blackboard::SetInt` (score 3.9, LOC 8)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:22` — `Zenith_Blackboard::SetBool` (score 3.9, LOC 8)

### Cluster 8 (6 functions)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:404` — `onNativeWindowCreated` (score 3.7, LOC 5)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:409` — `onNativeWindowDestroyed` (score 3.7, LOC 5)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:414` — `onNativeWindowRedrawNeeded` (score 3.7, LOC 5)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:419` — `onNativeWindowResized` (score 3.7, LOC 5)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:424` — `onInputQueueCreated` (score 3.7, LOC 5)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:429` — `onInputQueueDestroyed` (score 3.7, LOC 5)

### Cluster 9 (6 functions)
- `Zenith\Flux\Flux_RenderTargets.cpp:149` — `Flux_RenderAttachmentCube::SRV` (score 2.8, LOC 10)
- `Zenith\Flux\Flux_RenderTargets.cpp:113` — `Flux_RenderAttachment::SRV` (score 2.5, LOC 6)
- `Zenith\Flux\Flux_RenderTargets.cpp:125` — `Flux_RenderAttachment::UAV` (score 2.5, LOC 6)
- `Zenith\Flux\Flux_RenderTargets.cpp:131` — `Flux_RenderAttachment::RTV` (score 2.5, LOC 6)
- `Zenith\Flux\Flux_RenderTargets.cpp:165` — `Flux_RenderAttachmentCube::UAV` (score 2.5, LOC 6)
- `Zenith\Flux\Flux_RenderTargets.cpp:176` — `Flux_RenderAttachmentCube::RTV` (score 2.5, LOC 6)

### Cluster 10 (6 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:153` — `Zenith_EditorAutomation::AddStep_SetCameraPitch` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:161` — `Zenith_EditorAutomation::AddStep_SetCameraYaw` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:169` — `Zenith_EditorAutomation::AddStep_SetCameraFOV` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:177` — `Zenith_EditorAutomation::AddStep_SetCameraNear` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:185` — `Zenith_EditorAutomation::AddStep_SetCameraFar` (score 2.7, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:193` — `Zenith_EditorAutomation::AddStep_SetCameraAspect` (score 2.7, LOC 8)

### Cluster 11 (6 functions)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:53` — `Zenith_UIComponent::CreateRect` (score 2.6, LOC 7)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:60` — `Zenith_UIComponent::CreateImage` (score 2.6, LOC 7)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:74` — `Zenith_UIComponent::CreateLayoutGroup` (score 2.6, LOC 7)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:88` — `Zenith_UIComponent::CreateOverlay` (score 2.6, LOC 7)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:95` — `Zenith_UIComponent::CreateScrollView` (score 2.6, LOC 7)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:102` — `Zenith_UIComponent::CreateElement` (score 2.6, LOC 7)

### Cluster 12 (6 functions)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:151` — `Zenith_SceneManager::UnregisterActiveSceneChangedCallback` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:154` — `Zenith_SceneManager::UnregisterSceneLoadedCallback` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:157` — `Zenith_SceneManager::UnregisterSceneUnloadingCallback` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:160` — `Zenith_SceneManager::UnregisterSceneUnloadedCallback` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:163` — `Zenith_SceneManager::UnregisterSceneLoadStartedCallback` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Zenith_SceneManager_Callbacks.cpp:166` — `Zenith_SceneManager::UnregisterEntityPersistentCallback` (score 2.2, LOC 2)

### Cluster 13 (5 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1422` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.9, LOC 50)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1511` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.8, LOC 24)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1483` — `Zenith_Vulkan::ConvertToVkLoadAction` (score 9.0, LOC 15)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1498` — `Zenith_Vulkan::ConvertToVkStoreAction` (score 8.0, LOC 13)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1472` — `Zenith_Vulkan::ConvertToVkFormat_DepthStencil` (score 7.0, LOC 11)

### Cluster 14 (5 functions)
- `Zenith\Flux\Fog\Flux_Fog.cpp:150` — `Flux_Fog::ExecuteFroxelInject` (score 9.8, LOC 10)
- `Zenith\Flux\Fog\Flux_Fog.cpp:160` — `Flux_Fog::ExecuteFroxelLight` (score 9.8, LOC 10)
- `Zenith\Flux\Fog\Flux_Fog.cpp:170` — `Flux_Fog::ExecuteFroxelApply` (score 9.8, LOC 10)
- `Zenith\Flux\Fog\Flux_Fog.cpp:180` — `Flux_Fog::ExecuteRaymarch` (score 9.8, LOC 10)
- `Zenith\Flux\Fog\Flux_Fog.cpp:190` — `Flux_Fog::ExecuteGodRays` (score 9.8, LOC 10)

### Cluster 15 (5 functions)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:289` — `Zenith_MaterialAsset::GetDiffuseTexture` (score 6.5, LOC 10)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:299` — `Zenith_MaterialAsset::GetNormalTexture` (score 6.5, LOC 10)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:309` — `Zenith_MaterialAsset::GetRoughnessMetallicTexture` (score 6.5, LOC 10)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:319` — `Zenith_MaterialAsset::GetOcclusionTexture` (score 6.5, LOC 10)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:329` — `Zenith_MaterialAsset::GetEmissiveTexture` (score 6.5, LOC 10)

### Cluster 16 (5 functions)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:153` — `AddVector2` (score 6.3, LOC 6)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:158` — `AddVector3` (score 6.3, LOC 6)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:163` — `AddUVector4` (score 6.3, LOC 6)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:168` — `AddVector4` (score 6.3, LOC 6)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:173` — `AddVector3` (score 6.3, LOC 6)

### Cluster 17 (5 functions)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1428` — `Zenith_Vulkan_MemoryManager::DestroyVertexBuffer` (score 2.6, LOC 7)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1445` — `Zenith_Vulkan_MemoryManager::DestroyIndexBuffer` (score 2.6, LOC 7)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1452` — `Zenith_Vulkan_MemoryManager::DestroyConstantBuffer` (score 2.6, LOC 7)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1469` — `Zenith_Vulkan_MemoryManager::DestroyIndirectBuffer` (score 2.6, LOC 7)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1476` — `Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer` (score 2.6, LOC 7)

### Cluster 18 (5 functions)
- `Zenith\AssetHandling\Zenith_MaterialAsset.h:75` — `SetName` (score 2.2, LOC 2)
- `Zenith\AssetHandling\Zenith_MaterialAsset.h:79` — `SetBaseColor` (score 2.2, LOC 2)
- `Zenith\AssetHandling\Zenith_MaterialAsset.h:90` — `SetEmissiveColor` (score 2.2, LOC 2)
- `Zenith\AssetHandling\Zenith_MaterialAsset.h:104` — `SetUVTiling` (score 2.2, LOC 2)
- `Zenith\AssetHandling\Zenith_MaterialAsset.h:107` — `SetUVOffset` (score 2.2, LOC 2)

### Cluster 19 (5 functions)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:80` — `SetIntensity` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:83` — `SetRange` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationLayer.h:41` — `SetWeight` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.h:136` — `SetBlendWeight` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.h:286` — `SetAdditiveWeight` (score 2.2, LOC 2)

### Cluster 20 (5 functions)
- `Zenith\UI\Zenith_UIButton.h:77` — `SetBorderColor` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIButton.h:78` — `SetBorderThickness` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIButton.h:80` — `SetShadowOffset` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIButton.h:81` — `SetShadowSpread` (score 2.2, LOC 2)
- `Zenith\UI\Zenith_UIButton.h:82` — `SetShadowColor` (score 2.2, LOC 2)

## Tech-debt markers (44 total; showing first 30)

- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:14` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp:15` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:176` **TODO** Replace with engine hash map
- `Zenith\AI\Perception\Zenith_PerceptionSystem.h:177` **TODO** Replace with engine hash map
- `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:162` **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- `Zenith\Editor\Zenith_UndoSystem.cpp:352` **TODO** Serialize full entity state (all components)
- `Zenith\Editor\Zenith_UndoSystem.cpp:404` **TODO** Deserialize full entity state
- `Zenith\EntityComponent\Components\Zenith_ModelComponent.cpp:454` **TODO** Flux_MeshInstance needs to provide access to geometry or position data
- `Zenith\EntityComponent\Zenith_EventSystem.h:232` **TODO** Replace std::unordered_map with engine hash map when available
- `Zenith\EntityComponent\Zenith_SceneData.cpp:20` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.cpp:179` **TODO** auto-promote same-scene self-unload to async.");
- `Zenith\EntityComponent\Zenith_SceneData.h:581` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.h:764` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:162` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:256` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1318` **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1519` **TODO** Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime is affected by Time.timeScale)
- `Zenith\EntityComponent\Zenith_SceneManager.h:913` **TODO** Replace with engine hash map when available
- `Zenith\EntityComponent\Zenith_SceneManager.h:1031` **TODO** Replace with engine hash map
- `Zenith\Flux\Flux.h:104` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:113` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:88` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:546` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:178` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:293` **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:92` **TODO** Replace with engine hash map
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:309` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1687` **TODO** Replace std::unordered_map with engine hash map.
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:5` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:6` **TODO** Replace with engine hash set

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\EntityComponent\Components` | 26 | 6,043 | 75.7 | 53.0 | 27.3 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` |
| `Zenith\AI\Navigation` | 8 | 2,383 | 75.3 | 37.1 | 36.6 | `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` |
| `Zenith\Flux\StaticMeshes` | 2 | 253 | 74.5 | 37.9 | 14.5 | `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` |
| `Zenith\EntityComponent` | 23 | 5,840 | 72.3 | 41.5 | 35.3 | `Zenith\EntityComponent\Zenith_SceneData.cpp` |
| `Zenith\Flux\DynamicLights` | 2 | 797 | 72.2 | 37.0 | 36.5 | `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` |
| `Zenith\Editor\Panels` | 19 | 2,806 | 72.1 | 2.9 | 23.2 | `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,204 | 71.2 | 67.7 | 104.5 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\Flux\Terrain` | 5 | 1,062 | 70.6 | 10.9 | 23.8 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Flux\MeshAnimation` | 18 | 5,500 | 70.2 | 27.2 | 36.3 | `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` |
| `Zenith\AI\Perception` | 2 | 618 | 69.8 | 38.4 | 32.0 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\UI` | 25 | 4,038 | 69.8 | 13.7 | 21.6 | `Zenith\UI\Zenith_UICanvas.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,091 | 69.5 | 48.6 | 32.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |
| `Zenith\AI\Squad` | 6 | 2,070 | 69.1 | 25.6 | 45.7 | `Zenith\AI\Squad\Zenith_Squad.cpp` |
| `Zenith\Vulkan` | 19 | 5,704 | 69.1 | 10.7 | 27.2 | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` |
| `Zenith\Editor` | 15 | 5,148 | 68.7 | 45.7 | 31.9 | `Zenith\Editor\Zenith_Editor.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple.
