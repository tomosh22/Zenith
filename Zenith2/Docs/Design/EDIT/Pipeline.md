# EDIT design — content pipeline, hot reload, migration, recovery and source control

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies how the editor reaches the asset pipeline (the cook client over ASSET's cook server), reimport and its timing, asset operations with reference repair, script, shader and asset hot reload, the schema change and its undo, the authoring-document migration flow behind the migration UI, autosave, crash detection and recovery, source-control integration, and the rules that keep every file the editor writes merge-friendly.

## Scope of this file

| Topic | Heading |
|---|---|
| the `ZenithCook --serve` process and the protocol client | "Cook client" |
| reimporting assets and timing it | "Reimport" |
| create, duplicate, rename, move, delete with reference repair | "Asset operations" |
| script, shader and asset reload | "Hot reload" |
| applying and undoing a data-defined schema change | "Schema change" |
| upgrade preview, backup, resume | "Migration flow" |
| periodic recovery copies | "Autosave" |
| one session per game, detecting a crash | "Session lock and crash detection" |
| restoring after a crash | "Recovery on startup" |
| git status, diff, revert, resolve, commit, LFS locks | "Source control" |
| canonical bytes, per-user state, conflicts | "Merge-friendly files" |

## Cook client

### Server process

<!-- elaboration -->
The editor reaches the asset database, importers and cook steps only through `ZenithCook --serve`, one long-lived child process per (game, platform), speaking protocol version 1 of `Zenith2/Docs/Design/ASSET/Formats.md` → "Cook server messages" over its stdin and stdout (`Zenith2/Docs/Design/ASSET/Cook.md` → "The cook server"; rows AS14, AS30). The editor links no cook code: the server is the separate executable `<m_strHostToolsDir>/ZenithCook.exe`, the directory of the editor's own executable (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Host paths"), which the `win` tree builds beside it. The client is `ZenithEditorCore`'s (ruling Q86 (a)).

- Launch: `ZenithCook.exe --serve --game <Name> --platform windows --out <m_strOutRoot>/Cooked/<Name>/windows`, the server mode's command line (`Zenith2/Docs/Design/ASSET/Cook.md` → "Options"), plus `--game-root <dir>` when the session's games root is not the default (a test's scratch copy; `Zenith2/Docs/Design/ASSET/Cook.md` → "Options"), created through `Zenith_CreateEditorChildProcess` (`Tools/EditorCore/Internal/Windows/Zenith_EditorChildProcess_Windows.cpp`): `CreateProcessW` with three pipes (stdin, stdout, stderr), `CREATE_NO_WINDOW`, and the child placed in a job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, so the server never outlives the editor.
- Threads: one reader thread per output pipe (`EditorChildOut`: stdout lines to the protocol queue; `EditorChildErr`: stderr lines to the log as category `editor.cook.server`) and one writer thread (`EditorChildIn`) draining the request queue into stdin, so a full pipe never blocks the main thread. Queues are `Zenith_BoundedQueue` (lock-free, no lock-order row; `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue") of capacity 4,096 lines with `QUEUE_FULL_POLICY_BLOCK_PRODUCER` on the reader side (the reader blocks, so the server is back-pressured) and `QUEUE_FULL_POLICY_REJECT` on the request side (a full request queue fails the request with `EDITOR_ERROR_COOK_BUSY`).
- Every line is parsed on the main thread in step 2 of the editor frame with `Zenith_JsonDocument::Parse` under the default limits and the protocol's line limit `uCOOK_SERVER_MAX_LINE_BYTES` (4,194,304 bytes); the reader discards a longer line through its LF and reports `COOK_ERROR_PROTOCOL` locally.

### Client state machine

| State | Event | Next | Action |
|---|---|---|---|
| `STOPPED` | start (startup step 11), restart timer | `STARTING` | launch the process; send `hello` with `protocolVersion` 1, the editor's `engineCookVersion`, `client` `"ZenithEditor"` |
| `STARTING` | `hello` result with equal versions | `READY` | send `watch {enable: true, debounceMs: editor.cook.debounceMs}` (default 250); send every queued request |
| `STARTING` | `error` `COOK_ERROR_OUTPUT_LOCKED`, then exit code 6 | `WAITING_FOR_LOCK` | status "content build in progress"; restart timer 2 s (row AS30) |
| `STARTING` | `COOK_ERROR_PROTOCOL_VERSION`, exit code 2 | `INCOMPATIBLE` | status "cook server version mismatch: rebuild the tools"; no restart |
| `READY` | a request is sent | `READY` | requests run one at a time on the server; the client tracks them by `id` |
| `READY`, `STARTING` | EOF on stdout or a process exit other than 6 | `RESTARTING` | every in-flight request fails with `EDITOR_ERROR_COOK_SERVER_RESTARTED` (an unknown outcome: the request's asynchronous record resolves it from the restarted server's state, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes"); queued requests stay queued; restart after `min(2^k × 250 ms, 8 s)` for the k-th consecutive crash |
| `RESTARTING`, `WAITING_FOR_LOCK` | restart timer | `STARTING` | as `STOPPED` |
| any | shutdown | `STOPPED` | send `shutdown`, wait 5 s for exit, then close the job object |
| `RESTARTING` | 5 consecutive crashes within 60 s | `FAILED` | status "cook server keeps crashing; see the log"; the restart command (`cook.restartServer`) returns to `STARTING` |

`engineCookVersion` is the constant the cook defines and the editor compiles against (ASSET's), so a tools build mismatch is detected by the handshake, never by a malformed result.

### Request queue

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCookClient.h (the types it names: Declarations.md "Cook client types")
/// @contract type Zenith_EditorCookClient
/// Ownership: Owned by Zenith_EditorApp (or a test); owns the child process (Zenith_EditorChildProcess), its pipe threads, the
///   line queues and the request table.
/// Thread affinity: ANY_THREAD_EXCEPT(Submit, Cancel, Poll, Shutdown). Those run on the main thread; the pipe threads touch
///   only the queues.
/// Invalidation: A Zenith_OperationHandle it returned is valid until the request's final line or a restart fails it.
/// Units: Milliseconds.
/// Errors: EDITOR_ERROR_COOK_SERVER_UNAVAILABLE (not READY and not queueing: INCOMPATIBLE, FAILED, --no-cook-server),
///   EDITOR_ERROR_COOK_BUSY (the request queue is full), EDITOR_ERROR_COOK_REQUEST_FAILED (an error line: its code and
///   message are kept in the result), EDITOR_ERROR_COOK_CANCELLED, EDITOR_ERROR_COOK_SERVER_RESTARTED.
/// Reentrancy: NONE. Completion callbacks run from Poll on the main thread and may submit further requests, never Poll.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorCookClient
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorCookClient>, Zenith_EditorError> Create(
		Zenith_EditorCookClientDesc&& xDesc);                           // STOPPED; launches nothing
	void Start();                                                       // startup step 11: STOPPED → STARTING
	Zenith_Result<Zenith_OperationHandle, Zenith_EditorError> Submit(Zenith_StringView strRequest,
		const Zenith_JsonDocument& xArgs, Zenith_JsonNode xArgsObject, const Zenith_EditorCookCompletion& xCompletion);
	void Cancel(const Zenith_OperationHandle& xRequest);               // sends cancel {target: id}
	void Poll();                                                        // main thread, step 2: parse lines, dispatch
	Zenith_EditorCookClientState GetState() const;
	void AddListener(Zenith_EditorCookListener& xListener);             // progress, diagnostic, changed, published
	void RemoveListener(Zenith_EditorCookListener& xListener);
	bool IsIdle() const;                                  // no request running or queued, no watch run pending
	void Shutdown();
};
```

- The client assigns `id`s from 1, monotonically, never reused in a session; pages (`partial`) are concatenated in `page` order and handed to the completion with the final `result`'s `count` and `pages` checked (a mismatch fails the request with `EDITOR_ERROR_COOK_PROTOCOL`).
- At most `editor.cook.maxQueued` (default 256) requests wait; requests run one at a time on the server in arrival order, so the client keeps them in submission order and the UI shows the queue.
- The client reaches the process through the interface `Zenith_EditorChildProcess` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Child process header": launch, write a line, read lines, exit code, terminate), injected at construction: the Windows implementation `Zenith_CreateEditorChildProcess` returns (`Tools/EditorCore/Internal/Windows/Zenith_EditorChildProcess_Windows.cpp`) in the editor, and a scripted in-process fake in the tests (`Zenith_EditorFakeCookServer`, header-only in `Tools/EditorCore/Tests/Support/Zenith_EditorFakeCookServer.h`, X184, which answers from a table of expected requests and canned lines). `Test_EditorCookClient_ProtocolAgainstFakeServer` covers paging, cancel of a queued and of a running request, a duplicate `id` refusal, an unknown request, EOF mid-request with restart, exit code 6 with the 2-second retry, and a version mismatch; `Test_EditorCookClient_RealServerHandshake` launches the real `ZenithCook --serve` on the fixture game and asserts the handshake, a `list` and a clean `shutdown`.

### Notifications

| Notification | Editor response |
|---|---|
| `progress` | the status bar progress of the request (`id`) or of the watch-driven run (`id` null), at most 10 Hz |
| `diagnostic` | appended to the diagnostics panel with its GUID and path; an error of a watch-driven run also posts a notification |
| `changed` | the asset browser invalidates the pages of the listed sources' folders; the document table's listener passes the paths, as host paths, to `Zenith_EditorDocumentTable::OnSourcesChanged`, so documents open at those paths run § "External changes" of `Zenith2/Docs/Design/EDIT/Foundation.md` |
| `published` | § "Asset reload": `OnMountsChanged` when `catalogueChanged`, then `RequestReload` for each listed GUID that is ACTIVE in the runtime's asset registry; thumbnails of the listed GUIDs are invalidated |

### Cook client failures

| Failure | Behaviour |
|---|---|
| the server cannot start (executable missing) | `FAILED` with "ZenithCook not found under the host tools directory: build the tools"; document editing continues; asset operations report `EDITOR_ERROR_COOK_SERVER_UNAVAILABLE` |
| a request's `error` line | the completion receives the code (`COOK_ERROR_…` name), message, asset and path; the notification shows them |
| `COOK_ERROR_SOURCE_LOCKED` | the request failed because another writer holds a storage root; the notification names the holder; the user retries |
| `COOK_ERROR_META_CHANGED` | the requesting panel refreshes the record and asks the user to re-apply |
| a malformed line, an unknown notification | logged and skipped; after 16 consecutive malformed lines the client restarts the server |

## Reimport

### Reimport flow

<!-- elaboration -->
Reimport has three triggers: the `asset.reimport` command (asset browser, inspector "Reimport", the automation step `reimport`), a settings apply (`setSettings` result), and a source file change seen by the watching server (`changed`, then `published`, unasked: `Zenith2/Docs/Design/ASSET/Import.md` → "Editor reimport").

For the command:

1. `reimport {guids}`; progress notifications drive the status bar.
2. The result's `changed` GUIDs and `failed` entries; failures go to the diagnostics panel and the asset's badge.
3. For the editor's platform the server has already cooked and published the changed assets into the loose tree; through its `Zenith_EditorAssetReloadSink` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Asset reload sink header"; in production `Zenith_EditorRegistryReloadSink`, which forwards to the registry) the editor calls `OnMountsChanged` when the catalogue changed and `RequestReload` for every changed GUID that `IsActive` (`Zenith2/Docs/Design/ASSET/Loading.md` → "Development reload"); the registry is the runtime's one registry, shared by every world, so the edit, play and preview worlds all see the reloaded asset at once.
4. Thumbnails and pick-mesh cache entries of the changed GUIDs are invalidated.
5. The completion event "asset reimport visible" is recorded at `OnFrameEnd` of the first frame after the registry reported each reloaded asset ACTIVE again and the frame that drew it was submitted (§ "Reimport timing").

### Reimport timing

The row Z2-B-EDIT-004 "Asset reimport" (and the walkthrough row Z2-B-EDIT-010 "Walkthrough reimport an asset", which contains it; A5 row AB4) measures, with MP-EDIT (`Zenith2/Docs/Budgets.md` → "Editor iteration rows"), "from the source file's save (or the reimport command) to the first frame showing the reimported asset" ([Z2-N-BUDGET-322]):

| Event | Timestamp source |
|---|---|
| start, command trigger | the platform monotonic clock when `asset.reimport` executes |
| start, save trigger | the source file's last-write time converted to the monotonic clock by the offset sampled at session start (`QueryPerformanceCounter` against `GetSystemTimePreciseAsFileTime`), because the save happens in another process |
| end | `OnFrameEnd` of the frame whose render subgraph first drew with the reloaded resource (the asset registry's activation frame + 1, the render lag of Z2-D-050) |

The incremental no-op cook part of the row is ASSET's row Z2-B-ASSET-011 "Incremental no-op cook time per slice" ([Z2-N-BUDGET-327]). The automation step `measureEnd reimport` writes both timestamps into the automation report (`Zenith2/Docs/Design/EDIT/Automation.md` → "Operation timing").

## Asset operations

### Create and duplicate

- "Create" menus of the asset browser create authoring documents from the kind's template (a scene, prefab, material, animation graph, mask, UI document, bank, terrain through ENV's creation wizard): the document is created in memory with the owner's creation function or template bytes, written with `SaveDocuments` as a new document (no expected digest), then `assignGuids` and `import` through the server. One record (`EDITOR_CHANGE_FILE_OPERATION`, create): undo moves the file and its `.meta` into the editor trash, and redo moves both back from the trash directory the record names, so `assignGuids` returns the same GUID and every redone record built against it stays valid (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes", the create and duplicate row; `Test_EditorAssetOps_RedoCreateKeepsGuid`).
- Duplicate copies the source file and its `.meta` without the `guid` (the server assigns a new one at `assignGuids`), so references keep pointing at the original; an authoring document's `id` member is replaced by a fresh persistent ID before writing (SCENE's `REFERENCE_REPAIR_DUPLICATE_DOCUMENT` rule applied up front, `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Reference repair").

### Rename and move

1. Rename and move change the source path and keep the GUID: under the storage root's writer lock (`Zenith_EditorStorageRoots::AcquireWriterLock`, SCENE's `Zenith_DocumentWriterLock`, `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Storage-root writer lock"; held for this one operation, ruling 101, and released before any server request), the `.meta` is renamed to its new name, then the source, each with `MoveFileExW(MOVEFILE_WRITE_THROUGH)` (`Tools/EditorCore/Internal/Windows/Zenith_EditorFileMove_Windows.cpp`). The `.meta` is ASSET's document, so holding the lock keeps the cook (which writes `.meta` files under the same lock) from reading or rewriting either file mid-rename. `DOCUMENT_ERROR_STORAGE_LOCKED` after the lock's wait fails the operation with `EDITOR_ERROR_STORAGE_LOCKED` naming the holder, and nothing is moved.
2. Reference repair: `Zenith_PlanReferenceRepair({ REFERENCE_REPAIR_ASSET_MOVED, subject, old path, new path })` over the session's `Zenith_ReferenceIndex` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_ReferenceRepair header"): IDs do not change, so only prefab-instance `hint` paths are rewritten. Before step 1 moves anything, every target document of the plan is checked by `Zenith_EditorDocumentTable::CheckPlannedWrite`: an open dirty, conflicted, missing or migrated referencing document refuses the whole operation with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` naming it (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes"); then the plan's writes go through `SaveDocuments` of the game root in one batch (the migrator takes the writer lock itself), each with its expected digest, and each open clean referencing document takes its new bytes in memory in the same step (`NoteEditorWrite` before the batch, `ReplaceState` after it), so it stays CLEAN, keeps its history, and is never reloaded by its own write's notification.
3. `assignGuids` for the new path (the server recognises the moved `.meta`), then `reimport`; the asset browser refreshes. Steps 3's requests make the record asynchronous (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes").
4. One record: the inverse renames back (under the lock) and applies the plan's inverse writes (the old bytes, kept in the record) with the digests the forward wrote as expected digests (a file changed since is `DOCUMENT_ERROR_STALE_PLAN`, a permanent failure that fences the record), under the same per-document rule, then `assignGuids` of the old path and `reimport` of the GUID.
5. A failure at step 1 renames back what was renamed; a failure at step 2 renames both files back; a failure at step 3 is compensated as Foundation.md "Asynchronous changes" states (both files back, the hints restored, `assignGuids` of the old path); a failed compensation leaves a fence naming where each file is. In every case but the fence, nothing is recorded.

### Delete and the editor trash

- Delete never removes bytes: the source and its `.meta` are moved to `Games/<Name>/.editor/trash/<UTC timestamp>/<game-relative path>` (gitignored); then the server is sent `import {guids: [g]}` for the deleted GUID g, which finds no source and marks the GUID MISSING (no `assignGuids` is sent: no new source exists); and SCENE's `REFERENCE_REPAIR_ASSET_DELETED` plan runs when the user chose a replacement asset (references rewritten) or "set references to none" (references nulled; prefab instances of a deleted prefab are unpacked only when chosen), shown first as a preview listing every referencing document. The plan's writes follow the per-document rule of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes", checked before the files are moved.
- Undo moves the files back and applies the repair plan's inverse writes (with the forward's digests expected), then `assignGuids` of the restored path and `import` of the GUID.
- The trash is emptied by the "Empty editor trash" command (with a confirmation listing the files and their sizes), never automatically; files are permanently removed by that command alone.

### Reference repair

Reference repair across many documents (a folder move touching hundreds of scenes) is CONTENT's recoverable batch operation (`Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Batch API", "Move and rename", "Delete"; inventory items Z2-I-CONTENT-16 to -21): journaled, resumable, with a report. EDIT calls it for operations touching more than `editor.assetOps.batchThreshold` (default 64) documents and shows its report; at or below the threshold EDIT writes SCENE's plan in one `SaveDocuments` batch itself. Either path is one history record:

| Path | The record's change | Forward | Inverse | Open target documents |
|---|---|---|---|---|
| at most the threshold | `EDITOR_CHANGE_FILE_OPERATION` (`Zenith_EditorFileOperation`) | the file moves, the plan's batch, the server requests (§ "Rename and move") | the reverse moves, the plan's inverse batch, the server requests | `Zenith_EditorDocumentTable::CheckPlannedWrite`, `NoteEditorWrite` and `ReplaceState` per document (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes") |
| above the threshold | `EDITOR_CHANGE_FILE_OPERATION` holding a `Zenith_EditorContentBatchOperation` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Content cook channel header") over a `Zenith_ContentBatchOperation` | the batch, created and run on the thread `EditorContentBatch` (`Plan`, `Run` for planning, `Run` for execution), its cook requests through a FORWARD-mode `Zenith_EditorContentCookChannel` that never waits | `Zenith_ContentBatchOperation::Rollback` (`Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Abandon and rollback"), after `Open` on a new `EditorContentBatch` thread | checked with `CheckPlannedWrite` on the main thread over the documents `GetReferencingDocuments` lists once planning has returned at PLANNED and before execution starts (a dirty one refuses the operation: the batch is abandoned and discarded with nothing written); every open clean target gets `DeferExternalChanges`; when the batch (or its rollback) finishes, the main thread reads each such document's file, `ReplaceState`s it with the file's bytes, a new state ID and the read digest (CLEAN), drops the deferred notifications whose digest equals the read, and resumes them (`ResumeExternalChanges`) |

The batch's record is asynchronous (it completes in following frames). The batch thread follows CONTENT's thread model (`Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Batch API", "Planning"; `Zenith2/Docs/Design/CONTENT.md` → "Thread affinity per type"): the thread that calls `Plan` owns the batch, `Plan` reads nothing, planning and execution are two `Run` calls that poll the channel with a 1 ms sleep, and the progress callback's values (planning's indexed-document count included) reach the main thread's progress display through a depth-1 queue; the editor never steps the batch per frame; a batch interrupted by a crash is resumed or abandoned by CONTENT's own resume path at the next start, outside the history, and its report is shown in the migration panel's Resume view beside the journal recovery. `Test_EditorAssetOps_LargeBatchUsesBatchOperation` moves a folder whose referencers exceed the threshold (with one open clean referencing scene) and asserts one record holding the batch operation, the open scene CLEAN with the repaired hints and its history intact, and that undo runs `Rollback` and restores every file's bytes.

`Zenith_EditorDocumentTable::DeferExternalChanges(xDocument)` holds every external-change notification of the document in a per-document list of at most 64 entries (the newest kept) instead of running § "External changes" on it; `ResumeExternalChanges(xDocument, xAdoptedDigest)` drops the held entries whose digest equals `xAdoptedDigest` and runs the rest in order.

## Hot reload

### Script reload

<!-- elaboration -->
The editor owns the in-editor script workflow (`Zenith2/Docs/Design/SCRIPT.md` → "What SCRIPT authors and what EDIT owns"; A5 row SB14):

1. The storage roots' host watchers (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers": one recursive `Zenith_EditorHostWatcher` per storage root, so `Games/<Name>/Scripts/` and every engine library's `Internal/Content/Scripts/` are watched) report each changed `.luau` path; a rescan record of a root makes script reload hash every `.luau` file under it and take those whose digest differs from the module's last compiled digest.
2. A changed `.luau` file is compiled in process in the next editor frame's step 2: `Zenith_ScriptCompileModule` with the production options (so the result equals the cooked form, [Z2-N-SCRIPT-508]), then `Zenith_ScriptCheckStateless` for chunk-system and library modules (library importers re-checked), setting the bytecode flag the checked result carries (A3 row SC19); diagnostics go to the diagnostics panel with file and line.
3. Accepted bytecode is queued with `Zenith_ScriptWorld::RequestHotReload` on each play world's script world (`Zenith2/Docs/Scripting.md` → "Quarantine and hot reload": the reload is applied in the next frame prologue's step 6 inside the registry window, goes live at the world's next tick boundary, and rolls back entirely on any failure); the edit world runs no script, so it needs none.
4. The definitions (`.d.luau`) are regenerated in process with `Zenith_ScriptEmitDefinitions` whenever the type registry changed (a schema change, a module start), written to `${ZENITH_OUT}/Generated/Luau/<Name>/` (`<m_strOutRoot>/Generated/Luau/<Name>/`, the one untracked location [Z2-N-SCRIPT-114] gives them; the editor writes no copy inside a tracked content root, so the tree stays clean) (the directory the cook's `luau-types` step writes, with its `.luaurc`), which the external text editor's language server is pointed at.
5. The cook server's watch independently re-cooks the module into the loose tree, so the next play world loads the new bytecode from content; a play world created before that completes receives the in-process bytecode by the catch-up of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Starting play" step 6.
6. "Reload module" (`script.reloadModule`) calls `RequestModuleReload` (last-good restore with no code change) for a quarantined module.

### Script reload timing

Z2-B-EDIT-002 "Script hot reload" is measured from the source file's save to the first tick that runs the new bytecode ([Z2-N-BUDGET-322], [Z2-N-SCRIPT-427]):

<!-- assumption: pending SCRIPT (row ED16) -->
The end event is the script world's notification that a module generation went live, `Zenith_ScriptWorldListener::OnModuleReloaded(uint16 uWorldTag, uint64 ulTick, Zenith_ScriptModuleID xModule, uint32 uGeneration)`, beside the listener's `OnModuleDisabled` and `OnModuleRecovered` (`Zenith2/Docs/Scripting/PublicTypes.md` → "ZenithScript asynchronous results and host interfaces"). The listener is the runtime's (that header: "Implemented by ZenithRuntime"), called on the world's tick-boundary node; it records the event, as it records its other flags, and the tool access delivers it to the editor through the hook `OnScriptModuleLive` on the main thread after `RunFrame` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Runtime tool access"). The editor records the monotonic time at the `OnFrameEnd` of that frame and publishes `EDITOR_EVENT_SCRIPT_RELOAD_LIVE`. The runtime's per-frame record holds at most 64 such events; beyond that the oldest are dropped and counted (`editor.script.events_dropped`), and a measurement whose event was dropped is discarded and repeated by the W-Editor script.
<!-- /assumption -->

### Shader reload

<!-- elaboration -->
`ZenithShaderCompiler --watch --reload-journal <file>` runs as a second child process when the preference `editor.shaders.watch` is on (default on; launched like the cook server, with the game's shader roots), and the editor's reload watcher, which lives in ZenithEditor alone ([Z2-N-REND-131]), applies its journal (`Zenith2/Docs/Rendering/ShaderSystem.md` → "Watch mode and editor reload"; A5 row RPB20):

1. Tail `Zenith2/out/<checkout-hash>/<preset>/Shaders/reload.journal`: the journal's non-recursive host watcher (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers", over `<m_strOutRoot>/<preset>/Shaders/`) reports a change of the file or a rescan; the reload watcher then reads the bytes appended since its last offset, splits LF-terminated lines (a partial last line waits), parses each record, and applies records strictly in `seq` order; a gap in `seq` (a lost record) is reported and the watcher re-reads from offset 0, skipping `seq` values already applied ([Z2-N-REND-128]: never skip one).
2. Per record, by `status` and the program kind:

| Record | Editor action |
|---|---|
| `ok`, `layoutHash == previousLayoutHash` | load the new container and mark the program's PSOs stale through the renderer's reload entry; the previous PSO stays bound until each replacement is ready ([Z2-N-REND-129]) |
| `layout-changed`, engine program | keep the old container; notification and console line "The parameter struct of <program> changed layout; rebuild the C++ targets to load it" ([Z2-N-REND-130], [Z2-N-BUDGET-323]) |
| `layout-changed`, material program | re-cook the material constants of every loaded material of that type: `cook` of their GUIDs through the cook server, then reload them (`published`), then proceed as `ok` ([Z2-N-REND-254]: material constants are data) |
| `error` | keep the old container; each error to the diagnostics panel as `file(line,col): message` |

<!-- assumption: pending RPROD -->
The renderer's reload entry is `Flux_Renderer::ReloadShaderContainer(const Flux_ShaderReloadRequest&)` (main thread, between frames: the program ID, the container path, the container hash), which loads the container, moves every PSO table entry of the program to its reload state of `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states" (`READY` with a replacement `PENDING`, `FAILED` to `ABSENT`) and queues the replacements on the asynchronous creation schedule; and the counter `Flux.PSO.ReloadSwaps` with the frame of each swap, which the editor reads for the end event of § "Shader reload timing" (row ED12).
<!-- /assumption -->

### Shader reload timing

Z2-B-EDIT-003 "Shader blob reload" runs from the `.slang` save (its last-write time, converted as for reimport) to the first frame drawn with the rebuilt PSOs ([Z2-N-REND-132]): the end is `OnFrameEnd` of the frame after the last replacement PSO of the program swapped. A layout change is outside the row and reported instead of timed ([Z2-N-BUDGET-323]).

### Asset reload

Asset hot reload is the `published` path of § "Notifications": every world sees the reloaded asset through the one asset registry; a scene document's projection is not touched (the scene references assets by GUID, which do not change); a prefab document whose source file changed is an external document change (`Zenith2/Docs/Design/EDIT/Foundation.md` → "External changes") and reconciles its instances.

## Schema change

A data-defined component schema change runs the flow of `Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow" and is recorded as one record of kind `EDITOR_CHANGE_SCHEMA` holding: the schema file's old and new bytes, the migration batch name and backup root of the documents it migrated, and the list of those documents.

### Schema undo

Undoing a schema change (refused while any play world exists, as applying is):

A type's version only rises inside a hot-reload window (`Zenith2/Docs/Design/ECS/Reflect.md` → "Revisions and the hot-reload window"), so the previous revision cannot be re-registered into the running registry. Undo therefore restores the files and restarts the session's runtime. The schema change's operation drives it in two halves, because a restart never runs inside a hook (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Tool access calls per hook"):

1. `Begin(INVERSE)`, in the history's `Poll` (editor frame step 2): check every open document of the batch with `CheckPlannedWrite`, the schema file itself among them (it is a target of the batch: when it is open in the JSON editor panel, its document is checked like the others; a dirty one fails the undo with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN`, transient, nothing written); write, in one journaled batch of the game root, the schema file's old bytes and the pre-migration bytes of every document the apply migrated (the record holds them; they equal the backup batch's copies), each with the post-migration digest (the schema file's: its applied bytes' digest) as its expected digest, after `NoteEditorWrite` of each new digest, the schema file's included; every record newer than the schema change has already been undone (undo is last-in first-out), so each document's current state is its post-migration state, which the expected digests check (a mismatch is a permanent failure).
2. In the same call, replace the state of every open document of the batch, the schema file's document included, with `Zenith_EditorDocument::ReplaceState(old bytes, the record's "before" state ID, the new disk digest)`: the documents take exactly the state IDs the record stored, so the dirty tracking and every older record (the schema file's earlier JSON edits included) stay valid, and no fence is created (this is not a reload from disk).
3. Still in the same call, `Zenith_EditorRuntimeHost::RequestRuntimeRestart()`; `Begin` returns IN_FLIGHT. `Zenith_EditorApp::Run` performs the restart after this frame's `RunOneFrame` returned (§ "Runtime restart"), whose registry reads the old schema; the edit worlds are re-projected from the documents' restored states.
4. `Poll` of the next frame reads `GetRuntimeRestartState()`: `EDITOR_RUNTIME_RESTART_DONE` → DONE, and the record commits on the redo stack; a failed restart never returns (the process ends through § "Runtime restart" step 9, after its autosave).
5. Redo is the same procedure with the new bytes, the record's "after" state IDs (the schema file's document included) and a second restart.

The restart is the "Schema undo" cost the user sees (seconds, the scene-open time plus the runtime's startup); the notification says "Undoing a schema change restarts the editor runtime". `Test_EditorSchema_UndoRestoresDocumentsAndRows` (with the schema file open in the JSON editor panel and two earlier JSON edits of it recorded) asserts that after the undo the schema file and the documents hold their pre-apply bytes with the record's state IDs, the schema file's open document equal to its file and its two earlier edits still undoable, that the edit world equals a fresh projection of them under the old schema, that an older record touching one of those documents still undoes afterwards, and that redo restores the new schema, the post-apply bytes and the edit world's migrated rows.

### Runtime restart

<!-- elaboration -->
`Zenith_EditorApp::RestartRuntime` (private, `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Application header") replaces the runtime while the process, the session and its documents survive. It runs only in `Zenith_EditorApp::Run`, on the main thread, after `RunOneFrame` returned and before the next one (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor run loop" step 3), so no hook and no frame of the old runtime is on the stack; it is requested, never performed, by the history's `Poll` (`RequestRuntimeRestart`, refused while a play world exists), and the requesting record stays `PENDING_UNDO` or `PENDING_REDO` until it completes:

| Step | Action | On failure |
|---|---|---|
| 1 | flush the autosave writer and write an autosave of every dirty document (so a failure below loses nothing) | continue (a warning) |
| 2 | destroy the preview worlds (`Zenith_EditorPreviewHost::DestroyAll`), the terrain previews, the edit worlds with their projections, proxies and extractors (`Zenith_EditorWorldSet::DestroyAll`) | — (cannot fail) |
| 3 | release every texture-table slot (`Zenith_EditorTextureTable::ReleaseAll`: tool textures destroyed through the backend on the old renderer, asset handles dropped), the font atlas and the icon atlas; discard both draw snapshot slots and both overlay slots | — |
| 4 | unregister the view extensions, the input claimant and the cache consumer from the old renderer, input system and governor; release every object the editor borrowed from the old `Zenith_Platform` (a `Zenith_ToolServices` member, destroyed with the runtime at step 5): the ImGui platform adapter's window, text-input and clipboard references (`ReleasePlatform`, `Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui platform adapter") and the session clipboard's system clipboard (`Zenith_EditorClipboard::RebindSystemClipboard(nullptr)`); keep every open document's canonical bytes for the type rebind (`Zenith_EditorDocumentTable::PrepareTypeRebind`, while the old registry is alive) | `PrepareTypeRebind` failing (`EDITOR_ERROR_OUT_OF_MEMORY`): step 9; the rest cannot fail |
| 5 | destroy the runtime (MOD's shutdown order) | — |
| 6 | `Zenith_RuntimeToolAccess::Create` with the same command line, composition and hooks; it fills a new `Zenith_ToolServices` | step 9 |
| 7 | re-inject: `Zenith_EditorSession::RebindRuntime(runtime, services)` (the world set, the play controller, the schema model and the operation context take the new references; the session takes the new `m_pxTypes` and `m_pxStrings`, the document table re-reads every open document under the new registry, `RebindTypes`, and the session clipboard takes the new platform's `Zenith_Clipboard`, `RebindSystemClipboard(&xServices.m_pxPlatform->GetClipboard())`, null when headless), the shell, the panel context and the cook client's reload sink take the new services, the ImGui platform adapter re-acquires its window, text-input and clipboard references from the new platform (`BindPlatform`), `Zenith_EditorFluxTextureBackend::RebindRenderer` and `Zenith_EditorTextureTable::RebindBackend` the new renderer; re-register the view extensions, the claimant and the cache consumer; re-create the font atlas and the icon atlas tool textures | step 9 |
| 8 | re-create the main edit world and each open prefab stage with their proxies and extractors, and project every open scene document into them from its in-memory state (the scene-open path of Foundation.md "Reconcile algorithm"); preview worlds are re-created when their panels next draw; set the restart state to DONE and publish `EDITOR_EVENT_RUNTIME_RESTARTED` | step 9 |
| 9 | (failure of 6, 7 or 8) log the runtime error under `editor.runtime`, show it, and end the process with exit code 1 through the shutdown sequence from step 3 (dirty documents were autosaved at step 1, so the next start offers them for recovery) | — |

What survives a restart: the session and everything it owns that is not a world (the document table and every document's state, the history, the selections, the clipboard's content (its system-clipboard pointer is re-bound), the command registry, the event bus), the storage roots and the migration registry, the cook server process and its client, the host watchers (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Host watchers": they belong to `Zenith_EditorApp` and borrow nothing of the runtime), the ImGui context with its layout and panels, the per-user files. Nothing the editor keeps across step 5 points into the old runtime: every borrowed platform object is released at step 4 and re-acquired at step 7. Panels hold no texture reference or service pointer across frames (the panel context gives them each frame), so after step 7 they re-request their textures and draw the checker for at most one frame.

- `Test_EditorApp_RestartRuntimeKeepsSession` (`ZenithEditor_Tests`) restarts the runtime of an in-process headless session with two open documents (one dirty), four records, a selection and clipboard content, through `editor.restartRuntime`, and asserts the documents' bytes and state IDs, the undo count, the selection and the clipboard are unchanged, that the edit world equals a fresh projection, that `runtimeRestarted` is published, and that the restart ran between frames: `RestartRuntime` begins with `ZENITH_CHECK(!m_bInsideHook)` (a flag every hook sets for its duration), which did not fire, and `EDITOR_EVENT_RUNTIME_RESTARTED` carries the frame index of the frame whose `Poll` requested it. Being headless (a null system clipboard), it cannot see the platform objects; its windowed arm is the next test.
- `Test_EditorApp_RestartRebindsPlatformObjects` (`ZenithEditor_Tests`, `editor-render`, Stage 4) runs the same restart in an in-process hidden-window editor on WARP and lavapipe with a copied entity in the clipboard, then copies again after the restart (`edit.copy`), and asserts that the new platform's `Zenith_Clipboard::GetText` returns the `zenith2.editor-clipboard` document of the copied entity, that `edit.paste` from the system clipboard after the restart pastes it, and that the ImGui platform adapter reports the new platform's main window (its `DisplaySize` equals that window's pixel size in the first frame after the restart).
- `Test_EditorApp_RestartFailureExitsOneAfterAutosave` (`ZenithEditor_Tests`, suites `z2-stress` and `z2-edit-cooked`, Stage 4) launches a child editor (`--headless --automation-script` on a scratch copy of `EditorFixture`, `--random-seed 1`) whose script dirties a document and runs `{"step": "command", "id": "editor.restartRuntime", "args": {"faultAtStep": 6}}` (the command's test fault point: the restart's step 6 fails as `Create` failing would), and asserts that the child exits with code 1 and that the dirty document's `.zautosave` exists with its state ID; the test process is unaffected, because the exit is the child's.

## Migration flow

<!-- elaboration -->
The migration UI (inventory items Z2-I-EDIT-15 upgrade preview, -16 backup, -17 resume; Z2-D-048; `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind") presents SCENE's framework, which does the work (`Zenith2/Docs/Design/SCENE.md` → "What SCENE exposes to EDIT"; A5 row ScB13), for every registered document type of every storage root.

### Planning on open

- At session start, after recovery (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Startup sequence" step 8), the migration model (`Zenith_EditorMigrationModel`, `ZenithEditorCore`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Migration model header") runs `PlanMigrationOfDirectory("")` of each root's migrator on a background operation (the plan reads every document but writes nothing, so it runs off the frame in slices of `editor.migration.planDocumentsPerFrame` documents, default 64). The framework plans every file whose suffix a registered type claims, `.meta` files included; the model removes the entries of type `Zenith.AssetMeta` from the plan it keeps, and plans `.meta` documents through the server's `migratePreview` without paths (every `.meta` of the game's roots), because the cook server owns them.
- The overview shows the counts; nothing is migrated without the user's apply. A document needing migration still opens: it is migrated in memory (`DOCUMENT_OPEN_MIGRATED`) and its next save writes the migration (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Document states").
- The report of a plan is exported with "Save report…" as SCENE's `zenith2.migration-preview` v1 JSON (`Zenith_MigrationPlan::WriteReportJson`).

### Applying a migration

1. "Back up and migrate" takes the plan (or the selected entries) and refuses when any entry is blocked (the panel lists the blocking errors first: a newer version, a chain gap, a failed step, a non-deterministic step).
2. `ApplyPlan` of each root's migrator in root order: the stale-plan check, the low-disk check, the backup into `.migration/backups/<UTC timestamp>/` and the journaled write all happen inside the root's writer lock (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Applying a plan", "Journaled write", "Storage-root writer lock").
3. `.meta` entries go through the server's `migrateApply` with the preview token; a token mismatch (`COOK_ERROR_META_CHANGED`) re-plans and asks again. With `.meta` entries the record is asynchronous, and a cancelled, timed-out or restarted `migrateApply` is resolved with `migratePreview` and compensated for exactly its applied entries (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes").
4. Open documents of the plan follow `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes", checked before step 2 writes anything: a dirty, conflicted or missing open document of a selected entry refuses the apply with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` naming it; a document opened `MIGRATED` is the exception, because its in-memory state is the plan's migration: its entry is written and the document takes the written bytes as its saved state (CLEAN when its state ID had not moved since open); every clean open document takes its new bytes through `ReplaceState` (new state IDs, the history untouched, `NoteEditorWrite` before the batch) and is reconciled.
5. The whole apply is one history record (`EDITOR_CHANGE_DOCUMENT_BYTES` per document, the old bytes being the backup's), so a migration is undoable in the session; such a record has no per-record cap and is bounded by `editor.undo.maxBytes` alone (`Zenith2/Docs/Design/EDIT/Foundation.md` → "History memory bound"); across sessions the backup batch is the undo (§ "Restoring a backup").
6. Failures: `DOCUMENT_ERROR_STALE_PLAN` re-plans the root and shows the new plan; `LOW_DISK` and `STORAGE_LOCKED` leave everything unwritten and say why; an interruption (a crash) is recovered at the next start (§ "Resuming after an interruption").

### Resuming after an interruption

`RecoverPendingJournals` runs on every root at startup before any document opens (startup step 8). Its report (journals found, rolled back, rolled forward, entries restored) is shown in the migration panel's Resume view and summarised in a notification ("Resumed an interrupted migration of n documents" or "Rolled back an interrupted save of n documents"). A journal that cannot be recovered (`DOCUMENT_ERROR_JOURNAL_CORRUPT`, `DOCUMENT_ERROR_RECOVERY_CONFLICT`) makes the session read-only for that root, names the journal and the document, and offers "Open the journal" and "Restore the batch from backup"; nothing is written to that root until a person resolves it and restarts. `Test_EditorMigration_ResumeAfterKillAtEachJournalState` kills a child editor process (a test harness launching `ZenithEditor --headless --automation-script` that applies a migration with a fault point at each journal state of SCENE's table) and asserts the next session's recovery report and the documents' bytes.

### Restoring a backup

"Restore this batch" copies every file of a backup batch back over its document through one `SaveDocuments` batch whose expected digests are the documents' current digests (so a file edited since asks first), as one history record, with the per-document rule of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes" (a dirty open document of the batch refuses the restore before anything is written); it is the undo of a migration across sessions and the recovery path of a corrupt journal.

## Autosave

### Autosave schedule

- Every `editor.autosave.intervalSeconds` (default 120; 0 disables), before Play, and before a migration apply, the autosaver writes a recovery copy of every dirty document whose state ID changed since its last autosave.
- An autosave is never a save: the document's file is untouched and the document stays dirty.
- The work is split: the main thread serializes each document's canonical bytes (`Serialize`, `CollectWrites` for multi-file documents such as a terrain's dirty tiles) within `editor.autosave.frameBudgetUs` (default 2,000 µs per frame, continuing over frames), and hands the bytes to the autosave writer thread.

### Autosave write

<!-- elaboration -->
- The writer thread (`Zenith_Thread`, named `EditorAutosave`, joined at shutdown step 4) writes each recovery copy in the format of `Zenith2/Docs/Design/EDIT/Formats.md` → "Autosave file" to `<root state>/autosave/<document ID>.zautosave`, where `<root state>` is `Games/<Name>/.editor/` for the game root and `Games/<Name>/.editor/roots/<Library>/` for an engine content root (`Zenith2/Docs/Design/EDIT.md` → "Untracked editor files") (one file per document, terrain tiles stored inside their terrain's file), as a temporary `.zautosave.tmp` flushed with `FlushFileBuffers` and renamed over the previous copy with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`, then updates `<root state>/recovery.json` (`Zenith2/Docs/Design/EDIT/Formats.md` → "Recovery manifest") the same way.
- The hand-off queue is a `Zenith_BoundedQueue` of 64 entries with `QUEUE_FULL_POLICY_REJECT`: a full queue skips that document for this round and counts `editor.autosave.skipped`.
- `.editor/` directories are gitignored by the nested `Zenith2/.gitignore` row the scaffold writes, are excluded from documents by SCENE's path rule for dot-prefixed segments, and are ignored by the cook server's watch (names under `.`-prefixed directories, `Zenith2/Docs/Design/ASSET/Cook.md` → "The cook server").
- A successful save deletes the document's autosave file and its manifest entry; a clean shutdown deletes every autosave of the session.
- An autosave write failure (disk full, I/O) is a warning notification, once per 10 minutes, and never blocks editing.

## Session lock and crash detection

- At startup step 9 the editor opens `Games/<Name>/.editor/session.lock` with `CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)` and holds it for the session, writing one JSON line `{"pid": <u32>, "startedUtc": "<YYYYMMDDTHHMMSSZ>", "user": "<USERNAME>"}` and LF; a second editor on the same game gets `ERROR_SHARING_VIOLATION`, reads the line with `FILE_SHARE_READ | FILE_SHARE_WRITE`, and exits with code 1 naming the holder. The mechanism is the writer lock's (`Zenith2/Docs/Design/ASSET/Formats.md` → "Cook lock file"): the operating system releases it when the holder exits by any path, so it is never broken by age.
- A crash is detected by the state it leaves: autosave files (or a manifest entry) under a root state's `autosave/` while no session holds the lock. A clean exit leaves none (shutdown step 4).

## Recovery on startup

<!-- elaboration -->
Recovery (inventory item Z2-I-EDIT-09) runs at startup step 13, after the migrators recovered their journals:

1. Read each root's `recovery.json` and every `.zautosave` file it lists; a file failing its header or digest check is listed as unrecoverable and kept for inspection (moved to `<root state>/autosave/damaged/`).
2. For each recoverable autosave: open the document's current file and compute its SHA-256; compare with the autosave's base digest (the digest of the file when the autosaved state was based on it).
3. With `--recovery restore`, or the dialog's "Restore": an unchanged file opens the document, then executes `document.restoreRecovery`, one record of kind `EDITOR_CHANGE_DOCUMENT_BYTES` whose forward bytes are the autosave's and whose inverse is the file's, so the restore is undoable and the document is dirty (restored, not saved). A changed file is never overwritten: "Restore as copy" writes the autosave bytes to `<name>.recovered.<ext>` beside it as a new file (with a fresh document ID for a document type with an ID member), and "Show diff" shows the document diff between the file and the autosave.
4. With `--recovery discard`, or "Discard": the autosave is moved to `<root state>/autosave/discarded/<UTC timestamp>/` (kept until the next clean shutdown, then deleted).
5. Once every entry is handled, the manifest is rewritten without them.

`Test_EditorRecovery_RestoreAfterCrash` launches a child editor (`--headless --automation-script`) that edits three documents, waits for an autosave, and terminates itself with `TerminateProcess`; the test then starts a second session with `--recovery restore` and asserts each document's in-memory canonical bytes equal the autosaved bytes, that the files on disk are unchanged, and that one undo returns each to its file bytes. `Test_EditorRecovery_ChangedFileRestoresAsCopy` modifies one file between the two sessions and asserts the copy is written and the file untouched.

## Source control

### Backend

<!-- elaboration -->
The source-control backend runs the system `git` executable (found on `PATH`, else the preference `editor.git.path`) as child processes, one command at a time, on the source-control thread, with arguments passed as an argument vector (never a shell string) and `GIT_TERMINAL_PROMPT=0`, so no command ever waits for input. Each command has a 30-second timeout (the process is terminated and the operation fails with `EDITOR_ERROR_SOURCE_CONTROL`). A repository is detected by `git rev-parse --show-toplevel` from the game directory; without one the panel shows "Not under source control" and every badge is hidden.

| Operation | Command |
|---|---|
| status | `git status --porcelain=v2 -z --untracked-files=all -- Zenith2/Games/<Name>` |
| LFS state | `git lfs ls-files --json` (pointer files not fetched), `git lfs locks --json` (locks), only when `git lfs version` succeeds |
| file at HEAD | `git show HEAD:<path>` (stdout bytes) |
| revert a file | `git restore --source=HEAD --staged --worktree -- <path>` |
| stage, unstage | `git add -- <paths>`, `git restore --staged -- <paths>` |
| resolve with ours or theirs | `git checkout --ours -- <path>` / `--theirs`, then `git add -- <path>` |
| commit | `git commit -m <message>` with the message passed as one argument and no pathspec, so exactly the index is committed |
| lock, unlock | `git lfs lock <path>`, `git lfs unlock <path>` |

### Status

Status is refreshed at session start, after every editor save, after every source-control command, on the `changed` notification, and at most every 10 s; the parsed entries (path, index state, work-tree state, conflict state) feed the badges of § "Source control panel" in `Zenith2/Docs/Design/EDIT/Panels.md`.

### Diff revert and resolve

- Diff: the document's working bytes and its bytes at `HEAD` are both canonicalised (the HEAD bytes parsed and written through the document's canonical writer, so a HEAD file written by an older tool still diffs meaningfully) and diffed as lines with the framework's diff (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Zenith_DocumentDiff header"); a binary asset shows "binary" with sizes and hashes.
- Revert: an open, dirty document asks first ("Discard the unsaved changes of <path> and revert it to HEAD?"); the revert is one history record of kind `EDITOR_CHANGE_DOCUMENT_BYTES` (forward: the HEAD bytes; inverse: the file's pre-revert bytes and, for an open document, its in-memory canonical bytes and state ID, all kept in the record), executed as a document write through the migrator rather than by `git restore`, so the revert is undoable and journaled; it is the second exception of the per-document rule of `Zenith2/Docs/Design/EDIT/Foundation.md` → "Editor-originated writes" ("The revert exception"): a dirty open document takes the HEAD bytes through `ReplaceState` after `NoteEditorWrite`, and its undo restores the file and then the in-memory state with its old state ID, dirty again; a clean open document follows that table's CLEAN row; files that are not documents (binary sources) use `git restore` after a confirmation, with their previous bytes copied into the editor trash first so the record can still undo it.
- Resolve: a document whose bytes hold conflict markers fails to parse (`DOCUMENT_ERROR_PARSE`) and opens in the CONFLICTED state with the three options: "Open merge tool" (CONTENT's semantic merge driver for scenes, prefabs, schemas and every other canonical document, `Zenith2/Docs/Design/CONTENT/Merge.md` → "Driver command line", run as an external process on the three versions from `git show :1:<path>`, `:2:`, `:3:`), "Take ours", "Take theirs"; after a resolution the document is re-opened and the file staged with "Mark resolved".

### Commit and locks

- Commit acts on the index alone: the panel's "Commit" runs `git commit -m <message>` with no pathspec (a pathspec would commit the paths' working-tree state, `--only` semantics, not what was staged), so exactly the staged files and staged hunks are committed; it refuses while any staged document is dirty in the editor ("Save before committing"), so a commit always holds what the editor shows. `Test_EditorSourceControl_CommitUsesIndex` stages one of two modified files and asserts the commit holds that file alone, with its staged bytes.
- LFS-tracked binaries are not mergeable: the asset browser shows their lock owner, and editing a binary source's settings or replacing it warns when another user holds its lock.
- Source control is never automatic: no editor action commits, pushes, pulls or changes branches.

## Merge-friendly files

### Rules for files the editor writes

<!-- elaboration -->
Inventory item Z2-I-EDIT-12 rests on SCENE's canonical serialization (stable key order, one ID-keyed child per line; `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical serialization"), and the editor adds:

1. Every tracked file the editor writes is written by its owner's canonical writer (SCENE's for scenes and prefabs, the framework's for JSON documents, ENV's `Save` for terrain, UI's model, MOD's scaffold for `game.json`, PLAT's `Zenith_WriteBindingsJson`); the editor has no serializer of its own for a tracked file.
2. Saving an unchanged document writes nothing (no change of bytes, `m_bChanged` false), and saving a document after an edit changes only the lines of the edited objects: inserting an entity changes only its own lines, because its sort key is computed between its neighbours (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Sort keys for edits"); `Test_EditorSave_InsertChangesOnlyNewLines` asserts the line diff of an insert equals the new object's lines.
3. No timestamp, session ID, user name, camera position, selection, expansion state or other per-session value is written into a tracked document.
4. Line endings are LF, the encoding UTF-8 without a BOM, with a final LF, as the canonical rules state.

### Per-user state stays out of documents

Per-user editor state lives in untracked files: `Games/<Name>/.editor/user/session.json` (open documents, the active scene set, camera bookmarks, the selection, hidden and locked flags, panel states), `layout.json`, and the per-user preferences file outside the repository (`Zenith2/Docs/Design/EDIT/Formats.md` → "Session file", "Layout file", "Preferences file"). Two users editing one scene therefore never conflict over editor state.

### Merge conflicts in documents

A conflict inside a document is resolved by CONTENT's structure-aware three-way merge driver (inventory items Z2-I-CONTENT-27 to -29; `Zenith2/Docs/Design/CONTENT/Merge.md` → "Driver command line", "Conflict markers", "Git integration"; row CT18), which the canonical form makes possible; nested-prefab overrides and array-ordering conflicts fall back to an explicit conflict marker, which the editor then shows as a CONFLICTED document (§ "Diff revert and resolve"). "Open merge tool" runs `ZenithContent merge` as a child process (`Zenith_CreateEditorChildProcess`, an argument vector) on the three versions and shows its `--explain` output in the diff view; the merged result is written by the driver, which the editor then treats as an external change of the document (§ "External changes" of `Zenith2/Docs/Design/EDIT/Foundation.md`).
