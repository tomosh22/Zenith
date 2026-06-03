#include "Zenith.h"
#include "DP_Items.h"
#include "DP_Player.h"

#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include "../Components/DPItemManager_Behaviour.h"
#include "../Components/DPItemBase_Behaviour.h"

namespace DP_Items
{
	DP_ItemTag GetItemTag(Zenith_EntityID xItem)
	{
		// Forward to the active scene's DPItemManager. If no manager
		// is loaded (between-scenes, or non-DP-game scene), tag is
		// trivially None.
		const DPItemManager_Behaviour* pxMgr = DPItemManager_Behaviour::Instance();
		if (pxMgr == nullptr) return DP_ItemTag::None;
		return pxMgr->GetItemTag(xItem);
	}

	Vec3 GetItemWorldPos(Zenith_EntityID xItem)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xItem);
		if (pxScene == nullptr) return Vec3(0.0f);
		Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
		if (!xEnt.IsValid()) return Vec3(0.0f);
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return Vec3(0.0f);
		Vec3 xPos;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	bool TryConsumeKeyForUnlock(Zenith_EntityID xVillager, DP_ItemTag eRequiredKey)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Items::TryConsumeKeyForUnlock must be called from main thread");
		// SkeletonKey is a master key — opens any lock.
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (eHeld == DP_ItemTag::None) return false;
		if (eHeld != eRequiredKey && eHeld != DP_ItemTag::SkeletonKey) return false;

		// SkeletonKey persists across uses; a regular Key is consumed.
		if (eHeld != DP_ItemTag::SkeletonKey)
		{
			Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
			DP_Player::RemoveHeldItem(xVillager);
			if (xItem.IsValid())
			{
				Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xItem);
				if (pxScene != nullptr)
				{
					Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
					if (xEnt.IsValid())
					{
						xEnt.Destroy();
					}
				}
			}
		}
		return true;
	}

	// SourceBugFixed: GameJam0 AItemManager::FindItemByType derefs on miss.
	// This port returns INVALID_ENTITY_ID; callers null-check.
	Zenith_EntityID FindItemByTag(DP_ItemTag eTag)
	{
		const DPItemManager_Behaviour* pxMgr = DPItemManager_Behaviour::Instance();
		if (pxMgr == nullptr) return INVALID_ENTITY_ID;
		return pxMgr->FindItemByTag(eTag);
	}

	// Internal: B3 calls these from DPItemBase_Behaviour OnAwake/OnDestroy.
	void Internal_RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Items::Internal_RegisterItemTag must be called from main thread");
		DPItemManager_Behaviour* pxMgr = DPItemManager_Behaviour::Instance();
		if (pxMgr == nullptr) return;
		pxMgr->RegisterItemTag(xItem, eTag);
	}
	void Internal_UnregisterItemTag(Zenith_EntityID xItem)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Items::Internal_UnregisterItemTag must be called from main thread");
		DPItemManager_Behaviour* pxMgr = DPItemManager_Behaviour::Instance();
		if (pxMgr == nullptr) return;
		pxMgr->UnregisterItemTag(xItem);
	}

	// Cross-behaviour forwarder: arm the item's post-drop pickup cooldown so
	// DPItemBase::OnUpdate doesn't immediately re-pick-up from the villager's
	// foot position. Moved here from DPPlayerController_Behaviour::HandleDropItem
	// so the controller header no longer includes DPItemBase_Behaviour.h.
	void BeginPostDropCooldownForItem(Zenith_EntityID xItem)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_Items::BeginPostDropCooldownForItem must be called from main thread");
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xItem);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ScriptComponent>()) return;
		DPItemBase_Behaviour* pxItem =
			xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPItemBase_Behaviour>();
		if (pxItem) pxItem->BeginPostDropCooldown();
	}
}
