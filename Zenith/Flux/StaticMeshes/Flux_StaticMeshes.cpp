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
#include "EntityComponent/Zenith_SceneSystem.h"
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
static bool IsAnimatedSkinnedModel(const Flux_ModelInstance& xModelInstance);

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

void Flux_StaticMeshesImpl::Initialise(Flux_GraphicsImpl& xGraphics)
{
	// Wave-15 DI seam: store the injected cross-subsystem dep. The FluxGraphics
	// reach-ins (in SetupRenderGraph, ExecuteGBuffer and RenderModelInstanceMeshes)
	// route through this instead of g_xEngine.FluxGraphics().
	m_pxGraphics = &xGraphics;

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

void Flux_StaticMeshesImpl::Shutdown()
{
	// Pipelines reference their shaders, so destroy pipelines first.
	g_xEngine.StaticMeshes().m_xGBufferPipeline.Reset();
	g_xEngine.StaticMeshes().m_xShadowPipeline.Reset();
	g_xEngine.StaticMeshes().m_xGBufferShader.Reset();
	g_xEngine.StaticMeshes().m_xShadowShader.Reset();

	// Drop the injected dep so the instance returns to a clean default state.
	m_pxGraphics = nullptr;
}

void Flux_StaticMeshesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// The gather Prepare is hung on the GBuffer pass (Terrain pattern). It runs
	// on the main thread before any record callback, so the packet it builds is
	// available to BOTH this pass's ExecuteGBuffer and all 4 shadow cascades
	// (which call RenderToShadowMap from Flux_Shadows). Pass-registration order
	// is irrelevant to Prepare timing — CallPrepareCallbacks runs every enabled
	// pass's Prepare before RecordCommandLists dispatches any record task.
	xGraph.AddPass("Static Meshes GBuffer", ExecuteGBuffer)
		.Prepare([](void* p){ g_xEngine.StaticMeshes().GatherDrawPacket(p); })
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

// Prepare callback (main thread): walk every Zenith_ModelComponent in all
// scenes once, skip skinned-animated models (rendered by Flux_AnimatedMeshes)
// and components without a model instance, resolve the model matrix from the
// live Zenith_TransformComponent here, and store everything the worker-thread
// record path needs. This is the single shared source for the GBuffer pass and
// all shadow cascades — it removes every live-ECS read from those worker paths.
void Flux_StaticMeshesImpl::GatherDrawPacket(void*)
{
	Zenith_Vector<Flux_StaticMeshDrawItem>& xPacket = g_xEngine.StaticMeshes().m_xDrawPacket;
	xPacket.Clear();

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	g_xEngine.Scenes().GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
		if (!pxModelInstance) continue;

		// Skinned-animated models are rendered by Flux_AnimatedMeshes.
		if (IsAnimatedSkinnedModel(*pxModelInstance)) continue;

		Flux_StaticMeshDrawItem xItem;
		xItem.m_pxModel = pxModel;
		xItem.m_pxModelInstance = pxModelInstance;
		pxModel->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xItem.m_xModelMatrix);
		xPacket.PushBack(xItem);
	}
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
	Flux_ModelInstance* pxModelInstance, const Zenith_Maths::Matrix4& xModelMatrix)
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

	for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
	{
		Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
		if (!pxMeshInstance) continue;

		pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
		pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

		Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
		// Non-capturing static helper: re-enter via g_xEngine.StaticMeshes() to reach
		// the singleton instance, then route the FluxGraphics reach-in (blank-material
		// fallback) through the injected member (mirrors ExecuteSDFs / ExecuteQuads).
		if (!pxMaterial) pxMaterial = g_xEngine.StaticMeshes().m_pxGraphics->m_xBlankMaterial.GetDirect();
		Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

		DrawStaticMesh(pxCmdList, xBinder, xModelMatrix, pxMaterial, pxMeshInstance->GetNumIndices());
	}
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bStaticMeshesEnabled) return;

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.StaticMeshes().m_xGBufferPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	// Non-capturing graph callback: keep the g_xEngine.StaticMeshes() self-lookup to
	// reach the shader on this singleton instance, and route the FluxGraphics reach-in
	// (frame constants) through the injected member (mirrors ExecuteSDFs / ExecuteQuads).
	xBinder.BindCBV(g_xEngine.StaticMeshes().m_xGBufferShader, "FrameConstants", &g_xEngine.StaticMeshes().m_pxGraphics->m_xFrameConstantsBuffer.GetCBV());

	// Iterate the packet gathered on the main thread (GatherDrawPacket). No ECS
	// access here — this runs on a worker thread.
	Zenith_Vector<Flux_StaticMeshDrawItem>& xPacket = g_xEngine.StaticMeshes().m_xDrawPacket;
	for (u_int u = 0; u < xPacket.GetSize(); u++)
	{
		const Flux_StaticMeshDrawItem& xItem = xPacket.Get(u);
		RenderModelInstanceMeshes(pxCmdList, xBinder, xItem.m_pxModelInstance, xItem.m_xModelMatrix);
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

	// Iterate the packet gathered on the main thread (GatherDrawPacket). This is
	// the same packet the GBuffer pass consumes; it is shared across all 4 shadow
	// cascades. No ECS access here — this runs on a worker thread.
	Zenith_Vector<Flux_StaticMeshDrawItem>& xPacket = g_xEngine.StaticMeshes().m_xDrawPacket;
	for (u_int u = 0; u < xPacket.GetSize(); u++)
	{
		const Flux_StaticMeshDrawItem& xItem = xPacket.Get(u);
		Flux_ModelInstance* pxModelInstance = xItem.m_pxModelInstance;
		const Zenith_Maths::Matrix4& xModelMatrix = xItem.m_xModelMatrix;

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

