# ASSET design: the cook

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It specifies `ZenithCook`'s command line, its composition, the cook step interface every area's steps implement, planning, incremental and dependency-driven cooking, execution (deterministic ordering, worker processes, atomic publication), validation, the global outputs (catalogue, mount manifest, configuration merge, depfile, report), resume after interruption, low disk, stale-output removal and the cook server EDIT uses. Keys and caching are `Zenith2/Docs/Design/ASSET/DDC.md`; formats are `Zenith2/Docs/Design/ASSET/Formats.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| invoking the cook | "Cook command line" |
| the tool's composition root | "Cook services and composition" |
| what a step is | "Cook steps" |
| deciding what to cook | "Planning" |
| cooking only what changed | "Incremental cook" |
| running steps | "Execution" |
| checking outputs | "Validation" |
| whole-game outputs | "Global outputs" |
| failure modes of the run | "Interrupted cook and resume", "Low disk during a cook", "Stale output removal" |
| the editor's path | "The cook server" |

## Cook command line

### Options

| Mode | Command | Writes |
|---|---|---|
| cook (the build's `<Name>_Content`) | `ZenithCook --game <Name> --platform <windows\|android> --out <dir> [--clean] [--verify] [--workers <n>] [--in-process] [--set <key>=<value>]…` | `<dir>` (the cooked tree), `${ZENITH_OUT}/AssetDB/<Name>/`, the local DDC; never the source tree |
| types (`<Name>_Types`) | `ZenithCook --game <Name> --step luau-types` | `${ZENITH_OUT}/Generated/Luau/<Name>/` (SCRIPT's step) |
| assign GUIDs | `ZenithCook --game <Name> --assign-guids` | `.meta` files for sources without one and the tombstone lists, through the framework's journaled write (Migration.md "Reading and writing meta documents through the framework"), holding every writable storage root's writer lock for the run ("Output tree and database locks") |
| fix duplicates | `ZenithCook --game <Name> --fix-duplicate-guids` | the changed `.meta` files, under the same writer locks |
| migrate | `ZenithCook --game <Name> --migrate [--preview]` | `.meta` documents older than the tool, through the framework's plan and apply (Migration.md "Reading and writing meta documents through the framework"); `--preview` writes the plan report to stdout and nothing else; both hold the storage roots' writer locks for the run, `--preview` too, because it recovers journals |
| provenance | `ZenithCook --game <Name> --platform <p> --provenance <guid\|logical path\|cooked path>` | stdout |
| validate | `ZenithCook --game <Name> --platform <p> --out <dir> --validate-only` | `<dir>/.cook/validation-report.json` |
| serve | `ZenithCook --serve --game <Name> --platform <p> --out <dir>` | as cook and assign, on request; holds `<dir>`'s output lock for the whole session and a storage root's writer lock for each sidecar-writing request that touches the root ("Output tree and database locks") |
| worker | `ZenithCook --worker <pipe name>` | nothing directly: runs steps for its parent |
| schema lock | `ZenithCook --game <Name> --schema-lock` (ECS's verb, rows EB12 and ED35 (b)) | `Games/<Name>/Schemas/schemas.lock.json` alone, through ECS's writer, holding only the game storage root's writer lock for its one write and no output-tree or database lock, so it runs while a `--serve` holds the tree; the lock held for `cook.sourceLockWaitMs` is exit 6 with `COOK_ERROR_SOURCE_LOCKED` |

- `--game-root <dir>` (every mode; cross-area row ED35 (a)) names the directory holding `<Name>/game.json`, default `Zenith2/Games`; it must be `Zenith2/Games`, a directory under `${ZENITH_OUT}`, `Zenith2/Tests/` or `Zenith2/Tools/ZenithCook/Tests/Fixtures/`, else exit 2 with `COOK_ERROR_BAD_COMMAND_LINE`, so a test cooks and serves a scratch copy under `${ZENITH_OUT}/test-fixtures/<AREA>/Games/` and never writes the tracked game ([Z2-N-ARCH-025]). The game's storage root is then `<dir>/<Name>/`; logical paths, keys and outputs do not depend on `<dir>`.
- `--only @<file>` (cook mode; cross-area row CT23) restricts the run to `Zenith_CookRunDesc::m_axOnly`: `<file>` holds LF-separated GUID lines (36-character text, no blank lines, at most 1,048,576 lines, at most 64 MiB); a source GUID selects every product of the source; the run cooks those assets and their hard-reference and cook-input dependencies into `--out`, populates the DDC as any cook does, runs no global step, writes no catalogue, mount manifest or depfile, and exits 0 when every run item succeeded and 1 otherwise. A malformed line, a GUID with no live record or an unreadable file is exit 2 naming the line. It supersedes CT7's `cook.partition` key, withdrawn.
- `--clean` ignores the DDC for lookups (it still populates it) and cooks every item; `--verify` cooks every item and compares with the DDC entry byte for byte (DDC.md "Verification sampling" at 100 %).
- `--platform linux` exists for test fixtures of the headless host and is refused for any game directory outside `Zenith2/Tests/` and `Zenith2/Tools/ZenithCook/Tests/Fixtures/`.
- `--set` keys are the cook keys of DDC.md "Zenith_DerivedDataCache" and the ones below; they are tool options, not runtime configuration. ZenithCook's modes and options are a closed set: no step registers a command-line verb. A step that needs an operator input declares a step-owned cook key, which joins the cook-key table in that step's registration diff and is read through the services' command line (RPROD's `cook.permutationLimitPerGame`; UI's translator exchange, cross-area row UI17, as `cook.ui.exportXliff=<locale>` and `cook.ui.importXliff=<file>` read by UI's GLOBAL_POST step, which writes and reads its files under `<out>/.cook/`).
- Every mode that writes `<dir>` (cook, validate, serve) takes the output tree's lock before reading anything, every mode that would save the database takes the database lock or works on a read-only copy, and every sidecar-writing mode takes each writable storage root's writer lock before recovering that root's journals ("Output tree and database locks").

| Cook key | Meaning | Default |
|---|---|---|
| `cook.workers` | worker count; 0 = logical cores − 1 | 0 |
| `cook.outOfProcess` | run `m_bOutOfProcess` importers and steps in worker processes | `true` |
| `cook.lowDiskMarginBytes` | free space kept on the output volume | 1073741824 (1 GiB) |
| `cook.workerTimeoutSeconds` | a worker item's wall-clock limit | 600 |
| `cook.sourceLockWaitMs` | how long a source-tree writer, or a build cook that finds a live writer's journal, waits for a storage root's writer lock (polling every 50 ms) before refusing with exit code 6 | 10000 |

### Exit codes

| Code | Meaning |
|---|---|
| 0 | success; every output published; warnings allowed |
| 1 | at least one item failed (validation errors included); successfully cooked independent items are published; the report lists the failures |
| 2 | bad command line or cook key |
| 3 | the game cannot be cooked at all: `game.json` or `composition.json` unreadable, the database unopenable, a duplicate GUID, a logical-path collision, a missing `.meta` |
| 4 | low disk: stopped before publishing the batch that would cross the margin |
| 5 | internal failure (out of memory, a worker pool that cannot start, a storage root's writer-lock file that cannot be created or opened: `COOK_ERROR_IO` naming the root) |
| 6 | the output tree is locked by another ZenithCook process (a cook server, or another cook of the same tree): `COOK_ERROR_OUTPUT_LOCKED` naming the holder's PID and mode, nothing read, written or deleted; or a storage root's writer lock stayed held for `cook.sourceLockWaitMs` — a sidecar-writing mode refused it, or a build cook found a live writer's journal — `COOK_ERROR_SOURCE_LOCKED` naming the root, the holder's PID and mode, nothing written; or, in a sidecar-writing CLI mode, `COOK_ERROR_META_CHANGED`: a `.meta` changed between its open and its write inside the writer-lock hold, nothing was written; retry |

A failing cook writes its report before exiting, and the depfile only on 0 and 1 (so Ninja reruns a cook that stopped early). Exit code 6 writes no report: for `COOK_ERROR_OUTPUT_LOCKED` the report lives in the locked tree, and for `COOK_ERROR_SOURCE_LOCKED` and `COOK_ERROR_META_CHANGED` nothing was cooked.

## Cook services and composition

`Internal/Zenith_CookServices.cpp` is ZenithCook's composition root (tools are their own composition roots, [Z2-N-CONTRACT-050]). It creates, in this order, destroying in reverse:

1. the root heap and profiler hooks; the command line;
2. the job system; a `Zenith_VirtualFileSystem` whose loose mount is the output tree (so validation reads cooked files exactly as the runtime does); `Zenith_AsyncIO`; both compression codecs into a `Zenith_CompressionCodecSet`;
3. the `Zenith_TypeRegistry` in the runtime's order ([Z2-N-ARCH-847]): the engine types through ZenithRuntime's engine-type registration (ZenithAssets' `Zenith_RegisterAssetTypes` among them), each module of `composition.json` in canonical order through `Zenith_Module_<NAME>_RegisterTypes`, `InstallValueKindHooks(PROPERTY_TYPE_ASSET, Zenith_GetAssetHandleValueKindHooks())`, then the game's schemas by the `ecs.schemas` step's validation pass, then `Freeze`;
4. the importer registry, frozen, in this order: ASSET's passthrough importers; CONTENT's `Zenith_RegisterContentImporters` (cross-area row CT4); WORLD's `Zenith_RegisterWorldImporters`, which claims `.world.json` (row WD5); then every module's registration function in canonical module order (UI's `Zenith_RegisterUIImporters`, CINE's `cine.video` importer in `Internal/Cinematics/`, row CN1 (c), and the others); a registration that claims an extension already claimed fails `Create` with `IMPORT_ERROR_INVALID_DESC` naming both importers;
5. the `Zenith_MigrationRegistry` (SCENE's framework): `Zenith_RegisterAssetMetaMigrations` and the other types of Migration.md "Registered document types" in that table's order, CONTENT's `Zenith_RegisterContentDocumentTypes` included (row CT4), frozen; one `Zenith_HostDocumentStorage` and one `Zenith_DocumentMigrator` per content storage root, read-only in the build's cook; the recovery or refusal of Migration.md "Recovery before a cook";
6. the cook step registry (every area's step registration function, "Step registration functions", in a fixed order: ASSET, ECS, PLAT, SCRIPT, GEOM, RPROD, SCENE, CONTENT (`Zenith_RegisterContentCookSteps`, row CT4), then modules' steps in canonical module order, CINE's `Zenith_RegisterCinematicCookSteps` among them, row CN1 (d)), frozen; the content-rule table ("Content rules");
7. the DDC: the local store, and when `cook.ddc.sharedMode` is not `off` the shared store CONTENT's `Zenith_CreateSharedDerivedDataStore` (`Zenith2/Tools/Authoring/Zenith_AuthoringContent.h`, cross-area rows CT4 and AB9) creates from `cook.ddc.sharedRoot` and `cook.ddc.sharedTimeoutMs`; a creation failure logs `COOK_WARNING_DDC_UNAVAILABLE` once and the run continues with the shared mode off, and a ZenithCook built before CONTENT's factory call lands treats every shared mode as `off` with the same warning; the database (whose scan runs in "Plan construction"); after the scan, `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, &Zenith_ResolveCookAssetReferenceDefault, &database)` on the scratch type registry (`Zenith2/Docs/Design/ASSET/Database.md → "Reference defaults in the cook"`).

<!-- assumption: pending ARCH (X109, X114), MOD -->
This needs ZenithCook to link `ZenithRuntime`'s engine-type registration, every module library, and SCENE's `ZenithDocument` and `ZenithDocumentHost` (core-doc items X114 (a) and X109 (a), pending); each area's cook steps compile inside ZenithCook's `Internal/<Area>/` directories, and `ZenithCook_Tests` links the tool's internals through the object library of X114 (b) (pending).
<!-- /assumption -->

### Driver, server and worker pool API

```cpp
// Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_Cook.h
/// @contract value Zenith_CookRunDesc
/// Ownership: Plain value read by Zenith_Cook::Run; strings borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Run validates it (COOK_ERROR_BAD_COMMAND_LINE naming the field).
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_CookRunDesc
{
	const Zenith_AssetPlatform m_ePlatform = ASSET_PLATFORM_WINDOWS;
	const Zenith_StringView m_strOutDirectory = {};  // host path of the cooked tree
	const Zenith_Span<const Zenith_AssetGUID> m_axOnly = {}; // empty: every item; else these assets and their dependencies (the server's cook)
	const bool m_bClean = false, m_bVerify = false, m_bValidateOnly = false, m_bInProcess = false;
	const uint32 m_uWorkers = 0;                    // 0: cook.workers
	bool (* const m_pfnIsCancelled)(void* pUserData) = nullptr; // the server's request: polled at every item boundary
	void* const m_pCancelUserData = nullptr;
};

/// @contract value Zenith_CookSummary
/// Ownership: Plain value returned by Run.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. Tool-side; the report's "summary" object serializes it.
/// @endcontract
struct Zenith_CookSummary
{
	uint32 m_uItems = 0, m_uCooked = 0, m_uUpToDate = 0, m_uDDCLocalHits = 0, m_uDDCSharedHits = 0, m_uFailed = 0, m_uWarnings = 0;
	uint64 m_ulBytesWritten = 0;
	uint64 m_ulContentHash = 0;
	bool m_bCatalogueChanged = false;
	uint8 m_uExitCode = 0;                          // "Exit codes"
};

/// @contract type Zenith_Cook
/// Ownership: Created by Create over the cook services; owned by main or by the cook server; borrows the services.
/// Thread affinity: OWNER_THREAD (the tool's main thread); items run on workers and worker processes internally.
/// Invalidation: None.
/// Units: None.
/// Errors: Run returns the summary with its exit code; Zenith_CookError values of failed items are in the report.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
class Zenith_Cook
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Cook>, Zenith_CookError> Create(Zenith_CookServices& xServices);
	Zenith_CookSummary Run(const Zenith_CookRunDesc& xDesc);
	Zenith_Span<const Zenith_CookDiagnostic> GetDiagnostics() const;   // of the last Run
};

// Zenith2/Tools/ZenithCook/Internal/Server/Zenith_CookServer.h
/// @contract type Zenith_CookServer
/// Ownership: Created by Create over the cook services and the two streams; owned by main in --serve mode, or by a test.
/// Thread affinity: OWNER_THREAD for Run; the stdin reader is a dedicated thread the server starts and joins.
/// Invalidation: None.
/// Units: None.
/// Errors: Run returns the process exit code (0 after shutdown or EOF; 2 after a protocol version mismatch or 16 malformed lines).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Tool-side; the protocol is Formats.md "Cook server messages".
/// @endcontract
class Zenith_CookServer
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_CookServer>, Zenith_CookError> Create(Zenith_CookServices& xServices,
		Zenith_LineStream& xInput, Zenith_LineStream& xOutput);
	uint8 Run();
};

// Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookWorkers.h
/// @contract type Zenith_CookWorkerPool
/// Ownership: Owned by Zenith_Cook; owns up to cook.workers child processes, started lazily and ended by workShutdown.
/// Thread affinity: ANY_THREAD for Submit; results arrive on the driver's completion list.
/// Invalidation: None.
/// Units: Seconds for the timeout.
/// Errors: Submit fails an item with COOK_ERROR_WORKER_CRASHED, COOK_ERROR_WORKER_TIMEOUT or COOK_ERROR_PROTOCOL_VERSION.
/// Version: since Z2 Stage 4. Tool-side; the protocol is Formats.md "Worker messages".
/// @endcontract
class Zenith_CookWorkerPool
{
public:
	Zenith_Result<void, Zenith_CookError> Submit(const Zenith_CookWorkItem& xItem);
	void Shutdown();
};
```

```cpp
// Zenith2/Tools/ZenithCook/Internal/Zenith_CookServices.h
/// @contract enum Zenith_CookMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Zenith_ParseCookCommandLine selects exactly one.
/// Version: since Z2 Stage 2. Tool-side; the modes of "Options".
/// @endcontract
enum Zenith_CookMode : uint8
{
	COOK_MODE_COOK = 1, COOK_MODE_TYPES, COOK_MODE_ASSIGN_GUIDS, COOK_MODE_FIX_DUPLICATE_GUIDS, COOK_MODE_MIGRATE,
	COOK_MODE_PROVENANCE, COOK_MODE_VALIDATE, COOK_MODE_SERVE, COOK_MODE_WORKER,
};

/// @contract value Zenith_CookSetting
/// Ownership: A view element of Zenith_CookCommandLine::m_axSettings; the views point into argv.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the command line.
/// Units: None.
/// Errors: None; the parser has validated the key.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_CookSetting { const Zenith_StringView m_strKey = {}; const Zenith_StringView m_strValue = {}; };

/// @contract value Zenith_CookCommandLine
/// Ownership: Plain value produced by Zenith_ParseCookCommandLine; its string views point into the process's argv, which outlives it.
/// Thread affinity: IMMUTABLE. Members non-const: Zenith_ParseCookCommandLine fills it flag by flag.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ParseCookCommandLine returns COOK_ERROR_BAD_COMMAND_LINE naming the argument (exit code 2) for an unknown option, a
///   missing value, an option not valid in the selected mode, zero or two modes, a platform other than windows, android and
///   linux (linux only for fixture games), or a --set key that is not a cook key.
/// Version: since Z2 Stage 2. Tool-side; the options of "Options".
/// @endcontract
struct Zenith_CookCommandLine
{
	Zenith_CookMode m_eMode = COOK_MODE_COOK;
	Zenith_StringView m_strGame;                     // --game
	Zenith_AssetPlatform m_ePlatform = ASSET_PLATFORM_WINDOWS;   // --platform; meaningful in cook, provenance, validate, serve
	bool m_bPlatformGiven = false;
	Zenith_StringView m_strOutDirectory;             // --out
	Zenith_StringView m_strStep;                     // --step (types mode: "luau-types")
	Zenith_StringView m_strProvenanceQuery;          // --provenance <guid | logical path | cooked path>
	Zenith_StringView m_strWorkerPipe;               // --worker <pipe name>
	bool m_bClean = false, m_bVerify = false, m_bInProcess = false, m_bMigratePreview = false;
	uint32 m_uWorkers = 0;                          // --workers; 0: cook.workers
	Zenith_Span<const Zenith_CookSetting> m_axSettings;   // --set key=value, in argument order
};
Zenith_Result<Zenith_CookCommandLine, Zenith_CookError> Zenith_ParseCookCommandLine(sint32 iArgc, const char* const* aszArgv,
	Zenith_Arena& xArena);
```

`Zenith_CookServices` (`Internal/Zenith_CookServices.h`) is the plain aggregate of the objects the numbered list above creates, each as a `Zenith_Owned` member in creation order, with a destructor that destroys them in reverse; its `Create(const Zenith_CookCommandLine&)` returns `Zenith_Result<Zenith_Owned<Zenith_CookServices>, Zenith_CookError>` and fails at the first failing step with that step's error, having unwound the earlier ones. `Zenith_LineStream` is the tool's LF-framed byte stream over a pipe, a file or an in-memory pair (`Internal/Server/Zenith_LineStream.h`: `ReadLine(Zenith_Array<char>&, uint32 uMaxBytes) → Zenith_Result<bool, Zenith_CookError>` (false at EOF; an over-long line is `COOK_ERROR_PROTOCOL` after discarding through its LF) and `WriteLine(Zenith_StringView) → Zenith_Result<void, Zenith_CookError>`); `Zenith_CookWorkItem` is the `workItem` object of Formats.md "Worker messages" as a struct of the same fields.

### Step registration functions

Every area that owns cook steps declares one registration function in a header of its own `Internal/<Area>/` directory of ZenithCook, with the one signature `Zenith_Result<void, Zenith_CookError> Zenith_Register<Area>CookSteps(Zenith_CookStepRegistry& xRegistry, Zenith_CookServices& xServices)`. SCENE's is `Zenith_RegisterSceneCookSteps(Zenith_CookStepRegistry&, Zenith_CookServices&)` (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Scene and prefab cook steps"`), confirmed as declared there (cross-area row S24).

1. `Zenith_CookServices::Create` calls each function once, on the main thread, at item 6 of "Cook services and composition", in the order that item lists. SCENE's is the seventh call, after RPROD's; CONTENT's is the eighth; then the modules' (CINE's `Zenith_RegisterCinematicCookSteps` in `Zenith2/Tools/ZenithCook/Internal/Cinematics/`, whose validators call CINE's public `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile`; it stays in ZenithCook under the ZenithAuthoring rule that no cook-step registration moves out, cross-area row CN1 (d)).
2. When a function runs, items 1 to 5 exist and the type, importer and migration registries are frozen. Item 7 (the DDC and the database) does not exist yet, so a registration function reads nothing of it.
3. A function calls `Register`, `RegisterValidator` and `RegisterRule` on `xRegistry`, and may store `&xServices` in its static step instances: the services outlive every `Zenith_Cook::Run` and every server request.
4. After registration a step reads only the services' members that are `ANY_THREAD`: the root heap, the frozen type, importer, migration and step registries, and the codec set. The services' document storages and migrators (item 5), the database and the DDC are `OWNER_THREAD` or driver-owned, so a step reaches them only through its `Zenith_CookContext` (`ReadIntermediate`, `ReadDependencyOutput`, `OpenSourceDocument`).
5. A function returns the registry's `COOK_ERROR_INVALID_STEP` unchanged after the first refused call, and `Create` then fails with that error, having unwound items 1 to 5.

## Cook steps

### Zenith_CookStepDesc

```cpp
// Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookStep.h
/// @contract enum Zenith_CookPhase
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; registration refuses a step whose phase is not one of the enumerators (COOK_ERROR_INVALID_STEP).
/// Version: since Z2 Stage 2. Tool-side; the phase order is "Step phases".
/// @endcontract
enum Zenith_CookPhase : uint8
{
	COOK_PHASE_GLOBAL_PRE = 1,   // schemas (ECS), configuration merge (PLAT), script modules (SCRIPT)
	COOK_PHASE_ASSET,            // one item per (asset, platform): meshes, textures, materials, scenes, …
	COOK_PHASE_GLOBAL_POST,      // permutations (GEOM, RPROD), shader cook mode, PSO sets, dictionaries (PACK), catalogue, manifest
};

/// @contract value Zenith_CookStepDesc
/// Ownership: constexpr data of the step.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration refuses a duplicate ID, two ASSET-phase steps for one kind, a GLOBAL step whose outputs overlap
///   another's (COOK_ERROR_INVALID_STEP).
/// Version: since Z2 Stage 2. m_uVersion is a cook-key input: bumping it re-cooks every item of the step.
/// @endcontract
struct Zenith_CookStepDesc
{
	const char* const m_szID = nullptr;              // "geom.mesh", "geom.texture", "scene.scene", "ecs.schemas"
	const uint32 m_uVersion = 0;
	const Zenith_CookPhase m_ePhase = {};
	const Zenith_AssetKind m_eKind = {};             // COOK_PHASE_ASSET: the kind it cooks; ASSET_KIND_NONE otherwise
	const Zenith_Span<const char* const> m_aszRunsAfter = {}; // global steps that must finish first (IDs)
	const Zenith_Span<const char* const> m_aszToolVersions = {}; // manifest names whose pinned versions are key inputs
	const Zenith_Span<const char* const> m_aszOutputPrefixes = {}; // global steps: cooked-tree path prefixes they own ("Shaders/")
	const Zenith_Span<const Zenith_CookEngineFallback> m_axEngineFallbacks = {}; // COOK_PHASE_ASSET: "Engine fallbacks"
	const bool m_bOutOfProcess = false;
	const uint8 m_uPlatformMask = 0;                // bit per Zenith_AssetPlatform
};

/// @contract value Zenith_CookEngineFallback
/// Ownership: constexpr data of an ASSET-phase step.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration refuses two rows of one (kind, role) across steps, a role-0 row on a kind whose fallback policy is not
///   SUBSTITUTE, and a SUBSTITUTE kind's step without a role-0 row (COOK_ERROR_INVALID_STEP).
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_CookEngineFallback
{
	const char* const m_szLogicalPath = nullptr;     // "content:/Engine/Textures/Checker.png": engine content of the step's library
	const uint16 m_uRole = 0;                       // 0: the kind's SUBSTITUTE fallback; 1 … 65,535: the owner's further roles
};
```

### Zenith_CookStep

```cpp
/// @contract interface Zenith_CookStep
/// Ownership: Implemented by the area that owns the output; one static instance per step, registered by the cook services.
/// Thread affinity: ANY_THREAD. Items of one step run concurrently, in process or in worker processes.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Cook returns Zenith_Result<void, Zenith_CookError>; malformed intermediates or documents are errors naming the asset,
///   never asserts; a step that exceeds a content rule returns COOK_ERROR_RULE (Budgets BR-COOK).
/// Reentrancy: NOT_APPLICABLE. It calls only its context.
/// Version: since Z2 Stage 2. Source API of the tool.
/// @endcontract
class Zenith_CookStep
{
public:
	virtual ~Zenith_CookStep() = default;
	virtual const Zenith_CookStepDesc& GetDesc() const = 0;
	// Add every input that affects the output beyond the ones the driver adds (DDC.md "Key inputs"), in a fixed order.
	virtual void AddKeyInputs(Zenith_CookContext& xContext, Zenith_DerivedDataKeyBuilder& xKey) const = 0;
	// A pure function of the key inputs: read intermediates and upstream outputs through the context, write outputs through it.
	virtual Zenith_Result<void, Zenith_CookError> Cook(Zenith_CookContext& xContext) = 0;
};
```

### Zenith_CookContext

```cpp
class Zenith_OpenedDocument;   // ZenithDocument/Zenith_DocumentMigrator.h (SCENE)

/// @contract type Zenith_CookContext
/// Ownership: Owned by the driver for one item; borrowed by the step for AddKeyInputs and Cook. The objects a call creates
///   for OpenSourceDocument belong to that call and are destroyed before it returns ("Source documents in cook steps").
/// Thread affinity: OWNER_THREAD per call: the thread running the item's AddKeyInputs, or its Cook. The two calls may run
///   on different threads; the driver hands the context to the second only after the first has returned.
/// Invalidation: Spans it returns are valid until Cook returns.
/// Units: Bytes.
/// Errors: Zenith_Result<…, Zenith_CookError>. OpenSourceDocument: COOK_ERROR_MISSING_REFERENCE, COOK_ERROR_SOURCE_CHANGED,
///   COOK_ERROR_STEP_FAILED with the Zenith_DocumentErrorCode as detail, COOK_ERROR_OUT_OF_MEMORY, COOK_ERROR_INVALID_STEP
///   (the table of "Source documents in cook steps").
/// Version: since Z2 Stage 2 (OpenSourceDocument too: SCENE's scene.scene step needs it at Stage 2).
/// @endcontract
class Zenith_CookContext
{
public:
	Zenith_AssetPlatform GetPlatform() const;
	const Zenith_AssetRecord* GetAsset() const;                         // null for global steps
	// The plan's record snapshot looked up by logical path ("content:/…", ASSET.md "Content root mapping"), the snapshot's view of
	// Zenith_AssetDatabase::FindByLogicalPath; null when absent or in state TOMBSTONE or MISSING_SOURCE. Records nothing: a step
	// that uses the asset it found declares it itself (DeclareReference, or ReadDependencyOutput for a cook input).
	const Zenith_AssetRecord* FindAssetByLogicalPath(Zenith_StringView strLogicalPath) const;
	const Zenith_ResolvedSettings& GetSettings() const;
	Zenith_Result<Zenith_ByteSpan, Zenith_CookError> ReadIntermediate(Zenith_StringView strBlobName);
	// Another asset's cooked output or published property: recorded as a cook-input edge; its key hash joins this key.
	Zenith_Result<Zenith_ByteSpan, Zenith_CookError> ReadDependencyOutput(const Zenith_AssetGUID& xAsset,
		Zenith_StringView strPropertyName);
	// The authoring document of an asset of the plan's record snapshot, read through a call-local reader, verified against
	// the scan's source hash and migrated in memory ("Source documents in cook steps").
	Zenith_Result<void, Zenith_CookError> OpenSourceDocument(const Zenith_AssetGUID& xAsset,
		Zenith_OpenedDocument& xOut);
	void PublishProperty(Zenith_StringView strName, Zenith_ByteSpan xBytes);          // read by dependents' steps
	void DeclareReference(const Zenith_AssetGUID& xTarget, uint8 uFlags);            // ASSET_DEPENDENCY_FLAG_*
	Zenith_CookedFileWriterDesc MakeWriterDesc(uint32 uMagic, uint16 uFormatVersion) const;
	Zenith_Result<void, Zenith_CookError> WriteAssetFile(Zenith_ByteSpan xFileBytes, uint64 ulResidentEstimateBytes);
	Zenith_Result<void, Zenith_CookError> WriteFile(Zenith_StringView strTreeRelativePath, Zenith_ByteSpan xBytes);  // global steps
	void AddReportMember(Zenith_StringView strMember, Zenith_ByteSpan xJsonValue);    // GEOM's vertexFallbacks, …
	void Report(Zenith_CookDiagnosticLevel eLevel, Zenith_StringView strMessage);   // at most 256 per item; further ones counted
	const Zenith_CompressionCodecSet& GetCodecs() const;
	Zenith_Arena& GetScratch();
	Zenith_StringView GetHostToolPath(Zenith_StringView strTool) const;  // "ZenithShaderCompiler"
	// Global steps only ("Repository and third-party files"): every live record of the plan with its .meta provenance text.
	Zenith_Span<const Zenith_CookPlanOrigin> GetPlanOrigins() const;
	// A tracked repository file (Zenith2-relative), GLOBAL_POST steps only; keyed by path and hash in AddKeyInputs.
	Zenith_Result<Zenith_ByteSpan, Zenith_CookError> ReadRepositoryFile(Zenith_StringView strZenith2RelativePath,
		uint64 ulMaxBytes);
	// A file of the thirdparty: input root (ASSET.md "Content root mapping"), any phase; keyed by path and hash in AddKeyInputs.
	Zenith_Result<Zenith_ByteSpan, Zenith_CookError> ReadThirdPartyFile(Zenith_StringView strThirdPartyPath,
		uint64 ulMaxBytes);
};
struct Zenith_CookPlanOrigin
{
	Zenith_AssetGUID m_xAsset;
	Zenith_AssetKind m_eKind;
	Zenith_StringView m_strLogicalPath;
	Zenith_StringView m_strOriginJson;          // the .meta's provenance object in canonical compact text; empty when absent
};
```

`FindAssetByLogicalPath` answers PHYS's name lookups (cross-area row PH26: a heightfield or compound step resolving physical-material names, the ragdoll step resolving its document's `skeleton` path): it is O(log n) over the snapshot's logical-path index, built at plan step 6 in byte order of logical path with the database's own index rules (one record per logical path, sub-asset paths `L#<key>` included); a path that is not a valid logical path (empty, not beginning `content:/`, longer than 1,024 bytes) returns null; a worker-process context, which has no snapshot, returns null for every path, so a step that calls it declares `m_bOutOfProcess` false. The lookup itself is not a key input: what keys the step is the reference or the dependency output it then declares, plus the path text the step adds as its own row in `AddKeyInputs` when the result decides its output (a path that resolves to nothing today and to an asset tomorrow must change the key, so a step whose output depends on the lookup adds the found GUID, or the zero GUID when null, as a step-specific row).

A step writes an asset through `MakeWriterDesc` (magic, format version, the kind, the platform, the GUID and the item's key hash as the content hash filled in) and `WriteAssetFile`; the driver checks the written header against the desc before accepting it.

Other areas' questions about the context, answered here (cross-area row AU9): a `COOK_PHASE_GLOBAL_POST` step may call `ReadDependencyOutput` for any asset of the plan, because every ASSET-phase item has ended before the first GLOBAL_POST step runs (AUDIO's `audio.eventIndex` reading each bank's `audio.eventIDs` property); the plan's record snapshot holds every live record, including a document source whose kind has no step and which therefore cooks to nothing, so `OpenSourceDocument` reaches it (AUDIO's mixer document); an area's validators and content rules are registered by its own registration function through `RegisterValidator` and `RegisterRule` (AUDIO's `residentMegabytes` and `durationSeconds` for kinds 10 and 11).

### Repository and third-party files

Three members let a step read inputs that are not intermediates, dependency outputs or source documents (cross-area rows CT6, CT24 (d), UI9):

| Member | Phases | Answer | Errors |
|---|---|---|---|
| `GetPlanOrigins()` | GLOBAL_PRE and GLOBAL_POST steps; an ASSET-phase call returns an empty span | one `Zenith_CookPlanOrigin` per live record of the plan's snapshot (sources and sub-assets, not TOMBSTONE or MISSING_SOURCE), ascending GUID byte order; `m_strOriginJson` is the source `.meta`'s `provenance` object written by `Zenith_JsonWriter` in compact style with keys in byte order (empty when the `.meta` has none; a sub-asset carries its source's), views valid until `Cook` returns | none |
| `ReadRepositoryFile(strPath, ulMaxBytes)` | GLOBAL_POST steps, in `AddKeyInputs` and `Cook` | the whole file `Zenith2/<strPath>` (CONTENT's `ThirdParty/licence_policy.json`) | `COOK_ERROR_INVALID_STEP` for another phase or a path that is absolute, has `\`, `.` or `..` components, or begins `out/`; `COOK_ERROR_MISSING_REFERENCE` when absent; `COOK_ERROR_IO` for a read failure; `COOK_ERROR_RULE` above `ulMaxBytes` (at most 64 MiB) |
| `ReadThirdPartyFile(strPath, ulMaxBytes)` | every phase, in `AddKeyInputs` and `Cook` | the whole file `thirdparty:<entry>/<p>` resolved to `ZENITH_TP_<ID>_DIR/<p>` (UI's font step reading `thirdparty:noto-fonts/…`) | `COOK_ERROR_INVALID_STEP` for a path not of that form or an entry ZenithCook does not name; the other errors as `ReadRepositoryFile` |

- **Keying.** A read in `AddKeyInputs` appends two step-specific rows after the driver's rows: `inputFile` (the path text exactly as passed) and `inputFileHash` (`Zenith_Hash128Of` of the bytes), in reading order; a path read again in the same call adds nothing. In `Cook`, a path `AddKeyInputs` did not read is `COOK_ERROR_INVALID_STEP` ("<step> read <path> in Cook without keying it"), and bytes whose hash differs from the keyed one are `COOK_ERROR_SOURCE_CHANGED`. The host directory of a `thirdparty:` entry never reaches a key, so two machines with different dependency caches share DDC entries.
- **Depfile.** Every file read through either member is listed in `cook.d` ("Dependency file"), so editing `licence_policy.json` or repinning a data entry reruns the cook.
- **Origins and keys.** `GetPlanOrigins` is not keyed by itself: a step that uses it adds, in `AddKeyInputs`, the Hash128 of its own canonical serialization of the origins it reads (CONTENT's `content.licences` step does), because a `.meta` provenance change is not an import-key input.
- **Resolution table.** ZenithCook's `CMakeLists.txt` names every `data` entry its steps read in `THIRD_PARTY`, and the configure step passes the `name=directory` pairs as one compile definition `ZENITH_COOK_THIRDPARTY_ROOTS` (`;`-separated, from each entry's `ZENITH_TP_<ID>_DIR`), parsed once by the services at item 1; `ReadInput` of an importer accepts the same `thirdparty:` paths (Import.md "Zenith_ImportContext").

`Test_CookContext_RepositoryAndThirdPartyFiles` (Tests.md "Cook tests") covers the three members.

### Source documents in cook steps

A step whose output is a resolved authoring document reads that document and every document its resolution reaches, migrated in memory to the current version, rather than an intermediate: SCENE's `scene.scene` and `scene.prefab` (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Scene and prefab cook steps"`, cross-area row S24). `OpenSourceDocument` is the only way a step reads a source file (Import.md "Intermediates"), and it returns only a document whose bytes equal the bytes the plan's scan hashed.

Items run on worker threads, while the services' read-only migrators (item 5 of "Cook services and composition") and the database are `OWNER_THREAD` on the tool's main thread, which also schedules items. ZenithCook never makes an item wait for the main thread: a forwarded call would serialise every document read behind the driver and make an item's progress depend on the scheduler it runs under. Each call reads instead through objects it owns for its own duration:

| Object | Created | Owner and lifetime |
|---|---|---|
| the reader-set pool: R = the run's in-process worker count + 1 sets (at most one call per in-process item thread, plus one on the driver's thread, run at once), each holding one read-only `Zenith_HostDocumentStorage` per content storage root (`m_bReadOnly` true, the root heap) | by `Zenith_Cook::Run` on its own thread (the storage factory is `MAIN_THREAD`), after plan step 1 and before plan step 5 | the run; destroyed when `Run` returns, every set having been returned |
| the free-set queue: a `Zenith_BoundedQueue<uint32>` of free set indices, hard capacity the smallest power of two at or above R, full policy `QUEUE_FULL_POLICY_REJECT` | with the pool | the run |
| a call's set | `TryPop` at the call's first read; a failed pop is a driver defect (`ZENITH_CHECK`) | the call; pushed back when the call returns |
| a call's document store per root it reads from: a `Zenith_MemoryDocumentStorage` (the root heap) and a `Zenith_DocumentMigrator` over it, whose desc is the services' frozen migration registry, that memory storage, the frozen type registry, `m_pxStrings` null, `m_pxScriptHost` null (a `script` migration op fails, as in every cook open), the root heap and the batch timestamp of item 5's migrators | lazily, on the calling thread, which becomes the migrator's owner; then `RecoverPendingJournals` once, which succeeds with zero counts because the memory storage holds no journal | the call; destroyed on the same thread before the call returns |

The storage set is used by one call at a time, which is the external synchronisation `Zenith_DocumentStorage` requires; the frozen registries are `ANY_THREAD`; nothing else is shared between calls, so no lock is taken.

`OpenSourceDocument(xAsset, xOut)`:

1. `xOut.Reset()`. A context in a worker process (`ZenithCook --worker`) carries no record snapshot: the call is `COOK_ERROR_INVALID_STEP` ("<step> opens source documents and cannot run out of process"); a step that calls it declares `m_bOutOfProcess` false.
2. Find `xAsset` in the plan's record snapshot (Plan construction step 6). Absent, or its source record (itself for a source; the record of its `m_xSourceGUID` for a sub-asset) in state TOMBSTONE or MISSING_SOURCE: `COOK_ERROR_MISSING_REFERENCE`.
3. The root is the content storage root whose Zenith2-relative directory (`Games/<Name>/`, or `Engine/<Lib>/Internal/Content/`) begins the source record's `m_strSourcePath`; the rest of that path is the storage path. The scan walks only those roots, so a record outside every root is a driver defect (`ZENITH_CHECK`).
4. A document this call has already read is not read again: continue at step 7 over the bytes already in the call's memory storage.
5. Take the call's set when it has none, and `ReadWhole(storage path, 64 MiB)` (the limit of `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Opening a document"` step 1) from the set's storage for the root. `DOCUMENT_ERROR_NOT_FOUND` means the source vanished after the scan: `COOK_ERROR_SOURCE_CHANGED` with that code as detail. Any other storage error is `COOK_ERROR_STEP_FAILED` with the `Zenith_DocumentErrorCode` as detail.
6. `Zenith_Hash128Of(bytes)` must equal the source record's `m_xSourceHash`, the hash the plan's scan recorded; otherwise `COOK_ERROR_SOURCE_CHANGED`, reported as "<source path> changed after the cook's scan; build again". Then `WriteFlushed(storage path, bytes)` into the call's memory storage for the root, created with its migrator first.
7. `Open(storage path, xOut)` on the call's migrator for the root. An error is `COOK_ERROR_STEP_FAILED` with the `Zenith_DocumentErrorCode` as detail (`COOK_ERROR_OUT_OF_MEMORY` for `DOCUMENT_ERROR_OUT_OF_MEMORY`), the framework's error record logged with the source path ([Z2-N-CONTRACT-463]). Success is returned in every open state, `DOCUMENT_OPEN_READ_ONLY_NEWER` included: what a newer document means is the step's decision (SCENE's is a cook error). `xOut.GetFileDigest()` is the SHA-256 of exactly the verified bytes.
8. In `AddKeyInputs`, the first successful open of a document appends (`xAsset`, `GetFileDigest()`) to the item's opened list K, which the context keeps for the item's `Cook`. In `Cook`, an `xAsset` that K does not hold, or holds with another digest, is `COOK_ERROR_INVALID_STEP` ("<step> opened <source path> in Cook without keying it") and `xOut` is reset.

Every error carries `xAsset` as `m_xAsset`:

| Condition | Error | `m_uDetail` |
|---|---|---|
| called in a worker process; or, in `Cook`, a document `AddKeyInputs` did not open | `COOK_ERROR_INVALID_STEP` | 0 |
| no live record of that GUID in the snapshot | `COOK_ERROR_MISSING_REFERENCE` | 0 |
| the source vanished, or its bytes differ from the scan's | `COOK_ERROR_SOURCE_CHANGED` | `DOCUMENT_ERROR_NOT_FOUND`, or 0 |
| a storage or framework error | `COOK_ERROR_STEP_FAILED` | the `Zenith_DocumentErrorCode` |
| out of memory in the memory storage or the migrator | `COOK_ERROR_OUT_OF_MEMORY` | `DOCUMENT_ERROR_OUT_OF_MEMORY` |

**Key contribution.** The SHA-256 of every document a step opens enters that step's key, in reading order: right after each first successful open, `AddKeyInputs` adds the document's GUID and the 64-character lowercase hex `Zenith_FormatSha256Hex(GetFileDigest())`, under row names the step chooses (SCENE's are `scene.document` and `scene.document.sha256`), after the driver's rows (DDC.md "Key inputs", row "step-specific inputs"); a document whose open fails adds the step's own failure rows instead. The driver adds no row of its own for a source document. Step 8 means a `Cook` reads nothing its key did not cover, and step 6 means the bytes keyed in `AddKeyInputs` and the bytes read in `Cook` are identical even when the two calls run on different threads, so the key stays a function of the output's inputs. A document another item reads (a prefab its scenes instance) therefore needs no declared cook-input edge: editing it changes its digest and so the key of every item that opened it.

**Writers during a cook.** A build cook takes no storage-root lock ("Output tree and database locks" item 6), so an editor save or a sidecar writer may commit a batch at a root while items run. Each document an item opens either equals its scanned bytes or fails the item with `COOK_ERROR_SOURCE_CHANGED` (exit code 1); an item never sees a mix of the scan's documents and a newer batch's, and never reads a framework temporary. The depfile lists the changed source, so the next build re-scans and re-cooks it.

`Test_CookContext_OpenSourceDocument` covers the member (Tests.md "Cook tests").

### Cook diagnostics

```cpp
/// @contract enum Zenith_CookDiagnosticLevel
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an ERROR diagnostic fails its item (exit code 1), WARNING and INFO never change the exit code.
/// Version: since Z2 Stage 2. Tool-side; serialized as "error", "warning", "info" in the report and the server's notifications.
/// @endcontract
enum Zenith_CookDiagnosticLevel : uint8
{
	COOK_DIAGNOSTIC_LEVEL_INFO = 1,
	COOK_DIAGNOSTIC_LEVEL_WARNING,
	COOK_DIAGNOSTIC_LEVEL_ERROR,
};

/// @contract value Zenith_CookDiagnostic
/// Ownership: A record in the cook's diagnostic arena; strings point into it; valid until the next Run or Scan.
/// Thread affinity: IMMUTABLE once recorded. Members non-const: stored in a Zenith_Array that the cook sorts in place
///   by severity, code and path.
/// Invalidation: The next Run.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The report's "diagnostics" element serializes it (Formats.md "Cook report").
/// @endcontract
struct Zenith_CookDiagnostic
{
	Zenith_CookDiagnosticLevel m_eLevel;
	uint16 m_uCode;                                 // a Zenith_CookErrorCode, a COOK_WARNING_* value (1000 + its index in "Cook errors"),
	                                                 // uCOOK_DIAGNOSTIC_CODE_IMPORT or uCOOK_DIAGNOSTIC_CODE_NAMED
	Zenith_AssetGUID m_xAsset;                       // null for global diagnostics
	Zenith_StringView m_strPath;                     // a Zenith2-relative source path or a tree-relative output path; may be empty
	Zenith_StringView m_strMessage;
	Zenith_StringView m_strCode;                     // the importer's code name when m_uCode is uCOOK_DIAGNOSTIC_CODE_NAMED; empty otherwise
	bool m_bReplayed = false;                        // an import diagnostic replayed from storage (Import.md "Replay into the cook report")
};
inline constexpr uint16 uCOOK_DIAGNOSTIC_CODE_IMPORT = 2000;   // an import diagnostic reported without a code: "COOK_IMPORT_DIAGNOSTIC"
inline constexpr uint16 uCOOK_DIAGNOSTIC_CODE_NAMED = 2001;    // an import diagnostic reported with a code: the report writes m_strCode
```

The report's `code` string of a diagnostic is `Zenith_ToString` of its `Zenith_CookErrorCode`, the `COOK_WARNING_<NAME>` spelling of a warning, `"COOK_IMPORT_DIAGNOSTIC"` for `uCOOK_DIAGNOSTIC_CODE_IMPORT`, and `m_strCode` verbatim for `uCOOK_DIAGNOSTIC_CODE_NAMED`; an import diagnostic's `m_xAsset` is its source's GUID and `m_strPath` its source's Zenith2-relative path. Import diagnostics, their codes, their storage and their replay are Import.md "Import diagnostics".

### Zenith_CookStepRegistry

```cpp
// Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookStepRegistry.h
// Parses a cooked file's sections as the runtime loader would; returns the failing reason in xMessage.
using Zenith_PfnCookValidator = Zenith_Result<void, Zenith_CookError> (*)(const Zenith_CookedFileView& xFile,
	Zenith_ByteSpan xFileBytes, Zenith_Arena& xScratch, Zenith_Array<char>& xMessage);
// Measures one quantity of a cooked item for the content rules; returns the value in the table's unit.
using Zenith_PfnMeasureContent = Zenith_Result<double, Zenith_CookError> (*)(const Zenith_CookContext& xContext,
	const Zenith_CookedFileView& xFile, Zenith_ByteSpan xFileBytes);

/// @contract value Zenith_ContentRule
/// Ownership: constexpr data of the step that registers it.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: The quantity's unit, as the content-rule table's "unit" of its row.
/// Errors: RegisterRule refuses a duplicate (kind, quantity) (COOK_ERROR_INVALID_STEP).
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentRule
{
	const Zenith_AssetKind m_eKind = {};
	const char* const m_szQuantity = nullptr;        // "triangles": the table's "quantity"
	const Zenith_PfnMeasureContent m_pfnMeasure = nullptr;
};

/// @contract type Zenith_CookStepRegistry
/// Ownership: Created by Create as a Zenith_Owned; owned by the cook services; borrows every step (static instances).
/// Thread affinity: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards.
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns COOK_ERROR_INVALID_STEP for a duplicate ID, two ASSET-phase steps for one kind, overlapping global
///   output prefixes, an unknown m_aszRunsAfter ID (checked at Freeze), a runs-after cycle (Freeze), or an engine-fallback row
///   refused by Zenith_CookEngineFallback's rules; any call after Freeze is COOK_ERROR_INVALID_STEP.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
class Zenith_CookStepRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_CookStepRegistry>, Zenith_CookError> Create(Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_CookError> Register(Zenith_CookStep& xStep);
	Zenith_Result<void, Zenith_CookError> RegisterValidator(Zenith_AssetKind eKind, Zenith_PfnCookValidator pfnValidator);
	Zenith_Result<void, Zenith_CookError> RegisterRule(const Zenith_ContentRule& xRule);
	Zenith_Result<void, Zenith_CookError> Freeze();
	Zenith_CookStep* FindByID(Zenith_StringView strID) const;
	Zenith_CookStep* FindAssetStep(Zenith_AssetKind eKind) const;
	Zenith_PfnCookValidator FindValidator(Zenith_AssetKind eKind) const;
	Zenith_Span<const Zenith_ContentRule> GetRules() const;                  // ascending (kind, quantity)
	Zenith_Span<Zenith_CookStep* const> GetGlobalSteps(Zenith_CookPhase ePhase) const;   // run order
	Zenith_Span<const Zenith_CookEngineFallback* const> GetEngineFallbacks(Zenith_AssetKind eKind) const;
};
```

### Step phases

1. `COOK_PHASE_GLOBAL_PRE` steps run first in `m_aszRunsAfter` topological order (ties by ID byte order): `ecs.schemas` (validates every schema, writes `schemas.zreg`), `plat.config` ("Configuration merge"), `plat.bindings`, `script.modules` (`.zlbc` and the manifest; needs the schemas).
2. `COOK_PHASE_ASSET` items (one per asset record with a kind, per platform) run in the dependency order of "Deterministic ordering".
3. `COOK_PHASE_GLOBAL_POST` steps run last in `m_aszRunsAfter` order: GEOM's permutations, RPROD's shader cook mode and PSO precache set, PACK's dictionary training (when packaging), then ASSET's `asset.catalogue` and `asset.manifest`, which always run last.

### Engine-provided steps

| Step | Owner | Output |
|---|---|---|
| `asset.data` | ASSET | `ASSET_KIND_DATA` files from `raw` intermediates (one stored or compressed section kind 1) |
| `asset.catalogue` | ASSET | `assets.zcat` and fragments ("Catalogue generation") |
| `asset.manifest` | ASSET | `mount-manifest.json` ("Mount manifest") |
| `plat.config`, `plat.bindings` | PLAT (ASSET runs them) | "Configuration merge" |
| `ecs.schemas` | ECS | `schemas.zreg` |
| `script.modules`, `script.luau-types` | SCRIPT | `Scripts/…`; `.d.luau` |
| `geom.mesh`, `geom.material`, `geom.texture`, `geom.permutations` | GEOM | `Zenith2/Docs/Design/GEOM.md → "Cook steps and derived-data keys"` |
| `rprod.shaders`, `rprod.pso` | RPROD | `Shaders/…`, `pso_precache.zbin` |
| every other kind's step | its owner | its kind |

## Planning

### Inputs of a cook

`Games/<Name>/game.json` (for `content.roots` and `content.startupScene`), `${ZENITH_OUT}/Games/<Name>/composition.json` (the selected modules and backends, `Zenith2/Docs/Design/MOD.md → "The composition.json record"`), the content roots, the engine content roots of the linked libraries, every library's configuration fragments, the database, the DDC.

### Content selection

- The linked libraries are every engine library of `Zenith2/Docs/Architecture.md → "Library dependency graph"` up to `ZenithRuntime` plus the modules listed in `composition.json` `modules`; their `Internal/Content/` and `Internal/Config/` trees are scanned, and no other library's.
- Every asset reachable from the game's content roots is cooked, not only those the startup scene references (a game may load by path or GUID at run time); engine content is cooked only when referenced by game content, declared as an engine fallback by an ASSET-phase step ("Engine fallbacks"), or read by a global step.

### Engine fallbacks

- Each ASSET-phase step declares its kind's engine fallbacks in `Zenith_CookStepDesc::m_axEngineFallbacks` by logical path and role; a SUBSTITUTE kind's step declares exactly one role-0 row (GEOM's texture step also declares one row per usage, role `usage + 1`, for its per-usage fallbacks).
- For every game and every platform in the step's mask, the planner resolves each row's logical path through the database: absent, tombstoned, or of another kind is `COOK_ERROR_FALLBACK_MISSING` naming the step, the role and the path (exit code 3); otherwise the asset joins the plan as if game content referenced it.
- `asset.catalogue` writes one fallback-table row per declared (kind, role) into the base catalogue (Formats.md "Fallback table"), sorted by (kind, role); the runtime reads fallbacks only from that table ("Zenith_AssetRegistry" in RuntimeAPI.md), so no fallback GUID is compiled into any library and the cook knows every fallback it must include.
- `Test_Cook_EngineFallbacksInEveryCatalogue` cooks a fixture game that references no engine content and asserts every declared fallback is cooked and listed; `Test_Cook_FallbackMissingFails` removes one fallback source and asserts exit code 3.

### Content root mapping rules

The mapping of `Zenith2/Docs/Design/ASSET.md → "Content root mapping"`, with the checks of Database.md "Scan algorithm"; the startup scene's logical path (`content:/` + the mapped path of `content.startupScene`) must exist in the catalogue, else `COOK_ERROR_STARTUP_SCENE_MISSING`.

### Plan construction

1. Scan the database (Database.md "Scan and reconcile"); stop with exit 3 on duplicates, collisions or missing `.meta`.
2. Import every NEW or DIRTY source (Import.md "Import algorithm"), resolving the published-property inputs those imports read in the waves of Import.md "Published-property inputs".
3. Build the item list: global-pre steps, one ASSET item per (asset record with a kind, platform), global-post steps.
4. Order ASSET items topologically over hard references and cook inputs (Database.md "Queries"), ties broken by GUID byte order.
5. Compute each item's key in that order ("Keys and invalidation"), so an item's key includes its upstream keys.
6. Take an immutable snapshot of the records and settings the items read; workers read only the snapshot.

## Incremental cook

### Keys and invalidation

- Every item's DDC key covers everything its output depends on: the step ID and version, `uZENITH_ENGINE_COOK_VERSION`, the platform, the intermediate's import key (source bytes, declared inputs, importer version, settings), the resolved cook settings including compression codec, level, dictionary GUID and library version, the pinned versions of the step's declared tools, and the key hash of every cook-input dependency (DDC.md "Key inputs").
- An item whose key equals the key recorded for its current output in the cooked tree (its header's content hash, or for non-asset files the journal) and whose output file's hash matches the catalogue's recorded output hash is up to date: nothing runs.
- Otherwise the DDC is consulted; a verified hit is copied into the tree; a miss runs the step and populates the DDC.
- A key that is equal for different outputs cannot arise when steps are pure and declare every input; verification sampling (DDC.md "Verification sampling") detects a step that is not.

### Dependency-driven invalidation

Because a dependent's key contains its upstream keys, a change anywhere upstream changes every downstream key in the propagation of Database.md "Graph maintenance", which stops where a recomputed key is unchanged; a change that alters a dependency's key but not the property a dependent reads re-cooks the dependent once (its key changed) and its output is byte-identical, so nothing further downstream changes when outputs feed keys through `PublishProperty` hashes rather than full keys. Steps choose: `ReadDependencyOutput` of a published property keys on that property's hash; of the whole output keys on its key hash.

### Output reuse

- A clean cook (`--clean`) and an incremental cook of the same inputs publish byte-identical trees: `Test_Cook_CleanEqualsIncremental` cooks a fixture game clean, mutates one source, one setting, one importer version, one cook-input dependency and one engine-content file in turn, cooks incrementally after each, and compares the whole tree byte for byte with a clean cook of the mutated inputs; the Stage 3 acceptance runs it over every flagship slice's content.
- Before the DDC exists (Stage 2 and 3), incremental cooking uses the tree's own outputs as the only cache (the key comparison of "Keys and invalidation" without DDC lookups).

## Execution

### Deterministic ordering

- Items run concurrently on `cook.workers` threads or worker processes, but every published byte is independent of the worker count and of completion order: outputs are pure functions of their keys, the catalogue and report are built from item results sorted by (phase, step ID, GUID), and no path, timestamp or thread identity reaches an output (`Test_Cook_OutputIndependentOfWorkerCount` cooks with 1, 2 and 8 workers and compares trees).
- An item starts only after every item its key depends on has finished (a failed dependency fails it with `COOK_ERROR_DEPENDENCY_FAILED`).

### Worker processes

- Items of steps and importers with `m_bOutOfProcess` run in `ZenithCook --worker <pipe>` processes the driver starts (`cook.workers` of them, created lazily), over a named pipe (Windows) carrying the line-delimited JSON messages of Formats.md "Worker messages" (`workerHello`, `workItem`, `workResult`, `workShutdown`); inputs and outputs travel as DDC entries on disk, and a pipe line never carries payload bytes.
- A worker that exits, crashes or exceeds `cook.workerTimeoutSeconds` fails its item with `COOK_ERROR_WORKER_CRASHED` or `COOK_ERROR_WORKER_TIMEOUT` (its partial outputs are temporary files the driver deletes), is replaced, and the cook continues with the other items (the containment CONTENT's Z2-I-CONTENT-22 relies on).
- A worker is started as `ZenithCook --worker <pipe name> --game <Name> --platform <p>` followed by every `--set <key>=<value>` of the parent's command line in their original order, with `--set cook.ddc.localRoot=<the parent's root>` last, so importer- and step-owned cook keys (CONTENT's `cook.content.faultSource`, `cook.content.fbxConverterTimeoutSeconds`) reach out-of-process items exactly as in-process ones (cross-area row CT5).
- `--in-process` runs everything in the driver (debugging, and the tests that are not about isolation).

### Output tree and database locks

A cook server and a build cook (or two cooks) may be started over the same output tree — the editor's server keeps running while the developer builds — and a CLI sidecar mode, the server and the editor's own scene saves may write one source tree at once, so ZenithCook serialises every writer of a tree, of a database and of a document storage root through the lock files of `Zenith2/Docs/Design/ASSET/Formats.md → "Cook lock file"`:

1. A mode that writes `<out>` (cook, `--validate-only`, `--serve`) takes `<out>/.cook/lock` before it reads `<out>` or the journal. A cook or validate run that finds it held exits at once with code 6 and `COOK_ERROR_OUTPUT_LOCKED`, naming the holder's PID and mode from the lock's content ("the cook server of the editor holds this tree; cook through the editor or stop its server"); it reads, writes and deletes nothing in the tree, so the server's journal, temporaries and outputs are untouched. A server that finds it held answers `hello` with the same error and exits with code 6; the editor restarts it as it does after EOF (cross-area row AS30). The build cook never waits for the lock: a build that blocks behind an editor session would look hung.
2. The server holds the output lock from start to exit, so while it lives it is the tree's only writer, the owner of `.cook/journal.jsonl` and the only process that deletes temporaries in it; a cook run holds it from start to exit.
3. Every process that would `Save` the database — every cook, validate and sidecar-writing run, and the server — tries `${ZENITH_OUT}/AssetDB/<Name>/assetdb.lock` once at start and, having taken it, holds it until exit (the server: for its whole session); the database is per game, so the servers and cooks of both platforms contend for it. A process that finds it held does not wait and does not fail: it loads `assetdb.zdb` read-only (or rebuilds in memory when the file is absent or discarded), scans and runs on that in-memory copy, and never saves it, reporting `COOK_WARNING_DATABASE_LOCKED` with the holder's PID; the file is a cache (Database.md "Database file location and rebuild"), so this costs only rescan time. The tombstone lists are not part of `Save`: a sidecar-writing mode writes them with its `.meta` writes under the storage roots' writer locks of item 5 (`WriteTombstoneLists`, Database.md "Database file location and rebuild"), whether or not it holds the database lock, and the holder's next scan reads them.
4. Every temporary a cook writes carries its PID (`<final path>.<pid>.z2cook`, "Atomic output publication"), and a cook deletes temporaries only while holding the output lock, so it never deletes a temporary of a live process: any temporary it finds on taking the lock was left by a process that has exited.
5. Every writer of a document storage root — the game directory `Zenith2/Games/<Name>/` and each linked engine library's `Internal/Content/` (Migration.md "Reading and writing meta documents through the framework"); an engine root is shared by every game that links the library, so a per-game lock could not cover it — holds that root's writer lock while it recovers, reads to decide and writes. The lock is SCENE's `Zenith_DocumentWriterLock` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`), returned by the writable `Zenith_HostDocumentStorage`'s `Zenith_DocumentStorage::GetWriterLock()` and implemented by ZenithDocumentHost over ASSET's file `<storage root>/.migration/writer.lock` (Formats.md "Cook lock file" fixes its path, open mode and content line). ZenithCook never opens that file to write it; its only own access is the build cook's read-only probe of item 6.
	- A sidecar-writing mode creates its writable storages with `m_szWriterMode` `"assign"`, `"fix-duplicates"`, `"migrate"` or `"serve"`, `m_szGame` the game and `m_szPlatform` the server's platform (null for the CLI modes), and its migrators with `m_uWriterLockWaitMs` = `cook.sourceLockWaitMs`.
	- `--assign-guids`, `--fix-duplicate-guids` and `--migrate` (with or without `--preview`) call `GetWriterLock()->Acquire(cook.sourceLockWaitMs, holder)` for every writable root at start, in byte order of root path, before `RecoverPendingJournals`, and hold every lock until exit.
	- The cook server acquires, in the same order, the locks of the roots a sidecar-writing request touches (`assignGuids`, `setSettings`, `import`, `migrateApply`, and the import of a watch-driven run that writes a `.meta` or a tombstone line) when the request starts, recovers, re-opens every document it will write, writes its one batch and the tombstone lists, and calls `Release` on each when the request ends. It holds no root lock while idle, which leaves the editor's own scene and prefab saves (SCENE's migrator takes the lock per batch through the same `GetWriterLock()`) and a server of another platform or another game free to write between its requests.
	- The caller already holds the lock when it calls `RecoverPendingJournals`, `ApplyPlan` or `SaveDocuments`, so those members reuse its hold and release nothing (hold rule 2 of "Storage-root writer lock"). Writers take several roots only in byte order of root path, and the editor's migrator holds one root per batch, so no two writers wait on each other in a cycle.
	- `Acquire` polls every 50 ms. `DOCUMENT_ERROR_STORAGE_LOCKED` (held by another process for the whole `cook.sourceLockWaitMs`) makes the writer release the locks it took and refuse with `COOK_ERROR_SOURCE_LOCKED`: a CLI mode exits with code 6, a server request is answered with that error and the session continues. The message names the root and the holder's PID and mode from the holder line `Acquire` returns ("unknown holder" when the line does not parse), and reads "another tool is writing this content root; retry when it finishes", never a command that would write the root. `DOCUMENT_ERROR_IO` from `Acquire` is `COOK_ERROR_IO` naming the root (exit code 5 in a CLI mode).
	- Every framework journal is created and deleted inside its writer's hold (hold rule 3), so a journal that a lock holder finds was left by a writer that has exited and `RecoverPendingJournals` never rolls a live batch forward or back; and because the read-decide-write sequence runs inside the hold, two writers can never give one new source two GUIDs or overwrite each other's tombstone-list rewrite.
6. A build cook takes no root lock: it never writes a source, and its read-only storages return a null `GetWriterLock()`. When its read-only recovery check (Migration.md "Recovery before a cook") finds a journal at a root, it probes that root's writer lock with ZenithCook's own read-only probe `Zenith_ProbeStorageRootWriterLock(strRootHostPath, xHolderOut)` → `Zenith_Result<bool, Zenith_CookError>` (true: held; `Internal/Cook/Zenith_CookLocks.cpp`): the holder's open of Formats.md "Cook lock file" with `OPEN_EXISTING`, closed at once when it succeeds, so the probe never creates, truncates or writes the file; a missing file or a successful open is "not held", `ERROR_SHARING_VIOLATION` is "held" and the holder line is then read as that section states. Held, the cook re-probes and re-checks the journal every 50 ms for up to `cook.sourceLockWaitMs`: lock free and journal gone, it scans; still held at the end, it exits with code 6 and `COOK_ERROR_SOURCE_LOCKED` ("a writer is active at <root> (PID, mode); retry the build"), never exit code 3 and never a recovery command. Not held, at the first probe or once the holder released it with the journal still present, the journal is a dead writer's and the cook stops with exit code 3 as that section states.

`Test_Cook_ConcurrentServerAndBuildExclusive` runs a `--serve` process with an in-flight cook request, starts a build cook of the same tree (exit code 6, the tree byte-identical before and after, the server's temporaries present), finishes the server's request, shuts it down, and runs the build cook again (exit code 0); a second case starts the build cook first and asserts the server's `hello` gets `COOK_ERROR_OUTPUT_LOCKED` and exit code 6; a third holds the database lock in one process and asserts a cook of the other platform succeeds without writing `assetdb.zdb`; a fourth has a helper process hold a storage root's writer lock with a framework journal at the root (a live writer mid-batch) and asserts that `--assign-guids --set cook.sourceLockWaitMs=500` exits 6 with `COOK_ERROR_SOURCE_LOCKED` leaving the journal and every `.meta` byte-identical, that a build cook in the same situation exits 6, not 3, with no command in its message, and that the build cook succeeds once the helper deletes its journal and releases the lock; a fifth starts two `--assign-guids` processes over one new source and asserts exactly one `.meta` with one GUID; a sixth has a server answer a `setSettings` while a helper holds the game root's writer lock: the request gets `COOK_ERROR_SOURCE_LOCKED` and the session continues.

### Atomic output publication

- Every output is written to `<final path>.<pid>.z2cook` beside its final path (`<pid>` the writer's decimal process ID), flushed, verified (its header parse for asset files, its hash), then renamed over the final path (`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`); a journal line records the publication ("Interrupted cook and resume").
- The catalogue and mount manifest are published last, after every asset they list, so a tree whose catalogue is present lists only complete files.

## Validation

### Structural validation

Every published asset file is re-read and parsed with `Zenith_ParseCookedHeader` against the expectation its catalogue entry will carry, and every section is verified with `Zenith_VerifySection` and decoded with `Zenith_DecompressSection` (a test-decode that proves the runtime can read it); a failure is `COOK_ERROR_OUTPUT_INVALID` and the file is not published. The owner's step may register a kind validator (`Zenith_CookStepRegistry::RegisterValidator(kind, pfn)`) that parses the sections as the runtime loader would.

### Reference validation

- Every `DeclareReference` target must be in the plan for the same platform: a hard reference to an absent GUID is `COOK_ERROR_MISSING_REFERENCE` (the referrer fails); a soft reference is `COOK_WARNING_MISSING_SOFT_REFERENCE`.
- A reference to an orphaned sub-asset or a tombstoned GUID is an error naming the referrer and the target's last logical path.
- Hard-reference cycles are `COOK_ERROR_DEPENDENCY_CYCLE`.

### Content rules

- Every numeric cell of `Zenith2/Docs/ContentSpecs.md` is checked at cook time (Budgets [Z2-N-BUDGET-418], BR-COOK). The limits are read at cook start from the data file `Zenith2/Docs/ContentSpecs.rules.json` (Formats.md "Content rule table"), never from the Markdown: `Internal/Cook/Zenith_ContentRules.cpp` parses and validates it (`COOK_ERROR_RULE_TABLE_INVALID`, exit code 3), and the depfile lists it.
- The owning area's step registers the measure of each quantity (`Zenith_CookStepRegistry::RegisterRule(Zenith_ContentRule{kind, quantity, measure})`); after an item cooks, the driver pairs each table row of the item's kind with the registered rule of the same quantity, picks the class by the `.meta` `labels` (first matching selector, else `*`), measures, and compares; an exceeded limit is `COOK_ERROR_RULE` naming the asset, the table, the class and both values. A table row whose quantity no step registered is `COOK_ERROR_RULE_TABLE_INVALID`; a registered rule with no row imposes nothing and is listed once as `COOK_WARNING_RULE_WITHOUT_LIMIT`.
- `Test_ContentRules_TableAgreesWithContentSpecs` (a ZenithCook test, which may read Markdown with the STL) parses the numeric cells of every rule table of `ContentSpecs.md` and asserts the JSON carries the same tables, classes and limits, so the two files cannot drift.

### Validation report

`.cook/validation-report.json` (Formats.md "Cook report", member `validation`) lists every finding with severity, code, asset GUID, logical path, source path, rule and message, sorted by (severity, code, GUID); `--validate-only` produces it without cooking changed items. DEVT's content-validation tooling reads it (Z2-I-DEVT-23).

## Global outputs

### Catalogue generation

`asset.catalogue` writes `assets.zcat` (Formats.md "Asset catalogue file") from every successfully cooked asset item: entries sorted by GUID; logical paths; object paths; file bytes, header bytes, resident estimates, key and output hashes; hard and soft dependencies; the file entries of every non-asset cooked file SCRIPT, ECS and RPROD produced (path, bytes, Hash128); the kind format-version table (0 for kinds with no content); the fallback table of "Engine fallbacks"; the content hash. PACK's packaging may split it into fragments per asset pack (`Catalogues/<pack>.zcat`), writing a dependency on an asset outside the fragment as an external reference (Formats.md "Catalogue fragments"); the development cook writes one base catalogue.

### Mount manifest

`asset.manifest` writes `mount-manifest.json` in PLAT's schema (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount manifest schema"`) for the development tree: `game`, `buildId` (the ZenithCook-provided build ID of the configured game, from `composition.json`'s build record), `releaseLine`, `contentHash` (the catalogue's, 16 lowercase hex digits), an empty `package` list (the loose mount serves development content); it validates the result with `Zenith_ParseMountManifest` before publishing.

### Configuration merge

`plat.config` produces, per `Zenith2/Docs/Design/PLAT/Configuration.md → "Engine and game sub-sources"`:

1. `content:/Config/Platform/<platform>.config.json`: every linked library's `Internal/Config/Platform/<platform>.config.json` fragment merged (a key defined by two fragments is `COOK_ERROR_CONFIG_KEY_CONFLICT` naming both files), then the game's `Assets/Config/Platform/<platform>.config.json` members overriding key by key; members written in key byte order.
2. `content:/Engine/Config/Quality/<tier>.config.json` from every library's `Internal/Config/Quality/<tier>.config.json` the same way (no game overrides: the game's own `content:/Config/Quality/<tier>.config.json` is copied separately).
3. `content:/Engine/Config/Quality/tiers.<platform>.json`: ZenithPlatform's `Internal/Config/Quality/tiers.<platform>.json` validated against the `zenith2.quality-tiers` schema and copied (the tier list is engine data, never a game file, `Zenith2/Docs/Design/PLAT/Configuration.md → "Quality tier resolution"`); the game's `Config/game.config.json` and `Config/Quality/*.config.json` validated against the tools build's configuration schema and copied.
4. Every `*.bindings.json` validated with `Zenith_ValidateBindingsJson` and copied: the game's under their logical paths, the engine bindings (ZenithPlatform's `Internal/Content/Input/Engine.bindings.json`, engine content) to `content:/Engine/Input/Engine.bindings.json`.

Every merged or copied file is a file entry of the catalogue; a configuration file failing validation is `COOK_ERROR_CONFIG_INVALID`.

### Dependency file

`cook.d` (Formats.md "Dependency file") lists the stamp as the target and every file the cook read (sources, `.meta`, fragments, ContentSpecs.md, `composition.json`, the ZenithCook executable), so Ninja reruns `<Name>_Content` exactly when an input changes ([Z2-N-ARCH-207]).

### Cook report

`.cook/cook-report.json` (Formats.md "Cook report"): the summary (items per phase and outcome, DDC hits and misses per store, bytes written, the content hash), every diagnostic, the members contributed by steps (GEOM's `vertexFallbacks`, `meshes`, `streamSets`, `materials`, `permutations`, `textures`), and the validation findings; with no times or host names in it, so equal inputs give an equal report; the timings go to `.cook/cook-timings.json`.

Every report lists, for every live source of the game with a recorded import, the diagnostics of the import that produced its current intermediates, whether that import ran in this cook or not: a source whose import was skipped (CURRENT after the scan, or a verified DDC hit of its import) contributes its stored diagnostics marked `"replayed": true` (Import.md "Replay into the cook report"). So the report of an incremental cook, of a cook over a shared DDC and of a clean cook of the same inputs differ only in which entries carry `"replayed": true` (`Test_CookReport_ReplayedEqualsLive`), and a consumer that matches a diagnostic `code` (PACK's stale-bake check, `Zenith2/Docs/Design/PACK/Tooling.md → "Package build steps"` step 1b) sees the same findings in all three.

## Interrupted cook and resume

- The cook appends one line per published output to `.cook/journal.jsonl` (Formats.md "Cook journal"): the path, the key, the output hash; the journal is flushed after each batch of 64 lines and at the end.
- The journal belongs to the holder of the output lock ("Output tree and database locks"): a cook reads or writes it only while holding the lock, so a journal without an `end` line that a new holder finds was left by a process that has exited, never by a live one.
- A cook that takes the lock and finds a journal from an unfinished run (no `end` line) deletes every `*.<pid>.z2cook` temporary in the tree (logging each PID; none belongs to a live process, because only the lock's holder writes temporaries), verifies each journaled output's hash against its file (a mismatch deletes the file), and treats verified outputs as the tree's current outputs for "Keys and invalidation"; the run then proceeds as an ordinary incremental cook and ends by rewriting the journal with an `end` line.
- `Test_Cook_InterruptedResumesToCleanBytes` kills a cook (a fault point after N publications, for N sampled across the run) and asserts the resumed cook's tree equals a clean cook's.

## Low disk during a cook

Before publishing each batch (at most 64 outputs, or one output larger than 256 MiB), the driver checks the output volume's free bytes (`GetDiskFreeSpaceExW`) against the batch's bytes plus `cook.lowDiskMarginBytes`; short, it stops with exit 4 (`COOK_ERROR_LOW_DISK`) after deleting the batch's temporaries; outputs already published and journaled stay valid, so the next cook resumes. The local DDC checks its own volume before populating (DDC.md "Size cap and eviction").

## Stale output removal

After a successful cook, files in `Objects/` and in global steps' owned prefixes that no current item produced are deleted (an asset removed from the content, a renamed global output), and `Catalogues/` fragments the plan did not write are deleted; files outside every owned prefix and `.cook/` are left and warned about (`COOK_WARNING_FOREIGN_FILE`). The tree then contains exactly the catalogue's files plus the global outputs.

## The cook server

### Protocol

- `ZenithCook --serve` is a long-lived process per (game, platform) that the editor starts; it holds its output tree's lock for its whole life ("Output tree and database locks"), keeps the database, the registries and the DDC open, and speaks protocol version 1 of Formats.md "Cook server messages" over its stdin and stdout: a `hello` handshake that compares the protocol and engine cook versions first; each request carries an `id`; the server answers each request with zero or more `partial` pages and then exactly one `result` or `error`, and sends `progress`, `diagnostic`, `changed` and `published` notifications in between.
- Requests are processed one at a time in arrival order; `hello`, `cancel` and `watch` are answered on arrival; `cancel` removes a queued request or interrupts the running one at its next item boundary ("Session rules" of Formats.md "Cook server messages"); an unknown request name or a malformed line is answered with `COOK_ERROR_PROTOCOL` and the session continues.
- Watching (`watch` with `enable: true`): the server watches every content root of the game and the linked engine content roots (`ReadDirectoryChangesW`, recursive, on Windows; the tool is Windows-only for watching); after `debounceMs` without a further change it sends `changed`, then runs an internal scan, import and cook of the changed sources in arrival order with client requests, and sends `published` when outputs were published. Changes under `.`-prefixed names and outside the content roots are ignored.
- A sidecar-writing request holds the writer locks of the storage roots it touches for its own duration only ("Output tree and database locks" item 5); refused them, it is answered `COOK_ERROR_SOURCE_LOCKED` and the session continues.
- A server crash is a process exit the editor detects (EOF on stdout); the editor restarts it, and the journal makes an interrupted cook resume.
- The server exists only in the tools build; no Player contains it or any cook code ([Z2-N-ARCH-958], [Z2-N-ARCH-241]).

### Requests

Every request's `args` and `result` keys, types and presence are Formats.md "Requests"; in summary:

| Request | Arguments | Result |
|---|---|---|
| `hello` | protocol version, engine cook version, client name | the server's versions, game, platform, build ID |
| `list` | a logical-path prefix, kinds, states | paged record summaries |
| `find` | text, limit | paged matching summaries |
| `record` | GUID | the full record, settings, sub-assets, the `.meta` digest |
| `dependencies`, `dependents` | GUID, edge kinds, transitive | paged edges |
| `assignGuids` | source paths | the created `.meta` paths and GUIDs, each new `.meta`'s preset choice; move suggestions |
| `setSettings` | GUID, expected `.meta` digest, and any of: settings object (with platform), preset, provenance, sub-asset owner members, sub-asset remaps, orphan removals, sub-asset restores | the written `.meta` and its digest (journaled) |
| `import`, `reimport` | GUIDs | the changed GUIDs, the failures |
| `cook` | GUIDs or `all` | paged published outputs; the catalogue change and content hash |
| `validate` | GUIDs or `all` | paged findings |
| `provenance` | GUID or path, depth | the provenance chain |
| `property` | GUIDs, platform, property name | paged property bytes or per-asset errors, cooking each producer item that misses into the DDC (Import.md "Published-property inputs") |
| `migratePreview`, `migrateApply` | `.meta` paths; the preview token | paged plan entries and the token; the applied result |
| `watch` | enable, debounce | the watched roots |
| `cancel` | request `id` | whether a request was cancelled, and its state |
| `shutdown` | — | the process exits after saving the database |

## Cook errors

```cpp
// Zenith2/Tools/ZenithCook/Internal/Cook/Zenith_CookErrors.h
/// @contract enum Zenith_CookErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the exit code each maps to is the paragraph below this block.
/// Version: since Z2 Stage 2. Tool-side; values are appended, never renumbered (the server and the report carry them by name).
/// @endcontract
enum Zenith_CookErrorCode : uint16
{
	COOK_ERROR_BAD_COMMAND_LINE = 1,
	COOK_ERROR_GAME_INVALID,
	COOK_ERROR_COMPOSITION_INVALID,
	COOK_ERROR_META_MISSING,
	COOK_ERROR_META_SUBASSET_MISSING,
	COOK_ERROR_DUPLICATE_GUID,
	COOK_ERROR_NAME_NOT_NFC,
	COOK_ERROR_BAD_SOURCE_NAME,
	COOK_ERROR_LOGICAL_PATH_COLLISION,
	COOK_ERROR_OUTPUT_COLLISION,
	COOK_ERROR_STARTUP_SCENE_MISSING,
	COOK_ERROR_IMPORT_FAILED,
	COOK_ERROR_STEP_FAILED,
	COOK_ERROR_INVALID_STEP,
	COOK_ERROR_DEPENDENCY_FAILED,
	COOK_ERROR_DEPENDENCY_CYCLE,
	COOK_ERROR_MISSING_REFERENCE,
	COOK_ERROR_OUTPUT_INVALID,
	COOK_ERROR_RULE,
	COOK_ERROR_CONFIG_KEY_CONFLICT,
	COOK_ERROR_CONFIG_INVALID,
	COOK_ERROR_TEST_KIND_IN_CONTENT,
	COOK_ERROR_WORKER_CRASHED,
	COOK_ERROR_WORKER_TIMEOUT,
	COOK_ERROR_LOW_DISK,
	COOK_ERROR_IO,
	COOK_ERROR_DDC,
	COOK_ERROR_OUT_OF_MEMORY,
	COOK_ERROR_PERMUTATION_MISSING,      // RPROD's, [Z2-N-REND-125]
	COOK_ERROR_PERMUTATION_LIMIT,        // RPROD's, [Z2-N-REND-126]
	COOK_ERROR_PROTOCOL,                 // a malformed or unknown cook server request or worker line
	COOK_ERROR_PROTOCOL_VERSION,         // hello or workerHello versions differ
	COOK_ERROR_CANCELLED,                // a server request ended by cancel or shutdown
	COOK_ERROR_META_CHANGED,             // a .meta changed since the digest or preview token the request carried
	COOK_ERROR_FALLBACK_MISSING,         // a declared engine fallback is absent from the database
	COOK_ERROR_RULE_TABLE_INVALID,       // ContentSpecs.rules.json fails its schema, or names an unregistered quantity
	COOK_ERROR_OUTPUT_LOCKED,            // another ZenithCook process holds the output tree's lock (exit code 6)
	COOK_ERROR_RESULT_TOO_LARGE,         // a server result item larger than the 4 MiB line limit
	COOK_ERROR_SOURCE_LOCKED,            // a storage root's writer lock stayed held for cook.sourceLockWaitMs (exit code 6)
	COOK_ERROR_SOURCE_CHANGED,           // a source document OpenSourceDocument read differs from the plan's scan (the item fails)
	// STREAM's `stream.world` step (cross-area row ST6; reserved here, appended by STREAM's change in this order):
	COOK_ERROR_STREAM_DOCUMENT, COOK_ERROR_STREAM_GRID, COOK_ERROR_STREAM_CELL_RANGE, COOK_ERROR_STREAM_CELL_DUPLICATE,
	COOK_ERROR_STREAM_CELL_SCENE_KIND, COOK_ERROR_STREAM_CELL_ANCHOR, COOK_ERROR_STREAM_CELL_BOUNDS, COOK_ERROR_STREAM_DEPENDENCY_CYCLE,
	COOK_ERROR_STREAM_RADII, COOK_ERROR_STREAM_REFERENCE, COOK_ERROR_STREAM_PERSISTENT_ID, COOK_ERROR_STREAM_HLOD, COOK_ERROR_STREAM_LIMIT,
};

/// @contract value Zenith_CookError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: None.
/// Errors: None; it is the error.
/// Version: since Z2 Stage 2. Tool-side.
/// @endcontract
struct Zenith_CookError                  // 24 bytes
{
	Zenith_CookErrorCode m_eCode;
	uint16 m_uDetail;                    // the underlying enumeration's value (import, database, DDC, asset, step-specific)
	uint32 m_uReserved = 0;
	Zenith_AssetGUID m_xAsset;            // null for global failures
};
constexpr const char* Zenith_ToString(Zenith_CookErrorCode eError);
```

Warnings (`COOK_WARNING_*`, codes 1000 + index in this order: `UNRECOGNISED_FILE`, `PROBABLE_MOVE`, `ORPHANED_META`, `MISSING_SOFT_REFERENCE`, `FOREIGN_FILE`, `UNKNOWN_SETTING`, `DDC_UNAVAILABLE`, `RULE_WITHOUT_LIMIT`, `TOMBSTONE_NOT_TRACKED`, `DATABASE_LOCKED`) are diagnostics in the report and never change the exit code. The thirteen `COOK_ERROR_STREAM_*` codes fail their item (exit code 1) like `COOK_ERROR_STEP_FAILED`, with STREAM's detail; they are reserved by ASSET and added to `Zenith_CookErrors.h`, its `Zenith_ToString` and ZenithCook's error-header test by STREAM's own change (the growth rule of the kind table applied to this enumeration: the owner's diff, ASSET reviewing). The exit code of the new errors: `PROTOCOL` and `PROTOCOL_VERSION` end a server session with code 2 as "Session rules" states and a cook with code 5 when a worker's hello mismatches; `FALLBACK_MISSING` and `RULE_TABLE_INVALID` are exit code 3; `OUTPUT_LOCKED` is exit code 6 (and a server's `hello` error); `SOURCE_LOCKED` is exit code 6 for a CLI mode or a build cook and a server request's error; `SOURCE_CHANGED` fails its item (exit code 1, its independent items still publish) and a server request's cook reports it per item; `CANCELLED` and `RESULT_TOO_LARGE` exist only in server responses; `META_CHANGED` is a server request's error and, in a sidecar-writing CLI mode, exit code 6.
