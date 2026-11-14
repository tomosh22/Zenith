# EDIT design — panels and extensions

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies the panel framework and the editor's extension interface (static, compiled into the editor, Z2-D-024), and every generic panel with its data source: toolbar, hierarchy, inspector, schema panel, asset browser, previews and thumbnails, console, log, diagnostics, migration panel, project settings, the new-project wizard, the input-bindings editor, the recovery dialog, the source-control panel, preferences and the profiler integration. The authoring tools that drive other areas' documents are in `Zenith2/Docs/Design/EDIT/AuthoringTools.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| how a panel and an extension plug in | "Panel framework" |
| play controls, gizmo modes, snapping | "Toolbar" |
| the scene tree | "Hierarchy panel" |
| component and document property editing | "Inspector panel" |
| data-defined component schemas | "Schema panel" |
| assets: listing, import, settings, dependencies | "Asset browser" |
| previews and the thumbnail cache | "Previews and thumbnails" |
| console, log, diagnostics | "Console and log panels" |
| the authoring-document migration UI | "Migration panel" |
| `game.json` | "Project settings panel" |
| creating a game | "New project wizard" |
| bindings | "Input bindings editor" |
| restoring after a crash | "Recovery dialog" |
| git | "Source control panel" |
| per-user settings | "Preferences panel" |
| the on-screen profiler in the editor | "Profiler integration" |

## Panel framework

### Panel interface

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Shell/Zenith_EditorPanel.h
/// @contract interface Zenith_EditorPanel
/// Ownership: Created by its extension's factory as a Zenith_Owned, owned by Zenith_EditorPanelHost while open.
/// Thread affinity: MAIN_THREAD. Draw runs in the editor frame's step 2, inside an ImGui window the host opened.
/// Invalidation: None.
/// Units: Pixels of the shell window.
/// Errors: Draw cannot fail; a failed command it executes is shown as a notification by the history.
/// Reentrancy: ALLOWED(Close). A panel may close itself from Draw; it never draws another panel.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorPanel
{
public:
	virtual ~Zenith_EditorPanel() = default;
	virtual Zenith_EditorPanelTypeID GetType() const = 0;          // Zenith_Hash64 of its stable name
	virtual Zenith_StringView GetTitle() const = 0;                // window title, may carry the document name
	virtual void Draw(Zenith_EditorPanelContext& xContext) = 0;                  // ImGui calls between Begin and End
	virtual void OnSelectionChanged(const Zenith_EditorSelection& xSelection) = 0;
	virtual bool WantsKeyboard() const = 0;                        // owns shortcuts while focused
	virtual void SaveState(Zenith_JsonWriter& xWriter) const = 0;                // the per-user panel state (scroll, filters)
	virtual void LoadState(const Zenith_JsonDocument& xDocument, Zenith_JsonNode xState) = 0;
};
```

`Zenith_EditorPanelContext` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Panel context header") gives the panel the session (documents, history, selection, worlds, play controller, command registry), the shell (notifications, texture table, picker), the cook client, the preview host, the services and its rectangle, for the duration of the call. A panel reads documents and world inspectors and executes commands; it never mutates a document or a world directly (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Document identity and ownership"), and it keeps no reference the context returned across frames, so a runtime restart (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart") invalidates nothing a panel holds.

### Panel host

- `Zenith_EditorPanelHost` keeps the open panels in docking order, opens each panel's ImGui window (`ImGui::Begin` with the panel's stable window ID `"<title>###<type>:<instance>"`, so a title change keeps the docked position), calls `Draw`, and closes panels whose window closed.
- A panel type is single-instance (hierarchy, inspector, asset browser, console, log, diagnostics, migration, project settings, preferences) or multi-instance (scene view, game view, previews, document editors: one panel per open document).
- Panel draw time is measured per panel in the zone `Editor.Panel.<type>`; a panel whose draw exceeds `editor.panel.slowDrawMs` (default 8 ms) for 30 consecutive frames is named once in the log.

### Extension interface

<!-- elaboration -->
The editor's extension API is static (Z2-D-024: no dynamic plugins, and modding is excluded, Z2-D-075): an extension is C++ compiled into `ZenithEditor` under `Internal/<Area>/` and listed in the extension table. It registers, at shell creation:

```cpp
// Zenith2/Tools/ZenithEditor/Internal/Extensions/Zenith_EditorExtension.h
/// @contract interface Zenith_EditorExtension
/// Ownership: A constexpr-constructed object per extension in Internal/Extensions/Zenith_EditorExtensionTable.cpp; never freed.
/// Thread affinity: MAIN_THREAD. Register runs once, at startup step 12.
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns Zenith_EditorError; a failure stops startup with exit code 3 naming the extension.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorExtension
{
public:
	virtual ~Zenith_EditorExtension() = default;
	virtual const char* GetName() const = 0;                 // "Animation", "Audio", "Terrain", …
	virtual Zenith_Result<void, Zenith_EditorError> Register(Zenith_EditorExtensionContext& xContext) = 0;
};

class Zenith_EditorExtensionContext
{
public:
	void RegisterPanelType(const Zenith_EditorPanelTypeDesc& xDesc);           // factory, single or multi, menu path
	void RegisterCommand(const Zenith_EditorCommand& xCommand, const Zenith_EditorCommandDesc& xDesc);
	void RegisterComponentInspector(Zenith_TypeID xComponent, Zenith_PfnEditorInspector pfnDraw);   // replaces the generic one
	void RegisterGizmoProvider(Zenith_TypeID xComponent, const Zenith_EditorGizmoProvider& xProvider);
	void RegisterBoundsProvider(Zenith_TypeID xComponent, Zenith_PfnEditorBounds pfnBounds);
	void RegisterAssetKindHandler(Zenith_AssetKind eKind, const Zenith_EditorAssetKindHandler& xHandler);   // open, create, thumbnail
	void RegisterDocumentHandler(Zenith_TypeID xDocumentType, const Zenith_EditorDocumentHandler& xHandler);   // panel to open, validator, projection
	void RegisterPreferences(const Zenith_EditorPreferenceDesc* axDescs, uint32 uCount);
	void RegisterAutomationStep(const Zenith_EditorAutomationStepDesc& xDesc);   // Automation.md "Step kinds"
};
```

- The registration types (`Zenith_EditorPanelTypeDesc`, `Zenith_PfnEditorInspector`, `Zenith_EditorGizmoProvider`, `Zenith_PfnEditorBounds`, `Zenith_EditorAssetKindHandler`, `Zenith_EditorDocumentHandler`, `Zenith_EditorPreferenceDesc`, `Zenith_EditorAutomationStepDesc`) are declared in `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Extension registration types" and "Panel context header".
- Every registration is checked: a duplicate type, command ID, component inspector or handler is `EDITOR_ERROR_EXTENSION_DUPLICATE`; more than 256 panel types or 4,096 commands is `EDITOR_ERROR_EXTENSION_LIMIT`.
- A document handler's `m_pxProjection` is registered with the session (`Zenith_EditorSession::RegisterProjectionHandler`), which runs it after every forward and inverse change of a document of that type (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Projection per edit kind").
- Games extend the editor through data alone (they contain no C++, Z2-D-002): the `editor` object of a data-defined component's schema (`{ "hidden", "range", "step", "tooltip", "category" }`, `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Field keys") drives the generic inspector, and automation scripts drive commands.

### Extension table

| Extension | Directory | Specified in | Ticketed by | Stage |
|---|---|---|---|---|
| Core (hierarchy, inspector, asset browser, console, log, diagnostics, schema, migration, project settings, wizard, bindings, recovery, source control, preferences, the scene and game views) | `Internal/Panels/` | this file | EDIT | S3, S4 |
| Terrain and foliage tools, environment inspectors | `Internal/Terrain/`, `Internal/Environment/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain tools" | EDIT | S3, S4 |
| Lighting (component gizmos, probe capture, grading editor, bake panel) | `Internal/Lighting/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Lighting tools" | EDIT | S3, S4 |
| Materials (preview, parameter tuning) | `Internal/Materials/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Material tools" | EDIT | S3 |
| Animation (graph editor, blend space, mask, rig, clip timeline, retarget, preview, live debugger) | `Internal/Animation/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Animation tools" | EDIT | S4 |
| UI authoring | `Internal/UI/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "UI authoring tool" | EDIT | S4 |
| VFX inspector and preview | `Internal/VFX/` | `Zenith2/Docs/Design/VFX/Authoring.md` → "Effect inspector" and `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "VFX tools" | VFX for the effect-specific widgets; EDIT for the preview host | S5 |
| Audio (bank and event editor, mixer console, audition, voice inspector) | `Internal/Audio/` | `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Editor surfaces" | AUDIO, against this interface | S4 |
| Streaming panel, save and replay viewer | `Internal/Streaming/`, `Internal/Save/` | `Zenith2/Docs/Design/STREAM.md` → "Editor surfaces", `Zenith2/Docs/Design/SAVE.md` → "Editor surfaces" | STREAM, SAVE | S4 |
| Cinematics sequencer, 2D tools, character workflow, world partitions | `Internal/Cinematics/`, `Internal/2D/`, `Internal/Character/`, `Internal/World/` | `Zenith2/Docs/Design/CINE/Authoring.md` → "Sequencer panel" (row CN13); TWOD's, CHAR's and WORLD's designs | the owning areas, against this interface | S4 |
| Navigation (the bake panel, regeneration after `TERRAIN_REGENERATE_NAVIGATION` strokes, the AI debugger, link, obstacle, area-volume and volume-region gizmos, the kind-15 handler) | `Internal/Navigation/` | `Zenith2/Docs/Design/AI.md` → "Editor surfaces" | AI, against this interface (row AI12) | S4 |
| Packaging (the recipe editor over `package.recipe.json`, `size-report.json` and `partition.json` read-only) | `Internal/Package/` | `Zenith2/Docs/Design/PACK.md` → "Editor surfaces" | PACK, against this interface (row PK9) | S4 |
| Content (the import preset picker, the product-override editor, batch progress and reports, the merge driver's output) | `Internal/Content/` | `Zenith2/Docs/Design/CONTENT.md` → "Editor surfaces CONTENT supplies" | CONTENT, against this interface (row CT18) | S4 |
| Physics ("Drop to rest", the collider gizmos, the ragdoll preview) | `Internal/Physics/` | `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Physics tools"; `Zenith2/Docs/Design/PHYS.md` → "Editor surfaces" | EDIT for "Drop to rest" and the gizmos; PHYS for the ragdoll preview panel | S4 |

### Area-owned panels

A panel ticketed by another area (the "Ticketed by" column) is implemented in its `Internal/<Area>/` directory against `Zenith_EditorExtension`, uses the reusable widgets of `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Reusable authoring widgets", changes documents only through commands, and carries its tests in `Tools/ZenithEditor/Tests/<Area>/`; EDIT's review of such a diff checks the extension contract alone. The cross-area row ED15 records the split.

## Toolbar

| Control | Command | State shown |
|---|---|---|
| Play / Stop, Pause, Step (with a step count field, default 1) | `play.toggle`, `play.pause`, `play.step` | the play state of the focused play world; "n worlds" when several play |
| New play world | `play.startAnother` | disabled at `editor.play.maxWorlds` |
| Gizmo mode | `gizmo.translate`, `.rotate`, `.scale`, `.combined` | |
| Space | `gizmo.toggleSpace` | world, local, parent |
| Pivot | `gizmo.pivot.active`, `.centre`, `.individual` | |
| Snap toggles and increments | `snap.translate.toggle`, `snap.rotate.toggle`, `snap.scale.toggle`, `snap.surface.toggle`, increment fields | |
| Animate edit world | `editor.animateEditWorld.toggle` | adds or removes `WORLD_PAUSE_REASON_EDITOR` on the edit world |
| Camera scheme | `camera.scheme.unity`, `.unreal` | |
| Profiler | `devtools.toggleProfiler` | DEVT's overlay state |

## Hierarchy panel

### Hierarchy data source

- The tree is the active scene set's documents: one root row per scene document (or the prefab document in a prefab stage), then each document's roots and children in sibling order from `Zenith_SceneDocument::ListChildrenInSiblingOrder` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header"), never from the edit world, so it shows exactly what will be saved.
- Prefab instances show as one row with the instance icon, expandable into their resolved entities (from the prefab provider's resolved data), which are shown in italics and cannot be reparented out of the instance.
- Rows are built lazily: only expanded nodes are listed, 256 children per `ListChildrenInSiblingOrder` call, and the visible range is drawn with ImGui's list clipper, so a 100,000-entity scene scrolls at frame rate.
- Each row shows the name, the icon of its most significant component (camera, light, mesh, audio, UI, script), and markers: dirty overrides (instance rows), missing prefab, unknown component, read-only.
- During play the panel shows the focused play world's live entities in a second tab ("Play"), from the play world's inspector (`GetParent`, `ListEntities`), with runtime-spawned entities marked.

### Hierarchy operations

| Operation | Command and edits |
|---|---|
| create empty, create from template (camera, light kinds, mesh, audio emitter) | `entity.create`: `SCENE_EDIT_CREATE_ENTITY` + `ADD_COMPONENT` edits, a fresh authoring ID from the session's random source (`Zenith_EditorSession::GenerateAuthoringID`, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Session random source"), or the `id` argument when given, sort key after the active row's last child |
| rename (F2 or double-click) | `entity.rename`: `SCENE_EDIT_RENAME`; typing is an interaction, one record |
| drag to reparent or reorder | `entity.reparent`: per dragged root, `SCENE_EDIT_SET_PARENT` with a sort key from `Zenith_MakeSortKeyBetween` of the drop position's neighbours, with the rebalance rule (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Sort keys for edits") when needed; the world transform is kept by default (the local transform is recomputed against the new parent and written as `SET_PROPERTY` edits in the same record), or the local transform kept with `Alt` |
| delete | `edit.delete`: `SCENE_EDIT_DELETE_ENTITY` per selected root, or `SCENE_EDIT_DELETE_INSTANCE` |
| create prefab from selection | `prefab.createFromSelection` (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Prefab editing") |
| unpack prefab instance | `prefab.unpack`: `Zenith_UnpackPrefabInstance` (its inverse is recorded) |
| hide, lock (session flags, not document data) | `entity.hide`, `entity.lock`, stored in the session file |
| search | a filter by name substring, component stable name (`t:Zenith.Render.Camera`) or tag; matching rows keep their ancestors visible |

A drop that would make an entity its own ancestor, move an entity across documents (use cut and paste), or reparent inside a prefab instance is refused before any edit with the reason shown at the cursor.

## Inspector panel

### Inspector data source

<!-- elaboration -->
- In edit mode the inspector shows the selected objects' document values: a scene entity's component objects from the document tree (`Zenith_SceneEntityView::m_xComponents`), each read into a field set with `Zenith_ReadFieldSetJson` and walked with the type's reflection (`Zenith2/Docs/Design/ECS.md` → "Inspector data": property names, value types, ranges, tooltips, categories, hidden flags), with no branch on whether the type is C++-registered or data-defined ([Z2-N-ARCH-592], [Z2-N-ARCH-643]).
- For an entity of a prefab instance it shows the resolved value with the override state per property (§ "Prefab override display").
- The provenance line under each component names the type's source (`m_szSourcePath` of its `Zenith_TypeInfo`: the C++ library or the `*.schema.json` file).
- Hidden properties (`PROPERTY_FLAG_EDITOR_HIDDEN`, or `"hidden": true` in the schema) are skipped unless the preference `editor.inspector.showHidden` is on.

### Property editors

| Value type (reflection) | Editor | Edit produced |
|---|---|---|
| bool | checkbox | `SET_PROPERTY` |
| integer kinds | drag field with the declared range and step; typed entry | `SET_PROPERTY`, clamped to the range |
| float, double | drag field (speed `step` or 0.01 × range), typed entry with unit suffix | `SET_PROPERTY`, clamped; a non-finite typed value is refused |
| vec2, vec3, vec4 | one drag field per component, a lock toggle for uniform scale | one `SET_PROPERTY` of the whole vector |
| quaternion | Euler degrees (the CORE convention) shown and edited, stored as a normalised quaternion | `SET_PROPERTY` |
| colour | a colour picker (sRGB display, linear stored per the schema's colour kind) | `SET_PROPERTY` |
| enum | a combo of the declared names | `SET_PROPERTY` |
| string (interned) | text field | `SET_PROPERTY`; typing is an interaction |
| persistent ID, entity reference | a picker of entities of the document (drag a hierarchy row onto it, or the eyedropper which picks in a viewport) | `SET_PROPERTY` of the referenced authoring ID |
| asset handle | an asset picker filtered by the declared asset kind (drag from the asset browser) | `SET_PROPERTY` of the asset GUID |
| fixed array | one row per element | `SET_PROPERTY` per element |
| container | a list with add, remove and reorder | `SET_PROPERTY` of the whole container value |
| nested struct | a collapsible group | `SET_PROPERTY` of the field path |

Every editor writes into a gathered copy of the value and executes `component.setProperty` with the difference (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectionWalk header": inspector edits are never written into live storage); dragging a field is one interaction with merge key (command, document, entity, type, property path). Adding a component (`component.add`, a searchable list of every registered component type, filtered to those the entity does not have) and removing one (`component.remove`) are `ADD_COMPONENT` and `REMOVE_COMPONENT` edits with the type's default value from its reflection.

### Multi-object editing

With several entities selected, the inspector shows the components every selected entity has; a property whose values differ shows a mixed marker ("—"); an edit sets the value on every selected entity in one transaction, labelled "Set <property> on n entities". Components only some entities have are listed under "Not shared (n)" and are not editable together.

### Prefab override display

- A property overridden on an instance entity shows bold with a blue margin bar; its context menu offers "Revert" (`prefab.revertOverride`: `SCENE_EDIT_REVERT_OVERRIDE`) and "Apply to prefab" (`prefab.applyOverrides`: `SCENE_EDIT_APPLY_OVERRIDES_TO_PREFAB` over the instanced prefab's own writable document, the two-document edit of cross-area row S20).
- An instance row's inspector header shows the prefab, its variant chain, "Open prefab" (enters the prefab stage) and "Revert all" and "Apply all".
- A dangling override (its target no longer exists in the prefab, `DOCUMENT_WARNING_DANGLING_OVERRIDE`) is listed under "Dangling overrides" with "Remove" (an explicit edit) and is otherwise kept ([Z2-N-CONTRACT-547]).

### Unknown and newer data

- A component whose stable name the type registry does not know is shown as "Preserved unknown data: <stable name>" with its JSON read-only; it is kept on save (`Zenith2/Docs/Design/SCENE/Loading.md` → "Missing-type handling at load", editor row) and can only be removed (`component.remove`, which removes the object from the document).
- Unknown members of a known component (`DOCUMENT_WARNING_UNKNOWN_FIELD`) are listed read-only under "Unknown fields (kept)".
- A document opened read-only because a newer tool wrote it shows the banner of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Read-only and newer documents" and disables every editor.

### Play-mode inspection

During play the inspector shows the focused play world's values read with `Zenith_WorldInspector::ReadComponent` once per editor frame (between frames, out of tick), with a play-mode tint; edits go to the play world (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Play-world edits"); "Apply to document" copies them (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Applying play values to documents").

## Schema panel

### Schema panel workflow

The schema panel lists the game's `Schemas/*.schema.json` files with each type's stable name, version, field count and validation state, and drives the re-registration flow of `Zenith2/Docs/Design/ECS/Schemas.md` → "Editor re-registration" (row E11, A5 row EB13). The panel is the ImGui view; the flow is `Zenith_EditorSchemaModel` (`ZenithEditorCore`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Schema model header"), so ECS's integration suite drives the same code without the shell (ruling Q86 (a), row ED27):

1. Editing: the schema file is a JSON document opened in the JSON editor panel (or edited outside the editor; the game root's host watcher reports it, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers", and § "External changes" of the same file reloads or conflicts its open document). It is a target of the apply's batch like every migrated document (steps 3 and 4, and `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo"). On every change `Validate` checks the bytes into a scratch `Zenith_TypeRegistry` and the panel shows the validator's exact messages with positions (`ListDiagnostics`); nothing is applied (step 1 of ECS's flow).
2. Preview: "Preview migration" (`PreviewMigration`) runs `PlanMigration` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header" is the document side; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Planning a migration") over every open document and every document on disk holding the component (from SCENE's reference index), with the scratch registry as the target types; the migration panel shows the plan (§ "Migration panel").
3. Apply ("Apply schema change", command `schema.apply`, `Apply`): refused while any play world exists (`EDITOR_ERROR_SCHEMA_PLAY_ACTIVE`, [Z2-N-ARCH-648]) or a record is pending, and refused before anything is written when an open document of the plan is dirty, conflicted or missing (`EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN`, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes"); the schema file's own open document is checked too, but only for CONFLICTED or MISSING (refused the same way): a dirty one is the apply's source, its in-memory bytes are what is validated, applied and saved, and it is CLEAN after the write; otherwise: save the schema file (when its document is open, through the per-document rule: `NoteEditorWrite` of its new digest, the write, `ReplaceState` with the record's "after" state ID, so the record holds its "before" and "after" state IDs and its earlier JSON edits stay valid); `NoteEditorWrite` of each open document's planned digest, then `ApplyPlan` of the documents' plan (backup and journaled write), so the cook server's `changed` notifications and the host watchers' records for these writes, arriving in following frames, neither reload nor conflict the open documents; `Zenith_RuntimeToolAccess::QueueSchemaJson` with the file's bytes; the record is pending (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes") until, in the next frame's prologue step 6, the runtime opens the window, registers and ends it, and `OnSchemaWindowResult` reports success or the first refusal; the hook only records the result (no tool-access call is legal inside prologue step 6, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook"), and `Zenith_EditorSchemaModel::Poll` acts on it in the next frame's editor step 2.
4. In that `Poll`, on success: `Zenith_WorldSimulation::ApplySchemaRevision` on every edit world (out of tick, in editor step 2, which is ECS's "at the next frame") (ECS's entry point, A5 row EB13; the edit-world flag of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world rules"; both arrive with Z2T-ECS-099, whose same-diff update adds them to `Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world"), preview worlds destroyed in that frame's `OnFrameEnd` and recreated when their panels next draw; open scene documents take their migrated bytes (`ReplaceState` with the digest already noted) and are reconciled; the schedule recompiles (the runtime's trigger); the record commits. On refusal: the window was abandoned and the previous revision stays; the documents' batch is restored from its backup (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo" uses the same restore), the record is discarded, and the error is shown.
5. Lock: "Commit schema lock" (`schema.commitLock`, `CommitLock`) rewrites `schemas.lock.json` (ECS's step 5) by running ECS's verb as ZenithCook's schema-lock mode, `ZenithCook.exe --game <Name> --schema-lock` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Options"; `Zenith2/Docs/Design/ECS/Schemas.md` → "Cook integration"; row EB12) as a child process through `Zenith_CreateEditorChildProcess`, from `<m_strHostToolsDir>`, with `--game-root` when the session's games root is not the default. It is not a cook-server request: ASSET's request table has none for it. The verb writes one source file, so it takes the game root's storage writer lock for that write (SCENE's "Storage-root writer lock", per request, ruling 101) and no output-tree lock, and therefore runs while the cook server holds the output lock. The editor waits for the child in step 2 polls (not blocking the frame): exit code 0 → the panel shows the new lock version; exit code 6 (`COOK_ERROR_SOURCE_LOCKED`) → "another tool is writing the game directory; retry", nothing changed; any other code → the child's stderr lines in the diagnostics panel. It is a separate command so a work-in-progress schema never moves the lock, and it is not a history record (the lock is committed state ECS's validation compares against, never undone by the editor).

The whole apply is one history record of kind `EDITOR_CHANGE_SCHEMA`, undoable (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo"). `Test_EditorSchemaPanel_ApplyMigratesDocumentsAndWorld` drives it end to end; ECS's integration test `Test_EditorSchema_ChangeMigratesEditWorld` (in the suite `Zenith2/Tests/EditorSchema/`, `zenith_add_integration_tests(EditorSchema DEPENDS ZenithRuntime ZenithEditorCore ZenithECS_Custom ZenithECS_Flecs ZenithECS_EnTT SUPPORTED ZENITH_TOOLS_HOST)`, A5 ticket Z2T-ECS-100) covers the backend side by driving `Zenith_EditorSchemaModel` over a document-and-world session the suite creates.

## Asset browser

### Asset browser data source

<!-- elaboration -->
The asset browser reads the game's asset database through the cook server alone (the editor links no cook code; `Zenith2/Docs/Design/ASSET.md` → "Editor surfaces ASSET exposes", `Zenith2/Docs/Design/ASSET/Formats.md` → "Requests"):

| View | Request | Refresh |
|---|---|---|
| folder tree and listing | `list` with the folder's logical-path prefix, all kinds and states, paged 256 per page | on open, on `published` and `changed` notifications touching the folder, on F5 |
| search | `find` with the typed text (debounced 200 ms), limit 1,000 | as typed |
| a record's details | `record` (settings, sub-assets, importer, labels, the `.meta` digest) | on selection |
| references and referenced-by | `dependencies`, `dependents` (edge kinds selectable, transitive optional) | on demand |
| provenance | `provenance` with depth 8 | on demand |
| validation | `validate` for the selection or `all` | on demand; findings in the diagnostics panel |

The browser keeps no database of its own: its model is the last pages the server returned, keyed by request and invalidated by notifications, at most `editor.assetBrowser.cachedPages` (default 512) pages, LRU.

### Asset browser views

- Grid view (thumbnails, `Zenith2/Docs/Design/EDIT/Panels.md` → "Thumbnail generation") and list view (name, kind, state, last error, source path, size); sort by name, kind, state or modification; filter by kind, state (`CURRENT`, `DIRTY`, `FAILED`, …), label.
- Asset state badges: a failed import or cook shows the error code from the record summary's `lastError`, with "Reimport" and "Show diagnostics".
- Double-click opens the asset kind's handler (`RegisterAssetKindHandler`): a scene opens in the edit world; a prefab enters the prefab stage; a material, animation document, UI document, bank, terrain or environment document opens its editor panel; a texture or mesh opens a preview panel; a script opens in the external text editor (the preference `editor.externalEditor`, default `code -g %file%:%line%`).

### Import and drag and drop

1. A file dropped on the browser (`PLATFORM_EVENT_DROP_FILE`, `Zenith2/Docs/Design/PLAT.md` → "Editor surfaces PLAT exposes") or chosen with "Import…" is copied into the folder's source directory under `Games/<Name>/Assets/` (a name clash appends ` (2)`, ` (3)`), never moved from its original location.
2. `assignGuids` for the copied paths; the result's `created` `.meta` files and GUIDs are shown; `suggestions` (probable moves whose orphaned `.meta` matches) are offered for confirmation ("This looks like `<old>` moved; keep its GUID?"), and a confirmed suggestion is a `setSettings`-free move handled as § "Asset operations" of `Zenith2/Docs/Design/EDIT/Pipeline.md`.
3. `import` of the new GUIDs, with progress notifications in the status bar; failures listed with their codes.
4. The import is one history record (`EDITOR_CHANGE_FILE_OPERATION`, create), asynchronous while its `assignGuids` and `import` run (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes": a failure moves the copied file and its `.meta` into the editor trash and records nothing): undo deletes the copied file and its `.meta` (moved to the editor trash, `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Asset operations"), and redo moves both back from the trash directory the record names, so the asset keeps the GUID it was first given.

### Asset inspector

Selecting an asset shows its record and an importer-settings form generated from the importer's settings schema (the `.meta`'s `settings` object and per-platform `platforms` objects, `Zenith2/Docs/Design/ASSET/Formats.md` → "Meta sidecar schema"), with each importer's fragment editors (COMPRESS's codec dropdown, level field and dictionary picker, validated with `Zenith_ValidateCompressionSetting`, `Zenith2/Docs/Design/COMPRESS.md` → "Editor surfaces"). "Apply" sends `setSettings` with the record's `metaSha256` as `expectedMetaSha256`; a `COOK_ERROR_META_CHANGED` refreshes the record and shows "The settings changed on disk; review and apply again". The apply is one asynchronous record of kind `EDITOR_CHANGE_ASSET_SETTINGS` (pending until the `setSettings` result); a reimport follows automatically (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reimport") and is not part of the record, and it may rewrite the `.meta` again (its `subAssets` when the product set changed, `Zenith2/Docs/Design/ASSET/Import.md` step 5), so the digest the apply returned is not the digest an undo meets. Each undo and redo therefore starts from a fresh `record`: when its `settings` and `platforms` objects equal the settings the last completed direction wrote (the settings comparison of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"), its `metaSha256` is the expected digest of the direction's `setSettings`, so an undo after a sub-asset rewrite commits; when they differ, the settings were changed elsewhere and the undo is refused and fenced ("settings changed on disk; undo not possible") with nothing written.

For cooked kinds the inspector also shows the runtime side read from the edit world's asset registry: state, residency, resident bytes, and GEOM's mesh part and LOD information, texture residency and cook report members (`Zenith2/Docs/Design/GEOM.md` → "Editor surfaces").

### Dependencies and provenance

The dependency view draws the asset's `dependencies` and `dependents` as two lists with edge kinds; "Select referencing entities" selects the entities of open scenes referencing the asset (`Zenith_ReferenceIndex`). The provenance view sends `provenance` with depth 8 and lists the provenance records breadth-first, each with its inputs, hashes and tool versions; a record whose input is itself a provenance record is indented under it, and a depth limit reached is shown as "… (more)". `Test_EditorAssetBrowser_DependencyAndProvenanceViews` drives both views from the fake cook server's canned `dependencies`, `dependents` and `provenance` results and asserts the lists, the edge kinds, the breadth-first order and the depth marker.

## Previews and thumbnails

### Preview host

<!-- elaboration -->
`Zenith_EditorPreviewHost` owns the preview worlds (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Preview worlds") and the preview panels:

| Preview kind | Preview scene the host builds | Camera | Used by |
|---|---|---|---|
| mesh, prefab | the asset (an entity with the mesh renderer, or the resolved prefab), a key light, the preview environment (a neutral sky and a ground disc) | orbit, framed on the bounds | asset browser, prefab inspector |
| material | a sphere, a cube or a plane (selectable) with the material, the same light and environment | orbit | material editor (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Material tools") |
| VFX effect | an entity with `Zenith.VFX.Effect` of the effect, writes by command for play, stop and restart (row VX11) | orbit | VFX inspector |
| animated character | ANIM's `Zenith_AnimationPreview` drives the pose (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationPreview header"); the skinned mesh in the preview world | orbit | animation tools |
| texture | none: the texture drawn in ImGui through its texture reference, with mip, channel and exposure controls | 2D pan and zoom | asset browser |

- The host issues one `VIEW_KIND_PREVIEW` request per visible preview panel (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views") rendered by RPROD's preview path with LIGHT's preview lighting (`Zenith2/Docs/Design/LIGHT.md` → "Editor surfaces"). The preview path is the Forward path ([Z2-N-REND-254]), which lands at Stage 5 (Z2T-RPROD-112); ruling Q83 (a) (orchestrator, 2026-10-09) has `Flux_SelectShadingPath` return Forward+ for desktop preview and capture views until that ticket lands, so EDIT's Stage 3 previews, material preview and thumbnails draw on Forward+ from Stage 3 and switch to Forward with Z2T-RPROD-112's diff, with no change in EDIT (the view kind is the same).
- A preview world ticks only while its panel is visible; the host destroys the least recently shown preview world when `editor.preview.maxWorlds` is exceeded.
- Each preview panel reports the preview's GPU time from the view's pass timings in its footer.

### Material preview

The material preview renders the material through its runtime resource; parameter edits in the material editor are recorded as JSON edits of the material document, and the Materials extension registers a `Zenith_EditorProjectionHandler` for the material document type (`*.material.json`, `Zenith2/Docs/Design/GEOM/Materials.md` → "Material document schema"). The handler runs after every forward and every inverse change of such a document: it packs the document's current `parameters` into the material's constant block by the material's reflection (`Zenith2/Docs/Design/GEOM/Materials.md` → "Material reflection input") and applies it with `Flux_MaterialResource::ReplaceParameters` (`Zenith2/Docs/Design/GEOM.md` → "Editor surfaces"), so an edit, its undo and its redo each leave the runtime resource at the document's value. The packing is the pure function `Zenith_EditorPackMaterialParameters(const Zenith_JsonDocument& xDocument, const Zenith_JsonDocument& xReflection, Zenith_Array<uint8>& axOut)` (`Internal/Materials/Zenith_EditorMaterialPacking.h`: each parameter of the reflection's `parameters` array written at its `offset` as its `kind`'s little-endian bytes, texture fields left zero, which `ReplaceParameters` ignores). The preview keys (`preview` members of the material document: shape, environment, exposure) are read from the document itself, since GEOM exposes none (A5 row GB24). Two tests, neither doubling `Flux_MaterialResource` (GEOM's concrete resource class, which has no seam; ruling FINAL-DOUBLES): `Test_EditorMaterial_PackingMatchesReflection` packs a fixture document against a fixture reflection and compares with bytes written by hand from the reflection's offsets and kinds; `Test_EditorMaterial_UndoRestoresLiveParameters` (`editor-render`, WARP and lavapipe) renders the material preview of a fixture material whose `baseColour` is edited from (1, 0, 0) to (0, 0, 1), reads the preview image back after the edit, the undo and the redo, and asserts that the edited image differs from the original (the centre pixel's blue channel exceeds its red), that the image after the undo equals the original byte for byte, and that the image after the redo equals the edited one (in-run comparisons, no golden).

### Thumbnail generation

- An asset tile without a current thumbnail queues a thumbnail job; the host renders at most `editor.thumbnails.perFrame` (default 2) per frame, each as one `VIEW_KIND_PREVIEW` view (Forward+ until Z2T-RPROD-112, ruling Q83) of 128 × 128 pixels (256 × 256 at display scale 2) into a tool texture, then reads it back with an RHI readback operation (`Zenith2/Docs/Rendering.md` → "GPU lifetime and synchronisation": readbacks are operation handles resolved after fence completion), encodes it and stores it in the thumbnail cache.
- The thumbnail scene is the preview scene of the kind, rendered after `editor.thumbnails.warmupFrames` (default 4) frames so streaming and exposure have settled; kinds without a 3D preview get a vector icon of the kind.
- A thumbnail is current when its cache key equals `Zenith_Hash64` over (the record's `importKey`, the kind, the thumbnail format version); a reimport therefore invalidates it.
- Headless: no thumbnail is rendered; tiles show the kind icon.

### Thumbnail cache

The cache is the directory `${ZENITH_OUT}/EditorCache/<Name>/Thumbnails/` (never tracked, never under `Games/`), one file per thumbnail in the format of `Zenith2/Docs/Design/EDIT/Formats.md` → "Thumbnail file", with an in-memory LRU of decoded tool textures within the editor-caches budget. The memory governor's shedding of `MEMORY_SUBSYSTEM_EDITOR_CACHES` (`Zenith2/Docs/Architecture.md` → "Memory governor": warning level purges thumbnail and preview caches to 50% least recently used first; critical purges them entirely and drops in-memory derived-data caches) is implemented by `Zenith_EditorCacheConsumer` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Cache consumer header"), the editor's `Zenith_MemoryConsumer`, which frees pick-mesh cache entries at once and marks decoded thumbnails and idle preview worlds in LRU order for the preview host to release in the same frame's step 6 (the governor's callback runs in prologue step 4, where no world may be destroyed and the frame's draw snapshot may still name a thumbnail, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook"); the on-disk cache is untouched by shedding and is pruned to `editor.thumbnails.diskMaxBytes` (default 512 MiB) at startup, oldest first.

The shedding case of `MEMORY_SUBSYSTEM_EDITOR_CACHES` is `Test_EditorCacheConsumer_ShedsAtWarningAndCritical` in `ZenithEditor_Tests` (ruling Q86 (b), CORE row CB8): `Zenith2/Tests/Memory` boots a Player composition, which registers no editor consumer, so CORE's case table `axMEMORY_SHEDDING_CASES` records the subsystem as tool-only, tested in `ZenithEditor_Tests`, and its `Test_MemoryGovernor_EveryRegisteredSubsystemHasCase` never sees the editor's consumer (row ED28).

## Console and log panels

### Console

The console panel hosts DEVT's development console (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevConsole header": lines queued with `QueueLine` and run by `ExecuteQueued`) and the script console:

- a line starting `>` is Luau, evaluated with `Zenith_ScriptWorld::ConsoleEvaluate` on the focused play world's script world (or the edit world's, where only side-effect-free expressions are meaningful because no behaviour runs there), out of tick, in the TOOLS context, its result text shown (`Zenith2/Docs/Design/SCRIPT.md` → "What SCRIPT authors and what EDIT owns"); the editor supplies the compile thunk `Zenith_ScriptRuntimeDesc::m_pfnCompileConsole` over `Zenith_ScriptCompileModule` (A5 row SB14);
- any other line is a console command line, with completion over DEVT's registered commands;
- history (up to 200 lines, per user) with up and down arrows.

### Log panel

The log panel reads the process log ring (CORE's log router and its ring sink, `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Structured logging"): severity, category, time, message, with filters by severity and category and a text search; at most `editor.log.maxLines` (default 20,000) lines are kept in the panel, oldest dropped. Double-clicking a line that names a document path and position opens the document at that position.

### Diagnostics panel

One list of every diagnostic the editor knows, each with severity, source, location and message, and a "Go to" action:

| Source | Origin |
|---|---|
| script | the play worlds' diagnostic stores (`GetDiagnostic`, `GetLatestDiagnosticSequence`) and the in-process compile reports (`Zenith_ScriptCompileReport`) |
| cook | the cook server's `diagnostic` notifications and `validate` findings |
| document | document warnings (`Zenith_DocumentWarning`: unknown fields, dangling overrides, not canonical, duplicate sort keys) and `Zenith_SceneDiagnostics` entries of opened scenes |
| shader | the shader reload journal's `error` records (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload") |
| schema | the schema validator's messages |

## Migration panel

The migration panel is the authoring-document migration UI of Z2-D-048 (inventory items Z2-I-EDIT-15 to -17): upgrade preview, backup and resume. Its data and flow are `Zenith_EditorMigrationModel` (`ZenithEditorCore`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Migration model header"; `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow"), the model SCENE's integration test drives without the shell (ruling Q86 (a), row ED26); its views:

| View | Content |
|---|---|
| Overview | per storage root, the number of documents needing migration by type and version (`from → to`), blocked entries with their reason, documents opened read-only as newer |
| Plan | per entry of a `Zenith_MigrationPlan` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_DocumentMigrator header"): path, type, versions, changed or unchanged, the diff hunks (three lines of context, with `+` and `-` lines coloured), the structural summary of changed document pointers, warnings (each unknown field kept, each dangling override) |
| Apply | the "Back up and migrate" button, the backup directory that will be written (`.migration/backups/<UTC timestamp>/`), progress, the result (written count, batch name) |
| Resume | the report of `RecoverPendingJournals` at startup (journals found, rolled back, rolled forward, entries restored) and any journal that could not be recovered, with its error and the instruction to resolve it |
| Backups | the backup batches under each root's `.migration/backups/`, with "Restore this batch" (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Restoring a backup") |

`.meta` documents are planned and applied through the cook server's `migratePreview` and `migrateApply` (the preview token makes the apply refuse a plan that changed), and shown in the same views.

## Project settings panel

The project settings panel edits `game.json` through MOD's scaffold library, never through an editor serializer (`Zenith2/Docs/Design/MOD.md` → "game.json authoring"): the module checklist with each module's dependencies explained, the ECS and physics backend pickers, the platform list, the content roots, the startup scene (an asset picker of scene documents), the script-recovery settings and the configuration layer keys. Every change validates through `ZenithNewGameScaffold`'s C++ validator and shows its exact messages; "Apply" writes the canonical bytes through the scaffold's canonical writer as a `Zenith_DocumentWrite` of the game root (journaled, one history record of kind `EDITOR_CHANGE_DOCUMENT_BYTES`) and shows "Module selection changed: rebuild the game (the next build reconfigures it) and restart the editor", because the running editor's composition was fixed at startup (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor composition").

## New project wizard

The wizard is EDIT's half of MOD's wizard boundary (`Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Editor wizard boundary", row M19, inventory item Z2-I-MOD-09):

1. Fields: name (validated live with MOD's name rules), template (Blank, 3D, 2D), the module checklist with dependencies explained, the ECS and physics backends, the platforms, the Android application ID when Android is selected.
2. On every field change: fill `Zenith_NewGameDesc`, call `Zenith_PlanNewGame`, show the plan's file list and every validation message verbatim.
3. Confirm: `Zenith_ApplyNewGamePlan`; show the report's messages verbatim; on success show "The next build configures Games/<Name> (CMake re-runs on the new CMakeLists.txt); build it, then open it" and offer "Open after build", which starts a new editor process with `--game <Name>` once `composition.json` of the new game exists.
4. The wizard adds no default, rule or file of its own, so its output is byte-identical to `ZenithNewGame create` with the same options; `Test_NewProjectWizard_MatchesCli` (`Tools/ZenithEditor/Tests/Zenith_NewProjectWizard_Tests.cpp`) asserts it for `Harbour --template 3D --platforms windows`.

Creating a game is not an undoable record (it writes a new directory outside the open game's storage); the wizard states this before confirming.

## Input bindings editor

The bindings editor edits the game's bindings documents (`Zenith2/Docs/Design/PLAT.md` → "Editor surfaces PLAT exposes": "Input bindings authoring"): the loaded binding table, the actions and contexts, each binding's controls and processors, the provenance of each binding layer (`GetBindingProvenance`), validation with PLAT's validator, and saving through the byte-stable `Zenith_WriteBindingsJson` as a document write (A5 row PB21, cross-area row P15). A "Test" mode shows the live presentation action states of the focused play world.

## Recovery dialog

At startup step 13 (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Startup sequence"), when autosaves of an unclean session exist, the dialog lists each document with: its path, the time of its autosave, whether the file on disk still has the autosave's base digest ("unchanged since the crash") or differs ("changed on disk since the crash"), and the actions "Restore" (default for unchanged), "Restore as copy" (the only restore offered for changed files: the autosave is written next to the document as `<name>.recovered.<ext>`), "Discard", and "Show diff". The algorithm is `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Recovery on startup".

## Source control panel

The panel shows the working-tree state of the game directory from the source-control backend (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Source control"): modified, added, deleted, untracked, conflicted, LFS pointer not fetched, LFS locked by another user; per file "Diff" (the document diff of the working bytes against `HEAD`), "Revert", "Stage", "Unstage", "Mark resolved", "Lock" and "Unlock" (LFS binaries), and "Commit" of the staged files with a message. The hierarchy, the asset browser and the document tabs show the same state as a badge.

## Preferences panel

Every per-user preference (camera scheme and speeds, gizmo size, snap defaults, autosave interval, shortcuts, the external editor command, theme, font scale, the play options) with its default, its current value and a reset button; extensions register their own preferences (`RegisterPreferences`). Preferences are stored in the per-user preferences file (`Zenith2/Docs/Design/EDIT/Formats.md` → "Preferences file") and are never part of a document or of the game directory.

## Profiler integration

The editor creates DEVT's `Zenith_ProfilerOverlay` over the editor process's profiler (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Editor overlay"): its draws appear in `VIEW_KIND_EDITOR` views only, the toolbar's Profiler toggle calls `CycleState`, `F3` maps to the same call through the editor's shortcut table, and the editor contributes the panel provider `editor` (the editor frame's step times and each panel's draw time from the `Editor.*` zones, `Zenith2/Docs/Design/EDIT.md` → "Diagnostics and profiling"). The in-editor profiler itself (inventory item Z2-I-DEVT-02) is DEVT's; EDIT owns the toggle, the shortcut and the provider.
