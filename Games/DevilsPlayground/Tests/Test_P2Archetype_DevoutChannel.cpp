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
// Test_P2Archetype_DevoutChannel (MVP-2.1.1)
//
// Pins the Devout archetype's possession-channel mechanic:
// TryVoluntaryPossessSwitch onto a Devout starts a 0.8 s channel
// (per Tuning.json `possession.channel_devout_s`). During the channel:
//   * Possession stays on the SOURCE villager.
//   * IsChanneling() returns true; GetChannelTarget() returns the
//     Devout's handle.
//   * GetChannelRemaining() decrements toward 0.
// When the timer reaches 0, the possession COMMITS onto the Devout.
//
// The mechanic interacts with the priest pursuit loop: the demon is
// vulnerable while channeling (still locked to the SOURCE body, which
// the priest may still be pursuing). MVP-2.1.2 covers the priest-
// interrupt half; this test just confirms the channel COMPLETES when
// no priest is close enough to interrupt.
//
// Procedure:
//   1. Load GameLevel. Find priest.
//   2. Pick 2 villagers in mutual range (A and B).
//   3. ApplyArchetype("Devout") on B.
//   4. Teleport the priest FAR from B so no interrupt can fire.
//   5. SetPossessedVillager(A); wait one frame.
//   6. TryVoluntaryPossessSwitch(B). Returns true (channel started).
//   7. Snapshot mid-channel state:
//        IsChanneling() == true
//        GetChannelTarget() == B
//        GetPossessedVillager() == A   (not committed yet)
//   8. Tick enough frames to drain channel_devout_s + slack.
//   9. Snapshot post-channel state:
//        IsChanneling() == false
//        GetPossessedVillager() == B   (committed)
//
// What this catches:
//   * Devout archetype is treated as immediate (channel duration not
//     read; non-Devout default of 0 applied).
//   * Channel commit forgot to update possessed handle (channel
//     completes but player still possesses source).
//   * Channel commit forgot to bump scent / anchor / cooldown (these
//     would surface in downstream tests, but the commit path is the
//     same `CommitVoluntaryPossession` helper so we get it for free).
// ============================================================================

namespace
{
	enum Phase : int { kDC_Start, kDC_WaitScene, kDC_TeleportPriest,
	                   kDC_ApplyArchetype, kDC_PossessA, kDC_WaitForA,
	                   kDC_StartChannel, kDC_MidSnapshot, kDC_TickChannel,
	                   kDC_PostSnapshot, kDC_Verify, kDC_Done };

	int                     g_iPhase = kDC_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	bool                    g_bChannelingMid = false;
	Zenith_EntityID         g_xTargetMid;
	Zenith_EntityID         g_xPossessedMid;
	bool                    g_bChannelingPost = true;
	Zenith_EntityID         g_xPossessedPost;
	int                     g_iTickCounter = 0;
	bool                    g_bSwitchReturnedTrue = false;

	// Channel duration is 0.8s; tick 80 frames at fixed-dt 0.01666 =
	// ~1.33s. Comfortable overshoot.
	constexpr int kTICK_FRAMES = 80;
	// Far enough from the Devout to avoid the 6m interrupt distance.
	constexpr float kPRIEST_AWAY_X = 200.0f;

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

static void Setup_P2DevoutChannel()
{
	g_iPhase = kDC_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_bChannelingMid = false;
	g_xTargetMid = INVALID_ENTITY_ID;
	g_xPossessedMid = INVALID_ENTITY_ID;
	g_bChannelingPost = true;
	g_xPossessedPost = INVALID_ENTITY_ID;
	g_iTickCounter = 0;
	g_bSwitchReturnedTrue = false;
}

static bool Step_P2DevoutChannel(int iFrame)
{
	switch (g_iPhase)
	{
	case kDC_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kDC_WaitScene;
		return true;

	case kDC_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFoundPriest = xId; });
		PickClosestPair(g_xA, g_xB);
		if (xFoundPriest.IsValid() && g_xA.IsValid() && g_xB.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_iPhase = kDC_TeleportPriest;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kDC_Done;
		}
		return true;
	}

	case kDC_TeleportPriest:
	{
		// Teleport priest FAR from B so the interrupt check in
		// TickChannel doesn't fire during this positive-case test.
		Zenith_Maths::Vector3 xBPos;
		if (TryGetEntityPos(g_xB, xBPos))
		{
			Zenith_Maths::Vector3 xFar(xBPos.x + kPRIEST_AWAY_X, xBPos.y, xBPos.z);
			TeleportTo(g_xPriest, xFar);
		}
		g_iPhase = kDC_ApplyArchetype;
		return true;
	}

	case kDC_ApplyArchetype:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xB);
		if (pxV != nullptr) pxV->ApplyArchetype("Devout");
		g_iPhase = kDC_PossessA;
		return true;
	}

	case kDC_PossessA:
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kDC_WaitForA;
		return true;

	case kDC_WaitForA:
		g_iPhase = kDC_StartChannel;
		return true;

	case kDC_StartChannel:
		g_bSwitchReturnedTrue = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kDC_MidSnapshot;
		return true;

	case kDC_MidSnapshot:
		// Snapshot WITHOUT ticking -- we're on the same frame as the
		// switch call. The channel state should be active and the
		// possessed handle should still be A.
		g_bChannelingMid = DP_Player::IsChanneling();
		g_xTargetMid = DP_Player::GetChannelTarget();
		g_xPossessedMid = DP_Player::GetPossessedVillager();
		g_iTickCounter = 0;
		g_iPhase = kDC_TickChannel;
		return true;

	case kDC_TickChannel:
		++g_iTickCounter;
		if (g_iTickCounter >= kTICK_FRAMES) g_iPhase = kDC_PostSnapshot;
		return true;

	case kDC_PostSnapshot:
		g_bChannelingPost = DP_Player::IsChanneling();
		g_xPossessedPost = DP_Player::GetPossessedVillager();
		g_iPhase = kDC_Verify;
		return true;

	case kDC_Verify:
		std::printf("[P2DevoutChannel] switch=%d mid:chan=%d target=(%u/%u) poss=(%u/%u) post:chan=%d poss=(%u/%u) (expected post=B=(%u/%u))\n",
			(int)g_bSwitchReturnedTrue,
			(int)g_bChannelingMid,
			g_xTargetMid.m_uIndex, g_xTargetMid.m_uGeneration,
			g_xPossessedMid.m_uIndex, g_xPossessedMid.m_uGeneration,
			(int)g_bChannelingPost,
			g_xPossessedPost.m_uIndex, g_xPossessedPost.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		std::fflush(stdout);
		g_iPhase = kDC_Done;
		return false;

	case kDC_Done:
	default:
		return false;
	}
}

static bool Verify_P2DevoutChannel()
{
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2DevoutChannel: precondition (priest/villagers) failed");
		return false;
	}
	if (!g_bSwitchReturnedTrue)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: TryVoluntaryPossessSwitch returned false -- gates rejected the Devout target. Check archetype lookup + cooldown");
		return false;
	}
	// Mid-channel assertions.
	if (!g_bChannelingMid)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: IsChanneling() returned false immediately after switch -- channel didn't start. Archetype lookup is failing, or channel_devout_s is 0");
		return false;
	}
	if (g_xTargetMid.m_uIndex != g_xB.m_uIndex
		|| g_xTargetMid.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: GetChannelTarget=%u/%u but expected B=%u/%u",
			g_xTargetMid.m_uIndex, g_xTargetMid.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		return false;
	}
	if (g_xPossessedMid.m_uIndex != g_xA.m_uIndex
		|| g_xPossessedMid.m_uGeneration != g_xA.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: GetPossessedVillager during channel is %u/%u, expected A=%u/%u -- channel committed prematurely",
			g_xPossessedMid.m_uIndex, g_xPossessedMid.m_uGeneration,
			g_xA.m_uIndex, g_xA.m_uGeneration);
		return false;
	}
	// Post-channel assertions.
	if (g_bChannelingPost)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: IsChanneling() still true after %d ticks (~%.2fs at fixed-dt) -- TickChannel didn't decrement, or threshold is wrong",
			kTICK_FRAMES, kTICK_FRAMES * 0.01666f);
		return false;
	}
	if (g_xPossessedPost.m_uIndex != g_xB.m_uIndex
		|| g_xPossessedPost.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2DevoutChannel: GetPossessedVillager after channel is %u/%u, expected B=%u/%u -- channel timer expired but commit didn't run (or priest interrupted unexpectedly)",
			g_xPossessedPost.m_uIndex, g_xPossessedPost.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2DevoutChannelTest = {
	"Test_P2Archetype_DevoutChannel",
	&Setup_P2DevoutChannel,
	&Step_P2DevoutChannel,
	&Verify_P2DevoutChannel,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2DevoutChannelTest);

#endif // ZENITH_INPUT_SIMULATOR
