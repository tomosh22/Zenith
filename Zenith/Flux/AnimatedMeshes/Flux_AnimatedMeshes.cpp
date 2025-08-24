#include "Zenith.h"

#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"

static Flux_CommandList g_xCommandList("Animated Meshes");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;

void Flux_AnimatedMeshes::Initialise()
{
	s_xShader.Initialise("AnimatedMeshes/Flux_AnimatedMeshes.vert", "AnimatedMeshes/Flux_AnimatedMeshes.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);

	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT4);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 2;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
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

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Animated Meshes" }, dbg_bEnable);
#endif

	Zenith_Log("Flux_AnimatedMeshes initialised");
}

void Flux_AnimatedMeshes::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	g_xCommandList.Reset();
	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(1);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		//#TO_TODO: these 2 should probably be separate components
		if (!pxModel->GetMeshGeometryAtIndex(0).GetNumBones())
		{
			continue;
		}
		for (uint32_t uMesh = 0; uMesh < pxModel->GetNumMeshEntires(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&xMesh.GetVertexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&xMesh.m_pxAnimation->m_xBoneBuffer.GetBuffer(), 0);

			g_xCommandList.AddCommand<Flux_CommandBindTexture>(xMaterial.GetDiffuse(), 1);
			g_xCommandList.AddCommand<Flux_CommandBindTexture>(xMaterial.GetNormal(), 2);
			g_xCommandList.AddCommand<Flux_CommandBindTexture>(xMaterial.GetRoughness(), 3);
			g_xCommandList.AddCommand<Flux_CommandBindTexture>(xMaterial.GetMetallic(), 4);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}

	Flux::SubmitCommandList(&g_xCommandList, RENDER_ORDER_SKINNED_MESHES);
}