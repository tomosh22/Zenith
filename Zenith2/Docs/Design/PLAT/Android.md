# PLAT design: Android lifecycle, process death and thermal adaptation

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies the application lifecycle state machine, the event watch and the surface-loss handshake, surface recovery, the Windows equivalents, process death and restore, the low-memory relay, and thermal adaptation. The SPEC baseline is `Zenith2/Docs/Architecture/PlatformLayer.md` → "Android lifecycle and surface recovery" (with its subsections "Lifecycle state machine" and "Process death and restore") and "Thermal adaptation"; the build facts (minSdk 34, 16 KB pages, the HWASan build type) are `Zenith2/Docs/Architecture/BuildSystem.md` → "Android packaging".

## Scope of this file

| Topic | Heading here |
|---|---|
| the lifecycle API, state machine, event watch, handshake and recovery | "Application lifecycle" |
| the resume state written on background and restored on relaunch | "Process death and restore" |
| the OS low-memory signal | "Low memory relay" |
| headroom sampling and its actions | "Thermal adaptation" |
| storage paths and build facts PLAT relies on | "Android storage and packaging facts" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

## Application lifecycle

### Zenith_AppLifecycle API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_AppLifecycle.h

enum Zenith_AppLifecycleState : uint8
{
	APP_STATE_RUNNING = 1,
	APP_STATE_SUSPENDING,
	APP_STATE_BACKGROUND,
	APP_STATE_RESUMING,
	APP_STATE_TERMINATING,
};

enum Zenith_AppLifecycleEvent : uint8
{
	APP_EVENT_WILL_ENTER_BACKGROUND = 1,    // SDL_EVENT_WILL_ENTER_BACKGROUND
	APP_EVENT_DID_ENTER_BACKGROUND,         // SDL_EVENT_DID_ENTER_BACKGROUND
	APP_EVENT_WILL_ENTER_FOREGROUND,        // SDL_EVENT_WILL_ENTER_FOREGROUND
	APP_EVENT_DID_ENTER_FOREGROUND,         // SDL_EVENT_DID_ENTER_FOREGROUND
	APP_EVENT_TERMINATING,                  // SDL_EVENT_TERMINATING
};

enum Zenith_LifecycleError : uint8
{
	LIFECYCLE_ERROR_NO_LISTENER = 1,
	LIFECYCLE_ERROR_RESUME_DISABLED,
	LIFECYCLE_ERROR_RESUME_NOT_FOUND,
	LIFECYCLE_ERROR_RESUME_WRITE_FAILED,
	LIFECYCLE_ERROR_RESUME_DEADLINE_EXCEEDED,
};
constexpr const char* Zenith_ToString(Zenith_LifecycleError eError);

inline constexpr uint32 uSURFACE_HANDSHAKE_TIMEOUT_MS = 2000;
inline constexpr uint32 uRESUME_WRITE_DEADLINE_MS = 3000;

struct Zenith_ResumeIdentity
{
	uint8 m_auBuildID[16] = {};
	uint64 m_ulCompositionHash = 0;
	uint64 m_ulContentHash = 0;
	uint64 m_ulTickIndex = 0;              // recorded, not compared
};

enum Zenith_ResumeDiscardReason : uint8
{
	RESUME_DISCARD_REASON_NONE = 0,         // restored
	RESUME_DISCARD_REASON_BUILD_MISMATCH = 1,
	RESUME_DISCARD_REASON_COMPOSITION_MISMATCH,
	RESUME_DISCARD_REASON_CONTENT_MISMATCH,
	RESUME_DISCARD_REASON_UNREADABLE,       // the file failed SAVE's header or checksum validation
	RESUME_DISCARD_REASON_DISABLED,         // platform.resumeOnRelaunch = false
	RESUME_DISCARD_REASON_RESTORE_FAILED,   // restoring the worlds failed; a normal start follows
};

struct Zenith_ResumeWriteRequest
{
	Zenith_VirtualPath m_xTarget;           // user:/saves/resume.zsave
	uint64 m_ulDeadlineNs = 0;             // the DID_ENTER_BACKGROUND timestamp + 3,000 ms
};

/// @contract interface Zenith_AppLifecycleListener
/// Ownership: Implemented by ZenithRuntime (which forwards to the RHI, the renderer, audio, Zenith_Config, the worlds and SAVE);
///   passed to Zenith_AppLifecycle::SetListener by the composition root, unset before the runtime is destroyed.
/// Thread affinity: MAIN_THREAD. Every call arrives inside Platform.Pump (frame prologue step 1), when no frame-graph job runs.
/// Invalidation: None.
/// Units: Nanoseconds of the monotonic clock.
/// Errors: OnWriteResumeState returns Zenith_Result<Zenith_OperationHandle, Zenith_LifecycleError>; every other member
///   cannot fail from PLAT's point of view (a failure inside it follows its owner's recovery policy).
/// Reentrancy: NONE. A listener call may not call Pump or change the lifecycle state.
/// Version: since Z2 Stage 2, OnEnterBackground included (OnWriteResumeState since Z2 Stage 3). Source API.
/// @endcontract
class Zenith_AppLifecycleListener
{
public:
	virtual ~Zenith_AppLifecycleListener() = default;
	virtual void OnPause() = 0;                  // pause every world at its next tick boundary; stop haptics already done by PLAT
	virtual void OnReleaseSurface() = 0;         // finish GPU work on the window, destroy the swapchain and the surface
	virtual void OnStopAudio() = 0;
	virtual void OnEnterBackground() = 0;            // submit the user configuration layer without waiting (Zenith_Config::SubmitUserLayer)
	virtual Zenith_Result<Zenith_OperationHandle, Zenith_LifecycleError> OnWriteResumeState(
		const Zenith_ResumeWriteRequest& xRequest) = 0;
	virtual void OnRecreateSurface() = 0;        // create the surface and swapchain from the new native window
	virtual void OnResumeAudio() = 0;
	virtual void OnResume() = 0;                 // unpause the worlds
	virtual void OnTerminating() = 0;            // submit the user writes (never waiting) and request shutdown
};

/// @contract type Zenith_AppLifecycle
/// Ownership: Owned by Zenith_Platform.
/// Thread affinity: MAIN_THREAD, except the Android event watch, which runs on the Android UI thread inside
///   ZenithPlatform/Internal/Android/ and communicates only through the hand-over queue and the handshake semaphore.
/// Invalidation: None.
/// Units: Timeouts in milliseconds; timestamps in monotonic nanoseconds.
/// Errors: Zenith_Result<…, Zenith_LifecycleError> for the resume members; the state machine never fails.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_AppLifecycle
{
public:
	void SetListener(Zenith_AppLifecycleListener* pxListener);     // nullptr unsets
	Zenith_AppLifecycleState GetState() const;
	bool IsSurfaceLost() const;
	bool ShouldThrottle() const;                     // background, or every window minimised or occluded
	Zenith_Result<Zenith_VirtualPath, Zenith_LifecycleError> GetLaunchResumeCandidate() const;
	Zenith_ResumeDiscardReason DecideResume(const Zenith_ResumeIdentity& xFile,
		const Zenith_ResumeIdentity& xRunning) const;               // a pure function of the two identities and the key
	void ReportResumeOutcome(Zenith_ResumeDiscardReason eReason);  // logs; deletes the file on a discard
	void OnFirstTickCompleted();                                   // deletes a restored resume file
};

```

### Lifecycle state machine

The table is `Zenith2/Docs/Architecture/PlatformLayer.md` → "Lifecycle state machine", restated with PLAT's actions in the order they run; every cell not listed is ignored. The watch (UI thread) performs only the "watch" part; the rest runs on the main thread in the next `Platform.Pump`.

| State | WILL_ENTER_BACKGROUND | DID_ENTER_BACKGROUND | WILL_ENTER_FOREGROUND | DID_ENTER_FOREGROUND | TERMINATING |
|---|---|---|---|---|---|
| RUNNING | → SUSPENDING. Watch: queue the event, set the pause request, wait on the handshake semaphore (§ "Surface-loss handshake"). Main: `StopAll` haptics, cancel gestures, end text input, listener `OnPause`, `OnReleaseSurface`, signal the semaphore | — | ignored | ignored | → TERMINATING: listener `OnTerminating` |
| SUSPENDING | ignored | → BACKGROUND. Main: listener `OnStopAudio`; listener `OnEnterBackground` (the runtime submits the user configuration layer without waiting, `Zenith_Config::SubmitUserLayer`), called whether or not a resume write follows; the platform's own `Zenith_InputSystem::SubmitUserBindings` (without waiting); listener `OnWriteResumeState` starts the resume write (§ "Resume write sequence") | → RESUMING (the suspend was cancelled). Main: nothing released is restored yet; the next pump with a native window recreates the surface | — | → TERMINATING |
| BACKGROUND | ignored | ignored | → RESUMING. Main: recreate the surface at the first pump that has a native window (`OnRecreateSurface`) | — | → TERMINATING |
| RESUMING | → SUSPENDING (as from RUNNING, releasing a surface recreated meanwhile) | — | ignored | → RUNNING. Main: `OnRecreateSurface` if not yet done, `OnResumeAudio`, `OnResume`; delete the resume file of this still-living process (§ "Resume write sequence", step 6) | → TERMINATING |
| TERMINATING | ignored | ignored | ignored | ignored | ignored: shutdown in progress |

- `PLATFORM_EVENT_LIFECYCLE` records every transition (from, to) for the runtime and the logs; lifecycle events are presentation-only (not world-visible): simulation sees only the pause, which happens at a tick boundary (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Lifecycle state machine").
- `Test_AppLifecycle_StateMachine` covers every cell, including the ignored ones.

### The event watch

1. `Zenith_Platform::Create` on Android registers `SDL_AddEventWatch(&Zenith_AndroidLifecycleWatch, pxLifecycle)` (`Internal/Android/Zenith_AndroidLifecycle.cpp`); SDL3 calls the watch synchronously on the thread that raised the lifecycle event (the Android UI thread) before it queues the event.
2. The watch filters the five lifecycle events and `SDL_EVENT_LOW_MEMORY`; every other event returns immediately.
3. It pushes the event with its timestamp onto the lifecycle hand-over queue (32 entries, `QUEUE_FULL_POLICY_FATAL`, `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow") and, for `WILL_ENTER_BACKGROUND` while RUNNING or RESUMING, performs the handshake wait; it never calls engine code other than the queue and the semaphore.
4. The main thread's next pump drains the queue (step 2 of `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm") and runs the state machine in queue order.
5. On Windows and Linux no watch is installed; `Zenith_PlatformBackendHooks` can inject the five events on every host, so the state machine and handshake tests run in the ordinary `Test` step.

### Surface-loss handshake

1. The watch sets `m_bPauseRequested` (an `std::atomic<bool>` with release ordering) and waits on the handshake `Zenith_Semaphore` for at most `uSURFACE_HANDSHAKE_TIMEOUT_MS` (2,000 ms). The watch runs on the UI thread, not a job worker, so the wait is allowed (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Zenith_Multithreading").
2. The main thread reaches its next frame prologue within at most one frame of work, because the iteration in progress completes first (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure"); at that point every subgraph of the previous iteration, including the render subgraph, has completed.
3. In `Platform.Pump` it runs the RUNNING → SUSPENDING actions: the worlds pause (the simulation clock returns zero ticks from then on), and `OnReleaseSurface` finishes the GPU work that references the window, destroys the swapchain and releases the `ANativeWindow` (the RHI device, its resources and every world stay alive, `Zenith2/Docs/Architecture/PlatformLayer.md` → "Lifecycle state machine").
4. It then signals the semaphore; the watch returns and Android may destroy the surface.
5. Timeout: when the semaphore is not signalled in 2,000 ms, the watch marks the surface lost (`m_bSurfaceLost`, release ordering) and returns. From then until the foreground recreation, the RHI treats every acquire and present on that window as `SWAPCHAIN_OUT_OF_DATE`, which records no GPU work (`Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation"), so no GPU call ever uses a destroyed window; when the main thread next reaches its pump it still runs the SUSPENDING actions, and `OnReleaseSurface` destroys the swapchain without presenting. The timeout is counted in `lifecycle.handshake_timeouts` and logged with the main thread's frame index.
6. Android can deliver the surface's destruction (`surfaceDestroyed`) separately from `WILL_ENTER_BACKGROUND`; the watch treats the first of the two as the handshake trigger and the second as a no-op. Which SDL3 callback carries `surfaceDestroyed` in the pinned SDL3 release is verified by the Stage 2 Android lifecycle ticket and recorded as a ruling in its own diff (entry document → "Rulings PLAT records in its own tickets").

### Surface recovery

1. In RESUMING (or RUNNING after a cancelled suspend), each pump checks whether SDL reports a native window again (`SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER` non-null); the first pump that sees one sets `IsSurfaceAvailable()`, emits `PLATFORM_EVENT_SURFACE_AVAILABLE` and calls `OnRecreateSurface`.
2. The runtime creates the surface through `Zenith_Window::CreateVulkanSurface`, the RHI creates a swapchain for the new window's size, and the render graph rebuilds size-dependent targets on its next build; view histories whose extent changed are invalidated (`Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation").
3. A recreation failure follows the renderer's recovery policy (`Zenith2/Docs/Rendering.md` → "Recovery"); PLAT retries nothing itself.
4. `PLATFORM_EVENT_SURFACE_LOST` is emitted when the surface is released (step 3 of the handshake) or marked lost (step 5).

### Windows equivalents

| Windows event | PLAT behaviour | Lifecycle state |
|---|---|---|
| main window minimised | `PLATFORM_EVENT_WINDOW_MINIMISED`; the renderer produces no frames (`Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation"); `ShouldThrottle()` becomes true | stays RUNNING |
| main window restored | `PLATFORM_EVENT_WINDOW_RESTORED`, then a resize record | RUNNING |
| focus lost or gained | releases held keys and buttons, stops haptics when configured, releases relative mouse mode | RUNNING |
| session lock, logoff | account signals (`Zenith2/Docs/Design/PLAT/Platform.md` → "Account changes") | RUNNING |
| process termination request (`WM_QUERYENDSESSION`, `SDL_EVENT_QUIT`) | `PLATFORM_EVENT_QUIT_REQUESTED`; the runtime requests shutdown | RUNNING until shutdown |

Whether a game pauses when minimised is the game's choice, made from the window events it observes; the engine keeps simulating unless told otherwise. While `ShouldThrottle()` is true the frame loop calls `Zenith_Platform::WaitForEvents(100)` (an `SDL_WaitEventTimeout` on the main thread, outside every job) once per iteration, so a backgrounded or minimised game does not spin a core (`Zenith2/Docs/Design/MOD.md` → "Algorithms and state machines").

## Process death and restore

### Resume-state policy

Android may kill a backgrounded process; the runtime therefore writes a resumable state on every entry to the background and restores it on the next launch (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Process death and restore"). PLAT owns the policy (when to write, the deadline, the identity decision, deletion and logging); SAVE owns the file format, the snapshot serialization and the restore (`Zenith2/Docs/Design/SAVE.md` (pending; its "Public API" section)); the runtime connects them through `Zenith_AppLifecycleListener`.

| Rule | Value |
|---|---|
| file | `user:/saves/resume.zsave`, a SAVE save of kind resume, written by SAVE's journaled atomic write (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Journaled atomic write") |
| written | on every `DID_ENTER_BACKGROUND` on Android, unless `platform.resumeOnRelaunch` is false |
| deadline | 3,000 ms from the `DID_ENTER_BACKGROUND` timestamp (`uRESUME_WRITE_DEADLINE_MS`) |
| identities recorded | build ID, composition hash, cooked-content hash, tick index |
| restored | on launch, before the first tick, when build ID, composition hash and content hash all match the running game |
| deleted | after the first tick of a restored run completes; at once when discarded; on `DID_ENTER_FOREGROUND` of a process that was not killed |
| other platforms | never written; the policy code runs on every host under test hooks |

### Resume write sequence

1. At the pump that processes `DID_ENTER_BACKGROUND`, when resume is enabled and a listener is set, call `OnWriteResumeState({ user:/saves/resume.zsave, timestamp + 3,000 ms })`. The runtime runs the save-snapshot serialization of phase 11 (`Zenith2/Docs/Architecture/FramePhases.md` → "The twelve phases") on each paused world, outside a tick, and hands the bytes to SAVE's writer, which returns the write's operation handle.
2. Every following pump (the loop keeps pumping while backgrounded, throttled by `WaitForEvents`) checks the handle: completed successfully → log `resume state written (<bytes> bytes, <ms> ms)`; failed → log the error and leave no file.
3. When the deadline passes before completion, `RequestCancel` the handle, log `resume state abandoned: deadline`, and count `lifecycle.resume_deadline_missed`; SAVE's cancellation deletes the partial temporary file before reporting cancelled (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation"), so a relaunch never reads a torn resume state; a resume file of a previous background entry that the journal had fully committed remains and is still valid for its own identities.
4. A write refused for low disk ends the attempt (logged); there is no retry in the background.
5. If the process is killed at any point, the journal guarantees the file on disk is either the complete previous resume state, the complete new one, or absent.
6. On `DID_ENTER_FOREGROUND` of the living process: when the write is still in flight, let it complete, then delete the file; otherwise delete it at once (`AsyncIO::Delete`), counting `lifecycle.resume_deleted_live`.

### Relaunch decision

At startup, after the virtual filesystem and SAVE exist and before the startup scene loads (startup step 17, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"):

1. The runtime calls `GetLaunchResumeCandidate()`: `LIFECYCLE_ERROR_RESUME_DISABLED` when `platform.resumeOnRelaunch` is false (and any existing file is deleted with reason `DISABLED`), `LIFECYCLE_ERROR_RESUME_NOT_FOUND` when there is no file, otherwise the path.
2. SAVE reads the file's header; a header or checksum failure is `RESUME_DISCARD_REASON_UNREADABLE`.
3. `DecideResume(xFile, xRunning)` compares, in this order, build ID, composition hash, content hash, and returns the first mismatch's reason or `RESUME_DISCARD_REASON_NONE`.
4. `ReportResumeOutcome(eReason)`: `NONE` logs `resume state restored (tick <n>)`; any other reason logs `resume state discarded: <reason>` and deletes the file; both are logged at info level in every build, satisfying [Z2-N-ARCH-934] of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Process death and restore".
5. On `NONE` the runtime restores the worlds from the file instead of loading the startup scene; a restore failure reports `RESUME_DISCARD_REASON_RESTORE_FAILED` (logged, file deleted) and the startup scene loads as normal.
6. After the first tick of a restored run completes, the runtime calls `OnFirstTickCompleted()`, which deletes the file.

### Resume identity

- The build ID is the executable's build identity (the value the replay log records), the composition hash is `Zenith_GameComposition::m_ulCompositionHash` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "The generated composition table"), and the content hash is the mount manifest's `contentHash` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema").
- The tick index is recorded for the log and the restore, never compared.
- An installed update changes at least the build ID or the content hash, so a resume state never restores across an update; that is the intended behaviour, and the Play installed-update migration path is SAVE's and PACK's.

## Low memory relay

- `SDL_EVENT_LOW_MEMORY` (Android `onLowMemory`, and `onTrimMemory` at the levels SDL3 maps to it) becomes `PLATFORM_EVENT_LOW_MEMORY` at the next pump, whatever the lifecycle state.
- The runtime relays it to `Zenith_MemoryGovernor::ReportOSPressure(MEMORY_PRESSURE_CRITICAL)`; the governor holds the level for `memory.androidLowMemoryHoldMs` and applies the shedding table (`Zenith2/Docs/Architecture.md` → "Memory governor"). PLAT holds no memory policy of its own.
- The watch also sees the event (step 2 of "The event watch") only to queue it; it never waits for it.
- On Windows the governor queries the low-memory resource notification itself (`Zenith2/Docs/Architecture.md` → "Memory governor", "Pressure levels"); PLAT relays nothing there.

## Thermal adaptation

### Zenith_ThermalMonitor API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_ThermalMonitor.h

enum Zenith_ThermalStatus : uint8      // mirrors ATHERMAL_STATUS_*; UNKNOWN when no provider answers
{
	THERMAL_STATUS_UNKNOWN = 0, THERMAL_STATUS_NONE, THERMAL_STATUS_LIGHT, THERMAL_STATUS_MODERATE,
	THERMAL_STATUS_SEVERE, THERMAL_STATUS_CRITICAL, THERMAL_STATUS_EMERGENCY, THERMAL_STATUS_SHUTDOWN,
};

struct Zenith_ThermalSample
{
	float m_fHeadroom = 0.0f;               // AThermal_getThermalHeadroom(10): 1.0 = the SEVERE threshold forecast in 10 s
	Zenith_ThermalStatus m_eStatus = THERMAL_STATUS_UNKNOWN;
	uint64 m_ulTimestampUs = 0;
	bool m_bValid = false;                  // false when the provider returned NaN or is unavailable
};

/// @contract interface Zenith_ThermalProvider
/// Ownership: The platform default (Android AThermal, or the null provider) is owned by Zenith_ThermalMonitor; a test
///   provider is passed through Zenith_PlatformBackendHooks and outlives the platform.
/// Thread affinity: MAIN_THREAD for Sample; the status listener may call back on any thread (queued to the platform).
/// Invalidation: None.
/// Units: Headroom is a ratio; timestamps in microseconds.
/// Errors: None: an unavailable reading is a sample with m_bValid false.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 5. Source API.
/// @endcontract
class Zenith_ThermalProvider
{
public:
	virtual ~Zenith_ThermalProvider() = default;
	virtual bool IsAvailable() const = 0;
	virtual Zenith_ThermalSample Sample(uint64 ulNowUs) = 0;
	virtual Zenith_ThermalStatus GetCurrentStatus() = 0;
};

/// @contract interface Zenith_ThermalAdaptationTarget
/// Ownership: Implemented by ZenithRuntime (forwarding to the dynamic-resolution controller and to a user-layer write of
///   quality.activeTier through Zenith_Config::SetUserValue); set with Zenith_ThermalMonitor::SetTarget by the composition root.
/// Thread affinity: MAIN_THREAD; called in frame prologue step 5.
/// Invalidation: None.
/// Units: The resolution cap is a per-axis scale of the output resolution.
/// Errors: None.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 5. Source API.
/// @endcontract
class Zenith_ThermalAdaptationTarget
{
public:
	virtual ~Zenith_ThermalAdaptationTarget() = default;
	virtual void SetResolutionCap(float fMaxScale) = 0;      // the controller never exceeds it; never below the floor
	virtual void SetTierOffset(uint32 uLevelsBelowSelected) = 0;
	virtual void RequestCheckpointSave() = 0;
};

struct Zenith_ThermalConfig
{
	bool m_bEnabled = true;                  // thermal.enabled
	float m_fDrsHeadroom = 0.85f;            // thermal.drsHeadroom
	float m_fTierHeadroom = 0.95f;           // thermal.tierHeadroom
	float m_fRecoverHeadroom = 0.70f;        // thermal.recoverHeadroom
	uint32 m_uRecoverSeconds = 30;          // thermal.recoverSeconds
	float m_fDrsStep = 0.05f;                // thermal.drsStep: of output, per axis
	uint32 m_uTierCooldownSeconds = 10;     // thermal.tierCooldownSeconds
	float m_fResolutionFloor = 0.75f;        // the row Z2-B-SHIP-011 "Dynamic-resolution floor"
	uint32 m_uTierCount = 1;                // the platform's tier list length
	uint32 m_uSampleIntervalMs = 2000;      // Architecture: every 2 s
};

/// @contract type Zenith_ThermalMonitor
/// Ownership: Owned by Zenith_Platform.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: None.
/// Units: Seconds, milliseconds and ratios as the configuration keys state.
/// Errors: None.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 5. Source API (the null provider since Z2 Stage 2).
/// @endcontract
class Zenith_ThermalMonitor
{
public:
	void SetTarget(Zenith_ThermalAdaptationTarget* pxTarget);
	void Configure(const Zenith_ThermalConfig& xConfig);     // by the runtime: after Zenith_Config::Create in startup step 6, and on a thermal.* change
	void SampleIfDue(uint64 ulNowUs);                       // inside Platform.Pump (step 8 of "Pump algorithm")
	Zenith_ThermalSample GetLatestSample() const;   // prologue step 5 (Architecture's name)
	void ApplyActions(uint64 ulNowUs);                      // prologue step 5, after GetLatestSample
	void NotifyBaseTierChanged();                            // the player's quality.tier changed (from the change registry)
	Zenith_ThermalStatus GetStatus() const;     // the latest status (listener or sample)
	uint32 GetResolutionSteps() const;
	uint32 GetTierOffset() const;
	bool IsProviderAvailable() const;
};

```

### Thermal providers

| Platform | Provider | Mechanism |
|---|---|---|
| Android | `Internal/Android/Zenith_ThermalProvider_Android.cpp` | `AThermal_acquireManager()` at creation (minSdk 34 guarantees the API); `AThermal_getThermalHeadroom(manager, 10)` at most once per `m_uSampleIntervalMs` (the API returns NaN when called more often than once per second, and a NaN sample is invalid); `AThermal_getCurrentThermalStatus` for `GetCurrentStatus`; `AThermal_registerThermalStatusListener`, whose callback (a binder thread) queues a `PLATFORM_EVENT_THERMAL_STATUS_CHANGED` to the platform's cross-thread queue; `AThermal_releaseManager` at destruction |
| Windows, Linux | `Internal/Zenith_ThermalProvider_Null.cpp` | `IsAvailable()` false; `Sample` returns invalid samples; no adaptation ever happens (`Test_Thermal_NullProviderNoAdaptation`) |

The monitor publishes each valid sample as the counter `platform.thermal_headroom` ([Z2-N-ARCH-938] of `Zenith2/Docs/Architecture/PlatformLayer.md` → "Thermal adaptation").

### Thermal adaptation state machine

State: `r` = resolution steps (0 to `R`, where `R = floor((1 − m_fResolutionFloor) / m_fDrsStep)`, 5 with the defaults), `o` = tier offset (0 to `m_uTierCount − 1`), `c` = cool time (seconds the headroom has stayed at or below the recover threshold), `tLast` = the time of the last tier change.

Prologue step 5 (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure") is two calls: the runtime reads `GetLatestSample()` (the frame's thermal telemetry, as `Zenith2/Docs/Architecture.md` → "Purpose and scope", table "Assumed types", relies on) and then calls `ApplyActions(ulNowUs)`. The sample itself is taken by `SampleIfDue` inside `Platform.Pump`, at most once per `m_uSampleIntervalMs`. `ApplyActions` runs the table below when a new sample was taken or a status-change event arrived since its last run, or `NotifyBaseTierChanged` was called since its last run; in the last case the table is skipped and only the target calls below run, with `o` and `r` unchanged (§ "Applying thermal actions"). `h` is the sample's headroom and `s` the latest status. Rows are evaluated top to bottom and only the first matching row runs, with two exceptions: after the SHUTDOWN row no other row runs, and the `m_fDrsHeadroom` row is also evaluated when the tier row matched.

| Condition (first match, with the exceptions above) | Action |
|---|---|
| `m_bEnabled` false, or no valid sample and no status | nothing |
| `s` is `SHUTDOWN` | once per entry into SHUTDOWN: `RequestCheckpointSave`; set `o` to its maximum and `r` to `R`; `c = 0` |
| `h ≥ m_fTierHeadroom`, or `s ≥ SEVERE` | when `o < m_uTierCount − 1` and at least `m_uTierCooldownSeconds` have passed since `tLast`: `o = o + 1`, `tLast = now`; `c = 0` |
| `h ≥ m_fDrsHeadroom` (also evaluated when the tier row matched) | when `r < R`: `r = r + 1`; `c = 0` |
| `h ≤ m_fRecoverHeadroom` | `c = c + the time since the previous sample`; when `c ≥ m_uRecoverSeconds`: undo one step, the resolution first (`r > 0`: `r = r − 1`; else `o > 0`: `o = o − 1`), then `c = 0` |
| otherwise (`m_fRecoverHeadroom < h < m_fDrsHeadroom`) | `c = 0` |

After the table, the monitor calls `SetResolutionCap(max(m_fResolutionFloor, 1 − r × m_fDrsStep))` and `SetTierOffset(o)` when either value changed (and `SetTierOffset(o)` after a base-tier notification), and publishes the state in `platform.thermal_drs_steps` and `platform.thermal_tier_offset`. The monitor never reads `Zenith_Config` (the platform holds none): the runtime calls `Configure` after `Zenith_Config::Create` in startup step 6, with each field that has a `thermal.*` key read from that key and `m_uTierCount` from `Zenith_NarrowCast<uint32>(GetQualityTiers().GetCount())`, and again from `Zenith_Config::GetChangeRegistry()` whenever a `thermal.*` key changes (entry document → "The Platform Pump step"). The thresholds are read from the configuration keys named by `Zenith2/Docs/Architecture/PlatformLayer.md` → "Thermal adaptation" (defaults 0.85, 0.95, 0.70 and 30 s); the step (5% of output) and the floor are that table's.

### Applying thermal actions

- The runtime's target maps the tier offset to a user-layer write, `Zenith_Config::SetUserValue(quality.activeTier, tiers[min(selected + o, count − 1)], "thermal")`, where `selected` is the index of `quality.tier`, the player's chosen tier; an offset of 0 writes the empty string. The chain entry of the write carries the source `thermal` (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Provenance chain").
- A player's change of `quality.tier` clears `quality.activeTier` in the same commit (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier changes"), so the new choice takes effect at once. The runtime forwards that change from `Zenith_Config::GetChangeRegistry()` to `NotifyBaseTierChanged()`, and the next `ApplyActions` re-bases: it keeps `o` and `r`, and calls `SetTierOffset(o)` even though `o` did not change, so the target writes `tiers[min(newSelected + o, count − 1)]` when `o > 0`. The write is committed at the next prologue's step 3 and is never persisted (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier changes"), so `Zenith_Config` stays immutable except for user-layer writes.
- The resolution cap limits the dynamic-resolution controller's maximum scale (the LIGHT area's dynamic resolution, `Zenith2/Docs/Design/LIGHT.md` (pending; its "Public API" section)); the controller still chooses its scale below the cap from frame time, and never renders below the floor (`Zenith2/Docs/Rendering.md` → "Render views").
- Thermal adaptation changes presentation only (resolution, quality tier), never simulation state, tick rate or anything a replay records ([Z2-N-ARCH-919]); no simulation-affecting key can change with the tier (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Simulation-affecting keys").
- The checkpoint save requested at `SHUTDOWN` is an ordinary save request through the runtime (SAVE's checkpoint), issued at the next tick boundary.

### Thermal telemetry

| Output | Content |
|---|---|
| counter `platform.thermal_headroom` | every valid sample |
| counter `platform.thermal_status` | the status as its enumerator value, every change |
| counters `platform.thermal_drs_steps`, `platform.thermal_tier_offset` | the state after every change |
| counter `platform.thermal_samples_invalid` | NaN or unavailable samples |
| budget-run evidence `sustained.thermalStatus[]` | `GetStatus()` sampled at 1 Hz by the Player's budget-run mode during MP-SUSTAIN (`Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure"); the NDK `AThermal_getCurrentThermalStatus` reports the same status values as the `PowerManager.getCurrentThermalStatus` the procedure names |

## Android storage and packaging facts

| Fact | Source | PLAT use |
|---|---|---|
| minSdk 34 | `Zenith2/Docs/Architecture/BuildSystem.md` → "Android packaging" | `AThermal_*` (API 30 and 31) and `AAssetManager` are always present; no runtime API-level checks exist |
| 16 KB pages | as above | PLAT never assumes a 4 KiB page: any page-granular size (the I/O chunk alignment, mapped reads if introduced) comes from `sysconf(_SC_PAGESIZE)`; the ELF alignment is checked by `ApkCheck` |
| the HWASan build type | as above | every PLAT test runs in the HWASan device run of stage acceptance (entry document → "Device tests") |
| internal storage | `SDL_GetAndroidInternalStoragePath()`; SDL3's `SDL_GetPrefPath` returns this same directory on Android (the organisation and application names are ignored), with a trailing `/`, so `Zenith_Platform::GetPrefPath()` is the internal storage root | the user mount root; the root of PACK's Android install file system (`Patches/`, `.zenith2-install/`, `Zenith2/Docs/Design/PACK/Formats.md` → "Android install layout"), which therefore shares one volume and one directory with the user mount (user paths sit under `config/`, `saves/`, `cache/`, `logs/`, `crash/` and, outside Shipping, `devtools/` (rule R10, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order"), PACK's under its own names, so the two never collide) |
| the Java context | `SDL_GetAndroidJNIEnv`, `SDL_GetAndroidActivity` once at creation | `Zenith_Platform::GetAndroidJavaContext` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Android Java context"), the only way libraries above PLAT reach JNI |
| network state | `ConnectivityManager` through the template's `Zenith2Connectivity` class | `Zenith_Platform::GetConnectivityRevision` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity") |
| app-specific external storage | `SDL_GetAndroidExternalStoragePath()` | the development loose mount (`<external>/Loose/`, written by `adb push`), never used in Shipping |
| APK assets | `AAssetManager_fromJava` on the activity's `getAssets()` (JNI through `SDL_GetAndroidJNIEnv` and `SDL_GetAndroidActivity`) | the package mount's APK source; install-time asset packs appear through the same manager |
| the activity's configuration changes | the Gradle template's `android:configChanges` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "The Gradle template") | rotation and keyboard changes arrive as window and safe-area events, never an activity restart |

## Android and lifecycle tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/` and run in the `Test` step on every host preset, driving the lifecycle, resume and thermal paths through `Zenith_PlatformBackendHooks` and a test provider; the first four are `Zenith2/Docs/Architecture/PlatformLayer.md` → "Required tests" rows.

| Test | File | Asserts |
|---|---|---|
| `Test_AppLifecycle_StateMachine` | `Zenith_AppLifecycle_Tests.cpp` | every state × event cell of "Lifecycle state machine" produces the listed transition and listener calls in the listed order (SUSPENDING × DID_ENTER_BACKGROUND: `OnStopAudio`, `OnEnterBackground`, then `SubmitUserBindings`, then `OnWriteResumeState`; `OnEnterBackground` is still called when `platform.resumeOnRelaunch` is false and no resume write starts); unlisted cells are ignored |
| `Test_AppLifecycle_HandshakeTimeout` | `Zenith_AppLifecycle_Tests.cpp` | a main thread that does not pump for 2,500 ms: the watch returns after 2,000 ms (± 50 ms), `IsSurfaceLost()` is true, the late pump still runs the SUSPENDING actions |
| `Test_ResumeState_MatchingIdentityRestores` | `Zenith_ResumeState_Tests.cpp` | a resume file with matching identities restores; each single mismatch (build, composition, content) discards it with that reason logged and the file deleted |
| `Test_Thermal_NullProviderNoAdaptation` | `Zenith_Thermal_Tests.cpp` | on Windows and Linux the monitor reports no valid samples and never calls the target |
| `Test_AppLifecycle_HandshakeAcknowledgedWithinFrame` | `Zenith_AppLifecycle_Tests.cpp` | with the main thread pumping every 16 ms, the watch returns within one frame plus the release time, and `OnReleaseSurface` ran before the return |
| `Test_AppLifecycle_CancelledSuspendRecreatesSurface` | `Zenith_AppLifecycle_Tests.cpp` | WILL_ENTER_BACKGROUND then WILL_ENTER_FOREGROUND: the surface is recreated at the first pump with a native window |
| `Test_AppLifecycle_SurfaceDestroyedFirstIsHandshake` | `Zenith_AppLifecycle_Tests.cpp` | a surface-destroyed notification before WILL_ENTER_BACKGROUND triggers the handshake once; the second trigger is a no-op |
| `Test_AppLifecycle_SurfaceLostFlagAfterTimeout` | `Zenith_AppLifecycle_Tests.cpp` | after a handshake timeout `IsSurfaceLost()` is true and the main window's `GetNativeHandle()` returns `NATIVE_WINDOW_KIND_NONE` until the foreground recreation |
| `Test_AppLifecycle_PresentAfterTimeoutIsOutOfDate` | `Zenith2/Engine/ZenithRuntime/Tests/Zenith_LifecyclePresent_Tests.cpp` (built into `ZenithRuntime_Tests`, which may link the RHI; `ZenithPlatform_Tests` cannot, because ZenithRHI depends on ZenithPlatform) | after a handshake timeout every present through the RHI test device reports `SWAPCHAIN_OUT_OF_DATE` and records no GPU work until recovery |
| `Test_AppLifecycle_ThrottleWhenBackground` | `Zenith_AppLifecycle_Tests.cpp` | `ShouldThrottle()` is true in SUSPENDING, BACKGROUND and with the main window minimised, false otherwise |
| `Test_ResumeState_WrittenOnBackgroundOnly` | `Zenith_ResumeState_Tests.cpp` | DID_ENTER_BACKGROUND starts one write with the right target and deadline; `platform.resumeOnRelaunch = false` starts none and deletes an existing file at launch |
| `Test_ResumeState_DeadlineAbandonsAndCleans` | `Zenith_ResumeState_Tests.cpp` | a write that has not completed at 3,000 ms is cancelled, logged, and leaves no temporary file; the previous complete file survives |
| `Test_ResumeState_ForegroundDeletesLiveFile` | `Zenith_ResumeState_Tests.cpp` | DID_ENTER_FOREGROUND of a living process deletes the file, after an in-flight write completes |
| `Test_ResumeState_UnreadableAndRestoreFailedFallBack` | `Zenith_ResumeState_Tests.cpp` | an unreadable file and a failed restore each log their reason, delete the file and continue with a normal start |
| `Test_ResumeState_DeletedAfterFirstTick` | `Zenith2/Tests/Lifecycle/Zenith_ResumeIntegration_Tests.cpp` | a restored run deletes the file after its first tick, not before |
| `Test_LowMemory_RelayedToGovernorAsCritical` | `Zenith2/Tests/Lifecycle/Zenith_ResumeIntegration_Tests.cpp` | an injected `SDL_EVENT_LOW_MEMORY` raises the governor to CRITICAL at the next prologue in every lifecycle state |
| `Test_Lifecycle_BackgroundPausesAtTickBoundary` | `Zenith2/Tests/Lifecycle/Zenith_ResumeIntegration_Tests.cpp` | an injected WILL_ENTER_BACKGROUND mid-frame: the frame's ticks complete, the next frame runs zero ticks, every world stays alive, and the runtime listener's `OnReleaseSurface` returns before the handshake returns (a headless composition has no RHI device, `Zenith2/Docs/Architecture/CompositionRoot.md` [Z2-N-ARCH-867]; the device half is the Stage 3 lifecycle integration's, on a lavapipe or WARP device) |
| `Test_Thermal_StateMachineEveryRow` | `Zenith_Thermal_Tests.cpp` | a scripted headroom sequence exercises every row of the state table at the thresholds and threshold ± 0.01, including the tier cooldown, the first-match rule with its two exceptions, and the 30 s continuous-recover rule with an interrupting sample; `ApplyActions` without a new sample, status change or base-tier notification changes nothing |
| `Test_Thermal_ResolutionNeverBelowFloor` | `Zenith_Thermal_Tests.cpp` | 100 consecutive hot samples cap the scale at 0.75 and no lower |
| `Test_Thermal_ShutdownRequestsOneCheckpoint` | `Zenith_Thermal_Tests.cpp` | entering SHUTDOWN requests one checkpoint save and the lowest tier; staying in it requests no second one |
| `Test_Thermal_RebasesAfterPlayerTierChange` | `Zenith_Thermal_Tests.cpp` | with `o = 1`, a player change of `quality.tier` from DEFAULT to HIGH clears `quality.activeTier` at that prologue, and the next `ApplyActions` writes `quality.activeTier` = DEFAULT (the new choice minus one level) with source `thermal` |
| `Test_Thermal_InvalidSamplesIgnored` | `Zenith_Thermal_Tests.cpp` | NaN samples change nothing and are counted |
| `Test_Thermal_TierChangeTouchesNoSimulation` | `Zenith2/Tests/Lifecycle/Zenith_ResumeIntegration_Tests.cpp` | forcing every tier step during a 600-tick replay leaves every tick hash identical to the unforced replay |
| S24 device run: suspend and resume | device test | 50 background and foreground cycles recover the surface every time with no validation error (the Architecture table's row) |
| S24 device run: process death | device test | a process killed in the background (`adb shell am kill` after backgrounding) relaunches into the resume state; a relaunch after reinstalling a new build discards it with the build reason |
| S24 device run: thermal | device test | a 30-minute run shows resolution and tier steps following headroom (the Architecture table's row), with the counters exported |
| S24 device run: low memory | device test | `adb shell am send-trim-memory <package> RUNNING_CRITICAL` reaches the governor as CRITICAL and the shedding counters move |
| S24 device run: HWASan | device test | every ZenithPlatform test executable runs clean on the S24 in the `android-arm64-hwasan` build |
