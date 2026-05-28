# Zenith Engine — Code Health Snapshot

**Date:** 2026-05-27
**Parser:** tree-sitter (requested: auto) · **Profile:** engine-ci

## 1. Executive summary

The engine carries a mid-sized population of high-cognitive-load functions concentrated in AI navigation, scene-operation orchestration, and the render-graph; nesting depths are mostly modest, but bodies are long and branchy, and one Editor-side header cycle plus eight near-duplicate clusters round out the structural concerns.

- Parser backend confirmed: **tree-sitter**. Include edges resolved by relative path: **2012**; by basename fallback: **2** (≈0.1% — below 10%, cycle confidence is high).
- Function hotspots at priority_score **≥80: 1** · **≥70: 1** · **≥60: 4**.
- High-priority files: **13** (2.9% of files). **P90 file priority: 66.1**.
- Worst function: [Zenith/AI/Navigation/Zenith_NavMesh.cpp:782](Zenith/AI/Navigation/Zenith_NavMesh.cpp:782) — `Zenith_NavMesh::GetRandomReachablePointInRadius` (82.5).
- Worst file: [Zenith/AI/Navigation/Zenith_NavMesh.cpp](Zenith/AI/Navigation/Zenith_NavMesh.cpp) (82.5).
- Files tagged `god-file`: **25**.
- Tech-debt markers (after filtering hash-map placeholders): **13** — all **TODO** (FIXME 0, HACK 0, XXX 0).
- Highest hotspot concentration: `AI/Navigation` (8 of the top-50 hotspots).
- Near-duplicate function clusters: **8**.
- Include cycles: **1** — between [Zenith/Editor/Zenith_Editor.h](Zenith/Editor/Zenith_Editor.h) and [Zenith/Editor/Zenith_EditorState.h](Zenith/Editor/Zenith_EditorState.h).

## 2. Top 10 refactoring targets

1. [Zenith/AI/Navigation/Zenith_NavMesh.cpp:782](Zenith/AI/Navigation/Zenith_NavMesh.cpp:782) — `Zenith_NavMesh::GetRandomReachablePointInRadius` — score **82.5**, dominant **cognitive**, tags `[very-long-function, complex-branching, hard-to-follow, many-returns]`.
   - **Why it hurts:** 226-line body with CC 30 and 10 return points — cognitive load comes from interleaved branch+return flow, not deep nesting (depth 3).
   - **Extract candidate line range:** none reported.
   - **Suggested first move:** factor the 10 early-return guards into a single validation helper that returns an enum, then split sampling, reachability, and projection into named sub-steps.

2. [Zenith/Core/Zenith_AutomatedTest.cpp:388](Zenith/Core/Zenith_AutomatedTest.cpp:388) — `Zenith_AutomatedTestRunner::Tick` — score **68.0**, dominant **cognitive**, tags `[very-long-function, complex-branching, hard-to-follow, many-returns, big-switch]`.
   - **Why it hurts:** 267 LOC, CC 48, 10-case switch driving the runner — classic dispatch-table candidate.
   - **Extract candidate line range:** none reported.
   - **Suggested first move:** turn the per-state switch into a state-table lookup with one handler function per state.

3. [Zenith/Flux/MeshAnimation/Flux_InverseKinematics.cpp:287](Zenith/Flux/MeshAnimation/Flux_InverseKinematics.cpp:287) — `Flux_IKSolver::SolveChain` — score **67.9**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 115 LOC with depth-4 nesting and cog 39 — inner blocks doing too much work.
   - **Extract candidate line range:** none reported.
   - **Suggested first move:** extract per-joint iteration body into `SolveSingleJoint(...)` and flatten the outer loop.

4. [Zenith/UI/Zenith_UIText.cpp:200](Zenith/UI/Zenith_UIText.cpp:200) — `Zenith_UIText::Render` — score **61.6**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 124 LOC, CC 22, cog 37 — likely glyph layout, atlas binding and draw issuance bundled together.
   - **Suggested first move:** split into `LayoutGlyphs() / BuildVertexBuffer() / IssueDrawCalls()`.

5. [Zenith/Core/Zenith_AutomatedTest.cpp:189](Zenith/Core/Zenith_AutomatedTest.cpp:189) — `Zenith_AutomatedTestRunner::ParseCommandLine` — score **59.3**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 89 LOC with CC 19 and cog 39 — long if/else chain on flag names.
   - **Suggested first move:** replace flag-string `if/else` chain with a `{name → handler}` table.

6. [Zenith/Telemetry/Zenith_Telemetry.cpp:459](Zenith/Telemetry/Zenith_Telemetry.cpp:459) — `Reader::ExportJson` — score **57.5**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
   - **Why it hurts:** 156 LOC of nested per-record serialisation.
   - **Suggested first move:** extract `WriteRecord(json, rec)` and `WriteField(...)` helpers.

7. [Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:124](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:124) — `ExecuteGBuffer` — score **52.4**, dominant **cognitive**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 97 LOC, depth-4 nesting — passes-per-instance loop with inline state setup.
   - **Suggested first move:** pull instance-state setup into `BindAnimatedInstance(cmd, mesh)`.

8. [Zenith/AI/Navigation/Zenith_NavMeshAgent.cpp:107](Zenith/AI/Navigation/Zenith_NavMeshAgent.cpp:107) — `Zenith_NavMeshAgent::Update` — score **52.1**, dominant **cognitive**, tags `[long-function, complex-branching]`.
   - **Why it hurts:** 137 LOC mixing replan, steering, and arrival logic.
   - **Suggested first move:** split into `MaybeReplan() / ComputeSteering() / HandleArrival()`.

9. [Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:262) — `RenderCategoryTab` — score **51.7**, dominant **cognitive**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** depth-4 ImGui block tree, cog 34 inside 94 LOC.
   - **Suggested first move:** flatten with early returns; lift each tab section into its own free function.

10. [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1170](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1170) — `Flux_RenderGraph::SynthesizeAliasingBarriers` — score **51.2**, dominant **cognitive**, tags `[long-function, complex-branching, hard-to-follow]`.
    - **Why it hurts:** CC 17, cog 30 across 94 LOC, four return points — barrier-synthesis logic interleaved with access-kind classification.
    - **Suggested first move:** extract the per-resource barrier-decision branch into `ClassifyBarrier(prev, next)` returning a small struct.

## 3. Tag distribution (what shape is the problem?)

Across the top-50 function hotspots:

- `long-function`: 28
- `complex-branching`: 25
- `hard-to-follow`: 24
- `very-long-function`: 4
- `many-returns`: 4
- `big-switch`: 2
- `many-params`: 2
- `deep-nesting`: 1
- `nesting-hell`: 1

**Interpretation:** the dominant pattern is *long, branchy, cognitively heavy* functions — not deep-nested loops and not dispatch tables. Almost every hotspot is "too much logic in one body" rather than "too many levels deep" or "huge switch".

**Dominant signal across the same top-50:** cognitive 45 · cyclomatic 4 · length 1. Cognitive load is overwhelmingly the bleeding axis — the priority queue is driven by interleaved control flow inside long bodies more than by raw branch count.

## 4. God files

25 files carry the `god-file` tag (LOC ≥ 1000 or function count ≥ 40), sorted by file priority:

| File | LOC | Funcs | fan_in | fan_out | Worst func | Priority |
|---|---|---|---|---|---|---|
| [Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp) | 1084 | 54 | 0 | 20 | 50.0 | 74.5 |
| [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp) | 1474 | 91 | 0 | 5 | 51.2 | 71.4 |
| [Zenith/Physics/Zenith_Physics.cpp](Zenith/Physics/Zenith_Physics.cpp) | 619 | 44 | 0 | 6 | 46.8 | 70.0 |
| [Zenith/Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 1234 | 64 | 0 | 41 | 48.1 | 68.7 |
| [Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 772 | 44 | 0 | 8 | 43.9 | 67.4 |
| [Zenith/EntityComponent/Zenith_SceneData.cpp](Zenith/EntityComponent/Zenith_SceneData.cpp) | 741 | 46 | 0 | 9 | 36.3 | 66.9 |
| [Zenith/Vulkan/Zenith_Vulkan_Pipeline.cpp](Zenith/Vulkan/Zenith_Vulkan_Pipeline.cpp) | 635 | 42 | 0 | 5 | 38.6 | 65.8 |
| [Zenith/Vulkan/Zenith_Vulkan.cpp](Zenith/Vulkan/Zenith_Vulkan.cpp) | 1159 | 71 | 0 | 15 | 35.7 | 65.0 |
| [Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 1667 | 85 | 0 | 12 | 35.4 | 64.9 |
| [Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 804 | 63 | 0 | 2 | 35.0 | 64.8 |
| [Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp](Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp) | 660 | 66 | 0 | 2 | 34.5 | 64.6 |
| [Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp](Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 517 | 47 | 0 | 11 | 31.1 | 63.6 |
| [Zenith/AI/Squad/Zenith_Squad.cpp](Zenith/AI/Squad/Zenith_Squad.cpp) | 797 | 62 | 0 | 7 | 28.1 | 62.7 |
| [Zenith/EntityComponent/Internal/Zenith_SceneCallbackBus.cpp](Zenith/EntityComponent/Internal/Zenith_SceneCallbackBus.cpp) | 324 | 47 | 0 | 5 | 25.9 | 62.0 |
| [Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp) | 590 | 46 | 0 | 4 | 25.3 | 61.8 |
| [Zenith/Core/Memory/Zenith_MemoryManagement.cpp](Zenith/Core/Memory/Zenith_MemoryManagement.cpp) | 469 | 41 | 0 | 6 | 30.3 | 61.6 |
| [Zenith/Collections/Zenith_HashMap.h](Zenith/Collections/Zenith_HashMap.h) | 525 | 41 | 5 | 3 | 23.8 | 61.4 |
| [Zenith/Collections/Zenith_Vector.h](Zenith/Collections/Zenith_Vector.h) | 397 | 41 | 45 | 3 | 20.9 | 58.8 |
| [Zenith/EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) | 453 | 53 | 16 | 8 | 19.1 | 58.2 |
| [Zenith/Core/Zenith_Engine.cpp](Zenith/Core/Zenith_Engine.cpp) | 545 | 58 | 0 | 64 | 47.5 | 54.1 |
| [Zenith/Editor/Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) | 1266 | 118 | 0 | 22 | 46.8 | 46.9 |
| [Zenith/EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 331 | 129 | 0 | 23 | 23.5 | 44.3 |
| [Zenith/UI/Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h) | 196 | 53 | 10 | 4 | 14.9 | 29.6 |
| [Zenith/Flux/MeshAnimation/Flux_BlendTree.h](Zenith/Flux/MeshAnimation/Flux_BlendTree.h) | 235 | 42 | 3 | 1 | 2.2 | 11.7 |
| [Zenith/UI/Zenith_UIButton.h](Zenith/UI/Zenith_UIButton.h) | 110 | 43 | 4 | 2 | 5.0 | 10.7 |

## 5. Directory risk concentration

Top directories by count of entries in the top-50 hotspots:

| Directory | Hotspot count | Avg score |
|---|---|---|
| `AI\Navigation` | 8 | 51.3 |
| `EntityComponent\Components` | 6 | 44.6 |
| `Editor\Panels` | 4 | 46.4 |
| `Flux\RenderGraph` | 4 | 47.2 |
| `Core` | 3 | 58.3 |
| `Flux\MeshAnimation` | 3 | 53.4 |
| `UI` | 3 | 51.4 |
| `Flux\MeshGeometry` | 3 | 46.9 |
| `EntityComponent\Internal` | 3 | 47.5 |
| `Editor` | 3 | 47.3 |

## 6. Near-duplicate function clusters

8 clusters survived profile filtering (≥85% Jaccard on token-normalised 5-gram shingles — same control-flow shape, varying names).

- **Cluster 1** (5 members, similarity ≥ 85%):
  - [Zenith/Core/Zenith_TestFramework.h:185](Zenith/Core/Zenith_TestFramework.h:185) — `Zenith_TestAssertEq` — 15.8
  - [Zenith/Core/Zenith_TestFramework.h:213](Zenith/Core/Zenith_TestFramework.h:213) — `Zenith_TestAssertGt` — 15.8
  - [Zenith/Core/Zenith_TestFramework.h:227](Zenith/Core/Zenith_TestFramework.h:227) — `Zenith_TestAssertLt` — 15.8
  - [Zenith/Core/Zenith_TestFramework.h:241](Zenith/Core/Zenith_TestFramework.h:241) — `Zenith_TestAssertGe` — 15.8
  - [Zenith/Core/Zenith_TestFramework.h:255](Zenith/Core/Zenith_TestFramework.h:255) — `Zenith_TestAssertLe` — 15.8
  - **Suggested action:** collapse into a single `Zenith_TestAssertCmp<Op>` template (or a macro that takes the comparator) that diffs only the relational operator.

- **Cluster 2** (4 members, similarity ≥ 85%):
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:260](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:260) — `Zenith_ComponentMetaRegistry::DispatchOnAwake` — 15.1
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:291](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:291) — `Zenith_ComponentMetaRegistry::DispatchOnEnable` — 15.1
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:307](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:307) — `Zenith_ComponentMetaRegistry::DispatchOnDisable` — 15.1
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:276](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:276) — `Zenith_ComponentMetaRegistry::DispatchOnStart` — 15.0
  - **Suggested action:** introduce a `DispatchLifecycle(pfnHook)` helper that takes the per-component callback pointer; the four bodies differ only in which hook they call.

- **Cluster 3** (3 members, similarity ≥ 85%):
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:330](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:330) — `Zenith_ComponentMetaRegistry::DispatchOnUpdate` — 16.0
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:341](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:341) — `Zenith_ComponentMetaRegistry::DispatchOnLateUpdate` — 16.0
  - [Zenith/EntityComponent/Zenith_ComponentMeta.cpp:352](Zenith/EntityComponent/Zenith_ComponentMeta.cpp:352) — `Zenith_ComponentMetaRegistry::DispatchOnFixedUpdate` — 16.0
  - **Suggested action:** same as cluster 2 — fold the three tick variants into one helper parameterised by hook pointer.

- **Cluster 4** (2 members, similarity ≥ 85%):
  - [Zenith/Vulkan/Zenith_Vulkan.cpp:1442](Zenith/Vulkan/Zenith_Vulkan.cpp:1442) — `Zenith_Vulkan::ConvertToVkFormat_Colour` — 24.8
  - [Zenith/Vulkan/Zenith_Vulkan.cpp:1531](Zenith/Vulkan/Zenith_Vulkan.cpp:1531) — `Zenith_Vulkan::ShaderDataTypeToVulkanFormat` — 18.7
  - **Suggested action:** both are big format-mapping switches — back them with a shared `static constexpr` lookup table keyed by the source enum.

- **Cluster 5** (2 members, similarity ≥ 85%):
  - [Zenith/EntityComponent/Zenith_SceneManager.cpp:304](Zenith/EntityComponent/Zenith_SceneManager.cpp:304) — `Zenith_SceneManager::LoadSceneAsyncByIndex` — 23.5
  - [Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:1306](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:1306) — `Zenith_SceneOperationQueue::LoadSceneAsyncByIndex` — 22.8
  - **Suggested action:** the SceneManager entry-point should be a thin forwarder into the queue method; lift the shared validation/queuing into one body.

- **Cluster 6** (2 members, similarity ≥ 85%):
  - [Zenith/Core/Zenith_Tween.cpp:39](Zenith/Core/Zenith_Tween.cpp:39) — `Zenith_GetEasingTypeName` — 23.1
  - [Zenith/Flux/Slang/Flux_CodeGenerator.cpp:62](Zenith/Flux/Slang/Flux_CodeGenerator.cpp:62) — `ResourceKindName` — 16.8
  - **Suggested action:** both are enum-to-string switches; consider a small `ENUM_TO_STRING_CASES(...)` macro, or keep them separate and ignore — different domains.

- **Cluster 7** (2 members, similarity ≥ 85%):
  - [Zenith/EntityComponent/Zenith_SceneManager.cpp:203](Zenith/EntityComponent/Zenith_SceneManager.cpp:203) — `Zenith_SceneManager::LoadSceneAsync_OLD` — 21.2
  - [Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:1222](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:1222) — `Zenith_SceneOperationQueue::LoadSceneAsync` — 20.0
  - **Suggested action:** the `_OLD` suffix signals an in-progress migration — finish removing the SceneManager copy once callers have moved to the queue path.

- **Cluster 8** (2 members, similarity ≥ 85%):
  - [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:33](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:33) — `IsReadAccess` — 15.2
  - [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:54](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:54) — `IsWriteAccess` — 15.2
  - **Suggested action:** unify into one `ClassifyAccess(access) -> {read|write|both}` returning a small bitset, or back both with a shared lookup table indexed by `RESOURCE_ACCESS_*`.

## 7. Include coupling & cycles

**Edge confidence:** 2012 includes resolved by relative path vs **2 by basename fallback** (≈0.1%) — cycle and fan-in numbers are high-confidence.

**Cycles:** 1 detected.
- `Zenith/Editor/Zenith_Editor.h` ↔ `Zenith/Editor/Zenith_EditorState.h` — break by forward-declaring the type the cycle hinges on (likely the `EditorState` enum or struct).

**High fan-in headers (most-included):**

| File | fan_in | fan_out |
|---|---|---|
| [Zenith/Core/Zenith.h](Zenith/Core/Zenith.h) | 207 | 13 |
| [Zenith/Core/Memory/Zenith_MemoryManagement_Disabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Disabled.h) | 60 | 0 |
| [Zenith/Flux/Flux.h](Zenith/Flux/Flux.h) | 59 | 4 |
| [Zenith/Core/Memory/Zenith_MemoryManagement_Enabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Enabled.h) | 58 | 0 |
| [Zenith/Maths/Zenith_Maths.h](Zenith/Maths/Zenith_Maths.h) | 52 | 0 |

**High fan-out files (include the most):**

| File | fan_in | fan_out |
|---|---|---|
| [Zenith/Core/Zenith_Engine.cpp](Zenith/Core/Zenith_Engine.cpp) | 0 | 64 |
| [Zenith/Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 0 | 41 |
| [Zenith/Flux/Flux.cpp](Zenith/Flux/Flux.cpp) | 0 | 35 |
| [Zenith/Core/Zenith_Core.cpp](Zenith/Core/Zenith_Core.cpp) | 0 | 24 |
| [Zenith/EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 0 | 23 |

## 8. Low-cohesion classes

> **Caveat:** every record below carries `header-inline only` — LCOM5 is computed from inline-defined methods in the header, not the full class. Treat these as header-shape signals, not full-class cohesion verdicts.

Top 10 lowest-cohesion classes:

| Class | File:line | LCOM5 | Methods | Fields |
|---|---|---|---|---|
| `Zenith_EditorAutomation` | [Zenith/Editor/Zenith_EditorAutomation.h:213](Zenith/Editor/Zenith_EditorAutomation.h:213) | 1.000 | 6 | 4 |
| `Zenith_SceneOperationQueue` | [Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.h:36](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.h:36) | 0.991 | 3 | 36 |
| `Flux_TerrainStreamingManagerImpl` | [Zenith/Flux/Terrain/Flux_TerrainStreamingManagerImpl.h:146](Zenith/Flux/Terrain/Flux_TerrainStreamingManagerImpl.h:146) | 0.983 | 4 | 15 |
| `Zenith_Profiling` | [Zenith/Profiling/Zenith_Profiling.h:230](Zenith/Profiling/Zenith_Profiling.h:230) | 0.982 | 3 | 19 |
| `Zenith_AssetRegistry` | [Zenith/AssetHandling/Zenith_AssetRegistry.h:83](Zenith/AssetHandling/Zenith_AssetRegistry.h:83) | 0.980 | 10 | 5 |
| `Flux_RenderGraph` | [Zenith/Flux/RenderGraph/Flux_RenderGraph.h:293](Zenith/Flux/RenderGraph/Flux_RenderGraph.h:293) | 0.976 | 6 | 35 |
| `Flux_RenderGraph_Pass` | [Zenith/Flux/RenderGraph/Flux_RenderGraph.h:130](Zenith/Flux/RenderGraph/Flux_RenderGraph.h:130) | 0.963 | 3 | 18 |
| `Zenith_Input` | [Zenith/Input/Zenith_Input.h:13](Zenith/Input/Zenith_Input.h:13) | 0.963 | 3 | 9 |
| `Zenith_UIElement` | [Zenith/UI/Zenith_UIElement.h:79](Zenith/UI/Zenith_UIElement.h:79) | 0.958 | 52 | 29 |
| `Flux_TerrainImpl` | [Zenith/Flux/Terrain/Flux_TerrainImpl.h:13](Zenith/Flux/Terrain/Flux_TerrainImpl.h:13) | 0.955 | 3 | 22 |

## 9. Tech-debt markers

After filtering hash-map placeholders:

- **By type:** TODO 13 · FIXME 0 · HACK 0 · XXX 0.

Top markers (severity order — none above TODO today, listed by file/line):

- [Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Zenith/Editor/Zenith_UndoSystem.cpp:355](Zenith/Editor/Zenith_UndoSystem.cpp:355) **TODO** Serialize full entity state (all components)
- [Zenith/Editor/Zenith_UndoSystem.cpp:407](Zenith/Editor/Zenith_UndoSystem.cpp:407) **TODO** Deserialize full entity state
- [Zenith/EntityComponent/Zenith_SceneData.cpp:178](Zenith/EntityComponent/Zenith_SceneData.cpp:178) **TODO** auto-promote same-scene self-unload to async.");
- [Zenith/Flux/Flux.h:79](Zenith/Flux/Flux.h:79) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
- [Zenith/Flux/Flux.h:88](Zenith/Flux/Flux.h:88) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Zenith/Flux/Flux_CommandList.h:88](Zenith/Flux/Flux_CommandList.h:88) **TODO** elide second memcpy by having AddCommand record directly into the
- [Zenith/Flux/IBL/Flux_IBL.cpp:517](Zenith/Flux/IBL/Flux_IBL.cpp:517) **TODO** create per-face 2D SRVs for cubemap debug display.
- [Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:172](Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:172) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED when engine time scale support is added
- [Zenith/Flux/MeshAnimation/Flux_AnimationController.h:295](Zenith/Flux/MeshAnimation/Flux_AnimationController.h:295) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Zenith/Flux/Shadows/Flux_Shadows.cpp:152](Zenith/Flux/Shadows/Flux_Shadows.cpp:152) **TODO** Enable terrain shadow casting
- [Zenith/Vulkan/Zenith_Vulkan.cpp:1376](Zenith/Vulkan/Zenith_Vulkan.cpp:1376) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker
- [Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:411) **TODO** expose DONT_CARE LoadOp as a third option. Today a pass that

## 10. Headline numbers

`Files: 447 · SLOC: 84,041 · functions: 5,534 · avg MI: 30.56 · min MI: 0.00 · avg CC: 20.16 · max CC: 289 · est bugs: 1,285.55`

## 11. Appendix — top-20 hotspots

| # | file:line | name | score | dominant | tags | CC | cog | nesting | LOC |
|---|---|---|---|---|---|---|---|---|---|
| 1 | [AI/Navigation/Zenith_NavMesh.cpp:782](Zenith/AI/Navigation/Zenith_NavMesh.cpp:782) | `Zenith_NavMesh::GetRandomReachablePointInRadius` | 82.5 | cognitive | very-long-function, complex-branching, hard-to-follow, many-returns | 30 | 53 | 3 | 226 |
| 2 | [Core/Zenith_AutomatedTest.cpp:388](Zenith/Core/Zenith_AutomatedTest.cpp:388) | `Zenith_AutomatedTestRunner::Tick` | 68.0 | cognitive | very-long-function, complex-branching, hard-to-follow, many-returns, big-switch | 48 | 34 | 1 | 267 |
| 3 | [Flux/MeshAnimation/Flux_InverseKinematics.cpp:287](Zenith/Flux/MeshAnimation/Flux_InverseKinematics.cpp:287) | `Flux_IKSolver::SolveChain` | 67.9 | cognitive | long-function, complex-branching, hard-to-follow | 18 | 39 | 4 | 115 |
| 4 | [UI/Zenith_UIText.cpp:200](Zenith/UI/Zenith_UIText.cpp:200) | `Zenith_UIText::Render` | 61.6 | cognitive | long-function, complex-branching, hard-to-follow | 22 | 37 | 2 | 124 |
| 5 | [Core/Zenith_AutomatedTest.cpp:189](Zenith/Core/Zenith_AutomatedTest.cpp:189) | `Zenith_AutomatedTestRunner::ParseCommandLine` | 59.3 | cognitive | long-function, complex-branching, hard-to-follow | 19 | 39 | 2 | 89 |
| 6 | [Telemetry/Zenith_Telemetry.cpp:459](Zenith/Telemetry/Zenith_Telemetry.cpp:459) | `Reader::ExportJson` | 57.5 | cognitive | long-function, complex-branching, hard-to-follow | 16 | 30 | 3 | 156 |
| 7 | [Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:124](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:124) | `ExecuteGBuffer` | 52.4 | cognitive | long-function, hard-to-follow | 13 | 29 | 4 | 97 |
| 8 | [AI/Navigation/Zenith_NavMeshAgent.cpp:107](Zenith/AI/Navigation/Zenith_NavMeshAgent.cpp:107) | `Zenith_NavMeshAgent::Update` | 52.1 | cognitive | long-function, complex-branching | 16 | 23 | 3 | 137 |
| 9 | [Editor/Panels/Zenith_EditorPanel_Memory.cpp:262](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:262) | `RenderCategoryTab` | 51.7 | cognitive | long-function, hard-to-follow | 11 | 34 | 4 | 94 |
| 10 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1170](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1170) | `Flux_RenderGraph::SynthesizeAliasingBarriers` | 51.2 | cognitive | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 94 |
| 11 | [Flux/MeshGeometry/Flux_MeshGeometry.cpp:294](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp:294) | `Flux_MeshGeometry::Combine` | 51.0 | cognitive | long-function, complex-branching, many-returns | 20 | 24 | 3 | 84 |
| 12 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:932](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:932) | `Zenith_SceneOperationQueue::ProcessPendingAsyncUnloads` | 50.0 | cognitive | long-function, hard-to-follow | 13 | 25 | 3 | 172 |
| 13 | [Flux/RenderGraph/Flux_RenderGraph.cpp:1587](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1587) | `Flux_RenderGraph::AssignClearFlags` | 49.5 | cognitive | complex-branching, hard-to-follow | 18 | 32 | 3 | 40 |
| 14 | [AI/Navigation/Zenith_NavMeshGenerator.cpp:856](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:856) | `Zenith_NavMeshGenerator::EmitQuadsFromSpans` | 48.6 | cognitive | hard-to-follow | 9 | 32 | 4 | 62 |
| 15 | [Editor/Zenith_Editor.cpp:333](Zenith/Editor/Zenith_Editor.cpp:333) | `Zenith_Editor::Update` | 48.1 | cognitive | long-function, complex-branching, hard-to-follow | 16 | 25 | 3 | 114 |
| 16 | [EntityComponent/Components/Zenith_TerrainComponent.cpp:548](Zenith/EntityComponent/Components/Zenith_TerrainComponent.cpp:548) | `Zenith_TerrainComponent::LoadAndCombineLowLODChunks` | 47.7 | cognitive | long-function | 12 | 22 | 3 | 116 |
| 17 | [EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:399](Zenith/EntityComponent/Internal/Zenith_SceneOperationQueue.cpp:399) | `Zenith_SceneOperationQueue::ProcessPendingAsyncLoads` | 47.7 | cognitive | long-function, hard-to-follow | 13 | 30 | 3 | 91 |
| 18 | [Core/Zenith_Engine.cpp:314](Zenith/Core/Zenith_Engine.cpp:314) | `Zenith_Engine::Initialise` | 47.5 | cognitive | very-long-function, complex-branching | 15 | 20 | 2 | 348 |
| 19 | [Flux/MeshAnimation/Flux_BonePose.cpp:196](Zenith/Flux/MeshAnimation/Flux_BonePose.cpp:196) | `Flux_SkeletonPose::SampleFromClip` | 47.3 | cognitive | hard-to-follow | 11 | 29 | 3 | 68 |
| 20 | [Editor/Zenith_Editor_Menu.cpp:51](Zenith/Editor/Zenith_Editor_Menu.cpp:51) | `Zenith_Editor::RenderFileMenu` | 47.1 | cognitive | long-function, hard-to-follow | 12 | 27 | 4 | 91 |
