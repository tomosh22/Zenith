#pragma once

#include "Flux/Flux_ViewConstants.h"
#include "Maths/Zenith_Maths.h"

// =====================================================================
// Flux_RenderViews — the first-class render-view registry.
//
// A render view = a camera (ViewConstants payload) + an object set (per-draw-
// item view masks in the unified GPU scene) + a render-target set + one
// persistent VIEW descriptor-set instance per frame-in-flight slot (binding 0
// per-view; bindings 1-8 replicated-shared — see Flux_ViewSetBinding.h).
//
// Views live in FIXED slots so GPU addressing (cull frustum-plane blocks,
// visible-index/indirect slices) never re-indexes when a view toggles:
//   slot 0                        = MAIN camera (always active)
//   slots 1..ZENITH_FLUX_NUM_CSMS = shadow cascades (active iff shadows enabled)
//   slot 1+ZENITH_FLUX_NUM_CSMS   = editor material preview (tools, on demand)
//
// The registry is a pure CPU fixed-slot array (no device deps) owned by
// Flux_GraphicsImpl — unit-tested headless in Flux_RenderViews.Tests.inl.
// Constants staging: Shadows fills the cascade slots' ViewConstants each frame
// (UpdateShadowMatrices); the preview controller fills the preview slot; view 0
// is filled from FrameConstants by UploadFrameConstants. Flux_GraphicsImpl
// uploads every ACTIVE slot's payload to its per-view frame-indexed CB.
// =====================================================================

// Fixed capacity of the view registry — sizes the per-frame persistent VIEW
// descriptor-set array, the per-view constant buffers, and (Stage S4) the
// unified cull's frustum-plane block + visible/indirect buffer slices.
inline constexpr u_int FLUX_MAX_RENDER_VIEWS = 8u;

enum FluxRenderViewType : u_int
{
	FLUX_RENDER_VIEW_MAIN = 0,
	FLUX_RENDER_VIEW_SHADOW_CASCADE,
	FLUX_RENDER_VIEW_PREVIEW,
};

// Fixed slot assignments (see header comment). kuFluxViewSlotShadowFirst + c is
// cascade c's slot; the preview sits after the last cascade.
// kuFluxViewNumShadowSlots mirrors ZENITH_FLUX_NUM_CSMS — kept as a local
// constant so this header stays dependency-light (Flux_ShadowsImpl.h includes
// this header and static_asserts the two stay in lockstep).
inline constexpr u_int kuFluxViewNumShadowSlots  = 4u;
inline constexpr u_int kuFluxViewSlotMain        = 0u;
inline constexpr u_int kuFluxViewSlotShadowFirst = 1u;
inline constexpr u_int kuFluxViewSlotPreview     = 1u + kuFluxViewNumShadowSlots;
static_assert(kuFluxViewSlotPreview < FLUX_MAX_RENDER_VIEWS,
	"FLUX_MAX_RENDER_VIEWS must fit main + all shadow cascades + the preview view");

// The material-preview view's fixed square target size (G-buffer/HDR transients
// + the persistent LDR the editor samples).
inline constexpr u_int kuFLUX_PREVIEW_VIEW_SIZE = 512u;

// Per-view feature flags (mirrored to shaders as g_uViewFlags in ViewConstants /
// Common/Bindings.slang; consumed CPU-side by feature setup loops).
inline constexpr u_int FLUX_VIEW_FLAG_SHADOWS_ENABLED        = 1u << 0;
inline constexpr u_int FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED = 1u << 1;
inline constexpr u_int FLUX_VIEW_FLAG_SCENE_CONTENT          = 1u << 2;	// scene-derived features (terrain/grass/particles/decals/SDFs/fog) render in this view

// View-mask helpers — the per-draw-item mask (Stage S4) is a bit per view slot.
inline constexpr u_int Flux_ViewMaskAllSceneViews(bool bShadowsEnabled)
{
	// Main + (all cascade slots when shadows are on). The preview slot is opt-in
	// only — scene content NEVER defaults into it.
	u_int uMask = 1u << kuFluxViewSlotMain;
	if (bShadowsEnabled)
	{
		for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { uMask |= 1u << (kuFluxViewSlotShadowFirst + u); }
	}
	return uMask;
}
inline constexpr u_int Flux_ViewMaskPreviewOnly() { return 1u << kuFluxViewSlotPreview; }

struct Flux_RenderView
{
	FluxRenderViewType     m_eType         = FLUX_RENDER_VIEW_MAIN;
	bool                   m_bActive       = false;
	// True for views that run the full render pipeline (main + preview); false
	// for depth-only shadow views. Feature setup loops iterate full-pipeline
	// views when instantiating per-view pass chains (Stage S5).
	bool                   m_bFullPipeline = false;
	u_int                  m_uViewFlags    = 0u;
	Zenith_Maths::UVector2 m_xTargetDims   = Zenith_Maths::UVector2(0u, 0u);
	// The per-view spine constants payload, staged CPU-side by the view's owner
	// each frame and uploaded to the slot's frame-indexed CB by Flux_GraphicsImpl.
	Flux_ViewConstants     m_xConstants;
	// World-space frustum planes for the unified GPU cull (Stage S4): left/right/
	// bottom/top/near/far, extracted by the view's owner from m_xViewProjMat.
	Zenith_Maths::Vector4  m_axFrustumPlanes[6] = {};
};

class Flux_RenderViewRegistry
{
public:
	Flux_RenderViewRegistry();

	Flux_RenderView&       View(u_int uSlot);
	const Flux_RenderView& View(u_int uSlot) const;

	// Activate/deactivate a slot. Returns true iff the ACTIVE SET changed —
	// the caller (renderer) must then RequestGraphRebuild() so per-view passes
	// (Stage S5) are added/removed. Slot 0 (MAIN) can never be deactivated.
	bool SetViewActive(u_int uSlot, bool bActive);

	// Bit i set ⇔ slot i active.
	u_int ActiveViewMask() const;
	// 1 + the highest active slot index — the GPU cull's uNumViews (view-major
	// buffer slices are indexed by SLOT, so inactive holes are skipped by mask).
	u_int HighestActiveSlotPlusOne() const;

	bool IsViewActive(u_int uSlot) const { return View(uSlot).m_bActive; }

	// Invoke pfn(uSlot, xView) for every active full-pipeline view, in slot order.
	// Captureless fn-ptr + context (no std::function per engine convention).
	void ForEachActiveFullPipelineView(void (*pfn)(u_int uSlot, const Flux_RenderView&, void* pCtx), void* pCtx) const;

private:
	Flux_RenderView m_axViews[FLUX_MAX_RENDER_VIEWS];
};
