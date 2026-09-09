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
//   slot 2+ZENITH_FLUX_NUM_CSMS   = editor ANIMATION preview (tools, on demand)
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
// cascade c's slot; the two PREVIEW-class slots sit after the last cascade, in a
// contiguous range — Flux_IsPreviewViewSlot below is derived from that range
// rather than from a second enumerated list.
// kuFluxViewNumShadowSlots mirrors ZENITH_FLUX_NUM_CSMS — kept as a local
// constant so this header stays dependency-light (Flux_ShadowsImpl.h includes
// this header and static_asserts the two stay in lockstep).
inline constexpr u_int kuFluxViewNumShadowSlots  = 4u;
inline constexpr u_int kuFluxViewSlotMain        = 0u;
inline constexpr u_int kuFluxViewSlotShadowFirst = 1u;

// The editor's MATERIAL preview view — the first of the PREVIEW class.
inline constexpr u_int kuFluxViewSlotPreviewMaterial = 1u + kuFluxViewNumShadowSlots;
// The editor's ANIMATION preview view. Same shape as the material preview — a
// full-pipeline PREVIEW-typed view, INACTIVE until its owner activates it — and
// deliberately the NEXT slot up, so the preview class stays a contiguous range.
inline constexpr u_int kuFluxViewSlotPreviewAnim     = kuFluxViewSlotPreviewMaterial + 1u;

// The PREVIEW class as a RANGE, which is the form anything DERIVED should read.
// No name here means "the preview slot" any more, and that is the point of the
// rename: a singular constant is silently correct while there is one preview view
// and silently WRONG the day there are two, in every caller at once and with no
// diagnostic. The predicate above, the persistent-LDR list in Flux_Graphics.cpp
// and the sky-view-LUT list in Flux_Skybox.cpp are all expressed over the range
// or over the two NAMED slots; nothing spells a preview slot as a number.
inline constexpr u_int kuFluxViewSlotPreviewFirst = kuFluxViewSlotPreviewMaterial;
inline constexpr u_int kuFluxViewNumPreviewSlots  = 2u;
static_assert(kuFluxViewSlotPreviewFirst + kuFluxViewNumPreviewSlots <= FLUX_MAX_RENDER_VIEWS,
	"FLUX_MAX_RENDER_VIEWS must fit main + all shadow cascades + every preview view");
// The named members and the range are ONE fact: adding a third preview slot means
// bumping the count AND naming it, and forgetting either half fails here.
static_assert(kuFluxViewSlotPreviewAnim == kuFluxViewSlotPreviewFirst + kuFluxViewNumPreviewSlots - 1u,
	"the animation preview is the LAST slot of the preview range");

// True iff uSlot is one of the PREVIEW-class views. DERIVED from the contiguous
// range above and NOT a second list: an enumerated list is a place a later slot
// can be forgotten while every existing test stays green. Pinned against the
// registry's own m_eType for every slot by the RenderViews unit
// IsPreviewViewSlotIsTotalAndMatchesTheRegistryTypes — so the predicate and the
// constructor cannot drift apart silently. Total over all u_int inputs
// (out-of-range slots are simply not preview slots).
inline constexpr bool Flux_IsPreviewViewSlot(u_int uSlot)
{
	return uSlot >= kuFluxViewSlotPreviewFirst
		&& uSlot < kuFluxViewSlotPreviewFirst + kuFluxViewNumPreviewSlots;
}

// The preview views' fixed square target size.
//
// TWO THINGS ARE SIZED FROM THIS CONSTANT DIRECTLY and must keep agreeing: the
// persistent preview LDRs (built once at Flux_Graphics::Initialise and never
// rebuilt) and Flux_PreviewBuildViewConstants' screen dims. Everything else —
// every per-view TRANSIENT, and every per-view feature setup (SSR/SSGI/SSAO/HiZ/
// bloom/decals) — reads Flux_GraphicsImpl::GetViewSetupDims, which returns the
// m_xTargetDims the view's OWNER staged. The owners
// (Flux_MaterialPreviewController::Update and
// Zenith_AnimationPreviewSession::UpdatePreviewView) stage
// exactly this constant, which is what makes the two paths agree today; changing
// it in one place only would resize the transients but not the LDR the tonemap
// writes into.
inline constexpr u_int kuFLUX_PREVIEW_VIEW_SIZE = 512u;

// Per-view feature flags (mirrored to shaders as g_uViewFlags in ViewConstants /
// Common/Bindings.slang; consumed CPU-side by feature setup loops).
inline constexpr u_int FLUX_VIEW_FLAG_SHADOWS_ENABLED        = 1u << 0;
inline constexpr u_int FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED = 1u << 1;
inline constexpr u_int FLUX_VIEW_FLAG_SCENE_CONTENT          = 1u << 2;	// scene-derived features (terrain/grass/particles/decals/SDFs/fog) render in this view

// View-shading pipeline variant (Flux Shader System Overhaul — Stage 3a/3b). The
// two lit programs (DeferredShading, Translucency) build one pipeline per mode; a
// specialization-constant permission mask (Stage 3b) lets the compiler strip the
// shadow / cluster-light clauses for views that carry neither. FULL keeps them
// (the main view, flags 0b111); BASIC strips them (shadow-cascade-derived views +
// the material preview, flags 0b000). Stage 3a lands the variant arrays with EMPTY
// spec tables → every variant is byte-identical; Stage 3b bakes the per-mode values.
enum FluxViewShadingMode : u_int
{
	FLUX_VIEW_SHADING_MODE_FULL  = 0,
	FLUX_VIEW_SHADING_MODE_BASIC = 1,
	FLUX_VIEW_SHADING_MODE_COUNT = 2,
};

// Map a view's flag word to its shading variant. A view that permits EITHER the
// shadow or cluster-light clause (the main view) uses FULL; a view carrying
// neither (cascade-derived / material-preview) uses BASIC. Total over all inputs
// (no "mixed" combo to reject): every flag word maps cleanly, and BASIC only ever
// strips clauses the runtime flag would already gate off, so the mapping is
// behaviour-preserving regardless of the global shadow/cluster toggles.
constexpr FluxViewShadingMode Flux_ViewShadingModeFromFlags(u_int uViewFlags)
{
	const u_int uLitMask = FLUX_VIEW_FLAG_SHADOWS_ENABLED | FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED;
	return (uViewFlags & uLitMask) ? FLUX_VIEW_SHADING_MODE_FULL : FLUX_VIEW_SHADING_MODE_BASIC;
}

// View-mask helpers — the per-draw-item mask (Stage S4) is a bit per view slot.
inline constexpr u_int Flux_ViewMaskAllSceneViews(bool bShadowsEnabled)
{
	// Main + (all cascade slots when shadows are on). Built by INCLUSION, so BOTH
	// preview slots are opt-in only: scene content NEVER defaults into either of
	// them, and a preview slot added later cannot leak in by omission.
	u_int uMask = 1u << kuFluxViewSlotMain;
	if (bShadowsEnabled)
	{
		for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { uMask |= 1u << (kuFluxViewSlotShadowFirst + u); }
	}
	return uMask;
}
// The mask carrying exactly one view slot's bit. TOTAL over u_int: an
// out-of-range slot answers 0 (no view at all) rather than shifting past the
// width of the mask, which is undefined and would otherwise be reachable from any
// caller that computed a slot. It replaces the preview-only mask helper that
// stood here, which named ONE slot inside its own identifier and so could not
// express "this preview view" once there were two — every caller that asked for
// a preview mask silently got the material one.
inline constexpr u_int Flux_ViewMaskForSlot(u_int uSlot)
{
	return (uSlot < FLUX_MAX_RENDER_VIEWS) ? (1u << uSlot) : 0u;
}

struct Flux_RenderView
{
	FluxRenderViewType     m_eType         = FLUX_RENDER_VIEW_MAIN;
	bool                   m_bActive       = false;
	// True for views that run the full render pipeline (main + both previews); false
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
