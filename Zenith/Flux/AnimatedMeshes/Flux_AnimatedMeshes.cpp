#include "Zenith.h"

#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

void Flux_AnimatedMeshes::Initialise()
{
	s_xGBufferShader.Initialise("AnimatedMeshes/Flux_AnimatedMeshes_ToGBuffer.vert", "AnimatedMeshes/Flux_AnimatedMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("AnimatedMeshes/Flux_AnimatedMeshes_ToShadowmap.vert", "AnimatedMeshes/Flux_AnimatedMeshes_ToShadowmap.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);

	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT4);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	{
		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE] = MRT_FORMAT_DIFFUSE;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL] = MRT_FORMAT_MATERIAL;
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &s_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		s_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(s_xGBufferPipeline, xPipelineSpec);
	}

	{
		// CSMs are depth-only (GetCSMTargetSetup returns null colour, uNumColour=0).
		// Format is the shared CSM_FORMAT constant so we don't need the transient
		// attachment to exist yet.
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		s_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

	Zenith_Log(LOG_CATEGORY_ANIMATION, "Flux_AnimatedMeshes initialised");
}

void Flux_AnimatedMeshes::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Animated Meshes GBuffer", ExecuteGBuffer)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

void Flux_AnimatedMeshes::ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bAnimatedMeshesEnabled)
	{
		return;
	}

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xGBufferShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	static bool s_bLoggedOnce = false;
	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModelComponent = xIt.GetData();

		// Skip components without a model or skeleton (not animated)
		if (!pxModelComponent->HasModel() || !pxModelComponent->HasSkeleton())
		{
			continue;
		}

		Flux_ModelInstance* pxModelInstance = pxModelComponent->GetModelInstance();
		Flux_SkeletonInstance* pxSkeleton = pxModelComponent->GetSkeletonInstance();

		if (!pxModelInstance || !pxSkeleton)
		{
			continue;
		}

		if (!s_bLoggedOnce)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "Rendering animated model - meshes: %u", pxModelInstance->GetNumMeshes());
			for (uint32_t uDbg = 0; uDbg < pxModelInstance->GetNumMeshes(); uDbg++)
			{
				Flux_MeshInstance* pxDbgMesh = pxModelInstance->GetSkinnedMeshInstance(uDbg);
				if (pxDbgMesh)
				{
					Zenith_Log(LOG_CATEGORY_RENDERER, "  SkinnedMesh %u: %u verts, %u indices", uDbg, pxDbgMesh->GetNumVerts(), pxDbgMesh->GetNumIndices());
				}
				else
				{
					Zenith_Log(LOG_CATEGORY_RENDERER, "  SkinnedMesh %u: NULL (no skinning data)", uDbg);
				}
			}
			s_bLoggedOnce = true;
		}

		// Get bone buffer from skeleton instance
		const Flux_DynamicConstantBuffer& xBoneBuffer = pxSkeleton->GetBoneBuffer();

		for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
		{
			// Use skinned mesh instance (104-byte format with bone indices/weights)
			Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetSkinnedMeshInstance(uMesh);
			if (!pxMeshInstance)
			{
				continue;
			}

			pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
			pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModelComponent->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial)
			{
				// Skip rendering if no material - fallback handled during model instance creation
				continue;
			}

			// Build and push material constants (128 bytes) - uses scratch buffer in set 1
			MaterialDrawConstants xPushConstants;
			BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.BindDrawConstants(s_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and material textures (named bindings)
			xBinder.BindCBV(s_xGBufferShader, "Bones", &xBoneBuffer.GetCBV());
			xBinder.BindSRV(s_xGBufferShader, "g_xDiffuseTex", &pxMaterial->GetDiffuseTexture()->m_xSRV);
			xBinder.BindSRV(s_xGBufferShader, "g_xNormalTex", &pxMaterial->GetNormalTexture()->m_xSRV);
			xBinder.BindSRV(s_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
			xBinder.BindSRV(s_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetOcclusionTexture()->m_xSRV);
			xBinder.BindSRV(s_xGBufferShader, "g_xEmissiveTex", &pxMaterial->GetEmissiveTexture()->m_xSRV);

			pxCmdList->AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

void Flux_AnimatedMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xShadowShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModelComponent = xIt.GetData();

		// Skip components without a model or skeleton (not animated)
		if (!pxModelComponent->HasModel() || !pxModelComponent->HasSkeleton())
		{
			continue;
		}

		Flux_ModelInstance* pxModelInstance = pxModelComponent->GetModelInstance();
		Flux_SkeletonInstance* pxSkeleton = pxModelComponent->GetSkeletonInstance();

		if (!pxModelInstance || !pxSkeleton)
		{
			continue;
		}

		// Get bone buffer from skeleton instance
		const Flux_DynamicConstantBuffer& xBoneBuffer = pxSkeleton->GetBoneBuffer();

		for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
		{
			// Use skinned mesh instance (104-byte format with bone indices/weights)
			Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetSkinnedMeshInstance(uMesh);
			if (!pxMeshInstance)
			{
				continue;
			}

			xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
			xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModelComponent->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

			xBinder.BindDrawConstants(s_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));

			// Bind set 1: bone buffer and shadow matrix (named bindings)
			xBinder.BindCBV(s_xShadowShader, "Bones", &xBoneBuffer.GetCBV());
			xBinder.BindCBV(s_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_AnimatedMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}