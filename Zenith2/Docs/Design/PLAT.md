# PLAT design: platform, input, filesystem, configuration and lifecycle

This is the entry document of the PLAT design family: the detailed specification of the `ZenithPlatform` library and the platform-facing services every other area builds on. It holds the purpose, the scope, the index of the split files, the library layout, the top-level contracts (threading, errors, diagnostics, failure handling, tests, budgets, stages, decisions) and the requirement index; the split files under `Zenith2/Docs/Design/PLAT/` hold the rest. It elaborates the SPEC baselines `Zenith2/Docs/Architecture/PlatformLayer.md` and `Zenith2/Docs/Architecture/BuildSystem.md` and never contradicts them; where it adds detail the plan did not state, the line before carries `<!-- elaboration -->`, and where it assumes an interface another area has not yet published, the block is marked `<!-- assumption: pending <AREA> -->`. Every obligation is traced to a requirement of `Zenith2/Docs/Requirements/PLAT.md`.

## Purpose and scope

### What PLAT owns

PLAT owns the `ZenithPlatform` library and, through it:

| Capability | Specified in |
|---|---|
| the platform object, SDL3 initialisation, the event pump, platform events | `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Platform lifecycle", "The event pump" |
| windows (one in a game, up to 16 in the editor), native handles and Vulkan surfaces, display metrics, DPI, safe area, clipboard | `Zenith2/Docs/Design/PLAT/Platform.md` → "Windows", "Display metrics and DPI", "Clipboard" |
| the monotonic clock | `Zenith2/Docs/Design/PLAT/Platform.md` → "The monotonic clock" |
| input devices, controllers, hotplug, touch, pen, gestures, IME, haptics | `Zenith2/Docs/Design/PLAT/Input.md` → "Devices and controls" through "Haptics" |
| actions, bindings, contexts, the action mapper, rebinding, the bindings file | `Zenith2/Docs/Design/PLAT/Input.md` → "Actions" through "Rebinding" |
| input as replayable commands in phase 1, scripted input, latency attribution, the Luau input surface | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Simulation input", "Script-facing input API" |
| the virtual filesystem, mounts and precedence, path-traversal protection, the mount manifest, patch-mount verification | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Virtual paths", "Mounts", "The mount manifest" |
| asynchronous I/O with priorities and cancellation, atomic writes, low-disk detection and behaviour | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Asynchronous I/O", "Low-disk detection and behaviour" |
| configuration layering with provenance | `Zenith2/Docs/Design/PLAT/Configuration.md` |
| the Android lifecycle and surface recovery, process death and restore, the low-memory relay, thermal adaptation | `Zenith2/Docs/Design/PLAT/Android.md` |
| account-change signals | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account changes" |
| the development-only loopback listener and file watcher | `Zenith2/Docs/Design/PLAT/Platform.md` → "Development services" |
| the platform accessibility service: screen-reader detection, the Windows UI Automation and Android `AccessibilityNodeProvider` bridges, announcements, built-in speech (SAPI, `TextToSpeech`), OS-requested actions, the OS accessibility preferences in the configuration store, and the recording implementation tests select | `Zenith2/Docs/Design/PLAT/Accessibility.md` |
| the `linux-tsan` headless test host | `Zenith2/Docs/Design/PLAT/Platform.md` → "The headless test host" |
| the Stage 1 per-system budget harness for PLAT (the MP-MEM S24 sampler) | § "The Stage 1 PLAT harness" |

### Explicit exclusions

| Not PLAT's | Owner, with the decision where one exists |
|---|---|
| local multiplayer and split-screen: every device belongs to player slot 0 | excluded (Z2-D-076) |
| mod loading: no mount loads third-party content | excluded (Z2-D-075); the user mount never provides content |
| XR input (OpenXR device source, poses): PLAT keeps the model XR-ready | deferred to Stage 7, SCOPE (Z2-D-072) |
| console input and storage | deferred to Stage 7, SCOPE (Z2-D-071) |
| remote players' input (network command transport) | NET (Z2-D-073); commands carry a player slot for it |
| the input-binding editor UI and the remapping screen | EDIT and UI (Z2-I-UI-12); PLAT exposes the model, the rebind operation, display data and provenance |
| UI hit-testing, focus and input arbitration logic | UI (Z2-I-UI-17); PLAT provides the claim interface |
| the `.zpak` archive format and reader, `update.zman`, Play Asset Delivery | PACK; PLAT mounts sources and calls the verifier |
| the Ed25519 verification rules and the trust boundaries | SEC; PLAT implements the path-traversal control and hosts the verifier |
| the resume file format, snapshot serialization and restore; the journaled multi-document write | SAVE; PLAT owns the resume policy |
| decompression of stored archive entries and decode back-pressure | ASSET and COMPRESS |
| the memory governor's policy | CORE; PLAT relays the OS low-memory signal |
| the dynamic-resolution controller and the quality tiers' content | LIGHT and RPROD; PLAT drives them from thermal headroom |
| the crash handler | DEVT (D-SEL-CRASH, open, Stage 2); its third-party library, when admitted, is owned by ZenithPlatform |
| the configuration value types and store | CORE (Z2-I-CORE-16); PLAT owns the layers, sources and provenance (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Ownership split with CORE") |
| audio devices and audio focus | AUDIO (Z2-I-AUDIO-12, Z2-I-AUDIO-13); PLAT tells the listener to stop and resume audio |
| the semantic tree's content, narration composition, live regions, the accessibility settings and their keys, the accessible flows | UI (Z2-I-UI-13, Z2-I-UI-18 to Z2-I-UI-20, Z2-I-UI-27); UI implements PLAT's `Zenith_AccessibilityTreeProvider` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Ownership split with UI") |
| the Java class of the Android accessibility bridge and its packaging | MOD (the Player's Gradle template), against PLAT's contract (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract") |

### Inventory items covered

Every item of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1", section "PLAT Platform & input", is covered: Z2-I-PLAT-01 to Z2-I-PLAT-30 (the table "Coverage of inventory items" of `Zenith2/Docs/Requirements/PLAT.md` maps each to its requirements). PLAT also owns requirements for eleven items of other areas where the platform primitive is PLAT's: Z2-I-DEVT-16 (the MP-MEM S24 sampler), Z2-I-SCRIPT-06 and Z2-I-DEVT-18 (the loopback listener), Z2-I-SCRIPT-14 (the file watcher), Z2-I-SEC-09 and Z2-I-SEC-10 (development services absent from Shipping), Z2-I-NET-05 (headless execution: the `--timeout-seconds` watchdog), and Z2-I-UI-13, Z2-I-UI-19, Z2-I-UI-20 and Z2-I-UI-27 (the platform accessibility service under UI's accessibility settings, screen-reader and TTS hooks and screen-reader flow).

### Naming against the core documents

The core documents name the filesystem, lifecycle and thermal types; this family uses those names: `Zenith_VirtualFileSystem` (not "Zenith_FileSystem"), `Zenith_AppLifecycle` (not "Zenith_Lifecycle"), `Zenith_ThermalMonitor` (not "Zenith_Thermal"), `Zenith_Config` with its `Zenith_ConfigSource` chain (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership").

### Document family

| File | Its `##` headings |
|---|---|
| `Zenith2/Docs/Design/PLAT.md` (this file) | Purpose and scope · Dependencies · Library and file layout · Public API · Data structures and formats · Algorithms and state machines · Threading lifetime and error contract · Diagnostics and profiling · Authoring and cooking · Failure handling · Test strategy · Budget rows · Stage plan · Decisions · Requirement index |
| `Zenith2/Docs/Design/PLAT/Platform.md` | Scope of this file · Zenith_Platform lifecycle · The event pump · Windows · Display metrics and DPI · Clipboard · The monotonic clock · Android Java context · Connectivity · Account changes · Development services · Process watchdog · The headless test host · Platform core tests |
| `Zenith2/Docs/Design/PLAT/Input.md` | Scope of this file · Input model overview · Devices and controls · Keyboard and mouse · Controllers · Touch and pen · Gestures · Text input and IME · Haptics · Zenith_InputSystem API · Actions · Bindings · Input contexts · The action mapper · The bindings file format · Rebinding · Input error enumerations · Input tests |
| `Zenith2/Docs/Design/PLAT/SimulationInput.md` | Scope of this file · Simulation input · Script-facing input API |
| `Zenith2/Docs/Design/PLAT/Filesystem.md` | Scope of this file · Virtual paths · Mounts · The mount manifest · Synchronous file access · Command-line host files · Asynchronous I/O · Low-disk detection and behaviour · Filesystem error enumerations · Filesystem tests |
| `Zenith2/Docs/Design/PLAT/Configuration.md` | Scope of this file · Ownership split with CORE · Layers and sources · Key registration · Resolution algorithm · Provenance · Configuration file format · Command-line layer · Runtime changes · Persisting the user layer · Corrupt configuration handling · The configuration dump · Keys PLAT registers · Zenith_Config API · Configuration error enumerations · Configuration tests |
| `Zenith2/Docs/Design/PLAT/Android.md` | Scope of this file · Application lifecycle · Process death and restore · Low memory relay · Thermal adaptation · Android storage and packaging facts · Android and lifecycle tests |
| `Zenith2/Docs/Design/PLAT/Accessibility.md` | Scope of this file · Ownership split with UI · Zenith_Accessibility API · Backend selection · The published snapshot · Screen-reader detection · Windows UI Automation provider · Android node provider · Announcements · Built-in text to speech · Focus speech hold · OS-requested actions · OS accessibility preferences · The recording implementation · Zenith_AccessibilityTestAccess API · Accessibility threading and shutdown · Accessibility diagnostics · Accessibility failure handling · Accessibility tests · Accessibility device runs |

## Dependencies

### Libraries PLAT depends on

`ZenithPlatform` sits at layer 2 of `Zenith2/Docs/Architecture.md` → "Library dependency graph" and declares exactly `ZenithCore` and `ZenithProfiling`. It therefore uses no job system, no reflection and no ECS: its I/O threads are its own `Zenith_Thread`s, its phase-1 system and reflected resources live in the runtime bridge (§ "Runtime bridge files in ZenithRuntime"), and its script bindings are registered by the composition root.

| Uses from | What |
|---|---|
| ZenithCore | `Zenith_Result`, `Zenith_Owned`, `Zenith_Heap`, `Zenith_Arena`, `Zenith_SlotTable`, `Zenith_Span`, `Zenith_StringView`, `Zenith_OperationHandle` and its table, `Zenith_CallbackRegistry` and `Zenith_CallbackToken`, `Zenith_Thread`, `Zenith_Mutex`, `Zenith_Semaphore`, `Zenith_FaultInjector`, `Zenith_Hash64`, SHA-256, `Zenith_JsonWriter` and the JSON reader, `Zenith_ByteWriter`, `Zenith_ConfigSchema` and `Zenith_ConfigStore`, the clocks (`Zenith_Clock.h`), the log, `Zenith_BuildInfo.h` (`Zenith2/Docs/Contracts.md` → "Ownership and allocation", "Threading and concurrency contract", "Errors and results"; `Zenith2/Docs/Design/CORE.md` → "Public API") |
| ZenithProfiling | zones and counters through the installed profiler hooks (`Zenith2/Docs/Contracts.md` → "No mutable global state") |
| SDL3 (third party, owned) | window, input, Android lifecycle and filesystem locations (`Zenith2/Docs/Architecture.md` → "Third-party dependencies") |
| Ed25519 implementation (third party, owned, admitted by the Stage 1 security-architecture ticket) | patch-manifest signature verification (`Zenith2/Docs/Architecture.md` → "Third-party dependencies") |

### Core document sections PLAT relies on

| Section | For |
|---|---|
| `Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership" … "Required tests" | the SPEC baseline of every PLAT section |
| `Zenith2/Docs/Architecture/BuildSystem.md` → "The zenith_add_library helper", "The ZENITH_SUPPORTED matrix", "The manifest dependencies section", "Android packaging", "The linux-tsan host" | the library target, SDL3's per-platform options, the 16 KB and HWASan facts |
| `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure" | prologue steps 1, 3 and 5 |
| `Zenith2/Docs/Architecture/FramePhases.md` → "Phase-specific rules", "Sync points and the tick boundary" | phase 1 and tick-boundary publication |
| `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", "Command-line options", "Module interfaces", "Exit codes" | where PLAT objects are created, the options it serves, the exit codes it causes |
| `Zenith2/Docs/Architecture.md` → "Security architecture", "Memory governor", "Multiplayer foundations", "Determinism guarantee and mechanisms" | parser limits, path traversal, update manifests, the governor relay, the replay log, presentation-only adaptation |
| `Zenith2/Docs/Contracts.md` → "Handle kinds", "Threading and concurrency contract", "Errors and results", "Compatibility policy per kind" | operation handles, locks, queues, cancellation, shutdown, results, user-file compatibility |
| `Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types", "Merge-time capacity and asynchronous results" | the Luau surface and publication rules |
| `Zenith2/Docs/Rendering/GpuResources.md` → "Swapchain recreation" | what the surface handshake and DPI changes feed |

### Areas PLAT depends on

| Area | What PLAT needs | Heading relied on |
|---|---|---|
| CORE | the core types above, the operation table, `Zenith_ConfigStore`, the clock primitive if CORE places it | `Zenith2/Docs/Design/CORE.md` → "Public API" |
| MOD | the runtime bridge's registration points, the input-world mark, `Zenith_CommandLineFileGrant`, startup step placement, the throttled frame loop | `Zenith2/Docs/Design/MOD.md` → "Public API", "Algorithms and state machines" |
| SAVE | the replay reader and writer, the resume save kind, the journaled write | `Zenith2/Docs/Design/SAVE.md` (pending; its "Public API" section) |
| PACK | `update.zman`'s layout; the `Zenith_UpdateStampLookup` over `verified.zinv` handed to the patch mount (`Zenith_RuntimePackage_GetUpdateStampLookup`); PACK alone installs patches and DLC before the virtual filesystem exists and installs, verifies, mounts and unmounts delivered asset packs through PLAT's primitives | `Zenith2/Docs/Design/PACK/Formats.md` → "Update manifest", "Verified stamp file"; `Zenith2/Docs/Design/PACK/Algorithms.md` → "Pending installs at startup", "Verification pipeline", "Mounting and unmounting" |
| DEVT | the console registration API and the development file writer the input-script commands use | `Zenith2/Docs/Design/DEVT.md` → "Area registration"; `Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevFileWriter header" |
| SEC | the update-verification rules | `Zenith2/Docs/Design/SEC.md` (pending; its "Algorithms and state machines" section) |
| ASSET | cooking engine content from `Internal/Content/**`, merging the libraries' `Internal/Config/**` fragments and validating bindings, the NFC rule for content names, registering PLAT's migration steps | `Zenith2/Docs/Design/ASSET/Cook.md` → "Configuration merge"; `Zenith2/Docs/Design/ASSET/Database.md` → "Scan algorithm"; `Zenith2/Docs/Design/ASSET.md` → "Migration hooks" |
| ONLINE | posting account signals, consuming account events | `Zenith2/Docs/Design/ONLINE.md` (pending; its "Public API" section) |
| LIGHT, RPROD | the dynamic-resolution cap and quality tiers the thermal target drives | `Zenith2/Docs/Design/LIGHT.md` (pending; its "Public API" section) |
| UI, EDIT | claimant implementations, remapping and binding-editor surfaces; UI's `Zenith_AccessibilityTreeProvider` and its registration at module start | `Zenith2/Docs/Design/UI.md` → "Public API", "Module start sequence"; `Zenith2/Docs/Design/UI/Accessibility.md` → "Platform accessibility bridge" |
| MOD (Gradle template, connectivity) | the Java class `org.zenith2.player.Zenith2Connectivity` (constructor `(long handle, android.content.Context context)`, `detach()`, the native `nativeConnectivityChanged(long, int)`), its keep rule and `android.permission.ACCESS_NETWORK_STATE` in the template manifest; the `--script-migration-fixtures` grant built with `HOST_DIRECTORY_ACCESS_READ_ONLY` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring" (row P24 of the cross-area notes) |
| MOD (Gradle template) | the Java class `org.zenith2.player.Zenith2Accessibility`, `Zenith2Activity.getAccessibilityHostView()`, the keep rule and the `fontScale` configuration change of the accessibility bridge; the early key `platform.accessibility.backend` copied into `Zenith_PlatformDesc` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Android Gradle wiring" (row P20 of the cross-area notes) |

### Areas that depend on PLAT

RHI (surfaces, native handles, DPI and swapchain triggers), ASSET and STREAM (the virtual filesystem and asynchronous I/O), PACK (the update-manifest verifier and its halves, identity stamps, the directory-source factory, `ReadFromSource`, both content hashes, the base-path override, the Java context, the connectivity signal), AUDIO (the Java context for its focus bridge), SAVE (atomic writes, low disk, the resume policy), SCRIPT (input bindings, the loopback listener, the file watcher), UI (input frame, claims, text input, display metrics, binding display, the accessibility service), EDIT (multiple windows, file drops, the binding table), AUDIO (lifecycle audio stop and resume), ONLINE (account events), NET (commands), DEVT (counters, the dump, the overlay data, the sampler, the evidence-directory reads), MOD (every object here is created by the composition root; the migration-fixture reads; the base-path test hook), GAME and TWOD (input-script recording for their playthrough recordings).

### Upward interfaces PLAT declares

Higher libraries receive notifications only through interfaces PLAT declares and the composition root passes down (`Zenith2/Docs/Contracts.md` → "Layering DAG", "Callback direction"):

| Interface | Implemented by | Called |
|---|---|---|
| `Zenith_AppLifecycleListener` | ZenithRuntime | inside `Platform.Pump` on lifecycle transitions |
| `Zenith_ThermalAdaptationTarget` | ZenithRuntime (to the resolution controller and `Zenith_Config`) | prologue step 5 |
| `Zenith_InputClaimant` | ZenithUI, ZenithEditor | inside `Platform.Pump` |
| `Zenith_AccessibilityTreeProvider` | ZenithUI | on the main thread, only inside `SetTreeProvider` and `NotifyTreeChanged` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityTreeProvider") |
| `Zenith_MountSource` | ZenithAssets' archive reader (ASSET), ZenithPackage's container and install-tree sources (PACK) | by resolution, the I/O threads (`Read`, `ReadFromSource`) and the update-manifest verifier |
| `Zenith_UpdateStampLookup` | ZenithPackage's `Zenith_VerifiedStampFile` (PACK) | by `Zenith_VerifyUpdateManifestFiles`, inside `Zenith_VirtualFileSystem::Create` and PACK's own calls |
| `Zenith_ConfigSource` | tests (memory sources) | by `Zenith_Config::Create` |
| `Zenith_FreeSpaceSource`, `Zenith_ThermalProvider`, `Zenith_PlatformBackendHooks` | tests | by the I/O threads and the pump |

### Assumed interfaces pending other areas

Each is marked `<!-- assumption: pending <AREA> -->` at its point of use and recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`:

| Area | Assumed | Where |
|---|---|---|
| MOD | a development/Shipping source partition in `zenith_add_library` (optional) | `Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion" |
| MOD | the `ClearCarriedTransitions()` call after a `RunFrame` in which the input world ran a tick (MOD's step-19 row names only `CarryFrameTransitions`) | § "The Platform Pump step" |
| SCRIPT | the consumer note for `PUBLICATION_KIND_PLATFORM_RECORDED` in `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" | § "Frame-graph placement" |
| CONTRACT | `COUNTER_KIND_GAUGE` in `Zenith_CounterKind` (X83) | § "Counters" |
| CONTRACT | the "Shutdown ownership order" row for the platform object after step 10 (MOD's clarification of step 8), which the watchdog ticket extends with `Zenith.Watchdog` (X93) | § "Threads PLAT owns and shutdown" |
| CONTRACT | the lock row 640 `uLOCK_LEVEL_ACCESSIBILITY` and `Zenith.Speech` in the platform's shutdown row (X270) | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The accessibility lock"; § "Lock-order entries" |
| ARCH | the OS accessibility sub-source of configuration layer 2 and its prologue re-application; PLAT's two accessibility headers in the header table; the Gradle template's accessibility class, host-view accessor and `fontScale` configuration change (X269) | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer", "Java bridge contract" |
| MOD | the Java bridge class, the host-view accessor, the keep rule and the `fontScale` configuration change in the Gradle template; the early-key copy into `m_eAccessibilityBackend`; `Zenith_Bench_AccessibilityBridge` in the benchmark-APK list (row P20) | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract", "Accessibility device runs" |
| UI | UI's provider reads PLAT's snapshot contract (called on the main thread only), reads `platform.accessibility.*` for the OS defaults of its settings, and handles the service's results (row P21) | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Ownership split with UI" |
| MOD | the Java class `org.zenith2.player.Zenith2Connectivity`, its keep rule and `android.permission.ACCESS_NETWORK_STATE` in the Gradle template (row P24); `Zenith_CommandLine` building the `--script-migration-fixtures` grant with `HOST_DIRECTORY_ACCESS_READ_ONLY` | `Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files" |
| ARCH | the header rows of `Zenith_UpdateManifest.h` and `Zenith_FileIdentity.h` and the new `Zenith_HostFile.h` functions in `Zenith2/Docs/Architecture/PlatformLayer.md` → "Settled rules and ownership"; the connectivity class and permission in `Zenith2/Docs/Architecture/CompositionRoot.md` → "The Gradle template" (X427) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API", "File identity stamps"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity" |
| ARCH | the user mount's top-level directory list in `Zenith2/Docs/Architecture/PlatformLayer.md` → "Mounts and precedence" gains `crash/` (every build) and `devtools/` (non-Shipping builds), which rule R10 accepts by SEC's ruling (row SEC1) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order" |
| SEC | the Stage 1 security-architecture ticket confirms the identity-stamp set of "File identity stamps" and the unauthenticated-record premise ([Z2-N-ARCH-966]); the stamp skip ships only after that confirmation | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "File identity stamps", "Patch mount verification" |

Landed since round 1 and now cited, not assumed: CORE's `GetLayerEntry`, `ClearLayer` and `ApplySimulationAffecting` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"); the registration order at the start of startup step 6 (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence"); `ASYNC_SOURCE_PLATFORM` and `PUBLICATION_KIND_PLATFORM_RECORDED` ([Z2-N-ARCH-791]); the [Z2-N-ARCH-923] exception for the four command-line files; PLAT's enumerators in Architecture's code blocks; the lock rows 610, 620 and 630 (`Zenith2/Docs/Contracts.md` → "Lock ordering table"); the two shutdown flushes (`Zenith2/Docs/Design/MOD.md` → "Shutdown sequence" step 6); the cook's configuration merge and bindings validation (`Zenith2/Docs/Design/ASSET/Cook.md` → "Configuration merge"), the NFC rule (`Zenith2/Docs/Design/ASSET/Database.md` → "Scan algorithm") and the registration of PLAT's migration steps (`Zenith2/Docs/Design/ASSET.md` → "Migration hooks"). ASSET names the cooked tier list `Config/tiers.<platform>.json` where PLAT reads `content:/Engine/Config/Quality/tiers.<platform>.json`, and the engine bindings `content:/Engine/Bindings/` where PLAT reads `content:/Engine/Input/Engine.bindings.json`; PLAT keeps its paths and ASSET conforms (row P19 of the cross-area notes).

## Library and file layout

### ZenithPlatform target

```cmake
# Zenith2/Engine/ZenithPlatform/CMakeLists.txt
zenith_add_library(ZenithPlatform DEPENDS ZenithCore ZenithProfiling THIRD_PARTY SDL3)
```

The Ed25519 dependency joins `THIRD_PARTY` in the ticket that implements the Stage 1 security ruling; the crash handler's library (D-SEL-CRASH, open, Stage 2) joins it in the ticket implementing that ruling.

### Public root headers

| Header (`Zenith2/Engine/ZenithPlatform/`) | Declares | Specified in |
|---|---|---|
| `Zenith_Platform.h` | `Zenith_Platform`, `Zenith_PlatformDesc`, `Zenith_PlatformMode`, `Zenith_PlatformEvent` and its kinds and payloads, `Zenith_PlatformError`, `Zenith_Clipboard`, `Zenith_AccountSignal` and its kinds and sources, `Zenith_PlatformBackendHooks`, `Zenith_TestPlatformEvent`, `Zenith_AndroidJavaContext`, `Zenith_ConnectivityState` | `Zenith2/Docs/Design/PLAT/Platform.md` |
| `Zenith_Window.h` | `Zenith_WindowID`, `Zenith_WindowDesc`, `Zenith_WindowMode`, `Zenith_Window`, `Zenith_NativeWindowHandle`, `Zenith_NativeWindowKind`, `Zenith_DisplayMetrics`, `Zenith_DisplayInfo`, `Zenith_DisplayList`, `Zenith_SafeAreaInsets`, `Zenith_GetVulkanInstanceProcAddr`, `Zenith_GetVulkanInstanceExtensions` | `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API" |
| `Zenith_InputTypes.h` | `Zenith_InputDeviceKind`, `Zenith_InputDeviceID`, `Zenith_InputControl`, the key, mouse, gamepad, touch, pen and gesture control enumerations, the device constants, `Zenith_InputError` | `Zenith2/Docs/Design/PLAT/Input.md` → "Devices and controls", "Input error enumerations" |
| `Zenith_InputDevices.h` | `Zenith_InputDevices` (the architecture's `Zenith_InputDevices.h`), `Zenith_InputDeviceInfo`, `Zenith_GamepadType`, `Zenith_InputTransition` and its flags, the virtual-device descs, `uINPUT_DEVICE_FLAG_*`; text input and IME (`Zenith_TextInput`, `Zenith_TextInputDesc`, `Zenith_TextInputPurpose`, `Zenith_TextInputEvent`, `Zenith_TextInputSessionID`); haptics (`Zenith_HapticPlayer`, `Zenith_HapticEffect`, `Zenith_HapticEffectKind`, `Zenith_HapticChannel`, `Zenith_HapticTarget`, `Zenith_HapticPlayback`) | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API", "Text input and IME", "Haptics" |
| `Zenith_Gestures.h` | `Zenith_GestureKind`, `Zenith_GestureState`, `Zenith_GestureConfig`, `Zenith_GestureEvent`, `Zenith_SwipeDirection`, `Zenith_GestureRecogniserSet` | `Zenith2/Docs/Design/PLAT/Input.md` → "Gestures" |
| `Zenith_InputAction.h` | `Zenith_InputActionType`, `Zenith_InputActionID`, `Zenith_InputValue`, `Zenith_InputPhaseFlag`, `Zenith_InputActionState`, `Zenith_InputActionStateTable`, `Zenith_InputActionDesc`, `Zenith_InputActionSet` | `Zenith2/Docs/Design/PLAT/Input.md` → "Actions" |
| `Zenith_InputBindings.h` | `Zenith_BindingLayer`, `Zenith_ControlScheme`, composites, processors, interactions, `Zenith_ControlPath`, `Zenith_ParseControlPath`, `Zenith_FormatControlPath`, `Zenith_InputBinding`, `Zenith_InputContextDesc`, `Zenith_BindingSourceFile`, `Zenith_InputBindingTable`, `Zenith_GlyphFamily`, `Zenith_BindingDisplay`, `Zenith_BindingProvenanceEntry`, `Zenith_ValidateBindingsJson`, `Zenith_WriteBindingsJson` | `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings", "The bindings file format" |
| `Zenith_InputContext.h` | `Zenith_InputContextID`, `Zenith_InputContextScope`, `Zenith_InputContextEntry`, `Zenith_InputContextStack` | `Zenith2/Docs/Design/PLAT/Input.md` → "Input contexts" |
| `Zenith_ActionMapper.h` | `Zenith_ActionMapper`, `Zenith_ActionMapperInputs`, `Zenith_ActionChange` | `Zenith2/Docs/Design/PLAT/Input.md` → "The action mapper" |
| `Zenith_InputSystem.h` | `Zenith_InputSystem`, `Zenith_InputFrame`, `Zenith_InputClaimant`, `Zenith_InputClaimSet`, `Zenith_PointerDown`, the rebind types, `Zenith_InputDebugSnapshot`, `Zenith_RebindResetScope` | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputSystem API", "Presentation claims", "Rebinding" |
| `Zenith_InputCommand.h` | `Zenith_InputCommand`, `Zenith_EncodeInputValue`, `Zenith_DecodeInputValue`, `Zenith_InputCommandEncoder`, `Zenith_ApplyInputCommands` | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| `Zenith_AppLifecycle.h` | `Zenith_AppLifecycle`, `Zenith_AppLifecycleState`, `Zenith_AppLifecycleEvent`, `Zenith_AppLifecycleListener`, the resume types, `Zenith_LifecycleError` | `Zenith2/Docs/Design/PLAT/Android.md` → "Application lifecycle", "Process death and restore" |
| `Zenith_ThermalMonitor.h` | `Zenith_ThermalMonitor`, `Zenith_ThermalSample`, `Zenith_ThermalStatus`, `Zenith_ThermalProvider`, `Zenith_ThermalAdaptationTarget`, `Zenith_ThermalConfig` | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation" |
| `Zenith_VirtualFileSystem.h` | `Zenith_VirtualPath`, `Zenith_ParseVirtualPath`, `Zenith_PathRoot`, `Zenith_PathErrorCode`, `Zenith_PathError`, `Zenith_MountSourceID`, `Zenith_MountInfo`, `Zenith_VirtualFileSystem`, `Zenith_VirtualFileSystemDesc`, `Zenith_BuildIdentity`, `Zenith_MountKind`, `Zenith_MountSource`, `Zenith_SourceEntry`, `Zenith_SourceFile`, `Zenith_ResolvedPath`, `Zenith_FileStat`, `Zenith_DirectoryEntry`, `Zenith_FileError`, `Zenith_DirectorySourceDesc`, `Zenith_CreateDirectoryMountSource` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Virtual paths", "Mounts" |
| `Zenith_FileIdentity.h` | `Zenith_FileIdentityStamp`, `Zenith_FileVolumeKind`, `ulFILE_IDENTITY_RACY_WINDOW_NS`, `Zenith_GetCurrentFileTimeNs`, `Zenith_IsStampSettled` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "File identity stamps" |
| `Zenith_UpdateManifest.h` | `Zenith_UpdateManifestKind`, `Zenith_UpdateVerifyError`, `Zenith_UpdateManifestFileEntry`, `Zenith_UpdateManifestInfo`, `Zenith_UpdateStampLookup`, `Zenith_UpdateFileCheckReport`, `Zenith_DecodeUpdateManifest`, `Zenith_CheckUpdateManifestTarget`, `Zenith_VerifyUpdateManifestFiles`, `Zenith_VerifyUpdateManifest` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API" |
| `Zenith_AsyncIO.h` | `Zenith_AsyncIO`, `Zenith_AsyncIODesc`, `Zenith_IORequest`, `Zenith_IOPriority`, `Zenith_IOCompletion`, `Zenith_IOCompletionQueue`, `Zenith_FreeSpaceSource`, `Zenith_LowDiskState`, `Zenith_LowDiskReason`, `Zenith_IOStats`, `Zenith_IOErrorCode`, `Zenith_IOError` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Asynchronous I/O", "Low-disk detection and behaviour" |
| `Zenith_HostFile.h` | `Zenith_OpenCommandLineHostFile`, `Zenith_CreateFileInCommandLineHostDirectory`, `Zenith_OpenFileInCommandLineHostDirectory`, `Zenith_ListCommandLineHostDirectory`, `Zenith_HostFile`, `Zenith_HostFileMode`, `Zenith_HostDirectoryAccess`, `Zenith_HostDirectoryEntry`, the `HOST_DIRECTORY_*` limits, `Zenith_CommandLineFileGrant` (PLAT declares it; only ZenithRuntime's `Zenith_CommandLine` constructs it) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files", "Directory-grant reads" |
| `Zenith_Config.h` (includes ZenithCore's `Zenith_ConfigStore.h`) | `Zenith_Config`, `Zenith_ConfigDesc`, `Zenith_ConfigSource`, `Zenith_ConfigProvenanceEntry`, `Zenith_ConfigResetReason`, `Zenith_ConfigSourceError`, `Zenith_RegisterPlatformConfigKeys` | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Zenith_Config API" |
| `Zenith_DevSocket.h` | `Zenith_DevListener`, `Zenith_DevListenerDesc`, `Zenith_DevConnection`, `Zenith_DevPollResult`, `Zenith_DevClient`, `Zenith_DevServiceError` | `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket" |
| `Zenith_FileWatcher.h` | `Zenith_FileWatcher`, `Zenith_FileChange`, `Zenith_FileChangeKind` | `Zenith2/Docs/Design/PLAT/Platform.md` → "File watcher" |
| `Zenith_Accessibility.h` | `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider`, `Zenith_AccessibilityBackend`, `Zenith_AccessibilityRole`, `Zenith_AccessibilityStateFlags`, `Zenith_AccessibilityActionKind`, `Zenith_AccessibilityRect`, `Zenith_AccessibilityText`, `Zenith_AccessibilityNode`, `Zenith_AccessibilityTreeView`, `Zenith_AccessibilityAction`, `Zenith_AccessibilityPreferences`, `Zenith_AccessibilityState`, `Zenith_AccessibilityError`, the `uACCESSIBILITY_*` constants | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_Accessibility API" |
| `Zenith_AccessibilityTestAccess.h` (test access, compiled into every configuration) | `Zenith_AccessibilityTestAccess`, `Zenith_AccessibilityRecord`, `Zenith_AccessibilityRecordKind`, the `uACCESSIBILITY_FIELD_*` bits | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Record log", "Zenith_AccessibilityTestAccess API" |

### Internal shared files

<!-- elaboration -->
Every file below is under `Zenith2/Engine/ZenithPlatform/Internal/`, compiled on every platform, and may include SDL3 headers (third-party confinement allows `Internal/`) but no platform SDK header (rule F18); per-platform system calls go through the small function tables declared in `Internal/Zenith_HostFileOps.h`, `Internal/Zenith_HostSystem.h` and implemented once per platform directory.

| File | Contents |
|---|---|
| `Zenith_PlatformLocks.h` | the `uLOCK_LEVEL_*` constants of § "Lock-order entries" |
| `Zenith_Platform.cpp`, `Zenith_PlatformPump.cpp`, `Zenith_SdlEventTranslation.cpp` | creation steps, the pump algorithm, the translation table |
| `Zenith_Window.cpp`, `Zenith_Display.cpp`, `Zenith_Clipboard.cpp` | windows, metrics, clipboard |
| `Zenith_AccountSignals.cpp` | the signal queue and delivery |
| `Zenith_InputDevices.cpp`, `Zenith_InputSdlMapping.cpp`, `Zenith_TransitionLog.cpp`, `Zenith_Gamepads.cpp`, `Zenith_Touch.cpp`, `Zenith_VirtualDevices.cpp` | devices, the SDL mapping with its `static_assert`s, the log |
| `Zenith_ControlPath.cpp` | path parsing and formatting |
| `Zenith_Gestures.cpp` | the recognisers |
| `Zenith_TextInput.cpp`, `Zenith_Haptics.cpp` | IME and haptics |
| `Zenith_InputActionSet.cpp`, `Zenith_InputActionStateTable.cpp` | actions |
| `Zenith_BindingsLoader.cpp`, `Zenith_BindingsValidate.cpp`, `Zenith_BindingsWriter.cpp`, `Zenith_InputBindingTable.cpp` | the bindings file and layers |
| `Zenith_InputContextStack.cpp`, `Zenith_ActionMapper.cpp`, `Zenith_Interactions.cpp`, `Zenith_Processors.cpp`, `Zenith_Composites.cpp` | contexts and the mapper |
| `Zenith_InputSystem.cpp`, `Zenith_InputClaims.cpp`, `Zenith_Rebind.cpp`, `Zenith_UserBindings.cpp` | the system object, claims, rebinding, the user file |
| `Zenith_InputCommand.cpp`, `Zenith_InputScript.cpp` | commands and scripted input |
| `Zenith_AppLifecycle.cpp`, `Zenith_ResumePolicy.cpp` | the state machine and the resume policy |
| `Zenith_ThermalMonitor.cpp`, `Zenith_ThermalProvider_Null.cpp` | thermal adaptation and the null provider |
| `Zenith_VirtualPath.cpp`, `Zenith_VirtualFileSystem.cpp`, `Zenith_DirectoryMountSource.cpp`, `Zenith_MountManifest.cpp`, `Zenith_PatchMount.cpp`, `Zenith_HostFile.cpp` | the filesystem |
| `AsyncIO/Zenith_AsyncIO.cpp`, `AsyncIO/Zenith_IOScheduler.cpp`, `AsyncIO/Zenith_AtomicWrite.cpp`, `AsyncIO/Zenith_LowDisk.cpp`, `AsyncIO/Zenith_IOCompletionQueue.cpp` | asynchronous I/O |
| `Zenith_Config.cpp`, `Zenith_ConfigSources.cpp`, `Zenith_ConfigResolve.cpp`, `Zenith_ConfigUserFile.cpp`, `Zenith_ConfigDump.cpp`, `Zenith_ConfigKeys.cpp` | configuration and PLAT's key table |
| `Security/Zenith_UpdateManifestVerifier.cpp` | the four public functions of `Zenith_UpdateManifest.h` (SEC's rules); U3 calls SEC's `Zenith_Ed25519Verify` |
| `Security/Zenith_Ed25519.h`, `Security/Zenith_Ed25519.cpp` (SEC's files in ZenithPlatform) | `Zenith_Ed25519Verify`, the only includer of the admitted Ed25519 library (`Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Zenith_Ed25519 header") |
| `Zenith_FileIdentity.cpp` | the directory source's stamp and volume gate, over `Zenith_HostFileOps.h` |
| `Zenith_InputScriptRecorder.h`, `Zenith_InputScriptRecorder.cpp` | input-script recording and the version 2 writer |
| `DevTools/Zenith_PlatformDevToolsArea.h`, `DevTools/Zenith_PlatformDevToolsArea.cpp` | `Zenith_RegisterPlatformDevToolsArea`, `Zenith_PlatformDevToolsContext` and the `input.recordScript` and `input.stopRecordScript` handlers |
| `Zenith_Connectivity.h` | the connectivity cell and the per-platform source table |
| `DevServices/Zenith_DevSocket.cpp`, `DevServices/Zenith_FileWatcher.cpp` | development services |
| `Accessibility/Zenith_AccessibilityService.cpp`, `Accessibility/Zenith_AccessibilitySnapshot.h`, `Accessibility/Zenith_AccessibilitySnapshot.cpp`, `Accessibility/Zenith_AccessibilityRecording.cpp`, `Accessibility/Zenith_AccessibilityTestAccess.cpp`, `Accessibility/Zenith_AccessibilityVirtualTree.h`, `Accessibility/Zenith_AccessibilityVirtualTree.cpp`, `Accessibility/Zenith_AccessibilityPreferences.h`, `Accessibility/Zenith_AccessibilityPreferences.cpp`, `Accessibility/Zenith_AccessibilityJniTable.h`, `Accessibility/Zenith_SpeechQueue.h`, `Accessibility/Zenith_SpeechQueue.cpp`, `Accessibility/Zenith_AccessibilityBackend.h`, `Accessibility/Zenith_AccessibilityOsProbe.h` | the accessibility service, the snapshot and its readers, the recording backend, the test-access functions, the Android virtual tree, preference normalisation, the JNI string table, the speech queue, the internal backend table, the OS test probe (each platform defines the probe; Linux and Android return not available) (`Zenith2/Docs/Design/PLAT/Accessibility.md`) |
| `Content/Input/Engine.bindings.json` | the engine bindings (data, no compiled source) |
| `Config/Platform/windows.config.json`, `android.config.json` | ZenithPlatform's fragment of the platform layer (merged by the cook) |
| `Config/Quality/tiers.windows.json`, `tiers.android.json`, `<TierID>.config.json` | the tier lists and ZenithPlatform's fragment of the engine tier layer |

### Internal Windows files

`Zenith2/Engine/ZenithPlatform/Internal/Windows/`: `Zenith_HostFile_Windows.cpp` (`CreateFileW`, `ReadFile`, `WriteFile`, `FlushFileBuffers`, `MoveFileExW`), `Zenith_EscapeCheck_Windows.cpp` (`GetFinalPathNameByHandleW`, the case policy), `Zenith_FreeSpace_Windows.cpp` (`GetDiskFreeSpaceExW`), `Zenith_AccountSignals_Windows.cpp` (WTS notifications through the SDL message hook), `Zenith_DevSocket_Windows.cpp` (Winsock), `Zenith_FileWatcher_Windows.cpp` (`ReadDirectoryChangesW`), `Zenith_OSRandom_Windows.cpp` (`BCryptGenRandom` for the session token), `Zenith_HostSystem_Windows.cpp`, `Zenith_Accessibility_Windows.cpp` (screen-reader detection, preferences, the window subclass, announcements), `Zenith_UiaProviders_Windows.cpp` (the UI Automation providers), `Zenith_Speech_Windows.cpp` (SAPI on `Zenith.Speech`), `Zenith_Connectivity_Windows.cpp` (`NotifyNetworkConnectivityHintChange`, resolved at run time).

### Internal Android files

`Zenith2/Engine/ZenithPlatform/Internal/Android/`: `Zenith_AndroidLifecycle.cpp` (the event watch and handshake), `Zenith_AndroidPaths.cpp` (JNI: the activity, `AAssetManager`, storage paths), `Zenith_ApkAssetMountSource.cpp`, `Zenith_HostFile_Android.cpp`, `Zenith_EscapeCheck_Android.cpp` (`/proc/self/fd`), `Zenith_FreeSpace_Android.cpp` (`statvfs`), `Zenith_ThermalProvider_Android.cpp` (`AThermal_*`), `Zenith_DevSocket_Android.cpp`, `Zenith_FileWatcher_Android.cpp` (`inotify`), `Zenith_OSRandom_Android.cpp` (`getrandom`), `Zenith_HostSystem_Android.cpp`, `Zenith_Accessibility_Android.cpp` (JNI: the accessibility bridge's natives and calls), `Zenith_AndroidJavaContext.cpp` (the JavaVM, activity and class-loader references), `Zenith_Connectivity_Android.cpp` (JNI: constructs `org.zenith2.player.Zenith2Connectivity`, the native `nativeConnectivityChanged`, `detach`).

### Internal Linux files

`Zenith2/Engine/ZenithPlatform/Internal/Linux/`: `Zenith_LinuxHost.cpp` (forces headless mode, the loose and temporary user roots), `Zenith_HostFile_Linux.cpp`, `Zenith_EscapeCheck_Linux.cpp`, `Zenith_FreeSpace_Linux.cpp`, `Zenith_DevSocket_Linux.cpp`, `Zenith_FileWatcher_Linux.cpp`, `Zenith_OSRandom_Linux.cpp`, `Zenith_HostSystem_Linux.cpp`, `Zenith_Accessibility_Linux.cpp` (no OS accessibility backend: the host is always headless; the OS probe reports not available), `Zenith_Connectivity_Linux.cpp` (no source: UNKNOWN). Android and Linux each carry their own small POSIX files, because a source is selected by exactly one platform directory (`Zenith2/Docs/Architecture/BuildSystem.md` → "Directory rules and globbing").

### Tests files

`Zenith2/Engine/ZenithPlatform/Tests/` (built into `ZenithPlatform_Tests`): `Zenith_Platform_Tests.cpp`, `Zenith_PlatformPump_Tests.cpp`, `Zenith_Window_Tests.cpp`, `Zenith_Display_Tests.cpp`, `Zenith_Clipboard_Tests.cpp`, `Zenith_AccountSignal_Tests.cpp`, `Zenith_DevSocket_Tests.cpp`, `Zenith_FileWatcher_Tests.cpp`, `Zenith_ControlPath_Tests.cpp`, `Zenith_InputDevices_Tests.cpp`, `Zenith_Gamepad_Tests.cpp`, `Zenith_Touch_Tests.cpp`, `Zenith_Gestures_Tests.cpp`, `Zenith_TextInput_Tests.cpp`, `Zenith_Haptics_Tests.cpp`, `Zenith_InputBindings_Tests.cpp`, `Zenith_Interactions_Tests.cpp`, `Zenith_InputContext_Tests.cpp`, `Zenith_InputSystem_Tests.cpp`, `Zenith_ActionMapper_Tests.cpp`, `Zenith_BindingsFile_Tests.cpp`, `Zenith_Rebind_Tests.cpp`, `Zenith_UserBindings_Tests.cpp`, `Zenith_InputCommand_Tests.cpp`, `Zenith_InputScript_Tests.cpp`, `Zenith_VirtualPath_Tests.cpp`, `Zenith_VirtualFileSystem_Tests.cpp`, `Zenith_VirtualFileSystem_DefectTests.cpp`, `Zenith_MountManifest_Tests.cpp`, `Zenith_PatchMount_Tests.cpp`, `Zenith_UpdateManifest_Tests.cpp`, `Zenith_HostFileGrant_Tests.cpp`, `Zenith_AsyncIO_Tests.cpp`, `Zenith_Config_Tests.cpp`, `Zenith_AppLifecycle_Tests.cpp`, `Zenith_ResumeState_Tests.cpp`, `Zenith_Thermal_Tests.cpp`, `Zenith_PlatformCounters_Tests.cpp`, `Zenith_Accessibility_Tests.cpp`, `Zenith_AccessibilityOs_Tests.cpp`; `Tests/Fuzz/VirtualPath.cpp`, `BindingsJson.cpp`, `InputScript.cpp`, `InputCommandDecode.cpp`, `MountManifest.cpp`, `UpdateManifest.cpp`; `Tests/Fuzz/Corpus/<Stem>/` seed corpora; `Tests/Fixtures/` (bindings, configuration, mount manifests, input scripts, touch recordings, migration fixtures; update manifests are generated at test time by `Tests/Support/Zenith_UpdateManifestTestWriter.h`, never committed); `Tests/Examples/Zenith_VirtualFileSystem_Example.cpp`, `Zenith_InputSystem_Example.cpp`, `Zenith_Config_Example.cpp`.

Integration suites (`zenith_add_integration_tests`, linking ZenithRuntime and therefore Windows presets only, `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"): `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp`, `Zenith2/Tests/Lifecycle/Zenith_ResumeIntegration_Tests.cpp`, `Zenith2/Tests/PlatformEvents/Zenith_PlatformEventsIntegration_Tests.cpp`. Two PLAT tests need a library above ZenithPlatform and so cannot live in `ZenithPlatform_Tests` (`Zenith2/Docs/Contracts.md` → "Layering DAG"); both are built into `ZenithRuntime_Tests`: `Test_AppLifecycle_PresentAfterTimeoutIsOutOfDate` needs the RHI (ZenithRHI depends on ZenithPlatform) and is in `Zenith2/Engine/ZenithRuntime/Tests/Zenith_LifecyclePresent_Tests.cpp`; `Test_CommandLineHostFile_GrantOnly` needs a grant, which only ZenithRuntime's `Zenith_CommandLine` constructs, and is in `Zenith2/Engine/ZenithRuntime/Tests/Zenith_CommandLineHostFile_Tests.cpp`.

### Runtime bridge files in ZenithRuntime

<!-- elaboration -->
ZenithPlatform cannot register systems, reflected types or script bindings (it is below ZenithECS and ZenithReflect), so the composition root carries the bridge. These files are specified here and implemented by PLAT tickets, with MOD named in their dependencies (the precedent is `ZenithRuntime/Internal/ScriptBindings/`, `Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types"):

| File (`Zenith2/Engine/ZenithRuntime/Internal/`) | Contents |
|---|---|
| `PlatformBridge/Zenith_InputPhaseSystem.cpp` | the `Input.Actions` phase-1 system of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system" |
| `PlatformBridge/Zenith_InputResources.h` | the five world resources (`Zenith_FrameInputResource`, `Zenith_InputActionStateResource`, `Zenith_SimulationInputContexts`, `Zenith_InputTickSummary`, `Zenith_HapticRequestQueue`), declared in `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system" |
| `PlatformBridge/Zenith_InputResources.cpp` | the reflected world resources and their `TYPE_KIND_RESOURCE` registration |
| `PlatformBridge/Zenith_InputCommandKinds.cpp` | the `Input.PushContext`, `Input.PopContext`, `Input.PlayHaptic` and `Input.StopHaptic` command kinds and playback functions |
| `PlatformBridge/Zenith_HapticsStage.cpp` | the `Input.Haptics` epilogue stage |
| `PlatformBridge/Zenith_PlatformEventPublisher.cpp` | world-visible platform events as tick-boundary publications and phase-9 events (§ "Frame-graph placement") |
| `PlatformBridge/Zenith_PlatformEventTypes.cpp` | the `Platform.*` event types |
| `ScriptBindings/Zenith_InputScriptBindings.cpp` | the `Input` Luau namespace |

The runtime's implementations of `Zenith_AppLifecycleListener` and `Zenith_ThermalAdaptationTarget` are composition work of MOD against PLAT's interfaces.

### Benchmarks and content files

| Path | Contents |
|---|---|
| `Zenith2/Benchmarks/Budgets/PLAT/` | the Stage 1 per-system budget harness for PLAT (§ "The Stage 1 PLAT harness") |
| `Zenith2/Benchmarks/Platform/` | the I/O latency and throughput harness and the pump and mapper micro-benchmarks for the pending rows of § "Pending new rows" |
| `Zenith2/Benchmarks/AccessibilityBridge/` | `Zenith_Bench_AccessibilityBridge`: publication and query costs, and the interactive demo tree of the accessibility device runs (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility device runs") |
| `Games/<Name>/Assets/Input/*.bindings.json`, `Games/<Name>/Assets/Input/Platform/<windows\|android>/*.bindings.json` | a game's bindings (cooked to `content:/Input/…`) |
| `Games/<Name>/Assets/Config/game.config.json`, `Games/<Name>/Assets/Config/Platform/<p>.config.json`, `Games/<Name>/Assets/Config/Quality/<TierID>.config.json` | a game's configuration layers |

### ZENITH_SUPPORTED matrix and SDL3 configuration

| Preset | ZenithPlatform | SDL3 build | Platform directory compiled |
|---|---|---|---|
| `win`, `win-asan` | built | video, events, gamepad, haptic, sensor, audio | `Internal/Windows/` |
| `android-arm64`, `android-x86_64` | built | as Windows, with the Android backend | `Internal/Android/` |
| `linux-tsan` | built (headless) | the manifest's `linux` options: video, audio, render, GPU, camera, haptic, joystick and every window-system backend off (`Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section") | `Internal/Linux/` |

`SUPPORTED` is the default `ZENITH_ALL` (`Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix"); in sanitizer presets SDL3 is built from source with the sanitizer's flags. No Zenith2 file outside `ZenithPlatform/Internal/` includes an SDL3 header, and the `Libgraph` step's third-party owner rule proves it.

## Public API

### API map

| Group | Types | Handle kinds used |
|---|---|---|
| platform core | `Zenith_Platform`, `Zenith_PlatformEvent`, `Zenith_Window`, `Zenith_Clipboard`, the clock, account signals | `Zenith_WindowID` (a generational slot ID, not a Contracts handle kind: a platform-scoped identity) |
| input | `Zenith_InputDevices`, `Zenith_GestureRecogniserSet`, `Zenith_TextInput`, `Zenith_HapticPlayer`, `Zenith_InputSystem`, `Zenith_ActionMapper`, `Zenith_InputContextStack`, `Zenith_InputActionStateTable`, `Zenith_InputBindingTable`, `Zenith_InputCommand` | `Zenith_InputDeviceID` (platform-scoped), `Zenith_InputActionID` and `Zenith_InputContextID` (persistent identities from bindings files) |
| filesystem | `Zenith_VirtualPath`, `Zenith_VirtualFileSystem`, `Zenith_MountSource`, `Zenith_ResolvedPath`, `Zenith_AsyncIO`, `Zenith_IOCompletionQueue`, `Zenith_HostFile` | `Zenith_OperationHandle` for every asynchronous operation (`Zenith2/Docs/Contracts.md` → "Handle kinds") |
| configuration | `Zenith_Config`, `Zenith_ConfigSource`, `Zenith_ConfigProvenanceEntry` over ZenithCore's store types | `Zenith_ConfigKey` (CORE, an index into the frozen schema) |
| lifecycle and thermal | `Zenith_AppLifecycle`, `Zenith_AppLifecycleListener`, `Zenith_ThermalMonitor`, `Zenith_ThermalProvider`, `Zenith_ThermalAdaptationTarget` | `Zenith_OperationHandle` for the resume write |
| development | `Zenith_DevListener`, `Zenith_FileWatcher` | — |
| accessibility | `Zenith_AccessibilityService`, `Zenith_AccessibilityTreeProvider`, the PLAT-owned node, tree, action and preference values | node IDs (`uint64`, the provider's stable identities) |

Every fallible public function returns `Zenith_Result<T, E>` with a PLAT error enumeration (§ "Error enumerations"); every public type carries its contract block (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"), shown in the split files. PLAT's services are concrete classes, not replaceable interfaces: there is one platform implementation per OS, selected by directory (`Zenith2/Docs/Contracts.md` → "API boundary", "Interfaces and factories"); the interfaces PLAT declares are the upward and test seams of § "Upward interfaces PLAT declares".

### Zenith_Platform

The platform object is the aggregate the composition root creates in startup step 4 and passes by reference; its accessors are typed members, not service lookup. Its declaration, creation steps and destructor order are `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc".

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Platform.h (the error enumeration; the class is in the Platform file)
enum Zenith_PlatformError : uint8
{
	PLATFORM_ERROR_BAD_DESC = 1,         // a desc field outside its rule
	PLATFORM_ERROR_SDL_INIT,             // SDL_Init failed (the SDL_GetError text is logged); startup exit code 4
	PLATFORM_ERROR_PATHS,                // SDL_GetBasePath or SDL_GetPrefPath failed; startup exit code 4
	PLATFORM_ERROR_WINDOW_CREATE,        // SDL_CreateWindow failed; startup exit code 4 for the main window
	PLATFORM_ERROR_TOO_MANY_WINDOWS,
	PLATFORM_ERROR_UNKNOWN_WINDOW,       // a stale or foreign Zenith_WindowID
	PLATFORM_ERROR_MAIN_WINDOW,          // DestroyWindow of the main window
	PLATFORM_ERROR_NOT_AVAILABLE,        // a windowed-only member in headless mode
	PLATFORM_ERROR_VULKAN_SURFACE,       // SDL_Vulkan_CreateSurface or the loader query failed
	PLATFORM_ERROR_CLIPBOARD,
	PLATFORM_ERROR_BAD_TEXT,             // text that is not valid UTF-8
	PLATFORM_ERROR_BAD_SIGNAL,           // an account signal with an invalid kind or source
	PLATFORM_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_PlatformError eError);
```

### Zenith_PlatformEvent

The 64-byte event record, its kinds, payloads and the world-visible flag are `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent".

### Clocks

ZenithPlatform declares no clock: it calls ZenithCore's `Zenith_Clock.h` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks"), and every platform timestamp is on that clock (`Zenith2/Docs/Design/PLAT/Platform.md` → "The monotonic clock").

### Public API in the split files

| API | Heading |
|---|---|
| `Zenith_Window`, native handles, Vulkan surfaces | `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API" |
| `Zenith_Clipboard` | `Zenith2/Docs/Design/PLAT/Platform.md` → "Clipboard" |
| account signals | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account signal sources" |
| `Zenith_DevListener`, `Zenith_FileWatcher` | `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket", "File watcher" |
| devices, transitions | `Zenith2/Docs/Design/PLAT/Input.md` → "Device kinds and IDs", "Zenith_InputDevices API" |
| gestures | `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture recogniser API" |
| text input | `Zenith2/Docs/Design/PLAT/Input.md` → "Text input API" |
| haptics | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API" |
| the input system | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputSystem API" |
| actions and state | `Zenith2/Docs/Design/PLAT/Input.md` → "Action types", "Action IDs", "Action state" |
| bindings | `Zenith2/Docs/Design/PLAT/Input.md` → "Binding model", "Binding display" |
| contexts and claims | `Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks", "Presentation claims" |
| the mapper | `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm" |
| rebinding | `Zenith2/Docs/Design/PLAT/Input.md` → "Rebind operation" |
| commands | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| paths | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualPath" |
| mounts and the filesystem | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources", "Zenith_VirtualFileSystem API" |
| command-line files | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files" |
| asynchronous I/O | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API" |
| configuration | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API", "Zenith_Config API" |
| lifecycle and resume | `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_AppLifecycle API" |
| thermal | `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_ThermalMonitor API" |
| accessibility | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityService", "Zenith_AccessibilityTreeProvider", "Zenith_AccessibilityTestAccess API" |

## Data structures and formats

### Format index

| Format | Kind | Owner | Specified in |
|---|---|---|---|
| `Zenith_PlatformEvent` | in-memory, 64 bytes | PLAT | `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent" |
| `Zenith_InputTransition` | in-memory, 56 bytes | PLAT | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API" |
| `Zenith_InputControl`, `Zenith_ControlPath` | in-memory, 4 and 8 bytes | PLAT | `Zenith2/Docs/Design/PLAT/Input.md` → "Device kinds and IDs", "Binding model" |
| `Zenith_InputCommand` and its value encoding | in-memory 20 bytes; serialized 18 bytes per command in the ZRPL tick block | PLAT (the ZRPL container is SAVE's) | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| `*.bindings.json` (`zenith2.input-bindings`, `-platform`, `-user`) | JSON authoring and user data | PLAT | `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema", "Persisting user bindings" |
| input script (`zenith2.input-script`) | JSON test and CI input | PLAT | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format" |
| `*.config.json` | JSON authoring and user data | CORE's format; PLAT's file rules | `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration file schema" |
| tier list (`zenith2.quality-tiers`) | JSON engine content | PLAT | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier resolution" |
| configuration dump (`zenith2.config-dump`) | JSON diagnostics | PLAT | `Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump" |
| simulation-affecting key list in the replay header | binary | CORE writes and parses it; the header is SAVE's; PLAT applies it | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Simulation-affecting keys" |
| `mount-manifest.json` (`zenith2.mount-manifest`) | JSON install layout | PLAT reads; PACK writes | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema" |
| `update.zman` | binary, signed | PACK and SEC; PLAT verifies | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" |
| `resume.zsave` | SAVE's save format, kind resume | SAVE; PLAT's policy | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy" |
| temporary and quarantine names (`<file>.z2atomic`, `<file>.corrupt-<UTC>`, `<file>.v<N>.bak`) | file-naming conventions | PLAT | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling" |

### Common JSON conventions

- Every JSON document whose format PLAT owns (bindings, user bindings, input scripts, tier lists, the configuration dump, mount manifests) has a top-level `schema` string and an integer `schemaVersion`; a reader refuses a schema string it does not own and treats a newer version per the document's rules (content: rejected; user data: read-only, never overwritten).
- Documents are UTF-8 with LF line endings, parsed under the parser limits of `Zenith2/Docs/Architecture.md` → "Security architecture" with each format's own smaller cap, by ZenithCore's `Zenith_JsonDocument` reader (`Zenith2/Docs/Design/CORE/Text.md` → "JSON reader", backed by the D-SEL-JSON (open, Stage 1) selection), which reports the line of every value for provenance and diagnostics.
- Members prefixed `x-` are preserved and ignored; unknown members elsewhere are errors in content documents and preserved in user documents.
- Documents PLAT writes (the user configuration, user bindings, the dump) are byte-stable: members in a fixed order (sorted by key bytes where the format has keyed maps), written by ZenithCore's `Zenith_JsonWriter` in its byte format (`Zenith2/Docs/Design/CORE/Text.md` → "Byte format"), no timestamps.
- Game bindings and configuration files are project-settings authoring documents (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Authoring-document migration"): version 1 has no migration step; a schema change adds a step, its fixtures under `Zenith2/Engine/ZenithPlatform/Tests/Fixtures/Migration/InputBindings/` and `…/Migration/Config/`, and the registration described in § "Migration hooks".

### Platform event record layout

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | `m_eKind` |
| 2 | 2 | `m_uFlags` |
| 4 | 4 | reserved |
| 8 | 8 | `m_ulTimestampNs` |
| 16 | 8 | `m_ulSequence` |
| 24 | 8 | `m_xWindow` |
| 32 | 32 | the payload union |

The record never leaves the process and is not serialized; a world-visible event reaches the replay log as the publication payload of § "Frame-graph placement", encoded field by field little-endian, so the payload structs of the eight world-visible kinds are part of the ZRPL format version.

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in | Complexity |
|---|---|---|
| platform creation and teardown | `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" | O(windows + devices) |
| the pump | `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm" | O(events + transitions + claimants × pointer downs) |
| window creation | `Zenith2/Docs/Design/PLAT/Platform.md` → "Window creation and destruction" | O(1) |
| display-scale propagation | `Zenith2/Docs/Design/PLAT/Platform.md` → "Display scale changes" | O(1) per change |
| account-signal delivery | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery" | O(signals) |
| transition logging and coalescing | `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log" | O(transitions) |
| haptic mixing | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic mixing" | O(devices × channels) per pump |
| the action mapper | `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm" | O(T × k + A) |
| command emission and state rebuild | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" | O(changes), O(commands) |
| bindings loading and layering | `Zenith2/Docs/Design/PLAT/Input.md` → "Binding layers and provenance" | O(file bytes + bindings × log bindings) |
| path validation | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order" | O(512) worst case |
| resolution | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm" | O(mounts × sources) lookups, cached |
| the escape check | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution" | one syscall per open |
| patch verification | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" | O(patch bytes) hashing |
| the I/O scheduler | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "I/O scheduler algorithm" | O(1) per dequeue |
| atomic write | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes" | O(bytes) |
| configuration resolution | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Resolution algorithm" | O(source bytes), O(layers) per key |
| the resume write and relaunch decision | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence", "Relaunch decision" | O(1) policy; SAVE's serialization cost |
| accessibility publication and change detection | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Publication algorithm", "Change detection" | O(n log n) nodes, O(p) pool bytes |
| accessibility snapshot queries and hit testing | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Snapshot queries", "Hit testing" | O(log n) per node query, O(n) per hit test |

### The Platform Pump step

`Platform.Pump` is step 1 of the frame prologue, which `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure" fixes as exactly eight main-thread steps with no frame-graph job running. PLAT's work in the prologue is confined to three of them:

| Prologue step | PLAT work |
|---|---|
| 1 `Platform.Pump` | `Zenith_Platform::Pump()` (no argument: the platform owns its frame arena): cross-thread queues, the SDL drain and translation, pointer claims, gestures, the transition window, the presentation mapper and action claims, the lifecycle state machine's main-thread actions, the haptic update, the counters |
| 3 queued user-layer configuration writes | `Zenith_Config::ApplyQueuedUserWrites`: the staged user values, including any quality-tier change, and the change callbacks |
| 5 thermal actions | `Zenith_ThermalMonitor::GetLatestSample()` (Architecture's name), then `Zenith_ThermalMonitor::ApplyActions` (`Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"); the sample itself is taken inside step 1 |

The runtime calls `Zenith_InputDevices::CarryFrameTransitions()` after `RunFrame` when the input world ran no tick (frame loop step 3; `Zenith2/Docs/Design/MOD.md` → "Step construction notes", row 19) and `ClearCarriedTransitions()` after a `RunFrame` in which the input world ran at least one tick (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events"), and `Zenith_Platform::WaitForEvents(100)` when `Zenith_AppLifecycle::ShouldThrottle()` (`Zenith2/Docs/Design/PLAT/Android.md` → "Windows equivalents").

<!-- assumption: pending MOD -->
The step-19 row of `Zenith2/Docs/Design/MOD.md` → "Step construction notes" names `CarryFrameTransitions()` but not `ClearCarriedTransitions()`; PLAT assumes MOD's frame-loop driver adds the clear call after a `RunFrame` in which the input world ran at least one tick. The request is item (d) of row P6 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.
<!-- /assumption -->

The platform holds no `Zenith_Config` (`Zenith_PlatformDesc` has no configuration member), so the runtime configures thermal adaptation: it calls `Zenith_ThermalMonitor::Configure` after `Zenith_Config::Create` in startup step 6, with each `Zenith_ThermalConfig` field that has a `thermal.*` key read from that key and `m_uTierCount` from `Zenith_NarrowCast<uint32>(Zenith_Config::GetQualityTiers().GetCount())`, and calls it again from `Zenith_Config::GetChangeRegistry()` whenever a `thermal.*` key changes (`Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"; row P6 (i) of the cross-area notes).

### State machine index

| State machine | Specified in |
|---|---|
| the application lifecycle (5 states × 5 events) | `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine" |
| the surface handshake | `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake" |
| thermal adaptation | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine" |
| the low-disk state | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Low-disk state machine" |
| an I/O request (pending, running, chunk, complete; cancellation cells) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime", "Cancellation" |
| tap, double tap, long press, pan, pinch and rotate recognisers | `Zenith2/Docs/Design/PLAT/Input.md` → "Tap state table" … "Pinch and rotate state table" |
| interactions (default, press, hold, tap, slow tap, multi-tap) | `Zenith2/Docs/Design/PLAT/Input.md` → "Interactions" |
| the rebind operation | `Zenith2/Docs/Design/PLAT/Input.md` → "Rebind operation" |

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Thread affinity vocabulary") |
|---|---|
| `Zenith_Platform`, `Zenith_Window`, `Zenith_Clipboard`, `Zenith_TextInput`, `Zenith_AppLifecycle`, `Zenith_ThermalMonitor` | `MAIN_THREAD` (listed deviations: `PostAccountSignal`, `GetClockOffsetNs`, `Zenith_Window::GetNativeHandle`, `GetMetrics`) |
| `Zenith_InputDevices` | `MAIN_THREAD` except `GetFrameTransitions`, `GetCarriedTransitions` (read-only during the frame) and `InjectSyntheticTransition` |
| `Zenith_InputSystem` | `MAIN_THREAD` except `GetBindingTable` and `GetActionSet` (read-only during the frame) |
| `Zenith_HapticPlayer` | `MAIN_THREAD` except `Submit` (`ANY_THREAD`) |
| `Zenith_ActionMapper`, `Zenith_InputContextStack`, `Zenith_InputActionStateTable`, `Zenith_GestureRecogniserSet` | `OWNER_THREAD` |
| `Zenith_VirtualPath`, `Zenith_InputCommand`, `Zenith_PlatformEvent`, `Zenith_InputTransition` | `IMMUTABLE` values |
| `Zenith_VirtualFileSystem` | `ANY_THREAD` for resolution and stat; blocking reads not on job workers; mount changes `MAIN_THREAD` |
| `Zenith_MountSource` implementations | `ANY_THREAD` |
| `Zenith_AsyncIO` | `ANY_THREAD` (submission never blocks) |
| `Zenith_IOCompletionQueue` | `ANY_THREAD` producers, `OWNER_THREAD` drain |
| `Zenith_Config` | `MAIN_THREAD` (values reach other threads through their owners' descs and the input system's configuration-scale snapshot) |
| `Zenith_DevListener`, `Zenith_FileWatcher` | `OWNER_THREAD` |
| `Zenith_AccessibilityService` | `MAIN_THREAD`; OS queries read its snapshot on their own threads under `uLOCK_LEVEL_ACCESSIBILITY` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility thread affinity") |
| accessibility value types | `IMMUTABLE` values |

### Frame-graph placement

| Where | Node or step | Work | Declared access |
|---|---|---|---|
| frame prologue step 1 | `Platform.Pump` (main thread, no job running) | § "The Platform Pump step" | none (not a frame-graph node) |
| frame prologue step 3 | configuration writes | `ApplyQueuedUserWrites` | none |
| frame prologue step 5 | thermal | `GetLatestSample`, then `ApplyActions` | none |
| tick boundary, step 1, every world | asynchronous publication (`Zenith2/Docs/Architecture/FramePhases.md` → "The tick boundary") | the world-visible platform events of the frame, published in completion-ID order | the world's publication state (the tick-boundary node's own) |
| phase 1, every world | `Input.Actions@W<tag>` (`JOB_AFFINITY_ANY`) | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system" | reads `Zenith_FrameInputResource`; writes `Zenith_InputActionStateResource`, `Zenith_SimulationInputContexts`, `Zenith_InputTickSummary` |
| sync points after phases 2–10 | command playback | `Input.PushContext`, `Input.PopContext`, `Input.PlayHaptic`, `Input.StopHaptic` | the playback access of the sync point |
| phase 9, every world | event delivery | the `Platform.*` events appended at the tick boundary | the event queue |
| epilogue, every world | `Input.Haptics@W<tag>` (`JOB_AFFINITY_ANY`) | submits the tick's haptic requests | reads `Zenith_HapticRequestQueue` |
| any thread | I/O threads (not frame-graph threads) | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "I/O scheduler algorithm" | — |

Platform events in simulation (the world-visible kinds of `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent"):

1. In the prologue, the runtime bridge turns each world-visible event of the frame, in sequence order, into one asynchronous result per world with `m_uSourceKind = ASYNC_SOURCE_PLATFORM` and `m_uKind = PUBLICATION_KIND_PLATFORM_RECORDED` (`Zenith_ReplayPublicationHeader` of `Zenith2/Docs/Architecture.md` → "Multiplayer foundations"), and a completion ID from the world's monotonic counter assigned in that order. Its payload is 36 bytes, little-endian, field by field: `uint16` the `Zenith_PlatformEventKind`, `uint16` reserved (0), then 32 kind-specific bytes: the event's payload struct for the device, disk, account and configuration kinds (padding zeroed), and for `PLATFORM_EVENT_LOCALE_CHANGED` the locale tag as UTF-8 padded with zeros (a tag longer than 32 bytes is recorded as `und`). Window IDs, device IDs and text-arena offsets are platform-scoped and are zeroed in the payload; the script-visible fields of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" come only from these bytes.
2. At the world's next tick boundary they are published with every other completed result in completion-ID order, and the publication tick and payload are written to the replay log (a platform event is not reproducible, so its payload is recorded, `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results"); a replay publishes the recorded payloads at the recorded ticks and ignores the live platform events.
3. The bridge's publication handler appends the matching `Platform.*` event to the world's event buffer, delivered in phase 9 of that tick in merge order (`Zenith2/Docs/Architecture/FramePhases.md` → "Phase-specific rules").
4. Account events are first handed to the online-services adapter by the runtime, before step 1 (`Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery").

`ASYNC_SOURCE_PLATFORM = 7` and `PUBLICATION_KIND_PLATFORM_RECORDED = 3` are Architecture's (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations", [Z2-N-ARCH-791], which names this section's 36-byte payload as the layout).

<!-- assumption: pending SCRIPT -->
Scripting consumer note: `Zenith2/Docs/Scripting.md` → "Merge-time capacity and asynchronous results" still lists the source kinds up to `ASYNC_SOURCE_WORLD_LOAD`; PLAT assumes it publishes a `PUBLICATION_KIND_PLATFORM_RECORDED` result like `PUBLICATION_KIND_GENERIC_RECORDED` (payload recorded, replay publishes the recorded bytes at the recorded tick and ignores live platform events), with the runtime bridge's handler, not a script continuation, consuming it and appending the `Platform.*` event. The row is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.
<!-- /assumption -->

### Lock-order entries

| Level | Constant | Owner | Guards | Short-hold | Status |
|---|---|---|---|---|---|
| 600 | `uLOCK_LEVEL_FILESYSTEM_MOUNTS` | ZenithPlatform | the mount-table pointer and its snapshot counts | yes | in `Zenith2/Docs/Contracts.md` → "Lock ordering table" |
| 610 | `uLOCK_LEVEL_IO_QUEUES` | ZenithPlatform | the five priority queues, the per-target in-flight set | yes | in `Zenith2/Docs/Contracts.md` → "Lock ordering table" |
| 620 | `uLOCK_LEVEL_PLATFORM_CROSS_THREAD_QUEUES` | ZenithPlatform | the account-signal, lifecycle hand-over, thermal-status and low-disk queues | yes | in `Zenith2/Docs/Contracts.md` → "Lock ordering table" |
| 630 | `uLOCK_LEVEL_DEV_FILE_WATCH_QUEUE` | ZenithPlatform | the file watcher's change queue | yes | in `Zenith2/Docs/Contracts.md` → "Lock ordering table" |
| 640 | `uLOCK_LEVEL_ACCESSIBILITY` | ZenithPlatform | the published accessibility snapshot and revision, the action queue, the OS state, preference and speech cells, the Windows speech command queue | yes | in `Zenith2/Docs/Contracts.md` → "Lock ordering table" (cross-document item X270) |

- No PLAT lock is held across an I/O system call, an SDL call, a callback or a wait; the I/O threads take 610 only to dequeue and re-queue, and take 600 only inside `Resolve`, never while holding 610 (600 < 610, so the order would also permit it).
- The completion queue and the haptic submission queue are lock-free bounded rings; the handshake uses a `Zenith_Semaphore`, which is not in the lock table.
- The rows 610, 620 and 630 are in `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" (table "Lock ordering table").

### Threads PLAT owns and shutdown

| Thread | Owner | Started | Joined |
|---|---|---|---|
| I/O threads (`io.threadCount`, default 2) | `Zenith_AsyncIO` | `Zenith_AsyncIO::Create` (startup step 5) | its destructor, in Contracts shutdown step 8, after the two flushes (the platform object itself is destroyed after step 10, below) |
| the file-watcher thread (development builds) | `Zenith_FileWatcher` | `Create` | its destructor, by its owner before step 8 |
| the watchdog thread (`--timeout-seconds` only) | `Zenith_Platform` | `StartWatchdog`, after startup step 4 | the platform's destructor (or never: it ends the process when it fires) |
| the Android UI thread | Android | — | never (PLAT only receives the event watch on it) |
| SDL3's internal threads (hidapi, joystick) | SDL3 | `SDL_Init` | `SDL_Quit` in the platform's destructor |
| `Zenith.Speech` (Windows, accessibility OS backend) | `Zenith_AccessibilityService` | creation step 11 | the service's destructor, first in the platform's destructor (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility shutdown order") |
| UI Automation's provider threads; Android's UI thread and `TextToSpeech` threads | the OS | — | never (they read the snapshot or call the natives; PLAT disconnects them before the service is freed) |

The watchdog thread (`Zenith.Watchdog`) is joined by the platform's destructor after Contracts step 10 (MOD's step 7), which no row of `Zenith2/Docs/Contracts.md` → "Shutdown ownership order" names: per [Z2-N-CONTRACT-348] the ticket implementing `StartWatchdog` extends, in its own diff, the shutdown row that MOD's clarification of step 8 produces (the platform object, destroyed after step 10) with `Zenith.Watchdog` in its "Threads joined" cell, and adds that row when it has not landed.

<!-- assumption: pending CONTRACT -->
The shutdown row for the platform object after Contracts step 10 is MOD's recorded clarification (`Zenith2/Docs/Design/MOD.md` → "Shutdown sequence"), queued for Contracts as cross-doc item X93; PLAT assumes it and names its thread there.
<!-- /assumption -->

PLAT's objects are destroyed in two places, following `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence" (MOD's steps 6 and 7), which reads Contracts' step 8 "destroys audio, then platform services" as the owners of the platform's I/O and timer threads (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Shutdown ownership order"):

1. The file watcher has already been destroyed by its owner.
2. Contracts step 8, first: `Zenith_Config::FlushUserLayer()` and `Zenith_InputSystem::FlushUserBindings()` on the main thread. Each submits its pending `WriteAtomic`, if any, and waits for that operation to complete. Contracts step 4 has already cancelled and completed every operation outstanding at that time; a user write it cancelled is unsaved, so the flush submits it again. `Zenith_AsyncIO` still accepts requests here, because nothing has begun its own shutdown.
3. Contracts step 8, then: `Zenith_AsyncIO`, whose destructor refuses new requests with `IO_ERROR_SHUTTING_DOWN`, completes any request still queued as cancelled, and joins the I/O threads (the "I/O threads" of step 8's row). A flush attempted after this point returns `CONFIG_SOURCE_ERROR_IO_UNAVAILABLE` or `INPUT_ERROR_IO_UNAVAILABLE` and logs; the previous files stay intact.
4. Contracts steps 9 and 10: the RHI device with the swapchain, then the asset registry (MOD's step 6).
5. After Contracts step 10 (MOD's step 7): `Zenith_Config` (it holds `Zenith_AsyncIO*` and `Zenith_VirtualFileSystem*` and uses neither in its destructor), `Zenith_VirtualFileSystem`, then `Zenith_Platform`, which owns the input system and whose destructor order is `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc". The platform outlives the RHI device because the swapchain's surface belongs to a platform window and must be destroyed before it (`Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API").

Item 2 is `Zenith2/Docs/Design/MOD.md` → "Shutdown sequence" step 6, which calls both flushes on the main thread while `Zenith_AsyncIO` is still alive and then destroys it (cross-doc item X82, applied).

`Test_Config_FlushBeforeIOShutdown` and `Test_UserBindings_FlushBeforeIOShutdown` assert that the files land in this order.

### Cancellation

Every PLAT asynchronous operation is an operation handle whose cancellation points are stated in its contract (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Cancellation completion"):

| Operation | Cancellation points | Specified in |
|---|---|---|
| read | dequeue, every chunk boundary | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation" |
| atomic write | dequeue, every chunk boundary before the rename; never during the rename | as above |
| delete, rename, directory creation | dequeue only | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Other user-mount operations" |
| the resume write (SAVE's operation, PLAT's deadline) | SAVE's points; PLAT requests cancellation at the deadline | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence" |
| a rebind | `CancelRebind`, a cancel control, the timeout | `Zenith2/Docs/Design/PLAT/Input.md` → "Rebind operation" |

### Error enumerations

| Enumeration | Header | Enumerators in |
|---|---|---|
| `Zenith_PlatformError` | `Zenith_Platform.h` | § "Zenith_Platform" |
| `Zenith_InputError` | `Zenith_InputTypes.h` | `Zenith2/Docs/Design/PLAT/Input.md` → "Input error enumerations" |
| `Zenith_PathErrorCode` (in the error record `Zenith_PathError`), `Zenith_FileError` | `Zenith_VirtualFileSystem.h` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Filesystem error enumerations" |
| `Zenith_IOErrorCode` (in the error record `Zenith_IOError`) | `Zenith_AsyncIO.h` | as above |
| `Zenith_ConfigSourceError` (key-level errors are ZenithCore's `Zenith_ConfigError`) | `Zenith_Config.h` | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration error enumerations" |
| `Zenith_LifecycleError` | `Zenith_AppLifecycle.h` | `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_AppLifecycle API" |
| `Zenith_DevServiceError` | `Zenith_DevSocket.h` | below |
| `Zenith_AccessibilityError` | `Zenith_Accessibility.h` | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility error enumeration" |

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_DevSocket.h
enum Zenith_DevServiceError : uint8
{
	DEV_SERVICE_ERROR_NOT_IN_SHIPPING = 1,   // every entry point in a Shipping build
	DEV_SERVICE_ERROR_BIND_FAILED,           // the address or port could not be bound
	DEV_SERVICE_ERROR_SOCKET,                // any other socket failure; the OS code is logged
	DEV_SERVICE_ERROR_TOO_MANY_CONNECTIONS,
	DEV_SERVICE_ERROR_UNKNOWN_CONNECTION,
	DEV_SERVICE_ERROR_WATCH_UNAVAILABLE,     // no loose mount, or the OS watch could not be created
	DEV_SERVICE_ERROR_OUT_OF_MEMORY,
};
constexpr const char* Zenith_ToString(Zenith_DevServiceError eError);
```

Every enumeration starts at 1, has a `Zenith_ToString` overload, and keeps each enumerator's meaning once shipped (`Zenith2/Docs/Contracts.md` → "Errors and results", "Error enumerations"). Errors crossing into a caller's enumeration are mapped explicitly (the path, file and I/O mapping is `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Filesystem error enumerations").

### Luau errors versus asserts

| Condition | Handling |
|---|---|
| a script names an unknown action or context | the unknown-name misuse error at module load (`Zenith2/Docs/Scripting.md` → "Views epochs and misuse errors"), never an assert |
| a script pushes a context that fills the stack, or pops one not on it | the command is recorded; playback drops it with `input.contexts.push_refused` or counts the no-op; no Luau error, because the outcome depends on state at playback |
| a script passes a haptic effect with out-of-range values | clamped and counted at playback; no Luau error |
| a path, a bindings file, a configuration file, a mount manifest, an input script, a replay command, clipboard text, IME text, an account signal | untrusted input: a `Zenith_Result` error or a counted rejection, never an assert (`Zenith2/Docs/Contracts.md` → "Errors and results", "Untrusted input") |
| a synchronous read on a job worker, a null heap in a desc, a pump from a thread other than the main thread | programmer errors: `ZENITH_CHECK` (the blocking-wait check) or `ZENITH_ASSERT` |
| a full lifecycle hand-over queue (more than 32 events in a frame) | `ZENITH_FATAL(FATAL_CODE_QUEUE_FULL)`: a defect, never a load condition |

## Diagnostics and profiling

### Counters

<!-- elaboration -->
Every counter is published through the installed profiler hooks each frame with its `eKind` ([Z2-N-CONTRACT-155]): `CUMULATIVE` for a running total since the object was created (including high-water marks), and `GAUGE` for a value that is neither a total nor a change: the frame's own measurement or a current level. No PLAT counter is published as `COUNTER_KIND_DELTA`, so no name ends in `_delta` ([Z2-N-CONTRACT-156]). Every bounded queue of `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow" publishes the `.full`, `.dropped` and `.high_water` triple of [Z2-N-CONTRACT-345], cumulative as [Z2-N-CONTRACT-155] requires.

<!-- assumption: pending CONTRACT -->
`Zenith_CounterKind` of `Zenith2/Docs/Contracts.md` declares `COUNTER_KIND_CUMULATIVE` and `COUNTER_KIND_DELTA`; the rows marked `GAUGE` assume the enumerator `COUNTER_KIND_GAUGE` (cross-doc item X83). The row is recorded in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`.
<!-- /assumption -->

| Counter | Kind | Meaning |
|---|---|---|
| `platform.pump_us` | GAUGE | microseconds spent in `Platform.Pump` this frame |
| `platform.events.count` | GAUGE | events this frame |
| `platform.events.high_water` | CUMULATIVE | the ring's high-water mark |
| `platform.events.ignored`, `.full`, `.dropped`, `.stale_window` | CUMULATIVE | the event ring's rejections |
| `platform.frame_arena.exhausted` | CUMULATIVE | pumps whose frame arena ran out |
| `platform.account_signals.full`, `.dropped`, `.high_water`, `platform.clipboard.truncated` | CUMULATIVE | platform services |
| `platform.lifecycle_queue.full`, `.dropped`, `.high_water`, `platform.low_disk_queue.full`, `.dropped`, `.high_water` | CUMULATIVE | the lifecycle hand-over and low-disk queues (`.dropped` of the `FATAL` lifecycle queue is always 0) |
| `platform.thermal_headroom`, `platform.thermal_status`, `platform.thermal_drs_steps`, `platform.thermal_tier_offset` | GAUGE | thermal adaptation |
| `platform.thermal_samples_invalid` | CUMULATIVE | NaN or unavailable samples |
| `input.transitions.count`, `input.carry.count` | GAUGE | transitions this frame; carried transitions |
| `input.transitions.coalesced`, `.dropped_motion`, `.dropped_buttons`, `input.carry.dropped` | CUMULATIVE | the transition log and carry |
| `input.devices.gamepads` | GAUGE | connected gamepads |
| `input.devices.gamepads_rejected`, `.unmapped_joysticks`, `input.touch.over_limit`, `input.mappings.rejected` | CUMULATIVE | devices and the mapping database |
| `input.gestures.bad_sequence`, `input.text.invalid_utf8`, `input.claims.ignored` | CUMULATIVE | gestures, IME, claims |
| `input.haptics.active` | GAUGE | playing effects |
| `input.haptics.refused`, `.clamped`, `.unsupported`, `.requests_dropped` | CUMULATIVE | haptics |
| `input.haptics.submissions.full`, `.dropped`, `.high_water` | CUMULATIVE | the haptic submission queue |
| `devtools.watcher_queue.full`, `.dropped`, `.high_water` | CUMULATIVE | the file watcher's drain queue (development builds) |
| `input.commands.count` | GAUGE | commands of the input world's last tick |
| `input.commands.dropped`, `.rejected`, `input.contexts.push_refused`, `input.bindings.orphans` | CUMULATIVE | commands, contexts and user bindings |
| `vfs.resolve_cache.hits`, `.misses`, `vfs.escape_rejections`, `vfs.case_mismatches`, `vfs.name_mismatches`, `vfs.patches_rejected` | CUMULATIVE | the filesystem |
| `vfs.patch.stamp_skips`, `vfs.patch.files_hashed`, `vfs.patch.bytes_hashed` | CUMULATIVE | the patch mount's U9 at startup step 5: files skipped on an unchanged identity stamp, files and bytes hashed (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification") |
| `platform.connectivity.regained` | CUMULATIVE | OFFLINE → ONLINE transitions (`Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity") |
| `input.script.record_dropped` | CUMULATIVE | commands an input-script recording could not keep (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input-script recording") |
| `io.queue.<priority>.depth` | GAUGE | the I/O queues' current depths |
| `io.queue.<priority>.high_water` | CUMULATIVE | the I/O queues' high-water marks |
| `io.queue.<priority>.full`, `io.queue.<priority>.dropped` | CUMULATIVE | rejections per queue (`.dropped` equals `.full` for these `REJECT` queues) |
| `io.bytes_read`, `io.bytes_written`, `io.requests_completed`, `io.requests_cancelled`, `io.requests_failed`, `io.preemptions` | CUMULATIVE | I/O throughput; the profiler derives per-second rates from successive values |
| `io.critical_latency_us` | GAUGE | the latest critical request's queue-to-completion time |
| `io.low_disk_refusals`, `io.stale_temps_removed` | CUMULATIVE | writes |
| `config.unknown_keys`, `config.files_rejected`, `config.values_rejected`, `config.user_writes` | CUMULATIVE | configuration |
| `lifecycle.handshake_timeouts`, `lifecycle.resume_deadline_missed`, `lifecycle.resume_deleted_live` | CUMULATIVE | lifecycle |

The accessibility service's `platform.accessibility.*` counters are listed with their kinds in `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility counters" and asserted by `Test_Accessibility_CountersPublished`; `Test_PlatformCounters_Published` covers the table above.

### On-screen profiler rows

The on-screen profiler (DEVT, Z2-D-005) shows these PLAT rows, from the counters above and the objects' snapshot calls:

| Panel | Rows |
|---|---|
| frame | `Platform.Pump` time; the `Input.Actions` node per world (from its zone) |
| I/O | queue depth per priority, bytes read and written per second, critical latency, preemptions, cancellations, low-disk state and free bytes |
| configuration | every key with value, layer, source and line; pending-restart keys highlighted; rejected files (from `WriteDump`) |
| platform | windows with metrics and focus; lifecycle state; thermal headroom, status, resolution cap and tier offset |

### Input debug overlay

Toggled by `Dev.ToggleInputOverlay`, fed by `Zenith_InputSystem::WriteDebugSnapshot`:

| Row | Content |
|---|---|
| devices | each device: kind, ID, name, gamepad type, capabilities, connected time |
| scheme | the active scheme and the device that set it |
| stacks | the presentation stack and the input world's simulation stack, top first, pinned flags |
| actions | every action: value, held, pressed and released this frame, phase flags, tap count, held time, driving binding (presentation) and the input world's tick state |
| transitions | the last 32 transitions with path, value, flags, sequence |
| claims | claimed pointers and actions of the frame and their claimant |
| gestures | each recogniser's state |
| commands | commands this tick, dropped and rejected counts |

### Log categories

| Category | Used for |
|---|---|
| `platform` | creation, windows, displays, account signals, clipboard |
| `input` | devices, bindings diagnostics, rebinding, user bindings, commands |
| `vfs` | mounts, resolution failures, escape and case rejections, patches |
| `io` | request failures, low disk, stale temporary files |
| `config` | rejections, unknown keys, file failures, resets |
| `lifecycle` | transitions, the handshake, the resume decision and write |
| `thermal` | samples at debug level, every adaptation step at info level |
| `devtools` | listener binds, tokens, connections, watcher overflow |
| `accessibility` | the backend chosen, OS-side failures, rejected trees, speech availability, screen-reader state; never node text (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility log lines") |

Messages use the formats given where they are specified (`input bindings: <file>:<line>: <message>`, `config: <source>:<line>: <key>: <reason>; keeping <layer> value <value>`, `resume state discarded: <reason>`, `patch rejected: <version>: <reason>`).

### Frame-dump and config-dump entries

- The configuration dump (`--config-dump`, `WriteDump`) carries configuration and binding provenance (`Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump").
- The frame dump (DEVT's) includes, per frame: the frame's platform events (kind, window, sequence), the input world's commands of each tick, `Zenith_InputTickSummary` per tick, the lifecycle state, the thermal sample and the I/O queue depths.

### Telemetry

Frame telemetry (frame loop step 3) records `platform.pump_us`, the input world's command count and newest consumed event sequence (for latency, `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Latency attribution"), the thermal sample, the low-disk state and `io.critical_latency_us`. The budget-run mode adds the thermal status at 1 Hz and, on the S24, the MP-MEM samples of § "The Stage 1 PLAT harness".

## Authoring and cooking

### Input bindings authoring

- Games author `Games/<Name>/Assets/Input/*.bindings.json` and per-platform `Assets/Input/Platform/<p>/*.bindings.json` as JSON (Z2-D-038), cooked unchanged to `content:/Input/…`.
- The cook validates every bindings file with `Zenith_ValidateBindingsJson` (the loader's validator, same rules and messages) and fails the cook on any diagnostic, so a shipped build never meets a bindings error at startup.
- The editor's input-binding UI is EDIT's; PLAT exposes to it the loaded `Zenith_InputBindingTable`, `GetBindingProvenance`, `GetBindingDisplay`, the validator, the canonical writer `Zenith_WriteBindingsJson` (byte-stable output) and the rebind operation for capturing a control.

### Configuration authoring

- Games author `Games/<Name>/Assets/Config/game.config.json`, `Assets/Config/Platform/<p>.config.json` and `Assets/Config/Quality/<TierID>.config.json`; each engine library's own fragments are `Zenith2/Engine/<Library>/Internal/Config/**`, merged by the cook into the platform layer and the engine tier files (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Engine and game sub-sources").
- The cook validates each file against the registered key set of the tools build (unknown keys, types, ranges, allowed layers), reporting with file and line; unknown keys are cook warnings (a module the game does not select may own them) and every other finding fails the cook.

### Mount manifests and packaging

The cook writes `mount-manifest.json` into each cooked output for development; PACK's packager writes the shipped one and the asset-pack entries (`Zenith2/Docs/Design/PACK/Formats.md` → "Mount manifests PACK writes"). PLAT supplies `Zenith_ParseMountManifest` for both to validate what they write, and ZenithCook's `--package-verify` runs PLAT's public update-manifest verifier (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API").

### Editor surfaces PLAT exposes

| Surface | PLAT API |
|---|---|
| multiple windows and docking viewports | `Zenith_Platform::CreateWindow`, window-scoped contexts |
| drag-and-drop import | `PLATFORM_EVENT_DROP_FILE` with `m_bAcceptFileDrops` |
| input-binding editor and remapping preview | § "Input bindings authoring" |
| configuration panel | `WriteDump`, `GetProvenanceChain`, `SetUserValue` |
| script and asset hot reload | `Zenith_FileWatcher` |
| input recording for tests | `--record`, the input-script format |

### Migration hooks

Game bindings and configuration files are project-settings authoring documents whose migration chains are registered with the authoring-document migration framework (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Migration step interface"), which SCENE and ASSET deliver in Stage 2. ZenithPlatform is below the libraries that host the migration registry, so its document types' steps are registered by the tools that migrate documents (ZenithCook and ZenithEditor) from a PLAT-provided table of step functions, as `Zenith2/Docs/Design/ASSET.md` → "Migration hooks" registers them; version 1 has no steps. User files are user data, not authoring documents: their newer-version and orphan rules are `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings" and `Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer".

## Failure handling

### Failure table

| Failure | Behaviour | Surfaced | Specified in |
|---|---|---|---|
| SDL initialisation, base or pref path, or main window creation fails | startup stops with exit code 4 and the SDL error logged | the log | `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" |
| allocation failure in any PLAT factory | the factory unwinds what it created and returns its out-of-memory error | the caller's error | every `Create` of this family |
| event ring full | grows to the hard cap, then drops (motion first) and counts | counters | `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow" |
| transition log or carry full | coalesces, then drops motion, then buttons, counting; the mapper resynchronises from device state | counters | `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log", "Mapper algorithm" |
| a controller disconnects | § "Controller loss" | `Platform.DeviceDisconnected` | `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug" |
| the Android surface is lost | § "Surface loss" | `PLATFORM_EVENT_SURFACE_LOST` | `Zenith2/Docs/Design/PLAT/Android.md` |
| a bindings content file is invalid | startup stops with exit code 3 and every diagnostic | the log | `Zenith2/Docs/Design/PLAT/Input.md` → "Validation rules" |
| a user bindings or user configuration file is corrupt | quarantined, defaults apply | `Platform.ConfigReset` | § "Corrupt configuration" |
| a content configuration file is corrupt | the file is ignored, lower layers apply | the dump's `rejectedFiles` | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling" |
| a path from content, saves, configuration or scripts is invalid or escapes | a path error; the access does not happen | the caller's error; `vfs.escape_rejections` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order", "Escape check after resolution" |
| a Shipping build has no valid mount manifest | startup stops with exit code 3 | the log | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema" |
| a patch fails verification (any of U1–U5, U7–U9, U11, or a file changed while it was hashed) | rejected whole; the next lower candidate is tried; previous content stays mounted | the log; `vfs.patches_rejected` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" |
| a file identity stamp cannot be taken (an unsupported volume, an OS error) | the file is hashed in full; never a failure | `vfs.patch.files_hashed` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "File identity stamps" |
| the connectivity source cannot register (an OS without the API, a template without the Java class) | the state stays UNKNOWN; consumers fall back to their own back-off | one `platform` log line | `Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity" |
| a directory-grant read names a refused or oversized file | an error result; nothing is opened | the caller's error | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Directory-grant reads" |
| an I/O request fails (not found, access denied, short read, corrupt archive, OS error) | the operation reports FAILED with its `Zenith_IOError`; nothing is retried by PLAT | the requester | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime" |
| low disk or disk full | § "Low disk" | `Platform.LowDisk` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Low-disk detection and behaviour" |
| an interrupted atomic write (crash, power loss) | the old file is intact; the stale temporary file is removed at the next start | `io.stale_temps_removed` | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes" |
| the resume write misses its deadline | cancelled; no torn file | the log | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence" |
| a resume file does not match or cannot be restored | discarded with the reason logged; a normal start | the log | `Zenith2/Docs/Design/PLAT/Android.md` → "Relaunch decision" |
| thermal samples unavailable | no adaptation | `platform.thermal_samples_invalid` | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal providers" |
| the accessibility OS backend cannot start, no voice, an invalid tree, a rejected OS action | no service, no built-in speech, the previous snapshot kept, the action dropped; never a platform failure | the log, `platform.accessibility.*` | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility failure handling" |
| cancellation of any PLAT operation | releases everything on its own thread before reporting | the operation's status | § "Cancellation" |
| a budget breach of Z2-B-PLAT-001 | a regression BUG per `Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule" blocks the stage acceptance until fixed or re-budgeted | the evidence record | § "Rows PLAT owns" |

### Controller loss

A disconnected gamepad's held controls return to rest at the removal timestamp before anything else sees the disconnection, so no action stays held; its haptics stop; its slot is reserved for the reconnection window; the game receives `Platform.DeviceDisconnected` with the last-of-kind flag at the next tick boundary and decides whether to pause; a reconnection inside the window restores the same device ID. Specified in `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug" and "Reconnection".

### Surface loss

On Android the surface is released inside the 2,000 ms handshake at the next frame boundary; on a timeout the surface is marked lost and every present reports out-of-date until the foreground recreation; the device, its resources and every world stay alive; the first pump with a native window recreates the surface. Windows minimisation produces no frames without any surface loss. Specified in `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake", "Surface recovery", "Windows equivalents".

### Low disk

A write that would leave less than the margin is refused before touching any file, a disk-full error mid-write deletes the temporary file, reads and deletes are never refused, the low-disk state raises one event per entry with hysteresis on clearing, and the game and UI observe it; the refused subsystem reports its own failure. Specified in `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Low-disk detection and behaviour".

### Corrupt configuration

Provenance-aware fallback: a corrupt content file contributes nothing and the dump lists it; an invalid value falls back per key to the next-lower layer with the rejection in its chain; a corrupt user file is quarantined with a timestamp (three kept), defaults apply and `Platform.ConfigReset` tells the game; a newer user file is never overwritten; every value's provenance names the source that actually supplied it. Specified in `Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling".

## Test strategy

### Test files

The unit tests of each split file are its tests table: `Zenith2/Docs/Design/PLAT/Platform.md` → "Platform core tests", `Zenith2/Docs/Design/PLAT/Input.md` → "Input tests", `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Filesystem tests", `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration tests", `Zenith2/Docs/Design/PLAT/Android.md` → "Android and lifecycle tests". The file list is § "Tests files". The integration suite `Zenith2/Tests/PlatformEvents/` adds:

| Test | Asserts |
|---|---|
| `Test_PlatformEvents_PublishedAtTickBoundary` | a world-visible event pumped in frame k is observed by a script in phase 9 of the first tick after the prologue of frame k, identically with 1, 2, 8 and all workers, and a replay of the run observes it at the same tick from the recorded payload |
| `Test_PlatformEvents_DeviceDisconnectedReachesScript` | a scripted disconnect reaches a behaviour as `Platform.DeviceDisconnected` with the last-of-kind flag, after the gameplay action released |
| `Test_PlatformEvents_LowDiskReachesScript` | a fake-volume refusal reaches a behaviour as `Platform.LowDisk` with the free bytes and margin |
| `Test_PlatformEvents_AccountChangedOnlineFirst` | the offline online-services backend observes the account change before any world's phase 9 does |

and ZenithPlatform's own `Zenith_PlatformCounters_Tests.cpp` holds `Test_PlatformCounters_Published`, which runs one pump, one I/O request, one configuration write, one thermal sample, one haptic submission and one file-watcher drain (over a test loose mount) through test hooks and asserts every counter of § "Counters" was published at least once with its table kind; it also fills each bounded queue of `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow" except the `FATAL` lifecycle queue past capacity once and asserts the triple the counting rule of that section gives its policy — a drop-policy queue `.dropped == 1` and `.full == 0`, an I/O priority queue `.full == .dropped == 1`, a completion queue `.full == 1` and `.dropped == 0` — with `.high_water` equal to the capacity, and asserts the lifecycle queue's `.full` and `.dropped` are 0.

### Test gates

| Gate step | Runs |
|---|---|
| `Build -Preset win-debug`, `win-release`, `android-arm64` | every PLAT source, the SDL mapping `static_assert`s, the Android and Windows platform files |
| `Test` (`win-debug`, `win-release`) | `ZenithPlatform_Tests`, PLAT's two tests in `ZenithRuntime_Tests`, the integration suites `Input`, `Lifecycle`, `PlatformEvents`, the compiled examples |
| `Sanitizers -Preset linux-tsan` (`Zenith2Memory`, change-triggered) | `ZenithPlatform_Tests` on the headless host, including `Test_AsyncIO_StressUnderTsan` and `Test_Platform_LinuxHostRunsPumpUnderTsan`, once the `Zenith2Memory` paths include the PLAT I/O paths (§ "Pending new rows"); every stage acceptance runs them regardless |
| `Sanitizers -Preset win-asan` | `ZenithPlatform_Tests` and the fuzz targets' 60-second smoke runs (`Zenith_Fuzz_VirtualPath`, `_BindingsJson`, `_InputScript`, `_InputCommandDecode`, `_MountManifest`, `_UpdateManifest`) |
| `CompileCoverage`, `SourceLayout`, `Libgraph`, `ForbiddenCheck`, `ApiContractCheck`, `HeaderSelfContained`, `TestSeparationCheck` | PLAT's layout, SDL3 confinement, contract blocks, header self-containment, no test switch |
| `ApkCheck` | ZenithPlatform's contribution to the game shared object (16 KB alignment) |
| stage acceptance (`Run-Committed.ps1 -Acceptance -Stage N`) | the full sanitizer set, full-duration fuzzing, and the device runs below |

### Device tests

Each is a `needs-human` run whose record is attached to its stage's acceptance ticket (`Zenith2/Docs/Roadmap.md` → "Acceptance evidence"):

| Device run | Stage | Evidence |
|---|---|---|
| S24 suspend and resume: 50 background and foreground cycles recover the surface every time with no validation error | S3 | run record with the validation log |
| S24 process death: a background kill relaunches into the resume state; a new build discards it | S3 | run record with the relaunch logs |
| S24 touch and gestures | S3 | run record per gesture |
| S24 and desktop gamepad hotplug and haptics | S3 | run record per controller |
| Windows and S24 IME and soft keyboard | S3 | run record per IME and purpose |
| Windows DPI: moving a window between monitors of 100% and 150% scaling recreates the swapchain and relayouts | S3 | run record with screenshots |
| S24 thermal: a 30-minute run shows resolution and tier steps following headroom | S5 | run record with exported counters |
| S24 low memory: a critical trim-memory signal reaches the governor | S6 | run record with the shedding counters |
| S24 HWASan: every ZenithPlatform test executable runs clean in the `android-arm64-hwasan` build | S2 onward, each stage acceptance | run record |
| S24 Play Asset Delivery: an on-demand pack mounts after installation | S6 | run record |
| sustained thermal qualification (WS-SUSTAIN) | S6 | MP-SUSTAIN evidence records |
| Windows Narrator and S24 TalkBack over the accessibility bridge's demo tree; SAPI and `TextToSpeech` rates and interruption; OS preference changes reaching the configuration panel | S4 | run records (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility device runs") |

### Sanitizer and fuzz runs

- The path-traversal fuzz corpus and its expectations are `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Path-traversal fuzz corpus"; the other fuzz targets start from the fixtures of `Tests/Fixtures/`.
- The `linux-tsan` stress of asynchronous I/O is `Zenith2/Docs/Design/PLAT/Filesystem.md` → "linux-tsan async I/O stress".
- `Zenith_Fuzz_UpdateManifest` starts from a corpus its setup step generates with the header-only `Tests/Support/Zenith_UpdateManifestTestWriter.h`, which signs in process with RFC 8032 § 7.1 TEST 2's seed; no signed manifest is committed (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API").
- Every factory of this family carries its `Test_<Type>_Create_FailsAtEachStep_Unwinds` fault sweep (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Partial initialisation failure").

## Budget rows

### Rows PLAT owns

| ID | Name | Status | Procedure | Platform | Workloads | Producer |
|---|---|---|---|---|---|---|
| Z2-B-PLAT-001 | Input phase CPU | pending | MP-CPUZONE on the `Input.Actions` node | Win, S24 | WS-FRAME | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-002 | Platform pump CPU | pending | MP-CPUZONE on the `Platform.Pump` zone | Win, S24 | WS-FRAME | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-003 | Async I/O critical read latency | pending | MP-COUNTER on `io.critical_latency_us`, behind a saturating background load | Win, S24 | WS-STREAM, W-Load | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-004 | Resume-state write duration | pending | MP-COUNTER, the resume write's operation time on backgrounding | S24 | WS-SLICE | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-005 | Configuration and bindings build time at startup | pending | MP-CPUZONE on startup step 6 | Win, S24 | W-Load | Stage 1 per-system budget ticket for PLAT |
| Z2-B-PLAT-006 | Platform and input memory | pending | MP-SUBMEM, attributed to `MEMORY_SUBSYSTEM_OTHER` | Win, S24 | WS-FRAME | Stage 1 per-system budget ticket for PLAT |

### Rows PLAT is measured against

| ID | Name | How PLAT contributes |
|---|---|---|
| Z2-B-RPROD-027 | Input-to-photon latency | input sampling (the pump) and the latency attribution of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Latency attribution" |
| Z2-B-SHIP-006 to Z2-B-SHIP-010 | Sustained floor, run duration, ambient temperature, starting battery, power state | thermal adaptation during MP-SUSTAIN; PLAT samples the thermal status |
| Z2-B-SHIP-011 | Dynamic-resolution floor | the thermal resolution cap never goes below it |
| Z2-B-SHIP-013, Z2-B-SHIP-014 | Cold boot Windows, cold boot S24 | mounting, configuration and bindings loading in startup steps 4 to 6 |
| Z2-B-CORE-001 | S24 process RSS | measured with the MP-MEM S24 sampler PLAT's harness builds |

### Pending new rows

<!-- elaboration -->
Rows this design proposed and `Zenith2/Docs/Budgets.md` did not hold. All five have since been added to `Zenith2/Docs/Budgets.md` as Z2-B-PLAT-002 to Z2-B-PLAT-006 (§ "Rows PLAT owns"), with the producer "Stage 1 per-system budget ticket for PLAT", so none is pending addition; the table keeps the proposal for reference. In later stages they are measured with the harnesses under `Zenith2/Benchmarks/Platform/`; their Stage 1 allocation takes the basis § "The Stage 1 PLAT harness" names:

| Proposed row | Unit | Procedure | Platform |
|---|---|---|---|
| Platform pump CPU | ms | MP-CPUZONE on the `Platform.Pump` zone | Win, S24 |
| Async I/O critical read latency (queue to completion, behind a saturating background load) | ms | MP-COUNTER on `io.critical_latency_us` | Win, S24 |
| Resume-state write duration | ms | the resume write's operation time on backgrounding | S24 |
| Configuration and bindings build time at startup | ms | MP-CPUZONE on startup step 6 | Win, S24 |
| Platform and input memory (event ring, transition log, binding table, mount tables, I/O queues) | MB | MP-SUBMEM, attributed to `MEMORY_SUBSYSTEM_OTHER` | Win, S24 |

Gate and configuration edits this design needs, each PROC `needs-human` work recorded in the same notes file: the development entry symbols in `Zenith2/Tools/Gates/shipping_forbidden_symbols.json` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion"); the `Zenith2Memory` category's paths gaining `Zenith2/Engine/ZenithPlatform/Internal/AsyncIO/**` and `Zenith2/Engine/ZenithPlatform/Internal/Zenith_VirtualFileSystem*.cpp` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "linux-tsan async I/O stress").

### The Stage 1 PLAT harness

The Stage 1 per-system budget harness for PLAT (`Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule", table "Per-system producer shape"; `Zenith2/Docs/Roadmap.md` → "Stage 1 Architecture validation") is an ordinary harness TASK that builds, under `Zenith2/Benchmarks/Budgets/PLAT/`:

1. the MP-MEM S24 sampler: the in-process 10 Hz `VmRSS` reader of `/proc/self/status`, the kgsl reader of `gpumem_mapped` and `gpumem_unmapped`, and the runner-side `adb shell dumpsys meminfo <pid>` sampler with its parse rule, writing the evidence record's `gfxSamples` and `gfxSampleCostNs` (`Zenith2/Docs/Budgets.md` → "Memory budgets");
2. its device test: a 256 MB device-local image committed by a clear, asserting the statistic rises by at least 230 MB while `VmRSS` alone rises by less than 32 MB, and that kgsl and `dumpsys` agree within 5% where both exist;
3. the reference-engine input-phase measurement for Z2-B-PLAT-001: the per-frame cost of `Zenith/Input`'s action layer at `C:/dev/Zenith/Zenith` on the reference desktop and the S24;

each with a CTest smoke run in the `Test` step. The `needs-human` ruling ticket "Stage 1 per-system budget ticket for PLAT", blocked by it and by the CORE, JOBS and RPROD rulings, runs the measurements, commits `Zenith2/Docs/Research/Budgets/Stage1/PLAT/allocation.json` and the reports, allocates Z2-B-PLAT-001 to Z2-B-PLAT-006 (the pump row and the startup-build row from reference-engine measurements imported like the input-phase CSVs, the latency, resume-write and memory rows on a `derived` or `published` basis, because the Zenith2 code they measure does not exist in Stage 1), and appends its ruling to `Zenith2/Docs/DecisionLog.md` in its own diff (Z2-D-097). The Player's budget-run mode adopts the sampler in Stage 2.

## Stage plan

### Stage 1

- The Stage 1 PLAT budget harness and its ruling (§ "The Stage 1 PLAT harness").
- PLAT consumes, and is blocked by, D-SEL-JSON (open, Stage 1) for every JSON reader and writer, D-SEL-HASH (open, Stage 1) for the account key hash and the resolution cache, and the Stage 1 security-architecture ticket for the traversal rules' confirmation and the Ed25519 admission.
- Pull-forward candidates into the Stage 1 sprint (fix version Stage 2): the `Zenith_VirtualPath` parser and its fuzz corpus (they depend on CORE's string view alone), and the `Zenith_Config` API-only header.

### Stage 2

The Stage 2 deliverable "Platform" and "the virtual filesystem, async I/O and configuration layering" (`Zenith2/Docs/Roadmap.md` → "Stage 2 Foundation"):

1. API-only slices first: the public headers of § "Public root headers" for the platform core, the filesystem, asynchronous I/O and configuration, with contract blocks and no implementation, so ASSET, STREAM, SAVE, RHI and MOD tickets become claimable.
2. The library target, SDL3 integration on Windows, Android and the Linux host, the pump, platform events, windows, native handles and Vulkan surfaces, display metrics and DPI, the clock.
3. The virtual filesystem, mounts, the mount manifest, path validation with the fuzz corpus, the escape check, synchronous access, command-line host files.
4. Asynchronous I/O, priorities, chunking, cancellation, atomic writes, low-disk detection and the low-disk event.
5. Configuration layering with provenance, the dump, corrupt-file handling, PLAT's keys.
6. The Android lifecycle state machine and event watch, background pause, the low-memory relay, throttling; the surface-destruction verification ruling.
7. The file watcher; the development/Shipping exclusion; the thermal null provider (the monitor API with no adaptation).
8. The minimal game boots on Windows and the S24 with the platform (the Stage 2 acceptance item).

### Stage 3

The Stage 3 deliverable "input" of "A minimal editor, input, physics…" (`Zenith2/Docs/Roadmap.md` → "Stage 3 Playable authoring slice"):

1. Input devices, the transition log, controllers, hotplug and reconnection, touch and pen, gestures, text input and IME, haptics, multiple windows for the editor, the clipboard.
2. Actions, bindings, the bindings file, contexts, claims, the mapper, rebinding and user bindings.
3. Phase 1, commands and replay, scripted input, latency attribution, the Luau input surface, the runtime bridge.
4. Surface recovery and the handshake; process death and restore (with SAVE's Stage 3 save and load).
5. World-visible platform events in scripts (low disk, devices).
6. The loopback listener for the DAP server; the development services' Shipping exclusion evidence.
7. Device runs: suspend and resume, process death, gestures, gamepads and haptics, IME, DPI.

### Stage 4

Account changes: account signals from Windows sessions and the offline online-services backend, delivery to the adapter before gameplay (Z2-D-074; the adapter lands in Stage 4).

The platform accessibility service for UI's Stage 4 accessibility (`Zenith2/Docs/Design/PLAT/Accessibility.md`): the API slice first (pulled forward into the Stage 3 sprint, so UI's accessibility work can declare against it), then the core service with the recording implementation, the OS preferences in the platform layer, the Windows UI Automation bridge, the SAPI speech thread, the Android bridge (after MOD's Java class, row P20), the concurrency proof under `linux-tsan`, the harness and the Windows and S24 device runs.

### Stage 5

Thermal adaptation on the S24: the Android provider, the state machine, the resolution cap and tier steps, runtime quality-tier changes (per-platform quality tiers land in Stage 5 with RPROD); the S24 thermal device run.

### Stage 6

Thermal qualification: the sustained runs of WS-SUSTAIN with thermal adaptation active; the S24 memory-pressure run; patch-mount verification with its identity-stamp skip, and the PLAT primitives of PACK's update and delivery flows (the public verifier, the directory-source factory, `ReadFromSource`, the base-path override, the connectivity signal); asset-pack mounting itself is PACK's installer, proven by PACK's mount tests and the S24 Play Asset Delivery device record; the tamper-case security evidence. The update-manifest, file-identity, directory-source and `ReadFromSource` declarations are pulled into the Stage 2 sprint so PACK's Stage 5 API and implementation tickets can include them; the Java context lands in the Stage 2 sprint for AUDIO's Stage 3 focus bridge, the directory-grant reads in Stage 3 for DEVT's budget-run driver and MOD's migration fixtures, and input-script recording in Stage 4 for the playthrough recordings.

### Stage 7

XR input through OpenXR as a new device source (SCOPE's tickets, specified against the XR-ready model of `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"), with PLAT's two additive changes: a POSE-capable `Zenith_InputValue` and the platform bindings layer's `interactionProfile` key; console input and storage behind SDK access (SCOPE). No other PLAT contract changes are planned for either.

### API-only slices

| Slice | Stage | Unblocks |
|---|---|---|
| `Zenith_VirtualFileSystem.h`, `Zenith_AsyncIO.h` | 2 | ASSET, STREAM, SAVE, PACK |
| `Zenith_Config.h` | 2 | every library with keys, MOD |
| `Zenith_Platform.h`, `Zenith_Window.h` | 2 | RHI, MOD, DEVT |
| `Zenith_AppLifecycle.h`, `Zenith_ThermalMonitor.h` | 2 | MOD (the listener and target), RHI |
| `Zenith_InputSystem.h`, `Zenith_InputAction.h`, `Zenith_InputContext.h`, `Zenith_InputCommand.h` | 3 | UI, EDIT, SCRIPT bindings, SAVE's replay log, NET |
| `Zenith_Accessibility.h`, `Zenith_AccessibilityTestAccess.h` | 4 (Stage 3 sprint) | UI's provider, UI's accessibility tests, MOD's Java class (the contract) |
| `Zenith_UpdateManifest.h`, `Zenith_FileIdentity.h`, the directory-source factory, `ReadFromSource`, `GetIdentityStamp`, `m_pxUpdateStamps`, `m_ulPackageContentHash` | 6 (Stage 2 sprint) | PACK's API slices, applier, stamp file, installer and bridge; ZenithCook's `--package-verify` |

## Decisions

### Settled decisions governing PLAT

| ID | Governs |
|---|---|
| Z2-D-010 | the platforms, minSdk 34, 16 KB pages, the S24 as the one validated device |
| Z2-D-082 | SDL3 pinned and confined to `ZenithPlatform/Internal/` |
| Z2-D-053 | platform events and I/O results become simulation-visible only at tick boundaries in completion-ID order |
| Z2-D-072 | the XR-ready input model |
| Z2-D-019 | input and platform events enter simulation only as logged commands and publications; thermal and quality changes are presentation-only |
| Z2-D-073 | input commands shared with replays and the network loopback; headless execution |
| Z2-D-080 | phase 1 owns action state and input contexts |
| Z2-D-076 | one local player |
| Z2-D-075 | no mod mounts |
| Z2-D-046 | no mutable global state; the platform is passed by reference |
| Z2-D-045 | results, not exceptions; untrusted input never asserts |
| Z2-D-040 | operation handles and generational identities |
| Z2-D-038 | JSON authoring formats |
| Z2-D-048 | user and authoring file compatibility |
| Z2-D-012 | the budget rows PLAT is measured against |
| Z2-D-084, Z2-D-097 | the Stage 1 harness and ruling split; rulings appended in the ticket diff |

### Open decisions PLAT depends on

| Decision | Stage | PLAT's use |
|---|---|---|
| D-SEL-JSON | open, Stage 1 | the JSON reader (with line positions) and writer for every PLAT document |
| D-SEL-HASH | open, Stage 1 | `Zenith_Hash64` for account keys and the resolution cache |
| D-PHASES refinement | open, Stage 1 | phase 1's contract (pre-decided as Z2-D-080) |
| D-SCRIPT-TXN refinement | open, Stage 1 | the asynchronous publication rules PLAT's platform events use |
| D-SEL-CRASH | open, Stage 2 | the crash handler library ZenithPlatform will own |

### Selection decisions PLAT owns

PLAT owns no named selection decision of `Zenith2/Docs/Architecture.md` → "Named selection decisions".

### Rulings PLAT records in its own tickets

Findings that a PLAT ticket establishes by verification, appended to `Zenith2/Docs/DecisionLog.md` in that ticket's diff with the next free `Z2-D-NNN` (Z2-D-097):

| Ruling | Ticket stage | Question it settles |
|---|---|---|
| the surface-destruction callback | 2 | which callback of the pinned SDL3 release carries Android's `surfaceDestroyed`, and whether it precedes `WILL_ENTER_BACKGROUND` (`Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake") |
| the thermal cadence on the S24 | 5 | that `AThermal_getThermalHeadroom(10)` at 2 s gives valid samples on the S24 and the default thresholds, or the measured replacements written into the thermal keys' defaults |

## Requirement index

<!-- REQ-INDEX -->

Every requirement of `Zenith2/Docs/Requirements/PLAT.md` and `Zenith2/Docs/Requirements/PLAT/PackagingAndTooling.md`, and the design headings that specify it:

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-PLAT-001 | ZenithPlatform library target | S2 | `Zenith2/Docs/Design/PLAT.md` → "ZenithPlatform target"; `Zenith2/Docs/Design/PLAT.md` → "ZENITH_SUPPORTED matrix and SDL3 configuration" |
| Z2-R-PLAT-002 | SDL3 confined to ZenithPlatform Internal | S2 | `Zenith2/Docs/Design/PLAT.md` → "ZENITH_SUPPORTED matrix and SDL3 configuration"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Subsystem initialisation per mode" |
| Z2-R-PLAT-003 | Platform creation steps and unwinding | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" |
| Z2-R-PLAT-004 | Headless platform mode | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Subsystem initialisation per mode" |
| Z2-R-PLAT-005 | Platform pump in prologue step 1 | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm"; `Zenith2/Docs/Design/PLAT.md` → "The Platform Pump step" |
| Z2-R-PLAT-006 | Event translation table | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Event translation table"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent" |
| Z2-R-PLAT-007 | Bounded event ring with overflow policy | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Event queue and overflow" |
| Z2-R-PLAT-008 | World-visible platform events published at tick boundaries | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement" |
| Z2-R-PLAT-009 | Event timestamps on the monotonic clock | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "The monotonic clock"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" |
| Z2-R-PLAT-010 | Main window creation | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Window creation and destruction"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API" |
| Z2-R-PLAT-011 | Multiple windows | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Multiple windows"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Window creation and destruction" |
| Z2-R-PLAT-012 | Per-window event routing and focus | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Multiple windows"; `Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks" |
| Z2-R-PLAT-013 | Native handles Vulkan surfaces and window modes | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Native handles for the RHI"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Fullscreen and display modes"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API" |
| Z2-R-PLAT-014 | Display metrics in pixels points and dp | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Units points pixels and dp"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_Window API" |
| Z2-R-PLAT-015 | Display scale and safe-area changes | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Display scale changes"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Safe area" |
| Z2-R-PLAT-016 | Clipboard text validated | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Clipboard" |
| Z2-R-PLAT-017 | Headless linux-tsan host | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "The headless test host" |
| Z2-R-PLAT-018 | Test injection through backend hooks | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "The headless test host" |
| Z2-R-PLAT-019 | Input device registry | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Device kinds and IDs"; `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API" |
| Z2-R-PLAT-020 | Control path grammar | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Control paths" |
| Z2-R-PLAT-021 | Control catalogue | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Control catalogue" |
| Z2-R-PLAT-022 | Ordered transition log with bounded motion coalescing | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Transition log" |
| Z2-R-PLAT-023 | Keyboard and mouse behaviour | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Keyboard and mouse" |
| Z2-R-PLAT-024 | Virtual devices and injected input | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API" |
| Z2-R-PLAT-025 | Gamepads through the SDL3 gamepad API | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Gamepad API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gamepad types and glyph families" |
| Z2-R-PLAT-026 | Gamepad mapping database | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Mapping database" |
| Z2-R-PLAT-027 | Controller hotplug | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug" |
| Z2-R-PLAT-028 | Controller reconnection window | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Reconnection" |
| Z2-R-PLAT-029 | Controller loss surfaced to the game | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Hotplug"; `Zenith2/Docs/Design/PLAT.md` → "Controller loss" |
| Z2-R-PLAT-030 | Haptic effects and mixing | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic mixing" |
| Z2-R-PLAT-031 | Haptic intensity and stopping | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic mixing" |
| Z2-R-PLAT-032 | Haptics from simulation are transactional commands | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Haptics from simulation" |
| Z2-R-PLAT-033 | Touch points | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Touch points" |
| Z2-R-PLAT-034 | No synthetic pointer events and pen input | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Mouse and touch emulation"; `Zenith2/Docs/Design/PLAT/Input.md` → "Touch and pen" |
| Z2-R-PLAT-035 | Tap double-tap and long-press recognisers | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Tap state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Double tap state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Long press state table" |
| Z2-R-PLAT-036 | Pan and swipe recognisers | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Pan state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Swipe classification" |
| Z2-R-PLAT-037 | Two-finger recognisers and arbitration | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Pinch and rotate state table"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture arbitration" |
| Z2-R-PLAT-038 | Gesture thresholds and bindable gestures | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture thresholds"; `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture recogniser API" |
| Z2-R-PLAT-039 | S24 touch and gesture device run | S3 | `Zenith2/Docs/Design/PLAT.md` → "Device tests"; `Zenith2/Docs/Design/PLAT/Input.md` → "Input tests" |
| Z2-R-PLAT-040 | Text input sessions and the soft keyboard | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Text input API"; `Zenith2/Docs/Design/PLAT/Input.md` → "Soft keyboard on Android" |
| Z2-R-PLAT-041 | IME composition and commit events | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Composition events" |
| Z2-R-PLAT-042 | Text entry consumes keyboard controls | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Text entry consumes keyboard controls"; `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"; `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm" |
| Z2-R-PLAT-043 | IME and soft keyboard device run | S3 | `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-044 | Action types and conversions | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Action types" |
| Z2-R-PLAT-045 | Stable action IDs and action state | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Action IDs"; `Zenith2/Docs/Design/PLAT/Input.md` → "Action state" |
| Z2-R-PLAT-046 | Composites and modifiers | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Composites"; `Zenith2/Docs/Design/PLAT/Input.md` → "Modifiers" |
| Z2-R-PLAT-047 | Processors | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Processors" |
| Z2-R-PLAT-048 | Interactions | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Interactions" |
| Z2-R-PLAT-049 | Control schemes and binding display | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Control schemes"; `Zenith2/Docs/Design/PLAT/Input.md` → "Binding display" |
| Z2-R-PLAT-050 | XR-ready input model | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" |
| Z2-R-PLAT-051 | Bindings file schema and validation | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"; `Zenith2/Docs/Design/PLAT/Input.md` → "Validation rules" |
| Z2-R-PLAT-052 | Binding layers and provenance | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Binding layers and provenance"; `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputSystem API" |
| Z2-R-PLAT-053 | Engine actions and contexts | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Engine actions and contexts" |
| Z2-R-PLAT-054 | Input context stacks | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Context stacks" |
| Z2-R-PLAT-055 | Consumption rules | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Consumption rules" |
| Z2-R-PLAT-056 | Presentation claims | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Presentation claims" |
| Z2-R-PLAT-057 | Deterministic action mapper | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Mapper algorithm"; `Zenith2/Docs/Design/PLAT/Input.md` → "Determinism of the mapper" |
| Z2-R-PLAT-058 | Interactive rebinding | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Rebind operation" |
| Z2-R-PLAT-059 | Rebinding conflict detection | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Conflict detection" |
| Z2-R-PLAT-060 | Persisted user bindings | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings" |
| Z2-R-PLAT-061 | Corrupt user bindings quarantined | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Persisting user bindings" |
| Z2-R-PLAT-062 | Reset bindings to defaults | S3 | `Zenith2/Docs/Design/PLAT/Input.md` → "Reset to defaults" |
| Z2-R-PLAT-063 | Input phase system per world | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The Input phase system" |
| Z2-R-PLAT-064 | Tick assignment of input | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events" |
| Z2-R-PLAT-065 | Quantised input commands | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| Z2-R-PLAT-066 | Simulation state rebuilt from commands | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| Z2-R-PLAT-067 | Replay record and scripted input | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Replay and scripted input"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format" |
| Z2-R-PLAT-068 | Input latency attribution | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Latency attribution" |
| Z2-R-PLAT-069 | Script-facing input API | S3 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" |
| Z2-R-PLAT-070 | Input phase CPU within budget | S3 | `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT owns" |
| Z2-R-PLAT-071 | Gamepad and haptics device run | S3 | `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-072 | Input debug overlay data | S3 | `Zenith2/Docs/Design/PLAT.md` → "Input debug overlay" |
| Z2-R-PLAT-073 | Virtual path type | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualPath"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Normalisation" |
| Z2-R-PLAT-074 | Path validation rules in order | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Reserved device names" |
| Z2-R-PLAT-075 | Path-traversal fuzz target and corpus | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Path-traversal fuzz corpus" |
| Z2-R-PLAT-076 | Escape check on the opened file | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Escape check after resolution" |
| Z2-R-PLAT-077 | Case-exact path matching | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Case policy" |
| Z2-R-PLAT-078 | Mount precedence | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm" |
| Z2-R-PLAT-079 | User mount isolation | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime" |
| Z2-R-PLAT-080 | Loose mount absent in Shipping | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount kinds and roots" |
| Z2-R-PLAT-081 | Mount sources and mount-table snapshots | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API" |
| Z2-R-PLAT-082 | A hostile higher mount stops resolution | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Resolution algorithm" |
| Z2-R-PLAT-083 | Mount manifest | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema" |
| Z2-R-PLAT-084 | Play Asset Delivery packs mounted after verification | S6 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API" |
| Z2-R-PLAT-085 | Patch mount accepts only verified patches | S6 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" |
| Z2-R-PLAT-086 | Synchronous file access rules | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Synchronous file access" |
| Z2-R-PLAT-087 | Command-line host files | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files" |
| Z2-R-PLAT-088 | Asynchronous I/O service | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_AsyncIO API"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Request lifetime" |
| Z2-R-PLAT-089 | Priority queues | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "I/O scheduler algorithm" |
| Z2-R-PLAT-090 | Chunked I/O with preemption | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Chunking and preemption" |
| Z2-R-PLAT-091 | I/O cancellation | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation" |
| Z2-R-PLAT-092 | I/O cancellation stress | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Cancellation" |
| Z2-R-PLAT-093 | I/O completion delivery | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Completion delivery" |
| Z2-R-PLAT-094 | Atomic writes | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Other user-mount operations" |
| Z2-R-PLAT-095 | Async I/O thread-sanitizer stress | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "linux-tsan async I/O stress" |
| Z2-R-PLAT-096 | Low-disk refusal before writes | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Free-space checks" |
| Z2-R-PLAT-097 | Disk-full during a write | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Free-space checks"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes" |
| Z2-R-PLAT-098 | Low-disk state machine | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Low-disk state machine" |
| Z2-R-PLAT-099 | Low-disk event raised to the game | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Surfacing to the game" |
| Z2-R-PLAT-100 | Low disk observable by scripts | S3 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Surfacing to the game"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement" |
| Z2-R-PLAT-101 | Six configuration layers | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Layer table"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Engine and game sub-sources"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Resolution algorithm" |
| Z2-R-PLAT-102 | Quality-tier two-pass resolution | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier resolution" |
| Z2-R-PLAT-103 | Configuration keys registered before the store | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Registration API"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys of libraries below ZenithPlatform" |
| Z2-R-PLAT-104 | Early keys from the command line only | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Early keys" |
| Z2-R-PLAT-105 | Invalid values rejected per key | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Resolution algorithm"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Rejection reasons" |
| Z2-R-PLAT-106 | Configuration provenance and chain | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Provenance chain"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Rejection reasons" |
| Z2-R-PLAT-107 | Configuration file rules | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration file schema"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Value encoding per type"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Command-line layer" |
| Z2-R-PLAT-108 | Replay runs use recorded simulation keys | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Simulation-affecting keys" |
| Z2-R-PLAT-109 | User-layer writes at the prologue | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "User-layer writes"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Restart-required keys" |
| Z2-R-PLAT-110 | Persisted user configuration | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Persisting the user layer" |
| Z2-R-PLAT-111 | Corrupt configuration fallback | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling" |
| Z2-R-PLAT-112 | Configuration dump | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump" |
| Z2-R-PLAT-113 | PLAT configuration keys | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers" |
| Z2-R-PLAT-114 | Configuration facade contract and unwinding | S2 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Zenith_Config API"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Ownership split with CORE"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Configuration error enumerations" |
| Z2-R-PLAT-115 | Runtime quality-tier changes | S5 | `Zenith2/Docs/Design/PLAT/Configuration.md` → "Quality tier changes" |
| Z2-R-PLAT-116 | Application lifecycle state machine | S2 | `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "The event watch" |
| Z2-R-PLAT-117 | Background pauses at a tick boundary | S2 | `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake" |
| Z2-R-PLAT-118 | Surface-loss handshake | S3 | `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake" |
| Z2-R-PLAT-119 | Surface recovery on foreground | S3 | `Zenith2/Docs/Design/PLAT/Android.md` → "Surface recovery" |
| Z2-R-PLAT-120 | Surface destruction callback verified | S2 | `Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"; `Zenith2/Docs/Design/PLAT.md` → "Rulings PLAT records in its own tickets" |
| Z2-R-PLAT-121 | S24 suspend and resume device run | S3 | `Zenith2/Docs/Design/PLAT.md` → "Device tests"; `Zenith2/Docs/Design/PLAT/Android.md` → "Android and lifecycle tests" |
| Z2-R-PLAT-122 | Throttling and termination | S2 | `Zenith2/Docs/Design/PLAT/Android.md` → "Windows equivalents"; `Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine" |
| Z2-R-PLAT-123 | Resume state written on background | S3 | `Zenith2/Docs/Design/PLAT/Android.md` → "Resume-state policy"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence" |
| Z2-R-PLAT-124 | Resume state restored on relaunch | S3 | `Zenith2/Docs/Design/PLAT/Android.md` → "Relaunch decision"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume identity"; `Zenith2/Docs/Design/PLAT/Android.md` → "Resume write sequence" |
| Z2-R-PLAT-125 | S24 process-death device run | S3 | `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-126 | Low-memory signal relayed | S2 | `Zenith2/Docs/Design/PLAT/Android.md` → "Low memory relay" |
| Z2-R-PLAT-127 | S24 memory-pressure device run | S6 | `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-128 | Thermal headroom sampling | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Zenith_ThermalMonitor API"; `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal providers" |
| Z2-R-PLAT-129 | Headroom drives dynamic resolution | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions" |
| Z2-R-PLAT-130 | Headroom drives the quality tier | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine"; `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions" |
| Z2-R-PLAT-131 | Thermal shutdown status | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal adaptation state machine" |
| Z2-R-PLAT-132 | Thermal adaptation touches presentation only | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions" |
| Z2-R-PLAT-133 | Thermal telemetry | S5 | `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal telemetry" |
| Z2-R-PLAT-134 | S24 thermal adaptation device run | S5 | `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-135 | Sustained thermal qualification | S6 | `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT is measured against"; `Zenith2/Docs/Design/PLAT/Android.md` → "Thermal telemetry" |
| Z2-R-PLAT-136 | Account signals | S4 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account signal sources"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery" |
| Z2-R-PLAT-137 | Account changes reach online services before gameplay | S4 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery"; `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement" |
| Z2-R-PLAT-138 | No raw account identifiers | S4 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account event delivery" |
| Z2-R-PLAT-139 | Loopback development listener | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket" |
| Z2-R-PLAT-140 | Development file watcher | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "File watcher" |
| Z2-R-PLAT-141 | Development services absent from Shipping | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Shipping exclusion" |
| Z2-R-PLAT-142 | Process watchdog | S2 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Process watchdog" |
| Z2-R-PLAT-143 | Stage 1 PLAT budget harness | S1 | `Zenith2/Docs/Design/PLAT.md` → "The Stage 1 PLAT harness" |
| Z2-R-PLAT-144 | Platform counters and log categories | S2 | `Zenith2/Docs/Design/PLAT.md` → "Counters"; `Zenith2/Docs/Design/PLAT.md` → "Log categories" |
| Z2-R-PLAT-145 | Resolution cache never changes a result | S2 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API" |
| Z2-R-PLAT-146 | XR pose values in action state | S7 | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation" |
| Z2-R-PLAT-147 | XR interaction profiles in the platform bindings layer | S7 | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"; `Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema" |
| Z2-R-PLAT-148 | Stage 2 measurement of the PLAT rows 002 to 006 | S2 | `Zenith2/Docs/Design/PLAT.md` → "Rows PLAT owns"; `Zenith2/Docs/Design/PLAT.md` → "Benchmarks and content files" |
| Z2-R-PLAT-149 | Accessibility service and its backend selection | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityService"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Backend table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Service creation and destruction"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" |
| Z2-R-PLAT-150 | Tree provider called only on the main thread | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityTreeProvider"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Ownership split with UI"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Publication algorithm" |
| Z2-R-PLAT-151 | Tree validation and the published snapshot | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Tree validation rules"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Publication algorithm"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Change detection" |
| Z2-R-PLAT-152 | OS queries served from the snapshot | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Snapshot queries"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Hit testing"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The accessibility lock" |
| Z2-R-PLAT-153 | Screen-reader detection | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Screen-reader detection"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Pump algorithm" |
| Z2-R-PLAT-154 | Windows UI Automation fragment provider | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Windows provider lifetime"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Role to control type table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Properties"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Patterns and actions"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Events raised" |
| Z2-R-PLAT-155 | Android node provider through JNI | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Virtual view IDs"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Java bridge contract"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Native entry points"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Node information mapping"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android events"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android bridge lifetime" |
| Z2-R-PLAT-156 | Announcements to the screen reader | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Announcements" |
| Z2-R-PLAT-157 | Built-in text to speech | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Built-in text to speech"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Windows SAPI speech thread"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Android TextToSpeech"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Rate mapping" |
| Z2-R-PLAT-158 | Focus speech hold | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Focus speech hold" |
| Z2-R-PLAT-159 | OS-requested actions | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Action mapping table"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Action queue" |
| Z2-R-PLAT-160 | OS accessibility preferences read | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "OS preference sources" |
| Z2-R-PLAT-161 | OS preferences in the platform configuration layer | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "OS accessibility preference changes"; `Zenith2/Docs/Design/PLAT/Configuration.md` → "Keys PLAT registers" |
| Z2-R-PLAT-162 | Recording implementation on every test host | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The recording implementation"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Record log"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Recording behaviour per member" |
| Z2-R-PLAT-163 | Accessibility test access | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Zenith_AccessibilityTestAccess API" |
| Z2-R-PLAT-164 | Accessibility lifetime and shutdown | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility shutdown order"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility thread affinity"; `Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown" |
| Z2-R-PLAT-165 | Accessibility diagnostics and failure handling | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility counters"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility log lines"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility failure handling" |
| Z2-R-PLAT-166 | Accessibility concurrency under the thread sanitizer | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility tests"; `Zenith2/Docs/Design/PLAT/Accessibility.md` → "The accessibility lock" |
| Z2-R-PLAT-167 | Accessibility device runs | S4 | `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Accessibility device runs"; `Zenith2/Docs/Design/PLAT.md` → "Device tests" |
| Z2-R-PLAT-168 | Public update-manifest verifier and its three halves | S6 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Update manifest API" |
| Z2-R-PLAT-169 | File identity stamps and the patch mount's stamp skip | S6 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "File identity stamps"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Patch mount verification" |
| Z2-R-PLAT-170 | Directory-source factory and reads from an unmounted source | S6 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount sources"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Reads from an unmounted source"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Zenith_VirtualFileSystem API" |
| Z2-R-PLAT-171 | Base-path override for test install trees | S6 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Creation and the desc" |
| Z2-R-PLAT-172 | Android Java context for libraries above the platform | S3 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Android Java context" |
| Z2-R-PLAT-173 | Presentation-only connectivity signal | S6 | `Zenith2/Docs/Design/PLAT/Platform.md` → "Connectivity" |
| Z2-R-PLAT-174 | Directory-grant reads | S3 | `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Directory-grant reads" |
| Z2-R-PLAT-175 | Input-script ticks across world swaps and exact command bits | S4 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "The input-script format" |
| Z2-R-PLAT-176 | Input-script recording | S4 | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input-script recording" |
