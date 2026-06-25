#include "Zenith.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes_Shaders.h"

#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"

#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
// Phase 2: models arrive from the engine-owned Flux_RenderSceneSnapshot (rebuilt once
// per frame by the renderer; injected via SetSnapshot); the skeleton is read from the
// instance. No Zenith_ModelComponent.h / Zenith_TransformComponent.h / scene-query here.
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/AnimatedMeshes.h" // typed binding handles

// Phase 7b: state on Flux_AnimatedMeshesImpl held by Zenith_Engine.
//
// Cross-subsystem deps (FluxGraphics) are reached via g_xEngine at point of
// use. The non-capturing fn-pointer trampolines below (the ExecuteGBuffer graph
// callback, the SetupRenderGraph Prepare lambda, and the ZENITH_TOOLS
// hot-reload lambda) re-enter via g_xEngine.AnimatedMeshes() since they cannot
// capture. Bone buffers come from the gathered Flux_SkeletonInstance (sourced
// from Zenith_AnimatorComponent), not a render dep.


static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void*);

void Flux_AnimatedMeshesImpl::BuildPipelines()
{
	// Instance method: every member touched here belongs to THIS instance, so
	// the old g_xEngine.AnimatedMeshes() self-lookups are replaced with direct
	// member access (no singleton round-trip).
	m_xGBufferShader.Initialise(Flux_AnimatedMeshesShaders::xAnimatedMesh_ToGBuffer);
	m_xShadowShader.Initialise(Flux_AnimatedMeshesShaders::xAnimatedMesh_ToShadowmap);

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

		// Fixed-function slope-scaled depth bias (set per-cascade via vkCmdSetDepthBias
		// in Flux_Shadows::ExecuteShadowCascade). Dynamic so it is runtime-tunable.
		xShadowPipelineSpec.m_bDepthBias = true;
		xShadowPipelineSpec.m_bDynamicDepthBias = true;
		xShadowPipelineSpec.m_fDepthBiasConstant = 1.75f;
		xShadowPipelineSpec.m_fDepthBiasSlope = 3.0f;

		m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowPipelineSpec);
	}
}

void Flux_AnimatedMeshesImpl::Initialise()
{
	BuildPipelines();

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
	// G-buffer Prepare: ensure the animated packet for the current snapshot (also ensured
	// from the shadow cascade-0 Prepare, generation-guarded).
	EnsureAnimatedPacket();
}

void Flux_AnimatedMeshesImpl::EnsureAnimatedPacket()
{
	ZENITH_PROFILE_SCOPE("Flux Animated Mesh Gather");
	if (!m_pxSnapshot) return;
	const uint32_t uGen = m_pxSnapshot->GetGeneration();
	if (m_uAnimatedPacketGen == uGen) return;   // already built this snapshot

	// Instance method: the packet is our own member (direct access).
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = m_xDrawPacket;
	xPacket.Clear();

	// Phase 2/3: read the engine-owned uncullled snapshot. The filter is byte-identical to
	// the old gather: keep ONLY models with a skeleton instance (the inverse of the
	// StaticMeshes filter), in the same order. Animated meshes are not camera-culled.
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = m_pxSnapshot->Items();
	for (u_int u = 0; u < xItems.GetSize(); ++u)
	{
		Flux_ModelInstance* pxModelInstance = xItems.Get(u).m_pxModelInstance;

		// The skeleton instance distinguishes animated (skinned) models from static
		// ones. No skeleton -> rendered by Flux_StaticMeshes instead.
		Flux_SkeletonInstance* pxSkeleton = pxModelInstance->GetSkeletonInstance();
		if (!pxSkeleton) continue;

		Flux_AnimatedMeshDrawItem xItem;
		xItem.m_pxModelInstance = pxModelInstance;
		xItem.m_pxSkeleton      = pxSkeleton;
		xItem.m_xModelMatrix    = xItems.Get(u).m_xWorldMatrix;
		xPacket.PushBack(xItem);
	}
	m_uAnimatedPacketGen = uGen;
}

static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void*)
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

	namespace GB = Flux_Generated_AnimatedMeshes::AnimatedMesh_ToGBuffer;

	// Iterate the packet gathered on the main thread (GatherDrawPacket). No ECS
	// access here — this runs on a worker thread; only heap-stable Flux objects
	// (model instance, skeleton instance) are dereferenced. Two walks: one per
	// cull pipeline (one-sided cull-back, then two-sided cull-none).
	Zenith_Vector<Flux_AnimatedMeshDrawItem>& xPacket = xZZ.m_xDrawPacket;

	static bool s_bLoggedOnce = false;
	for (u_int uCullPass = 0; uCullPass < 2; uCullPass++)
	{
	const bool bTwoSidedPass = (uCullPass == 1);
	pxCmdList->SetPipeline(bTwoSidedPass ? &xZZ.m_xGBufferPipelineTwoSided : &xZZ.m_xGBufferPipeline);

	// Bind FrameConstants once per pipeline (set 0 - per-frame data). The
	// lone FluxGraphics reach-in routes through the recovered instance's
	// injected member.
	xBinder.BindCBV(GB::hg_xView, &g_xEngine.FluxGraphics().m_xViewConstantsBuffer.GetCBV());

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

			pxCmdList->SetVertexBuffer(pxMeshInstance->GetVertexBuffer());
			pxCmdList->SetIndexBuffer(pxMeshInstance->GetIndexBuffer());

			// Build and push material constants (192 bytes) - uses scratch buffer in set 1
			MaterialDrawConstants xPushConstants;
			BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.BindDrawConstants(GB::hDrawConstants, &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer and the full material texture set.
			xBinder.BindCBV(GB::hBones, &xBoneBuffer.GetCBV());
				static constexpr Flux_BindingHandle s_aMatHandles[] = FLUX_MATERIAL_TEXTURE_HANDLES(Flux_Generated_AnimatedMeshes::AnimatedMesh_ToGBuffer);
				static_assert(sizeof(s_aMatHandles) / sizeof(s_aMatHandles[0]) == MATERIAL_TEXTURE_SLOT_COUNT, "material handle array size mismatch");
			for (u_int uSlot = 0; uSlot < MATERIAL_TEXTURE_SLOT_COUNT; uSlot++)
			{
				Zenith_TextureAsset* pxTexture = pxMaterial->GetResolvedTexture(static_cast<MaterialTextureSlot>(uSlot));
				xBinder.BindSRV(s_aMatHandles[uSlot], &pxTexture->m_xSRV);
			}

			pxCmdList->DrawIndexed(pxMeshInstance->GetNumIndices());
		}
	}
	}
}

void Flux_AnimatedMeshesImpl::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	// Create binder for named resource binding
	Flux_ShaderBinder xBinder(xCmdBuf);

	namespace SM = Flux_Generated_AnimatedMeshes::AnimatedMesh_ToShadowmap;

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

			// Mirror the G-buffer null policy (skinned null-material meshes don't render, so they
			// don't cast); fetch the material for the masked cutout the shadow FS reads.
			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (!pxMaterial) continue;
			const MaterialBlendMode eBlend = pxMaterial->GetResolved().m_xParams.m_eBlendMode;
			if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE) continue;

			xCmdBuf.SetVertexBuffer(pxMeshInstance->GetVertexBuffer());
			xCmdBuf.SetIndexBuffer(pxMeshInstance->GetIndexBuffer());

			// Full material constants: the shadow FS reads the alpha cutoff + UV transform.
			// Opaque writes cutoff 0 -> the FS uniform-branches out -> depth-only cost.
			MaterialDrawConstants xPushConstants;
			BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
			xBinder.BindDrawConstants(SM::hDrawConstants, &xPushConstants, sizeof(xPushConstants));

			// Bind set 1: bone buffer, shadow matrix, base colour (named bindings)
			xBinder.BindCBV(SM::hBones, &xBoneBuffer.GetCBV());
			xBinder.BindCBV(SM::hShadowMatrix, &xShadowMatrixBuffer.GetCBV());
			xBinder.BindSRV(SM::hg_xBaseColorTex, &pxMaterial->GetResolvedTexture(MATERIAL_TEXTURE_BASE_COLOR)->m_xSRV);

			xCmdBuf.DrawIndexed(pxMeshInstance->GetNumIndices());
		}
	}
}

