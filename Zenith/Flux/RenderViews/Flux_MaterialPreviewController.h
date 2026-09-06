#pragma once

// TOOLS-only gating mirrors the old Flux_MaterialPreviewImpl exactly: the feature
// is registered under ZENITH_TOOLS in Flux_FeatureRegistry.cpp and reached via the
// ZENITH_TOOLS Zenith_Engine::MaterialPreview() accessor. The vcxproj compiles
// this TU in every config — in non-tools builds the file is empty.
#ifdef ZENITH_TOOLS

#include "Flux/Flux_ViewConstants.h"
#include "Flux/RenderViews/Flux_RenderViews.h"
#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Zenith_MaterialAsset;
class Zenith_MeshGeometryAsset;
class Flux_MeshInstance;
class Flux_RenderGraph;
struct Flux_ShaderResourceView;

enum MaterialPreviewMesh : u_int
{
	MATERIAL_PREVIEW_MESH_SPHERE = 0,
	MATERIAL_PREVIEW_MESH_CUBE,
	MATERIAL_PREVIEW_MESH_PLANE,
	MATERIAL_PREVIEW_MESH_CYLINDER,
	MATERIAL_PREVIEW_MESH_COUNT
};

//------------------------------------------------------------------------------
// Pure preview-orbit math (ported byte-for-byte from the retired offscreen
// renderer's UploadPreviewViewConstants / OrbitCamera / ZoomCamera / OrbitLight
// — Flux/MaterialPreview/Flux_MaterialPreview.cpp). Free functions so they are
// headlessly unit-tested in Flux_MaterialPreviewController.Tests.inl.
//------------------------------------------------------------------------------

// Orbit-input clamps: pitch is held inside (-1.5, 1.5) rad; zoom maps a wheel
// delta onto the orbit distance inside [0.7, 6.0] m around the unit primitive.
inline float Flux_PreviewClampPitch(float fPitch)
{
	return glm::clamp(fPitch, -1.5f, 1.5f);
}

inline float Flux_PreviewApplyZoom(float fDistance, float fDelta)
{
	return glm::clamp(fDistance - fDelta * 0.15f, 0.7f, 6.0f);
}

// Spherical orbit around the origin: yaw about +Y, pitch toward +Y, at the
// given distance. (yaw=0, pitch=0) looks down -Z from (0, 0, distance).
inline Zenith_Maths::Vector3 Flux_PreviewOrbitCameraPos(float fYaw, float fPitch, float fDistance)
{
	const float fCosPitch = cosf(fPitch);
	return Zenith_Maths::Vector3(
		fDistance * fCosPitch * sinf(fYaw),
		fDistance * sinf(fPitch),
		fDistance * fCosPitch * cosf(fYaw));
}

// Rotatable preview light (UE L-drag). The returned direction points FROM the
// light INTO the scene (matches the deferred convention) and is unit length
// (the spherical construction is inherently normalized; glm::normalize guards
// float drift exactly like the old code).
inline Zenith_Maths::Vector3 Flux_PreviewLightDir(float fLightYaw, float fLightPitch)
{
	const float fCosPitch = cosf(fLightPitch);
	const Zenith_Maths::Vector3 xLightDir = -Zenith_Maths::Vector3(
		fCosPitch * sinf(fLightYaw),
		sinf(fLightPitch),
		fCosPitch * cosf(fLightYaw));
	return glm::normalize(xLightDir);
}

// Build the preview view's spine ViewConstants from the orbit state: lookAt the
// origin from the orbit position, 40-deg square perspective over [0.05, 50]
// with the Vulkan Y-flip, all inverses, and the fixed 512^2 target dims. The
// payload is value-initialised first so every field the builder does not write
// (pads / flags / slot / sun — the caller stages those) is deterministically 0.
inline void Flux_PreviewBuildViewConstants(float fYaw, float fPitch, float fDistance, Flux_ViewConstants& xOut)
{
	xOut = Flux_ViewConstants();

	const Zenith_Maths::Vector3 xCameraPos = Flux_PreviewOrbitCameraPos(fYaw, fPitch, fDistance);
	xOut.m_xViewMat = glm::lookAt(xCameraPos, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xOut.m_xProjMat = glm::perspective(glm::radians(40.0f), 1.0f, 0.05f, 50.0f);
	// Flip Y for Vulkan (same as the editor camera).
	xOut.m_xProjMat[1][1] *= -1.0f;
	xOut.m_xViewProjMat    = xOut.m_xProjMat * xOut.m_xViewMat;
	xOut.m_xInvViewProjMat = glm::inverse(xOut.m_xViewProjMat);
	xOut.m_xInvViewMat     = glm::inverse(xOut.m_xViewMat);
	xOut.m_xInvProjMat     = glm::inverse(xOut.m_xProjMat);
	xOut.m_xCamPos_Pad     = Zenith_Maths::Vector4(xCameraPos, 0.0f);

	xOut.m_xScreenDims    = { kuFLUX_PREVIEW_VIEW_SIZE, kuFLUX_PREVIEW_VIEW_SIZE };
	xOut.m_xRcpScreenDims = { 1.0f / kuFLUX_PREVIEW_VIEW_SIZE, 1.0f / kuFLUX_PREVIEW_VIEW_SIZE };
	xOut.m_xRcpOutputDims = xOut.m_xRcpScreenDims;   // preview view is never upscaled (output == render)
	xOut.m_xCameraNearFar = { 0.05f, 50.0f };
}

// TOOLS-only controller behind the Material Editor's live preview (UE-style):
// drives the PREVIEW render view (slot kuFluxViewSlotPreview) through the real
// per-view render pipeline. Replaces the retired offscreen MaterialPreview
// renderer (own passes/pipelines/targets) with pure per-frame staging:
//   - activates/deactivates the preview view (graph rebuild on change),
//   - stages the view's ViewConstants (orbit camera + per-view sun) + dims,
//   - submits ONE external scene item (procedural primitive + edited material,
//     view-masked to the preview slot) into the unified GPU scene / the
//     per-view Translucency path.
// The visible output is the persistent preview LDR owned by Flux_GraphicsImpl
// (written by the per-view tonemap pass), so the SRV handed to ImGui stays
// valid across graph rebuilds.
//
// The public panel-facing API is source-compatible with the old
// Flux_MaterialPreviewImpl (see the alias below).
class Flux_MaterialPreviewController
{
public:
	static constexpr u_int uPREVIEW_SIZE = kuFLUX_PREVIEW_VIEW_SIZE;

	Flux_MaterialPreviewController() = default;
	// Releases the preview slot if this controller still holds it. A destroyed
	// owner left on record would have the arbiter reporting a name for something
	// that no longer exists; the arbiter never dereferences the identity, but the
	// dispossessed panel's placeholder would name a ghost.
	~Flux_MaterialPreviewController() { Flux_PreviewSlotArbiter::Release(this); }

	Flux_MaterialPreviewController(const Flux_MaterialPreviewController&) = delete;
	Flux_MaterialPreviewController& operator=(const Flux_MaterialPreviewController&) = delete;

	// Feature lifecycle (Flux_FeatureRegistry). The controller owns no passes,
	// pipelines or targets — the per-view features add the preview passes — so
	// everything except the asset/mesh teardown is a no-op.
	void Initialise();
	void Shutdown();
	void SetupRenderGraph(Flux_RenderGraph&) {}
	void BuildPipelines() {}

	// Drop every asset pin (procedural primitives + the previewed material) and
	// the mesh instances over them. MUST run in the pre-registry shutdown window
	// (Flux_RendererImpl::ReleaseAssetReferences) — the feature-walk Shutdown
	// runs AFTER Zenith_AssetRegistry::Shutdown has force-deleted the assets, so
	// releasing there would write into freed allocations.
	void ReleaseAssetReferences();

	// Per-frame drive, called from Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot
	// (main thread) BEFORE the sync consumes external items: consumes the liveness
	// flag, (de)activates the preview view, stages its constants and submits the
	// preview mesh.
	void Update();

	//--------------------------------------------------------------------------
	// Panel-facing API (main thread)
	//--------------------------------------------------------------------------

	// The panel calls this every frame it is visible (visibility = liveness).
	// SetActive(true) refreshes the liveness window; Update() deactivates only
	// after kuLIVENESS_GRACE_FRAMES frames without a refresh — so IsActive()
	// stays TRUE for the whole time the panel is open (the DP automation asserts
	// this), and the view (+ its 41 per-view passes / ~32MB of transients) is
	// torn down shortly after the panel closes. SetActive(false) is immediate.
	//
	// ★ THE PREVIEW SLOT IS CLAIMED ON THE TRANSITION INTO ACTIVE, NOT PER FRAME
	// (Flux_PreviewSlotArbiter, last-opened-wins). This is called EVERY frame the
	// panel is visible, so claiming unconditionally would make the material editor
	// impossible to dispossess: it would silently steal the slot back on the next
	// frame and last-opened-wins would degrade to last-drawn-wins. Only the rising
	// edge — the panel opening, or reopening after the grace period expired —
	// claims. m_bActive is NOT touched by the arbitration: IsActive() still means
	// "the panel is open", which is what the DP automation asserts.
	void SetActive(bool bActive)
	{
		if (bActive)
		{
			if (!m_bActive) { Flux_PreviewSlotArbiter::Claim(this, "Material Editor"); }
			m_uFramesSinceLiveness = 0u;
		}
		else if (m_bActive)
		{
			Flux_PreviewSlotArbiter::Release(this);
		}
		m_bActive = bActive;
	}
	bool IsActive() const { return m_bActive; }

	// Does this controller currently own the shared preview view slot? FALSE while
	// an animation preview holds it, in which case Update() neither activates nor
	// stages the view. Distinct from IsActive(): a panel can be open (active) and
	// dispossessed at the same time — that pair is exactly the placeholder state.
	bool HasPreviewSlot() const { return Flux_PreviewSlotArbiter::HasSlot(this); }
	const std::string& GetPreviewSlotOwnerName() const { return Flux_PreviewSlotArbiter::GetOwnerName(); }
	// Take the slot back (the panel's reclaim button), dispossessing the owner.
	void ReclaimPreviewSlot() { Flux_PreviewSlotArbiter::Claim(this, "Material Editor"); }

	void SetMaterial(Zenith_MaterialAsset* pxMaterial);
	Zenith_MaterialAsset* GetMaterial() { return m_xMaterial.GetDirect() ? m_xMaterial.GetDirect() : m_xMaterial.Resolve(); }

	void SetPreviewMesh(MaterialPreviewMesh eMesh) { m_eMesh = eMesh; }
	MaterialPreviewMesh GetPreviewMesh() const { return m_eMesh; }

	// Orbit camera: LMB-drag rotates, wheel zooms.
	void OrbitCamera(float fDeltaYaw, float fDeltaPitch)
	{
		m_fCameraYaw += fDeltaYaw;
		m_fCameraPitch = Flux_PreviewClampPitch(m_fCameraPitch + fDeltaPitch);
	}
	void ZoomCamera(float fDelta) { m_fCameraDistance = Flux_PreviewApplyZoom(m_fCameraDistance, fDelta); }

	// Light direction: L+drag (UE convention).
	void OrbitLight(float fDeltaYaw, float fDeltaPitch)
	{
		m_fLightYaw += fDeltaYaw;
		m_fLightPitch = Flux_PreviewClampPitch(m_fLightPitch + fDeltaPitch);
	}
	void SetLightAngles(float fYaw, float fPitch) { m_fLightYaw = fYaw; m_fLightPitch = fPitch; }
	void GetLightAngles(float& fYaw, float& fPitch) const { fYaw = m_fLightYaw; fPitch = m_fLightPitch; }

	// The ImGui-visible LDR target (persistent — stable across graph rebuilds).
	const Flux_ShaderResourceView& GetPreviewSRV();

	// Model matrix for the active primitive (plane = flattened unit cube;
	// cylinder scaled 0.8 — same values as the old renderer).
	Zenith_Maths::Matrix4 GetActiveMeshModelMatrix() const;

private:
	// Lazily create the four procedural primitives (registry-pinned assets) +
	// one Flux_MeshInstance each for the external-scene-item seam. First active
	// Update() only — never in headless idle.
	void EnsureMeshes();

	bool m_bActive = false;
	// Frames since the last SetActive(true) refresh (see SetActive).
	u_int m_uFramesSinceLiveness = 0u;
	static constexpr u_int kuLIVENESS_GRACE_FRAMES = 8u;

	// Rising/falling-edge tracking for the --preview-test-view diagnostic, which
	// forces the view active WITHOUT a panel and therefore without a SetActive
	// call. It needs the same edge-triggered claim (see SetActive): claiming every
	// frame it is on would make the diagnostic un-dispossessable.
	bool m_bForcedTestViewWasActive = false;

	MaterialHandle m_xMaterial;
	MaterialPreviewMesh m_eMesh = MATERIAL_PREVIEW_MESH_SPHERE;

	// Orbit state (radians; distance in metres around a unit primitive).
	float m_fCameraYaw = 0.6f;
	float m_fCameraPitch = 0.35f;
	float m_fCameraDistance = 1.6f;
	float m_fLightYaw = 0.8f;
	float m_fLightPitch = 0.7f;

	// Pinned procedural preview primitives (registry-cached assets) + the mesh
	// instances the external-item seam draws (procedural-geometry identity, so
	// the unified path shares/derefs them like any snapshot mesh). Instances are
	// destroyed in Shutdown; the geometry GPU buffers belong to the assets.
	bool               m_bMeshesCreated = false;
	MeshGeometryHandle m_axMeshes[MATERIAL_PREVIEW_MESH_COUNT];
	Flux_MeshInstance* m_apxMeshInstances[MATERIAL_PREVIEW_MESH_COUNT] = {};
};

// Source-compatibility alias: the editor panel + DP automation tests were
// written against the old offscreen renderer's type name; the controller is
// API-compatible by construction, so existing call sites compile unchanged.
using Flux_MaterialPreviewImpl = Flux_MaterialPreviewController;

#endif // ZENITH_TOOLS
