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
struct Flux_WorkDistribution;   // RecordFrame param (reference — forward decl suffices)

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
	static void InitialisePointClamp(Zenith_D3D12_Sampler& /*xSampler*/) { }
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
	// The Vulkan device does real per-frame begin work (fence wait,
	// descriptor-pool reset, deferred-deletion drain, scratch reset). The null
	// backend has none of that state, so Initialise is a pure no-op -- the
	// engine's per-frame ring still advances via Flux_RendererImpl independently.
	void Initialise() { }
	void InitialisePerFrameResources() { }

	// Per-frame begin work, called directly each frame by
	// Flux_RendererImpl::BeginFrame via the neutral Flux_PlatformAPI alias.
	// No-op: the null backend has no per-frame GPU state to advance.
	void PerFrameBegin(u_int /*uRingIndex*/) { }

	// Record every queued render pass by running its callback into a no-op command
	// buffer, so callback side effects (buffer uploads, ECS reads, draw-list
	// builds) still occur on the null backend — exactly as they did when the
	// callbacks ran through the (now removed) Flux_CommandList stage. Out-of-line
	// (Zenith_D3D12.cpp): the body needs the full render-graph / pass / entry types
	// which can't be pulled into this seam-reachable header. EndFrame stays a no-op
	// (the null backend submits nothing).
	void RecordFrame(const Flux_WorkDistribution& xWorkDistribution);
	// Phase 5.1: no-op on the null backend — it has no persistent descriptor sets.
	void PreparePersistentSets(Flux_BufferDescriptorHandle /*xGlobalCBV*/, Flux_BufferDescriptorHandle /*xMaterialsSSBO*/, Flux_BufferDescriptorHandle /*xViewCBV*/) { }
	// Phase 5.4: no-op on the null backend (no real VIEW-set image/buffer descriptors).
	void WritePersistentViewImage(u_int /*uBinding*/, const Flux_ShaderResourceView& /*xSRV*/, const Zenith_D3D12_Sampler& /*xSampler*/) { }
	void WritePersistentViewBuffer(u_int /*uBinding*/, const Flux_ShaderResourceView_Buffer& /*xSRV*/) { }
	void EndFrame(bool /*bSubmitRenderWork*/) { }

	// Wait for GPU idle — no-op (nothing is in flight in the null backend).
	void WaitForGPUIdle() { }

	const uint32_t GetQueueIndex(CommandType /*eType*/) { return 0; }

	const bool ShouldSubmitDrawCalls() { return false; }
	const bool ShouldUseDescSetCache() { return false; }
	const bool ShouldOnlyUpdateDirtyDescriptors() { return false; }
#ifdef ZENITH_DEBUG_VARIABLES
	void IncrementDescriptorSetAllocations() { }
#endif

	// Engine-typed bindless write (neutral entry point). No-op.
	void WriteBindlessTextureSlot(uint32_t /*uIndex*/, const Flux_ShaderResourceView& /*xView*/, const Zenith_D3D12_Sampler& /*xSampler*/) { }

	// Backend-neutral surface mirrored from Zenith_Vulkan: drives the
	// Flux_BindlessAllocator capacity. The null backend writes no real descriptors,
	// so it just hands back the (un-clamped) target size — large enough that a full
	// session's MarkAsBindless allocations never exhaust it.
	uint32_t GetBindlessTableSize() const { return 16384u; }

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
