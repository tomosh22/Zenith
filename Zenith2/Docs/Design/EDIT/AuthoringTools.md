# EDIT design — authoring tools

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies the authoring tools EDIT builds over other areas' documents and operations, the reusable widgets they share, and the hosting of panels other areas specify. Z2-D-079 fixes the split: animation, terrain, UI and cinematics get visual authoring in the editor; materials and VFX are authored in code with previews and inspectors. In every case the owning area owns the document, its schema, its migration chain and its operations; EDIT owns the tool that drives them, its undo integration through the command system (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Commands and history") and its panels.

## Scope of this file

| Topic | Heading |
|---|---|
| graph canvas, timeline, curve editor, JSON document editor | "Reusable authoring widgets" |
| prefabs, the prefab stage, overrides, variants | "Prefab editing" |
| sculpting, painting, foliage, the live preview | "Terrain tools" |
| atmosphere, weather, water and other environment documents | "Environment documents" |
| light and probe authoring, grading, the bake panel | "Lighting tools" |
| material preview and tuning | "Material tools" |
| effect inspector and preview | "VFX tools" |
| the eight animation panels | "Animation tools" |
| the visual UI editor | "UI authoring tool" |
| AUDIO's panels | "Audio tools" |
| "Drop to rest", collider gizmos, PHYS's ragdoll preview | "Physics tools" |
| panels other areas specify | "Tools hosted for other areas" |

## Reusable authoring widgets

### Graph canvas

`Zenith_EditorGraphCanvas` (`Internal/Widgets/Zenith_EditorGraphCanvas.h`) draws and edits a node graph whose model the caller supplies through `Zenith_EditorGraphModel` (an interface: node list with IDs, titles, pins with kinds and connection rules, links, node positions), used by the animation graph and state-machine editors and available to every node-shaped document:

- Canvas: pan (middle drag, or `Space` + left drag), zoom 0.1 … 4 about the cursor (wheel, ×1.15 per notch), a background grid of 32 canvas units; nodes drawn as ImGui draw-list rectangles with a title bar and pin rows, links as cubic Béziers from output to input with horizontal tangents of length `max(40, 0.5 × |dx|)`.
- Editing: drag nodes (an interaction, one record per drag), box select, `Delete`, copy and paste of nodes with their internal links (fresh node IDs remapped like SCENE's remap), connect by dragging from a pin (the model's `CanConnect(outPin, inPin)` refuses incompatible kinds with the reason at the cursor), disconnect by dragging a link end away or `Alt`+click, insert a node on a link by dropping it there, a searchable "add node" menu (right click) listing the model's node catalogue, comment frames (`Shift+C` around the selection).
- Each operation is a command the model turns into document edits (JSON edits of the graph document), so undo is the command system's; the canvas holds no state beyond the view transform and the transient drag.
- Diagnostics returned by the model (a compile error on a node, an unreachable state) draw as a red frame and an error icon on the node, with the message as a tooltip.
- Performance: nodes outside the visible rectangle are culled before drawing; a 2,000-node graph draws within the panel slow-draw threshold.

### Timeline

`Zenith_EditorTimeline` draws tracks of keys and ranges along a time axis, used by the clip timeline (`Animation tools`), the music segment editor (AUDIO) and CINE's sequencer (pending CINE):

- Time axis in seconds with frame ticks at the document's frame rate (or bars and beats for music), zoom 1 ms … 10 min per 100 px, a playhead scrubbed by dragging the ruler (scrubbing calls the caller's `OnScrub(seconds)`), loop range handles, snapping of drags to frames, to other keys within 6 px, and to markers.
- Tracks: rows the caller supplies (name, kind, keys or ranges, colour), expandable groups, mute and solo toggles where the model supports them; keys dragged (an interaction), box-selected, copied and pasted with time offsets, deleted; ranges resized by their ends.
- The widget edits nothing itself: every gesture becomes a command of the caller's model.

### Curve editor

`Zenith_EditorCurveEditor` edits float curves of keys with interpolation per key (constant, linear, cubic Hermite with in and out tangents, the representation ANIM's curves and AUDIO's parameter curves use): drag keys and tangent handles (an interaction), add a key by double-click, frame all (`F`), value and time snapping, several curves overlaid with colours; tangent modes auto (Catmull-Rom tangents `(p_{i+1} − p_{i−1}) / (t_{i+1} − t_{i−1})`), flat, broken; the curve is evaluated for display with the owner's evaluation function, so what is drawn is what the runtime evaluates.

### JSON document editor

The JSON document editor is the fallback panel for every registered document type without a dedicated tool (and the raw view of those with one): a tree of the document's members in canonical order with property editors per JSON kind, constrained by the type's canonical schema (`Zenith_CanonicalSchema` of the registry, `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical serialization"): known members appear in schema order with their kinds, unknown members are shown read-only as "kept unknown", required members cannot be removed. Every change is a JSON edit (`Zenith2/Docs/Design/EDIT/Foundation.md` → "JSON edits") followed by the type's validator when one is registered; a change the validator rejects is undone at once (the command fails and nothing is recorded) with the validator's message shown.

## Prefab editing

### Creating a prefab

`prefab.createFromSelection` (inventory item Z2-I-EDIT-07):

1. The selected roots and their subtrees are copied with `Zenith_SceneEditSet::Copy`.
2. A new prefab document is created (`Zenith_SceneDocument::CreateEmpty(SCENE_DOCUMENT_KIND_PREFAB, fresh ID, name)`), and the set is pasted into it as the prefab's content (a single root: the copied root; several roots: under a new root entity named after the prefab), with the IDs the copy carries (the prefab's local IDs).
3. The prefab document is saved as a new file under the chosen folder, given a GUID through `assignGuids` and imported.
4. In the scene, the copied roots are replaced by one `SCENE_EDIT_PLACE_INSTANCE` of the new prefab whose replaced-entity list is the copied entities, so their derived IDs (`Zenith_DeriveAuthoringID(instance, local)`) keep every reference from the rest of the scene valid when the prefab keeps the original IDs as local IDs; references are rewritten through the remap table where SCENE's derivation assigns new IDs.
5. Steps 2–4 are one transaction (the prefab file creation is a file-operation change), undone together.

### Prefab stage

Opening a prefab (double-click in the asset browser, "Open prefab" in the inspector) enters a prefab stage: a separate edit world (`Zenith2/Docs/Design/EDIT/Foundation.md` → "World kinds") projecting the prefab document alone, shown in the scene view with a breadcrumb ("Main scene › Door") and a neutral environment (or, with "Edit in context", the main scene drawn greyed behind it from the main edit world's view). Edits in the stage are edits of the prefab document; saving the prefab reconciles every instance in the main edit world (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind"). Nested prefab instances inside the prefab can be entered in turn (the breadcrumb grows); variants open their base read-only in a second stage on request.

### Instances overrides and variants

- Placing a prefab (drag from the asset browser into the scene view or the hierarchy) is `SCENE_EDIT_PLACE_INSTANCE` with an empty override block at the drop point (the pivot on the surface under the cursor, `Zenith2/Docs/Design/EDIT/Viewports.md` → "Surface snapping").
- Every edit of an instance entity's property becomes an override (SCENE writes it, `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Document edits for the editor"); the inspector shows the override state (`Zenith2/Docs/Design/EDIT/Panels.md` → "Prefab override display").
- "Create variant" (`prefab.createVariant`) creates a prefab document whose variant base is the selected prefab and whose override block is empty, saved and imported like a new prefab; the variant opens in a stage where edits become the variant's overrides. The walkthrough "author a prefab variant" (Z2-I-EDIT-19, Z2-B-EDIT-008) uses exactly this path (`Zenith2/Docs/Design/EDIT/Automation.md` → "Walkthrough author a prefab variant").

### Prefab apply and revert

| Command | Edit |
|---|---|
| `prefab.revertOverride` (one property), `prefab.revertAll` (an instance) | `SCENE_EDIT_REVERT_OVERRIDE` per override |
| `prefab.applyOverrides` (selected properties), `prefab.applyAll` | `SCENE_EDIT_APPLY_OVERRIDES_TO_PREFAB` with the instanced prefab's writable document (`pxPrefabDocument`, row S20); the prefab document is opened when it is not open; both documents are in the record |
| `prefab.unpack` | `Zenith_UnpackPrefabInstance`; its returned inverse is the record's inverse |
| `prefab.replace` (swap an instance's prefab) | `SCENE_EDIT_DELETE_INSTANCE` + `SCENE_EDIT_PLACE_INSTANCE` with the overrides whose targets exist in the new prefab kept, the rest reported dangling |

Applying to a prefab that is itself read-only (a newer version, an engine root) is refused before any edit.

## Terrain tools

### Terrain panel

The terrain panel drives ENV's terrain document (`Zenith2/Docs/Design/ENV.md` → "What ENV authors"; row EN12, A5 row EnB13): tool selection (raise, lower, flatten, set height, smooth, noise, ramp, thermal erosion from Stage 5; paint layer, erase layer, holes, density, exclusion from Stage 4; foliage scatter and erase), the brush settings (radius 0.5 … 512 m, strength and hardness 0 … 1, the target height for flatten and set, the noise wavelength and seed, the layer or foliage type or rule set), the layer palette (add, remove, reorder through `AddLayer`, `RemoveLayer`, `MoveLayer`, each a structural edit returning a `Zenith_TerrainEdit`), the tile grid (add and remove tiles), and the regeneration status.

### Sculpt and paint strokes

<!-- elaboration -->
1. A left-button drag in a scene view with a terrain tool active is a stroke and an interaction (merge key `Zenith_Hash64("terrain", document, stroke ordinal)`).
2. Dabs: while dragging, the cursor ray hits the terrain (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Precise mesh picking", terrain row); a dab is emitted at the first hit and then every `spacing = max(0.25 m, 0.25 × radius)` of arc length along the hit path (linear interpolation between frames' hits), each `Zenith_TerrainDab { centre, radius, strength, hardness }`; a ramp dab is the segment from the stroke's start point to the dab centre.
3. Each editor frame of the drag applies the frame's new dabs as one `Zenith_TerrainStroke` with `Zenith_TerrainDocument::ApplyStroke(stroke, pxJobs, xEditOut)` (the job system given, so large dabs fan out and join before return; `Zenith2/Docs/Design/ENV/Terrain.md` → "Terrain edit deltas and undo"), records the returned `Zenith_TerrainEdit` as an `EDITOR_CHANGE_TERRAIN_EDIT` change merged into the stroke's record, and runs the live preview step 1 (`ConsumeDirtyRegions`, `QueueRegions`).
4. Releasing the button ends the interaction and starts the regeneration (§ "Regeneration after a stroke").
5. Stroke input is deterministic: the same dab list applied to the same document produces the same bytes, so an automation script's stroke (the step `terrainStroke`, `Zenith2/Docs/Design/EDIT/Automation.md` → "Step kinds") reproduces a recorded edit exactly.

### Brush cursor

The brush cursor is drawn in the scene view as a ring of 64 segments on the terrain surface (each segment point's height sampled with `SampleHeight`), at the radius and at `hardness × radius`, coloured by the tool kind, plus `Zenith_TerrainBrushFalloff` (ENV's cursor function) shown as a falloff profile in the panel.

### Terrain undo

- Each stroke record holds the stroke's frame segments as an ordered list of `Zenith_TerrainEdit`s; undo applies `ApplyEdit(edit, TERRAIN_EDIT_DIRECTION_REVERT)` newest segment first and redo `REAPPLY` oldest first, so the revision chain ENV checks holds (`ENV_ERROR_EDIT_STALE` is unreachable through the history; reaching it fails the undo as § "Undo and redo" of `Zenith2/Docs/Design/EDIT/Foundation.md` describes).
- The record's memory is the sum of its edits' `GetMemoryBytes()`, counted against the history bound; a stroke record above `editor.undo.maxTerrainRecordBytes`, which takes its value from the pending row Z2-B-EDIT-011 "Terrain edit delta memory per undo step" ([Z2-N-BUDGET-328]: the memory one terrain undo step holds), becomes a fence (`Zenith2/Docs/Design/EDIT/Foundation.md` → "History memory bound"); ENV counts the same event in `environment.terrain.edit_over_budget`. Records of other kinds are bounded by `editor.undo.maxRecordBytes`, never by the terrain row.
- Every applied or reverted edit marks its regions dirty, so undo also updates the preview and regenerates.

### Terrain live preview

EDIT drives ENV's live preview (`Zenith2/Docs/Design/ENV/Terrain.md` → "Live preview") in the editor frame:

| ENV step | EDIT's call site |
|---|---|
| 1 | hook `OnInputFrame`, after each `ApplyStroke` or `ApplyEdit`: `ConsumeDirtyRegions`, then `Zenith_TerrainPreview::QueueRegions` |
| 2 | hook `OnPrologueEnd` (the frame prologue of the edit world's frame, before the frame graph runs): `Zenith_TerrainPreview::Prepare`, at most `environment.terrain.previewTilesPerFrame` tiles |
| 3 | the edit world's `Editor.Extract` node in phase 12: `Zenith_TerrainPreview::WriteRecords` (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor extractor") |
| 4, 5 | ENV's GPU side and § "Regeneration after a stroke" |

`Zenith_TerrainPreview` is created per edit world that holds an open terrain document and owned by the editor's terrain tool (ENV's contract block for it). The pending row Z2-B-EDIT-012 "Terrain sculpt preview latency" is measured from the end of a dab's `ApplyStroke` to the first presented frame showing it ([Z2-N-BUDGET-328]): the automation step `terrainStroke` records the time after `ApplyStroke`, and the end event at `OnFrameEnd` of the frame whose render subgraph drew with the replaced height chain.

<!-- assumption: pending ENV -->
ENV's GPU side publishes, per terrain, the frame index of the last frame in which a preview height chain replacement became visible (the frame in which the height texture's `GetSRV()` changed, GEOM's step 5), as the gauge `Flux.Environment.PreviewPublishedFrame`; the editor compares it with the frame index of the dab's `ApplyStroke` to find the end event (row ED20).
<!-- /assumption -->

### Regeneration after a stroke

When a stroke, an undo or a redo ends (the interaction ends, or the undo or redo returns), EDIT calls `Zenith_RegenerateTerrainDerivedData(document, the stroke's regions, flags, inputs, pxJobs, arena)` (`Zenith2/Docs/Design/ENV/Bake.md` → "Regeneration after edits") with `TERRAIN_REGENERATE_TILE_DATA | TERRAIN_REGENERATE_FOLIAGE` from Stage 3 and `| COLLISION | NAVIGATION` from Stage 4, the inputs filled from the loaded documents (the cooked height range, the placement rule sets, the foliage type infos from the open foliage type documents, the exclusions, the collision types, the water shapes), and hands the produced files to ASSET's in-memory asset replacement (row EN3, pending ASSET) so the edit world reloads them at a tick boundary; collision and navigation outputs go to PHYS's and AI's cooks the same way (rows EN3, EN4, pending PHYS and AI). The regeneration runs as a background operation when it covers more than 4 tiles (the job system fans the tiles out; the main thread polls the operation), and a new stroke cancels and restarts it for the union of the regions.

### Terrain creation wizard

"New terrain" asks for the origin, the tile counts (1 … 256 each), the initial height, the height range, the base layer document and the placement seed, calls `Zenith_TerrainDocument::Create(desc, heap)`, saves the terrain and its tile documents through the terrain sink as new documents, imports them, and places an entity with ENV's terrain component referencing it in the active scene, as one record of two changes: an `EDITOR_CHANGE_FILE_OPERATION` create (asynchronous: `assignGuids`, then `import`; its result publishes the new terrain asset's GUID), then a `Zenith_EditorDeferredSceneOperation` reading change 0's result, whose builder writes `SCENE_EDIT_CREATE_ENTITY` and the terrain component's `ADD_COMPONENT` with that GUID once the import completed (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command"). One undo removes the entity and moves the new files into the editor trash; one redo moves them back from the recorded trash directory, so `assignGuids` returns the terrain's first GUID and the deferred placement, which reuses the edits built at the first `Begin`, references a resolvable GUID again (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes", the create and duplicate row). `Test_EditorTerrain_WizardOneRecordPlacesImportedTerrain` runs the wizard against the fake cook server and asserts one record, the entity's component referencing the GUID the fake `assignGuids` returned, that undo leaves the scene's bytes and the folder as before, and that redo then places the entity again with its terrain component referencing a GUID equal to the first, which the fake server's `record` resolves.

### Foliage tools

- Scatter and erase are terrain brush kinds (`TERRAIN_BRUSH_SCATTER_FOLIAGE`, `TERRAIN_BRUSH_ERASE_FOLIAGE`) applied as strokes; a single placement is a one-dab scatter stroke (`Zenith2/Docs/Design/ENV.md` → "What ENV authors").
- The placement preview runs `Zenith_Place` over the terrain document for the tiles in view (re-run on dirty regions) and draws the would-be instances as DEVT debug-draw markers in the edit world's views, so procedural placement is visible before a cook (`Zenith2/Docs/Design/ENV/Procedural.md` → "Runtime and editor use").
- Foliage type documents are JSON documents edited in their inspector (§ "Environment inspectors").

## Environment documents

### Environment inspectors

The foliage type, placement rule set, atmosphere, weather, water and cloud-noise documents are JSON documents edited through the JSON document editor with dedicated layouts (grouped members, colour pickers, curve editors for the time-of-day gradients), each change validated by the document's reader (`Zenith_ReadFoliageTypeDocument` and its siblings, ENV's "Zenith_EnvironmentDocuments header") before it is committed (A5 row EnB13): a rejected change fails the command and nothing is recorded. A saved environment document is re-cooked by the server's watch and reloaded through the asset registry; the edit world re-records the document's command kinds where ENV's projection handler asks (§ "Time of day and weather scrubbing").

### Time of day and weather scrubbing

The environment panel's time-of-day slider and weather selector record ENV's five command kinds into the edit world (`Zenith2/Docs/Design/ENV.md` → "What ENV authors"), applied out of tick; they write ENV's world resource `Zenith.Environment.State` (`Zenith2/Docs/Design/ENV.md` → "Zenith_EnvironmentState header": `TYPE_KIND_RESOURCE`), change the edit world's presentation only, never a document (an edit-world-only write, not a history record, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes"), and play worlds start from the documents' authored time and weather, because a play copy carries entities alone and a new world's resource starts from the authored settings (row ED31).

## Lighting tools

### Light and probe inspectors

LIGHT has no tool of its own (`Zenith2/Docs/Design/LIGHT.md` → "What LIGHT authors"): lights, probes, volumes, world lighting and camera lighting are components edited in the generic inspector, with component inspectors registered by the Lighting extension for the light (units shown with LIGHT's unit suffixes, the colour-temperature picker), reflection probe (capture mode, baked asset, extents) and irradiance volume components, and the component gizmos of `Zenith2/Docs/Design/EDIT/Viewports.md` → "Component gizmos".

### Probe capture

"Capture now" on a probe increments its `CaptureRequest` property (`Zenith2/Docs/Design/LIGHT.md` → "Editor surfaces"): in edit mode as a document edit of the probe's serial (recorded, so it is undoable and saved) when the probe's capture mode is baked, and as an edit-world-only write when it is realtime (a `SetProperty` into the edit world, never a document or a record), which the play copy replaces by the document's value (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes", row ED32); LIGHT's capture runs as a `VIEW_KIND_CAPTURE` view.

### Grading editor

The grading editor edits LIGHT's camera-lighting and grading document members (exposure, the 3D LUT asset, the per-view LUT texture path) with a live preview in the scene view: the scene view's `Flux_ViewRequest::m_xCameraLighting` carries the edited settings each frame (`Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"), so tuning shows at once, and the document change is a JSON or scene property edit.

### Bake panel

<!-- assumption: pending WORLD -->
Lighting bakes (probes, baked GI) are WORLD's baking workflows (`Zenith2/Docs/Design/LIGHT.md` → "What LIGHT authors": "the bake is WORLD's"), executed by LIGHT's cook steps. The bake panel lists the active scene set's bake inputs and outputs and their freshness, and "Bake" sends the cook server a `cook` request for the bake outputs' GUIDs (the cook runs LIGHT's steps out of process, so a bake never blocks the editor), shows progress and diagnostics, and reloads the published outputs; the multi-cell orchestration (per partition, incremental, resumable) is WORLD's (pending WORLD, row ED18), which the panel calls when it lands.
<!-- /assumption -->

## Material tools

Materials are authored in code (a Slang surface function) plus a `.material.json` document (Z2-D-079; `Zenith2/Docs/Design/GEOM.md` → "What GEOM authors"). The material panel:

- shows the document's parameters with property editors from the material's reflection JSON (the parameter names, kinds, ranges written by the shader compiler's cook mode), edits them as JSON edits, and applies the document's value live with `Flux_MaterialResource::ReplaceParameters` (`Zenith2/Docs/Design/GEOM.md` → "Editor surfaces") through the material projection handler after every forward and inverse change (`Zenith2/Docs/Design/EDIT/Panels.md` → "Material preview"), so the material preview and every viewport update in the same frame without a cook, and undo and redo update them too;
- validates a changed document through the cook server's `validate` request (the material document reader lives in ZenithCook, which the editor does not link; A5 row GB24), shows its findings, and reverts nothing automatically (validation findings are warnings while editing; a document the cook rejects fails at the next cook with the same message);
- reads the preview keys (`preview` members: shape, environment, exposure) from the document itself (GB24);
- reloads when the material's Slang module changes, through the shader reload path (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload", the material `layout-changed` row);
- opens the Slang source in the external text editor ("Edit shader").

## VFX tools

VFX effects are authored in code with previews and inspectors (Z2-D-079; `Zenith2/Docs/Design/VFX/Authoring.md` → "Effect inspector", "Preview session"; row VX11): the effect component inspector (the effect picker and its named parameters with declared ranges, a VFX-registered component inspector); the play, stop and restart controls in the edit world (out-of-tick writes of `PlayRequest` and `StopRequest` into the edit world, edit-world-only, not recorded, and replaced by the documents' values in the play copy: `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit-world-only writes"); the viewers, `Zenith.VFX.Viewer` components on the editor proxy entity of each scene view (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor proxy entities", row ED30), so LOD selection follows the scene view; and the effect preview in a preview world of purpose `WORLD_PURPOSE_PREVIEW` (`Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host"), with the debug views of VFX in the view menu. A changed effect module reloads through the shader reload path.

## Animation tools

Animation gets visual authoring (Z2-D-079; inventory item Z2-I-ANIM-16, Stage 4); ANIM owns the documents and operations and EDIT the eight panels below (`Zenith2/Docs/Design/ANIM.md` → "What ANIM authors", row AN11). Every panel edits its document through JSON edits (or, for `.animgraph.json`, through the graph canvas whose model turns gestures into JSON edits), so undo, save, migration and recovery are the editor's common machinery; the editor registers ANIM's document types through `Zenith_RegisterAnimationDocumentTypes` (row AN11).

<!-- assumption: pending ANIM (rows ED17, ED39) -->
The graph compiler `Zenith_CompileAnimationGraph` lives today in `Tools/ZenithCook/Internal/Animation/Zenith_GraphCompiler.h` (`Zenith2/Docs/Design/ANIM/Graph.md` → "Compile steps"); the editor calls it in process for live diagnostics and for the preview's in-memory compile. Ruling Q84 (a) moves it into `ZenithAuthoring` (`Zenith2/Tools/Authoring/Internal/Animation/`, public header `Zenith2/Tools/Authoring/Zenith_AuthoringAnimation.h`), which both tools link (`Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs", row ED39). Node and state canvas positions are stored in the graph document as a known optional member `editor` (`{ "x": number, "y": number, "collapsed": boolean }` per node and state, and a top-level `editorComments` array of `{ "x", "y", "w", "h", "text" }`) of ANIM's canonical schema, so layouts are shared, canonical and free of unknown-field warnings (row ED17).
<!-- /assumption -->

### Animation graph editor

- The canvas shows the graph's nodes (ANIM's node catalogue: clip players, blend spaces, blends, layers, slots, IK steps, state machines), the parameters list (kinds and defaults; add, rename, remove, each a JSON edit), and the layers.
- A state machine node opens a nested canvas of states and transitions; a transition's inspector edits its conditions, duration, blend curve, interruption rules and sync settings.
- After every edit the panel compiles the edited document in memory with `Zenith_CompileAnimationGraph` (bounded to `editor.animation.compileBudgetMs`, default 50 ms; a longer compile runs as a background operation and the last result stays shown) and shows the compiler's diagnostics on their nodes; a graph that compiles is pushed to the preview (§ "Animation preview"), so what the author sees is the runtime's evaluation (`Zenith2/Docs/Design/ANIM.md` → "Editor surfaces").

### Blend space editor

A 1D axis or a 2D plane of the blend space's samples (each a clip and a position), draggable as an interaction; the compiler's Delaunay triangulation returned with the diagnostics is drawn for 2D spaces; a crosshair drives the preview's parameters live; samples added by dropping a clip from the asset browser.

### Mask editor

The skeleton tree of the mask's skeleton with a weight slider (0 … 1) per joint, "apply to children", presets (upper body, lower body, arms) as named selections, and a skeleton view in the preview with joints coloured by weight; edits of `.animmask.json`.

### Rig setup

Roles, chains, IK definitions, sockets, mirror pairs, shell distances and curves of `.animrig.json`, each a list with an inspector; joints chosen from the skeleton tree or by clicking a joint in the preview (joint picking against the preview pose's joint spheres); IK chains drawn in the preview.

### Clip timeline

The timeline widget over a clip: tracks for events, notify states, markers and curves; loop range; root-motion settings, additive settings and compression settings in the inspector; scrubbing drives the preview's clip time (`Zenith_AnimationPreview::SetClipTime`).

### Retarget setup

Source and target rigs side by side, the chain mapping table of `.animretarget.json`, and a live preview through `Zenith_RetargetMap` playing a source clip on the target skeleton.

### Animation preview

A preview panel over a preview world (`Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host") whose skinned mesh is posed by `Zenith_AnimationPreview` (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationPreview header"): parameter sliders (`SetParameter`), trigger buttons (`SetTrigger`), slot triggering (`PlaySlot`), play, pause, step and scrub, the root-motion trail, the fired events list (`GetEvents`), and the debug state (`GetDebugState`: active states, weights, sync phases). The preview recompiles on each edit without a cook round trip. The pending row "Animation graph preview latency" (row AN12, W-Editor; produced by EDIT's Stage 1 per-system budget ticket) is measured from a graph document edit to the first frame drawing the recompiled graph's pose.

### Live animation debugger

For a selected character of a play world: `Zenith_AnimationWorld::GetDebugState(simulation ID)` once per editor frame, shown as the active graph path, the state machine's current state and transition, parameter values and layer weights, with the graph canvas of the character's graph highlighting the active nodes; `anim.dump` from the console.

## UI authoring tool

The visual UI editor (Z2-D-079; inventory item Z2-I-UI-14; row UI15; `Zenith2/Docs/Design/UI.md` → "Editor surfaces") is built on what UI provides:

- the document model `Zenith_UIEditorModel` (reversible edit operations: insert, remove, move, set member, set style, bind, rename; validation after each edit; canonical save): every gesture of the tool is one model operation recorded as an `EDITOR_CHANGE_UI_EDIT` change with the inverse the model returns;
- a canvas showing a preview `Zenith_UIContext` laid out at the chosen surface size (presets for 1920 × 1080, 2560 × 1440, 2340 × 1080 and custom), locale, palette, pseudo-localization mode and text scale, drawn into a preview view; element picking with the context's `HitTest`; selection by element `uid`; drag to move and resize within the parent's layout rules; an outline of the layout boxes;
- the element palette from UI's widget catalogue (`Zenith_UIWidgetDesc` per element type with its members and defaults) and the style property inspector from the style property catalogue (`Zenith_UIPropertyDesc`);
- the element tree (hierarchy of the UI document), the bindings panel, the theme editor over theme documents, the string table editor over string table documents;
- validation diagnostics with document paths shown inline on the elements.

## Audio tools

AUDIO specifies its panels in full (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces": the bank and event editor, the mixer console, the audition panel, audition in context and the voice inspector), implemented in `Tools/ZenithEditor/Internal/Audio/` against the extension interface (row AU18). EDIT provides to them: document access and undo through the command system (bank and mixer documents are JSON documents), the timeline and curve widgets, drag of sound assets from the asset browser, the play-in-editor world's running `Zenith_AudioSystem` reached through `Zenith_RuntimeToolAccess::FindModuleInstance(runtime, "Audio")` and AUDIO's tools-only accessor `Zenith_Module_Audio_GetSystem` (row AU3), and the in-process preview cook of one bank or the mixer section (row AU18), which ruling Q84 (a) places in `ZenithAuthoring` (`Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h`, row ED41).

## Physics tools

<!-- assumption: pending PHYS (row ED34) -->
PHYS (drafting) asks the editor for two surfaces (`Zenith2/Docs/Design/PHYS.md` → "Spawn clearance rule": "the editor offers "Drop to rest", which runs a 2-second isolated simulation of the selected bodies and writes the settled poses through an undoable edit"; → "Editor surfaces": "a ragdoll preview that steps an isolated world on the editor's job system"). Both use an isolated simulation PHYS declares, which row ED34 asks for: `Zenith_PhysicsSandbox` (an isolated physics world outside every ECS world, created from a list of bodies with their shapes, materials, motion types and world poses, stepped on a given `Zenith_JobSystem` at a fixed tick, with read access to each body's pose). Until it lands the two surfaces are `draft`.

### Drop to rest

`physics.dropToRest` (enabled when the selection holds at least one entity with a dynamic physics body, edit mode only):

1. Collect the bodies: every selected root entity and descendant with a dynamic body (their shapes and materials from the edit world's physics components through the world inspector), plus every static or kinematic body of the edit world whose world AABB intersects the union of the selected bodies' AABBs grown by 2 m (the surfaces they may come to rest on).
2. Create the sandbox with those bodies at their current world poses and step it for `ceil(2 s × tick rate)` ticks (120 at 60 Hz) on the editor's job system, as a background operation when the step exceeds `editor.frame.syncBudgetUs`; the edit world is not stepped (ED5 holds) and no document changes while it runs.
3. Read each selected dynamic body's settled world pose; convert it to a local transform against its parent's world matrix exactly as a gizmo drag does (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Applying a gizmo delta"); for an entity of a prefab instance the edit is an override.
4. Execute one transaction of `SCENE_EDIT_SET_PROPERTY` edits of `Zenith.LocalTransform` position and rotation, labelled "Drop to rest (n bodies)", so one undo restores every pose. A body whose pose changed by less than 1 mm and 0.01° writes nothing.
5. Failures: PHYS refusing the sandbox (`EDITOR_ERROR_PHYSICS_SANDBOX`, the PHYS error logged) records nothing; a body that leaves the sandbox's bounds (falls without a surface) is reported and excluded from the edits.

`Test_EditorPhysics_DropToRestOneRecord` drops a box placed 0.5 m above a fixture floor, asserts one record whose undo restores the original pose and whose resting height is within 1 cm of the floor plus the box's half extent, and that a selection without dynamic bodies leaves the command disabled.

### Ragdoll preview

The ragdoll document's editor (the JSON document editor with PHYS's layout) shows a preview panel over a preview world (`Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host") with the ragdoll's skinned mesh, whose pose comes from stepping the ragdoll in a `Zenith_PhysicsSandbox`; it is an area-owned panel, ticketed by PHYS against the extension interface under `Internal/Physics/` (`Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels", row ED15). EDIT provides the preview host, the panel host and document undo.
<!-- /assumption -->

## Tools hosted for other areas

| Area | Tool | EDIT provides | State |
|---|---|---|---|
| CINE | the cinematics sequencer: tracks for cameras, animation, audio, events, dialogue and subtitles; scrubbing and blending preview (Z2-I-CINE-13), specified in `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel" | `Zenith_EditorTimeline` (its `OnScrub`), `Zenith_EditorCurveEditor` with CINE's evaluation function, preview worlds with the Cinematics module in the editor composition, the document and asset kind handlers, document undo, play-in-editor | CINE-ticketed under `Internal/Cinematics/` (row CN13) |
| TWOD | 2D authoring: sprite and atlas editors, the tilemap painter, 2D animation (Z2-I-TWOD-09) | an orthographic 2D scene view mode (the Front view with 2D navigation: pan with middle drag, zoom at the cursor), pixel-grid snapping, the brush-stroke model of the terrain tools for tile painting | pending TWOD's design |
| CHAR | the character workflow import → rig and retarget → author the graph → preview → in game (Z2-I-CHAR-11 to -16) | the animation panels in sequence and its timed walkthrough (Z2-B-CHAR-001) on the same automation machinery | pending CHAR's design |
| WORLD | world partitions, data layers, concurrent editing granularity, baking workflows | per-partition document ownership in the document table, the bake panel | pending WORLD's design |
| AI | navmesh baking, off-mesh links, behaviour authoring, debugging (`Zenith2/Docs/Design/AI.md` → "Editor surfaces") | the extension interface (`Internal/Navigation/`, tests `Tests/Navigation/`), component gizmos, the bake panel's cook requests, AI's `ZenithAuthoring` part for in-process bakes, live debugging in play worlds | AI-ticketed (row AI12) |
| PACK | the packaging panel: `package.recipe.json` edited as a JSON document (registered by `Zenith_RegisterPackageDocumentTypes` from `ZenithAuthoring` once PACK moves it, row ED49) with the framework's preview, backup and journaled write; `size-report.json` and `partition.json` shown read-only (`Zenith2/Docs/Design/PACK.md` → "Editor surfaces") | the panel host, the JSON document editor, document undo, the migration flow | PACK-ticketed under `Internal/Package/` (row PK9) |
| CONTENT | the import preset picker (`Zenith_ImportPresetSet::GetPresetsFor`), the product-override editor validated by `Zenith_ValidateProductOverridesJson`, batch progress and `report.json`, the merge driver's `--explain` output behind "Open merge tool", the `contentImports` report member (`Zenith2/Docs/Design/CONTENT.md` → "Editor surfaces CONTENT supplies") | the panel host, `Zenith_EditorContentCookChannel`, `Zenith_EditorContentBatchOperation` (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reference repair") | CONTENT-ticketed under `Internal/Content/` (row CT18) |
| PHYS | the ragdoll preview (§ "Ragdoll preview") | the preview host, the panel host, document undo | PHYS drafting; row ED34 |
| STREAM | the streaming panel in play worlds (`Zenith2/Docs/Design/STREAM.md` → "Editor surfaces") | the panel host; `Streaming.SetCellOverride` through the console (tools context) | STREAM specifies it |
| SAVE | play-in-editor saves under the profile key `editor-<play world tag>`, `save.dump`, the replay viewer (`Zenith2/Docs/Design/SAVE.md` → "Editor surfaces") | the panel host, the play controller | SAVE specifies it |
| GAME | the camera overlay debug view (`Zenith2/Docs/Design/GAME.md` → "Debug views and console commands"); play worlds created with purpose `WORLD_PURPOSE_PLAY` and origin `WORLD_ORIGIN_EDITOR_PLAY` and paused with `WORLD_PAUSE_REASON_EDITOR` (row GA14) | the play controller | GAME specifies it |

