# Zenith Engine — Code Health Snapshot — 2026-04-19

## 1. Executive summary

- **Overall rating:** **Poor (difficult to maintain).** The engine carries a large
  body of dense, branch-heavy functions and several oversized translation units;
  maintainability index sits well below a comfortable range.
- **Function hotspots by priority_score:** 3 at ≥80, 14 at ≥70, 35 at ≥60.
- **God files:** 23.
- **Tech-debt markers (after filtering hash-map placeholders):** 27 total —
  TODO: 27, FIXME: 0, HACK: 0, XXX: 0.
- **Directory with highest hotspot concentration (sub-dir, top-1):**
  `Editor/Panels` with 6 hotspots (avg score 65.97). Top-level dir is `Flux`
  with 17 of 50 hotspots.

## 2. Top 10 refactoring targets

1. [Flux/RenderGraph/Flux_RenderGraph.cpp:328](Flux/RenderGraph/Flux_RenderGraph.cpp:328) — `Flux_RenderGraph::AddResourceUsage` — score **85.5**, tags `[long-function, complex-branching, hard-to-follow, many-params]`.
   **Why it hurts:** 173 LOC with 8 parameters and 34 branches — a state-routing
   function that should decompose into per-resource-kind helpers.
   **Extract candidate:** none surfaced by the analyzer.
   **Suggested first move:** introduce a `ResourceUsageContext` parameter object
   and split the body by resource type into private helpers.

2. [Flux/Primitives/Flux_Primitives.cpp:687](Flux/Primitives/Flux_Primitives.cpp:687) — `Flux_Primitives::ExecuteGBuffer` — score **80.0**, tags `[very-long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 365 LOC body with cognitive 73 — a monolithic pass that
   mixes setup, per-primitive-type dispatch, and submission.
   **Extract candidate:** none surfaced.
   **Suggested first move:** extract each primitive-type block into a named
   helper and drive them from a small dispatch table.

3. [Profiling/Zenith_Profiling.cpp:347](Profiling/Zenith_Profiling.cpp:347) — `Zenith_Profiling::RenderThreadBreakdown` — score **80.0**, tags `[very-long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 287 LOC of ImGui layout mixing data aggregation and drawing.
   **Extract candidate:** none surfaced.
   **Suggested first move:** separate the data-collection pass from the ImGui
   render pass; extract per-section drawing helpers.

4. [Vulkan/Zenith_Vulkan.cpp:267](Vulkan/Zenith_Vulkan.cpp:267) — `Zenith_Vulkan::RecordCommandBuffersTask` — score **78.5**, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 196 LOC driving per-thread command recording with tangled
   setup/record/end phases.
   **Extract candidate:** none surfaced.
   **Suggested first move:** extract `BeginRecording`, `RecordPasses`,
   `EndRecording` helpers around the existing phase boundaries.

5. [Profiling/Zenith_Profiling.cpp:170](Profiling/Zenith_Profiling.cpp:170) — `Zenith_Profiling::RenderTimelineView` — score **78.3**, tags `[long-function, complex-branching, hard-to-follow, many-params]`.
   **Why it hurts:** 177 LOC with many parameters describing viewport/scroll
   state — a parameter object is overdue.
   **Extract candidate:** none surfaced.
   **Suggested first move:** introduce a `TimelineViewState` struct and split
   the rendering body by row type.

6. [AI/Navigation/Zenith_Pathfinding.cpp:18](AI/Navigation/Zenith_Pathfinding.cpp:18) — `Zenith_Pathfinding::FindPathInternal` — score **77.9**, tags `[very-long-function, complex-branching, hard-to-follow, many-returns]`.
   **Why it hurts:** 238 LOC, 4-deep nesting, many early returns — A* body that
   intertwines neighbour iteration and bookkeeping.
   **Extract candidate:** none surfaced.
   **Suggested first move:** extract the neighbour-expansion loop body into a
   helper; keep the outer function as the high-level A* driver.

7. [Flux/DynamicLights/Flux_DynamicLights.cpp:634](Flux/DynamicLights/Flux_DynamicLights.cpp:634) — `Flux_DynamicLights::GatherLightsFromScene` — score **77.5**, tags `[very-long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 329 LOC iterating and classifying every light type inline.
   **Extract candidate:** none surfaced.
   **Suggested first move:** extract one `GatherX` helper per light type
   (point / spot / directional / area) and have the outer function aggregate.

8. [Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:369](Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:369) — `Zenith_EditorPanelContentBrowser::RenderTopBar` — score **76.5**, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 169 LOC of ImGui top-bar controls packed into one body.
   **Extract candidate:** none surfaced.
   **Suggested first move:** split into `RenderNavButtons`, `RenderPathBar`,
   `RenderSearch`, `RenderViewOptions` helpers.

9. [Flux/RenderGraph/Flux_RenderGraph.cpp:1541](Flux/RenderGraph/Flux_RenderGraph.cpp:1541) — `Flux_RenderGraph::AssignAliasingGroups` — score **73.3**, tags `[long-function, complex-branching, hard-to-follow]`.
   **Why it hurts:** 177 LOC of lifetime/size bucketing logic at nesting 4.
   **Extract candidate:** none surfaced.
   **Suggested first move:** extract `ComputeLifetimes` and `BucketByCompat`
   helpers; leave assignment as a single loop over pre-computed buckets.

10. [Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:675](Editor/Panels/Zenith_EditorPanel_ContentBrowser.cpp:675) — `Zenith_EditorPanelContentBrowser::RenderFileGrid` — score **72.7**, tags `[long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell]`.
    **Why it hurts:** cognitive 72 at nesting 5 — deep inner blocks doing per-
    cell drag-drop and selection work.
    **Extract candidate lines:** 732–747 (peak depth 5, 16 lines) and 786–801
    (peak depth 5, 16 lines).
    **Suggested first move:** extract those two inner blocks into
    `HandleGridItemDragDrop` and `HandleGridItemClick`, then flatten the outer
    loop with early `continue`s.

## 3. Tag distribution (shape of the problem)

Across the top-50 function hotspots:

- `hard-to-follow` — 48
- `complex-branching` — 44
- `long-function` — 39
- `deep-nesting` — 13
- `nesting-hell` — 12
- `very-long-function` — 8
- `many-returns` — 6
- `many-params` — 4

**Interpretation:** the dominant shape is long imperative bodies with branchy,
hard-to-follow control flow. Deep nesting shows up in a significant minority
(13 + 12 across deep-nesting/nesting-hell) but the engine's problem is not
giant switch-dispatch tables — there are zero `big-switch` tags. Parameter-
object refactors (`many-params` = 4) and early-return flattening
(`many-returns` = 6) are secondary levers; the primary lever is function
extraction from long bodies.

## 4. God files

| Path | LOC | Functions | Worst fn score | Priority score |
|------|-----|-----------|----------------|----------------|
| `Flux\RenderGraph\Flux_RenderGraph.cpp` | 1815 | 80 | 85.5 | 81.7 |
| `Vulkan\Zenith_Vulkan_CommandBuffer.cpp` | 722 | 43 | 69.8 | 78.7 |
| `Vulkan\Zenith_Vulkan.cpp` | 1148 | 46 | 78.5 | 77.8 |
| `Editor\Zenith_Editor.cpp` | 1250 | 46 | 71.4 | 77.4 |
| `EntityComponent\Zenith_SceneManager.cpp` | 1650 | 97 | 59.9 | 75.7 |
| `EntityComponent\Zenith_SceneData.cpp` | 939 | 45 | 55.4 | 74.4 |
| `Vulkan\Zenith_Vulkan_Pipeline.cpp` | 915 | 46 | 57.3 | 71.4 |
| `AI\Squad\Zenith_Squad.cpp` | 786 | 56 | 49.6 | 69.1 |
| `Vulkan\Zenith_Vulkan_MemoryManager.cpp` | 1505 | 68 | 48.0 | 68.7 |
| `UnitTests\Zenith_SceneTests.cpp` | 9224 | 488 | 35.9 | 65.0 |
| `Flux\MeshAnimation\Flux_AnimationStateMachine.cpp` | 806 | 59 | 35.0 | 64.8 |
| `Flux\MeshAnimation\Flux_BlendTree.cpp` | 667 | 65 | 34.6 | 64.6 |
| `EntityComponent\Components\Zenith_AnimatorComponent.cpp` | 509 | 44 | 31.2 | 63.6 |
| `UnitTests\Zenith_UnitTests.cpp` | 7743 | 269 | 29.0 | 63.0 |
| `Flux\MeshAnimation\Flux_AnimationClip.cpp` | 596 | 42 | 20.7 | 60.5 |
| `EntityComponent\Zenith_SceneData.h` | 656 | 74 | 19.2 | 58.3 |
| `UnitTests\Zenith_AutomationTests.cpp` | 1973 | 87 | 23.0 | 57.6 |
| `UnitTests\Zenith_AITests.cpp` | 1835 | 94 | 20.7 | 55.2 |
| `EntityComponent\Components\Zenith_ScriptComponent.h` | 192 | 50 | 14.4 | 50.0 |
| `Editor\Zenith_EditorAutomation.cpp` | 1501 | 85 | 44.5 | 45.7 |
| `UI\Zenith_UIElement.h` | 195 | 50 | 15.0 | 29.6 |
| `UnitTests\Zenith_PhysicsTests.cpp` | 882 | 52 | 12.6 | 26.7 |
| `UnitTests\Zenith_EditorTests.cpp` | 1268 | 98 | 12.1 | 16.4 |

## 5. Directory risk concentration

Top directories by count of entries in the top-50 function hotspots:

| Directory | Hotspot count | Avg score |
|-----------|--------------:|----------:|
| `Editor/Panels` | 6 | 65.97 |
| `EntityComponent/Components` | 6 | 61.42 |
| `Flux/RenderGraph` | 5 | 70.24 |
| `AI/Navigation` | 4 | 64.30 |
| `Profiling/Zenith_Profiling.cpp` | 2 | 79.15 |
| `Editor/Zenith_Editor.cpp` | 2 | 68.90 |
| `EntityComponent/Zenith_SceneManager.cpp` | 2 | 59.05 |
| `Flux/Primitives` | 1 | 80.00 |
| `Vulkan/Zenith_Vulkan.cpp` | 1 | 78.50 |
| `Flux/DynamicLights` | 1 | 77.50 |

By top-level directory: `Flux` 17, `Editor` 10, `EntityComponent` 9, `AI` 5,
`Vulkan` 3, `Profiling` 2, `UI` 2, `AssetHandling` 1, `Physics` 1.

## 6. Tech-debt markers

Counts after filtering hash-map-placeholder entries:

- TODO: 27
- FIXME: 0
- HACK: 0
- XXX: 0

Top markers (FIXME > HACK ≈ XXX > TODO, then file:line):

- [AI/Components/Zenith_AIAgentComponent.cpp:211](AI/Components/Zenith_AIAgentComponent.cpp:211) **TODO** Iterate and display blackboard contents
- [Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Editor/Panels/Zenith_EditorPanel_RenderGraph.cpp:148](Editor/Panels/Zenith_EditorPanel_RenderGraph.cpp:148) **TODO** Barrier display was removed – graph-level barriers were dead code.
- [Editor/Zenith_Editor_Menu.cpp:180](Editor/Zenith_Editor_Menu.cpp:180) **TODO** Toggle hierarchy panel visibility
- [Editor/Zenith_Editor_Menu.cpp:186](Editor/Zenith_Editor_Menu.cpp:186) **TODO** Toggle properties panel visibility
- [Editor/Zenith_Editor_Menu.cpp:192](Editor/Zenith_Editor_Menu.cpp:192) **TODO** Toggle console panel visibility
- [Editor/Zenith_SelectionSystem.cpp:209](Editor/Zenith_SelectionSystem.cpp:209) **TODO** Also handle entities without models but with other pickable components
- [Editor/Zenith_SelectionSystem.cpp:347](Editor/Zenith_SelectionSystem.cpp:347) **TODO** Handle entities without models
- [Editor/Zenith_UndoSystem.cpp:343](Editor/Zenith_UndoSystem.cpp:343) **TODO** Serialize full entity state (all components)
- [Editor/Zenith_UndoSystem.cpp:391](Editor/Zenith_UndoSystem.cpp:391) **TODO** Deserialize full entity state
- [EntityComponent/Components/Zenith_ModelComponent.cpp:458](EntityComponent/Components/Zenith_ModelComponent.cpp:458) **TODO** Flux_MeshInstance needs to provide access to geometry or position data
- [EntityComponent/Zenith_SceneData.cpp:103](EntityComponent/Zenith_SceneData.cpp:103) **TODO** auto-promote same-scene self-unload to async.
- [EntityComponent/Zenith_SceneManager.cpp:1459](EntityComponent/Zenith_SceneManager.cpp:1459) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting.
- [EntityComponent/Zenith_SceneManager.cpp:2114](EntityComponent/Zenith_SceneManager.cpp:2114) **TODO** Use scaled time when timeScale system is implemented (Unity's Time.fixedDeltaTime)

## 7. Headline numbers

```
Files: 403   SLOC: 99,077   Functions: 5,307
avg MI: 30.20   min MI: 0
avg CC: 21.09   max CC: 365
Estimated bugs: 1,502.23
```

## 8. Appendix — top-20 hotspots

| # | File:Line | Name | Score | Tags | CC | Cog | Nest | LOC |
|--:|-----------|------|------:|------|---:|----:|-----:|----:|
| 1 | Flux\RenderGraph\Flux_RenderGraph.cpp:328 | Flux_RenderGraph::AddResourceUsage | 85.5 | long-function, complex-branching, hard-to-follow, many-params | 34 | 45 | 3 | 173 |
| 2 | Flux\Primitives\Flux_Primitives.cpp:687 | Flux_Primitives::ExecuteGBuffer | 80.0 | very-long-function, complex-branching, hard-to-follow | 36 | 73 | 3 | 365 |
| 3 | Profiling\Zenith_Profiling.cpp:347 | Zenith_Profiling::RenderThreadBreakdown | 80.0 | very-long-function, complex-branching, hard-to-follow | 30 | 52 | 3 | 287 |
| 4 | Vulkan\Zenith_Vulkan.cpp:267 | Zenith_Vulkan::RecordCommandBuffersTask | 78.5 | long-function, complex-branching, hard-to-follow | 27 | 40 | 3 | 196 |
| 5 | Profiling\Zenith_Profiling.cpp:170 | Zenith_Profiling::RenderTimelineView | 78.3 | long-function, complex-branching, hard-to-follow, many-params | 27 | 35 | 3 | 177 |
| 6 | AI\Navigation\Zenith_Pathfinding.cpp:18 | Zenith_Pathfinding::FindPathInternal | 77.9 | very-long-function, complex-branching, hard-to-follow, many-returns | 23 | 55 | 4 | 238 |
| 7 | Flux\DynamicLights\Flux_DynamicLights.cpp:634 | Flux_DynamicLights::GatherLightsFromScene | 77.5 | very-long-function, complex-branching, hard-to-follow | 34 | 54 | 3 | 329 |
| 8 | Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp:369 | Zenith_EditorPanelContentBrowser::RenderTopBar | 76.5 | long-function, complex-branching, hard-to-follow | 28 | 39 | 4 | 169 |
| 9 | Flux\RenderGraph\Flux_RenderGraph.cpp:1541 | Flux_RenderGraph::AssignAliasingGroups | 73.3 | long-function, complex-branching, hard-to-follow | 24 | 45 | 4 | 177 |
| 10 | Editor\Panels\Zenith_EditorPanel_ContentBrowser.cpp:675 | Zenith_EditorPanelContentBrowser::RenderFileGrid | 72.7 | long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell | 19 | 72 | 5 | 142 |
| 11 | Flux\IBL\Flux_IBL.cpp:130 | Flux_IBL::UpdateGraphPassEnables | 72.6 | long-function, complex-branching, hard-to-follow | 25 | 40 | 4 | 140 |
| 12 | Editor\Zenith_Editor.cpp:1030 | Zenith_Editor::SetEditorMode | 71.4 | long-function, complex-branching, hard-to-follow | 22 | 43 | 4 | 157 |
| 13 | AssetHandling\Zenith_FileWatcher.cpp:252 | Zenith_FileWatcher::WatchThreadFunc | 70.9 | long-function, deep-nesting, complex-branching, hard-to-follow | 16 | 41 | 7 | 118 |
| 14 | Flux\RenderGraph\Flux_RenderGraph.cpp:972 | Flux_RenderGraph::Validate | 70.6 | long-function, complex-branching, hard-to-follow | 27 | 53 | 4 | 108 |
| 15 | Vulkan\Zenith_Vulkan_CommandBuffer.cpp:151 | Zenith_Vulkan_CommandBuffer::UpdateDescriptorSets | 69.8 | long-function, complex-branching, hard-to-follow | 20 | 41 | 4 | 175 |
| 16 | Flux\StaticMeshes\Flux_StaticMeshes.cpp:101 | Flux_StaticMeshes::ExecuteGBuffer | 69.7 | long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell | 17 | 52 | 5 | 141 |
| 17 | Flux\MeshAnimation\Flux_AnimationController.cpp:197 | Flux_AnimationController::UpdateWithSkeletonInstance | 68.0 | long-function, complex-branching, hard-to-follow | 19 | 66 | 4 | 146 |
| 18 | Flux\Terrain\Flux_TerrainStreamingManager.cpp:305 | Flux_TerrainStreamingManager::UpdateStreaming | 67.2 | long-function, complex-branching, hard-to-follow | 21 | 39 | 4 | 122 |
| 19 | Editor\Zenith_Editor.cpp:1199 | Zenith_Editor::FlushPendingSceneOperations | 66.4 | long-function, complex-branching, hard-to-follow | 20 | 45 | 3 | 163 |
| 20 | AI\Navigation\Zenith_NavMeshGenerator.cpp:564 | Zenith_NavMeshGenerator::TraceContours | 65.6 | long-function, deep-nesting, complex-branching, hard-to-follow, nesting-hell | 18 | 40 | 5 | 91 |
