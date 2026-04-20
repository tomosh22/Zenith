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

static Flux_Shader s_xGBufferShader;
static Flux_Pipeline s_xGBufferPipeline;

static Flux_Shader s_xShadowShader;
static Flux_Pipeline s_xShadowPipeline;

DEBUGVAR bool dbg_bEnable = true;

void Flux_StaticMeshes::Initialise()
{

	s_xGBufferShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToGBuffer.vert", "StaticMeshes/Flux_StaticMeshes_ToGBuffer.frag");
	s_xShadowShader.Initialise("StaticMeshes/Flux_StaticMeshes_ToShadowmap.vert", "StaticMeshes/Flux_StaticMeshes_ToShadowmap.frag");

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

		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &s_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		s_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xShadowPipeline, xShadowPipelineSpec);
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Static Meshes" }, dbg_bEnable);
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_StaticMeshes initialised");
}

void Flux_StaticMeshes::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Static Meshes GBuffer", ExecuteGBuffer)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(Flux_Graphics::GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

// Animated meshes (skeleton + at least one skinned mesh instance) are rendered
// by Flux_AnimatedMeshes; this renderer skips them. Models with a skeleton but
// NO skinning data still render here as static meshes.
static bool IsAnimatedSkinnedModel(const Flux_ModelInstance& xModelInstance)
{
	if (!xModelInstance.HasSkeleton()) return false;
	for (uint32_t u = 0; u < xModelInstance.GetNumMeshes(); u++)
	{
		if (xModelInstance.GetSkinnedMeshInstance(u) != nullptr) return true;
	}
	return false;
}

// Per-mesh draw used by both the new model-instance and legacy mesh-entry
// branches. Bind material constants + SRVs, then emit the indexed draw.
static void DrawStaticMesh(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Maths::Matrix4& xModelMatrix,
	Zenith_MaterialAsset* pxMaterial,
	u_int uIndexCount)
{
	MaterialDrawConstants xPushConstants;
	BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
	xBinder.BindDrawConstants(s_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

	xBinder.BindSRV(s_xGBufferShader, "g_xDiffuseTex", &pxMaterial->GetDiffuseTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xNormalTex", &pxMaterial->GetNormalTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetOcclusionTexture()->m_xSRV);
	xBinder.BindSRV(s_xGBufferShader, "g_xEmissiveTex", &pxMaterial->GetEmissiveTexture()->m_xSRV);

	pxCmdList->AddCommand<Flux_CommandDrawIndexed>(uIndexCount);
}

static void RenderModelInstanceMeshes(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	Zenith_ModelComponent* pxModel, Flux_ModelInstance* pxModelInstance)
{
	// One-shot debug log: log the first static-model layout we render so we can
	// diagnose missing/empty meshes by looking at the first frame's logs.
	static bool ls_bLoggedOnce = false;
	if (!ls_bLoggedOnce)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes] Rendering static model - meshes: %u", pxModelInstance->GetNumMeshes());
		for (uint32_t uDbg = 0; uDbg < pxModelInstance->GetNumMeshes(); uDbg++)
		{
			Flux_MeshInstance* pxDbgMesh = pxModelInstance->GetMeshInstance(uDbg);
			if (pxDbgMesh)
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes]   Mesh %u: %u verts, %u indices",
					uDbg, pxDbgMesh->GetNumVerts(), pxDbgMesh->GetNumIndices());
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "[StaticMeshes]   Mesh %u: NULL", uDbg);
			}
		}
		ls_bLoggedOnce = true;
	}

	Zenith_Maths::Matrix4 xModelMatrix;
	pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

	for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
	{
		Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
		if (!pxMeshInstance) continue;

		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

		Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
		if (!pxMaterial) pxMaterial = Flux_Graphics::s_pxBlankMaterial;
		Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

		DrawStaticMesh(pxCmdList, xBinder, xModelMatrix, pxMaterial, pxMeshInstance->GetNumIndices());
	}
}

// Legacy procedural-mesh branch (Games/ procedural meshes that haven't moved
// to the model-instance system yet).
static void RenderLegacyMeshEntries(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	Zenith_ModelComponent* pxModel)
{
	if (!pxModel->GetNumMeshEntries()) return;

	Zenith_Maths::Matrix4 xModelMatrix;
	pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

	for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
	{
		const Flux_MeshGeometry& xMesh = pxModel->GetMeshGeometryAtIndex(uMesh);
		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&xMesh.GetVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&xMesh.GetIndexBuffer());

		Zenith_MaterialAsset& xMaterial = *pxModel->GetMaterialAtIndex(uMesh);
		DrawStaticMesh(pxCmdList, xBinder, xModelMatrix, &xMaterial, xMesh.GetNumIndices());
	}
}

void Flux_StaticMeshes::ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!dbg_bEnable) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&s_xGBufferPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	xBinder.BindCBV(s_xGBufferShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();

		// New model-instance system: skip skinned-animated models (rendered
		// by Flux_AnimatedMeshes), draw everything else here.
		if (Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance())
		{
			if (IsAnimatedSkinnedModel(*pxModelInstance)) continue;
			RenderModelInstanceMeshes(pxCmdList, xBinder, pxModel, pxModelInstance);
			continue;
		}

		// Fallback: legacy procedural mesh entries.
		// #TODO: these 2 should probably be separate components.
		RenderLegacyMeshEntries(pxCmdList, xBinder, pxModel);
	}
}

void Flux_StaticMeshes::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Bind FrameConstants once per command list (set 0 - per-frame data)
	xBinder.BindCBV(s_xShadowShader, "FrameConstants", &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

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

				xBinder.BindDrawConstants(s_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));
				xBinder.BindCBV(s_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());
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

			xBinder.BindDrawConstants(s_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));
			xBinder.BindCBV(s_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());
			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(xMesh.GetNumIndices());
		}
	}
}

Flux_Pipeline& Flux_StaticMeshes::GetShadowPipeline()
{
	return s_xShadowPipeline;
}
