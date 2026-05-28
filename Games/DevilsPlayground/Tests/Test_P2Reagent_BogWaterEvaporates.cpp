#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Reagent_BogWaterEvaporates (MVP-2.2.5)
//
// Pins BogWater's `special_behaviour: "evaporates_after_drop"` from
// Reagents.json: once a BogWater item is dropped (BeginPostDropCooldown
// fires from DPPlayerController::HandleDropItem), an 8 s evaporate
// timer starts. After 8 s the entity is destroyed.
//
// Procedure:
//   1. Load GameLevel; pick any villager.
//   2. Build a BogWater item entity.
//   3. Hand it to the villager via SetHeldItem (bypassing pickup
//      proximity -- the channel test already covers that path).
//   4. Call DP_Player::RemoveHeldItem + BeginPostDropCooldown on
//      the item directly. This is the exact sequence
//      DPPlayerController::HandleDropItem would have produced;
//      doing it directly keeps the test simulator-light.
//   5. Snapshot mid-drop state:
//        evaporateRemaining > 0 AND <= 8 s
//        evaporateDuration == 8 s (from JSON)
//        special_behaviour == "evaporates_after_drop"
//      The entity is still valid.
//   6. Tick 540 frames at fixed-dt 0.01666 = ~9 s, comfortably past
//      the 8 s threshold.
//   7. Assert the entity is DESTROYED (TryGetEntity returns invalid).
//
// What this catches:
//   * DP_Reagents loaded BogWater but didn't surface
//     special_behaviour/evaporate_duration_s.
//   * BeginPostDropCooldown didn't kick off the evaporate timer.
//   * OnUpdate doesn't decrement m_fEvaporateRemaining (or uses
//     wrong sign).
//   * The destroy path on zero-crossing failed (timer ticked but
//     entity remains).
//   * A non-evaporating item (e.g., Iron) would ALSO destroy under
//     this code path (the special_behaviour check protects against
//     this -- but the negative case lives in
//     Test_P2Reagent_NonReagentDoesNotEvaporate as a sibling).
// ============================================================================

namespace
{
	enum Phase : int {
		kBV_Start, kBV_WaitScene, kBV_BuildItem, kBV_Hand,
		kBV_Drop, kBV_SnapshotMid, kBV_TickEvaporate,
		kBV_SnapshotPost, kBV_Verify, kBV_Done
	};

	int                     g_iPhase = kBV_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xBogWater;
	DPItemBase_Behaviour*   g_pxBeh = nullptr;

	float                   g_fEvaporateRemainingMid = -1.0f;
	float                   g_fEvaporateDurationMid = -1.0f;
	std::string             g_strSpecialBehaviourMid;
	bool                    g_bEntityStillValidMid = false;
	bool                    g_bEntityStillValidPost = true;  // sentinel: must become false
	int                     g_iTickCounter = 0;

	// 540 frames at fixed-dt 0.01666 = ~9 s. Comfortably past 8 s
	// without burning excess frames.
	constexpr int kEVAPORATE_TICKS = 540;

	bool IsEntityValid(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		return xEnt.IsValid();
	}
}

static void Setup_P2BogWaterEvap()
{
	g_iPhase = kBV_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xBogWater = INVALID_ENTITY_ID;
	g_pxBeh = nullptr;
	g_fEvaporateRemainingMid = -1.0f;
	g_fEvaporateDurationMid = -1.0f;
	g_strSpecialBehaviourMid.clear();
	g_bEntityStillValidMid = false;
	g_bEntityStillValidPost = true;
	g_iTickCounter = 0;
}

static bool Step_P2BogWaterEvap(int iFrame)
{
	switch (g_iPhase)
	{
	case kBV_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBV_WaitScene;
		return true;

	case kBV_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kBV_BuildItem;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBV_Done;
		}
		return true;
	}

	case kBV_BuildItem:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kBV_Done; return false; }
		Zenith_Entity xEnt(pxScene, std::string("Test_BogWaterEvap"));
		if (!xEnt.IsValid()) { g_iPhase = kBV_Done; return false; }
		g_xBogWater = xEnt.GetEntityID();
		if (xEnt.HasComponent<Zenith_TransformComponent>())
		{
			xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(200.0f, 0.0f, 200.0f));
		}
		xEnt.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		g_pxBeh = xEnt.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (g_pxBeh != nullptr) g_pxBeh->SetTag(DP_ItemTag::BogWater);
		g_iPhase = kBV_Hand;
		return true;
	}

	case kBV_Hand:
		// Hand directly via SetHeldItem -- the pickup-channel test
		// covers the proximity path; here we want to exercise the
		// DROP path quickly without 60 frames of channel waiting.
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::SetHeldItem(g_xVillager, g_xBogWater);
		g_iPhase = kBV_Drop;
		return true;

	case kBV_Drop:
		// Drop sequence: clear the side-table AND fire the same
		// BeginPostDropCooldown path that HandleDropItem would have
		// triggered. The evaporate timer arms here.
		DP_Player::RemoveHeldItem(g_xVillager);
		if (g_pxBeh != nullptr) g_pxBeh->BeginPostDropCooldown();
		g_iPhase = kBV_SnapshotMid;
		return true;

	case kBV_SnapshotMid:
		if (g_pxBeh != nullptr)
		{
			g_fEvaporateRemainingMid = g_pxBeh->GetEvaporateRemainingForTest();
			g_fEvaporateDurationMid = g_pxBeh->GetEvaporateDurationForTest();
			g_strSpecialBehaviourMid = g_pxBeh->GetSpecialBehaviourForTest();
		}
		g_bEntityStillValidMid = IsEntityValid(g_xBogWater);
		g_iTickCounter = 0;
		g_iPhase = kBV_TickEvaporate;
		return true;

	case kBV_TickEvaporate:
		++g_iTickCounter;
		if (g_iTickCounter >= kEVAPORATE_TICKS) g_iPhase = kBV_SnapshotPost;
		return true;

	case kBV_SnapshotPost:
		g_bEntityStillValidPost = IsEntityValid(g_xBogWater);
		g_iPhase = kBV_Verify;
		return true;

	case kBV_Verify:
		std::printf("[P2BogWaterEvap] mid: rem=%.3f dur=%.3f beh=\"%s\" entValid=%d post: entValid=%d\n",
			g_fEvaporateRemainingMid, g_fEvaporateDurationMid,
			g_strSpecialBehaviourMid.c_str(),
			(int)g_bEntityStillValidMid, (int)g_bEntityStillValidPost);
		std::fflush(stdout);
		g_iPhase = kBV_Done;
		return false;

	case kBV_Done:
	default:
		return false;
	}
}

static bool Verify_P2BogWaterEvap()
{
	if (!g_xVillager.IsValid() || !g_xBogWater.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2BogWaterEvap: setup entities missing");
		return false;
	}
	if (g_strSpecialBehaviourMid != "evaporates_after_drop")
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BogWaterEvap: BogWater special_behaviour is \"%s\", expected \"evaporates_after_drop\" -- DP_Reagents didn't surface the JSON field, or DPItemBase didn't read it",
			g_strSpecialBehaviourMid.c_str());
		return false;
	}
	if (g_fEvaporateDurationMid < 4.0f || g_fEvaporateDurationMid > 12.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BogWaterEvap: evaporate duration is %.3f s, expected ~8 s (Reagents.json BogWater.evaporate_duration_s)",
			g_fEvaporateDurationMid);
		return false;
	}
	if (g_fEvaporateRemainingMid <= 0.0f
		|| g_fEvaporateRemainingMid > g_fEvaporateDurationMid + 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BogWaterEvap: mid-drop remaining is %.3f (expected 0 < remaining <= %.3f). BeginPostDropCooldown didn't arm the timer, or it armed to the wrong value",
			g_fEvaporateRemainingMid, g_fEvaporateDurationMid);
		return false;
	}
	if (!g_bEntityStillValidMid)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BogWaterEvap: entity already destroyed at the mid-drop snapshot -- destroy fired prematurely");
		return false;
	}
	if (g_bEntityStillValidPost)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BogWaterEvap: entity STILL valid after %d ticks (~%.2f s, well past the %.3f s timer). The evaporate->destroy path didn't fire",
			kEVAPORATE_TICKS, kEVAPORATE_TICKS * 0.01666f, g_fEvaporateDurationMid);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2BogWaterEvapTest = {
	"Test_P2Reagent_BogWaterEvaporates",
	&Setup_P2BogWaterEvap,
	&Step_P2BogWaterEvap,
	&Verify_P2BogWaterEvap,
	600
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2BogWaterEvapTest);

#endif // ZENITH_INPUT_SIMULATOR
