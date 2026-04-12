#include "Zenith.h"

#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Static member definitions
Flux_RenderAttachment Flux_HiZ::s_xHiZBuffer;
Flux_ShaderResourceView Flux_HiZ::s_axMipSRVs[uHIZ_MAX_MIPS];
Flux_UnorderedAccessView_Texture Flux_HiZ::s_axMipUAVs[uHIZ_MAX_MIPS];
u_int Flux_HiZ::s_uMipCount = 0;
bool Flux_HiZ::s_bEnabled = true;
bool Flux_HiZ::s_bInitialised = false;

// Debug variables
DEBUGVAR bool dbg_bHiZEnable = true;

// Compute shader and pipeline
static Zenith_Vulkan_Shader g_xComputeShader;
static Zenith_Vulkan_Pipeline g_xComputePipeline;
static Zenith_Vulkan_RootSig g_xComputeRootSig;

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

static void CreateHiZRenderTargets()
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
	xBuilder.m_eFormat = TEXTURE_FORMAT_R32G32_SFLOAT;
	xBuilder.m_uNumMips = Flux_HiZ::s_uMipCount;
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);

	xBuilder.BuildColour(Flux_HiZ::s_xHiZBuffer, "HiZ Buffer");

	// Create per-mip SRVs and UAVs
	for (u_int uMip = 0; uMip < Flux_HiZ::s_uMipCount; uMip++)
	{
		Flux_HiZ::s_axMipSRVs[uMip] = Zenith_Vulkan_MemoryManager::CreateShaderResourceView(
			Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle,
			Flux_HiZ::s_xHiZBuffer.m_xSurfaceInfo,
			uMip,  // baseMip
			1      // mipCount
		);

		Flux_HiZ::s_axMipUAVs[uMip] = Zenith_Vulkan_MemoryManager::CreateUnorderedAccessView(
			Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle,
			Flux_HiZ::s_xHiZBuffer.m_xSurfaceInfo,
			uMip   // mipLevel
		);
	}
}

static void DestroyHiZRenderTargets()
{
	// Queue VRAM for deferred deletion
	if (Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle);

		// Queue deletion with all image view handles
		Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle,
			Flux_HiZ::s_xHiZBuffer.m_pxRTV.m_xImageViewHandle,
			Flux_HiZ::s_xHiZBuffer.m_pxDSV.m_xImageViewHandle,
			Flux_HiZ::s_xHiZBuffer.m_pxSRV.m_xImageViewHandle,
			Flux_HiZ::s_xHiZBuffer.m_pxUAV.m_xImageViewHandle);

		// Queue deletion for per-mip views
		for (u_int uMip = 0; uMip < Flux_HiZ::s_uMipCount; uMip++)
		{
			Zenith_Vulkan_MemoryManager::QueueImageViewDeletion(Flux_HiZ::s_axMipSRVs[uMip].m_xImageViewHandle);
			Zenith_Vulkan_MemoryManager::QueueImageViewDeletion(Flux_HiZ::s_axMipUAVs[uMip].m_xImageViewHandle);
		}

		Flux_HiZ::s_xHiZBuffer.m_xVRAMHandle = Flux_VRAMHandle();
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ::DestroyRenderTargets()");
}

void Flux_HiZ::Initialise()
{
	CreateHiZRenderTargets();

	// Load compute shader
	g_xComputeShader.InitialiseCompute("HiZ/Flux_HiZ_Generate.comp");

	// Build root signature from shader reflection
	const Flux_ShaderReflection& xReflection = g_xComputeShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(g_xComputeRootSig, xReflection);

	// Cache binding handles
	s_xInputTexBinding = xReflection.GetBinding("g_xInputTex");
	s_xOutputTexBinding = xReflection.GetBinding("g_xOutputTex");
	s_xPushConstantsBinding = xReflection.GetBinding("pushConstants");

	// Build compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xPipelineBuilder;
	xPipelineBuilder.WithShader(g_xComputeShader)
		.WithLayout(g_xComputeRootSig.m_xLayout)
		.Build(g_xComputePipeline);

	g_xComputePipeline.m_xRootSig = g_xComputeRootSig;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Flux", "HiZ", "Enable" }, dbg_bHiZEnable);
	Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip0" }, s_axMipSRVs[0]);
	if (s_uMipCount > 2)
		Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip2" }, s_axMipSRVs[2]);
	if (s_uMipCount > 4)
		Zenith_DebugVariables::AddTexture({ "Flux", "HiZ", "Textures", "Mip4" }, s_axMipSRVs[4]);
#endif

	// Register resize callback to recreate HiZ buffer at new resolution
	Flux::AddResChangeCallback([]()
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ resize callback triggered");
		DestroyHiZRenderTargets();
		CreateHiZRenderTargets();
	});

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_HiZ initialised");
}

void Flux_HiZ::Shutdown()
{
	if (!s_bInitialised)
		return;

	DestroyHiZRenderTargets();

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
	// The render graph tracks access state per-attachment (not per-subresource),
	// so it cannot emit these transitions correctly on its own. We own the
	// ordering here, and it matches the compute pipeline's descriptor layout
	// expectations (SRVs want SHADER_READ_ONLY_OPTIMAL, UAVs want GENERAL).
	//
	// UNDEFINED as the source access on the target mip is safe: it's the
	// standard "discard + transition" path used across frame boundaries when
	// the previous frame's contents don't need to survive.
	// -----------------------------------------------------------------------

	// Transition current mip to GENERAL for UAV write (discard prior contents).
	pxCommandList->AddCommand<Flux_CommandImageTransition>(
		&Flux_HiZ::s_xHiZBuffer,
		uMip, 1,     // mip range
		0, 1,        // layer range
		RESOURCE_ACCESS_UNDEFINED, RESOURCE_ACCESS_WRITE_UAV);

	// For mip > 0 the previous mip (just written as UAV) needs to flip to
	// SHADER_READ_ONLY_OPTIMAL so the compute shader can sample it.
	if (uMip > 0)
	{
		pxCommandList->AddCommand<Flux_CommandImageTransition>(
			&Flux_HiZ::s_xHiZBuffer,
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
		xBinder.BindSRV(s_xInputTexBinding, &Flux_HiZ::s_axMipSRVs[uMip - 1]);
	}

	xBinder.BindUAV_Texture(s_xOutputTexBinding, &Flux_HiZ::s_axMipUAVs[uMip]);
	xBinder.PushConstant(s_xPushConstantsBinding, &xConstants, sizeof(HiZPushConstants));

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
			&Flux_HiZ::s_xHiZBuffer,
			uMip, 1,
			0, 1,
			RESOURCE_ACCESS_WRITE_UAV, RESOURCE_ACCESS_READ_SRV);
	}
}

void Flux_HiZ::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Always add all passes — enable/disable is handled at runtime in execute callback

	static const char* s_aszHiZPassNames[] = {
		"HiZ Mip 0", "HiZ Mip 1", "HiZ Mip 2", "HiZ Mip 3",
		"HiZ Mip 4", "HiZ Mip 5", "HiZ Mip 6", "HiZ Mip 7",
		"HiZ Mip 8", "HiZ Mip 9", "HiZ Mip 10", "HiZ Mip 11"
	};

	for (u_int uMip = 0; uMip < s_uMipCount; uMip++)
	{
		Zenith_Assert(uMip < sizeof(s_aszHiZPassNames) / sizeof(s_aszHiZPassNames[0]), "HiZ mip count exceeds pass name array size");

		u_int uPassIndex = xGraph.AddPass(s_aszHiZPassNames[uMip], ExecuteHiZMip, reinterpret_cast<void*>(static_cast<uintptr_t>(uMip)));
		xGraph.SetTargetSetup(uPassIndex, Flux_Graphics::s_xNullTargetSetup);

		// Resource dependencies
		if (uMip == 0)
		{
			xGraph.Read(uPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);
		}
		else
		{
			xGraph.Read(uPassIndex, s_xHiZBuffer, RESOURCE_ACCESS_READ_SRV, uMip - 1, 1);
		}

		xGraph.Write(uPassIndex, s_xHiZBuffer, RESOURCE_ACCESS_WRITE_UAV, uMip, 1);
	}
}

Flux_ShaderResourceView& Flux_HiZ::GetHiZSRV()
{
	return s_xHiZBuffer.m_pxSRV;
}

u_int Flux_HiZ::GetMipCount()
{
	return s_uMipCount;
}

Flux_ShaderResourceView& Flux_HiZ::GetMipSRV(u_int uMip)
{
	Zenith_Assert(uMip < s_uMipCount, "Mip level out of range");
	return s_axMipSRVs[uMip];
}

Flux_UnorderedAccessView_Texture& Flux_HiZ::GetMipUAV(u_int uMip)
{
	Zenith_Assert(uMip < s_uMipCount, "Mip level out of range");
	return s_axMipUAVs[uMip];
}

bool Flux_HiZ::IsEnabled()
{
	return dbg_bHiZEnable && s_bInitialised;
}
