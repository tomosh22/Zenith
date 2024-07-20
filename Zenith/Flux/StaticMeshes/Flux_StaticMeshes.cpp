#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshes.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static Flux_VertexBuffer s_xVertexBuffer;
static Flux_IndexBuffer s_xIndexBuffer;

//#TO_TODO: delete me
static Zenith_GUID xMeshGUID = 90834834756u;
static Flux_MeshGeometry& s_xMesh = Flux_Graphics::s_xBlankMesh;

void Flux_StaticMeshes::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("StaticMeshes/Flux_StaticMeshes.vert", "StaticMeshes/Flux_StaticMeshes.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });

	Zenith_Vulkan_PipelineSpecification xPipelineSpec(
		"Flux_StaticMeshes",
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		{ COLOUR_FORMAT_BGRA8_SRGB },
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		"#TO_TODO: delete me",
		true,
		false,
		{ {1,0} },
		//{ {3,0} },
		Flux_Graphics::s_xFinalRenderTarget,
		LOAD_ACTION_LOAD,
		STORE_ACTION_STORE,
		LOAD_ACTION_LOAD,
		STORE_ACTION_STORE,
		RENDER_TARGET_USAGE_RENDERTARGET
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	s_xMesh = Zenith_AssetHandler::GetMesh(xMeshGUID);

	Flux_MemoryManager::InitialiseVertexBuffer(s_xMesh.GetVertexData(), s_xMesh.GetVertexDataSize(), s_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(s_xMesh.GetIndexData(), s_xMesh.GetIndexDataSize(), s_xIndexBuffer);

	Zenith_Log("Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::Render()
{
	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(s_xVertexBuffer);
	s_xCommandBuffer.SetIndexBuffer(s_xIndexBuffer);

	Zenith_Maths::Matrix4 xModelMatrix = glm::scale(glm::identity<Zenith_Maths::Matrix4>(), glm::vec3(100, 100, 100));
	s_xCommandBuffer.PushConstant(&xModelMatrix, sizeof(xModelMatrix));

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.DrawIndexed(s_xMesh.GetNumIndices());

	s_xCommandBuffer.EndRecording(RENDER_ORDER_OPAQUE_MESHES);
}
