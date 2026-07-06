#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPVillager_Component.h"
#include "Components/Priest_Component.h"

#include <cstdio>

// ============================================================================
// Test_P2BellSoul_AudibleAcrossMap (MVP-2.2.6 GDD-PARITY -- INTEGRATION)
//
// Test_P2BellSoul_PriestHearsTheBell pins the WITHIN-RANGE perception
// path (priest 20 m from the bell). The GDD promises BellSoul is
// audible from the ENTIRE map -- the perception system's
// min(emit_radius, agent_max_range) clamp breaks that promise at the
// engine layer (a priest 100+ m away can't hear the bell even though
// the emit radius is 200 m, because the priest's hearing_range_m
// caps perception at 35 m).
//
// MVP-2.2.6 fix: DPItemBase's BellSoul branch now ALSO calls
// DP_AI::NotifyAllPriestsOfInvestigatePos which iterates every priest
// in the scene and writes BB.HasInvestigatePos=true + InvestigatePos
// directly, bypassing the perception clamp.
//
// This test pins THAT path: a priest 120 m away from the BellSoul
// receives the BB update.
//
// Procedure:
//   1. Load GameLevel; find priest + a villager + build a BellSoul.
//   2. Teleport priest 120 m EAST of the BellSoul -- well past the
//      perception clamp (30 m hearing range, 200 m emit). The
//      perception path would never fire here.
//   3. Possess the villager, teleport onto the BellSoul.
//   4. Tick 80 frames so the 1.0 s pickup channel completes AND the
//      priest's per-frame OnUpdate has had a chance to observe the
//      BB write.
//   5. Read priest's BB. HasInvestigatePos must be true, with
//      InvestigatePos near the BellSoul's world position.
//
// What this catches:
//   * Someone removes the NotifyAllPriestsOfInvestigatePos call from
//     DPItemBase's BellSoul branch (the "200 m emit covers everything"
//     comment that was wrong before MVP-2.2.6 returns).
//   * The helper's per-priest BB iteration regresses (e.g., starts
//     filtering by perception distance, defeating its purpose).
//   * The BridgePerceptionToBlackboard call overwrites BB.InvestigatePos
//     every frame with a default Vector3 when no perception hit lands,
//     erasing the direct write before the BT can read it.
// ============================================================================

namespace
{
	enum Phase : int {
		kBA_Start, kBA_WaitScene, kBA_BuildBellSoul, kBA_TeleportPriest,
		kBA_PossessVillager, kBA_TeleportVillager, kBA_TickPickup,
		kBA_Snapshot, kBA_Verify, kBA_Done
	};

	int                     g_iPhase = kBA_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xBellSoul;
	Zenith_Maths::Vector3   g_xBellSoulPos(0.0f);
	int                     g_iTickCounter = 0;

	bool                    g_bHasInvestigatePos = false;
	Zenith_Maths::Vector3   g_xInvestigatePos(0.0f);
	Zenith_EntityID         g_xHeldAfter;

	constexpr int kPICKUP_TICKS = 80;
	// 120 m: 4x the priest's 30 m hearing range. If the perception path
	// were the only one wired up, the bell would be silent here.
	constexpr float kPRIEST_FAR_DIST = 120.0f;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	void TeleportTo(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		pxTransform->SetPosition(xPos);
	}

	void ReadPriestBB(Zenith_EntityID xPriestId,
	                  bool& bHasInvestigatePosOut,
	                  Zenith_Maths::Vector3& xInvestigatePosOut)
	{
		bHasInvestigatePosOut = false;
		xInvestigatePosOut = Zenith_Maths::Vector3(0.0f);
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xPriestId);
		if (!xEnt.IsValid()) return;
		Priest_Component* pxPriestC = xEnt.TryGetComponent<Priest_Component>();
		Zenith_BehaviourGraph* pxGraph = pxPriestC ? pxPriestC->FindPriestGraph() : nullptr;
		if (pxGraph == nullptr) return;
		// W3: the bridge writes the priest's decision blackboard.
		bHasInvestigatePosOut = pxGraph->GetBlackboard().GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		xInvestigatePosOut = pxGraph->GetBlackboard().GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS, Zenith_Maths::Vector3(0.0f));
	}
}

static void Setup_P2BellSoulAudibleAcrossMap()
{
	g_iPhase = kBA_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_xBellSoul = INVALID_ENTITY_ID;
	g_xBellSoulPos = Zenith_Maths::Vector3(0.0f);
	g_iTickCounter = 0;
	g_bHasInvestigatePos = false;
	g_xInvestigatePos = Zenith_Maths::Vector3(0.0f);
	g_xHeldAfter = INVALID_ENTITY_ID;
}

static bool Step_P2BellSoulAudibleAcrossMap(int iFrame)
{
	switch (g_iPhase)
	{
	case kBA_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBA_WaitScene;
		return true;

	case kBA_WaitScene:
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
			g_iPhase = kBA_BuildBellSoul;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBA_Done;
		}
		return true;
	}

	case kBA_BuildBellSoul:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kBA_Done; return false; }
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxScene, std::string("Test_BellSoul_AudibleAcrossMap"));
		if (!xEnt.IsValid()) { g_iPhase = kBA_Done; return false; }
		g_xBellSoul = xEnt.GetEntityID();
		// Place the BellSoul far from the priest's start spot so the
		// teleport step (next) has somewhere to go.
		g_xBellSoulPos = Zenith_Maths::Vector3(500.0f, 0.0f, 500.0f);
		if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(g_xBellSoulPos);
		}
		xEnt.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Component* pxBeh = &xEnt.AddComponent<DPItemBase_Component>();
		if (pxBeh != nullptr) pxBeh->SetTag(DP_ItemTag::BellSoul);
		g_iPhase = kBA_TeleportPriest;
		return true;
	}

	case kBA_TeleportPriest:
	{
		// 120 m from the BellSoul. Priest's hearing_range_m is 35 m;
		// EmitSoundStimulus's perception path can't deliver here.
		// Only the direct BB fanout from NotifyAllPriestsOfInvestigatePos
		// will reach the priest at this distance.
		Zenith_Maths::Vector3 xFar(g_xBellSoulPos.x + kPRIEST_FAR_DIST,
		                           g_xBellSoulPos.y,
		                           g_xBellSoulPos.z);
		TeleportTo(g_xPriest, xFar);
		g_iPhase = kBA_PossessVillager;
		return true;
	}

	case kBA_PossessVillager:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kBA_TeleportVillager;
		return true;

	case kBA_TeleportVillager:
		TeleportTo(g_xVillager, g_xBellSoulPos);
		g_iTickCounter = 0;
		g_iPhase = kBA_TickPickup;
		return true;

	case kBA_TickPickup:
		++g_iTickCounter;
		if (g_iTickCounter >= kPICKUP_TICKS) g_iPhase = kBA_Snapshot;
		return true;

	case kBA_Snapshot:
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
		ReadPriestBB(g_xPriest, g_bHasInvestigatePos, g_xInvestigatePos);
		g_iPhase = kBA_Verify;
		return true;

	case kBA_Verify:
		std::printf("[P2BellSoulAudibleAcrossMap] held=(%u/%u) hasInvPos=%d invPos=(%.2f,%.2f,%.2f) expected bellSoulPos=(%.2f,%.2f,%.2f) priestDist=%.1fm\n",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			(int)g_bHasInvestigatePos,
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xBellSoulPos.x, g_xBellSoulPos.y, g_xBellSoulPos.z,
			kPRIEST_FAR_DIST);
		std::fflush(stdout);
		g_iPhase = kBA_Done;
		return false;

	case kBA_Done:
	default:
		return false;
	}
}

static bool Verify_P2BellSoulAudibleAcrossMap()
{
	if (!g_xPriest.IsValid() || !g_xVillager.IsValid() || !g_xBellSoul.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2BellSoulAudibleAcrossMap: setup entities missing");
		return false;
	}
	// Precondition: pickup must have completed.
	if (!g_xHeldAfter.IsValid()
		|| g_xHeldAfter.m_uIndex != g_xBellSoul.m_uIndex
		|| g_xHeldAfter.m_uGeneration != g_xBellSoul.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulAudibleAcrossMap: pickup didn't complete (held=%u/%u, expected BellSoul %u/%u)",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xBellSoul.m_uIndex, g_xBellSoul.m_uGeneration);
		return false;
	}
	// Main assertion: priest 120m away knows the bell rang.
	if (!g_bHasInvestigatePos)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulAudibleAcrossMap: priest at %.0fm did NOT receive the bell's investigate-pos. The map-wide BB fanout (DP_AI::NotifyAllPriestsOfInvestigatePos) regressed or DPItemBase's BellSoul branch stopped calling it -- the GDD's map-wide audibility is broken",
			kPRIEST_FAR_DIST);
		return false;
	}
	// Sanity: investigate position should be near the BellSoul. The
	// direct-write path delivers EXACTLY xMyPos so we use a tight
	// tolerance (the perception path's "last heard" can drift by a
	// few metres; the direct path doesn't).
	const float fDx = g_xInvestigatePos.x - g_xBellSoulPos.x;
	const float fDz = g_xInvestigatePos.z - g_xBellSoulPos.z;
	const float fDistSq = fDx * fDx + fDz * fDz;
	if (fDistSq > 1.0f) // 1m tolerance
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulAudibleAcrossMap: priest investigate pos (%.2f,%.2f,%.2f) too far from BellSoul (%.2f,%.2f,%.2f). The direct fanout should write xMyPos exactly; >1m drift implies either the perception path overwrote the direct write, or the helper sourced the position from somewhere other than the BellSoul",
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xBellSoulPos.x, g_xBellSoulPos.y, g_xBellSoulPos.z);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2BellSoulAudibleAcrossMapTest = {
	"Test_P2BellSoul_AudibleAcrossMap",
	&Setup_P2BellSoulAudibleAcrossMap,
	&Step_P2BellSoulAudibleAcrossMap,
	&Verify_P2BellSoulAudibleAcrossMap,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2BellSoulAudibleAcrossMapTest);

#endif // ZENITH_INPUT_SIMULATOR
