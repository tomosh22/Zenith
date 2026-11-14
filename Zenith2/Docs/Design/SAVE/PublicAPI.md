# SAVE design — public API

This file is part of the SAVE design family. The entry document is `Zenith2/Docs/Design/SAVE.md`; this file holds every public declaration of `ZenithSave`, each with its contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type". The formats these types read and write are `Zenith2/Docs/Design/SAVE/Formats.md`; the algorithms are `Zenith2/Docs/Design/SAVE/Algorithms.md`.

## Scope of this file

- Every root header of `Zenith2/Engine/ZenithSave/`, in full, with its includes, constants, enumerations, value types, interfaces and classes.
- A pointer to the SAVE-owned declarations of the runtime bridge, which are `Zenith2/Docs/Design/SAVE/RuntimeBridge.md`.
- The Luau-facing `Save` binding module (its functions, payloads and the events scripts observe).

Rules every declaration below follows (they restate nothing; each is the rule of the named section):

- Every fallible function returns `Zenith_Result<T, Zenith_SaveError>` or `Zenith_Result<T, Zenith_ReplayError>` (`Zenith2/Docs/Contracts.md` → "Errors and results").
- No STL type appears in a public header; spans, string views and the JSON writer are CORE's (`Zenith2/Docs/Contracts.md` → "Containers and standard library policy").
- Error records are string-free trivially copyable structs of at most 32 bytes whose first member is `m_eCode`; the context text is logged at the point of failure.
- No public type is a test double; tests drive the real `Zenith_SaveSystem` over the real `Zenith_AsyncIO` of the headless test host or over the `Zenith_SaveStorage` interface's in-memory implementation in the header-only `Tests/Support/Zenith_MemorySaveStorage.h` ([Z2-N-ARCH-255]).
- Every namespace-scope struct, class and enum carries its own contract block immediately before it ([Z2-N-CONTRACT-111]); a type that invokes a callback (a sink, a participant, a mismatch callback) states `Reentrancy` and holds a `Zenith_ReentrancyGuard` ([Z2-N-CONTRACT-350], [Z2-N-CONTRACT-351]).

## Header list

| Header | Declares | Section |
|---|---|---|
| `Zenith_SaveTypes.h` | version constants, `Zenith_SaveKind`, `Zenith_SaveErrorCode`, `Zenith_SaveError`, `Zenith_SaveVersion`, `Zenith_SaveSlotName`, `Zenith_SaveProfileKey`, `Zenith_SaveFidelity`, `Zenith_ToString` overloads | § "Zenith_SaveTypes header" |
| `Zenith_SaveFormat.h` | `Zenith_SaveSectionKind`, `Zenith_SaveFileHeader`, `Zenith_SaveSectionEntry`, `Zenith_SaveMetadataPair`, `Zenith_SaveMetadata`, `Zenith_SaveImageBuildDesc`, `Zenith_SaveImageBuilder`, `Zenith_SaveImageView`, `Zenith_SaveCrc32c`, `Zenith_ParseSaveHeader` | § "Zenith_SaveFormat header" |
| `Zenith_SaveStorage.h` | `Zenith_SaveCancelToken`, `Zenith_SaveStorage`, `Zenith_CreateAsyncIOSaveStorage` | § "Zenith_SaveStorage header" |
| `Zenith_SaveJournal.h` | `Zenith_SaveJournalState`, `Zenith_SaveJournalRecord`, `Zenith_SaveWriteTarget`, `Zenith_EncodeSaveJournal`, `Zenith_DecodeSaveJournal`, `Zenith_RunJournaledSaveWrite`, `Zenith_SaveRecoveryOutcome`, `Zenith_SaveRecoveryEntry`, `Zenith_RecoverSaveJournals`, `Zenith_SaveRecoveryReport` | § "Zenith_SaveJournal header" |
| `Zenith_SaveParticipant.h` | `Zenith_SaveReissueSet`, `Zenith_SaveParticipantFlags`, `Zenith_SaveSectionWriter`, `Zenith_SaveSectionReader`, `Zenith_SaveCaptureContext`, `Zenith_SaveRestoreContext`, `Zenith_SaveExclusionList`, `Zenith_SaveParticipant`, `Zenith_SaveWorldHooks`, `Zenith_SaveParticipantKeyOf` | § "Zenith_SaveParticipant header" |
| `Zenith_WorldSnapshot.h` | `Zenith_SaveReferenceResolver`, `Zenith_WorldSnapshotDesc`, `Zenith_CapturedSection`, `Zenith_WorldSnapshotter`, `Zenith_SaveAsyncState`, `Zenith_SaveAsyncDisposition`, `Zenith_SaveAsyncRestoreEntry`, `Zenith_SaveUnknownComponent`, `Zenith_SaveUnknownResource`, `Zenith_SaveUnknownSection`, `Zenith_SaveUnknownCarry`, `Zenith_WorldRestoreDesc`, `Zenith_WorldRestoreState`, `Zenith_WorldRestoreJob` | § "Zenith_WorldSnapshot header" |
| `Zenith_SaveMigration.h` | `Zenith_SaveImageTree`, `Zenith_PfnSaveFormatMigrationStep`, `Zenith_SaveMigrationRegistry`, `Zenith_MigrateSaveImage` | § "Zenith_SaveMigration header" |
| `Zenith_SaveSystem.h` | `Zenith_SaveCompletionSink`, `Zenith_SaveSystemDesc`, `Zenith_SaveOperation`, `Zenith_SaveRequest`, `Zenith_SaveResult`, `Zenith_SaveSlotState`, `Zenith_SaveSlotInfo`, `Zenith_SaveSlotListener`, `Zenith_SaveExportedSlot`, `Zenith_PreparedSave`, `Zenith_RetainedCheckpointLease`, `Zenith_SaveScriptLoad`, `Zenith_SaveSystemStats`, `Zenith_SaveSystem`, `uSAVE_EXIT_AUTOSAVE_BUDGET_MS`, `uSAVE_EXIT_ROLLBACK_WAIT_MS`, `Zenith_ReadLastSaveProfile` | § "Zenith_SaveSystem header" |
| `Zenith_ReplayLog.h` | Architecture's ZRPL enumerations, `Zenith_BuildID`, `Zenith_GetRunningBuildID`, `Zenith_ReplayErrorCode`, `Zenith_ReplayIdentityField`, `Zenith_ReplayError`, `Zenith_ReplayConfigPair`, `Zenith_ReplayHeaderInfo`, `Zenith_ReplaySink`, `Zenith_ReplayMemorySink`, `Zenith_CreateReplayHostFileSink`, `Zenith_ReplayLogWriterDesc`, `Zenith_ReplayLogWriter`, `Zenith_ReplayTickView`, `Zenith_ReplayWorldSession`, `Zenith_ReplayCheckpointView`, `Zenith_ReplayPublicationPeek`, `Zenith_ReplayLogReader`, `Zenith_ReplayMismatch`, `Zenith_ReplayVerifierDesc`, `Zenith_ReplayVerifier` | `Zenith2/Docs/Design/SAVE/ReplayAPI.md` → "Zenith_ReplayLog header" |
| `Zenith_SaveRegistration.h` | `Zenith_RegisterSaveTypes`, `Zenith_RegisterSaveConfigKeys`, the engine type IDs SAVE registers, `Zenith_BuildSaveScriptBindings` | § "Zenith_SaveRegistration header" |

Every header begins `#pragma once`, includes only what it names, and compiles alone (`HeaderSelfContained`).

## Zenith_SaveTypes header

<!-- elaboration -->
```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveTypes.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"

inline constexpr uint16 uSAVE_FORMAT_VERSION_MAJOR = 1;          // a major bump refuses every older save (SAVE_ERROR_VERSION_MAJOR)
inline constexpr uint16 uSAVE_FORMAT_VERSION_MINOR = 0;          // a minor bump adds one migration step and one fixture
inline constexpr uint64 ulSAVE_MAX_FILE_BYTES = 268435456ull;    // 256 MiB: Architecture's parser limit and PLAT's io.maxWriteBytes
inline constexpr uint32 uSAVE_MAX_SECTIONS = 1024;
inline constexpr uint32 uSAVE_MAX_WORLDS = 16;
inline constexpr uint32 uSAVE_MAX_SLOT_NAME_BYTES = 32;
inline constexpr uint32 uSAVE_MAX_DISPLAY_NAME_BYTES = 128;
inline constexpr uint32 uSAVE_MAX_METADATA_PAIRS = 32;           // per file: the game's pairs plus the three engine pairs
inline constexpr uint32 uSAVE_MAX_GAME_METADATA_PAIRS = 29;      // per request: uSAVE_MAX_METADATA_PAIRS minus zenith.location,
                                                                  // zenith.release and zenith.profile
inline constexpr const char* szSAVE_ENGINE_METADATA_PREFIX = "zenith.";   // reserved: a game key beginning with it is refused
inline constexpr uint32 uSAVE_MAX_METADATA_KEY_BYTES = 64;
inline constexpr uint32 uSAVE_MAX_METADATA_VALUE_BYTES = 256;
inline constexpr uint32 uSAVE_MAX_THUMBNAIL_WIDTH = 512;
inline constexpr uint32 uSAVE_MAX_THUMBNAIL_HEIGHT = 288;
inline constexpr uint32 uSAVE_LISTING_PREFIX_BYTES = 131072;    // a slot listing reads at most this prefix of each file; it holds
                                                                  // the header, a full 1,024-entry table and a maximal METADATA
                                                                  // section (128 + 57,344 + 10,512 = 67,984 bytes)
inline constexpr uint32 uSAVE_MAX_PUBLICATION_BYTES = 16384;     // the ASYNC_SOURCE_SAVE maximum result size
inline constexpr uint32 uSAVE_MAX_LISTED_SLOTS = 64;
inline constexpr sint32 iSAVE_ANDROID_MAX_COMPRESSION_LEVEL = 3;  // COMPRESS's S24 per-context memory guidance

/// @contract enum Zenith_SaveKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A value outside the enumerators read from a file is SAVE_ERROR_CORRUPT.
/// Version: since Z2 Stage 3. Serialized as one byte in the ZSAV header; values are never renumbered or reused.
/// @endcontract
enum Zenith_SaveKind : uint8
{
	SAVE_KIND_MANUAL = 1,        // a named player slot
	SAVE_KIND_QUICK = 2,         // the one quick slot "quick"
	SAVE_KIND_AUTOSAVE = 3,      // the rotating slots "autosave-0" … "autosave-<n-1>"
	SAVE_KIND_CHECKPOINT = 4,    // the one checkpoint slot "checkpoint", also kept in memory
	SAVE_KIND_RESUME = 5,        // user:/saves/resume.zsave, the primary world only, never migrated (PLAT policy)
	SAVE_KIND_REPLAY_CHECKPOINT = 6,   // an in-memory image embedded in a ZRPL CHECKPOINT record; never a file; header
	                                   // sequence 0 and UTC seconds 0 (legal for this kind only)
};

/// @contract enum Zenith_SaveErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. The table "SAVE error codes" of Zenith2/Docs/Design/SAVE.md → "Error enumerations" states when each value
///   is returned and whether it is recoverable.
/// Version: since Z2 Stage 3. An enumerator keeps its value and meaning once shipped; it is also the uint16 failure code
///   of every SAVE operation handle and the result byte of every ASYNC_SOURCE_SAVE publication.
/// @endcontract
enum Zenith_SaveErrorCode : uint8
{
	SAVE_ERROR_INVALID_DESC = 1,
	SAVE_ERROR_OUT_OF_MEMORY = 2,
	SAVE_ERROR_THREAD_CREATE = 3,
	SAVE_ERROR_INVALID_SLOT_NAME = 4,
	SAVE_ERROR_RESERVED_SLOT_NAME = 5,
	SAVE_ERROR_QUEUE_FULL = 6,
	SAVE_ERROR_SHUTTING_DOWN = 7,
	SAVE_ERROR_IMAGE_TOO_LARGE = 8,
	SAVE_ERROR_SECTION_LIMIT = 9,
	SAVE_ERROR_SERIALIZE = 10,
	SAVE_ERROR_PARTICIPANT = 11,
	SAVE_ERROR_COMPRESSION = 12,
	SAVE_ERROR_IO = 13,
	SAVE_ERROR_LOW_DISK = 14,
	SAVE_ERROR_DISK_FULL = 15,
	SAVE_ERROR_CANCELLED = 16,
	SAVE_ERROR_NOT_FOUND = 17,
	SAVE_ERROR_BAD_MAGIC = 18,
	SAVE_ERROR_TRUNCATED = 19,
	SAVE_ERROR_CHECKSUM = 20,
	SAVE_ERROR_CORRUPT = 21,
	SAVE_ERROR_VERSION_NEWER = 22,
	SAVE_ERROR_VERSION_MAJOR = 23,
	SAVE_ERROR_MIGRATION = 24,
	SAVE_ERROR_SLOT_NEWER_VERSION = 25,
	SAVE_ERROR_RESTORE = 26,
	SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING = 27,
	SAVE_ERROR_JOURNAL_CORRUPT = 28,
	SAVE_ERROR_RECOVERY_CONFLICT = 29,
	SAVE_ERROR_PROFILE_BUSY = 30,
	SAVE_ERROR_WORLD_NOT_BOUND = 31,
	SAVE_ERROR_IDENTITY_MISMATCH = 32,
	SAVE_ERROR_SUPPRESSED_IN_REPLAY = 33,
	SAVE_ERROR_SUPERSEDED = 34,
	SAVE_ERROR_REQUEST_LIMIT = 35,
	SAVE_ERROR_REFUSED_IN_REPLAY = 36,
	SAVE_ERROR_TRANSITION_BUSY = 37,          // a script Save.Load whose world already holds a transition or load in flight
	SAVE_ERROR_PROFILE_MISMATCH = 38,         // Stage 4: a world bound under another profile (Algorithms.md "Profile switch intake freeze")
};
constexpr const char* Zenith_ToString(Zenith_SaveErrorCode eCode);   // "SAVE_ERROR_IO", …
constexpr bool Zenith_IsSaveErrorRecoverableByBackup(Zenith_SaveErrorCode eCode);
	// true for BAD_MAGIC, TRUNCATED, CHECKSUM, CORRUPT, COMPRESSION: the load retries the slot's .bak

/// @contract value Zenith_SaveError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480]: 16 bytes, string-free.
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uOsCode is the OS error code of an I/O failure (GetLastError, errno), 0 otherwise; m_uSectionKind a
///   Zenith_SaveSectionKind value or 0; m_ulDetail per code: SAVE_ERROR_SERIALIZE the type ID of the component, event or
///   resource whose field set failed; SAVE_ERROR_PARTICIPANT and SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING the participant
///   key; BAD_MAGIC, TRUNCATED, CHECKSUM and CORRUPT the byte offset of the failing header field for a header failure and
///   the section index for a section-table or section-payload failure (m_uSectionKind then names its kind); SAVE_ERROR_RESTORE
///   the Zenith_ECSError code of an ApplyOutOfTick failure, the simulation ID of the entity that closes a parent cycle, the
///   saved ASYNC_SOURCE_SAVE entry count when it exceeds this build's declared capacity (RestoreAsyncState), or the first
///   unready participant's key when the synchronous restore driver times out;
///   SAVE_ERROR_MIGRATION the source minor of a failed format step, or the type ID of a failed field-set migration;
///   SAVE_ERROR_LOW_DISK the bytes the write needed; 0 for every other code.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveError
{
	Zenith_SaveErrorCode m_eCode;
	uint8 m_uWorldIndex = 0xFF;      // the world of a per-world failure, 0xFF for file-level
	uint16 m_uSectionKind = 0;
	uint32 m_uOsCode = 0;
	uint64 m_ulDetail = 0;
};
static_assert(sizeof(Zenith_SaveError) == 16);

/// @contract value Zenith_SaveVersion
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Serialized as two uint16 in the ZSAV header.
/// @endcontract
struct Zenith_SaveVersion
{
	uint16 m_uMajor = 0;
	uint16 m_uMinor = 0;
	friend constexpr bool operator==(const Zenith_SaveVersion&, const Zenith_SaveVersion&) = default;
};
constexpr Zenith_SaveVersion Zenith_GetCurrentSaveVersion();   // { uSAVE_FORMAT_VERSION_MAJOR, _MINOR }

/// @contract value Zenith_SaveSlotName
/// Ownership: Plain value holding up to 32 bytes inline and a length; never allocates.
/// Thread affinity: IMMUTABLE once constructed. Members non-const: a copy-assignable ID, reassigned where it is held in
///   mutable state, a container or an out-parameter.
/// Invalidation: None.
/// Units: Bytes of ASCII.
/// Errors: Zenith_ParseSaveSlotName returns SAVE_ERROR_INVALID_SLOT_NAME for a name that is empty, longer than 32 bytes or
///   holds a byte outside [a-z0-9_-], or that begins with '-' or '_'; SAVE_ERROR_RESERVED_SLOT_NAME, for every kind, for a
///   Windows reserved device name ("con", "prn", "aux", "nul", "com1" … "com9", "lpt1" … "lpt9"; reserved even with an
///   extension, so "con.zsave" could never be created), and, for a manual name, for "quick", "checkpoint" or "resume" or a
///   name beginning with "autosave-".
/// Version: since Z2 Stage 3. The name is the file stem; the character set keeps every name valid on every file system the
///   user mount reaches and case-insensitively unique on Windows.
/// @endcontract
struct Zenith_SaveSlotName
{
	char m_acBytes[uSAVE_MAX_SLOT_NAME_BYTES] = {};
	uint8 m_uLength = 0;
	Zenith_StringView GetView() const;
	friend constexpr bool operator==(const Zenith_SaveSlotName&, const Zenith_SaveSlotName&) = default;
};
Zenith_Result<Zenith_SaveSlotName, Zenith_SaveError> Zenith_ParseSaveSlotName(Zenith_StringView strName,
	Zenith_SaveKind eKind);              // eKind MANUAL applies the reserved-name rule; the engine kinds build their own names
Zenith_SaveSlotName Zenith_GetEngineSlotName(Zenith_SaveKind eKind, uint32 uAutosaveIndex);
	// QUICK → "quick", CHECKPOINT → "checkpoint", AUTOSAVE → "autosave-<index>", RESUME → "resume"; ZENITH_ASSERT for MANUAL

/// @contract value Zenith_SaveProfileKey
/// Ownership: Plain value: 16 lowercase hexadecimal characters, or "local".
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Zenith_MakeSaveProfileKey cannot fail.
/// Version: since Z2 Stage 3. The directory name of a profile under user:/saves/; derived with SHA-256 so it never changes.
/// @endcontract
struct Zenith_SaveProfileKey
{
	char m_acBytes[16] = {};
	uint8 m_uLength = 0;              // 16, or 5 for "local"
	Zenith_StringView GetView() const;
	friend constexpr bool operator==(const Zenith_SaveProfileKey&, const Zenith_SaveProfileKey&) = default;
};
Zenith_SaveProfileKey Zenith_GetLocalSaveProfileKey();            // "local"
Zenith_SaveProfileKey Zenith_MakeSaveProfileKey(Zenith_StringView strAccountID);
	// the first 8 bytes of Zenith_Sha256Digest(account ID bytes), as 16 lowercase hex digits; an empty ID gives "local"

/// @contract enum Zenith_SaveFidelity
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. A capture's fidelity is the weakest any participant reports for it
///   (Zenith_SaveParticipant::GetCaptureFidelity); a COHERENT capture sets uSAVE_HEADER_FLAG_COHERENT_ONLY.
/// @endcontract
enum Zenith_SaveFidelity : uint8
{
	SAVE_FIDELITY_EXACT = 1,         // a restore continues bit-identically to the uninterrupted run (same build, backend, inputs,
	                                 // under the delivery rule of SAVE.md "Reproducibility boundaries")
	SAVE_FIDELITY_COHERENT = 2,      // a restore is consistent but its continuation may differ from the uninterrupted run
};
```

## Zenith_SaveFormat header

The ZSAV container: its byte layout is `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSAV file"; these types read and write it and never copy a C++ object's bytes (Z2-D-038).

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveFormat.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCompression/Zenith_CompressedSection.h"   // Zenith_CompressedSectionDesc
#include "ZenithSave/Zenith_SaveTypes.h"

class Zenith_Arena;
class Zenith_Compression;            // ZenithCompression's codec interface (Zenith2/Docs/Design/COMPRESS/PublicAPI.md)
class Zenith_CompressionCodecSet;
class Zenith_FaultInjector;          // ZenithCore's fault injector (tests)

/// @contract enum Zenith_SaveSectionKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A reader skips a section of an unknown kind (keeping it in Zenith_SaveUnknownCarry) unless the entry's
///   required flag is set, which is SAVE_ERROR_CORRUPT.
/// Version: since Z2 Stage 3. Serialized as uint16; values never renumbered.
/// @endcontract
enum Zenith_SaveSectionKind : uint16
{
	SAVE_SECTION_METADATA = 1,          // file-level; stored, never compressed
	SAVE_SECTION_THUMBNAIL = 2,         // file-level, optional; stored
	SAVE_SECTION_WORLD = 3,             // per world: identity, tick, counters
	SAVE_SECTION_ENTITIES = 4,          // per world: every entity and its serialized components
	SAVE_SECTION_EVENTS = 5,            // per world: the carried events of the snapshot tick
	SAVE_SECTION_ASYNC = 6,             // per world: next completion ID, unpublished results, in-flight IDs
	SAVE_SECTION_RESOURCES = 7,         // per world: every world resource with a serialized property (Formats.md "Resources section")
	SAVE_SECTION_UNKNOWN_CARRY = 8,     // per world: components and sections kept from a load that could not interpret them
	SAVE_SECTION_PARTICIPANT = 16,      // per world: one participant's section, keyed by the participant key
};

inline constexpr uint32 uSAVE_FILE_MAGIC = 0x5641535Au;          // "ZSAV" little-endian
inline constexpr uint32 uSAVE_HEADER_BYTES = 128;
inline constexpr uint32 uSAVE_SECTION_ENTRY_BYTES = 56;
inline constexpr uint16 uSAVE_WORLD_INDEX_FILE = 0xFFFFu;
inline constexpr uint16 uSAVE_SECTION_FLAG_REQUIRED = 0x0001;    // a reader that cannot interpret the section refuses the file
inline constexpr uint32 uSAVE_HEADER_FLAG_HAS_THUMBNAIL = 0x0001;
inline constexpr uint32 uSAVE_HEADER_FLAG_HAS_REPLAY_POSITION = 0x0002;
inline constexpr uint32 uSAVE_HEADER_FLAG_COHERENT_ONLY = 0x0004;   // some participant reported SAVE_FIDELITY_COHERENT for this capture

/// @contract value Zenith_SaveFileHeader
/// Ownership: Plain value decoded from or encoded into the 128-byte header (Formats.md "File header").
/// Thread affinity: IMMUTABLE. Members non-const: the image builder fills the header field by field before writing it.
/// Invalidation: None.
/// Units: m_ulUtcSeconds is seconds since 1970-01-01T00:00:00Z; m_ulTickIndex is the primary world's tick; sizes in bytes.
/// Errors: None of its own; Zenith_ParseSaveHeader validates.
/// Version: since Z2 Stage 3. The 128-byte layout is format major 1; a layout change is a major bump.
/// @endcontract
struct Zenith_SaveFileHeader
{
	Zenith_SaveVersion m_xVersion;
	uint32 m_uFlags = 0;
	Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	uint8 m_uEcsBackend = 0;                 // Zenith_BackendID (diagnostic; a restore is backend-neutral)
	uint16 m_uSectionCount = 0;
	uint64 m_ulSequence = 0;                 // strictly increasing per profile; the autosave rotation's ordering key
	uint64 m_ulUtcSeconds = 0;
	uint8 m_auBuildID[16] = {};
	uint64 m_ulCompositionHash = 0;
	uint64 m_ulContentHash = 0;
	uint64 m_ulTickIndex = 0;
	uint64 m_ulFileBytes = 0;
	uint32 m_uReplayWorldOrdinal = 0xFFFFFFFFu;   // [Z2-N-ARCH-789] position; valid when HAS_REPLAY_POSITION
	uint64 m_ulReplayTick = 0;
	uint16 m_uReleaseMajor = 0;              // the game's release line, from Zenith_BuildIdentity::m_strReleaseLine
	uint16 m_uReleaseMinor = 0;
	uint32 m_uSectionTableCrc = 0;
	uint64 m_ulOwnCompletionID = 0;          // the writing request's ASYNC_SOURCE_SAVE completion ID, 0 for none
	uint32 m_uOwnRequestSerial = 0;          // that request's serial (offset 120), 0 when m_ulOwnCompletionID is 0; with the kind,
	                                          // slot and file size it rebuilds the request's own result payload at a restore
};

/// @contract value Zenith_SaveSectionEntry
/// Ownership: Plain value decoded from or encoded into one 56-byte section-table entry.
/// Thread affinity: IMMUTABLE. Members non-const: the image builder patches entries in place in the section table.
/// Invalidation: None.
/// Units: Bytes; m_ulOffset from the start of the file.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveSectionEntry
{
	Zenith_SaveSectionKind m_eKind = SAVE_SECTION_METADATA;
	uint16 m_uSectionVersion = 0;
	uint16 m_uWorldIndex = uSAVE_WORLD_INDEX_FILE;
	uint16 m_uFlags = 0;
	uint64 m_ulKey = 0;                      // the participant key for SAVE_SECTION_PARTICIPANT, else 0
	uint64 m_ulOffset = 0;
	Zenith_CompressedSectionDesc m_xCompression;
	uint32 m_uStoredCrc = 0;                 // CRC-32C of the stored bytes
	uint32 m_uDecodedCrc = 0;                // CRC-32C of the decoded bytes
};

/// @contract value Zenith_SaveMetadataPair
/// Ownership: Plain value; both views borrow the caller's strings (a write) or the decoded section (a read).
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its backing bytes.
/// Units: Bytes of UTF-8.
/// Errors: None of its own; the builder refuses a key above 64 bytes or outside [A-Za-z0-9_.] and a value above 256 bytes
///   with SAVE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveMetadataPair
{
	const Zenith_StringView m_strKey = {};    // at most 64 bytes, [A-Za-z0-9_.]
	const Zenith_StringView m_strValue = {};  // at most 256 bytes of UTF-8
};

/// @contract value Zenith_SaveMetadata
/// Ownership: Plain value; every view borrows the arena or buffer the metadata was decoded from, or the caller's strings
///   when built for a write.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its backing bytes.
/// Units: m_ulPlaytimeSeconds of simulation time (the primary world's playtime ticks / sim.tickHz); text is UTF-8.
/// Errors: None of its own; the builder refuses oversized values with SAVE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveMetadata
{
	const Zenith_SaveSlotName m_xSlot = {};
	const Zenith_StringView m_strDisplayName = {}; // at most 128 bytes; empty means the slot name is shown
	const uint64 m_ulPlaytimeSeconds = 0;
	const uint32 m_uWorldCount = 0;
	const Zenith_Span<const Zenith_SaveMetadataPair> m_axPairs = {}; // at most 32 (29 game pairs + 3 engine pairs), keys unique, in
	                                                        // byte order of key
};

/// @contract functions Zenith_SaveCrc32c
/// Ownership: Pure functions over caller memory.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes; the CRC-32C (Castagnoli, reflected polynomial 0x82F63B78, initial value and final XOR 0xFFFFFFFF).
/// Errors: None; a null pointer with a non-zero size is ZENITH_ASSERT.
/// Version: since Z2 Stage 3. Persisted in every ZSAV and ZSJN file; the algorithm never changes.
/// @endcontract
uint32 Zenith_SaveCrc32c(Zenith_ByteSpan xBytes);
uint32 Zenith_SaveCrc32cUpdate(uint32 uCrc, Zenith_ByteSpan xBytes);   // uCrc 0 starts a new value

/// @contract functions Zenith_SaveHeaderFunctions
/// Ownership: Pure functions; outputs are values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: SAVE_ERROR_TRUNCATED (fewer than 128 bytes, or a table that runs past the input), SAVE_ERROR_BAD_MAGIC,
///   SAVE_ERROR_CHECKSUM (header or table CRC), SAVE_ERROR_CORRUPT (a field outside its rule), SAVE_ERROR_VERSION_NEWER,
///   SAVE_ERROR_VERSION_MAJOR; untrusted input never asserts ([Z2-N-ARCH-951]).
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_SaveFileHeader, Zenith_SaveError> Zenith_ParseSaveHeader(Zenith_ByteSpan xPrefix);
Zenith_Result<void, Zenith_SaveError> Zenith_ParseSaveSectionTable(Zenith_ByteSpan xPrefix,
	const Zenith_SaveFileHeader& xHeader, Zenith_Span<Zenith_SaveSectionEntry> axOut);   // axOut holds m_uSectionCount
Zenith_Result<Zenith_SaveMetadata, Zenith_SaveError> Zenith_ParseSaveMetadata(Zenith_ByteSpan xSectionBytes,
	Zenith_Arena& xArena);

/// @contract value Zenith_SaveImageBuildDesc
/// Ownership: Plain value; both pointers borrowed for the build.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_iLevel is the codec's compression level.
/// Errors: None of its own; Begin refuses a level outside 1 … 9 with SAVE_ERROR_INVALID_DESC.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveImageBuildDesc
{
	const Zenith_Compression* const m_pxCodec = nullptr; // null: every section stored (codec NONE)
	const sint32 m_iLevel = 3;                        // save.compressionLevel, clamped to iSAVE_ANDROID_MAX_COMPRESSION_LEVEL on Android
	Zenith_FaultInjector* const m_pxFaultInjector = nullptr; // tests: uFAULT_POINT_SAVE_COMPRESS
};

/// @contract type Zenith_SaveImageBuilder
/// Ownership: Owned by its creator (the save writer thread, a test); all memory comes from the arena passed to Begin, whose
///   capacity is the image budget (Z2-B-SAVE-001); the builder itself allocates nothing.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Spans returned by Finish are valid until the arena is reset.
/// Units: Bytes.
/// Errors: SAVE_ERROR_IMAGE_TOO_LARGE when the arena or ulSAVE_MAX_FILE_BYTES is exceeded; SAVE_ERROR_SECTION_LIMIT above
///   uSAVE_MAX_SECTIONS or uSAVE_MAX_WORLDS; SAVE_ERROR_COMPRESSION from the codec; SAVE_ERROR_INVALID_DESC for a duplicate
///   (kind, world, key), for metadata outside its limits, or for header sequence 0 with a kind other than
///   SAVE_KIND_REPLAY_CHECKPOINT.
/// Version: since Z2 Stage 3. Writes format version Zenith_GetCurrentSaveVersion() only.
/// @endcontract
class Zenith_SaveImageBuilder
{
public:
	Zenith_Result<void, Zenith_SaveError> Begin(const Zenith_SaveFileHeader& xHeader, const Zenith_SaveMetadata& xMetadata,
		const Zenith_SaveImageBuildDesc& xDesc, Zenith_Arena& xArena);
	Zenith_Result<void, Zenith_SaveError> SetThumbnail(uint16 uWidth, uint16 uHeight, Zenith_ByteSpan xRgba8);
	// Adds a section from its decoded bytes; compressed (except METADATA, THUMBNAIL) with m_bStoreIfNotSmaller = true.
	Zenith_Result<void, Zenith_SaveError> AddSection(Zenith_SaveSectionKind eKind, uint16 uSectionVersion,
		uint16 uWorldIndex, uint64 ulKey, uint16 uFlags, Zenith_ByteSpan xDecoded);
	// Lays out header, table, METADATA, THUMBNAIL, then the other sections in (world index, kind, key) order; fills every CRC,
	// the file size and the table CRC; returns the whole file.
	Zenith_Result<Zenith_ByteSpan, Zenith_SaveError> Finish(Zenith_Arena& xArena);
	uint32 GetSectionCount() const;
};
inline constexpr uint32 uFAULT_POINT_SAVE_COMPRESS = 0x53560001;

/// @contract type Zenith_SaveImageView
/// Ownership: A non-owning view of a whole ZSAV file in caller memory; decoded sections go to the caller's arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when the file bytes are released; decoded spans when the arena is reset.
/// Units: Bytes.
/// Errors: Open returns the header and table errors of Zenith_ParseSaveHeader, plus SAVE_ERROR_TRUNCATED when
///   m_ulFileBytes differs from the input length or a section runs past it, and SAVE_ERROR_CORRUPT for overlapping
///   sections; DecodeSection returns SAVE_ERROR_CHECKSUM (stored or decoded CRC), SAVE_ERROR_COMPRESSION (any
///   Zenith_DecompressSection error) and SAVE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 3. Reads every format version Zenith_IsSaveVersionReadable accepts.
/// @endcontract
class Zenith_SaveImageView
{
public:
	static Zenith_Result<Zenith_SaveImageView, Zenith_SaveError> Open(Zenith_ByteSpan xFile, Zenith_Arena& xArena);
	const Zenith_SaveFileHeader& GetHeader() const;
	Zenith_Span<const Zenith_SaveSectionEntry> GetSections() const;
	const Zenith_SaveSectionEntry* FindSection(Zenith_SaveSectionKind eKind, uint16 uWorldIndex, uint64 ulKey) const;
	Zenith_Result<Zenith_ByteSpan, Zenith_SaveError> DecodeSection(const Zenith_SaveSectionEntry& xEntry,
		const Zenith_CompressionCodecSet& xCodecs, Zenith_Arena& xArena) const;
	Zenith_Result<Zenith_SaveMetadata, Zenith_SaveError> GetMetadata(Zenith_Arena& xArena) const;
	Zenith_Result<Zenith_ByteSpan, Zenith_SaveError> VerifyAll(const Zenith_CompressionCodecSet& xCodecs,
		Zenith_Arena& xArena) const;      // decodes every section once, checking both CRCs; returns nothing useful but the status
private:
	Zenith_ByteSpan m_xFile;
	Zenith_SaveFileHeader m_xHeader;
	Zenith_Span<const Zenith_SaveSectionEntry> m_axSections;
};
constexpr bool Zenith_IsSaveVersionReadable(Zenith_SaveVersion xVersion);
	// major == uSAVE_FORMAT_VERSION_MAJOR and minor <= uSAVE_FORMAT_VERSION_MINOR
```

## Zenith_SaveStorage header

<!-- elaboration -->
The storage seam: every byte SAVE reads or writes on disk goes through it. The production implementation runs over PLAT's `Zenith_AsyncIO` and `Zenith_VirtualFileSystem` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API", "Atomic writes", "Other user-mount operations"); the in-memory implementation `Zenith_MemorySaveStorage` (header-only, `Zenith2/Engine/ZenithSave/Tests/Support/Zenith_MemorySaveStorage.h`, [Z2-N-ARCH-255]) serves power-loss tests.

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveStorage.h
#pragma once
#include <atomic>                                        // Zenith_SaveCancelToken::m_bRequested (an allowed standard header)
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithSave/Zenith_SaveTypes.h"

class Zenith_AsyncIO;
class Zenith_VirtualFileSystem;
class Zenith_Arena;
class Zenith_Heap;

/// @contract type Zenith_SaveCancelToken
/// Ownership: Plain object owned by the operation it belongs to (one per write, load or listing).
/// Thread affinity: ANY_THREAD. Request is called by any thread; IsRequested by the save writer thread.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveCancelToken
{
public:
	void Request();                         // release store; idempotent
	bool IsRequested() const; // acquire load
private:
	std::atomic<bool> m_bRequested{ false };          // <atomic> is an allowed standard header
};

/// @contract interface Zenith_SaveStorage
/// Ownership: Created by Zenith_CreateAsyncIOSaveStorage (production) or by a test; owned by the Zenith_SaveSystemDesc's
///   creator (ZenithRuntime) and outlives the save system.
/// Thread affinity: OWNER_THREAD. Called by the save writer thread, and by the main thread only while the writer holds no job
///   and every SAVE queue is empty (Zenith_SaveSystem::Create before the thread starts, SetActiveProfile, ReadResumeIdentity
///   at startup step 17); every member blocks until its operation completes and is therefore never called on a job worker
///   (ZENITH_CHECK wherever ZENITH_ASSERTS_ENABLED=1).
/// Invalidation: Spans returned by ReadWhole and List live in the caller's arena.
/// Units: Paths are user-mount virtual paths relative to user:/ (for example "saves/local/quick.zsave"); sizes in bytes.
/// Errors: SAVE_ERROR_NOT_FOUND, SAVE_ERROR_IO (with the OS code), SAVE_ERROR_LOW_DISK, SAVE_ERROR_DISK_FULL,
///   SAVE_ERROR_CANCELLED (the token was requested before the operation's commit point), SAVE_ERROR_TRUNCATED (ReadWhole of
///   a file above ulMaxBytes), SAVE_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_SaveStorage
{
public:
	virtual ~Zenith_SaveStorage() = default;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_SaveError> ReadWhole(Zenith_StringView strPath, uint64 ulMaxBytes,
		Zenith_Arena& xArena, const Zenith_SaveCancelToken& xCancel) = 0;
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_SaveError> ReadPrefix(Zenith_StringView strPath, uint64 ulBytes,
		Zenith_Arena& xArena) = 0;         // at most ulBytes from offset 0 (fewer when the file is shorter)
	// Atomic whole-file replace: after success the target holds exactly xBytes; after any failure or crash it holds its old
	// bytes or is absent if it was absent (PLAT's W1–W7).
	virtual Zenith_Result<void, Zenith_SaveError> WriteAtomic(Zenith_StringView strPath, Zenith_ByteSpan xBytes,
		const Zenith_SaveCancelToken& xCancel) = 0;
	virtual Zenith_Result<void, Zenith_SaveError> Rename(Zenith_StringView strFrom, Zenith_StringView strTo) = 0;
	virtual Zenith_Result<void, Zenith_SaveError> Delete(Zenith_StringView strPath) = 0;   // missing succeeds
	virtual Zenith_Result<void, Zenith_SaveError> CreateDirectory(Zenith_StringView strPath) = 0;
	virtual Zenith_Result<bool, Zenith_SaveError> Exists(Zenith_StringView strPath) = 0;
	// Names of the files directly inside strDirectory, in byte order; directories are not listed.
	virtual Zenith_Result<Zenith_Span<const Zenith_StringView>, Zenith_SaveError> ListFiles(Zenith_StringView strDirectory,
		Zenith_Arena& xArena) = 0;
	virtual Zenith_Result<uint64, Zenith_SaveError> QueryFreeBytes() = 0;   // ~0ull when unknown
	// The free space WriteAtomic itself keeps in reserve: PLAT's io.lowDiskMarginBytes for the production storage, a
	// test-chosen value for the in-memory one; the journal's free-space check adds it (Algorithms.md "Journaled write sequence").
	virtual uint64 GetLowDiskMarginBytes() const = 0;
};

/// @contract functions Zenith_SaveStorageFactory
/// Ownership: Returns an owned storage borrowing xAsyncIO and xFiles, which outlive it.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: uPollIntervalUs is the writer thread's completion-poll interval (default 250 µs).
/// Errors: SAVE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_SaveStorage>, Zenith_SaveError> Zenith_CreateAsyncIOSaveStorage(
	Zenith_AsyncIO& xAsyncIO, Zenith_VirtualFileSystem& xFiles, Zenith_Heap& xHeap, uint32 uPollIntervalUs = 250);
```

`std::atomic` is in an allowed standard header (`Zenith2/Docs/Contracts.md` → "Allowed standard headers"); the header includes `<atomic>`.

## Zenith_SaveJournal header

The journaled atomic write of `Zenith2/Docs/Contracts.md` → "Journaled atomic write" for one save file, its `.bak` policy ([Z2-N-CONTRACT-553]) and its start-up recovery. The journal byte layout is `Zenith2/Docs/Design/SAVE/Formats.md` → "The ZSJN journal"; the step sequence is `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Journaled write sequence".

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveJournal.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Sha256.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithSave/Zenith_SaveStorage.h"

class Zenith_FaultInjector;

/// @contract enum Zenith_SaveJournalState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Serialized as one byte in ZSJN; the names are Contracts' journal states.
/// @endcontract
enum Zenith_SaveJournalState : uint8
{
	SAVE_JOURNAL_STATE_PREPARED = 1,
	SAVE_JOURNAL_STATE_BACKED_UP = 2,
	SAVE_JOURNAL_STATE_WRITTEN = 3,
	SAVE_JOURNAL_STATE_COMMITTING = 4,
};

/// @contract value Zenith_SaveJournalRecord
/// Ownership: Plain value; the decoded form of one ZSJN file.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Sizes in bytes.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveJournalRecord
{
	const Zenith_SaveJournalState m_eState = SAVE_JOURNAL_STATE_PREPARED;
	const uint8 m_uFlags = 0;                // bit 0 old file existed; bit 1 backup written; bit 2 renamed; bit 3 old file failed validation
	const uint64 m_ulSequence = 0;           // the header sequence of the new save
	const Zenith_SaveSlotName m_xSlot = {};
	const Zenith_Sha256Value m_xOldDigest = {}; // all zero when the old file did not exist
	const Zenith_Sha256Value m_xNewDigest = {};
	const uint64 m_ulOldBytes = 0;
	const uint64 m_ulNewBytes = 0;
};
inline constexpr uint8 uSAVE_JOURNAL_FLAG_OLD_EXISTED = 0x01;
inline constexpr uint8 uSAVE_JOURNAL_FLAG_BACKUP_WRITTEN = 0x02;
inline constexpr uint8 uSAVE_JOURNAL_FLAG_RENAMED = 0x04;
inline constexpr uint8 uSAVE_JOURNAL_FLAG_OLD_INVALID = 0x08;
inline constexpr uint32 uSAVE_JOURNAL_BYTES = 176;

Zenith_Result<void, Zenith_SaveError> Zenith_EncodeSaveJournal(const Zenith_SaveJournalRecord& xRecord,
	Zenith_MutableByteSpan xOut);             // xOut holds uSAVE_JOURNAL_BYTES
Zenith_Result<Zenith_SaveJournalRecord, Zenith_SaveError> Zenith_DecodeSaveJournal(Zenith_ByteSpan xIn);
	// SAVE_ERROR_JOURNAL_CORRUPT for a wrong length, magic, version, state, slot name or CRC

/// @contract value Zenith_SaveWriteTarget
/// Ownership: Plain value naming the storage directory and slot of one write.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveWriteTarget
{
	const Zenith_StringView m_strDirectory = {}; // "saves/<profile>" or "saves" for the resume kind
	const Zenith_SaveSlotName m_xSlot = {};
	const bool m_bKeepBackup = true;          // false for SAVE_KIND_RESUME (PLAT: no backup of the resume state)
	const bool m_bOverwriteNewer = false;     // permit replacing a save of a newer format version
};

/// @contract functions Zenith_SaveJournalFunctions
/// Ownership: Borrow the storage, the arena and the token for the call.
/// Thread affinity: OWNER_THREAD. The save writer thread (and the main thread at Create for recovery).
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_RunJournaledSaveWrite returns the storage errors, SAVE_ERROR_SLOT_NEWER_VERSION, SAVE_ERROR_CANCELLED
///   (only before step 4 begins: the rename is never cancelled) and SAVE_ERROR_JOURNAL_CORRUPT (a pending journal of the
///   slot is unreadable); after any failure the slot holds its old file, its old .bak and no .z2tmp.
///   Zenith_RecoverSaveJournals never fails as a whole: each journal's outcome is reported.
/// Version: since Z2 Stage 3.
/// @endcontract
inline constexpr uint32 uFAULT_POINT_SAVE_JOURNAL_STEP = 0x53560010;   // + step number 1–5, and 0x53560020 + rename
Zenith_Result<void, Zenith_SaveError> Zenith_RunJournaledSaveWrite(Zenith_SaveStorage& xStorage,
	const Zenith_SaveWriteTarget& xTarget, Zenith_ByteSpan xNewFile, uint64 ulSequence, Zenith_Arena& xScratch,
	const Zenith_SaveCancelToken& xCancel, Zenith_FaultInjector* pxFaultInjector);

/// @contract enum Zenith_SaveRecoveryOutcome
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Reported and logged, never serialized; values never renumbered.
/// @endcontract
enum Zenith_SaveRecoveryOutcome : uint8
{
	SAVE_RECOVERY_ROLLED_BACK = 1,      // PREPARED, BACKED_UP, or WRITTEN with a bad temporary: the old file stands
	SAVE_RECOVERY_ROLLED_FORWARD = 2,   // WRITTEN with a valid temporary, or COMMITTING: the new file stands
	SAVE_RECOVERY_RESTORED_BACKUP = 3,  // COMMITTING found the target torn: the .bak was renamed back
	SAVE_RECOVERY_CONFLICT = 4,         // nothing matched the journal: files left untouched, journal kept as .conflict
	SAVE_RECOVERY_JOURNAL_CORRUPT = 5,  // journal unreadable: renamed to .corrupt, temporaries removed
};

/// @contract value Zenith_SaveRecoveryEntry
/// Ownership: Plain value; one entry of a Zenith_SaveRecoveryReport.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; m_eOutcome is the result of that journal's recovery.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveRecoveryEntry
{
	const Zenith_SaveSlotName m_xSlot = {};
	const Zenith_SaveRecoveryOutcome m_eOutcome = {};
	const Zenith_SaveJournalState m_eFoundState = {};
};
/// @contract value Zenith_SaveRecoveryReport
/// Ownership: Plain value; m_axEntries borrows the caller's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveRecoveryReport
{
	const Zenith_Span<const Zenith_SaveRecoveryEntry> m_axEntries = {}; // in byte order of journal file name
	const uint32 m_uStaleTemporariesRemoved = 0;              // *.z2tmp with no journal
};
Zenith_SaveRecoveryReport Zenith_RecoverSaveJournals(Zenith_SaveStorage& xStorage, Zenith_StringView strDirectory,
	Zenith_Arena& xArena);
```

## Zenith_SaveParticipant header

<!-- elaboration -->
The upward-callback interface SAVE declares ([Z2-N-ARCH-078]): every library or module above `ZenithSave` whose simulation state lives outside reflected components and world resources implements one participant per world and registers it with `Zenith_SaveSystem::RegisterParticipant` at world attach (state that is a reflected world resource needs no participant: the RESOURCES section saves it, `Zenith2/Docs/Design/SAVE/Formats.md` → "Resources section"). A participant never calls back into the save system, and its capture members change no simulation state.

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveParticipant.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_OperationHandle.h"      // ListRestoreWaits
#include "ZenithECS/Zenith_EntityHandle.h"          // Zenith_EntityHandle (WriteEntityReference, ReadEntityReference)
#include "ZenithSave/Zenith_SaveTypes.h"

class Zenith_SystemBuilder;
class Zenith_WorldSimulation;
class Zenith_WorldInspector;
class Zenith_Arena;
class Zenith_SaveReferenceResolver;

/// @contract type Zenith_SaveReissueSet
/// Ownership: Owned by the restore job for one restore; borrowed by a participant through Zenith_SaveRestoreContext.
/// Thread affinity: MAIN_THREAD, during the restore job's PARTICIPANTS step.
/// Invalidation: Invalid when the restore job leaves PARTICIPANTS.
/// Units: Completion IDs of the world being restored.
/// Errors: MarkReissued returns false for an ID that was not an in-flight entry of a source kind the participant declared in
///   GetReissuedSourceKinds (the participant then must not re-issue it: it is not left in flight).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveReissueSet
{
public:
	// Records that the participant re-issued the in-flight entry ulCompletionID under that same ID. Every declared entry left
	// unmarked when PARTICIPANTS ends is completed with the zero-length "abandoned at a restore" payload (Formats.md "Async
	// section").
	bool MarkReissued(uint64 ulCompletionID);
	// True when ulCompletionID was in flight at the snapshot under uSourceKind (a read of the saved ASYNC section).
	bool WasInFlight(uint64 ulCompletionID, uint8 uSourceKind) const;
};

/// @contract value Zenith_SaveParticipantFlags
/// Ownership: Plain bit set.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
using Zenith_SaveParticipantFlags = uint32;
inline constexpr Zenith_SaveParticipantFlags uSAVE_PARTICIPANT_FLAG_REQUIRED = 0x1;    // its section is marked REQUIRED
inline constexpr Zenith_SaveParticipantFlags uSAVE_PARTICIPANT_FLAG_COHERENT = 0x2;    // every capture of it is COHERENT (the default
                                                                                        // GetCaptureFidelity reads this bit)
inline constexpr Zenith_SaveParticipantFlags uSAVE_PARTICIPANT_FLAG_EXCLUDE_FROM_RESUME = 0x4;   // left out of SAVE_KIND_RESUME images
                                                                                        // (absent: included, which is the default)

/// @contract type Zenith_SaveSectionWriter
/// Ownership: Owned by the snapshotter; borrowed by a participant for one CaptureSection call; writes into the snapshot arena.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when CaptureSection returns.
/// Units: Bytes; every integer little-endian.
/// Errors: Every member returns false once the section exceeds the remaining image budget; the snapshotter then fails the save
///   with SAVE_ERROR_IMAGE_TOO_LARGE and the participant returns its own failure or success unchanged.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveSectionWriter
{
public:
	bool WriteU8(uint8 uValue);
	bool WriteU16(uint16 uValue);
	bool WriteU32(uint32 uValue);
	bool WriteU64(uint64 ulValue);
	bool WriteF32Bits(float fValue);          // the IEEE-754 bit pattern
	bool WriteF64Bits(double dValue);
	bool WriteBytes(Zenith_ByteSpan xBytes);
	bool WriteLengthPrefixed(Zenith_ByteSpan xBytes);   // uint32 length then bytes
	bool WritePersistentID(const Zenith_PersistentID& xID);
	bool WriteEntityReference(Zenith_EntityHandle xEntity);   // 16 bytes through the save resolver
	uint64 GetBytesWritten() const;
};

/// @contract type Zenith_SaveSectionReader
/// Ownership: Owned by the restore job; borrowed by a participant for one RestoreSection call over decoded section bytes.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when RestoreSection returns.
/// Units: Bytes.
/// Errors: Every Read member returns false, and every subsequent read returns false, once the section is exhausted; the participant
///   returns SAVE_ERROR_CORRUPT. Section bytes are untrusted.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveSectionReader
{
public:
	bool ReadU8(uint8& uOut);
	bool ReadU16(uint16& uOut);
	bool ReadU32(uint32& uOut);
	bool ReadU64(uint64& ulOut);
	bool ReadF32Bits(float& fOut);
	bool ReadF64Bits(double& dOut);
	bool ReadBytes(Zenith_MutableByteSpan xOut);
	bool ReadLengthPrefixed(uint32 uMaxBytes, Zenith_ByteSpan& xOut);   // a view into the section
	bool ReadPersistentID(Zenith_PersistentID& xOut);
	bool ReadEntityReference(Zenith_EntityHandle& xOut);   // the restored handle, or null (unresolved)
	uint64 GetRemainingBytes() const;
	uint16 GetSectionVersion() const;
};

/// @contract value Zenith_SaveCaptureContext
/// Ownership: Plain value built by the snapshotter for one CaptureSection call; borrows the world and the inspector.
/// Thread affinity: OWNER_THREAD. The phase-11 snapshot node's thread, or the main thread out of tick.
/// Invalidation: Invalid when CaptureSection returns.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveCaptureContext
{
	const Zenith_WorldInspector* m_pxInspector = nullptr;
	uint16 m_uWorldTag = 0;
	uint64 m_ulTickIndex = 0;                // the state after this tick
	Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	bool m_bOutOfTick = false;                // true for resume and paused-world saves
};

/// @contract value Zenith_SaveRestoreContext
/// Ownership: Plain value built by the restore job for one RestoreSection or IsRestoreReady call; m_pxJob borrows the job.
/// Thread affinity: MAIN_THREAD with no frame of the world running.
/// Invalidation: Invalid when RestoreSection or IsRestoreReady returns.
/// Units: Ticks.
/// Errors: None. ReportCoherentRestore is idempotent and never fails; a key that is not a registered participant's is
///   ZENITH_ASSERT (engine code).
/// Version: since Z2 Stage 3. ReportCoherentRestore since the SAVE post-signoff patch for rows PH22, AI14 and PhB12.
/// @endcontract
struct Zenith_SaveRestoreContext
{
	// Called by a participant from RestoreSection or IsRestoreReady (never from capture) when its capture was EXACT but its
	// restore is only COHERENT (PHYS: an EXACT blob that RestoreState refuses, or a body whose shape asset FAILED; AI: the
	// navigation content changed since the capture). ulParticipantKey is the caller's Zenith_SaveParticipantKeyOf(GetName()).
	// The job records the first reporting key, logs "save: restore of <name> is COHERENT only" once per participant, and from
	// then on reports GetRestoredFidelity() == SAVE_FIDELITY_COHERENT, so the Q70 (a) rules apply as for a COHERENT image:
	// a resumed replay runs without hash comparison and says so, and a seek through the restore falls back to FULL.
	void ReportCoherentRestore(uint64 ulParticipantKey) const;

	Zenith_WorldSimulation* m_pxWorld = nullptr;
	uint64 m_ulTickIndex = 0;
	Zenith_SaveVersion m_xFileVersion;
	bool m_bSectionPresent = false;           // false: the save has no section for this participant (an older save, or the
	                                          // participant was not registered when it was written); the participant resets
	bool m_bUnderPublicationSource = false;   // the restore replays recorded publications (a seek, a restored replay session)
	Zenith_SaveReissueSet* m_pxReissue = nullptr;   // RestoreSection only: where the participant marks each entry it re-issued
	Zenith_WorldRestoreJob* m_pxJob = nullptr;        // set by the job; read only by ReportCoherentRestore
};

/// @contract type Zenith_SaveExclusionList
/// Ownership: Owned by the snapshotter for one capture; arena-backed.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when the capture ends.
/// Units: Simulation IDs.
/// Errors: Add returns false when the list's capacity (the world's entity count) is exhausted, which cannot happen for
///   distinct live entities; a duplicate is ignored.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SaveExclusionList
{
public:
	bool Add(uint64 ulSimulationID);           // the entity and, through the hierarchy, its descendants
	uint32 GetCount() const;
};

/// @contract interface Zenith_SaveParticipant
/// Ownership: Implemented by a library or module above ZenithSave (the runtime bridge for script states and additive
///   scenes; the physics, animation, AI and cinematics modules for their state outside reflection); one instance per
///   (participant, world); owned by its implementer, which unregisters it before destroying it.
/// Thread affinity: OWNER_THREAD. DeclareAccess, GetReissuedSourceKinds on the main thread at world attach; CollectExcludedEntities,
///   CaptureSection and GetCaptureFidelity on the phase-11 snapshot node of its world (any worker) or on the main thread out
///   of tick, none of them changing simulation state; RestoreSection, IsRestoreReady and ListRestoreWaits on the main thread
///   out of tick, with no frame of the world running.
/// Invalidation: None.
/// Units: Section versions are the participant's own uint16 counter, starting at 1.
/// Errors: CaptureSection and RestoreSection return Zenith_SaveError (SAVE_ERROR_PARTICIPANT with m_ulDetail = the key, or
///   SAVE_ERROR_CORRUPT); a capture failure fails that save, a restore failure fails that restore and the world is destroyed.
/// Reentrancy: NONE. A participant never calls Zenith_SaveSystem, Zenith_WorldSnapshotter or Zenith_WorldRestoreJob; the
///   caller's guard is the world binding's (capture) or the restore job's (restore).
/// Version: since Z2 Stage 3. Source API; a new member is added with a default implementation.
/// @endcontract
class Zenith_SaveParticipant
{
public:
	virtual ~Zenith_SaveParticipant() = default;
	// The stable name; the participant key is Zenith_SaveParticipantKeyOf(GetName()). "Zenith.<Area>.<Topic>".
	virtual const char* GetName() const = 0;
	virtual uint16 GetSectionVersion() const = 0;
	virtual Zenith_SaveParticipantFlags GetFlags() const = 0;
	// Declares, on the phase-11 snapshot system, a read of every world resource CaptureSection reads.
	virtual void DeclareAccess(Zenith_SystemBuilder& xSnapshotSystem) = 0;
	// Called before the entity capture: lists the entities whose owner is not in a consistent state at the snapshot point
	// (a streamed cell or additive instance whose activation or unload is incomplete); they and their descendants are not
	// saved, and the participant's section records what it must re-request at restore. The default lists nothing.
	virtual void CollectExcludedEntities(const Zenith_SaveCaptureContext& xContext, Zenith_SaveExclusionList& xOut)
	{
		(void)xContext; (void)xOut;
	}
	virtual Zenith_Result<void, Zenith_SaveError> CaptureSection(const Zenith_SaveCaptureContext& xContext,
		Zenith_SaveSectionWriter& xWriter) = 0;
	// Called after CaptureSection succeeded for the same capture: whether a restore of what was just captured continues
	// bit-identically (EXACT) or only consistently (COHERENT). A pure read of the state the capture saw; the capture's
	// fidelity is the weakest any participant returns, and a COHERENT capture sets uSAVE_HEADER_FLAG_COHERENT_ONLY.
	virtual Zenith_SaveFidelity GetCaptureFidelity(const Zenith_SaveCaptureContext& xContext) const
	{
		(void)xContext;
		return (GetFlags() & uSAVE_PARTICIPANT_FLAG_COHERENT) != 0 ? SAVE_FIDELITY_COHERENT : SAVE_FIDELITY_EXACT;
	}
	// The asynchronous source kinds whose in-flight requests this participant re-issues at restore under their recorded
	// completion IDs (bit k = Zenith_AsyncSourceKind k, k below uASYNC_SOURCE_KIND_CAPACITY). Read once at RegisterParticipant:
	// a bit another participant of the world already declared, bit ASYNC_SOURCE_SAVE or a bit at or above the capacity is
	// refused with SAVE_ERROR_INVALID_DESC. The restore leaves every in-flight entry of a declared kind in flight; the
	// participant re-issues it in RestoreSection and calls xContext.m_pxReissue->MarkReissued. The default declares none.
	virtual uint32 GetReissuedSourceKinds() const { return 0; }
	// Called after every entity and every world resource of the world exists, in participant registration order; for an older section version the
	// participant migrates its own bytes forward (one step per version) before applying them.
	virtual Zenith_Result<void, Zenith_SaveError> RestoreSection(const Zenith_SaveRestoreContext& xContext,
		Zenith_SaveSectionReader& xReader) = 0;
	// Called by the restore job's Step in WORLD_RESTORE_STATE_PARTICIPANTS_READY, in registration order, after every
	// RestoreSection succeeded, until it returns true (it is not asked again afterwards): true once everything the restored
	// world's first tick reads is in place (ANIM: every recorded asset loaded and every instance bound). The world's first
	// tick runs only after every participant returned true. The default is ready at once.
	virtual bool IsRestoreReady(const Zenith_SaveRestoreContext& xContext)
	{
		(void)xContext;
		return true;
	}
	// The operations IsRestoreReady is waiting for (borrowed; valid until the next Step), so a synchronous restore driver can
	// service them outside a frame (SAVE/RuntimeBridge.md "Synchronous restore driver"); returns the count written to axOut.
	// A frame-driven restore ignores them: the frame's own servicing advances them. The default lists none.
	virtual uint32 ListRestoreWaits(Zenith_Span<const Zenith_OperationHandle*> axOut) const
	{
		(void)axOut;
		return 0;
	}
};

/// @contract interface Zenith_SaveWorldHooks
/// Ownership: Implemented by ZenithRuntime's save bridge, one per bound world, over the world's Zenith_AsyncResultTable
///   (ZenithScript, which ZenithSave cannot include); passed to Zenith_SaveSystem::BindWorld and outliving the binding.
/// Thread affinity: OWNER_THREAD. CaptureAsyncState on the world's phase-11 snapshot node or on the main thread out of tick;
///   AllocateSnapshotCompletionID on the phase-11 snapshot node only (an engine autosave of a due tick); RestoreAsyncState and
///   CompleteAtRestore on the main thread out of tick.
/// Invalidation: The span CaptureAsyncState returns is valid until the capture ends.
/// Units: Completion IDs.
/// Errors: SAVE_ERROR_OUT_OF_MEMORY from CaptureAsyncState; SAVE_ERROR_RESTORE from RestoreAsyncState (the table refused a
///   restored entry; among those, a save holding more ASYNC_SOURCE_SAVE entries than this build's declared capacity
///   uSaveMaxOutstanding, written under larger save.maxQueuedRequests or save.maxRequestsPerTick values, fails with m_ulDetail
///   = the saved SAVE-entry count, logged naming both keys; SAVE.md "Asynchronous source capacity").
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveWorldHooks
{
public:
	virtual ~Zenith_SaveWorldHooks() = default;
	virtual Zenith_Result<Zenith_Span<const struct Zenith_SaveAsyncState>, Zenith_SaveError> CaptureAsyncState(
		Zenith_Arena& xArena) = 0;       // unpublished results and in-flight requests, ascending completion ID
	virtual uint64 GetNextCompletionID() const = 0;
	// Applies the restore job's per-entry decision (Formats.md "Async section"): PUT_BACK_COMPLETED entries become completed
	// results with their saved payload, LEAVE_IN_FLIGHT entries in-flight requests, COMPLETE_AT_RESTORE entries completed results
	// with the entry's m_xRestorePayload; all publish from the first tick boundary of the restored world.
	virtual Zenith_Result<void, Zenith_SaveError> RestoreAsyncState(uint64 ulNextCompletionID,
		Zenith_Span<const struct Zenith_SaveAsyncRestoreEntry> axEntries) = 0;
	// Completes a left-in-flight entry that no participant re-issued (Zenith_SaveReissueSet), with xPayload, before the first tick.
	virtual void CompleteAtRestore(uint64 ulCompletionID, Zenith_ByteSpan xPayload) = 0;
	// The completion ID of an engine autosave allocated on the phase-11 node of its due tick; 0 when the table refused (a defect: the source's capacity bound of SAVE.md "Asynchronous source capacity" makes it
	// unreachable).
	virtual uint64 AllocateSnapshotCompletionID() = 0;
};

/// @contract functions Zenith_SaveParticipantKey
/// Ownership: Pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. The first 8 bytes of Zenith_Sha256Digest(name), little-endian; persisted, never changes.
/// @endcontract
uint64 Zenith_SaveParticipantKeyOf(Zenith_StringView strName);
```

## Zenith_WorldSnapshot header

The capture of one world into section bytes and the incremental restore of one world from them. The algorithms are `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Phase-11 snapshot", "Out-of-tick snapshot" and "Restore pipeline".

```cpp
// Zenith2/Engine/ZenithSave/Zenith_WorldSnapshot.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithReflect/Zenith_FieldSet.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithCore/Zenith_ReentrancyGuard.h"
#include "ZenithCore/Zenith_OperationHandle.h"      // Zenith_WorldRestoreJob::ListRestoreWaits
#include "ZenithSave/Zenith_SaveFormat.h"
#include "ZenithSave/Zenith_SaveParticipant.h"

class Zenith_TypeRegistry;
class Zenith_StringTable;
class Zenith_WorldSimulation;
class Zenith_WorldInspector;
class Zenith_MigrationDiagnostics;
class Zenith_SaveWorldBinding;       // ZenithSave/Internal: the save system's per-world state (opaque here)

/// @contract type Zenith_SaveReferenceResolver
/// Ownership: Owned by the snapshotter or the restore job for one capture or restore; borrows the type registry.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid when its owner finishes.
/// Units: References are 16 bytes: a persistent ID, or the save-local form (bytes 0–7 the simulation ID little-endian,
///   bytes 8–15 zero) of Formats.md "Saved entity references".
/// Errors: An entity reference to a stale handle, or to an entity pending destroy at the snapshot point, maps to all-zero
///   (null) and is not counted; at restore an unknown reference resolves to null and is counted
///   (save.restore.unresolved_references) and logged once per (type, property).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveReferenceResolver final : public Zenith_ReferenceResolver
{
public:
	// The record being captured or applied: the owner's simulation ID and the component, event or resource type, so a
	// deferred reference is recorded per (entity, type) and the unresolved-reference log names the type.
	void SetContext(uint64 ulSimulationID, Zenith_TypeID xType);
	// Restore only: true while SPAWNING (an entity reference is left null and its (entity, type) recorded when the stored
	// reference is not null), false in LINKING (full resolution through the restored simulation-ID map).
	void SetDeferEntityReferences(bool bDefer);
	bool EntityToPersistent(const void* pEntityBits, uint8* pPersistentOut) override;
	bool PersistentToEntity(const uint8* pPersistent, void* pEntityBitsOut) override;
	bool AssetToPersistent(const void* pAssetBytes, uint8* pPersistentOut) override;   // value-kind hook m_pfnToPersistentID
	bool PersistentToAsset(const uint8* pPersistent, Zenith_TypeID xAssetKind, void* pAssetBytesOut) override;
	Zenith_ByteSpan ContainerElements(const void* pContainerRef, uint32& uCountOut) override;
	bool StoreContainer(Zenith_ByteSpan xElements, uint32 uCount, void* pContainerRefOut) override;
	uint32 GetUnresolvedCount() const;
	bool HadDeferredReference() const;   // since the last SetContext
};

/// @contract value Zenith_WorldSnapshotDesc
/// Ownership: Plain value; every pointer borrowed for one capture.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Ticks; bytes.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_WorldSnapshotDesc
{
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;
	const Zenith_StringTable* const m_pxStrings = nullptr;
	const Zenith_WorldInspector* const m_pxInspector = nullptr; // the phase-11 inspector or GetInspector() out of tick
	const Zenith_Span<Zenith_SaveParticipant* const> m_apxParticipants = {}; // registration order
	const struct Zenith_SaveUnknownCarry* const m_pxCarry = nullptr;
	const uint16 m_uWorldIndex = 0;
	const uint16 m_uWorldTag = 0;
	const uint64 m_ulTickIndex = 0;
	const uint64 m_ulNextSimulationID = 0;
	const uint64 m_ulWorldSeed = 0;
	const Zenith_PersistentID m_xBaseScene = {};
	const uint64 m_ulBaseSceneContentHash = 0;
	const uint64 m_ulNextCompletionID = 0;   // written to ASYNC only (Formats.md "Async section")
	const uint64 m_ulPlaytimeTicks = 0;      // the world's playtime after tick m_ulTickIndex (WORLD offset 32)
	const uint32 m_uNextRequestSerial = 1;   // WORLD offset 80
	const uint16 m_uWorldFlags = 0;          // WORLD offset 2: primary, paused, autosave disabled
	const uint64 m_ulOutstandingAutosaveID = 0; // WORLD offset 88
	const Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	const bool m_bOutOfTick = false;
	const uint64 m_ulBudgetBytes = 0;        // the image arena space left for this world's decoded sections
};

/// @contract value Zenith_CapturedSection
/// Ownership: Plain value; m_xDecoded borrows the capture arena.
/// Thread affinity: IMMUTABLE. Members non-const: participants fill it through the span the snapshotter passes.
/// Invalidation: m_xDecoded is invalid when the capture arena is reset.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CapturedSection
{
	Zenith_SaveSectionKind m_eKind;
	uint16 m_uSectionVersion;
	uint16 m_uFlags;
	uint64 m_ulKey;
	Zenith_ByteSpan m_xDecoded;
};

/// @contract type Zenith_WorldSnapshotter
/// Ownership: A value owned by the caller for one capture; all output in the caller's arena.
/// Thread affinity: OWNER_THREAD. The phase-11 snapshot node's thread, or the main thread out of tick.
/// Invalidation: Section spans are valid until the arena is reset.
/// Units: Bytes.
/// Errors: SAVE_ERROR_SERIALIZE (a Zenith_CaptureFieldSet error, m_ulDetail the type ID), SAVE_ERROR_PARTICIPANT,
///   SAVE_ERROR_IMAGE_TOO_LARGE, SAVE_ERROR_OUT_OF_MEMORY; a failed capture leaves the world untouched.
/// Reentrancy: NONE. It invokes each participant's CollectExcludedEntities, CaptureSection and GetCaptureFidelity, none of
///   which may call the snapshotter (or the save system); its Zenith_ReentrancyGuard checks it.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_WorldSnapshotter
{
public:
	// Captures WORLD, ENTITIES, EVENTS, ASYNC, RESOURCES, UNKNOWN_CARRY (when non-empty) and one PARTICIPANT section per
	// participant, in that order, into axOut; the capture's fidelity is the weakest any participant reported.
	Zenith_Result<void, Zenith_SaveError> Capture(const Zenith_WorldSnapshotDesc& xDesc,
		Zenith_Span<const struct Zenith_SaveAsyncState> axAsyncState, Zenith_Arena& xArena,
		Zenith_Span<Zenith_CapturedSection>& axOut);
	uint32 GetEntityCount() const;     // of the last capture
	uint32 GetEventCount() const;
	uint32 GetResourceCount() const;
	Zenith_SaveFidelity GetFidelity() const;
	uint64 GetCoherentParticipantKey() const;   // the first participant that reported COHERENT, or 0
private:
	Zenith_ReentrancyGuard m_xReentrancy;
};

/// @contract value Zenith_SaveAsyncState
/// Ownership: Plain value; m_xPayload borrows the provider's memory for the capture.
/// Thread affinity: IMMUTABLE.
/// Invalidation: Valid for one capture.
/// Units: Completion IDs.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveAsyncState
{
	const uint64 m_ulCompletionID = 0;
	const uint8 m_uSourceKind = 0;           // Zenith_AsyncSourceKind
	const uint8 m_uPublicationKind = 0;      // Zenith_PublicationKind; 0xFF for an in-flight (not yet completed) entry
	const uint16 m_uReserved = 0;
	const uint64 m_ulOwnerSimulationID = 0;
	const uint32 m_uOwnerModule = 0;
	const Zenith_ByteSpan m_xPayload = {};    // empty for in-flight entries; for an unpublished result, its payload verbatim
};

/// @contract enum Zenith_SaveAsyncDisposition
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Never serialized: the restore job derives it from the saved entry, the participants'
///   GetReissuedSourceKinds and the desc's m_bUnderPublicationSource (Formats.md "Async section").
/// @endcontract
enum Zenith_SaveAsyncDisposition : uint8
{
	SAVE_ASYNC_DISPOSITION_PUT_BACK_COMPLETED = 1,   // an unpublished result restored completed with its saved payload
	SAVE_ASYNC_DISPOSITION_LEAVE_IN_FLIGHT = 2,      // a participant re-issues it, or the publication source supplies it
	SAVE_ASYNC_DISPOSITION_COMPLETE_AT_RESTORE = 3,  // restored completed with m_xRestorePayload, which SAVE builds
};

/// @contract value Zenith_SaveAsyncRestoreEntry
/// Ownership: Plain value built by the restore job; m_xRestorePayload borrows the job's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: Valid for one RestoreAsyncState call.
/// Units: Completion IDs; bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveAsyncRestoreEntry
{
	const Zenith_SaveAsyncState m_xSaved = {}; // the entry as the ASYNC section holds it
	const Zenith_SaveAsyncDisposition m_eDisposition = SAVE_ASYNC_DISPOSITION_PUT_BACK_COMPLETED;
	const uint8 m_uRestorePublicationKind = 0; // COMPLETE_AT_RESTORE: always PUBLICATION_KIND_GENERIC_RECORDED
	const Zenith_ByteSpan m_xRestorePayload = {}; // COMPLETE_AT_RESTORE: SAVE's own success or CANCELLED payload, or zero length
};

/// @contract value Zenith_SaveUnknownComponent
/// Ownership: Plain value; m_xRecord borrows the world binding's carry arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the carry (replaced by each load, cleared when the world is destroyed).
/// Units: Encoded binary field-set record bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveUnknownComponent
{
	const uint64 m_ulSimulationID = 0;
	const Zenith_PersistentID m_xEntity = {};
	const Zenith_ByteSpan m_xRecord = {};    // one binary field-set record of a type the registry lacks
};

/// @contract value Zenith_SaveUnknownResource
/// Ownership: Plain value; m_xRecord borrows the world binding's carry arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the carry.
/// Units: Encoded binary field-set record bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveUnknownResource
{
	const uint64 m_ulTypeID = 0;            // a resource type the running world does not have
	const Zenith_ByteSpan m_xRecord = {};
};

/// @contract value Zenith_SaveUnknownSection
/// Ownership: Plain value; m_xDecoded borrows the world binding's carry arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the carry.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveUnknownSection
{
	const Zenith_SaveSectionEntry m_xEntry = {}; // as read, with m_xCompression replaced by NONE
	const Zenith_ByteSpan m_xDecoded = {};
};

/// @contract value Zenith_SaveUnknownCarry
/// Ownership: Owned per world by the save system's world binding; arena-backed; replaced by each load.
/// Thread affinity: OWNER_THREAD. Written by the restore job on the main thread out of tick, read by the snapshotter of the
///   same world (its phase-11 node or the main thread), which the frame structure orders after the restore.
/// Invalidation: Cleared when the world is destroyed.
/// Units: Encoded binary field-set records and raw section bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveUnknownCarry
{
	Zenith_Span<const Zenith_SaveUnknownComponent> m_axComponents;   // ascending (simulation ID, type ID)
	Zenith_Span<const Zenith_SaveUnknownResource> m_axResources;     // ascending type ID
	Zenith_Span<const Zenith_SaveUnknownSection> m_axSections;
};

/// @contract value Zenith_WorldRestoreDesc
/// Ownership: Plain value; pointers borrowed for the job's lifetime.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Microseconds; entities.
/// Errors: None of its own.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_WorldRestoreDesc
{
	Zenith_WorldSimulation* const m_pxWorld = nullptr;    // freshly created, empty, isolated, with the saved world seed
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;
	Zenith_StringTable* const m_pxStrings = nullptr;
	// The Zenith_MigrationDiagnostics every per-record Zenith_MigrationContext of the restore points at (m_pxDiagnostics), the
	// one parameter Zenith_MigrateFieldSet takes for it: it carries the arena, the string table, the save-loading script VM
	// factory and the warning sink (Zenith2/Docs/Design/ECS/Schemas.md → "Schema migration").
	Zenith_MigrationDiagnostics* const m_pxMigration = nullptr;
	const Zenith_Span<Zenith_SaveParticipant* const> m_apxParticipants = {};
	Zenith_SaveWorldBinding* const m_pxBinding = nullptr;  // the bound world's binding: DONE writes its restored counters
	const uint32 m_uEntitiesPerBatch = 1024;
	const uint64 m_ulBudgetUs = 4000;                    // save.restoreBudgetUs per Step call
	const bool m_bUnderPublicationSource = false;         // a seek or a restored replay session (Formats.md "Async section")
};

/// @contract enum Zenith_WorldRestoreState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; FAILED is the state after any Step error.
/// Version: since Z2 Stage 3. Never serialized.
/// @endcontract
enum Zenith_WorldRestoreState : uint8
{
	WORLD_RESTORE_STATE_DECODING = 1,     // world header, entity table and async state decoded and migrated
	WORLD_RESTORE_STATE_SPAWNING = 2,     // entity batches applied out of tick
	WORLD_RESTORE_STATE_LINKING = 3,      // parents and entity references (including those inside containers) applied
	WORLD_RESTORE_STATE_EVENTS = 4,       // carried events appended
	WORLD_RESTORE_STATE_RESOURCES = 5,    // reflected world resources written out of tick
	WORLD_RESTORE_STATE_PARTICIPANTS = 6, // asynchronous state, then each participant's RestoreSection, then unmarked re-issues abandoned
	WORLD_RESTORE_STATE_PARTICIPANTS_READY = 7,   // each participant's IsRestoreReady polled in registration order until all are true
	WORLD_RESTORE_STATE_DONE = 8,
	WORLD_RESTORE_STATE_FAILED = 9,
};

/// @contract type Zenith_WorldRestoreJob
/// Ownership: Created by Zenith_PreparedSave::BeginWorldRestore as a Zenith_Owned; owned by the caller (the world host);
///   borrows the prepared save, which outlives it.
/// Thread affinity: MAIN_THREAD. Step runs in the frame prologue (step 7), at startup step 17, or in a synchronous restore
///   driver (a seek, a replay's restored session; SAVE/RuntimeBridge.md "Synchronous restore driver"), with no frame of the
///   world running.
/// Invalidation: None.
/// Units: Microseconds.
/// Errors: Step returns SAVE_ERROR_RESTORE (an ApplyOutOfTick error, m_ulDetail the ECS error code; or RestoreAsyncState's
///   capacity refusal, m_ulDetail the saved ASYNC_SOURCE_SAVE entry count), SAVE_ERROR_MIGRATION,
///   SAVE_ERROR_CORRUPT, SAVE_ERROR_PARTICIPANT, SAVE_ERROR_REQUIRED_PARTICIPANT_MISSING; after an error the state is FAILED
///   and the caller destroys the world. Step inside a frame of the world is ZENITH_ASSERT.
/// Reentrancy: NONE. It invokes each participant's RestoreSection, IsRestoreReady and ListRestoreWaits and the hooks'
///   RestoreAsyncState and CompleteAtRestore, none of which may call the job; its own Zenith_ReentrancyGuard checks it (one
///   job is stepped by one thread, the main thread).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_WorldRestoreJob
{
public:
	Zenith_Result<Zenith_WorldRestoreState, Zenith_SaveError> Step();   // advances within m_ulBudgetUs
	Zenith_WorldRestoreState GetState() const;
	float GetProgress() const;           // entities applied / entity count, then 1.0 at DONE
	uint32 GetUnresolvedReferenceCount() const;
	uint32 GetUnknownComponentCount() const;
	uint32 GetUnknownResourceCount() const;
	// In PARTICIPANTS_READY: the waits of every participant not yet ready (ListRestoreWaits), for a synchronous driver; 0 in
	// any other state.
	uint32 ListRestoreWaits(Zenith_Span<const Zenith_OperationHandle*> axOut) const;
	// In PARTICIPANTS_READY: the key of the first participant that is not yet ready (logged by a driver that times out); 0 otherwise.
	uint64 GetFirstUnreadyParticipantKey() const;
	// The fidelity of the restored world: the image's (COHERENT when the header carries COHERENT_ONLY) weakened to COHERENT by
	// any participant's Zenith_SaveRestoreContext::ReportCoherentRestore; final once the job is DONE. The resume and seek
	// paths read this, never Zenith_PreparedSave::GetFidelity alone.
	Zenith_SaveFidelity GetRestoredFidelity() const;
	uint64 GetCoherentReporterKey() const;   // the first reporting participant's key; 0 when none reported
private:
	friend struct Zenith_SaveRestoreContext;   // ReportCoherentRestore records the report in the job
	Zenith_ReentrancyGuard m_xReentrancy;
};
```

## Zenith_SaveMigration header

Forward migration of a save image from any readable `(major, minor)` to the current version. Component data migrates per type through ECS's `Zenith_MigrateFieldSet` (`Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Migration"); behaviour states through SCRIPT's `Migrate` chains inside `RestoreSavedStates`; participant sections in each participant's `RestoreSection`. This header carries the format-level chain alone.

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveMigration.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithSave/Zenith_SaveFormat.h"

class Zenith_Arena;

/// @contract type Zenith_SaveImageTree
/// Ownership: The decoded, editable form of a save image: every section's decoded bytes in an arena, with its entry; owned
///   by the load operation.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Spans invalid when the arena is reset.
/// Units: Bytes.
/// Errors: ReplaceSection returns SAVE_ERROR_OUT_OF_MEMORY; AddSection SAVE_ERROR_SECTION_LIMIT.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveImageTree
{
public:
	Zenith_SaveVersion GetVersion() const;
	void SetVersion(Zenith_SaveVersion xVersion);               // a step sets its target version
	uint32 GetSectionCount() const;
	const Zenith_SaveSectionEntry& GetEntry(uint32 uIndex) const;
	Zenith_ByteSpan GetDecoded(uint32 uIndex) const;
	Zenith_Result<void, Zenith_SaveError> ReplaceSection(uint32 uIndex, uint16 uNewSectionVersion,
		Zenith_ByteSpan xNewDecoded);
	Zenith_Result<void, Zenith_SaveError> AddSection(const Zenith_SaveSectionEntry& xEntry, Zenith_ByteSpan xDecoded);
	void RemoveSection(uint32 uIndex);
	Zenith_Arena& GetArena();
};

// One step migrates (major, m) to (major, m + 1). Pure: no I/O, no engine state, deterministic ([Z2-N-CONTRACT-538]).
using Zenith_PfnSaveFormatMigrationStep = Zenith_Result<void, Zenith_SaveError> (*)(Zenith_SaveImageTree& xImage);

/// @contract type Zenith_SaveMigrationRegistry
/// Ownership: A value owned by the save system; filled once at Create from ZenithSave's own step table, then read-only.
/// Thread affinity: ANY_THREAD_EXCEPT(Register, Freeze). Register and Freeze on the main thread before any load.
/// Invalidation: None.
/// Units: Minor versions.
/// Errors: Register returns SAVE_ERROR_INVALID_DESC for a duplicate or a from-minor at or above the current minor; Freeze
///   returns SAVE_ERROR_MIGRATION naming the first missing minor ([Z2-N-CONTRACT-539]).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveMigrationRegistry
{
public:
	Zenith_Result<void, Zenith_SaveError> Register(uint16 uFromMinor, Zenith_PfnSaveFormatMigrationStep pfnStep);
	Zenith_Result<void, Zenith_SaveError> Freeze();   // one step for every minor below the current one
	uint32 GetStepCount() const;
private:
	Zenith_PfnSaveFormatMigrationStep m_apfnSteps[64] = {};
	bool m_bFrozen = false;
};
inline constexpr uint32 uSAVE_MAX_FORMAT_MINOR = 63;

/// @contract functions Zenith_SaveMigrationFunctions
/// Ownership: Mutates the caller's tree.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: SAVE_ERROR_VERSION_NEWER, SAVE_ERROR_VERSION_MAJOR, SAVE_ERROR_MIGRATION (a step's error); on any error the tree is
///   left at the version of the last completed step and the load fails; the file on disk is never touched.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_SaveError> Zenith_MigrateSaveImage(const Zenith_SaveMigrationRegistry& xRegistry,
	Zenith_SaveImageTree& xImage);
void Zenith_RegisterSaveFormatMigrations(Zenith_SaveMigrationRegistry& xRegistry);   // ZenithSave's own steps
```

## Zenith_SaveSystem header

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_OperationHandle.h"
#include "ZenithCore/Zenith_ReentrancyGuard.h"
#include "ZenithPlatform/Zenith_AppLifecycle.h"          // Zenith_ResumeIdentity
#include "ZenithSave/Zenith_SaveTypes.h"
#include "ZenithSave/Zenith_SaveFormat.h"
#include "ZenithSave/Zenith_SaveStorage.h"
#include "ZenithSave/Zenith_SaveJournal.h"               // Zenith_SaveRecoveryReport
#include "ZenithSave/Zenith_SaveParticipant.h"
#include "ZenithSave/Zenith_WorldSnapshot.h"
#include "ZenithSave/Zenith_ReplayLog.h"

class Zenith_Heap;
class Zenith_TypeRegistry;
class Zenith_StringTable;
class Zenith_CompressionCodecSet;
class Zenith_MemoryGovernor;
class Zenith_MemoryConsumer;          // ZenithCore (Zenith2/Docs/Architecture.md → "Memory governor")
class Zenith_FaultInjector;
class Zenith_ConfigStore;
class Zenith_SystemRegistry;          // ZenithECS (Zenith2/Docs/Design/ECS/SystemAPI.md)
class Zenith_WorldSimulation;
class Zenith_WorldInspector;
class Zenith_MigrationDiagnostics;
class Zenith_SaveCheckpointHolder;    // Internal: one world's retained and pending checkpoint, outliving its binding while leased

/// @contract interface Zenith_SaveCompletionSink
/// Ownership: Implemented by ZenithRuntime's save bridge (it allocates and completes ASYNC_SOURCE_SAVE results in the world's
///   Zenith_AsyncResultTable); borrowed by the save system, outlives it.
/// Thread affinity: ANY_THREAD. AllocateCompletionID on the world's sync-point playback (the merge chain); Complete and
///   CompleteLinkedLoadRefused on the save writer thread, on the world's merge chain for requests completed at intake, and on
///   the world's phase-11 snapshot node (an autosave refused QUEUE_FULL, a capture failure, the tick's limit batch, which
///   AllocateCompletionID allocated at the sync point of its first refused request). Calls for
///   one world never overlap (frame-ordered); calls for different worlds and from the writer thread may run concurrently,
///   each completing into its own world's table.
/// Invalidation: xPayload is valid for the call.
/// Units: Completion IDs; the payload layout is Formats.md "Publication payloads".
/// Errors: None; the sink logs a table refusal. In replay mode intake refusals still call Complete: SCRIPT discards a live
///   completion of an ID whose recorded publication it supplies (Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result
///   table", publication row), so the recorded payload is the only one published.
/// Reentrancy: NONE. The sink never calls the save system; the caller enters the guard of the thread's context (the world
///   binding's on world nodes, the writer guard on Zenith.SaveWriter).
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_SaveCompletionSink
{
public:
	virtual ~Zenith_SaveCompletionSink() = default;
	// Allocates the completion ID of a C++ save request of world uWorldTag at sync-point playback, in merge order (also the
	// one ID of the tick's limit batch, at the sync point of its first refused request). Script
	// requests arrive with the ID the bridge's SAVE_REQUEST consumer allocated on the committing merge node (SCRIPT's commit
	// hands the consumer 0); engine autosaves allocate through Zenith_SaveWorldHooks.
	virtual uint64 AllocateCompletionID(uint16 uWorldTag) = 0;   // 0 when the table refused
	virtual void Complete(uint16 uWorldTag, uint64 ulCompletionID, Zenith_ByteSpan xPayload) = 0;
	// Stage 4: completes the linked ASYNC_SOURCE_WORLD_LOAD ID of a script Save.Load that SAVE refused or cancelled before the
	// load ran, with GAME's 8-byte failure payload carrying WORLD_TRANSITION_FAILURE_PREPARE (Zenith2/Docs/Design/GAME/WorldHost.md
	// → "Publication payloads"), so the world's single world-load slot is released (Algorithms.md "Script loads and the world host").
	virtual void CompleteLinkedLoadRefused(uint16 uWorldTag, uint64 ulLinkedCompletionID) = 0;
};

/// @contract value Zenith_SaveSystemDesc
/// Ownership: Plain value; every pointer borrowed and outliving the save system.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes, ticks, milliseconds and microseconds as named.
/// Errors: Create returns SAVE_ERROR_INVALID_DESC for a null required pointer or a value outside its key's range.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveSystemDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_SaveStorage* const m_pxStorage = nullptr;
	const Zenith_CompressionCodecSet* const m_pxCodecs = nullptr;
	const Zenith_TypeRegistry* const m_pxTypes = nullptr;    // frozen
	Zenith_StringTable* const m_pxStrings = nullptr;
	Zenith_MigrationDiagnostics* const m_pxMigration = nullptr; // the save-loading migration diagnostics (VM factory, warning sink)
	Zenith_SaveCompletionSink* const m_pxCompletions = nullptr;
	Zenith_MemoryGovernor* const m_pxGovernor = nullptr;     // the consumer is registered on it; may be null in unit tests
	Zenith_FaultInjector* const m_pxFaultInjector = nullptr; // tests
	const Zenith_BuildID m_xBuildID = {};                    // Zenith_GetRunningBuildID(), read once by the bridge
	const uint64 m_ulCompositionHash = 0;
	const uint64 m_ulContentHash = 0;
	const uint16 m_uReleaseMajor = 0;
	const uint16 m_uReleaseMinor = 0;
	const uint32 m_uTickHz = 60;                            // sim.tickHz, for playtime
	const Zenith_CompressionCodecID m_eCodec = COMPRESSION_CODEC_ID_ZSTD; // save.compressionCodec
	const sint32 m_iCompressionLevel = 3;                   // save.compressionLevel (clamped on Android)
	const uint64 m_ulImageArenaBytes = 67108864;            // save.imageArenaBytes (Z2-B-SAVE-001)
	const uint32 m_uMaxQueuedRequests = 8;                  // save.maxQueuedRequests
	const uint32 m_uMaxRequestsPerTick = 4;                 // save.maxRequestsPerTick (the value of Z2-B-SCRIPT-025)
	const uint64 m_ulAutosaveIntervalTicks = 0;             // save.autosaveIntervalTicks; 0 = engine autosave off
	const uint32 m_uAutosaveSlotCount = 3;                  // save.autosaveSlotCount
	const uint32 m_uShutdownDrainMs = 5000;                 // save.shutdownDrainMs
	const uint64 m_ulRestoreBudgetUs = 4000;                // save.restoreBudgetUs
	// --replay: captures run, but no write, delete, listing or load touches storage, the writer completes nothing (the
	// recorded publications are the results), and the resume, exit-autosave and profile calls are refused
	// (Algorithms.md "Replay mode"); Create then runs no journal recovery and builds no slot table.
	const bool m_bReplayMode = false;
	const Zenith_SaveProfileKey m_xInitialProfile = {};      // Zenith_ReadLastSaveProfile(storage), else "local"; always
	                                                         // "local" under --replay (the last-profile record is not read)
};

/// @contract enum Zenith_SaveOperation
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a payload byte outside the enumerators is a binding defect, refused at intake (SAVE_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 3. Serialized as byte 0 of every ASYNC_SOURCE_SAVE publication; values never renumbered.
/// @endcontract
enum Zenith_SaveOperation : uint8
{
	SAVE_OPERATION_WRITE = 1,
	SAVE_OPERATION_LIST = 2,
	SAVE_OPERATION_DELETE = 3,
	SAVE_OPERATION_LOAD = 4,
	SAVE_OPERATION_REFUSED_BATCH = 5,   // the tick's C++ requests over save.maxRequestsPerTick, refused together under one ID
};

/// @contract value Zenith_SaveRequest
/// Ownership: Plain value; m_strDisplayName's and m_axMetadata's views are copied at intake (at most 12 KiB per request).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; intake validates.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveRequest
{
	const Zenith_SaveOperation m_eOperation = SAVE_OPERATION_WRITE;
	const Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	const Zenith_SaveSlotName m_xSlot = {};   // MANUAL: the player's name; ignored for QUICK, CHECKPOINT, AUTOSAVE
	const Zenith_StringView m_strDisplayName = {};
	const Zenith_Span<const Zenith_SaveMetadataPair> m_axMetadata = {}; // at most uSAVE_MAX_GAME_METADATA_PAIRS (29), no key beginning
	                                                          // "zenith." (SAVE_ERROR_INVALID_DESC); merged over the world's
	                                                          // per-tick list at intake (Algorithms.md "Request intake")
	const bool m_bOverwriteNewer = false;
	const uint64 m_ulCompletionID = 0;       // a script request: the ASYNC_SOURCE_SAVE ID the bridge's consumer allocated on the
	                                          // committing merge node (operations 1-4); 0 otherwise
	const uint64 m_ulLinkedCompletionID = 0; // a script LOAD: the requesting world's ASYNC_SOURCE_WORLD_LOAD ID (Stage 4)
};

/// @contract value Zenith_SaveResult
/// Ownership: Plain value; the decoded form of an ASYNC_SOURCE_SAVE publication payload header.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes; ticks.
/// Errors: m_uCode 0 on success.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveResult
{
	const Zenith_SaveOperation m_eOperation = {};
	const Zenith_SaveKind m_eKind = {};
	const uint8 m_uCode = 0;                 // 0 or a Zenith_SaveErrorCode
	const uint8 m_uFromBackup = 0;           // LOAD: 1 when the .bak was used
	const uint32 m_uRequestSerial = 0;       // per world, assigned at intake in merge order
	const uint64 m_ulBytes = 0;              // bytes written or read
	const uint64 m_ulSnapshotTick = 0;
	const Zenith_SaveSlotName m_xSlot = {};
};
Zenith_Result<Zenith_SaveResult, Zenith_SaveError> Zenith_DecodeSaveResult(Zenith_ByteSpan xPayload);
	// SAVE_ERROR_CORRUPT for a non-empty payload shorter than 64 bytes; a zero-length payload decodes as an operation that
	// completed with SAVE_ERROR_CANCELLED (abandoned at a restore, Formats.md "Async section")

/// @contract enum Zenith_SaveSlotState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the state is the slot's condition.
/// Version: since Z2 Stage 3. Serialized as one byte of the LIST publication; values never renumbered.
/// @endcontract
enum Zenith_SaveSlotState : uint8
{
	SAVE_SLOT_STATE_VALID = 1,                  // header, table and metadata verified
	SAVE_SLOT_STATE_RECOVERABLE_FROM_BACKUP,    // the save fails validation and its .bak passes
	SAVE_SLOT_STATE_CORRUPT,                    // neither passes
	SAVE_SLOT_STATE_NEWER_VERSION,              // written by a newer format version: listed, never loaded or overwritten implicitly
	SAVE_SLOT_STATE_INCOMPATIBLE,               // another major version
};

/// @contract value Zenith_SaveSlotInfo
/// Ownership: Plain value; text views borrow the listing's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the arena.
/// Units: Seconds; bytes.
/// Errors: m_eState carries the slot's condition.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveSlotInfo
{
	const Zenith_SaveSlotName m_xSlot = {};
	const Zenith_SaveKind m_eKind = {};
	const Zenith_SaveSlotState m_eState = {};
	const Zenith_SaveVersion m_xVersion = {};
	const uint64 m_ulSequence = 0;
	const uint64 m_ulUtcSeconds = 0;
	const uint64 m_ulPlaytimeSeconds = 0;
	const uint64 m_ulFileBytes = 0;
	const Zenith_StringView m_strDisplayName = {};
	const bool m_bHasThumbnail = false;
	const bool m_bCoherentOnly = false;       // the save carries uSAVE_HEADER_FLAG_COHERENT_ONLY
};

// elaboration (A3 rows ON1 (c), ON16 (b)): Algorithms.md "Slot listener".
/// @contract interface Zenith_SaveSlotListener
/// Ownership: Implemented above SAVE (ONLINE's cloud engine); borrowed from SetSlotListener until it is cleared.
/// Thread affinity: MAIN_THREAD, called from PumpMainThread alone, in the order the writer completed the work.
/// Invalidation: xInfo's display-name view and xSlot are valid for the call.
/// Units: As Zenith_SaveSlotInfo.
/// Errors: None.
/// Reentrancy: NONE. A listener never calls the save system; the call holds the save system's m_xReentrancy guard.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_SaveSlotListener
{
public:
	virtual ~Zenith_SaveSlotListener() = default;
	// Once per successful write into a slot of the active profile (never the resume write); m_eState is VALID.
	virtual void OnSlotWritten(const Zenith_SaveSlotInfo& xInfo) = 0;
	// Once per slot a delete removed (SUCCEEDED, and some file of the slot existed), and for nothing else.
	virtual void OnSlotDeleted(const Zenith_SaveSlotName& xSlot, Zenith_SaveKind eKind) = 0;
};

// elaboration (A3 rows ON1 (a), ON16 (d)): Algorithms.md "Slot export".
/// @contract value Zenith_SaveExportedSlot
/// Ownership: Plain value filled by Zenith_SaveSystem::TakeExportedSlot; m_strDisplayName borrows the caller's span.
/// Thread affinity: IMMUTABLE.
/// Invalidation: m_strDisplayName with the span passed to TakeExportedSlot.
/// Units: Seconds; bytes.
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_SaveExportedSlot
{
	const Zenith_SaveSlotName m_xSlot = {};   // the METADATA slot name
	const Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	const Zenith_SaveVersion m_xVersion = {};
	const bool m_bFromBackup = false, m_bCoherentOnly = false; // FromBackup: the current file failed; these are the .bak's bytes
	const uint64 m_ulSequence = 0, m_ulUtcSeconds = 0, m_ulPlaytimeSeconds = 0;
	const uint64 m_ulFileBytes = 0;          // the bytes copied into the caller's span
	const Zenith_StringView m_strDisplayName = {}; // the METADATA display name, inside the copied bytes
};

/// @contract type Zenith_PreparedSave
/// Ownership: Created by Zenith_SaveSystem::TakePreparedLoad, PrepareFromImage or the writer's retained-checkpoint build;
///   owned by its caller (the world host, the resume path, the replay bridge) or by the save system's retained-checkpoint
///   slot; owns the decoded, migrated image arena (charged to Z2-B-SAVE-001).
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Restore jobs borrow it; it outlives them (a retained checkpoint is kept alive by its lease).
/// Units: None.
/// Errors: BeginWorldRestore returns SAVE_ERROR_INVALID_DESC for a world index at or above GetWorldCount, a non-empty world
///   or a desc without the world's binding.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_PreparedSave
{
public:
	const Zenith_SaveFileHeader& GetHeader() const;
	uint32 GetWorldCount() const;
	uint64 GetWorldSeed(uint32 uWorldIndex) const;
	uint64 GetWorldTick(uint32 uWorldIndex) const;
	Zenith_PersistentID GetBaseScene(uint32 uWorldIndex) const;
	Zenith_SaveFidelity GetFidelity() const;    // COHERENT when the header carries COHERENT_ONLY
	bool WasLoadedFromBackup() const;
	// The desc comes from Zenith_SaveSystem::MakeRestoreDesc of the bound world, so the world's participants and binding
	// are the ones attach registered.
	Zenith_Result<Zenith_Owned<Zenith_WorldRestoreJob>, Zenith_SaveError> BeginWorldRestore(uint32 uWorldIndex,
		const Zenith_WorldRestoreDesc& xDesc);
};

/// @contract type Zenith_RetainedCheckpointLease
/// Ownership: A move-only value returned by Zenith_SaveSystem::AcquireRetainedCheckpoint; it pins the world's checkpoint
///   holder (owned by the save system, not by the world binding), so while it lives the checkpoint is neither replaced nor
///   destroyed: a newer one waits in the pending slot, SetActiveProfile is refused with SAVE_ERROR_PROFILE_BUSY, and
///   UnbindWorld frees the binding but leaves the holder alive until the last lease ends (SAVE.md "Cross-thread hand-offs").
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Get is invalid after the lease is destroyed or moved from; the lease must not outlive the save system
///   (~Zenith_SaveSystem ZENITH_CHECKs that no lease is alive).
/// Units: None.
/// Errors: None; an empty lease (IsValid false) means the world has no retained checkpoint.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_RetainedCheckpointLease
{
public:
	Zenith_RetainedCheckpointLease(Zenith_RetainedCheckpointLease&& xOther);
	Zenith_RetainedCheckpointLease& operator=(Zenith_RetainedCheckpointLease&& xOther);
	Zenith_RetainedCheckpointLease(const Zenith_RetainedCheckpointLease&) = delete;
	~Zenith_RetainedCheckpointLease();                 // releases the lease
	bool IsValid() const;
	Zenith_PreparedSave& Get();          // ZENITH_ASSERT when !IsValid()
private:
	Zenith_SaveCheckpointHolder* m_pxHolder = nullptr;   // Internal; its lease count is main-thread only
};

/// @contract type Zenith_SaveScriptLoad
/// Ownership: A move-only value filled by Zenith_SaveSystem::TakeFinishedScriptLoad; owns the prepared save of a successful
///   load until it is moved to the world host.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Completion IDs.
/// Errors: m_xError is meaningful when m_bSucceeded is false.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_SaveScriptLoad
{
	uint64 m_ulSaveCompletionID = 0;         // ASYNC_SOURCE_SAVE: publishes Zenith.SaveCompleted (Operation 4)
	uint64 m_ulWorldLoadCompletionID = 0;    // ASYNC_SOURCE_WORLD_LOAD of the requesting world: GAME's transition
	bool m_bSucceeded = false;
	Zenith_SaveError m_xError{ SAVE_ERROR_INVALID_DESC };
	Zenith_Owned<Zenith_PreparedSave> m_pxSave;   // null unless m_bSucceeded
};

/// @contract value Zenith_SaveSystemStats
/// Ownership: Plain value returned by GetStats.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts and bytes since Create, except the gauges marked current.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SaveSystemStats
{
	uint64 m_ulWritesCompleted = 0, m_ulWritesFailed = 0, m_ulWritesCancelled = 0, m_ulWritesSuperseded = 0;
	uint64 m_ulLoadsCompleted = 0, m_ulLoadsFromBackup = 0, m_ulLoadsFailed = 0;
	uint64 m_ulBytesWritten = 0, m_ulBytesRead = 0;
	uint64 m_ulRequestsRefused = 0;
	uint32 m_uQueueDepth = 0;                // current
	uint32 m_uOutstandingHighWater = 0;
	uint64 m_ulImageArenaHighWater = 0;
	uint64 m_ulLastSnapshotNs = 0;           // the latest snapshot's serialization time (Z2-B-SAVE-002 evidence)
	uint64 m_ulLastWriteNs = 0;              // the latest write's compress + journal time
	uint64 m_ulCoherentCaptures = 0;
};

/// @contract type Zenith_SaveSystem
/// Ownership: Created by Create at startup step 11 (after the codec set) and owned by Zenith_Runtime; owns the save writer
///   thread Zenith.SaveWriter, the three capture arenas, the assembly and load arenas (each growing in 1 MiB blocks from
///   xDesc.m_pxHeap up to save.imageArenaBytes), the priority slot, writer and operation queues, the wake semaphore, the
///   operation table and one world binding per bound world; destroyed in shutdown step 7b, before Zenith_AsyncIO.
/// Thread affinity: ANY_THREAD_EXCEPT(Create, BindWorld, UnbindWorld, RegisterParticipant, UnregisterParticipant,
///   RegisterSnapshotSystem, SetReplayRecorder, SetThumbnail, SetActiveProfile, SubmitOutOfTick, SubmitResumeWrite,
///   SubmitExitAutosave, ReadResumeIdentity, PrepareFromImage, TakePreparedLoad, TakeSlotList, TakeFinishedScriptLoad,
///   AcquireRetainedCheckpoint, MakeRestoreDesc, BeginProfileSwitch, AbandonProfileSwitch, SetSlotListener, GetSlotListener,
///   BeginExportSlot, GetExportedSlotBytes, TakeExportedSlot, BeginImportSlot, PumpMainThread, BeginShutdown, the
///   destructor). Those are MAIN_THREAD with no
///   frame running; QueueRequest is called from merge nodes and sync points of the bound world (one world's calls never
///   overlap); CaptureForTick by the phase-11 snapshot node of its world; OnResultPublished by its tick-boundary node;
///   GetStats and GetPlaytimeTicks from any thread (operation results are read through the Zenith_OperationHandle, CORE's rule).
/// Invalidation: A world binding is invalid after UnbindWorld; operation handles stay valid until destroyed.
/// Units: Ticks; bytes.
/// Errors: Create returns SAVE_ERROR_INVALID_DESC, SAVE_ERROR_OUT_OF_MEMORY, SAVE_ERROR_THREAD_CREATE; request intake returns
///   SAVE_ERROR_QUEUE_FULL, SAVE_ERROR_REQUEST_LIMIT, SAVE_ERROR_INVALID_SLOT_NAME, SAVE_ERROR_RESERVED_SLOT_NAME,
///   SAVE_ERROR_SHUTTING_DOWN, SAVE_ERROR_WORLD_NOT_BOUND, SAVE_ERROR_TRANSITION_BUSY, SAVE_ERROR_REFUSED_IN_REPLAY,
///   SAVE_ERROR_PROFILE_MISMATCH (Stage 4);
///   SetActiveProfile returns SAVE_ERROR_PROFILE_BUSY while work is queued or running, a checkpoint lease is alive or a slot
///   notice awaits delivery; every
///   subsequent failure is the operation's FAILED status with the Zenith_SaveErrorCode as its failure code, or an
///   ASYNC_SOURCE_SAVE publication for a request that carries a completion ID.
/// Reentrancy: NONE. It invokes the completion sink, the world hooks and the participants (through the snapshotter), none of
///   which may call the save system. Each invocation enters exactly one of three guard kinds, so no guard is ever entered by
///   two threads at once: the invoking world's binding guard (one Zenith_ReentrancyGuard per world binding) for every callback
///   made on that world's merge, sync-point, phase-11 and tick-boundary nodes and for its main-thread out-of-tick captures,
///   which the frame graph orders one at a time per world (a happens-before edge between consecutive nodes, so the guard's
///   plain depth counter is race-free); the writer guard for every callback on Zenith.SaveWriter; the member m_xReentrancy for
///   main-thread calls tied to no world (profile switching, PumpMainThread). Concurrent phase-11 nodes of different worlds
///   enter different guards (Test_SaveSystem_ConcurrentWorldCapturesShareArenas runs under linux-tsan).
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_SaveSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_SaveSystem>, Zenith_SaveError> Create(const Zenith_SaveSystemDesc& xDesc);
	~Zenith_SaveSystem();                                   // joins Zenith.SaveWriter; ZENITH_CHECK if BeginShutdown was not called

	// --- worlds and participants (MAIN_THREAD, before the world's first tick or out of tick) ---
	// ulPlaytimeBaseTicks: the playtime the world starts with (the replaced world's GetPlaytimeTicks at a scene transition,
	// 0 for a startup world; a restore overwrites it with the saved value).
	Zenith_Result<void, Zenith_SaveError> BindWorld(uint16 uWorldTag, Zenith_WorldSimulation& xWorld,
		Zenith_SaveWorldHooks& xHooks, uint64 ulWorldSeed, Zenith_PersistentID xBaseScene, uint64 ulBaseSceneContentHash,
		bool bPrimary, uint64 ulPlaytimeBaseTicks);
	// MAIN_THREAD between frames: the thumbnail every subsequent write of the world carries, copied (at most 512 × 288 RGBA8);
	// an empty span clears it. Presentation input only: it is never hashed or replayed.
	Zenith_Result<void, Zenith_SaveError> SetThumbnail(uint16 uWorldTag, uint16 uWidth, uint16 uHeight,
		Zenith_ByteSpan xRgba8);
	// Cancels the world's queued requests (SAVE_ERROR_CANCELLED; a script LOAD's linked ID through CompleteLinkedLoadRefused) and
	// frees the binding; a checkpoint holder with a live lease survives it until the lease ends.
	void UnbindWorld(uint16 uWorldTag);
	// SAVE_ERROR_INVALID_DESC when GetReissuedSourceKinds names a kind another participant of the world declared,
	// ASYNC_SOURCE_SAVE, or a kind at or above uASYNC_SOURCE_KIND_CAPACITY.
	Zenith_Result<void, Zenith_SaveError> RegisterParticipant(uint16 uWorldTag, Zenith_SaveParticipant& xParticipant);
	void UnregisterParticipant(uint16 uWorldTag, Zenith_SaveParticipant& xParticipant);
	// Registers the phase-11 system "Save.Snapshot" of the world on xSystems, declaring ReadAllComponents,
	// ReadAllResources and every participant's access.
	Zenith_Result<void, Zenith_SaveError> RegisterSnapshotSystem(uint16 uWorldTag, Zenith_SystemRegistry& xSystems);
	void SetReplayRecorder(uint16 uWorldTag, Zenith_ReplayLogWriter* pxWriter, uint64 ulCheckpointIntervalTicks);
	uint64 GetPlaytimeTicks(uint16 uWorldTag) const;   // after the world's latest completed tick

	// --- requests ---
	// Queued in merge order from a sync point (C++ command playback) or a merge node (the script consumer, with the request's
	// m_ulCompletionID set to the ID it allocated); serialized at the world's next phase 11 with a free arena. Returns the request serial.
	Zenith_Result<uint32, Zenith_SaveError> QueueRequest(uint16 uWorldTag, const Zenith_SaveRequest& xRequest);
	// MAIN_THREAD between frames (no frame running): captures the world out of tick at once into a free ordinary arena and
	// returns the operation; allocates no completion ID and no serial, publishes nothing and changes no world state
	// (menu saves of a paused world, the thermal checkpoint, tests). No free arena: SAVE_ERROR_QUEUE_FULL.
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> SubmitOutOfTick(uint16 uWorldTag,
		const Zenith_SaveRequest& xRequest);
	// The phase-11 body (the snapshot system calls it): decides a due autosave, serializes every pending request into a free
	// arena and, under --record, a due replay checkpoint, then hands the jobs to the writer thread.
	Zenith_Result<void, Zenith_SaveError> CaptureForTick(uint16 uWorldTag, const Zenith_WorldInspector& xInspector,
		uint64 ulTickIndex);
	// The tick-boundary handler of the world reports each ASYNC_SOURCE_SAVE publication: decrements m_uOutstanding and clears
	// the outstanding autosave when ulCompletionID is it.
	void OnResultPublished(uint16 uWorldTag, uint64 ulCompletionID);

	// --- resume and exit (PLAT's policy and RHI's clean exit call these through the runtime) ---
	// The primary world paused, out of tick; it alone is captured (the resume image holds one world).
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> SubmitResumeWrite(uint16 uPrimaryWorldTag,
		Zenith_StringView strTargetPath);
	// MAIN_THREAD after the last RunFrame returned: an AUTOSAVE of the primary world captured out of tick at once into arena 2;
	// it first cancels every queued job and the running ordinary write (which rolls back unless it reached journal step 4), so
	// the writer pops it next; BeginShutdown never cancels it; it is bounded by uSAVE_EXIT_AUTOSAVE_BUDGET_MS (Algorithms.md
	// "Shutdown sequence").
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> SubmitExitAutosave(uint16 uPrimaryWorldTag);
	Zenith_Result<Zenith_ResumeIdentity, Zenith_SaveError> ReadResumeIdentity(Zenith_StringView strPath);

	// --- loads and listings ---
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginLoad(Zenith_SaveKind eKind, const Zenith_SaveSlotName& xSlot);
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginLoadPath(Zenith_StringView strPath);   // resume
	// MAIN_THREAD, synchronous: decodes, verifies and migrates an image held in memory (a replay CHECKPOINT, a test image);
	// the bytes are copied.
	Zenith_Result<Zenith_Owned<Zenith_PreparedSave>, Zenith_SaveError> PrepareFromImage(Zenith_ByteSpan xFile);
	Zenith_Result<Zenith_Owned<Zenith_PreparedSave>, Zenith_SaveError> TakePreparedLoad(Zenith_OperationHandle& xLoad);
	// Stage 4: the oldest finished script Save.Load of the world (false when none finished).
	bool TakeFinishedScriptLoad(uint16 uWorldTag, Zenith_SaveScriptLoad& xOut);
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginListSlots();
	Zenith_Result<Zenith_Span<const Zenith_SaveSlotInfo>, Zenith_SaveError> TakeSlotList(Zenith_OperationHandle& xList,
		Zenith_Arena& xArena);
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginDelete(const Zenith_SaveSlotName& xSlot);
	// --- slot transfer, MAIN_THREAD (Stage 4, A3 rows ON1 (a)-(c), ON16 (b), (d); Algorithms.md "Slot export and import") ---
	// The verified bytes of the active profile's slot, or of its .bak as at a load; changes no file.
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginExportSlot(const Zenith_SaveSlotName& xSlot);
	uint64 GetExportedSlotBytes(const Zenith_OperationHandle& xExport) const;   // 0 until SUCCEEDED
	// Copies into xOut (shorter: SAVE_ERROR_INVALID_DESC, nothing copied) and releases the bytes.
	Zenith_Result<Zenith_SaveExportedSlot, Zenith_SaveError> TakeExportedSlot(Zenith_OperationHandle& xExport,
		Zenith_MutableByteSpan xOut);
	// Copies xFile, verifies it as at a load and against the active profile, and writes it to xTarget journaled with a backup.
	Zenith_Result<Zenith_OperationHandle, Zenith_SaveError> BeginImportSlot(const Zenith_SaveSlotName& xTarget,
		Zenith_ByteSpan xFile);
	// The one slot listener (Algorithms.md "Slot listener"); null clears it. INVALID_DESC while another is set.
	Zenith_Result<void, Zenith_SaveError> SetSlotListener(Zenith_SaveSlotListener* pxListener);
	Zenith_SaveSlotListener* GetSlotListener() const;
	// The world's latest retained CHECKPOINT image, pinned for as long as the lease lives.
	Zenith_RetainedCheckpointLease AcquireRetainedCheckpoint(uint16 uWorldTag);
	// The restore desc of a bound, empty world: its participants (registration order), binding, budget and migration context.
	Zenith_Result<Zenith_WorldRestoreDesc, Zenith_SaveError> MakeRestoreDesc(uint16 uWorldTag,
		bool bUnderPublicationSource) const;

	// --- profiles (accounts) ---
	// MAIN_THREAD: refused with SAVE_ERROR_PROFILE_BUSY while any operation is queued or running, any retained-checkpoint lease
	// is alive or a slot notice awaits delivery; otherwise recovers the new profile's journals synchronously, writes the
	// last-profile record and switches. Stage 4: the active profile is an accepted no-op (A3 row ON1 (e)); a malformed key is
	// SAVE_ERROR_INVALID_DESC.
	Zenith_Result<void, Zenith_SaveError> SetActiveProfile(const Zenith_SaveProfileKey& xProfile);
	Zenith_SaveProfileKey GetActiveProfile() const;
	// elaboration (A3 row ON16 (a)), MAIN_THREAD: the intake freeze of Algorithms.md "Profile switch intake freeze" until a
	// SetActiveProfile succeeds; a second call replaces the target; REFUSED_IN_REPLAY, SHUTTING_DOWN, INVALID_DESC (the key).
	Zenith_Result<void, Zenith_SaveError> BeginProfileSwitch(const Zenith_SaveProfileKey& xTarget);
	void AbandonProfileSwitch();                            // MAIN_THREAD: ends the freeze; no-op when none is pending
	// elaboration (A3 row ON1 (d)): the active profile when BindWorld bound uWorldTag, kept for the binding's life; a
	// default-constructed key for a tag with no binding. ANY_THREAD after the bind; SetActiveProfile never changes it.
	Zenith_SaveProfileKey GetBindingProfile(uint16 uWorldTag) const;
	const Zenith_SaveRecoveryReport& GetLastRecoveryReport() const;

	// --- lifecycle and diagnostics ---
	// MAIN_THREAD, prologue step 7: promotes a finished retained checkpoint (SAVE.md "Cross-thread hand-offs"); Stage 4:
	// delivers the slot notices. BeginShutdown also clears the slot listener.
	void PumpMainThread();
	void BeginShutdown();                                    // MAIN_THREAD: refuse new requests; cancel queued work except the exit autosave
	bool IsDrained() const;
	Zenith_SaveSystemStats GetStats() const;
	Zenith_MemoryConsumer& GetMemoryConsumer();   // MEMORY_SUBSYSTEM_SIMULATION, Z2-B-SAVE-001
private:
	Zenith_ReentrancyGuard m_xReentrancy;           // main-thread calls tied to no world
	Zenith_ReentrancyGuard m_xWriterReentrancy;     // callbacks made on Zenith.SaveWriter
	// each Internal Zenith_SaveWorldBinding holds its own Zenith_ReentrancyGuard m_xReentrancy for its world's callbacks
};
inline constexpr uint32 uSAVE_EXIT_AUTOSAVE_BUDGET_MS = 6000;   // the exit autosave's share of the 10 s clean exit
inline constexpr uint32 uSAVE_EXIT_ROLLBACK_WAIT_MS = 1000;     // after it: the wait for the cancelled write's rollback

/// @contract functions Zenith_SaveLastProfile
/// Ownership: Borrow the storage and the scratch arena for the call.
/// Thread affinity: MAIN_THREAD. Read at startup step 11 before Zenith_SaveSystem::Create; written by SetActiveProfile.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ReadLastSaveProfile never fails: a missing, unreadable or corrupt record (Formats.md "Last profile
///   record") gives Zenith_GetLocalSaveProfileKey(); Zenith_WriteLastSaveProfile returns the storage's WriteAtomic error.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_SaveProfileKey Zenith_ReadLastSaveProfile(Zenith_SaveStorage& xStorage, Zenith_Arena& xScratch);
Zenith_Result<void, Zenith_SaveError> Zenith_WriteLastSaveProfile(Zenith_SaveStorage& xStorage,
	const Zenith_SaveProfileKey& xProfile);
```

`Zenith_ResumeIdentity` is PLAT's (`Zenith2/Docs/Design/PLAT/Android.md` → "Resume identity"); `Zenith_SystemRegistry`, `Zenith_WorldSimulation` and `Zenith_WorldInspector` are ECS's (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_WorldSimulation header", "Zenith_WorldInspector header"); `Zenith_MemoryConsumer` is CORE's (`Zenith2/Docs/Architecture.md` → "Memory governor"); `Zenith_ReentrancyGuard` is CORE's ([Z2-N-CONTRACT-359]). `Zenith_SaveWorldBinding` is forward-declared in `Zenith_WorldSnapshot.h` and `Zenith_SaveCheckpointHolder` in `Zenith_SaveSystem.h`, each as an opaque class (their definitions are `ZenithSave/Internal/Zenith_SaveWorldBinding.h`). The two `uSAVE_EXIT_*` constants are derived in `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Shutdown sequence".

## Zenith_SaveRegistration header

```cpp
// Zenith2/Engine/ZenithSave/Zenith_SaveRegistration.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

class Zenith_TypeRegistry;
class Zenith_ConfigSchema;
class Zenith_ScriptBindingTable;

/// @contract functions Zenith_SaveRegistration
/// Ownership: Register into the caller's registry, schema and table.
/// Thread affinity: MAIN_THREAD. Startup step 6 (types and configuration keys) and step 14 (bindings).
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_RegisterSaveTypes returns the registry's Zenith_ReflectError; Zenith_RegisterSaveConfigKeys the schema's
///   error; Zenith_BuildSaveScriptBindings the table's Zenith_ScriptBindingError.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterSaveTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterSaveConfigKeys(Zenith_ConfigSchema& xSchema);
Zenith_Result<void, Zenith_ScriptBindingError> Zenith_BuildSaveScriptBindings(Zenith_ScriptBindingTable& xTable);

// Stable names of the types Zenith_RegisterSaveTypes registers (type IDs from the stable names, Z2-D-020's ID rule):
inline constexpr const char* szSAVE_TYPE_SNAPSHOT_BUFFER = "Zenith.SaveSnapshotBuffer";       // resource, owning PHASE_SAVE_SNAPSHOT;
                                                                                              // no serialized property (never saved)
inline constexpr const char* szSAVE_EVENT_COMPLETED = "Zenith.SaveCompleted";                 // event
inline constexpr const char* szSAVE_EVENT_SLOT_LISTED = "Zenith.SaveSlotListed";              // event
inline constexpr const char* szSAVE_EVENT_SLOT_LIST_DONE = "Zenith.SaveSlotListDone";         // event
inline constexpr const char* szSAVE_COMMAND_REQUEST = "Save.Request";                         // module command kind (C++ requests)
```

The event types' properties (all C++-registered, `Zenith2/Docs/Architecture.md` → "Reflection and runtime type registration"):

| Event | Property (ID, type) |
|---|---|
| `Zenith.SaveCompleted` | 1 `Operation` U8 (`Zenith_SaveOperation`); 2 `Kind` U8; 3 `Result` U8 (0 or a `Zenith_SaveErrorCode`); 4 `Slot` STRING_ID; 5 `RequestSerial` U32; 6 `Bytes` U64; 7 `SnapshotTick` U64; 8 `FromBackup` BOOL |
| `Zenith.SaveSlotListed` | 1 `Slot` STRING_ID; 2 `Kind` U8; 3 `State` U8 (`Zenith_SaveSlotState`); 4 `DisplayName` STRING_ID; 5 `UtcSeconds` U64; 6 `PlaytimeSeconds` U64; 7 `Sequence` U64; 8 `FileBytes` U64 |
| `Zenith.SaveSlotListDone` | 1 `RequestSerial` U32; 2 `Count` U32; 3 `Result` U8 |

Every property's authority is `PHASE_COMMANDS_ONLY` (events are emitted, never written in place).

## Runtime bridge declarations

The SAVE-owned declarations of the runtime bridge (`Zenith2/Engine/ZenithRuntime/Internal/Save/`) are `Zenith2/Docs/Design/SAVE/RuntimeBridge.md` → "Runtime bridge declarations" (moved there in SAVE revision 2 to keep this file under the length cap); they are internal to `ZenithRuntime` and not part of the `ZenithSave` API.

## Script-facing API

The `Save` binding module, built by `Zenith_BuildSaveScriptBindings` through the Luau-free ABI of `ZenithReflect/Zenith_ScriptBinding.h` (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"), so ZenithSave never depends on ZenithScript ([Z2-N-ARCH-085]). Every function appends one `SIDE_EFFECT_KIND_SAVE_REQUEST` record through `AppendSideEffect(SIDE_EFFECT_KIND_SAVE_REQUEST, payload, demand)` (`Zenith2/Docs/Scripting/Bindings.md` → "Public binding types"). The request functions (operations 1–4: `Save.Request`, `Save.Quick`, `Save.Checkpoint`, `Save.ListSlots`, `Save.Delete`, `Save.Load`) pass `demand.m_auCount[SCRIPT_CAPACITY_CLASS_SAVE_REQUESTS] = 1`, the capacity class whose per-tick capacity is Z2-B-SCRIPT-025, the value of `save.maxRequestsPerTick` (`Zenith2/Docs/Design/SCRIPT/Transactions.md` → "Capacity acceptance across ECS and SCRIPT classes"); `Save.SetMetadata` (0x80) and `Save.SetAutosaveEnabled` (0x81) pass a zero `Zenith_ScriptCapacityDemand` (no class), because neither allocates a completion ID nor occupies a queue, so a behaviour may set any number of metadata keys in the tick of a save request without its transaction being rejected for capacity. SCRIPT's commit hands the bridge's consumer each record with completion ID 0, and the consumer allocates the `ASYNC_SOURCE_SAVE` ID of operations 1–4 itself (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Request intake"). Nothing is written before the invocation commits, and a rolled-back invocation requests nothing.

| Luau function | Payload (little-endian) | Result observed by scripts | Stage |
|---|---|---|---|
| `Save.Request(slot: string, displayName: string?)` | `u8` operation 1, `u8` kind MANUAL, `u8` slot length, slot bytes, `u8` display-name length, bytes | `Zenith.SaveCompleted` in phase 9 of the tick whose boundary published the result (emitted in that tick's phase 1) | 3 |
| `Save.Quick()` | operation 1, kind QUICK | as above | 3 |
| `Save.Checkpoint()` | operation 1, kind CHECKPOINT | as above | 3 |
| `Save.SetAutosaveEnabled(enabled: boolean)` | operation 0x81, `u8` 0 or 1; zero capacity demand | none: the world's autosave flag changes in merge order at intake and is saved with the world (WORLD section flags bit 2) | 3 |
| `Save.SetMetadata(key: string, value: string)` | operation 0x80 (metadata only), key, value; zero capacity demand | none: at intake, in merge order, it sets the key in the world's per-tick metadata list (a key set twice keeps the second value; a 30th distinct key is ignored, logged and counted in `save.metadata.dropped`); every request recorded after it in merge order (further on in the same invocation, or in a subsequent invocation of the tick) copies the list as it stands at that request's intake; the list is empty again at the next tick | 3 |
| `Save.ListSlots()` | operation 2 | one `Zenith.SaveSlotListed` per slot (at most 64, byte order of slot name), then `Zenith.SaveSlotListDone` | 3 |
| `Save.Delete(slot: string)` | operation 3, kind MANUAL, slot | `Zenith.SaveCompleted` with `Operation` 3 | 3 |
| `Save.Load(slot: string)` | operation 4, kind MANUAL, slot | `Zenith.SaveCompleted` with `Operation` 4 in the requesting world (Result 0 when the save was read, verified, migrated and handed to the world host; the error otherwise), then, on success, GAME's world transition to the restored world, published under the linked `ASYNC_SOURCE_WORLD_LOAD` completion ID (`Zenith2/Docs/Design/SAVE/Algorithms.md` → "Load pipeline") | 4 (with GAME's save-load transition) |

Before Stage 4 the module registers every function but `Save.Load`, so a script calling it fails with SCRIPT's ordinary unknown-member error.

A script request over the world's per-tick capacity is not a SAVE error: SCRIPT's merge rejects the entity's transaction at the merge point (BR-MERGE, SCRIPT's diagnostic), so the binding raises no Luau error for it and no per-entity cap exists.

The codes below are in SAVE's block ZS5100–ZS5199 of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" ([Z2-N-SCRIPT-926]).

Misuse raises a controlled Luau error in every build ([Z2-N-CONTRACT-473]), never an assert:

| Code | Kind | Misuse | Luau error |
|---|---|---|---|
| ZS5101 | MISUSE | a slot name that fails `Zenith_ParseSaveSlotName` | `ZS5101 Save: invalid slot name '<name>': use 1-32 characters of a-z, 0-9, '_' and '-'` |
| ZS5102 | MISUSE | a reserved slot name (a Windows device name, or an engine slot name for a manual save) | `ZS5102 Save: slot name '<name>' is reserved` |
| ZS5103 | MISUSE | a display name above 128 bytes, a key above 64 bytes or a value above 256 bytes | `ZS5103 Save: <field> exceeds <n> bytes` |
| ZS5105 | MISUSE | a call from a chunk system (only behaviours may request saves) | `ZS5105 Save: save requests are allowed from behaviours only` |
| ZS5106 | MISUSE | a `Save.SetMetadata` key beginning with `zenith.` (reserved for the engine pairs `zenith.location`, `zenith.release` and `zenith.profile`, `Zenith2/Docs/Design/SAVE/Formats.md` → "Metadata section") | `ZS5106 Save: metadata key '<key>' is reserved for the engine` |

ZS5104 is retired unused (it named a per-entity request cap that SCRIPT's capacity class replaces) and is never reassigned.
