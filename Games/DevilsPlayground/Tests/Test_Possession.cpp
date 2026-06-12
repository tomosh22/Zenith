#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Component.h"

// ============================================================================
// Possession_RoundTrip_Test
//
// Loads GameLevel, finds the authored Villager_0, and proves the
// DPVillager_Component observes possession state through the public
// interface:
//
//   - Initial: IsPossessed() == false
//   - DP_Player::SetPossessedVillager(villagerId) → next frame IsPossessed() == true
//   - DP_Player::SetPossessedVillager(INVALID) → IsPossessed() flips back to false
//
// This is the end-to-end loop that the player-controller's click-to-possess
// path drives in normal gameplay; the test bypasses the click and writes the
// possessed-villager state directly.
// ============================================================================

namespace
{
	enum Phase : int { kP_Start, kP_WaitScene, kP_RecordVillager, kP_Possess,
	                   kP_VerifyPossessed, kP_Unpossess, kP_VerifyCleared, kP_Done };

	int             g_iPhase             = kP_Start;
	Zenith_EntityID g_xVillager;
	bool            g_bInitialNotPossessed = false;
	bool            g_bPossessedSeen        = false;
	bool            g_bUnpossessSeen        = false;
}

static void Setup_Possession()
{
	g_iPhase                  = kP_Start;
	g_xVillager               = INVALID_ENTITY_ID;
	g_bInitialNotPossessed    = false;
	g_bPossessedSeen          = false;
	g_bUnpossessSeen          = false;
}

static bool Step_Possession(int iFrame)
{
	switch (g_iPhase)
	{
	case kP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kP_WaitScene;
		return true;

	case kP_WaitScene:
	{
		// Wait until DPVillager_Component shows up in the active scene.
		Zenith_EntityID xFound;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFound](Zenith_EntityID xId, DPVillager_Component&) { xFound = xId; });
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kP_RecordVillager;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kP_Done; // give up
		}
		return true;
	}

	case kP_RecordVillager:
	{
		// Initial state: not possessed (DP_Player has no possessed villager
		// set yet because no controller has run click-to-possess).
		bool bIsPossessed = false;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&bIsPossessed](Zenith_EntityID, DPVillager_Component& xV)
			{
				if (xV.IsPossessed()) bIsPossessed = true;
			});
		g_bInitialNotPossessed = !bIsPossessed;
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kP_Possess;
		return true;
	}

	case kP_Possess:
		// Give the villager one frame to observe the new possessed state in
		// its own OnUpdate.
		g_iPhase = kP_VerifyPossessed;
		return true;

	case kP_VerifyPossessed:
	{
		bool bIsPossessed = false;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&bIsPossessed](Zenith_EntityID, DPVillager_Component& xV)
			{
				if (xV.IsPossessed()) bIsPossessed = true;
			});
		g_bPossessedSeen = bIsPossessed;
		DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
		g_iPhase = kP_Unpossess;
		return true;
	}

	case kP_Unpossess:
		g_iPhase = kP_VerifyCleared;
		return true;

	case kP_VerifyCleared:
	{
		bool bAnyPossessed = false;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&bAnyPossessed](Zenith_EntityID, DPVillager_Component& xV)
			{
				if (xV.IsPossessed()) bAnyPossessed = true;
			});
		g_bUnpossessSeen = !bAnyPossessed;
		g_iPhase = kP_Done;
		return false;
	}

	case kP_Done:
	default:
		return false;
	}
}

static bool Verify_Possession()
{
	return g_bInitialNotPossessed && g_bPossessedSeen && g_bUnpossessSeen;
}

static const Zenith_AutomatedTest g_xPossessionTest = {
	"Possession_RoundTrip_Test",
	&Setup_Possession,
	&Step_Possession,
	&Verify_Possession,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPossessionTest);

#endif // ZENITH_INPUT_SIMULATOR
