#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Component.h"
#include "Tests/DP_TestGraphHelpers.h"

// ============================================================================
// PentagramVictory_Test
//
// Drives the win loop end-to-end:
//   1. Load GameLevel.
//   2. Find the pentagram entity + a villager.
//   3. Subscribe to DP_OnVictory.
//   4. For each of the 5 objectives:
//        - Synthesize a held item: register a fake EntityID with the
//          right tag in DP_Items, then DP_Player::SetHeldItem.
//        - Teleport the villager onto the pentagram so DPInteractable's
//          rising-edge fires DP_OnInteract -> HandleInteract ->
//          DP_Win::NotifyObjectiveCollected.
//        - Move the villager away so the next iteration's rising-edge
//          fires (range exit clears m_bWasInRangeLastFrame).
//   5. After 5 cycles, verify DP_Win::HasWon() and DP_OnVictory fired.
//
// Note: because DPInteractable_Base::OnEnterRange polls
// DP_Input::ReadInteractPressed (F-press), and our headless harness has no
// keyboard input, the on-overlap path is taken instead. The pentagram is
// configured with bInteractOnOverlap=false by default, so we'd never trigger.
// This test instead dispatches DP_OnInteract directly — the pentagram
// subscribes to that event when in range, and its lambda invokes
// HandleInteract.
//
// To make the rising-edge subscription actually fire, the villager must
// transition into range first. We teleport, wait one frame (so OnUpdate
// runs the IsVillagerInRange check and subscribes), then dispatch
// DP_OnInteract. After consumption we move the villager out so the next
// objective triggers a fresh rising-edge.
// ============================================================================

namespace
{
	enum Phase : int {
		kPV_Start,
		kPV_WaitScene,
		kPV_FindPentagram,
		kPV_TeleportNear,
		kPV_WaitForSubscribe,
		kPV_Synthesize,
		kPV_DispatchInteract,
		kPV_WaitProcess,
		kPV_MoveAway,
		kPV_NextObjective,
		kPV_Verify,
		kPV_Done
	};

	int             g_iPVPhase      = kPV_Start;
	Zenith_EntityID g_xPVVillager;
	Zenith_EntityID g_xPVPentagram;
	int             g_iCurrentObjective = 0;
	uint32_t        g_uFakeIdCounter    = 0;
	bool            g_bVictoryFired     = false;
	uint32_t        g_uFinalMask        = 0;
	bool            g_bFinalHasWon      = false;
	bool            g_bPVPassed         = false;
	Zenith_EventHandle g_xVictoryHandle = INVALID_EVENT_HANDLE;
	Zenith_EntityID g_xCurrentFakeItem;

	// Generate non-conflicting fake EntityIDs for synthesised held items.
	Zenith_EntityID MakeFakeItemId()
	{
		Zenith_EntityID xId;
		xId.m_uIndex      = 0xF0000000u + g_uFakeIdCounter++;
		xId.m_uGeneration = 0xF;
		return xId;
	}

	DP_ItemTag ObjectiveTagAtIndex(int iIndex)
	{
		switch (iIndex)
		{
			case 0: return DP_ItemTag::Objective1;
			case 1: return DP_ItemTag::Objective2;
			case 2: return DP_ItemTag::Objective3;
			case 3: return DP_ItemTag::Objective4;
			case 4: return DP_ItemTag::Objective5;
			default: return DP_ItemTag::None;
		}
	}

	void OnVictoryEvent(const DP_OnVictory&)
	{
		g_bVictoryFired = true;
	}

	void TeleportVillager(const Zenith_Maths::Vector3& xPos)
	{
		if (!g_xPVVillager.IsValid()) return;
		Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(g_xPVVillager);
		Zenith_TransformComponent* pxTransform = xV.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		pxTransform->SetPosition(xPos);
	}

	Zenith_Maths::Vector3 GetPentagramPosition()
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (!g_xPVPentagram.IsValid()) return xPos;
		Zenith_Entity xP = g_xEngine.Scenes().ResolveEntity(g_xPVPentagram);
		Zenith_TransformComponent* pxTransform = xP.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return xPos;
		pxTransform->GetPosition(xPos);
		return xPos;
	}
}

static void Setup_PentagramVictory()
{
	g_iPVPhase          = kPV_Start;
	g_xPVVillager       = INVALID_ENTITY_ID;
	g_xPVPentagram      = INVALID_ENTITY_ID;
	g_iCurrentObjective = 0;
	g_uFakeIdCounter    = 0;
	g_bVictoryFired     = false;
	g_uFinalMask        = 0;
	g_bFinalHasWon      = false;
	g_bPVPassed         = false;
	g_xCurrentFakeItem  = INVALID_ENTITY_ID;

	DP_Win::Reset();
	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictoryEvent);
}

static bool Step_PentagramVictory(int iFrame)
{
	switch (g_iPVPhase)
	{
	case kPV_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPVPhase = kPV_WaitScene;
		return true;

	case kPV_WaitScene:
	{
		Zenith_EntityID xFoundV;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Component&) { xFoundV = xId; });
		if (xFoundV.IsValid())
		{
			g_xPVVillager = xFoundV;
			g_iPVPhase = kPV_FindPentagram;
		}
		else if (iFrame > 60)
		{
			g_iPVPhase = kPV_Done;
		}
		return true;
	}

	case kPV_FindPentagram:
	{
		Zenith_EntityID xFoundP = DP_FindFirstEntityWithGraph("game:Graphs/DP_Pentagram.bgraph");
		if (xFoundP.IsValid())
		{
			g_xPVPentagram = xFoundP;
			DP_Player::SetPossessedVillager(g_xPVVillager);
			g_iPVPhase = kPV_TeleportNear;
		}
		else if (iFrame > 90)
		{
			g_iPVPhase = kPV_Done;
		}
		return true;
	}

	case kPV_TeleportNear:
	{
		// Move villager OUT of pentagram range first to ensure the next
		// teleport is a clean rising-edge (DPInteractable tracks
		// m_bWasInRangeLastFrame; if we're already in range, the subscription
		// fires immediately on this frame's OnUpdate and we miss the chance
		// to set up the held item).
		TeleportVillager(Zenith_Maths::Vector3(-100.0f, 0.0f, -100.0f));
		g_iPVPhase = kPV_Synthesize;
		return true;
	}

	case kPV_Synthesize:
	{
		// Build a fake held-item entry: register a tag for a synthetic
		// EntityID, then SetHeldItem so DP_Player tracks the tag.
		const DP_ItemTag eObj = ObjectiveTagAtIndex(g_iCurrentObjective);
		if (eObj == DP_ItemTag::None)
		{
			g_iPVPhase = kPV_Verify;
			return true;
		}
		g_xCurrentFakeItem = MakeFakeItemId();
		DP_Items::Internal_RegisterItemTag(g_xCurrentFakeItem, eObj);
		DP_Player::SetHeldItem(g_xPVVillager, g_xCurrentFakeItem);
		g_iPVPhase = kPV_WaitForSubscribe;
		return true;
	}

	case kPV_WaitForSubscribe:
	{
		// Teleport the villager onto the pentagram. On the NEXT frame, the
		// pentagram's OnUpdate observes IsVillagerInRange = true and (because
		// m_bWasInRangeLastFrame was false) fires OnEnterRange, subscribing to
		// DP_OnInteract.
		TeleportVillager(GetPentagramPosition());
		g_iPVPhase = kPV_DispatchInteract;
		return true;
	}

	case kPV_DispatchInteract:
	{
		// Dispatch the DP_OnInteract event explicitly. The pentagram's
		// rising-edge subscription (set up on the previous frame) will pick
		// it up, validate the target, and call HandleInteract.
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnInteract{ g_xPVVillager, g_xPVPentagram });
		g_iPVPhase = kPV_WaitProcess;
		return true;
	}

	case kPV_WaitProcess:
		// Give the dispatcher one frame to drain.
		g_iPVPhase = kPV_MoveAway;
		return true;

	case kPV_MoveAway:
	{
		// Move villager out of range so the next objective gets a fresh
		// rising-edge. The pentagram's HandleInteract will have called
		// DP_Player::RemoveHeldItem and DP_Win::NotifyObjectiveCollected.
		TeleportVillager(Zenith_Maths::Vector3(-100.0f, 0.0f, -100.0f));
		// Clean up the synthesised tag entry so future iterations don't
		// accumulate stale registrations.
		DP_Items::Internal_UnregisterItemTag(g_xCurrentFakeItem);
		g_xCurrentFakeItem = INVALID_ENTITY_ID;
		g_iPVPhase = kPV_NextObjective;
		return true;
	}

	case kPV_NextObjective:
	{
		++g_iCurrentObjective;
		if (g_iCurrentObjective >= 5)
		{
			g_iPVPhase = kPV_Verify;
		}
		else
		{
			g_iPVPhase = kPV_Synthesize;
		}
		return true;
	}

	case kPV_Verify:
	{
		g_uFinalMask   = DP_Win::GetCollectedObjectivesMask();
		g_bFinalHasWon = DP_Win::HasWon();
		g_bPVPassed    = g_bVictoryFired
		              && g_bFinalHasWon
		              && (g_uFinalMask == DP_ALL_OBJECTIVES_MASK);

		// Tear down the event subscription.
		if (g_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
			g_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		g_iPVPhase = kPV_Done;
		return false;
	}

	case kPV_Done:
	default:
		return false;
	}
}

static bool Verify_PentagramVictory()
{
	return g_bPVPassed;
}

static const Zenith_AutomatedTest g_xPentagramVictoryTest = {
	"PentagramVictory_Test",
	&Setup_PentagramVictory,
	&Step_PentagramVictory,
	&Verify_PentagramVictory,
	600
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPentagramVictoryTest);

#endif // ZENITH_INPUT_SIMULATOR
