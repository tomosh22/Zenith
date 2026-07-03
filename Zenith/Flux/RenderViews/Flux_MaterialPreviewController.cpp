#include "Zenith.h"

// TOOLS-only (mirrors the retired Flux_MaterialPreview renderer's gating): in
// non-tools configs this TU compiles empty.
#ifdef ZENITH_TOOLS

#include "Flux/RenderViews/Flux_MaterialPreviewController.h"

#include "Core/Zenith_Engine.h"
#include "Flux/Flux_GraphicsImpl.h"                 // RenderViews() + MaterialTable() + GetPreviewLDR()
#include "Flux/Flux_RendererImpl.h"                 // SubmitExternalSceneItem + RequestGraphRebuild
#include "Flux/MeshGeometry/Flux_MeshInstance.h"    // CreateFromGeometry / Destroy
#include "AssetHandling/Zenith_MeshGeometryAsset.h" // procedural preview primitives
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstring>   // std::strcmp — the --preview-test-view CLI check

// Diagnostic toggle (moved verbatim from the retired S5a test driver in
// Flux_GPUSceneBuilder.cpp): forces the preview view active with the default
// sphere + null (blank) material so a windowed capture/smoke harness can
// exercise the whole preview chain without opening the editor panel.
DEBUGVAR bool dbg_bPreviewTestView = false;

void Flux_MaterialPreviewController::Initialise()
{
	// Nothing to create up front: the preview targets are per-view transients +
	// the FluxGraphics-owned persistent LDR, the passes belong to the per-view
	// features, and the primitives are lazily built on first active Update().
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_MaterialPreviewController initialised");
}

void Flux_MaterialPreviewController::ReleaseAssetReferences()
{
	// Runs in the PRE-REGISTRY shutdown window (Flux_RendererImpl::
	// ReleaseAssetReferences, before Zenith_AssetRegistry::Shutdown force-deletes
	// every remaining asset). Releasing the pins any later would Release() into
	// freed allocations — the same too-late-Clear rule documented at the
	// Flux_Graphics release site. Mesh instances are destroyed here too: their
	// non-owning procedural-geometry back-references dangle once the registry
	// deletes the pinned mesh-geometry assets.
	for (u_int u = 0; u < MATERIAL_PREVIEW_MESH_COUNT; u++)
	{
		if (m_apxMeshInstances[u] != nullptr)
		{
			m_apxMeshInstances[u]->Destroy();
			delete m_apxMeshInstances[u];
			m_apxMeshInstances[u] = nullptr;
		}
		m_axMeshes[u].Clear();
	}
	m_bMeshesCreated = false;
	m_xMaterial.Clear();
	m_bActive = false;
}

void Flux_MaterialPreviewController::Shutdown()
{
	// Asset pins + mesh instances were already dropped in ReleaseAssetReferences
	// (pre-registry window); the calls below are idempotent no-ops that only
	// matter if a future path shuts the feature down without that window.
	ReleaseAssetReferences();
}

void Flux_MaterialPreviewController::SetMaterial(Zenith_MaterialAsset* pxMaterial)
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

const Flux_ShaderResourceView& Flux_MaterialPreviewController::GetPreviewSRV()
{
	// The persistent preview LDR (written by the per-view tonemap pass) — NOT a
	// transient, so the reference handed to ImGui stays valid across rebuilds.
	return g_xEngine.FluxGraphics().GetPreviewLDR().SRV();
}

Zenith_Maths::Matrix4 Flux_MaterialPreviewController::GetActiveMeshModelMatrix() const
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

void Flux_MaterialPreviewController::EnsureMeshes()
{
	if (m_bMeshesCreated) { return; }
	m_bMeshesCreated = true;

	// Same procedural primitives as the old renderer, pinned via registry
	// handles. The asset creators GPU-upload the geometry; the instances proxy
	// those buffers (procedural identity), so the unified path resolves them
	// through the mesh-geometry registry like any snapshot mesh and the
	// translucent path binds them directly.
	m_axMeshes[MATERIAL_PREVIEW_MESH_SPHERE].Set(Zenith_MeshGeometryAsset::CreateUnitSphere(32));
	m_axMeshes[MATERIAL_PREVIEW_MESH_CUBE].Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	m_axMeshes[MATERIAL_PREVIEW_MESH_PLANE].Set(Zenith_MeshGeometryAsset::CreateUnitCube());	// flattened via the model matrix
	m_axMeshes[MATERIAL_PREVIEW_MESH_CYLINDER].Set(Zenith_MeshGeometryAsset::CreateUnitCylinder(32));

	for (u_int u = 0; u < MATERIAL_PREVIEW_MESH_COUNT; u++)
	{
		Zenith_MeshGeometryAsset* pxAsset = m_axMeshes[u].GetDirect();
		Flux_MeshGeometry* pxGeometry = pxAsset ? pxAsset->GetGeometry() : nullptr;
		if (pxGeometry != nullptr)
		{
			m_apxMeshInstances[u] = Flux_MeshInstance::CreateFromGeometry(pxGeometry);
		}
	}
}

void Flux_MaterialPreviewController::Update()
{
#ifdef ZENITH_DEBUG_VARIABLES
	static bool ls_bRegistered = false;
	if (!ls_bRegistered)
	{
		ls_bRegistered = true;
		g_xEngine.DebugVariables().AddBoolean({ "Render", "MultiView", "Preview Test View" }, dbg_bPreviewTestView);
	}
#endif

	// Liveness window: the panel refreshes via SetActive(true) every frame it is
	// open; when the refreshes stop (panel closed) the flag drops after the grace
	// period. IsActive() therefore reads TRUE throughout an open panel's life —
	// the DP automation asserts exactly that — while a closed panel still tears
	// the view down within a few frames.
	if (m_bActive && ++m_uFramesSinceLiveness > kuLIVENESS_GRACE_FRAMES)
	{
		m_bActive = false;
	}
	bool bActive = m_bActive;

	bool bForcedTestView = dbg_bPreviewTestView;
#ifdef ZENITH_WINDOWS
	// Diagnostic CLI override: --preview-test-view forces the toggle on so a
	// capture/smoke harness can exercise the preview chain without the debug
	// panel. ORs into the local (DEBUGVAR is const in non-debug-var configs).
	{
		static int s_iCLIOverride = -2;
		if (s_iCLIOverride == -2)
		{
			s_iCLIOverride = -1;
			for (int i = 1; i < __argc; i++)
			{
				if (std::strcmp(__argv[i], "--preview-test-view") == 0) { s_iCLIOverride = 1; }
			}
		}
		if (s_iCLIOverride == 1) { bForcedTestView = true; }
	}
#endif
	bActive |= bForcedTestView;

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	Flux_RenderViewRegistry& xViews = xGraphics.RenderViews();
	if (xViews.SetViewActive(kuFluxViewSlotPreview, bActive))
	{
		// The active view set changed: per-view transients + passes must be
		// (de)declared, so the next frame recompiles the graph from scratch.
		g_xEngine.FluxRenderer().RequestGraphRebuild();
	}
	if (!bActive) { return; }

	// Stage the preview view's constants from the orbit state: camera via the
	// pure builder, then the per-view sun (colour (1,1,1,3) like the old
	// preview), flags 0 (no shadows/clusters/scene content) and the fixed slot.
	Flux_RenderView& xView = xViews.View(kuFluxViewSlotPreview);
	xView.m_xTargetDims = Zenith_Maths::UVector2(kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE);

	Flux_ViewConstants& xVC = xView.m_xConstants;
	Flux_PreviewBuildViewConstants(m_fCameraYaw, m_fCameraPitch, m_fCameraDistance, xVC);
	xVC.m_xSunDir_Pad    = Zenith_Maths::Vector4(Flux_PreviewLightDir(m_fLightYaw, m_fLightPitch), 0.0f);
	xVC.m_xSunColour_Pad = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 3.0f);
	xVC.m_uViewFlags     = 0u;
	xVC.m_uViewSlot      = kuFluxViewSlotPreview;
	// TAA NoJitter: the preview view never jitters and never runs velocity/TAA, but the
	// GPU cull reads m_xViewProjMatNoJitter for EVERY active view — so stage it to this
	// view's own (unjittered) view-proj (prev == current; jitter UV = 0).
	xVC.m_xViewProjMatNoJitter     = xVC.m_xViewProjMat;
	xVC.m_xPrevViewProjMatNoJitter = xVC.m_xViewProjMat;
	xVC.m_xJitterUV_PrevJitterUV   = Zenith_Maths::Vector4(0.0f);

	EnsureMeshes();
	Flux_MeshInstance* pxMesh = m_apxMeshInstances[m_eMesh];
	if (pxMesh == nullptr) { return; }

	// Register the previewed material with the GPU table (MAIN THREAD) so the
	// draw's worker record reads a valid index + the textures are bindless
	// (mirrors the old Background-pass Prepare). Null material -> blank (the
	// sync substitutes it), no registration needed.
	Zenith_MaterialAsset* pxMaterial = GetMaterial();
	if (pxMaterial != nullptr)
	{
		xGraphics.MaterialTable().GetOrCreateIndex(pxMaterial);
	}

	// ONE external item, view-masked to the preview slot: opaque materials ride
	// the unified GPU scene; translucent/additive divert to the per-view
	// Translucency gather. The controller keeps mesh + material alive.
	Flux_RendererImpl::Flux_ExternalSceneItem xItem;
	xItem.m_xWorldMatrix   = GetActiveMeshModelMatrix();
	xItem.m_pxMeshInstance = pxMesh;
	xItem.m_pxMaterial     = pxMaterial;
	xItem.m_uViewMask      = Flux_ViewMaskPreviewOnly();
	g_xEngine.FluxRenderer().SubmitExternalSceneItem(xItem);
}

#include "Flux/RenderViews/Flux_MaterialPreviewController.Tests.inl"

#endif // ZENITH_TOOLS
