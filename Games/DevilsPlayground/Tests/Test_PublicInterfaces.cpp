#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DP_Tuning.h"
#include "../Components/DPItemManager_Behaviour.h"
#include "../Components/DPPlayerController_Behaviour.h"
#include "../Components/DPFogPass_Behaviour.h"

#include <cstdio>

// ============================================================================
// PublicInterfaces tests — pin down the contract for DP_Player / DP_Items /
// DP_Fog / DP_Win without needing scene-level entities. Each test runs against
// the namespace API directly, populating side-tables with synthetic EntityIDs.
//
// These cover the three SourceBugFixed: guards (RemoveHeldItem null-deref,
// FindItemByTag deref-on-miss, Interactable overlap-exit) plus the per-frame
// fog hole rebuild contract.
// ============================================================================

namespace
{
	Zenith_EntityID MakeFakeId(uint32_t uIndex, uint32_t uGen = 1)
	{
		Zenith_EntityID xId;
		xId.m_uIndex      = uIndex;
		xId.m_uGeneration = uGen;
		return xId;
	}

	bool   g_bAllPassed   = true;
	int    g_iFailCount   = 0;
	#define DP_EXPECT(cond, msg)                                  \
		do {                                                       \
			if (!(cond)) {                                         \
				g_bAllPassed = false;                              \
				++g_iFailCount;                                    \
				std::printf("  FAIL: %s\n", msg);                  \
			}                                                      \
		} while (0)
}

// ----------------------------------------------------------------------------
// Test: DP_Player held-item lifecycle (incl. RemoveHeldItem source-bug guard)
// ----------------------------------------------------------------------------
namespace
{
	bool g_bHeldItemSetupRan = false;
}

static void Setup_HeldItem()
{
	g_bHeldItemSetupRan = true;
	g_bAllPassed = true;
	g_iFailCount = 0;

	// 2026-05-17: DP_Items + DP_Player side tables were moved onto
	// DPItemManager_Behaviour + DPPlayerController_Behaviour so they
	// auto-clean on scene unload. Any test that exercises
	// Internal_RegisterItemTag / SetHeldItem has to load a scene with
	// both scripts attached -- otherwise the registration silently
	// no-ops. Spin up a one-entity scene with both scripts attached.
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("HeldItemTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xManagerEntity = g_xEngine.Scenes().CreateEntity(pxScene, "ManagerEntity");
	Zenith_ScriptComponent& xScripts =
		xManagerEntity.AddComponent<Zenith_ScriptComponent>();
	xScripts.AddScript<DPItemManager_Behaviour>();
	xScripts.AddScript<DPPlayerController_Behaviour>();

	const Zenith_EntityID xVillager = MakeFakeId(1001);
	const Zenith_EntityID xItemKey  = MakeFakeId(2001);

	// Register the item's tag so DP_Items::GetItemTag returns the right thing
	// when DP_Player::SetHeldItem looks it up.
	DP_Items::Internal_RegisterItemTag(xItemKey, DP_ItemTag::Key);

	// Initial: no item.
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager)    == DP_ItemTag::None,    "initial tag");
	DP_EXPECT(!DP_Player::GetHeldItemEntity(xVillager).IsValid(),             "initial entity");

	// Set held: tag and entity reflect.
	DP_Player::SetHeldItem(xVillager, xItemKey);
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager)    == DP_ItemTag::Key,     "set→tag=Key");
	DP_EXPECT(DP_Player::GetHeldItemEntity(xVillager).m_uIndex == xItemKey.m_uIndex,
		"set→entity index");

	// Remove held: clears.
	DP_Player::RemoveHeldItem(xVillager);
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager)    == DP_ItemTag::None,    "remove→tag");
	DP_EXPECT(!DP_Player::GetHeldItemEntity(xVillager).IsValid(),             "remove→entity");

	// SourceBugFixed: removing again on an already-empty villager must NOT crash.
	DP_Player::RemoveHeldItem(xVillager);
	DP_Player::RemoveHeldItem(xVillager);
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager)    == DP_ItemTag::None,    "double-remove safe");

	DP_Items::Internal_UnregisterItemTag(xItemKey);
	g_xEngine.Scenes().UnloadScene(xScene);
}

static bool Step_HeldItem(int /*iFrame*/)            { return false; /* one-shot */ }
static bool Verify_HeldItem()                        { return g_bHeldItemSetupRan && g_bAllPassed; }

static const Zenith_AutomatedTest g_xHeldItemTest = {
	"DP_HeldItem_Test", &Setup_HeldItem, &Step_HeldItem, &Verify_HeldItem, 5
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xHeldItemTest);

// ----------------------------------------------------------------------------
// Test: DP_Items::FindItemByTag — guarded miss, hit, unregister, miss-again,
// scene-unload purge.
//
// 2026-05-17: g_xItemTagTable was moved out of the PublicInterfaces.cpp anon
// namespace onto DPItemManager_Behaviour as an instance member. That means
// FindItemByTag forwards through DPItemManager_Behaviour::Instance() and the
// table dies with the scene that owns the manager -- no stale rows can leak
// across scenes. Test setup creates a scene with a DPItemManager script
// attached and exercises the same register/find/unregister round-trip as
// before, plus a scene-unload step that proves the new ownership boundary
// actually purges the registry (which the old global-map design could not
// guarantee).
// ----------------------------------------------------------------------------
static bool g_bFindByTagSetupRan = false;

static void Setup_FindByTag()
{
	g_bFindByTagSetupRan = true;
	g_bAllPassed = true;

	// 1) Empty state: no DPItemManager loaded -> FindItemByTag misses.
	{
		const Zenith_EntityID xMiss = DP_Items::FindItemByTag(DP_ItemTag::Iron);
		DP_EXPECT(!xMiss.IsValid(),
			"no-manager-loaded -> FindItemByTag returns invalid");
	}

	// 2) Spin up an in-memory scene with a DPItemManager attached
	// (creates DPItemManager_Behaviour::Instance()) + a real item
	// entity to register against.
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("FindByTagTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xManagerEntity = g_xEngine.Scenes().CreateEntity(pxScene, "ManagerEntity");
	xManagerEntity.AddComponent<Zenith_ScriptComponent>()
		.AddScript<DPItemManager_Behaviour>();
	Zenith_Entity xIronEntity = g_xEngine.Scenes().CreateEntity(pxScene, "IronEntity");
	const Zenith_EntityID xIron = xIronEntity.GetEntityID();

	// 3) Pre-register miss: manager exists but no Iron in table.
	const Zenith_EntityID xMiss = DP_Items::FindItemByTag(DP_ItemTag::Iron);
	DP_EXPECT(!xMiss.IsValid(),
		"manager-loaded, empty-table -> FindItemByTag returns invalid");

	// 4) Register, find returns the right entity.
	DP_Items::Internal_RegisterItemTag(xIron, DP_ItemTag::Iron);
	const Zenith_EntityID xHit = DP_Items::FindItemByTag(DP_ItemTag::Iron);
	DP_EXPECT(xHit.IsValid(),                     "find->hit valid");
	DP_EXPECT(xHit.m_uIndex == xIron.m_uIndex,    "find->correct index");
	DP_EXPECT(DP_Items::GetItemTag(xHit) == DP_ItemTag::Iron, "GetItemTag round-trip");

	// 5) Unregister, find misses again.
	DP_Items::Internal_UnregisterItemTag(xIron);
	const Zenith_EntityID xMiss2 = DP_Items::FindItemByTag(DP_ItemTag::Iron);
	DP_EXPECT(!xMiss2.IsValid(), "find->post-unregister miss");

	// 6) Re-register + Unload the scene. Pins the new ownership
	//    boundary: scene unload destroys the manager entity, which
	//    drops its m_xItemTagTable -- subsequent FindItemByTag with
	//    no manager loaded must return invalid. The old global-map
	//    design could leak across this boundary; the new design
	//    cannot.
	DP_Items::Internal_RegisterItemTag(xIron, DP_ItemTag::Iron);
	const Zenith_EntityID xHit2 = DP_Items::FindItemByTag(DP_ItemTag::Iron);
	DP_EXPECT(xHit2.IsValid(), "re-register before unload -> valid");
	g_xEngine.Scenes().UnloadScene(xScene);
	const Zenith_EntityID xMissAfterUnload = DP_Items::FindItemByTag(DP_ItemTag::Iron);
	DP_EXPECT(!xMissAfterUnload.IsValid(),
		"scene-unload -> manager destroyed -> FindItemByTag returns invalid "
		"(proves the registry is owned by the scene, not by a process-global)");
}

static bool Step_FindByTag(int)                      { return false; }
static bool Verify_FindByTag()                       { return g_bFindByTagSetupRan && g_bAllPassed; }

static const Zenith_AutomatedTest g_xFindByTagTest = {
	"DP_FindItemByTag_Test", &Setup_FindByTag, &Step_FindByTag, &Verify_FindByTag, 5
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFindByTagTest);

// ----------------------------------------------------------------------------
// Test: DP_Win pentagram flow — collect 5 objectives, victory fires
// ----------------------------------------------------------------------------
static bool g_bWinSetupRan = false;
static bool g_bVictoryFired = false;
static Zenith_EventHandle g_xVictoryHandle = INVALID_EVENT_HANDLE;

static void OnVictoryEvent(const DP_OnVictory&)
{
	g_bVictoryFired = true;
}

static void Setup_Win()
{
	g_bWinSetupRan = true;
	g_bAllPassed = true;
	g_bVictoryFired = false;

	// 2026-05-17: DP_Win's state lives on DPPlayerController_Behaviour
	// (was process-global before). Tests that exercise NotifyObjective-
	// Collected / Reset have to spin up a scene with the controller
	// attached -- otherwise every call silently no-ops on a null
	// Instance() and the assertions all fail. Mirrors Setup_HeldItem.
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("WinTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xManagerEntity = g_xEngine.Scenes().CreateEntity(pxScene, "ManagerEntity");
	Zenith_ScriptComponent& xScripts =
		xManagerEntity.AddComponent<Zenith_ScriptComponent>();
	xScripts.AddScript<DPPlayerController_Behaviour>();

	DP_Win::Reset();

	g_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(&OnVictoryEvent);

	DP_EXPECT(DP_Win::GetCollectedObjectivesMask() == 0, "initial mask = 0");
	DP_EXPECT(!DP_Win::HasWon(),                         "initial !HasWon");

	// Read the win threshold from tuning so this test stays correct as
	// the design ratchets between "3 of 5" and "5 of 5" without code
	// changes (see DP_Win::NotifyObjectiveCollected). Clamp to [1, 5]
	// because the available objective bits are 1..5.
	int iRequired = DP_Tuning::Get<int>("night.reagents_required_for_victory");
	if (iRequired < 1) iRequired = 1;
	if (iRequired > 5) iRequired = 5;

	// Collect objectives one-by-one. Below the threshold => !HasWon and
	// no victory event. At the threshold => HasWon and event fires
	// exactly once. The remaining objectives still set their bits but
	// do not re-fire the event (NotifyObjectiveCollected gates on
	// m_bHasWon).
	const DP_ItemTag aeOrder[5] = {
		DP_ItemTag::Objective1,
		DP_ItemTag::Objective2,
		DP_ItemTag::Objective3,
		DP_ItemTag::Objective4,
		DP_ItemTag::Objective5,
	};
	for (int i = 0; i < 5; ++i)
	{
		DP_Win::NotifyObjectiveCollected(aeOrder[i]);
		const int iCollected = i + 1;
		const bool bShouldHaveWon = (iCollected >= iRequired);
		DP_EXPECT(DP_Win::HasWon() == bShouldHaveWon,
			bShouldHaveWon ? "at/over threshold -> HasWon"
			               : "below threshold -> !HasWon");
		DP_EXPECT(g_bVictoryFired == bShouldHaveWon,
			bShouldHaveWon ? "at/over threshold -> victory event fired"
			               : "below threshold -> no victory event");
	}

	// After collecting all 5, mask should be every objective bit set.
	DP_EXPECT(DP_Win::GetCollectedObjectivesMask() == DP_ALL_OBJECTIVES_MASK,
		"5/5 mask all set");

	// Re-collecting an already-set objective should be idempotent.
	DP_Win::NotifyObjectiveCollected(DP_ItemTag::Objective1);
	DP_EXPECT(DP_Win::GetCollectedObjectivesMask() == DP_ALL_OBJECTIVES_MASK,
		"re-collect idempotent");

	// Reset clears state.
	DP_Win::Reset();
	DP_EXPECT(DP_Win::GetCollectedObjectivesMask() == 0, "reset -> mask = 0");
	DP_EXPECT(!DP_Win::HasWon(),                         "reset -> !HasWon");

	// Non-objective tags ignored.
	DP_Win::NotifyObjectiveCollected(DP_ItemTag::Key);
	DP_EXPECT(DP_Win::GetCollectedObjectivesMask() == 0, "non-objective ignored");

	Zenith_EventDispatcher::Get().Unsubscribe(g_xVictoryHandle);
	g_xVictoryHandle = INVALID_EVENT_HANDLE;
	g_xEngine.Scenes().UnloadScene(xScene);
}

static bool Step_Win(int)                            { return false; }
static bool Verify_Win()                             { return g_bWinSetupRan && g_bAllPassed; }

static const Zenith_AutomatedTest g_xWinTest = {
	"DP_Win_Test", &Setup_Win, &Step_Win, &Verify_Win, 5
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xWinTest);

// ----------------------------------------------------------------------------
// Test: DP_Fog clear-and-rebuild contract
// ----------------------------------------------------------------------------
static bool g_bFogSetupRan = false;

static void Setup_Fog()
{
	g_bFogSetupRan = true;
	g_bAllPassed = true;

	// 2026-05-17 ownership refactor: DP_Fog's fog-hole table moved
	// onto DPFogPass_Behaviour::m_xFogHoles. Tests now need that
	// script attached to a scene entity for any of the DP_Fog::*
	// forwarders to take effect (no-ops otherwise).
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("FogTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xFogEntity = g_xEngine.Scenes().CreateEntity(pxScene, "FogPassEntity");
	xFogEntity.AddComponent<Zenith_ScriptComponent>()
		.AddScript<DPFogPass_Behaviour>();

	DP_Fog::ClearAllFogHoles();

	DP_EXPECT(DP_Fog::GetFogHoleCount() == 0, "initial count = 0");

	DP_Fog::RegisterFogHole(MakeFakeId(7001), 4.0f);
	DP_Fog::RegisterFogHole(MakeFakeId(7002), 4.0f);
	DP_Fog::RegisterFogHole(MakeFakeId(7003), 5.0f);
	DP_EXPECT(DP_Fog::GetFogHoleCount() == 3, "after 3 registers");

	// Idempotent upsert: re-registering same EntityID does not double-count.
	DP_Fog::RegisterFogHole(MakeFakeId(7001), 6.0f);
	DP_EXPECT(DP_Fog::GetFogHoleCount() == 3, "re-register idempotent");

	DP_Fog::UnregisterFogHole(MakeFakeId(7002));
	DP_EXPECT(DP_Fog::GetFogHoleCount() == 2, "after unregister");

	DP_Fog::ClearAllFogHoles();
	DP_EXPECT(DP_Fog::GetFogHoleCount() == 0, "after clear");

	g_xEngine.Scenes().UnloadScene(xScene);
}

static bool Step_Fog(int)                            { return false; }
static bool Verify_Fog()                             { return g_bFogSetupRan && g_bAllPassed; }

static const Zenith_AutomatedTest g_xFogTest = {
	"DP_Fog_Test", &Setup_Fog, &Step_Fog, &Verify_Fog, 5
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFogTest);

// ----------------------------------------------------------------------------
// Test: DP_Items::TryConsumeKeyForUnlock — Key consumed, SkeletonKey not, wrong key fails
// ----------------------------------------------------------------------------
static bool g_bUnlockSetupRan = false;

static void Setup_Unlock()
{
	g_bUnlockSetupRan = true;
	g_bAllPassed = true;

	// Need a DPItemManager- + DPPlayerController-attached scene for the
	// item-tag + held-item registrations below to take effect (see
	// Setup_HeldItem's matching comment for the 2026-05-17 ownership-
	// refactor rationale).
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("UnlockTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xManagerEntity = g_xEngine.Scenes().CreateEntity(pxScene, "ManagerEntity");
	Zenith_ScriptComponent& xScripts =
		xManagerEntity.AddComponent<Zenith_ScriptComponent>();
	xScripts.AddScript<DPItemManager_Behaviour>();
	xScripts.AddScript<DPPlayerController_Behaviour>();

	const Zenith_EntityID xVillager = MakeFakeId(8001);
	const Zenith_EntityID xKeyItem  = MakeFakeId(8101);
	const Zenith_EntityID xSkelItem = MakeFakeId(8102);

	DP_Items::Internal_RegisterItemTag(xKeyItem,  DP_ItemTag::Key);
	DP_Items::Internal_RegisterItemTag(xSkelItem, DP_ItemTag::SkeletonKey);

	// 1. No held item → fails.
	DP_EXPECT(!DP_Items::TryConsumeKeyForUnlock(xVillager, DP_ItemTag::Key),
		"empty hand fails");

	// 2. Wrong key tag → fails.
	DP_Player::SetHeldItem(xVillager, xKeyItem);
	DP_EXPECT(!DP_Items::TryConsumeKeyForUnlock(xVillager, DP_ItemTag::SkeletonKey),
		"wrong-tag fails");
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager) == DP_ItemTag::Key,
		"wrong-tag does not consume");

	// 3. SkeletonKey opens any lock and PERSISTS (does not consume).
	DP_Player::RemoveHeldItem(xVillager);
	DP_Player::SetHeldItem(xVillager, xSkelItem);
	DP_EXPECT(DP_Items::TryConsumeKeyForUnlock(xVillager, DP_ItemTag::Key),
		"skeleton key opens Key lock");
	DP_EXPECT(DP_Player::GetHeldItemTag(xVillager) == DP_ItemTag::SkeletonKey,
		"skeleton key NOT consumed");

	// Cleanup
	DP_Player::RemoveHeldItem(xVillager);
	DP_Items::Internal_UnregisterItemTag(xKeyItem);
	DP_Items::Internal_UnregisterItemTag(xSkelItem);
	g_xEngine.Scenes().UnloadScene(xScene);
}

static bool Step_Unlock(int)                         { return false; }
static bool Verify_Unlock()                          { return g_bUnlockSetupRan && g_bAllPassed; }

static const Zenith_AutomatedTest g_xUnlockTest = {
	"DP_Unlock_Test", &Setup_Unlock, &Step_Unlock, &Verify_Unlock, 5
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xUnlockTest);

#endif // ZENITH_INPUT_SIMULATOR
