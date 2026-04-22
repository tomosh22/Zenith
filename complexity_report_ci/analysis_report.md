# Code Complexity Report

Root: `C:\dev\Zenith`


## Summary

- Files: **399**, SLOC: **76,030**, Functions: **4,927**, Classes: **997**
- Avg Cyclomatic: **20.6** (max 261), Avg Cognitive: **26.8** (max 380)
- Priority score distribution: **13** high (>=70, 3.3%), **120** medium, **266** low. P50=14.1, P90=66.2.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\EntityComponent\Zenith_SceneManager.cpp:1811` — `Zenith_SceneManager::UnloadAllNonPersistent` (score: 62.4)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=35, nesting=4, LOC=132, params=1, returns=0
Score breakdown: cognitive=42%, cyclomatic=24%, nesting=16%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 2. `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp:290` — `Zenith_TerrainComponent::RenderTerrainRegenerationSection` (score: 53.9)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=14, cognitive=30, nesting=5, LOC=96, params=0, returns=1
Score breakdown: cognitive=42%, nesting=23%, cyclomatic=22%, length=13%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 3. `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp:947` — `ExecuteDynamicLights` (score: 53.9)
Tags: `long-function`, `complex-branching`
CC=16, cognitive=23, nesting=3, LOC=177, params=2, returns=3
Score breakdown: cognitive=32%, cyclomatic=25%, length=25%, nesting=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 4. `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp:231` — `Flux_StaticMeshes::RenderToShadowMap` (score: 53.8)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=12, cognitive=30, nesting=5, LOC=84, params=2, returns=0
Score breakdown: cognitive=42%, nesting=23%, cyclomatic=19%, length=12%, params=5%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 5. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:420` — `RenderSceneHeaderAndBody` (score: 53.6)
Tags: `long-function`, `many-params`
CC=13, cognitive=21, nesting=4, LOC=94, params=9, returns=1
Score breakdown: cognitive=29%, cyclomatic=20%, nesting=19%, params=19%, length=13%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 6. `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp:86` — `RenderEntityContextMenu` (score: 52.9)
Tags: `complex-branching`, `hard-to-follow`
CC=15, cognitive=28, nesting=4, LOC=75, params=3, returns=1
Score breakdown: cognitive=40%, cyclomatic=24%, nesting=19%, length=11%, params=7%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 7. `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp:629` — `Zenith_NavMeshGenerator::BuildPolygonMesh` (score: 52.6)
Tags: `long-function`, `hard-to-follow`
CC=10, cognitive=33, nesting=4, LOC=110, params=1, returns=3
Score breakdown: cognitive=47%, nesting=19%, cyclomatic=16%, length=16%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 8. `Zenith\Windows\Zenith_Windows_FileWatcher.cpp:166` — `Zenith_FileWatcher::UpdatePlatform` (score: 52.5)
Tags: `long-function`, `complex-branching`
CC=18, cognitive=23, nesting=4, LOC=137, params=0, returns=3
Score breakdown: cognitive=33%, cyclomatic=29%, length=20%, nesting=19%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

### 9. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:105` — `Flux_AnimatedMeshes::ExecuteGBuffer` (score: 52.4)
Tags: `long-function`, `hard-to-follow`
CC=13, cognitive=29, nesting=4, LOC=97, params=2, returns=1
Score breakdown: cognitive=42%, cyclomatic=21%, nesting=19%, length=14%, params=5%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 10. `FluxCompiler\FluxCompiler.cpp:67` — `main` (score: 52.0)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=15, cognitive=27, nesting=3, LOC=156, params=0, returns=2
Score breakdown: cognitive=39%, cyclomatic=24%, length=22%, nesting=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 11. `Zenith\UI\Zenith_UICanvas.cpp:203` — `Zenith_UICanvas::Update` (score: 52.0)
Tags: `complex-branching`, `hard-to-follow`
CC=15, cognitive=45, nesting=2, LOC=43, params=1, returns=0
Score breakdown: cognitive=58%, cyclomatic=24%, nesting=10%, length=6%, params=2%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 12. `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp:262` — `RenderCategoryTab` (score: 51.7)
Tags: `long-function`, `hard-to-follow`
CC=11, cognitive=34, nesting=4, LOC=94, params=0, returns=0
Score breakdown: cognitive=49%, nesting=19%, cyclomatic=18%, length=14%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 13. `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp:297` — `Flux_MeshGeometry::Combine` (score: 51.0)
Tags: `long-function`, `complex-branching`, `many-returns`
CC=20, cognitive=24, nesting=3, LOC=84, params=2, returns=8
Score breakdown: cognitive=35%, cyclomatic=33%, nesting=15%, length=12%, params=5%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 14. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1087` — `Flux_RenderGraph::SynthesizeAliasingBarriers` (score: 50.7)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=30, nesting=3, LOC=87, params=0, returns=4
Score breakdown: cognitive=44%, cyclomatic=28%, nesting=15%, length=13%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 15. `Zenith\AI\Squad\Zenith_Squad.cpp:719` — `Zenith_Squad::AssignFormationSlots` (score: 49.6)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=18, cognitive=28, nesting=3, LOC=81, params=0, returns=1
Score breakdown: cognitive=42%, cyclomatic=30%, nesting=15%, length=12%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 16. `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1485` — `Flux_RenderGraph::AssignClearFlags` (score: 49.6)
Tags: `complex-branching`, `hard-to-follow`
CC=18, cognitive=32, nesting=3, LOC=42, params=0, returns=0
Score breakdown: cognitive=48%, cyclomatic=30%, nesting=15%, length=6%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 17. `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:934` — `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` (score: 49.5)
Tags: `long-function`
CC=13, cognitive=23, nesting=3, LOC=135, params=3, returns=3
Score breakdown: cognitive=35%, cyclomatic=22%, length=20%, nesting=15%, params=8%

Suggested approach: extract cohesive blocks into helpers.

### 18. `Zenith\EntityComponent\Zenith_SceneData.cpp:825` — `Zenith_SceneData::DispatchAwakeForNewScene` (score: 48.4)
Tags: `long-function`, `deep-nesting`, `hard-to-follow`, `nesting-hell`
CC=11, cognitive=26, nesting=5, LOC=97, params=0, returns=0
Score breakdown: cognitive=40%, nesting=26%, cyclomatic=19%, length=15%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 19. `Zenith\Editor\Zenith_Editor.cpp:377` — `Zenith_Editor::Update` (score: 48.1)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=25, nesting=3, LOC=114, params=0, returns=4
Score breakdown: cognitive=39%, cyclomatic=28%, length=18%, nesting=16%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 20. `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp:199` — `Flux_SkeletonPose::SampleFromClip` (score: 47.3)
Tags: `hard-to-follow`
CC=11, cognitive=29, nesting=3, LOC=68, params=3, returns=0
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=16%, length=11%, params=8%

Suggested approach: break up into smaller, named steps.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` | `low-mi` | 75.7 | 54 | 107 | 6 | 407 | 12 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 75.3 | 117 | 230 | 6 | 768 | 25 |
| `Zenith\EntityComponent\Zenith_SceneManager.cpp` | `low-mi`, `god-file` | 74.7 | 214 | 278 | 4 | 1239 | 69 |
| `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` | `low-mi` | 74.4 | 28 | 70 | 6 | 239 | 9 |
| `Zenith\EntityComponent\Zenith_SceneManager_AsyncLoad.cpp` | `low-mi` | 73.3 | 70 | 127 | 6 | 464 | 17 |
| `Zenith\EntityComponent\Zenith_SceneData.cpp` | `low-mi`, `god-file` | 72.3 | 138 | 250 | 5 | 746 | 46 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | `low-mi` | 72.2 | 72 | 115 | 4 | 781 | 25 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 72.1 | 92 | 139 | 4 | 493 | 16 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 68 | 136 | 4 | 543 | 13 |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 71.2 | 96 | 149 | 5 | 701 | 30 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` | `low-mi`, `god-file` | 71.2 | 261 | 380 | 4 | 1381 | 88 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.6 | 101 | 197 | 5 | 582 | 25 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 82 | 130 | 4 | 470 | 36 |
| `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` | `low-mi` | 69.8 | 63 | 106 | 4 | 496 | 28 |
| `Zenith\UI\Zenith_UICanvas.cpp` | `low-mi` | 69.8 | 110 | 174 | 3 | 569 | 34 |
| `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` | `low-mi` | 69.5 | 58 | 76 | 3 | 524 | 17 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 69.2 | 64 | 80 | 4 | 450 | 35 |
| `Zenith\AI\Squad\Zenith_Squad.cpp` | `low-mi`, `god-file` | 69.1 | 122 | 152 | 3 | 785 | 59 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 69.1 | 71 | 102 | 4 | 590 | 35 |
| `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` | `low-mi`, `god-file` | 69.1 | 172 | 211 | 3 | 1501 | 68 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi` | 68.8 | 73 | 128 | 4 | 297 | 16 |
| `Zenith\Editor\Zenith_Editor.cpp` | `low-mi`, `god-file` | 68.7 | 150 | 207 | 3 | 1264 | 52 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 68.4 | 28 | 48 | 4 | 210 | 9 |
| `Zenith\EntityComponent\Zenith_Entity.cpp` | `low-mi` | 68.3 | 45 | 69 | 4 | 301 | 28 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 68.3 | 70 | 103 | 3 | 578 | 41 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_Vulkan` | `Zenith\Vulkan\Zenith_Vulkan.h:103` | 1.00 | 12 | 4 |
| `Zenith_Vulkan_MemoryManager` | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.h:34` | 1.00 | 12 | 25 |
| `Flux_TerrainStreamingManager` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.h:91` | 1.00 | 8 | 11 |
| `Flux_Gizmos` | `Zenith\Flux\Gizmos\Flux_Gizmos.h:36` | 1.00 | 4 | 5 |
| `Zenith_SceneManager` | `Zenith\EntityComponent\Zenith_SceneManager.h:134` | 0.99 | 9 | 31 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:282` | 0.97 | 6 | 33 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:194` | 0.97 | 3 | 11 |
| `Flux_RenderGraph_Pass` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:119` | 0.96 | 3 | 18 |
| `Zenith_SceneData` | `Zenith\EntityComponent\Zenith_SceneData.h:229` | 0.96 | 49 | 36 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Zenith_MeshAsset` | `Zenith\AssetHandling\Zenith_MeshAsset.h:29` | 0.95 | 15 | 25 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:39` | 0.95 | 20 | 20 |
| `Flux_AnimationController` | `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:40` | 0.95 | 20 | 20 |
| `Zenith_UILayoutGroup` | `Zenith\UI\Zenith_UILayoutGroup.h:26` | 0.95 | 14 | 18 |
| `Zenith_UIButton` | `Zenith\UI\Zenith_UIButton.h:31` | 0.95 | 43 | 32 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 190 | 10 |  |
| `Zenith\Flux\Flux.h` | 71 | 6 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 63 | 0 |  |
| `Zenith\DataStream\Zenith_DataStream.h` | 61 | 1 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 58 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 54 | 0 |  |
| `Zenith\Flux\Flux_Graphics.h` | 46 | 2 |  |
| `Zenith\EntityComponent\Zenith_SceneManager.h` | 44 | 4 | **yes** |
| `Zenith\Flux\Flux_Buffers.h` | 44 | 1 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_Scene.h` | 41 | 3 |  |
| `Zenith\EntityComponent\Zenith_SceneData.h` | 39 | 6 | **yes** |
| `Zenith\Collections\Zenith_Vector.h` | 37 | 3 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 37 | 3 |  |
| `Zenith\EntityComponent\Components\Zenith_TransformComponent.h` | 34 | 7 |  |

## Include cycles (1 total)

Strongly-connected components in the `#include` graph. Any cycle means the files in it are mutually dependent — the clearest architectural red flag in a C++ codebase.

### Cycle 1 (2 files)
- `Zenith\EntityComponent\Zenith_SceneData.h`
- `Zenith\EntityComponent\Zenith_SceneManager.h`

## Near-duplicate functions (192 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (40 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:314` — `Zenith_EditorAutomation::AddStep_SetUIColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:390` — `Zenith_EditorAutomation::AddStep_SetUILayoutPadding` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:441` — `Zenith_EditorAutomation::AddStep_SetUIToggleOnColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:453` — `Zenith_EditorAutomation::AddStep_SetUIToggleOffColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:475` — `Zenith_EditorAutomation::AddStep_SetUIOverlayDimColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:533` — `Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:545` — `Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:557` — `Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:608` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:629` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:652` — `Zenith_EditorAutomation::AddStep_SetUIGradientColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:664` — `Zenith_EditorAutomation::AddStep_SetUIShadow` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:676` — `Zenith_EditorAutomation::AddStep_SetUIShadowColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:688` — `Zenith_EditorAutomation::AddStep_SetUIRectBorder` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:713` — `Zenith_EditorAutomation::AddStep_SetUITextShadowColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:736` — `Zenith_EditorAutomation::AddStep_SetUIButtonShadow` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:748` — `Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:760` — `Zenith_EditorAutomation::AddStep_SetUIButtonGradientColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:772` — `Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:813` — `Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor` (score 7.9, LOC 11)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:702` — `Zenith_EditorAutomation::AddStep_SetUITextShadow` (score 6.6, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:802` — `Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow` (score 6.6, LOC 10)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:285` — `Zenith_EditorAutomation::AddStep_SetUIPosition` (score 5.3, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:295` — `Zenith_EditorAutomation::AddStep_SetUISize` (score 5.3, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:487` — `Zenith_EditorAutomation::AddStep_SetUIOverlayContentSize` (score 5.3, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:521` — `Zenith_EditorAutomation::AddStep_SetUIScrollViewContentSize` (score 5.3, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:587` — `Zenith_EditorAutomation::AddStep_SetUIButtonIconSize` (score 5.3, LOC 9)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:276` — `Zenith_EditorAutomation::AddStep_SetUIAnchor` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:305` — `Zenith_EditorAutomation::AddStep_SetUIFontSize` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:326` — `Zenith_EditorAutomation::AddStep_SetUIAlignment` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:363` — `Zenith_EditorAutomation::AddStep_SetUILayoutDirection` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:372` — `Zenith_EditorAutomation::AddStep_SetUILayoutSpacing` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:381` — `Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:569` — `Zenith_EditorAutomation::AddStep_SetUIButtonFontSize` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:597` — `Zenith_EditorAutomation::AddStep_SetUIButtonIconPlacement` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:620` — `Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:643` — `Zenith_EditorAutomation::AddStep_SetUICornerRadius` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:727` — `Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:784` — `Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:793` — `Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration` (score 3.9, LOC 8)

### Cluster 2 (19 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:233` — `Zenith_EditorAutomation::AddStep_CreateUIText` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:242` — `Zenith_EditorAutomation::AddStep_CreateUIButton` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:267` — `Zenith_EditorAutomation::AddStep_SetUIImageTexturePath` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:354` — `Zenith_EditorAutomation::AddStep_AddUIChild` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:432` — `Zenith_EditorAutomation::AddStep_CreateUIToggle` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:578` — `Zenith_EditorAutomation::AddStep_SetUIButtonIcon` (score 3.9, LOC 8)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:83` — `Zenith_EditorAutomation::AddStep_CreateScene` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:91` — `Zenith_EditorAutomation::AddStep_SaveScene` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:108` — `Zenith_EditorAutomation::AddStep_CreateEntity` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:116` — `Zenith_EditorAutomation::AddStep_SelectEntity` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:134` — `Zenith_EditorAutomation::AddStep_AddComponent` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:251` — `Zenith_EditorAutomation::AddStep_CreateUIRect` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:259` — `Zenith_EditorAutomation::AddStep_CreateUIImage` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:346` — `Zenith_EditorAutomation::AddStep_CreateUILayoutGroup` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:467` — `Zenith_EditorAutomation::AddStep_CreateUIOverlay` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:513` — `Zenith_EditorAutomation::AddStep_CreateUIScrollView` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:827` — `Zenith_EditorAutomation::AddStep_SetBehaviour` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:835` — `Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:853` — `Zenith_EditorAutomation::AddStep_SetParticleConfigByName` (score 2.6, LOC 7)

### Cluster 3 (10 functions)
- `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp:1125` — `Flux_DynamicLights::SetupRenderGraph` (score 3.1, LOC 13)
- `Zenith\Flux\Vegetation\Flux_Grass.cpp:180` — `Flux_Grass::SetupRenderGraph` (score 3.0, LOC 12)
- `Zenith\Flux\Skybox\Flux_Skybox.cpp:346` — `Flux_Skybox::SetupAerialPerspectiveRenderGraph` (score 2.9, LOC 11)
- `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:96` — `Flux_AnimatedMeshes::SetupRenderGraph` (score 2.7, LOC 8)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:581` — `Flux_Primitives::SetupRenderGraph` (score 2.7, LOC 8)
- `Zenith\Flux\SDFs\Flux_SDFs.cpp:117` — `Flux_SDFs::SetupRenderGraph` (score 2.7, LOC 8)
- `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp:93` — `Flux_StaticMeshes::SetupRenderGraph` (score 2.7, LOC 8)
- `Zenith\Flux\Gizmos\Flux_Gizmos.cpp:332` — `Flux_Gizmos::SetupRenderGraph` (score 2.5, LOC 5)
- `Zenith\Flux\Quads\Flux_Quads.cpp:110` — `Flux_Quads::SetupRenderGraph` (score 2.5, LOC 5)
- `Zenith\Flux\Text\Flux_Text.cpp:282` — `Flux_Text::SetupRenderGraph` (score 2.5, LOC 5)

### Cluster 4 (8 functions)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:19` — `Zenith_AssetHandle<Zenith_TextureAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:73` — `Zenith_AssetHandle<Zenith_MaterialAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:97` — `Zenith_AssetHandle<Zenith_MeshAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:121` — `Zenith_AssetHandle<Zenith_SkeletonAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:145` — `Zenith_AssetHandle<Zenith_ModelAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:246` — `Zenith_AssetHandle<Zenith_Prefab>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:292` — `Zenith_AssetHandle<Zenith_AnimationAsset>::Get` (score 9.7, LOC 22)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:338` — `Zenith_AssetHandle<Zenith_MeshGeometryAsset>::Get` (score 9.7, LOC 22)

### Cluster 5 (8 functions)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:53` — `Zenith_AssetHandle<Zenith_TextureAsset>::ReadFromDataStream` (score 7.1, LOC 12)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:176` — `Zenith_AssetHandle<Zenith_MaterialAsset>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:194` — `Zenith_AssetHandle<Zenith_MeshAsset>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:212` — `Zenith_AssetHandle<Zenith_SkeletonAsset>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:230` — `Zenith_AssetHandle<Zenith_ModelAsset>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:276` — `Zenith_AssetHandle<Zenith_Prefab>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:322` — `Zenith_AssetHandle<Zenith_AnimationAsset>::ReadFromDataStream` (score 6.9, LOC 10)
- `Zenith\AssetHandling\Zenith_AssetHandle.cpp:368` — `Zenith_AssetHandle<Zenith_MeshGeometryAsset>::ReadFromDataStream` (score 6.9, LOC 10)

### Cluster 6 (6 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:379` — `Flux_PassBuilder::Reads` (score 12.0, LOC 7)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:387` — `Flux_PassBuilder::Writes` (score 12.0, LOC 7)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:365` — `Flux_PassBuilder::Reads` (score 9.4, LOC 6)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:372` — `Flux_PassBuilder::Writes` (score 9.4, LOC 6)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:395` — `Flux_PassBuilder::ReadsBuffer` (score 7.0, LOC 6)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:402` — `Flux_PassBuilder::WritesBuffer` (score 7.0, LOC 6)

### Cluster 7 (6 functions)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:637` — `Flux_Primitives::AddCapsule` (score 6.7, LOC 11)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:649` — `Flux_Primitives::AddCylinder` (score 6.7, LOC 11)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:661` — `Flux_Primitives::AddTriangle` (score 6.7, LOC 12)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:601` — `Flux_Primitives::AddCube` (score 5.4, LOC 11)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:613` — `Flux_Primitives::AddWireframeCube` (score 5.4, LOC 11)
- `Zenith\Flux\Primitives\Flux_Primitives.cpp:590` — `Flux_Primitives::AddSphere` (score 5.3, LOC 10)

### Cluster 8 (6 functions)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:7` — `Zenith_Blackboard::SetFloat` (score 3.9, LOC 7)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:15` — `Zenith_Blackboard::SetInt` (score 3.9, LOC 7)
- `Zenith\AI\BehaviorTree\Zenith_Blackboard.cpp:23` — `Zenith_Blackboard::SetBool` (score 3.9, LOC 7)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:58` — `Flux_AnimationParameters::AddFloat` (score 3.9, LOC 8)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:67` — `Flux_AnimationParameters::AddInt` (score 3.9, LOC 8)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.cpp:76` — `Flux_AnimationParameters::AddBool` (score 3.9, LOC 8)

### Cluster 9 (6 functions)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:405` — `onNativeWindowCreated` (score 3.6, LOC 4)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:410` — `onNativeWindowDestroyed` (score 3.6, LOC 4)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:415` — `onNativeWindowRedrawNeeded` (score 3.6, LOC 4)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:420` — `onNativeWindowResized` (score 3.6, LOC 4)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:425` — `onInputQueueCreated` (score 3.6, LOC 4)
- `Zenith\Android\NativeGlue\android_native_app_glue.c:430` — `onInputQueueDestroyed` (score 3.6, LOC 4)

### Cluster 10 (6 functions)
- `Zenith\Flux\Flux_RenderTargets.cpp:150` — `Flux_RenderAttachmentCube::SRV` (score 2.8, LOC 9)
- `Zenith\Flux\Flux_RenderTargets.cpp:114` — `Flux_RenderAttachment::SRV` (score 2.5, LOC 5)
- `Zenith\Flux\Flux_RenderTargets.cpp:126` — `Flux_RenderAttachment::UAV` (score 2.5, LOC 5)
- `Zenith\Flux\Flux_RenderTargets.cpp:132` — `Flux_RenderAttachment::RTV` (score 2.5, LOC 5)
- `Zenith\Flux\Flux_RenderTargets.cpp:166` — `Flux_RenderAttachmentCube::UAV` (score 2.5, LOC 5)
- `Zenith\Flux\Flux_RenderTargets.cpp:177` — `Flux_RenderAttachmentCube::RTV` (score 2.5, LOC 5)

### Cluster 11 (6 functions)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:154` — `Zenith_EditorAutomation::AddStep_SetCameraPitch` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:162` — `Zenith_EditorAutomation::AddStep_SetCameraYaw` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:170` — `Zenith_EditorAutomation::AddStep_SetCameraFOV` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:178` — `Zenith_EditorAutomation::AddStep_SetCameraNear` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:186` — `Zenith_EditorAutomation::AddStep_SetCameraFar` (score 2.6, LOC 7)
- `Zenith\Editor\Zenith_EditorAutomation.cpp:194` — `Zenith_EditorAutomation::AddStep_SetCameraAspect` (score 2.6, LOC 7)

### Cluster 12 (6 functions)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:54` — `Zenith_UIComponent::CreateRect` (score 2.5, LOC 6)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:61` — `Zenith_UIComponent::CreateImage` (score 2.5, LOC 6)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:75` — `Zenith_UIComponent::CreateLayoutGroup` (score 2.5, LOC 6)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:89` — `Zenith_UIComponent::CreateOverlay` (score 2.5, LOC 6)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:96` — `Zenith_UIComponent::CreateScrollView` (score 2.5, LOC 6)
- `Zenith\EntityComponent\Components\Zenith_UIComponent.cpp:103` — `Zenith_UIComponent::CreateElement` (score 2.5, LOC 6)

### Cluster 13 (5 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1423` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1512` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1484` — `Zenith_Vulkan::ConvertToVkLoadAction` (score 8.9, LOC 14)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1499` — `Zenith_Vulkan::ConvertToVkStoreAction` (score 7.9, LOC 12)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1473` — `Zenith_Vulkan::ConvertToVkFormat_DepthStencil` (score 6.9, LOC 10)

### Cluster 14 (5 functions)
- `Zenith\Flux\Fog\Flux_Fog.cpp:151` — `Flux_Fog::ExecuteFroxelInject` (score 9.7, LOC 9)
- `Zenith\Flux\Fog\Flux_Fog.cpp:161` — `Flux_Fog::ExecuteFroxelLight` (score 9.7, LOC 9)
- `Zenith\Flux\Fog\Flux_Fog.cpp:171` — `Flux_Fog::ExecuteFroxelApply` (score 9.7, LOC 9)
- `Zenith\Flux\Fog\Flux_Fog.cpp:181` — `Flux_Fog::ExecuteRaymarch` (score 9.7, LOC 9)
- `Zenith\Flux\Fog\Flux_Fog.cpp:191` — `Flux_Fog::ExecuteGodRays` (score 9.7, LOC 9)

### Cluster 15 (5 functions)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:290` — `Zenith_MaterialAsset::GetDiffuseTexture` (score 6.4, LOC 9)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:300` — `Zenith_MaterialAsset::GetNormalTexture` (score 6.4, LOC 9)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:310` — `Zenith_MaterialAsset::GetRoughnessMetallicTexture` (score 6.4, LOC 9)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:320` — `Zenith_MaterialAsset::GetOcclusionTexture` (score 6.4, LOC 9)
- `Zenith\AssetHandling\Zenith_MaterialAsset.cpp:330` — `Zenith_MaterialAsset::GetEmissiveTexture` (score 6.4, LOC 9)

### Cluster 16 (5 functions)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:154` — `AddVector2` (score 6.2, LOC 5)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:159` — `AddVector3` (score 6.2, LOC 5)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:164` — `AddUVector4` (score 6.2, LOC 5)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:169` — `AddVector4` (score 6.2, LOC 5)
- `Zenith\DebugVariables\Zenith_DebugVariables.h:174` — `AddVector3` (score 6.2, LOC 5)

### Cluster 17 (5 functions)
- `Zenith\Flux\SSGI\Flux_SSGI.cpp:91` — `DebugGetRawSRV` (score 2.8, LOC 5)
- `Zenith\Flux\SSGI\Flux_SSGI.cpp:96` — `DebugGetResolvedSRV` (score 2.8, LOC 5)
- `Zenith\Flux\SSGI\Flux_SSGI.cpp:101` — `DebugGetDenoisedSRV` (score 2.8, LOC 5)
- `Zenith\Flux\SSR\Flux_SSR.cpp:85` — `DebugGetRayMarchSRV` (score 2.8, LOC 5)
- `Zenith\Flux\SSR\Flux_SSR.cpp:90` — `DebugGetResolvedSRV` (score 2.8, LOC 5)

### Cluster 18 (5 functions)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1426` — `Zenith_Vulkan_MemoryManager::DestroyVertexBuffer` (score 2.5, LOC 6)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1443` — `Zenith_Vulkan_MemoryManager::DestroyIndexBuffer` (score 2.5, LOC 6)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1450` — `Zenith_Vulkan_MemoryManager::DestroyConstantBuffer` (score 2.5, LOC 6)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1467` — `Zenith_Vulkan_MemoryManager::DestroyIndirectBuffer` (score 2.5, LOC 6)
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp:1474` — `Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer` (score 2.5, LOC 6)

### Cluster 19 (5 functions)
- `Zenith\Flux\Flux_Buffers.h:29` — `GetBuffer` (score 1.3, LOC 6)
- `Zenith\Flux\Flux_Buffers.h:99` — `GetBuffer` (score 1.3, LOC 6)
- `Zenith\Flux\Flux_Buffers.h:112` — `GetCBV` (score 1.3, LOC 6)
- `Zenith\Flux\Flux_Buffers.h:185` — `GetBuffer` (score 1.3, LOC 6)
- `Zenith\Flux\Flux_Buffers.h:198` — `GetUAV` (score 1.3, LOC 6)

### Cluster 20 (4 functions)
- `Zenith\Flux\SSR\Flux_SSR.cpp:188` — `ExecuteSSRResolve` (score 12.9, LOC 22)
- `Zenith\Flux\SSGI\Flux_SSGI.cpp:222` — `ExecuteSSGIDenoise` (score 11.2, LOC 21)
- `Zenith\Flux\SSGI\Flux_SSGI.cpp:204` — `ExecuteSSGIUpsample` (score 10.9, LOC 17)
- `Zenith\Flux\SSAO\Flux_SSAO.cpp:153` — `ExecuteSSAOBlur` (score 9.4, LOC 17)

## Tech-debt markers (43 total; showing first 30)

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
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:154` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp:251` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1325` **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1538` **TODO** Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime is affected by Time.timeScale)
- `Zenith\EntityComponent\Zenith_SceneManager.h:913` **TODO** Replace with engine hash map when available
- `Zenith\EntityComponent\Zenith_SceneManager.h:1032` **TODO** Replace with engine hash map
- `Zenith\Flux\Flux.h:104` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:113` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:88` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:546` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:178` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:293` **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:92` **TODO** Replace with engine hash map
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:309` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:1687` **TODO** Replace std::unordered_map with engine hash map.
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:6` **TODO** Replace with engine hash map
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:7` **TODO** Replace with engine hash set

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\EntityComponent\Components` | 26 | 6,043 | 75.7 | 53.0 | 27.3 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` |
| `Zenith\AI\Navigation` | 8 | 2,383 | 75.3 | 37.3 | 36.6 | `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` |
| `Zenith\EntityComponent` | 23 | 5,913 | 74.7 | 41.5 | 36.2 | `Zenith\EntityComponent\Zenith_SceneManager.cpp` |
| `Zenith\Flux\StaticMeshes` | 2 | 253 | 74.4 | 37.9 | 14.5 | `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` |
| `Zenith\Flux\DynamicLights` | 2 | 797 | 72.2 | 37.0 | 36.5 | `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` |
| `Zenith\Editor\Panels` | 19 | 2,806 | 72.1 | 2.9 | 23.2 | `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,205 | 71.2 | 67.7 | 104.5 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |
| `Zenith\Flux\Terrain` | 5 | 1,062 | 70.6 | 10.9 | 23.8 | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` |
| `Zenith\Flux\MeshAnimation` | 18 | 5,500 | 70.2 | 27.2 | 36.3 | `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` |
| `Zenith\AI\Perception` | 2 | 618 | 69.8 | 38.4 | 32.0 | `Zenith\AI\Perception\Zenith_PerceptionSystem.cpp` |
| `Zenith\UI` | 25 | 4,038 | 69.8 | 13.7 | 21.6 | `Zenith\UI\Zenith_UICanvas.cpp` |
| `Zenith\Flux\MeshGeometry` | 4 | 1,091 | 69.5 | 48.5 | 32.2 | `Zenith\Flux\MeshGeometry\Flux_MeshGeometry.cpp` |
| `Zenith\AI\Squad` | 6 | 2,070 | 69.1 | 25.6 | 45.7 | `Zenith\AI\Squad\Zenith_Squad.cpp` |
| `Zenith\Vulkan` | 20 | 5,706 | 69.1 | 9.5 | 25.9 | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.cpp` |
| `Zenith\Editor` | 15 | 5,148 | 68.7 | 45.7 | 31.9 | `Zenith\Editor\Zenith_Editor.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple.
