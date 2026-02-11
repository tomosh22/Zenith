#include "Zenith.h"

#include "Flux_IBL.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Command lists
static Flux_CommandList g_xBRDFLUTCommandList("IBL_BRDF_LUT");

// Static member definitions
Flux_RenderAttachment Flux_IBL::s_xBRDFLUT;
Flux_TargetSetup Flux_IBL::s_xBRDFLUTSetup;
bool Flux_IBL::s_bBRDFLUTGenerated = false;

Flux_RenderAttachment Flux_IBL::s_xIrradianceMap;
Flux_TargetSetup Flux_IBL::s_axIrradianceFaceSetup[6];

Flux_RenderAttachment Flux_IBL::s_xPrefilteredMap;
Flux_TargetSetup Flux_IBL::s_axPrefilteredFaceSetup[6];

// Per-mip-per-face RTVs and target setups for prefiltered map
// Index as [mip][face]
static Flux_RenderTargetView s_axPrefilteredMipFaceRTVs[IBLConfig::uPREFILTER_MIP_COUNT][6];
static Flux_TargetSetup s_axPrefilteredMipFaceSetup[IBLConfig::uPREFILTER_MIP_COUNT][6];

Flux_Pipeline Flux_IBL::s_xBRDFLUTPipeline;
Flux_Pipeline Flux_IBL::s_xIrradianceConvolvePipeline;
Flux_Pipeline Flux_IBL::s_xPrefilterPipeline;

Flux_Shader Flux_IBL::s_xBRDFLUTShader;
Flux_Shader Flux_IBL::s_xIrradianceConvolveShader;
Flux_Shader Flux_IBL::s_xPrefilterShader;

bool Flux_IBL::s_bEnabled = true;
float Flux_IBL::s_fIntensity = 1.0f;
bool Flux_IBL::s_bDiffuseEnabled = true;
bool Flux_IBL::s_bSpecularEnabled = true;
bool Flux_IBL::s_bSkyIBLDirty = true;
bool Flux_IBL::s_bIBLReady = false;  // Set true after BRDF LUT AND sky IBL are generated
bool Flux_IBL::s_bFirstGeneration = true;  // First generation must be non-amortized

// Frame-amortized regeneration state
IBL_RegenState Flux_IBL::s_eRegenState = IBL_REGEN_IDLE;
u_int Flux_IBL::s_uRegenFace = 0;
u_int Flux_IBL::s_uRegenMip = 0;

// Cached binding handles
static Flux_BindingHandle s_xBRDFLUTFrameConstantsBinding;
Flux_BindingHandle Flux_IBL::s_xIrradianceFrameConstantsBinding;
Flux_BindingHandle Flux_IBL::s_xPrefilterFrameConstantsBinding;
static Flux_BindingHandle s_xIrradianceSkyboxBinding;
static Flux_BindingHandle s_xPrefilterSkyboxBinding;

// Command lists for convolution - one per face since SubmitCommandList stores pointers
static Flux_CommandList g_axIrradianceCommandLists[6] = {
	Flux_CommandList("IBL_Irradiance_0"),
	Flux_CommandList("IBL_Irradiance_1"),
	Flux_CommandList("IBL_Irradiance_2"),
	Flux_CommandList("IBL_Irradiance_3"),
	Flux_CommandList("IBL_Irradiance_4"),
	Flux_CommandList("IBL_Irradiance_5")
};
// Command lists for prefilter: 7 mips × 6 faces = 42 command lists
// Indexed as [mip * 6 + face]
static Flux_CommandList g_axPrefilterCommandLists[IBLConfig::uPREFILTER_MIP_COUNT * 6] = {
	// Mip 0 (roughness 0.0)
	Flux_CommandList("IBL_Prefilter_M0_F0"), Flux_CommandList("IBL_Prefilter_M0_F1"),
	Flux_CommandList("IBL_Prefilter_M0_F2"), Flux_CommandList("IBL_Prefilter_M0_F3"),
	Flux_CommandList("IBL_Prefilter_M0_F4"), Flux_CommandList("IBL_Prefilter_M0_F5"),
	// Mip 1 (roughness 0.25)
	Flux_CommandList("IBL_Prefilter_M1_F0"), Flux_CommandList("IBL_Prefilter_M1_F1"),
	Flux_CommandList("IBL_Prefilter_M1_F2"), Flux_CommandList("IBL_Prefilter_M1_F3"),
	Flux_CommandList("IBL_Prefilter_M1_F4"), Flux_CommandList("IBL_Prefilter_M1_F5"),
	// Mip 2 (roughness 0.5)
	Flux_CommandList("IBL_Prefilter_M2_F0"), Flux_CommandList("IBL_Prefilter_M2_F1"),
	Flux_CommandList("IBL_Prefilter_M2_F2"), Flux_CommandList("IBL_Prefilter_M2_F3"),
	Flux_CommandList("IBL_Prefilter_M2_F4"), Flux_CommandList("IBL_Prefilter_M2_F5"),
	// Mip 3 (roughness 0.75)
	Flux_CommandList("IBL_Prefilter_M3_F0"), Flux_CommandList("IBL_Prefilter_M3_F1"),
	Flux_CommandList("IBL_Prefilter_M3_F2"), Flux_CommandList("IBL_Prefilter_M3_F3"),
	Flux_CommandList("IBL_Prefilter_M3_F4"), Flux_CommandList("IBL_Prefilter_M3_F5"),
	// Mip 4 (roughness ~0.57)
	Flux_CommandList("IBL_Prefilter_M4_F0"), Flux_CommandList("IBL_Prefilter_M4_F1"),
	Flux_CommandList("IBL_Prefilter_M4_F2"), Flux_CommandList("IBL_Prefilter_M4_F3"),
	Flux_CommandList("IBL_Prefilter_M4_F4"), Flux_CommandList("IBL_Prefilter_M4_F5"),
	// Mip 5 (roughness ~0.71)
	Flux_CommandList("IBL_Prefilter_M5_F0"), Flux_CommandList("IBL_Prefilter_M5_F1"),
	Flux_CommandList("IBL_Prefilter_M5_F2"), Flux_CommandList("IBL_Prefilter_M5_F3"),
	Flux_CommandList("IBL_Prefilter_M5_F4"), Flux_CommandList("IBL_Prefilter_M5_F5"),
	// Mip 6 (roughness 1.0)
	Flux_CommandList("IBL_Prefilter_M6_F0"), Flux_CommandList("IBL_Prefilter_M6_F1"),
	Flux_CommandList("IBL_Prefilter_M6_F2"), Flux_CommandList("IBL_Prefilter_M6_F3"),
	Flux_CommandList("IBL_Prefilter_M6_F4"), Flux_CommandList("IBL_Prefilter_M6_F5")
};

DEBUGVAR bool dbg_bIBLShowBRDFLUT = false;
DEBUGVAR bool dbg_bIBLForceRoughness = false;
DEBUGVAR float dbg_fIBLForcedRoughness = 0.5f;
DEBUGVAR bool dbg_bIBLRegenerateBRDFLUT = false;

void Flux_IBL::Initialise()
{
	CreateRenderTargets();

	// Initialize BRDF LUT shader and pipeline
	s_xBRDFLUTShader.Initialise("Flux_Fullscreen_UV.vert", "IBL/Flux_BRDFIntegration.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xBRDFSpec;
	xBRDFSpec.m_pxTargetSetup = &s_xBRDFLUTSetup;
	xBRDFSpec.m_pxShader = &s_xBRDFLUTShader;
	xBRDFSpec.m_xVertexInputDesc = xVertexDesc;
	xBRDFSpec.m_bDepthTestEnabled = false;
	xBRDFSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineLayout& xBRDFLayout = xBRDFSpec.m_xPipelineLayout;
	xBRDFLayout.m_uNumDescriptorSets = 1;
	xBRDFLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants

	Flux_PipelineBuilder::FromSpecification(s_xBRDFLUTPipeline, xBRDFSpec);

	const Flux_ShaderReflection& xBRDFReflection = s_xBRDFLUTShader.GetReflection();
	s_xBRDFLUTFrameConstantsBinding = xBRDFReflection.GetBinding("FrameConstants");

	// Initialize irradiance convolution shader and pipeline
	s_xIrradianceConvolveShader.Initialise("Flux_Fullscreen_UV.vert", "IBL/Flux_IrradianceConvolution.frag");

	Flux_PipelineSpecification xIrradianceSpec;
	xIrradianceSpec.m_pxTargetSetup = &s_axIrradianceFaceSetup[0];  // Use first face for pipeline spec
	xIrradianceSpec.m_pxShader = &s_xIrradianceConvolveShader;
	xIrradianceSpec.m_xVertexInputDesc = xVertexDesc;
	xIrradianceSpec.m_bDepthTestEnabled = false;
	xIrradianceSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineLayout& xIrradianceLayout = xIrradianceSpec.m_xPipelineLayout;
	xIrradianceLayout.m_uNumDescriptorSets = 1;
	xIrradianceLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants
	xIrradianceLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Push constants
	xIrradianceLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE; // Skybox cubemap (optional)

	Flux_PipelineBuilder::FromSpecification(s_xIrradianceConvolvePipeline, xIrradianceSpec);

	const Flux_ShaderReflection& xIrradianceReflection = s_xIrradianceConvolveShader.GetReflection();
	s_xIrradianceFrameConstantsBinding = xIrradianceReflection.GetBinding("FrameConstants");
	s_xIrradianceSkyboxBinding = xIrradianceReflection.GetBinding("g_xSkyboxCubemap");

	// Initialize prefilter shader and pipeline
	s_xPrefilterShader.Initialise("Flux_Fullscreen_UV.vert", "IBL/Flux_PrefilterEnvMap.frag");

	Flux_PipelineSpecification xPrefilterSpec;
	xPrefilterSpec.m_pxTargetSetup = &s_axPrefilteredFaceSetup[0];  // Use first face for pipeline spec
	xPrefilterSpec.m_pxShader = &s_xPrefilterShader;
	xPrefilterSpec.m_xVertexInputDesc = xVertexDesc;
	xPrefilterSpec.m_bDepthTestEnabled = false;
	xPrefilterSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineLayout& xPrefilterLayout = xPrefilterSpec.m_xPipelineLayout;
	xPrefilterLayout.m_uNumDescriptorSets = 1;
	xPrefilterLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants
	xPrefilterLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Push constants
	xPrefilterLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE; // Skybox cubemap (optional)

	Flux_PipelineBuilder::FromSpecification(s_xPrefilterPipeline, xPrefilterSpec);

	const Flux_ShaderReflection& xPrefilterReflection = s_xPrefilterShader.GetReflection();
	s_xPrefilterFrameConstantsBinding = xPrefilterReflection.GetBinding("FrameConstants");
	s_xPrefilterSkyboxBinding = xPrefilterReflection.GetBinding("g_xSkyboxCubemap");

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	// BRDF LUT will be generated on first frame via SubmitRenderTask()
	// This ensures the render loop is active when the command list is submitted

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL Initialised");
}

void Flux_IBL::Shutdown()
{
	DestroyRenderTargets();
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL shut down");
}

void Flux_IBL::Reset()
{
	g_xBRDFLUTCommandList.Reset(true);
	for (u_int i = 0; i < 6; i++)
	{
		g_axIrradianceCommandLists[i].Reset(true);
	}
	// Reset all prefilter command lists (7 mips × 6 faces)
	for (u_int i = 0; i < IBLConfig::uPREFILTER_MIP_COUNT * 6; i++)
	{
		g_axPrefilterCommandLists[i].Reset(true);
	}
	s_bSkyIBLDirty = true;
	s_bIBLReady = false;  // Need to regenerate IBL on next frame
	s_bFirstGeneration = true;  // Force non-amortized generation after reset

	// Reset amortized regeneration state
	s_eRegenState = IBL_REGEN_IDLE;
	s_uRegenFace = 0;
	s_uRegenMip = 0;
}

void Flux_IBL::SubmitRenderTask()
{

	// Check if BRDF LUT needs generation (first frame or regenerate requested)
	if (!s_bBRDFLUTGenerated || dbg_bIBLRegenerateBRDFLUT)
	{
		if (dbg_bIBLRegenerateBRDFLUT)
		{
			// Reset the flag and force regeneration
#ifdef ZENITH_DEBUG_VARIABLES
			dbg_bIBLRegenerateBRDFLUT = false;
#endif
			s_bBRDFLUTGenerated = false;
			Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Regenerating BRDF LUT (manual trigger)");
		}
		GenerateBRDFLUT();
	}

	// Update sky IBL if dirty
	if (s_bSkyIBLDirty)
	{
		UpdateSkyIBL();
	}

	// Mark IBL as ready once all textures have been generated
	if (s_bBRDFLUTGenerated && !s_bSkyIBLDirty && !s_bIBLReady)
	{
		s_bIBLReady = true;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: All IBL textures ready");
	}
}

void Flux_IBL::WaitForRenderTask()
{
	// IBL runs synchronously (command lists submitted directly)
	// No task to wait for
}

void Flux_IBL::CreateRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;

	// BRDF LUT - 2D RG16F texture (NdotV x Roughness -> scale, bias)
	xBuilder.m_uWidth = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uHeight = IBLConfig::uBRDF_LUT_SIZE;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16_SFLOAT;  // Only need RG channels for scale/bias

	xBuilder.BuildColour(s_xBRDFLUT, "IBL BRDF LUT");
	s_xBRDFLUTSetup.m_axColourAttachments[0] = s_xBRDFLUT;

	// Irradiance map - cubemap for diffuse IBL
	xBuilder.m_uWidth = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_uHeight = IBLConfig::uIRRADIANCE_SIZE;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(s_xIrradianceMap, "IBL Irradiance Map");

	// Set up per-face target setups for irradiance (using face RTVs)
	for (u_int i = 0; i < 6; i++)
	{
		// Create a temporary attachment that uses the face RTV
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo = s_xIrradianceMap.m_xSurfaceInfo;
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;  // Treat as 2D for rendering
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_uNumLayers = 1;
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_uBaseLayer = i;  // Set base layer for correct barrier transitions
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_xVRAMHandle = s_xIrradianceMap.m_xVRAMHandle;
		s_axIrradianceFaceSetup[i].m_axColourAttachments[0].m_pxRTV = s_xIrradianceMap.m_axFaceRTVs[i];
	}

	// Prefiltered environment map - cubemap for specular IBL (with mip chain for roughness levels)
	xBuilder.m_uWidth = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uHeight = IBLConfig::uPREFILTER_SIZE;
	xBuilder.m_uNumMips = IBLConfig::uPREFILTER_MIP_COUNT;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.BuildColourCubemap(s_xPrefilteredMap, "IBL Prefiltered Map");

	// Set up per-face target setups for prefiltered map (mip 0 only, for backwards compatibility)
	for (u_int i = 0; i < 6; i++)
	{
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo = s_xPrefilteredMap.m_xSurfaceInfo;
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_uNumLayers = 1;
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_xSurfaceInfo.m_uBaseLayer = i;
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_xVRAMHandle = s_xPrefilteredMap.m_xVRAMHandle;
		s_axPrefilteredFaceSetup[i].m_axColourAttachments[0].m_pxRTV = s_xPrefilteredMap.m_axFaceRTVs[i];
	}

	// Create per-mip-per-face RTVs and target setups for all roughness levels
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		u_int uMipSize = IBLConfig::uPREFILTER_SIZE >> uMip;

		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			// Create RTV for this specific mip level and face
			s_axPrefilteredMipFaceRTVs[uMip][uFace] = Flux_MemoryManager::CreateRenderTargetViewForLayer(
				s_xPrefilteredMap.m_xVRAMHandle,
				s_xPrefilteredMap.m_xSurfaceInfo,
				uFace,
				uMip);

			// Set up target setup for this mip/face combination
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo = s_xPrefilteredMap.m_xSurfaceInfo;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_uNumLayers = 1;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_uBaseLayer = uFace;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_uBaseMip = uMip;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_uWidth = uMipSize;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xSurfaceInfo.m_uHeight = uMipSize;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_xVRAMHandle = s_xPrefilteredMap.m_xVRAMHandle;
			s_axPrefilteredMipFaceSetup[uMip][uFace].m_axColourAttachments[0].m_pxRTV = s_axPrefilteredMipFaceRTVs[uMip][uFace];
		}
	}
}

void Flux_IBL::DestroyRenderTargets()
{
	auto DestroyAttachment = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.m_pxRTV.m_xImageViewHandle, xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle, xAttachment.m_pxUAV.m_xImageViewHandle);
			xAttachment.m_xVRAMHandle = Flux_VRAMHandle();
		}
	};

	// Helper to clean up cubemap face RTVs and SRVs
	auto DestroyCubemapFaceViews = [](Flux_RenderAttachment& xAttachment)
	{
		for (u_int i = 0; i < 6; i++)
		{
			if (xAttachment.m_axFaceRTVs[i].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(xAttachment.m_axFaceRTVs[i].m_xImageViewHandle);
				xAttachment.m_axFaceRTVs[i] = Flux_RenderTargetView();
			}
			if (xAttachment.m_axFaceSRVs[i].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(xAttachment.m_axFaceSRVs[i].m_xImageViewHandle);
				xAttachment.m_axFaceSRVs[i] = Flux_ShaderResourceView();
			}
		}
	};

	DestroyAttachment(s_xBRDFLUT);

	// Clean up irradiance cubemap face views before destroying VRAM
	DestroyCubemapFaceViews(s_xIrradianceMap);
	DestroyAttachment(s_xIrradianceMap);

	// Clean up per-mip RTVs before destroying the prefiltered map VRAM
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			if (s_axPrefilteredMipFaceRTVs[uMip][uFace].m_xImageViewHandle.IsValid())
			{
				Flux_MemoryManager::QueueImageViewDeletion(s_axPrefilteredMipFaceRTVs[uMip][uFace].m_xImageViewHandle);
				s_axPrefilteredMipFaceRTVs[uMip][uFace] = Flux_RenderTargetView();
			}
		}
	}

	// Clean up prefiltered cubemap base face views
	DestroyCubemapFaceViews(s_xPrefilteredMap);
	DestroyAttachment(s_xPrefilteredMap);
}

void Flux_IBL::GenerateBRDFLUT()
{
	if (s_bBRDFLUTGenerated)
	{
		return;
	}

	g_xBRDFLUTCommandList.Reset(true);  // Clear needed - first render to this target

	g_xBRDFLUTCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xBRDFLUTPipeline);
	g_xBRDFLUTCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xBRDFLUTCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	{
		Flux_ShaderBinder xBinder(g_xBRDFLUTCommandList);
		xBinder.BindCBV(s_xBRDFLUTFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	}

	g_xBRDFLUTCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	// Submit to RENDER_ORDER_PROBE_CONVOLUTION which is actually processed
	// (RENDER_ORDER_MEMORY_UPDATE is skipped in the render loop)
	Flux::SubmitCommandList(&g_xBRDFLUTCommandList, s_xBRDFLUTSetup, RENDER_ORDER_PROBE_CONVOLUTION);

	s_bBRDFLUTGenerated = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Generated BRDF LUT");
}

void Flux_IBL::UpdateSkyIBL()
{
	if (!s_bSkyIBLDirty && s_eRegenState == IBL_REGEN_IDLE)
	{
		return;
	}

	// First generation MUST be non-amortized to ensure all mip levels are in valid
	// image layouts before the deferred shader binds the prefiltered cubemap.
	// Subsequent regenerations (e.g., skybox changes) use amortization to avoid hitches.
	if (s_bFirstGeneration)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: First generation - processing all passes");
		GenerateIrradianceMap();
		GeneratePrefilteredMap();
		s_bSkyIBLDirty = false;
		s_bFirstGeneration = false;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: First generation complete");
		return;
	}

	// Start regeneration if dirty and not already in progress
	if (s_bSkyIBLDirty && s_eRegenState == IBL_REGEN_IDLE)
	{
		s_eRegenState = IBL_REGEN_IRRADIANCE;
		s_uRegenFace = 0;
		s_uRegenMip = 0;
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Starting amortized IBL regeneration");
	}

	u_int uPassesThisFrame = 0;

	// Process irradiance faces (6 total)
	while (s_eRegenState == IBL_REGEN_IRRADIANCE && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
	{
		GenerateIrradianceFace(s_uRegenFace);
		s_uRegenFace++;
		uPassesThisFrame++;

		// Finished all irradiance faces?
		if (s_uRegenFace >= 6)
		{
			s_eRegenState = IBL_REGEN_PREFILTER;
			s_uRegenFace = 0;
			s_uRegenMip = 0;
		}
	}

	// Process prefilter mips/faces (7 mips × 6 faces = 42 total)
	while (s_eRegenState == IBL_REGEN_PREFILTER && uPassesThisFrame < IBLConfig::uPASSES_PER_FRAME)
	{
		GeneratePrefilteredFace(s_uRegenMip, s_uRegenFace);
		uPassesThisFrame++;

		// Advance to next face/mip
		s_uRegenFace++;
		if (s_uRegenFace >= 6)
		{
			s_uRegenFace = 0;
			s_uRegenMip++;

			// Finished all mips?
			if (s_uRegenMip >= IBLConfig::uPREFILTER_MIP_COUNT)
			{
				s_eRegenState = IBL_REGEN_IDLE;
				s_bSkyIBLDirty = false;
				Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_IBL: Completed amortized IBL regeneration");
			}
		}
	}
}

void Flux_IBL::GenerateIrradianceMap()
{
	struct IrradianceConstants
	{
		u_int m_uUseAtmosphere;
		float m_fSunIntensity;
		u_int m_uFaceIndex;
		float m_fPad;
	};

	// Render all 6 cubemap faces - each face needs its own command list
	// because SubmitCommandList stores a pointer, not a copy
	for (u_int uFace = 0; uFace < 6; uFace++)
	{
		Flux_CommandList& xCmdList = g_axIrradianceCommandLists[uFace];
		xCmdList.Reset(true);  // Clear needed - first render to this cubemap face

		xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xIrradianceConvolvePipeline);
		xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		IrradianceConstants xConsts;
		xConsts.m_uUseAtmosphere = 1;  // Use procedural atmosphere
		xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();  // Query from atmosphere system
		xConsts.m_uFaceIndex = uFace;
		xConsts.m_fPad = 0.0f;

		{
			Flux_ShaderBinder xBinder(xCmdList);
			xBinder.BindCBV(s_xIrradianceFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
			xBinder.PushConstant(&xConsts, sizeof(xConsts));
			// Bind placeholder texture for skybox cubemap (required by Vulkan even when using atmosphere)
			if (s_xIrradianceSkyboxBinding.IsValid())
			{
				if (Flux_Graphics::s_pxCubemapTexture)
				{
					xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
				}
				else if (Flux_Graphics::s_pxBlackTexture)
				{
					xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
				}
			}
		}

		xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);

		Flux::SubmitCommandList(&xCmdList, s_axIrradianceFaceSetup[uFace], RENDER_ORDER_PROBE_CONVOLUTION);
	}
}

void Flux_IBL::GeneratePrefilteredMap()
{
	struct PrefilterConstants
	{
		float m_fRoughness;
		u_int m_uUseAtmosphere;
		float m_fSunIntensity;
		u_int m_uFaceIndex;
	};

	// Render all mip levels × 6 cubemap faces = 30 total renders
	// Each mip level corresponds to a roughness value: mip 0 = 0.0 (mirror), mip 4 = 1.0 (diffuse-like)
	for (u_int uMip = 0; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
	{
		// Calculate roughness for this mip level (0.0 to 1.0)
		float fRoughness = static_cast<float>(uMip) / static_cast<float>(IBLConfig::uPREFILTER_MIP_COUNT - 1);

		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			// Get command list for this mip-face combination
			u_int uCmdListIndex = uMip * 6 + uFace;
			Flux_CommandList& xCmdList = g_axPrefilterCommandLists[uCmdListIndex];
			xCmdList.Reset(true);  // Clear needed - first render to this mip level

			xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xPrefilterPipeline);
			xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
			xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

			PrefilterConstants xConsts;
			xConsts.m_fRoughness = fRoughness;
			xConsts.m_uUseAtmosphere = 1;
			xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();  // Query from atmosphere system
			xConsts.m_uFaceIndex = uFace;

			{
				Flux_ShaderBinder xBinder(xCmdList);
				xBinder.BindCBV(s_xPrefilterFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
				xBinder.PushConstant(&xConsts, sizeof(xConsts));
				if (s_xPrefilterSkyboxBinding.IsValid())
				{
					if (Flux_Graphics::s_pxCubemapTexture)
					{
						xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
					}
					else if (Flux_Graphics::s_pxBlackTexture)
					{
						xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
					}
				}
			}

			xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);

			// Submit to the per-mip-face target setup
			Flux::SubmitCommandList(&xCmdList, s_axPrefilteredMipFaceSetup[uMip][uFace], RENDER_ORDER_PROBE_CONVOLUTION);
		}
	}
}

void Flux_IBL::GenerateIrradianceFace(u_int uFace)
{
	struct IrradianceConstants
	{
		u_int m_uUseAtmosphere;
		float m_fSunIntensity;
		u_int m_uFaceIndex;
		float m_fPad;
	};

	Flux_CommandList& xCmdList = g_axIrradianceCommandLists[uFace];
	xCmdList.Reset(true);  // Clear needed - first render to this cubemap face

	xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xIrradianceConvolvePipeline);
	xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	IrradianceConstants xConsts;
	xConsts.m_uUseAtmosphere = 1;  // Use procedural atmosphere
	xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();
	xConsts.m_uFaceIndex = uFace;
	xConsts.m_fPad = 0.0f;

	{
		Flux_ShaderBinder xBinder(xCmdList);
		xBinder.BindCBV(s_xIrradianceFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.PushConstant(&xConsts, sizeof(xConsts));
		// Bind placeholder texture for skybox cubemap (required by Vulkan even when using atmosphere)
		if (s_xIrradianceSkyboxBinding.IsValid())
		{
			if (Flux_Graphics::s_pxCubemapTexture)
			{
				xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
			}
			else if (Flux_Graphics::s_pxBlackTexture)
			{
				xBinder.BindSRV(s_xIrradianceSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
			}
		}
	}

	xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&xCmdList, s_axIrradianceFaceSetup[uFace], RENDER_ORDER_PROBE_CONVOLUTION);
}

void Flux_IBL::GeneratePrefilteredFace(u_int uMip, u_int uFace)
{
	struct PrefilterConstants
	{
		float m_fRoughness;
		u_int m_uUseAtmosphere;
		float m_fSunIntensity;
		u_int m_uFaceIndex;
	};

	// Calculate roughness for this mip level (0.0 to 1.0)
	float fRoughness = static_cast<float>(uMip) / static_cast<float>(IBLConfig::uPREFILTER_MIP_COUNT - 1);

	// Get command list for this mip-face combination
	u_int uCmdListIndex = uMip * 6 + uFace;
	Flux_CommandList& xCmdList = g_axPrefilterCommandLists[uCmdListIndex];
	xCmdList.Reset(true);  // Clear needed - first render to this mip level

	xCmdList.AddCommand<Flux_CommandSetPipeline>(&s_xPrefilterPipeline);
	xCmdList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	xCmdList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	PrefilterConstants xConsts;
	xConsts.m_fRoughness = fRoughness;
	xConsts.m_uUseAtmosphere = 1;
	xConsts.m_fSunIntensity = Flux_Skybox::GetSunIntensity();
	xConsts.m_uFaceIndex = uFace;

	{
		Flux_ShaderBinder xBinder(xCmdList);
		xBinder.BindCBV(s_xPrefilterFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.PushConstant(&xConsts, sizeof(xConsts));
		if (s_xPrefilterSkyboxBinding.IsValid())
		{
			if (Flux_Graphics::s_pxCubemapTexture)
			{
				xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxCubemapTexture->GetSRV());
			}
			else if (Flux_Graphics::s_pxBlackTexture)
			{
				xBinder.BindSRV(s_xPrefilterSkyboxBinding, &Flux_Graphics::s_pxBlackTexture->GetSRV());
			}
		}
	}

	xCmdList.AddCommand<Flux_CommandDrawIndexed>(6);

	// Submit to the per-mip-face target setup
	Flux::SubmitCommandList(&xCmdList, s_axPrefilteredMipFaceSetup[uMip][uFace], RENDER_ORDER_PROBE_CONVOLUTION);
}

void Flux_IBL::MarkAllProbesDirty()
{
	s_bSkyIBLDirty = true;
}

// Accessors - return const references to prevent modification and signal temporary nature
const Flux_ShaderResourceView& Flux_IBL::GetBRDFLUTSRV()
{
	return s_xBRDFLUT.m_pxSRV;
}

const Flux_ShaderResourceView& Flux_IBL::GetIrradianceMapSRV()
{
	return s_xIrradianceMap.m_pxSRV;
}

const Flux_ShaderResourceView& Flux_IBL::GetPrefilteredMapSRV()
{
	return s_xPrefilteredMap.m_pxSRV;
}

// Setters
void Flux_IBL::SetEnabled(bool bEnabled)
{
	s_bEnabled = bEnabled;
}
void Flux_IBL::SetIntensity(float fIntensity)
{
	s_fIntensity = fIntensity;
}
void Flux_IBL::SetDiffuseEnabled(bool bEnabled)
{
	s_bDiffuseEnabled = bEnabled;
}
void Flux_IBL::SetSpecularEnabled(bool bEnabled)
{
	s_bSpecularEnabled = bEnabled;
}

// Getters - return actual state variables (synced from debug variables in SubmitRenderTask)
bool Flux_IBL::IsEnabled() { return s_bEnabled; }
bool Flux_IBL::IsReady() { return s_bIBLReady; }
float Flux_IBL::GetIntensity() { return s_fIntensity; }
bool Flux_IBL::IsDiffuseEnabled() { return s_bDiffuseEnabled; }
bool Flux_IBL::IsSpecularEnabled() { return s_bSpecularEnabled; }
bool Flux_IBL::IsShowBRDFLUT() { return dbg_bIBLShowBRDFLUT; }
bool Flux_IBL::IsForceRoughness() { return dbg_bIBLForceRoughness; }
float Flux_IBL::GetForcedRoughness() { return dbg_fIBLForcedRoughness; }

#ifdef ZENITH_TOOLS
void Flux_IBL::RegisterDebugVariables()
{
	// NOTE: Texture debug variables are registered here during Initialise(), before
	// content is generated. The SRVs are valid (created in CreateRenderTargets) but
	// textures will appear black/undefined until GenerateBRDFLUT() and UpdateSkyIBL()
	// run on the first frame. This is expected behavior.
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "ShowBRDFLUT" }, dbg_bIBLShowBRDFLUT);
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "ForceRoughness" }, dbg_bIBLForceRoughness);
	Zenith_DebugVariables::AddFloat({ "Flux", "IBL", "ForcedRoughness" }, dbg_fIBLForcedRoughness, 0.0f, 1.0f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "IBL", "RegenerateBRDFLUT" }, dbg_bIBLRegenerateBRDFLUT);

	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "BRDF_LUT" }, s_xBRDFLUT.m_pxSRV);

	// Register individual cubemap faces for irradiance map (face order: +X, -X, +Y, -Y, +Z, -Z)
	// Using PosX/NegX naming to avoid special characters in debug variable paths
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face0_PosX" }, s_xIrradianceMap.m_axFaceSRVs[0]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face1_NegX" }, s_xIrradianceMap.m_axFaceSRVs[1]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face2_PosY" }, s_xIrradianceMap.m_axFaceSRVs[2]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face3_NegY" }, s_xIrradianceMap.m_axFaceSRVs[3]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face4_PosZ" }, s_xIrradianceMap.m_axFaceSRVs[4]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Irradiance", "Face5_NegZ" }, s_xIrradianceMap.m_axFaceSRVs[5]);

	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face0_PosX" }, s_xPrefilteredMap.m_axFaceSRVs[0]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face1_NegX" }, s_xPrefilteredMap.m_axFaceSRVs[1]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face2_PosY" }, s_xPrefilteredMap.m_axFaceSRVs[2]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face3_NegY" }, s_xPrefilteredMap.m_axFaceSRVs[3]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face4_PosZ" }, s_xPrefilteredMap.m_axFaceSRVs[4]);
	Zenith_DebugVariables::AddTexture({ "Flux", "IBL", "Textures", "Prefiltered", "Face5_NegZ" }, s_xPrefilteredMap.m_axFaceSRVs[5]);
}
#endif
