# EDIT design — declarations

This file is part of the EDIT design family; the entry document is `Zenith2/Docs/Design/EDIT.md`. It declares, with full signatures and contract blocks, every EDIT type the other files of the family name without declaring: the public headers of the tool library `ZenithEditorCore` (ruling Q86 (a): the session, documents, commands, history, selection, projection, migration model and cook client, with no ImGui and no shell) and, in `Zenith2/Docs/Design/EDIT/ToolDeclarations.md`, the internal headers of the `ZenithEditor` executable. The behaviour each type implements is specified in the heading the "Declaration index" names; this file adds the declarations and the contracts only.

## Scope of this file

| Topic | Heading |
|---|---|
| the headers of `Zenith2/Tools/EditorCore/` and the types they declare | "ZenithEditorCore headers" |
| where every named EDIT type is declared | "Declaration index" |

Every fallible function below returns `Zenith_Result<T, Zenith_EditorError>` (the error form of `Zenith2/Docs/Design/EDIT.md` → "Error enumeration"); every type carries a contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type". Headers of `ZenithEditorCore` are public root headers of a tool library (included as `#include "EditorCore/<Header>.h"`, [Z2-N-ARCH-164]); they include no ImGui header, no STL header and no third-party header, so an integration suite can include them.

## ZenithEditorCore headers

| Header (`Zenith2/Tools/EditorCore/…`) | Declares | Behaviour specified in |
|---|---|---|
| `Zenith_EditorError.h` | `Zenith_EditorErrorCode`, `Zenith_EditorError` | `Zenith2/Docs/Design/EDIT.md` → "Error enumeration" |
| `Zenith_EditorIDs.h` | `Zenith_EditorDocumentID`, `Zenith_EditorStorageRootID`, `Zenith_EditorCommandID`, `Zenith_MakeEditorCommandID` | § "Identifier header"; Foundation.md "Document table" |
| `Zenith_EditorHostPaths.h` | `Zenith_EditorHostPaths`, `Zenith_ResolveEditorHostPaths`, `Zenith_GetEditorExecutablePath`, `Zenith_ReadEditorHostFile` | Foundation.md "Host paths" |
| `Zenith_EditorHostWatcher.h` | `Zenith_EditorHostChangeKind`, `Zenith_EditorHostChange`, `Zenith_EditorHostWatcher` | § "Host watcher header"; Foundation.md "Host watchers" |
| `Zenith_EditorStorageRoots.h` | `Zenith_EditorStorageRootDesc`, `Zenith_EditorStorageRoots` | Foundation.md "Storage roots and the migration registry" |
| `Zenith_EditorDocumentTypes.h` | `Zenith_RegisterEditorDocumentTypes` | Foundation.md "Storage roots and the migration registry" |
| `Zenith_EditorDocument.h` | `Zenith_EditorDocumentKind`, `Zenith_EditorDocumentState`, `Zenith_EditorCloseMode`, `Zenith_EditorSaveReason`, `Zenith_EditorDocument` | Foundation.md "Document interface", "Document states" |
| `Zenith_EditorDocumentTable.h` | `Zenith_EditorDocumentTable`, `Zenith_EditorDocumentFactory` | Foundation.md "Document table" |
| `Zenith_EditorJsonEdit.h` | `Zenith_EditorJsonEditKind`, `Zenith_EditorJsonEdit`, `Zenith_ApplyEditorJsonEdit` | Foundation.md "JSON edits" |
| `Zenith_EditorChange.h` | `Zenith_EditorChangeKind`, `Zenith_EditorChange`, `Zenith_EditorOperation`, `Zenith_EditorOperationContext`, `Zenith_EditorResolution`, `Zenith_EditorChangeResult`, `Zenith_EditorDeferredSceneOperation` and their enumerations | § "Change header"; Foundation.md "Changes inside a record", "Asynchronous changes" |
| `Zenith_EditorHistory.h` | `Zenith_EditorRecordInfo`, `Zenith_EditorRecordState`, `Zenith_EditorHistoryDesc`, `Zenith_EditorHistory` | § "History header"; Foundation.md "Commands and history" |
| `Zenith_EditorCommand.h` | `Zenith_EditorCommand`, `Zenith_EditorCommandArgs`, `Zenith_EditorCommandContext` | § "Command header"; Foundation.md "Executing a command" |
| `Zenith_EditorCommandRegistry.h` | `Zenith_EditorCommandFlag`, `Zenith_EditorArgKind`, `Zenith_EditorArgDesc`, `Zenith_EditorCommandDesc`, `Zenith_EditorCommandRegistry` | § "Command registry header"; Automation.md "Command registry" |
| `Zenith_EditorSelection.h` | `Zenith_EditorObjectKind`, `Zenith_EditorObjectRef`, `Zenith_EditorSelection`, `Zenith_EditorSelectionSnapshot` | Foundation.md "Selection model"; § "Selection snapshot" |
| `Zenith_EditorClipboard.h` | `Zenith_EditorClipboardKind`, `Zenith_EditorClipboardNode`, `Zenith_EditorClipboard` | § "Clipboard header"; Foundation.md "Copy cut paste and duplicate" |
| `Zenith_EditorEvents.h` | `Zenith_EditorEventKind`, `Zenith_EditorEvent`, `Zenith_EditorEventListener`, `Zenith_EditorEvents` | § "Event header"; Automation.md "Waiting and events" |
| `Zenith_EditorSession.h` | `Zenith_EditorSession`, `Zenith_EditorSessionDesc`, `Zenith_EditorSessionConfig`, `Zenith_EditorOpenMode`, `Zenith_EditorRuntimeHost`, `Zenith_EditorRuntimeRestartState` | Foundation.md "Session object"; § "Session description" |
| `Zenith_EditorWorldSet.h` | `Zenith_EditorWorldRole`, `Zenith_EditorWorldEntry`, `Zenith_EditorWorldSet`, `Zenith_EditorProxyEntities` | § "World set header"; Foundation.md "World kinds", "Editor proxy entities" |
| `Zenith_EditorProjection.h` | `Zenith_EditorProjection`, `Zenith_EditorProjectionChange`, `Zenith_EditorProjectionChangeKind`, `Zenith_EditorProjectionHandler` | Foundation.md "Edit world projection"; § "Projection change header" |
| `Zenith_EditorPlay.h` | `Zenith_EditorPlayState`, `Zenith_EditorPlayWorldInfo`, `Zenith_EditorPlayCopyStats`, `Zenith_EditorPlayController` | Foundation.md "Play state machine"; § "Play controller header" |
| `Zenith_EditorCookClient.h` | `Zenith_EditorCookClient`, `Zenith_EditorCookClientDesc`, `Zenith_EditorCookClientState`, `Zenith_EditorCookResult`, `Zenith_EditorCookCompletion`, `Zenith_EditorCookListener` | Pipeline.md "Request queue"; § "Cook client types" |
| `Zenith_EditorContentCookChannel.h` | `Zenith_EditorContentChannelMode`, `Zenith_EditorContentCookChannel`, `Zenith_EditorContentBatchOperation` | § "Content cook channel header"; Pipeline.md "Reference repair" |
| `Zenith_EditorChildProcess.h` | `Zenith_EditorChildProcessDesc`, `Zenith_EditorChildStream`, `Zenith_EditorChildProcess`, `Zenith_CreateEditorChildProcess` | § "Child process header" |
| `Zenith_EditorMigrationModel.h` | `Zenith_EditorMigrationModelDesc`, `Zenith_EditorBackupBatch`, `Zenith_EditorMigrationModel` | Pipeline.md "Migration flow"; § "Migration model header" |
| `Zenith_EditorSchemaModel.h` | `Zenith_EditorSchemaModelDesc`, `Zenith_EditorSchemaDiagnostic`, `Zenith_EditorSchemaModel` | Panels.md "Schema panel workflow"; § "Schema model header" |

### Identifier header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorIDs.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_StringView.h"

// Zenith_EditorDocumentID: declared here, contract in Foundation.md "Document table".
struct Zenith_EditorDocumentID { Zenith_PersistentID m_xID; };

/// @contract value Zenith_EditorStorageRootID
/// Ownership: Plain value: the index of a storage root in Zenith_EditorStorageRoots; 0 is the game root, 1 … n the engine
///   content roots in byte order of their library name.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: Valid for one session; never persisted (the session file stores the root's directory, Formats.md "Session file").
/// Units: None.
/// Errors: None; an index no root has resolves to nothing (Zenith_EditorStorageRoots::Find returns nullptr).
/// Version: since Z2 Stage 2 (pull-forward slice of ZenithEditorCore).
/// @endcontract
struct Zenith_EditorStorageRootID { uint16 m_uIndex = 0xFFFF; };
inline constexpr Zenith_EditorStorageRootID xEDITOR_GAME_ROOT{ 0 };

/// @contract value Zenith_EditorCommandID
/// Ownership: Plain value: Zenith_Hash64 of the command's identifier text (Automation.md "Command identifiers").
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None within a build; never persisted (scripts, shortcuts and the session name commands by their text,
///   so a D-SEL-HASH (open, Stage 1) ruling that changes the hash changes no file).
/// Units: None.
/// Errors: None; 0 is the null command.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorCommandID { uint64 m_ulHash = 0; };
Zenith_EditorCommandID Zenith_MakeEditorCommandID(Zenith_StringView strIdentifier);
inline bool operator==(Zenith_EditorCommandID xA, Zenith_EditorCommandID xB) { return xA.m_ulHash == xB.m_ulHash; }
```

### Host paths header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorHostPaths.h
#pragma once
#include "ZenithCore/Zenith_Array.h"
#include "ZenithCore/Zenith_String.h"
#include "EditorCore/Zenith_EditorError.h"

/// @contract value Zenith_EditorHostPaths
/// Ownership: Plain value owning its strings (root heap); filled once by Zenith_ResolveEditorHostPaths at startup step 3
///   and passed by const reference to everything that needs a host path (constructor injection, Z2-D-046).
/// Thread affinity: IMMUTABLE after startup step 3; members non-const: Zenith_ResolveEditorHostPaths fills it at that step.
/// Invalidation: None.
/// Units: Absolute host paths with forward slashes and no trailing slash.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorHostPaths
{
	Zenith_String m_strOutRoot;          // ${ZENITH_OUT}: three directories above the executable's directory (<preset>/<Config>/bin)
	Zenith_String m_strZenith2Root;      // the Zenith2/ directory: two directories above m_strOutRoot (Zenith2/out/<checkout-hash>)
	Zenith_String m_strGamesRoot;        // --game-root, else <m_strZenith2Root>/Games
	Zenith_String m_strHostToolsDir;     // the executable's directory, which holds ZenithCook.exe and ZenithShaderCompiler.exe
	Zenith_String m_strUserDir;          // --user-dir, else %LOCALAPPDATA%/Zenith2/Editor
};

// Pure path arithmetic plus two existence checks (m_strOutRoot/declared_deps.json, m_strGamesRoot); EDITOR_ERROR_HOST_PATH
// names the path that failed.
Zenith_Result<Zenith_EditorHostPaths, Zenith_EditorError> Zenith_ResolveEditorHostPaths(
	Zenith_StringView strExecutablePath, Zenith_StringView strGameRootOverride, Zenith_StringView strUserDirOverride);
// Internal/Windows/Zenith_EditorHostPaths_Windows.cpp: GetModuleFileNameW(nullptr, …) converted to UTF-8.
Zenith_Result<void, Zenith_EditorError> Zenith_GetEditorExecutablePath(Zenith_String& strOut);
// Internal/Windows/Zenith_EditorHostPaths_Windows.cpp: CreateFileW + ReadFile of a whole file, before Zenith_Platform
// exists (startup step 3); EDITOR_ERROR_HOST_PATH when missing, EDITOR_ERROR_DOCUMENT_LIMIT above ulMaxBytes.
Zenith_Result<void, Zenith_EditorError> Zenith_ReadEditorHostFile(Zenith_StringView strPath, uint64 ulMaxBytes,
	Zenith_Array<uint8>& axOut);
```

### Host watcher header

<!-- elaboration -->
The source directories, the engine content roots and the out tree are host directories outside every engine mount, so the editor owns its own watcher of host directories, independent of the runtime (it survives a runtime restart), with the platform watcher's rules applied as its own: coalescing within a debounce, a bounded queue, and overflow reported as a rescan, never lost silently.

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorHostWatcher.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Span.h"
#include "EditorCore/Zenith_EditorError.h"

/// @contract enum Zenith_EditorHostChangeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorHostChangeKind : uint8
{
	EDITOR_HOST_CHANGE_ADDED = 1,     // FILE_ACTION_ADDED
	EDITOR_HOST_CHANGE_MODIFIED,      // FILE_ACTION_MODIFIED
	EDITOR_HOST_CHANGE_REMOVED,       // FILE_ACTION_REMOVED, and FILE_ACTION_RENAMED_OLD_NAME for the old path
	EDITOR_HOST_CHANGE_RENAMED,       // FILE_ACTION_RENAMED_NEW_NAME, for the new path
	EDITOR_HOST_CHANGE_RESCAN,        // the OS buffer overflowed or the queue dropped records: m_strHostPath is the watched directory
};

/// @contract value Zenith_EditorHostChange
/// Ownership: Plain value; the path points into the arena Drain was given.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when that arena is reset.
/// Units: Absolute host paths, UTF-8, forward slashes; microseconds of the host monotonic clock.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorHostChange
{
	Zenith_EditorHostChangeKind m_eKind = EDITOR_HOST_CHANGE_MODIFIED;
	Zenith_StringView m_strHostPath;
	uint64 m_ulTimestampUs = 0;      // the last OS report coalesced into this record
};

/// @contract type Zenith_EditorHostWatcher
/// Ownership: Created by Zenith_EditorHostWatcher::Create as a Zenith_Owned, owned by its client (Zenith_EditorApp: one per
///   storage root and one for the shader reload journal; a test); owns one thread named EditorHostWatch and the directory
///   handle, both released by its destructor, which joins the thread.
/// Thread affinity: OWNER_THREAD for Drain; the watcher thread is internal and hands records over through a
///   Zenith_BoundedQueue (no lock).
/// Invalidation: None.
/// Units: Debounce in milliseconds.
/// Errors: EDITOR_ERROR_HOST_PATH when the directory does not exist or cannot be opened for change notification;
///   EDITOR_ERROR_OUT_OF_MEMORY. A lost OS report is never silent: it becomes one EDITOR_HOST_CHANGE_RESCAN record.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorHostWatcher
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorHostWatcher>, Zenith_EditorError> Create(Zenith_Heap& xHeap,
		Zenith_StringView strHostDirectory, bool bRecursive, uint32 uDebounceMs);
	~Zenith_EditorHostWatcher();
	// The coalesced changes whose debounce elapsed since the last Drain, oldest first; at most 4,096 records.
	Zenith_Span<const Zenith_EditorHostChange> Drain(Zenith_Arena& xArena);
};
```

- Windows (`Tools/EditorCore/Internal/Windows/Zenith_EditorHostWatcher_Windows.cpp`): `CreateFileW(directory, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr)`, then `ReadDirectoryChangesW` with a 64 KiB buffer on the overlapped handle, `bWatchSubtree = bRecursive`, filter `FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE`, woken by the overlapped event or the destructor's stop event, re-armed after each completion. Each `FILE_NOTIFY_INFORMATION` name is converted from UTF-16 to UTF-8, joined to the directory with `/`, and noted in the change queue.
- The change queue (`Tools/EditorCore/Internal/Zenith_EditorHostChangeQueue.h`, `.cpp`, platform-independent, `Zenith_EditorHostChangeQueue` with `Note(strHostPath, eKind, ulNowUs)`, `NoteOverflow(ulNowUs)` and `Flush(ulNowUs, Zenith_BoundedQueue&)`, the clock passed in so the tests drive it): reports for one path within `uDebounceMs` (default 100) coalesce into one record whose kind is the last one; a path with a dot-prefixed segment below the watched directory (`.editor/`, `.migration/`, `.git/`) is never noted, the rule SCENE's documents and the cook server's watch use; records whose debounce elapsed move into the hand-off queue of at most 4,096 records with `QUEUE_FULL_POLICY_DROP_OLDEST`, counted in `editor.watcher.dropped` and `editor.watcher.high_water`, and a drop or an OS overflow (`ReadDirectoryChangesW` completing with zero bytes, or `ERROR_NOTIFY_ENUM_DIR`) adds one `EDITOR_HOST_CHANGE_RESCAN` record for the watched directory, counted in `editor.watcher.rescans`.
- `Test_EditorHostWatcher_ReportsSourceAndJournalChanges` and `Test_EditorHostWatcher_QueueCoalescesAndBounds` (`ZenithEditorCore_Tests`) cover it (`Zenith2/Docs/Design/EDIT/Tests.md` → "Document tests").

### Storage roots header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorStorageRoots.h
/// @contract value Zenith_EditorStorageRootDesc
/// Ownership: Plain value; the strings are borrowed for the duration of Zenith_EditorStorageRoots::Create.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Host paths.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorStorageRootDesc
{
	Zenith_StringView m_strDirectory;    // Games/<Name>/ or Engine/<Lib>/Internal/Content/
	Zenith_StringView m_strLibrary;      // empty for the game root
	bool m_bWritable = true;             // engine roots: editor.engineContent.writable
};

/// @contract type Zenith_EditorStorageRoots
/// Ownership: Owned by Zenith_EditorApp (or a suite); owns one Zenith_HostDocumentStorage and one Zenith_DocumentMigrator per
///   root (Foundation.md "Storage roots and the migration registry"); borrows the frozen Zenith_MigrationRegistry.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: The storage and migrator references it returns are valid until it is destroyed (shutdown step 9).
/// Units: None.
/// Errors: EDITOR_ERROR_DOCUMENT wraps a Zenith_DocumentError of storage creation or RecoverPendingJournals;
///   EDITOR_ERROR_STORAGE_LOCKED when a writer lock stays held for the migrator's wait.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorStorageRoots
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorStorageRoots>, Zenith_EditorError> Create(Zenith_Heap& xHeap,
		const Zenith_MigrationRegistry& xRegistry, Zenith_Span<const Zenith_EditorStorageRootDesc> axRoots,
		Zenith_StringView strGame, Zenith_StringView strBatchTimestamp);
	~Zenith_EditorStorageRoots();
	// Startup step 8: RecoverPendingJournals on every root; a corrupt or conflicting journal marks that root read-only.
	Zenith_Result<void, Zenith_EditorError> RecoverAll();
	uint16 GetCount() const;
	Zenith_DocumentMigrator* Find(Zenith_EditorStorageRootID xRoot);
	Zenith_DocumentStorage* FindStorage(Zenith_EditorStorageRootID xRoot);
	Zenith_EditorStorageRootID FindByPath(Zenith_StringView strHostPath) const;   // the root containing it
	bool IsReadOnly(Zenith_EditorStorageRootID xRoot) const;
	const Zenith_RecoveryReport& GetRecoveryReport(Zenith_EditorStorageRootID xRoot) const;
	// Takes the root's writer lock for one operation (ruling 101: per request), as Pipeline.md "Rename and move" step 1 does.
	Zenith_Result<void, Zenith_EditorError> AcquireWriterLock(Zenith_EditorStorageRootID xRoot, uint32 uWaitMs);
	void ReleaseWriterLock(Zenith_EditorStorageRootID xRoot);
};
```

### Document type registration header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorDocumentTypes.h
// Registers every owner's document types into xRegistry in the order of Zenith2/Docs/Design/ASSET/Migration.md
// "Registered document types", calling the owners' registration functions (SCENE's in ZenithScene; the tool-side ones in
// ZenithAuthoring, ruling Q84 (a)); the caller freezes the registry afterwards. Errors: the first owner's Zenith_DocumentError.
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterEditorDocumentTypes(Zenith_MigrationRegistry& xRegistry);
```

The function is a free function with no state, so it carries no contract block of its own beyond the comment above ([Z2-N-CONTRACT] blocks are per type); its failures are the owners' errors unchanged.

### Document kinds additions

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorDocument.h (additions beside Foundation.md "Document interface")
/// @contract enum Zenith_EditorDocumentState
/// Ownership: Plain enumeration; the states of Foundation.md "Document states".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorDocumentState : uint8
{
	EDITOR_DOCUMENT_STATE_CLEAN = 1,
	EDITOR_DOCUMENT_STATE_DIRTY,
	EDITOR_DOCUMENT_STATE_MIGRATED,
	EDITOR_DOCUMENT_STATE_READ_ONLY_NEWER,
	EDITOR_DOCUMENT_STATE_CONFLICTED,
	EDITOR_DOCUMENT_STATE_MISSING,
};

/// @contract enum Zenith_EditorCloseMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorCloseMode : uint8
{
	EDITOR_CLOSE_MODE_REFUSE_DIRTY = 1,   // a dirty document is not closed: EDITOR_ERROR_DOCUMENT_CONFLICT
	EDITOR_CLOSE_MODE_SAVE,               // save first; a failed save keeps it open
	EDITOR_CLOSE_MODE_DISCARD,            // close and drop unsaved changes (its autosave is deleted)
};

/// @contract enum Zenith_EditorSaveReason
/// Ownership: Plain enumeration; logged with every save batch and passed to Zenith_EditorEvents.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorSaveReason : uint8
{
	EDITOR_SAVE_REASON_COMMAND = 1,        // document.save, document.saveAll
	EDITOR_SAVE_REASON_BEFORE_PLAY,        // editor.play.saveBeforePlay
	EDITOR_SAVE_REASON_SHUTDOWN,           // shutdown step 2 "save"
	EDITOR_SAVE_REASON_AUTOMATION,         // the script's exit step "save"
	EDITOR_SAVE_REASON_CLOSE,              // EDITOR_CLOSE_MODE_SAVE
};

// Zenith_EditorDocument::ReplaceState (state replacement without touching the history) is declared with the interface in
// Foundation.md "Document interface".

/// @contract interface Zenith_EditorDocumentFactory
/// Ownership: Implemented per document kind (SCENE and JSON in ZenithEditorCore; TERRAIN in ZenithEditor/Internal/Terrain/;
///   UI in ZenithEditor/Internal/UI/); constexpr singletons registered with the document table at session creation.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Open returns EDITOR_ERROR_DOCUMENT with the Zenith_DocumentError logged.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2 (SCENE, JSON); Stage 3 (TERRAIN); Stage 4 (UI).
/// @endcontract
class Zenith_EditorDocumentFactory
{
public:
	virtual ~Zenith_EditorDocumentFactory() = default;
	virtual bool Claims(Zenith_TypeID xDocumentType) const = 0;
	// xTypes: the session's type registry (Zenith_EditorSessionDesc::m_pxTypes), which the SCENE kind passes to
	// Zenith_SceneDocument::Load and every kind keeps for ReplaceState, ReloadFromDisk and its validators until RebindTypes.
	virtual Zenith_Result<Zenith_Owned<Zenith_EditorDocument>, Zenith_EditorError> Open(Zenith_DocumentMigrator& xMigrator,
		Zenith_StringView strPath, Zenith_EditorStorageRootID xRoot, uint64 ulFirstStateID,
		const Zenith_TypeRegistry& xTypes) = 0;
};
// Zenith_EditorDocumentTable::RegisterFactory (Foundation.md "Document table"): the last factory claiming a type wins; JSON is
// registered first, so it is the fallback for every registered type. The table passes its registry to every Open.
```

### Change header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorChange.h
#pragma once
#include "EditorCore/Zenith_EditorIDs.h"
#include "EditorCore/Zenith_EditorError.h"

class Zenith_SceneEdit;            // ZenithScene
class Zenith_EditorJsonEdit;
class Zenith_EditorCookClient;
class Zenith_EditorDocumentTable;
class Zenith_EditorStorageRoots;
class Zenith_EditorProjection;
class Zenith_EditorRuntimeHost;
// Zenith_AssetGUID (ZenithAssets) and Zenith_Sha256Value (ZenithCore) come from their libraries' root headers, included here.

/// @contract enum Zenith_EditorChangeKind
/// Ownership: Plain enumeration of the change kinds of Foundation.md "Changes inside a record".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2; values never move; a new kind is appended.
/// @endcontract
enum Zenith_EditorChangeKind : uint8
{
	EDITOR_CHANGE_SCENE_EDIT = 1,
	EDITOR_CHANGE_JSON_EDIT,
	EDITOR_CHANGE_TERRAIN_EDIT,        // operation: Zenith_EditorTerrainOperation (ZenithEditor/Internal/Terrain/)
	EDITOR_CHANGE_UI_EDIT,             // operation: Zenith_EditorUIOperation (ZenithEditor/Internal/UI/)
	EDITOR_CHANGE_ASSET_SETTINGS,      // operation: asynchronous (setSettings)
	EDITOR_CHANGE_FILE_OPERATION,      // operation: asynchronous (assignGuids, import, reimport after local file moves)
	EDITOR_CHANGE_DOCUMENT_BYTES,      // operation: synchronous for documents, asynchronous for .meta entries (migrateApply)
	EDITOR_CHANGE_SCHEMA,              // operation: asynchronous (the schema window result; a runtime restart on undo)
	EDITOR_CHANGE_PLAY_WORLD_EDIT,     // operation: a play-world component edit and its captured previous value (play history only)
};

/// @contract enum Zenith_EditorOperationDirection
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorOperationDirection : uint8 { EDITOR_OPERATION_FORWARD = 1, EDITOR_OPERATION_INVERSE };

/// @contract enum Zenith_EditorOperationStatus
/// Ownership: Plain enumeration returned by Zenith_EditorOperation's members.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: EDITOR_OPERATION_FAILED carries its error and failure kind through Zenith_EditorOperationContext::SetFailure.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorOperationStatus : uint8 { EDITOR_OPERATION_DONE = 1, EDITOR_OPERATION_IN_FLIGHT, EDITOR_OPERATION_FAILED };

/// @contract enum Zenith_EditorFailureKind
/// Ownership: Plain enumeration (Foundation.md "Asynchronous changes": what a failure does to the record).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorFailureKind : uint8
{
	EDITOR_FAILURE_TRANSIENT = 1,   // server restarted or unavailable, queue full, timeout, cancel: the record keeps its place
	EDITOR_FAILURE_PERMANENT,       // META_CHANGED, STALE_PLAN, a path occupied, a refused schema: the record becomes a fence
};

/// @contract enum Zenith_EditorResolution
/// Ownership: Plain enumeration returned through Zenith_EditorOperationContext::SetResolution by Zenith_EditorOperation::Resolve
///   (Foundation.md "Asynchronous changes": the outcome of a request that ended without a result).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorResolution : uint8
{
	EDITOR_RESOLUTION_COMPLETED = 1,      // the server applied the whole request: the change counts as DONE
	EDITOR_RESOLUTION_NO_EFFECT,          // nothing of the change was applied, its local part included: transient, no compensation
	EDITOR_RESOLUTION_PARTIAL,            // some of it was applied (items, or the synchronous local part): transient,
	                                      // Compensate undoes exactly those; a change with a local part never resolves NO_EFFECT
	EDITOR_RESOLUTION_CHANGED_ELSEWHERE,  // the target holds a state no direction wrote: permanent
	EDITOR_RESOLUTION_UNKNOWN,            // the resolution requests failed: permanent, the idempotent compensation runs
};

/// @contract value Zenith_EditorChangeResult
/// Ownership: Plain value an asynchronous operation publishes when a direction completes; the spans point into its record's arena.
/// Thread affinity: OWNER_THREAD (the main thread).
/// Invalidation: With its record.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorChangeResult
{
	Zenith_Span<const Zenith_AssetGUID> m_axCreatedAssets;   // assignGuids' created GUIDs (import, create, duplicate)
	Zenith_Sha256Value m_xMetaSha256{};                      // setSettings: the digest the direction wrote
};

/// @contract type Zenith_EditorOperationContext
/// Ownership: Owned by the session; passed by reference to every operation member call; borrows the cook client, the
///   document table, the storage roots and the projections.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: The references it returns are valid for the call.
/// Units: Microseconds of the host monotonic clock.
/// Errors: None of its own.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2 (documents and roots); cook client from Stage 3.
/// @endcontract
class Zenith_EditorOperationContext
{
public:
	Zenith_EditorCookClient* GetCookClient();                 // nullptr with --no-cook-server
	Zenith_EditorDocumentTable& GetDocuments();
	Zenith_EditorStorageRoots& GetStorageRoots();
	Zenith_EditorProjection* FindProjection(Zenith_EditorDocumentID xDocument);   // nullptr when not projected
	uint64 GetNowUs() const;
	void SetFailure(const Zenith_EditorError& xError, Zenith_EditorFailureKind eKind);   // before returning FAILED
	void SetResolution(Zenith_EditorResolution eResolution);                            // from Resolve, before returning DONE
	void SetResult(const Zenith_EditorChangeResult& xResult);                           // when a direction completes
	// The result an earlier change of the same record published (Foundation.md "Executing a command": deferred operations).
	const Zenith_EditorChangeResult* GetChangeResult(uint32 uChangeIndex) const;
	Zenith_EditorRuntimeHost* GetRuntimeHost();                           // the schema operation's restart
};

/// @contract interface Zenith_EditorOperation
/// Ownership: Constructed by a command in the record arena with Zenith_Construct; owned by its record and destroyed with
///   Zenith_Destroy when the record is evicted, pruned or discarded.
/// Thread affinity: MAIN_THREAD. Every member runs in the editor frame's step 2.
/// Invalidation: None.
/// Units: Bytes for GetMemoryBytes.
/// Errors: A FAILED status names its error through the context; an operation never asserts on server output or file state.
///   Compensate after EDITOR_RESOLUTION_PARTIAL undoes exactly the parts Resolve found applied.
/// Reentrancy: NONE. A member never executes a command, never calls the history and never polls the cook client.
/// Version: since Z2 Stage 2 (synchronous kinds); asynchronous kinds since Z2 Stage 3.
/// @endcontract
class Zenith_EditorOperation
{
public:
	virtual ~Zenith_EditorOperation() = default;
	virtual bool IsAsynchronous() const = 0;
	// Starts the direction: the synchronous part runs now; DONE means complete, IN_FLIGHT means requests are pending.
	virtual Zenith_EditorOperationStatus Begin(Zenith_EditorOperationDirection eDirection,
		Zenith_EditorOperationContext& xContext) = 0;
	// Advances an IN_FLIGHT direction from completed requests; called once per editor frame while IN_FLIGHT.
	virtual Zenith_EditorOperationStatus Poll(Zenith_EditorOperationContext& xContext) = 0;
	// After a FAILED Begin or Poll: undoes the partial work of that direction (Foundation.md "Asynchronous changes").
	virtual Zenith_EditorOperationStatus Compensate(Zenith_EditorOperationContext& xContext) = 0;
	// Cancels the in-flight requests (cook cancel); the history then calls Resolve (Foundation.md "Asynchronous changes").
	virtual void Cancel(Zenith_EditorOperationContext& xContext) = 0;
	// After a request ended cancelled, timed out or failed by a restart: sends the resolution requests (record, migratePreview)
	// and returns IN_FLIGHT until they complete, then DONE with SetResolution called; an operation whose requests are all
	// idempotent (assignGuids, import, reimport) returns DONE at once with EDITOR_RESOLUTION_PARTIAL: its synchronous local part
	// (file moves, copies, repair writes) applied, and the requests' effect needs no resolution because the compensation is
	// idempotent, so Compensate then runs the change's documented compensation unchanged (Foundation.md "Asynchronous changes").
	virtual Zenith_EditorOperationStatus Resolve(Zenith_EditorOperationContext& xContext) = 0;
	virtual uint64 GetMemoryBytes() const = 0;
	// Interaction merging (Foundation.md "Interactions and merging"): true when xNewer was folded into this operation.
	virtual bool Merge(Zenith_EditorOperation& xNewer) = 0;
};

/// @contract value Zenith_EditorChange
/// Ownership: Plain value built by a command in the record arena; the payload pointers point into the same arena (scene
///   and JSON edits) or at an arena-constructed operation the record owns.
/// Thread affinity: OWNER_THREAD (the main thread).
/// Invalidation: Invalid when its record is released.
/// Units: State IDs of Foundation.md "Dirty tracking".
/// Errors: None of its own; Zenith_EditorCommandContext::Apply validates it (EDITOR_ERROR_COMMAND_ARGS for a payload that
///   does not match m_eKind).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorChange
{
	Zenith_EditorChangeKind m_eKind = EDITOR_CHANGE_SCENE_EDIT;
	Zenith_EditorDocumentID m_xDocument;                    // null for FILE_OPERATION and ASSET_SETTINGS that touch no open document
	const Zenith_SceneEdit* m_pxSceneEdit = nullptr;        // SCENE_EDIT: the forward edit
	const Zenith_EditorJsonEdit* m_pxJsonEdit = nullptr;    // JSON_EDIT: the forward edit
	Zenith_EditorOperation* m_pxOperation = nullptr;        // every other kind
	uint64 m_ulMergeTarget = 0;                            // Zenith_Hash64 of (document, object, property path); 0: never folded
	// Filled by the history when the change is applied:
	const Zenith_SceneEdit* m_pxInverseSceneEdit = nullptr;
	const Zenith_EditorJsonEdit* m_pxInverseJsonEdit = nullptr;
	uint64 m_ulStateBefore = 0;
	uint64 m_ulStateAfter = 0;
};

// Builds the scene edits of a deferred change from the result of an earlier change of the same record, into xArena;
// returns EDITOR_ERROR_SCENE or EDITOR_ERROR_OUT_OF_MEMORY on failure (the direction then fails, definite, and compensates).
using Zenith_PfnEditorBuildDeferredEdits = Zenith_Result<void, Zenith_EditorError> (*)(const Zenith_EditorChangeResult& xEarlier,
	const void* pUserData, Zenith_Arena& xArena, Zenith_Array<const Zenith_SceneEdit*>& axEditsOut);

/// @contract type Zenith_EditorDeferredSceneOperation
/// Ownership: Constructed by a command in the record arena; owned by its record; holds the builder, its user data (in the same
///   arena), the index of the change it reads, and, after the first forward Begin, the built edits and their inverses.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Begin returns FAILED with the builder's error, or EDITOR_ERROR_SCENE from Zenith_ApplySceneEdit, both definite.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorDeferredSceneOperation final : public Zenith_EditorOperation
{
public:
	Zenith_EditorDeferredSceneOperation(Zenith_EditorDocumentID xDocument, uint32 uReadsChange,
		Zenith_PfnEditorBuildDeferredEdits pfnBuild, const void* pUserData);
	// IsAsynchronous false; Begin(FORWARD) builds once (GetChangeResult(uReadsChange)) and applies; Begin(INVERSE) applies the
	// inverses newest first; Poll, Resolve and Cancel are never called (the operation is synchronous once begun); Merge false.
};
```

### History header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorHistory.h
#pragma once
#include "EditorCore/Zenith_EditorChange.h"
#include "EditorCore/Zenith_EditorSelection.h"

/// @contract enum Zenith_EditorRecordState
/// Ownership: Plain enumeration; Zenith_EditorRecordInfo::m_eState (Foundation.md "Command records").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3 (the pending states); COMMITTED since Stage 2.
/// @endcontract
enum Zenith_EditorRecordState : uint8
{
	EDITOR_RECORD_STATE_COMMITTED = 1,     // on the undo or the redo stack
	EDITOR_RECORD_STATE_PENDING_EXECUTE,   // executing: an asynchronous change of the new record is in flight
	EDITOR_RECORD_STATE_PENDING_UNDO,      // an asynchronous inverse is in flight
	EDITOR_RECORD_STATE_PENDING_REDO,      // an asynchronous forward change of a redo is in flight
	EDITOR_RECORD_STATE_COMPENSATING,      // a failure's compensation is in flight
	EDITOR_RECORD_STATE_RESOLVING,         // an unknown outcome is being resolved from the server's state (since Z2 Stage 3)
};

/// @contract value Zenith_EditorHistoryDesc
/// Ownership: Plain value; the pointers are borrowed for the history's lifetime.
/// Thread affinity: IMMUTABLE; members non-const: Create fills the pointers of the copy nested in Zenith_EditorSessionConfig.
/// Invalidation: None.
/// Units: Bytes, records, milliseconds, seconds.
/// Errors: None of its own; Create validates every member against the ranges of EDIT.md "Editor configuration keys".
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorHistoryDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_EditorDocumentTable* m_pxDocuments = nullptr;
	Zenith_EditorSelection* m_pxSelection = nullptr;
	Zenith_EditorEvents* m_pxEvents = nullptr;
	Zenith_EditorOperationContext* m_pxOperations = nullptr;
	uint32 m_uMaxRecords = 1000;                          // editor.undo.maxRecords
	uint64 m_ulMaxBytes = 512ull << 20;                    // editor.undo.maxBytes
	uint64 m_ulMaxRecordBytes = 256ull << 20;              // editor.undo.maxRecordBytes
	uint64 m_ulMaxTerrainRecordBytes = 256ull << 20;       // editor.undo.maxTerrainRecordBytes (Z2-B-EDIT-011 once allocated)
	uint32 m_uMergeWindowMs = 500;                         // editor.undo.mergeWindowMs
	uint32 m_uAsyncTimeoutSeconds = 300;                   // editor.history.asyncTimeoutSeconds: inactivity (Foundation.md "Asynchronous changes")
	bool m_bPlayHistory = false;                            // true: accepts EDITOR_CHANGE_PLAY_WORLD_EDIT alone
};

/// @contract type Zenith_EditorHistory
/// Ownership: Created by Zenith_EditorHistory::Create as a Zenith_Owned, owned by the session (the main history) or by the
///   play controller (one per play world); owns every record, its arena and the operations it holds.
/// Thread affinity: MAIN_THREAD. Every member runs in the editor frame's step 2.
/// Invalidation: Zenith_EditorRecordInfo label views are invalid after the record is evicted, pruned or discarded.
/// Units: Bytes; records; microseconds of the host monotonic clock.
/// Errors: EDITOR_ERROR_COMMAND_REENTRANT, EDITOR_ERROR_COMMAND_DISABLED, EDITOR_ERROR_PLAY_STATE (a document-changing
///   command while play is not EDITING), EDITOR_ERROR_HISTORY_PENDING (an asynchronous record is pending),
///   EDITOR_ERROR_NOTHING_TO_UNDO, EDITOR_ERROR_NOTHING_TO_REDO, EDITOR_ERROR_HISTORY_FENCE, EDITOR_ERROR_UNDO_FAILED,
///   EDITOR_ERROR_HISTORY_ASYNC_TIMEOUT, EDITOR_ERROR_OUT_OF_MEMORY, and the command's own error from Execute.
/// Reentrancy: NONE. Execute invokes Zenith_EditorCommand::Execute and operation members; none of them may call the history.
/// Version: since Z2 Stage 2 (synchronous records); asynchronous records since Z2 Stage 3.
/// @endcontract
class Zenith_EditorHistory
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorHistory>, Zenith_EditorError> Create(
		const Zenith_EditorHistoryDesc& xDesc);
	~Zenith_EditorHistory();
	// Foundation.md "Executing a command": the new record's ID, or 0 when the command applied no change. A record holding an
	// asynchronous change is returned in EDITOR_RECORD_STATE_PENDING_EXECUTE; its commit or failure is published when it completes.
	Zenith_Result<uint64, Zenith_EditorError> Execute(const Zenith_EditorCommand& xCommand,
		const Zenith_EditorCommandArgs& xArgs);
	Zenith_Result<void, Zenith_EditorError> Undo();
	Zenith_Result<void, Zenith_EditorError> Redo();
	Zenith_Result<void, Zenith_EditorError> UndoTo(uint64 ulRecordID);   // every record up to and including it
	Zenith_Result<void, Zenith_EditorError> RedoTo(uint64 ulRecordID);
	Zenith_Result<void, Zenith_EditorError> BeginInteraction(uint64 ulMergeKey, Zenith_StringView strLabel);
	void EndInteraction();
	void Poll();                                   // step 2: drives a pending record (Foundation.md "Asynchronous changes")
	void CancelPending();                          // the status bar's cancel: Cancel, then Resolve, then compensation
	bool IsPending() const;
	bool IsBusy() const;             // a command executing, an interaction open, or a record pending
	const Zenith_EditorRecordInfo* GetPending() const;   // nullptr when none
	uint32 GetUndoCount() const;
	uint32 GetRedoCount() const;
	uint32 ListUndo(Zenith_Span<Zenith_EditorRecordInfo> axOut) const;   // newest first
	uint32 ListRedo(Zenith_Span<Zenith_EditorRecordInfo> axOut) const;   // next redo first
	uint64 GetMemoryBytes() const;
	// Foundation.md "Closing a document and its history": used by close and by a reload from disk; returns the count removed.
	uint32 PruneDocument(Zenith_EditorDocumentID xDocument);
	void Clear();                                  // the play history at Stop; the main history never
};
```

### Command header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCommand.h (Zenith_EditorCommand itself: Foundation.md "Executing a command")
/// @contract value Zenith_EditorCommandArgs
/// Ownership: A view of a JSON object node; the document it views outlives the command call (the automation script, the
///   live-link line, the shortcut's constant table or a panel's arena).
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when the viewed document is destroyed or mutated.
/// Units: Whatever the argument descriptor declares (metres, degrees, seconds).
/// Errors: None at read: the registry validated the object against the command's Zenith_EditorArgDesc span before
///   Execute (EDITOR_ERROR_COMMAND_ARGS), so every getter returns the value or the descriptor's default.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorCommandArgs
{
public:
	Zenith_EditorCommandArgs(const Zenith_JsonDocument& xDocument, Zenith_JsonNode xObject);
	static Zenith_EditorCommandArgs Empty();    // {} : every argument at its default
	bool Has(Zenith_StringView strName) const;
	bool GetBool(Zenith_StringView strName) const;
	sint64 GetInteger(Zenith_StringView strName) const;
	double GetNumber(Zenith_StringView strName) const;
	Zenith_StringView GetString(Zenith_StringView strName) const;    // string and path kinds
	Zenith_PersistentID GetPersistentID(Zenith_StringView strName) const;
	Zenith_Vector3 GetVector3(Zenith_StringView strName) const;
	Zenith_JsonNode GetNode(Zenith_StringView strName) const;        // JSON kind; null node when absent
	const Zenith_JsonDocument& GetDocument() const;
};

/// @contract type Zenith_EditorCommandContext
/// Ownership: Created by the history for one Execute; borrows the session and the open transaction.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Invalid after Execute returns.
/// Units: None.
/// Errors: Apply returns the change's apply error (EDITOR_ERROR_JSON_EDIT, EDITOR_ERROR_SCENE, EDITOR_ERROR_DOCUMENT_READ_ONLY,
///   EDITOR_ERROR_PROJECTION is never returned: a projection failure marks a reconcile instead), EDITOR_ERROR_OUT_OF_MEMORY.
///   Apply on a context of a command without EDITOR_COMMAND_FLAG_CHANGES_DOCUMENTS is ZENITH_CHECK.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorCommandContext
{
public:
	Zenith_Result<void, Zenith_EditorError> Apply(const Zenith_EditorChange& xChange);   // synchronous part now
	Zenith_Arena& GetRecordArena();
	Zenith_EditorSession& GetSession();
	bool IsRecording() const;           // false for commands that change no document
	void SetLabel(Zenith_StringView strLabel);
	void SetMergeKey(uint64 ulMergeKey);
	void SetSelectionAfter(const Zenith_EditorSelectionSnapshot& xSelection);
};
```

### Command registry header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCommandRegistry.h (Zenith_EditorCommandFlag, Zenith_EditorCommandDesc:
// Automation.md "Command descriptors")
/// @contract enum Zenith_EditorArgKind
/// Ownership: Plain enumeration of argument value kinds.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorArgKind : uint8
{
	EDITOR_ARG_KIND_BOOL = 1, EDITOR_ARG_KIND_INTEGER, EDITOR_ARG_KIND_NUMBER, EDITOR_ARG_KIND_STRING,
	EDITOR_ARG_KIND_PATH,           // a game-relative path, validated by Zenith_ValidateDocumentPath
	EDITOR_ARG_KIND_PERSISTENT_ID,  // canonical text form, or a hierarchy path "/Root/Child" for automation
	EDITOR_ARG_KIND_VECTOR,         // an array of 2, 3 or 4 finite numbers
	EDITOR_ARG_KIND_JSON,           // any JSON value, validated by the command
	EDITOR_ARG_KIND_ARRAY_OF_IDS,   // an array of persistent IDs
};

/// @contract value Zenith_EditorArgDesc
/// Ownership: constexpr data beside each command descriptor.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: The unit named in m_szUnit (metres, degrees, seconds, ticks), or none.
/// Errors: None of its own; Zenith_EditorCommandRegistry::Register validates names (ASCII identifiers, at most 32 bytes, unique
///   per command) and defaults (they must parse as m_eKind).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorArgDesc
{
	const char* const m_szName = nullptr;
	const Zenith_EditorArgKind m_eKind = {};
	const bool m_bRequired = false;
	const char* const m_szDefaultJson = nullptr; // nullptr when required; else the JSON text of the default
	const char* const m_szUnit = nullptr; // nullptr or "m", "deg", "s", "ticks"
};

/// @contract type Zenith_EditorCommandRegistry
/// Ownership: Owned by the session; holds borrowed pointers to constexpr commands and descriptors (built-in and extensions').
/// Thread affinity: MAIN_THREAD; frozen after startup step 12, then read-only.
/// Invalidation: None.
/// Units: None.
/// Errors: Register: EDITOR_ERROR_EXTENSION_DUPLICATE, EDITOR_ERROR_EXTENSION_LIMIT (4,096 commands), EDITOR_ERROR_COMMAND_ARGS
///   (a malformed descriptor). Run: EDITOR_ERROR_COMMAND_UNKNOWN, EDITOR_ERROR_COMMAND_ARGS (naming the argument),
///   EDITOR_ERROR_COMMAND_DISABLED, and every history error.
/// Reentrancy: NONE. Run invokes the history or the command; a command never calls Run.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorCommandRegistry
{
public:
	Zenith_Result<void, Zenith_EditorError> Register(const Zenith_EditorCommand& xCommand,
		const Zenith_EditorCommandDesc& xDesc);
	void Freeze();
	const Zenith_EditorCommandDesc* Find(Zenith_EditorCommandID xID) const;
	const Zenith_EditorCommandDesc* FindByText(Zenith_StringView strIdentifier) const;
	Zenith_Result<void, Zenith_EditorError> ValidateArgs(Zenith_EditorCommandID xID,
		const Zenith_EditorCommandArgs& xArgs) const;
	// Validates, checks IsEnabled, then: CHANGES_DOCUMENTS → the history's Execute (the record ID, 0 for no change);
	// otherwise the command's Execute on a non-recording context (result 0).
	Zenith_Result<uint64, Zenith_EditorError> Run(Zenith_EditorCommandID xID, const Zenith_EditorCommandArgs& xArgs);
	uint32 List(Zenith_Span<const Zenith_EditorCommandDesc*> apxOut) const;   // byte order of m_szID
};
```

### Selection snapshot

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorSelection.h (beside Foundation.md "Selection model")
/// @contract value Zenith_EditorSelectionSnapshot
/// Ownership: Plain value whose reference array lives in the arena given to Zenith_EditorSelection::Snapshot (a record's arena).
/// Thread affinity: IMMUTABLE once taken; members non-const: Zenith_EditorSelection::Snapshot fills it field by field.
/// Invalidation: Invalid when its arena is reset or its record released.
/// Units: Counts.
/// Errors: None; Restore drops references whose objects no longer exist (Foundation.md "Selection persistence and mapping").
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorSelectionSnapshot
{
	Zenith_Span<const Zenith_EditorObjectRef> m_axRefs;   // selection order
	uint32 m_uActiveIndex = 0xFFFFFFFF;                   // index of the active reference, or none
	uint64 m_ulSelectionVersion = 0;                      // Zenith_EditorSelection::GetVersion when taken
};
```

`Zenith_EditorObjectRef` is 64 bytes: kind (1 byte, padded to 8), document ID (16), object ID (16), sub-ID (16), sub (8).

### Clipboard header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorClipboard.h
/// @contract enum Zenith_EditorClipboardKind
/// Ownership: Plain enumeration; the "kind" member of Formats.md "Clipboard document".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_EditorClipboardKind : uint8 { EDITOR_CLIPBOARD_EMPTY = 0, EDITOR_CLIPBOARD_SCENE_OBJECTS, EDITOR_CLIPBOARD_JSON_NODES, EDITOR_CLIPBOARD_ASSETS };

/// @contract value Zenith_EditorClipboardNode
/// Ownership: Plain value; both views point into the clipboard's arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the next Set or Clear.
/// Units: Pointers in the grammar of Zenith2/Docs/Design/CORE/Text.md "Pointer resolution steps".
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorClipboardNode { Zenith_StringView m_strPointer; Zenith_StringView m_strValueJson; };

/// @contract type Zenith_EditorClipboard
/// Ownership: One per session; owns an arena (reset by each Set) holding the copied set or nodes; borrows PLAT's
///   Zenith_Clipboard (nullptr in a headless session or a suite, where the system clipboard is not written). The borrowed
///   pointer belongs to the runtime's Zenith_Platform, so a runtime restart re-binds it (RebindSystemClipboard: nullptr in
///   restart step 4, the new platform's clipboard in step 7, Pipeline.md "Runtime restart"); the content is unaffected.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Views and sets it returns are invalid after the next Set, ReadSystemClipboard or Clear.
/// Units: Bytes.
/// Errors: EDITOR_ERROR_CLIPBOARD_FORMAT (system text that is not a known zenith2.editor-clipboard document, a wrong kind,
///   a newer version, an object failing validation), EDITOR_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2 (the internal clipboard and the clipboard document text, WriteDocument and ReadDocument;
///   Zenith_Clipboard is forward-declared and pxSystemClipboard is null in that slice); the system clipboard since Z2 Stage 3;
///   RebindSystemClipboard since Z2 Stage 4 (the runtime restart).
/// @endcontract
class Zenith_EditorClipboard
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorClipboard>, Zenith_EditorError> Create(Zenith_Heap& xHeap,
		Zenith_Clipboard* pxSystemClipboard, Zenith_StringView strGame);
	Zenith_Result<void, Zenith_EditorError> SetSceneObjects(const Zenith_SceneEditSet& xSet);
	Zenith_Result<void, Zenith_EditorError> SetJsonNodes(Zenith_Span<const Zenith_EditorClipboardNode> axNodes);
	Zenith_Result<void, Zenith_EditorError> SetAssets(Zenith_Span<const Zenith_AssetGUID> axAssets);
	// Parses the system clipboard's text under the default JSON limits and validates it object by object; on success the
	// internal content is replaced by it.
	Zenith_Result<void, Zenith_EditorError> ReadSystemClipboard();   // PLAT's text, then ReadDocument
	// Replaces the borrowed system clipboard (nullptr: no system clipboard until the next call); the internal content is kept,
	// and nothing is written to either clipboard by the call itself.
	void RebindSystemClipboard(Zenith_Clipboard* pxSystemClipboard);
	// The zenith2.editor-clipboard document text of the current content (Formats.md "Clipboard document"), and its validated
	// reader; the system-clipboard paths call these, and the tests assert through them without a platform clipboard.
	Zenith_Result<void, Zenith_EditorError> WriteDocument(Zenith_Array<char>& xTextOut) const;
	Zenith_Result<void, Zenith_EditorError> ReadDocument(Zenith_StringView strText);
	Zenith_EditorClipboardKind GetKind() const;
	const Zenith_SceneEditSet* GetSceneObjects() const;               // nullptr unless SCENE_OBJECTS
	Zenith_Span<const Zenith_EditorClipboardNode> GetJsonNodes() const;
	Zenith_Span<const Zenith_AssetGUID> GetAssets() const;
	void Clear();
};
```

### Event header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorEvents.h
/// @contract enum Zenith_EditorEventKind
/// Ownership: Plain enumeration; the events of Automation.md "Waiting and events" (the automation name in the comment).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2 (history and selection events); the others from the stage of their producer; values never move.
/// @endcontract
enum Zenith_EditorEventKind : uint8
{
	EDITOR_EVENT_SCENE_OPENED = 1,             // sceneOpened
	EDITOR_EVENT_DOCUMENT_SAVED,               // documentSaved
	EDITOR_EVENT_HISTORY_CHANGED,              // historyChanged
	EDITOR_EVENT_HISTORY_COMMITTED,            // historyCommitted
	EDITOR_EVENT_HISTORY_FAILED,               // historyFailed
	EDITOR_EVENT_SELECTION_CHANGED,            // selectionChanged
	EDITOR_EVENT_PLAY_STARTED,                 // playStarted
	EDITOR_EVENT_PLAY_STOPPED,                 // playStopped
	EDITOR_EVENT_REIMPORT_VISIBLE,             // reimportVisible
	EDITOR_EVENT_SCRIPT_RELOAD_LIVE,           // scriptReloadLive
	EDITOR_EVENT_SHADER_RELOAD_VISIBLE,        // shaderReloadVisible
	EDITOR_EVENT_TERRAIN_PREVIEW_VISIBLE,      // terrainPreviewVisible
	EDITOR_EVENT_ANIMATION_PREVIEW_VISIBLE,    // animationPreviewVisible
	EDITOR_EVENT_COOK_IDLE,                    // cookIdle
	EDITOR_EVENT_SCHEMA_APPLIED,               // schemaApplied
	EDITOR_EVENT_MIGRATION_APPLIED,            // migrationApplied
	EDITOR_EVENT_NOTIFICATION,                 // notification
	EDITOR_EVENT_AUTOSAVE_WRITTEN,             // autosaveWritten
	EDITOR_EVENT_RUNTIME_RESTARTED,            // runtimeRestarted
};

/// @contract value Zenith_EditorEvent
/// Ownership: Plain value; the text and ID views point into the bus's dispatch arena.
/// Thread affinity: IMMUTABLE during dispatch; members non-const: the bus builds each event field by field before dispatch.
/// Invalidation: The views are invalid after the listener call returns; a listener copies what it keeps.
/// Units: Microseconds of the host monotonic clock; ticks; frame indices.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorEvent
{
	Zenith_EditorEventKind m_eKind = EDITOR_EVENT_HISTORY_CHANGED;
	uint64 m_ulTimestampUs = 0;               // when the event occurred (for timed events: the completion of Operation timing)
	uint64 m_ulFrameIndex = 0;
	uint64 m_ulRecordID = 0;                  // HISTORY_COMMITTED, HISTORY_FAILED
	uint16 m_uWorldTag = 0;                   // play and script events
	uint64 m_ulTick = 0;
	uint32 m_uCount = 0;                      // entities, undo count, documents, generation
	uint32 m_uSecondCount = 0;                // redo count, stroke ordinal
	Zenith_EditorError m_xError{};             // HISTORY_FAILED
	Zenith_StringView m_strText;               // a path, a program, a module, a severity and text
	Zenith_Span<const Zenith_PersistentID> m_axIDs;   // GUIDs or documents
};

/// @contract interface Zenith_EditorEventListener
/// Ownership: Implemented by panels, the automation runner, the live link and operation timing; borrowed by the bus between
///   Subscribe and Unsubscribe, which the owner calls before it is destroyed.
/// Thread affinity: MAIN_THREAD. Called from Zenith_EditorEvents::Dispatch.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Reentrancy: ALLOWED(Publish). A listener may publish further events, which are dispatched after the current batch.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorEventListener
{
public:
	virtual ~Zenith_EditorEventListener() = default;
	virtual void OnEditorEvent(const Zenith_EditorEvent& xEvent) = 0;
};

/// @contract type Zenith_EditorEvents
/// Ownership: One per session; owns a pending queue of at most 4,096 events and a dispatch arena; borrows its listeners.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: A subscription token is invalid after Unsubscribe.
/// Units: Microseconds.
/// Errors: None returned; an event published into a full queue is dropped and counted (editor.events.dropped), and the
///   automation runner treats a drop as a failed wait.
/// Reentrancy: NONE for Dispatch itself; listeners may Publish (queued for the next batch).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_EditorEvents
{
public:
	uint32 Subscribe(Zenith_EditorEventListener& xListener, uint64 ulKindMask);   // bit (1 << kind)
	void Unsubscribe(uint32 uToken);
	void Publish(const Zenith_EditorEvent& xEvent);           // copies the views into the next batch's arena
	void Dispatch();                                          // end of editor frame steps 2 and 6, in publication order
};
```

### Session description

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorSession.h (beside Foundation.md "Session object")
/// @contract value Zenith_EditorSessionConfig
/// Ownership: Plain value of the preference values the core reads (EDIT.md "Editor configuration keys").
/// Thread affinity: IMMUTABLE for the session; members non-const: built field by field from the preference values.
/// Invalidation: None.
/// Units: As each key.
/// Errors: None of its own; values outside their ranges were replaced by defaults when the preferences file was read.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_EditorSessionConfig
{
	uint32 m_uMaxOpenDocuments = 512;
	uint64 m_ulProjectionArenaBytes = 64ull << 20, m_ulProjectionBufferBytes = 16ull << 20;
	uint32 m_uProjectionVerifyEveryN = 0;
	uint32 m_uMaxSelection = 100000;
	uint32 m_uMaxPlayWorlds = 4, m_uMaxPreviewWorlds = 8, m_uMaxSceneViews = 4;
	bool m_bSaveBeforePlay = false;
	uint32 m_uMigrationPlanDocumentsPerFrame = 64;
	uint32 m_uAssetOpsBatchThreshold = 64;
	Zenith_EditorHistoryDesc m_xHistory;      // the five undo keys and the asynchronous timeout; pointers filled by Create
};

/// @contract enum Zenith_EditorRuntimeRestartState
/// Ownership: Plain enumeration of a requested runtime restart.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_EditorRuntimeRestartState : uint8
{
	EDITOR_RUNTIME_RESTART_IDLE = 1,     // none requested since the last one completed
	EDITOR_RUNTIME_RESTART_REQUESTED,    // requested this frame; Zenith_EditorApp::Run performs it after RunOneFrame returns
	EDITOR_RUNTIME_RESTART_DONE,         // completed; reset to IDLE by the next request
};

/// @contract interface Zenith_EditorRuntimeHost
/// Ownership: Implemented by Zenith_EditorApp (and by a suite's test host); borrowed by the session for its lifetime.
/// Thread affinity: MAIN_THREAD. RequestRuntimeRestart is called from the history's Poll in editor frame step 2; the restart
///   itself runs between frames, in Zenith_EditorApp::Run after RunOneFrame returned (Foundation.md "Editor run loop").
/// Invalidation: None.
/// Units: None.
/// Errors: RequestRuntimeRestart returns EDITOR_ERROR_PLAY_STATE while a play world exists and EDITOR_ERROR_RUNTIME_RESTART
///   when one is already requested; a failed restart never returns to the caller: the host follows Pipeline.md "Runtime
///   restart" step 9 and the process ends with exit code 1.
/// Reentrancy: NONE. A request never runs the restart inside the call.
/// Version: since Z2 Stage 2 as a forward-declared member that is null (Zenith_EditorSessionDesc::m_pxRuntimeHost); the
///   interface is declared and implemented from Z2 Stage 4.
/// @endcontract
class Zenith_EditorRuntimeHost
{
public:
	virtual ~Zenith_EditorRuntimeHost() = default;
	virtual Zenith_Result<void, Zenith_EditorError> RequestRuntimeRestart() = 0;
	virtual Zenith_EditorRuntimeRestartState GetRuntimeRestartState() const = 0;
};

/// @contract value Zenith_EditorSessionDesc
/// Ownership: Plain value; every pointer is borrowed for the session's lifetime (the runtime, services, type registry and
///   string table until the next RebindRuntime).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; Zenith_EditorSession::Create returns EDITOR_ERROR_USAGE for a null required member (m_pxHeap,
///   m_pxTypes, m_pxStrings, m_pxRegistry, m_pxStorageRoots; for a runtime session also m_pxServices), and when a runtime
///   session's m_pxTypes or m_pxStrings differs from its services' pointer.
/// Version: since Z2 Stage 2 (documents, history, selection, clipboard; m_pxTypes and m_pxStrings non-null from that slice,
///   their types coming from ZenithReflect and ZenithCore, both Stage 2 edges of ZenithEditorCore). In the Stage 2 slice the
///   runtime, services, cook client, runtime host, system clipboard and host paths are forward-declared types whose members
///   are present and null (Zenith2/Docs/Design/EDIT.md "Dependency growth rule" rule 2); the runtime, services, cook client,
///   system clipboard and host paths are non-null from Z2 Stage 3 (worlds), the runtime host from Z2 Stage 4.
/// @endcontract
struct Zenith_EditorSessionDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_Runtime* m_pxRuntime = nullptr;                  // null: a document-only session (no worlds, no play)
	const Zenith_ToolServices* m_pxServices = nullptr;      // the references Zenith_RuntimeToolAccess::Create filled
	// Required for every session. A runtime session passes its services' m_pxTypes and m_pxStrings (RebindRuntime replaces
	// both); a document-only session passes a registry and string table its caller owns and keeps alive (a test: the
	// reflection fixtures registered into a test-owned Zenith_TypeRegistry, Tests.md "Fixtures"). The document table passes
	// m_pxTypes to every Zenith_EditorDocumentFactory::Open; the inspector, paste and JSON validators read it.
	const Zenith_TypeRegistry* m_pxTypes = nullptr;
	Zenith_StringTable* m_pxStrings = nullptr;
	const Zenith_MigrationRegistry* m_pxRegistry = nullptr; // frozen
	Zenith_EditorStorageRoots* m_pxStorageRoots = nullptr;
	Zenith_EditorCookClient* m_pxCookClient = nullptr;      // null with --no-cook-server and in suites without a server
	Zenith_EditorRuntimeHost* m_pxRuntimeHost = nullptr;    // null: the schema undo is refused (EDITOR_ERROR_RUNTIME_RESTART)
	Zenith_Clipboard* m_pxSystemClipboard = nullptr;
	const Zenith_EditorHostPaths* m_pxPaths = nullptr;
	Zenith_StringView m_strGame;
	uint64 m_ulRandomSeed = 0;                             // Foundation.md "Session random source": --random-seed, the script's seed, a test's value; 0: entropy
	bool m_bReadOnly = false;                               // startup step 8 found an unrecoverable journal
	Zenith_EditorSessionConfig m_xConfig;
};

// The members of Zenith_EditorSession itself, RebindRuntime and RegisterProjectionHandler among them: Foundation.md "Session object".
```

### World set header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorWorldSet.h
/// @contract enum Zenith_EditorWorldRole
/// Ownership: Plain enumeration of the editor's world roles; each maps to one X243 (b) Zenith_WorldPurpose.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorWorldRole : uint8
{
	EDITOR_WORLD_ROLE_MAIN_EDIT = 1,   // WORLD_PURPOSE_EDIT
	EDITOR_WORLD_ROLE_PREFAB_STAGE,    // WORLD_PURPOSE_EDIT
	EDITOR_WORLD_ROLE_PLAY,            // WORLD_PURPOSE_PLAY, origin WORLD_ORIGIN_EDITOR_PLAY
	EDITOR_WORLD_ROLE_PREVIEW,         // WORLD_PURPOSE_PREVIEW
};

/// @contract value Zenith_EditorWorldEntry
/// Ownership: Plain value owned by the world set; the world objects are borrowed from the runtime until Destroy.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Invalid after Destroy of its tag or a RebindRuntime.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorWorldEntry
{
	uint16 m_uTag = 0;
	Zenith_EditorWorldRole m_eRole = EDITOR_WORLD_ROLE_MAIN_EDIT;
	Zenith_ToolWorld m_xWorld;                         // Foundation.md "Runtime tool access"
	Zenith_EditorProjection* m_pxProjection = nullptr; // edit roles only
	Zenith_EditorProxyEntities* m_pxProxies = nullptr; // edit roles only
	Zenith_EditorDocumentID m_xPrefab;                 // PREFAB_STAGE: the staged prefab document
	uint64 m_ulSeed = 0;
};

/// @contract type Zenith_EditorProxyEntities
/// Ownership: One per edit world, owned by the world set; owns the persistent IDs of its proxies (Foundation.md
///   "Editor proxy entities") and a 64 KiB command-buffer arena reset after each Apply.
/// Thread affinity: MAIN_THREAD, between RunFrame calls.
/// Invalidation: None.
/// Units: Metres, radians.
/// Errors: Apply returns EDITOR_ERROR_PROJECTION (the ECS error is logged; the proxies are re-spawned at the next Apply);
///   Add beyond editor.viewports.maxScene proxies returns EDITOR_ERROR_DOCUMENT_LIMIT.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorProxyEntities
{
public:
	Zenith_Result<Zenith_PersistentID, Zenith_EditorError> Add(uint32 uViewportID);   // spawned at the next Apply
	void Remove(uint32 uViewportID);                                                              // destroyed at the next Apply
	void SetPose(uint32 uViewportID, const Zenith_Vector3& xPosition, const Zenith_Quaternion& xRotation,
		float fFovYRadians, bool bVisible);
	void SetAnchorViewport(uint32 uViewportID);            // the proxy that carries Zenith.Environment.Anchor
	Zenith_Result<void, Zenith_EditorError> Apply();   // step 2, after the projection's Apply
	bool IsProxy(const Zenith_PersistentID& xID) const;
	uint32 GetCount() const;
};

/// @contract type Zenith_EditorWorldSet
/// Ownership: Owned by the session; owns every world it created (through Zenith_RuntimeToolAccess), each world's projection
///   and proxies; borrows the runtime until RebindRuntime.
/// Thread affinity: MAIN_THREAD, between RunFrame calls.
/// Invalidation: A Zenith_EditorWorldEntry pointer is invalid after Destroy of its tag, DestroyAll or RebindRuntime.
/// Units: None.
/// Errors: EDITOR_ERROR_RUNTIME (CreateWorld's Zenith_RuntimeError is logged), EDITOR_ERROR_PLAY_WORLD_LIMIT,
///   EDITOR_ERROR_PREVIEW_UNAVAILABLE (a preview world in a headless session), EDITOR_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorWorldSet
{
public:
	Zenith_Result<uint16, Zenith_EditorError> CreateEditWorld(Zenith_EditorWorldRole eRole,
		Zenith_EditorDocumentID xPrefab, uint64 ulSeed);                  // MAIN_EDIT or PREFAB_STAGE
	Zenith_Result<uint16, Zenith_EditorError> CreatePlayWorld(uint64 ulSeed);
	Zenith_Result<uint16, Zenith_EditorError> CreatePreviewWorld(uint64 ulSeed);
	void Destroy(uint16 uTag);                                              // GAME's teardown through the tool access
	void DestroyAll();                                                       // play, then preview, then edit worlds
	const Zenith_EditorWorldEntry* Find(uint16 uTag) const;
	uint16 GetMainEditWorld() const;                          // 0 before session creation completes
	uint32 List(Zenith_EditorWorldRole eRole, Zenith_Span<uint16> auOut) const;   // creation order
	void RebindRuntime(Zenith_Runtime& xRuntime);                            // after DestroyAll, before re-creation
};
```

### Projection change header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorProjection.h (beside Foundation.md "Edit world projection")
/// @contract enum Zenith_EditorProjectionChangeKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorProjectionChangeKind : uint8
{
	EDITOR_PROJECTION_SCENE_EDIT = 1,         // one applied scene edit (forward or inverse), Foundation.md "Projection per edit kind"
	EDITOR_PROJECTION_RECONCILE_DOCUMENT,     // a full reconcile of one scene document
	EDITOR_PROJECTION_RECONCILE_INSTANCE,     // a reconcile of one prefab instance's resolved ID space
	EDITOR_PROJECTION_DOCUMENT_HANDLER,       // a non-scene document changed: run its registered handler
};

/// @contract value Zenith_EditorProjectionChange
/// Ownership: Plain value; m_pxEdit points into the applying record's arena, which outlives the frame's Apply.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the projection's next Apply.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorProjectionChange
{
	Zenith_EditorProjectionChangeKind m_eKind = EDITOR_PROJECTION_SCENE_EDIT;
	Zenith_EditorDocumentID m_xDocument;
	const Zenith_SceneEdit* m_pxEdit = nullptr;       // SCENE_EDIT: the edit as applied
	Zenith_PersistentID m_xInstance;                  // RECONCILE_INSTANCE
	Zenith_TypeID m_xDocumentType;                    // DOCUMENT_HANDLER
};

/// @contract interface Zenith_EditorProjectionHandler
/// Ownership: Implemented by the extension owning a non-scene document type (ENV's environment documents, GEOM's material
///   documents through the Materials extension); a constexpr singleton registered with the session.
/// Thread affinity: MAIN_THREAD, editor frame step 2, between RunFrame calls.
/// Invalidation: None.
/// Units: None.
/// Errors: Project returns EDITOR_ERROR_PROJECTION; the projection logs it and the next change retries.
/// Reentrancy: NONE. A handler never executes a command.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorProjectionHandler
{
public:
	virtual ~Zenith_EditorProjectionHandler() = default;
	// Called after every forward or inverse change of a document of the registered type, with the document's current state.
	virtual Zenith_Result<void, Zenith_EditorError> Project(const Zenith_EditorDocument& xDocument,
		const Zenith_EditorWorldEntry& xEditWorld, const Zenith_ToolServices& xServices, Zenith_CommandBuffer& xOutOfTick) = 0;
};
```

The projection's withheld component types (Foundation.md "Editor proxy entities") are a constexpr span in `Zenith_EditorProjection.cpp`, `axEDITOR_WITHHELD_COMPONENTS`, holding `Zenith.Environment.Anchor`.

### Play controller header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorPlay.h (Zenith_EditorPlayState: Foundation.md "Play state machine")
/// @contract value Zenith_EditorPlayWorldInfo
/// Ownership: Plain value copied out by Zenith_EditorPlayController::List.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPlayWorldInfo
{
	uint16 m_uTag = 0;
	Zenith_EditorPlayState m_eState = EDITOR_PLAY_STATE_EDITING;
	uint32 m_uOrdinal = 0;           // 1 for the session's first play world, +1 per Play; display only, never part of a seed
	uint64 m_ulTick = 0;
	uint32 m_uStepRemaining = 0;
	bool m_bInputWorld = false;
};

/// @contract value Zenith_EditorPlayCopyStats
/// Ownership: Plain value describing the last Play's copy (Foundation.md "Play copy normalisation").
/// Thread affinity: IMMUTABLE; members non-const: Play accumulates the counts in it.
/// Invalidation: None.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorPlayCopyStats
{
	uint32 m_uEntities = 0;              // entities in the play copy
	uint32 m_uDroppedEntities = 0;       // captured entities the documents do not hold (editor.play.copy_dropped_entities)
	uint32 m_uDroppedComponents = 0;     // captured components the documents do not hold
	uint32 m_uNormalisedSets = 0;        // captured field sets replaced by the documents' (editor.play.copy_normalised_sets)
	uint32 m_uRestored = 0;              // document components or entities absent from the capture (withheld, or drift)
	uint32 m_uDrift = 0;                 // of m_uRestored, those that were not withheld types: a projection error
};

/// @contract type Zenith_EditorPlayController
/// Ownership: Owned by the session; owns one Zenith_EditorHistory, one Zenith_EditorSelection and one capture arena per play
///   world; borrows the world set, the cook client and the documents.
/// Thread affinity: MAIN_THREAD, editor frame steps 2 and 6.
/// Invalidation: The history and selection pointers it returns are invalid after the world's Stop completes.
/// Units: Ticks.
/// Errors: EDITOR_ERROR_PLAY_STATE (Foundation.md "Starting play" step 1, an event the state table does not list),
///   EDITOR_ERROR_PLAY_WORLD_LIMIT, EDITOR_ERROR_PLAY_START_FAILED (creation or activation, the cause logged),
///   EDITOR_ERROR_HISTORY_PENDING, EDITOR_ERROR_COMMAND_ARGS (a step count outside 1 … 600).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorPlayController
{
public:
	Zenith_Result<uint16, Zenith_EditorError> Start();                 // a new play world in PREPARING
	Zenith_Result<void, Zenith_EditorError> Stop(uint16 uTag);
	void StopAll();
	Zenith_Result<void, Zenith_EditorError> Pause(uint16 uTag);
	Zenith_Result<void, Zenith_EditorError> Resume(uint16 uTag);
	Zenith_Result<void, Zenith_EditorError> Step(uint16 uTag, uint32 uTicks);
	void Focus(uint16 uTag);                                                         // 0: no input world
	// Hook forwarding (Foundation.md "Runtime tool access"): main thread, after RunFrame.
	void OnWorldHalted(uint16 uTag);
	void OnFrameEnd(uint64 ulFrameIndex);
	Zenith_Result<uint64, Zenith_EditorError> ApplyToDocument(uint16 uTag,
		Zenith_Span<const Zenith_PersistentID> axEntities);                             // a main-history record ID
	uint32 List(Zenith_Span<Zenith_EditorPlayWorldInfo> axOut) const;
	Zenith_EditorHistory* GetHistory(uint16 uTag);
	Zenith_EditorSelection* GetSelection(uint16 uTag);
	uint16 GetFocusedWorld() const;
	bool IsEditing() const;                                             // no play world exists
	const Zenith_EditorPlayCopyStats& GetLastCopyStats() const;
};
```

### Cook client types

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorCookClient.h (Zenith_EditorCookClient itself: Pipeline.md "Request queue")
/// @contract enum Zenith_EditorCookClientState
/// Ownership: Plain enumeration of Pipeline.md "Client state machine".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorCookClientState : uint8
{
	EDITOR_COOK_STATE_STOPPED = 1, EDITOR_COOK_STATE_STARTING, EDITOR_COOK_STATE_READY, EDITOR_COOK_STATE_WAITING_FOR_LOCK,
	EDITOR_COOK_STATE_RESTARTING, EDITOR_COOK_STATE_INCOMPATIBLE, EDITOR_COOK_STATE_FAILED,
};

/// @contract value Zenith_EditorCookResult
/// Ownership: Plain value; the views and the JSON document point into the client's per-request arena, valid during the
///   completion call only.
/// Thread affinity: IMMUTABLE during the completion call; members non-const: the client fills it field by field before the call.
/// Invalidation: Invalid after the completion returns.
/// Units: None.
/// Errors: m_bSucceeded false carries the server's error code name (COOK_ERROR_…) and message, or a local error in m_xError
///   (EDITOR_ERROR_COOK_SERVER_RESTARTED, EDITOR_ERROR_COOK_CANCELLED, EDITOR_ERROR_COOK_PROTOCOL).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorCookResult
{
	uint64 m_ulRequestID = 0;
	bool m_bSucceeded = false;
	Zenith_StringView m_strErrorCode;                 // e.g. "COOK_ERROR_META_CHANGED"; empty on success and for local errors
	Zenith_StringView m_strMessage;
	Zenith_EditorError m_xError{};
	const Zenith_JsonDocument* m_pxDocument = nullptr;   // the final result line, pages merged in page order
	Zenith_JsonNode m_xResult;
};

using Zenith_PfnEditorCookCompletion = void (*)(const Zenith_EditorCookResult& xResult, void* pUserData);

/// @contract value Zenith_EditorCookCompletion
/// Ownership: Plain value; pUserData is borrowed and must outlive the request (operations keep themselves alive in their record).
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorCookCompletion { Zenith_PfnEditorCookCompletion m_pfnComplete = nullptr; void* m_pUserData = nullptr; };

/// @contract interface Zenith_EditorCookListener
/// Ownership: Implemented by the asset browser, the diagnostics panel, reimport, the document table and the status bar;
///   borrowed by the client between AddListener and RemoveListener.
/// Thread affinity: MAIN_THREAD; called from Zenith_EditorCookClient::Poll.
/// Invalidation: The spans are valid for the call.
/// Units: Progress in [0, 1].
/// Errors: None.
/// Reentrancy: ALLOWED(Submit). A listener may submit requests; it never calls Poll.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorCookListener
{
public:
	virtual ~Zenith_EditorCookListener() = default;
	virtual void OnProgress(uint64 ulRequestID, float fProgress, Zenith_StringView strStage) = 0;   // 0: the watch run
	virtual void OnDiagnostic(const Zenith_JsonDocument& xLine, Zenith_JsonNode xDiagnostic) = 0;
	virtual void OnChanged(Zenith_Span<const Zenith_StringView> axSourcePaths) = 0;
	virtual void OnPublished(Zenith_Span<const Zenith_AssetGUID> axAssets, bool bCatalogueChanged) = 0;
	virtual void OnStateChanged(Zenith_EditorCookClientState eState) = 0;
};

/// @contract value Zenith_EditorCookClientDesc
/// Ownership: Plain value; the child process is owned by the client after Create (moved in); the strings are copied.
/// Thread affinity: IMMUTABLE; members non-const: Create moves m_xProcess out of it.
/// Invalidation: None.
/// Units: Milliseconds; requests.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorCookClientDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_Owned<Zenith_EditorChildProcess> m_xProcess;     // the Windows child process, or a test's scripted fake
	Zenith_StringView m_strExecutable;                      // <host tools dir>/ZenithCook.exe
	Zenith_StringView m_strGame;
	Zenith_StringView m_strGameRoot;                        // passed as --game-root when not the default Games root (row ED35)
	Zenith_StringView m_strOutDirectory;                    // ${ZENITH_OUT}/Cooked/<Name>/windows
	uint32 m_uDebounceMs = 250;                            // editor.cook.debounceMs
	uint32 m_uMaxQueued = 256;                             // editor.cook.maxQueued
};
// Create, Start, RemoveListener and IsIdle are declared with the client in Pipeline.md "Request queue".
```

### Content cook channel header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorContentCookChannel.h
#pragma once
#include "Authoring/Zenith_AuthoringContent.h"     // Zenith_ContentCookChannel, Zenith_ContentBatchOperation (CONTENT, row CT18)
#include "EditorCore/Zenith_EditorChange.h"

class Zenith_EditorCookClient;

/// @contract enum Zenith_EditorContentChannelMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_EditorContentChannelMode : uint8
{
	EDITOR_CONTENT_CHANNEL_MODE_PUMP = 1,   // the calling thread owns the client and polls it until the final line (ZenithContent)
	EDITOR_CONTENT_CHANNEL_MODE_FORWARD,    // the batch runs on EditorContentBatch; each call is forwarded to the main thread (the editor)
};

/// @contract type Zenith_EditorContentCookChannel
/// Ownership: Created by its owner (Zenith_EditorContentBatchOperation in the editor, ZenithContent's main) with the cook client
///   it borrows; one per batch in FORWARD mode, one per process in PUMP mode.
/// Thread affinity: OWNER_THREAD (CONTENT's channel contract: the thread that called the batch's Plan or Open), except
///   ForwardPending, MAIN_THREAD. No member blocks or waits on any primitive. PUMP: the owner thread is the client's one thread;
///   Begin* writes the request to the server's pipe and returns its ticket, and Poll drains the client's lines
///   (Zenith_EditorCookClient::Poll) without waiting. FORWARD: Begin* pushes the request onto a Zenith_BoundedQueue of 64 entries
///   (QUEUE_FULL_POLICY_BLOCK_PRODUCER; a batch has at most 32 requests outstanding, so the push never blocks) and returns its
///   ticket at once; the main thread's ForwardPending (editor frame step 2, after the cook client's Poll) submits each queued
///   request and pushes each finished request's completion onto a second Zenith_BoundedQueue of 64 entries
///   (QUEUE_FULL_POLICY_REJECT, never full for the same bound; a refused push is counted and fails that ticket with
///   CONTENT_ERROR_COOK_CHANNEL); Poll drains the completion queue into the owner's ticket table and returns
///   CONTENT_COOK_POLL_PENDING until its ticket's completion is there. Neither queue's semaphore is waited on (CONTENT's Run
///   sleeps 1 ms between polls); no lock is taken, so the channel has no lock-order row. Cancel is forwarded the same way.
/// Invalidation: A ticket is invalid after the Poll that returned DONE or an error, and after Cancel (CONTENT's rule).
/// Units: Milliseconds.
/// Errors: CONTENT's: CONTENT_ERROR_COOK_CHANNEL with the request's COOK_ERROR code index in m_uDetail, or 0 when the client
///   is not READY and not queueing (INCOMPATIBLE, FAILED, --no-cook-server).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_EditorContentCookChannel final : public Zenith_ContentCookChannel
{
public:
	Zenith_EditorContentCookChannel(Zenith_EditorCookClient& xClient, Zenith_EditorContentChannelMode eMode);
	// Zenith_ContentCookChannel (Zenith2/Docs/Design/CONTENT/BatchOperations.md "Cook channel"), each Begin* one server request
	// with its own request ID: BeginRecord → record, BeginList → list (every page) or find, BeginImport → import (bForce:
	// reimport), BeginSetSettings → setSettings, BeginDependents → dependents; Poll reads the request's final line; Cancel →
	// cancel naming the request ID.
	void ForwardPending();                                   // FORWARD mode, main thread, editor frame step 2
};

/// @contract type Zenith_EditorContentBatchOperation
/// Ownership: Constructed in a record's arena by the asset operations above editor.assetOps.batchThreshold documents
///   (Pipeline.md "Reference repair"), with the batch's desc and a copy of its spec; owns the EditorContentBatch thread while it
///   runs, the CONTENT batch that thread creates, a FORWARD channel and the batch's operation ID.
/// Thread affinity: MAIN_THREAD for every Zenith_EditorOperation member. The CONTENT batch is OWNER_THREAD of EditorContentBatch,
///   the thread that calls its Plan or Open; the two threads meet only through the channel's queues, a depth-1 progress queue
///   and the planned-documents hand-off, and neither waits on a primitive.
/// Invalidation: None.
/// Units: Chunks.
/// Errors: A failed batch is FAILED, definite, with the batch report's first error; Resolve reads the batch journal.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_EditorContentBatchOperation final : public Zenith_EditorOperation
{
public:
	// IsAsynchronous true. Begin(FORWARD) starts EditorContentBatch, which calls Plan (it returns in PLANNING, having read no
	// document and sent no request), then Run (planning; it returns at PLANNED), then reads GetReferencingDocuments of every
	// target and hands the lists to the main thread. Poll (main thread) runs CheckPlannedWrite over them: a dirty document
	// refuses the operation (the batch thread calls Abandon, then Discard, and the record fails with nothing written);
	// otherwise Poll calls DeferExternalChanges for every open clean target and sets the verdict, which the batch thread reads
	// with Zenith_SleepUs(1000) between reads, as CONTENT's Run polls, before calling Run again (execution). The thread destroys
	// the CONTENT batch, releasing its operation lock, and exits when Run returns. Poll: DONE when the thread has exited, then
	// adopt each open target's disk state (Pipeline.md "Reference repair"). Begin(INVERSE): a new EditorContentBatch thread calls
	// Open with the operation ID, then Rollback (CONTENT's states: PAUSED, COMPLETED, ABANDONED, ROLLING_BACK). Resolve: the
	// journal's completed chunks (PARTIAL or COMPLETED). Compensate: Rollback, the same way.
	// Progress: the batch's progress callback runs on EditorContentBatch after every unit and pushes the Zenith_ContentBatchProgress
	// onto a depth-1 Zenith_BoundedQueue (QUEUE_FULL_POLICY_DROP_OLDEST); Poll reads it for the progress display, planning's
	// m_uDocumentsIndexed of m_uDocumentsToIndex included. No per-frame Step call is made.
	Zenith_EditorContentBatchOperation(const Zenith_ContentBatchDesc& xDesc, const Zenith_ContentBatchSpec& xSpec);
};
```

### Child process header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorChildProcess.h
/// @contract enum Zenith_EditorChildStream
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_EditorChildStream : uint8 { EDITOR_CHILD_STREAM_STDOUT = 1, EDITOR_CHILD_STREAM_STDERR };

/// @contract value Zenith_EditorChildProcessDesc
/// Ownership: Plain value; every view is borrowed for the duration of Launch, which copies them.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Bytes; lines.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_EditorChildProcessDesc
{
	Zenith_StringView m_strExecutable;
	Zenith_Span<const Zenith_StringView> m_axArguments;     // an argument vector, never a shell string
	Zenith_StringView m_strWorkingDirectory;                // empty: the editor's
	Zenith_Span<const Zenith_StringView> m_axEnvironment;   // KEY=VALUE pairs added to the editor's environment
	bool m_bPipeStdin = true;
	uint32 m_uLineQueueCapacity = 4096;                    // per stream (Zenith_BoundedQueue, BLOCK_PRODUCER on the reader side)
	uint32 m_uMaxLineBytes = 4194304;                      // a longer line is discarded through its LF and counted
};

/// @contract interface Zenith_EditorChildProcess
/// Ownership: Created by Zenith_CreateEditorChildProcess (Internal/Windows/Zenith_EditorChildProcess_Windows.cpp: CreateProcessW
///   with three pipes, CREATE_NO_WINDOW, a job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, one reader thread per output
///   stream and one writer thread), or by a test's scripted fake; owned by its client (the cook client, the shader watcher,
///   the git backend, a schema-lock run, a test harness).
/// Thread affinity: ANY_THREAD_EXCEPT(Launch, WriteLine, TryReadLine, Terminate, WaitForExit, CloseStdin). Those run on the
///   owner's thread; the reader and writer threads touch only the queues.
/// Invalidation: None.
/// Units: Milliseconds.
/// Errors: Launch: EDITOR_ERROR_COOK_SERVER_UNAVAILABLE for a missing executable (the caller maps it to its own code),
///   EDITOR_ERROR_OUT_OF_MEMORY. WriteLine: EDITOR_ERROR_COOK_BUSY when the write queue is full. GetExitCode:
///   EDITOR_ERROR_NOT_FOUND while running.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_EditorChildProcess
{
public:
	virtual ~Zenith_EditorChildProcess() = default;     // terminates a running child and joins its threads
	virtual Zenith_Result<void, Zenith_EditorError> Launch(const Zenith_EditorChildProcessDesc& xDesc) = 0;
	virtual Zenith_Result<void, Zenith_EditorError> WriteLine(Zenith_StringView strLine) = 0;   // LF appended
	virtual bool TryReadLine(Zenith_EditorChildStream eStream, Zenith_String& strOut) = 0;   // without its LF
	virtual bool IsRunning() const = 0;
	virtual Zenith_Result<uint32, Zenith_EditorError> GetExitCode() const = 0;
	virtual void CloseStdin() = 0;
	virtual void Terminate() = 0;                       // TerminateJobObject; the reader threads see EOF
	virtual bool WaitForExit(uint32 uTimeoutMs) = 0;
};

Zenith_Result<Zenith_Owned<Zenith_EditorChildProcess>, Zenith_EditorError> Zenith_CreateEditorChildProcess(Zenith_Heap& xHeap);
```

### Migration model header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorMigrationModel.h
/// @contract value Zenith_EditorBackupBatch
/// Ownership: Plain value; the views point into the model's listing arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the next ListBackups.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_EditorBackupBatch { Zenith_StringView m_strName; Zenith_StringView m_strDirectory; uint32 m_uDocuments = 0; };

/// @contract value Zenith_EditorMigrationModelDesc
/// Ownership: Plain value; every pointer is borrowed for the model's lifetime.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Documents per frame.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_EditorMigrationModelDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_EditorStorageRoots* m_pxRoots = nullptr;
	Zenith_EditorDocumentTable* m_pxDocuments = nullptr;
	Zenith_EditorHistory* m_pxHistory = nullptr;
	Zenith_EditorEvents* m_pxEvents = nullptr;
	Zenith_EditorCookClient* m_pxCookClient = nullptr;     // null: .meta entries are not planned (reported as unavailable)
	uint32 m_uPlanDocumentsPerFrame = 64;
};

/// @contract type Zenith_EditorMigrationModel
/// Ownership: Owned by the session; owns one Zenith_MigrationPlan per root and the server's .meta preview per root.
/// Thread affinity: MAIN_THREAD. Step runs in editor frame step 2.
/// Invalidation: A plan pointer is invalid after the next BeginPlanning of its root or an Apply.
/// Units: Documents.
/// Errors: EDITOR_ERROR_MIGRATION_BLOCKED (a blocked entry, the first one's error in m_uDetail), EDITOR_ERROR_DOCUMENT
///   (STALE_PLAN, LOW_DISK, STORAGE_LOCKED wrapped), EDITOR_ERROR_COOK_SERVER_UNAVAILABLE for .meta entries without a server,
///   EDITOR_ERROR_HISTORY_PENDING, EDITOR_ERROR_DOCUMENT_READ_ONLY (a read-only root).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4 (plans of open documents since Z2 Stage 3).
/// @endcontract
class Zenith_EditorMigrationModel
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorMigrationModel>, Zenith_EditorError> Create(
		const Zenith_EditorMigrationModelDesc& xDesc);
	Zenith_Result<void, Zenith_EditorError> BeginPlanning(Zenith_EditorStorageRootID xRoot);
	void Step();                                     // plans at most m_uPlanDocumentsPerFrame documents
	bool IsPlanning() const;
	const Zenith_MigrationPlan* GetPlan(Zenith_EditorStorageRootID xRoot) const;   // .meta entries excluded
	const Zenith_JsonDocument* GetMetaPreview(Zenith_EditorStorageRootID xRoot) const;   // migratePreview
	Zenith_Result<void, Zenith_EditorError> WriteReport(Zenith_EditorStorageRootID xRoot, Zenith_StringView strHostPath) const;
	// One history record (Pipeline.md "Applying a migration"); asynchronous when .meta entries are selected.
	Zenith_Result<uint64, Zenith_EditorError> Apply(Zenith_EditorStorageRootID xRoot,
		Zenith_Span<const uint32> auEntries, bool bIncludeMeta);
	const Zenith_RecoveryReport& GetRecoveryReport(Zenith_EditorStorageRootID xRoot) const;
	uint32 ListBackups(Zenith_EditorStorageRootID xRoot, Zenith_Span<Zenith_EditorBackupBatch> axOut);
	Zenith_Result<uint64, Zenith_EditorError> RestoreBackup(Zenith_EditorStorageRootID xRoot,
		Zenith_StringView strBatch);
};
```

### Schema model header

```cpp
// Zenith2/Tools/EditorCore/Zenith_EditorSchemaModel.h
/// @contract value Zenith_EditorSchemaDiagnostic
/// Ownership: Plain value; the views point into the model's validation arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the next Validate.
/// Units: Lines and columns from 1.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_EditorSchemaDiagnostic
{
	Zenith_StringView m_strFile;
	Zenith_StringView m_strKeyPath;
	uint32 m_uLine = 0, m_uColumn = 0;
	Zenith_StringView m_strMessage;       // the validator's exact text, "<file>: <keyPath>: <message>"
};

/// @contract value Zenith_EditorSchemaModelDesc
/// Ownership: Plain value; every pointer is borrowed for the model's lifetime (the runtime until RebindRuntime).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_EditorSchemaModelDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_Runtime* m_pxRuntime = nullptr;
	Zenith_EditorWorldSet* m_pxWorlds = nullptr;
	Zenith_EditorPlayController* m_pxPlay = nullptr;
	Zenith_EditorMigrationModel* m_pxMigration = nullptr;
	Zenith_EditorHistory* m_pxHistory = nullptr;
	Zenith_EditorRuntimeHost* m_pxRuntimeHost = nullptr;
	const Zenith_EditorHostPaths* m_pxPaths = nullptr;     // ZenithCook.exe for the schema-lock run
	Zenith_StringView m_strGame;
};

/// @contract type Zenith_EditorSchemaModel
/// Ownership: Owned by the session; owns a scratch Zenith_TypeRegistry per Validate and the schema-lock child process while
///   it runs.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Diagnostic views are invalid after the next Validate.
/// Units: None.
/// Errors: EDITOR_ERROR_SCHEMA_INVALID (diagnostics listed), EDITOR_ERROR_SCHEMA_PLAY_ACTIVE ([Z2-N-ARCH-648]),
///   EDITOR_ERROR_HISTORY_PENDING, EDITOR_ERROR_COOK_REQUEST_FAILED for a failed schema-lock run (its stderr is logged),
///   EDITOR_ERROR_RUNTIME_RESTART for an undo without a runtime host.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_EditorSchemaModel
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_EditorSchemaModel>, Zenith_EditorError> Create(
		const Zenith_EditorSchemaModelDesc& xDesc);
	Zenith_Result<void, Zenith_EditorError> Validate(Zenith_StringView strPath, Zenith_ByteSpan xBytes);
	uint32 ListDiagnostics(Zenith_Span<Zenith_EditorSchemaDiagnostic> axOut) const;
	Zenith_Result<void, Zenith_EditorError> PreviewMigration(Zenith_StringView strPath);   // into the migration model
	// One asynchronous EDITOR_CHANGE_SCHEMA record (Panels.md "Schema panel workflow" step 3); committed at OnSchemaWindowResult.
	Zenith_Result<uint64, Zenith_EditorError> Apply(Zenith_StringView strPath);
	void OnSchemaWindowResult(const Zenith_SchemaWindowResult& xResult);      // forwarded from the tool hook
	// Runs `ZenithCook.exe schema-lock <Name>` (ECS's verb) as a child process; completes in a following frame.
	Zenith_Result<Zenith_OperationHandle, Zenith_EditorError> CommitLock();
	void Poll();                                                              // step 2: the schema-lock child
	void RebindRuntime(Zenith_Runtime& xRuntime);
};
```

## Declaration index

Every EDIT type named anywhere in the family, the header that declares it and the heading that declares it:

| Type | Header | Declared in |
|---|---|---|
| `Zenith_EditorApp`, `Zenith_EditorAppDesc` | `ZenithEditor/Internal/App/Zenith_EditorApp.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Application header" |
| `Zenith_EditorArgDesc`, `Zenith_EditorArgKind` | `EditorCore/Zenith_EditorCommandRegistry.h` | this file → "Command registry header" |
| `Zenith_EditorAssetKindHandler` | `ZenithEditor/Internal/Extensions/Zenith_EditorExtension.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Extension registration types" |
| `Zenith_EditorAutomationStepDesc` | the same | the same |
| `Zenith_EditorBackupBatch`, `Zenith_EditorMigrationModel`, `Zenith_EditorMigrationModelDesc` | `EditorCore/Zenith_EditorMigrationModel.h` | this file → "Migration model header" |
| `Zenith_EditorCacheConsumer` | `ZenithEditor/Internal/Pipeline/Zenith_EditorCacheConsumer.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Cache consumer header" |
| `Zenith_EditorChange`, `Zenith_EditorChangeKind`, `Zenith_EditorOperation`, `Zenith_EditorOperationContext`, `Zenith_EditorOperationDirection`, `Zenith_EditorOperationStatus`, `Zenith_EditorFailureKind`, `Zenith_EditorResolution`, `Zenith_EditorChangeResult`, `Zenith_EditorDeferredSceneOperation`, `Zenith_PfnEditorBuildDeferredEdits` | `EditorCore/Zenith_EditorChange.h` | this file → "Change header" |
| `Zenith_EditorContentChannelMode`, `Zenith_EditorContentCookChannel`, `Zenith_EditorContentBatchOperation` | `EditorCore/Zenith_EditorContentCookChannel.h` | this file → "Content cook channel header" |
| `Zenith_EditorChildProcess`, `Zenith_EditorChildProcessDesc`, `Zenith_EditorChildStream`, `Zenith_CreateEditorChildProcess` | `EditorCore/Zenith_EditorChildProcess.h` | this file → "Child process header" |
| `Zenith_EditorClipboard`, `Zenith_EditorClipboardKind`, `Zenith_EditorClipboardNode` | `EditorCore/Zenith_EditorClipboard.h` | this file → "Clipboard header" |
| `Zenith_EditorCloseMode`, `Zenith_EditorDocumentState`, `Zenith_EditorSaveReason`, `Zenith_EditorDocumentFactory` | `EditorCore/Zenith_EditorDocument.h` | this file → "Document kinds additions" |
| `Zenith_EditorCommandArgs`, `Zenith_EditorCommandContext` | `EditorCore/Zenith_EditorCommand.h` | this file → "Command header" |
| `Zenith_EditorCommandID`, `Zenith_EditorStorageRootID`, `Zenith_EditorDocumentID` | `EditorCore/Zenith_EditorIDs.h` | this file → "Identifier header" |
| `Zenith_EditorCommandRegistry` | `EditorCore/Zenith_EditorCommandRegistry.h` | this file → "Command registry header" |
| `Zenith_EditorCookClientState`, `Zenith_EditorCookCompletion`, `Zenith_EditorCookListener`, `Zenith_EditorCookResult`, `Zenith_EditorCookClientDesc` | `EditorCore/Zenith_EditorCookClient.h` | this file → "Cook client types" |
| `Zenith_EditorDocumentHandler`, `Zenith_EditorGizmoProvider`, `Zenith_EditorGizmoHandleDesc`, `Zenith_EditorInspectorContext`, `Zenith_EditorPickShape`, `Zenith_EditorPreferenceDesc`, `Zenith_EditorPreferenceKind`, `Zenith_PfnEditorBounds`, `Zenith_PfnEditorInspector` | `ZenithEditor/Internal/Extensions/Zenith_EditorExtension.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Extension registration types" |
| `Zenith_EditorDrawSnapshot`, `Zenith_EditorDrawCommand`, `Zenith_EditorDrawList` | `ZenithEditor/Internal/Shell/Zenith_EditorDrawSnapshot.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Draw snapshot header" |
| `Zenith_EditorECSBackendRow` | `ZenithEditor/Internal/App/Zenith_EditorModuleCatalogue.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "ECS backend row" |
| `Zenith_EditorEvents`, `Zenith_EditorEvent`, `Zenith_EditorEventKind`, `Zenith_EditorEventListener` | `EditorCore/Zenith_EditorEvents.h` | this file → "Event header" |
| `Zenith_EditorGridSettings`, `Zenith_EditorIconInstance`, `Zenith_EditorSelectedInstance` | `ZenithEditor/Internal/Viewport/Zenith_EditorOverlaySnapshot.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Overlay instance types" |
| `Zenith_EditorHistory`, `Zenith_EditorHistoryDesc`, `Zenith_EditorRecordState` | `EditorCore/Zenith_EditorHistory.h` | this file → "History header" |
| `Zenith_EditorHostPaths`, `Zenith_ResolveEditorHostPaths` | `EditorCore/Zenith_EditorHostPaths.h` | this file → "Host paths header" |
| `Zenith_EditorHostWatcher`, `Zenith_EditorHostChange`, `Zenith_EditorHostChangeKind` | `EditorCore/Zenith_EditorHostWatcher.h` | this file → "Host watcher header" |
| `Zenith_EditorNotifications`, `Zenith_EditorNotificationSeverity` | `ZenithEditor/Internal/Shell/Zenith_EditorNotifications.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Notifications header" |
| `Zenith_EditorPanelContext`, `Zenith_EditorPanelTypeDesc`, `Zenith_EditorPanelTypeID`, `Zenith_EditorPanelInstancing`, `Zenith_PfnCreateEditorPanel`, `Zenith_EditorRect` | `ZenithEditor/Internal/Shell/Zenith_EditorPanel.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Panel context header" |
| `Zenith_EditorPicker`, `Zenith_EditorPickFilter`, `Zenith_EditorPickHit` | `ZenithEditor/Internal/Viewport/Zenith_EditorPicker.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Picker header" |
| `Zenith_EditorPlayController`, `Zenith_EditorPlayWorldInfo`, `Zenith_EditorPlayCopyStats` | `EditorCore/Zenith_EditorPlay.h` | this file → "Play controller header" |
| `Zenith_EditorPreviewHost`, `Zenith_EditorPreviewKind` | `ZenithEditor/Internal/Pipeline/Zenith_EditorPreviewHost.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Preview host header" |
| `Zenith_EditorProjectionChange`, `Zenith_EditorProjectionChangeKind`, `Zenith_EditorProjectionHandler` | `EditorCore/Zenith_EditorProjection.h` | this file → "Projection change header" |
| `Zenith_EditorRuntimeHost`, `Zenith_EditorRuntimeRestartState`, `Zenith_EditorSessionConfig`, `Zenith_EditorSessionDesc` | `EditorCore/Zenith_EditorSession.h` | this file → "Session description" |
| `Zenith_EditorSchemaModel`, `Zenith_EditorSchemaModelDesc`, `Zenith_EditorSchemaDiagnostic` | `EditorCore/Zenith_EditorSchemaModel.h` | this file → "Schema model header" |
| `Zenith_EditorSelectionSnapshot` | `EditorCore/Zenith_EditorSelection.h` | this file → "Selection snapshot" |
| `Zenith_EditorStorageRoots`, `Zenith_EditorStorageRootDesc` | `EditorCore/Zenith_EditorStorageRoots.h` | this file → "Storage roots header" |
| `Zenith_EditorWorldSet`, `Zenith_EditorWorldEntry`, `Zenith_EditorWorldRole`, `Zenith_EditorProxyEntities` | `EditorCore/Zenith_EditorWorldSet.h` | this file → "World set header" |
| `Zenith_RegisterEditorDocumentTypes` | `EditorCore/Zenith_EditorDocumentTypes.h` | this file → "Document type registration header" |
| `Zenith_EditorSession`, `Zenith_EditorOpenMode` | `EditorCore/Zenith_EditorSession.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Session object" |
| `Zenith_EditorDocument`, `Zenith_EditorDocumentKind` | `EditorCore/Zenith_EditorDocument.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document interface" |
| `Zenith_EditorDocumentTable` | `EditorCore/Zenith_EditorDocumentTable.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Document table" |
| `Zenith_EditorJsonEdit`, `Zenith_EditorJsonEditKind`, `Zenith_ApplyEditorJsonEdit` | `EditorCore/Zenith_EditorJsonEdit.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "JSON edits" |
| `Zenith_EditorRecordInfo` | `EditorCore/Zenith_EditorHistory.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Command records" |
| `Zenith_EditorCommand` | `EditorCore/Zenith_EditorCommand.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executing a command" |
| `Zenith_EditorObjectRef`, `Zenith_EditorObjectKind`, `Zenith_EditorSelection` | `EditorCore/Zenith_EditorSelection.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Selection model" |
| `Zenith_EditorProjection` | `EditorCore/Zenith_EditorProjection.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Edit world projection" |
| `Zenith_EditorPlayState` | `EditorCore/Zenith_EditorPlay.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Play state machine" |
| `Zenith_EditorCookClient` | `EditorCore/Zenith_EditorCookClient.h` | `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Request queue" |
| `Zenith_EditorCommandFlag`, `Zenith_EditorCommandDesc` | `EditorCore/Zenith_EditorCommandRegistry.h` | `Zenith2/Docs/Design/EDIT/Automation.md` → "Command descriptors" |
| `Zenith_EditorOptions`, `Zenith_EditorRecoveryChoice`, `Zenith_EditorCommandLine` | `ZenithEditor/Internal/App/Zenith_EditorCommandLine.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Executable and command line" |
| `Zenith_EditorModuleRow` | `ZenithEditor/Internal/App/Zenith_EditorModuleCatalogue.h` | `Zenith2/Docs/Design/EDIT/Foundation.md` → "Module catalogue" |
| `Zenith_EditorTextureBackend`, `Zenith_EditorFluxTextureBackend` | `ZenithEditor/Internal/Shell/Zenith_EditorTextureBackend.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Texture backend header" |
| `Zenith_EditorAssetReloadSink` | `ZenithEditor/Internal/Pipeline/Zenith_EditorAssetReloadSink.h` | `Zenith2/Docs/Design/EDIT/ToolDeclarations.md` → "Asset reload sink header" |
| `Zenith_EditorTextureRef`, `Zenith_EditorTextureKind`, `Zenith_EditorTextureTable` | `ZenithEditor/Internal/Shell/Zenith_EditorTextureRef.h` | `Zenith2/Docs/Design/EDIT/Viewports.md` → "Editor texture references" |
| `Zenith_EditorPanel` | `ZenithEditor/Internal/Shell/Zenith_EditorPanel.h` | `Zenith2/Docs/Design/EDIT/Panels.md` → "Panel interface" |
| `Zenith_EditorExtension`, `Zenith_EditorExtensionContext` | `ZenithEditor/Internal/Extensions/Zenith_EditorExtension.h` | `Zenith2/Docs/Design/EDIT/Panels.md` → "Extension interface" |
| `Zenith_EditorOverlayVertex`, `Zenith_EditorOverlayView` | `ZenithEditor/Internal/Viewport/Zenith_EditorOverlaySnapshot.h` | `Zenith2/Docs/Design/EDIT/Viewports.md` → "Overlay records" |
| `Zenith_EditorErrorCode`, `Zenith_EditorError` | `EditorCore/Zenith_EditorError.h` | `Zenith2/Docs/Design/EDIT.md` → "Error enumeration" |

The internal types of single subsystems that no other file names (the pick-mesh cache, the panel host, the extension table, the automation runner, the live link, the git backend, the terrain sink, the graph canvas and its model, the timeline and curve editor) are declared by the ticket that implements them, in the header the "Internal directories" table of `Zenith2/Docs/Design/EDIT.md` → "Library and file layout" places them in, with a contract block by EDIT convention.
