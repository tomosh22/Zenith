#pragma once

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include <string>

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

// Splits a comma-separated node property into its tokens. Two properties of it
// are CONTRACT rather than incidental, because three node families now share
// it and every one of them is authored from a hand-written string:
//
//   * tokens are used VERBATIM -- no whitespace trimming. "a, b" is the two
//     tokens "a" and " b", and a variable literally named " b" is what the
//     second one looks up. Trimming would be friendlier but it would change
//     what SwitchOnString and StateMachine already do to their authored case
//     and state names, which reach baked .bgraph assets.
//   * EMPTY tokens are skipped, so "a,,b" is two operands and a trailing comma
//     adds nothing.
inline void Zenith_GraphNode_ParseCommaList(const std::string& strList, Zenith_Vector<std::string>& axOut)
{
	axOut.Clear();
	size_t uStart = 0;
	while (uStart <= strList.size())
	{
		size_t uComma = strList.find(',', uStart);
		if (uComma == std::string::npos)
		{
			uComma = strList.size();
		}
		if (uComma > uStart)
		{
			axOut.PushBack(strList.substr(uStart, uComma - uStart));
		}
		if (uComma == strList.size())
		{
			break;
		}
		uStart = uComma + 1;
	}
}
