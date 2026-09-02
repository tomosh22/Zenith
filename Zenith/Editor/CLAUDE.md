# Editor System

## Overview

ImGui-based scene editor for creating, editing, and testing game content. Active only in tools builds (`#ifdef ZENITH_TOOLS`). Features dockable panels, entity manipulation, play/pause/stop modes, undo/redo, and 3D gizmo integration.

**Core Design:** Deferred operations pattern prevents concurrent access to scene data during active render tasks. Scene loads (open/registered/play-stop restore) are queued and executed at safe synchronization points; New Scene and Save Scene run directly from the menu callback (Zenith_Editor::RenderImGuiFrame runs after render tasks complete, so direct execution is safe there).

## Files

- `Zenith_Editor.h/cpp` - Main controller, mode management, the dockspace host (menu bar + toolbar strip + dockspace + status bar), keyboard shortcuts, the "Save changes?" prompt and the Keyboard Shortcuts window
- `Zenith_Editor_Menu.cpp` - Main menu bar (File / Edit / Entity / Window / Help); every item calls a `Zenith_EditorActions` verb
- `Zenith_EditorActions.h/cpp` - **The verbs.** Create / delete / duplicate / rename / enable / reparent entities, add / remove components, new / open / save scenes (with the unsaved-changes prompt), play / pause / stop. Menus, shortcuts, toolbar buttons and panel context menus all call these, so one operation behaves identically from every entry point and undo + scene-dirtying live in exactly one place
- `Zenith_EditorCommands.h/cpp` - The undo command layer: `Zenith_EditorEntitySnapshot` (a serialised entity subtree that can be destroyed and rebuilt), `Zenith_UndoCommand_EntityLifetime` (delete / create / duplicate), `Zenith_UndoCommand_EntityState` (rename / enable / reparent), `Zenith_UndoCommand_ComponentBytes` (add / remove / edit ONE component by serialised payload), `Zenith_UndoCommand_Composite` (a multi-selection as one step), and `Zenith_EditorInspectorUndoTracker` (turns any Properties-panel edit into a command). Tests in `Zenith_EditorCommands.Tests.inl`
- `Zenith_EditorUI.h/cpp` - Look and feel: the embedded Roboto font (`Zenith_EditorFontData.generated.h`) at a DPI-aware base size, the theme + palette (sRGB values converted to linear for the sRGB swapchain), the play-mode tint, a vector icon set drawn straight into ImDrawLists, the styled widgets (icon buttons, search box, badges, the inspector component header), and the small helpers every panel shares rather than copying (`ContainsCaseInsensitive`, the `SmoothedFrameMs` frame-time filter)
- `Zenith_EditorPrefs.h/cpp` - Per-user, per-game preferences (`%LOCALAPPDATA%/Zenith/<Game>/editor_prefs.txt`): recent scenes, fly speed, look sensitivity, snapping, gizmo space, overlay toggles. `Parse` / `Serialize` are pure and unit-tested; never read in automated or headless runs
- `Zenith_Editor_SceneOps.cpp` - Scene load/save/new + deferred scene operations
- `Zenith_EditorQuery.cpp` - Editor-side scene queries (entity/component lookup)
- `Zenith_EditorSceneAccess.h` - Header-only friend-class wrapper exposing the editor-only `Zenith_SceneData` verbs (Save/LoadFromFile, RemoveEntity, SetMainCameraEntity, etc.) to editor code; `Zenith_Editor` is a namespace and cannot itself be `friend`ed, so this class is
- `Zenith_SceneGraphDebug.h/cpp` - Scene-graph debug visualization helpers
- `Zenith_SelectionSystem.h/cpp` - Entity picking via raycasting (ray-AABB, ray-triangle)
- `Zenith_Gizmo.h/cpp` - Screen-to-world ray conversion utilities
- `Zenith_UndoSystem.h/cpp` - Command pattern undo/redo with history stack
- `Zenith_Editor_MaterialUI.h/cpp` - Material editing UI
- `Zenith_EditorState.h` - Editor state management
- `Zenith_EditorCamera.cpp` - Editor camera implementation
- `TerrainEditor/` - Terrain sculpting/painting subsystem (`Zenith_TerrainEditor`):
  height brushes (raise/lower/smooth/flatten/set-height/noise/terrace/ramp/
  copy-stamp with radius/strength/falloff), splatmap layer painting (4 material
  slots, weights kept normalized), grass-density painting and grass-type
  stamping, tree painting (TreePaint scatters the ProceduralTree assets across two
  lockstep instanced entities; the TRUNK entity also authors a per-instance capsule
  collider config, so painted trees are solid, while the leaf cards deliberately stay
  collider-free) (the two Flux_Grass maps — a session owns FOUR maps in all,
  Height / Splat / GrassDensity / GrassType, each with its own texel count and
  bytes-per-texel, so undo rects are sized per map: 1 byte/texel for GrassType
  against 4 for the others), seeded procedural generation (deterministic
  integer-hash FBM/ridged),
  hydraulic+thermal erosion (main-thread sliced or synchronous), auto-splat by
  slope/height rules, region-delta undo, save (.ztxtr to game assets) + full
  bake (chunk re-export + physics + render re-init). Live height edits go ONLY
  through the terrain streaming hook + EvictLOD re-stream (race-free; never an
  in-place write to a resident chunk); live splat paints re-upload via the
  staged `UpdateTextureVRAM` path. `ServiceUpdate` runs every editor frame
  (edits stay visible in Play); interactive brush input is Stopped-only and
  claims viewport clicks ahead of gizmo/picking. Editor automation drives the
  same API via `AddStep_Terrain*` (RenderTest generates its terrain this way,
  seed 1337). It also owns a **working copy of the grass type table** (
  `GrassTypes()` + `GrassTypes_Reset/Reload/Apply/Save`): the panel's "Grass
  Types" section and the `AddStep_GrassTypes*` verbs both edit that one object,
  and only Apply/Save move the engine's live `Flux_GrassTypeTable`, so a
  half-typed value can never reach the placement compute shader.
- `Zenith_EditorAutomation.h/cpp` - Boot-time authoring step queue (scenes, entities,
  components, UI, terrain, Behaviour Graphs); games' `Project_RegisterEditorAutomationSteps`
  enqueue steps, drained before the initial scene load. See "Graph Authoring via
  Editor Automation" below for the graph verbs.
- `Zenith_ImGuiInputBridge.h/cpp` - Pumps `Zenith_InputSimulator` state into ImGui
  (TOOLS + INPUT_SIMULATOR builds) so automated tests drive editor UI deterministically.
- `Zenith_Editor.Tests.inl` / `Zenith_EditorAutomation.Tests.inl` - Unit tests for the editor controller and the automation step queue (included into the unit-test TU)
- `Panels/` - Panel implementations (Console, ContentBrowser, GraphEditor, Hierarchy, MaterialEditor, Memory, Properties, RenderGraph, StatusBar, TerrainEditor, Toolbar, VariantEditor, Viewport). Toolbar and StatusBar are strips drawn inside the dockspace host window, not dockable windows
- `../Core/Zenith_ImGuiWidgets.h/cpp` - Layer-0 ImGui widgets (`Vec3Field`, `PropertyLabel`) that component inspectors in EntityComponent may use without including `Editor/`
- `../Core/Zenith_EditorFontHook.h` - `Zenith_EditorFonts_Load()`, called by the Vulkan and Null backends right after `ImGui::CreateContext` so the editor font is registered before either backend builds the atlas (the Null backend's legacy atlas is locked at the first NewFrame)

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

### Toolbar strip

One row directly under the menu bar, part of the dockspace host (it can never
be docked away, closed, or clipped — it used to be a dock node whose 8% split
hid its second and third rows at 720p):
- **Undo / Redo** with the next description in the tooltip
- **Move / Rotate / Scale** (W / E / R), **Local / World** space (X), **Snap**
  toggle (right-click edits the increments; holding Ctrl snaps for one drag)
- **Play / Pause / Stop** centred (Ctrl+P, Ctrl+Shift+P); the Play button turns
  into a green-lit icon while running and the whole chrome takes a warm tint
- **Active scene** combo (dirty scenes show `*`) and the registered-scene loader
  on the right

### Status bar

The strip along the bottom of the host: the latest log line (icon per level;
click reveals the Console) and, on the right, entity count, selection count,
fps / frame ms and undo / redo depth.

### Hierarchy Panel

Lists every loaded scene as a framed header (icon, name, `*` when dirty,
entity-count badge) with its entity tree:
- Rows show a component icon (camera / light / mesh / terrain / ...), the name
  (dimmed when disabled) and an **eye toggle** on hover that enables / disables
  the entity (undoable)
- **Search** box filters the tree while keeping matching subtrees visible and
  auto-expanded; matches name or component names (`MatchesSearch` is pure)
- Click to select (Ctrl+click toggles, Shift+click ranges); the viewport scrolls
  the tree to whatever it picks
- **Double-click or F2** renames inline (undoable); Enter commits, Esc cancels
- Right-click: Rename, Duplicate, Delete, Focus, Create Child ▸, Unparent,
  Enable / Disable, Move To Scene ▸; the scene header offers Set Active, Save,
  Save As, Unload, Create ▸, Pause
- Drag a row onto another to reparent (undoable, cycles refused), onto a scene
  header to move it there, onto the empty space at the bottom to unparent
- The `+` button at the top creates Empty / Camera / Point / Spot / Directional
  Light entities at the camera's placement point

**Implementation Detail:**
- Uses EntityID for selection (not raw pointers) to prevent dangling references
- Selection persists across frames but clears on scene load
- Entity keys (Delete, Ctrl+D, F, Esc, Ctrl+A) are scoped to the hierarchy and
  the viewport, so a Delete pressed in the console never removes an entity

### Properties Panel

Displays and edits the primary selected entity:
- **Header** - enabled checkbox, name field (rename is undoable), scene / id
  line, Transient toggle, "1 of N selected" badge for multi-selections
- **Component Sections** - one framed header per component, **drawn by the
  panel** (`Zenith_EditorUI::ComponentHeader`: icon, name, right-edge remove
  button). Component `RenderPropertiesPanel` bodies draw ONLY their rows — no
  component calls `CollapsingHeader` for itself any more. The Transform uses
  `Zenith_ImGuiWidgets::Vec3Field` (colour-tagged X/Y/Z fields; click a tag to
  reset that axis)
- **Add Component** - a searchable popup (type to filter, Enter adds the first
  match); already-present components are listed disabled
- **Every edit is undoable.** `Zenith_EditorInspectorUndoTracker` opens a
  session on any click inside the panel (or Tab), snapshotting each component's
  serialised bytes; when the UI goes idle again it diffs and records one
  `ComponentBytes` command per changed component (the Transform diffs as
  position / rotation / scale into a `TransformEdit`, so physics bodies follow
  an undo the way they follow a gizmo drag). Component inspectors need no undo
  code of their own. Component ADD / REMOVE go through `Zenith_EditorActions`
  and cancel the tracker's session so they are not double-recorded

**Registry Integration:**
- Editor discovers components via ComponentRegistry at runtime
- Property editors registered as callbacks: `void(*)(Zenith_Entity&)`; the
  registry entry also carries `m_pfnRemoveComponent` for the remove button
- Enables extensible editor without modifying Zenith_Editor.cpp

### Viewport Panel

Renders game scene as ImGui texture with interaction handling:
- **Display:** ImGui::Image with the final render target, edge to edge
- **Overlays** (draw-list only, never ImGui items): a PLAYING / PAUSED badge,
  a statistics block (fps / ms, gizmo mode + space + snap, camera position and
  fly speed), a navigation hint that changes while looking, and an **axis
  widget** (bottom-right) projecting world X/Y/Z through the view rotation
  (`ProjectAxisForWidget` is pure). Window > Viewport toggles the stats, the
  axes and the selection bounds; the toggles persist in the prefs
- **Selection bounds:** every selected entity's AABB is drawn as an orange
  wireframe through the Flux_Primitives debug channel
- **Mouse Conversion:** Tracks viewport position/size for coordinate transformation
- **Object Picking:** Left-click raycasts against scene entities; Ctrl toggles,
  Shift adds, an empty click clears. A camera gesture (Alt+LMB orbit) never picks
- **Resource Management:** Deferred descriptor set deletion (waits 3 frames)

**Viewport-Relative Input:**
- Mouse position converted from screen-space to viewport-relative
- Ray casting accounts for viewport offset within ImGui window
- Only processes input when `m_xEditorState.m_xViewport.m_bHovered` flag set

### Content Browser

Unreal-style asset browser:
- **Folder tree** on the left (cached, rebuilt on Refresh; the current folder is
  highlighted, its branch auto-expanded), with a draggable splitter; the tree
  can be hidden from the top bar
- **Top bar:** back / forward / up / refresh icons, breadcrumbs, search, type
  filter, tile / detail view toggles and a tile-size slider (Ctrl+Scroll)
- **Tiles:** rounded cards with a type-coloured plate + icon, a short type
  badge (TEX / MAT / SCN ...), live thumbnails for textures, ellipsised names;
  the detail view is a table with an icon column
- **Double-click** opens scenes (through the unsaved-changes prompt),
  materials (Material Editor) and behaviour graphs (Graph Editor)
- **Drag-Drop:** Supports Texture, Mesh, Material, Prefab, Animation, Graph and
  generic file payloads (the drag preview shows the type icon)
- **Context menus:** Open / Open Additive (scenes), Duplicate, Delete, Export to
  .ztxtr (png / jpg), Show in Explorer, Copy Path; empty space offers Create
  Folder / Material, Show in Explorer, Refresh

**Drag-Drop Payload:**
- 32-character type identifier (ImGui limit)
- File path as payload data
- Absolute paths used for reliability

### Console Panel

Displays engine log messages with filtering:
- **Rows:** level icon + time | category | message, in a clipped table
- **Level toggles** carry live counts (Info N / Warnings N / Errors N)
- **Filters:** category popup and a text search (`PassesFilters` is pure)
- **Controls:** Clear, Collapse (consecutive identical messages fold into one
  row with an `xN` badge), Auto-scroll; right-click or Ctrl+C copies a message
- **Limit:** Max 1,000 entries (oldest discarded)
- Window > "Clear Console On Play" clears it on every Play
- `LevelIcon` / `LevelColour` are the ONE definition of how a log level looks;
  the status bar draws its latest-message line with them

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

**★ EVERY position accessor returns FALSE for an OFF-SCREEN rect, and the
palette must be scrolled before it is clicked.** The palette lists every
registered node type, so the left column's content is thousands of pixels tall
— far more than the window or the display — and most rows are scrolled out of
view at any moment. A clipped ImGui item is **not interactable**, so
`ScrollPaletteEntryIntoView(typeName)` must be called first and given a frame to
land (it is applied by the next `Render`) before reading the position or issuing
the click. Palette rows are additionally recorded only while `IsItemVisible()`.

This is a hard-won contract. The accessors used to hand out the *virtual*
(scrolled-away) rect, so `Test_GraphEditorLiveAuthoring` was clicking screen
y=1768 on a 720-tall display and reporting only "the nodes were not created" —
the click, the bridge and the panel were all healthy and none of them was at
fault. The palette also has its **own** scroll child for the same reason:
sharing one with the properties meant scrolling to a palette entry pushed the
property rows off the *top* (observed y=-2488). Failing closed turns both into
an immediate, local error instead of a click into empty space.

**Simulated-input bridge** (`Zenith_ImGuiInputBridge`, gated
`ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR`): pumps `Zenith_InputSimulator` state
into ImGui IO events, injected in `Zenith_Vulkan::ImGuiBeginFrame` BETWEEN the
GLFW backend and `ImGui::NewFrame` so the last-event-wins queue makes
simulated input deterministic. This is what lets automated tests drive the
editor with real clicks/keys — flagship proofs: `Test_GraphEditorLiveAuthoring`
and `Test_GraphEditorScreenshotTour` (DP suite, windowed).

### Every backend authors the same scene, and every publish is audited

`Zenith_Editor::SaveActiveScene` — the one verb `AddStep_SaveScene` routes to, and
the only way an authored scene reaches disk — **publishes on every backend**, Null
included. That rests on a rule the authoring steps have to keep:

> **A `Zenith_IsNullRenderer()` bail is a defect whenever it skips ENTITY or
> COMPONENT creation, and correct when it skips device traffic.** Entity and
> component state is what `WriteToDataStream` serializes; the GPU allocation
> underneath it is already a no-op on the Null backend
> (`Zenith_Null_MemoryManager` hands back dummy handles and copies nothing), so
> the "skip only the GPU half" behaviour needs no branch in the authoring code at
> all.

The save is still preceded by `AuditScenePublish`, which asks
`Zenith_SceneData::CompareWithFile` (a `Zenith_ScenePublishDelta`) to serialize
exactly the bytes `SaveToFile` would write and diff them against the file:

| Delta | What the audit does |
|---|---|
| `NO_FILE` | **writes**, logging the counts — it is how a new game's scene first appears |
| `IDENTICAL` | **skips the write** and logs `[ScenePublish] IDENTICAL`. Nothing is lost (the bytes match) and the file's mtime stays put |
| `DIFFERENT`, fewer entities than the file | **writes**, and reports it with `Zenith_Error` naming both counts and both sizes |
| `DIFFERENT`, same or more entities | **writes**, logging the change |

The two rows that changed meaning are worth being explicit about. `IDENTICAL` is
now a **completeness proof, not a guard**: "a headless boot re-authored a committed
scene to the same bytes" is precisely the assertion that the Null authoring path is
missing nothing, and it is checked on every publish without needing a machine with
a graphics driver. And the fewer-entities row is a **report, not a refusal** —
deleting an entity is a legitimate authoring change, so it is published; what is
not acceptable is doing it by accident, which is why both counts are shouted.

`CompareWithFile` shares `SerializeToDataStream` with `SaveToFile`, so "what a save
would write" and "what a save writes" cannot drift apart. Three units pin one link
each of the chain: `Editor, TreeAuthoringIsBackendNeutral` (the AUTHORING half —
`Zenith_TerrainEditor::EnsureTreeEntities` produces its two NAMED entities on
whichever backend is running), `Editor, SceneSaveDeltaClassifiesPublish` (the
COMPARISON — all four classifications, plus "transient entities never move the
counts"), and `Editor, ScenePublishWritesOnEveryBackend` (the POLICY — a
byte-identical save is a skipped no-op and a differing save publishes, on Null
exactly as on a real backend).

<details><summary>History: the headless publish guard, and why it is gone</summary>

There used to be a **refusal** here: on `Zenith_IsNullRenderer()`, a save that would
CHANGE an existing `.zscen` was rejected outright, and the boot went on to LOAD the
committed scene instead. The reason was real. A Null boot authored an INCOMPLETE
world, because authoring steps that wanted a live GPU resource bailed out *entirely*
rather than skipping only the GPU part — `Zenith_TerrainEditor::EnsureTreeEntities`
returned `false` on its first line, so the instanced-tree entities were never
created. Serializing that subset over a tracked asset silently DELETED content:
every headless RenderTest run rewrote its committed scene down to ~38 KB, dropping
the two `TerrainTrees_*` entities and ~323 KB of instance data, and the only symptom
was a dirty `git status` nobody was reading.

The refusal was correct for the world as it was, and its cost was that
**re-authoring a scene required a windowed tools boot** — a graphics driver in front
of every scene edit, and the reason a batch of tickets carried a "needs a GPU"
marker. ZEN-6 fixed the cause rather than the symptom: the bail conflated "create
scene data" with "allocate GPU buffers" and skipped the wrong one. With authoring
backend-neutral there is nothing left for a refusal to protect, so it went, and the
unit that pinned it (`Editor, HeadlessSaveNeverRewritesSceneAsset`) went with it.

</details>

**Corollary for games:** a per-run harness entity (a smoke runner, a capture rig)
must be spawned **transient, post-load**, never authored before `AddStep_SaveScene`
— otherwise every run of that mode writes an entity into the tracked asset that no
other run has. RenderTest's `RenderTestSmokeRunner` is the worked example. This
matters on **every** backend now: the refusal used to shield headless runs from the
mistake as a side effect, and a headless run publishes like any other.

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

**★ AUTHORED ROTATIONS THAT LAND IN A COMMITTED SCENE.** All three rotation steps are
now byte-stable across build configurations, but they are not equally strong:

| Step | Authoring-time math | Safe for a COMMITTED `.zscen`? |
|---|---|---|
| `AddStep_SetTransformYaw(rad)` | `Zenith_Maths::AuthoringRotationY` | yes — pinned FP model |
| `AddStep_SetTransformRotationEuler(x,y,z)` | `BuildEulerRotation` (all-`Authoring*`) | yes — pinned FP model |
| `AddStep_SetTransformRotationQuat(x,y,z,w)` | none -- verbatim to `SetRotation` | **YES, unconditionally** |

The first two used to call glm/libm directly, and MSVC Debug and Release codegen do
not agree on those to the last bit under the project's `/fp:fast`. An entity authored
through them serialized **different bytes from a Debug and a Release tools build**, so
a tracked scene file ping-ponged between two values in `git status` forever — and
because the drift is 1-2 ULP, every tolerance-based guard stayed green while it
happened. That is not hypothetical: it is the defect
`Games/Zenithmon/Docs/DecisionLog.md` ZM-D-183 fixed for `Npc_RivalVesper` (which hid
behind a *bit-exact* pre-save guard comparing the serialized bytes against a
re-computation of the same expression **in the same binary** — both sides moved
together), and it is what made RenderTest's scene differ by 19266 bytes between
configs.

Their math now runs through the `Zenith_Maths::Authoring*` helpers, which are single
non-inline definitions compiled under `ZENITH_AUTHORING_DETERMINISM_BEGIN` (see
`Core/Zenith.h`). Verified by authoring RenderTest's scene from
`Vulkan_vs2022_Debug_Win64_True` and `Vulkan_vs2022_Release_Win64_True` and comparing
MD5s. **Pinning a CALLER is not sufficient on its own** — glm's operators are header
inlines that take their FP model from their own definition point and are shared as
COMDATs with every `/fp:fast` TU, so authoring math must not call glm at all.

The quat step stays the strongest option and the right one for a handful of values:
it performs no arithmetic whatsoever, so it is immune even to a toolchain upgrade
moving libm underneath the other two.

The quat step's arguments are in **serialized order (x, y, z, w)** — deliberately not
`glm::quat`'s `(w, x, y, z)` constructor order — so a caller freezing bytes read out
of a `.zscen` types them in the order they appear in the file. `SetRotation` stores
the value verbatim (no normalization), which is what makes a chosen bit pattern
survive to disk. Yaw/euler remain correct for transient or gitignored scenes, where a
1-ULP difference has nowhere to show up. (Identity rotations are exact in every
config, so entities that never rotate are unaffected.)

Material assets are authored the same boot-time way via the `AddStep_Material*`
verbs (`AddStep_MaterialCreate`, `AddStep_MaterialOpen`,
`AddStep_MaterialSetParam{Float,Color,Int}`, `AddStep_MaterialSetTexture`,
`AddStep_MaterialSet{Parent,Override,PreviewMesh,PreviewLight}`,
`AddStep_MaterialSave`), routed to `ExecuteMaterialAction` (see below).

Grass type tables are authored the same boot-time way via the `AddStep_GrassTypes*`
verbs (`AddStep_GrassTypesCreate`, `AddStep_GrassTypesSetCount`,
`AddStep_GrassTypesSetName`, `AddStep_GrassTypesSetParam{Float,Color}`,
`AddStep_GrassTypesSave`), routed to `ExecuteGrassTypeAction` (see below). They
edit `Zenith_TerrainEditor`'s WORKING copy of the `Flux_GrassTypeTable` — the same
object the terrain editor panel's "Grass Types" section edits, so an authored
recipe and a human produce the same table. Parameters are addressed **by name**
(`"HeightMax"`, `"Density"`, `"WindResponse"`, ...; colours `"BaseColour"` /
`"TipColour"`) through the one name→field mapping in `Flux_GrassTypeTable.cpp`,
mirroring how the material verbs address the material param table; an unknown name
asserts at boot via `GrassTypeActionChecked`. `GrassTypesSave` writes
`game:Vegetation/GrassTypes.zdata` through `Zenith_GrassTypeTableAsset` and then
applies, so a file that reached disk but never took effect cannot go unnoticed.

### The split dispatcher: twelve contiguous ranges

`ExecuteAction` is a **router, not a switch**. Before its (now small) main switch
it forwards **twelve CONTIGUOUS enum ranges** to twelve sub-executors, which is
what keeps the dispatcher inside the complexity gate:

| Range | Sub-executor |
|---|---|
| `TERRAIN_EDITOR_SET_ASSET_SET` .. `TERRAIN_EDITOR_EXPORT_CHUNKS_RECT` | `ExecuteTerrainEditorAction` (via `TryRouteTerrainEditorAction`) |
| `CREATE_UI_TEXT` .. `SET_UI_VIRTUAL_BUTTON_HIT_SLOP` | `ExecuteUIAction` |
| `MATERIAL_CREATE` .. `MATERIAL_SAVE` | `ExecuteMaterialAction` |
| `GRASS_TYPES_CREATE` .. `GRASS_TYPES_SAVE` | `ExecuteGrassTypeAction` |
| `SET_CAMERA_POSITION` .. `SET_MAIN_CAMERA` | `ExecuteCameraAction` |
| `SET_TRANSFORM_POSITION` .. `SET_TRANSFORM_ROTATION_QUAT` | `ExecuteTransformAction` |
| `SET_LIGHT_INTENSITY` .. `SET_SUN_TIME_OF_DAY` | `ExecuteLightAction` |
| `GRAPH_OPEN_FRESH` .. `GRAPH_BUILD` | `ExecuteGraphAuthoringAction` |
| `SET_PARTICLE_CONFIG` .. `SET_PARTICLE_EMITTING` | `ExecuteParticleAction` |
| `ADD_COLLIDER_SHAPE` .. `SET_MODEL_MATERIAL` | `ExecuteColliderModelAction` |
| `SET_TERRAIN_MATERIAL` .. `SET_TERRAIN_SPLATMAP_PATH` | `ExecuteTerrainMaterialAction` |
| `CREATE_PREFAB_FROM_SELECTED` .. `INSTANTIATE_PREFAB` | `ExecutePrefabAction` |

**Ranges are COMPARED, never numbered.** Each row is a pair of `>=` / `<=` tests
against its block's first and last member, so:

- adding an action type at the **end of a block** is free;
- adding one **between two members of another block** silently routes it to that
  block's executor, where it hits the `default: Zenith_Assert` at boot;
- reordering members **inside** a block is invisible to the router but breaks the
  payload contract every step's `AddStep_*` packs into.

So every block carries a "must stay CONTIGUOUS" comment naming its first and last
member. The youngest block (`GRASS_TYPES`) is additionally pinned twice — a
`static_assert` on its width in `Zenith_EditorAutomation.h` and the
`Automation, GrassTypesEnumBlockIsContiguous` unit test on each member's position
plus both neighbouring boundaries.

## Selection System

### Selection Model

Multi-entity selection using EntityID:
- `m_xEditorState.m_xSelection.m_xSelectedEntityIDs` (unordered_set) stores the set of selected entities; `m_uPrimarySelectedEntityID` tracks the primary selection for gizmo operations and UI display
- ID-based (not pointer) for safety across scene reloads
- Selection cleared when entity deleted or scene loaded
- API: `SelectEntity` / `ToggleEntitySelection` / `GetSelectedEntityIDs` / `HasMultiSelection`

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
- `ScreenToWorldRay()` - Converts 2D viewport coords to 3D world ray (screen → viewport → clip → world). Signature: `Vector3 ScreenToWorldRay(const Vector2& mousePos, const Vector2& viewportPos, const Vector2& viewportSize, const Matrix4& viewMatrix, const Matrix4& projMatrix)` — `mousePos`/`viewportPos`/`viewportSize` give the viewport-relative position used for NDC conversion; returns the normalized world-space ray direction (origin is the camera position)

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
- W / E / R - Translate / Rotate / Scale mode; X - local / world space
- Only active when viewport focused and not in Playing mode

**Hover, space and snapping:** `Flux_GizmosImpl::UpdateHover` raycasts the
handles every frame the viewport is hovered so the handle under the cursor
draws brighter. `SetLocalSpace` rotates the handles (and the drag axes, frozen
at the rotation the drag began with) with the entity. `SetSnapSettings`
rounds translation deltas, rotation angles and resulting scales to the
increments in the prefs; `SnapValue` is pure. The interaction-bound wireframe
cubes that used to draw in every Debug build are behind
`m_bDrawInteractionBounds` (Window > Viewport > Gizmo Hit Bounds).

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
- When a drag ends, `Zenith_Editor::RecordGizmoDragUndo` compares the gizmo's
  captured initial TRS with the live one and **records** (without re-executing)
  a `Zenith_UndoCommand_TransformEdit`; a click without movement records nothing
- Stores before/after position, rotation, scale
- EntityID-based (safe across scene changes)
- Every drag also marks the owning scene dirty

## Undo/Redo System

Command pattern implementation with history stack management.

### Command Interface

Base class `Zenith_UndoCommand` requires:
- `Execute()` - Perform the action
- `Undo()` - Reverse the action
- `GetDescription()` - Human-readable text for UI tooltip

### Command Types

**TransformEdit:**
- Stores: EntityID, before/after position, rotation, scale
- Execute: Apply "after" transform
- Undo: Restore "before" transform
- Created automatically by gizmo interactions

**EntityLifetime / EntityState / ComponentBytes / Composite** (`Zenith_EditorCommands.h`):
- `Zenith_EditorEntitySnapshot` serialises an entity subtree (name, enabled,
  transient, parent record index, component bytes via the meta registry) and
  `Restore` rebuilds it with FRESH EntityIDs — slots are generation-counted,
  so the old IDs never come back; the hierarchy is relinked from record
  indices, the main-camera role is restored, and the result is selected
- `EntityLifetime` is delete (Execute destroys, Undo rebuilds; the snapshot is
  re-captured right before every destroy) or create / duplicate (the inverse)
- `EntityState` writes name + enabled + parent; `ComponentBytes` puts one
  component into a payload (empty = absent: everything but the Transform is
  removed and rebuilt from the bytes, the Transform is read in place)
- `Composite` runs children in order and undoes them in reverse, owning them;
  multi-selection delete / duplicate are one step
- `Zenith_UndoSystem::Record` pushes an ALREADY-APPLIED command without running
  Execute (gizmo drags, inspector edits, live creates); `Execute` still runs the
  command first (deletes, removes, renames)

**TerrainEdit** (`Zenith_UndoCommand_TerrainEdit`, `TerrainEditor/Zenith_TerrainEditorUndo.h`):
- Stores: the bounding texel rect of everything one brush stroke (or auto-splat run) touched on ONE map, with before/after byte copies
- Execute/Undo: rewrite the after-/before-region via `Zenith_TerrainEditor::WriteMapRegion`, re-marking dirty chunks / GPU flags so the visuals follow
- Created by terrain brush strokes and auto-splat operations; byte footprint reported to the editor's live-undo budget


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

Bound in `Zenith_Editor::UpdateEditorInput`, suppressed whenever ImGui has a
text field focused (`io.WantTextInput`). It reads the modifiers once and hands
the input system to three scoped handlers — `HandleGlobalShortcuts` (file,
undo, play, F1: any panel), `HandleEntityShortcuts` (viewport or hierarchy
focused, edit mode only) and `HandleViewportShortcuts` (viewport focused, no
modifier, not mid-look) — so a Delete pressed in the console never removes an
entity. F1 opens the Keyboard Shortcuts window, which lists them all.

| Keys | Action |
|---|---|
| Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) | Undo / Redo (the Edit menu reads "Undo Move Entity") |
| Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S | New / Open / Save / Save As (Save goes to the scene's own path; unsaved changes prompt first) |
| Ctrl+P / Ctrl+Shift+P | Play-or-Stop / Pause-or-Resume |
| Ctrl+Shift+N | Create empty entity |
| Delete / Ctrl+D / Ctrl+A / Esc / F | Delete / Duplicate / Select all / Deselect / Focus (viewport or hierarchy focused) |
| W / E / R / X | Gizmo mode / local space (viewport focused) |
| F2, double-click | Rename in the hierarchy |

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

Unity / Unreal conventions, one gesture at a time (`UpdateEditorCameraGestures`
decides which owns the mouse; a gesture that started over the viewport keeps
ownership until its button is released):

| Gesture | Effect |
|---|---|
| RMB hold | Mouse look; WASD fly along the LOOK direction, Q/E down/up, Shift 3x; the wheel scales the fly speed (persisted in the prefs) |
| Alt + LMB drag | Orbit the pivot — the selection's bounds centre, or the last focus point |
| MMB drag | Pan the view plane, scaled so the point under the cursor stays under it |
| Wheel | Dolly towards the pivot (proportional to the pivot distance) |
| F | Fly smoothly (0.28 s smoothstep) to frame the selection's bounds |

**★★ THE EDITOR MUST NOT CAPTURE THE CURSOR TO LOOK.** Every camera gesture uses
the ordinary OS-processed pointer delta in SCREEN PIXELS, the same thing the rest
of the editor sees. Capturing (`GLFW_CURSOR_DISABLED`) drags
`GLFW_RAW_MOUSE_MOTION` along with it, and that silently changes the UNIT
`GetMouseDelta` reports: out go pixels — bounded by the desktop, damped by the
pointer curve — and in come unbounded, unaccelerated device counts, roughly
1/1600 inch each. The delight pass added the capture and it was a regression, not
an improvement: the camera spun far too fast to steer, and a sweep meant to be
horizontal drifted in pitch until it was staring at the sky, because nothing
bounds a raw sweep and nothing damps the vertical component of a hand's arc. The
capture and its raw-motion toggle were removed; `m_fLookSensitivity` is back to
0.1 **deg/pixel**, the value the editor shipped with, and remains tunable in
**Edit > Camera** because a preference is useful, not because the default was
wrong. The cost of not capturing is the one the editor always had: a long sweep
stops at the edge of the screen.

**A single frame can never spin the camera.** The per-frame delta is clamped
(`fMAX_LOOK_PIXELS_PER_FRAME`) before it becomes an angle. Real pointer travel
tops out at a few hundred pixels a frame, so the clamp only ever catches a
NON-movement delta — a cursor warp, a cursor-mode switch, or a hitch that batched
many packets into one frame. The movement path clamps dt for exactly the same
reason.

**Pitch is written in exactly ONE place** (`ApplyMouseLookDelta`), from one call
site per frame, so pitch that moves during purely horizontal mouse movement is a
non-zero delta Y arriving from the input layer — the camera code cannot
manufacture axis cross-talk. Check the overlay's mouse numbers before looking for
it in the maths.

**The viewport stats overlay carries the camera's live truth**: the active
gesture (LOOK / ORBIT / PAN / idle), yaw and pitch in degrees, and the raw
per-frame mouse delta before clamping. Without those numbers "the camera goes the
wrong way" cannot be told apart from "the pitch is pegged at its limit" — they
look identical from the chair, and the person at the chair could not tell them
apart when asked. Toggle it with Window > Viewport > Show Stats.

Flying carries the pivot along; selecting an entity moves the pivot onto it
(`RefreshCameraPivotFromSelection`). The frame dt is the real one (clamped to
100 ms so a hitch cannot fling the camera). While a gesture owns the mouse the
gizmo and picking are skipped for the frame.

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

`m_xEditorState.m_xViewport.m_bHovered` flag determines input eligibility:
- Set to true when mouse inside viewport panel bounds
- Gizmo interaction only processes when true
- Object picking only when hovered
- Camera controls require viewport hover OR right-click drag (viewport-dependent)

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

## Look and feel

- **Font:** Roboto Medium, embedded as a compressed byte array
  (`Zenith_EditorFontData.generated.h`, regenerated with ImGui's
  `binary_to_compressed_c`) so no clone ever falls back to the 13 px bitmap
  font. Base size 15 px, scaled by the monitor's content scale
  (`Zenith_Window::GetContentScale`, `style.FontScaleDpi`); every hard-coded
  pixel size goes through `Zenith_EditorUI::Px`
- **Theme:** `Zenith_EditorUI::ApplyTheme` — authored in sRGB, converted to
  linear because the swapchain is sRGB; `Palette()` holds the packed colours
  the panels draw with; `PushPlayModeTint` warms the chrome while playing
- **Icons:** `Zenith_EditorUI::DrawIcon` draws every icon as vectors into a
  draw list — crisp at any DPI, no icon font, no texture asset
- **Window:** the title reads `Scene* - Game - Zenith Editor [Playing]`; an
  interactive tools run opens maximised (automated runs and `--screenshot`
  captures keep the requested size so frames stay reproducible)
- **Prefs:** `%LOCALAPPDATA%/Zenith/<Game>/editor_prefs.txt` beside `imgui.ini`;
  `Zenith_EditorPrefs::GetUserDataDirectory` is the one resolver for that folder

**★ Draw-list decorations, not items.** Anything drawn ON a row or over the
viewport (the enabled eye, the scene count badge, the mode badge, the axis
widget) is drawn straight into the draw list and hit-tested by hand. Placing an
ImGui item there with `SetCursorScreenPos` and then restoring the cursor trips
`ErrorCheckUsingSetCursorPosToExtendParentBoundaries` (this build defines
`IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, which turns that misuse into an assert): a
restored cursor sits one ItemSpacing past the window's max extent, and if no
item follows before `End`, the assert fires — in a windowed run that is a
modal CRT dialog nothing logs, so the process just hangs.

## Known Limitations

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
