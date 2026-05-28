#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P4Playthrough_Night1WinGolden (MVP-4.1.1)
//
// The canonical Night 1 win-side playthrough. Required by MVPScope §1.5
// definition-of-done: "Test_P4Playthrough_Night1WinGolden finishes in
// under 9000 frames."
//
// TestPlan §5.1 sketched this as an input-simulator-driven WASD + F-key
// playthrough; HumanPlaythrough_Test already provides that coverage as
// a graphics-required smoke. This headless variant pins the WIN-side
// game-state assertions (analogous to the 3 LOSS tests shipped in PR #75:
// Test_P4Playthrough_LossByApprehend, _LossByDawn, _LossByNoVessels).
// Both versions live together: the input-simulator path catches the
// pipeline (raycast -> click -> WASD -> F -> pentagram), the headless
// path catches the game-state contract (DP_OnVictory + DP_Win flags +
// HUD banner) without needing a window.
//
// Procedure:
//   1. Subscribe to DP_OnVictory + DP_OnRunLost + DP_OnVillagerDied.
//   2. Load GameLevel; find closest villager + pentagram + HUD.
//   3. Possess the villager via DP_Player::SetPossessedVillager.
//   4. For each of 5 objectives (Objective1..Objective5):
//        a. Teleport villager AWAY from pentagram to reset the
//           interactable's rising-edge tracker.
//        b. Synthesise a held item with the next objective tag
//           (same helper pattern as FullPlaythrough_Test). This
//           bypasses the find-the-real-item-on-map step; the spec's
//           "real geometry" requirement is exercised by HumanPlaythrough_Test
//           (Q-2026-05-12-002 resolved by m_bRequiresGraphics) so
//           headless can short-circuit.
//        c. Teleport villager INTO pentagram range.
//        d. Tick 1 frame for OnEnterRange -> HandleInteract to fire
//           and update DP_Win::GetCollectedObjectivesMask().
//   5. After all 5 deliveries, tick one more frame for the rising-edge
//      flush + DP_OnVictory dispatch + HUD subscriber to write the
//      "VICTORY" banner.
//   6. Snapshot DP_Win::HasWon, DP_Win::GetCollectedObjectivesMask,
//      the HUD Status text, and event-counts.
//   7. Verify:
//        DP_OnVictory fired exactly once.
//        DP_OnRunLost did NOT fire.
//        DP_OnVillagerDied count <= 1 (TestPlan §5.1 allows one burn-out).
//        DP_Win::HasWon() == true.
//        DP_Win::GetCollectedObjectivesMask() == DP_ALL_OBJECTIVES_MASK.
//        HUD Status text contains "VICTORY".
//        Test completes well under the 9000-frame MVPScope budget
//        (current implementation completes in ~30 frames; the 9000-cap
//        is a regression guard against the next contributor wiring in
//        a multi-second wait somewhere in the chain).
//
// What this catches (vs the 3 loss-state tests):
//   * DPPentagram's HandleInteract regressed and stopped calling
//     DP_Win::CollectObjective for a held objective tag.
//   * DP_Win::CollectObjective regressed and stopped setting the
//     bit in m_uObjectivesMask.
//   * The 5/5 -> DP_OnVictory dispatch broke (e.g., off-by-one in
//     the mask check, or a guard like `if(!m_bAlreadyWon)` that
//     never flips).
//   * The HUD's DP_OnVictory subscriber regressed (PR #75 wired this
//     alongside the run-lost banner; the integration is shared).
// ============================================================================

namespace
{
	enum Phase : int {
		kWG_Start, kWG_WaitScene, kWG_Possess,
		kWG_DeliverSetup, kWG_DeliverOutOfRange, kWG_DeliverInRange, kWG_DeliverFlush,
		kWG_FinalSettle, kWG_Snapshot, kWG_Verify, kWG_Done
	};

	int                     g_iPhase = kWG_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xPentagram;
	Zenith_Maths::Vector3   g_xPentagramPos(0.0f);
	int                     g_iObjectivesDelivered = 0;
	int                     g_iFinalSettleFrames = 0;

	// Event subscriptions + counters. The DoD asserts on exact counts
	// (DP_OnVictory == 1, DP_OnRunLost == 0, DP_OnVillagerDied <= 1) so
	// we tally independently rather than reading any one boolean flag.
	Zenith_EventHandle      g_xVictoryHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle      g_xDiedHandle    = INVALID_EVENT_HANDLE;
	int                     g_iVictoryCount = 0;
	int                     g_iRunLostCount = 0;
	int                     g_iVillagerDiedCount = 0;

	uint32_t                g_uMaskAtSnapshot = 0;
	bool                    g_bHasWonAtSnapshot = false;
	char                    g_szHudStatusText[128] = "<unset>";
	bool                    g_bHudStatusVisible = false;

	// Track which frame Victory fires on so we can verify "well under 9000".
	// Captured the frame DP_OnVictory dispatches.
	int                     g_iFrameAtVictory = -1;
	int                     g_iCurrentFrame = 0;

	constexpr int kSETTLE_FRAMES_AFTER_LAST_DELIVERY = 2;

	void OnVictory(const DP_OnVictory&)
	{
		++g_iVictoryCount;
		if (g_iFrameAtVictory < 0)
		{
			g_iFrameAtVictory = g_iCurrentFrame;
		}
	}
	void OnRunLost(const DP_OnRunLost&)    { ++g_iRunLostCount; }
	void OnVillagerDied(const DP_OnVillagerDied&) { ++g_iVillagerDiedCount; }

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

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

	bool TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		return true;
	}

	DPPentagram_Behaviour* GetPentagramScript()
	{
		DPPentagram_Behaviour* pxResult = nullptr;
		DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
			[&pxResult](Zenith_EntityID, DPPentagram_Behaviour& xP)
			{
				if (pxResult == nullptr) pxResult = &xP;
			});
		return pxResult;
	}

	// Walk all loaded scenes for "Status" (HUD lives in the active gameplay
	// scene, but mirror the PR #81 robust-lookup pattern in case future work
	// migrates the HUD to persistent).
	Zenith_UI::Zenith_UIText* FindStatusText()
	{
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		const uint32_t uSlotCount = g_xEngine.Scenes().GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_UIComponent>().ForEach(
				[&pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
				{
					if (pxResult != nullptr) return;
					pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
				});
			if (pxResult) break;
		}
		return pxResult;
	}

	// Mirror FullPlaythrough_Test's GiveSyntheticHeldItem: register a
	// throw-away EntityID with DP_Items so DP_Player::GetHeldItemTag
	// resolves to the requested objective tag.
	void GiveSyntheticObjective(Zenith_EntityID xVillager, DP_ItemTag eTag)
	{
		static uint32_t s_uFakeIdCounter = 0;
		Zenith_EntityID xFake;
		xFake.m_uIndex      = 0xE0000000u | s_uFakeIdCounter++;
		xFake.m_uGeneration = 0xE;
		DP_Items::Internal_RegisterItemTag(xFake, eTag);
		DP_Player::SetHeldItem(xVillager, xFake);
	}

	constexpr DP_ItemTag kObjectiveTagBySlot[5] = {
		DP_ItemTag::Objective1, DP_ItemTag::Objective2,
		DP_ItemTag::Objective3, DP_ItemTag::Objective4,
		DP_ItemTag::Objective5
	};
}

static void Setup_P4WinGolden()
{
	g_iPhase = kWG_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xPentagram = INVALID_ENTITY_ID;
	g_xPentagramPos = Zenith_Maths::Vector3(0.0f);
	g_iObjectivesDelivered = 0;
	g_iFinalSettleFrames = 0;
	g_iVictoryCount = 0;
	g_iRunLostCount = 0;
	g_iVillagerDiedCount = 0;
	g_uMaskAtSnapshot = 0;
	g_bHasWonAtSnapshot = false;
	std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText), "<unset>");
	g_bHudStatusVisible = false;
	g_iFrameAtVictory = -1;
	g_iCurrentFrame = 0;
	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictory);
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
	g_xDiedHandle    = Zenith_EventDispatcher::Get().Subscribe<DP_OnVillagerDied>(&OnVillagerDied);
}

static bool Step_P4WinGolden(int iFrame)
{
	g_iCurrentFrame = iFrame;
	switch (g_iPhase)
	{
	case kWG_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWG_WaitScene;
		return true;

	case kWG_WaitScene:
	{
		Zenith_EntityID xFoundV;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundV.IsValid()) xFoundV = xId;
			});
		Zenith_EntityID xFoundP;
		DPPentagram_Behaviour* pxPent = GetPentagramScript();
		if (pxPent != nullptr)
		{
			DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
				[&xFoundP](Zenith_EntityID xId, DPPentagram_Behaviour&)
				{
					if (!xFoundP.IsValid()) xFoundP = xId;
				});
		}
		if (xFoundV.IsValid() && xFoundP.IsValid())
		{
			g_xVillager = xFoundV;
			g_xPentagram = xFoundP;
			TryGetEntityPos(g_xPentagram, g_xPentagramPos);
			// Force the pentagram to interact on overlap (skips the F-key
			// poll path; matches FullPlaythrough_Test's pattern and the
			// 3 loss-state tests' rising-edge approach).
			if (pxPent != nullptr) pxPent->SetInteractOnOverlap(true);
			g_iPhase = kWG_Possess;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kWG_Done;
		}
		return true;
	}

	case kWG_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kWG_DeliverSetup;
		return true;

	case kWG_DeliverSetup:
	{
		// Out of range setup: teleport villager well clear of pentagram so the
		// next overlap is a rising edge. Drop any held item from the previous
		// objective.
		const Zenith_Maths::Vector3 xFar(
			g_xPentagramPos.x + 50.0f, g_xPentagramPos.y, g_xPentagramPos.z);
		TrySetEntityPos(g_xVillager, xFar);
		DP_Player::RemoveHeldItem(g_xVillager);
		g_iPhase = kWG_DeliverOutOfRange;
		return true;
	}

	case kWG_DeliverOutOfRange:
		// Tick one frame so DPInteractable_Behaviour::OnUpdate observes
		// the now-out-of-range villager and resets m_bWasInRangeLastFrame
		// to false. Without this the next teleport-into-range wouldn't
		// register as a rising edge.
		GiveSyntheticObjective(g_xVillager, kObjectiveTagBySlot[g_iObjectivesDelivered]);
		g_iPhase = kWG_DeliverInRange;
		return true;

	case kWG_DeliverInRange:
		// Teleport into pentagram range; the next OnUpdate's rising-edge
		// fires DPPentagram::HandleInteract -> DP_Win::CollectObjective.
		TrySetEntityPos(g_xVillager, g_xPentagramPos);
		g_iPhase = kWG_DeliverFlush;
		return true;

	case kWG_DeliverFlush:
		// One frame for HandleInteract to run + DP_Win mask to update.
		++g_iObjectivesDelivered;
		if (g_iObjectivesDelivered >= 5)
		{
			g_iFinalSettleFrames = 0;
			g_iPhase = kWG_FinalSettle;
		}
		else
		{
			g_iPhase = kWG_DeliverSetup;
		}
		return true;

	case kWG_FinalSettle:
		// Hold a couple of frames for DP_OnVictory dispatch + HUD
		// subscriber to write the "VICTORY" banner.
		++g_iFinalSettleFrames;
		if (g_iFinalSettleFrames >= kSETTLE_FRAMES_AFTER_LAST_DELIVERY)
		{
			g_iPhase = kWG_Snapshot;
		}
		return true;

	case kWG_Snapshot:
	{
		g_uMaskAtSnapshot = DP_Win::GetCollectedObjectivesMask();
		g_bHasWonAtSnapshot = DP_Win::HasWon();
		Zenith_UI::Zenith_UIText* pxStatus = FindStatusText();
		if (pxStatus != nullptr)
		{
			const std::string& strText = pxStatus->GetText();
			std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText),
				"%s", strText.c_str());
			g_bHudStatusVisible = pxStatus->IsVisible();
		}
		g_iPhase = kWG_Verify;
		return true;
	}

	case kWG_Verify:
		std::printf("[P4WinGolden] mask=0x%X hasWon=%d victory=%d runLost=%d died=%d "
			"hudStatusText='%s' hudVisible=%d frameAtVictory=%d totalFrames=%d (budget 9000)\n",
			g_uMaskAtSnapshot, (int)g_bHasWonAtSnapshot,
			g_iVictoryCount, g_iRunLostCount, g_iVillagerDiedCount,
			g_szHudStatusText, (int)g_bHudStatusVisible,
			g_iFrameAtVictory, g_iCurrentFrame);
		std::fflush(stdout);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xDiedHandle);
		g_xVictoryHandle = INVALID_EVENT_HANDLE;
		g_xRunLostHandle = INVALID_EVENT_HANDLE;
		g_xDiedHandle    = INVALID_EVENT_HANDLE;
		g_iPhase = kWG_Done;
		return false;

	case kWG_Done:
	default:
		if (g_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
			g_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
			g_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		if (g_xDiedHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xDiedHandle);
			g_xDiedHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}
}

static bool Verify_P4WinGolden()
{
	if (!g_xVillager.IsValid() || !g_xPentagram.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: setup entities missing -- villager.valid=%d pentagram.valid=%d",
			(int)g_xVillager.IsValid(), (int)g_xPentagram.IsValid());
		return false;
	}
	// MVPScope §1.5: must finish under 9000 frames. We're well under in
	// practice (~30 frames) but the cap exists as a regression guard.
	if (g_iCurrentFrame > 9000)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: test ran %d frames, exceeds MVPScope §1.5 cap of 9000. Something in the win path became orders-of-magnitude slower",
			g_iCurrentFrame);
		return false;
	}
	// Game-state contract: 5/5 objectives, HasWon flips, OnVictory dispatched once.
	if (g_uMaskAtSnapshot != DP_ALL_OBJECTIVES_MASK)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: objectives mask is 0x%X, expected 0x%X (all 5 bits). Pentagram's HandleInteract didn't call DP_Win::CollectObjective for at least one delivery -- check the rising-edge-flag flow between deliveries OR DP_Win::CollectObjective itself",
			g_uMaskAtSnapshot, (uint32_t)DP_ALL_OBJECTIVES_MASK);
		return false;
	}
	if (!g_bHasWonAtSnapshot)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: DP_Win::HasWon() == false even though mask is full. DP_Win's win-evaluation regressed -- the mask is being set but HasWon's threshold check isn't firing");
		return false;
	}
	if (g_iVictoryCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: DP_OnVictory fired %d times, expected exactly 1. >1 means missing dispatch-guard; 0 means the 5/5 -> OnVictory wiring broke",
			g_iVictoryCount);
		return false;
	}
	// Loss-side invariants: no RunLost during the win path, at most one
	// burn-out (TestPlan §5.1 allows it; current implementation runs in
	// ~30 frames so no villager would burn out, but the rule is preserved
	// against future test variants that take longer).
	if (g_iRunLostCount != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: DP_OnRunLost fired %d times during the win path -- the run was supposed to win, not lose. The most likely cause is the priest auto-pursued + apprehended during the playthrough, OR the night timer expired (Dawn cause)",
			g_iRunLostCount);
		return false;
	}
	if (g_iVillagerDiedCount > 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: DP_OnVillagerDied fired %d times; TestPlan §5.1 allows at most 1 (a single burn-out per run). Either the life-timer is decaying faster than spec OR multiple villagers are dying for non-life-timer reasons",
			g_iVillagerDiedCount);
		return false;
	}
	// HUD contract: PR #75 wired DPHUDController to DP_OnVictory; banner
	// should read "VICTORY" and be visible.
	if (std::strcmp(g_szHudStatusText, "VICTORY") != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: HUD Status text is '%s', expected 'VICTORY'. The DP_OnVictory subscriber in DPHUDController_Behaviour regressed (PR #75 wired it); OR a subsequent death banner overwrote the permanent victory banner",
			g_szHudStatusText);
		return false;
	}
	if (!g_bHudStatusVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4WinGolden: HUD Status text is set to VICTORY but visibility is false -- SetStatusText's SetVisible(true) didn't take");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP4WinGoldenTest = {
	"Test_P4Playthrough_Night1WinGolden",
	&Setup_P4WinGolden,
	&Step_P4WinGolden,
	&Verify_P4WinGolden,
	240  // Internal cap; we expect to complete in ~30 frames. The 9000-
	     // frame MVPScope budget is enforced separately inside Verify
	     // via g_iCurrentFrame. The smaller m_iMaxFrames cap here exists
	     // so a regression in the per-delivery state machine fails fast
	     // rather than spinning to 9000.
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP4WinGoldenTest);

#endif // ZENITH_INPUT_SIMULATOR
