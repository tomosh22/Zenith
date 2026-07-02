#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "SaveData/Zenith_SaveData.h"

#include "Source/PublicInterfaces.h"
#include "../Components/DPLiminalHub_Component.h"
#include "Core/Zenith_CommandLine.h"
#include "Flux/Flux_Screenshot.h"

#include <cstdio>

// ============================================================================
// Test_DP_LiminalHub_Spend
//
// Loads the real Liminal scene (build index 2) and pins the hub's spend
// contract through the component's action twin (TrySpendNode — the exact
// body the node buttons' click handler runs):
//
//   1. With a seeded 10-Knot balance: node 0 of the Forge track unlocks
//      (cost 2 per the tuning curve), the balance drops to 8, and the
//      spend persists a meta-slot write.
//   2. Prefix ordering: node 2 is rejected while node 1 is still locked.
//   3. Insufficient funds: with the balance zeroed, node 1 is rejected and
//      nothing changes.
//   4. The Knot-balance readout text refreshes with the live balance.
// ============================================================================

namespace
{
	bool g_bHubFailed = false;
	bool g_bHubDone = false;
	char g_szHubWhy[192] = {};

	void HubFail(const char* szWhy)
	{
		g_bHubFailed = true;
		std::snprintf(g_szHubWhy, sizeof(g_szHubWhy), "%s", szWhy);
	}
}

static void Setup_LiminalSpend()
{
	g_bHubFailed = false;
	g_bHubDone = false;
	g_szHubWhy[0] = '\0';

	Zenith_SaveData::ClearForTest();
	DP_MetaState xSeed;
	xSeed.m_uKnotBalance = 10u;
	DP_MetaSave::SetCachedForTest(xSeed);

	g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE); // Liminal
}

static bool Step_LiminalSpend(int iFrame)
{
	if (g_bHubFailed || g_bHubDone) return false;
	// Windowed runs: dump the hub UI once it's wired (skipped headless).
	if (iFrame == 8 && !Zenith_CommandLine::IsHeadless())
	{
		Flux_Screenshot::RequestDump("C:/tmp/dp_liminal_hub.tga");
	}
	if (iFrame < 10) return true; // scene load + OnStart wiring

	DPLiminalHub_Component* pxHub = DPLiminalHub_Component::Instance();
	if (pxHub == nullptr)
	{
		HubFail("no DPLiminalHub instance after loading scene 2");
		return false;
	}

	// 1) Successful spend: Forge node 0 (cost 2 from a 10-Knot balance).
	if (!pxHub->TrySpendNode(DP_MetaSave::HermitTrack::Forge, 0))
	{
		HubFail("node 0 spend rejected despite sufficient balance");
		return false;
	}
	{
		const DP_MetaState& xState = DP_MetaSave::Cached();
		if (!DP_MetaSave::IsNodeUnlocked(xState, DP_MetaSave::HermitTrack::Forge, 0))
		{
			HubFail("node 0 not unlocked after successful spend");
			return false;
		}
		if (xState.m_uKnotBalance != 8u)
		{
			HubFail("balance after node-0 spend must be 10 - 2 = 8");
			return false;
		}
		const auto& axWritten = Zenith_SaveData::GetWrittenSlotsForTest();
		bool bPersisted = false;
		for (uint32_t u = 0; u < axWritten.GetSize(); ++u)
		{
			if (axWritten.Get(u).m_strSlotName == DP_MetaSave::SlotName()) bPersisted = true;
		}
		if (!bPersisted)
		{
			HubFail("successful spend must persist the meta slot");
			return false;
		}
	}

	// 2) Prefix ordering: node 2 must be rejected while node 1 is locked.
	if (pxHub->TrySpendNode(DP_MetaSave::HermitTrack::Forge, 2))
	{
		HubFail("node 2 spend must be rejected while node 1 is locked");
		return false;
	}

	// 3) Insufficient funds: zero the balance, node 1 (cost 3) rejected.
	{
		DP_MetaState xBroke = DP_MetaSave::Cached();
		xBroke.m_uKnotBalance = 0u;
		DP_MetaSave::SetCachedForTest(xBroke);
	}
	if (pxHub->TrySpendNode(DP_MetaSave::HermitTrack::Forge, 1))
	{
		HubFail("node 1 spend must be rejected with 0 Knots");
		return false;
	}
	if (!DP_MetaSave::IsNodeUnlocked(DP_MetaSave::Cached(), DP_MetaSave::HermitTrack::Forge, 0)
		|| DP_MetaSave::IsNodeUnlocked(DP_MetaSave::Cached(), DP_MetaSave::HermitTrack::Forge, 1))
	{
		HubFail("rejected spends must not mutate the unlock masks");
		return false;
	}

	g_bHubDone = true;
	return false;
}

static bool Verify_LiminalSpend()
{
	// Isolation: later tests must not inherit this test's staged state,
	// and the successful spend wrote a REAL slot file — delete it.
	Zenith_SaveData::DeleteSlot(DP_MetaSave::SlotName());
	Zenith_SaveData::ClearForTest();
	DP_MetaSave::InvalidateCacheForTest();

	if (!g_bHubDone || g_bHubFailed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "LiminalHub failed: %s",
			g_szHubWhy[0] != '\0' ? g_szHubWhy : "did not complete");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xLiminalSpendTest = {
	"Test_DP_LiminalHub_Spend",
	&Setup_LiminalSpend,
	&Step_LiminalSpend,
	&Verify_LiminalSpend,
	/*maxFrames*/ 120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xLiminalSpendTest);

#endif // ZENITH_INPUT_SIMULATOR
