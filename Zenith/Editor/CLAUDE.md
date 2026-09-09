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
- `Zenith_AnimationDocument.h/cpp` - The editable WORKING COPY of one `.zanim`, and its only writer: a deep copy of the asset's clip, a STABLE per-key/per-event id (an index is not an identity — retiming reorders the track), every mutation as one undoable verb, a content-hash check for external modification, and D21's refusal to edit a GENERATED clip in place plus the `PromoteToAuthoredOverride` escape hatch. Tests in `Zenith_AnimationDocument.Tests.inl`
- `Zenith_AnimControllerDocument.h/cpp` - The editable WORKING COPY of one `.zanimctrl`, and its only writer: a deep copy of the asset's `Flux_AnimatorControllerDef`, a machine SELECTOR (the top-level machine or a stable LAYER ID — never a layer index), every mutation as one undoable verb, a content-hash check for external modification, WU-7.2's LAYER verbs (add / remove / rename / weight / blend mode / emit events / mask path / **reorder**, all by stable id), and the things the def's own API does not do (a removed state's INBOUND transitions, a renamed state's referrers, and a reorder at all — `Flux_AnimatorControllerDef` has no move verb). Tests in `Zenith_AnimControllerDocument.Tests.inl`
- `Zenith_EditorAnimCtrlCommands.h/cpp` - That document's OWN undo stack (not the shared editor one, and not the clip document's): state add / remove / rename, default state, state clip, node position, a whole-transition-LIST snapshot, a whole-parameter-TABLE snapshot, the clip-path list, WU-7.2's two LAYER commands (a whole-list snapshot for add/remove/reorder, and one layer's five scalar fields by stable ID — see "The Layers strip" below for why it is two and not one), and the compound. Tests in `Zenith_EditorAnimCtrlCommands.Tests.inl`
- `Zenith_EditorAnimCommands.h/cpp` - The document's OWN undo stack (not the shared editor one — a scene load clears that, and an animation edit has nothing to do with a scene): key insert / remove / retime / value, duration, and the three event commands. Tests in `Zenith_EditorAnimCommands.Tests.inl`
- `Zenith_AnimationPreviewSession.h/cpp` - One panel's live preview of one clip: its OWN controller, skeleton instance and clock (D30 — never an entity's, which would double-tick it), a deep copy of the clip, per-clip rig resolution + remembered override (D31), and ITS OWN render view — `UpdatePreviewView()` raises and stages `kuFluxViewSlotPreviewAnim`, `DeactivatePreviewView()` lowers it, and both are idempotent because the registry reports whether the active set actually changed. Nothing else stages or lowers that slot, so the session no longer claims `Flux_PreviewSlotArbiter` and cannot be dispossessed (D32 is spent; the arbiter is kept, and arbitrates the MATERIAL slot only). ★ Only `Close()` / the rig teardown and the panel's two hidden-frame calls lower the view — the material preview's per-frame janitor owns slot 5 and only slot 5, so a missed call leaves a full per-view pass chain rendering a preview nobody is looking at. **It also SUBMITS THE MESH that view draws (D5):** a resolved rig builds a `Flux_ModelInstance` (a `.zmodel`) or a `Flux_MeshInstance` (a bare `.zasset`/`.zmesh`), owned here and destroyed by the rig teardown, and registers the session as a renderer external-item PULL source (`Flux_RendererImpl::RegisterExternalSceneItemSource`, keyed on `this`) — polled once per GPU-scene sync for one `Flux_ExternalSceneItem` per submesh: the session's model matrix, that submesh's mesh instance, the aligned material, ★ **the SESSION'S OWN `Flux_SkeletonInstance`** (a `Flux_ModelInstance` builds a second one that nothing ever animates — submitting THAT would draw a T-pose while the clip played underneath), a distinct `m_uSubmeshSlot`, and `Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim)` alone. PULL rather than PUSH because a pushed item would sit in the renderer's list holding a raw mesh pointer until some later frame; a LOWERED slot 6 submits nothing, so a hidden dope sheet costs no arena slice or skin job. There is no `Zenith_IsNullRenderer()` branch anywhere on that path — the instances exist headless and the units read them through `GatherExternalSceneItemsForTesting`. Tests in `Zenith_AnimationPreviewSession.Tests.inl`
- `Zenith_BoneMaskDocument.h/cpp` - The editable WORKING COPY of one `.zanimmask`, and its only writer: a deep copy of the asset's `{bone name, weight}` entries plus D47's explicit flag, its OWN undo stack (three small command classes live in the same file), a content-hash check for external modification, and the one place the additive-layer rule is written down (`LayerAcceptsMask`). Tests in `Zenith_BoneMaskDocument.Tests.inl`
- `Zenith_AnimTimelineMath.h/cpp` - The PURE seconds<->pixels mapping the dope sheet, its ruler and its events row all share: zoom clamps, the inverse, delta conversions, visibility, frame snapping, zoom-about-a-pixel, view clamping, fit-to-window and the ruler tick ladder. Not one line of UI, so every timeline defect is catchable headless. Tests in `Zenith_AnimTimelineMath.Tests.inl`
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
- `Panels/` - Panel implementations (AnimStateMachine, Animation, Console, ContentBrowser, GraphEditor, Hierarchy, MaterialEditor, Memory, Properties, RenderGraph, StatusBar, TerrainEditor, Toolbar, VariantEditor, Viewport). Toolbar and StatusBar are strips drawn inside the dockspace host window, not dockable windows
- `Panels/Zenith_EditorPanel_Animation.h/cpp` (+ `_Render.cpp`, `_Ops.cpp`, `_Pose.cpp`, `_IK.cpp`, `_Curve.cpp`) - The animation DOPE SHEET over one `Zenith_AnimationDocument` and one `Zenith_AnimationPreviewSession`. A CLASS, not a pile of file statics (see "Animation Dope Sheet Panel" below); the `_Render` TU holds the drawing half and `_Curve` holds WU-8.2's curve view (its pure value↔pixel mapping, its drawing and its input translation — see "The Curve view" below). Tests in `Zenith_EditorPanel_Animation.Tests.inl`
- `Panels/Zenith_EditorPanel_AnimStateMachine.h/cpp` (+ `_Ops.cpp`, `_Render.cpp`) - The animator-controller STATE-MACHINE GRAPH over one `Zenith_AnimControllerDocument` (see "Animator State Machine Panel" below). A CLASS with undo, like the dope sheet and unlike the graph editor. Tests in `Zenith_EditorPanel_AnimStateMachine.Tests.inl`
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
- Animation Editor (the dope sheet) and Animator State Machine — both flags live
  on their panel objects, not in `Zenith_EditorPanelVisibility`, because each
  panel owns every other piece of its own state and a flag kept somewhere else is
  the seam a second instance would have to unpick first

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
  materials (Material Editor), behaviour graphs (Graph Editor), `.zanimctrl`
  animator controllers (Animator State Machine), and — both in the Animation
  Editor, which is SHOWN first and opened second, exactly as the
  `ANIM_OPEN_CLIP` automation step does it — `.zanim` clips (`OpenClip`) and
  `.zanimmask` bone masks (`Action_MaskOpen`, which raises the mask *section*
  but not the window, so the `ShowFlag` is not redundant on that branch)
- **Type filter:** the combo's ORDER *is* `MatchesAssetTypeFilter`'s switch
  index, so rows are appended, never inserted — `Animator Controllers` (8) and
  `Bone Masks` (9) are the last two
- **Drag-Drop:** Supports Texture, Mesh, Material, Prefab, Animation, Animator
  Controller, Bone Mask, Graph and generic file payloads (the drag preview shows
  the type icon)
- **★ A dedicated payload id exists so a target can REFUSE.** `.zanimctrl` and
  `.zanimmask` rode `DRAGDROP_PAYLOAD_FILE_GENERIC` while nothing accepted them;
  they now emit `DRAGDROP_PAYLOAD_ANIMCTRL` / `DRAGDROP_PAYLOAD_ANIMMASK`. The
  drag SOURCE and the drop TARGET are two files agreeing on one string, and a
  mismatch is a drag that silently does nothing — so repointing a row means
  repointing **every** target that accepted the old id in the same commit
  (`KnownFileTypesCarryDedicatedAnimationPayloadIds` is the unit that says so)
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

### Animation Dope Sheet Panel (`Panels/Zenith_EditorPanel_Animation`)

The keyframe editor for `.zanim` clips. One window over ONE
`Zenith_AnimationDocument` (the working copy, and the only writer of the file)
and ONE `Zenith_AnimationPreviewSession` (its own controller, skeleton instance
and clock):

- **Rows** — a collapsible header per bone, in the same total order
  `GetBoneNamesSorted` and the file itself use (D5), with **three** sub-rows
  (Translation / Rotation / Scale) underneath. All three are always present:
  a channel is DELETED when its last key goes (D14), and a row that vanished
  with it would leave nowhere to put a key back. Then a **Root Motion** group
  with exactly **two** rows — there is no scale delta (D16) — and an **Events**
  row last.
- **Ruler + playhead** — subdivisions from `Zenith_AnimTimelineChooseTicks`
  (frames first, then a seconds ladder); the playhead tracks the preview
  session's clock and carries a grab handle in the ruler.
- **Toolbar** — asset path entry (drop a `.zanim` on it), Open / Close, a
  **Masks** checkbox (WU-7.1's section, off by default), play-pause, a seconds
  and a frame readout, Zoom To Fit, the live px/s, and the two read-only state
  badges: `UNSAVED` and `CHANGED ON DISK`.
- **Preview pane** — the animation preview's OWN LDR image
  (`GetPreviewLDR(kuFluxViewSlotPreviewAnim)`, registered with ImGui once), or
  the **rig prompt** when `NeedsRigSelection()`. There is no dispossessed
  placeholder and no Reclaim button: the panel has a view slot of its own, so
  there is no owner to name and nothing to take back. What the image shows is the
  session's preview mesh, submitted per frame as a skinned external scene item
  masked to that slot (see `Zenith_AnimationPreviewSession` above) — so a pane
  that renders empty is a rig/submission question, not a view-activation one.
- **Keys past the duration (D13)** — shrinking a duration does not move a key,
  so a clip can legally hold keys nothing will ever sample. Each one gets a
  warning glyph, the region past the end is shaded, and a banner counts them.
  `GetKeysPastDurationCount()` / `IsKeyPastDuration()` are what a unit reads.
- **Promotion (D21)** — a GENERATED clip is refused, and the refusal is its own
  enumerator, so the panel offers "Promote to authored override" rather than a
  bare failure.

**Operations live in `_Ops.cpp` and every one has a bool-returning `Action_*` twin**
(`Action_SelectKey/SelectEvent/BoxSelect/ClearSelection/MoveSelection/DeleteSelection/`
`DuplicateSelection/CopySelection/PasteToBone/RippleRetime/Scrub/SetDuration/Undo/Redo`).
The mouse and keyboard handlers in `_Render.cpp` only translate input into those; the
actions never read ImGui state, which is what lets automation drive them without
synthesising input. Every mutation goes through `Zenith_AnimationDocument`'s verbs (stable
IDs, never indices); a multi-key operation is ONE undo step via the document's
`BeginCompound()`/`EndCompound()` bracket and `Zenith_AnimCommand_Compound`, and any
operation whose target time collides with an existing key (D11) is refused whole, flashes
the blocking key, and changes nothing.

**★ IT IS A CLASS, AND THAT IS THE ONE THING IT DOES NOT COPY FROM THE GRAPH
EDITOR.** That panel keeps its whole state in a single file-scope aggregate,
which is exactly why it can hold ONE asset open and has no undo at all: neither
is a decision anybody made, they are both consequences of the storage.
Everything here — document, session, view, row list, every rect map, the show
flag — is a member of `Zenith_EditorPanel_Animation`, so a second dope sheet is
a second object rather than a rewrite. `Instance()` is the editor's single one,
and a unit builds its own on the stack.

**★ EVERY POSITION ACCESSOR RETURNS FALSE FOR SOMETHING OFF SCREEN**, which is
the half of the graph editor's hard-won contract that IS worth copying (see
that panel's section below for what it cost to learn). `GetKeyRect`,
`GetRowRect` / `GetRowTrackRect`, `GetEventRect`, `GetEventsRowRect`,
`GetPlayheadRect`, `GetRulerRect` and `GetTrackAreaRect` all record only what
was painted inside the canvas this frame and hand out nothing whose centre is
outside the display. `ScrollTimeIntoView` and `ScrollRowIntoView` are the
required precursors and, like `ScrollPaletteEntryIntoView`, are applied by the
NEXT `Render` — give them a frame. `RequestWindowPlacement` pins the geometry
so a saved `imgui.ini` cannot move a measurement.

**★ "The display" is the bound CAPTURED WHEN THE RECT WAS RECORDED**, not
`ImGui::GetIO().DisplaySize` re-read at query time. A rect is a fact about a
frame and has to be judged against that frame's display. Reading it live works
in the editor (queries happen mid-frame) and is wrong everywhere else: ImGui
initialises `DisplaySize` to **(-1, -1)** and only a backend `NewFrame` fills it
in, so any query made outside a frame — which is every unit assertion, since a
test's frame must be closed before its result can be inspected — compared each
centre against -1 and refused it. Five units reported "nothing was published"
while the panel was drawing perfectly; the expected x they printed was computed
from the panel's own live view, which proved the layout right and the gate
wrong. `WasSheetDrawnLastFrame` / `GetLastTrackWidth` /
`GetRecordedDisplayWidth` exist to tell that failure apart from the three other
things a flat `false` can mean, and the units assert them *before* the rects.

**★ The sheet is ONE `InvisibleButton` and a draw list.** Row backgrounds,
labels, ticks, keys, events and the playhead are all painted at absolute screen
coordinates — see "draw-list decorations, not items" below for why placing
items there hangs a windowed build with a modal CRT dialog nothing logs.

**★★ NOTHING SHOWN DRAWS NOTHING — NOT A DISABLED STRIP, NOT A COLLAPSED
HEADER. `RenderSheet` is sized from `ImGui::GetContentRegionAvail()`, so every
item emitted above it comes straight out of the sheet's height — and the EVENTS
ROW IS THE SHEET'S LAST ROW.** Each optional block above the sheet
(`RenderEventToolbar`, `RenderEventInspector`, `RenderPoseToolbar`,
`RenderMaskSection`) therefore returns **before submitting a single item** when
it has nothing to show, and each one's toggle lives on a toolbar row that
already exists rather than on a line of its own.

This is not tidiness. WU-7.1 shipped the Bone Masks section as an always-present
*collapsed* `CollapsingHeader` — one row, ~24 px — and that alone pushed the
events row below the canvas bottom in the 900×600 unit window:
`AnimPanel::ChangingTheDurationMovesTheEventRowAndNotTheStoredValue` went red on
all three `Null_` exes with `GetEventRect` false **before and after** the edit,
which reads as "the events row is broken" and is nowhere near its cause. The
off-screen gate is what makes a height regression arrive as a flat `false` a long
way from the pixels that caused it, so the height has to be defended at the
source. `WasMaskSectionDrawnLastFrame()` and its siblings exist so a unit can
assert the *absence* directly instead of inferring it.

**★ AND THE UNIT THAT GUARDS IT MEASURES `GetTrackAreaRect().Height()`, NOT
"is row X still visible".** That rect spans the whole canvas, so it *is* "the
height the sheet was given" and is indifferent to how many rows fit inside it.
Asserting on a row instead couples the guard to the FIXTURE: a clip whose rig
RESOLVES puts `RenderPreviewPane` on its live-image branch — a 192 px square
(`fSHEET_PREVIEW_SIZE_1X`, occupied whether or not a texture exists) plus WU-4.3's
pose-toolbar line — while a rig-less clip takes the rig-PROMPT branch (a wrapped
line, two `InputText`s and a button). That is ~100 px of difference between two
probes in the same window with the same row count, and it is enough to put the
events row off the bottom on its own. WU-7.1's first guard asserted
`GetEventsRowRect` on a *rigged* probe and went red for exactly that reason,
having nothing to do with the section it was guarding. Pair the height equality
with a sensitivity check (the height must genuinely SHRINK while the block is
shown), or the equality would hold just as well against a constant.

(The rule was written in `RenderEventInspector`'s body from WU-5B onwards and was
followed by WU-4.3's pose toolbar; it was not written down here until WU-7.1
rediscovered it the expensive way — twice.)

**★ No coordinate maths lives in the panel.** Every seconds↔pixels conversion
goes through `Zenith_AnimTimelineMath`; a key's centre x *is*
`Zenith_AnimTimelineTimeToPixel(View(), t)`, and a unit asserts exactly that, so
the panel cannot grow a second copy of the mapping that drifts from the first.

**The window title is the bare constant** `szEDITOR_WINDOW_ANIMATION_EDITOR`.
`DockBuilderDockWindow` matches by name and `ImHashStr` hashes the whole string,
so a title decorated with a dirty marker or the clip name would dock nothing and
the window would silently float. Both are shown in the toolbar instead.

### The Bone Mask sub-panel (WU-7.1)

A **"Bone Masks"** section inside the Animation Editor, over its own
`Zenith_BoneMaskDocument` — a second, independent working copy with its **own
undo stack**. A mask edit and a keyframe edit are not one editing session: Ctrl+Z
in the sheet must not take back a slider drag in the mask list, and the two
documents save different files.

**★ IT IS OFF BY DEFAULT AND DRAWS NOTHING WHILE IT IS OFF** — zero items, zero
height, per the rule above, which this section is the reason for. The toggle is a
**"Masks" checkbox on the toolbar's existing first row** (beside Open / Close, and
above that row's no-clip early return, so a mask can be opened with no clip
loaded). `Action_MaskOpen` / `Action_MaskOpenFresh` raise the flag and
`Action_MaskClose` clears it, so the checkbox and what is on screen cannot
disagree; `RenderMaskSection` additionally draws whenever a mask document is open,
as a safety net so an open — possibly dirty — document can never be invisible.

**★ IT LIVES INSIDE THE DOPE SHEET BECAUSE THE RIG DOES.** A `.zanimmask` stores
weights **by bone NAME** (D46/D47) and only becomes indices when it meets a
specific `Zenith_SkeletonAsset`. The only place the editor already holds a
resolved, previewed rig is this panel's `Zenith_AnimationPreviewSession`, with
its metadata resolution, its remembered per-clip override (D31) and its prompt.
A standalone mask window would have to grow all three. The section lists one
weight slider per bone of **the session's rig, in skeleton order**
(`GetMaskRigBoneNames`), and shows a **prompt** rather than an empty list when
there is no rig — a blank list reads as "this rig has no bones".

**★ IT REFUSES TO OFFER A MASK CONTROL ON AN ADDITIVE LAYER, AND SAYS WHY.**
`Flux_AnimationController`'s layer loop tests `LAYER_BLEND_ADDITIVE` **first** and
goes straight to `Flux_SkeletonPose::AdditiveBlend`, whose signature has no mask
in it; only the OVERRIDE branch reaches `MaskedBlend`. So an additive layer
ignores its mask **entirely**, and a UI that offered the control would let
somebody author a whole mask, save it, assign it and observe nothing — every gate
green, nothing to grep for. `Zenith_BoneMaskDocument::LayerAcceptsMask(blendMode)`
is the ONE place that rule is written (WU-7.2's layer list asks the same
function), `AdditiveLayerMaskNotice()` is the one wording of the refusal, and
`WasMaskAssignmentDrawnLastFrame()` / `GetMaskNotice()` are what a unit reads.

**Actions** — `Action_MaskOpen` / `MaskOpenFresh` / `MaskSetWeight` /
`MaskSetSubtree` / `MaskSetHasAvatar` / `MaskSave` / `MaskClose` / `MaskUndo` /
`MaskRedo`, the same three rules the sheet's actions follow (bool-returning,
reading no ImGui state, every mutation through a document verb). The
`AddStep_AnimMask*` automation family calls exactly these twins.

**★ THEY ARE ASSIGNMENTS, so TRUE means "the value you asked for is in place"**
— the animator-controller panel's rule adopted verbatim, and for its reason: a
recipe that paints a subtree and then re-states one of its bones is completely
ordinary, and under a "false means nothing changed" reading it would trip the
automation's checked wrapper at boot on a step that did exactly what it was
asked. **The invariant to assert on is the undo-stack DEPTH**; a no-op is not an
edit and contributes zero steps. `RemoveBone` is the exception and is a REMOVAL,
so a miss is a genuine refusal.

**★ "Select subtree" takes the RIG AS A PARAMETER and is ONE compound.** A mask
is skeleton-*scoped* but not skeleton-*bound* — the same file is meant to be
opened against whichever rig the dope sheet is previewing — so a hierarchy cached
at Open would answer with the previous rig's parents after the preview changed.
The walk is a single forward pass, which rests on the skeleton invariant
`Flux_SkeletonPose::ComputeModelSpaceMatricesFromSkeleton` already asserts
(parents precede children). Painting an arm chain is **one** Ctrl+Z, not one per
bone.

**★ AN ENTRY AT 0.0 AND NO ENTRY ARE THE SAME NUMBER AND DIFFERENT FILES.**
`GetBoneWeight` answers 0 for both, and only one of them writes a row. So the
undo of the FIRST weight ever painted onto a bone **removes** the row rather than
writing a zero — otherwise every experimentally-touched bone stays in the
`.zanimmask` forever as an explicit "masked out" nobody meant. `RemoveBone` is
how a row is actually dropped; `SetBoneWeight(name, 0)` deliberately keeps it,
because an authored zero is a decision (D47).

Weight sliders commit on **edit-complete**, not per frame of the drag — the same
"preview, then commit" shape as the key drag, the duration handle and the event
drag, and for the same reason: a command per frame makes Ctrl+Z crawl back
through positions the user was only passing through.

**Double-clicking a `.zanimmask` in the Content Browser opens it here** (WU-9.2;
it used to only select and log the file). The browser shows the panel and then
calls `Action_MaskOpen`, which raises the mask *section* — a refusal is reported
through `GetMaskNotice()`. The section's path field + **Open** is still the other
route, and a `.zanim` double-click opens the clip the same way.

### The Curve view (WU-8.2)

The per-key **tangent** editor, inside the Animation Editor and living in its own
TU (`Panels/Zenith_EditorPanel_Animation_Curve.cpp`): the selected tracks' x/y/z
component curves, a point per key, a draggable handle per end, and the two
presets. A **"Curves"** checkbox on the toolbar's existing first row (beside
"Masks") turns it on.

**★ IT REPLACES THE SHEET'S ROW AREA; IT IS NOT A SECOND STRIP.** When it is on,
the canvas paints curves in exactly the rectangle the rows would have used —
same `InvisibleButton`, same ruler above it, same playhead, same
`Zenith_AnimTimelineMath` X mapping — and its five controls sit on toolbar rows
that already exist. So the curve view costs the sheet **zero height either way**,
which is this panel's standing "NOTHING SHOWN DRAWS NOTHING" rule taken to its
conclusion rather than merely obeyed: a curve editor drawn as an extra block
would have taken every pixel it occupied out of a canvas whose LAST ROW is the
events row. `TheCurveViewReplacesTheRowsAndCostsTheSheetNoHeight` asserts
`GetTrackAreaRect().Height()` **equal** across the toggle, paired (as the rule
requires) with a sensitivity check that grows the window, and with the rect
population changing over: `GetCurveViewRect` is recorded only while the view is
up, and the dope-sheet row / key / event rects are **not recorded at all** while
it is — those rows were not painted, and handing out a coordinate for one would
be a click into a row nobody can see.

**★ ONE SELECTION, TWO VIEWS.** The curve view shares the panel's key selection
— the same `(track, key id)` set — and switching views only changes which rects
it is hit-tested against. A separate curve selection was the obvious design and
is wrong: an *Auto* applied in one view would act on a set the other was not
showing. Box-select works in both, through `Action_BoxSelect`, which hit-tests
the curve POINT rects instead of the row diamonds while the curve view is up (one
component inside the band is enough — a box round "that key" means the key).

**★ WHAT IS DRAWN IS SAMPLED THROUGH `Flux_BoneChannel`'s OWN SAMPLER**, one
column per pixel, so the curve on screen is the curve that plays. A Hermite
re-derived in the panel would agree with the runtime right up until one of them
changed.

**★ TANGENT MODE IS STORED PER END AND IS ON THE WIRE (B2, schema 3), SO ALL FOUR
MODES ARE AUTHORABLE FROM THIS EDITOR.** The clip stores a four-valued
`Flux_TangentMode` on each end of each key (`Flux/MeshAnimation/CLAUDE.md` →
*Tangent sampling*), `Flux_BoneChannel::Set*Tangent` store what they are GIVEN, and
the document's verbs are the things that decide: `SetKeyTangentMode` is the mode
verb, a handle drag claims the end it moved as `CUSTOM`, `SetKeyTangentsAuto` and
`SetTrackTangentsAuto` write `AUTO`, `SetTrackTangentsLinear` / `SetTrackTangentsFlat`
write `LINEAR` / `FLAT` over identical zero vectors.

**★ AND `AUTO` IS MAINTAINED, WHICH IS WHY IT IS NOT JUST `CUSTOM`.** After
`InsertKey` / `RemoveKey` / `SetKeyTime` / `SetKeyValue` on a track, every `AUTO`
END OF THAT TRACK is recomputed and stored **inside the same undo entry**. Keys on
other tracks are untouched, a `CUSTOM`/`FLAT`/`LINEAR` end is untouched, and no key
id moves. The whole track is swept rather than the moved key's neighbours: a retime
changes the centred span of the key and of both its old AND both its new
neighbours, the id remap has already run, and "which keys were adjacent" would be
four different index calculations with four ways of being subtly wrong.

**★★ THE GROUPING RULE IS `BeginCompound` ONLY IF `!IsCompoundOpen()`.** Nesting is
REFUSED and asserts, and a refresh that then called `EndCompound` anyway would close
the CALLER's group — so a multi-key dope-sheet drag would be terminated halfway
through by the first key that happened to sit beside an Auto tangent. Seven
production sites hold a compound open around these verbs. When one is already open
the refresh's commands are adopted through `PushCommand`, exactly like every other
verb's.

`Zenith_AnimCurveTangentModeOf` is still a **projection** of the four stored modes
onto the two this panel displays — **Linear** when both ends are
`Flux_TangentMode::LINEAR`, **Custom** otherwise — never a second derivation from
the numbers. So a `FLAT` or `AUTO` key currently reads "Custom": an under-statement
rather than a lie, which is the direction this projection was built to fail in.
Widening the display enum, its labels and the `ANIM_CURVE_*` automation verbs
beside them is its own unit; until then the label stays "Linear"/"Custom", because
"Flat" would name the wrong one of the states Custom collapses.

**★ THE VALUE AXIS IS A SECOND, PURE MAPPING — the X axis is untouched.**
`Zenith_AnimCurveValueView` + `Zenith_AnimCurveValueToPixel` / `PixelToValue` /
`Clamp` / `FitRange` are free functions over numbers, in the panel header beside
the pose-ring geometry and for its reason: which way up the axis runs, and what a
handle pixel means as a derivative, are the two things a curve editor gets
silently wrong, and both are then catchable headless with no frame and no clip.
Time↔pixels stays `Zenith_AnimTimelineMath`'s and is not shadowed, which is why
the playhead, the ruler and the duration shade line up with the curves by
construction.

**★ A HANDLE IS A VELOCITY, WHICH IS WHY ITS LEVER IS IN SECONDS.**
`fANIM_CURVE_HANDLE_SECONDS` (0.15 s) rather than a pixel length: a tangent is
units per second, so at a fixed time offset the handle's vertical extent is
literally "how far this slope carries the value in 0.15 s". `HandlePixel` and
`TangentFromPixel` are exact inverses at BOTH ends (the in handle sits at
`-h`, so the two sign flips cancel) and a unit round-trips them — which is what
makes "drag it back to where it was drawn" a no-op rather than a slow drift.

**★ A ROTATION CURVE IS DRAWN AS EULER ANGLES IN RADIANS AND ITS HANDLE IS AN
ANGULAR VELOCITY, and that is an approximation stated out loud.** The clip stores
a rotation tangent as a body-frame angular velocity in axis × rad/s, so radians
on the value axis makes the units consistent and one handle arithmetic serve both
kinds. It is a PRESENTATION: an Euler rate and a body-frame angular-velocity
component agree for a rotation about one axis and diverge as the other two wind
up, and `glm::eulerAngles` has its own branch cuts, so a tumbling rotation draws
with seams. The alternative was three quaternion-derivative curves nobody can
read. `ARotationHandleWritesAnAngularVelocityAndMovesTheSampledPose` is what pins
that the edit reaches the SAMPLER — it measures the pose mid-segment, and
requires both endpoints to stay exactly where they were authored.

**★ ROOT MOTION IS NOT DRAWN AND EVERY TANGENT VERB REFUSES IT.**
`Flux_RootMotion` carries no `Flux_KeyTangents` array (D17) and is still sampled
linearly after WU-8.1, so a handle there would be a control with nothing behind
it — a value authored, saved and displayed that nothing ever reads, with every
gate green. `GetKeyTangents` on a root-motion track is a **refusal**, not an
all-zero answer: "this track's tangents are zero" and "this track cannot hold
one" are different facts. A root-motion key inside a mixed selection is SKIPPED
rather than failing the whole gesture.

**Document verbs** (`Zenith_AnimationDocument`, still the only writer):
`GetKeyTangents` / `SetKeyTangents` / `SetKeyInTangent` / `SetKeyOutTangent` /
`SetKeyTangentMode` / `SetKeyTangentsAuto` (per key) / `SetTrackTangentsAuto` /
`SetTrackTangentsLinear` / `SetTrackTangentsFlat` (one compound each), all by
stable key id, all **ASSIGNMENTS** — true means "the pair you asked for is in
place", and a no-op pushes nothing, so the invariant to assert on is the undo-stack
DEPTH. Undo is one `Zenith_AnimCommand_KeyTangents` per key carrying the exact
PREVIOUS pair, **modes included**; the whole-track presets capture every key's pair
before running `Flux_BoneChannel::ComputeAutoTangents` / `ComputeLinearTangents` /
`ComputeFlatTangents`, so an undo restores the track exactly instead of recomputing
a shape. Restoring the exact **unset** pair — zero vectors AND `LINEAR` — is what
puts a key back on the sampler's bit-identical linear branch.

★ **`SetKeyTangents` STORES WHAT IT IS GIVEN**, so its callers fill the modes:
`Action_SetKeyTangents` and `Action_DragTangentHandleToPixel` write `CUSTOM` on the
end(s) the gesture touched, `Action_SetSelectionTangentsLinear` writes `LINEAR`. A
default-constructed `Flux_KeyTangents` beside two authored numbers is an instruction
to IGNORE them — an edit that reaches the file and never reaches the pose.
`SetKeyIn/OutTangent` fill `CUSTOM` on the end that moved and leave the other end's
mode ALONE, so dragging one handle cannot silently un-`AUTO` the opposite one.
`SetKeyTangentMode` writes the VECTOR its mode implies in the same step: `FLAT` and
`LINEAR` store exact zeroes (both ignore the number, and a stale one would resurface
the moment the end went back to `CUSTOM`), `AUTO` computes and stores the
Catmull-Rom vector immediately, `CUSTOM` keeps the vector already there.

★ The per-key Catmull-Rom is **the clip's, with one home** (B1).
`SetKeyTangentsAuto` calls `Flux_BoneChannel::ComputeAutoTangentForKey` — the same
code `ComputeAutoTangents` runs per key — so the panel's per-key *Auto* and its
whole-track *Auto* cannot disagree about a key's slope. It used to be reproduced
in `Zenith_AnimationDocument.cpp` (centred slope, one-sided at the endpoints, zero
on a non-positive span; for rotation the same in angular-velocity terms, rotated
into key k's own body frame) because the channel offered no per-key helper, and
that copy dragged a second copy of `Flux_AnimationClip.cpp`'s anonymous-namespace
rotation-vector helper along with it. **Both are deleted.** The root-motion
refusal moved up into `SetKeyTangentsAuto` itself, because a bone channel has
never heard of a root-motion track.

**Actions** — `Action_SetCurveView` / `SetTangentsUnified` / `SetKeyTangents` /
`SetSelectionTangentsAuto` / `SetSelectionTangentsLinear` /
`DragTangentHandleToPixel` / `FitCurveViewToSelection`, the same three rules as
every other action here (bool-returning, reading no ImGui state, every mutation
through a document verb). The two toggles are **assignments** rather than
`Action_SetAutoKey`'s "the value CHANGED", so the `ANIM_CURVE_*` automation
family is checked wholesale with no exception list for a later verb to be
forgotten from. `Action_DragTangentHandleToPixel` **commits** — one call is one
undo step — so the pointer handler calls it exactly once, on release, and
previews the intermediate positions as a ghost handle: the same
preview-then-commit shape as the key drag, the duration handle and the event
drag. `HandleCurveInput` returns TRUE when it owns the frame's gesture, which is
what keeps a press on a handle from also reaching `HandleSheetInput` and being
read there as a click on empty space.

The `AddStep_AnimCurve*` family (the `ANIM_CURVE_*` block, the **seventh**
animation range) calls exactly these twins, naming a key by (bone, track, INDEX)
and resolving the stable id at execution time as the `ANIM_*` block does.

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

### Animator State Machine Panel (`Panels/Zenith_EditorPanel_AnimStateMachine`)

The node graph for `.zanimctrl` animator controllers (the runtime is
`Flux/MeshAnimation/` — see its CLAUDE.md → *The animator controller asset* and
*Hot reload*). One window over ONE `Zenith_AnimControllerDocument`:

- **A "Layers" STRIP, which is also the machine picker.** A
  `Flux_AnimatorControllerDef` holds an optional top-level machine AND N layers,
  each owning its own, and every layered game reaches its graph through a layer.
  WU-7.2 replaced WU-6.5's bare dropdown with the list (below) because "which
  machine am I editing" and "what are this controller's layers" were always one
  question. The selector is a stable **layer ID**, never an index — inserting a
  layer renumbers every index above it (WU-6.3), and
  `uANIMCTRL_TOP_LEVEL_MACHINE` is the def's own machine.
- **Canvas** — states as boxes, transitions as lines with a clickable midpoint
  marker carrying the condition count. Drag a node to move it (one undo step, and
  a click that never moved records nothing); **Ctrl+drag** from one node onto
  another adds a transition; right-click a node for Set As Default / Delete.
- **Side panel** — the parameter DECLARATIONS (add / remove, with the LIVE value
  beside each one while a preview is running) and the def's clip-path list.
- **Inspector** — the selected state (its clip, from the def's own clip list) or
  the selected transition (duration, exit time, interruptible, and its condition
  list).

**★ NODE POSITIONS ARE AUTHORED DATA AND NEED NO SIDE-CAR.**
`Flux_AnimationState::m_xEditorPosition` is already a *serialized* field of the
`.zanimctrl` (it has been since before this panel existed, with a zero placeholder
written in non-tools builds), so a laid-out graph survives a round trip with no
`Zenith_EditorPrefs` entry and no second file to keep in step — and a node drag is
therefore an undoable EDIT that dirties the document. A state the def places at
exactly **(0, 0)** is treated as *never placed* and gets a slot in an automatic
grid the panel computes; that grid is **not written back**, because doing so on
OPEN would dirty a document nobody edited and rewrite a tracked asset for a
cosmetic reason. `Action_SetStatePosition` nudges a drop at the origin off it, so
one value cannot mean two things.

**★ A STATE'S TREE IS A CLIP LEAF OR A BLEND SPACE, AND A NEST IS REFUSED BY
NAME.** `GetStateTreeKind` answers EMPTY / SINGLE_CLIP / **BLENDSPACE_1D /
BLENDSPACE_2D** / COMPLEX. The two blend spaces are edited by the *Blend Tree*
strip below (WU-7.3); what is left in COMPLEX is a **composite**
(Blend / Additive / Masked / Select) or a container state's sub-machine, and one
of those shows `BlendTreeRefusalText()` on its node and in the inspector.
Assigning a clip to any of the three — a nest **or** a blend space — is refused,
because it would delete the whole sub-graph and report success.

**★ LIVE HIGHLIGHTING HAS TWO SOURCES, AND THE SELECTED ENTITY'S CONTROLLER WINS.**
`ResolveLiveMachine` takes the editor's primary selected entity, looks for a
`Zenith_AnimatorComponent` on it, and compares that component's
`GetControllerAssetPath()` against the document's `GetAssetPath()`. Both come out
of `Zenith_AssetRegistry::NormalizeAssetPath`, so the comparison is a plain
byte-for-byte one and an **empty** path on either side matches nothing rather
than everything. No match — no selection, no animator, a different asset — and
the panel's own preview controller drives the ring exactly as it did before.
`IsHighlightLive()` / `GetLiveHighlightEntityName()` say which, and the toolbar
prints **`Live: <entity name>`** or **`Preview`**: the two sources paint the
identical ring on the identical canvas, so a ring from the wrong controller is
otherwise indistinguishable from a correct one.

This is what the panel could not do when it was written — `LoadControllerAsset`
acquired the asset, called `BuildFromControllerDef` and recorded nothing, so
there was **no path to compare**, and matching on anything weaker (a layer count,
a state name) would ring a *different* character's graph and look right. C2 added
the recorded path; C3 consumes it.

**★ IT IS NOT GATED ON `EditorMode::Playing`, unlike
`Zenith_EditorPanel_GraphEditor::FindLiveGraphForHighlight`** — which is the
panel this one otherwise copies. A Behaviour Graph *instance* only exists while
the graph executes, so that panel genuinely has nothing to read when the editor
is stopped; an animator's `Flux_AnimationController` is built by
`LoadControllerAsset` and is addressable in **every** mode, sitting in whatever
state it was left in. A mode gate here would blank the ring on a stopped
character for a reason nothing on screen explains.

**★ ONCE THE ENTITY HAS MATCHED THERE IS NO FALLING BACK.** If the machine the
canvas is showing has no counterpart on the live controller — which is exactly
what an *unsaved, not-yet-applied* new layer looks like — the highlight is
**empty** and `IsHighlightLive()` stays true. Silently reading the preview's
machine instead would ring a state belonging to a different controller and look
entirely correct.

**★ THE REFRESH IS PER-FRAME AND SITS OUTSIDE `Render`'s
`m_bPreviewEnabled && fDtSeconds > 0` GATE.** The live source is driven by the
GAME: the selection can change, and the controller can move state, in frames
where the preview is off or `dt` is 0. Inside the gate, a selection-driven ring
would update only while the panel's own preview happened to be running. (Every
unit renders with `dt` 0, which is what makes the misplacement fail rather than
merely look wrong to a human.)

**★ `Action_Apply` STILL TARGETS THE PREVIEW ONLY**, live highlighting or not.
Pushing an unsaved document edit into a live entity's controller would be an edit
to the SCENE made from a document nobody saved.

**★ THE PREVIEW TICKS THE MACHINE, NOT THE CONTROLLER, AND NEEDS NO RIG.**
`Flux_AnimationController::Update` returns on its first line without a
`Flux_SkeletonInstance`, so the panel drives the selected machine's own `Update`
against a **zero-bone `Zenith_SkeletonAsset`** it owns. Transition evaluation,
exit times, trigger consumption and every blend-tree playhead advance exactly as
they do in a game; only the POSE is empty, which a state-machine graph does not
draw. That is what makes the highlight — and its unit — work headless with no
skeleton asset on disk.

**★ APPLY IS A RELOAD, NOT A REBUILD (D45).** It hands the working def to
`Flux_AnimationController::ReloadFromControllerDef`, which carries the current
state (by NAME), the normalized time (with its state), matched parameter values
(by name AND type) and the layer weights (by layer ID) across the edit.
`BuildFromControllerDef` is a demolition and would snap the graph to its default
state at frame 0 — which is precisely what makes editing while playing useless.
There is no direct-play preview here to re-arm; that is the dope sheet's, and it
owns its own.

**Operations live in `_Ops.cpp` and every one has a bool-returning `Action_*`
twin** (`Action_SelectLayerMachine` / `AddState` / `RemoveState` / `RenameState` /
`SetDefaultState` / `SetStateClip` / `SetStatePosition` / `AddTransition` /
`RemoveTransition` / `SetTransitionDuration` / `SetTransitionExitTime` /
`SetTransitionInterruptible` / `AddCondition` / `RemoveCondition` /
`AddParameter` / `RemoveParameter` / `AddClipPath` / `RemoveClipPath` / `Undo` /
`Redo` / `Save` / `Apply`, plus WU-7.2's nine layer verbs, WU-7.3's eight blend
verbs and the preview verbs). The mouse handlers in
`_Render.cpp` only translate input into those; the actions never read ImGui state.
Every mutation goes through a `Zenith_AnimControllerDocument` verb, and the
`AddStep_AnimSm*` automation family calls exactly the same twins.

**★ AN ASSIGNMENT VERB RETURNS TRUE WHEN THE VALUE IS ALREADY IN PLACE; A
CREATION RETURNS FALSE ON A DUPLICATE.** `SetDefaultState`, `SetStateClip`,
`SetStatePosition` and the three `SetTransition*` verbs answer *"is the value what
you asked for"* — asking for the value they already hold is the caller's intent
SATISFIED, so they return true and push no undo entry. `AddState`,
`Add/RemoveTransition`, `Add/RemoveCondition`, `Add/RemoveParameter` and
`Add/RemoveClipPath` answer *"did I create/remove one"*, so a duplicate or a miss
is a real refusal.

This cost a red test and is worth stating rather than rediscovering.
`Flux_AnimationStateMachineDef::AddState` makes the FIRST state of a machine its
default, so the most natural authoring order in existence — `AddState("Idle")`,
`AddState("Walk")`, `SetDefaultState("Idle")` — asks for a value that is already
in place *every single time*. With the old "false means nothing changed" reading
that asserted at boot under `AnimSmActionChecked` and read in a unit as "the entry
point could not be set". **"One edit, one undo step" is unaffected and is the
invariant to assert on**: a no-op is not an edit, so it contributes zero steps —
the stack depth is the contract, the bool is not.

It is a deliberate divergence from the dope sheet's `Action_SetAutoKey`, which
reports "the value CHANGED" and therefore needs `ANIM_POSE_SET_AUTO_KEY` excluded
from its checked wrapper by hand. Making the assignment family report
satisfaction instead means `AnimSmActionChecked` covers **every** `ANIM_SM_*`
verb with no exception list for a later verb to be forgotten from.

**★ AN EMPTY from-state ADDRESSES THE MACHINE'S ANY-STATE LIST** on every
transition verb, every automation step **and now on the canvas**. The list is
drawn as a **pseudo-node** labelled `<Any State>`: always present (it is the only
handle the first any-state edge can be drawn from), anchored to the canvas's
bottom-left corner rather than to a graph position, and therefore
**scroll-independent** — there is no verb that could store a position for it, and
a node that scrolled away would take the handle with it. It is out of
`RebuildAutoLayout`'s slot 0 at the top-left on purpose, because the hit test
checks it FIRST and an overlap would stop slot 0 taking clicks. Ctrl-drag from it
adds an any-state transition; a drop **onto** it is refused explicitly (nothing
transitions *into* the list), and rename/delete are drawn **disabled with a
tooltip** rather than omitted.

**The owner shift is ONE edit, and that is the thing not to "fix".**
`DrawTransitions` pushes `""` into `m_axRectOwnerOrder` before the sorted state
names, so owner 0 is the any-state list and every state's index moves up by one
in the single place indices are assigned. Both readers —
`GetTransitionMidpointRect` and `FindTransitionAtScreenPos` — resolve an owner by
scanning that vector **by name**, so they follow for free; adding a `+ 1` to
either would double-shift every key and hand out a neighbour's edge.
`AnimSmPanel::StateTransitionKeysStillResolveAfterTheOwnerShift` is the test that
catches exactly that, and it exists because nothing pinned a state-owned edge's
midpoint before.

**Its selection is its own flag** (`IsAnyStateSelected`), not an empty
`m_strSelectedState`: "nothing is selected" and "the any-state list is selected"
are both the empty string, and the inspector branch, the node context menu and
"Rename Selected" all read an empty name as *nothing*. Its RECT is its own member
too (`GetAnyStateRect`), not an entry in `m_xNodeRects`: `GetDrawnNodeCount()`
counts **state** nodes and is asserted to reach zero once every state has been
scrolled away, which a canvas-anchored entry would break forever.
`GetStateNodeRect("")` still refuses, because `""` is not a state.

**Hit rects and the off-screen contract** are the dope sheet's, verbatim, and for
the same reason: `GetStateNodeRect` / `GetTransitionMidpointRect` /
`GetCanvasRect` / `GetAnyStateRect` hand out only what was painted inside the
canvas this frame, and judge it against the display bound **captured when the rect
was recorded** rather than `ImGui::GetIO().DisplaySize` re-read at query time
(which is (-1, -1) outside a frame, i.e. for every unit assertion).
`WasCanvasDrawnLastFrame`, `GetRecordedDisplayWidth/Height`,
`GetRenderedFrameCount`, `GetDrawnNodeCount` and `GetDrawnTransitionCount` are
what tell the four causes of a flat `false` apart.

**The window title is the bare constant** `szEDITOR_WINDOW_ANIM_STATE_MACHINE` —
same rule, same reason as the dope sheet: `DockBuilderDockWindow` hashes the whole
string, so a decorated title would dock nothing. The dirty and
changed-on-disk badges are in the toolbar.

### The Layers strip (WU-7.2)

The layer list inside the Animator State Machine panel's side child — the def's
top-level machine as one row, then every `Flux_AnimatorControllerLayerDef` **in
blend order**, and the selected layer's controls under it: name, weight slider,
blend-mode combo, `m_bEmitEvents` checkbox (D36), bone-mask path field, and
Up / Down / Remove.

**★ IT LIVES HERE AND NOT IN THE DOPE SHEET, and the reason is where the
document is.** A layer list is a view over one `Flux_AnimatorControllerDef`, and
the only `Zenith_AnimControllerDocument` in the editor is this panel's. The dope
sheet's mask sub-panel (WU-7.1) is the *consumer* of one field of it — see the
push below.

**★ EVERY VERB ADDRESSES A LAYER BY ITS STABLE ID (D43), and the single index in
the family is `MoveLayer`'s destination.** An index is a *position in the blend
order*, which is precisely what a reorder changes and what an insert or a remove
renumbers — an index-addressed edit lands on a different layer with nothing to
observe. `MoveLayer(id, newIndex)` is the one verb whose argument *is* a
position, because that is what it sets.

**★ THE UNDO SPLIT IS THE ONE DESIGN DECISION WORTH DEFENDING.** Two commands,
because the two families of edit have different exact inverses:

| Edit | Command | Why |
|---|---|---|
| add / remove / **move** | `Zenith_AnimCtrlCommand_Layers` — the WHOLE list, each layer as its serialized payload | what changes is the ORDER, and "move it back" is exact for one move and wrong for two. It is also the only inverse the def offers: `Flux_AnimatorControllerDef` has `AddLayer` and `RemoveLayer(index)` and **nothing that reorders**, so rebuilding in order IS the operation |
| name / weight / blend mode / emit events / mask path | `Zenith_AnimCtrlCommand_LayerFields` — five values, by layer ID | a weight slider commits on every edit-complete; snapshotting the list would serialize every layer's entire state machine per drag to record five values. The ID is what makes that safe — it cannot go stale under a reorder, which is exactly the failure an index-addressed field command would have |

The payload is BYTES rather than a copy because `Flux_AnimatorControllerLayerDef`
owns a `Flux_AnimationStateMachineDef` by value and that type is neither
copyable nor movable; `Write`/`ReadFromDataStream` is the one faithful walk of a
polymorphic blend tree that already exists. `ApplySetLayers` re-`AssignLayerId`s
every rebuilt layer — not `SetLayerId` — so the def's monotonic counter stays
past every id in the list and the next `AddLayer` cannot mint a duplicate.

**★ SELECTING A LAYER DOES TWO THINGS, AND THE SECOND IS THE ONE THAT IS EASY TO
FORGET.** It selects that layer's machine on the canvas, **and** it pushes the
layer's blend mode into the dope sheet's mask sub-panel
(`Zenith_EditorPanel_Animation::SetMaskTargetLayerBlendMode`).
`Action_SetLayerBlendMode` re-pushes for the selected layer too, so changing the
mode is not a way to leave the sub-panel holding a stale one. Without either,
WU-7.1's section would go on offering a mask assignment for whichever layer was
last looked at — and an additive layer ignores its mask **entirely**, so the
failure is a mask authored, saved, assigned and never consulted, with every gate
green. The push is **guarded** (`g_xEngine.HasEditor()` +
`TryGetAnimationPanel()`): `Instance()` asserts, and a unit builds this panel on
the stack with no editor around it. The top-level machine is not a layer and
pushes `LAYER_BLEND_OVERRIDE`.

**★ A MASK PATH ON AN ADDITIVE LAYER IS REFUSED, and the rule is not restated
here.** `Zenith_BoneMaskDocument::LayerAcceptsMask` is the ONE statement of it and
`AdditiveLayerMaskNotice()` the ONE wording; the document asks both and
`GetLayerNotice()` forwards the answer. Switching a masked layer TO additive is
allowed and does **not** clear the path — the runtime stops reading it and the
strip says so, where silently deleting an authored path on a combo-box change
would be an unrecoverable edit disguised as a toggle. Clearing (an empty path) is
always allowed.

**★ THE STRIP DRAWS NOTHING WITH NO DOCUMENT OPEN** — not a header, not a
disabled row — and it lives **inside the side child**, which is what keeps it out
of the canvas's height. The canvas is a child sized out of the main window's
remaining region, so anything emitted into the main window before it comes
straight out of the graph, which is the dope sheet's "NOTHING SHOWN DRAWS
NOTHING" defect one panel over.
`AnimSmPanel::TheLayerStripDrawsNothingWhenClosedAndNeverTakesCanvasHeight`
measures `GetCanvasRect().Height()` with zero layers and with six and asserts
they are EQUAL — paired, as the rule requires, with a sensitivity check that the
height genuinely moves when the window does, so the equality cannot be satisfied
by a constant. That check grows the window rather than shrinking it: shrinking
far enough to be convincing can drive the canvas below `RenderCanvas`'s 8 px
floor on a high-DPI machine, which would fail for a reason that has nothing to do
with what is being tested. `WasLayerStripDrawnLastFrame()` and
`GetDrawnLayerRowCount()` are what a unit reads for the absence.

**Actions** — `Action_AddLayer` / `RemoveLayer` / `RenameLayer` /
`SetLayerWeight` / `SetLayerBlendMode` / `SetLayerEmitEvents` /
`SetLayerMaskAssetPath` / `MoveLayer` / `SelectLayer`, following the panel's
existing ASSIGNMENT-vs-CREATION rule verbatim: Add and Remove answer *"did I
create / remove one"*, everything else answers *"is the value what you asked
for"* and pushes nothing when it already was. The `AddStep_AnimLayer*` automation
family (the `ANIM_LAYER_*` block, the FIFTH animation range) calls exactly these
twins; `AddStep_AnimLayerExpectOrder(index, name)` is its assertion verb, because
the blend order is what the family is about.

**Removing the selected layer falls back to the top-level machine.** The
document does it in `ApplySetLayers` — one rule in one place, so an undo that
deletes the selected layer is covered by the same line — and the panel clears its
state/transition selection to match, because a state name means nothing in a
different machine.

**Not wired:** there is no enumeration of `.zanimmask` files anywhere in the
registry, so the mask field is a path text box plus a drop target. A picker needs
an asset-type enumeration that does not exist yet.

**★ THE DROP TAKES `DRAGDROP_PAYLOAD_ANIMMASK` AND NOTHING ELSE** (WU-9.2). It
used to accept the content browser's *generic* file payload and write whatever
that carried straight into the layer, so a `.zscen` dropped on the field became a
bone-mask path, was saved into the `.zanimctrl` and resolved to nothing at
runtime — with every gate green. The controller path field on the toolbar moved
to `DRAGDROP_PAYLOAD_ANIMCTRL` in the same commit, for the opposite reason: it
was the target that *did* match the generic id, and leaving it there would have
made the `.zanimctrl` drag stop working the moment the row got its own.

**Both drops are ImGui-free verbs** — `HandleControllerPathDrop(type, path)` and
`HandleLayerMaskDrop(layerId, type, path)` — each checking the payload id AND the
extension, because an id is a claim and the extension is what the reader depends
on. The ImGui block is the accept call and the pointer cast, and nothing else:
**a headless unit cannot fabricate a drag** (the panel fixture parks the mouse at
ImGui's invalid marker on purpose), so a drop written inline is code no assertion
can reach. `HandleLayerMaskDrop` still ends in `Action_SetLayerMaskAssetPath`, so
the additive-layer rule bites a dropped mask exactly as it bites a typed one.

### The Blend Tree strip (WU-7.3)

The blend-space sub-graph editor, inside the Animator State Machine panel's
**inspector**: a 1D axis or a 2D square with the state's blend points as
draggable markers, the axis parameter binding(s), add / remove / re-clip, and the
**live parameter dot**.

**★ THE REFUSAL DID NOT GO AWAY — IT GOT SMALLER, and that is the whole shape of
this unit.** WU-6.5's `ZENITH_ANIMCTRL_TREE_COMPLEX` covered a blend space, a
composite and a container alike. A blend space is now its own kind with an editor
(`ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D` / `_2D`), and COMPLEX is exactly the set
that still has no editor anywhere: a **Blend / Additive / Masked / Select** nest,
or a sub-machine. `Zenith_AnimControllerDocument::BlendTreeRefusalText()` is the
ONE wording of that refusal and the panel's static of the same name **forwards**
it, so the node badge, the inspector, the document's own
`GetLastBlendTreeDiagnostic()` and an authoring recipe's log line cannot describe
one refusal four ways. `SetStateClip` refuses a blend space as firmly as it
refuses a nest — "give this state a clip" applied to a space would delete every
point in it — and `SetStateTreeKind` is the verb that converts.

**★ A CONVERSION CARRIES THE CLIPS ACROSS.** A clip leaf seeds the new space's
FIRST point at the origin; a space converted back to a leaf keeps its first
point's clip; one space converted to the other carries every point (2D→1D drops
the y, 1D→2D lands them on y = 0). A conversion that started from scratch would
silently delete the one thing the state was already playing, and report success.

**★ THE UNDO UNIT IS THE WHOLE STATE, AS BYTES** (`Zenith_AnimCtrlCommand_StateTree`),
and this is the one design decision worth defending. **A blend tree has no
identity below the state**: a point is a struct in a `Zenith_Vector` addressed by
index, its child is an owned raw pointer with no id, and a **1D position edit
RE-SORTS the list** — so an index-addressed command would be holding a number the
very next edit can move. That is the transition list's problem answered one level
up, at the first thing that HAS a key. Bytes rather than a copy for the reason the
layer snapshot gives (a polymorphic tree has no clone verb), and bytes are also
what makes "undo a conversion" EXACT: a clip leaf turned into a space loses its
playback rate and its playhead, and nothing inside the space could reconstruct
them. The state is restored **in place** (`ApplyRestoreState`), never removed and
re-added — the machine's map is keyed on the name and every transition targeting
it resolves through that key.

**★ A 1D BLEND POINT EDIT CAN RENUMBER, AND EVERY LAYER SAYS SO.**
`Flux_BlendTreeNode_BlendSpace1D::Evaluate` brackets the parameter between
**adjacent** points, and `ReadFromDataStream` sorts on the way in — so the list
is kept sorted and dragging a point past a neighbour swaps two indices. The Flux
setter reports where the point went (`SetBlendPointPosition(..., u_int*
puOutNewIndex)`), the document forwards it, and `Action_SetBlendPointPosition`
**moves the panel's selection with it**. A selection left on the number would
silently start naming the point that was dragged past. A 2D space is not sorted,
so its indices are stable — what it needs instead is a **re-triangulation**, which
its setter does, because `FindContainingTriangle` reads a triangulation derived
from the positions.

**★ THE LIVE DOT IS ONLY MEANINGFUL BECAUSE WU-6.1 REPAIRED THE BINDING (D48).**
Before that, `Flux_AnimationStateMachine::EvaluateState` called `Evaluate` with no
parameter set at all, so a blend space sat frozen at its deserialized literal in
the editor and in a shipping game alike — a dot drawn from it would never have
moved. `GetLiveParameterDot(outX, outY)` reads the bound parameter(s) **by name
from the panel's PREVIEW controller's one live set** (D42), which is the same set
the preview's machines resolve their positions through, so the dot and the pose
cannot disagree. An **unbound** space has NO dot rather than a dot at zero: a
marker pinned at the origin is indistinguishable from a parameter that happens to
be zero, and "this axis reads nothing" is exactly what an author staring at a
space that will not move needs to see.

**★ AN AXIS BINDS ONLY A DECLARED `Float`, and that is the runtime's rule.**
`ResolveParameters` reads the binding through
`Flux_AnimationParameters::GetFloat`, so an Int or Bool declaration would be read
through the wrong union member — and an **undeclared** name is left at the literal
by the runtime, which is a binding that looks authored and does nothing.
`BlendParameterRefusalText()` is the ONE wording. An empty name UNBINDS and is
always allowed. On a 2D space the two axes bind **independently**; a 1D space has
no Y and asking for one is refused rather than answered with zero.

**★ THE STRIP DRAWS NOTHING UNLESS THE SELECTED STATE IS A BLEND SPACE** — not a
header, not a disabled row — and it lives **inside the inspector child**, which is
a FIXED height, so whatever it emits costs the canvas nothing. Same placement
argument as the layer strip, same rule the dope sheet learned twice.
`TheBlendStripDrawsNothingForASingleClipStateAndNeverTakesCanvasHeight` measures
`GetCanvasRect().Height()` with a single-clip state and with a five-point 2D
space and asserts they are EQUAL — paired, as the rule requires, with a
sensitivity check that grows the window (shrinking can drive the canvas below
`RenderCanvas`'s 8 px floor on a high-DPI machine).
`WasBlendStripDrawnLastFrame()` and `GetDrawnBlendPointCount()` are what a unit
reads for the absence.

**★ NO COORDINATE MATHS LIVES IN THE STRIP.** `BlendPositionToPixel` /
`BlendPixelToPosition` / `ComputeBlendAxisRange` are PURE statics — the dope
sheet's `Zenith_AnimTimelineMath` rule applied to a blend axis — so the draw and
the drag are one derivation and a unit asserts the round trip with no frame open.
The **axis range is recorded with the rects**, because it is half of the mapping:
`Action_DragBlendPointToPixel` refuses outright when the strip was not drawn last
frame, rather than inventing a range and dropping the point at a position the
strip never showed. **Screen y is inverted** against the blend axis on both the
draw and the drag; the pure helper is a linear map between two ranges and knows
nothing about which way a screen grows.

**Live edit:** every successful blend verb calls `Action_Apply()` when the preview
is enabled, so a moved point changes the sampled pose immediately. That is cheap
because blend edits commit on **edit-complete** (a drag commits once, on release),
so it is one Apply per gesture rather than per frame — and Apply is a RELOAD
(D45), so the preview keeps its current state, its playhead and its live parameter
values across the edit, which is precisely what makes dragging while it runs
legible.

**Actions** — `Action_SetStateTreeKind` / `SetBlendSpaceParameter` /
`AddBlendPoint` / `RemoveBlendPoint` / `SetBlendPointClip` /
`SetBlendPointPosition` / `SelectBlendPoint` / `DragBlendPointToPixel`, following
the panel's ASSIGNMENT-vs-CREATION rule verbatim. The no-op test is **byte
equality on the state's payload**, which covers every verb at once instead of each
growing its own field comparison: re-stating a value the tree already carried
leaves the bytes identical, so it returns true and pushes zero undo steps. The
`AddStep_AnimBlend*` automation family (the `ANIM_BLEND_*` block, the SIXTH
animation range) calls exactly these twins;
`AddStep_AnimBlendExpectPointCount` / `…ExpectPointPosition` are its assertion
verbs, and the position one exists because a renumber is what a recipe can get
silently wrong.

**Not wired:** a blend point's child is assumed to be a clip leaf. A def that
nests a composite under one is READ correctly (the point reports an empty clip
name) and refused for editing with the same wording a nested state gets — there
is no editor for a tree inside a tree.

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

### The split dispatcher: nineteen contiguous ranges

`ExecuteAction` is a **router, not a switch**. Before its (now small) main switch
it forwards **nineteen CONTIGUOUS enum ranges** to nineteen sub-executors
(`Zenith_EditorAutomation.cpp:4281..4441`), which is what keeps the dispatcher
inside the complexity gate. The table below is the **twelve non-animation**
ranges, in router order; the seven animation ranges follow them and are described
under "SEVEN ANIMATION ranges" below:

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

...and the seven animation ranges that follow them, in router order
(`:4376..:4441`):

| Range | Sub-executor |
|---|---|
| `ANIM_OPEN_CLIP` .. `ANIM_EXPECT_SELECTED_COUNT` | `ExecuteAnimationAction` |
| `ANIM_POSE_SELECT_BONE` .. `ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION` | `ExecuteAnimationPoseAction` |
| `ANIM_SM_OPEN` .. `ANIM_SM_EXPECT_DEFAULT_STATE` | `ExecuteAnimStateMachineAction` |
| `ANIM_MASK_OPEN` .. `ANIM_MASK_EXPECT_WEIGHT` | `ExecuteAnimMaskAction` |
| `ANIM_LAYER_ADD` .. `ANIM_LAYER_EXPECT_ORDER` | `ExecuteAnimLayerAction` |
| `ANIM_BLEND_SET_TREE_KIND` .. `ANIM_BLEND_EXPECT_POINT_POSITION` | `ExecuteAnimBlendAction` |
| `ANIM_CURVE_SET_VIEW` .. `ANIM_CURVE_EXPECT_KEY_TANGENT` | `ExecuteAnimCurveAction` |

**Ranges are COMPARED, never numbered.** Each row is a pair of `>=` / `<=` tests
against its block's first and last member, so:

- adding an action type at the **end of a block** is free;
- adding one **between two members of another block** silently routes it to that
  block's executor, where it hits the `default: Zenith_Assert` at boot;
- reordering members **inside** a block is invisible to the router but breaks the
  payload contract every step's `AddStep_*` packs into.

So every block carries a "must stay CONTIGUOUS" comment naming its first and last
member. **Every block added since is pinned twice** — a `static_assert` on its
WIDTH in `Zenith_EditorAutomation.h`, and a unit test on each member's POSITION
plus both neighbouring boundaries, so a reorder that preserves the width fails
naming the member that moved instead of at boot inside a neighbour's `default:`
assert — eight of them now: `Automation, GrassTypesEnumBlockIsContiguous`,
`… AnimEnumBlockIsContiguous`, `… AnimPoseEnumBlockIsContiguous`,
`… AnimSmEnumBlockIsContiguous`, `… AnimMaskEnumBlockIsContiguous`,
`… AnimLayerEnumBlockIsContiguous`, `… AnimBlendEnumBlockIsContiguous` and
`… AnimCurveEnumBlockIsContiguous`.

**SEVEN ANIMATION ranges sit at the end of the enum, and they are seven rather
than one for a mechanical reason.** `ANIM_*` (WU-3.4, the dope sheet),
`ANIM_POSE_*` (WU-4.3, the bone manipulator), `ANIM_SM_*` (WU-6.5, the
animator-controller state machine), `ANIM_MASK_*` (WU-7.1, the bone-mask
sub-panel), `ANIM_LAYER_*` (WU-7.2, the layer strip), `ANIM_BLEND_*` (WU-7.3, the
blend-tree strip) and `ANIM_CURVE_*` (WU-8.2, the curve view) each route to their
own sub-executor, and each new family was APPENDED as its own block rather than
added to the one before it — because appending into an existing block moves its
LAST member, which is the upper bound both the router's range test and the
header's `static_assert` compare against and which that block's unit pins by
position. `SET_NAVMESH_ASSET` follows all seven and must stay outside every range;
`AnimCurveEnumBlockIsContiguous` is where that is now pinned — the assertion has
been re-pointed six times (off `ANIM`'s unit, then `ANIM_POSE`'s, then `ANIM_SM`'s,
then `ANIM_MASK`'s, then `ANIM_LAYER`'s, then `ANIM_BLEND`'s: WU-4.3, WU-6.5,
WU-7.1, WU-7.2, WU-7.3, WU-8.2) rather than deleted, which is the mechanism
working.

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
