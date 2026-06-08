#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P2Archetype_DevoutChannelInterrupt (MVP-2.1.2)
//
// Pins the priest-interrupt half of the Devout channel mechanic:
// while channeling onto a Devout, if a priest is within
// `possession.channel_interrupt_distance_m` (6 m default) of the
// channel TARGET, the channel is cancelled and possession stays on
// the source villager.
//
// Procedure:
//   1. Load GameLevel. Find priest + 2 villagers (A and B).
//   2. ApplyArchetype("Devout") on B.
//   3. Teleport priest FAR (200 m away) initially so the channel
//      starts cleanly.
//   4. SetPossessedVillager(A); wait one frame.
//   5. TryVoluntaryPossessSwitch(B). Channel starts.
//   6. Wait one frame so the channel is mid-flight.
//   7. Teleport priest to within 1 m of B (well inside the 6 m
//      interrupt distance).
//   8. Tick a few frames so TickChannel runs and detects the priest.
//   9. Assert:
//        IsChanneling() == false       (channel cancelled)
//        GetPossessedVillager() == A   (possession DIDN'T commit)
//        GetChannelTarget() == INVALID (state cleared)
//
// What this catches:
//   * TickChannel doesn't run the priest-distance check.
//   * Distance check uses the wrong tuning key (or hardcoded value).
//   * InterruptChannel clears state but leaves m_bChannelActive
//     true (state inconsistency).
//   * Interrupt accidentally commits possession (the wrong code
//     path on cancellation).
// ============================================================================

namespace
{
	enum Phase : int { kDI_Start, kDI_WaitScene, kDI_TeleportPriestFar,
	                   kDI_ApplyArchetype, kDI_PossessA, kDI_WaitForA,
	                   kDI_StartChannel, kDI_WaitOneFrame,
	                   kDI_TeleportPriestClose, kDI_TickForInterrupt,
	                   kDI_Snapshot, kDI_Verify, kDI_Done };

	int                     g_iPhase = kDI_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	bool                    g_bChannelingFinal = true;  // sentinel: must become false
	Zenith_EntityID         g_xPossessedFinal;
	Zenith_EntityID         g_xChannelTargetFinal;
	int                     g_iTickCounter = 0;

	// 6m default interrupt distance; teleport priest within 1m of B.
	constexpr float kPRIEST_CLOSE_X = 1.0f;
	constexpr float kPRIEST_FAR_X = 200.0f;
	constexpr int kTICK_FRAMES_FOR_INTERRUPT = 5;

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

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	void PickClosestPair(Zenith_EntityID& xA, Zenith_EntityID& xB)
	{
		struct VPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VPos> axVs;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVs](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				VPos xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axVs.PushBack(xV);
			});
		if (axVs.GetSize() < 2) return;
		float fMin = 1e30f;
		for (uint32_t i = 0; i < axVs.GetSize(); ++i)
		for (uint32_t j = i + 1; j < axVs.GetSize(); ++j)
		{
			const float fD = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(j).xPos);
			if (fD < fMin) { fMin = fD; xA = axVs.Get(i).xId; xB = axVs.Get(j).xId; }
		}
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

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P2DevoutInterrupt()
{
	g_iPhase = kDI_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_bChannelingFinal = true;
	g_xPossessedFinal = INVALID_ENTITY_ID;
	g_xChannelTargetFinal = INVALID_ENTITY_ID;
	g_iTickCounter = 0;
}

static bool Step_P2DevoutInterrupt(int iFrame)
{
	switch (g_iPhase)
	{
	case kDI_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kDI_WaitScene;
		return true;

	case kDI_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFoundPriest = xId; });
		PickClosestPair(g_xA, g_xB);
		if (xFoundPriest.IsValid() && g_xA.IsValid() && g_xB.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_iPhase = kDI_TeleportPriestFar;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kDI_Done;
		}
		return true;
	}

	case kDI_TeleportPriestFar:
	{
		// Far enough that the channel starts without immediate interrupt.
		Zenith_Maths::Vector3 xBPos;
		if (TryGetEntityPos(g_xB, xBPos))
		{
			Zenith_Maths::Vector3 xFar(xBPos.x + kPRIEST_FAR_X, xBPos.y, xBPos.z);
			TeleportTo(g_xPriest, xFar);
		}
		g_iPhase = kDI_ApplyArchetype;
		return true;
	}

	case kDI_ApplyArchetype:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xB);
		if (pxV != nullptr) pxV->ApplyArchetype("Devout");
		g_iPhase = kDI_PossessA;
		return true;
	}

	case kDI_PossessA:
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kDI_WaitForA;
		return true;

	case kDI_WaitForA:
		g_iPhase = kDI_StartChannel;
		return true;

	case kDI_StartChannel:
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kDI_WaitOneFrame;
		return true;

	case kDI_WaitOneFrame:
		// One frame of channel ticked but not completed (0.8s timer
		// is ~48 frames at fixed-dt). Verify mid-flight state is sane
		// by checking we're still channeling -- otherwise the
		// interrupt would be vacuous.
		if (!DP_Player::IsChanneling())
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P2DevoutInterrupt: channel ended before we could teleport priest in -- test scaffolding broken (priest already too close on first tick, or channel didn't start)");
			g_iPhase = kDI_Done;
			return false;
		}
		g_iPhase = kDI_TeleportPriestClose;
		return true;

	case kDI_TeleportPriestClose:
	{
		// Teleport priest to within 1m of B -- well inside the 6m
		// interrupt distance.
		Zenith_Maths::Vector3 xBPos;
		if (TryGetEntityPos(g_xB, xBPos))
		{
			Zenith_Maths::Vector3 xClose(xBPos.x + kPRIEST_CLOSE_X, xBPos.y, xBPos.z);
			TeleportTo(g_xPriest, xClose);
		}
		g_iTickCounter = 0;
		g_iPhase = kDI_TickForInterrupt;
		return true;
	}

	case kDI_TickForInterrupt:
		++g_iTickCounter;
		if (g_iTickCounter >= kTICK_FRAMES_FOR_INTERRUPT)
		{
			g_iPhase = kDI_Snapshot;
		}
		return true;

	case kDI_Snapshot:
		g_bChannelingFinal = DP_Player::IsChanneling();
		g_xPossessedFinal = DP_Player::GetPossessedVillager();
		g_xChannelTargetFinal = DP_Player::GetChannelTarget();
		g_iPhase = kDI_Verify;
		return true;

	case kDI_Verify:
		std::printf("[P2DevoutInterrupt] channeling=%d possessed=(%u/%u) channelTarget=(%u/%u) (expected: channeling=0 possessed=A=(%u/%u) target=INVALID)\n",
			(int)g_bChannelingFinal,
			g_xPossessedFinal.m_uIndex, g_xPossessedFinal.m_uGeneration,
			g_xChannelTargetFinal.m_uIndex, g_xChannelTargetFinal.m_uGeneration,
			g_xA.m_uIndex, g_xA.m_uGeneration);
		std::fflush(stdout);
		g_iPhase = kDI_Done;
		return false;

	case kDI_Done:
	default:
		return false;
	}
}

static bool Verify_P2DevoutInterrupt()
{
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2DevoutInterrupt: precondition failed");
		return false;
	}
	if (g_bChannelingFinal)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutInterrupt: IsChanneling() still true after priest got within 1m of channel target -- TickChannel's priest-distance check isn't firing");
		return false;
	}
	if (g_xPossessedFinal.m_uIndex != g_xA.m_uIndex
		|| g_xPossessedFinal.m_uGeneration != g_xA.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutInterrupt: GetPossessedVillager = %u/%u, expected A=%u/%u -- the channel either committed onto B before interrupt (bad order) or InterruptChannel mutated the possessed handle (bad implementation)",
			g_xPossessedFinal.m_uIndex, g_xPossessedFinal.m_uGeneration,
			g_xA.m_uIndex, g_xA.m_uGeneration);
		return false;
	}
	if (g_xChannelTargetFinal.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutInterrupt: GetChannelTarget returns (%u/%u) after interrupt, expected INVALID -- state not fully cleared",
			g_xChannelTargetFinal.m_uIndex, g_xChannelTargetFinal.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2DevoutInterruptTest = {
	"Test_P2Archetype_DevoutChannelInterrupt",
	&Setup_P2DevoutInterrupt,
	&Step_P2DevoutInterrupt,
	&Verify_P2DevoutInterrupt,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2DevoutInterruptTest);

#endif // ZENITH_INPUT_SIMULATOR
