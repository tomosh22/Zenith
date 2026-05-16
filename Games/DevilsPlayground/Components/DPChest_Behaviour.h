#pragma once
/**
 * DPChest_Behaviour - simple openable container. On interact, the lid child
 * pivots outward; loot dispensing is gameplay-tier and ships in Wave 4.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "Source/DP_Tuning.h"

class DPChest_Behaviour ZENITH_FINAL : public DPInteractable_Behaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPChest_Behaviour)

	DPChest_Behaviour() = delete;
	DPChest_Behaviour(Zenith_Entity& xParentEntity)
		: DPInteractable_Behaviour(xParentEntity)
	{}

	void OnAwake() ZENITH_FINAL override
	{
		DPInteractable_Behaviour::OnAwake();
		// MVP-0.1.4: tuning read. Chest had pre-existing drift (0.5f hardcoded
		// vs Tuning.json's 0.8s); the post-migration value matches the config.
		m_fOpenDuration = DP_Tuning::Get<float>("interactables.chest_open_duration_s");
		m_bIsOpen = false;
		m_fOpenT = 0.0f;
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		DPInteractable_Behaviour::OnUpdate(fDt);
		if (m_bIsOpen && m_fOpenT < 1.0f)
		{
			m_fOpenT = glm::min(1.0f, m_fOpenT + fDt / m_fOpenDuration);
			// Lid rotation animation lives on the child Lid entity; resolved
			// by name during the open transition. Wave-4 polish.
		}
	}

public:
	bool IsOpen() const { return m_bIsOpen; }

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		if (m_bIsOpen) return;
		m_bIsOpen = true;
		// Phase-5-audit (2026-05-16): emit DP_OnChestOpened so the
		// analyzer can require chest-open as a verified mechanic and
		// the visualiser can plot the moment. Loot dispense remains a
		// later milestone -- the event fires on the lid-opening, not
		// the inventory transfer.
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnChestOpened{ xVillager, m_xParentEntity.GetEntityID() });
	}

private:
	bool  m_bIsOpen = false;
	float m_fOpenT  = 0.0f;
	float m_fOpenDuration = 0.8f; // Fallback (Tuning.json value); OnAwake reads DP_Tuning.

#ifdef ZENITH_INPUT_SIMULATOR
public:
	float GetOpenDuration() const { return m_fOpenDuration; }
#endif
};
