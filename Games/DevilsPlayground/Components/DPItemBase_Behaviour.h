#pragma once
/**
 * DPItemBase_Behaviour - tagged pickup item.
 *
 * Each item entity carries a DP_ItemTag. On OnAwake the tag is registered
 * with DP_Items::Internal_RegisterItemTag so DP_Items::GetItemTag /
 * FindItemByTag work without per-frame component scanning. Pickup is
 * distance-based (no trigger semantics in Zenith): each frame, if the
 * possessed villager is within m_fPickupRadius, the item is picked up.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPMaterials.h"

class DPItemBase_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPItemBase_Behaviour)

	DPItemBase_Behaviour() = delete;
	DPItemBase_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		DP_Items::Internal_RegisterItemTag(m_xParentEntity.GetEntityID(), m_eTag);
		ApplyTagTint();
	}

	void OnStart() ZENITH_FINAL override
	{
		// Re-apply on OnStart so runtime-spawned items (where the model component
		// is added immediately before the script's OnAwake runs) catch up if
		// model materials weren't ready at OnAwake.
		ApplyTagTint();
	}

	void OnDestroy() ZENITH_FINAL override
	{
		DP_Items::Internal_UnregisterItemTag(m_xParentEntity.GetEntityID());
	}

	void OnUpdate(const float /*fDt*/) ZENITH_FINAL override
	{
		// Distance-based pickup: when the possessed villager wanders within
		// m_fPickupRadius of an item that has no holder, the villager picks
		// it up. Source-game semantics: an item already in the villager's
		// hand is dropped (swap), but for skeleton-grade we just refuse the
		// new pickup if held — the swap behaviour can be added in Wave 4.
		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		if (!xVillager.IsValid()) return;
		if (DP_Player::GetHeldItemTag(xVillager) != DP_ItemTag::None) return;

		Zenith_Maths::Vector3 xMyPos = DP_Items::GetItemWorldPos(m_xParentEntity.GetEntityID());

		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid()) return;
		if (!xV.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_Maths::Vector3 xVPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);

		const float fDx = xMyPos.x - xVPos.x;
		const float fDz = xMyPos.z - xVPos.z;
		if (fDx * fDx + fDz * fDz > m_fPickupRadius * m_fPickupRadius) return;

		DP_Player::SetHeldItem(xVillager, m_xParentEntity.GetEntityID());
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnItemPickedUp{ xVillager, m_xParentEntity.GetEntityID() });
	}

	DP_ItemTag GetTag() const { return m_eTag; }
	void SetTag(DP_ItemTag eTag)
	{
		// Re-register if the tag changed after OnAwake (e.g. editor setter).
		if (m_eTag == eTag) return;
		DP_Items::Internal_UnregisterItemTag(m_xParentEntity.GetEntityID());
		m_eTag = eTag;
		DP_Items::Internal_RegisterItemTag(m_xParentEntity.GetEntityID(), m_eTag);
		// Re-tint with the new tag's colour. Reset the guard so the materials
		// are re-set even if a previous tag's tint was already applied.
		m_bTintApplied = false;
		ApplyTagTint();
	}

private:
	// Map the item's tag to a tint colour and overwrite every material slot on
	// the model with the tinted variant. No-op if the entity has no model
	// component, no model instance, or the tag is None. Idempotent — once the
	// tint is applied, m_bTintApplied prevents OnStart from re-tinting (and
	// thus re-tinting an already-tinted material into a doubly-tinted variant).
	void ApplyTagTint()
	{
		if (m_bTintApplied) return;
		if (m_eTag == DP_ItemTag::None) return;
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>()) return;
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		Flux_ModelInstance* pxModelInstance = xModel.GetModelInstance();
		if (!pxModelInstance) return;
		const uint32_t uNumMaterials = pxModelInstance->GetNumMaterials();
		if (uNumMaterials == 0) return;

		Zenith_Maths::Vector3 xRgb(1.0f, 1.0f, 1.0f);
		const char* szLabel = "Tint";
		switch (m_eTag)
		{
			case DP_ItemTag::Iron:        xRgb = Zenith_Maths::Vector3(0.5f, 0.5f, 0.55f); szLabel = "TintIron";        break;
			case DP_ItemTag::Key:         xRgb = Zenith_Maths::Vector3(1.0f, 0.85f, 0.2f); szLabel = "TintKey";         break;
			case DP_ItemTag::SkeletonKey: xRgb = Zenith_Maths::Vector3(0.7f, 0.3f, 0.9f);  szLabel = "TintSkeletonKey"; break;
			case DP_ItemTag::Objective1:
			case DP_ItemTag::Objective2:
			case DP_ItemTag::Objective3:
			case DP_ItemTag::Objective4:
			case DP_ItemTag::Objective5:  xRgb = Zenith_Maths::Vector3(0.95f, 0.15f, 0.15f); szLabel = "TintObjective"; break;
			default: return;
		}

		for (uint32_t u = 0; u < uNumMaterials; ++u)
		{
			Zenith_MaterialAsset* pxBase = pxModelInstance->GetMaterial(u);
			Zenith_MaterialAsset* pxTint = DPMaterials::GetOrCreateColouredVariant(pxBase, xRgb, szLabel);
			if (pxTint) pxModelInstance->SetMaterial(u, pxTint);
		}
		m_bTintApplied = true;
	}

	DP_ItemTag m_eTag = DP_ItemTag::None;
	float      m_fPickupRadius = 1.5f;
	bool       m_bTintApplied = false;
};
