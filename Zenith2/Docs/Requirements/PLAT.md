# PLAT requirement catalogue

This is the requirement catalogue of the PLAT area (platform, input, filesystem, asynchronous I/O, configuration, lifecycle and thermal adaptation). Each requirement is one testable obligation with its inventory coverage, stage, classification, governing decisions, budget rows, acceptance evidence and the design headings of `Zenith2/Docs/Design/PLAT.md` and its split files that specify it.

## Purpose

The catalogue decomposes every PLAT item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into requirements `Z2-R-PLAT-NNN`, the unit that leaf and integration tickets cover (`Zenith2/Docs/Roadmap.md` → "Board conventions"). It also holds the requirements for the platform half of fifteen items owned by other areas (the table "Items of other areas with a PLAT half"). Requirements Z2-R-PLAT-168 to Z2-R-PLAT-176 (the packaging and tooling primitives) are in `Zenith2/Docs/Requirements/PLAT/PackagingAndTooling.md`, in the same numbering and format. Stages name the stage whose acceptance each requirement serves; a requirement whose governing decisions are settled may be scheduled early by the pull-forward rule (`Zenith2/Docs/Roadmap.md` → "Pull-forward rule"). Every requirement is classified `required`: PLAT owns no selection decision, so it has no research requirement, and nothing it owns is deferred or excluded except what "Excluded and deferred" lists. Acceptance evidence names tests and gate steps; `needs-human` device runs are recorded on the stage acceptance ticket (`Zenith2/Docs/Design/PLAT.md` → "Device tests").

## Requirements

### Z2-R-PLAT-001 ZenithPlatform library target

- **Statement:** ZenithPlatform is one static library declared as `zenith_add_library(ZenithPlatform DEPENDS ZenithCore ZenithProfiling THIRD_PARTY SDL3)` with `SUPPORTED ZENITH_ALL`, its public headers at the library root, platform sources only under `Internal/Windows/`, `Internal/Android/` and `Internal/Linux/`, and its tests under `Tests/`.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-12, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `Configure`, `Build -Preset win-debug`, `Build -Preset android-arm64`, `SourceLayout`, `CompileCoverage` and `Libgraph` gate steps pass with the library present, and the `linux-tsan` tree builds it in the `Sanitizers -Preset linux-tsan` step.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "ZenithPlatform target"; `Zenith2/Docs/Design/PLAT.md` → "ZENITH_SUPPORTED matrix and SDL3 configuration"

### Z2-R-PLAT-002 SDL3 confined to ZenithPlatform Internal

- **Statement:** Every SDL3 include and call is in `Zenith2/Engine/ZenithPlatform/Internal/`; no public header of any library and no other library includes an SDL3 header or calls an SDL3 function, and SDL3 threads and timers are never used.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `Libgraph` step (third-party owner and leakage rules) and `ForbiddenCheck` (F18 platform headers) pass; `Test_Platform_PumpTranslatesEachEventKind` runs through the Internal translation code only.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "ZENITH_SUPPORTED matrix and SDL3 configuration"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Subsystem initialisation per mode"

### Z2-R-PLAT-003 Platform creation steps and unwinding

- **Statement:** `Zenith_Platform::Create` runs its ten creation steps in order, rejects an invalid desc with `PLATFORM_ERROR_BAD_DESC`, and on a failure at any step unwinds exactly what it created, leaving SDL uninitialised and nothing leaked.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_Create_FailsAtEachStep_Unwinds` and `Test_Platform_BadDesc_Rejected` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc"

### Z2-R-PLAT-004 Headless platform mode

- **Statement:** In `PLATFORM_MODE_HEADLESS` the platform initialises SDL with the events subsystem alone, creates no window and no input device, and every windowed-only member returns `PLATFORM_ERROR_NOT_AVAILABLE`.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_HeadlessHasNoWindow` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Subsystem initialisation per mode"

### Z2-R-PLAT-005 Platform pump in prologue step 1

- **Statement:** `Zenith_Platform::Pump` runs only on the main thread as step 1 of the frame prologue, performs the nine steps of the pump algorithm in order, processes SDL events strictly in queue order, and never blocks.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_PumpPreservesOrder` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm"; `Zenith2/Docs/Design/PLAT.md` → "The Platform Pump step"

### Z2-R-PLAT-006 Event translation table

- **Statement:** Every SDL3 event of the event translation table is translated into the listed `Zenith_PlatformEvent` record with the listed payload, and every other SDL event is ignored and counted in `platform.events.ignored`.
- **Covers:** Z2-I-PLAT-05, Z2-I-PLAT-10, Z2-I-PLAT-11, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_PumpTranslatesEachEventKind` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Event translation table"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent"

### Z2-R-PLAT-007 Bounded event ring with overflow policy

- **Statement:** The frame event ring grows from `platform.eventQueueCapacity` to `platform.eventQueueHardCapacity` and then rejects, a rejected window, device or lifecycle record displaces a motion-transition record, and full events, drops and the high-water mark are counted.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_EventRingGrowsThenRejects` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow"

### Z2-R-PLAT-008 World-visible platform events published at tick boundaries

- **Statement:** Exactly the eight world-visible platform event kinds carry `uPLATFORM_EVENT_FLAG_WORLD_VISIBLE`, and each reaches every world only as a logged asynchronous publication with source kind `ASYNC_SOURCE_PLATFORM` and publication kind `PUBLICATION_KIND_PLATFORM_RECORDED` at a tick boundary, in completion-ID order, never mid-tick, carrying the 36-byte recorded payload.
- **Covers:** Z2-I-PLAT-05, Z2-I-PLAT-20, Z2-I-PLAT-30
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_WorldVisibleFlag` in the `Test` step (win-debug, win-release, linux-tsan) and `Test_PlatformEvents_PublishedAtTickBoundary` in the `Zenith2/Tests/PlatformEvents/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement"

### Z2-R-PLAT-009 Event timestamps on the monotonic clock

- **Statement:** Every platform event and input transition carries a timestamp of ZenithCore's monotonic clock `Zenith_GetMonotonicTimeNs`, converted from SDL timestamps with the offset measured at creation, and ZenithPlatform declares no clock of its own.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_TimestampsOnMonotonicClock` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "The monotonic clock"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc"

### Z2-R-PLAT-010 Main window creation

- **Statement:** The main window is created in windowed mode from a validated `Zenith_WindowDesc` (sizes clamped, high pixel density by default, Android forced to borderless), and the first pump after creation reports its size through `PLATFORM_EVENT_WINDOW_RESIZED`.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Window_CreateDestroy_Limit` (windowed, win-debug) and the Stage 2 acceptance record of the minimal game booting on Windows and the S24.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Window creation and destruction"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API"

### Z2-R-PLAT-011 Multiple windows

- **Statement:** Up to 16 windows exist at once; a 17th is refused with `PLATFORM_ERROR_TOO_MANY_WINDOWS`, the main window cannot be destroyed, and a destroyed window's ID is stale.
- **Covers:** Z2-I-PLAT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Window_CreateDestroy_Limit` and `Test_Window_MainWindowNotDestroyable` (windowed, win-debug).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Multiple windows"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Window creation and destruction"

### Z2-R-PLAT-012 Per-window event routing and focus

- **Statement:** Every window, text and input record carries the `Zenith_WindowID` of its window, an event for a destroyed window is dropped and counted, exactly one window holds keyboard focus, and a window-scoped presentation context is active only while its window has focus.
- **Covers:** Z2-I-PLAT-11, Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Window_EventsRoutedByWindowID` and `Test_Context_PinnedTopAndWindowScoped` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Multiple windows"; `Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks"

### Z2-R-PLAT-013 Native handles Vulkan surfaces and window modes

- **Statement:** A window provides its native handle for D3D12, creates and destroys Vulkan surfaces through SDL for the RHI with volk initialised from SDL's loader, and switches between windowed, borderless and exclusive fullscreen, each change reported as a resize, and `EnumerateDisplays` lists every connected display with its bounds, content scale and refresh rate.
- **Covers:** Z2-I-PLAT-10, Z2-I-PLAT-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-026, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Window_VulkanSurfaceRoundTrip` and `Test_Window_ModeChangeEmitsResize` (windowed, win-debug with lavapipe).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Native handles for the RHI"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Fullscreen and display modes"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API"

### Z2-R-PLAT-014 Display metrics in pixels points and dp

- **Statement:** `Zenith_DisplayMetrics` reports pixel size, point size, pixel density, display scale and refresh rate per window, points, pixels and dp convert by the unit table, and text payloads of a frame live in the platform's bounded per-frame text arena (`m_uTextArenaBytes`, at least 4,096 bytes), valid until the next pump.
- **Covers:** Z2-I-PLAT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Display_DpConversion` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Units points pixels and dp"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API"

### Z2-R-PLAT-015 Display scale and safe-area changes

- **Statement:** A display-scale, display or safe-area change updates the window's cached metrics before its record is appended, the scale change is a swapchain-recreation trigger, and gesture thresholds are recomputed at the same pump.
- **Covers:** Z2-I-PLAT-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Display_ScaleChangeUpdatesMetricsBeforeEvent` and `Test_Display_SafeAreaChangeEvent` in the `Test` step (win-debug, win-release, linux-tsan), and the Windows DPI device run.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Display scale changes"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Safe area"

### Z2-R-PLAT-016 Clipboard text validated

- **Statement:** `Zenith_Clipboard` stores and returns only valid UTF-8, truncates at `platform.clipboardMaxBytes` with a counter, and is never read by simulation.
- **Covers:** Z2-I-PLAT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Clipboard_RejectsInvalidUtf8` (windowed, win-debug).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Clipboard"

### Z2-R-PLAT-017 Headless linux-tsan host

- **Statement:** `ZenithPlatform/Internal/Linux/` implements the platform for the `linux-tsan` preset with no window, GPU or audio device: SDL initialised with events only, a loose mount and a temporary user directory, POSIX I/O on the I/O threads, the null thermal provider.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_LinuxHostRunsPumpUnderTsan` and every ZenithPlatform test in the `Sanitizers -Preset linux-tsan` step.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "The headless test host"

### Z2-R-PLAT-018 Test injection through backend hooks

- **Statement:** Tests replace the SDL event source, the thermal provider, the free-space source and the fault points through `Zenith_PlatformBackendHooks`, `Zenith_FreeSpaceSource` and `Zenith_FaultInjector` passed in descs, never through a preprocessor switch or a test-only code path in shipping code.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** The `TestSeparationCheck` gate step passes (no test switch in shipping code) and the lifecycle, pump and I/O tests of the `Test` step (win-debug, win-release, linux-tsan) use the hooks.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "The headless test host"

### Z2-R-PLAT-019 Input device registry

- **Statement:** Input devices live in a generational slot table of at most 64 devices, keyboards and mice are one logical device each, and a stale `Zenith_InputDeviceID` lookup returns `INPUT_ERROR_DEVICE_NOT_FOUND`.
- **Covers:** Z2-I-PLAT-04, Z2-I-PLAT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_Gamepad_HotplugAddRemove` and `Test_InputDevices_TransitionLogOrderAndSequence` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Device kinds and IDs"; `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API"

### Z2-R-PLAT-020 Control path grammar

- **Statement:** Control paths follow the path grammar, parse and format as exact inverses for every catalogue entry, reject everything else with `INPUT_ERROR_UNKNOWN_CONTROL_PATH`, and reserve the `/xr/` namespace.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_ControlPath_RoundTripEveryCatalogueEntry` and `Test_ControlPath_XrPathsReserved` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Control paths"

### Z2-R-PLAT-021 Control catalogue

- **Statement:** Every device kind exposes exactly the controls, value types and ranges of the control catalogue, SDL-numbered enumerations are checked against the pinned SDL3 by `static_assert`, and stick y is positive up.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** The `Build` gate steps (the static asserts) and `Test_InputDevices_StickYAxisUp` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Control catalogue"

### Z2-R-PLAT-022 Ordered transition log with bounded motion coalescing

- **Statement:** Each frame's control changes are recorded as transitions in SDL order with strictly increasing sequences, motion is coalesced before button transitions are ever dropped, a reserved region keeps 1,024 slots for non-motion transitions, and every drop is counted.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_InputDevices_TransitionLogOrderAndSequence` and `Test_InputDevices_MotionCoalescingKeepsButtons` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log"

### Z2-R-PLAT-023 Keyboard and mouse behaviour

- **Statement:** Bindings name physical keys with labels from the current layout, OS key repeats never reach the mapper, focus loss releases every held key and button synthetically, and relative mouse mode is released on focus loss and restored on focus gain.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-02, Z2-I-PLAT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_InputDevices_FocusLossReleasesHeld`, `Test_Mouse_RelativeModeReleasedOnFocusLoss` and `Test_BindingDisplay_LabelsAndGlyphs` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Keyboard and mouse"

### Z2-R-PLAT-024 Virtual devices and injected input

- **Statement:** Virtual devices registered by UI, tests and the input script, and synthetic injections, enter the transition log at the next pump in call order and flow through claims, gestures and both mappers like physical input.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualDevice_CreateSetDestroy` and `Test_InputScript_ActionAndControlEvents` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API"

### Z2-R-PLAT-025 Gamepads through the SDL3 gamepad API

- **Statement:** Up to eight SDL3 gamepads are opened with their type, glyph family, rumble, trigger-rumble and sensor capabilities, unmappable joysticks are ignored and counted, and sensors are enabled only while `input.gamepadSensors` is true and a binding uses them.
- **Covers:** Z2-I-PLAT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gamepad_NinthRejected` and `Test_Gamepad_SensorsEnabledOnlyWhenBound` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Gamepad API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gamepad types and glyph families"

### Z2-R-PLAT-026 Gamepad mapping database

- **Statement:** SDL3's built-in mappings apply, a game may add `content:/Input/gamecontrollerdb.txt` of at most 1 MiB whose rejected lines are counted and logged without failing startup, and a remap releases changed controls first.
- **Covers:** Z2-I-PLAT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gamepad_MappingFileBadLinesCounted` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Mapping database"

### Z2-R-PLAT-027 Controller hotplug

- **Statement:** A connected gamepad yields `PLATFORM_EVENT_INPUT_DEVICE_CONNECTED`; a removed one first returns every non-rest control to rest at the removal timestamp, stops its haptics, and then yields `PLATFORM_EVENT_INPUT_DEVICE_DISCONNECTED` with the last-of-kind flag when no gamepad remains.
- **Covers:** Z2-I-PLAT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gamepad_HotplugAddRemove` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug"

### Z2-R-PLAT-028 Controller reconnection window

- **Statement:** A gamepad that reconnects with the same identity within `input.reconnectWindowSeconds` gets its previous `Zenith_InputDeviceID` with the reconnected flag; after the window the slot is freed and the old ID is stale.
- **Covers:** Z2-I-PLAT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gamepad_ReconnectWithinWindowKeepsID` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Reconnection"

### Z2-R-PLAT-029 Controller loss surfaced to the game

- **Statement:** Device connection and disconnection are world-visible: scripts observe `Platform.DeviceConnected` and `Platform.DeviceDisconnected` with the reconnected and last-of-kind flags at the next tick boundary, and the engine itself pauses nothing.
- **Covers:** Z2-I-PLAT-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PlatformEvents_DeviceDisconnectedReachesScript` in the `Zenith2/Tests/PlatformEvents/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug"; `Zenith2/Docs/Design/PLAT.md` → "Controller loss"

### Z2-R-PLAT-030 Haptic effects and mixing

- **Statement:** `Zenith_HapticPlayer` plays rumble, trigger rumble and device vibration on capable devices, mixes one slot per channel by per-motor maximum, refuses a lower-priority effect on an occupied channel, and passes SDL the minimum remaining duration.
- **Covers:** Z2-I-PLAT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Haptics_MixMaxAcrossChannels` and `Test_Haptics_PriorityRefusesLower` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic mixing"

### Z2-R-PLAT-031 Haptic intensity and stopping

- **Statement:** The global `input.hapticsIntensity` (0 disables every haptic) and per-channel intensities scale every effect, and every effect stops on entering SUSPENDING, on focus loss when configured, on device disconnection and at destruction.
- **Covers:** Z2-I-PLAT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Haptics_GlobalIntensityZeroDisables` and `Test_Haptics_StopAllOnSuspendAndFocusLoss` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic mixing"

### Z2-R-PLAT-032 Haptics from simulation are transactional commands

- **Statement:** Gameplay requests haptics only through the `Input.PlayHaptic` and `Input.StopHaptic` commands, played back at sync points into a bounded per-world queue and submitted by the `Input.Haptics` epilogue stage, so a rolled-back invocation plays nothing and replays reproduce haptics.
- **Covers:** Z2-I-PLAT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Haptics_CommandsRolledBackPlayNothing` in the `Zenith2/Tests/Input/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Haptics from simulation"

### Z2-R-PLAT-033 Touch points

- **Statement:** Up to ten concurrent touches take the lowest free index, the primary touch passes to the earliest remaining finger without a new press, positions are in window pixels, and a cancelled finger releases synthetically.
- **Covers:** Z2-I-PLAT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Touch_IndexAssignmentAndPrimary` and `Test_Touch_CancelledFingerReleasesSynthetic` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Touch points"

### Z2-R-PLAT-034 No synthetic pointer events and pen input

- **Statement:** SDL's mouse, touch and pen synthesis is off in every direction, touch emulation by the mouse exists only as a non-Shipping development option, and pens report tip, buttons, pressure and tilt as their own device.
- **Covers:** Z2-I-PLAT-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Touch_NoSyntheticMouseEvents` and `Test_Pen_TransitionsAndPressure` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Mouse and touch emulation"; `Zenith2/Docs/Design/PLAT/Input.md` → "Touch and pen"

### Z2-R-PLAT-035 Tap double-tap and long-press recognisers

- **Statement:** The tap, double-tap and long-press recognisers follow their state tables exactly, including a tap waiting for a double tap to fail when `input.gesture.tapWaitsForDoubleTap` is true, and a second finger down too far for a double tap emitting the pending tap at once and starting a fresh tap.
- **Covers:** Z2-I-PLAT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gesture_TapStateTable`, `Test_Gesture_DoubleTapStateTable` and `Test_Gesture_LongPressStateTable` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Tap state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Double tap state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Long press state table"

### Z2-R-PLAT-036 Pan and swipe recognisers

- **Statement:** The pan recogniser follows its state table and a pan's end is classified as a swipe by the distance, duration and velocity thresholds, with its direction from the dominant axis and ties to the horizontal axis.
- **Covers:** Z2-I-PLAT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gesture_PanAndSwipe` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Pan state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Swipe classification"

### Z2-R-PLAT-037 Two-finger recognisers and arbitration

- **Statement:** Pinch, rotate and two-finger tap follow their state table, a second finger takes over from every one-finger recogniser, one-finger recognisers are exclusive, and cancellation, focus loss and background cancel every active recogniser.
- **Covers:** Z2-I-PLAT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gesture_PinchRotateTwoFingerTap` and `Test_Gesture_DeterministicReplay` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Pinch and rotate state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture arbitration"

### Z2-R-PLAT-038 Gesture thresholds and bindable gestures

- **Statement:** Gesture thresholds come from the `input.gesture.*` keys in dp converted with the display scale, recognised gestures become transitions of the gesture device that bindings name as `/gesture/…` controls, and claimed touches never reach the recognisers.
- **Covers:** Z2-I-PLAT-08, Z2-I-PLAT-10, Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Gesture_ThresholdsInDpFollowDisplayScale` and `Test_Gesture_ClaimedTouchIgnored` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture thresholds"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture recogniser API"

### Z2-R-PLAT-039 S24 touch and gesture device run

- **Statement:** Every gesture is recognised by hand on the S24 display with the default thresholds, and a thumb on a claimed on-screen control never pans the camera.
- **Covers:** Z2-I-PLAT-07, Z2-I-PLAT-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` S24 touch-and-gesture device run record, attached to the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"; `Zenith2/Docs/Design/PLAT/Input.md` → "Input tests"

### Z2-R-PLAT-040 Text input sessions and the soft keyboard

- **Statement:** One text-input session is active at a time with its purpose mapped to SDL's text-input type and its caret area set for the candidate window; a second `Begin` fails with `INPUT_ERROR_TEXT_INPUT_ACTIVE`, and on Android the session shows the soft keyboard with the purpose's layout.
- **Covers:** Z2-I-PLAT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_TextInput_SecondBeginFails` in the `Test` step (win-debug, win-release, linux-tsan) and the IME device run.
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Text input API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Soft keyboard on Android"

### Z2-R-PLAT-041 IME composition and commit events

- **Statement:** SDL editing events become composition events with cursor and selection, text-input events become commits, ending a session mid-composition yields `COMPOSITION_END` and no commit, and invalid UTF-8 is replaced by U+FFFD and counted.
- **Covers:** Z2-I-PLAT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_TextInput_CompositionThenCommit` and `Test_TextInput_InvalidUtf8Replaced` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Composition events"

### Z2-R-PLAT-042 Text entry consumes keyboard controls

- **Statement:** While a text-input session is active the pinned `Engine.TextEntry` context consumes every keyboard control, through its bindings-file `consumesDevices` list and mapper step 4.3, for the simulation mapper and lower presentation contexts, while the raw transitions stay readable by UI.
- **Covers:** Z2-I-PLAT-09, Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_TextInput_ConsumesKeyboardForSimulation` and `Test_Bindings_ConsumesDevicesRules` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Text entry consumes keyboard controls"; `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"; `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm"

### Z2-R-PLAT-043 IME and soft keyboard device run

- **Statement:** Japanese and Chinese IME composition works inline in a text field with candidates at the caret on Windows, and each soft-keyboard purpose shows its layout on the S24.
- **Covers:** Z2-I-PLAT-09
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` Windows and S24 IME device run record, attached to the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"

### Z2-R-PLAT-044 Action types and conversions

- **Statement:** Actions are typed BUTTON, AXIS1D, AXIS2D, AXIS3D or DELTA2D with POSE reserved and rejected until Stage 7, and controls convert to action types exactly by the conversion table, every other combination rejected with `INPUT_ERROR_TYPE_MISMATCH`.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_Action_TypeConversionTable` and `Test_BindingsFile_EachValidationRule` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Action types"

### Z2-R-PLAT-045 Stable action IDs and action state

- **Statement:** Action IDs are stable 24-bit values in the game or engine range, never reused after retirement, with bits 24-31 reserved for command components, and each action's state (held, edges, phase flags, tap count, held time, value) lives in an ID-sorted action-state table.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_BindingsFile_EachValidationRule` and `Test_ActionMapper_SameInputsSameOutputs` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Action IDs"; `Zenith2/Docs/Design/PLAT/Input.md` → "Action state"

### Z2-R-PLAT-046 Composites and modifiers

- **Statement:** Dpad, axis and vector-3 composites combine their parts as specified, modifiers must be held and (by default) pressed before the main control, and within a context the binding with the most modifiers suppresses less specific bindings of the same control.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Binding_CompositesDpadAxisVector3` and `Test_Binding_ModifiersOrderedAndSpecificity` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Composites"; `Zenith2/Docs/Design/PLAT/Input.md` → "Modifiers"

### Z2-R-PLAT-047 Processors

- **Statement:** Axial and radial deadzones, scale, invert, normalise, clamp, curve and configuration-scale processors run in declared order with the specified functions, and a configuration-scale change applies from the next prologue.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Binding_ProcessorsEachKind` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Processors"

### Z2-R-PLAT-048 Interactions

- **Statement:** Default, press, hold, tap, slow-tap and multi-tap interactions follow the interaction state table cell by cell, apply only to BUTTON and AXIS1D actions, and OR their phase flags into the action for the slice.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Interaction_EveryStateTableCell` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Interactions"

### Z2-R-PLAT-049 Control schemes and binding display

- **Statement:** Every binding belongs to schemes derived from its devices, all schemes are live at once, the active scheme follows the latest actuation above the threshold with a world-visible change event, and `GetBindingDisplay` returns per-control prompts with glyph family and layout label.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-02, Z2-I-PLAT-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Scheme_ActiveSchemeSwitchesOnThreshold` and `Test_BindingDisplay_LabelsAndGlyphs` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Control schemes"; `Zenith2/Docs/Design/PLAT/Input.md` → "Binding display"

### Z2-R-PLAT-050 XR-ready input model

- **Statement:** Gameplay code, UI and scripts address input only through actions and contexts, never through devices or control paths, and the model reserves the XR device kind, the `/xr/` paths, the POSE type and its command encoding so that Stage 7 OpenXR input adds a device source without changing actions, contexts or commands.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_ControlPath_XrPathsReserved` and `Test_InputScriptApi_ExposesActionsOnly` in the `Test` step (win-debug, win-release, linux-tsan) and the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"

### Z2-R-PLAT-051 Bindings file schema and validation

- **Statement:** A `*.bindings.json` file follows the bindings schema exactly, the loader reports every error of a file in one pass with file and line, and an engine, game or platform file that fails stops startup with exit code 3.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_BindingsFile_EachValidationRule` in the `Test` step (win-debug, win-release, linux-tsan) and `Zenith_Fuzz_BindingsJson` in the `win-asan` fuzz smoke.
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"; `Zenith2/Docs/Design/PLAT/Input.md` → "Validation rules"

### Z2-R-PLAT-052 Binding layers and provenance

- **Statement:** `LoadBindings` reads the engine, game, platform and user layers in order after the configuration is built, overrides are keyed by context, action and binding IDs, and every resolved binding records the chain of layers, files and lines that defined it.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_BindingsFile_LayerOrderAndProvenance` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Binding layers and provenance"; `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputSystem API"

### Z2-R-PLAT-053 Engine actions and contexts

- **Statement:** The engine bindings file declares the UI, system and development actions and the `Engine.UI`, `Engine.TextEntry`, `Engine.Dev` and `Engine.Simulation` contexts with the specified IDs and defaults, and development-only entries are absent from Shipping builds.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_BindingsFile_ShippingSkipsDevelopmentOnly` (Shipping test build) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Engine actions and contexts"

### Z2-R-PLAT-054 Input context stacks

- **Statement:** A presentation stack and one simulation stack per world hold at most 16 contexts each, pinned contexts stay on top, a push of a present context moves it to the top, the simulation stack changes only through `Input.PushContext`/`Input.PopContext` commands effective from the next phase 1, and refused pushes are counted, never asserted.
- **Covers:** Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Context_StackFullRefused` and `Test_Context_PinnedTopAndWindowScoped` in the `Test` step (win-debug, win-release, linux-tsan), `Test_InputPhase_ContextCommandsApplyNextTick` in the `Zenith2/Tests/Input/` suite.
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks"

### Z2-R-PLAT-055 Consumption rules

- **Statement:** Contexts are evaluated from claims and text entry, then the presentation stack, then the simulation stack; a consuming binding's controls are hidden from lower contexts until rest, a consumed transition is seen below as the control at rest, and releases reach every context that saw the press.
- **Covers:** Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Context_ConsumptionReleasesLowerHeld` and `Test_Context_ReleaseReachesContextsThatSawPress` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Consumption rules"

### Z2-R-PLAT-056 Presentation claims

- **Statement:** Claimants registered by UI and the editor claim new pointer presses before gestures and mapping and claim handled actions after the presentation mapper; claimed pointers never reach gestures and are seen at rest by simulation, and claimed actions consume their controls for the frame.
- **Covers:** Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Claims_PointerClaimRemovesFromSimulation` and `Test_Claims_ActionClaimConsumesControls` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Presentation claims"

### Z2-R-PLAT-057 Deterministic action mapper

- **Statement:** `Zenith_ActionMapper::Map` performs the mapper algorithm's seven steps, resynchronises with device state at the slice end so no action stays stuck after a dropped transition, and gives byte-identical results for identical inputs.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_ActionMapper_SameInputsSameOutputs`, `Test_ActionMapper_ResyncRepairsDroppedRelease` and `Test_ActionMapper_ContextRemovalCancelsInteraction` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm"; `Zenith2/Docs/Design/PLAT/Input.md` → "Determinism of the mapper"

### Z2-R-PLAT-058 Interactive rebinding

- **Statement:** One rebind operation at a time follows the rebind state table (wait for rest, listen, candidate, cancel, timeout), ignores motion and reserved controls, and the control pressed to rebind triggers no action in any context.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Rebind_StateTableEveryCell` and `Test_Rebind_CaptureTriggersNoAction` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Rebind operation"

### Z2-R-PLAT-059 Rebinding conflict detection

- **Statement:** A rebind detects conflicts with bindings of simultaneously active contexts sharing a conflict group, including composite parts of one binding, and resolves them by the requested reject, swap, unbind-other or allow policy.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Rebind_ConflictPoliciesEach` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Conflict detection"

### Z2-R-PLAT-060 Persisted user bindings

- **Statement:** User overrides are written to `user:/config/input.bindings.json` only through `WriteAtomic` at one priority, debounced, submitted without waiting on background entry and flushed with a wait at shutdown, completing before `Zenith_AsyncIO` is destroyed; orphaned overrides and unknown keys survive rewrites byte for byte; a newer-version file is never overwritten without a backup.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_UserBindings_PersistAndReload`, `Test_UserBindings_OrphansPreserved`, `Test_UserBindings_NewerVersionNotOverwritten` and `Test_UserBindings_FlushBeforeIOShutdown` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings"

### Z2-R-PLAT-061 Corrupt user bindings quarantined

- **Statement:** An unparsable user bindings file is renamed with a UTC timestamp (three kept), the defaults apply, and `PLATFORM_EVENT_CONFIG_RESET` is raised with the input-bindings reason.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UserBindings_CorruptFileQuarantined` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings"

### Z2-R-PLAT-062 Reset bindings to defaults

- **Statement:** `ResetUserBindings` removes the user overrides of exactly one binding, action, context or all contexts, rebuilds the table and schedules the user-file write.
- **Covers:** Z2-I-PLAT-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UserBindings_ResetScopes` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "Reset to defaults"

### Z2-R-PLAT-063 Input phase system per world

- **Statement:** Each world registers the `Input.Actions` system in phase 1 with its declared resource reads and writes, and only the input world maps transitions; every other world maps an empty slice.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-03, Z2-I-PLAT-11
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-PLAT-001
- **Acceptance evidence:** `Test_InputPhase_FirstTickTakesFrameInput` and `Test_InputPhase_OnlyInputWorldReceives` in the `Zenith2/Tests/Input/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system"

### Z2-R-PLAT-064 Tick assignment of input

- **Statement:** A frame's transitions belong to the input world's first tick of the frame, catch-up ticks receive none, transitions of zero-tick frames are carried (bounded, counted) to the next tick and cleared after a `RunFrame` in which the input world ran a tick, and nothing is carried while paused.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_InputPhase_FirstTickTakesFrameInput` and `Test_InputPhase_ZeroTickFramesCarry` in the `Zenith2/Tests/Input/` integration suite, and `Test_InputDevices_CarryFrameTransitions` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events"

### Z2-R-PLAT-065 Quantised input commands

- **Statement:** Phase 1 encodes the mapper's changes as `Zenith_InputCommand`s with the world's `Zenith_InputCommandEncoder`, which keeps the last quantised value it emitted per action, by the command value encoding and emission rules, in causing-transition order with per-player sequences, at most 4,096 per tick with buttons kept last; a dropped command leaves a `dropped` marker so the value is re-emitted at the next tick, and `Reset` returns both the encoder and `m_xStates` to rest, so the last-emitted value always equals `m_xStates`.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_InputCommand_EncodeDecodeEveryType`, `Test_InputCommand_EncoderTracksLastEmitted` and `Test_InputCommand_EmissionOrderAndCap` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"

### Z2-R-PLAT-066 Simulation state rebuilt from commands

- **Statement:** The simulation action state of every tick is rebuilt from that tick's commands by `Zenith_ApplyInputCommands` in live runs and replays alike, and decoding a malformed or unknown command skips it with a counter, never an assert.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_InputCommand_LiveEqualsReplay` in the `Zenith2/Tests/Input/` integration suite and `Zenith_Fuzz_InputCommandDecode` in the `win-asan` fuzz smoke.
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"

### Z2-R-PLAT-067 Replay record and scripted input

- **Statement:** `--record` logs phase 1's commands, `--replay` feeds the log's commands to phase 1 instead of devices, and `--input-script` applies action events as commands and control events through a virtual device, with exclusive mode ignoring physical devices, the file opened only through a command-line grant or a content path, and validated under its limits.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_InputCommand_LiveEqualsReplay`, `Test_InputScript_ActionAndControlEvents`, `Test_InputScript_LoadsFromGrantOrContentPath` and `Test_InputScript_LimitsAndBadFile`, plus `Zenith_Fuzz_InputScript`.
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Replay and scripted input"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format"

### Z2-R-PLAT-068 Input latency attribution

- **Statement:** Phase 1 records the newest consumed event sequence and timestamp and the latency-probe sequence in `Zenith_InputTickSummary`, which extraction carries to the present, and the MP-LATENCY probe is bound to `Dev.LatencyProbe` in `Engine.Simulation`.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-027
- **Acceptance evidence:** `Test_Latency_ProbeSequenceReachesSummary` in the `Zenith2/Tests/Input/` integration suite and MP-LATENCY evidence records of the Stage 3 workload runs.
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Latency attribution"

### Z2-R-PLAT-069 Script-facing input API

- **Statement:** Scripts read action state and phases through the `Input` namespace functions by action name, push and pop contexts and play haptics only as buffered commands, and a name the bindings do not declare fails the module at load with the unknown-name misuse error.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-03, Z2-I-PLAT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** `Test_InputScriptApi_UnknownActionRaisesAtLoad` and `Test_InputScriptApi_ExposesActionsOnly` in the `Zenith2/Tests/Input/` integration suite.
- **Design:** `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"

### Z2-R-PLAT-070 Input phase CPU within budget

- **Statement:** The `Input.Actions` phase-1 node's p95 span stays within the row "Input phase CPU" on the reference desktop and the S24 in every frame-rate workload.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-PLAT-001
- **Acceptance evidence:** MP-CPUZONE evidence records of the WS-FRAME workload runs in the Stage 3 acceptance, judged against Z2-B-PLAT-001 once allocated.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT owns"

### Z2-R-PLAT-071 Gamepad and haptics device run

- **Statement:** A Bluetooth and a USB controller connect, disconnect and reconnect mid-play on the S24 and on Windows without stuck actions, rumble and trigger rumble are felt, and the phone vibrator responds in touch play.
- **Covers:** Z2-I-PLAT-04, Z2-I-PLAT-05, Z2-I-PLAT-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` gamepad-and-haptics device run record on the S24 and the reference desktop, attached to the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"

### Z2-R-PLAT-072 Input debug overlay data

- **Statement:** `Zenith_InputSystem::WriteDebugSnapshot` provides every row of the input debug overlay: devices, active scheme, both context stacks, action values and phases, recent transitions, claims, gesture states and the command and drop counters.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-02, Z2-I-PLAT-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_InputSystem_DebugSnapshotRows` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Input debug overlay"

### Z2-R-PLAT-073 Virtual path type

- **Statement:** `Zenith_VirtualPath` stores a validated path inline without allocation, is constructed only by `Zenith_ParseVirtualPath`, `Root` and a validating `Append`, compares by bytes, and is canonical without any normalisation step.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualPath_AppendValidates` in the `Test` step (win-debug, win-release, linux-tsan) and the re-parse property of `Zenith_Fuzz_VirtualPath`.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualPath"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Normalisation"

### Z2-R-PLAT-074 Path validation rules in order

- **Statement:** `Zenith_ParseVirtualPath` applies rules R1 to R10 in order (length, encoding, root, separator, absolute, forbidden characters, traversal, component limits, reserved names, user namespace) and returns the first failure as a `Zenith_PathError`, never an assert. Rule R10 accepts a `user:/` first component of `config`, `saves`, `cache`, `logs` or `crash` in every build and `devtools` only where `Zenith_IsShippingBuild()` is false, selected by `if constexpr` with no layout-changing conditional; in a Shipping build a `user:/devtools/` path fails with `PATH_ERROR_USER_NAMESPACE` at byte offset 6.
- **Covers:** Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualPath_EachRuleRejects`, `Test_VirtualPath_ReservedNamesEveryForm`, `Test_VirtualPath_EncodingEdgeCases`, `Test_VirtualPath_UserCrashRootEveryBuild` and `Test_VirtualPath_UserDevtoolsRootOutsideShipping` in the `Test` step (win-debug, win-release, linux-tsan); `Test_VirtualPath_UserCrashRootEveryBuild` and `Test_VirtualPath_UserDevtoolsRefusedInShipping` in the Shipping test build (`win-shipping-test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Reserved device names"

### Z2-R-PLAT-075 Path-traversal fuzz target and corpus

- **Statement:** `Zenith_Fuzz_VirtualPath` fuzzes the parser from the committed seed corpus and finds no crash and no accepted path with a dot, dot-dot or empty component, a backslash or a colon after the root, and every seed's expected result is also asserted in the ordinary test step.
- **Covers:** Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualPath_Fuzz` (the `Zenith_Fuzz_VirtualPath` target) in the `win-asan` fuzz smoke and its full-duration run in stage acceptance, and `Test_VirtualPath_CorpusExpectations` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Path-traversal fuzz corpus"

### Z2-R-PLAT-076 Escape check on the opened file

- **Statement:** OS paths are formed only in `ZenithPlatform/Internal/`, and every open of a directory-backed mount verifies the file the OS actually opened lies inside its mount root after link resolution, failing with the escapes-mount error otherwise; created directories are checked the same way.
- **Covers:** Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_SymlinkEscapeRejected` and `Test_VirtualPath_ResolvedNeverEscapesRoot` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution"

### Z2-R-PLAT-077 Case-exact path matching

- **Statement:** Virtual paths are case-sensitive on every platform; on Windows the long final name of the opened file is compared with the request by bytes, so a path that differs only in case fails with `PATH_ERROR_CASE_MISMATCH` and one that opens through an NTFS 8.3 short name or any other alias fails with `PATH_ERROR_NAME_MISMATCH`, on reads and on the directories and temporary files the write path creates alike.
- **Covers:** Z2-I-PLAT-13, Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_CaseMismatchOnWindows` and `Test_VirtualFileSystem_ShortNameAliasRejected` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Case policy"

### Z2-R-PLAT-078 Mount precedence

- **Statement:** The four mounts package (1), patch (2), loose (3) and user (4) exist with their sources, `content:/` resolves through loose, patch and package in that order with sources ordered by manifest order, and `Resolve` reports the mount and source used.
- **Covers:** Z2-I-PLAT-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_PrecedenceOrder` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm"

### Z2-R-PLAT-079 User mount isolation

- **Statement:** `user:/` paths resolve in the user mount alone under `config/`, `saves/`, `cache/`, `logs/`, `crash/` and, outside Shipping, `devtools/`, the user mount never serves a `content:/` path, and every write goes to the user mount and nowhere else.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-075
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_UserNeverShadowsContent` and `Test_AsyncIO_SubmitValidation` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime"

### Z2-R-PLAT-080 Loose mount absent in Shipping

- **Statement:** The loose mount is created only in non-Shipping builds, and a Shipping build refuses to add a loose source with `FILE_ERROR_MOUNT_NOT_ALLOWED`.
- **Covers:** Z2-I-PLAT-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_LooseAbsentInShipping` (Shipping test build) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots"

### Z2-R-PLAT-081 Mount sources and mount-table snapshots

- **Statement:** Mounts are built from `Zenith_MountSource` implementations (directory, APK assets, and the PACK archive reader), mount changes swap a new table under the mount lock while in-flight requests keep the table they resolved in, and `Zenith_VirtualFileSystem::Create` unwinds on failure.
- **Covers:** Z2-I-PLAT-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_MountSwapDoesNotTearReads` and `Test_VirtualFileSystem_Create_FailsAtEachStep_Unwinds` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API"

### Z2-R-PLAT-082 A hostile higher mount stops resolution

- **Statement:** A lookup error other than not-found in a higher-precedence mount (an escape, a case mismatch, a corrupt archive entry) stops resolution and is reported, never bypassed to a lower mount.
- **Covers:** Z2-I-PLAT-13, Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_HostileHigherMountStopsSearch` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm"

### Z2-R-PLAT-083 Mount manifest

- **Statement:** The package mount is built from `mount-manifest.json` validated by its schema under the parser limits; an invalid or absent manifest stops a Shipping build with exit code 3 and leaves the package mount empty in development builds.
- **Covers:** Z2-I-PLAT-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_MountManifest_EachRule` and `Test_MountManifest_ShippingRequiresManifest` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema"

### Z2-R-PLAT-084 Play Asset Delivery packs mounted after verification

- **Statement:** Install-time asset packs mount with the package. Fast-follow and on-demand packs are installed, verified and mounted by PACK's installer alone, through PLAT's primitives (the public directory-source factory, `ReadFromSource`, and `AddSource` and `RemoveSource` in the frame prologue); PLAT parses and validates `assetPacks[]`, mounts only the `installTime` entries, and exposes no asset-pack mount call.
- **Covers:** Z2-I-PLAT-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_MountManifest_InstallTimePacksOnly` in the `Test` step (win-debug, win-release, linux-tsan); PACK's installer mount tests (`Test_AssetPackInstaller_FetchVerifyMount_HappyPath`, `Test_AssetPackInstaller_RemoveMounted_WaitsForRelease`, `Test_AssetPackInstaller_PartialMountRollback_LateReleaseIgnored`, `ZenithPackage_Tests`, `Test` step), which mount through PLAT's `AddSource` and the directory-source factory; and the Stage 6 S24 Play Asset Delivery device run record.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema"

### Z2-R-PLAT-085 Patch mount accepts only verified patches

- **Statement:** The patch mount mounts the highest patch version whose update manifest verifies (signature, release line, target build, directory name, base content hash, every file's hash and size), never mounts a failed patch in part, leaves the previous content mounted when verification fails, and changes only inside `Zenith_VirtualFileSystem::Create`: patches install before the virtual filesystem exists, never while the game runs.
- **Covers:** Z2-I-PLAT-13
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** `Test_PatchMount_HighestVerifiedMounted`, `Test_PatchMount_TamperCasesRejected` and `Test_PatchMount_DirectoryNameAndBaseContent` in the `Test` step (win-debug, win-release, linux-tsan), part of the Stage 6 security hardening evidence.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification"

### Z2-R-PLAT-086 Synchronous file access rules

- **Statement:** Synchronous reads, stat, exists and listing serve startup, tools, I/O threads and dedicated threads only; a synchronous read from a job worker fails the blocking-wait check, whole-file reads are capped, and listings are the sorted union over mounts.
- **Covers:** Z2-I-PLAT-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_SyncReadOnJobWorkerChecks` (`z2-death-check`) and `Test_VirtualFileSystem_ListUnionSorted` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Synchronous file access"

### Z2-R-PLAT-087 Command-line host files

- **Statement:** Files named by command-line options are opened only through a `Zenith_CommandLineFileGrant`, which PLAT declares in `Zenith_HostFile.h` and only `Zenith_CommandLine` constructs, read or created new without truncation, evidence files are created only inside the granted `--evidence-dir` directory under single-component names, and both are refused in Shipping builds.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_CommandLineHostFile_GrantOnly` in `ZenithRuntime_Tests` (`Zenith2/Engine/ZenithRuntime/Tests/Zenith_CommandLineHostFile_Tests.cpp`, grants obtained from `Zenith_CommandLine::Parse`) in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files"

### Z2-R-PLAT-088 Asynchronous I/O service

- **Statement:** `Zenith_AsyncIO` offers read, atomic write, delete, rename and directory creation as operation handles, validates every submission without asserting, and owns its I/O threads, which its destructor cancels, drains and joins.
- **Covers:** Z2-I-PLAT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_SubmitValidation`, `Test_AsyncIO_ShutdownCancelsAndJoins` and `Test_AsyncIO_Create_FailsAtEachStep_Unwinds` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime"

### Z2-R-PLAT-089 Priority queues

- **Statement:** Five priority queues of `io.queueCapacity` entries each reject when full (after sweeping cancelled entries), requests start in strict priority order and in submission order within a priority except that a request whose target path is in the in-flight set (a started write, delete or rename of that path) is passed over without spinning, and queue counters are published.
- **Covers:** Z2-I-PLAT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_PriorityOrder` and `Test_AsyncIO_QueueFullRejects` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "I/O scheduler algorithm"

### Z2-R-PLAT-090 Chunked I/O with preemption

- **Statement:** Reads and writes larger than `io.chunkBytes` run in chunks, and at a chunk boundary a running request yields to a strictly higher-priority request and resumes before any request of its own priority submitted after it; a rename step is never preempted.
- **Covers:** Z2-I-PLAT-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_PreemptAtChunkBoundary` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Chunking and preemption"

### Z2-R-PLAT-091 I/O cancellation

- **Statement:** A request cancelled while queued does no I/O; one cancelled mid-read reports cancelled only after the destination is no longer touched; one cancelled mid-write deletes its temporary file; a started rename completes; destroying an incomplete handle cancels and detaches.
- **Covers:** Z2-I-PLAT-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_CancelQueuedAndInFlight`, `Test_AsyncIO_HandleDestroyedCancelsAndDetaches` and `Test_AsyncIO_AtomicWriteInterrupted` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation"

### Z2-R-PLAT-092 I/O cancellation stress

- **Statement:** One thousand issue-then-cancel cycles at randomised stages leave no open file, temporary file, operation slot or allocation behind.
- **Covers:** Z2-I-PLAT-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_CancelStress1000Cycles` (`z2-stress`) in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation"

### Z2-R-PLAT-093 I/O completion delivery

- **Statement:** Completions are observed through operation handles or completion queues whose slots are reserved at submission, and I/O results reach simulation only through their consumers' tick-boundary publication in completion-ID order.
- **Covers:** Z2-I-PLAT-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_CompletionQueueReserved` in the `Test` step (win-debug, win-release, linux-tsan) and `Test_Script_Async_PublishedAtTickBoundaryInCompletionOrder` of the SCRIPT suite for the publication rule.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Completion delivery"

### Z2-R-PLAT-094 Atomic writes

- **Statement:** `WriteAtomic` performs steps W1 to W7 (validate, free space, parents, temporary file, chunks, flush, rename) so an interruption leaves the complete old or new file, stale `.z2atomic` files are removed at creation, operations on one target path submitted at one priority land in submission order (across priorities a higher-priority one may overtake a queued lower one), and delete, rename (both paths in flight) and directory creation follow their rules.
- **Covers:** Z2-I-PLAT-15, Z2-I-PLAT-19, Z2-I-PLAT-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_AtomicWriteInterrupted`, `Test_AsyncIO_SameTargetWritesSerialised` and `Test_AsyncIO_DeleteRenameCreateDirectory` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Other user-mount operations"

### Z2-R-PLAT-095 Async I/O thread-sanitizer stress

- **Statement:** The mixed-priority read, write and cancel stress with 8 submitters and 2 and 4 I/O threads completes every operation exactly once with intact destinations and no TSan or UBSan report.
- **Covers:** Z2-I-PLAT-15, Z2-I-PLAT-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_StressUnderTsan` in the `Sanitizers -Preset linux-tsan` step and every stage acceptance's full sanitizer set.
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "linux-tsan async I/O stress"

### Z2-R-PLAT-096 Low-disk refusal before writes

- **Statement:** Before every write step that can grow the volume, free space minus the write size is compared with `io.lowDiskMarginBytes`, and a write below the margin is refused with `IO_ERROR_LOW_DISK` before any file is created or opened; reads, deletes and renames are never refused for low disk.
- **Covers:** Z2-I-PLAT-18, Z2-I-PLAT-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_LowDiskRefusesWrite` and `Test_AsyncIO_ReadsNeverRefusedForLowDisk` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Free-space checks"

### Z2-R-PLAT-097 Disk-full during a write

- **Statement:** A write that meets the OS disk-full error deletes its temporary file, leaves the target untouched, reports `IO_ERROR_DISK_FULL`, and enters the low-disk state.
- **Covers:** Z2-I-PLAT-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_DiskFullMidWriteCleansUp` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Free-space checks"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"

### Z2-R-PLAT-098 Low-disk state machine

- **Statement:** The low-disk state enters LOW on a refusal, a disk-full error or a poll below the margin, raises one event per entry, counts refusals while LOW, and clears only when a poll finds free space at the clear factor times the margin.
- **Covers:** Z2-I-PLAT-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_LowDiskStateMachine` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Low-disk state machine"

### Z2-R-PLAT-099 Low-disk event raised to the game

- **Statement:** Entering and leaving the low-disk state raises `PLATFORM_EVENT_LOW_DISK` and `PLATFORM_EVENT_LOW_DISK_CLEARED` at the next pump with free bytes, margin and reason, and `GetLowDiskState` and `QueryFreeBytes` expose it to UI.
- **Covers:** Z2-I-PLAT-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AsyncIO_LowDiskRefusesWrite` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Surfacing to the game"

### Z2-R-PLAT-100 Low disk observable by scripts

- **Statement:** The low-disk events are published to every world at the next tick boundary, recorded in the replay log, and observed by scripts as `Platform.LowDisk` and `Platform.LowDiskCleared` in phase 9.
- **Covers:** Z2-I-PLAT-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PlatformEvents_LowDiskReachesScript` in the `Zenith2/Tests/PlatformEvents/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Surfacing to the game"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement"

### Z2-R-PLAT-101 Six configuration layers

- **Statement:** Configuration resolves from the engine default, platform, game, quality-tier, user and command-line layers in that precedence, reading the platform, game, tier and user files through the virtual filesystem, with the game tier file overriding the engine tier file within the quality-tier layer.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_LayerPrecedence` and `Test_Config_TierSubSourceOrderGameOverEngine` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Layer table"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Engine and game sub-sources"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Resolution algorithm"

### Z2-R-PLAT-102 Quality-tier two-pass resolution

- **Statement:** `quality.tier` is resolved without the quality-tier layer, validated against the platform's tier list (falling back with a warning), and then selects the quality-tier files applied as layer 4.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_QualityTierTwoPass` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier resolution"

### Z2-R-PLAT-103 Configuration keys registered before the store

- **Statement:** Every library and module registers its keys into ZenithCore's frozen schema before `Zenith_Config::Create`, ZenithPlatform through `Zenith_RegisterPlatformConfigKeys` and modules through their `m_pfnRegisterTypes`, at the start of startup step 6, and `Create` refuses a schema in which a simulation-affecting key is not restart-required, a restart-required key allows the quality-tier layer, or a tier-selection key allows it.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_SchemaRulesEnforced` and `Test_Config_PlatKeysRegistered` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys of libraries below ZenithPlatform"

### Z2-R-PLAT-104 Early keys from the command line only

- **Statement:** Keys consumed before startup step 6 accept only the engine default and the command line, and the value used to build `Zenith_PlatformDesc` equals the value the configuration reports after step 6.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_EarlyKeysRejectFileLayers` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Early keys"

### Z2-R-PLAT-105 Invalid values rejected per key

- **Statement:** A value in a disallowed layer, of the wrong type, out of range, not an enum name or nested is rejected by the store with a warning naming key, source and line, the lower layer's value stays, unknown keys are warned once with their source and line, and a missing required key fails creation naming it.
- **Covers:** Z2-I-PLAT-21, Z2-I-PLAT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_DisallowedLayerIgnored`, `Test_Config_EachRejectionInChain`, `Test_Config_UnknownKeysWarnedWithSource` and `Test_Config_RequiredKeyMissingFailsCreate` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Resolution algorithm"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Rejection reasons"

### Z2-R-PLAT-106 Configuration provenance and chain

- **Statement:** Every value records its winning provenance (layer, source, line), and `GetProvenanceChain` returns, lowest layer first, the accepted entry of every layer that set the key and every rejection of it, for the process lifetime, with runtime user writes carrying their writer's source (`settings` or `thermal`).
- **Covers:** Z2-I-PLAT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_LayerPrecedence` and `Test_Config_EachRejectionInChain` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Provenance chain"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Rejection reasons"

### Z2-R-PLAT-107 Configuration file rules

- **Statement:** Layer files are read in ZenithCore's format through the virtual filesystem, with content files capped at 1 MiB and the user file at 256 KiB before parsing, and command-line values applied in argument order.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_OversizedFileRejected` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration file schema"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Value encoding per type"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Command-line layer"

### Z2-R-PLAT-108 Replay runs use recorded simulation keys

- **Statement:** In a `--replay` run the replay header's simulation-affecting list is applied after every `--set` as the last command-line sub-source, so it wins, and the superseded `--set` is marked in the chain with a warning.
- **Covers:** Z2-I-PLAT-21, Z2-I-PLAT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_ReplayListOverridesSet` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Simulation-affecting keys"

### Z2-R-PLAT-109 User-layer writes at the prologue

- **Statement:** `SetUserValue` validates a write that takes effect only at prologue step 3 with one change callback per changed key, while a restart-required key's user value persists, keeps `Get` at the startup value and reports pending until the next launch.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_UserWriteAppliedAtPrologue`, `Test_Config_UserWriteSameKeyTwiceInFrame` and `Test_Config_RestartRequiredPending` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "User-layer writes"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Restart-required keys"

### Z2-R-PLAT-110 Persisted user configuration

- **Statement:** The user layer is written to `user:/config/user.config.json` through `WriteAtomic` at one priority, debounced, submitted without waiting on background entry and flushed with a wait at shutdown, byte-stable and preserving unknown members, a low-disk refusal is retried after the low-disk state clears, and the shutdown flush completes before `Zenith_AsyncIO` is destroyed.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_UserFilePersistedDebouncedAtomic`, `Test_Config_UserFileLowDiskRetried`, `Test_Config_FlushBeforeIOShutdown` and `Test_Config_BackgroundSubmitDoesNotWait` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer"

### Z2-R-PLAT-111 Corrupt configuration fallback

- **Statement:** A corrupt content configuration file contributes nothing and every key falls back to lower layers without stopping startup, a corrupt user file is quarantined with defaults applied and `PLATFORM_EVENT_CONFIG_RESET` raised, and provenance always names the source that supplied each value.
- **Covers:** Z2-I-PLAT-21, Z2-I-PLAT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_CorruptUserFileQuarantined` and `Test_Config_CorruptContentFileIgnoredWhole` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling"

### Z2-R-PLAT-112 Configuration dump

- **Statement:** `WriteDump` and `--config-dump` write the byte-stable `zenith2.config-dump` document holding the store's key dump, every key's chain, pending-restart keys, unknown keys, rejected files and binding provenance, and the on-screen profiler's configuration panel shows the same data.
- **Covers:** Z2-I-PLAT-22
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_DumpContainsProvenance` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump"

### Z2-R-PLAT-113 PLAT configuration keys

- **Statement:** ZenithPlatform registers exactly the keys of the PLAT key table with their types, defaults, ranges, allowed layers and restart flags.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_PlatKeysRegistered` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers"

### Z2-R-PLAT-114 Configuration facade contract and unwinding

- **Statement:** `Zenith_Config` is main-thread only and immutable after creation except user-layer writes through `SetUserValue` committed at the frame prologue, owns the one `Zenith_ConfigStore` as its only writer, returns the store's key errors and its own source errors, and `Create` unwinds on failure at every step.
- **Covers:** Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_Create_FailsAtEachStep_Unwinds` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Zenith_Config API"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Ownership split with CORE"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration error enumerations"

### Z2-R-PLAT-115 Runtime quality-tier changes

- **Statement:** A quality-tier change is a user-layer write of `quality.tier` (the player's choice, persisted) or `quality.activeTier` (thermal adaptation, never persisted); a player write of `quality.tier` also clears `quality.activeTier` in the same commit; the commit clears and reloads the quality-tier layer at the prologue, reports exactly the changed keys, and never changes a restart-required or simulation-affecting key, because neither may admit that layer.
- **Covers:** Z2-I-PLAT-21, Z2-I-PLAT-28
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_TierChangeReresolves` and `Test_Config_PlayerTierChangeClearsActiveTier` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier changes"

### Z2-R-PLAT-116 Application lifecycle state machine

- **Statement:** The lifecycle follows exactly the lifecycle state machine table, its Android events arrive through an SDL event watch on the UI thread that only queues and waits, and every transition runs its actions on the main thread in the next pump, with the listener calls in the listed order (on background entry `OnStopAudio`, `OnEnterBackground`, then `OnWriteResumeState`).
- **Covers:** Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_AppLifecycle_StateMachine` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "The event watch"

### Z2-R-PLAT-117 Background pauses at a tick boundary

- **Statement:** Entering the background pauses every world at a tick boundary and releases the surface before the watch returns, while the RHI device, its resources and every world stay alive.
- **Covers:** Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Lifecycle_BackgroundPausesAtTickBoundary` in the `Zenith2/Tests/Lifecycle/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"

### Z2-R-PLAT-118 Surface-loss handshake

- **Statement:** The watch waits at most 2,000 ms for the main thread to release the surface at its next frame boundary; on a timeout it marks the surface lost and every subsequent present on that window reports out-of-date and records no GPU work until recovery.
- **Covers:** Z2-I-PLAT-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_AppLifecycle_HandshakeTimeout`, `Test_AppLifecycle_HandshakeAcknowledgedWithinFrame` and `Test_AppLifecycle_SurfaceLostFlagAfterTimeout` in the `Test` step (win-debug, win-release, linux-tsan), and `Test_AppLifecycle_PresentAfterTimeoutIsOutOfDate` in `ZenithRuntime_Tests` in the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"

### Z2-R-PLAT-119 Surface recovery on foreground

- **Statement:** On returning to the foreground, or after a cancelled suspend, the first pump with a native window recreates the surface and swapchain through the listener and emits `PLATFORM_EVENT_SURFACE_AVAILABLE`, and a recreation failure follows the renderer's recovery policy.
- **Covers:** Z2-I-PLAT-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AppLifecycle_CancelledSuspendRecreatesSurface` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Surface recovery"

### Z2-R-PLAT-120 Surface destruction callback verified

- **Statement:** The Stage 2 Android lifecycle ticket verifies which pinned-SDL3 callback carries Android's surface destruction, records the finding as a ruling in its own diff, and the watch treats the first of that notification and `WILL_ENTER_BACKGROUND` as the handshake trigger.
- **Covers:** Z2-I-PLAT-24
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** The ruling entry appended to `Zenith2/Docs/DecisionLog.md` by that ticket, and `Test_AppLifecycle_SurfaceDestroyedFirstIsHandshake` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"; `Zenith2/Docs/Design/PLAT.md` → "Rulings PLAT records in its own tickets"

### Z2-R-PLAT-121 S24 suspend and resume device run

- **Statement:** Fifty background and foreground cycles on the S24 recover the surface every time with no validation error.
- **Covers:** Z2-I-PLAT-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` S24 suspend-and-resume device run record, attached to the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"; `Zenith2/Docs/Design/PLAT/Android.md` → "Android and lifecycle tests"

### Z2-R-PLAT-122 Throttling and termination

- **Statement:** While backgrounded or with every window minimised or occluded the frame loop waits for events for up to 100 ms per iteration, and `TERMINATING` submits the user layers without waiting and requests shutdown.
- **Covers:** Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AppLifecycle_ThrottleWhenBackground` and `Test_AppLifecycle_StateMachine` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Windows equivalents"; `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"

### Z2-R-PLAT-123 Resume state written on background

- **Statement:** On every `DID_ENTER_BACKGROUND` on Android, unless `platform.resumeOnRelaunch` is false, the runtime writes `user:/saves/resume.zsave` through SAVE's journaled write, and a write not complete within 3,000 ms is cancelled and leaves no torn file.
- **Covers:** Z2-I-PLAT-25
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ResumeState_WrittenOnBackgroundOnly` and `Test_ResumeState_DeadlineAbandonsAndCleans` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence"

### Z2-R-PLAT-124 Resume state restored on relaunch

- **Statement:** At launch a resume file is restored before the first tick when build ID, composition hash and content hash all match, otherwise discarded with its reason logged; the file is deleted after a restored run's first tick, at once on a discard, and on the foreground of a process that was not killed.
- **Covers:** Z2-I-PLAT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ResumeState_MatchingIdentityRestores`, `Test_ResumeState_UnreadableAndRestoreFailedFallBack`, `Test_ResumeState_ForegroundDeletesLiveFile` and `Test_ResumeState_DeletedAfterFirstTick`.
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Relaunch decision"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume identity"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence"

### Z2-R-PLAT-125 S24 process-death device run

- **Statement:** A process killed in the background on the S24 relaunches into its resume state, and a relaunch after installing a new build discards it with the build reason.
- **Covers:** Z2-I-PLAT-25, Z2-I-PLAT-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` S24 process-death device run record, attached to the Stage 3 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"

### Z2-R-PLAT-126 Low-memory signal relayed

- **Statement:** Android's low-memory signal becomes `PLATFORM_EVENT_LOW_MEMORY` at the next pump in every lifecycle state and is relayed by the runtime to the memory governor as CRITICAL.
- **Covers:** Z2-I-PLAT-23
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_LowMemory_RelayedToGovernorAsCritical` in the `Zenith2/Tests/Lifecycle/` integration suite.
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Low memory relay"

### Z2-R-PLAT-127 S24 memory-pressure device run

- **Statement:** A critical trim-memory signal on the S24 reaches the memory governor as CRITICAL and the shedding counters move.
- **Covers:** Z2-I-PLAT-23
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` S24 low-memory device run record, attached to the Stage 6 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"

### Z2-R-PLAT-128 Thermal headroom sampling

- **Statement:** `Zenith_ThermalMonitor` samples the provider's headroom every 2 s inside `Platform.Pump`, exposes it through `GetLatestSample()` and applies its actions in prologue step 5 through `ApplyActions`, receives status changes through the Android listener, ignores and counts invalid samples, and uses the null provider on Windows and Linux, which never adapts.
- **Covers:** Z2-I-PLAT-27
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Thermal_InvalidSamplesIgnored` and `Test_Thermal_NullProviderNoAdaptation` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_ThermalMonitor API"; `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal providers"

### Z2-R-PLAT-129 Headroom drives dynamic resolution

- **Statement:** Headroom at or above `thermal.drsHeadroom` lowers the resolution cap by one `thermal.drsStep` per sample, never below the dynamic-resolution floor.
- **Covers:** Z2-I-PLAT-29
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_Thermal_StateMachineEveryRow` and `Test_Thermal_ResolutionNeverBelowFloor` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions"

### Z2-R-PLAT-130 Headroom drives the quality tier

- **Statement:** Headroom at or above `thermal.tierHeadroom` or a SEVERE or worse status drops the quality tier by one level, at most once per `thermal.tierCooldownSeconds`, and headroom at or below `thermal.recoverHeadroom` for `thermal.recoverSeconds` undoes one step, resolution before tier; after a player change of `quality.tier` the next evaluation re-bases the held offset on the new choice.
- **Covers:** Z2-I-PLAT-28, Z2-I-PLAT-29
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Thermal_StateMachineEveryRow` and `Test_Thermal_RebasesAfterPlayerTierChange` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions"

### Z2-R-PLAT-131 Thermal shutdown status

- **Statement:** Entering the SHUTDOWN thermal status requests one checkpoint save and continues at the lowest tier and resolution cap.
- **Covers:** Z2-I-PLAT-28
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Thermal_ShutdownRequestsOneCheckpoint` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"

### Z2-R-PLAT-132 Thermal adaptation touches presentation only

- **Statement:** Thermal adaptation changes only the resolution cap and the quality tier, never simulation state, tick rate or anything a replay records.
- **Covers:** Z2-I-PLAT-27, Z2-I-PLAT-28, Z2-I-PLAT-29
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Thermal_TierChangeTouchesNoSimulation` in the `Zenith2/Tests/Lifecycle/` integration suite.
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions"

### Z2-R-PLAT-133 Thermal telemetry

- **Statement:** Headroom, status, resolution steps, tier offset and invalid samples are published as counters, and the budget-run mode samples the thermal status at 1 Hz into the sustained-run evidence record.
- **Covers:** Z2-I-PLAT-27
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PlatformCounters_Published` in the `Test` step (win-debug, win-release, linux-tsan) and the MP-SUSTAIN evidence records.
- **Design:** `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal telemetry"

### Z2-R-PLAT-134 S24 thermal adaptation device run

- **Statement:** A 30-minute S24 run shows resolution and tier steps following headroom, with the thermal counters exported.
- **Covers:** Z2-I-PLAT-27, Z2-I-PLAT-28, Z2-I-PLAT-29
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The `needs-human` S24 thermal device run record, attached to the Stage 5 acceptance ticket.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Device tests"

### Z2-R-PLAT-135 Sustained thermal qualification

- **Statement:** Every WS-SUSTAIN workload meets the sustained floor on the S24 under the sustained run procedure with thermal adaptation active and the dynamic-resolution floor respected.
- **Covers:** Z2-I-PLAT-27, Z2-I-PLAT-28, Z2-I-PLAT-29
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-SHIP-006, Z2-B-SHIP-007, Z2-B-SHIP-008, Z2-B-SHIP-009, Z2-B-SHIP-010, Z2-B-SHIP-011
- **Acceptance evidence:** MP-SUSTAIN evidence records for WS-SUSTAIN on the S24, committed under `Zenith2/Docs/Research/Budgets/Stage6/` and read by the Stage 6 acceptance.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT is measured against"; `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal telemetry"

### Z2-R-PLAT-136 Account signals

- **Statement:** Windows session notifications and signals posted by online backends through `PostAccountSignal` from any thread become `PLATFORM_EVENT_ACCOUNT_CHANGED` at the next pump, never earlier, through a 64-entry queue that drops the oldest.
- **Covers:** Z2-I-PLAT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074
- **Budgets:** none
- **Acceptance evidence:** `Test_AccountSignal_DeliveredAtNextPump` and `Test_AccountSignal_QueueDropsOldest` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Account signal sources"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery"

### Z2-R-PLAT-137 Account changes reach online services before gameplay

- **Statement:** The runtime hands each account-change event to the online-services adapter before publishing it to every world at the next tick boundary, where scripts observe `Platform.AccountChanged`.
- **Covers:** Z2-I-PLAT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-074, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_PlatformEvents_AccountChangedOnlineFirst` in the `Zenith2/Tests/PlatformEvents/` integration suite, the `Test` step (win-debug, win-release).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement"

### Z2-R-PLAT-138 No raw account identifiers

- **Statement:** Account events, logs and replay logs carry only a hash of the platform account identifier, never the raw identifier or a display name.
- **Covers:** Z2-I-PLAT-30
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_AccountSignal_NoRawIdentifier` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery"

### Z2-R-PLAT-139 Loopback development listener

- **Statement:** Development listeners bind 127.0.0.1 unless `devtools.bindAddress` was set on the command line and accept only connections that present the per-session token within 5 s, with every socket error recoverable.
- **Covers:** Z2-I-SCRIPT-06, Z2-I-DEVT-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_DevListener_LoopbackByDefault` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket"

### Z2-R-PLAT-140 Development file watcher

- **Statement:** The file watcher reports loose-mount changes as coalesced virtual-path records, drops paths that fail validation, and turns an OS buffer overflow into a root rescan record.
- **Covers:** Z2-I-SCRIPT-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_FileWatcher_CoalescesAndMapsToVirtualPaths` and `Test_FileWatcher_OverflowRequestsRescan` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "File watcher"

### Z2-R-PLAT-141 Development services absent from Shipping

- **Statement:** In Shipping builds the development listener and file watcher return `DEV_SERVICE_ERROR_NOT_IN_SHIPPING` and their socket and watcher code is not linked into any Shipping executable.
- **Covers:** Z2-I-SEC-09, Z2-I-SEC-10
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_DevListener_ShippingReturnsNotInShipping` (Shipping test build) and the `Libgraph` Shipping forbidden-symbol check.
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion"

### Z2-R-PLAT-142 Process watchdog

- **Statement:** `Zenith_Platform::StartWatchdog` starts one platform-owned timer thread that, when the run has not shut down within the given seconds, logs the watchdog line and ends the process immediately with the passed exit code, and is joined without firing when the platform is destroyed first.
- **Covers:** Z2-I-NET-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Platform_WatchdogFiresWithExitCode` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Platform.md` → "Process watchdog"

### Z2-R-PLAT-143 Stage 1 PLAT budget harness

- **Statement:** The Stage 1 per-system budget harness for PLAT builds, under `Zenith2/Benchmarks/Budgets/PLAT/`, the MP-MEM S24 sampler (VmRSS and kgsl readers, the runner-side dumpsys sampler) with its device test, and the reference-engine input-phase measurement for Z2-B-PLAT-001, each with a CTest smoke run.
- **Covers:** Z2-I-PLAT-01, Z2-I-DEVT-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-012
- **Budgets:** Z2-B-PLAT-001, Z2-B-CORE-001
- **Acceptance evidence:** The harness's CTest smoke in the `Test` step and the PLAT ruling ticket's report and allocation under `Zenith2/Docs/Research/Budgets/Stage1/PLAT/`.
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "The Stage 1 PLAT harness"

### Z2-R-PLAT-144 Platform counters and log categories

- **Statement:** ZenithPlatform publishes every counter of the counter table through the profiler hooks with the table's counter kind (cumulative, or gauge), none as a delta, and logs under the listed categories.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-12, Z2-I-PLAT-15, Z2-I-PLAT-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PlatformCounters_Published` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Counters"; `Zenith2/Docs/Design/PLAT.md` → "Log categories"

### Z2-R-PLAT-145 Resolution cache never changes a result

- **Statement:** The `content:/` resolution cache of `vfs.resolveCacheEntries` entries changes only the cost of `Resolve`, never its result: it caches only successful resolutions to the package and patch mounts, never a negative result or a loose-mount resolution, and is cleared on every mount change.
- **Covers:** Z2-I-PLAT-12, Z2-I-PLAT-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_VirtualFileSystem_ResolveCacheTransparent` in the `Test` step (win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API"

### Z2-R-PLAT-146 XR pose values in action state

- **Statement:** In Stage 7 `Zenith_InputValue` widens to hold an `INPUT_ACTION_TYPE_POSE` state (position and orientation), the bindings loader accepts the `Pose` action type for XR controls, and POSE commands use the reserved command encoding, with no change to any other action type or to the command layout.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_InputValue_PoseRoundTrip` in the `Test` step (win-debug, win-release, linux-tsan) (Stage 7).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"

### Z2-R-PLAT-147 XR interaction profiles in the platform bindings layer

- **Statement:** In Stage 7 the platform-layer bindings file accepts an optional `interactionProfile` key per binding set naming an OpenXR interaction-profile path, validated against the profiles the XR device source reports, with files without the key unchanged.
- **Covers:** Z2-I-PLAT-01
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** `Test_Bindings_InteractionProfileKey` in the `Test` step (win-debug, win-release, linux-tsan) (Stage 7).
- **Design:** `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"

### Z2-R-PLAT-148 Stage 2 measurement of the PLAT rows 002 to 006

- **Statement:** At Stage 2 the PLAT measurement harness `Zenith_Bench_Platform` measures each of the rows Z2-B-PLAT-002 to Z2-B-PLAT-006 (platform pump CPU, async I/O critical read latency, resume-state write duration, configuration and bindings build time at startup, platform and input memory) with one case per row, and a `cmake -P` comparator checks a harness report against each allocated row of `Zenith2/Docs/Budgets.md` → "Row index", reporting a breach as a benchmark regression and never reporting an unallocated row as met.
- **Covers:** Z2-I-PLAT-01, Z2-I-PLAT-15, Z2-I-PLAT-21, Z2-I-PLAT-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-PLAT-002, Z2-B-PLAT-003, Z2-B-PLAT-004, Z2-B-PLAT-005, Z2-B-PLAT-006
- **Acceptance evidence:** `Zenith_Bench_Platform --smoke` and the four `PlatBudget_Check_*` CTests in the `Test` step (win-debug); the Stage 2 desktop and S24 measurement records under `Zenith2/Docs/Research/Budgets/Stage2/PLAT/` (a `needs-human` device run).
- **Design:** `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT owns"; `Zenith2/Docs/Design/PLAT.md` → "Benchmarks and content files"

### Z2-R-PLAT-149 Accessibility service and its backend selection

- **Statement:** ZenithPlatform declares `Zenith_AccessibilityService` in `Zenith_Accessibility.h` with PLAT-owned value types (`Zenith_AccessibilityNode`, `Zenith_AccessibilityTreeView`, `Zenith_AccessibilityAction`, `Zenith_AccessibilityPreferences`, `Zenith_AccessibilityState`) and creates it in platform creation step 11 when `Zenith_PlatformDesc::m_eAccessibilityBackend`, the early key `platform.accessibility.backend`, resolves to OS (windowed Windows or Android) or RECORDING (any host); `Zenith_Platform::GetAccessibility()` returns null for NONE, for `os` without a window (with a warning), and when an OS backend fails to start, and an OS-side failure never fails platform creation.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-046, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_BackendSelection` and the `Accessibility` fault point of `Test_Platform_Create_FailsAtEachStep_Unwinds` (`ZenithPlatform_Tests`, `Test`, win-debug, win-release, linux-tsan); `ApiContractCheck` over `Zenith_Accessibility.h`.
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityService"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Backend table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Service creation and destruction"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc"

### Z2-R-PLAT-150 Tree provider called only on the main thread

- **Statement:** UI publishes through the PLAT-declared `Zenith_AccessibilityTreeProvider`, so ZenithPlatform names no higher-library type; PLAT calls `GetTree` only on the main thread and only inside `SetTreeProvider` and `NotifyTreeChanged`, never from an OS thread or a JNI callback; at most one provider is registered (`ACCESSIBILITY_ERROR_PROVIDER_ALREADY_SET`), and `SetTreeProvider(nullptr)` publishes an empty tree with a structure change.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_ProviderRegistration`, `Test_Accessibility_ProviderCalledOnlyFromPublication` (`ZenithPlatform_Tests`, `Test`); the `Libgraph` step (no ZenithPlatform edge to ZenithUI).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityTreeProvider"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Ownership split with UI"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Publication algorithm"

### Z2-R-PLAT-151 Tree validation and the published snapshot

- **Statement:** `NotifyTreeChanged` validates the provider's tree against rules V1 to V9 (at most 4,096 nodes and 256 KiB of pool, unique non-zero IDs, parents before children, known roles, states and actions, text ranges inside the pool of at most 512 bytes of valid UTF-8, non-negative bounds, finite in-range values, one consistent focused node, heading level and set position), rejects an invalid tree with the published snapshot unchanged, treats an equal revision as a no-op and a lower one as `ACCESSIBILITY_ERROR_STALE_REVISION`, copies an accepted tree into a PLAT-owned snapshot and computes the focus, structure, node and live-region changes, capping node events at 64 per publication.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_TreeValidationRules`, `Test_Accessibility_RevisionRules`, `Test_Accessibility_SnapshotIndependentOfProvider`, `Test_Accessibility_DiffRecordsEachChange` (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Tree validation rules"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Publication algorithm"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Change detection"

### Z2-R-PLAT-152 OS queries served from the snapshot

- **Statement:** Every OS query (UI Automation provider methods on any thread, the Android natives on the UI thread) reads the published snapshot through the snapshot readers under `uLOCK_LEVEL_ACCESSIBILITY` (640, short-hold), copying into caller storage and releasing the lock before any allocation, system call or callback; node lookup is O(log n) and the hit test returns the deepest visible node of the first root containing the point.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_HitTestFrontmostDeepest`, `Test_Accessibility_ProviderCalledOnlyFromPublication` (`ZenithPlatform_Tests`, `Test`); `Test_Accessibility_ConcurrentQueriesDuringPublish` under `Sanitizers -Preset linux-tsan`.
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Snapshot queries"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Hit testing"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The accessibility lock"

### Z2-R-PLAT-153 Screen-reader detection

- **Statement:** The service reports whether a screen reader is active (Windows `SPI_GETSCREENREADER`; Android `AccessibilityManager.isTouchExplorationEnabled`) and whether an OS client wants the tree (Windows `UiaClientsAreListening`; Android `AccessibilityManager.isEnabled`), refreshes both at creation, on the OS change notification and every 2,000 ms on Windows, and turns any change of `Zenith_AccessibilityState` into one presentation-only `PLATFORM_EVENT_ACCESSIBILITY_CHANGED` at the next pump, never world-visible.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_StateChangeRaisesPresentationEvent` (`ZenithPlatform_Tests`, `Test`); the Narrator and TalkBack device runs (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Screen-reader detection"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm"

### Z2-R-PLAT-154 Windows UI Automation fragment provider

- **Statement:** On Windows the OS backend subclasses the main window to answer `WM_GETOBJECT` with a UI Automation fragment root whose node providers expose each published node with the control type of the role table, its name, help text, enabled, focus, live setting, position, bounding rectangle in screen pixels and runtime ID, navigate by the snapshot links, offer the Invoke, Toggle, SelectionItem, RangeValue, Value and ScrollItem patterns per the table, return `UIA_E_ELEMENTNOTAVAILABLE` for a vanished node, and raise focus, structure, property and live-region events outside the lock while clients listen.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_UiaProviderReportsNodes`, `Test_Accessibility_UiaPatternsQueueActions` (`ZenithPlatform_Tests`, `Test`, win-debug, windowed); the Narrator device run (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Windows provider lifetime"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Role to control type table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Properties"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Patterns and actions"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Events raised"

### Z2-R-PLAT-155 Android node provider through JNI

- **Statement:** On Android the OS backend registers the natives of `Zenith_AccessibilityJniTable.h` against the Gradle template's `org.zenith2.player.Zenith2Accessibility`, whose `AccessibilityNodeProvider` on the game view builds each `AccessibilityNodeInfo` from `nativeQueryNode` with stable positive virtual view IDs, the class names and properties of the mapping table, screen bounds and hover hit testing, converts text with `Zenith_Utf8ToUtf16` and `NewString`, and receives content-change and focus events after each publication; the Java class matches the native table exactly.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-046, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_VirtualIdsStable`, `Test_Accessibility_VirtualQueryArrays`, `Test_Accessibility_JavaMatchesNativeRegistration` (`ZenithPlatform_Tests`, `Test`); the TalkBack device run on the S24 (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Virtual view IDs"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Native entry points"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Node information mapping"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android events"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android bridge lifetime"

### Z2-R-PLAT-156 Announcements to the screen reader

- **Statement:** `Announce(strText, bAssertive)` rejects empty text, invalid UTF-8 and U+0000 with `ACCESSIBILITY_ERROR_BAD_TEXT`, cuts text at a UTF-8 boundary at or below 512 bytes, and delivers it to the screen reader: `UiaRaiseNotificationEvent` with `NotificationProcessing_ImportantMostRecent` when assertive and `NotificationProcessing_All` otherwise on Windows, `announceForAccessibility` after `AccessibilityManager.interrupt()` when assertive on Android, and an ANNOUNCEMENT record on the recording backend; announcements never reach built-in speech.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_SpeechValidationAndRecords` (`ZenithPlatform_Tests`, `Test`); the Narrator and TalkBack device runs (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Announcements"

### Z2-R-PLAT-157 Built-in text to speech

- **Statement:** `Speak(strText, fRate, bInterrupt)` validates the text and a rate in [0.25, 4.0], returns `ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE` without a voice, and speaks through SAPI on the `Zenith.Speech` thread (rate `round(10 ln(fRate) / ln 3)` clamped to ±10, `SPF_PURGEBEFORESPEAK` on interrupt, a 32-command `DROP_OLDEST` queue) on Windows and `TextToSpeech` (`QUEUE_FLUSH` on interrupt) on Android; `StopSpeaking` discards current and queued speech, `IsSpeaking` reports the engine, and entering the background stops speech.
- **Covers:** Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_SpeechValidationAndRecords`, `Test_Accessibility_SapiRateMapping`, `Test_Accessibility_SpeechQueueInterruptAndOverflow`, `Test_Accessibility_BackgroundStopsSpeech` (`ZenithPlatform_Tests`, `Test`); the built-in speech device run on Windows and the S24 (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Built-in text to speech"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Windows SAPI speech thread"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android TextToSpeech"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Rate mapping"

### Z2-R-PLAT-158 Focus speech hold

- **Statement:** `IsFocusSpeechPending()` is true from a publication that changes the focused node while a screen reader is active until the first pump at least 600 ms after it, and false whenever no screen reader is active, so UI can keep live regions and announcements from talking over the screen reader's own focus speech.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_FocusSpeechHoldWindow` (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Focus speech hold"

### Z2-R-PLAT-159 OS-requested actions

- **Statement:** OS requests (UI Automation patterns and `SetFocus`; Android `ACTION_CLICK`, scroll forward and backward, `ACTION_SET_PROGRESS`, `ACTION_SHOW_ON_SCREEN`, `ACTION_FOCUS`, accessibility focus) become `Zenith_AccessibilityAction` values per the mapping table, are rejected and counted for an unknown node or an action the node does not advertise, are stamped with the published revision and queued in a 64-entry `DROP_OLDEST` ring with its counter triple, and `DrainActions` returns them oldest first on the main thread, leaving any that do not fit queued.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_ActionQueueOrderAndOverflow` (`ZenithPlatform_Tests`, `Test`), `Test_Accessibility_UiaPatternsQueueActions` (win-debug, windowed); the Narrator and TalkBack device runs (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Action mapping table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Action queue"

### Z2-R-PLAT-160 OS accessibility preferences read

- **Statement:** The service reads reduced motion, high contrast and text scale from the OS (Windows `SPI_GETCLIENTAREAANIMATION`, `SPI_GETHIGHCONTRAST`, `TextScaleFactor`; Android `ANIMATOR_DURATION_SCALE`, `UiModeManager.getContrast`, `Configuration.fontScale`) at creation and on change, normalised by the two pure normalisation functions to `Zenith_AccessibilityPreferences` with the text scale clamped to [1.0, 2.0] and a reported-field mask.
- **Covers:** Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_PreferenceNormalisation` (`ZenithPlatform_Tests`, `Test`); the preference device run on Windows and the S24 (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "OS preference sources"

### Z2-R-PLAT-161 OS preferences in the platform configuration layer

- **Statement:** PLAT registers `platform.accessibility.reducedMotion`, `platform.accessibility.highContrast` and `platform.accessibility.textScale` (engine default, platform and command-line layers), and `Zenith_Config` applies the reported OS preferences as the `os:accessibility` sub-source of `CONFIG_LAYER_PLATFORM` after the cooked platform file in `Create`, and again in `ApplyQueuedUserWrites` when the service's state revision changed, reporting each changed key through `GetChangedKeys` and one change callback, never writing them to the user file.
- **Covers:** Z2-I-UI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Config_OsAccessibilityPreferencesInPlatformLayer`, `Test_Config_OsAccessibilityChangeAppliedAtPrologue`, `Test_Config_PlatKeysRegistered` (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "OS accessibility preference changes"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers"

### Z2-R-PLAT-162 Recording implementation on every test host

- **Statement:** The RECORDING backend, selected by `platform.accessibility.backend = "recording"` or the desc on every host and configuration, runs the shared snapshot, validation, diff, action-queue, speech-validation and focus-hold code, replaces every OS call by a record of `Zenith_AccessibilityRecordKind` in a 4,096-record, 1 MiB log that drops the newest record when full, and takes its screen-reader state, tree request, speech availability and preferences from test access, so it is a declared seam and not a test double.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_RecordLogDropsNewest`, `Test_Accessibility_DiffRecordsEachChange`, `Test_Accessibility_SpeechValidationAndRecords` (`ZenithPlatform_Tests`, `Test`, win-debug, win-release, linux-tsan).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The recording implementation"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Record log"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Recording behaviour per member"

### Z2-R-PLAT-163 Accessibility test access

- **Statement:** `Zenith_AccessibilityTestAccess.h`, a public test-access header defined in ZenithPlatform's `Internal/` sources and compiled into every configuration, gives other libraries' tests the record log, the published tree, action injection and the recording backend's state setters, is reachable only from `Tests/`, `Zenith2/Benchmarks/` and ZenithPlatform's `Internal/` sources, and checks that the state setters run against a RECORDING service.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `TestSeparationCheck` (rule 6) and `ApiContractCheck` over `Zenith_AccessibilityTestAccess.h`; `Test_Accessibility_ActionQueueOrderAndOverflow` through test access (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityTestAccess API"

### Z2-R-PLAT-164 Accessibility lifetime and shutdown

- **Statement:** The service is destroyed first among the platform's services, before any window: on Windows it removes the window subclass, returns a null root, disconnects every provider with `UiaDisconnectAllProviders` so no UI Automation object stays live, and joins `Zenith.Speech`; on Android `detach()` guarantees no native is entered afterwards before the global references are deleted; a provider still registered is reported as a defect and never called.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_UiaDisconnectReleasesProviders` (win-debug, windowed), `Test_Platform_Create_FailsAtEachStep_Unwinds` (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility shutdown order"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility thread affinity"; `Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown"

### Z2-R-PLAT-165 Accessibility diagnostics and failure handling

- **Statement:** The service publishes the `platform.accessibility.*` counters of its table with their kinds, logs under the category `accessibility` without ever logging node text, and handles each failure of its failure table without failing the platform: no service when the OS backend cannot start, no built-in speech without a voice, the previous snapshot kept for a rejected tree or a failed growth, and rejected OS actions counted.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_CountersPublished`, `Test_Accessibility_TreeValidationRules`, `Test_Accessibility_BackendSelection` (`ZenithPlatform_Tests`, `Test`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility counters"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility log lines"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility failure handling"

### Z2-R-PLAT-166 Accessibility concurrency under the thread sanitizer

- **Statement:** Eight threads running the snapshot readers while the main thread publishes 10,000 revisions always copy a node consistent with one revision and run clean under ThreadSanitizer, proving that OS-thread queries and publication share only `uLOCK_LEVEL_ACCESSIBILITY`.
- **Covers:** Z2-I-UI-19
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test_Accessibility_ConcurrentQueriesDuringPublish` (`ZenithPlatform_Tests`, `Test` on every host and `Sanitizers -Preset linux-tsan`).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility tests"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The accessibility lock"

### Z2-R-PLAT-167 Accessibility device runs

- **Statement:** On the Windows reference desktop and the S24, Narrator and TalkBack read, navigate and operate the demo tree of `Zenith_Bench_AccessibilityBridge --interactive`, built-in speech speaks at rates 0.5, 1.0 and 2.0 with interruption and stops on backgrounding, and OS preference changes reach `platform.accessibility.*` without a restart; the harness's publication and query cases run in `--smoke` form in the `Test` step.
- **Covers:** Z2-I-UI-19, Z2-I-UI-20, Z2-I-UI-27
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Zenith_Bench_AccessibilityBridge --smoke` (`Test`, win-debug); the run records under `Zenith2/Docs/Research/Platform/Stage4/DeviceRuns/Accessibility/` (`needs-human`, Stage 4).
- **Design:** `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility device runs"; `Zenith2/Docs/Design/PLAT.md` → "Device tests"

## Coverage of inventory items

| Inventory item | Item | Stage | Requirements |
|---|---|---|---|
| Z2-I-PLAT-01 | action mapping | S3 | Z2-R-PLAT-005, Z2-R-PLAT-007, Z2-R-PLAT-009, Z2-R-PLAT-018, Z2-R-PLAT-020, Z2-R-PLAT-021, Z2-R-PLAT-022, Z2-R-PLAT-023, Z2-R-PLAT-024, Z2-R-PLAT-038, Z2-R-PLAT-044, Z2-R-PLAT-045, Z2-R-PLAT-046, Z2-R-PLAT-047, Z2-R-PLAT-048, Z2-R-PLAT-049, Z2-R-PLAT-050, Z2-R-PLAT-051, Z2-R-PLAT-053, Z2-R-PLAT-057, Z2-R-PLAT-063, Z2-R-PLAT-064, Z2-R-PLAT-065, Z2-R-PLAT-066, Z2-R-PLAT-067, Z2-R-PLAT-068, Z2-R-PLAT-069, Z2-R-PLAT-070, Z2-R-PLAT-072, Z2-R-PLAT-143, Z2-R-PLAT-144, Z2-R-PLAT-146, Z2-R-PLAT-147, Z2-R-PLAT-148, Z2-R-PLAT-175, Z2-R-PLAT-176 |
| Z2-I-PLAT-02 | rebinding | S3 | Z2-R-PLAT-023, Z2-R-PLAT-045, Z2-R-PLAT-049, Z2-R-PLAT-052, Z2-R-PLAT-058, Z2-R-PLAT-059, Z2-R-PLAT-060, Z2-R-PLAT-061, Z2-R-PLAT-062, Z2-R-PLAT-072 |
| Z2-I-PLAT-03 | input contexts | S3 | Z2-R-PLAT-012, Z2-R-PLAT-042, Z2-R-PLAT-053, Z2-R-PLAT-054, Z2-R-PLAT-055, Z2-R-PLAT-056, Z2-R-PLAT-063, Z2-R-PLAT-069, Z2-R-PLAT-072 |
| Z2-I-PLAT-04 | controllers | S3 | Z2-R-PLAT-019, Z2-R-PLAT-021, Z2-R-PLAT-025, Z2-R-PLAT-026, Z2-R-PLAT-047, Z2-R-PLAT-049, Z2-R-PLAT-071 |
| Z2-I-PLAT-05 | hotplug | S3 | Z2-R-PLAT-006, Z2-R-PLAT-008, Z2-R-PLAT-019, Z2-R-PLAT-027, Z2-R-PLAT-028, Z2-R-PLAT-029, Z2-R-PLAT-071 |
| Z2-I-PLAT-06 | haptics | S3 | Z2-R-PLAT-030, Z2-R-PLAT-031, Z2-R-PLAT-032, Z2-R-PLAT-069, Z2-R-PLAT-071 |
| Z2-I-PLAT-07 | touch | S3 | Z2-R-PLAT-024, Z2-R-PLAT-033, Z2-R-PLAT-034, Z2-R-PLAT-039 |
| Z2-I-PLAT-08 | gestures | S3 | Z2-R-PLAT-035, Z2-R-PLAT-036, Z2-R-PLAT-037, Z2-R-PLAT-038, Z2-R-PLAT-039 |
| Z2-I-PLAT-09 | IME | S3 | Z2-R-PLAT-016, Z2-R-PLAT-040, Z2-R-PLAT-041, Z2-R-PLAT-042, Z2-R-PLAT-043 |
| Z2-I-PLAT-10 | DPI | S2 | Z2-R-PLAT-001, Z2-R-PLAT-002, Z2-R-PLAT-003, Z2-R-PLAT-004, Z2-R-PLAT-006, Z2-R-PLAT-007, Z2-R-PLAT-010, Z2-R-PLAT-013, Z2-R-PLAT-014, Z2-R-PLAT-015, Z2-R-PLAT-038 |
| Z2-I-PLAT-11 | multiple windows | S3 | Z2-R-PLAT-004, Z2-R-PLAT-006, Z2-R-PLAT-010, Z2-R-PLAT-011, Z2-R-PLAT-012, Z2-R-PLAT-013, Z2-R-PLAT-023, Z2-R-PLAT-063 |
| Z2-I-PLAT-12 | a virtual filesystem | S2 | Z2-R-PLAT-001, Z2-R-PLAT-017, Z2-R-PLAT-073, Z2-R-PLAT-079, Z2-R-PLAT-081, Z2-R-PLAT-086, Z2-R-PLAT-087, Z2-R-PLAT-094, Z2-R-PLAT-144, Z2-R-PLAT-145, Z2-R-PLAT-170, Z2-R-PLAT-171, Z2-R-PLAT-174 |
| Z2-I-PLAT-13 | mount precedence (package, patch, loose, user) | S2 | Z2-R-PLAT-077, Z2-R-PLAT-078, Z2-R-PLAT-079, Z2-R-PLAT-080, Z2-R-PLAT-082, Z2-R-PLAT-083, Z2-R-PLAT-084, Z2-R-PLAT-085, Z2-R-PLAT-145, Z2-R-PLAT-168, Z2-R-PLAT-169 |
| Z2-I-PLAT-14 | path-traversal protection | S2 | Z2-R-PLAT-073, Z2-R-PLAT-074, Z2-R-PLAT-075, Z2-R-PLAT-076, Z2-R-PLAT-077, Z2-R-PLAT-082, Z2-R-PLAT-087, Z2-R-PLAT-174 |
| Z2-I-PLAT-15 | async I/O | S2 | Z2-R-PLAT-009, Z2-R-PLAT-017, Z2-R-PLAT-088, Z2-R-PLAT-093, Z2-R-PLAT-094, Z2-R-PLAT-095, Z2-R-PLAT-144, Z2-R-PLAT-148, Z2-R-PLAT-170 |
| Z2-I-PLAT-16 | async I/O priorities | S2 | Z2-R-PLAT-089, Z2-R-PLAT-090 |
| Z2-I-PLAT-17 | async I/O cancellation | S2 | Z2-R-PLAT-091, Z2-R-PLAT-092, Z2-R-PLAT-095 |
| Z2-I-PLAT-18 | low-disk detection | S2 | Z2-R-PLAT-096, Z2-R-PLAT-098 |
| Z2-I-PLAT-19 | low-disk behaviour: refuse writes safely | S2 | Z2-R-PLAT-094, Z2-R-PLAT-096, Z2-R-PLAT-097 |
| Z2-I-PLAT-20 | low-disk behaviour: surface the condition to the game | S2 | Z2-R-PLAT-008, Z2-R-PLAT-099, Z2-R-PLAT-100 |
| Z2-I-PLAT-21 | configuration layering (engine default → platform → game → quality tier → user → command line) | S2 | Z2-R-PLAT-101, Z2-R-PLAT-102, Z2-R-PLAT-103, Z2-R-PLAT-104, Z2-R-PLAT-105, Z2-R-PLAT-107, Z2-R-PLAT-108, Z2-R-PLAT-109, Z2-R-PLAT-110, Z2-R-PLAT-111, Z2-R-PLAT-113, Z2-R-PLAT-114, Z2-R-PLAT-115, Z2-R-PLAT-144, Z2-R-PLAT-148 |
| Z2-I-PLAT-22 | configuration provenance | S2 | Z2-R-PLAT-105, Z2-R-PLAT-106, Z2-R-PLAT-108, Z2-R-PLAT-111, Z2-R-PLAT-112 |
| Z2-I-PLAT-23 | Android lifecycle | S2 | Z2-R-PLAT-001, Z2-R-PLAT-002, Z2-R-PLAT-003, Z2-R-PLAT-005, Z2-R-PLAT-006, Z2-R-PLAT-018, Z2-R-PLAT-116, Z2-R-PLAT-117, Z2-R-PLAT-122, Z2-R-PLAT-126, Z2-R-PLAT-127, Z2-R-PLAT-172 |
| Z2-I-PLAT-24 | Android surface recovery | S3 | Z2-R-PLAT-118, Z2-R-PLAT-119, Z2-R-PLAT-120, Z2-R-PLAT-121 |
| Z2-I-PLAT-25 | process death on Android: save a resumable state on background | S3 | Z2-R-PLAT-123, Z2-R-PLAT-125, Z2-R-PLAT-148 |
| Z2-I-PLAT-26 | process death on Android: restore on relaunch | S3 | Z2-R-PLAT-124, Z2-R-PLAT-125 |
| Z2-I-PLAT-27 | thermal adaptation: the thermal-headroom API | S5 | Z2-R-PLAT-128, Z2-R-PLAT-132, Z2-R-PLAT-133, Z2-R-PLAT-134, Z2-R-PLAT-135 |
| Z2-I-PLAT-28 | thermal adaptation: headroom drives the quality tier | S5 | Z2-R-PLAT-115, Z2-R-PLAT-130, Z2-R-PLAT-131, Z2-R-PLAT-132, Z2-R-PLAT-134, Z2-R-PLAT-135 |
| Z2-I-PLAT-29 | thermal adaptation: headroom drives dynamic resolution | S5 | Z2-R-PLAT-129, Z2-R-PLAT-130, Z2-R-PLAT-132, Z2-R-PLAT-134, Z2-R-PLAT-135 |
| Z2-I-PLAT-30 | account changes | S4 | Z2-R-PLAT-008, Z2-R-PLAT-136, Z2-R-PLAT-137, Z2-R-PLAT-138 |

### Items of other areas with a PLAT half

These items stay covered by their own area's catalogue; the requirements below are the platform primitive each one needs.

| Inventory item | Item | Owning area | PLAT requirements |
|---|---|---|---|
| Z2-I-DEVT-16 | memory profiling | DEVT | Z2-R-PLAT-143 |
| Z2-I-PACK-07 | Play Asset Delivery (the platform primitives the installer calls) | PACK | Z2-R-PLAT-170, Z2-R-PLAT-172 |
| Z2-I-PACK-10 | patch manifests (the verifier and the patch mount's stamp skip) | PACK | Z2-R-PLAT-168, Z2-R-PLAT-169 |
| Z2-I-PACK-11 | DLC manifests (the verifier PACK's DLC mounting calls) | PACK | Z2-R-PLAT-168, Z2-R-PLAT-169 |
| Z2-I-PACK-14 | interrupted content installation (the connectivity signal of the network retry) | PACK | Z2-R-PLAT-173 |
| Z2-I-DEVT-18 | remote Android diagnostics | DEVT | Z2-R-PLAT-139 |
| Z2-I-NET-05 | headless execution | NET | Z2-R-PLAT-142 |
| Z2-I-SCRIPT-06 | the DAP debugger | SCRIPT | Z2-R-PLAT-139 |
| Z2-I-SCRIPT-14 | hot reload | SCRIPT | Z2-R-PLAT-140 |
| Z2-I-SEC-09 | no debugger exposed in shipping builds | SEC | Z2-R-PLAT-141 |
| Z2-I-SEC-10 | no remote tools exposed in shipping builds | SEC | Z2-R-PLAT-141 |
| Z2-I-UI-13 | accessibility settings (the OS preferences they default from) | UI | Z2-R-PLAT-160, Z2-R-PLAT-161 |
| Z2-I-UI-19 | narration: screen-reader hooks | UI | Z2-R-PLAT-149, Z2-R-PLAT-150, Z2-R-PLAT-151, Z2-R-PLAT-152, Z2-R-PLAT-153, Z2-R-PLAT-154, Z2-R-PLAT-155, Z2-R-PLAT-156, Z2-R-PLAT-158, Z2-R-PLAT-159, Z2-R-PLAT-162, Z2-R-PLAT-163, Z2-R-PLAT-164, Z2-R-PLAT-165, Z2-R-PLAT-166, Z2-R-PLAT-167 |
| Z2-I-UI-20 | narration: TTS hooks | UI | Z2-R-PLAT-149, Z2-R-PLAT-157, Z2-R-PLAT-162, Z2-R-PLAT-163, Z2-R-PLAT-165, Z2-R-PLAT-167 |
| Z2-I-UI-27 | accessibility end-to-end flow with a screen reader (the platform bridge's device runs) | UI | Z2-R-PLAT-167 |

## Excluded and deferred

No PLAT inventory item is excluded or deferred. Capabilities near PLAT's scope that it does not provide:

| Capability | Classification for PLAT | Rationale |
|---|---|---|
| local multiplayer and split-screen input (more than one player slot) | excluded | Z2-D-076; every device belongs to player slot 0, and commands carry a slot only for NET's remote players |
| mod mounts and third-party content loading | excluded | Z2-D-075; the user mount never provides content ([Z2-N-ARCH-920]) |
| XR input (an OpenXR device source and its tickets) | deferred to Stage 7, owned by SCOPE | Z2-D-072; the model is XR-ready (`Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"), and PLAT's two additive Stage 7 changes are its own requirements (the POSE value and the `interactionProfile` key) |
| console input and storage | deferred to Stage 7, owned by SCOPE | Z2-D-071 |
| a DirectStorage-style GPU decompression I/O path | not provided | no decision requires it; asynchronous I/O reads into CPU memory and decompression is COMPRESS's |
| the binding editor and remapping screen | owned by EDIT and UI | PLAT supplies the model, provenance, display data and the rebind operation (`Zenith2/Docs/Design/PLAT.md` → "Editor surfaces PLAT exposes") |
| the configuration value types, schema and store | owned by CORE | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" |
| the monotonic and wall clocks | owned by CORE | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks" |
| the crash handler | owned by DEVT | D-SEL-CRASH (open, Stage 2); its library, once admitted, is owned by ZenithPlatform |
