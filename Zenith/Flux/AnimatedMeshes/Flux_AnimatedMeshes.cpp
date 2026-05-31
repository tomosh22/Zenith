#include "Zenith.h"

#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
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

// Phase 7b: state on Flux_AnimatedMeshesImpl held by Zenith_Engine.
//
// Wave-15 DI seam (twin of Flux_StaticMeshesImpl; mirrors Flux_QuadsImpl): the
// lone render dep (Flux_GraphicsImpl) is injected into Initialise and stored in
// m_pxGraphics; instance methods route FluxGraphics reach-ins through
// m_pxGraphics. g_xEngine.AnimatedMeshes() self-lookup survives only in the
// non-capturing ExecuteGBuffer / hot-reload fn-pointer trampolines below. The WS7
// Prepare-gather's g_xEngine.Scenes() reach is an ECS lookup and stays
// self-routed (NOT injected); bone buffers come from the gathered
// Flux_SkeletonInstance (sourced from Zenith_AnimatorComponent), not a render dep.


static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*);

void Flux_AnimatedMeshesImpl::BuildPipelines()
{
	g_xEngine.AnimatedMeshes().m_xGBufferShader.Initialise(FluxShaderProgram::AnimatedMesh_ToGBuffer);
	g_xEngine.AnimatedMeshes().m_xShadowShader.Initialise(FluxShaderProgram::AnimatedMesh_ToShadowmap);

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
		xPipelineSpec.m_pxShader = &g_xEngine.AnimatedMeshes().m_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		g_xEngine.AnimatedMeshes().m_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		Flux_PipelineBuilder::FromSpecification(g_xEngine.AnimatedMeshes().m_xGBufferPipeline, xPipelineSpec);
	}

	{
		// CSMs are depth-only (GetCSMTargetSetup returns null colour, uNumColour=0).
		// Format is the shared CSM_FORMAT constant so we don't need the transient
		// attachment to exist yet.
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_pxShader = &g_xEngine.AnimatedMeshes().m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		g_xEngine.AnimatedMeshes().m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(g_xEngine.AnimatedMeshes().m_xShadowPipeline, xShadowPipelineSpec);
	}
}

void Flux_AnimatedMeshesImpl::Initialise(Flux_GraphicsImpl& xGraphics)
{
	// Wave-15 DI seam: store the injected render dep. The FluxGraphics reach-ins
	// (in ExecuteGBuffer / SetupRenderGraph) route through this instead of
	// g_xEngine.FluxGraphics().
	m_pxGraphics = &xGraphics;

	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::AnimatedMesh_ToGBuffer,
		FluxShaderProgram::AnimatedMesh_ToShadowmap,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.AnimatedMeshes().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_ANIMATION, "Flux_AnimatedMeshes initialised");
}

void Flux_AnimatedMeshesImpl::Shutdown()
{
	// Pipelines reference their shaders, so destroy pipelines first.
	g_xEngine.AnimatedMeshes().m_xGBufferPipeline.Reset();
	g_xEngine.AnimatedMeshes().m_xShadowPipeline.Reset();
	g_xEngine.AnimatedMeshes().m_xGBufferShader.Reset();
	g_xEngine.AnimatedMeshes().m_xShadowShader.Reset();

	// Drop the injected dep so the instance returns to a clean default state.
	m_pxGraphics = nullptr;
}

void Flux_AnimatedMeshesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// The gather Prepare is hung on the GBuffer pass (Terrain/StaticMeshes
	// pattern). It runs on the main thread before any record callback, so the
	// packet it builds is available to BOTH this pass's ExecuteGBuffer and all
	// shadow cascades (which call RenderToShadowMap from Flux_Shadows).
	// Pass-registration order is irrelevant to Prepare timing —
	// CallPrepareCallbacks runs every enabled pass's Prepare before
	// RecordCommandLists dispatches any record task. Reads/Writes/DependsOn are
	// unchanged so the resolved pass order stays byte-identical.
	xGraph.AddPass("Animated Meshes GBuffer", ExecuteGBuffer)
		.Prepare([](void* p){ g_xEngine.AnimatedMeshes().GatherDrawPacket(p); }) // ECS gather — self-routed, NOT injected.
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_pxGraphics->GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

// Prepare callback (main thread): walk every Zenith_ModelComponent in all
// scenes once, keep ONLY skinned-animated models (the inverse of the
// StaticMeshes filter), and store everything the worker-thread record path
// needs — the resolved model matrix (from the live Zenith_TransformComponent),
// the Flux_ModelInstance*, and the Flux_SkeletonInstance* (its bone buffer is
// read at record time). This is the single shared source for the GBuffer pass
// and all shadow cascades; it removes every live-ECS read from those worker
// paths. The selection/skip filter MUST stay identical to the old GBuffer
// callback so the same set of models renders, in the same order.
void Flux_AnimatedMeshesImpl::GatherDrawPacket(void*)
{
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = g_xEngine.AnimatedMeshes().m_xDrawPacket;
	xPacket.Clear();

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	g_xEngine.Scenes().GetAllOfComponentTypeFromAllScenes<Zenith_ModelComponent>(xModels);

	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModelComponent = xIt.GetData();

		// Skip components without a model or skeleton (not animated).
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

		Flux_AnimatedMeshDrawItem xItem;
		xItem.m_pxModel = pxModelComponent;
		xItem.m_pxModelInstance = pxModelInstance;
		xItem.m_pxSkeleton = pxSkeleton;
		pxModelComponent->GetParentEntity().GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xItem.m_xModelMatrix);
		xPacket.PushBack(xItem);
	}
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bAnimatedMeshesEnabled)
	{
		return;
	}

	pxCmdList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.AnimatedMeshes().m_xGBufferPipeline);

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Bind FrameConstants once per command list (set 0 - per-frame data). Non-
	// capturing graph callback: it re-enters via g_xEngine.AnimatedMeshes() to
	// reach the singleton instance, then routes its lone FluxGraphics reach-in
	// through the injected member (mirrors ExecuteSSAOGenerate / ExecuteQuads).
	xBinder.BindCBV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "FrameConstants", &g_xEngine.AnimatedMeshes().m_pxGraphics->m_xFrameConstantsBuffer.GetCBV());

	// Iterate the packet gathered on the main thread (GatherDrawPacket). No ECS
	// access here — this runs on a worker thread; only heap-stable Flux objects
	// (model instance, skeleton instance) are dereferenced.
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = g_xEngine.AnimatedMeshes().m_xDrawPacket;

	static bool s_bLoggedOnce = false;
	for (u_int uItem = 0; uItem < xPacket.GetSize(); uItem++)
	{
		const Flux_AnimatedMeshDrawItem& xItem = xPacket.Get(uItem);

		Flux_ModelInstance* pxModelInstance = xItem.m_pxModelInstance;
		Flux_SkeletonInstance* pxSkeleton = xItem.m_pxSkeleton;
		const Zenith_Maths::Matrix4& xModelMatrix = xItem.m_xModelMatrix;

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

			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial)
			{
				// Skip rendering if no material - fallback handled during model instance creation
				continue;
			}

			// Build and push material constants (128 bytes) - uses scratch buffer in set 1
			MaterialDrawConstants xPushConstants;
			BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.BindDrawConstants(g_xEngine.AnimatedMeshes().m_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and material textures (named bindings)
			xBinder.BindCBV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "Bones", &xBoneBuffer.GetCBV());
			xBinder.BindSRV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "g_xDiffuseTex", &pxMaterial->GetDiffuseTexture()->m_xSRV);
			xBinder.BindSRV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "g_xNormalTex", &pxMaterial->GetNormalTexture()->m_xSRV);
			xBinder.BindSRV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "g_xRoughnessMetallicTex", &pxMaterial->GetRoughnessMetallicTexture()->m_xSRV);
			xBinder.BindSRV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "g_xOcclusionTex", &pxMaterial->GetOcclusionTexture()->m_xSRV);
			xBinder.BindSRV(g_xEngine.AnimatedMeshes().m_xGBufferShader, "g_xEmissiveTex", &pxMaterial->GetEmissiveTexture()->m_xSRV);

			pxCmdList->AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

void Flux_AnimatedMeshesImpl::RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	// Shadow pass uses Bones + DrawConstants + ShadowMatrix; the Slang
	// version reflects only what's actually read, so binding FrameConstants
	// here would fail the name lookup.

	// Iterate the packet gathered on the main thread (GatherDrawPacket). This is
	// the same packet the GBuffer pass consumes; it is shared across all shadow
	// cascades. No ECS access here — this runs on a worker thread; only
	// heap-stable Flux objects (model instance, skeleton instance) are touched.
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = g_xEngine.AnimatedMeshes().m_xDrawPacket;
	for (u_int uItem = 0; uItem < xPacket.GetSize(); uItem++)
	{
		const Flux_AnimatedMeshDrawItem& xItem = xPacket.Get(uItem);

		Flux_ModelInstance* pxModelInstance = xItem.m_pxModelInstance;
		const Zenith_Maths::Matrix4& xModelMatrix = xItem.m_xModelMatrix;

		// Get bone buffer from skeleton instance (resolved on the main thread).
		const Flux_DynamicConstantBuffer& xBoneBuffer = xItem.m_pxSkeleton->GetBoneBuffer();

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

			xBinder.BindDrawConstants(g_xEngine.AnimatedMeshes().m_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));

			// Bind set 1: bone buffer and shadow matrix (named bindings)
			xBinder.BindCBV(g_xEngine.AnimatedMeshes().m_xShadowShader, "Bones", &xBoneBuffer.GetCBV());
			xBinder.BindCBV(g_xEngine.AnimatedMeshes().m_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

