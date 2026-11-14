# PLAT design: platform core, windows and events

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`; this file specifies the `Zenith_Platform` object, the SDL3 event pump, windows, display metrics and DPI, the clipboard, the monotonic clock, account-change signals, the development-only services and the headless test host. The SPEC baseline is `Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership" and "SDL3 responsibilities"; this file elaborates it and never contradicts it. Every obligation here is traced to a `Z2-R-PLAT-NNN` requirement in `Zenith2/Docs/Requirements/PLAT.md`.

## Scope of this file

| Topic | Heading here |
|---|---|
| creation of the platform object, SDL3 initialisation per mode | "Zenith_Platform lifecycle" |
| the pump, event translation, the event queue | "The event pump" |
| windows, multiple windows, native handles and surfaces | "Windows" |
| display metrics, DPI, safe area | "Display metrics and DPI" |
| clipboard | "Clipboard" |
| monotonic clock | "The monotonic clock" |
| the JNI handles libraries above PLAT use on Android | "Android Java context" |
| the presentation-only network connectivity signal | "Connectivity" |
| account signals | "Account changes" |
| loopback socket and file watcher | "Development services" |
| the `--timeout-seconds` watchdog | "Process watchdog" |
| the `linux-tsan` host | "The headless test host" |

The requirement that each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

<!-- informative -->
Zenith's platform layer was two sibling directories (`Zenith/Windows/`, `Zenith/Android/`) compiled into the aggregate engine library, with GLFW on Windows and a hand-written `ALooper` loop on Android, reached through the `g_xEngine` locator. Its input frame contract (a platform FIFO drained after the swapchain-acquire gate, an ordered transition log, claims resolved before gameplay) was sound and is carried over in spirit; the locator, the two windowing libraries and the platform `#ifdef`s are not. Zenith2 has one SDL3-backed library, `ZenithPlatform`, constructed by the composition root and passed by reference.
<!-- /informative -->

## Zenith_Platform lifecycle

### Creation and the desc

`Zenith_Platform` is created in startup step 4 of `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" and destroyed last among PLAT's objects, after the RHI device and the asset registry (after Contracts step 10, `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence" step 7), because the swapchain surface belongs to a platform window; the full order is `Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown".

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h

/// @contract enum Zenith_PlatformMode
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Values keep their meaning once shipped.
/// @endcontract
enum Zenith_PlatformMode : uint8
{
	PLATFORM_MODE_WINDOWED = 1,     // a main window, input devices, lifecycle, thermal; the Player's normal mode
	PLATFORM_MODE_HEADLESS,         // no window, no input device, no audio subsystem; events subsystem only
};

struct Zenith_PlatformDesc
{
	const char* m_szGameName = nullptr;              // game.json "name"; the SDL_GetPrefPath application name
	const char* m_szOrganisation = "Zenith2";        // the SDL_GetPrefPath organisation name
	const char* m_szDisplayName = nullptr;           // game.json "displayName"; the main window title
	Zenith_PlatformMode m_eMode = PLATFORM_MODE_WINDOWED;
	Zenith_WindowDesc m_xMainWindow;                 // ignored in PLATFORM_MODE_HEADLESS
	bool m_bInitAudioSubsystem = true;               // SDL_INIT_AUDIO for miniaudio's device enumeration; false when headless
	bool m_bAcceptFileDrops = false;                 // the editor sets true (PLATFORM_EVENT_DROP_FILE)
	uint32 m_uEventQueueCapacity = 4096;            // platform.eventQueueCapacity
	uint32 m_uEventQueueHardCapacity = 16384;       // platform.eventQueueHardCapacity
	uint32 m_uTransitionCapacity = 8192;            // input.transitionCapacity (Design/PLAT/Input.md → "Transition log")
	uint32 m_uTextArenaBytes = 65536;               // per-frame UTF-8 storage for text-input payloads
	uint32 m_uFrameArenaBytes = 262144;             // the pump's own frame arena (presentation mapping, claims); reset every Pump
	Zenith_Heap* m_pxHeap = nullptr;                 // the root heap; never null
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;   // tests only; nullptr in production
	Zenith_PlatformBackendHooks* m_pxBackendHooks = nullptr; // tests only: replaces the SDL event source
	Zenith_AccessibilityBackend m_eAccessibilityBackend = ACCESSIBILITY_BACKEND_AUTO;   // platform.accessibility.backend (early key; Design/PLAT/Accessibility.md)
	Zenith_StringView m_strBasePathOverride;         // tests only (Zenith_RuntimeTestHooks): empty = SDL_GetBasePath; else what
	                                                 // GetBasePath returns, copied at step 6; a Player and a tool pass empty
};

/// @contract value Zenith_AndroidJavaContext
/// Ownership: Plain value of JNI handles the platform owns: the JavaVM* (process-wide) and two JNI global references,
///   created at creation step 6 and deleted by the platform's destructor. Callers never delete them.
/// Thread affinity: IMMUTABLE. Any thread may use the handles after attaching to the VM (AttachCurrentThread).
///   Members non-const: the platform's state stores it at creation step 6, after the state exists.
/// Invalidation: Valid from Zenith_Platform::Create until the platform's destructor begins.
/// Units: None; void* so that no JNI header reaches a public header.
/// Errors: None; GetAndroidJavaContext returns PLATFORM_ERROR_NOT_AVAILABLE off Android or without an SDL activity.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
struct Zenith_AndroidJavaContext
{
	void* m_pJavaVM = nullptr;                       // JavaVM*
	void* m_pActivity = nullptr;                     // jobject: a global reference to SDL's activity
	void* m_pClassLoader = nullptr;                  // jobject: a global reference to that activity's getClassLoader()
};

enum Zenith_ConnectivityState : uint8
{
	CONNECTIVITY_STATE_UNKNOWN = 1,                  // no source on this host (Linux, headless, an OS without the API)
	CONNECTIVITY_STATE_OFFLINE,                      // no validated internet network
	CONNECTIVITY_STATE_ONLINE,                       // a validated internet network is the default
};

/// @contract type Zenith_Platform
/// Ownership: Created by Zenith_Platform::Create as a Zenith_Owned<Zenith_Platform>, owned by Zenith_Runtime for the
///   process lifetime; it owns its windows, Zenith_InputDevices, Zenith_InputSystem, Zenith_TextInput,
///   Zenith_HapticPlayer, Zenith_AppLifecycle, Zenith_ThermalMonitor, the account-signal queue and, when its backend
///   resolves to OS or RECORDING, the Zenith_AccessibilityService.
/// Thread affinity: MAIN_THREAD. PostAccountSignal and GetClockOffsetNs are ANY_THREAD (deviations below).
/// Invalidation: Pointers returned by the accessors stay valid until the platform is destroyed; a Zenith_Window*
///   from GetWindow is valid until DestroyWindow of that ID or the platform's destruction. The event span of
///   GetFrameEvents is valid until the next Pump.
/// Units: Timestamps in nanoseconds of the monotonic clock (Zenith_GetMonotonicTimeNs).
/// Errors: Zenith_Result<…, Zenith_PlatformError>; SDL failures map to PLATFORM_ERROR_SDL_INIT or
///   PLATFORM_ERROR_WINDOW_CREATE with the SDL_GetError text logged; calling a windowed-only member in
///   PLATFORM_MODE_HEADLESS returns PLATFORM_ERROR_NOT_AVAILABLE.
/// Reentrancy: NONE. Listener callbacks invoked from Pump may not call Pump or DestroyWindow.
/// Version: since Z2 Stage 2. Source API; deprecations per Contracts.md "Compatibility policy per kind".
/// @endcontract
class Zenith_Platform
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Platform>, Zenith_PlatformError> Create(
		const Zenith_PlatformDesc& xDesc);
	~Zenith_Platform();
	Zenith_Platform(const Zenith_Platform&) = delete;
	Zenith_Platform& operator=(const Zenith_Platform&) = delete;

	void Pump();                                                           // prologue step 1 (Architecture's signature)
	void WaitForEvents(uint32 uTimeoutMs);                                // throttled loops only; main thread, outside every job
	Zenith_Span<const Zenith_PlatformEvent> GetFrameEvents() const;
	uint64 GetFrameIndex() const;                           // pumps completed
	Zenith_PlatformMode GetMode() const;

	Zenith_WindowID GetMainWindowID() const;                 // null ID when headless
	Zenith_Result<Zenith_WindowID, Zenith_PlatformError> CreateWindow(const Zenith_WindowDesc& xDesc);
	Zenith_Result<void, Zenith_PlatformError> DestroyWindow(Zenith_WindowID xWindow);
	Zenith_Result<Zenith_Window*, Zenith_PlatformError> GetWindow(Zenith_WindowID xWindow);
	uint32 GetWindowCount() const;

	Zenith_InputDevices& GetInputDevices();
	Zenith_InputSystem& GetInputSystem();
	Zenith_TextInput& GetTextInput();
	Zenith_HapticPlayer& GetHaptics();
	Zenith_AppLifecycle& GetAppLifecycle();
	Zenith_ThermalMonitor& GetThermalMonitor();
	Zenith_Clipboard& GetClipboard();
	Zenith_AccessibilityService* GetAccessibility();        // null when the backend resolved to NONE (Design/PLAT/Accessibility.md)

	Zenith_Result<Zenith_DisplayList, Zenith_PlatformError> EnumerateDisplays(Zenith_Arena& xArena) const;   // Zenith_Window.h
	Zenith_StringView GetPreferredLocale() const;           // BCP 47, e.g. "en-GB"; from SDL_GetPreferredLocales
	Zenith_StringView GetBasePath() const;                   // m_strBasePathOverride when set, else SDL_GetBasePath; UTF-8, trailing '/'
	Zenith_StringView GetPrefPath() const;                   // SDL_GetPrefPath(organisation, game), trailing '/'

	/// @deviation Thread affinity: ANY_THREAD. Queues the signal; it becomes a PLATFORM_EVENT_ACCOUNT_CHANGED at the next Pump.
	Zenith_Result<void, Zenith_PlatformError> PostAccountSignal(const Zenith_AccountSignal& xSignal);
	/// @deviation Thread affinity: ANY_THREAD. The constant offset between SDL event timestamps and the monotonic clock.
	sint64 GetClockOffsetNs() const;
	/// @deviation Thread affinity: ANY_THREAD. Immutable after Create (§ "Android Java context").
	Zenith_Result<Zenith_AndroidJavaContext, Zenith_PlatformError> GetAndroidJavaContext() const;

	Zenith_ConnectivityState GetConnectivityState() const;  // as of the last Pump (§ "Connectivity")
	uint64 GetConnectivityRevision() const;                // OFFLINE→ONLINE transitions seen by Pump since Create
};
```

`Zenith_Platform::Create` runs these steps; each is a fault point of the sweep `Test_Platform_Create_FailsAtEachStep_Unwinds`, named in the table's second column (`Validate`, `SdlHints`, …, `Accessibility`) (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Partial initialisation failure"):

| Step | Fault point | Action | Unwinds on failure |
|---|---|---|---|
| 1 | `Validate` | reject a null heap, a null game name, `m_uEventQueueCapacity` of 0 or above `m_uEventQueueHardCapacity`, a hard capacity above 65,536, a transition capacity below 1,024 or above 65,536, a text arena below 4,096 bytes, a frame arena below 65,536 bytes, a non-empty `m_strBasePathOverride` that is not valid UTF-8, holds a C0 control or exceeds 4,096 bytes, `PLATFORM_ERROR_BAD_DESC` | nothing |
| 2 | `SdlHints` | set the SDL hints of "Subsystem initialisation per mode" (before `SDL_Init`) | nothing |
| 3 | `SdlInit` | `SDL_Init` with the subsystem mask of the mode; `PLATFORM_ERROR_SDL_INIT` on failure | nothing |
| 4 | `ClockOffset` | read `SDL_GetTicksNS()` and `Zenith_GetMonotonicTimeNs()` back to back three times and keep the pair with the smallest gap; `m_iClockOffsetNs = monotonic − ticks` | `SDL_Quit` |
| 5 | `EventQueue` | allocate the event ring (`m_uEventQueueCapacity` records), the transition log, the text arena and the frame arena | `SDL_Quit` |
| 6 | `Paths` | resolve the base path: a non-empty `m_strBasePathOverride` (validated in step 1) is copied into the platform's path storage with a `/` appended unless it already ends in `/` or `\`, and `SDL_GetBasePath` is not called; otherwise `SDL_GetBasePath`. Then resolve `SDL_GetPrefPath`, which creates the directory. On Android, create the Java context (§ "Android Java context"). A failure of any of these is `PLATFORM_ERROR_PATHS` | the Java context's global references, the ring, `SDL_Quit` |
| 7 | `MainWindow` | windowed mode: create the main window (§ "Window creation and destruction") | as above |
| 8 | `InputDevices` | create `Zenith_InputDevices`, enumerate already-connected gamepads (each yields a connected transition in the first pump) | window, as above |
| 9 | `InputSystem` | create `Zenith_InputSystem` with an empty action set (bindings are loaded by the runtime after the configuration is built, `Zenith2/Docs/Design/PLAT/Input.md` → "Binding layers and provenance") | devices, as above |
| 10 | `Services` | create `Zenith_TextInput`, `Zenith_HapticPlayer`, `Zenith_Clipboard`, `Zenith_AppLifecycle` (Android: installs the SDL event watch), `Zenith_ThermalMonitor` (its provider per platform), and register the connectivity source (§ "Connectivity"; a source that cannot register leaves the state UNKNOWN and is not a failure) | every earlier step, in reverse |
| 11 | `Accessibility` | resolve `m_eAccessibilityBackend` and, for OS or RECORDING, create `Zenith_AccessibilityService` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Service creation and destruction"); an OS-side failure leaves no service and is not a platform failure | every earlier step, in reverse |

The destructor runs, in order: remove the SDL event watch and release any waiting lifecycle handshake (`Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"), stop every haptic effect, end any text-input session, destroy the services in reverse creation order (the accessibility service first, `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility shutdown order"; the connectivity source is unregistered with the services, before the Java context goes), close every gamepad, destroy every window (secondary windows first, then the main window), delete the Java context's two global references, free the ring, `SDL_Quit`. No thread is started by `Zenith_Platform`; the I/O threads belong to `Zenith_AsyncIO` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API").

### Subsystem initialisation per mode

| Mode | `SDL_Init` flags | Hints set before `SDL_Init` |
|---|---|---|
| `PLATFORM_MODE_WINDOWED` (Windows, Android) | `SDL_INIT_EVENTS \| SDL_INIT_VIDEO \| SDL_INIT_GAMEPAD \| SDL_INIT_HAPTIC \| SDL_INIT_SENSOR`, plus `SDL_INIT_AUDIO` when `m_bInitAudioSubsystem` | `SDL_HINT_TOUCH_MOUSE_EVENTS=0`, `SDL_HINT_MOUSE_TOUCH_EVENTS=0`, `SDL_HINT_PEN_MOUSE_EVENTS=0`, `SDL_HINT_PEN_TOUCH_EVENTS=0`, `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS=0`, `SDL_HINT_IME_IMPLEMENTED_UI=composition` (the engine draws composition, the OS draws candidates), `SDL_HINT_ANDROID_TRAP_BACK_BUTTON=1`, `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0`, `SDL_HINT_VIDEO_ALLOW_SCREENSAVER=0` |
| `PLATFORM_MODE_HEADLESS` on Windows or Android | `SDL_INIT_EVENTS` | as above |
| the `linux-tsan` host (always headless) | `SDL_INIT_EVENTS` | as above; the SDL3 build itself has video, audio, haptic and joystick compiled out (`Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section") |

- The hint `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0` keeps SDL from blocking the main thread inside `SDL_PollEvent` while backgrounded, because the runtime's own lifecycle handling pauses the worlds and must keep pumping to see the foreground event (`Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine").
- Gamepad buttons are positional in SDL3 by design (`SDL_GAMEPAD_BUTTON_SOUTH` is always the bottom face button); the printed label comes from `SDL_GetGamepadButtonLabel` (`Zenith2/Docs/Design/PLAT/Input.md` → "Gamepad types and glyph families"), so no hint is needed.
- SDL's own threads and timers are never used; every thread and timer comes from `Zenith_Multithreading` and the monotonic clock (`Zenith2/Docs/Architecture/PlatformLayer.md` → "SDL3 responsibilities").

### Service accessors

The accessors return references to objects the platform owns. They are typed members of one aggregate that the composition root passes by reference, not a registry queried by name or type, so they are not service lookup (`Zenith2/Docs/Contracts.md` → "No mutable global state"). A module that needs a platform service receives `Zenith_Platform&` from `Zenith_ModuleStartContext::GetPlatform()` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces") and keeps the service reference as a member.

## The event pump

### Pump algorithm

`Zenith_Platform::Pump()` (the signature `Zenith2/Docs/Architecture.md` → "Purpose and scope", table "Assumed types", relies on) is step 1 of the frame prologue (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure"), named `Platform.Pump` in profiler zones, and runs on the main thread with no frame-graph job running. Steps, in order:

1. Clear the previous frame's event list, the text arena and the platform's frame arena (`m_uFrameArenaBytes`, allocated at creation step 5; exhaustion during a pump is counted in `platform.frame_arena.exhausted` and ends that frame's presentation mapping early, never an assert); clear the transition log's frame window (the log itself keeps the carry region, `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events"); increment the frame index.
2. Drain queued cross-thread inputs: account signals posted by `PostAccountSignal`, lifecycle events handed over by the Android event watch, thermal status callbacks, the haptic submissions of the previous epilogue (`Zenith2/Docs/Design/PLAT/Input.md` → "Haptics from simulation") and low-disk state changes from `Zenith_AsyncIO`. Each becomes one `Zenith_PlatformEvent` in the order it was queued; a queue that is drained holds its own `Zenith_Mutex` at `uLOCK_LEVEL_PLATFORM_CROSS_THREAD_QUEUES` (entry document → "Lock-order entries"). Then, when the platform has an accessibility service, its OS state, preference and speech cells are read under `uLOCK_LEVEL_ACCESSIBILITY`, a change yields one `PLATFORM_EVENT_ACCESSIBILITY_CHANGED`, the focus speech hold is advanced, and a lifecycle transition to `DID_ENTER_BACKGROUND` stops built-in speech (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Screen-reader detection", "Built-in text to speech"). Then the connectivity cell is read and published (§ "Connectivity"); it yields no event record.
3. `SDL_PumpEvents()`, then `SDL_PeepEvents(…, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST)` in batches of 128 until empty; for each SDL event, translate per the table "Event translation table". Events are processed strictly in SDL's queue order. Then append the transitions of the virtual-control sets, synthetic injections and input-script control events queued since the last pump, in call order, at the pump's timestamp (`Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API").
4. Pointer claims: for every new pointer press of the frame (mouse button, touch press, pen tip), call each registered `Zenith_InputClaimant::ClaimPointers` in registration order and flag the claimed pointers' transitions (`Zenith2/Docs/Design/PLAT/Input.md` → "Presentation claims").
5. Gestures: feed every unclaimed touch transition to its touchscreen's `Zenith_GestureRecogniserSet`, then advance its timers to the pump's timestamp; merge the resulting gesture transitions into the log at their timestamps (`Zenith2/Docs/Design/PLAT/Input.md` → "Gestures").
6. Close the frame's transition window: coalesce per `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log" and record the frame's device-state snapshot.
7. `Zenith_InputSystem::EvaluatePresentationFrame`: the presentation mapper over the frame's transitions, then each claimant's `ClaimActions`, then the consumed-transition flags (`Zenith2/Docs/Design/PLAT/Input.md` → "Consumption rules").
8. `Zenith_HapticPlayer::Update`: play the submissions drained in step 2 and expire finished effects; then `Zenith_ThermalMonitor` takes its sample when one is due (`Zenith2/Docs/Design/PLAT/Android.md` → "Thermal providers"), so prologue step 5 reads it with `GetLatestSample()`.
9. Publish the counters of this file (`platform.events.*`, `platform.pump_us`) through the profiler hooks.

The pump never blocks on SDL or on a lock that is not short-hold: every SDL call it makes is non-blocking (`SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0`). Its one waiting call is `Zenith_AppLifecycleListener::OnReleaseSurface` during the RUNNING → SUSPENDING transition, which finishes the GPU work that references the window and is bounded by the 2,000 ms surface-loss handshake (`Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"). Its CPU time is the pending new row "Platform pump CPU" (entry document → "Pending new rows").

### Event translation table

<!-- elaboration -->
| SDL3 event | Zenith2 record | Notes |
|---|---|---|
| `SDL_EVENT_QUIT` | `PLATFORM_EVENT_QUIT_REQUESTED` | the runtime requests shutdown, which takes effect when the current frame-loop iteration returns ([Z2-N-ARCH-560]) |
| `SDL_EVENT_WINDOW_CLOSE_REQUESTED` | `PLATFORM_EVENT_QUIT_REQUESTED` for the main window; `PLATFORM_EVENT_WINDOW_CLOSE_REQUESTED` for any other | the editor decides for secondary windows |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | `PLATFORM_EVENT_WINDOW_RESIZED` (pixel and point sizes) | a swapchain recreation trigger (`Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation") |
| `SDL_EVENT_WINDOW_MINIMIZED`, `SDL_EVENT_WINDOW_RESTORED`, `SDL_EVENT_WINDOW_MAXIMIZED` | `PLATFORM_EVENT_WINDOW_MINIMISED`, `PLATFORM_EVENT_WINDOW_RESTORED` (maximise reports resized) | |
| `SDL_EVENT_WINDOW_FOCUS_GAINED`, `SDL_EVENT_WINDOW_FOCUS_LOST` | `PLATFORM_EVENT_WINDOW_FOCUS_GAINED`, `PLATFORM_EVENT_WINDOW_FOCUS_LOST` | focus loss synthesises releases of every held keyboard key and mouse button (Input file, "Keyboard and mouse") |
| `SDL_EVENT_WINDOW_DISPLAY_CHANGED` | `PLATFORM_EVENT_WINDOW_DISPLAY_CHANGED` | the new display ID and its metrics |
| `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` | `PLATFORM_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` | § "Display scale changes" |
| `SDL_EVENT_WINDOW_SAFE_AREA_CHANGED` | `PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED` | § "Safe area" |
| `SDL_EVENT_WINDOW_OCCLUDED`, `SDL_EVENT_WINDOW_EXPOSED` | `PLATFORM_EVENT_WINDOW_OCCLUDED`, `PLATFORM_EVENT_WINDOW_EXPOSED` | |
| `SDL_EVENT_DISPLAY_ADDED`, `_REMOVED`, `_ORIENTATION`, `_CONTENT_SCALE_CHANGED` | `PLATFORM_EVENT_DISPLAY_ADDED`, `_REMOVED`, `_ORIENTATION_CHANGED`, `_SCALE_CHANGED` | |
| `SDL_EVENT_KEY_DOWN`, `_KEY_UP`, `SDL_EVENT_MOUSE_*`, `SDL_EVENT_GAMEPAD_BUTTON_*`, `SDL_EVENT_GAMEPAD_AXIS_MOTION`, `SDL_EVENT_GAMEPAD_SENSOR_UPDATE`, `SDL_EVENT_FINGER_*`, `SDL_EVENT_PEN_*` | one `PLATFORM_EVENT_INPUT_TRANSITION` per transition written to the transition log (the record holds the log index) | Input file, "Transition log" |
| `SDL_EVENT_GAMEPAD_ADDED`, `_REMOVED`, `_REMAPPED` | `PLATFORM_EVENT_INPUT_DEVICE_CONNECTED`, `_DISCONNECTED`, `_REMAPPED`, plus the transitions of "Hotplug" | |
| `SDL_EVENT_KEYBOARD_ADDED`, `_REMOVED`, `SDL_EVENT_MOUSE_ADDED`, `_REMOVED` | `PLATFORM_EVENT_INPUT_DEVICE_CONNECTED`, `_DISCONNECTED` | keyboards and mice are one logical device each (Input file, "Device kinds and IDs") |
| `SDL_EVENT_TEXT_INPUT`, `SDL_EVENT_TEXT_EDITING`, `SDL_EVENT_TEXT_EDITING_CANDIDATES` | `PLATFORM_EVENT_TEXT_INPUT` with a `Zenith_TextInputEvent` in the text arena | Input file, "Composition events" |
| `SDL_EVENT_WILL_ENTER_BACKGROUND`, `DID_ENTER_BACKGROUND`, `WILL_ENTER_FOREGROUND`, `DID_ENTER_FOREGROUND`, `TERMINATING` | handled by the event watch (Android file, "The event watch"); in the pump they only confirm the transition already queued and are otherwise ignored | the watch sees them first, synchronously |
| `SDL_EVENT_LOW_MEMORY` | `PLATFORM_EVENT_LOW_MEMORY` | relayed to the memory governor by the runtime (Android file, "Low memory relay") |
| `SDL_EVENT_LOCALE_CHANGED` | `PLATFORM_EVENT_LOCALE_CHANGED` | the new `GetPreferredLocale()` |
| `SDL_EVENT_CLIPBOARD_UPDATE` | `PLATFORM_EVENT_CLIPBOARD_UPDATED` | |
| `SDL_EVENT_DROP_FILE` | `PLATFORM_EVENT_DROP_FILE` with the host path in the text arena, only when `m_bAcceptFileDrops`; ignored otherwise | a host path, never a virtual path; the editor alone consumes it |
| `SDL_EVENT_SYSTEM_THEME_CHANGED` and every other SDL event | ignored, counted in `platform.events.ignored` | |

The account, thermal, low-disk, configuration-reset and accessibility records come from step 2 of "Pump algorithm", never from SDL.

### Zenith_PlatformEvent

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h

enum Zenith_PlatformEventKind : uint16
{
	PLATFORM_EVENT_NONE = 0,
	PLATFORM_EVENT_QUIT_REQUESTED = 1,
	PLATFORM_EVENT_WINDOW_CLOSE_REQUESTED,
	PLATFORM_EVENT_WINDOW_RESIZED,
	PLATFORM_EVENT_WINDOW_MINIMISED,
	PLATFORM_EVENT_WINDOW_RESTORED,
	PLATFORM_EVENT_WINDOW_FOCUS_GAINED,
	PLATFORM_EVENT_WINDOW_FOCUS_LOST,
	PLATFORM_EVENT_WINDOW_DISPLAY_CHANGED,
	PLATFORM_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
	PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED,
	PLATFORM_EVENT_WINDOW_OCCLUDED,
	PLATFORM_EVENT_WINDOW_EXPOSED,
	PLATFORM_EVENT_DISPLAY_ADDED,
	PLATFORM_EVENT_DISPLAY_REMOVED,
	PLATFORM_EVENT_DISPLAY_ORIENTATION_CHANGED,
	PLATFORM_EVENT_DISPLAY_SCALE_CHANGED,
	PLATFORM_EVENT_INPUT_TRANSITION,
	PLATFORM_EVENT_INPUT_DEVICE_CONNECTED,
	PLATFORM_EVENT_INPUT_DEVICE_DISCONNECTED,
	PLATFORM_EVENT_INPUT_DEVICE_REMAPPED,
	PLATFORM_EVENT_INPUT_SCHEME_CHANGED,
	PLATFORM_EVENT_TEXT_INPUT,
	PLATFORM_EVENT_LIFECYCLE,
	PLATFORM_EVENT_SURFACE_LOST,
	PLATFORM_EVENT_SURFACE_AVAILABLE,
	PLATFORM_EVENT_LOW_MEMORY,
	PLATFORM_EVENT_LOW_DISK,
	PLATFORM_EVENT_LOW_DISK_CLEARED,
	PLATFORM_EVENT_ACCOUNT_CHANGED,
	PLATFORM_EVENT_CONFIG_RESET,
	PLATFORM_EVENT_THERMAL_STATUS_CHANGED,
	PLATFORM_EVENT_LOCALE_CHANGED,
	PLATFORM_EVENT_CLIPBOARD_UPDATED,
	PLATFORM_EVENT_DROP_FILE,
	PLATFORM_EVENT_ACCESSIBILITY_CHANGED,   // appended in Z2 Stage 4; presentation-only (Design/PLAT/Accessibility.md "Screen-reader detection")
	PLATFORM_EVENT_KIND_COUNT,
};

struct Zenith_PlatformWindowPayload { uint32 m_uPixelWidth; uint32 m_uPixelHeight; uint32 m_uPointWidth; uint32 m_uPointHeight; uint32 m_uDisplayID; float m_fDisplayScale; };
struct Zenith_PlatformDevicePayload { Zenith_InputDeviceID m_xDevice; Zenith_InputDeviceKind m_eKind; Zenith_GamepadType m_eGamepadType; uint8 m_uPlayerSlot; uint8 m_uFlags; };   // uINPUT_DEVICE_FLAG_*
struct Zenith_PlatformTransitionPayload { uint32 m_uTransitionIndex; };
struct Zenith_PlatformTextPayload { uint32 m_uTextEventIndex; };
struct Zenith_PlatformLifecyclePayload { Zenith_AppLifecycleState m_eFrom; Zenith_AppLifecycleState m_eTo; };
struct Zenith_PlatformDiskPayload { uint64 m_ulFreeBytes; uint64 m_ulMarginBytes; Zenith_LowDiskReason m_eReason; };
struct Zenith_PlatformAccountPayload { Zenith_AccountSignalKind m_eKind; Zenith_AccountSignalSource m_eSource; uint64 m_ulAccountKey; };
struct Zenith_PlatformConfigPayload { Zenith_ConfigLayer m_eLayer; Zenith_ConfigResetReason m_eReason; };
struct Zenith_PlatformThermalPayload { Zenith_ThermalStatus m_eFrom; Zenith_ThermalStatus m_eTo; float m_fHeadroom; };
struct Zenith_PlatformStringPayload { uint32 m_uTextOffset; uint32 m_uTextBytes; };   // into the frame's text arena
struct Zenith_PlatformAccessibilityPayload { bool m_bScreenReaderActive; bool m_bTreeRequested; bool m_bSpeechAvailable; uint8 m_uPreferenceChangedMask; };   // uACCESSIBILITY_PREFERENCE_*

/// @contract value Zenith_PlatformEvent
/// Ownership: Plain value in the platform's event ring; the ring is reused every frame.
/// Thread affinity: IMMUTABLE after Pump returns. Members non-const: an element of the event ring, reused every frame.
/// Invalidation: The record, and the text-arena bytes a string payload names, are invalid after the next Pump.
/// Units: m_ulTimestampNs in nanoseconds of the monotonic clock; sizes in pixels and points as named.
/// Errors: None.
/// Version: since Z2 Stage 2. The record itself is never serialized; kinds keep their value once shipped, new kinds are
///   appended. The field order and widths of the payload structs of the eight world-visible kinds are part of the ZRPL
///   format version (they are the 32 kind-specific bytes of the recorded publication, Zenith2/Docs/Design/PLAT.md
///   "Frame-graph placement"); changing one bumps that version.
/// @endcontract
struct Zenith_PlatformEvent
{
	Zenith_PlatformEventKind m_eKind = PLATFORM_EVENT_NONE;
	uint16 m_uFlags = 0;                 // PLATFORM_EVENT_FLAG_WORLD_VISIBLE (bit 0): delivered to worlds, see below
	uint32 m_uReserved = 0;
	uint64 m_ulTimestampNs = 0;
	uint64 m_ulSequence = 0;             // process-wide monotonic, shared with the transition log
	Zenith_WindowID m_xWindow;            // null for events not tied to a window
	union
	{
		Zenith_PlatformWindowPayload m_xWindowData;
		Zenith_PlatformDevicePayload m_xDevice;
		Zenith_PlatformTransitionPayload m_xTransition;
		Zenith_PlatformTextPayload m_xText;
		Zenith_PlatformLifecyclePayload m_xLifecycle;
		Zenith_PlatformDiskPayload m_xDisk;
		Zenith_PlatformAccountPayload m_xAccount;
		Zenith_PlatformConfigPayload m_xConfig;
		Zenith_PlatformThermalPayload m_xThermal;
		Zenith_PlatformStringPayload m_xString;
		Zenith_PlatformAccessibilityPayload m_xAccessibility;
		uint8 m_auRaw[32];
	};
};
static_assert(sizeof(Zenith_PlatformEvent) == 64);

inline constexpr uint16 uPLATFORM_EVENT_FLAG_WORLD_VISIBLE = 1u << 0;
constexpr const char* Zenith_ToString(Zenith_PlatformEventKind eKind);
```

The payload member that is active is determined by `m_eKind` alone; `m_auRaw` exists so that the ring can be zeroed and copied bytewise. Every payload struct is trivially copyable.

World-visible kinds (the flag is set by the pump, and the runtime bridge delivers them to every world at the next tick boundary as publications, entry document → "Frame-graph placement"): `PLATFORM_EVENT_INPUT_DEVICE_CONNECTED`, `_DISCONNECTED`, `PLATFORM_EVENT_INPUT_SCHEME_CHANGED`, `PLATFORM_EVENT_LOW_DISK`, `PLATFORM_EVENT_LOW_DISK_CLEARED`, `PLATFORM_EVENT_ACCOUNT_CHANGED`, `PLATFORM_EVENT_CONFIG_RESET`, `PLATFORM_EVENT_LOCALE_CHANGED`. Every other kind is presentation-only: it reaches the runtime, the renderer, UI and the editor through `GetFrameEvents()` and never reaches simulation.

### Event queue and overflow

| Queue | Capacity | Full policy | Counters |
|---|---|---|---|
| the frame event ring | `m_uEventQueueCapacity` (4,096), growing by doubling to `m_uEventQueueHardCapacity` (16,384) | `QUEUE_FULL_POLICY_GROW_THEN_REJECT`; a rejected SDL event is dropped, except that a rejected window, device or lifecycle record replaces the oldest `PLATFORM_EVENT_INPUT_TRANSITION` record of a motion transition (the transition stays in the log, only its event record is dropped) | `platform.events.full`, `platform.events.dropped`, `platform.events.high_water` |
| the account-signal queue | 64 | `QUEUE_FULL_POLICY_DROP_OLDEST` (the pump drains every frame; on overflow the oldest signal is dropped) | `platform.account_signals.full`, `.dropped`, `.high_water` |
| the lifecycle hand-over queue | 32 | `QUEUE_FULL_POLICY_FATAL` (more than 32 lifecycle events in one frame is a defect) | `platform.lifecycle_queue.full`, `.dropped`, `.high_water` |
| the low-disk state queue | 8 | `QUEUE_FULL_POLICY_DROP_OLDEST` (only the latest state matters) | `platform.low_disk_queue.full`, `.dropped`, `.high_water` |
| the haptic submission queue (`Zenith_HapticPlayer::Submit`, `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API") | 256 | `QUEUE_FULL_POLICY_DROP_NEWEST` | `input.haptics.submissions.full`, `.dropped`, `.high_water` |
| the file watcher's drain queue (development builds, § "File watcher") | 4,096 | `QUEUE_FULL_POLICY_DROP_OLDEST` (an overflow adds the root rescan record) | `devtools.watcher_queue.full`, `.dropped`, `.high_water` |
| the five I/O priority queues (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "I/O scheduler algorithm") | `io.queueCapacity` (1,024) each | `QUEUE_FULL_POLICY_REJECT` | `io.queue.<priority>.full`, `.dropped`, `.high_water` |
| the accessibility action queue (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Action queue") | 64 | `QUEUE_FULL_POLICY_DROP_OLDEST` (UI drains every pump) | `platform.accessibility.actions.full`, `.dropped`, `.high_water` |
| the Windows speech command queue (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Windows SAPI speech thread") | 32 | `QUEUE_FULL_POLICY_DROP_OLDEST` | `platform.accessibility.speech_queue.full`, `.dropped`, `.high_water` |
| the recording backend's record log (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Record log") | 4,096 records, 1 MiB of text | `QUEUE_FULL_POLICY_DROP_NEWEST` (logged records keep valid text) | `platform.accessibility.recording.full`, `.dropped`, `.high_water` |

Every bounded queue counts its full events, drops and high-water mark under `<queue name>.full`, `<queue name>.dropped` and `<queue name>.high_water`, all cumulative ([Z2-N-CONTRACT-345] and [Z2-N-CONTRACT-155] of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Bounded queues"; entry document → "Counters"). One counting rule holds for every queue PLAT owns. `.full` counts a push the queue refuses back to its caller, `.dropped` counts an element the queue loses, and `.high_water` is the largest count reached. A queue on a CORE container (the cross-thread rings, the file watcher's drain queue, the haptic submission queue, the asynchronous-I/O completion queues' inner queues) counts exactly as `Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_RingBuffer" tabulates: a `REJECT` push, or a `GROW_THEN_REJECT` push at the hard capacity, increments `.full` only; a `DROP_OLDEST` or `DROP_NEWEST` overflow increments `.dropped` only; a `FATAL` queue counts neither, because a full event ends the process. PLAT's own queues follow the same reading: the five I/O priority queues and the frame event ring count each rejection in both `.full` and `.dropped`, because the rejected request or event record is refused and is not queued anywhere else (the frame event ring's displaced motion record counts as the one lost record); the asynchronous-I/O completion reservation counts a refused reservation in `.full` only, because no completion record exists to lose; the world's haptic request queue (`Zenith2/Docs/Design/PLAT/Input.md` → "Haptics from simulation") drops its newest request and counts `.dropped` only, as CORE's `DROP_NEWEST` does.

The transition log's capacity and its overflow rules are in `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log". Every capacity above is a configuration key registered by PLAT (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers"); the defaults are derived from bounded inputs in this file (at most a few hundred window and device events per frame even under a hot-plug storm) and are not budget rows (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Bounded queues").

## Windows

### Zenith_Window API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Window.h

/// @contract value Zenith_WindowID
/// Ownership: Plain value naming one window of one Zenith_Platform.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: Stale after DestroyWindow; Zenith_Platform::GetWindow then returns PLATFORM_ERROR_UNKNOWN_WINDOW.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. Never serialized.
/// @endcontract
struct Zenith_WindowID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Zenith_WindowID, Zenith_WindowID) = default;
};

enum Zenith_WindowMode : uint8
{
	WINDOW_MODE_WINDOWED = 1,
	WINDOW_MODE_BORDERLESS_FULLSCREEN,     // desktop resolution, no mode change; the default on Android
	WINDOW_MODE_EXCLUSIVE_FULLSCREEN,      // Windows only; a display mode change through SDL_SetWindowFullscreenMode
};

enum Zenith_NativeWindowKind : uint8
{
	NATIVE_WINDOW_KIND_NONE = 0,
	NATIVE_WINDOW_KIND_WIN32 = 1,          // m_pWindow = HWND, m_pInstance = HINSTANCE
	NATIVE_WINDOW_KIND_ANDROID,            // m_pWindow = ANativeWindow*, m_pInstance = nullptr
};

struct Zenith_WindowDesc
{
	const char* m_szTitle = nullptr;       // UTF-8
	uint32 m_uWidthPoints = 1280;
	uint32 m_uHeightPoints = 720;
	Zenith_WindowMode m_eMode = WINDOW_MODE_WINDOWED;
	uint32 m_uDisplayIndex = 0;           // index into EnumerateDisplays; 0 = primary
	bool m_bResizable = true;
	bool m_bHighPixelDensity = true;       // SDL_WINDOW_HIGH_PIXEL_DENSITY: the back buffer is in pixels, never scaled by the OS
	bool m_bHidden = false;                // created hidden (editor secondary windows until placed)
	bool m_bBorderless = false;            // editor docking viewports drawn by ImGui
	bool m_bVulkan = true;                 // SDL_WINDOW_VULKAN; false only for a D3D12-only window
};

struct Zenith_NativeWindowHandle
{
	Zenith_NativeWindowKind m_eKind = NATIVE_WINDOW_KIND_NONE;
	void* m_pWindow = nullptr;
	void* m_pInstance = nullptr;
};

struct Zenith_SafeAreaInsets { uint32 m_uLeftPixels = 0; uint32 m_uTopPixels = 0; uint32 m_uRightPixels = 0; uint32 m_uBottomPixels = 0; };

struct Zenith_DisplayMetrics
{
	uint32 m_uDisplayID = 0;              // SDL_DisplayID, stable while the display is connected
	uint32 m_uPixelWidth = 0;             // drawable size in physical pixels
	uint32 m_uPixelHeight = 0;
	uint32 m_uPointWidth = 0;             // window size in points (SDL window coordinates)
	uint32 m_uPointHeight = 0;
	float m_fPixelDensity = 1.0f;          // pixels per point: SDL_GetWindowPixelDensity
	float m_fDisplayScale = 1.0f;          // the OS content scale for UI: SDL_GetWindowDisplayScale
	float m_fRefreshHz = 60.0f;            // the display mode's refresh rate; 0 when unknown
	Zenith_SafeAreaInsets m_xSafeArea;     // in pixels, from SDL_GetWindowSafeArea
};

struct Zenith_DisplayInfo              // one connected display, from SDL_GetDisplays in SDL's order
{
	uint32 m_uDisplayID = 0;
	Zenith_StringView m_strName;           // SDL_GetDisplayName, in the caller's arena
	sint32 m_iBoundsX = 0, m_iBoundsY = 0; // desktop bounds in points (SDL_GetDisplayBounds)
	uint32 m_uPointWidth = 0, m_uPointHeight = 0;
	float m_fContentScale = 1.0f;          // SDL_GetDisplayContentScale
	float m_fRefreshHz = 0.0f;             // the current mode; 0 when unknown
	bool m_bPrimary = false;
};
using Zenith_DisplayList = Zenith_Span<const Zenith_DisplayInfo>;   // in the arena passed to EnumerateDisplays

/// @contract type Zenith_Window
/// Ownership: Owned by Zenith_Platform; created by CreateWindow (or as the main window by Create) and destroyed by
///   DestroyWindow or the platform's destruction. Callers hold a Zenith_WindowID and borrow Zenith_Window* per call.
/// Thread affinity: MAIN_THREAD. GetNativeHandle and GetMetrics are ANY_THREAD (they read a copy updated in Pump).
/// Invalidation: The native handle is invalid after PLATFORM_EVENT_SURFACE_LOST until PLATFORM_EVENT_SURFACE_AVAILABLE
///   (Android) and after DestroyWindow; a Vulkan surface created from it must be destroyed before either.
/// Units: Sizes in pixels or points as named; scale factors are ratios.
/// Errors: Zenith_Result<…, Zenith_PlatformError>; PLATFORM_ERROR_VULKAN_SURFACE carries the SDL_GetError text in the log.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_Window
{
public:
	Zenith_WindowID GetID() const;
	Zenith_DisplayMetrics GetMetrics() const;
	Zenith_NativeWindowHandle GetNativeHandle() const;
	bool IsSurfaceAvailable() const;           // false between surface loss and recovery
	bool IsMinimised() const;
	bool HasInputFocus() const;
	Zenith_WindowMode GetMode() const;
	Zenith_Result<void, Zenith_PlatformError> SetMode(Zenith_WindowMode eMode);
	Zenith_Result<void, Zenith_PlatformError> SetTitle(Zenith_StringView strTitle);
	Zenith_Result<void, Zenith_PlatformError> SetPositionPoints(sint32 iX, sint32 iY);
	Zenith_Result<void, Zenith_PlatformError> SetSizePoints(uint32 uWidth, uint32 uHeight);
	void Show();
	void Hide();
	void Raise();
	Zenith_Result<void, Zenith_PlatformError> SetRelativeMouseMode(bool bEnabled);
	bool IsRelativeMouseMode() const;

	// Vulkan support without Vulkan types in the public header: the RHI passes its VkInstance and receives a
	// VkSurfaceKHR as a 64-bit non-dispatchable handle value.
	Zenith_Result<uint64, Zenith_PlatformError> CreateVulkanSurface(void* pVkInstance) const;
	void DestroyVulkanSurface(void* pVkInstance, uint64 ulSurface) const;
};

// Vulkan instance bootstrap for ZenithRHI_Vulkan (volkInitializeCustom and instance extensions):
Zenith_Result<void*, Zenith_PlatformError> Zenith_GetVulkanInstanceProcAddr(Zenith_Platform& xPlatform);
Zenith_Result<Zenith_Span<const char* const>, Zenith_PlatformError> Zenith_GetVulkanInstanceExtensions(
	Zenith_Platform& xPlatform);
```

- `CreateVulkanSurface` calls `SDL_Vulkan_CreateSurface` inside `ZenithPlatform/Internal/`; `ZenithRHI_Vulkan` initialises volk with the pointer from `Zenith_GetVulkanInstanceProcAddr` (`SDL_Vulkan_GetVkGetInstanceProcAddr`) so that SDL and volk resolve the same loader; the D3D12 backend reads `NATIVE_WINDOW_KIND_WIN32` from `GetNativeHandle` (`SDL_PROP_WINDOW_WIN32_HWND_POINTER`).
- On Android `GetNativeHandle` returns `SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER` while the surface is available and `NATIVE_WINDOW_KIND_NONE` otherwise.

### Window creation and destruction

1. Validate the desc: a null title is replaced by the platform desc's display name; sizes are clamped to `[320, 16384]` points; `WINDOW_MODE_EXCLUSIVE_FULLSCREEN` on Android is treated as `WINDOW_MODE_BORDERLESS_FULLSCREEN`; a display index beyond the display count uses the primary display.
2. When the live window count equals `uPLATFORM_MAX_WINDOWS` (16), fail with `PLATFORM_ERROR_TOO_MANY_WINDOWS`.
3. `SDL_CreateWindow` with `SDL_WINDOW_HIGH_PIXEL_DENSITY` when requested, `SDL_WINDOW_RESIZABLE`, `SDL_WINDOW_HIDDEN`, `SDL_WINDOW_BORDERLESS`, `SDL_WINDOW_VULKAN` per the desc; failure is `PLATFORM_ERROR_WINDOW_CREATE`.
4. Allocate a slot in the platform's `Zenith_SlotTable<Zenith_WindowRecord>` (`Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"), whose `m_uMaxSlots` is 16; store the SDL window pointer, its `SDL_WindowID`, the cached metrics and the surface flag.
5. Apply the mode; record the initial metrics.
6. Return the ID. The first pump after creation emits `PLATFORM_EVENT_WINDOW_RESIZED` for it, so every consumer learns its size through the same path.

`DestroyWindow` refuses the main window (`PLATFORM_ERROR_MAIN_WINDOW`); for any other ID it ends a text-input session bound to that window, releases the keys and buttons whose focus was that window, calls `SDL_DestroyWindow` and frees the slot. The caller destroys any swapchain on that window first; the RHI's swapchain holds the `Zenith_WindowID` and checks it every frame.

### Multiple windows

Multiple windows serve the editor: the main editor window, detached docking viewports and extra scene views (`Zenith2/Docs/Rendering.md` → "Render views", `VIEW_KIND_EDITOR`). A shipping game uses the main window only.

| Rule | Behaviour |
|---|---|
| limit | at most 16 live windows (`uPLATFORM_MAX_WINDOWS`), the main window included |
| event routing | every window, text and input record carries the `Zenith_WindowID` SDL reports (`windowID` of the SDL event, mapped through the slot table); an event for an unknown `SDL_WindowID` (a window destroyed this frame) is dropped and counted in `platform.events.stale_window` |
| input focus | exactly one window has keyboard focus at a time; mouse and pen positions are reported in the coordinates of the window under the pointer (`Zenith2/Docs/Design/PLAT/Input.md` → "Keyboard and mouse") |
| contexts per window | a presentation input context may be scoped to a window (`Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks"), so the editor's viewport navigation binds only while that viewport has focus |
| swapchains | one swapchain per window that renders; each window's resize and display-scale events trigger only its own swapchain's recreation |
| surface loss | Android has one window; the multi-window path is Windows-only in practice and is still built on Android |

### Native handles for the RHI

| Backend | What it reads | When |
|---|---|---|
| Vulkan (Windows, Android) | `Zenith_GetVulkanInstanceProcAddr`, `Zenith_GetVulkanInstanceExtensions`, then `Zenith_Window::CreateVulkanSurface` | device creation (startup step 12), and swapchain recreation after surface recovery |
| D3D12 | `GetNativeHandle()` with `NATIVE_WINDOW_KIND_WIN32` | swapchain creation (`IDXGIFactory2::CreateSwapChainForHwnd`) |

### Fullscreen and display modes

- `WINDOW_MODE_BORDERLESS_FULLSCREEN` uses `SDL_SetWindowFullscreen(true)` with a null fullscreen mode (desktop resolution); it is the default for the Player on Windows when `platform.windowMode` is `borderless` and always on Android.
- `WINDOW_MODE_EXCLUSIVE_FULLSCREEN` picks the display mode closest to the requested pixel size and `platform.refreshHz` with `SDL_GetClosestFullscreenDisplayMode`, then `SDL_SetWindowFullscreenMode`; a failure falls back to borderless with a warning.
- A mode change emits `PLATFORM_EVENT_WINDOW_RESIZED`; HDR output toggles are a renderer setting and not a window mode.

## Display metrics and DPI

### Units points pixels and dp

| Unit | Definition | Used by |
|---|---|---|
| pixel | one physical pixel of the drawable surface | the RHI, render extents, touch positions in pixels |
| point | an SDL window coordinate; `pixel = point × m_fPixelDensity` | window placement, mouse positions |
| dp (density-independent pixel) | `1 dp = m_fDisplayScale / m_fPixelDensity` points, i.e. `m_fDisplayScale` pixels | gesture thresholds, UI layout units (UI area), touch slop |

- `m_fDisplayScale` is the scale the OS asks UI to use (Windows: the monitor's scaling percentage divided by 100; Android: `DisplayMetrics.density`); `m_fPixelDensity` is 1 on Windows with `SDL_WINDOW_HIGH_PIXEL_DENSITY` (SDL3 reports points equal to pixels there) and the ratio SDL reports elsewhere.
- UI scaling (Z2-I-UI-10) multiplies `m_fDisplayScale` by the user's UI scale setting; PLAT provides only the metric.

### Display scale changes

1. The pump translates `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` and `SDL_EVENT_WINDOW_DISPLAY_CHANGED` into records carrying the new `Zenith_DisplayMetrics`.
2. The cached metrics of the window are updated before the record is appended, so every consumer that reads `GetMetrics()` after the pump sees the new values.
3. The renderer treats the record as a swapchain recreation trigger ("window resize, DPI change, output change" in `Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation"); UI relayouts; gesture thresholds in dp are recomputed by `Zenith_InputSystem` at the same pump.
4. Moving the window between monitors of different scale on Windows also resizes the window's pixel size (SDL handles `WM_DPICHANGED`); the resize record follows the scale record in the same pump.

### Safe area

`m_xSafeArea` is the inset, in pixels, of the region free of display cut-outs, rounded corners and system bars (`SDL_GetWindowSafeArea`, converted from points). It is zero on Windows. It changes on rotation and on system-bar visibility changes (`PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED`). UI lays out interactive elements inside it; the renderer draws the whole surface.

## Clipboard

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h

/// @contract type Zenith_Clipboard
/// Ownership: Owned by Zenith_Platform.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: The view returned by GetText is valid until the next Pump.
/// Units: Byte lengths of UTF-8 text.
/// Errors: PLATFORM_ERROR_CLIPBOARD when SDL reports failure; text that is not valid UTF-8 is rejected with
///   PLATFORM_ERROR_BAD_TEXT and never stored.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_Clipboard
{
public:
	Zenith_Result<void, Zenith_PlatformError> SetText(Zenith_StringView strText);   // at most 1 MiB
	Zenith_Result<Zenith_StringView, Zenith_PlatformError> GetText(Zenith_Arena& xArena);
	bool HasText() const;
};
```

Clipboard text is untrusted input: `GetText` validates UTF-8 and truncates at 1 MiB (`platform.clipboardMaxBytes`), counting truncations in `platform.clipboard.truncated`. The clipboard is presentation-only; nothing in simulation reads it.

## The monotonic clock

The monotonic and wall clocks are ZenithCore's (`Zenith2/Engine/ZenithCore/Zenith_Clock.h`, specified in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"); ZenithPlatform declares no clock function (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership") and calls `Zenith_GetMonotonicTimeNs` and `Zenith_GetMonotonicTimeUs` from ZenithCore. What PLAT adds is the single time base of every platform record:

- SDL event timestamps (`SDL_GetTicksNS` base) are converted by adding `GetClockOffsetNs()` from step 4 of the creation table, so every platform event, input transition, gesture timer, haptic duration, I/O deadline and the resume-write deadline is on ZenithCore's monotonic clock.
- The monotonic clock does not advance while an Android device is suspended (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"), so a timestamp gap across a background period is shorter than the wall time; every active gesture recogniser is cancelled on entering the background rather than timed across it (`Zenith2/Docs/Design/PLAT/Input.md` → "Gesture arbitration").
- Simulation never reads a clock (`Zenith2/Docs/Architecture.md` → "Determinism guarantee and mechanisms"); PLAT timestamps reach simulation only as the event sequence numbers of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Latency attribution", never as times.

## Android Java context

Libraries above ZenithPlatform that call Java on Android receive the JNI handles from the platform and never call SDL. ZenithPackage uses them for its Play Asset Delivery bridge (`Zenith2/Docs/Design/PACK/Tooling.md` → "Java bridge"). ZenithAudio uses them for its audio-focus bridge (`Zenith2/Docs/Design/AUDIO/Device.md` → "Focus Java bridge"). `GetAndroidJavaContext` is the one accessor; there is no per-handle accessor.

Creation (Android, inside creation step 6):

1. `JNIEnv* pEnv = (JNIEnv*)SDL_GetAndroidJNIEnv()` on the main thread; then `pEnv->GetJavaVM(&pVM)`.
2. `jobject xLocal = (jobject)SDL_GetAndroidActivity()`, which SDL3 returns as a local reference. A null result means a process with no SDL activity, such as a native test executable run through `adb shell`. The context is then left empty and `GetAndroidJavaContext` returns `PLATFORM_ERROR_NOT_AVAILABLE`; this is not a creation failure.
3. `m_pActivity = pEnv->NewGlobalRef(xLocal)`, then `DeleteLocalRef(xLocal)`.
4. Call `getClassLoader()` on the activity (`GetObjectClass`, then `GetMethodID(…, "getClassLoader", "()Ljava/lang/ClassLoader;")`, then `CallObjectMethod`). Then `m_pClassLoader = NewGlobalRef(result)` and `DeleteLocalRef(result)`.
5. A pending Java exception, or a null result in steps 3–4, triggers `ExceptionClear`, deletes any global reference already made, and makes the step fail with `PLATFORM_ERROR_PATHS`. The step's fault point `Paths` covers it.

Use:

- A caller on a thread that is not attached calls `AttachCurrentThread` on `m_pJavaVM` and detaches before the thread exits.
- Application classes are loaded with `ClassLoader.loadClass` on `m_pClassLoader`, never with `FindClass` from a native thread. `FindClass` there sees only the system class loader.
- The handles are immutable after `Create`, so the accessor is ANY_THREAD and takes no lock.
- The destructor deletes both global references after every service, and after every window, because a listener may still name the activity until then.
- Callers must have released their own references by the platform's destruction, after Contracts step 10. ZenithPackage's Play delivery (shutdown step 6) and ZenithAudio's device (its module's `Stop`) do so.
- Off Android, the accessor returns `PLATFORM_ERROR_NOT_AVAILABLE` and the context is all null.

## Connectivity

A presentation-only connectivity signal lets a consumer retry network work when the device regains a network, instead of waiting for a back-off. Its first consumer is PACK's network retry, which raises its retry event N when `GetConnectivityRevision()` advanced since its last prologue (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Network retry"). Simulation never reads it.

| Host | Source | Delivery |
|---|---|---|
| Windows | `NotifyNetworkConnectivityHintChange` (iphlpapi; Windows 10 2004 or later, resolved with `GetProcAddress`, so an older OS leaves UNKNOWN), registered at creation step 10 with `InitialNotification = TRUE`; `NL_NETWORK_CONNECTIVITY_LEVEL_HINT_INTERNET_ACCESS` and `CONSTRAINED_INTERNET_ACCESS` → ONLINE; `NONE` and `LOCAL_ACCESS` → OFFLINE; `UNKNOWN` and `HIDDEN` → UNKNOWN | the callback runs on an OS thread-pool thread and stores the state into an atomic cell (`std::atomic<uint8>`, release); `CancelMibChangeNotify2` in the destructor waits for a callback in progress |
| Android | the Java class `org.zenith2.player.Zenith2Connectivity` in the Gradle template, constructed by the platform at creation step 10 through the Java context's class loader with the cell's address as a `long` handle; it registers `ConnectivityManager.registerDefaultNetworkCallback`, and `onCapabilitiesChanged` with `NET_CAPABILITY_VALIDATED` and `NET_CAPABILITY_INTERNET` → ONLINE, `onLost` and `onUnavailable` → OFFLINE, call the native `nativeConnectivityChanged(long handle, int state)` | the native method stores into the same atomic cell; `detach()` (called by the destructor, `synchronized` with the callbacks, as the accessibility bridge's handshake of `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android bridge lifetime") unregisters the callback and zeroes the handle, so no native call runs after it returns; a missing class (a template without it) leaves UNKNOWN and logs `platform: connectivity bridge unavailable` once |
| Linux, `PLATFORM_MODE_HEADLESS` on any host | none | UNKNOWN for the whole run |

- Pump step 2 reads the cell with acquire ordering. When the state differs from the last pump's, the new state is published. An OFFLINE → ONLINE change also increments the revision, and counts in `platform.connectivity.regained`. UNKNOWN → ONLINE does not count as regained, so a start with a network never triggers a retry.
- The getters return the published values, are MAIN_THREAD, and cost a load.
- Tests replace the source through `Zenith_PlatformBackendHooks::GetConnectivity`. A `true` return overrides the cell for that pump.
- The Android class needs `android.permission.ACCESS_NETWORK_STATE`, a normal permission with no prompt. MOD's Gradle template declares it beside the class.

## Account changes

### Account signal sources

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h
enum Zenith_AccountSignalKind : uint8
{
	ACCOUNT_SIGNAL_SIGNED_IN = 1,
	ACCOUNT_SIGNAL_SIGNED_OUT,
	ACCOUNT_SIGNAL_SWITCHED,          // a different account is now active
	ACCOUNT_SIGNAL_SESSION_LOCKED,    // Windows session lock or console disconnect
	ACCOUNT_SIGNAL_SESSION_UNLOCKED,
};

enum Zenith_AccountSignalSource : uint8
{
	ACCOUNT_SIGNAL_SOURCE_OS = 1,         // Windows session notifications
	ACCOUNT_SIGNAL_SOURCE_ONLINE_BACKEND, // posted by a ZenithOnline backend (Google Play Games, Steam) or the offline backend's tests
};

struct Zenith_AccountSignal
{
	Zenith_AccountSignalKind m_eKind = ACCOUNT_SIGNAL_SIGNED_IN;
	Zenith_AccountSignalSource m_eSource = ACCOUNT_SIGNAL_SOURCE_OS;
	uint64 m_ulAccountKey = 0;           // Zenith_Hash64 of the platform account identifier; 0 when signed out; never the raw ID
	uint64 m_ulTimestampNs = 0;          // filled by PostAccountSignal when 0
};
```

| Source | Mechanism | Signals |
|---|---|---|
| Windows session | `WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)` on the main window, handled through an SDL Windows message hook (`SDL_SetWindowsMessageHook`) in `Internal/Windows/Zenith_AccountSignals_Windows.cpp`; `WM_WTSSESSION_CHANGE` with `WTS_SESSION_LOCK`, `WTS_SESSION_UNLOCK`, `WTS_CONSOLE_DISCONNECT`, `WTS_CONSOLE_CONNECT`, `WTS_SESSION_LOGOFF` | `SESSION_LOCKED`, `SESSION_UNLOCKED`, `SIGNED_OUT` (logoff) |
| online backends | `Zenith_Platform::PostAccountSignal` from any thread, called by the backend when its identity service reports a sign-in, sign-out or account switch (`Zenith2/Docs/Design/ONLINE.md` (pending; its "Public API" section)) | `SIGNED_IN`, `SIGNED_OUT`, `SWITCHED` |
| Android | Android has no OS-level account-switch signal to a running app for game accounts; the profile's account changes reach the game only through the online backend | as online backends |

### Account event delivery

1. `PostAccountSignal` validates the kind and source (`PLATFORM_ERROR_BAD_SIGNAL` otherwise), stamps the time and pushes onto the account-signal queue.
2. The next pump converts each queued signal into `PLATFORM_EVENT_ACCOUNT_CHANGED` with `uPLATFORM_EVENT_FLAG_WORLD_VISIBLE`.
3. The runtime bridge hands the record to the online-services adapter first (so saves and entitlements rebind before gameplay reacts, `Zenith2/Docs/Design/ONLINE.md` (pending; its "Algorithms and state machines" section)) and then publishes it to every world at the next tick boundary as a platform publication (entry document → "Frame-graph placement"), where scripts observe it as the event `Platform.AccountChanged`.
4. The account key is a hash; neither the raw platform account identifier nor a display name ever appears in a platform event, a log line or a replay log, so no cross-account data can leak through PLAT's diagnostics.

## Development services

Both services exist for development tooling: the Luau DAP debugger server and the script file watcher (`Zenith2/Docs/Scripting.md` → "Debugging and profiling"), the remote profiler and the editor live link. They are built into non-Shipping configurations alone.

### Loopback socket

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_DevSocket.h

struct Zenith_DevListenerDesc
{
	const char* m_szServiceName = nullptr;   // "dap", "profiler", "livelink"; used in logs and counters
	Zenith_StringView m_strBindAddress;      // "127.0.0.1" unless devtools.bindAddress was set on the command line
	uint16 m_uPort = 0;                     // 0 = ephemeral; the chosen port is logged
	uint32 m_uMaxConnections = 4;
	uint32 m_uReceiveBufferBytes = 262144;
};

/// @contract type Zenith_DevListener
/// Ownership: Created by Zenith_DevListener::Create, owned by the development service that uses it.
/// Thread affinity: OWNER_THREAD. The owner calls Poll from one thread (the DAP service polls from its own thread).
/// Invalidation: A Zenith_DevConnection ID is stale after the peer disconnects or Close; sends to it fail.
/// Units: Byte counts; ports as TCP port numbers.
/// Errors: Zenith_Result<…, Zenith_DevServiceError>; every socket error is recoverable; received bytes are untrusted.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks; Poll returns what happened.
/// Version: since Z2 Stage 3. Absent from Shipping builds.
/// @endcontract
class Zenith_DevListener
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DevListener>, Zenith_DevServiceError> Create(
		const Zenith_DevListenerDesc& xDesc, Zenith_Heap& xHeap);
	uint16 GetPort() const;
	Zenith_StringView GetSessionToken() const;   // 32 hex characters, printed to the local log at Create
	// Non-blocking: accepts pending connections, reads available bytes, reports closed peers.
	Zenith_Result<Zenith_DevPollResult, Zenith_DevServiceError> Poll(Zenith_Arena& xArena);
	Zenith_Result<uint32, Zenith_DevServiceError> Send(Zenith_DevConnection xConnection, Zenith_ByteSpan xBytes);
	void Close(Zenith_DevConnection xConnection);
};

/// @contract type Zenith_DevClient
/// Ownership: Created by Zenith_DevClient::ConnectLoopback, owned by its caller (a development tool or a test, for example
///   SCRIPT's DAP test client); the destructor closes the socket.
/// Thread affinity: OWNER_THREAD. One thread sends, receives and closes.
/// Invalidation: After Close or a 0-byte Receive (the peer closed), Send returns DEV_SERVICE_ERROR_SOCKET.
/// Units: Byte counts; the port as a TCP port number; the receive timeout in milliseconds.
/// Errors: Zenith_Result<…, Zenith_DevServiceError>; every socket error is recoverable; received bytes are untrusted.
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Version: since Z2 Stage 3 (pulled into the Stage 1 sprint with the listener). Absent from Shipping builds.
/// @endcontract
class Zenith_DevClient
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_DevClient>, Zenith_DevServiceError> ConnectLoopback(uint16 uPort, Zenith_Heap& xHeap);
	Zenith_Result<uint32, Zenith_DevServiceError> Send(Zenith_ByteSpan xBytes);
	Zenith_Result<uint32, Zenith_DevServiceError> Receive(Zenith_MutableByteSpan xOut, uint32 uTimeoutMs);   // 0 bytes: the peer closed
	void Close();
};
```

- The listener binds `127.0.0.1` by default and binds another address only when `devtools.bindAddress` was set on the command line (`Zenith2/Docs/Architecture.md` → "Security architecture", "No debugger or remote tools in shipping").
- A new connection is accepted into a pending state and receives nothing but a closed socket unless its first message within 5 s is exactly `ZENITH2-TOKEN <token>\n` with the session token; the token is 128 bits from the OS random source (`BCryptGenRandom`, `getrandom`), printed to the local log only.
- `Zenith_DevClient` is the listener's loopback peer for development tools and tests, so a test talks to a listener without including a platform socket header (rule F18) or branching on the platform (rule F11): `ConnectLoopback` connects a blocking TCP socket to `127.0.0.1:<uPort>` only (no other address is reachable through it), returning `DEV_SERVICE_ERROR_BIND_FAILED` when nothing listens there (connection refused) and `DEV_SERVICE_ERROR_SOCKET` for any other failure, `DEV_SERVICE_ERROR_OUT_OF_MEMORY` when the object cannot be allocated from `xHeap`; `Send` writes all of `xBytes` and returns the count (a closed or failed socket is `DEV_SERVICE_ERROR_SOCKET`); `Receive` waits at most `uTimeoutMs` for readable bytes and returns how many it copied into `xOut` (at most its size), 0 when the peer closed, and `DEV_SERVICE_ERROR_SOCKET` when the timeout passes with nothing to read; `Close` is idempotent. The client sends nothing on its own: a caller that talks to a `Zenith_DevListener` sends `ZENITH2-TOKEN <token>
` as its first message, exactly as any other peer must.
- Windows uses Winsock (`WSAStartup` in `Create` and in `ConnectLoopback`, balanced by `WSACleanup` at destruction; non-blocking sockets and `select` with zero timeout in `Poll`, `select` with the caller's timeout in `Receive`); Android and Linux use BSD sockets with `poll` and zero timeout (the caller's timeout in `Receive`). On Android the service needs the `INTERNET` permission, which only the `debug` and `hwasan` Gradle build types declare (the Gradle template is MOD's, `Zenith2/Docs/Architecture/CompositionRoot.md` → "The Gradle template"); a `release` development APK reaches the listener through `adb forward`.

### File watcher

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_FileWatcher.h

enum Zenith_FileChangeKind : uint8 { FILE_CHANGE_ADDED = 1, FILE_CHANGE_MODIFIED, FILE_CHANGE_REMOVED, FILE_CHANGE_RENAMED };

struct Zenith_FileChange
{
	Zenith_FileChangeKind m_eKind;
	Zenith_VirtualPath m_xPath;              // a content:/ path under the loose mount
	uint64 m_ulTimestampNs;
};

/// @contract type Zenith_FileWatcher
/// Ownership: Created by Zenith_FileWatcher::Create, owned by its client (script hot reload, the editor's shader and
///   asset watchers); owns one watcher thread, joined by its destructor.
/// Thread affinity: OWNER_THREAD for Drain; the watcher thread is internal.
/// Invalidation: None.
/// Units: Debounce in milliseconds.
/// Errors: DEV_SERVICE_ERROR_WATCH_UNAVAILABLE when the loose mount is absent; overflow of the OS change buffer is
///   reported as one FILE_CHANGE_MODIFIED for the watched root (a rescan request), never lost silently.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2. Absent from Shipping builds.
/// @endcontract
class Zenith_FileWatcher
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_FileWatcher>, Zenith_DevServiceError> Create(
		Zenith_VirtualFileSystem& xFiles, const Zenith_VirtualPath& xRoot, uint32 uDebounceMs, Zenith_Heap& xHeap);
	~Zenith_FileWatcher();
	Zenith_Span<const Zenith_FileChange> Drain(Zenith_Arena& xArena);   // changes since the last Drain, coalesced
};
```

- The watcher watches the loose mount only (content that the tools rewrite during iteration); a change reported by the OS is mapped back to a `content:/` path and dropped when the path fails `Zenith_ParseVirtualPath`.
- Windows: `ReadDirectoryChangesW` with a 64 KiB buffer on an overlapped handle, woken by an event; Linux: `inotify` on every subdirectory; Android: `inotify` on the pushed loose directory.
- Changes to one path within `uDebounceMs` (default 100) coalesce into one record whose kind is the last one; the drain queue holds at most 4,096 records with `QUEUE_FULL_POLICY_DROP_OLDEST`, counted in `devtools.watcher_queue.full`, `.dropped` and `.high_water` (§ "Event queue and overflow"), and an overflow adds the root rescan record.

### Shipping exclusion

- The implementations of `Zenith_DevSocket.h` and `Zenith_FileWatcher.h` live under `Zenith2/Engine/ZenithPlatform/Internal/DevServices/`. Each public entry function begins `if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(DEV_SERVICE_ERROR_NOT_IN_SHIPPING); }` (`ZenithCore/Zenith_BuildInfo.h`, `Zenith2/Docs/Architecture/BuildSystem.md` → "Clang flags for the android and linux-tsan presets"), so in a Shipping build nothing references the socket or watcher code, and the linker's `/OPT:REF` and `--gc-sections` remove it; no preprocessor conditional is involved (rule F11).
- The development entry symbols (the internal `Zenith_DevListenerImpl::Open`, `Zenith_DevClientImpl::Connect` and `Zenith_FileWatcherImpl::Start`, the only functions that reach Winsock, BSD sockets, `ReadDirectoryChangesW` or `inotify`) are listed in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` (a PROC edit, entry document → "Pending new rows"), so the `Libgraph` step fails when either survives into a Shipping executable (`Zenith2/Docs/Architecture.md` → "Security architecture").

<!-- assumption: pending MOD -->
If MOD adds a configuration-scoped source partition to `zenith_add_library` (Development versus Shipping sources), the development bodies move to it and the `if constexpr` guards are removed in the same diff; the forbidden-symbol check stays either way. The request is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.
<!-- /assumption -->

## Process watchdog

<!-- elaboration -->
The wall-clock watchdog of `--timeout-seconds` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options", exit code 124) is a platform timer thread, because it must fire even when the main thread is wedged:

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h (member of Zenith_Platform)
/// @deviation Thread affinity: MAIN_THREAD, once; the watchdog thread it starts is owned by the platform.
Zenith_Result<void, Zenith_PlatformError> StartWatchdog(uint32 uSeconds, uint8 uExitCode);
```

- The runtime calls it right after startup step 4 when `--timeout-seconds` is present, with `uExitCode` = `EXIT_CODE_WATCHDOG` (ZenithPlatform does not include the runtime's exit-code header; the value is passed down).
- The thread (`Zenith.Watchdog`, a `Zenith_Thread`) waits on a `Zenith_Semaphore` with a timeout of `uSeconds` seconds measured on the monotonic clock. The platform's destructor signals the semaphore and joins the thread, so a run that shuts down in time never fires it.
- On timeout it logs `Runtime: watchdog fired after <n> s` at error level under the `platform` category and ends the process at once with the OS immediate-exit call (`TerminateProcess(GetCurrentProcess(), uExitCode)` on Windows, `_exit(uExitCode)` on Android and Linux): no destructor runs and no lock is taken, because a wedged process may hold any of them.
- A second call returns `PLATFORM_ERROR_BAD_DESC`; `uSeconds` outside 1 to 86,400 returns `PLATFORM_ERROR_BAD_DESC` (the command-line parser has already refused such values).

## The headless test host

`ZenithPlatform/Internal/Linux/` implements the platform for the `linux-tsan` preset (`Zenith2/Docs/Architecture/PlatformLayer.md` → "The headless test host"):

| Service | Linux implementation |
|---|---|
| `Zenith_Platform` | `PLATFORM_MODE_HEADLESS` forced; `SDL_Init(SDL_INIT_EVENTS)` alone, so the pump's shared code runs under TSan; no window, no device events |
| input | `Zenith_InputDevices` exists with no physical device; virtual devices and scripted input work (tests drive them) |
| virtual filesystem | the loose mount at `$ZENITH2_TEST_LOOSE_ROOT` (default: the test executable's `Fixtures/` directory) and a temporary user directory under `$TMPDIR/zenith2-<pid>/`, removed by the test main |
| async I/O | POSIX `pread`/`pwrite`/`fsync`/`rename`/`statvfs` on the two I/O threads |
| clock and threads | ZenithCore's POSIX implementations (`Zenith2/Docs/Architecture/PlatformLayer.md` → "The headless test host") |
| lifecycle | `APP_STATE_RUNNING` for the whole run; the state machine is still driven by tests through `Zenith_PlatformBackendHooks` |
| thermal | the null provider |
| account signals | tests post them through `PostAccountSignal` |

`Zenith_PlatformBackendHooks` (declared in `Zenith_Platform.h`, used by tests on every platform) replaces the SDL event source with a scripted list of `Zenith_TestPlatformEvent` records and replaces the free-space, thermal and lifecycle sources; it is passed in the desc, never selected by a preprocessor switch (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "Test separation").

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h
struct Zenith_TestPlatformEvent          // one SDL-equivalent event for the translation code, test-authored
{
	uint32 m_uSdlEventType = 0;             // the SDL_EventType value the translation table is keyed on
	uint64 m_ulSdlTimestampNs = 0;          // on SDL's clock; the platform adds the clock offset
	uint32 m_uWindowIndex = 0;              // 0 = the main window, else a secondary window's creation index
	uint32 m_uWhich = 0;                    // SDL's joystick, keyboard, mouse or finger instance ID
	sint32 m_aiData[4] = {};                // per type: key and modifiers, button, axis value, sizes
	float m_afData[4] = {};                  // per type: positions, deltas, pressure, display scale
	Zenith_StringView m_strText;             // text input, drop file and locale events
};

/// @contract interface Zenith_PlatformBackendHooks
/// Ownership: Implemented by tests; passed in Zenith_PlatformDesc; outlives the platform.
/// Thread affinity: MAIN_THREAD; called inside Pump and Create.
/// Invalidation: The events written to xOut, and the text they view, need to stay valid only until NextEvents returns
///   to the pump, which copies them.
/// Units: SDL's nanosecond clock for m_ulSdlTimestampNs.
/// Errors: None; a malformed test event is translated as the table says or ignored and counted like an SDL event.
/// Reentrancy: NONE. An implementation may not call back into the platform.
/// Version: since Z2 Stage 2. Test API; never used by a Player.
/// @endcontract
class Zenith_PlatformBackendHooks
{
public:
	virtual ~Zenith_PlatformBackendHooks() = default;
	// Fills xOut with the SDL-equivalent events for this pump, in order; returns the count.
	virtual uint32 NextEvents(Zenith_Span<Zenith_TestPlatformEvent> xOut, uint64 ulPumpIndex) = 0;
	virtual Zenith_ThermalProvider* GetThermalProvider() = 0;   // null: the platform default
	// Connectivity for this pump; false (the default) keeps the platform's own source (§ "Connectivity").
	virtual bool GetConnectivity(uint64 ulPumpIndex, Zenith_ConnectivityState& eOut)
	{
		(void)ulPumpIndex;
		(void)eOut;
		return false;
	}
};
```

## Platform core tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/`; each runs in the `Test` step on `win-debug`, `win-release` and `linux-tsan` unless the row says otherwise.

| Test | File | Asserts |
|---|---|---|
| `Test_Platform_Create_FailsAtEachStep_Unwinds` | `Zenith_Platform_Tests.cpp` | `Zenith_RunFaultSweep` over the ten creation steps: each failure returns its error, leaks nothing, leaves SDL uninitialised (`SDL_WasInit(0) == 0`) |
| `Test_Platform_BadDesc_Rejected` | `Zenith_Platform_Tests.cpp` | each invalid desc field of step 1 returns `PLATFORM_ERROR_BAD_DESC` |
| `Test_Platform_HeadlessHasNoWindow` | `Zenith_Platform_Tests.cpp` | headless mode: `GetMainWindowID().IsNull()`, `CreateWindow` returns `PLATFORM_ERROR_NOT_AVAILABLE` |
| `Test_Platform_PumpTranslatesEachEventKind` | `Zenith_PlatformPump_Tests.cpp` | through `Zenith_PlatformBackendHooks`, one scripted SDL-equivalent event per row of "Event translation table" yields the listed record, flags and payload |
| `Test_Platform_PumpPreservesOrder` | `Zenith_PlatformPump_Tests.cpp` | 1,000 interleaved window, device and input events keep their relative order and strictly increasing sequence numbers |
| `Test_Platform_EventRingGrowsThenRejects` | `Zenith_PlatformPump_Tests.cpp` | 20,000 events in one pump: the ring grows to 16,384, `platform.events.dropped` counts the rest, window and device records survive by displacing motion records |
| `Test_Platform_WorldVisibleFlag` | `Zenith_PlatformPump_Tests.cpp` | exactly the eight world-visible kinds carry `uPLATFORM_EVENT_FLAG_WORLD_VISIBLE` |
| `Test_Platform_WatchdogFiresWithExitCode` | `Zenith_Platform_Tests.cpp` (a child-process test) | a child that starts a 1 s watchdog and blocks exits with the passed code within 3 s and logs the line; a child that shuts down after 0.2 s exits normally |
| `Test_Platform_TimestampsOnMonotonicClock` | `Zenith_PlatformPump_Tests.cpp` | a scripted SDL timestamp plus the clock offset lands within the pump's monotonic time window |
| `Test_Window_CreateDestroy_Limit` | `Zenith_Window_Tests.cpp` (windowed, `win-debug` only: needs a display) | 16 windows create, the 17th fails `PLATFORM_ERROR_TOO_MANY_WINDOWS`, destroying one frees a slot, a stale ID fails `PLATFORM_ERROR_UNKNOWN_WINDOW` |
| `Test_Window_MainWindowNotDestroyable` | `Zenith_Window_Tests.cpp` | `DestroyWindow(main)` fails `PLATFORM_ERROR_MAIN_WINDOW` |
| `Test_Window_VulkanSurfaceRoundTrip` | `Zenith_Window_Tests.cpp` (windowed, `win-debug` with lavapipe) | `Zenith_GetVulkanInstanceProcAddr` and the instance extensions create an instance; `CreateVulkanSurface` returns a non-null surface that `DestroyVulkanSurface` releases; the D3D12 path reads a non-null `HWND` |
| `Test_Window_ModeChangeEmitsResize` | `Zenith_Window_Tests.cpp` (windowed) | switching windowed → borderless → windowed emits one resize record each; exclusive fullscreen falling back logs a warning |
| `Test_Window_EventsRoutedByWindowID` | `Zenith_PlatformPump_Tests.cpp` | scripted events for two windows carry their own IDs; an event for a destroyed window is dropped and counted |
| `Test_Display_ScaleChangeUpdatesMetricsBeforeEvent` | `Zenith_PlatformPump_Tests.cpp` | the metrics read inside the consumer of the scale record already hold the new scale |
| `Test_Display_DpConversion` | `Zenith_Display_Tests.cpp` | points, pixels and dp convert per the table "Units points pixels and dp" for densities 1, 1.5, 2.625 and scales 1, 1.25, 3.5 |
| `Test_Display_SafeAreaChangeEvent` | `Zenith_PlatformPump_Tests.cpp` | a scripted safe-area change updates `m_xSafeArea` (converted to pixels) before its `PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED` record |
| `Test_Clipboard_RejectsInvalidUtf8` | `Zenith_Clipboard_Tests.cpp` (windowed) | invalid UTF-8 is rejected; 2 MiB text is truncated to 1 MiB and counted |
| `Test_AccountSignal_DeliveredAtNextPump` | `Zenith_AccountSignal_Tests.cpp` | a signal posted from another thread appears as one `PLATFORM_EVENT_ACCOUNT_CHANGED` at the next pump, never earlier, with the hashed key |
| `Test_AccountSignal_QueueDropsOldest` | `Zenith_AccountSignal_Tests.cpp` | 65 posts leave 64, the oldest dropped and counted |
| `Test_AccountSignal_NoRawIdentifier` | `Zenith_AccountSignal_Tests.cpp` | the signal struct has no string member (`static_assert` on its layout) and the log line of a delivery contains only the key's hexadecimal hash |
| `Test_DevListener_LoopbackByDefault` | `Zenith_DevSocket_Tests.cpp` | the bound address is 127.0.0.1; a connection that does not send the token within 5 s is closed; one that does is accepted |
| `Test_DevListener_ShippingReturnsNotInShipping` | `Zenith_DevSocket_Tests.cpp` (Shipping test build) | `Create` returns `DEV_SERVICE_ERROR_NOT_IN_SHIPPING` |
| `Test_DevClient_RoundTripsThroughListener` | `Zenith_DevSocket_Tests.cpp` | a listener on an ephemeral port and a `Zenith_DevClient` connected to it: after the token message the listener's `Poll` reports one authenticated connection; 64 KiB sent by the client arrive in order through `Poll`, 64 KiB sent by the listener arrive through `Receive` with a 1,000 ms timeout; after the listener's `Close` the client's `Receive` returns 0 |
| `Test_DevClient_RefusedAndTimedOut` | `Zenith_DevSocket_Tests.cpp` | `ConnectLoopback` to a port nothing listens on returns `DEV_SERVICE_ERROR_BIND_FAILED`; a connected client with nothing to read returns `DEV_SERVICE_ERROR_SOCKET` from `Receive(…, 50)` after at least 50 ms; a client that never sends the token is closed by the listener after 5 s and its `Receive` returns 0 |
| `Test_DevClient_ShippingReturnsNotInShipping` | `Zenith_DevSocket_Tests.cpp` (Shipping test build) | `ConnectLoopback` returns `DEV_SERVICE_ERROR_NOT_IN_SHIPPING` |
| `Test_FileWatcher_CoalescesAndMapsToVirtualPaths` | `Zenith_FileWatcher_Tests.cpp` | three writes within 100 ms yield one `FILE_CHANGE_MODIFIED` for the `content:/` path; a path failing validation is dropped |
| `Test_FileWatcher_OverflowRequestsRescan` | `Zenith_FileWatcher_Tests.cpp` | 5,000 changes before a drain leave 4,096 records ending with the root rescan record |
| `Test_Platform_BasePathOverride` | `Zenith_Platform_Tests.cpp` | a desc with `m_strBasePathOverride` = a directory the test created under `${ZENITH_OUT}/test-fixtures/PLAT/` makes `GetBasePath()` return it with one trailing `/` (given with and without one); without the override `GetBasePath()` equals SDL's base path; an override with an invalid UTF-8 byte or a C0 control returns `PLATFORM_ERROR_BAD_DESC` from step 1 and SDL stays uninitialised |
| `Test_Platform_AndroidJavaContext` | `Zenith_Platform_Tests.cpp` | off Android, and in an Android process without an SDL activity, `GetAndroidJavaContext()` returns `PLATFORM_ERROR_NOT_AVAILABLE` and creation still succeeds; in an Android process with an activity, the three handles are non-null, `loadClass("org.libsdl.app.SDLActivity")` through `m_pClassLoader` succeeds on a second thread attached through `m_pJavaVM`, and a second platform created after the first is destroyed yields fresh, valid handles |
| `Test_Platform_ConnectivityRevision` | `Zenith_PlatformPump_Tests.cpp` | through `GetConnectivity` of the backend hooks over 8 pumps (UNKNOWN, ONLINE, OFFLINE, OFFLINE, ONLINE, ONLINE, OFFLINE, ONLINE): the state follows each pump, the revision is 0, 0, 0, 0, 1, 1, 1, 2 (UNKNOWN → ONLINE does not count), and `platform.connectivity.regained` equals the revision; with hooks returning false on the `linux-tsan` host the state stays UNKNOWN |
| `Test_Platform_LinuxHostRunsPumpUnderTsan` | `Zenith_PlatformPump_Tests.cpp` (`linux-tsan`) | 10,000 pumps with account signals posted from 8 threads run clean under TSan |
