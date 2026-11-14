# SCOPE design: XR and scope tests

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It lists every SCOPE test by target and file with the case it asserts, the real-runtime integration, the reference device matrix and the device runs (ruling Z2-D-138).

## Scope of this file

Test names are unique programme-wide and carry SCOPE's type prefixes (`Xr…`, `Scope…`). Every test is a doctest case registered as its own CTest ([Z2-N-ARCH-179]), pass or fail only. GPU tests run on the `win` preset's WARP and lavapipe devices (X170: not `needs-gpu`) with the RHI validation sink installed. No test of `ZenithXR_Tests` needs a headset or an installed OpenXR runtime: each creates its system with `xr.runtime = scripted` and a document built by `Tests/Support/Zenith_XrTestScripts.h`, written under `${ZENITH_OUT}/test-fixtures/SCOPE/` by the test's own setup (Q55: nothing binary is committed).

## Test targets

| Target | Directory | Presets | Label |
|---|---|---|---|
| `ZenithXR_Tests` | `Zenith2/Engine/ZenithXR/Tests/` | `win`, `win-asan` | `z2-xr` |
| `Zenith_Fuzz_XrScript` | `Zenith2/Engine/ZenithXR/Tests/Fuzz/` | `win-asan` | fuzz |
| `Zenith_Integration_ScopeChecks` | `Zenith2/Tests/ScopeChecks/` | `win` | `z2-scope` |
| `Zenith_Integration_XrRuntime` | `Zenith2/Tests/XrRuntime/` | `win` (windowed GPU configuration) | registered one case at a time with `SKIP_RETURN_CODE 77` |
| `Pwsh_ScopeRegister_Valid`, `Pwsh_ScopeRegister_ValidFixtures` | `Zenith2/CMake/Tests/Scope/` | `win` | `z2-cmake` |
| `Pwsh_ScopeRegister_ReadinessTestsPresent` | `Zenith2/CMake/Tests/Scope/` | `win` | `z2-acceptance-scope` |

## Readiness and exclusion tests

The cases of `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "Readiness tests SCOPE owns" and "Exclusion checks SCOPE owns", and the register checks of `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Register checks".

## Module and seam tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrModule_DisabledWhenNoRuntime` | `Zenith_XrModule_Tests.cpp` | a scripted document with `runtime.present = false` (rule 12, so `xrCreateInstance` answers `XR_ERROR_RUNTIME_UNAVAILABLE`): the provider fills an empty interop request, startup succeeds, the module starts, `GetStatus().m_eState` is `DISABLED` with reason `RUNTIME_UNAVAILABLE`, a stereo request renders mono and counts `xr.fallback.mono_frames`; failure case: the warning line is logged exactly once, and an invalid `xr.runtime` value instead makes startup step 11 return exit code 2 |
| `Test_XrModule_DisabledInHeadlessAndReplay` | `Zenith_XrModule_Tests.cpp` | a headless context and a replaying context each create a disabled provider whose `Zenith_XrTestAccess::GetProviderSeamCallLog` holds no record at all (the provider creates no seam), and the module starts in `DISABLED`; failure case: a windowed, non-replaying context with `xr.enabled = false` gives the same empty log |
| `Test_XrModule_StartRegistersInOrder` | `Zenith_XrModule_Tests.cpp` | `Start` performs the steps of `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header" in order; an injected registration failure at each step returns `MODULE_ERROR_REGISTRATION` and leaves no registration behind |
| `Test_XrModule_AndroidPlatformRefused` | `Zenith_XrModule_Tests.cpp` (`z2-cmake` fixtures through MOD's game.json fixture runner) | a `game.json` listing `XR` with `android`, and one listing `XR` with `playstation5`, each fail the configure with `Games/<Name>/game.json: modules: XR is supported on windows only`; the check reads the declared `platforms` list, so it fails even when the console's overlay is absent and the console platform would otherwise be dropped as declared-but-unavailable |
| `Test_XrSeam_CallGuardDetectsOverlap` | `Zenith_XrSeam_Tests.cpp` (suite `z2-death-assert`) | entering the guard from `BUILD_NODE` while `RENDER_SUBMIT` holds it asserts |
| `Test_XrSeam_ScriptedRuntimeEnforcesCallOrder` | `Zenith_XrSeam_Tests.cpp` | `BeginFrame` before `WaitFrame` and `EndFrame` without `BeginFrame` each return `XR_ERROR_CALL_ORDER_INVALID` from the scripted runtime |
| `Test_XrSeam_ExtensionCheckPerBackend` | `Zenith_XrSeam_Tests.cpp` | a runtime without `XR_KHR_D3D12_enable` disables XR with `BACKEND_UNSUPPORTED` for D3D12 and enables it for Vulkan; without the time-conversion extension both are `EXTENSION_MISSING` with `m_uDetail` 1 (its index in the required list of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "OpenXR version and extensions") |
| `Test_XrSeam_ApiVersionRefused` | `Zenith_XrSeam_Tests.cpp` | `apiVersion` 1.0.34 disables XR with `API_VERSION_UNSUPPORTED` and the error line |
| `Test_XrInterop_VulkanCallbacksCreateDevice` | `Zenith_XrInterop_Tests.cpp` (lavapipe) | the interop request's three callbacks are each called once by device creation, the device is the one the scripted runtime named, and the session binding carries the graphics queue family |
| `Test_XrInterop_D3D12AdapterLuid` | `Zenith_XrInterop_Tests.cpp` (WARP) | the request's LUID selects the WARP adapter over `rhi.adapter`, logged; a LUID matching no adapter disables XR with `GRAPHICS_REQUIREMENTS` and the device is created by the ordinary selection |
| `Test_XrSession_CreateFailsAtStep_Unwinds` | `Zenith_XrSession_Tests.cpp` | the provider steps P2 to P9: a `Zenith_FaultInjector` passed in `Zenith_DeviceInteropContext::m_pxFaults` fails each step in turn, and `GetProviderSeamCallLog` shows every handle created before it destroyed (create and destroy records pair up) and the documented state; P1's invalid key returns `DEVICE_INTEROP_ERROR_CONFIGURATION` and P2's allocation failure `DEVICE_INTEROP_ERROR_OUT_OF_MEMORY`, which the test's composition maps to exit codes 2 and 4; every other P-step failure leaves startup successful. The session steps S1 to S4: `Zenith_XrTestAccess::FailNextStep` fails each in turn and the system's call log shows the same pairing |

## Session and frame tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrSession_StateMachineTable` | `Zenith_XrSession_Tests.cpp` | every row of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Session state machine" driven by a timeline: the next state, `xrBeginSession` and `xrEndSession` issued in the same prologue, `PacesFrames()` true exactly from READY to STOPPING, one `XR.SessionChanged` per transition |
| `Test_XrSession_UnfocusedAddsPauseReason` | `Zenith_XrSession_Tests.cpp` | FOCUSED → VISIBLE adds `WORLD_PAUSE_REASON_XR_UNFOCUSED` to every LIVE world and VISIBLE → FOCUSED removes it; with `xr.pauseWhenUnfocused = false` neither happens |
| `Test_XrSession_LossRetriesAfterInterval` | `Zenith_XrSession_Tests.cpp` | `lossPending` at frame 150: handles destroyed, the three XR devices destroyed with releases, state `RETRY_WAIT`; with `xr.retrySeconds = 1` and a test clock, the next `CreateSession` is issued after 1 s and not before |
| `Test_XrSession_HeadsetAfterLaunchRecreatesDevice` | `Zenith_XrSession_Tests.cpp` (lavapipe and WARP) | `system.present = false` then `systemPresent` at frame 40: the device is first created without interop; `xrGetSystem` is retried each interval; after the entry ZenithXR requests one device recreation (`FLUX_DEVICE_RECREATION_REASON_XR`, not counted as a loss), the new device is created through the scripted runtime's callbacks (Vulkan) or its LUID (D3D12), and the session is created after it; failure case: with `FailNextStep("P8")` at the recreation the new device is created without the runtime's callbacks, the scripted `xrCreateSession` answers `XR_ERROR_GRAPHICS_DEVICE_INVALID`, and ZenithXR reports `GRAPHICS_REQUIREMENTS` with XR disabled |
| `Test_XrSession_InstanceLossRecreatesInstanceThenDevice` | `Zenith_XrSession_Tests.cpp` | an `instanceLoss` entry: the session and instance are destroyed, state `RETRY_WAIT`; after `xr.retrySeconds` a new instance and system are created and one device recreation is requested; the session exists after it |
| `Test_XrSession_EventsBoundedPerPrologue` | `Zenith_XrSession_Tests.cpp` | 100 queued events: 64 handled in the first prologue, 36 in the second, `xr.events.deferred` 36 |
| `Test_XrFrame_CycleOrderPerFrame` | `Zenith_XrFrame_Tests.cpp` (WARP, lavapipe) | 120 frames: per XR frame exactly one `WaitFrame`, `LocateViews`, `BeginFrame`, `EndFrame`, in the order W, A, B, E of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Frame cycle", with one acquire, wait and release per swapchain |
| `Test_XrFrame_FirstFrameNoDeadlock` | `Zenith_XrFrame_Tests.cpp` | a script whose first entry is `{ "frame": 0, "sessionState": "ready" }`: the call log shows `xrCreateSession`, a `PollEvent` delivering IDLE then READY, `xrBeginSession`, then the first `xrWaitFrame` at the next `Flux.Present`, within 3 engine frames |
| `Test_XrFrame_ShouldRenderFalseSubmitsZeroLayers` | `Zenith_XrFrame_Tests.cpp` | at frame 90 (`shouldRender: false`): begin and end are called, the layer count is 0, no image is acquired, the stereo request renders mono that frame |
| `Test_XrFrame_ImageWaitTimeoutSkipsFrame` | `Zenith_XrFrame_Tests.cpp` | a `fail` of `xrWaitSwapchainImage` with `XR_TIMEOUT_EXPIRED`: zero layers that frame, `xr.frames.skipped_wait_timeout` 1, the next frame submits normally |
| `Test_XrFrame_DisplayTimeFromWaitFrame` | `Zenith_XrFrame_Tests.cpp` | from the call log: each `xrEndFrame` record's `m_aulArg[0]` equals the `predictedDisplayTime` of its `WaitFrame` (`1,000,000,000 + n × period`), and the `xrLocateViews` record of the same frame carries the same time |
| `Test_XrFrame_ExternalPacingSkipsPacerWait` | `Zenith_XrFrame_Tests.cpp` | while a session runs the pacer reports `FLUX_PACING_MODE_EXTERNAL` and records no CPU wait; at STOPPING it returns to its configured mode |
| `Test_XrFrame_EyePosesComposeRigAndRecentre` | `Zenith_XrFrame_Tests.cpp` | with a rig at (10, 0, 5) rotated 90° about +Y and a recentre of yaw 0.3: each eye's world-from-eye equals `rig × recentre × pose(eye)` within 1e-5 m and 1e-5 rad of a hand-computed matrix |
| `Test_XrFrame_InvalidPoseFallsBackToMono` | `Zenith_XrFrame_Tests.cpp` | a `views` entry giving the left view `validBits: ["position"]` (no `ORIENTATION_VALID_BIT`): `AcquireStereoFrame` returns false, any acquired image is released at E, the stereo request renders mono |
| `Test_XrFrame_DeviceLossRecreatesSession` | `Zenith_XrFrame_Tests.cpp` (lavapipe and WARP) | a device loss injected through the RHI's fault injection: `OnDeviceDestroying` reaches the system through its device listener, which destroys the swapchains (imports first), the session and the three XR controller devices (releases synthesised), state `NO_SESSION`; while the renderer's state is `LOST` through `RESUMING` the call log holds no `xrWaitFrame`, `xrBeginFrame` or `xrEndFrame`; after `OnDeviceCreated` the listener holds the new device, a second `xrCreateSession` starts a new scripted timeline (its `frame: 0` `ready` entry delivered again), and the session reaches FOCUSED; failure case: a frame call during the interval fails the test |

## Swapchain and layer tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrSwapchain_FormatPreference` | `Zenith_XrSwapchain_Tests.cpp` | `system.swapchainFormats` lists in each order choose the first preferred format; a list with none fails with `SWAPCHAIN_FORMAT`; no depth format turns depth off with the log line |
| `Test_XrSwapchain_ExtentFromRecommendedAndScale` | `Zenith_XrSwapchain_Tests.cpp` | recommended 1440 × 1584 with scale 1.0, 1.5 and 2.5 (clamped at the maximum 2880 × 3168) and 0.01 (clamped to 64) gives the documented extents |
| `Test_XrSwapchain_ImagesImportedOnceAndReleased` | `Zenith_XrSwapchain_Tests.cpp` (WARP, lavapipe) | each image is imported once with its release state and debug name, and the validation sink reports no layout mismatch over 60 frames (the scripted images start and end each frame in the release state); destruction releases every import before `DestroySwapchain`; a provider destroyed with a scripted swapchain still alive destroys it in `OnDeviceDestroying`; the device reports no live handle at its destruction |
| `Test_XrSwapchain_RecreateOnScaleChange` | `Zenith_XrSwapchain_Tests.cpp` | `xr.renderScale` 1.0 → 0.8 recreates at the next render-idle point, new before old; an injected creation failure keeps the old swapchains and retries the next prologue |
| `Test_XrSwapchain_GovernorCriticalStepsScale` | `Zenith_XrSwapchain_Tests.cpp` | three CRITICAL entries lower the scale 1.0 → 0.9 → 0.8 → 0.7, a fourth keeps 0.7, NORMAL restores 1.0 |
| `Test_XrLayers_ProjectionUsesLocatedViews` | `Zenith_XrLayers_Tests.cpp` | from the call log: the `xrEndFrame` record's first projection view carries exactly the pose and `XrFovf` of the script's left view for that frame and the whole image extent as `subImage` |
| `Test_XrLayers_DepthInfoChainedAndSwappedForReversedZ` | `Zenith_XrLayers_Tests.cpp` | with depth submission: each view chains a depth info with `nearZ` and `farZ` of the eye, swapped when the view uses reversed Z; without the extension no chain |
| `Test_XrLayers_QuadDroppedAtLayerLimit` | `Zenith_XrLayers_Tests.cpp` | `maxLayerCount` 1: the quad is dropped and counted, the projection kept; `xr.uiLayer = world` submits no quad |
| `Test_XrLayers_OpaqueBlendModeOnly` | `Zenith_XrLayers_Tests.cpp` | every `xrEndFrame` record's `m_aulArg[2]` is `XR_ENVIRONMENT_BLEND_MODE_OPAQUE`; `system.blendModes = ["additive"]` disables XR at P6 with `VIEW_CONFIGURATION` |
| `Test_XrSpaces_ReferenceSpaceFallback` | `Zenith_XrSpaces_Tests.cpp` | offered {stage, localFloor, local}, {localFloor, local}, {local} with `auto` choose stage, localFloor, local; an explicit `local` chooses local |
| `Test_XrSpaces_RecentreLocalAndStage` | `Zenith_XrSpaces_Tests.cpp` | a head at (0.5, 1.7, −0.4) with yaw 0.6: in LOCAL the transform zeroes x and z and rotates by −0.6; in STAGE it only rotates; `XR.Recentred` carries 0.6 |
| `Test_XrSpaces_PlayAreaPublished` | `Zenith_XrSpaces_Tests.cpp` | stage bounds 3.0 × 2.5 publish a valid play area; a `boundsChange` entry republishes; `XR_SPACE_BOUNDS_UNAVAILABLE` publishes an invalid one |

## Input tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrInput_BindingTableMatchesDesign` | `Zenith_XrInput_Tests.cpp` | the constant table equals the machine-readable block of `Zenith2/Docs/Design/SCOPE/XrInput.md` → "Suggested bindings", read from the source tree, entry for entry; every catalogue control except `thumbrestTouch` and the trackpad controls has a binding in at least two profiles, and no (profile, action, hand, path) entry appears twice |
| `Test_XrInput_RefusedProfileSkipped` | `Zenith_XrInput_Tests.cpp` | `profiles.refuse` = [`/interaction_profiles/htc/vive_controller`]: the call log shows five suggestion calls, four `XR_SUCCESS` and the vive one `XR_ERROR_PATH_UNSUPPORTED`, one log line; refusing all five fails with `ACTION_SETUP` |
| `Test_XrInput_MirrorDrivesVirtualDevices` | `Zenith_XrInput_Tests.cpp` | a right trigger at 1.0 from frame 30 yields one `/xr/right/trigger` transition with 1.0 in frame 30's pump and none in frame 31 (unchanged value), and PLAT's mapper fires a bound action in frame 30's first tick |
| `Test_XrInput_InactiveActionsRestOnce` | `Zenith_XrInput_Tests.cpp` | FOCUSED → VISIBLE with the trigger held: one release to 0 is pushed, and no further transition while VISIBLE |
| `Test_XrInput_PoseSampleTime` | `Zenith_XrInput_Tests.cpp` | from the call log: frame k's `xrLocateSpace` record with space ordinal 3 (grip-left) carries `T(k − 1) + P`; a `pose` entry with `validBits: ["position"]` pushes nothing and counts `xr.input.pose_invalid` |
| `Test_XrInput_DevicesFollowProfiles` | `Zenith_XrInput_Tests.cpp` | the left profile becoming null destroys `left` with releases, becoming non-null recreates it, and `SetActiveInteractionProfiles` receives the deduplicated paths |
| `Test_XrInput_HapticMapping` | `Zenith_XrInput_Tests.cpp` | an effect of low 0.3, high 0.8 for 120 ms on the right device becomes one `xrApplyHapticFeedback` call-log record with amplitude 0.8 and duration 120,000,000 ns on the right subaction path; amplitude 0 becomes `StopHapticFeedback`; outside FOCUSED no call and `xr.haptics.skipped` 1 |
| `Test_XrInput_ReplayWithoutRuntimeMatches` | `Zenith_XrInput_Tests.cpp` | a 300-tick scripted-runtime run recorded with `--record` replays with `--replay` and no runtime to the same per-tick world hashes |

## Script surface tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrScript_SessionInfoFromPublishedState` | `Zenith_XrScript_Tests.cpp` | before any notification `XR.GetSessionInfo()` returns the disabled table; after READY, FOCUSED it returns `focused = true`, `stereo = true`, `refreshHz = 90.0` in the tick after the boundary that published it |
| `Test_XrScript_RecentreCommitsOnly` | `Zenith_XrScript_Tests.cpp` | a behaviour calling `XR.RequestRecentre()` and then erroring requests nothing; one that succeeds requests one recentre in the next prologue |
| `Test_XrScript_MisuseCodes` | `Zenith_XrScript_Tests.cpp` | each row of `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Misuse codes" raises its SCRIPT code (ZS1510, ZS1702) with its text |
| `Test_XrScript_NotificationQueueBound` | `Zenith_XrScript_Tests.cpp` | twelve state changes in one prologue: eight parked, four replacing the oldest unparked of their kind, `xr.notify.dropped` 4, the last state always delivered |

## Format and fuzz tests

| Test | File | Case asserted |
|---|---|---|
| `Test_XrScriptFormat_ValidDocumentParses` | `Zenith_XrScriptFormat_Tests.cpp` | the example of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Scripted runtime" parses to the documented values |
| `Test_XrScriptFormat_EveryRuleRejects` | `Zenith_XrScriptFormat_Tests.cpp` | one document per rule 1 to 11 fails with `INVALID_SCRIPT` and `m_uDetail` the rule |
| `Test_XrScriptFormat_ShippingRefused` | `Zenith_XrScriptFormat_Tests.cpp` | with the Shipping build flag of the test configuration, `xr.runtime = scripted` is `INVALID_CONFIG` and `AppendScript` is `SHIPPING_REFUSED` |
| `Zenith_Fuzz_XrScript` | `Tests/Fuzz/Zenith_XrScript_Fuzz.cpp` | libFuzzer over the parser: no crash, no leak, no unbounded allocation beyond 1 MiB of input |

## Real-runtime integration

`Zenith_Integration_XrRuntime`, run in the windowed GPU configuration on a host with an OpenXR runtime. Each case begins with `xrCreateInstance` through the loader; `XR_ERROR_RUNTIME_UNAVAILABLE` logs `xr-runtime: no OpenXR runtime, skipped` and ends the process with exit code 77, and each case is registered on its own in `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` as `add_test(NAME <case> COMMAND Zenith_Integration_XrRuntime --test-case=<case>)` with `SKIP_RETURN_CODE 77`, so a host without a runtime reports `Not Run (Skipped)`, never `Passed`. The tickets carry `needs-gpu` and `gpu-reason:requires-graphics-test`.

| Test | Case asserted |
|---|---|
| `Test_XrRuntime_SessionReachesFocused` | the active runtime (on a CI host: a simulated headset, § "Reference device matrix") reaches FOCUSED within 10 s, 600 frames submit with validation clean, the session ends cleanly at shutdown |
| `Test_XrRuntime_BothBackends` | the same on Vulkan and on D3D12, in two processes |
| `Test_XrRuntime_MissedPeriodsBounded` | over 600 frames of W-Indoor the missed-period count is at most 6 (1%) on the reference desktop |

RPROD's `Test_FluxRenderer_StereoFrameOnOpenXR` and GAME's `Test_XRGameplay_DirectorDrivesStereoPair` run through the same frame source; ZenithXR's integration is their XR runtime (rows RPB21, GmB23).

## Reference device matrix

| Headset | Runtime | RHI backends | Role |
|---|---|---|---|
| Meta Quest 3 over Link (USB-C) | Meta's PC OpenXR runtime | Vulkan, D3D12 | primary device |
| Meta Quest 3 over Steam Link | SteamVR's OpenXR runtime | Vulkan, D3D12 | second runtime on the same headset |
| Valve Index | SteamVR's OpenXR runtime | Vulkan, D3D12 | second headset, Index controllers |
| no headset | Monado with its simulated headset driver, or Meta's XR Simulator | Vulkan (Monado), D3D12 and Vulkan (XR Simulator) | the real-runtime integration on a GPU host without a headset |

All on the reference desktop (RTX 3070, `Zenith2/Docs/Budgets.md` → "Reference hardware"). A runtime or headset outside the matrix is best-effort.

## Device runs

`needs-human` runs, one record per run under `Zenith2/Docs/Research/Scope/XrDeviceRuns/<yyyy-mm-dd>-<device>-<runtime>-<backend>.md`, whose first table has the rows: tree SHA, build configuration, headset, runtime and version, backend, game, duration, result, the measured pending budget rows, the counters `xr.frames.missed_periods`, `xr.session.losses` and `xr.fallback.mono_frames`.

| Run | Content |
|---|---|
| `Device_Xr_OpenWorldSliceTraversal` | 20 minutes of OpenWorldSlice's playthrough in the headset, per matrix row: no crash, no session loss, the budget rows measured |
| `Device_Xr_ControllersAndHaptics` | every catalogue control of each controller pressed once in the input debug overlay, both hands' haptics felt, recentre from the console |
| `Device_Xr_HeadsetAfterLaunch` | the game started with the headset's runtime running but the headset disconnected; connecting it recreates the device once (the screen blanks for the recovery) and the session reaches FOCUSED |
| `Device_Xr_FocusAndRemoval` | the runtime's system menu (VISIBLE: worlds pause), headset removed and replaced (IDLE and back), the runtime quit (EXITING: the game keeps running mono) |
| `Device_Xr_DeviceLossRecovery` | a forced device removal (the RHI's fault injection in a development build): the session ends at the loss, the runtime shows its own not-responding state while the engine recovers, the desktop window shows the recovery, and after `RESUMING` a new session reaches FOCUSED and renders in the headset |

## Gate steps per test group

| Group | Step |
|---|---|
| register validity | `Test` (every category) |
| register presence | stage acceptance (`Run-Committed.ps1 -Acceptance -Stage N`) |
| `ScopeChecks` | `Test` on `win` |
| `ZenithXR_Tests` | `Test.ps1 -Label z2-xr` in `Zenith2Render` (PROC's Stage 7 line) and the ordinary `Test` step |
| `z2-death-assert` cases | the death suites of the `Test` step |
| fuzz | `win-asan`, the acceptance fuzz campaign |
| real-runtime integration | `Test` in the windowed GPU configuration (Stage 7 acceptance), skipped with 77 elsewhere |
| device runs | Stage 7 acceptance, `needs-human` |
