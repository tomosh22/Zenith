#include "Zenith.h"

#include "Flux/Fog/Flux_FroxelFog.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Command lists for the three passes
static Flux_CommandList g_xInjectCommandList("FroxelFog_Inject");
static Flux_CommandList g_xLightCommandList("FroxelFog_Light");
static Flux_CommandList g_xApplyCommandList("FroxelFog_Apply");

// Compute pipelines
static Zenith_Vulkan_Shader s_xInjectShader;
static Zenith_Vulkan_Pipeline s_xInjectPipeline;
static Zenith_Vulkan_RootSig s_xInjectRootSig;
static Zenith_Vulkan_Shader s_xLightShader;
static Zenith_Vulkan_Pipeline s_xLightPipeline;
static Zenith_Vulkan_RootSig s_xLightRootSig;

// Apply fragment pipeline
static Flux_Shader s_xApplyShader;
static Flux_Pipeline s_xApplyPipeline;

// 3D render targets for froxel grids
static Flux_RenderAttachment s_xDensityGrid;      // RGBA16F: density, scattering, absorption
static Flux_RenderAttachment s_xLightingGrid;     // RGBA16F: accumulated in-scatter
static Flux_RenderAttachment s_xScatteringGrid;   // RGBA16F: per-step scatter + extinction

// Froxel grid configuration
static constexpr u_int FROXEL_WIDTH = 160;
static constexpr u_int FROXEL_HEIGHT = 90;
static constexpr u_int FROXEL_DEPTH = 64;

// Debug variables
DEBUGVAR u_int dbg_uFroxelDebugSlice = 32;
DEBUGVAR float dbg_fFroxelNearZ = 0.5f;
DEBUGVAR float dbg_fFroxelFarZ = 500.0f;
DEBUGVAR float dbg_fFroxelPhaseG = 0.6f;
DEBUGVAR float dbg_fFroxelNoiseScale = 0.02f;
DEBUGVAR float dbg_fFroxelNoiseSpeed = 0.5f;
DEBUGVAR float dbg_fFroxelHeightBase = 0.0f;
DEBUGVAR float dbg_fFroxelHeightFalloff = 0.01f;

// Push constant structures (must match shader)
struct InjectConstants
{
	Zenith_Maths::Vector4 m_xFogParams;       // x = density, y = scattering, z = absorption, w = time
	Zenith_Maths::Vector4 m_xNoiseParams;     // x = scale, y = speed, z = detail, w = unused
	Zenith_Maths::Vector4 m_xHeightParams;    // x = base height, y = falloff, z = min height, w = max height
	Zenith_Maths::Vector4 m_xGridDimensions;  // x = width, y = height, z = depth, w = unused
	float m_fNearZ;
	float m_fFarZ;
	u_int m_uFrameIndex;
	float m_fPad0;
};

struct LightConstants
{
	Zenith_Maths::Vector4 m_xFogColour;
	Zenith_Maths::Vector4 m_xLightDirection;
	Zenith_Maths::Vector4 m_xLightColour;      // RGB = color, A = intensity
	Zenith_Maths::Vector4 m_xGridDimensions;
	float m_fScatteringCoeff;
	float m_fAbsorptionCoeff;
	float m_fPhaseG;
	u_int m_uDebugMode;
};

struct ApplyConstants
{
	Zenith_Maths::Vector4 m_xGridDimensions;
	float m_fNearZ;
	float m_fFarZ;
	u_int m_uDebugMode;
	u_int m_uDebugSliceIndex;
};

static InjectConstants s_xInjectConstants;
static LightConstants s_xLightConstants;
static ApplyConstants s_xApplyConstants;

// Cached binding handles from shader reflection
// Inject pass
static Flux_BindingHandle s_xInjectFrameConstantsBinding;
static Flux_BindingHandle s_xInjectNoiseBinding;
static Flux_BindingHandle s_xInjectDensityOutputBinding;
// Light pass
static Flux_BindingHandle s_xLightFrameConstantsBinding;
static Flux_BindingHandle s_xLightDensityInputBinding;
static Flux_BindingHandle s_xLightLightingOutputBinding;
static Flux_BindingHandle s_xLightScatteringOutputBinding;
// Apply pass
static Flux_BindingHandle s_xApplyFrameConstantsBinding;
static Flux_BindingHandle s_xApplyDepthBinding;
static Flux_BindingHandle s_xApplyLightingBinding;
static Flux_BindingHandle s_xApplyScatteringBinding;

void Flux_FroxelFog::Initialise()
{
	// Create 3D render targets for froxel grids
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = FROXEL_WIDTH;
	xBuilder.m_uHeight = FROXEL_HEIGHT;
	xBuilder.m_uDepth = FROXEL_DEPTH;
	xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xBuilder.m_eTextureType = TEXTURE_TYPE_3D;
	xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);

	xBuilder.BuildColour(s_xDensityGrid, "FroxelDensityGrid");
	xBuilder.BuildColour(s_xLightingGrid, "FroxelLightingGrid");
	xBuilder.BuildColour(s_xScatteringGrid, "FroxelScatteringGrid");

	// Initialize inject compute shader
	s_xInjectShader.InitialiseCompute("Fog/Flux_FroxelFog_Inject.comp");

	// Build inject root signature from shader reflection
	const Flux_ShaderReflection& xInjectReflection = s_xInjectShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(s_xInjectRootSig, xInjectReflection);

	// Build inject compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xInjectBuilder;
	xInjectBuilder.WithShader(s_xInjectShader)
		.WithLayout(s_xInjectRootSig.m_xLayout)
		.Build(s_xInjectPipeline);
	s_xInjectPipeline.m_xRootSig = s_xInjectRootSig;

	// Initialize light compute shader
	s_xLightShader.InitialiseCompute("Fog/Flux_FroxelFog_Light.comp");

	// Build light root signature from shader reflection
	const Flux_ShaderReflection& xLightReflection = s_xLightShader.GetReflection();
	Zenith_Vulkan_RootSigBuilder::FromReflection(s_xLightRootSig, xLightReflection);

	// Build light compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xLightBuilder;
	xLightBuilder.WithShader(s_xLightShader)
		.WithLayout(s_xLightRootSig.m_xLayout)
		.Build(s_xLightPipeline);
	s_xLightPipeline.m_xRootSig = s_xLightRootSig;

	// Cache inject binding handles (xInjectReflection already declared above)
	s_xInjectFrameConstantsBinding = xInjectReflection.GetBinding("FrameConstants");
	s_xInjectNoiseBinding = xInjectReflection.GetBinding("u_xNoiseTexture3D");
	s_xInjectDensityOutputBinding = xInjectReflection.GetBinding("u_xDensityGrid");

	// Cache light binding handles (xLightReflection already declared above)
	s_xLightFrameConstantsBinding = xLightReflection.GetBinding("FrameConstants");
	s_xLightDensityInputBinding = xLightReflection.GetBinding("u_xDensityGrid");
	s_xLightLightingOutputBinding = xLightReflection.GetBinding("u_xLightingGrid");
	s_xLightScatteringOutputBinding = xLightReflection.GetBinding("u_xScatteringGrid");

	// Initialize apply fragment shader
	s_xApplyShader.Initialise("Flux_Fullscreen_UV.vert", "Fog/Flux_FroxelFog_Apply.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xApplySpec;
	xApplySpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xApplySpec.m_pxShader = &s_xApplyShader;
	xApplySpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xApplySpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Frame constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;   // Scratch buffer for push constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Depth texture
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Lighting grid
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;  // Scattering grid

	xApplySpec.m_bDepthTestEnabled = false;
	xApplySpec.m_bDepthWriteEnabled = false;

	// Blend: fog over scene (src alpha, 1-src alpha)
	xApplySpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xApplySpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xApplySpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(s_xApplyPipeline, xApplySpec);

	// Cache apply binding handles
	const Flux_ShaderReflection& xApplyReflection = s_xApplyShader.GetReflection();
	s_xApplyFrameConstantsBinding = xApplyReflection.GetBinding("FrameConstants");
	s_xApplyDepthBinding = xApplyReflection.GetBinding("u_xDepthTexture");
	s_xApplyLightingBinding = xApplyReflection.GetBinding("u_xLightingGrid");
	s_xApplyScatteringBinding = xApplyReflection.GetBinding("u_xScatteringGrid");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Froxel", "Debug Slice Index" }, dbg_uFroxelDebugSlice, 0, 63);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Near Z" }, dbg_fFroxelNearZ, 0.1f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Far Z" }, dbg_fFroxelFarZ, 50.0f, 1000.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Phase G" }, dbg_fFroxelPhaseG, -1.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Scale" }, dbg_fFroxelNoiseScale, 0.001f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Speed" }, dbg_fFroxelNoiseSpeed, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Base" }, dbg_fFroxelHeightBase, -100.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Falloff" }, dbg_fFroxelHeightFalloff, 0.001f, 0.1f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FroxelFog initialised (%ux%ux%u grid)", FROXEL_WIDTH, FROXEL_HEIGHT, FROXEL_DEPTH);
}

void Flux_FroxelFog::Reset()
{
	g_xInjectCommandList.Reset(true);
	g_xLightCommandList.Reset(true);
	g_xApplyCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FroxelFog::Reset()");
}

Flux_RenderAttachment& Flux_FroxelFog::GetDensityGrid()
{
	return s_xDensityGrid;
}

Flux_RenderAttachment& Flux_FroxelFog::GetLightingGrid()
{
	return s_xLightingGrid;
}

Flux_RenderAttachment& Flux_FroxelFog::GetDebugSliceTexture()
{
	return s_xScatteringGrid;
}

float Flux_FroxelFog::GetNearZ()
{
	return dbg_fFroxelNearZ;
}

float Flux_FroxelFog::GetFarZ()
{
	return dbg_fFroxelFarZ;
}

void Flux_FroxelFog::Render(void*)
{
	// Get debug mode
	extern u_int dbg_uVolFogDebugMode;

	// Get shared fog constants
	const Flux_VolumeFogConstants& xShared = Flux_VolumeFog::GetSharedConstants();
	float fTime = static_cast<float>(Flux::GetFrameCounter()) * 0.016f;

	// ========== INJECT PASS ==========
	s_xInjectConstants.m_xFogParams = Zenith_Maths::Vector4(
		xShared.m_fDensity,
		xShared.m_fScatteringCoeff,
		xShared.m_fAbsorptionCoeff,
		fTime
	);
	s_xInjectConstants.m_xNoiseParams = Zenith_Maths::Vector4(
		dbg_fFroxelNoiseScale,
		dbg_fFroxelNoiseSpeed,
		1.0f,
		0.0f
	);
	s_xInjectConstants.m_xHeightParams = Zenith_Maths::Vector4(
		dbg_fFroxelHeightBase,
		dbg_fFroxelHeightFalloff,
		-1000.0f,  // min height
		1000.0f    // max height
	);
	s_xInjectConstants.m_xGridDimensions = Zenith_Maths::Vector4(
		static_cast<float>(FROXEL_WIDTH),
		static_cast<float>(FROXEL_HEIGHT),
		static_cast<float>(FROXEL_DEPTH),
		0.0f
	);
	s_xInjectConstants.m_fNearZ = dbg_fFroxelNearZ;
	s_xInjectConstants.m_fFarZ = dbg_fFroxelFarZ;
	s_xInjectConstants.m_uFrameIndex = Flux::GetFrameCounter();

	g_xInjectCommandList.Reset(false);
	g_xInjectCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xInjectPipeline);

	Flux_ShaderBinder xInjectBinder(g_xInjectCommandList);
	xInjectBinder.BindCBV(s_xInjectFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xInjectBinder.BindSRV(s_xInjectNoiseBinding, &Flux_VolumeFog::GetNoiseTexture3D()->m_xSRV, &Flux_Graphics::s_xRepeatSampler);
	xInjectBinder.BindUAV_Texture(s_xInjectDensityOutputBinding, &s_xDensityGrid.m_pxUAV);
	xInjectBinder.PushConstant(&s_xInjectConstants, sizeof(InjectConstants));
	g_xInjectCommandList.AddCommand<Flux_CommandDispatch>(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);

	Flux::SubmitCommandList(&g_xInjectCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_VOLUMEFOG_INJECT);

	// ========== LIGHT PASS ==========
	s_xLightConstants.m_xFogColour = xShared.m_xFogColour;
	s_xLightConstants.m_xLightDirection = Zenith_Maths::Vector4(
		Flux_Graphics::s_xFrameConstants.m_xSunDir_Pad.x,
		Flux_Graphics::s_xFrameConstants.m_xSunDir_Pad.y,
		Flux_Graphics::s_xFrameConstants.m_xSunDir_Pad.z,
		0.0f
	);
	s_xLightConstants.m_xLightColour = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	s_xLightConstants.m_xGridDimensions = s_xInjectConstants.m_xGridDimensions;
	s_xLightConstants.m_fScatteringCoeff = xShared.m_fScatteringCoeff;
	s_xLightConstants.m_fAbsorptionCoeff = xShared.m_fAbsorptionCoeff;
	s_xLightConstants.m_fPhaseG = dbg_fFroxelPhaseG;
	s_xLightConstants.m_uDebugMode = dbg_uVolFogDebugMode;

	g_xLightCommandList.Reset(false);
	g_xLightCommandList.AddCommand<Flux_CommandBindComputePipeline>(&s_xLightPipeline);

	Flux_ShaderBinder xLightBinder(g_xLightCommandList);
	xLightBinder.BindCBV(s_xLightFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xLightBinder.BindSRV(s_xLightDensityInputBinding, &s_xDensityGrid.m_pxSRV);
	xLightBinder.BindUAV_Texture(s_xLightLightingOutputBinding, &s_xLightingGrid.m_pxUAV);
	xLightBinder.BindUAV_Texture(s_xLightScatteringOutputBinding, &s_xScatteringGrid.m_pxUAV);
	xLightBinder.PushConstant(&s_xLightConstants, sizeof(LightConstants));
	g_xLightCommandList.AddCommand<Flux_CommandDispatch>(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);

	Flux::SubmitCommandList(&g_xLightCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_VOLUMEFOG_LIGHT);

	// ========== APPLY PASS ==========
	s_xApplyConstants.m_xGridDimensions = s_xInjectConstants.m_xGridDimensions;
	s_xApplyConstants.m_fNearZ = dbg_fFroxelNearZ;
	s_xApplyConstants.m_fFarZ = dbg_fFroxelFarZ;
	s_xApplyConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xApplyConstants.m_uDebugSliceIndex = dbg_uFroxelDebugSlice;

	g_xApplyCommandList.Reset(false);
	g_xApplyCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xApplyPipeline);
	g_xApplyCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xApplyCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xApplyBinder(g_xApplyCommandList);
	xApplyBinder.BindCBV(s_xApplyFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
	xApplyBinder.BindSRV(s_xApplyDepthBinding, Flux_Graphics::GetDepthStencilSRV());
	xApplyBinder.BindSRV(s_xApplyLightingBinding, &s_xLightingGrid.m_pxSRV);
	xApplyBinder.BindSRV(s_xApplyScatteringBinding, &s_xScatteringGrid.m_pxSRV);
	xApplyBinder.PushConstant(&s_xApplyConstants, sizeof(ApplyConstants));
	g_xApplyCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xApplyCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_FOG);
}
