#include "Zenith.h"

#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_QuadsImpl held by Zenith_Engine.



static void ExecuteQuads(Flux_CommandList* pxCommandList, void* pUserData);

void Flux_QuadsImpl::BuildPipelines()
{
	g_xEngine.Quads().m_xShader.Initialise(FluxShaderProgram::Quads);

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
	xPipelineSpec.m_pxShader = &g_xEngine.Quads().m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.Quads().m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.Quads().m_xPipeline, xPipelineSpec);
}

void Flux_QuadsImpl::Initialise()
{
	BuildPipelines();

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), g_xEngine.Quads().m_xInstanceBuffer, false);

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
	Flux_MemoryManager::DestroyDynamicVertexBuffer(g_xEngine.Quads().m_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads shut down");
}

void Flux_QuadsImpl::UploadInstanceData()
{
	Flux_MemoryManager::UploadBufferData(g_xEngine.Quads().m_xInstanceBuffer.GetBuffer().m_xVRAMHandle, g_xEngine.Quads().m_axQuadsToRender, sizeof(Quad) * g_xEngine.Quads().m_uQuadRenderIndex);
}

void Flux_QuadsImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}

	g_xEngine.Quads().UploadInstanceData();
}

static void ExecuteQuads(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled || g_xEngine.Quads().m_uQuadRenderIndex == 0)
	{
		return;
	}

	g_xEngine.Quads().UploadInstanceData();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Quads().m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.Quads().m_xInstanceBuffer, 1);

	pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);

	pxCommandList->AddCommand<Flux_CommandUseUnboundedTextures>(1);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, g_xEngine.Quads().m_uQuadRenderIndex);

	g_xEngine.Quads().m_uQuadRenderIndex = 0;
}

void Flux_QuadsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Quads", ExecuteQuads)
		.Writes(Flux_Graphics::GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}

void Flux_QuadsImpl::UploadQuad(const Quad& xQuad)
{
	if (!Zenith_GraphicsOptions::Get().m_bQuadsEnabled)
	{
		return;
	}

	g_xEngine.Quads().m_axQuadsToRender[g_xEngine.Quads().m_uQuadRenderIndex++] = xQuad;
}
