#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P1Switch_BurnOutDoesDie (MVP-1.4.3)
//
// Negative-control sibling to `Test_P1Switch_FaintNotDie`: burn-out
// (life timer hits 0) sets state to Dead, NOT Fainted.
//
// Without this guard, an over-zealous "all un-possession faints"
// regression in DPVillager_Behaviour::OnUpdate would have the
// possessed-villager's burn-out moment treated as a voluntary
// switch -- letting the player re-possess the burned-out villager
// after 10 s "recovery". That breaks the GDD's permanent-loss
// semantic for run-end conditions and would also break the
// MVP-1.3.5 NoVessels detection (burned-out villagers would tick
// back to Idle, never letting "all dead" hold).
//
// Procedure:
//   1. Load GameLevel; pick any villager.
//   2. SetPossessedVillager(V).
//   3. Wait for the Idle -> Possessed transition.
//   4. SetRemainingLifeForTest(0.001 s) -- one frame's drain past 0.
//   5. Tick a frame so TickLife drains + calls Kill().
//   6. Snapshot state + life + faint-recovery-remaining.
//   7. Assert:
//        V.state == Dead (NOT Fainted)
//        V.remainingLife == 0
//        V.faintRecoveryRemaining == 0 (timer didn't arm)
//   8. Also assert: TryVoluntaryPossessSwitch(V) refuses (V is Dead).
//
// What this catches:
//   * The OnUpdate state machine treats burn-out's un-possession
//     transition as voluntary (state -> Fainted) because Kill()
//     forgot to set state=Dead BEFORE the next OnUpdate observes
//     the un-possession.
//   * IsPossessable() returns true for Dead -- the dead villager
//     is selectable.
//   * Kill() set state=Fainted by accident, allowing recovery (the
//     GDD's permanent-loss promise breaks).
// ============================================================================

namespace
{
	enum Phase : int { kBD_Start, kBD_WaitScene, kBD_PossessV, kBD_WaitForV,
	                   kBD_DrainLife, kBD_WaitForKill, kBD_Snapshot,
	                   kBD_TryRePossess, kBD_Verify, kBD_Done };

	int                     g_iPhase = kBD_Start;
	Zenith_EntityID         g_xV;
	DPVillagerState         g_eVFinalState = DPVillagerState::Idle;
	float                   g_fVFinalLife = 0.0f;
	float                   g_fVFaintTimer = -1.0f;
	bool                    g_bRePossessOk = false;

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P1BurnOutDoesDie()
{
	g_iPhase = kBD_Start;
	g_xV = INVALID_ENTITY_ID;
	g_eVFinalState = DPVillagerState::Idle;
	g_fVFinalLife = 0.0f;
	g_fVFaintTimer = -1.0f;
	g_bRePossessOk = false;
}

static bool Step_P1BurnOutDoesDie(int iFrame)
{
	switch (g_iPhase)
	{
	case kBD_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBD_WaitScene;
		return true;

	case kBD_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xV = xFound;
			g_iPhase = kBD_PossessV;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBD_Done;
		}
		return true;
	}

	case kBD_PossessV:
		DP_Player::SetPossessedVillager(g_xV);
		g_iPhase = kBD_WaitForV;
		return true;

	case kBD_WaitForV:
		// One frame for OnUpdate to run Idle -> Possessed.
		g_iPhase = kBD_DrainLife;
		return true;

	case kBD_DrainLife:
	{
		// One frame's worth of TickLife drain (~0.01666 s) is more than
		// 0.001, so next OnUpdate's TickLife will drain past 0 and
		// invoke Kill().
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xV);
		if (pxV != nullptr)
		{
			pxV->SetRemainingLifeForTest(0.001f);
		}
		g_iPhase = kBD_WaitForKill;
		return true;
	}

	case kBD_WaitForKill:
		// One frame for TickLife -> Kill -> state=Dead, plus the
		// NoVessels scan (which won't trigger because there are 16
		// other villagers alive).
		g_iPhase = kBD_Snapshot;
		return true;

	case kBD_Snapshot:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xV);
		if (pxV != nullptr)
		{
			g_eVFinalState = pxV->GetState();
			g_fVFinalLife = pxV->GetRemainingLife();
			g_fVFaintTimer = pxV->GetFaintRecoveryRemaining();
		}
		g_iPhase = kBD_TryRePossess;
		return true;
	}

	case kBD_TryRePossess:
		// V is Dead. Voluntary switch should refuse.
		g_bRePossessOk = DP_Player::TryVoluntaryPossessSwitch(g_xV);
		g_iPhase = kBD_Verify;
		return true;

	case kBD_Verify:
		std::printf("[P1BurnOutDoesDie] state=%d life=%.3f faintTimer=%.3f rePossessOk=%d\n",
			(int)g_eVFinalState, g_fVFinalLife, g_fVFaintTimer,
			(int)g_bRePossessOk);
		std::fflush(stdout);
		g_iPhase = kBD_Done;
		return false;

	case kBD_Done:
	default:
		return false;
	}
}

static bool Verify_P1BurnOutDoesDie()
{
	if (!g_xV.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1BurnOutDoesDie: villager not found");
		return false;
	}
	if (g_eVFinalState != DPVillagerState::Dead)
	{
		const char* szState = "Idle";
		if (g_eVFinalState == DPVillagerState::Possessed) szState = "Possessed";
		else if (g_eVFinalState == DPVillagerState::Fainted) szState = "Fainted";
		else if (g_eVFinalState == DPVillagerState::Dead) szState = "Dead";
		Zenith_Log(LOG_CATEGORY_AI,
			"P1BurnOutDoesDie: V's state after burn-out is %s, expected Dead. Kill() forgot to set state=Dead, or OnUpdate's Possessed->Fainted transition fired before Kill()'s state assignment",
			szState);
		return false;
	}
	if (g_fVFinalLife > 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1BurnOutDoesDie: V's remaining life is %.3f after burn-out, expected 0 -- Kill() didn't clamp life",
			g_fVFinalLife);
		return false;
	}
	if (g_fVFaintTimer > 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1BurnOutDoesDie: V's faint recovery timer is %.3f after burn-out, expected 0 -- burn-out leaked into the Fainted-state code path",
			g_fVFaintTimer);
		return false;
	}
	if (g_bRePossessOk)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1BurnOutDoesDie: TryVoluntaryPossessSwitch(deadVillager) succeeded -- IsPossessable() returns true for Dead, or the gate is missing");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1BurnOutDoesDieTest = {
	"Test_P1Switch_BurnOutDoesDie",
	&Setup_P1BurnOutDoesDie,
	&Step_P1BurnOutDoesDie,
	&Verify_P1BurnOutDoesDie,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1BurnOutDoesDieTest);

#endif // ZENITH_INPUT_SIMULATOR
