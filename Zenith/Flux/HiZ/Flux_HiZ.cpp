#include "Zenith.h"

#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Static member definitions
u_int Flux_HiZ::s_uMipCount = 0;
bool Flux_HiZ::s_bInitialised = false;

// Graph-owned transient — backing Flux_RenderAttachment is allocated and
// destroyed by the render graph, sized from the descriptor in SetupRenderGraph.
static Flux_TransientHandle s_xHiZBufferHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// HiZ format.
static constexpr TextureFormat HIZ_FORMAT = TEXTURE_FORMAT_R32G32_SFLOAT;

// Compute shader and pipeline
static Flux_Shader g_xComputeShader;
static Flux_Pipeline g_xComputePipeline;
static Flux_RootSig g_xComputeRootSig;

// Resource bindings are looked up by name on the binder via the shader's
// reflection. The binder caches the (reflection, name-literal) pointer pair
// after first use, so the per-frame cost of these calls is bounded at a few
// pointer comparisons even though the names are passed as string literals at
// every dispatch site.

// Push constants for Hi-Z generation
struct HiZPushConstants
{
	u_int m_uOutputWidth;
	u_int m_uOutputHeight;
	u_int m_uInputMip;
	u_int m_uPad;
};

// Attachment accessor — always resolves through the graph's transient slot.
static Flux_RenderAttachment& GetHiZBuffer() { return s_pxGraph->GetTransientAttachment(s_xHiZBufferHandle); }

// Compute the mip count from the current swapchain resolution. Shared by
// Initialise (for pipeline building) and SetupRenderGraph (for the transient
// desc + per-mip pass loop); reading it from the swapchain each time keeps
// it consistent with the current framebuffer size.
static void UpdateMipCountFromSwapchain()
{
	const u_int uWidth  = Flux_Swapchain::GetWidth();
	const u_int uHeight = Flux_Swapchain::GetHeight();
	Flux_HiZ::s_uMipCount = static_cast<u_int>(floor(log2(static_cast<float>(std::max(uWidth, uHeight))))) + 1;
	Flux_HiZ::s_uMipCount = std::min(Flux_HiZ::s_uMipCount, Flux_HiZ::uHIZ_MAX_MIPS);
}

void Flux_HiZ::BuildPipelines()
{
	// HiZ_Generate is a compute-only program in the Slang registry.
	g_xComputeShader.Initialise(FluxShaderProgram::HiZ_Generate);
	Flux_RootSigBuilder::FromReflection(g_xComputeRootSig, g_xComputeShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(g_xComputePipeline, g_xComputeShader, g_xComputeRootSig);
}

void Flux_HiZ::Initialise()
{
	UpdateMipCountFromSwapchain();

	static_assert(FLUX_MAX_MIPS >= uHIZ_MAX_MIPS,
		"FLUX_MAX_MIPS must be >= uHIZ_MAX_MIPS");

	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::HiZ_Generate,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_HiZ::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	// Resize callback: recompute mip count. The graph owns the transient
	// image and re-creates it with new dimensions on the next SetupRenderGraph.
	Flux::AddResChangeCallback([]()
	{
		UpdateMipCountFromSwapchain();
	});

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ initialised");
}

void Flux_HiZ::Shutdown()
{
	if (!s_bInitialised)
		return;

	s_pxGraph = nullptr;
	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ shut down");
}

static void ExecuteHiZMip(Flux_CommandList* pxCommandList, void* pUserData)
{
	if (!Flux_HiZ::IsEnabled())
		return;

	const u_int uMip = Flux_UnpackUserData<u_int>(pUserData);

	// Layout transitions are emitted by Flux_RenderGraph::SynthesizeBarriers
	// from the per-mip Read/Write declarations in SetupRenderGraph. The graph emits:
	//   - Current mip: UNDEFINED → WRITE_UAV (GENERAL) before this dispatch
	//   - Previous mip (read by mip N+1): WRITE_UAV → READ_SRV before that pass
	// No inline transitions needed here.

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&g_xComputePipeline);

	u_int uWidth = Flux_Swapchain::GetWidth();
	u_int uHeight = Flux_Swapchain::GetHeight();

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

	// For mip 0, read from depth buffer; for other mips, read from previous mip
	if (uMip == 0)
	{
		xBinder.BindSRV(g_xComputeShader, "g_xInputTex", Flux_Graphics::GetDepthStencilSRV());
	}
	else
	{
		xBinder.BindSRV(g_xComputeShader, "g_xInputTex", &Flux_HiZ::GetMipSRV(uMip - 1));
	}

	xBinder.BindUAV_Texture(g_xComputeShader, "g_xOutputTex", &Flux_HiZ::GetMipUAV(uMip));
	xBinder.BindDrawConstants(g_xComputeShader, "pushConstants", &xConstants, sizeof(HiZPushConstants));

	// Dispatch: ceil(width/8) x ceil(height/16) workgroups
	// Workgroup size is 8x16 for better NVIDIA occupancy (4 warps vs 2 warps)
	u_int uGroupsX = (uMipWidth + 7) / 8;
	u_int uGroupsY = (uMipHeight + 15) / 16;
	pxCommandList->AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);

	// The "final mip → SHADER_READ_ONLY" transition is now emitted by the
	// graph as part of the next consumer's prologue (SSR / SSAO / SSGI all
	// declare a Read on the HiZ chain, which triggers SynthesizeBarriers to
	// transition every mip from WRITE_UAV → READ_SRV). Removed from here.
}

void Flux_HiZ::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

	// Rebuild the mip count each setup — resize callback updates it ahead of
	// SetupRenderGraph but calling it again here is cheap and keeps this
	// function self-contained.
	UpdateMipCountFromSwapchain();

	Flux_TransientTextureDesc xHiZDesc;
	xHiZDesc.m_uWidth       = Flux_Swapchain::GetWidth();
	xHiZDesc.m_uHeight      = Flux_Swapchain::GetHeight();
	xHiZDesc.m_eFormat      = HIZ_FORMAT;
	xHiZDesc.m_uNumMips     = s_uMipCount;
	xHiZDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);
	s_xHiZBufferHandle = xGraph.CreateTransient(xHiZDesc);

	static const char* s_aszHiZPassNames[] = {
		"HiZ Mip 0",  "HiZ Mip 1",  "HiZ Mip 2",  "HiZ Mip 3",
		"HiZ Mip 4",  "HiZ Mip 5",  "HiZ Mip 6",  "HiZ Mip 7",
		"HiZ Mip 8",  "HiZ Mip 9",  "HiZ Mip 10", "HiZ Mip 11"
	};

	for (u_int uMip = 0; uMip < s_uMipCount; uMip++)
	{
		Zenith_Assert(uMip < sizeof(s_aszHiZPassNames) / sizeof(s_aszHiZPassNames[0]),
			"HiZ mip count exceeds pass name array size");

		// Mip 0 reads the depth buffer; mip N>0 reads the SINGLE prior mip
		// (subresource-explicit so the read range doesn't overlap with this
		// pass's write of mip N). UserData(uMip) replaces the old
		// reinterpret_cast<void*>(uintptr_t) pack; ExecuteHiZMip recovers it
		// via Flux_UnpackUserData<u_int>.
		const Flux_PassHandle xPass = xGraph.AddPass(s_aszHiZPassNames[uMip], ExecuteHiZMip)
			.UserData(uMip)
			.WritesTransient(s_xHiZBufferHandle, RESOURCE_ACCESS_WRITE_UAV, uMip, 1);

		if (uMip == 0)
			xGraph.Read(xPass, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
		else
			xGraph.ReadTransient(xPass, s_xHiZBufferHandle, RESOURCE_ACCESS_READ_SRV, uMip - 1, 1);
	}
}

Flux_RenderAttachment& Flux_HiZ::GetHiZAttachment()
{
	return GetHiZBuffer();
}

Flux_ShaderResourceView& Flux_HiZ::GetHiZSRV()
{
	return GetHiZBuffer().SRV();
}

u_int Flux_HiZ::GetMipCount()
{
	return s_uMipCount;
}

Flux_ShaderResourceView& Flux_HiZ::GetMipSRV(u_int uMip)
{
	Zenith_Assert(uMip < s_uMipCount, "Mip level %u out of range (max %u)", uMip, s_uMipCount);
	return GetHiZBuffer().SRV(uMip);
}

Flux_UnorderedAccessView_Texture& Flux_HiZ::GetMipUAV(u_int uMip)
{
	Zenith_Assert(uMip < s_uMipCount, "Mip level %u out of range (max %u)", uMip, s_uMipCount);
	return GetHiZBuffer().UAV(uMip);
}

bool Flux_HiZ::IsEnabled()
{
	return Zenith_GraphicsOptions::Get().m_bHiZEnabled && s_bInitialised;
}
