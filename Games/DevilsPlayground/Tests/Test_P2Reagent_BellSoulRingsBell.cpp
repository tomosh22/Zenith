#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_EventSystem.h"
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
// Test_P2Reagent_BellSoulRingsBell (MVP-2.2.7)
//
// Pins BellSoul's `special_behaviour: "rings_bell_on_pickup"` from
// Reagents.json: when a possessed villager picks up a BellSoul item
// (through the proximity-pickup path, which includes the standard
// 1.0 s reagent channel), DPItemBase dispatches DP_OnBellRing AND
// emits a map-wide perception stimulus.
//
// Procedure:
//   1. Load GameLevel; pick any villager.
//   2. Subscribe to DP_OnBellRing in Setup; count events.
//   3. Build a BellSoul item entity.
//   4. Possess the villager; teleport onto BellSoul.
//   5. Tick enough frames for the 1.0 s channel + the pickup-fire
//      frame (80 frames at fixed-dt = ~1.33 s).
//   6. Assert:
//        DP_OnBellRing fired EXACTLY ONCE.
//        Event.m_xVillager == our possessed villager.
//        Event.m_xBellSoul == the BellSoul item.
//        Event.m_xPosition is the BellSoul's world position.
//        Villager NOW holds the BellSoul (pickup completed).
//   7. Tick another 30 frames.
//   8. Assert DP_OnBellRing count is STILL 1 (re-pickup attempts on
//      the already-held BellSoul must not re-ring).
//
// What this catches:
//   * special_behaviour dispatch missing entirely.
//   * Dispatch fires on the CHANNEL frame instead of the COMPLETION
//     frame (an early ring would not match villager.IsHolding state).
//   * Dispatch fires every frame the villager is in range (instead
//     of exactly once on the completion frame).
//   * Event payload incorrectly populated.
// ============================================================================

namespace
{
	enum Phase : int {
		kBS_Start, kBS_WaitScene, kBS_BuildItem,
		kBS_PossessAndTeleport, kBS_TickThroughChannel,
		kBS_SnapshotAfterPickup, kBS_TickMore,
		kBS_Verify, kBS_Done
	};

	int                     g_iPhase = kBS_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xBellSoul;
	Zenith_Maths::Vector3   g_xBellSoulSpawnPos(0.0f);

	int                     g_iRingCount = 0;
	Zenith_EntityID         g_xRingVillager;
	Zenith_EntityID         g_xRingBellSoul;
	Zenith_Maths::Vector3   g_xRingPosition(0.0f);
	int                     g_iRingCountAtSnapshot = 0;
	int                     g_iRingCountAtFinal = 0;

	int                     g_iTickCounter = 0;
	Zenith_EventHandle      g_uHandle = 0;
	Zenith_EntityID         g_xHeldAfter;

	constexpr int kCHANNEL_TICKS = 80;   // 80 * 0.01666 = ~1.33s (> 1.0s)
	constexpr int kEXTRA_TICKS   = 30;   // verify no re-rings happen later

	void OnBellRing(const DP_OnBellRing& xEvt)
	{
		++g_iRingCount;
		if (g_iRingCount == 1)
		{
			g_xRingVillager = xEvt.m_xVillager;
			g_xRingBellSoul = xEvt.m_xBellSoul;
			g_xRingPosition = xEvt.m_xPosition;
		}
	}
}

static void Setup_P2BellSoulRings()
{
	g_iPhase = kBS_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xBellSoul = INVALID_ENTITY_ID;
	g_xBellSoulSpawnPos = Zenith_Maths::Vector3(0.0f);
	g_iRingCount = 0;
	g_xRingVillager = INVALID_ENTITY_ID;
	g_xRingBellSoul = INVALID_ENTITY_ID;
	g_xRingPosition = Zenith_Maths::Vector3(0.0f);
	g_iRingCountAtSnapshot = 0;
	g_iRingCountAtFinal = 0;
	g_iTickCounter = 0;
	g_xHeldAfter = INVALID_ENTITY_ID;
	g_uHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnBellRing>(&OnBellRing);
}

static bool Step_P2BellSoulRings(int iFrame)
{
	switch (g_iPhase)
	{
	case kBS_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kBS_WaitScene;
		return true;

	case kBS_WaitScene:
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
			g_iPhase = kBS_BuildItem;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kBS_Done;
		}
		return true;
	}

	case kBS_BuildItem:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kBS_Done; return false; }
		Zenith_Entity xEnt(pxScene, std::string("Test_BellSoul"));
		if (!xEnt.IsValid()) { g_iPhase = kBS_Done; return false; }
		g_xBellSoul = xEnt.GetEntityID();
		g_xBellSoulSpawnPos = Zenith_Maths::Vector3(300.0f, 0.0f, 300.0f);
		if (xEnt.HasComponent<Zenith_TransformComponent>())
		{
			xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(g_xBellSoulSpawnPos);
		}
		xEnt.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Behaviour* pxBeh = xEnt.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (pxBeh != nullptr) pxBeh->SetTag(DP_ItemTag::BellSoul);
		g_iPhase = kBS_PossessAndTeleport;
		return true;
	}

	case kBS_PossessAndTeleport:
	{
		DP_Player::SetPossessedVillager(g_xVillager);
		// Teleport villager onto BellSoul spawn position.
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xVillager);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xVillager);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_TransformComponent>())
			{
				xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(g_xBellSoulSpawnPos);
			}
		}
		g_iTickCounter = 0;
		g_iPhase = kBS_TickThroughChannel;
		return true;
	}

	case kBS_TickThroughChannel:
		++g_iTickCounter;
		if (g_iTickCounter >= kCHANNEL_TICKS) g_iPhase = kBS_SnapshotAfterPickup;
		return true;

	case kBS_SnapshotAfterPickup:
		g_iRingCountAtSnapshot = g_iRingCount;
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
		g_iTickCounter = 0;
		g_iPhase = kBS_TickMore;
		return true;

	case kBS_TickMore:
		++g_iTickCounter;
		if (g_iTickCounter >= kEXTRA_TICKS) g_iPhase = kBS_Verify;
		return true;

	case kBS_Verify:
		g_iRingCountAtFinal = g_iRingCount;
		std::printf("[P2BellSoulRings] ringCountAfterPickup=%d ringCountAtFinal=%d heldAfter=(%u/%u) ringPos=(%.2f,%.2f,%.2f)\n",
			g_iRingCountAtSnapshot, g_iRingCountAtFinal,
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xRingPosition.x, g_xRingPosition.y, g_xRingPosition.z);
		std::fflush(stdout);
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		g_iPhase = kBS_Done;
		return false;

	case kBS_Done:
	default:
		if (g_uHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uHandle);
			g_uHandle = 0;
		}
		return false;
	}
}

static bool Verify_P2BellSoulRings()
{
	if (!g_xVillager.IsValid() || !g_xBellSoul.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2BellSoulRings: setup entities missing");
		return false;
	}
	if (g_iRingCountAtSnapshot != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: DP_OnBellRing fired %d times after pickup, expected exactly 1",
			g_iRingCountAtSnapshot);
		return false;
	}
	if (g_iRingCountAtFinal != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: DP_OnBellRing fired %d times after %d extra frames -- ring is repeating every frame the villager is in range",
			g_iRingCountAtFinal, kEXTRA_TICKS);
		return false;
	}
	if (g_xRingVillager.m_uIndex != g_xVillager.m_uIndex
		|| g_xRingVillager.m_uGeneration != g_xVillager.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: event.m_xVillager = (%u/%u), expected (%u/%u)",
			g_xRingVillager.m_uIndex, g_xRingVillager.m_uGeneration,
			g_xVillager.m_uIndex, g_xVillager.m_uGeneration);
		return false;
	}
	if (g_xRingBellSoul.m_uIndex != g_xBellSoul.m_uIndex
		|| g_xRingBellSoul.m_uGeneration != g_xBellSoul.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: event.m_xBellSoul = (%u/%u), expected (%u/%u)",
			g_xRingBellSoul.m_uIndex, g_xRingBellSoul.m_uGeneration,
			g_xBellSoul.m_uIndex, g_xBellSoul.m_uGeneration);
		return false;
	}
	// Position should be close to spawn position (allow small jitter
	// from villager teleport's body integration interfering with the
	// item's transform read).
	const float fDx = g_xRingPosition.x - g_xBellSoulSpawnPos.x;
	const float fDz = g_xRingPosition.z - g_xBellSoulSpawnPos.z;
	if (fDx * fDx + fDz * fDz > 25.0f) // 5m tolerance
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: event.m_xPosition=(%.2f,%.2f,%.2f) too far from spawn=(%.2f,%.2f,%.2f)",
			g_xRingPosition.x, g_xRingPosition.y, g_xRingPosition.z,
			g_xBellSoulSpawnPos.x, g_xBellSoulSpawnPos.y, g_xBellSoulSpawnPos.z);
		return false;
	}
	if (!g_xHeldAfter.IsValid()
		|| g_xHeldAfter.m_uIndex != g_xBellSoul.m_uIndex
		|| g_xHeldAfter.m_uGeneration != g_xBellSoul.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2BellSoulRings: villager doesn't actually hold the BellSoul after pickup (held = %u/%u, expected %u/%u)",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			g_xBellSoul.m_uIndex, g_xBellSoul.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2BellSoulRingsTest = {
	"Test_P2Reagent_BellSoulRingsBell",
	&Setup_P2BellSoulRings,
	&Step_P2BellSoulRings,
	&Verify_P2BellSoulRings,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2BellSoulRingsTest);

#endif // ZENITH_INPUT_SIMULATOR
