#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

// Phase 9: HiZ subsystem state + behaviour on one class. Was previously
// split between Flux_HiZ (static facade methods) and Flux_HiZImpl (data
// members); methods now live here as non-static members so the
// state-method split is gone. Cross-subsystem deps (swapchain, graphics,
// renderer) are reached via g_xEngine at point of use.
//
// The HiZ chain is per-render-view: SetupRenderGraph walks
// Flux_RenderViewRegistry::ForEachActiveFullPipelineView and builds ONE chain
// per active full-pipeline view, from that view's own depth buffer at that
// view's own GetViewSetupDims. Slot 0 (main, always active) reduces the
// render-res scene depth as before; the preview slot joins the walk while its
// owner has it up; depth-only shadow cascades are never full-pipeline and never
// get a chain. Consumers (SSR/SSGI) index the accessors by view slot; the
// defaults keep single-view callers unchanged.
class Flux_HiZImpl
{
public:
	Flux_HiZImpl() = default;
	~Flux_HiZImpl() = default;

	Flux_HiZImpl(const Flux_HiZImpl&) = delete;
	Flux_HiZImpl& operator=(const Flux_HiZImpl&) = delete;

	// 12-mip chain: a full chain for max dimension up to 4095 (covers ~4K); larger clamps.
	static constexpr u_int uHIZ_MAX_MIPS = 12;

	void Initialise();
	void Shutdown();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Accessors for other systems (SSR, SSGI, etc.). These route through the
	// per-view transient automatically; uViewSlot defaults to the main camera
	// so single-view callers stay unchanged.
	Flux_RenderAttachment&            GetHiZAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_ShaderResourceView&          GetHiZSRV(u_int uViewSlot = kuFluxViewSlotMain);   // Full mip chain
	u_int                             GetMipCount(u_int uViewSlot = kuFluxViewSlotMain) const { return m_auMipCounts[uViewSlot]; }
	Flux_ShaderResourceView&          GetMipSRV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // Single mip access
	Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip, u_int uViewSlot = kuFluxViewSlotMain);  // For compute write

	bool IsEnabled() const;

	// Mip count for a view of the given base dims: log2 of the max dimension,
	// clamped to uHIZ_MAX_MIPS. A pure function of its arguments (no state), so
	// it is public: Flux_HiZ.Tests.inl pins the formula directly, and the chain
	// length a view will get is derivable by anyone holding its dims.
	// PRECONDITION: both dims > 0 — floor(log2(0)) is UB.
	static u_int ComputeMipCount(u_int uWidth, u_int uHeight);

	// Data members.
	// Per-view mip counts + base dims. Each entry is filled by SetupViewPasses
	// from that slot's GetViewSetupDims (slot 0 tracks the render resolution;
	// every other slot tracks the target dims its owner staged); ExecuteHiZMip
	// reads them via the recording pass's view slot to size its per-mip
	// dispatches. Slots that are not active full-pipeline views are never
	// written and stay zero.
	u_int                 m_auMipCounts[FLUX_MAX_RENDER_VIEWS]   = {};
	u_int                 m_auViewWidths[FLUX_MAX_RENDER_VIEWS]  = {};
	u_int                 m_auViewHeights[FLUX_MAX_RENDER_VIEWS] = {};
	bool                  m_bInitialised = false;

	// Graph-owned per-view transients + back-ref.
	Flux_TransientHandle  m_axHiZBufferHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_RenderGraph*     m_pxGraph = nullptr;

	// Compute shader + pipeline + root signature (shared by every view).
	Flux_Shader           m_xComputeShader;
	Flux_Pipeline         m_xComputePipeline;
	Flux_RootSig          m_xComputeRootSig;

private:
	// Recompute the MAIN view's mip count from the swapchain resolution. Shared
	// by Initialise + the resize callback. Stays OUTSIDE the per-view setup walk:
	// it is the pre-first-frame / resize seed for slot 0 only.
	void UpdateMipCountFromSwapchain();

	// Per-view transient + per-mip pass chain: called once per ACTIVE FULL-PIPELINE
	// view by SetupRenderGraph's ForEachActiveFullPipelineView walk, at the dims
	// Flux_GraphicsImpl::GetViewSetupDims resolves for that slot. PRIVATE — the
	// walk's callback is therefore a captureless lambda written inside
	// SetupRenderGraph (a closure declared in a member body inherits the class's
	// access), not a file-static free function.
	void SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight);

	// Attachment accessor — resolves through the graph's transient slot. Was a
	// file-static helper reaching for g_xEngine; now a member.
	Flux_RenderAttachment& GetHiZBuffer(u_int uViewSlot);
};
