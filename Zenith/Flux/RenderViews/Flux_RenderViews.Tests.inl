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
	// The main view writes the SHARED final render target the swapchain presents,
	// so it owns no persistent LDR of its own. That is what keeps its tonemap the
	// separate, metering pass declared outside HDR's per-view walk.
	ZENITH_ASSERT_TRUE(!xReg.View(kuFluxViewSlotMain).m_bOwnsPersistentLDR, "MAIN writes the shared final RT, not an LDR of its own");
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		const Flux_RenderView& xV = xReg.View(kuFluxViewSlotShadowFirst + u);
		ZENITH_ASSERT_TRUE(xV.m_eType == FLUX_RENDER_VIEW_SHADOW_CASCADE, "slots 1..N are cascades");
		ZENITH_ASSERT_TRUE(!xV.m_bActive, "cascades start inactive");
		ZENITH_ASSERT_TRUE(!xV.m_bFullPipeline, "cascades are depth-only");
	}
	// The FIRST slot of the preview range is the MATERIAL preview. Spelled through
	// the named constant rather than a number: the range's base is free to move
	// with kuFluxViewNumShadowSlots and this clause must move with it.
	ZENITH_ASSERT_TRUE(kuFluxViewSlotPreviewMaterial == kuFluxViewSlotPreviewFirst,
		"the material preview is the first slot of the preview range");
	// ★ PREVIEW-CLASS IS NO LONGER A VIEW KIND, SO THE CLAUSE MOVED TO THE PROPERTY
	// THAT REPLACED IT. These two lines used to assert the slot was
	// FLUX_RENDER_VIEW_PREVIEW-typed; that enumerator described where the view's
	// image GOES, which is not a kind of view, so it is a per-view property now.
	// The thing worth pinning was never the enum name — it is that the registry
	// stamps these slots as owning their own output, which is exactly what makes
	// HDR give them a tonemap.
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewMaterial).m_bOwnsPersistentLDR, "the material preview owns its own persistent LDR");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewMaterial).m_eType == FLUX_RENDER_VIEW_MAIN,
		"...and is not a distinct view KIND — it renders the main view's pipeline");
	ZENITH_ASSERT_TRUE(!xReg.IsViewActive(kuFluxViewSlotPreviewMaterial), "the material preview starts inactive");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewMaterial).m_bFullPipeline, "the material preview is full-pipeline");
	// The ANIMATION preview: exactly the material preview's shape, and INACTIVE —
	// the default registry must still have precisely one active full-pipeline view.
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewAnim).m_bOwnsPersistentLDR, "the animation preview owns its own persistent LDR");
	ZENITH_ASSERT_TRUE(!xReg.IsViewActive(kuFluxViewSlotPreviewAnim), "the animation preview starts inactive");
	ZENITH_ASSERT_TRUE(xReg.View(kuFluxViewSlotPreviewAnim).m_bFullPipeline, "the animation preview is full-pipeline");
	ZENITH_ASSERT_EQ(xReg.View(kuFluxViewSlotPreviewAnim).m_uViewFlags, 0u, "the animation preview carries no view flags");
}

ZENITH_TEST(RenderViews, ActivationChangeDetection)
{
	Flux_RenderViewRegistry xReg;
	// Activating an inactive slot reports a change; re-activating does not.
	ZENITH_ASSERT_TRUE(xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, true), "inactive->active is a change");
	ZENITH_ASSERT_TRUE(!xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, true), "active->active is not a change");
	ZENITH_ASSERT_TRUE(xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, false), "active->inactive is a change");
	ZENITH_ASSERT_TRUE(!xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, false), "inactive->inactive is not a change");
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
	xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, true);
	ZENITH_ASSERT_TRUE(xReg.HighestActiveSlotPlusOne() == kuFluxViewSlotPreviewMaterial + 1u, "count spans the preview slot despite the cascade hole");
	ZENITH_ASSERT_TRUE(xReg.ActiveViewMask() == (Flux_ViewMaskForSlot(kuFluxViewSlotMain) | Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial)), "mask has the cascade hole");
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
	// Restated over EVERY preview slot, not over "the preview view". The singular
	// form of this clause was true and useless the moment there were two: it would
	// have stayed green with scene content leaking into the animation preview.
	for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++)
	{
		const u_int uPreviewSlot = kuFluxViewSlotPreviewFirst + u;
		ZENITH_ASSERT_TRUE((uOn & Flux_ViewMaskForSlot(uPreviewSlot)) == 0u,
			"scene content NEVER defaults into preview slot %u", uPreviewSlot);
		ZENITH_ASSERT_TRUE((Flux_ViewMaskAllSceneViews(false) & Flux_ViewMaskForSlot(uPreviewSlot)) == 0u,
			"shadows off: scene content NEVER defaults into preview slot %u", uPreviewSlot);
	}
}

// Flux_ViewMaskForSlot replaced the preview-only mask helper, so it is now the
// ONE helper every per-slot mask is built from — which makes "one bit, the right bit,
// and never a scene bit for a preview slot" a property of the helper rather than
// of each call site. Total over u_int: an out-of-range slot must answer 0 rather
// than shift by more than the mask is wide.
ZENITH_TEST(RenderViews, ViewMaskForSlotIsTotalAndDisjoint)
{
	u_int uUnion = 0u;
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		const u_int uMask = Flux_ViewMaskForSlot(u);
		ZENITH_ASSERT_EQ(uMask, 1u << u, "slot %u's mask is exactly its own bit", u);
		ZENITH_ASSERT_EQ(uMask & uUnion, 0u, "slot %u's bit overlaps a lower slot's", u);
		uUnion |= uMask;
	}
	ZENITH_ASSERT_EQ(uUnion, (1u << FLUX_MAX_RENDER_VIEWS) - 1u, "the eight slot masks tile the whole mask exactly");

	// Out of range is 0, not a wrapped shift.
	ZENITH_ASSERT_EQ(Flux_ViewMaskForSlot(FLUX_MAX_RENDER_VIEWS), 0u, "a slot past the registry has no bit");
	ZENITH_ASSERT_EQ(Flux_ViewMaskForSlot(~0u), 0u, "the helper is total — no wraparound yields a bit");

	// And the scene mask overlaps no preview slot's bit, in either shadow state.
	const u_int uSceneOn  = Flux_ViewMaskAllSceneViews(true);
	const u_int uSceneOff = Flux_ViewMaskAllSceneViews(false);
	for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++)
	{
		const u_int uPreviewMask = Flux_ViewMaskForSlot(kuFluxViewSlotPreviewFirst + u);
		ZENITH_ASSERT_EQ(uSceneOn & uPreviewMask,  0u, "shadows on: the scene mask carries no preview bit");
		ZENITH_ASSERT_EQ(uSceneOff & uPreviewMask, 0u, "shadows off: the scene mask carries no preview bit");
	}
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
	ZENITH_ASSERT_TRUE(xCtx.m_uCount == 1u && xCtx.m_uMask == Flux_ViewMaskForSlot(kuFluxViewSlotMain), "only MAIN is full-pipeline by default");

	xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, true);
	xCtx = RenderViewsIterCtx();
	xReg.ForEachActiveFullPipelineView(RenderViewsCountFullPipeline, &xCtx);
	ZENITH_ASSERT_TRUE(xCtx.m_uCount == 2u && (xCtx.m_uMask & Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial)) != 0u, "active preview joins the full-pipeline walk");
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
		xReg.SetViewActive(kuFluxViewSlotPreviewMaterial, bMaterialActive);
		xReg.SetViewActive(kuFluxViewSlotPreviewAnim,     bAnimActive);
		RenderViewsIterCtx xCtx;
		xReg.ForEachActiveFullPipelineView(RenderViewsCountFullPipeline, &xCtx);
		return xCtx;
	}
}

ZENITH_TEST(RenderViews, FullPipelineWalkYieldsExactlyTheActivePreviewSet)
{
	const u_int uMain     = Flux_ViewMaskForSlot(kuFluxViewSlotMain);
	const u_int uMaterial = Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial);
	const u_int uAnim     = Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim);

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
	// Built by walking the RANGE, so a third preview slot is covered here the
	// moment kuFluxViewNumPreviewSlots moves — an enumerated pair would not be.
	u_int uPreviewBits = 0u;
	for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++) { uPreviewBits |= Flux_ViewMaskForSlot(kuFluxViewSlotPreviewFirst + u); }
	ZENITH_ASSERT_EQ(uPreviewBits,
		Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial) | Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim),
		"the preview range covers exactly the two NAMED preview slots");

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
// the ctor's own stamping — the two are then a single fact, not two.
//
// ★ IT USED TO BE PINNED AGAINST m_eType == FLUX_RENDER_VIEW_PREVIEW, and the
// obvious move when that enumerator was deleted was to delete this with it. That
// would have been wrong: the drift this catches has nothing to do with the enum.
// What it says is that the SLOT-LAYOUT predicate every derived list is written
// over and the registry's own per-slot stamping describe the same set of slots —
// and the stamping is now m_bOwnsPersistentLDR, so that is what it reads.
ZENITH_TEST(RenderViews, IsPreviewViewSlotIsTotalAndMatchesTheRegistryOwnership)
{
	Flux_RenderViewRegistry xReg;
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		const bool bRegistrySaysPreview  = xReg.View(u).m_bOwnsPersistentLDR;
		const bool bPredicateSaysPreview = Flux_IsPreviewViewSlot(u);
		ZENITH_ASSERT_TRUE(bPredicateSaysPreview == bRegistrySaysPreview,
			"slot %u: Flux_IsPreviewViewSlot says %d, the registry's own-LDR stamping says %d",
			u, static_cast<int>(bPredicateSaysPreview), static_cast<int>(bRegistrySaysPreview));
	}

	// The range's ENDS, pinned at compile time — the runtime walk above only
	// covers 0..FLUX_MAX_RENDER_VIEWS-1, and the predicate is total over u_int.
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotMain), "slot 0 is not a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewFirst - 1u), "the last cascade is not a preview slot");
	static_assert(Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewMaterial), "the material preview is a preview slot");
	static_assert(Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewAnim), "the animation preview is a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(kuFluxViewSlotPreviewAnim + 1u), "the preview range ENDS at the animation preview");
	static_assert(!Flux_IsPreviewViewSlot(FLUX_MAX_RENDER_VIEWS), "an out-of-range slot is not a preview slot");
	static_assert(!Flux_IsPreviewViewSlot(~0u), "the predicate is total — no wraparound admits a huge slot");
}

// ----------------------------------------------------------------------------
// TWO DESTINATIONS, ONE SETUP. The registry half of "a view's destination must
// not change its pass list".
//
// Every per-view feature setup loop in the engine — HDR's bloom + tonemap walk,
// HiZ, SSAO, SSR, SSGI, Decals, deferred shading — decides what to instantiate
// for a view by reading the FIELDS below off Flux_RenderView. So if the two
// preview views agree on every one of them, no feature can give them different
// pass chains; the only thing left that differs between them is the SLOT, and a
// slot selects a destination (Flux_GraphicsImpl::GetPreviewLDR(uSlot)) and
// nothing else. That is the property that MAKES their pass lists identical.
//
// It is deliberately not a claim about the live graph, which this unit cannot
// reach (a pass list needs a compiled Flux_RenderGraph and a live
// Flux_GraphicsImpl). The live half is RT_RenderGraphViewStructure clause 5,
// which runs the SAME golden inventory over both preview slots of sample C and
// compares the two views against each other. This one runs in every game's boot
// suite on every backend, which is where a registry-level divergence would
// actually be introduced.
//
// ★ THE MAIN VIEW IS NOT A COUNTEREXAMPLE and is deliberately excluded. It
// differs from a preview view in more than its destination: it meters exposure,
// it writes the shared final RT, it carries the shadow / cluster-light / scene-
// content flags, and it is sized from the output dims rather than its staged
// target dims. "Destination alone changes nothing" is a claim about two views
// that are otherwise the same, and these two are the only such pair.
ZENITH_TEST(RenderViews, TheTwoPreviewViewsDifferOnlyInTheirDestination)
{
	Flux_RenderViewRegistry xReg;

	static_assert(kuFluxViewNumPreviewSlots >= 2u,
		"this clause compares preview views against each other — it needs at least two");

	const u_int uReference = kuFluxViewSlotPreviewFirst;
	const Flux_RenderView& xReference = xReg.View(uReference);

	// Walked over the RANGE rather than the two named slots, so a third preview
	// view is held to the same equality the day it is added.
	for (u_int u = 1u; u < kuFluxViewNumPreviewSlots; u++)
	{
		const u_int uSlot = kuFluxViewSlotPreviewFirst + u;
		const Flux_RenderView& xView = xReg.View(uSlot);

		ZENITH_ASSERT_TRUE(xView.m_eType == xReference.m_eType,
			"slot %u and slot %u disagree on view KIND — a destination is not a kind", uSlot, uReference);
		ZENITH_ASSERT_TRUE(xView.m_bFullPipeline == xReference.m_bFullPipeline,
			"slot %u and slot %u disagree on m_bFullPipeline, which decides whether a view is visited at all", uSlot, uReference);
		ZENITH_ASSERT_TRUE(xView.m_bOwnsPersistentLDR == xReference.m_bOwnsPersistentLDR,
			"slot %u and slot %u disagree on m_bOwnsPersistentLDR, which decides the tonemap + LDR-transition pair", uSlot, uReference);
		// ★ THIS PAIR PINS THE CONSTRUCTOR DEFAULT, NOT THE WORD THE PIPELINE READS,
		// and the difference is worth stating because the obvious reading is wrong.
		// Production gates the shadow / cluster-light / scene-content chains on
		// `View(uSlot).m_xConstants.m_uViewFlags` (Flux_DeferredShading.cpp,
		// Flux_Translucency.cpp), which each preview's OWNER stages per frame;
		// `m_uViewFlags` itself is written once at construction and read by nobody
		// (Flux_UnifiedMesh.cpp says so in as many words). So what these two clauses
		// establish is that the two slots LEAVE THE CONSTRUCTOR identical, which is
		// this unit's whole remit -- it holds a default-constructed registry and
		// cannot see a per-frame stage. A divergence introduced at stage time is
		// invisible here and is caught by RT_RenderGraphViewStructure's clause 5b,
		// which compares the two views' LIVE pass inventories.
		ZENITH_ASSERT_EQ(xView.m_uViewFlags, xReference.m_uViewFlags,
			"slot %u and slot %u leave the constructor with different view flags", uSlot, uReference);
		ZENITH_ASSERT_TRUE(Flux_ViewShadingModeFromFlags(xView.m_uViewFlags) == Flux_ViewShadingModeFromFlags(xReference.m_uViewFlags),
			"slot %u and slot %u resolve to different shading variants", uSlot, uReference);
		ZENITH_ASSERT_TRUE(xView.m_xTargetDims.x == xReference.m_xTargetDims.x && xView.m_xTargetDims.y == xReference.m_xTargetDims.y,
			"slot %u and slot %u start at different target dims — every per-view transient is sized from these", uSlot, uReference);
		ZENITH_ASSERT_TRUE(xView.m_bActive == xReference.m_bActive,
			"slot %u and slot %u start at different activation — each is raised by its own owner, never here", uSlot, uReference);

		// ...and the ONE thing that does differ. The destination is not a field of
		// the view at all: it is reached as GetPreviewLDR(uSlot), so "a different
		// destination" IS "a different slot" and can be nothing else. If these ever
		// collided, both previews would tonemap into one image.
		ZENITH_ASSERT_TRUE(uSlot != uReference, "each preview view must occupy a slot of its own");
		ZENITH_ASSERT_TRUE(Flux_ViewMaskForSlot(uSlot) != Flux_ViewMaskForSlot(uReference),
			"slot %u and slot %u must carry distinct view-mask bits, or content masked to one reaches the other", uSlot, uReference);
		ZENITH_ASSERT_TRUE(Flux_IsPreviewViewSlot(uSlot),
			"slot %u is inside the preview range it was derived from", uSlot);
	}
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
