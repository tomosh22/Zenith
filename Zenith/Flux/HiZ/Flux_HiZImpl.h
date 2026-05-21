#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 9: HiZ subsystem state + behaviour on one class. Was previously
// split between Flux_HiZ (static facade methods) and Flux_HiZImpl (data
// members); methods now live here as non-static members so the
// state-method split is gone.
class Flux_HiZImpl
{
public:
	Flux_HiZImpl() = default;
	~Flux_HiZImpl() = default;

	Flux_HiZImpl(const Flux_HiZImpl&) = delete;
	Flux_HiZImpl& operator=(const Flux_HiZImpl&) = delete;

	// Supports up to 4096x4096 (12 mip levels).
	static constexpr u_int uHIZ_MAX_MIPS = 12;

	void Initialise();
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
};
