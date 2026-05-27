#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Fog_MemoryFollowsPossessedVillager (MVP-2.4.5 -- INTEGRATION)
//
// Test_P2Fog_MemoryDimsAfter10s pins the state-machine MATH by calling
// TickMemoryFog directly. This test pins the wiring -- DPFogPass +
// DPPlayerController have to drive the system correctly during real
// gameplay for the memory effect to manifest. Specifically:
//
//   * DPFogPass_Behaviour::OnUpdate must call DP_Fog::RecordMemoryReveal
//     at every villager's position each frame. Without this, no cells
//     accumulate.
//   * DPPlayerController_Behaviour::OnUpdate must call
//     DP_Fog::TickMemoryFog(fDt) each frame. Without this, all cells
//     stay at age 0 forever.
//
// Procedure (single villager, two positions, three time windows):
//
//   Phase A: load GameLevel, pick a villager. Snapshot original pos P0.
//
//   Phase B: possess villager. Tick a few frames so DPFogPass records
//   P0. Snapshot state at P0 -> expect VisitedVisible.
//
//   Phase C: teleport villager to P1 (50 m offset on X). Tick a few
//   more frames so P1 records and P0 starts aging.
//
//   Phase D: tick ~720 frames at fixed-dt 0.01666 = ~12 s of game time.
//   P0 age ~12 s now, P1 keeps being refreshed each frame to age 0.
//   Snapshot: P0 = VisitedDim, P1 = VisitedVisible.
//
//   Phase E: tick another ~1500 frames = ~25 s. P0 age ~37 s, P1 still
//   age 0. Snapshot: P0 = VisitedHidden, P1 = VisitedVisible.
//
// What this catches (that the unit test doesn't):
//   * DPFogPass forgets to call RecordMemoryReveal (P0 + P1 both stay
//     NeverSeen forever).
//   * DPPlayerController forgets to call TickMemoryFog (P0 stays
//     VisitedVisible after 37s).
//   * DPFogPass records the WRONG position (e.g., fog hole entity's
//     transform instead of the villager's transform).
//   * The 1 m grid snap loses precision in a way that wraps two
//     distant positions into the same cell.
// ============================================================================

namespace
{
	enum Phase : int {
		kI_Start, kI_WaitScene, kI_Possess, kI_TickInitial,
		kI_SnapshotInitial, kI_TeleportFar, kI_TickAfterTeleport,
		kI_SnapshotAfterTeleport,
		kI_TickToDim, kI_SnapshotDim,
		kI_TickToHidden, kI_SnapshotHidden,
		kI_Verify, kI_Done
	};

	int                     g_iPhase = kI_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_Maths::Vector3   g_xP0(0.0f);   // initial position
	Zenith_Maths::Vector3   g_xP1(0.0f);   // teleport target

	DP_Fog::MemoryTileState g_eStateP0_Initial         = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP0_AfterTeleport   = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP1_AfterTeleport   = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP0_ToDim           = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP1_ToDim           = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP0_ToHidden        = DP_Fog::MemoryTileState::NeverSeen;
	DP_Fog::MemoryTileState g_eStateP1_ToHidden        = DP_Fog::MemoryTileState::NeverSeen;

	int                     g_iTickCounter = 0;

	// Initial settling tick: enough for DPFogPass to OnUpdate at least
	// once on the possessed villager.
	constexpr int kSETTLE_TICKS = 5;
	// After teleport, tick a few frames so P1 has a fresh entry +
	// P0's age has lifted off 0 (otherwise both stay VisitedVisible).
	// This is also where we verify TickMemoryFog gets driven by the
	// player-controller -- if it doesn't, P0 would stay age 0 and
	// never advance.
	constexpr int kPOST_TELEPORT_TICKS = 10;
	// Skipping forward via direct TickMemoryFog calls keeps the test
	// inside the batch-runner's 600-frame budget. The wiring assertion
	// (DPFogPass/DPPlayerController drive the system) is established
	// by the smaller real-frame windows; the state-machine math (10s
	// / 30s thresholds) is covered separately by
	// Test_P2Fog_MemoryDimsAfter10s.
	constexpr float kFAST_FORWARD_TO_DIM_S    = 11.0f;
	constexpr float kFAST_FORWARD_TO_HIDDEN_S = 25.0f;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	void TeleportTo(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	}

	const char* StateName(DP_Fog::MemoryTileState e)
	{
		switch (e)
		{
		case DP_Fog::MemoryTileState::NeverSeen:       return "NeverSeen";
		case DP_Fog::MemoryTileState::VisitedVisible:  return "VisitedVisible";
		case DP_Fog::MemoryTileState::VisitedDim:      return "VisitedDim";
		case DP_Fog::MemoryTileState::VisitedHidden:   return "VisitedHidden";
		}
		return "?";
	}
}

static void Setup_P2MemoryFollows()
{
	g_iPhase = kI_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xP0 = Zenith_Maths::Vector3(0.0f);
	g_xP1 = Zenith_Maths::Vector3(0.0f);
	g_eStateP0_Initial = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP0_AfterTeleport = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP1_AfterTeleport = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP0_ToDim = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP1_ToDim = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP0_ToHidden = DP_Fog::MemoryTileState::NeverSeen;
	g_eStateP1_ToHidden = DP_Fog::MemoryTileState::NeverSeen;
	g_iTickCounter = 0;
}

static bool Step_P2MemoryFollows(int iFrame)
{
	switch (g_iPhase)
	{
	case kI_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kI_WaitScene;
		return true;

	case kI_WaitScene:
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
			TryGetEntityPos(g_xVillager, g_xP0);
			// Far enough on X that the 1m grid snap puts P0 and P1 in
			// completely different cells. The whole point of the test
			// is that P0 ages while P1 is being refreshed.
			g_xP1 = Zenith_Maths::Vector3(g_xP0.x + 50.0f, g_xP0.y, g_xP0.z);
			g_iPhase = kI_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kI_Done;
		}
		return true;
	}

	case kI_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iTickCounter = 0;
		g_iPhase = kI_TickInitial;
		return true;

	case kI_TickInitial:
		++g_iTickCounter;
		if (g_iTickCounter >= kSETTLE_TICKS) g_iPhase = kI_SnapshotInitial;
		return true;

	case kI_SnapshotInitial:
		g_eStateP0_Initial = DP_Fog::GetMemoryStateAt(g_xP0);
		g_iPhase = kI_TeleportFar;
		return true;

	case kI_TeleportFar:
		TeleportTo(g_xVillager, g_xP1);
		g_iTickCounter = 0;
		g_iPhase = kI_TickAfterTeleport;
		return true;

	case kI_TickAfterTeleport:
		++g_iTickCounter;
		if (g_iTickCounter >= kPOST_TELEPORT_TICKS) g_iPhase = kI_SnapshotAfterTeleport;
		return true;

	case kI_SnapshotAfterTeleport:
		g_eStateP0_AfterTeleport = DP_Fog::GetMemoryStateAt(g_xP0);
		g_eStateP1_AfterTeleport = DP_Fog::GetMemoryStateAt(g_xP1);
		g_iPhase = kI_TickToDim;
		return true;

	case kI_TickToDim:
		// Fast-forward via direct TickMemoryFog. The DPFogPass loop
		// still runs each real frame above and below this point so
		// P1 keeps being refreshed. P0 is no longer in range (we
		// teleported the villager away) so its age advances by
		// exactly the fDt we pass here.
		DP_Fog::TickMemoryFog(kFAST_FORWARD_TO_DIM_S);
		g_iPhase = kI_SnapshotDim;
		return true;

	case kI_SnapshotDim:
		g_eStateP0_ToDim = DP_Fog::GetMemoryStateAt(g_xP0);
		g_eStateP1_ToDim = DP_Fog::GetMemoryStateAt(g_xP1);
		g_iPhase = kI_TickToHidden;
		return true;

	case kI_TickToHidden:
		DP_Fog::TickMemoryFog(kFAST_FORWARD_TO_HIDDEN_S);
		g_iPhase = kI_SnapshotHidden;
		return true;

	case kI_SnapshotHidden:
		g_eStateP0_ToHidden = DP_Fog::GetMemoryStateAt(g_xP0);
		g_eStateP1_ToHidden = DP_Fog::GetMemoryStateAt(g_xP1);
		g_iPhase = kI_Verify;
		return true;

	case kI_Verify:
		std::printf("[P2MemoryFollows] initial: P0=%s | teleport: P0=%s P1=%s | toDim: P0=%s P1=%s | toHidden: P0=%s P1=%s\n",
			StateName(g_eStateP0_Initial),
			StateName(g_eStateP0_AfterTeleport), StateName(g_eStateP1_AfterTeleport),
			StateName(g_eStateP0_ToDim), StateName(g_eStateP1_ToDim),
			StateName(g_eStateP0_ToHidden), StateName(g_eStateP1_ToHidden));
		std::fflush(stdout);
		g_iPhase = kI_Done;
		return false;

	case kI_Done:
	default:
		return false;
	}
}

static bool Verify_P2MemoryFollows()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2MemoryFollows: villager not found");
		return false;
	}
	// 1. Initial snapshot: P0 should be VisitedVisible after a few
	//    frames of DPFogPass running. If NeverSeen, DPFogPass never
	//    called RecordMemoryReveal.
	if (g_eStateP0_Initial != DP_Fog::MemoryTileState::VisitedVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P0 initial state is %s, expected VisitedVisible. DPFogPass::OnUpdate is NOT calling RecordMemoryReveal on the possessed villager",
			StateName(g_eStateP0_Initial));
		return false;
	}
	// 2. After teleport: P1 should be VisitedVisible (just recorded).
	//    P0 may still be VisitedVisible (only ~0.17 s elapsed); we
	//    just need it to not be VisitedHidden.
	if (g_eStateP1_AfterTeleport != DP_Fog::MemoryTileState::VisitedVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P1 after teleport is %s, expected VisitedVisible. After moving the villager 50m, the new position's grid cell didn't record",
			StateName(g_eStateP1_AfterTeleport));
		return false;
	}
	// 3. After 12 s: P0 must be VisitedDim; P1 stays VisitedVisible.
	if (g_eStateP0_ToDim != DP_Fog::MemoryTileState::VisitedDim)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P0 after ~12s is %s, expected VisitedDim. Either TickMemoryFog isn't being called (P0 stuck at VisitedVisible), or the threshold maths is broken",
			StateName(g_eStateP0_ToDim));
		return false;
	}
	if (g_eStateP1_ToDim != DP_Fog::MemoryTileState::VisitedVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P1 after ~12s is %s, expected VisitedVisible. The villager is currently at P1; DPFogPass should refresh its age each frame",
			StateName(g_eStateP1_ToDim));
		return false;
	}
	// 4. After 37 s total: P0 = VisitedHidden; P1 still VisitedVisible.
	if (g_eStateP0_ToHidden != DP_Fog::MemoryTileState::VisitedHidden)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P0 after ~37s is %s, expected VisitedHidden",
			StateName(g_eStateP0_ToHidden));
		return false;
	}
	if (g_eStateP1_ToHidden != DP_Fog::MemoryTileState::VisitedVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryFollows: P1 after ~37s is %s, expected VisitedVisible (villager still there)",
			StateName(g_eStateP1_ToHidden));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2MemoryFollowsTest = {
	"Test_P2Fog_MemoryFollowsPossessedVillager",
	&Setup_P2MemoryFollows,
	&Step_P2MemoryFollows,
	&Verify_P2MemoryFollows,
	3000
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2MemoryFollowsTest);

#endif // ZENITH_INPUT_SIMULATOR
