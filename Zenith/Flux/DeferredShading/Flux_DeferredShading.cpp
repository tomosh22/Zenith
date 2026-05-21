#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/IBL/Flux_IBL.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSGI/Flux_SSGI.h"
#include "Flux/DynamicLights/Flux_DynamicLights.h"
#include "Flux/DynamicLights/Flux_LightClustering.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR u_int dbg_uVisualiseCSMs = 0;
DEBUGVAR bool dbg_bVisualiseCSMs = false;
DEBUGVAR u_int dbg_uDeferredShadingDebugMode = 0;  // 0=normal, 1=cyan, 2=depth, 3=diffuse
DEBUGVAR float dbg_fAmbientFallbackIntensity = 0.03f;  // Ambient when IBL disabled (0.01-0.1 typical)

void Flux_DeferredShading::BuildPipelines()
{
	s_xShader.Initialise(FluxShaderProgram::DeferredShading);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = false;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);
}

void Flux_DeferredShading::Initialise()
{
	BuildPipelines();

	#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Shadows", "Visualise CSMs" }, dbg_bVisualiseCSMs);
	Zenith_DebugVariables::AddUInt32({ "Render", "DeferredShading", "DebugMode" }, dbg_uDeferredShadingDebugMode, 0, 3);
	Zenith_DebugVariables::AddFloat({ "Render", "DeferredShading", "AmbientFallback" }, dbg_fAmbientFallbackIntensity, 0.0f, 0.2f);
	#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::DeferredShading,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_DeferredShading::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading initialised");
}

static void ExecuteApplyLighting(Flux_CommandList* pxCommandList, void*)
{
	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	// Use named bindings via shader binder (auto-manages descriptor set switches)
	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind frame constants
	xBinder.BindCBV(s_xShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

	// Bind G-buffer textures (named bindings)
	xBinder.BindSRV(s_xShader, "g_xDiffuseTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xShader, "g_xNormalsAmbientTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xShader, "g_xMaterialTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xShader, "g_xDepthTex", Flux_Graphics::GetDepthStencilSRV());

	// Bind shadow maps (named bindings)
	static const char* const s_aszCSMNames[ZENITH_FLUX_NUM_CSMS] = { "g_xCSM0", "g_xCSM1", "g_xCSM2", "g_xCSM3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xSRV = Flux_Shadows::GetCSMSRV(u);
		xBinder.BindSRV(s_xShader, s_aszCSMNames[u], &xSRV, &g_xEngine.FluxGraphics().m_xClampSampler);
	}

	// Bind shadow matrix buffers (named bindings)
	static const char* const s_aszShadowMatrixNames[ZENITH_FLUX_NUM_CSMS] = { "ShadowMatrix0", "ShadowMatrix1", "ShadowMatrix2", "ShadowMatrix3" };
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBinder.BindCBV(s_xShader, s_aszShadowMatrixNames[u], &Flux_Shadows::GetShadowMatrixBuffer(u).GetCBV());
	}

	// Bind IBL textures
	xBinder.BindSRV(s_xShader, "g_xBRDFLUT", &Flux_IBL::GetBRDFLUTSRV());
	xBinder.BindSRV(s_xShader, "g_xIrradianceMap", &Flux_IBL::GetIrradianceMapSRV());
	xBinder.BindSRV(s_xShader, "g_xPrefilteredMap", &Flux_IBL::GetPrefilteredMapSRV());

	// Always bind SSR texture if initialised (shader checks g_bSSREnabled before sampling)
	// This avoids Vulkan validation errors for unbound descriptors
	if (Flux_SSR::IsInitialised())
	{
		xBinder.BindSRV(s_xShader, "g_xSSRTex", &Flux_SSR::GetReflectionSRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(s_xShader, "g_xSSRTex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

	// Always bind SSGI texture if initialised (shader checks g_bSSGIEnabled before sampling)
	if (Flux_SSGI::IsInitialised())
	{
		xBinder.BindSRV(s_xShader, "g_xSSGITex", &Flux_SSGI::GetSSGISRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(s_xShader, "g_xSSGITex", Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

	// Clustered dynamic lights — buffers populated by Flux_LightClustering.
	// All three are statically referenced by the shader, so all must be
	// bound regardless of whether dynamic lights exist this frame (the
	// fragment shader's cluster loop runs zero iterations when count = 0).
	xBinder.BindSRV_Buffer(s_xShader, "LightBuffer",
		Flux_DynamicLights::GetLightBufferSRV());
	xBinder.BindSRV_Buffer(s_xShader, "ClusterLightCounts",
		Flux_LightClustering::GetClusterLightCountsSRV());
	xBinder.BindSRV_Buffer(s_xShader, "ClusterLightIndices",
		Flux_LightClustering::GetClusterLightIndicesSRV());

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
	xConstants.m_bIBLEnabled = (Flux_IBL::IsEnabled() && Flux_IBL::IsReady()) ? 1 : 0;
	xConstants.m_uDebugMode = dbg_uDeferredShadingDebugMode;
	xConstants.m_bIBLDiffuseEnabled = Flux_IBL::IsDiffuseEnabled() ? 1 : 0;
	xConstants.m_bIBLSpecularEnabled = Flux_IBL::IsSpecularEnabled() ? 1 : 0;
	xConstants.m_fIBLIntensity = Flux_IBL::GetIntensity();
	xConstants.m_bShowBRDFLUT = Flux_IBL::IsShowBRDFLUT() ? 1 : 0;
	xConstants.m_bForceRoughness = Flux_IBL::IsForceRoughness() ? 1 : 0;
	xConstants.m_fForcedRoughness = Flux_IBL::GetForcedRoughness();
	xConstants.m_bSSREnabled = Flux_SSR::IsEnabled() ? 1 : 0;
	xConstants.m_bSSGIEnabled = Flux_SSGI::IsEnabled() ? 1 : 0;
	xConstants.m_fAmbientFallbackIntensity = dbg_fAmbientFallbackIntensity;

	xBinder.BindDrawConstants(s_xShader, "DeferredShadingConstants", &xConstants, sizeof(xConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_DeferredShading::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// First writer of the HDR scene target — clear overwrites every pixel.
	// Capture the handle via the implicit conversion; builder temporary dies
	// at the semicolon. All loop/conditional declarations below go through
	// the graph's Read/ReadTransient helpers with the captured handle.
	const Flux_PassHandle xPass = xGraph.AddPass("Apply Lighting", ExecuteApplyLighting)
		.Writes(Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.ClearTargets();

	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
		xGraph.Read(xPass, Flux_Graphics::GetMRTAttachment(static_cast<MRTIndex>(u)), RESOURCE_ACCESS_READ_SRV);

	xGraph.Read(xPass, Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV);

	// Shadow maps (CSM depth targets)
	for (u_int u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		uint32_t uNumColour;
		Flux_RenderAttachment* pxDepthStencil;
		Flux_Shadows::GetCSMTargetSetup(u, uNumColour, pxDepthStencil);
		xGraph.Read(xPass, *pxDepthStencil, RESOURCE_ACCESS_READ_SRV);
	}

	// Clustered-deferred cluster-output buffers — declaring the reads
	// here causes the graph to order this pass after Flux_LightClustering's
	// compute writes, with the necessary UAV→SRV barrier emitted
	// automatically. The LightBuffer itself is NOT graph-tracked: it's a
	// frame-indexed Flux_DynamicReadWriteBuffer whose GetBuffer() returns
	// a different physical pointer each frame, so any pointer captured
	// here would be stale on subsequent frames. Visibility of its
	// host-side upload is covered by vkQueueSubmit's implicit host-write
	// barrier instead.
	if (Flux_LightClustering::IsInitialised())
	{
		xGraph.ReadBuffer(xPass, Flux_LightClustering::GetClusterLightCountsBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadBuffer(xPass, Flux_LightClustering::GetClusterLightIndicesBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
	}

	// SSR / SSGI single-handle declarations. The subsystem decides which of
	// its internal handles serves as "the output" based on its debug toggles
	// at SetupRenderGraph time. Runtime toggles trigger Flux::RequestGraphRebuild()
	// via ApplyBlurSelectionToGraph / ApplyDenoiseSelectionToGraph, which re-runs
	// this SetupRenderGraph and re-resolves the handle.
	if (Flux_SSR::IsInitialised())
		xGraph.ReadTransient(xPass, Flux_SSR::GetReflectionHandle(), RESOURCE_ACCESS_READ_SRV);
	if (Flux_SSGI::IsInitialised())
		xGraph.ReadTransient(xPass, Flux_SSGI::GetSSGIHandle(), RESOURCE_ACCESS_READ_SRV);

	// IBL textures — BRDF LUT, irradiance cubemap, prefiltered cubemap. Cubemap
	// reads default to FLUX_RG_ALL_MIPS / FLUX_RG_ALL_LAYERS.
	xGraph.Read(xPass, Flux_IBL::s_xBRDFLUT,        RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, Flux_IBL::s_xIrradianceMap,  RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, Flux_IBL::s_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);
}