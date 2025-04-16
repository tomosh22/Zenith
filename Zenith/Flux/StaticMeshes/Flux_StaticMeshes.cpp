#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshes.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#ifndef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xCommandBuffer;
#endif

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

DEBUGVAR bool dbg_bEnable = true;

void Flux_StaticMeshes::Initialise()
{
	#ifndef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.Initialise();
	#endif

	s_xGBufferShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToGBuffer.vert", "StaticMeshes/Flux_StaticMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToShadowMap.vert", "StaticMeshes/Flux_StaticMeshes_ToShadowMap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{
		std::vector<Flux_BlendState> xBlendStates;
		xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
		xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
		xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });
		xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });

		Flux_PipelineSpecification xPipelineSpec(
			xVertexDesc,
			&s_xGBufferShader,
			xBlendStates,
			true,
			true,
			DEPTH_COMPARE_FUNC_LESSEQUAL,
			DEPTHSTENCIL_FORMAT_D32_SFLOAT,
			true,
			false,
			{ 1,0 },
			{ 0,4 },
			Flux_Graphics::s_xMRTTarget,
			false,
			false
		);

		Flux_PipelineBuilder::FromSpecification(s_xGBufferPipeline, xPipelineSpec);
	}

	{
		//#TO_TODO: shouldn't need this at all, this is depth only
		std::vector<Flux_BlendState> xBlendStates;
		xBlendStates.push_back({ BLEND_FACTOR_ZERO, BLEND_FACTOR_ZERO, false });

		Flux_PipelineSpecification xShadowPipelineSpec(
			xVertexDesc,
			&s_xShadowShader,
			xBlendStates,
			true,
			true,
			DEPTH_COMPARE_FUNC_LESSEQUAL,
			DEPTHSTENCIL_FORMAT_D32_SFLOAT,
			true,
			false,
			{ 1,0 },
			{ 1,0 },
			Flux_Shadows::GetCSMTargetSetup(0),
			false,
			false
		);

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Static Meshes" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::RenderToGBuffer()
{
	if (!dbg_bEnable)
	{
		return;
	}

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	//#TO_TODO: fix up naming convention
	Flux_CommandBuffer& s_xCommandBuffer = Flux_DeferredShading::GetStaticMeshesCommandBuffer();
	s_xCommandBuffer.BeginRecording();
	#else

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget);
	#endif

	s_xCommandBuffer.SetPipeline(&s_xGBufferPipeline);

	std::vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_DRAW);

	for (Zenith_ModelComponent* pxModel : xModels)
	{
		//#TO_TODO: these 2 should probably be separate components
		if (pxModel->GetMeshGeometryAtIndex(0).GetNumBones())
		{
			continue;
		}
		for (uint32_t uMesh = 0; uMesh < pxModel->GetNumMeshEntires(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			s_xCommandBuffer.SetVertexBuffer(xMesh.GetVertexBuffer());
			s_xCommandBuffer.SetIndexBuffer(xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			s_xCommandBuffer.PushConstant(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			s_xCommandBuffer.BindTexture(xMaterial.GetDiffuse(), 0);
			s_xCommandBuffer.BindTexture(xMaterial.GetNormal(), 1);
			s_xCommandBuffer.BindTexture(xMaterial.GetRoughness(), 2);
			s_xCommandBuffer.BindTexture(xMaterial.GetMetallic(), 3);

			s_xCommandBuffer.DrawIndexed(xMesh.GetNumIndices());
		}
	}

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
	#else
	s_xCommandBuffer.EndRecording(RENDER_ORDER_OPAQUE_MESHES);
	#endif
}

void Flux_StaticMeshes::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf)
{

	std::vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	for (Zenith_ModelComponent* pxModel : xModels)
	{
		//#TO_TODO: these 2 should probably be separate components
		if (pxModel->GetMeshGeometryAtIndex(0).GetNumBones())
		{
			continue;
		}
		for (uint32_t uMesh = 0; uMesh < pxModel->GetNumMeshEntires(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			xCmdBuf.SetVertexBuffer(xMesh.GetVertexBuffer());
			xCmdBuf.SetIndexBuffer(xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			xCmdBuf.PushConstant(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			xCmdBuf.DrawIndexed(xMesh.GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_StaticMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}
