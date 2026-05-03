# Zenith Engine — Code Health Snapshot — 2026-05-03

## 1. Executive summary

The Zenith engine codebase shows a healthy distribution overall, with most complexity concentrated in a small number of long, deeply-nested functions and large multi-purpose source files in the rendering and entity-component layers.

Parser backend: **tree-sitter** (resolved from `auto`). Profile: **engine-ci**. Include edges resolved by relative path: 2077 (99.0%); by basename fallback: 20 (1.0%) — well under the 10% threshold, so cycle confidence is high.

- Function hotspots at priority_score ≥80: **0** · ≥70: **3** · ≥60: **5**
- High-priority file count: **18** · file priority P90: **66.2**
- Worst function: `Flux_CodeGenerator::BuildSubsystemHeaderContent` ([Flux/Slang/Flux_CodeGenerator.cpp:170](Flux/Slang/Flux_CodeGenerator.cpp:170)) score **79.7**
- Worst file: [Flux/Slang/Flux_CodeGenerator.cpp](Flux/Slang/Flux_CodeGenerator.cpp) score **81.7**
- Files tagged `god-file`: **23**
- Tech-debt markers (after filtering hash-map placeholders): **18** — all `TODO` (no FIXME / HACK / XXX)
- Top directory by hotspot count: **Flux** with 15 of the top-50 hotspots (avg score 54.1)
- Near-duplicate clusters detected: **12**
- Include cycles: **2** — `EntityComponent/Internal/Zenith_SceneRegistry.h ↔ EntityComponent/Zenith_Scene.h`, and `EntityComponent/Zenith_SceneData.h ↔ EntityComponent/Zenith_SceneManager.h`

(Note: today's `DAILY_REPORT.md` was overwritten — an earlier run today produced the previous version.)

## 2. Top 10 refactoring targets

1. [Flux/Slang/Flux_CodeGenerator.cpp:170](Flux/Slang/Flux_CodeGenerator.cpp:170) — `Flux_CodeGenerator::BuildSubsystemHeaderContent` — score **79.7**, dominant `cognitive`, tags `[long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell]`.
   **Why it hurts:** deep inner blocks inside a 190 LOC body — extract inner blocks then flatten with early returns.
   **Extract candidate:** lines 238-301 (64L, peak depth 5).
   **Suggested first move:** lift the 238-301 inner block into a private helper that takes the subsystem context.

2. [Flux/Slang/Flux_SlangCompiler.cpp:581](Flux/Slang/Flux_SlangCompiler.cpp:581) — `Flux_SlangCompiler::CompileProgram` — score **78.4**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 38 paths and many returns spread across 179 LOC — error-handling is interleaved with happy-path logic.
   **Suggested first move:** extract per-stage error checks into small validators returning a result enum; keep the main path linear.

3. [Flux/DynamicLights/Flux_DynamicLights.cpp:305](Flux/DynamicLights/Flux_DynamicLights.cpp:305) — `Flux_DynamicLights::GatherLightsFromScene` — score **74.7**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 174 LOC mixing iteration, frustum-culling, and per-light-type dispatch with 26 branches.
   **Suggested first move:** split per-light-type (point / spot / directional) gather into separate methods.

4. [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:86](Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:86) — `RenderEntityContextMenu` — score **63.8**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 88 LOC of nested ImGui menu state — cognitive load high; look for cohesive sub-steps to name.
   **Suggested first move:** extract each top-level menu section into its own free function.

5. [EntityComponent/Zenith_SceneManager.cpp:1212](EntityComponent/Zenith_SceneManager.cpp:1212) — `Zenith_SceneManager::UnloadAllNonPersistent` — score **62.3**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 131 LOC orchestrating teardown across persistence, callbacks, and queues.
   **Suggested first move:** lift the per-scene teardown loop body into a `UnloadOneScene` helper.

6. [Editor/Zenith_SelectionSystem.cpp:329](Editor/Zenith_SelectionSystem.cpp:329) — `Zenith_SelectionSystem::CalculateBoundingBox` — score **58.0**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 114 LOC walking entity types with per-component AABB logic inline.
   **Suggested first move:** dispatch to per-component-type helpers (mesh, terrain, light) returning AABBs.

7. [EntityComponent/Components/Zenith_ScriptComponent.cpp:651](EntityComponent/Components/Zenith_ScriptComponent.cpp:651) — `Zenith_ScriptComponent::RenderPropertiesPanel` — score **54.9**, dominant `cognitive`, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 108 LOC of editor UI mixing property reflection with widget drawing.
   **Suggested first move:** split per-property-type widget rendering into helpers keyed on a property descriptor.

8. [Flux/Terrain/Flux_TerrainStreamingManager.cpp:1042](Flux/Terrain/Flux_TerrainStreamingManager.cpp:1042) — `Flux_TerrainStreamingManager::BuildChunkDataForGPU_Internal` — score **54.9**, dominant `cognitive`, tags `[long-function, hard-to-follow]`.
   **Why it hurts:** 83 LOC mixing layout decisions with copy loops; cognitive 36 with only depth 4.
   **Suggested first move:** name the cohesive sub-steps (offset table build, vertex pack, index pack) as helpers.

9. [EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:290](EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:290) — `Zenith_TerrainComponent::RenderTerrainRegenerationSection` — score **53.9**, dominant `cognitive`, tags `[long-function, deep-nesting, hard-to-follow, nesting-hell]`.
   **Why it hurts:** depth-5 nesting in a 96 LOC editor block — flatten with early returns and extract inner UI groups.
   **Suggested first move:** lift each `if (collapsedHeader)` body into its own free function.

10. [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:433](Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:433) — `RenderSceneHeaderAndBody` — score **53.6**, dominant `cognitive`, tags `[long-function, many-params]`.
    **Why it hurts:** 9 parameters across 94 LOC — introduce a parameter object that bundles scene/UI state.
    **Suggested first move:** introduce a `HierarchyRenderContext` struct that absorbs the 9 parameters.

## 3. Tag distribution (what shape is the problem?)

Tag tally across the top-50 hotspots:

- `long-function`: 32
- `hard-to-follow`: 31
- `complex-branching`: 25
- `deep-nesting`: 5
- `nesting-hell`: 5
- `many-returns`: 3
- `many-params`: 3
- `very-long-function`: 1
- `big-switch`: 1

Dominant signal across the same 50: cognitive **47**, cyclomatic **3**. The engine bleeds on the cognitive axis: long, branchy, hard-to-follow procedures with high reading load. Big-switch and many-params patterns are rare — the engine's pattern is deep imperative flow, not dispatch tables or argument bloat.

## 4. God files

23 files carry the `god-file` tag. Sorted by file priority_score:

| File | LOC | Functions | Fan-in | Fan-out | Worst fn score | Priority |
|------|----:|---------:|------:|-------:|--------------:|--------:|
| [EntityComponent/Zenith_SceneManager.cpp](EntityComponent/Zenith_SceneManager.cpp) | 1494 | 129 | 0 | 25 | 62.3 | **74.7** |
| [EntityComponent/Zenith_SceneData.cpp](EntityComponent/Zenith_SceneData.cpp) | 1057 | 46 | 0 | 12 | 48.4 | 72.3 |
| [Flux/RenderGraph/Flux_RenderGraph.cpp](Flux/RenderGraph/Flux_RenderGraph.cpp) | 2017 | 91 | 0 | 7 | 51.2 | 71.4 |
| [Vulkan/Zenith_Vulkan_MemoryManager.cpp](Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 2192 | 70 | 0 | 11 | 53.0 | 70.2 |
| [AI/Squad/Zenith_Squad.cpp](AI/Squad/Zenith_Squad.cpp) | 977 | 59 | 0 | 9 | 49.6 | 69.1 |
| [Editor/Zenith_Editor.cpp](Editor/Zenith_Editor.cpp) | 1785 | 52 | 0 | 39 | 48.1 | 68.7 |
| [Physics/Zenith_Physics.cpp](Physics/Zenith_Physics.cpp) | 787 | 41 | 0 | 12 | 46.8 | 68.3 |
| [Vulkan/Zenith_Vulkan_CommandBuffer.cpp](Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 1001 | 44 | 0 | 10 | 43.9 | 67.4 |
| [Vulkan/Zenith_Vulkan_Pipeline.cpp](Vulkan/Zenith_Vulkan_Pipeline.cpp) | 833 | 42 | 0 | 7 | 38.6 | 65.8 |
| [Vulkan/Zenith_Vulkan.cpp](Vulkan/Zenith_Vulkan.cpp) | 1568 | 59 | 0 | 17 | 35.7 | 65.0 |
| [Flux/MeshAnimation/Flux_AnimationStateMachine.cpp](Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 1042 | 63 | 0 | 4 | 35.0 | 64.8 |
| [Flux/MeshAnimation/Flux_BlendTree.cpp](Flux/MeshAnimation/Flux_BlendTree.cpp) | 854 | 65 | 0 | 4 | 34.5 | 64.6 |
| [EntityComponent/Components/Zenith_AnimatorComponent.cpp](EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 728 | 46 | 0 | 11 | 31.1 | 63.6 |
| [Core/Memory/Zenith_MemoryManagement.cpp](Core/Memory/Zenith_MemoryManagement.cpp) | 605 | 41 | 0 | 7 | 30.3 | 61.6 |
| [Collections/Zenith_HashMap.h](Collections/Zenith_HashMap.h) | 613 | 41 | 4 | 3 | 23.8 | 61.4 |
| [EntityComponent/Internal/Zenith_SceneCallbackBus.cpp](EntityComponent/Internal/Zenith_SceneCallbackBus.cpp) | 389 | 46 | 0 | 4 | 22.3 | 60.9 |
| [Flux/MeshAnimation/Flux_AnimationClip.cpp](Flux/MeshAnimation/Flux_AnimationClip.cpp) | 769 | 45 | 0 | 6 | 20.6 | 60.4 |
| [Collections/Zenith_Vector.h](Collections/Zenith_Vector.h) | 502 | 41 | 45 | 3 | 20.9 | 58.8 |
| [EntityComponent/Zenith_SceneData.h](EntityComponent/Zenith_SceneData.h) | 1055 | 77 | 40 | 6 | 19.1 | 58.2 |
| [Editor/Zenith_EditorAutomation.cpp](Editor/Zenith_EditorAutomation.cpp) | 1460 | 107 | 0 | 18 | 44.5 | 45.7 |
| [UI/Zenith_UIElement.h](UI/Zenith_UIElement.h) | 302 | 53 | 11 | 4 | 14.9 | 29.5 |
| [Flux/MeshAnimation/Flux_BlendTree.h](Flux/MeshAnimation/Flux_BlendTree.h) | 380 | 42 | 3 | 3 | 2.2 | 11.7 |
| [UI/Zenith_UIButton.h](UI/Zenith_UIButton.h) | 199 | 43 | 5 | 4 | 5.0 | 10.9 |

## 5. Directory risk concentration

Top-level directories ranked by count of top-50 hotspots:

| Directory | Hotspot count | Avg score |
|-----------|--------------:|----------:|
| Flux | 15 | 54.1 |
| EntityComponent | 11 | 49.4 |
| Editor | 8 | 51.5 |
| AI | 7 | 46.9 |
| UI | 3 | 48.2 |
| Vulkan | 2 | 48.5 |
| Physics | 2 | 45.7 |
| Windows | 1 | 52.5 |
| Profiling | 1 | 44.8 |

## 6. Near-duplicate function clusters

12 clusters detected (Jaccard ≥ 85% on token-normalised 5-gram shingles — same control-flow shape, names differ).

**Cluster 1** (5 members, similarity ≥ 85%):
- [Core/Zenith_TestFramework.h:164](Core/Zenith_TestFramework.h:164) — `Zenith_TestAssertEq` — score 16.2
- [Core/Zenith_TestFramework.h:202](Core/Zenith_TestFramework.h:202) — `Zenith_TestAssertGt` — score 16.2
- [Core/Zenith_TestFramework.h:221](Core/Zenith_TestFramework.h:221) — `Zenith_TestAssertLt` — score 16.2
- [Core/Zenith_TestFramework.h:240](Core/Zenith_TestFramework.h:240) — `Zenith_TestAssertGe` — score 16.2
- [Core/Zenith_TestFramework.h:259](Core/Zenith_TestFramework.h:259) — `Zenith_TestAssertLe` — score 16.2
- **Suggested action:** parameterise on the comparator (or wrap in a single macro/template that takes the operator), so all five share one body.

**Cluster 2** (3 members, similarity ≥ 85%):
- [UI/Zenith_UIButton.cpp:151](UI/Zenith_UIButton.cpp:151) — `Zenith_UIButton::ComputeMousePosition` — score 17.1
- [UI/Zenith_UIScrollView.cpp:59](UI/Zenith_UIScrollView.cpp:59) — `Zenith_UIScrollView::GetTransformedMousePosition` — score 17.1
- [UI/Zenith_UIToggle.cpp:96](UI/Zenith_UIToggle.cpp:96) — `Zenith_UIToggle::GetInteractionMousePosition` — score 17.1
- **Suggested action:** lift mouse-to-element transform into a UI-base helper; all three callers reduce to one call.

**Cluster 3** (2 members, similarity ≥ 85%):
- [Vulkan/Zenith_Vulkan.cpp:1446](Vulkan/Zenith_Vulkan.cpp:1446) — `Zenith_Vulkan::ConvertToVkFormat_Colour` — score 24.8
- [Vulkan/Zenith_Vulkan.cpp:1535](Vulkan/Zenith_Vulkan.cpp:1535) — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` — score 18.7
- **Suggested action:** factor a single mapping table keyed on a unified format enum; both functions become lookups.

**Cluster 4** (2 members, similarity ≥ 85%):
- [Core/Zenith_Tween.cpp:39](Core/Zenith_Tween.cpp:39) — `Zenith_GetEasingTypeName` — score 23.1
- [Flux/Slang/Flux_CodeGenerator.cpp:64](Flux/Slang/Flux_CodeGenerator.cpp:64) — `ResourceKindName` — score 16.8
- **Suggested action:** both are enum-to-name switches; cross-system extraction probably not worth it — consider an `ENUM_TO_STRING` macro per file instead.

**Cluster 5** (2 members, similarity ≥ 85%):
- [Flux/Flux_RenderTargets.cpp:267](Flux/Flux_RenderTargets.cpp:267) — `Flux_RenderAttachmentBuilder::BuildColourFromAliasedVRAM` — score 22.6
- [Flux/Flux_RenderTargets.cpp:202](Flux/Flux_RenderTargets.cpp:202) — `Flux_RenderAttachmentBuilder::BuildColour` — score 21.8
- **Suggested action:** extract a shared attachment-init body; the aliased-VRAM variant takes the alias source as an extra parameter.

**Cluster 6** (2 members, similarity ≥ 85%):
- [AI/BehaviorTree/Zenith_BTComposites.cpp:7](AI/BehaviorTree/Zenith_BTComposites.cpp:7) — `Zenith_BTSequence::Execute` — score 21.2
- [AI/BehaviorTree/Zenith_BTComposites.cpp:50](AI/BehaviorTree/Zenith_BTComposites.cpp:50) — `Zenith_BTSelector::Execute` — score 21.2
- **Suggested action:** templatise on the early-exit predicate (success vs failure) — sequence and selector are duals.

**Cluster 7** (2 members, similarity ≥ 85%):
- [Flux/MeshAnimation/Flux_AnimationClip.cpp:63](Flux/MeshAnimation/Flux_AnimationClip.cpp:63) — `Flux_RootMotion::SamplePositionDelta` — score 20.6
- [Flux/MeshAnimation/Flux_AnimationClip.cpp:88](Flux/MeshAnimation/Flux_AnimationClip.cpp:88) — `Flux_RootMotion::SampleRotationDelta` — score 20.6
- **Suggested action:** extract the time-bracket lookup; specialise only the per-channel lerp/slerp.

**Cluster 8** (2 members, similarity ≥ 85%):
- [Vulkan/Zenith_Vulkan_MemoryManager.cpp:797](Vulkan/Zenith_Vulkan_MemoryManager.cpp:797) — `Zenith_Vulkan_MemoryManager::InitialiseDynamicReadWriteBuffer` — score 16.6
- [Vulkan/Zenith_Vulkan_MemoryManager.cpp:715](Vulkan/Zenith_Vulkan_MemoryManager.cpp:715) — `Zenith_Vulkan_MemoryManager::InitialiseDynamicConstantBuffer` — score 15.5
- **Suggested action:** extract a private `InitialiseDynamicBuffer` taking usage flags; both wrappers become one-liners.

**Cluster 9** (2 members, similarity ≥ 85%):
- [EntityComponent/Zenith_SceneData.cpp:696](EntityComponent/Zenith_SceneData.cpp:696) — `Zenith_SceneData::DispatchOnUpdateForEntities` — score 15.5
- [EntityComponent/Zenith_SceneData.cpp:712](EntityComponent/Zenith_SceneData.cpp:712) — `Zenith_SceneData::DispatchOnLateUpdateForEntities` — score 15.5
- **Suggested action:** parameterise on the lifecycle hook (function pointer or method-on-component), so the dispatch loop is shared.

**Cluster 10** (2 members, similarity ≥ 85%):
- [AssetHandling/Zenith_AssetRegistry.cpp:183](AssetHandling/Zenith_AssetRegistry.cpp:183) — `Zenith_AssetRegistry::SetGameAssetsDir` — score 15.4
- [AssetHandling/Zenith_AssetRegistry.cpp:201](AssetHandling/Zenith_AssetRegistry.cpp:201) — `Zenith_AssetRegistry::SetEngineAssetsDir` — score 15.4
- **Suggested action:** share validation/normalisation; the two setters differ only by which member they write.

(Caps at 10. Remaining clusters 11–12 are 2-member each: `Flux_BlendTreeNode_BlendSpace1D::GetNormalizedTime` vs `Flux_BlendTreeNode_BlendSpace2D::GetNormalizedTime` in `Flux/MeshAnimation/Flux_BlendTree.cpp`, and `IsReadAccess` vs `IsWriteAccess` in `Flux/RenderGraph/Flux_RenderGraph.cpp`.)

## 7. Include coupling & cycles

**Edge confidence:** 2077 relative + 20 basename = 2097 edges total; basename fallback is **1.0%**, so cycle confidence is high across the graph.

**Cycles** (2 total):
1. `EntityComponent/Internal/Zenith_SceneRegistry.h` ↔ `EntityComponent/Zenith_Scene.h`
2. `EntityComponent/Zenith_SceneData.h` ↔ `EntityComponent/Zenith_SceneManager.h`

**High fan-in headers** (changes ripple widest):

| File | fan_in | fan_out |
|------|------:|-------:|
| [Core/Zenith.h](Core/Zenith.h) | 197 | 11 |
| [Flux/Flux.h](Flux/Flux.h) | 72 | 6 |
| [Core/Memory/Zenith_MemoryManagement_Disabled.h](Core/Memory/Zenith_MemoryManagement_Disabled.h) | 65 | 0 |
| [DataStream/Zenith_DataStream.h](DataStream/Zenith_DataStream.h) | 64 | 1 |
| [Core/Memory/Zenith_MemoryManagement_Enabled.h](Core/Memory/Zenith_MemoryManagement_Enabled.h) | 60 | 0 |

**High fan-out files** (include the most):

| File | fan_in | fan_out |
|------|------:|-------:|
| [Editor/Zenith_Editor.cpp](Editor/Zenith_Editor.cpp) | 0 | 39 |
| [Flux/Flux.cpp](Flux/Flux.cpp) | 0 | 31 |
| [Core/Zenith_Core.cpp](Core/Zenith_Core.cpp) | 0 | 26 |
| [EntityComponent/Zenith_SceneManager.cpp](EntityComponent/Zenith_SceneManager.cpp) | 0 | 25 |
| [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp](Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp) | 0 | 22 |

## 8. Low-cohesion classes

**Caveat:** every record in this section is `header-inline only` — LCOM5 is computed from inline-defined methods inside the header, not from full class implementations. Treat as a header-design signal, not a full-class score.

30 classes flagged. Top 10 by LCOM5 (all 1.0 — perfect lack of cohesion among inline methods):

| Class | File:line | LCOM5 | Methods | Fields |
|-------|-----------|------:|--------:|-------:|
| `Zenith_Vulkan` | [Vulkan/Zenith_Vulkan.h:103](Vulkan/Zenith_Vulkan.h:103) | 1.0 | 12 | 4 |
| `Zenith_Vulkan_MemoryManager` | [Vulkan/Zenith_Vulkan_MemoryManager.h:34](Vulkan/Zenith_Vulkan_MemoryManager.h:34) | 1.0 | 12 | 31 |
| `Flux_Gizmos` | [Flux/Gizmos/Flux_Gizmos.h:36](Flux/Gizmos/Flux_Gizmos.h:36) | 1.0 | 4 | 5 |
| `Flux_TerrainStreamingManager` | [Flux/Terrain/Flux_TerrainStreamingManager.h:183](Flux/Terrain/Flux_TerrainStreamingManager.h:183) | 1.0 | 4 | 12 |
| `Zenith_SceneOperationQueue` | [EntityComponent/Internal/Zenith_SceneOperationQueue.h:36](EntityComponent/Internal/Zenith_SceneOperationQueue.h:36) | 1.0 | 3 | 25 |

(25 more entries omitted — same caveat applies; consult the JSON for the full list.)

## 9. Tech-debt markers

After filtering hash-map placeholder entries (`Replace with engine hash map / hash set / container`):

- **TODO:** 18
- **FIXME:** 0
- **HACK:** 0
- **XXX:** 0

Top markers (FIXME > HACK ≈ XXX > TODO, then file/line):

- [Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor/Zenith_UndoSystem.cpp:352](Editor/Zenith_UndoSystem.cpp:352) **TODO** Serialize full entity state (all components)
- [Editor/Zenith_UndoSystem.cpp:404](Editor/Zenith_UndoSystem.cpp:404) **TODO** Deserialize full entity state
- [EntityComponent/Zenith_SceneData.cpp:179](EntityComponent/Zenith_SceneData.cpp:179) **TODO** auto-promote same-scene self-unload to async
- [EntityComponent/Zenith_SceneManager.cpp:1007](EntityComponent/Zenith_SceneManager.cpp:1007) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting
- [Flux/Flux.h:105](Flux/Flux.h:105) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Flux/Flux.h:114](Flux/Flux.h:114) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Flux/Flux_CommandList.h:89](Flux/Flux_CommandList.h:89) **TODO** elide second memcpy by having AddCommand record directly
- [Flux/IBL/Flux_IBL.cpp:543](Flux/IBL/Flux_IBL.cpp:543) **TODO** create per-face 2D SRVs for cubemap debug display
- [Flux/MeshAnimation/Flux_AnimationController.cpp:178](Flux/MeshAnimation/Flux_AnimationController.cpp:178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- [Flux/MeshAnimation/Flux_AnimationController.h:293](Flux/MeshAnimation/Flux_AnimationController.h:293) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Flux/Shadows/Flux_Shadows.cpp:154](Flux/Shadows/Flux_Shadows.cpp:154) **TODO** Enable terrain shadow casting
- [Vulkan/Zenith_Vulkan.cpp:1388](Vulkan/Zenith_Vulkan.cpp:1388) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- [Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411](Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411) **TODO** expose DONT_CARE LoadOp as a third option

(15 lower-volume render-graph TODOs match the hash-map filter and are excluded.)

## 10. Headline numbers

`Files 421 · SLOC 79,871 · functions 5,212 · avg MI 30.34 · min MI 0 · avg CC 20.30 · max CC 289 · est bugs 1150.94`

(Also: avg cognitive 26.49, max cognitive 426, total comment lines 21,243, total blank lines 17,925.)

## 11. Appendix — top-20 hotspots

| # | file:line | name | score | dominant | tags | CC | cog | nest | LOC |
|--:|-----------|------|------:|----------|------|---:|----:|----:|----:|
| 1 | [Flux/Slang/Flux_CodeGenerator.cpp:170](Flux/Slang/Flux_CodeGenerator.cpp:170) | `Flux_CodeGenerator::BuildSubsystemHeaderContent` | 79.7 | cognitive | long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell | 23 | 67 | 5 | 190 |
| 2 | [Flux/Slang/Flux_SlangCompiler.cpp:581](Flux/Slang/Flux_SlangCompiler.cpp:581) | `Flux_SlangCompiler::CompileProgram` | 78.4 | cognitive | long-function, complex-branching, hard-to-follow, many-returns | 38 | 41 | 3 | 179 |
| 3 | [Flux/DynamicLights/Flux_DynamicLights.cpp:305](Flux/DynamicLights/Flux_DynamicLights.cpp:305) | `Flux_DynamicLights::GatherLightsFromScene` | 74.7 | cognitive | long-function, complex-branching, hard-to-follow, many-returns | 26 | 60 | 4 | 174 |
| 4 | [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:86](Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:86) | `RenderEntityContextMenu` | 63.8 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 38 | 4 | 88 |
| 5 | [EntityComponent/Zenith_SceneManager.cpp:1212](EntityComponent/Zenith_SceneManager.cpp:1212) | `Zenith_SceneManager::UnloadAllNonPersistent` | 62.3 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 35 | 4 | 131 |
| 6 | [Editor/Zenith_SelectionSystem.cpp:329](Editor/Zenith_SelectionSystem.cpp:329) | `Zenith_SelectionSystem::CalculateBoundingBox` | 58.0 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 32 | 4 | 114 |
| 7 | [EntityComponent/Components/Zenith_ScriptComponent.cpp:651](EntityComponent/Components/Zenith_ScriptComponent.cpp:651) | `Zenith_ScriptComponent::RenderPropertiesPanel` | 54.9 | cognitive | long-function, complex-branching, hard-to-follow | 21 | 29 | 3 | 108 |
| 8 | [Flux/Terrain/Flux_TerrainStreamingManager.cpp:1042](Flux/Terrain/Flux_TerrainStreamingManager.cpp:1042) | `Flux_TerrainStreamingManager::BuildChunkDataForGPU_Internal` | 54.9 | cognitive | long-function, hard-to-follow | 11 | 36 | 4 | 83 |
| 9 | [EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:290](EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:290) | `Zenith_TerrainComponent::RenderTerrainRegenerationSection` | 53.9 | cognitive | long-function, deep-nesting, hard-to-follow, nesting-hell | 14 | 30 | 5 | 96 |
| 10 | [Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:433](Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:433) | `RenderSceneHeaderAndBody` | 53.6 | cognitive | long-function, many-params | 13 | 21 | 4 | 94 |
| 11 | [Vulkan/Zenith_Vulkan_MemoryManager.cpp:1012](Vulkan/Zenith_Vulkan_MemoryManager.cpp:1012) | `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` | 53.0 | cognitive | long-function, hard-to-follow | 14 | 26 | 3 | 141 |
| 12 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:687](AI/Navigation/Zenith_NavMeshGenerator.cpp:687) | `Zenith_NavMeshGenerator::BuildPolygonMesh` | 52.6 | cognitive | long-function, hard-to-follow | 10 | 33 | 4 | 110 |
| 13 | [Windows/Zenith_Windows_FileWatcher.cpp:166](Windows/Zenith_Windows_FileWatcher.cpp:166) | `Zenith_FileWatcher::UpdatePlatform` | 52.5 | cognitive | long-function, complex-branching | 18 | 23 | 4 | 137 |
| 14 | [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118) | `Flux_AnimatedMeshes::ExecuteGBuffer` | 52.4 | cognitive | long-function, hard-to-follow | 13 | 29 | 4 | 97 |
| 15 | [UI/Zenith_UICanvas.cpp:203](UI/Zenith_UICanvas.cpp:203) | `Zenith_UICanvas::Update` | 52.0 | cognitive | complex-branching, hard-to-follow | 15 | 45 | 2 | 43 |
| 16 | [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Editor/Panels/Zenith_EditorPanel_Memory.cpp:262) | `RenderCategoryTab` | 51.7 | cognitive | long-function, hard-to-follow | 11 | 34 | 4 | 94 |
| 17 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1165](Flux/RenderGraph/Flux_RenderGraph.cpp:1165) | `Flux_RenderGraph::SynthesizeAliasingBarriers` | 51.2 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 94 |
| 18 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:297](Flux/MeshGeometry/Flux_MeshGeometry.cpp:297) | `Flux_MeshGeometry::Combine` | 51.0 | cognitive | long-function, complex-branching, many-returns | 20 | 24 | 3 | 84 |
| 19 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844](EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844) | `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` | 50.0 | cognitive | long-function, hard-to-follow | 13 | 25 | 3 | 172 |
| 20 | [AI/Squad/Zenith_Squad.cpp:719](AI/Squad/Zenith_Squad.cpp:719) | `Zenith_Squad::AssignFormationSlots` | 49.6 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 28 | 3 | 81 |
