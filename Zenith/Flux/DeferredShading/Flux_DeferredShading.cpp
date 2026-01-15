#include "Zenith.h"

#include "Flux/DeferredShading/Flux_DeferredShading.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, Flux_DeferredShading::Render, nullptr);

static Flux_CommandList g_xCommandList("Apply Lighting");

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

DEBUGVAR u_int dbg_uVisualiseCSMs = 0;
DEBUGVAR bool dbg_bVisualiseCSMs = false;

void Flux_DeferredShading::Initialise()
{
	s_xShader.Initialise("Flux_Fullscreen_UV.vert", "DeferredShading/Flux_DeferredShading.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget_NoDepth;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Shadow matrix 0
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Shadow matrix 1
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Shadow matrix 2
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Shadow matrix 3
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[6].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[7].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[8].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[9].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[10].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[11].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[12].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[13].m_eType = DESCRIPTOR_TYPE_TEXTURE;

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

	#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Shadows", "Visualise CSMs" }, dbg_bVisualiseCSMs);
	#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading initialised");
}

void Flux_DeferredShading::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DeferredShading::Reset() - Reset command list");
}

void Flux_DeferredShading::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_DeferredShading::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_DeferredShading::Render(void*)
{
	g_xCommandList.Reset(true);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

	// Use named bindings via shader binder (auto-manages descriptor set switches)
	Flux_ShaderBinder xBinder(g_xCommandList);

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

	xBinder.PushConstant(&dbg_uVisualiseCSMs, sizeof(dbg_uVisualiseCSMs));

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_APPLY_LIGHTING);
}