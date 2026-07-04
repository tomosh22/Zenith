#include "Zenith.h"
#include "Flux/Translucency/Flux_Translucency_Shaders.h"

#include "Flux/Translucency/Flux_TranslucencyImpl.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RendererImpl.h"	// GetExternalTranslucentItems (per-view external gather)
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/Translucency.h" // typed binding handles
#include "Core/Zenith_GraphicsOptions.h"

#include <algorithm>

// Pass constants — mirrors TranslucencyConstantsLayout in
// Shaders/Translucency/Flux_Translucent_Forward.slang.
struct TranslucencyPassConstants
{
	u_int m_bIBLEnabled;
	u_int m_bIBLDiffuseEnabled;
	u_int m_bIBLSpecularEnabled;
	float m_fIBLIntensity;
	// (shadow/dynamic-light gates are per-view now — g_xView.g_uViewFlags)
	float m_fAmbientFallbackIntensity;
	// std140: g_xCSMTexelSize (float2) is 8-byte aligned → offset 24, with a
	// 4-byte pad at 20. Mirrors TranslucencyConstants_CB in Generated/
	// Translucency.h (static_asserted below against the reflected layout).
	u_int m_uPad0;
	float m_fCSMTexelSizeX;
	float m_fCSMTexelSizeY;
};
static_assert(sizeof(TranslucencyPassConstants) == 32, "TranslucencyPassConstants must match the reflected TranslucencyConstants_CB (32B, texelSize at offset 24)");
static_assert(offsetof(TranslucencyPassConstants, m_fCSMTexelSizeX) == 24, "g_xCSMTexelSize must sit at offset 24 (std140 float2 alignment)");

static void ExecuteTranslucency(Flux_CommandBuffer* pxCmdList, void*);

void Flux_TranslucencyImpl::BuildPipelines()
{
	m_xShader.Initialise(Flux_TranslucencyShaders::xTranslucent_Forward);

	// Same vertex layout as static meshes.
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xSpec;
	xSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xSpec.m_uNumColourAttachments = 1;
	xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xSpec.m_pxShader = &m_xShader;
	xSpec.m_xVertexInputDesc = xVertexDesc;
	// Depth-test against the opaque scene; never write depth (the graph binds
	// the scene depth READ-ONLY for this pass — Grass pattern).
	xSpec.m_bDepthTestEnabled = true;
	xSpec.m_bDepthWriteEnabled = false;
	xSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;

	m_xShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

	// One (blend × cull) quad per view-shading mode (Stage 3a/3b). The whole
	// blend/cull setup is re-established each iteration (the additive block below
	// mutates it). Stage 3b bakes the per-mode view-shading permission mask: FULL
	// permits shadows + cluster lights (identical to pre-3b, constants default true),
	// BASIC bakes both FALSE so the compiler strips the shadow/cluster clauses.
	namespace TFGen = Flux_Generated_Translucency::Translucent_Forward;
	for (u_int uMode = 0; uMode < FLUX_VIEW_SHADING_MODE_COUNT; uMode++)
	{
		const bool bPermitLit = (uMode == FLUX_VIEW_SHADING_MODE_FULL);
		xSpec.m_xSpecConstants = Flux_SpecConstantTable{};
		xSpec.m_xSpecConstants.AddBool(TFGen::hscFLUX_SC_VIEW_SHADOWS_PERMITTED,        bPermitLit);
		xSpec.m_xSpecConstants.AddBool(TFGen::hscFLUX_SC_VIEW_CLUSTER_LIGHTS_PERMITTED, bPermitLit);

		// Classic alpha blend.
		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

		xSpec.m_eCullMode = CULL_MODE_BACK;
		Flux_PipelineBuilder::FromSpecification(m_axPipelineTranslucent[uMode], xSpec);
		xSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_axPipelineTranslucentTwoSided[uMode], xSpec);

		// Additive (alpha scales the contribution: src*alpha + dst).
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

		xSpec.m_eCullMode = CULL_MODE_BACK;
		Flux_PipelineBuilder::FromSpecification(m_axPipelineAdditive[uMode], xSpec);
		xSpec.m_eCullMode = CULL_MODE_NONE;
		Flux_PipelineBuilder::FromSpecification(m_axPipelineAdditiveTwoSided[uMode], xSpec);
	}
}

void Flux_TranslucencyImpl::Initialise()
{
	BuildPipelines();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Translucency initialised");
}

void Flux_TranslucencyImpl::Shutdown()
{
	// Pipelines reference the shader, so destroy pipelines first.
	for (u_int uMode = 0; uMode < FLUX_VIEW_SHADING_MODE_COUNT; uMode++)
	{
		m_axPipelineTranslucent[uMode].Reset();
		m_axPipelineTranslucentTwoSided[uMode].Reset();
		m_axPipelineAdditive[uMode].Reset();
		m_axPipelineAdditiveTwoSided[uMode].Reset();
	}
	m_xShader.Reset();
}

void Flux_TranslucencyImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Forward pass over the lit HDR scene — registered between SSAO and Fog
	// in the setup walk (Flux_FeatureRegistry): glass must not be darkened by
	// SSAO's post-hoc multiply, and fog must composite over it. READ_DEPTH
	// binds the scene depth as a READ-ONLY depth attachment (Grass pattern).
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const Flux_PassHandle xPass = xGraph.AddPass("Translucency", ExecuteTranslucency)
		.Prepare([](void* p){ g_xEngine.Translucency().GatherDrawPacket(p); })
		.Writes(g_xEngine.FluxGraphics().GetHDRSceneTarget(), RESOURCE_ACCESS_WRITE_RTV)
		.Reads (xGraphics.GetDepthAttachment(),      RESOURCE_ACCESS_READ_DEPTH);

	// Shadow maps — one 4-cascade depth array (Phase 4b). Read ALL layers so the
	// graph orders this pass after the cascade writers with a full-array barrier.
	xGraph.ReadTransient(xPass, g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);

	// Clustered light buffers (LightBuffer itself is not graph-tracked — see
	// the DeferredShading declaration comment).
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (xLightClustering.IsInitialised())
	{
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadBuffer(xPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(),
			RESOURCE_ACCESS_READ_SRV);
	}

	// IBL textures.
	Flux_IBLImpl& xIBL = g_xEngine.IBL();
	xGraph.Read(xPass, xIBL.m_xBRDFLUT,        RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, xIBL.m_xIrradianceMap,  RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xPass, xIBL.m_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);

	// Preview view (S5b): a second translucency instance over the preview view's
	// HDR target + depth (NO ClearTargets — deferred already cleared the target).
	// Same record callback — the pass's view slot selects the per-view packet +
	// the preview VIEW set (whose flags gate shadow/cluster sampling off). The
	// CSM/cluster/IBL reads are still declared: the shader STATICALLY samples
	// those persistent VIEW members, so the graph-Read validator demands them.
	// NO Prepare here — the main pass's gather above fills EVERY view's packet
	// (a second Prepare would double-gather).
	if (xGraphics.RenderViews().IsViewActive(kuFluxViewSlotPreview))
	{
		const Flux_PassHandle xPreviewPass = xGraph.AddPass("Translucency (Preview)", ExecuteTranslucency)
			.View(kuFluxViewSlotPreview)
			.Writes(xGraphics.GetHDRSceneTarget(kuFluxViewSlotPreview), RESOURCE_ACCESS_WRITE_RTV)
			.Reads (xGraphics.GetDepthAttachment(kuFluxViewSlotPreview), RESOURCE_ACCESS_READ_DEPTH);
		xGraph.ReadTransient(xPreviewPass, g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);
		if (xLightClustering.IsInitialised())
		{
			xGraph.ReadBuffer(xPreviewPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(),
				RESOURCE_ACCESS_READ_SRV);
			xGraph.ReadBuffer(xPreviewPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(),
				RESOURCE_ACCESS_READ_SRV);
		}
		xGraph.Read(xPreviewPass, xIBL.m_xBRDFLUT,        RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(xPreviewPass, xIBL.m_xIrradianceMap,  RESOURCE_ACCESS_READ_SRV);
		xGraph.Read(xPreviewPass, xIBL.m_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);
	}
}

// Build one translucent packet entry from a draw source — mesh instance +
// (non-null) material + world matrix — with the sort key measured against the
// TARGET view's camera. Shared by the snapshot walk and the external-item
// gather so both build byte-identical entries.
static Flux_TranslucentDrawItem BuildTranslucentDrawItem(Flux_MeshInstance* pxMeshInstance,
	Zenith_MaterialAsset* pxMaterial, const Zenith_Maths::Matrix4& xModelMatrix,
	const Zenith_Maths::Vector3& xCameraPos)
{
	const Zenith_MaterialParams& xParams = pxMaterial->GetResolved().m_xParams;
	Flux_TranslucentDrawItem xItem;
	xItem.m_pxMeshInstance = pxMeshInstance;
	xItem.m_pxMaterial = pxMaterial;
	xItem.m_xModelMatrix = xModelMatrix;
	const Zenith_Maths::Vector3 xToCamera = Zenith_Maths::Vector3(xModelMatrix[3]) - xCameraPos;
	xItem.m_fViewDepthSq = glm::dot(xToCamera, xToCamera);
	xItem.m_bAdditive = (xParams.m_eBlendMode == MATERIAL_BLEND_ADDITIVE);
	xItem.m_bTwoSided = xParams.m_bTwoSided;
	return xItem;
}

// Back-to-front: farthest first so closer translucents blend over them. The one
// comparator for every view's packet (each packet's keys were measured against
// its own view's camera).
static void SortPacketBackToFront(Zenith_Vector<Flux_TranslucentDrawItem>& xPacket)
{
	if (xPacket.GetSize() > 1)
	{
		std::sort(&xPacket.Get(0), &xPacket.Get(0) + xPacket.GetSize(),
			[](const Flux_TranslucentDrawItem& xA, const Flux_TranslucentDrawItem& xB)
			{
				return xA.m_fViewDepthSq > xB.m_fViewDepthSq;
			});
	}
}

// Prepare callback (main thread): pull every Translucent/Additive submesh out
// of the EC-side model gather and depth-sort the result back-to-front.
// The opaque unified path skips these submeshes symmetrically, so each
// submesh renders on exactly one path.
// ONE callback fills EVERY view's packet: the MAIN packet from the snapshot
// walk (+ main-masked external items), the PREVIEW packet from the renderer's
// preserved external translucent items (scene content never enters the preview).
void Flux_TranslucencyImpl::GatherDrawPacket(void*)
{
	ZENITH_PROFILE_SCOPE("Flux Translucency Gather");
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; ++u)
	{
		m_axDrawPackets[u].Clear();
	}
	Zenith_Vector<Flux_TranslucentDrawItem>& xPacket = m_axDrawPackets[kuFluxViewSlotMain];

	const Zenith_Maths::Vector3& xCameraPos = g_xEngine.FluxGraphics().GetCameraPosition();

	// Phase 2: read the engine-owned uncullled snapshot (rebuilt once per frame) instead
	// of running our own ECS scan. Per-submesh translucent/additive selection below is
	// unchanged, so the same submeshes render in the same order.
	if (m_pxSnapshot)
	{
		const Zenith_Vector<Flux_RenderSceneItem>& xItems = m_pxSnapshot->Items();
		const bool bCull = m_pxSnapshot->IsCameraFrustumValid();   // skip culling against an unresolved camera
		const Zenith_Frustum& xFrustum = m_pxSnapshot->GetCameraFrustum();

		for (u_int u = 0; u < xItems.GetSize(); ++u)
		{
			const Flux_RenderSceneItem& xSrc = xItems.Get(u);
			Flux_ModelInstance* pxModelInstance = xSrc.m_pxModelInstance;
			const Zenith_Maths::Matrix4& xModelMatrix = xSrc.m_xWorldMatrix;

			// Phase 3: camera-frustum cull the whole model before walking submeshes (translucency
			// is a screen effect — off-screen translucent geometry contributes nothing). Conservative:
			// an invalid AABB is never culled; culling is skipped entirely when the camera is unresolved.
			if (bCull && xSrc.m_xWorldAABB.IsValid() && !Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xSrc.m_xWorldAABB)) continue;

			for (uint32_t uMesh = 0; uMesh < pxModelInstance->GetNumMeshes(); uMesh++)
			{
				Zenith_MaterialAsset* pxMaterial = pxModelInstance->GetMaterial(uMesh);
				if (!pxMaterial) continue;

				const Zenith_MaterialParams& xParams = pxMaterial->GetResolved().m_xParams;
				const bool bTranslucent = (xParams.m_eBlendMode == MATERIAL_BLEND_TRANSLUCENT) ||
										  (xParams.m_eBlendMode == MATERIAL_BLEND_ADDITIVE);
				if (!bTranslucent) continue;

				// v1: skinned-animated translucent submeshes are not supported.
				if (pxModelInstance->GetSkinnedMeshInstance(uMesh) != nullptr)
				{
					if (!m_bWarnedAnimatedTranslucent)
					{
						Zenith_Warning(LOG_CATEGORY_RENDERER,
							"Flux_Translucency: skinned-animated translucent submesh skipped — animated translucency is not supported yet");
						m_bWarnedAnimatedTranslucent = true;
					}
					continue;
				}

				Flux_MeshInstance* pxMeshInstance = pxModelInstance->GetMeshInstance(uMesh);
				if (!pxMeshInstance) continue;

				xPacket.PushBack(BuildTranslucentDrawItem(pxMeshInstance, pxMaterial, xModelMatrix, xCameraPos));
			}
		}
	}

	// External renderer-level submissions, preserved for us by the sync's
	// ExtractExternalSceneItems (mesh + material kept alive by the owner for the
	// frame; the material is pre-resolved, never null). Filtered per view by the
	// item's view-mask bit. No frustum cull (a handful of items at most), but the
	// sort key is measured against each TARGET view's camera so the shared
	// comparator sorts every packet consistently.
	const Flux_RenderViewRegistry& xViews = g_xEngine.FluxGraphics().RenderViews();
	const bool bPreviewActive = xViews.IsViewActive(kuFluxViewSlotPreview);
	const Zenith_Maths::Vector3 xPreviewCamPos =
		Zenith_Maths::Vector3(xViews.View(kuFluxViewSlotPreview).m_xConstants.m_xCamPos_Pad);

	const Zenith_Vector<Flux_RendererImpl::Flux_ExternalSceneItem>& xExternalItems =
		g_xEngine.FluxRenderer().GetExternalTranslucentItems();
	for (u_int u = 0; u < xExternalItems.GetSize(); ++u)
	{
		const Flux_RendererImpl::Flux_ExternalSceneItem& xExt = xExternalItems.Get(u);
		if (xExt.m_pxMeshInstance == nullptr || xExt.m_pxMaterial == nullptr) continue;

		// Main view is opt-in via the mask bit — a future caller can inject
		// translucent content into the main view (nothing sets it today).
		if ((xExt.m_uViewMask >> kuFluxViewSlotMain) & 1u)
		{
			xPacket.PushBack(BuildTranslucentDrawItem(xExt.m_pxMeshInstance, xExt.m_pxMaterial, xExt.m_xWorldMatrix, xCameraPos));
		}
		if (bPreviewActive && ((xExt.m_uViewMask >> kuFluxViewSlotPreview) & 1u))
		{
			m_axDrawPackets[kuFluxViewSlotPreview].PushBack(
				BuildTranslucentDrawItem(xExt.m_pxMeshInstance, xExt.m_pxMaterial, xExt.m_xWorldMatrix, xPreviewCamPos));
		}
	}

	// Sort every view's packet back-to-front and register its materials with the
	// GPU material table (MAIN THREAD — assigns the table index + builds the
	// record + makes its textures bindless). The worker draw path reads the
	// index off the asset lock-free.
	Flux_MaterialTable& xTable = g_xEngine.FluxGraphics().MaterialTable();
	for (u_int uView = 0; uView < FLUX_MAX_RENDER_VIEWS; ++uView)
	{
		Zenith_Vector<Flux_TranslucentDrawItem>& xViewPacket = m_axDrawPackets[uView];
		SortPacketBackToFront(xViewPacket);
		for (u_int u = 0; u < xViewPacket.GetSize(); ++u)
		{
			if (xViewPacket.Get(u).m_pxMaterial) xTable.GetOrCreateIndex(xViewPacket.Get(u).m_pxMaterial);
		}
	}
}

static void ExecuteTranslucency(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTranslucencyEnabled) return;

	Flux_TranslucencyImpl& xZZ = g_xEngine.Translucency();
	// The SAME callback records both the main pass and "Translucency (Preview)":
	// the recording pass's declared view slot selects that view's packet (and its
	// VIEW set was bound by the backend's per-pass persistent-set bind).
	const u_int uViewSlot = Flux_RenderGraph::GetCurrentRecordingPassViewSlot();
	Zenith_Vector<Flux_TranslucentDrawItem>& xPacket = xZZ.m_axDrawPackets[uViewSlot];
	if (xPacket.GetSize() == 0) return;

	// View-shading pipeline variant for this pass's view (Stage 3a/3b): FULL for the
	// main view, BASIC for the material preview. Reads the per-view CONSTANTS flag
	// word (m_xConstants.m_uViewFlags) — the SAME field the shader branches on via
	// g_xView.g_uViewFlags — a single source of truth for the variant + runtime flag.
	const FluxViewShadingMode eShadingMode =
		Flux_ViewShadingModeFromFlags(g_xEngine.FluxGraphics().RenderViews().View(uViewSlot).m_xConstants.m_uViewFlags);

	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	Flux_ShaderBinder xBinder(*pxCmdList);

	// Pass constants (per-frame).
	TranslucencyPassConstants xConstants;
	xConstants.m_bIBLEnabled = (xIBL.IsEnabled() && xIBL.IsReady()) ? 1 : 0;
	xConstants.m_bIBLDiffuseEnabled = xIBL.IsDiffuseEnabled() ? 1 : 0;
	xConstants.m_bIBLSpecularEnabled = xIBL.IsSpecularEnabled() ? 1 : 0;
	xConstants.m_fIBLIntensity = xIBL.GetIntensity();
	xConstants.m_fAmbientFallbackIntensity = 0.03f;	// matches the deferred default
	xConstants.m_uPad0 = 0u;
	xConstants.m_fCSMTexelSizeX = 1.0f / static_cast<float>(ZENITH_FLUX_CSM_RESOLUTION);
	xConstants.m_fCSMTexelSizeY = 1.0f / static_cast<float>(ZENITH_FLUX_CSM_RESOLUTION);

	// Track the bound pipeline so the sorted walk only switches when the
	// (blend, cull) bucket changes between consecutive items.
	Flux_Pipeline* pxBoundPipeline = nullptr;

	for (u_int u = 0; u < xPacket.GetSize(); u++)
	{
		const Flux_TranslucentDrawItem& xItem = xPacket.Get(u);

		Flux_Pipeline* pxPipeline =
			xItem.m_bAdditive ? (xItem.m_bTwoSided ? &xZZ.m_axPipelineAdditiveTwoSided[eShadingMode] : &xZZ.m_axPipelineAdditive[eShadingMode])
							  : (xItem.m_bTwoSided ? &xZZ.m_axPipelineTranslucentTwoSided[eShadingMode] : &xZZ.m_axPipelineTranslucent[eShadingMode]);
		if (pxPipeline != pxBoundPipeline)
		{
			pxCmdList->SetPipeline(pxPipeline);
			pxBoundPipeline = pxPipeline;

			// (Re)bind the per-pass resources for the new pipeline.
			namespace TF = Flux_Generated_Translucency::Translucent_Forward;
			// The g_axTextures bindless table (set 2). g_axMaterials is a DRAW-set (4) member
			// bound per-draw below — it must be staged right after DrawConstants so the
			// set-4 group isn't reset out from under it by the intervening PASS-set binds.
			pxCmdList->UseBindlessTextures(2);
			xBinder.BindDrawConstants(TF::hTranslucencyConstants, &xConstants, sizeof(xConstants));

			// CSM and the all-cascade ShadowMatrices SSBO are now in the persistent VIEW set
			// (Phase 5.4) — no per-pass bind.

			// (IBL textures are in the persistent VIEW set now — Phase 5.4; no per-pass bind.)

			// (Clustered dynamic lights are in the persistent VIEW set now — Phase 5.4; no per-pass bind.)
		}

		pxCmdList->SetVertexBuffer(xItem.m_pxMeshInstance->GetVertexBuffer());
		pxCmdList->SetIndexBuffer(xItem.m_pxMeshInstance->GetIndexBuffer());

		u_int uMaterialIndex = xItem.m_pxMaterial ? xItem.m_pxMaterial->GetMaterialTableIndex() : 0u;
		if (uMaterialIndex == uFLUX_INVALID_MATERIAL_INDEX) uMaterialIndex = 0u;
		MeshDrawConstants xDrawConstants;
		BuildMeshDrawConstants(xDrawConstants, xItem.m_xModelMatrix, uMaterialIndex);
		xBinder.BindDrawConstants(Flux_Generated_Translucency::Translucent_Forward::hDrawConstants, &xDrawConstants, sizeof(xDrawConstants));
		// (g_axMaterials lives in the persistent GLOBAL set now — Phase 5.3 dissolved the
		// fragile per-draw re-stage this forward pass used to need.)

		pxCmdList->DrawIndexed(xItem.m_pxMeshInstance->GetNumIndices());
	}
}
