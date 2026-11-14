# SCOPE design: the ZenithXR public API

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It declares every public header of the Stage 7 `ZenithXR` module, the interfaces of lower libraries ZenithXR implements, the `XR` Luau namespace, its event types, its configuration keys and its misuse codes.

## Scope of this file

Every fallible function below returns `Zenith_Result`, every public type carries one contract block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"), and no public header includes an OpenXR, Vulkan or D3D12 header ([Z2-N-CONTRACT-043]). OpenXR handles never appear in a public header.

## Rules across the XR API

- One `Zenith_XrSystem` exists per process, owned by the module instance; it exists even when XR is disabled, in state `ZENITH_XR_SESSION_STATE_DISABLED`, so callers never test for its absence.
- Every presentation action (`RequestRecentre`, `RequestExitSession`) is a request: it is performed in the next frame prologue and its outcome is observable through `GetStatus` and, for simulation, through an `XR` notification.
- Simulation never calls ZenithXR; the `XR` Luau namespace reads only published notification state (`Zenith2/Docs/Design/SCOPE/XrInput.md` → "Replay and determinism").
- Names that OpenXR also defines are prefixed `ZENITH_XR_`, so no enumerator collides with an `XR_*` name in the one TU that includes the OpenXR headers.

## Header list

| Header | Contents |
|---|---|
| `Zenith2/Engine/ZenithXR/Zenith_XrTypes.h` | session states, reference-space kinds, graphics APIs, the status snapshot, the play area, settings |
| `Zenith2/Engine/ZenithXR/Zenith_XrErrors.h` | `Zenith_XrErrorCode`, `Zenith_XrError` |
| `Zenith2/Engine/ZenithXR/Zenith_XrSystem.h` | `Zenith_XrSystem` |
| `Zenith2/Engine/ZenithXR/Zenith_Module_XR.h` | the two module entry points and the optional device-interop factory the XR module's descriptor names |
| `Zenith2/Engine/ZenithXR/Zenith_XrTestAccess.h` | test access (non-Shipping behaviour; refused in Shipping) |

## Zenith_XrTypes header

```cpp
// Zenith2/Engine/ZenithXR/Zenith_XrTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Maths.h"

/// @contract enum Zenith_XrSessionState
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7; values keep their meaning once shipped. Values 3 to 11 mirror XrSessionState's order.
/// @endcontract
enum Zenith_XrSessionState : uint8
{
	ZENITH_XR_SESSION_STATE_DISABLED = 1,        // no runtime, no system, a refused requirement, or xr.enabled = false
	ZENITH_XR_SESSION_STATE_NO_SESSION = 2,      // instance and system exist; the session is not yet created
	ZENITH_XR_SESSION_STATE_IDLE = 3,
	ZENITH_XR_SESSION_STATE_READY = 4,           // xrBeginSession is issued in the same prologue
	ZENITH_XR_SESSION_STATE_SYNCHRONIZED = 5,
	ZENITH_XR_SESSION_STATE_VISIBLE = 6,
	ZENITH_XR_SESSION_STATE_FOCUSED = 7,
	ZENITH_XR_SESSION_STATE_STOPPING = 8,        // xrEndSession is issued in the same prologue
	ZENITH_XR_SESSION_STATE_LOSS_PENDING = 9,
	ZENITH_XR_SESSION_STATE_EXITING = 10,
	ZENITH_XR_SESSION_STATE_RETRY_WAIT = 11,     // the session or system was lost; a new attempt after xr.retrySeconds
};

/// @contract enum Zenith_XrReferenceSpaceKind
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7; values keep their meaning once shipped.
/// @endcontract
enum Zenith_XrReferenceSpaceKind : uint8
{
	ZENITH_XR_REFERENCE_SPACE_STAGE = 1,         // XR_REFERENCE_SPACE_TYPE_STAGE
	ZENITH_XR_REFERENCE_SPACE_LOCAL_FLOOR = 2,   // XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR (core in OpenXR 1.1)
	ZENITH_XR_REFERENCE_SPACE_LOCAL = 3,         // XR_REFERENCE_SPACE_TYPE_LOCAL
};

/// @contract enum Zenith_XrGraphicsApi
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 7; values keep their meaning once shipped.
/// @endcontract
enum Zenith_XrGraphicsApi : uint8
{
	ZENITH_XR_GRAPHICS_API_NONE = 0,
	ZENITH_XR_GRAPHICS_API_VULKAN = 1,           // XR_KHR_vulkan_enable2
	ZENITH_XR_GRAPHICS_API_D3D12 = 2,            // XR_KHR_D3D12_enable
};

/// @contract value Zenith_XrPlayArea
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: None.
/// Units: Metres, the rectangle of xrGetReferenceSpaceBoundsRect centred on the stage origin.
/// Errors: None; m_bValid false when the runtime reports no bounds (XR_SPACE_BOUNDS_UNAVAILABLE).
/// Version: since Z2 Stage 7.
/// @endcontract
struct Zenith_XrPlayArea
{
	bool m_bValid = false;
	float m_fWidthMetres = 0.0f;                 // x extent
	float m_fDepthMetres = 0.0f;                 // z extent
};

/// @contract value Zenith_XrStatus
/// Ownership: Plain value copied out of the system's atomic snapshot.
/// Thread affinity: IMMUTABLE; members non-const: the issuer fills it in the caller's storage.
/// Invalidation: A snapshot; the next prologue may change the live state.
/// Units: Nanoseconds for the period; pixels for extents.
/// Errors: None.
/// Version: since Z2 Stage 7. Presentation state: never read by simulation.
/// @endcontract
struct Zenith_XrStatus
{
	Zenith_XrSessionState m_eState = ZENITH_XR_SESSION_STATE_DISABLED;
	Zenith_XrGraphicsApi m_eGraphicsApi = ZENITH_XR_GRAPHICS_API_NONE;
	Zenith_XrReferenceSpaceKind m_eSpace = ZENITH_XR_REFERENCE_SPACE_LOCAL;
	bool m_bStereoActive = false;                // a frame source currently serves stereo requests
	uint32 m_auRecommendedExtent[2] = {};       // per eye, recommendedImageRectWidth and Height
	uint32 m_auImageExtent[2] = {};             // per eye, the swapchain image extent after xr.renderScale
	uint64 m_ulDisplayPeriodNs = 0;             // the last predictedDisplayPeriod
	Zenith_XrPlayArea m_xPlayArea;
	uint32 m_uDisabledReason = 0;               // the Zenith_XrErrorCode that disabled XR, 0 otherwise
};

/// @contract value Zenith_XrSettings
/// Ownership: Plain value built from the configuration keys of "Configuration keys" at module start.
/// Thread affinity: IMMUTABLE; members non-const: filled field by field from the configuration keys.
/// Invalidation: None; user-layer changes of xr.renderScale and xr.mirror reach the system through ApplySettings.
/// Units: As each member states.
/// Errors: None; validated by the configuration schema.
/// Version: since Z2 Stage 7.
/// @endcontract
struct Zenith_XrSettings
{
	float m_fRenderScale = 1.0f;                 // [0.5, 2.0]
	bool m_bSubmitDepth = true;
	bool m_bMirrorLeftEye = true;
	bool m_bPauseWhenUnfocused = true;
	bool m_bUiQuadLayer = true;
	float m_fUiQuadDistanceMetres = 1.5f;        // [0.5, 5.0]
	float m_fUiQuadWidthMetres = 1.2f;           // [0.2, 5.0]
	uint32 m_uRetrySeconds = 5;                 // [1, 60]
	uint32 m_uSessionReadyTimeoutMs = 10000;    // [1000, 60000]
	uint32 m_uImageWaitTimeoutMs = 100;         // [10, 1000]
	uint32 m_uShutdownWaitMs = 500;             // [0, 5000]
	Zenith_XrReferenceSpaceKind m_ePreferredSpace = ZENITH_XR_REFERENCE_SPACE_STAGE;   // xr.referenceSpace auto = STAGE, then LOCAL_FLOOR, then LOCAL
};

constexpr const char* Zenith_ToString(Zenith_XrSessionState eState);   // "disabled", "noSession", "idle", "ready", "synchronized", "visible", "focused", "stopping", "lossPending", "exiting", "retryWait"
```

## Zenith_XrErrors header

```cpp
// Zenith2/Engine/ZenithXR/Zenith_XrErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_XrErrorCode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each value's meaning is its comment; Zenith2/Docs/Design/SCOPE/XrRuntime.md "Result mapping" maps every XrResult.
/// Version: since Z2 Stage 7; enumerators keep their value once shipped, new ones are appended.
/// @endcontract
enum Zenith_XrErrorCode : uint8
{
	ZENITH_XR_ERROR_RUNTIME_UNAVAILABLE = 1,       // no loader, no active runtime, or the runtime failed to load
	ZENITH_XR_ERROR_API_VERSION_UNSUPPORTED = 2,   // the runtime does not offer OpenXR 1.1
	ZENITH_XR_ERROR_EXTENSION_MISSING = 3,         // a required extension is absent; m_uDetail its index in the required list
	ZENITH_XR_ERROR_NO_HEADSET = 4,                // XR_ERROR_FORM_FACTOR_UNAVAILABLE
	ZENITH_XR_ERROR_VIEW_CONFIGURATION = 5,        // PRIMARY_STEREO unsupported, or not exactly two views
	ZENITH_XR_ERROR_GRAPHICS_REQUIREMENTS = 6,     // the device or API version the runtime requires is not the RHI's
	ZENITH_XR_ERROR_BACKEND_UNSUPPORTED = 7,       // the selected RHI backend has no binding extension in this runtime
	ZENITH_XR_ERROR_SESSION_CREATE = 8,
	ZENITH_XR_ERROR_SESSION_LOST = 9,
	ZENITH_XR_ERROR_INSTANCE_LOST = 10,
	ZENITH_XR_ERROR_SWAPCHAIN_FORMAT = 11,         // no format of the preference list is offered
	ZENITH_XR_ERROR_SWAPCHAIN_CREATE = 12,
	ZENITH_XR_ERROR_FRAME_CALL = 13,               // xrWaitFrame, xrBeginFrame or xrEndFrame failed other than by loss
	ZENITH_XR_ERROR_SPACE = 14,                    // a reference or action space could not be created or located
	ZENITH_XR_ERROR_ACTION_SETUP = 15,             // the mirror action set, an action or a binding suggestion failed
	ZENITH_XR_ERROR_OUT_OF_MEMORY = 16,
	ZENITH_XR_ERROR_INVALID_SCRIPT = 17,           // a zenith2.xr-script document failed validation; m_uDetail its rule
	ZENITH_XR_ERROR_INVALID_CONFIG = 18,
	ZENITH_XR_ERROR_NOT_RUNNING = 19,              // a request that needs a running session
	ZENITH_XR_ERROR_SHIPPING_REFUSED = 20,         // a test-access or scripted-runtime call in a Shipping build
};

/// @contract value Zenith_XrError
/// Ownership: Plain value returned inside a Zenith_Result.
/// Thread affinity: IMMUTABLE; members non-const: Zenith_Result and error records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: m_iXrResult is the OpenXR result that caused the error (0 when none), m_uDetail a code-specific index.
/// Version: since Z2 Stage 7. 8 bytes, no string ([Z2-N-CONTRACT-480]).
/// @endcontract
struct Zenith_XrError
{
	Zenith_XrErrorCode m_eCode;
	uint8 m_uReserved = 0;
	uint16 m_uDetail = 0;
	sint32 m_iXrResult = 0;
};
static_assert(sizeof(Zenith_XrError) == 8);

constexpr const char* Zenith_ToString(Zenith_XrErrorCode eCode);
```

## Zenith_XrInterop header

There is no separate interop header. The device-interop factory is declared in `Zenith_Module_XR.h` and reached only through the XR module's descriptor (§ "Zenith_Module_XR header"; ruling Z2-D-137).

## Zenith_XrSystem header

```cpp
// Zenith2/Engine/ZenithXR/Zenith_XrSystem.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithXR/Zenith_XrTypes.h"
#include "ZenithXR/Zenith_XrErrors.h"

class Flux_StereoFrameSource;   // ZenithFlux/Flux_StereoFrameSource.h

/// @contract type Zenith_XrSystem
/// Ownership: Owned by the XR module instance (Zenith_Owned), created at module start, destroyed at shutdown step 6
///   after the renderer's frame source is cleared; borrows the device-interop provider created at the end of startup step 11.
/// Thread affinity: MAIN_THREAD, except GetStatus (ANY_THREAD: an atomic snapshot written by the prologue) and
///   GetStereoFrameSource's object, whose members run where Zenith2/Docs/Design/SCOPE.md "Frame placement" says.
/// Invalidation: The frame-source reference is valid until BeginShutdown.
/// Units: As Zenith_XrStatus.
/// Errors: Requests made while no session runs return ZENITH_XR_ERROR_NOT_RUNNING and change nothing.
/// Reentrancy: NONE. No member calls back into the caller; the frame source never calls Zenith_XrSystem.
/// Version: since Z2 Stage 7. Source API.
/// @endcontract
class Zenith_XrSystem
{
public:
	Zenith_XrStatus GetStatus() const;
	Zenith_Result<void, Zenith_XrError> RequestRecentre();     // performed in the next prologue
	Zenith_Result<void, Zenith_XrError> RequestExitSession();  // xrRequestExitSession next prologue
	void ApplySettings(const Zenith_XrSettings& xSettings);                  // render scale and mirror take effect at the next render-idle point
	Flux_StereoFrameSource& GetStereoFrameSource();
	void PumpBeforePlatform();   // prologue step 1, OnBeforePlatformPump: controller sampling
	void PumpAfterPlatform();    // prologue step 1, OnPlatformEvents: events, transitions, requests
	void BeginShutdown();        // shutdown step 6, first: request exit, poll for STOPPING within m_uShutdownWaitMs
};
```

The constructor is private behind a `PrivateTag` passkey. Creation is the `Internal/` friend factory `Zenith_XrSystem_Create(const Zenith_XrSystemDesc&) → Zenith_Result<Zenith_Owned<Zenith_XrSystem>, Zenith_XrError>` declared in `Internal/Zenith_XrSystemFactory.h` and befriended by the class, so the public header names no `Internal/` type; it runs the fallible steps of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider" (module-start half) with a failure-injection site per step ([Z2-N-CONTRACT-341]); `Zenith_XrSystemDesc` is `Internal/` and holds the provider, the RHI device, the renderer, the input system, the governor, the settings and the replay flag.

## Zenith_Module_XR header

```cpp
// Zenith2/Engine/ZenithXR/Zenith_Module_XR.h
#pragma once
#include "ZenithRuntime/Zenith_Module.h"
#include "ZenithRuntime/Zenith_DeviceInteropProvider.h"

/// @contract functions Zenith_Module_XR
/// Ownership: Start returns the module instance the runtime owns until shutdown step 6; RegisterTypes retains nothing.
/// Thread affinity: MAIN_THREAD (RegisterTypes at startup step 6, Start at step 15).
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_ModuleError as the table "Step of Start" below gives; RegisterTypes returns MODULE_ERROR_REGISTRATION
///   when a type or key is refused and MODULE_ERROR_OUT_OF_MEMORY on allocation failure.
/// Version: since Z2 Stage 7. The two module entry points ([Z2-N-ARCH-848]).
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_XR_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_XR_Start(
	Zenith_ModuleStartContext& xContext);

/// @contract functions Zenith_Module_XR_CreateDeviceInterop
/// Ownership: Returns a Zenith_Owned provider the runtime owns from the end of startup step 11 until shutdown step 9,
///   after the device's destruction.
/// Thread affinity: MAIN_THREAD (startup step 11).
/// Invalidation: None.
/// Units: None.
/// Errors: Zenith_DeviceInteropError: DEVICE_INTEROP_ERROR_CONFIGURATION (an invalid xr.* key or scripted-runtime
///   document; startup exit code 2) and DEVICE_INTEROP_ERROR_OUT_OF_MEMORY (exit code 4) are the only failures, each
///   logged first with its Zenith_XrErrorCode; an absent loader, runtime or headset gives a disabled or waiting
///   provider, never an error.
/// Version: since Z2 Stage 7. Named only by the optional member m_pfnCreateDeviceInterop of the XR module's
///   Zenith_ModuleDescriptor: a composition-time hook, not a module entry point ([Z2-N-ARCH-848], [Z2-N-ARCH-261]
///   stand); uZENITH_MODULE_INTERFACE_VERSION stays 1.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_DeviceInteropProvider>, Zenith_DeviceInteropError> Zenith_Module_XR_CreateDeviceInterop(
	const Zenith_DeviceInteropContext& xContext);
```

The factory returns MOD's dedicated `Zenith_DeviceInteropError` (§ "Interfaces of lower libraries"): ZenithXR logs the `Zenith_XrErrorCode` first and returns `DEVICE_INTEROP_ERROR_CONFIGURATION` for `INVALID_CONFIG` and `INVALID_SCRIPT`, which step 11 maps to exit code 2, and `DEVICE_INTEROP_ERROR_OUT_OF_MEMORY`, mapped to exit code 4. `Zenith_ModuleError` and its reserved byte are not used.

| Step of `Start` | Action | Failure |
|---|---|---|
| Provider | `xContext.GetDeviceInteropProvider()` returns the provider the runtime created from this module's own descriptor member; null is a defect (`ZENITH_CHECK`); its kind is `DEVICE_INTEROP_PROVIDER_KIND_XR`, so it is converted with `static_cast` to `Zenith_XrInteropProvider` (asserted by kind, no RTTI) | — |
| Settings | read the keys of § "Configuration keys" into `Zenith_XrSettings`; under `--replay` the provider was created disabled (§ "Rules across the XR API") | — |
| System | `Zenith_XrSystem_Create`; a disabled provider gives a system in `DISABLED` | `MODULE_ERROR_OUT_OF_MEMORY` |
| Frame source | `xContext.GetRenderer()->SetStereoFrameSource(&system.GetStereoFrameSource())` when the renderer exists (not headless) | `MODULE_ERROR_REGISTRATION` |
| Bindings | the `XR` binding table and its result decoder, `RegisterScriptBindings` | `MODULE_ERROR_REGISTRATION` |
| AsyncSource | `DeclareScriptAsyncSource(ASYNC_SOURCE_XR, uXR_NOTIFY_PAYLOAD_BYTES)` (`ASYNC_SOURCE_XR` = 16 by ruling ASYNC-SOURCE-15-16; SCOPE writes no number in code, the registry's) | `MODULE_ERROR_REGISTRATION` |
| FrameListener | `RegisterFrameListener(instance)` (both prologue hooks) | `MODULE_ERROR_REGISTRATION` |
| MemoryConsumer | `RegisterMemoryConsumer(system.GetMemoryConsumer())` | `MODULE_ERROR_REGISTRATION` |
| Console, Panel | development builds: § "Console commands" and the `xr` panel of `Zenith2/Docs/Design/SCOPE.md` | `MODULE_ERROR_REGISTRATION` |

`RegisterTypes` registers the three event types of § "Event types" and the configuration keys. `AttachWorld` allocates the world's notification queue, binds it to `GetPublicationSink()` with a handler for `ASYNC_SOURCE_XR`, and registers `XR.Events@W<tag>` (`PHASE_INPUT`, `EmitsEvent` of the three types); `DetachWorld` drops the queue's unparked records and unbinds the handler.

The module's CMake call is `zenith_add_library(ZenithXR KIND MODULE MODULE_NAME XR DEPENDS ZenithRuntime ZenithFlux ZenithRHI ZenithPlatform ZenithScript ZenithReflect ZenithECS ZenithProfiling ZenithJobs ZenithCore THIRD_PARTY openxr-loader SUPPORTED ZENITH_WINDOWS TEST_LABELS z2-xr TEST_DEPENDS ZenithRHI_Vulkan ZenithRHI_D3D12)`; MOD's generator fills the descriptor's `m_pfnCreateDeviceInterop` with `&Zenith_Module_XR_CreateDeviceInterop` for this module alone (a module-to-target-mapping column, cross-area row SP5). XR is the 14th module of Architecture's module list (cross-doc item X429). A game whose `game.json` lists `XR` in `modules` and `android` in `platforms` fails the configure with `Games/<Name>/game.json: modules: XR is supported on windows only`, in the module-resolution format of `Zenith2/Docs/Design/MOD/GameBuild.md` → "Backend resolution".

## Zenith_XrTestAccess header

```cpp
// Zenith2/Engine/ZenithXR/Zenith_XrTestAccess.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithXR/Zenith_XrSystem.h"

/// @contract value Zenith_XrSeamCallRecord
/// Ownership: Plain value in the scripted runtime's call-log ring (4,096 records).
/// Thread affinity: IMMUTABLE once appended; members non-const: the call-log ring reuses its records by assignment.
/// Invalidation: Overwritten after 4,096 further calls; spans returned by GetSeamCallLog are valid until the next seam call.
/// Units: Nanoseconds of XrTime in m_aulArg where Zenith2/Docs/Design/SCOPE/XrRuntime.md "Scripted runtime" says so.
/// Errors: None.
/// Version: since Z2 Stage 7. Test seam; the field meaning per function is the "Call log" table.
/// @endcontract
struct Zenith_XrSeamCallRecord
{
	uint16 m_uFunction;        // Zenith_XrSeamFunction: the seam member's ordinal in the order of "The OpenXR API seam"
	uint16 m_uReserved = 0;
	sint32 m_iResult;          // the XrResult returned
	uint64 m_ulFrame;          // the scripted frame counter at the call
	uint64 m_aulArg[6];
	float m_afArg[13];
};

/// @contract type Zenith_XrTestAccess
/// Ownership: A struct of static functions; never instantiated.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: As Zenith_XrStatus.
/// Errors: Every function returns ZENITH_XR_ERROR_SHIPPING_REFUSED (or an empty value) in a Shipping build and on a
///   system that does not run the scripted runtime; a script that fails validation returns ZENITH_XR_ERROR_INVALID_SCRIPT.
/// Version: since Z2 Stage 7. Test seam, ruling FINAL-DOUBLES: the scripted runtime is a production implementation of
///   the Zenith_OpenXrApi seam selected by configuration, not a double.
/// @endcontract
struct Zenith_XrTestAccess
{
	// Appends events and state changes to the scripted runtime's timeline (Zenith2/Docs/Design/SCOPE/XrRuntime.md "Scripted runtime").
	static Zenith_Result<void, Zenith_XrError> AppendScript(Zenith_XrSystem& xSystem, Zenith_StringView strScriptJson);
	static uint64 GetSubmittedFrameCount(const Zenith_XrSystem& xSystem);
	static uint32 GetLastLayerCount(const Zenith_XrSystem& xSystem);
	static uint64 GetLastPredictedDisplayTime(const Zenith_XrSystem& xSystem);
	static uint32 GetSeamCallCount(const Zenith_XrSystem& xSystem, Zenith_StringView strFunction);   // "xrEndFrame"
	static Zenith_Span<const Zenith_XrSeamCallRecord> GetSeamCallLog(const Zenith_XrSystem& xSystem);  // oldest first
	static void ClearSeamCallLog(Zenith_XrSystem& xSystem);
	// Fault injection of the session steps S1 to S4, and of P8 and P9 at a device recreation; the provider steps at startup
	// are injected through Zenith_DeviceInteropContext::m_pxFaults instead, because no system exists before step 15.
	static void FailNextStep(Zenith_XrSystem& xSystem, Zenith_StringView strStep);                                  // "S2", "P8"
	// The provider's own view of the seam call log, readable before and without a system (the startup-step tests).
	static Zenith_Span<const Zenith_XrSeamCallRecord> GetProviderSeamCallLog(const Zenith_DeviceInteropProvider& xProvider);
};
```

## Interfaces of lower libraries

ZenithXR implements three interfaces declared below it. Their declarations are their owners'; the members SCOPE needs are stated here so the owners can land them.

<!-- assumption: pending MOD, ARCH (row SP5) -->
```cpp
// Zenith2/Engine/ZenithRuntime/Zenith_DeviceInteropProvider.h (MOD; Stage 7)
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithRHI/Zenith_RHIInterop.h"
#include "ZenithRuntime/Zenith_Module.h"

enum Zenith_DeviceInteropProviderKind : uint8 { DEVICE_INTEROP_PROVIDER_KIND_XR = 1 };

/// @contract value Zenith_DeviceInteropError
/// Ownership: Plain value returned inside a Zenith_Result.
/// Thread affinity: IMMUTABLE; members non-const: Zenith_Result and error records copy-assign it.
/// Invalidation: None.
/// Units: None.
/// Errors: DEVICE_INTEROP_ERROR_CONFIGURATION: an invalid configuration value or input document; startup step 11 maps
///   it to exit code 2. DEVICE_INTEROP_ERROR_OUT_OF_MEMORY: an allocation failed; exit code 4. m_uDetail is the
///   provider's own detail code (ZenithXR: its Zenith_XrErrorCode), logged by the provider before it returns.
/// Version: since Z2 Stage 7. 2 bytes, no string ([Z2-N-CONTRACT-480]).
/// @endcontract
enum Zenith_DeviceInteropErrorCode : uint8 { DEVICE_INTEROP_ERROR_CONFIGURATION = 1, DEVICE_INTEROP_ERROR_OUT_OF_MEMORY = 2 };
struct Zenith_DeviceInteropError
{
	Zenith_DeviceInteropErrorCode m_eCode;
	uint8 m_uDetail = 0;
};

struct Zenith_DeviceInteropContext            // what step 11 passes to the factory
{
	const char* m_szRHIBackendName;           // the value of rhi.backend, lower case ("vulkan" or "d3d12")
	const Zenith_Config* m_pxConfig;
	Zenith_Heap* m_pxHeap;
	bool m_bHeadless;                         // a headless run creates the provider disabled
	bool m_bReplaying;                        // --replay creates it disabled
	Zenith_FaultInjector* m_pxFaults = nullptr;   // tests only: consulted at the provider steps P2 to P9 (CINE's test-construction pattern)
};

/// @contract interface Zenith_DeviceInteropProvider
/// Ownership: Created through the optional descriptor member m_pfnCreateDeviceInterop as the last action of startup
///   step 11; owned by the runtime until shutdown step 9, after the device's destruction.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: FillInteropRequest and OnDeviceCreated return Zenith_DeviceInteropError; the runtime logs it and continues
///   without interop (the provider disables itself), never failing startup for it. Creation failures follow the
///   provider failure rule of Zenith2/Docs/Design/SCOPE/XrRuntime.md "Startup and the device-interop provider".
/// Reentrancy: NONE.
/// Version: since Z2 Stage 7.
/// @endcontract
class Zenith_DeviceInteropProvider
{
public:
	virtual ~Zenith_DeviceInteropProvider() = default;
	virtual Zenith_DeviceInteropProviderKind GetKind() const = 0;
	// step 12, before the factory call: the instance and device requirements, an empty request when disabled
	virtual Zenith_Result<void, Zenith_DeviceInteropError> FillInteropRequest(Zenith_RHIInteropRequest& xOut) = 0;
	// step 12, after the factory returned: the device's interop handles; also after each device recreation
	virtual Zenith_Result<void, Zenith_DeviceInteropError> OnDeviceCreated(Zenith_RHIDevice& xDevice) = 0;
	virtual void OnDeviceDestroying() = 0;   // before the device is destroyed or recreated (loss, requested recreation, shutdown step 9)
	virtual void OnDeviceDestroyed() = 0;    // after the device's destruction: the provider may now destroy what the device was created for
};
```
`Zenith_ModuleDescriptor` gains the optional member `Zenith_PfnCreateDeviceInterop m_pfnCreateDeviceInterop` (nullptr for every module but XR; ruling Z2-D-137), and no startup step is added: the last action of startup step 11 calls the one non-null member, step 12 calls `FillInteropRequest` before the factory and `OnDeviceCreated` after it, and shutdown step 9 calls `OnDeviceDestroying` and `OnDeviceDestroyed` around the device's destruction and then destroys the provider (cross-doc item X411). `Zenith_ModuleStartContext` gains `Zenith_DeviceInteropProvider* GetDeviceInteropProvider()` (the provider created from the calling module's descriptor, null for every other module) and `Flux_Renderer* GetRenderer()` where it lacks it, and `Zenith_ModuleFrameListener` gains `virtual void OnBeforePlatformPump() {}`, called in prologue step 1 immediately before `Platform.Pump`. `uZENITH_MODULE_INTERFACE_VERSION` stays 1 for the descriptor member, the provider interface and the new listener member (all additive, with defaults), as [Z2-N-ARCH-879] states for the earlier additions. MOD's step-failure mapping gains the rows "step 11, device interop: `DEVICE_INTEROP_ERROR_CONFIGURATION` → 2, `DEVICE_INTEROP_ERROR_OUT_OF_MEMORY` → 4" (cross-doc item X411, row SP5); a failure of step 12 after the provider exists unwinds the provider in reverse order with the step.
<!-- /assumption -->

<!-- assumption: pending RHI (row SP2) -->
```cpp
// Zenith2/Engine/ZenithRHI/Zenith_RHIInterop.h (RHI; Stage 7)
struct Zenith_RHIInteropRequest
{
	// Vulkan: the runtime creates the instance and device through these callbacks (XR_KHR_vulkan_enable2), so the RHI
	// calls them in place of vkCreateInstance (step 3) and vpCreateDevice's vkCreateDevice (step 6), and picks the
	// physical device the runtime names in place of its own selection (step 5).
	void* m_pUserData = nullptr;
	Zenith_Result<uint64, Zenith_RHIError> (*m_pfnCreateVulkanInstance)(void* pUserData, const void* pVkInstanceCreateInfo, void* pfnGetInstanceProcAddr) = nullptr;
	Zenith_Result<uint64, Zenith_RHIError> (*m_pfnSelectVulkanPhysicalDevice)(void* pUserData, uint64 ulVkInstance) = nullptr;
	Zenith_Result<uint64, Zenith_RHIError> (*m_pfnCreateVulkanDevice)(void* pUserData, uint64 ulVkPhysicalDevice, const void* pVkDeviceCreateInfo, void* pfnGetInstanceProcAddr) = nullptr;
	// D3D12: the adapter LUID and minimum feature level of xrGetD3D12GraphicsRequirementsKHR
	bool m_bHasAdapterLuid = false;
	uint8 m_auAdapterLuid[8] = {};
	uint32 m_uMinFeatureLevel = 0;           // D3D_FEATURE_LEVEL value
};
struct Zenith_RHIInteropHandles               // opaque native handles as integers, valid until OnDeviceDestroying
{
	uint64 m_ulVkInstance, m_ulVkPhysicalDevice, m_ulVkDevice; uint32 m_uVkQueueFamily, m_uVkQueueIndex;
	uint64 m_ulD3D12Device, m_ulD3D12GraphicsQueue;
};
struct Zenith_RHIExternalTextureDesc          // an image the runtime owns (an XrSwapchain image)
{
	uint64 m_ulNativeImage;                  // VkImage or ID3D12Resource*
	Zenith_RHIFormat m_eFormat; uint32 m_uWidth, m_uHeight, m_uArrayLayers;
	Zenith_RHITextureUsage m_eUsage;          // colour attachment and sampled, or depth attachment
	Zenith_RHIResourceState m_eReleaseState;  // the state the image is in at acquire and must be in at release:
	                                          // COLOR_ATTACHMENT / DEPTH_WRITE (OpenXR's Vulkan and D3D12 rules)
	const char* m_szDebugName;
};
// Zenith_RHIDevice members: Zenith_RHIInteropHandles GetInteropHandles() const;
//   uint64 GetNativeTexture(Zenith_RHITextureHandle xTexture) const;   // VkImage or ID3D12Resource*; the scripted XR runtime's route; returns 0 in a Shipping build
//   Zenith_Result<Zenith_RHITextureHandle, Zenith_RHIError> ImportExternalTexture(const Zenith_RHIExternalTextureDesc&);
//   (DestroyTexture of an imported handle releases its views and never frees the image)
```
<!-- /assumption -->

<!-- assumption: pending RPROD (row SP1) -->
```cpp
// Zenith2/Engine/ZenithFlux/Flux_StereoFrameSource.h (RPROD; Stage 7)
struct Flux_XrViewPoses                       // moved from Internal/Views/Flux_StereoViews.h and widened
{
	Zenith_Matrix4 m_axView[2];               // world-from-eye inverse, from xrLocateViews composed with the rig pose
	float m_afTanLeft[2], m_afTanRight[2], m_afTanUp[2], m_afTanDown[2];   // tan of XrFovf angles
	Zenith_RHITextureHandle m_axImages[2];    // this frame's colour image per eye
	Zenith_RHITextureHandle m_axDepthImages[2];   // null handles when depth is not submitted
	uint32 m_auExtent[2][2];                 // image extent per eye
	uint64 m_ulPredictedDisplayTime;         // XrTime of the frame
};
struct Flux_StereoSubmitInfo { uint64 m_ulFrameIndex; float m_fNear; float m_fFar; bool m_bReversedZ; bool m_bRendered; };
class Flux_StereoFrameSource
{
public:
	virtual ~Flux_StereoFrameSource() = default;
	// Flux.BuildGraph: false means no stereo this frame; the renderer then renders the stereo request as its mono main view
	virtual bool AcquireStereoFrame(uint64 ulFrameIndex, const Zenith_Matrix4& xRigWorldFromOrigin, Flux_XrViewPoses& xOut) = 0;
	virtual bool BeforeSubmit(uint64 ulFrameIndex) = 0;      // Flux.Submit; false: submit without the eye images
	virtual void AfterSubmit(const Flux_StereoSubmitInfo& xInfo) = 0;      // Flux.Present
	virtual bool PacesFrames() const = 0;                     // true while a session runs: the pacer uses FLUX_PACING_MODE_EXTERNAL
};
// Flux_Renderer member: void SetStereoFrameSource(Flux_StereoFrameSource* pxSource);   // MAIN_THREAD, render-idle point
```
<!-- /assumption -->

## Script-facing API

The `XR` namespace, registered by the module for behaviours (chunk systems call none of it; a call from a chunk system raises ZS1702). It reads the published notification state of the calling world, so its answers are recorded and replayable.

| Function | Returns | Semantics |
|---|---|---|
| `XR.GetSessionInfo()` | table `{ state: string, focused: boolean, stereo: boolean, refreshHz: number }` | the last `XR.SessionChanged` published to this world; before the first one `{ state = "disabled", focused = false, stereo = false, refreshHz = 0 }` |
| `XR.GetPlayArea()` | table `{ width: number, depth: number }` or `nil` | the last `XR.PlayAreaChanged` with valid bounds; `nil` before one or when invalid |
| `XR.RequestRecentre()` | nothing | records a side effect of kind `SIDE_EFFECT_KIND_XR_RECENTRE` in the invocation's private buffer; on commit the module's consumer calls `Zenith_XrSystem::RequestRecentre` in the next prologue; a rolled-back invocation requests nothing |

## Event types

| Event | Fields | Published when |
|---|---|---|
| `XR.SessionChanged` | `state: string` (`Zenith_ToString` of the state), `focused: boolean`, `stereo: boolean`, `refreshHz: number` | the session state or the stereo-active flag changed in a prologue |
| `XR.PlayAreaChanged` | `valid: boolean`, `width: number`, `depth: number` | at session start and on `XrEventDataReferenceSpaceChangePending` for the stage space |
| `XR.Recentred` | `yawRadians: number` | after a recentre was applied |

Each event is a notification: the prologue allocates a completion ID through the world's publication sink and parks a 64-byte payload (`uXR_NOTIFY_PAYLOAD_BYTES`), little-endian: byte 0 the kind (1, 2, 3), bytes 1–3 zero, then per kind — SessionChanged: u8 state, u8 focused, u8 stereo, u8 0, f32 refresh Hz; PlayAreaChanged: u8 valid, 3 zero bytes, f32 width, f32 depth; Recentred: f32 yaw — and the rest zero. At most 8 notifications are outstanding per world (`uXR_NOTIFY_MAX_OUTSTANDING`); a ninth replaces the oldest unparked one of the same kind, counted `xr.notify.dropped`. The handler at the tick boundary decodes them in completion-ID order into the world's `XR` state and queues the events for `XR.Events@W<tag>`, which emits them in phase 1.

## Configuration keys

Registered under the module prefix `xr` (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Key grammar"); none is simulation-affecting, none is written to the replay header.

| Key | Type | Default | Range | Layers | Meaning |
|---|---|---|---|---|---|
| `xr.enabled` | bool | true | — | E G U C | false creates the provider disabled |
| `xr.runtime` | enum | `openxr` | `openxr`, `scripted` | E C | `scripted` selects the scripted runtime; refused in Shipping (`ZENITH_XR_ERROR_INVALID_CONFIG`) |
| `xr.script` | string | `""` | a `content:/` or command-line-granted path | C | the scripted runtime's document |
| `xr.renderScale` | float | 1.0 | 0.5–2.0 | E G Q U C | swapchain extent = recommended × scale, clamped to the maximum |
| `xr.referenceSpace` | enum | `auto` | `auto`, `stage`, `localFloor`, `local` | G U C | the preferred space, falling back in that order |
| `xr.submitDepth` | bool | true | — | G U C | depth swapchains and `XR_KHR_composition_layer_depth` when the runtime offers it |
| `xr.mirror` | enum | `leftEye` | `none`, `leftEye` | G U C | the desktop window's content while a session runs |
| `xr.pauseWhenUnfocused` | bool | true | — | G U C | adds `WORLD_PAUSE_REASON_XR_UNFOCUSED` while not FOCUSED |
| `xr.uiLayer` | enum | `quad` | `quad`, `world` | G U C | the screen-space UI as a head-locked quad layer, or not at all (`world`: the game shows only world-space panels) |
| `xr.uiQuadDistanceMetres` | float | 1.5 | 0.5–5.0 | G U C | the quad's distance in view space |
| `xr.uiQuadWidthMetres` | float | 1.2 | 0.2–5.0 | G U C | its width; the height follows the canvas aspect |
| `xr.retrySeconds` | int | 5 | 1–60 | E C | retry interval in `RETRY_WAIT` |
| `xr.sessionReadyTimeoutMs` | int | 10000 | 1000–60000 | E C | the wait for `READY` |
| `xr.imageWaitTimeoutMs` | int | 100 | 10–1000 | E C | `xrWaitSwapchainImage` timeout |
| `xr.shutdownWaitMs` | int | 500 | 0–5000 | E C | the wait for `STOPPING` at shutdown |

## Misuse codes

The `XR` namespace raises SCRIPT's generic codes and none of its own: an argument-count error is ZS1510 (`<Function> expects <Expected> arguments, got <Given>`) and a call from a chunk system is ZS1702 (`<Namespace>.<Function> is not available in <Context>`), both from `Zenith2/Docs/Scripting/MisuseCatalogue.md`. SCOPE reserves no block: the XR script surface raises only SCRIPT's generic codes, as ADVGFX, which registers no Luau binding and raises no misuse code, reserves none (`Zenith2/Docs/Design/ADVGFX.md` → "Luau errors and asserts").

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS1510 | MISUSE | `XR.GetSessionInfo`, `XR.GetPlayArea` or `XR.RequestRecentre` called with arguments | `XR.<Function> expects 0 arguments, got <Given>` |
| ZS1702 | MISUSE | any `XR` function called from a chunk system | `XR.<Function> is not available in chunk systems` |
