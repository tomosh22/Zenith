#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
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
// Test_P1Drop_GoesToGroundAtBodyPosition (MVP-1.4.4 + 1.4.5)
//
// Verifies the G-key drop verb: after pressing G while possessing a
// villager that has a held item, the item must:
//   1. Be removed from DP_Player's held-item side-table for that
//      villager (GetHeldItemTag returns None).
//   2. Have its world transform repositioned to the villager's foot
//      position (i.e., the villager's transform origin, which is at
//      the feet by authoring convention).
//   3. Briefly refuse re-pickup so the player can step away without
//      the OnUpdate proximity check immediately re-engaging.
//
// Procedure:
//   1. Load GameLevel (has the DPItemManager-spawned items).
//   2. Find the possessed villager + an existing item near it.
//      The fastest way is to wait for the proximity-pickup to fire
//      naturally: possess a villager that's authored close to an
//      objective spawner, tick a few frames, observe held tag flips
//      from None -> non-None.
//   3. Snapshot the villager's foot position + the held item handle.
//   4. SimulateKeyPress(G).
//   5. Tick one frame so the controller's HandleDropItem fires.
//   6. Assert:
//      - GetHeldItemTag(villager) == None
//      - Item's transform position equals the snapshot foot position
//      - Item's m_fPostDropCooldownSec > 0 (re-pickup guard armed)
// ============================================================================

namespace
{
	enum Phase : int { kDR_Start, kDR_WaitScene, kDR_PossessAndArm,
	                   kDR_WaitForPickup, kDR_SnapshotAndDrop,
	                   kDR_VerifyDrop, kDR_Done };

	int                     g_iPhase = kDR_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xItem;
	Zenith_Maths::Vector3   g_xFootPos(0.0f);
	Zenith_Maths::Vector3   g_xItemPosAfterDrop(0.0f);
	DP_ItemTag              g_eTagBeforeDrop = DP_ItemTag::None;
	DP_ItemTag              g_eTagAfterDrop = DP_ItemTag::None;
	float                   g_fCooldownAfterDrop = -1.0f;
	int                     g_iWaitFrames = 0;

	// Pickup is distance-based at ~1.5 m; the test simply waits long
	// enough for DPItemBase::OnUpdate to fire once with the villager
	// in range. 60 frames (~1 s) is plenty of margin.
	constexpr int kPICKUP_WAIT_FRAMES = 60;

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

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	DPItemBase_Behaviour* GetItemBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPItemBase_Behaviour>();
	}

	// Find a villager that has at least one DPItem within 5 m -- if
	// we possess that villager and wait, the proximity pickup will
	// fire naturally. Without this we might possess a villager too
	// far from any items and wait forever.
	bool PickVillagerNearItem(Zenith_EntityID& xVillagerOut)
	{
		Zenith_Vector<Zenith_EntityID> axVillagers;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVillagers](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				axVillagers.PushBack(xId);
			});
		Zenith_Vector<Zenith_Maths::Vector3> axItemPositions;
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&axItemPositions](Zenith_EntityID xId, DPItemBase_Behaviour&)
			{
				Zenith_Maths::Vector3 xPos;
				if (TryGetEntityPos(xId, xPos)) axItemPositions.PushBack(xPos);
			});
		float fBestDist = 1e30f;
		for (uint32_t i = 0; i < axVillagers.GetSize(); ++i)
		{
			Zenith_Maths::Vector3 xVPos;
			if (!TryGetEntityPos(axVillagers.Get(i), xVPos)) continue;
			for (uint32_t j = 0; j < axItemPositions.GetSize(); ++j)
			{
				const float fD = HorizontalDistance(xVPos, axItemPositions.Get(j));
				if (fD < fBestDist)
				{
					fBestDist = fD;
					xVillagerOut = axVillagers.Get(i);
				}
			}
		}
		return xVillagerOut.IsValid() && fBestDist < 10.0f;
	}
}

static void Setup_P1Drop()
{
	g_iPhase = kDR_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xItem = INVALID_ENTITY_ID;
	g_xFootPos = Zenith_Maths::Vector3(0.0f);
	g_xItemPosAfterDrop = Zenith_Maths::Vector3(0.0f);
	g_eTagBeforeDrop = DP_ItemTag::None;
	g_eTagAfterDrop = DP_ItemTag::None;
	g_fCooldownAfterDrop = -1.0f;
	g_iWaitFrames = 0;
}

static bool Step_P1Drop(int iFrame)
{
	switch (g_iPhase)
	{
	case kDR_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kDR_WaitScene;
		return true;

	case kDR_WaitScene:
	{
		// Wait until BOTH a villager and at least one DPItemBase exist
		// in the active scene. DPItemManager spawns items in its OnStart,
		// which runs after the scene's first frame of OnAwake; items
		// usually exist by frame ~3, but allow ~2 seconds of slack.
		int iVillagerCount = 0;
		int iItemCount = 0;
		Zenith_EntityID xFoundVillager;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&iVillagerCount, &xFoundVillager]
			(Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				++iVillagerCount;
				if (!xFoundVillager.IsValid()) xFoundVillager = xId;
			});
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&iItemCount](Zenith_EntityID, DPItemBase_Behaviour&) { ++iItemCount; });
		if (iFrame == 30)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P1Drop kDR_WaitScene frame=%d villagers=%d items=%d",
				iFrame, iVillagerCount, iItemCount);
		}
		if (iVillagerCount > 0 && iItemCount > 0)
		{
			g_xVillager = xFoundVillager;
			g_iPhase = kDR_PossessAndArm;
		}
		else if (iFrame > 120)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P1Drop bail: villagers=%d items=%d at frame %d",
				iVillagerCount, iItemCount, iFrame);
			g_iPhase = kDR_Done;
		}
		return true;
	}

	case kDR_PossessAndArm:
	{
		DP_Player::SetPossessedVillager(g_xVillager);
		// Bypass the proximity-pickup OnUpdate path. The drop verb
		// is the system-under-test; getting an item onto a villager
		// is just setup. Find any DPItemBase, hand it to the villager
		// via DP_Player's public side-table API.
		Zenith_EntityID xTargetItem;
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&xTargetItem](Zenith_EntityID xId, DPItemBase_Behaviour&)
			{
				if (!xTargetItem.IsValid()) xTargetItem = xId;
			});
		if (!xTargetItem.IsValid())
		{
			Zenith_Log(LOG_CATEGORY_AI, "P1Drop: no DPItemBase entity found in scene");
			g_iPhase = kDR_Done;
			return false;
		}
		DP_Player::SetHeldItem(g_xVillager, xTargetItem);
		// Cache the handle directly (GetHeldItemTag may be None if the
		// item's tag isn't registered with DP_Items yet -- depends on
		// authoring order -- but the held-entity handle is enough for
		// the drop assertion).
		g_xItem = xTargetItem;
		g_eTagBeforeDrop = DP_Player::GetHeldItemTag(g_xVillager);
		g_iWaitFrames = 0;
		g_iPhase = kDR_WaitForPickup;
		return true;
	}

	case kDR_WaitForPickup:
	{
		++g_iWaitFrames;
		// Confirm the held-entity side-table actually holds the item
		// we just set. The tag may legitimately be None (untagged
		// asset), so we check the ENTITY id, not the tag.
		const Zenith_EntityID xHeld = DP_Player::GetHeldItemEntity(g_xVillager);
		if (xHeld.m_uIndex == g_xItem.m_uIndex
			&& xHeld.m_uGeneration == g_xItem.m_uGeneration)
		{
			g_iPhase = kDR_SnapshotAndDrop;
			return true;
		}
		if (g_iWaitFrames >= kPICKUP_WAIT_FRAMES)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P1Drop: held entity never matched after SetHeldItem (held=(%u/%u) expected=(%u/%u))",
				xHeld.m_uIndex, xHeld.m_uGeneration,
				g_xItem.m_uIndex, g_xItem.m_uGeneration);
			g_iPhase = kDR_Done;
		}
		return true;
	}

	case kDR_SnapshotAndDrop:
	{
		// Snapshot the villager's foot position BEFORE the drop frame.
		// HandleDropItem reads the villager's transform AT drop time
		// and writes the same value to the item, so the assertion
		// becomes "item.pos == snapshot foot pos".
		TryGetEntityPos(g_xVillager, g_xFootPos);
		// Edge-press G via SimulatorKeyPress (single-frame pulse, the
		// auto-release queue clears it on the next StepFrame).
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_G);
		g_iPhase = kDR_VerifyDrop;
		return true;
	}

	case kDR_VerifyDrop:
	{
		g_eTagAfterDrop = DP_Player::GetHeldItemTag(g_xVillager);
		TryGetEntityPos(g_xItem, g_xItemPosAfterDrop);
		if (DPItemBase_Behaviour* pxItemBeh = GetItemBehaviour(g_xItem))
		{
			g_fCooldownAfterDrop = pxItemBeh->GetPostDropCooldownForTest();
		}
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Drop: tagBefore=%d tagAfter=%d foot=(%.2f,%.2f,%.2f) itemAfter=(%.2f,%.2f,%.2f) cooldown=%.3f",
			(int)g_eTagBeforeDrop, (int)g_eTagAfterDrop,
			g_xFootPos.x, g_xFootPos.y, g_xFootPos.z,
			g_xItemPosAfterDrop.x, g_xItemPosAfterDrop.y, g_xItemPosAfterDrop.z,
			g_fCooldownAfterDrop);
		g_iPhase = kDR_Done;
		return false;
	}

	case kDR_Done:
	default:
		return false;
	}
}

static bool Verify_P1Drop()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Drop: no villager found near any item");
		return false;
	}
	if (!g_xItem.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Drop: held item handle never captured");
		return false;
	}
	// "Drop cleared held state" check uses the entity handle (which
	// the drop path explicitly clears via DP_Player::RemoveHeldItem),
	// not the tag (which may legitimately be None on untagged items).
	const Zenith_EntityID xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillager);
	if (xHeldAfter.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Drop: held entity after G is (%u/%u) (expected INVALID) -- drop didn't clear held state",
			xHeldAfter.m_uIndex, xHeldAfter.m_uGeneration);
		return false;
	}
	const float fDX = g_xItemPosAfterDrop.x - g_xFootPos.x;
	const float fDZ = g_xItemPosAfterDrop.z - g_xFootPos.z;
	const float fDistSq = fDX * fDX + fDZ * fDZ;
	if (fDistSq > 0.01f * 0.01f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Drop: item ended at (%.2f,%.2f,%.2f), expected foot pos (%.2f,%.2f,%.2f)",
			g_xItemPosAfterDrop.x, g_xItemPosAfterDrop.y, g_xItemPosAfterDrop.z,
			g_xFootPos.x, g_xFootPos.y, g_xFootPos.z);
		return false;
	}
	if (g_fCooldownAfterDrop <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Drop: post-drop pickup cooldown is %.3f (expected > 0) -- item would immediately re-pick-up",
			g_fCooldownAfterDrop);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1DropTest = {
	"Test_P1Drop_GoesToGroundAtBodyPosition",
	&Setup_P1Drop,
	&Step_P1Drop,
	&Verify_P1Drop,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1DropTest);

#endif // ZENITH_INPUT_SIMULATOR
