# Architecture — Platform layer

This file is part of the Architecture document family. The entry document is `Zenith2/Docs/Architecture.md`; this file holds the full specification of its section "Platform layer". Normative statements carry `[Z2-N-ARCH-NNN]` tags in the range 911–949. The PLAT design document (`Zenith2/Docs/Design/PLAT.md`) specifies input action mapping, rebinding and gestures on top of this layer.

## Settled rules and ownership

Platforms are Windows and Android arm64, validated on the Galaxy S24 Ultra alone, minSdk 34, 16 KB pages (Z2-D-010). SDL3 is the window, input, Android lifecycle and filesystem layer (Architecture.md → "Third-party dependencies"). `ZenithPlatform` owns SDL3 and every OS-facing service below; SDL3 headers stay inside `ZenithPlatform/Internal/` (rule bullet 911 below). Platform code lives in `Internal/Windows/`, `Internal/Android/` and `Internal/Linux/` (the headless test host), not behind a platform `#ifdef` in shared code (Zenith2/Docs/Contracts.md → "No layout-changing conditionals").

| Public header (`Engine/ZenithPlatform/`) | Declares |
|---|---|
| `Zenith_Platform.h` | `Zenith_Platform` (creation, the event pump, service accessors), `Zenith_PlatformDesc`, `Zenith_PlatformEvent` |
| `Zenith_Window.h` | `Zenith_Window`, native-handle access for the RHI (`Zenith_NativeWindowHandle`), display scale |
| `Zenith_InputTypes.h` | device kinds and IDs, controls and their enumerations, `Zenith_InputError` |
| `Zenith_InputDevices.h` | raw device state and events: keyboard, mouse, gamepads, touch, pen, text input and IME composition, haptics |
| `Zenith_Gestures.h` | gesture kinds, states, configuration, events and recognisers |
| `Zenith_InputAction.h` | input actions, values, phases and the action-state table |
| `Zenith_InputBindings.h` | binding layers, control schemes, control paths, bindings and the bindings file |
| `Zenith_InputContext.h` | input contexts and the context stack |
| `Zenith_ActionMapper.h` | `Zenith_ActionMapper`: devices to actions |
| `Zenith_InputSystem.h` | `Zenith_InputSystem`: the per-frame input frame, presentation claims, rebinding |
| `Zenith_InputCommand.h` | `Zenith_InputCommand` and its quantised encoding (the replayable input commands of Architecture.md → "Multiplayer foundations") |
| `Zenith_AppLifecycle.h` | `Zenith_AppLifecycleState`, lifecycle events, the surface-loss handshake |
| `Zenith_VirtualFileSystem.h` | `Zenith_VirtualPath`, `Zenith_VirtualFileSystem`, mounts |
| `Zenith_AsyncIO.h` | `Zenith_AsyncIO`, `Zenith_IORequest`, `Zenith_IOPriority` |
| `Zenith_HostFile.h` | `Zenith_OpenCommandLineHostFile`, `Zenith_CreateFileInCommandLineHostDirectory`, `Zenith_HostFile`, `Zenith_CommandLineFileGrant`: the command-line host files and the two host directories of bullet 923 (the writable evidence directory and the read-only migration-fixture directory) |
| `Zenith_Config.h` | `Zenith_Config`, the facade over ZenithCore's `Zenith_ConfigStore` that applies the configuration sources (§ "Configuration layering") |
| `Zenith_ThermalMonitor.h` | `Zenith_ThermalMonitor`, `Zenith_ThermalSample` |
| `Zenith_Accessibility.h` | `Zenith_AccessibilityService`, the `Zenith_AccessibilityTreeProvider` interface UI implements, and the PLAT-owned accessibility node, tree, action and preference values (`Zenith2/Docs/Design/PLAT/Accessibility.md`) |
| `Zenith_AccessibilityTestAccess.h` | test access to the recording accessibility backend |
| `Zenith_DevSocket.h` | the loopback development socket (non-Shipping builds) |
| `Zenith_FileWatcher.h` | the loose-mount file watcher (non-Shipping builds) |

Every declaration of every header is listed in `Zenith2/Docs/Design/PLAT.md` → "Library and file layout".

The monotonic and wall clocks (`Zenith_Clock.h`: `Zenith_GetMonotonicTimeUs`, `Zenith_GetMonotonicTimeNs`, `Zenith_GetUtcTime`) are ZenithCore's, below this layer, because ZenithProfiling and ZenithJobs read them (`Zenith2/Docs/Design/CORE.md`; Architecture/CPUFrameGraph.md → "Settled rules and ownership"); ZenithPlatform declares no clock.

- [Z2-N-ARCH-911] Every call into SDL3 is made from `ZenithPlatform/Internal/`; no other library includes an SDL3 header or calls an SDL3 function.
- [Z2-N-ARCH-912] The SDL event pump runs only on the main thread, in step 1 (`Platform.Pump`) of the frame prologue (Architecture/CPUFrameGraph.md → "Frame structure"), and translates SDL events into `Zenith_PlatformEvent` records queued for the Input phase; SDL events never reach other libraries directly.

## SDL3 responsibilities

| Responsibility | SDL3 facility | Zenith2 surface |
|---|---|---|
| window creation, resize, fullscreen, multiple windows (editor) | `SDL_CreateWindow`, window events | `Zenith_Window` |
| Vulkan surface; D3D12 window handle | `SDL_Vulkan_CreateSurface`; window properties (`SDL_PROP_WINDOW_WIN32_HWND_POINTER`) | `Zenith_NativeWindowHandle` for `ZenithRHI` |
| keyboard, mouse, relative mouse | keyboard and mouse events | `Zenith_InputDevices` |
| gamepads, hotplug, haptics and rumble | `SDL_EVENT_GAMEPAD_ADDED/REMOVED`, `SDL_RumbleGamepad`, the haptic API | `Zenith_InputDevices` |
| touch, pen | touch and pen events | `Zenith_InputDevices` |
| text input and IME | `SDL_StartTextInput`, `SDL_EVENT_TEXT_EDITING`, `SDL_EVENT_TEXT_INPUT` | composition and commit events |
| display scale (DPI) | `SDL_GetWindowDisplayScale`, `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED` | `Zenith_Window::GetDisplayScale` |
| Android lifecycle | `SDL_EVENT_WILL_ENTER_BACKGROUND`, `DID_ENTER_BACKGROUND`, `WILL_ENTER_FOREGROUND`, `DID_ENTER_FOREGROUND`, `LOW_MEMORY`, `TERMINATING`, via an event watch | `Zenith_AppLifecycle` |
| filesystem locations | `SDL_GetBasePath`, `SDL_GetPrefPath`; Android asset I/O | mount roots of the virtual filesystem |
| clipboard | `SDL_SetClipboardText`, `SDL_GetClipboardText` | editor and UI |

- [Z2-N-ARCH-913] Threads, mutexes, atomics and timers come from `Zenith_Multithreading` and `Zenith_Clock`, never from SDL3's thread or timer APIs, so the lock-ordering and concurrency contracts cover every thread (Zenith2/Docs/Contracts.md → "Threading and concurrency contract").

## Android lifecycle and surface recovery

SDL3 delivers lifecycle events synchronously on the Android UI thread through an event watch; the watch callback runs in `ZenithPlatform/Internal/Android/`, not in engine code, and hands each event to the main thread.

### Lifecycle state machine

| State | `SDL_EVENT_WILL_ENTER_BACKGROUND` | `SDL_EVENT_DID_ENTER_BACKGROUND` | `SDL_EVENT_WILL_ENTER_FOREGROUND` | `SDL_EVENT_DID_ENTER_FOREGROUND` | `SDL_EVENT_TERMINATING` |
|---|---|---|---|---|---|
| `APP_STATE_RUNNING` | → `APP_STATE_SUSPENDING`: request pause; handshake surface release | — | ignored | ignored | → `APP_STATE_TERMINATING` |
| `APP_STATE_SUSPENDING` | ignored | → `APP_STATE_BACKGROUND`: write the resume state; stop audio | → `APP_STATE_RESUMING` (the suspend was cancelled) | — | → `APP_STATE_TERMINATING` |
| `APP_STATE_BACKGROUND` | ignored | ignored | → `APP_STATE_RESUMING`: recreate the surface on the next pump | — | → `APP_STATE_TERMINATING` |
| `APP_STATE_RESUMING` | → `APP_STATE_SUSPENDING` | — | ignored | → `APP_STATE_RUNNING`: recreate the swapchain, resume audio, unpause | → `APP_STATE_TERMINATING` |
| `APP_STATE_TERMINATING` | ignored | ignored | ignored | ignored | ignored: shutdown in progress |

The surface-loss handshake: on `WILL_ENTER_BACKGROUND` the watch sets the pause request and waits on a `Zenith_Semaphore`, at most 2,000 ms, for the main thread to acknowledge that the render subgraph has finished, the swapchain has been destroyed and no GPU work references the `ANativeWindow`. The main thread acknowledges at its next frame boundary.

- [Z2-N-ARCH-914] On entering the background the runtime pauses every world at a tick boundary (the simulation clock returns zero ticks), completes the in-flight render subgraph, destroys the swapchain and releases the surface before the watch callback returns; the RHI device, its resources and every world stay alive.
- [Z2-N-ARCH-939] The application lifecycle follows exactly the table "Lifecycle state machine": every state and event cell produces the transition and action listed, and an event not listed for a state is ignored.
- [Z2-N-ARCH-915] On returning to the foreground the runtime recreates the surface and swapchain from the new `ANativeWindow`, rebuilds size-dependent render targets through the render graph, resumes audio and unpauses; a swapchain recreation failure follows the device recovery policy of Zenith2/Docs/Rendering.md → "Recovery".
- [Z2-N-ARCH-916] The surface-loss handshake waits at most 2,000 ms; when the main thread has not acknowledged in time, the watch returns, the platform marks the surface lost, and the RHI treats every subsequent present as `SWAPCHAIN_OUT_OF_DATE` until the foreground recreation, so no GPU call ever uses a destroyed window.

### Process death and restore

Android may kill a backgrounded process. The runtime therefore writes a resumable state on every entry to the background and restores it on the next launch.

1. On `DID_ENTER_BACKGROUND`, for each world, run the save-snapshot serialization (phase 11 of the frame phases) on the paused world state, outside a tick, into `user:/saves/resume.zsave` through a journaled atomic write (temporary file, flush, rename), within a deadline of 3,000 ms.
2. The file records the build ID, the composition hash, the cooked-content hash and the tick index.
3. On launch, when `user:/saves/resume.zsave` exists and all three recorded identities match the running game, restore the worlds from it before the first tick and delete the file after the first tick completes; otherwise delete it and log `resume state discarded: <reason>`.
4. On `DID_ENTER_FOREGROUND` of a process that was not killed, delete the file.

- [Z2-N-ARCH-917] A process killed while backgrounded relaunches into the state saved at its last background entry when the build, composition and content identities match, and into a normal start otherwise.
- [Z2-N-ARCH-934] The relaunch choice (restored or discarded, with the reason) is logged.
- [Z2-N-ARCH-935] The configuration key `platform.resumeOnRelaunch = false` disables writing and restoring the resume state for a game.
- [Z2-N-ARCH-918] The resume write that misses its 3,000 ms deadline is abandoned and the partial temporary file deleted, so a relaunch never reads a torn resume state.

## Thermal adaptation

`Zenith_ThermalMonitor` (Android implementation in `Internal/Android/`, a null implementation on Windows and Linux) samples `AThermal_getThermalHeadroom(10)` every 2 s and listens with `AThermal_registerThermalStatusListener`. Each sample is published as the counter `platform.thermal_headroom` and, at the frame prologue, drives the dynamic-resolution controller and the quality tier.

<!-- elaboration -->
| Condition | Action | Configuration key (default) |
|---|---|---|
| headroom ≥ 0.85 | lower the dynamic-resolution target by one step (5% of output, not below the budget's resolution floor) | `thermal.drsHeadroom` (0.85) |
| headroom ≥ 0.95, or status `ATHERMAL_STATUS_SEVERE` or worse | drop the quality tier by one level | `thermal.tierHeadroom` (0.95) |
| headroom ≤ 0.70 continuously for 30 s | undo one step, dynamic resolution before tier | `thermal.recoverHeadroom` (0.70), `thermal.recoverSeconds` (30) |
| `ATHERMAL_STATUS_SHUTDOWN` | request a checkpoint save, then continue at the lowest tier | — |

- [Z2-N-ARCH-919] Thermal adaptation changes presentation only (resolution, quality tier), never simulation state, tick rate or anything a replay records, so it cannot break the determinism guarantee.
- [Z2-N-ARCH-938] The thermal monitor applies the actions of the table "Thermal adaptation" at the frame prologue, with the thresholds read from the configuration keys that table names.

## Virtual filesystem

### Mounts and precedence

| Mount | Precedence (higher wins) | Writable | Source on Windows | Source on Android | Present in |
|---|---|---|---|---|---|
| `package` | 1 | no | `<install>/Content/*.zpak` | APK assets, Play Asset Delivery packs | every build |
| `patch` | 2 | no | `<install>/Patches/<version>/*.zpak`, verified manifest | the patch directory in app storage, verified manifest | every build |
| `loose` | 3 | no | `--assets-root <dir>` or `${ZENITH_OUT}/Cooked/<Game>/windows/` | `adb push` directory | non-Shipping builds only |
| `user` | 4 | yes | `SDL_GetPrefPath("Zenith2", <Game>)` | the app's internal storage | every build |

The `.zpak` archives of the package and patch mounts (and install-time asset packs) are read by ZenithAssets' archive reader, which ZenithPlatform reaches through the factory pointer and user data the composition root places in `Zenith_VirtualFileSystemDesc` at startup step 5 (`&Zenith_CreateArchiveMountSource`, a `Zenith_ArchiveReaderDesc`; Architecture/CompositionRoot.md → "Startup sequence"), so ZenithPlatform (layer 2) names no ZenithAssets type; PACK writes the archives.

Virtual paths have the form `<root>:/<relative>`, with two roots: `content:/` resolves through `loose`, then `patch`, then `package` (highest precedence first); `user:/` resolves in `user` alone, restricted to the top-level directories `config/`, `saves/`, `cache/` and `logs/`.

- [Z2-N-ARCH-920] A content read resolves to the highest-precedence mount that contains the path, and the resolution result (mount and archive) is reported by `Zenith_VirtualFileSystem::Resolve` for diagnostics; the user mount never shadows content, so a file in the user directory can never replace a shipped asset or script.
- [Z2-N-ARCH-921] Every write goes to the `user` mount and nowhere else, with one exception: before the virtual filesystem exists, in the closing action of startup step 4, PACK's update applier writes under `<install>/Patches/`, `<install>/DLC/` and `<install>/.zenith2-install/` (Windows) and the internal storage's `Patches/` and `.zenith2-install/` (Android), through ZenithPackage's install file system ([Z2-N-ARCH-923]), and no other PACK runtime code writes outside the `user` mount; `content:/` paths are read-only in every build.
- [Z2-N-ARCH-922] The `loose` mount does not exist in Shipping builds (Architecture.md → "Security architecture").

### Path validation

```cpp
// Engine/ZenithPlatform/Zenith_VirtualFileSystem.h
enum Zenith_PathErrorCode : uint8
{
	PATH_ERROR_BAD_ROOT = 1, PATH_ERROR_ENCODING, PATH_ERROR_SEPARATOR, PATH_ERROR_TRAVERSAL, PATH_ERROR_ABSOLUTE,
	PATH_ERROR_RESERVED_NAME, PATH_ERROR_TOO_LONG, PATH_ERROR_USER_NAMESPACE, PATH_ERROR_ESCAPES_MOUNT, PATH_ERROR_NOT_FOUND,
	// the PLAT design's enumerators, after these (Zenith2/Docs/Design/PLAT/Filesystem.md → "Filesystem error enumerations"):
	PATH_ERROR_FORBIDDEN_CHARACTER, PATH_ERROR_CASE_MISMATCH, PATH_ERROR_SOURCE_FAILED, PATH_ERROR_NAME_MISMATCH,
};

struct Zenith_PathError
{
	Zenith_PathErrorCode m_eCode;
	uint32 m_uByteOffset;      // the offending byte or component start in the input path
};

Zenith_Result<Zenith_VirtualPath, Zenith_PathError> Zenith_ParseVirtualPath(Zenith_StringView strPath);
```

`Zenith_ParseVirtualPath` accepts a path when all of these hold, and rejects it otherwise:

| Rule | Rejected with |
|---|---|
| begins with `content:/` or `user:/` | `PATH_ERROR_BAD_ROOT` |
| is valid UTF-8 with no NUL and no control character | `PATH_ERROR_ENCODING` |
| uses `/` separators only (no `\`) | `PATH_ERROR_SEPARATOR` |
| has no empty, `.` or `..` component | `PATH_ERROR_TRAVERSAL` |
| has no `:` after the root, no drive letter, no leading `/` after the root | `PATH_ERROR_ABSOLUTE` |
| has no component that is a Windows reserved device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, `LPT1`–`LPT9`, with any extension) or ends in `.` or space | `PATH_ERROR_RESERVED_NAME` |
| total length at most 512 bytes; each component at most 255 bytes | `PATH_ERROR_TOO_LONG` |
| `user:/` paths start with an allowed top-level directory | `PATH_ERROR_USER_NAMESPACE` |

- [Z2-N-ARCH-923] Every file access in Zenith2 runtime code goes through a `Zenith_VirtualPath` produced by `Zenith_ParseVirtualPath`, whose rules above reject path traversal, with one exception: in non-Shipping builds the host files named on the command line (`--config-dump`, `--record`, `--replay`, `--input-script`) are opened only by `Zenith_OpenCommandLineHostFile` under a `Zenith_CommandLineFileGrant`, the host directory `--evidence-dir` (a grant whose `IsDirectory()` is true) receives files only through `Zenith_CreateFileInCommandLineHostDirectory` with a single-component file name that does not exist yet and admits one read-open of each of the two named budget-run inputs `rows.json` and `fingerprint.json` through `Zenith_OpenFileInCommandLineHostDirectory`, which refuses any name holding a path separator (Zenith2/Docs/Design/DEVT/Evidence.md), and the second, read-only host directory `--script-migration-fixtures` (`COMMAND_LINE_FILE_SCRIPT_MIGRATION_FIXTURES`, a directory grant whose files are listed and read through ZenithPlatform's directory-grant read functions by relative paths without `..`, `Zenith2/Docs/Design/PLAT.md`) is never written; every grant is constructed by `Zenith_CommandLine` alone, so no path from content, saves, configuration, scripts or the network reaches any of these functions; `--assets-root` becomes the loose mount root in non-Shipping builds only. Two further named exceptions belong to ZenithPackage (`Zenith2/Docs/Design/PACK/Algorithms.md` → "Pending installs at startup", "Mounting and unmounting"): its install file system, which from the closing action of startup step 4 until shutdown step 10b opens host paths only under `<install>` (Windows) or the internal storage root (Android) — `Patches/`, `DLC/`, `.zenith2-install/` and, read-only, `Content/mount-manifest.json` — confined to that root by the escape check on every open and never following a link; and the read-only directory sources ZenithPackage creates through `Zenith_CreateDirectoryMountSource` over Play's `AssetPackLocation.assetsPath()` and, in non-Shipping builds only, over the simulated delivery's source root (`package.simulatedDelivery.root`).
- [Z2-N-ARCH-936] An OS path is formed only inside `ZenithPlatform/Internal/`, where the resolved OS path is checked to lie inside its mount root after symbolic-link resolution, and a check failure is `PATH_ERROR_ESCAPES_MOUNT`.
- [Z2-N-ARCH-924] A path error is a recoverable `Zenith_Result` error, never an assert, because paths come from content, saves, configuration and scripts.

## Asynchronous I/O

```cpp
// Engine/ZenithPlatform/Zenith_AsyncIO.h
enum Zenith_IOErrorCode : uint8
{
	IO_ERROR_QUEUE_FULL = 1, IO_ERROR_LOW_DISK, IO_ERROR_NOT_FOUND, IO_ERROR_ACCESS, IO_ERROR_READ, IO_ERROR_WRITE,
	IO_ERROR_READ_ONLY_MOUNT,    // a write to a content:/ path
	IO_ERROR_PATH,               // the path failed validation; m_xPath carries the detail
	// the PLAT design's enumerators, after these:
	IO_ERROR_DISK_FULL, IO_ERROR_SHORT_READ, IO_ERROR_DESTINATION_TOO_SMALL, IO_ERROR_ARCHIVE_CORRUPT,
	IO_ERROR_INVALID_REQUEST, IO_ERROR_SHUTTING_DOWN, IO_ERROR_OUT_OF_MEMORY,
};

struct Zenith_IOError
{
	Zenith_IOErrorCode m_eCode;
	Zenith_PathError m_xPath;    // valid for IO_ERROR_PATH
	uint32 m_uOSError;          // GetLastError / errno, 0 when not applicable
};

enum Zenith_IOPriority : uint8
{
	IO_PRIORITY_CRITICAL,      // blocks visible progress now (the startup scene, a streamed cell in view)
	IO_PRIORITY_HIGH,
	IO_PRIORITY_NORMAL,
	IO_PRIORITY_LOW,
	IO_PRIORITY_BACKGROUND,    // prefetch
};

struct Zenith_IORequest
{
	Zenith_VirtualPath m_xPath;
	uint64 m_ulOffset = 0;
	uint64 m_ulBytes = 0;                  // 0 = to end of file
	Zenith_MutableByteSpan m_xDestination;  // owned by the requester until completion
	Zenith_IOPriority m_ePriority = IO_PRIORITY_NORMAL;
};

// Contract: ANY_THREAD. Owns its I/O threads (two per platform); joined by its destructor.
class Zenith_AsyncIO
{
public:
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> Read(const Zenith_IORequest& xRequest);
	Zenith_Result<Zenith_OperationHandle, Zenith_IOError> WriteAtomic(Zenith_VirtualPath xPath,
		Zenith_ByteSpan xBytes, Zenith_IOPriority ePriority);   // user mount only; journaled replace
};
```

| Queue | Capacity | Full policy |
|---|---|---|
| one request queue per priority | 1,024 (configuration `io.queueCapacity`) | `QUEUE_FULL_POLICY_REJECT`: `IO_ERROR_QUEUE_FULL` returned to the requester |

- [Z2-N-ARCH-925] I/O threads take the highest-priority non-empty queue first and requests within one priority in submission order.
- [Z2-N-ARCH-940] A request cancelled while queued is removed without any I/O.
- [Z2-N-ARCH-941] A request cancelled in flight completes its system call, discards the data, and reports `OPERATION_STATUS_CANCELLED` only after the destination buffer is no longer touched (Zenith2/Docs/Contracts.md → "Threading and concurrency contract").
- [Z2-N-ARCH-926] `WriteAtomic` writes a temporary file beside the target, flushes it, and renames it over the target, so an interrupted write leaves either the complete old file or the complete new file.

### Low-disk behaviour

- [Z2-N-ARCH-927] Before every write, `Zenith_AsyncIO` checks the user mount's free space (`GetDiskFreeSpaceExW`, `statvfs`); when free space minus the write size would fall below `io.lowDiskMarginBytes` (64 MiB by default), it refuses the write with `IO_ERROR_LOW_DISK` without touching the target.
- [Z2-N-ARCH-942] A refused low-disk write raises the platform event `PLATFORM_EVENT_LOW_DISK`, which the runtime delivers to the game at the next tick boundary as an engine event its scripts can observe.

## Configuration layering

Configuration keys are declared by the library that owns them and resolved from six layers, lowest to highest precedence:

| Layer | Source | Example |
|---|---|---|
| 1 engine default | the default in the key's registration | `jobs.workerCount = 0` |
| 2 platform | `content:/Config/Platform/<windows|android>.config.json`, then the OS accessibility preferences (`os:accessibility`) | `rhi.backend = "vulkan"`; `platform.accessibility.reducedMotion = true` |
| 3 game | `content:/Config/game.config.json` (from `Games/<Name>/Assets/Config/`) | `sim.tickHz = 30` |
| 4 quality tier | `content:/Config/Quality/<tier>.config.json`, engine file then game file | `render.shadowCascades = 3` |
| 5 user | `user:/config/user.config.json` | `audio.masterVolume = 0.6` |
| 6 command line | `--set <key>=<value>` | `--set rhi.backend=d3d12` |

The configuration value types, key descriptors, provenance, layers, errors, the key schema, the layered store with its resolution and provenance, the JSON layer parser and the dump are ZenithCore's (`Zenith2/Engine/ZenithCore/Zenith_ConfigStore.h`, declared by `Zenith2/Docs/Design/CORE.md`), so a library below ZenithPlatform (for example ZenithJobs's `jobs.workerCount`) registers its keys without depending on this layer. ZenithPlatform owns the sources — the layer files read through the virtual filesystem, the command line's `--set` values, and the user layer persisted to `user:/config/user.config.json` — and the facade:

```cpp
// Engine/ZenithPlatform/Zenith_Config.h
#include "ZenithCore/Zenith_ConfigStore.h"

// Contract: MAIN_THREAD. Built by the composition root at startup step 6 over a frozen Zenith_ConfigSchema; owns one
// Zenith_ConfigStore and applies layers 2 to 6 to it; IMMUTABLE afterwards except user-layer writes through
// SetUserValue, which the store stages and the frame prologue commits, and the OS accessibility sub-source of layer 2,
// which the frame prologue re-applies when the platform reports a change (it can set no simulation-affecting or
// restart-required key).
class Zenith_Config
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Config>, Zenith_ConfigSourceError> Create(const Zenith_ConfigDesc& xDesc);
	const Zenith_ConfigStore& GetStore() const;
	Zenith_Result<Zenith_ConfigKey, Zenith_ConfigError> FindKey(Zenith_StringView strKey) const;
	const Zenith_ConfigValue& Get(Zenith_ConfigKey xKey) const;
	Zenith_ConfigProvenance GetProvenance(Zenith_ConfigKey xKey) const;
	Zenith_Result<void, Zenith_ConfigError> SetUserValue(Zenith_ConfigKey xKey, const Zenith_ConfigValue& xValue);
	Zenith_Result<void, Zenith_JsonWriteError> WriteDump(Zenith_JsonWriter& xWriter) const;  // every key, value, provenance
};
```

- [Z2-N-ARCH-928] Every configuration value records its provenance (layer, source file and line).
- [Z2-N-ARCH-937] `--config-dump <path>` writes every key with its value and provenance, and the on-screen profiler's configuration panel shows the same data.
- [Z2-N-ARCH-929] A key set in a layer its registration does not allow, a value of the wrong type, or a value out of range is rejected with a logged warning naming the key, the source and the line, and the next-lower layer's value stays in effect; an unknown key is a logged warning and is otherwise ignored.
- [Z2-N-ARCH-943] The OS accessibility sub-source of layer 2 (`os:accessibility`) writes only PLAT's own keys `platform.accessibility.reducedMotion`, `platform.accessibility.highContrast` and `platform.accessibility.textScale` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer"), none of them simulation-affecting or restart-required, and the frame prologue re-applies it when the platform reports a change; it never writes another library's key ([Z2-N-ARCH-930]).
- [Z2-N-ARCH-930] Configuration keys are registered by the owning library in its registration function, never looked up by a library that did not register or declare a dependency on them, and a key that affects simulation (for example `sim.tickHz`) is written into every replay header so a replay runs with the recorded value.

## Account changes

- [Z2-N-ARCH-931] OS-level account signals (a Windows user switch, an Android profile or account change delivered by the online backend) are converted by the platform layer into `PLATFORM_EVENT_ACCOUNT_CHANGED` and delivered to the online-services adapter and the game at a tick boundary; the account consumer is called once per `PLATFORM_EVENT_ACCOUNT_CHANGED` on the main thread in prologue step 1, after `Platform.Pump` and before the frame listeners' `OnPlatformEvents`, in event order; rebinding saves and entitlements is specified by `Zenith2/Docs/Design/ONLINE/Accounts.md` → "Rebind sequence".

## The headless test host

- [Z2-N-ARCH-932] `ZenithPlatform/Internal/Linux/` implements the platform for the `linux-tsan` host with no window, no GPU and no audio device: the event pump returns no device events, the virtual filesystem mounts a loose directory and a temporary user directory, and the clock and threads are ZenithCore's POSIX implementations.

## Required tests

The table "Platform tests" lists the tests of this section; they live in `Engine/ZenithPlatform/Tests/`, except the device rows, which are `needs-human` S24 acceptance runs.

- [Z2-N-ARCH-933] Every test in the table "Platform tests" exists under the name given and asserts the case given.

Platform tests:

| Test | Asserts |
|---|---|
| `Test_VirtualPath_EachRuleRejects` | one input per validation rule is rejected with its error code; valid paths parse |
| `Test_VirtualPath_Fuzz` (`Zenith_Fuzz_VirtualPath`) | libFuzzer over `Zenith_ParseVirtualPath` finds no crash and no accepted path containing `..` |
| `Test_VirtualFileSystem_PrecedenceOrder` | a file present in package, patch and loose resolves to loose; without loose, to patch |
| `Test_VirtualFileSystem_UserNeverShadowsContent` | a user-directory file with a content path is never returned for `content:/` |
| `Test_VirtualFileSystem_SymlinkEscapeRejected` | a symbolic link pointing outside a mount root fails with `PATH_ERROR_ESCAPES_MOUNT` |
| `Test_AsyncIO_PriorityOrder` | queued critical requests complete before queued low ones |
| `Test_AsyncIO_CancelQueuedAndInFlight` | a queued cancel does no I/O; an in-flight cancel reports cancelled after the buffer is released |
| `Test_AsyncIO_QueueFullRejects` | the 1,025th queued request returns `IO_ERROR_QUEUE_FULL` |
| `Test_AsyncIO_AtomicWriteInterrupted` | a fault-injected write interruption leaves the old file intact |
| `Test_AsyncIO_LowDiskRefusesWrite` | a fake volume below the margin refuses the write and raises `PLATFORM_EVENT_LOW_DISK` |
| `Test_Config_LayerPrecedence` | each layer overrides the one below; provenance names the winning layer, file and line |
| `Test_Config_DisallowedLayerIgnored` | a key set in a disallowed layer keeps the lower value and logs a warning |
| `Test_Config_DumpContainsProvenance` | the dump lists every registered key with provenance |
| `Test_AppLifecycle_StateMachine` | every state × event cell of the table produces the listed transition |
| `Test_AppLifecycle_HandshakeTimeout` | an unacknowledged surface release returns after 2,000 ms and marks the surface lost |
| `Test_ResumeState_MatchingIdentityRestores` | a resume file with matching identities restores; any mismatch discards it with the reason logged |
| `Test_Thermal_NullProviderNoAdaptation` | on Windows and Linux the monitor reports no samples and changes nothing |
| S24 device run: suspend and resume | 50 background/foreground cycles recover the surface every time with no validation error |
| S24 device run: process death | a process killed in the background relaunches into the resume state |
| S24 device run: thermal | a 30-minute run shows resolution and tier steps following headroom |
