# Code Complexity Report

Root: `C:\dev\Zenith`


## Summary

- Files: **507**, SLOC: **142,498**, Functions: **6,794**, Classes: **1,243**
- Avg Cyclomatic: **28.5** (max 1451), Avg Cognitive: **40.2** (max 1860)
- Priority score distribution: **37** high (>=70, 7.3%), **154** medium, **316** low. P50=24.4, P90=68.2.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:827` — `ReverseBFSScramble` (score: 100.0)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-params`, `nesting-hell`
CC=82, cognitive=297, nesting=10, LOC=364, params=11, returns=2
Score breakdown: cognitive=30%, cyclomatic=25%, nesting=20%, length=15%, params=10%

Extract candidates:
- Lines 984-1120 (depth 10+ for 137 lines)

Suggested approach: extract `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:984-1120` (the depth-10+ block of 137 lines) into a helper.

### 2. `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:1201` — `GenerateLevelAttempt` (score: 96.2)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-returns`, `nesting-hell`
CC=119, cognitive=359, nesting=9, LOC=750, params=5, returns=10
Score breakdown: cognitive=31%, cyclomatic=26%, nesting=21%, length=16%, params=6%

Extract candidates:
- Lines 1288-1305 (depth 5+ for 18 lines)
- Lines 1506-1575 (depth 9+ for 70 lines)
- Lines 1763-1817 (depth 7+ for 55 lines)

Suggested approach: extract `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:1506-1575` (the depth-9+ block of 70 lines) into a helper.

### 3. `Games\TilePuzzle\Components\TilePuzzle_Solver.h:156` — `SolveLevel` (score: 93.8)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-returns`, `nesting-hell`
CC=91, cognitive=335, nesting=10, LOC=434, params=3, returns=6
Score breakdown: cognitive=32%, cyclomatic=27%, nesting=21%, length=16%, params=4%

Extract candidates:
- Lines 361-566 (depth 10+ for 206 lines)

Suggested approach: extract `Games\TilePuzzle\Components\TilePuzzle_Solver.h:361-566` (the depth-10+ block of 206 lines) into a helper.

### 4. `Games\TilePuzzle\Components\TilePuzzle_Solver.h:608` — `SolveLevelWithPath` (score: 93.8)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-returns`, `nesting-hell`
CC=84, cognitive=284, nesting=9, LOC=419, params=3, returns=7
Score breakdown: cognitive=32%, cyclomatic=27%, nesting=21%, length=16%, params=4%

Extract candidates:
- Lines 829-876 (depth 7+ for 48 lines)
- Lines 886-1007 (depth 9+ for 122 lines)

Suggested approach: extract `Games\TilePuzzle\Components\TilePuzzle_Solver.h:886-1007` (the depth-9+ block of 122 lines) into a helper.

### 5. `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2410` — `FindSolverInnerPath` (score: 93.8)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-params`, `many-returns`, `nesting-hell`
CC=80, cognitive=200, nesting=6, LOC=287, params=7, returns=8
Score breakdown: cognitive=32%, cyclomatic=27%, nesting=16%, length=16%, params=9%

Extract candidates:
- Lines 2607-2627 (depth 6+ for 21 lines)

Suggested approach: extract `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2607-2627` (the depth-6+ block of 21 lines) into a helper.

### 6. `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2865` — `FindCellPath` (score: 89.6)
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `many-params`, `many-returns`, `nesting-hell`
CC=43, cognitive=89, nesting=5, LOC=195, params=6, returns=6
Score breakdown: cognitive=34%, cyclomatic=28%, length=16%, nesting=14%, params=8%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 7. `Tools\Zenith_Tools_MeshExport.cpp:88` — `ExportAssimpMesh` (score: 88.8)
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-params`
CC=35, cognitive=77, nesting=4, LOC=296, params=7, returns=2
Score breakdown: cognitive=34%, cyclomatic=28%, length=17%, nesting=11%, params=10%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; introduce a parameter object / struct.

### 8. `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:2938` — `CheckCatElimination` (score: 85.0)
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=49, cognitive=100, nesting=6, LOC=200, params=0, returns=2
Score breakdown: cognitive=35%, cyclomatic=29%, nesting=18%, length=18%

Extract candidates:
- Lines 3086-3100 (depth 6+ for 15 lines)

Suggested approach: extract `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:3086-3100` (the depth-6+ block of 15 lines) into a helper.

### 9. `TilePuzzleRegistryViewer\TilePuzzleRegistryViewer.cpp:412` — `main` (score: 85.0)
Tags: `very-long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=34, cognitive=73, nesting=5, LOC=346, params=2, returns=3
Score breakdown: cognitive=35%, cyclomatic=29%, length=18%, nesting=15%, params=3%

Extract candidates:
- Lines 644-658 (depth 5+ for 15 lines)

Suggested approach: extract `TilePuzzleRegistryViewer\TilePuzzleRegistryViewer.cpp:644-658` (the depth-5+ block of 15 lines) into a helper.

### 10. `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2699` — `FindRoundTripPath` (score: 84.7)
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=42, cognitive=88, nesting=5, LOC=162, params=4, returns=5
Score breakdown: cognitive=35%, cyclomatic=30%, nesting=15%, length=14%, params=6%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 11. `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:548` — `ParallelGenerateTask` (score: 83.8)
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=31, cognitive=79, nesting=5, LOC=167, params=3, returns=0
Score breakdown: cognitive=36%, cyclomatic=30%, length=15%, nesting=15%, params=4%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 12. `Games\TilePuzzle\Components\TilePuzzle_MetaGame.h:384` — `UpdateVictoryOverlay` (score: 81.2)
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`
CC=38, cognitive=69, nesting=4, LOC=228, params=1, returns=4
Score breakdown: cognitive=37%, cyclomatic=31%, length=18%, nesting=12%, params=2%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; break up into smaller, named steps.

### 13. `Games\TilePuzzle\Components\TilePuzzle_Rules.h:65` — `CanMoveShape` (score: 80.2)
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-params`, `many-returns`
CC=27, cognitive=55, nesting=4, LOC=102, params=9, returns=8
Score breakdown: cognitive=37%, cyclomatic=28%, nesting=12%, params=12%, length=10%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 14. `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp:1465` — `main` (score: 80.0)
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=77, cognitive=149, nesting=3, LOC=571, params=2, returns=10
Score breakdown: cognitive=38%, cyclomatic=31%, length=19%, nesting=9%, params=3%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; check whether returns can be consolidated via result-holding variable.

### 15. `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:3305` — `UpdateVisuals` (score: 77.4)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=30, cognitive=47, nesting=4, LOC=149, params=1, returns=3
Score breakdown: cognitive=39%, cyclomatic=32%, length=14%, nesting=13%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 16. `Games\Sokoban\Components\Sokoban_Solver.h:95` — `SolveLevel` (score: 76.8)
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-params`
CC=21, cognitive=41, nesting=4, LOC=140, params=7, returns=3
Score breakdown: cognitive=39%, cyclomatic=23%, length=14%, nesting=13%, params=11%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 17. `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:5520` — `Test_ShapeRotation` (score: 74.6)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=33, cognitive=52, nesting=4, LOC=128, params=0, returns=3
Score breakdown: cognitive=40%, cyclomatic=34%, nesting=13%, length=13%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 18. `Tools\Zenith_Tools_FontExport.cpp:75` — `Zenith_Tools_FontExport::ExportFromFile` (score: 74.5)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=22, cognitive=42, nesting=4, LOC=132, params=5, returns=2
Score breakdown: cognitive=40%, cyclomatic=25%, nesting=13%, length=13%, params=8%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 19. `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h:302` — `GenerateLevel` (score: 74.4)
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-params`
CC=22, cognitive=36, nesting=3, LOC=188, params=6, returns=4
Score breakdown: cognitive=36%, cyclomatic=25%, length=19%, nesting=10%, params=10%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 20. `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:1147` — `OnLevelCompleted` (score: 73.2)
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=34, cognitive=52, nesting=3, LOC=142, params=0, returns=0
Score breakdown: cognitive=41%, cyclomatic=34%, length=15%, nesting=10%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h` | `low-mi`, `god-file` | 93.0 | 270 | 797 | 10 | 1551 | 11 |
| `Games\TilePuzzle\Components\TilePuzzle_Solver.h` | `low-mi` | 91.1 | 187 | 664 | 10 | 806 | 18 |
| `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h` | `low-mi`, `god-file` | 89.4 | 1451 | 1860 | 7 | 7227 | 122 |
| `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h` | `low-mi`, `god-file` | 86.8 | 681 | 1128 | 7 | 3410 | 102 |
| `TilePuzzleRegistryViewer\TilePuzzleRegistryViewer.cpp` | `low-mi` | 83.2 | 94 | 125 | 5 | 590 | 11 |
| `Tools\Zenith_Tools_MeshExport.cpp` | `low-mi` | 82.6 | 89 | 152 | 4 | 640 | 15 |
| `Games\TilePuzzle\Components\TilePuzzle_Rules.h` | `low-mi` | 81.8 | 63 | 149 | 5 | 268 | 5 |
| `Games\TilePuzzle\Components\TilePuzzle_MetaGame.h` | `low-mi` | 80.4 | 128 | 246 | 4 | 733 | 32 |
| `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp` | `low-mi`, `god-file` | 80.0 | 255 | 422 | 4 | 1522 | 28 |
| `Tools\Zenith_Tools_GltfExport.cpp` | `low-mi` | 79.1 | 34 | 67 | 5 | 248 | 4 |
| `Games\TilePuzzle\Components\Pinball_Behaviour.h` | `low-mi`, `god-file` | 78.7 | 288 | 500 | 5 | 2040 | 63 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` | `low-mi` | 75.7 | 54 | 107 | 6 | 407 | 12 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 75.3 | 117 | 230 | 6 | 768 | 25 |
| `Tools\Zenith_Tools_TerrainExport.cpp` | `low-mi` | 75.0 | 42 | 74 | 3 | 544 | 13 |
| `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` | `low-mi` | 74.5 | 28 | 70 | 6 | 239 | 9 |
| `Tools\Zenith_Tools_AssimpConvert.cpp` | `low-mi` | 74.2 | 40 | 66 | 4 | 292 | 15 |
| `Tools\Zenith_Tools_TextureExport.cpp` | `low-mi` | 74.0 | 56 | 82 | 4 | 346 | 14 |
| `Games\AIShowcase\Components\AIShowcase_Behaviour.h` | `low-mi` | 73.9 | 101 | 172 | 5 | 856 | 29 |
| `Games\Sokoban\Components\Sokoban_Solver.h` | `low-mi` | 73.9 | 26 | 47 | 4 | 147 | 4 |
| `Games\Sokoban\Components\Sokoban_Rendering.h` | `low-mi` | 73.5 | 25 | 46 | 6 | 220 | 9 |
| `Tools\Zenith_Tools_FontExport.cpp` | `low-mi` | 72.4 | 24 | 44 | 4 | 153 | 5 |
| `Zenith\EntityComponent\Zenith_SceneData.cpp` | `low-mi`, `god-file` | 72.3 | 172 | 293 | 5 | 946 | 49 |
| `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` | `low-mi` | 72.2 | 72 | 115 | 4 | 781 | 25 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` | `low-mi` | 72.1 | 92 | 139 | 4 | 493 | 16 |
| `Zenith\EntityComponent\Zenith_SceneManager_AsyncLoad.cpp` | `low-mi` | 71.9 | 69 | 124 | 6 | 449 | 17 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_Vulkan_MemoryManager` | `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.h:35` | 1.00 | 10 | 25 |
| `Flux_TerrainStreamingManager` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.h:91` | 1.00 | 6 | 11 |
| `Flux_Gizmos` | `Zenith\Flux\Gizmos\Flux_Gizmos.h:36` | 1.00 | 4 | 5 |
| `Zenith_Vulkan` | `Zenith\Vulkan\Zenith_Vulkan.h:103` | 1.00 | 3 | 4 |
| `Zenith_SceneManager` | `Zenith\EntityComponent\Zenith_SceneManager.h:134` | 0.99 | 11 | 26 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:281` | 0.98 | 3 | 33 |
| `TilePuzzle_AutoTest` | `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:41` | 0.97 | 122 | 100 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:194` | 0.97 | 3 | 11 |
| `Runner_AnimationDriver` | `Games\Runner\Components\Runner_AnimationDriver.h:51` | 0.96 | 9 | 3 |
| `Zenith_SceneData` | `Zenith\EntityComponent\Zenith_SceneData.h:229` | 0.96 | 48 | 36 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 49 | 29 |
| `Zenith_TerrainComponent` | `Zenith\EntityComponent\Components\Zenith_TerrainComponent.h:55` | 0.95 | 10 | 17 |
| `Flux_AnimationController` | `Zenith\Flux\MeshAnimation\Flux_AnimationController.h:40` | 0.95 | 14 | 20 |
| `Flux_InstanceGroup` | `Zenith\Flux\InstancedMeshes\Flux_InstanceGroup.h:39` | 0.95 | 12 | 20 |
| `Zenith_Window` | `Zenith\Android\Zenith_Android_Window.h:9` | 0.95 | 10 | 8 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 217 | 10 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 90 | 0 |  |
| `Zenith\Maths\Zenith_Maths.h` | 85 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 84 | 0 |  |
| `Zenith\EntityComponent\Zenith_SceneManager.h` | 82 | 4 | **yes** |
| `Zenith\DataStream\Zenith_DataStream.h` | 78 | 1 |  |
| `Zenith\EntityComponent\Zenith_SceneData.h` | 77 | 6 | **yes** |
| `Zenith\Flux\Flux.h` | 76 | 5 |  |
| `Zenith\EntityComponent\Zenith_Scene.h` | 71 | 3 |  |
| `Zenith\EntityComponent\Components\Zenith_TransformComponent.h` | 68 | 7 |  |
| `Zenith\Flux\Flux_Graphics.h` | 55 | 2 |  |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 49 | 1 |  |
| `Zenith\Flux\Flux_Buffers.h` | 44 | 1 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 42 | 3 |  |
| `Zenith\Collections\Zenith_Vector.h` | 41 | 3 |  |

## Include cycles (2 total)

Strongly-connected components in the `#include` graph. Any cycle means the files in it are mutually dependent — the clearest architectural red flag in a C++ codebase.

### Cycle 1 (2 files)
- `Zenith\EntityComponent\Zenith_SceneData.h`
- `Zenith\EntityComponent\Zenith_SceneManager.h`

### Cycle 2 (2 files)
- `Zenith\Vulkan\Zenith_PlatformGraphics_Include.h`
- `Zenith\Vulkan\Zenith_Vulkan_MemoryManager.h`

## Near-duplicate functions (346 cluster(s))

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
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:484` — `GenerateIcon_SoundOn` (score 15.2, LOC 39)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:692` — `GenerateIcon_Hint` (score 15.2, LOC 40)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:375` — `GenerateIcon_Skip` (score 14.6, LOC 31)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:523` — `GenerateIcon_SoundOff` (score 14.5, LOC 30)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:346` — `GenerateIcon_Undo` (score 14.4, LOC 29)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:406` — `GenerateIcon_Lock` (score 14.4, LOC 29)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:732` — `GenerateIcon_HintToken` (score 14.3, LOC 27)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:435` — `GenerateIcon_Menu` (score 14.1, LOC 25)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:460` — `GenerateIcon_Back` (score 14.1, LOC 24)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:265` — `GenerateIcon_StarEmpty` (score 14.0, LOC 23)
- `Games\TilePuzzle\Components\TilePuzzle_AssetGen.h:243` — `GenerateIcon_StarFilled` (score 13.9, LOC 22)

### Cluster 4 (11 functions)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1551` — `Zenith_AutomationTests::TestLayoutHorizontalPositioning` (score 6.6, LOC 56)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2109` — `Zenith_AutomationTests::TestLayoutInvisibleChildrenSkipped` (score 6.4, LOC 53)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1607` — `Zenith_AutomationTests::TestLayoutVerticalPositioning` (score 6.2, LOC 50)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1869` — `Zenith_AutomationTests::TestLayoutChildForceExpand` (score 6.2, LOC 50)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1822` — `Zenith_AutomationTests::TestLayoutReverseArrangement` (score 5.9, LOC 47)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1657` — `Zenith_AutomationTests::TestLayoutPaddingAffectsPositioning` (score 5.8, LOC 45)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2065` — `Zenith_AutomationTests::TestLayoutSingleChild` (score 5.7, LOC 44)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1702` — `Zenith_AutomationTests::TestLayoutMiddleCenterAlignment` (score 5.4, LOC 40)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1742` — `Zenith_AutomationTests::TestLayoutUpperLeftAlignment` (score 5.4, LOC 40)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:1782` — `Zenith_AutomationTests::TestLayoutLowerRightAlignment` (score 5.4, LOC 40)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2029` — `Zenith_AutomationTests::TestLayoutEmptyGroup` (score 5.1, LOC 36)

### Cluster 5 (11 functions)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8783` — `Zenith_SceneTests::TestDeepHierarchyActiveInHierarchy` (score 3.9, LOC 41)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8730` — `Zenith_SceneTests::TestHasChildrenAndCount` (score 3.1, LOC 30)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8824` — `Zenith_SceneTests::TestSetParentAcrossScenes` (score 3.0, LOC 29)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8704` — `Zenith_SceneTests::TestReparentEntity` (score 2.8, LOC 26)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8680` — `Zenith_SceneTests::TestUnparentEntity` (score 2.6, LOC 24)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8760` — `Zenith_SceneTests::TestIsRootEntity` (score 2.6, LOC 23)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:11041` — `Zenith_SceneTests::TestCircularHierarchyPreventionGrandchild` (score 2.6, LOC 23)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:11079` — `Zenith_SceneTests::TestDetachFromParent` (score 2.6, LOC 24)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8951` — `Zenith_SceneTests::TestIsEnabledVsIsActiveInHierarchy` (score 2.5, LOC 22)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:8997` — `Zenith_SceneTests::TestSetTransientIsTransient` (score 2.2, LOC 18)
- `Zenith\UnitTests\Zenith_SceneTests.cpp:11064` — `Zenith_SceneTests::TestSelfParentPrevention` (score 2.0, LOC 15)

### Cluster 6 (11 functions)
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

### Cluster 7 (10 functions)
- `Tools\Zenith_Tools_TestAssetExport.cpp:774` — `CreateDeathAnimation` (score 5.6, LOC 63)
- `Zenith\UnitTests\Zenith_UnitTests.cpp:4491` — `CreateWalkAnimation` (score 5.5, LOC 62)
- `Zenith\UnitTests\Zenith_UnitTests.cpp:4556` — `CreateRunAnimation` (score 5.5, LOC 62)
- `Tools\Zenith_Tools_TestAssetExport.cpp:432` — `CreateWalkAnimation` (score 5.4, LOC 61)
- `Tools\Zenith_Tools_TestAssetExport.cpp:493` — `CreateRunAnimation` (score 5.4, LOC 61)
- `Tools\Zenith_Tools_TestAssetExport.cpp:633` — `CreateAttack3Animation` (score 5.0, LOC 55)
- `Tools\Zenith_Tools_TestAssetExport.cpp:588` — `CreateAttack2Animation` (score 4.2, LOC 45)
- `Tools\Zenith_Tools_TestAssetExport.cpp:688` — `CreateDodgeAnimation` (score 4.1, LOC 43)
- `Tools\Zenith_Tools_TestAssetExport.cpp:731` — `CreateHitAnimation` (score 4.1, LOC 43)
- `Tools\Zenith_Tools_TestAssetExport.cpp:554` — `CreateAttack1Animation` (score 3.4, LOC 34)

### Cluster 8 (10 functions)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2285` — `Zenith_AutomationTests::TestSetUIShadowStep` (score 4.5, LOC 28)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2259` — `Zenith_AutomationTests::TestSetUIGradientColorStep` (score 4.4, LOC 26)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2313` — `Zenith_AutomationTests::TestSetUIShadowColorStep` (score 4.4, LOC 26)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2339` — `Zenith_AutomationTests::TestSetUIRectBorderStep` (score 4.4, LOC 26)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2413` — `Zenith_AutomationTests::TestSetUIButtonCornerRadiusStep` (score 4.4, LOC 27)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2440` — `Zenith_AutomationTests::TestSetUIButtonShadowStep` (score 4.4, LOC 27)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2492` — `Zenith_AutomationTests::TestSetUIButtonGradientColorStep` (score 4.4, LOC 26)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2518` — `Zenith_AutomationTests::TestSetUIButtonBorderColorStep` (score 4.4, LOC 27)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2467` — `Zenith_AutomationTests::TestSetUIButtonShadowColorStep` (score 4.3, LOC 25)
- `Zenith\UnitTests\Zenith_AutomationTests.cpp:2545` — `Zenith_AutomationTests::TestSetUIButtonBorderThicknessStep` (score 4.3, LOC 25)

### Cluster 9 (10 functions)
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

### Cluster 10 (10 functions)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:284` — `Zenith_BTAction_MoveToEntity::WriteToDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:296` — `Zenith_BTCondition_BlackboardCompare::WriteToDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:154` — `Zenith_BTAction_MoveTo::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:324` — `Zenith_BTAction_SetBlackboardBool::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:362` — `Zenith_BTAction_SetBlackboardFloat::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:131` — `Zenith_BTCondition_InRange::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:185` — `Zenith_BTCondition_CanSeeTarget::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:232` — `Zenith_BTCondition_BlackboardBool::WriteToDataStream` (score 2.9, LOC 11)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:399` — `Zenith_BTAction_Log::WriteToDataStream` (score 2.8, LOC 9)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:44` — `Zenith_BTCondition_HasTarget::WriteToDataStream` (score 2.8, LOC 9)

### Cluster 11 (10 functions)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:1134` — `Zenith_PhysicsTests::TestAccumulatorDoesNotOverStep` (score 2.9, LOC 27)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:328` — `Zenith_PhysicsTests::TestGravityReenabledBodyFalls` (score 2.7, LOC 25)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:500` — `Zenith_PhysicsTests::TestAddImpulseInstantVelocityChange` (score 2.7, LOC 25)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:444` — `Zenith_PhysicsTests::TestZeroVelocityNoMovement` (score 2.6, LOC 24)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:1018` — `Zenith_PhysicsTests::TestRebuildColliderPreservesVelocity` (score 2.6, LOC 23)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:379` — `Zenith_PhysicsTests::TestGetLinearVelocityMatchesSet` (score 2.5, LOC 22)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:422` — `Zenith_PhysicsTests::TestGetAngularVelocityMatchesSet` (score 2.5, LOC 22)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:563` — `Zenith_PhysicsTests::TestImpulseOnStaticBodyNoEffect` (score 2.4, LOC 21)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:288` — `Zenith_PhysicsTests::TestStaticBodyDoesNotFall` (score 2.3, LOC 20)
- `Zenith\UnitTests\Zenith_PhysicsTests.cpp:308` — `Zenith_PhysicsTests::TestGravityDisabledBodyStaysStill` (score 2.3, LOC 20)

### Cluster 12 (9 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1422` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.9, LOC 50)
- `Zenith\Vulkan\Zenith_Vulkan_Texture.cpp:22` — `Zenith_Vulkan_Texture::ConvertToVkFormat_Colour` (score 24.9, LOC 50)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1511` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.8, LOC 24)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1483` — `Zenith_Vulkan::ConvertToVkLoadAction` (score 9.0, LOC 15)
- `Zenith\Vulkan\Zenith_Vulkan_Texture.cpp:83` — `Zenith_Vulkan_Texture::ConvertToVkLoadAction` (score 9.0, LOC 15)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1498` — `Zenith_Vulkan::ConvertToVkStoreAction` (score 8.0, LOC 13)
- `Zenith\Vulkan\Zenith_Vulkan_Texture.cpp:98` — `Zenith_Vulkan_Texture::ConvertToVkStoreAction` (score 8.0, LOC 13)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1472` — `Zenith_Vulkan::ConvertToVkFormat_DepthStencil` (score 7.0, LOC 11)
- `Zenith\Vulkan\Zenith_Vulkan_Texture.cpp:72` — `Zenith_Vulkan_Texture::ConvertToVkFormat_DepthStencil` (score 7.0, LOC 11)

### Cluster 13 (9 functions)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:296` — `Zenith_BTAction_MoveToEntity::ReadFromDataStream` (score 3.1, LOC 13)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:165` — `Zenith_BTAction_MoveTo::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:335` — `Zenith_BTAction_SetBlackboardBool::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:373` — `Zenith_BTAction_SetBlackboardFloat::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:142` — `Zenith_BTCondition_InRange::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:196` — `Zenith_BTCondition_CanSeeTarget::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:243` — `Zenith_BTCondition_BlackboardBool::ReadFromDataStream` (score 3.0, LOC 12)
- `Zenith\AI\BehaviorTree\Zenith_BTActions.cpp:408` — `Zenith_BTAction_Log::ReadFromDataStream` (score 2.8, LOC 10)
- `Zenith\AI\BehaviorTree\Zenith_BTConditions.cpp:53` — `Zenith_BTCondition_HasTarget::ReadFromDataStream` (score 2.8, LOC 10)

### Cluster 14 (8 functions)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:503` — `Zenith_EditorTests::TestModeTransitionFullCycle` (score 3.4, LOC 34)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:482` — `Zenith_EditorTests::TestModeTransitionPausedToStopped` (score 2.4, LOC 21)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:444` — `Zenith_EditorTests::TestModeTransitionStoppedToPlaying` (score 2.3, LOC 19)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:463` — `Zenith_EditorTests::TestModeTransitionPlayingToPaused` (score 2.3, LOC 19)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:576` — `Zenith_EditorTests::TestGizmoModeSwitch` (score 2.0, LOC 16)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:592` — `Zenith_EditorTests::TestGizmoModeTranslate` (score 1.7, LOC 11)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:603` — `Zenith_EditorTests::TestGizmoModeRotate` (score 1.7, LOC 11)
- `Zenith\UnitTests\Zenith_EditorTests.cpp:614` — `Zenith_EditorTests::TestGizmoModeScale` (score 1.7, LOC 11)

### Cluster 15 (8 functions)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:159` — `GetSingleShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:169` — `GetDominoShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:179` — `GetLShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:189` — `GetTShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:199` — `GetIShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:209` — `GetSShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:219` — `GetZShape` (score 2.8, LOC 9)
- `Games\TilePuzzle\Components\TilePuzzle_Types.h:229` — `GetOShape` (score 2.8, LOC 9)

### Cluster 16 (8 functions)
- `Zenith\AI\BehaviorTree\Zenith_BTNode.h:68` — `SetNodeName` (score 2.2, LOC 2)
- `Zenith\AI\Squad\Zenith_Squad.h:133` — `SetName` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:98` — `SetPositionOffset` (score 2.2, LOC 2)
- `Zenith\EntityComponent\Components\Zenith_LightComponent.h:104` — `SetDirectionOffset` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationLayer.h:37` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:167` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_AnimationStateMachine.h:280` — `SetName` (score 2.2, LOC 2)
- `Zenith\Flux\MeshAnimation\Flux_BlendTree.h:233` — `SetParameter` (score 2.2, LOC 2)

### Cluster 17 (8 functions)
- `Games\AIShowcase\AIShowcase.cpp:288` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Combat\Combat.cpp:768` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Exploration\Exploration.cpp:756` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Marble\Marble.cpp:390` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Runner\Runner.cpp:591` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Sokoban\Sokoban.cpp:355` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Survival\Survival.cpp:836` — `Project_LoadInitialScene` (score 1.4, LOC 7)
- `Games\Test\Test.cpp:99` — `Project_LoadInitialScene` (score 1.4, LOC 7)

### Cluster 18 (7 functions)
- `Games\TilePuzzle\Components\TilePuzzle_MetaGame.h:749` — `SetSettingsVisible` (score 16.0, LOC 26)
- `Games\Survival\Components\Survival_Behaviour.h:784` — `SetHUDVisible` (score 11.7, LOC 22)
- `Games\Combat\Components\Combat_Behaviour.h:457` — `SetHUDVisible` (score 11.5, LOC 19)
- `Games\Sokoban\Components\Sokoban_Behaviour.h:460` — `SetHUDVisible` (score 11.5, LOC 19)
- `Games\Exploration\Components\Exploration_Behaviour.h:393` — `SetHUDVisible` (score 11.1, LOC 14)
- `Games\Marble\Components\Marble_Behaviour.h:396` — `SetHUDVisible` (score 11.1, LOC 14)
- `Games\Runner\Components\Runner_Behaviour.h:351` — `SetHUDVisible` (score 11.1, LOC 14)

### Cluster 19 (7 functions)
- `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp:546` — `operator==` (score 9.1, LOC 9)
- `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp:502` — `operator==` (score 7.4, LOC 8)
- `Games\Sokoban\Components\Sokoban_Solver.h:44` — `operator==` (score 5.8, LOC 7)
- `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp:527` — `operator==` (score 5.8, LOC 7)
- `Zenith\Flux\Flux_Types.h:90` — `operator==` (score 5.6, LOC 5)
- `Zenith\Flux\Flux.h:204` — `operator==` (score 5.4, LOC 2)
- `Zenith\Flux\Flux.h:223` — `operator==` (score 5.4, LOC 2)

### Cluster 20 (6 functions)
- `Games\Survival\Survival.cpp:658` — `Project_RegisterEditorAutomationSteps` (score 14.1, LOC 177)
- `Games\Sokoban\Sokoban.cpp:200` — `Project_RegisterEditorAutomationSteps` (score 12.4, LOC 154)
- `Games\Runner\Runner.cpp:476` — `Project_RegisterEditorAutomationSteps` (score 9.4, LOC 114)
- `Games\Combat\Combat.cpp:657` — `Project_RegisterEditorAutomationSteps` (score 9.1, LOC 110)
- `Games\Marble\Marble.cpp:281` — `Project_RegisterEditorAutomationSteps` (score 8.9, LOC 108)
- `Games\Test\Test.cpp:63` — `Project_RegisterEditorAutomationSteps` (score 3.5, LOC 35)

## Tech-debt markers (52 total; showing first 30)

- `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:1668` **TODO** Replace with engine hash map
- `Games\TilePuzzle\Components\TilePuzzle_Behaviour.h:1852` **TODO** Replace with engine container
- `Games\TilePuzzle\Components\TilePuzzle_Solver.h:746` **TODO** Replace with engine hash map
- `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:39` **TODO** Replace with engine hash map
- `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2525` **TODO** Replace with engine hash map
- `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2730` **TODO** Replace with engine hash map
- `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h:2903` **TODO** Replace with engine hash map
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
- `Zenith\EntityComponent\Zenith_SceneData.cpp:1195` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.cpp:1289` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.h:581` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneData.h:764` **TODO** Replace with engine hash map
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1318` **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- `Zenith\EntityComponent\Zenith_SceneManager.cpp:1519` **TODO** Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime is affected by Time.timeScale)
- `Zenith\EntityComponent\Zenith_SceneManager.h:913` **TODO** Replace with engine hash map when available
- `Zenith\EntityComponent\Zenith_SceneManager.h:1031` **TODO** Replace with engine hash map
- `Zenith\Flux\Flux.h:156` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux.h:165` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\Flux_CommandList.h:88` **TODO** elide second memcpy by having AddCommand record directly into the
- `Zenith\Flux\IBL\Flux_IBL.cpp:546` **TODO** create per-face 2D SRVs for cubemap debug display.

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Games\TilePuzzle\Components` | 11 | 11,008 | 93.0 | 78.7 | 169.4 | `Games\TilePuzzle\Components\TilePuzzle_LevelGenerator.h` |
| `Games\TilePuzzle\Tests` | 1 | 7,227 | 89.4 | 89.4 | 1451.0 | `Games\TilePuzzle\Tests\TilePuzzle_AutoTest.h` |
| `TilePuzzleRegistryViewer` | 1 | 590 | 83.2 | 83.2 | 94.0 | `TilePuzzleRegistryViewer\TilePuzzleRegistryViewer.cpp` |
| `Tools` | 14 | 3,224 | 82.6 | 34.0 | 23.6 | `Tools\Zenith_Tools_MeshExport.cpp` |
| `TilePuzzleLevelGen` | 4 | 1,944 | 80.0 | 60.3 | 78.5 | `TilePuzzleLevelGen\TilePuzzleLevelGen.cpp` |
| `Zenith\EntityComponent\Components` | 26 | 6,040 | 75.7 | 53.0 | 27.3 | `Zenith\EntityComponent\Components\Zenith_TerrainComponent_Editor.cpp` |
| `Zenith\AI\Navigation` | 8 | 2,383 | 75.3 | 37.1 | 36.6 | `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` |
| `Zenith\Flux\StaticMeshes` | 2 | 253 | 74.5 | 37.9 | 14.5 | `Zenith\Flux\StaticMeshes\Flux_StaticMeshes.cpp` |
| `Games\AIShowcase\Components` | 1 | 856 | 73.9 | 73.9 | 101.0 | `Games\AIShowcase\Components\AIShowcase_Behaviour.h` |
| `Games\Sokoban\Components` | 8 | 1,525 | 73.9 | 48.0 | 24.9 | `Games\Sokoban\Components\Sokoban_Solver.h` |
| `Zenith\EntityComponent` | 22 | 5,832 | 72.3 | 36.0 | 36.9 | `Zenith\EntityComponent\Zenith_SceneData.cpp` |
| `Zenith\Flux\DynamicLights` | 2 | 797 | 72.2 | 37.0 | 36.5 | `Zenith\Flux\DynamicLights\Flux_DynamicLights.cpp` |
| `Zenith\Editor\Panels` | 19 | 2,799 | 72.1 | 2.9 | 23.1 | `Zenith\Editor\Panels\Zenith_EditorPanel_Hierarchy.cpp` |
| `Games\Test\Components` | 4 | 570 | 71.8 | 15.0 | 19.2 | `Games\Test\Components\PlayerController_Behaviour.cpp` |
| `Zenith\Flux\RenderGraph` | 4 | 2,204 | 71.2 | 67.7 | 104.5 | `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple.
