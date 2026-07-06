#pragma once

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

//------------------------------------------------------------------------------
// Shared helpers for the engine Behaviour Graph node TUs
// (Zenith_GraphNode_Registration_*.cpp). Engine-side glue - may name concrete
// components; never included by Zenith/Scripting/ (which stays leaf-safe).
//------------------------------------------------------------------------------

// Resolves a position reference: empty var = self's position; a VECTOR3 var =
// that position; an ENTITY_ID var = that entity's transform position. Returns
// false when unresolvable - the polymorphic input every chase / range-gate /
// raycast / nav-destination node builds on.
inline bool Zenith_GraphNode_ResolvePositionRef(Zenith_GraphContext& xContext, const std::string& strVar, Zenith_Maths::Vector3& xOut)
{
	Zenith_Entity xEntity;
	if (strVar.empty())
	{
		xEntity = xContext.m_xSelf;
	}
	else
	{
		const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(strVar);
		if (pxValue == nullptr)
		{
			return false;
		}
		if (pxValue->GetType() == PROPERTY_TYPE_VECTOR3)
		{
			xOut = pxValue->GetVector3();
			return true;
		}
		if (pxValue->GetType() != PROPERTY_TYPE_ENTITY_ID)
		{
			return false;
		}
		xEntity = xContext.ResolveTargetEntity(strVar);
	}
	if (!xEntity.IsValid())
	{
		return false;
	}
	Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
	if (pxTransform == nullptr)
	{
		return false;
	}
	pxTransform->GetPosition(xOut);
	return true;
}
