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
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreview).m_eType == FLUX_RENDER_VIEW_PREVIEW, "slot 5 is the MATERIAL preview");
	ZENITH_ASSERT_TRUE(!xReg.IsViewActive(kuFluxViewSlotPreview), "the material preview starts inactive");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreview).m_bFullPipeline, "the material preview is full-pipeline");
	// Slot 6 is the ANIMATION preview: exactly slot 5's shape, and INACTIVE — the
	// default registry must still have precisely one active full-pipeline view.
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewAnim).m_eType == FLUX_RENDER_VIEW_PREVIEW, "slot 6 is the ANIMATION preview");
	ZENITH_ASSERT_TRUE(!xReg.IsViewActive(kuFluxViewSlotPreviewAnim), "the animation preview starts inactive");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewAnim).m_bFullPipeline, "the animation preview is full-pipeline");
	ZENITH_ASSERT_EQ(xReg.View(kuFluxViewSlotPreviewAnim).m_uViewFlags, 0u, "the animation preview carries no view flags");
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

// ----------------------------------------------------------------------------
// Slot 6 — the ANIMATION preview (unit D2-a). Slot 6 is admissible: it carries
// slot 5's exact shape (PREVIEW type, full pipeline) and is INACTIVE by default,
// so every existing walk is byte-for-byte unchanged until an owner activates it.
//
// The tests below assert the EXACT mask, never `(mask & bit) != 0`: a walk that
// enumerated an extra view would satisfy an any-bit-set assertion, which is the
// precise failure a second preview slot introduces.
// ----------------------------------------------------------------------------
namespace
{
	// The (count, mask) the full-pipeline walk yields with the two preview slots
	// activated as given. The CASCADES ARE ON in every case: they are active but
	// depth-only, so a walk that leaked a non-full-pipeline view shows up in the
	// mask rather than being masked out by the arrangement of the fixture.
	RenderViewsIterCtx RenderViewsWalkWithPreviews(bool bMaterialActive, bool bAnimActive)
	{
		Flux_RenderViewRegistry xReg;
		for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { xReg.SetViewActive(kuFluxViewSlotShadowFirst + u, true); }
		xReg.SetViewActive(kuFluxViewSlotPreview,     bMaterialActive);
		xReg.SetViewActive(kuFluxViewSlotPreviewAnim, bAnimActive);
		RenderViewsIterCtx xCtx;
		xReg.ForEachActiveFullPipelineView(RenderViewsCountFullPipeline, &xCtx);
		return xCtx;
	}
}

ZENITH_TEST(RenderViews, FullPipelineWalkYieldsExactlyTheActivePreviewSet)
{
	const u_int uMain     = 1u << kuFluxViewSlotMain;
	const u_int uMaterial = 1u << kuFluxViewSlotPreview;
	const u_int uAnim     = 1u << kuFluxViewSlotPreviewAnim;

	// {0} — neither preview active. This is the default registry's shape, and it
	// is the clause that makes slot 6 a NO-OP for every walk landed before D2-a.
	const RenderViewsIterCtx xNone = RenderViewsWalkWithPreviews(false, false);
	ZENITH_ASSERT_EQ(xNone.m_uMask,  uMain, "{0}: the walk is EXACTLY the main view");
	ZENITH_ASSERT_EQ(xNone.m_uCount, 1u,    "{0}: exactly one full-pipeline view");

	// {0,5} — the material preview only, the shape the engine has today.
	const RenderViewsIterCtx xMaterial = RenderViewsWalkWithPreviews(true, false);
	ZENITH_ASSERT_EQ(xMaterial.m_uMask,  uMain | uMaterial, "{0,5}: main + the material preview, and nothing else");
	ZENITH_ASSERT_EQ(xMaterial.m_uCount, 2u,                "{0,5}: exactly two full-pipeline views");

	// {0,6} — the animation preview WITHOUT the material preview. Slot 5 being
	// inactive must not gate slot 6 (a `<= 5` bound or an early-out on the first
	// inactive preview would fail exactly here and nowhere else).
	const RenderViewsIterCtx xAnim = RenderViewsWalkWithPreviews(false, true);
	ZENITH_ASSERT_EQ(xAnim.m_uMask,  uMain | uAnim, "{0,6}: main + the animation preview, with the slot-5 hole");
	ZENITH_ASSERT_EQ(xAnim.m_uCount, 2u,            "{0,6}: exactly two full-pipeline views");

	// {0,5,6} — both previews at once, which is what makes the per-view pass
	// chains have to be distinct rather than merely parameterised.
	const RenderViewsIterCtx xBoth = RenderViewsWalkWithPreviews(true, true);
	ZENITH_ASSERT_EQ(xBoth.m_uMask,  uMain | uMaterial | uAnim, "{0,5,6}: main + both previews, and nothing else");
	ZENITH_ASSERT_EQ(xBoth.m_uCount, 3u,                        "{0,5,6}: exactly three full-pipeline views");
}

ZENITH_TEST(RenderViews, SceneViewMaskExcludesBothPreviewSlots)
{
	const u_int uPreviewBits = (1u << kuFluxViewSlotPreview) | (1u << kuFluxViewSlotPreviewAnim);

	const u_int uShadowsOn = Flux_ViewMaskAllSceneViews(true);
	ZENITH_ASSERT_EQ(uShadowsOn & uPreviewBits, 0u, "shadows on: scene content reaches NEITHER preview slot");
	// Asserted against the exact expected mask too, so the clause above cannot
	// pass by the whole scene mask having collapsed to zero.
	u_int uExpectedOn = 1u << kuFluxViewSlotMain;
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++) { uExpectedOn |= 1u << (kuFluxViewSlotShadowFirst + u); }
	ZENITH_ASSERT_EQ(uShadowsOn, uExpectedOn, "shadows on: the scene mask is EXACTLY main + every cascade");

	const u_int uShadowsOff = Flux_ViewMaskAllSceneViews(false);
	ZENITH_ASSERT_EQ(uShadowsOff & uPreviewBits, 0u, "shadows off: scene content reaches NEITHER preview slot");
	ZENITH_ASSERT_EQ(uShadowsOff, 1u << kuFluxViewSlotMain, "shadows off: the scene mask is EXACTLY main");
}

ZENITH_TEST(RenderViews, HighestActiveSlotPlusOneSeesSlotSix)
{
	Flux_RenderViewRegistry xReg;
	// Only slot 6 active besides MAIN: the cascades and slot 5 are both holes, so
	// a count derived from "number of active views" rather than from the HIGHEST
	// active slot would answer 2 instead of 7 and undersize every view-major slice.
	xReg.SetViewActive(kuFluxViewSlotPreviewAnim, true);
	ZENITH_ASSERT_EQ(xReg.HighestActiveSlotPlusOne(), kuFluxViewSlotPreviewAnim + 1u, "the view count spans the animation-preview slot");
	ZENITH_ASSERT_EQ(xReg.HighestActiveSlotPlusOne(), 7u, "...which is 7 with today's fixed layout");
	ZENITH_ASSERT_EQ(xReg.ActiveViewMask(), (1u << kuFluxViewSlotMain) | (1u << kuFluxViewSlotPreviewAnim),
		"the active mask carries the cascade AND slot-5 holes");
}

// Flux_IsPreviewViewSlot is derived from the preview RANGE, so the way it can go
// wrong is not "a slot is missing from a list" but "the range and the registry
// constructor disagree". This walks EVERY slot and holds the predicate against
// the ctor's own m_eType — the two are then a single fact, not two.
ZENITH_TEST(RenderViews, IsPreviewViewSlotIsTotalAndMatchesTheRegistryTypes)
{
	Flux_RenderViewRegistry xReg;
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		const bool bRegistrySaysPreview  = xReg.View(u).m_eType == FLUX_RENDER_VIEW_PREVIEW;
		const bool bPredicateSaysPreview = Flux_IsPreviewViewSlot(u);
		ZENITH_ASSERT_TRUE(bPredicateSaysPreview == bRegistrySaysPreview,
			"slot %u: Flux_IsPreviewViewSlot says %d, the registry type says %d",
			u, static_cast<int>(bPredicateSaysPreview), static_cast<int>(bRegistrySaysPreview));
	}

	// The range's ENDS, pinned at compile time — the runtime walk above only
	// covers 0..FLUX_MAX_RENDER_VIEWS-1, and the predicate is total over u_int.
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotMain), "slot 0 is not a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotPreview - 1u), "the last cascade is not a preview slot");
	static_assert(Flux_IsPreviewViewSlot(kuFluxViewSlotPreview), "the material preview is a preview slot");
	static_assert(Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewAnim), "the animation preview is a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewAnim + 1u), "the preview range ENDS at the animation preview");
	static_assert(!Flux_IsPreviewViewSlot(FLUX_MAX_RENDER_VIEWS), "an out-of-range slot is not a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(~0u), "the predicate is total — no wraparound admits a huge slot");
}

// ----------------------------------------------------------------------------
// FluxViewShadingMode selector (Flux Shader System Overhaul — Stage 3a). Maps a
// view's flag word to the FULL/BASIC pipeline variant: FULL when the view permits
// EITHER shadows or cluster lights (the main view), BASIC when it carries neither
// (shadow-cascade-derived + material-preview views). Total over all inputs.
// ----------------------------------------------------------------------------
ZENITH_TEST(RenderViews, ViewShadingModeFromFlags)
{
	static_assert(FLUX_VIEW_SHADING_MODE_COUNT == 2u, "two shading variants (FULL, BASIC)");

	// Main view (0b111): scene content + shadows + cluster -> FULL.
	const u_int uMain = FLUX_VIEW_FLAG_SHADOWS_ENABLED | FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED | FLUX_VIEW_FLAG_SCENE_CONTENT;
	ZENITH_ASSERT_EQ(Flux_ViewShadingModeFromFlags(uMain), FLUX_VIEW_SHADING_MODE_FULL, "main view (0b111) -> FULL");

	// Cascade / preview (0b000): no shadow/cluster/scene flags -> BASIC.
	ZENITH_ASSERT_EQ(Flux_ViewShadingModeFromFlags(0u), FLUX_VIEW_SHADING_MODE_BASIC, "flags 0 -> BASIC");

	// Scene content WITHOUT the lit clauses (e.g. shadows globally off, no cluster)
	// still strips → BASIC (behaviour-preserving: the runtime flag would gate them off anyway).
	ZENITH_ASSERT_EQ(Flux_ViewShadingModeFromFlags(FLUX_VIEW_FLAG_SCENE_CONTENT), FLUX_VIEW_SHADING_MODE_BASIC, "scene-content-only -> BASIC");

	// Either lit clause alone is enough for FULL.
	ZENITH_ASSERT_EQ(Flux_ViewShadingModeFromFlags(FLUX_VIEW_FLAG_SHADOWS_ENABLED), FLUX_VIEW_SHADING_MODE_FULL, "shadows-only -> FULL");
	ZENITH_ASSERT_EQ(Flux_ViewShadingModeFromFlags(FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED), FLUX_VIEW_SHADING_MODE_FULL, "cluster-only -> FULL");
}
