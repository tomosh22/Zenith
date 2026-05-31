#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Cross-subsystem dependencies injected into Initialise (see the DI seam note
// below). Forward-declared here; full headers are pulled in by Flux_HiZ.cpp.
class Zenith_Vulkan_Swapchain;
class Flux_GraphicsImpl;
class Flux_RendererImpl;

// Phase 9: HiZ subsystem state + behaviour on one class. Was previously
// split between Flux_HiZ (static facade methods) and Flux_HiZImpl (data
// members); methods now live here as non-static members so the
// state-method split is gone.
//
// Wave 9 DI seam (reusable template for the other ~50 subsystems): cross-
// subsystem dependencies are INJECTED through Initialise as explicit
// references and stored as member pointers, rather than reached for via
// g_xEngine.X() inside every method. The only place g_xEngine self-lookup
// survives is the non-capturing fn-pointer trampolines (res-change callback,
// hot-reload callback, the ExecuteHiZMip graph callback) — those cannot
// capture state, so they re-enter via g_xEngine.HiZ() to reach this singleton
// instance and then route their other reach-ins through the injected members.
class Flux_HiZImpl
{
public:
	Flux_HiZImpl() = default;
	~Flux_HiZImpl() = default;

	Flux_HiZImpl(const Flux_HiZImpl&) = delete;
	Flux_HiZImpl& operator=(const Flux_HiZImpl&) = delete;

	// Supports up to 4096x4096 (12 mip levels).
	static constexpr u_int uHIZ_MAX_MIPS = 12;

	// Cross-subsystem deps are injected here and stored into the member
	// pointers below. This is the Wave 9 DI template: explicit ref params ->
	// stored member pointers.
	void Initialise(Zenith_Vulkan_Swapchain& xSwapchain, Flux_GraphicsImpl& xGraphics, Flux_RendererImpl& xRenderer);
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Accessors for other systems (SSR, SSGI, etc.). These route through the
	// transient or owned attachment automatically.
	Flux_RenderAttachment&            GetHiZAttachment();
	Flux_ShaderResourceView&          GetHiZSRV();            // Full mip chain
	u_int                             GetMipCount() const { return m_uMipCount; }
	Flux_ShaderResourceView&          GetMipSRV(u_int uMip);  // Single mip access
	Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip);  // For compute write

	bool IsEnabled() const;

	// Data members.
	u_int                 m_uMipCount    = 0;
	bool                  m_bInitialised = false;

	// Graph-owned transient + back-ref.
	Flux_TransientHandle  m_xHiZBufferHandle;
	Flux_RenderGraph*     m_pxGraph = nullptr;

	// Compute shader + pipeline + root signature.
	Flux_Shader           m_xComputeShader;
	Flux_Pipeline         m_xComputePipeline;
	Flux_RootSig          m_xComputeRootSig;

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real
	// boot path wires them in Flux.cpp.
	Zenith_Vulkan_Swapchain* m_pxSwapchain = nullptr;
	Flux_GraphicsImpl*       m_pxGraphics  = nullptr;
	Flux_RendererImpl*       m_pxRenderer  = nullptr;

private:
	// Recompute mip count from the injected swapchain resolution. Was a file-
	// static helper reaching for g_xEngine; now a member so it uses
	// m_pxSwapchain. Shared by Initialise + SetupRenderGraph.
	void UpdateMipCountFromSwapchain();

	// Attachment accessor — resolves through the graph's transient slot. Was a
	// file-static helper reaching for g_xEngine; now a member.
	Flux_RenderAttachment& GetHiZBuffer();
};
