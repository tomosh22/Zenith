#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshes.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES, Flux_StaticMeshes::RenderToGBuffer, nullptr);

static Flux_CommandList g_xCommandList("Static Meshes");

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

DEBUGVAR bool dbg_bEnable = true;

void Flux_StaticMeshes::Initialise()
{

	s_xGBufferShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToGBuffer.vert", "StaticMeshes/Flux_StaticMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToShadowMap.vert", "StaticMeshes/Flux_StaticMeshes_ToShadowMap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
		xPipelineSpec.m_pxShader = &s_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xGBufferPipeline, xPipelineSpec);
	}

	{

		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_pxTargetSetup = &Flux_Shadows::GetCSMTargetSetup(0);
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		Flux_PipelineLayout& xLayout = xShadowPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Static Meshes" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::SubmitRenderToGBufferTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_StaticMeshes::WaitForRenderToGBufferTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_StaticMeshes::RenderToGBuffer(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	g_xCommandList.Reset(false);
	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(1);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		//#TO_TODO: these 2 should probably be separate components
		if (pxModel->GetMeshGeometryAtIndex(0).GetNumBones())
		{
			continue;
		}
		for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&xMesh.GetVertexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&xMaterial.GetDiffuse()->m_xSRV, 0);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&xMaterial.GetNormal()->m_xSRV, 1);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&xMaterial.GetRoughnessMetallic()->m_xSRV, 2);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&xMaterial.GetOcclusion()->m_xSRV, 3);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&xMaterial.GetEmissive()->m_xSRV, 4);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_OPAQUE_MESHES);
}

void Flux_StaticMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf)
{

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		//#TO_TODO: these 2 should probably be separate components
		if (pxModel->GetMeshGeometryAtIndex(0).GetNumBones())
		{
			continue;
		}
		for (uint32_t uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&xMesh.GetVertexBuffer());
			xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			xCmdBuf.AddCommand<Flux_CommandPushConstant>(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_StaticMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}
