# CONTENT design: recoverable batch operations and reference repair

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies the batch engine (`Zenith_ContentBatchOperation` in `ZenithAuthoring`): the batch specification, planning, chunked execution, the operation journal, resume, abandon, rollback and the report; the operations built on it (move, rename, delete with reference repair, reimport and settings batches, sub-asset remaps, orphan repair); the generic reference repairer beside SCENE's planner; the cook channel through which a batch reaches the cook server; and the `ZenithContent batch` command line. The engine sits above two mechanisms it never re-specifies: the document framework's per-batch journaled write (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"`) and ASSET's cook server requests (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook server messages"`).

## Scope of this file

| Topic | Heading |
|---|---|
| the C++ surface | "Batch API" |
| what a batch is asked to do | "Batch specification", "Operation kinds" |
| how it runs and recovers | "Planning", "Chunked execution", "Operation journal", "Resume algorithm", "Abandon and rollback", "Batch state machine" |
| what it reports | "Batch report" |
| the operations | "Move and rename", "Delete", "Reimport and settings batches", "Sub-asset remap and orphan repair" |
| repairs | "Generic reference repairer" |
| reaching the cook server | "Cook channel" |
| the tool | "Batch command line" |

<!-- informative -->
The framework's journaled write makes one `SaveDocuments` batch atomic, but a folder move touching 3,000 scenes is too large for one batch (memory, the backup retention of 20 save batches, and the editor freezing for minutes) and it also moves files and talks to the cook server, which no document journal covers. The batch engine therefore splits the work into chunks, makes each chunk re-checkable by SHA-256 digests of every file it touches and by the recorded before-state of every `.meta` it changes through the cook server, keeps its own journal (an immutable plan, and per started chunk an immutable prepared file and one small mutable file), groups targets that share referencing documents into the same chunk so a scene is repaired once rather than once per chunk, and runs, planning included, in units whose cook requests are asynchronous; the editor runs it on its own batch thread, so it keeps drawing frames while a 3,000-scene move runs; an interruption anywhere leaves every chunk fully done or fully not done after `Resume`, and the report says exactly what changed.
<!-- /informative -->

## Batch API

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
class Zenith_ContentCookChannel;
class Zenith_DocumentMigrator;
class Zenith_DocumentStorage;
class Zenith_PrefabProvider;
class Zenith_TypeRegistry;

/// @contract enum Zenith_ContentBatchKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized by name without prefix, lower camel case ("move", "delete", "reimport", "setPreset",
///   "setSettings", "remapSubAssets", "repairOrphans").
/// @endcontract
enum Zenith_ContentBatchKind : uint8
{
	CONTENT_BATCH_KIND_MOVE = 1,
	CONTENT_BATCH_KIND_DELETE,
	CONTENT_BATCH_KIND_REIMPORT,
	CONTENT_BATCH_KIND_SET_PRESET,
	CONTENT_BATCH_KIND_SET_SETTINGS,
	CONTENT_BATCH_KIND_REMAP_SUB_ASSETS,
	CONTENT_BATCH_KIND_REPAIR_ORPHANS,
};

/// @contract enum Zenith_ContentBatchState
/// Ownership: Plain value; the states of "Batch state machine".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as the upper-case name without prefix ("RUNNING"); values are appended, never
///   renumbered, so PLANNING, which no journal ever holds, is the last.
/// @endcontract
enum Zenith_ContentBatchState : uint8
{
	CONTENT_BATCH_STATE_PLANNED = 1,
	CONTENT_BATCH_STATE_RUNNING,
	CONTENT_BATCH_STATE_PAUSED,
	CONTENT_BATCH_STATE_COMPLETED,
	CONTENT_BATCH_STATE_ABANDONED,
	CONTENT_BATCH_STATE_ROLLING_BACK,
	CONTENT_BATCH_STATE_ROLLED_BACK,
	CONTENT_BATCH_STATE_PLANNING,      // in memory only: Plan returned, planning units still running; never journaled
};

/// @contract enum Zenith_ContentChunkState
/// Ownership: Plain value; the steps of "Chunked execution".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as the upper-case name without prefix.
/// @endcontract
enum Zenith_ContentChunkState : uint8
{
	CONTENT_CHUNK_STATE_PENDING = 1,
	CONTENT_CHUNK_STATE_PREPARED,      // edits computed; before-copies and staged new bytes written
	CONTENT_CHUNK_STATE_MOVING,
	CONTENT_CHUNK_STATE_SAVING,
	CONTENT_CHUNK_STATE_COOK_PENDING,
	CONTENT_CHUNK_STATE_DONE,
	CONTENT_CHUNK_STATE_CONFLICT,
	CONTENT_CHUNK_STATE_REVERTED,
};

/// @contract value Zenith_ContentBatchSpecElements
/// Ownership: Plain values; every view and span is borrowed for the Plan call, which copies them into the journal.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after Plan returns.
/// Units: Paths are game-relative ("Assets/Props/Crate.glb"), forward slashes.
/// Errors: Plan validates them ("Batch specification"; CONTENT_ERROR_SPEC_INVALID with the rule index).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentMoveSpec { const Zenith_StringView m_strFrom = {}; const Zenith_StringView m_strTo = {}; };      // a file or a directory
struct Zenith_ContentDeleteSpec { const Zenith_StringView m_strPath = {}; const Zenith_PersistentID m_xReplacement = {}; const bool m_bUnpackInstances = false; };
struct Zenith_ContentKeyPair { const Zenith_StringView m_strFrom = {}; const Zenith_StringView m_strTo = {}; };
struct Zenith_ContentSubAssetRemapSpec { const Zenith_PersistentID m_xSource = {}; const Zenith_Span<const Zenith_ContentKeyPair> m_axMap = {}; };
struct Zenith_ContentOrphanRepairSpec { const Zenith_PersistentID m_xOrphan = {}; const Zenith_PersistentID m_xReplacement = {}; };   // null: references nulled
struct Zenith_ContentAssetFilter
{
	const Zenith_Span<const Zenith_PersistentID> m_axGUIDs = {}; // explicit set, or empty
	const Zenith_StringView m_strImporter = {};      // "" any
	const Zenith_Span<const Zenith_StringView> m_astrLabels = {}; // every label must be present; empty: no label condition
	const Zenith_StringView m_strPathPrefix = {};    // game-relative; "" any
	const bool m_bAll = false;                       // every source of the game (ignored when m_axGUIDs is non-empty)
};

/// @contract value Zenith_ContentBatchSpec
/// Ownership: Plain value; as Zenith_ContentBatchSpecElements.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Zenith_ContentBatchSpecElements.
/// Version: since Z2 Stage 4. Its JSON form is "Batch specification".
/// @endcontract
struct Zenith_ContentBatchSpec
{
	const Zenith_ContentBatchKind m_eKind = CONTENT_BATCH_KIND_MOVE;
	const Zenith_Span<const Zenith_ContentMoveSpec> m_axMoves = {};
	const Zenith_Span<const Zenith_ContentDeleteSpec> m_axDeletes = {};
	const Zenith_ContentAssetFilter m_xFilter = {};  // reimport, setPreset, setSettings
	const Zenith_PersistentID m_xPreset = {};        // setPreset (null clears)
	const Zenith_JsonDocument* const m_pxSettings = nullptr; // setSettings: an object of settings keys merged into each .meta
	const Zenith_Span<const Zenith_ContentSubAssetRemapSpec> m_axRemaps = {};
	const Zenith_Span<const Zenith_ContentOrphanRepairSpec> m_axOrphans = {};
	const bool m_bPruneDangling = false;             // SCENE's m_bPruneDangling for the repairs
};

/// @contract value Zenith_ContentBatchDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the operation.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Milliseconds for m_uLockWaitMs; UTC "YYYYMMDDTHHMMSSZ" for m_szTimestamp (the tool's clock; the engine reads none);
///   microseconds of m_xClock for Step's budget; bytes for m_pfnQueryFreeBytes.
/// Errors: Plan and Open return CONTENT_ERROR_INVALID_ARGUMENT for a null pointer, a root span whose first entry is not the game's
///   writable storage, a chunk size outside 1 … 1,024 or a document cap outside 1 … 4,096, and when called on a job worker.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentBatchDesc
{
	const Zenith_StringView m_strZenithRoot = {};    // host path of Zenith2/
	const Zenith_StringView m_strGame = {};
	Zenith_DocumentStorage* const m_pxGameStorage = nullptr;     // the game directory's writable host storage
	Zenith_DocumentMigrator* const m_pxGameMigrator = nullptr;   // over it
	const Zenith_Span<Zenith_DocumentMigrator* const> m_apxEngineMigrators = {}; // read-only, one per linked engine content root
	Zenith_ContentCookChannel* const m_pxCook = nullptr;
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;        // the game's frozen registry (SCENE's planner)
	Zenith_PrefabProvider* const m_pxPrefabs = nullptr;          // SCENE's planner
	const uint32 m_uChunkTargets = 64;              // content.batch.chunkTargets
	const uint32 m_uMaxDocumentsPerSave = 256;      // content.batch.maxDocumentsPerSave
	const uint32 m_uLockWaitMs = 10000;             // content.batch.lockWaitMs
	const char* const m_szTimestamp = nullptr;
	const Zenith_ContentClock m_xClock = {};         // null members: Zenith_GetSystemContentClock() (ContentCore); Step's budget reads it
	// The batch's own free-space check reads this; null: m_pxGameStorage->QueryFreeBytes(). A declared seam with a production
	// default (the storage), so the low-disk test sets a function and no storage is doubled.
	const Zenith_PfnContentFreeBytes m_pfnQueryFreeBytes = nullptr;
	void* const m_pFreeBytesUserData = nullptr;
	Zenith_FaultInjector* const m_pxFaults = nullptr; // fault points 0x43540010 … ("Chunked execution")
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract value Zenith_ContentBatchProgress
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentBatchProgress
{
	Zenith_ContentBatchState m_eState;
	uint32 m_uChunksDone, m_uChunksTotal, m_uTargetsDone, m_uTargetsTotal, m_uDocumentsWritten, m_uMovesDone;
	uint32 m_uDocumentsIndexed, m_uDocumentsToIndex;   // PLANNING: the reference index's progress (0 when not indexing)
};
// Called on the owner thread after every unit of work, planning units included; returning false requests cancellation at the
// next chunk boundary (in PLANNING: at the end of the current unit).
using Zenith_PfnContentBatchProgress = bool (*)(void* pUserData, const Zenith_ContentBatchProgress& xProgress);

/// @contract value Zenith_ContentBatchStepResult
/// Ownership: Plain value returned by Step.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Microseconds of the desc's monotonic clock; counts.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentBatchStepResult
{
	const bool m_bDone = false;                       // this call reached PLANNED (from PLANNING), COMPLETED, PAUSED, ABANDONED or ROLLED_BACK
	const bool m_bWaitingOnCook = false;              // a cook request is in flight; the next Step polls it
	const uint32 m_uUnitsRun = 0;                    // units of "Chunked execution" this call performed (0 only while waiting)
	const uint64 m_ulElapsedUs = 0;                  // this call's time
	const Zenith_ContentBatchProgress m_xProgress = {};
};

/// @contract type Zenith_ContentBatchOperation
/// Ownership: Created by Plan or Open as a Zenith_Owned; owns the plan, the current chunk's journal image and the operation lock
///   while it lives; borrows the desc's objects. Destroying it releases the lock; an operation still PLANNING has written nothing.
/// Thread affinity: OWNER_THREAD: the one thread that called Plan or Open, which may be any thread that is not a job worker
///   (EDIT's EditorContentBatch thread in the editor, the main thread in ZenithContent); every subsequent call is made on it
///   (ZENITH_CHECK otherwise).
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_Result<…, Zenith_ContentError> with the codes of Zenith2/Docs/Design/CONTENT.md "Errors"; every failure leaves
///   the journal describing the files exactly ("Resume algorithm").
/// Reentrancy: NONE. The progress callback must not call the operation; a Zenith_ReentrancyGuard reports a violation.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_ContentBatchOperation
{
public:
	// Validates what the spec alone decides ("Planning" step 1), copies it, takes the operation lock and returns the operation in
	// PLANNING without reading a document or sending a request; the planning units run in Step or Run. Writes nothing.
	static Zenith_Result<Zenith_Owned<Zenith_ContentBatchOperation>, Zenith_ContentError> Plan(
		const Zenith_ContentBatchDesc& xDesc, const Zenith_ContentBatchSpec& xSpec);
	// Opens an existing operation's journal (plan.json, state.json and its chunk files) and takes the operation lock.
	static Zenith_Result<Zenith_Owned<Zenith_ContentBatchOperation>, Zenith_ContentError> Open(
		const Zenith_ContentBatchDesc& xDesc, Zenith_StringView strOperationID);
	// Operation IDs (directories holding a plan.json) under the game's .migration/content-ops/, ascending (no lock taken).
	static Zenith_Result<uint32, Zenith_ContentError> List(const Zenith_ContentBatchDesc& xDesc,
		Zenith_Array<Zenith_String>& astrOut);

	Zenith_StringView GetID() const;
	Zenith_ContentBatchKind GetKind() const;
	Zenith_ContentBatchState GetState() const;
	Zenith_ContentBatchProgress GetProgress() const;
	// The documents each target's repair will touch, for the editor's preview (from PLANNED on; empty while PLANNING).
	Zenith_Span<const Zenith_StringView> GetReferencingDocuments(uint32 uTarget) const;

	// Performs units ("Planning" in PLANNING, "Chunked execution" otherwise) while the elapsed time plus the next unit's
	// estimate stays within uMaxMicroseconds ("Time budget"); never splits a unit, never blocks on the cook channel, and runs at
	// least one unit when none is waiting. PLANNING (ends the call at PLANNED), PLANNED, PAUSED (resume) or RUNNING; the same
	// errors as Run, returned by the call that reaches them.
	Zenith_Result<Zenith_ContentBatchStepResult, Zenith_ContentError> Step(uint32 uMaxMicroseconds,
		Zenith_PfnContentBatchProgress pfnProgress, void* pUserData);
	// Step(UINT32_MAX) in a loop until m_bDone, sleeping 1 ms (Zenith_SleepUs(1000)) between calls while a request is in flight,
	// so it polls and never busy-waits: from PLANNING it returns at PLANNED, from PLANNED or PAUSED at COMPLETED or PAUSED. The
	// editor's batch thread and ZenithContent use it; plan-then-run is two Run calls.
	Zenith_Result<void, Zenith_ContentError> Run(Zenith_PfnContentBatchProgress pfnProgress, void* pUserData);   // PLANNING, PLANNED or PAUSED
	// Abandon and Rollback poll the channel as Run does.
	Zenith_Result<void, Zenith_ContentError> Abandon();                          // PLANNED or PAUSED
	Zenith_Result<void, Zenith_ContentError> Rollback(Zenith_PfnContentBatchProgress pfnProgress, void* pUserData);   // PAUSED, COMPLETED, ABANDONED, ROLLING_BACK
	Zenith_Result<void, Zenith_ContentError> Discard();                          // COMPLETED, ABANDONED, ROLLED_BACK
	Zenith_Result<void, Zenith_ContentError> WriteReportJson(Zenith_Array<char>& xOut) const;
};

inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_JOURNAL_WRITTEN = 0x43540010u;   // after any journal file write (plan, state, prepared, element or chunk)
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_PREPARED = 0x43540011u;
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_MOVE = 0x43540012u;              // after each individual move ("Fault-point consultation order")
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_SAVE = 0x43540013u;              // after each SaveDocuments sub-batch
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_COOK = 0x43540014u;              // a cook request's poll returned DONE, its result not yet journaled
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_DONE = 0x43540015u;
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_META_RECORDED = 0x43540016u;    // a .meta before-state journaled, its request not sent
inline constexpr uint32 uFAULT_POINT_CONTENT_BATCH_META_APPLIED = 0x43540017u;     // the server answered a .meta request, the journal not yet updated
```

The clock and free-space seams are ContentCore's (`Zenith2/Tools/ContentCore/Zenith_ContentTypes.h`), so the batch engine, the distribution coordinator and the merge driver's time bound share one:

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentTypes.h
/// @contract value Zenith_ContentClock
/// Ownership: Plain value; the user data is borrowed and outlives every caller.
/// Thread affinity: IMMUTABLE; the functions are ANY_THREAD.
/// Invalidation: None.
/// Units: m_pfnMonotonicUs microseconds of a monotonic clock; m_pfnUtcSeconds seconds since 1970-01-01T00:00:00Z.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; tests install a scripted clock, production code Zenith_GetSystemContentClock().
/// @endcontract
struct Zenith_ContentClock
{
	uint64 (* const m_pfnMonotonicUs)(void* pUserData) = nullptr;
	uint64 (* const m_pfnUtcSeconds)(void* pUserData) = nullptr;
	void* const m_pUserData = nullptr;
};
// CORE's Zenith_GetMonotonicTimeUs and Zenith_GetUtcTime (Zenith2/Docs/Design/CORE/Diagnostics.md "Clocks"), user data null.
Zenith_ContentClock Zenith_GetSystemContentClock();
using Zenith_PfnContentFreeBytes = uint64 (*)(void* pUserData);   // free bytes for writes under the game root
```

A fault point that fires makes the call return `CONTENT_ERROR_IO` at once with nothing further written, which is how tests simulate a crash at that point in process; the integration suite kills a real child process (Tests.md "Integration suite").

## Batch specification

The JSON form `ZenithContent batch plan` reads (`$schema` `zenith2.content-batch-spec`, version 1; at most 64 MiB):

```json
{
	"$schema": "zenith2.content-batch-spec",
	"schemaVersion": 1,
	"kind": "move",
	"moves": [ { "from": "Assets/Props/Old", "to": "Assets/Props/Crates" } ],
	"pruneDangling": false
}
```

| Key | Kinds | Rule |
|---|---|---|
| `kind` | all | one of the seven names (B1) |
| `moves[]` `{from, to}` | move | 1 … 1,048,576 entries; game-relative paths under one of the game's content roots, validated as virtual paths (R1–R10 of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Validation rules and order"`); `from` exists; `to` does not, and no two entries share a `to`; a `from` directory moves every file beneath it (B2) |
| `deletes[]` `{path, replacement, unpackInstances}` | delete | `path` an asset source (not a `.meta`); `replacement` a GUID text of a live asset of the same kind, or null (B3) |
| `filter` `{guids, importer, labels, pathPrefix, all}` | reimport, setPreset, setSettings | at least one condition (B4) |
| `preset` | setPreset | a preset GUID text, or null to clear; its `importer.id` must equal each target's importer (B5) |
| `settings` | setSettings | an object of settings keys, validated per target against its importer (B6) |
| `remaps[]` `{source, map: {old key: new key}}` | remapSubAssets | `source` a live source GUID; each old key orphaned in its `.meta`, each new key live (B7) |
| `orphans[]` `{guid, replacement}` | repairOrphans | `guid` an orphaned sub-asset GUID; `replacement` a live GUID of the same kind or null (B8) |
| `pruneDangling` | move, delete, repairOrphans | boolean, default false (B9) |
| (derived at planning) | move, delete, remapSubAssets, repairOrphans | no target is referenced by more than 65,536 documents, and the operation's document-repair count W ("Planning" step 4) is at most 1,000,000 (B11) |
| (derived at planning) | all | the journal's own limits ("Operation journal"): at most 1,048,576 expanded targets (checked at step 2 as each target is added), at most 16,384 chunks (step 4), each chunk's prepared-file bound at most 256 MiB (step 5), and the serialized `plan.json` at most 256 MiB (step 6, checked on the bytes before the write) (B12) |

Targets must lie in the game's own content roots: an engine-content target is `CONTENT_ERROR_SPEC_INVALID` (rule B10), because engine content is changed by a ticket's diff, never by a game's batch. `Plan` checks the rules the specification alone decides (B1, B4, B9, B10, and the path syntax and uniqueness parts of B2, B3, B7 and B8); the rules that need the files or the server's records (B2's existence checks, B3's replacement kind, B5 … B8 against the records, B11, B12) are checked by the planning units as their inputs arrive ("Planning"). A B12 failure is `CONTENT_ERROR_SPEC_INVALID` with `m_uDetail` 12 and a message naming the limit and the value reached (`expanded targets 1048577 > 1048576`, `chunks 20000 > 16384`, `plan.json 268435457 bytes > 268435456`, `chunk 3 prepared bound 301000000 bytes > 268435456`), so an admissible plan never produces a journal its own reader refuses.

## Operation kinds

| Kind | Targets | Per chunk: file moves | Per chunk: document writes | Per chunk: cook requests |
|---|---|---|---|---|
| move | each source file (with its `.meta`) and each authoring document moved | `.meta` then source, `MoveFileExW` without replace | repairs of path references ("Generic reference repairer") and SCENE's hint rewrites | `reimport` of the moved GUIDs |
| delete | each deleted source | after the document writes: source and `.meta` into `.migration/content-ops/<opId>/deleted/` | repairs: GUID references rewritten to the replacement, or nulled, or SCENE's unpack | `import` of the deleted GUIDs (they become TOMBSTONE; the server writes the tombstone lines) |
| reimport | each matched source | none | none | `reimport` of the chunk's GUIDs |
| setPreset | each matched source | none | none | `record` (the before-state journaled), `setSettings` with `preset` per target, then `reimport` |
| setSettings | each matched source | none | none | `record`, `setSettings` with the merged settings per target, then `reimport` |
| remapSubAssets | each (source, old key, new key) | none | references to the new key's GUID rewritten to the old key's GUID | `record`, `setSettings` with `remapSubAssets` per source, then `reimport` |
| repairOrphans | each orphaned GUID | none | references rewritten to the replacement or nulled | `record`, `setSettings` with `removeOrphanedSubAssets` per source |

The `setSettings` arguments `preset`, `subAssetMembers`, `provenance`, `remapSubAssets`, `removeOrphanedSubAssets` and `restoreSubAssets`, an optional `settings`, and the `preset`, `provenance` and entry members in `record`'s result are pending ASSET (rows CT1 (b), CT22).

## Planning

`Plan(desc, spec)` performs step 1 and returns the operation in PLANNING; steps 2 … 6 are **planning units** run by `Step` or `Run` on the owner thread. A planning unit never blocks on the cook channel and writes nothing, so an operation that dies or is destroyed while PLANNING leaves no file but the operation lock's content line, which the next holder overwrites.

1. In `Plan`: validate what the specification alone decides ("Batch specification"); take the operation lock `<game root>/.migration/content-ops/active.lock` (held by holding the file open as the storage-root writer lock is, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Storage-root writer lock"`; content line `{"pid", "operation": "planning", "mode": "content-batch"}`, rewritten with the operation ID at step 6); held by another process: `CONTENT_ERROR_BATCH_BUSY` naming the holder.
2. Expand targets. Units: one directory of a move's `from` listed (a directory move becomes one target per file beneath it, files and their `.meta` paired; a `.meta` without its source moves alone and is reported); one cook request begun; one poll of the requests in flight. A filter is resolved through the channel ("Cook channel", filter resolution) with at most 32 requests in flight; B2's existence checks, B3's replacement kind and B5 … B8 are checked against the records as they arrive. Targets are put in byte order of path (filter targets: of logical path) and numbered in that order.
3. For move, delete, remapSubAssets and repairOrphans, index the references. Units: one directory of the game's content roots listed (the documents to index: every registered document type's suffix); one document read and indexed, a scene or prefab by SCENE's `Zenith_ReferenceIndex` (`Build` over an empty path span at the first such unit, then `Update(documents, path)` per document, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_ReferenceRepair header"`) and any other document by the generic index ("Generic reference repairer"). The unit records each document's byte size and, per target, its referencing documents by their plan-time paths. Progress reports `m_uDocumentsIndexed` of `m_uDocumentsToIndex`.
4. Chunk the targets (one unit), so that targets sharing referencing documents share a chunk and a document is repaired by as few chunks as the chunk size allows:
   1. Union every two targets that share a referencing document (union–find with path compression and union by size, O(R · α(T)) for T targets and R (target, document) pairs). A component's targets are kept in target order; components are ordered by their first target.
   2. Pack, walking the components in that order, with C = 16 × `m_uChunkTargets`: a component of at most `m_uChunkTargets` targets joins the current chunk while the chunk's target count stays at most `m_uChunkTargets`, else it starts a new chunk; a larger component closes the current chunk and is cut into p = ⌈n / C⌉ chunks of its own, each of ⌈n / p⌉ consecutive targets in target order (the last holds the rest).
   3. A chunk whose distinct referencing documents exceed 65,536 (the journal's per-chunk limit) is halved in target order until every part fits; one target referenced by more than 65,536 documents fails `CONTENT_ERROR_SPEC_INVALID` (rule B11).
   4. W, the operation's document-repair count, is the sum over chunks of each chunk's distinct referencing documents; W above 1,000,000 fails `CONTENT_ERROR_SPEC_INVALID` (rule B11) with a message naming W and the smallest chunk size of 2 × `m_uChunkTargets`, 4 × …, up to 1,024 whose packing gives W ≤ 1,000,000 (the batch command's `--chunk`), or saying that none does. A document is therefore repaired once per chunk holding one of its targets: a component of n targets costs each of its documents at most ⌈n / C⌉ repairs. The plan's headline case, a folder move of 5,000 assets referenced from 3,000 scenes that each reference assets across the folder, is one component: p = ⌈5,000 / 1,024⌉ = 5 chunks of 1,000 targets and W ≤ 5 × 3,000 = 15,000 repairs, where 64-target chunks in target order would give up to 79 × 3,000 = 237,000.

   The packing is one pure function, which the planning unit calls and `Test_ContentBatch_ChunkPackingGroupsSharedDocuments` drives directly:

   ```cpp
   // Zenith2/Tools/Authoring/Internal/CONTENT/Batch/Zenith_ContentBatchChunking.h (ZenithAuthoring-internal; its tests include it)
   /// @contract functions Zenith_ContentBatchChunking
   /// Ownership: Reads the caller's spans; appends to the caller's arrays.
   /// Thread affinity: ANY_THREAD.
   /// Invalidation: None.
   /// Units: Target and document indices; counts.
   /// Errors: CONTENT_ERROR_SPEC_INVALID with m_uDetail 11 (rule B11; m_uIndex the offending target for the 65,536-document
   ///   rule, else 0xFFFFFFFF) and the suggested chunk size in uSuggestedChunkTargetsOut (0: none fits);
   ///   CONTENT_ERROR_INVALID_ARGUMENT for a chunk size outside 1 … 1,024 or a pair naming an index out of range;
   ///   CONTENT_ERROR_OUT_OF_MEMORY.
   /// Version: since Z2 Stage 4. Tool-internal.
   /// @endcontract
   struct Zenith_ContentReferencePair { uint32 m_uTarget; uint32 m_uDocument; };   // target index in target order, document index
   struct Zenith_ContentPackedChunk { uint32 m_uFirstTarget; uint32 m_uTargetCount; uint32 m_uDocumentCount; };   // into auTargetOrderOut
   Zenith_Result<void, Zenith_ContentError> Zenith_PackContentBatchChunks(uint32 uTargetCount, uint32 uDocumentCount,
   	Zenith_Span<const Zenith_ContentReferencePair> axPairs, uint32 uChunkTargets, Zenith_Array<uint32>& auTargetOrderOut,
   	Zenith_Array<Zenith_ContentPackedChunk>& axChunksOut, uint64& ulDocumentRepairsOut, uint32& uSuggestedChunkTargetsOut);
   ```

   `auTargetOrderOut` lists every target index once, chunk by chunk, each chunk's targets in target order; chunk k's targets are `auTargetOrderOut[m_uFirstTarget …]`.
5. Compute the move maps (one unit per chunk): M₋₁ is the identity; M_c maps the source path (and the `.meta` path) of every target of chunks 0 … c to its destination. For each chunk c and each of its referencing documents with plan-time path p, record `readPath` = M_{c−1}(p), where the document is when chunk c starts, and `writePath` = M_c(p), where it is after chunk c's own moves; they differ exactly when chunk c moves the document itself. A document referenced by targets of several chunks is listed in each, at that chunk's paths, so a scene moved by chunk 0 and referencing a chunk-1 target is read and written at its new path by chunk 1. A chunk's documents are listed in byte order of plan-time path, which is the order its `SaveDocuments` sub-batches take them. The same unit computes the chunk's prepared-file bound (rule B12): the sum, over its documents, of the bytes of `readPath`, `writePath` and the two staged paths plus 2 × 64 hex digits and a 128-byte allowance for keys and punctuation; over its moves, of the bytes of `from` and `to` plus 128; over its cook groups, of 38 bytes per GUID plus 128; plus the 16 MiB edit allowance and the 4 MiB warning allowance of "Batch report"; plus 4 KiB.
6. Compute `planSha256` ("Operation journal"), write `plan.json` and then `state.json` in state PLANNED (one unit), and rewrite the operation lock's content line with the operation ID. Nothing else is written. A planning unit that finds a specification error returns `CONTENT_ERROR_SPEC_INVALID` with the rule index; the operation then stays PLANNING with nothing written, every further `Step` or `Run` returns the same error, and the caller destroys it, which releases the lock. A channel failure returns `CONTENT_ERROR_COOK_CHANNEL` and the next `Step` re-issues the failed request.

Plan cost: O(document bytes) for the index scan, once per operation; O(R · α(T)) for the chunking; O(targets + documents × log moves) for the move maps; O(targets + R) memory.

## Chunked execution

`Step` (and `Run`, its loop) from PLANNED, or from PAUSED, which is resume ("Resume algorithm"), sets RUNNING and executes chunks in index order. A chunk's work is divided into **units**, each of which ends with the journal describing the files exactly: acquiring the lock with the recovery of step 1; the free-space check; preparing one document; writing the chunk's prepared file and then its chunk file in PREPARED; creating the chunk's missing destination directories; 16 moves and their chunk-file rewrite; one `SaveDocuments` sub-batch and its chunk-file rewrite; recording one `.meta` before-state (its element file, then the chunk-file rewrite); sending one cook request; one poll of an in-flight request; the chunk's final chunk-file and state-file rewrite. For chunk k:

| Step | Action | Chunk state written to `chunk-<k>.json` after the step |
|---|---|---|
| 1 | acquire the game root's writer lock (`GetWriterLock()->Acquire(m_uLockWaitMs, …)`; a storage whose lock is null, `Zenith_MemoryDocumentStorage`, needs none); refused: release, state PAUSED, return `CONTENT_ERROR_SOURCE_LOCKED`; then `RecoverPendingJournals` on the game root (hold rule 2 reuses this hold) | — |
| 2 | free-space check: the free bytes (`m_pfnQueryFreeBytes`, else the game storage's `QueryFreeBytes`) must be at least 3 × D_k + 64 MiB, where D_k is the sum of the plan-time byte sizes of the chunk's documents (`plan.json` `bytes`: one before-copy, one staged new copy and the written document each), otherwise release the lock, state PAUSED, `CONTENT_ERROR_LOW_DISK`, nothing written; then, one unit per document: open it at its `readPath`, run its repairer (SCENE's planner for scenes and prefabs, the generic repairer otherwise), copy the bytes read to `before/<k>/<readPath>` and write the new canonical bytes to `after/<k>/<writePath>` (both with `WriteFlushed` of a `.z2tmp` then `ReplaceAtomically`), and keep the two SHA-256 values as `oldSha256` (the bytes read) and `newSha256`; a document the repair leaves unchanged is listed with `newSha256` equal to `oldSha256` and is not written at step 4. Memory per unit is one document and its repair; no document's bytes are held between units. After the chunk's last document: write the immutable `chunk-<k>.prepared.json` (the move list, every document's paths, digests, listed edits and warnings, the cook groups, and the destination directories that do not exist yet), then `chunk-<k>.json` recording its SHA-256 | PREPARED, written once after the prepared file |
| 3 | for move: create each directory of the prepared file's `directories[]` (shallowest first, `CreateDirectoryW`, an existing directory being success), then perform the moves in plan order, each `.meta` before its source (`MoveFileExW(from, to, MOVEFILE_WRITE_THROUGH)`, no replace; an existing destination is a conflict, step 8a), so a referencing document the chunk moves is at its `writePath` before step 4; for delete: nothing yet | MOVING (`movesDone` rewritten after every 16 moves and at the end) |
| 4 | write the changed documents at their `writePath` with `SaveDocuments` in sub-batches of at most `m_uMaxDocumentsPerSave` documents and 64 MiB, in the chunk's document order, skipping on resume every document already classified written ("Resume algorithm"), each document's bytes read from `after/<k>/<writePath>` and refused unless their SHA-256 is `newSha256` (`CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` for the chunk, nothing of the sub-batch written), each write carrying `oldSha256` as its expected digest (a document moved at step 3 still has its `oldSha256` bytes, at its new path); `DOCUMENT_ERROR_STALE_PLAN` is a conflict (step 8b), `DOCUMENT_ERROR_LOW_DISK` pauses with `CONTENT_ERROR_LOW_DISK`; for delete, then move the source and `.meta` into `deleted/<game-relative path>` | SAVING (`savedSubBatches` rewritten after each sub-batch) |
| 5 | release the writer lock (the cook server takes the same lock for its sidecar writes) | COOK_PENDING |
| 6 | for each target whose kind changes its `.meta` through the server (setPreset, setSettings, remapSubAssets, repairOrphans), in the chunk's target order, before its first such request: send `record`, write the element file `meta/<k>/<n>.json` with the result's `metaSha256` as `beforeSha256` and the `.meta` members the request will change (`preset`, the settings keys the request sets with their current values or absence, the touched `subAssets` entries with every member, `provenance`) as `before` ("Operation journal"), then increment `metaRecorded` | COOK_PENDING (`metaRecorded` rewritten per target) |
| 7 | issue the kind's cook requests through the channel, asynchronously, in the prepared file's group order (at most 256 GUIDs each), with at most one request in flight per operation; each `setSettings` carries `beforeSha256` as `expectedMetaSha256`; when a request completes, journal its result (`metaDone` or `cookDone` incremented, per-GUID failures appended to `cookFailed`); a channel failure pauses with `CONTENT_ERROR_COOK_CHANNEL` (the chunk stays COOK_PENDING) | DONE (after the last request); then `state.json`'s `firstOpenChunk` becomes k + 1 |
| 8a | on a conflict at step 3 (an existing destination, or a move whose source is gone): undo this chunk's completed moves (`to` → `from`, in reverse plan order), remove each directory of `directories[]` that is empty after the undo (deepest first), write nothing further, release the lock, set the chunk CONFLICT and the operation PAUSED, return `CONTENT_ERROR_BATCH_CONFLICT` with the chunk index and the paths in the report. No document has been written yet, so the tree is exactly the tree before the chunk | CONFLICT |
| 8b | on a conflict at step 4 (`DOCUMENT_ERROR_STALE_PLAN`: a writer that ignores the storage-root lock, such as git or an external editor, changed a document of the failing sub-batch, which the framework therefore wrote nothing of): keep every completed move and every sub-batch already written, release the lock, set the chunk CONFLICT (with `savedSubBatches` as journaled) and the operation PAUSED, and return `CONTENT_ERROR_BATCH_CONFLICT` naming each stale document and its current digest. The person either restores the stale document to its recorded `oldSha256` bytes and runs `Step` or `Run`, which re-classifies and writes the remaining sub-batches ("Resume algorithm"), or calls `Abandon`, which restores the written documents and then undoes the moves ("Abandon and rollback"); either way every document is exactly old or exactly new | CONFLICT |

Every operation's work is bounded by its plan: W document repairs (each a read, a before-copy, a staged write and at most one framework write), 2 × the moved files' count of moves, one prepared file per chunk, one element file per `.meta` request, and a chunk-file rewrite per unit whose size is a constant plus the chunk's per-GUID cook failures, never O(the chunk) or O(the operation) ("Operation journal").

### Time budget

`Step(uMaxMicroseconds)` reads the desc's monotonic clock before each unit and runs the unit only when the elapsed time plus the unit's estimate stays within the budget. A unit's estimate is the longest duration observed for that unit kind in this operation; a kind not yet observed has no estimate and runs only as the first unit of a call, so it is measured without overrunning a call that has already spent part of its budget. A unit is never split. When no unit fits and none has run this call, the call runs exactly one, so every call makes progress unless a request is in flight. A unit that sends a request returns at once; a poll unit that finds the request pending ends the call with `m_bWaitingOnCook` true. Each unit is bounded by count and bytes, not by time: one directory listed, one document indexed or prepared, one request begun or polled, 16 moves, one `SaveDocuments` sub-batch (at most `m_uMaxDocumentsPerSave` documents and 64 MiB), one chunk-file rewrite; the two unbounded-looking units are planning's chunking (O(R · α(T)), at most 1,048,576 targets) and the `plan.json` write (at most 256 MiB).

`Step` serves a tool that pumps its own main thread. The editor does not: EDIT runs `Plan` and both `Run` calls on its `EditorContentBatch` thread (`Zenith2/Docs/Design/EDIT.md → "Threads EDIT owns"`, row CT30), where a long unit delays nothing the person sees, so CONTENT defines no editor-specific budget. `Test_ContentBatch_StepNeverExceedsBudget` drives the rule with the scripted clock: the test's progress callback advances it by c = 700 µs per call (one call per unit), so once each unit kind has been measured every `Step(4000)` runs at most ⌊4,000 / 700⌋ = 5 units and its elapsed time is at most 4,000 µs; a call whose next unit's kind is unmeasured ends before it (fewer units), the next call starts with it, and a call that runs exactly one unit is the only one allowed to exceed the budget.

After each unit the progress callback runs; a false return pauses the operation at the next chunk boundary (after step 7 of the current chunk, or before step 1 of the next) with `CONTENT_ERROR_BATCH_CANCELLED`. When every chunk is DONE the operation becomes COMPLETED, the report is written and the operation lock released. Before the COMPLETED write, a move operation removes every source directory that held a moved file and is now empty (deepest first, `RemoveDirectoryW`; a directory holding anything is kept, and a directory that was already empty before the operation held no moved file and is never touched); the report lists them in `removedDirectories`. A crash during the removal leaves directories that the next `Run` from RUNNING removes the same way, because removal is idempotent.

- Every chunk is all-or-nothing from the user's view: between steps 3 and 7 the tree is transiently inconsistent, and `Resume` completes or `Abandon` reverts that one chunk.
- Only the game root is written; engine roots are read.
- The operation lock is held from `Plan` or `Open` to the operation object's destruction, so two batches never run over one game root (`Test_ContentBatch_SecondOperationRefusedBusy`).
- The before-copies and staged new bytes are the operation's own, not the framework's save backups, whose retention (20 save batches) a large operation would exceed; they are kept until `Discard`. Each chunk has its own `before/<k>/` and `after/<k>/`, so a document that several chunks repair keeps one before-copy per chunk, each holding the bytes that chunk read.

### Fault-point consultation order

Each fault point is consulted through `Zenith_FaultInjector::ShouldFail` exactly where the "Batch API" constants say, in this fixed order, so a test can count hits: `JOURNAL_WRITTEN` immediately after each journal file write (plan, state, prepared, element or chunk file), except in a PREPARED unit and a DONE unit, which consult their own point (`PREPARED` after the chunk file's PREPARED write; `DONE` after the final chunk-file and `state.json` writes) first and then `JOURNAL_WRITTEN` once per write of the unit, in write order; `MOVE` after each individual move, before the next; `SAVE` after each `SaveDocuments` sub-batch returns success, before its chunk-file rewrite; `COOK` when a poll returns DONE, before the request's result is journaled; `META_RECORDED` after an element file is written and before its request is sent; `META_APPLIED` when a `.meta` request's poll returns DONE, before its result is journaled. A point that fires ends the call, so a point the same unit would consult after it is not reached and not counted. A resume never re-hits `PREPARED`, `MOVE`, `SAVE` or `DONE` for work it classifies as done; it re-hits `COOK` (and `META_APPLIED`) exactly once for a request whose result was not journaled, because it re-issues that request.

### Stress case

`Test_ContentBatch_TenThousandTargetsWithRandomFaults` (`z2-stress`, 600 s, [Z2-N-CONTRACT-409]) builds, with the default chunk size 64 (C = 1,024), 10,000 target documents `Assets/Stress/T<i>.stresstarget.json` (i = 00000 … 09999, each 256 bytes with a `.meta`, moved by one directory move) and 2,000 referencing documents `Scenes/S<j>.stressref.json` (j = 0 … 1,999, each about 4 KiB), where document j references targets 5j … 5j + 4 and target (5j + 7) mod 10,000 by path. The two document types are the test's own, registered in the test's migration registry (each version 1 with no step, a canonical schema of one OBJECT root holding a `refs` array of path strings): `Zenith.Test.StressTarget` with suffix `.stresstarget.json` and `Zenith.Test.StressReferrer` with suffix `.stressref.json`. Neither is a scene or prefab type, so planning step 3 indexes both suffixes through the generic index and the generic repairer rewrites the `refs` strings ("Generic reference repairer"). Derived by hand from "Planning" step 4: document j links the five targets of group j to group j + 1 (and document 1,999 links the last group to the first), so the targets form one component of 10,000; p = ⌈10,000 / 1,024⌉ = 10 chunks of 1,000 consecutive targets; chunk k holds the 200 documents 200k … 200k + 199 and the one document whose extra reference enters it from the previous chunk (200k − 1, and 1,999 for chunk 0), so W = 10 × 201 = 2,010 and the plan has 20,000 moves. The test asserts those counts from `plan.json`, then runs the operation once with its own `Zenith_FaultInjector`, which counts each point's hits over the whole interrupted run, resumes included, and fires at a hit whose number is in that point's position set; the positions follow from "Fault-point consultation order" and the uninterrupted run's hits, derived by hand:

| Point | Hits in an uninterrupted run | Positions fired | Interruptions |
|---|---|---|---|
| `JOURNAL_WRITTEN` | 1,354: per chunk 135 (2 in the PREPARED unit, 2,000 / 16 = 125 move rewrites, 1 save rewrite, 1 COOK_PENDING write, 4 cook results for the groups of 256, 256, 256 and 232 GUIDs, 2 in the DONE unit), plus `plan.json`, `state.json` at PLANNED, RUNNING and COMPLETED | 100 distinct seeded positions in 1 … 1,200 | 100 |
| `MOVE` | 20,000 | 100 distinct seeded positions in 1 … 20,000 | 100 |
| `PREPARED` | 10, one per chunk | every hit, 1 … 10 | 10 |
| `SAVE` | 10, one sub-batch per chunk (201 documents ≤ 256) | every hit, 1 … 10 | 10 |
| `COOK` | 40, four requests per chunk | the odd hits 1, 3, …, 79: each fired request's result is unjournaled, so the resume re-issues it and its second hit (the next, even number) passes; the interrupted run makes 80 hits and interrupts each of the 40 requests once | 40 |
| `DONE` | 10 | every hit, 1 … 10 | 10 |

That is 270 interruptions, each followed by `Open` and `Run` on a fresh operation object. Every position is reachable: `MOVE`, `PREPARED`, `SAVE` and `DONE` are never re-hit by a resume, so their counts equal the uninterrupted run's; the interrupted run makes at least 1,354 − 40 − 10 = 1,304 `JOURNAL_WRITTEN` hits, because the 10 `PREPARED` and 10 `DONE` interruptions pre-empt the 2 consultations of their units each and each of the 10 `SAVE` interruptions pre-empts its unit's rewrite, while every other interruption only adds hits; the positions are therefore drawn below 1,200. The injector fires in ascending hit order per point, and the test asserts it fired exactly 270 times (the table's last column) and that the final tree and `.meta` digests equal an uninterrupted run's byte for byte. Its time on the CI host's SSD, at 2 ms per flushed write and 0.5 ms per write-through move: one run is 20,000 moves (10 s), 2,010 repairs of 4 flushed writes each (16 s), 1,354 journal writes (3 s) and the planning index of 12,000 documents (4 s), about 33 s; two runs (the interrupted one and the reference) are 66 s; each resume reads `plan.json` (1.5 MB), one prepared file (at most 0.5 MB) and one chunk file, recovers the framework's journals and re-classifies at most one chunk (its 2,000 moves checked for presence, its 201 documents hashed), at most 0.1 s, so 270 resumes add 27 s; the total of about 93 s leaves a margin above 6 under the 600-second `z2-stress` timeout. A run writes about 8 MB of journal: 1,354 chunk and state rewrites of at most 1 KiB each (1.4 MB), ten prepared files of about 0.5 MB each (201 documents with paths, digests and edits, 2,000 moves and 1,000 cook GUIDs), and `plan.json` once (1.5 MB).

## Operation journal

The journal is the directory `<game root>/.migration/content-ops/<opId>/`, where `<opId>` is `<m_szTimestamp>-<first 8 hex digits of planSha256>`. `.migration/` is excluded from source control (`Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`, [Z2-N-CONTRACT-541]), so an operation is local to the machine that ran it; the tree it leaves is consistent at every chunk boundary without it.

| File | Written | Content |
|---|---|---|
| `plan.json` | once, by planning step 6; never rewritten | the immutable plan: specification, targets, chunk membership, each chunk's documents with their plan-time paths and sizes, `planSha256` |
| `state.json` | at every operation state change and whenever a chunk becomes DONE or REVERTED | the operation state and the first chunk that is not DONE (`firstOpenChunk`) or, rolling back, the last chunk not yet REVERTED (`lastUnrevertedChunk`) |
| `chunk-<k>.prepared.json` | once, at the end of chunk k's step 2, before `chunk-<k>.json`; never rewritten | the chunk's immutable prepared state: the moves in plan order, every document's paths, digests, staged paths and listed edits, the warnings its repairs reported, the cook groups, and the destination directories that did not exist at PREPARED |
| `chunk-<k>.json` | from chunk k's PREPARED on, after every unit of chunk k (and of its revert) | the chunk's mutable state: the state, the prepared file's digest, six progress counts, the per-GUID cook failures, error |
| `meta/<k>/<n>.json` | once per `.meta` request target n of chunk k, at step 6, before its request is sent; never rewritten | one `meta` element: the `.meta`'s before-state ("Meta element classification") |
| `before/<k>/<readPath>` | at chunk k's step 2 | the bytes chunk k read, one copy per chunk per document |
| `after/<k>/<writePath>` | at chunk k's step 2 | the new bytes chunk k writes |
| `deleted/<game-relative path>` | at a delete chunk's step 4 | the deleted source and `.meta` |
| `report.json` | at every pause and at the end | "Batch report" |

Every journal file is canonical JSON written through the game storage's `WriteFlushed` of `<name>.z2tmp` then `ReplaceAtomically`, so a reader sees the previous or the new file, never a mix. Everything that grows with the chunk is written once, in the immutable prepared and element files; the file a unit rewrites, `chunk-<k>.json`, holds counts in place of per-move and per-document flags (moves and sub-batches run in plan order, and resume classifies them from the files, never from flags), so a rewrite is a constant number of bytes plus the chunk's per-GUID cook failures. The operation's total journal bytes are therefore O(W + targets + moves) for `plan.json` and the prepared and element files, plus a constant per unit; `plan.json` is written once. A chunk with no `chunk-<k>.json` is PENDING (a prepared file without its chunk file is an interruption inside the PREPARED unit, and step 2 rewrites both); a `plan.json` without a `state.json` (an interruption between the two writes of planning step 6) is PLANNED.

`plan.json` (`$schema` `zenith2.content-batch-plan`, version 1):

```json
{
	"$schema": "zenith2.content-batch-plan",
	"schemaVersion": 1,
	"operation": "20271014T101500Z-3fa9c21e",
	"kind": "move",
	"game": "OpenWorldSlice",
	"spec": { "kind": "move", "moves": [ { "from": "Assets/Props/Old", "to": "Assets/Props/Crates" } ], "pruneDangling": false },
	"chunkTargets": 64,
	"documentRepairs": 2,
	"planSha256": "3fa9c21e…",
	"chunks": [
		{
			"index": 0,
			"targets": [ { "guid": "0b6e…", "from": "Assets/Props/Old/Crate.glb", "to": "Assets/Props/Crates/Crate.glb" } ],
			"documents": [
				{ "readPath": "Scenes/Harbour.scene.json", "writePath": "Scenes/Harbour.scene.json", "bytes": 48211 },
				{ "readPath": "Assets/Props/Old/Crate.prefab.json", "writePath": "Assets/Props/Crates/Crate.prefab.json", "bytes": 1904 }
			]
		}
	]
}
```

`state.json` (`$schema` `zenith2.content-batch-state`, version 1): `{ "$schema", "schemaVersion": 1, "operation", "planSha256", "state": "RUNNING", "firstOpenChunk": 0, "lastUnrevertedChunk": null }`.

`chunk-<k>.prepared.json` (`$schema` `zenith2.content-batch-chunk-prepared`, version 1):

```json
{
	"$schema": "zenith2.content-batch-chunk-prepared",
	"schemaVersion": 1,
	"operation": "20271014T101500Z-3fa9c21e",
	"index": 0,
	"moves": [ { "from": "Assets/Props/Old/Crate.glb.meta", "to": "Assets/Props/Crates/Crate.glb.meta" }, { "from": "Assets/Props/Old/Crate.glb", "to": "Assets/Props/Crates/Crate.glb" } ],
	"directories": [ "Assets/Props/Crates" ],
	"documents": [
		{ "readPath": "Assets/Props/Old/Crate.prefab.json", "writePath": "Assets/Props/Crates/Crate.prefab.json", "oldSha256": "4d…", "newSha256": "7f…", "before": "before/0/Assets/Props/Old/Crate.prefab.json", "after": "after/0/Assets/Props/Crates/Crate.prefab.json", "edits": [], "editsNotListed": 0 },
		{ "readPath": "Scenes/Harbour.scene.json", "writePath": "Scenes/Harbour.scene.json", "oldSha256": "9c…", "newSha256": "a1…", "before": "before/0/Scenes/Harbour.scene.json", "after": "after/0/Scenes/Harbour.scene.json", "edits": [ { "pointer": "/instances/8a1e…/hint", "from": "Assets/Props/Old/Crate.prefab.json", "to": "Assets/Props/Crates/Crate.prefab.json" } ], "editsNotListed": 0 }
	],
	"warnings": [ { "code": "DOCUMENT_WARNING_DANGLING_OVERRIDE", "path": "Scenes/Harbour.scene.json", "pointer": "/instances/…/overrides/properties/…" } ],
	"warningsNotListed": 0,
	"cook": [ { "request": "reimport", "guids": [ "0b6e…" ] } ]
}
```

`chunk-<k>.json` (`$schema` `zenith2.content-batch-chunk`, version 1):

```json
{
	"$schema": "zenith2.content-batch-chunk",
	"schemaVersion": 1,
	"operation": "20271014T101500Z-3fa9c21e",
	"index": 0,
	"state": "MOVING",
	"preparedSha256": "e0…",
	"movesDone": 1,
	"savedSubBatches": 0,
	"metaRecorded": 0,
	"metaDone": 0,
	"metaReverted": 0,
	"cookDone": 0,
	"cookFailed": [],
	"error": null
}
```

A `setPreset` chunk's element file `meta/0/0.json` (`$schema` `zenith2.content-batch-meta`, version 1): `{ "$schema", "schemaVersion": 1, "operation", "index": 0, "element": 0, "guid": "5e2a…", "metaPath": "Assets/Rock.png.meta", "request": "setSettings", "args": { "preset": "4c1d…" }, "beforeSha256": "b3…", "before": { "preset": null } }`.

| File and key | Rule |
|---|---|
| every file: `operation` | equal to the directory's name |
| `plan.json`: `kind`, `game`, `spec` | as named; the specification canonical and verbatim |
| `plan.json`: `chunkTargets`, `documentRepairs` | the desc's chunk size; W of "Planning" step 4 |
| `plan.json`: `planSha256` | 64 hex: SHA-256 of the canonical `spec` bytes followed, for each chunk in index order, by the line `chunk <k>` and LF, then one line `<from>` TAB `<to>` LF per target and one line `<readPath>` TAB `<writePath>` LF per document, in the plan's order (`Zenith_ComputeContentBatchPlanSha256`); `Open` recomputes it, and a mismatch, or a `state.json` or chunk file naming another digest or operation, is `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` |
| `plan.json`: `chunks[]` | in index order; every target in exactly one chunk; `documents[].bytes` the plan-time size |
| `state.json`: `state` | a `Zenith_ContentBatchState` name other than PLANNING |
| `state.json`: `firstOpenChunk`, `lastUnrevertedChunk` | chunk indexes or null |
| `chunk-<k>.prepared.json`: `moves[]` | every move of the plan's chunk k (`.meta` then source per target), in plan order |
| `chunk-<k>.prepared.json`: `directories[]` | the destination parent directories, game-relative, that did not exist at PREPARED, shallowest first; created at step 3, removed when empty by step 8a, `Abandon` and `Rollback` |
| `chunk-<k>.prepared.json`: `documents[]` | one per document of the plan's chunk k, in the same order and at the same paths; `oldSha256`, `newSha256` 64 hex, `newSha256` null for a document the chunk deletes; `before` and `after` the staged files' journal-relative paths, `before/<k>/` and `after/<k>/` prefixed; `edits` the repair's first edits (the listing rule of "Batch report"), `editsNotListed` the rest |
| `chunk-<k>.prepared.json`: `warnings[]`, `warningsNotListed` | the chunk's repair warnings in document order, listed while their text stays within 4 MiB, the rest counted |
| `chunk-<k>.prepared.json`: `cook[]` | the kind's cook groups in issue order, at most 256 GUIDs each |
| `chunk-<k>.json`: `state` | a `Zenith_ContentChunkState` name other than PENDING |
| `chunk-<k>.json`: `preparedSha256` | 64 hex: SHA-256 of the prepared file's bytes; a mismatch is `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` |
| `chunk-<k>.json`: `movesDone`, `savedSubBatches` | moves performed in plan order (at most the prepared file's count); `SaveDocuments` sub-batches written in document order; progress records only, since resume classifies from the files |
| `chunk-<k>.json`: `metaRecorded`, `metaDone`, `metaReverted` | element files written (elements 0 … `metaRecorded` − 1 exist); elements whose request result is journaled; elements reverted, counted from the last downwards; `metaReverted` ≤ `metaDone` ≤ `metaRecorded` |
| `chunk-<k>.json`: `cookDone`, `cookFailed[]` | cook groups whose result is journaled; `{group, guid, code}` per GUID a journaled result failed (a `COOK_ERROR_…` name) |
| `meta/<k>/<n>.json` | one element per target whose `.meta` the chunk changes through the server, numbered n = 0, 1, … in the chunk's target order: `index` k, `element` n, `guid`, `metaPath`, `request` (`setSettings`), `args` (the request's arguments, canonical), `beforeSha256` (64 hex), `before` (the changed members' values before the request: `preset` GUID or null, `settings` holding exactly the keys the request sets, each with its value or JSON `null` for an absent key, `subAssets` the touched entries with every member, `provenance` object or null) |
| `chunk-<k>.json`: `error` | null, or `{code, detail, message}` of the failure that paused the chunk |

Limits: `plan.json` at most 256 MiB, 16,384 chunks and 1,048,576 targets; a prepared file at most 256 MiB, 65,536 documents, 32,768 moves and 65,536 directories; a chunk file at most 64 MiB and 65,536 `cookFailed` entries; an element file at most 1 MiB; `state.json` at most 64 KiB; any rule violation is `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT`. Planning rule B12 keeps every file an admissible plan produces inside these limits, and each `Zenith_Write…` function refuses, with the same error and detail, every value its reader would refuse, so a writer never produces a file its reader rejects. The codec is `ZenithContentCore`'s, fuzzed by `Zenith_Fuzz_BatchJournal` over all five readers:

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentValidation.h
/// @contract value Zenith_ContentBatchJournalValues
/// Ownership: Plain values; every view and span points into the caller's arena (read) or is borrowed for the call (write).
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: With the arena.
/// Units: Paths game-relative; digests 32-byte SHA-256 values; byte sizes; counts.
/// Errors: None of their own.
/// Version: since Z2 Stage 4. Tool-side; the schemas of "Operation journal".
/// @endcontract
struct Zenith_ContentJournalTarget { const Zenith_PersistentID m_xGUID = {}; const Zenith_StringView m_strFrom = {}, m_strTo = {}; };
struct Zenith_ContentPlanDocument { const Zenith_StringView m_strReadPath = {}, m_strWritePath = {}; const uint64 m_ulBytes = 0; };
struct Zenith_ContentPlanChunk
{
	const uint32 m_uIndex = 0;
	const Zenith_Span<const Zenith_ContentJournalTarget> m_axTargets = {};
	const Zenith_Span<const Zenith_ContentPlanDocument> m_axDocuments = {};
};
struct Zenith_ContentBatchPlanView
{
	const Zenith_StringView m_strOperation = {}, m_strGame = {};
	const Zenith_ContentBatchKind m_eKind = {};
	const Zenith_JsonNode m_xSpec = {};
	const uint32 m_uChunkTargets = 0;
	const uint64 m_ulDocumentRepairs = 0;
	const Zenith_Sha256Value m_xPlanSha256 = {};
	const Zenith_Span<const Zenith_ContentPlanChunk> m_axChunks = {};
};
struct Zenith_ContentBatchStateView
{
	const Zenith_StringView m_strOperation = {};
	const Zenith_Sha256Value m_xPlanSha256 = {};
	const Zenith_ContentBatchState m_eState = {};
	const bool m_bHasFirstOpenChunk = false, m_bHasLastUnrevertedChunk = false;
	const uint32 m_uFirstOpenChunk = 0, m_uLastUnrevertedChunk = 0;
};
struct Zenith_ContentJournalMove { const Zenith_StringView m_strFrom = {}, m_strTo = {}; };
struct Zenith_ContentJournalEdit { const Zenith_StringView m_strPointer = {}, m_strFrom = {}, m_strTo = {}; };
struct Zenith_ContentJournalWarning { const Zenith_StringView m_strCode = {}, m_strPath = {}, m_strPointer = {}; };
struct Zenith_ContentJournalDocument
{
	const Zenith_StringView m_strReadPath = {}, m_strWritePath = {}, m_strBefore = {}, m_strAfter = {};
	const Zenith_Sha256Value m_xOldSha256 = {}, m_xNewSha256 = {};
	const bool m_bDeleted = false;                    // newSha256 null
	const Zenith_Span<const Zenith_ContentJournalEdit> m_axEdits = {};
	const uint32 m_uEditsNotListed = 0;
};
struct Zenith_ContentJournalCookGroup
{
	const Zenith_StringView m_strRequest = {};        // "import", "reimport", "setSettings"
	const Zenith_Span<const Zenith_PersistentID> m_axGUIDs = {};
};
struct Zenith_ContentJournalPrepared
{
	const Zenith_StringView m_strOperation = {};
	const uint32 m_uIndex = 0;
	const Zenith_Span<const Zenith_ContentJournalMove> m_axMoves = {};
	const Zenith_Span<const Zenith_StringView> m_astrDirectories = {};
	const Zenith_Span<const Zenith_ContentJournalDocument> m_axDocuments = {};
	const Zenith_Span<const Zenith_ContentJournalWarning> m_axWarnings = {};
	const uint32 m_uWarningsNotListed = 0;
	const Zenith_Span<const Zenith_ContentJournalCookGroup> m_axCook = {};
};
struct Zenith_ContentJournalMeta
{
	const Zenith_StringView m_strOperation = {};
	const uint32 m_uIndex = 0, m_uElement = 0;
	const Zenith_PersistentID m_xGUID = {};
	const Zenith_StringView m_strMetaPath = {}, m_strRequest = {};
	const Zenith_JsonNode m_xArgs = {}, m_xBefore = {}; // canonical subtrees
	const Zenith_Sha256Value m_xBeforeSha256 = {};
};
struct Zenith_ContentJournalFailure { const uint32 m_uGroup = 0; const Zenith_PersistentID m_xGUID = {}; const Zenith_StringView m_strCode = {}; };   // a COOK_ERROR_… name
struct Zenith_ContentJournalChunk
{
	const Zenith_StringView m_strOperation = {};
	const uint32 m_uIndex = 0;
	const Zenith_ContentChunkState m_eState = {};
	const Zenith_Sha256Value m_xPreparedSha256 = {};
	const uint32 m_uMovesDone = 0, m_uSavedSubBatches = 0, m_uMetaRecorded = 0, m_uMetaDone = 0, m_uMetaReverted = 0, m_uCookDone = 0;
	const Zenith_Span<const Zenith_ContentJournalFailure> m_axCookFailed = {};
	const bool m_bHasError = false;
	const Zenith_ContentError m_xError = {};
	const Zenith_StringView m_strErrorMessage = {};
};

/// @contract functions Zenith_ContentBatchJournalCodec
/// Ownership: Read parses into the caller's arena; Write appends canonical bytes to the caller's array.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: CONTENT_ERROR_BATCH_JOURNAL_CORRUPT (m_uDetail the failing rule's row index of the key table, 0xFFFF for the
///   planSha256 recomputation, 0xFFFE for a limit) for any input that is not a file of its schema; CONTENT_ERROR_OUT_OF_MEMORY.
///   Read never asserts on input bytes ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 4. Tool-side; for each of the five schemas, Write(Read(b)) is b for every canonical file b, and Write
///   refuses (with the reader's error and detail) every value Read would refuse.
/// @endcontract
Zenith_Result<Zenith_ContentBatchPlanView, Zenith_ContentError> Zenith_ReadContentBatchPlan(Zenith_ByteSpan xBytes,
	Zenith_Arena& xArena);   // also recomputes and checks planSha256
Zenith_Result<void, Zenith_ContentError> Zenith_WriteContentBatchPlan(const Zenith_ContentBatchPlanView& xPlan,
	Zenith_Array<uint8>& auOut);
Zenith_Result<Zenith_ContentBatchStateView, Zenith_ContentError> Zenith_ReadContentBatchState(Zenith_ByteSpan xBytes,
	Zenith_Arena& xArena);
Zenith_Result<void, Zenith_ContentError> Zenith_WriteContentBatchState(const Zenith_ContentBatchStateView& xState,
	Zenith_Array<uint8>& auOut);
Zenith_Result<Zenith_ContentJournalChunk, Zenith_ContentError> Zenith_ReadContentBatchChunk(Zenith_ByteSpan xBytes,
	Zenith_Arena& xArena);
Zenith_Result<void, Zenith_ContentError> Zenith_WriteContentBatchChunk(const Zenith_ContentJournalChunk& xChunk,
	Zenith_Array<uint8>& auOut);
Zenith_Result<Zenith_ContentJournalPrepared, Zenith_ContentError> Zenith_ReadContentBatchPrepared(Zenith_ByteSpan xBytes,
	Zenith_Arena& xArena);
Zenith_Result<void, Zenith_ContentError> Zenith_WriteContentBatchPrepared(const Zenith_ContentJournalPrepared& xPrepared,
	Zenith_Array<uint8>& auOut);
Zenith_Result<Zenith_ContentJournalMeta, Zenith_ContentError> Zenith_ReadContentBatchMeta(Zenith_ByteSpan xBytes,
	Zenith_Arena& xArena);
Zenith_Result<void, Zenith_ContentError> Zenith_WriteContentBatchMeta(const Zenith_ContentJournalMeta& xMeta,
	Zenith_Array<uint8>& auOut);
// The digest of the planSha256 rule above, over the plan's spec, targets and documents.
Zenith_Sha256Value Zenith_ComputeContentBatchPlanSha256(const Zenith_ContentBatchPlanView& xPlan);
```

`Open` reads `plan.json` (checking `planSha256`), `state.json`, and the chunk files, their prepared files (checking `preparedSha256`) and their element files from `firstOpenChunk` on (or, rolling back, down from `lastUnrevertedChunk`); it never reads a DONE chunk's files again unless a rollback or the report reaches it.

## Resume algorithm

`Open` then `Step` or `Run` on a PAUSED operation, or on a RUNNING one whose process died (a RUNNING `state.json` found with the operation lock free belongs to an exited process):

1. `RecoverPendingJournals` on the game root under its writer lock first, so the framework's own batch is fully old or fully new.
2. Skip DONE chunks (from `firstOpenChunk`). For the first chunk not DONE, classify from the files and the server's records, never from the journal's step flags alone:
   - each move: `from` present and `to` absent → not done; `to` present and `from` absent → done; both present or both absent → conflict;
   - each document: its `writePath` holds `newSha256` (or is absent where `newSha256` is null) → written; its `writePath` holds `oldSha256` → not written (for a document the chunk moves, the move is done and the write is not); `readPath` ≠ `writePath`, `writePath` absent and `readPath` holding `oldSha256` → not written and not moved (the move's own classification decides the move); otherwise → conflict;
   - each staged file the remaining steps need: `after/<k>/<writePath>` must hold `newSha256` and `before/<k>/<readPath>` `oldSha256`, otherwise `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` for the chunk, nothing written;
   - each `meta` element n with `metaDone` ≤ n < `metaRecorded`: send `record` and classify it by "Meta element classification": BEFORE → not applied (re-issued); TARGET → applied (`metaDone` becomes n + 1); OTHER → conflict, so a request is never re-issued over a state it already produced (`Test_ContentBatch_RemapResumeAfterServerWrite`); elements are classified in order, and at most the first of them can have been applied, because at most one request is in flight;
   - each cook group g ≥ `cookDone` → re-issued (`reimport` and `import` are idempotent).
3. A conflict sets the chunk CONFLICT and the operation PAUSED with `CONTENT_ERROR_BATCH_CONFLICT`, naming every conflicting path and its current digest; nothing is written. The person resolves the files (making each conflicting document hold its recorded `oldSha256` or `newSha256` bytes again, and each conflicting move's files one side or the other), then runs `Step` or `Run` again (which re-classifies) or `Abandon`.
4. Otherwise continue the chunk from its first incomplete step of "Chunked execution", with the recorded edits (the prepared file, the staged before and after files and the element files are authoritative; edits are recomputed only for a chunk that has no chunk file yet, whose partly staged files step 2 rewrites). A chunk paused by step 8b continues at step 4: the sub-batches are formed again, in document order, from the documents classified not written, so every document written before the conflict is skipped and the rest are written ("Chunked execution" step 4).

`Test_ContentBatch_InterruptedAtEachStepResumes` fires every fault point once for a 3-chunk move, a 2-chunk delete, a remap and a setSettings batch, resumes each, and asserts the files and `.meta` digests equal an uninterrupted run's, and that the final report's counts, `documents` (edits included) and `warnings` equal the uninterrupted run's.

### Meta element classification

One rule classifies a `meta` element against the server's current `record` of its `.meta`, for resume, `Abandon` and `Rollback` alike. Let B be the element's `before` values, T the target state (B with `args` applied: `preset` → the requested preset; `settings` → each key of `args.settings` holding its requested value; `provenance` → the requested object or absence; `subAssetMembers` → each named entry holding the requested owner members; `remapSubAssets` {from: old, to: new} → the entry `new` holding the old key's GUID and owner members and the entry `old` absent; `removeOrphanedSubAssets` [key] → the entry absent), and C the record's current values of exactly the members B lists. Equality is equality of canonical JSON bytes, member by member; an absent member equals only an absent member (B's JSON `null` for a settings key means absent).

| C equals | Class | Resume | Abandon and rollback |
|---|---|---|---|
| B | BEFORE | the request was not applied: re-issue it | nothing to restore: the element is reverted (`metaReverted` + 1) |
| T (and not B) | TARGET | the request was applied: journal it (`metaDone` + 1) | restore: `setSettings` with `expectedMetaSha256` the record's digest and the B values (`preset`, the `settings` keys of B, `provenance`, and for touched entries `restoreSubAssets`, the recorded entries with their GUIDs and members, pending ASSET, row CT22); then the element is reverted (`metaReverted` + 1) |
| anything else | OTHER | conflict | conflict: the operation stays ROLLING_BACK (or PAUSED, for `Abandon`), nothing is sent, and the element is listed in the report with its key path, its B, T and current values |

A `COOK_ERROR_META_CHANGED` answer to the restore (the `.meta` changed between the `record` and the `setSettings`) is the OTHER outcome. A person's subsequent edit of the same setting, preset, provenance or entry is therefore never overwritten: it makes C differ from T (`Test_ContentBatch_RollbackRefusesLaterMetaEdit`). An edit of a member B does not list changes nothing the rule compares, and the restore's `setSettings` writes only B's members, so it survives too.

## Abandon and rollback

A chunk being reverted is reverted in three ordered passes, after one check; a document the chunk listed unchanged (`newSha256` equal to `oldSha256`) needs nothing in any pass:

0. **Digest check.** Before anything of the chunk is written, the SHA-256 of each before-copy `before/<k>/<readPath>` that pass 1 will use is checked against the entry's `oldSha256`; a mismatch is `CONTENT_ERROR_BATCH_JOURNAL_CORRUPT` for that chunk, and nothing of the chunk is written, so a damaged copy can never be written over a document.
1. **Restore the documents.** Every document whose `writePath` holds `newSha256` is written from its before-copy at `writePath`, with `newSha256` as the expected digest; a document the chunk deleted (`newSha256` null, `writePath` absent) and the delete's source and `.meta` are restored from `deleted/`. A document holding anything else is left for pass 3.
2. **Undo the moves.** Each move of the prepared file, in reverse plan order, is classified as resume classifies it: `to` present and `from` absent → undone (`to` → `from`, after creating `from`'s missing parent directories, an existing directory being success); `from` present and `to` absent → never done, nothing to undo; both or neither present → conflict. This carries every moved document back to its `readPath`. Each directory of the prepared file's `directories[]` that is empty afterwards is removed, deepest first.
3. **Verify.** Every document the chunk moves (`readPath` ≠ `writePath`) must now hold `oldSha256` at its `readPath`, and every other document `oldSha256` at its `writePath`; resume's third row (`writePath` absent and `readPath` holding `oldSha256`: written and moved by nothing) passes here. Anything else is a conflict.

The passes are idempotent: run again after a conflict is resolved, pass 1 finds nothing holding `newSha256`, pass 2 nothing to undo, and pass 3 re-checks, so a partial revert is completed by a second call.

- `Abandon` (PLANNED or PAUSED): revert the incomplete chunk if it has a chunk file, by the passes above for its documents, moves and deleted files and by "Meta element classification" for every element below `metaRecorded` (from the last down), then set the chunk REVERTED and the operation ABANDONED, keeping every DONE chunk. A conflicting file or `.meta` in the incomplete chunk is left as it is and listed; the operation then stays PAUSED. Abandoning a chunk paused by step 8a, whose moves were already undone, therefore finds every moved document at its `readPath` holding `oldSha256` and succeeds; abandoning one paused by step 8b restores the documents its earlier sub-batches wrote and then undoes its moves.
- `Rollback` (PAUSED, COMPLETED, ABANDONED, or ROLLING_BACK to continue): set ROLLING_BACK and revert chunks in reverse index order (`lastUnrevertedChunk` downwards), each with the same rules. `lastUnrevertedChunk` starts, from PAUSED, at the incomplete chunk (`firstOpenChunk`) when its chunk file exists and otherwise at `firstOpenChunk` − 1, the last DONE chunk; from COMPLETED at the last chunk; from ABANDONED at `firstOpenChunk` − 1, because `Abandon` already reverted the incomplete chunk; when it would start below chunk 0 the operation becomes ROLLED_BACK at once. It is written to `state.json` with the ROLLING_BACK state, before the first revert; a chunk's `meta` elements are classified and restored in reverse order. Each chunk restores from its own `before/<k>/`, so a document that chunks 0, 1 and 2 all repaired is restored by chunk 2 to the bytes chunk 2 read, then by chunk 1 to the bytes chunk 1 read, then by chunk 0 to its original bytes. A GUID that a remap or orphan repair tombstoned comes back to its entry, because ASSET accepts a tombstoned GUID found on a live `.meta` and removes it from the tombstones (`Zenith2/Docs/Design/ASSET/Database.md → "Duplicate GUIDs"`, applied to sub-asset entries by row CT22): every batch kind is reversible until `Discard` (`Test_ContentBatch_SettingsBatchRollsBack`, `Test_ContentBatch_RemapRollbackRestoresTombstonedGuid`). Then the inverse cook requests run: `reimport` of every GUID the chunk touched, and for delete `import` after the files are back, which takes the GUID out of TOMBSTONE because a restored `.meta` leaves the tombstones. Each reverted chunk becomes REVERTED (`state.json`'s `lastUnrevertedChunk` moves down) and, after chunk 0, the operation ROLLED_BACK. A conflict (a document or a `.meta` changed since the operation) stops the rollback with `CONTENT_ERROR_BATCH_CONFLICT`; the operation stays ROLLING_BACK, and `Rollback` may be run again after the person resolves it, which re-classifies.
- `Discard` (COMPLETED, ABANDONED, ROLLED_BACK): delete the operation directory (staged copies and deleted files included). `ZenithContent batch discard` asks for confirmation listing the directory's size and, for a remap or orphan-repair operation, the GUIDs whose tombstones become permanent, because discarded deleted files and before-states are gone.
- A corrupt journal: `ZenithContent batch abandon --force-journal <opId>` renames the directory to `<opId>.corrupt` and releases the operation lock, writing nothing else; the tree is whatever the interrupted chunk left, and the person inspects it with the staged copies.

The editor records one history record for an operation (`Zenith2/Docs/Design/EDIT/Pipeline.md → "Reference repair"`); its undo is `Rollback` (rows CT18, CT30).

## Batch report

`report.json` (`$schema` `zenith2.content-batch-report`, version 1), written at every pause and at the end:

```json
{
	"$schema": "zenith2.content-batch-report",
	"schemaVersion": 1,
	"operation": "20271014T101500Z-3fa9c21e",
	"kind": "move",
	"game": "OpenWorldSlice",
	"state": "COMPLETED",
	"started": "20271014T101500Z",
	"finished": "20271014T101912Z",
	"counts": { "chunks": 8, "targets": 500, "targetsDone": 500, "targetsFailed": 0, "moves": 1000, "documentRepairs": 230, "documentsWritten": 214, "danglingOverrides": 3, "metaConflicts": 0, "cookFailures": 0, "warningsNotListed": 0 },
	"targets": [ { "guid": "0b6e…", "from": "Assets/Props/Old/Crate.glb", "to": "Assets/Props/Crates/Crate.glb", "outcome": "done" } ],
	"documents": [ { "path": "Scenes/Harbour.scene.json", "oldSha256": "9c…", "newSha256": "a1…", "edits": [ { "pointer": "/instances/8a1e…/hint", "from": "Assets/Props/Old/Crate.prefab.json", "to": "Assets/Props/Crates/Crate.prefab.json" } ], "editsNotListed": 0 } ],
	"cook": [ { "request": "reimport", "guids": 64, "failed": [] } ],
	"warnings": [ { "code": "DOCUMENT_WARNING_DANGLING_OVERRIDE", "path": "Scenes/Harbour.scene.json", "pointer": "/instances/…/overrides/properties/…" } ],
	"errors": [],
	"removedDirectories": [ "Assets/Props/Old" ]
}
```

- `outcome` is `done`, `pending`, `failed` (with `code`) or `reverted`; at most 64 edits are listed per document, and only while the chunk's listed edit text (the bytes of every listed `pointer`, `from` and `to`) stays within 16 MiB, the rest counted in `editsNotListed`; warnings are listed while a chunk's warning text stays within 4 MiB, the rest counted in `warningsNotListed`; `documentRepairs` is the plan's W and `documentsWritten` the repairs that changed bytes; `metaConflicts` counts the OTHER elements of "Meta element classification", each listed in `errors` with its key path and its before, target and current values; the report holds no host or user name.
- Source of every member: the counts, outcomes and cook results come from `plan.json`, `state.json` and the chunk files; each document's digests, listed edits and `editsNotListed`, and each chunk's warnings and `warningsNotListed`, come from that chunk's prepared file, where step 2 recorded them from the repairer's output; `errors` from the chunk files' `error` and the classification of the call that wrote the report. Nothing the report lists exists only in a process's memory, so the report written after any number of interruptions and resumes equals an uninterrupted run's (`Test_ContentBatch_InterruptedAtEachStepResumes`).
- The editor shows it (row CT18); `ZenithContent batch status --json` prints it.

## Move and rename

1. Rename is a move whose `to` differs only in the last segment; a folder rename is a directory move.
2. Identity: the `.meta` moves with its source, so the GUID is unchanged; ASSET's scan sees the moved `.meta` and records the move (`Zenith2/Docs/Design/ASSET/Database.md → "Rename and move detection"`), so a pure move re-cooks nothing but the catalogue's logical path.
3. An authoring document (scene, prefab, material, bank, …) moves the same way: its `id` is unchanged, so GUID references to it stay valid.
4. Path references are repaired: SCENE's `REFERENCE_REPAIR_ASSET_MOVED` rewrites prefab-instance hints (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Reference repair"`), and the generic repairer rewrites every other string that names the old path ("Generic reference repairer").
5. The cook request `reimport` of the moved GUIDs makes the server rescan, record the moves and re-cook the catalogue.

6. Directories: a destination directory that does not exist (`Assets/Props/Crates` when only `Assets/Props/Old` exists) is created by the chunk before its first move into it, and recorded in the prepared file's `directories[]`; at COMPLETED each source directory left empty by the moves is removed ("Chunked execution"); `Rollback` and `Abandon` recreate a missing source directory before moving a file back into it and remove each recorded destination directory that is empty afterwards ("Abandon and rollback").

`Test_ContentBatch_MoveKeepsGuidsAndRepairsPaths` moves a folder of a glTF, its images, a prefab and a physics material referenced by path into a new folder `Assets/Props/Crates` that does not exist, and asserts GUIDs unchanged, hints and path references rewritten, a following cook re-cooking only the catalogue, `Assets/Props/Crates` created and `Assets/Props/Old` removed at COMPLETED and listed in `removedDirectories`; then `Rollback` recreates `Assets/Props/Old`, restores every file there byte for byte and removes `Assets/Props/Crates`.

## Delete

1. The replacement, when given, must be a live asset of the same kind (checked through the channel's `record`; `CONTENT_ERROR_SPEC_INVALID` otherwise).
2. Repairs before removal: SCENE's `REFERENCE_REPAIR_ASSET_DELETED` with the replacement (or without, unpacking instances when `unpackInstances`), and the generic repairer rewriting the GUID text to the replacement's, or to JSON `null` when there is none.
3. The source and its `.meta` move into the operation's `deleted/` directory, never removed, so `Rollback` restores them byte for byte.
4. `import` of the GUIDs: the server rescans, the records become TOMBSTONE and the server writes the tombstone lines (`Zenith2/Docs/Design/ASSET/Database.md → "Orphaned meta files and tombstones"`), so the GUIDs are never reissued.
5. A reference whose owner's schema forbids null is left null and appears in the next cook's reference validation; the report lists every nulled pointer.

`Test_ContentBatch_DeleteWithReplacementRewritesReferences`, `Test_ContentBatch_DeleteWithoutReplacementNullsAndUnpacks`, `Test_ContentBatch_DeleteRecoverableAndTombstoned`.

## Reimport and settings batches

- reimport: the filter's sources in chunks; each chunk's `reimport` request; failures (`failed[{guid, code}]`) are recorded per target and the operation continues (a failed import is the content's problem, not the batch's); the report lists them.
- setPreset and setSettings: per target, `record` gives the `.meta` digest and the members the request changes, journaled as the before-state ("Chunked execution" step 6); `setSettings` with that digest as `expectedMetaSha256` and the new preset or the merged settings (the spec's object merged over the `.meta`'s `settings`, key by key); `COOK_ERROR_META_CHANGED` is a conflict of that target only (reported, the target `failed`), never of the chunk; then `reimport` of the chunk's changed targets. `Rollback` restores each target's recorded preset or settings ("Abandon and rollback").

## Sub-asset remap and orphan repair

- remapSubAssets: for each (source, old key, new key): references to the new key's current GUID are rewritten to the old key's GUID (generic and SCENE repair), then `setSettings` with `remapSubAssets: [{from: old, to: new}]` makes the server move the old GUID, with the old entry's owner members, onto the new key and tombstone the new key's discarded GUID (pending ASSET, rows CT1, CT22), then `reimport`; the person's overrides and references keyed by the old GUID are thereby valid again. The two touched entries are journaled before the request, so a rollback restores both, the tombstoned GUID included.
- repairOrphans: for each orphaned GUID: references rewritten to the replacement or nulled, then `setSettings` with `removeOrphanedSubAssets: [key]`, which removes the entry and adds its GUID to the tombstone list (rows CT1, CT22), so it is never reissued to another product; the removed entry is journaled, so a rollback restores it to its own `.meta` until the operation is discarded.

## Generic reference repairer

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract value Zenith_ContentRepairRequest
/// Ownership: Plain value; views borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Game-relative paths; logical paths.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentRepairRequest
{
	const Zenith_ContentBatchKind m_eKind = {};       // MOVE, DELETE, REMAP_SUB_ASSETS, REPAIR_ORPHANS
	const Zenith_PersistentID m_xSubject = {};        // the asset (or sub-asset) whose references change
	const Zenith_PersistentID m_xReplacement = {};    // DELETE, REPAIR_ORPHANS: null or the replacement; REMAP: the old GUID
	const Zenith_StringView m_strOldPath = {}, m_strNewPath = {}; // MOVE: game-relative
	const Zenith_StringView m_strOldLogicalPath = {}, m_strNewLogicalPath = {}; // MOVE
	const bool m_bUnpackInstances = false, m_bPruneDangling = false;
};

/// @contract value Zenith_ContentRepairEdit
/// Ownership: Plain value in the repairer's output arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the arena.
/// Units: A document pointer (Zenith2/Docs/Design/SCENE/DocumentFramework.md "Document pointers").
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_ContentRepairEdit { const Zenith_StringView m_strPointer = {}; const Zenith_StringView m_strFrom = {}; const Zenith_StringView m_strTo = {}; };

/// @contract interface Zenith_ContentReferenceRepairer
/// Ownership: Implemented inside ZenithAuthoring/Internal/CONTENT/Repair/ (the generic repairer, the SCENE adapter); static
///   instances chosen per document type by the batch engine.
/// Thread affinity: OWNER_THREAD (the batch's).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: CONTENT_ERROR_REPAIR_FAILED with the SCENE error code in m_uDetail, CONTENT_ERROR_DOCUMENT, CONTENT_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_ContentReferenceRepairer
{
public:
	virtual ~Zenith_ContentReferenceRepairer() = default;
	// Repairs one opened document in memory; appends its canonical new bytes (unchanged bytes when nothing applies) and its edits.
	virtual Zenith_Result<bool, Zenith_ContentError> Repair(const Zenith_ContentRepairRequest& xRequest,
		Zenith_OpenedDocument& xDocument, Zenith_DocumentMigrator& xMigrator, Zenith_Array<uint8>& auNewBytesOut,
		Zenith_Array<Zenith_ContentRepairEdit>& axEditsOut, Zenith_Arena& xArena) = 0;   // true: changed
};
```

| Document type | Repairer |
|---|---|
| `Zenith.SceneDocument`, `Zenith.PrefabDocument` | the SCENE adapter: maps the request to `Zenith_ReferenceRepairRequest` (MOVE → `REFERENCE_REPAIR_ASSET_MOVED` with the paths; DELETE and REPAIR_ORPHANS → `REFERENCE_REPAIR_ASSET_DELETED` with the replacement and the flags; REMAP → `ASSET_DELETED` of the new GUID with the old GUID as replacement), calls `Zenith_PlanReferenceRepair` over the operation's index and provider, and takes the plan's write for this document |
| `Zenith.AssetMeta` | none: `.meta` changes go through the cook server ("Cook channel") |
| every other registered document type | the generic repairer |

The generic repairer walks the opened tree (known and unknown members alike) and, for every JSON string value:

| Request | A string value equal to | becomes |
|---|---|---|
| MOVE | the old game-relative path; or the old logical path, alone or followed by `#<sub-asset key>` | the new path, keeping any `#` suffix |
| DELETE, REPAIR_ORPHANS | the subject GUID's 36-character text | the replacement's text, or JSON `null` |
| REMAP_SUB_ASSETS | the subject (the new key's GUID) | the old GUID's text |

The new tree is written with the type's canonical writer (`Zenith_DocumentMigrator::Serialize`), so only the edited lines change. The generic index used at planning maps each GUID text and each path string to the documents containing it, built by one walk of every document of the game's roots. GUID texts match only whole string values, so a 122-bit random GUID matching an unrelated value is not an expected event; a path matches only whole values equal to a source's path.

`Test_GenericRepairer_RewritesWholeStringValuesOnly`, `Test_GenericRepairer_KeepsSubAssetSuffix`, `Test_GenericRepairer_ReachesUnknownMembers`, `Test_SceneRepairAdapter_MapsEachKind`.

## Cook channel

The channel is asynchronous: every request is begun, returns a ticket at once, and is polled without blocking, so no unit waits for the cook server and `Run` polls with a 1 ms sleep between calls rather than waiting on any primitive (the server's protocol is already asynchronous: requests carry IDs, progress arrives as notifications and `cancel` names a request, `Zenith2/Docs/Design/ASSET/Formats.md → "Cook server messages"`). An operation never has more than 32 requests outstanding (planning's filter resolution; execution keeps one), so an implementation that queues requests for another thread needs a queue of 32 entries to never block.

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract value Zenith_ContentCookValues
/// Ownership: Plain values; views and spans point into the arena passed to Poll, or are borrowed for a Begin call.
/// Thread affinity: IMMUTABLE. Members non-const in Zenith_ContentCookRecord, Zenith_ContentCookResult,
///   Zenith_ContentCookTicket and Zenith_ContentCookResponse: assigned into the Zenith_ContentCookResponse that the
///   channel fills through its out-parameter; a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter; filled through the out-parameter of Zenith_ContentCookChannel.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The fields of the cook server's requests and results.
/// @endcontract
struct Zenith_ContentCookRecord
{
	Zenith_PersistentID m_xGUID;
	Zenith_StringView m_strKind, m_strSourcePath, m_strLogicalPath, m_strImporter, m_strState;
	Zenith_Sha256Value m_xMetaSha256;
	Zenith_PersistentID m_xPreset;                              // null: none (record's "preset", row CT22)
	Zenith_JsonNode m_xSettings, m_xProvenance, m_xLabels;      // the .meta objects verbatim; null nodes when absent
	Zenith_Span<const Zenith_ContentSubAssetView> m_axSubAssets; // every entry with its members
};
struct Zenith_ContentCookFailure { const Zenith_PersistentID m_xGUID = {}; const Zenith_StringView m_strCode = {}; };
struct Zenith_ContentCookResult { Zenith_Span<const Zenith_PersistentID> m_axChanged; Zenith_Span<const Zenith_ContentCookFailure> m_axFailed; };
struct Zenith_ContentSubAssetMembers { const Zenith_StringView m_strKey = {}; const Zenith_JsonDocument* const m_pxMembers = nullptr; };   // owner members to set
struct Zenith_ContentSetSettingsRequest
{
	const Zenith_PersistentID m_xGUID = {};
	const Zenith_Sha256Value m_xExpectedMetaSha256 = {};
	const Zenith_JsonDocument* const m_pxSettings = nullptr;    // null: settings unchanged (optional, row CT1 (b))
	const bool m_bSetPreset = false;
	const Zenith_PersistentID m_xPreset = {};
	const bool m_bSetProvenance = false;
	const Zenith_JsonDocument* const m_pxProvenance = nullptr;  // with m_bSetProvenance: the new object, null to remove it
	const Zenith_Span<const Zenith_ContentSubAssetMembers> m_axSubAssetMembers = {};
	const Zenith_Span<const Zenith_ContentKeyPair> m_axRemapSubAssets = {};
	const Zenith_Span<const Zenith_StringView> m_astrRemoveOrphanedSubAssets = {};
	const Zenith_Span<const Zenith_JsonNode> m_axRestoreSubAssets = {}; // Rollback: recorded entries restored whole, GUIDs included
};
struct Zenith_ContentCookListRequest { const Zenith_StringView m_strLogicalPrefix = {}; const Zenith_Span<const Zenith_StringView> m_astrKinds = {}; };
enum Zenith_ContentCookRequestKind : uint8
{
	CONTENT_COOK_REQUEST_RECORD = 1, CONTENT_COOK_REQUEST_LIST, CONTENT_COOK_REQUEST_IMPORT, CONTENT_COOK_REQUEST_REIMPORT,
	CONTENT_COOK_REQUEST_SET_SETTINGS, CONTENT_COOK_REQUEST_DEPENDENTS,
};
struct Zenith_ContentCookTicket { uint64 m_ulID = 0; };       // 0: no request
enum Zenith_ContentCookPoll : uint8 { CONTENT_COOK_POLL_PENDING = 1, CONTENT_COOK_POLL_DONE };
struct Zenith_ContentCookResponse
{
	Zenith_ContentCookRequestKind m_eKind;
	Zenith_ContentCookRecord m_xRecord;                         // RECORD
	Zenith_Span<const Zenith_PersistentID> m_axGUIDs;           // LIST (every page, logical-path order), DEPENDENTS
	Zenith_ContentCookResult m_xResult;                         // IMPORT, REIMPORT
	Zenith_Sha256Value m_xMetaSha256;                           // SET_SETTINGS: the .meta's new digest
};

/// @contract interface Zenith_ContentCookChannel
/// Ownership: Implemented by EDIT's ZenithEditorCore over Zenith_EditorCookClient (rows CT18, CT27 (b), CT30); borrowed by a batch for its life.
/// Thread affinity: OWNER_THREAD (the batch's: whichever thread called Plan or Open). No member blocks: Begin* returns once the
///   request is written to the server's pipe or queued for the thread that owns the cook client (at most 32 outstanding per
///   operation), Poll returns PENDING until the request's final line arrived and never waits for it.
/// Invalidation: A ticket is invalid after the Poll that returned DONE or an error, and after Cancel.
/// Units: None.
/// Errors: CONTENT_ERROR_COOK_CHANNEL with the request's COOK_ERROR_* code index in m_uDetail (the server's error), or with 0 when
///   the server is not reachable; a per-asset failure inside a successful request is a Zenith_ContentCookFailure, not an error.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_ContentCookChannel
{
public:
	virtual ~Zenith_ContentCookChannel() = default;
	virtual Zenith_Result<Zenith_ContentCookTicket, Zenith_ContentError> BeginRecord(const Zenith_PersistentID& xGUID) = 0;
	virtual Zenith_Result<Zenith_ContentCookTicket, Zenith_ContentError> BeginList(
		const Zenith_ContentCookListRequest& xRequest) = 0;                                                // list
	virtual Zenith_Result<Zenith_ContentCookTicket, Zenith_ContentError> BeginImport(
		Zenith_Span<const Zenith_PersistentID> axGUIDs, bool bForce) = 0;                                  // import, reimport
	virtual Zenith_Result<Zenith_ContentCookTicket, Zenith_ContentError> BeginSetSettings(
		const Zenith_ContentSetSettingsRequest& xRequest) = 0;                                             // setSettings
	virtual Zenith_Result<Zenith_ContentCookTicket, Zenith_ContentError> BeginDependents(
		const Zenith_PersistentID& xGUID) = 0;                                                             // dependents
	virtual Zenith_Result<Zenith_ContentCookPoll, Zenith_ContentError> Poll(Zenith_ContentCookTicket xTicket,
		Zenith_ContentCookResponse& xOut, Zenith_Arena& xArena) = 0;
	virtual void Cancel(Zenith_ContentCookTicket xTicket) = 0;                                                // the server's cancel
};
```

**Filter resolution** (Plan step 2). `m_axGUIDs` non-empty: one `record` per GUID. Otherwise one `list` whose `prefix` is the logical form of `m_strPathPrefix` under the game's content-root mapping (`content:/` for an empty prefix or `m_bAll`; `Zenith2/Docs/Design/ASSET/Cook.md → "Content root mapping rules"`), because the server's `list` filters by prefix, kind and state only; then one `record` per listed GUID, at most 32 in flight at once (planning step 2's units begin and poll them), keeping a GUID when its `importer.id` equals `m_strImporter` (when set) and its `labels` contain every label of `m_astrLabels`: O(n) requests for n listed records, the records also giving each target's `.meta` digest for the following steps.

<!-- assumption: pending EDIT, ASSET -->
`ZenithEditorCore` provides `Zenith_EditorContentCookChannel`, the one implementation, over `Zenith_EditorCookClient`, used by the editor and by `ZenithContent` (which starts its own `ZenithCook --serve` child through that client). In EDIT's FORWARD mode, where the batch runs on the `EditorContentBatch` thread and the client belongs to the main thread, `Begin*` pushes the request onto the queue the main thread drains each frame and returns its ticket at once, and `Poll` reads the completion the main thread posts for that ticket and returns PENDING until it is posted; neither waits on the queue's semaphore, which only a caller that chooses to sleep may use (row CT30). In PUMP mode (`ZenithContent`), `Begin*` writes to the pipe and `Poll` drains the client's lines without waiting. The `setSettings` arguments of the request struct and the extra `record` members are ASSET's extension of rows CT1 (b) and CT22, and the storage-root writer lock's mode string `"content-batch"` joins ASSET's list of lock-holder modes (`Zenith2/Docs/Design/ASSET/Formats.md → "Cook lock file"`, row CT1 (e)). Rows CT18, CT27 (b), CT30, CT1, CT22.
<!-- /assumption -->

## Batch state machine

| State \ Event | Run | Step | planning done or chunk DONE (last) | conflict | cancel | channel or lock failure | process death | Abandon | Rollback | Discard |
|---|---|---|---|---|---|---|---|---|---|---|
| PLANNING | planning units until PLANNED | planning units | PLANNED (`plan.json`, `state.json` written) | — (a specification error returns `SPEC_INVALID`; the state stays and only destruction is useful) | stays PLANNING, `BATCH_CANCELLED` | stays PLANNING, `COOK_CHANNEL`; the next call re-issues | nothing written; the lock is released | `BATCH_STATE` | `BATCH_STATE` | `BATCH_STATE` |
| PLANNED | RUNNING | RUNNING (first units run) | — | — | — | — | stays PLANNED | ABANDONED | `BATCH_STATE` | `BATCH_STATE` |
| RUNNING | `BATCH_STATE` | RUNNING (continues; a request in flight is polled) | COMPLETED | PAUSED | PAUSED | PAUSED | stays RUNNING in the journal; `Open` treats it as PAUSED | `BATCH_STATE` | `BATCH_STATE` | `BATCH_STATE` |
| PAUSED | RUNNING (resume) | RUNNING (resume) | — | — | — | — | stays PAUSED | ABANDONED (PAUSED on a conflict) | ROLLING_BACK | `BATCH_STATE` |
| COMPLETED | `BATCH_STATE` | `BATCH_STATE` | — | — | — | — | — | `BATCH_STATE` | ROLLING_BACK | deleted |
| ABANDONED | `BATCH_STATE` | `BATCH_STATE` | — | — | — | — | — | `BATCH_STATE` | ROLLING_BACK | deleted |
| ROLLING_BACK | `BATCH_STATE` | `BATCH_STATE` | ROLLED_BACK (chunk 0 reverted) | stays ROLLING_BACK | stays ROLLING_BACK | stays ROLLING_BACK | stays ROLLING_BACK | `BATCH_STATE` | ROLLING_BACK (continues) | `BATCH_STATE` |
| ROLLED_BACK | `BATCH_STATE` | `BATCH_STATE` | — | — | — | — | — | `BATCH_STATE` | `BATCH_STATE` | deleted |

Every batch kind is reversible by `Rollback` from PAUSED, COMPLETED and ABANDONED, delete, remapSubAssets and repairOrphans included ("Abandon and rollback"); only `Discard` makes an operation's effects permanent.

`BATCH_STATE` is `CONTENT_ERROR_BATCH_STATE` with the state in the detail; nothing changes. `Test_ContentBatch_StateMachineEveryCell` drives every cell, the PLANNING row and the `Step` column included.

## Batch command line

```text
ZenithContent batch plan     --game <Name> --spec <file.json> [--chunk <n>]   Plan, then Run to PLANNED; prints the operation ID, the chunk count, W and the referencing documents
ZenithContent batch run      --game <Name> (--spec <file.json> | --op <id>) [--chunk <n>]   with --spec: Plan and Run twice; with --op: Open and Run
ZenithContent batch resume   --game <Name> --op <id>
ZenithContent batch status   --game <Name> [--op <id>] [--json]
ZenithContent batch abandon  --game <Name> --op <id> [--force-journal]
ZenithContent batch rollback --game <Name> --op <id>
ZenithContent batch discard  --game <Name> --op <id> [--yes]
```

Every command runs the operation on its main thread, its owner thread. The command creates the game root's writable `Zenith_HostDocumentStorage` (writer mode `"content-batch"`), the read-only engine-root storages and their migrators over the registry `Zenith_RegisterEditorDocumentTypes` fills (pending EDIT, row CT18), the game's type registry and prefab provider through `ZenithEditorCore`, and the cook channel over a `ZenithCook --serve --game <Name> --platform windows --out ${ZENITH_OUT}/Cooked/<Name>/windows` child.

| Exit code | Meaning |
|---|---|
| 0 | the operation reached COMPLETED (or the requested terminal state) |
| 1 | the operation is PAUSED: a conflict, a cancellation, or per-target failures; the report names them |
| 2 | bad command line or specification (`CONTENT_ERROR_SPEC_INVALID`) |
| 3 | the journal is corrupt or the game cannot be opened |
| 4 | low disk |
| 5 | internal failure (out of memory, I/O) |
| 6 | the operation lock or a writer lock is held by another process (`BATCH_BUSY`, `SOURCE_LOCKED`) |
| 7 | the cook server could not be started or reached; with the editor's server holding the output tree (`COOK_ERROR_OUTPUT_LOCKED`) the message reads "the editor's cook server holds this tree; run the batch from the editor" |

`Test_ContentBatchCommand_ExitCodes` produces each code from a fixture.
