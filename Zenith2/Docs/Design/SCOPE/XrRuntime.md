# SCOPE design: the OpenXR runtime integration

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It specifies, against OpenXR 1.1, how ZenithXR creates and drives the OpenXR instance, system and session, the frame cycle and its pacing, swapchains, composition layers, reference spaces, recentring and play-area bounds, loss recovery, the mapping of every `XrResult`, and the scripted runtime the gate tests use.

## Scope of this file

Every OpenXR function, structure and enumerant named here is the OpenXR 1.1 specification's. Code lives in `Zenith2/Engine/ZenithXR/Internal/` and, for the loader-backed seam, `Internal/Windows/`. Input is `Zenith2/Docs/Design/SCOPE/XrInput.md`; tests are `Zenith2/Docs/Design/SCOPE/XrTests.md`.

## OpenXR version and extensions

`XrInstanceCreateInfo::applicationInfo`: `applicationName` the game's `name`, `applicationVersion` its `version` packed as `major << 16 | minor << 8 | patch` (each clamped to 255 for the minor and patch bytes), `engineName` `"Zenith2"`, `engineVersion` the engine's build version in the same packing, `apiVersion` `XR_MAKE_VERSION(1, 1, 0)`. A runtime that answers `XR_ERROR_API_VERSION_UNSUPPORTED` disables XR with `ZENITH_XR_ERROR_API_VERSION_UNSUPPORTED`; no 1.0 fallback exists (ruling SC-R3, pending Q99). Every runtime of the reference device matrix (`Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix") must report 1.1 at the version the device runs pin: each device-run record and the `xr` startup log line state the reported version, and a matrix runtime reporting 1.0 is raised against Q99 rather than handled by a fallback.

Extensions, enumerated with `xrEnumerateInstanceExtensionProperties` before creation:

| Extension | Need | Use |
|---|---|---|
| `XR_KHR_vulkan_enable2` | required when `rhi.backend` is Vulkan | instance and device creation through the runtime, the Vulkan graphics binding, Vulkan swapchain images |
| `XR_KHR_D3D12_enable` | required when `rhi.backend` is D3D12 | adapter requirements, the D3D12 binding and images |
| `XR_KHR_win32_convert_performance_counter_time` | required | `XrTime` ↔ the platform's monotonic clock (`QueryPerformanceCounter` domain) for input timestamps and diagnostics |
| `XR_KHR_composition_layer_depth` | optional | depth submission (`xr.submitDepth`) |
| `XR_EXT_debug_utils` | optional, development builds | runtime messages routed to the `xr` log category, labels on the session |

Only the extension of the selected backend is enabled. The **required list** is, in this order: index 0 the selected backend's extension (`XR_KHR_vulkan_enable2` or `XR_KHR_D3D12_enable`), index 1 `XR_KHR_win32_convert_performance_counter_time`. A missing backend extension disables XR with `ZENITH_XR_ERROR_BACKEND_UNSUPPORTED`; a missing index-1 extension with `ZENITH_XR_ERROR_EXTENSION_MISSING` and `m_uDetail` 1. The optional extensions are enabled when offered and never fail. No other extension is enabled in Stage 7.

## The OpenXR API seam

`Internal/Zenith_OpenXrApi.h` declares the abstract class `Zenith_OpenXrApi` with one pure virtual member per OpenXR entry point ZenithXR calls, named as the entry point without its `xr` prefix and taking and returning the mirrored structures of `Internal/Zenith_OpenXrTypes.h` (plain Zenith structs with the OpenXR fields ZenithXR reads or writes, handles as `uint64`, results as `sint32` with the OpenXR values; only the loader-backed implementation below names OpenXR's own types, `XrResult`, `XrTime` (`int64_t`), `XrDuration` and the handle types, converting at each call):

`EnumerateInstanceExtensionProperties`, `CreateInstance`, `DestroyInstance`, `GetInstanceProperties`, `GetSystem`, `GetSystemProperties`, `EnumerateViewConfigurations`, `EnumerateViewConfigurationViews`, `EnumerateEnvironmentBlendModes`, `GetVulkanGraphicsRequirements2KHR`, `CreateVulkanInstanceKHR`, `GetVulkanGraphicsDevice2KHR`, `CreateVulkanDeviceKHR`, `GetD3D12GraphicsRequirementsKHR`, `CreateSession`, `DestroySession`, `BeginSession`, `EndSession`, `RequestExitSession`, `PollEvent`, `EnumerateReferenceSpaces`, `CreateReferenceSpace`, `DestroySpace`, `LocateSpace`, `GetReferenceSpaceBoundsRect`, `EnumerateSwapchainFormats`, `CreateSwapchain`, `DestroySwapchain`, `EnumerateSwapchainImages`, `AcquireSwapchainImage`, `WaitSwapchainImage`, `ReleaseSwapchainImage`, `WaitFrame`, `BeginFrame`, `EndFrame`, `LocateViews`, `StringToPath`, `PathToString`, `CreateActionSet`, `DestroyActionSet`, `CreateAction`, `SuggestInteractionProfileBindings`, `AttachSessionActionSets`, `CreateActionSpace`, `SyncActions`, `GetActionStateBoolean`, `GetActionStateFloat`, `GetActionStateVector2f`, `GetActionStatePose`, `GetCurrentInteractionProfile`, `ApplyHapticFeedback`, `StopHapticFeedback`, `ConvertTimeToWin32PerformanceCounterKHR`, `ConvertWin32PerformanceCounterToTimeKHR`, `ResultToString`.

| Implementation | Where | Selected by |
|---|---|---|
| `Zenith_OpenXrApiLoader` | `Internal/Windows/Zenith_OpenXrApiLoader_Windows.cpp`, the one TU that includes `<openxr/openxr.h>` and `<openxr/openxr_platform.h>`; it calls the loader's exported core functions directly and resolves extension functions once through `xrGetInstanceProcAddr` after `xrCreateInstance` | `xr.runtime = openxr` |
| `Zenith_OpenXrApiScripted` | `Internal/Zenith_OpenXrApiScripted.cpp` (§ "Scripted runtime") | `xr.runtime = scripted`, non-Shipping builds |

`Zenith_XrCallGuard` wraps every seam call in debug builds: an atomic word holds the calling context (`MAIN_PROLOGUE`, `BUILD_NODE`, `RENDER_SUBMIT`, `STARTUP`, `SHUTDOWN`); entering while another context holds it is `ZENITH_ASSERT`, so a structural change that let two contexts overlap fails the first test that exercises it. The loader is linked through its import library and staged beside the executable (`openxr_loader.dll`, row SP9), so no Zenith2 code calls `LoadLibrary*` ([Z2-N-ARCH-262], [Z2-N-ARCH-274] stand: the loader library loads the active runtime as the Vulkan loader loads drivers).

## Startup and the device-interop provider

Ruling Z2-D-137: the provider half runs as the last action of startup step 11 and inside step 12; the session half at module start (step 15). No startup step and no `Zenith_StartupStep` enumerator is added: `uZENITH_STARTUP_STEP_COUNT` stays 19, the provider's time is part of step 11's and step 12's `m_aulStepMicroseconds` entries, and its unwind is part of those steps' unwind (the X307 second form; cross-doc item X411). The factory is the optional member `m_pfnCreateDeviceInterop` of the XR module's own `Zenith_ModuleDescriptor` (nullptr for every other module), so ZenithXR exports no third module entry point and [Z2-N-ARCH-848] and [Z2-N-ARCH-261] stand unamended: the composition root still reaches a module only through what its generated descriptor names (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header").

| Step | Where | Action | On failure |
|---|---|---|---|
| P1 | 11 (last action) | for the one descriptor whose `m_pfnCreateDeviceInterop` is non-null (two such descriptors are a composition-validation defect, exit code 3), call it: read `xr.enabled`, `xr.runtime`, `xr.script`; headless, `--replay` or `xr.enabled = false` → a disabled provider, stop | invalid key value: `ZENITH_XR_ERROR_INVALID_CONFIG` logged, `DEVICE_INTEROP_ERROR_CONFIGURATION`, exit code 2 |
| P2 | 11 | create the seam (loader or scripted; the scripted runtime parses `xr.script`) | `ZENITH_XR_ERROR_INVALID_SCRIPT` logged, `DEVICE_INTEROP_ERROR_CONFIGURATION`, exit code 2; allocation failure: `DEVICE_INTEROP_ERROR_OUT_OF_MEMORY`, exit code 4 |
| P3 | 11 | enumerate extensions; check the required list of § "OpenXR version and extensions" for `m_szRHIBackendName` | disabled provider, logged |
| P4 | 11 | `xrCreateInstance` | `XR_ERROR_RUNTIME_UNAVAILABLE`, `XR_ERROR_RUNTIME_FAILURE`: disabled (`RUNTIME_UNAVAILABLE`); `XR_ERROR_API_VERSION_UNSUPPORTED`: disabled |
| P5 | 11 | `xrGetSystem(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY)`; `xrGetSystemProperties` (logged) | `XR_ERROR_FORM_FACTOR_UNAVAILABLE`: provider enabled with no system; P6 to P9 are skipped, the device is created as without XR, and the module starts in `RETRY_WAIT` (§ "Loss recovery and shutdown", "No headset at launch") |
| P6 | 11 | `xrEnumerateViewConfigurations` contains `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO`; `xrEnumerateViewConfigurationViews` returns exactly 2; `xrEnumerateEnvironmentBlendModes` contains `XR_ENVIRONMENT_BLEND_MODE_OPAQUE` | disabled (`VIEW_CONFIGURATION`) |
| P7 | 11 | graphics requirements for the system (§ "Graphics bindings"; every one of these calls takes the `XrSystemId`) | disabled (`GRAPHICS_REQUIREMENTS`) |
| P8 | 12, before the factory call | `FillInteropRequest`: the callbacks or the adapter LUID; an empty request when disabled or without a system | — |
| P9 | 12, after the factory returned | `OnDeviceCreated`: read `GetInteropHandles`, check the Vulkan device is the one the runtime named, or the D3D12 adapter LUID matches | disabled, logged |
| S1 | 15 | `xrCreateSession` with the binding of § "Graphics bindings" (skipped while `RETRY_WAIT`) | `RETRY_WAIT` (`SESSION_CREATE`) |
| S2 | 15 | reference spaces (§ "Reference spaces"), the view space | `RETRY_WAIT` (`SPACE`) |
| S3 | 15 | the mirror action set, its actions, the suggested bindings, `xrAttachSessionActionSets` (`Zenith2/Docs/Design/SCOPE/XrInput.md` → "The control-mirror action set") | `RETRY_WAIT` (`ACTION_SETUP`) |
| S4 | 15 | swapchains (§ "Swapchains") | `RETRY_WAIT` (`SWAPCHAIN_*`) |
| S5 | 15 | state `IDLE`; the first `READY` arrives through events | — |

**Provider failure rule, stated once.** A configuration or scripted-runtime error (`INVALID_CONFIG`, `INVALID_SCRIPT`) is exit code 2 at step 11; an allocation failure of the provider is exit code 4 at step 11; an absent loader, an absent runtime, an absent headset, a missing extension, an unsupported version or a refused requirement never fails startup: the provider is disabled (or, without a headset, waiting) and the device is created as without XR. Every other text of this family states the same rule by reference to this paragraph.

A step that fails unwinds every handle the steps before it created in reverse order, and each step is a fault-injection site: P2 to P9 consult `Zenith_DeviceInteropContext::m_pxFaults` (the system does not exist yet), S1 to S4 and P8/P9 at a recreation consult `Zenith_XrTestAccess::FailNextStep` (`Test_XrSession_CreateFailsAtStep_Unwinds`). An allocation failure at a session step S1 to S4 is `ZENITH_XR_ERROR_OUT_OF_MEMORY`, which `Start` returns as `MODULE_ERROR_OUT_OF_MEMORY`.

**Provider to system path.** The provider (startup step 11) exists before the system (step 15) and outlives it (step 9 versus step 6), so it reaches the session half through `Internal/Zenith_XrDeviceListener.h`: the system registers itself as the provider's one device listener at step S1 and clears the registration in `BeginShutdown`; the provider's `OnDeviceDestroying` calls the listener first (the system destroys its swapchains, imports first, then the session), and `OnDeviceCreated` passes the new `Zenith_RHIDevice&` to the listener, which replaces its device pointer before running S1 to S4. With no listener registered (before step 15, after step 6) the provider acts on its own half only.

**Shutdown placement (cross-doc item X411).** The session half is destroyed with the module instance in shutdown step 6 (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Shutdown ownership order"). The provider outlives the device: in shutdown step 9 the runtime calls `OnDeviceDestroying` before it destroys the device and `OnDeviceDestroyed` after it, then destroys the provider, whose destructor calls `xrDestroyInstance`. So no code the runtime injected into a Vulkan device it created through `xrCreateVulkanDeviceKHR` runs after the runtime is unloaded.

## Graphics bindings

**Vulkan.** P7 calls `xrGetVulkanGraphicsRequirements2KHR`; the RHI's Vulkan API version (1.3) must lie in `[minApiVersionSupported, maxApiVersionSupported]` by major and minor. P8 fills `m_pfnCreateVulkanInstance` (calls `xrCreateVulkanInstanceKHR` with the RHI's `VkInstanceCreateInfo` unchanged and `vkGetInstanceProcAddr` from volk; a `VkResult` failure returns `ZENITH_RHI_ERROR_DEVICE_CREATE`), `m_pfnSelectVulkanPhysicalDevice` (`xrGetVulkanGraphicsDevice2KHR`; the RHI uses that device and refuses startup only if it fails the baseline profile, which disables XR and re-runs its own selection instead) and `m_pfnCreateVulkanDevice` (`xrCreateVulkanDeviceKHR` with the device create info vpCreateDevice built). The session binding is `XrGraphicsBindingVulkan2KHR { instance, physicalDevice, device, queueFamilyIndex = graphics family, queueIndex = 0 }` from `GetInteropHandles`.

**D3D12.** P7 calls `xrGetD3D12GraphicsRequirementsKHR`; P8 passes `adapterLuid` and `minFeatureLevel`; the RHI selects the adapter with that LUID (overriding `rhi.adapter`, logged) and refuses it only below the baseline. The binding is `XrGraphicsBindingD3D12KHR { device, queue = the graphics queue }`.

**Queue rule.** OpenXR may submit to the bound queue inside `xrEndFrame`, `xrWaitSwapchainImage` and `xrReleaseSwapchainImage`; all three run on the render-submit thread, which alone submits to the graphics queue, so no external lock is needed. The async-compute and copy queues are never bound.

## Session state machine

| State | Event | Next | Action |
|---|---|---|---|
| `DISABLED` | any | `DISABLED` | none; stereo requests render mono |
| `NO_SESSION` | module start or a device recreation succeeded steps S1 to S4 | `IDLE` | the runtime delivers `XR_SESSION_STATE_IDLE` first, then READY |
| `IDLE` | `XrEventDataSessionStateChanged` READY | `READY` | `xrBeginSession(PRIMARY_STEREO)` in the same prologue; the first `xrWaitFrame` is issued at the next `Flux.Present` |
| `IDLE` | `xr.sessionReadyTimeoutMs` elapsed without READY | `IDLE` | logged once; keeps waiting (a runtime may hold IDLE while the headset is off) |
| `READY` | `xrBeginSession` succeeded | `READY` | `PacesFrames()` true |
| `READY`, `SYNCHRONIZED`, `VISIBLE`, `FOCUSED` | state event to any of these | that state | `SYNCHRONIZED`: frames submitted, no layers shown by the runtime; `VISIBLE`: layers shown, input not focused (actions report inactive); `FOCUSED`: input active |
| `FOCUSED` | leaves FOCUSED | the new state | with `xr.pauseWhenUnfocused`, add `WORLD_PAUSE_REASON_XR_UNFOCUSED` to every LIVE world |
| any running state | enters FOCUSED | `FOCUSED` | remove `WORLD_PAUSE_REASON_XR_UNFOCUSED` |
| any running state | STOPPING | `STOPPING` | stop the frame cycle after the current `xrEndFrame`; `xrEndSession` in the same prologue; `PacesFrames()` false |
| `STOPPING` | `xrEndSession` succeeded | `IDLE` | — |
| any session state | LOSS_PENDING or `XR_ERROR_SESSION_LOST` from any call | `RETRY_WAIT` | destroy swapchains (imports first), spaces, actions, session; remove the pause reason; destroy the XR controller devices (releases synthesised) |
| any session state | `OnDeviceDestroying` (a device loss, or a recreation XR requested) | `NO_SESSION` | destroy the session and its objects as for loss, before the device's destruction; stereo renders mono once rendering resumes; no XR frame call is made while the render subgraph is empty |
| `NO_SESSION` | `OnDeviceCreated` of the recreated device | `NO_SESSION` → `IDLE` | step P9, then steps S1 to S4 in the same prologue |
| any | EXITING | `EXITING` | destroy the session as for loss; when the exit was requested by the application (`RequestExitSession`), stay `EXITING` and render mono; when the runtime asked to exit (the user quit from the runtime's menu), additionally request the process shutdown through the runtime's shutdown request (`Zenith2/Docs/Architecture/CompositionRoot.md` → "The frame loop", step 4) |
| `RETRY_WAIT` with a system | `xr.retrySeconds` elapsed | `NO_SESSION` | steps S1 to S4 |
| `RETRY_WAIT` without a system | `xr.retrySeconds` elapsed and `xrGetSystem` now succeeds | `RECREATING_DEVICE` | steps P6 and P7; on D3D12, when the existing device's adapter LUID equals the runtime's and its feature level is at least `minFeatureLevel`, no recreation is needed: P9 and S1 to S4 run at once (next row skipped); otherwise `Flux_Renderer::RequestDeviceRecreation(FLUX_DEVICE_RECREATION_REASON_XR)` (§ "Loss recovery and shutdown"); a `false` return (the renderer is not in `NORMAL`) leaves the state unchanged and the request is repeated at the next prologue |
| `RECREATING_DEVICE` | `OnDeviceCreated` | `NO_SESSION` → `IDLE` | step P9, steps S1 to S4 |
| `RECREATING_DEVICE` | the renderer's recreation fails | process exit | RHI's recovery policy: `EXITING`, `EXIT_CODE_DEVICE_LOST` (`Zenith2/Docs/Design/RHI/Recovery.md` → "Recover-or-clean-exit policy") |
| any | instance loss | `RETRY_WAIT` without a system | § "Loss recovery and shutdown" |

`RECREATING_DEVICE` is a ZenithXR state reported by `GetStatus` as `ZENITH_XR_SESSION_STATE_RETRY_WAIT` (the public enumeration is unchanged). Each transition logs `xr.session: <from> -> <to>` and publishes `XR.SessionChanged` to every world.

## Event handling

`PumpAfterPlatform` calls `xrPollEvent` into one `XrEventDataBuffer` until `XR_EVENT_UNAVAILABLE`, at most 64 events per prologue (the rest wait for the next prologue, counted `xr.events.deferred`):

| Event | Handling |
|---|---|
| `XrEventDataSessionStateChanged` | the state machine above |
| `XrEventDataInstanceLossPending` | § "Loss recovery and shutdown" |
| `XrEventDataReferenceSpaceChangePending` | for the active reference space: recompute the recentre offset base and re-query bounds; for the stage space, publish `XR.PlayAreaChanged` |
| `XrEventDataInteractionProfileChanged` | `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Interaction profile changes" |
| `XrEventDataEventsLost` | logged with its count, counted `xr.events.lost`; no state is inferred |
| any other type | ignored, counted `xr.events.ignored` |

## Frame cycle

One XR frame per engine frame while `PacesFrames()` is true. Frame k is built in iteration k + 1 (pipelining, Z2-D-050):

| Phase | Where | Calls | Outcome |
|---|---|---|---|
| W(k) | `Flux.Present` of frame k − 1 (render-submit) | `xrWaitFrame` → `XrFrameState { predictedDisplayTime, predictedDisplayPeriod, shouldRender }` stored in the slot of frame k | the loop's pacing wait |
| A(k) | `Flux.BuildGraph` of frame k | `xrLocateViews(PRIMARY_STEREO, predictedDisplayTime, the active space)`; both view poses must carry `XR_VIEW_STATE_POSITION_VALID_BIT` and `ORIENTATION_VALID_BIT`; `xrAcquireSwapchainImage` on each eye's colour (and depth) swapchain | fills `Flux_XrViewPoses`; returns false when `shouldRender` is false, a pose is invalid or acquire failed (stereo falls back to mono this frame; the acquired images, if any, are released empty at E(k)) |
| B(k) | `Flux.Submit` of frame k | `xrBeginFrame`; `xrWaitSwapchainImage(timeout)` for each acquired image | false on `XR_TIMEOUT_EXPIRED`: the frame's eye passes are culled by the renderer and E(k) submits zero layers |
| E(k) | `Flux.Present` of frame k | `xrReleaseSwapchainImage` for each acquired image; `xrEndFrame { displayTime = predictedDisplayTime, environmentBlendMode = OPAQUE, layers }` (§ "Composition layers"); then W(k + 1) | — |

- The first W is issued at the first `Flux.Present` after `xrBeginSession`; before it, frames render mono.
- `xrBeginFrame` returning `XR_FRAME_DISCARDED` means the previous frame was discarded by the runtime: counted `xr.frames.discarded`, the frame proceeds.
- A failed `xrEndFrame` other than by loss is `ZENITH_XR_ERROR_FRAME_CALL`, logged at most once per second, counted; the cycle continues with W(k + 1).
- When `shouldRender` is false, B(k) and E(k) still run with no acquired images and zero layers, as OpenXR requires a begin and end for each waited frame.
- A frame whose slot holds no `XrFrameState` (the frames after `xrBeginSession` and before the first W, and any frame after the session ended) skips A, B and E entirely, because `xrBeginFrame` without a preceding `xrWaitFrame` is `XR_ERROR_CALL_ORDER_INVALID`; it renders mono.
- Device loss: from `LOST` through `RESUMING` the frame loop runs with `bRunRender` false ([Z2-N-ARCH-880]), so no `Flux.BuildGraph`, `Flux.Submit` or `Flux.Present` node runs and ZenithXR makes no frame call and submits no frame; the session was already destroyed by `OnDeviceDestroying`. The loop's pacing during that interval is the engine's own (the empty render subgraph), not the runtime's; the runtime shows its own not-responding state. Rendering resumes mono, and stereo once the session recreated after `OnDeviceCreated` reaches READY.

## Pacing and the desktop mirror

While `PacesFrames()` is true the frame pacer runs in `FLUX_PACING_MODE_EXTERNAL` (row SP1): its desired-time computation, swap-interval control and CPU wait are skipped, its observation ring records the intervals between `xrEndFrame` calls, and `W(k + 1)` is the frame loop's one pacing wait (XRR-V7). The latency slot wait (pacer step 6) stays, so run-ahead stays bounded by `r.framesInFlight`.

The desktop window keeps its swapchain. With `xr.mirror = leftEye` the present pass copies the left eye's colour image, scaled to fit with letterboxing, into the window swapchain before `Flux.Present` (RPROD's mirror copy, row SP1), presenting with the window's present mode forced to IMMEDIATE or MAILBOX so the desktop present never waits on the desktop's refresh; with `none` the window shows the cleared colour. The mirror never adds a render of the scene.

## Swapchains

**Formats.** `xrEnumerateSwapchainFormats`, then the first offered format of the preference list:

| Use | Vulkan | D3D12 |
|---|---|---|
| eye colour | `VK_FORMAT_R8G8B8A8_SRGB`, `VK_FORMAT_B8G8R8A8_SRGB` | `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`, `DXGI_FORMAT_B8G8R8A8_UNORM_SRGB` |
| eye depth | `VK_FORMAT_D32_SFLOAT`, `VK_FORMAT_D24_UNORM_S8_UINT` | `DXGI_FORMAT_D32_FLOAT`, `DXGI_FORMAT_D24_UNORM_S8_UINT` |
| UI quad | as eye colour | as eye colour |

No colour format offered: `ZENITH_XR_ERROR_SWAPCHAIN_FORMAT`; no depth format: depth submission off, logged. The eye view's output transform is the SDR path of the main view, encoding through the sRGB format.

**Sizes.** Per eye, `w = min(round(recommendedImageRectWidth × xr.renderScale), maxImageRectWidth)` and the same for height, each at least 64. One swapchain per eye, `arraySize` 1, `mipCount` 1, `faceCount` 1, `sampleCount` 1 (no MSAA in any path), usage colour attachment and sampled (colour) or depth-stencil attachment (depth). The UI quad swapchain has the UI canvas's extent.

**Import.** After `xrEnumerateSwapchainImages` (at most 8 images; more is refused with `ZENITH_XR_ERROR_SWAPCHAIN_CREATE`), each image is imported once through `Zenith_RHIDevice::ImportExternalTexture` with the release state of OpenXR's rule (colour attachment, depth write) and the debug name `xr.<eye>.<colour|depth>.<i>`; the handles live until the swapchain is destroyed, which first destroys them through the device's deferred deletion with the last frame's queue values.

**Recreation.** At a render-idle point of the prologue (`Zenith2/Docs/Design/RHI.md` → "Render-idle points"), when `xr.renderScale` changed, the recommended size changed (re-read on every session start), or the governor's CRITICAL step lowered the scale: new swapchains are created first; on success the old ones are destroyed; on failure the old ones stay and the attempt repeats next prologue, counted `xr.swapchains.recreate_failed`.

## Composition layers

| Order | Layer | Content | Condition |
|---|---|---|---|
| 1 | `XrCompositionLayerProjection` in the active space, 2 views | per eye: the pose and `XrFovf` that A(k) located (never re-located), `subImage` the whole acquired image; `layerFlags` 0; with depth, each view chains `XrCompositionLayerDepthInfoKHR { subImage of the depth image, minDepth 0, maxDepth 1, nearZ, farZ }` with near and far swapped for reversed Z | the frame was rendered |
| 2 | `XrCompositionLayerQuad` in the view space, `XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT`, pose (0, 0, −`xr.uiQuadDistanceMetres`), size (`xr.uiQuadWidthMetres`, width × canvas height / canvas width) | the screen-space UI composited into the quad swapchain's image (row SP8) | `xr.uiLayer = quad` and the UI drew this frame |

The runtime's `XrSystemGraphicsProperties::maxLayerCount` bounds the list; a quad that does not fit is dropped and counted `xr.layers.quad_dropped`. The environment blend mode is always `XR_ENVIRONMENT_BLEND_MODE_OPAQUE` (ruling SC-R3).

## Reference spaces

At S2, from `xrEnumerateReferenceSpaces`: with `xr.referenceSpace = auto`, STAGE when offered, else LOCAL_FLOOR (core in 1.1), else LOCAL; an explicit preference falls back in the same order when not offered. The VIEW space is always created (the head pose and the UI quad). The active space's origin is the XR tracking origin; the camera rig's world pose from GAME's director output (`Zenith2/Docs/Design/GAME/Cameras.md` → "Camera model") maps tracking-origin coordinates to world coordinates: an eye's world-from-eye matrix is `rig × recentre × pose(eye)`, built inside `AcquireStereoFrame` from `xRigWorldFromOrigin`, which the renderer passes from the stereo request's camera.

## Recentre and play area

**Recentre.** A request (the presentation action, the console command, or a script's committed `XR.RequestRecentre`) is applied in the next prologue while a session runs: locate the VIEW space in the active space at the last predicted display time; take its yaw ψ (rotation about +Y, from the forward vector's x and z by `Zenith_Atan2`); with LOCAL or LOCAL_FLOOR, set the recentre transform to the rotation by −ψ about +Y and the translation that moves the head's x and z to 0; with STAGE, rotate only (the stage stays anchored to the floor plan). The transform multiplies every located pose, for eyes and for controllers, so simulation sees recentred controller poses through commands. `XR.Recentred { yawRadians = ψ }` is published.

**Play area.** For the STAGE space, `xrGetReferenceSpaceBoundsRect` at session start and on its change event; `XR_SPACE_BOUNDS_UNAVAILABLE` gives an invalid area. `XR.PlayAreaChanged` is published with the extents in metres.

## Loss recovery and shutdown

- **Session loss**: the session state machine's `RETRY_WAIT` row; retries never block a frame.
- **No headset at launch** (ruling Z2-D-137). Every graphics-requirements call of both bindings takes the `XrSystemId` (`xrGetVulkanGraphicsRequirements2KHR`, `xrCreateVulkanInstanceKHR`, `xrGetVulkanGraphicsDevice2KHR`, `xrCreateVulkanDeviceKHR`, `xrGetD3D12GraphicsRequirementsKHR`), so a device created while there was no system was not created for the runtime, and a session on it would fail with `XR_ERROR_GRAPHICS_DEVICE_INVALID`. When `xrGetSystem` first succeeds in `RETRY_WAIT`, ZenithXR runs P6 and P7 and asks the renderer for a device recreation through RHI's recovery path; the provider is asked for a fresh interop request (P8) when the device is created again, and the session is created after `OnDeviceCreated` (P9). The cost — the recovery path reloads every resident GPU resource and recreates the boot, global and scene PSO groups — is accepted because it happens once per headset appearance after launch or per instance recreation, and it is the path RHI already proves for device loss. On D3D12 the recreation is skipped when the existing device already has the runtime's adapter LUID and feature level, because a D3D12 session needs no runtime-created device; Vulkan always recreates, because the runtime creates the instance.
- **Device loss**: the session is destroyed at `OnDeviceDestroying`, before the lost device is destroyed in RHI's recovery step 3; P8 runs again when the device is recreated and P9 and S1 to S4 after it (the session state machine's rows).
- **Instance loss** (`XrEventDataInstanceLossPending` or `XR_ERROR_INSTANCE_LOST`): destroy everything down to the instance; state `RETRY_WAIT` without a system; every `xr.retrySeconds`, P4 and P5 run again; when they succeed, the device is recreated as for "No headset at launch", because the device was created for the lost instance.
- **Shutdown**: `BeginShutdown` requests exit, polls events up to `xr.shutdownWaitMs` for STOPPING, calls `xrEndSession` when it arrived, then destroys swapchains (their imports first), action spaces, the action set, reference spaces and the session; the provider outlives the device and destroys the instance after it (§ "Startup and the device-interop provider", "Shutdown placement").

<!-- assumption: pending RHI, RPROD (row SP16) -->
`bool Flux_Renderer::RequestDeviceRecreation(Flux_DeviceRecreationReason eReason)` (main thread, a render-idle point; `enum Flux_DeviceRecreationReason : uint8 { FLUX_DEVICE_RECREATION_REASON_XR = 1 };` declared in RPROD's `Flux_Renderer.h`) returns false and does nothing when the device-loss state is not `NORMAL`; otherwise it returns true and makes `Flux_DeviceLossHandler` leave `NORMAL` for `RECOVERING` at its next prologue call, after a device wait-idle (the device is healthy), skipping `LOST` and `REPORTING`. On entry the runtime pauses every world at its next tick boundary exactly as for a loss, so simulation never runs unseen while the render subgraph is empty from `RECOVERING` to `RESUMING` ([Z2-N-ARCH-880]); step 5 unpauses them. `rhi.deviceLossPolicy` does not apply to a requested recreation (it is read in `REPORTING`, which is skipped), it is not counted as a loss for the 60-second second-loss rule, a failure of the recreation takes `EXITING` regardless, and it counts `RHI.Device.RequestedRecreations` instead of `RHI.Device.Recoveries`. The "same backend and adapter" clause of the recovery policy is lifted for a requested recreation: the desc's `m_pxInterop` request decides the instance and adapter. `RECOVERING` calls the composition's provider `OnDeviceDestroying` before step 3 destroys the device, `OnDeviceDestroyed` after it, and step 12's `FillInteropRequest` and `OnDeviceCreated` around the new factory call, for loss and requested recreation alike (cross-doc item X443 patches the core text).
<!-- /assumption -->

## Result mapping

| `XrResult` | Where it can occur | Mapped to |
|---|---|---|
| `XR_SUCCESS`, `XR_SESSION_VISIBILITY_UNAVAILABLE` | any | success |
| `XR_TIMEOUT_EXPIRED` | `xrWaitSwapchainImage` | the frame is skipped |
| `XR_EVENT_UNAVAILABLE` | `xrPollEvent` | end of the poll |
| `XR_FRAME_DISCARDED` | `xrBeginFrame` | success, counted |
| `XR_SESSION_LOSS_PENDING` | frame calls | success this frame; the loss event follows |
| `XR_SPACE_BOUNDS_UNAVAILABLE` | `xrGetReferenceSpaceBoundsRect` | invalid play area |
| `XR_ERROR_RUNTIME_UNAVAILABLE`, `XR_ERROR_RUNTIME_FAILURE` | instance creation | `ZENITH_XR_ERROR_RUNTIME_UNAVAILABLE` |
| `XR_ERROR_API_VERSION_UNSUPPORTED` | instance creation | `ZENITH_XR_ERROR_API_VERSION_UNSUPPORTED` |
| `XR_ERROR_EXTENSION_NOT_PRESENT` | instance creation | `ZENITH_XR_ERROR_EXTENSION_MISSING` |
| `XR_ERROR_FORM_FACTOR_UNAVAILABLE` | `xrGetSystem` | `ZENITH_XR_ERROR_NO_HEADSET` |
| `XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED` | session begin | `ZENITH_XR_ERROR_VIEW_CONFIGURATION` |
| `XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING`, `XR_ERROR_GRAPHICS_DEVICE_INVALID` | session creation | `ZENITH_XR_ERROR_GRAPHICS_REQUIREMENTS` |
| `XR_ERROR_SESSION_LOST` | any session call | `ZENITH_XR_ERROR_SESSION_LOST` |
| `XR_ERROR_INSTANCE_LOST` | any call | `ZENITH_XR_ERROR_INSTANCE_LOST` |
| `XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED`, `XR_ERROR_SWAPCHAIN_RECT_INVALID` | swapchain creation, layers | `ZENITH_XR_ERROR_SWAPCHAIN_CREATE` |
| `XR_ERROR_CALL_ORDER_INVALID`, `XR_ERROR_SESSION_NOT_RUNNING`, `XR_ERROR_SESSION_NOT_READY`, `XR_ERROR_SESSION_RUNNING` | frame and session calls | `ZENITH_XR_ERROR_FRAME_CALL` (a defect of the cycle: also `ZENITH_ASSERT` in debug builds) |
| `XR_ERROR_PATH_UNSUPPORTED`, `XR_ERROR_PATH_INVALID`, `XR_ERROR_INTERACTION_PROFILE_UNSUPPORTED` | binding suggestions | that profile's suggestion is skipped, logged (`Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings") |
| `XR_ERROR_ACTIONSETS_ALREADY_ATTACHED`, `XR_ERROR_ACTIONSET_NOT_ATTACHED` | input | `ZENITH_XR_ERROR_ACTION_SETUP` (a defect: `ZENITH_ASSERT`) |
| `XR_ERROR_OUT_OF_MEMORY`, `XR_ERROR_LIMIT_REACHED` | any | `ZENITH_XR_ERROR_OUT_OF_MEMORY` |
| any other negative value | any | the code of the calling step, with `m_iXrResult` the value |

## Scripted runtime

`Zenith_OpenXrApiScripted` implements every seam member deterministically from a `zenith2.xr-script` document, so the gate tests drive the whole integration on WARP and lavapipe with no headset (ruling Z2-D-138). Every named test of `Zenith2/Docs/Design/SCOPE/XrTests.md` is expressible and observable through it and `Zenith_XrTestAccess`.

```json
{
	"schema": "zenith2.xr-script",
	"schemaVersion": 1,
	"runtime": { "present": true, "name": "Zenith2 scripted runtime", "apiVersion": "1.1.0", "extensions": ["XR_KHR_vulkan_enable2", "XR_KHR_D3D12_enable", "XR_KHR_win32_convert_performance_counter_time", "XR_KHR_composition_layer_depth"] },
	"system": {
		"present": true, "recommendedExtent": [1440, 1584], "maxExtent": [2880, 3168], "maxLayerCount": 16, "displayPeriodNs": 11111111,
		"referenceSpaces": ["stage", "localFloor", "local", "view"], "stageBounds": [3.0, 2.5],
		"blendModes": ["opaque"],
		"swapchainFormats": { "vulkan": ["R8G8B8A8_SRGB", "B8G8R8A8_SRGB", "D32_SFLOAT"], "d3d12": ["R8G8B8A8_UNORM_SRGB", "B8G8R8A8_UNORM_SRGB", "D32_FLOAT"] }
	},
	"views": [
		{ "eye": "left", "position": [-0.032, 1.6, 0.0], "orientation": [0, 0, 0, 1], "fov": [-0.785398, 0.698132, 0.785398, -0.785398], "validBits": ["position", "orientation"] },
		{ "eye": "right", "position": [0.032, 1.6, 0.0], "orientation": [0, 0, 0, 1], "fov": [-0.698132, 0.785398, 0.785398, -0.785398], "validBits": ["position", "orientation"] }
	],
	"profiles": { "left": "/interaction_profiles/oculus/touch_controller", "right": "/interaction_profiles/oculus/touch_controller", "refuse": [] },
	"timeline": [
		{ "frame": 0, "sessionState": "ready" },
		{ "frame": 1, "sessionState": "synchronized" },
		{ "frame": 2, "sessionState": "visible" },
		{ "frame": 3, "sessionState": "focused" },
		{ "frame": 30, "input": { "path": "/user/hand/right/input/trigger/value", "value": 1.0 } },
		{ "frame": 60, "pose": { "path": "/user/hand/left/input/grip/pose", "position": [-0.2, 1.2, -0.3], "orientation": [0, 0, 0, 1], "validBits": ["position", "orientation"] } },
		{ "frame": 80, "views": { "left": { "validBits": ["position"] } } },
		{ "frame": 90, "shouldRender": false },
		{ "frame": 120, "fail": { "function": "xrWaitSwapchainImage", "result": "XR_TIMEOUT_EXPIRED", "occurrence": 1 } },
		{ "frame": 150, "sessionState": "lossPending" }
	]
}
```

| Key | Rule (rule number for `m_uDetail` of `ZENITH_XR_ERROR_INVALID_SCRIPT`) |
|---|---|
| `schema`, `schemaVersion` | exactly `zenith2.xr-script` and `1` (1) |
| `runtime.apiVersion` | `MAJOR.MINOR.PATCH` (2) |
| `runtime.extensions` | names from the table of § "OpenXR version and extensions" (3) |
| `system.present` | false makes `xrGetSystem` answer `XR_ERROR_FORM_FACTOR_UNAVAILABLE` until a timeline entry `{ "systemPresent": true }` (4) |
| `system.recommendedExtent`, `maxExtent` | two integers in [64, 8192], recommended ≤ max (5) |
| `system.displayPeriodNs` | in [5,000,000, 50,000,000] (6) |
| `views` | exactly two, `left` then `right`; `fov` is `[angleLeft, angleRight, angleUp, angleDown]` radians with left < right, down < up, each in (−π/2, π/2); `validBits` a subset of `position`, `orientation`, default both (7) |
| `profiles.left`, `profiles.right` | a profile path of `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings" or `null` (8) |
| `timeline[].frame` | non-decreasing integers ≥ 0 (9) |
| `timeline[]` entry | exactly one of `sessionState` (an OpenXR state name in lower camel case), `input` (`path` an input path of a bound profile, `value` a number or `[x, y]`), `pose` (with optional `validBits`), `views` (per-eye `validBits` overrides), `shouldRender`, `fail` (`function` a seam member's OpenXR name, `result` an `XrResult` name, `occurrence` an optional 1-based ordinal), `systemPresent`, `profiles` (per-hand profile changes), `instanceLoss`, `boundsChange` (10) |
| document size | at most 1 MiB, at most 100,000 timeline entries (11) |
| `runtime.present` | false makes `xrCreateInstance` answer `XR_ERROR_RUNTIME_UNAVAILABLE` (12) |
| `system.blendModes` | a non-empty subset of `opaque`, `additive`, `alphaBlend`; default `["opaque"]` (13) |
| `system.swapchainFormats` | per backend, the offered list in order, from the names of § "Swapchains" without their `VK_FORMAT_` or `DXGI_FORMAT_` prefix; default the full preference list (14) |
| `profiles.refuse` | interaction-profile paths for which `xrSuggestInteractionProfileBindings` answers `XR_ERROR_PATH_UNSUPPORTED` (15) |

**Timing and events.** Each `CreateSession` starts a new session timeline: the frame counter n and the timeline cursor reset to 0, and the entries of every frame ≥ 0 are delivered again for the new session; `systemPresent`, `profiles`, `instanceLoss`, `boundsChange` and `fail` entries are consumed once per process and are not re-delivered. Within a session, the frame counter n counts calls of `WaitFrame`, starting at 0. `CreateSession` itself queues `XR_SESSION_STATE_IDLE` first, as a real runtime does. Entries of frame 0 are delivered by the first `PollEvent` after `CreateSession`, so a `frame: 0` `ready` entry arrives before any `WaitFrame` and the cycle never waits on itself; entries of frame n ≥ 1 are delivered by the first `PollEvent` after the n-th `WaitFrame`. `predictedDisplayTime` of frame n is `1,000,000,000 + n × displayPeriodNs`; `WaitFrame` returns at once, so tests run at full speed. `input`, `pose` and `views` entries change what `SyncActions`, `LocateSpace` and `LocateViews` report from that frame on. A `fail` entry makes the `occurrence`-th call of that function at or after the frame (default the first) return the result once.

**Device route.** The scripted implementation enforces the graphics constraints of real runtimes. Its `CreateVulkanInstanceKHR` and `CreateVulkanDeviceKHR` call the creation functions the RHI supplies (resolved through the RHI's `vkGetInstanceProcAddr`) with the RHI's create infos unchanged, and remember the created handles; its `GetVulkanGraphicsDevice2KHR` returns the first physical device of that instance; `CreateSession` fails with `XR_ERROR_GRAPHICS_DEVICE_INVALID` when the binding's `VkDevice` was not created through its `CreateVulkanDeviceKHR` (Vulkan), or when the binding's adapter LUID differs from the one `GetD3D12GraphicsRequirementsKHR` returned (D3D12; WARP's LUID). The mirrored `XrGraphicsBinding` struct of `Internal/Zenith_OpenXrTypes.h` carries `Zenith_RHIInteropHandles` and a `Zenith_RHIDevice*` that only the scripted implementation reads (the loader implementation ignores it); the scripted `CreateSwapchain` creates its images as ordinary RHI textures H1 through that device, transitions each with a one-shot upload-queue barrier on H1's tracker to the release state OpenXR defines (`COLOR_ATTACHMENT` for colour, `DEPTH_WRITE` for depth), and hands out their native handles from `Zenith_RHIDevice::GetNativeTexture` (row SP2; 0 in a Shipping build, where the scripted runtime is refused anyway), which ZenithXR then imports as H2 with that release state like any runtime's images; the scripted `ReleaseSwapchainImage` asserts the image is back in the release state (H2's tracker ends every frame in it, the external-import contract), so the validation sink sees no layout mismatch. An import never frees, so H2's destruction before `DestroySwapchain` and H1's destruction inside it free the image once. The provider's `OnDeviceDestroying` destroys any scripted swapchain that is still alive before the device is destroyed, because the provider and its seam outlive the device.

**Call log.** Every seam call appends one `Zenith_XrSeamCallRecord` to a ring of 4,096 records that `Zenith_XrTestAccess::GetSeamCallLog` returns oldest first (`Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_XrTestAccess header"): the function, the frame counter at the call, the result, and the argument summary its row defines.

| Function | Summary fields |
|---|---|
| `xrLocateViews` | `m_aulArg[0]` the display time, `m_aulArg[1]` the space |
| `xrEndFrame` | `m_aulArg[0]` the display time, `m_aulArg[1]` the layer count, `m_aulArg[2]` the blend mode, `m_afArg[0..6]` the first projection view's pose (position, orientation), `m_afArg[7..10]` its `XrFovf`, `m_aulArg[3]` its `subImage` width (high 32 bits) and height (low 32 bits), `m_aulArg[4]` 1 when depth info is chained, `m_afArg[11..12]` its `nearZ` and `farZ` |
| `xrSuggestInteractionProfileBindings` | `m_aulArg[0]` the profile path's `Zenith_Hash64`, `m_aulArg[1]` the binding count |
| `xrApplyHapticFeedback` | `m_aulArg[0]` the subaction path's `Zenith_Hash64`, `m_aulArg[1]` the duration in ns, `m_afArg[0]` the amplitude |
| `xrStopHapticFeedback` | `m_aulArg[0]` the subaction path's `Zenith_Hash64` |
| `xrLocateSpace` | `m_aulArg[0]` the space, `m_aulArg[1]` the time; the scripted runtime numbers its spaces as ordinals in creation order of a session, fixed by ZenithXR's step S2 and S3 order: `1` the active reference space, `2` view, `3` grip-left, `4` grip-right, `5` aim-left, `6` aim-right |
| every other function | none |

Calls out of OpenXR's required order return `XR_ERROR_CALL_ORDER_INVALID` exactly as the specification requires, so the call-order tests are meaningful.
