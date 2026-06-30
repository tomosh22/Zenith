#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Component.h"
#include "Components/DPVillager_Component.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>

// ============================================================================
// Test_P1Apprehend_SwitchBreaksChannel (MVP-1.3.3)
//
// The Apprehend BT branch channels for priest.apprehend_channel_s seconds
// before dispatching DP_OnRunLost. The contract is that the channel
// MUST break if the player switches to a different villager mid-channel
// (Tuning.json: priest.apprehend_interruptible_by_switch = true).
//
// Procedure:
//   1. Load GameLevel + subscribe to DP_OnRunLost.
//   2. Pick the priest + the CLOSEST villager (villager A; the one
//      Test_P1Apprehend_PriestCatchesPlayer uses).
//   3. Pick a SECOND villager far from the priest (villager B). 67m+ in
//      practice -- the GameLevel-data Pleb12 placement at (5.8, _, 21.6).
//      Far enough that the priest can't pursue + re-channel within the
//      remaining frame budget.
//   4. Possess A, teleport the priest to 1.5m east of A (inside
//      apprehend_range = 2m). Start the channel.
//   5. Run ~50 frames (~0.8s into the 3s channel).
//   6. Switch possession to villager B.
//   7. Run another ~200 frames (~3.3s -- enough that the ORIGINAL
//      channel would have completed had the switch not broken it).
//   8. Assert DP_OnRunLost did NOT fire during this window. (Eventually
//      the priest will reach B and re-channel, dispatching the event --
//      but the 200-frame budget is well short of priest-traverse-67m
//      + 3s-channel, so no spurious dispatch is expected.)
//
// What this proves:
//   * Mid-channel target switch DOES interrupt the channel.
//   * Apprehend doesn't "remember" the old target and complete the
//     channel anyway after a switch (would have been a bug given how
//     m_xChannelTarget is locked in OnEnter and never refreshed inside
//     a single channel window).
// ============================================================================

namespace
{
	enum Phase : int { kSB_Start, kSB_WaitScene, kSB_Possess, kSB_TeleportPriest,
	                   kSB_RunInitialChannel, kSB_SwitchTarget, kSB_RunPostSwitch,
	                   kSB_Verify, kSB_Done };

	int                     g_iPhase = kSB_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillagerA;
	Zenith_EntityID         g_xVillagerB;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	bool                    g_bRunLostFired = false;
	int                     g_iRunFrames = 0;

	constexpr int kFRAMES_BEFORE_SWITCH = 50;   // ~0.83s @ 60Hz
	constexpr int kFRAMES_AFTER_SWITCH  = 200;  // ~3.33s @ 60Hz

	void OnRunLost(const DP_OnRunLost& /*xEvt*/)
	{
		g_bRunLostFired = true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	bool TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->SetPosition(xPos);
		return true;
	}
}

static void Setup_P1ApprehendSwitch()
{
	g_iPhase = kSB_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillagerA = INVALID_ENTITY_ID;
	g_xVillagerB = INVALID_ENTITY_ID;
	g_bRunLostFired = false;
	g_iRunFrames = 0;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1ApprehendSwitch(int iFrame)
{
	switch (g_iPhase)
	{
	case kSB_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSB_WaitScene;
		return true;

	case kSB_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xClosest;
		Zenith_EntityID xFarthest;
		float fClosestDist = 1e30f;
		float fFarthestDist = 0.0f;

		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos]
			(Zenith_EntityID xId, Priest_Component&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});

		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
				[&xClosest, &xFarthest, &fClosestDist, &fFarthestDist, &xPriestPos]
				(Zenith_EntityID xId, DPVillager_Component&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					const float fD = HorizontalDistance(xPriestPos, xVPos);
					if (fD < fClosestDist) { fClosestDist = fD; xClosest = xId; }
					if (fD > fFarthestDist) { fFarthestDist = fD; xFarthest = xId; }
				});
		}

		if (xFoundPriest.IsValid() && xClosest.IsValid() && xFarthest.IsValid()
			&& xClosest.m_uIndex != xFarthest.m_uIndex)
		{
			g_xPriest    = xFoundPriest;
			g_xVillagerA = xClosest;
			g_xVillagerB = xFarthest;
			g_iPhase     = kSB_Possess;
			Zenith_Log(LOG_CATEGORY_AI,
				"P1ApprehendSwitch: priest=(%.1f,%.1f,%.1f) closest@%.1fm farthest@%.1fm",
				xPriestPos.x, xPriestPos.y, xPriestPos.z,
				fClosestDist, fFarthestDist);
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSB_Done;
		}
		return true;
	}

	case kSB_Possess:
		// MVP-1.9 cleanup: register both villagers so the priest's
		// sight pass considers either as a target. The test then
		// teleports priest within range of villager A; after the
		// switch to villager B the priest is far from B (~67 m, well
		// out of sight) so perception drops the target -- the same
		// invariant the test asserts via the channel-broken check.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillagerA, /*hostile=*/true);
		Zenith_PerceptionSystem::RegisterTarget(g_xVillagerB, /*hostile=*/true);
		DP_Player::SetPossessedVillager(g_xVillagerA);
		g_iPhase = kSB_TeleportPriest;
		return true;

	case kSB_TeleportPriest:
	{
		Zenith_Maths::Vector3 xVPos;
		if (!TryGetEntityPos(g_xVillagerA, xVPos)) { g_iPhase = kSB_Done; return false; }
		TrySetEntityPos(g_xPriest,
			Zenith_Maths::Vector3(xVPos.x + 1.5f, xVPos.y, xVPos.z));
		g_iRunFrames = 0;
		g_iPhase = kSB_RunInitialChannel;
		return true;
	}

	case kSB_RunInitialChannel:
		++g_iRunFrames;
		if (g_bRunLostFired)
		{
			// Channel completed BEFORE we got to switch -- the channel
			// duration is shorter than the test assumed, or the priest
			// is way closer than expected. Either way it's a test
			// configuration bug, not the system-under-test failing.
			Zenith_Log(LOG_CATEGORY_AI,
				"P1ApprehendSwitch: DP_OnRunLost fired during initial channel (frame=%d) -- switch wasn't tested",
				g_iRunFrames);
			g_iPhase = kSB_Verify;
			return true;
		}
		if (g_iRunFrames >= kFRAMES_BEFORE_SWITCH)
		{
			g_iPhase = kSB_SwitchTarget;
		}
		return true;

	case kSB_SwitchTarget:
		// Switch to the far villager. The previous channel should now
		// terminate (Apprehend's target-mismatch check returns FAILURE);
		// the Selector falls to Pursue and the priest starts walking
		// toward villager_B. Whether the priest can re-engage Apprehend
		// before kFRAMES_AFTER_SWITCH elapses depends on the priest's
		// pursue speed (~5 m/s) and the priest-to-B distance (~67m for
		// the FAR pick). At 5 m/s the priest covers ~17m in 3.3s --
		// nowhere near 67m -- so no DP_OnRunLost should fire in this
		// window.
		DP_Player::SetPossessedVillager(g_xVillagerB);
		g_iRunFrames = 0;
		g_iPhase = kSB_RunPostSwitch;
		return true;

	case kSB_RunPostSwitch:
		++g_iRunFrames;
		if (g_iRunFrames >= kFRAMES_AFTER_SWITCH)
		{
			g_iPhase = kSB_Verify;
		}
		return true;

	case kSB_Verify:
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendSwitch: runLost=%d (expect 0 -- channel broken)",
			(int)g_bRunLostFired);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_iPhase = kSB_Done;
		return false;

	case kSB_Done:
	default:
		return false;
	}
}

static bool Verify_P1ApprehendSwitch()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ApprehendSwitch: priest not found");
		return false;
	}
	if (!g_xVillagerA.IsValid() || !g_xVillagerB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ApprehendSwitch: villager A or B not found");
		return false;
	}
	if (g_bRunLostFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ApprehendSwitch: DP_OnRunLost fired -- channel was NOT broken by switch");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ApprehendSwitchTest = {
	"Test_P1Apprehend_SwitchBreaksChannel",
	&Setup_P1ApprehendSwitch,
	&Step_P1ApprehendSwitch,
	&Verify_P1ApprehendSwitch,
	500
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ApprehendSwitchTest);

#endif // ZENITH_INPUT_SIMULATOR
