#pragma once
#include "Zenith.h"            // u_int / u_int64 / Zenith_Assert (defined before Flux.h in the PCH)
#include "Flux/Flux_Types.h"   // handles, enums, view structs, Flux_BindingSlot/SubresourceRange
#include "Flux/Flux_Fwd.h"     // the Flux_* aliases + forward decls of the other D3D12 classes

// ============================================================================
// NO-OP "null" D3D12 render backend — device + sampler + VRAM.
//
// This header mirrors the NEUTRAL public surface of Zenith_Vulkan /
// Zenith_Vulkan_Sampler / Zenith_Vulkan_VRAM. Its ONLY job is to COMPILE and
// LINK against the backend-neutral Flux surface (the FluxBackendDevice /
// FluxBackendImGuiTools concepts), proving the Flux concepts are
// backend-agnostic. It does ZERO real rendering — every method is an inline
// no-op stub. No Vulkan / D3D12 native headers are referenced.
// ============================================================================

// Forward decls of the cross-subsystem deps the Vulkan device stores as raw
// member pointers (wired in Initialise by the composition root). We only need
// the names for symmetry; the null backend never dereferences them.
class Flux_RendererImpl;
class Zenith_TaskSystem;
class Zenith_D3D12_Swapchain;
class Zenith_D3D12_MemoryManager;

// ----------------------------------------------------------------------------
// Zenith_D3D12_Sampler — mirrors Zenith_Vulkan_Sampler (alias Flux_Sampler).
// Alias-only (no concept) — just the neutral public methods.
// ----------------------------------------------------------------------------
class Zenith_D3D12_Sampler
{
public:
	Zenith_D3D12_Sampler() = default;

	static void InitialiseRepeat(Zenith_D3D12_Sampler& /*xSampler*/) { }
	static void InitialiseClamp(Zenith_D3D12_Sampler& /*xSampler*/) { }
};

// ----------------------------------------------------------------------------
// Zenith_D3D12_VRAM — mirrors Zenith_Vulkan_VRAM (alias Flux_VRAM).
// Alias-only (no concept). The Vulkan ctors take vk::Image / vk::Buffer /
// VmaAllocation* — all BACKEND-INTERNAL types, so per RULE 1 they are SKIPPED.
// Only the default ctor (RULE 5) + the neutral accessors are mirrored.
// GetAllocation()/GetAllocator()/GetImage()/GetBuffer() return vk:: / Vma:: in
// the Vulkan header → SKIPPED. GetBufferSize()/IsAliased()/IsPool() are neutral.
// ----------------------------------------------------------------------------
class Zenith_D3D12_VRAM
{
public:
	Zenith_D3D12_VRAM() = default;
	~Zenith_D3D12_VRAM() = default;

	u_int GetBufferSize() const { return 0; }
	bool IsAliased() const { return false; }
	bool IsPool() const { return false; }
};

// ----------------------------------------------------------------------------
// Zenith_D3D12 — mirrors Zenith_Vulkan (the device / Flux_PlatformAPI).
// Satisfies FluxBackendDevice (+ FluxBackendImGuiTools in tools builds).
// ----------------------------------------------------------------------------
class Zenith_D3D12
{
public:
	Zenith_D3D12() = default;
	~Zenith_D3D12() = default;
	Zenith_D3D12(const Zenith_D3D12&) = delete;
	Zenith_D3D12& operator=(const Zenith_D3D12&) = delete;

	// ===== Bootstrap =====
	// The Vulkan device registers a begin-frame callback here (fence wait,
	// descriptor-pool reset, deferred-deletion drain, scratch reset). The null
	// backend has none of that state, so Initialise is a pure no-op -- the
	// engine's per-frame ring still advances via Flux_RendererImpl independently.
	void Initialise() { }
	void InitialisePerFrameResources() { }

	void EndFrame(bool /*bSubmitRenderWork*/) { }

	// Wait for GPU idle — no-op (nothing is in flight in the null backend).
	void WaitForGPUIdle() { }

	// Task-system entry point (static so it can be a Zenith_TaskArrayFunction).
	static void RecordCommandBuffersTask(void* /*pData*/, u_int /*uInvocationIndex*/, u_int /*uNumInvocations*/) { }

	const uint32_t GetQueueIndex(CommandType /*eType*/) { return 0; }

	const bool ShouldSubmitDrawCalls() { return false; }
	const bool ShouldUseDescSetCache() { return false; }
	const bool ShouldOnlyUpdateDirtyDescriptors() { return false; }
#ifdef ZENITH_DEBUG_VARIABLES
	void IncrementDescriptorSetAllocations() { }
#endif

	// Engine-typed bindless write (neutral entry point). No-op.
	void WriteBindlessTextureSlot(uint32_t /*uIndex*/, const Flux_ShaderResourceView& /*xView*/, const Zenith_D3D12_Sampler& /*xSampler*/) { }

	// ===== VRAM Registry =====
	Flux_VRAMHandle RegisterVRAM(Zenith_D3D12_VRAM* /*pxVRAM*/) { Flux_VRAMHandle x; x.SetValue(ms_uDummyHandle++); return x; }
	Zenith_D3D12_VRAM* GetVRAM(const Flux_VRAMHandle /*xHandle*/) { return nullptr; }
	void ReleaseVRAMHandle(const Flux_VRAMHandle /*xHandle*/) { }

#ifdef ZENITH_TOOLS
	// ===== ImGui integration (tools-only) — FluxBackendImGuiTools =====
	void InitialiseImGui() { }
	void InitialiseImGuiRenderPass() { }
	void ShutdownImGui() { }
	void ImGuiBeginFrame() { }

	// ImGui memory tracking.
	u_int64 GetImGuiMemoryAllocated() { return 0; }
	u_int64 GetImGuiAllocationCount() { return 0; }

	// Engine-typed wrapper that builds an ImGui-compatible texture identifier.
	// Returns a dummy non-zero id of the right type (uint64_t).
	uint64_t CreateImGuiTextureID(const Flux_ShaderResourceView& /*xView*/, const Zenith_D3D12_Sampler& /*xSampler*/) { return ms_uDummyHandle++; }
#endif

private:
	static inline u_int ms_uDummyHandle = 1;
};
