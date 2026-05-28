#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Forge_PriestHearsTheHammer (MVP-2.3.4 INTEGRATION)
//
// Test_P2Forge_AudibleAt30m pins the AudioBus instrumentation (a
// "DP.Forge.Hammer" sound is emitted with the right loudness/radius).
// This integration test pins the rest of the chain: after the forge
// craft, the priest's perception system receives the emit AND the
// priest's BridgePerceptionToBlackboard sets BB.HasInvestigatePos
// near the forge.
//
// The chain it pins:
//   craft -> Zenith_AudioBus::EmitSound -- via DPForge::HandleInteract
//   craft -> Zenith_PerceptionSystem::EmitSoundStimulus -- WAIT.
//
// CRITICAL DETAIL: DPForge currently emits via Zenith_AudioBus only,
// NOT Zenith_PerceptionSystem. The AudioBus is a test instrumentation
// hook; the perception system is the actual priest-hearing path.
// If the forge wants the priest to hear it, the forge needs BOTH:
//   1. AudioBus emit (test instrumentation -- already done).
//   2. PerceptionSystem::EmitSoundStimulus (priest-hearing path).
//
// This integration test surfaces that gap. If it FAILS, the fix is
// to add the perception-system emit in DPForge::HandleInteract
// alongside the AudioBus call. The audible-at-30m tuning values
// already exist (interactables.forge_audible_at_m,
// forge_audible_loudness) -- both calls share them.
//
// Procedure:
//   1. Load GameLevel; find priest + a villager + build a forge near
//      the priest.
//   2. Teleport priest to within 20m of the forge.
//   3. Hand the villager an Iron item, position near the forge.
//   4. Fire CraftForTest (the forge sees Iron, consumes it, spawns
//      Key, calls EmitSound + EmitSoundStimulus).
//   5. Tick a few frames for the priest's BridgePerceptionToBlackboard.
//   6. Assert priest's BB.HasInvestigatePos == true with the
//      InvestigatePos near the forge.
// ============================================================================

namespace
{
	enum Phase : int {
		kFP_Start, kFP_WaitScene, kFP_BuildForge, kFP_TeleportPriest,
		kFP_BuildInput, kFP_HandToVillager, kFP_Craft,
		kFP_TickForBridge, kFP_Snapshot, kFP_Verify, kFP_Done
	};

	int                     g_iPhase = kFP_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xForge;
	Zenith_EntityID         g_xInput;
	DPForge_Behaviour*      g_pxForge = nullptr;
	Zenith_Maths::Vector3   g_xForgePos(0.0f);

	int                     g_iTickCounter = 0;
	bool                    g_bHasInvestigatePos = false;
	Zenith_Maths::Vector3   g_xInvestigatePos(0.0f);

	constexpr int kBRIDGE_TICKS = 10;
	constexpr float kPRIEST_DIST = 20.0f;

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
	                  bool& bHasInvOut, Zenith_Maths::Vector3& xInvOut)
	{
		bHasInvOut = false;
		xInvOut = Zenith_Maths::Vector3(0.0f);
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_AIAgentComponent>()) return;
		Zenith_Blackboard& xBB =
			xEnt.GetComponent<Zenith_AIAgentComponent>().GetBlackboard();
		bHasInvOut = xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS);
		xInvOut = xBB.GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS);
	}
}

static void Setup_P2ForgePriestHears()
{
	g_iPhase = kFP_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xPriest = INVALID_ENTITY_ID;
	g_xForge = INVALID_ENTITY_ID;
	g_xInput = INVALID_ENTITY_ID;
	g_pxForge = nullptr;
	g_xForgePos = Zenith_Maths::Vector3(0.0f);
	g_iTickCounter = 0;
	g_bHasInvestigatePos = false;
	g_xInvestigatePos = Zenith_Maths::Vector3(0.0f);
}

static bool Step_P2ForgePriestHears(int iFrame)
{
	switch (g_iPhase)
	{
	case kFP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFP_WaitScene;
		return true;

	case kFP_WaitScene:
	{
		Zenith_EntityID xFoundV, xFoundP;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundV.IsValid()) xFoundV = xId;
			});
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundP](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFoundP = xId; });
		if (xFoundV.IsValid() && xFoundP.IsValid())
		{
			g_xVillager = xFoundV;
			g_xPriest = xFoundP;
			g_iPhase = kFP_BuildForge;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFP_Done;
		}
		return true;
	}

	case kFP_BuildForge:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFP_Done; return false; }
		Zenith_Entity xForge(pxScene, std::string("Test_PriestHearsForge"));
		if (!xForge.IsValid()) { g_iPhase = kFP_Done; return false; }
		g_xForge = xForge.GetEntityID();
		g_xForgePos = Zenith_Maths::Vector3(400.0f, 0.0f, 400.0f);
		if (xForge.HasComponent<Zenith_TransformComponent>())
		{
			xForge.GetComponent<Zenith_TransformComponent>().SetPosition(g_xForgePos);
		}
		g_pxForge = xForge.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPForge_Behaviour>();
		// Default recipe Iron -> Key.
		g_iPhase = kFP_TeleportPriest;
		return true;
	}

	case kFP_TeleportPriest:
	{
		// Within 30m hearing range. The forge emit clamps to the
		// priest's m_fMaxRange, so distance must be <= 30m.
		Zenith_Maths::Vector3 xPriestPos(
			g_xForgePos.x + kPRIEST_DIST, g_xForgePos.y, g_xForgePos.z);
		TeleportTo(g_xPriest, xPriestPos);
		g_iPhase = kFP_BuildInput;
		return true;
	}

	case kFP_BuildInput:
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kFP_Done; return false; }
		Zenith_Entity xInput(pxScene, std::string("ForgePriestHears_Iron"));
		if (!xInput.IsValid()) { g_iPhase = kFP_Done; return false; }
		g_xInput = xInput.GetEntityID();
		xInput.AddComponent<Zenith_ModelComponent>().LoadModel(
			std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" ZENITH_MODEL_EXT);
		DPItemBase_Behaviour* pxIB = xInput.AddComponent<Zenith_ScriptComponent>()
			.AddScript<DPItemBase_Behaviour>();
		if (pxIB != nullptr) pxIB->SetTag(DP_ItemTag::Iron);
		g_iPhase = kFP_HandToVillager;
		return true;
	}

	case kFP_HandToVillager:
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::SetHeldItem(g_xVillager, g_xInput);
		g_iPhase = kFP_Craft;
		return true;

	case kFP_Craft:
		if (g_pxForge != nullptr) g_pxForge->CraftForTest(g_xVillager);
		g_iTickCounter = 0;
		g_iPhase = kFP_TickForBridge;
		return true;

	case kFP_TickForBridge:
		++g_iTickCounter;
		if (g_iTickCounter >= kBRIDGE_TICKS) g_iPhase = kFP_Snapshot;
		return true;

	case kFP_Snapshot:
		ReadPriestBB(g_xPriest, g_bHasInvestigatePos, g_xInvestigatePos);
		g_iPhase = kFP_Verify;
		return true;

	case kFP_Verify:
		std::printf("[P2ForgePriestHears] hasInv=%d invPos=(%.2f,%.2f,%.2f) forgePos=(%.2f,%.2f,%.2f)\n",
			(int)g_bHasInvestigatePos,
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xForgePos.x, g_xForgePos.y, g_xForgePos.z);
		std::fflush(stdout);
		g_iPhase = kFP_Done;
		return false;

	case kFP_Done:
	default:
		return false;
	}
}

static bool Verify_P2ForgePriestHears()
{
	if (!g_xVillager.IsValid() || !g_xPriest.IsValid() || !g_xForge.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ForgePriestHears: setup missing");
		return false;
	}
	if (!g_bHasInvestigatePos)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgePriestHears: priest BB.HasInvestigatePos false after a forge craft 20m away. The forge needs to call Zenith_PerceptionSystem::EmitSoundStimulus alongside the existing AudioBus emit -- the AudioBus is test instrumentation only, NOT the priest-hearing path");
		return false;
	}
	const float fDx = g_xInvestigatePos.x - g_xForgePos.x;
	const float fDz = g_xInvestigatePos.z - g_xForgePos.z;
	if (fDx * fDx + fDz * fDz > 25.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ForgePriestHears: priest investigate pos (%.2f,%.2f,%.2f) too far from forge (%.2f,%.2f,%.2f)",
			g_xInvestigatePos.x, g_xInvestigatePos.y, g_xInvestigatePos.z,
			g_xForgePos.x, g_xForgePos.y, g_xForgePos.z);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ForgePriestHearsTest = {
	"Test_P2Forge_PriestHearsTheHammer",
	&Setup_P2ForgePriestHears,
	&Step_P2ForgePriestHears,
	&Verify_P2ForgePriestHears,
	180
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ForgePriestHearsTest);

#endif // ZENITH_INPUT_SIMULATOR
