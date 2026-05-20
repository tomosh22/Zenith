# Phase −1 Inventory: Mutable Static & Global State

Status: **First-pass draft.** Captures the major surfaces with verified
counts. Areas marked `TODO` need a follow-up sweep before Phase 0 ships.

This is the contract for Phases 0+. Classifications:
- **migrate** — moves onto `Zenith_Engine` (or a subsystem owned by it)
- **carve-out** — stays static per documented rationale
- **test-reset** — stays static but added to boot/shutdown reset path

## Refreshed call-site counts (source-only)

| Symbol | Sites (source) | Notes |
|--------|----------------|-------|
| `Zenith_TaskSystem::SubmitTask*` | **26** in 15 files | Plan estimated 27; prior review said 19. Actual: 26. |
| `Zenith_Core::GetDt / GetTimePassed / SetDt / AddTimePassed` | **29** in 16 files | Plan estimated 27. Close. |
| `RegisterScene*Callback` | **~99** | 84 in `Zenith_SceneManager.Tests.inl`, rest spread across SceneManager + tests. |
| `UnregisterScene*Callback` | **~85** | 77 in `Zenith_SceneManager.Tests.inl`. |
| Combined scene callback reg/unreg | **~184** | Mostly in the test inl. Still mechanical, no AST tooling required. |

## Major-system inventory

### Zenith_TaskSystem — `Zenith_TaskSystem.cpp:9-17`
**Classification: migrate.**
- `g_xTaskQueue` (CircularQueue<Zenith_Task*, 128>)
- `g_pxWorkAvailableSem`, `g_pxThreadsTerminatedSem` (Zenith_Semaphore*)
- `g_xQueueMutex` (Zenith_Mutex)
- `g_bTerminateThreads`, `g_bInitialized` (atomic<bool>)
- `g_uNumWorkerThreads`
7 module-scope statics.

### Zenith_Profiling — `Zenith_Profiling.cpp`
**Classification: migrate (shared state) + carve-out (TLS).**
- Non-TLS (migrate, 7): `g_xEvents`, `g_xPreviousFrameEvents`,
  `g_xEventsMutex`, `g_xFrameStart`, `g_xFrameEnd`,
  `g_xPreviousFrameStart`, `g_xPreviousFrameEnd`
- TLS (carve-out, 5 per thread): `tl_g_uCurrentDepth`,
  `tl_g_aeIndices[uMAX_PROFILE_DEPTH]`, `tl_g_aszLabels[...]`,
  `tl_g_axStartPoints[...]`, `tl_g_axEndPoints[...]`
- Macro: `ZENITH_PROFILING_FUNCTION_WRAPPER` at
  `Zenith_Profiling.h:211` — 8 call sites (all in `Zenith_Core.cpp`)
- Direct `Zenith_Profiling::BeginProfile/EndProfile` call sites: ~20.

### Zenith_Multithreading
**Classification: migrate (main-thread tracking) + carve-out (TLS).**
- Windows (`Zenith_Windows_Multithreading.cpp:11-12`):
  `tl_g_acThreadName[]`, `tl_g_uThreadID` (TLS — carve-out)
  + presumably `g_uMainThreadID` (migrate; needs verification)
- Android (`Zenith_Android_Multithreading.cpp:12-13`): same pattern.

### Zenith_Physics — `Zenith_Physics.cpp`
**Classification: migrate.**
Module-scope statics found:
- `g_bInitialised` (line 35)
- `s_ulJoltMemoryAllocated`, `s_ulJoltAllocationCount` (atomic counters, lines 38-39)
- `s_xBroadPhaseLayerInterface`, `s_xObjectVsBroadPhaseLayerFilter`,
  `s_xObjectLayerPairFilter` (lines 312-314)
Plus class statics on `Zenith_Physics`:
- `s_pxTempAllocator`, `s_pxJobSystem`, `s_pxPhysicsSystem`,
  `s_fTimestepAccumulator`
**TODO:** contact listener state, deferred collision queues,
dropped-event counters — full audit before Phase 4.

### Zenith_AssetRegistry — `Zenith_AssetRegistry.h:346`
**Classification: migrate.**
- `s_pxInstance` (class static) — formalize as Engine member.
- Static asset directory strings (`s_strGameAssetsDir`, `s_strEngineAssetsDir`).
- Per-type loader registries (function-local statics? or class statics?).
**TODO:** locate the loader registry implementation; classify per
sub-component.

### Zenith_SceneManager + 5 internal subsystems
**Classification: migrate.**

`Zenith_SceneCallbackBus.cpp` (anonymous namespace, 5+ statics):
- `g_ulNextCallbackHandle` (handle allocator)
- `g_uFiringCallbacksDepth` (re-entrancy counter)
- `g_bSuppressActiveSceneChanged`, `g_bHaveDeferredOldActive`,
  `g_xDeferredOldActive` (deferred-active-scene state)
- 6 callback lists (per the existing plan inventory):
  `g_xActiveSceneChangedCallbacks`, `g_xSceneLoadedCallbacks`,
  `g_xSceneUnloadingCallbacks`, `g_xSceneUnloadedCallbacks`,
  `g_xSceneLoadStartedCallbacks`, `g_xEntityPersistentCallbacks`
- `g_axCallbacksPendingRemoval` (deferred-removal queue)

`Zenith_SceneLifecycleScheduler.cpp`:
- `g_pxAnimUpdateTask` (animation task pointer)
- `s_bIsLoadingScene`, `s_bIsPrefabInstantiating`, `s_bIsUpdating`
  (gate flags)
- `s_fFixedTimeAccumulator`, `s_ulLastDeferredLoadOp`,
  `g_xAnimationsToUpdate`

Entity storage (`Zenith_SceneData.h`):
- `s_axEntitySlots[]`, `s_axFreeEntityIndices`, `s_axEntityComponents`
  — all three move into engine-owned `EntityStore` per Phase 5.

### Zenith_Editor — `Zenith_Editor.h:297-358`
**Classification: migrate** (large surface).
**38 class statics confirmed**, plus module-scope statics in `.cpp`:
- Mode/gizmo state: `s_eEditorMode`, `s_eGizmoMode`,
  `s_uPrimarySelectedEntityID`, `s_uLastClickedEntityID`,
  `s_xSelectedEntityIDs`
- Viewport: `s_xViewportSize`, `s_xViewportPos`, `s_bViewportHovered`,
  `s_bViewportFocused`
- Content browser: `s_strCurrentDirectory`, `s_xDirectoryContents`,
  `s_xFilteredContents`, `s_bDirectoryNeedsRefresh`, `s_szSearchBuffer`,
  `s_iAssetTypeFilter`, `s_iSelectedContentIndex`, `s_fThumbnailSize`,
  `s_axNavigationHistory`, `s_iHistoryIndex`, `s_eViewMode`
- Console: `s_xConsoleLogs`, `s_bConsoleAutoScroll`,
  `s_bShowConsoleInfo`, `s_bShowConsoleWarnings`, `s_bShowConsoleErrors`,
  `s_xCategoryFilters`
- Panels visible: `s_bShowHierarchyPanel`, `s_bShowPropertiesPanel`,
  `s_bShowConsolePanel`
- Editor camera (10 fields): `s_xEditorCameraPosition`,
  `s_fEditorCameraPitch/Yaw/FOV/Near/Far/MoveSpeed/RotateSpeed`,
  `s_uGameCameraEntity`, `s_bEditorCameraInitialized`
- Material editor: `s_pxSelectedMaterial`, `s_bShowMaterialEditor`

`Zenith_Editor.cpp` module statics:
- `s_xCachedGameTextureHandle`, `s_xCachedImageViewHandle`,
  `s_xPendingDeletions`

Editor submodules:
- `Zenith_Gizmo.h:112-120`: `s_eActiveAxis`, `s_bIsManipulating`,
  `s_xManipulationStartPos`, `s_xMouseStartPos`, `s_bSnapEnabled`,
  `s_fSnapValue`, `s_fGizmoSize`
- `Zenith_UndoSystem.h:180-183`: `s_xUndoStack`, `s_xRedoStack`
- `Zenith_SelectionSystem.cpp:81`: `s_xEntityBoundingBoxes`
- `Zenith_EditorAutomation.h:496-499`: `s_axActions`, `s_uCurrentAction`,
  `s_bRunning`, `s_bComplete`
- `Zenith_Editor_MaterialUI.cpp:30`: `s_xTexturePreviewCache`
- `Zenith_EditorPanel_ContentBrowser.cpp:149`: `s_xThumbnailCache`
- `Zenith_EditorPanel_RenderGraph.cpp:12-13`: `s_bVisible`, `s_bHideDisabled`
- `Zenith_EditorPanel_Memory.cpp:32+`: several panel-local statics

All `#ifdef ZENITH_TOOLS`. The `Zenith_Editor` engine member is itself
`#ifdef ZENITH_TOOLS`.

### Zenith_Input — `Zenith_Input.cpp`
**Classification: migrate.**
- `s_xFrameKeyPresses` (unordered_set — replace with engine container)
- `s_xLastMousePosition`, `s_xMouseDelta`
- `s_fMouseWheelDelta`, `s_bFirstFrame`
- `s_bSimWasEnabledLastFrame`
- `MAX_GAMEPADS = 4` (constant — stays)

`Zenith_TouchInput.cpp` (15 module-scope statics):
- Touch state (10): `s_bTouchActive`, `s_xTouchStartPos`,
  `s_fTouchStartTime`, `s_bWasTouchDownLastFrame`,
  `s_bTapThisFrame`, `s_bSwipeThisFrame`, `s_eSwipeDirection`,
  `s_xTapPosition`, `s_xSwipeStartPos`, `s_fSwipeDistance`
- Current input (2): `s_xCurrentTouchPos`, `s_bCurrentlyDown`
- Config (3): `s_fSwipeThreshold`, `s_fTapMaxMovement`,
  `s_fTapMaxDuration`

`Zenith_InputSimulator.h:80-94` (~10 class statics):
- `s_bEnabled`, `s_bFixedDtEnabled`, `s_fFixedDt`
- `s_abKeyState[512]`, `s_abKeyPressedThisFrame[512]`,
  `s_xMousePosition`
- `s_aeAutoReleaseKeys[32]`, `s_uAutoReleaseCount`
- `s_fMouseWheelDelta`

**Classification note:** `Zenith_InputSimulator` is currently in the
plan's "What Stays Static" list as a test helper. Decision needed:
migrate alongside `Zenith_Input` (consistent) or carve-out (rationale:
test-only). Recommendation: migrate, since both are coupled.

### Zenith_MemoryManagement — TLS-heavy
**Classification: carve-out (allocator core) + migrate (tracking).**
- `tl_eCurrentCategory`, `tl_aeCategoryStack[]`, `tl_uCategoryStackDepth`
  (TLS — carve-out, OS thread state)
- `tl_bInAllocation`, `tl_bInDeallocation` (TLS recursion guards —
  carve-out)
- `s_axRecordsCopy` in MemoryTracker (TLS — carve-out)
**TODO:** locate the global allocation tracker state (process-wide
counters, registers); classify those.

### Zenith_Vulkan + Flux/
**Classification: migrate (Phase 6a/6b).**
- `Zenith_Vulkan` class statics: backend handles, command buffers,
  swapchain, memory manager, VRAM registry.
- `Zenith_Vulkan_MemoryManager` — VMA allocator state, staging pool.
- `Zenith_Vulkan_VRAM` — VRAM handle registry.
- `Zenith_Vulkan_PerFrame` — per-frame command pools/descriptor
  pools/fences.
- `Flux::s_pxRenderGraph`, `s_xPendingCommandLists`, `s_uFrameCounter`,
  `s_bGraphRebuildRequested` (Flux.h:460-467).
- `Flux_PerFrame::s_uFrameCounter`, `s_apfn*Callbacks[4]` (max is 4,
  `ZenithConfig.h:110`).
- `Flux_Graphics::s_xRepeatSampler/s_xClampSampler/s_xQuadMesh/
  s_xFrameConstantsBuffer/s_xWhite/Black/GridTexture/s_xBlankMesh/
  s_xBlankMaterial/s_xCubemapTexture/s_xWaterNormalTexture/
  s_aeMRTFormats/s_xFrameConstants` (`Flux_Graphics.h:50-78`).
- TLS: `tls_pxCurrentRecordingPass`, `tls_pxCurrentRecordingGraph`
  (`Flux_RenderGraph_Execution.cpp:24-25`).
- ~25 individual Flux subsystem managers (Phase 7).

### Zenith_SceneManager (already covered above)

## Other systems (not in original 10-major list)

### AI systems
**Classification: migrate.**

`AI/Perception/Zenith_PerceptionSystem.h:200-202`:
- `s_xAgentData` (unordered_map<uint64_t, AgentPerceptionData>) —
  `#TODO: Replace with engine hash map` already noted in source
- `s_xTargets` (unordered_map<uint64_t, TargetInfo>) — same
- `s_axActiveSounds` (Zenith_Vector<Zenith_SoundStimulus>)

`AI/Squad/Zenith_Squad.h:213-214`:
- `s_axSquads`, `s_bInitialised`

`AI/Squad/Zenith_Formation.h:85-90` (6 statics):
- 5 default formation instances, `s_bFormationsInitialised`

`AI/Squad/Zenith_TacticalPoint.h:203-217` (8 statics):
- Scoring weights (4): `s_fDistanceWeight`, `s_fCoverWeight`,
  `s_fVisibilityWeight`, `s_fElevationWeight`
- Storage: `s_axPoints`, `s_axPointActive`, `s_uNextPointID`,
  `s_bInitialised`

`AI/Navigation/Zenith_NavMesh.cpp`:
- `s_bLoggedHeights` (line 1227, logging guard) — **test-reset**
- `s_xRng` thread_local at line 924 — **carve-out** (TLS)

**TODO:** `AI/BehaviorTree/` static state audit. The initial grep
found only stateless helper functions; needs a deeper look.

### DebugVariables
**Classification: migrate (recommended) — see Phase 5.7.**
- `Zenith_DebugVariableTree s_xTree` (Zenith_DebugVariables.h:236)
- Callback storage holds function pointers into subsystem state.
- Decision needed in Phase 5.7: move tree onto `Zenith_Engine` vs.
  keep static with Clear()/Rebuild() contract.

### SaveData — `Zenith_SaveData.cpp:22-81`
**Classification: migrate.**
- `s_xWrittenSlotsLog` (Zenith_Vector<WrittenSlot>)
- `s_xReadbackStash` (std::vector<WrittenSlot> — note: replace
  std::vector with Zenith_Vector during migration)
- `s_bCRC32TableInitialised` (CRC table init guard — could stay
  test-reset)
- `s_bInitialised`

### Platform window — `Zenith_Windows_Window.cpp`
**Classification: carve-out (process-level OS resource).**
- `s_ulGLFWMemoryAllocated`, `s_ulGLFWAllocationCount` (atomic counters)
- Callback functions (Error/Key/Mouse/Scroll) — stateless function
  pointers, no state to migrate.
**Note:** `Zenith_Window` itself has `GetInstance()` singleton pattern
(see "Static singletons" below). Window stays carve-out per plan.

### Static singletons via `GetInstance()` or `s_pxInstance`
**Most stay carve-out per plan; verify each:**

From grep (21 files):
- `Zenith_AutomatedTest` — carve-out (test helper)
- `Zenith_Vulkan_MemoryManager` — migrate (Phase 6b)
- `Zenith_Vulkan` — migrate (Phase 6b)
- `Zenith_SaveData` (uses GetInstance pattern?) — migrate (see above)
- `Zenith_SceneData` — already engine-owned via SceneManager
- `Zenith_Editor` — migrate (Phase 5.5)
- `Zenith_ComponentMeta` — carve-out (static-init populated)
- `Zenith_Window` — carve-out (OS process-level)
- `Zenith_Input` — migrate
- `Flux_Fog`, `Flux_Graphics`, `Flux_Text`, `Flux_Grass`,
  `Flux_Vulkan_Swapchain` — migrate (Phase 6/7)
- `Zenith_UICanvas` — TODO classify
- `Zenith_Prefab` — TODO classify
- `Zenith_CameraComponent` — TODO classify (likely component-level
  state, not singleton)
- `Zenith_AssetRegistry` — migrate (Phase 4)

## Follow-up sweep results (resolved)

### AI/BehaviorTree — `Zenith\AI\BehaviorTree`
**Classification: stateless (no migration needed).**
No module-scope or class statics with mutable state. Composite logic is
function-pure. Trees themselves are per-instance (owned by the AI agent
component).

### AssetHandling additions

`Zenith_AsyncAssetLoader.h:145-152` (**migrate**, 6 statics):
- `s_xPendingLoads` (Zenith_Vector<LoadRequest>)
- `s_xCompletedLoads` (Zenith_Vector<CompletedLoad>)
- `s_xPendingMutex`, `s_xCompletedMutex`, `s_xStateMutex` (3 mutexes)
- `s_xLoadStates` (unordered_map<string, AssetLoadState>)
- Plus `Zenith_AsyncAssetLoader.cpp:197-198`: function-local `s_xLogged`
  + `s_xMutex` for dedup logging — **test-reset** (caches log entries).

`Zenith_MaterialAsset.h:225-226` (**migrate**):
- `s_xDefaultWhite`, `s_xDefaultNormal` (default TextureHandles).

`Zenith_AssetRegistry.cpp:172-178` (**migrate**):
- `s_xRegistry` (factory map for `ZENITH_REGISTER_ASSET_TYPE`)
- `s_xMutex` (no-profiling mutex)
- Function-local Meyer's singleton pattern.

`Zenith_ScriptAsset.cpp:75-81` (**migrate** — critical for
`ZENITH_BEHAVIOUR_TYPE_NAME` macro):
- `s_xFactoryMap` (factory map keyed by type name)
- `s_xInternalNames` (registered behaviour name set)
- `Zenith_Engine::Initialise()` must read from these side-lists during
  construction.

`Zenith_FileWatcher.h:143-164` (**migrate**, ~12 class statics):
- Lifecycle: `s_bInitialized`, `s_bPaused`, `s_strWatchPath`
- Events: `s_xPendingEvents`, `s_xEventMutex`
- Callbacks: `s_xCallbacks`, `s_uNextCallbackHandle`, `s_xCallbackMutex`
- ModTimes: `s_xFileModTimes`
- OS handles: `s_hDirectory`, `s_hCompletionPort`, `s_hWatchThread`,
  `s_bWatchThreadRunning`

### UI/Zenith_UICanvas — `Zenith\UI\Zenith_UICanvas.h:157-160`
**Classification: migrate.**
- `s_pxPrimaryCanvas` (Zenith_UICanvas*)
- `s_xPendingTextEntries` (Zenith_Vector<UITextEntry>)

### Prefab — `Zenith\Prefab`
**Classification: stateless / per-instance (no migration needed).**
No module-scope or class statics. Prefab instances are managed by the
AssetRegistry; instantiation is per-scene.

### MemoryTracker — `Zenith\Core\Memory\Zenith_MemoryTracker.h:101-112`
**Classification: carve-out (allocator-level, must remain available
during static init / before Engine::Initialise()).**
- `s_xAllocations` (unordered_map<void*, Zenith_AllocationRecord>)
- `s_uFreedIndex`, `s_xStats`, `s_xMutex`, `s_ulFrameNumber`
- `s_ulNextAllocationID` (atomic)
- `s_bInitialised`
- Plus `Zenith_MemoryManagement.cpp:30`: `g_bMemoryManagementInitialised`
  (atomic) — must remain process-global per the existing init-order
  rationale.
- `Zenith_MemoryBudgets.cpp:188`: `s_xEmpty` (constant fallback) —
  carve-out (constant).

### FileAccess — `Zenith\FileAccess`
**Classification: stateless.**
No module-scope statics. Stateless file-IO helpers only.

### Component / behaviour-script / callback registries
**Static-init populated; Engine reads side-lists during Initialise.**

- `ZENITH_REGISTER_COMPONENT` macro: **19 occurrences across 17 files**
  in `Zenith/EntityComponent/Components/` and `Zenith/AI/Components/`.
- Side-list storage: `Zenith_ComponentMeta` (`Zenith\EntityComponent\
  Zenith_ComponentMeta.h`) — confirms the registry singleton pattern.
- `ZENITH_BEHAVIOUR_TYPE_NAME` macro: writes into
  `Zenith_ScriptAsset::s_xFactoryMap` and `s_xInternalNames` at static
  init.
- **Carve-out** the macros + their side-lists per the plan; `Zenith_Engine`
  reads from them on construction.

### Flux subsystems
**Pattern confirmed: 40 files under `Zenith\Flux/` carry module-scope
or class statics.** Subsystem count consistent with the ~25 expected
plus infrastructure (Backend, Shaders, Slang, RenderGraph). Full
per-subsystem audit deferred to Phase 7.

## Construction model validation — **VERIFIED**

Compiled the standalone prototype `constinit_prototype.cpp` (see
`Docs/design/` in this worktree) against MSVC C++20 (toolset 14.41).
**Result: `constinit Zenith_Engine g_xEngine;` compiles cleanly.**

Key findings:
- `constinit` requires **constant initialisation**, not trivial
  initialisation. In-class member initialisers (`= nullptr`) are
  constant expressions and satisfy this.
- The right compile-time guard is
  `static_assert(std::is_trivially_destructible_v<Zenith_Engine>);`,
  NOT `is_trivially_default_constructible_v` (which would fail because
  in-class initialisers make the ctor non-trivial in the type-traits
  sense — even though every initialiser is constant). Triviality of
  destruction is what prevents the static-destruction-order fiasco we
  care about.
- All-pointer-members with `= nullptr` initialisers + `= default`
  ctor/dtor → satisfies `constinit` and `is_trivially_destructible_v`.
- Forward-declared pointer-to-incomplete-type members are fine
  (pointer to incomplete type is a complete type).

Build command (reusable):
```
cmd /c "C:\tmp\Zenith-engine-refactor\Docs\design\build_constinit.cmd"
```
Verifies the design pattern doesn't drift as new subsystem pointers are
added during the refactor.

## EntityStore migration analysis

### Current shape

Three static arrays defined in `Zenith\EntityComponent\Zenith_SceneData.h:405-407`
and instantiated in `Zenith_SceneData.cpp:14-16`:

```cpp
static Zenith_Vector<Zenith_EntitySlot>                        s_axEntitySlots;
static Zenith_Vector<uint32_t>                                 s_axFreeEntityIndices;
static Zenith_Vector<std::unordered_map<TypeID, u_int>>        s_axEntityComponents; // #TODO replace with engine hash map
```

Existing reset helper: `Zenith_SceneData::ResetGlobalEntityStorage()`
(`Zenith_SceneData.cpp:18`) — clears all three. Already called from
`Zenith_SceneManager.cpp:227` during scene reset.

### Why static today
Per `EntityComponent/CLAUDE.md`: process-wide by design so
`EntityID.m_uIndex` stays stable across `MoveEntityToScene` and
persistence. Slot entries record `m_iSceneHandle` so destination
resolution stays correct.

### Migration target
Engine-owned `Zenith_EntityStore` class (or three members directly on
`Zenith_SceneManager`). Accessed through `g_xEngine.Scenes().EntityStore()`
or the chosen shape. Same single-instance semantics — moving the
storage doesn't change behavior in a single-engine model.

### Files touched (9 source files)

| File | Type of reference |
|------|-------------------|
| `Zenith\EntityComponent\Zenith_SceneData.cpp` | Definitions + many accessor bodies |
| `Zenith\EntityComponent\Zenith_SceneData.h` | Declarations + inline accessors |
| `Zenith\EntityComponent\Zenith_SceneData_Serialization.cpp` | `.zscen` load/save touches slots |
| `Zenith\EntityComponent\Zenith_Entity.cpp` | Constructor allocates a slot |
| `Zenith\EntityComponent\Zenith_Entity.inl` | Templated component access via slot indices |
| `Zenith\EntityComponent\Internal\Zenith_SceneRegistry.cpp` | Slot lookups during scene queries |
| `Zenith\EntityComponent\Internal\Zenith_SceneEntityOwnership.cpp` | `MoveEntityToScene` / persistence — updates `m_iSceneHandle` in slot |
| `Zenith\EntityComponent\Zenith_SceneManager.Tests.inl` | Test fixtures |
| `Zenith\EntityComponent\CLAUDE.md` | Doc reference only |

### `.zscen` serialization impact

EntityIDs in `.zscen` files are indices + generations. On load,
`Zenith_SceneData_Serialization.cpp` allocates fresh slots in the
global table and re-keys file IDs to runtime IDs. Storage relocation
(static → engine member) is transparent through this re-keying path —
file IDs never directly equal storage indices.

**Verification plan (Phase 5, not now):**
1. Snapshot a representative tools-built `.zscen` (DP main scene +
   1–2 prefabs).
2. Boot engine, load scene, dump runtime entity list.
3. Save scene, boot fresh, load again, dump runtime entity list.
4. Diff (1) vs. (3) on observable component state — should match.
5. If diff is empty, storage relocation is verified transparent.

### Recommended shape (Phase 5)

Make `Zenith_EntityStore` a member of `Zenith_SceneManager` (held as a
raw pointer per the `constinit` rule on `Zenith_Engine`). The reset
helper becomes `Zenith_EntityStore::Reset()`. `Zenith_SceneData`'s
static accessors become non-static methods that go through
`m_pxOwningManager->EntityStore()` — but `SceneData` already lives
inside `SceneManager`'s slot table, so the back-pointer is cheap.

### Out-of-scope simplifications

The `#TODO: Replace with engine hash map` comment on
`s_axEntityComponents` is a separate refactor (replacing
`std::unordered_map` with a Zenith-native container). Track but don't
fold into Phase 5.

## Phase impact summary

| Phase | Surface (estimate, refresh during execution) |
|-------|----------------------------------------------|
| 0 | `Zenith_Main.cpp:61-216` body — 1 file. |
| 1 | New automated test using existing harness — minimal code. |
| 2 | 29 `Zenith_Core` timing sites + FrameContext type. |
| 3a | Multithreading: 2 platform files. |
| 3b | TaskSystem (26 sites) + Profiling (7 statics + ~28 BeginProfile/EndProfile sites + 8 macro sites). |
| 4 | AssetRegistry + Physics — full counts TBD per TODOs above. |
| 5 | SceneManager: ~184 callback reg/unreg sites + 5 internal subsystems + entity arrays. |
| 5.5 | Input (15+ statics) + Editor (38+ statics). |
| 5.7 | DebugVariables + callback API adaptation. |
| 6a | Flux namespace + Flux_Graphics (~12 statics) + new Flux_Renderer shell. |
| 6b | Zenith_Vulkan backend + MemoryManager + VRAM + PerFrame. |
| 7 | ~25 Flux subsystems. |
| 8 | 11 game projects (per-game extern globals). |
| 9 | Static-API removal sweep. |
| 10 | Optional leak tracking enable. |
