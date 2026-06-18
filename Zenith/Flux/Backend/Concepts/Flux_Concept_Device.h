#pragma once

#include "Flux/Flux.h"

// Concept: device lifecycle, VRAM registry, top-level GPU state.
//
// What the engine layer calls on the backend "device" type (aliased as
// Flux_PlatformAPI). Methods are now INSTANCE methods because the backend
// type lives on g_xEngine (e.g. g_xEngine.FluxBackend()) rather than a process-
// wide static facade. The requires-clause uses t.Method(args) — t is a
// synthetic instance whose only purpose is concept satisfaction; the
// actual call sites use g_xEngine.X().Method(args).
//
// Bindless and ImGui-texture wrappers live here because they are the
// engine's portable entry points into backend-private bindless / ImGui-
// integration plumbing — keeping the concept dependency-free of
// vk::ImageView / vk::DescriptorSet etc.
//
// Methods explicitly NOT in this concept (and reasons):
//   - GetDevice() / GetQueue() / GetCommandPool() / GetDescriptorPool*()
//     — return raw vk::* types. Backend-internal; engine code must not call.
//   - GetVRAM() — resolves a Flux_VRAMHandle to the backend-internal VRAM
//     record (Flux_VRAM*). Engine code holds handles and frees them through
//     the FluxBackendMemoryDelete concept (QueueVRAMDeletion), never the raw
//     record, so GetVRAM stays backend-private.
//   - BeginFrame() — the device exposes no such method. Per-frame begin work
//     (fence wait, pool reset, deletion drain, scratch reset) is the
//     PerFrameBegin() method (in this concept), called directly each frame by
//     Flux_RendererImpl::BeginFrame.
//   - InitialiseImGui / ShutdownImGui / ImGuiBeginFrame — tools-only;
//     conformance-asserted only in tools builds.
//
// RecordFrame(workDist) is the direct-recording seam: Flux_RendererImpl::RecordFrame
// (driven from Flux_RenderGraph::Execute) calls it to record every queued pass's
// callback directly into the backend's per-worker command buffers. The Vulkan
// backend records the worker command buffers in parallel; the D3D12 null backend
// runs the callbacks into a no-op command buffer (so callback side effects still
// occur). EndFrame then only submits.

template <typename T>
concept FluxBackendDevice = requires(
	T t,
	bool b,
	uint32_t u,
	const Flux_WorkDistribution& xWork,
	const Flux_ShaderResourceView& xView,
	const Flux_Sampler& xSampler)
{
	{ t.Initialise()                                                 } -> std::same_as<void>;
	{ t.InitialisePerFrameResources()                                } -> std::same_as<void>;
	{ t.WaitForGPUIdle()                                             } -> std::same_as<void>;
	{ t.PerFrameBegin(u)                                             } -> std::same_as<void>;
	{ t.RecordFrame(xWork)                                           } -> std::same_as<void>;
	{ t.EndFrame(b)                                                  } -> std::same_as<void>;
	{ t.WriteBindlessTextureSlot(u, xView, xSampler)                 } -> std::same_as<void>;
};

// Tools-only device methods (ImGui integration).
template <typename T>
concept FluxBackendImGuiTools = requires(
	T t,
	const Flux_ShaderResourceView& xView,
	const Flux_Sampler& xSampler)
{
	{ t.InitialiseImGui()                                            } -> std::same_as<void>;
	{ t.ShutdownImGui()                                              } -> std::same_as<void>;
	{ t.ImGuiBeginFrame()                                            } -> std::same_as<void>;
	{ t.CreateImGuiTextureID(xView, xSampler)                        } -> std::same_as<uint64_t>;
};
