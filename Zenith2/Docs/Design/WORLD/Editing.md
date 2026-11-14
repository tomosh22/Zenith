# WORLD design: editing worlds and concurrent editing

Section file of `Zenith2/Docs/Design/WORLD.md`. It specifies WORLD's editor extension: the World panel, how region documents are projected into the edit world and loaded around the camera, the commands that create regions, move content between them and manage layers, the partition preview, the bake panel and its commands, play in the editor for a partitioned world, and the concurrent-editing granularity of a world: which file each edit touches, how independent edits merge, and what is re-derived after a merge. EDIT owns the editor shell, documents, commands, undo, the cook client and source control (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Documents", "Commands and history"); WORLD's code is an area extension against EDIT's interface (`Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels").

## Scope of this file

| Need | Heading |
|---|---|
| the extension and its registrations | "World editor extension" |
| the World panel | "World panel" |
| region documents in the edit world | "Region projection", "Region loading" |
| commands | "World commands", "Moving content between regions" |
| the partition preview | "Partition preview" |
| baking from the editor | "Bake panel and commands" |
| playing a world | "Play in the editor" |
| team editing | "Concurrent editing granularity", "Terrain editing granularity", "Merge conflicts in WORLD documents", "After a merge" |

## World editor extension

`Zenith_WorldEditorExtension` (`Zenith2/Tools/ZenithEditor/Internal/World/Zenith_WorldEditorExtension.cpp`), name `"World"`, one row of EDIT's extension table (`Internal/World/`, ticketed by WORLD, `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension table"). `Register` registers, at editor startup step 12:

| Registration | Item |
|---|---|
| panel types | `World` (single), `World Bakes` (single) |
| document handler | `Zenith.World.WorldDocument` → open the World panel; validator `Zenith_ValidateWorldDocument`; `Zenith.World.GeneratorDocument` → the JSON document editor with the generator's parameter validation; the light bake types → a read-only viewer that reads the file through the world's source access and never opens it as an editor document (so a bake's write never meets an open bake document, `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake execution") |
| commands | the table of "World commands" |
| automation steps | `world.bake`, `world.generate`, `world.partition.check` |
| preferences | `world.editor.loadRadiusRegions` (default 2, range 0 … 16), `world.editor.unloadRadiusRegions` (default 3, ≥ load radius + 1), `world.editor.previewGrid` (grid name, default the first gameplay grid) |
| debug-draw provider | the partition preview |

Tests are in `Zenith2/Tools/ZenithEditor/Tests/World/`; shared, non-ImGui code lives in ZenithAuthoring (`Zenith2/Tools/Authoring/Internal/WORLD/`), so the cook and the editor run the same partition, generator and fingerprint code (ruling Q84 (a), `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs").

**Background work.** Every WORLD computation longer than `editor.frame.syncBudgetUs` (4,000 µs) runs as an editor background operation (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor frame"), and every job those operations submit uses the tools-host priority `JOB_PRIORITY_BACKGROUND_TOOL` of [Z2-N-ARCH-443] (ruling X350 (a); declared by JOBS in `Zenith2/Docs/Design/JOBS/Algorithms.md` → "Tools-host background priority" and accepted by a job system created with `Zenith_JobSystemDesc::m_bAcceptToolBackground`), which exempts tool-host jobs from the 2 ms slice of [Z2-N-ARCH-393] and lets at most `m_uMaxConcurrentToolBackground` workers (default max(1, N / 2) of N) run them, so a minutes-long bake never takes the workers the editor's own frame graph runs on. None uses `RunRoot` (it would occupy the main thread) or a nested fork-join (`Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_JobSystem header"). The main thread runs no WORLD computation except the partition preview's per-frame assignment, bounded by `uWORLD_EDITOR_PREVIEW_ROOTS_PER_FRAME` ("Partition preview"); everything else on it is bookkeeping in O(documents or paths touched). There are three operations:

| Operation | Jobs (one at a time unless stated) | Main-thread work |
|---|---|---|
| `Zenith_WorldEditorPlanOperation` | (1) the **partition job**: `Zenith_PlanWorldPartition` over the run's `Zenith_WorldEditorSourceAccess` of the saved documents, with `m_bIncludeBakeFingerprints` false; (2) after the summary request, the **fingerprint job**: `Zenith_PlanWorldBakes` (every lighting fingerprint through `Zenith_ComputeWorldLightingFingerprint`, every baked generator region's fingerprint through `Zenith_PrepareWorldGenerator`), over the same access | starting the jobs; one asynchronous cook-server request for the LOD 0 summaries of the plan's meshes (below); the generation check and publication |
| `Zenith_WorldBakeOperation` | per unit: the prepare job, the unit's work items (up to `2 × GetMaxConcurrentToolBackground()` in flight), the finish job | submitting, polling, the per-unit generation check, one `SaveDocuments` batch per unit (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake execution") |
| `Zenith_WorldGenerateOperation` | one prepare job (`Zenith_PrepareWorldGenerator` over the pinned plan), then one job per selected region job (up to `2 × GetMaxConcurrentToolBackground()` in flight), each also writing its generated document's canonical bytes (`Zenith_WriteWorldGeneratedDocument`) and their SHA-256 | submitting, polling, then the synchronous record of `world.generate`, which applies the prepared bytes ("Bake panel and commands") |

A bake and a generation of one world are mutually exclusive: starting either while the other, or another of its own kind, runs is refused with "a bake or generation of <world> is running". At most one plan is therefore pinned at a time.

**Input-change generation.** The extension keeps, per active world, a `uint64` **input-change generation**, incremented by every trigger of step 1 below, and a map from storage path to the generation of that path's last change (the saved, externally changed or WORLD-written path). Each plan run records the generation at its start, `G0`; every document a run or a pinned operation reads goes through a `Zenith_WorldEditorSourceAccess` that records, at the first open of each storage path, `path → Zenith_OpenedDocument::GetFileDigest()` in the plan's **read table** (`Zenith_WorldPartitionPlan::GetReadDocuments`).

**The plan operation.** The extension keeps one `Zenith_WorldEditorPlanOperation` per active world; its completed **plan** (`Zenith_WorldEditorPlan`: the partition plan, its diagnostics, its read table and `G0`, the summary table and the bake plan, each in the plan's own arena) is what the World Bakes panel lists, what `world.partition.check` shows and what `world.bake.lighting` and `world.generate` pin.

1. **Trigger.** A refresh is requested, and the generation incremented with the changed paths recorded, when the world becomes active, on `EDITOR_EVENT_HISTORY_COMMITTED` of a record that saved a document of the world, on every save of one, on EDIT's external-change notification for a file under `Worlds/<W>/`, a prefab, a terrain document or tile, on every successful unit batch of a bake (its header and page paths), on every applied `world.generate` record (its documents' paths), and by `world.partition.check`. The last three are needed because a bake's and a generation's writes are editor-originated writes to unopened documents, which EDIT's external-change notification does not report. Requests coalesce: while a run is in flight, any number of requests set one flag, and one more run starts when it completes.
2. **Inputs.** A run reads the **saved** documents only: the access opens each document from disk through the storage roots' migrators on the job's thread and records its file digest in the read table; it never reads an editor document object, which is `MAIN_THREAD`, so the main thread copies nothing. A second open of a recorded path by the same run compares the file digest with the recorded one, and a mismatch is `WORLD_ERROR_SOURCE_ACCESS` with detail `DOCUMENT_ERROR_STALE_PLAN`, which discards the run. Unsaved edits are not in the plan, exactly as play in the editor uses the saved region documents ("Play in the editor"); the panel shows "plan of the saved documents (<n> unsaved)" from EDIT's document states.
3. **Summaries.** When the partition job completes, the main thread takes the plan's ascending list of meshes (`Zenith_WorldPartitionPlan::GetLightingMeshes`: every mesh a `Zenith.Render.MeshInstance` of a BAKED layer names, because which of them a unit's item 4 holds needs the summaries' spheres; an O(meshes) copy) and sends one cook-server request `property { guids, platform: "windows", name: "geom.lod0Summary" }` (row WD31 (c)), which cooks any mesh whose key misses and answers each mesh's published summary. Its answer fills the plan's summary table; `GetMeshLod0Summary` of the run's access answers from that table only, and a mesh missing from it (the cook failed) is `WORLD_ERROR_SOURCE_ACCESS`, which marks the units naming it FAILED in the plan. Then the fingerprint job runs.
4. **Publication.** On the main thread, after the fingerprint job: when a path in the run's read table has a last-change generation above `G0`, the run is discarded (it may have read one file of a multi-document save batch before the batch and another after it) and another run starts; otherwise the plan replaces the latest one. A plan is therefore published only after a quiet interval as long as one run for the paths it reads; while edits to those paths continue, the latest published plan stays listed with "plan out of date (edits in progress)". At most three plans are alive at once (`uWORLD_EDITOR_MAX_LIVE_PLANS`: the latest, one in computation and the one pinned by a running bake or generation), each released when its last user ends; a run that completes while three are alive keeps its plan unpublished until a pin is released, then publishes it if it is still current.

<!-- assumption: pending ASSET (row WD31 (c)) -->
The cook server answers the request `property { guids, platform, name }` with each item's published property bytes, cooking an item whose key misses into the DDC first, as row WD31 (c) asks; ASSET's request table has no such request today. Until it lands, the plan operation's summary step and the bake are `draft` WORLD tickets blocked by ASSET's patch.
<!-- /assumption -->

**Pinned access.** Every operation that pins a plan (a bake, a generation) reads through its own `Zenith_WorldEditorSourceAccess` in **pinned** mode over that plan's read table: an open of a recorded path compares its file digest with the recorded one, and a mismatch is `WORLD_ERROR_SOURCE_ACCESS` with detail `DOCUMENT_ERROR_STALE_PLAN` (the bake unit is FAILED with "input changed during bake; bake again"; the generation region is refused); an open of a path the plan never read (a terrain tile outside every gather rectangle that ENV's loader reads, a light page) records its digest in the operation's own table, against which its later opens compare. So a job may read only bytes the plan recorded, or bytes the operation itself recorded once.

**Starting a pinned operation.** `Start` of a bake and the `world.generate` record refuse when a document the plan read is DIRTY (O(open documents of the world)) or when a path of the plan's read table has a last-change generation above the plan's `G0` (O(paths read) map lookups), with "save <n> documents and wait for the plan first". No file is hashed on the main thread. **"Save and bake"** saves the dirty documents, notes the generation G_s after the last save, and starts on the first published plan whose `G0` ≥ G_s; changes after that plan's start are ignored by the start and make the units they reach STALE, as for any edit during a bake.

**Source access affinity.** Each operation creates its own `Zenith_WorldEditorSourceAccess`, and orders its jobs so that only one job uses that access at a time (a job is submitted only after the previous job of the operation has completed, the work items of a bake or generation, which read only immutable plans and never the access, excepted). The access's affinity is therefore "the one thread running the current call: the import thread in the cook, one job at a time in the editor, the test's thread in tests; never two threads at once" (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Source access interface").

**Job-entry observer.** Each operation's constructor takes an optional `Zenith_WorldEditorJobObserver*` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Editor declarations"; null in production): the operation calls `OnJobEntry(uFunctionID, Zenith_IsJobWorkerThread(), ePriority)` at the entry of each of the nine WORLD functions its jobs call, which is how `Test_WorldEditor_NoHeavyWorkOnMainThread` observes thread and priority without a global.

## World panel

- **Worlds.** Lists every `*.world.json` of the game; opening one makes it the session's **active world** (one at a time) and opens its world document as an EDIT JSON-kind document.
- **Layers.** One row per layer, name order: colour, kind and state, and three per-user toggles — visible (its region documents' entities hidden in viewports and picking when off), loaded (its region documents opened by "Region loading" when on) and locked (its region documents read-only for this user: every command touching them is refused with `EDITOR_ERROR_DOCUMENT_READ_ONLY`). The toggles' team defaults are the layer object's `editor` member; per-user overrides go into the session file (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Per-user state stays out of documents") and never into the world document (`Test_WorldEditor_LayerOverridesStayInSession`). The **current layer** (one, highlighted) receives new content.
- **Region map.** A top-down grid of the world's region coordinates: each cell shaded by whether the current layer has an authored region there, outlined when loaded, badged with the dirty, conflicted or source-control state of its document (EDIT's status, `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Status") and with the LFS lock owner of any terrain tile it holds ("Terrain editing granularity"); clicking a region frames it, double-clicking loads it.
- **Generators.** One row per generator reference, in generator-key order, showing its document's display `name`, generator ID and version, mode, layer and staleness from the latest plan; rename (`world.generator.rename`: one record writing the generator document's `name` member only, since nothing else names a generator by its name), upgrade and generate.
- **Layer edits.** Add, rename, change state or realisation, change `grid`, change `bakes`, delete (refused while the layer has regions): commands of "World commands", each one record touching the world document only. A rename moves the layer object to its new key in `layers` and changes nothing else: region keys, region paths, grid names and products derive from the layer `id` (`Zenith2/Docs/Design/WORLD/Documents.md` → "Layer objects"). Adding a grid-realised layer, or changing a layer to grid realisation, appends its `id` to `streaming.layerGridOrder` in the same record.

## Region projection

Region documents are SCENE documents whose root transforms are relative to the region anchor (`Zenith2/Docs/Design/WORLD/Documents.md` → "Region documents", R2). WORLD projects them through EDIT's scene projection with a per-document translation: the region anchor minus the edit world's current origin, computed in `double` and narrowed once.

<!-- assumption: pending EDIT (row WD24, refining WD15 (b)) -->
EDIT's session gains `Zenith_Result<void, Zenith_EditorError> SetDocumentProjectionAnchor(Zenith_EditorDocumentID, const double adAnchor[3])`: the projection adds `(float)(anchor − O)` (O the edit world's `Zenith.WorldOrigin`, `Zenith2/Docs/Design/STREAM/PublicAPI.md` → "SCENE world origin") to the root transforms of that document it projects, and subtracts it from root-transform edits it records, so a gizmo drag of a region root writes an anchor-relative value. The edit world's origin is its own `Zenith.WorldOrigin`, moved only through SCENE's shift (`Zenith_RecordShiftWorldOrigin`, the same mechanism and the same 2,048 m quantum STREAM uses), which the extension records when the camera's local position exceeds 2,048 m on an axis; the shift moves every projected document together (the persistent scene, which is world-absolute, ENV's terrain projection and every region), and each region's translation is recomputed from the new O, so terrain and regions stay aligned anywhere in a world of ±2^20 regions (`Test_WorldEditor_ProjectionOrigin`).
<!-- /assumption -->

## Region loading

Every editor frame (EDIT's frame step 2), `Zenith_WorldRegionLoader` (`Internal/World/Zenith_WorldRegionLoader.cpp`):

1. computes the region R under the active scene view's camera;
2. wants every region of every loaded layer within Chebyshev distance `world.editor.loadRadiusRegions` of R that the world document lists;
3. opens each wanted region that is not open with `Zenith_EditorSession::OpenScene(path, EDITOR_OPEN_MODE_ADDITIVE)` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Session object"), by its path (region documents are input-only: they have no asset GUID), nearest first, at most 4 per frame;
4. closes each open region beyond `world.editor.unloadRadiusRegions` that is CLEAN, farthest first, at most 4 per frame; a DIRTY, CONFLICTED or MIGRATED region stays open until saved;
5. keeps the open count at most `editor.documents.maxOpen` − 64 (EDIT's limit with headroom for other documents); when the wanted set exceeds it, the farthest wanted regions are not opened and the panel shows "load radius limited to <n>".

The persistent scene stays open while the world is active. "Load all" (a confirmation above 256 regions) opens every region of the loaded layers. Region loading is an editor-session concern only: it writes no document, records no history and never affects a cook.

## World commands

Every command is a `Zenith_EditorCommand` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command") registered by the extension; each document-changing command changes documents only through EDIT's changes, so every one is one undoable record. A command whose computation is long (`world.generate`, `world.generator.upgrade`) computes in a background operation first and then executes its record, synchronously, with the computed result.

| Command ID | Arguments | Changes |
|---|---|---|
| `world.create` | `name`, `regionSize`, `mode` | a new world directory: the world document, a persistent scene, one `Main` data layer (baked); through EDIT's create operation (asynchronous: `assignGuids` for the two assets) |
| `world.layer.add`, `.set`, `.rename`, `.delete` | layer name and members | the world document only ("World panel") |
| `world.streaming.renumberLayerGrids` | — | after a confirmation stating that every saved explicit `GridMask` keeps its bits, which then name other grids: sets `layerGridBase` to the base grid count and removes retired entries from `layerGridOrder` (W28) |
| `world.region.create` | layer, x, z | a new empty region document (`CreateEmpty` with a fresh ID, name the region key) at `Regions/<LayerKey>/<x>_<z>.scene.json`, created as an input-only document (no asset request, row WD24) in the same record as its region object |
| `world.region.delete` | layer, x, z | refused while the region holds entities; removes the document (EDIT's delete into the editor trash, with no asset request) and the region object |
| `world.entity.place` | prefab or entity data, world position | the current layer's region under the position (created first in the same record when absent): a `SCENE_EDIT_PLACE_INSTANCE` or `SCENE_EDIT_CREATE_SUBTREE` with the anchor-relative position |
| `world.entity.moveToRegion` | entity IDs | "Moving content between regions" |
| `world.entity.moveToLayer` | entity IDs, layer | as `moveToRegion`, the target being the same coordinate in another layer |
| `world.generator.rename` | generator key, new name | the generator document's `name` member only; no world document member, path, key, region, product or fingerprint changes |
| `world.generate` | generator, regions, `overwriteEdited`, `force` | `Zenith2/Docs/Design/WORLD/Generation.md` → "Generator outputs and staleness"; "Bake panel and commands" |
| `world.generate.detach` | generator, region | `Zenith2/Docs/Design/WORLD/Generation.md` → "Detaching an output" |
| `world.generator.upgrade` | generator, version | `Zenith2/Docs/Design/WORLD/Generation.md` → "Upgrading a generator version" |
| `world.bake.lighting` | `scope`, `force` | no command record: starts the `Zenith_WorldBakeOperation` ("Bake panel and commands") |
| `world.bake.navigation`, `world.bake.hlod` | `scope` | a `cook` request through the cook client for the scope's tile or HLOD products; no document change |
| `world.partition.check` | — | requests a refresh of the plan operation and shows the diagnostics of the plan it completes (the whole partition of the saved documents, reader and importer rules); no document change |

### Moving content between regions

A gizmo move, a paste or a nudge that leaves a root entity or instance outside its region document's rectangle (R3) is followed, in the same record, by `world.entity.moveToRegion` for that root: the root's subtree is removed from the source document (`SCENE_EDIT_DELETE_ENTITY` or `SCENE_EDIT_DELETE_INSTANCE`) and created in the target region's document (`SCENE_EDIT_CREATE_SUBTREE` or `SCENE_EDIT_PLACE_INSTANCE`) with the same IDs, the root transform rewritten relative to the target anchor and a sort key appended last among the target's roots (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Sort keys for edits"); the target region is created first when absent. One record holds both documents, so undo restores the root in its old document at its old sort key. IDs never change across a move (SCENE's identity rule 2 forbids reuse within one document only, and a move removes the ID from the source in the same record), so references, saves and baked fingerprints follow the entity. A move of more than 16 documents' worth of roots is one record listing every document (`uEDITOR_RECORD_MAX_DOCUMENTS` keeps the first 16 in the info, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Command records").

<!-- assumption: pending SCENE (row WD6 (b)) -->
Removing an ID from one document and inserting it into another inside one record is accepted by SCENE's retired-ID rule: `Zenith_SceneDocument`'s retired set refuses a re-insertion into the same document only, so the target document accepts the moved IDs.
<!-- /assumption -->

## Partition preview

With the preview enabled, the extension's debug-draw provider draws, for the grid `world.editor.previewGrid` within 2 km of the camera: cell rectangles, each labelled with the entity count of the loaded regions' content the grid assignment sends there; HLOD node outlines; lighting units coloured by their bake state (FRESH green, STALE orange, MISSING red); generator scopes; and every root that violates R3 or STREAM's overlap bound in magenta. The counts come from `Zenith_AssignWorldContent` over the loaded regions only (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Partition functions"), which reads each open region document's resolved data on the main thread and is therefore bounded per frame: the preview keeps one assignment result per open region document, keyed by its document ID and state ID; on `EDITOR_EVENT_HISTORY_COMMITTED` it marks the documents whose state ID changed, and each editor frame re-assigns marked regions in region-key order until `uWORLD_EDITOR_PREVIEW_ROOTS_PER_FRAME` (16,384) roots have been assigned that frame, finishing the region in progress in the next frame, so one edit costs the edited region's roots, spread over frames, never the loaded world. This is the one WORLD computation the main thread runs ("World editor extension"). HLOD outlines, bake states and generator scopes come from the latest plan; `world.partition.check` runs the whole partition in the plan operation.

## Bake panel and commands

The `World Bakes` panel is WORLD's orchestration behind EDIT's bake panel (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Bake panel"; row ED18 answered by row WD15): it lists the bake plan of the plan operation's latest plan ("Bake planner" in `Zenith2/Docs/Design/WORLD/Bakes.md`; "World editor extension") as a table of units with kind, key, state and last result, filterable by kind and state, with the line "plan of the saved documents (<n> unsaved)", and offers:

| Action | Effect |
|---|---|
| Bake stale lighting | `world.bake.lighting` over every STALE and MISSING lighting unit of the scope (the selection on the region map, else the whole world) |
| Force bake | the same over every unit of the scope |
| Generate | `world.generate` for the selected baked generators |
| Bake navigation, Rebuild HLOD | cook requests for the scope's products |
| Cancel | cancels the running operation at its next row of texels ("Bake execution" in `Zenith2/Docs/Design/WORLD/Bakes.md`) |
| Open report | the last bake report, with each unit's backup batch and a "Restore this batch" link (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Restoring a backup") |

`world.bake.lighting` pins the latest plan and starts `Zenith_WorldBakeOperation` with it, refused as "Starting a pinned operation" states, and the panel's "Save and bake" follows that section's policy. The bake is a background operation outside the history: no record is pending while it runs, so every command, save, close and Play stays available; each unit is written in its own journaled, backed-up batch when the unit completes; editing an input during the bake makes the unit STALE when written, which the panel shows ("Bake execution"). A bake and a generation of one world are mutually exclusive ("World editor extension"); starting either while one runs is refused with the notification "a bake or generation of <world> is running".

`world.generate` runs in two parts. Its computation is a `Zenith_WorldGenerateOperation` over the pinned latest plan, refused as "Starting a pinned operation" states: one job builds the generator plan (`Zenith_PrepareWorldGenerator` over the plan's resolved regions and its own pinned access, so every rule set, terrain and prefab it reads is the plan's bytes or is refused), then one job per selected region job computes the outputs and writes the generated document's canonical bytes and their SHA-256; the main thread only submits and polls, and the history is not locked. Each region's recorded fingerprint is the one its prepare computed from the bytes it read through the pinned access. When every region job has completed, the extension executes the record of `world.generate` synchronously, on the main thread, in O(documents of the record) work: for each region it refuses the region when a path its job read has a last-change generation above the plan's `G0`, or a document it writes is DIRTY ("inputs changed during generation; run world.generate again"), generating the others, then applies, as one record, the prepared bytes of each generated document (created or rewritten as an input-only document), its region object in the world document and the generator document's `outputs` record. Undo restores every document of the record. A record larger than `editor.undo.maxBytes` becomes a fence naming its backup batch (EDIT's rule for oversize records, `Zenith2/Docs/Design/EDIT/Foundation.md` → "History memory bound").

## Play in the editor

Play of a streamed world: the editor saves nothing implicitly (EDIT's `editor.play.saveBeforePlay`); the play controller asks the cook server to `cook` the world's primary product and its dependencies (cell scenes, HLOD meshes, lighting assets, navigation tiles; ASSET's incremental keys make an unchanged world a no-op, Z2-B-ASSET-011), then starts the play world from the persistent scene, whose `Zenith.Streaming.WorldRef` binds the cooked world (`Zenith2/Docs/Design/STREAM.md` → "Editor surfaces"). A play started with unsaved region documents shows "the play world uses the saved region documents (<n> unsaved)". An unstreamed world plays by loading its content scene additively. The time from Play to the first ready source is measured against Z2-B-EDIT-006.

## Concurrent editing granularity

Zenith2 has no shared editing server: team members edit separate checkouts and combine work through source control, and on one machine the editor, the cook server and CLI tools share files through SCENE's storage-root writer lock (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Storage-root writer lock"). WORLD's granularity is therefore the file, and inside a WORLD document the keyed child, each kind of edit touches:

| Edit | Files and keyed children it touches | Conflicts with |
|---|---|---|
| place, move, edit or delete content within a region | that region document | an edit of the same entity, instance or member (CONTENT's scene policy) |
| move content across a region border | the two region documents | edits of the moved roots in either |
| create or delete a region | one keyed child of the world document's `regions`, and the region document | another create or delete of the same region |
| add, rename or change a layer | one keyed child of `layers` (a rename removes the old key and adds the new one), and for a new grid-realised layer one element of `streaming.layerGridOrder` | another change of the same layer; two layers made grid-realised on two branches (both append to `layerGridOrder`, an ordering conflict, resolved by keeping both in either order and re-checking W28) |
| change grids or bake settings | the world document's `streaming.grids` or `bakes` member | another change of the same member |
| edit a generator's configuration | that generator document | another edit of the same member |
| rename a generator | that generator document's `name` member | another rename of it |
| regenerate a baked generator | its generated region documents, those regions' keyed children of `regions`, its `outputs` | another regeneration of it |
| re-run a placement generator (a cook-mode `world.scatter` over edited rules) | nothing: cook-mode output exists only in the cook; a baked one is "regenerate a baked generator" | — |
| bake lighting for a cell | that cell's header and page documents | another bake of the same cell |
| sculpt or paint terrain | the `*.terraintile.json` documents of the 128 m tiles under the brush ("Terrain editing granularity") | another edit of the same tile |

Rules that keep independent work independent:

1. Every collection of identified children in a WORLD document is a keyed or ID map (`Zenith2/Docs/Design/WORLD/Documents.md` → "World canonical schema"), so two people adding different layers, regions or generators add disjoint keyed children. WORLD documents are merged by CONTENT's structural merge driver (`Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge API", "Merge policies": `Zenith.World.WorldDocument` and `Zenith.World.GeneratorDocument` are STRUCTURAL over WORLD's canonical schemas, row WD16), which merges keyed children by key, so disjoint keys merge to their union whatever lines they occupy, two additions at the end of a map included. A line merge is never relied on: two additions after the same last key edit the same line (its trailing `,`) and conflict under a line merge (`Test_WorldConcurrency_DisjointBranchesMergeToUnion`).
2. No cooked or derived runtime data is a source file: cells, the streaming world index, HLOD meshes, navigation input scenes, AI's navigation products and every other product of the partition live in the cooked tree and the DDC, with GUIDs derived from keys (`Zenith2/Docs/Design/WORLD/Partition.md` → "Product identity"), so the world's `.meta` never changes when content moves and no generated or derived file can conflict.
3. The partition's output depends only on the documents' content, never on the order edits were made or merged (`Test_WorldPartition_RegionOrderIrrelevant`); two branches that edited disjoint regions produce, after the driver's merge, the cook output of the union.
4. Explicit bake outputs are owned by exactly one unit each; two people baking different cells never touch the same file.
5. A region is the unit a person "owns" while working. The advisory claim mechanisms are the per-user layer lock (local to one checkout) and EDIT's LFS locks on LFS-tracked files (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Commit and locks"): bake documents are always LFS-tracked and not mergeable (their suffixes are in CONTENT's `binaryExtensions`, `Zenith2/Docs/Design/WORLD/Documents.md` → "Common document rules"), and terrain tile documents are LFS-tracked when ENV's `.terraintile.json` suffix is in that list (ENV's and CONTENT's per-suffix choice, row CT31 (b)), so the bake panel warns before baking a cell whose documents another user has locked, and the terrain tool warns before a stroke over a tile another user has locked ("Terrain editing granularity").

## Terrain editing granularity

ENV assigns to WORLD "which partition a tile belongs to, per-partition locking for concurrent editing" (`Zenith2/Docs/Design/ENV.md` → "Boundary with WORLD") and "which partition locks it takes under concurrent editing" for placement (`Zenith2/Docs/Design/ENV/Procedural.md` → "Boundary with WORLD procedural generation"):

- **Tile to region.** A 128 m terrain tile belongs to the region whose rectangle holds the tile's minimum corner; the world origin is a multiple of 128 m from every terrain's origin and the region size a multiple of 128 m (W4, W14), so every tile belongs to exactly one region and a region of size S holds (S / 128)² tiles.
- **What a stroke touches.** A sculpt or paint stroke writes the `*.terraintile.json` documents of the tiles its brush footprint overlaps (ENV's terrain sink, one journaled batch per stroke, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Saving"); a stroke crossing a region border touches the tiles of both regions and claims nothing beyond them. Placement regenerates derived data only (ENV's "Regeneration after edits"), never a tracked file, and a cook-mode generator over the edited terrain re-runs in the next import; neither takes a lock.
- **Claims.** The per-user layer lock does not cover terrain (terrain is not region content). A tile document is LFS-tracked when its suffix is in CONTENT's `binaryExtensions` (row CT31 (b): LFS is per suffix, never per size); the terrain tool shows, per brush footprint, the LFS lock owner of every tile under it and warns before a stroke over a tile another user has locked; the region map badges each region with the lock owners of its tiles, so "taking a region" for terrain work is locking its (S / 128)² tile documents through EDIT's LFS lock commands (row WD24).
- **Merge policy.** Terrain tile documents merge whole-file (CONTENT's `Zenith.Environment.TerrainTileDocument` policy, WHOLE_FILE, row EN20): a tile edited on both branches is one conflict resolved by taking one side whole; the terrain document itself merges structurally.

## Merge conflicts in WORLD documents

| Document | Resolution |
|---|---|
| region and generated region documents | CONTENT's semantic merge driver for scenes (`Zenith2/Docs/Design/CONTENT/Merge.md` → "Scene and prefab rules"; the documents are input-only `.scene.json` files and keep SCENE's policy); a remaining conflict opens the document CONFLICTED (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Diff revert and resolve") |
| world and generator documents | CONTENT's STRUCTURAL policy over WORLD's canonical schemas (row WD16); a conflict inside one keyed child is a textual-conflict marker the World panel resolves member by member |
| light bake header and page documents | never merged: "take ours" or "take theirs" (EDIT's resolve), after which the unit is STALE or FRESH by its fingerprint and the panel offers to re-bake |
| terrain tile documents | whole-file, as "Terrain editing granularity" states |

## After a merge

After a pull or merge, nothing derived is trusted: the next cook re-imports the world (its inputs changed), recomputes every fingerprint, re-derives every cell, navigation input and navigation product, and reports staleness (W34, W41); the World Bakes panel shows the result. A merge that combined two edits each valid alone can still make a world invalid, which the importer reports with both paths: two people creating the same region on two branches produce one region key with two different `scene` values and two different files at one path, which CONTENT's driver reports as a conflict of that keyed child and git as an add/add conflict of the region document (the person keeps one document, or moves one side's content into it with `world.entity.moveToRegion` after resolving); two entities placed with one ID by copying files outside the editor are W23; a base grid added on one branch while another added a grid-realised layer is W28.
