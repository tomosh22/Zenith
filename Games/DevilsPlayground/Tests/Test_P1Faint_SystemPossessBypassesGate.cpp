#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Faint_SystemPossessBypassesGate (MVP-1.4.2 follow-up coverage gap)
//
// Sibling to Test_P1Faint_RecoversToIdle. Pins the SYSTEM-PATH bypass
// of the faint gate: SetPossessedVillager(faintedV) -- a direct write
// used by tests, editor automation, and future "respawn from save"
// flows -- wakes the fainted villager into Possessed regardless of
// the recovery timer.
//
// This is intentional asymmetry between paths:
//   * TryVoluntaryPossessSwitch (player-driven) -- refuses fainted
//     targets (IsPossessable() gate).
//   * SetPossessedVillager (system path) -- bypasses the gate. The
//     OnUpdate state machine's `case Fainted` branch handles the
//     bIsPossessedThisFrame=true case by transitioning straight to
//     Possessed and bumping life.
//
// Tests pinning the asymmetry catch:
//   * A regression where SetPossessedVillager picks up the same
//     IsPossessable check that TryVoluntary uses (which would make
//     editor automation / save-load unable to restore a possession
//     where the previous possessed villager was fainted).
//   * The state-machine OnUpdate handler for `case Fainted` +
//     bIsPossessedThisFrame missing entirely (in which case the
//     villager's state stays Fainted, TickLife/TickMovement don't
//     run, and the player thinks they're possessing a non-functional
//     body).
//
// Procedure:
//   1. Load GameLevel; pick two villagers in mutual range.
//   2. SetPossessedVillager(A); switch to B via TryVoluntary. A goes
//      Fainted.
//   3. Wait one frame for A's OnUpdate to set state=Fainted.
//   4. SetPossessedVillager(A) -- system path bypass.
//   5. Wait one frame so A's OnUpdate `case Fainted +
//      bIsPossessedThisFrame=true` branch runs.
//   6. Assert:
//        A.state == Possessed       (transitioned)
//        A.life == m_fMaxLife       (life bumped on transition)
//        A.faintTimer == 0          (timer cleared)
//        IsPossessed() == true      (legacy flag updated)
// ============================================================================

namespace
{
	enum Phase : int { kBP_Start, kBP_WaitScene, kBP_PossessA, kBP_WaitForA,
	                   kBP_SwitchToB, kBP_WaitForFaint, kBP_PossessAViaSystem,
	                   kBP_WaitForWake, kBP_Verify, kBP_Done };

	int                     g_iPhase = kBP_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	DPVillagerState         g_eAStateFinal = DPVillagerState::Idle;
	float                   g_fALifeFinal = 0.0f;
	float                   g_fAMaxLifeFinal = 0.0f;
	float                   g_fAFaintTimerFinal = -1.0f;
	bool                    g_bAIsPossessedFinal = false;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	void PickClosestPair(Zenith_EntityID& xA, Zenith_EntityID& xB)
	{
		struct VPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VPos> axVs;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVs](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				VPos xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axVs.PushBack(xV);
			});
		if (axVs.GetSize() < 2) return;
		float fMin = 1e30f;
		for (uint32_t i = 0; i < axVs.GetSize(); ++i)
		for (uint32_t j = i + 1; j < axVs.GetSize(); ++j)
		{
			const float fD = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(j).xPos);
			if (fD < fMin) { fMin = fD; xA = axVs.Get(i).xId; xB = axVs.Get(j).xId; }
		}
	}

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P1FaintBypass()
{
	g_iPhase = kBP_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_eAStateFinal = DPVillagerState::Idle;
	g_fALifeFinal = 0.0f;
	g_fAMaxLifeFinal = 0.0f;
	g_fAFaintTimerFinal = -1.0f;
	g_bAIsPossessedFinal = false;
}

static bool Step_P1FaintBypass(int iFrame)
{
	switch (g_iPhase)
	{
	case kBP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBP_WaitScene;
		return true;

	case kBP_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid())
		{
			g_iPhase = kBP_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBP_Done;
		}
		return true;

	case kBP_PossessA:
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kBP_WaitForA;
		return true;

	case kBP_WaitForA:
		g_iPhase = kBP_SwitchToB;
		return true;

	case kBP_SwitchToB:
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kBP_WaitForFaint;
		return true;

	case kBP_WaitForFaint:
		// One frame for A's OnUpdate to run Possessed -> Fainted.
		g_iPhase = kBP_PossessAViaSystem;
		return true;

	case kBP_PossessAViaSystem:
		// SYSTEM PATH bypass. A is Fainted; SetPossessedVillager
		// should overwrite DP_Player's possessed handle to A. On the
		// next OnUpdate, A's state-machine `case Fainted +
		// bIsPossessedThisFrame=true` branch transitions to Possessed
		// and bumps life.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kBP_WaitForWake;
		return true;

	case kBP_WaitForWake:
		// One frame for A's OnUpdate to observe possession and run the
		// Fainted -> Possessed transition (system bypass branch).
		g_iPhase = kBP_Verify;
		return true;

	case kBP_Verify:
	{
		DPVillager_Behaviour* pxA = GetVillagerBehaviour(g_xA);
		if (pxA != nullptr)
		{
			g_eAStateFinal = pxA->GetState();
			g_fALifeFinal = pxA->GetRemainingLife();
			g_fAMaxLifeFinal = pxA->GetMaxLife();
			g_fAFaintTimerFinal = pxA->GetFaintRecoveryRemaining();
			g_bAIsPossessedFinal = pxA->IsPossessed();
		}
		std::printf("[P1FaintBypass] state=%d life=%.2f/%.2f faintTimer=%.3f isPossessed=%d\n",
			(int)g_eAStateFinal, g_fALifeFinal, g_fAMaxLifeFinal,
			g_fAFaintTimerFinal, (int)g_bAIsPossessedFinal);
		std::fflush(stdout);
		g_iPhase = kBP_Done;
		return false;
	}

	case kBP_Done:
	default:
		return false;
	}
}

static bool Verify_P1FaintBypass()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1FaintBypass: villager pair not picked");
		return false;
	}
	if (g_eAStateFinal != DPVillagerState::Possessed)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintBypass: A's state after system-path SetPossessedVillager is %d, expected Possessed. Either the state-machine `case Fainted + bIsPossessedThisFrame=true` branch is missing, or SetPossessedVillager itself failed",
			(int)g_eAStateFinal);
		return false;
	}
	if (g_fALifeFinal < g_fAMaxLifeFinal - 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintBypass: A's life after wake is %.2f but maxLife is %.2f -- the Fainted->Possessed transition forgot to bump life. (Tolerance 0.05 absorbs one frame of TickLife drain after the bump.)",
			g_fALifeFinal, g_fAMaxLifeFinal);
		return false;
	}
	if (g_fAFaintTimerFinal != 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintBypass: A's faint timer after wake is %.3f, expected 0 (timer should clear on Fainted->Possessed bypass)",
			g_fAFaintTimerFinal);
		return false;
	}
	if (!g_bAIsPossessedFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1FaintBypass: IsPossessed() returns false but state is Possessed -- the m_bIsPossessed legacy flag sync after the state-machine update is broken");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1FaintBypassTest = {
	"Test_P1Faint_SystemPossessBypassesGate",
	&Setup_P1FaintBypass,
	&Step_P1FaintBypass,
	&Verify_P1FaintBypass,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1FaintBypassTest);

#endif // ZENITH_INPUT_SIMULATOR
