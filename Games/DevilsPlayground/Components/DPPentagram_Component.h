#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPPentagram_Component - win-condition altar. Each interact consumes one
 * Objective[1..5] item; collecting the victory threshold fires DP_OnVictory.
 */

#include "Components/DPInteractable_Base.h"
class DPPentagram_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
	DPPentagram_Component() = delete;
	DPPentagram_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

	void OnAwake()
	{
		DPInteractable_Base::OnAwake();
		// Pentagram runs the win-state side-table; re-init on each fresh
		// scene so a replay starts clean.
		DP_Win::Reset();
	}

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (!DP_IsObjectiveTag(eHeld)) return;

		const uint32_t uBit = DP_ObjectiveTagToBit(eHeld);
		if (DP_Win::GetCollectedObjectivesMask() & uBit) return; // already collected

		// Forward villager + pentagram into the notify call so the eventual
		// DP_OnVictory dispatch (fires only on the winning objective) carries
		// world-locatable entity context. Earlier objectives ignore the
		// extra args; only the winning placement uses them.
		DP_Win::NotifyObjectiveCollected(eHeld, xVillager, m_xParentEntity.GetEntityID());
		// Consume the held objective.
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

		// Phase-5-audit (2026-05-16): announce the objective-placed
		// milestone so the analyzer can require ObjectivePlaced as a
		// verified mechanic + the visualiser can plot each of the 5
		// deliveries as a distinct marker.
		const int iBitIdx = static_cast<int>(eHeld) - static_cast<int>(DP_ItemTag::Objective1);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnObjectivePlaced{
				xVillager,
				m_xParentEntity.GetEntityID(),
				iBitIdx });
	}
};
