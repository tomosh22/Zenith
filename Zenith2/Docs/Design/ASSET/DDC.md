# ASSET design: the derived-data cache

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It specifies the derived-data cache (DDC): the key, the client `Zenith_DerivedDataCache`, the store interface a shared store implements, the local store, lookup, population and verification, the failure modes and the relationship to evidence fingerprints. The code lives in `Zenith2/Tools/ZenithCook/Internal/DDC/`. CONTENT owns the shared store's backend and its operational requirements (Z2-I-CONTENT-23 … -26); this file owns the mechanism and the client's rules.

## Scope of this file

| Topic | Heading |
|---|---|
| what a key is made of | "Key derivation" |
| the entries an import is cached in | "Import entries" |
| the client and store interfaces | "Zenith_DerivedDataCache API" |
| the on-disk local store | "Local store" |
| the read and write paths | "Lookup populate and verify" |
| unreachable, corrupt, stale | "Failure modes" |
| what a shared store must do | "Shared store contract" |
| several cooks at once | "Concurrency across processes" |

## Key derivation

### Key inputs

A key identifies one derived output. Every input that can change the output bytes is an input of the key; nothing else is:

| Input | Import key | Cook key |
|---|---|---|
| the key kind tag (`"import"` or `"cook"`) and the key schema version (1) | yes | yes |
| `uZENITH_ENGINE_COOK_VERSION` | yes | yes |
| importer ID, `m_uVersion`, `m_uSettingsVersion` | yes | — |
| step ID and `m_uVersion` | — | yes |
| the source bytes' Hash128 | yes | — |
| each declared import input: its Zenith2-relative path and bytes' Hash128, sorted by path (a document opened through `OpenInputDocumentRelativeTo` is one; an absent one has the all-zero Hash128) | yes | — |
| each published-property input (Import.md "Published-property inputs"): rows `propertyAsset` (the producing GUID), `propertyPlatform` (u32, the `Zenith_AssetPlatform` value), `propertyName` (string) and `propertyHash` (the Hash128 of the bytes read, all zero for a failed read), sorted by (GUID bytes, platform, name bytes), after the declared inputs; the producing item's cook key is not an input | yes | — |
| each product-list input (Import.md "Source product lists"): rows `productsSource` (the source GUID) and `productsHash` (the Hash128 of the list's canonical text), sorted by source GUID bytes, after the property rows | yes | — |
| the canonical resolved settings Hash128 (Import.md "Settings resolution"), including the preset's | yes | yes (the platform-resolved form) |
| the platform | — | yes |
| the product GUID and the intermediate's import key | — | yes |
| compression: codec ID, resolved level, dictionary GUID (null when none) and the codec's `m_szLibraryVersion` (`Zenith2/Docs/Design/COMPRESS.md → "Cook integration"`) | — | yes |
| the pinned version string of every tool the step or importer declares (`m_aszToolVersions`: fastgltf, meshoptimizer, KTX-Software, MikkTSpace, Slang, DXC as the manifest pins them) | yes | yes |
| for each cook-input dependency: the dependency's GUID and its key Hash128 (or the Hash128 of the published property read) | — | yes |
| step-specific inputs the step adds in `AddKeyInputs` | — | yes |

The source file's path is not an input (a move re-uses the entry); the host, user, time, worker count and ZenithCook's own build identity are not inputs (so a rebuild of the tool that changes no step version re-uses the cache; a step whose output changes bumps its version).

`Test_DerivedDataKey_EachInputChangesKey` changes each row's input in turn and asserts the key changes, and changes each excluded quantity (path, worker count, time) and asserts it does not.

### Key encoding

```cpp
// Zenith2/Tools/ZenithCook/Internal/DDC/Zenith_DerivedDataKey.h
/// @contract type Zenith_DerivedDataKeyBuilder
/// Ownership: A stack value.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None; adding after Finish is ZENITH_ASSERT.
/// Version: since Z2 Stage 4. The encoding is the key schema: a change bumps the key schema version, which changes every key.
/// @endcontract
class Zenith_DerivedDataKeyBuilder
{
public:
	explicit Zenith_DerivedDataKeyBuilder(Zenith_StringView strKind);   // "import" or "cook"
	void AddU32(Zenith_StringView strName, uint32 uValue);
	void AddU64(Zenith_StringView strName, uint64 ulValue);
	void AddString(Zenith_StringView strName, Zenith_StringView strValue);
	void AddHash(Zenith_StringView strName, const Zenith_Hash128& xHash);
	void AddGUID(Zenith_StringView strName, const Zenith_AssetGUID& xGUID);
	Zenith_DerivedDataKey Finish();
};

/// @contract value Zenith_DerivedDataKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: None.
/// Units: None.
/// Errors: None; parsing the text form is the store's (a malformed name is not an entry).
/// Version: since Z2 Stage 4. Its value changes whenever the key schema version changes.
/// @endcontract
struct Zenith_DerivedDataKey { Zenith_Hash128 m_xHash; };   // text form: 32 lowercase hex digits, high word first; declared in Zenith_AuthoringAsset.h ("Zenith_DerivedDataCache API"), the builder stays here
```

Encoding: a byte stream of records `uint32 LE name length, name bytes, uint8 type tag, uint32 LE value length, value bytes` (u32 and u64 little-endian; strings UTF-8; hashes low word then high word, each little-endian; GUIDs in their 16-byte binary form), in the order the inputs are added: the driver adds the table's rows in table order, then the step's inputs in its own fixed order; the key is `Zenith_Hash128Of` of the stream (D-SEL-HASH (open, Stage 1) fixes the function). The builder also keeps the stream so a verification failure can report which input differs.

## Import entries

An import (Import.md "Import algorithm" steps 2 and 6) is cached as one **import record entry** and one **intermediate entry** per declared product:

| Entry | Key | Producer fields | Payloads, in this order |
|---|---|---|---|
| import record | the import key itself (`Zenith_DerivedDataKey { m_xImportKey }`) | step ID = the importer ID, step version = the importer's `m_uVersion`, the engine cook version, key schema version 1 | `asset.importManifest` (below), `asset.importDiagnostics` (Formats.md "Import diagnostics blob") |
| intermediate | `Zenith_DerivedDataKeyBuilder("import")` with `AddHash("importKey", import key)` then `AddGUID("product", product GUID)` | the same as the import record's | one payload per blob the importer wrote for the product with `WriteIntermediate`, named by the blob name, in byte order of name |

`asset.importManifest` is UTF-8 JSON written by `Zenith_JsonWriter` in compact style, members in this order: `"schema": "zenith2.import-manifest"`, `"schemaVersion": 1`, `"importer"` (ID), `"importerVersion"`, `"settingsVersion"`, `"products"` (objects `{key, guid, kind, primary}`, `kind` the kind's script name, in key byte order), `"inputs"` (objects `{path, hash}`, by path), `"propertyInputs"` (objects `{guid, platform, name, hash, producerKey}`, `platform` `windows` or `android`, by (GUID bytes, platform, name bytes)), `"productListInputs"` (objects `{source, hash}`, by source GUID bytes), `"fingerprints"` (objects `{guid, fingerprint}` for products with one, by GUID bytes), `"derived"` (GUIDs of derived products, ascending), `"reportMembers"` (objects `{member, value}` in call order, `value` the raw JSON), `"references"` (GUIDs, ascending byte order); GUIDs are 36-character text and hashes 32 lowercase hex digits. Limits: 64 MiB, and at most 1,048,576 elements in each array; a manifest that does not parse, names another importer or version, or breaks a limit or an order makes the lookup a miss, and the import driver calls `Quarantine` on the entry ("Corrupt entry", "Verification sampling").

- **Lookup.** `Find` the import record entry; on a verified hit parse the manifest, then `Find` every product's intermediate entry with the same producer. Every entry hit and verified: the import is a hit; the driver writes each payload to the intermediate area `<intermediate root>/<32 hex import key>/<36-character product GUID>/<blob name>` (a temporary then `Zenith_ReplaceHostFileAtomically`; an identical file is left as it is), rewrites `Inputs/<source GUID>.txt` from `inputs` and `propertyInputs`, and calls `UpdateAfterImport` with `m_bFromDDC` true and the diagnostics decoded from `asset.importDiagnostics`; cook steps keep reading the intermediate area (`Zenith_CookContext::ReadIntermediate`). Any miss is a miss of the import, which then runs.
- **Population.** After a successful import that was not deferred: `Populate` every product's intermediate entry, in product key order, then the import record entry, only once every intermediate entry was stored, so a record entry never names an intermediate entry this population failed to store. A `Populate` failure is a warning ("Failure modes"), never a failure of the import. A product with more than 256 blobs, or blobs above 4 GiB together, is not cached (`DDC_ERROR_IO` or `DDC_ERROR_ENTRY_TOO_LARGE`, logged with the importer ID) and the import is re-run by the next cook that needs it.
- **Modes.** `--clean` skips the lookup and still populates; the editor's forced reimport skips the lookup for its sources and keeps the key; a sampled hit (`ShouldVerify` of the import key) runs the import anyway and compares its intermediates, manifest and diagnostics byte for byte with the entry, a difference being the verification failure of "Verification sampling" naming the importer.
- **Worker processes.** An out-of-process import's worker populates the intermediate entries and the import record entry into the local store itself, by these rules; the driver then reads them through the lookup above (Formats.md "Worker messages").

`Test_DerivedDataCache_ImportEntriesRoundTrip` (Tests.md "DDC tests") covers the lookup, the population order and the materialised intermediate area.

## Zenith_DerivedDataCache API

**Home of the store interface (cross-area rows CT8, CT24 (f), AB9).** Under ruling Q84 (a), `Zenith_DerivedDataStoreResult`, `Zenith_DerivedDataStore`, `Zenith_DerivedDataKey` and the entry verifier below are declared in ASSET's ZenithAuthoring root header `Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h` (bodies in `Zenith2/Tools/Authoring/Internal/Asset/`), so CONTENT's file-share store and `ZenithContent ddc verify` reach them without including ZenithCook's `Internal/`; ZenithCook's `Internal/DDC/` headers include that header and keep `Zenith_DerivedDataCache`, the key builder and the local store. The shared store's factory is CONTENT's `Zenith_CreateSharedDerivedDataStore` in `Zenith2/Tools/Authoring/Zenith_AuthoringContent.h`: ZenithCook declares no factory of its own and has no null body to replace, and CONTENT's change that defines the factory also adds its call at services item 7 (Cook.md "Cook services and composition").

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h
/// @contract value Zenith_DerivedDataEntryInfo
/// Ownership: Plain value; m_strStepID is a view into the verified entry bytes.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the entry bytes passed to Zenith_VerifyDerivedDataEntry.
/// Units: Bytes; counts.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; the header fields of Formats.md "DDC entry file".
/// @endcontract
struct Zenith_DerivedDataEntryInfo { const Zenith_StringView m_strStepID = {}; const uint32 m_uStepVersion = 0; const uint32 m_uEngineCookVersion = 0; const uint32 m_uKeySchemaVersion = 0;
	const uint16 m_uPayloadCount = 0; const uint64 m_ulTotalBytes = 0; };
// The checks of "Lookup populate and verify" step 1 without a producer comparison: magic, version, header check, key equal to
// xKey, bounds and each payload's Hash128. DDC_ERROR_IO for any failed check (the first failure logged), never an assert.
Zenith_Result<Zenith_DerivedDataEntryInfo, Zenith_DerivedDataError> Zenith_VerifyDerivedDataEntry(
	Zenith_ByteSpan xEntry, const Zenith_DerivedDataKey& xKey);
```

`Zenith_DerivedDataError` moves with them. A test that needs a stale or corrupt entry builds one with `Zenith2/Tools/Authoring/Tests/Support/Zenith_DerivedDataEntryWriter.h` (header-only, [Z2-N-ARCH-255]): `Zenith_WriteTestDerivedDataEntry(key, producer fields, payloads, Zenith_Array<uint8>& out)` writes a valid entry for any producer fields, so `ddc verify --expect-producers` cases need no recorded file.

### Zenith_DerivedDataStore

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h (rows CT8, AB9; ZenithCook's Internal/DDC/Zenith_DerivedDataCache.h includes it)
/// @contract enum Zenith_DerivedDataStoreResult
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; UNAVAILABLE is a miss for the cook ("Unreachable store").
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
enum Zenith_DerivedDataStoreResult : uint8
{
	DDC_STORE_RESULT_HIT = 1,
	DDC_STORE_RESULT_MISS,
	DDC_STORE_RESULT_UNAVAILABLE,     // the store could not be reached or read this time
};

/// @contract interface Zenith_DerivedDataStore
/// Ownership: Implemented by the local store (ASSET) and a shared store (CONTENT); owned by the cook services.
/// Thread affinity: ANY_THREAD. Get, Put and Remove may run concurrently for different keys and for one key.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Get returns HIT with the entry's bytes, MISS, or UNAVAILABLE; Put returns false when the entry was not stored; a store
///   never returns partial bytes as a HIT.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. The entry bytes are Formats.md "DDC entry file".
/// @endcontract
class Zenith_DerivedDataStore
{
public:
	virtual ~Zenith_DerivedDataStore() = default;
	virtual Zenith_StringView GetName() const = 0;                    // "local", "shared:<url>"
	virtual Zenith_DerivedDataStoreResult Get(const Zenith_DerivedDataKey& xKey, Zenith_Array<uint8>& xEntryOut) = 0;
	// Store a complete, verified entry; must be atomic (a reader sees the old state or the whole new entry) and must not replace
	// an existing entry for the key.
	virtual bool Put(const Zenith_DerivedDataKey& xKey, Zenith_ByteSpan xEntry) = 0;
	virtual void Remove(const Zenith_DerivedDataKey& xKey) = 0;                     // the local store; a shared store ignores it
	virtual bool IsWritable() const = 0;
};
```

### Zenith_DerivedDataCache

```cpp
/// @contract enum Zenith_DerivedDataSharedMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An unknown cook.ddc.sharedMode string is COOK_ERROR_BAD_COMMAND_LINE at start.
/// Version: since Z2 Stage 4. Tool-side; the key's strings are "off", "read", "readWrite".
/// @endcontract
enum Zenith_DerivedDataSharedMode : uint8
{
	DDC_SHARED_MODE_OFF = 0,           // the shared store is not consulted
	DDC_SHARED_MODE_READ,              // lookups only; nothing is Put
	DDC_SHARED_MODE_READ_WRITE,        // lookups and best-effort Put of locally produced entries
};

/// @contract value Zenith_DerivedDataCacheDesc
/// Ownership: Plain value read by Create; strings and pointers borrowed; m_pxShared outlives the cache.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes; percent.
/// Errors: Create returns DDC_ERROR_ROOT_UNUSABLE for an unusable root, a null heap or m_uVerifyPercent above 100.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_DerivedDataCacheDesc
{
	const Zenith_StringView m_strLocalRoot = {};  // cook.ddc.localRoot; default ${ZENITH_OUT}/DDC
	const uint64 m_ulLocalMaxBytes = 0;          // cook.ddc.localMaxBytes (the pending row "Local DDC size")
	const uint64 m_ulLocalFreeSpaceMarginBytes = 0; // cook.ddc.localFreeSpaceMarginBytes; default 2 GiB
	Zenith_DerivedDataStore* const m_pxShared = nullptr; // null when cook.ddc.sharedMode is off
	const Zenith_DerivedDataSharedMode m_eSharedMode = {}; // DDC_SHARED_MODE_OFF, _READ, _READ_WRITE (cook.ddc.sharedMode)
	const uint32 m_uVerifyPercent = 0;           // cook.ddc.verifyPercent, 0 … 100; default 1
	const uint64 m_ulVerifySeed = 0;             // the sampling seed (default 0: sampling is a pure function of the key)
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_DerivedDataCache
/// Ownership: Created by Create and owned by the cook services or the cook server.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_Result<…, Zenith_DerivedDataError>; a store failure is never an error of Find (it is a miss), only of Create.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_DerivedDataCache
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DerivedDataCache>, Zenith_DerivedDataError> Create(
		const Zenith_DerivedDataCacheDesc& xDesc);
	// Local, then shared; a shared hit is copied into the local store. The payloads are verified before return.
	Zenith_DerivedDataLookup Find(const Zenith_DerivedDataKey& xKey, const Zenith_DerivedDataProducer& xExpected,
		Zenith_Array<uint8>& xEntryOut);
	// Build the entry from the payloads, store locally, then (READ_WRITE) best-effort in the shared store.
	Zenith_Result<void, Zenith_DerivedDataError> Populate(const Zenith_DerivedDataKey& xKey,
		const Zenith_DerivedDataProducer& xProducer, Zenith_Span<const Zenith_DerivedDataPayload> axPayloads);
	bool ShouldVerify(const Zenith_DerivedDataKey& xKey) const;       // sampling
	void Quarantine(const Zenith_DerivedDataKey& xKey, Zenith_StringView strReason); // after a failed verification
	Zenith_DerivedDataStats GetStats() const;
};

/// @contract value Zenith_DerivedDataProducer
/// Ownership: Plain value; m_strStepID is borrowed for the call (Find, Populate).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an entry whose recorded producer fields differ from the expected ones is a stale miss ("Stale entry").
/// Version: since Z2 Stage 4. Tool-side; its fields are the DDC entry header's step ID, step version, engine cook version and
///   key schema version (Formats.md "DDC entry file").
/// @endcontract
struct Zenith_DerivedDataProducer { const Zenith_StringView m_strStepID = {}; const uint32 m_uStepVersion = 0; const uint32 m_uEngineCookVersion = 0; const uint32 m_uKeySchemaVersion = 0; };

/// @contract value Zenith_DerivedDataPayload
/// Ownership: Plain view; the name and bytes are borrowed for the Populate call, which copies them into the entry.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None after Populate returns.
/// Units: Bytes.
/// Errors: Populate returns DDC_ERROR_ENTRY_TOO_LARGE when the payloads exceed 4 GiB together, and refuses a name longer than
///   128 bytes or more than 256 payloads as DDC_ERROR_IO (logged with the step ID).
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_DerivedDataPayload { const Zenith_StringView m_strName = {}; const Zenith_ByteSpan m_xBytes = {}; };

/// @contract enum Zenith_DerivedDataLookup
/// Ownership: Plain value returned by Find.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; corrupt, stale and unavailable entries are all DDC_LOOKUP_MISS (counted separately in the stats).
/// Version: since Z2 Stage 4. Tool-side; never serialized.
/// @endcontract
enum Zenith_DerivedDataLookup : uint8 { DDC_LOOKUP_HIT_LOCAL = 1, DDC_LOOKUP_HIT_SHARED, DDC_LOOKUP_MISS };

/// @contract value Zenith_DerivedDataStats
/// Ownership: Plain value snapshot returned by GetStats.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts since Create; bytes (m_ulEvictedBytes cumulative, m_ulLocalBytes the local store's current size).
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side; the cook report's DDC members serialize it.
/// @endcontract
struct Zenith_DerivedDataStats { uint64 m_ulLocalHits, m_ulSharedHits, m_ulMisses, m_ulCorrupt, m_ulStale, m_ulUnavailable,
	m_ulVerified, m_ulVerifyFailures, m_ulEvictedBytes, m_ulLocalBytes; };
```

## Local store

### Zenith_LocalDerivedDataStore

```cpp
// Zenith2/Tools/ZenithCook/Internal/DDC/Zenith_LocalDerivedDataStore.h
/// @contract type Zenith_LocalDerivedDataStore
/// Ownership: Created by Create as a Zenith_Owned; owned by the DDC client (the cook services); owns its index in memory.
/// Thread affinity: ANY_THREAD, as Zenith_DerivedDataStore; eviction serialises across processes through the lock file.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Create returns DDC_ERROR_ROOT_UNUSABLE when the root cannot be created or written; Get, Put and Remove follow the
///   interface (UNAVAILABLE for an I/O failure other than a missing file, which is MISS).
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 4. Tool-side; the layout is "Directory layout".
/// @endcontract
class Zenith_LocalDerivedDataStore final : public Zenith_DerivedDataStore
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_LocalDerivedDataStore>, Zenith_DerivedDataError> Create(
		Zenith_StringView strRoot, uint64 ulMaxBytes, uint64 ulFreeSpaceMarginBytes, Zenith_Heap& xHeap);
	// Zenith_DerivedDataStore overrides (GetName returns "local"), all.
	Zenith_Result<uint64, Zenith_DerivedDataError> EvictToFit(uint64 ulIncomingBytes);   // "Size cap and eviction"
	void Quarantine(const Zenith_DerivedDataKey& xKey);                  // moves the entry under quarantine/ (at most 64 kept)
	uint64 GetStoredBytes() const;
	Zenith_Result<void, Zenith_DerivedDataError> RebuildIndex();                            // a directory walk
};
```

### Directory layout

```text
<local root>/v1/                                   the key schema version
<local root>/v1/<k0k1>/<k2k3>/<key 32 hex>.zddc    one entry per key (k0…k3: the key's first four hex digits)
<local root>/v1/quarantine/<key>.zddc              entries that failed verification, kept for diagnosis (at most 64)
<local root>/v1/index.zddcidx                      the LRU index: key, bytes, last-use sequence (rebuildable by a directory walk)
```

The default root is `${ZENITH_OUT}/DDC`, inside the checkout's output tree ([Z2-N-ARCH-025]); `cook.ddc.localRoot` may point elsewhere on a developer machine, never under `Zenith2/` outside `out/`.

### Atomic population

1. Build the entry bytes (Formats.md "DDC entry file") in memory.
2. Write `<key>.zddc.<process id>.<sequence>.tmp` in the entry's directory, flush it.
3. Rename it to `<key>.zddc` without replace (`MoveFileExW` without `MOVEFILE_REPLACE_EXISTING`); a rename that fails because the entry exists is success (another process populated the same key; outputs are deterministic), and the temporary is deleted.
4. Append the key to the index.

A crash leaves only `*.tmp` files, which the next `Create` deletes when older than 1 hour.

### Size cap and eviction

- When the store exceeds `m_ulLocalMaxBytes`, or the volume's free bytes fall below the margin, `Populate` first evicts least-recently-used entries (by the index's last-use sequence, updated on every hit) until the store is at 90 % of the cap and the margin holds; an entry being read is never evicted (read handles use `FILE_SHARE_DELETE`, so an eviction during a read unlinks after the read).
- When eviction cannot free enough (the margin cannot be met), `Populate` skips the store with `DDC_ERROR_LOW_DISK`, which the cook reports as a warning; the cook proceeds without caching.

## Lookup populate and verify

1. `Find`: `Get` from the local store; on HIT, verify the entry (its magic, version, check, key equal to the requested key, producer fields equal to the expected producer, each payload's Hash128); a verified entry is a hit; a failure is handled per "Failure modes" and falls through.
2. On a local miss and a shared mode other than OFF: `Get` from the shared store with the same verification; a verified shared hit is `Put` into the local store and returned.
3. Otherwise a miss: the caller runs the import or step and calls `Populate`.
4. Sampling: when `ShouldVerify(key)` (the key's low 32 bits modulo 10,000 below `m_uVerifyPercent` × 100, after mixing with the seed), a hit is re-produced locally and compared byte for byte; `--verify` makes every hit sampled.

## Failure modes

The client's one rule: a DDC problem never fails a cook and never damages a store; the output is produced locally.

### Unreachable store

A shared `Get` or `Put` returning UNAVAILABLE (network path absent, timeout of `cook.ddc.sharedTimeoutMs`, default 2,000 ms, access denied) counts `m_ulUnavailable`, logs `COOK_WARNING_DDC_UNAVAILABLE` once per run, and disables the shared store for the rest of the run after 3 consecutive failures; the item cooks locally.

### Corrupt entry

An entry whose bytes fail verification (magic, check, payload hash, truncated file) counts `m_ulCorrupt`; a local entry is removed (`Remove`), a shared entry is left untouched (the client never deletes from a shared store) and reported with its key; the lookup continues as a miss.

### Stale entry

An entry whose key matches but whose producer fields (step ID, step version, engine cook version, key schema version) differ from the expected ones counts `m_ulStale` and is a miss (local stale entries are removed). Such an entry arises only from a key-schema defect, a hash collision, or a producer that forgot to include an input; the report lists them.

### Verification sampling

A sampled hit whose local re-production differs is a verification failure: the entry is quarantined (local) or reported (shared), the locally produced output is used, `m_ulVerifyFailures` increments, and the cook reports `COOK_ERROR_DDC` as a warning naming the step: a non-deterministic step or an undeclared input, which is a defect of that step.

`Test_DerivedDataCache_UnreachableFallsBackLocal`, `_CorruptEntryIsMiss`, `_StaleEntryIsMiss` and `_VerifyFailureQuarantines` run each mode against a fault-injecting store and assert the cook's tree equals a clean cook's and the store's bytes are unchanged (shared) or the bad entry is gone (local).

## Shared store contract

A shared store implementation (CONTENT's backend, Z2-I-CONTENT-23) implements `Zenith_DerivedDataStore` and guarantees:

1. `Get` returns either a complete entry that was once `Put` for that key, a miss, or UNAVAILABLE; never a partial or mixed entry.
2. `Put` is atomic and never replaces an existing entry for the key.
3. `Remove` is a no-op for clients.
4. A store may evict entries at any time (a subsequent `Get` misses).
5. Access control, retention, deployment and monitoring are the backend's; the client needs nothing else.

The client adds: entries are verified on every read, never written partially, and never written unless produced locally in this run or copied from a verified entry.

## Concurrency across processes

- Several cooks (lanes, the cook server and a build) may share a local root: population is create-only rename, readers never see partial files, and the index is advisory (a lost index update only makes eviction less exact; `Create` rebuilds it when its entry count disagrees with a sampled directory walk).
- Eviction takes an exclusive lock file `<root>/v1/.evict.lock` (created with `CREATE_NEW`, deleted after; a lock older than 10 minutes is broken) so two processes do not evict at once; lookups and populations take no lock.

## DDC errors

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAsset.h (moved with the store interface, rows CT8, AB9)
/// @contract enum Zenith_DerivedDataError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; at the cook's boundary each is COOK_ERROR_DDC with this value in the detail.
/// Version: since Z2 Stage 4. Tool-side; values are appended, never renumbered.
/// @endcontract
enum Zenith_DerivedDataError : uint8
{
	DDC_ERROR_ROOT_UNUSABLE = 1,       // Create: the local root cannot be created or written
	DDC_ERROR_LOW_DISK,
	DDC_ERROR_IO,
	DDC_ERROR_ENTRY_TOO_LARGE,         // above 4 GiB
	DDC_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_DerivedDataError eError);
```

## Relationship to evidence fingerprints

- The evidence fingerprint of Z2-D-065 includes the "content and workload version hashes"; the content hash is the catalogue's content hash, whose one definition is the catalogue header's offset-48 field (`Zenith2/Docs/Design/ASSET/Formats.md → "Catalogue header"`): it covers the entry table and the file entry table, which hold every cooked output's key hash and output hash, together with the external reference, fallback and string tables. So a change to any source, setting, importer, step, tool version or engine cook version that changes any output changes the content hash, and invalidates evidence recorded against it; DEVT's fingerprint reads it from `mount-manifest.json`'s `contentHash` or `Zenith_AssetRegistry::GetContentHash`.
- The same content hash is the ZRPL header's content hash and the resume state's (`Zenith2/Docs/Architecture.md → "Multiplayer foundations"`).
