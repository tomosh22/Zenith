#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"

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
//      every frame via DPPlayerController_Component::OnUpdate's
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

	// Place xVillager fDist metres along the priest's ACTUAL facing (read from
	// its transform), so it lands in the priest's sight cone regardless of the
	// procgen spawn orientation. The original test assumed the priest faced +Z
	// (authored yaw 0), but on procgen the navmesh agent faces its patrol
	// target, so a +Z-placed villager fell outside the FOV and sight never
	// fired. Re-call each frame as the priest rotates. (Matches the perception
	// system's forward = quat * +Z for a yaw-only agent.)
	void PlaceInPriestFOV(Zenith_EntityID xPriest, Zenith_EntityID xVillager, float fDist)
	{
		Zenith_Maths::Vector3 xPriestPos;
		if (!TryGetEntityPos(xPriest, xPriestPos)) return;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xPriest);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriest);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Quaternion xQuat;
		xEnt.GetComponent<Zenith_TransformComponent>().GetRotation(xQuat);
		// HORIZONTAL forward (zero the Y, renormalise) to match the perception
		// system's yaw-only sight forward, and place at the priest's OWN height.
		// A forward with any vertical component drops the villager off the sight
		// cone's vertical centre, and a far placement can land past a wall the
		// procgen priest happens to face -- both give awareness 0 in batch.
		Zenith_Maths::Vector3 xFwd = xQuat * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xFwd.y = 0.0f;
		const float fLen = std::sqrt(xFwd.x * xFwd.x + xFwd.z * xFwd.z);
		if (fLen > 1e-4f) { xFwd.x /= fLen; xFwd.z /= fLen; }
		Zenith_Maths::Vector3 xPos = xPriestPos + xFwd * fDist;
		xPos.y = xPriestPos.y;
		TrySetEntityPos(xVillager, xPos);
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
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Component&) { xFoundPriest = xId; });
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFoundVillager](Zenith_EntityID xId, DPVillager_Component&)
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
		// considers it. Production DPVillager_Component doesn't
		// self-register today (the omniscient fallback obviated that
		// for the pre-1.9 tests); a future PR can wire OnAwake
		// registration once the production gameplay loop needs sight
		// to feed the priest BT in non-fallback mode.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);

		// Place the villager along the priest's ACTUAL facing (see
		// PlaceInPriestFOV) so it lands in the sight cone regardless of the
		// procgen spawn orientation -- the old +Z assumption only held for the
		// retired hand-authored level.
		PlaceInPriestFOV(g_xPriest, g_xVillager, 2.0f);
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iRunFrames = 0;
		g_iPhase = kLS_RunFrames;
		return true;
	}

	case kLS_RunFrames:
	{
		// Re-place each frame: the priest rotates (navmesh patrol), so keep the
		// villager in its current FOV until sight acquires it.
		PlaceInPriestFOV(g_xPriest, g_xVillager, 2.0f);
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
	{
		const float fAware = Zenith_PerceptionSystem::GetAwarenessOf(g_xPriest, g_xVillager);
		const bool bIsAgent = Zenith_PerceptionSystem::GetPerceivedTargets(g_xPriest) != nullptr;
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		Zenith_Maths::Vector3 xPP(0.0f), xVP(0.0f);
		TryGetEntityPos(g_xPriest, xPP);
		TryGetEntityPos(g_xVillager, xVP);
		const float fDist = std::sqrt((xPP.x - xVP.x) * (xPP.x - xVP.x) + (xPP.z - xVP.z) * (xPP.z - xVP.z));
		Zenith_Log(LOG_CATEGORY_AI,
			"P1PursuesAfterLOS: firstSightingFrame=%d target=(%u/%u) expected=(%u/%u) aware=%.2f isAgent=%d possessed=(%u/%u) dist=%.1f",
			g_iFrameTargetFirstSeen,
			g_xTargetAtFirstSighting.m_uIndex, g_xTargetAtFirstSighting.m_uGeneration,
			g_xVillager.m_uIndex, g_xVillager.m_uGeneration,
			fAware, (int)bIsAgent, xPossessed.m_uIndex, xPossessed.m_uGeneration, fDist);
		g_iPhase = kLS_Done;
		return false;
	}

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
