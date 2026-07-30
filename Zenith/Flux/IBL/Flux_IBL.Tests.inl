#include "UnitTests/Zenith_UnitTests.h"

ZENITH_TEST(IBLRegeneration, AmortizedCursorConvergesInSixFrames)
{
	bool bDirty = true;
	IBL_RegenState eState = IBL_REGEN_IDLE;
	u_int uFace = 0u;
	u_int uMip = 0u;
	u_int uIrradiancePasses = 0u;
	u_int uPrefilterPasses = 0u;
	u_int uFrames = 0u;

	while (bDirty && uFrames < 20u)
	{
		for (u_int u = 0u; u < IBLConfig::uPASSES_PER_FRAME; u++)
		{
			const Flux_IBLRegen::Pass xPass = Flux_IBLRegen::Next(bDirty, eState, uFace, uMip);
			if (xPass.m_eType == Flux_IBLRegen::PASS_NONE) break;
			if (xPass.m_eType == Flux_IBLRegen::PASS_IRRADIANCE) uIrradiancePasses++;
			if (xPass.m_eType == Flux_IBLRegen::PASS_PREFILTER) uPrefilterPasses++;
		}
		uFrames++;
	}

	ZENITH_ASSERT_EQ(uFrames, 6u);
	ZENITH_ASSERT_EQ(uIrradiancePasses, 6u);
	ZENITH_ASSERT_EQ(uPrefilterPasses, 42u);
	ZENITH_ASSERT_FALSE(bDirty);
	ZENITH_ASSERT_TRUE(eState == IBL_REGEN_IDLE);
}

ZENITH_TEST(IBLRegeneration, DirtyDuringPrefilterRestartsAtIrradianceZero)
{
	bool bDirty = true;
	IBL_RegenState eState = IBL_REGEN_PREFILTER;
	u_int uFace = 4u;
	u_int uMip = 5u;
	Flux_IBLRegen::MarkDirty(false, bDirty, eState, uFace, uMip);
	ZENITH_ASSERT_TRUE(eState == IBL_REGEN_IRRADIANCE);
	ZENITH_ASSERT_EQ(uFace, 0u);
	ZENITH_ASSERT_EQ(uMip, 0u);
	const Flux_IBLRegen::Pass xFirst = Flux_IBLRegen::Next(bDirty, eState, uFace, uMip);
	ZENITH_ASSERT_TRUE(xFirst.m_eType == Flux_IBLRegen::PASS_IRRADIANCE);
	ZENITH_ASSERT_EQ(xFirst.m_uFace, 0u);
}

ZENITH_TEST(IBLRegeneration, RuntimeDirtyKeepsReadyLatched)
{
	Flux_IBLImpl xIBL;
	xIBL.m_bFirstGeneration = false;
	xIBL.m_bIBLReady = true;
	xIBL.m_bSkyIBLDirty = true;
	xIBL.m_eRegenState = IBL_REGEN_PREFILTER;
	xIBL.m_uRegenFace = 3u;
	xIBL.m_uRegenMip = 2u;
	xIBL.MarkAllProbesDirty();
	ZENITH_ASSERT_TRUE(xIBL.IsReady());
	ZENITH_ASSERT_TRUE(xIBL.m_eRegenState == IBL_REGEN_IRRADIANCE);
	ZENITH_ASSERT_EQ(xIBL.m_uRegenFace, 0u);
	ZENITH_ASSERT_EQ(xIBL.m_uRegenMip, 0u);
}
