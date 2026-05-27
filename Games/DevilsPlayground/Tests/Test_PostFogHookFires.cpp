#include "Zenith.h"

#include "Flux/Flux_RendererImpl.h"
#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Flux/Fog/Flux_FogImpl.h"

// ============================================================================
// PostFogHookFires_Test
//
// Regression test for a tools-vs-non-tools differential bug. Boot order is:
//
//   1. g_xEngine.FluxRenderer().LateInitialise()
//        -> SetupRenderGraph()
//             -> Zenith_GameRenderHook::InvokePostFogRegistrations()
//                  (callback list is EMPTY here — DPFogPass has not run yet)
//   2. Project_RegisterScriptBehaviours()
//        -> DPFogPass::Init()
//             -> Zenith_GameRenderHook::RegisterPostFogPass(SetupDPFog)
//
// In step 1 there's no DP callback to fire, and in step 2 the graph has
// already been built. Without an explicit g_xEngine.FluxRenderer().RequestGraphRebuild() in
// DPFogPass::Init, the post-fog hook is never invoked — the DP_Fog pass
// never lands in the render graph and the engine fog override never sticks.
//
// In tools builds this was masked because Terrain / SSR / etc. trigger
// incidental graph rebuilds during gameplay init. Non-tools builds have
// no such rebuild and the bug surfaces as "fog completely missing".
//
// Symptom probe: g_xEngine.Fog().IsExternallyOverridden() — set true only inside
// DPFogPass::SetupDPFog. If the hook fired, the flag is true. If it
// didn't, the flag stayed false from boot.
// ============================================================================

namespace
{
	bool g_bWasOverriddenAfterBoot = false;
}

static void Setup_PostFogHookFires()
{
	g_bWasOverriddenAfterBoot = false;
}

static bool Step_PostFogHookFires(int iFrame)
{
	// One frame is enough: the override flag is set inside SetupDPFog,
	// which fires from InvokePostFogRegistrations during graph (re)build.
	// DPFogPass::Init's RequestGraphRebuild forces the rebuild before the
	// first frame ticks, so by Step's first call the flag must be true.
	if (iFrame == 0)
	{
		g_bWasOverriddenAfterBoot = g_xEngine.Fog().IsExternallyOverridden();
		return false;
	}
	return false;
}

static bool Verify_PostFogHookFires()
{
	return g_bWasOverriddenAfterBoot;
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
