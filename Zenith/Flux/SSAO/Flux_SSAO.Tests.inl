#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// SSAO graph-selection + blur-constant unit tests. No GPU, no graph, no engine.
//
// Hosted at the end of Flux_SSAO.cpp (the always-linked feature TU) for two
// reasons: the static-init test registrations survive MSVC dead-strip there,
// and the file-static SSAOBlurConstants / MakeSSAOBlurConstants are in scope.
//
// What these pin:
//   * Flux_SSAOSelection compares on ALL THREE fields. If a field is dropped
//     from the defaulted operator==, ApplySelectionToGraph silently stops
//     requesting the rebuild that keeps DeferredShading's declared Read in
//     step with the SRV ExecuteApplyLighting binds — a stale-descriptor bug
//     with no other tripwire.
//   * The committed handle does NOT move until the next Commit. That one-frame
//     hold is the whole point of the selector: the rebuild is requested on
//     frame N and lands on N+1, and the graph must keep resolving the handle it
//     was actually built with in between.
//   * The reciprocal-texel fill, which the separable H/V passes now depend on
//     (the shader no longer calls GetDimensions).
// ============================================================================

namespace
{
	// Handles are compared by (index, generation, graph-instance); build them
	// directly rather than standing up a graph.
	Flux_TransientHandle SSAOTestHandle(u_int uIndex)
	{
		Flux_TransientHandle xHandle;
		xHandle.m_uIndex           = uIndex;
		xHandle.m_uGeneration      = 1u;
		xHandle.m_uGraphInstanceID = 1u;
		return xHandle;
	}

	constexpr Flux_SSAOSelection kxSSAOTestBaseline{ true, true, 2u };
}

// ---- Flux_SSAOSelection equality -------------------------------------------

ZENITH_TEST(FluxSSAO, SelectionComparesEveryField)
{
	ZENITH_ASSERT_TRUE(kxSSAOTestBaseline == Flux_SSAOSelection({ true, true, 2u }),
		"an identical selection must compare equal");

	// Each field on its own must break equality — a defaulted operator== that
	// loses a member would still pass a same-value test.
	ZENITH_ASSERT_TRUE(!(kxSSAOTestBaseline == Flux_SSAOSelection({ false, true, 2u })),
		"m_bBlurEnabled must participate in equality");
	ZENITH_ASSERT_TRUE(!(kxSSAOTestBaseline == Flux_SSAOSelection({ true, false, 2u })),
		"m_bSeparableBlur must participate in equality");
	ZENITH_ASSERT_TRUE(!(kxSSAOTestBaseline == Flux_SSAOSelection({ true, true, 4u })),
		"m_uResolutionDivisor must participate in equality");
}

// ---- Committed-handle selector round trip ----------------------------------

ZENITH_TEST(FluxSSAO, SelectorCommitsFilteredHandleWhenBlurEnabled)
{
	const Flux_TransientHandle xRaw      = SSAOTestHandle(10u);
	const Flux_TransientHandle xFiltered = SSAOTestHandle(11u);

	Flux_CommittedHandleSelector<Flux_SSAOSelection> xSelector;
	xSelector.Commit(xFiltered, xRaw, /*bEnabled*/ true, kxSSAOTestBaseline);
	ZENITH_ASSERT_TRUE(xSelector.GetCommittedHandle() == xFiltered,
		"blur on must commit the filtered handle");

	xSelector.Commit(xFiltered, xRaw, /*bEnabled*/ false, Flux_SSAOSelection({ false, true, 2u }));
	ZENITH_ASSERT_TRUE(xSelector.GetCommittedHandle() == xRaw,
		"blur off must commit the raw handle");
}

ZENITH_TEST(FluxSSAO, SelectorRequestsRebuildOnAnySelectionChange)
{
	const Flux_TransientHandle xRaw      = SSAOTestHandle(20u);
	const Flux_TransientHandle xFiltered = SSAOTestHandle(21u);

	Flux_CommittedHandleSelector<Flux_SSAOSelection> xSelector;
	xSelector.Commit(xFiltered, xRaw, true, kxSSAOTestBaseline);

	ZENITH_ASSERT_FALSE(xSelector.RequestRebuildIfSelectionChanged(kxSSAOTestBaseline),
		"an unchanged selection must not request a rebuild");
	ZENITH_ASSERT_TRUE(xSelector.RequestRebuildIfSelectionChanged(Flux_SSAOSelection({ false, true, 2u })),
		"toggling the blur must request a rebuild");
	ZENITH_ASSERT_TRUE(xSelector.RequestRebuildIfSelectionChanged(Flux_SSAOSelection({ true, false, 2u })),
		"switching separable/non-separable must request a rebuild");
	ZENITH_ASSERT_TRUE(xSelector.RequestRebuildIfSelectionChanged(Flux_SSAOSelection({ true, true, 4u })),
		"changing the resolution divisor must request a rebuild");
}

ZENITH_TEST(FluxSSAO, SelectorHoldsCommittedHandleUntilNextCommit)
{
	const Flux_TransientHandle xRaw      = SSAOTestHandle(30u);
	const Flux_TransientHandle xFiltered = SSAOTestHandle(31u);

	Flux_CommittedHandleSelector<Flux_SSAOSelection> xSelector;
	xSelector.Commit(xFiltered, xRaw, true, kxSSAOTestBaseline);

	// The frame a toggle lands: a rebuild is requested, but the graph in flight
	// was built against the OLD selection, so the committed handle must not move
	// until SetupRenderGraph re-commits. DeferredShading declared that handle.
	const Flux_SSAOSelection xLive{ false, true, 2u };
	ZENITH_ASSERT_TRUE(xSelector.RequestRebuildIfSelectionChanged(xLive),
		"precondition: the live selection differs");
	ZENITH_ASSERT_TRUE(xSelector.GetCommittedHandle() == xFiltered,
		"the committed handle must survive the rebuild request unchanged");

	xSelector.Commit(xFiltered, xRaw, false, xLive);
	ZENITH_ASSERT_TRUE(xSelector.GetCommittedHandle() == xRaw,
		"the re-commit is what moves the handle");
	ZENITH_ASSERT_FALSE(xSelector.RequestRebuildIfSelectionChanged(xLive),
		"after re-committing, the same live selection is quiet again");
}

// ---- Blur-constant reciprocal-texel fill ------------------------------------

ZENITH_TEST(FluxSSAO, BlurConstantsFillReciprocalTexelSize)
{
	SSAOBlurConstants xBase;
	xBase.m_fSpatialSigma = 1.5f;
	xBase.m_fDepthSigma   = 0.02f;
	xBase.m_fNormalSigma  = 0.2f;
	xBase.m_uKernelRadius = 3u;

	// Half-res of 1920x1080.
	const SSAOBlurConstants xHalf = MakeSSAOBlurConstants(xBase, 960u, 540u);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.m_fRcpTexelWidth,  1.0f / 960.0f, 1e-9f, "1/width");
	ZENITH_ASSERT_EQ_FLOAT(xHalf.m_fRcpTexelHeight, 1.0f / 540.0f, 1e-9f, "1/height");

	// Non-square (the 512² preview at quarter res) must not share one axis.
	const SSAOBlurConstants xOblong = MakeSSAOBlurConstants(xBase, 128u, 64u);
	ZENITH_ASSERT_EQ_FLOAT(xOblong.m_fRcpTexelWidth,  1.0f / 128.0f, 1e-9f, "1/width, oblong");
	ZENITH_ASSERT_EQ_FLOAT(xOblong.m_fRcpTexelHeight, 1.0f /  64.0f, 1e-9f, "1/height, oblong");

	// The tunables pass through untouched — only the two texel fields move.
	ZENITH_ASSERT_EQ_FLOAT(xOblong.m_fSpatialSigma, xBase.m_fSpatialSigma, 1e-9f, "spatial sigma passthrough");
	ZENITH_ASSERT_EQ_FLOAT(xOblong.m_fDepthSigma,   xBase.m_fDepthSigma,   1e-9f, "depth sigma passthrough");
	ZENITH_ASSERT_EQ_FLOAT(xOblong.m_fNormalSigma,  xBase.m_fNormalSigma,  1e-9f, "normal sigma passthrough");
	ZENITH_ASSERT_EQ(xOblong.m_uKernelRadius, xBase.m_uKernelRadius, "kernel radius passthrough");
}

ZENITH_TEST(FluxSSAO, BlurConstantsMatchSlangCBLayout)
{
	// The Slang side is float,float,float,uint,float2,float2 — the float2 lands
	// at offset 16, so the two scalar reciprocals must occupy 16 and 20.
	static_assert(sizeof(SSAOBlurConstants) == 32, "CB size");
	ZENITH_ASSERT_EQ(u_int(offsetof(SSAOBlurConstants, m_fRcpTexelWidth)),  16u,
		"u_xRcpTexelSize.x must land at offset 16");
	ZENITH_ASSERT_EQ(u_int(offsetof(SSAOBlurConstants, m_fRcpTexelHeight)), 20u,
		"u_xRcpTexelSize.y must land at offset 20");
	ZENITH_ASSERT_EQ(u_int(sizeof(SSAOBlurConstants)), 32u,
		"the CB must stay 32 bytes to match SSAOBlurConstants_CB in Generated/SSAO.h");
}
