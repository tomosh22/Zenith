#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPCommonTypes.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPDoor_Component.h"

#include <cstdio>

// ============================================================================
// Test_DPDoor -- pins the new door contracts from the 2026-05-25 overhaul:
//
//   Test_DPDoor_UnlockedOpensWithoutKey
//     A door with GetRequiredKey() == None opens on F-press without
//     requiring (or consuming) any held item, and DP_OnDoorLockRejected
//     does NOT fire.
//
//   Test_DPDoor_UnlockedCloseAndReopen
//     F-pressing an Open door transitions it to Closing -> Closed and
//     fires DP_OnDoorClosed. A second rising-edge F-press reopens it.
//
//   Test_DPDoor_LockedStaysUnlockedAfterCloseReopen
//     The first unlock consumes the Key and clears m_eRequiredKey;
//     subsequent close + reopen cycles work with EMPTY hand (no
//     DP_OnDoorLockRejected on the post-unlock re-open).
//
// The 2 other tests in the original plan
// (Test_DPDoor_LockedRequiresKey + Test_DPDoor_NavMeshReBlocksOnClose)
// are covered by existing tests:
//   * DoorUnlock_Test in Test_GameplaySystems.cpp (now filtered to a
//     locked door, fixed by the same change) covers the locked path.
//   * Test_P1NavMesh_ClosedDoorBlocksPath covers the navmesh-block
//     contract on a synthetic scene; the new code routes the same
//     SyncNavMeshBlock call on every state transition (verified by
//     code inspection + the existing test).
// ============================================================================

namespace
{
	template<typename T>
	T* GetGameComponent(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<T>();
	}

	bool TrySetEntityPosition(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		return true;
	}

	template<typename Pred>
	Zenith_EntityID FindDoorMatching(Pred xPred)
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachComponentInActiveScene<DPDoor_Component>(
			[&xResult, &xPred](Zenith_EntityID xId, DPDoor_Component& xDoor)
			{
				if (xResult.IsValid()) return;
				if (xPred(xDoor)) xResult = xId;
			});
		return xResult;
	}

	Zenith_EntityID FindFirstVillager()
	{
		Zenith_EntityID xResult;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xResult](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xResult.IsValid()) xResult = xId;
			});
		return xResult;
	}
}

// =========================================================================
// Test_DPDoor_UnlockedOpensWithoutKey
// =========================================================================
namespace UnlockedOpensState
{
	enum Phase : int { kStart, kWait, kSetup, kPressF, kVerify, kDone };
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xDoor;
	bool            g_bDoorOpenedBefore  = true;
	bool            g_bDoorOpenedAfter   = false;
	bool            g_bRejectionFired    = false;
	Zenith_EventHandle g_xRejectHandle   = INVALID_EVENT_HANDLE;
}

static void Setup_UnlockedOpens()
{
	using namespace UnlockedOpensState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xDoor     = INVALID_ENTITY_ID;
	g_bDoorOpenedBefore = true;
	g_bDoorOpenedAfter  = false;
	g_bRejectionFired   = false;
	g_xRejectHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorLockRejected>(
		[](const DP_OnDoorLockRejected&) { g_bRejectionFired = true; });
}

static bool Step_UnlockedOpens(int /*iFrame*/)
{
	using namespace UnlockedOpensState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
		g_xVillager = FindFirstVillager();
		g_xDoor = FindDoorMatching([](DPDoor_Component& xD) {
			return xD.GetRequiredKey() == DP_ItemTag::None;
		});
		if (!g_xVillager.IsValid() || !g_xDoor.IsValid()) return true;
		g_iPhase = kSetup;
		return true;

	case kSetup:
	{
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		g_bDoorOpenedBefore = pxDoor->IsOpen();   // expect false
		DP_Player::SetPossessedVillager(g_xVillager);
		// Ensure empty hand -- the contract is "unlocked door opens with
		// no key consumed", and DP_Player::RemoveHeldItem is safe to
		// call when no item is held (early-returns).
		DP_Player::RemoveHeldItem(g_xVillager);
		TrySetEntityPosition(g_xVillager,
			pxDoor->GetInteractionCentre() + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		g_iPhase = kPressF;
		return true;
	}

	case kPressF:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor != nullptr) g_bDoorOpenedAfter = pxDoor->IsOpen();
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_UnlockedOpens()
{
	using namespace UnlockedOpensState;
	// 2026-05-25 review fix: cleanup in Verify, unconditional, so the
	// rejection subscription's lambda can't leak into subsequent
	// batched tests (where it would flip `g_bRejectionFired` on any
	// dispatch). Verify always runs regardless of Step's exit path.
	if (g_xRejectHandle != INVALID_EVENT_HANDLE)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRejectHandle);
		g_xRejectHandle = INVALID_EVENT_HANDLE;
	}
	if (!g_xVillager.IsValid())  { std::printf("[UnlockedOpens] no villager\n"); return false; }
	if (!g_xDoor.IsValid())      { std::printf("[UnlockedOpens] no unlocked door\n"); return false; }
	if (g_bDoorOpenedBefore)     { std::printf("[UnlockedOpens] door pre-open\n"); return false; }
	if (!g_bDoorOpenedAfter)     { std::printf("[UnlockedOpens] door did not open\n"); return false; }
	if (g_bRejectionFired)       { std::printf("[UnlockedOpens] spurious lock-rejection event\n"); return false; }
	return true;
}

static const Zenith_AutomatedTest g_xUnlockedOpensTest = {
	"Test_DPDoor_UnlockedOpensWithoutKey",
	&Setup_UnlockedOpens, &Step_UnlockedOpens, &Verify_UnlockedOpens,
	/*maxFrames*/ 240, /*requiresGraphics*/ false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xUnlockedOpensTest);

// =========================================================================
// Test_DPDoor_UnlockedCloseAndReopen
// =========================================================================
namespace CloseReopenState
{
	enum Phase : int {
		kStart, kWait, kSetup, kPressOpen, kWaitOpen,
		kReleaseGap, kPressClose, kWaitClosed,
		kReleaseGap2, kPressReopen, kWaitReopened, kDone
	};
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xDoor;
	int             g_iWait = 0;
	bool            g_bClosedEventFired = false;
	bool            g_bSawClosedState   = false;
	bool            g_bReopened         = false;
	Zenith_EventHandle g_xClosedHandle  = INVALID_EVENT_HANDLE;
}

static void Setup_CloseReopen()
{
	using namespace CloseReopenState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xDoor     = INVALID_ENTITY_ID;
	g_iWait     = 0;
	g_bClosedEventFired = false;
	g_bSawClosedState   = false;
	g_bReopened         = false;
	// Pin dt so the 0.4 s open/close animations complete in a predictable
	// frame count (~24 frames at 60 Hz). Without this the headless harness
	// runs with microsecond-scale wall-clock dt and the animation never
	// progresses within the test's frame budget.
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_xClosedHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorClosed>(
		[](const DP_OnDoorClosed&) { g_bClosedEventFired = true; });
}

static bool Step_CloseReopen(int /*iFrame*/)
{
	using namespace CloseReopenState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
		g_xVillager = FindFirstVillager();
		g_xDoor = FindDoorMatching([](DPDoor_Component& xD) {
			return xD.GetRequiredKey() == DP_ItemTag::None;
		});
		if (!g_xVillager.IsValid() || !g_xDoor.IsValid()) return true;
		g_iPhase = kSetup;
		return true;

	case kSetup:
	{
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::RemoveHeldItem(g_xVillager);
		TrySetEntityPosition(g_xVillager,
			pxDoor->GetInteractionCentre() + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		g_iPhase = kPressOpen;
		return true;
	}

	case kPressOpen:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitOpen;
		g_iWait = 0;
		return true;

	case kWaitOpen:
	{
		// Wait for the Opening -> Open transition (m_fOpenDuration is
		// 0.4 s by default = ~24 frames at 60 Hz). Give it 60 to be safe.
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		if (pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Open) {
			g_iPhase = kReleaseGap;
			g_iWait = 0;
			return true;
		}
		if (g_iWait > 90) {
			g_iPhase = kDone;   // fail: never reached Open
			return false;
		}
		return true;
	}

	case kReleaseGap:
		// Skip a frame with no F-press so the rising-edge latch in
		// DPDoor resets (m_bWasInteractingLastFrame becomes false).
		++g_iWait;
		if (g_iWait >= 2) {
			g_iPhase = kPressClose;
			g_iWait = 0;
		}
		return true;

	case kPressClose:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitClosed;
		g_iWait = 0;
		return true;

	case kWaitClosed:
	{
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		if (pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Closed) {
			g_bSawClosedState = true;
			g_iPhase = kReleaseGap2;
			g_iWait = 0;
			return true;
		}
		if (g_iWait > 90) {
			g_iPhase = kDone;   // fail: never reached Closed
			return false;
		}
		return true;
	}

	case kReleaseGap2:
		++g_iWait;
		if (g_iWait >= 2) {
			g_iPhase = kPressReopen;
			g_iWait = 0;
		}
		return true;

	case kPressReopen:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitReopened;
		g_iWait = 0;
		return true;

	case kWaitReopened:
	{
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor != nullptr && pxDoor->IsOpen()) {
			g_bReopened = true;
		}
		if (g_iWait > 90 || g_bReopened) {
			g_iPhase = kDone;
			return false;
		}
		return true;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_CloseReopen()
{
	using namespace CloseReopenState;
	// 2026-05-25 review fix: do cleanup in Verify unconditionally so
	// timeout / early-exit paths in Step don't leak the subscription
	// (a dangling lambda mutates `g_bClosedEventFired` for any
	// SUBSEQUENT batched test that happens to dispatch DP_OnDoorClosed,
	// silently corrupting cross-test state).
	Zenith_InputSimulator::ClearFixedDt();
	if (g_xClosedHandle != INVALID_EVENT_HANDLE)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(g_xClosedHandle);
		g_xClosedHandle = INVALID_EVENT_HANDLE;
	}
	if (!g_xDoor.IsValid())     { std::printf("[CloseReopen] no door\n"); return false; }
	if (!g_bSawClosedState)     { std::printf("[CloseReopen] never reached Closed state\n"); return false; }
	if (!g_bClosedEventFired)   { std::printf("[CloseReopen] DP_OnDoorClosed never fired\n"); return false; }
	if (!g_bReopened)           { std::printf("[CloseReopen] door did not re-open\n"); return false; }
	return true;
}

static const Zenith_AutomatedTest g_xCloseReopenTest = {
	"Test_DPDoor_UnlockedCloseAndReopen",
	&Setup_CloseReopen, &Step_CloseReopen, &Verify_CloseReopen,
	/*maxFrames*/ 600, /*requiresGraphics*/ false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCloseReopenTest);

// =========================================================================
// Test_DPDoor_LockedStaysUnlockedAfterCloseReopen
// Pins the sticky-unlock contract: once a key is consumed, the door's
// m_eRequiredKey is cleared and subsequent close + reopen cycles work
// with an empty hand (no DP_OnDoorLockRejected fires on the re-open).
// =========================================================================
namespace StickyUnlockState
{
	enum Phase : int {
		kStart, kWait, kSetup, kPressUnlock, kWaitOpen,
		kReleaseGap, kPressClose, kWaitClosed,
		kReleaseGap2, kPressReopenEmpty, kWaitReopened, kDone
	};
	int             g_iPhase = kStart;
	Zenith_EntityID g_xVillager;
	Zenith_EntityID g_xDoor;
	Zenith_EntityID g_xKeyItem;
	int             g_iWait = 0;
	bool            g_bRejectionFired = false;   // must stay false
	bool            g_bDoorReopenedAfterClose = false;
	Zenith_EventHandle g_xRejectHandle = INVALID_EVENT_HANDLE;
}

static void Setup_StickyUnlock()
{
	using namespace StickyUnlockState;
	g_iPhase = kStart;
	g_xVillager = INVALID_ENTITY_ID;
	g_xDoor     = INVALID_ENTITY_ID;
	g_xKeyItem  = INVALID_ENTITY_ID;
	g_iWait     = 0;
	g_bRejectionFired         = false;
	g_bDoorReopenedAfterClose = false;
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_xRejectHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnDoorLockRejected>(
		[](const DP_OnDoorLockRejected&) { g_bRejectionFired = true; });
}

static bool Step_StickyUnlock(int /*iFrame*/)
{
	using namespace StickyUnlockState;
	switch (g_iPhase)
	{
	case kStart:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWait;
		return true;

	case kWait:
		g_xVillager = FindFirstVillager();
		g_xDoor = FindDoorMatching([](DPDoor_Component& xD) {
			return xD.GetRequiredKey() == DP_ItemTag::Key;
		});
		if (!g_xVillager.IsValid() || !g_xDoor.IsValid()) return true;
		g_iPhase = kSetup;
		return true;

	case kSetup:
	{
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		// Synthetic Key (same trick DoorUnlock_Test uses -- a fake
		// EntityID registered under the Key tag; TryConsumeKeyForUnlock's
		// destroy path early-exits when the ID doesn't resolve to a
		// real entity).
		g_xKeyItem.m_uIndex      = 999111;
		g_xKeyItem.m_uGeneration = 1;
		DP_Items::Internal_RegisterItemTag(g_xKeyItem, DP_ItemTag::Key);
		DP_Player::SetPossessedVillager(g_xVillager);
		DP_Player::SetHeldItem(g_xVillager, g_xKeyItem);
		TrySetEntityPosition(g_xVillager,
			pxDoor->GetInteractionCentre() + Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		g_iPhase = kPressUnlock;
		return true;
	}

	case kPressUnlock:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitOpen;
		g_iWait = 0;
		return true;

	case kWaitOpen:
	{
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		if (pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Open) {
			g_iPhase = kReleaseGap;
			g_iWait = 0;
			return true;
		}
		if (g_iWait > 90) {
			g_iPhase = kDone;
			return false;
		}
		return true;
	}

	case kReleaseGap:
		++g_iWait;
		if (g_iWait >= 2) { g_iPhase = kPressClose; g_iWait = 0; }
		return true;

	case kPressClose:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitClosed;
		g_iWait = 0;
		return true;

	case kWaitClosed:
	{
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor == nullptr) return true;
		if (pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Closed) {
			g_iPhase = kReleaseGap2;
			g_iWait = 0;
			return true;
		}
		if (g_iWait > 90) {
			g_iPhase = kDone;
			return false;
		}
		return true;
	}

	case kReleaseGap2:
		++g_iWait;
		if (g_iWait >= 2) { g_iPhase = kPressReopenEmpty; g_iWait = 0; }
		return true;

	case kPressReopenEmpty:
		// CRITICAL: empty-hand re-press. The original Key was consumed
		// on the unlock; m_eRequiredKey should now be None so this
		// F-press just opens the door without firing
		// DP_OnDoorLockRejected.
		DP_Player::RemoveHeldItem(g_xVillager);
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_F);
		g_iPhase = kWaitReopened;
		g_iWait = 0;
		return true;

	case kWaitReopened:
	{
		++g_iWait;
		DPDoor_Component* pxDoor = GetGameComponent<DPDoor_Component>(g_xDoor);
		if (pxDoor != nullptr && pxDoor->IsOpen()) {
			g_bDoorReopenedAfterClose = true;
		}
		if (g_iWait > 90 || g_bDoorReopenedAfterClose) {
			g_iPhase = kDone;
			return false;
		}
		return true;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_StickyUnlock()
{
	using namespace StickyUnlockState;
	// 2026-05-25 review fix: do cleanup in Verify unconditionally so
	// timeout paths (kWaitOpen timeout, kWaitClosed timeout) don't
	// leak the synthetic Key tag into subsequent batched tests AND
	// don't leak the rejection subscription's lambda. Both would
	// silently corrupt downstream tests.
	Zenith_InputSimulator::ClearFixedDt();
	if (g_xKeyItem.IsValid())
	{
		DP_Items::Internal_UnregisterItemTag(g_xKeyItem);
	}
	if (g_xRejectHandle != INVALID_EVENT_HANDLE)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRejectHandle);
		g_xRejectHandle = INVALID_EVENT_HANDLE;
	}
	if (!g_xDoor.IsValid())            { std::printf("[StickyUnlock] no locked door\n"); return false; }
	if (g_bRejectionFired)             { std::printf("[StickyUnlock] empty-hand re-press fired DP_OnDoorLockRejected (sticky-unlock broken)\n"); return false; }
	if (!g_bDoorReopenedAfterClose)    { std::printf("[StickyUnlock] door did not re-open with empty hand\n"); return false; }
	return true;
}

static const Zenith_AutomatedTest g_xStickyUnlockTest = {
	"Test_DPDoor_LockedStaysUnlockedAfterCloseReopen",
	&Setup_StickyUnlock, &Step_StickyUnlock, &Verify_StickyUnlock,
	/*maxFrames*/ 600, /*requiresGraphics*/ false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xStickyUnlockTest);

#endif // ZENITH_INPUT_SIMULATOR
