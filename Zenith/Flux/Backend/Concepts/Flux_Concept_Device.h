#pragma once

#include "Flux/Flux.h"

// Concept: device lifecycle, VRAM registry, top-level GPU state.
//
// What the engine layer calls on the backend "device" type (aliased as
// Flux_PlatformAPI). Methods are now INSTANCE methods because the backend
// type lives on g_xEngine (e.g. g_xEngine.Vulkan()) rather than a process-
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
//   - BeginFrame() — no longer called from engine code; the Vulkan begin
//     sequence runs via FluxRenderer's RegisterBeginFrameCallback registered
//     during Initialise. The contract is therefore "Initialise registers a
//     begin-frame callback," not "BeginFrame exists."
//   - InitialiseImGui / ShutdownImGui / ImGuiBeginFrame — tools-only;
//     conformance-asserted only in tools builds.

template <typename T>
concept FluxBackendDevice = requires(
	T t,
	bool b,
	uint32_t u,
	Flux_VRAMHandle xVRAMHandle,
	const Flux_ShaderResourceView& xView,
	const Flux_Sampler& xSampler)
{
	{ t.Initialise()                                                 } -> std::same_as<void>;
	{ t.InitialiseScratchBuffers()                                   } -> std::same_as<void>;
	{ t.WaitForGPUIdle()                                             } -> std::same_as<void>;
	{ t.EndFrame(b)                                                  } -> std::same_as<void>;
	{ t.GetVRAM(xVRAMHandle)                                         } -> std::same_as<Flux_VRAM*>;
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
