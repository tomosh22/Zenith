# EDIT design — editor foundation

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It specifies the editor process (`ZenithEditor`), its composition over `ZenithRuntime`, the editor session, the world model (edit, play and preview worlds), play in editor, the document model, the command and undo model, selection and the clipboard. Together these are the Stage 3 "editor foundation, before any authoring tool" of `Zenith2/Docs/Roadmap.md` → "Stage 3 Playable authoring slice": stable authoring identities (SCENE's, used here), a command system, undo transactions, document ownership and selection. Every authoring tool of `Zenith2/Docs/Design/EDIT/AuthoringTools.md` builds on it.

## Scope of this file

| Topic | Heading |
|---|---|
| the executable, its options, startup, the frame, shutdown | "Editor process" |
| what one opened game holds | "Editor session" |
| the edit world, play worlds and preview worlds | "Worlds in the editor" |
| starting, pausing, stepping and stopping play | "Play in editor" |
| documents, their states, saving, JSON edits | "Documents" |
| command records, undo, redo, transactions, merging, the memory bound | "Commands and history" |
| what is selected and how it survives edits | "Selection" |
| copy, cut, paste and duplicate | "Clipboard" |

Every type below lives either in a public root header of the tool library `ZenithEditorCore` (`Zenith2/Tools/EditorCore/`: the session, documents, commands, history, selection, clipboard, worlds, projection and play, ruling Q86 (a)) or in `Zenith2/Tools/ZenithEditor/Internal/` (the executable's process, composition and shell); the path comment of each code block says which (`Zenith2/Docs/Design/EDIT.md` → "Library and file layout"). Every fallible function returns `Zenith_Result<T, Zenith_EditorError>` unless stated, and every type carries a contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type". Types this file names without declaring are declared in `Zenith2/Docs/Design/EDIT/Declarations.md` (its "Declaration index" lists every one).

## Editor process

### Executable and command line

`ZenithEditor.exe` is a host tool built by `zenith_add_tool(ZenithEditor …)` in the `win` configure preset alone (`ZENITH_TOOLS_HOST`, `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"). One executable serves every game under `Zenith2/Games/`: it links `ZenithRuntime`, every module and every backend (`Zenith2/Docs/Architecture.md` → "Library dependency graph", tool table), so the game's selection is made at startup from the game's resolved composition record, never by rebuilding the editor.

<!-- elaboration -->
`Zenith_EditorCommandLine::Parse(int iArgc, const char* const* aszArgv, Zenith_EditorOptions& xOut, Zenith_Array<const char*>& axRuntimeArgvOut)` consumes the editor's own options and forwards every other argument, in order, to `Zenith_CommandLine::Parse` (MOD's runtime parser, `Zenith2/Docs/Design/MOD.md` → "Zenith_CommandLine.h"), so the runtime options (`--set`, `--headless`, `--timeout-seconds`, `--assets-root`, `--config-dump`, `--record`, `--replay`) keep their one spelling.

| Option | Argument | Meaning | Default |
|---|---|---|---|
| `--game` | `<Name>` | the game directory `Zenith2/Games/<Name>/` to open; required | — |
| `--scene` | `<game-relative path>` | open this scene document after startup (`Scenes/Main.scene.json`) | the session file's active scene, else `game.json`'s startup scene |
| `--automation-script` | `<path>` | run this automation script after startup (`Zenith2/Docs/Design/EDIT/Automation.md` → "Automation scripts") | none |
| `--automation-report` | `<path>` | write the automation report there | `<out>/EditorReports/<Name>/<UTC timestamp>.editorreport.json` |
| `--exit-after-script` | — | exit when the script finishes: code 0 on success, 5 (`EXIT_CODE_TEST_FAILED`) on a failed assertion or step | off |
| `--automation-endpoint` | `[<port>]` | open the live-link automation endpoint on `127.0.0.1` (`Zenith2/Docs/Design/EDIT/Automation.md` → "Live link endpoint"); port 0 or none picks an ephemeral port | off |
| `--recovery` | `ask`, `restore` or `discard` | what to do with recovery data from an earlier session that did not exit cleanly (§ "Startup sequence" step 13) | `ask` |
| `--no-cook-server` | — | do not start `ZenithCook --serve`; asset operations report `EDITOR_ERROR_COOK_SERVER_UNAVAILABLE` (tests of the document layer) | off |
| `--layout` | `default` or `session` | ignore (`default`) or load (`session`) the per-user layout file | `session` |
| `--user-dir` | `<dir>` | the per-user preferences directory (tests point it at a scratch directory) | `%LOCALAPPDATA%\Zenith2\Editor` |
| `--game-root` | `<dir>` | the directory holding `<Name>/game.json` (tests point it at a scratch copy of a game under `${ZENITH_OUT}/test-fixtures/EDIT/Games/`) | `<Zenith2>/Games` (§ "Host paths") |
| `--random-seed` | `<u64>` (decimal, or hexadecimal with `0x`) | the seed of the session's random source (§ "Session random source"); 0 is refused (`EDITOR_ERROR_USAGE`) | the automation script's `seed` member when a script is given and has one, else operating-system entropy |

- An unknown editor-looking option is never silently forwarded: an option beginning `--` that neither table names makes the runtime parser exit with code 2 naming it ([Z2-N-ARCH-860]), and `--game` missing or naming no directory holding `game.json` exits with code 2 and `editor: game '<Name>' not found under <games root>`.
- The game name is validated with MOD's name rule, `^[A-Z][A-Za-z0-9]{0,31}$` (the check `Zenith_PlanNewGame` reports as `NEW_GAME_ERROR_BAD_NAME`, `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Public API"), through the predicate `Zenith_IsValidGameName(Zenith_StringView)` that `ZenithNewGameScaffold` exports (row ED38), so the editor and MOD never hold two copies of the rule; a name that fails is `EDITOR_ERROR_USAGE` with `editor: '<Name>' is not a game name (^[A-Z][A-Za-z0-9]{0,31}$)`.
- `--recovery ask` with `--automation-script` and without an interactive window (headless) is resolved as `discard` and logged once, so an unattended run never waits on a dialog.

```cpp
// Zenith2/Tools/ZenithEditor/Internal/App/Zenith_EditorCommandLine.h
/// @contract value Zenith_EditorOptions
/// Ownership: Plain value filled by Zenith_EditorCommandLine::Parse; the pointers borrow argv, which outlives the process.
/// Thread affinity: IMMUTABLE after Parse; members non-const: Zenith_EditorCommandLine::Parse fills it field by field.
/// Invalidation: None.
/// Units: Port numbers; seconds.
/// Errors: None of its own; Parse validates every member.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorRecoveryChoice : uint8
{
	EDITOR_RECOVERY_CHOICE_ASK = 1,
	EDITOR_RECOVERY_CHOICE_RESTORE,
	EDITOR_RECOVERY_CHOICE_DISCARD,
};

struct Zenith_EditorOptions
{
	char m_acGame[33] = {};                       // the game name, validated by Zenith_IsValidGameName (MOD's rule, row ED38)
	const char* m_szGameRoot = nullptr;           // --game-root, or nullptr for <Zenith2>/Games
	const char* m_szScene = nullptr;
	const char* m_szAutomationScript = nullptr;
	const char* m_szAutomationReport = nullptr;
	const char* m_szUserDir = nullptr;
	bool m_bExitAfterScript = false;
	bool m_bAutomationEndpoint = false;
	uint16 m_uEndpointPort = 0;
	bool m_bNoCookServer = false;
	bool m_bLoadSessionLayout = true;
	Zenith_EditorRecoveryChoice m_eRecovery = EDITOR_RECOVERY_CHOICE_ASK;
	uint64 m_ulRandomSeed = 0;                   // --random-seed, or 0 (none given)
};

class Zenith_EditorCommandLine
{
public:
	static Zenith_Result<void, Zenith_EditorError> Parse(int iArgc, const char* const* aszArgv,
		Zenith_EditorOptions& xOut, Zenith_Array<const char*>& axRuntimeArgvOut);   // EDITOR_ERROR_USAGE
};
```

### Host paths

The editor finds every host path from the location of its own executable, so it needs no configure-time path compiled into it and runs from any build tree of the checkout:

1. `Zenith_GetEditorExecutablePath` (`GetModuleFileNameW(nullptr, …)` in `Tools/EditorCore/Internal/Windows/Zenith_EditorHostPaths_Windows.cpp`) gives `…/<checkout>/Zenith2/out/<checkout-hash>/<preset>/<Config>/bin/ZenithEditor.exe` (`Zenith2/Docs/Architecture/BuildSystem.md` → "Checkout hash and output root": executables live in `${ZENITH_OUT}/<configure-preset>/<Config>/bin/`).
2. `Zenith_ResolveEditorHostPaths`: `m_strOutRoot` (`${ZENITH_OUT}`) is the executable's directory with three components removed (`bin`, `<Config>`, `<preset>`); it is valid when `<m_strOutRoot>/declared_deps.json` exists, else `EDITOR_ERROR_HOST_PATH` and exit code 3 with `editor: not run from a build tree (no declared_deps.json above <dir>)`. `m_strZenith2Root` is `m_strOutRoot` with two components removed (`<checkout-hash>`, `out`). `m_strHostToolsDir` is the executable's directory, which holds `ZenithCook.exe` and `ZenithShaderCompiler.exe` of the same configuration (both host tools of the same `win` tree). `m_strGamesRoot` is `--game-root` or `<m_strZenith2Root>/Games`. The same rule serves `ZenithEditor_Tests`, `ZenithEditorCore_Tests` and the integration suites, whose executables sit in the same `bin/` directory.
3. Files read before `Zenith_Platform` exists (startup step 3) are read with `Zenith_ReadEditorHostFile` (`CreateFileW` and `ReadFile` of the whole file, at most the stated byte cap), the editor's one host read outside PLAT; every read after it goes through PLAT and the document storages.

### Startup sequence

`Zenith_EditorMain` (in `Internal/Zenith_ZenithEditorMain.cpp`, the tool's composition root) runs these steps; each step that fails unwinds every earlier step in reverse and returns the exit code shown, after writing one `editor.startup` log line naming the step and its cause.

| Step | Action | Failure → exit code |
|---|---|---|
| 1 | install the root heap with `Zenith_InstallRootHeap(&xHeap)` as the first statement ([Z2-N-CONTRACT-142]); as the second, dispatch the crash helper (row DV27, X245 (d); `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler lifecycle"): `int iExit = 0; if (Zenith_RunCrashHelperIfRequested(iArgc, aszArgv, Zenith_CrashHelperIdentity{ "ZenithEditor", 0 }, iExit)) { Zenith_InstallRootHeap(nullptr); return iExit; }`, because the editor creates a crash handler whose Windows helper relaunches `ZenithEditor.exe` with `--zenith-crash-helper`, which `Zenith_EditorCommandLine::Parse` would otherwise refuse with exit code 2 (the build identity is 0, as DEVT's `ZenithPlatformTests` main passes: the editor's per-game build identity is computed only at step 4, and the helper derives nothing from it but the crash directory's game folder `ZenithEditor`; the crash handler the runtime creates at its own startup step 7 carries the same values, because `Zenith_EditorApp` passes `Zenith_ToolRuntimeDesc::m_xCrashIdentity = { "ZenithEditor", 0 }` to `Zenith_RuntimeToolAccess::Create` (§ "Runtime tool access"), as `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler lifecycle" requires of `Zenith_CrashHandlerDesc::m_xIdentity`); then install the profiler hooks, the only other mutable globals a composition root may hold (`Zenith2/Docs/Contracts.md` → "No mutable global state"), exactly as the Player main does; create the `Zenith_LogRouter` and the `Zenith_OOMPressureFlag` the runtime borrows ([Z2-N-ARCH-876]) | a helper run returns its own `iExit` |
| 2 | `Zenith_EditorCommandLine::Parse`, then `Zenith_CommandLine::Parse` over the forwarded arguments | 2 |
| 3 | § "Host paths"; then read `<m_strOutRoot>/Games/<Name>/composition.json` (`Zenith2/Docs/Design/MOD.md` → "The composition.json record") with `Zenith_ReadEditorHostFile` (at most 1 MiB) and `Zenith_JsonDocument::Parse` under the default limits | 3: `composition.json missing; build the game once so zenith_add_game writes it`, or the host-path message |
| 4 | build the editor composition (§ "Editor composition"); verify its composition hash against the record's `compositionHash.windows` | 3: `composition.json is stale or names a module the editor does not link; reconfigure` |
| 5 | `Zenith_RuntimeToolAccess::Create` (§ "Runtime tool access"): runtime startup steps 4–15 and 18 with the tool host's differences; it fills the `Zenith_ToolServices` the root hands on by constructor injection | the runtime's code (`Zenith_StartupReport::m_eExitCode`) |
| 6 | create the editor's `Zenith_MigrationRegistry`, `Zenith_RegisterEditorDocumentTypes` (§ "Storage roots and the migration registry"), `Freeze` | 3 |
| 7 | create the storage roots: `Zenith_EditorStorageRoots::Create` with one `Zenith_HostDocumentStorage` (writer mode `"editor"`, game `<Name>`, platform `"windows"`) and one `Zenith_DocumentMigrator` per root, `m_szBatchTimestamp` from the host clock | 1 |
| 8 | `Zenith_EditorStorageRoots::RecoverAll` (`RecoverPendingJournals` on every root), before any document is opened; the report is kept for the migration panel's "Resume" view (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow") | a `DOCUMENT_ERROR_JOURNAL_CORRUPT` or `RECOVERY_CONFLICT` opens that root read-only with the error shown (no exit); `STORAGE_LOCKED` waits as the migrator does, then opens the root read-only |
| 9 | take the session lock (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Session lock and crash detection") | 1: `another editor session holds Games/<Name>; close it first` |
| 10 | create the cook client unstarted (`Zenith_EditorCookClient::Create` with `<m_strHostToolsDir>/ZenithCook.exe`), unless `--no-cook-server`; create the editor session (§ "Session object"): the document table, the history, the selection, the main edit world and its proxies; create the host watchers of § "Host watchers" (one per storage root) | 4 (an allocation or world creation failure); a host watcher that cannot be created is a warning, not a failure (§ "Host watchers") |
| 11 | start the cook client (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Cook client") | none: the client retries in the background and the asset browser shows the server state |
| 12 | create the shell: the ImGui context, the panel host, the extension table, the view extensions (registered with `Flux_Renderer::RegisterViewExtension` now, on the main thread with no frame running, row ED33), the input claimant, the cache consumer (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor shell") | 1 |
| 13 | recovery: when the earlier session left autosaves (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Recovery on startup"), apply `--recovery` | none |
| 14 | open the scene (`--scene`, else the session file's active scene set, else `game.json`'s `startupScene`) | none: a failure leaves an empty edit world and shows the document error |
| 15 | start the automation script and the live-link endpoint when requested | 2 for a malformed script |
| 16 | the editor frame loop (§ "Editor frame") | — |

### Editor composition

<!-- elaboration -->
The Player's composition is a generated constant (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Generated composition TU template"); the editor builds the same `Zenith_GameComposition` value at startup from the record and the editor's module catalogue, so the runtime it creates is the game's runtime:

1. `m_szGameName`, `m_szGameVersion`, `m_xSettings.m_szStartupScene` and the script-recovery members from the record (`name`, `version`, `startupScene`, `scriptRecovery`).
2. `m_axModules`: for each name of `modules`, in the record's order (already canonical, `Zenith2/Docs/Design/MOD.md` → "Canonical module order"), the catalogue row whose `MODULE_NAME` equals it and whose backend matches `physics.windows` or `online.windows`; a name with no row fails step 4.
3. `m_pfnCreateWorld` and `m_szECSBackendName` from `ecs` (`Custom`, `Flecs`, `EnTT`).
4. `m_axRHIBackends`: Vulkan and D3D12 (both linked; the runtime choice is the runtime's configuration).
5. `m_axCodecs`: zlib and zstd.
6. `m_ulCompositionHash`: computed by `Zenith_ComputeEditorCompositionHash` (`Internal/App/Zenith_EditorComposition.cpp`), the C++ form of the algorithm of `Zenith2/Docs/Design/MOD.md` → "Composition hash" for the windows platform over the values above (the LF-joined lines, SHA-256 with CORE's `Zenith_Sha256`, the first eight bytes little-endian); it must equal the record's `compositionHash.windows` read as that section's printed spelling, else step 4 fails. `Test_EditorComposition_HashMatchesMinimalGolden` pins the function to MOD's "Hash golden" (`Games/Minimal`: printed spelling `704cef4b6c1da71a`). `m_ulBuildIdentity` is computed by "Build identity" with the preset name `win`.

The editor never invents a module selection: a game whose `game.json` changed since its last configure has a stale record and the editor refuses it with the message of step 4, which names the reconfigure.

### Module catalogue

```cpp
// Zenith2/Tools/ZenithEditor/Internal/App/Zenith_EditorModuleCatalogue.h
/// @contract value Zenith_EditorModuleRow
/// Ownership: constexpr data in Internal/App/Zenith_EditorModuleCatalogue.cpp.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3; a row is added in the diff that adds the module or backend library to ZenithEditor's DEPENDS.
/// @endcontract
struct Zenith_EditorModuleRow
{
	const Zenith_ModuleDescriptor m_xDescriptor = {}; // MOD's descriptor: name, interface version, entry points, backend name, deps
	const char* const m_szLibrary = nullptr;      // the module or backend library ("ZenithPhysics_Jolt"), for the catalogue test
	const char* const m_szPlatformBackend = nullptr; // the backend this row binds ("Jolt", "PhysX", "Offline"), or nullptr
};

Zenith_Span<const Zenith_EditorModuleRow> Zenith_GetEditorModuleCatalogue();
// Zenith_EditorECSBackendRow and Zenith_GetEditorECSBackends: Zenith2/Docs/Design/EDIT/ToolDeclarations.md "ECS backend row".
```

`Zenith_EditorModuleCatalogue.cpp` is the one editor file that includes every module root header (`Zenith_Module_<MODULE_NAME>.h`) and every backend factory header; it is a composition-root TU, like the Player's generated TU and the tool mains of ZenithCook, which is why the backend headers are admitted there (cross-area row C9). It holds one row per module, and for a module with a backend family one row per backend (Physics × {Jolt, PhysX}, Online × {Offline}), each row's `m_pfnStart` being the same backend-binding thunk form the generator emits (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Generated composition TU template"). Rows grow with `ZenithEditor`'s `DEPENDS` (`Zenith2/Docs/Design/EDIT.md` → "Dependency growth rule"): the diff that adds a module or backend library to the list adds its row.

`Test_EditorCatalogue_CoversDeclaredModules` reads the build's own record, `<m_strOutRoot>/declared_deps.json` (`Zenith2/Docs/Architecture/BuildSystem.md` → "The declared_deps.json record", located by § "Host paths" from the test executable), and asserts both directions: every library in the `ZenithEditor` record's `depends` whose `libraries[]` record has kind `MODULE` has a catalogue row with that record's `moduleName`, and every `BACKEND` library there whose `backendOf` is a module's API library has a row with its `backendName`; and every catalogue row's `m_szLibrary` is in that `depends` list. So the test passes at every stage of the growth rule and fails the diff that adds a library without its row, or a row without its library.

### Runtime tool access

MOD adopted row ED1: the tool host's access to the runtime is MOD's `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeToolAccess.h`, declared in `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access" (declarations Z2T-MOD-082, implementation Z2T-MOD-083), compiled in every configuration and reachable only from tool sources (rule 7 of `TestSeparationCheck`, cross-doc items X258 and X370). EDIT uses it exactly as declared there; this file restates only what the editor depends on:

- `Create(xCommandLine, xComposition, xRootHeap, xLogRouter, xOOMPressureFlag, xProfiler, xDesc, xHooks, xServicesOut, xReportOut)`: the log router, the OOM pressure flag and the `Zenith_Profiler` are process objects `Zenith_EditorMain` creates (startup step 2) and that outlive every runtime, so a runtime restart passes the same three again.
- `Zenith_ToolRuntimeDesc`: `m_bHiddenWindow` (render tests), `m_pxRHIFaultInjector` (tests only), `m_xCrashIdentity` (the editor passes `{ "ZenithEditor", 0 }`, the values its main gave the crash helper dispatch) and `m_pfnCompileConsole` (the editor passes `Zenith_EditorCompileConsole`, a `Zenith_PfnScriptCompileConsole` over `Zenith_ScriptCompileModule`, for `ConsoleEvaluate`).
- `Zenith_ToolFrameHooks` has seven members: `OnInputFrame`, `OnSchemaWindowResult` (inside prologue step 6), `OnPrologueEnd`, `OnWorldHalted`, `OnScriptModuleLive`, `OnWorldScheduleFailed(uWorldTag, const Zenith_ScheduleError&)` (a tool world whose schedule failed to freeze at prologue step 8 was destroyed by the runtime; delivered after the script events) and `OnFrameEnd`.
- `Zenith_SchemaWindowResult` is MOD's `{ m_bApplied, m_uFileCount, m_uFailedFileIndex, m_xError, m_ulRegistryRevision }`; `QueueSchemaJson` holds at most 64 files and 8 MiB in total.
- `GetRequestedExitCode` returns `EXIT_CODE_NORMAL` before any request and `EXIT_CODE_DEVICE_LOST` for the renderer's device-loss exit.
- The five tool-access errors and the editor's mapping: `RUNTIME_ERROR_WORLD_CREATE` (a module failed, or a 65th tool world: `m_uModuleIndex` 0xFFFF) → `EDITOR_ERROR_RUNTIME` (`EDITOR_ERROR_DOCUMENT_LIMIT` is refused earlier by the world set's budget of 64); `RUNTIME_ERROR_WORLD_SCHEDULED` and `RUNTIME_ERROR_REGISTRATION` (an `AddToolExtractor` after the world's registry froze, or a duplicate extractor name) → `EDITOR_ERROR_RUNTIME`, a creation failure of that world; `RUNTIME_ERROR_QUEUE_FULL` → `EDITOR_ERROR_SCHEMA_QUEUE_FULL`; `RUNTIME_ERROR_FRAME_FAILED` from `RunOneFrame` → exit code 1 (§ "Editor run loop").
- At most 64 tool worlds exist at once; the editor's world set budgets them (one main edit world, the open prefab stages, at most `editor.play.maxWorlds` play worlds and `editor.preview.maxWorlds` preview worlds).
- `CreateWorld({ m_ePurpose, m_ulWorldSeed })` chooses the origin from the purpose (`WORLD_ORIGIN_EDITOR_PLAY` for PLAY, `WORLD_ORIGIN_STARTUP` otherwise) and, for `WORLD_PURPOSE_EDIT`, sets the edit-world flag ECS defines (`Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world"); the editor never sets that flag itself. `AddToolExtractor` for a world must follow its `CreateWorld` before the next prologue step 8.

- The services are handed over once: `Create` fills `xServicesOut`; `Zenith_EditorMain` keeps the struct in `Zenith_EditorApp` and passes the individual references to the session, the shell, the cook client and the panels' context by constructor; there is no `GetServices` getter. After a runtime restart (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart") the new `Create` fills a new struct and the root re-injects it (`RebindRuntime`), which is a hand-over by the composition root, not a lookup.
- `FindModuleInstance` exists for AUDIO's tools-only accessor (row AU3) and is reachable only from tool sources (X258).
- The edit and preview worlds are told from play worlds by their world purpose (X243 (b), `Zenith_WorldAttachContext::GetWorldPurpose()`), never by an origin.

### Tool access calls per hook

Every editor call into the runtime is made either from a hook inside `RunOneFrame` or from `Zenith_EditorApp::Run` between two `RunOneFrame` calls (§ "Editor run loop"). Which call is legal where follows from what the frame is doing: prologue step 8 compiles the schedule, `RunFrame` runs every world, prologue step 6 is the hot-reload window, and prologue step 4 runs the memory governor's callbacks. A call outside its legal positions is a programmer error that the runtime reports with `ZENITH_CHECK` (it knows which hook it is calling), and EDIT makes none:

| Call | `OnInputFrame` (editor step 2, before prologue step 4) | prologue step 4 (governor callbacks, `OnPressureLevelChanged`) | `OnSchemaWindowResult` (inside prologue step 6) | `OnPrologueEnd` (editor step 4, after prologue step 8) | `OnWorldHalted`, `OnScriptModuleLive`, `OnWorldScheduleFailed` (editor step 6, MOD's `SCRIPT_EVENTS`) | `OnFrameEnd` (editor step 6) | between frames (`Run`, after `RunOneFrame` returned) |
|---|---|---|---|---|---|---|---|
| `CreateWorld` | yes: the world joins this frame's schedule at step 8 | no | no | no: the schedule is compiled | no: record only | yes: joins the next frame | yes |
| `DestroyWorld` | yes | no | no | no | no | yes | yes |
| `AddToolExtractor` | yes (with the world's `CreateWorld`, before step 8) | no | no | no | no | yes | yes |
| `QueueSchemaJson` | yes: runs in this frame's step 6 | no | no | no | no | yes: the next frame's step 6 | yes |
| `SetInputWorld` | yes | no | no | no | no | yes | yes |
| `FindModuleInstance` | yes | no | no | yes | no | yes | yes |
| ECS out-of-tick application (`ApplyOutOfTick`, the projection, the proxies, play-world edits) and world-inspector reads | yes | no | no | no | no | no | yes |
| `RunOneFrame` | no (no hook reenters the loop) | no | no | no | no | no | yes: `Run` alone |
| destroying the runtime, `Zenith_RuntimeToolAccess::Create` (the runtime restart) | no | no | no | no | no | no | yes: `Run` alone, Pipeline.md "Runtime restart" |

The table is MOD's call-position table (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Runtime tool access", "Call positions"), which the runtime enforces with `ZENITH_CHECK`.

Where EDIT makes each call:

| Caller | Call | Position |
|---|---|---|
| `play.start` (Play) | `CreateWorld`, `AddToolExtractor`, `SetInputWorld` | `OnInputFrame` (the command executes in editor step 2) |
| Stop, a halted script world, the game's quit | `DestroyWorld` | the next `OnFrameEnd` (`OnWorldHalted` records the halt; the play controller acts at `OnFrameEnd`) |
| the preview host (opening, LRU eviction, release marked by the cache consumer) | `CreateWorld` when a panel opens (`OnInputFrame`); `DestroyWorld` in `Zenith_EditorPreviewHost::Update` (`OnFrameEnd`) | as listed |
| the schema apply | `QueueSchemaJson` | `OnInputFrame`; the result is recorded in `OnSchemaWindowResult` and acted on by `Zenith_EditorSchemaModel::Poll` in the next `OnInputFrame` (`ApplySchemaRevision` on the edit worlds, the preview worlds' re-creation, the record's commit), which is ECS's "at the next frame" (`Zenith2/Docs/Design/ECS/Schemas.md` → "Editor re-registration" step 4) |
| the cache consumer | none: it marks idle previews and decoded thumbnails for release, frees pick meshes, and returns (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Cache consumer header") | prologue step 4 |
| the schema undo and redo (a runtime restart) | the restart | between frames: the history's `Poll` requests it through `Zenith_EditorRuntimeHost::RequestRuntimeRestart`, `Run` performs it after `RunOneFrame` returned |
| the audio panels | `FindModuleInstance` | `OnInputFrame` |
| a tool world the runtime destroyed for a schedule failure | none: the world set drops the entry, its projection and proxies, and notifies | `OnWorldScheduleFailed` records it; the world set acts at `OnFrameEnd` |

### Editor frame

The editor drives the runtime one frame at a time; there is no second loop and no second frame graph. One editor frame, all on the main thread except the frame-graph nodes:

| Order | Step | Owner |
|---|---|---|
| 1 | `Zenith_RuntimeToolAccess::RunOneFrame` begins: the runtime's prologue steps 1–3 (platform pump) | MOD |
| 2 | hook `OnInputFrame`: the ImGui platform adapter feeds the frame's input (`Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui platform adapter"); `ImGui::NewFrame`; the host watchers' `Drain` and its routing (§ "Host watchers"); the cook client's `Poll`; the history's `Poll` (a pending asynchronous record, § "Asynchronous changes"); every visible panel draws (reading documents and, out of tick, world inspectors); the automation runner advances one step when its wait condition holds; queued commands execute (§ "Executing a command"); out-of-tick edit-world projections apply (§ "Edit world projection"), then each edit world's proxies (§ "Editor proxy entities"); `ImGui::Render`; the draw snapshot is written (`Zenith2/Docs/Design/EDIT/Viewports.md` → "ImGui renderer"); the event bus dispatches | EDIT |
| 3 | prologue steps 4–8 (memory governor, renderer housekeeping, the hot-reload window with queued schema files and script reloads, transitions, recompile) | MOD, GAME, SCRIPT |
| 4 | hook `OnPrologueEnd`: terrain preview `Prepare` for each edit world holding an open terrain (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain live preview"), view requests staged for the editor extractor | EDIT |
| 5 | `RunFrame`: the ticks of every running world, then the epilogue, where each edit and play world's EDIT extractor writes its view requests and overlay records, concurrently with the render subgraph of the previous frame | GRAPH, EDIT nodes |
| 6 | hooks `OnWorldHalted` and `OnScriptModuleLive` for the script world events the runtime recorded (the play controller and the script-reload timing act on them here, on the main thread); hook `OnFrameEnd`: completion events (pick readbacks, play-world first tick, reload go-live) are timestamped for MP-EDIT (`Zenith2/Docs/Design/EDIT/Automation.md` → "Operation timing"); autosave and thumbnail work is scheduled; the preview host's `Update` (pooling, the releases the cache consumer marked); the event bus dispatches | EDIT |
| 7 | `RunOneFrame` has returned: `Zenith_EditorApp::Run` performs a runtime restart requested in this frame, then handles a false return or an exit request (§ "Editor run loop") | EDIT |

- The world inspectors are read in step 2 only, which is between `RunFrame` calls, so the inspector's rule "MAIN_THREAD, only while no frame of its world runs" holds (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldInspector header").
- A panel never blocks the frame: every operation longer than `editor.frame.syncBudgetUs` (default 4,000 µs) is a background operation with an operation handle and a progress line in the status bar.
- The editor frame's own CPU time (steps 2, 4 and 6) is the zone `Editor.Frame` with children per step and per panel, the provider `editor` of DEVT's overlay (`Zenith2/Docs/Design/DEVT/Overlay.md` → "Editor overlay").
- No editor object is ever called from a thread other than the main thread by the script world: the runtime's listener records flags on the observing thread, as its contract says ("it records flags the frame loop reads"), and the tool access delivers them through the hooks of step 6.

### Editor run loop

`Zenith_EditorApp::Run` (`Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Application header") is the one loop, on the main thread; each iteration is one `RunOneFrame` followed by the between-frames work:

1. `Zenith_RuntimeToolAccess::RunOneFrame(runtime)`; the hooks of § "Editor frame" run inside it.
2. An error result (a `Zenith_RuntimeError`): log it under `editor.runtime`, write an autosave of every dirty document (§ "Device loss" step 2, from Stage 4), run shutdown steps 3–10 and return exit code 1.
3. A runtime restart requested in this frame (`m_bRestartRequested`, set by `RequestRuntimeRestart` during the history's `Poll` in step 2 of this frame): `RestartRuntime` (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart"), now that no hook of the old runtime is on the stack; its failure follows that table's step 9; on success the restart state becomes DONE, which the pending record's operation reads in the next frame's `Poll`. A restart is never performed from a hook.
4. `RunOneFrame` returned false: `GetRequestedExitCode`; `EXIT_CODE_DEVICE_LOST` → § "Device loss"; any other code → an exit request, as `RequestExit` (the window's close button and `editor.exit` call `RequestExit` too).
5. An exit request: the exit phase runs in the following iterations, which keep calling `RunOneFrame` (so the cook client, the history's `Poll`, the panels and the dialogs keep working): shutdown step 1 until the pending record settles, then step 2's prompt, drawn as an ImGui modal in those frames; when both are done `Run` returns, and the destructor performs steps 3–10. While the exit phase runs, every document-changing command is refused with `EDITOR_ERROR_PLAY_STATE` and the status bar shows "Closing: waiting for <label>".

### Device loss

A device loss the renderer cannot recover enters RHI's `EXITING` state (`Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine": it requests shutdown with `EXIT_CODE_DEVICE_LOST`, shows the platform message box, and raises `FATAL_CODE_DEVICE_LOST` if the process has not exited within 10 s). The editor learns of it as `RunOneFrame` returning false with `GetRequestedExitCode` = `EXIT_CODE_DEVICE_LOST`, and then runs no further frame:

1. A pending asynchronous record is left as it is (no frame runs to compensate it); its touched documents are in the autosave of step 2, and the next session's recovery offers them (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Recovery on startup").
2. Every dirty document is serialized on the main thread and written by the autosave writer, which is flushed with a deadline of 7,000 ms (`Zenith_EditorAutosave::FlushAllNow(7000)`); a document not written by the deadline is logged by path, and the writer's remaining queue is discarded so shutdown step 4 joins it without waiting for further files. Before Stage 4 (no autosave) this step logs the dirty documents' paths.
3. Shutdown step 2's prompt is skipped (the UI cannot render), and steps 3–10 run with the renderer lost (every renderer call they make, the extension unregistrations and texture destroys, is a no-op on a lost device, `Zenith2/Docs/Design/RHI.md` → "Device status state machine": every call returns `ZENITH_RHI_ERROR_DEVICE_LOST` and no submission reaches the device), with the waits of steps 3 and 6 replaced by a combined deadline: step 3 terminates a running git command at once (`Zenith_EditorChildProcess::Terminate`, no 30-second timeout) and joins `EditorSourceControl`; step 6 sends the cook server `shutdown`, waits at most 500 ms for its exit, then terminates it by closing its job object, and terminates the shader watcher at once (no 2-second wait).
4. `Run` returns `EXIT_CODE_DEVICE_LOST` (6).

The bound is the sum of the waits: at most 7,000 ms for the autosave flush (step 2), at most 500 ms for the cook server (step 6), and at most 1,500 ms for every other step together (the thread joins of steps 3, 4 and 6 after their child processes ended or their queues were discarded, and steps 7–10, which wait on nothing), so `Run` returns within 9 s of the loss, inside RHI's 10-second fatal fallback. The device-loss path measures it: a step that would still be waiting when 9 s have passed since the loss terminates what it waits for (a child through its job object) or abandons the join with an `editor.runtime` log line naming the step, and the exit continues.

`Test_EditorApp_DeviceLostAutosavesAndExitsSix` (`editor-render`, `z2-edit-cooked`, Stage 4) runs an in-process hidden-window editor on WARP with `Zenith_ToolRuntimeDesc::m_pxRHIFaultInjector` set to RHI's `Zenith_RHITestHooks` (`Zenith2/Docs/Design/RHI/Tests.md` → "Test files"), the real cook server child on its own scratch copy of `EditorFixture` (`--game-root`), one dirty document and a second loss injected with `SimulateDeviceLost()` within 60 s of the first (the loss RHI does not recover), and asserts that `Run` returns 6, that the document's `.zautosave` exists and carries its state ID, that no prompt was opened, that the cook server process has exited, and that the elapsed time from the loss to the return is under 9 s.

### Shutdown sequence

| Order | Step |
|---|---|
| 1 | (frames keep running, § "Editor run loop" step 5) a pending asynchronous record is driven by the history's `Poll` until it commits or fails: its inactivity timeout (`editor.history.asyncTimeoutSeconds`) cancels it, its outcome is resolved and it is compensated (§ "Asynchronous changes"); a record still `RESOLVING` or `COMPENSATING` one further `editor.history.asyncTimeoutSeconds` after that cancel becomes the "incomplete" fence of § "Asynchronous changes", whose handling autosaves every dirty document it touched, and shutdown continues; then stop play (§ "Stopping play"): every play world is destroyed at that frame's `OnFrameEnd` |
| 2 | (frames keep running) prompt for dirty documents (save, discard, cancel) in an ImGui modal; cancel aborts shutdown; in automation the script's `exit` step decides (`save` or `discard`); after device loss the prompt is skipped (§ "Device loss"); `Run` returns after this step |
| 3 | close the live-link endpoint and join `EditorLiveLink`; stop the automation runner; stop the source-control backend and join `EditorSourceControl` (after its running git command ends or its 30-second timeout terminates it) |
| 4 | flush the autosave writer and join `EditorAutosave`, then delete this session's autosave files when every document is clean (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Autosave") |
| 5 | write the per-user session and layout files (`Zenith2/Docs/Design/EDIT/Formats.md` → "Session file", "Layout file"); from Stage 4 only (`Zenith2/Docs/Design/EDIT.md` → "Untracked editor files") |
| 6 | send `shutdown` to the cook server and wait up to 5 s for its exit; terminate the shader watcher (`Zenith_EditorChildProcess::Terminate`, whose job object kills it) after closing its stdin and waiting 2 s; destroy both child processes, which joins their pipe threads |
| 7 | destroy the host watchers (each destructor joins its `EditorHostWatch` thread), then preview worlds, the edit worlds and their proxies, the session |
| 8 | unregister the view extensions, the input claimant and the cache consumer, destroy the shell |
| 9 | destroy the migrators, the storages and the registry; release the session lock |
| 10 | destroy the runtime (MOD's shutdown order) |

A crash at any step leaves the session lock released by the operating system and the autosaves on disk, which is exactly the state recovery detects.

## Editor session

### Session object

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorSession.h
/// @contract type Zenith_EditorSession
/// Ownership: Created by Zenith_EditorSession::Create as a Zenith_Owned, owned by Zenith_EditorApp for the process (or by an
///   integration suite); owns the document table, the history, the operation context, the selection, the clipboard, the event
///   bus, the command registry, the world set, the play controller and the migration and schema models; borrows the runtime,
///   the services, the storage roots and the cook client.
/// Thread affinity: MAIN_THREAD. Every member runs in the editor frame's step 2, 4 or 6.
/// Invalidation: References to documents and worlds it returns are invalid after the document closes or the world is destroyed.
/// Units: None.
/// Errors: Zenith_EditorError; document and scene errors are wrapped with their detail logged.
/// Reentrancy: NONE. Panels call the session; the session never calls a panel.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorSession
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorSession>, Zenith_EditorError> Create(
		const Zenith_EditorSessionDesc& xDesc);
	Zenith_EditorDocumentTable& GetDocuments();
	Zenith_EditorHistory& GetHistory();
	Zenith_EditorSelection& GetSelection();
	Zenith_EditorClipboard& GetClipboard();
	Zenith_EditorWorldSet& GetWorlds();
	Zenith_EditorPlayController& GetPlay();
	Zenith_EditorCommandRegistry& GetCommands();
	Zenith_EditorEvents& GetEvents();                 // the typed event bus panels and automation observe
	Zenith_EditorOperationContext& GetOperations();
	Zenith_EditorStorageRoots& GetStorageRoots();
	Zenith_EditorMigrationModel& GetMigration();
	Zenith_EditorSchemaModel& GetSchema();
	const Zenith_ToolServices& GetServices() const;    // the references injected at Create or RebindRuntime
	bool IsReadOnly() const;                          // startup step 8 found an unrecoverable journal
	Zenith_PersistentID GenerateAuthoringID();        // § "Session random source"
	void RegisterProjectionHandler(Zenith_TypeID xDocumentType, Zenith_EditorProjectionHandler& xHandler);
	// Pipeline.md "Runtime restart" step 7: the composition root re-injects the new runtime and services after re-creating
	// them; the session replaces its type registry and string table by the services' m_pxTypes and m_pxStrings, calls
	// Zenith_EditorDocumentTable::RebindTypes(*xServices.m_pxTypes), and calls
	// Zenith_EditorClipboard::RebindSystemClipboard(&xServices.m_pxPlatform->GetClipboard()), or with nullptr when the
	// session was created with a null m_pxSystemClipboard (headless, a suite).
	void RebindRuntime(Zenith_Runtime& xRuntime, const Zenith_ToolServices& xServices);
	// The active scene set: the scene documents projected into the main edit world, the first being the active one.
	Zenith_Result<void, Zenith_EditorError> OpenScene(Zenith_StringView strPath, Zenith_EditorOpenMode eMode);
	Zenith_Result<void, Zenith_EditorError> CloseScene(Zenith_EditorDocumentID xDocument);
	Zenith_Result<void, Zenith_EditorError> NewScene(Zenith_StringView strPath);
};

enum Zenith_EditorOpenMode : uint8
{
	EDITOR_OPEN_MODE_SINGLE = 1,     // replace the active scene set (prompts for dirty scenes)
	EDITOR_OPEN_MODE_ADDITIVE,       // add to the active scene set, projected into the same edit world
};
```

`Zenith_EditorSessionDesc` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Session description") carries the runtime, the tool services, the type registry and string table, the migration registry, the storage roots, the cook client, the runtime host, the host paths and the configuration values of `Zenith2/Docs/Design/EDIT.md` → "Editor configuration keys". A session whose desc has no runtime is a document-only session: it opens, edits, undoes and saves documents but creates no world (the Stage 2 pull-forward slice and the document tests use it). Every session has a type registry (`m_pxTypes`, with `m_pxStrings`, required non-null), because the SCENE document kind opens through `Zenith_SceneDocument::Load(…, const Zenith_TypeRegistry& xTypes, …)` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header") and the inspector, paste and JSON validators read reflection: a runtime session passes the services' pointers, and a document-only session passes a registry its caller owns, so the Stage 2 slice, which cannot name `Zenith_ToolServices`, opens scene documents.

### Session random source

<!-- elaboration -->
Every authoring ID the editor generates, and nothing else, is drawn from one `Zenith_RandomSource` the session owns (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"), through `Zenith_EditorSession::GenerateAuthoringID()`, which calls `Zenith_GeneratePersistentID(xRandom)`:

| Rule | Value |
|---|---|
| seed | `Zenith_EditorSessionDesc::m_ulRandomSeed`: the editor's `--random-seed`, else the automation script's top-level `seed` member (`Zenith2/Docs/Design/EDIT/Automation.md` → "Automation script format"), else 0; a test session passes a non-zero value of its own |
| construction | a non-zero seed: `Zenith_RandomSource(ulSeed, 0x45444954)` (the stream is the four ASCII bytes `EDIT`); 0: `Zenith_RandomSource::CreateFromEntropy()`, whose failure fails session creation with `EDITOR_ERROR_RUNTIME` |
| draws | `entity.create` (one ID per created entity, unless its `id` argument is given), `edit.paste` and `edit.duplicate` (the remap of `Zenith_RemapAuthoringIDs`, unless the `idSeed` argument is given, in which case the remap uses `Zenith_RandomSource(idSeed, 0x45444954)`), `prefab.createFromSelection` and `prefab.createVariant` (the new document's ID), `asset.duplicate` and the creation menus (a new document's `id`) |
| never | the editor proxy entities (derived IDs, § "Editor proxy entities"), play and preview world seeds (§ "Starting play" step 4), merge keys, viewport IDs |
| redo | a record stores the IDs its forward changes carry (scene edits hold the created entities' IDs), so redo reuses them and never draws |

So two sessions given the same seed and the same command sequence generate the same IDs, a windowed and a headless session of one script generate the same IDs (a windowed session has proxies, which draw nothing), and a test can state the IDs it expects by passing `id` or `idSeed` arguments explicitly. `Test_EditorSession_SeededIDsReproducible` runs one script of creates, pastes and duplicates twice with `--random-seed 7`, once windowed (two scene views) and once headless, and asserts the generated IDs are equal; with no seed, two runs generate different IDs.

### Storage roots and the migration registry

<!-- elaboration -->
A storage root is one directory the editor writes authoring documents into, with its own `Zenith_HostDocumentStorage`, writer lock, `Zenith_DocumentMigrator`, journal location and `.migration/backups/` tree (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "ZenithDocumentHost", "Storage-root writer lock"):

| Root | Directory | Documents |
|---|---|---|
| game | `Zenith2/Games/<Name>/` | every document of the game (scenes, prefabs, schemas, scripts are text and not documents, materials, animation, UI, audio, environment, `game.json`, bindings, `.meta`) |
| engine content, one per library | `Zenith2/Engine/<Lib>/Internal/Content/` for each library that has the directory | engine-owned authoring documents (engine bindings, default materials); opened read-only unless the preference `editor.engineContent.writable` is true |

The editor's `Zenith_MigrationRegistry` registers every document type, before `Freeze`, in the order of § "Editor document type registrations", which extends `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" with every owner's function, its header and its library. The calls are `Zenith_RegisterEditorDocumentTypes` (`Tools/EditorCore/Internal/Zenith_EditorDocumentTypes.cpp`, declared in `Zenith2/Docs/Design/EDIT/Declarations.md` → "Document type registration header"), so the integration suite of SCENE's Z2T-SCENE-089 registers exactly the editor's set. The tool-side registration functions are reached through `ZenithAuthoring` (ruling Q84 (a), `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs").

`Zenith.AssetMeta` is registered so the registry is ASSET's complete set and `.meta` files are typed, but the editor never migrates a `.meta` in process: the migration model removes every plan entry of that type from its in-process plans and plans and applies `.meta` documents through the cook server's `migratePreview` and `migrateApply` (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Planning on open"), and the asset inspector changes `.meta` settings through `setSettings` alone. The whole `.meta` document type moves into `ZenithAuthoring`, its member migrations included: `Zenith_RegisterAssetMetaMigrations`, the `Zenith_AssetMetaSettingsSource` interface (the editor passes null, so the importer-settings visitor leaves every settings object unchanged) and the member-migration table (`Zenith_AssetMetaMemberMigration`, `Zenith_GetAssetMetaMemberMigrations`, whose rows include CONTENT's `provenance` member) are ASSET's, declared in `Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h` with their bodies under `Zenith2/Tools/Authoring/Internal/Asset/` and their fixtures under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Asset/AssetMeta/` (`Zenith2/Docs/Design/ASSET/Migration.md` → "The meta document type"); ZenithCook keeps only its settings-source adapter, and a member migration CONTENT adds is a row of that ASSET table, placed with its fixtures under `AssetMeta/<member>/`.

`Test_EditorDocumentTypes_RegistrationOrderMatchesAsset` asserts the order and that `Freeze` succeeds over the editor's full registration.

### Editor document type registrations

<!-- assumption: pending ANIM, ENV, AUDIO, ASSET, UI, VFX, GEOM, AI, CINE, CONTENT, TWOD, WORLD, STREAM, PACK, MOD, ECS, PLAT (rows ED39 to ED49) -->
`Zenith_RegisterEditorDocumentTypes(Zenith_MigrationRegistry&)` (`Tools/EditorCore/Internal/Zenith_EditorDocumentTypes.cpp`) calls, in this order, which is `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" with the owners it does not list appended in the order their rows were raised, and returns the first failure unchanged:

| # | Function | Declared in | Library | Types |
|---|---|---|---|---|
| 1 | `Zenith_RegisterAssetMetaMigrations` | `Authoring/Zenith_AuthoringAsset.h` | ZenithAuthoring (ASSET, ED42) | `Zenith.AssetMeta` |
| 2 | `Zenith_RegisterSceneDocumentTypes` | `ZenithScene/Zenith_SceneDocumentTypes.h` | ZenithScene | scene, prefab |
| 3 | the schema document registration | `Authoring/Zenith_AuthoringSchema.h` | ZenithAuthoring (ECS, ED49) | `*.schema.json` |
| 4 | `Zenith_RegisterMaterialMigrations` | `Authoring/Zenith_AuthoringGeom.h` | ZenithAuthoring (GEOM, ED46) | `Flux.Material` |
| 5 | the `game.json` registration | `Authoring/Zenith_AuthoringGame.h` | ZenithAuthoring (MOD, ED49) | `game.json` |
| 6 | `Zenith_RegisterAnimationDocumentTypes` | `Authoring/Zenith_AuthoringAnimation.h` | ZenithAuthoring (ANIM, ED39) | the five animation documents |
| 7 | `Zenith_RegisterUIDocumentTypes` | `Authoring/Zenith_AuthoringUI.h` | ZenithAuthoring (UI, ED43) | UI's five documents |
| 8 | `Zenith_RegisterCinematicDocumentTypes` | `Authoring/Zenith_AuthoringCinematics.h` | ZenithAuthoring (CINE, ED45) | `Zenith.Cinematics.SequenceDocument` |
| 9 | `Zenith_RegisterEnvironmentDocumentTypes` | `Authoring/Zenith_AuthoringEnvironment.h` | ZenithAuthoring (ENV, ED40) | the nine environment documents |
| 10 | PLAT's bindings and configuration registration | `Authoring/Zenith_AuthoringPlatform.h` | ZenithAuthoring (PLAT, ED49) | bindings, configuration |
| 11 | `Zenith_RegisterAudioDocumentTypes` | `Authoring/Zenith_AuthoringAudio.h` | ZenithAuthoring (AUDIO, ED41) | bank, mixer |
| 12 | `Zenith_RegisterVFXDocumentTypes` (from Stage 5) | `Authoring/Zenith_AuthoringVFX.h` | ZenithAuthoring (VFX, ED44) | `Zenith.VFX.EffectDocument` |
| 13 | `Zenith_RegisterAIDocumentTypes` | `Authoring/Zenith_AuthoringAI.h` | ZenithAuthoring (AI, AI12) | AI's documents |
| 14 | `Zenith_RegisterContentDocumentTypes` | `Authoring/Zenith_AuthoringContent.h` | ZenithAuthoring (CONTENT, CT18) | `Zenith.Content.ImportPresetDocument` |
| 15 | `Zenith_RegisterTwoDDocumentTypes` | `Authoring/Zenith_AuthoringTwoD.h` | ZenithAuthoring (TWOD) | the 2D documents |
| 16 | `Zenith_RegisterWorldDocumentTypes` | `Authoring/Zenith_AuthoringWorld.h` | ZenithAuthoring (WORLD) | the four world documents |
| 17 | `Zenith_RegisterStreamingDocumentTypes` | `Authoring/Zenith_AuthoringStreaming.h` | ZenithAuthoring (STREAM, ED49) | `Zenith.Streaming.WorldDocument` |
| 18 | `Zenith_RegisterPackageDocumentTypes` | `Authoring/Zenith_AuthoringPackage.h` | ZenithAuthoring (PACK, ED49) | `Zenith.Package.RecipeDocument` |
<!-- /assumption -->

- A row is added to the function in the diff of the EDIT ticket that first registers that owner's types, which is blocked by the owner's move (rule 6 of `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"); a row whose owner has not moved is absent, so the function is correct at every stage and `Test_EditorDocumentTypes_RegistrationOrderMatchesAsset` asserts the rows present against this table's order.
- Rows 3, 5 and 10 have no function in `ZenithAuthoring` yet: MOD's, ECS's and PLAT's are unnamed. Rows 17 and 18 are declared in `ZenithAuthoring` by their owners (`Zenith_AuthoringStreaming.h`, `Zenith_AuthoringPackage.h`); a function in ZenithCook's `Internal/` could not be called, because the editor cannot include it ([Z2-N-ARCH-161], non-negotiable 1). Row ED49 asks each owner to declare its function in `Zenith_Authoring<Area>.h` with the name `Zenith_Register<Area>DocumentTypes(Zenith_MigrationRegistry&)`; until then those rows are absent, and the editor opens no document of those types (the asset browser lists the files; double-clicking one opens it in the external text editor, `editor.externalEditor`), so no unregistered type is ever edited or saved by the editor.

### Document table

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorDocumentTable.h (Zenith_EditorDocumentID itself is in Zenith_EditorIDs.h)
/// @contract value Zenith_EditorDocumentID
/// Ownership: Plain value: the document's persistent ID (its "id" member, or Zenith_MakePathDocumentID of its path).
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None; stable across reloads, renames and sessions.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
// struct Zenith_EditorDocumentID { Zenith_PersistentID m_xID; };   (Zenith2/Docs/Design/EDIT/Declarations.md "Identifier header")

/// @contract type Zenith_EditorDocumentTable
/// Ownership: Owned by the session; owns every open Zenith_EditorDocument through Zenith_Owned.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: A Zenith_EditorDocument& is invalid after Close of that document.
/// Units: None.
/// Errors: EDITOR_ERROR_DOCUMENT (the Zenith_DocumentError is logged), EDITOR_ERROR_DOCUMENT_LIMIT above
///   editor.documents.maxOpen open documents, EDITOR_ERROR_DOCUMENT_NOT_OPEN, EDITOR_ERROR_HISTORY_PENDING (Close and SaveAll
///   while an asynchronous record is pending); PrepareTypeRebind returns EDITOR_ERROR_OUT_OF_MEMORY, RebindTypes the first
///   document's EDITOR_ERROR_DOCUMENT (a document whose bytes no longer parse under the new registry; the restart fails).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3; OnSourcesChanged since Z2 Stage 3; PrepareTypeRebind and RebindTypes since Z2 Stage 4.
/// @endcontract
class Zenith_EditorDocumentTable
{
public:
	Zenith_Result<Zenith_EditorDocument*, Zenith_EditorError> Open(Zenith_StringView strPath,
		Zenith_EditorStorageRootID xRoot);                     // returns the open document when already open
	Zenith_EditorDocument* Find(Zenith_EditorDocumentID xID);
	Zenith_EditorDocument* FindByPath(Zenith_StringView strPath);
	Zenith_Result<void, Zenith_EditorError> Close(Zenith_EditorDocumentID xID, Zenith_EditorCloseMode eMode);
	uint32 List(Zenith_Span<Zenith_EditorDocument*> apxOut) const;   // open order
	uint32 GetDirtyCount() const;
	Zenith_Result<void, Zenith_EditorError> SaveAll(Zenith_EditorSaveReason eReason);
	void RegisterFactory(const Zenith_EditorDocumentFactory& xFactory);   // the last factory claiming a type wins
	// § "Editor-originated writes": the digest the editor is about to write for an open document, so its own write
	// is never taken for an external change (at most 4,096 noted entries, the oldest dropped).
	void NoteEditorWrite(Zenith_EditorDocumentID xID, const Zenith_Sha256Value& xDigest);
	bool ConsumeNotedWrite(Zenith_EditorDocumentID xID, const Zenith_Sha256Value& xDigest);   // true once per noted digest
	// The rule of § "Editor-originated writes" for one planned target: OK for not open or CLEAN; EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN
	// for DIRTY, CONFLICTED, MISSING or MIGRATED.
	Zenith_Result<void, Zenith_EditorError> CheckPlannedWrite(Zenith_StringView strPath, Zenith_EditorStorageRootID xRoot) const;
	// Pipeline.md "Reference repair": hold, then release, the external-change notifications of a document a CONTENT batch writes.
	void DeferExternalChanges(Zenith_EditorDocumentID xID);
	void ResumeExternalChanges(Zenith_EditorDocumentID xID, const Zenith_Sha256Value& xAdoptedDigest);
	// § "External changes": the one entry for "these source paths changed on disk", on the main thread in editor frame step 2,
	// called by the host watchers' drain (§ "Host watchers") and by the table's own Zenith_EditorCookListener::OnChanged (the
	// cook server's Zenith2-relative paths prefixed with Zenith_EditorHostPaths::m_strZenith2Root). Absolute host paths; a
	// path naming no open document is ignored; a watched directory (an EDITOR_HOST_CHANGE_RESCAN record) re-checks every open
	// document under it.
	void OnSourcesChanged(Zenith_Span<const Zenith_StringView> axHostPaths);
	// Pipeline.md "Runtime restart": PrepareTypeRebind (step 4, while the old registry is alive) keeps each open document's
	// canonical bytes (Serialize); RebindTypes (step 7, from RebindRuntime) passes the new registry to every open document
	// (Zenith_EditorDocument::RebindTypes) and replaces its state by the kept bytes with its own state ID and disk digest
	// (ReplaceState), so each document is re-read under the new registry, its dirty state and history unchanged.
	Zenith_Result<void, Zenith_EditorError> PrepareTypeRebind();
	Zenith_Result<void, Zenith_EditorError> RebindTypes(const Zenith_TypeRegistry& xTypes);
};
```

The table borrows the session's type registry (`Zenith_EditorSessionDesc::m_pxTypes`) and passes it to every `Zenith_EditorDocumentFactory::Open` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Document kinds additions"). `editor.documents.maxOpen` defaults to 512. A document is opened once per session: every panel, tool and command that touches it holds a `Zenith_EditorDocumentID` and resolves it through `Find`, never a pointer kept across frames.

## Worlds in the editor

### World kinds

<!-- elaboration -->
| Kind | Purpose (X243 (b)) | Count | Created | Holds | Ticks | Input | Destroyed |
|---|---|---|---|---|---|---|---|
| edit world | `WORLD_PURPOSE_EDIT` | one main edit world, plus one per open prefab stage (§ "Prefab stage" in `Zenith2/Docs/Design/EDIT/AuthoringTools.md`) | session creation; entering a prefab stage | the projection of the active scene set or of one prefab document, and the editor proxy entities | yes, at the game's tick rate, unless "Animate edit world" is off (`WORLD_PAUSE_REASON_EDITOR` held) | never the input world | session end; leaving the prefab stage |
| play world | `WORLD_PURPOSE_PLAY` (origin `WORLD_ORIGIN_EDITOR_PLAY`) | 0 … `editor.play.maxWorlds` (default 4, at most 8) | the Play command (§ "Starting play") | the play copy of the main edit world ([Z2-N-ARCH-766], § "Play copy normalisation") | yes, unless paused | the focused play world, while its game viewport has focus | Stop; the game's own quit command; a fatal script halt |
| preview world | `WORLD_PURPOSE_PREVIEW` | 0 … `editor.preview.maxWorlds` (default 8) | a preview panel opens (`Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host") | one asset's preview scene | yes while its panel is visible; paused otherwise | never | the panel closes, or LRU eviction when the limit is reached |

Every world is a real world of the runtime, isolated as [Z2-N-ARCH-762] states: its own ECS storage, physics, scripts, queues and frame-graph nodes. The worlds share the process's asset registry, renderer and type registry, as every world of a Player does. The purpose is what a module reads to choose its behaviour (§ "Module behaviour in edit and preview worlds"); the origin is GAME's lifecycle record and is `WORLD_ORIGIN_STARTUP` for edit and preview worlds (a fresh world: first tick 1, continuity base 0, row ED37), which no system of an edit or preview world reads, because GAME's gameplay framework registers none there (row ED7).

### Edit world rules

- The documents are the source of truth; the edit world is their projection. Data flows from documents into the edit world through out-of-tick command buffers (§ "Edit world projection") and never back: nothing reads an edit world's component values to write a document, except the explicit "apply play values" command, whose source is a play world (§ "Applying play values to documents").
- So whatever a module's systems write into edit-world components while the edit world ticks (a particle state, a foliage wind phase, a preview pose) is runtime-only state, never saved, reset by a reconcile (§ "Reconcile algorithm"), and replaced by the documents' values in the play copy (§ "Play copy normalisation").
- The edit world is created through `Zenith_RuntimeToolAccess::CreateWorld({ WORLD_PURPOSE_EDIT, seed })`, which sets the edit-world flag ECS defines (the flag `Zenith_WorldSimulation::ApplySchemaRevision` requires; ECS declares it in a Stage 3 API slice, row EdB26, and Z2T-ECS-099 adds `ApplySchemaRevision`, `Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world", row E11; EDIT never sets the flag itself), with seed `Zenith_Hash64` of the 16 bytes of the active scene's document ID; it is never the input world.
- A paused edit world still extracts every frame (phase 12 runs when no tick runs, [Z2-N-ARCH-384]), so the "Animate edit world" toggle never hides an edit.

### Editor proxy entities

<!-- elaboration -->
Two area designs place components on "the editor camera": VFX's `Zenith.VFX.Viewer` (`Zenith2/Docs/Design/VFX/Authoring.md` → "Effect inspector": "components the editor places on its viewport camera entities") and ENV's `Zenith.Environment.Anchor` (EN12, "the anchor on the editor camera"; `Zenith2/Docs/Design/ENV.md` → "Zenith_EnvironmentComponents header": "games put it on the main camera rig and EDIT on the active editor camera"). The editor cameras of `Zenith2/Docs/Design/EDIT/Viewports.md` → "Camera schemes" are editor state, not entities, so each edit world holds **editor proxy entities**:

| Rule | Content |
|---|---|
| count | one per open scene view showing that edit world, at most `editor.viewports.maxScene` (default 4); created when the scene view first shows the world, destroyed when it stops |
| components | `Zenith.LocalTransform` (the scene view camera's position and rotation, written out of tick every editor frame); `Zenith.VFX.Viewer` with `FovY` the camera's vertical field of view and `Enabled` true while the view is visible, when the game selects VFX; `Zenith.Environment.Anchor` on the proxy of the scene view that most recently had focus, when the game selects Environment; nothing else |
| identity | a derived persistent ID, `{ Zenith_Hash64("EDITPRXY" ++ the 8 little-endian bytes of the edit world's seed), Zenith_Hash64("EDITPRXY" ++ the 4 little-endian bytes of the viewport ID) }`, drawing nothing from the session's random source (§ "Session random source"), recorded in `Zenith_EditorProxyEntities` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "World set header"); owned in the projection's owner map by the editor owner, never by a document |
| projection | spawned, updated and destroyed by `Zenith_EditorProxyEntities::Apply` in editor frame step 2 after the projection's `Apply`, with its own out-of-tick buffer, so the projection version does not change; reconcile steps 2 and 4 visit only a document's own entities and so never touch a proxy |
| withheld components | the projection does not create `Zenith.Environment.Anchor` from documents in an edit world (the constexpr list `axEDITOR_WITHHELD_COMPONENTS`); the documents keep their anchors and the play copy restores them, so the edit world's one anchor is the active scene view's proxy (ENV: "at most one per world") and a play world's is the game's |
| exclusions | proxies are never picked, outlined, listed in the hierarchy, copied, captured into a play copy, saved or autosaved; a prefab stage's edit world has its own proxies |

`Test_EditorProxies_ViewerAndAnchorFollowSceneView` asserts that moving a scene view's camera moves its proxy at the next frame, that the anchor moves to the newly focused view's proxy, that a reconcile leaves the proxies untouched, and that no document changes.

### Edit-world-only writes

Some editor tools write the edit world without changing a document: they are presentation of the edit world, not authoring. Every such write targets one of three places, and each place keeps it out of a play world:

| Write | Target | Why it never reaches a play world |
|---|---|---|
| VFX in-place play, stop and restart (`PlayRequest`, `StopRequest`, serialized properties of `Zenith.VFX.Effect`, `Zenith2/Docs/Design/VFX/PublicAPI.md` → "Zenith_VFXComponents header"; row ED30) | a component of a document-owned entity | the play copy replaces the captured field set with the document's (§ "Play copy normalisation") |
| `Zenith.VFX.EffectState`, animation preview poses and every other state a module's system adds or writes while the edit world ticks | components of document-owned entities | the play copy drops components the documents do not hold and replaces differing sets |
| a realtime probe's "capture now" (`CaptureRequest`, LIGHT; row ED32) | a component of a document-owned entity | as the first row |
| the environment panel's time of day and weather (ENV's five command kinds, which write `Zenith.Environment.State`, a world resource; row ED31) | a world resource | a play copy carries entities alone; the play world's resource starts from the documents' authored settings |
| "Animate edit world" (`WORLD_PAUSE_REASON_EDITOR` on the edit world's control) | the world control | a new world has its own control |
| the proxies' transforms, viewers and anchor | editor proxy entities | excluded from the capture |

An edit-world-only write is never a history record and never marks a document dirty. A new edit-world-only write is added to this table in the diff that adds it, with its row in `Test_EditorPlay_CaptureExcludesEditWorldOnlyState`.

### Module behaviour in edit and preview worlds

<!-- assumption: pending SCRIPT, PHYS, AI, GAME, RPROD, ENV, VFX, ANIM, AUDIO, UI, STREAM (rows ED4 to ED9) -->
The edit and preview worlds run each module "like a play world" where the owning design says so (ENV: `Zenith2/Docs/Design/ENV.md` → "Editor surfaces"; ANIM: `Zenith2/Docs/Design/ANIM.md` → "Editor surfaces"; VFX: `Zenith2/Docs/Design/VFX/Authoring.md` → "Preview session"). A module whose systems would change authored state or run gameplay registers no simulation systems in a world whose purpose is `WORLD_PURPOSE_EDIT` or `WORLD_PURPOSE_PREVIEW`, read through `Zenith_WorldAttachContext::GetWorldPurpose()` (X243 (b)), the mechanism VFX already uses for its preview-only catch-up (`Zenith2/Docs/Design/VFX/Simulation.md` → "Catch-up steps"). EDIT's expectation per area, each recorded as a cross-area row:

| Area | In a world of purpose EDIT or PREVIEW | Row |
|---|---|---|
| SCRIPT | the script world exists (console evaluation, diagnostics) but attaches no behaviour instance and registers no chunk system; scripts run only in worlds of purpose PLAY | ED4 |
| PHYS | the physics world holds the bodies (queries for snapping, `Zenith2/Docs/Design/EDIT/Viewports.md` → "Surface snapping") but runs no step; no contact or trigger event is produced | ED5 |
| AI | no agent update, no path request | ED6 |
| GAME | no gameplay-framework system (directors, timers, abilities, rules, world events); the camera rig components are inert | ED7 |
| RPROD | `Render.ExtractCameras` writes no `VIEW_KIND_MAIN` request in a world of purpose EDIT or PREVIEW; in a world of purpose PLAY holding `Zenith.Editor.GameViewTarget` it writes the game camera as that viewport's `VIEW_KIND_EDITOR` request (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Viewport views"); EDIT's extractor writes the editor and preview view requests | ED8 |
| ENV, VFX, ANIM, AUDIO, UI, STREAM | as their designs state: ENV and VFX simulate their presentation state, ANIM evaluates preview poses, AUDIO plays no world voices in an edit world (auditioning uses its internal audition world), UI lays out nothing in an edit world (the UI authoring tool uses UI's preview context), STREAM streams nothing in an edit world (`Zenith2/Docs/Design/STREAM.md` → "Editor surfaces") | ED9 |
<!-- /assumption -->

`Test_EditorWorlds_EditWorldFlagAndPurpose` asserts that the main edit world and a prefab stage report `WORLD_PURPOSE_EDIT` through the attach context, carry the edit-world flag and origin `WORLD_ORIGIN_STARTUP`, and are never the input world; that a preview world reports `WORLD_PURPOSE_PREVIEW` without the flag; and that a play world reports `WORLD_PURPOSE_PLAY` with origin `WORLD_ORIGIN_EDITOR_PLAY`.

### Edit world projection

<!-- elaboration -->
A projection turns document changes into edit-world changes, out of tick, in the editor frame's step 2:

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorProjection.h
/// @contract type Zenith_EditorProjection
/// Ownership: One per edit world, owned by Zenith_EditorWorldSet; owns the projection arena (editor.projection.arenaBytes,
///   default 64 MiB, reset after each Apply) and the document-to-world entity map.
/// Thread affinity: MAIN_THREAD, between RunFrame calls.
/// Invalidation: None.
/// Units: None.
/// Errors: EDITOR_ERROR_PROJECTION (the ECS or SCENE error is logged); a failed projection marks the world for a reconcile.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorProjection
{
public:
	void QueueChange(const Zenith_EditorProjectionChange& xChange);   // recorded by each applied document change
	Zenith_Result<void, Zenith_EditorError> Apply();   // step 2, after commands
	void RequestReconcile(Zenith_EditorDocumentID xDocument);        // full re-resolution of one scene document
	Zenith_Result<Zenith_EntityHandle, Zenith_EditorError> FindEntity(const Zenith_PersistentID& xObject) const;
	uint64 GetProjectionVersion() const;               // +1 per Apply that changed the world
};
```

`Apply` builds one `Zenith_CommandBuffer` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandBuffer header") over the projection arena, with the out-of-tick base key and a cap of `editor.projection.bufferBytes` (default 16 MiB), records the changes queued this frame in queue order (§ "Projection per edit kind"), and calls `Zenith_WorldSimulation::ApplyOutOfTick`, then `Zenith_PropagateTransformsOutOfTick` when a transform or a parent changed. A buffer that would exceed its cap is split into consecutive `ApplyOutOfTick` calls at a record boundary. `ApplyOutOfTick` returning an error marks the document for reconcile, logs the first error and continues with the next document; the history is never rolled back by a projection failure, because the document change already succeeded and is the truth.

### Projection per edit kind

| Document change | Edit-world commands |
|---|---|
| `SCENE_EDIT_CREATE_ENTITY` + its `ADD_COMPONENT` edits (one command) | one `Spawn` with the entity's authoring ID as `m_xPersistentID`, the parent's handle (`FindEntity` of the parent; a ticket when the parent is created in the same buffer), and the component inits built by `Zenith_ReadFieldSetJson` then `Zenith_ApplyFieldSet` over a default-constructed value of each registered type; unknown components are not created (`Zenith2/Docs/Design/SCENE/Loading.md` → "Missing-type handling at load"), nor are withheld ones (§ "Editor proxy entities") |
| `SCENE_EDIT_DELETE_ENTITY` | one `Destroy` per entity of the subtree, children first |
| `SCENE_EDIT_CREATE_SUBTREE` | spawns as above, parents first, in the subtree's stored order |
| `SCENE_EDIT_SET_PARENT` | `SetParent` with the new parent's handle (or null); no sort-key effect, because sibling order is presentation of the document only |
| `SCENE_EDIT_RENAME` | none (the name lives in the document; ENV and others read names from documents) |
| `SCENE_EDIT_SET_PROPERTY` on an entity of the document | one `SetProperty` with the value encoded from the edit's JSON value by the property's reflected type (`Zenith_ReadFieldSetJson` of a one-property object, then the property's value bytes); a container or string property is a `SetContainer` |
| `SCENE_EDIT_ADD_COMPONENT` / `REMOVE_COMPONENT` | `AddComponent` with the init built as above / `RemoveComponent` |
| any instance or override kind (`PLACE_INSTANCE`, `DELETE_INSTANCE`, `REVERT_OVERRIDE`, `APPLY_OVERRIDES_TO_PREFAB`, a `SET_PROPERTY` whose target is in an instance's resolved ID space) | reconcile of the instance's subtree: § "Reconcile algorithm" restricted to the entities whose IDs are in the instance's resolved ID space |
| a prefab document saved while instanced in the edit world | reconcile of every instance of it |
| a schema revision published | none: the backends migrate stored rows (`Zenith2/Docs/Design/ECS/Backends.md` → "Schema revision in the edit world") |
| a JSON-document change of a non-scene type, forward or inverse | none, unless the type registers a `Zenith_EditorProjectionHandler`, which runs after every forward and every inverse change with the document's current state: ENV's environment documents re-record their command kinds (`Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Environment documents"); material documents re-apply their parameters with `ReplaceParameters` (`Zenith2/Docs/Design/EDIT/Panels.md` → "Material preview"), so undo and redo leave the runtime resource at the document's value |

The two maps `persistent ID → entity handle` (from `FindByPersistentID`, cached per Apply) and `entity → owning document` let the selection, the inspector and picking map between the two.

### Reconcile algorithm

A reconcile makes the edit world equal to the documents, whatever happened to it. It runs on `RequestReconcile`, after a projection error, after an external document reload, after a schema migration of documents, on the "Reset edit world" command, and in debug configurations every `editor.projection.verifyEveryN` Apply calls (default 0, off):

1. `Zenith_ResolveDocument(xDocument, &xPrefabProvider, xTypes, xStrings, xArena, xData, xDiagnostics)` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_PrefabResolver header") gives the flat resolved data D in activation order.
2. Walk the edit world's entities owned by this document (the owner map; proxies are owned by the editor owner and are not visited) into a hash map `ID → handle`, O(n).
3. For each record of D, in order: absent from the world → `Spawn` it (parents precede children in D); present → capture its serialized field sets with `Zenith_CaptureFieldSet(bSerializedOnly = true)` through the world inspector and compare with D's sets by `Zenith_EncodeFieldSet` bytes; then per component of the record, in the record's component order: present in D and absent in the world (and not withheld) → `AddComponent` with D's set; absent in D and present in the world → `RemoveComponent`; present in both with differing bytes → one `SetProperty` per property whose encoded value bytes differ, in property-ID order (a container or string property is a `SetContainer`); a differing parent → `SetParent` with D's parent handle (or null).
4. Every world entity owned by the document whose ID D does not hold → `Destroy`, children before parents.
5. `ApplyOutOfTick`, then `Zenith_PropagateTransformsOutOfTick`.

Complexity O(entities × components) for the document; it is the projection path of "Scene open" (Z2-B-EDIT-005) for the whole active scene, where step 3 degenerates to spawns because the world is empty, so opening a scene is `Zenith_ResolveDocument` plus `Zenith_ActivateSceneData` (`Zenith2/Docs/Design/SCENE/Loading.md` → "Activation paths", editor row) and never a cook. `Test_EditorProjection_ReconcileRestoresDriftedWorld` drives a world with injected drift (a destroyed entity, a changed property, an extra component) and asserts the reconcile restores the resolved state exactly.

### Preview worlds

A preview world shows one asset in isolation (a VFX effect, a prefab, a mesh, an animated character): the preview host (`Zenith2/Docs/Design/EDIT/Panels.md` → "Preview host") creates it with `WORLD_PURPOSE_PREVIEW` (X243 (b); VFX's preview session relies on exactly this, row VX11), activates the asset's preview scene (a small resolved scene the host builds in memory: the asset entity, a key light, the preview environment), ticks it while its panel is visible, and requests one `VIEW_KIND_PREVIEW` view per panel. Preview worlds are pooled; the least recently shown is destroyed when `editor.preview.maxWorlds` is reached. A preview world never holds a document projection: its content is rebuilt from the asset each time it opens, so a preview needs no undo.

## Play in editor

### Play state machine

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorPlay.h
enum Zenith_EditorPlayState : uint8
{
	EDITOR_PLAY_STATE_EDITING = 1,     // no play world
	EDITOR_PLAY_STATE_PREPARING,       // cooking the play set and capturing the edit world
	EDITOR_PLAY_STATE_STARTING,        // the play world exists; waiting for its first tick
	EDITOR_PLAY_STATE_PLAYING,
	EDITOR_PLAY_STATE_PAUSED,
	EDITOR_PLAY_STATE_STEPPING,        // paused, with a step of n ticks in flight
	EDITOR_PLAY_STATE_STOPPING,
};
```

Per play world, events × states:

| State | Event | Next | Action |
|---|---|---|---|
| EDITING | Play | PREPARING | § "Starting play" steps 1–3 |
| PREPARING | play-set cook completed | STARTING | steps 4–8 |
| PREPARING | cook failed or cancelled; Stop | EDITING | show the cook diagnostics; nothing was created |
| STARTING | the world's tick index advanced past 0 at `OnFrameEnd` | PLAYING | timestamp the PIE-start completion event (Z2-B-EDIT-006) |
| STARTING | world creation or activation failed | EDITING | destroy what was created; show the error |
| PLAYING | Pause | PAUSED | `AddPauseReasons(WORLD_PAUSE_REASON_EDITOR)` |
| PAUSED | Resume | PLAYING | `RemovePauseReasons(WORLD_PAUSE_REASON_EDITOR)` |
| PAUSED | Step n (1 … 600) | STEPPING | `RequestStep(n)` (§ "Pause and single step") |
| STEPPING | n ticks ran | PAUSED | — |
| PLAYING, PAUSED, STEPPING | Stop; the game's `Runtime.Quit`; the script world halted (the tool hook `OnWorldHalted`, delivered on the main thread in editor frame step 6, § "Runtime tool access") | STOPPING | § "Stopping play" |
| STEPPING | a `DEBUGGER` pause reason applied during the step | PAUSED | the step's remaining count is cleared (§ "Pause and single step") |
| STOPPING | the world was destroyed at the next `OnFrameEnd` | EDITING (when no other play world remains) | restore the editor's input routing |
| any | the editor shuts down | STOPPING | as Stop |

Any event not in the table is ignored and counted (`editor.play.ignored_events`). The toolbar's play controls act on the focused play world; Stop All stops every play world.

### Starting play

1. Refuse with `EDITOR_ERROR_PLAY_STATE` while a schema window is queued or a migration plan is being applied, and with `EDITOR_ERROR_HISTORY_PENDING` while an asynchronous record is pending. A read-only session may still play (play changes no document), except when the active scene set has a document that failed to open or to resolve, which refuses with `EDITOR_ERROR_PLAY_STATE` naming it. Otherwise flush the projection and the proxies (`Apply`) and record the MP-EDIT start timestamp of "Play-in-editor start".
2. The play copy: § "Play copy normalisation", into a fresh arena of the play controller; it keeps every persistent ID (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Identity rules the editor foundation builds on", rule 5).
3. Play set: the play copy's `GetAssetReferences()` plus every script module the game's manifest names; send the cook server one `cook` request with those GUIDs (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Cook client"), so every referenced asset is current in the loose tree before the world loads it; with watching enabled most are already current and the request returns at once.
4. `Zenith_RuntimeToolAccess::CreateWorld({ WORLD_PURPOSE_PLAY, ulSeed })` (origin `WORLD_ORIGIN_EDITOR_PLAY`) with `ulSeed = Zenith_Hash64` over 16 bytes: the main edit world's seed (8 bytes, little-endian) followed by the ASCII tag `EDITPLAY`. The seed has no play ordinal: play worlds share nothing ([Z2-N-ARCH-762]), so they need no distinct seeds, and every play world started from the same documents, whether it is the session's first or fifth, starts from the same seed as a fresh session's first (the determinism obligation of `Zenith2/Docs/Design/EDIT.md` → "Determinism obligations").
5. `Zenith_ActivateSceneData(xData, *xWorld.m_pxSimulation, *xWorld.m_pxSceneState, xAssets)` of the play copy, out of tick.
6. Script hot-reload catch-up: every module compiled in process since the cooked bytecode was written is queued with `RequestHotReload` on the play world's script world, so the play world runs the newest source.
7. When the game viewport has focus: `SetInputWorld(play tag)`; set the play world's UI presentation rectangle to the game viewport (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Game viewport input").
8. Enter STARTING; the first tick runs in the next `RunFrame`.

### Play copy normalisation

<!-- elaboration -->
[Z2-N-ARCH-766] starts play by copying the edit world by serialization. The edit world is the documents' projection plus runtime-only state (§ "Edit world rules", § "Edit-world-only writes") plus the proxies, so the copy is taken from the edit world and then normalised to the documents, which makes the play world's start state a function of the documents alone:

1. **Resolve.** For every document of the active scene set, `Zenith_ResolveDocument` with the session's prefab provider into the play arena gives D, the resolved data the projection projects (Z2-B-EDIT-006 includes this cost).
2. **Capture.** `Zenith_CaptureEntities(xEditInspector, xTypes, xStrings, axRoots, xPlayArena, xCaptured)` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneCapture header") with `axRoots` the persistent IDs of the root entities owned by the active scene set's documents, in D's activation order. The proxies are roots owned by the editor owner, so they are never in `axRoots` and never captured; no SCENE change is needed for the exclusion.
3. **Normalise** C (the capture) against D, one entity at a time, in D's activation order:
   - an entity of C whose ID D does not hold (an entity a module's system spawned under a document entity) is dropped (`editor.play.copy_dropped_entities`);
   - a component of a kept entity that D's record does not hold (state a system added, such as `Zenith.VFX.EffectState`) is dropped;
   - a component whose `Zenith_EncodeFieldSet` bytes differ from D's is replaced by D's set (`editor.play.copy_normalised_sets`): VFX's `PlayRequest`, a realtime probe's `CaptureRequest`, an animation pose, any state an edit-world tick wrote;
   - a component or entity of D absent from C is added from D: a withheld component (the anchor) as expected, anything else counted as projection drift (`editor.play.copy_drift`), logged once (`editor.play: the edit world drifted from <document>; play starts from the document, and a reconcile is requested`) and followed by `RequestReconcile` of the document.
4. **Result.** The play copy is C after step 3: every persistent ID, parent and component of D, in D's activation order, with D's values. `Zenith_EditorPlayController::GetLastCopyStats` reports the counts.

Complexity O(entities × components) over the active scene set, the same order as a reconcile. `Test_EditorPlay_CaptureExcludesEditWorldOnlyState` (Stage 3) scrubs the time of day, presses "capture now" on a realtime probe, lets the edit world tick 120 times, adds two scene views (so two proxies exist), then Plays; it asserts that the play world's `HashTickState` after its first tick equals that of a Play started in a fresh session from the same documents with none of those actions, that no proxy ID is in the play world, and that the documents' bytes are unchanged. Each subsequent edit-world-only write adds its own named case in the ticket that adds the write, with the same assertions: `Test_EditorPlay_CaptureExcludesWeatherScrub` (Stage 4, ENV's weather commands) and `Test_EditorPlay_CaptureExcludesInPlaceEffect` (Stage 5, VFX's in-place play, with the `VFX` module the EditorFixture game adds at Stage 5).

### Stopping play

1. `SetInputWorld(0)` when the stopped world was the input world.
2. Clear the play world's selection mapping (the selection keeps its document object refs, § "Selection persistence and mapping").
3. `Zenith_RuntimeToolAccess::DestroyWorld(tag)`, which walks GAME's teardown order (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Shutdown and teardown").
4. Discard the play world's history (§ "Play-world edits") and release the capture arena.
5. Nothing reaches the edit world or a document ([Z2-N-ARCH-766]); `Test_EditorPlay_StopLeavesEditWorldUnchanged` hashes the edit world (`HashTickState`) and every open document's canonical bytes before Play and after Stop and asserts both equal.

### Pause and single step

- Pause and Resume add and remove `WORLD_PAUSE_REASON_EDITOR` on the play world's `Zenith_WorldControl` (`Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration", row GA14); the reason is the editor's alone, so a gameplay or menu pause the game set stays in force when the editor resumes.
- Step n (1 … 600) runs exactly n fixed ticks and pauses again. It needs a counted run that a pause mask cannot express (a frame may run several ticks):

<!-- assumption: pending GAME, ECS (row ED10) -->
`Zenith_WorldControl::RequestStep(uint32 uTicks)` (GAME) stores n; prologue step 7 un-pauses a world whose only applied reasons are `EDITOR` and/or `DEBUGGER` when n > 0 and calls `Zenith_WorldSimulation::RunTicksThenPause(n)` (ECS), whose tick boundary decrements the count after each tick and applies `SetPaused(true)` at the boundary after the n-th. The semantics:

| Rule | Value |
|---|---|
| T | the number of fixed ticks the frame would run if the world were not paused: the simulation clock's T for the frame's Δt, capped by `sim.maxCatchUpTicks` (`Zenith_SimulationClockDesc::m_uMaxCatchUpTicks`, `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_SimulationClock header") |
| ticks per frame during a step | `min(n_remaining, max(1, T))`: a step always advances at least one tick per frame, even when less than one tick of time has elapsed |
| the accumulator | not consumed and not filled during a step (step ticks are counted, not timed); while paused no time accumulates; at Resume the accumulator restarts from 0, so a Resume never runs a burst of catch-up ticks |
| `DEBUGGER` applied during a step | the step is cancelled: the remaining count is cleared, the world stays paused under both reasons, and the play state is PAUSED |
| determinism | a step's ticks are ordinary ticks with the world's inputs (Z2-D-019); `Test_EditorPlay_StepRunsExactlyN` checks n against the tick index |
<!-- /assumption -->

### Play-world edits

While playing, the inspector edits the focused play world, not the documents: each edit is a `SetProperty`, `AddComponent` or `RemoveComponent` recorded into a command buffer applied out of tick at the next editor frame. Such edits are undoable inside a separate per-play-world history (the same `Zenith_EditorHistory` type, instance owned by the play controller), discarded at Stop; the main history is not touched, so a play session can never leave a document change behind. The inspector shows a play-mode tint and the banner "Play-mode edits are discarded at Stop; use Apply to document to keep them".

### Applying play values to documents

"Apply to document" (command `play.applyToDocument`) copies the selected entities' current component values from the play world into their scene documents:

1. For each selected play-world entity: `GetPersistentID`; find the owning document and object (an entity of an instance maps to its instance through the resolved ID space).
2. Capture the entity's serialized field sets from the play inspector (out of tick, at a tick boundary of the play world, [Z2-N-ARCH-762]).
3. For each property whose value differs from the resolved document value, build a `SCENE_EDIT_SET_PROPERTY` (on an instance entity it writes an override).
4. Execute all of them as one transaction on the main history ("Apply play values (n properties)"), so the copy is undoable like any edit.

### Several play worlds

Up to `editor.play.maxWorlds` play worlds run at once, each from its own play copy at its own Play command (Z2-I-EDIT-10). They share nothing mutable ([Z2-N-ARCH-762]); each has its own game viewport tab, its own pause state, its own play history and its own UI context. Exactly one is the input world at a time: the one whose game viewport last received focus.

`Test_EditorPlay_TwoWorldsIndependent` uses fixed inputs: a scripted input timeline T (a `Zenith_InputFrame` per frame, holding a move key from tick 10 to tick 70) and one inspector edit E (a `SetProperty` of a fixture behaviour's speed property, applied out of tick at the boundary after tick 30). It starts play world A, then play world B from the same documents, routes T into A only and applies E to B only, runs both 120 ticks, and asserts: A's `HashTickState` at tick 120 equals that of a solo run (a fresh session, one play world, T routed into it, no edit), and B's equals that of a solo run with no input and E applied at the boundary after tick 30.

### Input routing during play

| Focus | Input world | Editor shortcuts |
|---|---|---|
| a game viewport of a play world | that play world (`SetInputWorld`) | only the reserved play shortcuts (`Shift+F5` stop, `F6` pause, `F7` step, `Shift+F1` release focus); every other key reaches the game |
| any other panel, or the game viewport released | none (`SetInputWorld(0)`, row ED11) | every editor shortcut |

The editor's `Zenith_InputClaimant` (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor input claimant") claims every pointer press outside a focused game viewport, so a click on a panel never reaches the game.

## Documents

### Document interface

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorDocument.h (Zenith_EditorDocumentState and the rest: Declarations.md "Document kinds additions")
enum Zenith_EditorDocumentKind : uint8
{
	EDITOR_DOCUMENT_KIND_SCENE = 1,    // Zenith_SceneDocument, scene or prefab
	EDITOR_DOCUMENT_KIND_JSON,         // any registered document type edited as a JSON tree (anim, audio, materials, schemas, …)
	EDITOR_DOCUMENT_KIND_TERRAIN,      // Zenith_TerrainDocument with its tile documents
	EDITOR_DOCUMENT_KIND_UI,           // Zenith_UIEditorModel over a .ui.json document
};

/// @contract interface Zenith_EditorDocument
/// Ownership: Implemented in Internal/Documents/ per kind; owned by Zenith_EditorDocumentTable through Zenith_Owned.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Kind-specific views are invalid after the next applied change.
/// Units: State IDs are monotonic per session.
/// Errors: Zenith_EditorError; the kind-specific error is logged with the document path.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2 (SCENE, JSON: the pull-forward slice of ZenithEditorCore); TERRAIN since Z2 Stage 3; UI since Z2 Stage 4.
/// @endcontract
class Zenith_EditorDocument
{
public:
	virtual ~Zenith_EditorDocument() = default;
	virtual Zenith_EditorDocumentKind GetKind() const = 0;
	virtual Zenith_EditorDocumentID GetID() const = 0;
	virtual Zenith_StringView GetPath() const = 0;              // storage path within its root
	virtual Zenith_EditorStorageRootID GetRoot() const = 0;
	virtual Zenith_TypeID GetDocumentType() const = 0;          // the registry's type
	virtual Zenith_EditorDocumentState GetState() const = 0;
	virtual uint64 GetStateID() const = 0;                    // § "Dirty tracking"
	virtual uint64 GetSavedStateID() const = 0;
	virtual bool IsReadOnly() const = 0;
	// The canonical bytes of the current state (SCENE's Serialize, the migrator's Serialize, ENV's Save into memory, UI's save).
	virtual Zenith_Result<void, Zenith_EditorError> Serialize(Zenith_Array<char>& xOut) const = 0;
	// Writes for SaveDocuments: one entry per file (a terrain contributes its dirty tile documents).
	virtual Zenith_Result<void, Zenith_EditorError> CollectWrites(Zenith_Arena& xArena,
		Zenith_Array<Zenith_DocumentWrite>& axOut) = 0;
	virtual void OnSaved(uint64 ulStateID, const Zenith_Sha256Value& xNewDigest) = 0;
	virtual const Zenith_Sha256Value& GetDiskDigest() const = 0;   // the bytes last read or written
	virtual Zenith_Result<void, Zenith_EditorError> ReloadFromDisk() = 0;   // § "External changes"
	// Replaces the state by canonical bytes, parsed through the root's migrator, with the given state ID and disk digest, without
	// touching the history: DOCUMENT_BYTES changes, the schema undo (Pipeline.md "Schema undo"), recovery. A parse failure
	// leaves the document unchanged and returns EDITOR_ERROR_DOCUMENT.
	virtual Zenith_Result<void, Zenith_EditorError> ReplaceState(Zenith_ByteSpan xCanonicalBytes, uint64 ulStateID,
		const Zenith_Sha256Value& xDiskDigest) = 0;
	// The registry the kind keeps from Open is replaced (a runtime restart, Zenith_EditorDocumentTable::RebindTypes); the
	// table calls ReplaceState right after, so the kind never reads through the old registry again.
	virtual void RebindTypes(const Zenith_TypeRegistry& xTypes) = 0;
	virtual uint64 GetMemoryBytes() const = 0;
};
```

### Document kinds

| Kind | Opened through | Edits | Save |
|---|---|---|---|
| SCENE | `Zenith_DocumentMigrator::Open` → `Zenith_SceneDocument::Load` | `Zenith_ApplySceneEdit` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneEdit header") | `Serialize` → one `Zenith_DocumentWrite` |
| JSON | `Zenith_DocumentMigrator::Open` (the tree is the document) | § "JSON edits", validated after each change by the type's validator when it registers one (ANIM's compiler, ENV's readers, GEOM's material reader through the cook server's `validate`) | `Zenith_DocumentMigrator::Serialize` → one write |
| TERRAIN | `Zenith_DocumentMigrator::Open` of the `.terrain.json` and, through EDIT's `Zenith_TerrainTileSource`, of each tile document → `Zenith_TerrainDocument::Load(…, bRepairBorders = true, …)` (`Zenith2/Docs/Design/ENV/Terrain.md` → "Terrain edit deltas and undo" for edits) | `ApplyStroke`, `ApplyEdit`, the structural members | `Zenith_TerrainDocument::Save` through EDIT's `Zenith_TerrainDocumentSink` (§ "Saving") |
| UI | `Zenith_UIEditorModel` (`Zenith2/Docs/Design/UI.md` → "Editor surfaces") | the model's reversible operations | the model's canonical save |

### Document identity and ownership

- A document's identity is its `Zenith_EditorDocumentID` (its persistent ID), never its path: a rename (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Asset operations") keeps every reference, selection and history entry valid.
- Document ownership (the Stage 3 foundation item): every open document is owned by exactly one session's document table and changed only through that session's commands, and every file write the editor itself originates for an authoring document goes through the document model (§ "Editor-originated writes"), never behind it; a tool or panel never edits a document's tree or object directly. The cross-process half is SCENE's writer lock, taken per save batch (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Storage-root writer lock"), plus EDIT's session lock, which keeps a second editor session off the same game (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Session lock and crash detection").
- Every object inside a document is addressed by an authoring ID that SCENE guarantees stable (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Identity rules the editor foundation builds on"); JSON documents address their objects by document pointer plus, where the type defines one, an object ID member (`"uid"` for UI elements, node IDs for animation graphs, event paths for audio banks).

### Document states

| State | Meaning | Entered |
|---|---|---|
| `EDITOR_DOCUMENT_STATE_CLEAN` | the state ID equals the saved state ID | open; save; undo back to the saved state |
| `EDITOR_DOCUMENT_STATE_DIRTY` | unsaved changes | any applied change that moves the state ID away from the saved one |
| `EDITOR_DOCUMENT_STATE_MIGRATED` | opened as `DOCUMENT_OPEN_MIGRATED`: the in-memory tree is at the current version; saving writes the migration | open of an older document; leaves to CLEAN at save |
| `EDITOR_DOCUMENT_STATE_READ_ONLY_NEWER` | written by a newer tool ([Z2-N-CONTRACT-537]); every edit command refuses (`EDITOR_ERROR_DOCUMENT_READ_ONLY`) | open |
| `EDITOR_DOCUMENT_STATE_CONFLICTED` | the file changed on disk while the document was dirty | § "External changes" |
| `EDITOR_DOCUMENT_STATE_MISSING` | the file was deleted or moved outside the editor | the watch notification; the document stays open, saving recreates it |

### External changes

Two sources report changed source paths, and both reach one entry, `Zenith_EditorDocumentTable::OnSourcesChanged(axHostPaths)` (§ "Document table"), on the main thread in editor frame step 2: the cook server's `changed` notification (the table's cook listener prefixes each Zenith2-relative path with `m_strZenith2Root`), and the editor's host watchers over every storage root (§ "Host watchers"), which also see the documents the cook server does not import and every edit made outside the editor. A change both report is handled once: the second report finds the digest of step 1 equal to `GetDiskDigest()`. Each host path is resolved to its storage root (`Zenith_EditorStorageRoots::FindByPath`) and the root-relative path; a path naming no open document is ignored; a deferred document's reports are held (`DeferExternalChanges`); and an `EDITOR_HOST_CHANGE_RESCAN` path (a watched directory) applies these steps to every open document under it. For each open document at such a path:

0. The file no longer exists → MISSING: the document stays open with its in-memory state, and saving recreates the file (§ "Document states").
1. Read the file and compute its SHA-256; equal to `GetDiskDigest()`, or to a digest `NoteEditorWrite` recorded for this document (§ "Editor-originated writes") → no change (the editor's own save or write, or a touch).
2. The document is CLEAN → `ReloadFromDisk`, which re-opens through the migrator and replaces the document's state with a new state ID; the history prunes the document's records exactly as closing it does (`Zenith_EditorHistory::PruneDocument`, § "Closing a document and its history"), so the records of other documents stay undoable and no fence is created; the projection reconciles the document; the notification "Reloaded <path> (changed on disk); n undo steps of it removed".
3. The document is DIRTY → CONFLICTED: the editor keeps the in-memory state, shows the diff between the disk bytes and the in-memory canonical bytes (`Zenith_DocumentDiff` over the two texts), and offers "Keep mine" (the next save overwrites, with its expected digest set to the new disk digest) or "Take disk" (as step 2, the in-memory changes discarded after a confirmation, and recoverable from the autosave until the next autosave).

### Host watchers

<!-- elaboration -->
The paths the editor must watch (the source directories of every storage root, the engine content roots, the shader reload journal in the out tree) are host directories outside every engine mount, so the editor watches them with its own `Zenith_EditorHostWatcher` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Host watcher header") and uses no engine watcher. The watchers belong to `Zenith_EditorApp`, not to the runtime: a runtime restart leaves them running (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Runtime restart").

| Watcher | Directory | Recursive | Created | Its changes go to |
|---|---|---|---|---|
| one per storage root | the root's directory (`Games/<Name>/`, each `Engine/<Lib>/Internal/Content/`), every source, schema and script of the root included | yes | startup step 10 | every path: `Zenith_EditorDocumentTable::OnSourcesChanged`; a `.luau` path, or a rescan of the root: script reload (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Script reload" step 1) |
| the shader reload journal | `<m_strOutRoot>/<preset>/Shaders/`, created by the editor when absent before it launches the shader watcher | no | when the shader watcher child starts (`editor.shaders.watch`) | a change of `reload.journal`, or a rescan: the journal tailer (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Shader reload" step 1) |

- Debounce: 100 ms for every watcher.
- In editor frame step 2 `Zenith_EditorApp` drains each watcher into the frame arena, in creation order, and routes the records as the table says, before the cook client's `Poll`, so a source change and the server's `changed` for it, arriving in the same frame, are handled by the first report and found unchanged by the second.
- A rescan record of a storage root re-checks every open document under it (§ "External changes") and makes script reload hash every `.luau` file under the root and compile those whose digest differs from the module's last compiled digest; a rescan of the journal directory makes the tailer read from its last offset, as any change does.
- A watcher that cannot be created (`EDITOR_ERROR_HOST_PATH`: the directory is missing or cannot be opened) logs one `editor.document` warning naming the directory and shows "File watching unavailable for <directory>" in the status bar; the session continues, and the cook server's `changed` notification still reports the sources it imports.
- `Test_EditorHostWatcher_ReportsSourceAndJournalChanges` (`ZenithEditorCore_Tests`, a scratch directory under `${ZENITH_OUT}/test-fixtures/EDIT/watch/`) creates a recursive watcher over a scratch root and a non-recursive one over a scratch journal directory, then adds, modifies, renames and removes a nested `Scripts/Mover.luau`, writes a document under `.editor/` and appends three lines to `reload.journal`, and asserts the drained records: one ADDED, one MODIFIED for two writes within the debounce, REMOVED then RENAMED for the rename, one REMOVED, nothing for the `.editor/` path, MODIFIED records for the journal, and nothing from the non-recursive watcher for a file in a subdirectory of the journal directory; a watcher over a missing directory returns `EDITOR_ERROR_HOST_PATH`. `Test_EditorHostWatcher_QueueCoalescesAndBounds` drives `Zenith_EditorHostChangeQueue` with a test clock: three reports of one path within the debounce give one record of the last kind, a report after the debounce gives a second record, 5,000 distinct paths flushed at once give a drain of 4,096 records, the oldest dropped and the last record the RESCAN of the watched directory (5,001 pushes, so `editor.watcher.dropped` counts 905), and `NoteOverflow` gives one RESCAN record.

### Saving

- Saving is refused with `EDITOR_ERROR_HISTORY_PENDING` while an asynchronous record is pending (its documents may be mid-operation, § "Asynchronous changes").
- Save (command `document.save`), Save All (`document.saveAll`) and the save before Play (`editor.play.saveBeforePlay`, default false) call `SaveDocuments` of the document's root migrator with every write `CollectWrites` returned, each with the expected old digest of `GetDiskDigest()` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Applying a plan": a mismatch is `DOCUMENT_ERROR_STALE_PLAN`, so an external edit is never overwritten).
- Documents of one root saved together go in one journaled batch; documents of different roots go in one batch per root, in root order.
- On success: `OnSaved(state ID, new digest)` for each document; the autosave of the document is deleted. On `DOCUMENT_ERROR_STORAGE_LOCKED`: the documents stay dirty, the notification names the holder ("another tool is writing this content root; retry when it finishes"), and the save command fails with `EDITOR_ERROR_STORAGE_LOCKED`. On `DOCUMENT_ERROR_STALE_PLAN`: the document becomes CONFLICTED. On `DOCUMENT_ERROR_LOW_DISK`: the documents stay dirty and the notification states the free and required bytes.
- The terrain sink: `Zenith_EditorTerrainSink` implements `Zenith_TerrainDocumentSink` (`Zenith2/Docs/Design/ENV/Terrain.md` → "Zenith_TerrainDocument header"): `BeginBatch` starts collecting, `WriteDocument` appends a write with the tile's expected digest, `EndBatch` calls `SaveDocuments` once for the terrain document and its dirty tiles (row EN12).
- Saving never changes a document's bytes beyond canonical form: an unchanged document saved twice writes the same bytes, and a document opened `MIGRATED` and saved writes exactly the bytes of the migration plan for it (`Test_EditorSave_MigratedEqualsPlanBytes`).

### Editor-originated writes

<!-- elaboration -->
Besides a document's own save, the editor writes authoring documents' files itself: a rename's or delete's reference-repair plan (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Rename and move", "Delete and the editor trash"), a migration apply ("Applying a migration"), the schema apply's document batch and its restore (`Zenith2/Docs/Design/EDIT/Panels.md` → "Schema panel workflow"), the schema undo and redo ("Schema undo"), "Restore this batch", a revert to `HEAD`, a recovery "Restore as copy", and the inverse writes of each of these when its record is undone. Every such write goes through the document model, never behind it, by one rule applied per target document before anything is written:

| Target document | Before the write | The write | After it succeeds |
|---|---|---|---|
| not open | — | `SaveDocuments` of its root with the expected digest the plan read (a mismatch is `DOCUMENT_ERROR_STALE_PLAN`) | nothing to update: no document object exists |
| open, CLEAN | the operation's record gains, for this document, an `EDITOR_CHANGE_DOCUMENT_BYTES` change holding its current canonical bytes (the inverse) and the planned bytes (the forward) | the same batch write, expected digest `GetDiskDigest()`; immediately before it, `Zenith_EditorDocumentTable::NoteEditorWrite(document, new digest)` | `ReplaceState(planned bytes, a new state ID, the new digest)`, and the document's saved state ID becomes that state ID, so it stays CLEAN; the projection reconciles it; its history is untouched (no prune, no fence) |
| open, DIRTY, CONFLICTED or MISSING | the whole operation is refused before any change, with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` (`m_uDetail` the document's index in the plan) and the notification "Save or revert <path> first: <operation> would change it"; a revert to `HEAD` of that document itself is the exception below | — | — |
| open, `MIGRATED` (opened older, migrated in memory) | treated as DIRTY (its migration is unsaved work), except by a migration apply of its own entry, which writes exactly that migration (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Applying a migration" step 4) | — | — |

- **The revert exception.** A revert to `HEAD` (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Diff revert and resolve") is the rule's second exception, beside a `MIGRATED` document's own migration entry: its target is the document the user chose to revert, so a dirty (or `MIGRATED`, or `CONFLICTED`) open document is not refused. After the confirmation ("Discard the unsaved changes of <path> and revert it to HEAD?"), the record gains an `EDITOR_CHANGE_DOCUMENT_BYTES` change whose forward is the HEAD bytes and whose inverse is the file's pre-revert bytes (digest `GetDiskDigest()`) together with the document's in-memory canonical bytes (`Serialize`) and its current state ID, all kept in the record; the HEAD bytes are written with `NoteEditorWrite` of their digest before the batch, and the document takes them through `ReplaceState` (a new state ID, the new digest, CLEAN). Undo writes the file's pre-revert bytes back with the HEAD digest expected (and `NoteEditorWrite` of their digest) and restores the in-memory state through `ReplaceState(the kept in-memory bytes, the kept state ID, the pre-revert digest)`, so the document is dirty again exactly as before the revert; redo repeats the forward. A clean open document follows the table's CLEAN row, and a document that is not open its first row.

- **Self-write suppression.** `NoteEditorWrite(xDocument, xDigest)` records `(document, digest)` in a table of at most 4,096 entries (the oldest dropped when full); § "External changes" step 1 treats a disk digest equal either to `GetDiskDigest()` or to a noted digest of the same document as "no change" and removes the noted entry, counting `editor.documents.self_writes_ignored`. So the cook server's `changed` notification and the host watcher's record for the editor's own write never reload or conflict the document, whichever frame they arrive in (the schema apply writes its batch in editor frame step 2 and its result arrives in a subsequent frame).
- **Inverse writes carry expected digests.** An undo or redo of such a record writes the inverse (or forward) bytes with the digests the opposite direction wrote as its expected digests, kept in the record; a file changed since (`DOCUMENT_ERROR_STALE_PLAN`) is a permanent failure (§ "Asynchronous changes": a fence for an undo or redo), and the rule above applies to the target documents again at undo time (a document dirtied since refuses the undo with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN`, a transient failure: saving or reverting the document makes the undo possible again).
- **Failures never discard unsaved edits.** No editor-originated path reloads a dirty document from disk: the "incomplete" compensation fence of § "Asynchronous changes" writes an autosave of every dirty document the record touched and leaves it dirty in memory (CONFLICTED when its file differs from `GetDiskDigest()`, so the user chooses "Keep mine" or "Take disk"), and leaves every clean touched document to § "External changes".
- `Test_EditorAssetOps_RenameWithDirtyReferencerRefused` renames a prefab while a scene referencing it is open and dirty: the rename fails with `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` naming the scene, no file moved, nothing recorded; with the scene open and clean, the rename succeeds, the scene's in-memory `hint` equals its file's, the scene stays CLEAN, and its earlier records still undo. `Test_EditorSchemaPanel_ApplyKeepsOpenDocumentHistory` applies a schema change while a migrated scene is open and clean with three earlier records, delivers the cook server's `changed` notification for the scene after the apply, and asserts the scene was not reloaded (no prune), its three records still undo, and `editor.documents.self_writes_ignored` counted the notification.

### Read-only and newer documents

A document opened `DOCUMENT_OPEN_READ_ONLY_NEWER`, a document of a read-only root, and every document of a read-only session show a lock icon and the banner "Read-only: <reason>"; every command whose record would touch it fails at Execute with `EDITOR_ERROR_DOCUMENT_READ_ONLY` before any change is applied, and the command is not recorded.

### JSON edits

<!-- elaboration -->
SCENE's edits cover scene and prefab documents; every other document type is edited as a JSON tree through EDIT's pointer-addressed edits, which have exact inverses:

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorJsonEdit.h
enum Zenith_EditorJsonEditKind : uint8
{
	EDITOR_JSON_EDIT_SET_VALUE = 1,        // replace the value at a pointer (any kind to any kind)
	EDITOR_JSON_EDIT_INSERT_MEMBER,        // insert key:value into an object at a member index
	EDITOR_JSON_EDIT_REMOVE_MEMBER,        // remove a member by key
	EDITOR_JSON_EDIT_INSERT_ELEMENT,       // insert a value into an array at an index
	EDITOR_JSON_EDIT_REMOVE_ELEMENT,       // remove the element at an index
	EDITOR_JSON_EDIT_MOVE_ELEMENT,         // move an element from index i to index j of the same array
};

/// @contract value Zenith_EditorJsonEdit
/// Ownership: A value whose pointer text and value subtree are copied into the arena of its builder; owned by the command record.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when its arena is reset.
/// Units: Pointers in the grammar of Zenith2/Docs/Design/CORE/Text.md "Pointer resolution steps"; indices from 0.
/// Errors: Builders return EDITOR_ERROR_OUT_OF_MEMORY and EDITOR_ERROR_JSON_EDIT for a malformed pointer; application
///   returns EDITOR_ERROR_JSON_EDIT for a pointer that does not resolve, a wrong kind, a duplicate key or an index out of range,
///   leaving the tree unchanged.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorJsonEdit
{
public:
	Zenith_EditorJsonEditKind GetKind() const;
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> SetValue(Zenith_Arena& xArena,
		Zenith_StringView strPointer, const Zenith_JsonDocument& xValueDocument, Zenith_JsonNode xValue);
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> InsertMember(Zenith_Arena& xArena,
		Zenith_StringView strObjectPointer, uint32 uIndex, Zenith_StringView strKey, const Zenith_JsonDocument& xValueDocument,
		Zenith_JsonNode xValue);
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> RemoveMember(Zenith_Arena& xArena,
		Zenith_StringView strObjectPointer, Zenith_StringView strKey);
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> InsertElement(Zenith_Arena& xArena,
		Zenith_StringView strArrayPointer, uint32 uIndex, const Zenith_JsonDocument& xValueDocument, Zenith_JsonNode xValue);
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> RemoveElement(Zenith_Arena& xArena,
		Zenith_StringView strArrayPointer, uint32 uIndex);
	static Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> MoveElement(Zenith_Arena& xArena,
		Zenith_StringView strArrayPointer, uint32 uFrom, uint32 uTo);
};

// Applies xEdit to the tree and returns its inverse, built in xArena; the tree is unchanged on error.
Zenith_Result<Zenith_EditorJsonEdit, Zenith_EditorError> Zenith_ApplyEditorJsonEdit(Zenith_JsonDocument& xTree,
	const Zenith_EditorJsonEdit& xEdit, Zenith_Arena& xArena);
```

### JSON edit inverses

| Edit | Inverse | Application through `Zenith_JsonDocument`'s mutators (`Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree") |
|---|---|---|
| `SET_VALUE(p, v)` | `SET_VALUE(p, old value at p)` | copy v with `CreateValue` into the tree, then replace the node at p; the old subtree is copied into the inverse first |
| `INSERT_MEMBER(o, i, k, v)` | `REMOVE_MEMBER(o, k)` | `InsertMember(o, i, k, …)`; a key already present is an error |
| `REMOVE_MEMBER(o, k)` | `INSERT_MEMBER(o, index of k, k, old value)` | `RemoveMember` |
| `INSERT_ELEMENT(a, i, v)` | `REMOVE_ELEMENT(a, i)` | `InsertElement` |
| `REMOVE_ELEMENT(a, i)` | `INSERT_ELEMENT(a, i, old value)` | `RemoveElement` |
| `MOVE_ELEMENT(a, i, j)` | `MOVE_ELEMENT(a, j, i)` | remove at i, insert at j, without copying the subtree |

Members are inserted at the index the caller gives, which the inspector computes from the type's canonical schema order, so an edit never makes a document non-canonical; unknown members keep their anchors (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Unknown members across migration steps" describes the anchor rule the canonical writer applies). `Test_EditorJsonEdit_InverseRestoresBytes` applies every kind at the first, a middle and the last position of a fixture and asserts that edit then inverse restores the canonical bytes, and that an inverse's inverse equals the original edit.

## Commands and history

### Command records

<!-- elaboration -->
Every change to a document goes through a command. A command is a registered action (`Zenith2/Docs/Design/EDIT/Automation.md` → "Command registry") whose execution produces one history record:

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorHistory.h (Zenith_EditorHistory itself: Declarations.md "History header")
inline constexpr uint32 uEDITOR_RECORD_MAX_DOCUMENTS = 16;

/// @contract value Zenith_EditorRecordInfo
/// Ownership: Plain value describing one record; the label lives in the record's arena.
/// Thread affinity: IMMUTABLE once the record is committed; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: The label view is invalid when the record is evicted.
/// Units: Bytes; microseconds of the host monotonic clock.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorRecordInfo
{
	uint64 m_ulRecordID;                                   // monotonic per history
	Zenith_StringView m_strLabel;                           // "Move 3 entities", shown in the undo menu
	Zenith_EditorCommandID m_xCommand;                      // the registered command that produced it
	uint64 m_ulTimestampUs;
	uint32 m_uDocumentCount;
	Zenith_EditorDocumentID m_axDocuments[uEDITOR_RECORD_MAX_DOCUMENTS];
	uint64 m_ulMemoryBytes;                                // the record's arena plus the terrain edits it owns
	uint64 m_ulMergeKey;                                   // 0: never merges
	bool m_bFence;                                          // a history fence (§ "History fences"): not undoable
	bool m_bAsynchronous;                                   // holds an asynchronous change (§ "Asynchronous changes")
	Zenith_EditorRecordState m_eState;                      // COMMITTED, or a pending state
};
```

A record holds, in order of application, a list of changes (`Zenith_EditorChange`, `Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header"); each change carries what it needs to apply forward and to apply its inverse, the state ID of its document before and after, and the selection before and after the whole record. A record that touches more than `uEDITOR_RECORD_MAX_DOCUMENTS` documents (a migration, a large repair) lists the first 16 in `m_axDocuments` and keeps the full list in its arena, which pruning reads.

### Changes inside a record

| Change kind | Forward | Inverse | Payload | Synchronous |
|---|---|---|---|---|
| `EDITOR_CHANGE_SCENE_EDIT` | the `Zenith_SceneEdit` | the inverse `Zenith_ApplySceneEdit` returned | the record's arena | yes |
| `EDITOR_CHANGE_JSON_EDIT` | the `Zenith_EditorJsonEdit` | its inverse | the record's arena | yes |
| `EDITOR_CHANGE_TERRAIN_EDIT` | `Zenith_EditorTerrainOperation`: `ApplyEdit(edit, TERRAIN_EDIT_DIRECTION_REAPPLY)` | `ApplyEdit(edit, TERRAIN_EDIT_DIRECTION_REVERT)` | the `Zenith_TerrainEdit`, owned by the operation (row EN12) | yes |
| `EDITOR_CHANGE_UI_EDIT` | `Zenith_EditorUIOperation`: the `Zenith_UIEditOperation` | the inverse operation the model returned | the record's arena | yes |
| `EDITOR_CHANGE_ASSET_SETTINGS` | `Zenith_EditorAssetSettingsOperation`: cook server `setSettings` with the new settings and the record's "before" `.meta` digest (execute); a redo starts from a fresh `record` (§ "Asynchronous changes") | `setSettings` with the old settings and the expected digest a fresh `record` gives when its settings equal those the last completed direction wrote (§ "Asynchronous changes") | the record's arena (both settings objects, the digests each completed direction wrote) | no |
| `EDITOR_CHANGE_FILE_OPERATION` | `Zenith_EditorFileOperation`: an asset create, rename, move, delete, duplicate or import, with its reference-repair plan (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Asset operations") | the reverse operation and the inverse repair | the record's arena | no (the file moves are immediate; the server requests are not) |
| `EDITOR_CHANGE_DOCUMENT_BYTES` | write these canonical bytes as the document's state (recovery restore, migration, revert to disk) through `ReplaceState`; a `.meta` entry through `migrateApply` | the previous canonical bytes | the record's arena, uncompressed (counted in the bound) | yes for documents, no for `.meta` entries |
| `EDITOR_CHANGE_SCHEMA` | the schema re-registration of a new revision with its document batch | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo" | the record's arena | no (the window result arrives in a following frame; the undo restarts the runtime) |
| `EDITOR_CHANGE_PLAY_WORLD_EDIT` | a play-world `SetProperty`, `AddComponent` or `RemoveComponent` out of tick (§ "Play-world edits") | the captured previous field set restored out of tick | the record's arena | yes |

Each document change also queues its projection (§ "Projection per edit kind") on apply, forward or inverse. Every kind other than scene and JSON edits is an operation (`Zenith_EditorOperation`): the history drives operations through one interface, so terrain and UI edits, which `ZenithEditorCore` cannot name (they are ENV's and UI's types), are implemented in `ZenithEditor` and handed to the core's history as operations.

### Executing a command

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCommand.h
/// @contract interface Zenith_EditorCommand
/// Ownership: Implemented per action (Internal/Commands/, Internal/<Area>/); instances are constexpr-registered singletons
///   of no state (state lives in the arguments and the record).
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Execute returns Zenith_EditorError; on error nothing it applied remains (§ "Transactions").
/// Reentrancy: NONE. Execute may open the context's transaction and apply changes; it never executes another command.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorCommand
{
public:
	virtual ~Zenith_EditorCommand() = default;
	virtual Zenith_EditorCommandID GetID() const = 0;
	virtual bool IsEnabled(const Zenith_EditorCommandContext& xContext) const = 0;
	virtual Zenith_Result<void, Zenith_EditorError> Execute(Zenith_EditorCommandContext& xContext,
		const Zenith_EditorCommandArgs& xArgs) = 0;
};
```

`Zenith_EditorHistory::Execute(command, args)`:

1. Refuse when a command is executing (`EDITOR_ERROR_COMMAND_REENTRANT`), when an asynchronous record is pending (`EDITOR_ERROR_HISTORY_PENDING`), when the command is disabled, or when it is a document-changing command and play is not EDITING on the main history (play-world commands use the play history).
2. Open a transaction (§ "Transactions"); record the selection before.
3. `Execute`: the command applies changes through `Zenith_EditorCommandContext::Apply(change)`. A synchronous change is applied at once to its document (so each change of the command sees the ones applied before it) and appended to the transaction. The first asynchronous change is begun (`Begin(FORWARD)`), and every change after it is appended unapplied: the history applies them in order as each earlier one completes (§ "Asynchronous changes"). A change whose payload needs an earlier asynchronous change's result (the GUID of an asset that change imports or creates) cannot be built during `Execute`; it is appended as a deferred operation, `Zenith_EditorDeferredSceneOperation` (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header"), which holds a builder function and the index of the change it reads: its `Begin(FORWARD)` reads that change's results through `Zenith_EditorOperationContext::GetChangeResult(uChangeIndex)`, builds the scene edits (the terrain wizard's entity with ENV's terrain component referencing the new GUID, `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Terrain creation wizard"), applies them synchronously and keeps their inverses, and its `Begin(INVERSE)` applies those inverses; a redo reuses the edits built at the first `Begin`, which stay valid because the redo of the change it reads restores the same asset under the same GUID rather than creating a new one (§ "Asynchronous changes", the create, duplicate and import rows).
4. On success with no asynchronous change: record the selection after; commit the transaction as one record, or merge it into the previous record (§ "Interactions and merging"); clear the redo stack; enforce the memory bound (§ "History memory bound"); publish `EDITOR_EVENT_HISTORY_CHANGED` and `EDITOR_EVENT_HISTORY_COMMITTED`. With an asynchronous change: the record becomes the pending record (`EDITOR_RECORD_STATE_PENDING_EXECUTE`), which is on neither stack; the redo stack is cleared when it commits.
5. On error: abort the transaction, which reverts every change applied in step 3 in reverse order (an asynchronous change that began is compensated, § "Asynchronous changes"); nothing is recorded; the error is returned, shown and published as `EDITOR_EVENT_HISTORY_FAILED`.
6. A command that applied no change records nothing (a no-op command never pollutes the history).

### Undo and redo

- `Undo()`: take the newest record of the undo stack; fail with `EDITOR_ERROR_NOTHING_TO_UNDO` when empty and `EDITOR_ERROR_HISTORY_FENCE` when it is a fence; apply each change's inverse in reverse order; set each document's state ID to the change's "before"; restore the selection before; move the record to the redo stack.
- `Redo()`: the mirror: forward changes in order, state IDs "after", selection after.
- An inverse that fails stops the undo, re-applies the inverses already applied in forward order (a compensating redo), and reports `EDITOR_ERROR_UNDO_FAILED`; the document is left exactly as before the attempt. A synchronous inverse fails only after an external change the history did not see (§ "External changes" prunes the reloaded document's records, so none survives to fail), and the record becomes a fence. An asynchronous inverse is driven by § "Asynchronous changes": a transient failure leaves the record on the undo stack, a permanent one fences it.
- Undo and redo are themselves never recorded, and are refused while a transaction or an interaction is open or an asynchronous record is pending (`EDITOR_ERROR_HISTORY_PENDING`); an undo or redo of a record holding an asynchronous change is itself pending until its requests complete.
- The undo menu lists up to 50 labels of each stack; choosing an entry undoes or redoes every record up to it.

### Transactions

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCommand.h: Zenith_EditorCommandContext (Declarations.md "Command header"):
// Apply (synchronous part at once), GetRecordArena, GetSession, IsRecording, SetLabel, SetMergeKey, SetSelectionAfter.
```

A transaction is atomic: either every change of the command is applied and recorded as one record, or none is. Multi-step edits are one transaction: a paste (create subtree plus rebalanced sort keys, `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Sort keys for edits"), "apply overrides to prefab" (a two-document scene edit), a reparent of several entities, an asset rename with its reference repair. `Test_EditorHistory_FailedCommandLeavesNoTrace` injects a failure at the k-th change of a 5-change command for every k and asserts every document's canonical bytes and the history are unchanged.

### Interactions and merging

- An interaction is a continuous gesture: a gizmo drag, a slider drag, a terrain stroke, typing into a text field. `BeginInteraction(ulMergeKey, strLabel)` … `EndInteraction()` bracket it; every command executed inside with the same merge key merges into the interaction's record, whatever the time between them.
- Outside interactions, a command merges into the newest record when both carry the same non-zero merge key, the newest record was created less than `editor.undo.mergeWindowMs` (default 500 ms) ago, no other record was created since, and no undo or redo happened since.
- Merging appends the new changes to the record's change list; for `SET_VALUE` and `SET_PROPERTY` changes on the same target, the record keeps the first change's inverse and replaces its forward value with the newest one, so a 10-second slider drag is one record holding two values.
- Terrain stroke segments (one `ApplyStroke` per editor frame during a drag, `Zenith2/Docs/Design/EDIT/AuthoringTools.md` → "Sculpt and paint strokes") merge by concatenation: the record's terrain changes are reverted newest first, so the revision chain of `Zenith_TerrainEdit` holds.
- Merge keys are `Zenith_Hash64` of (command ID, target document ID, target object ID, property path), computed by the command; 0 never merges.

### Asynchronous changes

<!-- elaboration -->
Five change kinds need cook-server round trips or a following frame (the table of § "Changes inside a record", "Synchronous" column). The history keeps Execute, Undo and Redo atomic from its own point of view by holding such a record outside both stacks until every request of the current direction has completed. A request that ends without a result (cancelled, timed out, or failed by a server restart) has an unknown outcome: the server may have applied it, wholly or in part, before it stopped (`Zenith2/Docs/Design/ASSET/Formats.md` → "Session rules", rule 4: a running request is interrupted "at its next item boundary"). So the history never assumes such a request had no effect: it first **resolves** the outcome from the server's reported state, and only then compensates what actually happened. **A change with a local part never resolves `NO_EFFECT`:** the file moves, copies and repair writes of a file operation ran synchronously in `Begin` before its first request, so they are applied whatever the request did, and such a change resolves `PARTIAL` at once (`Zenith2/Docs/Design/EDIT/Declarations.md` → "Change header", `Zenith_EditorOperation::Resolve`).

| State (`Zenith_EditorRecordState`) | Event | Next | Action |
|---|---|---|---|
| (none) | Execute reaches an asynchronous change | `PENDING_EXECUTE` | the record is the pending record; `editor.undo.pending` = 1; the status bar shows "<label> (pending)" with a cancel button |
| `PENDING_EXECUTE`, `PENDING_UNDO`, `PENDING_REDO` | `Poll` returns DONE for the in-flight change | same | begin the next change of the direction in order (synchronous ones apply at once); when none remains: commit |
| `PENDING_EXECUTE` | commit | `COMMITTED`, on the undo stack | clear the redo stack; enforce the memory bound; publish `EDITOR_EVENT_HISTORY_COMMITTED` (record ID) and `EDITOR_EVENT_HISTORY_CHANGED` |
| `PENDING_UNDO` | commit | `COMMITTED`, on the redo stack | state IDs "before", selection before; publish both events |
| `PENDING_REDO` | commit | `COMMITTED`, on the undo stack | state IDs "after", selection after; publish both events |
| any pending state | `Begin` or `Poll` returns FAILED with a definite server error (an `error` line other than `COOK_ERROR_CANCELLED`: the request did nothing) | `COMPENSATING` | compensate (below) |
| any pending state | the in-flight request ends `COOK_ERROR_CANCELLED` or `EDITOR_ERROR_COOK_SERVER_RESTARTED`; the inactivity timeout expires (the history calls `Cancel`, and the request's end follows); the user's cancel (`CancelPending`, the same) | `RESOLVING` | `Resolve` the in-flight change: its resolution requests read the server's state (the table below) |
| `RESOLVING` | `Resolve` returns `EDITOR_RESOLUTION_COMPLETED` | the pending state it came from | the change counts as DONE (its result taken from the resolution: the `.meta` digest a `setSettings` wrote); the direction continues with the next change, exactly as after DONE |
| `RESOLVING` | `EDITOR_RESOLUTION_NO_EFFECT` (nothing of the change was applied, its local part included) | `COMPENSATING` | a transient failure; the failed change needs no compensation of its own |
| `RESOLVING` | `EDITOR_RESOLUTION_PARTIAL` (some items applied, or a synchronous local part applied) | `COMPENSATING` | a transient failure; the change compensates exactly the parts the resolution found applied; for a change whose requests are all idempotent this is the change's documented compensation, run unchanged (the table below) |
| `RESOLVING` | `EDITOR_RESOLUTION_CHANGED_ELSEWHERE` | `COMPENSATING` | a permanent failure: the target holds a third state no direction wrote; the failed change is not compensated (nothing of it is known to be applied) |
| `RESOLVING` | `EDITOR_RESOLUTION_UNKNOWN` (the resolution requests themselves failed or timed out) | `COMPENSATING` | a permanent failure; the failed change runs its idempotent compensation unchanged (below) |
| `COMPENSATING` | every compensation DONE | before the attempt: execute → no record; undo → `COMMITTED` on the undo stack; redo → `COMMITTED` on the redo stack | publish `EDITOR_EVENT_HISTORY_FAILED` with the error; a permanent failure kind also turns the record into a fence (undo and redo only); `editor.undo.async_failures` += 1 |
| `COMPENSATING` | a compensation FAILED, or its own inactivity timeout expired | a fence `"<label> (incomplete: <reason>)"` on the undo stack | release the record's payload; for every document the record touched: a dirty one is named in `EDITOR_EVENT_HISTORY_FAILED` (from Stage 4 the autosave writes it at once, `Test_EditorAutosave_IncompleteFenceWritesTouchedDocuments`) and stays dirty (CONFLICTED when its file differs from `GetDiskDigest()`), a clean one is left to § "External changes" (§ "Editor-originated writes": no failure discards an unsaved edit); the notification lists each touched file's state on disk; publish `EDITOR_EVENT_HISTORY_FAILED`; `editor.undo.compensation_fences` += 1 |

**Compensating** a direction: `Compensate` the failed change (its own partial work, as resolved), then apply, in reverse, the inverse of every change of this direction already completed (synchronous ones at once, asynchronous ones awaited in turn, each of them able to go through `RESOLVING` itself).

**The inactivity timeout.** A pending change's in-flight request is cancelled when the cook server has sent no line at all (a `progress`, `partial`, `diagnostic`, notification or final line of any request, the stdout queue of `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Server process") for `editor.history.asyncTimeoutSeconds` (default 300 s) while that request is queued or running, and its `stderr` is silent too. The clock restarts at every line, so an `import` of large sources that reports progress, or runs behind a watch-driven run that does, is never cancelled for its total length; a server that is alive but silent for 300 s inside one item (the protocol sends no line between items of one request) is cancelled, which the resolution then treats like any other cancel. Requests waiting in the client's own queue while the server restarts (`RESTARTING`, `WAITING_FOR_LOCK`) are not timed: the client's restart policy bounds them (`FAILED` after 5 crashes within 60 s, which fails them with `EDITOR_ERROR_COOK_SERVER_UNAVAILABLE`, a definite error).

While a record is pending (`IsPending()`, the states `PENDING_*`, `RESOLVING`, `COMPENSATING`), the history is locked:

| Refused with `EDITOR_ERROR_HISTORY_PENDING` | Allowed |
|---|---|
| every command with `EDITOR_COMMAND_FLAG_CHANGES_DOCUMENTS`; Undo, Redo, UndoTo, RedoTo; BeginInteraction; document save, save all, close; Play (§ "Starting play"); the schema and migration applies; "Restore this batch" | selection, camera, view, gizmo-mode and every other command that changes no document; panels drawing; the automation runner's waits; `editor.cancelPending`; autosave, which skips the round (counted `editor.autosave.skipped`) and runs after the commit |

Failure kinds (`Zenith_EditorFailureKind`): transient for `EDITOR_ERROR_COOK_SERVER_RESTARTED`, `EDITOR_ERROR_COOK_SERVER_UNAVAILABLE`, `EDITOR_ERROR_COOK_BUSY`, `EDITOR_ERROR_COOK_CANCELLED`, `EDITOR_ERROR_HISTORY_ASYNC_TIMEOUT` and `EDITOR_ERROR_DOCUMENT_DIRTY_IN_PLAN` when resolved as no effect or partial (the same request can succeed on a retry); permanent for `COOK_ERROR_META_CHANGED`, `DOCUMENT_ERROR_STALE_PLAN`, a destination path that already exists, a schema the window refused, and the resolutions `CHANGED_ELSEWHERE` and `UNKNOWN` (the inverse can never succeed as recorded). A compensation or resolution request waits for a restarting server (the client keeps queued requests across a restart, `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Client state machine") under the same inactivity rule.

The asynchronous changes, how an unknown outcome is resolved, and their compensations:

| Change | Requests of a direction | Resolution of a cancelled, timed-out or restarted request | Compensation of a failed direction |
|---|---|---|---|
| asset settings | execute: one `setSettings` with `expectedMetaSha256` = the `metaSha256` of the `record` the inspector applied from ("before"). Undo and redo: first a fresh `record {guid}`, because the reimport that follows every settings apply may have rewritten the `.meta` since the last direction (`Zenith2/Docs/Design/ASSET/Import.md` step 5 reconciles products with the `.meta` sub-assets, and the server's `import` writes the sidecar: an added, removed or orphaned sub-asset changes the digest); when, by the settings comparison (below), that `record` holds the settings the record's last completed direction wrote, its `metaSha256` is the `expectedMetaSha256` of the direction's one `setSettings`; otherwise the `.meta` was changed elsewhere, which is permanent: the record becomes a fence "settings changed on disk; undo not possible" ("redo not possible" for a redo), nothing is written, and the asset inspector refreshes the record | `record {guid}`: by the settings comparison the record holds the direction's target settings → `COMPLETED`, the record's `metaSha256` becoming the direction's result digest; it holds the direction's starting settings → `NO_EFFECT`; anything else → `CHANGED_ELSEWHERE` | `NO_EFFECT`: none. `COMPLETED` of an earlier change but a subsequent failure: a fresh `record`; holding the direction's target settings → `setSettings` back to the starting settings with that `record`'s digest expected; anything else → no write and the "incomplete" fence naming the `.meta`. `UNKNOWN` (the resolution's own `record` failed): it is re-resolved once the server answers, with a fresh `record`: its settings equal the target → `setSettings` back to the starting settings with that `record`'s digest; they equal the start (its digest equal to the starting digest included) → nothing; anything else → no write and the "incomplete" fence naming the `.meta` (a third writer's change, a `git pull` while the server was down, is never overwritten). A `setSettings` meeting `COOK_ERROR_META_CHANGED` (the `.meta` changed between the fresh `record` and the write) is permanent: the fence above |
| rename and move | local: `.meta` then source renamed under the root's writer lock; the repair plan's writes (§ "Editor-originated writes"); then `assignGuids`, then `reimport` | `PARTIAL` at once, with no request: the local part applied, and `assignGuids` of a path whose `.meta` exists returns that `.meta`'s GUID and creates nothing, and `import` and `reimport` re-run the import of the listed GUIDs from their current sources, so each is idempotent and the compensation is correct whether or not the request took effect (`UNKNOWN` is never reported for them) | rename both files back under the writer lock, apply the plan's inverse writes, then `assignGuids` of the old paths and `reimport` of the GUID |
| import | execute: local: the file copied into the folder; then `assignGuids` (which creates the `.meta` and the GUID, published in `Zenith_EditorChangeResult::m_axCreatedAssets`), then `import`. Undo: the copied file and its `.meta` moved into `.editor/trash/<UTC timestamp>/` (the directory recorded in the record, with its `trash.json`, `Zenith2/Docs/Design/EDIT/Formats.md` → "Editor trash layout"), then `import` of the GUID (which becomes MISSING). Redo, the inverse of the undo: both files moved back from the recorded trash directory (their `trash.json` entries removed), then `assignGuids` of the restored path, which returns the restored `.meta`'s GUID and creates nothing, then `import`; a returned GUID other than the recorded `m_axCreatedAssets` entry fails the redo permanently (a fence "the restored file no longer has its GUID"), so a redone record never names a second GUID | `PARTIAL` at once (idempotent, as above) | execute: move the copied file and its `.meta` (when one was created) into the editor trash, then `import` of the GUID (which becomes MISSING); redo: move both back into the trash directory they came from, then `import` of the GUID |
| create and duplicate | execute: local: the document written (and, for a duplicate, the `.meta` copied without its `guid`); then `assignGuids`, then `import`. Undo and redo: as import's (the trash and back, the recorded GUID asserted on redo) | `PARTIAL` at once | as import's |
| delete | local: source and `.meta` moved into the trash with the repair plan's writes; then `import {guids: [g]}` of the GUID, which marks it MISSING | `PARTIAL` at once | move both back and apply the inverse writes; `assignGuids` of the restored path, then `import` of the GUID |

**The settings comparison** (used by the asset-settings row): a `record` result holds a given settings state when its `settings` object and its `platforms` object, each written as canonical JSON bytes (one `Zenith_JsonWriter` form for both sides), are byte-equal to that state's two objects; the `.meta`'s other members (`subAssets`, `labels`, the import key) and its digest take no part, so a reimport that rewrote only them never fences an undo.
| `.meta` migration entries | forward: one `migrateApply` with the preview token; inverse: the backup batch's `.meta` bytes written back through the game root's `SaveDocuments` with the post-migration digests expected, then `reimport` of the entries' GUIDs | `migratePreview {paths}` over the direction's entries: an entry whose plan now reports it unchanged at the target version was applied. Every entry applied → `COMPLETED`; none → `NO_EFFECT`; some → `PARTIAL` with the applied set | `PARTIAL`, or a subsequent failure after `COMPLETED`: the inverse above for exactly the applied entries. A token mismatch (`COOK_ERROR_META_CHANGED`) is permanent for an undo and re-plans for an execute |
| schema change | the schema window opened at the next prologue step 6; `OnSchemaWindowResult` | none: the window result always arrives (the runtime reports success or the first refusal in the frame it ran) | the documents' batch restored from its backup (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Schema undo" uses the same restore) |
| CONTENT batch | `Zenith_ContentBatchOperation` chunks through the content cook channel | the batch's journal (`Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Operation journal"): its completed chunks are the applied part | `Rollback` of the batch (`Zenith2/Docs/Design/CONTENT/BatchOperations.md` → "Abandon and rollback", row CT18) |

Automation waits on the events: a `command` step whose record is asynchronous completes on `EDITOR_EVENT_HISTORY_COMMITTED` for its record ID and fails on `EDITOR_EVENT_HISTORY_FAILED` (`Zenith2/Docs/Design/EDIT/Automation.md` → "Running a script"); the live link answers its line at the same moment.

Tests:

- `Test_EditorHistory_AsyncChangeLocksHistory` (`ZenithEditorCore_Tests`) drives a scripted asynchronous operation and asserts every history, document-table and migration-model refusal of the lock table, that the record is on neither stack while pending, and that commit, transient failure and permanent failure each leave the stacks and documents as the state table says; a failed compensation leaves an "incomplete" fence, autosaves a dirty touched document and leaves it dirty with its unsaved state; the Play and schema refusals are `Test_EditorPlay_RefusedWhilePendingOrUnresolved` and `Test_EditorSchemaPanel_ApplyRefusedDuringPlay` (`ZenithEditor_Tests`).
- `Test_EditorHistory_AsyncTimeoutIsInactivity` (`ZenithEditorCore_Tests`, a test clock and the fake server): a request whose server sends a `progress` line every 200 s runs 900 s and commits; one whose server sends nothing for `editor.history.asyncTimeoutSeconds` is cancelled at that point, resolved, compensated, and reported with `EDITOR_ERROR_HISTORY_ASYNC_TIMEOUT`.
- `Test_EditorAssetOps_SettingsCancelledAfterWriteCommits` (`ZenithEditor_Tests`): the fake server's script "apply, then answer `COOK_ERROR_CANCELLED`" for a `setSettings`, then a `record` showing the new settings: the record resolves `COMPLETED` and commits with the recorded digest; with a `record` showing the old settings (the old digest) it resolves `NO_EFFECT` and nothing is recorded; with a third writer's settings it resolves `CHANGED_ELSEWHERE` and nothing is recorded (execute) or the record becomes a fence (undo); with the resolution's `record` itself failing (`UNKNOWN`), the re-resolution's fresh `record` showing the new settings sends `setSettings` back to the old settings with that digest, one showing the old settings sends nothing, and one showing a third writer's settings sends nothing and leaves the "incomplete" fence naming the `.meta`.
- `Test_EditorMigration_MetaPartialApplyCompensates` (`ZenithEditorCore_Tests`): a `migrateApply` of three entries cancelled after the second item: `migratePreview` shows two applied; the resolution is `PARTIAL`, the compensation writes those two entries' backup bytes with their post-migration digests expected, and the three `.meta` files end at their original bytes with nothing recorded.
- `Test_EditorAssetOps_UndoSettingsServerConflict` applies settings through the fake cook server, changes the `.meta`'s settings behind it, undoes, and asserts the fence after the fresh `record`, no `setSettings` sent, and the unchanged `.meta`; `Test_EditorAssetOps_SettingsUndoAfterSubAssetRewrite` applies settings whose reimport (the fake server's scripted `import`) rewrites the `.meta`'s `subAssets` after the forward, undoes, and asserts the undo commits and restores the old settings (its `setSettings` carrying the fresh `record`'s digest), then redoes and asserts the new settings again; `Test_EditorAssetOps_RenameServerRestartCompensates` kills the fake server during `assignGuids` and asserts the resolution `PARTIAL` (`editor.undo.resolutions`), both files back at their old names, the hints restored, `assignGuids` of the old paths sent once the server is back, and nothing recorded; `Test_EditorAssetOps_RedoCreateKeepsGuid` creates a document, undoes and redoes it, and asserts the redo moved the file and its `.meta` back from the recorded trash directory, sent no `assignGuids` that created anything, and left the GUID equal to the first one; with the trashed `.meta` replaced by one of another GUID, the redo fails permanently and fences (all `ZenithEditor_Tests`).

### History memory bound

<!-- elaboration -->
| Key | Default | Meaning |
|---|---|---|
| `editor.undo.maxRecords` | 1,000 | records kept on the undo stack |
| `editor.undo.maxBytes` | 512 MiB | the sum of `m_ulMemoryBytes` over both stacks; the pending row "Editor undo history memory" (row ED24) allocates the value |
| `editor.undo.maxTerrainRecordBytes` | the value of Z2-B-EDIT-011 "Terrain edit delta memory per undo step" once allocated, until then 256 MiB | the largest record holding a terrain change ([Z2-N-BUDGET-328] bounds one terrain undo step) |
| `editor.undo.maxRecordBytes` | 256 MiB | the largest record of any other kind, except document-bytes and schema records |

- After each commit: while either history limit is exceeded, evict the oldest undo record (whole records; its arena and operations released), then, if still exceeded, the oldest redo record. Eviction publishes `EDITOR_EVENT_HISTORY_CHANGED` and counts `editor.undo.evicted`.
- A record holding a terrain change larger than `editor.undo.maxTerrainRecordBytes` (a huge terrain stroke, `environment.terrain.edit_over_budget` counting the same event on ENV's side, `Zenith2/Docs/Design/ENV/Terrain.md` → "Terrain edit deltas and undo"), or a record of another kind larger than `editor.undo.maxRecordBytes`, is applied but not kept: its payload is released, the record is replaced by a fence labelled "<label> (too large to undo)", and the notification says so; ENV's delta encoding makes such a stroke rare (memory proportional to the area touched).
- Records of `EDITOR_CHANGE_DOCUMENT_BYTES` and `EDITOR_CHANGE_SCHEMA` changes (a migration of many documents, a schema change with its batch) have no per-record cap: they are limited by `editor.undo.maxBytes` alone, older records being evicted to make room, so "the whole apply is one history record, undoable in the session" holds for every apply that fits the history. One that alone exceeds `editor.undo.maxBytes` becomes a fence whose notification names its backup batch, which "Restore this batch" restores (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Restoring a backup").
- `Test_EditorHistory_MemoryBoundEvictsOldest`, `Test_EditorHistory_OversizeRecordBecomesFence` and `Test_EditorHistory_MigrationRecordExemptFromRecordCap` assert the three rules.

### History fences

A fence is a record that cannot be undone, and undo stops at it. A fence is created by: an oversize record (§ "History memory bound"); a failed synchronous undo (§ "Undo and redo"); a permanent failure of an asynchronous undo or redo, or a failed compensation (§ "Asynchronous changes"). Undo is strictly last-in first-out over one session history, so a fence stops undo for every document, including records older than the fence that touch other documents: per-document undo across one global history would reorder changes between documents that a single record links. Redo is unaffected by a fence that is older than the redo stack. Closing a document and reloading a document from disk do not create a fence; they prune (§ "Closing a document and its history").

### Closing a document and its history

Closing document D, or reloading it from disk (§ "External changes"), removes from both stacks every record that touches D; for each other document E touched by a removed record, every record touching E that is older than the newest removed record touching E is also removed, so no remaining record depends on a state that no longer exists. Records touching neither D nor such an E stay. The count removed is shown ("Closing D removes n undo steps"); in automation the close proceeds. `Test_EditorHistory_CloseDocumentPrunesDependentRecords` covers a record spanning D and E, and `Test_EditorHistory_ReloadPrunesOnlyThatDocument` asserts that after an external change of D the records of an unrelated document F still undo.

### Dirty tracking

Each document keeps a state ID: a 64-bit value from the session's monotonic counter, assigned anew by every applied change. Each change records the document's state ID before and after; undo restores "before", redo restores "after". The document is dirty exactly when its state ID differs from the state ID at its last save or open. So undoing back to the saved state makes the document clean again, and a redo away from it dirty, without hashing the document.

### Round-trip invariants

For every document kind and every command:

1. Execute then Undo leaves every touched document's canonical bytes (`Serialize`) and the edit world's projected state (`HashTickState` of the edit world after a reconcile-free projection) equal to their state before Execute.
2. Undo then Redo leaves them equal to their state after Execute.
3. Any interleaving of n commands, undos and redos ends in the state that the same commands without the undone suffix produce.

`Test_EditorUndo_PropertySequences` (`ZenithEditorCore_Tests`, a document-only session) checks 3 with 10,000 random sequences of up to 200 operations over a scene and a JSON animation-graph document, through test-local commands of the five scene-edit and six JSON-edit kinds, comparing against a model that replays the surviving commands on a fresh copy; every command that creates objects carries its IDs as arguments drawn from the test's own seeded generator (`entity.create`'s `id`, `edit.paste`'s `idSeed`, § "Session random source"), so the actual run and the model create identical IDs; the seed is fixed per case and printed on failure. `Test_EditorUndo_TerrainPropertySequences` (`ZenithEditor_Tests`, which links ENV and `Zenith_EditorTerrainOperation`) runs the same check with 2,000 sequences of up to 100 operations of strokes, structural terrain edits, undos and redos over the fixture terrain document. `Test_EditorUndo_EveryCommandRoundTrips` (`ZenithEditor_Tests`, which registers the built-in commands of `Internal/Commands/`) executes each registered document-changing command once on its fixture and asserts 1 and 2 for it, so a new command without a correct inverse fails the gate.

## Selection

### Selection model

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorSelection.h
enum Zenith_EditorObjectKind : uint8
{
	EDITOR_OBJECT_KIND_ENTITY = 1,         // m_xObject: an authoring ID (an entity or a resolved instance entity)
	EDITOR_OBJECT_KIND_INSTANCE,           // m_xObject: a prefab-instance object ID
	EDITOR_OBJECT_KIND_COMPONENT,          // m_xObject: the entity; m_ulSub: Zenith_MakeTypeID of the stable name
	EDITOR_OBJECT_KIND_ASSET,              // m_xObject: an asset GUID (asset browser selection)
	EDITOR_OBJECT_KIND_JSON_NODE,          // m_xObject: the document; m_ulSub: Zenith_Hash64 of the pointer; pointer text kept
	EDITOR_OBJECT_KIND_TERRAIN_TILE,       // m_xObject: the terrain document; m_ulSub: (z << 16) | x
	EDITOR_OBJECT_KIND_UI_ELEMENT,         // m_xObject: the UI document; m_xSubID: the element's uid
};

/// @contract value Zenith_EditorObjectRef
/// Ownership: Plain value of 64 bytes (the kind is padded to 8 by the alignment of the members after it).
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None: it names objects by persistent IDs, never by handles.
/// Units: None.
/// Errors: None; a reference to an object that no longer exists resolves to nothing (Resolve returns EDITOR_ERROR_NOT_FOUND).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorObjectRef
{
	Zenith_EditorObjectKind m_eKind;
	Zenith_EditorDocumentID m_xDocument;     // the owning document (null for ASSET)
	Zenith_PersistentID m_xObject;
	Zenith_PersistentID m_xSubID;            // UI element uid; null otherwise
	uint64 m_ulSub = 0;
};

/// @contract type Zenith_EditorSelection
/// Ownership: One per session (the main selection) and one per play world; owns its ordered reference array.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Counts.
/// Errors: EDITOR_ERROR_SELECTION_LIMIT above editor.selection.maxObjects (default 100,000) objects; the set is unchanged.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorSelection
{
public:
	Zenith_Span<const Zenith_EditorObjectRef> Get() const;            // selection order
	const Zenith_EditorObjectRef* GetActive() const;                 // the last one added; null when empty
	Zenith_Result<void, Zenith_EditorError> Set(Zenith_Span<const Zenith_EditorObjectRef> axRefs);
	Zenith_Result<void, Zenith_EditorError> Add(Zenith_Span<const Zenith_EditorObjectRef> axRefs);
	void Remove(Zenith_Span<const Zenith_EditorObjectRef> axRefs);
	Zenith_Result<void, Zenith_EditorError> Toggle(const Zenith_EditorObjectRef& xRef);
	void Clear();
	bool Contains(const Zenith_EditorObjectRef& xRef) const;         // O(1): a hash set beside the array
	uint64 GetVersion() const;                                       // +1 per change
	Zenith_EditorSelectionSnapshot Snapshot(Zenith_Arena& xArena) const;
	Zenith_Result<void, Zenith_EditorError> Restore(const Zenith_EditorSelectionSnapshot& xSnapshot);
	// The selected entities' roots: entities none of whose ancestors is selected (transform edits apply to these only).
	uint32 ListRootEntities(Zenith_Span<Zenith_EditorObjectRef> axOut) const;
};
```

### Selection operations

| Gesture | Operation |
|---|---|
| click (viewport pick, hierarchy row, asset tile) | `Set({hit})`; a click on empty space `Clear` |
| `Ctrl+click` | `Toggle(hit)` |
| `Shift+click` (hierarchy, asset browser) | range: every row between the active one and the hit in display order, `Add` |
| `Shift+click` (viewport) | `Add({hit})` |
| marquee drag in a viewport | every entity whose world bounds intersect the marquee's frustum (`Zenith2/Docs/Design/EDIT/Viewports.md` → "Marquee selection"); replaces, adds with `Shift`, removes with `Alt` |
| `Ctrl+A` | every entity of the active scene set |
| select children / parent / siblings (hierarchy context menu) | the obvious sets |
| "Select referencing" (asset browser) | every entity whose components reference the selected asset, from SCENE's `Zenith_ReferenceIndex` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_ReferenceRepair header") |

### Selection persistence and mapping

- A reference names objects by persistent ID, so it survives undo, redo, reconcile, reload and a rename. A reference whose object no longer exists is kept in the array but resolves to nothing; panels skip it; the next `Set` drops it. Deleting the selected entity by command removes it from the selection after-state.
- In play, the main selection maps to play-world entities by persistent ID (`FindByPersistentID` in the play world), because the play world's entities carry the authoring IDs (rule 5 of `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Identity rules the editor foundation builds on"); entities the game spawned at run time have no document object and are selected in the play selection alone.
- The session file stores the main selection's references, so a session reopens with the same selection (`Zenith2/Docs/Design/EDIT/Formats.md` → "Session file").

### Selection and undo

Selection changes are not records of their own: every record stores the selection before and after its command, and undo and redo restore them. Clicking around therefore never fills the history, while undoing a deletion re-selects what was deleted. `Test_EditorSelection_UndoRestoresSelection` asserts it.

## Clipboard

### Copy cut paste and duplicate

| Command | Behaviour |
|---|---|
| `edit.copy` | for scene selections: `Zenith_SceneEditSet::Copy(document, root IDs, arena)` (subtrees included) into the editor clipboard, plus the canonical JSON of the set on the system clipboard as `{"$schema": "zenith2.editor-clipboard", "schemaVersion": 1, "objects": …}` (PLAT's clipboard, `Zenith2/Docs/Design/PLAT.md` → "Editor surfaces PLAT exposes"); for JSON selections: the selected subtrees; for assets: the GUID list |
| `edit.cut` | copy, then delete the selection as one record |
| `edit.paste` | `Zenith_RemapAuthoringIDs(set, xRandom, arena)` (fresh IDs, internal references rewritten, `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_AuthoringID header"), then `Zenith_PasteEditSet` under the active entity's parent (or the root), sort keys by SCENE's paste rule; one record; the pasted roots become the selection |
| `edit.duplicate` (`Ctrl+D`) | copy then paste beside the originals (under each original's parent, immediately after it in sibling order) without touching the system clipboard |

- Pasting from the system clipboard parses the JSON with the default limits and refuses (`EDITOR_ERROR_CLIPBOARD_FORMAT`) anything that is not a `zenith2.editor-clipboard` document of a version the editor knows; a component the target game does not register is pasted as preserved unknown data, as the document model keeps it.
- Copy and paste between the edit world and a play world (the "Copy from play" context action) captures the play entities with `Zenith_CaptureEntities` and converts the field sets to document JSON, then pastes as above ([Z2-N-ARCH-762]: by persistent ID, at a tick boundary).
- The random source for remapping is the session's (§ "Session random source"): unique within a session, and reproducible when the session is seeded or the command carries `idSeed`.
