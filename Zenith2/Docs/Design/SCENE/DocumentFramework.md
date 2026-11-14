# SCENE design — the authoring-document framework

This file is part of the SCENE design family; the entry document is `Zenith2/Docs/Design/SCENE.md`. It specifies `ZenithDocument`, the library that holds the authoring-document migration framework of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind" (per-type schema version and chain, upgrade preview, automatic backup, journaled atomic write, override and unknown-field preservation, fixtures) and canonical serialization, and `ZenithDocumentHost`, the tool-side storage over the host filesystem. [Z2-N-CONTRACT-550] delivers the framework in Stage 2 "by SCENE and ASSET"; the one home of the framework is this library, owned by SCENE, and ASSET's half is the `.meta` document type with its importer-settings chains, its fixtures and the cook's integration (§ "Ownership split with ASSET"). EDIT owns the migration UI. Component-level schema evolution (`Zenith_MigrationChain`, `Zenith_MigrateFieldSet`) is ECS's and is called, never restated (`Zenith2/Docs/Design/ECS/Schemas.md` → "Schema migration").

## Why a separate library

<!-- elaboration -->
<!-- assumption: pending X109(a) -->
The framework has consumers at three levels of the library graph. A home inside `ZenithScene` (layer 4) would make every consumer link ZenithECS, ZenithAssets and ZenithJobs, which the tool-side consumers below do not use, and would forbid the framework to any runtime ASSET code, because ZenithAssets sits below ZenithScene:

| Consumer | Its library and layer | Can it depend on `ZenithScene` (layer 4)? |
|---|---|---|
| ASSET, the co-deliverer of [Z2-N-CONTRACT-550], whose half of the framework ([Z2-N-CONTRACT-552] lists "importer `.meta` settings") belongs to ASSET's libraries: the tool `ZenithCook` today, and the runtime `ZenithAssets` (layer 3) for any runtime use | `ZenithCook` (a tool) and `ZenithAssets` (layer 3) | `ZenithCook` can: its dependency set reaches `ZenithScene` (cross-doc item X114 (a)), and ASSET's `.meta` code and asset database are tool-side (`Zenith2/Docs/Design/ASSET/Migration.md`, `Zenith2/Docs/Design/ASSET/Database.md`). `ZenithAssets` cannot: `ZenithScene` depends on `ZenithAssets` (`Zenith2/Docs/Architecture.md` → "Library dependency graph"), so an edge `ZenithAssets → ZenithScene` closes the cycle `ZenithAssets → ZenithScene → ZenithAssets`, which the declared graph may not contain ([Z2-N-CONTRACT-172]) and the `Libgraph` step fails ([Z2-N-CONTRACT-174]). A layer-2 home keeps both free to use the framework; a `ZenithScene` home costs the cook nothing it does not link already and forbids the runtime one |
| MOD's `game.json` chain in `ZenithNewGameScaffold` | a tool library whose `DEPENDS` is ZenithCore, ZenithPlatform and ZenithReflect (`Zenith2/Docs/Design/MOD.md` → "Library and file layout"); `Zenith2/Docs/Design/MOD/ProjectCreation.md` → "game.json migration" runs "the authoring-document migration framework" and adds "the library that hosts it" | it can, at the cost of pulling ZenithECS, ZenithAssets and ZenithJobs into a project scaffolder that uses none of them |
| SCENE's scene and prefab types, ECS's schema documents, PLAT, GEOM, ANIM, UI and CINE documents | `ZenithScene` (layer 4) and the tools | yes |

The conclusion rests on cohesion and on that cost, not on reachability: a library at layer 2 is reachable from all three without a cycle and brings nothing the framework does not use: `ZenithDocument` depends on ZenithCore (layer 0), ZenithReflect (layer 1, for `Zenith_TypeID` and the migration types of `ZenithReflect/Zenith_Migration.h`) and ZenithProfiling (layer 1, zones and counters), so it sits at layer 2; `ZenithAssets` (layer 3) may name it, and `ZenithScene` keeps layer 4 with `ZenithDocument` (2) and `ZenithCompression` (1) added to its `DEPENDS`. The library list of `Zenith2/Docs/Architecture.md` → "Library dependency graph" is not a settled decision (seven libraries joined it by review, cross-doc item X5), and a library is not custom infrastructure under [Z2-N-CONTRACT-278] (whose justification records cover containers, allocators, heap wrappers and threading primitives), so no flagged proposal is needed. The Architecture patch that adds the `ZenithDocument` and `ZenithDocumentHost` rows to "Library dependency graph", "Repository layout" and "Ownership per directory", and the `ZenithDocument` and `ZenithCompression` edges of `ZenithScene`, is cross-doc item X109(a), pending the core-doc batch that follows the Q43 ruling.
<!-- /assumption -->

`ZenithDocument` performs no host file I/O: every read and write goes through the `Zenith_DocumentStorage` interface it declares, so a runtime target that links it never gains a file path outside the virtual filesystem ([Z2-N-ARCH-923] is untouched). The host-filesystem implementation is `ZenithDocumentHost`, a tool-side library the editor, the cook and `ZenithNewGameScaffold` link. SAVE may reuse the journaled write for its own writes over a storage adapter of its own; SCENE binds nothing in SAVE, and cross-area row S17 records the offer.

## Ownership split with ASSET

The Q43 ruling makes this library the framework's one home. The split of every framework piece:

| Piece | Owner | Where |
|---|---|---|
| registry, chain completeness at `Freeze`, open, plan (the upgrade preview), apply, save, backup, journaled write, recovery, the unknown-member guard, low-disk refusal, canonical serialization, document paths and document pointers, the storage interface and both storages | SCENE | this file |
| the scene and prefab document types, their canonical schemas, chains, `Zenith_RewriteOverridePaths` and fixtures | SCENE | `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocumentTypes header"; § "Migration fixtures" |
| the `.meta` document type: its `Zenith_DocumentTypeDesc` (stable name `Zenith.AssetMeta`, suffix `.meta`), its canonical schema, its steps (`Zenith_RegisterAssetMetaMigrations`), the importer-settings chains it runs inside a `.meta` through its embedded visitor (`m_pfnVisitFieldSets`), the rule that a removed settings key is kept under `x-removed.<key>`, and its fixtures with `Test_MetaMigration_Fixtures` | ASSET | `Zenith2/Docs/Design/ASSET.md` → "Migration hooks" |
| the cook's integration: the cook opens every document through `Zenith_DocumentMigrator` over a read-only host storage, never writes a source document, and refuses to cook while `RecoverPendingJournals` reports `DOCUMENT_ERROR_JOURNAL_PENDING` (§ "Journaled write") | ASSET | `Zenith2/Docs/Design/ASSET.md` → "Migration hooks" |
| every other document type's registration, steps and fixtures | its owner ([Z2-N-CONTRACT-548]) | `Zenith2/Docs/Design/SCENE.md` → "Migration hooks" |
| the migration UI | EDIT (Stage 4) | Z2-I-EDIT-15 to -17 |

ASSET withdraws the tool library `ZenithDocumentMigration`, its `Zenith_MigrationRegistry` and `Zenith_DocumentMigrationError`, its run functions, journal and diff, and its `ZenithDocumentMigration_Tests`; ASSET's low-disk check is carried by this file (`DOCUMENT_ERROR_LOW_DISK`, § "Journaled write"), so nothing of ASSET's framework specification is lost. Cross-area row S15 lists the hooks that stay ASSET's (`Zenith2/Docs/Design/ASSET/Migration.md` already re-cites this file).

## Document paths

A storage path is the storage-root-relative name of one file in a `Zenith_DocumentStorage` (for a game's storage, relative to `Zenith2/Games/<Name>/`): a document, or one of the framework's own files (journals, temporaries, backups). A document path is a storage path that names a document. Two validators, applied at two levels:

| Rule | Condition | `Zenith_ValidateStoragePath` | `Zenith_ValidateDocumentPath` |
|---|---|---|---|
| P1 | 1 to 240 bytes of valid UTF-8 in Unicode normalisation form C, no U+0000 to U+001F, no U+007F | yes | yes |
| P2 | no `\`, no `:`, no leading `/`, no trailing `/` | yes | yes |
| P3 | components separated by `/`; no empty component, no `.` or `..` component | yes | yes |
| P4 | no component equal, case-insensitively and with any extension removed, to `CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9` | yes | yes |
| P5 | no component ending in `.` or a space | yes | yes |
| P6 | not `.migration` as the first component, and no final component ending in `.z2journal` or `.z2tmp` (the framework's own names) | no | yes |

The rules run in order and the first failing one is reported as `DOCUMENT_ERROR_INVALID_PATH` with the rule number logged. Who applies which:

- Every `Zenith_DocumentStorage` implementation applies `Zenith_ValidateStoragePath` to every path argument of every member, so the framework's own writes (`<batch>.z2journal`, `<batch>.z2journal.z2tmp`, `<path>.z2tmp`, `.migration/backups/…`) pass through the storage.
- `Zenith_DocumentMigrator` applies `Zenith_ValidateDocumentPath` to every path a caller supplies (`Open`, `PlanMigration`, the files `PlanMigrationOfDirectory` finds, `SaveDocuments`), so a caller can never open, migrate or overwrite a framework file as a document; `PlanMigrationOfDirectory` skips a framework file silently rather than reporting it.
- Two paths of one batch that are equal under ASCII case folding are `DOCUMENT_ERROR_INVALID_PATH` (Windows and Android file systems differ in case sensitivity).

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentPath.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithDocument/Zenith_DocumentErrors.h"

inline constexpr uint32 uDOCUMENT_MAX_PATH_BYTES = 240;

/// @contract functions Zenith_DocumentPathFunctions
/// Ownership: Pure functions over the caller's string.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes of UTF-8.
/// Errors: DOCUMENT_ERROR_INVALID_PATH, with the failing rule number (1 to 6) logged in category "document"; never an
///   assert on a path.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_ValidateStoragePath(Zenith_StringView strPath);   // P1-P5
Zenith_Result<void, Zenith_DocumentError> Zenith_ValidateDocumentPath(Zenith_StringView strPath);  // P1-P6
bool Zenith_IsFrameworkFilePath(Zenith_StringView strPath);   // the P6 test alone
```

### Document pointers

<!-- elaboration -->
Every path the framework produces or accepts inside a document — `Zenith_DocumentTree::FindPath` (§ "Document tree"), the removal declarations of `m_pfnDeclareRemoved`, the unknown-member guard's object and member paths (§ "Unknown members across migration steps"), `Zenith_DocumentDiff::m_astrChangedPaths` and `Zenith_DocumentWarning::m_strFieldPath` — is a document pointer: the JSON Pointer grammar of RFC 6901 over the document tree. A dotted grammar cannot serve, because member keys contain `.`: component keys are stable names (`Zenith.LocalTransform`), rule C5 admits any non-empty key, and ASSET keeps removed settings under `x-removed.<key>` (`Zenith2/Docs/Design/ASSET/Migration.md`), so `components.Zenith.LocalTransform` could name either one member or a member `Zenith` holding `LocalTransform`.

| Rule | Statement |
|---|---|
| D1 | the empty string is the document root; every other pointer is a sequence of reference tokens, each preceded by `/` |
| D2 | a token names an object member by its key with `~` written `~0` and `/` written `~1`; no other escape exists, and a `~` followed by anything but `0` or `1` is invalid |
| D3 | a token applied to an array is an element index: `0`, or decimal digits without a leading zero, below the array's length; `-` is invalid. The rule depends on the tree (whether a token meets an array, and its length), so `Zenith_ValidateDocumentPointer`, which has no tree, cannot check it: `FindPath` (§ "Document tree") and the unknown-member guard's resolution of a declared path apply it, and a violation is `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE` |
| D4 | tokens compare as bytes after unescaping; a pointer is valid UTF-8 of at most `uDOCUMENT_MAX_POINTER_BYTES` (4,096) bytes |

| Pointer | Names |
|---|---|
| `/entities/3a9d4e2b-8c1f-4a6d-b2e7-0f5c9d1e4a83/components/Zenith.LocalTransform/fields/1` | property 1 of that entity's local transform, reached through the member whose key is `Zenith.LocalTransform` |
| `/settings/x-removed.maxSize` | the member `x-removed.maxSize` of a `.meta` document's `settings` object (ASSET's kept removed setting) |
| `/instances/8a1e5c3d-2f7b-4e90-b4d6-0c9a3f1e7d28/overrides/removedEntities/0` | the first removed-entity target of that instance |
| `/a~1b/~0c` | the member `~c` of the member `a/b` |

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentPath.h (continued; includes "ZenithCore/Zenith_DocumentTree.h")
inline constexpr uint32 uDOCUMENT_MAX_POINTER_BYTES = uJSON_POINTER_MAX_BYTES;   // 4,096: one limit for the validator and FindPath

/// @contract functions Zenith_DocumentPointerFunctions
/// Ownership: Pure functions; the append functions extend the caller's array.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes of UTF-8; rules D1 to D4 of "Document pointers".
/// Errors: Zenith_ValidateDocumentPointer checks the tree-free rules D1, D2 and D4 and returns DOCUMENT_ERROR_INVALID_PATH
///   with the failing rule logged (D3 needs the tree and is applied where the pointer is resolved);
///   the append functions return DOCUMENT_ERROR_INVALID_PATH past uDOCUMENT_MAX_POINTER_BYTES and
///   DOCUMENT_ERROR_OUT_OF_MEMORY, leaving xPointer unchanged on failure. Never an assert on a pointer.
/// Version: since Z2 Stage 2. The grammar is permanent: journals, reports and warnings store pointers.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_ValidateDocumentPointer(Zenith_StringView strPointer);
Zenith_Result<void, Zenith_DocumentError> Zenith_AppendDocumentPointerToken(Zenith_Array<char>& xPointer,
	Zenith_StringView strKey);      // appends '/', then the key with '~' -> "~0" and '/' -> "~1"
Zenith_Result<void, Zenith_DocumentError> Zenith_AppendDocumentPointerIndex(Zenith_Array<char>& xPointer,
	uint32 uIndex);                // appends '/', then the decimal index
```

`Test_DocumentPointer_GrammarAndEscapes` asserts the four examples, the escapes in both directions, one refusal of `Zenith_ValidateDocumentPointer` per tree-free rule, the D3 refusals of `FindPath` over an array, and the ambiguity the grammar removes: in a tree holding both a member `Zenith.LocalTransform` and a member `Zenith` whose child is `LocalTransform`, the two pointers `/Zenith.LocalTransform` and `/Zenith/LocalTransform` reach different nodes through `FindPath`.

## Zenith_DocumentErrors header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_DocumentErrorCode
/// Ownership: Plain enumeration, the code of Zenith_DocumentError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: INVALID_PATH: a path fails "Document paths". NOT_FOUND: no such document. IO: a storage operation failed
///   (the OS code is logged). TOO_LARGE: above 64 MiB. PARSE: JSON syntax or a parser limit (the Zenith_JsonReadError
///   is logged). SHAPE: a header key is missing or has the wrong kind. UNKNOWN_TYPE: no registered document type
///   matches the "$schema" tag or the file suffix. NEWER_VERSION: the document's version is newer than the tool's; it is
///   opened read-only. CHAIN_GAP: a step is missing between the document's version and the current one. STEP_FAILED: a
///   migration step returned an error (its Zenith_MigrationError is logged). NOT_DETERMINISTIC: re-running the plan's
///   migration produced different bytes. READ_ONLY: a write of a read-only document. JOURNAL_PENDING: an unrecovered
///   journal exists. JOURNAL_CORRUPT: a journal fails its own validation. RECOVERY_CONFLICT: a document matches neither
///   hash its journal recorded. BACKUP: a backup copy failed. STALE_PLAN: a document changed since its plan was made.
///   DUPLICATE_REGISTRATION: a document type or step registered twice. INVALID_DESC: a desc field out of range.
///   OUT_OF_MEMORY. WRITE: the canonical writer failed (its Zenith_JsonWriteError is logged). REGISTRY_FROZEN: a
///   registration after Freeze. LOW_DISK: the free space of the storage is below the threshold of "Journaled write" step
///   0b; nothing was written. STORAGE_LOCKED: the storage root's writer lock stayed held by another process for the
///   migrator's wait ("Storage-root writer lock"); nothing was recovered, read to decide or written.
/// Version: since Z2 Stage 2. An enumerator never changes meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_DocumentErrorCode : uint8
{
	DOCUMENT_ERROR_INVALID_PATH = 1,
	DOCUMENT_ERROR_NOT_FOUND,
	DOCUMENT_ERROR_IO,
	DOCUMENT_ERROR_TOO_LARGE,
	DOCUMENT_ERROR_PARSE,
	DOCUMENT_ERROR_SHAPE,
	DOCUMENT_ERROR_UNKNOWN_TYPE,
	DOCUMENT_ERROR_NEWER_VERSION,
	DOCUMENT_ERROR_CHAIN_GAP,
	DOCUMENT_ERROR_STEP_FAILED,
	DOCUMENT_ERROR_NOT_DETERMINISTIC,
	DOCUMENT_ERROR_READ_ONLY,
	DOCUMENT_ERROR_JOURNAL_PENDING,
	DOCUMENT_ERROR_JOURNAL_CORRUPT,
	DOCUMENT_ERROR_RECOVERY_CONFLICT,
	DOCUMENT_ERROR_BACKUP,
	DOCUMENT_ERROR_STALE_PLAN,
	DOCUMENT_ERROR_DUPLICATE_REGISTRATION,
	DOCUMENT_ERROR_INVALID_DESC,
	DOCUMENT_ERROR_OUT_OF_MEMORY,
	DOCUMENT_ERROR_WRITE,
	DOCUMENT_ERROR_REGISTRY_FROZEN,
	DOCUMENT_ERROR_LOW_DISK,
	DOCUMENT_ERROR_STORAGE_LOCKED,
};

/// @contract value Zenith_DocumentError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480] (code first, 16 bytes).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uDocumentIndex indexes the call's document list (0xFFFFFFFF when not applicable); line and column are
///   1-based source positions (0 when not applicable).
/// Errors: None; the path and the text are logged at the point of failure ([Z2-N-CONTRACT-464]) in category "document".
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentError
{
	Zenith_DocumentErrorCode m_eCode;
	uint8 m_uReserved0 = 0;
	uint16 m_uReserved1 = 0;
	uint32 m_uDocumentIndex = 0xFFFFFFFFu;
	uint32 m_uLine = 0;
	uint32 m_uColumn = 0;
};
static_assert(sizeof(Zenith_DocumentError) == 16);

/// @contract enum Zenith_DocumentWarningCode
/// Ownership: Plain enumeration of the warnings a document operation reports without failing.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: UNKNOWN_FIELD: a member the reader does not know was kept ([Z2-N-CONTRACT-544]). DANGLING_OVERRIDE: an
///   override whose target no longer exists was kept ([Z2-N-CONTRACT-547]). NOT_CANONICAL: the input bytes differ from
///   their canonical form (the next write normalises them). UNKNOWN_COMPONENT: a component whose stable name is not
///   registered was kept as opaque data. NEWER_COMPONENT_VERSION: a component written by a newer schema; the document
///   opens read-only. REFERENCE_UNRESOLVED: an entity or asset reference could not be resolved. DUPLICATE_SORT_KEY:
///   two siblings carry equal sort keys; the bytes may be canonical, the siblings are ordered by ID, and nothing is
///   rewritten until an edit of that sibling group (Zenith2/Docs/Design/SCENE/Format.md "Sort keys").
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_DocumentWarningCode : uint8
{
	DOCUMENT_WARNING_UNKNOWN_FIELD = 1,
	DOCUMENT_WARNING_DANGLING_OVERRIDE,
	DOCUMENT_WARNING_NOT_CANONICAL,
	DOCUMENT_WARNING_UNKNOWN_COMPONENT,
	DOCUMENT_WARNING_NEWER_COMPONENT_VERSION,
	DOCUMENT_WARNING_REFERENCE_UNRESOLVED,
	DOCUMENT_WARNING_DUPLICATE_SORT_KEY,
};

constexpr const char* Zenith_ToString(Zenith_DocumentErrorCode eCode);     // "DOCUMENT_ERROR_IO", …
constexpr const char* Zenith_ToString(Zenith_DocumentWarningCode eCode);
```

`DOCUMENT_WARNING_UNKNOWN_FIELD` is the warning Contracts names `MIGRATION_WARNING_UNKNOWN_FIELD` ([Z2-N-CONTRACT-544]); the log line carries both spellings (`MIGRATION_WARNING_UNKNOWN_FIELD (DOCUMENT_WARNING_UNKNOWN_FIELD): <document>: <path> (written by schema version <n>)`).

## Zenith_DocumentStorage header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentStorage.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithDocument/Zenith_DocumentErrors.h"

template<typename T> class Zenith_Array;

/// @contract functions Zenith_DocumentListVisitor
/// Ownership: A function pointer the caller of Zenith_DocumentStorage::List passes with its user data; owns nothing.
/// Thread affinity: OWNER_THREAD. Invoked on the thread calling List, once per file, in byte order of path.
/// Invalidation: strPath is valid for the call only.
/// Units: strPath is a storage path; ulBytes is the file size in bytes.
/// Errors: None; the visitor cannot stop the listing.
/// Reentrancy: NONE. The visitor never calls into the storage that invokes it.
/// Version: since Z2 Stage 2.
/// @endcontract
using Zenith_PfnDocumentListVisitor = void (*)(Zenith_StringView strPath, uint64 ulBytes, void* pUserData);

/// @contract interface Zenith_DocumentWriterLock
/// Ownership: Implemented by ZenithDocumentHost for one writable host storage root and owned by that storage, which returns
///   it from GetWriterLock; borrowed by the migrator and by tools that hold the root for a run or a request.
/// Thread affinity: OWNER_THREAD. The thread of the storage's migrator or writer.
/// Invalidation: Destroying the storage releases a held lock.
/// Units: Milliseconds.
/// Errors: Acquire returns DOCUMENT_ERROR_STORAGE_LOCKED when another process holds the lock for all of uWaitMs (polled
///   every 50 ms), with the holder's content line copied into xHolderOut ("unknown holder" when it does not parse), and
///   DOCUMENT_ERROR_IO for any other failure to create or open the file; Acquire while held, or Release while not held,
///   is ZENITH_CHECK.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2. The file's path, open mode and content are ASSET's (Zenith2/Docs/Design/ASSET/Formats.md
///   "Cook lock file"); this interface is the framework's way to take it ("Storage-root writer lock").
/// @endcontract
class Zenith_DocumentWriterLock
{
public:
	virtual ~Zenith_DocumentWriterLock() = default;
	virtual Zenith_Result<void, Zenith_DocumentError> Acquire(uint32 uWaitMs, Zenith_Array<char>& xHolderOut) = 0;
	virtual void Release() = 0;
	virtual bool IsHeld() const = 0;
};

/// @contract interface Zenith_DocumentStorage
/// Ownership: Implemented by Zenith_MemoryDocumentStorage (this library) and Zenith_HostDocumentStorage
///   (ZenithDocumentHost); owned by the tool or test that creates it; borrowed by every framework object for its lifetime.
/// Thread affinity: EXTERNALLY_SYNCHRONISED. One migrator or writer uses a storage at a time.
/// Invalidation: None.
/// Units: Paths are storage paths; every implementation applies Zenith_ValidateStoragePath (rules P1 to P5 of
///   "Document paths") to every path argument, so the framework's journal, temporary and backup names are accepted;
///   Zenith_ValidateDocumentPath (P1 to P6) is the migrator's, never the storage's. Sizes in bytes.
/// Errors: Zenith_Result<…, Zenith_DocumentError>: INVALID_PATH, NOT_FOUND, IO, TOO_LARGE; never an assert on a path
///   or on stored bytes.
/// Reentrancy: NONE. List invokes its visitor, which never calls into the storage; a Zenith_ReentrancyGuard reports a
///   violation.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_DocumentStorage
{
public:
	virtual ~Zenith_DocumentStorage() = default;
	// The free bytes available to writes under the storage root (GetDiskFreeSpaceExW / statvfs; the desc's value for
	// the memory storage). A storage that cannot tell returns ~0ull.
	virtual Zenith_Result<uint64, Zenith_DocumentError> QueryFreeBytes() = 0;
	virtual Zenith_StringView GetDescription() const = 0;
	// The cross-process writer lock of the storage root: non-null for a writable host storage, null for a read-only one
	// and for Zenith_MemoryDocumentStorage, which no other process can reach.
	virtual Zenith_DocumentWriterLock* GetWriterLock() = 0;
	virtual Zenith_Result<void, Zenith_DocumentError> ReadWhole(Zenith_StringView strPath, uint64 ulMaxBytes,
		Zenith_Array<uint8>& axOut) = 0;
	virtual Zenith_Result<bool, Zenith_DocumentError> Exists(Zenith_StringView strPath) = 0;
	// Creates or truncates strPath, writes every byte, flushes to stable storage (FlushFileBuffers / fsync), closes.
	virtual Zenith_Result<void, Zenith_DocumentError> WriteFlushed(Zenith_StringView strPath, Zenith_ByteSpan xBytes) = 0;
	// Renames strFrom over strTo atomically and durably (write-through rename, then a directory flush).
	virtual Zenith_Result<void, Zenith_DocumentError> ReplaceAtomically(Zenith_StringView strFrom, Zenith_StringView strTo) = 0;
	// Copies strFrom to strTo (creating parent directories), flushed as WriteFlushed.
	virtual Zenith_Result<void, Zenith_DocumentError> CopyFlushed(Zenith_StringView strFrom, Zenith_StringView strTo) = 0;
	// Removes a file; a missing file succeeds.
	virtual Zenith_Result<void, Zenith_DocumentError> Delete(Zenith_StringView strPath) = 0;
	// Visits every file under strDirectory ("" = the root), recursively when bRecursive, in byte order of path.
	virtual Zenith_Result<void, Zenith_DocumentError> List(Zenith_StringView strDirectory, bool bRecursive,
		Zenith_PfnDocumentListVisitor pfnVisitor, void* pUserData) = 0;
};

/// @contract value Zenith_MemoryDocumentStorageDesc
/// Ownership: Plain value; m_pxHeap and m_pxFaults are borrowed and outlive the storage.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uCrashAfterMutations counts WriteFlushed, ReplaceAtomically, CopyFlushed and Delete calls.
/// Errors: A null heap is DOCUMENT_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_MemoryDocumentStorageDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_FaultInjector* const m_pxFaults = nullptr; // fault points below; null in production
	const uint32 m_uCrashAfterMutations = 0;          // 0 = never; K = the (K+1)th and every following mutation fails with no effect
	const bool m_bTornWriteOnCrash = false;            // the crashing WriteFlushed leaves the first half of its bytes
	const uint64 m_ulFreeBytes = ~0ull;               // what QueryFreeBytes reports (the low-disk tests lower it)
};

/// @contract type Zenith_MemoryDocumentStorage
/// Ownership: Created by Create as a Zenith_Owned<Zenith_MemoryDocumentStorage>; files live in the given heap.
/// Thread affinity: EXTERNALLY_SYNCHRONISED.
/// Invalidation: Spans returned by nothing; ReadWhole copies.
/// Units: Bytes.
/// Errors: As Zenith_DocumentStorage; a crashed storage returns DOCUMENT_ERROR_IO for every mutation until Revive.
/// Reentrancy: NONE. As Zenith_DocumentStorage (List's visitor).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_MemoryDocumentStorage final : public Zenith_DocumentStorage
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_MemoryDocumentStorage>, Zenith_DocumentError> Create(
		const Zenith_MemoryDocumentStorageDesc& xDesc);
	// Zenith_DocumentStorage overrides, all.
	void Revive(uint32 uCrashAfterMutations);   // models a restart: mutations work again, with a new crash budget
	uint32 GetMutationCount() const;
	uint32 GetFileCount() const;
};

inline constexpr uint32 uFAULT_POINT_DOCUMENT_STORAGE_WRITE = 0x444F0001u;
inline constexpr uint32 uFAULT_POINT_DOCUMENT_STORAGE_REPLACE = 0x444F0002u;
inline constexpr uint32 uFAULT_POINT_DOCUMENT_STORAGE_COPY = 0x444F0003u;
inline constexpr uint32 uFAULT_POINT_DOCUMENT_STORAGE_DELETE = 0x444F0004u;
```

`Zenith_MemoryDocumentStorage` is a real storage, not a test switch ([Z2-N-CONTRACT-415]): the preview writes nothing and needs none, but the editor's scratch documents, the cook's in-memory resolution and every framework test run on it; fault behaviour is reached only through the desc and the fault injector.

## Canonical serialization

Canonical serialization is the one byte form of an authoring document: stable key order, one ID-keyed child per line, the basis of CONTENT's structure-aware 3-way merge driver (Z2-I-CONTENT-28, Z2-D-038). It is schema-driven: a document type supplies a `Zenith_CanonicalSchema` describing each object's kind and member order, and `Zenith_WriteCanonicalJson` imposes it; `Zenith_JsonWriter` imposes no order (`Zenith2/Docs/Design/CORE/Text.md` → "JSON writer").

### Canonical rules

| Rule | Statement |
|---|---|
| C1 bytes | the byte format of CORE's `JSON_WRITE_STYLE_PRETTY`: UTF-8 without a byte-order mark, LF line endings, one tab per depth, `"key": value`, one trailing LF (`Zenith2/Docs/Design/CORE/Text.md` → "Byte format") |
| C2 object | known members in the order of the object's rule; a member absent from the input is not invented (a default is written only when the rule marks the member required) |
| C3 unknown members | a member whose key is not in the object's rule is unknown; its anchor is the nearest member before it in the tree's member order whose key is in the rule, or the object start when there is none; it is written directly after its anchor's value, and unknown members sharing an anchor keep their tree order ([Z2-N-CONTRACT-543]). The anchor is evaluated over the tree as it stands when `Zenith_WriteCanonicalJson` runs, never over the input bytes: § "Unknown members across migration steps" |
| C4 ID map | an object whose keys are persistent IDs (36-character canonical form, `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions"); members sorted by key byte order, which for the fixed-width lowercase form is numeric order of (high, low); each member starts on its own line; a key that is not a canonical persistent ID is `DOCUMENT_ERROR_SHAPE` |
| C5 keyed map | an object keyed by names that are not persistent IDs: stable names (the components of an entity) or any other non-empty UTF-8 key without control characters (for example ASSET's importer setting keys); members sorted by key byte order |
| C6 field set | an object keyed by decimal property IDs (the JSON field-set form, `Zenith2/Docs/Design/ECS/Reflect.md` → "JSON field-set form"); members sorted by numeric ID; nested structs are field sets |
| C7 inline array | vectors, quaternions, colours and arrays whose elements are all scalars: one line, `[a, b, c]` (CORE's `JSON_ARRAY_LAYOUT_INLINE`) |
| C8 ordered array | an array of objects or of arrays: one element per line, element order as authored (the order is data) |
| C9 numbers | an integer-valued number of an integer property is written as an integer; a float property is written with `Zenith_FormatFloat` (f32) or `Zenith_FormatDouble` (f64), the shortest round-trip text; an unknown member's numbers keep their raw text (`Zenith_JsonDocument::Write` writes raw number text) |
| C10 strings | CORE's escaping; persistent IDs, stable names and property IDs never need escapes |
| C11 idempotence | `Canonical(Parse(Canonical(x))) == Canonical(x)` byte for byte, for every accepted input |
| C12 no duplicates | duplicate keys are refused by the reader (`JSON_READ_ERROR_DUPLICATE_KEY`) |

<!-- informative -->
One ID-keyed child per line is achieved by making every collection of identified children an object keyed by the child's ID rather than an array: two authors who add entities to one scene add two members at two sorted positions, which a line merge and CONTENT's structure-aware merge both combine without a conflict, and no array index shifts. Author-defined sibling order, which an ID-sorted map cannot carry, is the entity's `sortKey` member (a fractional-index string, `Zenith2/Docs/Design/SCENE/Format.md` → "Sort keys").
<!-- /informative -->

### Zenith_CanonicalJson header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_CanonicalJson.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_JsonDocument.h"
#include "ZenithDocument/Zenith_DocumentErrors.h"

/// @contract enum Zenith_CanonicalNodeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_CanonicalNodeKind : uint8
{
	CANONICAL_NODE_OBJECT = 1,     // C2, C3
	CANONICAL_NODE_ID_MAP,         // C4
	CANONICAL_NODE_KEYED_MAP,      // C5
	CANONICAL_NODE_FIELD_SET,      // C6
	CANONICAL_NODE_INLINE_ARRAY,   // C7
	CANONICAL_NODE_ORDERED_ARRAY,  // C8
	CANONICAL_NODE_VALUE,          // any scalar; an unknown shape below is written verbatim
	CANONICAL_NODE_OPAQUE,         // written verbatim in canonical bytes (unknown components)
};

inline constexpr uint16 uCANONICAL_NO_RULE = 0xFFFF;

/// @contract value Zenith_CanonicalMember
/// Ownership: Plain value in a static table.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CanonicalMember
{
	const char* const m_szKey = nullptr;
	const uint16 m_uRule = uCANONICAL_NO_RULE; // the member value's rule; uCANONICAL_NO_RULE = CANONICAL_NODE_VALUE
	const bool m_bRequired = false;
};

/// @contract value Zenith_CanonicalRule
/// Ownership: Plain value in a static table.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CanonicalRule
{
	const Zenith_CanonicalNodeKind m_eKind = CANONICAL_NODE_OBJECT;
	const Zenith_Span<const Zenith_CanonicalMember> m_axMembers = {}; // OBJECT: known members in canonical order
	const uint16 m_uElementRule = uCANONICAL_NO_RULE;       // maps and arrays: the rule of every member or element
};

/// @contract value Zenith_CanonicalSchema
/// Ownership: Plain value; static tables owned by the document type's library.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Rule indices into m_axRules.
/// Errors: None; Zenith_ValidateCanonicalSchema checks the indices at registration.
/// Version: since Z2 Stage 2. A change to a schema's rules is a canonical-form change: it bumps the document type's
///   version and ships a migration step whose only effect is the rewrite.
/// @endcontract
struct Zenith_CanonicalSchema
{
	const char* const m_szName = nullptr;
	const Zenith_Span<const Zenith_CanonicalRule> m_axRules = {};
	const uint16 m_uRootRule = 0;
};

/// @contract functions Zenith_CanonicalJsonFunctions
/// Ownership: Output bytes are appended to the caller's array.
/// Thread affinity: ANY_THREAD; the document is read-only for the call.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: DOCUMENT_ERROR_SHAPE (a node whose kind contradicts its rule, a non-ID key in an ID map, a non-numeric key in a
///   field set), DOCUMENT_ERROR_WRITE, DOCUMENT_ERROR_OUT_OF_MEMORY; on failure xOut holds its previous content.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_WriteCanonicalJson(const Zenith_JsonDocument& xDocument,
	Zenith_JsonNode xRoot, const Zenith_CanonicalSchema& xSchema, Zenith_Array<char>& xOut);
Zenith_Result<bool, Zenith_DocumentError> Zenith_IsCanonicalJson(Zenith_ByteSpan xBytes,
	const Zenith_CanonicalSchema& xSchema);
Zenith_Result<void, Zenith_DocumentError> Zenith_ValidateCanonicalSchema(const Zenith_CanonicalSchema& xSchema);
```

### Canonical writing algorithm

`Zenith_WriteCanonicalJson(doc, root, schema, out)`:

1. Record `ulStart = out.GetCount()`; construct a `Zenith_JsonWriter` over a scratch array with `JSON_WRITE_STYLE_PRETTY`.
2. Visit `root` with rule `schema.m_uRootRule` (explicit stack of at most 64 frames, the JSON depth limit):
   - OBJECT: scan the object's members once in tree order, keeping `strAnchor` (initially the object start) and appending each unknown member to the list of `strAnchor`, and setting `strAnchor` to each known member's key as it is met; then build the output order: the object start's list, then for each known member of the rule in rule order that is present, the member followed by its list; write `Key` then visit the value with the member's rule (unknown members with `CANONICAL_NODE_OPAQUE`). The scan is O(members) with one hash probe per key into the rule's key table.
   - ID_MAP / KEYED_MAP / FIELD_SET: collect `(key, member index)`, validate keys (C4, C5 or C6), sort with `Zenith_Sort` (byte order; FIELD_SET by parsed `uint32`), write in that order.
   - INLINE_ARRAY: `BeginArray(JSON_ARRAY_LAYOUT_INLINE)`; ORDERED_ARRAY: `BeginArray(JSON_ARRAY_LAYOUT_MULTILINE)`; elements in input order.
   - VALUE: a number is written from its raw text when the member is unknown or the rule carries no numeric type, otherwise per C9; strings, booleans and null directly.
   - OPAQUE: `Zenith_JsonDocument::Write(writer, node)` (raw number text preserved; nested object members in input order).
3. `Finish()`; on success append the scratch bytes to `out`; on failure leave `out` unchanged and return the mapped error.

Complexity O(n log n) in the member count; memory O(n) scratch. `Zenith_IsCanonicalJson` parses (`Zenith_JsonLimits` defaults), writes canonically and compares bytes.

### Unknown members across migration steps

<!-- elaboration -->
Migration steps mutate the tree between the read and the write, so C3's anchor is defined over the tree at write time, which makes the output a pure function of the tree and keeps C11:

| What a step does | Effect on the unknown members' position |
|---|---|
| removes a known member that anchored unknown members (a field retired by the step, declared through the diagnostics' `m_pfnDeclareRemoved`) | its unknown followers re-anchor to the nearest preceding member of the tree that is still known, else to the object start; their order among themselves is unchanged |
| renames a known member in place (`RemoveMember` then `InsertMember` at the same index, which `Zenith_RenameMemberInPlace` of this library does) | the renamed member keeps its index, so its unknown followers keep it as their anchor |
| inserts a known member | unknown members before its index keep their anchor; those after it and before the next known member anchor to it |
| moves an unknown member to another object, reorders unknown members of one object among themselves, rewrites or deletes one | forbidden ([Z2-N-CONTRACT-545]): the unknown-member guard below fails the step; a change of an unknown member's anchor caused by edits of known members is not a move, because C3 places it by its anchor at write time |

The unknown-member guard runs around every step of every chain (document steps and the embedded field-set visitor alike). It compares the order of the surviving unknown members, never their absolute positions, because a declared removal shifts every member that follows it and an older step may insert members of its own:

1. Before the step, collect `U`: for every node of the tree whose rule in the type's current canonical schema is `CANONICAL_NODE_OBJECT` (maps and field sets have no unknown members in the C3 sense; an unknown property ID inside a field set is preserved by ECS's `Zenith_MigrateFieldSet`), the object's document pointer P and the list `U[P]` of its members whose keys are not in that rule, in tree order, each as (key, SHA-256 of the member value's compact `Zenith_JsonDocument::Write` bytes). Members inside an `OPAQUE` subtree are collected as one unit (the opaque root).
2. Run the step, collecting in `R` every document pointer it passes to `m_pfnDeclareRemoved`; a pointer failing `Zenith_ValidateDocumentPointer` fails the step.
3. After the step, for every P of `U` such that neither P nor any proper prefix of P (token-wise) is in `R`:
   1. P must still name an object;
   2. `W` = the keys of `U[P]` in their pre-step order, minus each key k whose member pointer (P followed by the token of k) is in `R`;
   3. `V` = the keys of the object's members that are not in its rule and are in `W`, in post-step tree order;
   4. require every key of `W` to occur exactly once in the object, `V == W` as sequences, and each surviving member's digest to equal its pre-step digest.
   Members of the object that `U[P]` does not hold — inserted by the step, including members of an older version that the current schema does not know — are not checked. Any failure is `DOCUMENT_ERROR_STEP_FAILED` for that step, logged as `document: <path>: step <from>-><from + 1> of <type> deleted, moved or changed unknown member <pointer>`; the tree is discarded and the stored bytes are untouched.
4. A member removed by a declared pointer (the step called `m_pfnDeclareRemoved` of its `Zenith_MigrationDiagnostics` with the member's document pointer before removing it) is exempt: that is the "explicit step operation" of [Z2-N-CONTRACT-545], and the plan's entry lists the pointer among the changed paths. A step that renames or rewrites a member of an older version that the current schema no longer knows (a field on its way to its current name or shape) declares the old pointer removed in the same way, because to the guard that member is indistinguishable from an unknown one.

Complexity O(members + unknown members × depth) per step; the guard is skipped for a document with no unknown member (the common case), detected in the same scan that opens it. `Test_CanonicalJson_UnknownMemberAnchorRemovedByStep`, `Test_DocumentMigrator_StepCannotDeleteUnknownField` and `Test_DocumentMigrator_DeclaredRemovalIsAllowed` assert the three halves (re-anchoring, the refusals, and the declared removal and older-version insertion that must pass).

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_CanonicalJson.h (continued)
/// @contract functions Zenith_CanonicalTreeEdits
/// Ownership: Edits the caller's document tree.
/// Thread affinity: OWNER_THREAD. The thread running the step.
/// Invalidation: Nodes of the object obtained before the call remain valid; member indices after the renamed one are
///   unchanged.
/// Units: None.
/// Errors: DOCUMENT_ERROR_SHAPE (xObject is not an object, strFrom absent, strTo already present),
///   DOCUMENT_ERROR_OUT_OF_MEMORY; on failure the tree is unchanged.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RenameMemberInPlace(Zenith_JsonDocument& xDocument,
	Zenith_JsonNode xObject, Zenith_StringView strFrom, Zenith_StringView strTo);
```

## Zenith_MigrationRegistry header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_MigrationRegistry.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithReflect/Zenith_Migration.h"      // Zenith_PfnMigrationStep, Zenith_MigrationContext, Zenith_MigrationError (ECS)
#include "ZenithDocument/Zenith_CanonicalJson.h"

class Zenith_DocumentTree;

/// @contract functions Zenith_EmbeddedFieldSetVisitor
/// Ownership: A function pointer a document type supplies in Zenith_DocumentTypeDesc::m_pfnVisitFieldSets, with the desc's
///   m_pUserData; owns nothing.
/// Thread affinity: OWNER_THREAD. Invoked by the migrator on its thread, after a document's chain and when a current
///   document opens.
/// Invalidation: xDocument is valid for the call only.
/// Units: None.
/// Errors: Returns the first Zenith_MigrationError of the embedded chains it runs; the migrator maps it to
///   DOCUMENT_ERROR_STEP_FAILED and discards the tree.
/// Reentrancy: NONE. The visitor never calls back into the migrator; the unknown-member guard runs around it.
/// Version: since Z2 Stage 2.
/// @endcontract
// Visits every separately versioned object embedded in a document tree (component field sets; ASSET's importer
// settings), so their chains run inside documents.
using Zenith_PfnVisitEmbeddedFieldSets = Zenith_Result<void, Zenith_MigrationError> (*)(Zenith_DocumentTree& xDocument,
	const Zenith_MigrationContext& xContext, void* pUserData);

/// @contract value Zenith_DocumentTypeDesc
/// Ownership: Plain value; strings, the schema and m_pUserData are borrowed and outlive the registry.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Versions are uint32 from 1.
/// Errors: Validated by RegisterDocumentType (DOCUMENT_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentTypeDesc
{
	const char* const m_szStableName = nullptr;  // "Zenith.SceneDocument"; the type ID is Zenith_MakeTypeID of it
	const char* const m_szSchemaTag = nullptr;   // the "$schema" value ("zenith2.scene"); null for types without one (game.json)
	const char* const m_szVersionKey = "schemaVersion";
	const char* const m_szIDKey = "id";          // null: the document ID is Zenith_MakePathDocumentID(path)
	const Zenith_Span<const char* const> m_aszSuffixes = {}; // ".scene.json"; an exact file name is written with a leading '/' ("/game.json");
	                                              // a suffix matches when "/" + the storage path ends with it
	const uint32 m_uCurrentVersion = 1;
	const Zenith_CanonicalSchema* const m_pxCanonical = nullptr;
	const Zenith_PfnVisitEmbeddedFieldSets m_pfnVisitFieldSets = nullptr; // null: the type embeds no separately versioned object
	                                                                 // (component field sets; ASSET's importer settings)
	void* const m_pUserData = nullptr;
};

/// @contract type Zenith_MigrationRegistry
/// Ownership: Created by Create as a Zenith_Owned<Zenith_MigrationRegistry>; one per tool process (editor, cook,
///   ZenithNewGame) or test, owned by its composition root; storage from the given heap.
/// Thread affinity: MAIN_THREAD until Freeze; ANY_THREAD and IMMUTABLE afterwards.
/// Invalidation: Document type IDs are valid for the registry's lifetime.
/// Units: Versions.
/// Errors: RegisterDocumentType returns DOCUMENT_ERROR_DUPLICATE_REGISTRATION for a stable name, schema tag or suffix
///   already registered, DOCUMENT_ERROR_INVALID_DESC for a null stable name, version 0, no suffix or a canonical
///   schema failing Zenith_ValidateCanonicalSchema, DOCUMENT_ERROR_REGISTRY_FROZEN after Freeze. Freeze returns
///   DOCUMENT_ERROR_CHAIN_GAP naming the type and the missing version ([Z2-N-CONTRACT-539]).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_MigrationRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_MigrationRegistry>, Zenith_DocumentError> Create(Zenith_Heap& xHeap);
	Zenith_Result<Zenith_TypeID, Zenith_DocumentError> RegisterDocumentType(const Zenith_DocumentTypeDesc& xDesc);
	Zenith_Result<void, Zenith_DocumentError> Freeze();
	bool IsFrozen() const;
	const Zenith_DocumentTypeDesc* FindType(Zenith_TypeID xType) const;
	const Zenith_DocumentTypeDesc* FindTypeBySchemaTag(Zenith_StringView strTag) const;
	const Zenith_DocumentTypeDesc* FindTypeByPath(Zenith_StringView strPath) const;   // longest suffix of "/" + strPath
	Zenith_PfnMigrationStep FindStep(Zenith_TypeID xType, uint32 uFromVersion) const;  // null when absent
	uint32 GetTypeCount() const;
};

/// @contract functions Zenith_MigrationStepRegistration
/// Ownership: Registers into the caller's registry.
/// Thread affinity: MAIN_THREAD, before Freeze.
/// Invalidation: None.
/// Units: uFromVersion migrates uFromVersion to uFromVersion + 1.
/// Errors: MIGRATION_ERROR_BAD_INPUT for an unregistered document type, a from-version of 0 or at or above the type's
///   current version, a null step, a duplicate (type, from-version), or a frozen registry.
/// Version: since Z2 Stage 2. The signature is Zenith2/Docs/Contracts.md "Compatibility policy per kind" verbatim.
/// @endcontract
Zenith_Result<void, Zenith_MigrationError> Zenith_RegisterMigrationStep(
	Zenith_MigrationRegistry& xRegistry, Zenith_TypeID xDocumentType, uint32 uFromVersion,
	Zenith_PfnMigrationStep pfnStep);

Zenith_PersistentID Zenith_MakePathDocumentID(Zenith_StringView strPath);   // for types without an ID key
```

`FindTypeByPath(strPath)` compares every registered suffix against the string `"/" + strPath`, because a storage path never starts with `/` (rule P2): the root document `game.json` then matches the exact-name suffix `"/game.json"`, `Scenes/Main.scene.json` matches `".scene.json"`, and a file named `pregame.json` matches neither; the longest matching suffix wins. `Test_MigrationRegistry_SuffixMatchesRootAndNested` asserts the three cases.

A document type's ID is `Zenith_MakeTypeID(m_szStableName)` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_ReflectIDs header", [Z2-N-CONTRACT-373]), so a reflected type registered under the same stable name (MOD's `Zenith.GameJson` in a scratch `Zenith_TypeRegistry`) has the same ID, and a step registration made with either ID reaches the same chain.

`Zenith_MakePathDocumentID(strPath)` is the document ID of a type without an ID key: the first 16 bytes of the SHA-256 of the 8 ASCII bytes `"ZDOCPATH"` followed by the path's UTF-8 bytes (a `Zenith_Sha256Stream` updated with both, § "Digests"), read as a `Zenith_PersistentID` in its cooked binary form ([Z2-N-CONTRACT-067]), with the version-4 bits forced exactly as `Zenith_DeriveAuthoringID` step 3 forces them (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"). It depends on no open decision, is stable across runs, machines and tools, and is never null. Such an ID is never stored in a document; it names the document in migration contexts, warnings and reports only.

### Document tree

Contracts' step signature takes a `Zenith_DocumentTree&`. CORE declares it in `Zenith2/Engine/ZenithCore/Zenith_DocumentTree.h` (`Zenith2/Docs/Design/CORE/Text.md` → "JSON document tree", subsections "Zenith_DocumentTree declarations" and "Pointer resolution steps"; cross-area notes C13 and S5), and SCENE uses that declaration unchanged: a non-owning cursor `Zenith_DocumentTree(Zenith_JsonDocument&, Zenith_JsonNode)` with `GetDocument`, `GetRoot` and `FindPath`. SCENE declares no copy of it.

`FindPath` accepts a document pointer relative to the cursor's root (rules D1 to D4 of § "Document pointers", which are CORE's rules G1 to G4, for example `/entities/<id>/components/Zenith.LocalTransform`) and never asserts on the pointer's bytes. Its errors, which SCENE's migrator, guard and tests rely on:

| Pointer | `Zenith_JsonAccessError` |
|---|---|
| malformed: no leading `/`, a `~` not followed by `0` or `1`, invalid UTF-8, or more than `uJSON_POINTER_MAX_BYTES` (4,096) bytes — checked before any node is visited | `JSON_ACCESS_ERROR_INVALID_POINTER` |
| a well-formed object token naming no member | `JSON_ACCESS_ERROR_MISSING_MEMBER` |
| a token that meets an array and is not an element index below its length (rule D3: a leading zero, `-`, a sign, a non-digit, or an index at or beyond the length) | `JSON_ACCESS_ERROR_INDEX_OUT_OF_RANGE` |
| a token applied to a null, bool, number or string | `JSON_ACCESS_ERROR_WRONG_KIND` |

`uDOCUMENT_MAX_POINTER_BYTES` is defined as `uJSON_POINTER_MAX_BYTES`, so `Zenith_ValidateDocumentPointer` and `FindPath` share one limit, and `FindPath`'s parser implements exactly the grammar `Zenith_ValidateDocumentPointer` checks plus rule D3.

<!-- assumption: pending ECS -->
`Zenith_MigrationDiagnostics` is listed by `Zenith2/Docs/Design/ECS/Reflect.md` → "Header list" in `ZenithReflect/Zenith_Migration.h` and only forward-declared there. SCENE's migrator fills one per document with: `Zenith_Arena* m_pxArena` (the document's scratch), `Zenith_StringTable* m_pxStrings` (may be null in tools that intern nothing), `Zenith_MigrationScriptHost* m_pxScriptHost` (the editor's, null in the cook's dry runs, which then fail a `script` op with `MIGRATION_ERROR_SCRIPT`), a warning sink `void (*m_pfnWarn)(void* pUserData, uint8 uWarning, Zenith_StringView strFieldPath, uint32 uWrittenByVersion)` with `void* m_pWarnUserData`, where `uWarning` 1 is the unknown-field warning, and a removal declaration `void (*m_pfnDeclareRemoved)(void* pUserData, Zenith_StringView strFieldPath)` sharing `m_pWarnUserData`, which a document step calls before it removes, renames or rewrites an object member the current schema does not know (§ "Unknown members across migration steps"). In both callbacks `strFieldPath` is a document pointer (§ "Document pointers"): a document step passes it from the document root; when ECS's field-set migration reports an unknown field it passes the pointer relative to the field set in the same grammar, and the embedded visitor's forwarding sink prefixes the field set's own pointer. ECS's schema ops never call `m_pfnDeclareRemoved`: they act inside field sets, which hold no unknown member in the guard's sense. Cross-area notes S6 and S18.
<!-- /assumption -->

## Zenith_DocumentMigrator header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentMigrator.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_String.h"
#include "ZenithDocument/Zenith_DocumentStorage.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithDocument/Zenith_DocumentDiff.h"
#include "ZenithCore/Zenith_Sha256.h"      // Zenith_Sha256Value (§ "Digests")

class Zenith_TypeRegistry;
class Zenith_StringTable;
class Zenith_MigrationScriptHost;

/// @contract value Zenith_DocumentMigratorDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the migrator.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_szBatchTimestamp is UTC "YYYYMMDDTHHMMSSZ" (16 characters), supplied by the tool (runtime code reads no clock).
/// Errors: A null registry, storage or heap, an unfrozen registry or a malformed timestamp is DOCUMENT_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentMigratorDesc
{
	const Zenith_MigrationRegistry* const m_pxRegistry = nullptr;
	Zenith_DocumentStorage* const m_pxStorage = nullptr;
	const Zenith_TypeRegistry* const m_pxTypes = nullptr; // for component chains; null when no type embeds field sets
	Zenith_StringTable* const m_pxStrings = nullptr;
	Zenith_MigrationScriptHost* const m_pxScriptHost = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
	const char* const m_szBatchTimestamp = nullptr;
	const char* const m_szBackupRoot = ".migration/backups";
	const uint32 m_uSaveBackupRetention = 20;             // save batches kept under <root>/save-*; migration batches are never pruned
	const uint32 m_uWriterLockWaitMs = 10000;            // how long a writing member waits for the storage root's writer lock
	Zenith_FaultInjector* const m_pxFaults = nullptr;
};

/// @contract enum Zenith_DocumentOpenState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_DocumentOpenState : uint8
{
	DOCUMENT_OPEN_CURRENT = 1,     // at the current version; writable
	DOCUMENT_OPEN_MIGRATED,        // migrated in memory from an older version; writable; writing is a migration
	DOCUMENT_OPEN_READ_ONLY_NEWER, // newer than the tool ([Z2-N-CONTRACT-537])
};

/// @contract value Zenith_DocumentWarning
/// Ownership: Plain value; m_strFieldPath points into the owning report's text arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its report.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentWarning
{
	const Zenith_DocumentWarningCode m_eCode = {};
	const uint32 m_uDocumentIndex = 0;
	const uint32 m_uWrittenByVersion = 0;
	const Zenith_StringView m_strFieldPath = {}; // a document pointer (§ "Document pointers")
};

/// @contract type Zenith_OpenedDocument
/// Ownership: Owned by the caller; owns its parsed tree and warnings.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: The tree is invalid after Reset or destruction.
/// Units: Versions; SHA-256 digests of the bytes read.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_OpenedDocument
{
public:
	Zenith_DocumentOpenState GetState() const;
	Zenith_TypeID GetType() const;
	Zenith_PersistentID GetDocumentID() const;
	uint32 GetFileVersion() const;
	Zenith_JsonDocument& GetTree();            // at the current version unless READ_ONLY_NEWER
	const Zenith_Sha256Value& GetFileDigest() const;
	Zenith_Span<const Zenith_DocumentWarning> GetWarnings() const;
	void Reset();
};

/// @contract value Zenith_MigrationPlanEntry
/// Ownership: Owned by its Zenith_MigrationPlan; spans point into the plan's arrays.
/// Thread affinity: IMMUTABLE after PlanMigration returns. Members non-const: PlanMigration fills each entry field by
///   field as it plans the document.
/// Invalidation: With the plan.
/// Units: Versions; bytes; SHA-256 digests.
/// Errors: m_bBlocked is true for an entry that cannot migrate (m_xBlockingError says why); ApplyPlan refuses a plan
///   with any blocked entry.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_MigrationPlanEntry
{
	Zenith_StringView m_strPath;
	Zenith_TypeID m_xType;
	uint32 m_uFromVersion;
	uint32 m_uToVersion;
	Zenith_Sha256Value m_xOldDigest;
	Zenith_Sha256Value m_xNewDigest;
	Zenith_ByteSpan m_xNewBytes;              // canonical bytes the apply writes
	Zenith_DocumentDiff m_xDiff;              // the dry-run diff the preview shows
	Zenith_Span<const Zenith_DocumentWarning> m_axWarnings;
	bool m_bChanged;                          // false: already current and canonical; not written
	bool m_bBlocked;
	Zenith_DocumentError m_xBlockingError;
};

/// @contract type Zenith_MigrationPlan
/// Ownership: Owned by the caller; holds every entry's bytes, diffs and warnings in its own storage.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after Reset; stale (ApplyPlan fails DOCUMENT_ERROR_STALE_PLAN) once any listed document changes.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_MigrationPlan
{
public:
	Zenith_Span<const Zenith_MigrationPlanEntry> GetEntries() const;
	uint32 GetChangedCount() const;
	uint32 GetBlockedCount() const;
	void Reset();
	Zenith_Result<void, Zenith_DocumentError> WriteReportJson(Zenith_Array<char>& xOut) const;  // "zenith2.migration-preview" v1
};

/// @contract value Zenith_DocumentWrite
/// Ownership: Plain value; borrows the bytes for the call.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of the meta writer.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentWrite
{
	Zenith_StringView m_strPath;
	Zenith_ByteSpan m_xBytes;                 // canonical bytes of a current-version document; empty = delete the document
	const Zenith_Sha256Value* m_pxExpectedOldDigest;  // null for a new document
};

/// @contract value Zenith_RecoveryReport
/// Ownership: Plain value filled by RecoverPendingJournals.
/// Thread affinity: IMMUTABLE after the call. Members non-const: filled through the out-parameter of
///   RecoverPendingJournals.
/// Invalidation: None.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RecoveryReport
{
	uint32 m_uJournalsFound = 0;
	uint32 m_uRolledBack = 0;
	uint32 m_uRolledForward = 0;
	uint32 m_uEntriesRestored = 0;
};

/// @contract type Zenith_DocumentMigrator
/// Ownership: Created by Create as a Zenith_Owned<Zenith_DocumentMigrator>; owned by a tool session; borrows the desc's
///   objects.
/// Thread affinity: OWNER_THREAD. Every member runs on the creating thread; the editor calls it between frames, the
///   cook and ZenithNewGame on their main thread.
/// Invalidation: None of its own; a plan is invalidated by any storage change it did not make.
/// Units: None.
/// Errors: Every member returns Zenith_Result<…, Zenith_DocumentError>. RecoverPendingJournals must succeed before any
///   other member; until it has run, the others return DOCUMENT_ERROR_JOURNAL_PENDING when a journal exists. Over a
///   read-only storage (the cook's) RecoverPendingJournals changes nothing and returns DOCUMENT_ERROR_JOURNAL_PENDING
///   naming the first journal when one exists. Untrusted document bytes never assert ([Z2-N-CONTRACT-472]).
/// Reentrancy: NONE. The migrator invokes migration steps and embedded field-set visitors, which never call back into
///   it; a Zenith_ReentrancyGuard reports a violation.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_DocumentMigrator
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DocumentMigrator>, Zenith_DocumentError> Create(
		const Zenith_DocumentMigratorDesc& xDesc);
	Zenith_Result<void, Zenith_DocumentError> RecoverPendingJournals(Zenith_RecoveryReport& xOut);
	Zenith_Result<void, Zenith_DocumentError> Open(Zenith_StringView strPath, Zenith_OpenedDocument& xOut);
	Zenith_Result<void, Zenith_DocumentError> PlanMigration(Zenith_Span<const Zenith_StringView> astrPaths,
		Zenith_MigrationPlan& xOut);
	Zenith_Result<void, Zenith_DocumentError> PlanMigrationOfDirectory(Zenith_StringView strDirectory,
		Zenith_MigrationPlan& xOut);             // every file under it whose suffix a registered type claims
	Zenith_Result<void, Zenith_DocumentError> ApplyPlan(const Zenith_MigrationPlan& xPlan);
	Zenith_Result<void, Zenith_DocumentError> SaveDocuments(Zenith_Span<const Zenith_DocumentWrite> axWrites);
	Zenith_Result<void, Zenith_DocumentError> Serialize(const Zenith_OpenedDocument& xDocument,
		Zenith_Array<char>& xOut) const;         // the canonical bytes of the document's tree
};

inline constexpr uint32 uFAULT_POINT_MIGRATOR_JOURNAL_PREPARED = 0x444F0010u;
inline constexpr uint32 uFAULT_POINT_MIGRATOR_JOURNAL_BACKED_UP = 0x444F0011u;
inline constexpr uint32 uFAULT_POINT_MIGRATOR_JOURNAL_WRITTEN = 0x444F0012u;
inline constexpr uint32 uFAULT_POINT_MIGRATOR_JOURNAL_COMMITTING = 0x444F0013u;
```

## Zenith_DocumentDiff header

```cpp
// Zenith2/Engine/ZenithDocument/Zenith_DocumentDiff.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract value Zenith_DiffHunk
/// Ownership: Plain value; line spans point into the owning plan's text.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the owning plan.
/// Units: 1-based line numbers of the old and new texts.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DiffHunk
{
	const uint32 m_uOldStart = 0;
	const uint32 m_uOldCount = 0;
	const uint32 m_uNewStart = 0;
	const uint32 m_uNewCount = 0;
	const Zenith_Span<const Zenith_StringView> m_astrLines = {}; // each prefixed ' ', '-' or '+', three lines of context
};

/// @contract value Zenith_DocumentDiff
/// Ownership: Plain value; spans point into the owning plan.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of the diff builder.
/// Invalidation: With the owning plan.
/// Units: Lines; document pointers (DocumentFramework.md "Document pointers").
/// Errors: m_bTruncated is true when the edit distance exceeded uDIFF_MAX_EDIT_DISTANCE; the hunks then show the
///   whole old and new texts as one replacement.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_DocumentDiff
{
	Zenith_Span<const Zenith_DiffHunk> m_axHunks;
	Zenith_Span<const Zenith_StringView> m_astrChangedPaths;   // structural summary: document pointers whose value differs
	bool m_bTruncated;
};

inline constexpr uint32 uDIFF_MAX_EDIT_DISTANCE = 100000;
```

The line diff is Myers' O((N + M)·D) algorithm over lines with three lines of context; the structural summary walks both trees in canonical order and lists the document pointer of every node whose value differs (a changed leaf, an added or removed member), sorted by byte order of the pointer, plus every pointer a step declared removed.

## Digests

Every digest of this framework (an opened document's file digest, a plan entry's old and new digests, a write's expected digest, a journal's hex digests, the batch name, `Zenith_MakePathDocumentID`, the unknown-member guard's member digests) and SCENE's derived authoring IDs (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation") use CORE's SHA-256 (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_Sha256", "Zenith_Sha256 declarations"): the 32-byte `Zenith_Sha256Value`, the one-shot `Zenith_Sha256Digest` (pointer and `Zenith_ByteSpan` forms), `Zenith_Sha256Stream` where a digest covers several spans (one `Update` per span, then `Finish`), and `Zenith_FormatSha256Hex` / `Zenith_ParseSha256Hex` for the 64-character lowercase text of journals and reports. SHA-256 is outside D-SEL-HASH (open, Stage 1) because its output is persisted (`Zenith2/Docs/Design/CORE/Maths.md` → "SHA-256 outside D-SEL-HASH"), and ZenithPlatform verifies patch archives with the same functions (cross-area row S23), so the engine holds one implementation; ZenithDocument and ZenithScene declare none. CORE's `Test_Sha256_NistVectors` and `Test_Sha256_StreamEqualsOneShot` cover the functions.

## Migration algorithms

### Opening a document

`Open(path, out)`:

1. `Zenith_ValidateDocumentPath(path)` (P1 to P6, so a framework file is never opened as a document); `ReadWhole(path, 64 MiB)` (the JSON limit of `Zenith2/Docs/Architecture.md` → "Security architecture", "Parser limits").
2. `Zenith_JsonDocument::Parse` with `Zenith_JsonLimits` defaults; failure → `DOCUMENT_ERROR_PARSE` with line and column.
3. Identify the type: the root's `"$schema"` string through `FindTypeBySchemaTag`, else the path through `FindTypeByPath`; neither → `DOCUMENT_ERROR_UNKNOWN_TYPE`. A tag and a suffix naming different types is `DOCUMENT_ERROR_SHAPE`.
4. Read the version member (`m_szVersionKey`): absent, not an integer or 0 → `DOCUMENT_ERROR_SHAPE`. Read the ID member when `m_szIDKey` is set: absent or not a canonical persistent ID → `DOCUMENT_ERROR_SHAPE`.
5. Version above `m_uCurrentVersion` → state `DOCUMENT_OPEN_READ_ONLY_NEWER`, warning logged, tree kept unmodified, return success (the caller may display it; every write refuses it).
6. Version below: run the chain (§ "Running a chain"); state `DOCUMENT_OPEN_MIGRATED`.
7. Equal: state `DOCUMENT_OPEN_CURRENT`; still run the type's embedded field-set visitor, which migrates components whose own schema version is older (`Zenith_MigrateFieldSet` through ECS's chain) — a component newer than the registry's type sets `DOCUMENT_OPEN_READ_ONLY_NEWER` with `DOCUMENT_WARNING_NEWER_COMPONENT_VERSION`; a component migrated this way changes the state to `DOCUMENT_OPEN_MIGRATED`.
8. Record the SHA-256 of the bytes read; record `DOCUMENT_WARNING_NOT_CANONICAL` when the input bytes differ from `Serialize`'s output.

### Running a chain

For v from the file's version to `m_uCurrentVersion − 1`:

1. `step = FindStep(type, v)`; null → `DOCUMENT_ERROR_CHAIN_GAP` (the registry's `Freeze` makes this unreachable for a registered type, [Z2-N-CONTRACT-539]).
2. Build `Zenith_MigrationContext { documentID, type, v, &diagnostics }` and a `Zenith_DocumentTree` over the root.
3. Call the step. An error → `DOCUMENT_ERROR_STEP_FAILED`, the tree is discarded (the step works on the in-memory copy; the stored bytes are untouched).
4. Set the version member to v + 1.

After the last step, run the embedded field-set visitor (component chains). Steps are pure functions of the tree ([Z2-N-CONTRACT-538]); the framework enforces the consequence by planning twice (below).

### Planning a migration

`PlanMigration(paths, plan)`: for each path in byte order: `Open`; READ_ONLY_NEWER → blocked entry (`DOCUMENT_ERROR_NEWER_VERSION`); an open error → blocked entry with that error; otherwise `Serialize` to canonical bytes, then a second, independent `Open` + `Serialize` of the same input bytes; differing outputs → blocked `DOCUMENT_ERROR_NOT_DETERMINISTIC`; compute the new digest, the diff against the input bytes, and `m_bChanged = new bytes ≠ old bytes`. Warnings of both kinds of preservation are copied into the entry: every unknown field (one per document per field path per load, [Z2-N-CONTRACT-544]) and every dangling override (`DOCUMENT_WARNING_DANGLING_OVERRIDE`, [Z2-N-CONTRACT-547]). Nothing is written: this is the upgrade preview ([Z2-N-CONTRACT-531]), and `WriteReportJson` is the data EDIT's migration UI and `ZenithNewGame --migrate --preview` display.

### Applying a plan

`ApplyPlan(plan)`: refuse a plan with a blocked entry (the first entry's error); for each changed entry re-read the current bytes and compare their digest with `m_xOldDigest` (`DOCUMENT_ERROR_STALE_PLAN` on the first mismatch, nothing written); then run the journaled write (§ "Journaled write") over the changed entries with backup root `<m_szBackupRoot>/<m_szBatchTimestamp>/` — the automatic backup before any document is written ([Z2-N-CONTRACT-532]).

`SaveDocuments(writes)` is the ordinary editor save: each write is checked against its expected old digest (a mismatch is `DOCUMENT_ERROR_STALE_PLAN`, so an external edit is never overwritten), and the journaled write runs with backup root `<m_szBackupRoot>/save-<timestamp>/`; after success, save batches beyond `m_uSaveBackupRetention` are deleted oldest first ([Z2-N-CONTRACT-540] applies to every authoring-document write). A write whose document type opened read-only is `DOCUMENT_ERROR_READ_ONLY`.

### Journaled write

The steps and recovery are Contracts' tables ([Z2-N-CONTRACT-540]); SCENE fixes the file names, the journal's own atomicity and the low-disk refusal.

| Step | Framework action | Journal state |
|---|---|---|
| 0a | refuse when any `*.z2journal` exists at the document storage's root (`List("", false, …)`): `DOCUMENT_ERROR_JOURNAL_PENDING`, nothing written | — |
| 0b | the low-disk threshold, stated here once: `QueryFreeBytes()` of the storage must be at least `ulRequired = 2 × Σnew + Σold`, where Σnew is the sum of the new bytes of the batch's entries (step 3 writes each once as a temporary, which step 4 renames over its document) and Σold the sum of the sizes of the existing documents step 2 copies to backups; the factor 2 on Σnew is the headroom for the journal rewrites and the file system's allocation granularity. Below it: `DOCUMENT_ERROR_LOW_DISK` logged with the storage, `ulRequired` and the free bytes; nothing written. A storage that cannot tell (`~0ull`) always passes | — |
| 1 | write `<batch>.z2journal` at the document storage's root (§ "Journal format") through `WriteFlushed` of `<batch>.z2journal.z2tmp` then `ReplaceAtomically` | `PREPARED` |
| 2 | `CopyFlushed(path, <backup root>/<path>)` for every entry whose document exists; rewrite the journal (tmp + replace) | `BACKED_UP` |
| 3 | `WriteFlushed(<path>.z2tmp, new bytes)` for every entry; rewrite the journal | `WRITTEN` |
| 4 | `ReplaceAtomically(<path>.z2tmp, <path>)` in entry order, rewriting the journal after each rename with the entry's `renamed` flag set; a deletion entry is `Delete(path)` | `COMMITTING` |
| 5 | `Delete(<batch>.z2journal)` | none |

<!-- assumption: pending X109(d) -->
Contracts' step 1 writes the journal "beside the documents"; this framework writes it at the storage root (`Games/<Name>/` for a game's storage) because one batch spans directories (a scene and the prefabs it instances) and recovery then scans one location. Cross-doc item X109(d) rewords Contracts' step 1 accordingly; MOD's "`<batch>.z2journal` beside `game.json`" holds trivially, because `game.json` sits at the root.
<!-- /assumption -->

`<batch>` is `<m_szBatchTimestamp>-<first 8 hex digits of SHA-256 over the sorted entry paths, each followed by LF>`. Recovery (`RecoverPendingJournals`), for each journal at the storage root in byte order of name:

| State found | Action |
|---|---|
| unreadable or invalid journal | `DOCUMENT_ERROR_JOURNAL_CORRUPT`; nothing is changed; the tool refuses to write until a person resolves it (the error names the journal) |
| `PREPARED`, `BACKED_UP` | delete every `<path>.z2tmp`; delete the journal (roll back; the originals are untouched) |
| `WRITTEN` | when every `<path>.z2tmp` exists with its recorded new digest, continue as `COMMITTING`; otherwise roll back as above |
| `COMMITTING` | for each entry in order: target digest = new → counted renamed; `.z2tmp` digest = new → `ReplaceAtomically`; target digest = old and no valid tmp → restore from the entry's backup path and roll the whole batch back from the backups; anything else → `DOCUMENT_ERROR_RECOVERY_CONFLICT` naming the path; then delete the journal |
| any state, read-only document storage | nothing is changed; `DOCUMENT_ERROR_JOURNAL_PENDING` names the first journal (the cook's refusal, § "Ownership split with ASSET") |

Re-running recovery after an interruption during recovery converges, because every action is idempotent and the journal is deleted last. `Test_JournaledWrite_InterruptedAtEachStep_Recovers` ([Z2-N-CONTRACT-542]) drives it (`Zenith2/Docs/Design/SCENE/Tests.md` → "Document framework tests").

### Storage-root writer lock

<!-- elaboration -->
ZenithEditor saves scenes and prefabs into the same storage roots ZenithCook's sidecar-writing modes write (the game directory and each engine library's `Internal/Content/`), so without a cross-process guard one process's `RecoverPendingJournals` could roll the other's live batch forward or back, and two writers could race a check-then-write (cross-area row AS31). Ownership is split this way: **SCENE owns the lock type** (`Zenith_DocumentWriterLock`, returned by `Zenith_DocumentStorage::GetWriterLock` and implemented by ZenithDocumentHost), and **ASSET owns the lock file**: its path `<storage root>/.migration/writer.lock`, its open mode and its one-line content (`Zenith2/Docs/Design/ASSET/Formats.md` → "Cook lock file"; ASSET's holders are `Zenith2/Docs/Design/ASSET/Cook.md` → "Output tree and database locks" item 5). ASSET's writers take the lock through the same `GetWriterLock()` of their host storage, so one implementation serves both.

Hold rule:

1. Over a storage whose `GetWriterLock()` is non-null, the migrator's writing members `RecoverPendingJournals`, `ApplyPlan` and `SaveDocuments` take the lock first, with `Acquire(m_uWriterLockWaitMs, …)`, before reading anything they will decide on, and release it when they return, on success and on every failure. The lock is held per batch, never for an editor session.
2. A member that finds the lock already held by its own caller (`IsHeld()`, because a ZenithCook mode holds the root for its run or a server for one request) uses that hold and does not release it; it releases only what it took.
3. Recovery runs only under the lock. Every journal is created (step 1) and deleted (step 5) inside its writer's hold, so a journal a holder finds was left by a writer that has exited, and recovery never touches a live batch. Step 0a's journal check, the expected-digest checks of `SaveDocuments` and the stale-plan check of `ApplyPlan` run inside the same hold, so no other writer can change a document between the check and the write.
4. Refused for the whole wait: the member returns `DOCUMENT_ERROR_STORAGE_LOCKED`, logged with the root and the holder's PID and mode as "another tool is writing this content root; retry when it finishes"; nothing is recovered, read to decide or written. The editor keeps the documents dirty and reports the error (EDIT's presentation).
5. Reading members (`Open`, `PlanMigration`, `PlanMigrationOfDirectory`) take no lock. A plan made outside the hold is re-validated inside it by `ApplyPlan`'s stale-plan check.

The mechanism, in ZenithDocumentHost's `Internal/Windows/Zenith_HostDocumentStorage_Windows.cpp`, is the one ASSET's table states: `Acquire` creates `.migration/` under the root, opens `.migration/writer.lock` with `CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)`, truncates it and writes `{"pid": <u32>, "mode": <m_szWriterMode>, "game": <m_szGame>, "platform": <m_szPlatform or null>}` and LF. While another process holds it, the open fails with `ERROR_SHARING_VIOLATION`; the refused process then reads the holder line with `GENERIC_READ` and `FILE_SHARE_READ | FILE_SHARE_WRITE`. `Release` closes the handle. The operating system releases the lock when its holder exits by any path, so no lock is broken by age or PID. On Linux, where only the `linux-tsan` test preset runs the host storage, `Internal/Linux/Zenith_HostDocumentStorage_Linux.cpp` opens the same path with `O_RDWR | O_CREAT` and holds `flock(fd, LOCK_EX | LOCK_NB)`, retried on `EWOULDBLOCK`. The lock file lies under `.migration/`, so rule P6 already keeps it from being treated as a document. `Zenith_MemoryDocumentStorage` and a read-only host storage return a null lock: no other process can write the former, and the latter never writes. `Test_HostDocumentStorage_WriterLockSerialisesWriters` asserts the rule.

### Journal format

A canonical JSON document (rules of § "Canonical rules"), `$schema` `zenith2.migration-journal`, version 1:

| Key | Type | Meaning |
|---|---|---|
| `$schema` | string | `zenith2.migration-journal` |
| `schemaVersion` | integer | 1 |
| `batch` | string | the batch name |
| `state` | string | `PREPARED`, `BACKED_UP`, `WRITTEN` or `COMMITTING` |
| `backupRoot` | string | the batch's backup directory (a storage path) |
| `entries` | ordered array | one object per document, in byte order of path |
| `entries[].path` | string | the document path |
| `entries[].oldSha256` | string or null | 64 lowercase hex digits; null for a new document |
| `entries[].newSha256` | string or null | null for a deletion |
| `entries[].backupPath` | string or null | `<backupRoot>/<path>`, the entry's backup copy (Contracts' "its backup path"); null for a new document |
| `entries[].renamed` | boolean | set in `COMMITTING` after the entry's rename |

The journal is parsed with `Zenith_JsonLimits` defaults and validated field by field; any violation is `DOCUMENT_ERROR_JOURNAL_CORRUPT`. Digests are `Zenith_Sha256Digest` values (§ "Digests") written with `Zenith_FormatSha256Hex` and read with `Zenith_ParseSha256Hex`.

### Backups

A migration batch's backups live under `<storage root>/.migration/backups/<UTC timestamp>/<storage path>`, which for a game's storage is `Games/<Name>/.migration/backups/<UTC timestamp>/<game-relative path>` ([Z2-N-CONTRACT-541]); save batches under `.migration/backups/save-<UTC timestamp>/`. The nested `Zenith2/.gitignore` excludes `.migration/` directories wherever they occur (an engine library's content root, `Zenith2/Engine/<Lib>/Internal/Content/`, holds its own), and MOD's game-directory allowlist admits it (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game directory entries").

### Override preservation in migration steps

A step that renames, moves or restructures a property rewrites every override keyed to it in the same step ([Z2-N-CONTRACT-547]). For SCENE's own types the helper `Zenith_RewriteOverridePaths` (declared in `Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocumentTypes header") rewrites every override block of a scene or prefab document; a component chain's `rename` op changes no data at all, because overrides and field sets are keyed by property ID ([Z2-N-ARCH-604]). An override whose target entity no longer exists is kept, reported as `DOCUMENT_WARNING_DANGLING_OVERRIDE`, and listed in the preview.

## Migration fixtures

Every document type SCENE owns has, for each schema version from 1, `Zenith2/Engine/ZenithScene/Tests/Fixtures/Migration/<Type>/v<N>.json` and `v<N>.expected.json` ([Z2-N-CONTRACT-548]), with `<Type>` `SceneDocument` and `PrefabDocument`; version 1 is the identity pair (the expected output is the canonical form of the input). `Test_SceneDocumentMigration_Fixtures` migrates each fixture through a registry holding SCENE's types and every engine component type and compares bytes. A version bump without the new pair fails that test, because the test enumerates versions 1 … `m_uCurrentVersion` and requires both files for each ([Z2-N-CONTRACT-549]). The framework's own tests use the synthetic type `Test.Doc` with versions 1–3 and fixtures under `Zenith2/Engine/ZenithDocument/Tests/Fixtures/Migration/TestDoc/`.

## ZenithDocumentHost

```cpp
// Zenith2/Tools/DocumentHost/Zenith_HostDocumentStorage.h  (included as "DocumentHost/Zenith_HostDocumentStorage.h")
#pragma once
#include "ZenithDocument/Zenith_DocumentStorage.h"

/// @contract value Zenith_HostDocumentStorageDesc
/// Ownership: Plain value; the strings are copied at Create.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_szRoot is an absolute host path in UTF-8.
/// Errors: A relative or missing root, or a writable storage without m_szGame or m_szWriterMode, is
///   DOCUMENT_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_HostDocumentStorageDesc
{
	const char* const m_szRoot = nullptr; // e.g. "C:/dev/Zenith/Zenith2/Games/Harbour"
	const bool m_bReadOnly = false;       // every mutation returns DOCUMENT_ERROR_IO when set (the cook)
	Zenith_Heap* const m_pxHeap = nullptr;
	const char* const m_szWriterMode = "editor"; // the lock line's "mode" (ASSET's values: "assign", "migrate", "serve", ...)
	const char* const m_szGame = nullptr; // the lock line's "game"; required when m_bReadOnly is false
	const char* const m_szPlatform = nullptr; // the lock line's "platform"; null writes JSON null
};

/// @contract functions Zenith_HostDocumentStorageFactory
/// Ownership: Returns a Zenith_Owned<Zenith_DocumentStorage> the tool owns.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: DOCUMENT_ERROR_INVALID_DESC, DOCUMENT_ERROR_IO (the root cannot be opened), DOCUMENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 2. Tool-side; never linked by a Player.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_DocumentStorage>, Zenith_DocumentError> Zenith_CreateHostDocumentStorage(
	const Zenith_HostDocumentStorageDesc& xDesc);
```

<!-- assumption: pending X109(a) -->
`Zenith2/Tools/DocumentHost/CMakeLists.txt` is the one helper call

```cmake
zenith_add_tool(ZenithDocumentHost DEPENDS ZenithDocument ZenithCore SUPPORTED ZENITH_HOST)
```

The directory has no `Internal/Zenith_ZenithDocumentHostMain.cpp`, so `zenith_add_tool` creates a static library (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"); its include root is `Zenith2/Tools` ([Z2-N-ARCH-164]), so consumers write `#include "DocumentHost/Zenith_HostDocumentStorage.h"`. `SUPPORTED ZENITH_HOST` (`win`, `win-asan`, `linux-tsan`) replaces the tool default `ZENITH_TOOLS_HOST` so the host storage and its tests run under ASan and TSan; its dependencies are `ZENITH_ALL`, so [Z2-N-ARCH-172] admits every one of those presets. The sibling helpers apply `zenith_add_library`'s glob and partition rules, so `Tests/**` becomes the `ZenithDocumentHost_Tests` executable (the partition row "`Tests/**` (other) → the `<Name>_Tests` executable" of `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper"), registered with CTest in the `ZENITH_HOST` presets, as MOD's tool library `ZenithNewGameScaffold` has `ZenithNewGameScaffold_Tests`. Consumers: ZenithEditor, ZenithCook and ZenithNewGameScaffold; being a tool target it is outside every Shipping link closure ([Z2-N-ARCH-241]). The tool-table row of `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" is part of cross-doc item X109(a); cross-area row S19 asks ARCH to confirm the test-target rule for tool libraries in that patch.
<!-- /assumption -->

| File | Content |
|---|---|
| `Zenith2/Tools/DocumentHost/CMakeLists.txt` | the call above |
| `Zenith2/Tools/DocumentHost/Zenith_HostDocumentStorage.h` | the header above |
| `Zenith2/Tools/DocumentHost/Internal/Zenith_HostDocumentStorage.cpp` | storage-path validation (`Zenith_ValidateStoragePath`), root joining, directory creation one component at a time, listing (`std::filesystem::recursive_directory_iterator`, sorted; tools may use the STL, [Z2-N-CONTRACT-275]) |
| `Zenith2/Tools/DocumentHost/Internal/Windows/Zenith_HostDocumentStorage_Windows.cpp` | `CreateFileW` on `\\?\` paths, `WriteFile`, `FlushFileBuffers`, `MoveFileExW(MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)`, `GetDiskFreeSpaceExW` for `QueryFreeBytes`, the escape check with `GetFinalPathNameByHandleW` against the root's final path, the writer lock (`CreateFileW` share-mode hold of `.migration/writer.lock`, § "Storage-root writer lock") |
| `Zenith2/Tools/DocumentHost/Internal/Linux/Zenith_HostDocumentStorage_Linux.cpp` | `open`/`write`/`fsync`/`close`, `rename` then `fsync` of the directory, `statvfs` (`f_bavail × f_frsize`) for `QueryFreeBytes`, the escape check through `/proc/self/fd` and `realpath`, the writer lock (`flock`) |
| `Zenith2/Tools/DocumentHost/Tests/Zenith_HostDocumentStorage_Tests.cpp` | § "Host storage tests" of the tests file |
| `Zenith2/Tools/DocumentHost/Tests/Zenith_HostDocumentStorage_ChildProcess.cpp` | the child-process harness of `Test_HostDocumentStorage_JournalRecoveryOnDisk` (test code: the STL and the platform process API are allowed there) |

The escape check mirrors PLAT's (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution"): a link or junction inside the root that resolves outside it fails `DOCUMENT_ERROR_INVALID_PATH`. Every host path is formed inside `Internal/` from the root and a validated storage path.
