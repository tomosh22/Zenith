# EDIT design — commands, automation, live link, timing and walkthroughs

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies the command registry (one identifier per editor action, shared by menus, shortcuts, automation and the live link), the automation script format and runner (inventory item Z2-I-EDIT-13), the live-link automation endpoint, the operation timing that measures the W-Editor rows, and the four usability walkthroughs with time targets (Z2-I-EDIT-18 to -21).

## Scope of this file

| Topic | Heading |
|---|---|
| command identifiers and descriptors | "Command registry" |
| scripted editor sessions | "Automation scripts" |
| a loopback control channel for tools and tests | "Live link endpoint" |
| measuring scene open, reimport, reloads and play start | "Operation timing" |
| the human-run timed task walkthroughs | "Usability walkthroughs" |

## Command registry

### Command identifiers

Every editor action is a command with a stable string identifier `<group>.<action>` (ASCII letters, digits and dots, at most 64 bytes, `Zenith_IsAsciiIdentifier` with dots allowed) and `Zenith_EditorCommandID`, `Zenith_Hash64` of it (`Zenith_MakeEditorCommandID`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Identifier header"). Identifiers are never reused for another action and never renamed (an automation script or a user's shortcut names them), the same stability rule as headings and stable names.

### Command descriptors

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCommandRegistry.h (Zenith_EditorArgDesc and the registry: Declarations.md "Command registry header")
enum Zenith_EditorCommandFlag : uint32
{
	EDITOR_COMMAND_FLAG_CHANGES_DOCUMENTS = 1u << 0,   // produces a history record
	EDITOR_COMMAND_FLAG_PLAY_ONLY = 1u << 1,           // enabled only while a play world exists
	EDITOR_COMMAND_FLAG_EDIT_ONLY = 1u << 2,           // disabled while a play world exists
	EDITOR_COMMAND_FLAG_AUTOMATABLE = 1u << 3,         // callable from automation scripts and the live link
	EDITOR_COMMAND_FLAG_DESTRUCTIVE = 1u << 4,         // asks for confirmation interactively (empty trash, discard all)
};

/// @contract value Zenith_EditorCommandDesc
/// Ownership: constexpr data beside each command.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration validates the identifier and the argument schema (EDITOR_ERROR_EXTENSION_DUPLICATE, EDITOR_ERROR_COMMAND_ARGS).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorCommandDesc
{
	const char* const m_szID = nullptr;     // "entity.create"
	const char* const m_szDisplayName = nullptr; // "Create Entity", the menu and palette text
	const char* const m_szMenuPath = nullptr; // "Entity/Create Empty", or nullptr
	const uint32 m_uFlags = 0;
	const Zenith_Span<const Zenith_EditorArgDesc> m_axArgs = {}; // name, kind (bool, integer, number, string, path, persistent ID,
	                                                    // vector, JSON), required, default; checked before Execute
};
```

`Zenith_EditorCommandArgs` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Command header") is a view of a JSON object (a `Zenith_JsonDocument` node) checked against `m_axArgs` by `Zenith_EditorCommandRegistry::ValidateArgs` before `Execute`; an unknown argument, a missing required one or a wrong kind fails with `EDITOR_ERROR_COMMAND_ARGS` naming it, so interactive callers, scripts and the live link pass arguments in one form.

### Built-in commands

The built-in set (extensions add their own; the full list is generated into the command palette and `editor.commands.dump` writes it as JSON):

| Group | Commands (arguments in brackets) |
|---|---|
| `scene` | `open [path, additive]`, `new [path]`, `close [path]`, `reload [path]`, `resetEditWorld` |
| `document` | `save [path]`, `saveAll`, `revertToDisk [path]`, `restoreRecovery [document]`, `close [path]` |
| `edit` | `undo`, `redo`, `undoTo [recordID]`, `copy`, `cut`, `paste [parent, idSeed]`, `duplicate [idSeed]`, `delete`, `selectAll` (`idSeed`: an optional non-zero u64 seeding the remap instead of the session's random source, Foundation.md "Session random source") |
| `selection` | `set [refs]`, `add [refs]`, `clear`, `selectReferencing [asset]` |
| `entity` | `create [parent, name, template, id]` (`id`: an optional persistent ID for the new entity, refused with `EDITOR_ERROR_COMMAND_ARGS` when it already exists in the document), `rename [entity, name]`, `reparent [entities, parent, before, keepWorld]`, `hide [entities]`, `lock [entities]` |
| `component` | `add [entity, type, value]`, `remove [entity, type]`, `setProperty [entity, type, path, value]` |
| `transform` | `setWorld [entities, position, rotation, scale]`, `setLocal […]` |
| `prefab` | `createFromSelection [path]`, `createVariant [prefab, path]`, `open [prefab]`, `close`, `revertOverride […]`, `revertAll [instance]`, `applyOverrides […]`, `applyAll [instance]`, `unpack [instance]`, `replace [instance, prefab]` |
| `asset` | `import [paths, folder]`, `reimport [guids]`, `setSettings [guid, settings, platform]`, `rename [guid, name]`, `move [guids, folder]`, `delete [guids, replacement]`, `duplicate [guid]`, `emptyTrash`, `validate [guids]` |
| `play` | `start`, `startAnother`, `stop`, `stopAll`, `pause`, `resume`, `step [ticks]`, `focus [world]`, `applyToDocument` |
| `script` | `reloadModule [module]`, `consoleEval [text, world]` |
| `schema` | `validate [path]`, `previewMigration [path]`, `apply [path]`, `commitLock` |
| `migration` | `plan [root]`, `apply [root, entries]`, `restoreBackup [root, batch]` |
| `terrain` | `stroke [document, kind, dabs, settings]`, `addTile`, `removeTile`, `addLayer`, `removeLayer`, `moveLayer`, `create [desc]`, `regenerate [document]` |
| `camera` | `frameSelection`, `setView [viewport, camera]`, `bookmarkStore [n]`, `bookmarkGo [n]`, `scheme.unity`, `scheme.unreal` |
| `gizmo`, `snap` | the toolbar's toggles and increments |
| `sourceControl` | `refresh`, `diff [path]`, `revert [paths]`, `stage [paths]`, `unstage [paths]`, `resolve [path, side]`, `commit [paths, message]`, `lock [path]`, `unlock [path]` |
| `physics` | `dropToRest [entities]` (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Drop to rest") |
| `editor` | `commandPalette`, `animateEditWorld.toggle`, `restartRuntime [faultAtStep]` (`faultAtStep` 0 … 8, default 0: the test fault point that makes that step of Pipeline.md "Runtime restart" fail), `screenshot [viewport, path]`, `exit [save or discard]`, `commands.dump [path]`, `cancelPending` (the pending asynchronous record) |
| `cook` | `restartServer`, `cookPlaySet` |
| `devtools` | `toggleProfiler` |

## Automation scripts

### Automation script format

An automation script drives an editor session the way a user would, step by step, through commands (inventory item Z2-I-EDIT-13). It is the W-Editor workload's "editor automation script that triggers each operation" (`Zenith2/Docs/Budgets.md` → "Workload definitions"), the driver of the editor's integration tests, and the rehearsal of the walkthroughs. File `*.editorscript.json`, schema `zenith2.editor-script` version 1:

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | | `zenith2.editor-script`, 1 |
| `name` | string | shown in the report |
| `game` | string | the game the script expects; a mismatch with `--game` fails the script at step 0 |
| `timeoutSeconds` | integer | the whole script's wall-clock limit (default 600) |
| `seed` | integer | optional, a non-zero u64: the session's random seed when `--random-seed` is not given (Foundation.md "Session random source"); every committed test, rehearsal and W-Editor script carries one |
| `defaults` | object | `{ "waitTimeoutSeconds": 60 }` |
| `steps` | array of step objects | executed in order; each has `"step"` (the kind) and its members |

### Step kinds

| Step | Members | Behaviour |
|---|---|---|
| `command` | `id`, `args` | execute a command (`EDITOR_COMMAND_FLAG_AUTOMATABLE` required); a failure fails the step |
| `openScene` | `path`, `additive` | `scene.open`, then wait for `sceneOpened` |
| `select` | `objects` (persistent IDs or hierarchy paths `"/Root/Child"`) | `selection.set` |
| `setProperty` | `entity`, `type`, `path`, `value` | `component.setProperty` |
| `transform` | `entities`, `translate`, `rotateDegrees`, `scale`, `space` | `transform.setWorld` with the deltas applied to the current values |
| `terrainStroke` | `document`, `kind`, `dabs` (array of `{ x, z, radius, strength, hardness }`), `settings` | `terrain.stroke`; records the preview-latency timestamps |
| `play`, `stop`, `pause`, `step` | `ticks` for `step` | the play commands; `play` waits for `playStarted` |
| `waitTicks`, `waitFrames` | `count`, `world` | wait until the play world's tick index or the frame index advanced by `count` |
| `waitEvent` | `event`, `match` (member filters), `timeoutSeconds` | § "Waiting and events" |
| `writeFile` | `path` (game-relative), `text` or `fromFixture` | write bytes to a source file outside the editor's document layer (it simulates an external save for hot-reload timing); the path must lie under the game directory or the test fixture root |
| `reimport` | `guids` or `paths` | `asset.reimport` |
| `measureBegin`, `measureEnd` | `label`, `operation` (`sceneOpen`, `reimport`, `scriptReload`, `shaderReload`, `playStart`, `terrainPreview`, `animationPreview`) | § "Operation timing" |
| `assert` | `kind` and its members | § "Assertions" |
| `undo`, `redo` | `count` | the history commands |
| `screenshot` | `viewport`, `path` | `editor.screenshot` (no-op headless, reported) |
| `log` | `text` | a report line |
| `exit` | `documents` (`save` or `discard`) | end the script; the shutdown sequence |

Extensions add step kinds through `RegisterAutomationStep` (for example AUDIO's audition steps), each with its member schema; the runner rejects an unknown step kind before running the first step.

### Waiting and events

The editor's event bus (`Zenith_EditorEvents`) publishes typed events with a monotonic timestamp; automation waits on them, the operation timing reads them, and panels subscribe to them:

| Event | Members | Published |
|---|---|---|
| `sceneOpened` | `path`, `entities` | at `OnFrameEnd` of the first editor frame in which the scene's projection is applied and the frame that renders it was submitted (or, headless, after the projection) |
| `documentSaved` | `paths`, `batch` | after `SaveDocuments` succeeded |
| `historyChanged` | `undoCount`, `redoCount` | after every record, undo, redo or eviction |
| `historyCommitted` | `record` | a record (synchronous, or asynchronous once its requests completed) was committed by Execute, Undo or Redo (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Asynchronous changes") |
| `historyFailed` | `record`, `error` | an Execute, Undo or Redo failed; for an asynchronous record, after its compensation finished |
| `selectionChanged` | `count`, `active` | after every change |
| `playStarted` | `world`, `tick` | the play world's first tick ran |
| `playStopped` | `world` | the world was destroyed |
| `reimportVisible` | `guids` | the reloaded assets were drawn (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Reimport timing") |
| `scriptReloadLive` | `world`, `module`, `generation`, `tick` | the reloaded module ran its first tick (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload timing") |
| `shaderReloadVisible` | `program` | the frame after the last replacement PSO swapped |
| `terrainPreviewVisible` | `document`, `strokeOrdinal` | the frame drawing the dab's preview |
| `animationPreviewVisible` | `document` | the first frame drawing the recompiled graph's pose |
| `runtimeRestarted` | — | a runtime restart completed (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart") |
| `cookIdle` | — | the cook server has no request running and no watch run pending |
| `schemaApplied`, `migrationApplied` | `paths` | the respective flows succeeded |
| `notification` | `severity`, `text` | every toast |
| `autosaveWritten` | `documents` | an autosave round completed |

`waitEvent` waits until an event of the named kind whose members match the filters is published after the step started, or fails the step at its timeout (`EDITOR_ERROR_AUTOMATION_TIMEOUT`).

### Assertions

| `assert` kind | Members | Passes when |
|---|---|---|
| `property` | `entity`, `type`, `path`, `equals`, `tolerance` | the document value (or with `world`, the play world's value) equals within the tolerance (numbers) or exactly |
| `entityCount` | `world` or `document`, `equals` | the count matches |
| `documentBytes` | `path`, `sha256` or `equalsFile` | the document's canonical bytes have the digest or equal the fixture file |
| `noDiagnostics` | `severity` (`error` default), `source` | the diagnostics panel holds none of that severity and source |
| `dirty` | `path`, `equals` | the document's dirty state |
| `undoDepth` | `equals` | the undo stack's record count |
| `editWorldHash` | `equalsStep` (a label of an earlier `record`) or `equalsHex` | `HashTickState` of the edit world equals |
| `record` | `label` | stores the edit world hash and every open document's digest under the label for a subsequent assertion |

A failed assertion fails the step, writes the expected and actual values to the report, and, with `--exit-after-script`, ends the process with exit code 5 (`EXIT_CODE_TEST_FAILED`) after the script's remaining steps are skipped.

### Running a script

- The runner (`Zenith_EditorAutomation`) runs in the editor frame's step 2, one step per frame at most, a step starting only when the previous one completed, so a script exercises the same frame structure as a user. A `command` step completes when its command's record commits: a synchronous command's record commits within the step's frame (`historyCommitted` is published that frame), and an asynchronous one's (an import, a settings apply, a rename, a schema apply, a `.meta` migration) when its requests complete, the runner waiting for `historyCommitted` with the record ID `Run` returned and failing the step on `historyFailed` for it; a command that changes no document completes at once. When the step's `waitTimeoutSeconds` expires first, the runner does not fail the step at once: it calls `Zenith_EditorHistory::CancelPending` and keeps waiting, one step per frame, for the record's `historyFailed` (its resolution and compensation, Foundation.md "Asynchronous changes"), then fails the step with `EDITOR_ERROR_AUTOMATION_TIMEOUT` naming the record and its label; so the history is not pending when the next step starts, and a subsequent document-changing step never fails with `EDITOR_ERROR_HISTORY_PENDING` for this step's record. The script's own `timeoutSeconds` still bounds the whole run. Waits complete on their event.
- A script is untrusted input: it is parsed with the default limits, validated whole before the first step (step kinds, members, kinds of values, paths), and its `writeFile` step accepts paths only under the game directory or `ZENITH_TEST_FIXTURE_ROOT`.
- Scripts run in windowed and headless sessions alike; the headless run is the CI form (`Zenith2/Docs/Design/EDIT/Tests.md` → "Headless editing tests").

### Automation report

`zenith2.editor-report` version 1, written at script end (and on a crash-free failure) to the path of `--automation-report`:

| Key | Type | Meaning |
|---|---|---|
| `$schema`, `schemaVersion` | | `zenith2.editor-report`, 1 |
| `script`, `game`, `editorBuildId`, `headless`, `rhi` | strings, bool | identification |
| `result` | string | `passed`, `failed`, `timeout` |
| `steps` | array | per step: `{ "index", "step", "result", "durationMs", "message" }` |
| `measurements` | array | per `measureEnd`: `{ "label", "operation", "startMonotonicUs", "endMonotonicUs", "durationMs" }` |
| `assertions` | array | per failed assertion: `{ "index", "kind", "expected", "actual" }` |

The measurements are the MP-EDIT samples the W-Editor budget runs collect (§ "Measurement procedure").

## Live link endpoint

### Endpoint security

The live link (the "editor live link" remote tool of `Zenith2/Docs/Architecture.md` → "Security architecture": development only, loopback by default, a per-session token, absent from Shipping) is EDIT's automation endpoint:

- Opened only with `--automation-endpoint`; it binds `127.0.0.1` alone (never `0.0.0.0` and never a non-loopback address, with no option to change it), on the given port or an ephemeral one.
- The first line of every connection must be `{"hello": 1, "token": "<64 hex>"}` with the token of the endpoint file (`Zenith2/Docs/Design/EDIT/Formats.md` → "Endpoint file"); any other first line, or a wrong token, closes the connection, counts `editor.liveLink.rejected` and logs the peer port; five rejections within 10 s close the endpoint for 60 s.
- One client at a time; a second connection is refused while one is authenticated.
- No Player contains the endpoint: it is code of the editor tool alone ([Z2-N-ARCH-958]).

### Endpoint protocol

After the handshake the client sends one JSON object per LF-terminated line (at most 1 MiB), each a step object of § "Step kinds" with an added `"id"`; the editor answers each with `{"id", "result": "ok" | "failed", "message", "data"}` when the step completes (waits complete on their events), and pushes `{"event": <kind>, …}` lines for events the client subscribed to with `{"subscribe": [kinds]}`. Steps from the live link run through the same runner as scripts, interleaved with nothing (a script and a live-link client cannot run at once; the second is refused with `EDITOR_ERROR_AUTOMATION_BUSY`).

## Operation timing

### Timed operations

The W-Editor iteration rows (`Zenith2/Docs/Budgets.md` → "Editor iteration rows", [Z2-N-BUDGET-321], [Z2-N-BUDGET-322]) and EDIT's other editor rows are measured as MP-EDIT samples between these events:

| Row | Start | End |
|---|---|---|
| Z2-B-EDIT-002 Script hot reload | the `.luau` file's last-write time (converted to the monotonic clock) | `scriptReloadLive` |
| Z2-B-EDIT-003 Shader blob reload | the `.slang` file's last-write time | `shaderReloadVisible` |
| Z2-B-EDIT-004 Asset reimport | the source file's last-write time, or the `asset.reimport` command | `reimportVisible` |
| Z2-B-EDIT-005 Scene open | the `scene.open` command | `sceneOpened` |
| Z2-B-EDIT-006 Play-in-editor start | the `play.start` command | `playStarted` |
| Z2-B-EDIT-012 Terrain sculpt preview latency | the end of the dab's `ApplyStroke` | `terrainPreviewVisible` |
| Animation graph preview latency (pending row, row AN12) | the graph document edit | the first frame drawing the recompiled graph's pose |

The file-time conversion uses the offset between the host's precise system time and the monotonic clock sampled at session start and re-sampled every 60 s, so a save in another process (the user's text editor) is timed from the moment it hit the disk.

### Measurement procedure

- W-Editor runs (`Zenith2/Docs/Budgets.md` → "Workload definitions", W-Editor: OpenWorldSlice opened in `ZenithEditor` from the `win-tools` build, before Stage 4 the minimal game) execute the committed script `Zenith2/Games/OpenWorldSlice/Workload/WEditor.editorscript.json` (before Stage 4 `Zenith2/Games/Minimal/Workload/WEditor.editorscript.json`; the `Workload` directory holds workload definitions, and row ED29 asks MOD's allowlist to admit `<W>.editorscript.json` there), which performs each timed operation five times with `measureBegin`/`measureEnd` around it and writes the report; MP-EDIT takes the median of the five and requires every repetition within the row's value plus its tolerance (`Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure": MP-EDIT; tolerance 10% of the value for editor iteration rows).
- Each repetition starts from the same state: the script reverts the edit it timed (an undo, or rewriting the original file bytes) and waits for `cookIdle` before the next.
- The script's order of sections, each a block of steps: (1) `openScene` of the workload scene, timed (`sceneOpen`) five times by closing and reopening it; (2) five `reimport` timings of a fixture mesh after a `writeFile` of new bytes; (3) five script reloads during play (`play`, a `writeFile` of the changed `.luau`, `measureEnd scriptReload`, `stop`), whose `play` and `stop` steps are also the play-world recompile triggers; (4) five shader reloads after a `writeFile` of a `.slang`; (5) five `play.start` timings (`playStart`), each followed by `stop`; (6) from Stage 3, five `terrainStroke` timings when the workload scene holds a terrain; (7) from Stage 4, five `animationPreview` timings, and five `schema.apply` steps of a fixture schema change, each followed by an `undo` (a runtime restart), which are the schema recompile triggers. GRAPH's W-Editor recompile measurement (A5 row GR14, Z2-B-GRAPH-001) reads the `CPU frame-graph schedule recompile` zone samples that sections (3), (5) and (7) produce: ten play-world creations and destructions from Stage 3, and ten schema re-registrations from Stage 4.
- The measured values are evidence for the rows; the rows' values themselves are set by their producers ("Stage 1 W-Editor row ticket for <operation>" and "Stage 1 per-system budget ticket for EDIT", `Zenith2/Docs/Design/EDIT.md` → "Budget rows").

## Usability walkthroughs

### Walkthrough rules

The four walkthroughs (inventory items Z2-I-EDIT-18 to -21; rows Z2-B-EDIT-007 to -010; [Z2-N-BUDGET-324]) are scripted task walkthroughs run by a person in the editor with MP-WALKTHROUGH (`Zenith2/Docs/Budgets.md` → "Editor iteration rows"): the operator follows the steps below on the reference desktop with a fresh checkout of the flagship content at the stated starting state, a stopwatch runs from the first step to the last, and the time is compared with the row. A walkthrough fails when the time exceeds the row's value plus its tolerance, when any step cannot be completed as written, or when the result does not pass its check. Each walkthrough also has an automation rehearsal (§ "Walkthrough rehearsal tests") proving every step exists and works, so the human run measures usability, never missing features.

### Walkthrough build a scene

Starting state: the editor open on OpenWorldSlice with an empty new scene. Steps:

1. Create a scene `Scenes/Walkthrough.scene.json` and open it.
2. Place a terrain: run the terrain creation wizard with 2 × 2 tiles and the default base layer.
3. Sculpt a hill and a valley with the raise and lower brushes (at least one stroke each), then smooth the ridge.
4. Place five instances of `Prefabs/Props/Rock.prefab.json` from the asset browser onto the terrain with surface snapping.
5. Add a directional light and a point light; set the point light's radius with its gizmo.
6. Add a camera entity and frame it on the hill with the gizmo.
7. Play, walk the default player around for 30 seconds, stop.
8. Save the scene.

Check: the saved scene re-opens with every placed object, and plays.

### Walkthrough author a prefab variant

Starting state: OpenWorldSlice open on its main scene; `Prefabs/Characters/Villager.prefab.json` exists. Steps:

1. Create a variant of `Villager` named `VillagerGuard.prefab.json`.
2. In the variant's prefab stage, change the mesh's material to `Materials/Guard.material.json`, add a `Zenith.Gameplay.Tag` component with the tag `Guard`, and scale the hat entity by 1.2.
3. Save the variant.
4. Place two instances of the variant in the main scene; override one instance's name and position.
5. Apply one instance's position override back to the variant with "Apply to prefab"; revert the other instance's name override.
6. Play, check the guards appear with the new material, stop, save.

Check: the variant document holds exactly the three overrides and the applied position; the scene holds the two instances with the remaining override.

### Walkthrough iterate a script

Starting state: OpenWorldSlice open, `Scripts/Gameplay/Door.luau` attached to a door entity in the main scene. Steps:

1. Play.
2. In the external text editor, change the door's opening speed constant and save.
3. Observe the running door use the new speed without stopping play (hot reload).
4. Introduce a syntax error, save, read the error in the diagnostics panel, fix it, save.
5. Add a new state field to the behaviour, save, and observe the reload migrate the running instance.
6. Stop play.

Check: no restart of play was needed; the diagnostics panel is clear at the end.

### Walkthrough reimport an asset

Starting state: OpenWorldSlice open on its main scene, which uses `Assets/Props/Crate.glb`. Steps:

1. In the DCC tool, modify the crate mesh and export over `Assets/Props/Crate.glb`.
2. Observe the crate update in the scene view (watch-driven reimport).
3. Change the crate's import settings in the asset inspector (scale 1.5, generate LODs off), apply, observe the update.
4. Change the crate texture's compression setting, apply, observe.
5. Undo the settings change, observe the crate return.

Check: the asset's record shows the final settings; no error in the diagnostics panel.

### Walkthrough rehearsal tests

Each walkthrough has an automation script under `Zenith2/Tools/ZenithEditor/Tests/Fixtures/Scripts/Walkthroughs/` that performs the same steps through commands on a fixture copy of the content (the DCC export replaced by a `writeFile` of a fixture `.glb`, the text editor by `writeFile` of the changed `.luau`), run headless by `Test_Walkthrough_BuildScene_Rehearsal`, `Test_Walkthrough_PrefabVariant_Rehearsal`, `Test_Walkthrough_IterateScript_Rehearsal` and `Test_Walkthrough_ReimportAsset_Rehearsal` (`Zenith2/Docs/Design/EDIT/Tests.md` → "Integration tests"); each asserts the walkthrough's check. The timed human run is the Stage 4 acceptance evidence (`needs-human`), the rehearsal its precondition.
