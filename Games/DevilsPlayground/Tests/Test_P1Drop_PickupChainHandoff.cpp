#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Drop_PickupChainHandoff (MVP-1.4.6)
//
// `Test_P1Drop_GoesToGroundAtBodyPosition` proves the drop verb clears
// the dropping villager's held slot + places the item at the foot
// position + arms the post-drop cooldown. It does NOT prove that
// ANOTHER villager can then pick up the dropped item.
//
// This test pins the full "demon-hop chain" item-transfer scenario:
// possess villager A holding an item -> drop -> possess villager B ->
// B walks (or teleports) onto the dropped item -> B picks it up.
//
// What this exercises that no existing test covers:
//   * The post-drop cooldown correctly RELEASES the item for proximity
//     pickup once the timer expires (the existing drop test only proves
//     the cooldown ARMS, not that it disarms).
//   * `DP_Player::GetHeldItemTag` and the held-item side-table are
//     keyed PER-VILLAGER -- after drop, A's slot is empty AND B's slot
//     starts empty AND the proximity-pickup code path can fill B's slot
//     without leaking A's stale state.
//   * `DPItemBase_Behaviour::OnUpdate`'s pickup check correctly reads
//     the CURRENTLY possessed villager (B), not the most-recently
//     possessed (A who just dropped it).
//
// Regression scenarios this catches:
//   * Post-drop cooldown that never resets -> item is unpickable
//     forever; the test's pickup phase times out.
//   * A regression where `DP_Player::RemoveHeldItem` only nulls the
//     ENTITY but not the TAG (or vice versa) -- B's pickup code would
//     refuse because A is still flagged as holding something.
//   * A regression where the item's pickup code reads
//     `GetPossessedVillager` from a stale cache -> picks up onto A.
//
// Procedure:
//   1. Load GameLevel; find two villagers + first available DPItem.
//   2. SetPossessedVillager(A); SetHeldItem(A, item).
//   3. SimulateKeyPress(G) -> drop. Item lands at A's foot, post-drop
//      cooldown armed.
//   4. Teleport A 100 m away from the drop position. A is still the
//      possessed villager during the cooldown wait, and once the
//      cooldown expires the item's OnUpdate WOULD re-pick-up onto
//      A (A is right next to it after the drop). Moving A out of
//      pickup radius means the chain-handoff is the ONLY path by
//      which the item gets picked up again, which is exactly the
//      scenario this test pins.
//   5. Tick ~40 frames (~0.67s, comfortably past the 0.5s cooldown).
//   6. SetPossessedVillager(B). Anchor moves to B; A keeps its
//      (cleared) held-item slot.
//   7. Teleport B to the dropped-item position via SetPosition (same
//      pattern as `Test_ItemPickup`).
//   8. Tick 2 frames so DPItemBase::OnUpdate fires with B in range.
//   9. Verify:
//        GetHeldItemEntity(A) == INVALID (drop cleared A's slot)
//        GetHeldItemEntity(B) == item   (B picked it up cleanly)
//        GetHeldItemTag(B)    == itemTag (sanity: side-table coherent)
// ============================================================================

namespace
{
	enum Phase : int { kCH_Start, kCH_WaitScene, kCH_PossessAndArm,
	                   kCH_DropAndWaitCooldown, kCH_SwitchAndTeleport,
	                   kCH_WaitPickup, kCH_Verify, kCH_Done };

	int                     g_iPhase = kCH_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xItem;
	DP_ItemTag              g_eItemTag = DP_ItemTag::None;
	int                     g_iCooldownTicks = 0;
	int                     g_iPickupTicks = 0;
	Zenith_EntityID         g_xHeldByAFinal;
	Zenith_EntityID         g_xHeldByBFinal;
	DP_ItemTag              g_eHeldTagByBFinal = DP_ItemTag::None;
	Zenith_Maths::Vector3   g_xItemPosAtDropTime(0.0f);

	// 40 frames at fixed-dt 0.01666 = ~0.67 s, comfortably past the
	// 0.5 s post-drop cooldown. Going much higher than that risks the
	// life timer ticking us close to death (irrelevant to the chain
	// semantic).
	constexpr int kCOOLDOWN_TICKS  = 40;
	// 2 frames is plenty for DPItemBase::OnUpdate to fire once with B
	// in range -- one frame for B's possession state to flip on
	// (m_bIsPossessed transition), one for the actual pickup check.
	constexpr int kPICKUP_TICKS    = 3;

	void PickTwoVillagersAndFirstItem(Zenith_EntityID& xA,
	                                  Zenith_EntityID& xB,
	                                  Zenith_EntityID& xItem,
	                                  DP_ItemTag& eTag)
	{
		// Picking the first two villagers iterated is fine -- the chain
		// test does NOT depend on them being close (we teleport B to the
		// dropped item position) or in any particular spatial layout.
		Zenith_Vector<Zenith_EntityID> axVillagers;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVillagers](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				axVillagers.PushBack(xId);
			});
		if (axVillagers.GetSize() >= 2)
		{
			xA = axVillagers.Get(0);
			xB = axVillagers.Get(1);
		}

		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&xItem, &eTag](Zenith_EntityID xId, DPItemBase_Behaviour& xBeh)
			{
				if (!xItem.IsValid())
				{
					xItem = xId;
					eTag  = xBeh.GetTag();
				}
			});
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

	void TeleportTo(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	}
}

static void Setup_P1DropChain()
{
	g_iPhase = kCH_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xItem = INVALID_ENTITY_ID;
	g_eItemTag = DP_ItemTag::None;
	g_iCooldownTicks = 0;
	g_iPickupTicks = 0;
	g_xHeldByAFinal = INVALID_ENTITY_ID;
	g_xHeldByBFinal = INVALID_ENTITY_ID;
	g_eHeldTagByBFinal = DP_ItemTag::None;
	g_xItemPosAtDropTime = Zenith_Maths::Vector3(0.0f);
}

static bool Step_P1DropChain(int iFrame)
{
	switch (g_iPhase)
	{
	case kCH_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kCH_WaitScene;
		return true;

	case kCH_WaitScene:
	{
		// Items spawn in DPItemManager::OnStart, which fires after all
		// OnAwake hooks. Wait until both villagers AND at least one
		// item exist before kicking off the chain.
		PickTwoVillagersAndFirstItem(g_xA, g_xB, g_xItem, g_eItemTag);
		if (g_xA.IsValid() && g_xB.IsValid() && g_xItem.IsValid())
		{
			g_iPhase = kCH_PossessAndArm;
		}
		else if (iFrame > 120)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P1DropChain bail at frame %d: A.valid=%d B.valid=%d item.valid=%d",
				iFrame,
				(int)g_xA.IsValid(), (int)g_xB.IsValid(),
				(int)g_xItem.IsValid());
			g_iPhase = kCH_Done;
		}
		return true;
	}

	case kCH_PossessAndArm:
		// Possess A; hand A the item via the public side-table (we
		// don't care about exercising the pickup path on A -- only the
		// chain handoff to B is the system under test here).
		DP_Player::SetPossessedVillager(g_xA);
		DP_Player::SetHeldItem(g_xA, g_xItem);
		g_iPhase = kCH_DropAndWaitCooldown;
		g_iCooldownTicks = 0;
		return true;

	case kCH_DropAndWaitCooldown:
	{
		// On the FIRST entry to this phase, fire the drop verb. The
		// remaining ticks just wait out the post-drop cooldown so the
		// item is re-pickup-able when we switch possession.
		if (g_iCooldownTicks == 0)
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
			// One frame from now HandleDropItem will run and the item
			// lands at A's foot. Snapshot AFTER the next frame so the
			// item position reflects the post-drop placement.
		}
		else if (g_iCooldownTicks == 1)
		{
			// Drop frame has completed -- snapshot the item's position
			// (where A dropped it).
			TryGetEntityPos(g_xItem, g_xItemPosAtDropTime);
			// CRITICAL: A is still the possessed villager and sitting
			// right on top of the dropped item. Once the post-drop
			// cooldown expires, DPItemBase::OnUpdate would re-pick-up
			// onto A (distance ~0 < pickup radius). That would defeat
			// the test before B ever gets a turn. Teleport A far away
			// so A's distance check fails for the rest of the wait.
			Zenith_Maths::Vector3 xFar(g_xItemPosAtDropTime.x + 100.0f,
			                            g_xItemPosAtDropTime.y,
			                            g_xItemPosAtDropTime.z);
			TeleportTo(g_xA, xFar);
		}
		++g_iCooldownTicks;
		if (g_iCooldownTicks >= kCOOLDOWN_TICKS)
		{
			g_iPhase = kCH_SwitchAndTeleport;
		}
		return true;
	}

	case kCH_SwitchAndTeleport:
		// System path -- no scent bump, no cooldown gate. B becomes
		// the new possessed villager; anchor moves to B (irrelevant
		// for this test).
		DP_Player::SetPossessedVillager(g_xB);
		// Teleport B onto the dropped-item position. Same pattern as
		// Test_ItemPickup: the Jolt body may fight us next frame, but
		// the DPItemBase::OnUpdate proximity check runs in the same
		// frame we teleport so a single frame at the target position
		// suffices.
		TeleportTo(g_xB, g_xItemPosAtDropTime);
		g_iPickupTicks = 0;
		g_iPhase = kCH_WaitPickup;
		return true;

	case kCH_WaitPickup:
		++g_iPickupTicks;
		if (g_iPickupTicks >= kPICKUP_TICKS)
		{
			g_iPhase = kCH_Verify;
		}
		return true;

	case kCH_Verify:
	{
		g_xHeldByAFinal = DP_Player::GetHeldItemEntity(g_xA);
		g_xHeldByBFinal = DP_Player::GetHeldItemEntity(g_xB);
		g_eHeldTagByBFinal = DP_Player::GetHeldItemTag(g_xB);
		std::printf("[P1DropChain] heldByA=(%u/%u) heldByB=(%u/%u) heldTagB=%d itemExpected=(%u/%u) tag=%d itemPos=(%.2f,%.2f,%.2f)\n",
			g_xHeldByAFinal.m_uIndex, g_xHeldByAFinal.m_uGeneration,
			g_xHeldByBFinal.m_uIndex, g_xHeldByBFinal.m_uGeneration,
			(int)g_eHeldTagByBFinal,
			g_xItem.m_uIndex, g_xItem.m_uGeneration,
			(int)g_eItemTag,
			g_xItemPosAtDropTime.x, g_xItemPosAtDropTime.y, g_xItemPosAtDropTime.z);
		std::fflush(stdout);
		g_iPhase = kCH_Done;
		return false;
	}

	case kCH_Done:
	default:
		return false;
	}
}

static bool Verify_P1DropChain()
{
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xItem.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DropChain: setup failed (A.valid=%d B.valid=%d item.valid=%d)",
			(int)g_xA.IsValid(), (int)g_xB.IsValid(), (int)g_xItem.IsValid());
		return false;
	}
	if (g_xHeldByAFinal.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DropChain: A still flagged as holding (%u/%u) after drop -- drop verb didn't clear A's slot",
			g_xHeldByAFinal.m_uIndex, g_xHeldByAFinal.m_uGeneration);
		return false;
	}
	if (!g_xHeldByBFinal.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DropChain: B never picked up the dropped item. Possible causes: post-drop cooldown never released (still > 0 after %.2fs); pickup proximity check used wrong villager; B teleport didn't take effect.",
			kCOOLDOWN_TICKS * 0.01666f);
		return false;
	}
	if (g_xHeldByBFinal.m_uIndex != g_xItem.m_uIndex
		|| g_xHeldByBFinal.m_uGeneration != g_xItem.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DropChain: B picked up entity (%u/%u) but expected (%u/%u) -- pickup grabbed the wrong item",
			g_xHeldByBFinal.m_uIndex, g_xHeldByBFinal.m_uGeneration,
			g_xItem.m_uIndex, g_xItem.m_uGeneration);
		return false;
	}
	// Tag-coherency sanity: the side-table's TAG and ENTITY columns
	// should agree. If GetHeldItemTag returns None but GetHeldItemEntity
	// returns a valid handle, the SetHeldItem path forgot to read the
	// tag (or the item's tag is genuinely None).
	if (g_eHeldTagByBFinal != g_eItemTag)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1DropChain: B's held tag=%d but item's tag=%d -- side-table tag/entity columns out of sync",
			(int)g_eHeldTagByBFinal, (int)g_eItemTag);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1DropChainTest = {
	"Test_P1Drop_PickupChainHandoff",
	&Setup_P1DropChain,
	&Step_P1DropChain,
	&Verify_P1DropChain,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1DropChainTest);

#endif // ZENITH_INPUT_SIMULATOR
