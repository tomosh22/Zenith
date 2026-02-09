#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshes.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES, Flux_StaticMeshes::RenderToGBuffer, nullptr);

static Flux_CommandList g_xCommandList("Static Meshes");

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

// Cached binding handles for named resource binding (populated at init from shader reflection)
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xScratchBufferBinding;  // For PushConstant calls
static Flux_BindingHandle s_xDiffuseTexBinding;
static Flux_BindingHandle s_xNormalTexBinding;
static Flux_BindingHandle s_xRoughnessMetallicTexBinding;
static Flux_BindingHandle s_xOcclusionTexBinding;
static Flux_BindingHandle s_xEmissiveTexBinding;

// Shadow pass binding handles
static Flux_BindingHandle s_xShadowFrameConstantsBinding;
static Flux_BindingHandle s_xShadowScratchBufferBinding;
static Flux_BindingHandle s_xShadowMatrixBinding;

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
		// Set 0: Per-frame (FrameConstants only - bound once per command list)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		// Set 1: Per-draw (scratch buffer + textures)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
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
		// Set 0: Per-frame (FrameConstants only)
		xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
		// Set 1: Per-draw (scratch buffer + shadow matrix)
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Scratch buffer for push constants
		xLayout.m_axDescriptorSetLayouts[1].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Shadow matrix

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

	// Cache binding handles from shader reflection for named resource binding
	// GBuffer shader bindings
	const Flux_ShaderReflection& xGBufferReflection = s_xGBufferShader.GetReflection();
	s_xFrameConstantsBinding = xGBufferReflection.GetBinding("FrameConstants");
	s_xScratchBufferBinding = xGBufferReflection.GetBinding("PushConstants");  // Scratch buffer for per-draw data
	s_xDiffuseTexBinding = xGBufferReflection.GetBinding("g_xDiffuseTex");
	s_xNormalTexBinding = xGBufferReflection.GetBinding("g_xNormalTex");
	s_xRoughnessMetallicTexBinding = xGBufferReflection.GetBinding("g_xRoughnessMetallicTex");
	s_xOcclusionTexBinding = xGBufferReflection.GetBinding("g_xOcclusionTex");
	s_xEmissiveTexBinding = xGBufferReflection.GetBinding("g_xEmissiveTex");

	// Shadow shader bindings
	const Flux_ShaderReflection& xShadowReflection = s_xShadowShader.GetReflection();
	s_xShadowFrameConstantsBinding = xShadowReflection.GetBinding("FrameConstants");
	s_xShadowScratchBufferBinding = xShadowReflection.GetBinding("PushConstants");
	s_xShadowMatrixBinding = xShadowReflection.GetBinding("ShadowMatrix");

	// Log binding info for debugging
	Zenith_Log(LOG_CATEGORY_MESH, "StaticMeshes bindings: FrameConstants(set=%u,bind=%u) DiffuseTex(set=%u,bind=%u)",
		s_xFrameConstantsBinding.m_uSet, s_xFrameConstantsBinding.m_uBinding,
		s_xDiffuseTexBinding.m_uSet, s_xDiffuseTexBinding.m_uBinding);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Static Meshes" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_StaticMeshes::Reset() - Reset command list");
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

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(g_xCommandList);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	static bool s_bLoggedOnce = false;
	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();

		// New model instance system - only render static meshes (no skeleton)
		// Animated meshes with skeletons are rendered by Flux_AnimatedMeshes
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
		if (pxModelInstance)
		{
			// Check if this model should be rendered by the animated mesh renderer
			// A model is animated if it has a skeleton AND at least one skinned mesh instance
			bool bHasSkinnedMeshes = false;
			if (pxModelInstance->HasSkeleton())
			{
				for (uint32_t uCheck = 0; uCheck < pxModelInstance->GetNumMeshes(); uCheck++)
				{
					if (pxModelInstance->GetSkinnedMeshInstance(uCheck) != nullptr)
					{
						bHasSkinnedMeshes = true;
						break;
					}
				}
			}

			// Skip models that have a skeleton AND skinned mesh data - they are rendered by Flux_AnimatedMeshes
			// Models with a skeleton but NO skinning data are rendered here using static mesh instances
			if (pxModelInstance->HasSkeleton() && bHasSkinnedMeshes)
			{
				continue;
			}

			if (!s_bLoggedOnce)
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes] Rendering static model - meshes: %u", pxModelInstance->GetNumMeshes());
				for (uint32_t uDbg = 0; uDbg < pxModelInstance->GetNumMeshes(); uDbg++)
				{
					Flux_MeshInstance* pxDbgMesh = pxModelInstance->GetMeshInstance(uDbg);
					if (pxDbgMesh)
					{
						Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes]   Mesh %u: %u verts, %u indices", uDbg, pxDbgMesh->GetNumVerts(), pxDbgMesh->GetNumIndices());
					}
					else
					{
						Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes]   Mesh %u: NULL", uDbg);
					}
				}
				s_bLoggedOnce = true;
			}

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

			for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
			{
				Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
				if (!pxMeshInstance)
				{
					continue;
				}

				g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
				g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

				Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
				if (!pxMaterial)
				{
					pxMaterial = Flux_Graphics::s_pxBlankMaterial;
				}
				Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

				// Build and push material constants (128 bytes) - uses scratch buffer in set 1
				MaterialPushConstants xPushConstants;
				BuildMaterialPushConstants(xPushConstants, xModelMatrix, pxMaterial);
				xBinder.PushConstant(s_xScratchBufferBinding, &xPushConstants, sizeof(xPushConstants));

				// Bind set 1: material textures (named bindings)
				xBinder.BindSRV(s_xDiffuseTexBinding, &pxMaterial->GetDiffuseTexture()->m_xSRV);
				xBinder.BindSRV(s_xNormalTexBinding, &pxMaterial->GetNormalTexture()->m_xSRV);
				xBinder.BindSRV(s_xRoughnessMetallicTexBinding, &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
				xBinder.BindSRV(s_xOcclusionTexBinding, &pxMaterial->GetOcclusionTexture()->m_xSRV);
				xBinder.BindSRV(s_xEmissiveTexBinding, &pxMaterial->GetEmissiveTexture()->m_xSRV);

				g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
			}
			continue;
		}

		// Legacy mesh entry system (procedural meshes from Games/)
		//#TO_TODO: these 2 should probably be separate components
		if (!pxModel->GetNumMeshEntries())
		{
			continue;
		}

		Zenith_Maths::Matrix4 xModelMatrix;
		pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

		for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
		{
			const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
			g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&xMesh.GetVertexBuffer());
			g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&xMesh.GetIndexBuffer());

			Zenith_MaterialAsset& xMaterial = *pxModel->GetMaterialAtIndex(uMesh);

			// Build and push material constants (128 bytes) - uses scratch buffer in set 1
			MaterialPushConstants xPushConstants;
			BuildMaterialPushConstants(xPushConstants, xModelMatrix, &xMaterial);
			xBinder.PushConstant(s_xScratchBufferBinding, &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: material textures (named bindings)
			xBinder.BindSRV(s_xDiffuseTexBinding, &xMaterial.GetDiffuseTexture()->m_xSRV);
			xBinder.BindSRV(s_xNormalTexBinding, &xMaterial.GetNormalTexture()->m_xSRV);
			xBinder.BindSRV(s_xRoughnessMetallicTexBinding, &xMaterial.GetRoughnessMetallicTexture()->m_xSRV);
			xBinder.BindSRV(s_xOcclusionTexBinding, &xMaterial.GetOcclusionTexture()->m_xSRV);
			xBinder.BindSRV(s_xEmissiveTexBinding, &xMaterial.GetEmissiveTexture()->m_xSRV);

			g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xMRTTarget, RENDER_ORDER_OPAQUE_MESHES);
}

void Flux_StaticMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xShadowFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();

		// New model instance system - only render static meshes (no skeleton)
		// Animated meshes with skeletons are rendered by Flux_AnimatedMeshes
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
		if (pxModelInstance)
		{
			// Check if this model should be rendered by the animated mesh renderer
			// A model is animated if it has a skeleton AND at least one skinned mesh instance
			bool bHasSkinnedMeshes = false;
			if (pxModelInstance->HasSkeleton())
			{
				for (uint32_t uCheck = 0; uCheck < pxModelInstance->GetNumMeshes(); uCheck++)
				{
					if (pxModelInstance->GetSkinnedMeshInstance(uCheck) != nullptr)
					{
						bHasSkinnedMeshes = true;
						break;
					}
				}
			}

			// Skip models that have a skeleton AND skinned mesh data - they are rendered by Flux_AnimatedMeshes
			// Models with a skeleton but NO skinning data are rendered here using static mesh instances
			if (pxModelInstance->HasSkeleton() && bHasSkinnedMeshes)
			{
				continue;
			}

			Zenith_Maths::Matrix4 xModelMatrix;
			pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

			for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
			{
				Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
				if (!pxMeshInstance)
				{
					continue;
				}

				xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
				xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

				xBinder.PushConstant(s_xShadowScratchBufferBinding, &xModelMatrix, sizeof(xModelMatrix));
				xBinder.BindCBV(s_xShadowMatrixBinding, &xShadowMatrixBuffer.GetCBV());
				xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
			}
			continue;
		}

		// Legacy mesh entry system
		//#TO_TODO: these 2 should probably be separate components
		if (!pxModel->GetNumMeshEntries())
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

			xBinder.PushConstant(s_xShadowScratchBufferBinding, &xModelMatrix, sizeof(xModelMatrix));
			xBinder.BindCBV(s_xShadowMatrixBinding, &xShadowMatrixBuffer.GetCBV());
			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_StaticMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}
