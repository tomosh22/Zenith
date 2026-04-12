#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/IBL/Flux_IBL.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSGI/Flux_SSGI.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

// Cached binding handles for named resource binding (populated at init from shader reflection)
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_axShadowMatrixBindings[ZENITH_FLUX_NUM_CSMS];
static Flux_BindingHandle s_xDiffuseTexBinding;
static Flux_BindingHandle s_xNormalsAmbientTexBinding;
static Flux_BindingHandle s_xMaterialTexBinding;
static Flux_BindingHandle s_xDepthTexBinding;
static Flux_BindingHandle s_axCSMBindings[ZENITH_FLUX_NUM_CSMS];

// IBL texture bindings
static Flux_BindingHandle s_xBRDFLUTBinding;
static Flux_BindingHandle s_xIrradianceMapBinding;
static Flux_BindingHandle s_xPrefilteredMapBinding;

// SSR texture binding
static Flux_BindingHandle s_xSSRTexBinding;

// SSGI texture binding
static Flux_BindingHandle s_xSSGITexBinding;

DEBUGVAR u_int dbg_uVisualiseCSMs = 0;
DEBUGVAR bool dbg_bVisualiseCSMs = false;
DEBUGVAR u_int dbg_uDeferredShadingDebugMode = 0;  // 0=normal, 1=cyan, 2=depth, 3=diffuse
DEBUGVAR float dbg_fAmbientFallbackIntensity = 0.03f;  // Ambient when IBL disabled (0.01-0.1 typical)

void Flux_DeferredShading::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "DeferredShading/Flux_DeferredShading.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	// Render to HDR target for proper HDR lighting pipeline (tone mapping converts to final output)
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = false;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	// Cache binding handles from shader reflection for named resource binding
	const Flux_ShaderReflection& xReflection = s_xShader.GetReflection();
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_axShadowMatrixBindings[0] = xReflection.GetBinding("ShadowMatrix0");
	s_axShadowMatrixBindings[1] = xReflection.GetBinding("ShadowMatrix1");
	s_axShadowMatrixBindings[2] = xReflection.GetBinding("ShadowMatrix2");
	s_axShadowMatrixBindings[3] = xReflection.GetBinding("ShadowMatrix3");
	s_xDiffuseTexBinding = xReflection.GetBinding("g_xDiffuseTex");
	s_xNormalsAmbientTexBinding = xReflection.GetBinding("g_xNormalsAmbientTex");
	s_xMaterialTexBinding = xReflection.GetBinding("g_xMaterialTex");
	s_xDepthTexBinding = xReflection.GetBinding("g_xDepthTex");
	s_axCSMBindings[0] = xReflection.GetBinding("g_xCSM0");
	s_axCSMBindings[1] = xReflection.GetBinding("g_xCSM1");
	s_axCSMBindings[2] = xReflection.GetBinding("g_xCSM2");
	s_axCSMBindings[3] = xReflection.GetBinding("g_xCSM3");

	// IBL bindings
	s_xBRDFLUTBinding = xReflection.GetBinding("g_xBRDFLUT");
	s_xIrradianceMapBinding = xReflection.GetBinding("g_xIrradianceMap");
	s_xPrefilteredMapBinding = xReflection.GetBinding("g_xPrefilteredMap");

	// SSR binding
	s_xSSRTexBinding = xReflection.GetBinding("g_xSSRTex");

	// SSGI binding
	s_xSSGITexBinding = xReflection.GetBinding("g_xSSGITex");

	// Debug: Log IBL binding handles
	Zenith_Log(LOG_CATEGORY_RENDERER, "IBL Bindings - BRDF: set=%u binding=%u valid=%d, Irradiance: set=%u binding=%u valid=%d, Prefiltered: set=%u binding=%u valid=%d",
		s_xBRDFLUTBinding.m_uSet, s_xBRDFLUTBinding.m_uBinding, s_xBRDFLUTBinding.IsValid(),
		s_xIrradianceMapBinding.m_uSet, s_xIrradianceMapBinding.m_uBinding, s_xIrradianceMapBinding.IsValid(),
		s_xPrefilteredMapBinding.m_uSet, s_xPrefilteredMapBinding.m_uBinding, s_xPrefilteredMapBinding.IsValid());

	#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Shadows", "Visualise CSMs" }, dbg_bVisualiseCSMs);
	Zenith_DebugVariables::AddUInt32({ "Render", "DeferredShading", "DebugMode" }, dbg_uDeferredShadingDebugMode, 0, 3);
	Zenith_DebugVariables::AddFloat({ "Render", "DeferredShading", "AmbientFallback" }, dbg_fAmbientFallbackIntensity, 0.0f, 0.2f);
	#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading initialised");
}

static void ExecuteApplyLighting(Flux_CommandList* pxCommandList, void*)
{
	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	// Use named bindings via shader binder (auto-manages descriptor set switches)
	Flux_ShaderBinder xBinder(*pxCommandList);

	// Bind frame constants
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	// Bind G-buffer textures (named bindings)
	xBinder.BindSRV(s_xDiffuseTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xNormalsAmbientTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xMaterialTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xDepthTexBinding, Flux_Graphics::GetDepthStencilSRV());

	// Bind shadow maps (named bindings)
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		Flux_ShaderResourceView& xSRV = Flux_Shadows::GetCSMSRV(u);
		xBinder.BindSRV(s_axCSMBindings[u], &xSRV, &Flux_Graphics::s_xClampSampler);
	}

	// Bind shadow matrix buffers (named bindings)
	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xBinder.BindCBV(s_axShadowMatrixBindings[u], &Flux_Shadows::GetShadowMatrixBuffer(u).GetCBV());
	}

	// Bind IBL textures
	xBinder.BindSRV(s_xBRDFLUTBinding, &Flux_IBL::GetBRDFLUTSRV());
	xBinder.BindSRV(s_xIrradianceMapBinding, &Flux_IBL::GetIrradianceMapSRV());
	xBinder.BindSRV(s_xPrefilteredMapBinding, &Flux_IBL::GetPrefilteredMapSRV());

	// Always bind SSR texture if initialised (shader checks g_bSSREnabled before sampling)
	// This avoids Vulkan validation errors for unbound descriptors
	if (Flux_SSR::IsInitialised())
	{
		xBinder.BindSRV(s_xSSRTexBinding, &Flux_SSR::GetReflectionSRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(s_xSSRTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

	// Always bind SSGI texture if initialised (shader checks g_bSSGIEnabled before sampling)
	if (Flux_SSGI::IsInitialised())
	{
		xBinder.BindSRV(s_xSSGITexBinding, &Flux_SSGI::GetSSGISRV());
	}
	else
	{
		// Fallback: bind diffuse G-Buffer as placeholder to satisfy descriptor validation
		xBinder.BindSRV(s_xSSGITexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	}

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

	xBinder.PushConstant(&xConstants, sizeof(xConstants));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

void Flux_DeferredShading::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	u_int uPassIndex = xGraph.AddPass("Apply Lighting", ExecuteApplyLighting);
	xGraph.SetTargetSetup(uPassIndex, Flux_HDR::GetHDRSceneTargetSetup());
	// First writer of the HDR scene no-depth setup — overwrites every pixel
	// with the lighting result, so clear is the correct LoadOp.
	xGraph.SetClear(uPassIndex, true);

	// Reads: G-Buffer MRT attachments
	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
	{
		xGraph.Read(uPassIndex, Flux_Graphics::s_xMRTTarget.m_axColourAttachments[u], RESOURCE_ACCESS_READ_SRV);
	}

	// Reads: depth buffer
	xGraph.Read(uPassIndex, Flux_Graphics::s_xDepthBuffer, RESOURCE_ACCESS_READ_SRV);

	// Reads: shadow maps (CSM depth targets)
	for (u_int u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		xGraph.Read(uPassIndex, *Flux_Shadows::GetCSMTargetSetup(u).m_pxDepthStencil, RESOURCE_ACCESS_READ_SRV);
	}

	// Reads: SSR results. The execute callback binds GetReflectionSRV() which
	// returns either s_xResolvedReflection or s_xRayMarchResult depending on
	// the dbg_bRoughnessBlur runtime toggle. Same dual-binding pattern as SSGI
	// — SetupRenderGraph runs once at init / on resolution change so we have
	// to declare BOTH so whichever one ends up bound at record time has been
	// transitioned out of COLOR_ATTACHMENT_OPTIMAL by the graph.
	if (Flux_SSR::IsInitialised())
	{
		xGraph.Read(uPassIndex, Flux_SSR::s_xRayMarchResult, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_SSR::s_xResolvedReflection, RESOURCE_ACCESS_READ_SRV);
	}

	// Reads: SSGI results. The execute callback binds GetSSGISRV() which returns
	// either s_xResolved or s_xDenoised depending on a runtime debug variable.
	// SetupRenderGraph runs once at init (and on resolution change), so we have
	// to declare BOTH so the graph transitions whichever one ends up bound at
	// record time. Both are RGBA16F color attachments written by the SSGI passes
	// — without these declarations they stay in COLOR_ATTACHMENT_OPTIMAL and the
	// validator rejects the SRV bind here.
	if (Flux_SSGI::IsInitialised())
	{
		xGraph.Read(uPassIndex, Flux_SSGI::s_xResolved, RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(uPassIndex, Flux_SSGI::s_xDenoised, RESOURCE_ACCESS_READ_SRV);
	}

	// Reads: IBL textures. The IBL graph passes write these as color attachments
	// (BRDF LUT once at init, irradiance / prefiltered cubemap faces frame-amortised
	// when the sky is dirty), so they sit in COLOR_ATTACHMENT_OPTIMAL until the
	// graph transitions them. Cubemaps cover all faces via the FLUX_RG_ALL_LAYERS
	// default, prefilter covers all mips via FLUX_RG_ALL_MIPS.
	xGraph.Read(uPassIndex, Flux_IBL::s_xBRDFLUT, RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(uPassIndex, Flux_IBL::s_xIrradianceMap, RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(uPassIndex, Flux_IBL::s_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);

	// Writes: HDR scene target
	xGraph.Write(uPassIndex, Flux_HDR::GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV);
}