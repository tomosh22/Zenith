# Zenith Engine — Daily Code Health Snapshot

**Date:** 2026-05-04
**Profile:** `engine-ci`
**Parser backend:** tree-sitter

> Snapshot only. No comparisons against previous runs.

---

## 1. Executive summary

The engine is a moderately complex C++20 codebase whose pain is concentrated in a handful of long, cognitively dense functions inside the scene/render-graph/navmesh subsystems; nothing is in the highest-severity bracket.

- Parser: **tree-sitter**, profile: **engine-ci**.
- Include-edge resolution: **2090 relative / 20 basename (0.9% basename)** — cycle confidence is high; no warning needed.
- Function hotspots: **0 ≥80**, **0 ≥70**, **0 ≥60** (top score is 52.4).
- High-priority files: **9** (≥70). P90 file priority: **66.0**.
- Worst function: `Flux_AnimatedMeshes::ExecuteGBuffer` at [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp#L118), score **52.4**.
- Worst file: [Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp), score **74.5**.
- **23** files tagged `god-file`.
- **14** tech-debt markers (after filtering hash-map placeholders), all of type `TODO`.
- Highest hotspot concentration directory: **Editor** (6 hotspots, avg score 44.3).
- Near-duplicate clusters: **4**.
- No include cycles detected.

---

## 2. Top 10 refactoring targets

1. [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp#L118) — `Flux_AnimatedMeshes::ExecuteGBuffer` — score **52.4**, dominant **cognitive**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 97 LOC with cognitive load 29 at depth 4 — too many concerns interleaved in a single render-pass body.
   - **Suggested first move:** carve out per-stage helpers (binding setup, draw submission, descriptor push) so the top-level function reads as a sequence of named steps.

2. [UI/Zenith_UICanvas.cpp:203](Zenith/UI/Zenith_UICanvas.cpp#L203) — `Zenith_UICanvas::Update` — score **52.0**, dominant **cognitive**, tags `[complex-branching, hard-to-follow]`.
   - **Why it hurts:** cognitive 45 in only 43 LOC and CC 15 — branch-heavy update loop where decision points are tightly packed.
   - **Suggested first move:** split per-state branches into named helpers (e.g. `UpdateLayout()`, `UpdateInputDispatch()`) so each branch can be reasoned about independently.

3. [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp#L262) — `RenderCategoryTab` — score **51.7**, dominant **cognitive**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 94 LOC, depth 4, cognitive 34 — ImGui render code with deep conditional UI sections.
   - **Suggested first move:** extract each UI section (table render, summary stats, drilldown) into local lambdas or static helpers.

4. [Flux/RenderGraph/Flux_RenderGraph.cpp:1172](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L1172) — `Flux_RenderGraph::SynthesizeAliasingBarriers` — score **51.2**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 94 LOC, CC 17, cognitive 30 — barrier-synthesis logic with multiple resource-state branches in one body.
   - **Suggested first move:** isolate per-resource-class barrier emission (image vs buffer vs depth) into separate helpers driven from the outer loop.

5. [Flux/MeshGeometry/Flux_MeshGeometry.cpp:297](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp#L297) — `Flux_MeshGeometry::Combine` — score **51.0**, dominant **cognitive**, tags `[long-function, complex-branching, many-returns]`.
   - **Why it hurts:** CC 20 with multiple early returns spread over 84 LOC — hard to trace the success path.
   - **Suggested first move:** introduce a small `CombineResult` validation step up-front, then write the merge with one happy path.

6. [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp#L844) — `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` — score **50.0**, dominant **cognitive**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 172 LOC — by far the longest hotspot here; mixes status checks, asset releases, and book-keeping.
   - **Suggested first move:** split into `DrainCompletedUnloads()`, `ApplyUnloadEffects()`, `RetireSlots()` helpers driven from a thin top-level loop.

7. [AI/Squad/Zenith_Squad.cpp:719](Zenith/AI/Squad/Zenith_Squad.cpp#L719) — `Zenith_Squad::AssignFormationSlots` — score **49.6**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 81 LOC, CC 18, cognitive 28 — formation-assignment branches interleaved with iteration over members.
   - **Suggested first move:** split slot-scoring from slot-assignment so the matching pass becomes a tight loop over already-scored candidates.

8. [Flux/RenderGraph/Flux_RenderGraph.cpp:1589](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L1589) — `Flux_RenderGraph::AssignClearFlags` — score **49.5**, dominant **cognitive**, tags `[complex-branching, hard-to-follow]`.
   - **Why it hurts:** CC 18 and cognitive 32 packed into 40 LOC — branch density per line is the issue, not size.
   - **Suggested first move:** lift a small predicate (e.g. `ShouldClearAttachment(passIdx, attachmentIdx)`) so the outer loop is one assignment per attachment.

9. [AI/Navigation/Zenith_NavMeshGenerator.cpp:714](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp#L714) — `Zenith_NavMeshGenerator::EmitQuadsFromSpans` — score **48.6**, dominant **cognitive**, tags `[hard-to-follow]`.
   - **Why it hurts:** depth 4 with cognitive 32 in 61 LOC — span/quad geometry walking with nested conditionals.
   - **Suggested first move:** flatten with early-`continue`s on degenerate spans before the inner emission block.

10. [EntityComponent/Zenith_SceneData.cpp:814](Zenith/EntityComponent/Zenith_SceneData.cpp#L814) — `Zenith_SceneData::DispatchAwakeForNewScene` — score **48.4**, dominant **cognitive**, tags `[long-function, deep-nesting, hard-to-follow, nesting-hell]`.
   - **Why it hurts:** depth 5 inside 97 LOC — only function in the top 10 tagged `nesting-hell`.
   - **Suggested first move:** invert guards into early returns at the outer loop, then extract the deepest block (per-component awake dispatch) into its own helper.

---

## 3. Tag distribution (what shape is the problem?)

Across the top-50 `function_hotspots`:

- `long-function`: 27
- `hard-to-follow`: 21
- `complex-branching`: 21
- `deep-nesting`: 3
- `nesting-hell`: 3
- `many-params`: 2
- `many-returns`: 1
- `very-long-function`: 1
- `big-switch`: 1

Dominant signal across the same top-50:

- `cognitive`: 44
- `cyclomatic`: 6

The engine bleeds on the **cognitive** axis: the hotspots are mostly long, branchy, hard-to-follow imperative bodies — not big switch statements, not parameter-explosion, and only a handful of true nesting-hell cases. The cure is mostly extraction and flattening rather than dispatch tables or polymorphism.

---

## 4. God files

| Path | LOC | Funcs | fan_in | fan_out | worst_func | priority |
|------|----:|-----:|-------:|--------:|-----------:|---------:|
| [EntityComponent/Zenith_SceneData.cpp](Zenith/EntityComponent/Zenith_SceneData.cpp) | 737 | 45 | 0 | 12 | 48.4 | **72.3** |
| [Flux/RenderGraph/Flux_RenderGraph.cpp](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp) | 1476 | 91 | 0 | 7 | 51.2 | **71.4** |
| [AI/Squad/Zenith_Squad.cpp](Zenith/AI/Squad/Zenith_Squad.cpp) | 788 | 59 | 0 | 9 | 49.6 | 69.1 |
| [Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 1247 | 52 | 0 | 39 | 48.1 | 68.7 |
| [Physics/Zenith_Physics.cpp](Zenith/Physics/Zenith_Physics.cpp) | 580 | 41 | 0 | 12 | 46.8 | 68.3 |
| [Vulkan/Zenith_Vulkan_CommandBuffer.cpp](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 772 | 44 | 0 | 10 | 43.9 | 67.4 |
| [Vulkan/Zenith_Vulkan_Pipeline.cpp](Zenith/Vulkan/Zenith_Vulkan_Pipeline.cpp) | 641 | 42 | 0 | 7 | 38.6 | 65.8 |
| [EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 921 | 135 | 0 | 27 | 37.9 | 65.6 |
| [Vulkan/Zenith_Vulkan.cpp](Zenith/Vulkan/Zenith_Vulkan.cpp) | 1168 | 59 | 0 | 17 | 35.7 | 65.0 |
| [Flux/MeshAnimation/Flux_AnimationStateMachine.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 806 | 63 | 0 | 4 | 35.0 | 64.8 |
| [Flux/MeshAnimation/Flux_BlendTree.cpp](Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp) | 662 | 66 | 0 | 4 | 34.5 | 64.6 |
| [Vulkan/Zenith_Vulkan_MemoryManager.cpp](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 1621 | 75 | 0 | 11 | 31.7 | 63.8 |
| [EntityComponent/Components/Zenith_AnimatorComponent.cpp](Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 513 | 46 | 0 | 11 | 31.1 | 63.6 |
| [Flux/MeshAnimation/Flux_AnimationClip.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp) | 592 | 46 | 0 | 6 | 25.3 | 61.8 |
| [Core/Memory/Zenith_MemoryManagement.cpp](Zenith/Core/Memory/Zenith_MemoryManagement.cpp) | 470 | 41 | 0 | 7 | 30.3 | 61.6 |
| [Collections/Zenith_HashMap.h](Zenith/Collections/Zenith_HashMap.h) | 525 | 41 | 5 | 3 | 23.8 | 61.4 |
| [EntityComponent/Internal/Zenith_SceneCallbackBus.cpp](Zenith/EntityComponent/Internal/Zenith_SceneCallbackBus.cpp) | 311 | 46 | 0 | 4 | 22.3 | 60.9 |
| [Collections/Zenith_Vector.h](Zenith/Collections/Zenith_Vector.h) | 397 | 41 | 46 | 3 | 20.9 | 58.8 |
| [EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) | 533 | 63 | 40 | 7 | 19.1 | 58.2 |
| [Editor/Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) | 1155 | 107 | 0 | 18 | 44.5 | 45.7 |
| [UI/Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h) | 196 | 53 | 11 | 4 | 14.9 | 29.6 |
| [Flux/MeshAnimation/Flux_BlendTree.h](Zenith/Flux/MeshAnimation/Flux_BlendTree.h) | 237 | 42 | 3 | 3 | 2.2 | 11.7 |
| [UI/Zenith_UIButton.h](Zenith/UI/Zenith_UIButton.h) | 112 | 43 | 5 | 4 | 5.0 | 10.8 |

---

## 5. Directory risk concentration

Top directories by count of `function_hotspots`:

| Directory | Hotspot count | Avg score |
|-----------|--------------:|----------:|
| Editor | 6 | 44.3 |
| Flux/RenderGraph | 5 | 46.3 |
| AI/Navigation | 5 | 45.7 |
| EntityComponent/Components | 5 | 44.9 |
| Editor/Panels | 4 | 46.4 |
| UI | 3 | 48.2 |
| Flux/MeshGeometry | 3 | 46.9 |
| EntityComponent/Internal | 3 | 47.5 |
| Flux/MeshAnimation | 3 | 45.3 |
| Flux/Terrain | 2 | 46.0 |

---

## 6. Near-duplicate function clusters

Read from `duplicate_clusters` (Jaccard ≥ 85% on token-normalised 5-gram shingles). 4 clusters total.

**Cluster 1** (5 members, similarity ≥ 85%):
- [Core/Zenith_TestFramework.h:185](Zenith/Core/Zenith_TestFramework.h#L185) — `Zenith_TestAssertEq` — score 15.8
- [Core/Zenith_TestFramework.h:213](Zenith/Core/Zenith_TestFramework.h#L213) — `Zenith_TestAssertGt` — score 15.8
- [Core/Zenith_TestFramework.h:227](Zenith/Core/Zenith_TestFramework.h#L227) — `Zenith_TestAssertLt` — score 15.8
- [Core/Zenith_TestFramework.h:241](Zenith/Core/Zenith_TestFramework.h#L241) — `Zenith_TestAssertGe` — score 15.8
- [Core/Zenith_TestFramework.h:255](Zenith/Core/Zenith_TestFramework.h#L255) — `Zenith_TestAssertLe` — score 15.8
  - **Suggested action:** collapse the five comparison-assert variants into one parameterised macro/template that takes a comparison op, leaving the public names as thin wrappers.

**Cluster 2** (2 members, similarity ≥ 85%):
- [Vulkan/Zenith_Vulkan.cpp:1446](Zenith/Vulkan/Zenith_Vulkan.cpp#L1446) — `Zenith_Vulkan::ConvertToVkFormat_Colour` — score 24.8
- [Vulkan/Zenith_Vulkan.cpp:1535](Zenith/Vulkan/Zenith_Vulkan.cpp#L1535) — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` — score 18.7
  - **Suggested action:** unify the two switch-based format mappers behind one `enum -> VkFormat` lookup table, with the two callers narrowing to the relevant enum subset.

**Cluster 3** (2 members, similarity ≥ 85%):
- [Core/Zenith_Tween.cpp:39](Zenith/Core/Zenith_Tween.cpp#L39) — `Zenith_GetEasingTypeName` — score 23.1
- [Flux/Slang/Flux_CodeGenerator.cpp:64](Zenith/Flux/Slang/Flux_CodeGenerator.cpp#L64) — `ResourceKindName` — score 16.8
  - **Suggested action:** these are independent enum-name-stringifiers in unrelated subsystems — leave as-is or generate via a small `X-macro`/table helper if either one grows.

**Cluster 4** (2 members, similarity ≥ 85%):
- [Flux/RenderGraph/Flux_RenderGraph.cpp:35](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L35) — `IsReadAccess` — score 15.2
- [Flux/RenderGraph/Flux_RenderGraph.cpp:56](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L56) — `IsWriteAccess` — score 15.2
  - **Suggested action:** factor the access-bit predicate into one `HasAccess(value, mask)` helper called by both, with named read/write masks.

---

## 7. Include coupling & cycles

- **Edge confidence:** 2090 relative edges vs 20 basename-only (0.9% basename) — confidence is high; cycle detection is reliable across the graph.
- **Cycles:** none. No include cycles detected.

**High fan-in headers** (most-included; ripple widest on change):

| File | fan_in | fan_out |
|------|------:|-------:|
| [Core/Zenith.h](Zenith/Core/Zenith.h) | 197 | 11 |
| [Flux/Flux.h](Zenith/Flux/Flux.h) | 72 | 6 |
| [Core/Memory/Zenith_MemoryManagement_Disabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Disabled.h) | 66 | 0 |
| [DataStream/Zenith_DataStream.h](Zenith/DataStream/Zenith_DataStream.h) | 64 | 1 |
| [Core/Memory/Zenith_MemoryManagement_Enabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Enabled.h) | 61 | 0 |

**High fan-out files** (include the most; broadest dependency surface):

| File | fan_in | fan_out |
|------|------:|-------:|
| [Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 0 | 39 |
| [Flux/Flux.cpp](Zenith/Flux/Flux.cpp) | 0 | 31 |
| [EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 0 | 27 |
| [Core/Zenith_Core.cpp](Zenith/Core/Zenith_Core.cpp) | 0 | 26 |
| [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp) | 0 | 22 |

---

## 8. Low-cohesion classes

> Caveat: every record is `header-inline only` — LCOM5 here is computed from inline-defined methods only, so this is a header-inline cohesion signal, not a full-class cohesion score. Treat values cautiously.

Top 10 (by LCOM5 desc):

| Class | File:line | LCOM5 | Methods | Fields |
|-------|-----------|------:|--------:|-------:|
| `Zenith_Vulkan` | [Vulkan/Zenith_Vulkan.h:103](Zenith/Vulkan/Zenith_Vulkan.h#L103) | 1.000 | 12 | 4 |
| `Zenith_Vulkan_MemoryManager` | [Vulkan/Zenith_Vulkan_MemoryManager.h:34](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.h#L34) | 1.000 | 12 | 31 |
| `Flux_Gizmos` | [Flux/Gizmos/Flux_Gizmos.h:36](Zenith/Flux/Gizmos/Flux_Gizmos.h#L36) | 1.000 | 4 | 5 |
| `Flux_TerrainStreamingManager` | [Flux/Terrain/Flux_TerrainStreamingManager.h:183](Zenith/Flux/Terrain/Flux_TerrainStreamingManager.h#L183) | 1.000 | 4 | 12 |
| `Zenith_SceneOperationQueue` | [EntityComponent/Internal/Zenith_SceneOperationQueue.h:36](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.h#L36) | 1.000 | 3 | 25 |
| `Zenith_AssetRegistry` | [AssetHandling/Zenith_AssetRegistry.h:81](Zenith/AssetHandling/Zenith_AssetRegistry.h#L81) | 0.980 | 10 | 5 |
| `Flux_RenderGraph` | [Flux/RenderGraph/Flux_RenderGraph.h:293](Zenith/Flux/RenderGraph/Flux_RenderGraph.h#L293) | 0.976 | 6 | 35 |
| `Zenith_Profiling` | [Profiling/Zenith_Profiling.h:196](Zenith/Profiling/Zenith_Profiling.h#L196) | 0.970 | 3 | 11 |
| `Flux_RenderGraph_Pass` | [Flux/RenderGraph/Flux_RenderGraph.h:130](Zenith/Flux/RenderGraph/Flux_RenderGraph.h#L130) | 0.963 | 3 | 18 |
| `Zenith_SceneData` | [EntityComponent/Zenith_SceneData.h:51](Zenith/EntityComponent/Zenith_SceneData.h#L51) | 0.962 | 51 | 36 |

---

## 9. Tech-debt markers

After filtering hash-map placeholders.

- TODO: 14
- FIXME: 0
- HACK: 0
- XXX: 0

Top markers (FIXME > HACK ≈ XXX > TODO, then file/line):

- [Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp#L162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor/Zenith_UndoSystem.cpp:352](Zenith/Editor/Zenith_UndoSystem.cpp#L352) **TODO** Serialize full entity state (all components)
- [Editor/Zenith_UndoSystem.cpp:404](Zenith/Editor/Zenith_UndoSystem.cpp#L404) **TODO** Deserialize full entity state
- [EntityComponent/Zenith_SceneData.cpp:179](Zenith/EntityComponent/Zenith_SceneData.cpp#L179) **TODO** auto-promote same-scene self-unload to async
- [EntityComponent/Zenith_SceneManager.cpp:1009](Zenith/EntityComponent/Zenith_SceneManager.cpp#L1009) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting
- [Flux/Flux.h:105](Zenith/Flux/Flux.h#L105) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Flux/Flux.h:114](Zenith/Flux/Flux.h#L114) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Flux/Flux_CommandList.h:89](Zenith/Flux/Flux_CommandList.h#L89) **TODO** elide second memcpy by having AddCommand record directly into the buffer
- [Flux/IBL/Flux_IBL.cpp:543](Zenith/Flux/IBL/Flux_IBL.cpp#L543) **TODO** create per-face 2D SRVs for cubemap debug display
- [Flux/MeshAnimation/Flux_AnimationController.cpp:178](Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp#L178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- [Flux/MeshAnimation/Flux_AnimationController.h:293](Zenith/Flux/MeshAnimation/Flux_AnimationController.h#L293) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Flux/Shadows/Flux_Shadows.cpp:154](Zenith/Flux/Shadows/Flux_Shadows.cpp#L154) **TODO** Enable terrain shadow casting
- [Vulkan/Zenith_Vulkan.cpp:1388](Zenith/Vulkan/Zenith_Vulkan.cpp#L1388) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker pool is exhausted
- [Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp#L411) **TODO** expose DONT_CARE LoadOp as a third option

---

## 10. Headline numbers

Files **423** · SLOC **80,016** · functions **5,263** · avg MI **30.46** · min MI **0.00** · avg CC **20.16** · max CC **289** · est bugs **1153.58**

---

## 11. Appendix — top-20 hotspots

| # | File:line | Name | Score | Dominant | Tags | CC | Cog | Nest | LOC |
|--:|-----------|------|------:|----------|------|---:|----:|----:|----:|
| 1 | [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:118](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp#L118) | `Flux_AnimatedMeshes::ExecuteGBuffer` | 52.4 | cognitive | long-function, hard-to-follow | 13 | 29 | 4 | 97 |
| 2 | [UI/Zenith_UICanvas.cpp:203](Zenith/UI/Zenith_UICanvas.cpp#L203) | `Zenith_UICanvas::Update` | 52.0 | cognitive | complex-branching, hard-to-follow | 15 | 45 | 2 | 43 |
| 3 | [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp#L262) | `RenderCategoryTab` | 51.7 | cognitive | long-function, hard-to-follow | 11 | 34 | 4 | 94 |
| 4 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1172](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L1172) | `Flux_RenderGraph::SynthesizeAliasingBarriers` | 51.2 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 94 |
| 5 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:297](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp#L297) | `Flux_MeshGeometry::Combine` | 51.0 | cognitive | long-function, complex-branching, many-returns | 20 | 24 | 3 | 84 |
| 6 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:844](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp#L844) | `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` | 50.0 | cognitive | long-function, hard-to-follow | 13 | 25 | 3 | 172 |
| 7 | [AI/Squad/Zenith_Squad.cpp:719](Zenith/AI/Squad/Zenith_Squad.cpp#L719) | `Zenith_Squad::AssignFormationSlots` | 49.6 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 28 | 3 | 81 |
| 8 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1589](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp#L1589) | `Flux_RenderGraph::AssignClearFlags` | 49.5 | cognitive | complex-branching, hard-to-follow | 18 | 32 | 3 | 40 |
| 9 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:714](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp#L714) | `Zenith_NavMeshGenerator::EmitQuadsFromSpans` | 48.6 | cognitive | hard-to-follow | 9 | 32 | 4 | 61 |
| 10 | [EntityComponent/Zenith_SceneData.cpp:814](Zenith/EntityComponent/Zenith_SceneData.cpp#L814) | `Zenith_SceneData::DispatchAwakeForNewScene` | 48.4 | cognitive | long-function, deep-nesting, hard-to-follow, nesting-hell | 11 | 26 | 5 | 97 |
| 11 | [Editor/Zenith_Editor.cpp:368](Zenith/Editor/Zenith_Editor.cpp#L368) | `Zenith_Editor::Update` | 48.1 | cognitive | long-function, complex-branching, hard-to-follow | 16 | 25 | 3 | 114 |
| 12 | [EntityComponent/Components/Zenith_TerrainComponent.cpp:551](Zenith/EntityComponent/Components/Zenith_TerrainComponent.cpp#L551) | `Zenith_TerrainComponent::LoadAndCombineLowLODChunks` | 47.7 | cognitive | long-function | 12 | 22 | 3 | 116 |
| 13 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:311](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp#L311) | `Zenith_SceneOperationQueue::ProcessPendingAsyncLoads` | 47.7 | cognitive | long-function, hard-to-follow | 13 | 30 | 3 | 91 |
| 14 | [Flux/MeshAnimation/Flux_BonePose.cpp:199](Zenith/Flux/MeshAnimation/Flux_BonePose.cpp#L199) | `Flux_SkeletonPose::SampleFromClip` | 47.3 | cognitive | hard-to-follow | 11 | 29 | 3 | 68 |
| 15 | [Editor/Zenith_Editor_Menu.cpp:50](Zenith/Editor/Zenith_Editor_Menu.cpp#L50) | `Zenith_Editor::RenderFileMenu` | 47.1 | cognitive | long-function, hard-to-follow | 12 | 27 | 4 | 91 |
| 16 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:419](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp#L419) | `Flux_MeshGeometry::GenerateLayoutAndVertexData` | 47.0 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 25 | 2 | 110 |
| 17 | [Flux/Terrain/Flux_TerrainStreamingManager.cpp:388](Zenith/Flux/Terrain/Flux_TerrainStreamingManager.cpp#L388) | `Flux_TerrainStreamingManager::RequestNearbyHighLOD` | 46.9 | cognitive | hard-to-follow | 12 | 27 | 3 | 72 |
| 18 | [Physics/Zenith_Physics.cpp:584](Zenith/Physics/Zenith_Physics.cpp#L584) | `Zenith_Physics::LockRotation` | 46.8 | cognitive | hard-to-follow | 13 | 27 | 3 | 43 |
| 19 | [UI/Zenith_UILayoutGroup.cpp:268](Zenith/UI/Zenith_UILayoutGroup.cpp#L268) | `Zenith_UILayoutGroup::PlaceChild` | 46.8 | cognitive | complex-branching | 18 | 22 | 3 | 71 |
| 20 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:871](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp#L871) | `Zenith_NavMeshGenerator::MergeOverlappingSpans` | 46.2 | cognitive | deep-nesting, hard-to-follow, nesting-hell | 11 | 26 | 5 | 51 |
