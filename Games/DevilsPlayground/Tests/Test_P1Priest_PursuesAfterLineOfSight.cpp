#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Priest_PursuesAfterLineOfSight (MVP-1.9.3)
//
// Companion to Test_P1Priest_DoesNotChasePossessedOutOfSight: with the
// omniscient fallback OFF, the priest STILL gets BB_KEY_TARGET_WITH_DEVIL
// populated -- via the regular sight-based perception path -- once the
// possessed villager is in its sight cone with line-of-sight.
//
// Procedure:
//   1. Load GameLevel.
//   2. Find the priest. Pick a villager.
//   3. Teleport the villager to a position 4 m IN FRONT of the priest
//      (priest's authored yaw=0 means facing +Z, so put the villager
//      at priest_pos + (0, 0, +4)).
//   5. Possess the villager.
//   6. Tick frames; the perception system's UpdateSightPerception runs
//      every frame via DPPlayerController_Behaviour::OnUpdate's
//      `Zenith_PerceptionSystem::Update(fDt)` call. With awareness
//      gain rate ~2.0/s the priest's awareness of the villager
//      crosses any reasonable threshold within ~1 s.
//   7. Sample priest's BB_KEY_TARGET_WITH_DEVIL each frame; record
//      when it first becomes valid AND matches the villager handle.
//   8. Assert it became valid before the 180-frame window expired.
// ============================================================================

namespace
{
	enum Phase : int { kLS_Start, kLS_WaitScene, kLS_TeleportAndPossess,
	                   kLS_RunFrames, kLS_Verify, kLS_Done };

	int                     g_iPhase = kLS_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	int                     g_iRunFrames = 0;
	int                     g_iFrameTargetFirstSeen = -1;
	Zenith_EntityID         g_xTargetAtFirstSighting;

	constexpr int kRUN_FRAMES_MAX = 180;

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

	bool TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		return true;
	}

	Zenith_EntityID ReadPriestBBTarget(Zenith_EntityID xPriestId)
	{
		Zenith_SceneData* pxScene =
			g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return INVALID_ENTITY_ID;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return INVALID_ENTITY_ID;
		Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
		return xAg.GetBlackboard().GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
	}
}

static void Setup_P1PursuesAfterLOS()
{
	g_iPhase = kLS_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_iRunFrames = 0;
	g_iFrameTargetFirstSeen = -1;
	g_xTargetAtFirstSighting = INVALID_ENTITY_ID;
}

static bool Step_P1PursuesAfterLOS(int iFrame)
{
	switch (g_iPhase)
	{
	case kLS_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kLS_WaitScene;
		return true;

	case kLS_WaitScene:
	{
		Zenith_EntityID xFoundPriest, xFoundVillager;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Behaviour&) { xFoundPriest = xId; });
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundVillager](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundVillager.IsValid()) xFoundVillager = xId;
			});
		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPhase = kLS_TeleportAndPossess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kLS_Done;
		}
		return true;
	}

	case kLS_TeleportAndPossess:
	{
		// MVP-1.9 setup: register the villager with the perception
		// system as a HOSTILE target so the priest's sight pass
		// considers it. Production DPVillager_Behaviour doesn't
		// self-register today (the omniscient fallback obviated that
		// for the pre-1.9 tests); a future PR can wire OnAwake
		// registration once the production gameplay loop needs sight
		// to feed the priest BT in non-fallback mode.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);

		// Place the villager 4 m IN FRONT of the priest. Priest's
		// authored yaw is 0 (facing +Z by Zenith convention -- the
		// default "no rotation" forward direction). Putting the
		// villager at priest + (0, 0, +4) puts it directly in the
		// priest's primary FOV cone.
		Zenith_Maths::Vector3 xPriestPos;
		if (!TryGetEntityPos(g_xPriest, xPriestPos))
		{
			g_iPhase = kLS_Done;
			return false;
		}
		Zenith_Maths::Vector3 xVillagerPos(
			xPriestPos.x, xPriestPos.y, xPriestPos.z + 4.0f);
		TrySetEntityPos(g_xVillager, xVillagerPos);
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iRunFrames = 0;
		g_iPhase = kLS_RunFrames;
		return true;
	}

	case kLS_RunFrames:
	{
		++g_iRunFrames;
		if (g_iFrameTargetFirstSeen < 0)
		{
			const Zenith_EntityID xBB = ReadPriestBBTarget(g_xPriest);
			if (xBB.IsValid())
			{
				g_iFrameTargetFirstSeen = g_iRunFrames;
				g_xTargetAtFirstSighting = xBB;
			}
		}
		if (g_iFrameTargetFirstSeen >= 0 || g_iRunFrames >= kRUN_FRAMES_MAX)
		{
			g_iPhase = kLS_Verify;
		}
		return true;
	}

	case kLS_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1PursuesAfterLOS: firstSightingFrame=%d target=(%u/%u) expected=(%u/%u)",
			g_iFrameTargetFirstSeen,
			g_xTargetAtFirstSighting.m_uIndex, g_xTargetAtFirstSighting.m_uGeneration,
			g_xVillager.m_uIndex, g_xVillager.m_uGeneration);
		g_iPhase = kLS_Done;
		return false;

	case kLS_Done:
	default:
		return false;
	}
}

static bool Verify_P1PursuesAfterLOS()
{
	if (!g_xPriest.IsValid() || !g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1PursuesAfterLOS: priest or villager not found");
		return false;
	}
	if (g_iFrameTargetFirstSeen < 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1PursuesAfterLOS: priest's BB_KEY_TARGET_WITH_DEVIL never became valid in %d frames -- perception didn't fire on a 4 m face-on villager",
			kRUN_FRAMES_MAX);
		return false;
	}
	if (g_xTargetAtFirstSighting.m_uIndex != g_xVillager.m_uIndex
		|| g_xTargetAtFirstSighting.m_uGeneration != g_xVillager.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1PursuesAfterLOS: BB target = (%u/%u), expected possessed villager (%u/%u)",
			g_xTargetAtFirstSighting.m_uIndex, g_xTargetAtFirstSighting.m_uGeneration,
			g_xVillager.m_uIndex, g_xVillager.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1PursuesAfterLOSTest = {
	"Test_P1Priest_PursuesAfterLineOfSight",
	&Setup_P1PursuesAfterLOS,
	&Step_P1PursuesAfterLOS,
	&Verify_P1PursuesAfterLOS,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1PursuesAfterLOSTest);

#endif // ZENITH_INPUT_SIMULATOR
