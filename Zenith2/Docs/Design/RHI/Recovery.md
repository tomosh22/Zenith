# RHI design: presentation, recovery and pipeline caches

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`. It specifies presentation (the swapchain model, present modes and formats, acquire and present, the frame-pacing hooks), swapchain recreation including Android surface loss, device-loss detection, breadcrumbs, the device-loss report, the recover-or-clean-exit policy, the persistent pipeline-cache store, and captures.

## Scope of this file

The specification is `Zenith2/Docs/Rendering.md → "Recovery"`, `Zenith2/Docs/Rendering/GpuResources.md → "Swapchain recreation"`, "Device loss" and "Pipeline cache invalidation", `Zenith2/Docs/Rendering/ShaderSystem.md → "Persistent pipeline caches"`, and `Zenith2/Docs/Architecture/PlatformLayer.md → "Android lifecycle and surface recovery"`. Flux files are in `Zenith2/Engine/ZenithFlux/Internal/Recovery/` and `Internal/PipelineCache/`.

## Swapchain model

`Flux_SwapchainManager` owns one `Zenith_RHISwapchain` per rendering window (the main window in a game; every editor viewport window in the editor, at most 16 by `uPLATFORM_MAX_WINDOWS`), keyed by `Zenith_WindowID`, each with its own resize and display-scale handling (`Zenith2/Docs/Design/PLAT/Platform.md` → "Multiple windows"; that design is PLAT's, cited under the assumption of entry document "Dependencies").

| Element | Rule |
|---|---|
| creation | startup step 13 (Flux renderer creation) creates the main window's swapchain from `rhi.presentMode` and `rhi.output`, with the image count derived from the device's `GetFramesInFlight()` (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHISwapchain header"`); editor windows' swapchains are created when the window first renders |
| output view | `VIEW_KIND_MAIN` and game-view `VIEW_KIND_EDITOR` views whose output is `FLUX_VIEW_OUTPUT_SWAPCHAIN` import the acquired image as the present pass's target |
| pre-rotation | on Android the present pass writes the final image rotated by `m_eTransform` (identity on Windows), so the compositor does no rotation; views render in the window's logical orientation |
| clip-space convention | +Y up and depth [0, 1] on both APIs; Vulkan's negative-height viewport realises it (`Zenith2/Docs/Design/RHI/Vulkan.md → "Command recording mapping"`) |

## Present modes and formats

`rhi.presentMode` selects FIFO (the default, vsync), FIFO_RELAXED, MAILBOX or IMMEDIATE; each backend maps it and falls back as its file specifies, and `GetInfo().m_ePresentMode` reports the mode in use, which the frame-pacing controller (RPROD) reads. `rhi.output` selects SDR sRGB, HDR10 (ST.2084, BT.2020) or scRGB (linear, BT.709 primaries); the tonemap pass's output transform follows `GetInfo().m_eColourSpace` (`Zenith2/Docs/Rendering.md → "Frame anatomy common epilogue"`). Every swapchain format is in the boot PSO group of every pass that writes the swapchain ([Z2-N-REND-985]), so changing the output at runtime causes no PSO miss.

<!-- elaboration: the colour-space report and the HDR metadata call (LIGHT's L21) -->
| Element | Rule |
|---|---|
| colour-space report | every creation and recreation fills `Zenith_RHISwapchainInfo::m_uOutputColourSpacesSupported` with bit `1 << (eSpace − 1)` for each `Zenith_RHIOutputColourSpace` whose format and colour-space pair the surface lists (Vulkan: `vkGetPhysicalDeviceSurfaceFormatsKHR`; D3D12: `CheckColorSpaceSupport` with `DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT`), SDR always set; `m_bDisplayHdr` is D3D12's `IDXGIOutput6::GetDesc1().ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020` for the output holding the window, and on Vulkan whether the surface lists `HDR10_ST2084_EXT` (the only signal the API gives); `m_fDisplayMaxLuminanceNits` is `DXGI_OUTPUT_DESC1::MaxLuminance` on D3D12 and 0 (unknown) on Vulkan. LIGHT's output-mode selection reads the main window's values (`Zenith2/Docs/Design/LIGHT/ColourManagement.md → "Output mode selection"`) |
| HDR metadata | `Flux_SwapchainManager::SetHDRMetadata(const Flux_HDRMetadata& xMetadata)` (main thread, prologue step 3 where LIGHT applies its configuration, a render-idle point) copies LIGHT's value into a `Zenith_RHIHDRMetadata` field for field and records it; when the main window's swapchain is HDR10 it calls `Zenith_RHISwapchain::SetHDRMetadata` at once, and every recreation of an HDR10 swapchain calls it again with the recorded value; SDR and scRGB swapchains are sent none. Vulkan: `vkSetHdrMetadataEXT` with `VkHdrMetadataEXT` (primaries, white point, mastering luminance, MaxCLL, MaxFALL) when `Zenith_RHICapabilities::m_bHdrMetadata` (`VK_EXT_hdr_metadata` enabled), else `FEATURE_UNAVAILABLE`, which the manager logs once in `rhi.swapchain` and ignores; D3D12: `IDXGISwapChain4::SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, …)` with the chromaticities in units of 0.00002, the mastering luminances in 0.0001 nits and MaxCLL/MaxFALL in nits, as the DXGI structure defines them |

## Acquire and present

| Node | Call | Result handling |
|---|---|---|
| `Flux.Submit` step 1 | `AcquireNextImage(frameSlot)` | success: import the image; `SWAPCHAIN_OUT_OF_DATE`, `SURFACE_UNAVAILABLE` or `TIMEOUT`: mark the swapchain for recreation (`RHI.Swapchain.OutOfDate`), render into the stand-in (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Submission"`), skip the present; `DEVICE_LOST`: the device-loss path |
| `Flux.Present` | `Present(image)` | success; `IsRecreateRecommended()` true after `VK_SUBOPTIMAL_KHR` or a size change: mark for recreation; `SWAPCHAIN_OUT_OF_DATE`: mark; `DEVICE_LOST`: the device-loss path |
| frame prologue, main thread | the swapchain manager's `ProcessPendingRecreations()`, called by `Flux_Renderer::RunPrologue()` in prologue step 4 (entry document "Render-idle points", assumption pending MOD, ARCH (X128)) | runs the recreation of every marked swapchain before `RunFrame`, where no render subgraph is running ([Z2-N-ARCH-386]) |

## Frame pacing hooks

The frame-pacing algorithm and its row `Z2-B-RPROD-020` are RPROD's. The RHI provides:

| Hook | Vulkan | D3D12 |
|---|---|---|
| `WaitForLatencySlot(timeout)` | `WaitForPresent(presentID − framesInFlight)` with `VK_KHR_present_wait`, otherwise `ZENITH_RHI_WAIT_STATUS_REACHED` at once | the frame-latency waitable object |
| `WaitForPresent(id, timeout)` | `vkWaitForPresentKHR` | `FEATURE_UNAVAILABLE` |
| `GetPresentTimings(out)` | `vkGetPastPresentationTimingGOOGLE` (Android) | `GetFrameStatistics` |
| `SetMaximumFrameLatency(n)` | records n; the Vulkan wait above uses it | `IDXGISwapChain2::SetMaximumFrameLatency` |
| `SetDesiredPresentTime(ulPresentID, ulDesiredNs)` (RP8) | with `VK_GOOGLE_display_timing` (`m_bPresentTiming`): records the pair; the next `Present` whose image carries `m_ulPresentID == ulPresentID` chains `VkPresentTimesInfoGOOGLE` with one `VkPresentTimeGOOGLE { presentID = (uint32) ulPresentID, desiredPresentTime }` into `VkPresentInfoKHR::pNext`, the time converted from the monotonic clock with the inverse of the mapping `GetPresentTimings` applies; a recorded ID that no present carries by the second present after it is dropped; without the extension `FEATURE_UNAVAILABLE` | `FEATURE_UNAVAILABLE` |
| `SetSwapInterval(uInterval)` (RP8) | 1 to 4, else `INVALID_DESC`; recorded and returned as success; FIFO presents once per vertical blank and ignores it (the pacer's desired present time is the Vulkan pacing path) | 1 to 4, else `INVALID_DESC`; the sync interval of every subsequent `Present` while the mode is FIFO (`Present(uInterval, 0)`); MAILBOX and IMMEDIATE keep interval 0 and the value is recorded for the next switch back to FIFO |

The swap interval starts at 1, survives `Recreate`, and is read back through `GetInfo().m_uSwapInterval`. Every present-timing value is converted to the monotonic clock (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Clocks"`) so the pacing controller and the latency measurement ([Z2-N-BUDGET-102] `latencySamples`) read one time base. The hooks run on the render-submit thread in `Flux.Present` alone, before that frame's `Present`.

## Swapchain recreation

The triggers and the five steps are `Zenith2/Docs/Rendering/GpuResources.md → "Swapchain recreation"`; the recreation runs on the main thread in the frame prologue, a render-idle point (`Zenith2/Docs/Design/RHI.md → "Render-idle points"`), which is where no frame's render subgraph runs. Its GPU waits are main-thread waits, which [Z2-N-REND-486] does not yet admit; they are the permitted waits of `Zenith2/Docs/Design/RHI.md → "Permitted main-thread GPU waits"` (reconciliation item R11).

### Recreation state machine

| State | Event | Next | Action |
|---|---|---|---|
| `LIVE` | out of date, suboptimal, resize, DPI or output change, present-mode, output or frames-in-flight setting change | `PENDING` | mark; the current frame renders into the stand-in if it has not acquired |
| `PENDING` | frame prologue | `DRAINING` | stop acquiring |
| `DRAINING` | — | `RECREATING` | (a main-thread wait, RHI.md "Permitted main-thread GPU waits"; elaboration) for every trigger but a frames-in-flight change — Vulkan: wait on the graphics timeline for the final values of every in-flight frame that acquired from this swapchain; D3D12: wait for the direct queue to finish every frame (every back-buffer reference released) ([Z2-N-REND-983]); no device-wide idle on Vulkan and no `WaitIdle`. A frames-in-flight change drains every swapchain of the device and then idles every queue through `Zenith_RHIDevice::WaitIdle`, a CPU wait on each queue's last-signalled timeline value, never `vkDeviceWaitIdle` (<!-- assumption: pending core-doc batch 4 (X89) --> the one trigger [Z2-N-REND-983] is to admit, "Reconciliation items" R17) |
| `RECREATING` | size 0 or minimised | `SUSPENDED` | no swapchain; the window's views produce no frames and record no GPU work ([Z2-N-REND-984]) |
| `RECREATING` | size > 0 | `LIVE` | recreate (`Recreate(desc)`; Vulkan with `oldSwapchain`, the old images enqueued on the retirement queue from the main thread with the graphics value of the last frame that used them, already completed, so they retire at the next `RetireCompleted` — an `Enqueue` the retirement queue's contract admits at a render-idle point; D3D12 `ResizeBuffers` or recreation for a format or colour-space change); a frames-in-flight change (`r.framesInFlight` differs from `GetFramesInFlight()`) runs the procedure of entry document "Changing the frames in flight": `WaitIdle()`, `SetFramesInFlight(r.framesInFlight)` (the device's per-slot objects: breadcrumb regions, command pools, split-barrier event pools, acquire semaphores, the old ones destroyed at once; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "Zenith_RHIDevice header"`), `Flux_FrameLifetime::OnFramesInFlightChanged` (the discarded preparation of the next frame, upload segments, timestamp pools, the re-preparation on slot 0), then every swapchain recreated with the new image count; a failure of `SetFramesInFlight` or `OnFramesInFlightChanged` keeps the old value and objects, logs in `rhi.swapchain` and follows the `SWAPCHAIN_CREATE` row; update every swapchain-output view's output extent; invalidate the histories of exactly the views whose render extent changed ([Z2-N-REND-599]); `RHI.Swapchain.Recreations` increments |
| `RECREATING` | `Recreate` fails with `SWAPCHAIN_CREATE` | `PENDING` | retry at the next prologue; after 3 consecutive failures, the device-recovery policy ([Z2-N-ARCH-915]) |
| `SUSPENDED` | the window has a size again | `PENDING` | as a resize |
| any | Android surface loss (`OnReleaseSurface`) | `SURFACE_LOST`, or `SURFACE_STALE` when the wait ran out of the handshake budget | "Android surface loss" below |

The first frame after recreation is a normal frame (step 5 of the specification); transients re-key through the graph caches because their descriptions changed.

### Android surface loss

`Flux_SwapchainManager` implements the surface members of `Zenith_AppLifecycleListener` (`Zenith2/Docs/Design/PLAT/Android.md` → "Surface-loss handshake"), which the runtime forwards to it:

| Callback (main thread) | Action |
|---|---|
| `OnReleaseSurface` | a render-idle point (the runtime completed the in-flight render subgraph first); wait on the graphics timeline for every in-flight frame that used the window (a permitted main-thread wait, RHI.md "Permitted main-thread GPU waits") with the handshake's remaining budget — 2,000 ms of [Z2-N-ARCH-916] minus the time the handshake has already spent, read from the monotonic clock at the callback's entry — as its one timeout, with no repeat and no loss declaration; completed: `ReleaseSurface()` (destroy the images, the swapchain and the `VkSurfaceKHR` at once, a named immediate destruction of RHI.md "Rules across the API"), state `SURFACE_LOST`; the device and every resource stay alive ([Z2-N-ARCH-914]). Timed out: return without releasing, count `RHI.Wait.Timeouts`, state `SURFACE_STALE`; the platform then marks the surface lost ([Z2-N-ARCH-916]) |
| a frame prologue while `SURFACE_STALE` | acquire and present return `SWAPCHAIN_OUT_OF_DATE` without an API call; once the frames that used the window have completed (a zero-timeout check), `ReleaseSurface()` runs and the state becomes `SURFACE_LOST`; the timeout rules of "Device loss detection" apply to those frames as to any other, so a hang is still detected by the render-submit waits |
| a frame while `SURFACE_LOST` or while the platform marks the surface lost | acquire and present return `SWAPCHAIN_OUT_OF_DATE` without an API call ([Z2-N-ARCH-916]); the main view renders nothing (the view list omits swapchain outputs) |
| `OnRecreateSurface` | when still `SURFACE_STALE`, wait for the stale swapchain's frames (`rhi.gpuWaitTimeoutMs` and the loss check) and `ReleaseSurface()` first; create the surface from the new window (`CreateVulkanSurface`) and a swapchain for its size; state `LIVE`; invalidate size-changed histories ([Z2-N-ARCH-915]); a failure follows the recreation-failure row above |

## Device loss detection

| Source | Where it is seen | Effect |
|---|---|---|
| submit, wait, acquire or present returns `VK_ERROR_DEVICE_LOST` or `DXGI_ERROR_DEVICE_REMOVED`/`_RESET`/`_HUNG` | the backend call | the device's status becomes `ZENITH_RHI_DEVICE_STATUS_LOST`; the call returns `ZENITH_RHI_ERROR_DEVICE_LOST` |
| a `WaitCPU` timeout (`rhi.gpuWaitTimeoutMs`, default 2 s) | `Flux_FrameLifetime`, the retirement queue, the swapchain drain | the caller checks status (Vulkan loss-probe fence, D3D12 `GetDeviceRemovedReason`); lost: as above; not lost: `RHI.Wait.Timeouts` increments and the wait repeats, and after 5 consecutive timeouts on one value (10 s) the device is declared lost with reason `gpu hang (timeout)` |
| the fault injector's `SUBMIT` site | tests | as the first row |

Once lost, every device call returns `DEVICE_LOST`, no submission is made to the device again ([Z2-N-REND-987]), and `Flux_DeviceLossHandler::OnDeviceLost()` is called once, on the render-submit thread, which stops the render subgraph (the remaining render nodes of the frame return at once) and signals the main thread through the runtime's device-loss flag — the handler's state, stored `FLUX_DEVICE_LOSS_STATE_LOST` before `OnDeviceLost` returns and polled through `Flux_Renderer::GetDeviceLossState()` (entry document "Render-idle points", assumption pending MOD, ARCH (X128)) — read in the next frame prologue.

The render-idle flag is cleared on the loss path too, so the handler's entry assert of the next prologue holds: when the loss is seen by `PrepareNextFrame`'s step-2 wait, `PrepareNextFrame` stores `m_bSubgraphActive = false` before it returns `ZENITH_RHI_ERROR_DEVICE_LOST`; when it is seen earlier (submit, acquire or present), the remaining nodes return at once and `Flux.Present` skips its present and `PrepareNextFrame`; in every case `Flux.Present` runs `Flux_FrameLifetime::OnSubgraphEnd()` as its last statement (entry document "Frame-in-flight rotation"). `IsRenderIdle()` is therefore true at the first prologue after the loss.

## Breadcrumbs

<!-- elaboration: the index range, the wrap rule and the mechanism order -->
| Element | Rule |
|---|---|
| buffer | per queue, one `READBACK`-class buffer (Vulkan: `HOST_VISIBLE \| HOST_COHERENT`, `HOST_CACHED` where exposed; D3D12: readback heap) of `framesInFlight × 2 × 4,096 × 4` bytes: per frame slot a begin array and an end array of 4,096 `uint32` entries indexed by the pass's per-queue breadcrumb index |
| write | `WriteBreadcrumb(pass, BEGIN)` stores `(uint32) frameIndex` into begin[pass] before the pass body, `END` into end[pass] after it |
| Vulkan mechanism | `vkCmdWriteBufferMarker2AMD` with stage `TOP_OF_PIPE` for begin and `BOTTOM_OF_PIPE` for end when `VK_AMD_buffer_marker` is present; else `vkCmdSetCheckpointNV` with the packed value when `VK_NV_device_diagnostic_checkpoints` is present (read with `vkGetQueueCheckpointData2NV`); else `vkCmdFillBuffer` of 4 bytes, recorded outside rendering scopes, so a merged scope reports at scope granularity ([Z2-N-REND-986] table) |
| D3D12 mechanism | `WriteBufferImmediate` with `D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN` for begin and `MARKER_OUT` for end |
| passes above 4,096 in one queue-frame | the index wraps to `4,095`, reporting at "≥ 4,095" granularity; counted in `RHI.Breadcrumbs.Overflow` |
| read | `CollectDeviceFaultReport`: for each queue, the most recent frame slot with a submission; the last completed pass is the highest index p with end[p] = frame; the first incomplete pass is the lowest index p with begin[p] = frame and end[p] ≠ frame |
| mapping | the report's pass indices map to pass names through the per-frame breadcrumb name table (a copy of the compiled graph's per-queue pass names, kept in the dump ring of `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Frame dump back-end sections"`; in Shipping a separate 3-frame name ring of pass-name pointers, which are string literals) |

`rhi.breadcrumbs` defaults to true in every build; the per-pass cost is two 4-byte writes.

## Device-loss report

`Flux_DeviceLossReportWriter` writes, on the main thread, after `CollectDeviceFaultReport`:

| Artefact | Path | Content |
|---|---|---|
| report JSON | `user:/crash/device_loss_<utc>.json` (the user data directory; on Windows development builds also `Zenith2/out/<checkout-hash>/DeviceLoss/`) | `{ "schema": "zenith2.device-loss", "schemaVersion": 1, "frame", "backend", "adapter", "driver", "agilitySdk", "reason", "queues": [ { "queue", "lastCompletedPass", "firstIncompletePass" } ], "fault": { "available", "description", "address", "vendorCode" }, "dred": { … D3D12 only }, "recovered": bool, "lossCountInWindow" }` with pass names resolved |
| frame dumps | beside the report, `device_loss_<utc>_frame_<n>.json` for the last three frames kept in the dump ring (development builds) | the frame dump of [Z2-N-REND-314] |
| log | `rhi.recovery` at `LOG_LEVEL_ERROR` | one line per queue: `device lost: <queue> last completed '<pass>' first incomplete '<pass>' (frame <n>)` |
| crash report | the DEVT crash report attaches the JSON (`Zenith2/Docs/Design/DEVT.md`, format owned there) | — |

<!-- elaboration -->
The writer allocates only from a 256 KiB arena reserved at renderer creation, so a report is written even when the general heap is under pressure. Recovery diagnostics also publish `RHI.Device.Losses`, `RHI.Device.Recoveries` and the report's reason code to telemetry.

## Recover-or-clean-exit policy

The six steps are `Zenith2/Docs/Rendering/GpuResources.md → "Device loss"` (heading "Recover-or-clean-exit policy"). `Flux_DeviceLossHandler` runs them on the main thread in the frame prologue after the loss is signalled, from its `RunPrologue()`, which `Flux_Renderer::RunPrologue()` calls in prologue step 4 (entry document "Render-idle points", assumption pending MOD, ARCH (X128)); each call handles at most one prologue event of the table below and asserts `Flux_FrameLifetime::IsRenderIdle()` on entry. The same handler also takes the frame-failure exit, which is not a device loss. The handler's state is `Flux_DeviceLossState` (`Zenith2/Docs/Design/RHI/PublicAPIDevice.md → "RHI-owned ZenithFlux public headers"`), one enumerator per state of the table below, which `GetState()` returns and the runtime polls.

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Recovery/Flux_DeviceLossHandler.h
class Flux_DeviceLossHandler                       // owned by Flux_Renderer, one per renderer
{
public:
	void OnDeviceLost();                              // render-submit thread, once per loss: state = LOST (release store); the reason is the device's report
	void RunPrologue();                               // main thread, prologue step 4, a render-idle point: one prologue event of the table
	Flux_DeviceLossState GetState() const;   // any thread (acquire load)
private:
	std::atomic<uint8> m_eState = FLUX_DEVICE_LOSS_STATE_NORMAL;
	// the loss times of the 60 s window, the policy, the 10 s exit deadline
};
```

### Device-loss state machine

| State | Event | Next | Action |
|---|---|---|---|
| `NORMAL` | loss signalled | `LOST` | step 1: rendering stopped (already); the runtime pauses every world at its next tick boundary (the simulation clock returns zero ticks); record the loss time |
| `NORMAL` | exit requested by a frame failure: the prologue call reads `Flux_FrameLifetime::GetExitRequest()` non-zero (stored by the build node's 30th consecutive `FailFrame`, `Zenith2/Docs/Design/RHI/Memory.md → "Frame failures"`) | `EXITING` | step 6 with the reason's message instead of the device-loss message: `The renderer could not fit a frame's uploads into memory. Your progress has been saved.` for `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`, `The graphics device ran out of memory. Your progress has been saved.` for `FLUX_FRAME_FAILURE_DEVICE_OOM`; the device is not lost, so `OnDeviceLost` is not called, no device-loss report is written (no `REPORTING`), and neither `RHI.Device.Losses` nor `RHI.Device.Recoveries` increments; the reason and the consecutive count go to telemetry (entry document "Telemetry"); `EXIT_CODE_DEVICE_LOST` (6) stays the code ("Reconciliation items" R3); the 10 s fatal fallback of step 6 applies unchanged |
| `LOST` | prologue | `REPORTING` | step 2: the report above |
| `REPORTING` | done, policy `recover`, no earlier loss within 60 s | `RECOVERING` | — |
| `REPORTING` | done, policy `exit`, or a second loss within 60 s of the first | `EXITING` | — |
| `RECOVERING` | — | `REBUILDING` | step 3, a render-idle point: release every Flux-owned GPU object without GPU waits (the device is lost) — Flux enqueues nothing: it clears the retirement listener, destroys its RHI-owning components (which drop their handles), the swapchains and the pipeline cache (its last good file on disk is kept), then the device, whose `LOST` destruction releases every remaining object, the retirement ring's pending entries included, without a wait (entry document "Device status state machine"); generational handles held elsewhere now fail with `STALE_HANDLE` safely; create a new device with the same `Zenith_RHIDeviceDesc` (same backend and adapter filter) and new swapchains |
| `RECOVERING` | device or swapchain creation fails | `EXITING` | — |
| `REBUILDING` | — | `RESUMING` | step 4: the asset system reloads every resident GPU resource through its residency set (the loader interface Flux implements, `Zenith2/Docs/Design/ASSET.md`); the boot, global and current-scene PSO groups are recreated through the persistent cache; the GPU scene rebuilds from a full-state render-world extraction instead of deltas: the handler calls `Flux_RenderWorldRing::RequestFullExtraction()`, and the next world, sealed with `m_bFullState`, is consumed by `Flux_GPUScene::ApplyDeltas` as `Zenith2/Docs/Rendering.md → "Render world resync"` step 4 specifies ([Z2-N-REND-015]; `Zenith2/Docs/Design/RHI/Memory.md → "GPU scene implementation"`) |
| `RESUMING` | the boot group ready | `NORMAL` | step 5: invalidate every view history; unpause the worlds; `RHI.Device.Recoveries` increments |
| `REBUILDING` | a reload or PSO recreation fails | `EXITING` | — |
| `EXITING` | — | (process exit) | step 6: request an autosave through the save system at the next tick boundary (the save path touches no GPU state); show the message `The graphics device stopped responding and could not be recovered. Your progress has been saved.` through the platform's message box (the UI cannot render); request shutdown with `EXIT_CODE_DEVICE_LOST` (6); if the autosave and shutdown have not finished within 10 s, `ZENITH_FATAL(FATAL_CODE_DEVICE_LOST, …)`, the fatal being the fallback alone |

- During `LOST` to `RESUMING`, and while `EXITING`, the render subgraph is empty (the frame loop runs `RunFrame` with `bRunRender = false` because `Flux_Renderer::GetDeviceLossState()` is not `FLUX_DEVICE_LOSS_STATE_NORMAL`; the term is assumed of MOD and ARCH, entry document "Render-idle points", X128), so simulation keeps its determinism; the pause is at a tick boundary, never mid-tick.
- The frame-failure exit request is stored with `std::memory_order_release` on the build node and read with acquire in the prologue; the iteration barrier ([Z2-N-ARCH-386]) already orders the two, so the atomic only makes the read well-defined. A request is taken in the first prologue after the frame that stored it, and no further frame builds.
- A loss during `REBUILDING` or `RESUMING` counts as the second loss.
- The clean exit uses the new enumerator `EXIT_CODE_DEVICE_LOST = 6` ("the graphics device was lost and could not be recovered; autosave requested"), which cross-document item X72 adds to `Zenith2/Docs/Architecture/CompositionRoot.md → "Startup sequence"` (table "Exit codes") and to the Contracts and MOD wording (pending); `EXIT_CODE_DEVICE` (4) stays "a required device could not be created", the factory refusal of startup step 12, so CI can tell a runtime loss from a missing driver ([Z2-N-ARCH-852]). The frame-failure exit takes the same code: the enumerator's text gains "or 30 consecutive frames could not be rendered", which entry document "Reconciliation items" R3 records beside X72.

## Pipeline cache store

`Flux_PipelineCacheStore` (`ZenithFlux/Internal/PipelineCache/`) owns the device's `Zenith_RHIPipelineCache` and its file.

### Cache key and file naming

The file names and the Zenith2 file header are `Zenith2/Docs/Rendering/ShaderSystem.md → "Persistent pipeline caches"`: `vk_<vendor>_<device>_<driverVersion>_<cacheUUIDhex>_<containerVersion>.bin` and `d3d12_<vendor>_<device>_<umdVersion>_<agilitySDKVersion>_<containerVersion>.bin` under `<UserCache>/Zenith2/<Game>/PipelineCache/`, where `<UserCache>` is the platform's user cache mount (`user:/cache/`). Every number is lowercase hexadecimal without prefix; the header is serialised field by field, little-endian ([Z2-N-REND-982]).

### Load and invalidation

| Step (renderer creation, after the device) | Rule |
|---|---|
| 1 | compute the current key from the capabilities and `uFLUX_SHADER_CONTAINER_VERSION` |
| 2 | delete every file in the directory whose name is not the current key's ([Z2-N-REND-207]); log `graphics driver changed: pipeline caches rebuilding` when a deleted file had the same vendor and device but another driver version, and persist the last driver version seen in `<UserCache>/Zenith2/<Game>/PipelineCache/last_driver.json` ([Z2-N-REND-990]) |
| 3 | read the current file: absent, a short read, a magic, header-version, vendor, device, driver, UUID or Agility mismatch, a container-version mismatch, or a payload hash mismatch (`Zenith_Hash64` of the payload, D-SEL-HASH (open, Stage 1)) deletes it and starts empty |
| 4 | `CreatePipelineCache(payload)`; `WasLoadRejected()` (the driver ignored it) deletes the file and continues with the empty cache |
| 5 | any discard sets the run's cold-PSO-cache telemetry flag and logs once ([Z2-N-REND-989]); device creation never fails because of the cache ([Z2-N-REND-205]) |

### Write cadence

The cache is serialised on a background job (`Zenith2/Docs/Design/JOBS/PublicAPI.md → "Zenith_JobSystem"`, background operations) at clean shutdown and at most once per 60 s after new PSOs were created, into a job arena, then written through the platform's journaled atomic write (temporary file, flush, rename), so an interrupted write leaves the previous file ([Z2-N-REND-206]). When the payload exceeds `Z2-B-RPROD-030` the file is deleted and the cache rebuilds from empty on the next run instead of being trimmed ([Z2-N-REND-207]).

## Captures

| Element | Rule |
|---|---|
| availability | development builds with `rhi.capture` set and the tool present; `Zenith_RHIDevice::GetCaptureTool()` returns it; Shipping ignores the key, loads no capture library and exposes no capture console command (`Zenith2/Docs/Architecture.md → "Security architecture"`: no debugging surface in shipping builds) |
| trigger | the console command `r.capture <frames>` (1 to 8, default 1) and the on-screen profiler's "Capture" button set `Flux_CaptureController::m_uFramesToCapture`; the capture begins at the start of the next render subgraph (`Flux.BuildGraph` sets the flag, `Flux.Submit` of that frame calls `BeginFrameCapture` before its first submission) and ends in `Flux.Present` after the last captured frame's present (`EndFrameCapture`) |
| naming | the capture title is `<game> frame <index> <backend>` |
| automated | a parity failure in a development run with a tool attached captures the failing frame's re-render (`Zenith2/Docs/Rendering.md → "Validation roles"`, [Z2-N-REND-943] artefacts) |
| markers | every pass is a debug label, every partition and batch a label, so the capture's event list mirrors the frame dump |
