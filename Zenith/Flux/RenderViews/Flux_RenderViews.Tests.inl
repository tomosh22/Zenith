#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/RenderViews/Flux_RenderViews.h"

// ============================================================================
// Flux_RenderViews unit tests — the pure view-registry core: fixed slot layout,
// activation lifecycle (active-set-change detection drives graph rebuilds),
// active-mask / view-count derivation, view-mask helpers, and the full-pipeline
// iteration used by per-view feature setup loops. Headless (no device).
// ============================================================================

ZENITH_TEST(RenderViews, FixedSlotLayoutAndDefaults)
{
	Flux_RenderViewRegistry xReg;
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotMain).m_eType == FLUX_RENDER_VIEW_MAIN, "slot 0 is MAIN");
	ZENITH_ASSERT_TRUE(xReg.IsViewActive(kuFluxViewSlotMain), "MAIN starts active");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotMain).m_bFullPipeline, "MAIN is full-pipeline");
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		const Flux_RenderView& xV = xReg.View(kuFluxViewSlotShadowFirst + u);
		ZENITH_ASSERT_TRUE(xV.m_eType == FLUX_RENDER_VIEW_SHADOW_CASCADE, "slots 1..N are cascades");
		ZENITH_ASSERT_TRUE(!xV.m_bActive, "cascades start inactive");
		ZENITH_ASSERT_TRUE(!xV.m_bFullPipeline, "cascades are depth-only");
	}
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreview).m_eType == FLUX_RENDER_VIEW_PREVIEW, "last fixed slot is the preview");
	ZENITH_ASSERT_TRUE(!xReg.IsViewActive(kuFluxViewSlotPreview), "preview starts inactive");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreview).m_bFullPipeline, "preview is full-pipeline");
}

ZENITH_TEST(RenderViews, ActivationChangeDetection)
{
	Flux_RenderViewRegistry xReg;
	// Activating an inactive slot reports a change; re-activating does not.
	ZENITH_ASSERT_TRUE(xReg.SetViewActive(kuFluxViewSlotPreview, true), "inactive->active is a change");
	ZENITH_ASSERT_TRUE(!xReg.SetViewActive(kuFluxViewSlotPreview, true), "active->active is not a change");
	ZENITH_ASSERT_TRUE(xReg.SetViewActive(kuFluxViewSlotPreview, false), "active->inactive is a change");
	ZENITH_ASSERT_TRUE(!xReg.SetViewActive(kuFluxViewSlotPreview, false), "inactive->inactive is not a change");
	// MAIN can never be deactivated (the request is ignored, no change reported).
	ZENITH_ASSERT_TRUE(!xReg.SetViewActive(kuFluxViewSlotMain, true), "MAIN already active");
}

ZENITH_TEST(RenderViews, ActiveMaskAndCount)
{
	Flux_RenderViewRegistry xReg;
	ZENITH_ASSERT_TRUE(xReg.ActiveViewMask() == (1u << kuFluxViewSlotMain), "only MAIN active at start");
	ZENITH_ASSERT_TRUE(xReg.HighestActiveSlotPlusOne() == 1u, "view count 1 at start");

	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { xReg.SetViewActive(kuFluxViewSlotShadowFirst + u, true); }
	ZENITH_ASSERT_TRUE(xReg.ActiveViewMask() == Flux_ViewMaskAllSceneViews(true), "main+cascades == all-scene-views mask (shadows on)");
	ZENITH_ASSERT_TRUE(xReg.HighestActiveSlotPlusOne() == 1u + kuFluxViewNumShadowSlots, "count spans cascades");

	// Preview active with cascades OFF: the count must still span the preview's
	// fixed slot (view-major GPU slices are indexed by slot, holes skipped by mask).
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { xReg.SetViewActive(kuFluxViewSlotShadowFirst + u, false); }
	xReg.SetViewActive(kuFluxViewSlotPreview, true);
	ZENITH_ASSERT_TRUE(xReg.HighestActiveSlotPlusOne() == kuFluxViewSlotPreview + 1u, "count spans the preview slot despite the cascade hole");
	ZENITH_ASSERT_TRUE(xReg.ActiveViewMask() == ((1u << kuFluxViewSlotMain) | Flux_ViewMaskPreviewOnly()), "mask has the cascade hole");
}

ZENITH_TEST(RenderViews, ViewMaskHelpers)
{
	ZENITH_ASSERT_TRUE(Flux_ViewMaskAllSceneViews(false) == (1u << kuFluxViewSlotMain), "shadows off: scene mask is MAIN only");
	const u_int uOn = Flux_ViewMaskAllSceneViews(true);
	ZENITH_ASSERT_TRUE((uOn & (1u << kuFluxViewSlotMain)) != 0u, "scene mask always has MAIN");
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		ZENITH_ASSERT_TRUE((uOn & (1u << (kuFluxViewSlotShadowFirst + u))) != 0u, "scene mask has every cascade when shadows on");
	}
	ZENITH_ASSERT_TRUE((uOn & Flux_ViewMaskPreviewOnly()) == 0u, "scene content NEVER defaults into the preview view");
	ZENITH_ASSERT_TRUE(Flux_ViewMaskPreviewOnly() == (1u << kuFluxViewSlotPreview), "preview mask is exactly the preview slot bit");
}

namespace
{
	struct RenderViewsIterCtx { u_int m_uCount = 0; u_int m_uMask = 0; };
	void RenderViewsCountFullPipeline(u_int uSlot, const Flux_RenderView&, void* pCtx)
	{
		RenderViewsIterCtx* pxCtx = static_cast<RenderViewsIterCtx*>(pCtx);
		pxCtx->m_uCount++;
		pxCtx->m_uMask |= 1u << uSlot;
	}
}

ZENITH_TEST(RenderViews, ForEachActiveFullPipelineView)
{
	Flux_RenderViewRegistry xReg;
	// Cascades active but depth-only: never enumerated as full-pipeline views.
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { xReg.SetViewActive(kuFluxViewSlotShadowFirst + u, true); }
	RenderViewsIterCtx xCtx;
	xReg.ForEachActiveFullPipelineView(RenderViewsCountFullPipeline, &xCtx);
	ZENITH_ASSERT_TRUE(xCtx.m_uCount == 1u && xCtx.m_uMask == (1u << kuFluxViewSlotMain), "only MAIN is full-pipeline by default");

	xReg.SetViewActive(kuFluxViewSlotPreview, true);
	xCtx = RenderViewsIterCtx();
	xReg.ForEachActiveFullPipelineView(RenderViewsCountFullPipeline, &xCtx);
	ZENITH_ASSERT_TRUE(xCtx.m_uCount == 2u && (xCtx.m_uMask & Flux_ViewMaskPreviewOnly()) != 0u, "active preview joins the full-pipeline walk");
}
