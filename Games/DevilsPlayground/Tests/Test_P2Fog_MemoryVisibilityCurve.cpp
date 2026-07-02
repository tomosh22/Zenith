#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"

#include <cstdio>

// ============================================================================
// Test_P2Fog_MemoryVisibilityCurve
//
// Pins DP_Fog::MemoryVisibilityForAge — the pure age -> rendered-visibility
// mapping the GPU memory-fog texture (and, later, the minimap) is rasterized
// with. Contract (visible=10, dim=30, dimVis=0.4, fade=5 passed explicitly so
// the test is independent of Tuning.json):
//
//   age < 0 (no entry)        -> 0    (never seen = full fog)
//   0 <= age <= visible_s     -> 255  (remembered-visible = clear)
//   visible_s < age <= dim_s  -> linear 255 -> dimVis*255
//   dim_s < age <= dim_s+fade -> linear dimVis*255 -> 0
//   age > dim_s + fade        -> 0    (forgotten = full fog)
//
// Also asserts the curve is continuous at both thresholds (no pop) and
// monotonically non-increasing, and that the Tuning.json keys the production
// rasterizer reads exist and are ordered sanely.
//
// What this catches:
//   * Off-by-one at the state thresholds (pop from 255 to dim floor).
//   * Inverted lerp direction (fog CLEARING as memories age).
//   * Missing/typo'd Tuning.json keys (DP_Tuning::Get would misbehave).
// ============================================================================

namespace
{
	bool g_bCurvePassed = false;
	const char* g_szCurveFailure = "";
}

static void Setup_P2MemoryVisibilityCurve()
{
	g_bCurvePassed = false;
	g_szCurveFailure = "";

	constexpr float fVis = 10.0f, fDim = 30.0f, fDimV = 0.4f, fFade = 5.0f;
	auto V = [&](float fAge)
	{
		return DP_Fog::MemoryVisibilityForAge(fAge, fVis, fDim, fDimV, fFade);
	};

	// Endpoint + plateau checks.
	if (V(-1.0f) != 0)     { g_szCurveFailure = "age<0 must map to 0";              return; }
	if (V(0.0f) != 255)    { g_szCurveFailure = "age 0 must map to 255";            return; }
	if (V(10.0f) != 255)   { g_szCurveFailure = "age==visible_s must still be 255"; return; }
	if (V(100.0f) != 0)    { g_szCurveFailure = "age >> dim_s+fade must be 0";      return; }
	if (V(35.0f) != 0)     { g_szCurveFailure = "age==dim_s+fade must reach 0";     return; }

	// Dim window midpoint: t=0.5 -> 1 + (0.4-1)*0.5 = 0.7 -> 179.
	if (V(20.0f) != 179)   { g_szCurveFailure = "dim-window midpoint drifted (expect 179)"; return; }
	// Dim floor at exactly dim_s: 0.4 * 255 = 102.
	if (V(30.0f) != 102)   { g_szCurveFailure = "value at dim_s must be dimVis*255 (102)";  return; }
	// Fade midpoint: 0.4 * 0.5 * 255 = 51.
	if (V(32.5f) != 51)    { g_szCurveFailure = "fade-tail midpoint drifted (expect 51)";   return; }

	// Continuity at both thresholds: one tick either side differs by < 8/255.
	if (V(10.001f) < 250)  { g_szCurveFailure = "pop at visible_s boundary"; return; }
	if (V(29.999f) < 100 || V(30.001f) < 95)
	{
		g_szCurveFailure = "pop at dim_s boundary";
		return;
	}

	// Monotonic non-increasing across a fine sweep.
	uint8_t uPrev = 255;
	for (float fAge = 0.0f; fAge <= 40.0f; fAge += 0.25f)
	{
		const uint8_t uCur = V(fAge);
		if (uCur > uPrev)
		{
			g_szCurveFailure = "curve is not monotonically non-increasing";
			return;
		}
		uPrev = uCur;
	}

	// The production rasterizer reads these four keys — pin their existence
	// and sane ordering so a Tuning.json typo fails loudly here, not as an
	// invisible fog regression.
	const float fTunVis   = DP_Tuning::Get<float>("fog_of_war.memory_visible_s");
	const float fTunDim   = DP_Tuning::Get<float>("fog_of_war.memory_dim_s");
	const float fTunDimV  = DP_Tuning::Get<float>("fog_of_war.memory_dim_visibility");
	const float fTunFade  = DP_Tuning::Get<float>("fog_of_war.memory_hidden_fade_s");
	if (!(fTunVis > 0.0f && fTunDim > fTunVis))
	{
		g_szCurveFailure = "tuning: memory_visible_s/memory_dim_s missing or unordered";
		return;
	}
	if (!(fTunDimV > 0.0f && fTunDimV < 1.0f))
	{
		g_szCurveFailure = "tuning: memory_dim_visibility must be in (0,1)";
		return;
	}
	if (!(fTunFade > 0.0f))
	{
		g_szCurveFailure = "tuning: memory_hidden_fade_s must be > 0";
		return;
	}

	g_bCurvePassed = true;
	std::printf("[P2MemoryVisibilityCurve] all checks passed\n");
	std::fflush(stdout);
}

static bool Step_P2MemoryVisibilityCurve(int /*iFrame*/)
{
	return false; // all work happens in Setup
}

static bool Verify_P2MemoryVisibilityCurve()
{
	if (!g_bCurvePassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2MemoryVisibilityCurve failed: %s", g_szCurveFailure);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2MemoryVisibilityCurveTest = {
	"Test_P2Fog_MemoryVisibilityCurve",
	&Setup_P2MemoryVisibilityCurve,
	&Step_P2MemoryVisibilityCurve,
	&Verify_P2MemoryVisibilityCurve,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2MemoryVisibilityCurveTest);

#endif // ZENITH_INPUT_SIMULATOR
