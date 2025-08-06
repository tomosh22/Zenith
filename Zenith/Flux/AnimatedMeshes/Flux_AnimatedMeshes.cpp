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

#ifndef ZENITH_MERGE_GBUFFER_PASSES
static Flux_CommandBuffer s_xCommandBuffer;
#endif

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

DEBUGVAR bool dbg_bEnable = true;

void Flux_AnimatedMeshes::Initialise()
{
	#ifndef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.Initialise();
	#endif

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
#if 0
	(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		true,
		true,
		DEPTH_COMPARE_FUNC_LESSEQUAL,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{ 1,0 },
		{ 1,4 },
		Flux_Graphics::s_xMRTTarget,
		false
	);
#endif

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

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	//#TO_TODO: fix up naming convention
	Flux_CommandBuffer& s_xCommandBuffer = Flux_DeferredShading::GetAnimatedMeshesCommandBuffer();

	s_xCommandBuffer.BeginRecording();
	#else
	s_xCommandBuffer.BeginRecording();
	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xMRTTarget);
	#endif

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

	s_xCommandBuffer.BeginBind(0);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);

	s_xCommandBuffer.BeginBind(1);

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
			s_xCommandBuffer.SetVertexBuffer(xMesh.GetVertexBuffer());
			s_xCommandBuffer.SetIndexBuffer(xMesh.GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);
			s_xCommandBuffer.PushConstant(&xModelMatrix, sizeof(xModelMatrix));
			const Flux_Material& xMaterial = pxModel->GetMaterialAtIndex(uMesh);

			s_xCommandBuffer.BindBuffer(&xMesh.m_pxAnimation->m_xBoneBuffer.GetBuffer(), 0);

			s_xCommandBuffer.BindTexture(xMaterial.GetDiffuse(), 1);
			s_xCommandBuffer.BindTexture(xMaterial.GetNormal(), 2);
			s_xCommandBuffer.BindTexture(xMaterial.GetRoughness(), 3);
			s_xCommandBuffer.BindTexture(xMaterial.GetMetallic(), 4);

			s_xCommandBuffer.DrawIndexed(xMesh.GetNumIndices());
		}
	}

	#ifdef ZENITH_MERGE_GBUFFER_PASSES
	s_xCommandBuffer.EndRecording(RENDER_ORDER_GBUFFER);
	#else
	s_xCommandBuffer.EndRecording(RENDER_ORDER_OPAQUE_MESHES);
	#endif
}