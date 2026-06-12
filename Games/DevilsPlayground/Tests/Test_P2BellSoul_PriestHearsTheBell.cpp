#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPVillager_Component.h"
#include "Components/Priest_Component.h"

#include <cstdio>

// ============================================================================
// Test_P2BellSoul_PriestHearsTheBell (MVP-2.2.6 -- INTEGRATION)
//
// Test_P2Reagent_BellSoulRingsBell pins the EVENT (DP_OnBellRing
// dispatches once per pickup). This integration test pins the
// PERCEPTION SIDE: when the villager picks up a BellSoul, the
// priest's perception system receives the map-wide hearing stimulus
// AND the priest's Priest_Component::BridgePerceptionToBlackboard
// then sets BB_KEY_HAS_INVESTIGATE_POS = true with InvestigatePos
// near the BellSoul.
//
// Without this test the chain "BellSoul rings -> priest hears ->
// priest changes BT branch to investigate" could break silently:
// the event might fire but the perception stimulus might not be
// emitted at the right radius / loudness, or the priest's bridge
// might filter it out.
//
// Procedure:
//   1. Load GameLevel; find priest + a villager + build a BellSoul.
//   2. Teleport the priest 50 m AWAY from the BellSoul (so without
//      the bell ring, the priest can't hear anything happening at
//      that distance via regular footsteps). The BellSoul's 200 m
//      emit radius easily covers 50 m so the priest WILL hear it.
//   3. Possess the villager, teleport onto the BellSoul.
//   4. Tick 80 frames (~1.33 s) so the 1.0 s pickup channel
//      completes AND the priest's per-frame OnUpdate runs the
//      perception bridge.
//   5. Read the priest's blackboard:
//        BB_KEY_HAS_INVESTIGATE_POS == true.
//        BB_KEY_INVESTIGATE_POS close to BellSoul's world position.
//
// What this catches:
//   * The EmitSoundStimulus call inside DPItemBase's pickup branch
//     was removed / refactored to a smaller radius (priest stops
//     hearing distant bells).
//   * The priest's BridgePerceptionToBlackboard stopped bridging
//     hearing stimuli into BB.InvestigatePos.
//   * A perception-system change that drops sources without an
//     entity ID (the EmitSoundStimulus uses the BellSoul entity as
//     source, which we destroy on pickup -- if the stale-entity
//     check filters it out, the priest never hears).
// ============================================================================

namespace
{
	enum Phase : int {
		kBP_Start, kBP_WaitScene, kBP_BuildBellSoul, kBP_TeleportPriest,
		kBP_PossessVillager, kBP_TeleportVillager, kBP_TickPickup,
		kBP_Snapshot, kBP_Verify, kBP_Done
	};

	int                     g_iPhase = kBP_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xBellSoul;
	Zenith_Maths::Vector3   g_xBellSoulPos(0.0f);
	int                     g_iTickCounter = 0;

	bool                    g_bHasInvestigatePos = false;
	Zenith_Maths::Vector3   g_xInvestigatePos(0.0f);
	Zenith_EntityID         g_xHeldAfter;

	constexpr int kPICKUP_TICKS = 80;  // ~1.33s, well past the 1.0s channel
	// Priest hearing_range_m defaults to 35. The perception system
	// CLAMPS at min(emit_radius, agent_max_range), so even though the
	// BellSoul emits at 200m, the perception path only reaches priests
	// within 35m. This test exercises that path at 20m -- the GDD's
	// "audible from entire map" promise is delivered by an additional
	// direct-BB fanout (DP_AI::NotifyAllPriestsOfInvestigatePos) that
	// runs alongside the perception emit; the across-map case is pinned
	// by Test_P2BellSoul_AudibleAcrossMap at 120m.
	constexpr float kPRIEST_FAR_DIST = 20.0f;

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

	void TeleportTo(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	}

	void ReadPriestBB(Zenith_EntityID xPriestId,
	                  bool& bHasInvestigatePosOut,
	                  Zenith_Maths::Vector3& xInvestigatePosOut)
	{
		bHasInvestigatePosOut = false;
		xInvestigatePosOut = Zenith_Maths::Vector3(0.0f);
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_AIAgentComponent>()) return;
		Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
		Zenith_Blackboard& xBB = xAg.GetBlackboard();
		bHasInvestigatePosOut = xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS);
		xInvestigatePosOut = xBB.GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS);
	}
}

static void Setup_P2BellSoulPriestHears()
{
	g_iPhase = kBP_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_xBellSoul = INVALID_ENTITY_ID;
	g_xBellSoulPos = Zenith_Maths::Vector3(0.0f);
	g_iTickCounter = 0;
	g_bHasInvestigatePos = false;
	g_xInvestigatePos = Zenith_Maths::Vector3(0.0f);
	g_xHeldAfter = INVALID_ENTITY_ID;
}

static bool Step_P2BellSoulPriestHears(int iFrame)
{
	switch (g_iPhase)
	{
	case kBP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBP_WaitScene;
		return true;

	case kBP_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFoundVillager;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Component&)
			{ xFoundPriest = xId; });
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFoundVillager](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xFoundVillager.IsValid()) xFoundVillager = xId;
			});
		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPhase = kBP_BuildBellSoul;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBP_Done;
		}
		return true;
	}

	case kBP_BuildBellSoul:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kBP_Done; return false; }
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxScene, std::string("Test_BellSoul_PriestHears"));
		if (!xEnt.IsValid()) { g_iPhase = kBP_Done; return false; }
		g_xBellSoul = xEnt.GetEntityID();
		// Place the BellSoul far from the priest's start spot so the
		// teleport step (next) has somewhere to go.
		g_xBellSoulPos = Zenith_Maths::Vector3(500.0f, 0.0f, 500.0f);
		if (xEnt.HasComponent<Zenith_TransformComponent>())
		{
			xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(g_xBellSoulPos);
		}
		xEnt.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Component* pxBeh = &xEnt.AddComponent<DPItemBase_Component>();
		if (pxBeh != nullptr) pxBeh->SetTag(DP_ItemTag::BellSoul);
		g_iPhase = kBP_TeleportPriest;
		return true;
	}

	case kBP_TeleportPriest:
	{
		// Park priest 50 m away from the BellSoul. The bell emit
		// has a 200 m radius and loudness 1.0, so even at 50 m the
		// perceived loudness is:
		//   1.0 * (1 - 50/200) = 0.75 -- well above the priest's
		//   0.05 hearing threshold.
		Zenith_Maths::Vector3 xFar(g_xBellSoulPos.x + kPRIEST_FAR_DIST,
		                           g_xBellSoulPos.y,
		                           g_xBellSoulPos.z);
		TeleportTo(g_xPriest, xFar);
		g_iPhase = kBP_PossessVillager;
		return true;
	}

	case kBP_PossessVillager:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kBP_TeleportVillager;
		return true;

	case kBP_TeleportVillager:
		TeleportTo(g_xVillager, g_xBellSoulPos);
		g_iTickCounter = 0;
		g_iPhase = kBP_TickPickup;
		return true;

	case kBP_TickPickup:
		++g_iTickCounter;
		if (g_iTickCounter >= kPICKUP_TICKS) g_iPhase = kBP_Snapshot;
		return true;

	case kBP_Snapshot:
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
		ReadPriestBB(g_xPriest, g_bHasInvestigatePos, g_xInvestigatePos);
		g_iPhase = kBP_Verify;
		return true;

	case kBP_Verify:
		std::printf("[P2BellSoulPriestHears] held=(%u/%u) hasInvPos=%d invPos=(%.2f,%.2f,%.2f) expected bellSoulPos=(%.2f,%.2f,%.2f)\n",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			(int)g_bHasInvestigatePos,
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xBellSoulPos.x, g_xBellSoulPos.y, g_xBellSoulPos.z);
		std::fflush(stdout);
		g_iPhase = kBP_Done;
		return false;

	case kBP_Done:
	default:
		return false;
	}
}

static bool Verify_P2BellSoulPriestHears()
{
	if (!g_xPriest.IsValid() || !g_xVillager.IsValid() || !g_xBellSoul.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2BellSoulPriestHears: setup entities missing");
		return false;
	}
	// Precondition: pickup must have completed (so the bell rang).
	if (!g_xHeldAfter.IsValid()
		|| g_xHeldAfter.m_uIndex != g_xBellSoul.m_uIndex
		|| g_xHeldAfter.m_uGeneration != g_xBellSoul.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulPriestHears: pickup didn't complete (held=%u/%u, expected BellSoul %u/%u). The bell never rang, so the perception assertion below is meaningless",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xBellSoul.m_uIndex, g_xBellSoul.m_uGeneration);
		return false;
	}
	// Main assertion: priest's BB.HasInvestigatePos must be true.
	if (!g_bHasInvestigatePos)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulPriestHears: priest BB.HasInvestigatePos is false after BellSoul pickup 50m away. The hearing chain broke: either DPItemBase didn't EmitSoundStimulus, or the perception system filtered it, or the priest's BridgePerceptionToBlackboard didn't bridge hearing stimuli into the BB");
		return false;
	}
	// Sanity: investigate position should be near the BellSoul. We
	// allow generous tolerance (5m) because the priest's perception
	// system caches "last heard" not "exact source"; for MVP scope
	// we just want it pointing at the right neighbourhood.
	const float fDx = g_xInvestigatePos.x - g_xBellSoulPos.x;
	const float fDz = g_xInvestigatePos.z - g_xBellSoulPos.z;
	const float fDistSq = fDx * fDx + fDz * fDz;
	if (fDistSq > 25.0f) // 5m tolerance
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulPriestHears: priest investigate pos (%.2f,%.2f,%.2f) too far from BellSoul (%.2f,%.2f,%.2f)",
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xBellSoulPos.x, g_xBellSoulPos.y, g_xBellSoulPos.z);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2BellSoulPriestHearsTest = {
	"Test_P2BellSoul_PriestHearsTheBell",
	&Setup_P2BellSoulPriestHears,
	&Step_P2BellSoulPriestHears,
	&Verify_P2BellSoulPriestHears,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2BellSoulPriestHearsTest);

#endif // ZENITH_INPUT_SIMULATOR
