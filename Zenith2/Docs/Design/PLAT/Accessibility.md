# PLAT design: the platform accessibility service

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies `Zenith_AccessibilityService`, the platform half of the screen-reader and text-to-speech hooks: the PLAT-owned value types a semantic tree is published in, the `Zenith_AccessibilityTreeProvider` interface UI implements and PLAT calls, screen-reader detection, the Windows UI Automation fragment provider, the Android `AccessibilityNodeProvider` reached through JNI, announcements, built-in speech through SAPI and Android `TextToSpeech`, the OS accessibility preferences and the configuration layer they enter, OS-requested actions, and the recording implementation every test host selects by configuration together with its declared test-access header. Every obligation here is traced to a `Z2-R-PLAT-NNN` requirement of `Zenith2/Docs/Requirements/PLAT.md` (Z2-R-PLAT-149 to Z2-R-PLAT-167).

## Scope of this file

| Topic | Heading here |
|---|---|
| what PLAT owns and what UI owns | "Ownership split with UI" |
| the header, its types and the service class | "Zenith_Accessibility API" |
| which implementation runs, creation and destruction | "Backend selection" |
| copying and validating a provider's tree, the snapshot OS threads read | "The published snapshot" |
| is a screen reader running | "Screen-reader detection" |
| Windows | "Windows UI Automation provider" |
| Android | "Android node provider" |
| speech to an active screen reader, built-in speech | "Announcements", "Built-in text to speech", "Focus speech hold" |
| activate, increment, focus and the rest, requested by the OS | "OS-requested actions" |
| reduced motion, high contrast, text scale | "OS accessibility preferences" |
| tests and the seam they read | "The recording implementation", "Zenith_AccessibilityTestAccess API" |
| threads, the lock, shutdown | "Accessibility threading and shutdown" |
| counters, logs, failures, tests, device runs | "Accessibility diagnostics", "Accessibility failure handling", "Accessibility tests", "Accessibility device runs" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

## Ownership split with UI

The OS integration is PLAT's; the semantic content is UI's. The dependency points down: ZenithUI depends on ZenithPlatform, implements the PLAT-declared `Zenith_AccessibilityTreeProvider`, and calls the service; ZenithPlatform never names a ZenithUI type (`Zenith2/Docs/Contracts.md` → "Layering DAG", "Callback direction").

| Piece | Owner | Where |
|---|---|---|
| building the semantic tree from screens (roles, names, values, live regions), narration composition, live-region debouncing, the accessibility settings and their keys, the engine settings screen, the accessible flows | UI | `Zenith2/Docs/Design/UI/Accessibility.md` → "Semantic tree", "Narration", "Accessibility settings", "End-to-end accessible flows" |
| `Zenith_AccessibilityTreeProvider` over UI's tree, its registration at module start, turning drained OS actions into navigation input | UI | `Zenith2/Docs/Design/UI/Accessibility.md` → "Platform accessibility bridge" |
| `Zenith_AccessibilityService`, its value types, the snapshot, every OS API (UI Automation, `AccessibilityNodeProvider`, `AccessibilityManager`, SAPI, `TextToSpeech`, `SystemParametersInfoW`, the Android settings), the OS preferences in the configuration store, the recording implementation and its test access | PLAT | this file |
| the Java class of the Android bridge, its packaging and keep rule, the activity's host-view accessor | MOD (the Player's Gradle template), against the contract of § "Java bridge contract" | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring" |

The value types mirror UI's semantic node one to one (`Zenith2/Docs/Design/UI/ServicesAPI.md` → "Zenith_UIAccessibility header"): the role and state enumerators are listed in the same order as `Zenith_UIRole` and `Zenith_UIA11yStateFlags`, and the first five action bits are UI's `m_uActions` bits, so UI's conversion is a table per enumerator and a copy of bits. PLAT adds one action UI does not yet emit, `ACCESSIBILITY_ACTION_SET_VALUE`, which a node offers only when its action bits advertise it.

## Zenith_Accessibility API

### Accessibility value types

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Accessibility.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_Result.h"

inline constexpr uint32 uACCESSIBILITY_NO_NODE = 0xFFFFFFFFu;
inline constexpr uint32 uACCESSIBILITY_MAX_NODES = 4096;
inline constexpr uint32 uACCESSIBILITY_MAX_POOL_BYTES = 262144;      // 256 KiB of UTF-8 per tree
inline constexpr uint32 uACCESSIBILITY_MAX_TEXT_BYTES = 512;         // one name, value, description, announcement or utterance
inline constexpr uint32 uACCESSIBILITY_ACTION_QUEUE_CAPACITY = 64;
inline constexpr uint32 uACCESSIBILITY_MAX_OS_EVENTS_PER_PUBLISH = 64;
inline constexpr uint32 uACCESSIBILITY_FOCUS_SPEECH_HOLD_MS = 600;

/// @contract enum Zenith_AccessibilityBackend
/// Ownership: Plain enumeration; the value of platform.accessibility.backend, in the key's name order.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable (configuration files name them).
/// @endcontract
enum Zenith_AccessibilityBackend : uint8
{
	ACCESSIBILITY_BACKEND_AUTO = 0,        // OS when windowed on Windows or Android, NONE otherwise
	ACCESSIBILITY_BACKEND_OS,              // UI Automation and SAPI, or AccessibilityNodeProvider and TextToSpeech
	ACCESSIBILITY_BACKEND_RECORDING,       // the recording implementation; every test host
	ACCESSIBILITY_BACKEND_NONE,            // no service: Zenith_Platform::GetAccessibility() returns null
};

/// @contract enum Zenith_AccessibilityRole
/// Ownership: Plain enumeration; one enumerator per Zenith_UIRole, in the same order.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value at or above ACCESSIBILITY_ROLE_COUNT fails tree validation (rule V4).
/// Version: since Z2 Stage 4; values stable, new roles are appended before the count.
/// @endcontract
enum Zenith_AccessibilityRole : uint8
{
	ACCESSIBILITY_ROLE_NONE = 0, ACCESSIBILITY_ROLE_GROUP, ACCESSIBILITY_ROLE_TEXT, ACCESSIBILITY_ROLE_HEADING,
	ACCESSIBILITY_ROLE_IMAGE, ACCESSIBILITY_ROLE_BUTTON, ACCESSIBILITY_ROLE_CHECKBOX, ACCESSIBILITY_ROLE_SLIDER,
	ACCESSIBILITY_ROLE_TEXT_FIELD, ACCESSIBILITY_ROLE_LIST, ACCESSIBILITY_ROLE_LIST_ITEM, ACCESSIBILITY_ROLE_TAB_LIST,
	ACCESSIBILITY_ROLE_TAB, ACCESSIBILITY_ROLE_DIALOG, ACCESSIBILITY_ROLE_PROGRESS, ACCESSIBILITY_ROLE_COMBO_BOX,
	ACCESSIBILITY_ROLE_ALERT, ACCESSIBILITY_ROLE_STATUS, ACCESSIBILITY_ROLE_SCROLL_AREA,
	ACCESSIBILITY_ROLE_COUNT,
};

/// @contract enum Zenith_AccessibilityStateFlags
/// Ownership: Plain enumeration of bits; the bits of Zenith_UIA11yStateFlags at the same positions.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a bit above ACCESSIBILITY_STATE_HIDDEN fails tree validation (rule V4).
/// Version: since Z2 Stage 4; bit positions stable.
/// @endcontract
enum Zenith_AccessibilityStateFlags : uint16
{
	ACCESSIBILITY_STATE_FOCUSED = 1u << 0, ACCESSIBILITY_STATE_FOCUSABLE = 1u << 1, ACCESSIBILITY_STATE_DISABLED = 1u << 2,
	ACCESSIBILITY_STATE_CHECKED = 1u << 3, ACCESSIBILITY_STATE_CHECKABLE = 1u << 4, ACCESSIBILITY_STATE_SELECTED = 1u << 5,
	ACCESSIBILITY_STATE_EXPANDED = 1u << 6, ACCESSIBILITY_STATE_COLLAPSED = 1u << 7, ACCESSIBILITY_STATE_READ_ONLY = 1u << 8,
	ACCESSIBILITY_STATE_PASSWORD = 1u << 9, ACCESSIBILITY_STATE_LIVE_POLITE = 1u << 10,
	ACCESSIBILITY_STATE_LIVE_ASSERTIVE = 1u << 11, ACCESSIBILITY_STATE_HIDDEN = 1u << 12,
};

/// @contract enum Zenith_AccessibilityActionKind
/// Ownership: Plain enumeration; an OS-requested action. Bit (kind − 1) of Zenith_AccessibilityNode::m_uActions
///   advertises it, so the first five bits equal UI's m_uActions bits.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4; values stable.
/// @endcontract
enum Zenith_AccessibilityActionKind : uint8
{
	ACCESSIBILITY_ACTION_ACTIVATE = 1,        // bit 0
	ACCESSIBILITY_ACTION_INCREMENT,           // bit 1
	ACCESSIBILITY_ACTION_DECREMENT,           // bit 2
	ACCESSIBILITY_ACTION_SCROLL_INTO_VIEW,    // bit 3
	ACCESSIBILITY_ACTION_SET_FOCUS,           // bit 4
	ACCESSIBILITY_ACTION_SET_VALUE,           // bit 5; m_dValue carries the requested value
};

/// @contract value Zenith_AccessibilityRect
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a member of Zenith_AccessibilityNode, which its producer fills field by field.
/// Invalidation: None.
/// Units: Pixels of the main window's client area, origin top left (the window's pixel size, Zenith_Window.h).
/// Errors: None; a negative width or height fails tree validation (rule V6).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityRect
{
	sint32 m_iX = 0;
	sint32 m_iY = 0;
	sint32 m_iWidth = 0;
	sint32 m_iHeight = 0;
};

/// @contract value Zenith_AccessibilityText
/// Ownership: Plain value; a byte range of the tree's string pool.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a member of Zenith_AccessibilityNode, which its producer fills field by field.
/// Invalidation: With the pool it indexes.
/// Units: Bytes of UTF-8.
/// Errors: None; a range outside the pool or longer than uACCESSIBILITY_MAX_TEXT_BYTES fails validation (rule V5).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityText
{
	uint32 m_uOffset = 0;
	uint32 m_uBytes = 0;                   // 0: empty
};

/// @contract value Zenith_AccessibilityNode
/// Ownership: Plain value in a node array the publisher owns.
/// Thread affinity: IMMUTABLE. Members non-const: its producer fills each node of the tree array field by field.
/// Invalidation: With its array.
/// Units: m_xBounds in main-window pixels; range values in the control's own units.
/// Errors: None; validation rules V1 to V9 of Zenith2/Docs/Design/PLAT/Accessibility.md "Tree validation rules".
/// Version: since Z2 Stage 4; 96 bytes; members are appended only in reserved space.
/// @endcontract
struct Zenith_AccessibilityNode
{
	uint64 m_ulID = 0;                                  // non-zero, unique in the tree, stable across revisions
	uint32 m_uParent = uACCESSIBILITY_NO_NODE;          // an index below this node's own; NO_NODE for a root
	Zenith_AccessibilityRole m_eRole = ACCESSIBILITY_ROLE_NONE;
	uint8 m_uActions = 0;                               // bit (Zenith_AccessibilityActionKind − 1) per offered action
	uint16 m_uState = 0;                                // Zenith_AccessibilityStateFlags
	Zenith_AccessibilityRect m_xBounds;
	Zenith_AccessibilityText m_xName;
	Zenith_AccessibilityText m_xValue;                   // slider value text, text-field content (bullets for passwords), percent
	Zenith_AccessibilityText m_xDescription;             // the hint
	uint32 m_uPositionInSet = 0;                        // 1-based; 0 when not applicable
	uint32 m_uSetSize = 0;
	uint8 m_uHeadingLevel = 0;                          // 1–6 for ACCESSIBILITY_ROLE_HEADING, 0 otherwise
	uint8 m_auReserved[3] = {};
	double m_dRangeMin = 0.0;
	double m_dRangeMax = 0.0;
	double m_dRangeValue = 0.0;
};
static_assert(sizeof(Zenith_AccessibilityNode) == 96);

/// @contract value Zenith_AccessibilityTreeView
/// Ownership: Plain value; views the provider's node array and pool.
/// Thread affinity: IMMUTABLE. Members non-const: its producer fills it field by field.
/// Invalidation: The views stay valid until the provider call that returned them returns; PLAT copies them before then.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityTreeView
{
	Zenith_Span<const Zenith_AccessibilityNode> m_axNodes;   // depth first, parents before children, frontmost screen first
	Zenith_StringView m_strPool;                             // the UTF-8 bytes every Zenith_AccessibilityText indexes
	uint32 m_uFocused = uACCESSIBILITY_NO_NODE;              // index of the focused node
	uint64 m_ulRevision = 0;                                // equals the revision passed to NotifyTreeChanged
};

/// @contract value Zenith_AccessibilityAction
/// Ownership: Plain value copied out of the service's action queue.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the action queue, reused slot by slot.
/// Invalidation: None.
/// Units: m_ulTimestampNs on the monotonic clock; m_dValue in the target node's range units.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityAction
{
	uint64 m_ulNodeID = 0;
	Zenith_AccessibilityActionKind m_eKind = ACCESSIBILITY_ACTION_ACTIVATE;
	uint8 m_auReserved[7] = {};
	double m_dValue = 0.0;                                   // ACCESSIBILITY_ACTION_SET_VALUE only
	uint64 m_ulRevision = 0;                                // the published revision the OS acted on
	uint64 m_ulTimestampNs = 0;
};

inline constexpr uint8 uACCESSIBILITY_PREFERENCE_REDUCED_MOTION = 1u << 0;
inline constexpr uint8 uACCESSIBILITY_PREFERENCE_HIGH_CONTRAST = 1u << 1;
inline constexpr uint8 uACCESSIBILITY_PREFERENCE_TEXT_SCALE = 1u << 2;

/// @contract value Zenith_AccessibilityPreferences
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: a member of Zenith_AccessibilityState, which each pump overwrites.
/// Invalidation: None.
/// Units: m_fTextScale a multiplier in [1.0, 2.0].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityPreferences
{
	bool m_bReducedMotion = false;
	bool m_bHighContrast = false;
	uint8 m_uReportedMask = 0;          // uACCESSIBILITY_PREFERENCE_* the OS reports; an unreported field keeps its default
	float m_fTextScale = 1.0f;
};

/// @contract value Zenith_AccessibilityState
/// Ownership: Plain value owned by the service; read through GetState.
/// Thread affinity: IMMUTABLE between pumps. Members non-const: each pump overwrites it.
/// Invalidation: Refreshed in Platform.Pump step 2.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AccessibilityState
{
	bool m_bScreenReaderActive = false;  // a screen reader is running (Narrator, TalkBack, or another reader)
	bool m_bTreeRequested = false;       // an OS client wants the tree; true whenever m_bScreenReaderActive is
	bool m_bSpeechAvailable = false;     // built-in speech can speak
	Zenith_AccessibilityPreferences m_xPreferences;
	uint64 m_ulRevision = 0;            // increments when any member changes
};
```

### Zenith_AccessibilityTreeProvider

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Accessibility.h

/// @contract interface Zenith_AccessibilityTreeProvider
/// Ownership: Implemented by a higher library (ZenithUI) and registered with SetTreeProvider; the implementer owns it and
///   unregisters it (SetTreeProvider(nullptr)) before destroying it.
/// Thread affinity: MAIN_THREAD. PLAT calls GetTree only on the main thread, only inside SetTreeProvider and
///   NotifyTreeChanged, and never from an OS thread or a JNI callback.
/// Invalidation: The returned view must stay valid until GetTree's caller returns; PLAT copies it before returning.
/// Units: None.
/// Errors: None; an invalid tree is rejected by the service with ACCESSIBILITY_ERROR_INVALID_TREE.
/// Reentrancy: NONE. GetTree may not call any Zenith_AccessibilityService member.
/// Version: since Z2 Stage 4. Source API.
/// @endcontract
class Zenith_AccessibilityTreeProvider
{
public:
	virtual ~Zenith_AccessibilityTreeProvider() = default;
	virtual Zenith_AccessibilityTreeView GetTree() const = 0;
};
```

### Zenith_AccessibilityService

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Accessibility.h

/// @contract type Zenith_AccessibilityService
/// Ownership: Created by Zenith_Platform::Create (creation step 11) when the resolved backend is OS or RECORDING and
///   owned by the platform; Zenith_Platform::GetAccessibility() returns it, or null when there is none. Destroyed first
///   among the platform's services.
/// Thread affinity: MAIN_THREAD. OS queries from UI Automation's threads and the Android UI thread never call a member:
///   they read the service's published snapshot under uLOCK_LEVEL_ACCESSIBILITY.
/// Invalidation: GetState is refreshed at each Pump. The provider pointer is borrowed until SetTreeProvider(nullptr).
/// Units: Speech rates are multipliers of the voice's normal rate; times in nanoseconds of the monotonic clock.
/// Errors: Zenith_Result<…, Zenith_AccessibilityError>; text is untrusted-input-safe (invalid UTF-8 is an error,
///   never an assert); an OS failure after creation is counted and logged, never fatal.
/// Reentrancy: NONE. Members may not be called from inside the provider's GetTree.
/// Version: since Z2 Stage 4. Source API.
/// @endcontract
class Zenith_AccessibilityService
{
public:
	~Zenith_AccessibilityService();
	Zenith_AccessibilityService(const Zenith_AccessibilityService&) = delete;
	Zenith_AccessibilityService& operator=(const Zenith_AccessibilityService&) = delete;

	Zenith_AccessibilityBackend GetBackend() const;           // OS or RECORDING
	const Zenith_AccessibilityState& GetState() const;
	bool IsScreenReaderActive() const;                        // GetState().m_bScreenReaderActive
	bool IsFocusSpeechPending() const;                        // § "Focus speech hold"

	// Publication.
	Zenith_Result<void, Zenith_AccessibilityError> SetTreeProvider(Zenith_AccessibilityTreeProvider* pxProvider);
	Zenith_Result<void, Zenith_AccessibilityError> NotifyTreeChanged(uint64 ulRevision);
	uint64 GetPublishedRevision() const;                    // 0 before the first publication

	// Speech.
	Zenith_Result<void, Zenith_AccessibilityError> Announce(Zenith_StringView strText, bool bAssertive);
	Zenith_Result<void, Zenith_AccessibilityError> Speak(Zenith_StringView strText, float fRate, bool bInterrupt);
	void StopSpeaking();
	bool IsSpeaking() const;

	// OS-requested actions, oldest first; returns the number written (the rest stay queued).
	uint32 DrainActions(Zenith_Span<Zenith_AccessibilityAction> axOut);

private:
	friend class Zenith_Platform;
	friend class Zenith_AccessibilityTestAccess;
	struct State;
	explicit Zenith_AccessibilityService(State* pxState);
	State* m_pxState = nullptr;
};

constexpr const char* Zenith_ToString(Zenith_AccessibilityBackend eBackend);
constexpr const char* Zenith_ToString(Zenith_AccessibilityRole eRole);
constexpr const char* Zenith_ToString(Zenith_AccessibilityActionKind eKind);
```

`Zenith_Platform.h` gains, beside the other service accessors and desc members of `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc":

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h (additions)
#include "ZenithPlatform/Zenith_Accessibility.h"

// in Zenith_PlatformDesc:
	Zenith_AccessibilityBackend m_eAccessibilityBackend = ACCESSIBILITY_BACKEND_AUTO;   // platform.accessibility.backend (early key)

// in Zenith_Platform:
	Zenith_AccessibilityService* GetAccessibility();            // null when the backend resolved to NONE
```

### Accessibility error enumeration

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Accessibility.h
enum Zenith_AccessibilityError : uint8
{
	ACCESSIBILITY_ERROR_PROVIDER_ALREADY_SET = 1,   // SetTreeProvider while another provider is registered
	ACCESSIBILITY_ERROR_NO_PROVIDER,                // NotifyTreeChanged with no provider
	ACCESSIBILITY_ERROR_INVALID_TREE,               // a rule of "Tree validation rules" failed; the snapshot is unchanged
	ACCESSIBILITY_ERROR_TREE_TOO_LARGE,             // more than uACCESSIBILITY_MAX_NODES nodes or uACCESSIBILITY_MAX_POOL_BYTES
	ACCESSIBILITY_ERROR_STALE_REVISION,             // a revision below the published one
	ACCESSIBILITY_ERROR_BAD_TEXT,                   // empty text, invalid UTF-8 or an embedded U+0000
	ACCESSIBILITY_ERROR_BAD_RATE,                   // a speech rate outside [0.25, 4.0] or not finite
	ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE,         // no voice, no engine, or the engine failed to initialise
	ACCESSIBILITY_ERROR_UNKNOWN_NODE,               // an action (test injection) for a node the snapshot lacks
	ACCESSIBILITY_ERROR_ACTION_NOT_OFFERED,         // an action the node's m_uActions does not advertise
	ACCESSIBILITY_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_AccessibilityError eError);
```

## Backend selection

### Backend table

`platform.accessibility.backend` is an early key (engine default and command line only, restart-required; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Early keys"), so the runtime copies it into `Zenith_PlatformDesc::m_eAccessibilityBackend` before startup step 4, and a test that creates the platform directly sets the desc member.

| Requested | Windows windowed | Android windowed | headless (any platform), the `linux-tsan` host |
|---|---|---|---|
| `auto` (default) | OS | OS | NONE |
| `os` | OS | OS | NONE, with the warning `accessibility: backend 'os' needs a window; no accessibility service` |
| `recording` | RECORDING | RECORDING | RECORDING |
| `none` | NONE | NONE | NONE |

RECORDING is available on every platform and in every configuration, Shipping included, because it calls no OS API and holds nothing a player could reach; it is selected only by the configuration key or the desc, never by a preprocessor switch (`Zenith2/Docs/Contracts.md` → "Test separation"). Integration tests start the runtime with `--set platform.accessibility.backend=recording`; ZenithPlatform's own tests set the desc member.

### Service creation and destruction

<!-- elaboration -->
`Zenith_Platform::Create` gains step 11, fault point `Accessibility`, after step 10 `Services` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc"):

1. Resolve the backend by the table above; NONE creates nothing and the step succeeds.
2. Allocate the service state from the platform's heap: the two empty snapshots (node, pool and index capacities 0; they grow on first publication), the action queue (`Zenith_RingBuffer` of 64, `QUEUE_FULL_POLICY_DROP_OLDEST`), the lock (`Zenith_Mutex` at `uLOCK_LEVEL_ACCESSIBILITY`, short-hold); for RECORDING also the record log (§ "Record log"). An allocation failure is `PLATFORM_ERROR_OUT_OF_MEMORY` and unwinds every earlier step.
3. OS backend, Windows: § "Windows provider lifetime" steps W1 to W4; the speech thread is started (§ "Windows SAPI speech thread"). Android: § "Android bridge lifetime" steps A1 to A4.
4. Read the initial OS state and preferences (§ "Screen-reader detection", § "OS preference sources"), so `GetState()` is meaningful before the first pump and `Zenith_Config::Create` (startup step 6) applies the preferences.

An OS-side failure in step 3 (COM initialisation, the UI Automation root, the Java bridge class missing from the APK, a failed `RegisterNatives`) is not a platform failure: the service is destroyed, `GetAccessibility()` returns null, and one warning `accessibility: OS backend unavailable: <reason>` is logged. Built-in speech failing alone (no SAPI voice, `TextToSpeech` initialisation failing) keeps the service and reports `m_bSpeechAvailable = false`.

Destruction is § "Accessibility shutdown order".

## The published snapshot

PLAT never answers an OS query from the provider's memory. `NotifyTreeChanged` copies the provider's tree into a PLAT-owned snapshot, and every OS query, on any thread, reads the published snapshot under `uLOCK_LEVEL_ACCESSIBILITY`. The provider is therefore called only on the main thread, inside `SetTreeProvider` and `NotifyTreeChanged`, and needs no lock of its own.

### Tree validation rules

Checked in order over the view, before anything is copied; the first failure returns the error, logs `accessibility: tree revision <r> rejected: <rule> at node <index>` once per rule per session, counts `platform.accessibility.trees_rejected`, and leaves the published snapshot unchanged.

| Rule | Check | Error |
|---|---|---|
| V1 | at most `uACCESSIBILITY_MAX_NODES` nodes and `uACCESSIBILITY_MAX_POOL_BYTES` pool bytes | `ACCESSIBILITY_ERROR_TREE_TOO_LARGE` |
| V2 | every `m_ulID` is non-zero and no two nodes share one | `ACCESSIBILITY_ERROR_INVALID_TREE` |
| V3 | `m_uParent` is `uACCESSIBILITY_NO_NODE` or below the node's own index | as above |
| V4 | `m_eRole` below `ACCESSIBILITY_ROLE_COUNT`; no state bit above `ACCESSIBILITY_STATE_HIDDEN`; no action bit above bit 5 | as above |
| V5 | every text range lies inside the pool, is at most `uACCESSIBILITY_MAX_TEXT_BYTES`, is valid UTF-8 (`Zenith_ValidateUtf8`) and holds no U+0000 | as above |
| V6 | bounds width and height are not negative | as above |
| V7 | the three range values are finite and, when `m_dRangeMax > m_dRangeMin`, the value lies in `[min, max]` | as above |
| V8 | `m_uFocused` is `uACCESSIBILITY_NO_NODE` or a valid index; exactly that node, and no other, carries `ACCESSIBILITY_STATE_FOCUSED` | as above |
| V9 | `m_uHeadingLevel` is at most 6; `m_uPositionInSet` is at most `m_uSetSize` | as above |

V2 uses the sorted ID index built for the copy (§ "Publication algorithm"); every other rule is one pass, so validation is O(n log n) for n nodes.

### Publication algorithm

`NotifyTreeChanged(ulRevision)`, main thread:

1. No provider: `ACCESSIBILITY_ERROR_NO_PROVIDER`. `ulRevision` equal to the published revision: return success, nothing done. Below it: `ACCESSIBILITY_ERROR_STALE_REVISION`.
2. `xView = pxProvider->GetTree()`; a view whose `m_ulRevision` differs from `ulRevision` is `ACCESSIBILITY_ERROR_INVALID_TREE`.
3. Validate (V1 to V9).
4. Grow the back snapshot's arrays to fit, doubling and capped at the V1 limits; the back snapshot is never visible to another thread, so growth allocates outside the lock. A failure is `ACCESSIBILITY_ERROR_OUT_OF_MEMORY` with the published snapshot unchanged.
5. Copy the nodes and the pool; build the ID index (pairs of ID and node index sorted by `Zenith_Sort`, deterministic), each node's first child and next sibling (one pass, children in array order), and on Android the virtual view IDs (§ "Virtual view IDs").
6. Diff the back snapshot against the published one (§ "Change detection").
7. Take the lock; swap the published and back snapshots; record the revision and the main window's client-origin offset (Windows; § "Properties"); release the lock.
8. Raise the diff's OS events outside the lock (§ "Events raised", § "Android events", and for RECORDING the records of § "Record log").
9. When the focused node's ID changed and a screen reader is active, start the focus speech hold (§ "Focus speech hold").
10. Publish `platform.accessibility.publishes`, `.nodes`, `.pool_bytes` and `.publish_us`.

`SetTreeProvider(pxProvider)` with a provider while one is set is `ACCESSIBILITY_ERROR_PROVIDER_ALREADY_SET`. With a provider when none is set it stores the pointer and publishes the provider's current tree as `NotifyTreeChanged(GetTree().m_ulRevision)` would, except that a revision of 0 publishes an empty tree; a rejected first tree leaves the provider registered and the published tree empty. `SetTreeProvider(nullptr)` clears the provider, publishes an empty tree at the next revision and raises a structure change, so no OS client keeps nodes of a destroyed UI. The published revision is reset to 0 by each registration.

Cost: O(n log n) in the node count and O(p) in the pool bytes; at the caps, about 1 MiB copied. UI publishes only when its tree's revision changed and a consumer is active, from its claimant callback inside `Platform.Pump`, so the work is inside the `Platform.Pump` zone under the child zone `Platform.Accessibility.Publish`. Memory is at most two snapshots of 4,096 × 96 bytes of nodes, 256 KiB of pool and 64 KiB of index and links each, about 1.3 MiB, allocated on first publication.

### Change detection

The diff walks both ID indexes in order (a linear merge) and produces, in this order:

| Change | Condition | Event kind |
|---|---|---|
| focus | the focused node's ID differs (including to or from none) | focus changed, naming the new node |
| structure | an ID is present in one snapshot only, or a common ID's parent ID or child order differs | one structure change at the root |
| node | for a common ID: name, value, description, state, bounds, range or heading level differ (a bit per field) | node changed, with the field bits |
| live region | a node with `LIVE_POLITE` or `LIVE_ASSERTIVE` whose name or value changed | live region changed |

At most `uACCESSIBILITY_MAX_OS_EVENTS_PER_PUBLISH` (64) node and live-region events are raised for one publication; beyond that they are replaced by one structure change and `platform.accessibility.os_events_capped` is counted. The focus event is always raised.

### Snapshot queries

The OS adapters call internal snapshot readers (`Internal/Accessibility/Zenith_AccessibilitySnapshot.h`), each of which takes the lock, copies what it needs into the caller's stack storage and releases it before the caller makes any OS call:

| Reader | Returns | Cost under the lock |
|---|---|---|
| `CopyNode(ulID, Zenith_AccessibilityNodeCopy&)` | the node and its three texts (three 512-byte buffers), its parent, first child, next and previous sibling IDs | O(log n) |
| `GetRootIDs(Zenith_Span<uint64>)`, `GetChildIDs(ulID, Zenith_Span<uint64>)` | children in order | O(log n + k) |
| `GetFocusedID()` | the focused ID or 0 | O(1) |
| `HitTest(iX, iY)` | § "Hit testing" | O(n) |
| `GetPublishedRevision()` | the revision | O(1) |

No reader allocates, makes a system call, calls back or waits while holding the lock; the lock holder takes no other lock (`Zenith2/Docs/Contracts.md` → "Lock ordering table", short-hold).

### Hit testing

`HitTest(iX, iY)` (main-window pixels) returns the ID of the node whose bounds contain the point and that is deepest in the first root, in array order, containing it; nodes with `ACCESSIBILITY_STATE_HIDDEN` and their subtrees are skipped; 0 when none contains it. Because UI emits the frontmost screen first, the frontmost visible node wins.

## Screen-reader detection

| Platform | `m_bScreenReaderActive` | `m_bTreeRequested` | When it is read |
|---|---|---|---|
| Windows | `SystemParametersInfoW(SPI_GETSCREENREADER)` | the screen-reader flag, or `UiaClientsAreListening()` | at creation; on `WM_SETTINGCHANGE` (the subclass procedure sets a dirty flag the next pump reads); every 2,000 ms in the pump |
| Android | `AccessibilityManager.isTouchExplorationEnabled()` | `AccessibilityManager.isEnabled()` | at creation; on the manager's touch-exploration and accessibility-state listeners, through the native `nativeOnState` (§ "Native entry points") |
| RECORDING | test access `SetScreenReaderActive` | that, or test access `SetTreeRequested` | at the next pump |

Pump step 2 (`Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm") reads the OS cells under the lock, and when any member of `Zenith_AccessibilityState` changed it increments `m_ulRevision` and appends one presentation-only `PLATFORM_EVENT_ACCESSIBILITY_CHANGED` record (`Zenith_PlatformAccessibilityPayload`: the three flags and the bit mask of changed preferences), never world-visible. Detection never changes simulation.

## Windows UI Automation provider

All Windows code is in `Zenith2/Engine/ZenithPlatform/Internal/Windows/`: `Zenith_Accessibility_Windows.cpp` (detection, preferences, the subclass procedure, announcements), `Zenith_UiaProviders_Windows.cpp` (the COM objects) and `Zenith_Speech_Windows.cpp` (SAPI). They name the import libraries `uiautomationcore.lib`, `comctl32.lib`, `ole32.lib` and `oleaut32.lib` with `#pragma comment(lib, …)`; the Windows SDK system libraries are not manifest dependencies.

### Windows provider lifetime

W1. Read the main window's `HWND` (`SDL_PROP_WINDOW_WIN32_HWND_POINTER`).
W2. Allocate the shared block every COM object references (the service state pointer, a reference count, a disconnected flag) from the root heap with `Zenith_New`, and create the root provider object.
W3. `SetWindowSubclass(hwnd, Zenith_AccessibilitySubclassProc, uACCESSIBILITY_SUBCLASS_ID, shared)` on the main thread. The procedure answers `WM_GETOBJECT` with `lParam == UiaRootObjectId` by `UiaReturnRawElementProvider(hwnd, wParam, lParam, root)`, marks the OS state dirty on `WM_SETTINGCHANGE` and passes every message to `DefSubclassProc`. `WM_GETOBJECT` and `WM_SETTINGCHANGE` are sent messages, which SDL's message hook does not see, so a subclass is the only correct place.
W4. Start `Zenith.Speech`.

Every provider is created with `ProviderOptions_ServerSideProvider` and without `ProviderOptions_UseComThreading`, so UI Automation may call it on any thread; each method copies what it needs through a snapshot reader, releases the lock, and only then allocates a `BSTR` or `SAFEARRAY`. Node provider objects are created per query for a node ID, hold the shared block, and return `UIA_E_ELEMENTNOTAVAILABLE` when the ID is no longer in the published snapshot or the block is disconnected. The live object count is the gauge `platform.accessibility.uia_objects`.

### Role to control type table

| Role | `UIA_ControlTypePropertyId` | Extra properties |
|---|---|---|
| NONE | `UIA_CustomControlTypeId` | — |
| GROUP | `UIA_GroupControlTypeId` | — |
| TEXT | `UIA_TextControlTypeId` | — |
| HEADING | `UIA_TextControlTypeId` | `UIA_HeadingLevelPropertyId` = `HeadingLevel1` + level − 1 |
| IMAGE | `UIA_ImageControlTypeId` | — |
| BUTTON | `UIA_ButtonControlTypeId` | — |
| CHECKBOX | `UIA_CheckBoxControlTypeId` | — |
| SLIDER | `UIA_SliderControlTypeId` | — |
| TEXT_FIELD | `UIA_EditControlTypeId` | `UIA_IsPasswordPropertyId` from `PASSWORD` |
| LIST | `UIA_ListControlTypeId` | — |
| LIST_ITEM | `UIA_ListItemControlTypeId` | — |
| TAB_LIST | `UIA_TabControlTypeId` | — |
| TAB | `UIA_TabItemControlTypeId` | — |
| DIALOG | `UIA_WindowControlTypeId` | `UIA_IsDialogPropertyId` = TRUE |
| PROGRESS | `UIA_ProgressBarControlTypeId` | — |
| COMBO_BOX | `UIA_ComboBoxControlTypeId` | — |
| ALERT | `UIA_TextControlTypeId` | `UIA_LiveSettingPropertyId` = `Assertive` |
| STATUS | `UIA_StatusBarControlTypeId` | `UIA_LiveSettingPropertyId` = `Polite` |
| SCROLL_AREA | `UIA_PaneControlTypeId` | — |

### Properties

`UIA_NamePropertyId` (the name), `UIA_HelpTextPropertyId` (the description), `UIA_IsEnabledPropertyId` (not `DISABLED`), `UIA_HasKeyboardFocusPropertyId` (`FOCUSED`), `UIA_IsKeyboardFocusablePropertyId` (`FOCUSABLE`), `UIA_IsOffscreenPropertyId` (bounds outside the client area), `UIA_PositionInSetPropertyId` and `UIA_SizeOfSetPropertyId` (when non-zero), `UIA_LiveSettingPropertyId` (from the live bits for any role), `UIA_AutomationIdPropertyId` (the node ID as 16 lower-case hexadecimal digits), `UIA_FrameworkIdPropertyId` (`L"Zenith2"`). `BoundingRectangle` is the node's bounds plus the client-origin offset (`ClientToScreen(hwnd, {0, 0})`, read by the main thread at each publication and each pump and stored with the snapshot), in physical screen pixels. `GetRuntimeId` returns `{ UiaAppendRuntimeId, low 32 bits, high 32 bits }` of the node ID. Navigation: parent, first and last child, next and previous sibling from the snapshot links; the roots are the root provider's children. `ElementProviderFromPoint` converts the screen point with `ScreenToClient` before taking the lock and returns `HitTest`'s node; `GetFocus` returns the focused node. Hidden nodes are never returned.

### Patterns and actions

| Pattern | Offered when | Method → queued action |
|---|---|---|
| `IInvokeProvider` | `ACTIVATE` advertised and role is not CHECKBOX, TAB or LIST_ITEM | `Invoke` → ACTIVATE |
| `IToggleProvider` | role CHECKBOX, or `CHECKABLE` | `get_ToggleState` from `CHECKED`; `Toggle` → ACTIVATE |
| `ISelectionItemProvider` | role TAB or LIST_ITEM | `get_IsSelected` from `SELECTED`; `Select` → ACTIVATE; `AddToSelection`, `RemoveFromSelection` → `UIA_E_INVALIDOPERATION` |
| `IRangeValueProvider` | role SLIDER or PROGRESS | value, minimum, maximum from the range; `SmallChange` = `LargeChange` = (max − min) / 20; `IsReadOnly` unless SET_VALUE is advertised; `SetValue(v)` → SET_VALUE with `v` |
| `IValueProvider` | role TEXT_FIELD or COMBO_BOX | `get_Value` the value text; `IsReadOnly` TRUE; `SetValue` → `UIA_E_INVALIDOPERATION` (text entry goes through IME sessions) |
| `IScrollItemProvider` | `SCROLL_INTO_VIEW` advertised | `ScrollIntoView` → SCROLL_INTO_VIEW |
| `IRawElementProviderFragment::SetFocus` | `SET_FOCUS` advertised | SET_FOCUS |

Increment and decrement have no UI Automation pattern of their own; a slider advertising them without SET_VALUE is read-only to UI Automation clients and is changed by the screen reader's keyboard commands, which arrive as ordinary key input. A pattern method for an action the node does not advertise returns `UIA_E_INVALIDOPERATION` and counts `platform.accessibility.actions_rejected`.

### Events raised

Only while `UiaClientsAreListening()` returns TRUE, on the main thread, outside the lock (a client may call back into a provider synchronously):

| Diff entry | Call |
|---|---|
| focus changed | `UiaRaiseAutomationEvent(node, UIA_AutomationFocusChangedEventId)` |
| structure | `UiaRaiseStructureChangedEvent(root, StructureChangeType_ChildrenInvalidated, runtimeId, length)` |
| node changed (name) | `UiaRaiseAutomationPropertyChangedEvent(node, UIA_NamePropertyId, old, new)` |
| node changed (value, range) | the same with `UIA_ValueValuePropertyId` or `UIA_RangeValueValuePropertyId` |
| node changed (`CHECKED`) | the same with `UIA_ToggleToggleStatePropertyId` |
| live region changed | `UiaRaiseAutomationEvent(node, UIA_LiveRegionChangedEventId)` |

Every call counts `platform.accessibility.os_events`.

## Android node provider

The native side is `Zenith2/Engine/ZenithPlatform/Internal/Android/Zenith_Accessibility_Android.cpp`; the platform-neutral virtual-tree code the natives wrap is `Internal/Accessibility/Zenith_AccessibilityVirtualTree.cpp`, so every host's tests exercise it. The Java class is MOD's file in the Player's Gradle template; its contract is § "Java bridge contract".

### Virtual view IDs

Android identifies a virtual view by a positive `int`. The service keeps a mapping from node ID to virtual ID, sorted by node ID and rebuilt at each publication by merging the previous mapping with the new ID index: a node present in both keeps its virtual ID, a new node takes the next value of a counter that starts at 1 and never repeats within the process, and a removed node's ID is dropped. `View.NO_ID` and `AccessibilityNodeProvider.HOST_VIEW_ID` (both −1) are never assigned. The counter's wrap at 2³¹ − 1 restarts it at 1 with a structure change (a session would need two billion new nodes).

### Java bridge contract

The class `org.zenith2.player.Zenith2Accessibility` in `Zenith2/Player/GradleTemplate/app/src/main/java/org/zenith2/player/Zenith2Accessibility.java` (MOD's template; row P20 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`) has exactly these members, which `Internal/Accessibility/Zenith_AccessibilityJniTable.h` lists as the strings the native side looks up and registers:

| Member | JNI signature | Called by, on | Does |
|---|---|---|---|
| constructor `(Activity xActivity, long lNative)` | `(Landroid/app/Activity;J)V` | native, the SDL main thread | stores both; no Android call |
| `void attach()` | `()V` | native, main thread | posts to the UI thread: sets an `AccessibilityDelegate` on `((Zenith2Activity) xActivity).getAccessibilityHostView()` whose `getAccessibilityNodeProvider` returns the bridge's provider, an `OnHoverListener` for touch exploration, the `AccessibilityManager` state and touch-exploration listeners, a `ContentObserver` on `Settings.Global.ANIMATOR_DURATION_SCALE`, a `UiModeManager` contrast listener, `ComponentCallbacks` for the font scale, and creates the `TextToSpeech`; then reports state, preferences and speech state through the natives |
| `void detach()` | `()V` | native, main thread | `synchronized`: sets the stored native value to 0, then posts to the UI thread the removal of everything `attach` installed and `TextToSpeech.shutdown()`; after it returns no native method is entered again |
| `void notifyTreeChanged(int iFocusedId, int[] aiChanged, boolean bStructure)` | `(I[IZ)V` | native, main thread | posts to the UI thread: `TYPE_WINDOW_CONTENT_CHANGED` with `CONTENT_CHANGE_TYPE_SUBTREE` on the host when `bStructure`; one `TYPE_WINDOW_CONTENT_CHANGED` per changed virtual view; `TYPE_VIEW_FOCUSED` for `iFocusedId` when it is not −1 |
| `void announce(String strText, boolean bAssertive)` | `(Ljava/lang/String;Z)V` | native, main thread | posts: `AccessibilityManager.interrupt()` when assertive, then `View.announceForAccessibility(strText)` on the host view |
| `boolean speak(String strText, float fRate, boolean bInterrupt)` | `(Ljava/lang/String;FZ)Z` | native, main thread | `setSpeechRate(fRate)` and `speak(strText, bInterrupt ? QUEUE_FLUSH : QUEUE_ADD, null, id)`; before initialisation completes, keeps at most 32 pending utterances; false when the engine failed |
| `void stopSpeaking()` | `()V` | native, main thread | `TextToSpeech.stop()` and clears pending utterances |
| `static native void nativeOnState(long, boolean bTouchExploration, boolean bEnabled)` | `(JZZ)V` | Java, UI thread | § "Native entry points" |
| `static native void nativeOnPreferences(long, float fAnimatorScale, float fFontScale, float fContrast)` | `(JFFF)V` | Java, UI thread | as above |
| `static native void nativeOnSpeechState(long, int iState)` | `(JI)V` | Java, any thread (`UtteranceProgressListener`, `OnInitListener`) | 0 unavailable, 1 idle, 2 speaking |
| `static native int[] nativeGetChildren(long, int iVirtualId)` | `(JI)[I` | Java, UI thread | children of a virtual view; −1 is the host (the roots) |
| `static native String[] nativeQueryNode(long, int iVirtualId, int[] aiInts, double[] adRange)` | `(JI[I[D)[Ljava/lang/String;` | Java, UI thread | fills `aiInts` (12 entries: parent virtual ID or −1, role, state, actions, x, y, width, height, position in set, set size, heading level, virtual ID of the focused node) and `adRange` (minimum, maximum, value); returns name, value and description, or null for an unknown ID |
| `static native int nativeHitTest(long, float fX, float fY)` | `(JFF)I` | Java, UI thread | the virtual ID of § "Hit testing", or −1 |
| `static native boolean nativePerformAction(long, int iVirtualId, int iAction, double dValue)` | `(JIID)Z` | Java, UI thread | § "OS-requested actions" |

Every Java method that calls a native is `synchronized` on the bridge and reads the stored native value inside the monitor, returning at once when it is 0, so `detach` and a callback cannot interleave; a native holds `uLOCK_LEVEL_ACCESSIBILITY` only inside the call, and the main thread never holds that lock while calling Java, so the monitor and the lock cannot deadlock. The provider builds each `AccessibilityNodeInfo` from `nativeQueryNode`'s result: class name by role (§ "Node information mapping"), `setBoundsInScreen` from the bounds plus the host view's `getLocationOnScreen`, `setParent` and `addChild` from the virtual IDs, the actions of § "OS-requested actions", and `setAccessibilityFocused` from the virtual ID the bridge itself tracks. The Java file is `-keep`'d whole by the template's `proguard-rules.pro` (MOD).

### Native entry points

Owner pointers travel as the `long` (Z2-D-046: no static state); every entry point casts it to the service state, takes the lock only to copy or store, and returns:

- `nativeOnState` and `nativeOnPreferences` write the OS cells read at the next pump step 2; preference values are normalised by § "OS preference sources".
- `nativeOnSpeechState` writes the speech cell; `IsSpeaking` and `m_bSpeechAvailable` read it at the next pump.
- `nativeGetChildren`, `nativeQueryNode`, `nativeHitTest` call the virtual-tree code over the snapshot readers; strings are converted with `Zenith_Utf8ToUtf16` into a 512-unit stack buffer and created with `NewString`, never `NewStringUTF`, whose modified UTF-8 mangles characters outside the Basic Multilingual Plane.
- `nativePerformAction` maps the action (§ "OS-requested actions") and pushes it to the action queue.

`Zenith_AccessibilityVirtualTree.cpp` exposes the same three queries as C++ functions over plain arrays (`Zenith_AccessibilityQueryVirtualNode`, `Zenith_AccessibilityGetVirtualChildren`, `Zenith_AccessibilityHitTestVirtual`), which the JNI layer wraps and the tests call directly.

### Node information mapping

| Role | `setClassName` | Extra |
|---|---|---|
| NONE, GROUP | `android.view.ViewGroup` | — |
| TEXT, ALERT, STATUS | `android.widget.TextView` | `setText(name)`; live region from the live bits (`ACCESSIBILITY_LIVE_REGION_POLITE` or `_ASSERTIVE`); ALERT is assertive, STATUS polite |
| HEADING | `android.widget.TextView` | `setText(name)`, `setHeading(true)` |
| IMAGE | `android.widget.ImageView` | `setContentDescription(name)` |
| BUTTON | `android.widget.Button` | `setContentDescription(name)`, `setClickable` |
| CHECKBOX | `android.widget.CheckBox` | `setCheckable(true)`, `setChecked` from `CHECKED` |
| SLIDER, PROGRESS | `android.widget.SeekBar`, `android.widget.ProgressBar` | `setRangeInfo(new RangeInfo(RANGE_TYPE_FLOAT, min, max, value))`, `setStateDescription(value text)` |
| TEXT_FIELD | `android.widget.EditText` | `setText(value)`, `setHintText(name)`, `setPassword` |
| LIST | `android.widget.AbsListView` | `setCollectionInfo(new CollectionInfo(setSize, 1, false))` |
| LIST_ITEM | `android.view.View` | `setCollectionItemInfo(new CollectionItemInfo(position − 1, 1, 0, 1, false, selected))` |
| TAB_LIST | `android.widget.TabWidget` | — |
| TAB | `android.view.View` | `setSelected` from `SELECTED`, `setClickable` |
| DIALOG | `android.view.ViewGroup` | `setPaneTitle(name)` |
| COMBO_BOX | `android.widget.Spinner` | `setStateDescription(value text)` |
| SCROLL_AREA | `android.widget.ScrollView` | `setScrollable(true)` |

Every node also gets `setEnabled` (not `DISABLED`), `setFocusable` (`FOCUSABLE`), `setFocused` (`FOCUSED`), `setHintText(description)` when the role does not use the hint for its name, and `setVisibleToUser` (bounds intersect the host view); hidden nodes are never returned. The table index is the role value, which is the `m_uOsRoleCode` the tests compare.

### Android events

After a publication, the native side calls `notifyTreeChanged` with the focused node's virtual ID (−1 when none), the virtual IDs of the node and live-region events (at most 64), and the structure flag; every event counts `platform.accessibility.os_events`.

### Android bridge lifetime

A1. `SDL_GetAndroidJNIEnv()` (the main thread is attached by SDL) and `SDL_GetAndroidActivity()` as a global reference.
A2. Load the class through the activity's class loader (`getClassLoader().loadClass("org.zenith2.player.Zenith2Accessibility")`, because `FindClass` on a native thread sees only system classes) and `RegisterNatives` with the table of `Zenith_AccessibilityJniTable.h`.
A3. Construct the bridge with the service state's address as the `long`; keep a global reference.
A4. Call `attach()`; the initial state, preferences and speech state arrive through the natives before the first pump reads them (or at a subsequent pump, with the defaults until then).

A missing class or a failed registration is § "Service creation and destruction"'s OS-side failure.

## Announcements

`Announce(strText, bAssertive)` validates the text (non-empty, valid UTF-8, no U+0000; `ACCESSIBILITY_ERROR_BAD_TEXT`), cuts it at the last UTF-8 boundary at or before 512 bytes (counting `platform.accessibility.text_truncated`), counts `platform.accessibility.announcements`, and delivers it whether or not a screen reader is running (the OS discards it when none listens):

| Backend | Delivery |
|---|---|
| Windows | `UiaRaiseNotificationEvent(root, NotificationKind_Other, bAssertive ? NotificationProcessing_ImportantMostRecent : NotificationProcessing_All, text, L"Zenith2.Announcement")`, main thread, when `UiaClientsAreListening()` |
| Android | the bridge's `announce` |
| RECORDING | an `ACCESSIBILITY_RECORD_ANNOUNCEMENT` record |

Announcements never go to built-in speech; UI chooses between a screen reader and built-in narration (`Zenith2/Docs/Design/UI/Accessibility.md` → "Platform accessibility bridge").

## Built-in text to speech

`Speak(strText, fRate, bInterrupt)` validates the text as `Announce` does and the rate (finite, 0.25–4.0; `ACCESSIBILITY_ERROR_BAD_RATE`), returns `ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE` when `m_bSpeechAvailable` is false, counts `platform.accessibility.utterances`, and queues the utterance; `bInterrupt` discards the queued and current utterances first. `StopSpeaking()` discards both. `IsSpeaking()` is true from a successful `Speak` until the engine reports the queue empty. When the application lifecycle enters `DID_ENTER_BACKGROUND`, pump step 2 calls `StopSpeaking()`, so a backgrounded game never speaks.

### Windows SAPI speech thread

`Zenith.Speech` (`Internal/Windows/Zenith_Speech_Windows.cpp`, over the platform-neutral queue of `Internal/Accessibility/Zenith_SpeechQueue.cpp`) owns the voice:

1. At start: `CoInitializeEx(nullptr, COINIT_MULTITHREADED)`, `CoCreateInstance(CLSID_SpVoice)`; a failure sets the speech cell to unavailable, logs once and the thread waits only for its quit command.
2. Loop: wait on the queue's `Zenith_Semaphore` with a 100 ms timeout; pop one command under the lock; for SPEAK, `SetRate(Zenith_AccessibilitySapiRate(fRate))` then `Speak(text, SPF_ASYNC | SPF_IS_NOT_XML | (interrupt ? SPF_PURGEBEFORESPEAK : 0), nullptr)` with the text converted by `Zenith_Utf8ToUtf16`; for STOP, `Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr)`; after every command and every timeout, `GetStatus` updates the speaking cell (`SPRS_IS_SPEAKING`).
3. QUIT: purge, release the voice, `CoUninitialize`, exit.

The command queue holds 32 commands with their text in 32 slots of 512 bytes (`Zenith_RingBuffer`, `QUEUE_FULL_POLICY_DROP_OLDEST`; counters `platform.accessibility.speech_queue.full`, `.dropped`, `.high_water`); an interrupting SPEAK or a STOP clears the queue before it is pushed. The queue's thread loop runs over the internal interface `Zenith_SpeechSink` (`Open`, `Speak`, `Stop`, `IsSpeaking`, `Close`), which the SAPI file implements and the tests implement in memory, so the queue's ordering, purge and overflow are tested on every host.

### Android TextToSpeech

The native side calls the bridge's `speak` and `stopSpeaking` on the main thread; Android's engine queues and speaks on its own threads, and `nativeOnSpeechState` reports state changes. A `speak` returning false sets the speech cell unavailable at the next pump.

### Rate mapping

`fRate` is a multiplier of the voice's normal rate (UI's `ui.access.narrationRate`, 0.5–2.0). SAPI's rate is an integer from −10 to 10 in which each step is a factor of 3^(1/10), so `Zenith_AccessibilitySapiRate(fRate) = clamp(round(10 × ln(fRate) / ln(3)), −10, 10)` (0.5 → −6, 1.0 → 0, 2.0 → 6). Android's `setSpeechRate` takes the multiplier unchanged. RECORDING records the multiplier.

## Focus speech hold

When a publication changes the focused node while a screen reader is active, a screen reader speaks the new focus by itself; UI suppresses its own live regions and announcements meanwhile (`Zenith2/Docs/Design/UI/Accessibility.md` → "Live regions"). `IsFocusSpeechPending()` reports that window: true from such a publication until the first `Platform.Pump` whose timestamp is at least `uACCESSIBILITY_FOCUS_SPEECH_HOLD_MS` (600) after that publication, false whenever no screen reader is active. No OS reports when its own speech ends, so the window is a fixed hold; test access `SetFocusSpeechHoldMs` shortens it for tests.

## OS-requested actions

### Action mapping table

| Backend | OS request | Queued action |
|---|---|---|
| Windows | `Invoke`, `Toggle`, `Select` | ACTIVATE |
| Windows | `IRangeValueProvider::SetValue(v)` | SET_VALUE, `v` |
| Windows | `ScrollIntoView` | SCROLL_INTO_VIEW |
| Windows | `IRawElementProviderFragment::SetFocus` | SET_FOCUS |
| Android | `ACTION_CLICK` | ACTIVATE |
| Android | `ACTION_SCROLL_FORWARD`, `ACTION_SCROLL_BACKWARD` | INCREMENT, DECREMENT |
| Android | `ACTION_SET_PROGRESS` (`ACTION_ARGUMENT_PROGRESS_VALUE`) | SET_VALUE, the value |
| Android | `ACTION_SHOW_ON_SCREEN` | SCROLL_INTO_VIEW |
| Android | `ACTION_FOCUS` | SET_FOCUS |
| Android | `ACTION_ACCESSIBILITY_FOCUS` | the bridge records the accessibility-focused virtual view; a node advertising SET_FOCUS also queues SET_FOCUS |
| Android | `ACTION_CLEAR_ACCESSIBILITY_FOCUS` | the bridge clears its record; nothing is queued |
| RECORDING | test access `InjectAction` | the action as given |

Each node offers exactly the OS actions whose queued action it advertises; `ACTION_ACCESSIBILITY_FOCUS` and `ACTION_CLEAR_ACCESSIBILITY_FOCUS` are offered on every visible node.

### Action queue

An OS request is checked under the lock against the published snapshot: an ID the snapshot lacks is rejected (`UIA_E_ELEMENTNOTAVAILABLE`, `false` to Android, `ACCESSIBILITY_ERROR_UNKNOWN_NODE` to test access) and an action the node does not advertise is rejected (`UIA_E_INVALIDOPERATION`, `false`, `ACCESSIBILITY_ERROR_ACTION_NOT_OFFERED`), each counting `platform.accessibility.actions_rejected`. An accepted action is stamped with the published revision and the monotonic time and pushed to the 64-entry ring (`QUEUE_FULL_POLICY_DROP_OLDEST`: the oldest action is lost; counters `platform.accessibility.actions.full`, `.dropped`, `.high_water` per the counting rule of `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow"). `DrainActions` copies the oldest actions into the span under the lock and removes them; actions that do not fit stay queued. UI drains in its claimant callback inside `Platform.Pump` and turns each into navigation input, so an OS action takes the same path a key press does and reaches simulation only as UI's recorded publications (Z2-D-019).

## OS accessibility preferences

### OS preference sources

| Preference | Windows | Android | Normalised |
|---|---|---|---|
| reduced motion | `SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION)` FALSE | `Settings.Global.ANIMATOR_DURATION_SCALE` = 0 | `m_bReducedMotion` |
| high contrast | `SystemParametersInfoW(SPI_GETHIGHCONTRAST)` with `HCF_HIGHCONTRASTON` | `UiModeManager.getContrast()` ≥ 0.5 (medium or high contrast) | `m_bHighContrast` |
| text scale | `HKEY_CURRENT_USER\Software\Microsoft\Accessibility`, DWORD `TextScaleFactor` (100–225; absent means 100) ÷ 100 | `Configuration.fontScale` | `m_fTextScale`, clamped to [1.0, 2.0] |

Windows reads them at creation, at the pump after a `WM_SETTINGCHANGE`, and every 2,000 ms in the pump (the text-scale value has no reliable change broadcast). Android reads them in `attach` and on the observers of § "Java bridge contract"; a font-scale change reaches the bridge's `ComponentCallbacks` only because the template's manifest lists `fontScale` in `configChanges` (row P20). Both report all three bits of `m_uReportedMask`; RECORDING reports what test access sets. The pure functions `Zenith_AccessibilityNormaliseWindowsPreferences(bool bClientAreaAnimation, bool bHighContrastOn, uint32 uTextScaleFactor)` and `Zenith_AccessibilityNormaliseAndroidPreferences(float fAnimatorScale, float fFontScale, float fContrast)` in `Internal/Accessibility/Zenith_AccessibilityPreferences.h` and `.cpp` do the normalisation and are tested on every host.

### Preference keys and the platform layer

PLAT registers three keys holding the OS preferences (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers"): `platform.accessibility.reducedMotion`, `platform.accessibility.highContrast` and `platform.accessibility.textScale`, allowed in the engine-default, platform and command-line layers. They are PLAT's keys because a library may look up only keys it registers or declares a dependency on (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering", [Z2-N-ARCH-930]): ZenithPlatform cannot write ZenithUI's `ui.access.*` keys, and UI, which depends on ZenithPlatform, reads PLAT's keys to give its own settings their OS default (row P21 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`).

The OS is a source of the platform layer, applied by `Zenith_Config`:

1. `Zenith_Config::Create` (startup step 6), after applying the cooked platform file in resolution step 2, reads `m_pxPlatform->GetAccessibility()->GetState().m_xPreferences` (when the desc has a platform and the platform has a service) and applies every reported preference with `ApplyValueText(CONFIG_LAYER_PLATFORM, <key>, <text>, "os:accessibility", 0)` (`true`/`false`, or `Zenith_FormatFloat`'s text), so the OS value wins over the cooked file's within layer 2, while the command line still wins above it. An unreported preference keeps the cooked file's value or the engine default.
2. `ApplyQueuedUserWrites` (prologue step 3), before committing user values: when the service's state revision changed since the last application, it applies the changed reported preferences the same way, adds every key whose resolved value changed to `GetChangedKeys()` and dispatches the change registry once per changed key, so UI's `Zenith_UISystem::ApplyConfig` sees an OS change at the next prologue.
3. These values are never written to the user file (they are not user-layer values) and their provenance is `os:accessibility` in the platform layer, so the dump and the configuration panel explain them.

`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" admits step 2 above, the second runtime change of `Zenith_Config` (cross-document item X269): its layer-2 row names the OS accessibility preferences (`os:accessibility`) after the cooked file, its `Zenith_Config` comment names the sub-source beside user-layer writes, and [Z2-N-ARCH-943] states that the sub-source writes only these three keys, none of them simulation-affecting or restart-required, and is re-applied at the frame prologue when the platform reports a change.

## The recording implementation

The recording implementation is a real implementation of the service, selected by configuration on every test host, not a test double: the snapshot, validation, diff, action queue, speech validation and focus hold are the code every backend runs, and only the OS calls are replaced by records. It is a declared seam of ZenithPlatform, reached by other libraries' tests through § "Zenith_AccessibilityTestAccess API".

### Record log

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_AccessibilityTestAccess.h
enum Zenith_AccessibilityRecordKind : uint8
{
	ACCESSIBILITY_RECORD_TREE_PUBLISHED = 1,   // m_ulRevision, m_uNodeCount, m_ulNodeID = the focused node's ID
	ACCESSIBILITY_RECORD_FOCUS_CHANGED,        // m_ulNodeID
	ACCESSIBILITY_RECORD_STRUCTURE_CHANGED,
	ACCESSIBILITY_RECORD_NODE_CHANGED,         // m_ulNodeID, m_uChangedFields
	ACCESSIBILITY_RECORD_LIVE_REGION_CHANGED,  // m_ulNodeID, m_strText = the new name
	ACCESSIBILITY_RECORD_ANNOUNCEMENT,         // m_strText, m_bAssertive
	ACCESSIBILITY_RECORD_UTTERANCE,            // m_strText, m_fRate, m_bInterrupt
	ACCESSIBILITY_RECORD_STOP_SPEAKING,
	ACCESSIBILITY_RECORD_ACTION_QUEUED,        // m_ulNodeID, m_eAction, m_dValue
	ACCESSIBILITY_RECORD_ACTIONS_DRAINED,      // m_uNodeCount = the number drained
	ACCESSIBILITY_RECORD_PROVIDER_CHANGED,     // m_uNodeCount = 1 registered, 0 cleared
};

inline constexpr uint8 uACCESSIBILITY_FIELD_NAME = 1u << 0, uACCESSIBILITY_FIELD_VALUE = 1u << 1,
	uACCESSIBILITY_FIELD_DESCRIPTION = 1u << 2, uACCESSIBILITY_FIELD_STATE = 1u << 3, uACCESSIBILITY_FIELD_BOUNDS = 1u << 4,
	uACCESSIBILITY_FIELD_RANGE = 1u << 5, uACCESSIBILITY_FIELD_HEADING = 1u << 6;

/// @contract value Zenith_AccessibilityRecord
/// Ownership: Plain value in the recording service's log; m_strText views the log's text pool.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the record log, written in place.
/// Invalidation: Until ClearRecords or the service's destruction.
/// Units: m_ulPumpIndex is Zenith_Platform::GetFrameIndex() when recorded; m_fRate a multiplier.
/// Errors: None.
/// Version: since Z2 Stage 4. Test API.
/// @endcontract
struct Zenith_AccessibilityRecord
{
	Zenith_AccessibilityRecordKind m_eKind = ACCESSIBILITY_RECORD_TREE_PUBLISHED;
	bool m_bAssertive = false;
	bool m_bInterrupt = false;
	uint8 m_uChangedFields = 0;                       // uACCESSIBILITY_FIELD_*
	Zenith_AccessibilityActionKind m_eAction = ACCESSIBILITY_ACTION_ACTIVATE;
	float m_fRate = 1.0f;
	uint32 m_uNodeCount = 0;
	uint64 m_ulPumpIndex = 0;
	uint64 m_ulRevision = 0;
	uint64 m_ulNodeID = 0;
	double m_dValue = 0.0;
	Zenith_StringView m_strText;
};
```

The log holds 4,096 records and a 1 MiB text pool, allocated at creation. When either is full, a new record is dropped (`QUEUE_FULL_POLICY_DROP_NEWEST`; counters `platform.accessibility.recording.full`, `.dropped`, `.high_water`), so the views of the records already logged stay valid; `ClearRecords` empties both.

### Recording behaviour per member

| Member | Recording behaviour |
|---|---|
| state | screen reader, tree requested, speech availability (default true) and preferences (default all reported, every value the default) as test access last set them, applied at the next pump |
| `NotifyTreeChanged`, `SetTreeProvider` | the shared algorithm; step 8 appends one TREE_PUBLISHED record followed by the diff's records, in the diff's order |
| `Announce` | one ANNOUNCEMENT record |
| `Speak` | one UTTERANCE record; `IsSpeaking` is true until the next pump, or until `SetSpeechHold(false)` while a hold is set |
| `StopSpeaking` | one STOP_SPEAKING record; `IsSpeaking` false |
| actions | `InjectAction` runs the action-queue check and appends ACTION_QUEUED; `DrainActions` appends ACTIONS_DRAINED when it returns at least one |

## Zenith_AccessibilityTestAccess API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_AccessibilityTestAccess.h
#pragma once
#include "ZenithPlatform/Zenith_Accessibility.h"

/// @contract functions Zenith_AccessibilityTestAccess
/// Ownership: Static functions over a service the test reaches through Zenith_Platform::GetAccessibility(); no state.
/// Thread affinity: MAIN_THREAD (the service's).
/// Invalidation: A record reference and its text are valid until ClearRecords; GetPublishedTree's views until the next
///   publication.
/// Units: Milliseconds for SetFocusSpeechHoldMs.
/// Errors: The record and state setters ZENITH_CHECK that the backend is RECORDING (a test defect otherwise);
///   InjectAction returns the action-queue check's error.
/// Version: since Z2 Stage 4. Test API: compiled into every configuration, reachable only from Tests/, Zenith2/Benchmarks/
///   and ZenithPlatform's Internal/ sources (TestSeparationCheck rule 6).
/// @endcontract
class Zenith_AccessibilityTestAccess
{
public:
	static uint32 GetRecordCount(const Zenith_AccessibilityService& xService);
	static const Zenith_AccessibilityRecord& GetRecord(const Zenith_AccessibilityService& xService, uint32 uIndex);   // oldest first
	static uint64 GetDroppedRecordCount(const Zenith_AccessibilityService& xService);
	static void ClearRecords(Zenith_AccessibilityService& xService);

	static Zenith_AccessibilityTreeView GetPublishedTree(const Zenith_AccessibilityService& xService);   // any backend
	static Zenith_Result<void, Zenith_AccessibilityError> InjectAction(Zenith_AccessibilityService& xService,
		uint64 ulNodeID, Zenith_AccessibilityActionKind eKind, double dValue);

	static void SetScreenReaderActive(Zenith_AccessibilityService& xService, bool bActive);
	static void SetTreeRequested(Zenith_AccessibilityService& xService, bool bRequested);
	static void SetSpeechAvailable(Zenith_AccessibilityService& xService, bool bAvailable);
	static void SetSpeechHold(Zenith_AccessibilityService& xService, bool bHold);
	static void SetPreferences(Zenith_AccessibilityService& xService, const Zenith_AccessibilityPreferences& xPreferences);
	static void SetFocusSpeechHoldMs(Zenith_AccessibilityService& xService, uint32 uMilliseconds);   // any backend
};
```

The functions are defined in `Zenith2/Engine/ZenithPlatform/Internal/Accessibility/Zenith_AccessibilityTestAccess.cpp`, compiled into ZenithPlatform in every configuration, as `Zenith2/Engine/ZenithRuntime/Zenith_RuntimeTestAccess.h` is in ZenithRuntime (`Zenith2/Docs/Contracts.md` → "Test separation"). UI's tests, the `Zenith_Integration_UI` suite and ZenithPlatform's tests read the log through it.

## Accessibility threading and shutdown

### Accessibility thread affinity

| Code | Thread |
|---|---|
| every `Zenith_AccessibilityService` member, the provider's `GetTree`, publication, the diff, OS events, announcements, `Speak` and `StopSpeaking` calls | the main thread |
| the UI Automation provider objects | any thread UI Automation chooses |
| the Android natives except `nativeOnSpeechState` | the Android UI thread |
| `nativeOnSpeechState` | the `TextToSpeech` callback threads |
| the SAPI voice | `Zenith.Speech` |

### The accessibility lock

`uLOCK_LEVEL_ACCESSIBILITY` (640, ZenithPlatform, short-hold) guards the published-snapshot pointers and revision, the client-origin offset, the action queue, the OS state, preference and speech cells, and the Windows speech command queue. Its holders copy and store only; it nests under no other lock and takes none (`Zenith2/Docs/Design/PLAT.md` → "Lock-order entries").

The row 640 `uLOCK_LEVEL_ACCESSIBILITY` is in `Zenith2/Docs/Contracts.md` → "Lock ordering table" (short-hold; acquired on the main thread, the UI Automation provider threads, the Android UI thread through the JNI natives and `Zenith.Speech`), and step 10b of `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" names `Zenith.Speech`, joined by `Zenith_AccessibilityService`'s destructor first in the platform's destructor (cross-document item X270; [Z2-N-CONTRACT-320], [Z2-N-CONTRACT-348]).

### Accessibility shutdown order

UI unregisters its provider (`SetTreeProvider(nullptr)`) in its module shutdown, Contracts step 6, before the platform is destroyed after step 10; the service then holds no borrowed pointer. A provider still registered at destruction is a defect: `ZENITH_ASSERT` in Debug, and in other configurations the pointer is dropped without a call. The service is destroyed first among the platform's services (it is created last), before any window:

1. Windows: `RemoveWindowSubclass`; `UiaReturnRawElementProvider(hwnd, 0, 0, nullptr)`; set the shared block's disconnected flag under the lock; `UiaDisconnectAllProviders()`, which releases UI Automation's references; push QUIT to `Zenith.Speech` and join it; release the root provider. The live object count is then 0; a non-zero count logs `accessibility: <n> UI Automation objects still referenced at shutdown`, and the objects and the shared block stay allocated (so a late call finds the disconnected flag) and are reported by the Debug leak table.
2. Android: `detach()` (after it returns no native is entered), then delete the bridge's and the activity's global references.
3. RECORDING: free the log.
4. Free the snapshots and the queue.

The `Zenith.Speech` thread is joined in the platform's destructor, after Contracts step 10, with `Zenith.Watchdog` (`Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown").

## Accessibility diagnostics

### Accessibility counters

| Counter | Kind | Meaning |
|---|---|---|
| `platform.accessibility.screen_reader_active` | GAUGE | 1 while a screen reader is active |
| `platform.accessibility.nodes`, `.pool_bytes` | GAUGE | the published tree |
| `platform.accessibility.publish_us` | GAUGE | the last publication's time |
| `platform.accessibility.publishes`, `.trees_rejected` | CUMULATIVE | publications and rejected trees |
| `platform.accessibility.os_events`, `.os_events_capped` | CUMULATIVE | OS events raised; publications that hit the 64-event cap |
| `platform.accessibility.announcements`, `.utterances`, `.text_truncated`, `.speech_unavailable` | CUMULATIVE | speech |
| `platform.accessibility.actions.full`, `.dropped`, `.high_water`, `platform.accessibility.actions_rejected` | CUMULATIVE | the action queue (`.dropped` only, `DROP_OLDEST`) and rejected OS requests |
| `platform.accessibility.speech_queue.full`, `.dropped`, `.high_water` | CUMULATIVE | the Windows speech queue (`.dropped` only) |
| `platform.accessibility.recording.full`, `.dropped`, `.high_water` | CUMULATIVE | the record log (`.dropped` only, `DROP_NEWEST`) |
| `platform.accessibility.uia_objects` | GAUGE | live UI Automation provider objects (Windows) |

### Accessibility log lines

Category `accessibility`: the backend chosen at creation (info), OS-side failures (`accessibility: OS backend unavailable: <reason>`, warning), a rejected tree (once per rule per session, warning), speech unavailability (once, warning), screen-reader state changes (info), objects alive at shutdown (error). No log line contains node text, so a name or a typed value never reaches a log.

## Accessibility failure handling

| Failure | Behaviour | Surfaced |
|---|---|---|
| COM, the UI Automation root, the Java class or `RegisterNatives` fails at creation | no service; the platform starts | the warning; `GetAccessibility()` is null |
| no SAPI voice, `TextToSpeech` initialisation fails | the service stays; `m_bSpeechAvailable` false; `Speak` returns `ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE` | the warning, `platform.accessibility.speech_unavailable` |
| an invalid or oversized tree | the previous snapshot stays published | the error result, `platform.accessibility.trees_rejected` |
| an OS query for a node that vanished | `UIA_E_ELEMENTNOTAVAILABLE`; Android receives null | none (normal) |
| an OS action for an unknown node or an action not offered | rejected | `platform.accessibility.actions_rejected` |
| more than 64 actions before a drain | the oldest is lost | `platform.accessibility.actions.dropped` |
| allocation failure at creation | `PLATFORM_ERROR_OUT_OF_MEMORY`; the platform unwinds | the caller |
| allocation failure while growing a snapshot | `ACCESSIBILITY_ERROR_OUT_OF_MEMORY`; the previous snapshot stays | the error result |
| invalid announcement or utterance text | `ACCESSIBILITY_ERROR_BAD_TEXT`; nothing spoken | the error result |

## Accessibility tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/`, built into `ZenithPlatform_Tests`, and run in the `Test` step on `win-debug`, `win-release` and `linux-tsan` unless the row says otherwise. They use the recording backend and ZenithPlatform's own internal headers; nothing in them is a double of a PLAT or UI type.

| Test | File | Asserts |
|---|---|---|
| `Test_Accessibility_ApiLayout` | `Zenith_Accessibility_Tests.cpp` | `sizeof(Zenith_AccessibilityNode) == 96` with the member offsets of "Accessibility value types"; every enumerator of the four accessibility enumerations and `PLATFORM_EVENT_ACCESSIBILITY_CHANGED` spelled by `Zenith_ToString`; an out-of-range value spells `"<unknown>"`; the action-bit rule (bit = kind − 1) for every kind |
| `Test_Accessibility_BackendSelection` | `Zenith_Accessibility_Tests.cpp` | each row of "Backend table" through the desc in headless mode (on the windowed rows' hosts, through a hidden non-Vulkan window on `win-debug`): the resolved backend, `GetAccessibility()` null for NONE, the `os`-without-window warning |
| `Test_Accessibility_ProviderRegistration` | `Zenith_Accessibility_Tests.cpp` | a second provider fails `ACCESSIBILITY_ERROR_PROVIDER_ALREADY_SET`; `NotifyTreeChanged` with none fails `ACCESSIBILITY_ERROR_NO_PROVIDER`; clearing publishes an empty tree and a STRUCTURE_CHANGED record; registration resets the published revision |
| `Test_Accessibility_ProviderCalledOnlyFromPublication` | `Zenith_Accessibility_Tests.cpp` | a counting provider's `GetTree` runs on the main thread only, once per `SetTreeProvider` and per accepted `NotifyTreeChanged`, and never while four threads run snapshot readers |
| `Test_Accessibility_TreeValidationRules` | `Zenith_Accessibility_Tests.cpp` | one `SUBCASE` per rule V1 to V9: the listed error, the published tree unchanged byte for byte, `trees_rejected` + 1 |
| `Test_Accessibility_RevisionRules` | `Zenith_Accessibility_Tests.cpp` | an equal revision does nothing and calls no provider; a lower one fails `ACCESSIBILITY_ERROR_STALE_REVISION`; a view revision that differs fails `ACCESSIBILITY_ERROR_INVALID_TREE` |
| `Test_Accessibility_SnapshotIndependentOfProvider` | `Zenith_Accessibility_Tests.cpp` | after a publication the provider's arrays are overwritten; `GetPublishedTree` and every snapshot reader still return the published content |
| `Test_Accessibility_DiffRecordsEachChange` | `Zenith_Accessibility_Tests.cpp` | focus, structure, each node field and a live region each produce their record in the table's order; 100 changed nodes give 64 records then one structure change and `os_events_capped` + 1 |
| `Test_Accessibility_HitTestFrontmostDeepest` | `Zenith_Accessibility_Tests.cpp` | overlapping roots: the first root's deepest containing node wins; hidden subtrees are skipped; a miss is 0 |
| `Test_Accessibility_ActionQueueOrderAndOverflow` | `Zenith_Accessibility_Tests.cpp` | 65 injected actions: 64 drained in order with the published revision, the oldest lost, `actions.dropped == 1`, `.full == 0`, `.high_water == 64`; an unknown node and an action not offered are rejected and counted; a span of 10 leaves the rest queued |
| `Test_Accessibility_SpeechValidationAndRecords` | `Zenith_Accessibility_Tests.cpp` | `Announce` and `Speak` record text, priority, rate and interrupt; empty text, invalid UTF-8 and U+0000 fail `ACCESSIBILITY_ERROR_BAD_TEXT`; 600 bytes are cut at a UTF-8 boundary at or below 512 and counted; rate 0.1 fails `ACCESSIBILITY_ERROR_BAD_RATE`; with speech unavailable `Speak` fails `ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE` |
| `Test_Accessibility_FocusSpeechHoldWindow` | `Zenith_Accessibility_Tests.cpp` | with a 50 ms hold and a screen reader active, a focus-changing publication makes `IsFocusSpeechPending` true and the first pump at least 50 ms after it makes it false; without a screen reader it stays false |
| `Test_Accessibility_StateChangeRaisesPresentationEvent` | `Zenith_Accessibility_Tests.cpp` | a test-access state change yields exactly one `PLATFORM_EVENT_ACCESSIBILITY_CHANGED` at the next pump, without `uPLATFORM_EVENT_FLAG_WORLD_VISIBLE`, with the changed-preference bits |
| `Test_Accessibility_BackgroundStopsSpeech` | `Zenith_Accessibility_Tests.cpp` | a scripted `DID_ENTER_BACKGROUND` through `Zenith_PlatformBackendHooks` records STOP_SPEAKING at that pump |
| `Test_Accessibility_CountersPublished` | `Zenith_Accessibility_Tests.cpp` | one publication, one rejected tree, one announcement, one utterance, one injected and drained action and one overfilled action queue on the recording backend publish every counter of "Accessibility counters" that the recording backend has (all but `speech_queue.*` and `uia_objects`) with its table kind |
| `Test_Accessibility_RecordLogDropsNewest` | `Zenith_Accessibility_Tests.cpp` | 4,100 announcements keep the first 4,096 with valid text, `recording.dropped == 4`, `GetDroppedRecordCount() == 4`; `ClearRecords` empties the log |
| `Test_Accessibility_VirtualIdsStable` | `Zenith_AccessibilityOs_Tests.cpp` | across three publications a surviving node keeps its virtual ID, a new node takes a fresh one, no ID is reused, −1 is never assigned |
| `Test_Accessibility_VirtualQueryArrays` | `Zenith_AccessibilityOs_Tests.cpp` | `Zenith_AccessibilityQueryVirtualNode` fills the 12 integers, the range and the three texts for every role; a name with U+1F600 converts to a surrogate pair; an unknown ID returns nothing |
| `Test_Accessibility_JavaMatchesNativeRegistration` | `Zenith_AccessibilityOs_Tests.cpp` | reads `Zenith2/Player/GradleTemplate/app/src/main/java/org/zenith2/player/Zenith2Accessibility.java` through `ZENITH_REPO_ROOT`: the class name, the constructor and every method and native of "Java bridge contract" with its JNI signature equal the strings of `Zenith_AccessibilityJniTable.h`, and every Java method that calls a native is `synchronized` |
| `Test_Accessibility_PreferenceNormalisation` | `Zenith_AccessibilityOs_Tests.cpp` | both normalisation functions for each raw value of "OS preference sources" (text-scale factor 100, 150, 225 and 300 → 1.0, 1.5, 2.0, 2.0; contrast 0, 0.5, 1) |
| `Test_Accessibility_SapiRateMapping` | `Zenith_AccessibilityOs_Tests.cpp` | 0.25, 0.5, 1.0, 2.0, 4.0 → −10, −6, 0, 6, 10 |
| `Test_Accessibility_SpeechQueueInterruptAndOverflow` | `Zenith_AccessibilityOs_Tests.cpp` | over an in-memory `Zenith_SpeechSink`: commands reach the sink in order on the queue's thread; an interrupt or STOP clears queued commands; 40 commands while the sink is blocked keep the newest 32 and count `speech_queue.dropped == 8` |
| `Test_Accessibility_UiaProviderReportsNodes` | `Zenith_AccessibilityOs_Tests.cpp` (windowed, `win-debug` only: needs a display) | an OS backend on a hidden non-Vulkan window: through the root provider's COM interfaces, every role's control type, name, help text, enabled, focus, bounding rectangle in screen pixels, runtime ID and navigation match the published tree |
| `Test_Accessibility_UiaPatternsQueueActions` | `Zenith_AccessibilityOs_Tests.cpp` (windowed, `win-debug` only) | `Invoke`, `Toggle`, `Select`, `SetValue`, `ScrollIntoView` and `SetFocus` queue the mapped actions; a pattern method for an action not offered returns `UIA_E_INVALIDOPERATION`; a vanished node returns `UIA_E_ELEMENTNOTAVAILABLE` |
| `Test_Accessibility_UiaDisconnectReleasesProviders` | `Zenith_AccessibilityOs_Tests.cpp` (windowed, `win-debug` only) | with node objects held by an in-process `IUIAutomation` client, destroying the platform leaves `uia_objects` 0 and the client's next call fails |
| `Test_Accessibility_EndToEndThroughPump` | `Zenith_Accessibility_Tests.cpp` | over 120 scripted pumps of a recording platform with a `Zenith_Config` built over it: a provider whose tree changes every 10 pumps is published, OS state changes arrive as events, injected actions are drained in order inside a claimant, a preference change reaches `platform.accessibility.*` at the next `ApplyQueuedUserWrites`, and the record log holds exactly the expected sequence; then the provider unregisters and the platform is destroyed with no leak (counting heap) |
| `Test_Accessibility_ConcurrentQueriesDuringPublish` | `Zenith_AccessibilityOs_Tests.cpp` | eight threads run snapshot readers while the main thread publishes 10,000 revisions whose names encode their revision: every copied node is internally consistent with one revision; clean under `Sanitizers -Preset linux-tsan` |

`Zenith_AccessibilityOs_Tests.cpp` has no platform branch: the three windowed cases are registered with the `win-debug`-only rule the windowed rows of `Zenith2/Docs/Design/PLAT/Platform.md` → "Platform core tests" use, and every other case runs the platform-neutral code on every host. `Test_Platform_Create_FailsAtEachStep_Unwinds` gains the `Accessibility` fault point (creation step 11) with the recording backend. Configuration's two preference tests are `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration tests".

## Accessibility device runs

`needs-human` runs, recorded under `Zenith2/Docs/Research/Platform/Stage4/DeviceRuns/Accessibility/`, against the harness `Zenith_Bench_AccessibilityBridge` (`Zenith2/Benchmarks/AccessibilityBridge/`), whose `--interactive <seconds>` mode opens a window with a fixed demo tree (a dialog with a heading, two buttons, a checkbox, a slider offering increment, decrement and set value, a list of 20 items, a polite live text updated every 2 s and an assertive alert every 10 s), logs every drained action, speaks the focused node's name with built-in speech when no screen reader runs, and announces on each activation. The harness's measured cases (publication cost for 64, 512 and 4,096 nodes, snapshot query latency from a second thread) run in `--smoke` form in the `Test` step on the recording backend. On the S24 it runs as a benchmark APK (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs"; listing it is row P20 (c)).

| Run | Platform | Evidence |
|---|---|---|
| Narrator reads every demo node with its role and state, moves focus, activates the buttons, toggles the checkbox, sets the slider, reads the live text politely and the alert assertively | Windows 11 reference desktop | run record with the action log |
| TalkBack explores by touch and by swipe, double-tap activates, the slider responds to volume-key adjustment, live text and alerts are spoken | S24 | run record with the action log |
| built-in speech: SAPI and `TextToSpeech` speak at rates 0.5, 1.0 and 2.0, an interrupt cuts the current utterance, backgrounding stops speech | Windows, S24 | run record |
| preferences: toggling animation effects, high contrast and text size on Windows, and remove animations, contrast and font size on the S24, changes `platform.accessibility.*` in the configuration panel without a restart | Windows, S24 | run record with the configuration dump before and after |
