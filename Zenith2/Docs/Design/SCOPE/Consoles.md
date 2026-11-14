# SCOPE design: consoles

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It specifies the Stage 7 console scope (Z2-D-071) to the level possible without an NDA: the platform set (ruling Z2-D-133), what the main repository may hold and the private overlay proposed to hold the rest (flagged proposal SCOPE-1, pending Q98), the per-platform SDK-access tickets, the neutral seams a console backend implements, the per-platform requirement tree, presets and `game.json` values, the certification mapping, the readiness that holds today, and the tests and devkit runs.

## Scope of this file

Nothing in this file is covered by a console vendor's non-disclosure terms: it names only publicly documented facts (the platforms exist, their developer programmes are licensed, their builds need the vendor's SDK and devkits, Xbox consoles run a DirectX 12 variant, SDL3 offers licensed console ports to licensed developers) and Zenith2's own interfaces. Every platform-specific API, limit, file format, certification requirement text and performance figure lives in the overlay.

## Console platform set

| Platform | `game.json` value | Overlay directory | SDK-access ticket | Notes |
|---|---|---|---|---|
| PlayStation 5 | `playstation5` | `Zenith2/Platforms/PlayStation5/` | "Obtain PlayStation 5 SDK access" | licensed developer programme |
| Xbox Series X and S | `xboxseries` | `Zenith2/Platforms/XboxSeries/` | "Obtain Xbox Series SDK access" | DirectX 12 variant; D-CONSOLE-RHI (open, Stage 7) decides between an overlay variant of `ZenithRHI_D3D12` and a separate backend |
| Nintendo Switch 2 | `switch2` | `Zenith2/Platforms/Switch2/` | "Obtain Nintendo Switch 2 SDK access" | licensed developer programme |

Each platform is gated on its own SDK-access ticket and nothing else; a platform whose access is never granted leaves its tickets blocked and blocks no other Stage 7 work (cross-doc row X412 clarifies Roadmap entry criterion 3 accordingly).

## What the main repository may hold

| May hold | Never holds |
|---|---|
| the platform names, the `game.json` values, the overlay mount rules and the declared-but-unavailable target names | a vendor SDK header, library, tool, sample or document, or any text copied from one |
| neutral seams (the interfaces of § "Neutral seams a console implements") and their tests on Windows and Android | a function, structure, error code, file format, path, memory figure or timing figure of a console API |
| tickets whose bodies name only Zenith2 types, seams, neutral behaviours and neutral certification categories | a certification requirement's identifier or text, a submission checklist, a devkit address or credential |
| the adopted budget numbers of a console row once its owner ruled them (a number with no derivation) | the vendor limit the number derives from |

Enforcement is review only (`Zenith2/Docs/Contracts.md` → "Review and change management"): no gate can know NDA vocabulary, and no denylist is kept, because a denylist of vendor names would itself be NDA material. The overlay's location is the structural guard (§ "The platform overlay").

**Agent access (flagged proposal SCOPE-1, pending Q98).** Console SDK material may not reach a model provider under the developer agreements, and the loop's workers read the checkout with `Read`, `Grep` and `Glob`. The recommended option of Q98, which this design assumes until the human rules: every console ticket is `needs-human`, so the loop never claims one; the overlay path `Zenith2/Platforms/**` is added to the loop's denied read paths by a PROC `needs-human` change (cross-area row SP17); no overlay content is pasted into a worker prompt, a ticket body, a board record or a main-repository doc. If the human chooses another option of Q98, the console tree is revalidated by the platform's specification-revalidation ticket.

## The platform overlay

Proposed by flagged proposal SCOPE-1 (pending Q98): it is an exception to Z2-D-002 and [Z2-N-ARCH-263], because engine libraries of a console platform would live outside the main repository, and the main gates would not cover them (each overlay runs the same gate steps in its own preset). Cross-doc row X430 holds the patch text that applies once the human approves it: the [Z2-N-ARCH-263] exception sentence, `Platforms/` in the layout block of `Zenith2/CLAUDE.md` (a protected file, a human edit) and the `Zenith2/.gitignore` rule.

Each platform's overlay is a separate, access-controlled git repository checked out at `Zenith2/Platforms/<Platform>/` by a licensed developer. Its layout:

```text
Zenith2/Platforms/<Platform>/
	CMakeLists.txt            one zenith_add_platform_overlay(<Platform>) call (SHIP's helper, row SP9)
	CMakePresets.json         schema 6, "include": ["../../CMakePresets.json"]: the platform's configure and build presets
	Engine/<Library>/         backend libraries: KIND BACKEND targets of the families of § "Neutral seams a console implements"
	CMake/                    the toolchain file
	Docs/                     the platform's NDA-covered design: API mapping, certification map, limits
	Tests/                    the platform's own tests and devkit run records
```

| Rule | Mechanism |
|---|---|
| the main CMake tree includes `Zenith2/Platforms/<Platform>/CMakeLists.txt` when it exists and the configure preset is that platform's | `CMake/ZenithPlatforms.cmake` (SHIP, row SP9) |
| without the overlay, every console target is declared-but-unavailable: listed by name in the configure report, built by no preset, never a configure error (the ONLINE Steam-backend shape) | `CompileCoverage`, `SourceLayout` and MOD's backend resolution treat the platform's backend names as known-but-absent; `CMake_Platforms_AbsentOverlayDeclaresUnavailable` |
| the console presets exist only on a machine with the overlay: CMake preset conditions cannot test a file's existence, so each overlay ships its own `Zenith2/Platforms/<Platform>/CMakePresets.json` (schema 6, including the main `CMakePresets.json` so its presets inherit the main base presets), and one developer-local `Zenith2/CMakeUserPresets.json` includes the overlay preset files present on that machine; an overlay's setup script writes only its own preset file's path into that user file's `include` list, so a second overlay never overwrites the first; `Zenith2/CMakeUserPresets.json` is ignored by the main repository through the rule cross-doc item X430 (c) adds | the overlay's setup script |
| overlay libraries follow every Zenith2 rule: Hungarian names, one `zenith_add_library` call, `Internal/<Platform>/` for platform code, public headers that include no vendor header | the overlay runs the same gate step scripts with its own preset |
| an overlay library depends only on main-repository libraries and its own; no main-repository library depends on an overlay library | `Libgraph` in the overlay's preset |
| the overlay's records reference the main board only by Draft-ID or key; the main board never references an overlay file | review |
| loop workers never read the overlay | the denied read path (§ "What the main repository may hold", agent access; pending Q98) |

**`CMake_Platforms_AbsentOverlayDeclaresUnavailable`** (a `z2-cmake` fixture registered in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, run by every `Test` step): configures a generated fixture tree with no `Platforms/` directory and a fixture game listing `playstation5` beside `windows`; asserts the configure succeeds, the configure report lists the three console platforms as declared-but-unavailable with the message `zenith: platform <value> is declared but its overlay Zenith2/Platforms/<Platform>/ is absent`, the fixture game configures for `windows` alone, and no console target exists; its failure case adds an empty `Platforms/PlayStation5/` directory without `CMakeLists.txt` and asserts the same report, never a configure error.

## SDK-access tickets

One `needs-human` TASK per platform, kind `proc`, in the Stage 6 sprint with fix version `Z2 Stage 7`, under the SCOPE epic. Its Definition of Done: the licensed developer agreement is in force; the SDK and its tools for the chosen SDK version are installed on the console build machine; at least one devkit is reachable from it; the overlay repository exists with the layout above and an empty `zenith_add_platform_overlay` that configures; `Zenith2/Platforms/<Platform>/Docs/SdkPin.md` (in the overlay) records the SDK version. Its failure scenario: access is refused or lapses, and the ticket is closed `Blocked` with the platform's tickets left blocked. At `sprint complete` of the Stage 6 sprint an SDK-access ticket that is not Done moves to the Stage 7 sprint with its fix version `Z2 Stage 7` unchanged, and `Coverage.ps1 -Stage 6` ignores it. Because the console tickets are `needs-human` (§ "What the main repository may hold"), the SDK-access ticket is the only console ticket whose Done is a human outcome a reviewer can check from the main repository.

| State | Event | Next | Effect on the platform's tickets |
|---|---|---|---|
| `NOT_REQUESTED` | a human applies to the programme | `REQUESTED` | blocked |
| `REQUESTED` | access granted, SDK installed, devkit reachable, overlay configured | `DONE` | unblocked in topological order |
| `REQUESTED` | access refused | `REFUSED` (ticket Blocked) | blocked; no other platform affected |
| `DONE` | access lapses | `LAPSED` (a new SDK-access ticket is filed) | the platform's unfinished tickets re-blocked by the new ticket |

Every other ticket of the platform is `blocks`-linked from its SDK-access ticket with the reason `backlog: <platform> SDK access`, checked by the coverage gate step's link rules; a console ticket without that link is a coverage failure (requirement "Console tickets blocked by their platform's SDK-access ticket" of `Zenith2/Docs/Requirements/SCOPE.md`).

## Neutral seams a console implements

Each row is an existing interface with two implementations today (Windows and Android); a console adds a third behind it, in its overlay, without changing the interface. A row that would need an interface change is a flagged proposal at that platform's specification revalidation, never a silent edit.

| Subsystem | Seam | Owner heading | What the console implementation provides |
|---|---|---|---|
| platform layer | `Zenith_Platform`, its window, display, lifecycle and native handles | `Zenith2/Docs/Design/PLAT/Platform.md` → "Native handles for the RHI"; `Zenith2/Docs/Architecture/PlatformLayer.md` → "Android lifecycle and surface recovery" | the display surface, suspend and resume within the platform's deadline (the Android lifecycle's states are the model: pause at a tick boundary, release the surface, recreate on resume), the platform's constrained or background mode as the background state |
| input devices | `Zenith_InputDevices` with SDL3's licensed console port, or platform devices through virtual devices | `Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API" | the platform controller as `INPUT_DEVICE_KIND_GAMEPAD` with its `Zenith_GamepadType` (PS5 and Switch types exist), haptics, the glyph family; one local user's controllers only (Z2-D-076) |
| filesystem | the virtual filesystem's package and user mounts | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Virtual filesystem" | the package mount over the platform's installed content, the user mount over the platform's save area |
| save storage | `Zenith_SaveStorage` | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveStorage header" | SAVE's journaled atomic write over the platform's save-data commit model, the platform's size declaration |
| accounts | PLAT's account signals | `Zenith2/Docs/Design/PLAT/Platform.md` → "Account changes" | the signed-in user, sign-out and user switch as account signals; one bound user |
| online services | `Zenith_OnlineServices` | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend contract" | the platform's identity, entitlements, achievements (trophies) and cloud saves as a backend |
| RHI | `Zenith_RHIDevice` and the backend factory | `Zenith2/Docs/Design/RHI.md` → "Device factories and the factory pointer type" | the device on the platform's graphics API (D-CONSOLE-RHI (open, Stage 7)), passing the baseline-equivalence check below |
| shaders | the offline shader compiler's targets | `Zenith2/Docs/Rendering.md` → "Shader system and offline compilation" | the platform's shader bytecode (D-CONSOLE-SHADERS (open, Stage 7)), generated layouts unchanged |
| audio | the audio device backends | `Zenith2/Docs/Design/AUDIO/Device.md` → "Device backends" | the platform's output device, focus and interruption as device events |
| packaging | PACK's install file system and asset-pack delivery | `Zenith2/Docs/Design/PACK.md` → "Stage 7" | the platform package, patches and DLC (PACK's interfaces take them without restructuring) |
| memory | the governor's pressure source | `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Level evaluation" | the platform's memory figures as pressure levels |
| crash capture | the crash handler | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence" (step 7) | the platform's crash reporting behind `Zenith_CrashHandler` |
| build | presets and the `ZENITH_SUPPORTED` matrix | `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix" | a configure preset and a matrix macro per platform |
| cooking | ASSET's cook targets and texture formats | `Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions" | the platform's cook target registered by the overlay's tool library |

**Baseline-equivalence check.** Before D-CONSOLE-RHI rules for a platform, its harness (in the overlay) proves each item of the GPU baseline (Z2-D-013) on the devkit: bindless sampled and storage textures and buffers at the descriptor-heap capacities of `Zenith2/Docs/Rendering.md` → "Binding model", non-uniform indexing, native 16-bit shader arithmetic, dynamic rendering or its equivalent, timeline-style completion values, indirect draws with a count buffer; the ruling records pass or fail per item in the overlay and only the outcome on the main board.

## Per-platform requirement tree

The same tree for each platform, every item a Stage 7 requirement of `Zenith2/Docs/Requirements/SCOPE.md` and a ticket per platform, blocked by that platform's SDK-access ticket:

1. the overlay and its `zenith_add_platform_overlay` call, the toolchain file and the configure preset;
2. the `game.json` platform value with its schema migration step (`Zenith2/Docs/Design/MOD.md` → "Stage 5 to Stage 7");
3. the platform layer: window and display, lifecycle within the platform's deadline, the background state;
4. input devices and controller haptics;
5. filesystem mounts and save storage;
6. accounts, one bound user;
7. the RHI realisation and the baseline-equivalence check (D-CONSOLE-RHI);
8. the shader target (D-CONSOLE-SHADERS);
9. the audio device backend;
10. the crash capture;
11. the memory pressure source and the platform's budget rows;
12. packaging, patches and DLC;
13. the online-services backend;
14. the cook target;
15. the certification map (§ "Certification mapping");
16. PROC's console gate line (`Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines");
17. the devkit runs of the three flagship slices (§ "Console tests and device runs").

## Console build presets and game.json

| Item | Specification |
|---|---|
| configure presets | `ps5`, `xbox-series`, `switch2`, defined only in each overlay's `CMakePresets.json`, each inheriting the main base preset's cache variables, with `toolchainFile` the overlay's `CMake/Toolchain.cmake` |
| build presets | `ps5-debug`, `ps5-release`, `xbox-series-debug`, `xbox-series-release`, `switch2-debug`, `switch2-release`, in the same file, each naming its configure preset and the configuration (`Debug`, `Release`) of the main tree's multi-config generator; `Build.ps1 -Preset <console-preset>` takes the configure preset and builds both |
| matrix macros | `ZENITH_PLAYSTATION5`, `ZENITH_XBOXSERIES`, `ZENITH_SWITCH2`, each the one preset; `ZENITH_GPU` gains the three presets in the overlay-present configuration only |
| `game.json` | `platforms` accepts `playstation5`, `xboxseries`, `switch2` from Stage 7 (schema version 2, with a migration step from 1 that changes nothing, because version 1 files list only `windows` and `android`); a value whose overlay is absent configures the game for its other platforms and lists the console as declared-but-unavailable |
| `backends.physics` | the object form may name the new platform keys; PhysX on a console follows the platform's ruling, Jolt otherwise |
| `XR` module | refused with any console platform (ruling SC-R3, pending Q99) |
| the console build line | `Build.ps1 -Preset <console-preset>` exits 3 (`GATE_EXIT_ENVIRONMENT`) with `Build: environment: overlay Zenith2/Platforms/<Platform>/ absent; run the overlay setup` when `Zenith2/Platforms/<Platform>/CMakeLists.txt` is missing; no new gate status exists, and the line is red if ever selected without the overlay. It runs only in the overlay's own checkout, through that overlay's own project configuration; the main board's categories carry no console line, so no main-repository diff selects it (PROC row SP17) |

## Certification mapping

Each platform's certification requirements are mapped, in the overlay's `Docs/CertificationMap.md`, onto Zenith2 behaviours already specified for Windows and Android, in these neutral categories:

| Category | Zenith2 behaviour it maps to |
|---|---|
| suspend and resume | PLAT's lifecycle (pause at a tick boundary, surface release, the bounded handshake), SAVE's resume state |
| user sign-in, sign-out and switch | ONLINE's rebind sequence and PLAT's account signals |
| save-data integrity and space | SAVE's journaled write, corruption recovery and low-disk refusal |
| controller disconnect | PLAT's hotplug and reconnection window, the game's pause on disconnect |
| network loss | ONLINE's offline results; NET's Stage 7 disconnect handling |
| content entitlements and DLC | ONLINE's entitlements, PACK's mounting |
| crash and hang | the crash handler, the process watchdog |
| accessibility and localization | UI's accessible flows and localization |

A requirement that no existing behaviour satisfies becomes a ticket of the owning area, filed at the platform's specification revalidation with a neutral title.

## Console readiness today

| Check | From | Enforcement |
|---|---|---|
| no platform or backend conditional outside `Internal/<Platform>/` or backend targets | S0 | `ForbiddenCheck` F11 ([Z2-N-ARCH-171] and `Zenith2/Docs/Contracts.md` → "No layout-changing conditionals") |
| every seam of § "Neutral seams a console implements" exists with two implementations (Windows, Android) by the stage its owner lands it | S2–S6 | the owners' tests; the register's `consoles` boundary names the table |
| no main-repository code names a console | S0 | review; the overlay's location |

## Console tests and device runs

The overlay's tests run in its preset through the console line in the overlay's own checkout (§ "Console build presets and game.json"), plus the platform's `Test` line once a devkit runner exists; PROC's `Test-Build.ps1` gains the case `Test_Build_ConsolePresetAbsentOverlayExits3` (row SP17). The main repository adds one console test, `CMake_Platforms_AbsentOverlayDeclaresUnavailable` (§ "The platform overlay"). Devkit runs are `needs-human`, recorded in the overlay, with only the outcome (pass or fail per run, the adopted budget numbers) mirrored to the main board:

| Run | Content |
|---|---|
| `Device_Console_<Platform>_FlagshipPlaythroughs` | each flagship slice's playthrough script on the devkit: zero crash or blocker defects, the platform's budget rows measured |
| `Device_Console_<Platform>_Lifecycle` | suspend and resume mid-save, a user switch mid-session, a controller removed and replaced, network removed |
| `Device_Console_<Platform>_CertificationPrecheck` | every row of the certification map exercised once |
