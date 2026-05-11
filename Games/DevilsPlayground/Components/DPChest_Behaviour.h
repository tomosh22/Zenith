#pragma once
/**
 * DPChest_Behaviour - simple openable container. On interact, the lid child
 * pivots outward; loot dispensing is gameplay-tier and ships in Wave 4.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

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
	void HandleInteract(Zenith_EntityID /*xVillager*/) override
	{
		if (m_bIsOpen) return;
		m_bIsOpen = true;
	}

private:
	bool  m_bIsOpen = false;
	float m_fOpenT  = 0.0f;
	float m_fOpenDuration = 0.5f;
};
