#include "Zenith.h"
#include "Flux/MaterialPreview/Flux_MaterialPreview_Shaders.h"

#ifdef ZENITH_TOOLS

#include "Flux/MaterialPreview/Flux_MaterialPreviewImpl.h"
#include "Core/Zenith_Engine.h"

#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"   // GetCSMArrayHandle() for the mesh-pass CSM graph Read()
#include "Flux/Flux_MaterialBinding.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Shaders/Generated/MaterialPreview.h" // typed binding handles
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

// Preview tonemap constants — mirrors PreviewTonemapConstantsLayout in
// Shaders/MaterialPreview/Flux_MaterialPreview_Tonemap.slang.
struct MaterialPreviewTonemapConstants
{
	float m_fExposure;
};

// Translucency pass constants — mirrors TranslucencyConstantsLayout in
// Shaders/Translucency/Flux_Translucent_Forward.slang (the preview reuses
// that program; shadows and dynamic lights are disabled here).
struct MaterialPreviewPassConstants
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

static void ExecutePreviewBackground(Flux_CommandBuffer* pxCmdList, void*);
static void ExecutePreviewMesh(Flux_CommandBuffer* pxCmdList, void*);
static void ExecutePreviewTonemap(Flux_CommandBuffer* pxCmdList, void*);
// No-op: exists only so the graph leaves the LDR target in
// SHADER_READ_ONLY_OPTIMAL for ImGui to sample (same trick as the engine's
// @FinalRTLayoutTransition pass for the final render target).
static void ExecutePreviewLayoutTransition(Flux_CommandBuffer*, void*) {}

void Flux_MaterialPreviewImpl::BuildPipelines()
{
	m_xMeshShader.Initialise(Flux_MaterialPreviewShaders::xMaterialPreview_Forward);
	m_xBackgroundShader.Initialise(Flux_MaterialPreviewShaders::xMaterialPreview_Background);
	m_xTonemapShader.Initialise(Flux_MaterialPreviewShaders::xMaterialPreview_Tonemap);

	// ---- Mesh pipelines (static-mesh vertex layout) ----
	{
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
		xSpec.m_pxShader = &m_xMeshShader;
		xSpec.m_xVertexInputDesc = xVertexDesc;
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_bDepthWriteEnabled = true;
		xSpec.m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
		// Preview primitives are closed meshes; cull none avoids any winding
		// surprises at negligible cost for a 512^2 target.
		xSpec.m_eCullMode = CULL_MODE_NONE;

		m_xMeshShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = false;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ZERO;
		Flux_PipelineBuilder::FromSpecification(m_xMeshPipelineOpaque, xSpec);

		xSpec.m_axBlendStates[0].m_bBlendEnabled = true;
		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
		Flux_PipelineBuilder::FromSpecification(m_xMeshPipelineTranslucent, xSpec);

		xSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
		xSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;
		Flux_PipelineBuilder::FromSpecification(m_xMeshPipelineAdditive, xSpec);
	}

	// ---- Background (fullscreen cubemap sample into the HDR target) ----
	{
		Flux_PipelineSpecification xSpec = Flux_PipelineHelper::CreateFullscreenSpec(
			m_xBackgroundShader, Flux_MaterialPreviewShaders::xMaterialPreview_Background, HDR_SCENE_FORMAT, DEPTH_FORMAT);
		// Depth attached purely so the pass's ClearTargets resets it for the
		// mesh draw; background neither tests nor writes.
		xSpec.m_bDepthTestEnabled = false;
		xSpec.m_bDepthWriteEnabled = false;
		Flux_PipelineBuilder::FromSpecification(m_xBackgroundPipeline, xSpec);
	}

	// ---- Tonemap (HDR -> LDR for ImGui) ----
	{
		Flux_PipelineHelper::BuildFullscreenPipeline(
			m_xTonemapShader, m_xTonemapPipeline,
			Flux_MaterialPreviewShaders::xMaterialPreview_Tonemap, TEXTURE_FORMAT_RGBA8_UNORM);
	}
}

void Flux_MaterialPreviewImpl::Initialise()
{
	// Persistent targets (IBL CreateRenderTargets pattern) — they survive
	// graph rebuilds so the LDR SRV handed to ImGui stays valid.
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = uPREVIEW_SIZE;
	xBuilder.m_uHeight = uPREVIEW_SIZE;
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;

	xBuilder.m_eFormat = HDR_SCENE_FORMAT;
	xBuilder.BuildColour(m_xPreviewHDR, "MaterialPreview HDR");

	xBuilder.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xBuilder.BuildColour(m_xPreviewLDR, "MaterialPreview LDR");

	xBuilder.m_eFormat = DEPTH_FORMAT;
	xBuilder.BuildDepthStencil(m_xPreviewDepth, "MaterialPreview Depth");

	// Procedural preview primitives, pinned via registry handles.
	m_axMeshes[MATERIAL_PREVIEW_MESH_SPHERE].Set(Zenith_MeshGeometryAsset::CreateUnitSphere(32));
	m_axMeshes[MATERIAL_PREVIEW_MESH_CUBE].Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	m_axMeshes[MATERIAL_PREVIEW_MESH_PLANE].Set(Zenith_MeshGeometryAsset::CreateUnitCube());	// flattened via the model matrix
	m_axMeshes[MATERIAL_PREVIEW_MESH_CYLINDER].Set(Zenith_MeshGeometryAsset::CreateUnitCylinder(32));

	// Preview-camera VIEW constants (a real ViewConstants bound to the VIEW set).
	m_xPreviewViewConstants = Flux_GraphicsImpl::ViewConstants();
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&m_xPreviewViewConstants,
		sizeof(Flux_GraphicsImpl::ViewConstants), m_xPreviewViewConstantsBuffer);

	// Preview GLOBAL constants (bound to the GLOBAL set — carries the preview sun).
	m_xPreviewGlobalConstants = Flux_GraphicsImpl::GlobalConstants();
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&m_xPreviewGlobalConstants,
		sizeof(Flux_GraphicsImpl::GlobalConstants), m_xPreviewGlobalConstantsBuffer);

	BuildPipelines();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_MaterialPreview initialised");
}

void Flux_MaterialPreviewImpl::Shutdown()
{
	m_xMeshPipelineOpaque.Reset();
	m_xMeshPipelineTranslucent.Reset();
	m_xMeshPipelineAdditive.Reset();
	m_xBackgroundPipeline.Reset();
	m_xTonemapPipeline.Reset();
	m_xMeshShader.Reset();
	m_xBackgroundShader.Reset();
	m_xTonemapShader.Reset();

	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xPreviewViewConstantsBuffer);
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xPreviewGlobalConstantsBuffer);

	Flux_RenderAttachmentBuilder::Destroy(m_xPreviewHDR);
	Flux_RenderAttachmentBuilder::Destroy(m_xPreviewLDR);
	Flux_RenderAttachmentBuilder::Destroy(m_xPreviewDepth);

	for (u_int u = 0; u < MATERIAL_PREVIEW_MESH_COUNT; u++)
	{
		m_axMeshes[u].Clear();
	}
	m_xMaterial.Clear();
}

void Flux_MaterialPreviewImpl::SetMaterial(Zenith_MaterialAsset* pxMaterial)
{
	if (pxMaterial)
	{
		m_xMaterial.Set(pxMaterial);
	}
	else
	{
		m_xMaterial.Clear();
	}
}

void Flux_MaterialPreviewImpl::OrbitCamera(float fDeltaYaw, float fDeltaPitch)
{
	m_fCameraYaw += fDeltaYaw;
	m_fCameraPitch = glm::clamp(m_fCameraPitch + fDeltaPitch, -1.5f, 1.5f);
}

void Flux_MaterialPreviewImpl::ZoomCamera(float fDelta)
{
	m_fCameraDistance = glm::clamp(m_fCameraDistance - fDelta * 0.15f, 0.7f, 6.0f);
}

void Flux_MaterialPreviewImpl::OrbitLight(float fDeltaYaw, float fDeltaPitch)
{
	m_fLightYaw += fDeltaYaw;
	m_fLightPitch = glm::clamp(m_fLightPitch + fDeltaPitch, -1.5f, 1.5f);
}

Zenith_MeshGeometryAsset* Flux_MaterialPreviewImpl::GetActiveMeshGeometry()
{
	return m_axMeshes[m_eMesh].GetDirect();
}

Zenith_Maths::Matrix4 Flux_MaterialPreviewImpl::GetActiveMeshModelMatrix() const
{
	switch (m_eMesh)
	{
		case MATERIAL_PREVIEW_MESH_PLANE:
			// Flattened unit cube — a tilted slab reads tiling/normal maps well.
			return glm::scale(Zenith_Maths::Vector3(1.2f, 0.04f, 1.2f));
		case MATERIAL_PREVIEW_MESH_CYLINDER:
			return glm::scale(Zenith_Maths::Vector3(0.8f, 0.8f, 0.8f));
		case MATERIAL_PREVIEW_MESH_SPHERE:
		case MATERIAL_PREVIEW_MESH_CUBE:
		default:
			return Zenith_Maths::Matrix4(1.0f);
	}
}

// Prepare (main thread): rebuild + upload the preview camera's frame
// constants from the orbit state.
void Flux_MaterialPreviewImpl::UploadPreviewViewConstants()
{
	if (!m_bActive) return;

	Flux_GraphicsImpl::ViewConstants& xFC = m_xPreviewViewConstants;

	const float fCosPitch = cosf(m_fCameraPitch);
	const Zenith_Maths::Vector3 xCameraPos(
		m_fCameraDistance * fCosPitch * sinf(m_fCameraYaw),
		m_fCameraDistance * sinf(m_fCameraPitch),
		m_fCameraDistance * fCosPitch * cosf(m_fCameraYaw));

	xFC.m_xViewMat = glm::lookAt(xCameraPos, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xFC.m_xProjMat = glm::perspective(glm::radians(40.0f), 1.0f, 0.05f, 50.0f);
	// Flip Y for Vulkan (same as the editor camera).
	xFC.m_xProjMat[1][1] *= -1.0f;
	xFC.m_xViewProjMat = xFC.m_xProjMat * xFC.m_xViewMat;
	xFC.m_xInvViewProjMat = glm::inverse(xFC.m_xViewProjMat);
	xFC.m_xInvViewMat = glm::inverse(xFC.m_xViewMat);
	xFC.m_xInvProjMat = glm::inverse(xFC.m_xProjMat);
	xFC.m_xCamPos_Pad = Zenith_Maths::Vector4(xCameraPos, 0.0f);

	xFC.m_xScreenDims = { uPREVIEW_SIZE, uPREVIEW_SIZE };
	xFC.m_xRcpScreenDims = { 1.0f / uPREVIEW_SIZE, 1.0f / uPREVIEW_SIZE };
	xFC.m_xCameraNearFar = { 0.05f, 50.0f };

	g_xEngine.FluxMemory().UploadBufferData(m_xPreviewViewConstantsBuffer.GetBuffer().m_xVRAMHandle,
		&xFC, sizeof(Flux_GraphicsImpl::ViewConstants));

	// Rotatable preview light (UE L-drag). g_xSunDir points FROM the light INTO the
	// scene (matches the deferred convention). The sun lives in the GLOBAL set (set 0)
	// — the spine forward shader reads it from g_xGlobalSet.g_xGlobal, not the VIEW set
	// — so write it straight into the GLOBAL buffer (it is NOT a ViewConstants field).
	const float fCosLightPitch = cosf(m_fLightPitch);
	const Zenith_Maths::Vector3 xLightDir = -Zenith_Maths::Vector3(
		fCosLightPitch * sinf(m_fLightYaw),
		sinf(m_fLightPitch),
		fCosLightPitch * cosf(m_fLightYaw));
	m_xPreviewGlobalConstants.m_xSunDir_Pad    = Zenith_Maths::Vector4(glm::normalize(xLightDir), 0.0f);
	m_xPreviewGlobalConstants.m_xSunColour_Pad = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 3.0f);
	g_xEngine.FluxMemory().UploadBufferData(m_xPreviewGlobalConstantsBuffer.GetBuffer().m_xVRAMHandle,
		&m_xPreviewGlobalConstants, sizeof(Flux_GraphicsImpl::GlobalConstants));
}

void Flux_MaterialPreviewImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	// Pass 1: environment background (clears colour + depth for the mesh draw).
	const Flux_PassHandle xBackgroundPass = xGraph.AddPass("MaterialPreview Background", ExecutePreviewBackground)
		.Prepare([](void*){
			Flux_MaterialPreviewImpl& xPrev = g_xEngine.MaterialPreview();
			xPrev.UploadPreviewViewConstants();
			// Register the previewed material with the GPU table (MAIN THREAD), so the
			// mesh pass's worker record reads a valid index + the textures are bindless.
			if (xPrev.IsActive())
			{
				Zenith_MaterialAsset* pxPrevMat = xPrev.GetMaterial();
				if (pxPrevMat) g_xEngine.FluxGraphics().MaterialTable().GetOrCreateIndex(pxPrevMat);
			}
		})
		.Writes(m_xPreviewHDR,   RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_xPreviewDepth, RESOURCE_ACCESS_WRITE_DSV)
		.ClearTargets();
	xGraph.Read(xBackgroundPass, xIBL.m_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);

	// Pass 2: the preview mesh with the edited material, IBL-lit.
	const Flux_PassHandle xMeshPass = xGraph.AddPass("MaterialPreview Mesh", ExecutePreviewMesh)
		.Writes(m_xPreviewHDR,   RESOURCE_ACCESS_WRITE_RTV)
		.Writes(m_xPreviewDepth, RESOURCE_ACCESS_WRITE_DSV);
	xGraph.Read(xMeshPass, xIBL.m_xBRDFLUT,        RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xMeshPass, xIBL.m_xIrradianceMap,  RESOURCE_ACCESS_READ_SRV);
	xGraph.Read(xMeshPass, xIBL.m_xPrefilteredMap, RESOURCE_ACCESS_READ_SRV);

	// The shared Translucent_Forward shader statically references the clustered
	// light buffers, so they must be bound (and therefore graph-declared) even
	// though the preview disables the dynamic-light walk via its constants.
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (xLightClustering.IsInitialised())
	{
		xGraph.ReadBuffer(xMeshPass, xLightClustering.GetClusterLightCountsBuffer().GetBuffer(), RESOURCE_ACCESS_READ_SRV);
		xGraph.ReadBuffer(xMeshPass, xLightClustering.GetClusterLightIndicesBuffer().GetBuffer(), RESOURCE_ACCESS_READ_SRV);
	}

	// The shared forward shader statically references the CSM array (Sampler2DArray);
	// declare a full-array read for graph completeness so the pass is ordered after
	// the cascade writers with a correct WRITE_DSV -> SHADER_READ barrier. Shadows
	// are gated off via the preview constants, but the descriptor + image layout
	// must still be valid (Phase 4b — mirrors the DeferredShading/fog declarations).
	xGraph.ReadTransient(xMeshPass, g_xEngine.Shadows().GetCSMArrayHandle(), RESOURCE_ACCESS_READ_SRV, 0, 1, 0, FLUX_RG_ALL_LAYERS);

	// Pass 3: fixed-exposure tonemap to the ImGui-visible LDR target.
	const Flux_PassHandle xTonemapPass = xGraph.AddPass("MaterialPreview Tonemap", ExecutePreviewTonemap)
		.Writes(m_xPreviewLDR, RESOURCE_ACCESS_WRITE_RTV);
	xGraph.Read(xTonemapPass, m_xPreviewHDR, RESOURCE_ACCESS_READ_SRV);

	// Pass 4: leave the LDR target in SHADER_READ_ONLY for ImGui's sample.
	xGraph.AddPass("MaterialPreview LDR Transition", ExecutePreviewLayoutTransition)
		.Reads(m_xPreviewLDR, RESOURCE_ACCESS_READ_SRV);
}

static void ExecutePreviewBackground(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_MaterialPreviewImpl& xZZ = g_xEngine.MaterialPreview();
	if (!xZZ.IsActive()) return;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	pxCmdList->SetPipeline(&xZZ.m_xBackgroundPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace BG = Flux_Generated_MaterialPreview::MaterialPreview_Background;
	// VIEW (set 1) g_xView is the PERSISTENT main-camera set (Phase 5.1). Restoring the
	// preview's own orbit camera needs a secondary-view override (Phase 5.6, skipped), so
	// the preview currently renders with the main scene camera — an accepted, tools-only
	// regression. The per-pass preview-camera bind here was a no-op (the persistent set
	// wins) and is removed; m_xPreviewViewConstantsBuffer is retained as 5.6 scaffolding.
	// The IBL prefiltered environment doubles as the visible background, so
	// what you see is exactly what lights the mesh.
	xBinder.BindSRV(BG::hg_xCubemap, &g_xEngine.IBL().GetPrefilteredMapSRV());

	pxCmdList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCmdList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
	pxCmdList->DrawIndexed(6);
}

static void ExecutePreviewMesh(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_MaterialPreviewImpl& xZZ = g_xEngine.MaterialPreview();
	if (!xZZ.IsActive()) return;

	Zenith_MaterialAsset* pxMaterial = xZZ.GetMaterial();
	if (!pxMaterial) return;

	Zenith_MeshGeometryAsset* pxMeshAsset = xZZ.GetActiveMeshGeometry();
	Flux_MeshGeometry* pxGeometry = pxMeshAsset ? pxMeshAsset->GetGeometry() : nullptr;
	if (!pxGeometry) return;

	Flux_IBLImpl& xIBL = g_xEngine.IBL();

	const MaterialBlendMode eBlend = pxMaterial->GetResolved().m_xParams.m_eBlendMode;
	Flux_Pipeline* pxPipeline = &xZZ.m_xMeshPipelineOpaque;
	if (eBlend == MATERIAL_BLEND_TRANSLUCENT) pxPipeline = &xZZ.m_xMeshPipelineTranslucent;
	else if (eBlend == MATERIAL_BLEND_ADDITIVE) pxPipeline = &xZZ.m_xMeshPipelineAdditive;
	pxCmdList->SetPipeline(pxPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace FW = Flux_Generated_MaterialPreview::MaterialPreview_Forward;
	// VIEW g_xView (set 1) + GLOBAL g_xGlobal (set 0) are the PERSISTENT main-camera/sun
	// sets (Phase 5.1/5.3). The per-pass preview-camera/sun binds here were no-ops (the
	// persistent sets win) and are removed, so the preview renders with the main scene
	// camera/sun pending the Phase-5.6 secondary-view override (accepted, tools-only).
	// m_xPreview{View,Global}ConstantsBuffer are retained as 5.6 scaffolding.
	// The g_axTextures bindless table (set 2); g_axMaterials lives in the persistent
	// GLOBAL set (Phase 5.3) — no per-draw re-stage.
	pxCmdList->UseBindlessTextures(2);

	// Shared forward-preview constants: IBL on, shadows + dynamic lights off.
	MaterialPreviewPassConstants xConstants;
	xConstants.m_bIBLEnabled = (xIBL.IsEnabled() && xIBL.IsReady()) ? 1 : 0;
	xConstants.m_bIBLDiffuseEnabled = 1;
	xConstants.m_bIBLSpecularEnabled = 1;
	xConstants.m_fIBLIntensity = xIBL.GetIntensity();
	xConstants.m_bShadowsEnabled = 0;
	xConstants.m_bDynamicLightsEnabled = 0;	// preview: IBL + the rotatable sun only
	xConstants.m_fAmbientFallbackIntensity = 0.15f;
	xConstants.m_fCSMTexelSizeX = 1.0f / 2048.0f;
	xConstants.m_fCSMTexelSizeY = 1.0f / 2048.0f;
	xBinder.BindDrawConstants(FW::hTranslucencyConstants, &xConstants, sizeof(xConstants));

	// CSM (g_xCSM) is now in the persistent VIEW set (Phase 5.4) — written once/frame in
	// WritePersistentViewImage, no per-pass bind. The forward shader still samples it
	// (shadows disabled via the constants) and the pass declares the graph Read().
	// The all-cascade ShadowMatrices SSBO is in the persistent VIEW set now (Phase 5.4) —
	// no per-pass bind (preview disables shadow sampling via the constants anyway).

	xBinder.BindSRV(FW::hg_xBRDFLUT, &xIBL.GetBRDFLUTSRV());
	xBinder.BindSRV(FW::hg_xIrradianceMap, &xIBL.GetIrradianceMapSRV());
	xBinder.BindSRV(FW::hg_xPrefilteredMap, &xIBL.GetPrefilteredMapSRV());

	// (Clustered dynamic lights are in the persistent VIEW set now — Phase 5.4; the preview
	// disables dynamic lights via the constants anyway. The mesh pass declares the cluster
	// ReadBuffer decls in SetupRenderGraph for graph-completeness / the VIEW-set validator.)

	// Per-draw constants (model + material-table index). Material textures are bindless
	// (set 2), bound once per pass above; the material was registered in the Background
	// pass Prepare (main thread).
	u_int uMaterialIndex = pxMaterial->GetMaterialTableIndex();
	if (uMaterialIndex == uFLUX_INVALID_MATERIAL_INDEX) uMaterialIndex = 0u;
	MeshDrawConstants xDrawConstants;
	BuildMeshDrawConstants(xDrawConstants, xZZ.GetActiveMeshModelMatrix(), uMaterialIndex);
	xBinder.BindDrawConstants(FW::hDrawConstants, &xDrawConstants, sizeof(xDrawConstants));
	// (g_axMaterials lives in the persistent GLOBAL set now — no per-draw re-stage. Phase 5.3.)

	pxCmdList->SetVertexBuffer(pxGeometry->GetVertexBuffer());
	pxCmdList->SetIndexBuffer(pxGeometry->GetIndexBuffer());
	pxCmdList->DrawIndexed(pxGeometry->GetNumIndices());
}

static void ExecutePreviewTonemap(Flux_CommandBuffer* pxCmdList, void*)
{
	Flux_MaterialPreviewImpl& xZZ = g_xEngine.MaterialPreview();
	if (!xZZ.IsActive()) return;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();

	pxCmdList->SetPipeline(&xZZ.m_xTonemapPipeline);

	Flux_ShaderBinder xBinder(*pxCmdList);
	namespace TM = Flux_Generated_MaterialPreview::MaterialPreview_Tonemap;
	MaterialPreviewTonemapConstants xConstants;
	xConstants.m_fExposure = 1.0f;
	xBinder.BindDrawConstants(TM::hPreviewTonemapConstants, &xConstants, sizeof(xConstants));
	xBinder.BindSRV(TM::hg_xHDRTex, &xZZ.m_xPreviewHDR.SRV());

	pxCmdList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer());
	pxCmdList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
	pxCmdList->DrawIndexed(6);
}

#endif // ZENITH_TOOLS
