#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DevilsPlayground_Tags.h"
#include "DPCommonTypes.h"

// ============================================================================
// DP_Items — published by B3.
// Tag side-table (EntityID -> DP_ItemTag) lives on DPItemManager_Component;
// these functions forward through DPItemManager_Component::Instance().
// ============================================================================
namespace DP_Items
{
	DP_ItemTag GetItemTag(Zenith_EntityID xItem);
	Vec3 GetItemWorldPos(Zenith_EntityID xItem);

	bool TryConsumeKeyForUnlock(Zenith_EntityID xVillager, DP_ItemTag eRequiredKey);

	Zenith_EntityID FindItemByTag(DP_ItemTag eTag);

	// Internal — called by DPItemBase_Component::OnAwake/OnDestroy to maintain
	// the item-tag side-table that backs GetItemTag / FindItemByTag.
	void Internal_RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag);
	void Internal_UnregisterItemTag(Zenith_EntityID xItem);

	// Cross-behaviour forwarder: begin an item's post-drop pickup cooldown.
	// Mediates DPPlayerController_Component <-> DPItemBase_Component so the
	// controller header doesn't include the item header (cross-behaviour
	// rule). No-op when the item can't be resolved to a DPItemBase_Component.
	void BeginPostDropCooldownForItem(Zenith_EntityID xItem);
}
