#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Core/Zenith_Engine.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_StaticMeshesImpl held by Zenith_Engine.



static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*);

void Flux_StaticMeshesImpl::BuildPipelines()
{
	g_xEngine.StaticMeshes().m_xGBufferShader.Initialise(FluxShaderProgram::StaticMesh_ToGBuffer);
	g_xEngine.StaticMeshes().m_xShadowShader.Initialise(FluxShaderProgram::StaticMesh_ToShadowmap);

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
		xPipelineSpec.m_pxShader = &g_xEngine.StaticMeshes().m_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		g_xEngine.StaticMeshes().m_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(g_xEngine.StaticMeshes().m_xGBufferPipeline, xPipelineSpec);
	}

	{

		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &g_xEngine.StaticMeshes().m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		g_xEngine.StaticMeshes().m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(g_xEngine.StaticMeshes().m_xShadowPipeline, xShadowPipelineSpec);
	}
}

void Flux_StaticMeshesImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::StaticMesh_ToGBuffer,
		FluxShaderProgram::StaticMesh_ToShadowmap,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.StaticMeshes().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_StaticMeshes initialised");
}

void Flux_StaticMeshesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Static Meshes GBuffer", ExecuteGBuffer)
		.Writes(g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(g_xEngine.FluxGraphics().GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(g_xEngine.FluxGraphics().GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
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

// Per-mesh draw helper. Binds material constants + SRVs, then emits the
// indexed draw.
static void DrawStaticMesh(Flux_CommandList* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Maths::Matrix4& xModelMatrix,
	Zenith_MaterialAsset* pxMaterial,
	u_int uIndexCount)
{
	MaterialDrawConstants xPushConstants;
	BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
	xBinder.BindDrawConstants(g_xEngine.StaticMeshes().m_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

	xBinder.BindSRV(g_xEngine.StaticMeshes().m_xGBufferShader, "g_xDiffuseTex", &pxMaterial->GetDiffuseTexture()->m_xSRV);
	xBinder.BindSRV(g_xEngine.StaticMeshes().m_xGBufferShader, "g_xNormalTex", &pxMaterial->GetNormalTexture()->m_xSRV);
	xBinder.BindSRV(g_xEngine.StaticMeshes().m_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
	xBinder.BindSRV(g_xEngine.StaticMeshes().m_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetOcclusionTexture()->m_xSRV);
	xBinder.BindSRV(g_xEngine.StaticMeshes().m_xGBufferShader, "g_xEmissiveTex", &pxMaterial->GetEmissiveTexture()->m_xSRV);

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
		if (!pxMaterial) pxMaterial = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
		Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

		DrawStaticMesh(pxCmdList, xBinder, xModelMatrix, pxMaterial, pxMeshInstance->GetNumIndices());
	}
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bStaticMeshesEnabled) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.StaticMeshes().m_xGBufferPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	xBinder.BindCBV(g_xEngine.StaticMeshes().m_xGBufferShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	g_xEngine.SceneRegistry().GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
		if (!pxModelInstance) continue;

		// Skinned-animated models are rendered by Flux_AnimatedMeshes.
		if (IsAnimatedSkinnedModel(*pxModelInstance)) continue;

		RenderModelInstanceMeshes(pxCmdList, xBinder, pxModel, pxModelInstance);
	}
}

void Flux_StaticMeshesImpl::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	Flux_ShaderBinder xBinder(xCmdBuf);
	// Shadow pass binds only DrawConstants + ShadowMatrix. Slang reflection
	// drops bindings that the shader doesn't actually reference (this
	// program imports Common.Frame but reads no FrameConstants field), so
	// FrameConstants doesn't appear in the reflected layout — binding it
	// here would fail the name lookup.

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	g_xEngine.SceneRegistry().GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
		if (!pxModelInstance) continue;

		// Skinned-animated models are rendered by Flux_AnimatedMeshes.
		if (IsAnimatedSkinnedModel(*pxModelInstance)) continue;

		Zenith_Maths::Matrix4 xModelMatrix;
		pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xModelMatrix);

		for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
		{
			Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
			if (!pxMeshInstance) continue;

			xCmdBuf.AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
			xCmdBuf.AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

			xBinder.BindDrawConstants(g_xEngine.StaticMeshes().m_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));
			xBinder.BindCBV(g_xEngine.StaticMeshes().m_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());
			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

