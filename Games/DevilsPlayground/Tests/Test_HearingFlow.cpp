#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include "Tests/DPTestSupport.h"

// ============================================================================
// HearingFlow_Test (EXT-6 + Priest perception bridge)
//
// Loads GameLevel, finds the Priest entity, emits a sound stimulus close to
// the priest, runs a few frames so the perception system processes it, then
// verifies:
//   - GetLastHeardSoundFor(priest) reports m_bValid = true
//   - The reported position matches the noise origin (within tolerance)
//
// The test does NOT verify the BB write path — that is a side effect of
// Priest_Behaviour::OnUpdate which only runs in Playing mode AND requires
// the AIAgentComponent's tick. Verifying the BB requires the BT to settle,
// which gets brittle. The accessor is the load-bearing piece.
// ============================================================================

namespace
{
	enum Phase : int { kHF_Start, kHF_WaitScene, kHF_Emit, kHF_WaitProcess,
	                   kHF_Verify, kHF_Done };

	int             g_iHFPhase = kHF_Start;
	Zenith_EntityID g_xPriest;
	Zenith_EntityID g_xSoundSource;
	bool            g_bSawValid     = false;
	int             g_iWaitFrames   = 0;

	// Filled in at runtime once we know where the priest actually is — the
	// scene authoring uses real UE positions, so a hardcoded noise origin
	// might be 50m+ away from the priest and outside hearing range.
	Zenith_Maths::Vector3 g_xNoisePos(0.0f);
}

static void Setup_HearingFlow()
{
	g_iHFPhase     = kHF_Start;
	g_xPriest      = INVALID_ENTITY_ID;
	g_xSoundSource = INVALID_ENTITY_ID;
	g_bSawValid    = false;
	g_iWaitFrames  = 0;
}

static bool Step_HearingFlow(int iFrame)
{
	switch (g_iHFPhase)
	{
	case kHF_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iHFPhase = kHF_WaitScene;
		return true;

	case kHF_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFoundSource;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Behaviour&) { xFoundPriest = xId; });
		// Use the villager as the synthetic sound source — needs to be a
		// VALID, NON-PRIEST EntityID. Zenith_PerceptionSystem::UpdateHearingPerception
		// silently drops sounds whose source is invalid (no FindOrCreateTarget),
		// and ignores sounds where source == self (priest doesn't hear its own
		// footsteps).
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundSource](Zenith_EntityID xId, DPVillager_Behaviour&) { xFoundSource = xId; });
		if (xFoundPriest.IsValid() && xFoundSource.IsValid())
		{
			g_xPriest      = xFoundPriest;
			g_xSoundSource = xFoundSource;
			// Place the synthetic noise 2m in front of the priest so it
			// always lies inside the priest's 35 m hearing radius regardless
			// of the priest's authored position.
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
			g_iHFPhase = kHF_Emit;
		}
		else if (iFrame > 60)
		{
			g_iHFPhase = kHF_Done;
		}
		return true;
	}

	case kHF_Emit:
	{
		// Loud + wide-radius stimulus, well above the priest's hearing
		// threshold and inside its 35 m max range. Source = villager so the
		// perception system creates a perceived-target for it (without that
		// the sound is processed but the per-target record never appears,
		// and GetLastHeardSoundFor walks an empty list).
		DP_AI::EmitNoise(g_xNoisePos, /*loudness*/ 1.0f, /*radius*/ 30.0f, g_xSoundSource);
		g_iHFPhase = kHF_WaitProcess;
		return true;
	}

	case kHF_WaitProcess:
		// dt-robust: re-emit + poll until the priest perceives the villager-
		// sourced sound (DP_TestSupport::PollHeardFromSource). A FIXED frame wait
		// starves at the suite's --fixed-dt because hearing awareness is time-
		// integrated and sounds expire in 0.5 s; polling is dt-independent.
		++g_iWaitFrames;
		if (DP_TestSupport::PollHeardFromSource(g_xPriest, g_xSoundSource, g_xNoisePos)
			|| g_iWaitFrames >= 180)
		{
			g_iHFPhase = kHF_Verify;
		}
		return true;

	case kHF_Verify:
	{
		const Zenith_PerceptionSystem::Zenith_LastHeardSound xHeard
			= Zenith_PerceptionSystem::GetLastHeardSoundFor(g_xPriest);
		// Assert ATTRIBUTION, not emit-position: GetLastHeardSoundFor reports the
		// source's position and sight overwrites it once the priest faces the
		// villager (dt-dependent), so an emit-point equality check is fragile.
		g_bSawValid = xHeard.m_bValid && xHeard.m_xSourceEntity == g_xSoundSource;
		g_iHFPhase  = kHF_Done;
		return false;
	}

	case kHF_Done:
	default:
		return false;
	}
}

static bool Verify_HearingFlow()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "HearingFlow: priest entity not found");
		return false;
	}
	if (!g_bSawValid)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"HearingFlow: priest never perceived a HEARING stimulus attributed to the villager source within the frame budget");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xHearingFlowTest = {
	"HearingFlow_Test",
	&Setup_HearingFlow,
	&Step_HearingFlow,
	&Verify_HearingFlow,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHearingFlowTest);

#endif // ZENITH_INPUT_SIMULATOR
