# SCOPE — platform scope

## Purpose and scope

### What SCOPE owns

SCOPE owns the recorded scope boundaries of Zenith2 that are not capabilities of one engine subsystem, and the requirement trees of the two deferred platform scopes that have no other owner:

- **The scope records.** One decision record per scope boundary named by the programme — consoles, VR/XR, modding, local multiplayer, dedicated servers and online backends — each with its options, the option chosen and the reason (`Zenith2/Docs/Design/SCOPE/ScopeRecords.md`). The six settled entries Z2-D-071 to Z2-D-076 are the records' rulings; the Stage 1 SCOPE decision tickets confirm them and land the scope register.
- **The scope register.** The machine-readable list of every scope boundary, `Zenith2/Docs/Scope/ScopeRegister.json`, with its classification, stage, decision, requirement range and enforcing checks, and the two checks that keep it true (`Pwsh_ScopeRegister_Valid` on every `Test` run, `Pwsh_ScopeRegister_ReadinessTestsPresent` at stage acceptance).
- **XR readiness.** The concrete checks that Stage 2 to Stage 6 tickets of other areas satisfy so that the Stage 7 XR work adds code without changing an earlier contract (Z2-D-072): each check names its owner heading, its test and the stage from which it holds (`Zenith2/Docs/Design/SCOPE/XrReadiness.md`), and SCOPE's own integration suite `Zenith_Integration_ScopeChecks` holds the checks no owner's suite covers.
- **The Stage 7 XR tree, specified against OpenXR 1.1.** The `ZenithXR` module: the device-interop provider created before the RHI device, the OpenXR instance, system and session lifecycle, the frame cycle and its pacing, swapchains, composition layers, reference spaces, recentring, play-area bounds, the OpenXR device source that feeds PLAT's input model, haptics, the `XR` Luau namespace and its events, diagnostics, the `Zenith_OpenXrApi` seam with its loader and scripted implementations, and the reference device matrix (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md`, `XrRuntime.md`, `XrInput.md`, `XrTests.md`).
- **The Stage 7 console trees, specified without NDA material.** For PlayStation 5, Xbox Series X and S, and Nintendo Switch 2: the per-platform `needs-human` SDK-access ticket that blocks every other ticket of that platform, the proposed private platform overlay that would hold everything an NDA covers (flagged proposal SCOPE-1, pending Q98), and one requirement per neutral seam a console backend implements (`Zenith2/Docs/Design/SCOPE/Consoles.md`).
- **The exclusions** as `excluded` requirements with their rationale and decision.

### Explicit exclusions

| Not SCOPE's | Owner | Basis |
|---|---|---|
| render views, `VIEW_KIND_STEREO`, the eye expansion `Flux_ExpandStereoRequest` and every stereo render pass | RPROD | `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view"; Z2-D-059, Z2-D-072 |
| the XR-ready input model, the POSE action value and its command encoding, the `interactionProfile` bindings key, `INPUT_DEVICE_KIND_XR_CONTROLLER` virtual devices | PLAT | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design" |
| the stereo camera output of the gameplay camera director | GAME | `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera model" |
| the RHI backends, device creation and the interop additions XR asks for | RHI | `Zenith2/Docs/Design/RHI.md` → "Device creation sequence" |
| networked multiplayer and dedicated server builds, their requirement tree | NET (`Zenith2/Docs/Design/NET/Stage7.md` → "Dedicated server builds"; `Zenith2/Docs/Requirements/NET.md`; cross-area row SP11) | Z2-D-073 |
| the online-services adapter and its Steam and Google Play backends | ONLINE | `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend contract"; Z2-D-074 |
| dynamic plugins and out-of-repo projects | MOD | `Zenith2/Docs/Design/MOD.md` → "Exclusions"; Z2-D-024 |
| editor authoring for the Stage 7 platforms | EDIT | `Zenith2/Docs/Design/EDIT.md` → "Stage 7" |
| the Stage 7 gate lines (protected) | PROC | `Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines" |
| modding, local multiplayer and split-screen | excluded | Z2-D-075, Z2-D-076 |
| XR on consoles (a console's own headset) | excluded (ruling SC-R3, pending Q99) | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "XR record" |

### Inventory items covered

| Item | Name | Where |
|---|---|---|
| Z2-I-SCOPE-01 | consoles (SDK-gated) | `Zenith2/Docs/Design/SCOPE/Consoles.md` (every heading) |
| Z2-I-SCOPE-02 | console tickets specified to the level possible without an NDA | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold" |
| Z2-I-SCOPE-03 | console tickets blocked by a `needs-human` SDK-access ticket per platform | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-I-SCOPE-04 | VR/XR via OpenXR | `Zenith2/Docs/Design/SCOPE/XrRuntime.md`, `XrInput.md`, `XrPublicAPI.md`, `XrTests.md` |
| Z2-I-SCOPE-05 | XR tickets specified against OpenXR | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions" |
| Z2-I-SCOPE-06 | multi-view kept XR-ready | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-I-SCOPE-07 | input kept XR-ready | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks" |
| Z2-I-SCOPE-08 | modding | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Modding record" |
| Z2-I-SCOPE-09 | local multiplayer | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Local multiplayer record" |
| Z2-I-SCOPE-10 | each scope boundary recorded with a decision ID | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register" |
| Z2-I-SCOPE-11 | deferred scopes get full requirement trees now | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Deferred scopes and their trees" |

### Section files

| File | Headings (`##`) |
|---|---|
| `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` | Scope of this file · Record format · Consoles record · XR record · Modding record · Local multiplayer record · Dedicated servers record · Online backends record · The scope register · Register checks · Deferred scopes and their trees · Stage 1 SCOPE decision tickets |
| `Zenith2/Docs/Design/SCOPE/XrReadiness.md` | Scope of this file · How a readiness check binds a ticket · Multi-view checks · Input checks · Other subsystems XR touches · Readiness tests SCOPE owns · Exclusion checks SCOPE owns · Retirement at Stage 7 |
| `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` | Scope of this file · Rules across the XR API · Header list · Zenith_XrTypes header · Zenith_XrErrors header · Zenith_XrInterop header · Zenith_XrSystem header · Zenith_Module_XR header · Zenith_XrTestAccess header · Interfaces of lower libraries · Script-facing API · Event types · Configuration keys · Misuse codes |
| `Zenith2/Docs/Design/SCOPE/XrRuntime.md` | Scope of this file · OpenXR version and extensions · The OpenXR API seam · Startup and the device-interop provider · Graphics bindings · Session state machine · Event handling · Frame cycle · Pacing and the desktop mirror · Swapchains · Composition layers · Reference spaces · Recentre and play area · Loss recovery and shutdown · Result mapping · Scripted runtime |
| `Zenith2/Docs/Design/SCOPE/XrInput.md` | Scope of this file · The control-mirror action set · XR control catalogue · Suggested bindings · Per-frame input flow · Poses for simulation · Haptic output · Interaction profile changes · Replay and determinism |
| `Zenith2/Docs/Design/SCOPE/XrTests.md` | Scope of this file · Test targets · Readiness and exclusion tests · Module and seam tests · Session and frame tests · Swapchain and layer tests · Input tests · Script surface tests · Format and fuzz tests · Real-runtime integration · Reference device matrix · Device runs · Gate steps per test group |
| `Zenith2/Docs/Design/SCOPE/Consoles.md` | Scope of this file · Console platform set · What the main repository may hold · The platform overlay · SDK-access tickets · Neutral seams a console implements · Per-platform requirement tree · Console build presets and game.json · Certification mapping · Console readiness today · Console tests and device runs |

### Conventions

"XR" means the Stage 7 virtual-reality support through OpenXR; "the runtime" in an XR sentence is the active OpenXR runtime (a vendor's `openxr_runtime` library chosen by the OpenXR loader), and "the Zenith2 runtime" is `ZenithRuntime`. "Session" is the one `XrSession` of a process. "Frame" is one iteration of `Zenith_Runtime::RunFrameLoop`; "XR frame" is one `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame` cycle. "Platform" in a console sentence is one of the three console platforms; "overlay" is that platform's private checkout (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay"). Times are nanoseconds of the OpenXR runtime's `XrTime` where an OpenXR call takes or returns one, and the platform's monotonic nanoseconds otherwise; the conversion uses `XR_KHR_win32_convert_performance_counter_time` (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions"). Lengths are metres, angles radians.

The rulings this design makes were labelled SC-R1 to SC-R8 (§ "Rulings this design makes"); six are entries of `Zenith2/Docs/DecisionLog.md` → "Session rulings" (Z2-D-133 and Z2-D-136 to Z2-D-140), and a ticket cites the entry ID. SC-R2 is a flagged proposal, not a ruling (pending Q98), and SC-R3 waits on the human (pending Q99).

### Prior art

<!-- informative -->
The old engine has no XR support, no console platform and no modding; `Zenith/` therefore offers no structure to carry over. The XR design follows the OpenXR 1.1 specification and its loader, and the input mirror follows the practice of engines that keep their own action model above OpenXR (an action per controller input, suggested bindings per interaction profile, the engine's own mapper on top). The console design follows the practice of licensed middleware that ships a public core and a per-platform private layer: the public tree holds only seams and platform-neutral behaviour, and every NDA-covered fact lives in an access-controlled repository that builds into the same tree.
<!-- /informative -->

## Dependencies

### Libraries SCOPE depends on

| Library | Kind | Layer | `DEPENDS` |
|---|---|---|---|
| ZenithXR | module (`KIND MODULE`, `MODULE_NAME XR`, Stage 7) | 7 | ZenithRuntime, ZenithFlux, ZenithRHI, ZenithPlatform, ZenithScript, ZenithReflect, ZenithECS, ZenithProfiling, ZenithJobs, ZenithCore; third party `openxr-loader` (`Internal/` only) |
| `Zenith_Integration_ScopeChecks` | integration suite (Stage 2) | — | ZenithRuntime, ZenithFlux, ZenithPlatform, ZenithRHI_Vulkan, ZenithRHI_D3D12, ZenithECS_Custom |
| `Zenith_Integration_XrRuntime` | integration suite (Stage 7) | — | ZenithRuntime, ZenithXR, ZenithGameplay, ZenithFlux, ZenithRHI_Vulkan, ZenithRHI_D3D12, ZenithECS_Custom |

Every `DEPENDS` edge points to a strictly lower layer (`Zenith2/Docs/Contracts.md` → "Layering DAG"). No console code is a library of the main repository; each console's libraries live in its overlay and are declared there (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay").

### Headings SCOPE relies on

| Need | Source |
|---|---|
| render views, the stereo kind, view lists, view histories | `Zenith2/Docs/Rendering.md` → "Render views"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header", "View list construction", "View registry", "Advanced multi-view" |
| the frame pacer and the render-submit thread | `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer", "Flux_Renderer header" |
| the XR-ready input model and the Stage 7 PLAT additions | `Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design", "Zenith_InputDevices API", "Zenith_InputSystem API", "Haptic API", "Control paths", "Control catalogue", "Bindings file schema"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation", "Script-facing input API" |
| device creation and swapchains | `Zenith2/Docs/Design/RHI.md` → "Device creation sequence", "Device factories and the factory pointer type"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header", "Zenith_RHISwapchain header" |
| the composition table, startup and the frame loop | `Zenith2/Docs/Architecture/CompositionRoot.md` → "The generated composition table", "Startup sequence", "The frame loop"; `Zenith2/Docs/Design/MOD.md` → "Zenith_GameComposition.h", "Zenith_Module.h" |
| module and backend resolution, `game.json` | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping", "Backend resolution", "game.json schema v1"; `Zenith2/Docs/Architecture/BuildSystem.md` → "The game.json schema", "The ZENITH_SUPPORTED matrix", "Presets" |
| pause reasons and the camera director | `Zenith2/Docs/Design/GAME/WorldHost.md` → "Pause arbitration"; `Zenith2/Docs/Design/GAME/Cameras.md` → "Camera model", "Output and cuts" |
| asynchronous publications and misuse codes | `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" |
| world-space UI and the UI composite | `Zenith2/Docs/Design/UI/Layout.md` → "World-space panels"; `Zenith2/Docs/Design/UI/Rendering.md` → "UIComposite implementation" |
| console seams | `Zenith2/Docs/Design/SAVE/PublicAPI.md` → "Zenith_SaveStorage header"; `Zenith2/Docs/Design/AUDIO/Device.md` → "Device backends"; `Zenith2/Docs/Design/ONLINE/Backends.md` → "Backend contract"; `Zenith2/Docs/Design/PACK.md` → "Stage 7"; `Zenith2/Docs/Design/PLAT/Platform.md` → "Native handles for the RHI", "Account changes" |
| third-party pins | `Zenith2/Docs/Design/SHIP/Manifest.md` → "The dependencies section", "Dependency entry fields" |
| console, overlay and memory services | `Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"; `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers"; `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration" |
| the Stage 7 gate lines and the acceptance run | `Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N" |

### Areas SCOPE depends on

RPROD (views, stereo expansion, pacing), PLAT (input, platform layer), RHI (device interop), MOD (composition, game.json, module host), GAME (camera director, pause reasons), SCRIPT (async source, bindings, misuse block), UI (world-space panels, the XR quad layer), CORE (off-centre projections, governor), SHIP (the loader pin), PROC (gate lines, acceptance), NET and ONLINE (their scope records), and for consoles PACK, SAVE, AUDIO, DEVT, ASSET and EDIT.

### Areas that consume SCOPE

RPROD's Stage 7 stereo tickets (cross-area row RPB21: the session, swapchain, frame timing and pose source), GAME's Stage 7 camera tickets (rows GmB23, GmB28), PLAT's Stage 7 input tickets (the OpenXR device source PLAT names as SCOPE's), EDIT's Stage 7 platform cook (row EdB24: the SDK-access tickets), PROC's Stage 7 gate lines (the `z2-xr` label and the console presets), every area whose Stage 2 to Stage 6 tickets carry a readiness check.

### Interfaces SCOPE declares and implements

- SCOPE declares no interface for a lower library. ZenithXR sits above ZenithRuntime and ZenithFlux, so every callback it needs from them goes through an interface the lower library declares, which ZenithXR implements: `Zenith_DeviceInteropProvider` (ZenithRuntime, Stage 7), `Flux_StereoFrameSource` (ZenithFlux, Stage 7), `Zenith_ModuleInstance` and `Zenith_ModuleFrameListener` (ZenithRuntime), `Zenith_MemoryConsumer` (ZenithCore). The first two are additions SCOPE asks of MOD and RPROD (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Interfaces of lower libraries").
- Every callback therefore points downward (`Zenith2/Docs/Contracts.md` → "Layering DAG").

### Assumed interfaces pending other areas

<!-- assumption: pending RPROD, RHI, MOD, ARCH, CORE, PLAT, GAME, SCRIPT, UI, SHIP, PROC -->
| Interface | Owner | SCOPE's use | Row |
|---|---|---|---|
| `Flux_StereoFrameSource` (ZenithFlux public header `Flux_StereoFrameSource.h`) with `AcquireStereoFrame`, `BeforeSubmit`, `AfterSubmit`, `PacesFrames`; `Flux_XrViewPoses` moved from `Internal/Views/` to that header and widened with depth images and clip planes; `Flux_Renderer::SetStereoFrameSource` | RPROD | the frame cycle and late-latched poses | SP1 |
| pacing mode `FLUX_PACING_MODE_EXTERNAL` (the pacer neither waits nor sets a swap interval while a frame source paces), the left-eye mirror copy into the desktop swapchain, the eye-depth copy into an XR depth image | RPROD | pacing and the mirror | SP1 |
| `Flux_ViewLens` off-centre tangents (`m_fTanLeft`, `m_fTanRight`, `m_fTanUp`, `m_fTanDown`, zero meaning symmetric) honoured by `Flux_BuildViewList` from Stage 5 | RPROD | readiness check XRR-V5 | SP1 |
| `Zenith_RHIInteropRequest` in `Zenith_RHIDeviceDesc` (`m_pxInterop`), the native-handle queries `Zenith_RHIDevice::GetInteropHandles` and `GetNativeTexture`, the externally owned texture import `Zenith_RHIDevice::ImportExternalTexture` with its release-state contract | RHI | graphics bindings and swapchain images | SP2 |
| `bool Flux_Renderer::RequestDeviceRecreation(Flux_DeviceRecreationReason)` (false outside `NORMAL`) entering RHI's recovery path from `NORMAL` with worlds paused as for a loss, `rhi.deviceLossPolicy` not applied and the same-adapter clause lifted; the provider's calls around the old device's destruction and the new device's creation | RHI, RPROD | no headset at launch, instance loss, device loss | SP16, X443 |
| the optional `Zenith_ModuleDescriptor` member `m_pfnCreateDeviceInterop`, the provider's calls at the end of startup step 11, inside step 12 and around the device's destruction in shutdown step 9, `Zenith_ModuleStartContext::GetDeviceInteropProvider`, `Zenith_ModuleFrameListener::OnBeforePlatformPump`; ZenithXR as the 14th module | MOD, ARCH | startup and input sampling | SP5, X411, X429 |
| `Zenith_MakePerspectiveOffCentreStandardZ`, `Zenith_MakePerspectiveOffCentreReversedZ` and `Zenith_MakePerspectiveOffCentreReversedZInfinite` in `ZenithCore/Zenith_Maths.h` from Stage 5 | CORE | off-centre projections | SP3 |
| `WORLD_PAUSE_REASON_XR_UNFOCUSED = 1u << 4` | GAME | focus-loss pause | SP6 |
| the source kind `ASYNC_SOURCE_XR` = 16 (ruling ASYNC-SOURCE-15-16; mixed: ticketless notifications), at most 64 bytes per result, capacity column from `uXR_NOTIFY_MAX_OUTSTANDING` (8 per world) | SCRIPT | the `XR` events | SP7 |
| the UI composite into an offscreen target per frame for the XR quad layer | UI | the UI quad layer | SP8 |
| the `openxr-loader` manifest entry (shared library, import-linked, staged beside the executable) | SHIP | the loader | SP9 |
| `ZENITH_ACCEPTANCE_STAGE` exported to every acceptance line; the `z2-xr` and console preset lines; the console build line exiting 3 (`GATE_EXIT_ENVIRONMENT`) without the overlay and run only in the overlay's own checkout | PROC | the register check, the Stage 7 gates | SP10, SP17 |
<!-- /assumption -->

## Library and file layout

### Targets

| Target | Kind | Directory | `ZENITH_SUPPORTED` | Stage |
|---|---|---|---|---|
| `ZenithXR` | `KIND MODULE`, `MODULE_NAME XR` | `Zenith2/Engine/ZenithXR/` | `ZENITH_WINDOWS` | S7 |
| `ZenithXR_Tests` | tests of ZenithXR, `TEST_LABELS z2-xr` | `Zenith2/Engine/ZenithXR/Tests/` | `ZENITH_WINDOWS` | S7 |
| `Zenith_Fuzz_XrScript` | fuzz target (`win-asan`) | `Zenith2/Engine/ZenithXR/Tests/Fuzz/` | `win-asan` | S7 |
| `Zenith_Integration_ScopeChecks` | integration suite, `TEST_LABELS z2-scope` | `Zenith2/Tests/ScopeChecks/` | `ZENITH_WINDOWS` (its `DEPENDS` include the D3D12 backend) | S2 |
| `Zenith_Integration_XrRuntime` | integration suite (real OpenXR runtime, skip code 77) | `Zenith2/Tests/XrRuntime/` | `ZENITH_WINDOWS` | S7 |
| the two register checks | `pwsh` checks registered by name in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` | `Zenith2/CMake/Tests/Scope/` | `win` | S1 |

`ZenithXR` is `ZENITH_WINDOWS` alone: Stage 7 XR is Windows PC VR (ruling SC-R3), and a game that lists `XR` in `modules` with `android` in `platforms` fails the configure (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header"). No source is excluded per configuration ([Z2-N-ARCH-171]); the OpenXR loader calls live in `Internal/Windows/`.

### ZenithXR files

| File | Contents |
|---|---|
| `Zenith_XrTypes.h`, `Zenith_XrErrors.h`, `Zenith_XrSystem.h`, `Zenith_Module_XR.h`, `Zenith_XrTestAccess.h` | the public headers of `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` |
| `Internal/Zenith_OpenXrApi.h` | the seam: one virtual member per OpenXR entry point ZenithXR calls (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam") |
| `Internal/Windows/Zenith_OpenXrApiLoader_Windows.cpp` | the loader-backed implementation, the one TU including `<openxr/openxr.h>` with `XR_USE_PLATFORM_WIN32`, `XR_USE_GRAPHICS_API_VULKAN` and `XR_USE_GRAPHICS_API_D3D12` |
| `Internal/Zenith_OpenXrTypes.h` | the subset of OpenXR structures the seam passes, mirrored as Zenith structs so no other TU includes the OpenXR headers (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam") |
| `Internal/Zenith_OpenXrApiScripted.h`, `.cpp` | the scripted runtime (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime") |
| `Internal/Zenith_XrScriptFormat.h`, `.cpp` | the `zenith2.xr-script` parser and validator |
| `Internal/Zenith_XrInteropProvider.h`, `.cpp` | `Zenith_XrInteropProvider`: instance, system, graphics requirements, the RHI interop request |
| `Internal/Zenith_XrSession.h`, `.cpp` | the session state machine and event handling |
| `Internal/Zenith_XrDeviceListener.h` | the interface through which the provider reaches the system at device destruction and creation (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider") |
| `Internal/Zenith_XrFrameCycle.h`, `.cpp` | the per-frame state machine, pacing and the stereo frame source |
| `Internal/Zenith_XrSwapchains.h`, `.cpp` | swapchain creation, sizing, recreation, image import |
| `Internal/Zenith_XrLayers.cpp` | projection, depth and quad layer assembly |
| `Internal/Zenith_XrSpaces.h`, `.cpp` | reference spaces, recentre, play-area bounds |
| `Internal/Zenith_XrInputSource.h`, `.cpp` | the control-mirror action set, sync, pose sampling, haptics |
| `Internal/Zenith_XrBindingTable.cpp` | the suggested-binding table of `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings" as constant data |
| `Internal/Zenith_XrResultMap.cpp` | `XrResult` to `Zenith_XrError` |
| `Internal/Zenith_XrModule.cpp`, `Internal/Zenith_XrModuleInstance.h`, `.cpp` | the entry points, start, attach, detach, frame listener, shutdown |
| `Internal/Zenith_XrBindings.cpp` | the `XR` Luau binding table and its misuse codes |
| `Internal/Zenith_XrEvents.cpp` | the three event types and the `XR.Events` system |
| `Internal/Zenith_XrNotifications.h`, `.cpp` | the notification queue and its tick-boundary handler |
| `Internal/Zenith_XrConfigKeys.cpp` | the configuration keys |
| `Internal/Zenith_XrMemoryConsumer.cpp` | the memory consumer |
| `Internal/Zenith_XrDevTools.cpp` | console commands and the `xr` overlay panel |
| `Internal/Zenith_XrTestAccess.cpp` | the test access |
| `Tests/…` | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Test targets"; `Tests/Support/Zenith_XrTestScripts.h` (header-only, X184: inline builders of `zenith2.xr-script` documents); `Tests/Examples/Zenith_XrSystem_Example.cpp` |

### Repository files outside ZenithXR

| File | Owner rule |
|---|---|
| `Zenith2/Docs/Scope/ScopeRegister.json` | the scope register (`Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register"); an unprotected doc data file |
| `Zenith2/CMake/Tests/Scope/Pwsh_ScopeRegister_Valid.ps1`, `Pwsh_ScopeRegister_ReadinessTestsPresent.ps1` | the register checks (`Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks"), registered by name in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, label `z2-cmake` and `z2-acceptance-scope` respectively |
| `Zenith2/Tests/ScopeChecks/CMakeLists.txt` | exactly `zenith_add_integration_tests(ScopeChecks DEPENDS ZenithRuntime ZenithFlux ZenithPlatform ZenithRHI_Vulkan ZenithRHI_D3D12 ZenithECS_Custom TEST_LABELS z2-scope)` ([Z2-N-ARCH-157]) |
| `Zenith2/Tests/ScopeChecks/Zenith_ScopeMultiView_Tests.cpp`, `Zenith_ScopeInput_Tests.cpp`, `Zenith_ScopeExclusion_Tests.cpp` | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns", "Exclusion checks SCOPE owns" |
| `Zenith2/Tests/XrRuntime/CMakeLists.txt` | exactly `zenith_add_integration_tests(XrRuntime DEPENDS ZenithRuntime ZenithXR ZenithGameplay ZenithFlux ZenithRHI_Vulkan ZenithRHI_D3D12 ZenithECS_Custom)` |
| `Zenith2/Tests/XrRuntime/Zenith_XrRuntime_Tests.cpp` | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Real-runtime integration" |
| `Zenith2/Docs/Research/Scope/XrDeviceRuns/<date>-<device>.md` | the device-run records of `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Device runs" (needs-human) |
| `Zenith2/Platforms/<Platform>/` | proposed (flagged proposal SCOPE-1, pending Q98): never tracked, the private overlay mount point, ignored by `Zenith2/.gitignore` and denied to loop workers (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay") |

## Public API

### API summary

| Header | Specified in |
|---|---|
| every ZenithXR header | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Header list" and the header headings it lists |
| the `XR` Luau namespace, its events and misuse codes | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Script-facing API", "Event types", "Misuse codes" |
| the configuration keys | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Configuration keys" |
| the interfaces ZenithXR implements for lower libraries | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Interfaces of lower libraries" |
| console seams | no public type: every console implementation implements an existing interface of its owner (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements") |

### Handle kinds used

ZenithXR declares no handle kind. It holds `Zenith_RHITextureHandle`s of the swapchain images it imports (device handles, owned by the device, released through the device), the `Zenith_InputDeviceID`s of the three XR controller devices it creates through PLAT (plain values, never serialized), and OpenXR handles (`XrInstance`, `XrSession`, `XrSwapchain`, `XrSpace`, `XrActionSet`, `XrAction`) as opaque 64-bit values inside `Internal/` (`Zenith2/Docs/Contracts.md` → "Handle kinds": none crosses a public header).

## Data structures and formats

### In-memory structures

| Structure | Contents | Size |
|---|---|---|
| `Zenith_XrInteropProvider` | instance, system ID, system properties, the enabled extension mask, graphics requirements, the RHI interop request with its string storage (at most 32 instance and 32 device extension names, 256 bytes each) | ≈ 17 KiB, fixed at creation |
| `Zenith_XrSession` | session handle, state, the event buffer (one `XrEventDataBuffer`, 4,000 bytes), the retry clock, the reference spaces, counters | ≈ 5 KiB |
| `Zenith_XrFrameCycle` | the frame-slot ring of `r.framesInFlight` + 1 records (frame index, `XrFrameState`, phase, located views, acquired image indices), the pacing statistics ring of 600 intervals | ≈ 9 KiB |
| `Zenith_XrSwapchains` | two colour and two depth swapchains, each with at most 8 imported image handles, plus one quad swapchain | ≈ 1 KiB plus GPU images |
| `Zenith_XrInputSource` | the action set, 20 actions (19 control kinds and the haptic action, each with the left and right subaction paths; `Zenith2/Docs/Design/SCOPE/XrInput.md` → "XR control catalogue"), 4 action spaces, the last values per control, the bound-profile list | ≈ 4 KiB |
| notification queue | 16 records of 64 bytes per world | 1 KiB per world |
| seam call-log ring (non-Shipping, scripted runtime only) | 4,096 `Zenith_XrSeamCallRecord` of 120 bytes (2 + 2 + 4 + 8 + 48 + 52 = 116, padded to 8-byte alignment) | 480 KiB, allocated by the scripted runtime |

### Formats summary

| Format | Where | Specified in |
|---|---|---|
| `zenith2.scope-register` (JSON, schema version 1) | `Zenith2/Docs/Scope/ScopeRegister.json` | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register" |
| `zenith2.xr-script` (JSON, schema version 1) | test fixtures written by test setup, and `xr.script` in development builds | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime" |
| `XR` notification payloads (64 bytes, little-endian) | `ASYNC_SOURCE_XR` publications and the replay log | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Event types" |
| device-run record (Markdown with a fixed front table) | `Zenith2/Docs/Research/Scope/XrDeviceRuns/` | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Device runs" |

No cooked format: XR adds no asset kind, and its settings are configuration keys. No save data: the recentre offset and the reference-space choice are presentation state of the process and are never written to a save.

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| scope register validation | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks" |
| readiness presence check | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "How a readiness check binds a ticket" |
| instance creation and graphics requirements | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider", "Graphics bindings" |
| event handling | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Event handling" |
| swapchain format choice, sizing and recreation | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains" |
| layer assembly | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers" |
| reference-space choice, recentre, bounds | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces", "Recentre and play area" |
| action sync, pose sampling, haptics | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Per-frame input flow", "Poses for simulation", "Haptic output" |
| loss recovery | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown" |

### State machine index

| State machine | Specified in |
|---|---|
| the session (OpenXR states plus Zenith2's own) | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine" |
| the XR frame cycle | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle" |
| the scripted runtime's state | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime" |
| a console platform's ticket gate | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |

### Frame placement

| Work | Where | Thread |
|---|---|---|
| instance, system, graphics requirements | the last action of startup step 11; the interop request and device check inside step 12 (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider") | main |
| session creation, spaces, action set, swapchains | startup step 15 (module start) | main |
| controller sampling (`xrSyncActions`, `xrLocateSpace` of the four action spaces) | frame prologue step 1, `OnBeforePlatformPump`, before `Platform.Pump` | main |
| event polling (`xrPollEvent` until empty), session transitions, recentre, play-area query | frame prologue step 1, `OnPlatformEvents`, after `Platform.Pump` | main |
| eye location (`xrLocateViews`) and image acquisition (`xrAcquireSwapchainImage`) | `Flux.BuildGraph` node, through `Flux_StereoFrameSource::AcquireStereoFrame` | the build node's worker |
| `xrBeginFrame`, `xrWaitSwapchainImage` | `Flux.Submit`, through `BeforeSubmit` | render-submit |
| `xrReleaseSwapchainImage`, `xrEndFrame`, then `xrWaitFrame` of the next XR frame | `Flux.Present`, through `AfterSubmit` | render-submit |
| notifications to worlds | each world's tick boundary (the `ASYNC_SOURCE_XR` handler) and phase 1 (`XR.Events@W<tag>`) | job workers |
| shutdown | shutdown step 6 (module destruction), before the device is destroyed at step 9 | main |

The three thread contexts never call OpenXR concurrently: the prologue runs between frames ([Z2-N-ARCH-870]), the build node precedes `Flux.Submit` and `Flux.Present` inside one render subgraph, and the iteration barrier orders `Flux.Present` before the next prologue. `Zenith_XrCallGuard` (an atomic owner word, debug builds) asserts that ordering on every seam call (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam").

## Threading lifetime and error contract

### Thread affinity per type

| Type | Affinity |
|---|---|
| `Zenith_XrSystem` | `MAIN_THREAD`, except `GetStatus` (`ANY_THREAD`, an atomic snapshot) and the `Flux_StereoFrameSource` members (their own placement, § "Frame placement") |
| `Zenith_XrInteropProvider` | `MAIN_THREAD` (startup steps 11 and 12, the device-recreation prologue, shutdown step 9) |
| `Zenith_OpenXrApi` and both implementations | as the calling member's placement; never concurrently (`Zenith_XrCallGuard`) |
| `Zenith_XrTestAccess` | `MAIN_THREAD` |
| every value type | `IMMUTABLE` |

### Threads SCOPE owns

None. OpenXR runtimes may run internal threads; they belong to the runtime and stop at `xrDestroyInstance`. No row of `Zenith2/Docs/Contracts.md` → "Threading and concurrency contract" ("Shutdown ownership order") is added.

### Blocking calls

`xrWaitFrame` blocks the render-submit thread until the runtime wants the next XR frame; it is the frame loop's one pacing wait while a session runs, and it replaces the frame pacer's CPU wait (`Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"; [Z2-N-ARCH-394] allows render-submit waits). `xrWaitSwapchainImage` blocks the render-submit thread for at most `xr.imageWaitTimeoutMs` (default 100 ms), passed to OpenXR as `timeout = xr.imageWaitTimeoutMs × 1,000,000` ns; a timeout is `XR_TIMEOUT_EXPIRED`, which skips the frame (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"). No other OpenXR call made by ZenithXR blocks, and no job calls one. While the device-loss state is not normal the render subgraph is empty ([Z2-N-ARCH-880]) and no OpenXR frame call is made; the loop's pacing in that interval is the engine's own.

### Lock-order entries

None. The three call contexts are ordered by the frame structure, not by a lock; the notification queues are `Zenith_SpscRing` instances (CORE; ruling Q80), one producer (the main thread) and one consumer (the world's tick-boundary node) each.

### Startup and shutdown

- Startup (ruling Z2-D-137; no startup step and no enumerator is added): step 6 registers the `XR` configuration keys and types; the last action of step 11 calls the XR descriptor's optional `m_pfnCreateDeviceInterop` (instance and system, before the device); step 12 passes the provider's interop request to the device factory and its `OnDeviceCreated` the device; step 15 starts the module (session, spaces, actions, swapchains, the stereo frame source registered with the renderer); step 16 attaches each world. The provider failure rule is `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider".
- Shutdown step 6 destroys the module instance: `Flux_Renderer::SetStereoFrameSource(nullptr)` at a render-idle point, `xrRequestExitSession` when the session is running, at most `xr.shutdownWaitMs` (500) of event polling for `XR_SESSION_STATE_STOPPING`, `xrEndSession`, the swapchains (their imported textures released through the device first), the spaces and action set, `xrDestroySession`. Shutdown step 9 calls the provider's `OnDeviceDestroying`, destroys the device, calls `OnDeviceDestroyed`, then destroys the provider, which calls `xrDestroyInstance` after the device it may have created (cross-doc item X411 patches the shutdown table).

### Cancellation

| Operation | Cancellation points | Who frees partial results |
|---|---|---|
| a session that never reaches `READY` | `xr.sessionReadyTimeoutMs` (10,000) elapses | the session destroys its handles and enters `RETRY_WAIT` |
| a swapchain recreation | the session leaves `RUNNING_*` before it completes | the swapchain module destroys the half-created swapchain and its imports |
| a recentre request | the session is lost | the request is dropped and counted `xr.recentre.dropped` |

### Error enumerations

`Zenith_XrErrorCode` with the record `Zenith_XrError` (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_XrErrors header") for every fallible call; every `XrResult` is mapped by `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Result mapping". Errors crossing into a module error are mapped at `Start` and `AttachWorld` (`MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_REGISTRATION`); an absent runtime is never a module error (§ "Failure handling").

### Luau errors versus asserts

- Script misuse of the `XR` namespace (arguments where none are taken, a call from a chunk system) raises SCRIPT's catalogued Luau errors ZS1510 (argument count) and ZS1702 (not available in a chunk system) at the call, never an assert ([Z2-N-CONTRACT-473]).
- Runtime results, scripted-runtime files and configuration values are untrusted: every violation is an error value ([Z2-N-CONTRACT-472]).
- Asserts guard engine preconditions only: a seam call out of order (`Zenith_XrCallGuard`), a call on the wrong thread, destruction of the module with a frame source still registered (`ZENITH_CHECK`).

### Determinism obligations

- Simulation observes XR only through PLAT's input commands (the XR controller devices' transitions, mapped by PLAT's mapper into replay-logged commands) and through the `ASYNC_SOURCE_XR` notifications, every one `PUBLICATION_KIND_GENERIC_RECORDED` (Z2-D-053, Z2-D-019).
- No world resource, component or system reads ZenithXR state; a replay runs with `xr.enabled = false` forced and reproduces every result and tick hash (`Zenith2/Docs/Design/SCOPE/XrInput.md` → "Replay and determinism").
- Eye poses located at render time are presentation state: they change what is drawn, never what is simulated.

## Diagnostics and profiling

### Counters

Names follow [Z2-N-CONTRACT-156]; every bounded queue also has its `.full`, `.dropped` and `.high_water` triple.

| Counter | Kind | Meaning |
|---|---|---|
| `xr.session.state` | gauge | `Zenith_XrSessionState` value |
| `xr.session.starts`, `xr.session.losses`, `xr.instance.losses`, `xr.session.retries` | cumulative | session lifecycle |
| `xr.frames.submitted`, `xr.frames.not_rendered`, `xr.frames.skipped_wait_timeout`, `xr.frames.end_failed` | cumulative | XR frames by outcome |
| `xr.frames.missed_periods` | cumulative | display periods skipped between consecutive predicted display times (`round((t_k − t_{k−1}) / period) − 1`, summed) |
| `xr.frame.wait_us` | gauge | the last `xrWaitFrame` duration |
| `xr.frame.period_ns` | gauge | `predictedDisplayPeriod` |
| `xr.frame.pose_to_display_us` | gauge | predicted display time minus the `xrLocateViews` call time |
| `xr.swapchains.recreated` | cumulative | recreations |
| `xr.layers.quad_dropped` | cumulative | quad layers omitted for the runtime's layer limit |
| `xr.input.syncs`, `xr.input.sync_failures`, `xr.input.profile_changes` | cumulative | input |
| `xr.input.pose_invalid` | cumulative | pose samples without both valid bits |
| `xr.recentre.requests`, `xr.recentre.dropped` | cumulative | recentre |
| `xr.notify.parked`, `xr.notify.full`, `xr.notify.dropped`, `xr.notify.high_water` | cumulative | notifications |
| `xr.memory.swapchain_bytes` | gauge | the memory consumer's report |
| `xr.fallback.mono_frames` | cumulative | frames whose stereo request rendered mono |

### Profiling zones

`Xr.SampleInput` (prologue, before the pump), `Xr.PollEvents` (prologue, after the pump), `Xr.AcquireFrame` (build node), `Xr.BeginFrame` and `Xr.WaitImages` (submit), `Xr.EndFrame` and `Xr.WaitFrame` (present). `Xr.WaitFrame` is excluded from the frame's CPU work in the profiler's frame-time decomposition, as the pacer's wait is.

### Log categories and messages

Categories `xr` (startup, runtime, configuration), `xr.session`, `xr.frame`, `xr.input`. No message carries a user identifier; runtime names and system names are logged at startup as the runtime reports them.

| Category | Level | Message |
|---|---|---|
| `xr` | info | `xr: runtime '<runtimeName>' <major.minor.patch>, system '<systemName>' (vendor 0x<id>), backend <Vulkan or D3D12>` |
| `xr` | warning | `xr: no OpenXR runtime (<XrResult name>); XR disabled, rendering mono` |
| `xr` | warning | `xr: runtime has no head-mounted display (XR_ERROR_FORM_FACTOR_UNAVAILABLE); retrying every <n> s` |
| `xr` | error | `xr: runtime lacks <extension> for the <backend> backend; XR disabled` |
| `xr` | error | `xr: required OpenXR 1.1, runtime offers <version>; XR disabled` |
| `xr.session` | info | `xr.session: <from> -> <to>` |
| `xr.session` | warning | `xr.session: session lost; retrying in <n> s` |
| `xr.session` | warning | `xr.session: instance lost; recreating the instance at the next device recreation` |
| `xr.frame` | warning | `xr.frame: swapchain image wait timed out (<ms> ms); frame <f> skipped` (at most once per second) |
| `xr.frame` | error | `xr.frame: xrEndFrame failed (<XrResult name>)` (at most once per second) |
| `xr.input` | info | `xr.input: interaction profile <hand> = <path>` |
| `xr.input` | warning | `xr.input: binding suggestions for <profile> refused (<XrResult name>)` |

### On-screen profiler rows and panel

The `xr` panel (a provider registered through `GetProfilerOverlay()`, `Zenith2/Docs/Design/DEVT/Overlay.md` → "Panel providers") shows: the runtime and system names, the backend, the session state, the reference space kind, the recommended and actual image extents per eye, the render scale, the display period, the last wait time, the pose-to-display interval, the missed-period count, the bound interaction profile per hand and the last 16 `xr.session` log lines.

### Console commands

Registered in development builds through `GetDevConsole()` (`Zenith2/Docs/Design/DEVT/Console.md` → "Command registry"), first segment `xr`; none changes simulation-visible state directly.

| Command | Effect |
|---|---|
| `xr.status` | prints the panel's values |
| `xr.recentre` | the presentation action `Zenith_XrSystem::RequestRecentre` |
| `xr.renderScale <f>` | writes `xr.renderScale` to the user layer; the swapchains are recreated at the next render-idle point |
| `xr.mirror <none\|leftEye>` | writes `xr.mirror` to the user layer |
| `xr.exit` | `RequestExitSession` (the session stops; the application keeps running mono) |

### Telemetry

The cumulative counters above enter the frame telemetry with every other counter; ZenithXR emits no telemetry of its own.

### Memory and the governor

The module's consumer reports `MEMORY_SUBSYSTEM_RENDERING`: the bytes of the imported swapchain images as the runtime's `XrSwapchainCreateInfo` describe them (width × height × bytes per texel × image count, per swapchain), registered through `Zenith2/Docs/Design/CORE/MemoryGovernor.md` → "Consumer registration". Shedding: at WARNING nothing; at CRITICAL the render scale steps down by 0.1 to at least 0.7 at the next render-idle point (one step per CRITICAL entry), restored when the level returns to NORMAL.

## Authoring and cooking

### What SCOPE authors

SCOPE authors two documents by hand, as JSON and Markdown text: the scope register (`Zenith2/Docs/Scope/ScopeRegister.json`) and the device-run records. ZenithXR has no authoring document, no asset kind and no cook step: XR settings are configuration keys in the game's configuration layer, the suggested bindings are constant data in `Internal/Zenith_XrBindingTable.cpp`, and the game's bindings are PLAT's `*.bindings.json` (`Zenith2/Docs/Design/PLAT/Input.md` → "Bindings file schema"), whose `interactionProfile` key is PLAT's.

### Editor surfaces

The editor's play-in-editor and the Stage 7 platform cook are EDIT's (`Zenith2/Docs/Design/EDIT.md` → "Stage 7"). The XR module runs inside play-in-editor sessions unchanged when the game selects it and `xr.enabled` is true; the editor viewports stay mono (`VIEW_KIND_EDITOR`), and only the play session's camera renders stereo.

### Migration hooks

None: no SCOPE format is an authoring document of `Zenith2/Docs/Contracts.md` → "Compatibility policy per kind". The scope register carries `schemaVersion` 1 and changes only through its check, which refuses an unknown version.

### Console content

Each console's cook target, texture formats and package layout are cooked by ASSET's and PACK's pipelines through the overlay's registration of its platform (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"); the main repository holds no console cook code.

## Failure handling

| Failure | Behaviour |
|---|---|
| no OpenXR runtime (`XR_ERROR_RUNTIME_UNAVAILABLE`, `XR_ERROR_RUNTIME_FAILURE` at `xrCreateInstance`), or the loader absent | XR disabled for the process: the provider reports no interop request, the device is created as without XR, the module starts with `ZENITH_XR_SESSION_STATE_DISABLED`, every stereo camera renders mono through RPROD's fallback (`xr.fallback.mono_frames`), one `xr` warning; exit code unchanged (the provider failure rule of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider") |
| invalid `xr.*` configuration or scripted-runtime document | exit code 2 at startup step 11; an allocation failure of the provider is exit code 4 |
| no headset (`XR_ERROR_FORM_FACTOR_UNAVAILABLE`) | every graphics-requirements call takes the `XrSystemId`, so the device created without a system is not the runtime's; state `RETRY_WAIT`, `xrGetSystem` retried every `xr.retrySeconds` (5) in the prologue; when it succeeds, ZenithXR asks for a device recreation through RHI's recovery path (`Zenith2/Docs/Design/RHI/Recovery.md` → "Recover-or-clean-exit policy"), the provider supplies a fresh interop request for the new device, and the session is created after it (ruling Z2-D-137); on D3D12 no recreation is needed when the existing device already has the runtime's adapter LUID and feature level |
| a required extension or the API version missing | XR disabled, one `xr` error |
| session lost (`XR_SESSION_STATE_LOSS_PENDING`, `XR_ERROR_SESSION_LOST`) | the session is torn down at once, state `RETRY_WAIT`, stereo falls back to mono, a new session is attempted every `xr.retrySeconds` |
| instance lost (`XR_ERROR_INSTANCE_LOST`, `XrEventDataInstanceLossPending`) | the session and instance are destroyed; every `xr.retrySeconds` a new instance and system are tried, and when they exist the device is recreated through the recovery path as for a headset connected after launch; until then XR is disabled with `xr.instance.losses` counted |
| device loss (RHI) | the provider's `OnDeviceDestroying` destroys the session before the lost device's destruction; from `LOST` through `RESUMING` the render subgraph is empty and no XR frame call or frame submission happens; at the device's recreation the provider supplies a fresh interop request and, after `OnDeviceCreated`, the session and swapchains are recreated (ruling Z2-D-137) |
| swapchain image wait timeout | the frame is skipped: no layers are submitted except an `xrEndFrame` with zero layers, `xr.frames.skipped_wait_timeout` + 1 |
| allocation failure | at startup: `MODULE_ERROR_OUT_OF_MEMORY` from `Start`, the runtime unwinds; at a swapchain recreation: the previous swapchains stay in use and the recreation is retried at the next render-idle point |
| a corrupt `zenith2.xr-script` | `ZENITH_XR_ERROR_INVALID_SCRIPT` with the JSON pointer; the scripted runtime is not created and XR is disabled |
| budget breach (XR frame interval or missed periods) | reported by the budget run against the pending rows (§ "Budget rows"); in a session, the render scale is not changed automatically (dynamic resolution is LIGHT's per-view controller, which keeps working per eye) |
| console SDK absent | the platform's targets are declared-but-unavailable (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay"); no gate builds them |

## Test strategy

### Test summary

| Group | Where | Gate step |
|---|---|---|
| register checks | `Zenith2/CMake/Tests/Scope/` | `Test` (label `z2-cmake`); the presence check at stage acceptance (label `z2-acceptance-scope`) |
| readiness and exclusion tests | `Zenith_Integration_ScopeChecks` | `Test` on `win` (WARP and lavapipe), from Stage 2 |
| ZenithXR unit and seam tests against the scripted runtime | `ZenithXR_Tests` | `Test` (label `z2-xr`; PROC's line `Test.ps1 -Label z2-xr` in `Zenith2Render`) |
| fuzzing of the script format | `Zenith_Fuzz_XrScript` | `win-asan`, DEVT's fuzz campaign at acceptance |
| real-runtime integration | `Zenith_Integration_XrRuntime` | `Test` in the windowed GPU configuration; skip code 77 without a runtime |
| device runs | `Zenith2/Docs/Research/Scope/XrDeviceRuns/` | `needs-human`, Stage 7 acceptance |
| console tests and runs | the overlays | each overlay's own gate lines (PROC per platform) and `needs-human` devkit runs |

Every named test and the case it asserts is in `Zenith2/Docs/Design/SCOPE/XrTests.md` and, for the register and readiness checks, `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks" and `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns".

## Budget rows

### Rows SCOPE owns

None yet: every row below is a pending need recorded by cross-doc row X409 (Budgets.md additions, the next free SCOPE row IDs, 001 onward in this order), producer "Stage 7 XR budget ticket for SCOPE" or, for the console rows, "Stage 7 console budget ticket for SCOPE, per platform" (blocked by that platform's SDK-access ticket).

| Pending row | Unit | Procedure | Platform | Target basis |
|---|---|---|---|---|
| XR frame interval p95 | ms | MP-XRFRAME: the frame pacer's observation ring of intervals between `xrEndFrame` returns while `FLUX_PACING_MODE_EXTERNAL` is active (the same hook MP-FRAME reads), defined by the Stage 7 XR budget ticket | Windows PC VR, reference matrix | at most the display period (11.1 ms at 90 Hz) |
| XR missed display periods | % of XR frames | `xr.frames.missed_periods` over a run | as above | at most 1% |
| XR pose-to-display interval p95 | ms | `xr.frame.pose_to_display_us` | as above | the measured value of the reference matrix, adopted by the ticket |
| XR main-thread and build-node CPU | ms per frame | the `Xr.*` zones except `Xr.WaitFrame` | RTX 3070 desktop | adopted by the ticket |
| XR swapchain memory | MB | `xr.memory.swapchain_bytes` | as above | adopted by the ticket |
| console memory, frame rate and package size (three rows per platform) | MB, ms, MB | the platform equivalents of MP-SUBMEM, MP-FRAME and MP-PKG | each console | from the platform's own limits, recorded in the overlay, with only the adopted number in `Budgets.md` |

### Rows SCOPE is measured against

`Z2-B-RPROD-027` (input-to-photon latency: the XR runs report it as their pose-to-display interval beside it, never in its place), `Z2-B-RPROD-020` (pacing deviation: suspended while an XR frame source paces).

## Stage plan

### Stage 1

The scope register and its validity check (pulled into the Stage 1 sprint, fix version Stage 1); the six Stage 1 SCOPE decision tickets (`needs-human`, kind `decision`), each confirming one record of `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` and the rulings Z2-D-133 and Z2-D-136 to Z2-D-140 and the two awaiting the human (SC-R2, SC-R3); the drafting obligations on console and XR tickets (NDA-free, SDK-gated, specified against OpenXR 1.1), checked by the coverage gate step.

### Stage 2

The `ScopeChecks` integration suite and the multi-view readiness checks XRR-V1 to XRR-V3; the readiness register rows of Stage 2.

### Stage 3

The input readiness checks XRR-I1 to XRR-I5, the pacing check XRR-V7, the local-multiplayer exclusion test.

### Stage 4

The world-space UI check XRR-V8.

### Stage 5

The off-centre split-frustum parity check XRR-V5, the output-texture rotation check XRR-V6 and the two-view group check XRR-V4, after RPROD's advanced multi-view and CORE's off-centre builders (rows SP1, SP3).

### Stage 6

Nothing: no SCOPE item is scheduled. The console SDK-access tickets are filed into the Stage 6 sprint with fix version `Z2 Stage 7`, so a platform's access is in hand when Stage 7 starts; their completion gates only their own platform (`Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets"). Carry-over rule: at `sprint complete` of the Stage 6 sprint, an SDK-access ticket that is not Done (still requested, refused and closed `Blocked`, or lapsed) moves to the Stage 7 sprint with its fix version unchanged, and the Stage 6 acceptance's `Coverage.ps1 -Stage 6` ignores it, because its fix version is `Z2 Stage 7`.

### Stage 7

XR, in this order of API-first slices: (1) the ZenithXR library with its public headers, the seam and the scripted runtime (API-only plus the seam, so RPROD's and GAME's stereo tickets can declare against `Flux_StereoFrameSource`'s implementation); (2) the provider and the RHI interop (blocked by RHI's interop slice, row SP2); (3) the session and events; (4) the frame cycle and pacing (blocked by RPROD's frame-source slice, row SP1); (5) swapchains and layers; (6) spaces, recentre and bounds; (7) the input mirror (blocked by PLAT's Stage 7 input ticket); (8) the script surface; (9) diagnostics and the memory consumer; (10) the real-runtime integration and the device runs. Consoles, per platform, after its SDK-access ticket: the overlay and preset, the platform layer, the RHI realisation and shader target (after D-CONSOLE-RHI and D-CONSOLE-SHADERS rule), audio, save, package, online, crash, budgets, the certification map, the devkit runs, PROC's gate line.

### API-only slices

| Slice | Stage | Unblocks |
|---|---|---|
| `Zenith2/Docs/Scope/ScopeRegister.json` with every Stage 1 row and its check | 1 | every readiness row of the stages after it |
| `Zenith_XrTypes.h`, `Zenith_XrErrors.h`, `Zenith_XrSystem.h`, `Zenith_Module_XR.h` with stub definitions returning `ZENITH_XR_ERROR_RUNTIME_UNAVAILABLE` | 7 | RPROD's stereo tickets, GAME's camera tickets, PLAT's device-source consumer, EDIT's XR preview |

## Decisions

### Settled decisions governing SCOPE

| ID | Governs |
|---|---|
| Z2-D-071 | consoles deferred to Stage 7, gated on SDK access |
| Z2-D-072 | VR/XR deferred to Stage 7 via OpenXR; multi-view and input XR-ready |
| Z2-D-073 | networked multiplayer and dedicated servers (NET's tree; SCOPE holds the record) |
| Z2-D-074 | online backends (ONLINE's tree; SCOPE holds the record) |
| Z2-D-075 | modding excluded |
| Z2-D-076 | local multiplayer and split-screen excluded |
| Z2-D-070 | every stage drafted in full detail; deferred trees ticketed now |
| Z2-D-010 | the platforms and the one validated Android device |
| Z2-D-019, Z2-D-053 | XR reaches simulation only through recorded commands and publications |
| Z2-D-024, Z2-D-081 | ZenithXR is a static module of the one game executable |
| Z2-D-082 | the OpenXR loader pinned in the manifest, confined to `Internal/` |
| Z2-D-059 | each eye an independent render view |

### Rulings this design makes

Each ruling below except SC-R2 and SC-R3 is an entry of `Zenith2/Docs/DecisionLog.md` → "Session rulings" under the ID in its first column; SC-R3 keeps its reserved number until the human confirms it; the Stage 1 SCOPE decision tickets confirm them. SC-R2 is not a ruling: it is flagged proposal SCOPE-1 (`C:/dev/Zenith2-drafts/findings/FLAGGED_PROPOSALS.md`), an exception to Z2-D-002 and [Z2-N-ARCH-263] that only the human may approve (pending Q98); number 134 stays reserved for it, and row X401 holds its entry text until then. SC-R3 and the console-headset exclusion wait on the human's confirmation of the XR scope (pending Q99).

| Ruling | Decision | Record |
|---|---|---|
| Z2-D-133 | The console platform set is PlayStation 5, Xbox Series X and S, and Nintendo Switch 2; `game.json` platform values `playstation5`, `xboxseries`, `switch2`; one `needs-human` SDK-access ticket per platform blocks every other ticket of that platform and nothing else | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Consoles record" |
| SC-R2 (flagged proposal SCOPE-1, pending Q98) | proposed: NDA-covered material lives only in a per-platform private overlay mounted at `Zenith2/Platforms/<Platform>/`, ignored by the main repository and denied to loop workers, every overlay ticket `needs-human`; the main repository, its board and its docs hold only neutral seams, neutral behaviour and the overlay's mount rules | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold" |
| SC-R3 (pending Q99) | XR is OpenXR 1.1 through the Khronos loader, Windows PC VR on both RHIs in Stage 7, the opaque environment blend mode only; standalone Android headsets and passthrough are D-XR-PLATFORMS (open, Stage 7); a console's own headset is excluded; this ruling replaces the "OpenXR runtime targets" item of the Roadmap's Stage 7 decisions (cross-doc item X402) | "XR record" |
| Z2-D-136 | XR input reaches PLAT's model through a control-mirror action set: one OpenXR action per XR catalogue control, suggested bindings per interaction profile, PLAT's mapper above it | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "The control-mirror action set" |
| Z2-D-137 | The OpenXR instance and system are created before the RHI device by a factory named by the optional `m_pfnCreateDeviceInterop` member of the XR module's descriptor, called at the end of startup step 11, with the interop request and device check inside step 12; no startup step, no enumerator and no third module entry point is added; a headset connected after launch, an instance loss and a device loss recreate the device through RHI's recovery path; the session is created at module start | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider" |
| Z2-D-138 | ZenithXR's gate tests run against a scripted OpenXR runtime behind the `Zenith_OpenXrApi` seam on WARP and lavapipe; real runtimes run in the windowed GPU configuration with skip code 77; headsets are `needs-human` device runs on the reference matrix | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix" |
| Z2-D-139 | The XR-readiness checks of the scope register bind Stage 2 to Stage 6 tickets: a ticket that touches a check's owner heading keeps the check's test, and the stage-acceptance presence check fails on a missing one | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "How a readiness check binds a ticket" |
| Z2-D-140 | Every scope boundary is a row of the machine-checked scope register with its decision, classification, stage, explicit requirement IDs and enforcing checks | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register" |

### Open decisions SCOPE owns

| Decision | Stage | Question | Alternatives | Acceptance rule |
|---|---|---|---|---|
| D-XR-PLATFORMS (open, Stage 7) | 7 | does Stage 7 add a standalone Android OpenXR headset as a second validated Android device, and passthrough with a non-opaque environment blend mode | (a) neither; (b) a standalone headset (Meta Quest 3 class through the Khronos Android loader) without passthrough; (c) (b) plus `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND` passthrough | (b) or (c) is adopted only when a harness build of WorkloadIndoor runs on the headset at the XR frame-interval row with the S24's Android build unchanged, and a human accepts adding a second validated Android device to Z2-D-010 through a flagged proposal; otherwise (a) |
| D-XR-MULTIVIEW (open, Stage 7) | 7 | single-pass stereo through `VK_KHR_multiview` and D3D12 view instancing, or the two per-eye views of RPROD's stereo group | (a) two views (the Stage 7 baseline); (b) multiview for the opaque and prepass passes only; (c) multiview everywhere it is legal | a harness ticket under `Zenith2/Benchmarks/XrMultiview/` measures W-Indoor per-eye GPU time on the RTX 3070 with each alternative at the same image; (b) or (c) is adopted when it saves at least 15% of the frame with PathParity's stereo images bit-identical to (a); otherwise (a) |
| D-CONSOLE-RHI (open, Stage 7) | 7 | per console, the RHI realisation | Xbox Series: (a) `ZenithRHI_D3D12` with an overlay `Internal/` variant, (b) a separate backend; PlayStation 5 and Switch 2: (c) a separate backend library in the overlay | ruled per platform after its SDK-access ticket, by a needs-human ruling ticket recording the baseline-equivalence check of `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| D-CONSOLE-SHADERS (open, Stage 7) | 7 | per console, the offline shader path | (a) Slang emits the platform's accepted intermediate form and the overlay's compiler step finishes it; (b) the overlay's compiler consumes HLSL that Slang emits | ruled per platform by a harness that compiles every program of the shader set and a ruling ticket that rejects any alternative failing a program or the generated-layout checks |

The four open decisions are filed for `Zenith2/Docs/DecisionLog.md` → "Open decisions" by cross-doc row X408.

## Requirement index

<!-- requirement-index -->
| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-SCOPE-001 | Scope register file | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register" |
| Z2-R-SCOPE-002 | Register validity check | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks" |
| Z2-R-SCOPE-003 | Deferred boundaries carry a requirement tree | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Deferred scopes and their trees"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks" |
| Z2-R-SCOPE-004 | Consoles scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Consoles record"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets" |
| Z2-R-SCOPE-005 | XR scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "XR record"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets" |
| Z2-R-SCOPE-006 | Modding scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Modding record" |
| Z2-R-SCOPE-007 | Local multiplayer scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Local multiplayer record" |
| Z2-R-SCOPE-008 | Dedicated servers scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Dedicated servers record" |
| Z2-R-SCOPE-009 | Online backends scope record | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Online backends record" |
| Z2-R-SCOPE-010 | Register landing ticket | S1 | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Stage 1 SCOPE decision tickets"; `Zenith2/Docs/Design/SCOPE.md` → "Repository files outside ZenithXR" |
| Z2-R-SCOPE-011 | Console tickets carry no NDA material | S1 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold" |
| Z2-R-SCOPE-012 | Console tickets blocked by their platform's SDK-access ticket | S1 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-R-SCOPE-013 | XR tickets specified against OpenXR 1.1 | S1 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam" |
| Z2-R-SCOPE-014 | SDK access gates only its own platform | S1 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console platform set"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-R-SCOPE-015 | Modding excluded | — | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Modding record" |
| Z2-R-SCOPE-016 | Local multiplayer and split-screen excluded | — | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Local multiplayer record" |
| Z2-R-SCOPE-017 | Console headsets excluded | — | `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "XR record"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json" |
| Z2-R-SCOPE-018 | Standalone headsets and passthrough ruled by D-XR-PLATFORMS | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Open decisions SCOPE owns" |
| Z2-R-SCOPE-019 | Readiness register rows and presence check | S2 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "How a readiness check binds a ticket"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks" |
| Z2-R-SCOPE-020 | XRR-V1 stereo kind reserved | S2 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-R-SCOPE-021 | XRR-V2 independent view histories | S2 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-R-SCOPE-022 | XRR-V3 distinct stable eye view IDs | S2 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" |
| Z2-R-SCOPE-023 | XRR-V4 two-view group without cross-talk | S5 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-R-SCOPE-024 | XRR-V5 off-centre projection parity | S5 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" |
| Z2-R-SCOPE-025 | XRR-V6 output texture rotation | S5 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" |
| Z2-R-SCOPE-026 | XRR-V7 one pacing wait bounds run-ahead | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-R-SCOPE-027 | XRR-V8 world-space panels | S4 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Multi-view checks" |
| Z2-R-SCOPE-028 | XRR-O1 UI composite into a supplied target | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Other subsystems XR touches"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" |
| Z2-R-SCOPE-029 | XRR-I1 scripts read actions only | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks" |
| Z2-R-SCOPE-030 | XRR-I2 XR control paths reserved | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks" |
| Z2-R-SCOPE-031 | XRR-I3 POSE action type reserved | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks"; `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" |
| Z2-R-SCOPE-032 | XRR-I4 runtime-created devices | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks" |
| Z2-R-SCOPE-033 | XRR-I5 simulation haptics through commands | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Input checks" |
| Z2-R-SCOPE-034 | Local devices share player slot zero | S3 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Exclusion checks SCOPE owns" |
| Z2-R-SCOPE-035 | Readiness retirement at Stage 7 | S7 | `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Retirement at Stage 7" |
| Z2-R-SCOPE-036 | ZenithXR module library | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Targets"; `Zenith2/Docs/Design/SCOPE.md` → "ZenithXR files"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Header list" |
| Z2-R-SCOPE-037 | OpenXR loader dependency | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam" |
| Z2-R-SCOPE-038 | XR on Windows only | S7 | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header" |
| Z2-R-SCOPE-039 | Device-interop provider before the device | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Interfaces of lower libraries" |
| Z2-R-SCOPE-040 | Missing runtime falls back to mono | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Failure handling"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider" |
| Z2-R-SCOPE-041 | Startup unwinds every step | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider" |
| Z2-R-SCOPE-042 | Instance creation against OpenXR 1.1 | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions" |
| Z2-R-SCOPE-043 | Head-mounted stereo system | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider" |
| Z2-R-SCOPE-044 | Vulkan graphics binding | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings" |
| Z2-R-SCOPE-045 | D3D12 graphics binding | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings" |
| Z2-R-SCOPE-046 | Graphics queue rule | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Graphics bindings"; `Zenith2/Docs/Design/SCOPE.md` → "Frame placement" |
| Z2-R-SCOPE-047 | Session state machine | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine" |
| Z2-R-SCOPE-048 | Bounded event polling | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Event handling" |
| Z2-R-SCOPE-049 | Focus loss pauses worlds | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine" |
| Z2-R-SCOPE-050 | Session loss retries | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown" |
| Z2-R-SCOPE-051 | Instance loss recreates instance then device | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown" |
| Z2-R-SCOPE-052 | Orderly XR shutdown | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"; `Zenith2/Docs/Design/SCOPE.md` → "Startup and shutdown" |
| Z2-R-SCOPE-053 | Result mapping | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Result mapping" |
| Z2-R-SCOPE-054 | Frame cycle placement | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"; `Zenith2/Docs/Design/SCOPE.md` → "Frame placement" |
| Z2-R-SCOPE-055 | Display time from the waited frame | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle" |
| Z2-R-SCOPE-056 | Not-rendered and skipped frames | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle" |
| Z2-R-SCOPE-057 | Runtime paces the frame loop | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Pacing and the desktop mirror" |
| Z2-R-SCOPE-058 | Late-latched eye poses | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle" |
| Z2-R-SCOPE-059 | Device loss recreates the session | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle"; `Zenith2/Docs/Design/SCOPE.md` → "Failure handling" |
| Z2-R-SCOPE-060 | Desktop mirror | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Pacing and the desktop mirror" |
| Z2-R-SCOPE-061 | Swapchain format preference | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains" |
| Z2-R-SCOPE-062 | Swapchain extent | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains" |
| Z2-R-SCOPE-063 | Images imported as externally owned textures | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains" |
| Z2-R-SCOPE-064 | Swapchain recreation | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Swapchains"; `Zenith2/Docs/Design/SCOPE.md` → "Memory and the governor" |
| Z2-R-SCOPE-065 | Projection layer | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers" |
| Z2-R-SCOPE-066 | Depth submission | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers" |
| Z2-R-SCOPE-067 | UI quad layer | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Composition layers" |
| Z2-R-SCOPE-068 | Reference space choice | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Reference spaces" |
| Z2-R-SCOPE-069 | Recentre | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Recentre and play area" |
| Z2-R-SCOPE-070 | Play area | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Recentre and play area" |
| Z2-R-SCOPE-071 | Control-mirror action set | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "The control-mirror action set"; `Zenith2/Docs/Design/SCOPE/XrInput.md` → "XR control catalogue" |
| Z2-R-SCOPE-072 | Suggested bindings per profile | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings" |
| Z2-R-SCOPE-073 | Controls into XR controller devices | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Per-frame input flow" |
| Z2-R-SCOPE-074 | Pose sampling for simulation | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Poses for simulation" |
| Z2-R-SCOPE-075 | Interaction profile changes | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Interaction profile changes" |
| Z2-R-SCOPE-076 | Controller haptics | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Haptic output" |
| Z2-R-SCOPE-077 | Replay without a runtime | S7 | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Replay and determinism"; `Zenith2/Docs/Design/SCOPE.md` → "Determinism obligations" |
| Z2-R-SCOPE-078 | XR Luau namespace | S7 | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Script-facing API" |
| Z2-R-SCOPE-079 | XR notifications as recorded publications | S7 | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Event types" |
| Z2-R-SCOPE-080 | XR misuse raises SCRIPT generic codes | S7 | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Misuse codes" |
| Z2-R-SCOPE-081 | XR configuration keys | S7 | `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Configuration keys" |
| Z2-R-SCOPE-082 | XR counters and zones | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Counters"; `Zenith2/Docs/Design/SCOPE.md` → "Profiling zones" |
| Z2-R-SCOPE-083 | XR log lines, panel and console commands | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Log categories and messages"; `Zenith2/Docs/Design/SCOPE.md` → "Console commands" |
| Z2-R-SCOPE-084 | XR memory consumer | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Memory and the governor" |
| Z2-R-SCOPE-085 | OpenXR API seam | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam" |
| Z2-R-SCOPE-086 | Scripted runtime format | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime" |
| Z2-R-SCOPE-087 | Script format fuzzing | S7 | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Format and fuzz tests" |
| Z2-R-SCOPE-088 | Gate tests on the scripted runtime | S7 | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Test targets"; `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Gate steps per test group" |
| Z2-R-SCOPE-089 | Real-runtime integration | S7 | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Real-runtime integration" |
| Z2-R-SCOPE-090 | Reference device matrix runs | S7 | `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix"; `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Device runs" |
| Z2-R-SCOPE-091 | XR budget rows | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Rows SCOPE owns" |
| Z2-R-SCOPE-092 | Single-pass multiview ruled by D-XR-MULTIVIEW | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Open decisions SCOPE owns" |
| Z2-R-SCOPE-093 | PlayStation 5 SDK-access ticket | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-R-SCOPE-094 | Xbox Series SDK-access ticket | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-R-SCOPE-095 | Nintendo Switch 2 SDK-access ticket | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "SDK-access tickets" |
| Z2-R-SCOPE-096 | Private platform overlay | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay" |
| Z2-R-SCOPE-097 | Console presets and matrix macros | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json" |
| Z2-R-SCOPE-098 | Console platform values in game.json | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json" |
| Z2-R-SCOPE-099 | Console platform layer | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-100 | Console input devices | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-101 | Console filesystem and save storage | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-102 | Console accounts | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-103 | Console RHI realisation | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-104 | Console shader target | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-105 | Console audio device | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-106 | Console crash capture | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-107 | Console memory and budgets | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements"; `Zenith2/Docs/Design/SCOPE.md` → "Rows SCOPE owns" |
| Z2-R-SCOPE-108 | Console packaging | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-109 | Console online services | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-110 | Console cook target | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements" |
| Z2-R-SCOPE-111 | Console certification map | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Certification mapping" |
| Z2-R-SCOPE-112 | Console gate line | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console tests and device runs" |
| Z2-R-SCOPE-113 | Console devkit runs | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console tests and device runs" |
| Z2-R-SCOPE-114 | Console seams exist before Stage 7 | S6 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console readiness today" |
| Z2-R-SCOPE-115 | SCOPE Stage 7 acceptance evidence | S7 | `Zenith2/Docs/Design/SCOPE.md` → "Stage 7"; `Zenith2/Docs/Design/SCOPE.md` → "Test summary" |
| Z2-R-SCOPE-116 | Headset after launch recreates the device | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Loss recovery and shutdown"; `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine" |
| Z2-R-SCOPE-117 | Console build line exits 3 without its overlay | S7 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console build presets and game.json"; `Zenith2/Docs/Design/SCOPE/Consoles.md` → "The platform overlay" |
| Z2-R-SCOPE-118 | Console tickets are human-only | S1 | `Zenith2/Docs/Design/SCOPE/Consoles.md` → "What the main repository may hold" |
| Z2-R-SCOPE-119 | Scripted runtime observability | S7 | `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_XrTestAccess header" |
