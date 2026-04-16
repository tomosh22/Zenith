#include "Zenith.h"

#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Static member definitions
u_int Flux_HiZ::s_uMipCount = 0;
bool Flux_HiZ::s_bEnabled = true;
bool Flux_HiZ::s_bInitialised = false;

// ---- Transient path (graph-owned allocation) ----
static Flux_TransientHandle s_xHiZBufferHandle;
static Flux_RenderGraph* s_pxGraph = nullptr;

// ---- Fallback path (subsystem-owned allocation) ----
static Flux_RenderAttachment s_xHiZBuffer_Owned;

// ---- Toggle: which path is active this frame ----
static bool s_bUsingTransients = false;

// HiZ format (constant -- pipeline building doesn't depend on allocation path)
static constexpr TextureFormat HIZ_FORMAT = TEXTURE_FORMAT_R32G32_SFLOAT;

// Debug variables
DEBUGVAR bool dbg_bHiZEnable = true;

// Compute shader and pipeline
static Flux_Shader g_xComputeShader;
static Flux_Pipeline g_xComputePipeline;
static Flux_RootSig g_xComputeRootSig;

// Cached binding handles
static Flux_BindingHandle s_xInputTexBinding;
static Flux_BindingHandle s_xOutputTexBinding;
static Flux_BindingHandle s_xPushConstantsBinding;

// Push constants for Hi-Z generation
struct HiZPushConstants
{
	u_int m_uOutputWidth;
	u_int m_uOutputHeight;
	u_int m_uInputMip;
	u_int m_uPad;
};

// ---- Helper to get the right attachment regardless of path ----

static Flux_RenderAttachment& GetHiZBuffer()
{
	if (s_bUsingTransients)
		return s_pxGraph->GetTransientAttachment(s_xHiZBufferHandle);
	return s_xHiZBuffer_Owned;
}

// ---- Fallback: subsystem-owned create/destroy ----

static void CreateOwnedRenderTargets()
{
	// Calculate mip count based on screen resolution
	u_int uWidth = Flux_Swapchain::GetWidth();
	u_int uHeight = Flux_Swapchain::GetHeight();
	Flux_HiZ::s_uMipCount = static_cast<u_int>(floor(log2(static_cast<float>(std::max(uWidth, uHeight))))) + 1;
	Flux_HiZ::s_uMipCount = std::min(Flux_HiZ::s_uMipCount, Flux_HiZ::uHIZ_MAX_MIPS);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ::CreateRenderTargets() - Resolution: %ux%u, Mip count: %u", uWidth, uHeight, Flux_HiZ::s_uMipCount);

	// Create Hi-Z buffer with full mip chain
	// RG32F format: R = min depth, G = max depth for proper hierarchical traversal
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = uWidth;
	xBuilder.m_uHeight = uHeight;
	xBuilder.m_eFormat = HIZ_FORMAT;
	xBuilder.m_uNumMips = Flux_HiZ::s_uMipCount;
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);

	xBuilder.BuildColour(s_xHiZBuffer_Owned, "HiZ Buffer (owned)");
	// BuildColour now populates all mip/slice view arrays automatically
}

static void DestroyOwnedRenderTargets()
{
	// Queue VRAM for deferred deletion
	if (s_xHiZBuffer_Owned.m_xVRAMHandle.IsValid())
	{
		Flux_VRAM* pxVRAM = Flux_PlatformAPI::GetVRAM(s_xHiZBuffer_Owned.m_xVRAMHandle);

		// Queue deletion with all image view handles
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, s_xHiZBuffer_Owned.m_xVRAMHandle,
			s_xHiZBuffer_Owned.RTV().m_xImageViewHandle,
			s_xHiZBuffer_Owned.DSV().m_xImageViewHandle,
			s_xHiZBuffer_Owned.SRV().m_xImageViewHandle,
			s_xHiZBuffer_Owned.UAV(0).m_xImageViewHandle);

		s_xHiZBuffer_Owned.m_xVRAMHandle = Flux_VRAMHandle();
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ::DestroyRenderTargets()");
}

void Flux_HiZ::Initialise()
{
	// Owned targets are always created -- used when transients are toggled off.
	CreateOwnedRenderTargets();

	static_assert(FLUX_MAX_MIPS >= uHIZ_MAX_MIPS,
		"FLUX_MAX_MIPS must be >= uHIZ_MAX_MIPS");

	// Load compute shader
	g_xComputeShader.InitialiseCompute("HiZ/Flux_HiZ_Generate.comp");

	// Build root signature from shader reflection
	const Flux_ShaderReflection& xReflection = g_xComputeShader.GetReflection();
	Flux_RootSigBuilder::FromReflection(g_xComputeRootSig, xReflection);

	// Cache binding handles
	s_xInputTexBinding = xReflection.GetBinding("g_xInputTex");
	s_xOutputTexBinding = xReflection.GetBinding("g_xOutputTex");
	s_xPushConstantsBinding = xReflection.GetBinding("pushConstants");

	// Build compute pipeline
	Flux_ComputePipelineBuilder xPipelineBuilder;
	xPipelineBuilder.WithShader(g_xComputeShader)
		.WithLayout(g_xComputeRootSig.m_xLayout)
		.Build(g_xComputePipeline);

	g_xComputePipeline.m_xRootSig = g_xComputeRootSig;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Flux", "HiZ", "Enable" }, dbg_bHiZEnable);
	Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip0" }, GetMipSRV(0));
	if (s_uMipCount > 2)
		Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip2" }, GetMipSRV(2));
	if (s_uMipCount > 4)
		Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip4" }, GetMipSRV(4));
#endif

	// Register resize callback to recreate HiZ buffer at new resolution
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ resize callback triggered");
		DestroyOwnedRenderTargets();
		CreateOwnedRenderTargets();
	});

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ initialised");
}

void Flux_HiZ::Shutdown()
{
	if (!s_bInitialised)
		return;

	DestroyOwnedRenderTargets();
	s_pxGraph = nullptr;

	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ shut down");
}

static void ExecuteHiZMip(Flux_CommandList* pxCommandList, void* pUserData)
{
	if (!Flux_HiZ::IsEnabled())
		return;

	u_int uMip = static_cast<u_int>(reinterpret_cast<uintptr_t>(pUserData));

	// -----------------------------------------------------------------------
	// Inline per-mip layout transitions.
	//
	// The HiZ buffer's initial layout is SHADER_READ_ONLY_OPTIMAL (set by the
	// memory manager for render targets with SHADER_READ + UAV flags). Each
	// mip pass needs:
	//   - Current mip (UAV target)   : UNDEFINED -> GENERAL (discard prior)
	//   - Previous mip (SRV source)  : GENERAL   -> SHADER_READ_ONLY_OPTIMAL
	//
	// #TODO: Retire these inline transitions once the render graph's compute-pass
	// barrier emission is wired to the platform layer.
	//
	// Graph-level barrier generation was removed (it was untested dead code).
	// Barriers are computed by RecordCommandBuffersTask in Zenith_Vulkan.cpp.
	// The per-mip Read/Write declarations in SetupRenderGraph() drive dependency
	// ordering; actual vkCmdPipelineBarrier calls are emitted by the backend.
	// The explicit transitions below match the compute pipeline's descriptor
	// layout expectations (SRVs want SHADER_READ_ONLY_OPTIMAL, UAVs want GENERAL).
	//
	// UNDEFINED as the source access on the target mip is safe: it's the
	// standard "discard + transition" path used across frame boundaries when
	// the previous frame's contents don't need to survive.
	// -----------------------------------------------------------------------

	// Transition current mip to GENERAL for UAV write (discard prior contents).
	pxCommandList->AddCommand<Flux_CommandImageTransition>(
		&GetHiZBuffer(),
		uMip, 1,     // mip range
		0, 1,        // layer range
		RESOURCE_ACCESS_UNDEFINED, RESOURCE_ACCESS_WRITE_UAV);

	// For mip > 0 the previous mip (just written as UAV) needs to flip to
	// SHADER_READ_ONLY_OPTIMAL so the compute shader can sample it.
	if (uMip > 0)
	{
		pxCommandList->AddCommand<Flux_CommandImageTransition>(
			&GetHiZBuffer(),
			uMip - 1, 1, // previous mip
			0, 1,
			RESOURCE_ACCESS_WRITE_UAV, RESOURCE_ACCESS_READ_SRV);
	}

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
		xBinder.BindSRV(s_xInputTexBinding, Flux_Graphics::GetDepthStencilSRV());
	}
	else
	{
		xBinder.BindSRV(s_xInputTexBinding, &Flux_HiZ::GetMipSRV(uMip - 1));
	}

	xBinder.BindUAV_Texture(s_xOutputTexBinding, &Flux_HiZ::GetMipUAV(uMip));
	xBinder.BindDrawConstants(s_xPushConstantsBinding, &xConstants, sizeof(HiZPushConstants));

	// Dispatch: ceil(width/8) x ceil(height/16) workgroups
	// Workgroup size is 8x16 for better NVIDIA occupancy (4 warps vs 2 warps)
	u_int uGroupsX = (uMipWidth + 7) / 8;
	u_int uGroupsY = (uMipHeight + 15) / 16;
	pxCommandList->AddCommand<Flux_CommandDispatch>(uGroupsX, uGroupsY, 1);

	// On the last mip, flip it back to SHADER_READ_ONLY_OPTIMAL so downstream
	// consumers (SSR, SSAO, etc.) see it in the layout their descriptors
	// expect. Earlier mips are already flipped by the next mip's transition.
	if (uMip == Flux_HiZ::GetMipCount() - 1)
	{
		pxCommandList->AddCommand<Flux_CommandImageTransition>(
			&GetHiZBuffer(),
			uMip, 1,
			0, 1,
			RESOURCE_ACCESS_WRITE_UAV, RESOURCE_ACCESS_READ_SRV);
	}
}

void Flux_HiZ::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;
	s_bUsingTransients = xGraph.AreTransientsEnabled();

	// Declare transient resource if enabled
	if (s_bUsingTransients)
	{
		u_int uWidth = Flux_Swapchain::GetWidth();
		u_int uHeight = Flux_Swapchain::GetHeight();

		Flux_TransientTextureDesc xHiZDesc;
		xHiZDesc.m_uWidth = uWidth;
		xHiZDesc.m_uHeight = uHeight;
		xHiZDesc.m_eFormat = HIZ_FORMAT;
		xHiZDesc.m_uNumMips = s_uMipCount;
		xHiZDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);

		s_xHiZBufferHandle = xGraph.CreateTransient(xHiZDesc);
	}

	// Always add all passes -- enable/disable is handled at runtime in execute callback

	static const char* s_aszHiZPassNames[] = {
		"HiZ Mip 0", "HiZ Mip 1", "HiZ Mip 2", "HiZ Mip 3",
		"HiZ Mip 4", "HiZ Mip 5", "HiZ Mip 6", "HiZ Mip 7",
		"HiZ Mip 8", "HiZ Mip 9", "HiZ Mip 10", "HiZ Mip 11"
	};

	for (u_int uMip = 0; uMip < s_uMipCount; uMip++)
	{
		Zenith_Assert(uMip < sizeof(s_aszHiZPassNames) / sizeof(s_aszHiZPassNames[0]), "HiZ mip count exceeds pass name array size");

		u_int uPassIndex = xGraph.AddPass(s_aszHiZPassNames[uMip], ExecuteHiZMip, reinterpret_cast<void*>(static_cast<uintptr_t>(uMip)));

		// Resource dependencies
		if (uMip == 0)
		{
			xGraph.Read(uPassIndex, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);
		}
		else
		{
			if (s_bUsingTransients)
				xGraph.ReadTransient(uPassIndex, s_xHiZBufferHandle, RESOURCE_ACCESS_READ_SRV);
			else
				xGraph.Read(uPassIndex, s_xHiZBuffer_Owned, RESOURCE_ACCESS_READ_SRV, uMip - 1, 1);
		}

		if (s_bUsingTransients)
			xGraph.WriteTransient(uPassIndex, s_xHiZBufferHandle, RESOURCE_ACCESS_WRITE_UAV);
		else
			xGraph.Write(uPassIndex, s_xHiZBuffer_Owned, RESOURCE_ACCESS_WRITE_UAV, uMip, 1);
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
	return dbg_bHiZEnable && s_bInitialised;
}
