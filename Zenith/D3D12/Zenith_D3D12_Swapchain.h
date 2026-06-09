#pragma once
#include "Zenith.h"            // u_int / u_int64 / Zenith_Assert (defined before Flux.h in the PCH)
#include "Flux/Flux_Types.h"   // handles, enums, view structs, Flux_BindingSlot/SubresourceRange
#include "Flux/Flux_Fwd.h"     // the Flux_* aliases + forward decls of the other D3D12 classes

// ============================================================================
// NO-OP D3D12 null backend: swapchain / presentation.
//
// Mirrors the neutral public surface of Zenith_Vulkan_Swapchain (aliased as
// Flux_Swapchain) and satisfies the FluxBackendPresentation concept. Performs
// ZERO real rendering: BeginFrame() returns true so the main loop proceeds,
// EndFrame() presents nothing, every accessor returns a benign zero / empty
// value. Exists only to prove the Flux presentation concept is backend-neutral.
//
// vk::-typed methods on the Vulkan class (GetExent, GetCurrentImageAvailable-
// Semaphore, GetFormat) are intentionally OMITTED: their signatures mention
// backend-internal Vulkan types, so the engine never calls them on the neutral
// Flux_Swapchain alias.
// ============================================================================
class Zenith_D3D12_Swapchain
{
public:
	Zenith_D3D12_Swapchain() {}
	~Zenith_D3D12_Swapchain() {}
	Zenith_D3D12_Swapchain(const Zenith_D3D12_Swapchain&) = delete;
	Zenith_D3D12_Swapchain& operator=(const Zenith_D3D12_Swapchain&) = delete;

	void Initialise() { }
	void Shutdown() { }

	// Main-loop entries (called via g_xEngine.FluxSwapchain()).
	bool BeginFrame() { return true; }
	void EndFrame() { }

	uint32_t GetWidth() { return 0; }
	uint32_t GetHeight() { return 0; }

	// Ring index in [0, MAX_FRAMES_IN_FLIGHT). Concept contract: matches
	// g_xEngine.FluxRenderer().GetRingIndex(); a no-op backend just returns 0.
	uint32_t GetCurrentFrameIndex() { return 0; }

	bool ShouldWaitOnImageAvailableSemaphore() { return false; }

	Flux_RenderAttachment* GetCurrentSwapchainTarget(uint32_t& uNumColourAttachments, Flux_RenderAttachment*& pxDepthStencil)
	{
		uNumColourAttachments = 0;
		pxDepthStencil = nullptr;
		return nullptr;
	}

private:
	static inline u_int ms_uDummyHandle = 1;
};
