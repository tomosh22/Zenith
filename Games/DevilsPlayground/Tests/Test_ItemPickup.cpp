#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"

// ============================================================================
// ItemPickup_Test
//
// End-to-end pickup loop:
//   1. Load GameLevel.
//   2. Wait for DPItemManager_Behaviour::OnStart to spawn the 15 items.
//   3. Pick a villager and the first spawned item.
//   4. Possess the villager + teleport it onto the item (within pickup radius).
//   5. Wait one frame for DPItemBase_Behaviour::OnUpdate to detect proximity.
//   6. Verify DP_Player::GetHeldItemTag(villager) matches the item's tag.
//
// The pickup-detection path runs entirely off the public DP_Player /
// DP_Items interfaces, so this exercises the full game loop minus the
// input-simulator path (we drive possession + teleport directly).
// ============================================================================

namespace
{
	enum Phase : int {
		kIP_Start,
		kIP_WaitScene,
		kIP_WaitItems,
		kIP_PickAndTeleport,
		kIP_WaitPickup,
		kIP_Verify,
		kIP_Done
	};

	int             g_iIPPhase = kIP_Start;
	Zenith_EntityID g_xIPVillager;
	Zenith_EntityID g_xIPItem;
	DP_ItemTag      g_eIPItemTag      = DP_ItemTag::None;
	DP_ItemTag      g_eIPHeldAfter    = DP_ItemTag::None;
	bool            g_bIPPassed       = false;
}

static void Setup_ItemPickup()
{
	g_iIPPhase     = kIP_Start;
	g_xIPVillager  = INVALID_ENTITY_ID;
	g_xIPItem      = INVALID_ENTITY_ID;
	g_eIPItemTag   = DP_ItemTag::None;
	g_eIPHeldAfter = DP_ItemTag::None;
	g_bIPPassed    = false;
}

static bool Step_ItemPickup(int iFrame)
{
	switch (g_iIPPhase)
	{
	case kIP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iIPPhase = kIP_WaitScene;
		return true;

	case kIP_WaitScene:
	{
		// Wait for the villager scripts to appear (proxy for "scene fully
		// loaded and OnAwake hooks fired").
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&) { xFound = xId; });
		if (xFound.IsValid())
		{
			g_xIPVillager = xFound;
			g_iIPPhase = kIP_WaitItems;
		}
		else if (iFrame > 60)
		{
			g_iIPPhase = kIP_Done; // bail
		}
		return true;
	}

	case kIP_WaitItems:
	{
		// Items are spawned by DPItemManager_Behaviour::OnStart, which fires
		// after all OnAwake hooks. By the time the manager spawns them and
		// AddScript<DPItemBase_Behaviour> calls OnAwake -> Internal_RegisterItemTag,
		// we should see DPItemBase_Behaviour scripts in the scene.
		Zenith_EntityID xFirstItem;
		DP_ItemTag      eFirstTag = DP_ItemTag::None;
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&xFirstItem, &eFirstTag](Zenith_EntityID xId, DPItemBase_Behaviour& xItem)
			{
				if (!xFirstItem.IsValid())
				{
					xFirstItem = xId;
					eFirstTag  = xItem.GetTag();
				}
			});
		if (xFirstItem.IsValid() && eFirstTag != DP_ItemTag::None)
		{
			g_xIPItem    = xFirstItem;
			g_eIPItemTag = eFirstTag;
			g_iIPPhase   = kIP_PickAndTeleport;
		}
		else if (iFrame > 90)
		{
			g_iIPPhase = kIP_Done; // bail
		}
		return true;
	}

	case kIP_PickAndTeleport:
	{
		// Possess the villager and teleport it onto the item. The villager's
		// Jolt body fights us if we just SetPosition on the transform without
		// disabling the body, so we accept that the position may snap-back —
		// but the pickup check runs in DPItemBase::OnUpdate so a single frame
		// at the item's position is enough.
		DP_Player::SetPossessedVillager(g_xIPVillager);

		// Teleport the villager to the item's position via direct transform
		// SetPosition. The collider's body position is updated separately
		// inside the engine, so for skeleton-grade we just do the raw move.
		Zenith_Maths::Vector3 xItemPos = DP_Items::GetItemWorldPos(g_xIPItem);
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xIPVillager);
		if (pxScene != nullptr)
		{
			Zenith_Entity xV = pxScene->TryGetEntity(g_xIPVillager);
			if (xV.IsValid() && xV.HasComponent<Zenith_TransformComponent>())
			{
				xV.GetComponent<Zenith_TransformComponent>().SetPosition(xItemPos);
			}
		}
		g_iIPPhase = kIP_WaitPickup;
		return true;
	}

	case kIP_WaitPickup:
		// Give DPItemBase_Behaviour::OnUpdate a frame to detect proximity and
		// fire DP_Player::SetHeldItem.
		g_iIPPhase = kIP_Verify;
		return true;

	case kIP_Verify:
	{
		g_eIPHeldAfter = DP_Player::GetHeldItemTag(g_xIPVillager);
		g_bIPPassed = (g_eIPHeldAfter != DP_ItemTag::None)
		           && (g_eIPHeldAfter == g_eIPItemTag);
		g_iIPPhase = kIP_Done;
		return false;
	}

	case kIP_Done:
	default:
		return false;
	}
}

static bool Verify_ItemPickup()
{
	return g_bIPPassed
	    && g_xIPVillager.IsValid()
	    && g_xIPItem.IsValid()
	    && g_eIPItemTag != DP_ItemTag::None;
}

static const Zenith_AutomatedTest g_xItemPickupTest = {
	"ItemPickup_Test",
	&Setup_ItemPickup,
	&Step_ItemPickup,
	&Verify_ItemPickup,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xItemPickupTest);

#endif // ZENITH_INPUT_SIMULATOR
