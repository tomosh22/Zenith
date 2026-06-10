#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_GraphicsOptions.h"

// ============================================================================
// SSGIDenoiseToggle_Test
//
// Regression test for the runtime screen-space-effect output toggles.
// Flipping Graphics/SSGI/Denoise (or Flux/SSR/RoughnessBlur) at runtime
// requests a render-graph rebuild that only lands on the NEXT frame; the
// frame in between records against the old graph. GetSSGISRV /
// GetReflectionSRV must therefore resolve from the COMMITTED transient
// handle (the one Flux_DeferredShading declared its Read on), not the live
// graphics option — otherwise the deferred pass binds an undeclared
// resource and AssertBoundResourceDeclared kills the process.
//
// The test passes by surviving the toggle cycles: each flip is followed by
// several rendered frames (the crash frame, pre-fix, is the first frame
// recorded after the flip). Both toggle directions are exercised for both
// effects.
// ============================================================================

namespace
{
	bool g_bCompletedAllPhases = false;
}

static void Setup_SSGIDenoiseToggle()
{
	g_bCompletedAllPhases = false;
	// Start from the defaults the boot-time graph was built with.
	Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled      = true;
	Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled = true;
}

static bool Step_SSGIDenoiseToggle(int iFrame)
{
	Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	switch (iFrame)
	{
	case 10: xOpts.m_bSSGIDenoiseEnabled      = false; break;
	case 20: xOpts.m_bSSGIDenoiseEnabled      = true;  break;
	case 30: xOpts.m_bSSRRoughnessBlurEnabled = false; break;
	case 40: xOpts.m_bSSRRoughnessBlurEnabled = true;  break;
	case 50: g_bCompletedAllPhases = true; return false;
	default: break;
	}
	return true;
}

static bool Verify_SSGIDenoiseToggle()
{
	// Restore defaults regardless of outcome so later batched tests see the
	// stock graph configuration.
	Zenith_GraphicsOptions::Get().m_bSSGIDenoiseEnabled      = true;
	Zenith_GraphicsOptions::Get().m_bSSRRoughnessBlurEnabled = true;
	return g_bCompletedAllPhases;
}

static const Zenith_AutomatedTest g_xSSGIDenoiseToggleTest = {
	"SSGIDenoiseToggle_Test",
	&Setup_SSGIDenoiseToggle,
	&Step_SSGIDenoiseToggle,
	&Verify_SSGIDenoiseToggle,
	120,
	true // m_bRequiresGraphics: the toggle/rebuild window only exists with Flux up (windowed)
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSSGIDenoiseToggleTest);

#endif // ZENITH_INPUT_SIMULATOR
