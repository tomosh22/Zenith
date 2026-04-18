#pragma once

#include "Flux/Flux.h"
#include "Zenith_PlatformGraphics_Include.h"

// Concept: device lifecycle, VRAM registry, top-level GPU state.
//
// What the engine layer calls on the backend "device" type (aliased as
// Flux_PlatformAPI). Static methods because the device is a process-wide
// singleton in every backend; the requires-clause uses T::Method(args)
// rather than t.Method(args).
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
//     sequence runs via Flux_PerFrame::RegisterBeginFrameCallback registered
//     during Initialise. The contract is therefore "Initialise registers a
//     begin-frame callback," not "BeginFrame exists."
//   - InitialiseImGui / ShutdownImGui / ImGuiBeginFrame — tools-only;
//     conformance-asserted only in tools builds.

template <typename T>
concept FluxBackendDevice = requires(
	bool b,
	uint32_t u,
	Flux_VRAMHandle xVRAMHandle,
	const Flux_ShaderResourceView& xView,
	const Flux_Sampler& xSampler)
{
	{ T::Initialise()                                                } -> std::same_as<void>;
	{ T::InitialiseScratchBuffers()                                  } -> std::same_as<void>;
	{ T::WaitForGPUIdle()                                            } -> std::same_as<void>;
	{ T::EndFrame(b)                                                 } -> std::same_as<void>;
	{ T::GetVRAM(xVRAMHandle)                                        } -> std::same_as<Flux_VRAM*>;
	{ T::WriteBindlessTextureSlot(u, xView, xSampler)                } -> std::same_as<void>;
};

// Tools-only device methods (ImGui integration). Pulled into a separate
// concept so the main FluxBackendDevice conformance stays callable from
// shipping builds where these methods are compiled out. The conformance file
// wraps the static_assert for this one in #ifdef ZENITH_TOOLS.
//
// CreateImGuiTextureID returns a uint64 that ImGui's ImTextureID accepts;
// the backend is responsible for whatever descriptor-set allocation is
// needed to produce that handle. The other three methods bracket an ImGui
// frame (init, begin, shutdown) and do not take engine-typed parameters.
template <typename T>
concept FluxBackendImGuiTools = requires(
	const Flux_ShaderResourceView& xView,
	const Flux_Sampler& xSampler)
{
	{ T::InitialiseImGui()                                           } -> std::same_as<void>;
	{ T::ShutdownImGui()                                             } -> std::same_as<void>;
	{ T::ImGuiBeginFrame()                                           } -> std::same_as<void>;
	{ T::CreateImGuiTextureID(xView, xSampler)                       } -> std::same_as<uint64_t>;
};
