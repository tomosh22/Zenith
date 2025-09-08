#include "Zenith.h"

#include "Flux/Quads/Flux_Quads.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DebugVariables/Zenith_DebugVariables.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_DynamicVertexBuffer s_xInstanceBuffer;

DEBUGVAR bool dbg_bEnable = true;

Flux_Quads::Quad Flux_Quads::s_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
uint32_t Flux_Quads::s_uQuadRenderIndex;

void Flux_Quads::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Quads/Flux_Quads.vert", "Quads/Flux_Quads.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);//position
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);//uv
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_UINT4);//position size
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_UINT);//texture index
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);//UVMult UVAdd
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONEMINUSSRCALPHA, true });

	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		false, //#TO don't write to depth, need to make sure nothing can draw over particles later in the frame
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{ 1,0 },
		{ 0,0 },
		Flux_Graphics::s_xFinalRenderTarget,
		false,
		true
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, FLUX_MAX_QUADS_PER_FRAME * sizeof(Quad), s_xInstanceBuffer, false);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Quads" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_Quads initialised");
}

void Flux_Quads::UploadInstanceData()
{
	//#TO_TODO: need a buffer per frame in flight
	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer(), s_axQuadsToRender, sizeof(Quad) * s_uQuadRenderIndex);
}

void Flux_Quads::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	UploadInstanceData();

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	s_xCommandBuffer.SetVertexBuffer(s_xInstanceBuffer, 1);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.UseBindlessTextures(2);

	s_xCommandBuffer.DrawIndexed(6, s_uQuadRenderIndex);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_QUADS);

	s_uQuadRenderIndex = 0;

}

void Flux_Quads::UploadQuad(const Quad& xQuad)
{
	s_axQuadsToRender[s_uQuadRenderIndex++] = xQuad;
}
