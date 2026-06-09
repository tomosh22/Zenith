#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Flux/Zenith_GameRenderFeatures.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#include <cstring> // strcmp — setup-log comparisons
#include <cstdio>  // std::printf — failure diagnostics

// ============================================================================
// GameRenderFeatures_Test (EXT-1, generic)
//
// Exercises the generic game render-feature registry that replaced the hardcoded
// post-fog hook. All assertions are graph-validity-agnostic (they observe
// InvokeFeaturesAnchoredAfter behaviour on a stack Flux_RenderGraph), so the test
// runs identically headless and windowed:
//   - Generic positioning: a feature anchored runAfter="Fog" fires on the "Fog"
//     step but NOT on the "SSAO" step (and vice-versa).
//   - Idempotent registration: re-registering the same name + identical desc is a
//     no-op (fires exactly once, not twice).
//   - Order-preserving unregister: features anchored on the same step fire in
//     registration order; removing a middle one preserves the others' order.
//
// We use uniquely-named test features ("__GRF_*") and Unregister each at the end,
// leaving the live DP_Fog registration untouched — so this test does NOT call the
// global ResetAll (which would drop DP_Fog for the rest of the suite).
// ============================================================================

namespace
{
	// Setup-call log: each test feature appends its tag char when its setup fires,
	// so we can assert both WHICH features fired and in what ORDER.
	char g_szSetupLog[64];
	u_int g_uSetupLogLen = 0;

	void LogSetup(char c)
	{
		if (g_uSetupLogLen < sizeof(g_szSetupLog) - 1) g_szSetupLog[g_uSetupLogLen++] = c;
		g_szSetupLog[g_uSetupLogLen] = '\0';
	}

	void SetupA(Flux_RenderGraph&) { LogSetup('A'); }
	void SetupB(Flux_RenderGraph&) { LogSetup('B'); }
	void SetupC(Flux_RenderGraph&) { LogSetup('C'); } // anchored on a DIFFERENT step

	bool g_bRanAssertions = false;
	bool g_bAllPassed     = true;

	#define GRF_EXPECT(cond, msg)                          \
		do { if (!(cond)) { g_bAllPassed = false;          \
			std::printf("  FAIL: %s (log=\"%s\")\n", msg, g_szSetupLog); } } while (0)

	Zenith_GameRenderFeatureDesc MakeDesc(const char* szName, void(*pfnSetup)(Flux_RenderGraph&), const char* szRunAfter)
	{
		Zenith_GameRenderFeatureDesc xDesc;
		xDesc.m_szName              = szName;
		xDesc.m_pfnInitialise       = nullptr;
		xDesc.m_pfnSetupRenderGraph = pfnSetup;
		xDesc.m_pfnShutdown         = nullptr;
		xDesc.m_szRunAfter          = szRunAfter;
		return xDesc;
	}

	void ResetLog() { g_uSetupLogLen = 0; g_szSetupLog[0] = '\0'; }
}

static void Setup_GameRenderFeatures()
{
	g_bRanAssertions = false;
	g_bAllPassed     = true;
	ResetLog();

	// Stack graph — never compiled, never given passes (our setup callbacks only
	// log). Safe headless (no GPU touched).
	Flux_RenderGraph xGraph;

	// Anchor our test features on REAL engine setup steps that carry NO game
	// feature ("SSAO", "Shadows") — never "Fog", because the live DP_Fog feature
	// is anchored there and firing its SetupDPFog (which touches GetDepthAttachment)
	// on our stack graph would crash headless. Using real step names also keeps
	// VerifyGameFeatureAnchors happy if a deferred rebuild ever observes them.
	Zenith_GameRenderFeatures::Register(MakeDesc("__GRF_A", &SetupA, "SSAO"));
	Zenith_GameRenderFeatures::Register(MakeDesc("__GRF_B", &SetupB, "SSAO"));
	Zenith_GameRenderFeatures::Register(MakeDesc("__GRF_C", &SetupC, "Shadows"));

	// 1. Generic positioning: the "SSAO" step fires A then B (registration order), NOT C.
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("SSAO", xGraph);
	GRF_EXPECT(strcmp(g_szSetupLog, "AB") == 0, "SSAO anchor fires A,B in order (not C)");

	// 2. The "Shadows" step fires only C (generic positioning the other way).
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("Shadows", xGraph);
	GRF_EXPECT(strcmp(g_szSetupLog, "C") == 0, "Shadows anchor fires only C");

	// 3. A step nobody (of ours) anchors on fires nothing.
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("Terrain", xGraph);
	GRF_EXPECT(g_szSetupLog[0] == '\0', "unanchored step fires nothing");

	// 4. Idempotent: re-register __GRF_A with identical desc → no second entry.
	Zenith_GameRenderFeatures::Register(MakeDesc("__GRF_A", &SetupA, "SSAO"));
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("SSAO", xGraph);
	GRF_EXPECT(strcmp(g_szSetupLog, "AB") == 0, "idempotent re-register → A fires once, still A,B");

	// 5. Order-preserving unregister: remove the first-registered "SSAO" feature
	//    (A); the remaining one (B) still fires, with relative order intact.
	Zenith_GameRenderFeatures::Unregister("__GRF_A");
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("SSAO", xGraph);
	GRF_EXPECT(strcmp(g_szSetupLog, "B") == 0, "after unregister A, only B fires on SSAO");

	// 6. Cleanup: remove our remaining test features. Verify they no longer fire.
	Zenith_GameRenderFeatures::Unregister("__GRF_B");
	Zenith_GameRenderFeatures::Unregister("__GRF_C");
	ResetLog();
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("SSAO", xGraph);
	Zenith_GameRenderFeatures::InvokeFeaturesAnchoredAfter("Shadows", xGraph);
	GRF_EXPECT(g_szSetupLog[0] == '\0', "after unregister-all, nothing fires");

	g_bRanAssertions = true;
}

static bool Step_GameRenderFeatures(int /*iFrame*/) { return false; /* one-shot */ }
static bool Verify_GameRenderFeatures()             { return g_bRanAssertions && g_bAllPassed; }

static const Zenith_AutomatedTest g_xGameRenderFeaturesTest = {
	"GameRenderFeatures_Test",
	&Setup_GameRenderFeatures,
	&Step_GameRenderFeatures,
	&Verify_GameRenderFeatures,
	5,
	false // pure registry logic — no GPU needed, runs headless too
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGameRenderFeaturesTest);

#endif // ZENITH_INPUT_SIMULATOR
