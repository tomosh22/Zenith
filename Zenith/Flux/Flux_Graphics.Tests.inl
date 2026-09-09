#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the two out-of-contract slot getters assert on purpose

// ============================================================================
// Flux_GraphicsImpl unit tests — the per-view dims derivation (GetViewSetupDims)
// and the per-slot persistent preview LDRs (GetPreviewLDR).
//
// These run against the LIVE graphics object, not a fresh instance: both things
// under test are properties of the booted renderer (the LDRs are built once in
// Initialise; the dims read the live registry + the render-scale latch). The
// suite runs after the first SetupRenderGraph, so both are populated.
//
// Every test obtains the object through g_xEngine.TryGetFluxGraphics() and
// RETURNS SILENTLY when it is null — a run with no Flux at all (a boot-time unit
// batch on a build with no renderer) must not assert. A `.Tests.inl` is outside
// the engine-singleton ratchet, so naming g_xEngine here costs nothing.
//
// NOTHING HERE CALLS SetupTransients. The graph is live while the suite runs, so
// a second setup walk would append a duplicate set of transients to it. Where a
// test needs different dims it STAGES the inputs the getter reads and restores
// them; the getter is a pure read, so no rebuild is involved.
// ============================================================================

namespace
{
	// Save/restore for the MAIN view's render-scale latch. Both fields are public
	// on Flux_GraphicsImpl (it has no private section), and both are read by
	// GetRenderDims — which is what GetViewSetupDims(0) forwards to. Restoring
	// through a destructor rather than by hand means a failed assertion partway
	// through can never leave the live renderer believing it is upscaling.
	struct Flux_ScopedRenderDimsLatch
	{
		explicit Flux_ScopedRenderDimsLatch(Flux_GraphicsImpl& xGraphics)
			: m_xGraphics(xGraphics)
			, m_bUpscalingActive(xGraphics.m_bUpscalingActive)
			, m_xRenderDimsThisBuild(xGraphics.m_xRenderDimsThisBuild)
		{
		}
		~Flux_ScopedRenderDimsLatch()
		{
			m_xGraphics.m_bUpscalingActive     = m_bUpscalingActive;
			m_xGraphics.m_xRenderDimsThisBuild = m_xRenderDimsThisBuild;
		}

		Flux_ScopedRenderDimsLatch(const Flux_ScopedRenderDimsLatch&) = delete;
		Flux_ScopedRenderDimsLatch& operator=(const Flux_ScopedRenderDimsLatch&) = delete;

		Flux_GraphicsImpl&     m_xGraphics;
		bool                   m_bUpscalingActive;
		Zenith_Maths::UVector2 m_xRenderDimsThisBuild;
	};

	// Save/restore for one view slot's staged target dims.
	struct Flux_ScopedViewTargetDims
	{
		Flux_ScopedViewTargetDims(Flux_GraphicsImpl& xGraphics, u_int uSlot)
			: m_xGraphics(xGraphics)
			, m_uSlot(uSlot)
			, m_xTargetDims(xGraphics.RenderViews().View(uSlot).m_xTargetDims)
		{
		}
		~Flux_ScopedViewTargetDims()
		{
			m_xGraphics.RenderViews().View(m_uSlot).m_xTargetDims = m_xTargetDims;
		}

		Flux_ScopedViewTargetDims(const Flux_ScopedViewTargetDims&) = delete;
		Flux_ScopedViewTargetDims& operator=(const Flux_ScopedViewTargetDims&) = delete;

		Flux_GraphicsImpl&     m_xGraphics;
		u_int                  m_uSlot;
		Zenith_Maths::UVector2 m_xTargetDims;
	};
}

// Slot 0 must resolve THROUGH GetRenderDims(), not through the registry (nobody
// stages slot 0's m_xTargetDims) and not through the swapchain directly (that
// would drop the temporal-upscaling render-scale latch). Asserted against
// GetRenderDims() rather than a literal: the Null swapchain is 1280x720 today,
// but the assertion under test is the EQUALITY, not the number.
ZENITH_TEST(Graphics, ViewSetupDimsMainIsRenderDims)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	const Zenith_Maths::UVector2 xRender = pxGraphics->GetRenderDims();
	const Zenith_Maths::UVector2 xSetup  = pxGraphics->GetViewSetupDims(kuFluxViewSlotMain);
	ZENITH_ASSERT_EQ(xSetup.x, xRender.x, "slot 0 setup width == GetRenderDims().x");
	ZENITH_ASSERT_EQ(xSetup.y, xRender.y, "slot 0 setup height == GetRenderDims().y");
	ZENITH_ASSERT_TRUE(xSetup.x > 0u && xSetup.y > 0u, "the main view is never sized 0");
}

// ...and it must FOLLOW that latch, which is the whole reason slot 0 is special-
// cased. Staging the latched pair is exactly what SetupTransients does at the top
// of a build; the getter is a pure read of it, so no rebuild is needed (and one
// must not be triggered — see the file header).
ZENITH_TEST(Graphics, ViewSetupDimsMainFollowsRenderScale)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	Flux_ScopedRenderDimsLatch xRestore(*pxGraphics);

	pxGraphics->m_bUpscalingActive     = true;
	pxGraphics->m_xRenderDimsThisBuild = Zenith_Maths::UVector2(640u, 360u);

	const Zenith_Maths::UVector2 xSetup = pxGraphics->GetViewSetupDims(kuFluxViewSlotMain);
	ZENITH_ASSERT_EQ(xSetup.x, 640u, "slot 0 follows the upscaling render-dims latch");
	ZENITH_ASSERT_EQ(xSetup.y, 360u, "slot 0 follows the upscaling render-dims latch");
}

// A non-main slot reads the m_xTargetDims its OWNER staged — not the preview size
// constant, which is only what today's two owners happen to stage. Staged twice
// with different values so the test cannot pass on a hardcoded 512.
ZENITH_TEST(Graphics, ViewSetupDimsPreviewReadsStagedTargetDims)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	Flux_ScopedViewTargetDims xRestore(*pxGraphics, kuFluxViewSlotPreviewMaterial);

	pxGraphics->RenderViews().View(kuFluxViewSlotPreviewMaterial).m_xTargetDims = Zenith_Maths::UVector2(512u, 512u);
	Zenith_Maths::UVector2 xSetup = pxGraphics->GetViewSetupDims(kuFluxViewSlotPreviewMaterial);
	ZENITH_ASSERT_EQ(xSetup.x, 512u, "the preview slot reads its staged target dims");
	ZENITH_ASSERT_EQ(xSetup.y, 512u, "the preview slot reads its staged target dims");

	pxGraphics->RenderViews().View(kuFluxViewSlotPreviewMaterial).m_xTargetDims = Zenith_Maths::UVector2(256u, 128u);
	xSetup = pxGraphics->GetViewSetupDims(kuFluxViewSlotPreviewMaterial);
	ZENITH_ASSERT_EQ(xSetup.x, 256u, "it FOLLOWS the staged dims, non-square included");
	ZENITH_ASSERT_EQ(xSetup.y, 128u, "it FOLLOWS the staged dims, non-square included");
}

// An owner that activates a view without staging its size is a hard authoring
// error: every consumer downstream would create a 0x0 transient or divide by
// zero, a long way from the mistake. This is the assert SetupTransients' loop
// used to spell out inline, now covering the feature setups too. The animation-
// preview slot is used as the subject because it is inactive and unused, so
// nothing observes the zeroed value.
ZENITH_TEST(Graphics, ViewSetupDimsZeroAsserts)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	Flux_ScopedViewTargetDims xRestore(*pxGraphics, kuFluxViewSlotPreviewAnim);

	pxGraphics->RenderViews().View(kuFluxViewSlotPreviewAnim).m_xTargetDims = Zenith_Maths::UVector2(0u, 0u);
	{
		Zenith_AssertCaptureScope xCapture;
		const Zenith_Maths::UVector2 xSetup = pxGraphics->GetViewSetupDims(kuFluxViewSlotPreviewAnim);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "zero staged dims assert exactly once");
		ZENITH_ASSERT_EQ(xSetup.x, 0u, "the value is still returned verbatim after the assert");
		ZENITH_ASSERT_EQ(xSetup.y, 0u, "the value is still returned verbatim after the assert");
	}
}

// Both preview-class slots own a BUILT, DISTINCT persistent LDR. Distinctness is
// meaningful headless because the Null memory manager hands out monotonically
// increasing dummy VRAM handles, so two separate allocations can never collide;
// a builder that wrote both entries from one allocation would show up here.
ZENITH_TEST(Graphics, PreviewLDRsAreDistinctAndBuilt)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	const Flux_RenderAttachment& xLDRMaterial = pxGraphics->GetPreviewLDR(kuFluxViewSlotPreviewMaterial);
	const Flux_RenderAttachment& xLDRAnim     = pxGraphics->GetPreviewLDR(kuFluxViewSlotPreviewAnim);

	ZENITH_ASSERT_TRUE(xLDRMaterial.m_xVRAMHandle.IsValid(), "the material-preview LDR is built at Initialise");
	ZENITH_ASSERT_TRUE(xLDRAnim.m_xVRAMHandle.IsValid(), "the animation-preview LDR is built at Initialise too");
	ZENITH_ASSERT_TRUE(xLDRMaterial.m_xVRAMHandle != xLDRAnim.m_xVRAMHandle, "the two LDRs are separate allocations");
	ZENITH_ASSERT_TRUE(&xLDRMaterial != &xLDRAnim, "and separate array entries");

	ZENITH_ASSERT_EQ(xLDRMaterial.m_xSurfaceInfo.m_uWidth,  kuFLUX_PREVIEW_VIEW_SIZE, "the material-preview LDR is the fixed preview size");
	ZENITH_ASSERT_EQ(xLDRMaterial.m_xSurfaceInfo.m_uHeight, kuFLUX_PREVIEW_VIEW_SIZE, "the material-preview LDR is the fixed preview size");
	ZENITH_ASSERT_EQ(xLDRAnim.m_xSurfaceInfo.m_uWidth,      kuFLUX_PREVIEW_VIEW_SIZE, "the animation-preview LDR is the fixed preview size");
	ZENITH_ASSERT_EQ(xLDRAnim.m_xSurfaceInfo.m_uHeight,     kuFLUX_PREVIEW_VIEW_SIZE, "the animation-preview LDR is the fixed preview size");

	// EVERY slot of the preview range owns one, not just the two named above: the
	// LDR list and the range are separate facts by design (see the note above
	// kuFLUX_PREVIEW_LDR_SLOTS), and this is the clause that would notice a third
	// preview slot typed into the registry with no allocation behind it.
	for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++)
	{
		const u_int uSlot = kuFluxViewSlotPreviewFirst + u;
		ZENITH_ASSERT_TRUE(pxGraphics->GetPreviewLDR(uSlot).m_xVRAMHandle.IsValid(),
			"preview slot %u owns a built persistent LDR", uSlot);
	}
}

// A non-preview slot has no built entry, so asking for one is a caller bug. The
// getter asserts and — because Zenith_Assert falls through rather than returning
// — must still hand back a USABLE attachment, which is the material-preview
// slot's. Without that, a shipping build with asserts compiled out would bind a
// default-constructed attachment.
ZENITH_TEST(Graphics, PreviewLDRMainSlotAsserts)
{
	Flux_GraphicsImpl* pxGraphics = g_xEngine.TryGetFluxGraphics();
	if (pxGraphics == nullptr) { return; }

	const Flux_RenderAttachment* pxFallback = nullptr;
	{
		Zenith_AssertCaptureScope xCapture;
		pxFallback = &pxGraphics->GetPreviewLDR(kuFluxViewSlotMain);
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "a non-preview slot asserts exactly once");
	}
	ZENITH_ASSERT_TRUE(pxFallback == &pxGraphics->GetPreviewLDR(kuFluxViewSlotPreviewMaterial),
		"the fall-through returns the material-preview slot's attachment");
}
