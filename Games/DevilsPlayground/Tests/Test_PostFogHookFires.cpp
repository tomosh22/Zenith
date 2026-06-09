#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RendererImpl.h"
#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// ============================================================================
// PostFogHookFires_Test
//
// Regression test for a late-registration-needs-rebuild differential. Boot order:
//
//   1. g_xEngine.FluxRenderer().LateInitialise()
//        -> InitialiseAllPending() + SetupRenderGraph()
//             (DP_Fog is not registered yet — DPFogPass::Init runs later)
//   2. Project_RegisterScriptBehaviours()
//        -> DPFogPass::Init()
//             -> Zenith_GameRenderFeatures::Register("DP_Fog", runAfter="Fog")
//
// The graph is already built when DP_Fog registers in step 2. The registry's
// late-registration path requests a graph rebuild so SetupDPFog runs before the
// first frame — without it, the DP_Fog pass would never land in the graph and
// the engine fog would never be force-disabled. (Historically this was masked in
// tools builds by incidental Terrain/SSR rebuilds; non-tools surfaced it as
// "fog completely missing".)
//
// Symptom probe (post-refactor): the render graph must, by the first frame, both
//   (a) have owner "Fog" force-disabled  (set inside SetupDPFog), AND
//   (b) contain a pass named "DP_Fog"    (added by SetupDPFog).
// Either being false means the rebuild/anchor path didn't fire.
// ============================================================================

namespace
{
	bool g_bDPFogActiveAfterBoot = false;
}

static void Setup_PostFogHookFires()
{
	g_bDPFogActiveAfterBoot = false;
}

static bool Step_PostFogHookFires(int iFrame)
{
	// One frame is enough: the override + DP_Fog pass are established inside
	// SetupDPFog, which fires from the feature interleave during the graph
	// (re)build that DP_Fog's late registration requested — so by Step's first
	// call both probes must be true.
	if (iFrame == 0)
	{
		if (g_xEngine.FluxRenderer().IsRenderGraphValid())
		{
			const Flux_RenderGraph& xGraph = g_xEngine.FluxRenderer().GetRenderGraph();
			g_bDPFogActiveAfterBoot =
				xGraph.IsOwnerForceDisabled("Fog") && xGraph.FindPass("DP_Fog").IsValid();
		}
		return false;
	}
	return false;
}

static bool Verify_PostFogHookFires()
{
	return g_bDPFogActiveAfterBoot;
}

static const Zenith_AutomatedTest g_xPostFogHookFiresTest = {
	"PostFogHookFires_Test",
	&Setup_PostFogHookFires,
	&Step_PostFogHookFires,
	&Verify_PostFogHookFires,
	8, // max-frames safety net — Step exits on frame 0
	true // m_bRequiresGraphics: post-fog render hook ordering check
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPostFogHookFiresTest);

#endif // ZENITH_INPUT_SIMULATOR
