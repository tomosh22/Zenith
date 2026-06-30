#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DP_Reagents.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPVillager_Component.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P2Reagent_UniquePickupChannel (MVP-2.2.3)
//
// Pins the reagent pickup-channel contract: an item with reagent tag
// requires the possessed villager to stay in pickup range for
// `pickup_channel_s` seconds (1.0 s for MVP reagents per
// Reagents.json) before pickup fires. Non-reagent items (Iron, Key,
// Objectives) pick up immediately as they do today.
//
// Three things are pinned together:
//   1. DP_Reagents::Initialize ran during Project_Initialise (it
//      can be queried at OnAwake time).
//   2. DPItemBase::OnAwake reads the reagent's pickup_channel_s and
//      stamps m_fPickupChannelDuration to it.
//   3. DPItemBase::OnUpdate ticks the channel only while the
//      possessed villager is in range; pickup fires when the timer
//      reaches 0.
//
// Procedure (one scene, two phases):
//
//   PHASE A (reagent slow pickup):
//     1. Load GameLevel; pick any villager.
//     2. Construct a BogWater item entity in the scene (a reagent --
//        Reagents.json has pickup_channel_s = 1.0).
//     3. Possess the villager; teleport onto the BogWater.
//     4. Tick a few frames -- not enough for the channel to complete.
//     5. Assert villager holds NOTHING yet, and the item's channel
//        state shows we're mid-flight (channeling villager = us,
//        remaining > 0 but <= the configured 1.0 s).
//     6. Tick many more frames (>1.0 s total).
//     7. Assert villager NOW holds the BogWater.
//
//   PHASE B (tool immediate pickup):
//     8. Construct an Iron item entity (NOT a reagent -- channel
//        duration should default to 0).
//     9. Drop the held BogWater (RemoveHeldItem) so we can pick up
//        the Iron.
//    10. Teleport onto the Iron.
//    11. Tick a small number of frames (3, well under any channel).
//    12. Assert villager holds the Iron immediately.
//
// What this catches:
//   * DP_Reagents::Initialize wasn't called (TryGet returns nullptr;
//     channel duration stays 0; reagent picks up instantly).
//   * DPItemBase::OnAwake didn't read pickup_channel_s.
//   * The channel state machine misuses fDt (e.g., decrements by
//     wall-clock instead of game-time).
//   * Channel runs for non-reagents too (Iron should still pick up
//     immediately).
// ============================================================================

namespace
{
	enum Phase : int {
		kR_Start, kR_WaitScene,
		kR_BuildReagent, kR_PossessAndTeleportToReagent,
		kR_TickPartial, kR_PartialSnapshot,
		kR_TickRest, kR_FullSnapshot,
		kR_DropReagent, kR_BuildIron,
		kR_TeleportToIron, kR_TickIronShort,
		kR_IronSnapshot, kR_Verify, kR_Done
	};

	int                     g_iPhase = kR_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xBogWater;
	Zenith_EntityID         g_xIron;
	DPItemBase_Component*   g_pxBogWaterBeh = nullptr;
	DPItemBase_Component*   g_pxIronBeh = nullptr;

	int                     g_iTickCounter = 0;

	// Mid-channel snapshot
	Zenith_EntityID         g_xHeldMid;
	float                   g_fChannelRemainingMid = -1.0f;
	float                   g_fChannelDurationMid = -1.0f;

	// Post-channel snapshot (after the 1.0s completes)
	Zenith_EntityID         g_xHeldFull;

	// Iron snapshot (after a few frames)
	Zenith_EntityID         g_xHeldIron;
	float                   g_fIronChannelDuration = -1.0f;

	// 1.0 s channel duration at fixed-dt 0.01666 = ~60 frames.
	// 30 frames = ~0.5 s -- well under the threshold.
	constexpr int kPARTIAL_TICKS = 30;
	// Another 50 frames = total ~80 = ~1.33 s, comfortably past 1.0 s.
	constexpr int kREST_TICKS    = 50;
	// 3 frames for the Iron pickup -- way under any channel threshold.
	constexpr int kIRON_TICKS    = 3;

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

	Zenith_EntityID BuildItem(const char* szName, DP_ItemTag eTag,
	                          const Zenith_Maths::Vector3& xPos,
	                          DPItemBase_Component** ppxOutBeh)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) return INVALID_ENTITY_ID;
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxScene, std::string(szName));
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->SetPosition(xPos);
		}
		xEnt.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Component* pxBeh = &xEnt.AddComponent<DPItemBase_Component>();
		if (pxBeh != nullptr) pxBeh->SetTag(eTag);
		if (ppxOutBeh != nullptr) *ppxOutBeh = pxBeh;
		return xEnt.GetEntityID();
	}
}

static void Setup_P2ReagentChannel()
{
	g_iPhase = kR_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xBogWater = INVALID_ENTITY_ID;
	g_xIron = INVALID_ENTITY_ID;
	g_pxBogWaterBeh = nullptr;
	g_pxIronBeh = nullptr;
	g_iTickCounter = 0;
	g_xHeldMid = INVALID_ENTITY_ID;
	g_fChannelRemainingMid = -1.0f;
	g_fChannelDurationMid = -1.0f;
	g_xHeldFull = INVALID_ENTITY_ID;
	g_xHeldIron = INVALID_ENTITY_ID;
	g_fIronChannelDuration = -1.0f;
}

static bool Step_P2ReagentChannel(int iFrame)
{
	switch (g_iPhase)
	{
	case kR_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kR_WaitScene;
		return true;

	case kR_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFound](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kR_BuildReagent;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kR_Done;
		}
		return true;
	}

	case kR_BuildReagent:
	{
		Zenith_Maths::Vector3 xPos(100.0f, 0.0f, 100.0f); // arbitrary
		g_xBogWater = BuildItem("Test_BogWater", DP_ItemTag::BogWater,
			xPos, &g_pxBogWaterBeh);
		g_iPhase = kR_PossessAndTeleportToReagent;
		return true;
	}

	case kR_PossessAndTeleportToReagent:
	{
		DP_Player::SetPossessedVillager(g_xVillager);
		Zenith_Maths::Vector3 xPos;
		if (TryGetEntityPos(g_xBogWater, xPos)) TeleportTo(g_xVillager, xPos);
		g_iTickCounter = 0;
		g_iPhase = kR_TickPartial;
		return true;
	}

	case kR_TickPartial:
		++g_iTickCounter;
		if (g_iTickCounter >= kPARTIAL_TICKS) g_iPhase = kR_PartialSnapshot;
		return true;

	case kR_PartialSnapshot:
		g_xHeldMid = DP_Player::GetHeldItemEntity(g_xVillager);
		if (g_pxBogWaterBeh != nullptr)
		{
			g_fChannelRemainingMid = g_pxBogWaterBeh->GetChannelRemainingForTest();
			g_fChannelDurationMid = g_pxBogWaterBeh->GetPickupChannelDurationForTest();
		}
		g_iTickCounter = 0;
		g_iPhase = kR_TickRest;
		return true;

	case kR_TickRest:
		++g_iTickCounter;
		if (g_iTickCounter >= kREST_TICKS) g_iPhase = kR_FullSnapshot;
		return true;

	case kR_FullSnapshot:
		g_xHeldFull = DP_Player::GetHeldItemEntity(g_xVillager);
		g_iPhase = kR_DropReagent;
		return true;

	case kR_DropReagent:
		// Drop the BogWater so Phase B (Iron) can pick up.
		DP_Player::RemoveHeldItem(g_xVillager);
		g_iPhase = kR_BuildIron;
		return true;

	case kR_BuildIron:
	{
		Zenith_Maths::Vector3 xPos(105.0f, 0.0f, 100.0f);
		g_xIron = BuildItem("Test_Iron", DP_ItemTag::Iron, xPos, &g_pxIronBeh);
		g_iPhase = kR_TeleportToIron;
		return true;
	}

	case kR_TeleportToIron:
	{
		Zenith_Maths::Vector3 xPos;
		if (TryGetEntityPos(g_xIron, xPos)) TeleportTo(g_xVillager, xPos);
		g_iTickCounter = 0;
		g_iPhase = kR_TickIronShort;
		return true;
	}

	case kR_TickIronShort:
		++g_iTickCounter;
		if (g_iTickCounter >= kIRON_TICKS) g_iPhase = kR_IronSnapshot;
		return true;

	case kR_IronSnapshot:
		g_xHeldIron = DP_Player::GetHeldItemEntity(g_xVillager);
		if (g_pxIronBeh != nullptr)
		{
			g_fIronChannelDuration = g_pxIronBeh->GetPickupChannelDurationForTest();
		}
		g_iPhase = kR_Verify;
		return true;

	case kR_Verify:
		std::printf("[P2ReagentChannel] mid:held=(%u/%u) channelRem=%.3f channelDur=%.3f post:held=(%u/%u) iron:held=(%u/%u) ironChannelDur=%.3f\n",
			g_xHeldMid.m_uIndex, g_xHeldMid.m_uGeneration,
			g_fChannelRemainingMid, g_fChannelDurationMid,
			g_xHeldFull.m_uIndex, g_xHeldFull.m_uGeneration,
			g_xHeldIron.m_uIndex, g_xHeldIron.m_uGeneration,
			g_fIronChannelDuration);
		std::fflush(stdout);
		g_iPhase = kR_Done;
		return false;

	case kR_Done:
	default:
		return false;
	}
}

static bool Verify_P2ReagentChannel()
{
	if (!g_xVillager.IsValid() || !g_xBogWater.IsValid() || !g_xIron.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ReagentChannel: setup entities missing");
		return false;
	}
	// PHASE A assertions.
	if (g_fChannelDurationMid < 0.5f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: BogWater channel duration is %.3f, expected ~1.0 (from Reagents.json). DP_Reagents not initialized, or DPItemBase didn't read pickup_channel_s at OnAwake",
			g_fChannelDurationMid);
		return false;
	}
	if (g_xHeldMid.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: villager held the BogWater after only %d ticks (~0.5s) -- the channel should not have completed yet (configured %.3fs)",
			kPARTIAL_TICKS, g_fChannelDurationMid);
		return false;
	}
	if (g_fChannelRemainingMid <= 0.0f || g_fChannelRemainingMid >= g_fChannelDurationMid)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: mid-channel remaining is %.3f (duration %.3f). Expected 0 < remaining < duration. Either the channel never started or it already completed.",
			g_fChannelRemainingMid, g_fChannelDurationMid);
		return false;
	}
	if (!g_xHeldFull.IsValid()
		|| g_xHeldFull.m_uIndex != g_xBogWater.m_uIndex
		|| g_xHeldFull.m_uGeneration != g_xBogWater.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: after full channel + extra ticks villager holds (%u/%u), expected BogWater=(%u/%u). Channel didn't complete OR pickup fired onto the wrong entity",
			g_xHeldFull.m_uIndex, g_xHeldFull.m_uGeneration,
			g_xBogWater.m_uIndex, g_xBogWater.m_uGeneration);
		return false;
	}
	// PHASE B assertions.
	if (g_fIronChannelDuration != 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: Iron channel duration is %.3f, expected 0 (tools have no channel)",
			g_fIronChannelDuration);
		return false;
	}
	if (!g_xHeldIron.IsValid()
		|| g_xHeldIron.m_uIndex != g_xIron.m_uIndex
		|| g_xHeldIron.m_uGeneration != g_xIron.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ReagentChannel: after %d ticks villager holds (%u/%u), expected Iron=(%u/%u). The non-reagent immediate-pickup path didn't fire",
			kIRON_TICKS,
			g_xHeldIron.m_uIndex, g_xHeldIron.m_uGeneration,
			g_xIron.m_uIndex, g_xIron.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ReagentChannelTest = {
	"Test_P2Reagent_UniquePickupChannel",
	&Setup_P2ReagentChannel,
	&Step_P2ReagentChannel,
	&Verify_P2ReagentChannel,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ReagentChannelTest);

#endif // ZENITH_INPUT_SIMULATOR
