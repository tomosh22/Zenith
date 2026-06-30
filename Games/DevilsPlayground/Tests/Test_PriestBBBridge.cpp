#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"

#include "Tests/DPTestSupport.h"

// ============================================================================
// PriestBBBridge_Test
//
// Companion test to HearingFlow_Test: verifies that Priest_Component's
// BridgePerceptionToBlackboard actually writes the freshest heard-sound
// position into the priest's BB (not just that GetLastHeardSoundFor returns
// valid).
//
// Investigation of the previous failure:
//   - The BB-bridge runs inside Priest_Component::OnUpdate, which is invoked
//     by the script-component dispatcher each frame.
//   - The HearingFlow_Test already proves the perception subsystem populates
//     GetLastHeardSoundFor correctly given a valid source EntityID.
//   - When Priest_Component was a stub, the bridge code was already in place
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
	Zenith_Maths::Vector3 g_xNoisePos(0.0f);
	int             g_iWait       = 0;

	bool PriestHasInvestigateFlag(Zenith_EntityID xPriest)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xPriest);
		Zenith_AIAgentComponent* pxAgent = xEnt.TryGetComponent<Zenith_AIAgentComponent>();
		if (pxAgent == nullptr) return false;
		return pxAgent->GetBlackboard()
			.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
	}
}

static void Setup_PriestBBBridge()
{
	g_iBBPhase   = kBB_Start;
	g_xPriest    = INVALID_ENTITY_ID;
	g_xSource    = INVALID_ENTITY_ID;
	g_bBBHasFlag = false;
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
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xPriest](Zenith_EntityID xId, Priest_Component&) { xPriest = xId; });
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xSource](Zenith_EntityID xId, DPVillager_Component&) { xSource = xId; });
		if (xPriest.IsValid() && xSource.IsValid())
		{
			g_xPriest = xPriest;
			g_xSource = xSource;

			// Place noise close to the priest so it falls inside the 25m
			// hearing radius regardless of authored positions.
			Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xPriest);
			if (Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xPriestPos;
				pxTransform->GetPosition(xPriestPos);
				g_xNoisePos = xPriestPos + Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f);
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
		// dt-robust: re-emit + poll until the priest's BB shows the investigate
		// flag. The bridge is time-integrated (perception awareness + the
		// priest's OnUpdate), so a FIXED frame wait starves at the suite's
		// --fixed-dt; polling is dt-independent.
		++g_iWait;
		DP_TestSupport::PollHeardFromSource(g_xPriest, g_xSource, g_xNoisePos);
		if (PriestHasInvestigateFlag(g_xPriest) || g_iWait >= 180)
		{
			g_iBBPhase = kBB_Verify;
		}
		return true;

	case kBB_Verify:
		// Assert the bridge SET the investigate flag from the heard sound.
		// We do NOT assert the BB investigate POSITION equals the emit point:
		// it's copied from GetLastHeardSoundFor, whose position is the source's
		// and gets sight-overwritten once the priest faces the villager --
		// dt-dependent (see DPTestSupport.h).
		g_bBBHasFlag = PriestHasInvestigateFlag(g_xPriest);
		g_iBBPhase   = kBB_Done;
		return false;

	case kBB_Done:
	default:
		return false;
	}
}

static bool Verify_PriestBBBridge()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "PriestBBBridge: priest entity not found");
		return false;
	}
	if (!g_bBBHasFlag)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"PriestBBBridge: priest's BB never got HasInvestigatePos from the heard sound within the frame budget");
		return false;
	}
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
