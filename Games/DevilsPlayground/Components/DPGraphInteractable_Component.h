#pragma once
/**
 * DPGraphInteractable_Component - the graph-era interactable shim. Keeps the
 * systems-tier plumbing DPInteractable_Base owns (proximity, F-press,
 * DP_OnInteract wiring) and forwards the gameplay decision to the sibling
 * Zenith_GraphComponent as an "Interact" custom event carrying the
 * interacting villager (packed EntityID payload; the graph's OnCustomEvent
 * source stores it to a blackboard variable, default "payload").
 *
 * Doctrine split: input plumbing = C++ (this shim + the base), what an
 * interactable DOES = the entity's Behaviour Graph.
 */

#include "Components/DPInteractable_Base.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"

class DPGraphInteractable_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
	DPGraphInteractable_Component() = delete;
	DPGraphInteractable_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		Zenith_PropertyValue xPayload;
		xPayload.SetPackedEntityID(xVillager.GetPacked());
		pxGraph->FireCustomEvent("Interact", &xPayload);
	}
};
