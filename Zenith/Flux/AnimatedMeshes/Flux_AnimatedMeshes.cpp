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
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES, Flux_AnimatedMeshes::RenderToGBuffer, nullptr);

static Flux_CommandList g_xCommandList("Animated Meshes");

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

// Cached binding handles for named resource binding (populated at init from shader reflection)
// GBuffer shader bindings
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xScratchBufferBinding;  // For PushConstant calls
static Flux_BindingHandle s_xBonesBinding;
static Flux_BindingHandle s_xDiffuseTexBinding;
static Flux_BindingHandle s_xNormalTexBinding;
static Flux_BindingHandle s_xRoughnessMetallicTexBinding;
static Flux_BindingHandle s_xOcclusionTexBinding;
static Flux_BindingHandle s_xEmissiveTexBinding;

// Shadow shader bindings
static Flux_BindingHandle s_xShadowFrameConstantsBinding;
static Flux_BindingHandle s_xShadowScratchBufferBinding;
static Flux_BindingHandle s_xShadowBonesBinding;
static Flux_BindingHandle s_xShadowMatrixBinding;

DEBUGVAR bool dbg_bEnable = true;

void Flux_AnimatedMeshes::Initialise()
{
	s_xGBufferShader.Initialise("AnimatedMeshes/Flux_AnimatedMeshes_ToGBuffer.vert", "AnimatedMeshes/Flux_AnimatedMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("AnimatedMeshes/Flux_AnimatedMeshes_ToShadowMap.vert", "AnimatedMeshes/Flux_AnimatedMeshes_ToShadowMap.frag");

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
		xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xMRTTarget;
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
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_pxTargetSetup = &Flux_Shadows::GetCSMTargetSetup(0);
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		s_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

	// Cache binding handles from shader reflection for named resource binding
	// GBuffer shader bindings
	const Flux_ShaderReflection& xGBufferReflection = s_xGBufferShader.GetReflection();
	s_xFrameConstantsBinding = xGBufferReflection.GetBinding("FrameConstants");
	s_xScratchBufferBinding = xGBufferReflection.GetBinding("PushConstants");  // Scratch buffer for per-draw data
	s_xBonesBinding = xGBufferReflection.GetBinding("Bones");
	s_xDiffuseTexBinding = xGBufferReflection.GetBinding("g_xDiffuseTex");
	s_xNormalTexBinding = xGBufferReflection.GetBinding("g_xNormalTex");
	s_xRoughnessMetallicTexBinding = xGBufferReflection.GetBinding("g_xRoughnessMetallicTex");
	s_xOcclusionTexBinding = xGBufferReflection.GetBinding("g_xOcclusionTex");
	s_xEmissiveTexBinding = xGBufferReflection.GetBinding("g_xEmissiveTex");

	// Shadow shader bindings
	const Flux_ShaderReflection& xShadowReflection = s_xShadowShader.GetReflection();
	s_xShadowFrameConstantsBinding = xShadowReflection.GetBinding("FrameConstants");
	s_xShadowScratchBufferBinding = xShadowReflection.GetBinding("PushConstants");
	s_xShadowBonesBinding = xShadowReflection.GetBinding("Bones");
	s_xShadowMatrixBinding = xShadowReflection.GetBinding("ShadowMatrix");

	// Log binding info for debugging
	Zenith_Log(LOG_CATEGORY_ANIMATION, "AnimatedMeshes bindings: FrameConstants(set=%u,bind=%u) Bones(set=%u,bind=%u)",
		s_xFrameConstantsBinding.m_uSet, s_xFrameConstantsBinding.m_uBinding,
		s_xBonesBinding.m_uSet, s_xBonesBinding.m_uBinding);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Animated Meshes" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_ANIMATION, "Flux_AnimatedMeshes initialised");
}

void Flux_AnimatedMeshes::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);

	Zenith_Log(LOG_CATEGORY_ANIMATION, "Flux_AnimatedMeshes::Reset() - Reset command list");
}

void Flux_AnimatedMeshes::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_AnimatedMeshes::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_AnimatedMeshes::RenderToGBuffer(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	g_xCommandList.Reset(false);
	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(g_xCommandList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

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

			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModelComponent->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial)
			{
				// Skip rendering if no material - fallback handled during model instance creation
				continue;
			}

			// Build and push material constants (128 bytes) - uses scratch buffer in set 1
			MaterialPushConstants xPushConstants;
			BuildMaterialPushConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.PushConstant(s_xScratchBufferBinding, &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and material textures (named bindings)
			xBinder.BindCBV(s_xBonesBinding, &xBoneBuffer.GetCBV());
			xBinder.BindSRV(s_xDiffuseTexBinding, &pxMaterial->GetDiffuseTexture()->m_xSRV);
			xBinder.BindSRV(s_xNormalTexBinding, &pxMaterial->GetNormalTexture()->m_xSRV);
			xBinder.BindSRV(s_xRoughnessMetallicTexBinding, &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
			xBinder.BindSRV(s_xOcclusionTexBinding, &pxMaterial->GetOcclusionTexture()->m_xSRV);
			xBinder.BindSRV(s_xEmissiveTexBinding, &pxMaterial->GetEmissiveTexture()->m_xSRV);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKINNED_MESHES);
}

void Flux_AnimatedMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xShadowFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

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

			xBinder.PushConstant(s_xShadowScratchBufferBinding, &xModelMatrix, sizeof(xModelMatrix));

			// Bind set 1: bone buffer and shadow matrix (named bindings)
			xBinder.BindCBV(s_xShadowBonesBinding, &xBoneBuffer.GetCBV());
			xBinder.BindCBV(s_xShadowMatrixBinding, &xShadowMatrixBuffer.GetCBV());

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_AnimatedMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}