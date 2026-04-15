#include "Zenith.h"

#include "Flux/Quads/Flux_Quads.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_DynamicVertexBuffer s_xInstanceBuffer;

DEBUGVAR bool dbg_bEnable = true;

Flux_Quads::Quad Flux_Quads::s_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
uint32_t Flux_Quads::s_uQuadRenderIndex;

void Flux_Quads::Initialise()
{
	s_xShader.Initialise("Quads/Flux_Quads.vert", "Quads/Flux_Quads.frag");

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
	xPipelineSpec.m_aeColourAttachmentFormats[0] = Flux_Graphics::s_xFinalRenderTarget_NoDepth.m_xSurfaceInfo.m_eFormat;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), s_xInstanceBuffer, false);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Quads" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads initialised");
}

void Flux_Quads::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Quads shut down");
}

void Flux_Quads::UploadInstanceData()
{
	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer().m_xVRAMHandle, s_axQuadsToRender, sizeof(Quad) * s_uQuadRenderIndex);
}

void Flux_Quads::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	UploadInstanceData();
}

void Flux_Quads::ExecuteQuads(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!dbg_bEnable || s_uQuadRenderIndex == 0)
	{
		return;
	}

	UploadInstanceData();

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBuffer, 1);

	pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

	pxCommandList->AddCommand<Flux_CommandUseUnboundedTextures>(1);

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, s_uQuadRenderIndex);

	s_uQuadRenderIndex = 0;
}

void Flux_Quads::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	uint32_t uPass = xGraph.AddPass("Quads", ExecuteQuads);
	xGraph.Write(uPass, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RESOURCE_ACCESS_WRITE_RTV);
}

void Flux_Quads::UploadQuad(const Quad& xQuad)
{
	if (!dbg_bEnable)
	{
		return;
	}

	s_axQuadsToRender[s_uQuadRenderIndex++] = xQuad;
}
