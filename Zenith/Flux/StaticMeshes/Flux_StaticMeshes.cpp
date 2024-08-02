#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshes.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"

static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_Enable = true;

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
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
	xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });

	std::vector<ColourFormat> xFormats;
	for (ColourFormat eFormat : Flux_Graphics::s_aeMRTFormats)
	{
		xFormats.push_back(eFormat);
	}

	Zenith_Vulkan_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		true,
		DEPTH_COMPARE_FUNC_GREATEREQUAL,
		xFormats,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{1,0},
		{0,4},
		Flux_Graphics::s_xMRTTarget,
		LOAD_ACTION_LOAD,
		STORE_ACTION_STORE,
		LOAD_ACTION_LOAD,
		STORE_ACTION_STORE,
		RENDER_TARGET_USAGE_RENDERTARGET
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Static Meshes" }, dbg_Enable);
#endif

	Zenith_Log("Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::Render()
{
	if (!dbg_Enable)
	{
		return;
	}

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	std::vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);

	for (Zenith_ModelComponent* pxModel : xModels)
	{
		s_xCommandBuffer.SetVertexBuffer(pxModel->GetMeshGeometry().GetVertexBuffer());
		s_xCommandBuffer.SetIndexBuffer(pxModel->GetMeshGeometry().GetIndexBuffer());

		Zenith_Maths::Matrix4 xModelMatrix;
		pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
		s_xCommandBuffer.PushConstant(&xModelMatrix, sizeof(xModelMatrix));
		Flux_Material& xMaterial = pxModel->GetMaterial();

		s_xCommandBuffer.BindTexture(xMaterial.GetDiffuse(), 0);
		s_xCommandBuffer.BindTexture(xMaterial.GetNormal(), 1);
		s_xCommandBuffer.BindTexture(xMaterial.GetRoughness(), 2);
		s_xCommandBuffer.BindTexture(xMaterial.GetMetallic(), 3);

		s_xCommandBuffer.DrawIndexed(pxModel->GetMeshGeometry().GetNumIndices());
	}

	s_xCommandBuffer.EndRecording(RENDER_ORDER_OPAQUE_MESHES);
}
