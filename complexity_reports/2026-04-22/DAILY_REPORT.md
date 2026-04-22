# Zenith Engine — Daily Code Health Snapshot
**Date:** 2026-04-22
**Scope:** `Zenith/` (411 source files analyzed)

---

## 1. Executive summary

- **Overall rating:** not emitted by the analyzer in this run (`summary.overall_rating` is `null`). Derived picture from the raw counters: 3.2% of files sit in the high-priority bucket (13 files), peak hotspot priority is 53.9 — no function is in "drop-everything" territory, but the engine carries a meaningful slab of long, cognitively heavy code concentrated in a handful of god-tier `.cpp` files.
- **Function hotspots by priority_score:** ≥80: **0** · ≥70: **0** · ≥60: **0**. The top-50 list runs 42.4–53.9.
- **Files tagged `god-file`:** **22**.
- **Tech-debt markers (after filtering hash-map placeholders):** **17** — all `TODO` (FIXME/HACK/XXX: 0).
- **Directory with highest hotspot concentration:** `Editor/` (6 top-50 hotspots; Editor + Editor/Panels together account for 11).
- **Near-duplicate function clusters:** **290** detected.
- **Include cycles:** **1** — between [EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) and [EntityComponent/Zenith_SceneManager.h](Zenith/EntityComponent/Zenith_SceneManager.h).

---

## 2. Top 10 refactoring targets

1. [Zenith/EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289](Zenith/EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289) — `Zenith_TerrainComponent::RenderTerrainRegenerationSection` — score **53.9**, tags `[long-function, deep-nesting, hard-to-follow, nesting-hell]`.
   - **Why it hurts:** 97-line body with 5-deep nesting — extract each inner block behind a named helper and flatten the outer scope with early returns.
   - **Extract candidate line range:** not recorded by analyzer.
   - **First move:** pull each `ImGui::CollapsingHeader`/`TreeNode` subsection into its own `RenderXxxControls()` helper.

2. [Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp:946](Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp:946) — `ExecuteDynamicLights` — score **53.9**, tags `[long-function, complex-branching]`.
   - **Why it hurts:** 178-LOC execute pass with CC=16 — the length is the dominant cost; carve by pipeline stage (cull → bin → shade → resolve).
   - **Extract candidate line range:** not recorded.
   - **First move:** split per-stage helpers that take the `Flux_RenderGraph::BuildContext&` and return a sub-result.

3. [Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:230](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:230) — `Flux_StaticMeshes::RenderToShadowMap` — score **53.9**, tags `[long-function, deep-nesting, hard-to-follow, nesting-hell]`.
   - **Why it hurts:** 85-LOC, 5-deep — the cascade × slice × instance loop nest is what costs; hoist the per-slice work into a helper.
   - **Extract candidate line range:** not recorded.
   - **First move:** extract `RenderShadowSliceForCascade(cascadeIdx, sliceIdx, …)` and early-continue on culled slices.

4. [Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419) — `RenderSceneHeaderAndBody` — score **53.7**, tags `[long-function, many-params]`.
   - **Why it hurts:** 95-LOC with a wide parameter list — introduce a parameter object (`SceneRenderContext`) grouping scene handle, filter state, and selection ptr.
   - **Extract candidate line range:** not recorded.
   - **First move:** bundle the panel-wide state into a struct and pass by reference.

5. [Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85) — `RenderEntityContextMenu` — score **52.9**, tags `[complex-branching, hard-to-follow]`.
   - **Why it hurts:** CC=15, cognitive=28 in 76 LOC — the menu is a flat sequence of conditional `MenuItem` blocks; cohesive sub-steps exist (Create, Parent, Component, Destroy).
   - **Extract candidate line range:** not recorded.
   - **First move:** split into `RenderContextMenu_Create/_Hierarchy/_Components/_Destroy`.

6. [Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:628](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:628) — `Zenith_NavMeshGenerator::BuildPolygonMesh` — score **52.7**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 111-LOC polygon-merging routine, cognitive=33 — the reader has to keep region/edge/poly indices in their head.
   - **Extract candidate line range:** not recorded.
   - **First move:** extract `TriangulateContour`, `MergePolygons`, `WriteOutputPolys` named phases.

7. [Zenith/Windows/Zenith_Windows_FileWatcher.cpp:165](Zenith/Windows/Zenith_Windows_FileWatcher.cpp:165) — `Zenith_FileWatcher::UpdatePlatform` — score **52.6**, tags `[long-function, complex-branching]`.
   - **Why it hurts:** 138 LOC, CC=18 — IOCP completion-routine switching dominates; a dispatch table keyed on `FILE_NOTIFY_*` cleans this up.
   - **Extract candidate line range:** not recorded.
   - **First move:** replace the if-ladder over `FileAction` values with a per-action handler function.

8. [Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104) — `Flux_AnimatedMeshes::ExecuteGBuffer` — score **52.4**, tags `[long-function, hard-to-follow]`.
   - **Why it hurts:** 98-LOC GBuffer emit with cognitive=29 — mirrors the static-meshes pass; likely shares structure worth unifying (see §6 cluster 7).
   - **Extract candidate line range:** not recorded.
   - **First move:** extract `BindMaterialState`, `DrawInstanceBatch` helpers; align with `Flux_StaticMeshes` form.

9. [Zenith/UI/Zenith_UICanvas.cpp:202](Zenith/UI/Zenith_UICanvas.cpp:202) — `Zenith_UICanvas::Update` — score **52.0**, tags `[complex-branching, hard-to-follow]`.
   - **Why it hurts:** **cognitive=45** in only 44 LOC — dense conditional logic per frame; very high cost-per-line.
   - **Extract candidate line range:** not recorded.
   - **First move:** pull focus/hover/input-routing conditions into predicates named for the transition they gate.

10. [Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:261](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:261) — `RenderCategoryTab` — score **51.8**, tags `[long-function, hard-to-follow]`.
    - **Why it hurts:** 95 LOC, cognitive=34 — tab layout intermixed with metric aggregation.
    - **Extract candidate line range:** not recorded.
    - **First move:** separate aggregation (`CollectCategoryStats`) from rendering (`RenderCategoryStatsTable`).

---

## 3. Tag distribution (top-50 hotspots)

- `long-function`: 28
- `hard-to-follow`: 23
- `complex-branching`: 21
- `deep-nesting`: 5
- `nesting-hell`: 5
- `many-params`: 2
- `many-returns`: 1
- `very-long-function`: 1
- `big-switch`: 1

**Interpretation:** the signature is long + hard-to-follow + complex-branching — imperative bodies that grew by accretion. Very few big-switch / many-params entries, so dispatch-table or parameter-object refactors buy little. The right playbook is: extract cohesive sub-steps from inside long functions, and flatten 4+ deep nests with early returns.

---

## 4. God files

| Path | LOC | Funcs | fan_in | fan_out | Worst func score | Priority |
|------|-----|-------|--------|---------|------------------|----------|
| [Zenith/EntityComponent/Zenith_SceneData.cpp](Zenith/EntityComponent/Zenith_SceneData.cpp) | 1057 | 45 | 0 | 12 | 48.5 | 72.3 |
| [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp) | 1857 | 73 | 0 | 7 | 50.8 | 71.2 |
| [Zenith/EntityComponent/Zenith_SceneManager.cpp](Zenith/EntityComponent/Zenith_SceneManager.cpp) | 1934 | 69 | 0 | 22 | 50.4 | 71.1 |
| [Zenith/AI/Squad/Zenith_Squad.cpp](Zenith/AI/Squad/Zenith_Squad.cpp) | 973 | 56 | 0 | 9 | 49.6 | 69.1 |
| [Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp) | 2041 | 68 | 0 | 9 | 49.5 | 69.1 |
| [Zenith/Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 1806 | 52 | 0 | 38 | 48.2 | 68.7 |
| [Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp) | 940 | 44 | 0 | 10 | 40.1 | 66.3 |
| [Zenith/Vulkan/Zenith_Vulkan.cpp](Zenith/Vulkan/Zenith_Vulkan.cpp) | 1545 | 51 | 0 | 17 | 35.8 | 65.0 |
| [Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.cpp) | 1042 | 59 | 0 | 4 | 35.0 | 64.8 |
| [Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp](Zenith/Flux/MeshAnimation/Flux_BlendTree.cpp) | 854 | 65 | 0 | 4 | 34.6 | 64.6 |
| [Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp](Zenith/EntityComponent/Components/Zenith_AnimatorComponent.cpp) | 718 | 44 | 0 | 11 | 31.2 | 63.6 |
| [Zenith/UnitTests/Zenith_SceneTests.cpp](Zenith/UnitTests/Zenith_SceneTests.cpp) | 14992 | 515 | 0 | 17 | 35.9 | 63.3 |
| [Zenith/UnitTests/Zenith_UnitTests.cpp](Zenith/UnitTests/Zenith_UnitTests.cpp) | 11986 | 276 | 0 | 58 | 29.0 | 63.0 |
| [Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp](Zenith/Flux/MeshAnimation/Flux_AnimationClip.cpp) | 769 | 42 | 0 | 6 | 20.7 | 60.5 |
| [Zenith/EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) | 1051 | 75 | 47 | 6 | 19.2 | 58.3 |
| [Zenith/UnitTests/Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp) | 2821 | 87 | 0 | 24 | 23.0 | 57.6 |
| [Zenith/UnitTests/Zenith_AITests.cpp](Zenith/UnitTests/Zenith_AITests.cpp) | 2988 | 98 | 0 | 23 | 20.7 | 56.8 |
| [Zenith/EntityComponent/Components/Zenith_ScriptComponent.h](Zenith/EntityComponent/Components/Zenith_ScriptComponent.h) | 356 | 50 | 9 | 5 | 15.9 | 51.1 |
| [Zenith/Editor/Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) | 1809 | 85 | 0 | 14 | 44.5 | 45.7 |
| [Zenith/UI/Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h) | 302 | 50 | 12 | 4 | 15.0 | 29.6 |
| [Zenith/UnitTests/Zenith_PhysicsTests.cpp](Zenith/UnitTests/Zenith_PhysicsTests.cpp) | 1327 | 52 | 0 | 12 | 12.6 | 26.7 |
| [Zenith/UnitTests/Zenith_EditorTests.cpp](Zenith/UnitTests/Zenith_EditorTests.cpp) | 2029 | 99 | 0 | 21 | 12.3 | 17.6 |

---

## 5. Directory risk concentration

| Directory | Hotspot count | Average score |
|-----------|---------------|---------------|
| Editor | 6 | 44.6 |
| Editor/Panels | 5 | 49.4 |
| AI/Navigation | 5 | 46.6 |
| Flux/RenderGraph | 5 | 46.3 |
| EntityComponent/Components | 4 | 46.4 |
| UI | 3 | 48.3 |
| Flux/MeshGeometry | 3 | 47.0 |
| EntityComponent | 3 | 48.1 |
| Flux/MeshAnimation | 3 | 45.3 |
| Flux/DynamicLights | 2 | 48.8 |

---

## 6. Near-duplicate function clusters

290 clusters detected (Jaccard ≥ 85% on token-normalised 5-gram shingles). The 10 largest are almost all in the editor-automation builder API and unit-test suites — strong evidence of copy-paste templates that a helper + parameter would collapse.

- **Cluster 1 (40 members, peak score 8.0):** `Zenith_EditorAutomation::AddStep_SetUI*` setters — [Zenith/Editor/Zenith_EditorAutomation.cpp](Zenith/Editor/Zenith_EditorAutomation.cpp) (`SetUIColor`, `SetUILayoutPadding`, `SetUIToggleOnColor`, `SetUIToggleOffColor`, `SetUIOverlayDimColor`, `SetUIButtonNormalColor`, … +34 more).
  - **Suggested action:** Replace with a single templated `AddStep_SetUIProperty<T>(name, propertyId, value)` helper; callers become one-line property tags.

- **Cluster 2 (19 members, peak score 4.0):** `Zenith_EditorAutomation::AddStep_Create*` and `Add*` creators (`CreateUIText`, `CreateUIButton`, `SetUIImageTexturePath`, `AddUIChild`, `CreateUIToggle`, …).
  - **Suggested action:** Generalize to `AddStep_CreateUIWidget(WidgetKind, args…)` with a dispatch on `WidgetKind`.

- **Cluster 3 (11 members, peak score 6.6):** UI layout tests — `Zenith_AutomationTests::TestLayoutHorizontalPositioning`, `TestLayoutInvisibleChildrenSkipped`, `TestLayoutVerticalPositioning`, `TestLayoutChildForceExpand`, `TestLayoutReverseArrangement`, `TestLayoutPaddingAffectsPositioning`, … +5 more in [Zenith/UnitTests/Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp).
  - **Suggested action:** Introduce a table-driven test that iterates layout permutations; the varying parts become rows, not duplicated bodies.

- **Cluster 4 (11 members, peak score 3.9):** Scene hierarchy tests in [Zenith/UnitTests/Zenith_SceneTests.cpp](Zenith/UnitTests/Zenith_SceneTests.cpp) — `TestDeepHierarchyActiveInHierarchy`, `TestHasChildrenAndCount`, `TestSetParentAcrossScenes`, `TestReparentEntity`, `TestUnparentEntity`, `TestIsRootEntity`, … +5 more.
  - **Suggested action:** Factor a `BuildHierarchyFixture(depth, layout)` helper and assert via a common checker.

- **Cluster 5 (11 members, peak score 2.2):** Trivial UI property setters — `SetTransitionDuration`, `SetBackgroundBorderColor`, `SetBackgroundBorderThickness`, `SetShadowColor`, `SetShadowSpread`, `SetBorderColor`, … across [Zenith/UI/Zenith_UIButton.h](Zenith/UI/Zenith_UIButton.h), [Zenith/UI/Zenith_UIElement.h](Zenith/UI/Zenith_UIElement.h), [Zenith/UI/Zenith_UIImage.h](Zenith/UI/Zenith_UIImage.h), [Zenith/UI/Zenith_UIRect.h](Zenith/UI/Zenith_UIRect.h).
  - **Suggested action:** Likely acceptable as-is (one-line property setters); a macro could generate them if duplication continues to grow.

- **Cluster 6 (10 members, peak score 4.5):** Automation-step setters for shadow/gradient/border/corner — [Zenith/UnitTests/Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp) (`TestSetUIShadowStep`, `TestSetUIGradientColorStep`, `TestSetUIShadowColorStep`, `TestSetUIRectBorderStep`, `TestSetUIButtonCornerRadiusStep`, `TestSetUIButtonShadowStep`, … +4 more).
  - **Suggested action:** Parameterize a single `TestSetUIPropertyStep` over the property-id / payload pair.

- **Cluster 7 (10 members, peak score 3.1):** `SetupRenderGraph`/`SetupAerialPerspectiveRenderGraph` across render subsystems — [Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp](Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp), [Zenith/Flux/Vegetation/Flux_Grass.cpp](Zenith/Flux/Vegetation/Flux_Grass.cpp), [Zenith/Flux/Skybox/Flux_Skybox.cpp](Zenith/Flux/Skybox/Flux_Skybox.cpp), [Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp), [Zenith/Flux/Primitives/Flux_Primitives.cpp](Zenith/Flux/Primitives/Flux_Primitives.cpp), [Zenith/Flux/SDFs/Flux_SDFs.cpp](Zenith/Flux/SDFs/Flux_SDFs.cpp), … +4 more.
  - **Suggested action:** Shape-wise they all register a pass + pass-through resources; extract a common `RegisterRenderPass(PassDesc&)` helper to keep subsystems symmetric.

- **Cluster 8 (10 members, peak score 2.9):** Physics-velocity tests in [Zenith/UnitTests/Zenith_PhysicsTests.cpp](Zenith/UnitTests/Zenith_PhysicsTests.cpp) — `TestAccumulatorDoesNotOverStep`, `TestGravityReenabledBodyFalls`, `TestAddImpulseInstantVelocityChange`, `TestZeroVelocityNoMovement`, `TestRebuildColliderPreservesVelocity`, `TestGetLinearVelocityMatchesSet`, … +4 more.
  - **Suggested action:** Factor a `SpawnBodyAndSimulate(steps, preFn, postFn)` helper; the bodies become one-liners.

- **Cluster 9 (8 members, peak score 3.4):** Editor-mode transition tests in [Zenith/UnitTests/Zenith_EditorTests.cpp](Zenith/UnitTests/Zenith_EditorTests.cpp) — `TestModeTransitionFullCycle`, `TestModeTransitionPausedToStopped`, `TestModeTransitionStoppedToPlaying`, `TestModeTransitionPlayingToPaused`, `TestGizmoModeSwitch`, `TestGizmoModeTranslate`, … +2 more.
  - **Suggested action:** Table-drive the (from, to, expected) transitions.

- **Cluster 10 (8 members, peak score 2.2):** `SetName`/`SetNodeName`/`SetPositionOffset` etc. across [Zenith/AI/BehaviorTree/Zenith_BTNode.h](Zenith/AI/BehaviorTree/Zenith_BTNode.h), [Zenith/AI/Squad/Zenith_Squad.h](Zenith/AI/Squad/Zenith_Squad.h), [Zenith/EntityComponent/Components/Zenith_LightComponent.h](Zenith/EntityComponent/Components/Zenith_LightComponent.h), [Zenith/Flux/MeshAnimation/Flux_AnimationLayer.h](Zenith/Flux/MeshAnimation/Flux_AnimationLayer.h), [Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.h](Zenith/Flux/MeshAnimation/Flux_AnimationStateMachine.h).
  - **Suggested action:** Trivial one-line setters — leave as-is unless a name-interface base class is wanted.

---

## 7. Include coupling & cycles

**Cycles:**
- [Zenith/EntityComponent/Zenith_SceneData.h](Zenith/EntityComponent/Zenith_SceneData.h) ↔ [Zenith/EntityComponent/Zenith_SceneManager.h](Zenith/EntityComponent/Zenith_SceneManager.h). Break the loop by forward-declaring one side and moving the `#include` to the `.cpp`.

**High fan-in headers (most-included):**

| File | fan_in | fan_out |
|------|--------|---------|
| [Zenith/Core/Zenith.h](Zenith/Core/Zenith.h) | 197 | 10 |
| [Zenith/Flux/Flux.h](Zenith/Flux/Flux.h) | 71 | 6 |
| [Zenith/Core/Memory/Zenith_MemoryManagement_Disabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Disabled.h) | 62 | 0 |
| [Zenith/DataStream/Zenith_DataStream.h](Zenith/DataStream/Zenith_DataStream.h) | 62 | 1 |
| [Zenith/Core/Memory/Zenith_MemoryManagement_Enabled.h](Zenith/Core/Memory/Zenith_MemoryManagement_Enabled.h) | 59 | 0 |

**High fan-out files (include the most):**

| File | fan_in | fan_out |
|------|--------|---------|
| [Zenith/UnitTests/Zenith_UnitTests.cpp](Zenith/UnitTests/Zenith_UnitTests.cpp) | 0 | 58 |
| [Zenith/Editor/Zenith_Editor.cpp](Zenith/Editor/Zenith_Editor.cpp) | 0 | 38 |
| [Zenith/Flux/Flux.cpp](Zenith/Flux/Flux.cpp) | 0 | 30 |
| [Zenith/Core/Zenith_Core.cpp](Zenith/Core/Zenith_Core.cpp) | 0 | 26 |
| [Zenith/UnitTests/Zenith_AutomationTests.cpp](Zenith/UnitTests/Zenith_AutomationTests.cpp) | 0 | 24 |

---

## 8. Tech-debt markers

After filtering hash-map/hash-set/container placeholder entries:

- **TODO:** 17 · **FIXME:** 0 · **HACK:** 0 · **XXX:** 0

Top markers (TODO only — ordered by file path):

- [Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162](Zenith/Editor/Panels/Zenith_EditorPanel_MaterialEditor.cpp:162) **TODO** Re-implement material list using Zenith_AssetRegistry API when needed
- [Zenith/Editor/Zenith_UndoSystem.cpp:352](Zenith/Editor/Zenith_UndoSystem.cpp:352) **TODO** Serialize full entity state (all components)
- [Zenith/Editor/Zenith_UndoSystem.cpp:404](Zenith/Editor/Zenith_UndoSystem.cpp:404) **TODO** Deserialize full entity state
- [Zenith/EntityComponent/Components/Zenith_ModelComponent.cpp:454](Zenith/EntityComponent/Components/Zenith_ModelComponent.cpp:454) **TODO** Flux_MeshInstance needs to provide access to geometry or position data
- [Zenith/EntityComponent/Zenith_SceneData.cpp:179](Zenith/EntityComponent/Zenith_SceneData.cpp:179) **TODO** auto-promote same-scene self-unload to async
- [Zenith/EntityComponent/Zenith_SceneManager.cpp:1318](Zenith/EntityComponent/Zenith_SceneManager.cpp:1318) **TODO** (flux-refcount): Implement once Flux asset managers support reference counting
- [Zenith/EntityComponent/Zenith_SceneManager.cpp:1519](Zenith/EntityComponent/Zenith_SceneManager.cpp:1519) **TODO** Use scaled time when timeScale system is implemented
- [Zenith/Flux/Flux.h:104](Zenith/Flux/Flux.h:104) **TODO** add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS)
- [Zenith/Flux/Flux.h:113](Zenith/Flux/Flux.h:113) **TODO** Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
- [Zenith/Flux/Flux_CommandList.h:88](Zenith/Flux/Flux_CommandList.h:88) **TODO** elide second memcpy by having AddCommand record directly into the target
- [Zenith/Flux/IBL/Flux_IBL.cpp:546](Zenith/Flux/IBL/Flux_IBL.cpp:546) **TODO** create per-face 2D SRVs for cubemap debug display
- [Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:178](Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:178) **TODO** Implement ANIMATION_UPDATE_FIXED and ANIMATION_UPDATE_UNSCALED
- [Zenith/Flux/MeshAnimation/Flux_AnimationController.h:293](Zenith/Flux/MeshAnimation/Flux_AnimationController.h:293) **TODO** Replace with engine type when MaskedBlend accepts non-std::vector
- [Zenith/Flux/Shadows/Flux_Shadows.cpp:155](Zenith/Flux/Shadows/Flux_Shadows.cpp:155) **TODO** Enable terrain shadow casting
- [Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:226](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:226) **TODO** these 2 should probably be separate components
- [Zenith/Vulkan/Zenith_Vulkan.cpp:1365](Zenith/Vulkan/Zenith_Vulkan.cpp:1365) **TODO** graceful spill — fall back to a VMA sub-allocation when a worker exhausts its pool
- [Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:389](Zenith/Vulkan/Zenith_Vulkan_CommandBuffer.cpp:389) **TODO** expose DONT_CARE LoadOp as a third option

---

## 9. Headline numbers

Files **411** · SLOC **100,158** · functions **5,517** · avg MI **30.0** · min MI **0.0** · avg CC **20.65** · max CC **248** · est bugs **1520.85**

---

## 10. Appendix — top-20 hotspots

| # | File:line | Name | Score | Tags | CC | Cog | Nest | LOC |
|---|-----------|------|-------|------|----|-----|------|-----|
| 1 | [Zenith/EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289](Zenith/EntityComponent/Components/Zenith_TerrainComponent_Editor.cpp:289) | `Zenith_TerrainComponent::RenderTerrainRegenerationSection` | 53.9 | long-function, deep-nesting, hard-to-follow, nesting-hell | 14 | 30 | 5 | 97 |
| 2 | [Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp:946](Zenith/Flux/DynamicLights/Flux_DynamicLights.cpp:946) | `ExecuteDynamicLights` | 53.9 | long-function, complex-branching | 16 | 23 | 3 | 178 |
| 3 | [Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:230](Zenith/Flux/StaticMeshes/Flux_StaticMeshes.cpp:230) | `Flux_StaticMeshes::RenderToShadowMap` | 53.9 | long-function, deep-nesting, hard-to-follow, nesting-hell | 12 | 30 | 5 | 85 |
| 4 | [Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:419) | `RenderSceneHeaderAndBody` | 53.7 | long-function, many-params | 13 | 21 | 4 | 95 |
| 5 | [Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85](Zenith/Editor/Panels/Zenith_EditorPanel_Hierarchy.cpp:85) | `RenderEntityContextMenu` | 52.9 | complex-branching, hard-to-follow | 15 | 28 | 4 | 76 |
| 6 | [Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:628](Zenith/AI/Navigation/Zenith_NavMeshGenerator.cpp:628) | `Zenith_NavMeshGenerator::BuildPolygonMesh` | 52.7 | long-function, hard-to-follow | 10 | 33 | 4 | 111 |
| 7 | [Zenith/Windows/Zenith_Windows_FileWatcher.cpp:165](Zenith/Windows/Zenith_Windows_FileWatcher.cpp:165) | `Zenith_FileWatcher::UpdatePlatform` | 52.6 | long-function, complex-branching | 18 | 23 | 4 | 138 |
| 8 | [Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104](Zenith/Flux/AnimatedMeshes/Flux_AnimatedMeshes.cpp:104) | `Flux_AnimatedMeshes::ExecuteGBuffer` | 52.4 | long-function, hard-to-follow | 13 | 29 | 4 | 98 |
| 9 | [Zenith/UI/Zenith_UICanvas.cpp:202](Zenith/UI/Zenith_UICanvas.cpp:202) | `Zenith_UICanvas::Update` | 52.0 | complex-branching, hard-to-follow | 15 | 45 | 2 | 44 |
| 10 | [Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:261](Zenith/Editor/Panels/Zenith_EditorPanel_Memory.cpp:261) | `RenderCategoryTab` | 51.8 | long-function, hard-to-follow | 11 | 34 | 4 | 95 |
| 11 | [Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp](Zenith/Flux/MeshGeometry/Flux_MeshGeometry.cpp) | `Flux_MeshGeometry::Combine` | 51.0 | long-function, complex-branching, many-returns | 17 | — | — | — |
| 12 | [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1086](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1086) | `Flux_RenderGraph::SynthesizeAliasingBarriers` | 50.8 | long-function, complex-branching, hard-to-follow | 17 | 30 | 3 | 88 |
| 13 | [Zenith/EntityComponent/Zenith_SceneManager.cpp:1797](Zenith/EntityComponent/Zenith_SceneManager.cpp:1797) | `Zenith_SceneManager::UnloadAllNonPersistent` | 50.4 | long-function, hard-to-follow | 14 | 26 | 4 | 106 |
| 14 | [Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1484](Zenith/Flux/RenderGraph/Flux_RenderGraph.cpp:1484) | `Flux_RenderGraph::AssignClearFlags` | 49.7 | complex-branching, hard-to-follow | 18 | 32 | 3 | 43 |
| 15 | [Zenith/AI/Squad/Zenith_Squad.cpp:718](Zenith/AI/Squad/Zenith_Squad.cpp:718) | `Zenith_Squad::AssignFormationSlots` | 49.6 | long-function, complex-branching, hard-to-follow | 18 | 28 | 3 | 82 |
| 16 | [Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp:936](Zenith/Vulkan/Zenith_Vulkan_MemoryManager.cpp:936) | `Zenith_Vulkan_MemoryManager::CreateTextureVRAM` | 49.5 | long-function | 13 | 23 | 3 | 136 |
| 17 | [Zenith/EntityComponent/Zenith_SceneData.cpp:824](Zenith/EntityComponent/Zenith_SceneData.cpp:824) | `Zenith_SceneData::DispatchAwakeForNewScene` | 48.5 | long-function, deep-nesting, hard-to-follow, nesting-hell | 11 | 26 | 5 | 98 |
| 18 | [Zenith/Editor/Zenith_Editor.cpp:376](Zenith/Editor/Zenith_Editor.cpp:376) | `Zenith_Editor::Update` | 48.2 | long-function, complex-branching, hard-to-follow | 16 | 25 | 3 | 115 |
| 19 | [Zenith/Flux/MeshAnimation/Flux_BonePose.cpp:198](Zenith/Flux/MeshAnimation/Flux_BonePose.cpp:198) | `Flux_SkeletonPose::SampleFromClip` | 47.3 | hard-to-follow | 11 | 29 | 3 | 69 |
| 20 | [Zenith/Editor/Zenith_Editor_Menu.cpp:48](Zenith/Editor/Zenith_Editor_Menu.cpp:48) | `Zenith_Editor::RenderFileMenu` | 47.2 | long-function, hard-to-follow | 12 | 27 | 4 | 92 |

---

*Archive: [complexity_reports/2026-04-22/analysis_report.json](complexity_reports/2026-04-22/analysis_report.json), [complexity_reports/2026-04-22/analysis_report.html](complexity_reports/2026-04-22/analysis_report.html).*
