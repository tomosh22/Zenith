# ASSET design: ZenithAssets public API

This file is part of the ASSET design family; the entry document is `Zenith2/Docs/Design/ASSET.md`. It holds every public declaration of `Zenith2/Engine/ZenithAssets/` with the contract block of each public type in the grammar of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`, and the registry internals that the declarations imply. Behaviour over time (the pipeline, back-pressure, cancellation, residency) is `Zenith2/Docs/Design/ASSET/Loading.md`; byte formats are `Zenith2/Docs/Design/ASSET/Formats.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| the header set and include order | "Header list" |
| kinds, priorities, states, errors, constants | "Zenith_AssetTypes header" |
| handles and their reflection hooks | "Zenith_AssetHandle header" |
| the registry | "Zenith_AssetRegistry header" |
| the upward loader interface | "Zenith_AssetKindLoader header" |
| load requests and completions | "Zenith_AssetLoader header" |
| range, section and verified reads | "Zenith_AssetReader header" |
| the catalogue | "Zenith_AssetCatalogue header" |
| the cooked container | "Zenith_CookedFile header" |
| the `.zpak` archive mount source | "Zenith_AssetArchive header" |
| the built-in data and dictionary kinds | "Zenith_BuiltInAssets header" |
| slots, reference counts, retirement, snapshots | "Registry internals" |

## Header list

| Header | Includes (besides allowed standard headers) |
|---|---|
| `Zenith_AssetTypes.h` | `ZenithCore/Zenith_Types.h`, `Zenith_PersistentID.h`, `Zenith_Result.h`, `ZenithCore/Zenith_MemoryGovernor.h` (for `Zenith_MemorySubsystem`) |
| `Zenith_AssetHandle.h` (+ `.inl`) | `Zenith_AssetTypes.h`, `ZenithReflect/Zenith_TypeRegistry.h` (for `Zenith_ValueKindHooks` and `Zenith_PfnConstructFromPersistentID`) |
| `Zenith_CookedFile.h` | `Zenith_AssetTypes.h`, `ZenithCompression/Zenith_CompressedSection.h`, `ZenithCore/Zenith_Hash.h`, `ZenithCore/Zenith_Array.h` |
| `Zenith_AssetCatalogue.h` | `Zenith_AssetTypes.h`, `ZenithCore/Zenith_Hash.h`, `ZenithCore/Zenith_StringView.h`, `ZenithCore/Zenith_Span.h` |
| `Zenith_AssetKindLoader.h` | `Zenith_AssetHandle.h`, `Zenith_CookedFile.h` |
| `Zenith_AssetLoader.h` | `Zenith_AssetHandle.h`, `ZenithCore/Zenith_OperationHandle.h` |
| `Zenith_AssetReader.h` | `Zenith_AssetHandle.h`, `ZenithCore/Zenith_OperationHandle.h`, `ZenithPlatform/Zenith_VirtualFileSystem.h` |
| `Zenith_AssetRegistry.h` (+ `.inl`) | every header above; forward-declares `Zenith_VirtualFileSystem`, `Zenith_AsyncIO`, `Zenith_JobSystem`, `Zenith_CompressionCodecSet`, `Zenith_MemoryGovernor`, `Zenith_TypeRegistry`, `Zenith_ConfigSchema`, `Zenith_JsonWriter`, `Zenith_FaultInjector` by pointer or reference ([Z2-N-CONTRACT-029]) |
| `Zenith_AssetLoadResult.h` | `Zenith_AssetTypes.h`, `ZenithCore/Zenith_Span.h` |
| `Zenith_AssetArchive.h` | `Zenith_AssetTypes.h`, `ZenithPlatform/Zenith_VirtualFileSystem.h` (for `Zenith_MountSource` and `Zenith_FileError`) |
| `Zenith_BuiltInAssets.h` | `Zenith_AssetTypes.h`, `ZenithCore/Zenith_Span.h`, `ZenithCore/Zenith_Owned.h`, `ZenithCompression/Zenith_CompressionDictionary.h` |

Every header is self-contained (`HeaderSelfContained`), `#pragma once`, and names no STL, platform or third-party type.

## Zenith_AssetTypes header

### Asset kinds and the kind table

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetTypes.h
#pragma once
#include "ZenithCore/Zenith_MemoryGovernor.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Types.h"

/// @contract functions Zenith_AssetIdentity
/// Ownership: An alias; no state.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Asset GUIDs are Zenith_PersistentID values (Zenith2/Docs/Contracts.md "Handle kinds",
///   [Z2-N-CONTRACT-066]); their JSON and binary forms are Contracts' ([Z2-N-CONTRACT-067]).
/// @endcontract
using Zenith_AssetGUID = Zenith_PersistentID;

/// @contract enum Zenith_AssetKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a code read from a cooked file or catalogue outside the defined enumerators (or a test code without
///   m_bAllowTestKinds) is refused by the parser with ASSET_ERROR_CATALOGUE_INVALID or ASSET_ERROR_CORRUPT.
/// Version: since Z2 Stage 2. Serialized as uint16 in cooked headers and catalogues; a value keeps its meaning forever.
/// @endcontract
enum Zenith_AssetKind : uint16
{
	ASSET_KIND_NONE = 0,
	ASSET_KIND_TEXTURE = 1, ASSET_KIND_MESH = 2, ASSET_KIND_MATERIAL = 3, ASSET_KIND_TERRAIN = 4,
	ASSET_KIND_SCENE = 5, ASSET_KIND_PREFAB = 6, ASSET_KIND_SKELETON = 7, ASSET_KIND_ANIMATION_CLIP = 8,
	ASSET_KIND_ANIMATION_GRAPH = 9, ASSET_KIND_SOUND = 10, ASSET_KIND_SOUND_BANK = 11, ASSET_KIND_FONT = 12,
	ASSET_KIND_UI_DOCUMENT = 13, ASSET_KIND_CINEMATIC = 14, ASSET_KIND_NAV_MESH = 15, ASSET_KIND_COLLISION_SHAPE = 16,
	ASSET_KIND_COMPRESSION_DICTIONARY = 17, ASSET_KIND_VIRTUAL_TEXTURE = 18, ASSET_KIND_PARTICLE_EFFECT = 19,
	ASSET_KIND_LOCALIZATION_TABLE = 20, ASSET_KIND_VIDEO = 21, ASSET_KIND_DATA = 22,
	ASSET_KIND_TERRAIN_TILE = 23, ASSET_KIND_FOLIAGE_CELL = 24, ASSET_KIND_ATMOSPHERE = 25, ASSET_KIND_WEATHER = 26,
	ASSET_KIND_WATER = 27, ASSET_KIND_ENVIRONMENT_MAP = 28, ASSET_KIND_REFLECTION_PROBE = 29,
	ASSET_KIND_IRRADIANCE_VOLUME = 30, ASSET_KIND_LIGHTMAP_SET = 31, ASSET_KIND_GRADING_LUT = 32, ASSET_KIND_LIGHTING_LUT = 33,
};
constexpr uint16 uASSET_KIND_DEFINED_COUNT = 34;          // codes 0 … 33; the next kind added takes code 34
constexpr uint16 uASSET_KIND_CODE_CAP = 256;              // every defined code is below it; codes 256 … 0x7EFF are never defined
constexpr uint16 uASSET_KIND_TEST_FIRST = 0x7F00;         // test kinds 0x7F00 … 0x7F0F
constexpr uint16 uASSET_KIND_TEST_LAST = 0x7F0F;
static_assert(uASSET_KIND_DEFINED_COUNT <= uASSET_KIND_CODE_CAP);

/// @contract enum Zenith_AssetFallbackPolicy
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized; each kind's value is the kind table's column.
/// @endcontract
enum Zenith_AssetFallbackPolicy : uint8
{
	ASSET_FALLBACK_POLICY_SUBSTITUTE = 1,   // ResolveOrFallback returns the kind's registered fallback asset
	ASSET_FALLBACK_POLICY_NULL_OBJECT,      // ResolveOrFallback returns null; the consumer renders or plays nothing
	ASSET_FALLBACK_POLICY_FAIL,             // the failure is reported to the consumer, which fails its own operation
};

/// @contract value Zenith_AssetKindInfo
/// Ownership: constexpr data in ZenithAssets' kind table; returned by const reference.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The columns are the table "Asset kinds" of Zenith2/Docs/Design/ASSET.md.
/// @endcontract
struct Zenith_AssetKindInfo
{
	const Zenith_AssetKind m_eKind = {};
	const char* const m_szStableName = nullptr; // "Flux.Texture": the TYPE_KIND_ASSET stable name
	const char* const m_szScriptName = nullptr; // "Texture": the name asset<K> and Zenith.Asset<TKind> use
	const char* const m_szRuntimeType = nullptr; // "Flux_TextureResource": the TAsset of Zenith_AssetHandle<TAsset> (diagnostics)
	const char* const m_szExtension = nullptr; // ".ztex"
	const uint32 m_uMagic = 0;               // 'ZTEX' as a little-endian uint32
	const Zenith_MemorySubsystem m_eMemorySubsystem = {};
	const Zenith_AssetFallbackPolicy m_eDefaultFallbackPolicy = {};
	const bool m_bCacheable = false;          // may enter the keep-alive cache at zero references
	const bool m_bGpuResident = false;        // the "GPU resident" column: reloaded by ReloadGpuResidencySet, served by the
	                                          // validate-only loader when headless, DropCpuCopies at CRITICAL, GPU residency tracked
};

/// @contract value Zenith_AssetKindSet
/// Ownership: Plain value, 40 bytes: one bit per kind code below uASSET_KIND_CODE_CAP (m_aulBits) and one bit per test code
///   uASSET_KIND_TEST_FIRST … uASSET_KIND_TEST_LAST (m_uTestBits, bit n for code 0x7F00 + n), padded to 8-byte alignment.
/// Thread affinity: IMMUTABLE once shared; a set is written by one thread at a time. Members non-const: a set is
///   written in place before it is shared.
/// Invalidation: None.
/// Units: None.
/// Errors: None. Add and Remove ignore a code that is neither below uASSET_KIND_CODE_CAP nor a test code (256 … 0x7EFF,
///   0x7F10 … 0xFFFF), and Contains returns false for it; GetCount counts both members' bits.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_AssetKindSet
{
	uint64 m_aulBits[uASSET_KIND_CODE_CAP / 64] = {};
	uint16 m_uTestBits = 0;
	void Add(Zenith_AssetKind eKind);
	void Remove(Zenith_AssetKind eKind);
	bool Contains(Zenith_AssetKind eKind) const;
	uint32 GetCount() const;
	bool IsEmpty() const;
};

/// @contract functions Zenith_AssetKindFunctions
/// Ownership: No state.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_GetAssetKindInfo returns null for a code that is not a defined kind, test codes included: a test kind's
///   row is declared by the test that uses it and read through Zenith_AssetRegistry::GetKindInfo ("Zenith_AssetRegistry");
///   the three Find functions return ASSET_KIND_NONE when unknown and never return a test code.
/// Version: since Z2 Stage 2.
/// @endcontract
const Zenith_AssetKindInfo* Zenith_GetAssetKindInfo(uint16 uKindCode);
Zenith_AssetKind Zenith_FindAssetKindByScriptName(Zenith_StringView strName);
Zenith_AssetKind Zenith_FindAssetKindByStableName(Zenith_StringView strName);
Zenith_AssetKind Zenith_FindAssetKindByTypeID(Zenith_TypeID xType);   // Zenith_Hash64 of the stable name
bool Zenith_IsTestAssetKind(uint16 uKindCode);
Zenith_AssetKindSet Zenith_GetGpuResidentAssetKinds();               // the defined kinds whose column is yes
```

Test kinds. Codes `0x7F00` … `0x7F0F` exist only in tests. Their rows are not part of the closed table: a test declares each test kind it uses as a `Zenith_AssetKindInfo` in `Zenith_AssetRegistryDesc::m_axTestKinds` ("Zenith_AssetRegistryDesc"), which only a registry created with `m_bAllowTestKinds` accepts. The registry answers every column question through `GetKindInfo` (the closed table for a defined code, the declared row for a declared test code, null otherwise) and `GetGpuResidentKinds` (the closed table's GPU-resident kinds plus the declared test kinds whose row says so), so a test kind is cacheable, GPU resident or of any fallback policy exactly as its declared row says. A catalogue carries the format versions of test kinds in its test-kind table (`Zenith2/Docs/Design/ASSET/Formats.md → "Kind table"`), which `Zenith_AssetCatalogue::Load` accepts only when its caller allows test kinds. The standard rows every runtime test declares are `Zenith_FakeKindLoader::GetStandardTestKinds()` (`Zenith2/Docs/Design/ASSET/Tests.md → "Fixtures"`).

The table rows are exactly those of `Zenith2/Docs/Design/ASSET.md → "Asset kinds"` (every column, `m_szRuntimeType` and `m_bGpuResident` included); `Test_AssetTypes_KindTableMatchesDesign` asserts every column of every row against a copy of that table committed as `Tests/Fixtures/KindTable.json`, that stable names, script names, runtime types, extensions and magics are unique, and that the codes are exactly 1 … `uASSET_KIND_DEFINED_COUNT` − 1 with no gap.

Growth of the table:

1. A new kind takes the code `uASSET_KIND_DEFINED_COUNT` (codes are dense and never skipped); ASSET first reserves the code and its row in the entry document ("Asset kinds", "Requested kind rows"; only ASSET edits that table, on the owner's request), then the owner's API change adds the enumerator and the `KindTable.json` row exactly as that row states, increments `uASSET_KIND_DEFINED_COUNT`, and increments `uZENITH_ENGINE_COOK_VERSION`, because the catalogue header's kind count K must equal `uASSET_KIND_DEFINED_COUNT` ("Zenith_AssetCatalogue header"), so every catalogue re-cooks.
2. The diff is made by the kind's owning area and is an API change of ZenithAssets (label `api-change`, ASSET's public headers in the review paths, Z2-D-049) with ASSET as reviewer.
3. No code at or above `uASSET_KIND_CODE_CAP` is ever defined; raising the cap is an ASSET design change that widens `Zenith_AssetKindSet` in the same diff.
4. A code, once defined, keeps its meaning forever; a withdrawn kind keeps its row, marked withdrawn, and its code is never reissued.

### Priorities and states

```cpp
/// @contract enum Zenith_AssetPriority
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value ≥ ASSET_PRIORITY_COUNT passed to any API is ASSET_ERROR_INVALID_REQUEST.
/// Version: since Z2 Stage 2. Never serialized. Maps one to one onto Zenith_IOPriority (same order).
/// @endcontract
enum Zenith_AssetPriority : uint8
{
	ASSET_PRIORITY_CRITICAL = 0,   // blocks visible progress now: the startup scene, a world transition, a fallback
	ASSET_PRIORITY_HIGH,           // needed within a few frames: a cell in view, a mip on screen close by
	ASSET_PRIORITY_NORMAL,
	ASSET_PRIORITY_LOW,
	ASSET_PRIORITY_BACKGROUND,     // prefetch
	ASSET_PRIORITY_COUNT,
};

/// @contract enum Zenith_AssetState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized; the live state, for presentation, tools and tests, never for simulation
///   (Loading.md "Asset state machine").
/// @endcontract
enum Zenith_AssetState : uint8
{
	ASSET_STATE_UNLOADED = 1,      // a slot exists (handles may exist); no load requested, or the last request was cancelled
	ASSET_STATE_QUEUED,            // a load is requested and waits for admission
	ASSET_STATE_LOADING,           // in the pipeline
	ASSET_STATE_ACTIVE,            // the runtime object is published
	ASSET_STATE_FAILED,            // the load failed; GetFailure holds the error; the kind's fallback policy applies
	ASSET_STATE_UNLOADING,         // the kind loader is releasing the runtime object
	ASSET_STATE_CACHED,            // zero references, still ACTIVE underneath, in the keep-alive cache
};

/// @contract enum Zenith_AssetGpuResidency
/// Ownership: Plain value; bits 57–59 of the slot header's residency word ("Slot header layout").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized; the GPU sub-state of an ACTIVE or CACHED asset of a GPU-resident kind
///   (Loading.md "GPU residency sub-state").
/// @endcontract
enum Zenith_AssetGpuResidency : uint8
{
	ASSET_GPU_RESIDENCY_NONE = 0,  // not a GPU-resident kind, a headless run, or no object
	ASSET_GPU_RESIDENCY_PENDING,   // activated with its uploads requested from the kind's owner; the object reports itself not resident
	ASSET_GPU_RESIDENCY_RESIDENT,  // the loader reported every upload complete (ReportGpuResident)
	ASSET_GPU_RESIDENCY_LOST,      // the device was lost; a device-recovery pipeline re-creates the GPU side
	ASSET_GPU_RESIDENCY_FAILED,    // an allocation or upload failed after activation (ReportGpuFailed); the kind's fallback applies
};

/// @contract value Zenith_AssetResidencyTicket
/// Ownership: Plain value naming one ATTEMPT of one slot — one admitted load, reload or device-recovery pipeline, from its admission
///   (or its E12 restart) to its activation or its end — and, once that attempt activates, the activation it installs; holds no
///   reference.
/// Thread affinity: IMMUTABLE. Members non-const: a copy-assignable ID, reassigned where it is held in mutable state, a
///   container or an out-parameter.
/// Invalidation: Issued in Zenith_AssetLoadInfo::m_xResidency at admission and again at every E12 restart (a new attempt, a new
///   value). A value is issued to one attempt only and never to a subsequent attempt of the slot until the slot's 28-bit attempt counter
///   wraps after 2^28 − 1 further attempts; the counter is never reset, not at free and not at reissue. A report naming the attempt
///   that is open but not yet activated is kept and applied at its activation; a report naming the installed activation applies
///   while it is PENDING; any other report (an attempt that ended without activating, an activation a subsequent one replaced, a second
///   report) is stale and ignored ("Slot header layout").
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetResidencyTicket
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;     // diagnostics only: the comparison that decides is the attempt value's
	uint32 m_uAttempt = 0;        // bits 0–27: the attempt value, 1 … 2^28 − 1 (0 names no attempt and matches nothing); bits 28–31 zero
};

/// @contract enum Zenith_AssetPlatform
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A value other than the enumerators, read from a file, is refused by its parser.
/// Version: since Z2 Stage 2. Serialized as uint16 in cooked headers and catalogues; a value keeps its meaning forever.
/// @endcontract
enum Zenith_AssetPlatform : uint8
{
	ASSET_PLATFORM_WINDOWS = 0,
	ASSET_PLATFORM_ANDROID = 1,
	ASSET_PLATFORM_LINUX = 2,      // the headless test host; the cook writes it only for test fixtures
};
```

### Errors

```cpp
/// @contract enum Zenith_AssetErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. When each enumerator is returned: the table below this block.
/// Version: since Z2 Stage 2. An enumerator shipped in a fix version keeps its meaning ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_AssetErrorCode : uint8
{
	ASSET_ERROR_NULL_HANDLE = 1,
	ASSET_ERROR_STALE_HANDLE,
	ASSET_ERROR_FOREIGN_HANDLE,
	ASSET_ERROR_KIND_MISMATCH,
	ASSET_ERROR_NOT_FOUND,
	ASSET_ERROR_NOT_INSTALLED,
	ASSET_ERROR_IO,
	ASSET_ERROR_CORRUPT,
	ASSET_ERROR_VERSION_MISMATCH,
	ASSET_ERROR_PLATFORM_MISMATCH,
	ASSET_ERROR_NO_LOADER,
	ASSET_ERROR_LOADER_FAILED,
	ASSET_ERROR_DEPENDENCY_FAILED,
	ASSET_ERROR_NOT_ACTIVE,
	ASSET_ERROR_NO_RUNTIME_OBJECT,
	ASSET_ERROR_QUEUE_FULL,
	ASSET_ERROR_REGISTRY_FULL,
	ASSET_ERROR_OUT_OF_MEMORY,
	ASSET_ERROR_INVALID_REQUEST,
	ASSET_ERROR_SHUTTING_DOWN,
	ASSET_ERROR_CATALOGUE_INVALID,
	ASSET_ERROR_LIMIT_EXCEEDED,
	ASSET_ERROR_KIND_ALREADY_REGISTERED,
	ASSET_ERROR_REGISTRATION_CLOSED,
	ASSET_ERROR_NOT_ALLOWED,
	ASSET_ERROR_COOK_IDENTITY_MISMATCH,
	ASSET_ERROR_TIMEOUT,
	ASSET_ERROR_NO_FALLBACK,
};

/// @contract enum Zenith_AssetCorruption
/// Ownership: Plain value; the detail of ASSET_ERROR_CORRUPT.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. Each value names the check of Formats.md "Checks" (or the verified-file hash) that failed.
/// Version: since Z2 Stage 2. A value keeps its meaning forever ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_AssetCorruption : uint16
{
	ASSET_CORRUPTION_BAD_MAGIC = 1,
	ASSET_CORRUPTION_BAD_HEADER_CHECK,
	ASSET_CORRUPTION_BAD_FILE_SIZE,
	ASSET_CORRUPTION_BAD_SECTION_TABLE,
	ASSET_CORRUPTION_BAD_SECTION_BOUNDS,
	ASSET_CORRUPTION_BAD_SECTION_CHECK,
	ASSET_CORRUPTION_BAD_DESCRIPTOR,
	ASSET_CORRUPTION_DECODE_FAILED,
	ASSET_CORRUPTION_GUID_MISMATCH,
	ASSET_CORRUPTION_KIND_MISMATCH,
	ASSET_CORRUPTION_CONTENT_HASH_MISMATCH,
	ASSET_CORRUPTION_BAD_RESERVED_FIELD,
	ASSET_CORRUPTION_FILE_HASH_MISMATCH,
};

/// @contract value Zenith_AssetError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480] (24 bytes).
/// Thread affinity: IMMUTABLE. Members non-const: an error record is copy-assigned into the first-error and last-error
///   members of mutable state.
/// Invalidation: None.
/// Units: m_uSectionIndex is an index into the cooked file's section table; 0xFFFFFFFF when not applicable.
/// Errors: m_uDetail holds, by m_eCode: ASSET_ERROR_IO a Zenith_IOErrorCode; ASSET_ERROR_CORRUPT a
///   Zenith_AssetCorruption, or with ASSET_CORRUPTION_DECODE_FAILED the Zenith_CompressionError in its high byte;
///   ASSET_ERROR_LOADER_FAILED the loader's own code; otherwise 0. The text is logged at the point of failure
///   ([Z2-N-CONTRACT-464]).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetError
{
	Zenith_AssetErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uDetail = 0;
	uint32 m_uSectionIndex = 0xFFFFFFFF;
	Zenith_AssetGUID m_xAsset;
};
static_assert(sizeof(Zenith_AssetError) == 24);

constexpr const char* Zenith_ToString(Zenith_AssetErrorCode eError);
constexpr const char* Zenith_ToString(Zenith_AssetCorruption eCorruption);
constexpr const char* Zenith_ToString(Zenith_AssetState eState);
constexpr const char* Zenith_ToString(Zenith_AssetPriority ePriority);
```

| Enumerator | Returned when |
|---|---|
| `NULL_HANDLE`, `STALE_HANDLE`, `FOREIGN_HANDLE` | resolving a null handle; a handle whose generation no longer matches its slot; a handle of another registry or a slot index outside the table |
| `KIND_MISMATCH` | `Acquire<T>` of a GUID whose catalogue kind differs from `T::eASSET_KIND`; `FromAny` to the wrong type |
| `NOT_FOUND` | the GUID or logical path is absent from the merged catalogue |
| `NOT_INSTALLED` | the catalogue lists the asset but its cooked file does not resolve (`PATH_ERROR_NOT_FOUND`), for example an undelivered asset pack |
| `IO` | an I/O operation failed (the I/O code in the detail) |
| `CORRUPT` | a check of Formats.md "Checks" failed, or decoding failed |
| `VERSION_MISMATCH` | the header's format version differs from the registered loader's, or its engine cook version from `uZENITH_ENGINE_COOK_VERSION`; `RegisterKindLoader` of a loader whose format version differs from a non-zero element of the catalogue's kind table |
| `PLATFORM_MISMATCH` | the header's platform differs from the registry's |
| `NO_LOADER` | `CloseRegistration`, for every pipeline still waiting in WAIT_LOADER for its kind's loader (event E19); afterwards, a load of a kind no loader serves (headless GPU kinds excepted) |
| `LOADER_FAILED` | the kind loader called `Fail` |
| `DEPENDENCY_FAILED` | a hard dependency failed with policy `FAIL` or `NULL_OBJECT`; `m_uDetail` holds the root cause, the `Zenith_AssetErrorCode` of the first failure in the dependency chain that is not itself `DEPENDENCY_FAILED` (a dependency failed for `OUT_OF_MEMORY` gives detail `ASSET_ERROR_OUT_OF_MEMORY` however deep it is), and `m_xAsset` stays the failing asset itself (cross-area row ST22) |
| `NOT_ACTIVE` | `Resolve` of an asset that is not ACTIVE or CACHED; a range or section read of such an asset |
| `NO_RUNTIME_OBJECT` | `Resolve` of an ACTIVE asset activated with a null object (headless validate-only loads) |
| `QUEUE_FULL` | the request intake queue, a completion queue or an operation table is full |
| `REGISTRY_FULL` | no free slot below `asset.registry.maxAssets` |
| `OUT_OF_MEMORY` | an allocation failed |
| `INVALID_REQUEST` | an out-of-range priority, a null handle in a request, an offset overflow, a section index outside the table, a destination of the wrong size |
| `SHUTTING_DOWN` | after `BeginShutdown` |
| `CATALOGUE_INVALID` | a catalogue or fragment fails its checks or limits, or two fragments of one precedence disagree |
| `LIMIT_EXCEEDED` | a parser limit of Formats.md "Limits", or more than 8 section rounds in one load |
| `KIND_ALREADY_REGISTERED` | a second loader for a kind |
| `REGISTRATION_CLOSED` | `RegisterKindLoader` after `CloseRegistration` |
| `NOT_ALLOWED` | a development-only call (`RequestReload`) in a Shipping build, or a test kind without `m_bAllowTestKinds` or not declared in `m_axTestKinds` |
| `COOK_IDENTITY_MISMATCH` | the base catalogue's engine cook version differs from `uZENITH_ENGINE_COOK_VERSION` or its kind count K from `uASSET_KIND_DEFINED_COUNT`; `Create` fails and startup exits with code 3 (per-kind format versions are checked at `RegisterKindLoader`) |
| `TIMEOUT` | `ServiceUntilComplete` reached its timeout with the operation still running (the operation continues) |
| `NO_FALLBACK` | `RegisterKindLoader` of a SUBSTITUTE kind whose merged catalogue has no role-0 row in its fallback table |

### Constants and fault points

```cpp
/// @contract functions Zenith_AssetConstants
/// Ownership: constexpr data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes and counts.
/// Errors: None.
/// Version: since Z2 Stage 2. uZENITH_ENGINE_COOK_VERSION and the format limits are part of every cooked format;
///   changing them is a re-cook of all content ([Z2-N-CONTRACT-529]).
/// @endcontract
constexpr uint32 uZENITH_ENGINE_COOK_VERSION = 1;               // bumped by any change that alters cooked bytes for unchanged inputs
constexpr uint32 uASSET_COOKED_HEADER_BYTES = 64;
constexpr uint32 uASSET_COOKED_SECTION_ENTRY_BYTES = 40;
constexpr uint32 uASSET_MAX_SECTIONS = 4096;
constexpr uint64 ulASSET_MAX_COOKED_FILE_BYTES = 17179869184ull; // 16 GiB
constexpr uint32 uASSET_SECTION_ALIGNMENT = 16;
constexpr uint32 uASSET_MAX_SECTION_ROUNDS = 8;
constexpr uint32 uASSET_MAX_HARD_DEPENDENCIES = 4096;          // per asset
constexpr uint32 uASSET_MAX_CATALOGUE_ENTRIES = 1048576;
constexpr uint64 ulASSET_MAX_CATALOGUE_BYTES = 536870912ull;    // 512 MiB
constexpr uint32 uASSET_MAX_CATALOGUE_FRAGMENTS = 256;
constexpr uint32 uASSET_MAX_CATALOGUE_DEPENDENCIES = 16777216;
constexpr uint32 uASSET_MAX_CATALOGUE_STRING_BYTES = 268435456; // 256 MiB
constexpr uint32 uASSET_MAX_EXTERNAL_REFERENCES = 1048576;      // per fragment
constexpr uint32 uASSET_MAX_FALLBACK_ROWS = 4096;
constexpr uint32 uASSET_MAX_ARCHIVE_ENTRIES = 1048576;
constexpr uint32 uASSET_MAX_ARCHIVE_STRING_BYTES = 134217728;   // 128 MiB
constexpr uint64 ulASSET_MAX_ARCHIVE_BYTES = 68719476736ull;    // 64 GiB
constexpr uint32 uASSET_MISSING_LOG_CAPACITY = 1024;            // GUIDs remembered for the once-per-GUID asset.missing record
constexpr uint32 uASSET_DECODE_RESERVE_DIVISOR = 4;             // ceil(D/4) units only CRITICAL and HIGH may use

constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_SLOTS = 0x41530001;        // 'AS'
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_OPERATIONS = 0x41530002;
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_QUEUES = 0x41530003;
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_CATALOGUE = 0x41530004;
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_STAGING = 0x41530005;
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_CONSUMER = 0x41530006;
constexpr uint32 uFAULT_POINT_ASSET_REGISTRY_MISSING_SET = 0x41530007;
constexpr uint32 uFAULT_POINT_ASSET_ARCHIVE_TABLE = 0x41530020;          // the archive reader's table allocation
constexpr uint32 uFAULT_POINT_ASSET_ARCHIVE_SOURCE = 0x41530021;         // the archive source object
constexpr uint32 uFAULT_POINT_ASSET_LOAD_RECORD = 0x41530010;           // a load record allocation during a load
constexpr uint32 uFAULT_POINT_ASSET_SECTION_STAGING = 0x41530011;       // staging for one section
```

## Zenith_AssetHandle header

### Zenith_AnyAssetHandle

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetHandle.h
class Zenith_AssetRegistry;

/// @contract value Zenith_AnyAssetHandle
/// Ownership: A shared reference to one registry slot: a non-null handle holds one reference counted by the registry
///   ([Z2-N-CONTRACT-082]). Copy adds a reference (relaxed atomic increment); destruction and reassignment drop one
///   (acq_rel decrement; the last drop queues the slot for retirement); move transfers it.
/// Thread affinity: ANY_THREAD. Each handle object is used by one thread at a time; distinct copies may be used concurrently.
/// Invalidation: A non-null handle keeps its slot alive, so it never becomes stale through normal use; resolution still
///   compares the generation in every configuration ([Z2-N-CONTRACT-062]) and a byte-copied (uncounted) handle is caught.
/// Units: None.
/// Errors: Accessors do not fail; GetPersistentID of a null handle returns the null ID and GetKind ASSET_KIND_NONE.
/// Version: since Z2 Stage 2. Never serialized as bytes: serialized as its GUID ([Z2-N-CONTRACT-086]).
/// @endcontract
class Zenith_AnyAssetHandle
{
public:
	constexpr Zenith_AnyAssetHandle() = default;
	Zenith_AnyAssetHandle(const Zenith_AnyAssetHandle& xOther);
	Zenith_AnyAssetHandle& operator=(const Zenith_AnyAssetHandle& xOther);
	Zenith_AnyAssetHandle(Zenith_AnyAssetHandle&& xOther);             // xOther becomes null
	Zenith_AnyAssetHandle& operator=(Zenith_AnyAssetHandle&& xOther);  // drops the current reference first
	~Zenith_AnyAssetHandle();
	bool IsNull() const;
	Zenith_AssetGUID GetPersistentID() const;
	Zenith_AssetKind GetKind() const;
	Zenith_AssetRegistry* GetRegistry() const;
	void Reset();
	friend bool operator==(const Zenith_AnyAssetHandle& xA, const Zenith_AnyAssetHandle& xB);
private:
	friend class Zenith_AssetRegistry;
	template<typename TAsset> friend class Zenith_AssetHandle;
	Zenith_AssetRegistry* m_pxRegistry = nullptr;
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
};
static_assert(sizeof(Zenith_AnyAssetHandle) == 16);
```

Equality compares registry, slot and generation; two handles to one asset are equal.

### Zenith_AssetHandle

```cpp
/// @contract functions Zenith_AssetTypeConcept
/// Ownership: None.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A type used as TAsset without a static constexpr Zenith_AssetKind eASSET_KIND fails to compile.
/// Version: since Z2 Stage 2.
/// @endcontract
template<typename TAsset>
concept Zenith_AssetType = std::is_same_v<std::remove_cv_t<decltype(TAsset::eASSET_KIND)>, Zenith_AssetKind>;

/// @contract value Zenith_AssetHandle
/// Ownership: As Zenith_AnyAssetHandle, typed: TAsset is the kind's runtime object type (Flux_MeshResource,
///   Zenith_Scene, …), which declares static constexpr Zenith_AssetKind eASSET_KIND.
/// Thread affinity: ANY_THREAD, as Zenith_AnyAssetHandle.
/// Invalidation: As Zenith_AnyAssetHandle.
/// Units: None.
/// Errors: FromAny returns ASSET_ERROR_KIND_MISMATCH for a handle of another kind; ASSET_ERROR_NULL_HANDLE is never
///   returned by FromAny (a null input gives a null output).
/// Version: since Z2 Stage 2. The layout is Zenith2/Docs/Contracts.md "Handle kinds": exactly 16 bytes,
///   { Zenith_AssetRegistry* m_pxRegistry; uint32 m_uSlot; uint32 m_uGeneration; }.
/// @endcontract
template<typename TAsset>
class Zenith_AssetHandle
{
	static_assert(Zenith_AssetType<TAsset>);
public:
	constexpr Zenith_AssetHandle() = default;
	Zenith_AssetHandle(const Zenith_AssetHandle& xOther);
	Zenith_AssetHandle& operator=(const Zenith_AssetHandle& xOther);
	Zenith_AssetHandle(Zenith_AssetHandle&& xOther);
	Zenith_AssetHandle& operator=(Zenith_AssetHandle&& xOther);
	~Zenith_AssetHandle();
	bool IsNull() const;
	Zenith_PersistentID GetPersistentID() const;
	Zenith_AnyAssetHandle ToAny() const;
	static Zenith_Result<Zenith_AssetHandle, Zenith_AssetError> FromAny(const Zenith_AnyAssetHandle& xAny);
	void Reset();
private:
	friend class Zenith_AssetRegistry;
	Zenith_AssetRegistry* m_pxRegistry = nullptr;
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
};
```

The member templates are defined in `Zenith_AssetHandle.inl`, which calls the out-of-line functions `Zenith_AssetHandleAddRef(Zenith_AssetRegistry*, uint32 uSlot)` and `Zenith_AssetHandleRelease(Zenith_AssetRegistry*, uint32 uSlot)` declared in the header ("Reference counting and retirement").

### Reflection value hooks

```cpp
/// @contract functions Zenith_AssetHandleValueKindHooks
/// Ownership: Hooks over raw 16-byte handle storage in reflected stores (ECS columns, shadows, snapshots, save buffers,
///   construction default objects); the returned struct is a plain value the type registry copies.
/// Thread affinity: ANY_THREAD for Zenith_GetAssetHandleValueKindHooks and the lifecycle hooks it returns;
///   Zenith_ResolveAssetReferenceDefault MAIN_THREAD, called only by Zenith_TypeRegistry::ResolveReferenceDefaults and
///   EndHotReload (Zenith2/Docs/Design/ECS/Reflect.md "Value-kind hooks").
/// Invalidation: None.
/// Units: One 16-byte element per call.
/// Errors: The lifecycle hooks cannot fail. Zenith_ResolveAssetReferenceDefault returns false and leaves the element null when
///   the persistent ID names no catalogue asset of the kind (an asset.missing record is logged once per GUID).
/// Version: since Z2 Stage 2. The signatures are ECS's Zenith_ValueKindHooks and Zenith_PfnConstructFromPersistentID.
/// @endcontract
Zenith_ValueKindHooks Zenith_GetAssetHandleValueKindHooks();
bool Zenith_ResolveAssetReferenceDefault(void* pElement, const uint8* pPersistent, Zenith_TypeID xKind,
	void* pUserData);     // a Zenith_PfnConstructFromPersistentID; pUserData is the Zenith_AssetRegistry*
```

`Zenith_GetAssetHandleValueKindHooks` returns the struct of `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeRegistry"`:

| Member | Value |
|---|---|
| `m_pfnConstructNull` | zero the 16 bytes (the null handle) |
| `m_pfnDestroy` | `Zenith_AssetHandleRelease` when the element is non-null, then zero it |
| `m_pfnCopy` | copy the 16 bytes, then `Zenith_AssetHandleAddRef` when non-null |
| `m_uElementBytes` | 16 |
| `m_pfnConstructFromPersistentID` | `nullptr` (the registry binds the resolver through `ResolveReferenceDefaults`) |
| `m_pUserData` | `nullptr` |

- Move needs no hook: the schema hooks relocate component and struct values bytewise (`Zenith2/Docs/Design/ECS/Reflect.md → "Schema hooks"`), and a handle is trivially relocatable (no self-reference; the moved-from bytes are abandoned, so no count changes).
- ZenithAssets supplies no `Zenith_TypeLifecycleHooks`: a C++-registered type with a `Zenith_AssetHandle<T>` member gets its hooks from `Zenith_MakeLifecycleHooks<T>` (`Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectValueTraits header"`), which call the handle's own copy and move constructors; schema types call the value-kind hooks above per `asset<K>` field.
- `Zenith_ResolveAssetReferenceDefault(pElement, pPersistent, xKind, pUserData)`: (1) `pUserData` is the `Zenith_AssetRegistry*`; (2) decode the 16 bytes as a `Zenith_PersistentID` binary form; (3) `eKind = Zenith_FindAssetKindByTypeID(xKind)`, where `xKind` is the `TYPE_KIND_ASSET` type ID of the field's `K`; `ASSET_KIND_NONE` returns false; (4) `xHandle = AcquireForDeserialization(guid, eKind)`; a null handle returns false; (5) move the handle's 16 bytes into `pElement` (the reference transfers to the element, which the type registry's construction default object then owns) and return true.
- Installation (`Zenith2/Docs/Design/ECS/Reflect.md → "Value-kind hooks"`): the composition root calls `InstallValueKindHooks(PROPERTY_TYPE_ASSET, Zenith_GetAssetHandleValueKindHooks())` in startup step 10 before `RegisterSchemaBlob`, and `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, &Zenith_ResolveAssetReferenceDefault, pxRegistry)` in step 11 directly after `Zenith_AssetRegistry::Create` and before any world exists; a failure is exit code 3 (`Zenith2/Docs/Design/ECS/Schemas.md → "Default values"`). Every store therefore adjusts reference counts ([Z2-N-CONTRACT-087], [Z2-N-ARCH-645]).
- The cook installs the same hooks into its scratch type registry (its handles are all null, so the hooks never touch a registry) and resolves defaults with the cook-side resolver `Zenith_ResolveCookAssetReferenceDefault` (`Zenith2/Docs/Design/ASSET/Database.md → "Reference defaults in the cook"`).

<!-- assumption: pending ARCH (X110, X147), MOD -->
The two composition-root calls are core-doc items for `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`: the step-10 installation and the step-11 `Create` are X110 (pending), and the step-11 `ResolveReferenceDefaults(PROPERTY_TYPE_ASSET, …)` with exit code 3 on an unresolvable default is X147 (4) (pending); cross-area rows E13 and AS12 carry them for MOD's design.
<!-- /assumption -->

## Zenith_AssetRegistry header

### Zenith_AssetRegistryDesc

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetRegistry.h
/// @contract value Zenith_AssetRegistryDesc
/// Ownership: Plain value read by Create; every pointer is borrowed and must outlive the registry.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts and bytes.
/// Errors: Create validates it (ASSET_ERROR_INVALID_REQUEST naming the field in the log).
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
struct Zenith_AssetRegistryDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;
	Zenith_VirtualFileSystem* const m_pxFiles = nullptr;
	Zenith_AsyncIO* const m_pxIO = nullptr;
	Zenith_JobSystem* const m_pxJobs = nullptr;
	const Zenith_CompressionCodecSet* const m_pxCodecs = nullptr;
	Zenith_MemoryGovernor* const m_pxGovernor = nullptr;      // transition peaks; may be null in tools and tests
	const Zenith_AssetPlatform m_ePlatform = ASSET_PLATFORM_WINDOWS;
	const uint32 m_uMaxAssets = 0;                          // asset.registry.maxAssets, 1 … 2^24
	const Zenith_AssetLoaderDesc m_xLoader = {};             // the loader's budgets (Zenith_AssetLoader.h)
	const Zenith_StringView m_strCataloguePath = "content:/assets.zcat";
	const Zenith_StringView m_strFragmentDirectory = "content:/Catalogues";
	const bool m_bHeadless = false;
	const bool m_bAllowTestKinds = false;                    // tests only; Create refuses it in a Shipping build (NOT_ALLOWED)
	// Tests only: one row per test kind the test uses (codes 0x7F00 … 0x7F0F, unique, magic 'ZTST', extension ".ztest",
	// a non-null stable name, a valid fallback policy); non-empty requires m_bAllowTestKinds. Copied by Create; the rows'
	// strings are borrowed and outlive the registry.
	const Zenith_Span<const Zenith_AssetKindInfo> m_axTestKinds = {};
	const bool m_bDevelopmentFeatures = true;                // false in Shipping (Zenith_IsShippingBuild())
	Zenith_FaultInjector* const m_pxFaultInjector = nullptr; // tests; nullptr in production
};
```

### Zenith_AssetRegistry

```cpp
/// @contract type Zenith_AssetRegistry
/// Ownership: Created by Create at startup step 11 and owned by Zenith_Runtime through Zenith_Owned; destroyed at shutdown
///   step 10, after every world, module and the RHI device, and before ZenithRuntime ([Z2-N-CONTRACT-085]).
///   Tools and tests own their own instance. Owns the slot array, the catalogue snapshots, the loader, the reader,
///   the operation tables and every queue ("Internal queues" in Loading.md: the request intake queue
///   QUEUE_FULL_POLICY_REJECT; the retire, loader-acknowledgement, decode-completion and I/O-completion queues
///   QUEUE_FULL_POLICY_FATAL with capacities derived so they never fill).
/// Thread affinity: ANY_THREAD_EXCEPT(Create, RegisterKindLoader, CloseRegistration, GetPinnedLoads, UnregisterKindLoader,
///   OnMountsChanged, BeginShutdown, destructor). Those are MAIN_THREAD (a tool's or test's owning thread).
/// Invalidation: A const T* from Resolve stays valid while the caller holds a handle to the asset and until the next
///   frame boundary (AdvanceFrame), whichever ends first; code keeps handles, never object pointers, across frames.
/// Units: Bytes; frame indices; GetAvailabilityEpoch is a count of publications (0 until the first, never reset).
/// Errors: Zenith_Result<…, Zenith_AssetError>; see "Errors". Destroying the registry with a load in flight is ZENITH_CHECK;
///   destroying it with slots still referenced logs each (GUID, logical path, count) as a leak ([Z2-N-CONTRACT-085]).
/// Reentrancy: NONE. A kind loader called by the registry may call Acquire, AcquireAny, Resolve, GetKindInfo, GetPinnedAsset,
///   ReportGpuResident, ReportGpuFailed, the loader's RequestLoad and its own context, and no other registry member
///   (Zenith_ReentrancyGuard); Zenith_AssetKindLoader's Reentrancy field is the same list.
/// Version: since Z2 Stage 2. Source API. GetAvailabilityEpoch: ANY_THREAD, an acquire load, lock-free (cross-area row RP33).
/// Locks: uLOCK_LEVEL_ASSET_REGISTRY (200) and uLOCK_LEVEL_STREAMING_REQUESTS (250), both short-hold.
/// Example: Zenith2/Engine/ZenithAssets/Tests/Examples/Zenith_AssetLoader_Example.cpp
/// @endcontract
struct Zenith_AssetRegistryConstructTag;   // defined only in Internal/Registry/Zenith_AssetRegistry.h: only Create can construct
struct Zenith_AssetRegistryState;          // defined only in Internal/Registry/Zenith_AssetRegistry.h

class Zenith_AssetRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AssetRegistry>, Zenith_AssetError> Create(
		const Zenith_AssetRegistryDesc& xDesc);
	Zenith_AssetRegistry(const Zenith_AssetRegistryConstructTag& xTag, const Zenith_AssetRegistryDesc& xDesc);   // infallible; Create runs the steps
	~Zenith_AssetRegistry();

	// Kind loaders (MAIN_THREAD, before CloseRegistration).
	Zenith_Result<void, Zenith_AssetError> RegisterKindLoader(Zenith_AssetKind eKind,
		Zenith_AssetKindLoader& xLoader, const Zenith_AssetKindLoaderDesc& xDesc);
	// Fails every pipeline still waiting for an unregistered kind's loader with ASSET_ERROR_NO_LOADER (event E19).
	void CloseRegistration();
	bool IsRegistrationClosed() const;
	Zenith_AssetKindSet GetServedKinds() const;
	// The row of a defined kind, or of a test kind declared in m_axTestKinds; null otherwise (ANY_THREAD).
	const Zenith_AssetKindInfo* GetKindInfo(uint16 uKindCode) const;
	// Zenith_GetGpuResidentAssetKinds() plus every declared test kind whose row has m_bGpuResident (ANY_THREAD).
	Zenith_AssetKindSet GetGpuResidentKinds() const;
	// One operation that completes when every pinned engine asset requested so far (every fallback-table row of every
	// registered kind) is ACTIVE, or fails with the first pinned failure (MAIN_THREAD, after CloseRegistration).
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> GetPinnedLoads();
	// Shutdown step 6, before the loader's owner is destroyed: unloads every ACTIVE and CACHED asset of the kind through
	// the loader, services until each is acknowledged, then leaves the kind unserved.
	void UnregisterKindLoader(Zenith_AssetKind eKind);

	// Acquisition (ANY_THREAD). Acquire never starts a load.
	template<typename TAsset> requires Zenith_AssetType<TAsset>
	Zenith_Result<Zenith_AssetHandle<TAsset>, Zenith_AssetError> Acquire(const Zenith_AssetGUID& xGUID);
	template<typename TAsset> requires Zenith_AssetType<TAsset>
	Zenith_Result<Zenith_AssetHandle<TAsset>, Zenith_AssetError> AcquireByLogicalPath(Zenith_StringView strPath);
	Zenith_Result<Zenith_AnyAssetHandle, Zenith_AssetError> AcquireAny(const Zenith_AssetGUID& xGUID,
		Zenith_AssetKind eExpectedKind);            // ASSET_KIND_NONE accepts any kind
	// The deserialization form of [Z2-N-CONTRACT-086]: an unresolvable GUID gives a null handle and one asset.missing diagnostic.
	Zenith_AnyAssetHandle AcquireForDeserialization(const Zenith_AssetGUID& xGUID, Zenith_AssetKind eExpectedKind);

	// Resolution (ANY_THREAD).
	template<typename TAsset> requires Zenith_AssetType<TAsset>
	Zenith_Result<const TAsset*, Zenith_AssetError> Resolve(const Zenith_AssetHandle<TAsset>& xHandle) const;
	template<typename TAsset> requires Zenith_AssetType<TAsset>
	const TAsset* ResolveOrFallback(const Zenith_AssetHandle<TAsset>& xHandle) const;
	template<typename TAsset> requires Zenith_AssetType<TAsset>
	const TAsset* GetFallback() const;                   // null when the kind has none or it is not ACTIVE
	Zenith_AssetState GetState(const Zenith_AnyAssetHandle& xHandle) const;   // live state: presentation only
	Zenith_AssetError GetFailure(const Zenith_AnyAssetHandle& xHandle) const; // m_eCode 0 unless FAILED
	Zenith_StringView GetLogicalPath(const Zenith_AnyAssetHandle& xHandle) const; // diagnostics
	Zenith_AssetGpuResidency GetGpuResidency(const Zenith_AnyAssetHandle& xHandle) const; // presentation
	// Moves (+1) after every publication that can make a handle's resolution newly available; 0 until the first; ANY_THREAD.
	uint64 GetAvailabilityEpoch() const;
	// A counted handle to the pinned engine asset of (kind, role) from the catalogue's fallback table; null when none.
	Zenith_AnyAssetHandle GetPinnedAsset(Zenith_AssetKind eKind, uint16 uRole) const;
	// A loader reports that every upload of the activation the ticket names is complete (PENDING → RESIDENT); lock-free,
	// one compare-exchange of the slot's residency word; a stale ticket is ignored.
	void ReportGpuResident(const Zenith_AssetResidencyTicket& xTicket);
	// A loader reports that an allocation or upload of the activation failed after Activate (PENDING → FAILED), with its own
	// code; counted and logged once per activation; lock-free as ReportGpuResident; a stale ticket is ignored.
	void ReportGpuFailed(const Zenith_AssetResidencyTicket& xTicket, uint16 uLoaderCode);

	// Services.
	Zenith_AssetLoader& GetLoader();
	Zenith_AssetReader& GetReader();
	Zenith_MemoryConsumer& GetMemoryConsumer();           // MEMORY_SUBSYSTEM_ASSETS
	uint64 GetContentHash() const;                       // the merged catalogue's content hash (ZRPL, evidence)
	uint64 GetAssetContentHash(const Zenith_AnyAssetHandle& xHandle) const;   // low word of its output hash

	// Mounts, recovery, development reload.
	void OnMountsChanged();                                             // MAIN_THREAD, after AddSource/RemoveSource
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> ReloadGpuResidencySet();   // device-loss step 4
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> RequestReload(const Zenith_AnyAssetHandle& xHandle);

	// Frame and lifetime.
	void AdvanceFrame(uint64 ulFrameIndex);                            // the Assets.Service node
	void BeginShutdown();                                               // MAIN_THREAD, shutdown step 4
	Zenith_AssetRegistryStats GetStats() const;
	void WriteDump(Zenith_JsonWriter& xWriter) const;

private:
	Zenith_AssetRegistryState* m_pxState = nullptr;                             // Internal, owned
};

/// @contract value Zenith_AssetRegistryStats
/// Ownership: Plain value snapshot.
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetRegistryStats
{
	uint32 m_uLiveSlots = 0, m_uSlotHighWater = 0, m_uMaxSlots = 0;
	uint32 m_auStateCounts[8] = {};                 // indexed by Zenith_AssetState
	uint32 m_auGpuResidencyCounts[5] = {};          // indexed by Zenith_AssetGpuResidency; element NONE is always 0 ("Slot header layout")
	uint64 m_ulCacheBytes = 0, m_ulHostResidentBytes = 0, m_ulDeviceResidentBytes = 0;
	uint64 m_ulMissingAcquires = 0, m_ulFallbacksSubstituted = 0, m_ulMissingLogSuppressed = 0;
};
```

`Create` performs these steps, each unwinding the ones before on failure (`Test_AssetRegistry_Create_FailsAt<Step>_Unwinds`): desc validation (not a fault point: first, `m_bAllowTestKinds` set or `m_axTestKinds` non-empty is `NOT_ALLOWED` when `Zenith_IsShippingBuild()` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Build information"`, a `constexpr` query, so a Shipping registry can never accept a test kind whatever its composition root passes) or when `m_bDevelopmentFeatures` is false (the same refusal, reachable by a test); then every rule of `Zenith_AssetRegistryDesc`, `m_axTestKinds` included, a violation being `INVALID_REQUEST`); 1 the state object (heap countdown); 2 the slot-header array and the slot side-record array, `m_uMaxAssets` entries each, and the pool of `asset.load.maxInFlight` load records (`uFAULT_POINT_ASSET_REGISTRY_SLOTS`; "Slot header layout", "Slot side records"); 3 the two operation tables, loads and reads (`…_OPERATIONS`); 4 the request, retire, acknowledgement, decode-completion and I/O-completion queues (`…_QUEUES`); 5 the staging pool (`…_STAGING`); 6 catalogue load and merge (`…_CATALOGUE`, plus every catalogue failure, `COOK_IDENTITY_MISMATCH` included; a merged catalogue listing an entry of a test code that `m_axTestKinds` does not declare fails this step with `NOT_ALLOWED`, naming the code and the GUID, which is the one refusal point for such an entry); 7 the memory consumer object (`…_CONSUMER`; the composition root registers it with the governor after `Create`); 8 the missing-reference log set of `uASSET_MISSING_LOG_CAPACITY` GUIDs (`…_MISSING_SET`). After step 8 `Create` registers ZenithAssets' built-in loaders ("Zenith_BuiltInAssets header") for `ASSET_KIND_DATA` and `ASSET_KIND_COMPRESSION_DICTIONARY`, and, when `m_bHeadless`, the validate-only loader for every kind of `GetGpuResidentKinds()`.

Every kind column the registry uses — the magic and extension of an expectation, the memory subsystem, the fallback policy, cacheable, GPU resident — is read through `GetKindInfo`; a catalogue entry (refused at `Create` step 6), request or registration of a test code that `m_axTestKinds` does not declare is `NOT_ALLOWED`, and the catalogue load refuses test-kind entries unless `m_bAllowTestKinds` is set (`Zenith_AssetCatalogue::Load`'s `bAllowTestKinds`).

`RegisterKindLoader(eKind, xLoader, xDesc)` applies, in order:

1. after `CloseRegistration` → `REGISTRATION_CLOSED` (and `ZENITH_CHECK` on the main thread, a programmer error); a test kind without `m_bAllowTestKinds`, or one `m_axTestKinds` does not declare → `NOT_ALLOWED`; a null name or a format version of 0 → `INVALID_REQUEST`; a kind already served → `KIND_ALREADY_REGISTERED`, except that a GPU-resident kind in a headless registry accepts and ignores the registration with an info log (the validate-only loader keeps serving it);
2. the merged catalogue's kind-table element for the kind is compared with `xDesc.m_uFormatVersion`: 0 (no content of the kind) passes, equal passes, anything else → `VERSION_MISMATCH` naming both versions, so a build that reads format N never boots on content cooked at N − 1;
3. the kind's fallback policy is `GetKindInfo(kind)->m_eDefaultFallbackPolicy` (a test kind's declared row); SUBSTITUTE with no role-0 row for the kind in the catalogue's fallback table → `NO_FALLBACK`;
4. every fallback-table row of the kind (all roles) is acquired as a pinned slot (flag bit 1: one reference owned by the registry until `BeginShutdown`) and requested at `ASSET_PRIORITY_CRITICAL`; the requests join the operation `GetPinnedLoads` returns;
5. the loader is recorded and the kind joins `GetServedKinds()`;
6. the registration-changed flag is set and the service kicked; the service's next slice applies event E18 to every pipeline of the kind waiting in sub-stage WAIT_LOADER (`Zenith2/Docs/Design/ASSET/Loading.md → "Load record stages"`; the side records are the service's alone, so no main-thread call touches them), so a pinned asset whose hard dependency is of a kind registered at a subsequent step, or a registration order that puts a dependent kind first, loads once both loaders exist.

`CloseRegistration` sets the closed flag under lock 200, sets the registration-changed flag and kicks the service. The slice that sees the registration-changed flag reads `GetServedKinds()` and the closed flag once, under lock 200, and then applies E18 to every waiting pipeline whose kind is served and, only when the flag is closed, E19 to every waiting pipeline (WAIT_LOADER, or CATALOGUE and DEPENDENCIES with the "no loader" flag) whose kind is still absent from the served set it read: a registration made just before `CloseRegistration` is always seen before E19 is applied, whatever order the two main-thread calls' kicks reach the service in, so E19 fails only a kind that is truly unserved with `NO_LOADER`; a pinned pipeline failing so fails `GetPinnedLoads`' operation, which the composition root turns into exit code 3 naming the kind. `RequestReload` returns `NOT_ALLOWED` when `m_bDevelopmentFeatures` is false. `GetPinnedAsset` is valid from the kind's registration.

`ReportGpuResident(xTicket)` and `ReportGpuFailed(xTicket, uCode)` take no lock: each is writer W7 of the slot's residency word ("Slot header layout", table "Residency word writers"): it reads the word (acquire) and, when `xTicket.m_uAttempt` equals the installed value and the residency is PENDING, compare-exchanges the residency to RESIDENT or FAILED; when instead it equals the current value of the open attempt and no early outcome is recorded, compare-exchanges the early outcome to RESIDENT or FAILED (a report that precedes the service's application of `Activate`, or the loader's call of it); otherwise it returns having changed nothing. A failed exchange retries from the read, so a report racing an activation either records an early outcome that the activation's exchange then consumes, or sees the installed activation and applies to it. Every writer of the word is one compare-exchange of the whole word, so a report can never mark another attempt's object RESIDENT or FAILED. The thread whose exchange accepts a FAILED report, early or installed, counts `asset.gpu.failed` and logs one `asset.load` error record (GUID, logical path, the loader's code, whether the attempt had activated), so a failure is logged once per attempt; an early FAILED of an attempt that then ends without activating stays logged and changes no residency. `m_uGeneration` of the ticket is not compared: the attempt counter survives the slot's free and reissue, so the value alone names the attempt.

`GetAvailabilityEpoch()` reads a registry-wide `std::atomic<uint64>` with acquire ordering. It is 0 until the first increment, is never reset, and is incremented by one (`fetch_add`, release) by the thread that makes each of these publications, after the publication's own stores: (1) the service's application of an activation that publishes a non-null object — a load's E4 (`Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"` step 4), a reload swap and a recovery activation (`Zenith2/Docs/Design/ASSET/Loading.md → "Reload and recovery pipelines"`) — once per activation, after the object store and writer W4, so an activation that installs an early RESIDENT or FAILED outcome moves it once; (2) a W7 exchange that moves an installed object's residency to RESIDENT or FAILED; (3) a load failure (E5), after which `ResolveOrFallback` returns the kind's fallback. A reader that observes a new value therefore observes, through `Resolve`, `ResolveOrFallback` and `GetGpuResidency`, every publication that increment followed. Nothing else moves it: a revive from CACHED publishes nothing new, an early outcome is not yet visible, and E12 and unloads make objects unavailable rather than available. A headless registry moves it only for CPU kinds, because its validate-only loader activates null objects and no residency is ever reported there. RPROD's extraction resolver retries its held entities only when the value moves (cross-area row RP33); `Test_AssetRegistry_AvailabilityEpochAdvancesOnActivation` asserts each point.

### Registration functions

```cpp
/// @contract functions Zenith_AssetRegistration
/// Ownership: Write into the caller's registry or schema.
/// Thread affinity: MAIN_THREAD (startup step 6, the declaration phase of the type registry and configuration schema).
/// Invalidation: None.
/// Units: None.
/// Errors: The type registry's and configuration schema's errors, returned unchanged.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterAssetTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterAssetsConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);
```

Both run in startup step 6 (`Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"`): `Zenith_RegisterAssetsConfigKeys` with the other engine libraries' configuration keys in library-layer order, and `Zenith_RegisterAssetTypes` among the engine types (Core, Reflect, ECS, Scene, Assets, …) before any module's `m_pfnRegisterTypes` ([Z2-N-ARCH-847]); step 10 only installs the value-kind hooks ("Reflection value hooks"). The cook calls both, in the same order, before registering the game's modules (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook services and composition"`).

`Zenith_RegisterAssetTypes` registers one `TYPE_KIND_ASSET` type per defined kind with the table's stable and script names and version 1, with no properties; its type ID is `Zenith_Hash64(stableName)`, which `Zenith_PropertyInfo::m_xElementType` of an asset property names ([Z2-N-ARCH-593]). A schema's `asset<K>` resolves `K` against the script names (`Zenith2/Docs/Architecture/DataDefinedComponents.md → "Field type grammar"`). `Zenith_RegisterAssetsConfigKeys` registers the keys of `Zenith2/Docs/Design/ASSET.md → "Configuration keys"`, each `m_bRequired` (the fragments supply the values).

## Zenith_AssetKindLoader header

### Zenith_AssetKindLoaderDesc

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetKindLoader.h
/// @contract value Zenith_AssetKindLoaderDesc
/// Ownership: Plain value copied at registration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: RegisterKindLoader returns ASSET_ERROR_INVALID_REQUEST for a null name or a format version of 0.
/// Version: since Z2 Stage 2. A kind's fallback asset comes from the catalogue's fallback table and its policy from the kind's
///   row (Zenith_AssetRegistry::GetKindInfo: the kind table, or a test's declared row); neither is a loader field.
/// @endcontract
struct Zenith_AssetKindLoaderDesc
{
	const char* const m_szName = nullptr;                    // "Flux.MeshLoader": zones and logs (static lifetime)
	const uint16 m_uFormatVersion = 0;                      // the cooked format version this build reads (the header's must equal it)
	const uint32 m_uMaxConcurrentLoads = 0;                 // 0: only the global limits apply; else at most this many of the kind's
	                                                         // pipelines hold a load record at once (Loading.md "In-flight load budget")
};
```

### Zenith_AssetLoadContext

```cpp
/// @contract value Zenith_AssetLoadInfo
/// Ownership: Owned by the load record; returned by Zenith_AssetLoadContext::GetInfo by const reference. m_pxFile points into the
///   record's arena and m_axDependencies into the pipeline's dependency block; neither is owned by the loader.
/// Thread affinity: IMMUTABLE for the life of one attempt: an E12 restart ends the attempt with Cancel and AcknowledgeCancel and
///   calls BeginLoad again with a new Zenith_AssetLoadInfo whose m_xResidency is a new ticket (Loading.md "Asset state machine").
/// Invalidation: The struct, m_pxFile and the span are valid until the loader's terminal call (Activate, Fail, AcknowledgeCancel)
///   returns; a loader that keeps a dependency beyond it copies the handle (a counted reference). m_xResidency is a plain
///   value the loader may copy and keep, and a loader reports the uploads of an attempt with the ticket of that attempt's own
///   BeginLoad; m_pExistingObject is the loader's own object.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetLoadInfo
{
	const Zenith_AssetGUID m_xAsset = {};
	const Zenith_AssetKind m_eKind = {};
	const Zenith_AssetPriority m_ePriority = {};
	const bool m_bDeviceRecovery = false;                    // ReloadGpuResidencySet: the runtime object exists; recreate its GPU side
	const bool m_bReload = false;                            // RequestReload: build a new object; the old one stays until retired
	const bool m_bGpuResident = false;                       // the kind table's column
	void* const m_pExistingObject = nullptr;                 // non-null exactly when m_bDeviceRecovery
	const Zenith_AssetResidencyTicket m_xResidency = {};     // this attempt's ticket, for its one ReportGpuResident or ReportGpuFailed
	const Zenith_CookedFileView* const m_pxFile = nullptr;   // the validated header and section table
	const Zenith_Span<const Zenith_AnyAssetHandle> m_axDependencies = {}; // hard dependencies, each ACTIVE or FAILED with a fallback
};

/// @contract value Zenith_AssetSectionRequest
/// Ownership: Plain value copied by RequestSections; m_xDestination is loader-owned memory the loader lends to ZenithAssets.
/// Thread affinity: IMMUTABLE.
/// Invalidation: The destination must stay valid and untouched by the loader until the round's OnSectionsLoaded call begins,
///   or until AcknowledgeCancel when the load is cancelled first.
/// Units: Bytes.
/// Errors: RequestSections validates it (ASSET_ERROR_INVALID_REQUEST for an index outside the table, a duplicate index in the
///   round, or a non-empty destination whose size differs from the section's decoded bytes).
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetSectionRequest
{
	const uint32 m_uSectionIndex = 0;                       // index into the section table
	const Zenith_MutableByteSpan m_xDestination = {};        // empty: ZenithAssets decodes into its own buffer; else exactly
	                                                         // m_ulDecodedBytes bytes the loader owns until the round completes
};

/// @contract value Zenith_AssetLoadedSection
/// Ownership: A view element of the span OnSectionsLoaded receives; m_xBytes points into ZenithAssets' decode buffer or into
///   the loader's own destination.
/// Thread affinity: IMMUTABLE.
/// Invalidation: Bytes in a ZenithAssets buffer are valid until OnSectionsLoaded returns (the loader copies what it keeps);
///   bytes in a loader-owned destination are the loader's.
/// Units: Bytes.
/// Errors: None; the bytes passed their stored check and decode.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetLoadedSection
{
	const uint32 m_uSectionIndex = 0;
	const uint16 m_uKind = 0;
	const uint16 m_uIndex = 0;
	const Zenith_ByteSpan m_xBytes = {};                     // decoded bytes; ZenithAssets-owned bytes are valid until OnSectionsLoaded returns
};

/// @contract type Zenith_AssetLoadContext
/// Ownership: Owned by ZenithAssets' load record; passed by reference to every loader call of one load. The loader may keep
///   the reference until it makes one terminal call (Activate, Fail or AcknowledgeCancel), after which it is invalid.
/// Thread affinity: ANY_THREAD for Activate, Fail, AcknowledgeCancel, SetResidentBytes, IsCancelRequested; RequestSections,
///   SetUserData and GetUserData only on the thread of a BeginLoad or OnSectionsLoaded call.
/// Invalidation: Invalid after the terminal call returns.
/// Units: Bytes.
/// Errors: RequestSections returns ASSET_ERROR_INVALID_REQUEST for a section index outside the table, a duplicate index in one
///   round or a destination whose size differs from the section's decoded bytes, ASSET_ERROR_LIMIT_EXCEEDED past
///   uASSET_MAX_SECTION_ROUNDS; a second terminal call is ZENITH_CHECK.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AssetLoadContext
{
public:
	const Zenith_AssetLoadInfo& GetInfo() const;
	Zenith_Result<void, Zenith_AssetError> RequestSections(Zenith_Span<const Zenith_AssetSectionRequest> axSections);
	void SetUserData(void* pUserData);
	void* GetUserData() const;
	void SetResidentBytes(Zenith_MemorySubsystem eSubsystem, uint64 ulHostBytes, uint64 ulDeviceBytes);
	bool IsCancelRequested() const;
	// terminal: the object is published (release store). For a GPU-resident kind, call it once the object is complete on the
	// CPU side and every upload it needs has been requested: handed to the kind owner's pending-upload list by a step that
	// cannot fail (every fallible step of the hand-off comes first, and its failure is Fail instead). Never wait for the owner's
	// allocation, a rendered frame or the copy timeline. The residency becomes PENDING; afterwards the loader calls
	// Zenith_AssetRegistry::ReportGpuResident(GetInfo().m_xResidency) when the uploads complete, or ReportGpuFailed with its
	// code when an allocation or upload fails: exactly one report per attempt, which may come from the owner's thread before the
	// service has applied Activate, or before Activate is called at all; it is kept and applied at the activation. A
	// device-recovery load activates with its existing object pointer.
	void Activate(void* pObject);
	void Fail(uint16 uLoaderCode);                 // terminal: ASSET_ERROR_LOADER_FAILED with the code
	void AcknowledgeCancel();                       // terminal, only after Cancel: everything created for the load released
private:
	friend class Zenith_AssetLoadRecordAccess;               // Internal
	void* m_pRecord = nullptr;                               // the Internal load record, opaque to this header
};

/// @contract type Zenith_AssetUnloadContext
/// Ownership: Owned by ZenithAssets; valid until AcknowledgeUnload.
/// Thread affinity: ANY_THREAD for AcknowledgeUnload.
/// Invalidation: Invalid after AcknowledgeUnload.
/// Units: None.
/// Errors: A second AcknowledgeUnload is ZENITH_CHECK.
/// Version: since Z2 Stage 2.
/// @endcontract
class Zenith_AssetUnloadContext
{
public:
	const Zenith_AssetGUID& GetAsset() const;
	void* GetObject() const;
	void AcknowledgeUnload();                       // the object and every resource it owns are released or queued for deferred deletion
private:
	void* m_pRecord = nullptr;                               // the Internal unload record, opaque to this header
};
```

### Zenith_AssetKindLoader

```cpp
/// @contract interface Zenith_AssetKindLoader
/// Ownership: Implemented by the kind's owner (Flux, ZenithScene, a module); registered by reference; the owner keeps it alive
///   until every asset of its kind is unloaded (Loading.md "Shutdown sequence").
/// Thread affinity: ANY_THREAD. ZenithAssets calls the members from its background slices, one call at a time per asset,
///   possibly concurrently for different assets.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Failures are reported through the load context (Fail); no member returns an error; untrusted section bytes never
///   reach an assert inside a loader ([Z2-N-CONTRACT-472]).
/// Reentrancy: NONE for ZenithAssets' calls into the loader: a loader member may call Acquire, AcquireAny, Resolve, GetKindInfo,
///   GetPinnedAsset, ReportGpuResident, ReportGpuFailed, the loader's RequestLoad and its own context, and no other registry
///   member (the list of Zenith_AssetRegistry's Reentrancy field). A loader never defers Activate, Fail or AcknowledgeCancel
///   until a load it requested with RequestLoad completes: anything it needs ACTIVE before activation is a catalogue hard
///   dependency (m_axDependencies), and a request it makes is fire-and-forget (Loading.md "Dependency loading").
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AssetKindLoader
{
public:
	virtual ~Zenith_AssetKindLoader() = default;
	// Choose the first round of sections (RequestSections), or make a terminal call. Must return within 1 ms.
	virtual void BeginLoad(Zenith_AssetLoadContext& xContext) = 0;
	// A round's sections are decoded and checked. Build the object; then request another round, or make a terminal call now
	// or afterwards from any thread. A GPU kind activates once the object is complete on the CPU side and its uploads are
	// requested from its owner (Zenith_AssetLoadContext::Activate), never at upload completion; completion is
	// ReportGpuResident and a subsequent failure ReportGpuFailed.
	virtual void OnSectionsLoaded(Zenith_AssetLoadContext& xContext, Zenith_Span<const Zenith_AssetLoadedSection> axSections) = 0;
	// The load is cancelled. Release everything created for it (cancel queued uploads; release partial GPU resources through
	// deferred deletion), then AcknowledgeCancel. A loader that has already called Activate is never sent Cancel.
	virtual void Cancel(Zenith_AssetLoadContext& xContext) = 0;
	// Release an object previously activated (last reference, cache eviction, reload retirement, shutdown).
	virtual void Unload(Zenith_AssetUnloadContext& xContext) = 0;
	// Memory pressure CRITICAL: release CPU-side copies of GPU-resident data of every object whose residency is RESIDENT or
	// FAILED (an object whose uploads are PENDING keeps the data its uploads read); return the host bytes freed.
	virtual uint64 DropCpuCopies() = 0;
};
```

GEOM's loaders implement this interface in the order of cross-area row AS29 (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`): `OnSectionsLoaded` pushes the object onto GEOM's load queue (a push that cannot fail, accepted from ZenithAssets' background slices outside `Flux.BuildGraph`) and then calls `Activate(pObject)` in the same call; the drain at `Flux.BuildGraph` performs the fallible allocation and `RequestUpload` (retrying `FLUX_UPLOAD_ERROR_QUEUE_FULL` at its next run, `Zenith2/Docs/Rendering/GpuResources.md → "Upload paths"`), and a failed allocation or an upload reporting `FAILED` after activation is `ReportGpuFailed(GetInfo().m_xResidency, code)`, never `Fail`. A GEOM asset therefore reaches ACTIVE during the boot, before the first frame renders, and a texture activated before its tail is resident binds its usage's fallback until then (`Zenith2/Docs/Design/GEOM/Textures.md → "Fallback textures"`).

## Zenith_AssetLoader header

### Zenith_AssetLoadRequest

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetLoader.h
/// @contract enum Zenith_AssetLoadFlags
/// Ownership: Plain value; a bit set in Zenith_AssetLoadRequest::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: RequestLoad returns ASSET_ERROR_INVALID_REQUEST for a bit that is not an enumerator.
/// Version: since Z2 Stage 2. Never serialized; a new flag takes the next bit.
/// @endcontract
enum Zenith_AssetLoadFlags : uint32
{
	ASSET_LOAD_FLAG_NONE = 0,
	ASSET_LOAD_FLAG_PREFETCH_SOFT_DEPENDENCIES = 1u << 0,    // also request the soft dependencies, at BACKGROUND priority
};

/// @contract value Zenith_AssetLoadRequest
/// Ownership: Plain value read by RequestLoad; the handle is copied (the request holds its own reference until completion).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: RequestLoad validates it.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetLoadRequest
{
	const Zenith_AnyAssetHandle m_xAsset = {};
	const Zenith_AssetPriority m_ePriority = ASSET_PRIORITY_NORMAL;
	const uint32 m_uFlags = ASSET_LOAD_FLAG_NONE;
	Zenith_AssetLoadCompletionQueue* const m_pxCompletions = nullptr; // optional: one record at completion
	const uint64 m_ulUserTag = 0;                                // echoed; simulation requesters pass the completion ID
	const char* const m_szRequester = "unnamed";                  // static-lifetime literal for logs and the dump
};
```

### Zenith_AssetLoadCompletionQueue

```cpp
/// @contract value Zenith_AssetLoadCompletion
/// Ownership: Plain value, copied into the requester's queue by ZenithAssets and out of it by Drain into the caller's arena.
/// Thread affinity: IMMUTABLE once drained. Members non-const: the completion queue overwrites its ring slots.
/// Invalidation: A drained record lives as long as the arena Drain wrote it into.
/// Units: None.
/// Errors: m_xError is meaningful only when m_eStatus is FAILED (m_eCode 0 otherwise).
/// Version: since Z2 Stage 2. Never serialized as bytes; a simulation requester publishes a FAILED record through
///   Zenith_AssetLoadResultRecord (Formats.md "Load result payload").
/// @endcontract
struct Zenith_AssetLoadCompletion
{
	uint64 m_ulIssueID;                                     // Zenith_OperationHandle::GetIssueID
	uint64 m_ulUserTag;
	Zenith_AssetGUID m_xAsset;
	Zenith_OperationStatus m_eStatus;                        // SUCCEEDED, FAILED or CANCELLED
	Zenith_AssetError m_xError;                              // FAILED only; m_eCode 0 otherwise
};

/// @contract container Zenith_AssetLoadCompletionQueue
/// Ownership: Owned by a requester (the script runtime, the streamer, the scene loader); must outlive every request naming it.
/// Thread affinity: ANY_THREAD for pushes by ZenithAssets; Drain is OWNER_THREAD.
/// Invalidation: The span returned by Drain is valid until the next Drain.
/// Units: None.
/// Errors: None: RequestLoad reserves a slot when a request naming the queue is accepted and refuses the request with
///   ASSET_ERROR_QUEUE_FULL when none is free, so a completion is never lost (QUEUE_FULL_POLICY_REJECT at submission).
///   Counters <name>.full (refused reservations), <name>.dropped (always 0) and <name>.high_water ([Z2-N-CONTRACT-345]),
///   published by the registry's service for every live queue.
/// Version: since Z2 Stage 2. Source API.
/// Alignment: alignof(Zenith_AssetLoadCompletion).
/// Overflow: capacity fixed at Create, 1 … 65,536; the name is 1 … 48 bytes of [a-z0-9._] ("script.loads.world1").
/// Allocation failure: Create returns ASSET_ERROR_OUT_OF_MEMORY; nothing allocates afterwards.
/// Complexity: push and Drain O(1) per record; lock-free multi-producer single-consumer ring.
/// @endcontract
class Zenith_AssetLoadCompletionQueue
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AssetLoadCompletionQueue>, Zenith_AssetError> Create(
		uint32 uCapacity, Zenith_StringView strName, Zenith_Heap& xHeap);
	Zenith_StringView GetName() const;
	Zenith_Span<const Zenith_AssetLoadCompletion> Drain(Zenith_Arena& xArena);   // completion order
	uint32 GetCapacity() const;
};
```

### Zenith_AssetLoader

```cpp
/// @contract value Zenith_AssetLoaderDesc
/// Ownership: Plain value inside Zenith_AssetRegistryDesc.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Counts, bytes, microseconds.
/// Errors: Create rejects zero values and a queue depth below 8 with ASSET_ERROR_INVALID_REQUEST.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetLoaderDesc
{
	const uint32 m_uRequestQueueCapacity = 4096;    // intake queue, power of two
	const uint32 m_uMaxLoadsInFlight = 0;           // asset.load.maxInFlight
	const uint64 m_ulMaxInFlightBytes = 0;          // asset.load.maxInFlightBytes
	const uint32 m_uDecodeQueueDepth = 0;           // asset.decode.queueDepth (Z2-B-ASSET-002)
	const uint32 m_uDecodeConcurrency = 0;          // asset.decode.concurrency; 0 = max(1, workers / 2)
	const uint64 m_ulDecodeSliceBytes = 0;          // asset.decode.sliceBytes
	const uint64 m_ulStagingBytes = 0;              // asset.io.stagingBytes
	const uint64 m_ulChunkBytes = 0;                // asset.io.chunkBytes
	const uint64 m_ulStreamThresholdBytes = 0;      // asset.io.streamThresholdBytes
	const uint64 m_ulCacheMaxBytes = 0;             // asset.cache.maxBytes
	const uint32 m_uServiceFrameBudgetUs = 0;       // asset.service.frameBudgetUs
	const uint32 m_uReloadRetireFrames = 0;         // asset.reload.retireFrames
};

/// @contract type Zenith_AssetLoader
/// Ownership: Owned by Zenith_AssetRegistry; reached through GetLoader.
/// Thread affinity: ANY_THREAD_EXCEPT(ServiceFrame, ServiceUntilComplete). ServiceFrame runs in the Assets.Service node alone;
///   ServiceUntilComplete runs on the main thread outside a frame iteration, or on a tool or test thread that is not a job worker.
/// Invalidation: None.
/// Units: Priorities; milliseconds for timeouts.
/// Errors: RequestLoad returns ASSET_ERROR_INVALID_REQUEST, _QUEUE_FULL, _SHUTTING_DOWN, _OUT_OF_MEMORY; a load's own failure is its
///   operation's FAILED status with the code, and GetError returns the full record. Cancellation is the operation handle's
///   RequestCancel or destruction ([Z2-N-CONTRACT-091]).
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks of the requester; completions are records.
/// Version: since Z2 Stage 2. Source API.
/// Locks: uLOCK_LEVEL_STREAMING_REQUESTS (250) for the request table.
/// @endcontract
class Zenith_AssetLoader
{
public:
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> RequestLoad(const Zenith_AssetLoadRequest& xRequest);
	Zenith_Result<void, Zenith_AssetError> SetPriority(const Zenith_OperationHandle& xLoad, Zenith_AssetPriority ePriority);
	Zenith_AssetError GetError(const Zenith_OperationHandle& xLoad) const;
	void ServiceFrame(Zenith_JobContext& xContext);
	Zenith_Result<void, Zenith_AssetError> ServiceUntilComplete(const Zenith_OperationHandle& xLoad, uint32 uTimeoutMs);
	Zenith_AssetLoaderStats GetStats() const;
};

/// @contract value Zenith_AssetLoaderStats
/// Ownership: Plain value snapshot (the counters of Zenith2/Docs/Design/ASSET.md "Counters").
/// Thread affinity: IMMUTABLE. Members non-const: its producer default-constructs it and fills it field by field before
///   returning it.
/// Invalidation: None.
/// Units: Counts, bytes, microseconds.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetLoaderStats
{
	uint32 m_uLoadsInFlight = 0; uint64 m_ulInFlightBytes = 0;
	uint32 m_uDecodeUnitsReserved = 0, m_uDecodeUnitsHighWater = 0; uint64 m_ulDecodeFull = 0;
	uint64 m_ulStagingReserved = 0, m_ulStagingHighWater = 0;
	uint64 m_ulThrottled = 0, m_ulIOQueueFull = 0, m_ulSliceOverruns = 0;
	uint64 m_aulOutcomes[4] = {};                   // requested, succeeded, failed, cancelled
	uint64 m_ulLastCriticalLatencyUs = 0;
};
```

`ServiceUntilComplete` returns `ASSET_ERROR_INVALID_REQUEST` when the operation is null or when `Zenith_IsJobWorkerThread()` (`ZenithCore/Zenith_Multithreading.h`) is true, which JOBS sets around every job body it runs, on a worker or on the main thread helping inside `RunFrame` or `RunRoot`; a frame iteration runs main-thread code only inside job bodies, so a call from a frame node is refused by this one query and no other frame-iteration check exists; it never returns `ASSET_ERROR_QUEUE_FULL`; on timeout it returns `ASSET_ERROR_TIMEOUT` with the operation still running; when the operation completes it returns success for `SUCCEEDED` and the operation's error record for `FAILED` or `CANCELLED` (`GetError`).

## Zenith_AssetReader header

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetReader.h
/// @contract type Zenith_AssetReader
/// Ownership: Owned by Zenith_AssetRegistry; reached through GetReader. Each read holds a reference to its asset until it completes.
/// Thread affinity: ANY_THREAD_EXCEPT(ServiceUntilComplete). ServiceUntilComplete has Zenith_AssetLoader::ServiceUntilComplete's
///   affinity: the main thread outside a frame iteration, or a tool or test thread that is not a job worker.
/// Invalidation: A destination must stay valid and untouched by the requester until the read's operation reports complete.
/// Units: Bytes; offsets from the start of the cooked file.
/// Errors: Submission returns ASSET_ERROR_NULL_HANDLE, _FOREIGN_HANDLE, _STALE_HANDLE, _NOT_ACTIVE (ReadRange and ReadSection need
///   an ACTIVE or CACHED asset), _NOT_FOUND (ReadVerifiedFile and GetVerifiedFileSize of a path no catalogue file entry lists),
///   _INVALID_REQUEST (priority, range outside the file or overflowing, zero bytes, section outside the table, a ReadRange or
///   ReadVerifiedFile destination of the wrong size), _QUEUE_FULL, _SHUTTING_DOWN. Execution failures are the operation's FAILED
///   status: IO, NOT_INSTALLED (the file is absent), CORRUPT, and INVALID_REQUEST for a ReadSection destination whose size differs
///   from the section's decoded bytes, known only once the header is read. GetError returns the record.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AssetReader
{
public:
	// Raw bytes of the cooked file: no check, no decode. Streaming consumers decode themselves.
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> ReadRange(const Zenith_AnyAssetHandle& xAsset,
		uint64 ulOffset, uint64 ulBytes, Zenith_MutableByteSpan xDestination, Zenith_AssetPriority ePriority);
	// One section: read, section check, decode through the decode queue (back-pressured like loads) into xDestination,
	// which must be exactly the section's decoded bytes.
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> ReadSection(const Zenith_AnyAssetHandle& xAsset,
		uint32 uSectionIndex, Zenith_MutableByteSpan xDestination, Zenith_AssetPriority ePriority);
	// A non-asset cooked file (bytecode, blobs) listed as a file entry of the catalogue: read whole into xDestination, which
	// must be exactly GetVerifiedFileSize bytes (INVALID_REQUEST otherwise), and verify its recorded Hash128; a mismatch is
	// ASSET_ERROR_CORRUPT with ASSET_CORRUPTION_FILE_HASH_MISMATCH and the destination's content is then unspecified.
	Zenith_Result<Zenith_OperationHandle, Zenith_AssetError> ReadVerifiedFile(const Zenith_VirtualPath& xPath,
		Zenith_MutableByteSpan xDestination, Zenith_AssetPriority ePriority);
	Zenith_Result<uint64, Zenith_AssetError> GetVerifiedFileSize(const Zenith_VirtualPath& xPath) const;
	Zenith_AssetError GetError(const Zenith_OperationHandle& xRead) const;
	// Runs the registry's service slices on the calling thread until the read completes or the timeout passes. The rules of
	// Zenith_AssetLoader::ServiceUntilComplete: INVALID_REQUEST for a null handle or inside any job body (Zenith_IsJobWorkerThread(),
	// which also covers a frame iteration's main-thread nodes), TIMEOUT with the read still running; on completion success for
	// SUCCEEDED and GetError's record otherwise.
	Zenith_Result<void, Zenith_AssetError> ServiceUntilComplete(const Zenith_OperationHandle& xRead,
		uint32 uTimeoutMs);
};
```

`ServiceUntilComplete` is the read counterpart of the loader's (cross-area row AU26 cites it): a tool, a test or a library's `Create` waits for a read with no frame loop. Reads reach the service through the request intake queue; a RANGE read holds no decode unit and is polled; a SECTION read repeats the load path's header stages and holds one decode unit; a VERIFIED_FILE read holds one decode unit and hashes in one decode slice; a `ReadSection` destination is checked once the header is read.

## Engine content reads during Create

A library that reads engine content while it is being created, before any frame runs (LIGHT's `Flux_LightingSystem::Create` reading its lookup tables and its default environment, `Zenith2/Docs/Design/LIGHT.md`; cross-area row AB13), uses exactly this sequence on the main thread, after `Zenith_AssetRegistry::Create` (startup step 11) and outside any job body:

1. `GetVerifiedFileSize(xPath)`, which also answers whether the file exists: `ASSET_ERROR_NOT_FOUND` when no catalogue file entry lists the path.
2. A destination of exactly that many bytes from the library's own allocator.
3. `ReadVerifiedFile(xPath, xDestination, ASSET_PRIORITY_CRITICAL)`.
4. `ServiceUntilComplete(xRead, uTimeoutMs)` with `uTimeoutMs` = `runtime.startupLoadTimeoutMs`; success means the bytes are the file's and its recorded Hash128 verified, `ASSET_ERROR_CORRUPT` with `ASSET_CORRUPTION_FILE_HASH_MISMATCH` means a re-cook is needed, and `ASSET_ERROR_TIMEOUT` leaves the read running, so the library returns its own error and the composition root's unwinding destroys the registry only after `BeginShutdown` has drained it.

| Path | Accepted | Why |
|---|---|---|
| `content:/<p>` naming a catalogue file entry: a non-asset cooked file a cook step wrote with `Zenith_CookContext::WriteFile`, such as LIGHT's `content:/Engine/Lighting/DFG.zllt` when its global step writes `Engine/Lighting/DFG.zllt` (Formats.md "File entries") | yes, in every build | served by whichever mount the VFS resolves `content:/` to: the package and patch archives, or the loose mount |
| `content:/<p>` naming an asset's logical path or object path | no: `ASSET_ERROR_NOT_FOUND` | an asset is loaded through `Zenith_AssetLoader::RequestLoad` and its kind loader, never read raw at `Create` |
| a host directory holding a cooked fixture tree (a test's generated cooked fixture, such as LIGHT's `LightCookedFixtures`) | only as a loose mount: the test creates its VFS with a `MOUNT_KIND_LOOSE` source over that directory, so the tree's catalogue and files appear under `content:/`, and the reader is then called with `content:/` paths as above | PLAT creates the loose mount only when `Zenith_IsShippingBuild()` is false, and `AddSource` with `MOUNT_KIND_LOOSE` in a Shipping build is `FILE_ERROR_MOUNT_NOT_ALLOWED` (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount kinds and roots"`), so a host-directory root is refused in Shipping by construction |
| a host path string (`C:/…`, `/data/…`) or any path outside `content:/` | no | it is not a valid `Zenith_VirtualPath` (`Zenith_VirtualPath` construction refuses it), and ZenithAssets has no host-file read; there is no second read call |

`Test_AssetReader_EngineContentReadBeforeFrames` (Tests.md "Loader tests") runs the sequence over a registry whose VFS mounts a generated cooked fixture directory as its loose source, with no frame loop.

<!-- assumption: pending GEOM -->
GEOM's `Zenith_AssetReader::ReadRange(const Zenith_PersistentID&, …)` takes a handle here, so the asset stays referenced while the read runs; GEOM's texture streamer holds the texture's handle already. GEOM's streaming state machine decodes mips in its own background job after `ReadRange`; routing mip reads through `ReadSection` instead puts those decodes under the decode-queue back-pressure (a cross-area note to GEOM).
<!-- /assumption -->

## Zenith_AssetCatalogue header

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetCatalogue.h
/// @contract enum Zenith_AssetDependencyFlags
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A dependency whose flags are not exactly one of HARD and SOFT, optionally with EXTERNAL, is CATALOGUE_INVALID.
/// Version: since Z2 Stage 2. Serialized as uint8 in the dependency table (Formats.md "Dependency and string tables").
/// @endcontract
enum Zenith_AssetDependencyFlags : uint8
{
	ASSET_DEPENDENCY_FLAG_HARD = 1u << 0,        // loaded and ACTIVE (or substituted) before the dependent's BeginLoad
	ASSET_DEPENDENCY_FLAG_SOFT = 1u << 1,        // referenced; loaded on demand or prefetched
	ASSET_DEPENDENCY_FLAG_EXTERNAL = 1u << 2,    // fragments only: the target indexes the external reference table
};

/// @contract value Zenith_AssetDependency
/// Ownership: A view element of its catalogue.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its catalogue.
/// Units: None.
/// Errors: None. In a merged catalogue EXTERNAL is never set and m_uEntryIndex indexes the merged entry table.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetDependency
{
	const uint32 m_uEntryIndex = 0;             // entry index, or (EXTERNAL, unmerged fragment) external reference index
	const uint8 m_uFlags = 0;                   // Zenith_AssetDependencyFlags
};

/// @contract value Zenith_AssetFallbackRow
/// Ownership: A view element of its catalogue (Formats.md "Fallback table").
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its catalogue.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetFallbackRow
{
	const Zenith_AssetKind m_eKind = {};
	const uint16 m_uRole = 0;                   // 0: the kind's SUBSTITUTE fallback; 1 … 65,535: the owner's further engine assets
	const uint32 m_uEntryIndex = 0;
	const Zenith_AssetGUID m_xAsset = {};
};

/// @contract value Zenith_AssetCatalogueEntry
/// Ownership: A view into its catalogue; valid while the catalogue lives.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its catalogue.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. Formats.md "Entry table" is its serialized form.
/// @endcontract
struct Zenith_AssetCatalogueEntry
{
	const Zenith_AssetGUID m_xAsset = {};
	const Zenith_AssetKind m_eKind = {};
	const uint16 m_uFormatVersion = 0;
	const Zenith_StringView m_strObjectPath = {}; // "content:/Objects/0b/0b6e….ztex"
	const Zenith_StringView m_strLogicalPath = {}; // "content:/Characters/Hero.gltf#mesh:Body"
	const uint64 m_ulFileBytes = 0;
	const uint32 m_uHeaderBytes = 0;            // 64 + 40 × section count
	const uint64 m_ulResidentEstimateBytes = 0; // the cook's estimate of host + device bytes when resident
	const Zenith_Hash128 m_xKeyHash = {};        // the DDC key hash = the header's content hash
	const Zenith_Hash128 m_xOutputHash = {};     // Zenith_Hash128Of the whole cooked file
	const Zenith_Span<const Zenith_AssetDependency> m_axDependencies = {};
	const uint8 m_uSourceMount = 0;             // the Zenith_MountKind of the catalogue that supplied the entry
};

/// @contract value Zenith_AssetCatalogueFile
/// Ownership: A view element of its catalogue: a non-asset cooked file the reader verifies (Formats.md "File entries").
/// Thread affinity: IMMUTABLE.
/// Invalidation: With its catalogue.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_AssetCatalogueFile
{
	const Zenith_StringView m_strPath = {};
	const uint64 m_ulBytes = 0;
	const Zenith_Hash128 m_xHash = {};
};

/// @contract type Zenith_AssetCatalogue
/// Ownership: Created by Load or Merge as a Zenith_Owned; the registry holds the current merged snapshot and retires a replaced one
///   once no load uses it ("GUID index and catalogue snapshots").
/// Thread affinity: IMMUTABLE after creation; every member is ANY_THREAD.
/// Invalidation: Entries and views live as long as the catalogue.
/// Units: Bytes.
/// Errors: Load returns ASSET_ERROR_CATALOGUE_INVALID (bad magic, check, limits, unsorted table, duplicate GUID or logical path,
///   dependency index out of range, a hard-dependency cycle, a kind-table or fallback-table inconsistency, EXTERNAL in a base
///   catalogue, a test-kind table or test-kind entry without bAllowTestKinds), ASSET_ERROR_VERSION_MISMATCH (the catalogue format version), ASSET_ERROR_COOK_IDENTITY_MISMATCH (a base
///   catalogue's engine cook version or kind count), ASSET_ERROR_OUT_OF_MEMORY; Merge returns CATALOGUE_INVALID for an
///   unresolvable external reference, disagreeing kind tables or same-precedence fragments listing one GUID with different
///   output hashes; never an assert.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AssetCatalogue
{
public:
	// bAllowTestKinds: only a registry created with m_bAllowTestKinds (tests) passes true; otherwise a catalogue carrying a
	// test-kind table (header flag bit 1) or an entry of a test code is CATALOGUE_INVALID.
	static Zenith_Result<Zenith_Owned<Zenith_AssetCatalogue>, Zenith_AssetError> Load(Zenith_ByteSpan xBytes,
		Zenith_AssetPlatform ePlatform, bool bAllowTestKinds, Zenith_Heap& xHeap);
	static Zenith_Result<Zenith_Owned<Zenith_AssetCatalogue>, Zenith_AssetError> Merge(
		Zenith_Span<const Zenith_AssetCatalogue* const> apxInPrecedenceOrder, Zenith_Heap& xHeap);   // highest precedence last
	const Zenith_AssetCatalogueEntry* Find(const Zenith_AssetGUID& xAsset) const;       // O(log n)
	const Zenith_AssetCatalogueEntry* FindByLogicalPath(Zenith_StringView strPath) const;
	const Zenith_AssetCatalogueFile* FindFile(Zenith_StringView strPath) const;
	Zenith_Span<const Zenith_AssetCatalogueEntry> GetEntries() const;                  // ascending GUID
	uint64 GetContentHash() const;
	uint32 GetEngineCookVersion() const;
	// 0: no content of the kind. A test code reads the test-kind table (0 when the catalogue has none).
	uint16 GetKindFormatVersion(Zenith_AssetKind eKind) const;
	Zenith_Span<const Zenith_AssetFallbackRow> GetFallbacks() const;       // ascending (kind, role)
	const Zenith_AssetFallbackRow* FindFallback(Zenith_AssetKind eKind, uint16 uRole) const;
	bool IsFragment() const;
};
```

`Load` checks the engine cook version and the kind count K of a base catalogue against the build (`uZENITH_ENGINE_COOK_VERSION`, `uASSET_KIND_DEFINED_COUNT`) and fails with `ASSET_ERROR_COOK_IDENTITY_MISMATCH` when either differs, which `Create` returns and startup turns into exit code 3; nothing else is compared at `Load`, because the build's per-kind format versions exist only as its kind loaders register. The per-kind check is `RegisterKindLoader`'s ("Zenith_AssetRegistry"): the loader's `m_uFormatVersion` against `GetKindFormatVersion(kind)`, where 0 means the game has no content of the kind and passes. The earlier cook-identity field (a hash of the engine cook version and the kind table) is not part of the format: a hash of values the reader itself holds proves nothing the two direct comparisons do not.

## Zenith_CookedFile header

### Zenith_CookedHeader and Zenith_CookedSectionEntry

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_CookedFile.h
/// @contract value Zenith_CookedHeader
/// Ownership: Plain value decoded from the 64 header bytes field by field (never memcpy, Z2-D-038).
/// Thread affinity: IMMUTABLE. Members non-const: the writer fills the header field by field and patches m_uHeaderCheck
///   last.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None; Zenith_ParseCookedHeader states every rule.
/// Version: since Z2 Stage 2. Formats.md "Header layout" is its byte layout.
/// @endcontract
struct Zenith_CookedHeader
{
	uint32 m_uMagic;
	uint16 m_uFormatVersion;
	uint16 m_uFlags;                            // 0 in container version 1
	uint32 m_uEngineCookVersion;
	uint32 m_uSectionCount;
	Zenith_Hash128 m_xContentHash;               // the DDC key hash of the cook step that produced the file
	Zenith_AssetGUID m_xAsset;
	uint16 m_uPlatform;                         // Zenith_AssetPlatform
	uint16 m_uKind;                             // Zenith_AssetKind
	uint32 m_uHeaderCheck;
	uint64 m_ulFileBytes;
};

/// @contract value Zenith_CookedSectionEntry
/// Ownership: Plain value decoded from one 40-byte table entry.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes; offsets from the file start.
/// Errors: None.
/// Version: since Z2 Stage 2. Formats.md "Section table".
/// @endcontract
struct Zenith_CookedSectionEntry
{
	const uint16 m_uKind = 0;                   // format-defined section kind; 0xFF00 … 0xFFFF reserved to ASSET
	const uint16 m_uIndex = 0;
	const uint32 m_uStoredCheck = 0;            // low 32 bits of Zenith_Hash64 over the stored bytes
	const uint64 m_ulOffset = 0;
	const Zenith_CompressedSectionDesc m_xDesc = {}; // COMPRESS's 24-byte descriptor, decoded
};

/// @contract value Zenith_CookedFileExpectation
/// Ownership: Plain value built by the loader from the catalogue entry and the kind's row (magic), or by a tool or test.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None; Zenith_ParseCookedHeader compares the header against it (checks C1 and C5 to C9 of Formats.md "Checks").
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CookedFileExpectation              // what the catalogue says the file must be
{
	const uint32 m_uMagic = 0;
	const uint16 m_uFormatVersion = 0;
	const Zenith_AssetKind m_eKind = {};
	const Zenith_AssetPlatform m_ePlatform = {};
	const Zenith_AssetGUID m_xAsset = {};
	const Zenith_Hash128 m_xContentHash = {};
	const uint64 m_ulFileBytes = 0;
};

/// @contract type Zenith_CookedFileView
/// Ownership: Arena-allocated by Zenith_ParseCookedHeader in the caller's arena; the section array lives in the same arena.
/// Thread affinity: IMMUTABLE after the parse.
/// Invalidation: With the arena.
/// Units: Bytes.
/// Errors: None; accessors of a parsed view do not fail.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CookedFileView
{
	const Zenith_CookedHeader m_xHeader = {};
	const Zenith_Span<const Zenith_CookedSectionEntry> m_axSections = {}; // table order: ascending (kind, index)
	const Zenith_CookedSectionEntry* Find(uint16 uKind, uint16 uIndex) const;   // binary search
};
```

### Parsing and validation functions

```cpp
/// @contract functions Zenith_CookedFileFunctions
/// Ownership: No state; the view is allocated in xArena.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: ASSET_ERROR_CORRUPT with the Zenith_AssetCorruption detail, ASSET_ERROR_VERSION_MISMATCH, _PLATFORM_MISMATCH,
///   _LIMIT_EXCEEDED, _OUT_OF_MEMORY; every rule of Formats.md "Checks" in its order; no input asserts.
/// Version: since Z2 Stage 2.
/// @endcontract
Zenith_Result<const Zenith_CookedFileView*, Zenith_AssetError> Zenith_ParseCookedHeader(
	Zenith_ByteSpan xHeaderBytes, const Zenith_CookedFileExpectation& xExpect, Zenith_Arena& xArena);
Zenith_Result<uint32, Zenith_AssetError> Zenith_ReadCookedHeaderBytesNeeded(Zenith_ByteSpan xFirst16Bytes);
uint32 Zenith_ComputeSectionCheck(Zenith_ByteSpan xStoredBytes);   // low 32 bits of Zenith_Hash64(bytes, 0)
Zenith_Result<void, Zenith_AssetError> Zenith_VerifySection(const Zenith_CookedSectionEntry& xEntry,
	Zenith_ByteSpan xStoredBytes);                                                // the size and the stored check
```

### Zenith_CookedFileWriter

```cpp
/// @contract value Zenith_CookedFileWriterDesc
/// Ownership: Plain value copied by the writer's constructor; m_pxCodecs and m_pxHeap are borrowed and outlive the writer.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A null codec set or heap, a format version of 0 or a kind of 0 makes every AddSection and Finish return
///   ASSET_ERROR_INVALID_REQUEST.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_CookedFileWriterDesc
{
	const uint32 m_uMagic = 0;
	const uint16 m_uFormatVersion = 0;
	const Zenith_AssetKind m_eKind = {};
	const Zenith_AssetPlatform m_ePlatform = {};
	const Zenith_AssetGUID m_xAsset = {};
	const Zenith_Hash128 m_xContentHash = {};
	const Zenith_CompressionCodecSet* const m_pxCodecs = nullptr;
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_CookedFileWriter
/// Ownership: A stack or heap object owned by one cook step or test; owns copies of the sections it was given until Finish.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: AddSection returns ASSET_ERROR_INVALID_REQUEST for a duplicate (kind, index) or a reserved kind,
///   ASSET_ERROR_LIMIT_EXCEEDED past uASSET_MAX_SECTIONS or ulASSET_MAX_COOKED_FILE_BYTES, and COMPRESS's errors mapped to
///   ASSET_ERROR_INVALID_REQUEST (a bad level or dictionary) or _OUT_OF_MEMORY; Finish returns _OUT_OF_MEMORY.
/// Version: since Z2 Stage 2. Output is byte-identical for the same desc, sections (in any AddSection order), codec settings and
///   pinned codec version.
/// Example: Zenith2/Engine/ZenithAssets/Tests/Examples/Zenith_CookedFile_Example.cpp
/// @endcontract
class Zenith_CookedFileWriter
{
public:
	explicit Zenith_CookedFileWriter(const Zenith_CookedFileWriterDesc& xDesc);
	Zenith_Result<void, Zenith_AssetError> AddSection(uint16 uKind, uint16 uIndex, Zenith_ByteSpan xBytes,
		Zenith_CompressionCodecID eCodec, sint32 iLevel, const Zenith_CompressionDictionary* pxDictionary,
		Zenith_Arena& xScratch);
	Zenith_Result<void, Zenith_AssetError> Finish(Zenith_Array<uint8>& xOut);
};
```

`Finish` sorts sections by (kind, index), lays them out after the table at 16-byte-aligned offsets with zero padding, encodes each descriptor with `Zenith_EncodeCompressedSectionDesc`, computes each stored check, writes the header with `m_ulFileBytes`, then computes the header check over bytes `[0, 64 + 40 N)` with the check field zeroed and writes it. A section compressed with `m_bStoreIfNotSmaller` falling back to `NONE` is recorded with codec 0 (`Zenith2/Docs/Design/COMPRESS/PublicAPI.md → "Zenith_CompressedSection.h"`).

## Zenith_AssetArchive header

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_AssetArchive.h
#pragma once
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithPlatform/Zenith_VirtualFileSystem.h"

/// @contract value Zenith_ArchiveReaderDesc
/// Ownership: Plain value owned by the composition root (or a tool or test) for as long as any archive source it created lives;
///   passed as the factory's user data; every pointer is borrowed.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A null heap makes every factory call fail FILE_ERROR_OUT_OF_MEMORY with a log line.
/// Version: since Z2 Stage 4. Source API.
/// @endcontract
struct Zenith_ArchiveReaderDesc
{
	Zenith_Heap* const m_pxHeap = nullptr;             // the entry table and the source object
	Zenith_FaultInjector* const m_pxFaultInjector = nullptr; // tests; nullptr in production
};

/// @contract functions Zenith_ArchiveReaderFunctions
/// Ownership: Zenith_CreateArchiveMountSource takes ownership of xContainer (the source that holds the archive file) and returns
///   a Zenith_Owned<Zenith_MountSource> the caller passes to Zenith_VirtualFileSystem::AddSource; the returned source owns
///   the container, the parsed entry table and its string table.
/// Thread affinity: MAIN_THREAD for the factory (startup step 5, or the frame prologue when PLAT mounts a patch or an
///   installed asset pack); the returned source is ANY_THREAD as Zenith_MountSource requires; Zenith_VerifyArchiveData is
///   a tool or test call on a thread that is not a job worker.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_Result<…, Zenith_FileError>: FILE_ERROR_NOT_FOUND (the archive is not in the container),
///   FILE_ERROR_ARCHIVE_CORRUPT (a check of Formats.md "Archive checks", its number logged), FILE_ERROR_OS and
///   FILE_ERROR_SHORT_READ from the container, FILE_ERROR_OUT_OF_MEMORY; never an assert on archive bytes.
/// Version: since Z2 Stage 4. The factory's signature is PLAT's Zenith_PfnCreateArchiveMountSource ("Zenith_AssetArchive
///   header" assumption); the format is Formats.md "Archive file" version 1.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_MountSource>, Zenith_FileError> Zenith_CreateArchiveMountSource(
	Zenith_Owned<Zenith_MountSource> xContainer, Zenith_StringView strArchiveName, void* pUserData);
// Reads every entry through the source and compares its data hash; the first mismatch is FILE_ERROR_ARCHIVE_CORRUPT naming it.
Zenith_Result<uint32, Zenith_FileError> Zenith_VerifyArchiveData(const Zenith_MountSource& xArchiveSource,
	Zenith_MutableByteSpan xScratch);   // returns the entry count; xScratch at least 64 KiB
```

The archive source implements PLAT's `Zenith_MountSource` (`Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount sources"`) over its container:

| Member | Behaviour |
|---|---|
| `GetDescription` | the archive name (`"base.zpak"`) |
| `Lookup(rel)` | binary search of the sorted entry table for `rel` by bytes: a file entry gives `{ m_ulSize = data bytes, m_ulUncompressedSize = data bytes, m_uCodec = 0, m_ulOpaque = entry index, m_bDirectory = false }`; otherwise, when some entry's path begins with `rel` + `/` (the first path at or after that prefix in the table), a directory entry `{ 0, 0, 0, the first matching index, true }`; otherwise `FILE_ERROR_NOT_FOUND`; the empty `rel` is the root directory. Paths compare exactly (archives are case-sensitive stores) |
| `Open(entry, rel)` | a directory entry → `FILE_ERROR_NOT_A_FILE`; otherwise opens the archive file through the container (`xContainer->Open` of the container entry recorded at creation) and returns a `Zenith_SourceFile` holding the container's file (words 0–1), the entry's data offset (word 2) and its data bytes (word 3) |
| `Read(file, offset, destination)` | `offset > data bytes` → `FILE_ERROR_SHORT_READ`; reads `min(destination size, data bytes − offset)` bytes through the container at `data offset + offset`; returns the bytes read (0 at the entry's end) |
| `Close(file)` | closes the container's file |
| `List(dir, visitor)` | visits each distinct next path component under `dir` + `/` once, in byte order: a file entry when the remainder has no `/`, else a directory entry; `FILE_ERROR_NOT_FOUND` when `dir` is neither the root nor a directory |

Creation (`Zenith_CreateArchiveMountSource`): (1) `xContainer->Lookup(strArchiveName)`; (2) `Open` it and read the 64-byte header; apply checks A1–A3 of `Zenith2/Docs/Design/ASSET/Formats.md → "Archive checks"`; (3) allocate `40 N + S` bytes from the desc's heap (`uFAULT_POINT_ASSET_ARCHIVE_TABLE`) and read the entry and string tables in reads of at most 16 MiB; (4) apply A4–A7 (A7 sorts a temporary index array by data offset, O(N log N)); (5) close the container file, allocate the source object (`uFAULT_POINT_ASSET_ARCHIVE_SOURCE`) and return it. Every failure frees what the earlier steps allocated. The reads are synchronous container reads on the main thread (the mount step and the prologue are not job work, `Zenith2/Docs/Design/PLAT/Filesystem.md → "Synchronous file access"`).

<!-- assumption: pending PLAT (X154) -->
This is a new ask of PLAT, not an existing interface: PLAT's `Zenith2/Docs/Design/PLAT/Filesystem.md` today has no factory member in `Zenith_VirtualFileSystemDesc` and a two-word `Zenith_SourceFile`. The ask is core-doc and area item X154 (open; CompositionRoot's step-5 row and PLAT's two API changes, extending X114 (d), which carries only the ownership wording) and cross-area row AS22. Two PLAT changes are needed: (1) `Zenith_VirtualFileSystemDesc` gains `Zenith_PfnCreateArchiveMountSource m_pfnCreateArchiveSource` and `void* m_pArchiveUserData`, with `using Zenith_PfnCreateArchiveMountSource = Zenith_Result<Zenith_Owned<Zenith_MountSource>, Zenith_FileError> (*)(Zenith_Owned<Zenith_MountSource> xContainer, Zenith_StringView strArchiveName, void* pUserData);`, which the package mount (every `package[]` and install-time `assetPacks[]` archive of the mount manifest), the patch mount after "Patch mount verification" and the prologue's `AddSource` of downloaded packs call with a container source over the archive's directory (a directory source on Windows and for downloaded packs, the APK asset source on Android); the composition root passes `&Zenith_CreateArchiveMountSource` and a `Zenith_ArchiveReaderDesc` at startup step 5, because ZenithPlatform sits below ZenithAssets ([Z2-N-ARCH-078]); a manifest naming an archive when no factory is set fails as `FILE_ERROR_MANIFEST_INVALID`; (2) `Zenith_SourceFile` widens to `uint64 m_aulOpaque[4]`, so an archive's per-request file nests its container's file and the entry's range. `Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount sources"` already places the archive reader in ZenithAssets; its "PACK archive reader" wording and `Zenith2/Docs/Design/PLAT.md`'s rows that give the reader to PACK become "ASSET's archive reader (PACK writes the archives)". The step-5 call is X154 (a); the ownership wording X114 (d).
<!-- /assumption -->

## Zenith_BuiltInAssets header

```cpp
// Zenith2/Engine/ZenithAssets/Zenith_BuiltInAssets.h
#pragma once
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithCompression/Zenith_CompressionDictionary.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"

/// @contract type Zenith_DataAsset
/// Ownership: Created and destroyed by ZenithAssets' built-in ASSET_KIND_DATA loader; owns its bytes on the registry's heap.
/// Thread affinity: IMMUTABLE after activation. Members non-const: the kind loader sets the members at activation,
///   after the pool constructs the object.
/// Invalidation: Pointers from Resolve follow Zenith_AssetRegistry's invalidation rule.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 2. The .zdata format: section kind 1 index 0 holds the bytes (stored or compressed).
/// @endcontract
class Zenith_DataAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_DATA;
	Zenith_ByteSpan GetBytes() const;
private:
	Zenith_ByteSpan m_xBytes;
};

/// @contract type Zenith_CompressionDictionaryAsset
/// Ownership: Created and destroyed by ZenithAssets' built-in ASSET_KIND_COMPRESSION_DICTIONARY loader; owns the COMPRESS
///   dictionary object it created with Zenith_Compression::CreateDictionary.
/// Thread affinity: IMMUTABLE after activation; the dictionary is used concurrently by decode slices. Members
///   non-const: the kind loader sets the members at activation, after the pool constructs the object.
/// Invalidation: As Zenith_DataAsset.
/// Units: Bytes.
/// Errors: None; creation errors fail the load with ASSET_ERROR_LOADER_FAILED carrying the Zenith_CompressionError.
/// Version: since Z2 Stage 2. The .zdict format (format version 1): section kind 1 index 0 is an 8-byte info block
///   (codec ID u8, three reserved zero bytes, the level as sint32 LE); section kind 2 index 0 the dictionary bytes, stored.
///   PACK trains and writes dictionaries (Zenith2/Docs/Design/COMPRESS.md "Cook integration"); ASSET loads them.
/// @endcontract
class Zenith_CompressionDictionaryAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_COMPRESSION_DICTIONARY;
	const Zenith_CompressionDictionary& GetDictionary() const;
private:
	Zenith_Owned<Zenith_CompressionDictionary> m_xDictionary;
};
```

- The two loaders are registered by `Create` (PACK has no engine library, and dictionaries must exist before any section that names one decodes), are CPU kinds, and have fallback policy FAIL ("Asset kinds" in the entry document); they request one round, copy or adopt the decoded bytes, `SetResidentBytes(MEMORY_SUBSYSTEM_ASSETS, …)` and `Activate`.
- A dictionary's section-1 codec must be a codec of the registry's codec set and its dictionary ID (from `GetDictionaryID`) must equal the ID the referring sections' descriptors name; a mismatch fails the dependent's decode with `DECODE_FAILED` (COMPRESS's `DICTIONARY_MISMATCH` in the detail's high byte).

## Registry internals

### Slot header layout

<!-- elaboration -->
One `alignas(64)` record per slot in a `Zenith_Array` reserved once at `Create` to `m_uMaxAssets` entries (it never grows, so pointers into it are stable and no reader needs the registry lock):

| Offset | Field | Type | Written by |
|---|---|---|---|
| 0 | `m_uReferences` | `std::atomic<uint32>` | handle copy (relaxed +1), destroy (acq_rel −1) |
| 4 | `m_uGeneration` | `std::atomic<uint32>` | the retire step (release) |
| 8 | `m_eState` | `std::atomic<uint8>` | the service (release) |
| 9 | `m_uFlags` | `std::atomic<uint8>` | bit 0 retire queued, bit 1 pinned (fallback-table asset), bit 2 reload pipeline reserving, queued, running or retiring, bit 3 one more reload queued behind the in-flight load or pipeline, bit 4 device-recovery pipeline queued or running, bit 5 load requested while UNLOADING, bits 6–7 zero |
| 10 | `m_eKind` | `uint16` | set at allocation |
| 12 | `m_uLoadRecord` | `std::atomic<uint32>` | the service; 0 when no pipeline |
| 16 | `m_xAsset` | `Zenith_AssetGUID` | set at allocation |
| 32 | `m_pObject` | `std::atomic<void*>` | `Activate` (release); read by `Resolve` (acquire) |
| 40 | `m_uCatalogueEntry` | `uint32` | index in the current snapshot |
| 44 | `m_uFailure` | `std::atomic<uint32>`: bits 0–7 the `Zenith_AssetErrorCode`, bits 8–23 the detail, bits 24–31 zero | the service |
| 48 | `m_ulResidency` | `std::atomic<uint64>`: bits 0–27 the installed attempt value I (the attempt whose activation the published object carries; 0 before the first), bits 28–55 the current attempt value C (the last value issued), bit 56 open (C names an admitted attempt that has neither activated nor ended), bits 57–59 the `Zenith_AssetGpuResidency` of the installed object, bits 60–61 the open attempt's early outcome (0 none, 1 RESIDENT, 2 FAILED), bits 62–63 zero | the writers W1 … W7 of the table below, each one compare-exchange of the whole word; the word is never reset, not at free and not at reissue |
| 56 | `m_uResidentHostKiB`, `m_uResidentDeviceKiB` | `uint32` × 2 as one atomic `uint64` | `SetResidentBytes` |

Residency word writers (acq_rel compare-exchange loops; the service's writers W1 … W6 run on its slice, W7 on any thread; `next(C)` is `C + 1`, or 1 after 2^28 − 1, so 0 is never issued):

| Writer | When | Precondition | New word |
|---|---|---|---|
| W1 issue | E3: any pipeline (load, reload, recovery) is admitted | open clear | C = next(C), open set, early outcome 0; the ticket `{slot, generation, C}` goes into the attempt's `m_xResidency` |
| W2 re-issue | an E12 restart, after the attempt's `AcknowledgeCancel` | open set | C = next(C), early outcome 0 (open stays set); the next `BeginLoad` receives the new ticket |
| W3 close | the attempt ends without activating: E5, E7, a failed or cancelled reload or recovery | open set | open clear, early outcome 0; C is kept and never issued again |
| W4 activate | the service applies `Activate`: load E4, reload swap, recovery activation | open set | I = C, open clear, early outcome 0; residency NONE for a kind outside `GetGpuResidentKinds()` or a headless registry, else PENDING (early outcome 0), RESIDENT (1) or FAILED (2) |
| W5 loss | E12 on an installed GPU object | residency PENDING, RESIDENT or FAILED | residency LOST |
| W6 unload | E10 | open clear | residency NONE; I and C kept |
| W7 report | `ReportGpuResident`, `ReportGpuFailed` | value = I ≠ 0 and residency PENDING; or open set, value = C and early outcome 0 | residency RESIDENT or FAILED; or early outcome 1 or 2; any other report changes nothing |

- No report is lost: a ticket's value is issued by W1 or W2 before the loader receives it, so its report meets the word either while the attempt is open (W7 records the early outcome, which W4 turns into the residency) or after W4 (W7 applies to the installed activation while it is PENDING); a report that loses a race re-reads and re-decides.
- No report is applied to the wrong attempt: a value is issued once (W1 and W2 never reuse C, and W3 keeps it), so a report of an attempt that was cancelled after its infallible push, restarted by E12, or replaced by a subsequent activation matches neither I with PENDING nor the open C, and is ignored; a pre-loss upload's report of the installed value finds residency LOST and is ignored.
- No activated slot stays PENDING once its report is made: W4 installs PENDING only when no early outcome was recorded, and the attempt's one report then meets I with PENDING; PENDING otherwise ends at E12, a reload swap or E10.
- Two values alias only when 2^28 − 1 further attempts of one slot are admitted while a report of the first is still outstanding; a loader reports within the life of its attempt's uploads, and each attempt needs an admission and a header read, so this is the design's bound.
- Gauges follow each successful exchange: when the residency field changes from r0 to r1, `m_auGpuResidencyCounts[r0]` falls by 1 and `[r1]` rises by 1 for r0, r1 other than NONE (element NONE stays 0); `asset.gpu.pending` and `asset.gpu.lost` are the PENDING and LOST elements; the early-outcome field moves no gauge.

Slot 0 is never issued, so a zero slot index in a handle is always null. The frame of the last release, which the keep-alive cache orders by, is not in the header: the service writes it into the slot's side record when it processes the retire entry ("Slot side records"), so no handle release writes anything but the reference count.

### Slot side records

<!-- elaboration -->
One 64-byte record per slot in a second array of `m_uMaxAssets` entries reserved at `Create` step 2 beside the headers. Only the service reads or writes it (one `bg/Asset.Service` slice at a time), so it holds no atomics:

| Offset | Field | Type | Meaning |
|---|---|---|---|
| 0 | `m_eQueuedKind` | `uint8` | the queued or running pipeline of the slot: 0 none, 1 load, 2 reload, 3 device recovery (a slot has at most one such pipeline besides its ACTIVE object; a reload's retiring old object lives in that reload's load record, not here) |
| 1 | `m_eSubStage` | `uint8` | RESERVING (a reload waiting for its peak reservation) or the QUEUED sub-stage: CATALOGUE, DEPENDENCIES, WAIT_LOADER or ADMISSION (Loading.md "Load record stages") |
| 2 | `m_ePriority` | `uint8` | the pipeline's priority: the minimum over its live requests and its dependents' |
| 3 | `m_uPipelineFlags` | `uint8` | bit 0 the kind had no loader at CATALOGUE; bit 1 pinned request; bit 2 a device recovery is owed when the reserving, queued or running reload fails or is cancelled; bit 3 the slot is counted by the device-recovery operation (Loading.md "Device-loss reload"); bits 4–7 zero |
| 4 | `m_uAdmissionNext` | `uint32` | the next slot in its priority's admission FIFO; 0 at the tail |
| 8 | `m_ulRequestSequence` | `uint64` | the loader's request counter when the pipeline was created: admission order within a priority |
| 16 | `m_uRequestHead` | `uint32` | the first `Zenith_RequestRecord` of the pipeline (a pool index); 0 when none |
| 20 | `m_uDependenciesWaiting` | `uint16` | hard dependencies not yet ACTIVE or substituted |
| 22 | `m_uPipelineDependencyCount` | `uint16` | the pipeline's hard-dependency count, ≤ 4,096 |
| 24 | `m_pxPipelineDependencies` | `Zenith_AnyAssetHandle*` | the pipeline's dependency block, from the registry's heap (`m_uPipelineDependencyCount` handles) |
| 32 | `m_pxActiveDependencies` | `Zenith_AnyAssetHandle*` | the ACTIVE object's dependency block, taken over from the pipeline block at activation and released at E10 (a reload's block replaces it when the old object retires) |
| 40 | `m_uActiveDependencyCount` | `uint16` | its count |
| 42 | reserved | 6 bytes | zero |
| 48 | `m_uCachePrev`, `m_uCacheNext` | `uint32` × 2 | the keep-alive cache's LRU list; 0 when not CACHED |
| 56 | `m_ulLastReleaseFrame` | `uint64` | the frame whose `AdvanceFrame` preceded the service's processing of the last retire entry: the LRU key |

A QUEUED pipeline is therefore a side record linked into one of five admission FIFOs (one per `Zenith_AssetPriority`) and holds no load record; the 256-byte load record is taken from its pool only at admission (E3). The dependency blocks are the only allocations of the load path outside the pools: one per pipeline with hard dependencies, `16 × count` bytes; failing to allocate one fails the pipeline with `OUT_OF_MEMORY` (`uFAULT_POINT_ASSET_LOAD_RECORD`).

### Reference counting and retirement

1. `Acquire(guid)`: take lock 200; look the GUID up in the GUID index (`Zenith_HashMap<Zenith_AssetGUID, uint32>`); if found and the slot is live, increment `m_uReferences` (a slot at zero references in state CACHED, UNLOADED, FAILED or UNLOADING is revived here: the retire-queued flag is left set, because the entry it marks may still be queued and only the service clears it when it pops that entry; a CACHED slot returns to ACTIVE; an UNLOADING slot keeps unloading and stays allocated after its acknowledgement, Loading.md "Asset state machine"); else find the catalogue entry (`NOT_FOUND` if absent), check the kind (`KIND_MISMATCH`), pop a slot from the FIFO free list (`REGISTRY_FULL` if empty), initialise it (state UNLOADED, references 1), insert it in the index; release the lock; return the handle.
2. Copy: `m_uReferences.fetch_add(1, relaxed)`; a handle can only be copied from a live handle, so the count is already at least 1.
3. Destroy: `if (m_uReferences.fetch_sub(1, acq_rel) == 1)` and the retire-queued flag CAS from 0 to 1 succeeds, push the slot index onto the retire queue (a `Zenith_BoundedQueue<uint32>` of capacity `m_uMaxAssets`) and kick the service. The flag is set exactly while one entry for the slot is queued (a revive never clears it and a slot allocation keeps it), so the queue holds at most one entry per slot and never fills, however the service is starved.
4. The service pops a retire entry, takes lock 200, clears the flag, and re-reads the count: non-zero means the slot was revived (or reallocated); skip. A free slot is skipped too. Zero: by state — ACTIVE and cacheable within the cache budget → CACHED (Loading.md "The keep-alive cache"); ACTIVE otherwise → UNLOADING (Loading.md "Unload"); QUEUED or LOADING cannot occur (a request holds a reference); UNLOADING → nothing now (the acknowledgement frees it); UNLOADED or FAILED → free now. Release the lock before calling any loader.
5. Free: under lock 200, remove the index entry, increment the generation (a slot whose generation reaches `0xFFFFFFFF` is retired and never pushed back, counted in `asset.slots.retired_exhausted`), push the slot at the tail of the FIFO.
6. Pinned slots (fallback assets) hold one reference owned by the registry until `BeginShutdown`.

`AcquireForDeserialization(guid, kind)` is `AcquireAny(guid, kind)` that returns a null handle on `NOT_FOUND` or `KIND_MISMATCH` and then, under lock 200, inserts the GUID into the missing-reference log set (a `Zenith_HashSet<Zenith_AssetGUID>` of `uASSET_MISSING_LOG_CAPACITY` entries reserved at `Create`): an insertion logs one `asset.missing` record (GUID, expected kind, the caller's document path when the deserializer passed one through `Zenith_ReflectionWalk`'s context); a GUID already present logs nothing; when the set is full, at most one `asset.missing` record per frame is logged (the frame index of the last one is kept) and `m_ulMissingLogSuppressed` counts the rest. `asset.missing` counts every such acquire in every case ([Z2-N-CONTRACT-086], [Z2-N-CONTRACT-342]).

Resolution (`Resolve`, `GetState`, `GetFailure`): a null handle → `NULL_HANDLE`; `m_pxRegistry != this`, slot 0 or slot > `m_uMaxAssets` → `FOREIGN_HANDLE` (the table holds `m_uMaxAssets` + 1 headers and slot 0 is never issued, so slots 1 … `m_uMaxAssets` are valid); `m_uGeneration` (acquire) differing → `STALE_HANDLE`; kind differing from `T::eASSET_KIND` → `KIND_MISMATCH`; state not ACTIVE or CACHED → `NOT_ACTIVE`; object null → `NO_RUNTIME_OBJECT`. No path locks or asserts.

### GUID index and catalogue snapshots

- The merged catalogue is an immutable snapshot with a reference count (`std::atomic<uint32>`); a load record takes a reference for its lifetime, and the registry holds one for the current snapshot.
- `OnMountsChanged` submits `bg/Asset.CatalogueRefresh`: list `content:/Catalogues/`, load `content:/assets.zcat` and every `*.zcat` there (at most `uASSET_MAX_CATALOGUE_FRAGMENTS`), merge them in mount precedence (package, patch, loose; within a mount by file name byte order), and publish the new snapshot at the next `AdvanceFrame` under lock 200 by swapping the pointer; slots keep their GUIDs and re-map `m_uCatalogueEntry` in the same swap. The previous snapshot is destroyed when its reference count reaches zero.
- A merge in which two catalogues of the same precedence list one GUID with different output hashes is `CATALOGUE_INVALID`; the previous snapshot stays current and the failure is logged. A higher-precedence entry replaces a lower one (a patch).
- The effect of a swap on each slot, by state and by whether its entry was removed, changed (a different output hash) or kept, is the "catalogue swap" rows of Loading.md "Asset state machine"; in short, an ACTIVE asset keeps its object in every case, a CACHED asset whose entry was removed or changed is evicted, and a FAILED asset whose entry changed, or whose failure was `NOT_INSTALLED` or `NOT_FOUND`, returns to UNLOADED.
- The fallback table of the new snapshot must list the same (kind, role, GUID) rows as the current one for every served kind; a swap that would change a pinned asset is refused as `CATALOGUE_INVALID` and logged (pinned assets are fixed for the process).

## Internal types and files

| Type | File | Content |
|---|---|---|
| `Zenith_AssetRegistryState` | `Internal/Registry/Zenith_AssetRegistry.h` | the slot array, free list, GUID index, snapshot pointer, loader, reader, consumer, pinned list |
| `Zenith_LoadRecord` (256 bytes) | `Internal/Loading/Zenith_LoadRecord.h` | slot, stage, pipeline kind, priority, the file view and its arena, round counter, section record list, the kind loader, the load context, cancel flag, start time (the requests and dependency handles stay in the slot's side record); in stage RETIRING a reload's old object, its dependency block and count, the frame of the swap and the old object's unload context |
| `Zenith_AssetSlotSide` (64 bytes) | `Internal/Registry/Zenith_AssetSlots.h` | the side record of "Slot side records": the queued pipeline, its admission link, requests, dependency blocks, the cache link |
| `Zenith_SectionRecord` (96 bytes) | same | section index, stage, reserved unit, staging span, destination, I/O operation handle, decode operation handle, streamed-decode context, stored and decoded bytes |
| `Zenith_RequestRecord` | same | operation ticket, completion queue and reserved slot, user tag, priority, requester name |
| `Zenith_DecodeQueue` | `Internal/Loading/Zenith_DecodeQueue.h` | unit counters per priority class, the ready list ordered by (priority, request order), running slice count |
| `Zenith_StagingPool` | `Internal/Loading/Zenith_StagingPool.h` | a TLSF-free, size-classed pool of 64 KiB granules carved from one block of `asset.io.stagingBytes` allocated at `Create`; reservations are byte counts rounded to granules |
| `Zenith_HeadlessKindLoader` | `Internal/Loading/Zenith_HeadlessKindLoader.h` | the validate-only loader (Loading.md "Headless loading") |
| `Zenith_DataAssetLoader`, `Zenith_DictionaryAssetLoader` | `Internal/BuiltIn/Zenith_BuiltInLoaders.h` | the built-in loaders of "Zenith_BuiltInAssets header" |
| `Zenith_ArchiveMountSource` | `Internal/Archive/Zenith_ArchiveMountSource.h` | the `Zenith_MountSource` implementation of "Zenith_AssetArchive header": the container, the container entry, the entry table, the string table |
| `Zenith_AssetRegistryConstructTag` | `Internal/Registry/Zenith_AssetRegistry.h` | the empty tag only `Create` constructs, so no other code calls the public constructor |

Pools, each created at `Create` and never grown: load records, exactly `asset.load.maxInFlight` (a record is taken at admission and returned when the service has drained the pipeline's terminal call, or, for a reload that swapped, its old object's `AcknowledgeUnload`, so admission never finds the pool empty); section records, `asset.decode.queueDepth` + `asset.load.maxInFlight` (a section record exists only while it holds a decode unit or waits in WAIT_UNIT for its record's round); request records, the operation table's capacity (`asset.load.maxInFlight` × 8 + 256, one per live request operation). Queued pipelines, reload and device-recovery pipelines included, need no pooled record ("Slot side records"), so no number of queued pipelines exhausts a pool; the steady state allocates nothing but dependency blocks.

### Test access

```cpp
// Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetRegistryTestAccess.h (a friend declared in Internal/Registry/Zenith_AssetRegistry.h)
/// @contract functions Zenith_AssetRegistryTestAccess
/// Ownership: Static functions over a registry the test owns; no state.
/// Thread affinity: The test's thread; the counts are snapshots.
/// Invalidation: None.
/// Units: Counts; bytes.
/// Errors: None; a slot index outside the table is ZENITH_CHECK (a test defect).
/// Version: since Z2 Stage 2. Test support only and header-only ([Z2-N-ARCH-255]): every member is defined inline in this header,
///   which ZenithAssets' own tests include; never part of a shipping target.
/// @endcontract
struct Zenith_AssetLiveRecordCounts
{
	uint32 m_uLoadRecords = 0, m_uSectionRecords = 0, m_uRequestRecords = 0, m_uOperations = 0, m_uCompletionSlots = 0;
	uint32 m_uDecodeUnits = 0, m_uPinnedSlots = 0, m_uQueuedPipelines = 0, m_uWaitingForLoader = 0, m_uDependencyBlocks = 0;
	uint64 m_ulStagingBytes = 0;
};
class Zenith_AssetRegistryTestAccess
{
public:
	static Zenith_AnyAssetHandle MakeUncountedCopy(const Zenith_AnyAssetHandle& xHandle);   // bytes only
	static void ForgetUncountedCopy(Zenith_AnyAssetHandle& xCopy);          // zero it without releasing
	static void SetSlotGeneration(Zenith_AssetRegistry& xRegistry, uint32 uSlot, uint32 uGeneration);
	static uint32 GetSlotReferenceCount(const Zenith_AssetRegistry& xRegistry, uint32 uSlot);
	static Zenith_AssetState GetSlotState(const Zenith_AssetRegistry& xRegistry, uint32 uSlot);
	static Zenith_AssetLiveRecordCounts GetLiveRecordCounts(const Zenith_AssetRegistry& xRegistry);
	static void PauseDecodeSlices(Zenith_AssetRegistry& xRegistry, bool bPaused);   // back-pressure tests
	// While paused no service slice runs (the acknowledgement queue and the registration-changed flag stay unprocessed);
	// residency and boot-order tests.
	static void PauseService(Zenith_AssetRegistry& xRegistry, bool bPaused);
};
```

`Zenith_AssetRegistryTestAccess` also declares the stage-hold seam other areas' cancellation stress uses through the header-only `Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetStageHold.h` (cross-area row ST20; STREAM's `Test_Streaming_CancelReload1000Cycles_NoLeak`):

```cpp
// Zenith2/Engine/ZenithAssets/Tests/Support/Zenith_AssetStageHold.h (header-only, [Z2-N-ARCH-255])
/// @contract value Zenith_AssetHoldStage
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Test support only.
/// @endcontract
enum Zenith_AssetHoldStage : uint8 { ASSET_HOLD_STAGE_DECODING = 1, ASSET_HOLD_STAGE_DELIVER, ASSET_HOLD_STAGE_ACTIVATING };
/// @contract type Zenith_AssetStageHold
/// Ownership: A test-owned value bound to one registry and one GUID; Release or destruction lets the pipeline continue.
/// Thread affinity: The test's thread; IsParked ANY_THREAD (an atomic read).
/// Invalidation: Must be released before the registry is destroyed (ZENITH_CHECK).
/// Units: None.
/// Errors: Arm returns false when another hold is armed on the registry (at most one per registry).
/// Version: since Z2 Stage 2. Test support only.
/// @endcontract
class Zenith_AssetStageHold
{
public:
	bool Arm(Zenith_AssetRegistry& xRegistry, const Zenith_AssetGUID& xAsset, Zenith_AssetHoldStage eStage);
	bool IsParked() const;   // the pipeline reached the stage and is waiting
	void Release();                         // idempotent
	~Zenith_AssetStageHold();               // releases
};
```

A held pipeline re-polls the hold once per service slice, and each re-poll is that stage's cancellation point (Loading.md "Cancellation points by stage"): a cancellation while parked completes `CANCELLED` and counts `asset.loads.cancelled.<stage>`. The seam is `Zenith_AssetRegistryTestAccess::SetStageHold(Zenith_AssetRegistry&, const Zenith_AssetGUID&, uint8 uStage, const std::atomic<uint8>* pState)`, null to clear; the pipeline reads that one pointer of the registry state at the start of the three stages, and it is null unless a test set it, so a non-test run never parks.

## Examples

`Zenith2/Engine/ZenithAssets/Tests/Examples/Zenith_AssetLoader_Example.cpp` creates a registry over a fixture catalogue with a fake kind loader, acquires a handle, requests a load with a completion queue, services it with `ServiceUntilComplete`, resolves the object and destroys everything, asserting zero leaks; `Zenith_CookedFile_Example.cpp` writes a two-section file (one zstd, one stored) and parses it back. Both run as tests (`Test_AssetLoader_Example_Runs`, `Test_CookedFile_Example_Runs`, [Z2-N-CONTRACT-421]).
