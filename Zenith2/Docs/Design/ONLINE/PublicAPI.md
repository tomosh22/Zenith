# ONLINE — public API

## Scope of this file

This file is part of the ONLINE design family; the entry document is `Zenith2/Docs/Design/ONLINE.md`. It specifies every public header of the six ONLINE libraries (`ZenithOnline`, `ZenithOnline_Offline`, `ZenithOnline_SteamCore`, `ZenithOnline_Steam`, `ZenithOnline_GooglePlayCore`, `ZenithOnline_GooglePlay`), the internal declarations other ONLINE files share, the Luau surface (`Online` namespace), the event types, the configuration keys and the misuse codes. Byte layouts of every payload and file are `Zenith2/Docs/Design/ONLINE/Formats.md`; algorithms are `Zenith2/Docs/Design/ONLINE/Accounts.md`, `Zenith2/Docs/Design/ONLINE/CloudSave.md` and `Zenith2/Docs/Design/ONLINE/Backends.md`.

## Rules across the ONLINE API

- Every public header follows `Zenith2/Docs/Contracts.md` → "API boundary": `#pragma once`, self-contained, no STL type, no third-party header (no Steamworks, JNI or Play type ever reaches a public header).
- Every namespace-scope type carries one contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Contract blocks per type"; the blocks below are the ones the headers carry.
- The API library `ZenithOnline` exports no backend factory ([Z2-N-CONTRACT-038]); each backend library's public root header declares its factory and nothing else a consumer could depend on, except the offline backend's test-access header, whose name ends `TestAccess.h` so `TestSeparationCheck` rule 6 confines it to tests; the two store core libraries are engine libraries, not backends, whose root headers declare the store seams, their scripted implementations and the factory over a seam (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove"), each with a `TestAccess` header of its own (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates").
- No public type holds a raw platform account identifier outside `Zenith_OnlineAccountID`, whose contract forbids logging it; no public function returns a credential, token or purchase token, because ONLINE never holds one (`Zenith2/Docs/Design/ONLINE.md` → "Security assumptions").
- No public type is a test double; tests drive the real `Zenith_OnlineSession` over the real offline backend, and the Steam and Google Play backend logic over their declared seams' scripted recording implementations (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Steam API seam", "Play bridge seam"), selected by configuration and refused in Shipping (FINAL-DOUBLES).
- Strings are UTF-8 `Zenith_StringView`s (`Zenith2/Docs/Design/CORE.md` → "Public API"); fixed-capacity name values hold their bytes inline and never allocate.

## Header list

| Header | Library | Declares | Stage |
|---|---|---|---|
| `Zenith2/Engine/ZenithOnline/Zenith_OnlineTypes.h` | ZenithOnline | constants, `Zenith_OnlineBackendKind`, `Zenith_OnlineAccountID`, `Zenith_OnlineAccountKey`, `Zenith_OnlineDisplayName`, `Zenith_OnlineIdentityState`, `Zenith_OnlineIdentitySnapshot`, `Zenith_OnlineEntitlementState`, `Zenith_OnlineAchievementState`, `Zenith_OnlineCloudRevision`, `Zenith_OnlineStatus`, capability bits | S4 |
| `Zenith2/Engine/ZenithOnline/Zenith_OnlineErrors.h` | ZenithOnline | `Zenith_OnlineErrorCode`, `Zenith_OnlineError`, `Zenith_ToString` overloads | S4 |
| `Zenith2/Engine/ZenithOnline/Zenith_OnlineCatalogue.h` | ZenithOnline | `Zenith_OnlineAchievementKind`, `Zenith_OnlineConflictPolicy`, `Zenith_OnlineAchievementDef`, `Zenith_OnlineEntitlementDef`, `Zenith_OnlineCloudPolicy`, `Zenith_OnlineCatalogue` | S4 |
| `Zenith2/Engine/ZenithOnline/Zenith_OnlineServices.h` | ZenithOnline | `Zenith_OnlineRequestID`, `Zenith_OnlineRequestKind`, `Zenith_OnlineRequest`, `Zenith_OnlineCompletion`, `Zenith_OnlineInvalidation`, `Zenith_OnlineEventSink`, `Zenith_OnlineServices`, the backend settings and `Zenith_OnlineServicesDesc`, `Zenith_PfnCreateOnlineServices`, `Zenith_OnlineBackendEntry` | S4 |
| `Zenith2/Engine/ZenithOnline/Zenith_OnlineSession.h` | ZenithOnline | `Zenith_OnlinePublicationPort`, `Zenith_OnlineSessionState`, `Zenith_OnlineCloudSlotState`, `Zenith_OnlineConflictReason`, `Zenith_OnlineResolveChoice`, `Zenith_OnlineSessionSettings`, `Zenith_OnlineSessionDesc`, `Zenith_OnlineIdentityView`, `Zenith_OnlineConflictToken`, `Zenith_OnlineCloudSlotStatus`, `Zenith_OnlineSessionStats`, `Zenith_OnlineSession` | S4 (the package entitlement accessor S6) |
| `Zenith2/Engine/ZenithOnline/Zenith_Module_Online.h` | ZenithOnline | `Zenith_Module_Online_RegisterTypes`, `Zenith_Module_Online_Start` | S4 |
| `Zenith2/Engine/ZenithOnline_Offline/Zenith_Online_Offline.h` | ZenithOnline_Offline | `Zenith_CreateOnlineServices_Offline` | S4 |
| `Zenith2/Engine/ZenithOnline_Offline/Zenith_OnlineOfflineTestAccess.h` | ZenithOnline_Offline | `Zenith_OnlineOfflineTestAccess` | S4 |
| `Zenith2/Engine/ZenithOnline_SteamCore/Zenith_OnlineSteamCore.h` | ZenithOnline_SteamCore | `Zenith_SteamCallbackSink`, `Zenith_SteamApi`, `Zenith_SteamApiScriptedDesc`, `Zenith_CreateSteamApiScripted`, `Zenith_CreateOnlineServicesOverSteamApi` | S6 |
| `Zenith2/Engine/ZenithOnline_SteamCore/Zenith_OnlineSteamCoreTestAccess.h` | ZenithOnline_SteamCore | `Zenith_SteamRecordedCall`, `Zenith_OnlineSteamCoreTestAccess` | S6 |
| `Zenith2/Engine/ZenithOnline_Steam/Zenith_Online_Steam.h` | ZenithOnline_Steam | `Zenith_CreateOnlineServices_Steam` | S6 |
| `Zenith2/Engine/ZenithOnline_GooglePlayCore/Zenith_OnlineGooglePlayCore.h` | ZenithOnline_GooglePlayCore | `Zenith_PlayEventSink`, `Zenith_PlayBridge`, `Zenith_PlayBridgeScriptedDesc`, `Zenith_PlayEventQueue`, `Zenith_CreatePlayBridgeScripted`, `Zenith_CreateOnlineServicesOverPlayBridge` | S6 |
| `Zenith2/Engine/ZenithOnline_GooglePlayCore/Zenith_OnlineGooglePlayCoreTestAccess.h` | ZenithOnline_GooglePlayCore | `Zenith_PlayRecordedCall`, `Zenith_OnlineGooglePlayCoreTestAccess` | S6 |
| `Zenith2/Engine/ZenithOnline_GooglePlay/Zenith_Online_GooglePlay.h` | ZenithOnline_GooglePlay | `Zenith_CreateOnlineServices_GooglePlay` | S6 |

The backend header names follow `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping": `<Lib>/Zenith_<Family>_<BACKEND_NAME>.h` with the family `Online` and the backend names `Offline`, `Steam` and `GooglePlay`, so the generated composition's `#include "ZenithOnline_Offline/Zenith_Online_Offline.h"` and its `{ "Offline", &Zenith_CreateOnlineServices_Offline }` entry resolve unchanged (`Zenith2/Docs/Architecture/CompositionRoot.md` → "The generated composition table").

## Zenith_OnlineTypes header

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_OnlineTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Span.h"

inline constexpr uint32 uONLINE_MAX_NATIVE_ACCOUNT_BYTES = 80;     // a backend's own account identifier
inline constexpr uint32 uONLINE_MAX_ACCOUNT_ID_BYTES = 96;         // "<prefix>:<native>"
inline constexpr uint32 uONLINE_MAX_DISPLAY_NAME_BYTES = 64;       // UTF-8, truncated at a code-point boundary
inline constexpr uint32 uONLINE_MAX_CATALOGUE_ID_BYTES = 64;       // achievement and entitlement IDs
inline constexpr uint32 uONLINE_MAX_NATIVE_ID_BYTES = 128;         // a backend's achievement name, product ID, stat name
inline constexpr uint32 uONLINE_MAX_ACHIEVEMENTS = 1024;
inline constexpr uint32 uONLINE_MAX_ENTITLEMENTS = 256;
inline constexpr uint32 uONLINE_MAX_CLOUD_SLOTS = 64;              // slots one profile may keep in the cloud
inline constexpr uint32 uONLINE_RESULT_MAX_BYTES = 256;            // every ASYNC_SOURCE_ONLINE payload (results and notifications)
inline constexpr uint32 uONLINE_REQUEST_MAX_BYTES = 80;            // every script request payload
inline constexpr uint32 uONLINE_NOTIFY_MAX_OUTSTANDING = 8;        // parked, unpublished notifications per world
inline constexpr uint32 uONLINE_NOTIFY_QUEUE_CAPACITY = 96;        // notifications waiting for a parking slot per world (≥ 64 conflicts + 32)
inline constexpr uint32 uONLINE_CLOUD_ENVELOPE_HEADER_BYTES = 320;
inline constexpr uint32 uONLINE_MAX_CLOUD_LIST_OBJECTS = 1024;      // objects one CLOUD_LIST may report
inline constexpr uint32 uONLINE_MAX_CLOUD_OBJECT_NAME_BYTES = 69;   // "z2.<slot>.<16 hex>.<16 hex>"
inline constexpr uint32 uONLINE_MAX_ATTACHED_WORLDS = 8;           // worlds attached at once (a play world, a transition target, editor worlds)
inline constexpr uint64 ulONLINE_MAX_CLOUD_OBJECT_BYTES = 268435456ull + 320ull;   // the largest ZSAV file plus the envelope header

/// @contract enum Zenith_OnlineBackendKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. A byte outside the enumerators read from a file or payload is ONLINE_ERROR_CORRUPT at the reader.
/// Version: since Z2 Stage 4. Serialized as one byte in the cloud envelope, the cloud index and the sync manifest;
///   values are never renumbered or reused.
/// @endcontract
enum Zenith_OnlineBackendKind : uint8
{
	ONLINE_BACKEND_KIND_NONE = 0,          // no backend (replay mode, or before Start)
	ONLINE_BACKEND_KIND_OFFLINE = 1,
	ONLINE_BACKEND_KIND_STEAM = 2,
	ONLINE_BACKEND_KIND_GOOGLE_PLAY = 3,
};
constexpr const char* Zenith_ToString(Zenith_OnlineBackendKind eKind);   // "None", "Offline", "Steam", "GooglePlay"
constexpr Zenith_StringView Zenith_GetOnlineAccountPrefix(Zenith_OnlineBackendKind eKind);
	// "offline", "steam", "gpgs"; empty for NONE

/// @contract value Zenith_OnlineAccountID
/// Ownership: Plain value holding up to uONLINE_MAX_ACCOUNT_ID_BYTES bytes inline and a length; never allocates.
/// Thread affinity: IMMUTABLE once constructed; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: Bytes of printable ASCII (0x21 … 0x7E).
/// Errors: Zenith_MakeOnlineAccountID returns ONLINE_ERROR_INVALID_ID for a NONE backend, an empty native identifier, one
///   longer than uONLINE_MAX_NATIVE_ACCOUNT_BYTES or one holding a byte outside 0x21 … 0x7E.
/// Version: since Z2 Stage 4. The value is personal data: it is never logged, counted, published to a world, written into
///   a replay log or a save, and never leaves the process except into the save profile key derivation
///   (Zenith_MakeSaveProfileKey, which hashes it) and the account key (Zenith_DeriveOnlineAccountKey, Internal, which hashes it
///   keyed with the install secret). Account comparisons inside the session compare whole IDs, never keys.
/// @endcontract
struct Zenith_OnlineAccountID
{
	char m_acBytes[uONLINE_MAX_ACCOUNT_ID_BYTES] = {};
	uint8 m_uLength = 0;              // 0: no account
	Zenith_StringView GetView() const;
	bool IsEmpty() const { return m_uLength == 0; }
	friend constexpr bool operator==(const Zenith_OnlineAccountID&, const Zenith_OnlineAccountID&) = default;
};
// "<prefix>:<native>", for example "steam:76561197960287930", "gpgs:a_1234567890123456789", "offline:local".
Zenith_Result<Zenith_OnlineAccountID, Zenith_OnlineError> Zenith_MakeOnlineAccountID(Zenith_OnlineBackendKind eBackend,
	Zenith_StringView strNativeID);

/// @contract value Zenith_OnlineAccountKey
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the derivation (Internal, Zenith2/Docs/Design/ONLINE/Accounts.md → "Account identifiers and keys") cannot fail.
/// Version: since Z2 Stage 4. The value PLAT's account signals carry (Zenith_AccountSignal::m_ulAccountKey) and PLAT records in
///   replay logs: LE64 of the first 8 bytes of SHA-256(install secret ‖ account ID bytes), with 0 reserved for "no account"
///   (Zenith_OnlineAccountKeyFromHash maps 0 to 1). Keyed per install, so it cannot be inverted without the secret, which never
///   leaves the device record; compared only within one process, so its values carry no meaning across installs or replays.
/// @endcontract
struct Zenith_OnlineAccountKey
{
	uint64 m_ulValue = 0;
	friend constexpr bool operator==(Zenith_OnlineAccountKey, Zenith_OnlineAccountKey) = default;
};

/// @contract value Zenith_OnlineDisplayName
/// Ownership: Plain value holding up to uONLINE_MAX_DISPLAY_NAME_BYTES bytes of UTF-8 inline.
/// Thread affinity: IMMUTABLE once constructed; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: Zenith_MakeOnlineDisplayName never fails: invalid UTF-8 sequences become U+FFFD, the text is truncated at the last
///   code point that fits, and control characters (U+0000 … U+001F, U+007F) are removed.
/// Version: since Z2 Stage 4. Personal data under the same rule as Zenith_OnlineAccountID, except that it reaches a world when
///   online.exposeDisplayName is true and the process is not recording a replay (the module turns exposure off under --record).
/// @endcontract
struct Zenith_OnlineDisplayName
{
	char m_acBytes[uONLINE_MAX_DISPLAY_NAME_BYTES] = {};
	uint8 m_uLength = 0;
	Zenith_StringView GetView() const;
};
Zenith_OnlineDisplayName Zenith_MakeOnlineDisplayName(Zenith_StringView strUtf8);

/// @contract enum Zenith_OnlineIdentityState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_OnlineIdentityState : uint8
{
	ONLINE_IDENTITY_STATE_SIGNED_OUT = 1,
	ONLINE_IDENTITY_STATE_SIGNING_IN = 2,
	ONLINE_IDENTITY_STATE_SIGNED_IN = 3,
};

/// @contract value Zenith_OnlineIdentitySnapshot
/// Ownership: Plain value returned by Zenith_OnlineServices::GetIdentity.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None; it describes the backend at the time of the call.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineIdentitySnapshot
{
	Zenith_OnlineIdentityState m_eState = ONLINE_IDENTITY_STATE_SIGNED_OUT;
	Zenith_OnlineAccountID m_xAccount;          // empty unless SIGNED_IN
	Zenith_OnlineDisplayName m_xDisplayName;    // empty unless SIGNED_IN
	bool m_bConnected = false;                  // the backend can reach its service now (Steam BLoggedOn, Play connectivity)
};

/// @contract enum Zenith_OnlineEntitlementState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte in results; the values equal PACK's Zenith_EntitlementState values
///   (UNKNOWN 1, GRANTED 2, DENIED 3), which Zenith_OnlinePackEntitlements maps one to one from Stage 6.
/// @endcontract
enum Zenith_OnlineEntitlementState : uint8
{
	ONLINE_ENTITLEMENT_STATE_UNKNOWN = 1,
	ONLINE_ENTITLEMENT_STATE_GRANTED = 2,
	ONLINE_ENTITLEMENT_STATE_DENIED = 3,
};

/// @contract value Zenith_OnlineAchievementState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: Steps.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineAchievementState
{
	bool m_bUnlocked = false;
	uint32 m_uSteps = 0;              // 0 for a standard achievement; ≤ m_uTotalSteps
	uint32 m_uTotalSteps = 0;         // 0 for a standard achievement
};

/// @contract value Zenith_OnlineCloudRevision
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. An opaque 32-byte token a backend assigns to one stored cloud object version; only
///   equality is meaningful; all-zero means "no object". The derivation per backend is Zenith2/Docs/Design/ONLINE/Backends.md
///   → "Request completion rules".
/// @endcontract
struct Zenith_OnlineCloudRevision
{
	uint8 m_auBytes[32] = {};
	bool IsNone() const;
	friend constexpr bool operator==(const Zenith_OnlineCloudRevision&, const Zenith_OnlineCloudRevision&) = default;
};

/// @contract enum Zenith_OnlineStatus
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. The outcome of one backend request and of one script request; serialized as one byte in result payloads
///   (Zenith2/Docs/Design/ONLINE/Formats.md → "Script result payloads"); values are never renumbered or reused.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_OnlineStatus : uint8
{
	ONLINE_STATUS_OK = 1,
	ONLINE_STATUS_NOT_SIGNED_IN = 2,
	ONLINE_STATUS_ACCOUNT_CHANGED = 3,      // the account changed after the request was accepted
	ONLINE_STATUS_ACCOUNT_CHANGING = 4,     // a rebind was in progress when the request was taken
	ONLINE_STATUS_UNKNOWN_ID = 5,           // the ID is valid but not in the catalogue
	ONLINE_STATUS_NOT_SUPPORTED = 6,        // the backend lacks the capability
	ONLINE_STATUS_UNAVAILABLE = 7,          // the backend cannot reach its service
	ONLINE_STATUS_TIMEOUT = 8,              // online.requestTimeoutMs elapsed
	ONLINE_STATUS_REFUSED = 9,              // a precondition of the operation does not hold (named per operation)
	ONLINE_STATUS_BACKEND_ERROR = 10,       // any other backend failure
	ONLINE_STATUS_ABANDONED = 11,           // the world detached, or a save restore abandoned the request
	ONLINE_STATUS_CLOUD_DISABLED = 12,
	ONLINE_STATUS_NOT_IN_CONFLICT = 13,     // a resolution for a slot that is not in CONFLICT
	ONLINE_STATUS_CANCELLED = 14,           // backend requests only: Cancel was called
	ONLINE_STATUS_SHUTTING_DOWN = 15,
	ONLINE_STATUS_REVISION_MISMATCH = 16,   // backend requests only: a conditional cloud write lost the race
	ONLINE_STATUS_NOT_FOUND = 17,           // backend requests only: no such cloud object
	ONLINE_STATUS_QUOTA = 18,               // backend requests only: the cloud quota is exhausted
	ONLINE_STATUS_TOO_LARGE = 19,           // the object exceeds the backend's cloud object limit
	ONLINE_STATUS_STALE = 20,               // a conflict resolution whose token no longer matches the inputs (CloudSave.md "Resolution choices")
};
constexpr const char* Zenith_ToString(Zenith_OnlineStatus eStatus);   // the Luau status strings of
	// Zenith2/Docs/Design/ONLINE/PublicAPI.md → "Script-facing API": "ok", "notSignedIn", "accountChanged", …

// Capability bits a backend reports through Zenith_OnlineServices::GetCapabilities.
inline constexpr uint32 uONLINE_CAPABILITY_IDENTITY = 1u << 0;
inline constexpr uint32 uONLINE_CAPABILITY_ENTITLEMENTS = 1u << 1;
inline constexpr uint32 uONLINE_CAPABILITY_ACHIEVEMENTS = 1u << 2;
inline constexpr uint32 uONLINE_CAPABILITY_INCREMENTAL_ACHIEVEMENTS = 1u << 3;
inline constexpr uint32 uONLINE_CAPABILITY_CLOUD = 1u << 4;
inline constexpr uint32 uONLINE_CAPABILITY_CLOUD_CONDITIONAL_WRITE = 1u << 5;   // CLOUD_WRITE honours m_xExpectedRevision
inline constexpr uint32 uONLINE_CAPABILITY_INTERACTIVE_SIGN_IN = 1u << 6;
inline constexpr uint32 uONLINE_CAPABILITY_SIGN_OUT = 1u << 7;
inline constexpr uint32 uONLINE_CAPABILITY_SWITCH_ACCOUNT = 1u << 8;
inline constexpr uint32 uONLINE_CAPABILITY_ENTITLEMENTS_WITHOUT_SIGN_IN = 1u << 9;   // ownership comes from a store account, not the signed-in identity
```

The capability matrix per backend is `Zenith2/Docs/Design/ONLINE/Backends.md` → "Capability matrix".

## Zenith_OnlineErrors header

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_OnlineErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithOnline/Zenith_OnlineTypes.h"

/// @contract enum Zenith_OnlineErrorCode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The code enumeration of Zenith_OnlineError; each enumerator's meaning is the comment beside it; numbered from 1.
/// Version: since Z2 Stage 4. An enumerator keeps its meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_OnlineErrorCode : uint8
{
	ONLINE_ERROR_INVALID_DESC = 1,          // a desc field out of range (named in the log)
	ONLINE_ERROR_OUT_OF_MEMORY = 2,
	ONLINE_ERROR_BACKEND_UNAVAILABLE = 3,   // the backend could not start: SDK absent, client not running, Java bridge missing
	ONLINE_ERROR_NOT_SUPPORTED = 4,         // a request kind the backend's capabilities exclude
	ONLINE_ERROR_NOT_SIGNED_IN = 5,
	ONLINE_ERROR_ACCOUNT_CHANGED = 6,
	ONLINE_ERROR_QUEUE_FULL = 7,            // the backend's in-flight table (m_uMaxInFlight) is full
	ONLINE_ERROR_CANCELLED = 8,
	ONLINE_ERROR_TIMEOUT = 9,
	ONLINE_ERROR_NETWORK = 10,              // the service is unreachable
	ONLINE_ERROR_UNKNOWN_ID = 11,
	ONLINE_ERROR_INVALID_ID = 12,           // an ID or name violating its grammar
	ONLINE_ERROR_CLOUD_DISABLED = 13,       // the user or the application disabled cloud storage
	ONLINE_ERROR_CLOUD_QUOTA = 14,
	ONLINE_ERROR_CLOUD_TOO_LARGE = 15,
	ONLINE_ERROR_CLOUD_REVISION_MISMATCH = 16,
	ONLINE_ERROR_CLOUD_NOT_FOUND = 17,
	ONLINE_ERROR_CORRUPT = 18,              // untrusted bytes (a cooked catalogue, a cloud object, an ONLINE file) failed validation
	ONLINE_ERROR_IO = 19,                   // a user-mount operation failed; m_iNativeCode the Zenith_IOError code
	ONLINE_ERROR_SAVE = 20,                 // a ZenithSave call failed; m_uDetail the Zenith_SaveErrorCode
	ONLINE_ERROR_BACKEND = 21,              // any other native failure; m_iNativeCode the native code
	ONLINE_ERROR_SHUTTING_DOWN = 22,
	ONLINE_ERROR_REPLAY = 23,               // refused because the process runs --replay
	ONLINE_ERROR_CATALOGUE = 24,            // the online catalogue asset could not be loaded or does not match the game
	ONLINE_ERROR_CONFLICT_STALE = 25,       // ResolveCloudConflict: the token differs from the conflict record's
};
constexpr const char* Zenith_ToString(Zenith_OnlineErrorCode eCode);   // "ONLINE_ERROR_INVALID_DESC", …

/// @contract value Zenith_OnlineError
/// Ownership: Plain value; the error record of [Z2-N-CONTRACT-480]: 8 bytes, string-free.
/// Thread affinity: IMMUTABLE; members non-const: Zenith_Result and error records copy-assign it.
/// Invalidation: None.
/// Units: m_iNativeCode is the backend's own code: Steam EResult or ESteamAPIInitResult, a Play Games status code, a Play
///   Billing response code, or the Zenith_IOError code for ONLINE_ERROR_IO; m_uDetail per code (ONLINE_ERROR_SAVE: the
///   Zenith_SaveErrorCode; ONLINE_ERROR_CORRUPT: the 1-based rule number of the failed check in
///   Zenith2/Docs/Design/ONLINE/Formats.md; 0 otherwise).
/// Errors: None of its own.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineError
{
	Zenith_OnlineErrorCode m_eCode;
	Zenith_OnlineBackendKind m_eBackend = ONLINE_BACKEND_KIND_NONE;
	uint16 m_uDetail = 0;
	sint32 m_iNativeCode = 0;
};
static_assert(sizeof(Zenith_OnlineError) == 8);
```

## Zenith_OnlineCatalogue header

The runtime form of the game's online catalogue (achievements, entitlements, the cloud policy); its authoring document and cooked format are `Zenith2/Docs/Design/ONLINE/Formats.md` → "Online catalogue document" and "Cooked online catalogue".

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_OnlineCatalogue.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithOnline/Zenith_OnlineErrors.h"

class Zenith_Heap;

/// @contract enum Zenith_OnlineAchievementKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte in the cooked catalogue.
/// @endcontract
enum Zenith_OnlineAchievementKind : uint8
{
	ONLINE_ACHIEVEMENT_KIND_STANDARD = 1,      // locked or unlocked
	ONLINE_ACHIEVEMENT_KIND_INCREMENTAL = 2,   // unlocks when its steps reach m_uTotalSteps
};

/// @contract enum Zenith_OnlineConflictPolicy
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte in the cooked catalogue.
/// @endcontract
enum Zenith_OnlineConflictPolicy : uint8
{
	ONLINE_CONFLICT_POLICY_ASK = 1,                     // publish Online.CloudConflict and wait for a resolution
	ONLINE_CONFLICT_POLICY_PREFER_NEWEST_UTC = 2,
	ONLINE_CONFLICT_POLICY_PREFER_LONGEST_PLAYTIME = 3,
	ONLINE_CONFLICT_POLICY_PREFER_LOCAL = 4,
	ONLINE_CONFLICT_POLICY_PREFER_CLOUD = 5,
};

/// @contract value Zenith_OnlineAchievementDef
/// Ownership: Plain value; the views borrow the catalogue's string pool.
/// Thread affinity: IMMUTABLE; members non-const: Parse fills the catalogue's arrays field by field.
/// Invalidation: With its catalogue.
/// Units: Steps.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineAchievementDef
{
	Zenith_StringView m_strID;                 // ^[a-z][a-z0-9_.]{0,63}$
	Zenith_OnlineAchievementKind m_eKind = ONLINE_ACHIEVEMENT_KIND_STANDARD;
	bool m_bHidden = false;                    // presentation hint only
	uint32 m_uTotalSteps = 0;                 // 2 … 1,000,000 for INCREMENTAL, 0 for STANDARD
	Zenith_StringView m_strSteamName;          // the Steamworks API name; empty: not on Steam
	Zenith_StringView m_strSteamProgressStat;  // INCREMENTAL on Steam: the INT stat holding the steps
	Zenith_StringView m_strPlayID;             // the Play Games achievement ID; empty: not on Play
};

/// @contract value Zenith_OnlineEntitlementDef
/// Ownership: Plain value; the views borrow the catalogue's string pool.
/// Thread affinity: IMMUTABLE; members non-const: Parse fills the catalogue's arrays field by field.
/// Invalidation: With its catalogue.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineEntitlementDef
{
	Zenith_StringView m_strID;                 // ^[a-z][a-z0-9_.]{0,63}$, PACK's entitlement grammar
	uint32 m_uSteamAppID = 0;                 // the DLC's Steam app ID; 0: not on Steam
	Zenith_StringView m_strPlayProductID;      // the Play Billing one-time product ID; empty: not on Play
	bool m_bOfflineGranted = false;            // the offline backend grants it without configuration
};

/// @contract value Zenith_OnlineCloudPolicy
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: Parse fills it field by field.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineCloudPolicy
{
	uint8 m_uSyncedKindMask = 0x07;           // bit (k − 1) for Zenith_SaveKind k: MANUAL, QUICK, AUTOSAVE by default;
	                                           // CHECKPOINT (bit 3) only when the document asks; RESUME and REPLAY_CHECKPOINT never
	Zenith_OnlineConflictPolicy m_eManualPolicy = ONLINE_CONFLICT_POLICY_ASK;      // MANUAL and QUICK slots
	Zenith_OnlineConflictPolicy m_eAutosavePolicy = ONLINE_CONFLICT_POLICY_PREFER_NEWEST_UTC;   // AUTOSAVE and CHECKPOINT slots
	uint32 m_uMaxSlots = 32;                  // 1 … uONLINE_MAX_CLOUD_SLOTS
};

/// @contract type Zenith_OnlineCatalogue
/// Ownership: Created by Parse or CreateEmpty as a Zenith_Owned on the given heap; owned by the Zenith_OnlineSession's
///   module instance for the process lifetime; owns one string pool and the two sorted definition arrays.
/// Thread affinity: IMMUTABLE after creation; private members non-const: Parse and CreateEmpty fill them after allocating the catalogue.
/// Invalidation: Definitions and views are valid for the catalogue's lifetime.
/// Units: None.
/// Errors: Parse returns ONLINE_ERROR_CORRUPT (m_uDetail the rule of Formats.md "Cooked online catalogue" that failed) and
///   ONLINE_ERROR_OUT_OF_MEMORY; it validates untrusted bytes and never asserts on them.
/// Version: since Z2 Stage 4. The cooked format version is uONLINE_CATALOGUE_FORMAT_VERSION; a catalogue of another version is
///   re-cooked, never migrated at run time (Zenith2/Docs/Contracts.md → "Compatibility policy per kind").
/// @endcontract
class Zenith_OnlineCatalogue
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_OnlineCatalogue>, Zenith_OnlineError> Parse(Zenith_ByteSpan xCooked,
		Zenith_Heap& xHeap);
	static Zenith_Result<Zenith_Owned<Zenith_OnlineCatalogue>, Zenith_OnlineError> CreateEmpty(Zenith_Heap& xHeap);
	~Zenith_OnlineCatalogue();
	const Zenith_OnlineAchievementDef* FindAchievement(Zenith_StringView strID) const;   // O(log n); null: absent
	const Zenith_OnlineEntitlementDef* FindEntitlement(Zenith_StringView strID) const;   // O(log n); null: absent
	Zenith_Span<const Zenith_OnlineAchievementDef> GetAchievements() const;   // ascending ID byte order
	Zenith_Span<const Zenith_OnlineEntitlementDef> GetEntitlements() const;   // ascending ID byte order
	const Zenith_OnlineCloudPolicy& GetCloudPolicy() const;
	uint64 GetContentHash() const;                 // Zenith_Hash64 of the cooked bytes; 0 for CreateEmpty
private:
	struct PrivateTag {};
public:
	explicit Zenith_OnlineCatalogue(PrivateTag);
};
inline constexpr uint16 uONLINE_CATALOGUE_FORMAT_VERSION = 1;
bool Zenith_IsValidOnlineCatalogueID(Zenith_StringView strID);   // ^[a-z][a-z0-9_.]{0,63}$
```

## Zenith_OnlineServices header

The adapter API: the interface every online-services backend implements (Z2-D-074). It is a request/completion protocol on the main thread, polled once per frame by the session; it holds no simulation state.

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_OnlineServices.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithOnline/Zenith_OnlineErrors.h"
#include "ZenithOnline/Zenith_OnlineTypes.h"

class Zenith_Heap;
class Zenith_Platform;          // ZenithPlatform/Zenith_Platform.h
class Zenith_AsyncIO;           // ZenithPlatform/Zenith_AsyncIO.h
class Zenith_FaultInjector;     // ZenithCore
class Zenith_OnlineCatalogue;

/// @contract value Zenith_OnlineRequestID
/// Ownership: Plain value assigned by the session (monotonic from 1 per session); 0 is null.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Never serialized.
/// @endcontract
struct Zenith_OnlineRequestID
{
	uint64 m_ulValue = 0;
	friend constexpr bool operator==(Zenith_OnlineRequestID, Zenith_OnlineRequestID) = default;
};

/// @contract enum Zenith_OnlineRequestKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are never renumbered.
/// @endcontract
enum Zenith_OnlineRequestKind : uint8
{
	ONLINE_REQUEST_SIGN_IN_SILENT = 1,
	ONLINE_REQUEST_SIGN_IN_INTERACTIVE = 2,    // uONLINE_CAPABILITY_INTERACTIVE_SIGN_IN
	ONLINE_REQUEST_SIGN_OUT = 3,               // uONLINE_CAPABILITY_SIGN_OUT
	ONLINE_REQUEST_SWITCH_ACCOUNT = 4,         // uONLINE_CAPABILITY_SWITCH_ACCOUNT; m_strNative is the target native ID
	ONLINE_REQUEST_QUERY_ENTITLEMENTS = 5,     // m_axNativeIDs: Steam app IDs in decimal, Play product IDs, offline catalogue IDs
	ONLINE_REQUEST_QUERY_ACHIEVEMENTS = 6,     // m_axNativeIDs; m_axProgressStats beside them for Steam incremental entries
	ONLINE_REQUEST_UNLOCK_ACHIEVEMENT = 7,     // m_strNative
	ONLINE_REQUEST_SET_ACHIEVEMENT_STEPS = 8,  // m_strNative, m_strProgressStat, m_uValue = steps, m_uTotal
	ONLINE_REQUEST_CLOUD_LIST = 9,
	ONLINE_REQUEST_CLOUD_READ = 10,            // m_strCloudName into m_xReadBuffer
	ONLINE_REQUEST_CLOUD_WRITE = 11,           // m_strCloudName, m_xBytes, optional m_xExpectedRevision
	ONLINE_REQUEST_CLOUD_DELETE = 12,          // m_strCloudName, optional m_xExpectedRevision
};

/// @contract value Zenith_OnlineRequest
/// Ownership: Plain value built by the session; every view and span borrows session memory that stays valid until the
///   request's completion has been delivered (or Cancel has returned for a request the backend has not started).
/// Thread affinity: IMMUTABLE; members non-const: the session fills it field by field.
/// Invalidation: As above.
/// Units: Bytes; steps.
/// Errors: None of its own; Zenith_OnlineServices::Submit validates it.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineRequest
{
	Zenith_OnlineRequestID m_xID;
	Zenith_OnlineRequestKind m_eKind = ONLINE_REQUEST_SIGN_IN_SILENT;
	bool m_bHasExpectedRevision = false;
	uint32 m_uValue = 0;                                  // SET_ACHIEVEMENT_STEPS: the steps (monotonic, the backend never lowers them)
	uint32 m_uTotal = 0;                                  // SET_ACHIEVEMENT_STEPS: the catalogue's total
	Zenith_StringView m_strNative;                         // one native ID (UNLOCK, SET_ACHIEVEMENT_STEPS, SWITCH_ACCOUNT)
	Zenith_StringView m_strProgressStat;                   // SET_ACHIEVEMENT_STEPS on Steam
	Zenith_Span<const Zenith_StringView> m_axNativeIDs;    // QUERY_ENTITLEMENTS, QUERY_ACHIEVEMENTS; ≤ 1,024 entries
	Zenith_Span<const Zenith_StringView> m_axProgressStats;   // QUERY_ACHIEVEMENTS: same length as m_axNativeIDs, empty entries allowed
	Zenith_StringView m_strCloudName;                      // CLOUD_*: Formats.md "Cloud object names"
	Zenith_ByteSpan m_xBytes;                              // CLOUD_WRITE
	Zenith_MutableByteSpan m_xReadBuffer;                  // CLOUD_READ: at least the object's stored bytes
	Zenith_OnlineCloudRevision m_xExpectedRevision;        // CLOUD_WRITE, CLOUD_DELETE when m_bHasExpectedRevision
};

/// @contract value Zenith_OnlineCompletion
/// Ownership: Plain value passed to Zenith_OnlineEventSink::OnRequestCompleted; m_xPayload borrows backend memory.
/// Thread affinity: IMMUTABLE; members non-const: the backend fills it field by field.
/// Invalidation: m_xPayload is valid for the duration of the OnRequestCompleted call only.
/// Units: Bytes.
/// Errors: m_eStatus is the outcome; m_xError is meaningful when m_eStatus is ONLINE_STATUS_BACKEND_ERROR or
///   ONLINE_STATUS_UNAVAILABLE.
/// Version: since Z2 Stage 4. The payload layout per kind is Zenith2/Docs/Design/ONLINE/Formats.md → "Backend completion payloads".
/// @endcontract
struct Zenith_OnlineCompletion
{
	Zenith_OnlineRequestID m_xID;
	Zenith_OnlineRequestKind m_eKind = ONLINE_REQUEST_SIGN_IN_SILENT;
	Zenith_OnlineStatus m_eStatus = ONLINE_STATUS_OK;
	Zenith_OnlineError m_xError{ ONLINE_ERROR_BACKEND };
	Zenith_ByteSpan m_xPayload;
};

/// @contract enum Zenith_OnlineInvalidation
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_OnlineInvalidation : uint8
{
	ONLINE_INVALIDATION_ENTITLEMENTS = 1,   // ownership may have changed (Steam DlcInstalled_t, Play PurchasesUpdatedListener)
	ONLINE_INVALIDATION_ACHIEVEMENTS = 2,   // the service's achievement state may differ from the session's cache
	ONLINE_INVALIDATION_CLOUD = 3,          // the cloud may hold objects the session has not listed
	ONLINE_INVALIDATION_CONNECTIVITY = 4,   // m_bConnected of the identity changed
};

/// @contract interface Zenith_OnlineEventSink
/// Ownership: Implemented by Zenith_OnlineSession (Internal); borrowed by the backend for one Pump call.
/// Thread affinity: MAIN_THREAD. Called only from inside Zenith_OnlineServices::Pump.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the sink validates every payload it receives (untrusted backend data).
/// Reentrancy: NONE. A sink callback never calls Pump, Submit or Shutdown of the backend that invoked it; it may call Cancel
///   and GetIdentity.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_OnlineEventSink
{
public:
	virtual ~Zenith_OnlineEventSink() = default;
	virtual void OnIdentityChanged(const Zenith_OnlineIdentitySnapshot& xIdentity) = 0;
	virtual void OnRequestCompleted(const Zenith_OnlineCompletion& xCompletion) = 0;
	virtual void OnInvalidated(Zenith_OnlineInvalidation eWhat) = 0;
};

/// @contract interface Zenith_OnlineServices
/// Ownership: Created by a backend factory (Zenith_CreateOnlineServices_<Backend>) as a Zenith_Owned<Zenith_OnlineServices>;
///   owned by the Online module instance; destroyed after Shutdown returns, in shutdown step 6 with the module.
/// Thread affinity: MAIN_THREAD for every member. A backend whose SDK calls back on other threads (the Play Java bridge)
///   queues those calls internally and delivers them only from Pump.
/// Invalidation: A request ID is invalid once its completion has been delivered.
/// Units: Bytes; milliseconds for the Shutdown deadline.
/// Errors: Submit returns ONLINE_ERROR_NOT_SUPPORTED (capability absent), ONLINE_ERROR_QUEUE_FULL (m_uMaxInFlight in flight),
///   ONLINE_ERROR_INVALID_ID (a native ID or cloud name violating Formats.md), ONLINE_ERROR_SHUTTING_DOWN; once Submit
///   succeeds, the request completes exactly once through OnRequestCompleted, including after Cancel (status CANCELLED,
///   unless it had already finished) and at Shutdown (status SHUTTING_DOWN). A refused Submit delivers no completion.
/// Reentrancy: NONE. Pump invokes the sink; the sink's rules are its contract block's.
/// Version: since Z2 Stage 4. Source API; a new request kind is appended and existing kinds keep their semantics.
/// Example: Zenith2/Engine/ZenithOnline/Tests/Conformance/Examples/Zenith_OnlineServices_Example.cpp
/// @endcontract
class Zenith_OnlineServices
{
public:
	virtual ~Zenith_OnlineServices() = default;
	virtual Zenith_OnlineBackendKind GetKind() const = 0;
	virtual uint32 GetCapabilities() const = 0;              // uONLINE_CAPABILITY_* bits
	virtual uint64 GetMaxCloudObjectBytes() const = 0;       // 0 without uONLINE_CAPABILITY_CLOUD
	virtual Zenith_OnlineIdentitySnapshot GetIdentity() const = 0;
	// Delivers queued backend events and completions, in the order the backend produced them, to xSink.
	virtual void Pump(Zenith_OnlineEventSink& xSink) = 0;
	virtual Zenith_Result<void, Zenith_OnlineError> Submit(const Zenith_OnlineRequest& xRequest) = 0;
	// Idempotent, never blocks; an unknown or completed ID is ignored.
	virtual void Cancel(Zenith_OnlineRequestID xID) = 0;
	// Completes every outstanding request with SHUTTING_DOWN through xSink, flushes the SDK (Steam StoreStats, the Play
	// bridge's pending commits) for at most uDeadlineMs, then stops the SDK; subsequent calls of every member except the
	// destructor are refused with ONLINE_ERROR_SHUTTING_DOWN.
	virtual void Shutdown(Zenith_OnlineEventSink& xSink, uint32 uDeadlineMs) = 0;
};

/// @contract value Zenith_OnlineOfflineSettings
/// Ownership: Plain value; the spans and views borrow configuration-store memory for the factory call only.
/// Thread affinity: IMMUTABLE; members non-const: filled field by field from the configuration keys.
/// Invalidation: After the factory returns.
/// Units: Frames for m_uLatencyFrames.
/// Errors: The offline factory returns ONLINE_ERROR_INVALID_DESC for an account name outside ^[a-z0-9_-]{1,32}$, duplicate
///   names, more than 16 accounts, an active account not in the list, or a cloud root that is not empty and not one component of [a-z0-9_-], 1 … 32 bytes.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineOfflineSettings
{
	Zenith_Span<const Zenith_StringView> m_axAccounts;           // online.offline.accounts, default ["local"]
	Zenith_StringView m_strActiveAccount;                        // online.offline.activeAccount, default the first account
	bool m_bAutoSignIn = true;                                   // online.offline.autoSignIn
	Zenith_Span<const Zenith_StringView> m_axGrantedEntitlements;   // online.offline.grantedEntitlements
	Zenith_StringView m_strCloudRoot;                            // online.offline.cloudRoot (one component under saves/online/cloud/); empty: no cloud capability
	uint32 m_uLatencyFrames = 0;                                // online.offline.latencyFrames, 0 … 600
};

/// @contract value Zenith_OnlineSteamSettings
/// Ownership: Plain value; the view borrows configuration-store memory for the factory call only.
/// Thread affinity: IMMUTABLE; members non-const: filled field by field from the configuration keys.
/// Invalidation: After the factory returns.
/// Units: None.
/// Errors: The Steam factory returns ONLINE_ERROR_INVALID_DESC for a scripted-API path in a Shipping build.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_OnlineSteamSettings
{
	uint32 m_uAppID = 0;                       // online.steam.appId; 0: the SDK reads steam_appid.txt (development only)
	bool m_bRestartIfNecessary = false;         // online.steam.restartIfNecessary
	Zenith_StringView m_strScriptedApiPath;     // online.steam.scriptedApi; non-empty selects the scripted seam (not in Shipping)
};

/// @contract value Zenith_OnlinePlaySettings
/// Ownership: Plain value; the view borrows configuration-store memory for the factory call only.
/// Thread affinity: IMMUTABLE; members non-const: filled field by field from the configuration keys.
/// Invalidation: After the factory returns.
/// Units: None.
/// Errors: The Google Play factory returns ONLINE_ERROR_INVALID_DESC for a scripted-bridge path in a Shipping build.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_OnlinePlaySettings
{
	bool m_bInteractiveSignInOnStart = false;   // online.googlePlay.interactiveSignInOnStart
	Zenith_StringView m_strScriptedBridgePath;  // online.googlePlay.scriptedBridge; non-empty selects the scripted seam (not in Shipping)
};

/// @contract value Zenith_OnlineServicesDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the created backend.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: Requests for m_uMaxInFlight.
/// Errors: Each factory returns ONLINE_ERROR_INVALID_DESC for a null m_pxHeap, m_pxPlatform or m_pxCatalogue, a null m_pxIO
///   for the offline backend, or m_uMaxInFlight outside 1 … 4,096.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineServicesDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_Platform* m_pxPlatform = nullptr;               // the Android activity and JNI environment; the user mount
	Zenith_AsyncIO* m_pxIO = nullptr;                      // the offline backend's user-mount files
	const Zenith_OnlineCatalogue* m_pxCatalogue = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;            // tests only; null in a Player
	uint32 m_uMaxInFlight = 256;
	bool m_bShipping = false;                              // Zenith_GetBuildIdentity().m_bShipping
	Zenith_OnlineOfflineSettings m_xOffline;
	Zenith_OnlineSteamSettings m_xSteam;
	Zenith_OnlinePlaySettings m_xPlay;
};

/// @contract functions Zenith_OnlineBackendRegistration
/// Ownership: The factory returns a Zenith_Owned<Zenith_OnlineServices> allocated on xDesc.m_pxHeap.
/// Thread affinity: MAIN_THREAD (startup step 15, inside Zenith_Module_Online_Start).
/// Invalidation: None.
/// Units: None.
/// Errors: A factory returns ONLINE_ERROR_INVALID_DESC, ONLINE_ERROR_OUT_OF_MEMORY or ONLINE_ERROR_BACKEND_UNAVAILABLE and
///   unwinds what it built ([Z2-N-CONTRACT-340]).
/// Version: since Z2 Stage 4. Zenith_OnlineBackendEntry is the element type of the backend list the generated composition passes
///   to Zenith_Module_Online_Start (Zenith2/Docs/Architecture.md → "Engine modules and game projects").
/// @endcontract
using Zenith_PfnCreateOnlineServices = Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> (*)(
	const Zenith_OnlineServicesDesc& xDesc);

struct Zenith_OnlineBackendEntry
{
	const char* m_szName;                       // the backend's BACKEND_NAME: "Offline", "Steam", "GooglePlay"
	Zenith_PfnCreateOnlineServices m_pfnCreate;
};
```

## Zenith_OnlineSession header

The engine side of the adapter: the identity state machine and the rebind sequence (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind sequence"), the caches, the script-request processor and the cloud-save sync engine (`Zenith2/Docs/Design/ONLINE/CloudSave.md`). One session exists per process, owned by the Online module instance.

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_OnlineSession.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_ReentrancyGuard.h"
#include "ZenithSave/Zenith_SaveTypes.h"              // Zenith_SaveSlotName, Zenith_SaveKind, Zenith_SaveProfileKey
#include "ZenithOnline/Zenith_OnlineCatalogue.h"
#include "ZenithOnline/Zenith_OnlineServices.h"

class Zenith_Arena;
class Zenith_SaveSystem;              // ZenithSave/Zenith_SaveSystem.h
class Zenith_MemoryGovernor;          // ZenithCore
class Zenith_MemoryConsumer;          // ZenithCore
class Zenith_EntitlementQuery;        // ZenithPackage (Stage 6)
struct Zenith_PlatformAccountPayload; // ZenithPlatform/Zenith_Platform.h
enum Zenith_WorldPurpose : uint8;    // ZenithRuntime/Zenith_Module.h

/// @contract enum Zenith_OnlineSessionState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The states of Zenith2/Docs/Design/ONLINE/Accounts.md → "Session state machine".
/// @endcontract
enum Zenith_OnlineSessionState : uint8
{
	ONLINE_SESSION_STATE_REPLAY = 1,        // --replay: inert, no backend
	ONLINE_SESSION_STATE_STARTING = 2,      // created; the first Pump has not run
	ONLINE_SESSION_STATE_SIGNED_OUT = 3,    // signed out; the save profile is local, or the last profile after a failed silent sign-in
	ONLINE_SESSION_STATE_SIGNING_IN = 4,
	ONLINE_SESSION_STATE_SIGNED_IN = 5,     // bound to the account's profile
	ONLINE_SESSION_STATE_REBINDING = 6,     // fenced (odd generation); waiting for the account consumer or the save profile switch
	ONLINE_SESSION_STATE_SHUTTING_DOWN = 7,
	ONLINE_SESSION_STATE_SHUT_DOWN = 8,
	ONLINE_SESSION_STATE_VERIFYING = 9,     // signed in, but the backend reported an unverified identity (Play on resume): nothing is
	                                        // submitted, drained or applied until it confirms the player (Accounts.md "Identity verification")
};

/// @contract enum Zenith_OnlineCloudSlotState
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte in the sync manifest and in results; values are never renumbered.
///   The states of Zenith2/Docs/Design/ONLINE/CloudSave.md → "Slot sync state machine".
/// @endcontract
enum Zenith_OnlineCloudSlotState : uint8
{
	ONLINE_CLOUD_SLOT_IN_SYNC = 1,
	ONLINE_CLOUD_SLOT_LOCAL_AHEAD = 2,      // an upload is due
	ONLINE_CLOUD_SLOT_CLOUD_AHEAD = 3,      // a download is due
	ONLINE_CLOUD_SLOT_UPLOADING = 4,
	ONLINE_CLOUD_SLOT_DOWNLOADING = 5,
	ONLINE_CLOUD_SLOT_DELETING_CLOUD = 6,
	ONLINE_CLOUD_SLOT_DELETING_LOCAL = 7,
	ONLINE_CLOUD_SLOT_CONFLICT = 8,
	ONLINE_CLOUD_SLOT_RESOLVING = 9,
	ONLINE_CLOUD_SLOT_SKIPPED_VERSION = 10, // the local save is NEWER_VERSION or INCOMPATIBLE: never synced
	ONLINE_CLOUD_SLOT_CLOUD_NEWER = 11,     // the cloud copy needs a newer save format: never downloaded
	ONLINE_CLOUD_SLOT_TOO_LARGE = 12,       // the envelope exceeds the backend's cloud object limit
	ONLINE_CLOUD_SLOT_ERROR = 13,           // the last transfer failed; retried after the backoff
	ONLINE_CLOUD_SLOT_NOT_SYNCED = 14,      // the slot's kind is excluded by the cloud policy, or the slot limit is reached
	ONLINE_CLOUD_SLOT_CLOUD_UNKNOWN = 15,   // the cloud index could not be read or trusted (CloudSave.md "Index view"): no action; never persisted
};

/// @contract enum Zenith_OnlineConflictReason
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte; the rows of Zenith2/Docs/Design/ONLINE/CloudSave.md → "Three-way decision table".
/// @endcontract
enum Zenith_OnlineConflictReason : uint8
{
	ONLINE_CONFLICT_REASON_NONE = 0,
	ONLINE_CONFLICT_REASON_FIRST_SYNC = 1,                 // both exist, differ, and were never synced
	ONLINE_CONFLICT_REASON_BOTH_CHANGED = 2,
	ONLINE_CONFLICT_REASON_DELETED_LOCALLY_CHANGED_IN_CLOUD = 3,
	ONLINE_CONFLICT_REASON_CHANGED_LOCALLY_DELETED_IN_CLOUD = 4,
};

/// @contract enum Zenith_OnlineResolveChoice
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as one byte in script request payloads.
/// @endcontract
enum Zenith_OnlineResolveChoice : uint8
{
	ONLINE_RESOLVE_KEEP_LOCAL = 1,
	ONLINE_RESOLVE_KEEP_CLOUD = 2,
	ONLINE_RESOLVE_KEEP_BOTH = 3,          // MANUAL slots only: the cloud copy becomes a new manual slot
	ONLINE_RESOLVE_DEFER = 4,              // keep the conflict; the slot stays out of sync
};

/// @contract enum Zenith_OnlineNotifyRefusal
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None. Why the port could not allocate a notification ID; the production port maps the sink's
///   Zenith_PublicationSinkErrorCode one to one (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Allocation sites and acceptance").
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_OnlineNotifyRefusal : uint8
{
	ONLINE_NOTIFY_REFUSAL_NONE = 0,        // an ID was allocated
	ONLINE_NOTIFY_REFUSAL_FULL = 1,        // capacity: retry at the next Pump
	ONLINE_NOTIFY_REFUSAL_REPLAYING = 2,   // never reached: the session allocates nothing in REPLAY
	ONLINE_NOTIFY_REFUSAL_SITE = 3,        // the sink does not admit the allocation site: permanent (Accounts.md "Notifications" step 3)
	ONLINE_NOTIFY_REFUSAL_DETACHED = 4,    // the world's table is retired: permanent for that world
};

/// @contract interface Zenith_OnlinePublicationPort
/// Ownership: Implemented by the Online module instance over the world's module publication sink (Internal
///   Zenith_OnlineSinkPort), or by a test (ZenithOnline/Tests/Support/Zenith_OnlineRecordingPort.h); one per attached world;
///   borrowed by the session from AttachWorld until DetachWorld returns.
/// Thread affinity: ANY_THREAD_EXCEPT(AllocateNotificationID). Park runs on the main thread (the prologue) or on the
///   committing world's merge node (an inbox-full refusal); AllocateNotificationID only on the main thread in the frame
///   prologue (a recorded kind, orchestrator ruling Q89 (a)).
/// Invalidation: None.
/// Units: Completion IDs.
/// Errors: AllocateNotificationID returns 0 and sets eRefusalOut when the sink refuses (FULL, REPLAYING, SITE or DETACHED; the
///   session treats FULL as a retry and SITE and DETACHED as permanent); Park never fails visibly, as the
///   sink's Park (Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Parking a result"): a park into a retired table is discarded and
///   counted by the table, and the session never parks under --replay.
/// Reentrancy: NOT_APPLICABLE. The port invokes no callback.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_OnlinePublicationPort
{
public:
	virtual ~Zenith_OnlinePublicationPort() = default;
	virtual bool IsReplaying() const = 0;
	// AllocateCompletionID(ASYNC_SOURCE_ONLINE); 0: refused, with the reason in eRefusalOut (NONE on success).
	virtual uint64 AllocateNotificationID(Zenith_OnlineNotifyRefusal& eRefusalOut) = 0;
	// Completes ulCompletionID with ASYNC_SOURCE_ONLINE and PUBLICATION_KIND_GENERIC_RECORDED.
	virtual void Park(uint64 ulCompletionID, Zenith_ByteSpan xPayload) = 0;
};

/// @contract value Zenith_OnlineSessionSettings
/// Ownership: Plain value read from the online.* configuration keys at Start.
/// Thread affinity: IMMUTABLE; members non-const: Start reads it field by field from the configuration keys.
/// Invalidation: None.
/// Units: Milliseconds, seconds and bytes as named.
/// Errors: Create returns ONLINE_ERROR_INVALID_DESC for a value outside the range of its key (PublicAPI.md "Configuration keys").
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineSessionSettings
{
	uint32 m_uRequestTimeoutMs = 15000;
	bool m_bExposeDisplayName = false;
	bool m_bCloudEnabled = true;
	uint32 m_uUploadDelayMs = 2000;
	uint32 m_uPollSeconds = 0;              // 0: no periodic cloud listing
	uint64 m_ulMaxTransferBytes = 67108864; // the largest envelope the session moves; larger slots are TOO_LARGE
	uint32 m_uShutdownFlushMs = 3000;
	uint32 m_uRebindMaxSaveAttempts = 600;  // non-busy SetActiveProfile failures tolerated before cloud is disabled
	uint32 m_uMaxRequestsPerWorld = 64;     // the Z2-B-SCRIPT-026 online value; sizes each world's inbox
	uint32 m_uRetryBackoffMs = 30000;       // the first ERROR backoff; doubles to at most 480,000
};

/// @contract value Zenith_OnlineSessionDesc
/// Ownership: Plain value; every pointer is borrowed and outlives the session; m_pxServices is null exactly in replay mode.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns ONLINE_ERROR_INVALID_DESC for a null m_pxHeap, m_pxCatalogue, m_pxSave, m_pxPlatform, m_pxIO or
///   m_pxGovernor, or a null m_pxServices outside replay mode.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineSessionDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_OnlineServices* m_pxServices = nullptr;
	const Zenith_OnlineCatalogue* m_pxCatalogue = nullptr;
	Zenith_SaveSystem* m_pxSave = nullptr;
	Zenith_Platform* m_pxPlatform = nullptr;
	Zenith_AsyncIO* m_pxIO = nullptr;
	Zenith_MemoryGovernor* m_pxGovernor = nullptr;
	Zenith_FaultInjector* m_pxFaults = nullptr;     // tests only
	bool m_bReplay = false;
	Zenith_OnlineSessionSettings m_xSettings;
};

/// @contract value Zenith_OnlineIdentityView
/// Ownership: Plain value returned by GetIdentityView.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Presentation data (editor panel, overlay, console): never read by a simulation system. It
///   carries no account identifier, account key or profile key (Accounts.md "Account identifiers and keys").
/// @endcontract
struct Zenith_OnlineIdentityView
{
	Zenith_OnlineSessionState m_eState = ONLINE_SESSION_STATE_STARTING;
	Zenith_OnlineBackendKind m_eBackend = ONLINE_BACKEND_KIND_NONE;
	bool m_bSignedIn = false;
	bool m_bConnected = false;
	bool m_bCloudAvailable = false;
	uint32 m_uGeneration = 0;                  // the bind generation (low 32 bits; odd while fenced)
	uint32 m_uAccountOrdinal = 0;              // the session-local account ordinal (#n in logs); 0 when signed out
	bool m_bLocalProfile = true;                // the bound save profile is the local profile
	Zenith_OnlineDisplayName m_xDisplayName;    // empty unless signed in
};

/// @contract value Zenith_OnlineConflictToken
/// Ownership: Plain value; copied from a conflict record into a notification, a slot status or a script result.
/// Thread affinity: IMMUTABLE; members non-const: containers and records copy-assign it.
/// Invalidation: None; a token goes stale when the conflict record is refreshed (CloudSave.md "Conflict detection").
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
// Formats.md "Notification payloads": the inputs a conflict record was built from (41 bytes on the wire).
struct Zenith_OnlineConflictToken
{
	uint8 m_uKinds = 0;                       // bits 0 … 1 local (1 absent, 2 present); bits 2 … 3 cloud (1 absent, 2 entry, 3 record)
	uint8 m_auLocalHashPrefix[16] = {};
	uint64 m_ulCloudCommitSerial = 0;
	uint8 m_auCloudHashPrefix[16] = {};
	friend constexpr bool operator==(const Zenith_OnlineConflictToken&, const Zenith_OnlineConflictToken&) = default;
};

/// @contract value Zenith_OnlineCloudSlotStatus
/// Ownership: Plain value; returned in a caller arena by GetCloudSlots.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: With the arena.
/// Units: Seconds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineCloudSlotStatus
{
	Zenith_SaveSlotName m_xSlot;
	Zenith_SaveKind m_eKind = SAVE_KIND_MANUAL;
	Zenith_OnlineCloudSlotState m_eState = ONLINE_CLOUD_SLOT_IN_SYNC;
	Zenith_OnlineConflictReason m_eConflict = ONLINE_CONFLICT_REASON_NONE;
	uint64 m_ulLocalUtcSeconds = 0, m_ulCloudUtcSeconds = 0;
	uint64 m_ulLocalPlaytimeSeconds = 0, m_ulCloudPlaytimeSeconds = 0;
	Zenith_OnlineConflictToken m_xConflictToken;   // CONFLICT only: pass it back to ResolveCloudConflict
};

/// @contract value Zenith_OnlineSessionStats
/// Ownership: Plain value returned by GetStats.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: Counts since Create; bytes; the gauges marked current.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_OnlineSessionStats
{
	uint64 m_ulRebinds = 0, m_ulStaleSignals = 0, m_ulDuplicateSignals = 0, m_ulProfileSwitchRetries = 0;
	uint64 m_ulScriptRequests = 0, m_ulScriptRequestsDroppedInReplay = 0, m_ulRequestsAccountChanged = 0, m_ulTimeouts = 0;
	uint64 m_ulNotificationsParked = 0, m_ulNotificationsDropped = 0;
	uint64 m_ulUploads = 0, m_ulDownloads = 0, m_ulConflictsDetected = 0, m_ulConflictsResolved = 0, m_ulTransferFailures = 0;
	uint64 m_ulBytesUploaded = 0, m_ulBytesDownloaded = 0;
	uint32 m_uPendingBackendRequests = 0;      // current
	uint64 m_ulResidentBytes = 0;              // current: the memory consumer's report
};

/// @contract type Zenith_OnlineSession
/// Ownership: Created by Create as a Zenith_Owned on xDesc.m_pxHeap; owned by the Online module instance; owns the
///   per-world records (inboxes, notification queues, published buffers), the pending-request table, the caches, the cloud
///   sync engine, its manifest writers and its memory consumer; borrows the backend, the catalogue and every desc pointer.
/// Thread affinity: MAIN_THREAD, except SubmitScriptRequest (ANY_THREAD under the per-world rule below),
///   OnPublished (the world's tick-boundary node, step 1), TakePublishedNotifications (the world's phase-1 node, ordered after
///   that boundary by the tick) and GetStats (ANY_THREAD).
///   SubmitScriptRequest is called by the script consumer on the committing world's merge node; one world's calls never
///   overlap, two worlds' calls may run concurrently, so each world has its own single-producer inbox.
/// Invalidation: AttachWorld's record is invalid after DetachWorld for that tag.
/// Units: Milliseconds of the monotonic clock for Pump; bytes.
/// Errors: Create returns ONLINE_ERROR_INVALID_DESC, ONLINE_ERROR_OUT_OF_MEMORY and the I/O errors of loading the device
///   record; AttachWorld returns ONLINE_ERROR_OUT_OF_MEMORY and ONLINE_ERROR_QUEUE_FULL (more than uONLINE_MAX_ATTACHED_WORLDS worlds attached); the presentation actions return ONLINE_ERROR_NOT_SUPPORTED,
///   ONLINE_ERROR_REPLAY, ONLINE_ERROR_SHUTTING_DOWN and ONLINE_ERROR_INVALID_ID. Every failure after acceptance reaches
///   scripts as a result status, never as an assert.
/// Reentrancy: NONE. It invokes the backend (whose sink callbacks re-enter only through Zenith_OnlineEventSink) and the
///   publication ports; SAVE calls its Internal Zenith_OnlineSaveListener (registered by Create, cleared first by
///   BeginShutdown) from PumpMainThread, outside every session call; none of those calls back into the session's public
///   members.
/// Version: since Z2 Stage 4. Source API.
/// Example: Zenith2/Engine/ZenithOnline/Tests/Examples/Zenith_OnlineSession_Example.cpp
/// @endcontract
class Zenith_OnlineSession
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_OnlineSession>, Zenith_OnlineError> Create(
		const Zenith_OnlineSessionDesc& xDesc);
	~Zenith_OnlineSession();   // ZENITH_CHECK that BeginShutdown ran and IsDrained is true

	// --- frame (MAIN_THREAD, frame prologue step 1) ---
	// The account consumer (registered through Zenith_ModuleStartContext::RegisterAccountConsumer): the rebind sequence.
	void OnAccountEvent(const Zenith_PlatformAccountPayload& xAccount);
	// The frame listener's OnPlatformEvents: backend pump, request processing, profile-switch retry, cloud engine, parking.
	void Pump(uint64 ulMonotonicMs);
	// The lifecycle half of OnPlatformEvents: entering the background starts the background flush.
	void OnEnterBackground(uint64 ulMonotonicMs);

	// --- worlds (MAIN_THREAD, inside the module instance's AttachWorld / DetachWorld) ---
	Zenith_Result<void, Zenith_OnlineError> AttachWorld(uint16 uWorldTag, Zenith_WorldPurpose ePurpose,
		Zenith_OnlinePublicationPort& xPort);
	void DetachWorld(uint16 uWorldTag);          // completes every outstanding request of the world ABANDONED
	// The world's tick-boundary handler body (called through the sink's module-side handler, step 1).
	void OnPublished(uint16 uWorldTag, uint64 ulCompletionID, uint8 uPublicationKind, Zenith_ByteSpan xPayload);
	// The phase-1 system Online.Events@W<tag>: takes the world's decoded notifications in completion-ID order, then empties them.
	Zenith_Span<const Zenith_ByteSpan> TakePublishedNotifications(uint16 uWorldTag);

	// --- script requests (the committing world's merge node) ---
	void SubmitScriptRequest(uint16 uWorldTag, uint64 ulCompletionID, Zenith_ByteSpan xPayload);

	// --- presentation (MAIN_THREAD; editor panel, overlay, console, UI glue) ---
	Zenith_OnlineIdentityView GetIdentityView() const;
	Zenith_OnlineEntitlementState GetEntitlementState(Zenith_StringView strEntitlementID) const;
	uint64 GetEntitlementRevision() const;
	Zenith_Result<Zenith_Span<const Zenith_OnlineCloudSlotStatus>, Zenith_OnlineError> GetCloudSlots(
		Zenith_Arena& xArena) const;
	Zenith_Result<void, Zenith_OnlineError> RequestSignIn(bool bInteractive);
	Zenith_Result<void, Zenith_OnlineError> RequestSignOut();
	Zenith_Result<void, Zenith_OnlineError> RequestSwitchAccount(Zenith_StringView strNativeID);
	Zenith_Result<void, Zenith_OnlineError> RequestCloudSync();
	// CloudSave.md "Index repair": only while the index view is UNKNOWN with cause INVALID, FOREIGN_PROFILE or MISSING;
	// ONLINE_ERROR_NOT_SUPPORTED otherwise.
	Zenith_Result<void, Zenith_OnlineError> RequestCloudIndexRepair();
	// CloudSave.md "Resolution choices": xToken is the conflict token the player was shown (the slot status's
	// m_xConflictToken); a token that differs from the record's is ONLINE_ERROR_CONFLICT_STALE (no action, the conflict is
	// notified again). A resolution accepted here is compared again with fresh inputs before it acts; a mismatch then
	// returns the slot to CONFLICT (stale) without writing or deleting anything. Other errors: ONLINE_ERROR_NOT_SUPPORTED
	// (the slot is not in CONFLICT, KEEP_BOTH refused, or the index view is UNKNOWN). DEFER ignores the token.
	Zenith_Result<void, Zenith_OnlineError> ResolveCloudConflict(const Zenith_SaveSlotName& xSlot,
		Zenith_OnlineResolveChoice eChoice, const Zenith_OnlineConflictToken& xToken);
	Zenith_OnlineSessionStats GetStats() const;

	// --- services for other subsystems (MAIN_THREAD) ---
	Zenith_MemoryConsumer& GetMemoryConsumer();            // MEMORY_SUBSYSTEM_OTHER
	Zenith_EntitlementQuery& GetPackageEntitlements();     // Stage 6: PACK's lower-declared interface

	// --- shutdown (MAIN_THREAD) ---
	// --- start (MAIN_THREAD, Start's Session step only, before any world attaches) ---
	// Accounts.md "Bind at start": binds synchronously when the backend reports SIGNED_IN; false leaves STARTING unchanged.
	bool BindAtStart();

	void BeginShutdown(uint64 ulMonotonicMs);   // refuse new work; flush uploads for at most m_uShutdownFlushMs
	void PumpShutdown(uint64 ulMonotonicMs);    // drives the flush; called by the module destructor's drain loop
	bool IsDrained() const;
private:
	struct PrivateTag {};
	Zenith_ReentrancyGuard m_xReentrancy;
public:
	Zenith_OnlineSession(PrivateTag, const Zenith_OnlineSessionDesc& xDesc);
};
```

`TakePublishedNotifications` returns views into the world record's published buffer, valid until the next tick boundary of that world; it is called only by the world's phase-1 node, which the tick orders after its boundary ([Z2-N-ARCH-383]), so the buffer needs no lock (the reasoning of `Zenith2/Docs/Design/UI/Interaction.md` → "UI events to simulation" step 4, which this design follows).

## Zenith_Module_Online header

```cpp
// Zenith2/Engine/ZenithOnline/Zenith_Module_Online.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithRuntime/Zenith_Module.h"
#include "ZenithOnline/Zenith_OnlineServices.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_Online
/// Ownership: Start returns the module instance (Internal Zenith_OnlineModuleInstance) owning the session, the backend, the
///   catalogue, the script consumer, the console tokens and the panel provider; ZenithRuntime owns the instance.
/// Thread affinity: MAIN_THREAD: RegisterTypes in startup step 6, Start in startup step 15.
/// Invalidation: None.
/// Units: None.
/// Errors: RegisterTypes returns MODULE_ERROR_REGISTRATION when a type or configuration key is rejected; Start returns
///   MODULE_ERROR_REGISTRATION, MODULE_ERROR_BACKEND_CREATE (no backend could be created, the offline fallback included) and
///   MODULE_ERROR_OUT_OF_MEMORY, each after unwinding every step it completed (Zenith2/Docs/Design/ONLINE.md → "Module start").
/// Version: since Z2 Stage 4. The entry points of Zenith2/Docs/Architecture.md → "Module entry points"; uZENITH_MODULE_INTERFACE_VERSION 1.
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Online_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Online_Start(
	Zenith_ModuleStartContext& xContext, Zenith_Span<const Zenith_OnlineBackendEntry> axBackends);
```

## Zenith_Online_Offline header

```cpp
// Zenith2/Engine/ZenithOnline_Offline/Zenith_Online_Offline.h
#pragma once
#include "ZenithOnline/Zenith_OnlineServices.h"

/// @contract functions Zenith_Online_Offline
/// Ownership: Returns the offline backend (Internal Zenith_OnlineServicesOffline) allocated on xDesc.m_pxHeap.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: ONLINE_ERROR_INVALID_DESC (the desc rules and Zenith_OnlineOfflineSettings' rules), ONLINE_ERROR_OUT_OF_MEMORY,
///   ONLINE_ERROR_IO (the cloud root directory could not be created); never ONLINE_ERROR_BACKEND_UNAVAILABLE.
/// Version: since Z2 Stage 4. The offline implementation of Z2-D-074: it never opens a socket or any network API.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> Zenith_CreateOnlineServices_Offline(
	const Zenith_OnlineServicesDesc& xDesc);
```

## Zenith_OnlineOfflineTestAccess header

The test seam of the offline backend: tests, integration suites and benchmarks drive account changes, other-device cloud writes and faults through it. It reaches the backend's `Internal/` state, which is why it is a `TestAccess` header ([Z2-N-CONTRACT-414] condition 6).

```cpp
// Zenith2/Engine/ZenithOnline_Offline/Zenith_OnlineOfflineTestAccess.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithOnline/Zenith_OnlineServices.h"

/// @contract type Zenith_OnlineOfflineTestAccess
/// Ownership: Static functions over an offline backend created by Zenith_CreateOnlineServices_Offline; holds nothing.
/// Thread affinity: MAIN_THREAD, between Pump calls of the backend.
/// Invalidation: None.
/// Units: Frames for latency; counts for faults.
/// Errors: Every function returns ONLINE_ERROR_INVALID_DESC when xServices is not an offline backend (GetKind check) and
///   ONLINE_ERROR_UNKNOWN_ID for an account name not configured.
/// Version: since Z2 Stage 4. Tests only (TestSeparationCheck rule 6).
/// @endcontract
class Zenith_OnlineOfflineTestAccess
{
public:
	// The next Pump reports the identity change through OnIdentityChanged, as the service would.
	static Zenith_Result<void, Zenith_OnlineError> SignIn(Zenith_OnlineServices& xServices, Zenith_StringView strAccount);
	static Zenith_Result<void, Zenith_OnlineError> SignOut(Zenith_OnlineServices& xServices);
	static Zenith_Result<void, Zenith_OnlineError> SetConnected(Zenith_OnlineServices& xServices, bool bConnected);
	static Zenith_Result<void, Zenith_OnlineError> SetEntitlement(Zenith_OnlineServices& xServices,
		Zenith_StringView strAccount, Zenith_StringView strEntitlementID, bool bGranted);
	// Writes (or deletes, for an empty span) one cloud object of strAccount's cloud as another device would: object and index
	// together, with a new revision and xDeviceID in the index entry.
	static Zenith_Result<void, Zenith_OnlineError> WriteCloudAsOtherDevice(Zenith_OnlineServices& xServices,
		Zenith_StringView strAccount, Zenith_StringView strCloudName, Zenith_ByteSpan xObject, uint64 ulDeviceID);
	// Fails the next uCount requests of eKind with eStatus (a per-kind countdown; uCount 0 clears it).
	static Zenith_Result<void, Zenith_OnlineError> FailNext(Zenith_OnlineServices& xServices,
		Zenith_OnlineRequestKind eKind, Zenith_OnlineStatus eStatus, uint32 uCount);
	static Zenith_Result<void, Zenith_OnlineError> SetLatencyFrames(Zenith_OnlineServices& xServices, uint32 uFrames);
	// Holds every completion of eKind until ReleaseHeld (to place an account change between submission and completion).
	static Zenith_Result<void, Zenith_OnlineError> HoldCompletions(Zenith_OnlineServices& xServices,
		Zenith_OnlineRequestKind eKind, bool bHold);
	// Writes xBytes as the raw stored object strName of strAccount's cloud (any name, any bytes: a corrupt, newer-version or
	// foreign index, a garbage envelope), with a new revision and the given modification time; reports ONLINE_INVALIDATION_CLOUD.
	static Zenith_Result<void, Zenith_OnlineError> WriteRawCloudObject(Zenith_OnlineServices& xServices,
		Zenith_StringView strAccount, Zenith_StringView strName, Zenith_ByteSpan xBytes, uint64 ulModifiedUtcSeconds);
	// While true the backend reports its signed-in account as ONLINE_IDENTITY_STATE_SIGNING_IN (the offline form of Play's resume).
	static Zenith_Result<void, Zenith_OnlineError> SetUnverifiedIdentity(Zenith_OnlineServices& xServices,
		bool bUnverified);
	// Delivers, at the next Pump, every completion of eKind held by HoldCompletions; equivalent to HoldCompletions(…, false).
	static Zenith_Result<void, Zenith_OnlineError> ReleaseHeld(Zenith_OnlineServices& xServices,
		Zenith_OnlineRequestKind eKind);
	static uint32 GetOutstandingCount(const Zenith_OnlineServices& xServices);
};
```

## Zenith_Online_Steam header

```cpp
// Zenith2/Engine/ZenithOnline_Steam/Zenith_Online_Steam.h
#pragma once
#include "ZenithOnline/Zenith_OnlineServices.h"

/// @contract functions Zenith_Online_Steam
/// Ownership: Returns the Steam backend (Internal Zenith_OnlineServicesSteam) allocated on xDesc.m_pxHeap; it owns the
///   Steam API seam implementation selected by xDesc.m_xSteam.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: ONLINE_ERROR_INVALID_DESC, ONLINE_ERROR_OUT_OF_MEMORY, ONLINE_ERROR_BACKEND_UNAVAILABLE (SteamAPI_InitFlat did not
///   return k_ESteamAPIInitResult_OK; m_iNativeCode carries the ESteamAPIInitResult; the module then falls back to Offline).
/// Version: since Z2 Stage 6. Windows only (SUPPORTED), and the target exists only when the steamworks-sdk archive is
///   provisioned, so no gate builds it (Zenith2/Docs/Design/ONLINE/Backends.md → "Store backend libraries and what the gates
///   prove"). With online.steam.scriptedApi set it creates the seam with Zenith_CreateSteamApiScripted (refused in Shipping),
///   otherwise the flat-API seam; then Zenith_CreateOnlineServicesOverSteamApi.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> Zenith_CreateOnlineServices_Steam(
	const Zenith_OnlineServicesDesc& xDesc);
```

## Zenith_Online_GooglePlay header

```cpp
// Zenith2/Engine/ZenithOnline_GooglePlay/Zenith_Online_GooglePlay.h
#pragma once
#include "ZenithOnline/Zenith_OnlineServices.h"

/// @contract functions Zenith_Online_GooglePlay
/// Ownership: Returns the Google Play backend (Internal Zenith_OnlineServicesGooglePlay) allocated on xDesc.m_pxHeap; it
///   owns the Play bridge seam implementation selected by xDesc.m_xPlay.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: ONLINE_ERROR_INVALID_DESC, ONLINE_ERROR_OUT_OF_MEMORY, ONLINE_ERROR_BACKEND_UNAVAILABLE (the Java bridge class is
///   missing or its init returned false: Play Games Services or the Billing library is absent; the module then falls back to
///   Offline).
/// Version: since Z2 Stage 6. Android only (SUPPORTED); Zenith2/Docs/Design/ONLINE/Backends.md → "Google Play backend". With
///   online.googlePlay.scriptedBridge set it creates the bridge with Zenith_CreatePlayBridgeScripted (refused in Shipping),
///   otherwise the JNI bridge; then Zenith_CreateOnlineServicesOverPlayBridge.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> Zenith_CreateOnlineServices_GooglePlay(
	const Zenith_OnlineServicesDesc& xDesc);
```

## Zenith_OnlineSteamCore header

The store-independent half of the Steam backend (`Zenith2/Docs/Design/ONLINE/Backends.md` → "Store backend libraries and what the gates prove"): the seam every Steamworks call goes through, its scripted recording implementation, and the factory of the Steam backend's logic over a seam. No Steamworks type or header appears here; `ZenithOnline_Steam` supplies the flat-API implementation.

```cpp
// Zenith2/Engine/ZenithOnline_SteamCore/Zenith_OnlineSteamCore.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithOnline/Zenith_OnlineServices.h"

inline constexpr uint32 uONLINE_STEAM_READ_CHUNK_BYTES = 4194304;   // one FileReadAsync call; bounds each main-thread copy

/// @contract interface Zenith_SteamCallbackSink
/// Ownership: Implemented by the Steam backend's logic (Internal); borrowed by Zenith_SteamApi::RunFrame for the call.
/// Thread affinity: MAIN_THREAD; called only from inside RunFrame.
/// Invalidation: None.
/// Units: Steam call handles (opaque uint64); EResult and ESteamAPIInitResult values as sint32.
/// Errors: None; every value is untrusted SDK data the backend validates (Backends.md "Request completion rules" rule 8).
/// Reentrancy: NONE. A callback never calls RunFrame; it may call the other Zenith_SteamApi members.
/// Version: since Z2 Stage 6.
/// @endcontract
class Zenith_SteamCallbackSink
{
public:
	virtual ~Zenith_SteamCallbackSink() = default;
	virtual void OnSteamShutdown() = 0;                                         // SteamShutdown_t
	virtual void OnServersConnected(bool bConnected) = 0;                       // SteamServersConnected_t / Disconnected_t
	virtual void OnDlcInstalled(uint32 uAppID) = 0;                            // DlcInstalled_t
	virtual void OnUserStatsReceived(sint32 iResult) = 0;                      // UserStatsReceived_t, local user only
	virtual void OnUserStatsStored(sint32 iResult) = 0;                        // UserStatsStored_t
	virtual void OnFileWriteAsyncComplete(uint64 ulCall, sint32 iResult) = 0; // RemoteStorageFileWriteAsyncComplete_t
	virtual void OnFileReadAsyncComplete(uint64 ulCall, sint32 iResult, uint32 uOffset,
		uint32 uBytesRead) = 0;                                                // RemoteStorageFileReadAsyncComplete_t
};

/// @contract interface Zenith_SteamApi
/// Ownership: Created by Zenith_CreateSteamApiScripted or by ZenithOnline_Steam's flat-API factory (Internal); owned by the
///   Steam backend it is given to.
/// Thread affinity: MAIN_THREAD for every member.
/// Invalidation: A call handle is invalid once its call result has been delivered to the sink.
/// Units: Bytes; Steam's own units for timestamps (seconds since 1970, sint64).
/// Errors: Each member reports failure as Steam does (false, 0, or the invalid handle 0); the backend maps them.
/// Reentrancy: NONE. RunFrame invokes the sink; the sink's rules are its contract block's.
/// Version: since Z2 Stage 6. Internal seam of the Steam backend family; the flat-API implementation calls the functions named
///   in Backends.md "Steam API seam", the scripted one calls nothing outside the process.
/// @endcontract
class Zenith_SteamApi
{
public:
	virtual ~Zenith_SteamApi() = default;
	virtual sint32 Init(uint32 uAppID, bool bRestartIfNecessary, bool& bOutMustExit) = 0;   // ESteamAPIInitResult
	virtual void Shutdown() = 0;
	virtual void RunFrame(Zenith_SteamCallbackSink& xSink) = 0;       // manual dispatch of callbacks and call results
	virtual uint64 GetSteamID() const = 0;
	virtual bool IsLoggedOn() const = 0;
	virtual Zenith_StringView GetPersonaName() const = 0;
	virtual bool IsSubscribedApp(uint32 uAppID) const = 0;
	virtual bool GetAchievement(Zenith_StringView strName, bool& bOutAchieved) const = 0;
	virtual bool SetAchievement(Zenith_StringView strName) = 0;
	virtual bool GetStatInt32(Zenith_StringView strName, sint32& iOut) const = 0;
	virtual bool SetStatInt32(Zenith_StringView strName, sint32 iValue) = 0;
	virtual bool IndicateAchievementProgress(Zenith_StringView strName, uint32 uCurrent, uint32 uMax) = 0;
	virtual bool StoreStats() = 0;
	virtual bool IsCloudEnabledForApp() const = 0;
	virtual bool IsCloudEnabledForAccount() const = 0;
	virtual bool GetQuota(uint64& ulOutTotal, uint64& ulOutAvailable) const = 0;
	virtual uint32 GetFileCount() const = 0;
	virtual Zenith_StringView GetFileNameAndSize(uint32 uIndex, uint32& uOutBytes) const = 0;
	virtual uint32 GetFileSize(Zenith_StringView strName) const = 0;           // 0: absent
	virtual sint64 GetFileTimestamp(Zenith_StringView strName) const = 0;
	// Asynchronous transfers: each returns the call handle (0: the call failed); the result arrives through RunFrame.
	virtual uint64 FileWriteAsync(Zenith_StringView strName, Zenith_ByteSpan xBytes) = 0;
	virtual uint64 FileReadAsync(Zenith_StringView strName, uint32 uOffset, uint32 uBytes) = 0;
	// Called from the read's OnFileReadAsyncComplete only: copies the call's bytes into xOut (at most uBytesRead).
	virtual bool FileReadAsyncComplete(uint64 ulCall, Zenith_MutableByteSpan xOut) = 0;
	virtual bool FileDelete(Zenith_StringView strName) = 0;    // metadata only; called from Pump
};

/// @contract value Zenith_SteamApiScriptedDesc
/// Ownership: Plain value; the views and the heap are borrowed for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: After the factory returns.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_SteamApiScriptedDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_StringView m_strScriptJson;          // the script's text (Backends.md "Steam API seam")
	bool m_bShipping = false;                   // Zenith_GetBuildIdentity().m_bShipping: a Shipping build refuses the seam
};

/// @contract functions Zenith_OnlineSteamCore
/// Ownership: Each factory returns a Zenith_Owned allocated on the given heap; Zenith_CreateOnlineServicesOverSteamApi takes
///   ownership of the seam.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_CreateSteamApiScripted: ONLINE_ERROR_INVALID_DESC (Shipping, a null heap, a script failing the JSON limits or
///   its schema, with the JSON pointer logged), ONLINE_ERROR_OUT_OF_MEMORY. Zenith_CreateOnlineServicesOverSteamApi: the
///   errors of Zenith_Online_Steam's contract block (ONLINE_ERROR_BACKEND_UNAVAILABLE with the init result, or −1 for a
///   relaunch through Steam).
/// Version: since Z2 Stage 6. Windows presets only (SUPPORTED ZENITH_WINDOWS); built and tested without the Steamworks SDK.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_SteamApi>, Zenith_OnlineError> Zenith_CreateSteamApiScripted(
	const Zenith_SteamApiScriptedDesc& xDesc);
Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> Zenith_CreateOnlineServicesOverSteamApi(
	const Zenith_OnlineServicesDesc& xDesc, Zenith_Owned<Zenith_SteamApi> xApi);
```

`Zenith2/Engine/ZenithOnline_SteamCore/Zenith_OnlineSteamCoreTestAccess.h` (tests only, `TestSeparationCheck` rule 6) declares `struct Zenith_SteamRecordedCall { uint8 m_uKind; uint8 m_uNameLength; char m_acName[128]; sint64 m_iValue; }` (kinds 1 `SetAchievement`, 2 `SetStatInt32`, 3 `IndicateAchievementProgress`, 4 `StoreStats`, 5 `FileWriteAsync`, 6 `FileReadAsync`, 7 `FileDelete`; the value is the stat, the progress, the byte count or the offset) and `class Zenith_OnlineSteamCoreTestAccess { public: static Zenith_Span<const Zenith_SteamRecordedCall> GetRecordedCalls(const Zenith_SteamApi& xScripted); static Zenith_SteamApi* GetApi(Zenith_OnlineServices& xSteamServices); };` with one contract block each (Ownership: views into the scripted seam's own record, valid until its next mutating call; Thread affinity: MAIN_THREAD; an argument that is not the scripted seam, or not a backend created over a Steam seam, answers an empty span or null).

## Zenith_OnlineGooglePlayCore header

The store-independent half of the Google Play backend: the bridge every Play Games and Play Billing call goes through, its scripted recording implementation, and the factory of the Play backend's logic over a bridge. No JNI or Java type appears here; `ZenithOnline_GooglePlay` supplies the JNI implementation.

```cpp
// Zenith2/Engine/ZenithOnline_GooglePlayCore/Zenith_OnlineGooglePlayCore.h
#pragma once
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithOnline/Zenith_OnlineServices.h"

/// @contract interface Zenith_PlayEventSink
/// Ownership: Implemented by the Play backend's logic (Internal); borrowed by Zenith_PlayBridge::Drain for the call.
/// Thread affinity: MAIN_THREAD; called only from inside Drain, in the order the bridge queued the events.
/// Invalidation: Every span and view argument is valid for the call only.
/// Units: Play's own codes (GamesStatusCodes, BillingResponseCode) as sint32; milliseconds.
/// Errors: None; every value is untrusted service data the backend validates (Backends.md "Request completion rules" rule 8).
/// Reentrancy: NONE. A callback never calls Drain; it may call the other Zenith_PlayBridge members.
/// Version: since Z2 Stage 6. One member per native of Backends.md "Java bridge", with the same arguments.
/// @endcontract
class Zenith_PlayEventSink
{
public:
	virtual ~Zenith_PlayEventSink() = default;
	virtual void OnPlayer(uint64 ulRequestID, sint32 iStatus, bool bAuthenticated, Zenith_StringView strPlayerID,
		Zenith_StringView strDisplayName) = 0;
	virtual void OnPurchases(uint64 ulRequestID, sint32 iBillingResponse, Zenith_Span<const Zenith_StringView> axProductIDs,
		Zenith_Span<const sint32> aiStates) = 0;
	virtual void OnAchievements(uint64 ulRequestID, sint32 iStatus, Zenith_Span<const Zenith_StringView> axIDs,
		Zenith_Span<const uint8> auUnlocked, Zenith_Span<const sint32> aiSteps) = 0;
	virtual void OnDone(uint64 ulRequestID, sint32 iStatus, sint32 iNativeCode) = 0;
	virtual void OnSnapshotList(uint64 ulRequestID, sint32 iStatus, Zenith_Span<const Zenith_StringView> axNames,
		Zenith_Span<const sint64> aiModifiedMs, Zenith_Span<const sint64> aiPlayedMs, Zenith_Span<const sint64> aiBytes) = 0;
	// The bytes were already copied into the request's read buffer; uBytes says how many.
	virtual void OnSnapshotData(uint64 ulRequestID, sint32 iStatus, uint64 ulBytes, sint64 iModifiedMs, sint64 iPlayedMs) = 0;
	virtual void OnPurchasesUpdated() = 0;
	virtual void OnResume() = 0;
	virtual void OnQueueOverflow(uint64 ulRequestID) = 0;   // the bridge's queue refused this request's event
};

/// @contract interface Zenith_PlayBridge
/// Ownership: Created by Zenith_CreatePlayBridgeScripted or by ZenithOnline_GooglePlay's JNI factory (Internal); owned by the
///   Play backend it is given to.
/// Thread affinity: MAIN_THREAD for every member; the JNI implementation's natives run on the Android UI thread and only queue,
///   under uLOCK_LEVEL_ONLINE_PLAY_EVENTS.
/// Invalidation: A read buffer registered by ReadSnapshot is written only until its OnSnapshotData is drained or CancelRequest
///   returns, whichever is first.
/// Units: Bytes; milliseconds.
/// Errors: Each call returns false when the request could not be issued (the backend completes it BACKEND_ERROR at the next Pump).
/// Reentrancy: NONE. Drain invokes the sink; the sink's rules are its contract block's.
/// Version: since Z2 Stage 6.
/// @endcontract
class Zenith_PlayBridge
{
public:
	virtual ~Zenith_PlayBridge() = default;
	virtual bool Init() = 0;                  // false: Play services or the Billing library absent
	virtual void Shutdown() = 0;
	virtual void Drain(Zenith_PlayEventSink& xSink) = 0;
	virtual bool RefreshIdentity(uint64 ulRequestID, bool bInteractive) = 0;
	virtual bool QueryPurchases(uint64 ulRequestID, Zenith_Span<const Zenith_StringView> axProductIDs) = 0;
	virtual bool LoadAchievements(uint64 ulRequestID, Zenith_Span<const Zenith_StringView> axIDs) = 0;
	virtual bool UnlockAchievement(uint64 ulRequestID, Zenith_StringView strID) = 0;
	virtual bool SetAchievementSteps(uint64 ulRequestID, Zenith_StringView strID, sint32 iSteps) = 0;
	virtual bool ListSnapshots(uint64 ulRequestID) = 0;
	virtual bool ReadSnapshot(uint64 ulRequestID, Zenith_StringView strName, Zenith_MutableByteSpan xReadBuffer) = 0;
	virtual bool WriteSnapshot(uint64 ulRequestID, Zenith_StringView strName, Zenith_ByteSpan xBytes, sint64 iPlayedMs,
		Zenith_StringView strDescription, sint64 iExpectedModifiedMs, bool bConditional) = 0;
	virtual bool DeleteSnapshot(uint64 ulRequestID, Zenith_StringView strName, sint64 iExpectedModifiedMs,
		bool bConditional) = 0;
	// Removes the request's read-buffer registration, so nothing writes the buffer afterwards; events of the request that arrive
	// afterwards are dropped by the bridge.
	virtual void CancelRequest(uint64 ulRequestID) = 0;
	virtual uint64 GetMaxSnapshotBytes() const = 0;
};

/// @contract value Zenith_PlayBridgeScriptedDesc
/// Ownership: Plain value; the views and the heap are borrowed for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the fields they use.
/// Invalidation: After the factory returns.
/// Units: None.
/// Errors: None of its own.
/// Version: since Z2 Stage 6.
/// @endcontract
struct Zenith_PlayBridgeScriptedDesc
{
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_StringView m_strScriptJson;          // the script's text (Backends.md "Play bridge seam")
	bool m_bShipping = false;                   // a Shipping build refuses the seam
	uint32 m_uQueueCapacity = 256;             // the event queue's capacity, as the JNI bridge's
};

/// @contract type Zenith_PlayEventQueue
/// Ownership: Created by Create on the given heap; owned by a Zenith_PlayBridge (the scripted bridge and ZenithOnline_GooglePlay's
///   JNI bridge each own one); reserves its ring and its request table at Create, never allocating afterwards.
/// Thread affinity: The Zenith_PlayEventSink members and CopySnapshotBytes are ANY_THREAD (the producer: the Android UI thread in
///   the JNI bridge, the main thread in the scripted one); RegisterReadBuffer, UnregisterReadBuffer and Drain are MAIN_THREAD.
///   Every member takes uLOCK_LEVEL_ONLINE_PLAY_EVENTS (670), copies bytes or pops under it, takes no other lock and allocates nothing.
/// Invalidation: A buffer passed to RegisterReadBuffer is written only by CopySnapshotBytes, and only until UnregisterReadBuffer
///   returns or the request's OnSnapshotData is drained.
/// Units: Bytes; records.
/// Errors: Create: ONLINE_ERROR_INVALID_DESC (a capacity of 0 or above 256), ONLINE_ERROR_OUT_OF_MEMORY. A producer call that does
///   not fit (uCapacity records, or uPLAY_EVENT_QUEUE_BYTES of copied arguments) is refused (QUEUE_FULL_POLICY_REJECT): its request
///   gets the overflow flag, Drain reports OnQueueOverflow(request) once before any later record, and online.play.events.full counts it.
/// Reentrancy: NONE. Drain pops under the lock and calls xSink outside it; xSink never calls Drain.
/// Version: since Z2 Stage 6. Answers Z2-R-ONLINE-133 for both bridges.
/// @endcontract
inline constexpr uint32 uPLAY_EVENT_QUEUE_BYTES = 1048576;
class Zenith_PlayEventQueue final : public Zenith_PlayEventSink
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_PlayEventQueue>, Zenith_OnlineError> Create(Zenith_Heap& xHeap,
		uint32 uCapacity);
	// Producer side: each Zenith_PlayEventSink member copies its arguments into one record (refused when full, as above).
	void OnPlayer(uint64 ulRequestID, sint32 iStatus, bool bAuthenticated, Zenith_StringView strPlayerID,
		Zenith_StringView strDisplayName) override;
	void OnPurchases(uint64 ulRequestID, sint32 iBillingResponse, Zenith_Span<const Zenith_StringView> axProductIDs,
		Zenith_Span<const sint32> aiStates) override;
	void OnAchievements(uint64 ulRequestID, sint32 iStatus, Zenith_Span<const Zenith_StringView> axIDs,
		Zenith_Span<const uint8> auUnlocked, Zenith_Span<const sint32> aiSteps) override;
	void OnDone(uint64 ulRequestID, sint32 iStatus, sint32 iNativeCode) override;
	void OnSnapshotList(uint64 ulRequestID, sint32 iStatus, Zenith_Span<const Zenith_StringView> axNames,
		Zenith_Span<const sint64> aiModifiedMs, Zenith_Span<const sint64> aiPlayedMs, Zenith_Span<const sint64> aiBytes) override;
	void OnSnapshotData(uint64 ulRequestID, sint32 iStatus, uint64 ulBytes, sint64 iModifiedMs, sint64 iPlayedMs) override;
	void OnPurchasesUpdated() override;
	void OnResume() override;
	void OnQueueOverflow(uint64 ulRequestID) override;    // sets the request's overflow flag directly
	// Copies xBytes at ulOffset into the request's registered buffer; false (nothing copied) when none is registered or it is too small.
	bool CopySnapshotBytes(uint64 ulRequestID, uint64 ulOffset, Zenith_ByteSpan xBytes);
	// Consumer side.
	void RegisterReadBuffer(uint64 ulRequestID, Zenith_MutableByteSpan xBuffer);
	void UnregisterReadBuffer(uint64 ulRequestID);           // CancelRequest; the request's later records are dropped
	void Drain(Zenith_PlayEventSink& xSink);                  // in push order, each overflow before the request's later records
private:
	struct PrivateTag {};
public:
	Zenith_PlayEventQueue(PrivateTag, Zenith_Heap& xHeap, uint32 uCapacity);
};

/// @contract functions Zenith_OnlineGooglePlayCore
/// Ownership: Each factory returns a Zenith_Owned allocated on the given heap; Zenith_CreateOnlineServicesOverPlayBridge takes
///   ownership of the bridge.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_CreatePlayBridgeScripted: ONLINE_ERROR_INVALID_DESC (Shipping, a null heap, a script failing the JSON limits
///   or its schema), ONLINE_ERROR_OUT_OF_MEMORY. Zenith_CreateOnlineServicesOverPlayBridge: ONLINE_ERROR_BACKEND_UNAVAILABLE when
///   the bridge's Init returns false, ONLINE_ERROR_INVALID_DESC, ONLINE_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 6. Every ZENITH_GPU preset (SUPPORTED); built and tested on Windows without Play services.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_PlayBridge>, Zenith_OnlineError> Zenith_CreatePlayBridgeScripted(
	const Zenith_PlayBridgeScriptedDesc& xDesc);
Zenith_Result<Zenith_Owned<Zenith_OnlineServices>, Zenith_OnlineError> Zenith_CreateOnlineServicesOverPlayBridge(
	const Zenith_OnlineServicesDesc& xDesc, Zenith_Owned<Zenith_PlayBridge> xBridge);
```

`Zenith2/Engine/ZenithOnline_GooglePlayCore/Zenith_OnlineGooglePlayCoreTestAccess.h` (tests only) declares `struct Zenith_PlayRecordedCall { uint8 m_uKind; uint8 m_uNameLength; char m_acName[128]; sint64 m_iValue; }` (kinds 1 `RefreshIdentity`, 2 `QueryPurchases`, 3 `LoadAchievements`, 4 `UnlockAchievement`, 5 `SetAchievementSteps`, 6 `ListSnapshots`, 7 `ReadSnapshot`, 8 `WriteSnapshot`, 9 `DeleteSnapshot`, 10 `CancelRequest`, 11 an acknowledgement; no field holds a purchase token, which the scripted purchases do not carry) and `class Zenith_OnlineGooglePlayCoreTestAccess { public: static Zenith_Span<const Zenith_PlayRecordedCall> GetRecordedCalls(const Zenith_PlayBridge& xScripted); static Zenith_PlayBridge* GetBridge(Zenith_OnlineServices& xPlayServices); };` with the same contract rules as the Steam test access.

## Internal declarations

These are `Internal/` declarations of `ZenithOnline` (not public types; `ApiContractCheck` does not read them); each states its affinity in a comment. Tests in `ZenithOnline_Tests` include them.

```cpp
// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineModuleInstance.h
class Zenith_OnlineModuleInstance final : public Zenith_ModuleInstance, public Zenith_ModuleFrameListener
{
public:
	// MAIN_THREAD. Creates the world record; registers the Online.Events system and the sink handler; journaled by the context.
	Zenith_Result<void, Zenith_ModuleError> AttachWorld(Zenith_WorldAttachContext& xContext) override;
	void DetachWorld(uint16 uWorldTag) override;
	void OnPlatformEvents(Zenith_Span<const Zenith_PlatformEvent> axEvents) override;   // Pump; lifecycle background
	~Zenith_OnlineModuleInstance();   // BeginShutdown, drain loop (PumpShutdown + backend Pump) ≤ flush + 1,000 ms, Shutdown
private:
	Zenith_Owned<Zenith_OnlineCatalogue> m_xCatalogue;
	Zenith_Owned<Zenith_OnlineServices> m_xServices;       // null in replay mode
	Zenith_Owned<Zenith_OnlineSession> m_xSession;
	Zenith_OnlineScriptConsumer m_xScriptConsumer;
	Zenith_OnlineSinkPort m_axPorts[uONLINE_MAX_ATTACHED_WORLDS];   // one per attached world, over its module publication sink
	Zenith_DevCommandToken m_axConsoleTokens[8];           // development builds; null tokens in Shipping
	Zenith_ProfilerPanelToken m_xPanelToken;               // development builds
};

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineSinkPort.h
// The production Zenith_OnlinePublicationPort: forwards to the world's Zenith_ModulePublicationSink (MOD's single module
// publication sink, Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Zenith_ModulePublication header"): IsReplaying,
// AllocateCompletionID(ASYNC_SOURCE_ONLINE) (an error maps to 0 with the refusal; the sink admits the prologue site of this
// script-declared mixed kind, RuntimeSeams.md "Allocation sites and acceptance" step 1), and
// Park(id, ASYNC_SOURCE_ONLINE, PUBLICATION_KIND_GENERIC_RECORDED, payload).
class Zenith_OnlineSinkPort final : public Zenith_OnlinePublicationPort
{
public:
	void Bind(Zenith_ModulePublicationSink* pxSink);   // AttachWorld; nullptr at DetachWorld
	bool IsReplaying() const override;
	uint64 AllocateNotificationID(Zenith_OnlineNotifyRefusal& eRefusalOut) override;
	void Park(uint64 ulCompletionID, Zenith_ByteSpan xPayload) override;
};

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineScriptConsumer.h
// SCRIPT's Zenith_ScriptSideEffectConsumer for SIDE_EFFECT_KIND_ASYNC_REQUEST records of ASYNC_SOURCE_ONLINE, routed by the
// runtime's Zenith_AsyncRequestMultiplexer; called on the committing world's merge node; forwards to SubmitScriptRequest.
class Zenith_OnlineScriptConsumer final : public Zenith_ScriptSideEffectConsumer
{
public:
	explicit Zenith_OnlineScriptConsumer(Zenith_OnlineSession& xSession);
	void Consume(uint16 uWorldTag, Zenith_SideEffectKind eKind, uint64 ulCompletionID, Zenith_ByteSpan xPayload) override;
};

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineCodec.h  (ANY_THREAD pure functions; fuzzed)
Zenith_Result<Zenith_OnlineScriptRequest, Zenith_OnlineError> Zenith_DecodeOnlineScriptRequest(Zenith_ByteSpan xPayload);
uint32 Zenith_EncodeOnlineResult(const Zenith_OnlineScriptResult& xResult, Zenith_MutableByteSpan xOut);
uint32 Zenith_EncodeOnlineNotification(const Zenith_OnlineNotification& xNotification, Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_OnlineNotification, Zenith_OnlineError> Zenith_DecodeOnlineNotification(Zenith_ByteSpan xPayload);
bool Zenith_DecodeOnlineResultForScript(uint8 uPublicationKind, Zenith_ByteSpan xPayload,
	Zenith_ScriptResultFields& xOut);   // the Zenith_ScriptAsyncResultDecoder of ASYNC_SOURCE_ONLINE

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineAccountKeys.h  (ANY_THREAD pure functions)
// 0 → 1, every other value unchanged (0 is "no account").
constexpr Zenith_OnlineAccountKey Zenith_OnlineAccountKeyFromHash(uint64 ulHash);
// Zenith_OnlineAccountKeyFromHash(LE64 of the first 8 bytes of Zenith_Sha256Digest(xInstallSecret ‖ account ID bytes)); an empty
// account gives 0. xInstallSecret is the device record's 32 bytes (Formats.md "Device record").
Zenith_OnlineAccountKey Zenith_DeriveOnlineAccountKey(Zenith_ByteSpan xInstallSecret,
	const Zenith_OnlineAccountID& xAccount);
// The offline backend's directory token: the first 8 bytes of SHA-256("zenith2.offline-store:" ‖ account ID bytes), each byte
// as two lower-case hexadecimal digits, byte 0 first, into acOut (16 characters, no terminator); offline:alice → 2796e9763c2cb976.
void Zenith_FormatOfflineStoreKey(const Zenith_OnlineAccountID& xAccount, char (&acOut)[16]);
// Formats.md "Commit tokens and the commit log": LE64 of the first 8 bytes of
// SHA-256("z2.commit" ‖ uint8 slot length ‖ slot ‖ 32-byte hash ‖ uint8 deleted), 0 mapped to 1.
uint64 Zenith_ComputeOnlineCommitToken(const Zenith_SaveSlotName& xSlot, const Zenith_Sha256Value& xHash,
	bool bDeleted);

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineCloudEngine.h  (MAIN_THREAD)
// assumption pending SAVE (row ON1 (c)): Zenith_SaveSlotListener is SAVE's lower-declared interface, called on the main thread
// in Zenith_SaveSystem::PumpMainThread.
class Zenith_OnlineSaveListener final : public Zenith_SaveSlotListener
{
public:
	explicit Zenith_OnlineSaveListener(Zenith_OnlineCloudEngine& xEngine);
	// An eligible slot's write: marks it dirty and (re)starts its upload debounce, unless the slot is DOWNLOADING (the
	// session's own import); CloudSave.md "Polling and triggers".
	void OnSlotWritten(const Zenith_SaveSlotInfo& xInfo) override;
	// The tombstone rule of CloudSave.md "Deletion propagation" (not for a slot the session itself is DELETING_LOCAL),
	// then queues a targeted reconcile; its SAVE calls are made from the session's next Pump, never from this callback.
	void OnSlotDeleted(const Zenith_SaveSlotName& xSlot, Zenith_SaveKind eKind) override;
};
// Lifecycle: Zenith_OnlineSession::Create builds it with the cloud engine and, outside replay, calls
// SetSlotListener(&listener) as its last step (a refusal or a failure of any earlier step unwinds with SetSlotListener(nullptr)
// when it was set); BeginShutdown calls SetSlotListener(nullptr) first, before any flush; the session's destructor asserts it
// is cleared (Zenith2/CLAUDE.md non-negotiable 12: callbacks are unregistered before their owner is destroyed).

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineCloudFormats.h  (ANY_THREAD pure functions; fuzzed)
Zenith_Result<Zenith_OnlineCloudObjectName, Zenith_OnlineError> Zenith_ParseOnlineCloudObjectName(Zenith_StringView strName);
uint32 Zenith_FormatOnlineCloudObjectName(const Zenith_OnlineCloudObjectName& xName, Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_OnlineDeviceRecord, Zenith_OnlineError> Zenith_ParseOnlineDeviceRecord(Zenith_ByteSpan xFile);
// CloudSave.md "Index view": classifies one read of z2.index (the read's status and bytes, the listing when one was made, the
// active profile); NEWER_FORMAT requires at least 48 bytes, the magic ZCIX and a version above 1, read before parsing.
Zenith_OnlineIndexView Zenith_ClassifyOnlineCloudIndex(const Zenith_OnlineIndexViewInputs& xInputs,
	Zenith_Arena& xArena);
Zenith_Result<Zenith_OnlineEnvelopeHeader, Zenith_OnlineError> Zenith_ParseOnlineEnvelope(Zenith_ByteSpan xObject);
Zenith_Result<uint64, Zenith_OnlineError> Zenith_WriteOnlineEnvelope(const Zenith_OnlineEnvelopeHeader& xHeader,
	Zenith_ByteSpan xPayload, Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_OnlineCloudIndex, Zenith_OnlineError> Zenith_ParseOnlineCloudIndex(Zenith_ByteSpan xObject,
	Zenith_Arena& xArena);
Zenith_Result<uint64, Zenith_OnlineError> Zenith_WriteOnlineCloudIndex(const Zenith_OnlineCloudIndex& xIndex,
	Zenith_MutableByteSpan xOut);
Zenith_Result<Zenith_OnlineSyncManifest, Zenith_OnlineError> Zenith_ParseOnlineSyncManifest(Zenith_ByteSpan xFile,
	Zenith_Arena& xArena);
Zenith_Result<uint64, Zenith_OnlineError> Zenith_WriteOnlineSyncManifest(const Zenith_OnlineSyncManifest& xManifest,
	Zenith_MutableByteSpan xOut);
// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlineCloudDecision.h  (ANY_THREAD pure functions)
// The view of one reconcile's read of z2.index (CloudSave.md "Index view"); the values equal the online.cloud.index_view gauge.
enum Zenith_OnlineIndexViewKind : uint8
{
	ONLINE_INDEX_VIEW_KNOWN = 1,
	ONLINE_INDEX_VIEW_EMPTY = 2,
	ONLINE_INDEX_VIEW_UNKNOWN_READ_FAILED = 3,
	ONLINE_INDEX_VIEW_UNKNOWN_INVALID = 4,
	ONLINE_INDEX_VIEW_UNKNOWN_FOREIGN_PROFILE = 5,
	ONLINE_INDEX_VIEW_UNKNOWN_MISSING = 6,
	ONLINE_INDEX_VIEW_UNKNOWN_NEWER_FORMAT = 7,
};
struct Zenith_OnlineIndexViewInputs
{
	Zenith_OnlineStatus m_eReadStatus = ONLINE_STATUS_OK;          // the CLOUD_READ completion of z2.index
	Zenith_ByteSpan m_xIndexBytes;                                  // its bytes when OK
	bool m_bListed = false;                                         // a CLOUD_LIST of the same reconcile completed
	bool m_bListingFailed = false;                                  // a CLOUD_LIST was needed and failed
	uint32 m_uListedEnvelopeNames = 0;                             // listed objects whose name begins "z2." other than z2.index
	Zenith_SaveProfileKey m_xActiveProfile;
};
struct Zenith_OnlineIndexView
{
	Zenith_OnlineIndexViewKind m_eKind = ONLINE_INDEX_VIEW_UNKNOWN_READ_FAILED;
	uint16 m_uFailedRule = 0;                                      // INVALID: the parser's rule (m_uDetail)
	uint64 m_ulSerial = 0;                                         // KNOWN: the index serial; EMPTY: 0
	bool m_bRepaired = false;                                       // KNOWN: the header's repaired flag
	Zenith_OnlineCloudIndex m_xIndex;                               // KNOWN: the parsed index (entries and deletion records), in xArena
};
// What the decision table reads of L and C (CloudSave.md "Hashes and identities").
enum Zenith_OnlineSyncSideKind : uint8
{
	ONLINE_SYNC_SIDE_ABSENT = 1,
	ONLINE_SYNC_SIDE_HASH = 2,         // L: the local hash; C: a live entry
	ONLINE_SYNC_SIDE_DELETED = 3,      // C only: a deletion record
	ONLINE_SYNC_SIDE_UNKNOWN = 4,      // L: the export failed; C: the view is UNKNOWN
};
struct Zenith_OnlineSyncSide
{
	Zenith_OnlineSyncSideKind m_eKind = ONLINE_SYNC_SIDE_ABSENT;
	Zenith_Sha256Value m_xHash;       // HASH: the payload hash; DELETED: the deleted payload hash
	uint64 m_ulCommitSerial = 0;      // C: the entry's or record's commit serial
	bool m_bRepaired = false;          // C: the entry's repaired flag
};
// Whether the read index's commit log proves this device's base is in its history, CloudSave.md "Hashes and identities".
enum Zenith_OnlineLineage : uint8
{
	ONLINE_LINEAGE_DESCENDS = 1,       // no base, or the log holds (s_B, token(slot, b, live)): the only proof
	ONLINE_LINEAGE_HORIZON = 2,        // neither; s_B ≤ the log floor (overflow or repair floor): untestable
	ONLINE_LINEAGE_DIVERGED = 3,       // otherwise: s_B above the floor without its pair; decides exactly as HORIZON
};
struct Zenith_OnlineSyncInputs
{
	bool m_bHasBase = false;                   // B is not NONE
	Zenith_Sha256Value m_xBase;               // b
	uint64 m_ulBaseSerial = 0;                // s_B
	bool m_bTombstone = false;                 // T
	Zenith_OnlineSyncSide m_xLocal;            // L (m_ulCommitSerial and m_bRepaired unused)
	Zenith_OnlineSyncSide m_xCloud;            // C
	bool m_bLocalFromBackup = false;           // SAVE listed the slot RECOVERABLE_FROM_BACKUP (override 5)
	bool m_bLocalVersionSkipped = false;       // NEWER_VERSION or INCOMPATIBLE (override 3)
	bool m_bCloudNewerFormat = false;          // the entry's save format is newer or of another major, or the view is NEWER_FORMAT (override 3)
	bool m_bTooLarge = false;                  // override 3
	bool m_bInConflictUnchanged = false;       // override 4: CONFLICT recorded with these same inputs
	Zenith_OnlineLineage m_eLineage = ONLINE_LINEAGE_DESCENDS;   // CloudSave.md "Hashes and identities"
	bool m_bCloudIsPreviousBase = false;       // the entry's token (slot, y, live) equals the manifest's previous base token
};
enum Zenith_OnlineSyncActionKind : uint8
{
	ONLINE_SYNC_ACTION_NONE = 1,
	ONLINE_SYNC_ACTION_UPLOAD = 2,
	ONLINE_SYNC_ACTION_UPLOAD_LOST_COMMIT = 3,     // rows 18, 19: re-commit what this device holds; U4 lets it through
	ONLINE_SYNC_ACTION_DOWNLOAD = 4,
	ONLINE_SYNC_ACTION_ADOPT = 5,
	ONLINE_SYNC_ACTION_CONFLICT = 6,
	ONLINE_SYNC_ACTION_DELETE_CLOUD = 7,           // rows 11, 20: write a deletion record
	ONLINE_SYNC_ACTION_DELETE_LOCAL = 8,           // row 12 only (DESCENDS, or a record of b)
	ONLINE_SYNC_ACTION_FORGET = 9,
	ONLINE_SYNC_ACTION_ERROR = 10,                 // override 1
	ONLINE_SYNC_ACTION_CLOUD_UNKNOWN = 11,         // override 2
	ONLINE_SYNC_ACTION_SKIP = 12,                  // override 3 (with the state in m_eState)
	ONLINE_SYNC_ACTION_KEEP_CONFLICT = 13,         // override 4
	ONLINE_SYNC_ACTION_ADVANCE_BASE_SERIAL = 14,   // row 6 with c > s_B: manifest only
};
struct Zenith_OnlineSyncAction
{
	Zenith_OnlineSyncActionKind m_eAction = ONLINE_SYNC_ACTION_NONE;
	Zenith_OnlineConflictReason m_eReason = ONLINE_CONFLICT_REASON_NONE;   // CONFLICT
	Zenith_OnlineCloudSlotState m_eState = ONLINE_CLOUD_SLOT_IN_SYNC;       // the slot state the action leaves (CloudSave.md "Slot sync state machine")
	uint8 m_uRow = 0;                         // 1 … 26, or 0 when an override decided
	uint8 m_uOverride = 0;                    // 1 … 7, or 0 when a row decided
	uint64 m_ulNewBaseSerial = 0;             // ADOPT and ADVANCE_BASE_SERIAL: the commit serial to record
};
// The decision table with its overrides (CloudSave.md "Three-way decision table"); every row and override is one test.
Zenith_OnlineSyncAction Zenith_DecideOnlineSync(const Zenith_OnlineSyncInputs& xInputs);

// Zenith2/Engine/ZenithOnline/Internal/Zenith_OnlinePackEntitlements.h  (Stage 6; MAIN_THREAD)
class Zenith_OnlinePackEntitlements final : public Zenith_EntitlementQuery
{
public:
	explicit Zenith_OnlinePackEntitlements(const Zenith_OnlineSession& xSession);
	// The cached state mapped one to one, or DENIED for every ID in the fenced-deny mode (Accounts.md "Entitlement query for packages").
	Zenith_EntitlementState GetEntitlement(Zenith_StringView strEntitlementID) const override;
	uint64 GetRevision() const override;
};
```

The struct types named above (`Zenith_OnlineScriptRequest`, `Zenith_OnlineScriptResult`, `Zenith_OnlineNotification`, `Zenith_OnlineEnvelopeHeader`, `Zenith_OnlineCloudObjectName`, `Zenith_OnlineCloudIndex`, `Zenith_OnlineSyncManifest`, `Zenith_OnlineDeviceRecord`) are the in-memory forms of the layouts of `Zenith2/Docs/Design/ONLINE/Formats.md`, field for field, in the same order and with the same names in Hungarian (`m_u…`, `m_ul…`, `m_x…`); the cloud index's form holds the commit log (floor, count, pairs) and both entries and deletion records, each with its flags and commit serial, and the manifest's form the previous base token. The decision and view types are declared in full above, because no file holds them.

## Script-facing API

The `Online` Luau namespace, registered by the module at `Start` through `Zenith_ModuleStartContext::RegisterScriptBindings` (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types"). Every function is a `SIDE_EFFECT_KIND_ASYNC_REQUEST` binding of source kind `ASYNC_SOURCE_ONLINE` (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; ONLINE writes no numeric value of its own), callable from behaviours only (function context `BEHAVIOUR`; a call from a chunk system raises SCRIPT's context misuse error of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Catalogue"), returning a `Zenith.AsyncTicket` whose `Result()` is decoded by `Zenith_DecodeOnlineResultForScript`. No `Online` function reads backend state synchronously: every answer is a recorded publication.

| Function | Arguments (validated at the call; a violation is the misuse code named) | `ticket:Result()` fields |
|---|---|---|
| `Online.QueryIdentity(ctx)` | — | `status`, `signedIn` (boolean), `backend` (`"Offline"`, `"Steam"`, `"GooglePlay"`, `"None"`) |
| `Online.RequestSignIn(ctx, interactive)` | `interactive` boolean, default false | `status`, `signedIn`, `backend` |
| `Online.QueryEntitlement(ctx, id)` | `id` string matching `^[a-z][a-z0-9_.]{0,63}$` (ZS2901) | `status`, `state` (`"granted"`, `"denied"`, `"unknown"`) |
| `Online.UnlockAchievement(ctx, id)` | `id` as above (ZS2901) | `status`, `alreadyUnlocked` (boolean) |
| `Online.SetAchievementSteps(ctx, id, steps)` | `id` (ZS2901); `steps` an integer 0 … 1,000,000 (ZS2902) | `status`, `steps`, `total`, `unlocked` |
| `Online.IncrementAchievement(ctx, id, delta)` | `id` (ZS2901); `delta` an integer 1 … 65,535 (ZS2903) | `status`, `steps`, `total`, `unlocked` |
| `Online.QueryAchievement(ctx, id)` | `id` (ZS2901) | `status`, `steps`, `total`, `unlocked` |
| `Online.QueryCloudSlot(ctx, slot)` | `slot` a string passing `Zenith_ParseSaveSlotName` for any kind (ZS2904) | `status`, `state` (the slot state names below), `conflict` (the reason names below, or nil), `token` (the conflict token as 82 lower-case hexadecimal digits, or nil) |
| `Online.ResolveCloudConflict(ctx, slot, choice, token)` | `slot` (ZS2904); `choice` one of `"local"`, `"cloud"`, `"both"`, `"defer"` (ZS2905); `token` the `Token` of the `Online.CloudConflict` event or of `QueryCloudSlot` (ZS2906; ignored for `"defer"`, which may pass nil) | `status` (`"stale"` when the token no longer matches: nothing was done and a new `Online.CloudConflict` follows), `applied` (the choice string), `newSlot` (string or nil) |
| `Online.SyncCloudNow(ctx)` | — | `status`, `conflicts` (number), `transfers` (number) |
| `Online.RepairCloudIndex(ctx)` | — | `status` (`"refused"` unless the cloud index is unreadable, `Zenith2/Docs/Design/ONLINE/CloudSave.md` → "Index repair"), `slots` (number) |

- A request from a world whose previous-profile flag is set answers `"accountChanged"`, and one recorded while the session was fenced answers `"accountChanging"` (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Request execution").
- `status` is the `Zenith_ToString` spelling of `Zenith_OnlineStatus`: `"ok"`, `"notSignedIn"`, `"accountChanged"`, `"accountChanging"`, `"unknownId"`, `"notSupported"`, `"unavailable"`, `"timeout"`, `"refused"`, `"backendError"`, `"abandoned"`, `"cloudDisabled"`, `"notInConflict"`, `"shuttingDown"`, `"stale"`; the backend-only statuses never reach a script (the session maps them: `cancelled` → `"accountChanged"` when a fence cancelled it, else `"backendError"`; `revisionMismatch`, `notFound`, `quota`, `tooLarge` → `"backendError"` with the cloud slot's state carrying the detail).
- Slot state names: `"inSync"`, `"localAhead"`, `"cloudAhead"`, `"uploading"`, `"downloading"`, `"deletingCloud"`, `"deletingLocal"`, `"conflict"`, `"resolving"`, `"skippedVersion"`, `"cloudNewer"`, `"tooLarge"`, `"error"`, `"notSynced"`, `"cloudUnknown"`. Conflict reason names: `"firstSync"`, `"bothChanged"`, `"deletedLocallyChangedInCloud"`, `"changedLocallyDeletedInCloud"`.
- A result with a zero-length payload (a request abandoned by a save restore, `Zenith2/Docs/Design/SAVE/Formats.md` → "Async section") decodes to `{ status = "abandoned" }`.
- Every field set is at most four keys, within `uSCRIPT_ASYNC_RESULT_MAX_FIELDS` (8, `Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header").
- The generated `.d.luau` declares the namespace from the binding table like every other module's (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types").

The reconcile pattern for achievements (achievements earned while signed out are not queued, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Achievement operations"): a game re-asserts the achievements its own save state records when `Online.IdentityChanged` reports `SignedIn`, because unlocking is idempotent; the Game2D and OpenWorldSlice samples do this (`Zenith2/Docs/Design/ONLINE.md` → "Flagship slice integration").

## Event types

Registered by `Zenith_Module_Online_RegisterTypes` as `TYPE_KIND_EVENT` C++-registered structs (the engine-component ruling of `Zenith2/Docs/Architecture/DataDefinedComponents.md` → "Settled rules and ownership" applies to engine event types alike), emitted by the phase-1 system `Online.Events@W<tag>` through `Events(0)` of its system context (`Zenith2/Docs/Design/ECS/SystemAPI.md` → "Zenith_SystemContext header") in completion-ID order, and delivered in phase 9 of that tick. No event carries an account identifier, an account key or a raw native ID.

| Stable name | Luau name | Fields (reflected property IDs 1 … n in this order) |
|---|---|---|
| `Zenith.Online.IdentityChanged` | `Online.IdentityChanged` | `SignedIn` (bool), `Reason` (`"SignedIn"`, `"SignedOut"`, `"Switched"`, `"SignInFailed"`, `"Attached"`: the identity a newly attached world finds, `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Bind at start"), `Backend` (string), `PreviousProfileSession` (bool: this world was bound to the save profile of the previous identity, so its saves are refused until the game returns to a world bound under the new one), `DisplayName` (string; empty unless `online.exposeDisplayName`, and always empty in a process recording a replay) |
| `Zenith.Online.EntitlementsChanged` | `Online.EntitlementsChanged` | `Revision` (number), `GrantedCount` (number) |
| `Zenith.Online.CloudConflict` | `Online.CloudConflict` | `Slot` (string), `Kind` (`"Manual"`, `"Quick"`, `"Autosave"`, `"Checkpoint"`), `Reason` (the conflict reason names), `LocalUtcSeconds`, `CloudUtcSeconds`, `LocalPlaytimeSeconds`, `CloudPlaytimeSeconds` (numbers), `CloudDisplayName` (the cloud copy's save display name, a game-authored string), `CloudFromThisDevice` (bool), `Token` (the conflict token as 82 lower-case hexadecimal digits, passed back to `Online.ResolveCloudConflict`) |
| `Zenith.Online.CloudSlotReplaced` | `Online.CloudSlotReplaced` | `Slot` (string), `Kind`, `Cause` (`"Download"`, `"KeepCloud"`, `"KeepBoth"`, `"DeletedInCloud"`), `NewSlot` (string; empty unless `KeepBoth`) |
| `Zenith.Online.CloudStatus` | `Online.CloudStatus` | `Status` (`"Available"`, `"Unavailable"`, `"Disabled"`, `"QuotaExceeded"`, `"Degraded"`, `"IndexUnreadable"`, `"ReadOnly"`), `Reason` (`"Network"`, `"UserSetting"`, `"AppSetting"`, `"Quota"`, `"ProfileSwitchFailed"`, `"IndexInvalid"`, `"IndexForeign"`, `"IndexMissing"`, `"IndexNewer"`, `"None"`) |

String fields are interned at emission (`Zenith_StringTable::Intern`); a full intern table drops the event and counts `online.events.intern_failed`, the rule UI's emitter follows (`Zenith2/Docs/Design/UI/Interaction.md` → "UI events to simulation").

## Configuration keys

Registered from `Zenith_Module_Online_RegisterTypes` into the schema `Zenith_TypeRegistry::GetConfigSchema()` exposes (`Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"), with PLAT's key grammar (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Key grammar"). None is simulation-affecting (online results reach simulation only as recorded publications), so none is written into replay headers. None holds a credential, and no key's name contains `token`, `secret`, `password`, `ticket`, `credential` or `receipt` (`Test_OnlineModule_ConfigKeysHoldNoCredential`).

| Key | Type, range, default | Layers | Meaning |
|---|---|---|---|
| `online.backend` | string, `""` | every layer | the name of a linked backend to try first; `""`: the order the composition passes (store backend first, `Offline` last, `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend selection"); a name not among the passed entries is a logged warning and the composition's order applies |
| `online.catalogue` | string, `""` | game, command line | the online catalogue asset's persistent ID (36-character form); `""`: an empty catalogue |
| `online.requestTimeoutMs` | u32 1,000 … 120,000, 15,000 | every layer | per script request, from intake (the time `SubmitScriptRequest` recorded in the inbox record) to result |
| `online.exposeDisplayName` | bool, false | game, command line | publish the display name, which is personal data, in `Online.IdentityChanged`; ignored (with a log line) in a process that records a replay, so a display name never enters a replay log (`Zenith2/Docs/Design/ONLINE/Accounts.md` → "Display name exposure") |
| `online.cloud.enabled` | bool, true | game, user, command line | the cloud-save engine runs |
| `online.cloud.uploadDelayMs` | u32 0 … 60,000, 2,000 | every layer | debounce between a local slot write and its upload |
| `online.cloud.pollSeconds` | u32 0 … 86,400, 0 | every layer | period of an unprompted cloud listing; 0 never |
| `online.cloud.maxTransferBytes` | u64 1,048,576 … `ulONLINE_MAX_CLOUD_OBJECT_BYTES`, 67,108,864 | game, command line | the largest envelope moved |
| `online.cloud.shutdownFlushMs` | u32 0 … 10,000, 3,000 | game, command line | the shutdown upload flush |
| `online.cloud.retryBackoffMs` | u32 1,000 … 480,000, 30,000 | game, command line | the first backoff after a failed transfer |
| `online.rebind.maxSaveAttempts` | u32 1 … 36,000, 600 | game, command line | non-busy profile-switch failures before cloud is disabled for the session |
| `online.offline.accounts` | string list, 1 … 16 names of `^[a-z0-9_-]{1,32}$`, `["local"]` | game, command line | the offline backend's accounts |
| `online.offline.activeAccount` | string, `""` (the first account) | game, user, command line | the account signed in at start |
| `online.offline.autoSignIn` | bool, true | game, command line | sign in silently at start |
| `online.offline.grantedEntitlements` | string list of entitlement IDs, `[]` | game, command line | granted by the offline backend, beside the catalogue's `offline.granted` |
| `online.offline.cloudRoot` | string, `""` or one path component of `[a-z0-9_-]`, 1 … 32 bytes | game, command line | the offline cloud store `user:/saves/online/cloud/<cloudRoot>/` (`Zenith2/Docs/Design/ONLINE/Formats.md` → "Offline cloud store"); `""` gives the offline backend no cloud capability |
| `online.offline.latencyFrames` | u32 0 … 600, 0 | command line | frames before an offline completion is delivered |
| `online.steam.appId` | u32, 0 | game, command line | the Steam app ID; 0 lets the SDK read `steam_appid.txt` (development builds only) |
| `online.steam.restartIfNecessary` | bool, false | game | call `SteamAPI_RestartAppIfNecessary` before init (Shipping builds launched outside Steam restart through Steam) |
| `online.steam.scriptedApi` | string (a host path), `""` | command line, non-Shipping only | select the scripted Steam API seam |
| `online.googlePlay.interactiveSignInOnStart` | bool, false | game | show Play's sign-in UI at start when silent sign-in fails |
| `online.googlePlay.scriptedBridge` | string (a file read through PLAT's file I/O: a host path on Windows, a file under the test APK's generated-input directory on a device, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files"), `""` | command line, non-Shipping only | select the scripted Play bridge seam |

## Misuse codes

ONLINE holds the block ZS2900–ZS2999 (orchestrator ruling MISUSE-BLOCKS-2; the reservation row is the core-doc item that adds it to `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges"). Every code is kind `MISUSE`, raised by the binding thunk before any side effect is recorded, so a misuse never consumes a completion ID or capacity.

| Code | Text | Raised when |
|---|---|---|
| ZS2901 | `Online: '<Id>' is not a valid online identifier (expected ^[a-z][a-z0-9_.]{0,63}$)` | an achievement or entitlement ID violating the grammar |
| ZS2902 | `Online: steps <Value> out of range 0..1000000` | `SetAchievementSteps` with a non-integer, negative or larger value |
| ZS2903 | `Online: increment <Value> out of range 1..65535` | `IncrementAchievement` outside the range or non-integer |
| ZS2904 | `Online: '<Slot>' is not a valid save slot name` | a slot name `Zenith_ParseSaveSlotName` refuses |
| ZS2905 | `Online: '<Choice>' is not a conflict choice (expected local, cloud, both or defer)` | `ResolveCloudConflict` with another string |
| ZS2906 | `Online: '<Token>' is not a conflict token (expected 82 lower-case hexadecimal digits)` | `ResolveCloudConflict` with a choice other than `"defer"` and a token that is not 82 lower-case hexadecimal digits, or whose decoded kinds byte fails rule T1 |

ZS2907 … ZS2999 are unassigned. A missing argument or a wrong Luau type is SCRIPT's marshalling error (ZS1511), not an ONLINE code.
