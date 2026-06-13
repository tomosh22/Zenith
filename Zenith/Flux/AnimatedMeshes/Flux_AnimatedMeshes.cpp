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
// Wave 3: models arrive from the EC-side gatherer (g_pfnZenithModelGather, declared in
// Flux_ModelInstance.h) as (instance, matrix) pairs; the skeleton is read from the
// instance. No Zenith_ModelComponent.h / Zenith_TransformComponent.h / scene-query here.
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Phase 7b: state on Flux_AnimatedMeshesImpl held by Zenith_Engine.
//
// Cross-subsystem deps (FluxGraphics) are reached via g_xEngine at point of
// use. The non-capturing fn-pointer trampolines below (the ExecuteGBuffer graph
// callback, the SetupRenderGraph Prepare lambda, and the ZENITH_TOOLS
// hot-reload lambda) re-enter via g_xEngine.AnimatedMeshes() since they cannot
// capture. Bone buffers come from the gathered Flux_SkeletonInstance (sourced
// from Zenith_AnimatorComponent), not a render dep.


static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*);

void Flux_AnimatedMeshesImpl::BuildPipelines()
{
	// Instance method: every member touched here belongs to THIS instance, so
	// the old g_xEngine.AnimatedMeshes() self-lookups are replaced with direct
	// member access (no singleton round-trip).
	m_xGBufferShader.Initialise(FluxShaderProgram::AnimatedMesh_ToGBuffer);
	m_xShadowShader.Initialise(FluxShaderProgram::AnimatedMesh_ToShadowmap);

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
		xPipelineSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE] = MRT_FORMAT_EMISSIVE;
		xPipelineSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xPipelineSpec.m_pxShader = &m_xGBufferShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		m_xGBufferShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		for (Flux_BlendState& xBlendState : xPipelineSpec.m_axBlendStates)
		{
			xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
			xBlendState.m_bBlendEnabled = false;
		}

		// Cull permutation pair (StaticMeshes pattern): one-sided culls back
		// faces, two-sided renders both with the shader-side normal flip.
		xPipelineSpec.m_eCullMode = CULL_MODE_BACK;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipelineTwoSided, xPipelineSpec);
	}

	{
		// CSMs are depth-only (GetCSMTargetSetup returns null colour, uNumColour=0).
		// Format is the shared CSM_FORMAT constant so we don't need the transient
		// attachment to exist yet.
		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_pxShader = &m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		xShadowPipelineSpec.m_bDepthBias = false;

		m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowPipelineSpec);
	}
}

void Flux_AnimatedMeshesImpl::Initialise()
{
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
	// Pipelines reference their shaders, so destroy pipelines first. Instance
	// method: direct member access, no g_xEngine.AnimatedMeshes() self-lookup.
	m_xGBufferPipeline.Reset();
	m_xGBufferPipelineTwoSided.Reset();
	m_xShadowPipeline.Reset();
	m_xGBufferShader.Reset();
	m_xShadowShader.Reset();
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
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Animated Meshes GBuffer", ExecuteGBuffer)
		.Prepare([](void* p){ g_xEngine.AnimatedMeshes().GatherDrawPacket(p); }) // ECS gather — self-routed, NOT injected.
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

// Prepare callback (main thread): consume the EC-side model gather (every model
// instance + its world matrix), keep ONLY skinned-animated models (the inverse of
// the StaticMeshes filter), and store everything the worker-thread record path
// needs — the model matrix, the Flux_ModelInstance*, and the Flux_SkeletonInstance*
// (its bone buffer is read at record time). This is the single shared source for the
// GBuffer pass and all shadow cascades; the worker paths do no live-ECS read. The
// selection/skip filter MUST stay identical to the old GBuffer callback so the same
// set of models renders, in the same order.
void Flux_AnimatedMeshesImpl::GatherDrawPacket(void*)
{
	// Instance method: the packet is our own member (direct access). Wave 3: the
	// scene query now lives in the EC-side gatherer; this body touches no ECS.
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = m_xDrawPacket;
	xPacket.Clear();

	// Wave 3: models are gathered EC-side into parallel (instance, matrix) lists.
	Zenith_Vector<Flux_ModelInstance*> xInstances;
	Zenith_Vector<Zenith_Maths::Matrix4> xMatrices;
	if (g_pfnZenithModelGather) g_pfnZenithModelGather(xInstances, xMatrices);

	for (u_int u = 0; u < xInstances.GetSize(); ++u)
	{
		Flux_ModelInstance* pxModelInstance = xInstances.Get(u);

		// The skeleton instance distinguishes animated (skinned) models from static
		// ones. No skeleton -> rendered by Flux_StaticMeshes instead. (Equivalent to
		// the old HasModel() && HasSkeleton() component test — both delegate to the
		// model instance.)
		Flux_SkeletonInstance* pxSkeleton = pxModelInstance->GetSkeletonInstance();
		if (!pxSkeleton) continue;

		Flux_AnimatedMeshDrawItem xItem;
		xItem.m_pxModelInstance = pxModelInstance;
		xItem.m_pxSkeleton      = pxSkeleton;
		xItem.m_xModelMatrix    = xMatrices.Get(u);
		xPacket.PushBack(xItem);
	}
}

static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bAnimatedMeshesEnabled)
	{
		return;
	}

	// Non-capturing graph callback: it cannot capture state, so it re-enters
	// via g_xEngine.AnimatedMeshes() ONCE to recover the singleton instance,
	// then routes everything (its own members + the injected FluxGraphics
	// member) through xZZ (mirrors ExecuteSSAOGenerate / ExecuteQuads).
	Flux_AnimatedMeshesImpl& xZZ = g_xEngine.AnimatedMeshes();

	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(*pxCmdList);

	// Iterate the packet gathered on the main thread (GatherDrawPacket). No ECS
	// access here — this runs on a worker thread; only heap-stable Flux objects
	// (model instance, skeleton instance) are dereferenced. Two walks: one per
	// cull pipeline (one-sided cull-back, then two-sided cull-none).
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = xZZ.m_xDrawPacket;

	static bool s_bLoggedOnce = false;
	for (u_int uCullPass = 0; uCullPass < 2; uCullPass++)
	{
	const bool bTwoSidedPass = (uCullPass == 1);
	pxCmdList->AddCommand<Flux_CommandSetPipeline>(bTwoSidedPass ? &xZZ.m_xGBufferPipelineTwoSided : &xZZ.m_xGBufferPipeline);

	// Bind FrameConstants once per pipeline (set 0 - per-frame data). The
	// lone FluxGraphics reach-in routes through the recovered instance's
	// injected member.
	xBinder.BindCBV(xZZ.m_xGBufferShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

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

			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial)
			{
				// Skip rendering if no material - fallback handled during model instance creation
				continue;
			}

			const Zenith_MaterialParams& xParams = pxMaterial->GetResolved().m_xParams;

			// Translucent/Additive submeshes are routed to the forward
			// Translucency path (which skips skinned meshes with a warning).
			if (xParams.m_eBlendMode == MATERIAL_BLEND_TRANSLUCENT ||
				xParams.m_eBlendMode == MATERIAL_BLEND_ADDITIVE) continue;

			// Bin by cull mode (StaticMeshes pattern).
			const bool bMaterialTwoSided = xParams.m_bTwoSided;
			if (bMaterialTwoSided != bTwoSidedPass) continue;

			pxCmdList->AddCommand<Flux_CommandSetVertexBuffer>(&pxMeshInstance->GetVertexBuffer());
			pxCmdList->AddCommand<Flux_CommandSetIndexBuffer>(&pxMeshInstance->GetIndexBuffer());

			// Build and push material constants (192 bytes) - uses scratch buffer in set 1
			MaterialDrawConstants xPushConstants;
			BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.BindDrawConstants(xZZ.m_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and the full material texture set.
			xBinder.BindCBV(xZZ.m_xGBufferShader, "Bones", &xBoneBuffer.GetCBV());
			for (u_int uSlot = 0; uSlot < MATERIAL_TEXTURE_SLOT_COUNT; uSlot++)
			{
				Zenith_TextureAsset* pxTexture = pxMaterial->GetResolvedTexture(static_cast<MaterialTextureSlot>(uSlot));
				xBinder.BindSRV(xZZ.m_xGBufferShader, GetMaterialTextureBindingName(uSlot), &pxTexture->m_xSRV);
			}

			pxCmdList->AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
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
	// Instance method: the packet/shader are our own members (direct access).
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = m_xDrawPacket;
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

			xBinder.BindDrawConstants(m_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));

			// Bind set 1: bone buffer and shadow matrix (named bindings)
			xBinder.BindCBV(m_xShadowShader, "Bones", &xBoneBuffer.GetCBV());
			xBinder.BindCBV(m_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());

			xCmdBuf.AddCommand<Flux_CommandDrawIndexed>(pxMeshInstance->GetNumIndices());
		}
	}
}

