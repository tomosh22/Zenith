# SAVE design — replay API

This file is part of the SAVE design family. The entry document is `Zenith2/Docs/Design/SAVE.md`; the other public headers of `ZenithSave` are `Zenith2/Docs/Design/SAVE/PublicAPI.md`, whose rules (results, string-free error records, contract blocks) apply here unchanged. This file holds the one root header of the replay log, `Zenith_ReplayLog.h`; its algorithms are `Zenith2/Docs/Design/SAVE/Algorithms.md` → "Replay recording", "Replay playback", "Replay seeking" and "Replay verification".

## Scope of this file

- The `ZRPL` writer, its sinks, the reader, the verifier and the running build ID, with their contract blocks.
- Architecture's replay enumerations and `Zenith_ReplayPublicationHeader`, placed in this header verbatim and not re-specified (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations").

## Zenith_ReplayLog header

The `ZRPL` replay log of `Zenith2/Docs/Architecture.md` → "Multiplayer foundations", owned by `ZenithSave`. The enumerations and `Zenith_ReplayPublicationHeader` are Architecture's declarations placed in this header verbatim (they are not re-specified here); SAVE adds the writer, reader, index, verifier and the running build ID.

```cpp
// Zenith2/Engine/ZenithSave/Zenith_ReplayLog.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithPlatform/Zenith_InputCommand.h"
#include "ZenithCore/Zenith_ReentrancyGuard.h"
#include "ZenithSave/Zenith_SaveTypes.h"
#include <atomic>                                   // an allowed standard header

class Zenith_Heap;
class Zenith_Arena;
class Zenith_CommandLineFileGrant;

// --- Architecture's declarations, verbatim: Zenith_BackendID, Zenith_ReplayRecordKind, Zenith_ReplayTickFlags,
//     Zenith_AsyncSourceKind, uASYNC_SOURCE_KIND_CAPACITY, Zenith_PublicationKind, Zenith_ReplayPublicationHeader ---

inline constexpr uint32 uREPLAY_FILE_MAGIC = 0x4C50525Au;        // "ZRPL" little-endian
inline constexpr uint16 uREPLAY_FORMAT_VERSION = 1;
inline constexpr uint16 uREPLAY_FLAG_HAS_CHECKPOINTS = 0x0001;
inline constexpr uint32 uREPLAY_MAX_COMMANDS_PER_TICK = 4096;    // Architecture's parser limit
inline constexpr uint32 uREPLAY_MAX_PUBLICATIONS_PER_TICK = 4096;
inline constexpr uint32 uREPLAY_MAX_RECORD_BYTES = 268435456;    // one record, a CHECKPOINT's image included
inline constexpr uint32 uREPLAY_CHECKPOINT_PREFIX_BYTES = 14;     // a CHECKPOINT record's fields before its image
inline constexpr uint32 uREPLAY_MAX_SIMULATION_KEYS = 1024;
inline constexpr uint32 uREPLAY_COMMIT_CHUNK_BYTES = 1048576;     // one chunk of the writer's commit queue

/// @contract value Zenith_BuildID
/// Ownership: Plain 16-byte value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None; IsValid is false for the all-zero value.
/// Version: since Z2 Stage 2. Recorded in ZRPL and ZSAV headers and in the resume identity.
/// @endcontract
struct Zenith_BuildID
{
	uint8 m_auBytes[16] = {};
	bool IsValid() const;
	friend constexpr bool operator==(const Zenith_BuildID&, const Zenith_BuildID&) = default;
};
/// @contract functions Zenith_RunningBuildID
/// Ownership: Reads the image of the module that contains this function; stateless (no cache, no flag), so every call
///   re-reads it; callers read it once at startup and keep the value.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Returns the all-zero ID when the image carries no identity (a link without /DEBUG or --build-id); it never logs
///   (the runtime's single call at startup step 11 logs "save.replay: executable has no build ID").
/// Version: since Z2 Stage 2. Windows: the 16-byte CodeView RSDS GUID of __ImageBase's debug directory (the age is
///   ignored: /INCREMENTAL:NO holds in every configuration). Android and Linux: the object containing
///   &Zenith_GetRunningBuildID, found with dladdr and matched in dl_iterate_phdr by its load base (on Android the game's
///   shared object, never the app_process64 executable); the first 16 bytes of its NT_GNU_BUILD_ID note. The value is the
///   ZRPL header's build ID, the ZSAV header's and Zenith_ResumeIdentity::m_auBuildID.
/// @endcontract
Zenith_BuildID Zenith_GetRunningBuildID();

/// @contract enum Zenith_ReplayErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. The table "Replay error codes" of Zenith2/Docs/Design/SAVE.md → "Error enumerations" gives each meaning.
/// Version: since Z2 Stage 2.
/// @endcontract
enum Zenith_ReplayErrorCode : uint8
{
	REPLAY_ERROR_INVALID_DESC = 1,
	REPLAY_ERROR_OUT_OF_MEMORY = 2,
	REPLAY_ERROR_IO = 3,
	REPLAY_ERROR_BAD_MAGIC = 4,
	REPLAY_ERROR_VERSION = 5,
	REPLAY_ERROR_TRUNCATED = 6,
	REPLAY_ERROR_LIMIT = 7,
	REPLAY_ERROR_IDENTITY = 8,          // m_uField names the mismatched header field
	REPLAY_ERROR_TICK_ORDER = 9,
	REPLAY_ERROR_UNKNOWN_WORLD = 10,
	REPLAY_ERROR_NOT_FOUND = 11,
	REPLAY_ERROR_SINK_FULL = 12,
	REPLAY_ERROR_THREAD_CREATE = 13,
	REPLAY_ERROR_CHECKPOINT = 14,       // a seek's checkpoint failed to restore
};
/// @contract enum Zenith_ReplayIdentityField
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; it names the field of a REPLAY_ERROR_IDENTITY result.
/// Version: since Z2 Stage 2. Never serialized; the order is CheckIdentity's comparison order ([Z2-N-ARCH-784] plus the tick
///   rate, cross-doc item X199).
/// @endcontract
enum Zenith_ReplayIdentityField : uint8
{
	REPLAY_IDENTITY_FIELD_NONE = 0,
	REPLAY_IDENTITY_FIELD_BUILD_ID = 1,
	REPLAY_IDENTITY_FIELD_COMPOSITION_HASH = 2,
	REPLAY_IDENTITY_FIELD_CONTENT_HASH = 3,
	REPLAY_IDENTITY_FIELD_ECS_BACKEND = 4,
	REPLAY_IDENTITY_FIELD_PHYSICS_BACKEND = 5,
	REPLAY_IDENTITY_FIELD_TICK_RATE = 6,
};
/// @contract value Zenith_ReplayError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480].
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_ulOffset is a byte offset in the log; m_ulTick a tick index.
/// Errors: None of its own.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayError
{
	Zenith_ReplayErrorCode m_eCode;
	Zenith_ReplayIdentityField m_eField = REPLAY_IDENTITY_FIELD_NONE;
	uint16 m_uWorldTag = 0;
	uint32 m_uOsCode = 0;
	uint64 m_ulOffset = 0;
	uint64 m_ulTick = 0;
};
static_assert(sizeof(Zenith_ReplayError) == 24);

/// @contract value Zenith_ReplayConfigPair
/// Ownership: Plain value; both views borrow the writer's caller or the reader's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its backing memory.
/// Units: UTF-8 text.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayConfigPair
{
	const Zenith_StringView m_strKey = {};
	const Zenith_StringView m_strValue = {};
};

/// @contract value Zenith_ReplayHeaderInfo
/// Ownership: Plain value; m_axSimulationKeys borrows the writer's caller or the reader's arena.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its backing memory.
/// Units: Hertz for m_uTickHz.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayHeaderInfo
{
	const uint16 m_uFormatVersion = uREPLAY_FORMAT_VERSION;
	const uint16 m_uFlags = 0;
	const Zenith_BuildID m_xBuildID = {};
	const uint64 m_ulCompositionHash = 0;
	const uint64 m_ulContentHash = 0;
	const uint8 m_uEcsBackend = 0;           // Zenith_BackendID
	const uint8 m_uPhysicsBackend = 0;
	const uint32 m_uTickHz = 60;
	const Zenith_Span<const Zenith_ReplayConfigPair> m_axSimulationKeys = {}; // CORE's simulation-affecting list, in key byte order
};

/// @contract interface Zenith_ReplaySink
/// Ownership: Created by Zenith_CreateReplayHostFileSink (--record) or as a Zenith_ReplayMemorySink (tests, the NET command
///   transport, seeking in tools); owned by the writer's creator; outlives the writer.
/// Thread affinity: OWNER_THREAD. Called only by the writer's thread Zenith.ReplayWrite (host-file sink) or by the main thread
///   at CommitFrame (memory sink).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: REPLAY_ERROR_IO (with the OS code) and REPLAY_ERROR_SINK_FULL (a memory sink at capacity).
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ReplaySink
{
public:
	virtual ~Zenith_ReplaySink() = default;
	virtual Zenith_Result<void, Zenith_ReplayError> Append(Zenith_ByteSpan xBytes) = 0;
	virtual Zenith_Result<void, Zenith_ReplayError> Flush() = 0;
	virtual bool NeedsWriterThread() const = 0;   // true for the host-file sink
};

/// @contract type Zenith_ReplayMemorySink
/// Ownership: Owned by its creator; one buffer of m_ulCapacityBytes from the heap at Create.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: GetBytes is invalid after the next Append.
/// Units: Bytes.
/// Errors: Create returns REPLAY_ERROR_OUT_OF_MEMORY; Append beyond the capacity REPLAY_ERROR_SINK_FULL (nothing appended).
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ReplayMemorySink final : public Zenith_ReplaySink
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ReplayMemorySink>, Zenith_ReplayError> Create(uint64 ulCapacityBytes,
		Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_ReplayError> Append(Zenith_ByteSpan xBytes) override;
	Zenith_Result<void, Zenith_ReplayError> Flush() override;     // no-op
	bool NeedsWriterThread() const override;                     // false
	Zenith_ByteSpan GetBytes() const;
};
// The host-file sink over PLAT's Zenith_OpenCommandLineHostFile(xGrant, HOST_FILE_MODE_WRITE_NEW); non-Shipping builds only.
Zenith_Result<Zenith_Owned<Zenith_ReplaySink>, Zenith_ReplayError> Zenith_CreateReplayHostFileSink(
	const Zenith_CommandLineFileGrant& xGrant, Zenith_Heap& xHeap);

/// @contract value Zenith_ReplayLogWriterDesc
/// Ownership: Plain value; both pointers borrowed and outliving the writer.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Milliseconds; chunks of uREPLAY_COMMIT_CHUNK_BYTES; bytes.
/// Errors: None of its own; Create returns REPLAY_ERROR_INVALID_DESC for a null pointer, m_uQueueChunks below 2 or not a
///   power of two, or m_ulMaxLogBytes below 16 MiB.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayLogWriterDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_ReplaySink* const m_pxSink = nullptr;
	const Zenith_ReplayHeaderInfo m_xHeader = {};
	const uint32 m_uMaxWorlds = uSAVE_MAX_WORLDS;
	const uint32 m_uFlushIntervalMs = 1000;  // replay.flushIntervalMs
	const uint32 m_uQueueChunks = 64;        // chunks between CommitFrame and the writer thread (64 MiB by default)
	const uint64 m_ulMaxLogBytes = 536870912; // replay.maxLogBytes: recording stops (FAILED) before the log would exceed it
};

/// @contract type Zenith_ReplayLogWriter
/// Ownership: Created by Create and owned by the runtime's core recorder (or a test); owns one assembler per world session,
///   the commit queue (a Zenith_BoundedQueue of m_uQueueChunks chunks, QUEUE_FULL_POLICY_REJECT) and, when the sink needs
///   one, the writer thread Zenith.ReplayWrite; Close flushes and joins it.
/// Thread affinity: ANY_THREAD_EXCEPT(Create, BeginWorld, SwapWorld, CommitFrame, Close). Those are MAIN_THREAD (startup,
///   frame prologue step 7, frame-loop step 3, shutdown); SetTickPublications runs on the world's tick-boundary node,
///   SetTickInputCommands on its phase-1 node, SetTickHash on its next tick-boundary node, AddCheckpoint on its phase-11 node
///   or on the main thread out of tick (a restored world's checkpoint at the swap); one world's calls are ordered by the
///   frame graph and different worlds touch different assemblers.
/// Invalidation: None.
/// Units: Ticks; bytes.
/// Errors: REPLAY_ERROR_LIMIT (more than 4,096 commands or publications in one tick: the tick is marked
///   REPLAY_TICK_FLAG_NON_REPLAYABLE and the excess is dropped, counted in save.replay.limit_drops; a checkpoint image above
///   min(uREPLAY_MAX_RECORD_BYTES - uREPLAY_CHECKPOINT_PREFIX_BYTES, m_uQueueChunks x uREPLAY_COMMIT_CHUNK_BYTES / 2) is
///   skipped and counted in save.replay.checkpoints_skipped, changing nothing else), REPLAY_ERROR_TICK_ORDER (a tick index
///   not strictly increasing: ZENITH_ASSERT in asserting builds, the record dropped otherwise), REPLAY_ERROR_SINK_FULL (the
///   commit queue is full at CommitFrame, since the main thread never blocks, [Z2-N-CONTRACT-344], or the log would exceed
///   m_ulMaxLogBytes) and REPLAY_ERROR_IO from the sink: in each of these three cases recording stops (FAILED), the log keeps
///   its committed prefix, save.replay.write_errors and the commit queue's dropped counter count it, and the run continues.
/// Reentrancy: NONE. It invokes the sink (on Zenith.ReplayWrite or, for a memory sink, inside CommitFrame), which never calls
///   the writer; its Zenith_ReentrancyGuard checks it.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ReplayLogWriter
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ReplayLogWriter>, Zenith_ReplayError> Create(const Zenith_ReplayLogWriterDesc& xDesc);
	Zenith_Result<uint32, Zenith_ReplayError> BeginWorld(uint16 uWorldTag, uint64 ulWorldSeed,
		Zenith_PersistentID xScene, uint64 ulSceneContentHash);            // returns the WORLD_BEGIN ordinal
	Zenith_Result<void, Zenith_ReplayError> SwapWorld(uint16 uOldTag, uint64 ulOldLastTick, uint16 uNewTag,
		uint64 ulNewSceneContentHash);
	void SetTickPublications(uint16 uWorldTag, uint64 ulTick, Zenith_Span<const Zenith_ReplayPublicationHeader> axHeaders,
		Zenith_Span<const Zenith_ByteSpan> axPayloads);                     // copied; completion-ID order
	void SetTickInputCommands(uint16 uWorldTag, uint64 ulTick, Zenith_Span<const Zenith_InputCommand> axCommands);
	void SetTickFlags(uint16 uWorldTag, uint64 ulTick, uint16 uFlags);   // OR-ed
	void SetTickHash(uint16 uWorldTag, uint64 ulTick, uint64 ulHash);   // completes the tick's record
	void AddCheckpoint(uint16 uWorldTag, uint64 ulTick, Zenith_ByteSpan xSaveImage);   // copied
	Zenith_Result<void, Zenith_ReplayError> CommitFrame();    // completed records, ordered, to the sink
	Zenith_Result<void, Zenith_ReplayError> Close();          // hashes pending ticks must be set first
	uint32 GetWorldOrdinal(uint16 uWorldTag) const;         // of the world's current session
	uint64 GetBytesCommitted() const;
	bool HasFailed() const;
private:
	Zenith_ReentrancyGuard m_xReentrancy;
};

/// @contract value Zenith_ReplayTickView
/// Ownership: Plain value; spans borrow the reader's loaded log.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the reader.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayTickView
{
	const uint64 m_ulTick = 0;
	const uint16 m_uFlags = 0;
	const Zenith_Span<const Zenith_InputCommand> m_axCommands = {}; // decoded, recorded order
	const Zenith_Span<const Zenith_ReplayPublicationHeader> m_axPublications = {};
	const Zenith_Span<const Zenith_ByteSpan> m_axPayloads = {};    // parallel to m_axPublications
	const uint64 m_ulTickHash = 0;
};

/// @contract value Zenith_ReplayWorldSession
/// Ownership: Plain value held in the reader's session table.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the reader.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayWorldSession
{
	const uint32 m_uOrdinal = 0;
	const uint16 m_uWorldTag = 0;
	const uint64 m_ulWorldSeed = 0;
	const Zenith_PersistentID m_xScene = {};
	const uint64 m_ulSceneContentHash = 0;
	const uint64 m_ulFirstTick = 1;          // 1, or the restored tick + 1 (Formats.md "Restored-world convention")
	const uint64 m_ulLastTick = 0;
	const bool m_bRestored = false;
	const uint64 m_ulSwapAfterTick = 0;      // 0 when the session did not end in a swap
	const uint32 m_uSwappedToOrdinal = 0xFFFFFFFFu; // the session the swap began (the next WORLD_BEGIN after its WORLD_SWAP)
};

/// @contract value Zenith_ReplayCheckpointView
/// Ownership: Plain value; m_xSaveImage borrows the reader's loaded log.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the reader.
/// Units: Ticks; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayCheckpointView
{
	const uint32 m_uWorldOrdinal = 0;
	const uint64 m_ulTick = 0;
	const bool m_bCoherentOnly = false;       // the image's header carries uSAVE_HEADER_FLAG_COHERENT_ONLY
	const bool m_bRestoredWorld = false;      // the checkpoint right after a WORLD_BEGIN (the restored-world convention)
	const Zenith_ByteSpan m_xSaveImage = {};
};

/// @contract value Zenith_ReplayPublicationPeek
/// Ownership: Plain value filled by Zenith_ReplayLogReader::FindPublication.
/// Thread affinity: IMMUTABLE. Members non-const: filled through the out-parameter of
///   Zenith_ReplayLogReader::FindPublication.
/// Invalidation: None (a copy, borrowing nothing).
/// Units: Ticks; completion IDs; bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. 32 bytes; asked by GAME (cross-area row GA11 (d')).
/// @endcontract
struct Zenith_ReplayPublicationPeek
{
	uint64 m_ulTick = 0;                     // the publishing tick (the enclosing TICK record's tick index)
	uint64 m_ulCompletionID = 0;
	uint8 m_uPublicationKind = 0;            // Zenith_PublicationKind
	uint8 m_auReserved[7] = {};              // 0
	uint8 m_auPayload[8] = {};               // the first 8 payload bytes, zero-padded when the payload is shorter
};
static_assert(sizeof(Zenith_ReplayPublicationPeek) == 32);

/// @contract type Zenith_ReplayLogReader
/// Ownership: Created by Open over the whole log in memory (read through the host-file grant or given as bytes); owns the
///   decoded index (the session table, each tick record's offset, the checkpoint list and, per (session, source kind), the
///   publications sorted by completion ID) and the per-tick command arrays, all from the heap at Open.
/// Thread affinity: ANY_THREAD after Open: every member is const.
/// Invalidation: None while it lives.
/// Units: Ticks; bytes.
/// Errors: Open returns REPLAY_ERROR_BAD_MAGIC, VERSION, TRUNCATED (a record header or length running past the end: the
///   reader keeps every complete record before it and reports the offset as the end of the log, so an interrupted recording
///   is readable), LIMIT, OUT_OF_MEMORY, IO; CheckIdentity returns REPLAY_ERROR_IDENTITY naming the first mismatched field in
///   the order build ID, composition hash, content hash, ECS backend, physics backend, tick rate ([Z2-N-ARCH-784]).
///   FindNextPublication, FindPublication, GetSessionAfterSwap, GetRestoredCheckpoint and the Find...Checkpoint members return
///   null or false, never an error, for an index, tag or ID the log does not hold.
/// Version: since Z2 Stage 2 (FindNextPublication, FindPublication and GetSessionAfterSwap since Z2 Stage 4). Reads format
///   version 1.
/// @endcontract
class Zenith_ReplayLogReader
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ReplayLogReader>, Zenith_ReplayError> Open(Zenith_ByteSpan xLog,
		Zenith_Heap& xHeap);                              // copies xLog
	static Zenith_Result<Zenith_Owned<Zenith_ReplayLogReader>, Zenith_ReplayError> OpenHostFile(
		const Zenith_CommandLineFileGrant& xGrant, uint64 ulMaxBytes, Zenith_Heap& xHeap);   // replay.maxLogBytes
	const Zenith_ReplayHeaderInfo& GetHeader() const;
	Zenith_Result<void, Zenith_ReplayError> CheckIdentity(const Zenith_ReplayHeaderInfo& xRunning) const;
	Zenith_Span<const Zenith_ReplayWorldSession> GetSessions() const;
	Zenith_Result<Zenith_ReplayTickView, Zenith_ReplayError> GetTick(uint32 uSessionOrdinal, uint64 ulTick) const;
	Zenith_Span<const Zenith_ReplayCheckpointView> GetCheckpoints() const;   // ascending (ordinal, tick)
	const Zenith_ReplayCheckpointView* FindLatestCheckpointAtOrBefore(uint32 uSessionOrdinal, uint64 ulTick) const;
	// The latest checkpoint at or before ulTick whose image is EXACT (seeking skips COHERENT ones); null when none.
	const Zenith_ReplayCheckpointView* FindLatestExactCheckpointAtOrBefore(uint32 uSessionOrdinal, uint64 ulTick) const;
	// The restored-world checkpoint of a session whose m_bRestored is set; null otherwise.
	const Zenith_ReplayCheckpointView* GetRestoredCheckpoint(uint32 uSessionOrdinal) const;
	// GAME's lookahead (cross-area row GA11 (d)): the first publication of source kind uSourceKind in the session of world
	// tag uWorldTag whose tick lies after ulAfterTick (the reader holds no cursor, so the caller passes the tick it reached);
	// false when the session has none.
	bool FindNextPublication(uint16 uWorldTag, uint8 uSourceKind, uint64 ulAfterTick, uint64& ulTickOut,
		uint64& ulCompletionIDOut) const;
	// GAME's intake lookup (cross-area row GA11 (d')): the publication of completion ID ulCompletionID of source kind uSourceKind
	// in the sessions of world tag uWorldTag, searched in log order (the first match wins; a world's completion IDs never repeat
	// within a session, and a restored session continues its predecessor's counter); false when the log holds none. O(log P)
	// through the per-(session, source kind) index built at Open, P that session's publications of the kind.
	bool FindPublication(uint16 uWorldTag, uint8 uSourceKind, uint64 ulCompletionID,
		Zenith_ReplayPublicationPeek& xOut) const;
	// elaboration (A3 row PK24): every publication of source kind uSourceKind in log order (sessions in log order, ticks
	// ascending, completion-ID order within a tick), each passed to pfnVisit once with its world tag, tick, completion ID,
	// publication kind and payload (a span into the reader's copy of the log, valid for the reader's life). No allocation;
	// the visitor must not call back into the reader.
	using Zenith_PfnReplayPublicationVisitor = void (*)(uint16 uWorldTag, uint64 ulTick, uint64 ulCompletionID,
		uint8 uPublicationKind, Zenith_ByteSpan xPayload, void* pUserData);
	void ForEachPublication(uint8 uSourceKind, Zenith_PfnReplayPublicationVisitor pfnVisit, void* pUserData) const;
	// The session that the swap ending session uSessionOrdinal began; null when it did not end in a swap.
	const Zenith_ReplayWorldSession* GetSessionAfterSwap(uint32 uSessionOrdinal) const;
	bool IsTruncated() const;
	uint64 GetValidBytes() const;
};

/// @contract value Zenith_ReplayMismatch
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: the verifier copies the first mismatch into its state.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayMismatch
{
	uint32 m_uSessionOrdinal = 0;
	uint16 m_uWorldTag = 0;
	uint64 m_ulTick = 0;
	uint64 m_ulRecordedHash = 0;
	uint64 m_ulReplayedHash = 0;
};
using Zenith_PfnReplayMismatch = void (*)(const Zenith_ReplayMismatch& xMismatch, void* pUserData);

/// @contract value Zenith_ReplayVerifierDesc
/// Ownership: Plain value; the reader and the user data are borrowed and outlive the verifier.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None of its own; Create returns REPLAY_ERROR_INVALID_DESC for a null reader or callback.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_ReplayVerifierDesc
{
	const Zenith_ReplayLogReader* const m_pxReader = nullptr;
	const Zenith_PfnReplayMismatch m_pfnOnMismatch = nullptr;
	void* const m_pUserData = nullptr;
};

/// @contract type Zenith_ReplayVerifier
/// Ownership: Created by Create as a Zenith_Owned (it holds an atomic, so it is neither copied nor moved) and owned by the
///   runtime's core player; borrows the reader.
/// Thread affinity: ANY_THREAD_EXCEPT(Create). Check runs on a world's tick-boundary node; one world's calls never overlap and
///   the first mismatch is recorded with an atomic compare-exchange on m_bMismatch.
/// Invalidation: None.
/// Units: Ticks.
/// Errors: None; a mismatch is a result: HasMismatch, GetFirstMismatch, and the desc's callback (called once, for the first
///   mismatch), through which the composition root calls Zenith_Runtime::RecordTestAssertionFailure (exit code 5).
/// Reentrancy: NONE. The mismatch callback runs on the boundary node that found the mismatch and never calls the verifier;
///   its Zenith_ReentrancyGuard checks it.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_ReplayVerifier
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ReplayVerifier>, Zenith_ReplayError> Create(const Zenith_ReplayVerifierDesc& xDesc);
	Zenith_ReplayVerifier(const Zenith_ReplayVerifier&) = delete;
	Zenith_ReplayVerifier& operator=(const Zenith_ReplayVerifier&) = delete;
	void Check(uint32 uSessionOrdinal, uint64 ulTick, uint64 ulReplayedHash);
	bool HasMismatch() const;
	Zenith_ReplayMismatch GetFirstMismatch() const;
	uint64 GetTicksVerified() const;
private:
	std::atomic<bool> m_bMismatch{ false };
	Zenith_ReentrancyGuard m_xReentrancy;
};
```

