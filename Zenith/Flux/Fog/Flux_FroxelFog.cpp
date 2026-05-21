#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Fog/Flux_VolumeFog.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Compute pipelines

// Apply fragment pipeline

// Graph-owned transient handles — backing Flux_RenderAttachments are allocated
// and destroyed by the render graph, sized from the descriptor in SetupTransients.

// Froxel grid format
static constexpr TextureFormat FROXEL_FORMAT = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Froxel grid configuration (fixed at compile time for performance)
// 160x90: 16:9 aspect ratio, approximately 1 froxel per 12x12 pixels at 1080p
// 64 depth slices: exponential distribution provides more detail near camera
// At 1080p (1920x1080): 12x12 pixel coverage per froxel (acceptable quality/perf balance)
// At 4K (3840x2160): 24x24 pixel coverage (slightly coarser, compensated by higher resolution)
// Changing requires texture recreation - not recommended at runtime
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
// Volumetric shadow parameters - now runtime-adjustable for scene tuning
// Bias: prevents self-shadowing, increase for distant/large scenes
// Cone radius: softness of shadows, increase for softer volumetric shadows
DEBUGVAR float dbg_fVolShadowBias = 0.001f;
DEBUGVAR float dbg_fVolShadowConeRadius = 0.002f;

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
	// Volumetric shadow parameters (now runtime-adjustable)
	// Shadow bias prevents self-shadowing artifacts in fog
	// Cone radius controls softness of volumetric shadows
	float m_fVolShadowBias;
	float m_fVolShadowConeRadius;
	// Ambient irradiance ratio: fraction of sky light vs direct sun (0.15-0.6 typical)
	float m_fAmbientIrradianceRatio;
	float m_fPad0;  // Padding to maintain 16-byte alignment
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

static Flux_RenderAttachment& GetDensityGridInternal()
{
	Zenith_Assert(g_xEngine.FroxelFog().m_pxGraph, "Flux_FroxelFogImpl::GetDensityGridInternal: graph pointer is null");
	return g_xEngine.FroxelFog().m_pxGraph->GetTransientAttachment(g_xEngine.FroxelFog().m_xDensityGridHandle);
}

static Flux_RenderAttachment& GetLightingGridInternal()
{
	Zenith_Assert(g_xEngine.FroxelFog().m_pxGraph, "Flux_FroxelFogImpl::GetLightingGridInternal: graph pointer is null");
	return g_xEngine.FroxelFog().m_pxGraph->GetTransientAttachment(g_xEngine.FroxelFog().m_xLightingGridHandle);
}

static Flux_RenderAttachment& GetScatteringGridInternal()
{
	Zenith_Assert(g_xEngine.FroxelFog().m_pxGraph, "Flux_FroxelFogImpl::GetScatteringGridInternal: graph pointer is null");
	return g_xEngine.FroxelFog().m_pxGraph->GetTransientAttachment(g_xEngine.FroxelFog().m_xScatteringGridHandle);
}

void Flux_FroxelFogImpl::BuildPipelines()
{
	// Initialize inject compute shader
	g_xEngine.FroxelFog().m_xInjectShader.Initialise(FluxShaderProgram::Fog_FroxelInject);

	// Build inject root signature from shader reflection
	Flux_RootSigBuilder::FromReflection(g_xEngine.FroxelFog().m_xInjectRootSig, g_xEngine.FroxelFog().m_xInjectShader.GetReflection());

	// Build inject compute pipeline
	Flux_ComputePipelineBuilder xInjectBuilder;
	xInjectBuilder.WithShader(g_xEngine.FroxelFog().m_xInjectShader)
		.WithLayout(g_xEngine.FroxelFog().m_xInjectRootSig.m_xLayout)
		.Build(g_xEngine.FroxelFog().m_xInjectPipeline);
	g_xEngine.FroxelFog().m_xInjectPipeline.m_xRootSig = g_xEngine.FroxelFog().m_xInjectRootSig;

	// Initialize light compute shader
	g_xEngine.FroxelFog().m_xLightShader.Initialise(FluxShaderProgram::Fog_FroxelLight);

	// Build light root signature from shader reflection
	Flux_RootSigBuilder::FromReflection(g_xEngine.FroxelFog().m_xLightRootSig, g_xEngine.FroxelFog().m_xLightShader.GetReflection());

	// Build light compute pipeline
	Flux_ComputePipelineBuilder xLightBuilder;
	xLightBuilder.WithShader(g_xEngine.FroxelFog().m_xLightShader)
		.WithLayout(g_xEngine.FroxelFog().m_xLightRootSig.m_xLayout)
		.Build(g_xEngine.FroxelFog().m_xLightPipeline);
	g_xEngine.FroxelFog().m_xLightPipeline.m_xRootSig = g_xEngine.FroxelFog().m_xLightRootSig;

	// Initialize apply fragment shader
	g_xEngine.FroxelFog().m_xApplyShader.Initialise(FluxShaderProgram::Fog_FroxelApply);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xApplySpec;
	xApplySpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xApplySpec.m_uNumColourAttachments = 1;
	xApplySpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xApplySpec.m_pxShader = &g_xEngine.FroxelFog().m_xApplyShader;
	xApplySpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.FroxelFog().m_xApplyShader.GetReflection().PopulateLayout(xApplySpec.m_xPipelineLayout);

	xApplySpec.m_bDepthTestEnabled = false;
	xApplySpec.m_bDepthWriteEnabled = false;

	// Blend: fog over scene (src alpha, 1-src alpha)
	xApplySpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xApplySpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xApplySpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.FroxelFog().m_xApplyPipeline, xApplySpec);
}

void Flux_FroxelFogImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddUInt32({ "Render", "Volumetric Fog", "Froxel", "Debug Slice Index" }, dbg_uFroxelDebugSlice, 0, 63);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Near Z" }, dbg_fFroxelNearZ, 0.1f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Far Z" }, dbg_fFroxelFarZ, 50.0f, 1000.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Phase G" }, dbg_fFroxelPhaseG, -1.0f, 1.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Scale" }, dbg_fFroxelNoiseScale, 0.001f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Speed" }, dbg_fFroxelNoiseSpeed, 0.0f, 2.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Base" }, dbg_fFroxelHeightBase, -100.0f, 100.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Falloff" }, dbg_fFroxelHeightFalloff, 0.001f, 0.1f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Shadow Bias" }, dbg_fVolShadowBias, 0.0001f, 0.01f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Froxel", "Shadow Cone Radius" }, dbg_fVolShadowConeRadius, 0.0001f, 0.01f);
#endif

#ifdef ZENITH_TOOLS
	// Fog_TemporalResolve listed by the wiring task is registered in
	// Flux_ShaderRegistry but never built — this subsystem is spatial-only
	// per Flux/Fog/CLAUDE.md, so only the three pipelines this Initialise
	// actually constructs are wired into hot reload.
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Fog_FroxelInject,
		FluxShaderProgram::Fog_FroxelLight,
		FluxShaderProgram::Fog_FroxelApply,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.FroxelFog().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FroxelFog initialised (%ux%ux%u grid)", FROXEL_WIDTH, FROXEL_HEIGHT, FROXEL_DEPTH);
}

void Flux_FroxelFogImpl::SetupTransients(Flux_RenderGraph& xGraph)
{
	g_xEngine.FroxelFog().m_pxGraph = &xGraph;

	Flux_TransientTextureDesc xFroxelDesc;
	xFroxelDesc.m_uWidth = FROXEL_WIDTH;
	xFroxelDesc.m_uHeight = FROXEL_HEIGHT;
	xFroxelDesc.m_uDepth = FROXEL_DEPTH;
	xFroxelDesc.m_eFormat = FROXEL_FORMAT;
	xFroxelDesc.m_eTextureType = TEXTURE_TYPE_3D;
	xFroxelDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);

	g_xEngine.FroxelFog().m_xDensityGridHandle = xGraph.CreateTransient(xFroxelDesc);
	g_xEngine.FroxelFog().m_xLightingGridHandle = xGraph.CreateTransient(xFroxelDesc);
	g_xEngine.FroxelFog().m_xScatteringGridHandle = xGraph.CreateTransient(xFroxelDesc);
}

void Flux_FroxelFogImpl::Reset()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FroxelFogImpl::Reset()");
}

Flux_RenderAttachment& Flux_FroxelFogImpl::GetDensityGrid()
{
	return GetDensityGridInternal();
}

Flux_RenderAttachment& Flux_FroxelFogImpl::GetLightingGrid()
{
	return GetLightingGridInternal();
}

Flux_RenderAttachment& Flux_FroxelFogImpl::GetScatteringGrid()
{
	return GetScatteringGridInternal();
}




Flux_RenderAttachment& Flux_FroxelFogImpl::GetDebugSliceTexture()
{
	return GetScatteringGridInternal();
}

float Flux_FroxelFogImpl::GetNearZ()
{
	return dbg_fFroxelNearZ;
}

float Flux_FroxelFogImpl::GetFarZ()
{
	return dbg_fFroxelFarZ;
}

void Flux_FroxelFogImpl::RenderInject(Flux_CommandList* pxCommandList)
{
	// Get shared fog constants
	const Flux_VolumeFogConstants& xShared = Flux_VolumeFog::GetSharedConstants();
	float fTime = static_cast<float>(Flux::GetFrameCounter()) * 0.016f;

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

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&g_xEngine.FroxelFog().m_xInjectPipeline);

	Flux_ShaderBinder xInjectBinder(*pxCommandList);
	xInjectBinder.BindCBV(g_xEngine.FroxelFog().m_xInjectShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xInjectBinder.BindSRV(g_xEngine.FroxelFog().m_xInjectShader, "u_xNoiseTexture3D", &Flux_VolumeFog::GetNoiseTexture3D()->m_xSRV, &g_xEngine.FluxGraphics().m_xRepeatSampler);
	xInjectBinder.BindUAV_Texture(g_xEngine.FroxelFog().m_xInjectShader, "u_xDensityGrid", &GetDensityGridInternal().UAV(0));
	xInjectBinder.BindDrawConstants(g_xEngine.FroxelFog().m_xInjectShader, "InjectConstants", &s_xInjectConstants, sizeof(InjectConstants));
	pxCommandList->AddCommand<Flux_CommandDispatch>(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);
}

void Flux_FroxelFogImpl::RenderLight(Flux_CommandList* pxCommandList)
{
	extern u_int dbg_uVolFogDebugMode;
	const Flux_VolumeFogConstants& xShared = Flux_VolumeFog::GetSharedConstants();

	s_xLightConstants.m_xFogColour = xShared.m_xFogColour;
	s_xLightConstants.m_xLightDirection = Zenith_Maths::Vector4(
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad.x,
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad.y,
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad.z,
		0.0f
	);
	s_xLightConstants.m_xLightColour = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	s_xLightConstants.m_xGridDimensions = s_xInjectConstants.m_xGridDimensions;
	s_xLightConstants.m_fScatteringCoeff = xShared.m_fScatteringCoeff;
	s_xLightConstants.m_fAbsorptionCoeff = xShared.m_fAbsorptionCoeff;
	s_xLightConstants.m_fPhaseG = dbg_fFroxelPhaseG;
	s_xLightConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xLightConstants.m_fVolShadowBias = dbg_fVolShadowBias;
	s_xLightConstants.m_fVolShadowConeRadius = dbg_fVolShadowConeRadius;
	s_xLightConstants.m_fAmbientIrradianceRatio = xShared.m_fAmbientIrradianceRatio;

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&g_xEngine.FroxelFog().m_xLightPipeline);

	Flux_ShaderBinder xLightBinder(*pxCommandList);
	xLightBinder.BindCBV(g_xEngine.FroxelFog().m_xLightShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xLightBinder.BindSRV(g_xEngine.FroxelFog().m_xLightShader, "u_xDensityGrid", &GetDensityGridInternal().SRV());
	xLightBinder.BindUAV_Texture(g_xEngine.FroxelFog().m_xLightShader, "u_xLightingGrid", &GetLightingGridInternal().UAV(0));
	xLightBinder.BindUAV_Texture(g_xEngine.FroxelFog().m_xLightShader, "u_xScatteringGrid", &GetScatteringGridInternal().UAV(0));

	// Bind CSM shadow maps and matrices for volumetric shadows
	static const char* const s_aszCSMNames[ZENITH_FLUX_NUM_CSMS] = { "u_xCSM0", "u_xCSM1", "u_xCSM2", "u_xCSM3" };
	static const char* const s_aszShadowMatrixNames[ZENITH_FLUX_NUM_CSMS] = { "ShadowMatrix0", "ShadowMatrix1", "ShadowMatrix2", "ShadowMatrix3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xCSMSRV = g_xEngine.Shadows().GetCSMSRV(u);
		xLightBinder.BindSRV(g_xEngine.FroxelFog().m_xLightShader, s_aszCSMNames[u], &xCSMSRV, &g_xEngine.FluxGraphics().m_xClampSampler);
		xLightBinder.BindCBV(g_xEngine.FroxelFog().m_xLightShader, s_aszShadowMatrixNames[u], &g_xEngine.Shadows().GetShadowMatrixBuffer(u).GetCBV());
	}

	xLightBinder.BindDrawConstants(g_xEngine.FroxelFog().m_xLightShader, "LightConstants", &s_xLightConstants, sizeof(LightConstants));
	pxCommandList->AddCommand<Flux_CommandDispatch>(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);
}

void Flux_FroxelFogImpl::RenderApply(Flux_CommandList* pxCommandList)
{
	extern u_int dbg_uVolFogDebugMode;

	s_xApplyConstants.m_xGridDimensions = s_xInjectConstants.m_xGridDimensions;
	s_xApplyConstants.m_fNearZ = dbg_fFroxelNearZ;
	s_xApplyConstants.m_fFarZ = dbg_fFroxelFarZ;
	s_xApplyConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	s_xApplyConstants.m_uDebugSliceIndex = dbg_uFroxelDebugSlice;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.FroxelFog().m_xApplyPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xApplyBinder(*pxCommandList);
	xApplyBinder.BindCBV(g_xEngine.FroxelFog().m_xApplyShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xApplyBinder.BindSRV(g_xEngine.FroxelFog().m_xApplyShader, "u_xDepthTexture", Flux_Graphics::GetDepthStencilSRV());
	xApplyBinder.BindSRV(g_xEngine.FroxelFog().m_xApplyShader, "u_xLightingGrid", &GetLightingGridInternal().SRV());
	xApplyBinder.BindSRV(g_xEngine.FroxelFog().m_xApplyShader, "u_xScatteringGrid", &GetScatteringGridInternal().SRV());
	xApplyBinder.BindDrawConstants(g_xEngine.FroxelFog().m_xApplyShader, "ApplyConstants", &s_xApplyConstants, sizeof(ApplyConstants));
	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}
