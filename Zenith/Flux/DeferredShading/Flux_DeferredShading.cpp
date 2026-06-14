#include "Zenith.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_DeferredShadingImpl held by Zenith_Engine.

DEBUGVAR u_int dbg_uVisualiseCSMs = 0;
DEBUGVAR bool dbg_bVisualiseCSMs = false;
DEBUGVAR u_int dbg_uDeferredShadingDebugMode = 0;  // 0=normal, 1=cyan, 2=depth, 3=diffuse
DEBUGVAR float dbg_fAmbientFallbackIntensity = 0.03f;  // Ambient when IBL disabled (0.01-0.1 typical)

void Flux_DeferredShadingImpl::BuildPipelines()
{
	m_xShader.Initialise(FluxShaderProgram::DeferredShading);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = false;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xPipelineSpec);
}

void Flux_DeferredShadingImpl::Initialise()
{
	BuildPipelines();

	#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Render", "Shadows", "Visualise CSMs" }, dbg_bVisualiseCSMs);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "DeferredShading", "DebugMode" }, dbg_uDeferredShadingDebugMode, 0, 3);
	g_xEngine.DebugVariables().AddFloat({ "Render", "DeferredShading", "AmbientFallback" }, dbg_fAmbientFallbackIntensity, 0.0f, 0.2f);
	#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::DeferredShading,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.DeferredShading().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading initialised");
}

void Flux_DeferredShadingImpl::Shutdown()
{
	// Pipeline references its shader, so destroy pipeline first.
	m_xPipeline.Reset();
	m_xShader.Reset();
}

static void ExecuteApplyLighting(Flux_CommandList* pxCommandList, void*)
{
	Flux_DeferredShadingImpl& xDS = g_xEngine.DeferredShading();
	Flux_GraphicsImpl& xFluxGraphics = g_xEngine.FluxGraphics();
	Flux_ShadowsImpl& xShadows = g_xEngine.Shadows();
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	Flux_SSRImpl& xSSR = g_xEngine.SSR();
	Flux_SSGIImpl& xSSGI = g_xEngine.SSGI();
	Flux_DynamicLightsImpl& xDynamicLights = g_xEngine.DynamicLights();
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xDS.m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xFluxGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xFluxGraphics.m_xQuadMesh.GetIndexBuffer());

	// Use named bindings via shader binder (auto-manages descriptor set switches)
	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind frame constants
	xBinder.BindCBV(xDS.m_xShader, "FrameConstants", &xFluxGraphics.m_xFrameConstantsBuffer.GetCBV());

	// Bind G-buffer textures (named bindings)
	xBinder.BindSRV(xDS.m_xShader, "g_xDiffuseTex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(xDS.m_xShader, "g_xNormalsAmbientTex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(xDS.m_xShader, "g_xMaterialTex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(xDS.m_xShader, "g_xGBufferEmissiveTex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_EMISSIVE));
	xBinder.BindSRV(xDS.m_xShader, "g_xDepthTex", xFluxGraphics.GetDepthStencilSRV());

	// Bind shadow maps (named bindings)
	static const char* const s_aszCSMNames[ZENITH_FLUX_NUM_CSMS] = { "g_xCSM0", "g_xCSM1", "g_xCSM2", "g_xCSM3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xSRV = xShadows.GetCSMSRV(u);
		xBinder.BindSRV(xDS.m_xShader, s_aszCSMNames[u], &xSRV, &xFluxGraphics.m_xClampSampler);
	}

	// Bind shadow matrix buffers (named bindings)
	static const char* const s_aszShadowMatrixNames[ZENITH_FLUX_NUM_CSMS] = { "ShadowMatrix0", "ShadowMatrix1", "ShadowMatrix2", "ShadowMatrix3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBinder.BindCBV(xDS.m_xShader, s_aszShadowMatrixNames[u], &xShadows.GetShadowMatrixBuffer(u).GetCBV());
	}

	// Bind IBL textures
	xBinder.BindSRV(xDS.m_xShader, "g_xBRDFLUT", &xIBL.GetBRDFLUTSRV());
	xBinder.BindSRV(xDS.m_xShader, "g_xIrradianceMap", &xIBL.GetIrradianceMapSRV());
	xBinder.BindSRV(xDS.m_xShader, "g_xPrefilteredMap", &xIBL.GetPrefilteredMapSRV());

	// Always bind SSR texture if initialised (shader checks g_bSSREnabled before sampling)
	// This avoids Vulkan validation errors for unbound descriptors
	if (xSSR.IsInitialised())
	{
		xBinder.BindSRV(xDS.m_xShader, "g_xSSRTex", &xSSR.GetReflectionSRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(xDS.m_xShader, "g_xSSRTex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

	// Always bind SSGI texture if initialised (shader checks g_bSSGIEnabled before sampling)
	if (xSSGI.IsInitialised())
	{
		xBinder.BindSRV(xDS.m_xShader, "g_xSSGITex", &xSSGI.GetSSGISRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(xDS.m_xShader, "g_xSSGITex", xFluxGraphics.GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

	// Clustered dynamic lights — buffers populated by Flux_LightClustering.
	// All three are statically referenced by the shader, so all must be
	// bound regardless of whether dynamic lights exist this frame (the
	// fragment shader's cluster loop runs zero iterations when count = 0).
	xBinder.BindSRV_Buffer(xDS.m_xShader, "LightBuffer",
		xDynamicLights.GetLightBufferSRV());
	xBinder.BindSRV_Buffer(xDS.m_xShader, "ClusterLightCounts",
		xLightClustering.GetClusterLightCountsSRV());
	xBinder.BindSRV_Buffer(xDS.m_xShader, "ClusterLightIndices",
		xLightClustering.GetClusterLightIndicesSRV());

	// Pass constants to shader
	struct DeferredShadingConstants
	{
		u_int m_bVisualiseCSMs;
		u_int m_bIBLEnabled;
		u_int m_uDebugMode;  // 0=normal, 1=cyan (verify running), 2=depth, 3=diffuse
		u_int m_bIBLDiffuseEnabled;
		u_int m_bIBLSpecularEnabled;
		float m_fIBLIntensity;
		u_int m_bShowBRDFLUT;
		u_int m_bForceRoughness;
		float m_fForcedRoughness;
		u_int m_bSSREnabled;
		u_int m_bSSGIEnabled;
		float m_fAmbientFallbackIntensity;  // Configurable ambient when IBL disabled
	};
	DeferredShadingConstants xConstants;
	xConstants.m_bVisualiseCSMs = dbg_uVisualiseCSMs;
	// Only enable IBL if both enabled AND ready (textures have been generated)
	xConstants.m_bIBLEnabled = (xIBL.IsEnabled() && xIBL.IsReady()) ? 1 : 0;
	xConstants.m_uDebugMode = dbg_uDeferredShadingDebugMode;
	xConstants.m_bIBLDiffuseEnabled = xIBL.IsDiffuseEnabled() ? 1 : 0;
	xConstants.m_bIBLSpecularEnabled = xIBL.IsSpecularEnabled() ? 1 : 0;
	xConstants.m_fIBLIntensity = xIBL.GetIntensity();
	xConstants.m_bShowBRDFLUT = xIBL.IsShowBRDFLUT() ? 1 : 0;
	xConstants.m_bForceRoughness = xIBL.IsForceRoughness() ? 1 : 0;
	xConstants.m_fForcedRoughness = xIBL.GetForcedRoughness();
	xConstants.m_bSSREnabled = xSSR.IsEnabled() ? 1 : 0;
	xConstants.m_bSSGIEnabled = xSSGI.IsEnabled() ? 1 : 0;
	xConstants.m_fAmbientFallbackIntensity = dbg_fAmbientFallbackIntensity;

	xBinder.BindDrawConstants(xDS.m_xShader, "DeferredShadingConstants", &xConstants, sizeof(xConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_DeferredShadingImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// First writer of the HDR scene target — clear overwrites every pixel.
	// Capture the handle via the implicit conversion; builder temporary dies
	// at the semicolon. All loop/conditional declarations below go through
	// the graph's Read/ReadTransient helpers with the captured handle.
	const Flux_PassHandle xPass = xGraph.AddPass("Apply Lighting", ExecuteApplyLighting)
		.Writes(g_xEngine.HDR().GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.ClearTargets();

	Flux_GraphicsImpl& xFluxGraphics = g_xEngine.FluxGraphics();

	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
		xGraph.Read(xPass, xFluxGraphics.GetMRTAttachment(static_cast<MRTIndex>(u)), RESOURCE_ACCESS_READ_SRV);

	xGraph.Read(xPass, xFluxGraphics.GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// Shadow maps (CSM depth targets)
	for (u_int u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		uint32_t uNumColour;
		Flux_RenderAttachment* pxDepthStencil;
		g_xEngine.Shadows().GetCSMTargetSetup(u, uNumColour, pxDepthStencil);
		xGraph.Read(xPass, *pxDepthStencil, RESOURCE_ACCESS_READ_SRV);
	}

	// Clustered-deferred cluster-output buffers — declaring the reads here causes
	// the graph to order this pass after Flux_LightClustering's compute writes,
	// with the necessary UAV→SRV barrier emitted automatically. The LightBuffer
	// itself is a frame-indexed Flux_DynamicReadWriteBuffer and so is NOT
	// graph-tracked — see the RENDER-GRAPH CONTRACT on Flux_FrameIndexedBufferBase
	// (Flux_Buffers.h).
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (xLightClustering.IsInitialised())
	{
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
	}

	// SSR / SSGI single-handle declarations. The subsystem decides which of
	// its internal handles serves as "the output" based on its debug toggles
	// at SetupRenderGraph time. Runtime toggles trigger g_xEngine.FluxRenderer().RequestGraphRebuild()
	// via ApplyBlurSelectionToGraph / ApplyDenoiseSelectionToGraph, which re-runs
	// this SetupRenderGraph and re-resolves the handle.
	if (g_xEngine.SSR().IsInitialised())
		xGraph.ReadTransient(xPass, g_xEngine.SSR().GetReflectionHandle(), RESOURCE_ACCESS_READ_SRV);
	if (g_xEngine.SSGI().IsInitialised())
		xGraph.ReadTransient(xPass, g_xEngine.SSGI().GetSSGIHandle(), RESOURCE_ACCESS_READ_SRV);

	// IBL textures — BRDF LUT, irradiance cubemap, prefiltered cubemap. Cubemap
	// reads default to FLUX_RG_ALL_MIPS / FLUX_RG_ALL_LAYERS.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	xGraph.Read(xPass, xIBL.m_xBRDFLUT,        RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, xIBL.m_xIrradianceMap,  RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, xIBL.m_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);
}
