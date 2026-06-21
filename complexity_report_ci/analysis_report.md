# Code Complexity Report

Root: `C:\dev\Zenith`


## Health summary

- Profile: `engine-ci`
- Parser: `tree-sitter` (requested `tree-sitter`)
- High-priority files (score >= 70): **23**
- P90 file priority: **65.9**
- Include cycles: **0**
- Duplicate clusters: **5**
- Include edges resolved: 2852 relative-path + 0 unique-basename (high confidence, gated); 2 ambiguous-basename (low confidence, suppressed from gates); 2 basename total (0%)
- External includes (middleware/third-party): 115
- Include redundancy: score **259** (52 duplicate, 207 transitive, 0 unresolved, 0 conflicting)
- Worst function: `Zenith\AI\Navigation\Zenith_NavMesh.cpp:779` `Zenith_NavMesh::GetRandomReachablePointInRadius` (score 82.5)
- Worst file: `Zenith\AI\Navigation\Zenith_NavMesh.cpp` (score 82.5)

## Architecture

**Non-allowlisted findings that FAIL the gate** (add to the matching allowlist only when deliberately accepted):
- **max-architecture-violations: 4** — e.g. `layer-up: Zenith/D3D12/Zenith_D3D12.cpp => Flux/Flux_RendererImpl.h`
- **max-engine-singleton-count: 6** — e.g. `Zenith/D3D12/Zenith_D3D12.cpp => g_xEngine (1>0)`
- **max-forbidden-tokens: 5** — e.g. `token:Zenith/AssetHandling/Zenith_FontAsset.cpp => std::vector`

Report-only signals (not gated by default):
- max-module-cycles: 1 — e.g. `module-cycle: AI|AssetHandling|Collections|Core|DataStream|DebugVariables|Editor|EntityComponent|FileAccess|Flux|Input|Maths|Physics|Prefab|Profiling|Scripting|TaskSystem|UI|Vulkan|Windows|ZenithECS`
- max-zone-of-pain-modules: 4 — e.g. `zone-of-pain: Maths`

- Modules: **25**, unmapped files: **8**, module-level cycles (report-only): **1**, hub files: **0**

| Module | Layer | Files | Ca | Ce | Instability | Abstractness | Distance | Zone |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Android | 0 | 12 | 1 | 4 | 0.80 | 0.00 | 0.20 | main-sequence |
| Collections | 0 | 5 | 16 | 2 | 0.11 | 0.00 | 0.89 | pain |
| Core | 0 | 57 | 24 | 8 | 0.25 | 0.00 | 0.75 | - |
| DataStream | 0 | 4 | 10 | 2 | 0.17 | 0.00 | 0.83 | pain |
| DebugVariables | 0 | 2 | 8 | 3 | 0.27 | 1.00 | 0.27 | - |
| FileAccess | 0 | 2 | 6 | 1 | 0.14 | n/a | 0.00 | - |
| Maths | 0 | 4 | 12 | 1 | 0.08 | 0.00 | 0.92 | pain |
| Profiling | 0 | 2 | 12 | 3 | 0.20 | 0.00 | 0.80 | pain |
| Windows | 0 | 11 | 3 | 4 | 0.57 | 0.00 | 0.43 | - |
| Input | 1 | 7 | 6 | 2 | 0.25 | 0.00 | 0.75 | - |
| SaveData | 1 | 2 | 0 | 3 | 1.00 | 0.00 | 0.00 | main-sequence |
| TaskSystem | 1 | 2 | 4 | 4 | 0.50 | 0.00 | 0.50 | - |
| Telemetry | 1 | 2 | 0 | 5 | 1.00 | 0.00 | 0.00 | main-sequence |
| ZenithECS | 1 | 29 | 9 | 3 | 0.25 | 0.07 | 0.68 | - |
| AI | 2 | 38 | 2 | 6 | 0.75 | 0.05 | 0.20 | main-sequence |
| AssetHandling | 2 | 32 | 6 | 8 | 0.57 | 0.00 | 0.43 | - |
| EntityComponent | 2 | 51 | 3 | 16 | 0.84 | 0.00 | 0.16 | main-sequence |
| Flux | 2 | 214 | 8 | 11 | 0.58 | 0.01 | 0.41 | - |
| Physics | 2 | 8 | 4 | 4 | 0.50 | 0.00 | 0.50 | - |
| Prefab | 2 | 2 | 2 | 7 | 0.78 | 0.00 | 0.22 | - |
| Scripting | 2 | 7 | 4 | 5 | 0.56 | 0.25 | 0.19 | main-sequence |
| Vulkan | 2 | 27 | 1 | 6 | 0.86 | 0.00 | 0.14 | main-sequence |
| UI | 3 | 25 | 2 | 7 | 0.78 | 0.00 | 0.22 | - |
| Editor | 4 | 56 | 3 | 17 | 0.85 | 0.04 | 0.11 | main-sequence |
| EngineComposition | 5 | 5 | 0 | 14 | 1.00 | 0.00 | 0.00 | main-sequence |

_A/D are a regex proxy and report-only. module-cycles and zone-of-pain are reported but not gated by default; module-scope statics, forbidden tokens, missing PCH/`#pragma once`, leaf `g_xEngine`, layering, leaf and encapsulation findings ARE gated (threshold 0, ratcheted via allowlists)._

## Summary

- Files: **614**, SLOC: **98,138**, Functions: **6,191**, Classes: **1,465**
- Avg Cyclomatic: **17.6** (max 249), Avg Cognitive: **22.3** (max 475)
- Priority score distribution: **23** high (>=70, 3.7%), **144** medium, **447** low. P50=9.1, P90=65.9.

## Suggested refactoring queue (top 20 functions)

Sorted by composite priority score. Each entry is a single function — the `file:line` link goes directly to its first line.

### 1. `Zenith\AI\Navigation\Zenith_NavMesh.cpp:779` — `Zenith_NavMesh::GetRandomReachablePointInRadius` (score: 82.5)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=30, cognitive=53, nesting=3, LOC=226, params=4, returns=10
Score breakdown: cognitive=36%, cyclomatic=30%, length=18%, nesting=9%, params=6%

Suggested approach: split into multiple functions; identify the 2-3 core responsibilities; check whether returns can be consolidated via result-holding variable.

### 2. `Zenith\Profiling\Zenith_Profiling.cpp:470` — `Zenith_Profiling::WriteTextReport` (score: 82.2)
Dominant signal: **cognitive**
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=34, cognitive=69, nesting=5, LOC=179, params=1, returns=0
Score breakdown: cognitive=36%, cyclomatic=30%, length=16%, nesting=15%, params=2%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 3. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp:419` — `Zenith_TerrainEditor::HandleViewportInput` (score: 70.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=40, cognitive=48, nesting=2, LOC=123, params=1, returns=2
Score breakdown: cognitive=43%, cyclomatic=36%, length=13%, nesting=7%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 4. `Zenith\Core\Zenith_AutomatedTest.cpp:389` — `Zenith_AutomatedTestRunner::Tick` (score: 70.2)
Dominant signal: **cognitive**
Tags: `very-long-function`, `complex-branching`, `hard-to-follow`, `many-returns`, `big-switch`
CC=52, cognitive=37, nesting=1, LOC=288, params=0, returns=18
Score breakdown: cognitive=40%, cyclomatic=36%, length=21%, nesting=4%

Suggested approach: consider a dispatch table or polymorphism if the switch is on a type/enum; split into multiple functions; identify the 2-3 core responsibilities.

### 5. `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp:300` — `Flux_IKSolver::SolveChain` (score: 69.8)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=40, nesting=4, LOC=119, params=4, returns=1
Score breakdown: cognitive=43%, cyclomatic=23%, nesting=14%, length=13%, params=7%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 6. `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp:196` — `ExecuteGBuffer` (score: 65.8)
Dominant signal: **cognitive**
Tags: `long-function`, `deep-nesting`, `complex-branching`, `hard-to-follow`, `nesting-hell`
CC=15, cognitive=45, nesting=5, LOC=110, params=2, returns=1
Score breakdown: cognitive=46%, cyclomatic=19%, nesting=19%, length=12%, params=4%

Suggested approach: flatten with guard clauses / early returns; extract inner branches into helpers; extract cohesive blocks into helpers.

### 7. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp:243` — `Zenith_TerrainEditor::ApplySplatDab` (score: 63.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=35, nesting=4, LOC=93, params=5, returns=2
Score breakdown: cognitive=41%, cyclomatic=22%, nesting=16%, length=11%, params=10%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 8. `Zenith\UI\Zenith_UIText.cpp:200` — `Zenith_UIText::Render` (score: 61.6)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=22, cognitive=37, nesting=2, LOC=124, params=1, returns=2
Score breakdown: cognitive=45%, cyclomatic=30%, length=15%, nesting=8%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 9. `Zenith\Flux\Slang\Flux_ShaderCatalog.cpp:207` — `Flux_ShaderCatalog::ValidateFeatureParity` (score: 61.1)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`, `many-returns`
CC=23, cognitive=30, nesting=4, LOC=92, params=2, returns=10
Score breakdown: cognitive=37%, cyclomatic=31%, nesting=16%, length=11%, params=4%

Suggested approach: extract cohesive blocks into helpers; check whether returns can be consolidated via result-holding variable.

### 10. `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:166` — `Zenith_Vulkan_CommandBuffer::BuildDescriptorWritesForSet` (score: 60.4)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `many-params`
CC=22, cognitive=24, nesting=2, LOC=138, params=7, returns=0
Score breakdown: cyclomatic=30%, cognitive=30%, length=17%, params=14%, nesting=8%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 11. `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp:635` — `Flux_IKSolver::ConvertPositionsToRotations` (score: 60.0)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=17, cognitive=28, nesting=4, LOC=115, params=5, returns=1
Score breakdown: cognitive=35%, cyclomatic=24%, nesting=17%, length=14%, params=10%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 12. `Zenith\Flux\Slang\Flux_ShaderCatalog.cpp:132` — `Flux_ShaderCatalog::Validate` (score: 60.0)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`, `many-returns`
CC=26, cognitive=33, nesting=3, LOC=65, params=1, returns=9
Score breakdown: cognitive=41%, cyclomatic=36%, nesting=12%, length=8%, params=2%

Suggested approach: check whether returns can be consolidated via result-holding variable; break up into smaller, named steps.

### 13. `Zenith\Core\Zenith_AutomatedTest.cpp:190` — `Zenith_AutomatedTestRunner::ParseCommandLine` (score: 59.3)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=19, cognitive=39, nesting=2, LOC=89, params=2, returns=1
Score breakdown: cognitive=49%, cyclomatic=27%, length=11%, nesting=8%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 14. `Zenith\Physics\Zenith_Physics.cpp:644` — `Zenith_Physics::LockRotation` (score: 58.6)
Dominant signal: **cognitive**
Tags: `hard-to-follow`
CC=14, cognitive=37, nesting=4, LOC=56, params=4, returns=2
Score breakdown: cognitive=47%, cyclomatic=20%, nesting=17%, params=8%, length=7%

Suggested approach: break up into smaller, named steps.

### 15. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp:82` — `Zenith_TerrainEditor::ApplyHeightDab` (score: 57.9)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `many-params`
CC=20, cognitive=19, nesting=3, LOC=160, params=6, returns=2
Score breakdown: cyclomatic=29%, cognitive=25%, length=21%, nesting=13%, params=13%

Suggested approach: extract cohesive blocks into helpers; introduce a parameter object / struct.

### 16. `Zenith\Editor\Panels\Zenith_EditorPanel_TerrainEditor.cpp:49` — `RenderBrushSection` (score: 57.6)
Dominant signal: **cyclomatic**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=25, cognitive=26, nesting=3, LOC=113, params=1, returns=1
Score breakdown: cyclomatic=36%, cognitive=34%, length=15%, nesting=13%, params=2%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 17. `Zenith\Telemetry\Zenith_Telemetry.cpp:459` — `Reader::ExportJson` (score: 57.5)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`, `hard-to-follow`
CC=16, cognitive=30, nesting=3, LOC=156, params=2, returns=2
Score breakdown: cognitive=39%, cyclomatic=23%, length=20%, nesting=13%, params=4%

Suggested approach: extract cohesive blocks into helpers; break up into smaller, named steps.

### 18. `Zenith\Profiling\Zenith_Profiling.cpp:928` — `DrawGPUStrip` (score: 57.1)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=23, cognitive=31, nesting=2, LOC=79, params=3, returns=0
Score breakdown: cognitive=41%, cyclomatic=34%, length=10%, nesting=9%, params=7%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 19. `FluxCompiler\FluxCompiler.cpp:65` — `PruneStaleArtifacts` (score: 55.3)
Dominant signal: **cognitive**
Tags: `complex-branching`, `hard-to-follow`
CC=22, cognitive=31, nesting=3, LOC=66, params=1, returns=2
Score breakdown: cognitive=42%, cyclomatic=33%, nesting=14%, length=9%, params=2%

Suggested approach: break up into smaller, named steps; extract branches into named helpers or strategy objects.

### 20. `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Trees.cpp:112` — `Zenith_TerrainEditor::ApplyTreeDab` (score: 55.2)
Dominant signal: **cognitive**
Tags: `long-function`, `complex-branching`
CC=16, cognitive=24, nesting=3, LOC=135, params=5, returns=4
Score breakdown: cognitive=33%, cyclomatic=24%, length=18%, nesting=14%, params=11%

Suggested approach: extract cohesive blocks into helpers; extract branches into named helpers or strategy objects.

## Tagged files

| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `Zenith\AI\Navigation\Zenith_NavMesh.cpp` | `low-mi` | 82.5 | 139 | 235 | 5 | 930 | 37 |
| `Zenith\Profiling\Zenith_Profiling.cpp` | `low-mi`, `god-file` | 82.4 | 221 | 369 | 5 | 1356 | 75 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp` | `low-mi`, `god-file` | 77.2 | 198 | 240 | 4 | 1071 | 39 |
| `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` | `low-mi` | 76.9 | 91 | 140 | 4 | 677 | 35 |
| `Zenith\Physics\Zenith_Physics.cpp` | `low-mi`, `god-file` | 75.3 | 91 | 148 | 5 | 662 | 51 |
| `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` | `low-mi` | 75.2 | 23 | 70 | 6 | 218 | 8 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Brushes.cpp` | `low-mi` | 75.1 | 53 | 69 | 4 | 316 | 6 |
| `Zenith\Flux\Slang\Flux_ShaderCatalog.cpp` | `low-mi` | 74.3 | 51 | 70 | 4 | 248 | 10 |
| `Zenith\Core\Zenith_AutomatedTest.cpp` | `low-mi` | 73.6 | 113 | 124 | 2 | 477 | 14 |
| `Zenith\Telemetry\Zenith_Telemetry.cpp` | `low-mi` | 73.2 | 72 | 143 | 4 | 600 | 26 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` | `low-mi`, `god-file` | 73.1 | 249 | 475 | 5 | 1084 | 52 |
| `Zenith\UI\Zenith_UIText.cpp` | `low-mi` | 72.7 | 56 | 87 | 3 | 309 | 12 |
| `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` | `low-mi`, `god-file` | 72.4 | 110 | 115 | 3 | 830 | 45 |
| `Zenith\AI\Navigation\Zenith_NavMeshGenerator.cpp` | `low-mi` | 72.3 | 105 | 198 | 5 | 723 | 26 |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph_Execution.cpp` | `low-mi` | 72.0 | 50 | 102 | 5 | 236 | 15 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` | `low-mi` | 71.5 | 71 | 140 | 4 | 553 | 13 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_TerrainEditor.cpp` | `low-mi` | 71.5 | 59 | 63 | 3 | 330 | 7 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp` | `low-mi` | 71.0 | 137 | 235 | 4 | 616 | 38 |
| `FluxCompiler\FluxCompiler.cpp` | `low-mi` | 70.5 | 39 | 54 | 3 | 194 | 13 |
| `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` | `low-mi` | 70.3 | 65 | 83 | 4 | 461 | 35 |
| `Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp` | `low-mi` | 70.3 | 64 | 107 | 4 | 628 | 35 |
| `Zenith\Flux\MeshAnimation\Flux_BonePose.cpp` | `low-mi` | 70.2 | 88 | 139 | 4 | 493 | 37 |
| `Zenith\Flux\Terrain\Flux_TerrainStreamingManager.cpp` | `low-mi` | 70.1 | 153 | 266 | 4 | 880 | 40 |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | `low-mi` | 69.9 | 44 | 74 | 4 | 362 | 15 |
| `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor_Erosion.cpp` | `low-mi` | 69.8 | 38 | 61 | 3 | 270 | 4 |

## Low-cohesion classes (LCOM5 >= 0.7, top 15)

LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means methods mostly use disjoint subsets of the fields — a split-responsibilities candidate. **Caveat: header-inline only.** Method bodies that live in separate `.cpp` files are invisible at this scope, so this is a header-inline cohesion signal, not a full-class score.

| Class | File | LCOM5 | Methods | Fields |
| --- | --- | ---: | ---: | ---: |
| `Zenith_EditorAutomation` | `Zenith\Editor\Zenith_EditorAutomation.h:266` | 1.00 | 6 | 4 |
| `Flux_BindingSlot` | `Zenith\Flux\Flux_Types.h:60` | 1.00 | 3 | 3 |
| `Zenith_Profiling` | `Zenith\Profiling\Zenith_Profiling.h:82` | 0.99 | 6 | 52 |
| `Flux_TerrainStreamingManagerImpl` | `Zenith\Flux\Terrain\Flux_TerrainStreamingManagerImpl.h:226` | 0.98 | 4 | 17 |
| `Zenith_AssetRegistry` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:58` | 0.98 | 10 | 5 |
| `Zenith_TerrainEditor` | `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.h:154` | 0.98 | 19 | 48 |
| `Flux_RenderGraph` | `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:195` | 0.98 | 7 | 36 |
| `Flux_GraphicsImpl` | `Zenith\Flux\Flux_GraphicsImpl.h:18` | 0.97 | 5 | 39 |
| `Zenith_Vulkan` | `Zenith\Vulkan\Zenith_Vulkan.h:142` | 0.97 | 3 | 34 |
| `Zenith_SceneSystem` | `Zenith\ZenithECS\Zenith_SceneSystem.h:74` | 0.97 | 13 | 31 |
| `Zenith_MaterialAsset` | `Zenith\AssetHandling\Zenith_MaterialAsset.h:56` | 0.96 | 76 | 32 |
| `Flux_SkyboxImpl` | `Zenith\Flux\Skybox\Flux_SkyboxImpl.h:94` | 0.96 | 8 | 26 |
| `Zenith_UIElement` | `Zenith\UI\Zenith_UIElement.h:79` | 0.96 | 52 | 29 |
| `Flux_IBLImpl` | `Zenith\Flux\IBL\Flux_IBLImpl.h:42` | 0.96 | 3 | 24 |
| `Zenith_D3D12_MemoryManager` | `Zenith\D3D12\Zenith_D3D12_MemoryManager.h:28` | 0.95 | 46 | 11 |

## Coupling: highest fan-in (top 15)

Files included by the most other files. High fan-in = change risk.

| File | Fan-in | Fan-out | In cycle |
| --- | ---: | ---: | :---: |
| `Zenith\Core\Zenith.h` | 272 | 12 |  |
| `Zenith\Core\Zenith_Engine.h` | 139 | 1 |  |
| `Zenith\Maths\Zenith_Maths.h` | 80 | 0 |  |
| `Zenith\Collections\Zenith_Vector.h` | 78 | 3 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Disabled.h` | 65 | 0 |  |
| `Zenith\Core\Memory\Zenith_MemoryManagement_Enabled.h` | 62 | 0 |  |
| `Zenith\Flux\Flux_GraphicsImpl.h` | 61 | 4 |  |
| `Zenith\Flux\Flux.h` | 60 | 9 |  |
| `Zenith\ZenithECS\Zenith_SceneSystem.h` | 54 | 7 |  |
| `Zenith\DebugVariables\Zenith_DebugVariables.h` | 49 | 3 |  |
| `Zenith\Profiling\Zenith_Profiling.h` | 47 | 2 |  |
| `Zenith\ZenithECS\Zenith_Entity.h` | 42 | 1 |  |
| `Zenith\Flux\RenderGraph\Flux_RenderGraph.h` | 39 | 5 |  |
| `Zenith\ZenithECS\Zenith_Scene.h` | 38 | 0 |  |
| `Zenith\Flux\Flux_Buffers.h` | 37 | 2 |  |

## Near-duplicate functions (5 cluster(s))

Token-normalized Jaccard similarity over 5-gram shingles (threshold >= 85%). Identifiers and numbers are normalized, so variables can differ without breaking a match.

### Cluster 1 (5 functions)
- `Zenith\Core\Zenith_TestFramework.h:196` — `Zenith_TestAssertEq` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:224` — `Zenith_TestAssertGt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:238` — `Zenith_TestAssertLt` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:252` — `Zenith_TestAssertGe` (score 15.8, LOC 12)
- `Zenith\Core\Zenith_TestFramework.h:266` — `Zenith_TestAssertLe` (score 15.8, LOC 12)

### Cluster 2 (2 functions)
- `Zenith\Editor\Zenith_Editor_SceneOps.cpp:225` — `Zenith_Editor::HandlePendingSceneLoadDeferred` (score 37.1, LOC 84)
- `Zenith\Editor\Zenith_Editor.cpp:744` — `Zenith_Editor::HandlePendingSceneLoad` (score 36.8, LOC 101)

### Cluster 3 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1609` — `Zenith_Vulkan::ConvertToVkFormat_Colour` (score 24.8, LOC 49)
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1698` — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` (score 18.7, LOC 23)

### Cluster 4 (2 functions)
- `Zenith\Vulkan\Zenith_Vulkan_Shader.cpp:264` — `Zenith_Vulkan_Shader::MergeReflection` (score 22.8, LOC 31)
- `Zenith\D3D12\Zenith_D3D12_Pipeline.h:105` — `MergeReflection` (score 22.5, LOC 26)

### Cluster 5 (2 functions)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:32` — `IsReadAccess` (score 15.2, LOC 20)
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.cpp:53` — `IsWriteAccess` (score 15.2, LOC 20)

## Include redundancy

Per-file `#include` findings: duplicates, transitive coverage, and same-basename conflicts. Each row is a concrete cleanup candidate.

### Duplicate includes (52 total)
| File | Line | Include | First seen |
| --- | ---: | --- | ---: |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 8 | `Core/Multithreading/Zenith_Multithreading.h` | 6 |
| `Zenith\AssetHandling\Zenith_TextureAsset.cpp` | 8 | `Flux/Flux_GraphicsImpl.h` | 7 |
| `Zenith\Core\Zenith_Engine.cpp` | 69 | `Flux/Flux_GraphicsImpl.h` | 34 |
| `Zenith\Core\Zenith_Engine.cpp` | 72 | `Editor/Zenith_Editor.h` | 71 |
| `Zenith\Core\Zenith_Engine.cpp` | 75 | `Editor/Zenith_EditorAutomation.h` | 74 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp` | 21 | `Flux/Flux_GraphicsImpl.h` | 20 |
| `Zenith\Editor\Panels\Zenith_EditorPanel_Viewport.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Editor\Zenith_Editor.cpp` | 83 | `Core/Zenith_CommandLine.h` | 15 |
| `Zenith\Editor\Zenith_Editor_MaterialUI.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Editor\Zenith_Editor_Menu.cpp` | 7 | `Zenith_Editor.h` | 6 |
| `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` | 10 | `Flux/Flux_GraphicsImpl.h` | 9 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 11 | `Flux/Flux_GraphicsImpl.h` | 10 |
| `Zenith\Flux\DeferredShading\Flux_DeferredShading.cpp` | 14 | `Flux/IBL/Flux_IBLImpl.h` | 13 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 6 | `Flux/DynamicLights/Flux_LightClusteringImpl.h` | 5 |
| `Zenith\Flux\DynamicLights\Flux_LightClustering.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Flux\Flux_Graphics.cpp` | 9 | `Flux/Flux_GraphicsImpl.h` | 8 |
| `Zenith\Flux\Fog\Flux_Fog.cpp` | 7 | `Flux/Fog/Flux_FogImpl.h` | 6 |
| `Zenith\Flux\Fog\Flux_Fog.cpp` | 19 | `Flux/Flux_GraphicsImpl.h` | 18 |
| `Zenith\Flux\Fog\Flux_FroxelFog.cpp` | 7 | `Flux/Fog/Flux_FroxelFogImpl.h` | 6 |
| `Zenith\Flux\Fog\Flux_FroxelFog.cpp` | 12 | `Flux/Flux_GraphicsImpl.h` | 11 |
| `Zenith\Flux\Fog\Flux_GodRaysFog.cpp` | 6 | `Flux/Fog/Flux_GodRaysFogImpl.h` | 5 |
| `Zenith\Flux\Fog\Flux_GodRaysFog.cpp` | 10 | `Flux/Flux_GraphicsImpl.h` | 9 |
| `Zenith\Flux\Fog\Flux_RaymarchFog.cpp` | 8 | `Flux/Fog/Flux_RaymarchFogImpl.h` | 7 |
| `Zenith\Flux\Fog\Flux_RaymarchFog.cpp` | 13 | `Flux/Flux_GraphicsImpl.h` | 12 |
| `Zenith\Flux\Fog\Flux_VolumeFog.cpp` | 5 | `Flux/Fog/Flux_VolumeFogImpl.h` | 4 |

### Transitively-covered includes (207 total)
| File | Line | Include | Covered by |
| --- | ---: | --- | --- |
| `FluxCompiler\FluxCompiler.cpp` | 3 | `Core/Memory/Zenith_MemoryManagement_Disabled.h` | `Zenith\Core\Zenith.h:1` |
| `FluxCompiler\FluxCompiler.cpp` | 6 | `DataStream/Zenith_DataStream.h` | `Zenith\Flux\Slang\Flux_SlangCompiler.h:4` |
| `Tests\SentinelAI\main.cpp` | 22 | `AI/Navigation/Zenith_Pathfinding.h` | `Zenith\AI\Navigation\Zenith_NavMeshAgent.h:21` |
| `Tests\SentinelAI\main.cpp` | 28 | `Collections/Zenith_Vector.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:17` |
| `Tests\SentinelAI\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Tests\SentinelECS\main.cpp` | 39 | `ZenithECS/Zenith_Scene.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 40 | `ZenithECS/Zenith_Entity.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 41 | `ZenithECS/Zenith_Query.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\main.cpp` | 47 | `DataStream/Zenith_DataStream.h` | `Zenith\ZenithECS\Zenith_SceneSystem.h:37` |
| `Tests\SentinelECS\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Tests\SentinelPhysics\sentinel_platform.cpp` | 3 | `Windows/Multithreading/Zenith_Windows_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AI\Navigation\Zenith_Pathfinding.cpp` | 6 | `Collections/Zenith_HashMap.h` | `Zenith\Profiling\Zenith_Profiling.h:5` |
| `Zenith\Android\Callstack\Zenith_Android_Callstack.cpp` | 3 | `Zenith_Android_Callstack.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Multithreading\Zenith_Android_Multithreading.cpp` | 4 | `Zenith_Android_Multithreading.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\Android\Zenith_Android_Window.cpp` | 3 | `Zenith_Android_Window.h` | `Zenith\Core\Zenith.h:1` |
| `Zenith\AssetHandling\Zenith_AnimationAsset.cpp` | 4 | `Profiling/Zenith_Profiling.h` | `Zenith\Flux\MeshAnimation\Flux_AnimationClip.h:3` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.cpp` | 14 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:2` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.cpp` | 15 | `Profiling/Zenith_Profiling.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:2` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 5 | `Core/Multithreading/Zenith_Multithreading.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_AssetRegistry.h` | 7 | `Collections/Zenith_HashMap.h` | `Zenith\Flux\Flux_RendererImpl.h:4` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 3 | `AssetHandling/Zenith_AssetRegistry.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.cpp` | 5 | `DataStream/Zenith_DataStream.h` | `Zenith\AssetHandling\Zenith_FontAsset.h:2` |
| `Zenith\AssetHandling\Zenith_FontAsset.h` | 6 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetHandle.h:4` |
| `Zenith\AssetHandling\Zenith_MaterialAsset.h` | 6 | `Maths/Zenith_Maths.h` | `Zenith\AssetHandling\Zenith_MaterialParamTable.h:5` |
| `Zenith\AssetHandling\Zenith_MeshAsset.h` | 5 | `Collections/Zenith_Vector.h` | `Zenith\AssetHandling\Zenith_AssetRegistry.h:3` |

## External includes (115 resolved to 35 middleware/third-party header(s))

These resolve into excluded directories (Middleware/, ThirdParty/, External/, …). Listed for visibility only — not flagged as issues.

| Count | Header |
| ---: | --- |
| 46 | `imgui.h` |
| 23 | `glm/glm.hpp` |
| 7 | `vulkan/vulkan.hpp` |
| 4 | `backends/imgui_impl_vulkan.h` |
| 4 | `vma/vk_mem_alloc.h` |
| 2 | `GLFW/glfw3.h` |
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
| 1 | `AI/Zenith_AIDebugVariables.Tests.inl` |
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
| 1 | `EntityComponent/Zenith_Physics.Tests.inl` |
| 1 | `EntityComponent/Components/Zenith_GraphComponent.Tests.inl` |
| 1 | `EntityComponent/Components/Zenith_LightComponent.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_AnimationClip.Tests.inl` |
| 1 | `Flux/MeshAnimation/Flux_BlendTree.Tests.inl` |
| 1 | `Scripting/Zenith_Scripting.Tests.inl` |
| 1 | `backends/imgui_impl_glfw.h` |

## Tech-debt markers (9 total; showing first 30)

- `Zenith\Flux\Flux_RenderResources.h:73` **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- `Zenith\Flux\Flux_TransientDesc.h:12` **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- `Zenith\Flux\IBL\Flux_IBL.cpp:515` **TODO** create per-face 2D SRVs for cubemap debug display.
- `Zenith\Flux\MeshAnimation\Flux_AnimationController.cpp:173` **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- `Zenith\Flux\RenderGraph\Flux_RenderGraph.h:8` **TODO** Replace with engine hash set
- `Zenith\Flux\Shadows\Flux_Shadows.cpp:203` **TODO** Enable terrain shadow casting
- `Zenith\Flux\Slang\Flux_ShaderHotReload.cpp:15` **TODO** Replace with engine hash set
- `Zenith\Vulkan\Zenith_Vulkan.cpp:1543` **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp:445` **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that

## Directories (top 15 by worst file)

| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `Zenith\AI\Navigation` | 9 | 2,697 | 82.5 | 13.4 | 38.6 | `Zenith\AI\Navigation\Zenith_NavMesh.cpp` |
| `Zenith\Profiling` | 2 | 1,564 | 82.4 | 51.7 | 113.0 | `Zenith\Profiling\Zenith_Profiling.cpp` |
| `Zenith\Editor\TerrainEditor` | 10 | 2,547 | 77.2 | 33.5 | 36.2 | `Zenith\Editor\TerrainEditor\Zenith_TerrainEditor.cpp` |
| `Zenith\Flux\MeshAnimation` | 20 | 5,792 | 76.9 | 27.3 | 34.4 | `Zenith\Flux\MeshAnimation\Flux_InverseKinematics.cpp` |
| `Zenith\Physics` | 7 | 1,453 | 75.3 | 6.4 | 24.9 | `Zenith\Physics\Zenith_Physics.cpp` |
| `Zenith\Flux\AnimatedMeshes` | 3 | 270 | 75.2 | 3.3 | 8.3 | `Zenith\Flux\AnimatedMeshes\Flux_AnimatedMeshes.cpp` |
| `Zenith\Flux\Slang` | 12 | 1,795 | 74.3 | 5.2 | 26.6 | `Zenith\Flux\Slang\Flux_ShaderCatalog.cpp` |
| `Zenith\Core` | 40 | 3,727 | 73.6 | 3.0 | 10.4 | `Zenith\Core\Zenith_AutomatedTest.cpp` |
| `Zenith\Telemetry` | 2 | 746 | 73.2 | 41.7 | 37.0 | `Zenith\Telemetry\Zenith_Telemetry.cpp` |
| `Zenith\Flux\RenderGraph` | 5 | 2,373 | 73.1 | 62.6 | 96.6 | `Zenith\Flux\RenderGraph\Flux_RenderGraph_Compilation.cpp` |
| `Zenith\UI` | 25 | 4,080 | 72.7 | 13.7 | 22.0 | `Zenith\UI\Zenith_UIText.cpp` |
| `Zenith\Vulkan` | 25 | 6,433 | 72.4 | 17.4 | 25.0 | `Zenith\Vulkan\Zenith_Vulkan_CommandBuffer.cpp` |
| `Zenith\Editor\Panels` | 24 | 4,857 | 71.5 | 13.0 | 34.5 | `Zenith\Editor\Panels\Zenith_EditorPanel_Memory.cpp` |
| `FluxCompiler` | 1 | 194 | 70.5 | 70.5 | 39.0 | `FluxCompiler\FluxCompiler.cpp` |
| `Zenith\EntityComponent\Components` | 30 | 6,943 | 70.3 | 31.6 | 26.4 | `Zenith\EntityComponent\Components\Zenith_InstancedMeshComponent.cpp` |

## Caveats

> Function-level metrics come from regex-based C++ parsing, which is approximate: function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs may cause functions to be missed. File-level metrics are unaffected. Missing functions means they are absent from per-function lists, not that the file is simple. Architecture: module Abstractness (A) and Distance (D) are a regex proxy (pure-virtual presence per header) and are reported, never gated; a std::function/pimpl-free engine tends to show uniformly low A. Direction/encapsulation gates exclude only AMBIGUOUS basename-resolved edges (a basename shared by 2+ files); a unique basename is high-confidence and IS gated, like a relative-path match.
