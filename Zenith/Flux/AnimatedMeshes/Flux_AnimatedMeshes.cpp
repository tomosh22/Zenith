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
#include "AssetHandling/Zenith_AssetHandler.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Material Push Constants structure - matches shader layout (128 bytes total)
struct MaterialPushConstants
{
	Zenith_Maths::Matrix4 m_xModelMatrix;       // 64 bytes - Model transform matrix
	Zenith_Maths::Vector4 m_xBaseColor;         // 16 bytes - RGBA base color multiplier
	Zenith_Maths::Vector4 m_xMaterialParams;    // 16 bytes - (metallic, roughness, alphaCutoff, occlusionStrength)
	Zenith_Maths::Vector4 m_xUVParams;          // 16 bytes - (tilingX, tilingY, offsetX, offsetY)
	Zenith_Maths::Vector4 m_xEmissiveParams;    // 16 bytes - (R, G, B, intensity)
};  // Total: 128 bytes

static_assert(sizeof(MaterialPushConstants) == 128, "MaterialPushConstants must be exactly 128 bytes");

// Helper to build MaterialPushConstants from a material asset
static void BuildMaterialPushConstants(MaterialPushConstants& xOut,
									   const Zenith_Maths::Matrix4& xModelMatrix,
									   const Flux_MaterialAsset* pxMaterial)
{
	xOut.m_xModelMatrix = xModelMatrix;

	if (pxMaterial)
	{
		xOut.m_xBaseColor = pxMaterial->GetBaseColor();
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(
			pxMaterial->GetMetallic(),
			pxMaterial->GetRoughness(),
			pxMaterial->GetAlphaCutoff(),
			pxMaterial->GetOcclusionStrength()
		);
		const Zenith_Maths::Vector2& xTiling = pxMaterial->GetUVTiling();
		const Zenith_Maths::Vector2& xOffset = pxMaterial->GetUVOffset();
		xOut.m_xUVParams = Zenith_Maths::Vector4(xTiling.x, xTiling.y, xOffset.x, xOffset.y);
		const Zenith_Maths::Vector3& xEmissive = pxMaterial->GetEmissiveColor();
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(xEmissive.x, xEmissive.y, xEmissive.z, pxMaterial->GetEmissiveIntensity());
	}
	else
	{
		// Default values for missing material
		xOut.m_xBaseColor = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		xOut.m_xMaterialParams = Zenith_Maths::Vector4(0.0f, 0.5f, 0.5f, 1.0f);  // metallic=0, roughness=0.5, alphaCutoff=0.5, occlusionStrength=1
		xOut.m_xUVParams = Zenith_Maths::Vector4(1.0f, 1.0f, 0.0f, 0.0f);  // tiling=1, offset=0
		xOut.m_xEmissiveParams = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES, Flux_AnimatedMeshes::RenderToGBuffer, nullptr);

static Flux_CommandList g_xCommandList("Animated Meshes");

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

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

		Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
		xLayout.m_uNumDescriptorSets = 2;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_TEXTURE;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_TEXTURE;

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
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

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

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

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

			Flux_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial)
			{
				pxMaterial = Flux_Graphics::s_pxBlankMaterial;
			}
			Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

			// Bind set 0: frame constants + push constants (scratch buffer)
			g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
			g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);

			// Build and push material constants (128 bytes)
			MaterialPushConstants xPushConstants;
			BuildMaterialPushConstants(xPushConstants, xModelMatrix, pxMaterial);
			g_xCommandList.AddCommand<Flux_CommandPushConstant>(&xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and material textures
			g_xCommandList.AddCommand<Flux_CommandBeginBind>(1);
			g_xCommandList.AddCommand<Flux_CommandBindCBV>(&xBoneBuffer.GetCBV(), 0);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&pxMaterial->GetDiffuseTexture()->m_xSRV, 1);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&pxMaterial->GetNormalTexture()->m_xSRV, 2);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&pxMaterial->GetRoughnessMetallicTexture()->m_xSRV, 3);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&pxMaterial->GetOcclusionTexture()->m_xSRV, 4);
			g_xCommandList.AddCommand<Flux_CommandBindSRV>(&pxMaterial->GetEmissiveTexture()->m_xSRV, 5);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_SKINNED_MESHES);
}

void Flux_AnimatedMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf)
{
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);

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
			xCmdBuf.AddCommand<Flux_CommandBeginBind>(0);  // Required for scratch buffer system
			xCmdBuf.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
			xCmdBuf.AddCommand<Flux_CommandPushConstant>(&xModelMatrix, sizeof(xModelMatrix));

			xCmdBuf.AddCommand<Flux_CommandBeginBind>(1);  // Switch to set 1 for bone buffer
			xCmdBuf.AddCommand<Flux_CommandBindCBV>(&xBoneBuffer.GetCBV(), 0);

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_AnimatedMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}