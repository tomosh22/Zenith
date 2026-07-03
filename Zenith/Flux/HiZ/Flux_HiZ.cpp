#include "Zenith.h"
#include "Flux/HiZ/Flux_HiZ_Shaders.h"
#include "Flux/Shaders/Generated/HiZ.h" // typed binding handles

#include "Flux/Flux_RendererImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
// GetWidth/GetHeight on the swapchain need the full type.
#include "Flux/Flux_BackendTypes.h"

// Phase 9: methods now live on Flux_HiZImpl. Callers use g_xEngine.HiZ().X().

// HiZ format (constexpr stays here).
static constexpr TextureFormat HIZ_FORMAT = TEXTURE_FORMAT_R32G32_SFLOAT;

// Push constants for Hi-Z generation
struct HiZPushConstants
{
	u_int m_uOutputWidth;
	u_int m_uOutputHeight;
	u_int m_uInputMip;
	u_int m_uPad;
};

// Attachment accessor — always resolves through the graph's transient slot.
// Now a private member: reads its own m_pxGraph / m_axHiZBufferHandles instead
// of reaching for g_xEngine.HiZ().
Flux_RenderAttachment& Flux_HiZImpl::GetHiZBuffer(u_int uViewSlot)
{
	return m_pxGraph->GetTransientAttachment(m_axHiZBufferHandles[uViewSlot]);
}

// Mip count for a view of the given base dims. Shared by the main-view resize
// path (UpdateMipCountFromSwapchain) and SetupViewPasses (transient desc +
// per-mip pass loop) so every view derives its chain length the same way.
u_int Flux_HiZImpl::ComputeMipCount(u_int uWidth, u_int uHeight)
{
	u_int uMipCount = static_cast<u_int>(floor(log2(static_cast<float>(std::max(uWidth, uHeight))))) + 1;
	return std::min(uMipCount, uHIZ_MAX_MIPS);
}

// Recompute the MAIN view's mip count from the swapchain resolution. Shared by
// Initialise (so GetMipCount is valid before the first SetupRenderGraph) and
// the resize callback; SetupViewPasses recomputes it per setup anyway, which
// keeps it consistent with the current framebuffer size.
void Flux_HiZImpl::UpdateMipCountFromSwapchain()
{
	// RENDER dims (== output when upscaling off): the HiZ chain is built from the render-res
	// scene depth, so mip 0 must be 1:1 with it. SetupViewPasses recomputes this from the same
	// GetRenderWidth/Height each setup — this only seeds the pre-first-frame / resize mip count.
	const u_int uWidth  = g_xEngine.FluxGraphics().GetRenderWidth();
	const u_int uHeight = g_xEngine.FluxGraphics().GetRenderHeight();
	m_auMipCounts[kuFluxViewSlotMain] = ComputeMipCount(uWidth, uHeight);
}

void Flux_HiZImpl::BuildPipelines()
{
	// HiZ_Generate is a compute-only program in the Slang registry.
	m_xComputeShader.Initialise(Flux_HiZShaders::xHiZ_Generate);
	Flux_RootSigBuilder::FromReflection(m_xComputeRootSig, m_xComputeShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xComputePipeline, m_xComputeShader, m_xComputeRootSig);
}

void Flux_HiZImpl::Initialise()
{
	UpdateMipCountFromSwapchain();

	static_assert(FLUX_MAX_MIPS >= uHIZ_MAX_MIPS,
		"FLUX_MAX_MIPS must be >= uHIZ_MAX_MIPS");

	BuildPipelines();

	// Resize callback: recompute the main view's mip count. The graph owns the
	// transient image and re-creates it with new dimensions on the next
	// SetupRenderGraph. Non-capturing fn-pointer trampoline: re-enters via
	// g_xEngine.HiZ() to reach the singleton instance (it cannot capture `this`).
	g_xEngine.FluxRenderer().AddResChangeCallback([]()
	{
		g_xEngine.HiZ().UpdateMipCountFromSwapchain();
	});

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ initialised");
}

void Flux_HiZImpl::Shutdown()
{
	if (!m_bInitialised)
		return;

	m_pxGraph = nullptr;
	m_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ shut down");
}

static void ExecuteHiZMip(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it
	// cannot capture, so it re-enters via g_xEngine.HiZ() to reach the
	// singleton instance.
	Flux_HiZImpl& xHiZ = g_xEngine.HiZ();
	if (!xHiZ.IsEnabled())
		return;

	const u_int uMip = Flux_UnpackUserData<u_int>(pUserData);
	// The pass's declared view slot selects this view's chain, base dims and
	// depth input (slot 0 = main/swapchain, preview = 512²).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();

	// Layout transitions are emitted by Flux_RenderGraph::SynthesizeBarriers
	// from the per-mip Read/Write declarations in SetupRenderGraph. The graph emits:
	//   - Current mip: UNDEFINED → WRITE_UAV (GENERAL) before this dispatch
	//   - Previous mip (read by mip N+1): WRITE_UAV → READ_SRV before that pass
	// No inline transitions needed here.

	pxCommandList->BindComputePipeline(&xHiZ.m_xComputePipeline);

	u_int uWidth = xHiZ.m_auViewWidths[uViewSlot];
	u_int uHeight = xHiZ.m_auViewHeights[uViewSlot];

	// Calculate output dimensions for this mip
	u_int uMipWidth = std::max(1u, uWidth >> uMip);
	u_int uMipHeight = std::max(1u, uHeight >> uMip);

	HiZPushConstants xConstants;
	xConstants.m_uOutputWidth = uMipWidth;
	xConstants.m_uOutputHeight = uMipHeight;
	// u_uInputMip == 0 tells shader to read from depth buffer (R32F)
	// u_uInputMip > 0 tells shader to read from HiZ (RG32F) and sample .rg
	xConstants.m_uInputMip = uMip;
	xConstants.m_uPad = 0;

	Flux_ShaderBinder xBinder(*pxCommandList);
	namespace HZ = Flux_Generated_HiZ::HiZ_Generate;

	// For mip 0, read from the view's depth buffer; for other mips, read from
	// the previous mip of the view's own chain
	if (uMip == 0)
	{
		xBinder.BindSRV(HZ::hg_xInputTex, g_xEngine.FluxGraphics().GetDepthStencilSRV(uViewSlot));
	}
	else
	{
		xBinder.BindSRV(HZ::hg_xInputTex, &xHiZ.GetMipSRV(uMip - 1, uViewSlot));
	}

	xBinder.BindUAV_Texture(HZ::hg_xOutputTex, &xHiZ.GetMipUAV(uMip, uViewSlot));
	xBinder.BindDrawConstants(HZ::hpushConstants, &xConstants, sizeof(HiZPushConstants));

	// Dispatch: ceil(width/8) x ceil(height/16) workgroups
	// Workgroup size is 8x16 for better NVIDIA occupancy (4 warps vs 2 warps)
	u_int uGroupsX = (uMipWidth + 7) / 8;
	u_int uGroupsY = (uMipHeight + 15) / 16;
	pxCommandList->Dispatch(uGroupsX, uGroupsY, 1);

	// The "final mip → SHADER_READ_ONLY" transition is now emitted by the
	// graph as part of the next consumer's prologue (SSR / SSAO / SSGI all
	// declare a Read on the HiZ chain, which triggers SynthesizeBarriers to
	// transition every mip from WRITE_UAV → READ_SRV). Removed from here.
}

void Flux_HiZImpl::SetupViewPasses(Flux_RenderGraph& xGraph, u_int uViewSlot, u_int uWidth, u_int uHeight)
{
	// Per-view dims + mip count — ExecuteHiZMip derives its per-mip dispatch
	// sizes from these via the recording pass's view slot. Recomputing each
	// setup keeps them consistent with the current framebuffer size.
	m_auViewWidths[uViewSlot]  = uWidth;
	m_auViewHeights[uViewSlot] = uHeight;
	m_auMipCounts[uViewSlot]   = ComputeMipCount(uWidth, uHeight);

	Flux_TransientTextureDesc xHiZDesc;
	xHiZDesc.m_uWidth       = uWidth;
	xHiZDesc.m_uHeight      = uHeight;
	xHiZDesc.m_eFormat      = HIZ_FORMAT;
	xHiZDesc.m_uNumMips     = m_auMipCounts[uViewSlot];
	xHiZDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);
	m_axHiZBufferHandles[uViewSlot] = xGraph.CreateTransient(xHiZDesc);

	// Pass names must be per-view unique + static-lifetime (duplicate names are
	// a hard assert). View 0 keeps the historical names (profiling / FindPass
	// stability); the preview gets its own " (Preview)" table.
	static const char* s_aszHiZPassNames[] = {
		"HiZ Mip 0",  "HiZ Mip 1",  "HiZ Mip 2",  "HiZ Mip 3",
		"HiZ Mip 4",  "HiZ Mip 5",  "HiZ Mip 6",  "HiZ Mip 7",
		"HiZ Mip 8",  "HiZ Mip 9",  "HiZ Mip 10", "HiZ Mip 11"
	};
	static const char* s_aszHiZPreviewPassNames[] = {
		"HiZ Mip 0 (Preview)",  "HiZ Mip 1 (Preview)",  "HiZ Mip 2 (Preview)",  "HiZ Mip 3 (Preview)",
		"HiZ Mip 4 (Preview)",  "HiZ Mip 5 (Preview)",  "HiZ Mip 6 (Preview)",  "HiZ Mip 7 (Preview)",
		"HiZ Mip 8 (Preview)",  "HiZ Mip 9 (Preview)",  "HiZ Mip 10 (Preview)", "HiZ Mip 11 (Preview)"
	};
	const char* const* pszPassNames = (uViewSlot == kuFluxViewSlotMain) ? s_aszHiZPassNames : s_aszHiZPreviewPassNames;

	for (u_int uMip = 0; uMip < m_auMipCounts[uViewSlot]; uMip++)
	{
		Zenith_Assert(uMip < sizeof(s_aszHiZPassNames) / sizeof(s_aszHiZPassNames[0]),
			"HiZ mip count exceeds pass name array size");

		// Mip 0 reads the view's depth buffer; mip N>0 reads the SINGLE prior
		// mip (subresource-explicit so the read range doesn't overlap with this
		// pass's write of mip N). UserData(uMip) carries the mip; ExecuteHiZMip
		// recovers it via Flux_UnpackUserData<u_int> and derives the view from
		// the recording pass's slot (declared via .View below).
		const Flux_PassHandle xPass = xGraph.AddPass(pszPassNames[uMip], ExecuteHiZMip)
			.UserData(uMip)
			.View(uViewSlot)
			.WritesTransient(m_axHiZBufferHandles[uViewSlot], RESOURCE_ACCESS_WRITE_UAV, uMip, 1);

		if (uMip == 0)
			xGraph.Read(xPass, g_xEngine.FluxGraphics().GetDepthAttachment(uViewSlot), RESOURCE_ACCESS_READ_SRV);
		else
			xGraph.ReadTransient(xPass, m_axHiZBufferHandles[uViewSlot], RESOURCE_ACCESS_READ_SRV, uMip - 1, 1);
	}
}

void Flux_HiZImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// Main view at swapchain dims (byte-equivalent to the historical
	// single-view path), then the preview view at its fixed 512² dims — only
	// while active, so its transients exist exactly when its passes do (the
	// graph's unused-transient validation demands this).
	SetupViewPasses(xGraph, kuFluxViewSlotMain, g_xEngine.FluxGraphics().GetRenderWidth(), g_xEngine.FluxGraphics().GetRenderHeight());
	if (g_xEngine.FluxGraphics().RenderViews().IsViewActive(kuFluxViewSlotPreview))
		SetupViewPasses(xGraph, kuFluxViewSlotPreview, kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);
}

Flux_RenderAttachment& Flux_HiZImpl::GetHiZAttachment(u_int uViewSlot)
{
	return GetHiZBuffer(uViewSlot);
}

Flux_ShaderResourceView& Flux_HiZImpl::GetHiZSRV(u_int uViewSlot)
{
	return GetHiZBuffer(uViewSlot).SRV();
}

Flux_ShaderResourceView& Flux_HiZImpl::GetMipSRV(u_int uMip, u_int uViewSlot)
{
	Zenith_Assert(uMip < m_auMipCounts[uViewSlot], "Mip level %u out of range (max %u)", uMip, m_auMipCounts[uViewSlot]);
	return GetHiZBuffer(uViewSlot).SRV(uMip);
}

Flux_UnorderedAccessView_Texture& Flux_HiZImpl::GetMipUAV(u_int uMip, u_int uViewSlot)
{
	Zenith_Assert(uMip < m_auMipCounts[uViewSlot], "Mip level %u out of range (max %u)", uMip, m_auMipCounts[uViewSlot]);
	return GetHiZBuffer(uViewSlot).UAV(uMip);
}

bool Flux_HiZImpl::IsEnabled() const
{
	return Zenith_GraphicsOptions::Get().m_bHiZEnabled && m_bInitialised;
}
