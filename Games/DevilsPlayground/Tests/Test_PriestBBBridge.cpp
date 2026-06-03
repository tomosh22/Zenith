#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// PriestBBBridge_Test
//
// Companion test to HearingFlow_Test: verifies that Priest_Behaviour's
// BridgePerceptionToBlackboard actually writes the freshest heard-sound
// position into the priest's BB (not just that GetLastHeardSoundFor returns
// valid).
//
// Investigation of the previous failure:
//   - The BB-bridge runs inside Priest_Behaviour::OnUpdate, which is invoked
//     by the script-component dispatcher each frame.
//   - The HearingFlow_Test already proves the perception subsystem populates
//     GetLastHeardSoundFor correctly given a valid source EntityID.
//   - When Priest_Behaviour was a stub, the bridge code was already in place
//     but the test was reading the BB BEFORE OnUpdate had a chance to fire
//     after the perception update. This test waits an extra two frames after
//     emitting noise so the perception system processes the stimulus AND
//     the priest's OnUpdate writes the BB.
// ============================================================================

namespace
{
	enum Phase : int { kBB_Start, kBB_WaitScene, kBB_Emit, kBB_WaitPropagate,
	                   kBB_Verify, kBB_Done };

	int             g_iBBPhase    = kBB_Start;
	Zenith_EntityID g_xPriest;
	Zenith_EntityID g_xSource;
	bool            g_bBBHasFlag  = false;
	Zenith_Maths::Vector3 g_xBBPos(0.0f);
	Zenith_Maths::Vector3 g_xNoisePos(0.0f);
	int             g_iWait       = 0;

	Priest_Behaviour* FindPriestScript(Zenith_EntityID xPriest)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xPriest);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriest);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<Priest_Behaviour>();
	}
}

static void Setup_PriestBBBridge()
{
	g_iBBPhase   = kBB_Start;
	g_xPriest    = INVALID_ENTITY_ID;
	g_xSource    = INVALID_ENTITY_ID;
	g_bBBHasFlag = false;
	g_xBBPos     = Zenith_Maths::Vector3(0.0f);
	g_xNoisePos  = Zenith_Maths::Vector3(0.0f);
	g_iWait      = 0;
}

static bool Step_PriestBBBridge(int iFrame)
{
	switch (g_iBBPhase)
	{
	case kBB_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iBBPhase = kBB_WaitScene;
		return true;

	case kBB_WaitScene:
	{
		Zenith_EntityID xPriest;
		Zenith_EntityID xSource;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xPriest](Zenith_EntityID xId, Priest_Behaviour&) { xPriest = xId; });
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xSource](Zenith_EntityID xId, DPVillager_Behaviour&) { xSource = xId; });
		if (xPriest.IsValid() && xSource.IsValid())
		{
			g_xPriest = xPriest;
			g_xSource = xSource;

			// Place noise close to the priest so it falls inside the 25m
			// hearing radius regardless of authored positions.
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xPriest);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(g_xPriest);
				if (xEnt.IsValid() && xEnt.HasComponent<Zenith_TransformComponent>())
				{
					Zenith_Maths::Vector3 xPriestPos;
					xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPriestPos);
					g_xNoisePos = xPriestPos + Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f);
				}
			}
			g_iBBPhase = kBB_Emit;
		}
		else if (iFrame > 60)
		{
			g_iBBPhase = kBB_Done;
		}
		return true;
	}

	case kBB_Emit:
		// Emit a loud sound 2m from the priest, with the villager as source
		// (perception::UpdateHearingPerception drops sounds whose source is
		// the agent itself, and silently ignores sounds whose source is
		// invalid because FindOrCreateTarget needs a real EntityID).
		DP_AI::EmitNoise(g_xNoisePos, /*loudness*/ 1.0f, /*radius*/ 30.0f, g_xSource);
		g_iBBPhase = kBB_WaitPropagate;
		return true;

	case kBB_WaitPropagate:
		// Two phases need to fire after emission:
		//   1. PerceptionSystem::Update processes the stimulus (T+1 frame).
		//   2. Priest_Behaviour::OnUpdate runs BridgePerceptionToBlackboard
		//      and writes BB.HasInvestigatePos (T+2 frames; OnUpdate may run
		//      before perception in the dispatch order).
		// Five frames is overkill — gives wiggle room for editor-deferred
		// scene-load callbacks that occasionally push real work back a tick.
		++g_iWait;
		if (g_iWait >= 5)
		{
			g_iBBPhase = kBB_Verify;
		}
		return true;

	case kBB_Verify:
	{
		Priest_Behaviour* pxPriest = FindPriestScript(g_xPriest);
		if (pxPriest == nullptr)
		{
			g_iBBPhase = kBB_Done;
			return false;
		}

		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xPriest);
		if (pxScene == nullptr)
		{
			g_iBBPhase = kBB_Done;
			return false;
		}
		Zenith_Entity xEnt = pxScene->TryGetEntity(g_xPriest);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_AIAgentComponent>())
		{
			g_iBBPhase = kBB_Done;
			return false;
		}

		const Zenith_Blackboard& xBB =
			xEnt.GetComponent<Zenith_AIAgentComponent>().GetBlackboard();
		g_bBBHasFlag = xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		g_xBBPos     = xBB.GetVector3(DP_AI::BB_KEY_INVESTIGATE_POS);
		g_iBBPhase   = kBB_Done;
		return false;
	}

	case kBB_Done:
	default:
		return false;
	}
}

static bool Verify_PriestBBBridge()
{
	if (!g_xPriest.IsValid()) return false;
	if (!g_bBBHasFlag) return false;
	// BB position must be near the emitted noise (within 0.5m on XZ).
	const float fDx = g_xBBPos.x - g_xNoisePos.x;
	const float fDz = g_xBBPos.z - g_xNoisePos.z;
	if (fDx > 0.5f || fDx < -0.5f) return false;
	if (fDz > 0.5f || fDz < -0.5f) return false;
	return true;
}

static const Zenith_AutomatedTest g_xPriestBBBridgeTest = {
	"PriestBBBridge_Test",
	&Setup_PriestBBBridge,
	&Step_PriestBBBridge,
	&Verify_PriestBBBridge,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPriestBBBridgeTest);

#endif // ZENITH_INPUT_SIMULATOR
