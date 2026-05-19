#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include <cstdio>

// ============================================================================
// Test_P2Archetype_BeggarIgnoredByAelfric (MVP-2.1.6)
//
// Pins the GDD's Beggar mechanic: "Aelfric's gaze slides off Beggars
// -- he can't fix on them as a target". Even when the player possesses
// a Beggar with the priest in line-of-sight, the priest's
// BB_KEY_TARGET_WITH_DEVIL must stay INVALID. The priest's pursuit
// branch is gated on that BB key being non-null, so a Beggar is
// effectively un-pursuable.
//
// This is the archetype's ONLY mechanical hook in MVP -- without it
// a Beggar is just a Farmhand with a 25s life timer (which is
// strictly worse, no reason to ever possess them). The filter is
// what makes the archetype playable as a hidey-pocket.
//
// Procedure:
//   1. Load GameLevel + find the priest + any villager + the priest
//      itself.
//   2. Apply the "Beggar" archetype to the chosen villager via
//      ApplyArchetype("Beggar"). This re-resolves life/speed AND
//      stamps the m_strArchetypeId so the priest's filter can read
//      it back.
//   3. Register the Beggar villager + teleport it 4 m into the
//      priest's facing direction so the priest's sight cone catches
//      it through real perception.
//   4. SetPossessedVillager(beggarId). System path; the priest's
//      BridgePerceptionToBlackboard will fire on the next OnUpdate.
//   5. Tick ~60 frames so the priest's perception bridge runs at
//      least a dozen times across the omniscient-fallback window.
//   6. Read the priest's BB_KEY_TARGET_WITH_DEVIL.
//   7. Assert the BB value is INVALID (NOT the Beggar's handle).
//
// What this catches:
//   * MVP-2.1.6 implementation regression: the archetype filter got
//     dropped from BridgePerceptionToBlackboard.
//   * A regression where the fallback path was filtered but the
//     real-perception path wasn't (or vice versa).
//   * GetArchetypeId() returning a stale value after ApplyArchetype
//     (the filter would read "Farmhand" and let the Beggar through).
// ============================================================================

namespace
{
	enum Phase : int { kBG_Start, kBG_WaitScene, kBG_ApplyArchetype,
	                   kBG_PossessBeggar, kBG_Tick, kBG_Verify, kBG_Done };

	int                     g_iPhase = kBG_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xBeggar;
	int                     g_iTickFrames = 0;
	Zenith_EntityID         g_xBBTargetFinal;
	std::string             g_strArchetypeFinal;

	constexpr int kTICK_FRAMES = 60;

	Zenith_EntityID ReadPriestBBTarget(Zenith_EntityID xPriestId)
	{
		Zenith_SceneData* pxScene =
			Zenith_SceneManager::GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return INVALID_ENTITY_ID;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return INVALID_ENTITY_ID;
		Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
		return xAg.GetBlackboard().GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
	}

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

static void Setup_P2BeggarIgnored()
{
	g_iPhase = kBG_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xBeggar = INVALID_ENTITY_ID;
	g_iTickFrames = 0;
	g_xBBTargetFinal = INVALID_ENTITY_ID;
	g_strArchetypeFinal.clear();
}

static bool Step_P2BeggarIgnored(int iFrame)
{
	switch (g_iPhase)
	{
	case kBG_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBG_WaitScene;
		return true;

	case kBG_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFoundPriest = xId; });
		Zenith_EntityID xFoundVillager;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundVillager](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundVillager.IsValid()) xFoundVillager = xId;
			});
		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_xBeggar = xFoundVillager;
			g_iPhase = kBG_ApplyArchetype;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBG_Done;
		}
		return true;
	}

	case kBG_ApplyArchetype:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xBeggar);
		if (pxV != nullptr) pxV->ApplyArchetype("Beggar");
		// MVP-1.9 cleanup: real perception only. Register the Beggar
		// villager as a hostile target + teleport it 4 m IN FRONT of
		// the priest (authored priest yaw = 0, facing +Z) so the
		// priest's sight cone catches it. Without the Beggar filter,
		// this setup would make the priest target the villager; the
		// test asserts that the IsBeggarVillager check in
		// BridgePerceptionToBlackboard rejects it instead.
		Zenith_PerceptionSystem::RegisterTarget(g_xBeggar, /*hostile=*/true);
		Zenith_SceneData* pxPriestScene =
			Zenith_SceneManager::GetSceneDataForEntity(g_xPriest);
		Zenith_SceneData* pxVillagerScene =
			Zenith_SceneManager::GetSceneDataForEntity(g_xBeggar);
		if (pxPriestScene != nullptr && pxVillagerScene != nullptr)
		{
			Zenith_Entity xPriestEnt = pxPriestScene->TryGetEntity(g_xPriest);
			Zenith_Entity xBeggarEnt  = pxVillagerScene->TryGetEntity(g_xBeggar);
			if (xPriestEnt.IsValid() && xBeggarEnt.IsValid()
			 && xPriestEnt.HasComponent<Zenith_TransformComponent>()
			 && xBeggarEnt.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xPriestPos;
				xPriestEnt.GetComponent<Zenith_TransformComponent>()
					.GetPosition(xPriestPos);
				xBeggarEnt.GetComponent<Zenith_TransformComponent>()
					.SetPosition(Zenith_Maths::Vector3(
						xPriestPos.x, xPriestPos.y, xPriestPos.z + 4.0f));
			}
		}
		g_iPhase = kBG_PossessBeggar;
		return true;
	}

	case kBG_PossessBeggar:
		DP_Player::SetPossessedVillager(g_xBeggar);
		g_iTickFrames = 0;
		g_iPhase = kBG_Tick;
		return true;

	case kBG_Tick:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_FRAMES)
		{
			g_iPhase = kBG_Verify;
		}
		return true;

	case kBG_Verify:
	{
		g_xBBTargetFinal = ReadPriestBBTarget(g_xPriest);
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xBeggar);
		if (pxV != nullptr) g_strArchetypeFinal = pxV->GetArchetypeId();
		std::printf("[P2BeggarIgnored] beggar=(%u/%u) priest=(%u/%u) archetype=%s bbTarget=(%u/%u) (expected INVALID)\n",
			g_xBeggar.m_uIndex, g_xBeggar.m_uGeneration,
			g_xPriest.m_uIndex, g_xPriest.m_uGeneration,
			g_strArchetypeFinal.c_str(),
			g_xBBTargetFinal.m_uIndex, g_xBBTargetFinal.m_uGeneration);
		std::fflush(stdout);
		g_iPhase = kBG_Done;
		return false;
	}

	case kBG_Done:
	default:
		return false;
	}
}

static bool Verify_P2BeggarIgnored()
{
	if (!g_xPriest.IsValid() || !g_xBeggar.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2BeggarIgnored: priest or villager not found");
		return false;
	}
	if (g_strArchetypeFinal != "Beggar")
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BeggarIgnored: ApplyArchetype(\"Beggar\") didn't stick -- GetArchetypeId returned \"%s\". Filter would read the wrong identity",
			g_strArchetypeFinal.c_str());
		return false;
	}
	if (g_xBBTargetFinal.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BeggarIgnored: BB_KEY_TARGET_WITH_DEVIL = (%u/%u), expected INVALID. The priest's BridgePerceptionToBlackboard is not filtering Beggars -- either IsBeggarVillager returns false, or one of the perception code paths skipped the filter",
			g_xBBTargetFinal.m_uIndex, g_xBBTargetFinal.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2BeggarIgnoredTest = {
	"Test_P2Archetype_BeggarIgnoredByAelfric",
	&Setup_P2BeggarIgnored,
	&Step_P2BeggarIgnored,
	&Verify_P2BeggarIgnored,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2BeggarIgnoredTest);

#endif // ZENITH_INPUT_SIMULATOR
