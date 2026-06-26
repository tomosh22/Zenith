#include "Zenith.h"
#include "Flux/Fog/Flux_Fog_Shaders.h"
#include "Flux/Flux_RendererImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Fog/Flux_FroxelFogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"

#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Shaders/Generated/Fog.h" // typed binding handles

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

// Push constant structures (InjectConstants / LightConstants / ApplyConstants)
// now live in Flux_FroxelFogImpl.h, and the per-frame scratch instances are
// members of Flux_FroxelFogImpl (m_xInjectConstants / m_xLightConstants /
// m_xApplyConstants), accessed via implicit this inside the Render* members.

Flux_RenderAttachment& Flux_FroxelFogImpl::GetDensityGridInternal()
{
	Zenith_Assert(m_pxGraph, "Flux_FroxelFogImpl::GetDensityGridInternal: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xDensityGridHandle);
}

Flux_RenderAttachment& Flux_FroxelFogImpl::GetLightingGridInternal()
{
	Zenith_Assert(m_pxGraph, "Flux_FroxelFogImpl::GetLightingGridInternal: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xLightingGridHandle);
}

Flux_RenderAttachment& Flux_FroxelFogImpl::GetScatteringGridInternal()
{
	Zenith_Assert(m_pxGraph, "Flux_FroxelFogImpl::GetScatteringGridInternal: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xScatteringGridHandle);
}

void Flux_FroxelFogImpl::BuildPipelines()
{
	// Initialize inject compute shader
	m_xInjectShader.Initialise(Flux_FogShaders::xFog_FroxelInject);

	// Build inject root signature from shader reflection
	Flux_RootSigBuilder::FromReflection(m_xInjectRootSig, m_xInjectShader.GetReflection());

	// Build inject compute pipeline
	Flux_ComputePipelineBuilder xInjectBuilder;
	xInjectBuilder.WithShader(m_xInjectShader)
		.WithLayout(m_xInjectRootSig.m_xLayout)
		.Build(m_xInjectPipeline);
	m_xInjectPipeline.m_xRootSig = m_xInjectRootSig;

	// Initialize light compute shader
	m_xLightShader.Initialise(Flux_FogShaders::xFog_FroxelLight);

	// Build light root signature from shader reflection
	Flux_RootSigBuilder::FromReflection(m_xLightRootSig, m_xLightShader.GetReflection());

	// Build light compute pipeline
	Flux_ComputePipelineBuilder xLightBuilder;
	xLightBuilder.WithShader(m_xLightShader)
		.WithLayout(m_xLightRootSig.m_xLayout)
		.Build(m_xLightPipeline);
	m_xLightPipeline.m_xRootSig = m_xLightRootSig;

	// Initialize apply fragment shader
	m_xApplyShader.Initialise(Flux_FogShaders::xFog_FroxelApply);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xApplySpec;
	xApplySpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xApplySpec.m_uNumColourAttachments = 1;
	xApplySpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xApplySpec.m_pxShader = &m_xApplyShader;
	xApplySpec.m_xVertexInputDesc = xVertexDesc;

	m_xApplyShader.GetReflection().PopulateLayout(xApplySpec.m_xPipelineLayout);

	xApplySpec.m_bDepthTestEnabled = false;
	xApplySpec.m_bDepthWriteEnabled = false;

	// Blend: fog over scene (src alpha, 1-src alpha)
	xApplySpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xApplySpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xApplySpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	Flux_PipelineBuilder::FromSpecification(m_xApplyPipeline, xApplySpec);
}

void Flux_FroxelFogImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Volumetric Fog", "Froxel", "Debug Slice Index" }, dbg_uFroxelDebugSlice, 0, 63);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Near Z" }, dbg_fFroxelNearZ, 0.1f, 10.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Far Z" }, dbg_fFroxelFarZ, 50.0f, 1000.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Phase G" }, dbg_fFroxelPhaseG, -1.0f, 1.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Scale" }, dbg_fFroxelNoiseScale, 0.001f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Noise Speed" }, dbg_fFroxelNoiseSpeed, 0.0f, 2.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Base" }, dbg_fFroxelHeightBase, -100.0f, 100.0f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Height Falloff" }, dbg_fFroxelHeightFalloff, 0.001f, 0.1f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Shadow Bias" }, dbg_fVolShadowBias, 0.0001f, 0.01f);
	g_xEngine.DebugVariables().AddFloat({ "Render", "Volumetric Fog", "Froxel", "Shadow Cone Radius" }, dbg_fVolShadowConeRadius, 0.0001f, 0.01f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_FroxelFog initialised (%ux%ux%u grid)", FROXEL_WIDTH, FROXEL_HEIGHT, FROXEL_DEPTH);
}

void Flux_FroxelFogImpl::SetupTransients(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	Flux_TransientTextureDesc xFroxelDesc;
	xFroxelDesc.m_uWidth = FROXEL_WIDTH;
	xFroxelDesc.m_uHeight = FROXEL_HEIGHT;
	xFroxelDesc.m_uDepth = FROXEL_DEPTH;
	xFroxelDesc.m_eFormat = FROXEL_FORMAT;
	xFroxelDesc.m_eTextureType = TEXTURE_TYPE_3D;
	xFroxelDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ) | (1u << MEMORY_FLAGS__UNORDERED_ACCESS);

	m_xDensityGridHandle = xGraph.CreateTransient(xFroxelDesc);
	m_xLightingGridHandle = xGraph.CreateTransient(xFroxelDesc);
	m_xScatteringGridHandle = xGraph.CreateTransient(xFroxelDesc);
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

void Flux_FroxelFogImpl::RenderInject(Flux_CommandBuffer* pxCommandList)
{
	// Get shared fog constants
	const Flux_VolumeFogConstants& xShared = g_xEngine.VolumeFog().GetSharedConstants();
	float fTime = static_cast<float>(g_xEngine.Frame().GetFrameIndex()) * 0.016f;

	m_xInjectConstants.m_xFogParams = Zenith_Maths::Vector4(
		xShared.m_fDensity,
		xShared.m_fScatteringCoeff,
		xShared.m_fAbsorptionCoeff,
		fTime
	);
	m_xInjectConstants.m_xNoiseParams = Zenith_Maths::Vector4(
		dbg_fFroxelNoiseScale,
		dbg_fFroxelNoiseSpeed,
		1.0f,
		0.0f
	);
	m_xInjectConstants.m_xHeightParams = Zenith_Maths::Vector4(
		dbg_fFroxelHeightBase,
		dbg_fFroxelHeightFalloff,
		-1000.0f,  // min height
		1000.0f    // max height
	);
	m_xInjectConstants.m_xGridDimensions = Zenith_Maths::Vector4(
		static_cast<float>(FROXEL_WIDTH),
		static_cast<float>(FROXEL_HEIGHT),
		static_cast<float>(FROXEL_DEPTH),
		0.0f
	);
	m_xInjectConstants.m_fNearZ = dbg_fFroxelNearZ;
	m_xInjectConstants.m_fFarZ = dbg_fFroxelFarZ;
	m_xInjectConstants.m_uFrameIndex = g_xEngine.Frame().GetFrameIndex();

	pxCommandList->BindComputePipeline(&m_xInjectPipeline);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	namespace FI = Flux_Generated_Fog::Fog_FroxelInject;
	Flux_ShaderBinder xInjectBinder(*pxCommandList);
	// Spine: VIEW (camera matrices) at set 1. Inject reads no GLOBAL data.
	xInjectBinder.BindCBV(FI::hg_xView, &xGraphics.m_xViewConstantsBuffer.GetCBV());
	xInjectBinder.BindSRV(FI::hu_xNoiseTexture3D, &g_xEngine.VolumeFog().GetNoiseTexture3D()->m_xSRV, &xGraphics.m_xRepeatSampler);
	xInjectBinder.BindUAV_Texture(FI::hu_xDensityGrid, &GetDensityGridInternal().UAV(0));
	xInjectBinder.BindDrawConstants(FI::hInjectConstants, &m_xInjectConstants, sizeof(InjectConstants));
	pxCommandList->Dispatch(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);
}

void Flux_FroxelFogImpl::RenderLight(Flux_CommandBuffer* pxCommandList)
{
	extern u_int dbg_uVolFogDebugMode;
	const Flux_VolumeFogConstants& xShared = g_xEngine.VolumeFog().GetSharedConstants();
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_ShadowsImpl& xShadows = g_xEngine.Shadows();

	m_xLightConstants.m_xFogColour = xShared.m_xFogColour;
	m_xLightConstants.m_xLightDirection = Zenith_Maths::Vector4(
		xGraphics.m_xFrameConstants.m_xSunDir_Pad.x,
		xGraphics.m_xFrameConstants.m_xSunDir_Pad.y,
		xGraphics.m_xFrameConstants.m_xSunDir_Pad.z,
		0.0f
	);
	m_xLightConstants.m_xLightColour = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	m_xLightConstants.m_xGridDimensions = m_xInjectConstants.m_xGridDimensions;
	m_xLightConstants.m_fScatteringCoeff = xShared.m_fScatteringCoeff;
	m_xLightConstants.m_fAbsorptionCoeff = xShared.m_fAbsorptionCoeff;
	m_xLightConstants.m_fPhaseG = dbg_fFroxelPhaseG;
	m_xLightConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	m_xLightConstants.m_fVolShadowBias = dbg_fVolShadowBias;
	m_xLightConstants.m_fVolShadowConeRadius = dbg_fVolShadowConeRadius;
	m_xLightConstants.m_fAmbientIrradianceRatio = xShared.m_fAmbientIrradianceRatio;

	pxCommandList->BindComputePipeline(&m_xLightPipeline);

	namespace FL = Flux_Generated_Fog::Fog_FroxelLight;
	Flux_ShaderBinder xLightBinder(*pxCommandList);
	// Spine: GLOBAL (sun) at set 0, VIEW (camera) at set 1.
	xLightBinder.BindCBV(FL::hg_xGlobal, &xGraphics.m_xGlobalConstantsBuffer.GetCBV());
	xLightBinder.BindCBV(FL::hg_xView, &xGraphics.m_xViewConstantsBuffer.GetCBV());
	xLightBinder.BindSRV(FL::hu_xDensityGrid, &GetDensityGridInternal().SRV());
	xLightBinder.BindUAV_Texture(FL::hu_xLightingGrid, &GetLightingGridInternal().UAV(0));
	xLightBinder.BindUAV_Texture(FL::hu_xScatteringGrid, &GetScatteringGridInternal().UAV(0));

	// CSM is now in the persistent VIEW set (Phase 5.4) — no per-pass bind. The 4 cascade
	// matrices still come from the single ShadowMatrices SSBO (Phase 4a).
	xLightBinder.BindSRV_Buffer(FL::hShadowMatrices, xShadows.GetShadowMatricesSRV());

	xLightBinder.BindDrawConstants(FL::hLightConstants, &m_xLightConstants, sizeof(LightConstants));
	pxCommandList->Dispatch(
		(FROXEL_WIDTH + 7) / 8,
		(FROXEL_HEIGHT + 7) / 8,
		(FROXEL_DEPTH + 7) / 8
	);
}

void Flux_FroxelFogImpl::RenderApply(Flux_CommandBuffer* pxCommandList)
{
	extern u_int dbg_uVolFogDebugMode;

	m_xApplyConstants.m_xGridDimensions = m_xInjectConstants.m_xGridDimensions;
	m_xApplyConstants.m_fNearZ = dbg_fFroxelNearZ;
	m_xApplyConstants.m_fFarZ = dbg_fFroxelFarZ;
	m_xApplyConstants.m_uDebugMode = dbg_uVolFogDebugMode;
	m_xApplyConstants.m_uDebugSliceIndex = dbg_uFroxelDebugSlice;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	pxCommandList->SetPipeline(&m_xApplyPipeline);
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());

	namespace FA = Flux_Generated_Fog::Fog_FroxelApply;
	Flux_ShaderBinder xApplyBinder(*pxCommandList);
	// Spine: VIEW (camera g_xInvProjMat) at set 1. Apply reads no GLOBAL data.
	xApplyBinder.BindCBV(FA::hg_xView, &xGraphics.m_xViewConstantsBuffer.GetCBV());
	xApplyBinder.BindSRV(FA::hu_xDepthTexture, xGraphics.GetDepthStencilSRV());
	xApplyBinder.BindSRV(FA::hu_xLightingGrid, &GetLightingGridInternal().SRV());
	xApplyBinder.BindSRV(FA::hu_xScatteringGrid, &GetScatteringGridInternal().SRV());
	xApplyBinder.BindDrawConstants(FA::hApplyConstants, &m_xApplyConstants, sizeof(ApplyConstants));
	pxCommandList->DrawIndexed(6);
}
