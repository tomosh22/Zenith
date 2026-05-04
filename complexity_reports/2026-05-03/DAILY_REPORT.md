# Zenith Engine — Daily Code Health Snapshot
**Date:** 2026-05-03

## 1. Executive summary

The engine codebase is in moderate health: 423 engine files, ~79.9k SLOC, with a long tail of large, deeply imperative `.cpp` files concentrated in scene/entity orchestration, the render graph, navmesh generation, and Vulkan resource managers — most hotspots are dominated by cognitive load rather than dispatch/branching.

- **Parser:** tree-sitter (auto), profile **engine-ci**.
- **Include edge resolution:** 2087 relative / 20 basename (basename = 0.9% of edges) — well below the 10% threshold; cycle confidence is high.
- **Function hotspots by priority score:** **0** at ≥80, **0** at ≥70, **0** at ≥60 (worst function scores 58.9).
- **High-priority files:** 12 files at score ≥70 (P90 file priority **66.1**).
- **Worst function:** `Zenith_SceneManager::UnloadAllNonPersistent` at [EntityComponent/Zenith_SceneManager.cpp:1236](EntityComponent/Zenith_SceneManager.cpp:1236) — score 58.9.
- **Worst file:** [AI/Navigation/Zenith_NavMeshGenerator.cpp](AI/Navigation/Zenith_NavMeshGenerator.cpp) — score 75.3.
- **God files tagged `god-file`:** **23**.
- **Tech-debt markers (after hash-map placeholder filter):** 18, all `TODO` (0 FIXME, 0 HACK, 0 XXX).
- **Highest hotspot concentration:** three-way tie at the top — `AI/Navigation`, `Flux/RenderGraph`, and `EntityComponent/Components` each hold 5 of the top-50 function hotspots; `Editor/Panels` follows with 4.
- **Near-duplicate function clusters:** **4** clusters detected.
- **Include cycles:** No include cycles detected.

## 2. Top 10 refactoring targets

1. [EntityComponent/Zenith_SceneManager.cpp:1236](EntityComponent/Zenith_SceneManager.cpp:1236) — `Zenith_SceneManager::UnloadAllNonPersistent` — score **58.9**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 117 LOC body with CC 17 / cog 33 — many cohabiting concerns (filtering, ordering, dispatch, callback bookkeeping) inside one function.
   **Extract candidate line range:** none reported (bottleneck is breadth of branching, not deep blocks).
   **Suggested first move:** split into a "collect non-persistent scenes" pass and a "drive unload state machine per scene" pass — name each loop body as a private helper.

2. [Vulkan/Zenith_Vulkan_MemoryManager.cpp:1015](Vulkan/Zenith_Vulkan_MemoryManager.cpp:1015) — `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` — score **53.0**, dominant `cognitive`, tags `[long-function, hard-to-follow]`.
   **Why it hurts:** 141 LOC of resource setup; cognitive load reads as accumulated branchy configuration in one body.
   **Extract candidate line range:** none reported.
   **Suggested first move:** factor out a `Flux_TextureCreateInfo`-builder step from the actual VkImage/VMA allocation — give descriptor preparation its own named function.

3. [AI/Navigation/Zenith_NavMeshGenerator.cpp:687](AI/Navigation/Zenith_NavMeshGenerator.cpp:687) — `Zenith_NavMeshGenerator::BuildPolygonMesh` — score **52.6**, dominant `cognitive`, tags `[long-function, hard-to-follow]`.
   **Why it hurts:** 110 LOC, depth 4, cog 33 — multi-pass mesh assembly inline.
   **Extract candidate line range:** none reported.
   **Suggested first move:** name each pass (edge collection / polygon emission / merging) as a static free function; the outer routine becomes a 3-line orchestrator.

4. [Flux/Slang/Flux_SlangCompiler.cpp:708](Flux/Slang/Flux_SlangCompiler.cpp:708) — `Flux_SlangCompiler::CompileProgram` — score **52.6**, dominant `cognitive`, tags `[long-function, complex-branching, many-returns]`.
   **Why it hurts:** 95 LOC, CC 21 with multiple early-returns — long pipeline of "check error, bail out" branches.
   **Extract candidate line range:** none reported.
   **Suggested first move:** wrap each Slang stage (`session->load`, link, target gen) in a helper that returns `bool` + populated diagnostic; the outer function turns into a sequence of `if (!Step()) return false`.

5. [Windows/Zenith_Windows_FileWatcher.cpp:166](Windows/Zenith_Windows_FileWatcher.cpp:166) — `Zenith_FileWatcher::UpdatePlatform` — score **52.5**, dominant `cognitive`, tags `[long-function, complex-branching]`.
   **Why it hurts:** 137 LOC, CC 18, depth 4 — Win32 ReadDirectoryChangesW dispatch and event coalescing are inline in one body.
   **Extract candidate line range:** none reported.
   **Suggested first move:** extract `DispatchSingleNotification(...)` from the `FILE_NOTIFY_INFORMATION` walk; the outer function then just owns the polling loop.

6. [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118) — `Flux_AnimatedMeshes::ExecuteGBuffer` — score **52.4**, dominant `cognitive`, tags `[long-function, hard-to-follow]`.
   **Why it hurts:** 97 LOC at depth 4 — per-instance binding + draw recording for animated meshes inline.
   **Extract candidate line range:** none reported.
   **Suggested first move:** lift the inner per-instance loop body into a `RecordOneInstance(cmdList, instance)` helper.

7. [UI/Zenith_UICanvas.cpp:203](UI/Zenith_UICanvas.cpp:203) — `Zenith_UICanvas::Update` — score **52.0**, dominant `cognitive`, tags `[complex-branching, hard-to-follow]`.
   **Why it hurts:** Only 43 LOC but cog 45 / CC 15 — extreme density of conditionals (input + layout + dirty-tracking) per line.
   **Extract candidate line range:** none reported.
   **Suggested first move:** split into `UpdateInputState()`, `UpdateLayout()`, `UpdateDirtyChildren()` — give the cognitive load somewhere to land.

8. [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Editor/Panels/Zenith_EditorPanel_Memory.cpp:262) — `RenderCategoryTab` — score **51.7**, dominant `cognitive`, tags `[long-function, hard-to-follow]`.
   **Why it hurts:** 94 LOC at depth 4 — table-rendering branches mixed with summary aggregation.
   **Extract candidate line range:** none reported.
   **Suggested first move:** extract `BuildCategorySummary(category)` (data) and `RenderCategoryTable(summary)` (view) — split the data prep from the ImGui calls.

9. [Flux/RenderGraph/Flux_RenderGraph.cpp:1171](Flux/RenderGraph/Flux_RenderGraph.cpp:1171) — `Flux_RenderGraph::SynthesizeAliasingBarriers` — score **51.2**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 94 LOC, CC 17, cog 30 — barrier synthesis interleaves alias-set lookup, transition decision, and emit.
   **Extract candidate line range:** none reported.
   **Suggested first move:** factor out `FindAliasGroup(resource)` and `EmitTransition(from, to, resource)`; the loop body becomes 3-4 lines.

10. [Flux/MeshGeometry/Flux_MeshGeometry.cpp:297](Flux/MeshGeometry/Flux_MeshGeometry.cpp:297) — `Flux_MeshGeometry::Combine` — score **51.0**, dominant `cognitive`, tags `[long-function, complex-branching, many-returns]`.
    **Why it hurts:** 84 LOC, CC 20 — vertex/index merging logic with several "incompatible layout, bail" paths.
    **Extract candidate line range:** none reported.
    **Suggested first move:** push compatibility checks into a `CanCombine(a, b)` predicate at the top, then the body assumes a happy path.

## 3. Tag distribution (what shape is the problem?)

Across the top-50 function hotspots:

- `long-function`: 28
- `complex-branching`: 23
- `hard-to-follow`: 23
- `deep-nesting`: 3
- `nesting-hell`: 3
- `many-returns`: 2
- `many-params`: 2
- `very-long-function`: 1
- `big-switch`: 1

Dominant signal across the same 50: **cognitive 45**, **cyclomatic 5**. There are no nesting-, length-, or params-dominant entries in the top 50.

**Interpretation:** the engine's pattern is long, dense imperative `.cpp` bodies — not deeply nested, not dispatch-table-shaped. Cognitive load (interleaved concerns, many short branches) outweighs raw cyclomatic and nesting concerns. Refactoring will pay off most by extracting cohesive sub-steps and naming them, rather than flattening nesting or replacing switches with polymorphism.

## 4. God files

| Path | LOC | Functions | fan_in | fan_out | Worst func score | Priority |
|------|----:|----------:|-------:|--------:|-----------------:|---------:|
| EntityComponent/Zenith_SceneManager.cpp | 907 | 131 | 0 | 26 | 58.9 | 73.7 |
| EntityComponent/Zenith_SceneData.cpp | 737 | 45 | 0 | 12 | 48.4 | 72.3 |
| Flux/RenderGraph/Flux_RenderGraph.cpp | 1475 | 91 | 0 | 7 | 51.2 | 71.4 |
| Vulkan/Zenith_Vulkan_MemoryManager.cpp | 1594 | 71 | 0 | 11 | 53.0 | 70.2 |
| AI/Squad/Zenith_Squad.cpp | 788 | 59 | 0 | 9 | 49.6 | 69.1 |
| Editor/Zenith_Editor.cpp | 1247 | 52 | 0 | 39 | 48.1 | 68.7 |
| Physics/Zenith_Physics.cpp | 580 | 41 | 0 | 12 | 46.8 | 68.3 |
| Vulkan/Zenith_Vulkan_CommandBuffer.cpp | 772 | 44 | 0 | 10 | 43.9 | 67.4 |
| Vulkan/Zenith_Vulkan_Pipeline.cpp | 641 | 42 | 0 | 7 | 38.6 | 65.8 |
| Vulkan/Zenith_Vulkan.cpp | 1168 | 59 | 0 | 17 | 35.7 | 65.0 |
| Flux/MeshAnimation/Flux_AnimationStateMachine.cpp | 806 | 63 | 0 | 4 | 35.0 | 64.8 |
| Flux/MeshAnimation/Flux_BlendTree.cpp | 662 | 66 | 0 | 4 | 34.5 | 64.6 |
| EntityComponent/Components/Zenith_AnimatorComponent.cpp | 513 | 46 | 0 | 11 | 31.1 | 63.6 |
| Flux/MeshAnimation/Flux_AnimationClip.cpp | 592 | 46 | 0 | 6 | 25.3 | 61.8 |
| Core/Memory/Zenith_MemoryManagement.cpp | 470 | 41 | 0 | 7 | 30.3 | 61.6 |
| Collections/Zenith_HashMap.h | 525 | 41 | 4 | 3 | 23.8 | 61.4 |
| EntityComponent/Internal/Zenith_SceneCallbackBus.cpp | 311 | 46 | 0 | 4 | 22.3 | 60.9 |
| Collections/Zenith_Vector.h | 397 | 41 | 46 | 3 | 20.9 | 58.8 |
| EntityComponent/Zenith_SceneData.h | 533 | 63 | 40 | 7 | 19.1 | 58.2 |
| Editor/Zenith_EditorAutomation.cpp | 1155 | 107 | 0 | 18 | 44.5 | 45.7 |
| UI/Zenith_UIElement.h | 196 | 53 | 11 | 4 | 14.9 | 29.6 |
| Flux/MeshAnimation/Flux_BlendTree.h | 237 | 42 | 3 | 3 | 11.7 | 11.7 |
| UI/Zenith_UIButton.h | 112 | 43 | 5 | 4 | 5.0 | 10.8 |

## 5. Directory risk concentration

Top directories by count of entries in the top-50 `function_hotspots`:

| Directory | Hotspot count | Avg score |
|-----------|--------------:|----------:|
| AI/Navigation | 5 | 46.5 |
| Flux/RenderGraph | 5 | 46.3 |
| EntityComponent/Components | 5 | 44.9 |
| Editor/Panels | 4 | 46.4 |
| Flux/MeshGeometry | 3 | 46.9 |
| EntityComponent/Internal | 3 | 47.5 |
| Flux/MeshAnimation | 3 | 45.3 |
| Flux/Slang | 2 | 48.2 |
| Flux/Terrain | 2 | 46.0 |
| AI/Squad | 1 | 49.6 |
| Flux/AnimatedMeshes | 1 | 52.4 |
| Vulkan (Vulkan_CommandBuffer) | 1 | 43.9 |

(Top-level files like `Zenith_SceneManager.cpp` and `Zenith_Vulkan_MemoryManager.cpp` each contributed one further hotspot from outside the per-subdirectory tallies above.)

## 6. Near-duplicate function clusters

4 clusters detected (Jaccard ≥ 85% on token-normalised 5-gram shingles).

- **Cluster 1** (5 members):
  - [Core/Zenith_TestFramework.h:185](Core/Zenith_TestFramework.h:185) — `Zenith_TestAssertEq` — score 15.8
  - [Core/Zenith_TestFramework.h:213](Core/Zenith_TestFramework.h:213) — `Zenith_TestAssertGt` — score 15.8
  - [Core/Zenith_TestFramework.h:227](Core/Zenith_TestFramework.h:227) — `Zenith_TestAssertLt` — score 15.8
  - [Core/Zenith_TestFramework.h:241](Core/Zenith_TestFramework.h:241) — `Zenith_TestAssertGe` — score 15.8
  - [Core/Zenith_TestFramework.h:255](Core/Zenith_TestFramework.h:255) — `Zenith_TestAssertLe` — score 15.8
  - **Suggested action:** collapse the five comparison-assert overloads into a single template parameterised on a comparator (or a macro that takes the operator), so each helper becomes a one-liner forwarder.

- **Cluster 2** (2 members):
  - [Vulkan/Zenith_Vulkan.cpp:1446](Vulkan/Zenith_Vulkan.cpp:1446) — `Zenith_Vulkan::ConvertToVkFormat_Colour` — score 24.8
  - [Vulkan/Zenith_Vulkan.cpp:1535](Vulkan/Zenith_Vulkan.cpp:1535) — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` — score 18.7
  - **Suggested action:** unify the two format-conversion switches behind a single static lookup table indexed by enum; both functions become one `Map(...)` taking the enum class.

- **Cluster 3** (2 members):
  - [Core/Zenith_Tween.cpp:39](Core/Zenith_Tween.cpp:39) — `Zenith_GetEasingTypeName` — score 23.1
  - [Flux/Slang/Flux_CodeGenerator.cpp:64](Flux/Slang/Flux_CodeGenerator.cpp:64) — `ResourceKindName` — score 16.8
  - **Suggested action:** these are both enum-to-cstring maps with identical shape — the duplicate is "shape" not "data", so a small `ENUM_TO_STRING` macro/inline template would replace both bodies.

- **Cluster 4** (2 members):
  - [Flux/RenderGraph/Flux_RenderGraph.cpp:35](Flux/RenderGraph/Flux_RenderGraph.cpp:35) — `IsReadAccess` — score 15.2
  - [Flux/RenderGraph/Flux_RenderGraph.cpp:56](Flux/RenderGraph/Flux_RenderGraph.cpp:56) — `IsWriteAccess` — score 15.2
  - **Suggested action:** both are bitmask-membership tests against `RESOURCE_ACCESS_*` enums; replace with a single `HasAnyAccess(access, mask)` helper plus two named masks.

## 7. Include coupling & cycles

**Edge confidence:** 2087 relative-resolved edges vs 20 basename-resolved (0.9% basename — well below the 10% threshold). Cycle confidence is high.

**Cycles:** No include cycles detected.

**High fan-in headers (top 5):**

| File | fan_in | fan_out |
|------|------:|-------:|
| Core/Zenith.h | 197 | 11 |
| Flux/Flux.h | 72 | 6 |
| Core/Memory/Zenith_MemoryManagement_Disabled.h | 66 | 0 |
| DataStream/Zenith_DataStream.h | 64 | 1 |
| Core/Memory/Zenith_MemoryManagement_Enabled.h | 61 | 0 |

**High fan-out files (top 5):**

| File | fan_in | fan_out |
|------|------:|-------:|
| Editor/Zenith_Editor.cpp | 0 | 39 |
| Flux/Flux.cpp | 0 | 31 |
| Core/Zenith_Core.cpp | 0 | 26 |
| EntityComponent/Zenith_SceneManager.cpp | 0 | 26 |
| Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp | 0 | 22 |

## 8. Low-cohesion classes

**Caveat:** every record below is **header-inline only** — LCOM5 is computed from inline-defined methods only, not the full class. Read these as a "header surface mismatch" signal rather than full-class cohesion.

| Class | File:line | LCOM5 | Methods | Fields |
|-------|-----------|------:|--------:|-------:|
| Zenith_Vulkan | [Vulkan/Zenith_Vulkan.h:103](Vulkan/Zenith_Vulkan.h:103) | 1.000 | 12 | 4 |
| Zenith_Vulkan_MemoryManager | [Vulkan/Zenith_Vulkan_MemoryManager.h:34](Vulkan/Zenith_Vulkan_MemoryManager.h:34) | 1.000 | 12 | 31 |
| Flux_Gizmos | [Flux/Gizmos/Flux_Gizmos.h:36](Flux/Gizmos/Flux_Gizmos.h:36) | 1.000 | 4 | 5 |
| Flux_TerrainStreamingManager | [Flux/Terrain/Flux_TerrainStreamingManager.h:183](Flux/Terrain/Flux_TerrainStreamingManager.h:183) | 1.000 | 4 | 12 |
| Zenith_SceneOperationQueue | [EntityComponent/Internal/Zenith_SceneOperationQueue.h:36](EntityComponent/Internal/Zenith_SceneOperationQueue.h:36) | 1.000 | 3 | 25 |
| Zenith_AssetRegistry | [AssetHandling/Zenith_AssetRegistry.h:81](AssetHandling/Zenith_AssetRegistry.h:81) | 0.980 | 10 | 5 |
| Flux_RenderGraph | [Flux/RenderGraph/Flux_RenderGraph.h:294](Flux/RenderGraph/Flux_RenderGraph.h:294) | 0.976 | 6 | 35 |
| Zenith_Profiling | [Profiling/Zenith_Profiling.h:196](Profiling/Zenith_Profiling.h:196) | 0.970 | 3 | 11 |
| Flux_RenderGraph_Pass | [Flux/RenderGraph/Flux_RenderGraph.h:131](Flux/RenderGraph/Flux_RenderGraph.h:131) | 0.963 | 3 | 18 |
| Zenith_SceneData | [EntityComponent/Zenith_SceneData.h:51](EntityComponent/Zenith_SceneData.h:51) | 0.962 | 51 | 36 |

## 9. Tech-debt markers

After filtering hash-map placeholders matching `Replace with engine (hash map|hash set|container)`: **18** markers remain (15 filtered out of 33 total).

**Counts by marker type:**
- `TODO`: 18
- `FIXME`: 0
- `HACK`: 0
- `XXX`: 0

**All kept markers (severity-sorted, then file/line):**

- [Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor/Zenith_UndoSystem.cpp:352](Editor/Zenith_UndoSystem.cpp:352) **TODO** Serialize full entity state (all components)
- [Editor/Zenith_UndoSystem.cpp:404](Editor/Zenith_UndoSystem.cpp:404) **TODO** Deserialize full entity state
- [EntityComponent/Zenith_SceneData.cpp:179](EntityComponent/Zenith_SceneData.cpp:179) **TODO** auto-promote same-scene self-unload to async
- [EntityComponent/Zenith_SceneManager.cpp:1008](EntityComponent/Zenith_SceneManager.cpp:1008) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting
- [Flux/Flux.h:105](Flux/Flux.h:105) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Flux/Flux.h:114](Flux/Flux.h:114) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Flux/Flux_CommandList.h:89](Flux/Flux_CommandList.h:89) **TODO** elide second memcpy by having AddCommand record directly into the …
- [Flux/IBL/Flux_IBL.cpp:543](Flux/IBL/Flux_IBL.cpp:543) **TODO** create per-face 2D SRVs for cubemap debug display
- [Flux/MeshAnimation/Flux_AnimationController.cpp:178](Flux/MeshAnimation/Flux_AnimationController.cpp:178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- [Flux/MeshAnimation/Flux_AnimationController.h:293](Flux/MeshAnimation/Flux_AnimationController.h:293) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Flux/RenderGraph/Flux_RenderGraph.cpp:1792](Flux/RenderGraph/Flux_RenderGraph.cpp:1792) **TODO** Replace std::unordered_map with engine hash map
- [Flux/RenderGraph/Flux_RenderGraph.h:512](Flux/RenderGraph/Flux_RenderGraph.h:512) **TODO** Replace std::unordered_map with engine hash map
- [Flux/RenderGraph/Flux_RenderGraph.h:561](Flux/RenderGraph/Flux_RenderGraph.h:561) **TODO** Replace std::unordered_map with engine hash map
- [Flux/Shadows/Flux_Shadows.cpp:154](Flux/Shadows/Flux_Shadows.cpp:154) **TODO** Enable terrain shadow casting
- [Vulkan/Zenith_Vulkan.cpp:1388](Vulkan/Zenith_Vulkan.cpp:1388) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker …
- [Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411](Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411) **TODO** expose DONT_CARE LoadOp as a third option
- [Vulkan/Zenith_Vulkan_MemoryManager.cpp:295](Vulkan/Zenith_Vulkan_MemoryManager.cpp:295) **TODO** Replace std::unordered_map with engine hash map

## 10. Headline numbers

`Files 423 · SLOC 79,916 · functions 5,247 · avg MI 30.5 · min MI 0.0 · avg CC 20.15 · max CC 289 · est bugs 1151.49`

## 11. Appendix — top-20 hotspots

| # | File:line | Name | Score | Dominant | Tags | CC | Cog | Nest | LOC |
|--:|-----------|------|------:|----------|------|---:|----:|-----:|----:|
| 1 | [EntityComponent/Zenith_SceneManager.cpp:1236](EntityComponent/Zenith_SceneManager.cpp:1236) | Zenith_SceneManager::UnloadAllNonPersistent | 58.9 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 33 | 4 | 117 |
| 2 | [Vulkan/Zenith_Vulkan_MemoryManager.cpp:1015](Vulkan/Zenith_Vulkan_MemoryManager.cpp:1015) | Zenith_Vulkan_MemoryManager::CreateTextureVRAM | 53.0 | cognitive | long-function, hard-to-follow | 14 | 26 | 3 | 141 |
| 3 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:687](AI/Navigation/Zenith_NavMeshGenerator.cpp:687) | Zenith_NavMeshGenerator::BuildPolygonMesh | 52.6 | cognitive | long-function, hard-to-follow | 10 | 33 | 4 | 110 |
| 4 | [Flux/Slang/Flux_SlangCompiler.cpp:708](Flux/Slang/Flux_SlangCompiler.cpp:708) | Flux_SlangCompiler::CompileProgram | 52.6 | cognitive | long-function, complex-branching, many-returns | 21 | 24 | 3 | 95 |
| 5 | [Windows/Zenith_Windows_FileWatcher.cpp:166](Windows/Zenith_Windows_FileWatcher.cpp:166) | Zenith_FileWatcher::UpdatePlatform | 52.5 | cognitive | long-function, complex-branching | 18 | 23 | 4 | 137 |
| 6 | [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118) | Flux_AnimatedMeshes::ExecuteGBuffer | 52.4 | cognitive | long-function, hard-to-follow | 13 | 29 | 4 | 97 |
| 7 | [UI/Zenith_UICanvas.cpp:203](UI/Zenith_UICanvas.cpp:203) | Zenith_UICanvas::Update | 52.0 | cognitive | complex-branching, hard-to-follow | 15 | 45 | 2 | 43 |
| 8 | [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Editor/Panels/Zenith_EditorPanel_Memory.cpp:262) | RenderCategoryTab | 51.7 | cognitive | long-function, hard-to-follow | 11 | 34 | 4 | 94 |
| 9 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1171](Flux/RenderGraph/Flux_RenderGraph.cpp:1171) | Flux_RenderGraph::SynthesizeAliasingBarriers | 51.2 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 94 |
| 10 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:297](Flux/MeshGeometry/Flux_MeshGeometry.cpp:297) | Flux_MeshGeometry::Combine | 51.0 | cognitive | long-function, complex-branching, many-returns | 20 | 24 | 3 | 84 |
| 11 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844](EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844) | Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads | 50.0 | cognitive | long-function, hard-to-follow | 13 | 25 | 3 | 172 |
| 12 | [AI/Squad/Zenith_Squad.cpp:719](AI/Squad/Zenith_Squad.cpp:719) | Zenith_Squad::AssignFormationSlots | 49.6 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 28 | 3 | 81 |
| 13 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1588](Flux/RenderGraph/Flux_RenderGraph.cpp:1588) | Flux_RenderGraph::AssignClearFlags | 49.5 | cognitive | complex-branching, hard-to-follow | 18 | 32 | 3 | 40 |
| 14 | [EntityComponent/Zenith_SceneData.cpp:814](EntityComponent/Zenith_SceneData.cpp:814) | Zenith_SceneData::DispatchAwakeForNewScene | 48.4 | cognitive | long-function, deep-nesting, hard-to-follow, nesting-hell | 11 | 26 | 5 | 97 |
| 15 | [Editor/Zenith_Editor.cpp:368](Editor/Zenith_Editor.cpp:368) | Zenith_Editor::Update | 48.1 | cognitive | long-function, complex-branching, hard-to-follow | 16 | 25 | 3 | 114 |
| 16 | [EntityComponent/Components/Zenith_TerrainComponent.cpp:551](EntityComponent/Components/Zenith_TerrainComponent.cpp:551) | Zenith_TerrainComponent::LoadAndCombineLowLODChunks | 47.7 | cognitive | long-function | 12 | 22 | 3 | 116 |
| 17 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:311](EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:311) | Zenith_SceneOperationQueue::ProcessPendingAsyncLoads | 47.7 | cognitive | long-function, hard-to-follow | 13 | 30 | 3 | 91 |
| 18 | [Flux/MeshAnimation/Flux_BonePose.cpp:199](Flux/MeshAnimation/Flux_BonePose.cpp:199) | Flux_SkeletonPose::SampleFromClip | 47.3 | cognitive | hard-to-follow | 11 | 29 | 3 | 68 |
| 19 | [Editor/Zenith_Editor_Menu.cpp:50](Editor/Zenith_Editor_Menu.cpp:50) | Zenith_Editor::RenderFileMenu | 47.1 | cognitive | long-function, hard-to-follow | 12 | 27 | 4 | 91 |
| 20 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:419](Flux/MeshGeometry/Flux_MeshGeometry.cpp:419) | Flux_MeshGeometry::GenerateLayoutAndVertexData | 47.0 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 25 | 2 | 110 |

---

*Note: today's `DAILY_REPORT.md` was overwritten — an earlier version existed in this directory.*
