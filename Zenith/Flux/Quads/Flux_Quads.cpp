#include "Zenith.h"

#include "Flux/Quads/Flux_Quads.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_QUADS, Flux_Quads::Render, nullptr);

static Flux_CommandList g_xCommandList("Quads");

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
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget_NoDepth;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 2;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;
	xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_UNBOUNDED_TEXTURES;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), s_xInstanceBuffer, false);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Quads" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_Quads initialised");
}

void Flux_Quads::UploadInstanceData()
{
	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer().m_xVRAMHandle, s_axQuadsToRender, sizeof(Quad) * s_uQuadRenderIndex);
}

void Flux_Quads::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Quads::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Quads::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	UploadInstanceData();

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBuffer, 1);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

	g_xCommandList.AddCommand<Flux_CommandUseUnboundedTextures>(1);

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uQuadRenderIndex);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_QUADS);

	s_uQuadRenderIndex = 0;

}

void Flux_Quads::UploadQuad(const Quad& xQuad)
{
	if (!dbg_bEnable)
	{
		return;
	}

	s_axQuadsToRender[s_uQuadRenderIndex++] = xQuad;
}
