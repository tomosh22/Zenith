#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "../Components/DPOrbitCamera_Component.h"

#include "Flux/Flux_Screenshot.h"

#include "DP_TestTGAHelpers.h"

#include <cstdio>

// ============================================================================
// Test_DPFogPass_VisualOutput
//
// End-to-end pixel proof that the fog memory table actually reaches the
// screen — the gap the C++-only Test_P2Fog_* suite structurally cannot see
// (the table was fully maintained on the CPU for weeks while the shader
// ignored it).
//
// Procedure (windowed only — requiresGraphics):
//   1. Load ProcLevel and let it settle (procgen + first-frame shader build).
//   2. For ~60 frames, record synthetic memory reveals over a 20x20 m cell
//      block centred on the orbit-camera target (guaranteed near screen
//      centre; far enough from idle villagers that their holes don't cover
//      it). Ages stay 0 -> block is remembered-visible -> fog cleared there.
//   3. Screenshot A.
//   4. ClearAllMemoryReveals + stop the synthetic reveals; give the pass a
//      few frames to re-upload (villagers re-reveal only their own cells).
//   5. Screenshot B — the centre block is opaque fog again.
//   6. Assert mean |A-B| over the centre window is large, and that it
//      dominates the same metric over the corners (which are full fog in
//      both shots — only temporal noise). The relative check keeps the test
//      robust to tonemap/bloom absolute-colour drift; the priest wandering
//      through a sliver of the window is absorbed by the mean.
// ============================================================================

namespace
{
	constexpr const char* SHOT_A = "C:/tmp/dp_fog_memory_A.tga";
	constexpr const char* SHOT_B = "C:/tmp/dp_fog_memory_B.tga";

	bool g_bVisualSetupOk = false;
	Zenith_Maths::Vector3 g_xRevealCentre = { 0.0f, 0.0f, 0.0f };
	bool g_bHaveCentre = false;

	void RecordRevealBlock()
	{
		// 20x20 m of 1 m cells centred on the orbit target.
		for (int32_t iX = -10; iX < 10; ++iX)
		{
			for (int32_t iZ = -10; iZ < 10; ++iZ)
			{
				DP_Fog::RecordMemoryReveal(Zenith_Maths::Vector3(
					g_xRevealCentre.x + static_cast<float>(iX) + 0.5f,
					0.0f,
					g_xRevealCentre.z + static_cast<float>(iZ) + 0.5f));
			}
		}
	}
}

static void Setup_DPFogVisual()
{
	g_bVisualSetupOk = false;
	g_bHaveCentre = false;
	std::remove(SHOT_A);
	std::remove(SHOT_B);
	g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE); // ProcLevel
	g_bVisualSetupOk = true;
}

static bool Step_DPFogVisual(int iFrame)
{
	// Frame 60: scene + camera settled; grab the orbit target once.
	if (iFrame == 60)
	{
		DP_Query::ForEachComponentInActiveScene<DPOrbitCamera_Component>(
			[](Zenith_EntityID /*xId*/, DPOrbitCamera_Component& xCam)
			{
				g_xRevealCentre = xCam.GetOrbitTarget();
				g_bHaveCentre = true;
			});
	}

	// Frames 60..149: hold the reveal block at age 0 (remembered-visible).
	if (iFrame >= 60 && iFrame < 150 && g_bHaveCentre)
	{
		RecordRevealBlock();
	}

	if (iFrame == 145)
	{
		Flux_Screenshot::RequestDump(SHOT_A);
	}

	// Frame 150: forget everything; only the villagers' own cells repopulate.
	if (iFrame == 150)
	{
		DP_Fog::ClearAllMemoryReveals();
	}

	if (iFrame == 158)
	{
		Flux_Screenshot::RequestDump(SHOT_B);
	}

	return iFrame < 165;
}

static bool Verify_DPFogVisual()
{
	if (!g_bVisualSetupOk || !g_bHaveCentre)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPFogVisual: setup failed (no orbit camera found?)");
		return false;
	}

	DP_TestTGAImage xA, xB;
	if (!DP_TestLoadTGA(SHOT_A, xA) || !DP_TestLoadTGA(SHOT_B, xB))
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPFogVisual: screenshot load failed (%s / %s)", SHOT_A, SHOT_B);
		return false;
	}
	if (xA.m_uWidth != xB.m_uWidth || xA.m_uHeight != xB.m_uHeight)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPFogVisual: screenshot dimensions differ");
		return false;
	}

	// Centre window: +/-12% of the smaller dimension around screen centre.
	const uint32_t uCX = xA.m_uWidth / 2u;
	const uint32_t uCY = xA.m_uHeight / 2u;
	const uint32_t uHalf = (xA.m_uWidth < xA.m_uHeight ? xA.m_uWidth : xA.m_uHeight) * 12u / 100u;
	const float fCentreDiff = DP_TestMeanAbsDiffBGR(xA, xB,
		uCX - uHalf, uCY - uHalf, uCX + uHalf, uCY + uHalf);

	// Corner windows (10% squares inset 2%): full fog in both shots.
	const uint32_t uCorner = (xA.m_uWidth < xA.m_uHeight ? xA.m_uWidth : xA.m_uHeight) / 10u;
	const uint32_t uInsetX = xA.m_uWidth * 2u / 100u;
	const uint32_t uInsetY = xA.m_uHeight * 2u / 100u;
	const float fCornerDiff =
		(DP_TestMeanAbsDiffBGR(xA, xB, uInsetX, uInsetY, uInsetX + uCorner, uInsetY + uCorner) +
		 DP_TestMeanAbsDiffBGR(xA, xB, xA.m_uWidth - uInsetX - uCorner, uInsetY,
			xA.m_uWidth - uInsetX, uInsetY + uCorner)) * 0.5f;

	std::printf("[DPFogVisual] centre diff=%.2f corner diff=%.2f (%ux%u)\n",
		fCentreDiff, fCornerDiff, xA.m_uWidth, xA.m_uHeight);
	std::fflush(stdout);

	// The remembered-visible block must visibly change the centre of the
	// screen (fog cleared vs full fog) and must dominate ambient temporal
	// noise measured at the fogged corners.
	if (fCentreDiff < 5.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"DPFogVisual: centre diff %.2f < 5.0 -- memory fog is not reaching the screen",
			fCentreDiff);
		return false;
	}
	if (fCentreDiff < fCornerDiff * 3.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"DPFogVisual: centre diff %.2f does not dominate corner noise %.2f",
			fCentreDiff, fCornerDiff);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDPFogVisualTest = {
	"Test_DPFogPass_VisualOutput",
	&Setup_DPFogVisual,
	&Step_DPFogVisual,
	&Verify_DPFogVisual,
	/*maxFrames*/ 300,
	/*requiresGraphics*/ true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPFogVisualTest);

#endif // ZENITH_INPUT_SIMULATOR
