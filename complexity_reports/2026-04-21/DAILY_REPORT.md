# Zenith Engine — Daily Code Health Snapshot (2026-04-21)

## 1. Executive summary

- **Overall rating:** Poor (difficult to maintain). Average MI across the engine sits at 30.0 with 12 files bottoming at MI=0 — large imperative hot spots dominate the maintainability score.
- **Priority-score buckets in `function_hotspots`:** 0 at ≥80, 0 at ≥70, 0 at ≥60 (top single-function score is 59.2).
- **God files:** 22 files tagged `god-file`.
- **Tech-debt markers (hash-map placeholders filtered out):** 19 total — TODO 19, FIXME 0, HACK 0, XXX 0.
- **Directory with the highest hotspot concentration:** `AI/Navigation` — 7 of 50 function hotspots live there (avg score 48.8).

## 2. Top 10 refactoring targets

1. [Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247](Zenith/Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247) — `Flux_RenderGraph::AssertBoundResourceDeclared` — score 59.2, tags `[complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 18-way branching validation with 7 exit points — reader has to trace every early return to understand the invariants.
   **Suggested first move:** extract each declared-resource check into a predicate helper and return a single status enum from the top level.

2. [EntityComponent/Components/Zenith_TweenComponent.cpp:291](Zenith/EntityComponent/Components/Zenith_TweenComponent.cpp:291) — `Zenith_TweenComponent::RenderPropertiesPanel` — score 56.2, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 137 LOC UI block with per-property branching — each tween kind inlines its own widget code.
   **Suggested first move:** split one `Render<Kind>Section()` per tween property type and dispatch from the outer body.

3. [AI/Navigation/Zenith_Pathfinding.cpp:106](Zenith/AI/Navigation/Zenith_Pathfinding.cpp:106) — `Zenith_Pathfinding::FindPathInternal` — score 55.9, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 136 LOC A*-style body with 17 branches and multiple bail-outs mixed into the main loop.
   **Suggested first move:** pull open/closed-list maintenance and goal-reached handling into named helpers; keep the outer loop to pop/expand/push.

4. [AI/Navigation/Zenith_NavMeshGenerator.cpp:354](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:354) — `Zenith_NavMeshGenerator::FilterWalkableSpans` — score 55.6, tags `[long-function, deep-nesting, hard-to-follow, nesting-hell]`.
   **Why it hurts:** 5-deep inner loops over span columns with filter predicates buried inside — classic deep-nested imperative.
   **Extract candidate:** lines 399-418 (peak depth 5, 20 LOC).
   **Suggested first move:** extract the inner walkable-span filter into `IsWalkableSpan(...)` and flatten with `continue`.

5. [UI/Zenith_UIText.cpp:126](Zenith/UI/Zenith_UIText.cpp:126) — `Zenith_UIText::Render` — score 55.5, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 121 LOC render path mixes layout, glyph selection, and draw-call emission in one body.
   **Suggested first move:** separate `ComputeLayout()` from `EmitDrawCommands()` and keep `Render` as the orchestrator.

6. [EntityComponent/Zenith_SceneData.cpp:92](Zenith/EntityComponent/Zenith_SceneData.cpp:92) — `Zenith_SceneData::ResetEntitiesOnly` — score 55.4, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 127 LOC with 19 branches reaching into entity slot state machine by hand.
   **Suggested first move:** extract lifecycle-state transitions (PENDING_START cleanup, MarkedForDestruction handling) into small step functions.

7. [Flux/Slang/Flux_SlangCompiler.cpp:582](Zenith/Flux/Slang/Flux_SlangCompiler.cpp:582) — `Flux_SlangCompiler::ExtractReflection` — score 55.1, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 82 LOC, 4-deep nesting walking the Slang reflection tree with inline type-kind branching.
   **Suggested first move:** dispatch on Slang type kind into `ExtractStruct`, `ExtractResource`, `ExtractSampler` helpers.

8. [AI/BehaviorTree/Zenith_BTComposites.cpp:117](Zenith/AI/BehaviorTree/Zenith_BTComposites.cpp:117) — `Zenith_BTParallel::Execute` — score 55.0, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** Parallel-node tick combines success/failure-policy logic with child iteration and 5+ early returns.
   **Suggested first move:** compute child statuses into a small accumulator struct, then apply the policy in one place at the end.

9. [Flux/RenderGraph/Flux_RenderGraph.cpp:1683](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1683) — `Flux_RenderGraph::SynthesizeBarriers` — score 55.0, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 150 LOC, CC 21 — barrier synthesis interleaves resource-state bookkeeping with emit logic.
   **Suggested first move:** extract `DetermineTransition(prevState, nextState)` and `EmitBarrier(...)` so the outer loop just drives the walk.

10. [EntityComponent/Components/Zenith_LightComponent.cpp:166](Zenith/EntityComponent/Components/Zenith_LightComponent.cpp:166) — `Zenith_LightComponent::RenderPropertiesPanel` — score 54.8, tags `[long-function, complex-branching, hard-to-follow]`.
    **Why it hurts:** 130 LOC with 21 branches keyed on light type — directional / point / spot / area widgets all coexist.
    **Suggested first move:** one helper per `LIGHT_TYPE_*` and switch-dispatch from the panel entry point.

## 3. Tag distribution (what shape is the problem?)

Across the top-50 `function_hotspots`:

- `long-function`: 33
- `hard-to-follow`: 32
- `complex-branching`: 30
- `deep-nesting`: 6
- `nesting-hell`: 6
- `many-returns`: 5
- `many-params`: 3
- `very-long-function`: 1
- `big-switch`: 1

**Interpretation:** the dominant shape is long + branchy + hard-to-follow bodies — cognitive load comes from length and fan-out of `if`/`else`, not from deep indentation or dispatch tables. Classic wins here are extract-method (splitting 100+ LOC functions by cohesive step) and guard-clause flattening, not polymorphism replacing switches.

## 4. God files

| Path | LOC | Functions | Worst func score | Priority |
|---|---:|---:|---:|---:|
| [EntityComponent/Zenith_SceneData.cpp](Zenith/EntityComponent/Zenith_SceneData.cpp) | 946 | 45 | 55.4 | 74.4 |
| [Flux/RenderGraph/Flux_RenderGraph.cpp](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp) | 1370 | 65 | 55.0 | 72.5 |
| [EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 1405 | 74 | 50.4 | 71.1 |
| [AI/Squad/Zenith_Squad.cpp](Zenith/AI/Squad/Zenith_Squad.cpp) | 786 | 56 | 49.6 | 69.1 |
| [Vulkan/Zenith_Vulkan_MemoryManager.cpp](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 1505 | 68 | 48.0 | 68.7 |
| [Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 1264 | 52 | 47.5 | 68.5 |
| [Vulkan/Zenith_Vulkan_CommandBuffer.cpp](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 733 | 44 | 40.1 | 66.3 |
| [UnitTests/Zenith_SceneTests.cpp](Zenith/UnitTests/Zenith_SceneTests.cpp) | 9373 | 496 | 35.9 | 65.0 |
| [Vulkan/Zenith_Vulkan.cpp](Zenith/Vulkan/Zenith_Vulkan.cpp) | 1167 | 51 | 35.8 | 65.0 |
| [Flux/MeshAnimation/Flux_AnimationStateMachine.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 806 | 59 | 35.0 | 64.8 |
| [Flux/MeshAnimation/Flux_BlendTree.cpp](Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp) | 667 | 65 | 34.6 | 64.6 |
| [EntityComponent/Components/Zenith_AnimatorComponent.cpp](Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 509 | 44 | 31.2 | 63.6 |
| [UnitTests/Zenith_UnitTests.cpp](Zenith/UnitTests/Zenith_UnitTests.cpp) | 7743 | 269 | 29.0 | 63.0 |
| [Flux/MeshAnimation/Flux_AnimationClip.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp) | 596 | 42 | 20.7 | 60.5 |
| [EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) | 664 | 75 | 19.2 | 58.3 |
| [UnitTests/Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp) | 1973 | 87 | 23.0 | 57.6 |
| [UnitTests/Zenith_AITests.cpp](Zenith/UnitTests/Zenith_AITests.cpp) | 1890 | 94 | 20.7 | 56.3 |
| [EntityComponent/Components/Zenith_ScriptComponent.h](Zenith/EntityComponent/Components/Zenith_ScriptComponent.h) | 196 | 50 | 14.4 | 50.2 |
| [Editor/Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) | 1501 | 85 | 44.5 | 45.7 |
| [UI/Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h) | 195 | 50 | 15.0 | 29.6 |
| [UnitTests/Zenith_PhysicsTests.cpp](Zenith/UnitTests/Zenith_PhysicsTests.cpp) | 882 | 52 | 12.6 | 26.7 |
| [UnitTests/Zenith_EditorTests.cpp](Zenith/UnitTests/Zenith_EditorTests.cpp) | 1268 | 98 | 12.1 | 16.4 |

## 5. Directory risk concentration

| Directory | Hotspots | Avg score |
|---|---:|---:|
| AI/Navigation | 7 | 48.8 |
| Flux/RenderGraph | 5 | 51.9 |
| EntityComponent/Components | 5 | 52.5 |
| Editor/Panels | 5 | 51.0 |
| UI | 4 | 48.2 |
| EntityComponent | 4 | 49.7 |
| Editor | 4 | 48.4 |
| Flux/MeshGeometry | 2 | 49.0 |
| Flux/MeshAnimation | 2 | 46.1 |
| Physics | 2 | 45.8 |

## 6. Tech-debt markers

After filtering hash-map/hash-set/container placeholders:

- **TODO:** 19
- **FIXME:** 0
- **HACK:** 0
- **XXX:** 0

Top markers (sorted by severity then `file`:`line`):

- [Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor/Zenith_UndoSystem.cpp:343](Zenith/Editor/Zenith_UndoSystem.cpp:343) **TODO** Serialize full entity state (all components)
- [Editor/Zenith_UndoSystem.cpp:391](Zenith/Editor/Zenith_UndoSystem.cpp:391) **TODO** Deserialize full entity state
- [EntityComponent/Components/Zenith_ModelComponent.cpp:454](Zenith/EntityComponent/Components/Zenith_ModelComponent.cpp:454) **TODO** Flux_MeshInstance needs to provide access to geometry or position data
- [EntityComponent/Zenith_SceneData.cpp:103](Zenith/EntityComponent/Zenith_SceneData.cpp:103) **TODO** auto-promote same-scene self-unload to async.
- [EntityComponent/Zenith_SceneManager.cpp:1285](Zenith/EntityComponent/Zenith_SceneManager.cpp:1285) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- [EntityComponent/Zenith_SceneManager.cpp:1764](Zenith/EntityComponent/Zenith_SceneManager.cpp:1764) **TODO** Use scaled time when timeScale system is implemented
- [Flux/Flux.h:156](Zenith/Flux/Flux.h:156) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Flux/Flux.h:165](Zenith/Flux/Flux.h:165) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Flux/Flux_CommandList.h:88](Zenith/Flux/Flux_CommandList.h:88) **TODO** elide second memcpy by having AddCommand record directly into the ring
- [Flux/IBL/Flux_IBL.cpp:546](Zenith/Flux/IBL/Flux_IBL.cpp:546) **TODO** create per-face 2D SRVs for cubemap debug display.
- [Flux/MeshAnimation/Flux_AnimationController.cpp:178](Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED
- [Flux/MeshAnimation/Flux_AnimationController.h:293](Zenith/Flux/MeshAnimation/Flux_AnimationController.h:293) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Flux/Shadows/Flux_Shadows.cpp:155](Zenith/Flux/Shadows/Flux_Shadows.cpp:155) **TODO** Enable terrain shadow casting
- [Flux/StaticMeshes/Flux_StaticMeshes.cpp:226](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:226) **TODO** these 2 should probably be separate components.
- [UnitTests/Zenith_AITests.cpp:2446](Zenith/UnitTests/Zenith_AITests.cpp:2446) **TODO** Re-enable once CountWalkableSpans/WalkableSpanStats are restored to NavMeshGenerator
- [UnitTests/Zenith_AITests.cpp:2452](Zenith/UnitTests/Zenith_AITests.cpp:2452) **TODO** Re-enable once HasSufficientClearance is restored to NavMeshGenerator
- [Vulkan/Zenith_Vulkan.cpp:1365](Zenith/Vulkan/Zenith_Vulkan.cpp:1365) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker heap is exhausted
- [Vulkan/Zenith_Vulkan_CommandBuffer.cpp:389](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:389) **TODO** expose DONT_CARE LoadOp as a third option.

## 7. Headline numbers

Files **409** · SLOC **99,745** · Functions **5,439** · Avg MI **30.04** · Min MI **0.00** · Avg CC **20.85** · Max CC **249** · Est. bugs **1,512.55**

## 8. Appendix — top-20 hotspots

| # | file:line | Name | Score | Tags | CC | Cog | Nest | LOC |
|---:|---|---|---:|---|---:|---:|---:|---:|
| 1 | [Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247](Zenith/Flux/RenderGraph/Flux_RenderGraph_Execution.cpp:247) | Flux_RenderGraph::AssertBoundResourceDeclared | 59.2 | complex-branching, hard-to-follow, many-returns | 18 | 36 | 3 | 80 |
| 2 | [EntityComponent/Components/Zenith_TweenComponent.cpp:291](Zenith/EntityComponent/Components/Zenith_TweenComponent.cpp:291) | Zenith_TweenComponent::RenderPropertiesPanel | 56.2 | long-function, complex-branching, hard-to-follow | 20 | 29 | 3 | 137 |
| 3 | [AI/Navigation/Zenith_Pathfinding.cpp:106](Zenith/AI/Navigation/Zenith_Pathfinding.cpp:106) | Zenith_Pathfinding::FindPathInternal | 55.9 | long-function, complex-branching, hard-to-follow, many-returns | 17 | 27 | 3 | 136 |
| 4 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:354](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:354) | Zenith_NavMeshGenerator::FilterWalkableSpans | 55.6 | long-function, deep-nesting, hard-to-follow, nesting-hell | 13 | 32 | 5 | 94 |
| 5 | [UI/Zenith_UIText.cpp:126](Zenith/UI/Zenith_UIText.cpp:126) | Zenith_UIText::Render | 55.5 | long-function, complex-branching, hard-to-follow | 17 | 28 | 4 | 121 |
| 6 | [EntityComponent/Zenith_SceneData.cpp:92](Zenith/EntityComponent/Zenith_SceneData.cpp:92) | Zenith_SceneData::ResetEntitiesOnly | 55.4 | long-function, complex-branching, hard-to-follow | 19 | 30 | 3 | 127 |
| 7 | [Flux/Slang/Flux_SlangCompiler.cpp:582](Zenith/Flux/Slang/Flux_SlangCompiler.cpp:582) | Flux_SlangCompiler::ExtractReflection | 55.1 | long-function, complex-branching, hard-to-follow | 15 | 32 | 4 | 82 |
| 8 | [AI/BehaviorTree/Zenith_BTComposites.cpp:117](Zenith/AI/BehaviorTree/Zenith_BTComposites.cpp:117) | Zenith_BTParallel::Execute | 55.0 | long-function, complex-branching, hard-to-follow, many-returns | 15 | 31 | 3 | 107 |
| 9 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1683](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1683) | Flux_RenderGraph::SynthesizeBarriers | 55.0 | long-function, complex-branching, hard-to-follow, many-returns | 21 | 25 | 3 | 150 |
| 10 | [EntityComponent/Components/Zenith_LightComponent.cpp:166](Zenith/EntityComponent/Components/Zenith_LightComponent.cpp:166) | Zenith_LightComponent::RenderPropertiesPanel | 54.8 | long-function, complex-branching, hard-to-follow | 21 | 30 | 2 | 130 |
| 11 | [Editor/Zenith_EditorCamera.cpp:91](Zenith/Editor/Zenith_EditorCamera.cpp:91) | Zenith_Editor::UpdateEditorCamera | 54.6 | long-function, complex-branching, hard-to-follow | 19 | 29 | 3 | 110 |
| 12 | [Editor/Panels/Zenith_EditorPanel_Console.cpp:21](Zenith/Editor/Panels/Zenith_EditorPanel_Console.cpp:21) | Render | 54.3 | long-function, complex-branching, many-params | 17 | 22 | 3 | 115 |
| 13 | [EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289](Zenith/EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289) | Zenith_TerrainComponent::RenderTerrainRegenerationSection | 53.9 | long-function, deep-nesting, hard-to-follow, nesting-hell | 14 | 30 | 5 | 97 |
| 14 | [Flux/DynamicLights/Flux_DynamicLights.cpp:946](Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp:946) | ExecuteDynamicLights | 53.9 | long-function, complex-branching | 16 | 23 | 3 | 178 |
| 15 | [Flux/StaticMeshes/Flux_StaticMeshes.cpp:230](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:230) | Flux_StaticMeshes::RenderToShadowMap | 53.9 | long-function, deep-nesting, hard-to-follow, nesting-hell | 12 | 30 | 5 | 85 |
| 16 | [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419) | RenderSceneHeaderAndBody | 53.7 | long-function, many-params | 13 | 21 | 4 | 95 |
| 17 | [EntityComponent/Components/Zenith_TerrainComponent.cpp:312](Zenith/EntityComponent/Components/Zenith_TerrainComponent.cpp:312) | Zenith_TerrainComponent::ReadFromDataStream | 53.5 | long-function, complex-branching, hard-to-follow | 15 | 32 | 3 | 110 |
| 18 | [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85) | RenderEntityContextMenu | 52.9 | complex-branching, hard-to-follow | 15 | 28 | 4 | 76 |
| 19 | [Windows/Zenith_Windows_FileWatcher.cpp:165](Zenith/Windows/Zenith_Windows_FileWatcher.cpp:165) | Zenith_FileWatcher::UpdatePlatform | 52.6 | long-function, complex-branching | 18 | 23 | 4 | 138 |
| 20 | [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104) | Flux_AnimatedMeshes::ExecuteGBuffer | 52.4 | long-function, hard-to-follow | 13 | 29 | 4 | 98 |
