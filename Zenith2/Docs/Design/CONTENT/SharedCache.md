# CONTENT design: shared DDC, distributed cooking, provenance and storage policy

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies the shared derived-data cache backend (a file-share store implementing ASSET's `Zenith_DerivedDataStore`), its operation, failure modes, administration and deployment; distributed cooking as a partitioned pre-cook through that store; content provenance (the origin schema of the `.meta` member `provenance`, the licence table, the `content.licences` cook step); and the repository storage policy check behind Z2-B-CONTENT-001 … -009. ASSET owns the DDC mechanism, the key, the entry format and the client's fallback rules (`Zenith2/Docs/Design/ASSET/DDC.md → "Failure modes"`, "Shared store contract"); nothing here relaxes them.

## Scope of this file

| Topic | Heading |
|---|---|
| the shared store | "File-share store", "Store layout", "Store operations", "Timeout watchdog" |
| its failure modes | "Unreachable share", "Corrupt entries", "Stale entries" |
| running it | "Store administration", "Deployment" |
| distributed cooking | "Distributed cooking", "Partition protocol" |
| provenance | "Content provenance", "Licence table", "Content licences step" |
| storage budgets | "Storage policy check" |

## File-share store

The shared store is a directory on a file share (an SMB path `\\host\share\ZenithDDC`, or a local or mapped directory for one machine and for tests), holding ASSET's DDC entry files in the same create-only discipline as the local store (`Zenith2/Docs/Design/ASSET/DDC.md → "Local store"`). No server process is deployed: every cook host reads the share directly, and only the hosts "Deployment" names write it (design ruling CT-R1).

<!-- informative -->
A cache server (an HTTP store) would add a long-running service, an HTTP client to ZenithCook and an authentication scheme, for a team whose tools build only on Windows hosts (`ZENITH_TOOLS_HOST`). A file share gives atomic create-only renames (`MoveFileExW` without replace is atomic on NTFS, including through SMB), access control through the share's ACLs, and nothing to operate beyond the share; its weakness, slow failure on a lost network, is bounded by the watchdog below.
<!-- /informative -->

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract value Zenith_FileShareStoreDesc
/// Ownership: Plain value read by Zenith_CreateSharedDerivedDataStore; the strings are copied.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Milliseconds; bytes.
/// Errors: An empty root, a timeout outside 100 … 60,000 ms or a null heap is CONTENT_ERROR_INVALID_ARGUMENT.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_FileShareStoreDesc
{
	const Zenith_StringView m_strRoot = {};        // cook.ddc.sharedRoot
	const uint32 m_uTimeoutMs = 2000;             // cook.ddc.sharedTimeoutMs (ASSET's key; its default)
	const bool m_bWritable = false;                // true for DDC_SHARED_MODE_READ_WRITE
	Zenith_FaultInjector* const m_pxFaults = nullptr; // fault points 0x43540030 …
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract value Zenith_FileShareStoreStats
/// Ownership: Plain value, a snapshot of the store's counters.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; `ZenithContent ddc stats` and the cook report's DDC diagnostics print it.
/// @endcontract
struct Zenith_FileShareStoreStats
{
	uint64 m_ulGets = 0, m_ulHits = 0, m_ulMisses = 0, m_ulUnavailable = 0;
	uint64 m_ulPuts = 0, m_ulPutsStored = 0, m_ulPutsExisting = 0, m_ulPutsFailed = 0;
	uint64 m_ulBytesRead = 0, m_ulBytesWritten = 0;
	uint64 m_ulTimeouts = 0;                      // watchdog cancellations
	uint64 m_ulWatchdogFull = 0;                  // operations refused because all 256 watchdog slots were in use
};

Zenith_Result<Zenith_Owned<Zenith_DerivedDataStore>, Zenith_ContentError> Zenith_CreateSharedDerivedDataStore(
	const Zenith_FileShareStoreDesc& xDesc);
// The counters of a store Zenith_CreateSharedDerivedDataStore created; CONTENT_ERROR_INVALID_ARGUMENT for any other store
// (its GetName() does not begin with "shared:").
Zenith_Result<Zenith_FileShareStoreStats, Zenith_ContentError> Zenith_QueryFileShareStoreStats(
	const Zenith_DerivedDataStore& xStore);

// Zenith2/Tools/Authoring/Internal/CONTENT/SharedDDC/Zenith_FileShareDerivedDataStore.h
/// @contract type Zenith_FileShareDerivedDataStore
/// Ownership: Created by Zenith_CreateSharedDerivedDataStore as a Zenith_Owned<Zenith_DerivedDataStore>; owned by the cook services
///   (item 7) or ZenithContent's ddc commands; owns its watchdog thread, started by Create and joined by the destructor.
/// Thread affinity: ANY_THREAD, as Zenith_DerivedDataStore; Get and Put run on the caller's thread; the counters are atomics.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As the store interface: Get returns HIT, MISS or UNAVAILABLE; Put false when nothing was stored; never a partial HIT.
/// Reentrancy: NOT_APPLICABLE.
/// Locks: uLOCK_LEVEL_CONTENT_DDC_WATCHDOG (950, leaf, short hold; "Timeout watchdog").
/// Version: since Z2 Stage 4. GetName returns "shared:<root>".
/// @endcontract
class Zenith_FileShareDerivedDataStore final : public Zenith_DerivedDataStore
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FileShareDerivedDataStore>, Zenith_ContentError> Create(
		const Zenith_FileShareStoreDesc& xDesc);
	~Zenith_FileShareDerivedDataStore() override;                       // stops and joins the watchdog
	Zenith_StringView GetName() const override;
	Zenith_DerivedDataStoreResult Get(const Zenith_DerivedDataKey& xKey, Zenith_Array<uint8>& xEntryOut) override;
	bool Put(const Zenith_DerivedDataKey& xKey, Zenith_ByteSpan xEntry) override;
	void Remove(const Zenith_DerivedDataKey& xKey) override;            // does nothing ("Store operations")
	bool IsWritable() const override;
	Zenith_FileShareStoreStats GetStats() const;
};
inline constexpr uint16 uLOCK_LEVEL_CONTENT_DDC_WATCHDOG = 950;               // row X306 of the lock ordering table
```

<!-- assumption: pending ASSET -->
`Zenith_DerivedDataStore`, `Zenith_DerivedDataStoreResult`, `Zenith_DerivedDataKey` and an entry verifier `Zenith_VerifyDerivedDataEntry` are reachable from `ZenithAuthoring` (ASSET moves them into `Zenith_AuthoringAsset.h`, the per-owner root header of `Zenith2/Docs/Design/EDIT.md → "Tool-side code the editor needs"`, row CT8; the verifier's success value carries the entry's producer fields, row CT24), and the cook services create the shared store through `Zenith_CreateSharedDerivedDataStore` at item 7 when `cook.ddc.sharedMode` is not `off` (row CT4). Creation failure (`CONTENT_ERROR_STORE_UNUSABLE`) is not a cook failure: the services log `COOK_WARNING_DDC_UNAVAILABLE` and run with the shared mode off, ASSET's unreachable rule applied from the start.
<!-- /assumption -->

## Store layout

```text
<root>/
	store.json                                  the marker: {"$schema": "zenith2.shared-ddc", "schemaVersion": 1, "keySchemaVersion": 1}
	v1/<k0k1>/<k2k3>/<key>.zddc                 one ASSET DDC entry per key (k0 … k3 the key's first four hex digits)
	v1/<k0k1>/<k2k3>/<key>.zddc.<host8>.<pid>.<seq>.tmp   an in-flight Put (host8: 8 hex digits of SHA-256 of the host name)
	quarantine/<key>.zddc.<UTC>                 entries ddc verify found corrupt
	jobs/                                       distributed-cook coordination ("Partition protocol"); never read by the store
```

- The store is usable only when `store.json` exists and parses with `keySchemaVersion` equal to ASSET's key schema version; otherwise `Create` fails `CONTENT_ERROR_STORE_UNUSABLE` naming the reason ("not initialised: run ZenithContent ddc init", "key schema 2 is newer than this tool's 1").
- A key schema change starts a new `v<N>/` beside the old one; the operator deletes the old directory with `ddc prune --version 1` once no tool of that version cooks.
- The host component of temporary names keeps two hosts' temporaries distinct; no host name reaches an entry or a cooked output.

## Store operations

| Operation | Algorithm |
|---|---|
| `Get(key, out)` | open `<entry path>` with `CreateFileW(GENERIC_READ, FILE_SHARE_READ \| FILE_SHARE_DELETE, OPEN_EXISTING)`: not found → MISS; any other open failure → UNAVAILABLE; `GetFileSizeEx` (a server round trip on a share); a size above 4 GiB or below the 64-byte header → UNAVAILABLE; read exactly that many bytes in 8 MiB `ReadFile` calls into `out`; a short read, a read error or the watchdog's cancellation → `out` emptied, UNAVAILABLE. The bytes are returned unverified: ASSET's client verifies every hit (magic, check, key, producer fields, payload hashes) |
| `Put(key, entry)` (writable only) | create the two directories (`CreateDirectoryW`, existing is success); write `<entry path>.<host8>.<pid>.<seq>.tmp` with `CREATE_NEW` in 8 MiB `WriteFile` calls, `FlushFileBuffers`, close; `MoveFileExW(tmp, entry, MOVEFILE_WRITE_THROUGH)` without `MOVEFILE_REPLACE_EXISTING`: success, or `ERROR_ALREADY_EXISTS`/`ERROR_FILE_EXISTS` treated as success (another host stored the same deterministic entry) with the temporary deleted; any other failure deletes the temporary and returns false |
| `Remove(key)` | does nothing: a client never deletes from a shared store (`Zenith2/Docs/Design/ASSET/DDC.md → "Shared store contract"` item 3) |
| `IsWritable()` | `m_bWritable` |

- The store never writes anything but complete entries through create-only rename (and its own temporaries, which only it deletes, and only on a failed Put of its own); a reader therefore sees no entry or a complete one.
- Entries are never modified after the rename, so a Get racing a Put of the same key returns MISS or the complete entry (`Test_ContentPipeline_SharedDDCConcurrentPuts` runs 8 `ZenithCook` processes cooking one game into one share at once and asserts one entry per key, no partial read and no surviving temporary).

## Timeout watchdog

A share whose server vanishes can block a Windows file call for tens of seconds. The store bounds every **file call** by `m_uTimeoutMs`, and so every operation by the sum over its calls:

1. Before each `CreateFileW`, `GetFileSizeEx`, `ReadFile`, `WriteFile`, `FlushFileBuffers`, `CreateDirectoryW`, `MoveFileExW` or `DeleteFileW`, the calling thread registers (thread handle, deadline, cancelled false) in the watchdog's table, a fixed array of 256 slots guarded by the watchdog mutex (`uLOCK_LEVEL_CONTENT_DDC_WATCHDOG`, 950, a leaf: its holder acquires no other lock, never logs, allocates or performs file I/O, because every lock it could reach, such as the log sinks at 920, is lower); a full table makes the operation UNAVAILABLE at once, counted.
2. The watchdog thread wakes every 50 ms, takes the mutex, and for each slot past its deadline and not yet cancelled calls `CancelSynchronousIo(thread)` once and marks the slot cancelled, all **while holding the mutex**; it releases the mutex before sleeping.
3. After the call returns, the calling thread takes the mutex, clears its slot and releases it. Because the cancel is issued only under the mutex and only for a registered slot, a cancel can never land on the thread's next call: once the slot is cleared, no cancel for it can be issued, and a cancel issued before the clear hits the call it was meant for or none (`CancelSynchronousIo` on a thread with no pending synchronous I/O does nothing).
4. The cancelled call fails with `ERROR_OPERATION_ABORTED`; the operation returns UNAVAILABLE (Get) or false (Put, after deleting its temporary when it can).
5. ASSET's client counts it and, after 3 consecutive unavailable results, disables the shared store for the rest of the run (`Zenith2/Docs/Design/ASSET/DDC.md → "Unreachable store"`).

The bounds guaranteed. Per call: the watchdog issues the cancel within `m_uTimeoutMs` + 50 ms of the call's registration (its 50 ms wake period), and a call the SMB redirector honours the cancel for returns after that plus the redirector's own return latency. Per operation: a `Get` of s bytes makes at most ⌈s / 8 MiB⌉ + 2 watched calls (`CreateFileW`, `GetFileSizeEx`, the reads) and a `Put` at most ⌈s / 8 MiB⌉ + 5 (two `CreateDirectoryW`, `CreateFileW`, the writes, `FlushFileBuffers`, `MoveFileExW`), plus one `DeleteFileW` on failure, so an operation returns within its call count × (`m_uTimeoutMs` + 50 ms) plus the redirector's return latency per call: a 4 GiB `Get` against a share that stalls on every call could take 514 × 2.05 s, which is why ASSET's client stops calling after 3 consecutive unavailable results. A call the redirector does not cancel promptly (an open against a server whose session is already being torn down can wait for the redirector's own session timeout) blocks its own thread until the redirector gives up, and no other thread waits for it, because every other item's calls carry their own deadlines and, once 3 consecutive results were unavailable, ASSET's client sends no further call to the store. `Test_SharedDDC_TimeoutCancelsAndFallsBack` holds a Get with the fault point `uFAULT_POINT_CONTENT_SHARED_DDC_STALL` (0x43540030, which blocks the call in a cancellable wait on a pipe that never fills) and asserts UNAVAILABLE within `m_uTimeoutMs` + 100 ms (the cancel bound plus the cancellable wait's return) and a local cook of the item; the real redirector's behaviour is the `needs-human` Stage 4 acceptance step "share unplugged mid-cook" (Tests.md "Needs-human recordings"): a two-host cook whose share host's network cable is pulled mid-cook, recording that the cook completes with exit 0, a byte-identical tree and every stalled thread released by the end of the run.

## Unreachable share

| Cause | Store result | Client and cook behaviour |
|---|---|---|
| root path absent, share offline, access denied at open | UNAVAILABLE (Get), false (Put) | ASSET: counted, `COOK_WARNING_DDC_UNAVAILABLE` once, disabled after 3 consecutive failures; every item cooks locally; exit code unchanged |
| timeout | UNAVAILABLE / false | the same |
| marker missing or newer | `Create` fails; the services run with the shared mode off | the same, from the start |

The cooked tree is byte-identical to a cook with the shared mode off, and the share is unchanged (`Test_SharedDDC_UnreachableFallsBackLocal`; the Stage 4 acceptance run `Test_ContentPipeline_SharedDDCFailureModes`).

## Corrupt entries

A shared entry that fails ASSET's verification (a flipped byte, a truncated file left by an external tool, a wrong key) is a miss for the client, which reports it with its key and never deletes it (ASSET's rule); the item cooks locally and, in read-write mode, the client's `Put` of the good entry fails as "exists", so the corrupt entry stays until the operator removes it. CONTENT therefore makes corruption visible and removable:

- The cook report's `diagnostics` carry each corrupt key (ASSET), and `ZenithContent ddc verify` ("Store administration") finds and quarantines corrupt entries.
- `Test_SharedDDC_CorruptEntryLeftAndReported` flips one payload byte of a shared entry, cooks, and asserts the cooked tree equals a clean cook's, the share's bytes are unchanged, and the report names the key; `Test_SharedDDCAdmin_VerifyQuarantinesCorrupt` then runs `ddc verify` and asserts the entry moved to `quarantine/` and the next cook re-populates the key.

## Stale entries

An entry whose key matches but whose producer fields (step ID, step version, engine cook version, key schema version) differ is a stale miss for the client (`Zenith2/Docs/Design/ASSET/DDC.md → "Stale entry"`), left in place; it arises only from a producer defect, such as a step whose output changed without a version bump, which "Deployment" keeps out of the shared store by letting only pinned, clean tool builds write it. `Test_SharedDDC_StaleEntryIsMiss` writes an entry with the expected key and a lower step version (through ASSET's entry writer, the test-access route of row CT8), cooks, and asserts a local cook, an unchanged share and the stale key in the report; `ddc verify --expect-producers <file>` lists such entries for the operator.

## Store administration

```text
ZenithContent ddc init   --root <dir>                               creates the marker and v1/ (refuses a non-empty directory without a marker)
ZenithContent ddc stats  --root <dir> [--json]                      entry count and bytes per v<N>/, temporaries older than 1 hour, quarantine size
ZenithContent ddc verify --root <dir> [--sample <percent>] [--expect-producers <file>] [--json]
ZenithContent ddc prune  --root <dir> --max-bytes <n> [--min-age-days <d>] [--version <N>] [--json]
```

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract value Zenith_FileShareAdminValues
/// Ownership: Plain values; spans and views point into the arena the call was given.
/// Thread affinity: IMMUTABLE. Members non-const in Zenith_FileShareVersionStats, Zenith_FileShareAdminStats and
///   Zenith_FileSharePruneResult: its producer default-constructs it and fills it field by field before returning it.
/// Invalidation: With the arena.
/// Units: Bytes; counts; days; percent.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; their JSON forms are the --json outputs of the four commands.
/// @endcontract
struct Zenith_FileShareAdminDesc
{
	const Zenith_StringView m_strRoot = {};
	const Zenith_ContentClock m_xClock = {};      // ages of temporaries, entries and quarantine files; null members: the system clock
	Zenith_FaultInjector* const m_pxFaults = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
};
struct Zenith_FileShareVersionStats { uint32 m_uVersion; uint64 m_ulEntries, m_ulBytes; };
struct Zenith_FileShareAdminStats
{
	Zenith_Span<const Zenith_FileShareVersionStats> m_axVersions;   // ascending version
	uint64 m_ulStaleTemporaries, m_ulStaleTemporaryBytes;           // *.tmp older than 1 hour
	uint64 m_ulQuarantineFiles, m_ulQuarantineBytes;
};
struct Zenith_FileShareVerifyOptions
{
	const uint32 m_uSamplePercent = 100;                            // 1 … 100; the sample seed is SHA-256 of the root path
	const Zenith_JsonDocument* const m_pxExpectProducers = nullptr;  // step ID → version; null: no stale listing
};
enum Zenith_FileShareFindingKind : uint8 { FILE_SHARE_FINDING_CORRUPT = 1, FILE_SHARE_FINDING_STALE };
struct Zenith_FileShareVerifyFinding { const Zenith_FileShareFindingKind m_eKind = {}; const Zenith_StringView m_strKey = {}, m_strReason = {}; };
struct Zenith_FileShareVerifyResult
{
	const uint64 m_ulChecked = 0, m_ulQuarantined = 0, m_ulStale = 0;
	const Zenith_Span<const Zenith_FileShareVerifyFinding> m_axFindings = {}; // key order
};
struct Zenith_FileSharePruneOptions
{
	const uint64 m_ulMaxBytes = 0;                                  // per version directory
	const uint32 m_uMinAgeDays = 7;
	const uint32 m_uDeleteVersion = 0;                              // 0: none; N: delete the whole v<N>/ (never the current)
};
struct Zenith_FileSharePruneResult { uint64 m_ulEntriesDeleted, m_ulBytesDeleted, m_ulTemporariesDeleted, m_ulQuarantineDeleted, m_ulBytesRemaining; };

/// @contract type Zenith_FileShareStoreAdmin
/// Ownership: Created by Create as a Zenith_Owned; borrows the desc's heap, faults and clock user data; holds no file open between
///   calls.
/// Thread affinity: OWNER_THREAD (ZenithContent's main thread).
/// Invalidation: None.
/// Units: As Zenith_FileShareAdminValues.
/// Errors: CONTENT_ERROR_INVALID_ARGUMENT (an empty root, a sample outside 1 … 100, a prune of the current version),
///   CONTENT_ERROR_STORE_UNUSABLE (Stats, Verify, Prune without a valid marker; Init over a non-empty directory without one),
///   CONTENT_ERROR_IO (the OS error's low 16 bits in m_uDetail), CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side; run as the store operator, never inside a cook.
/// @endcontract
class Zenith_FileShareStoreAdmin
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FileShareStoreAdmin>, Zenith_ContentError> Create(
		const Zenith_FileShareAdminDesc& xDesc);
	Zenith_Result<void, Zenith_ContentError> Init();
	Zenith_Result<Zenith_FileShareAdminStats, Zenith_ContentError> Stats(Zenith_Arena& xArena);
	Zenith_Result<Zenith_FileShareVerifyResult, Zenith_ContentError> Verify(const Zenith_FileShareVerifyOptions& xOptions,
		Zenith_Arena& xArena);
	Zenith_Result<Zenith_FileSharePruneResult, Zenith_ContentError> Prune(const Zenith_FileSharePruneOptions& xOptions);
};
```

<!-- elaboration -->
- `verify` reads every entry (or a seeded sample, `--sample` 1 … 100), verifies it with `Zenith_VerifyDerivedDataEntry` against the key its name states, and moves each failing entry to `<root>/quarantine/<key>.zddc.<UTC>` with `MoveFileExW` (a reader holding it open keeps reading: entries are opened with `FILE_SHARE_DELETE`); with `--expect-producers` (a JSON map step ID → version) it also compares each verified entry's producer step ID and version (the verifier's success value, row CT24) and lists stale entries without moving them. Exit 1 when anything was quarantined.
- `prune` evicts entries oldest first by creation time (`FILETIME` of `GetFileInformationByHandle`; last-access time is not used, because servers commonly disable it) until the version directory's bytes are at most `--max-bytes`, never an entry younger than `--min-age-days` (default 7); it deletes `*.tmp` older than 1 hour and quarantine files older than 30 days; `--version N` deletes a whole old `v<N>/`. A deleted entry is a future miss, never an error. Ages are measured against `m_xClock`'s UTC.
- Both run as the store operator (an account with delete rights), never inside a cook; `Test_SharedDDCAdmin_PruneNeverExceedsAndKeepsYoung` and `_VerifyQuarantinesCorrupt` cover them.

## Deployment

Who may write the store is decided by provenance, not by trust in a machine. ASSET's keys do not include ZenithCook's own build identity (`Zenith2/Docs/Design/ASSET/DDC.md → "Key inputs"`), so an entry cooked by a tool built from uncommitted or unreviewed code (a local step change that did not bump its version) is verified as valid under the right key with the wrong bytes, and every reader would take it. The store is therefore written only by ZenithCook binaries built from a clean checkout at a committed revision, which in this design means the distribution helpers alone ("Partition protocol"): they verify the checkout, build their own ZenithCook from it, and record the tool they ran.

<!-- elaboration -->
| Item | Rule |
|---|---|
| location | one share per team, on a host reachable from every cook host at ≤ 2 ms round trip (same LAN); `cook.ddc.sharedRoot` set in each developer's command-line layer or the editor's preferences, never in a tracked file |
| permissions | the writer group (the service account under which the helpers run): read, create files and folders, write, and delete on files they create (`CREATOR OWNER` delete, needed to remove a failed temporary and for the rename); every other account, developers and CI included: read only, so a developer machine cannot write the store even when misconfigured; the operator account: full control |
| modes | developer machines `read`; CI cooks `read`; distribution helpers `readWrite`, set by the helper itself on the cook it runs; default `off`. A full pre-population of the store is a distributed cook with one partition (`ZenithContent distribute --partitions 1`), so it too runs through a helper |
| population | a scheduled task on one coordinator host the operator registers (Windows Task Scheduler), nightly at 02:00 local time and, when the team wants warmer hits, after each commit to `master`: it updates its own clean checkout to the commit (`git fetch` then `git switch --detach <commit>`), then for each flagship slice and platform runs `ZenithContent distribute --game <Name> --platform <windows\|android> --jobs <share>/jobs --partitions <n>` against the registered helpers; the shared hit-ratio row ("Pending new rows" of `Zenith2/Docs/Design/CONTENT.md`) is measured on a second host after that population. The task is operator configuration on a host, not a tracked file; running it from CI instead needs a `.github/workflows` file, a protected path, so that is a PROC `needs-human` hand-off, never a CONTENT ticket's edit |
| toolchain | helpers build with the toolchain `Setup-Zenith2.ps1` pins (`Zenith2/Docs/Design/SHIP/Manifest.md → "The toolchain section"`), so two helpers at one commit build the same tool |
| capacity | at least 4 × the sum of the flagship slices' cooked sizes per platform; `ddc prune --max-bytes` scheduled daily by the operator |
| initialisation | `ZenithContent ddc init --root <dir>` once |
| security | the store holds only derived data any host can reproduce; a malicious entry is still verified by key, producer and payload hashes before use, and `--verify` cooks re-produce sampled hits (`Zenith2/Docs/Design/ASSET/DDC.md → "Verification sampling"`) |

`Zenith2/Docs/Design/CONTENT/SharedCache.md` is the deployment reference; `ZenithContent ddc init --help` prints the permission table.

## Distributed cooking

Distributed cooking is a **partitioned pre-cook** (design ruling CT-R6): n helper hosts, each with a clean checkout at the commit being cooked and a ZenithCook built from it, run ordinary cooks restricted to disjoint partitions of the game's sources into the shared store, and the coordinator's own ordinary cook then finds their outputs as shared hits. Correctness rests on ASSET's keys and on the tool pin: an output is used only under the key of its exact inputs, and only a tool built from the cooked commit writes it, so a helper on a wrong commit, with a different tool, crashed or missing costs time and never changes a byte (`Zenith2/Docs/Design/ASSET/Cook.md → "Keys and invalidation"`). That needs every intermediate CONTENT writes to be byte-identical on every helper for the same inputs, which is why no CONTENT code whose result reaches an intermediate calls a CRT transcendental (`Zenith2/Docs/Design/CONTENT.md → "Common format rules"`: the colour downscale and stb_vorbis's tables use CORE's deterministic functions).

<!-- assumption: pending ASSET -->
ZenithCook exposes the run description's existing `m_axOnly` ("these assets and their dependencies", `Zenith2/Docs/Design/ASSET/Cook.md → "Driver, server and worker pool API"`) on its command line as `--only @<file>` (a file of GUID lines, LF, at most 1,048,576 lines; a source GUID selects every product of the source), rows CT23 superseding CT7's partition key: a helper cooks its partition's sources and their dependencies into a scratch `--out`, populating the shared store, and needs no new cook mode. The cost of the lighter mechanism is that a dependency two partitions share is cooked or fetched by both, which the create-only `Put` makes harmless.
<!-- /assumption -->

```text
ZenithContent distribute --game <Name> --platform <windows|android> --jobs <dir> --partitions <n> [--wait-min <m>] [--cook]
ZenithContent distribute --clean --jobs <dir> [--older-than-days <d>]
ZenithContent helper     --jobs <dir> --checkout <dir> --ddc-root <dir> [--once] [--max-jobs <k>]
```

- `distribute` (the coordinator) writes a job and its partition files, waits for its partitions, prints the summary, and with `--cook` then runs the ordinary cook; `n` is 1 … 64; `--wait-min` default 30.
- `helper` runs on each helper host as a long-lived process (or `--once` for one job), claiming partitions of any job whose game and platform it can cook.

## Partition protocol

The job directory (`--jobs`, normally `<share>/jobs/`) holds one directory per job, `<jobs>/<jobId>/`, with `jobId` = `<UTC>-<8 hex of SHA-256 of the job.json bytes>`; every file is written as `<name>.<host8>.tmp` then renamed, so readers see complete files; every JSON file has `$schema` `zenith2.content-distribute`, version 1:

| File | Writer | Content |
|---|---|---|
| `job.json` | coordinator | `{game, platform, commit (40 hex), partitions n, created (UTC), cookArgs: [extra --set pairs]}` |
| `partition-<k>.txt` | coordinator | the partition's source GUIDs, one per line, ascending |
| `claim-<k>.json` | the helper claiming partition k | `{host8, pid, claimed (UTC)}`, created with `CREATE_NEW`, so exactly one helper wins a partition |
| `heartbeat-<k>.json` | that helper, every 10 s | `{host8, pid, at (UTC), itemsRun}` |
| `done-<k>.json` | that helper | `{host8, exitCode, itemsRun, ddcSharedPuts, seconds, toolCommit (40 hex), toolSha256 (64 hex), outcome: "ok" \| "failed" \| "treeMismatch" \| "toolMismatch"}` |
| `cancel.json` | coordinator | present: helpers stop at their next item boundary |

Partition states are derived from the files by one pure function, so the 60-second rule is testable with an injected clock:

```cpp
// Zenith2/Tools/ZenithContent/Internal/Distribute/Zenith_ContentDistribute.h (ZenithContent_Objects; ZenithContent_Tests links it)
/// @contract value Zenith_ContentPartitionFiles
/// Ownership: Plain value; what the coordinator or a helper read from one partition's files.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Seconds since 1970-01-01T00:00:00Z, each from its writer's clock.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-internal.
/// @endcontract
struct Zenith_ContentPartitionFiles
{
	const bool m_bClaim = false;     const uint64 m_ulClaimedUtcSeconds = 0;
	const bool m_bHeartbeat = false; const uint64 m_ulHeartbeatUtcSeconds = 0;
	const bool m_bDone = false;
};
enum Zenith_ContentPartitionState : uint8 { CONTENT_PARTITION_STATE_OPEN = 1, CONTENT_PARTITION_STATE_CLAIMED,
	CONTENT_PARTITION_STATE_STALE, CONTENT_PARTITION_STATE_DONE };
inline constexpr uint64 ulCONTENT_PARTITION_STALE_SECONDS = 60;
// DONE when a done file exists; OPEN without a claim; otherwise with t = the larger of the claim and heartbeat times, CLAIMED when
// ulNowUtcSeconds < t + 60 (a t in the future, a writer's clock ahead of the reader's, is CLAIMED) and STALE otherwise.
Zenith_ContentPartitionState Zenith_DerivePartitionState(const Zenith_ContentPartitionFiles& xFiles,
	uint64 ulNowUtcSeconds);

/// @contract value Zenith_ContentDistributeDesc
/// Ownership: Plain value; strings borrowed for the call; the clock's user data outlives it.
/// Thread affinity: IMMUTABLE. Members non-const in Zenith_ContentPartitionSummary: its producer default-constructs it
///   and fills it field by field before returning it.
/// Invalidation: None.
/// Units: Minutes for m_uWaitMinutes; the clock's units.
/// Errors: Zenith_RunContentCoordinator returns CONTENT_ERROR_INVALID_ARGUMENT for n outside 1 … 64, CONTENT_ERROR_GIT for a dirty or
///   unreadable checkout, CONTENT_ERROR_DISTRIBUTE for malformed job files, CONTENT_ERROR_IO.
/// Version: since Z2 Stage 4. Tool-internal.
/// @endcontract
struct Zenith_ContentDistributeDesc
{
	const Zenith_StringView m_strGame = {}, m_strPlatform = {}, m_strJobsDir = {}, m_strCheckout = {};
	const uint32 m_uPartitions = 1;
	const uint32 m_uWaitMinutes = 30;
	const uint32 m_uPollSeconds = 10;
	const Zenith_ContentClock m_xClock = {};        // null members: Zenith_GetSystemContentClock()
	Zenith_Heap* const m_pxHeap = nullptr;
};
struct Zenith_ContentPartitionSummary { uint32 m_uIndex; Zenith_ContentPartitionState m_eState; Zenith_StringView m_strHost8, m_strOutcome; uint64 m_ulItemsRun, m_ulSharedPuts; };
Zenith_Result<uint8, Zenith_ContentError> Zenith_RunContentCoordinator(const Zenith_ContentDistributeDesc& xDesc,
	Zenith_Array<Zenith_ContentPartitionSummary>& axSummaryOut);   // the exit code of "Coordinator" step 3
```

Times are each writer's UTC clock; hosts on one LAN are kept within a few seconds by the Windows time service, and the 60-second threshold is six heartbeat periods, so skew below 50 s never makes a live partition STALE.

Helper, per job it can serve (game present in its checkout, platform supported):

1. Claim the lowest OPEN or STALE partition k (a STALE one: delete its claim, then its heartbeat, in that order, then claim).
2. Verify the checkout: `git -C <checkout> rev-parse HEAD` equals the job's commit, and `git -C <checkout> status --porcelain -- Zenith2` is empty; otherwise write `done-<k>.json` with `treeMismatch` and stop (another helper may not retry it: a mismatch is reported, not repeated; the coordinator's cook covers the partition).
3. Pin the tool. Let h be the checkout hash of `<checkout>/Zenith2` ([Z2-N-ARCH-159]: the first 16 lower-case hex digits of the SHA-256 of its absolute path, lower-cased, with `/` separators and no trailing slash; the helper computes it and sets `ZENITH_CHECKOUT_HASH` for its children, as `Get-Zenith2CheckoutHash` would) and B = `<checkout>/Zenith2/out/<h>/win`, the `win` preset's binary directory. The helper's tool stamp `<checkout>/Zenith2/out/<h>/distribute/tool-stamp.json` records `{commit, toolPath, toolSha256}` of the last ZenithCook it built. When the stamp's commit differs from the job's, or the file at `toolPath` no longer has `toolSha256`, the helper builds: it creates the empty CMake File API query file `B/.cmake/api/v1/query/codemodel-v2` (CMake writes a reply only for a query present before configure), runs `cmake --preset win` (configure), then `cmake --build --preset win-release --target ZenithCook`, with a 3,600-second timeout for the two, then reads the reply: the lexicographically last `B/.cmake/api/v1/reply/index-*.json`, its `reply` member `codemodel-v2`'s `jsonFile`, in that file the `configurations` element whose `name` is `Release`, its `targets` element whose `name` is `ZenithCook`, that target's `jsonFile`, and its `artifacts[0].path`, relative to B; then it writes the stamp with the executable's SHA-256. A failed or timed-out build, or a reply without that target, writes `done-<k>.json` with `toolMismatch` and stops. The built tool is the job commit's by construction: the checkout was verified clean at that commit in step 2 and the toolchain is the pinned one.
4. Start the heartbeat thread; run `<toolPath> --game <G> --platform <P> --out <checkout>/Zenith2/out/<h>/distribute/<jobId> --only @<jobs>/<jobId>/partition-<k>.txt --set cook.ddc.sharedMode=readWrite --set cook.ddc.sharedRoot=<ddc-root>` plus the job's `cookArgs`, polling `cancel.json` every 10 s (on cancel: terminate the cook, which leaves no partial entry because entries are published by rename).
5. Join the heartbeat thread; write `done-<k>.json` with the exit code, the cook summary's counts, `toolCommit` (the stamp's commit) and `toolSha256`; delete its scratch output directory.

Coordinator:

1. Check `git rev-parse HEAD` and a clean `Zenith2/` (a dirty tree is refused, exit 3: helpers cook the commit, not the working tree); write `job.json`; list the game's sources by reading every `*.meta` under the content roots of the game's `game.json` (outside `.migration/`) and taking its top-level `guid`; sort the GUID texts ascending and give the GUID at index i to partition i mod n; write `partition-<k>.txt` for each k.
2. Every `m_uPollSeconds`, derive each partition's state with `Zenith_DerivePartitionState`; finish when every partition is DONE or STALE, or when `--wait-min` passes (then write `cancel.json`).
3. Print the summary (`--json`: per partition the state, host8, outcome, items and puts); a DONE partition counts as `ok` only when its outcome is `ok` and its `toolCommit` equals the job's commit (another commit's tool is reported `toolMismatch` whatever the helper wrote); exit 0 when every partition is DONE with `ok`, 1 otherwise (a degraded run is still correct).
4. With `--cook`, run the ordinary cook (shared mode `read`, its own deployment mode), whose report shows the shared hits.

Jobs are never deleted by helpers; `ZenithContent distribute --clean --jobs <dir> --older-than-days <d>` (default 7) deletes old job directories.

Equivalence: `Test_ContentPipeline_DistributedCookEqualsLocal` creates a temporary git repository holding the fixture game at one commit, makes three helper checkouts of it as git worktrees at that commit, and runs a coordinator and three helper processes over a local share; each helper runs the suite's own built `ZenithCook`, whose stamp the test writes with the repository's commit (so step 3 builds nothing, and the test still exercises the stamp check by giving one helper a stamp of another commit, which must report `toolMismatch`); it kills one helper mid-run, and asserts the coordinator's final cooked tree is byte-identical to a clean local cook, its report shows shared hits for the completed partitions, and no host name appears in any output; `Test_ContentDistribute_LostHelperDoesNotBlock`, `Test_ContentDistribute_TreeMismatchRefused` and `Test_ContentDistribute_PartitionStateClock` cover the failure rows.

## Content provenance

The `.meta` member `provenance` is CONTENT's origin schema, carried verbatim by ASSET into every provenance record of the source's outputs (`Zenith2/Docs/Design/ASSET/Database.md → "Provenance records"`); version 1:

```json
"provenance": {
	"schema": 1,
	"origin": "thirdParty",
	"tool": "Blender",
	"toolVersion": "4.2.3",
	"author": "Studio Art Team",
	"sourceFile": "Art/Characters/Hero/Hero.blend",
	"licence": "CC-BY-4.0",
	"attribution": "Hero model by J. Example, CC BY 4.0",
	"url": "https://example.org/hero",
	"notes": ""
}
```

| Key | Type | Presence | Rule |
|---|---|---|---|
| `schema` | integer | required | 1 (V1) |
| `origin` | string | required | `authored` (made by the project), `thirdParty` (obtained from outside), `generated` (produced by a tool or script of the project), `scanned` (photogrammetry, recordings) (V2) |
| `tool`, `toolVersion` | string | optional | ≤ 64 bytes each |
| `author` | string | optional | ≤ 256 bytes |
| `sourceFile` | string | optional | ≤ 512 bytes: where the working file lives (a repository path or an external reference); informative |
| `licence` | string | required when `origin` is `thirdParty` or `scanned` | a row of "Licence table" (V3, V4) |
| `licenceText` | string | required for `LicenseRef-Commercial-*` | the licence's terms or the purchase record, ≤ 16 KiB (V5) |
| `attribution` | string | required when the licence row requires attribution | ≤ 1,024 bytes (V6) |
| `url` | string | optional | ≤ 2,048 bytes, `https:` or `http:` |
| `notes` | string | optional | ≤ 4,096 bytes |
| `x-<anything>` | any | optional | preserved |

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentValidation.h
/// @contract functions Zenith_ContentProvenanceValidation
/// Ownership: Messages are appended to the caller's array, their text in the caller's arena.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: CONTENT_ERROR_PROVENANCE_INVALID (m_uDetail the first error rule) for any error message; CONTENT_ERROR_LICENCE_FORBIDDEN
///   (m_uDetail the licence table row) for a forbidden licence; CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
Zenith_Result<void, Zenith_ContentError> Zenith_ValidateContentProvenanceJson(Zenith_JsonNode xProvenance,
	Zenith_Array<Zenith_ContentValidationMessage>& axMessagesOut, Zenith_Arena& xArena);
```

The function applies V1 … V7 (V7: no member outside the table except `x-`, a warning). No time, host or user is required or added by any tool: a person writes the object (through the editor's asset inspector and `setSettings` with its `provenance` argument, rows CT1 (b) and CT22, or a diff). ASSET's example `.meta` of `Zenith2/Docs/Design/ASSET/Formats.md → "Meta sidecar schema"` shows a provenance object without `schema` and `origin`, which V1 and V2 refuse; row CT24 asks ASSET to align its example.

## Licence table

CONTENT's table answers two questions per licence identifier: does an asset under it need an attribution, and does it need its licence text. Whether an identifier may **ship** is not CONTENT's to decide: it is the `allowedContent` list of `Zenith2/ThirdParty/licence_policy.json` (`Zenith2/Docs/Design/SHIP/Manifest.md → "Licence fields"`), and admitting a term to that list is SHIP's `needs-human` licence-compliance ruling, never an edit of this table.

<!-- elaboration -->
| Identifier | Attribution required | Licence text required | Notes |
|---|---|---|---|
| `LicenseRef-Project` | no | no | the project's own content; the default meaning of `origin: authored` and `generated`; never listed for shipping (step 2 of "Content licences step") |
| `CC0-1.0` | no | no | |
| `CC-BY-4.0`, `CC-BY-3.0` | yes | no | |
| `CC-BY-SA-4.0` | yes | no | share-alike terms apply to the asset |
| `OFL-1.1` | yes | no | fonts |
| `MIT`, `BSD-2-Clause`, `BSD-3-Clause`, `Apache-2.0`, `Zlib` | yes | no | |
| `LicenseRef-Commercial-<vendor>` (`<vendor>` `^[A-Za-z0-9-]{1,32}$`) | no | yes | a purchased marketplace licence |
| `CC-BY-NC-*`, `CC-BY-ND-*`, `CC-BY-NC-SA-*`, `CC-BY-NC-ND-*` | — | — | forbidden: `CONTENT_ERROR_LICENCE_FORBIDDEN` (non-commercial or no-derivatives terms are incompatible with shipping and with import conversion) |
| any other identifier | — | — | `CONTENT_ERROR_PROVENANCE_INVALID` (unknown licence) |

A valid identifier that `allowedContent` does not list (in the first policy everything above except `CC0-1.0`, `CC-BY-4.0` and `OFL-1.1`) imports and cooks, and the `content.licences` step warns `CONTENT_COOK_WARNING_LICENCE_NOT_SHIPPABLE` naming the asset and the identifier; SHIP's `<Game>_Licences` packaging step refuses it (`Zenith2/Docs/Design/SHIP/Release.md → "Content licences"`). A new row of this table states only the two requirement columns of a new identifier; it never makes content shippable.

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentValidation.h
/// @contract value Zenith_ContentLicenceInfo
/// Ownership: constexpr data of the licence table; returned by pointer, never freed.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; one row per identifier pattern of "Licence table".
/// @endcontract
struct Zenith_ContentLicenceInfo
{
	const char* const m_szPattern = nullptr; // an identifier, or a pattern ending "-*" or "-<vendor>" as the table writes it
	const bool m_bAttribution = false;
	const bool m_bLicenceText = false;
	const bool m_bForbidden = false;
	const char* const m_szSpdxUrl = nullptr; // "https://spdx.org/licenses/<id>.html"; null for LicenseRef rows
};
// The row matching strIdentifier (exact rows first, then patterns in table order); null for an unknown identifier.
const Zenith_ContentLicenceInfo* Zenith_FindContentLicence(Zenith_StringView strIdentifier);
// Whether strIdentifier is in the allowedContent list of a parsed licence_policy.json (SHIP's schema); false when the list is absent.
bool Zenith_IsContentLicenceShippable(const Zenith_JsonDocument& xLicencePolicy, Zenith_StringView strIdentifier);
```

## Content licences step

`content.licences` (CONTENT's GLOBAL_POST step, registered after SCENE's and before the modules', pending ASSET, row CT4) is a static step instance registered by the one signature of `Zenith2/Docs/Design/ASSET/Cook.md → "Step registration functions"`:

```cpp
// Zenith2/Tools/ZenithCook/Internal/Content/Zenith_ContentCookSteps.h
/// @contract functions Zenith_ContentCookStepRegistration
/// Ownership: Registers the static content.licences step with the caller's registry; keeps no pointer to the services.
/// Thread affinity: MAIN_THREAD, cook services item 6, before Freeze.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's COOK_ERROR_INVALID_STEP (a second registration before Freeze), returned unchanged.
/// Version: since Z2 Stage 4. Tool-internal.
/// @endcontract
Zenith_Result<void, Zenith_CookError> Zenith_RegisterContentCookSteps(Zenith_CookStepRegistry& xRegistry,
	Zenith_CookServices& xServices);
```


| Desc member | Value |
|---|---|
| `m_szID` | `content.licences` |
| `m_uVersion` | 1 |
| `m_ePhase`, `m_eKind` | `COOK_PHASE_GLOBAL_POST`, `ASSET_KIND_NONE` |
| `m_aszRunsAfter` | none (global-post steps run after every ASSET-phase item; `asset.catalogue` and `asset.manifest` always run last) |
| `m_aszOutputPrefixes` | `provenance-licences.json` |
| `m_bOutOfProcess` | false |
| `m_uPlatformMask` | every platform |

`AddKeyInputs`: the step version; the SHA-256 of `Zenith2/ThirdParty/licence_policy.json`, read with `Zenith_CookContext::ReadRepositoryFile("ThirdParty/licence_policy.json", 1 MiB)` (pending ASSET, row CT24: a repository file a GLOBAL_POST step reads, whose hash the driver adds to the step's key); then for each plan origin (pending ASSET, row CT6: `GetPlanOrigins()`, GUID order) the GUID and the SHA-256 of its canonical origin text.

`Cook`:

1. For each plan origin with a non-empty origin object: validate it (`Zenith_ValidateContentProvenanceJson`); an error, a forbidden licence included, is `COOK_ERROR_STEP_FAILED` naming the asset, the `.meta` key path and the rule, collected for every asset before failing (one failing run reports them all).
2. Keep the origins whose `licence` is present and is not `LicenseRef-Project`.
3. For each kept origin whose identifier `Zenith_IsContentLicenceShippable` rejects against the policy read in `AddKeyInputs`: `Report(COOK_DIAGNOSTIC_LEVEL_WARNING, "CONTENT_COOK_WARNING_LICENCE_NOT_SHIPPABLE: <logical path>: <identifier> is not in licence_policy.json allowedContent")` (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook diagnostics"`; a step's own warning is text with its area's code prefix, as GEOM's `GEOM_COOK_WARNING_*` are, never a value of ASSET's `COOK_WARNING_*` list); the cook passes and the origin is still listed, so SHIP's packaging step names it when it refuses.
4. Write `provenance-licences.json` with `WriteFile`: a JSON array sorted by asset GUID of `{ "asset": <guid>, "name": <logical path>, "licence": <identifier>, "licenceText": <the origin's licenceText, else the row's SPDX URL>, "attribution": <the origin's attribution or ""> }` (row SH9's shape), `Zenith_JsonWriter` pretty, LF.
5. Add the report member `contentLicences`: `{ "<identifier>": <count> }` in identifier order.

SHIP's licence inventory reads the file from each slice's cooked tree (`Zenith2/Docs/Design/SHIP/Release.md → "Content licences"`). `Test_ContentLicencesStep_WritesSortedThirdPartyList`, `_ForbiddenLicenceFailsCook`, `_ProjectContentOmitted`, `_NotShippableWarns` (each over a fixture `licence_policy.json` written by the test).

## Storage policy check

`ZenithContent storage-check (--game <Name> | --all) [--json <file>]` enforces the repository asset storage rows ([Z2-N-BUDGET-176], [Z2-N-BUDGET-177], `Zenith2/Docs/Budgets.md → "Package and storage budgets"`). It needs `git` and never needs git-lfs: LFS pointer files are read as git blobs. Its scope is the tracked paths under `Zenith2/Games/<Name>/` for `--game` and every tracked path under `Zenith2/` for `--all` (the CTest case), so engine content, tool fixtures and recorded test fixtures are checked as well as games; the quota rows apply per game either way.

1. Read `Zenith2/Tools/ZenithContent/Internal/Config/storage.config.json` (`$schema` `zenith2.content-storage`, version 1): `lfsThresholdBytes` (Z2-B-CONTENT-001), `quotasGB` (an object mapping each game with a quota row, Z2-B-CONTENT-002 … -009, to its value in GB = 10^9 bytes), and `binaryExtensions` (the list below). The values are written by the Stage 1 per-system budget ticket for CONTENT; while a row is pending its key holds `null` and the corresponding check is skipped with a notice.
2. `git ls-files -s -z -- <scope>` lists tracked files with their blob IDs, and `git check-attr -z --stdin filter` (the paths on standard input) gives each one's `filter` attribute; `git cat-file --batch-check` gives each blob's size; every blob of at most 1,024 bytes is read with `git cat-file --batch`, and one that begins `version https://git-lfs.github.com/spec/v1` is an LFS pointer whose `size <n>` line is the object's size. No git-lfs command runs, so a host without git-lfs checks the repository exactly.
3. A tracked file that is not a pointer is binary when its name ends with an entry of `binaryExtensions` (rule 4) or its first 8,000 bytes contain a NUL byte (git's own heuristic); a binary at or above `lfsThresholdBytes` is `STORAGE_NOT_LFS` (error). A file whose name ends with an entry of `binaryExtensions` is LFS-tracked whatever its size by rules 4 and 5, so the threshold catches binaries with other suffixes.
4. Every entry of `binaryExtensions` must have the rule `*<ext> filter=lfs diff=lfs merge=lfs -text` in `Zenith2/.gitattributes` (`STORAGE_ATTRIBUTES_MISSING`, error). An entry is a file-name suffix beginning with `.`, matched case-insensitively against the end of the file name, so a compound suffix such as `.lightbake.json` is valid.
5. A tracked path whose `filter` attribute is `lfs` but whose blob is not a pointer is `STORAGE_NOT_LFS` (error), whatever its size: it is a file committed before its LFS rule existed, which git-lfs on a host shows as modified and so leaves the tree dirty.
6. A game's storage (MP-STORAGE, [Z2-N-BUDGET-179]: LFS object sizes plus tracked non-pointer blob sizes under `Zenith2/Games/<Name>/`) above its quota is `STORAGE_QUOTA_EXCEEDED` (error); within 90% of it, a warning.
7. Print one line per finding (`<path>: <code>: <message>`) and the per-game totals; `--json` writes `{ games: [{ name, bytes, quotaBytes, findings: [...] }] }`.

| Exit | Meaning |
|---|---|
| 0 | no error finding |
| 1 | at least one error finding |
| 2 | bad command line |
| 3 | git unavailable, or the configuration invalid |

```cpp
// Zenith2/Tools/ZenithContent/Internal/Storage/Zenith_ContentStorageCheck.h (ZenithContent_Objects)
/// @contract value Zenith_ContentStorageCheckValues
/// Ownership: Plain values; the desc's views are borrowed for the call; the report's arrays are the caller's.
/// Thread affinity: IMMUTABLE. Members non-const in Zenith_ContentStorageReport: filled through the out-parameter of
///   the storage check.
/// Invalidation: With the report.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-internal.
/// @endcontract
// Runs `git <args>` in strRepoRoot, writing xStdin to its standard input; the production runner is Zenith_RunGitProcess
// (Internal/Windows/Zenith_ContentProcess_Windows.cpp); a test passes a scripted runner, a recording implementation of this seam.
using Zenith_PfnContentRunGit = Zenith_Result<void, Zenith_ContentError> (*)(void* pUserData, Zenith_Span<const Zenith_StringView> astrArgs,
	Zenith_ByteSpan xStdin, sint32& iExitCodeOut, Zenith_Array<uint8>& auStdoutOut);
struct Zenith_ContentStorageCheckDesc
{
	const Zenith_StringView m_strRepoRoot = {};            // the directory holding Zenith2/
	const Zenith_Span<const Zenith_StringView> m_astrGames = {}; // empty with m_bAll: every directory of Zenith2/Games/
	const bool m_bAll = false;
	const Zenith_ByteSpan m_xConfigJson = {};              // storage.config.json
	const Zenith_ByteSpan m_xGitAttributes = {};           // Zenith2/.gitattributes
	const Zenith_PfnContentRunGit m_pfnRunGit = nullptr;
	void* const m_pGitUserData = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
};
enum Zenith_ContentStorageFindingCode : uint8
{
	STORAGE_FINDING_NOT_LFS = 1, STORAGE_FINDING_ATTRIBUTES_MISSING, STORAGE_FINDING_QUOTA_EXCEEDED, STORAGE_FINDING_QUOTA_WARNING,
	STORAGE_FINDING_ROW_PENDING,
};
struct Zenith_ContentStorageFinding { const Zenith_ContentStorageFindingCode m_eCode = {}; const bool m_bError = false; const Zenith_String m_strPath = {}, m_strMessage = {}; };
struct Zenith_ContentStorageGame { const Zenith_String m_strName = {}; const uint64 m_ulBytes = 0, m_ulQuotaBytes = 0; const bool m_bHasQuota = false; };
struct Zenith_ContentStorageReport
{
	Zenith_Array<Zenith_ContentStorageGame> m_axGames;
	Zenith_Array<Zenith_ContentStorageFinding> m_axFindings;
	uint8 m_uExitCode = 0;                                // the table above
};
// CONTENT_ERROR_GIT (a git command absent or failing: exit 3), CONTENT_ERROR_INVALID_ARGUMENT (an invalid configuration: exit 3),
// CONTENT_ERROR_OUT_OF_MEMORY; findings are report entries, never errors.
Zenith_Result<void, Zenith_ContentError> Zenith_RunContentStorageCheck(const Zenith_ContentStorageCheckDesc& xDesc,
	Zenith_ContentStorageReport& xReport);
```

<!-- elaboration -->
`binaryExtensions` v1: `.glb .fbx .png .jpg .jpeg .tga .bmp .exr .hdr .psd .tif .tiff .ktx2 .dds .wav .flac .ogg .mp3 .aif .aiff .mp4 .webm .ttf .otf .blend .max .mb .hip .spp .sbs .sbsar .zip`.

<!-- assumption: pending WORLD -->
WORLD's bake documents (`*.lightbake.json`, `*.lightpage.json`) are described by WORLD as "LFS above Z2-B-CONTENT-001", a size-conditional rule `.gitattributes` cannot express. CONTENT asks WORLD (row CT31) to choose: either the two suffixes join `binaryExtensions` (always LFS, never merged: `--print-gitattributes` then leaves them out of the merge block), added in the diff of WORLD's integrating ticket, or WORLD drops the LFS clause and the two types stay plain text merged WHOLE_FILE (`Zenith2/Docs/Design/CONTENT/Merge.md → "Merge policies"`). CONTENT's mechanisms support both; until WORLD answers, v1 omits them.
<!-- /assumption -->

The check runs as the CTest case `Test_ContentStorage_RepositoryWithinPolicy` of `Zenith_Integration_ContentPipeline` (the `Test` step on `win-debug`, so every ticket's diff is checked, on any host with git) and in every stage acceptance; `Test_ContentStorage_DetectsUntrackedBinary`, `_DetectsMissingAttributes`, `_DetectsLfsAttributeWithoutPointer`, `_QuotaExceeded` and `_ReadsPointersWithoutGitLfs` run it over a temporary repository built by the test.

Ordering, so that no recorded binary is ever committed before its rule and no Stage 3 evidence waits on a Stage 4 ticket. The LFS block and the check that reads it are two slices:

1. **The LFS attributes block (Stage 3, Z2-R-CONTENT-151).** One ticket writes, in `Zenith2/.gitattributes` (an ordinary, unprotected file; Z2-D-108's `Docs/Board/** -text` line stays), the block between the comment lines `# BEGIN zenith-content lfs` and `# END zenith-content lfs`, holding one line `*<ext> filter=lfs diff=lfs merge=lfs -text` per v1 `binaryExtensions` suffix in the list's order above: 32 lines, a hand-written golden derivable from the list (the first `*.glb filter=lfs diff=lfs merge=lfs -text`, the last `*.zip filter=lfs diff=lfs merge=lfs -text`). It is its own ticket in the Stage 3 plan (`Zenith2/Docs/Design/CONTENT.md → "Stage 3"`), blocked by nothing, and it `blocks` every needs-human recording ticket of `Zenith2/Docs/Design/CONTENT/Tests.md → "Needs-human recordings"` and every other ticket that commits a file of a `binaryExtensions` suffix; so a `.ogg`, `.mp3`, `.jpg` or `.glb` fixture is committed as an LFS object from its first commit, and adding the block never turns a committed file into a modified one. The merge block of `Zenith2/Docs/Design/CONTENT/Merge.md → "Git integration"` is a separate block of the same file, added by the Stage 4 merge-driver ticket. The pointer refusal of `Zenith_LoadRecordedFixture` and its test `Test_MediaDecoders_RecordedFixturePointerFailsClearly` land with the ContentCore parsers slice's media-decoder ticket, the first reader of a recorded file, and are Z2-R-CONTENT-151's test evidence.
2. **The storage-policy check (Stage 4, Z2-R-CONTENT-132, -134).** The Stage 4 storage-policy ticket delivers `ZenithContent storage-check`, whose rule 4 checks the block the Stage 3 ticket wrote (`STORAGE_ATTRIBUTES_MISSING` for a suffix without its rule) and whose rules 3 and 5 report a binary outside LFS. When the check finds a tracked binary outside LFS (a file committed before this design, or rule 5's case), the storage-policy ticket files the conversion: one `needs-human` ticket that runs `git lfs migrate import --no-rewrite <paths>` (a new commit that replaces each file with its pointer, rewriting no history) on a machine with git-lfs.

git-lfs is a prerequisite of every gate host and lane that runs the recorded-fixture tests (`Test_MediaDecoders_ImageFormats`, `…AudioFormats`, `…VorbisWorkBlockGrows` and every `Test_DccMatrix_*` case), from Stage 3 on, because the media-decoder cases are Stage 3 evidence (Z2-R-CONTENT-048, -050, -055): SHIP's setup script installs it and runs `git lfs install` and `git lfs pull` (rows CT16, CT32), before the first gate run of CONTENT's Stage 3 media-decoder ticket. A recorded-fixture test that reads a file beginning `version https://git-lfs.github.com/spec/v1` fails at once with `<path> is a git-lfs pointer, not the recorded file: install git-lfs and run git lfs pull (Setup-Zenith2.ps1 does both)`, rather than decoding pointer text. The storage check itself never needs git-lfs.
