# Zenith Engine — Daily Code Health Snapshot
**Date:** 2026-04-20
**Scope:** `Zenith/` only · 409 files analysed
*Note: an existing `DAILY_REPORT.md` for today was overwritten.*

---

## 1. Executive summary

- **Overall rating:** **Poor (difficult to maintain)** — average maintainability index of 30.1 across the engine; several core files score 0.0, meaning the analyser flags them as effectively unreadable without context.
- **Function hotspots:** **0** at priority ≥80 · **0** at priority ≥70 · **2** at priority ≥60.
- **God files:** **22** files tagged `god-file`.
- **Tech-debt markers (filtered):** **20** entries — all **TODO**, 0 FIXME, 0 HACK, 0 XXX. (28 hash-map / hash-set / engine-container placeholder TODOs were filtered out per project policy.)
- **Highest hotspot concentration:** `AI/Navigation` with 7 hotspots (avg score 52.5), driven by NavMesh generation and pathfinding code.

---

## 2. Top 10 refactoring targets

1. [Flux\RenderGraph\Flux_RenderGraph.cpp:1656](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1656) — `Flux_RenderGraph::SynthesizeBarriers` — score **60.4**, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 158 LOC of barrier-synthesis branching with 10 return points — the function is doing several distinct sub-tasks (resource scan, transition decision, queue ownership) inline.
   **Suggested first move:** extract the per-resource transition decision into a helper returning a `BarrierDecision` value, then collapse early-exits into a single result.

2. [Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp:234](Zenith/Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:234) — `Zenith_EditorPanelContentBrowser::RenderItemContextMenu` — score **60.2**, tags `[complex-branching, hard-to-follow]`.
   **Why it hurts:** cog 39 inside a 4-deep ImGui menu with intermixed type checks and action dispatch — readers must hold the menu shape in their head while parsing the actions.
   **Suggested first move:** split per-asset-type sub-menus into named helpers (`RenderMaterialContextMenu`, `RenderMeshContextMenu`, ...) and dispatch by asset type.

3. [Flux\RenderGraph\Flux_RenderGraph_Execution.cpp:247](Zenith/Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247) — `Flux_RenderGraph::AssertBoundResourceDeclared` — score **59.2**, tags `[complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** validation logic with cog 36 and many early returns — assertions interleaved with traversal make the success path hard to read.
   **Suggested first move:** flatten with guard clauses; pull each "what kind of resource is this?" branch into a small validation helper that returns a result enum.

4. [Flux\MeshGeometry\Flux_MeshGeometry.cpp:274](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp:274) — `Flux_MeshGeometry::Combine` — score **58.9**, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 140 LOC merging vertex/index/material data with 10 returns — a long sequence of "if this attribute exists, copy it" stanzas.
   **Suggested first move:** extract per-attribute-stream copy helpers (positions, normals, UVs, weights) so `Combine` becomes a list of named operations.

5. [EntityComponent\Zenith_SceneManager.cpp:510](Zenith/EntityComponent/Zenith_SceneManager.cpp:510) — `Zenith_SceneManager::LoadScene` — score **58.2**, tags `[very-long-function, complex-branching, many-returns]`.
   **Why it hurts:** 253 LOC orchestrating slot allocation, async dispatch, asset wiring, and post-load events — five logical phases jammed into one body.
   **Suggested first move:** carve out the phases into private methods (`AllocateSceneSlot`, `BeginAsyncLoad`, `LinkAssetReferences`, `FireOnLoaded`); `LoadScene` becomes the orchestration shell.

6. [AI\Navigation\Zenith_NavMeshGenerator.cpp:567](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:567) — `Zenith_NavMeshGenerator::CollectRegionContour` — score **58.1**, tags `[deep-nesting, hard-to-follow, nesting-hell]`.
   **Why it hurts:** 5 levels of nesting in a 52-line contour walk — the loop-inside-loop-inside-conditional structure makes the geometric intent invisible.
   **Suggested first move:** extract the inner edge-classification block, then flatten with `continue` early-outs in the outer scan loop.

7. [AI\Navigation\Zenith_NavMeshGenerator.cpp:831](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:831) — `Zenith_NavMeshGenerator::AddSpan` — score **58.1**, tags `[deep-nesting, hard-to-follow, nesting-hell]`.
   **Extract candidate:** lines **882–896** (peak depth 5, 15 lines).
   **Suggested first move:** lift the 882–896 block (the inner span-merge case) into a `MergeOverlappingSpan` helper; the outer body becomes a flat dispatch.

8. [Flux\Gizmos\Flux_Gizmos.cpp:604](Zenith/Flux/Gizmos/Flux_Gizmos.cpp:604) — `Flux_Gizmos::RaycastGizmo` — score **57.5**, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 81 LOC, CC 23 picking against per-axis translate/rotate/scale handles in one function.
   **Suggested first move:** extract per-handle-type intersect helpers (`IntersectTranslateAxis`, `IntersectRotateRing`, `IntersectScaleHandle`); main function reduces to "pick best hit".

9. [Flux\InstancedMeshes\Flux_InstancedMeshes.cpp:308](Zenith/Flux/InstancedMeshes/Flux_InstancedMeshes.cpp:308) — `Flux_InstancedMeshes::ExecuteGBuffer` — score **57.4**, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 143 LOC GBuffer pass mixing per-instance-batch culling, descriptor binding, and draw issuance.
   **Suggested first move:** extract `BindBatchDescriptors` and `IssueBatchDraw`; `ExecuteGBuffer` becomes a loop over batches.

10. [Vulkan\Zenith_Vulkan_Pipeline.cpp:536](Zenith/Vulkan/Zenith_Vulkan_Pipeline.cpp:536) — `Zenith_Vulkan_PipelineBuilder::FromSpecification` — score **57.3**, tags `[very-long-function, complex-branching]`.
    **Why it hurts:** 208 LOC translating a `Flux_PipelineSpec` into a `VkPipeline` — every Vk substruct (rasterizer, blend, depth, dynamic, vertex input) is built inline.
    **Suggested first move:** extract one builder per Vk substruct (`BuildRasterizationState`, `BuildColorBlendState`, ...) returning by value into local Vk structs; main body becomes assembly.

---

## 3. Tag distribution (what shape is the problem?)

Across the top-50 hotspots:

- `hard-to-follow`: **36**
- `long-function`: **33**
- `complex-branching`: **31**
- `many-returns`: **6**
- `deep-nesting`: **6**
- `nesting-hell`: **6**
- `many-params`: **3**
- `very-long-function`: **2**

**Interpretation:** the dominant pattern is *long imperative functions with thick branching* (long-function + complex-branching + hard-to-follow co-occur on most entries). True nesting hell is concentrated in AI/Navigation and a few editor panels. Almost no big-switch / dispatch-table candidates surfaced, and parameter-bloat is rare — refactoring effort should target *extract-method on long bodies* rather than introducing polymorphism or parameter objects.

---

## 4. God files

| Path | LOC | Funcs | Worst func score | Priority |
|------|-----|-------|------------------|---------:|
| [EntityComponent\Zenith_SceneData.cpp](Zenith/EntityComponent/Zenith_SceneData.cpp) | 939 | 45 | 55.4 | 74.4 |
| [Flux\RenderGraph\Flux_RenderGraph.cpp](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp) | 1352 | 64 | 60.4 | 74.1 |
| [EntityComponent\Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 1361 | 68 | 58.2 | 73.5 |
| [AI\Squad\Zenith_Squad.cpp](Zenith/AI/Squad/Zenith_Squad.cpp) | 786 | 56 | 49.6 | 69.1 |
| [Vulkan\Zenith_Vulkan_MemoryManager.cpp](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 1505 | 68 | 48.0 | 68.7 |
| [Editor\Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 1264 | 52 | 47.5 | 68.5 |
| [Vulkan\Zenith_Vulkan_CommandBuffer.cpp](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 733 | 44 | 40.1 | 66.3 |
| [UnitTests\Zenith_SceneTests.cpp](Zenith/UnitTests/Zenith_SceneTests.cpp) | 9224 | 488 | 35.9 | 65.0 |
| [Vulkan\Zenith_Vulkan.cpp](Zenith/Vulkan/Zenith_Vulkan.cpp) | 1167 | 51 | 35.8 | 65.0 |
| [Flux\MeshAnimation\Flux_AnimationStateMachine.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 806 | 59 | 35.0 | 64.8 |
| [Flux\MeshAnimation\Flux_BlendTree.cpp](Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp) | 667 | 65 | 34.6 | 64.6 |
| [EntityComponent\Components\Zenith_AnimatorComponent.cpp](Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 509 | 44 | 31.2 | 63.6 |
| [UnitTests\Zenith_UnitTests.cpp](Zenith/UnitTests/Zenith_UnitTests.cpp) | 7743 | 269 | 29.0 | 63.0 |
| [Flux\MeshAnimation\Flux_AnimationClip.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp) | 596 | 42 | 20.7 | 60.5 |
| [EntityComponent\Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) | 656 | 74 | 19.2 | 58.3 |
| [UnitTests\Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp) | 1973 | 87 | 23.0 | 57.6 |
| [UnitTests\Zenith_AITests.cpp](Zenith/UnitTests/Zenith_AITests.cpp) | 1835 | 94 | 20.7 | 55.2 |
| [EntityComponent\Components\Zenith_ScriptComponent.h](Zenith/EntityComponent/Components/Zenith_ScriptComponent.h) | 196 | 50 | 14.4 | 50.2 |
| [Editor\Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) | 1501 | 85 | 44.5 | 45.7 |
| [UI\Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h) | 195 | 50 | 15.0 | 29.6 |
| [UnitTests\Zenith_PhysicsTests.cpp](Zenith/UnitTests/Zenith_PhysicsTests.cpp) | 882 | 52 | 12.6 | 26.7 |
| [UnitTests\Zenith_EditorTests.cpp](Zenith/UnitTests/Zenith_EditorTests.cpp) | 1268 | 98 | 12.1 | 16.4 |

---

## 5. Directory risk concentration

Top directories ranked by count of `function_hotspots` entries:

| Directory | Hotspot count | Avg score |
|-----------|--------------:|----------:|
| AI/Navigation | 7 | 52.5 |
| Editor/Panels | 6 | 52.5 |
| Flux/RenderGraph | 5 | 53.0 |
| EntityComponent | 4 | 52.8 |
| EntityComponent/Components | 4 | 54.6 |
| UI | 3 | 49.4 |
| Editor | 3 | 49.7 |
| Flux/MeshGeometry | 2 | 53.0 |
| Vulkan | 2 | 52.6 |
| Flux/MeshAnimation | 2 | 46.1 |

---

## 6. Tech-debt markers

After filtering hash-map / hash-set / engine-container placeholders (28 entries removed):

- **TODO:** 20
- **FIXME:** 0
- **HACK:** 0
- **XXX:** 0

Markers ordered by severity then by file/line:

- [Editor\Panels\Zenith_EditorPanel_MaterialEditor.cpp:162](Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor\Zenith_UndoSystem.cpp:343](Zenith/Editor/Zenith_UndoSystem.cpp:343) **TODO** Serialize full entity state (all components)
- [Editor\Zenith_UndoSystem.cpp:391](Zenith/Editor/Zenith_UndoSystem.cpp:391) **TODO** Deserialize full entity state
- [EntityComponent\Components\Zenith_ModelComponent.cpp:454](Zenith/EntityComponent/Components/Zenith_ModelComponent.cpp:454) **TODO** Flux_MeshInstance needs to provide access to geometry or position data
- [EntityComponent\Zenith_SceneData.cpp:103](Zenith/EntityComponent/Zenith_SceneData.cpp:103) **TODO** auto-promote same-scene self-unload to async
- [EntityComponent\Zenith_SceneManager.cpp:1248](Zenith/EntityComponent/Zenith_SceneManager.cpp:1248) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting
- [EntityComponent\Zenith_SceneManager.cpp:1713](Zenith/EntityComponent/Zenith_SceneManager.cpp:1713) **TODO** Use scaled time when timeScale system is implemented
- [Flux\Flux.h:156](Zenith/Flux/Flux.h:156) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Flux\Flux.h:165](Zenith/Flux/Flux.h:165) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Flux\Flux_CommandList.h:88](Zenith/Flux/Flux_CommandList.h:88) **TODO** elide second memcpy by having AddCommand record directly into the buffer
- [Flux\IBL\Flux_IBL.cpp:546](Zenith/Flux/IBL/Flux_IBL.cpp:546) **TODO** create per-face 2D SRVs for cubemap debug display
- [Flux\MeshAnimation\Flux_AnimationController.cpp:178](Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED
- [Flux\Shadows\Flux_Shadows.cpp:155](Zenith/Flux/Shadows/Flux_Shadows.cpp:155) **TODO** Enable terrain shadow casting
- [Flux\StaticMeshes\Flux_StaticMeshes.cpp:226](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:226) **TODO** these 2 should probably be separate components
- [UnitTests\Zenith_AITests.cpp:2446](Zenith/UnitTests/Zenith_AITests.cpp:2446) **TODO** Re-enable once CountWalkableSpans/WalkableSpanStats are restored to NavMeshGenerator
- [UnitTests\Zenith_AITests.cpp:2452](Zenith/UnitTests/Zenith_AITests.cpp:2452) **TODO** Re-enable once HasSufficientClearance is restored to NavMeshGenerator
- [UnitTests\Zenith_AITests.cpp:2458](Zenith/UnitTests/Zenith_AITests.cpp:2458) **TODO** Re-enable once MergeOverlappingSpans is restored to NavMeshGenerator
- [Vulkan\Zenith_Vulkan.cpp:1365](Zenith/Vulkan/Zenith_Vulkan.cpp:1365) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker pool is exhausted
- [Vulkan\Zenith_Vulkan_CommandBuffer.cpp:389](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:389) **TODO** expose DONT_CARE LoadOp as a third option

---

## 7. Headline numbers

`Files: 409 · SLOC: 99,413 · Functions: 5,407 · Avg MI: 30.1 · Min MI: 0.0 · Avg CC: 20.8 · Max CC: 249 · Est bugs: 1505.5`

---

## 8. Appendix — top-20 hotspots

| # | File:line | Name | Score | Tags | CC | Cog | Nest | LOC |
|---|-----------|------|------:|------|---:|----:|-----:|----:|
| 1 | [Flux\RenderGraph\Flux_RenderGraph.cpp:1656](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1656) | Flux_RenderGraph::SynthesizeBarriers | 60.4 | long-function, complex-branching, hard-to-follow, many-returns | 24 | 28 | 3 | 158 |
| 2 | [Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp:234](Zenith/Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:234) | Zenith_EditorPanelContentBrowser::RenderItemContextMenu | 60.2 | complex-branching, hard-to-follow | 15 | 39 | 4 | 80 |
| 3 | [Flux\RenderGraph\Flux_RenderGraph_Execution.cpp:247](Zenith/Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247) | Flux_RenderGraph::AssertBoundResourceDeclared | 59.2 | complex-branching, hard-to-follow, many-returns | 18 | 36 | 3 | 80 |
| 4 | [Flux\MeshGeometry\Flux_MeshGeometry.cpp:274](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp:274) | Flux_MeshGeometry::Combine | 58.9 | long-function, complex-branching, hard-to-follow, many-returns | 20 | 29 | 3 | 140 |
| 5 | [EntityComponent\Zenith_SceneManager.cpp:510](Zenith/EntityComponent/Zenith_SceneManager.cpp:510) | Zenith_SceneManager::LoadScene | 58.2 | very-long-function, complex-branching, many-returns | 20 | 22 | 3 | 253 |
| 6 | [AI\Navigation\Zenith_NavMeshGenerator.cpp:567](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:567) | Zenith_NavMeshGenerator::CollectRegionContour | 58.1 | deep-nesting, hard-to-follow, nesting-hell | 14 | 35 | 5 | 52 |
| 7 | [AI\Navigation\Zenith_NavMeshGenerator.cpp:831](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:831) | Zenith_NavMeshGenerator::AddSpan | 58.1 | deep-nesting, hard-to-follow, nesting-hell | 14 | 31 | 5 | 76 |
| 8 | [Flux\Gizmos\Flux_Gizmos.cpp:604](Zenith/Flux/Gizmos/Flux_Gizmos.cpp:604) | Flux_Gizmos::RaycastGizmo | 57.5 | long-function, complex-branching, hard-to-follow | 23 | 28 | 3 | 81 |
| 9 | [Flux\InstancedMeshes\Flux_InstancedMeshes.cpp:308](Zenith/Flux/InstancedMeshes/Flux_InstancedMeshes.cpp:308) | Flux_InstancedMeshes::ExecuteGBuffer | 57.4 | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 143 |
| 10 | [Vulkan\Zenith_Vulkan_Pipeline.cpp:536](Zenith/Vulkan/Zenith_Vulkan_Pipeline.cpp:536) | Zenith_Vulkan_PipelineBuilder::FromSpecification | 57.3 | very-long-function, complex-branching | 19 | 22 | 3 | 208 |
| 11 | [EntityComponent\Components\Zenith_TweenComponent.cpp:291](Zenith/EntityComponent/Components/Zenith_TweenComponent.cpp:291) | Zenith_TweenComponent::RenderPropertiesPanel | 56.2 | long-function, complex-branching, hard-to-follow | 20 | 29 | 3 | 137 |
| 12 | [AI\Navigation\Zenith_Pathfinding.cpp:106](Zenith/AI/Navigation/Zenith_Pathfinding.cpp:106) | Zenith_Pathfinding::FindPathInternal | 55.9 | long-function, complex-branching, hard-to-follow, many-returns | 17 | 27 | 3 | 136 |
| 13 | [AI\Navigation\Zenith_NavMeshGenerator.cpp:354](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:354) | Zenith_NavMeshGenerator::FilterWalkableSpans | 55.6 | long-function, deep-nesting, hard-to-follow, nesting-hell | 13 | 32 | 5 | 94 |
| 14 | [UI\Zenith_UIText.cpp:126](Zenith/UI/Zenith_UIText.cpp:126) | Zenith_UIText::Render | 55.5 | long-function, complex-branching, hard-to-follow | 17 | 28 | 4 | 121 |
| 15 | [EntityComponent\Zenith_SceneData.cpp:92](Zenith/EntityComponent/Zenith_SceneData.cpp:92) | Zenith_SceneData::ResetEntitiesOnly | 55.4 | long-function, complex-branching, hard-to-follow | 19 | 30 | 3 | 127 |
| 16 | [Flux\Slang\Flux_SlangCompiler.cpp:582](Zenith/Flux/Slang/Flux_SlangCompiler.cpp:582) | Flux_SlangCompiler::ExtractReflection | 55.1 | long-function, complex-branching, hard-to-follow | 15 | 32 | 4 | 82 |
| 17 | [AI\BehaviorTree\Zenith_BTComposites.cpp:117](Zenith/AI/BehaviorTree/Zenith_BTComposites.cpp:117) | Zenith_BTParallel::Execute | 55.0 | long-function, complex-branching, hard-to-follow, many-returns | 15 | 31 | 3 | 107 |
| 18 | [EntityComponent\Components\Zenith_LightComponent.cpp:166](Zenith/EntityComponent/Components/Zenith_LightComponent.cpp:166) | Zenith_LightComponent::RenderPropertiesPanel | 54.8 | long-function, complex-branching, hard-to-follow | 21 | 30 | 2 | 130 |
| 19 | [Editor\Zenith_EditorCamera.cpp:91](Zenith/Editor/Zenith_EditorCamera.cpp:91) | Zenith_Editor::UpdateEditorCamera | 54.6 | long-function, complex-branching, hard-to-follow | 19 | 29 | 3 | 110 |
| 20 | [Editor\Panels\Zenith_EditorPanel_Console.cpp:21](Zenith/Editor/Panels/Zenith_EditorPanel_Console.cpp:21) | Render | 54.3 | long-function, complex-branching, many-params | 17 | 22 | 3 | 115 |
