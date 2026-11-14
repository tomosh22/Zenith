# PACK design: public API

This file is part of the PACK design family; the entry document is `Zenith2/Docs/Design/PACK.md`. It declares every public header of the `ZenithPackage` engine library, the PACK-owned runtime bridge files inside `ZenithRuntime`, and the Luau surface. Formats are `Zenith2/Docs/Design/PACK/Formats.md`; state machines and algorithms are `Zenith2/Docs/Design/PACK/Algorithms.md`; the packaging tool side is `Zenith2/Docs/Design/PACK/Tooling.md`; tests are `Zenith2/Docs/Design/PACK/Tests.md`.

## Scope of this file

| Header | Declares | Heading |
|---|---|---|
| `Zenith2/Engine/ZenithPackage/Zenith_PackageTypes.h` | the error enumeration, unit kinds, pack states, identities, events, limits | "Zenith_PackageTypes header" |
| `Zenith2/Engine/ZenithPackage/Zenith_PackageManifest.h` | the reader of `package.zpkm` | "Zenith_PackageManifest header" |
| `Zenith2/Engine/ZenithPackage/Zenith_AssetPackDelivery.h` | the delivery interface, the Play and simulated deliveries | "Zenith_AssetPackDelivery header" |
| `Zenith2/Engine/ZenithPackage/Zenith_EntitlementQuery.h` | the entitlement interface and the configuration-backed implementation | "Zenith_EntitlementQuery header" |
| `Zenith2/Engine/ZenithPackage/Zenith_AssetPackInstaller.h` | the asset-pack installer | "Zenith_AssetPackInstaller header" |
| `Zenith2/Engine/ZenithPackage/Zenith_InstallFileSystem.h` | the install file-system interface, the host and memory implementations | "Zenith_InstallFileSystem header" |
| `Zenith2/Engine/ZenithPackage/Zenith_UpdateApplier.h` | the patch and DLC applier and the pre-mount entry point | "Zenith_UpdateApplier header" |
| `Zenith2/Engine/ZenithPackage/Zenith_InstallRecord.h` | the install record and update classification | "Zenith_InstallRecord header" |
| `Zenith2/Engine/ZenithPackage/Zenith_UpdateMigration.h` | the installed-update migration registry | "Zenith_UpdateMigration header" |
| `Zenith2/Engine/ZenithPackage/Zenith_PackageService.h` | the service the composition root owns | "Zenith_PackageService header" |
| `Zenith2/Engine/ZenithRuntime/Internal/Package/Zenith_RuntimePackage.h` | the runtime bridge (MOD's library, PACK's subdirectory) | "Runtime bridge declarations" |
| `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/Zenith_PackageScriptBindings.cpp` | the Luau surface | "Script-facing API" |

## Rules across the PACK API

- Every fallible public function returns `Zenith_Result<T, Zenith_PackageError>`; untrusted bytes (manifests, journals, state files, Play callbacks, recipe JSON, delivered files) never reach an assert (`Zenith2/Docs/Contracts.md → "Errors and results"`).
- Every public type carries one contract block (`Zenith2/Docs/Contracts.md → "Contract blocks per type"`); declarations below show the blocks in full.
- No public header names an STL type, a JNI type, a Play type, a Windows or POSIX type; JNI and the Java bridge live in `Internal/Android/` files (`Zenith2/Docs/Contracts.md → "API boundary"`).
- Factories take a `const <Name>Desc&` and every dependency by reference or pointer; nothing is looked up (`Zenith2/Docs/Contracts.md → "No mutable global state"`).
- Fault points carry the library tag `0x504B` (`PK`) in their high half (`Zenith2/Docs/Design/CORE/Memory.md → "Fault-injection support"`); they are listed per type below and collected in `Zenith2/Docs/Design/PACK/Tests.md → "Fault point table"`.
- Strings that name packs, archives, units and DLC are UTF-8 views into storage the issuing object owns; a pack name always matches `^[a-z][a-z0-9_]{0,63}$` (the mount manifest's rule, `Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount manifest schema"`).

## Zenith_PackageTypes header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_PackageTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"

/// @contract enum Zenith_PackageError
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The error enumeration of ZenithPackage; every enumerator is described in Zenith2/Docs/Design/PACK.md
///   "Error enumeration". Value 0 is never an enumerator.
/// Version: since Z2 Stage 2. Enumerators keep their values once shipped; new failures append.
/// @endcontract
enum Zenith_PackageError : uint8
{
	PACKAGE_ERROR_OUT_OF_MEMORY = 1,
	PACKAGE_ERROR_INVALID_DESC,
	PACKAGE_ERROR_MANIFEST_MISSING,
	PACKAGE_ERROR_MANIFEST_CORRUPT,
	PACKAGE_ERROR_MANIFEST_VERSION,
	PACKAGE_ERROR_MANIFEST_IDENTITY,
	PACKAGE_ERROR_UNKNOWN_PACK,
	PACKAGE_ERROR_NOT_DELIVERABLE,
	PACKAGE_ERROR_DELIVERY_UNAVAILABLE,
	PACKAGE_ERROR_DELIVERY_FAILED,
	PACKAGE_ERROR_NETWORK_UNAVAILABLE,
	PACKAGE_ERROR_INSUFFICIENT_STORAGE,
	PACKAGE_ERROR_ENTITLEMENT_DENIED,
	PACKAGE_ERROR_VERIFY_FAILED,
	PACKAGE_ERROR_PACK_BUSY,
	PACKAGE_ERROR_CANCELLED,
	PACKAGE_ERROR_SHUTTING_DOWN,
	PACKAGE_ERROR_MOUNT_FAILED,
	PACKAGE_ERROR_JOURNAL_CORRUPT,
	PACKAGE_ERROR_JOURNAL_VERSION,
	PACKAGE_ERROR_STATE_FILE_CORRUPT,
	PACKAGE_ERROR_INSTALL_ROOT_READ_ONLY,
	PACKAGE_ERROR_INSTALL_IO,
	PACKAGE_ERROR_INSTALL_LOCKED,
	PACKAGE_ERROR_LOW_DISK,
	PACKAGE_ERROR_PATH_INVALID,
	PACKAGE_ERROR_UPDATE_MANIFEST_INVALID,
	PACKAGE_ERROR_UPDATE_WRONG_TARGET,
	PACKAGE_ERROR_UPDATE_NOT_NEWER,
	PACKAGE_ERROR_UPDATE_FILE_MISMATCH,
	PACKAGE_ERROR_UPDATE_INCOMPLETE,
	PACKAGE_ERROR_DLC_INCOMPATIBLE,
	PACKAGE_ERROR_MIGRATION_STEP_FAILED,
	PACKAGE_ERROR_NOT_SUPPORTED,
	PACKAGE_ERROR_RECORD_CORRUPT,
	PACKAGE_ERROR_PLATFORM,
	PACKAGE_ERROR_REPLAY_CONTENT_MISSING,      // --replay: a unit of the recorded pack set, or a DLC revision, is missing (startup exit code 5)
};
constexpr const char* Zenith_ToString(Zenith_PackageError eError);   // "PACKAGE_ERROR_OUT_OF_MEMORY", …

/// @contract enum Zenith_PackageUnitKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A unit-kind byte outside 1..5 in package.zpkm is PACKAGE_ERROR_MANIFEST_CORRUPT.
/// Version: since Z2 Stage 4. Serialized as uint8 in package.zpkm (Zenith2/Docs/Design/PACK/Formats.md "Package manifest").
/// @endcontract
enum Zenith_PackageUnitKind : uint8
{
	PACKAGE_UNIT_KIND_BASE = 1,          // mounted at startup on every platform
	PACKAGE_UNIT_KIND_INSTALL_TIME,      // Android: installed by Play with the app; Windows: folded into the package
	PACKAGE_UNIT_KIND_FAST_FOLLOW,       // Android: downloaded by Play right after install; Windows: folded
	PACKAGE_UNIT_KIND_ON_DEMAND,         // Android: downloaded on the game's request; Windows: folded
	PACKAGE_UNIT_KIND_DLC,               // entitlement-gated content: an on-demand pack (Android) or a DLC directory (Windows)
};

/// @contract enum Zenith_PackageLayout
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A layout byte outside 1..4 in package.zpkm is PACKAGE_ERROR_MANIFEST_CORRUPT.
/// Version: since Z2 Stage 4. Serialized as uint8 in package.zpkm (Formats.md "Package manifest header").
/// @endcontract
enum Zenith_PackageLayout : uint8
{
	PACKAGE_LAYOUT_WINDOWS = 1,          // Content/: every non-DLC unit folded; DLC as directories
	PACKAGE_LAYOUT_ANDROID_SIDELOAD,     // the sideload APK: every non-DLC unit folded; no DLC
	PACKAGE_LAYOUT_ANDROID_BUNDLE,       // the App Bundle: base and install-time present; fast-follow, on-demand and DLC delivered by Play
	PACKAGE_LAYOUT_SIMULATED,            // development and test packages only: the bundle's unit partition on a Windows or Linux host
};
constexpr const char* Zenith_ToString(Zenith_PackageLayout eLayout);   // "Windows", "AndroidSideload", "AndroidBundle", "Simulated"

/// @contract enum Zenith_ConfirmationReason
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized in the detail byte of Package.ConfirmationRequired.
/// @endcontract
enum Zenith_ConfirmationReason : uint8
{
	CONFIRMATION_REASON_CELLULAR = 1,    // Play WAITING_FOR_WIFI: the user may allow the download over cellular data
	CONFIRMATION_REASON_CONSENT,         // Play REQUIRES_USER_CONFIRMATION: the user must consent to the download
};

/// @contract enum Zenith_AssetPackState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized as uint8 in the payload of Package.PackStateChanged and Package.DlcStateChanged
///   (Formats.md "Package event payload"); values 1 … 19, never renumbered. The transitions are Zenith2/Docs/Design/PACK/Algorithms.md
///   "Asset pack transitions".
/// @endcontract
enum Zenith_AssetPackState : uint8
{
	ASSET_PACK_STATE_UNKNOWN = 1,              // not queried yet
	ASSET_PACK_STATE_NOT_INSTALLED,
	ASSET_PACK_STATE_REQUESTED,                // a fetch was issued; Play has not reported it yet
	ASSET_PACK_STATE_PENDING,                  // Play queued the download
	ASSET_PACK_STATE_DOWNLOADING,
	ASSET_PACK_STATE_WAITING_FOR_NETWORK,      // Play FAILED with NETWORK_ERROR: the installer's own wait for its retry (N)
	ASSET_PACK_STATE_WAITING_FOR_CONFIRMATION, // Play REQUIRES_USER_CONFIRMATION: consent needed (Package.Confirm)
	ASSET_PACK_STATE_TRANSFERRING,
	ASSET_PACK_STATE_DELIVERED,                // Play COMPLETED; waiting for a verification slot
	ASSET_PACK_STATE_VERIFYING,
	ASSET_PACK_STATE_VERIFIED,                 // every archive matches package.zpkm; mounting in the same prologue
	ASSET_PACK_STATE_MOUNTED,
	ASSET_PACK_STATE_CANCELLING,               // m_eTarget: REMOVING or AWAITING_ENTITLEMENT
	ASSET_PACK_STATE_REMOVING,
	ASSET_PACK_STATE_FAILED,                   // m_eError names why; a new fetch request leaves this state
	ASSET_PACK_STATE_WAITING_FOR_WIFI,         // Play WAITING_FOR_WIFI: cellular data needs the user's consent (Package.Confirm)
	ASSET_PACK_STATE_UNMOUNTING,               // sources removed, waiting for their release (X); m_eTarget names the next state
	ASSET_PACK_STATE_AWAITING_ENTITLEMENT,     // held until the unit's entitlement is GRANTED; files kept
	ASSET_PACK_STATE_STOPPING_VERIFICATION,    // a cancelled verification draining its reads and job (V3); m_eTarget names the next state
};
constexpr const char* Zenith_ToString(Zenith_AssetPackState eState);   // "Mounted", the Luau name

/// @contract value Zenith_AssetPackStatus
/// Ownership: Plain value copied out of the installer.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None; a snapshot.
/// Units: Bytes; m_fProgress in [0, 1].
/// Errors: m_eError is 0 unless m_eState is ASSET_PACK_STATE_FAILED or the last attempt failed; it is then a Zenith_PackageError.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_AssetPackStatus
{
	Zenith_AssetPackState m_eState = ASSET_PACK_STATE_UNKNOWN;
	uint8 m_eError = 0;                       // a Zenith_PackageError value or 0
	Zenith_PackageUnitKind m_eKind = PACKAGE_UNIT_KIND_ON_DEMAND;
	Zenith_AssetPackState m_eTarget = ASSET_PACK_STATE_UNKNOWN;   // UNMOUNTING, CANCELLING, STOPPING_VERIFICATION: the next state; else UNKNOWN
	uint32 m_uAttempt = 0;                    // fetches issued since the process started
	uint8 m_uNetworkRetries = 0;              // automatic re-fetches after a network failure in this process (≤ uPACKAGE_MAX_NETWORK_RETRIES)
	bool m_bVerifyRetryUsed = false;           // the one remove-and-refetch after a failed verification is used
	bool m_bPresent = false;                   // the unit has the PRESENT flag: always MOUNTED, never a record
	uint8 m_uReserved = 0;
	uint64 m_ulBytesDone = 0;                 // download bytes, then verified bytes during VERIFYING
	uint64 m_ulBytesTotal = 0;                // package.zpkm's unit bytes
	uint64 m_ulPackVersion = 0;               // Play's pack version; 0 for the simulated delivery's first version
	float m_fProgress = 0.0f;                  // monotonic within one attempt; 1.0 only at VERIFIED or MOUNTED
};

/// @contract value Zenith_InstallIdentity
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized field by field in install-record.zinr (Formats.md "Install record").
/// @endcontract
struct Zenith_InstallIdentity
{
	uint8 m_auBuildID[16] = {};               // Zenith_BuildIdentity::m_auBuildID of the running Player
	uint16 m_uReleaseMajor = 0;
	uint16 m_uReleaseMinor = 0;
	uint16 m_uPatchVersion = 0;               // the mounted patch's PATCH component, 0 without a patch
	uint16 m_uReserved = 0;
	uint32 m_uEngineCookVersion = 0;
	uint64 m_ulContentHash = 0;               // the effective content hash (patched when a patch is mounted)
	uint64 m_ulCompositionHash = 0;
	friend constexpr bool operator==(const Zenith_InstallIdentity&, const Zenith_InstallIdentity&) = default;
};

/// @contract enum Zenith_UpdateKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized as uint8 in install-record.zinr and the payload of Package.Updated.
/// @endcontract
enum Zenith_UpdateKind : uint8
{
	UPDATE_KIND_NONE = 1,                      // identity unchanged since the last run
	UPDATE_KIND_FIRST_RUN,                     // no record: a fresh install, or the user cleared the app's data
	UPDATE_KIND_CONTENT,                       // same build ID, release line and cook version; content hash changed (a patch)
	UPDATE_KIND_WITHIN_LINE,                   // build ID changed, same release line
	UPDATE_KIND_NEW_LINE,                      // a higher release line
	UPDATE_KIND_DOWNGRADE,                     // a lower release line, or a lower patch of the same build
};
constexpr const char* Zenith_ToString(Zenith_UpdateKind eKind);   // "Content", the Luau name

/// @contract enum Zenith_PackageEventKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized as uint8 in the replay payload of each Package.* event.
/// @endcontract
enum Zenith_PackageEventKind : uint8
{
	PACKAGE_EVENT_PACK_STATE_CHANGED = 1,
	PACKAGE_EVENT_UPDATED,
	PACKAGE_EVENT_UPDATE_FAILED,
	PACKAGE_EVENT_DLC_STATE_CHANGED,
	PACKAGE_EVENT_CONFIRMATION_REQUIRED,
};

/// @contract value Zenith_PackageEvent
/// Ownership: Plain value; the service queues at most uPACKAGE_EVENT_QUEUE_CAPACITY of them per frame.
/// Thread affinity: IMMUTABLE; members non-const: the event queue reuses its slots by assignment.
/// Invalidation: None.
/// Units: Bytes; versions as MAJOR.MINOR.PATCH triples.
/// Errors: None.
/// Version: since Z2 Stage 6. 104 bytes in memory; its 96-byte publication payload layout is Formats.md "Package event payload".
/// @endcontract
struct Zenith_PackageEvent
{
	Zenith_PackageEventKind m_eKind = PACKAGE_EVENT_PACK_STATE_CHANGED;
	Zenith_AssetPackState m_eState = ASSET_PACK_STATE_UNKNOWN;   // PACK_STATE_CHANGED, DLC_STATE_CHANGED
	uint8 m_eError = 0;                                         // a Zenith_PackageError or 0
	uint8 m_uDetail = 0;                                        // UPDATED: Zenith_UpdateKind; UPDATE_FAILED: Zenith_InstallTargetKind;
	                                                             // CONFIRMATION_REQUIRED: Zenith_ConfirmationReason; else 0
	uint16 m_auFromVersion[3] = {};                             // UPDATED
	uint16 m_auToVersion[3] = {};
	uint64 m_ulBytesDone = 0;                                   // DLC_STATE_CHANGED: the mounted revision's dlc.zman result
	                                                             // content hash when MOUNTED, else 0 (Formats.md "Package event payload")
	uint64 m_ulBytesTotal = 0;
	char m_acName[65] = {};                                      // the pack or DLC name, NUL-terminated
};
static_assert(sizeof(Zenith_PackageEvent) == 104);

inline constexpr uint32 uPACKAGE_MAX_UNITS = 4096;                 // units in package.zpkm
inline constexpr uint32 uPACKAGE_MAX_ARCHIVES = 65536;             // archives in package.zpkm
inline constexpr uint32 uPACKAGE_MAX_ARCHIVES_PER_UNIT = 100;      // <unit>-00 … <unit>-99
inline constexpr uint32 uPACKAGE_MAX_STRING_BYTES = 1048576;       // string tables of every PACK binary format
inline constexpr uint32 uPACKAGE_MAX_UPDATE_FILES = 65536;         // files listed by one update.zman or dlc.zman
inline constexpr uint32 uPACKAGE_MAX_DELIVERY_PACKS = 256;         // packs the installer tracks
inline constexpr uint32 uPACKAGE_EVENT_QUEUE_CAPACITY = 512;       // events queued between two tick boundaries
inline constexpr uint32 uPACKAGE_MAX_MIGRATION_STEPS = 64;
inline constexpr uint64 ulPACKAGE_MAX_MANIFEST_BYTES = 5374016;     // 64 + 32 × 4,096 + 64 × 65,536 + 1,048,576
inline constexpr uint32 uPACKAGE_MAX_NETWORK_RETRIES = 3;          // automatic re-fetches after a network failure, per pack per process
inline constexpr uint32 uPACKAGE_INSTALL_STATE_BYTES = 48;         // install-state.zins
inline constexpr uint32 uPACKAGE_DEFERRED_EVENTS_PER_WORLD = 64;    // non-state events a world's deferral queue holds
inline constexpr uint32 uPACKAGE_MAX_STATE_UNITS = 256;            // units without the PRESENT flag (check M12): a world's package state and snapshot
inline constexpr uint32 uPACKAGE_MAX_REPORTED_REJECTIONS = 16;     // Package.UpdateFailed records a snapshot carries
inline constexpr uint32 uPACKAGE_FAULT_TAG = 0x504B0000;           // 'PK'
```

## Zenith_PackageManifest header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_PackageManifest.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Sha256.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_Heap;
struct Zenith_BuildIdentity;
enum Zenith_BuildPlatform : uint8;

/// @contract value Zenith_PackageArchiveInfo
/// Ownership: A view element of its manifest.
/// Thread affinity: IMMUTABLE; members non-const: Parse fills the manifest's arrays field by field.
/// Invalidation: With its manifest.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. Formats.md "Package manifest" archive table is its serialized form.
/// @endcontract
struct Zenith_PackageArchiveInfo
{
	Zenith_StringView m_strName;                 // "world_textures-00.zpak"
	uint64 m_ulBytes = 0;
	Zenith_Sha256Value m_xSha256;
	uint32 m_uUnitIndex = 0;
};

/// @contract value Zenith_PackageUnitInfo
/// Ownership: A view element of its manifest.
/// Thread affinity: IMMUTABLE; members non-const: Parse fills the manifest's arrays field by field.
/// Invalidation: With its manifest.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. Formats.md "Package manifest" unit table is its serialized form.
/// @endcontract
struct Zenith_PackageUnitInfo
{
	Zenith_StringView m_strName;                 // "base", "world_textures", "expansion_isles"
	Zenith_StringView m_strEntitlement;          // empty unless the unit is entitlement-gated
	Zenith_PackageUnitKind m_eKind = PACKAGE_UNIT_KIND_BASE;
	sint32 m_iMountOrder = 0;                   // the mount manifest's order of the unit's archives
	uint64 m_ulBytes = 0;                       // the sum of the unit's archive bytes
	Zenith_Span<const Zenith_PackageArchiveInfo> m_axArchives;
	bool m_bOwnCatalogueFragment = false;        // DLC units carry Catalogues/<unit>.zcat inside their archives
	bool m_bPresent = false;                     // flag bit 2: mounted with the package in this layout; no installer record
	bool IsDeliverable() const;   // !m_bPresent and kind FAST_FOLLOW, ON_DEMAND or DLC in an ANDROID_BUNDLE or SIMULATED manifest
};

/// @contract type Zenith_PackageManifest
/// Ownership: Created by Parse as a Zenith_Owned; the package service owns the one parsed from content:/Package/package.zpkm.
/// Thread affinity: IMMUTABLE after Parse; every member ANY_THREAD; private members non-const: Parse fills them after allocating the manifest.
/// Invalidation: Views live as long as the manifest.
/// Units: Bytes.
/// Errors: Parse returns PACKAGE_ERROR_MANIFEST_CORRUPT naming the failed check M1–M11 in the log, PACKAGE_ERROR_MANIFEST_VERSION
///   for a format version other than 1, PACKAGE_ERROR_OUT_OF_MEMORY; CheckIdentity returns PACKAGE_ERROR_MANIFEST_IDENTITY;
///   never an assert.
/// Version: since Z2 Stage 4. The format is Zenith2/Docs/Design/PACK/Formats.md "Package manifest", version 1.
/// @endcontract
class Zenith_PackageManifest
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_PackageManifest>, Zenith_PackageError> Parse(Zenith_ByteSpan xBytes,
		Zenith_Heap& xHeap);
	// m_ulPackageContentHash (the pre-patch identity, PK14) and the layout against the platform, and the build ID only when
	// bCompareBuildID is true (Formats.md "Package manifest checks"): the service passes m_bShippingRules, as PLAT compares a
	// mount manifest's buildId only in Shipping builds.
	Zenith_Result<void, Zenith_PackageError> CheckIdentity(const Zenith_BuildIdentity& xBuild,
		Zenith_BuildPlatform ePlatform, bool bCompareBuildID) const;
	Zenith_PackageLayout GetLayout() const;
	Zenith_Span<const Zenith_PackageUnitInfo> GetUnits() const;           // in file order: base first
	const Zenith_PackageUnitInfo* FindUnit(Zenith_StringView strName) const;   // O(log n), name order index
	const uint8* GetBuildID() const;                                      // 16 bytes
	uint64 GetContentHash() const;
	uint16 GetReleaseMajor() const;
	uint16 GetReleaseMinor() const;
	uint32 GetEngineCookVersion() const;
};
```

## Zenith_AssetPackDelivery header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_AssetPackDelivery.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_Heap;
class Zenith_Platform;
class Zenith_Arena;
class Zenith_FaultInjector;
class Zenith_InstallFileSystem;

/// @contract enum Zenith_DeliveryStatus
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An unrecognised Play status integer maps to DELIVERY_STATUS_UNKNOWN and is counted (package.delivery.unknown_status).
/// Version: since Z2 Stage 6. The values mirror com.google.android.play.core.assetpacks.model.AssetPackStatus plus 1.
/// @endcontract
enum Zenith_DeliveryStatus : uint8
{
	DELIVERY_STATUS_UNKNOWN = 1,                 // AssetPackStatus.UNKNOWN (0)
	DELIVERY_STATUS_PENDING,                     // PENDING (1)
	DELIVERY_STATUS_DOWNLOADING,                 // DOWNLOADING (2)
	DELIVERY_STATUS_TRANSFERRING,                // TRANSFERRING (3)
	DELIVERY_STATUS_COMPLETED,                   // COMPLETED (4)
	DELIVERY_STATUS_FAILED,                      // FAILED (5)
	DELIVERY_STATUS_CANCELED,                    // CANCELED (6)
	DELIVERY_STATUS_WAITING_FOR_WIFI,            // WAITING_FOR_WIFI (7)
	DELIVERY_STATUS_NOT_INSTALLED,               // NOT_INSTALLED (8)
	DELIVERY_STATUS_REQUIRES_USER_CONFIRMATION,  // REQUIRES_USER_CONFIRMATION (9)
	DELIVERY_STATUS_REMOVED,                     // result cell: a removePack success reported by the bridge (no Play status)
	DELIVERY_STATUS_REMOVE_FAILED,               // result cell: a removePack failure reported by the bridge
	DELIVERY_STATUS_REQUEST_FAILED,              // result cell: a fetch or getPackStates task failed (m_uError names why)
	DELIVERY_STATUS_CONFIRMATION_RESULT,         // the confirmation cell: m_uResultCode 1 accepted, 2 declined; m_uError for an error
};

/// @contract enum Zenith_DeliveryErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: An unrecognised Play error integer maps to DELIVERY_ERROR_INTERNAL_ERROR.
/// Version: since Z2 Stage 6. Mapped from com.google.android.play.core.assetpacks.model.AssetPackErrorCode in
///   Zenith2/Docs/Design/PACK/Algorithms.md "Play status and error mapping".
/// @endcontract
enum Zenith_DeliveryErrorCode : uint8
{
	DELIVERY_ERROR_API_NOT_AVAILABLE = 1,
	DELIVERY_ERROR_PACK_UNAVAILABLE,
	DELIVERY_ERROR_INVALID_REQUEST,
	DELIVERY_ERROR_DOWNLOAD_NOT_FOUND,
	DELIVERY_ERROR_ACCESS_DENIED,
	DELIVERY_ERROR_NETWORK_ERROR,
	DELIVERY_ERROR_APP_NOT_OWNED,
	DELIVERY_ERROR_INSUFFICIENT_STORAGE,
	DELIVERY_ERROR_PLAY_STORE_NOT_FOUND,
	DELIVERY_ERROR_NETWORK_UNRESTRICTED,
	DELIVERY_ERROR_APP_UNAVAILABLE,
	DELIVERY_ERROR_CONFIRMATION_NOT_REQUIRED,
	DELIVERY_ERROR_UNRECOGNIZED_INSTALLATION,
	DELIVERY_ERROR_INTERNAL_ERROR,
};

/// @contract value Zenith_DeliveryUpdate
/// Ownership: Plain value; the delivery keeps the latest one per pack until Poll copies it out.
/// Thread affinity: IMMUTABLE; members non-const: the delivery overwrites the latest one per pack.
/// Invalidation: None.
/// Units: Bytes; m_uTransferPercent 0..100.
/// Errors: m_uError is a Zenith_DeliveryErrorCode when m_eStatus is FAILED, REMOVE_FAILED, REQUEST_FAILED or a failed
///   CONFIRMATION_RESULT, else 0.
/// Version: since Z2 Stage 6. 104 bytes; one per cell of Algorithms.md "Delivery update slots" (state, result, confirmation).
/// @endcontract
struct Zenith_DeliveryUpdate
{
	char m_acPack[65] = {};                      // NUL-terminated, at most 64 bytes; empty for the confirmation cell
	Zenith_DeliveryStatus m_eStatus = DELIVERY_STATUS_UNKNOWN;
	uint8 m_uError = 0;
	uint8 m_uTransferPercent = 0;
	uint8 m_uResultCode = 0;                    // CONFIRMATION_RESULT: 1 accepted (RESULT_OK), 2 declined (RESULT_CANCELED); else 0
	uint64 m_ulBytesDownloaded = 0;
	uint64 m_ulTotalBytesToDownload = 0;
	uint64 m_ulPackVersion = 0;
	uint64 m_ulSequence = 0;                    // per delivery, strictly increasing across all packs
};
static_assert(sizeof(Zenith_DeliveryUpdate) == 104);

/// @contract value Zenith_PackLocation
/// Ownership: The host path view lives in the caller's arena.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: With the arena.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_PackLocation
{
	Zenith_StringView m_strAssetsHostPath;       // Play's AssetPackLocation.assetsPath(): the directory holding the pack's archives
	uint64 m_ulPackVersion = 0;
};

/// @contract enum Zenith_DeliveryBackend
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_DeliveryBackend : uint8
{
	DELIVERY_BACKEND_PLAY = 1,
	DELIVERY_BACKEND_SIMULATED,
};

/// @contract interface Zenith_AssetPackDelivery
/// Ownership: Created by Zenith_CreateAssetPackDelivery_Play or Zenith_CreateAssetPackDelivery_Simulated; owned by the
///   package service, destroyed in shutdown step 6 after the installer.
/// Thread affinity: MAIN_THREAD for every member. The Play implementation receives Play's callbacks on the Android UI
///   thread and Play's task threads and only writes its update slot table under uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES (650);
///   its destructor returns only after the Java bridge's shutdown() returned, after which no native runs (Tooling.md "Java bridge").
/// Invalidation: Pack names passed in are copied; GetLocation's view lives in the caller's arena.
/// Units: Bytes.
/// Errors: Requests return PACKAGE_ERROR_UNKNOWN_PACK (a name not declared at creation), PACKAGE_ERROR_DELIVERY_UNAVAILABLE
///   (the Java bridge or Play is missing), PACKAGE_ERROR_PLATFORM (a JNI call failed), PACKAGE_ERROR_SHUTTING_DOWN;
///   asynchronous outcomes arrive as Zenith_DeliveryUpdate records from Poll.
/// Reentrancy: NOT_APPLICABLE. The interface invokes no callbacks; updates are polled.
/// Version: since Z2 Stage 6. Source API.
/// @endcontract
class Zenith_AssetPackDelivery
{
public:
	virtual ~Zenith_AssetPackDelivery() = default;
	virtual Zenith_DeliveryBackend GetBackend() const = 0;
	virtual Zenith_Result<void, Zenith_PackageError> RequestStates(Zenith_Span<const Zenith_StringView> axPacks) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> Fetch(Zenith_Span<const Zenith_StringView> axPacks) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> Cancel(Zenith_Span<const Zenith_StringView> axPacks) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> Remove(Zenith_StringView strPack) = 0;
	// Play's showConfirmationDialog(Activity) for every pack waiting for Wi-Fi or consent; the answer arrives in the confirmation cell.
	virtual Zenith_Result<void, Zenith_PackageError> ShowConfirmation() = 0;
	// Synchronous: Play's getPackLocation; PACKAGE_ERROR_UNKNOWN_PACK when the pack is not on disk.
	virtual Zenith_Result<Zenith_PackLocation, Zenith_PackageError> GetLocation(Zenith_StringView strPack,
		Zenith_Arena& xArena) = 0;
	// Copies out every cell (state, result, confirmation) written since the previous Poll, in ascending m_ulSequence;
	// axOut holds at least 2 × the pack count + 1 entries.
	virtual uint32 Poll(Zenith_Span<Zenith_DeliveryUpdate> axOut) = 0;
};

/// @contract value Zenith_AssetPackDeliveryDesc
/// Ownership: Plain value; the pack name span and the platform are borrowed for the factory call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by the factories (PACKAGE_ERROR_INVALID_DESC: no pack, more than uPACKAGE_MAX_DELIVERY_PACKS, a bad name).
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_AssetPackDeliveryDesc
{
	Zenith_Span<const Zenith_StringView> m_axPacks;       // every FAST_FOLLOW, ON_DEMAND and Android DLC unit of package.zpkm
	Zenith_Platform* m_pxPlatform = nullptr;              // Play only: GetAndroidJavaContext (the JavaVM, the activity, its class loader; PK14)
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
};

inline constexpr uint32 uFAULT_POINT_PACKAGE_PLAY_LOAD_CLASS = 0x504B0001;     // the class loader's loadClass("org.zenith2.pack.Zenith2AssetPacks")
inline constexpr uint32 uFAULT_POINT_PACKAGE_PLAY_INIT = 0x504B0002;           // the bridge's constructor and init()
inline constexpr uint32 uFAULT_POINT_PACKAGE_PLAY_REGISTER_NATIVES = 0x504B0003;

// Android: the Play Asset Delivery bridge; Windows and Linux: returns PACKAGE_ERROR_NOT_SUPPORTED (Internal/<Platform>/ stubs).
Zenith_Result<Zenith_Owned<Zenith_AssetPackDelivery>, Zenith_PackageError> Zenith_CreateAssetPackDelivery_Play(
	const Zenith_AssetPackDeliveryDesc& xDesc);

/// @contract enum Zenith_SimulatedFaultKind
/// Ownership: Plain value held in the simulated delivery's fault script.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes of the pack's download.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_SimulatedFaultKind : uint8
{
	SIMULATED_FAULT_NETWORK_LOSS = 1,        // at m_ulAtByte: FAILED with NETWORK_ERROR; every fetch fails so until ResolveNetworkLoss
	SIMULATED_FAULT_FAIL,                    // at m_ulAtByte: FAILED with m_eError
	SIMULATED_FAULT_CORRUPT_BYTE,            // the byte at m_ulAtByte of the delivered pack is XOR 0x5A (verification must fail)
	SIMULATED_FAULT_TRUNCATE,                // the delivered archive ends at m_ulAtByte (verification must fail)
	SIMULATED_FAULT_REQUIRE_CONFIRMATION,    // at m_ulAtByte: REQUIRES_USER_CONFIRMATION until ShowConfirmation is accepted
	SIMULATED_FAULT_REMOVE_FAILS,            // the next Remove of the pack reports REMOVE_FAILED once
	SIMULATED_FAULT_WAIT_FOR_WIFI,           // at m_ulAtByte: WAITING_FOR_WIFI until ShowConfirmation is accepted
};

/// @contract value Zenith_SimulatedFault
/// Ownership: Plain value; Zenith_CreateAssetPackDelivery_Simulated copies the desc's span, so the caller's array may die after it.
/// Thread affinity: IMMUTABLE; members non-const: the simulated delivery copies the desc's span into its own array.
/// Invalidation: None.
/// Units: m_ulAtByte in bytes of the pack's download; m_uAttempt counts fetch attempts from 1.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_SimulatedFault
{
	Zenith_SimulatedFaultKind m_eKind = SIMULATED_FAULT_NETWORK_LOSS;
	Zenith_DeliveryErrorCode m_eError = DELIVERY_ERROR_NETWORK_ERROR;
	uint16 m_uAttempt = 1;                  // the fetch attempt it applies to (1 = the first)
	uint64 m_ulAtByte = 0;
	char m_acPack[65] = {};
};

/// @contract value Zenith_SimulatedDeliveryDesc
/// Ownership: Plain value; the file system and the strings are borrowed and outlive the delivery.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Bytes per Poll.
/// Errors: Validated by Zenith_CreateAssetPackDelivery_Simulated (PACKAGE_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_SimulatedDeliveryDesc
{
	Zenith_AssetPackDeliveryDesc m_xBase;
	Zenith_InstallFileSystem* m_pxFiles = nullptr;  // copying mode (tests only): where "Play" keeps packs; null: the in-place mode the runtime uses
	Zenith_StringView m_strSourceRoot;              // <root>/<pack>/<archive>: the packs as the bundle holds them
	Zenith_StringView m_strTargetRoot;              // copying mode: <root>/<pack>/<packVersion>/assets/<archive>, Play's layout; else empty
	uint64 m_ulBytesPerPoll = 8388608;             // package.simulatedDelivery.bytesPerFrame
	uint64 m_ulPackVersion = 1;
	Zenith_Span<const Zenith_SimulatedFault> m_axFaults;   // copied
	bool m_bFastFollowStartsAtCreate = true;        // Play starts fast-follow downloads after install by itself
};

/// @contract type Zenith_SimulatedAssetPackDelivery
/// Ownership: Created by Zenith_CreateAssetPackDelivery_Simulated; owned like any delivery. Tests keep a borrowed
///   pointer to the concrete type for its control members.
/// Thread affinity: MAIN_THREAD for every member; it starts no thread, so it runs deterministically, advancing only in Poll.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: As Zenith_AssetPackDelivery; the control members return PACKAGE_ERROR_UNKNOWN_PACK for an undeclared pack.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. Source API; a real delivery used by tests (copying mode), the linux-tsan host and development
///   builds (in-place mode, Algorithms.md "Simulated delivery"); refused in Shipping builds by the bridge.
/// @endcontract
class Zenith_SimulatedAssetPackDelivery final : public Zenith_AssetPackDelivery
{
public:
	Zenith_DeliveryBackend GetBackend() const override;   // DELIVERY_BACKEND_SIMULATED
	Zenith_Result<void, Zenith_PackageError> RequestStates(Zenith_Span<const Zenith_StringView> axPacks) override;
	Zenith_Result<void, Zenith_PackageError> Fetch(Zenith_Span<const Zenith_StringView> axPacks) override;
	Zenith_Result<void, Zenith_PackageError> Cancel(Zenith_Span<const Zenith_StringView> axPacks) override;
	Zenith_Result<void, Zenith_PackageError> Remove(Zenith_StringView strPack) override;
	Zenith_Result<void, Zenith_PackageError> ShowConfirmation() override;
	Zenith_Result<Zenith_PackLocation, Zenith_PackageError> GetLocation(Zenith_StringView strPack,
		Zenith_Arena& xArena) override;
	uint32 Poll(Zenith_Span<Zenith_DeliveryUpdate> axOut) override;
	// control
	Zenith_Result<void, Zenith_PackageError> ResolveNetworkLoss(Zenith_StringView strPack);
	void SetConfirmationAnswer(bool bAccept);     // the next ShowConfirmation's answer; true by default
	// Models the process being killed and relaunched: forgets the requester's state, keeps "Play's" on-disk state and any
	// download in progress (Play keeps downloading in its own service), and continues from the same byte.
	void SimulateRequesterRestart();
	uint64 GetDeliveredBytes(Zenith_StringView strPack) const;
	uint32 GetRemoveCount(Zenith_StringView strPack) const;
};

Zenith_Result<Zenith_Owned<Zenith_SimulatedAssetPackDelivery>, Zenith_PackageError> Zenith_CreateAssetPackDelivery_Simulated(
	const Zenith_SimulatedDeliveryDesc& xDesc);
```

## Zenith_EntitlementQuery header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_EntitlementQuery.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_Heap;

/// @contract enum Zenith_EntitlementState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_EntitlementState : uint8
{
	ENTITLEMENT_STATE_UNKNOWN = 1,           // not resolved yet (an online query in flight); DLC stays unmounted, no error
	ENTITLEMENT_STATE_GRANTED,
	ENTITLEMENT_STATE_DENIED,
};

/// @contract interface Zenith_EntitlementQuery
/// Ownership: Implemented by the ONLINE adapter (S6), by Zenith_ConfigEntitlementQuery and by Zenith_ForwardingEntitlementQuery;
///   the package service is given the runtime bridge's forwarder, which outlives it, and the forwarder borrows its provider
///   (Algorithms.md "Entitlement provider").
/// Thread affinity: MAIN_THREAD. The package service calls it at Create (Windows DLC mounting, the guard on automatic fetches)
///   and in the prologue.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unresolvable entitlement answers ENTITLEMENT_STATE_UNKNOWN.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. Source API; a lower-declared interface (Zenith2/Docs/Contracts.md "Layering DAG").
/// @endcontract
class Zenith_EntitlementQuery
{
public:
	virtual ~Zenith_EntitlementQuery() = default;
	virtual Zenith_EntitlementState GetEntitlement(Zenith_StringView strEntitlementID) const = 0;
	// Increments whenever any answer may have changed (sign-in, account switch, purchase); the service re-evaluates DLC then.
	virtual uint64 GetRevision() const = 0;
};

/// @contract type Zenith_ConfigEntitlementQuery
/// Ownership: Created by Create; owned by the composition root when no online adapter supplies entitlements.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns PACKAGE_ERROR_INVALID_DESC for an ID that is not ^[a-z][a-z0-9_.]{0,63}$, PACKAGE_ERROR_OUT_OF_MEMORY.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. Answers GRANTED for every ID of the package.grantedEntitlements configuration key, DENIED otherwise.
/// @endcontract
class Zenith_ConfigEntitlementQuery final : public Zenith_EntitlementQuery
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ConfigEntitlementQuery>, Zenith_PackageError> Create(
		Zenith_Span<const Zenith_StringView> axGranted, Zenith_Heap& xHeap);
	Zenith_EntitlementState GetEntitlement(Zenith_StringView strEntitlementID) const override;
	uint64 GetRevision() const override;   // always 1
};

/// @contract type Zenith_ForwardingEntitlementQuery
/// Ownership: Created by Create; owned by ZenithRuntime's package bridge from the closing action of startup step 11 until
///   shutdown step 6; borrows its provider, which the bridge sets and clears (Algorithms.md "Entitlement provider").
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None; a provider set with SetProvider must outlive the next SetProvider or the forwarder.
/// Units: None.
/// Errors: None; with no provider every ID answers ENTITLEMENT_STATE_UNKNOWN.
/// Reentrancy: NONE. GetEntitlement and GetRevision call the provider's members of the same names and nothing else.
/// Version: since Z2 Stage 6. GetRevision is (provider changes) x 2^48 + (the provider's revision mod 2^48), so every
///   SetProvider is a revision change.
/// @endcontract
class Zenith_ForwardingEntitlementQuery final : public Zenith_EntitlementQuery
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_ForwardingEntitlementQuery>, Zenith_PackageError> Create(Zenith_Heap& xHeap);
	void SetProvider(const Zenith_EntitlementQuery* pxProvider);   // null: no provider (every ID UNKNOWN); counts a change
	const Zenith_EntitlementQuery* GetProvider() const;
	Zenith_EntitlementState GetEntitlement(Zenith_StringView strEntitlementID) const override;
	uint64 GetRevision() const override;
};
```

## Zenith_AssetPackInstaller header

The class name is the one DecisionLog Z2-D-027 commits to.

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_AssetPackInstaller.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_PackageManifest;
class Zenith_AssetPackDelivery;
class Zenith_EntitlementQuery;
class Zenith_VirtualFileSystem;
class Zenith_AsyncIO;
class Zenith_JobSystem;
class Zenith_Heap;
class Zenith_FaultInjector;
struct Zenith_ArchiveReaderDesc;

/// @contract value Zenith_AssetPackInstallerDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the installer.
/// Thread affinity: IMMUTABLE; members non-const: Zenith_PackageService::Create fills the pointers of the copy nested in its desc.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Validated by Create (PACKAGE_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_AssetPackInstallerDesc
{
	const Zenith_PackageManifest* m_pxManifest = nullptr;
	Zenith_AssetPackDelivery* m_pxDelivery = nullptr;
	const Zenith_EntitlementQuery* m_pxEntitlements = nullptr;
	Zenith_VirtualFileSystem* m_pxFiles = nullptr;
	Zenith_AsyncIO* m_pxIO = nullptr;
	Zenith_JobSystem* m_pxJobs = nullptr;
	const Zenith_ArchiveReaderDesc* m_pxArchiveReader = nullptr;   // ZenithAssets' archive factory user data
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
	uint32 m_uVerifyChunkBytes = 1048576;            // package.verifyChunkBytes
	uint32 m_uVerifyBuffersPerArchive = 4;           // package.verifyBuffersPerArchive
	uint32 m_uMaxConcurrentVerifications = 2;        // package.maxConcurrentVerifications
	uint32 m_uNetworkRetrySeconds = 30;              // package.networkRetrySeconds
	bool m_bAutoFetchFastFollow = true;               // package.autoFetchFastFollow
	bool m_bRemoveOnVerifyFailure = true;             // package.removeOnVerifyFailure
	bool m_bReverifyOnStartup = false;                // package.reverifyOnStartup: no cache check, every delivered pack hashed in full
	bool m_bReplay = false;                           // --replay: no automatic F or R, no resume; only m_axReplayUnits mount
	Zenith_Span<const Zenith_StringView> m_axReplayUnits;   // R of Algorithms.md "Replay pack set"; copied at Create
};

/// @contract value Zenith_InstallerFrameInput
/// Ownership: Plain value built by the runtime bridge each prologue.
/// Thread affinity: IMMUTABLE; members non-const: the runtime bridge fills it field by field each prologue.
/// Invalidation: None.
/// Units: Microseconds of Zenith_GetMonotonicTimeUs.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_InstallerFrameInput
{
	uint64 m_ulNowUs = 0;                    // the prologue's monotonic time; tests pass any value
	bool m_bForeground = true;                // Zenith_AppLifecycle::GetState() == APP_STATE_RUNNING
	bool m_bConnectivityRegained = false;     // Zenith_Platform::GetConnectivityRevision() advanced since the last prologue
};

inline constexpr uint32 uFAULT_POINT_PACKAGE_INSTALLER_STATE_READ = 0x504B0010;
inline constexpr uint32 uFAULT_POINT_PACKAGE_INSTALLER_COMPLETION_QUEUE = 0x504B0011;
inline constexpr uint32 uFAULT_POINT_PACKAGE_INSTALLER_INITIAL_QUERY = 0x504B0012;

/// @contract value Zenith_InstallerPrologueResult
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_InstallerPrologueResult
{
	bool m_bMountsChanged = false;            // the caller calls Zenith_AssetRegistry::OnMountsChanged
	uint32 m_uEventsQueued = 0;
	uint32 m_uUpdatesApplied = 0;
};

/// @contract type Zenith_AssetPackInstaller
/// Ownership: Created by Create as a Zenith_Owned; owned by Zenith_PackageService; one per process.
/// Thread affinity: MAIN_THREAD for every member. Verification runs as background jobs (bg/Package.Verify) whose results
///   return through operation handles the installer polls; reads go through Zenith_AsyncIO's I/O threads.
/// Invalidation: Status snapshots are values; nothing returned references installer storage.
/// Units: Bytes; progress in [0, 1].
/// Errors: Request members return PACKAGE_ERROR_UNKNOWN_PACK, PACKAGE_ERROR_NOT_DELIVERABLE (a base or install-time unit),
///   PACKAGE_ERROR_ENTITLEMENT_DENIED (a DLC unit the query denies), PACKAGE_ERROR_PACK_BUSY (the BUSY cells of
///   Algorithms.md "Asset pack transitions"), PACKAGE_ERROR_SHUTTING_DOWN; asynchronous failures surface as
///   ASSET_PACK_STATE_FAILED with m_eError. Never an assert on delivery input.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Locks: none of its own; its delivery takes uLOCK_LEVEL_PACKAGE_DELIVERY_UPDATES (650) inside Poll.
/// Version: since Z2 Stage 6. The state machine is Zenith2/Docs/Design/PACK/Algorithms.md "Asset pack state machine".
/// @endcontract
class Zenith_AssetPackInstaller
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AssetPackInstaller>, Zenith_PackageError> Create(
		const Zenith_AssetPackInstallerDesc& xDesc);   // reads user:/cache/package/delivery-state.zdls
	~Zenith_AssetPackInstaller();

	Zenith_Result<void, Zenith_PackageError> RequestFetch(Zenith_StringView strPack);
	Zenith_Result<void, Zenith_PackageError> RequestCancel(Zenith_StringView strPack);
	Zenith_Result<void, Zenith_PackageError> RequestRemove(Zenith_StringView strPack);
	Zenith_Result<void, Zenith_PackageError> RequestConfirmation();
	Zenith_Result<Zenith_AssetPackStatus, Zenith_PackageError> GetStatus(Zenith_StringView strPack) const;
	uint32 GetPackCount() const;
	Zenith_StringView GetPackName(uint32 uIndex) const;

	// Frame prologue step 1b: Poll the delivery, raise E, G, S and N, advance every pack's state machine, pump
	// verification, apply mount changes.
	Zenith_InstallerPrologueResult RunPrologue(const Zenith_InstallerFrameInput& xFrame,
		Zenith_Span<Zenith_PackageEvent> axEventsOut);

	// Shutdown step 4: the shutdown rule of Algorithms.md "Asset pack transitions": MOUNTED -> UNMOUNTING (NOT_INSTALLED) with A5,
	// VERIFYING -> STOPPING_VERIFICATION (DELIVERED) with A7, intents kept. Afterwards no delivery call is issued, A3 and A4
	// are suppressed (VFY resolves to DELIVERED), S, E, G and N are never raised, so no verification starts, no pack leaves
	// DELIVERED and nothing is mounted (no AddSource); an UNMOUNTING pack ends at its target at X (DELIVERED, NOT_INSTALLED,
	// REMOVING or AWAITING_ENTITLEMENT) with no call issued; every request returns PACKAGE_ERROR_SHUTTING_DOWN.
	void BeginShutdown();
	// Drains the completion queue and the finished hash jobs (V3), compares each current release block with its expected
	// count (X), applies their cells and drops released draining blocks; returns IsShutdownComplete(). Only after BeginShutdown.
	bool PumpShutdown();
	bool IsShutdownComplete() const;   // no pack VERIFYING, STOPPING_VERIFICATION or UNMOUNTING
};
```

## Zenith_InstallFileSystem header

The file system the applier (and, in tests, the simulated delivery) writes through. It is a lower-declared interface so tests run the real applier against an in-memory implementation that models power loss exactly as `Zenith2/Docs/Design/PACK/Algorithms.md → "Durability model"` states.

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_InstallFileSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"
#include "ZenithPlatform/Zenith_FileIdentity.h"   // Zenith_FileIdentityStamp (PLAT, cross-area row PK27 (a))

class Zenith_Heap;
class Zenith_Arena;
class Zenith_FaultInjector;

/// @contract enum Zenith_InstallOpenMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_InstallOpenMode : uint8
{
	INSTALL_OPEN_MODE_READ = 1,
	INSTALL_OPEN_MODE_CREATE_NEW,           // fails PACKAGE_ERROR_INSTALL_IO when the name exists; a metadata operation
	INSTALL_OPEN_MODE_READ_WRITE,           // an existing file, opened for update (and for FlushFile, which needs write access)
};

/// @contract enum Zenith_InstallLock
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Zenith2/Docs/Design/PACK/Algorithms.md "Install lock and instance guard".
/// @endcontract
enum Zenith_InstallLock : uint8
{
	INSTALL_LOCK_APPLY = 1,                 // <root>/.zenith2-install/lock: one installer at a time
	INSTALL_LOCK_RUNNING,                   // <root>/.zenith2-install/running: shared by every running instance
};

/// @contract enum Zenith_InstallLockMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_InstallLockMode : uint8
{
	INSTALL_LOCK_MODE_SHARED = 1,
	INSTALL_LOCK_MODE_EXCLUSIVE,
};

/// @contract enum Zenith_InstallVolumeKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. The supported set per platform is Algorithms.md "Pending installs at startup" step 1.
/// @endcontract
enum Zenith_InstallVolumeKind : uint8
{
	INSTALL_VOLUME_KIND_UNSUPPORTED = 1,    // FAT32, exFAT, a network share, anything not listed below
	INSTALL_VOLUME_KIND_NTFS,
	INSTALL_VOLUME_KIND_REFS,
	INSTALL_VOLUME_KIND_EXT4,
	INSTALL_VOLUME_KIND_F2FS,
	INSTALL_VOLUME_KIND_XFS,
	INSTALL_VOLUME_KIND_BTRFS,
	INSTALL_VOLUME_KIND_TMPFS,
};

/// @contract value Zenith_InstallFile
/// Ownership: A per-open token of one file system; closed by Close; never shared between threads.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after Close or the file system's destruction.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_InstallFile { uint64 m_aulOpaque[2] = {}; };

/// @contract value Zenith_InstallStat
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_InstallStat
{
	uint64 m_ulBytes = 0;
	bool m_bDirectory = false;                // a directory that is not a link
	bool m_bLink = false;                     // a symbolic link, a junction or any other reparse point; never followed
};

using Zenith_PfnInstallListVisitor = void (*)(Zenith_StringView strName, const Zenith_InstallStat& xStat, void* pUserData);

/// @contract interface Zenith_InstallFileSystem
/// Ownership: Created by Zenith_CreateHostInstallFileSystem or as a Zenith_MemoryInstallFileSystem; owned by the runtime
///   bridge from the closing action of startup step 4 until shutdown step 10b (it holds the instance guard), or by a test.
/// Thread affinity: OWNER_THREAD. The applier uses it on the main thread before the job system exists.
/// Invalidation: Zenith_InstallFile tokens die with Close; List's views are valid during the visitor call only.
/// Units: Bytes; paths are relative to the root, '/'-separated, each component valid by rules R2–R9 of
///   Zenith2/Docs/Design/PLAT/Filesystem.md "Validation rules and order" (a leading '.' is allowed for the install
///   directories .trash, .incoming and .zenith2-install, and a delivery-in-progress name under .incoming).
/// Errors: PACKAGE_ERROR_PATH_INVALID (also: a link met while opening or traversing), PACKAGE_ERROR_INSTALL_IO (the OS
///   code logged), PACKAGE_ERROR_LOW_DISK, PACKAGE_ERROR_INSTALL_ROOT_READ_ONLY (access denied on the root),
///   PACKAGE_ERROR_INSTALL_LOCKED; never an assert.
/// Reentrancy: NOT_APPLICABLE except List, whose visitor may not call the file system.
/// Version: since Z2 Stage 6. Source API.
/// @endcontract
class Zenith_InstallFileSystem
{
public:
	virtual ~Zenith_InstallFileSystem() = default;
	virtual Zenith_Result<Zenith_InstallStat, Zenith_PackageError> Stat(Zenith_StringView strPath) = 0;   // never follows a link
	virtual Zenith_Result<void, Zenith_PackageError> List(Zenith_StringView strDirectory,
		Zenith_PfnInstallListVisitor pfnVisitor, void* pUserData) = 0;            // byte order of names; links reported, not followed
	virtual Zenith_Result<Zenith_InstallFile, Zenith_PackageError> Open(Zenith_StringView strPath,
		Zenith_InstallOpenMode eMode) = 0;                                       // a link is PATH_INVALID
	virtual Zenith_Result<uint64, Zenith_PackageError> Read(Zenith_InstallFile& xFile, uint64 ulOffset,
		Zenith_MutableByteSpan xDestination) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> Write(Zenith_InstallFile& xFile, uint64 ulOffset,
		Zenith_ByteSpan xBytes) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> Truncate(Zenith_InstallFile& xFile, uint64 ulBytes) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> FlushFile(Zenith_InstallFile& xFile) = 0;   // durable data; READ_WRITE or CREATE_NEW only
	virtual void Close(Zenith_InstallFile& xFile) = 0;
	// Atomic within one volume; bReplace false fails PACKAGE_ERROR_INSTALL_IO when the target exists. Files and directories.
	virtual Zenith_Result<void, Zenith_PackageError> Rename(Zenith_StringView strFrom, Zenith_StringView strTo,
		bool bReplace) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> RemoveFile(Zenith_StringView strPath) = 0;   // missing: ok; a link: PATH_INVALID
	virtual Zenith_Result<void, Zenith_PackageError> RemoveLink(Zenith_StringView strPath) = 0;   // the link itself, never its target; missing: ok
	virtual Zenith_Result<void, Zenith_PackageError> CreateDirectory(Zenith_StringView strPath) = 0;   // existing: ok
	virtual Zenith_Result<void, Zenith_PackageError> RemoveEmptyDirectory(Zenith_StringView strPath) = 0;
	virtual Zenith_Result<void, Zenith_PackageError> FlushDirectory(Zenith_StringView strPath) = 0;   // durable names
	virtual Zenith_Result<uint64, Zenith_PackageError> QueryFreeBytes() = 0;
	virtual Zenith_Result<Zenith_InstallVolumeKind, Zenith_PackageError> QueryVolumeKind() = 0;
	// Non-blocking; creates .zenith2-install/ and the lock file when absent; held until ReleaseLock or the process ends.
	// A second acquisition of a lock this object already holds is PACKAGE_ERROR_INVALID_DESC. On a root that denies writes, a
	// SHARED request opens an existing lock file read-only and locks it shared; a missing file or an EXCLUSIVE request is
	// PACKAGE_ERROR_INSTALL_ROOT_READ_ONLY (Algorithms.md "Durability model", host mapping).
	virtual Zenith_Result<void, Zenith_PackageError> AcquireLock(Zenith_InstallLock eLock,
		Zenith_InstallLockMode eMode) = 0;
	virtual void ReleaseLock(Zenith_InstallLock eLock) = 0;
	virtual bool HoldsLock(Zenith_InstallLock eLock, Zenith_InstallLockMode eMode) const = 0;
	// The identity stamp of an open file (Algorithms.md "Verified cache" item 1: size, last-write and change time in ns since
	// the Unix epoch, volume, 128-bit file identity), with exactly the host calls of PLAT's directory source, so the stamp of a
	// file taken here equals the one Zenith_MountSource::GetIdentityStamp takes of it (cross-area row PK27 (a)). Neither a
	// metadata nor a data operation. PACKAGE_ERROR_INSTALL_IO when the OS call fails; PACKAGE_ERROR_NOT_SUPPORTED when the root's
	// volume is outside the supported set of Algorithms.md "Durability model" (the stamp is then never recorded).
	virtual Zenith_Result<Zenith_FileIdentityStamp, Zenith_PackageError> GetIdentityStamp(Zenith_InstallFile& xFile) = 0;
	// The current time on the stamps' scale, for the 100 ms racy-timestamp rule (Algorithms.md "Verified cache" item 2): ns since
	// the Unix epoch from GetSystemTimePreciseAsFileTime (Windows) or clock_gettime(CLOCK_REALTIME) (Linux, Android).
	virtual uint64 GetCurrentFileTimeNs() const = 0;
};

/// @contract value Zenith_HostInstallFileSystemDesc
/// Ownership: Plain value; the root view is borrowed for the factory call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by the factory.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_HostInstallFileSystemDesc
{
	Zenith_StringView m_strRootHostPath;     // Windows <install> (Zenith_Platform::GetBasePath()); Android the internal storage root (GetPrefPath(), PK14)
	Zenith_InstallVolumeKind m_eVolumeKindOverride = static_cast<Zenith_InstallVolumeKind>(0);   // tests only: 0 queries the OS; any other
	                                         // value is what QueryVolumeKind and the stamp gate of GetIdentityStamp see
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
};

inline constexpr uint32 uFAULT_POINT_PACKAGE_HOST_FS_ROOT_OPEN = 0x504B0020;

// Internal/Windows/Zenith_HostInstallFileSystem_Windows.cpp, Internal/Android/…_Android.cpp, Internal/Linux/…_Linux.cpp;
// the operation mapping is Algorithms.md "Durability model" (host mapping table), including the Windows transient retries.
Zenith_Result<Zenith_Owned<Zenith_InstallFileSystem>, Zenith_PackageError> Zenith_CreateHostInstallFileSystem(
	const Zenith_HostInstallFileSystemDesc& xDesc);

/// @contract enum Zenith_MemoryFileDurability
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_MemoryFileDurability : uint8
{
	MEMORY_FILE_DURABILITY_DURABLE = 1,     // part of the state at creation: survives every simulated power loss
	MEMORY_FILE_DURABILITY_UNFLUSHED,       // created by ordinary CreateDirectory, Open, Write, Close with no flush (a third party's delivery)
};

/// @contract enum Zenith_MemoryFsCall
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
enum Zenith_MemoryFsCall : uint8
{
	MEMORY_FS_CALL_STAT = 1, MEMORY_FS_CALL_LIST, MEMORY_FS_CALL_OPEN, MEMORY_FS_CALL_READ, MEMORY_FS_CALL_WRITE, MEMORY_FS_CALL_TRUNCATE,
	MEMORY_FS_CALL_FLUSH_FILE, MEMORY_FS_CALL_CLOSE, MEMORY_FS_CALL_RENAME, MEMORY_FS_CALL_REMOVE_FILE, MEMORY_FS_CALL_REMOVE_LINK,
	MEMORY_FS_CALL_CREATE_DIRECTORY, MEMORY_FS_CALL_REMOVE_EMPTY_DIRECTORY, MEMORY_FS_CALL_FLUSH_DIRECTORY, MEMORY_FS_CALL_QUERY_FREE_BYTES,
	MEMORY_FS_CALL_QUERY_VOLUME_KIND, MEMORY_FS_CALL_ACQUIRE_LOCK, MEMORY_FS_CALL_RELEASE_LOCK, MEMORY_FS_CALL_CREATE_LINK,
};

/// @contract value Zenith_MemoryFsTraceEntry
/// Ownership: A view element of the memory file system's trace.
/// Thread affinity: IMMUTABLE; members non-const: the trace stores it by assignment.
/// Invalidation: Until the next operation on the file system.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_MemoryFsTraceEntry
{
	uint64 m_ulOperation = 0;             // the operation number (every call, from 1)
	uint64 m_ulMetadataOperation = 0;     // the metadata number, 0 for a call that is not a metadata operation
	Zenith_MemoryFsCall m_eCall = MEMORY_FS_CALL_STAT;
	uint32 m_uProcess = 1;                // the process view that issued it
	Zenith_StringView m_strPath;           // the (first) path; empty when none
	Zenith_StringView m_strPath2;          // the rename target; empty otherwise
};

/// @contract type Zenith_MemoryInstallFileSystem
/// Ownership: Created by Create as a Zenith_Owned; owned by a test or by the simulated delivery's caller. Process views
///   created by CreateProcessView share its tree, are owned by their caller and must be destroyed before it.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: As Zenith_InstallFileSystem; GetTrace's span until the next operation.
/// Units: Bytes; operations are counted from 1 in call order, metadata operations separately from 1.
/// Errors: As Zenith_InstallFileSystem; an operation numbered by FailAtOperation returns the configured error and changes nothing.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. Source API; a real implementation with a durability model, never a test double
///   (Zenith2/Docs/Design/PACK/Algorithms.md "Durability model").
/// @endcontract
class Zenith_MemoryInstallFileSystem final : public Zenith_InstallFileSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_MemoryInstallFileSystem>, Zenith_PackageError> Create(Zenith_Heap& xHeap,
		uint64 ulCapacityBytes);                                         // process 1, volume kind EXT4
	// … every Zenith_InstallFileSystem override …
	Zenith_Result<Zenith_InstallStat, Zenith_PackageError> Stat(Zenith_StringView strPath) override;
	Zenith_Result<void, Zenith_PackageError> List(Zenith_StringView strDirectory, Zenith_PfnInstallListVisitor pfnVisitor,
		void* pUserData) override;
	Zenith_Result<Zenith_InstallFile, Zenith_PackageError> Open(Zenith_StringView strPath, Zenith_InstallOpenMode eMode) override;
	Zenith_Result<uint64, Zenith_PackageError> Read(Zenith_InstallFile& xFile, uint64 ulOffset,
		Zenith_MutableByteSpan xDestination) override;
	Zenith_Result<void, Zenith_PackageError> Write(Zenith_InstallFile& xFile, uint64 ulOffset, Zenith_ByteSpan xBytes) override;
	Zenith_Result<void, Zenith_PackageError> Truncate(Zenith_InstallFile& xFile, uint64 ulBytes) override;
	Zenith_Result<void, Zenith_PackageError> FlushFile(Zenith_InstallFile& xFile) override;
	void Close(Zenith_InstallFile& xFile) override;
	Zenith_Result<void, Zenith_PackageError> Rename(Zenith_StringView strFrom, Zenith_StringView strTo, bool bReplace) override;
	Zenith_Result<void, Zenith_PackageError> RemoveFile(Zenith_StringView strPath) override;
	Zenith_Result<void, Zenith_PackageError> RemoveLink(Zenith_StringView strPath) override;
	Zenith_Result<void, Zenith_PackageError> CreateDirectory(Zenith_StringView strPath) override;
	Zenith_Result<void, Zenith_PackageError> RemoveEmptyDirectory(Zenith_StringView strPath) override;
	Zenith_Result<void, Zenith_PackageError> FlushDirectory(Zenith_StringView strPath) override;
	Zenith_Result<uint64, Zenith_PackageError> QueryFreeBytes() override;
	Zenith_Result<Zenith_InstallVolumeKind, Zenith_PackageError> QueryVolumeKind() override;
	Zenith_Result<void, Zenith_PackageError> AcquireLock(Zenith_InstallLock eLock, Zenith_InstallLockMode eMode) override;
	void ReleaseLock(Zenith_InstallLock eLock) override;
	bool HoldsLock(Zenith_InstallLock eLock, Zenith_InstallLockMode eMode) const override;
	// The stamp model: the identity is the node's number (assigned from 1 at creation, never reused by this object, kept by
	// Rename and by every power loss the node survives), the volume is 1, the size the current length; a logical clock advances
	// by the clock step (1,000,000,000 ns by default, SetClockStepNs) at every call, and GetCurrentFileTimeNs returns it, so
	// stamps are never racy unless a test shrinks the step; the last-write time is the clock of the node's last Write or Truncate, the change time the clock of its
	// creation or its last Write, Truncate, SetLastWriteTime, read-only attribute change or Rename of the node itself (NTFS, ext4,
	// XFS, Btrfs and F2FS all advance a renamed file's change time); renaming a directory leaves the stamps of every node inside it
	// unchanged. On a volume kind outside the supported set (SetVolumeKind) it returns PACKAGE_ERROR_NOT_SUPPORTED.
	Zenith_Result<Zenith_FileIdentityStamp, Zenith_PackageError> GetIdentityStamp(Zenith_InstallFile& xFile) override;
	void SetLastWriteTime(Zenith_StringView strPath, uint64 ulLastWriteNs);   // models SetFileTime / utimensat: the change time advances
	uint64 GetCurrentFileTimeNs() const override;
	void SetClockStepNs(uint64 ulStepNs);                           // the logical clock's advance per call (default 10^9)
	// durability model, processes and fault control
	void SimulatePowerLoss();                                        // SimulatePowerLossAfterMetadata(the durable metadata count)
	void SimulatePowerLossAfterMetadata(uint64 ulMetadataOperation);   // metadata 1 … max(k, durable) survive; data as last flushed
	void SimulateProcessKill();                                      // everything survives; every process's files and locks released
	Zenith_Result<Zenith_Owned<Zenith_InstallFileSystem>, Zenith_PackageError> CreateProcessView(uint32 uProcess);   // 2 …; its own locks and files
	void FailAtOperation(uint64 ulOperation, Zenith_PackageError eError);   // 0 disables
	void SetVolumeKind(Zenith_InstallVolumeKind eKind);
	void SetRootReadOnly(bool bReadOnly);                            // every mutation INSTALL_ROOT_READ_ONLY; AcquireLock's read-only fallback
	void SetFreeBytes(uint64 ulFreeBytes);                          // QueryFreeBytes's answer; default the capacity minus the bytes stored
	uint64 GetOperationCount() const;
	uint64 GetMetadataOperationCount() const;
	uint64 GetDurableMetadataCount() const;
	Zenith_Span<const Zenith_MemoryFsTraceEntry> GetTrace() const;   // every call since Create, ClearTrace or the last power loss
	void ClearTrace();
	Zenith_Result<void, Zenith_PackageError> WriteWholeFile(Zenith_StringView strPath, Zenith_ByteSpan xBytes,
		Zenith_MemoryFileDurability eDurability);                    // creates missing parents with the same durability
	Zenith_Result<void, Zenith_PackageError> CreateLink(Zenith_StringView strPath, Zenith_StringView strTarget,
		bool bDirectory);                                            // a metadata operation; models a junction or a symbolic link
	Zenith_Result<Zenith_ByteSpan, Zenith_PackageError> ReadWholeFile(Zenith_StringView strPath, Zenith_Arena& xArena);
};
```

## Zenith_UpdateApplier header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_UpdateApplier.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"
#include "ZenithPlatform/Zenith_UpdateManifest.h"   // Zenith_UpdateStampLookup (PLAT, cross-area row PK27 (b))
#include "ZenithPlatform/Zenith_VirtualFileSystem.h"   // Zenith_MountSource (PLAT; GetIdentityStamp is PK27 (a))
#include "ZenithPackage/Zenith_InstallFileSystem.h"

class Zenith_InstallFileSystem;
class Zenith_Heap;
class Zenith_Arena;
class Zenith_FaultInjector;
struct Zenith_BuildIdentity;

/// @contract enum Zenith_InstallPhase
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A phase byte outside 1..5 in install-journal.zinj is PACKAGE_ERROR_JOURNAL_CORRUPT.
/// Version: since Z2 Stage 6. Serialized as uint8 in install-journal.zinj (Formats.md "Install journal").
/// @endcontract
enum Zenith_InstallPhase : uint8
{
	INSTALL_PHASE_COMMITTING = 1,    // a sealed, verified incoming bundle is being renamed into place (and, for a DLC, the old revision out)
	INSTALL_PHASE_COMMITTED,         // the rename and the install state are durable; obsolete directories not yet moved to .trash
	INSTALL_PHASE_CLEANING,          // obsolete directories being moved to .trash and deleted
	INSTALL_PHASE_REJECTING,         // a refused incoming bundle being moved to .trash and deleted
	INSTALL_PHASE_REMOVING,          // a DLC removal or an obsolete sweep in progress
};

/// @contract enum Zenith_InstallTargetKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. Serialized as uint8 in install-journal.zinj and in the detail byte of Package.UpdateFailed.
/// @endcontract
enum Zenith_InstallTargetKind : uint8
{
	INSTALL_TARGET_KIND_PATCH = 1,   // <root>/<patchRoot>/<MAJOR.MINOR.PATCH>/
	INSTALL_TARGET_KIND_DLC,         // <root>/DLC/<name>/
};

using Zenith_PfnInstallWait = void (*)(uint32 uPollIndex, void* pUserData);

/// @contract value Zenith_UpdateApplierDesc
/// Ownership: Plain value; pointers and views are borrowed and outlive the applier.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Bytes; milliseconds.
/// Errors: Validated by Create (PACKAGE_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_UpdateApplierDesc
{
	Zenith_InstallFileSystem* m_pxFiles = nullptr;   // rooted at <install> (Windows) or the internal storage root (Android); outlives the call
	Zenith_StringView m_strPatchRoot = "Patches";    // the mount manifest's patchRoot
	Zenith_StringView m_strDlcRoot = "DLC";
	Zenith_ByteSpan m_xUpdatePublicKey;              // the 32-byte compiled-in Ed25519 key (Player data)
	const Zenith_BuildIdentity* m_pxBuild = nullptr; // the running build (PLAT's type)
	uint64 m_ulPackageContentHash = 0;              // the base package's content hash for U11; 0: unknown at step 4, every patch bundle left in place
	uint32 m_uHashChunkBytes = 4194304;             // bytes read per call while hashing a bundle's files
	uint32 m_uLockWaitMs = 30000;                   // the INSTALL_LOCK_APPLY wait of Zenith_RunPendingInstalls step 2; 0: one attempt
	// Called once per 50 ms poll of steps 2 and 10 with that step's poll index, which starts at 1 in each step (step 10's
	// does not continue step 2's; with m_uLockWaitMs 0 step 2 calls it never); null: Zenith_SleepUs(50000). Tests pass a hook
	// that changes another process view's locks at a chosen poll, so the unbounded wait of step 10 is tested without a clock.
	Zenith_PfnInstallWait m_pfnWait = nullptr;
	void* m_pWaitUserData = nullptr;
	uint64 m_ulLowDiskMarginBytes = 67108864;       // free space the journal and trash moves keep (they write at most 4 MiB)
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
	bool m_bInstallIncoming = true;                  // false in every Android build (Algorithms.md "Android content updates")
};

inline constexpr uint32 uFAULT_POINT_PACKAGE_APPLIER_LOCK = 0x504B0030;
inline constexpr uint32 uFAULT_POINT_PACKAGE_APPLIER_JOURNAL_READ = 0x504B0031;

/// @contract value Zenith_UpdateApplierReport
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the applier accumulates its counts in it.
/// Invalidation: None.
/// Units: Counts and bytes.
/// Errors: m_eFirstError is the first rejected or failed item's Zenith_PackageError, else 0.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_UpdateApplierReport
{
	uint32 m_uRecoveredForward = 0;   // journals completed by recovery
	uint32 m_uRecoveredBackward = 0;  // journals abandoned by recovery (recovery rows 6 and 8 with a vanished bundle)
	uint32 m_uInstalled = 0;          // incoming bundles committed
	uint32 m_uRejected = 0;           // incoming bundles refused (verification, target, not newer)
	uint32 m_uRemovedObsolete = 0;    // superseded, other-build and other-line patch versions deleted
	uint32 m_uTransientRetries = 0;   // Windows rename and delete retries (Algorithms.md "Durability model")
	uint32 m_uDeferredObsolete = 0;   // obsolete directories beyond one journal's 1,024 records, left for the next start
	uint32 m_uGuardWaitPolls = 0;     // polls of step 10's wait for INSTALL_LOCK_RUNNING shared
	uint32 m_uIncompleteBundles = 0;  // bundles left in place: a listed file absent (B4 (b)) or the base identity unknown
	uint64 m_ulBytesVerified = 0;
	uint8 m_eFirstError = 0;
	bool m_bOtherInstanceRunning = false;   // step 3 found another instance: no install, recovery or sweep this start
	bool m_bInstanceGuardHeld = false;      // INSTALL_LOCK_RUNNING is held shared on return; false only after step 1's unsupported
	                                        // volume or a read-only root without a running file (each logged)
};

/// @contract type Zenith_UpdateApplier
/// Ownership: Created by Create as a Zenith_Owned; owned by Zenith_RunPendingInstalls for its call, or by a test.
/// Thread affinity: OWNER_THREAD (the main thread in the closing action of startup step 4).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Create returns PACKAGE_ERROR_INSTALL_LOCKED unless m_pxFiles holds INSTALL_LOCK_APPLY and INSTALL_LOCK_RUNNING
///   exclusively. Every member returns the Zenith_PackageError of its first failure and leaves the journal at the last
///   durable phase; recovery from that phase is always possible. Never an assert on journal, manifest or bundle bytes.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. The state machine is Zenith2/Docs/Design/PACK/Algorithms.md "Install state machine".
/// @endcontract
class Zenith_UpdateApplier
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UpdateApplier>, Zenith_PackageError> Create(
		const Zenith_UpdateApplierDesc& xDesc);   // reads the journal and the install state; takes no lock
	~Zenith_UpdateApplier();

	// Finishes or abandons the operation an existing journal describes ("Recovery table").
	Zenith_Result<void, Zenith_PackageError> Recover(Zenith_UpdateApplierReport& xReport);
	// B1–B7 of Algorithms.md "Install steps per operation": lists, decodes, checks U0, seals and verifies one incoming bundle
	// (<patchRoot>/.incoming/<version>/ or <dlcRoot>/.incoming/<name>/) in place; on success writes the COMMITTING journal,
	// on a refusal the REJECTING journal. Returns the verification outcome.
	Zenith_Result<void, Zenith_PackageError> BeginInstall(Zenith_InstallTargetKind eKind,
		Zenith_StringView strIncomingName);
	Zenith_Result<void, Zenith_PackageError> BeginRemoveDlc(Zenith_StringView strName);   // tests, distributor tools
	// Computes rules O1, O2 and O4 of "Obsolete directories" and writes the REMOVING journal; returns false when the set is empty.
	Zenith_Result<bool, Zenith_PackageError> BeginRemoveObsolete();
	// Performs exactly one durable step of the operation in progress; returns the phase reached.
	Zenith_Result<Zenith_InstallPhase, Zenith_PackageError> Step(Zenith_UpdateApplierReport& xReport);
	bool HasOperation() const;
	Zenith_InstallPhase GetPhase() const;
	// Writes .zenith2-install/install-state.zins with the running line, the highest committed patch and the running build
	// as the swept build, when any differs from the state read at Create (one "write the install state" step).
	Zenith_Result<void, Zenith_PackageError> WriteInstallState();
};

inline constexpr uint32 uPACKAGE_MAX_VERIFIED_DIRECTORIES = 1024;      // directory records of verified.zinv
inline constexpr uint32 uPACKAGE_MAX_VERIFIED_FILES = 131072;          // file records of verified.zinv
inline constexpr uint64 ulPACKAGE_MAX_VERIFIED_FILE_BYTES = 8437792;   // 32 + 48 × 1,024 + 56 × 131,072 + 1,048,576

/// @contract value Zenith_VerifiedFile
/// Ownership: Plain value; m_strName borrows the owning Zenith_VerifiedStampSet's storage (or B6's name storage).
/// Thread affinity: IMMUTABLE; members non-const: its owner fills its arrays field by field.
/// Invalidation: The owner's next ReplaceDirectory, DropDirectory or Decode.
/// Units: Times in ns since the Unix epoch; bytes.
/// Errors: None.
/// Version: since Z2 Stage 6. One file record of verified.zinv (Formats.md "Verified stamp file").
/// @endcontract
struct Zenith_VerifiedFile
{
	Zenith_StringView m_strName;                     // a listed file name, one path component
	Zenith_FileIdentityStamp m_xStamp;               // taken before the first read, equal to the one after the last
};

/// @contract value Zenith_VerifiedDirectory
/// Ownership: Plain value; the path and the file span borrow the owner's storage.
/// Thread affinity: IMMUTABLE; members non-const: its owner fills its arrays field by field.
/// Invalidation: As Zenith_VerifiedFile.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6. One directory record; B6 builds one per verified bundle.
/// @endcontract
struct Zenith_VerifiedDirectory
{
	Zenith_StringView m_strPath;                     // "Patches/1.2.3" or "DLC/<name>", install-relative
	uint8 m_auManifestSha256[32] = {};              // SHA-256 of the whole update.zman or dlc.zman B6 verified
	Zenith_Span<const Zenith_VerifiedFile> m_axFiles;   // 1 … 65,536, sorted by name bytes
};

/// @contract type Zenith_VerifiedStampSet
/// Ownership: Created by Create as a Zenith_Owned from xHeap; owns copies of every path, name and file array it holds.
///   Owned by the applier for one "write the verified stamp file" step.
/// Thread affinity: OWNER_THREAD (the main thread in the closing action of startup step 4).
/// Invalidation: GetDirectory's spans are invalidated by the next mutating member.
/// Units: Bytes.
/// Errors: Decode returns PACKAGE_ERROR_STATE_FILE_CORRUPT (VS1–VS6) and leaves the set empty; every member returns
///   PACKAGE_ERROR_OUT_OF_MEMORY when its copy cannot be allocated, leaving the set as before; never an assert on file bytes.
/// Version: since Z2 Stage 6. The merge of Algorithms.md "Verified cache" item 4.
/// @endcontract
class Zenith_VerifiedStampSet
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_VerifiedStampSet>, Zenith_PackageError> Create(Zenith_Heap& xHeap);
	~Zenith_VerifiedStampSet();
	// Replaces the set's contents with the records of xFileBytes (the file read from disk at step 3b or 4b).
	Zenith_Result<void, Zenith_PackageError> Decode(Zenith_ByteSpan xFileBytes);
	uint32 GetDirectoryCount() const;
	Zenith_VerifiedDirectory GetDirectory(uint32 uIndex) const;   // in path byte order
	const Zenith_VerifiedDirectory* FindDirectory(Zenith_StringView strPath) const;   // null: none
	// Inserts xDirectory (copied) or replaces the record of the same path; keeps path byte order.
	Zenith_Result<void, Zenith_PackageError> ReplaceDirectory(const Zenith_VerifiedDirectory& xDirectory);
	void DropDirectory(Zenith_StringView strPath);                                // absent: nothing
	// Applies the overflow rule of Formats.md "Verified stamp file" (other directories dropped highest path first, then the
	// target's record when it alone does not fit), counts the directories it dropped in uDroppedOut, and writes the file's
	// bytes (header, records, string table, check) into xArena; the result is at most ulPACKAGE_MAX_VERIFIED_FILE_BYTES.
	Zenith_Result<Zenith_ByteSpan, Zenith_PackageError> Encode(Zenith_StringView strTargetPath, uint64 ulGeneration,
		Zenith_Arena& xArena, uint32& uDroppedOut);
};

/// @contract type Zenith_InstallTreeMountSource
/// Ownership: Created by Create as a Zenith_Owned; borrows xFiles (non-const: GetIdentityStamp and Open go through it), which
///   must outlive it. Created by the applier per bundle at B6 and per committed directory by recovery's re-stamp.
/// Thread affinity: OWNER_THREAD, the applier's thread (every call reaches the install file system, which is not thread-safe).
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Each override maps its Zenith_PackageError to PLAT's Zenith_FileError (PATH_INVALID → INVALID_PATH, an absent
///   entry → NOT_FOUND, any other → IO); GetIdentityStamp answers false when the stamp cannot be taken (an I/O error, an
///   unsupported volume), which means "hash it".
/// Reentrancy: NOT_APPLICABLE; no callback.
/// Version: since Z2 Stage 6. Implements PLAT's Zenith_MountSource (Zenith2/Docs/Design/PLAT/Filesystem.md → "Mount sources").
/// @endcontract
class Zenith_InstallTreeMountSource final : public Zenith_MountSource
{
public:
	// strRoot: an install-relative directory ("Patches/.incoming/1.2.3", "DLC/a"); every strRelative resolves under it.
	static Zenith_Result<Zenith_Owned<Zenith_InstallTreeMountSource>, Zenith_PackageError> Create(
		Zenith_InstallFileSystem& xFiles, Zenith_StringView strRoot, Zenith_Heap& xHeap);
	~Zenith_InstallTreeMountSource() override;
	Zenith_StringView GetDescription() const override;   // "install:<root>"
	Zenith_Result<Zenith_SourceEntry, Zenith_FileError> Lookup(Zenith_StringView strRelative) const override;
	Zenith_Result<Zenith_SourceFile, Zenith_FileError> Open(const Zenith_SourceEntry& xEntry,
		Zenith_StringView strRelative) const override;
	Zenith_Result<uint64, Zenith_FileError> Read(Zenith_SourceFile& xFile, uint64 ulOffset,
		Zenith_MutableByteSpan xDestination) const override;
	void Close(Zenith_SourceFile& xFile) const override;
	Zenith_Result<void, Zenith_FileError> List(Zenith_StringView strDirectory, Zenith_PfnDirectoryVisitor pfnVisitor,
		void* pUserData) const override;
	// Opens strRelative READ through the install file system, takes its GetIdentityStamp, closes it (PK27 (a)).
	bool GetIdentityStamp(Zenith_StringView strRelative, Zenith_FileIdentityStamp& xOut) const override;
};

/// @contract type Zenith_VerifiedStampFile
/// Ownership: Created by Create as a Zenith_Owned from xHeap; owns a copy of the file's bytes. Owned by the runtime bridge from
///   the close of startup step 4 until shutdown step 6 (the patch mount at step 5 and the service's DLC mounting borrow it).
/// Thread affinity: IMMUTABLE after Create; FindStamp is ANY_THREAD (the patch mount may hash on its own thread); private members non-const: Create
///   fills them after allocating the file.
/// Invalidation: None while owned.
/// Units: Bytes; times in ns since the Unix epoch.
/// Errors: Create returns PACKAGE_ERROR_STATE_FILE_CORRUPT naming the failed check VS1–VS6 in the log (Formats.md "Verified
///   stamp file"), PACKAGE_ERROR_OUT_OF_MEMORY for the copy; never an assert on file bytes.
/// Reentrancy: FindStamp calls nothing back.
/// Version: since Z2 Stage 6. Format version 1; Q90 ruled (a), Algorithms.md "Verified cache" item 4.
/// @endcontract
class Zenith_VerifiedStampFile final : public Zenith_UpdateStampLookup
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_VerifiedStampFile>, Zenith_PackageError> Create(Zenith_ByteSpan xFileBytes,
		Zenith_Heap& xHeap);
	~Zenith_VerifiedStampFile() override;
	// True with the recorded stamp when strDirectory ("Patches/1.2.3", "DLC/<name>") is recorded with exactly xManifestSha256
	// (32 bytes) and lists strFile; false otherwise. O(log D + log F) binary search, no allocation.
	bool FindStamp(Zenith_StringView strDirectory, Zenith_ByteSpan xManifestSha256, Zenith_StringView strFile,
		Zenith_FileIdentityStamp& xStampOut) const override;
	uint32 GetDirectoryCount() const;
	uint32 GetFileCount() const;
};

// Closing action of startup step 4, steps 1–11 of Algorithms.md "Pending installs at startup": volume check, the two locks,
// the cheap check, Recover, the obsolete sweep, every incoming bundle (patches in descending version order, then DLC in byte
// order), the install state; returns with INSTALL_LOCK_RUNNING held shared on xDesc.m_pxFiles (m_bInstanceGuardHeld), after
// waiting without bound, when it could not take INSTALL_LOCK_APPLY, for the installing process to hand its guard over.
// With m_bInstallIncoming false (every Android build) it performs recovery and the sweep only. Each commit writes
// .zenith2-install/verified.zinv (patch step 3b, DLC step 4b) from B6's stamps (Algorithms.md "Verified cache" item 4).
Zenith_Result<Zenith_UpdateApplierReport, Zenith_PackageError> Zenith_RunPendingInstalls(
	const Zenith_UpdateApplierDesc& xDesc);
```

## Zenith_InstallRecord header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_InstallRecord.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_Arena;

/// @contract enum Zenith_MigrationPhase
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: A byte outside 1..2 is PACKAGE_ERROR_RECORD_CORRUPT.
/// Version: since Z2 Stage 6. Serialized as uint8 in install-record.zinr.
/// @endcontract
enum Zenith_MigrationPhase : uint8
{
	MIGRATION_PHASE_PENDING = 1,     // written before the first migration step runs
	MIGRATION_PHASE_DONE,
};

/// @contract value Zenith_InstallRecord
/// Ownership: Plain value read from and written to user:/cache/package/install-record.zinr.
/// Thread affinity: IMMUTABLE; members non-const: read field by field from install-record.zinr.
/// Invalidation: None.
/// Units: None.
/// Errors: Decode returns PACKAGE_ERROR_RECORD_CORRUPT for any check failure (Formats.md "Install record" R1–R5),
///   another format version included (R2); never an assert.
/// Version: since Z2 Stage 6. Format version 1.
/// @endcontract
struct Zenith_InstallRecord
{
	Zenith_InstallIdentity m_xIdentity;
	Zenith_InstallIdentity m_xPreviousIdentity;      // the identity before the last update; all zero after a first run
	Zenith_UpdateKind m_eLastUpdate = UPDATE_KIND_FIRST_RUN;
	Zenith_MigrationPhase m_ePhase = MIGRATION_PHASE_DONE;
	uint16 m_uHighestPatchSeen = 0;                 // per release line, for the downgrade rule
	uint64 m_ulRunCount = 0;
};

Zenith_Result<Zenith_InstallRecord, Zenith_PackageError> Zenith_DecodeInstallRecord(Zenith_ByteSpan xBytes);
Zenith_ByteSpan Zenith_EncodeInstallRecord(const Zenith_InstallRecord& xRecord, Zenith_Arena& xArena);   // 192 bytes
// The classification of Zenith2/Docs/Design/PACK/Algorithms.md "Update classification".
Zenith_UpdateKind Zenith_ClassifyUpdate(const Zenith_InstallRecord* pxRecord, const Zenith_InstallIdentity& xRunning);
```

## Zenith_UpdateMigration header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_UpdateMigration.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithPackage/Zenith_PackageTypes.h"

class Zenith_VirtualFileSystem;
class Zenith_AsyncIO;
class Zenith_Heap;

/// @contract value Zenith_UpdateMigrationContext
/// Ownership: Plain value built by the package service for one migration run; pointers borrowed for the run.
/// Thread affinity: IMMUTABLE; members non-const: the package service fills it field by field for each run.
/// Invalidation: Valid for the step call only.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_UpdateMigrationContext
{
	Zenith_UpdateKind m_eKind = UPDATE_KIND_NONE;
	Zenith_InstallIdentity m_xFrom;
	Zenith_InstallIdentity m_xTo;
	Zenith_VirtualFileSystem* m_pxFiles = nullptr;
	Zenith_AsyncIO* m_pxIO = nullptr;
};

/// @contract functions Zenith_UpdateMigrationStep
/// Ownership: A function pointer plus user data registered by the composition root on behalf of an owner.
/// Thread affinity: MAIN_THREAD, in the closing action of startup step 11, before any world exists.
/// Invalidation: None.
/// Units: None.
/// Errors: A step returns PACKAGE_ERROR_MIGRATION_STEP_FAILED or the error of its first failed write; the registry then runs
///   the step's fallback (delete the step's declared user:/cache/<dir>/ tree) and continues.
/// Version: since Z2 Stage 6. Steps are idempotent: a step re-run after a kill gives the same result.
/// @endcontract
using Zenith_PfnUpdateMigrationStep = Zenith_Result<void, Zenith_PackageError> (*)(const Zenith_UpdateMigrationContext& xContext,
	void* pUserData);

/// @contract value Zenith_UpdateMigrationStepDesc
/// Ownership: Plain value; the strings are static data of the registering owner.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Register.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_UpdateMigrationStepDesc
{
	const char* const m_szName = nullptr;           // "Package.ForgetRemovedUnits"; unique; ^[A-Z][A-Za-z0-9]*\.[A-Z][A-Za-z0-9.]*$
	const uint16 m_uOrder = 0;                     // ascending run order; ties broken by name byte order
	const uint8 m_uKindsMask = 0;                  // bit (Zenith_UpdateKind - 1) set for each kind the step runs on
	const char* const m_szFallbackCacheDirectory = nullptr; // "pso" → user:/cache/pso/ is deleted when the step fails; null: none
	const Zenith_PfnUpdateMigrationStep m_pfnStep = nullptr;
	void* const m_pUserData = nullptr;
};

/// @contract value Zenith_UpdateMigrationReport
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: Run accumulates it.
/// Invalidation: None.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_UpdateMigrationReport
{
	uint32 m_uStepsRun = 0;
	uint32 m_uStepsFailed = 0;
	uint32 m_uFallbacksRun = 0;
};

/// @contract type Zenith_UpdateMigrationRegistry
/// Ownership: Created by Create as a Zenith_Owned; owned by the composition root; filled in the closing action of startup step 11 before Freeze.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Register returns PACKAGE_ERROR_INVALID_DESC (bad name, duplicate name, null step, mask 0, more than
///   uPACKAGE_MAX_MIGRATION_STEPS); Run never fails as a whole: a failing step is counted and its fallback runs.
/// Reentrancy: NONE. A step never calls the registry.
/// Version: since Z2 Stage 6. Source API.
/// @endcontract
class Zenith_UpdateMigrationRegistry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_UpdateMigrationRegistry>, Zenith_PackageError> Create(Zenith_Heap& xHeap);
	Zenith_Result<void, Zenith_PackageError> Register(const Zenith_UpdateMigrationStepDesc& xDesc);
	void Freeze();
	Zenith_UpdateMigrationReport Run(const Zenith_UpdateMigrationContext& xContext);
	uint32 GetStepCount() const;
};

// PACK's own steps, registered first by Zenith_RegisterPackageMigrationSteps (Algorithms.md "Built-in migration steps").
Zenith_Result<void, Zenith_PackageError> Zenith_RegisterPackageMigrationSteps(Zenith_UpdateMigrationRegistry& xRegistry);
```

## Zenith_PackageService header

```cpp
// Zenith2/Engine/ZenithPackage/Zenith_PackageService.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithPackage/Zenith_PackageTypes.h"
#include "ZenithPackage/Zenith_AssetPackInstaller.h"     // Zenith_AssetPackInstallerDesc, Zenith_InstallerPrologueResult
#include "ZenithPackage/Zenith_UpdateMigration.h"         // Zenith_UpdateMigrationReport

class Zenith_PackageManifest;
class Zenith_AssetPackDelivery;
class Zenith_EntitlementQuery;
class Zenith_VirtualFileSystem;
class Zenith_AsyncIO;
class Zenith_JobSystem;
class Zenith_Platform;
class Zenith_Heap;
class Zenith_FaultInjector;
class Zenith_JsonWriter;
class Zenith_ConfigSchema;
class Zenith_InstallFileSystem;
struct Zenith_ArchiveReaderDesc;
struct Zenith_BuildIdentity;
enum Zenith_BuildPlatform : uint8;
enum Zenith_ConfigError : uint8;

/// @contract value Zenith_PackageServiceDesc
/// Ownership: Plain value; pointers are borrowed and outlive the service, except m_xDelivery, which it takes.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Validated by Create (PACKAGE_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_PackageServiceDesc
{
	Zenith_Platform* m_pxPlatform = nullptr;
	Zenith_VirtualFileSystem* m_pxFiles = nullptr;
	Zenith_AsyncIO* m_pxIO = nullptr;
	Zenith_JobSystem* m_pxJobs = nullptr;
	const Zenith_ArchiveReaderDesc* m_pxArchiveReader = nullptr;
	const Zenith_BuildIdentity* m_pxBuild = nullptr;
	const Zenith_EntitlementQuery* m_pxEntitlements = nullptr;         // the bridge's Zenith_ForwardingEntitlementQuery
	Zenith_InstallFileSystem* m_pxInstallFiles = nullptr;              // Windows: the bridge's install file system (DLC U0); else null
	const Zenith_UpdateStampLookup* m_pxUpdateStamps = nullptr;        // Windows: the bridge's Zenith_VerifiedStampFile for DLC U9; null:
	                                                                    // every DLC file hashed (no valid verified.zinv, or package.reverifyOnStartup)
	Zenith_UpdateMigrationRegistry* m_pxMigrations = nullptr;          // frozen by the service after its own steps
	Zenith_Owned<Zenith_AssetPackDelivery>* m_pxDelivery = nullptr;    // moved in; null when package.zpkm lists no deliverable unit
	Zenith_StringView m_strDlcHostRoot;                                 // Windows: <install>/DLC; empty elsewhere
	Zenith_BuildPlatform m_ePlatform = static_cast<Zenith_BuildPlatform>(0);   // the running platform, for CheckIdentity's layout rule
	uint32 m_uInitialQueryTimeoutMs = 500;                             // package.initialQueryTimeoutMs
	bool m_bShippingRules = false;                                      // the bridge sets Zenith_IsShippingBuild(); tests set it directly
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
	Zenith_AssetPackInstallerDesc m_xInstaller;                         // the configuration values; pointers filled by Create
};

inline constexpr uint32 uFAULT_POINT_PACKAGE_SERVICE_MANIFEST_READ = 0x504B0040;
inline constexpr uint32 uFAULT_POINT_PACKAGE_SERVICE_RECORD_WRITE = 0x504B0041;
inline constexpr uint32 uFAULT_POINT_PACKAGE_SERVICE_DLC_MOUNT = 0x504B0042;

/// @contract value Zenith_PackageUpdateInfo
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_PackageUpdateInfo
{
	Zenith_UpdateKind m_eKind = UPDATE_KIND_NONE;
	Zenith_InstallIdentity m_xFrom;
	Zenith_InstallIdentity m_xTo;
	Zenith_UpdateMigrationReport m_xMigration;
};

/// @contract type Zenith_PackageService
/// Ownership: Created by Create in the closing action of startup step 11 as a Zenith_Owned; owned by Zenith_Runtime; destroyed in shutdown step 6.
/// Thread affinity: MAIN_THREAD for every member.
/// Invalidation: GetManifest and GetInstaller references live as long as the service.
/// Units: Bytes.
/// Errors: Create returns PACKAGE_ERROR_MANIFEST_* when content:/Package/package.zpkm is absent or invalid and
///   m_bShippingRules is set (startup exit code 3); otherwise an absent manifest yields a service with no units. Record and
///   migration failures are logged and never fail Create.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 6. Source API.
/// @endcontract
class Zenith_PackageService
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_PackageService>, Zenith_PackageError> Create(
		Zenith_PackageServiceDesc& xDesc);
	~Zenith_PackageService();

	const Zenith_PackageManifest* GetManifest() const;    // null in a development build without one
	Zenith_AssetPackInstaller* GetInstaller();            // null when no deliverable unit exists
	const Zenith_PackageUpdateInfo& GetUpdateInfo() const;
	Zenith_InstallIdentity GetRunningIdentity() const;

	// Frame prologue step 1b. Fills axEventsOut with the events the bridge completes as ASYNC_SOURCE_PACKAGE results.
	Zenith_InstallerPrologueResult RunPrologue(const Zenith_InstallerFrameInput& xFrame,
		Zenith_Span<Zenith_PackageEvent> axEventsOut);
	// PRESENT units: MOUNTED; deliverable units: the installer's status; Windows DLC units: the DLC's state and error.
	Zenith_AssetPackStatus GetUnitStatus(Zenith_StringView strUnit) const;
	void Describe(Zenith_JsonWriter& xWriter) const;                    // the frame-dump entry "package"

	void BeginShutdown();                      // the installer's BeginShutdown and A5 on every mounted Windows DLC
	bool PumpShutdown();          // the installer's PumpShutdown and the Windows DLC releases
	bool IsShutdownComplete() const;
};

Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterPackageConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);
```

## Runtime bridge declarations

`ZenithRuntime` is MOD's library; `Internal/Package/` is PACK's subdirectory of it, the pattern of `Zenith2/Docs/Design/SAVE.md → "Runtime bridge files"` (cross-area rows PK3, PK15, PK21, PK22). ZenithRuntime declares `ZenithPackage` as a direct dependency (`Zenith2/Docs/Architecture.md` → "Library dependency graph"). The bridge reaches the async-result table, the command buffer and the event buffer because it is the composition root's own code; ZenithPackage itself depends on none of them.

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/Package/Zenith_RuntimePackage.h
#pragma once
#include "ZenithPackage/Zenith_PackageService.h"
#include "ZenithPackage/Zenith_UpdateApplier.h"
#include "ZenithPackage/Zenith_EntitlementQuery.h"   // Zenith_ForwardingEntitlementQuery
#include "ZenithScript/Zenith_ScriptHost.h"          // Zenith_AsyncPublicationHandler

class Zenith_Runtime;
class Zenith_AsyncResultTable;
class Zenith_CommandKindTable;
class Zenith_ScriptBindingTable;
class Zenith_TickBoundaryAccess;
struct Zenith_ScriptPublication;

// Which provider the bridge's forwarding entitlement query answers from (Algorithms.md "Entitlement provider").
enum Zenith_EntitlementProviderKind : uint8
{
	ENTITLEMENT_PROVIDER_KIND_NONE = 1,          // every ID UNKNOWN
	ENTITLEMENT_PROVIDER_KIND_CONFIG,            // Zenith_ConfigEntitlementQuery over package.grantedEntitlements
	ENTITLEMENT_PROVIDER_KIND_MODULE,            // the query a module provided at step 15 (ONLINE's)
};

// What the step-order tests read (Test_RuntimePackage_StartupStepOrder, Test_RuntimePackage_EntitlementProviderLifecycle);
// recorded by the bridge as it runs.
struct Zenith_RuntimePackageStartupTrace
{
	bool m_bPendingInstallsRan = false;          // the closing action of step 4 ran
	bool m_bFileSystemExistedAtPendingInstalls = false;   // must be false: the VFS is created at step 5
	bool m_bServiceCreated = false;              // the closing action of step 11 ran
	bool m_bAssetRegistryExistedAtService = false;        // must be true
	bool m_bSaveSystemExistedAtService = false;           // must be true: the service follows Zenith_RuntimeSave_CreateSystem
	bool m_bRhiDeviceExistedAtService = false;            // must be false: step 12 follows
	bool m_bInstanceGuardHeld = false;           // the report's m_bInstanceGuardHeld
	Zenith_EntitlementProviderKind m_eProviderAtServiceCreate = ENTITLEMENT_PROVIDER_KIND_NONE;
	Zenith_EntitlementProviderKind m_eProviderAfterModulesStarted = ENTITLEMENT_PROVIDER_KIND_NONE;
	uint32 m_uReplayUnits = 0;                  // the size of R under --replay (Algorithms.md "Replay pack set"), else 0
};

/// @contract value Zenith_PackageUnitState
/// Ownership: An element of Zenith_PackageStateResource.
/// Thread affinity: IMMUTABLE within a tick (written only at tick-boundary step 1); members non-const: step 1 rewrites them in place.
/// Invalidation: None while the world lives.
/// Units: Bytes.
/// Errors: m_eError is a Zenith_PackageError or 0.
/// Version: since Z2 Stage 6. 24 bytes.
/// @endcontract
struct Zenith_PackageUnitState
{
	uint16 m_uUnitIndex = 0;                    // the unit's index in package.zpkm
	Zenith_AssetPackState m_eState = ASSET_PACK_STATE_UNKNOWN;
	uint8 m_eError = 0;
	uint32 m_uReserved = 0;
	uint64 m_ulBytesDone = 0;
	uint64 m_ulBytesTotal = 0;                  // package.zpkm's unit bytes
};
static_assert(sizeof(Zenith_PackageUnitState) == 24);

/// @contract value Zenith_PackageStateResource
/// Ownership: Registered as the world resource "Zenith.Package.State" (owning phase PHASE_INPUT) at world creation step 4 by
///   Zenith_RuntimePackage_AttachWorld; the object is the world's Zenith_RuntimePackageWorldRecord member; readers borrow it
///   through Zenith_SystemContext::GetResource and the Package.* bindings.
/// Thread affinity: Written only by the record's handler at tick-boundary step 1, before any phase of the tick, so every tick
///   reads constant values; read from any node that declares ReadResource of it.
/// Invalidation: None while the world lives.
/// Units: Bytes; versions as MAJOR.MINOR.PATCH triples.
/// Errors: None.
/// Version: since Z2 Stage 6; type version 1. Every property transient (never saved): a restored world reads its own snapshot.
///   Initial value: update kind NONE with zero versions; m_uUnitCount = the units without the PRESENT flag, each UNKNOWN with
///   zero bytes, in unit order (Algorithms.md "World snapshot").
/// @endcontract
struct Zenith_PackageStateResource                   // 6,168 bytes
{
	Zenith_UpdateKind m_eUpdateKind = UPDATE_KIND_NONE;   // 1 UpdateKind
	uint8 m_uReserved = 0;
	uint16 m_auFromVersion[3] = {};                 // 2 FromVersion
	uint16 m_auToVersion[3] = {};                   // 3 ToVersion
	uint16 m_uUnitCount = 0;                        // 4 UnitCount: units without the PRESENT flag, at most uPACKAGE_MAX_STATE_UNITS
	uint16 m_auReserved[4] = {};                    // pads the header to 24 bytes
	Zenith_PackageUnitState m_axUnits[uPACKAGE_MAX_STATE_UNITS];   // 5 Units: the first m_uUnitCount, in unit order
	static const Zenith_TypeID xTYPE_ID;             // "Zenith.Package.State"
};
static_assert(sizeof(Zenith_PackageStateResource) == 6168);

// One per world, created at world creation step 4 (Zenith2/Docs/Design/GAME/WorldHost.md "World creation"), held in the
// world's host record, bound once to the world's Zenith_AsyncResultTable and retired with it (MOD's sink-binding rule).
class Zenith_RuntimePackageWorldRecord final : public Zenith_AsyncPublicationHandler
{
public:
	// Tick-boundary step 1: decode the 96-byte payload (Formats.md "Package event payload"), update the record's
	// Zenith_PackageStateResource (the unit's entry, or the update kind and versions), append the Package.* event for phase 9;
	// decrement the outstanding count. A malformed payload is dropped and counted (package.events.decode_failed).
	void OnPublished(const Zenith_ScriptPublication& xPublication, Zenith_TickBoundaryAccess& xWorld) override;
	uint32 GetOutstanding() const;      // allocated and not yet published
	uint32 GetDeferredCount() const;    // held by the deferral queue
	bool NeedsSnapshot() const;          // true from creation until its snapshot is allocated
	const Zenith_PackageStateResource& GetState() const;
};

// Closing action of startup step 4: builds the host install file system over Zenith_Platform::GetBasePath() (Windows) or
// GetPrefPath() (Android, the internal storage root, PK14); on Windows reads <install>/Content/mount-manifest.json
// through it for m_ulPackageContentHash (Zenith_ParseMountManifest; absent or invalid: 0, and every patch bundle waits);
// sets m_bInstallIncoming false on Android; runs Zenith_RunPendingInstalls; then, unless package.reverifyOnStartup, reads
// .zenith2-install/verified.zinv (at most ulPACKAGE_MAX_VERIFIED_FILE_BYTES) into a Zenith_VerifiedStampFile (absent or refused:
// none, logged); logs the report and keeps its rejections for the
// snapshots; keeps the file system (and so the instance guard) until shutdown step 10b, or until step 4's unwind
// (cross-area row PK22). Never fails startup: the previous content stays in effect.
Zenith_UpdateApplierReport Zenith_RuntimePackage_RunPendingInstalls(Zenith_Runtime& xRuntime);

// The stamp lookup read at the close of step 4, or null; the composition root passes it to PLAT's patch mount at step 5
// (Zenith_VirtualFileSystemDesc::m_pxUpdateStamps, cross-area row PK27 (d)) and the bridge to the service at step 11.
const Zenith_UpdateStampLookup* Zenith_RuntimePackage_GetUpdateStampLookup(const Zenith_Runtime& xRuntime);

// Closing action of startup step 11, after Zenith_RuntimeSave_CreateSystem: creates the forwarding entitlement query (its
// provider the configuration query when the composition lists no Online module, else none), the delivery (Play when
// package.zpkm's layout is ANDROID_BUNDLE with a deliverable unit; simulated, in place over <package.simulatedDelivery.root>/Packs,
// when package.simulatedDelivery is true in a non-Shipping build (Algorithms.md "Simulated deliverable units"); none otherwise), the migration registry with PACK's steps, and the package service with
// m_bShippingRules = Zenith_IsShippingBuild(), the running platform, the forwarder and, on Windows, the install file system.
// Under --replay it first forms R from the log and passes it to the installer, then waits for R's mounts (Algorithms.md
// "Replay pack set"); a missing unit or DLC revision returns PACKAGE_ERROR_REPLAY_CONTENT_MISSING, which MOD's step-11 handler
// maps to startup exit code 5. A Shipping build's absent or invalid package.zpkm, and every other error, is exit code 3.
Zenith_Result<void, Zenith_PackageError> Zenith_RuntimePackage_Create(Zenith_Runtime& xRuntime);
// The step-11 call site's mapping of a Create error to the startup exit code: PACKAGE_ERROR_REPLAY_CONTENT_MISSING is 5, every
// other error 3 (Zenith2/Docs/Design/MOD/RuntimeSeams.md "Call sites PACK's tickets add").
constexpr int Zenith_RuntimePackage_CreateErrorExitCode(Zenith_PackageError eError);

// Step 15, called by MOD's Zenith_ModuleStartContext::ProvideEntitlementQuery during a module's Start (cross-area row PK21):
// sets xProvider as the forwarder's provider (ENTITLEMENT_PROVIDER_KIND_MODULE). A second call in one process is refused by
// MOD as a rejected registration; xProvider outlives the service's use of it because the forwarder drops it at shutdown step 4.
void Zenith_RuntimePackage_SetEntitlementProvider(Zenith_Runtime& xRuntime, const Zenith_EntitlementQuery& xProvider);
// The undo of that registration (a module whose Start fails after providing) and the last action of shutdown step 4: no
// provider, every ID UNKNOWN (ENTITLEMENT_PROVIDER_KIND_NONE).
void Zenith_RuntimePackage_ClearEntitlementProvider(Zenith_Runtime& xRuntime);

// End of step 15, after Zenith_ModuleHost::StartAll returned (cross-area row PK21): when the forwarder still has no provider,
// sets the configuration query; records the trace's m_eProviderAfterModulesStarted.
void Zenith_RuntimePackage_AfterModulesStarted(Zenith_Runtime& xRuntime);

// World creation step 4 (PK15, PK22): registers the record's Zenith_PackageStateResource as "Zenith.Package.State" with its
// initial value, DeclareSource(ASYNC_SOURCE_PACKAGE, uPACKAGE_EVENT_QUEUE_CAPACITY, 96) and SetHandler on xResults, and marks
// the record as needing its snapshot; the record is owned by the caller's world host record.
Zenith_Result<Zenith_Owned<Zenith_RuntimePackageWorldRecord>, Zenith_PackageError> Zenith_RuntimePackage_AttachWorld(
	Zenith_Runtime& xRuntime, Zenith_AsyncResultTable& xResults, uint16 uWorldTag);

// Frame prologue step 1b (after Platform.Pump): builds Zenith_InstallerFrameInput (monotonic time, APP_STATE_RUNNING,
// m_bConnectivityRegained when Zenith_Platform::GetConnectivityRevision() advanced since the last prologue), RunPrologue; on m_bMountsChanged, Zenith_AssetRegistry::OnMountsChanged;
// then, outside --replay, for each attached world in tag order: its snapshot when it needs one, then each queued event in
// order, AllocateCompletionID and Complete as Algorithms.md "Event publication and replay" step 3 states; under --replay it
// completes nothing.
void Zenith_RuntimePackage_RunPrologue(Zenith_Runtime& xRuntime);

// The first call of Zenith_FrameLoop_AfterRunFrame (Zenith2/Docs/Design/MOD/RuntimeSeams.md "Call sites PACK's tickets add"),
// on the main thread after RunFrame returned: applies the Package.* commands the frame's sync-point playbacks buffered in each
// world's package record, in world-tag order, then merge order (not under --replay); an apply-time refusal queues a
// Package.PackStateChanged with the error, which is never coalesced.
void Zenith_RuntimePackage_ApplyCommands(Zenith_Runtime& xRuntime);

// Startup step 15, the last call of MOD's Zenith_RegisterEngineCommandKinds and Zenith_RegisterEngineScriptBindings, before
// StartAll and the seals: the four Package.* module command kinds, and the Package binding namespace.
bool Zenith_RuntimePackage_RegisterCommandKinds(Zenith_Runtime& xRuntime, Zenith_CommandKindTable& xKinds);
bool Zenith_RuntimePackage_RegisterScriptBindings(Zenith_Runtime& xRuntime, Zenith_ScriptBindingTable& xBindings);

// The bridge's event queue (Algorithms.md "Event publication and replay" step 2): the installer's and the service's events
// of the prologue enter here; uPACKAGE_EVENT_QUEUE_CAPACITY, QUEUE_FULL_POLICY_DROP_OLDEST, error-free state events coalesced.
void Zenith_RuntimePackage_QueueEvent(Zenith_Runtime& xRuntime, const Zenith_PackageEvent& xEvent);

// Shutdown step 4: the service's BeginShutdown, then PumpShutdown and Zenith_SleepUs(1000) in a loop until it returns true or
// package.shutdownDrainMs passed (Algorithms.md "Shutdown drain"), then, only when the delivery state is dirty and
// bDeviceLossExit is false, the delivery state file (an in-flight write waited for within the same bound, then one
// WriteAtomic, waited for); with bDeviceLossExit true nothing is written or waited for. Then the forwarder's provider is
// cleared (every ID UNKNOWN), before any module is destroyed in step 6. The composition root passes bDeviceLossExit true
// exactly on RHI's device-loss clean exit, the exit for which it called Zenith_RuntimeSave_RequestAutosaveForExit.
void Zenith_RuntimePackage_BeginShutdown(Zenith_Runtime& xRuntime, bool bDeviceLossExit);
void Zenith_RuntimePackage_Destroy(Zenith_Runtime& xRuntime);         // shutdown step 6, after the modules: the service, then the forwarder
// Shutdown step 10b, after Zenith_VirtualFileSystem and before Zenith_Platform (Contracts.md "Shutdown ownership order"); also the first
// action of startup step 4's unwind (cross-area row PK22), so an injected failure after step 4 leaves no lock held.
void Zenith_RuntimePackage_ReleaseInstanceGuard(Zenith_Runtime& xRuntime);
const Zenith_RuntimePackageStartupTrace& Zenith_RuntimePackage_GetStartupTrace(const Zenith_Runtime& xRuntime);
```

| File | Content |
|---|---|
| `Zenith2/Engine/ZenithRuntime/Internal/Package/Zenith_RuntimePackage.h`, `.cpp` | the functions above and the bridge state (the install file system, the report, the service, the trace) |
| `Zenith2/Engine/ZenithRuntime/Internal/Package/Zenith_PackageEvents.cpp` | registration, in this order, of the five `Package.*` event types as `TYPE_KIND_EVENT`; of the element struct `Zenith.Package.UnitState` (`Zenith_PackageUnitState`, `TYPE_KIND_STRUCT`, 24 bytes) with five transient properties, `UnitIndex` (`PROPERTY_TYPE_U16`), `State` (`PROPERTY_TYPE_ENUM` over the C++ enumeration `Zenith_AssetPackState`, 1 byte), `Error` (`PROPERTY_TYPE_U8`: a `Zenith_PackageError` value or 0), `BytesDone` and `BytesTotal` (`PROPERTY_TYPE_U64`), its `m_uReserved` padding being no property; and of `Zenith.Package.State` (`Zenith_PackageStateResource`, `TYPE_KIND_RESOURCE`) with five transient properties, `UpdateKind` (`PROPERTY_TYPE_ENUM` over `Zenith_UpdateKind`, 1 byte), `FromVersion` and `ToVersion` (`PROPERTY_TYPE_FIXED_ARRAY` of 3 `PROPERTY_TYPE_U16` elements, `m_uArrayCount` 3), `UnitCount` (`PROPERTY_TYPE_U16`) and `Units` (`PROPERTY_TYPE_FIXED_ARRAY` with `m_xElementType` the type ID of `Zenith.Package.UnitState` and `m_uArrayCount` 256, `uPACKAGE_MAX_STATE_UNITS`), its reserved members being no property. No property carries `PROPERTY_FLAG_SERIALIZED`; the member-to-value-type mapping (the C arrays `T[N]` and the reflected enumerations) is `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectValueTraits header"`; both type names are unique programme-wide (X353-SCRIPTNAMES); `Zenith_EncodePackageEvent`, `Zenith_DecodePackageEvent`, `Zenith_RuntimePackageWorldRecord` with its deferral queue and its snapshot |
| `Zenith2/Engine/ZenithRuntime/Internal/Package/Zenith_PackageCommands.cpp` | the four module command kinds of "Script-facing API", registered at startup step 15 by `Zenith_RuntimePackage_RegisterCommandKinds` and applied by `Zenith_RuntimePackage_ApplyCommands` after `RunFrame` by calling the installer |
| `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/Zenith_PackageScriptBindings.cpp` | the Luau functions |
| `Zenith2/Engine/ZenithRuntime/Tests/Zenith_RuntimePackage_Tests.cpp` | the runtime-bridge tests of `Zenith2/Docs/Design/PACK/Tests.md → "Runtime bridge tests"` |

## Script-facing API

Registered by the composition root through the Luau-free binding ABI (`Zenith2/Docs/Scripting.md → "Modules, bindings and generated types"`), following the PLAT pattern of `Zenith2/Docs/Design/PLAT/Filesystem.md → "Low-disk detection and behaviour"`: reads come from the world resource `Zenith.Package.State` (`Zenith_PackageStateResource`, declared in "Runtime bridge declarations"), which the `ASYNC_SOURCE_PACKAGE` handler refreshes at tick boundaries from recorded publications (every world's snapshot first, Algorithms.md "World snapshot"), and, for a unit with the PRESENT flag and for names and sizes, from the immutable `package.zpkm`, so every read is replay-deterministic; requests are buffered module commands, applied after the transaction commits. The binding ABI enforces the contexts column generically: a call from a context the row does not list raises ZS1702 (`Zenith2/Docs/Scripting/MisuseCatalogue.md`), so PACK catalogues no context code of its own.

| Luau function | Contexts | Effect | Returns or does |
|---|---|---|---|
| `Package.GetPackState(ctx, pack)` | system, behaviour | read world (`Zenith_PackageStateResource`) | the state name of `Zenith_ToString(Zenith_AssetPackState)` (`"NotInstalled"`, `"Downloading"`, `"Mounted"`, …); `"Unknown"` in a world whose snapshot has not published yet; a unit with the PRESENT flag is always `"Mounted"` |
| `Package.GetPackProgress(ctx, pack)` | system, behaviour | read world | number in [0, 1] |
| `Package.GetPackSizeBytes(ctx, pack)` | system, behaviour | read world | number (package.zpkm's unit bytes; 0 for `base`) |
| `Package.IsMounted(ctx, pack)` | system, behaviour | read world | boolean; true for every unit with the PRESENT flag |
| `Package.ListPacks(ctx)` | behaviour | read world | array of unit names in package.zpkm order, excluding `base` |
| `Package.GetUpdateKind(ctx)` | behaviour | read world | `"None"`, `"FirstRun"`, `"Content"`, `"WithinLine"`, `"NewLine"`, `"Downgrade"` |
| `Package.RequestPack(ctx, pack)` | behaviour, system | buffered (module command kind `Package.Fetch`) | nothing; applied at the next sync point |
| `Package.CancelPack(ctx, pack)` | behaviour, system | buffered (`Package.Cancel`) | nothing |
| `Package.RemovePack(ctx, pack)` | behaviour | buffered (`Package.Remove`) | nothing |
| `Package.Confirm(ctx)` | behaviour | buffered (`Package.Confirm`) | nothing; shows Play's confirmation dialog for every pack waiting for Wi-Fi or for consent |

Events, registered as `TYPE_KIND_EVENT` and delivered in phase 9 of the tick they are published at:

| Event type | Fields |
|---|---|
| `Package.PackStateChanged` | `Pack` (string), `State` (state name), `Error` (`""` or the `Zenith_PackageError` name without its prefix, `"NetworkUnavailable"`), `BytesDone`, `BytesTotal` (numbers); a request refused when its command was applied (`PackBusy`, `EntitlementDenied`, `NotDeliverable`, `UnknownPack`) arrives as this event with the unchanged state and the error |
| `Package.Updated` | `Kind` (update kind name), `From`, `To` (`"MAJOR.MINOR.PATCH"` strings) |
| `Package.UpdateFailed` | `Kind` (`"Patch"`, `"Dlc"`), `Name` (version or DLC name), `Error` |
| `Package.DlcStateChanged` | `Dlc`, `State`, `Error` |
| `Package.ConfirmationRequired` | `Pack`, `Reason` (`"Cellular"`: Play waits for Wi-Fi and the user may allow cellular data; `"Consent"`: Play requires the user's consent) |

Misuse errors, raised as controlled Luau errors at the call (never asserts), in PACK's block ZS2600–ZS2699 of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" (MISUSE-BLOCKS-2):

| Code | Kind | Raised when | Message |
|---|---|---|---|
| ZS2601 | MISUSE | `pack` is not a string or names no unit of package.zpkm | `Package: unknown pack '<pack>'` |
| ZS2602 | MISUSE | `RequestPack`, `CancelPack` or `RemovePack` names a unit with the PRESENT flag (the base, a folded or an install-time unit) | `Package: pack '<pack>' is installed with the app and cannot be requested, cancelled or removed` |
| ZS2603 | MISUSE | more than 16 package commands from one invocation (counted by the binding per invocation; the command buffer itself has no separate limit or error) | `Package: at most 16 package requests per invocation` |

<!-- informative -->
Requests are commands rather than direct calls because a script invocation is a transaction (Z2-D-016): a fetch issued by an invocation that then rolls back must not have happened, and the command buffer gives exactly that. A pack becoming mounted is an asynchronous result, published at a tick boundary in completion-ID order as an `ASYNC_SOURCE_PACKAGE` result of a recorded kind (Z2-D-053), so a replay reproduces the same `Package.PackStateChanged` sequence without Play.
<!-- /informative -->
