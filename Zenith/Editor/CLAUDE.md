# Editor System

## Overview

ImGui-based scene editor for creating, editing, and testing game content. Active only in tools builds (`#ifdef ZENITH_TOOLS`). Features dockable panels, entity manipulation, play/pause/stop modes, undo/redo, and 3D gizmo integration.

**Core Design:** Deferred operations pattern prevents concurrent access to scene data during active render tasks. Scene loads (open/registered/play-stop restore) are queued and executed at safe synchronization points; New Scene and Save Scene run directly from the menu callback (Zenith_Editor::RenderImGuiFrame runs after render tasks complete, so direct execution is safe there).

## Files

- `Zenith_Editor.h/cpp` - Main controller, mode management
- `Zenith_SelectionSystem.h/cpp` - Entity picking via raycasting (ray-AABB, ray-triangle)
- `Zenith_Gizmo.h/cpp` - Screen-to-world ray conversion utilities
- `Zenith_UndoSystem.h/cpp` - Command pattern undo/redo with history stack
- `Zenith_Editor_MaterialUI.h/cpp` - Material editing UI
- `Zenith_EditorState.h` - Editor state management
- `Zenith_EditorCamera.cpp` - Editor camera implementation
- `TerrainEditor/` - Terrain sculpting/painting subsystem (`Zenith_TerrainEditor`):
  height brushes (raise/lower/smooth/flatten/set-height/noise/terrace/ramp/
  copy-stamp with radius/strength/falloff), splatmap layer painting (4 material
  slots, weights kept normalized), grass-density painting (Flux_Grass density
  map), seeded procedural generation (deterministic integer-hash FBM/ridged),
  hydraulic+thermal erosion (main-thread sliced or synchronous), auto-splat by
  slope/height rules, region-delta undo, save (.ztxtr to game assets) + full
  bake (chunk re-export + physics + render re-init). Live height edits go ONLY
  through the terrain streaming hook + EvictLOD re-stream (race-free; never an
  in-place write to a resident chunk); live splat paints re-upload via the
  staged `UpdateTextureVRAM` path. `ServiceUpdate` runs every editor frame
  (edits stay visible in Play); interactive brush input is Stopped-only and
  claims viewport clicks ahead of gizmo/picking. Editor automation drives the
  same API via `AddStep_Terrain*` (RenderTest generates its terrain this way,
  seed 1337).
- `Zenith_EditorAutomation.h/cpp` - Boot-time authoring step queue (scenes, entities,
  components, UI, terrain, Behaviour Graphs); games' `Project_RegisterEditorAutomationSteps`
  enqueue steps, drained before the initial scene load. See "Graph Authoring via
  Editor Automation" below for the graph verbs.
- `Zenith_ImGuiInputBridge.h/cpp` - Pumps `Zenith_InputSimulator` state into ImGui
  (TOOLS + INPUT_SIMULATOR builds) so automated tests drive editor UI deterministically.
- `Panels/` - Panel implementations (Console, ContentBrowser, GraphEditor, Hierarchy, MaterialEditor, Memory, Properties, TerrainEditor, Toolbar, Viewport)

## Related Systems

- **3D Gizmos:** See [Flux/Gizmos/CLAUDE.md](../Flux/Gizmos/CLAUDE.md) for rendering and interaction
- **Entity Component System:** See [EntityComponent/CLAUDE.md](../EntityComponent/CLAUDE.md) for scene architecture
- **ImGui:** Docking branch provides panel management

## Integration with Main Loop

### Frame Timing - Critical

Editor integrates into main loop at specific synchronization points:

```
BeginFrame (platform, swapchain)
  ↓
Zenith_Editor::Update() ← CRITICAL: Process deferred scene operations FIRST
  ↓
Physics::Update (conditional)
  ↓
Scene::Update (conditional)
  ↓
UploadFrameConstants
  ↓
SubmitRenderTasks
  ├─ Zenith_Editor::RenderImGuiFrame() (composes the ImGui frame, calls Render())
  └─ Flux_Gizmos::SubmitRenderTask()
  ↓
WaitForRenderTasks
  ↓
EndFrame (recording + present)
```

**Why Update() Must Be First:**
- Deferred scene load/reset calls `Zenith_Scene::Reset()` which destroys component pools
- If Reset() happens during active render tasks, concurrent access causes crashes
- Update() executes BEFORE any render tasks start, ensuring safe scene modification
- Location: `Zenith_Core::Zenith_MainLoop()`

### Deferred Operations Pattern

Three operations use deferred execution, all scene loads:

1. **Scene Load from file** - User picks a file in "Open Scene" / content browser, actual load next frame
2. **Registered Scene Load** - Toolbar dropdown sets the build index, load next frame
3. **Play→Stop Restore** - EnterStopMode queues the backup-scene load, restore next frame

New Scene and Save Scene execute directly in the menu callback (no render tasks are active during RenderImGuiFrame).

**Synchronization Sequence:**
- Menu items rendered during `Zenith_Editor::RenderImGuiFrame()` (render tasks active)
- Flags set, no immediate action
- Next frame: `Update()` checks flags BEFORE any rendering starts
- Safe to modify scene data (no concurrent access)

## Editor Modes

Three execution states control editor behavior:

### Stopped Mode
- **Scene State:** Loaded but not executing game logic
- **Camera:** Editor camera active (WASD + mouse look controls)
- **Gizmos:** Enabled, can manipulate selected entity
- **Physics:** Not simulated
- **Scripted Components:** Not updated

### Playing Mode
- **Scene State:** Game logic executing normally
- **Camera:** Scene's main camera active
- **Gizmos:** Disabled (W/E/R keys ignored)
- **Physics:** Full simulation at fixed timestep
- **Scripted Components:** Update() called each frame

### Paused Mode
- **Scene State:** Execution halted mid-play
- **Camera:** Game camera frozen at last position
- **Gizmos:** Enabled for inspection/editing
- **Physics:** Suspended
- **Scripted Components:** Not updated

### Mode Transitions

**Stopped → Playing:**
1. Serialize entire scene to backup file
2. Locate game camera entity (first camera component found)
3. Save editor camera state (position, rotation, FOV)
4. Switch to game camera for rendering
5. Begin physics simulation and script updates

**Playing/Paused → Stopped:**
1. Set deferred scene load flag with backup file path
2. Next frame: process the deferred load before render tasks start (no GPU wait — teardown frees GPU resources via the deferred-deletion grace period)
3. Load backup scene (calls Reset() then deserialize)
4. Restore editor camera from saved state
5. Delete backup file
6. Clear selection and undo history (EntityIDs invalid)

**Why Deferred Restore:** Playing → Stopped transition must defer scene load because:
- Immediate execution would Reset() scene during active render tasks
- New terrain components created during load wouldn't be registered with streaming manager
- Load must happen BEFORE SubmitRenderTasks() for proper component initialization

### Static State in Game Components

**CRITICAL:** Static variables in game components persist across Play/Stop/Play cycles because the executable is not reloaded. Game components MUST manually reset all static state in `OnAwake()`. (Behaviour-Graph state is immune: graph blackboards are re-seeded from the asset's declared defaults on every instantiation.)

**What Must Be Reset:**
- Static containers (vectors, maps) holding entity IDs or game state
- Static system instances (damage systems, enemy managers, etc.)
- Static event subscriptions - unsubscribe old handles before resubscribing to prevent accumulation

**Symptoms of Stale Static State:**
- First play works correctly, subsequent plays behave differently
- Event handlers fire multiple times (orphaned subscriptions accumulate)
- Systems reference invalid entity IDs from previous session
- Health/damage systems report entities as dead when they shouldn't be

See `Combat_GameComponent::OnAwake()` for a reference implementation.

## UI Panel System

### Docking Layout

ImGui docking branch provides central dock space with persistent layout. Panels can be dragged, resized, tabbed, and detached.

### Main Menu Bar

**File Menu:**
- New Scene - Force-unloads the active scene, creates a fresh empty scene (direct, not deferred)
- Open Scene - File dialog, deferred load
- Save Scene - File dialog, direct save via Zenith_EditorSceneAccess::SaveToFile
- Exit - Closes application

**Edit Menu:**
- Undo (Ctrl+Z) - Command pattern reversal
- Redo (Ctrl+Y) - Re-execute undone command
- Tooltips show next undo/redo description

**View Menu:**
- Toggle individual panels on/off
- Animation State Machine Editor (currently disabled)

### Toolbar

Horizontal button bar below menu:
- **Play/Pause/Stop Buttons** - Mode control with icons
- **Gizmo Mode Buttons** - Translate/Rotate/Scale (W/E/R shortcuts)

### Hierarchy Panel

Lists all scene entities with selection and manipulation:
- Each entity shown as: `EntityName [ComponentCount]`
- Falls back to `Entity_{ID}` if no name set
- Click to select entity (single selection only)
- Right-click context menu for deletion
- "+ Create Entity" button at bottom

**Implementation Detail:**
- Uses EntityID for selection (not raw pointers) to prevent dangling references
- Selection persists across frames but clears on scene load

### Properties Panel

Displays and edits components of selected entity:
- **Entity Name Field** - Editable text input
- **Component Sections** - One collapsible header per component
- **Component Properties** - Rendered via ComponentRegistry callbacks
- **Add Component Button** - Popup menu filtered to available component types
- **Remove Component** - Per-component "X" button

**Registry Integration:**
- Editor discovers components via ComponentRegistry at runtime
- Property editors registered as callbacks: `void(*)(Zenith_Entity&)`
- Enables extensible editor without modifying Zenith_Editor.cpp

### Viewport Panel

Renders game scene as ImGui texture with interaction handling:
- **Display:** ImGui::Image with GBuffer color texture
- **Mouse Conversion:** Tracks viewport position/size for coordinate transformation
- **Gizmo Rendering:** Submits Flux_Gizmos render task when entity selected
- **Object Picking:** Left-click raycasts against scene entities
- **Resource Management:** Deferred descriptor set deletion (waits 3 frames)

**Viewport-Relative Input:**
- Mouse position converted from screen-space to viewport-relative
- Ray casting accounts for viewport offset within ImGui window
- Only processes input when `s_bViewportHovered` flag set

### Content Browser

File browser for asset discovery and drag-drop:
- **Navigation:** Directory tree starting at GAME_ASSETS_DIR
- **Sorting:** Directories first, then alphabetically by name
- **Display:** Icons + filenames, double-click to enter directories
- **Drag-Drop:** Supports Texture, Mesh, Material, Prefab payloads
- **Filtering:** Shows all files with extensions (no filtering yet)

**Drag-Drop Payload:**
- 32-character type identifier (ImGui limit)
- File path as payload data
- Absolute paths used for reliability

### Console Panel

Displays engine log messages with filtering:
- **Message Format:** `[HH:MM:SS] Message text`
- **Color Coding:** Info (gray), Warning (yellow), Error (red)
- **Filters:** Checkboxes for Info/Warnings/Errors visibility
- **Controls:** Auto-scroll toggle, Clear button
- **Limit:** Max 1,000 entries (oldest discarded)

**Log Redirection:**
- Engine's Zenith_Log(), Zenith_Warning(), Zenith_Error() macros append to console
- Timestamp captured at log time
- Thread-safe (mutex-protected writes)

### Material Editor Panel

Create and edit materials with texture assignment:
- **File Operations:** Create new, Load from disk, Save to .zmtrl
- **Texture Slots:** Diffuse, Normal, Roughness, Metallic
- **Assignment:** Drag-drop from Content Browser to slot
- **Preview:** Texture thumbnail in each slot
- **Reload Button:** Live refresh without restarting editor

### Behaviour Graph Editor Panel (`Panels/Zenith_EditorPanel_GraphEditor`)

The hand-rolled node editor for `.bgraph` Behaviour Graph assets (the runtime
is `Zenith/Scripting/` — see its CLAUDE.md):

- **Palette** — registered node types grouped by editor category; click to
  place at the next free canvas spot.
- **Canvas** — drag nodes; drag an output pin onto an input pin to connect
  (one edge per (node, pin) enforced); right-click an output pin to
  disconnect; Delete removes the selected node.
- **Parameter editing** — the reflected-property auto panel (`ZENITH_PROPERTY`)
  for the selected node: float/int/bool/string/vector3 fields.
- **Blackboard variable panel** — declare variables with type combo
  `"float" / "int" / "bool" / "string" / "vector3"` + numeric default.
- **Unresolved nodes** render error-red ("UNRESOLVED") when the type isn't in
  `Zenith_GraphNodeRegistry`; the asset round-trips them verbatim.
- **Live execution highlighting** — while Playing, recently-executed nodes of
  the selected entity's matching graph slot glow (fed by
  `Zenith_BehaviourGraph::GetRecentlyExecuted`).
- **Open/Save/Close:** `OpenAsset` (registry-backed), `OpenAssetFresh`
  (boot-time authoring: clears the definition for regenerate-from-scratch),
  `Save` (creates parent directories, writes through the asset registry, then
  queues `Zenith_GraphReload::NotifyAssetChanged` → live instances hot-swap at
  the next safe point).

**Atomic `Action_*` verbs.** Every UI gesture has a static, bool-returning
twin that performs EXACTLY the handler's body — `Action_AddNode(typeName)`,
`Action_SelectNode(typeName, occurrence)`,
`Action_SetSelectedNodeParam{Float,Int,String,Vec3}(declaredFieldName, ...)`,
`Action_Connect(srcType, srcOcc, srcPin, dstType, dstOcc)`,
`Action_AddVariable(name, typeString, defaultNumeric)`. Nodes are addressed by
**(typeName, occurrence)** in creation order; param names are the DECLARED
property field names (`"m_fDegreesPerSecond"`, not `"DegreesPerSecond"`).

**ZENITH_TESTING accessors** record live screen rects each Render so simulated
input can click real coordinates: `GetPaletteEntryScreenPos`,
`GetNodeScreenPos`, `GetPinScreenPos`, `GetToolbarButtonScreenPos`,
`GetPropertyRowScreenPos/Rect`, plus state probes (`GetNodeCount`,
`GetEdgeCount`, `GetSelectedNodeID`, `FindNodeIDByType`, `IsDirty`).

**Simulated-input bridge** (`Zenith_ImGuiInputBridge`, gated
`ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR`): pumps `Zenith_InputSimulator` state
into ImGui IO events, injected in `Zenith_Vulkan::ImGuiBeginFrame` BETWEEN the
GLFW backend and `ImGui::NewFrame` so the last-event-wins queue makes
simulated input deterministic. This is what lets automated tests drive the
editor with real clicks/keys — flagship proofs: `Test_GraphEditorLiveAuthoring`
and `Test_GraphEditorScreenshotTour` (DP suite, windowed).

### Graph Authoring via Editor Automation

`Zenith_EditorAutomation` exposes one step per atomic editor verb, used by
games to regenerate their `.bgraph` assets every tools boot (exactly like
scene authoring): `AddStep_GraphOpenFresh`, `AddStep_GraphAddNode`,
`AddStep_GraphSelectNode`, `AddStep_GraphSetNodeParam{Float,String,Int,Vec3}`,
`AddStep_GraphConnect`, `AddStep_GraphAddVariable`, `AddStep_GraphSave`,
`AddStep_GraphClose`, plus `AddStep_AttachGraph(assetPath)`
(`Zenith_Editor::AttachGraphToSelected` — lazy-adds `Zenith_GraphComponent`
and appends the slot). Each graph step is wrapped in `GraphActionChecked`,
which asserts on failure so an authoring typo (wrong node type/occurrence/pin)
surfaces at boot, not as a silently-empty graph.

`ExecuteAction` routes two CONTIGUOUS enum ranges to sub-executors before its
main switch — terrain-editor actions → `ExecuteTerrainEditorAction`, UI actions
(`CREATE_UI_TEXT` .. `SET_UI_SCROLL_VIEW_CONTENT_SIZE`) → `ExecuteUIAction` —
keeping the dispatcher inside the complexity gate. Keep those ranges contiguous
when adding action types.

## Selection System

### Selection Model

Single entity selection using EntityID:
- `s_uSelectedEntityID` stores current selection
- ID-based (not pointer) for safety across scene reloads
- Selection cleared when entity deleted or scene loaded

### Selection Methods

**1. Hierarchy Click:**
- Direct selection from entity list
- Immediate, no raycasting required

**2. Viewport Click:**
- Left-click in viewport when not interacting with gizmo
- Raycasts from camera through mouse position
- Two-stage intersection test (see below)

**3. Programmatic:**
- Undo/redo commands can restore selection
- Component addition defaults to selecting new entity

### Raycast Implementation

**Two-Stage Intersection:**

1. **Coarse Phase (AABB):**
   - Test ray against every entity's bounding box
   - Slab method for axis-aligned box intersection
   - Build list of potential hits with distances
   - Skip entities without ModelComponent (not renderable)

2. **Precise Phase (Triangle):**
   - Take closest AABB hit only (optimize for single selection)
   - Ray-triangle intersection on mesh geometry
   - Möller-Trumbore algorithm for barycentric coordinates
   - Returns exact hit point on mesh surface

**Bounding Box System:**
- AABBs calculated per-frame via `UpdateBoundingBoxes()`
- Transforms applied to mesh AABB (local → world space)
- Stored per-entity for fast iteration
- Rebuilt every frame (entities can move)

**Screen-to-World Ray Conversion:**
- Viewport-relative mouse position (accounts for panel offset)
- Normalized device coordinates [-1, 1] range
- Inverse projection matrix to view space
- Inverse view matrix to world space
- Ray origin: camera position
- Ray direction: normalized vector from origin through pixel

## Gizmo Integration

Editor integrates with Flux_Gizmos for 3D transform manipulation. Architecture split between utility functions (Zenith_Gizmo) and rendering system (Flux_Gizmos).

### Zenith_Gizmo (Utilities)

Located in `Editor/Zenith_Gizmo.h/cpp`, provides one helper:
- `ScreenToWorldRay()` - Converts 2D viewport coords to 3D world ray (screen → viewport → clip → world)

(The legacy ImGui-drawlist translate gizmo that used to live here was superseded by Flux_Gizmos and has been deleted.)

**Coordinate System Considerations:**
- Vulkan depth range [0, 1] not OpenGL [-1, 1]
- Projection matrix handles Y-axis flip (no manual inversion)
- Ray construction uses `z = 0.0` for near plane in clip space

### Flux_Gizmos (Rendering)

Located in `Flux/Gizmos/`, handles 3D rendering and interaction. See [Flux/Gizmos/CLAUDE.md](../Flux/Gizmos/CLAUDE.md) for full documentation.

**Three Gizmo Modes:**
- **Translate:** Arrow geometry on X/Y/Z axes
- **Rotate:** Circle rings for rotation around axes
- **Scale:** Cube handles for per-axis + center uniform scaling

**Keyboard Shortcuts:**
- W - Translate mode
- E - Rotate mode
- R - Scale mode
- Only active when viewport focused and not in Playing mode

### Interaction Flow

```
HandleGizmoInteraction():
  1. Get viewport-relative mouse position
  2. ScreenToWorldRay from camera through mouse
  3. If mouse pressed and gizmo hit:
       Flux_Gizmos::BeginInteraction(ray, entityID)
  4. If mouse held during interaction:
       Flux_Gizmos::UpdateInteraction(ray)
  5. If mouse released:
       Flux_Gizmos::EndInteraction()

  If NOT interacting:
    SetTargetEntity(selectedID)
    SetGizmoMode(currentMode)

  SubmitRenderTask()
```

**Critical Safety Rule:** `SetTargetEntity()` and `SetGizmoMode()` only called when NOT actively interacting. Calling during interaction would reset internal state and corrupt the drag operation.

### Transform Application

Gizmo system directly modifies TransformComponent:
- **BeginInteraction:** Captures initial transform state
- **UpdateInteraction:** Calculates delta from initial state, applies to component
- **EndInteraction:** Finalizes transform, creates undo command

**Undo Integration:**
- Transform edits automatically create `Zenith_UndoCommand_TransformEdit`
- Stores before/after position, rotation, scale
- EntityID-based (safe across scene changes)

## Undo/Redo System

Command pattern implementation with history stack management.

### Command Interface

Base class `Zenith_UndoCommand` requires:
- `Execute()` - Perform the action
- `Undo()` - Reverse the action
- `GetDescription()` - Human-readable text for UI tooltip

### Command Types

**TransformEdit** (the only command type):
- Stores: EntityID, before/after position, rotation, scale
- Execute: Apply "after" transform
- Undo: Restore "before" transform
- Created automatically by gizmo interactions

(The never-instantiated CreateEntity/DeleteEntity command stubs were deleted; entity deletion from the hierarchy panel is not undoable.)

### Stack Management

**Undo Stack:**
- LIFO (last-in-first-out) vector
- Most recent command at back
- Ctrl+Z pops from back, executes Undo(), pushes to redo stack
- Max 100 commands (oldest discarded when exceeded)

**Redo Stack:**
- Cleared whenever new command executed (branching timeline)
- Ctrl+Y pops from redo, executes Execute(), pushes to undo stack

**Stack Clearing:**
- Scene load: All EntityIDs become invalid, clear both stacks
- Scene reset: Same reason
- Play → Stop: Scene restored from backup, EntityIDs change

### Keyboard Shortcuts

- **Ctrl+Z** - Undo last action
- **Ctrl+Y** - Redo previously undone action
- Menu items show next action description in tooltip

## Editor Camera System

Dual camera architecture supports editing and playtesting.

### Camera Modes

**Editor Camera (Stopped/Paused):**
- Controlled by editor input (WASD + mouse look)
- Position, pitch, yaw stored in editor state
- Independent of scene entities
- Persistent across scene loads

**Game Camera (Playing):**
- Uses scene's main camera component
- Controlled by game scripts
- Delegates view/projection matrix building to camera component

### Editor Camera Controls

**Right-Click + Drag:**
- Enables mouse look (cursor hidden)
- Drag horizontally: Yaw rotation
- Drag vertically: Pitch rotation
- Release: Restore cursor, disable look

**WASD Movement:**
- Only active during right-click drag
- W/S: Forward/backward along look direction (horizontal plane only)
- A/D: Left/right strafe
- Yaw-relative (not world-aligned)

**Q/E Vertical:**
- Q: Move down (world -Y)
- E: Move up (world +Y)
- World-space (not camera-relative)

**Shift Modifier:**
- Hold Shift: 3× movement speed
- Applies to all WASD/Q/E movement

### Camera State Persistence

**Stopped → Playing:**
- Save editor camera: position, pitch, yaw, FOV, near/far planes
- Locate game camera: Find first CameraComponent in scene
- If no camera found: Use editor camera as fallback

**Playing → Stopped:**
- Restore editor camera from saved state
- Ensures consistent editing experience

**Scene Load:**
- Editor camera initialized from loaded scene's main camera
- First frame only, then editor maintains independent state

## Thread Safety and Synchronization

### Main Thread Restriction

All editor operations execute on main thread only:
- ImGui requires single-threaded UI rendering
- Scene modifications unsafe during worker thread execution
- Deferred operations guarantee safe timing

### Synchronization Points

**Scene Load/Reset:**
1. Runs in Update(), before render-task submission — no CPU render tasks are active (asserted in Reset())
2. No GPU wait: every GPU resource the teardown frees is queued through `QueueVRAMDeletion`'s MAX_FRAMES_IN_FLIGHT+1 grace period, the same contract runtime `LoadScene` relies on mid-play
3. The unit-test entry point `FlushPendingSceneOperations()` still waits for GPU idle (`WaitForGPUAndFlushDeferred`) because it runs outside the frame loop where the per-frame deletion tick isn't running

**GPU Resource Lifecycle:**
- Descriptor sets deleted immediately from application state
- Actual Vulkan destruction deferred 3 frames
- Ensures GPU finished using resource before free
- Vector tracks `{descriptorSet, framesRemaining}` pairs

### Race Condition Prevention

**Why Deferred Operations:**
- Menu bar rendered during `Zenith_Editor::RenderImGuiFrame()` → active render tasks
- Immediate scene load would race with workers reading component data
- Deferred to next frame's `Update()` → executes BEFORE render tasks start

**Entity Deletion:**
- Hierarchy panel right-click sets deferred delete flag
- Actual deletion in Update() after synchronization
- Prevents iterator invalidation during UI rendering

## Input Handling

### Viewport Hover Detection

`s_bViewportHovered` flag determines input eligibility:
- Set to true when mouse inside viewport panel bounds
- Gizmo interaction only processes when true
- Object picking only when hovered
- Camera controls always active (not viewport-dependent)

### Input Priority

Gizmo interaction takes precedence over object picking:
1. Check if mouse pressed during active interaction → update gizmo
2. Else check if mouse pressed on gizmo geometry → begin interaction
3. Else check if mouse pressed on empty space → object picking
4. Camera controls independent (right-click drag always works)

### Keyboard Shortcut Filtering

Gizmo mode keys (W/E/R) only active when:
- Viewport is focused (ImGui focus state)
- Not in Playing mode (game logic uses those keys)
- Not during active text input (ImGui captures keyboard)

## Design Patterns

### Singleton Pattern
- `Zenith_Editor` provides static methods for global state access
- Single instance created at engine initialization
- Ensures consistent editor state across all systems

### Command Pattern
- Undo/redo implemented as reversible command objects
- Encapsulates actions with Execute/Undo methods
- Stack-based history management

### Registry Pattern
- ComponentRegistry enables extensible component system
- Editor discovers components at runtime
- Property editors registered as callbacks
- No hardcoded component types in editor code

### RAII (Resource Management)
- Deferred descriptor set deletion with frame counting
- Scoped mutex locks for thread safety
- Automatic resource cleanup on panel close

### Deferred Execution
- Scene operations queued as flags
- Processed at safe synchronization points
- Prevents concurrent access violations

## Critical Constraints

### EntityID Lifetime
- EntityIDs invalid after scene Reset()
- Undo/redo stack cleared on scene load
- Selection cleared when entity deleted
- Never store raw pointers to entities

### Gizmo Interaction State
- Cannot call SetTargetEntity() during active drag
- Cannot call SetGizmoMode() during active drag
- Violating causes transform corruption and state reset

### Deferred Operation Timing
- Update() must execute BEFORE any render tasks
- GPU must be idle before scene Reset()
- Descriptor sets wait N frames before GPU free

### Viewport Coordinate Conversion
- Must account for viewport panel offset in ImGui window
- Vulkan depth [0, 1] not OpenGL [-1, 1]
- Projection matrix handles Y-flip (no manual inversion)

## Performance Considerations

**UI Rendering:**
- ImGui rendering minimal overhead (~0.5ms for all panels)
- Texture uploads only when viewport resizes
- Deferred deletions prevent synchronous GPU waits

**Object Picking:**
- Two-stage intersection (AABB then triangle) optimizes for common case
- Only tests closest AABB hit for triangle intersection
- Bounding boxes rebuilt per-frame (acceptable for editor workload)

**Undo/Redo:**
- Transform edits lightweight (just 3 vectors)
- 100 command limit prevents unbounded memory growth

## Known Limitations

### Multi-Selection
- Multi-entity selection supported (SelectEntity, ToggleEntitySelection)
- Gizmo system operates on selected entities

### Component Reordering
- Components appear in registration order (hardcoded)
- No drag-drop reordering in Properties panel

### Performance Profiling
- No editor panel for profiling data visualization yet
- Must use external tools or log output

## Integration Summary

Editor system integrates with engine architecture via:
- **Zenith_Core:** Update() called each frame before rendering
- **Zenith_Scene:** Read/write access to entities and components
- **Flux_Gizmos:** 3D transform manipulation via separate render task
- **ComponentRegistry:** Runtime component discovery for extensibility
- **Zenith_Input:** Keyboard/mouse state for controls
- **ImGui:** Panel rendering and layout management

All operations respect thread safety through deferred execution and proper synchronization with rendering pipeline.
