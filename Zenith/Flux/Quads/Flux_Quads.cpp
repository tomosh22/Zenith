#include "Zenith.h"

#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_QuadsImpl held by Zenith_Engine.
//
// Wave-14 DI seam (mirrors Flux_SSAOImpl): the lone cross-subsystem dep
// (Flux_GraphicsImpl) is injected into Initialise and stored in m_pxGraphics;
// instance methods read their own members via this-> and route FluxGraphics
// reach-ins through m_pxGraphics. g_xEngine.Quads() self-lookup survives only in
// the non-capturing ExecuteQuads / hot-reload fn-pointer trampolines below;
// VulkanMemory()/DebugVariables() stay direct engine-infra lookups.



static void ExecuteQuads(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_QuadsImpl::BuildPipelines()
{
	m_xShader.Initialise(FluxShaderProgram::Quads);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);//position
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//uv
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT4);//position size
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT);//texture index
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//UVMult UVAdd
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT);//corner radius
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//size pixels
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour2 (gradient)
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(m_xPipeline, xPipelineSpec);
}

void Flux_QuadsImpl::Initialise(Flux_GraphicsImpl& xGraphics)
{
	// Wave-14 DI seam: store the injected cross-subsystem dep. The FluxGraphics
	// reach-ins (in ExecuteQuads / SetupRenderGraph) route through this instead of
	// g_xEngine.FluxGraphics().
	m_pxGraphics = &xGraphics;

	BuildPipelines();

	g_xEngine.VulkanMemory().InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), m_xInstanceBuffer, false);

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Quads,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Quads().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads initialised");
}

void Flux_QuadsImpl::Shutdown()
{
	g_xEngine.VulkanMemory().DestroyDynamicVertexBuffer(m_xInstanceBuffer);
	// Drop the injected dep so the instance returns to a clean default state.
	m_pxGraphics = nullptr;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads shut down");
}

void Flux_QuadsImpl::UploadInstanceData()
{
	g_xEngine.VulkanMemory().UploadBufferData(m_xInstanceBuffer.GetBuffer().m_xVRAMHandle, m_axQuadsToRender, sizeof(Quad) * m_uQuadRenderIndex);
}

void Flux_QuadsImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}

	UploadInstanceData();
}

static void ExecuteQuads(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	// Non-capturing graph callback (void(*)(Flux_CommandList*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Quads() to reach the singleton
	// instance, then routes its FluxGraphics reach-ins through the injected member
	// (mirrors ExecuteSSAOGenerate).
	Flux_QuadsImpl& xQuads = g_xEngine.Quads();

	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled || xQuads.m_uQuadRenderIndex == 0)
	{
		return;
	}

	xQuads.UploadInstanceData();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&xQuads.m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xQuads.m_pxGraphics->m_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&xQuads.m_pxGraphics->m_xQuadMesh.GetIndexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&xQuads.m_xInstanceBuffer, 1);

	pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&xQuads.m_pxGraphics->m_xFrameConstantsBuffer.GetCBV(), 0);

	pxCommandList->AddCommand<Flux_CommandUseUnboundedTextures>(1);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, xQuads.m_uQuadRenderIndex);

	xQuads.m_uQuadRenderIndex = 0;
}

void Flux_QuadsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Quads", ExecuteQuads)
		.Writes(m_pxGraphics->GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}

void Flux_QuadsImpl::UploadQuad(const Quad& xQuad)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}

	m_axQuadsToRender[m_uQuadRenderIndex++] = xQuad;
}
