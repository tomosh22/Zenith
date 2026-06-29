#include "Zenith.h"
#include "Flux/Translucency/Flux_Translucency_Shaders.h"

#include "Flux/Translucency/Flux_TranslucencyImpl.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"

#include "Flux/Flux_GraphicsImpl.h"
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
	u_int m_bShadowsEnabled;
	u_int m_bDynamicLightsEnabled;
	float m_fAmbientFallbackIntensity;
	float m_fCSMTexelSizeX;
	float m_fCSMTexelSizeY;
};

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

	// Classic alpha blend.
	xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

	xSpec.m_eCullMode = CULL_MODE_BACK;
	Flux_PipelineBuilder::FromSpecification(m_xPipelineTranslucent, xSpec);
	xSpec.m_eCullMode = CULL_MODE_NONE;
	Flux_PipelineBuilder::FromSpecification(m_xPipelineTranslucentTwoSided, xSpec);

	// Additive (alpha scales the contribution: src*alpha + dst).
	xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	xSpec.m_eCullMode = CULL_MODE_BACK;
	Flux_PipelineBuilder::FromSpecification(m_xPipelineAdditive, xSpec);
	xSpec.m_eCullMode = CULL_MODE_NONE;
	Flux_PipelineBuilder::FromSpecification(m_xPipelineAdditiveTwoSided, xSpec);
}

void Flux_TranslucencyImpl::Initialise()
{
	BuildPipelines();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Translucency initialised");
}

void Flux_TranslucencyImpl::Shutdown()
{
	// Pipelines reference the shader, so destroy pipelines first.
	m_xPipelineTranslucent.Reset();
	m_xPipelineTranslucentTwoSided.Reset();
	m_xPipelineAdditive.Reset();
	m_xPipelineAdditiveTwoSided.Reset();
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
}

// Prepare callback (main thread): pull every Translucent/Additive submesh out
// of the EC-side model gather and depth-sort the result back-to-front.
// The opaque unified path skips these submeshes symmetrically, so each
// submesh renders on exactly one path.
void Flux_TranslucencyImpl::GatherDrawPacket(void*)
{
	ZENITH_PROFILE_SCOPE("Flux Translucency Gather");
	Zenith_Vector<Flux_TranslucentDrawItem>& xPacket = m_xDrawPacket;
	xPacket.Clear();

	// Phase 2: read the engine-owned uncullled snapshot (rebuilt once per frame) instead
	// of running our own ECS scan. Per-submesh translucent/additive selection below is
	// unchanged, so the same submeshes render in the same order.
	if (!m_pxSnapshot) return;
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = m_pxSnapshot->Items();
	const bool bCull = m_pxSnapshot->IsCameraFrustumValid();   // skip culling against an unresolved camera
	const Zenith_Frustum& xFrustum = m_pxSnapshot->GetCameraFrustum();

	const Zenith_Maths::Vector3& xCameraPos = g_xEngine.FluxGraphics().GetCameraPosition();

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

			Flux_TranslucentDrawItem xItem;
			xItem.m_pxMeshInstance = pxMeshInstance;
			xItem.m_pxMaterial = pxMaterial;
			xItem.m_xModelMatrix = xModelMatrix;
			const Zenith_Maths::Vector3 xToCamera = Zenith_Maths::Vector3(xModelMatrix[3]) - xCameraPos;
			xItem.m_fViewDepthSq = glm::dot(xToCamera, xToCamera);
			xItem.m_bAdditive = (xParams.m_eBlendMode == MATERIAL_BLEND_ADDITIVE);
			xItem.m_bTwoSided = xParams.m_bTwoSided;
			xPacket.PushBack(xItem);
		}
	}

	// Back-to-front: farthest first so closer translucents blend over them.
	if (xPacket.GetSize() > 1)
	{
		std::sort(&xPacket.Get(0), &xPacket.Get(0) + xPacket.GetSize(),
			[](const Flux_TranslucentDrawItem& xA, const Flux_TranslucentDrawItem& xB)
			{
				return xA.m_fViewDepthSq > xB.m_fViewDepthSq;
			});
	}

	// Register every translucent material with the GPU material table (MAIN THREAD —
	// assigns the table index + builds the record + makes its textures bindless). The
	// worker draw path reads the index off the asset lock-free.
	Flux_MaterialTable& xTable = g_xEngine.FluxGraphics().MaterialTable();
	for (u_int u = 0; u < xPacket.GetSize(); ++u)
	{
		if (xPacket.Get(u).m_pxMaterial) xTable.GetOrCreateIndex(xPacket.Get(u).m_pxMaterial);
	}
}

static void ExecuteTranslucency(Flux_CommandBuffer* pxCmdList, void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTranslucencyEnabled) return;

	Flux_TranslucencyImpl& xZZ = g_xEngine.Translucency();
	if (xZZ.m_xDrawPacket.GetSize() == 0) return;

	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	Flux_ShaderBinder xBinder(*pxCmdList);

	// Pass constants (per-frame).
	TranslucencyPassConstants xConstants;
	xConstants.m_bIBLEnabled = (xIBL.IsEnabled() && xIBL.IsReady()) ? 1 : 0;
	xConstants.m_bIBLDiffuseEnabled = xIBL.IsDiffuseEnabled() ? 1 : 0;
	xConstants.m_bIBLSpecularEnabled = xIBL.IsSpecularEnabled() ? 1 : 0;
	xConstants.m_fIBLIntensity = xIBL.GetIntensity();
	xConstants.m_bShadowsEnabled = 1;
	xConstants.m_bDynamicLightsEnabled = 1;
	xConstants.m_fAmbientFallbackIntensity = 0.03f;	// matches the deferred default
	xConstants.m_fCSMTexelSizeX = 1.0f / static_cast<float>(ZENITH_FLUX_CSM_RESOLUTION);
	xConstants.m_fCSMTexelSizeY = 1.0f / static_cast<float>(ZENITH_FLUX_CSM_RESOLUTION);

	// Track the bound pipeline so the sorted walk only switches when the
	// (blend, cull) bucket changes between consecutive items.
	Flux_Pipeline* pxBoundPipeline = nullptr;

	Zenith_Vector<Flux_TranslucentDrawItem>& xPacket = xZZ.m_xDrawPacket;
	for (u_int u = 0; u < xPacket.GetSize(); u++)
	{
		const Flux_TranslucentDrawItem& xItem = xPacket.Get(u);

		Flux_Pipeline* pxPipeline =
			xItem.m_bAdditive ? (xItem.m_bTwoSided ? &xZZ.m_xPipelineAdditiveTwoSided : &xZZ.m_xPipelineAdditive)
							  : (xItem.m_bTwoSided ? &xZZ.m_xPipelineTranslucentTwoSided : &xZZ.m_xPipelineTranslucent);
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
