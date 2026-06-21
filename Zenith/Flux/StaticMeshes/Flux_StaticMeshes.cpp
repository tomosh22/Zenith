#include "Zenith.h"

#include "Flux/StaticMeshes/Flux_StaticMeshesImpl.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"


#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
// Phase 2: models arrive from the engine-owned Flux_RenderSceneSnapshot (rebuilt once
// per frame by the renderer; injected via SetSnapshot) -- no Zenith_ModelComponent.h /
// Zenith_TransformComponent.h / scene-query includes here any more.
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

// Phase 7b: state on Flux_StaticMeshesImpl held by Zenith_Engine.



static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void*);

void Flux_StaticMeshesImpl::BuildPipelines()
{
	m_xGBufferShader.Initialise(FluxShaderProgram::StaticMesh_ToGBuffer);
	m_xShadowShader.Initialise(FluxShaderProgram::StaticMesh_ToShadowmap);

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

		// Cull permutation pair: one-sided materials cull back faces; two-sided
		// materials render both (and flip the shading normal in the shader).
		xPipelineSpec.m_eCullMode = CULL_MODE_BACK;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xPipelineSpec);

		xPipelineSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipelineTwoSided, xPipelineSpec);
	}

	{

		Flux_PipelineSpecification xShadowPipelineSpec;
		xShadowPipelineSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xShadowPipelineSpec.m_uNumColourAttachments = 0;
		xShadowPipelineSpec.m_pxShader = &m_xShadowShader;
		xShadowPipelineSpec.m_xVertexInputDesc = xVertexDesc;

		// Fixed-function slope-scaled depth bias for shadow acne, set per-cascade
		// via vkCmdSetDepthBias (Flux_Shadows::ExecuteShadowCascade). Dynamic so the
		// factors are tunable at runtime; the static values are a sane fallback.
		xShadowPipelineSpec.m_bDepthBias = true;
		xShadowPipelineSpec.m_bDynamicDepthBias = true;
		xShadowPipelineSpec.m_fDepthBiasConstant = 1.75f;
		xShadowPipelineSpec.m_fDepthBiasSlope = 3.0f;

		m_xShadowShader.GetReflection().PopulateLayout(xShadowPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xShadowPipelineSpec);
	}
}

void Flux_StaticMeshesImpl::Initialise()
{
	BuildPipelines();

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddBoolean({ "Render", "StaticMeshes", "Force Cull None" }, m_bDbgForceCullNone);
#endif

	Zenith_Log(LOG_CATEGORY_MESH, "Flux_StaticMeshes initialised");
}

void Flux_StaticMeshesImpl::Shutdown()
{
	// Pipelines reference their shaders, so destroy pipelines first.
	m_xGBufferPipeline.Reset();
	m_xGBufferPipelineTwoSided.Reset();
	m_xShadowPipeline.Reset();
	m_xGBufferShader.Reset();
	m_xShadowShader.Reset();
}

void Flux_StaticMeshesImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// The gather Prepare is hung on the GBuffer pass (Terrain pattern). It runs
	// on the main thread before any record callback, so the packet it builds is
	// available to BOTH this pass's ExecuteGBuffer and all 4 shadow cascades
	// (which call RenderToShadowMap from Flux_Shadows). Pass-registration order
	// is irrelevant to Prepare timing — CallPrepareCallbacks runs every enabled
	// pass's Prepare before RecordCommandLists dispatches any record task.
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	xGraph.AddPass("Static Meshes GBuffer", ExecuteGBuffer)
		.Prepare([](void* p){ g_xEngine.StaticMeshes().GatherDrawPacket(p); })
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetMRTAttachment(MRT_INDEX_EMISSIVE),       RESOURCE_ACCESS_WRITE_RTV)
		.Writes(xGraphics.GetDepthAttachment(),                       RESOURCE_ACCESS_WRITE_DSV);
}

// Prepare callback (main thread): walk every Zenith_ModelComponent in all
// scenes once, skip skinned-animated models (rendered by Flux_AnimatedMeshes)
// and components without a model instance, resolve the model matrix from the
// live Zenith_TransformComponent here, and store everything the worker-thread
// record path needs. This is the single shared source for the GBuffer pass and
// all shadow cascades — it removes every live-ECS read from those worker paths.
void Flux_StaticMeshesImpl::GatherDrawPacket(void*)
{
	ZENITH_PROFILE_SCOPE("Flux Static Mesh Gather");
	// G-buffer Prepare: ensure both the camera (culled) and shadow (uncullled) packets for
	// this snapshot. Generation-guarded, so a repeat call from the shadow Prepare is a no-op.
	EnsureCameraPacket();
	EnsureShadowPacket();
}

void Flux_StaticMeshesImpl::EnsureCameraPacket()
{
	if (!m_pxSnapshot) return;
	const uint32_t uGen = m_pxSnapshot->GetGeneration();
	if (m_uCameraPacketGen == uGen) return;   // already built this snapshot

	BuildCameraPacket(*m_pxSnapshot, m_xCameraDrawPacket);
	m_uCameraPacketGen = uGen;
}

void Flux_StaticMeshesImpl::EnsureShadowPacket()
{
	if (!m_pxSnapshot) return;
	const uint32_t uGen = m_pxSnapshot->GetGeneration();
	if (m_uShadowPacketGen == uGen) return;   // already built this snapshot

	BuildShadowPacket(*m_pxSnapshot, m_xShadowDrawPacket);
	m_uShadowPacketGen = uGen;
}

// Pure (GPU-free) packet builders. Extracted from the Ensure* wrappers so the exact cull
// decision is unit-testable against a hand-built snapshot without a GPU-backed subsystem.
void Flux_StaticMeshesImpl::BuildCameraPacket(const Flux_RenderSceneSnapshot& xSnapshot,
	Zenith_Vector<Flux_StaticMeshDrawItem>& xOut)
{
	xOut.Clear();

	// Phase 3: camera-frustum cull the opaque static set (drops off-screen draws from the
	// G-buffer). Filter (skip skinned-animated) is byte-identical to Phase 2. Culling is
	// SKIPPED when the camera frustum is not valid this frame (no resolved camera — would
	// otherwise cull against an identity/stale matrix).
	const bool bCull = xSnapshot.IsCameraFrustumValid();
	const Zenith_Frustum& xFrustum = xSnapshot.GetCameraFrustum();
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = xSnapshot.Items();
	for (u_int u = 0; u < xItems.GetSize(); ++u)
	{
		const Flux_RenderSceneItem& xSrc = xItems.Get(u);
		// Skinned-animated models are rendered by Flux_AnimatedMeshes. Use the flag the
		// snapshot fill already stamped (byte-identical to re-running the predicate — the
		// build-before-workers contract means skinning topology can't change mid-frame).
		if (xSrc.m_bAnimatedSkinned) continue;

		// Conservative cull: an invalid AABB (no bounds) is never culled.
		if (bCull && xSrc.m_xWorldAABB.IsValid() && !Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xSrc.m_xWorldAABB)) continue;

		Flux_StaticMeshDrawItem xItem;
		xItem.m_pxModelInstance = xSrc.m_pxModelInstance;
		xItem.m_xModelMatrix    = xSrc.m_xWorldMatrix;
		xOut.PushBack(xItem);
	}
}

void Flux_StaticMeshesImpl::BuildShadowPacket(const Flux_RenderSceneSnapshot& xSnapshot,
	Zenith_Vector<Flux_StaticMeshDrawItem>& xOut)
{
	xOut.Clear();

	// Phase 3: shadow casters are UNCULLLED against the camera (an off-screen caster can
	// still cast a visible shadow). Per-cascade light-frustum culling is deferred.
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = xSnapshot.Items();
	for (u_int u = 0; u < xItems.GetSize(); ++u)
	{
		const Flux_RenderSceneItem& xSrc = xItems.Get(u);
		if (xSrc.m_bAnimatedSkinned) continue;   // drawn by Flux_AnimatedMeshes (precomputed flag)

		Flux_StaticMeshDrawItem xItem;
		xItem.m_pxModelInstance = xSrc.m_pxModelInstance;
		xItem.m_xModelMatrix    = xSrc.m_xWorldMatrix;
		xOut.PushBack(xItem);
	}
}

// Per-mesh draw helper. Binds material constants + SRVs, then emits the
// indexed draw.
void Flux_StaticMeshesImpl::DrawStaticMesh(Flux_CommandBuffer* pxCmdList, Flux_ShaderBinder& xBinder,
	const Zenith_Maths::Matrix4& xModelMatrix,
	Zenith_MaterialAsset* pxMaterial,
	u_int uIndexCount)
{
	MaterialDrawConstants xPushConstants;
	BuildMaterialDrawConstants(xPushConstants, xModelMatrix, pxMaterial);
	xBinder.BindDrawConstants(m_xGBufferShader, "DrawConstants", &xPushConstants, sizeof(xPushConstants));

	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		Zenith_TextureAsset* pxTexture = pxMaterial->GetResolvedTexture(static_cast<MaterialTextureSlot>(u));
		xBinder.BindSRV(m_xGBufferShader, GetMaterialTextureBindingName(u), &pxTexture->m_xSRV);
	}

	pxCmdList->DrawIndexed(uIndexCount);
}

// Called from the recovered instance in ExecuteGBuffer, once per cull pass.
void Flux_StaticMeshesImpl::RenderModelInstanceMeshes(Flux_CommandBuffer* pxCmdList, Flux_ShaderBinder& xBinder,
	Flux_ModelInstance* pxModelInstance, const Zenith_Maths::Matrix4& xModelMatrix,
	bool bTwoSidedPass)
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

		Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
		// Member method (promoted): route the FluxGraphics reach-in (blank-material
		// fallback) through the injected member directly.
		if (!pxMaterial) pxMaterial = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();
		Zenith_Assert(pxMaterial != nullptr, "Material is null and blank material fallback also null");

		const Zenith_MaterialParams& xParams = pxMaterial->GetResolved().m_xParams;

		// Translucent/Additive submeshes render on the forward Translucency
		// path, not the opaque G-buffer.
		if (xParams.m_eBlendMode == MATERIAL_BLEND_TRANSLUCENT ||
			xParams.m_eBlendMode == MATERIAL_BLEND_ADDITIVE) continue;

		// Bin by cull mode: meshes belong to exactly one of the two cull
		// passes. With ForceCullNone everything draws in the two-sided pass
		// (the engine's historical behaviour — safety valve for assets with
		// reversed winding).
		const bool bMaterialTwoSided = xParams.m_bTwoSided || m_bDbgForceCullNone;
		if (bMaterialTwoSided != bTwoSidedPass) continue;

		pxCmdList->SetVertexBuffer(pxMeshInstance->GetVertexBuffer());
		pxCmdList->SetIndexBuffer(pxMeshInstance->GetIndexBuffer());

		DrawStaticMesh(pxCmdList, xBinder, xModelMatrix, pxMaterial, pxMeshInstance->GetNumIndices());
	}
}

static void ExecuteGBuffer(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bStaticMeshesEnabled) return;

	// Non-capturing graph callback: recover the singleton instance ONCE via
	// g_xEngine.StaticMeshes() (it cannot capture), then drive the pipeline, shader,
	// injected FluxGraphics, packet and the promoted record helper through it.
	Flux_StaticMeshesImpl& xZZ = g_xEngine.StaticMeshes();

	Flux_ShaderBinder xBinder(*pxCmdList);

	// Iterate the CAMERA-CULLED packet (EnsureCameraPacket, main thread). No ECS access
	// here — this runs on a worker thread. Two walks: one per cull pipeline (one-sided
	// cull-back, then two-sided cull-none).
	Zenith_Vector<Flux_StaticMeshDrawItem>& xPacket = xZZ.m_xCameraDrawPacket;
	for (u_int uCullPass = 0; uCullPass < 2; uCullPass++)
	{
		const bool bTwoSidedPass = (uCullPass == 1);
		pxCmdList->SetPipeline(bTwoSidedPass ? &xZZ.m_xGBufferPipelineTwoSided : &xZZ.m_xGBufferPipeline);
		xBinder.BindCBV(xZZ.m_xGBufferShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

		for (u_int u = 0; u < xPacket.GetSize(); u++)
		{
			const Flux_StaticMeshDrawItem& xItem = xPacket.Get(u);
			xZZ.RenderModelInstanceMeshes(pxCmdList, xBinder, xItem.m_pxModelInstance, xItem.m_xModelMatrix, bTwoSidedPass);
		}
	}
}

void Flux_StaticMeshesImpl::RenderToShadowMap(Flux_CommandBuffer& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer)
{
	Flux_ShaderBinder xBinder(xCmdBuf);
	// Shadow pass binds only DrawConstants + ShadowMatrix. Slang reflection
	// drops bindings that the shader doesn't actually reference (this
	// program imports Common.Frame but reads no FrameConstants field), so
	// FrameConstants doesn't appear in the reflected layout — binding it
	// here would fail the name lookup.

	// Iterate the UNCULLLED shadow packet (EnsureShadowPacket, main thread) — off-screen
	// casters still cast. Shared across all 4 shadow cascades. No ECS access here — this
	// runs on a worker thread.
	Zenith_Vector<Flux_StaticMeshDrawItem>& xPacket = m_xShadowDrawPacket;
	for (u_int u = 0; u < xPacket.GetSize(); u++)
	{
		const Flux_StaticMeshDrawItem& xItem = xPacket.Get(u);
		Flux_ModelInstance* pxModelInstance = xItem.m_pxModelInstance;
		const Zenith_Maths::Matrix4& xModelMatrix = xItem.m_xModelMatrix;

		for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
		{
			Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
			if (!pxMeshInstance) continue;

			// Translucent/Additive submeshes don't cast shadows (UE-style).
			Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
			if (pxMaterial)
			{
				const MaterialBlendMode eBlend = pxMaterial->GetResolved().m_xParams.m_eBlendMode;
				if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE) continue;
			}

			xCmdBuf.SetVertexBuffer(pxMeshInstance->GetVertexBuffer());
			xCmdBuf.SetIndexBuffer(pxMeshInstance->GetIndexBuffer());

			xBinder.BindDrawConstants(m_xShadowShader, "DrawConstants", &xModelMatrix, sizeof(xModelMatrix));
			xBinder.BindCBV(m_xShadowShader, "ShadowMatrix", &xShadowMatrixBuffer.GetCBV());
			xCmdBuf.DrawIndexed(pxMeshInstance->GetNumIndices());
		}
	}
}

