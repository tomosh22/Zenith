#pragma once
/**
 * DPDoubleDoor_Behaviour - same lock semantics as DPDoor, but rotates two
 * child entities (left + right leaf) outwards on open. Children are
 * resolved by name on each frame the door is animating (cheap — typical
 * doors have ≤4 children) so that Editor Stop/Play swaps don't leave
 * stale EntityID caches.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"

class DPDoubleDoor_Behaviour ZENITH_FINAL : public DPInteractable_Behaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPDoubleDoor_Behaviour)

	DPDoubleDoor_Behaviour() = delete;
	DPDoubleDoor_Behaviour(Zenith_Entity& xParentEntity)
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
			ApplyRotations();
		}
	}

	// Test + HUD accessors
	bool IsOpen() const { return m_bIsOpen; }
	float GetOpenProgress() const { return m_fOpenT; }
	DP_ItemTag GetRequiredKey() const { return m_eRequiredKey; }

	// Test-only: bypass the rising-edge interact path. Used by
	// DoubleDoor_Test to verify the animation + child-rotation logic
	// in isolation from DPInteractable's proximity check.
	void OpenForTest() { m_bIsOpen = true; }

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		if (m_bIsOpen) return;
		if (!DP_Items::TryConsumeKeyForUnlock(xVillager, m_eRequiredKey)) return;
		m_bIsOpen = true;
	}

private:
	void ApplyRotations()
	{
		Zenith_TransformComponent* pxLeft  = FindChildTransform("Leaf_L");
		Zenith_TransformComponent* pxRight = FindChildTransform("Leaf_R");
		const float fA = glm::radians(m_fOpenYaw * m_fOpenT);
		if (pxLeft != nullptr)  pxLeft->SetRotation(glm::angleAxis(+fA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		if (pxRight != nullptr) pxRight->SetRotation(glm::angleAxis(-fA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	}

	// Walk children of the parent entity, return the TransformComponent of
	// the first child whose name matches szName, or nullptr if none.
	Zenith_TransformComponent* FindChildTransform(const char* szName)
	{
		if (szName == nullptr) return nullptr;
		const Zenith_Vector<Zenith_EntityID>& xChildren = m_xParentEntity.GetChildEntityIDs();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(
			m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return nullptr;
		for (u_int u = 0; u < xChildren.GetSize(); ++u)
		{
			Zenith_EntityID xChildID = xChildren.Get(u);
			Zenith_Entity xChild = pxScene->TryGetEntity(xChildID);
			if (!xChild.IsValid()) continue;
			if (xChild.GetName() != szName) continue;
			if (!xChild.HasComponent<Zenith_TransformComponent>()) continue;
			return &xChild.GetComponent<Zenith_TransformComponent>();
		}
		return nullptr;
	}

	DP_ItemTag m_eRequiredKey = DP_ItemTag::Key;
	bool       m_bIsOpen      = false;
	float      m_fOpenT       = 0.0f;
	float      m_fOpenYaw     = 80.0f;
	float      m_fOpenDuration = 0.5f;
};
