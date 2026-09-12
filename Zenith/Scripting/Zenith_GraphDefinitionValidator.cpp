#include "Zenith.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphPinTable.h"

#include <cstdarg>
#include <cstdio>

namespace
{
	//--------------------------------------------------------------------------
	// One node's pin bindings, resolved against its param blob.
	//--------------------------------------------------------------------------
	struct ValidatorPin
	{
		const Zenith_GraphPinDesc* m_pxDesc = nullptr;
		u_int m_uPinIndex = 0;
		std::string m_strVar;								// "" = not bound (skipped)
		Zenith_PropertyType m_eType = eGRAPH_PIN_TYPE_ANY;	// eGRAPH_PIN_TYPE_ANY = unifies with everything
		bool m_bBoundThroughFallback = false;				// the primary var-name property read EMPTY (the in-place form)
		bool m_bWired = false;								// pass 1b resolved a data edge INTO this pin
	};

	struct ValidatorNode
	{
		u_int m_uNodeID = 0;
		std::string m_strTypeName;
		// ★ REGISTERED and OPAQUE are two different facts and pass 1b needs both:
		// an UNREGISTERED endpoint is skipped silently (a per-game node library
		// this exe does not carry), while a REGISTERED endpoint with no pin table
		// is an ERROR (the runtime could never bind that wire).
		bool m_bRegistered = false;
		bool m_bOpaque = true;
		bool m_bPure = false;
		bool m_bEventSource = false;
		bool m_bVariadicNameCollision = false;
		// Read off the PARAM-APPLIED temp in pass 0, while it is still alive: -1 =
		// the type declares no variadic family.
		int32_t m_iDynamicDataInputCount = -1;
		const Zenith_GraphPinTable* m_pxPins = nullptr;
		Zenith_Vector<ValidatorPin> m_axPins;
	};

	struct ValidatorWriter
	{
		std::string m_strVar;
		u_int m_uNodeID = 0;
		Zenith_PropertyType m_eType = eGRAPH_PIN_TYPE_ANY;
	};

	// The var-name read has ONE home, Zenith_GraphPin_ReadStringProperty in
	// Zenith_GraphPinTable.h - the RUNTIME binds its pins through the same
	// function (B-2), so "the var name the validator checked" and "the var name
	// the runtime bound" cannot drift. These aliases keep the call sites below
	// reading as they did.
	typedef Zenith_GraphPinReadPropertyResult ReadPropertyResult;
	constexpr ReadPropertyResult READ_PROPERTY_INVALID = GRAPH_PIN_READ_PROPERTY_INVALID;

	inline ReadPropertyResult ReadStringProperty(const Zenith_PropertyTable* pxTable, const Zenith_GraphNode* pxNode,
		const char* szProperty, std::string& strOut)
	{
		return Zenith_GraphPin_ReadStringProperty(pxTable, pxNode, szProperty, strOut);
	}

	bool PinRoleReads(Zenith_GraphPinRole eRole)
	{
		return eRole == GRAPH_PIN_ROLE_INPUT
			|| eRole == GRAPH_PIN_ROLE_SELECTOR_READ
			|| eRole == GRAPH_PIN_ROLE_SELECTOR_READWRITE
			|| eRole == GRAPH_PIN_ROLE_TARGET_REF;
	}

	bool PinRoleWrites(Zenith_GraphPinRole eRole)
	{
		return eRole == GRAPH_PIN_ROLE_OUTPUT
			|| eRole == GRAPH_PIN_ROLE_SELECTOR_WRITE
			|| eRole == GRAPH_PIN_ROLE_SELECTOR_READWRITE;
	}

	const Zenith_GraphVariableDecl* FindDeclaredVariable(const Zenith_GraphDefinition& xDefinition, const std::string& strName)
	{
		for (u_int u = 0; u < xDefinition.GetVariableCount(); ++u)
		{
			if (xDefinition.GetVariableAt(u).m_strName == strName)
			{
				return &xDefinition.GetVariableAt(u);
			}
		}
		return nullptr;
	}

	ValidatorNode* FindValidatorNodeMutable(Zenith_Vector<ValidatorNode>& axNodes, u_int uNodeID)
	{
		for (u_int u = 0; u < axNodes.GetSize(); ++u)
		{
			if (axNodes.Get(u).m_uNodeID == uNodeID)
			{
				return &axNodes.Get(u);
			}
		}
		return nullptr;
	}

	const ValidatorNode* FindValidatorNode(const Zenith_Vector<ValidatorNode>& axNodes, u_int uNodeID)
	{
		for (u_int u = 0; u < axNodes.GetSize(); ++u)
		{
			if (axNodes.Get(u).m_uNodeID == uNodeID)
			{
				return &axNodes.Get(u);
			}
		}
		return nullptr;
	}

	bool PinHasTypeFromVariable(const Zenith_GraphPinDesc& xDesc)
	{
		return xDesc.m_szTypeFromVarNameProperty != nullptr && xDesc.m_szTypeFromVarNameProperty[0] != '\0';
	}

	// ★ THE ONE PIN-TYPE RESOLUTION. Pass 0 calls it with the temp instance it
	// already built; Zenith_GraphDefinitionValidator::ResolvePinType calls it with
	// one of its own. Three sources, in the order a descriptor can carry them:
	// the GRAPH (a from-variable pin takes the DECLARED type of the variable its
	// property names), the INSTANCE (GetPinType on the param-applied node), then
	// the CLASS (the static m_eType). A from-variable pin naming an undeclared
	// variable is ANY and is NOT a declined instance resolution - it earns no
	// warning, because the SELECTOR_READ that names the same property is already
	// reported by declare-or-error and one mistake deserves one finding.
	Zenith_PropertyType ResolveOnePinType(const Zenith_GraphDefinition& xDefinition, const Zenith_GraphPinDesc& xDesc,
		u_int uPinIndex, const Zenith_PropertyTable* pxProperties, const Zenith_GraphNode* pxInstance,
		bool& bOutInstanceDeclined)
	{
		bOutInstanceDeclined = false;
		if (PinHasTypeFromVariable(xDesc))
		{
			std::string strTypeVar;
			if (ReadStringProperty(pxProperties, pxInstance, xDesc.m_szTypeFromVarNameProperty, strTypeVar)
				!= GRAPH_PIN_READ_PROPERTY_OK)
			{
				return eGRAPH_PIN_TYPE_ANY;
			}
			const Zenith_GraphVariableDecl* pxDecl = FindDeclaredVariable(xDefinition, strTypeVar);
			return pxDecl != nullptr ? pxDecl->m_xDefault.GetType() : eGRAPH_PIN_TYPE_ANY;
		}
		if (xDesc.m_bInstanceResolved)
		{
			Zenith_PropertyType eResolved = eGRAPH_PIN_TYPE_ANY;
			if (pxInstance != nullptr && pxInstance->GetPinType(uPinIndex, eResolved) && eResolved < PROPERTY_TYPE_COUNT)
			{
				return eResolved;
			}
			bOutInstanceDeclined = true;
			return eGRAPH_PIN_TYPE_ANY;
		}
		return xDesc.m_eType;
	}

	// Splits a wire's destination pin name into "<family><ordinal>", B-2's
	// algorithm verbatim. false = there is no digit tail, or the name is ALL
	// digits (no family part), so it can only ever be an exact pin name.
	bool SplitVariadicMemberName(const std::string& strName, std::string& strFamilyOut, u_int& uOrdinalOut,
		bool& bOverflowedOut)
	{
		bOverflowedOut = false;
		uOrdinalOut = 0;
		size_t uDigitStart = strName.size();
		while (uDigitStart > 0 && strName[uDigitStart - 1] >= '0' && strName[uDigitStart - 1] <= '9')
		{
			--uDigitStart;
		}
		if (uDigitStart == 0 || uDigitStart == strName.size())
		{
			return false;
		}
		strFamilyOut = strName.substr(0, uDigitStart);
		u_int uOrdinal = 0;
		for (size_t uAt = uDigitStart; uAt < strName.size(); ++uAt)
		{
			if (uOrdinal > 0xFFFFFFu)
			{
				bOverflowedOut = true;	// far past any member count; treated as past the end
				break;
			}
			uOrdinal = uOrdinal * 10u + static_cast<u_int>(strName[uAt] - '0');
		}
		uOrdinalOut = uOrdinal;
		return true;
	}

	bool VectorContainsString(const Zenith_Vector<std::string>& axStrings, const std::string& strValue)
	{
		for (u_int u = 0; u < axStrings.GetSize(); ++u)
		{
			if (axStrings.Get(u) == strValue)
			{
				return true;
			}
		}
		return false;
	}

	// Every finding is built here. bError IS the severity bit: true = a defect
	// that fails Zenith_GraphBuilder::Build(), false = an informational warning.
	void AddFinding(Zenith_Vector<Zenith_GraphValidationFinding>& axOut, bool bError,
		Zenith_GraphValidationRule eRule, u_int uNodeID, const char* szTypeName, const char* szPin, const char* szVar,
		const char* szFormat, ...)
	{
		Zenith_GraphValidationFinding xFinding;
		xFinding.m_eRule = eRule;
		xFinding.m_eSeverity = bError ? GRAPH_VALIDATION_SEVERITY_ERROR : GRAPH_VALIDATION_SEVERITY_WARNING;
		xFinding.m_uNodeID = uNodeID;
		xFinding.m_strTypeName = szTypeName ? szTypeName : "";
		xFinding.m_strPin = szPin ? szPin : "";
		xFinding.m_strVar = szVar ? szVar : "";

		char acText[512];
		va_list xArgs;
		va_start(xArgs, szFormat);
		vsnprintf(acText, sizeof(acText), szFormat, xArgs);
		va_end(xArgs);
		xFinding.m_strWhat = acText;

		axOut.PushBack(xFinding);
	}

	const char* TypeName(Zenith_PropertyType eType)
	{
		switch (eType)
		{
		case PROPERTY_TYPE_FLOAT:     return "FLOAT";
		case PROPERTY_TYPE_INT32:     return "INT32";
		case PROPERTY_TYPE_UINT32:    return "UINT32";
		case PROPERTY_TYPE_BOOL:      return "BOOL";
		case PROPERTY_TYPE_VECTOR2:   return "VECTOR2";
		case PROPERTY_TYPE_VECTOR3:   return "VECTOR3";
		case PROPERTY_TYPE_VECTOR4:   return "VECTOR4";
		case PROPERTY_TYPE_STRING:    return "STRING";
		case PROPERTY_TYPE_ENTITY_ID: return "ENTITY_ID";
		case PROPERTY_TYPE_GUID:      return "GUID";
		default:                      return "ANY";
		}
	}
}

//==============================================================================
// Zenith_GraphDefinitionValidator
//==============================================================================

namespace
{
	//--------------------------------------------------------------------------
	// LOAD_SAFETY's two registry-dependent checks. Both live here, and ONLY here,
	// so the FULL tier cannot count the same defect twice.
	//--------------------------------------------------------------------------

	// The pin table of one node def, or null when the type is unregistered or
	// declares none. bOutPure/bOutCollision are only meaningful on a non-null
	// return.
	const Zenith_GraphPinTable* FindPinTableForNode(const Zenith_GraphNodeRegistry& xRegistry,
		const Zenith_GraphNodeDef& xNodeDef, bool& bOutPure, bool& bOutCollision)
	{
		bOutPure = false;
		bOutCollision = false;
		const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(xNodeDef.m_strTypeName.c_str());
		if (pxInfo == nullptr || pxInfo->m_pfnCreate == nullptr || pxInfo->m_pfnGetPinTable == nullptr)
		{
			return nullptr;
		}
		bOutPure = pxInfo->m_bPureNode;
		bOutCollision = pxInfo->m_bVariadicNameCollision;
		const Zenith_GraphPinTable* pxPins = pxInfo->m_pfnGetPinTable();
		return (pxPins != nullptr && pxPins->GetPinCount() > 0) ? pxPins : nullptr;
	}

	// The descriptor a wire's pin NAME points at, WITHOUT an instance: an exact
	// name, else a "<family><ordinal>" whose family is variadic. Null = unknown
	// here, which this tier reports nothing about (the FULL tier's wire pass
	// does). The ORDINAL is deliberately NOT range-checked - that needs a
	// param-applied instance, and every member of a family carries the family's
	// one static type anyway.
	const Zenith_GraphPinDesc* FindStaticDescriptorForWireName(const Zenith_GraphPinTable& xPins,
		bool bVariadicNameCollision, const std::string& strName)
	{
		const u_int uExact = xPins.FindPinIndex(strName.c_str());
		if (uExact < xPins.GetPinCount())
		{
			// A BARE variadic family name is not a wire endpoint (the runtime
			// refuses it; pass 1b reports WIRE_PIN_UNKNOWN) - never type-check it.
			return xPins.GetPinAt(uExact).m_bVariadic ? nullptr : &xPins.GetPinAt(uExact);
		}
		std::string strFamily;
		u_int uOrdinal = 0;
		bool bOverflowed = false;
		if (bVariadicNameCollision || !SplitVariadicMemberName(strName, strFamily, uOrdinal, bOverflowed))
		{
			return nullptr;
		}
		const u_int uFamily = xPins.FindPinIndex(strFamily.c_str());
		if (uFamily >= xPins.GetPinCount() || !xPins.GetPinAt(uFamily).m_bVariadic)
		{
			return nullptr;
		}
		return &xPins.GetPinAt(uFamily);
	}

	u_int FindNodeIndexByID(const Zenith_GraphDefinition& xDefinition, u_int uNodeID)
	{
		for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
		{
			if (xDefinition.GetNodeAt(u).m_uNodeID == uNodeID)
			{
				return u;
			}
		}
		return xDefinition.GetNodeCount();
	}

	constexpr u_int8 uCYCLE_COLOUR_WHITE = 0;
	constexpr u_int8 uCYCLE_COLOUR_GREY = 1;
	constexpr u_int8 uCYCLE_COLOUR_BLACK = 2;

	// Depth-first search for a back edge over the PURE-SOURCED data graph. The
	// stack carries node INDICES, so the reporting side can name the cycle.
	// Returns true - and leaves auStack holding the cycle from uCycleStartDepth
	// to its top - on the first back edge found.
	bool FindPureDataCycle(const Zenith_GraphDefinition& xDefinition, const Zenith_Vector<u_int8>& aePureSource,
		u_int uIndex, Zenith_Vector<u_int8>& aeColour, Zenith_Vector<u_int>& auStack, u_int& uCycleStartDepth)
	{
		aeColour.Get(uIndex) = uCYCLE_COLOUR_GREY;
		auStack.PushBack(uIndex);

		const u_int uNodeID = xDefinition.GetNodeAt(uIndex).m_uNodeID;
		if (aePureSource.Get(uIndex) != 0u)
		{
			for (u_int uEdge = 0; uEdge < xDefinition.GetDataEdgeCount(); ++uEdge)
			{
				const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(uEdge);
				if (xEdge.m_uSrcNodeID != uNodeID)
				{
					continue;
				}
				const u_int uNext = FindNodeIndexByID(xDefinition, xEdge.m_uDstNodeID);
				if (uNext >= xDefinition.GetNodeCount())
				{
					continue;	// an orphan endpoint: the FULL tier reports it
				}
				if (aeColour.Get(uNext) == uCYCLE_COLOUR_GREY)
				{
					// A BACK EDGE. Everything from uNext's position on the stack to
					// the top is the cycle.
					for (u_int uAt = 0; uAt < auStack.GetSize(); ++uAt)
					{
						if (auStack.Get(uAt) == uNext)
						{
							uCycleStartDepth = uAt;
							break;
						}
					}
					return true;
				}
				if (aeColour.Get(uNext) == uCYCLE_COLOUR_WHITE
					&& FindPureDataCycle(xDefinition, aePureSource, uNext, aeColour, auStack, uCycleStartDepth))
				{
					return true;
				}
			}
		}

		auStack.PopBack();
		aeColour.Get(uIndex) = uCYCLE_COLOUR_BLACK;
		return false;
	}

	//--------------------------------------------------------------------------
	// DOMINANCE support.
	//--------------------------------------------------------------------------

	// Marks every node reachable from the given event-source roots over EXEC
	// edges, with uExcludedNodeID (and therefore every edge that leaves it)
	// removed from the graph. uExcludedNodeID == 0 removes nothing.
	// A flow node fans through ALL its pins and a routed failure pin is an exec
	// edge like any other, so "an exec edge exists" IS the successor relation -
	// no pin index is consulted.
	void MarkExecReachable(const Zenith_GraphDefinition& xDefinition, const Zenith_Vector<u_int>& auEventSourceIndices,
		u_int uExcludedNodeID, Zenith_Vector<u_int8>& abReachableOut)
	{
		abReachableOut.Clear();
		abReachableOut.Resize(xDefinition.GetNodeCount(), static_cast<u_int8>(0u));

		Zenith_Vector<u_int> auFrontier;
		for (u_int u = 0; u < auEventSourceIndices.GetSize(); ++u)
		{
			const u_int uIndex = auEventSourceIndices.Get(u);
			if (xDefinition.GetNodeAt(uIndex).m_uNodeID == uExcludedNodeID)
			{
				continue;
			}
			abReachableOut.Get(uIndex) = static_cast<u_int8>(1u);
			auFrontier.PushBack(uIndex);
		}
		while (auFrontier.GetSize() > 0)
		{
			const u_int uIndex = auFrontier.Get(auFrontier.GetSize() - 1u);
			auFrontier.PopBack();
			const u_int uNodeID = xDefinition.GetNodeAt(uIndex).m_uNodeID;
			for (u_int uEdge = 0; uEdge < xDefinition.GetEdgeCount(); ++uEdge)
			{
				const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(uEdge);
				if (xEdge.m_uSrcNodeID != uNodeID || xEdge.m_uDstNodeID == uExcludedNodeID)
				{
					continue;
				}
				const u_int uNext = FindNodeIndexByID(xDefinition, xEdge.m_uDstNodeID);
				if (uNext >= xDefinition.GetNodeCount() || abReachableOut.Get(uNext) != 0u)
				{
					continue;
				}
				abReachableOut.Get(uNext) = static_cast<u_int8>(1u);
				auFrontier.PushBack(uNext);
			}
		}
	}

	// The EXECUTABLE consumers one producer's outgoing wires reach, followed
	// THROUGH pure nodes: a pure node runs inside its own consumer's gather, so
	// it is a relay rather than a consumer. auVisited is both the recursion guard
	// (a data cycle would otherwise not terminate) and the de-duplicator.
	void CollectExecutableConsumers(const Zenith_GraphDefinition& xDefinition,
		const Zenith_Vector<ValidatorNode>& axNodes, u_int uFromNodeID,
		Zenith_Vector<u_int>& auVisited, Zenith_Vector<u_int>& auConsumersOut)
	{
		for (u_int uEdge = 0; uEdge < xDefinition.GetDataEdgeCount(); ++uEdge)
		{
			const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(uEdge);
			if (xEdge.m_uSrcNodeID != uFromNodeID)
			{
				continue;
			}
			const ValidatorNode* pxDst = FindValidatorNode(axNodes, xEdge.m_uDstNodeID);
			if (pxDst == nullptr || !pxDst->m_bRegistered)
			{
				continue;
			}
			bool bSeen = false;
			for (u_int u = 0; u < auVisited.GetSize() && !bSeen; ++u)
			{
				bSeen = auVisited.Get(u) == xEdge.m_uDstNodeID;
			}
			if (bSeen)
			{
				continue;
			}
			auVisited.PushBack(xEdge.m_uDstNodeID);
			if (pxDst->m_bPure)
			{
				CollectExecutableConsumers(xDefinition, axNodes, xEdge.m_uDstNodeID, auVisited, auConsumersOut);
				continue;
			}
			auConsumersOut.PushBack(xEdge.m_uDstNodeID);
		}
	}

	void BuildDominanceWarnings(const Zenith_GraphDefinition& xDefinition, const Zenith_Vector<ValidatorNode>& axNodes,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
	{
		if (xDefinition.GetNodeCount() == 0 || xDefinition.GetDataEdgeCount() == 0)
		{
			return;
		}

		// The event-source ROOTS, found ONCE: every reachability walk below is the
		// same search from the same roots with one node removed.
		Zenith_Vector<u_int> auEventSourceIndices;
		for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
		{
			const ValidatorNode* pxNode = FindValidatorNode(axNodes, xDefinition.GetNodeAt(u).m_uNodeID);
			if (pxNode != nullptr && pxNode->m_bEventSource)
			{
				auEventSourceIndices.PushBack(u);
			}
		}
		if (auEventSourceIndices.GetSize() == 0)
		{
			return;	// nothing runs at all; the graph has bigger problems than ordering
		}

		for (u_int uNode = 0; uNode < axNodes.GetSize(); ++uNode)
		{
			const ValidatorNode& xProducer = axNodes.Get(uNode);
			// A PURE producer cannot run late: it evaluates from inside its
			// consumer's gather. An EVENT SOURCE producer is skipped too - what it
			// publishes is a dispatch payload, not a latched pin.
			if (!xProducer.m_bRegistered || xProducer.m_bPure || xProducer.m_bEventSource)
			{
				continue;
			}

			Zenith_Vector<u_int> auVisited;
			Zenith_Vector<u_int> auConsumers;
			CollectExecutableConsumers(xDefinition, axNodes, xProducer.m_uNodeID, auVisited, auConsumers);
			if (auConsumers.GetSize() == 0)
			{
				continue;
			}

			Zenith_Vector<u_int8> abReachableWithout;
			MarkExecReachable(xDefinition, auEventSourceIndices, xProducer.m_uNodeID, abReachableWithout);

			u_int uAffected = 0;
			u_int uFirstConsumer = 0;
			for (u_int u = 0; u < auConsumers.GetSize(); ++u)
			{
				const u_int uConsumerID = auConsumers.Get(u);
				if (uConsumerID == xProducer.m_uNodeID)
				{
					continue;
				}
				const u_int uIndex = FindNodeIndexByID(xDefinition, uConsumerID);
				if (uIndex >= xDefinition.GetNodeCount() || abReachableWithout.Get(uIndex) == 0u)
				{
					continue;
				}
				if (uAffected == 0)
				{
					uFirstConsumer = uConsumerID;
				}
				++uAffected;
			}
			if (uAffected == 0)
			{
				continue;
			}

			const Zenith_GraphNodeDef* pxFirst = xDefinition.FindNodeDef(uFirstConsumer);
			AddFinding(axOut, false, GRAPH_VALIDATION_RULE_DOMINANCE,
				xProducer.m_uNodeID, xProducer.m_strTypeName.c_str(), "", "",
				"consumer %u:%s may execute before its producer has run - the wire reads the slot's default (%u consumer(s) affected)",
				uFirstConsumer, pxFirst != nullptr ? pxFirst->m_strTypeName.c_str() : "?", uAffected);
		}
	}
}

void Zenith_GraphDefinitionValidator::AppendLoadSafetyFindings(const Zenith_GraphDefinition& xDefinition,
	const Zenith_GraphNodeRegistry& xRegistry, Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
{
	//--------------------------------------------------------------------------
	// Two exec edges leaving one (node, pin). AddEdge refuses to create one, so
	// the only way in is a loaded asset - and the walk would silently take the
	// first, which is a wiring the author cannot see and cannot predict.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetEdgeCount(); ++u)
	{
		const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(u);
		for (u_int uEarlier = 0; uEarlier < u; ++uEarlier)
		{
			const Zenith_GraphEdge& xOther = xDefinition.GetEdgeAt(uEarlier);
			if (xOther.m_uSrcNodeID != xEdge.m_uSrcNodeID || xOther.m_uSrcPin != xEdge.m_uSrcPin)
			{
				continue;
			}
			const Zenith_GraphNodeDef* pxSrc = xDefinition.FindNodeDef(xEdge.m_uSrcNodeID);
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE,
				xEdge.m_uSrcNodeID, pxSrc ? pxSrc->m_strTypeName.c_str() : "", "", "",
				"a second exec edge leaves (node %u, pin %u) - which successor runs would be arbitrary",
				xEdge.m_uSrcNodeID, xEdge.m_uSrcPin);
			break;	// one finding per EXTRA edge, not one per pair
		}
	}

	//--------------------------------------------------------------------------
	// Two data edges entering one (node, pin name): one incoming wire per input.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetDataEdgeCount(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(u);
		for (u_int uEarlier = 0; uEarlier < u; ++uEarlier)
		{
			const Zenith_GraphDataEdge& xOther = xDefinition.GetDataEdgeAt(uEarlier);
			if (xOther.m_uDstNodeID != xEdge.m_uDstNodeID || xOther.m_strDstPin != xEdge.m_strDstPin)
			{
				continue;
			}
			const Zenith_GraphNodeDef* pxDst = xDefinition.FindNodeDef(xEdge.m_uDstNodeID);
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT,
				xEdge.m_uDstNodeID, pxDst ? pxDst->m_strTypeName.c_str() : "", xEdge.m_strDstPin.c_str(), "",
				"a second data edge enters (node %u, pin '%s') - an input takes ONE wire",
				xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str());
			break;
		}
	}

	//--------------------------------------------------------------------------
	// Malformed data edges. A self-loop is the NODE pair, whatever the pin names
	// say - the same reading AddDataEdge uses.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetDataEdgeCount(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(u);
		const char* szWhy = nullptr;
		if (xEdge.m_uSrcNodeID == 0 || xEdge.m_uDstNodeID == 0)
		{
			szWhy = "names node id 0, which is never a node";
		}
		else if (xEdge.m_uSrcNodeID == xEdge.m_uDstNodeID)
		{
			szWhy = "is a self-loop - a node cannot feed its own input";
		}
		else if (xEdge.m_strSrcPin.empty() || xEdge.m_strDstPin.empty())
		{
			szWhy = "carries an empty pin name, which can never resolve to a pin";
		}
		if (szWhy == nullptr)
		{
			continue;
		}
		const Zenith_GraphNodeDef* pxDst = xDefinition.FindNodeDef(xEdge.m_uDstNodeID);
		AddFinding(axOut, true, GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED,
			xEdge.m_uDstNodeID, pxDst ? pxDst->m_strTypeName.c_str() : "", xEdge.m_strDstPin.c_str(), "",
			"data edge %u:'%s' -> %u:'%s' %s",
			xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(), szWhy);
	}

	//--------------------------------------------------------------------------
	// STATIC-vs-STATIC type mismatch across a wire. BOTH descriptors carry a
	// static m_eType (never instance-resolved, never from-variable - those need
	// params or declarations, which is the FULL tier's job), both are non-ANY,
	// and they differ: the consumer can only ever take its pin default, and
	// nothing at runtime would say why.
	//
	// ★ THIS IS THE ONLY PLACE THE STATIC CASE IS REPORTED. Validate() runs this
	// body first, so its wire pass reports a type mismatch only when an endpoint
	// is instance-resolved or from-variable.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetDataEdgeCount(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(u);
		const Zenith_GraphNodeDef* pxSrcDef = xDefinition.FindNodeDef(xEdge.m_uSrcNodeID);
		const Zenith_GraphNodeDef* pxDstDef = xDefinition.FindNodeDef(xEdge.m_uDstNodeID);
		if (pxSrcDef == nullptr || pxDstDef == nullptr)
		{
			continue;
		}
		bool bSrcPure = false;
		bool bSrcCollision = false;
		bool bDstPure = false;
		bool bDstCollision = false;
		const Zenith_GraphPinTable* pxSrcPins = FindPinTableForNode(xRegistry, *pxSrcDef, bSrcPure, bSrcCollision);
		const Zenith_GraphPinTable* pxDstPins = FindPinTableForNode(xRegistry, *pxDstDef, bDstPure, bDstCollision);
		if (pxSrcPins == nullptr || pxDstPins == nullptr)
		{
			continue;
		}
		const Zenith_GraphPinDesc* pxSrcDesc = FindStaticDescriptorForWireName(*pxSrcPins, bSrcCollision, xEdge.m_strSrcPin);
		const Zenith_GraphPinDesc* pxDstDesc = FindStaticDescriptorForWireName(*pxDstPins, bDstCollision, xEdge.m_strDstPin);
		if (pxSrcDesc == nullptr || pxDstDesc == nullptr)
		{
			continue;
		}
		// ROLE first: a wire leaving an INPUT or entering an OUTPUT is a
		// WIRE_ROLE_MISMATCH for the FULL tier, never a load-time refusal - and
		// never a second finding on top of the role error.
		if (pxSrcDesc->m_eRole != GRAPH_PIN_ROLE_OUTPUT || pxDstDesc->m_eRole != GRAPH_PIN_ROLE_INPUT)
		{
			continue;
		}
		if (pxSrcDesc->m_bInstanceResolved || pxDstDesc->m_bInstanceResolved
			|| PinHasTypeFromVariable(*pxSrcDesc) || PinHasTypeFromVariable(*pxDstDesc))
		{
			continue;
		}
		if (pxSrcDesc->m_eType == eGRAPH_PIN_TYPE_ANY || pxDstDesc->m_eType == eGRAPH_PIN_TYPE_ANY
			|| pxSrcDesc->m_eType == pxDstDesc->m_eType)
		{
			continue;
		}
		AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
			xEdge.m_uDstNodeID, pxDstDef->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), "",
			"wire %u:'%s' (%s) -> %u:'%s' (%s): the endpoint types disagree",
			xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), TypeName(pxSrcDesc->m_eType),
			xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(), TypeName(pxDstDesc->m_eType));
	}

	//--------------------------------------------------------------------------
	// A DATA CYCLE through PURE producers. A pure node evaluates on demand from
	// inside its consumer's gather, so a cycle among them is unbounded recursion
	// on paper; B-2's re-entry flag makes it merely a warning and a defaulted
	// read at runtime, which is exactly why it must be caught here instead.
	//
	// ★ AN IMPURE PRODUCER BREAKS A CYCLE BY DESIGN: its slot is LATCHED by its
	// own Execute, so a consumer pulling it reads a value from a previous run
	// rather than re-entering it. Only edges whose SOURCE is pure are followed.
	//--------------------------------------------------------------------------
	if (xDefinition.GetNodeCount() > 0 && xDefinition.GetDataEdgeCount() > 0)
	{
		Zenith_Vector<u_int8> aePureSource;
		Zenith_Vector<u_int8> aeColour;
		aePureSource.Resize(xDefinition.GetNodeCount(), static_cast<u_int8>(0u));
		aeColour.Resize(xDefinition.GetNodeCount(), uCYCLE_COLOUR_WHITE);
		bool bAnyPure = false;
		for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
		{
			bool bPure = false;
			bool bCollision = false;
			FindPinTableForNode(xRegistry, xDefinition.GetNodeAt(u), bPure, bCollision);
			aePureSource.Get(u) = static_cast<u_int8>(bPure ? 1u : 0u);
			bAnyPure = bAnyPure || bPure;
		}

		if (bAnyPure)
		{
			Zenith_Vector<u_int> auStack;
			u_int uCycleStartDepth = 0;
			bool bFound = false;
			for (u_int u = 0; u < xDefinition.GetNodeCount() && !bFound; ++u)
			{
				if (aeColour.Get(u) != uCYCLE_COLOUR_WHITE)
				{
					continue;
				}
				auStack.Clear();
				bFound = FindPureDataCycle(xDefinition, aePureSource, u, aeColour, auStack, uCycleStartDepth);
			}
			if (bFound)
			{
				// ONE finding, naming the ring. A second search would report the
				// same ring from a different entry point, so the walk stops here.
				std::string strRing;
				for (u_int uAt = uCycleStartDepth; uAt < auStack.GetSize(); ++uAt)
				{
					const Zenith_GraphNodeDef& xNodeDef = xDefinition.GetNodeAt(auStack.Get(uAt));
					char acNode[96];
					snprintf(acNode, sizeof(acNode), "%s%u:%s", strRing.empty() ? "" : " -> ",
						xNodeDef.m_uNodeID, xNodeDef.m_strTypeName.c_str());
					strRing += acNode;
				}
				const Zenith_GraphNodeDef& xStart = xDefinition.GetNodeAt(auStack.Get(uCycleStartDepth));
				AddFinding(axOut, true, GRAPH_VALIDATION_RULE_DATA_CYCLE,
					xStart.m_uNodeID, xStart.m_strTypeName.c_str(), "", "",
					"pure data cycle: %s -> %u:%s", strRing.c_str(), xStart.m_uNodeID, xStart.m_strTypeName.c_str());
			}
		}
	}
}

void Zenith_GraphDefinitionValidator::ValidateLoadSafety(const Zenith_GraphDefinition& xDefinition,
	const Zenith_GraphNodeRegistry& xRegistry, Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
{
	axOut.Clear();
	AppendLoadSafetyFindings(xDefinition, xRegistry, axOut);
}

void Zenith_GraphDefinitionValidator::Validate(const Zenith_GraphDefinition& xDefinition,
	const Zenith_GraphNodeRegistry& xRegistry, const char* szGraphName,
	Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
{
	(void)szGraphName;	// the graph name travels on the LOG line, not on a per-node finding
	axOut.Clear();

	// The LOAD_SAFETY subset first, through the APPENDING helper - one report
	// covers both tiers, and the clearing entry point is never called from here.
	// It OWNS the pure DATA_CYCLE and the STATIC-vs-STATIC wire TYPE_MISMATCH;
	// pass 1b below reports neither again.
	AppendLoadSafetyFindings(xDefinition, xRegistry, axOut);

	//--------------------------------------------------------------------------
	// Pass 0 - resolve every node's pin bindings against its param blob.
	//--------------------------------------------------------------------------
	Zenith_Vector<ValidatorNode> axNodes;
	bool bAnyOpaqueNode = false;

	for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
	{
		const Zenith_GraphNodeDef& xNodeDef = xDefinition.GetNodeAt(u);

		ValidatorNode xNode;
		xNode.m_uNodeID = xNodeDef.m_uNodeID;
		xNode.m_strTypeName = xNodeDef.m_strTypeName;
		xNode.m_bOpaque = true;

		const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(xNodeDef.m_strTypeName.c_str());
		// A null create fn is real: the Sentinel link proofs run against an empty
		// registry, and an unresolved node has no type info at all.
		xNode.m_bRegistered = pxInfo != nullptr && pxInfo->m_pfnCreate != nullptr;
		if (xNode.m_bRegistered)
		{
			xNode.m_bPure = pxInfo->m_bPureNode;
			xNode.m_bEventSource = pxInfo->m_eEventType != GRAPH_EVENT_NONE;
			xNode.m_bVariadicNameCollision = pxInfo->m_bVariadicNameCollision;
		}
		const Zenith_GraphPinTable* pxPins = (xNode.m_bRegistered && pxInfo->m_pfnGetPinTable)
			? pxInfo->m_pfnGetPinTable() : nullptr;

		if (pxPins != nullptr && pxPins->GetPinCount() > 0)
		{
			xNode.m_bOpaque = false;
			xNode.m_pxPins = pxPins;

			const Zenith_PropertyTable* pxProperties = pxInfo->m_pfnGetPropertyTable
				? pxInfo->m_pfnGetPropertyTable() : nullptr;
			Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
			xDefinition.ApplyNodeParams(xNodeDef.m_uNodeID, pxTemp, *pxInfo);

			for (u_int uPin = 0; uPin < pxPins->GetPinCount(); ++uPin)
			{
				const Zenith_GraphPinDesc& xDesc = pxPins->GetPinAt(uPin);

				ValidatorPin xPin;
				xPin.m_pxDesc = &xDesc;		// into the class's static table - stable for the process
				xPin.m_uPinIndex = uPin;

				// --- the bound variable name (with the empty-var fallback) ---
				std::string strVar;
				const ReadPropertyResult ePrimary = ReadStringProperty(pxProperties, pxTemp, xDesc.m_szVarNameProperty, strVar);
				if (ePrimary == READ_PROPERTY_INVALID)
				{
					strVar.clear();
					AddFinding(axOut, false, GRAPH_VALIDATION_RULE_PIN_BINDING_INVALID,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, "",
						"pin binds var-name property '%s', which the type does not declare as a string property",
						xDesc.m_szVarNameProperty ? xDesc.m_szVarNameProperty : "");
				}
				if (strVar.empty() && xDesc.m_szFallbackVarNameProperty != nullptr && xDesc.m_szFallbackVarNameProperty[0] != '\0')
				{
					std::string strFallback;
					const ReadPropertyResult eFallback = ReadStringProperty(pxProperties, pxTemp, xDesc.m_szFallbackVarNameProperty, strFallback);
					if (eFallback == READ_PROPERTY_INVALID)
					{
						AddFinding(axOut, false, GRAPH_VALIDATION_RULE_PIN_BINDING_INVALID,
							xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, "",
							"pin names fallback var-name property '%s', which the type does not declare as a string property",
							xDesc.m_szFallbackVarNameProperty);
					}
					else
					{
						strVar = strFallback;
						xPin.m_bBoundThroughFallback = !strFallback.empty();
					}
				}
				xPin.m_strVar = strVar;

				// --- IN-PLACE ALIASING ------------------------------------------
				// The primary var-name property read EMPTY and the FALLBACK named
				// something, so this OUTPUT writes back over the variable it was
				// computed from - the two Math nodes' in-place form. A warning, not
				// an error: it is legal, it is what the shape means, and C-1
				// deletes the fallback binding entirely.
				if (xPin.m_bBoundThroughFallback && xDesc.m_eRole == GRAPH_PIN_ROLE_OUTPUT)
				{
					AddFinding(axOut, false, GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"result var '%s' is empty, so the output aliases its own source variable '%s'",
						xDesc.m_szVarNameProperty ? xDesc.m_szVarNameProperty : "", xPin.m_strVar.c_str());
				}

				// --- the pin's resolved type ---
				bool bInstanceDeclined = false;
				xPin.m_eType = ResolveOnePinType(xDefinition, xDesc, uPin, pxProperties, pxTemp, bInstanceDeclined);
				if (bInstanceDeclined && !xPin.m_strVar.empty())
				{
					// Never fabricate a type: ANY plus one warning naming the type.
					// An UNBOUND pin (empty var name) takes part in no later pass,
					// so it earns no warning either - a blank in-place result var
					// is the normal shape, not a defect.
					AddFinding(axOut, false, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"instance-resolved pin: '%s' declined to answer GetPinType, treating the pin as ANY",
						xNode.m_strTypeName.c_str());
				}

				xNode.m_axPins.PushBack(xPin);
			}

			// Read off the PARAM-APPLIED instance while it is alive - pass 1b needs
			// it to range-check a variadic ordinal, exactly as BuildPinState does.
			xNode.m_iDynamicDataInputCount = pxTemp->GetDynamicDataInputCount();
			if (xNode.m_iDynamicDataInputCount > 255)
			{
				xNode.m_iDynamicDataInputCount = 255;
			}

			delete pxTemp;
		}

		if (xNode.m_bOpaque)
		{
			bAnyOpaqueNode = true;
		}
		axNodes.PushBack(xNode);
	}

	//--------------------------------------------------------------------------
	// Pass 1 - structural. Always reported: these do not depend on any node
	// carrying a pin table, so opacity never suppresses them.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetEdgeCount(); ++u)
	{
		const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(u);
		const Zenith_GraphNodeDef* pxSrc = xDefinition.FindNodeDef(xEdge.m_uSrcNodeID);
		const Zenith_GraphNodeDef* pxDst = xDefinition.FindNodeDef(xEdge.m_uDstNodeID);

		if (pxSrc == nullptr || pxDst == nullptr)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_ORPHAN_EDGE,
				pxSrc ? xEdge.m_uSrcNodeID : xEdge.m_uDstNodeID,
				pxSrc ? pxSrc->m_strTypeName.c_str() : (pxDst ? pxDst->m_strTypeName.c_str() : ""),
				"", "",
				"edge %u:%u -> %u names a node that is not in this graph",
				xEdge.m_uSrcNodeID, xEdge.m_uSrcPin, xEdge.m_uDstNodeID);
			continue;
		}

		const u_int uOutputs = xRegistry.GetExecOutputCount(xDefinition, xEdge.m_uSrcNodeID);
		if (xEdge.m_uSrcPin >= uOutputs)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE,
				xEdge.m_uSrcNodeID, pxSrc->m_strTypeName.c_str(), "", "",
				"edge leaves pin %u, but the node has %u exec output pin(s)", xEdge.m_uSrcPin, uOutputs);
		}

		// An exec edge INTO a PURE node. B-2 DROPS it at instantiation (a pure
		// node has no exec input and no chain lifecycle), so the chain silently
		// ends one node early; here it is an author-time error.
		//
		// ★ There is no rule for an exec edge OUT of a pure node, deliberately:
		// a surviving PURE flag forces m_uExecOutputCount to 0, so the range
		// check above already reports it as PIN_OUT_OF_RANGE.
		const ValidatorNode* pxDstNode = FindValidatorNode(axNodes, xEdge.m_uDstNodeID);
		if (pxDstNode != nullptr && pxDstNode->m_bPure)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_EXEC_INTO_PURE,
				xEdge.m_uDstNodeID, pxDst->m_strTypeName.c_str(), "", "",
				"exec edge %u:%u -> %u: '%s' is a PURE node and has no exec input (the runtime drops this wire)",
				xEdge.m_uSrcNodeID, xEdge.m_uSrcPin, xEdge.m_uDstNodeID, pxDst->m_strTypeName.c_str());
		}
	}

	//--------------------------------------------------------------------------
	// Pass 1b - WIRES. Every data edge resolved to a real pin, of the right
	// ROLE, on a real node, with agreeing endpoint TYPES.
	//
	// At most ONE finding per edge: the first defect found is reported and the
	// edge is abandoned, so "one finding per defect" holds and a test can assert
	// an exact count.
	//--------------------------------------------------------------------------
	for (u_int u = 0; u < xDefinition.GetDataEdgeCount(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(u);

		// A MALFORMED edge (node id 0, a self-loop, an empty pin name) is already
		// one finding from the load-safety body above; resolving it here would
		// report the same mistake twice.
		if (xEdge.m_uSrcNodeID == 0 || xEdge.m_uDstNodeID == 0 || xEdge.m_uSrcNodeID == xEdge.m_uDstNodeID
			|| xEdge.m_strSrcPin.empty() || xEdge.m_strDstPin.empty())
		{
			continue;
		}

		// (a) both endpoints must be nodes of this graph.
		const Zenith_GraphNodeDef* pxSrcDef = xDefinition.FindNodeDef(xEdge.m_uSrcNodeID);
		const Zenith_GraphNodeDef* pxDstDef = xDefinition.FindNodeDef(xEdge.m_uDstNodeID);
		if (pxSrcDef == nullptr || pxDstDef == nullptr)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_ORPHAN_EDGE,
				pxSrcDef ? xEdge.m_uSrcNodeID : xEdge.m_uDstNodeID,
				pxSrcDef ? pxSrcDef->m_strTypeName.c_str() : (pxDstDef ? pxDstDef->m_strTypeName.c_str() : ""),
				"", "",
				"data edge %u:'%s' -> %u:'%s' names a node that is not in this graph",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str());
			continue;
		}

		ValidatorNode* pxSrcNode = FindValidatorNodeMutable(axNodes, xEdge.m_uSrcNodeID);
		ValidatorNode* pxDstNode = FindValidatorNodeMutable(axNodes, xEdge.m_uDstNodeID);
		if (pxSrcNode == nullptr || pxDstNode == nullptr)
		{
			continue;	// unreachable: pass 0 built one entry per node def
		}

		// (b) an UNREGISTERED endpoint is skipped SILENTLY - a per-game node
		//     library this exe does not carry. The instantiation warning covers
		//     it, and the wired-input record below is deliberately NOT taken, so
		//     the destination's var-name check stays ON (the runtime falls back
		//     to the var name there too).
		if (!pxSrcNode->m_bRegistered || !pxDstNode->m_bRegistered)
		{
			continue;
		}

		// (c) a REGISTERED endpoint with NO pin table. See the header: this
		//     supersedes the opaque doctrine for WIRES only, because
		//     ResolveDataEdges skips exactly this wire - it can never carry a
		//     value, so reporting it is not a false finding.
		if (pxSrcNode->m_bOpaque || pxDstNode->m_bOpaque)
		{
			const ValidatorNode& xBlame = pxSrcNode->m_bOpaque ? *pxSrcNode : *pxDstNode;
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN,
				xBlame.m_uNodeID, xBlame.m_strTypeName.c_str(),
				pxSrcNode->m_bOpaque ? xEdge.m_strSrcPin.c_str() : xEdge.m_strDstPin.c_str(), "",
				"wire %u:'%s' -> %u:'%s': node type '%s' declares no pins, so this wire can never bind",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
				xBlame.m_strTypeName.c_str());
			continue;
		}

		// (d) the SOURCE pin: an exact OUTPUT name, never an ordinal (there are
		//     no variadic outputs).
		const u_int uSrcPin = pxSrcNode->m_pxPins->FindPinIndex(xEdge.m_strSrcPin.c_str());
		if (uSrcPin >= pxSrcNode->m_pxPins->GetPinCount())
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN,
				xEdge.m_uSrcNodeID, pxSrcNode->m_strTypeName.c_str(), xEdge.m_strSrcPin.c_str(), "",
				"wire source: '%s' declares no pin '%s'",
				pxSrcNode->m_strTypeName.c_str(), xEdge.m_strSrcPin.c_str());
			continue;
		}
		if (pxSrcNode->m_pxPins->GetPinAt(uSrcPin).m_eRole != GRAPH_PIN_ROLE_OUTPUT)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH,
				xEdge.m_uSrcNodeID, pxSrcNode->m_strTypeName.c_str(), xEdge.m_strSrcPin.c_str(), "",
				"wire source: '%s'.'%s' is a real pin but is not an OUTPUT",
				pxSrcNode->m_strTypeName.c_str(), xEdge.m_strSrcPin.c_str());
			continue;
		}

		// (d) the DESTINATION pin: an exact name first, then a variadic ordinal
		//     inside the param-applied instance's member count - B-2's algorithm,
		//     BARE-FAMILY refusal included (a family has no non-ordinal member, so
		//     a wire naming it could never be addressed).
		u_int uDstPin = pxDstNode->m_pxPins->FindPinIndex(xEdge.m_strDstPin.c_str());
		bool bDstUnknown = false;
		const char* szDstWhy = "";
		if (uDstPin < pxDstNode->m_pxPins->GetPinCount())
		{
			if (pxDstNode->m_pxPins->GetPinAt(uDstPin).m_bVariadic)
			{
				bDstUnknown = true;
				szDstWhy = "is a variadic FAMILY name; a wire must name a member ('<family>0', '<family>1', ...)";
			}
		}
		else
		{
			std::string strFamily;
			u_int uOrdinal = 0;
			bool bOverflowed = false;
			const bool bSplit = SplitVariadicMemberName(xEdge.m_strDstPin, strFamily, uOrdinal, bOverflowed);
			const u_int uFamilyPin = bSplit ? pxDstNode->m_pxPins->FindPinIndex(strFamily.c_str())
				: pxDstNode->m_pxPins->GetPinCount();
			if (!bSplit || uFamilyPin >= pxDstNode->m_pxPins->GetPinCount()
				|| !pxDstNode->m_pxPins->GetPinAt(uFamilyPin).m_bVariadic
				|| pxDstNode->m_bVariadicNameCollision)
			{
				bDstUnknown = true;
				szDstWhy = "names no declared pin and no variadic family";
			}
			else if (bOverflowed || pxDstNode->m_iDynamicDataInputCount < 0
				|| uOrdinal >= static_cast<u_int>(pxDstNode->m_iDynamicDataInputCount))
			{
				bDstUnknown = true;
				szDstWhy = "names a variadic ordinal past the configured member count";
			}
			else
			{
				uDstPin = uFamilyPin;
			}
		}
		if (bDstUnknown)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN,
				xEdge.m_uDstNodeID, pxDstNode->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), "",
				"wire destination: '%s'.'%s' %s",
				pxDstNode->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), szDstWhy);
			continue;
		}
		if (pxDstNode->m_pxPins->GetPinAt(uDstPin).m_eRole != GRAPH_PIN_ROLE_INPUT)
		{
			AddFinding(axOut, true, GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH,
				xEdge.m_uDstNodeID, pxDstNode->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), "",
				"wire destination: '%s'.'%s' is a real pin but is not an INPUT",
				pxDstNode->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str());
			continue;
		}

		// Both endpoints and both pins resolved - EXACTLY the conditions under
		// which the runtime sets m_bConnected. Record the wired input so pass 3
		// skips its var-name check: the WIRE supersedes the fallback binding that
		// the C-1 sweep deletes.
		if (uDstPin < pxDstNode->m_axPins.GetSize())
		{
			pxDstNode->m_axPins.Get(uDstPin).m_bWired = true;
		}

		// (e) type agreement between the two RESOLVED types. Equal, or either
		//     ANY, is fine - "ANY unifies" is exact, and there is deliberately NO
		//     warning for an ANY output feeding a typed input: the residual
		//     value-dependent case is B-2's runtime [GraphPin] MISMATCH census.
		const Zenith_PropertyType eSrcType = pxSrcNode->m_axPins.Get(uSrcPin).m_eType;
		const Zenith_PropertyType eDstType = pxDstNode->m_axPins.Get(uDstPin).m_eType;
		if (eSrcType == eGRAPH_PIN_TYPE_ANY || eDstType == eGRAPH_PIN_TYPE_ANY || eSrcType == eDstType)
		{
			continue;
		}
		// ★ NO DOUBLE-COUNTING. A static-vs-static disagreement is already one
		// finding from the load-safety body; only a type that needed PARAMS or a
		// DECLARATION to resolve is reported here.
		const Zenith_GraphPinDesc& xSrcDesc = *pxSrcNode->m_axPins.Get(uSrcPin).m_pxDesc;
		const Zenith_GraphPinDesc& xDstDesc = *pxDstNode->m_axPins.Get(uDstPin).m_pxDesc;
		const bool bResolvedEndpoint = xSrcDesc.m_bInstanceResolved || xDstDesc.m_bInstanceResolved
			|| PinHasTypeFromVariable(xSrcDesc) || PinHasTypeFromVariable(xDstDesc);
		if (!bResolvedEndpoint)
		{
			continue;
		}
		AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
			xEdge.m_uDstNodeID, pxDstNode->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), "",
			"wire %u:'%s' (%s) -> %u:'%s' (%s): the endpoint types disagree",
			xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), TypeName(eSrcType),
			xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(), TypeName(eDstType));
	}

	//--------------------------------------------------------------------------
	// Pass 1c - a PURE node nothing consumes. A pure node has no exec pins, so
	// it runs only when a consumer gathers one of its outputs: with no outgoing
	// data edge it can never run at all.
	//--------------------------------------------------------------------------
	for (u_int uNode = 0; uNode < axNodes.GetSize(); ++uNode)
	{
		const ValidatorNode& xNode = axNodes.Get(uNode);
		if (!xNode.m_bPure)
		{
			continue;
		}
		bool bConsumed = false;
		for (u_int uEdge = 0; uEdge < xDefinition.GetDataEdgeCount() && !bConsumed; ++uEdge)
		{
			bConsumed = xDefinition.GetDataEdgeAt(uEdge).m_uSrcNodeID == xNode.m_uNodeID;
		}
		if (!bConsumed)
		{
			AddFinding(axOut, false, GRAPH_VALIDATION_RULE_PURE_UNCONSUMED,
				xNode.m_uNodeID, xNode.m_strTypeName.c_str(), "", "",
				"'%s' is a PURE node with no outgoing wire, so nothing can ever evaluate it",
				xNode.m_strTypeName.c_str());
		}
	}

	//--------------------------------------------------------------------------
	// Pass 2 - collect the annotated writers and every referenced name.
	//--------------------------------------------------------------------------
	Zenith_Vector<ValidatorWriter> axWriters;
	Zenith_Vector<std::string> axReferenced;

	for (u_int uNode = 0; uNode < axNodes.GetSize(); ++uNode)
	{
		const ValidatorNode& xNode = axNodes.Get(uNode);
		for (u_int uPin = 0; uPin < xNode.m_axPins.GetSize(); ++uPin)
		{
			const ValidatorPin& xPin = xNode.m_axPins.Get(uPin);
			if (xPin.m_strVar.empty())
			{
				continue;	// an unbound pin (a const-only INPUT, or an empty name) is skipped
			}
			if (!VectorContainsString(axReferenced, xPin.m_strVar))
			{
				axReferenced.PushBack(xPin.m_strVar);
			}
			if (PinRoleWrites(xPin.m_pxDesc->m_eRole))
			{
				ValidatorWriter xWriter;
				xWriter.m_strVar = xPin.m_strVar;
				xWriter.m_uNodeID = xNode.m_uNodeID;
				xWriter.m_eType = xPin.m_eType;
				axWriters.PushBack(xWriter);
			}
		}
	}

	//--------------------------------------------------------------------------
	// Pass 3 - the read + type checks.
	//--------------------------------------------------------------------------
	for (u_int uNode = 0; uNode < axNodes.GetSize(); ++uNode)
	{
		const ValidatorNode& xNode = axNodes.Get(uNode);
		for (u_int uPin = 0; uPin < xNode.m_axPins.GetSize(); ++uPin)
		{
			const ValidatorPin& xPin = xNode.m_axPins.Get(uPin);
			const Zenith_GraphPinDesc& xDesc = *xPin.m_pxDesc;
			if (xPin.m_strVar.empty())
			{
				continue;
			}

			// ★ A WIRE SUPERSEDES THE FALLBACK the C-1 sweep deletes. Pass 1b
			// resolved a data edge into this input under exactly the conditions
			// that make the runtime set m_bConnected, and a CONNECTED input never
			// consults its var name (Zenith_GraphNode::ResolveInput takes the pull
			// path), so neither declare-or-error nor type agreement applies to a
			// name nothing reads.
			if (xPin.m_bWired)
			{
				continue;
			}

			if (xDesc.m_eRole == GRAPH_PIN_ROLE_LIST)
			{
				// Lists live in the blackboard's parallel store: never declared,
				// never typed, created on first use. Informational only.
				AddFinding(axOut, false, GRAPH_VALIDATION_RULE_LIST_NAME,
					xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
					"blackboard list name (lists are runtime-only and never declared)");
				continue;
			}

			if (!PinRoleReads(xDesc.m_eRole))
			{
				continue;
			}

			const Zenith_GraphVariableDecl* pxDecl = FindDeclaredVariable(xDefinition, xPin.m_strVar);

			// --- declare-or-error -------------------------------------------
			bool bSatisfied = pxDecl != nullptr;
			bool bSelfWriterOnly = false;
			for (u_int uWriter = 0; uWriter < axWriters.GetSize(); ++uWriter)
			{
				const ValidatorWriter& xWriter = axWriters.Get(uWriter);
				if (xWriter.m_strVar != xPin.m_strVar)
				{
					continue;
				}
				if (xWriter.m_uNodeID != xNode.m_uNodeID)
				{
					bSatisfied = true;	// SOME other node writes it - that is a writer
				}
				else
				{
					bSelfWriterOnly = true;
				}
			}
			if (!bSatisfied)
			{
				if (bSelfWriterOnly)
				{
					// A READWRITE reference cannot satisfy its own read: the
					// value it writes is a function of the value it read.
					AddFinding(axOut, true, GRAPH_VALIDATION_RULE_SELF_READWRITE,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"reads '%s', which nothing declares and only this node writes - a read-modify-write cannot seed itself",
						xPin.m_strVar.c_str());
				}
				else
				{
					AddFinding(axOut, true, GRAPH_VALIDATION_RULE_UNDECLARED_READ,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"reads '%s', which this graph neither declares nor writes", xPin.m_strVar.c_str());
				}
			}

			// --- type agreement ---------------------------------------------
			if (xDesc.m_eRole == GRAPH_PIN_ROLE_TARGET_REF && xDesc.m_uAcceptedTypeMask != uGRAPH_PIN_ACCEPT_ANY)
			{
				if (pxDecl != nullptr
					&& (xDesc.m_uAcceptedTypeMask & Zenith_GraphPinTypeMaskBit(pxDecl->m_xDefault.GetType())) == 0u)
				{
					AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"target reference '%s' is declared %s, which this pin's resolver does not accept",
						xPin.m_strVar.c_str(), TypeName(pxDecl->m_xDefault.GetType()));
				}
				for (u_int uWriter = 0; uWriter < axWriters.GetSize(); ++uWriter)
				{
					const ValidatorWriter& xWriter = axWriters.Get(uWriter);
					if (xWriter.m_strVar != xPin.m_strVar || xWriter.m_eType == eGRAPH_PIN_TYPE_ANY)
					{
						continue;
					}
					if ((xDesc.m_uAcceptedTypeMask & Zenith_GraphPinTypeMaskBit(xWriter.m_eType)) == 0u)
					{
						AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
							xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
							"target reference '%s' is written as %s by node %u, which this pin's resolver does not accept",
							xPin.m_strVar.c_str(), TypeName(xWriter.m_eType), xWriter.m_uNodeID);
					}
				}
			}
			else if (xPin.m_eType != eGRAPH_PIN_TYPE_ANY)
			{
				if (pxDecl != nullptr && pxDecl->m_xDefault.GetType() != xPin.m_eType)
				{
					AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
						xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
						"reads '%s' as %s, but the graph declares it %s",
						xPin.m_strVar.c_str(), TypeName(xPin.m_eType), TypeName(pxDecl->m_xDefault.GetType()));
				}
				for (u_int uWriter = 0; uWriter < axWriters.GetSize(); ++uWriter)
				{
					const ValidatorWriter& xWriter = axWriters.Get(uWriter);
					if (xWriter.m_strVar != xPin.m_strVar || xWriter.m_eType == eGRAPH_PIN_TYPE_ANY)
					{
						continue;	// ANY unifies with everything
					}
					if (xWriter.m_eType != xPin.m_eType)
					{
						AddFinding(axOut, true, GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
							xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
							"reads '%s' as %s, but node %u writes it as %s",
							xPin.m_strVar.c_str(), TypeName(xPin.m_eType), xWriter.m_uNodeID, TypeName(xWriter.m_eType));
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------
	// Pass 4 - declared but never referenced.
	//
	// ★ SUPPRESSED while ANY node in the graph is opaque. An opaque node's reads
	// are invisible, so "nothing references this" would be a claim the validator
	// cannot make - and every declaration in every shipped graph would warn.
	//--------------------------------------------------------------------------
	if (!bAnyOpaqueNode)
	{
		for (u_int u = 0; u < xDefinition.GetVariableCount(); ++u)
		{
			const Zenith_GraphVariableDecl& xDecl = xDefinition.GetVariableAt(u);
			if (!VectorContainsString(axReferenced, xDecl.m_strName))
			{
				AddFinding(axOut, false, GRAPH_VALIDATION_RULE_DECLARED_UNUSED,
					0, "", "", xDecl.m_strName.c_str(),
					"variable '%s' is declared (%s) but no annotated pin references it",
					xDecl.m_strName.c_str(), TypeName(xDecl.m_xDefault.GetType()));
			}
		}
	}

	//--------------------------------------------------------------------------
	// Pass 5 - DOMINANCE. A wire reads the producer's LATCHED slot, so a
	// consumer that can execute before its producer has ever run reads the
	// slot's default and nothing at runtime says so.
	//
	// ★ CONSERVATIVE, AND NEVER AN ERROR. The question asked is exactly:
	// "with the producer removed from the exec graph, is the consumer still
	// reachable from an event source?" It models neither Sequence's branch
	// ORDER nor reactive preemption, both of which can make a reachable
	// consumer unreachable in practice - which is why a false positive here
	// must stay a warning. BOUNDED at one finding per PRODUCER, naming the
	// first consumer and how many there were.
	//--------------------------------------------------------------------------
	BuildDominanceWarnings(xDefinition, axNodes, axOut);
}

void Zenith_GraphDefinitionValidator::LogFindings(const char* szGraphName, u_int uNodeCount,
	const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings)
{
	const char* szGraph = (szGraphName && szGraphName[0] != '\0') ? szGraphName : "<unnamed>";

	u_int uErrors = 0;
	u_int uWarnings = 0;
	for (u_int u = 0; u < axFindings.GetSize(); ++u)
	{
		const Zenith_GraphValidationFinding& xFinding = axFindings.Get(u);
		const bool bError = xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR;
		if (bError)
		{
			++uErrors;
		}
		else
		{
			++uWarnings;
		}

		// The <SEV> token carries the severity: an ERROR is a real defect and
		// goes to Zenith_Error, a WARNING stays on Zenith_Log.
		if (bError)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"[GraphValidator] %s graph=%s node=%u:%s pin=%s var=%s rule=%s | %s",
				GetSeverityName(xFinding.m_eSeverity),
				szGraph,
				xFinding.m_uNodeID,
				xFinding.m_strTypeName.empty() ? "-" : xFinding.m_strTypeName.c_str(),
				xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
				xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
				GetRuleName(xFinding.m_eRule),
				xFinding.m_strWhat.c_str());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphValidator] %s graph=%s node=%u:%s pin=%s var=%s rule=%s | %s",
				GetSeverityName(xFinding.m_eSeverity),
				szGraph,
				xFinding.m_uNodeID,
				xFinding.m_strTypeName.empty() ? "-" : xFinding.m_strTypeName.c_str(),
				xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
				xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
				GetRuleName(xFinding.m_eRule),
				xFinding.m_strWhat.c_str());
		}
	}

	// findings=<errors>/<warnings>
	if (uErrors > 0)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "[GraphValidator] graph=%s nodes=%u findings=%u/%u",
			szGraph, uNodeCount, uErrors, uWarnings);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_CORE, "[GraphValidator] graph=%s nodes=%u findings=%u/%u",
			szGraph, uNodeCount, uErrors, uWarnings);
	}
}

bool Zenith_GraphDefinitionValidator::ResolvePinType(const Zenith_GraphDefinition& xDefinition,
	const Zenith_GraphNodeRegistry& xRegistry, u_int uNodeID, u_int uPinIndex, Zenith_PropertyType& eOut)
{
	eOut = eGRAPH_PIN_TYPE_ANY;

	const Zenith_GraphNodeDef* pxNodeDef = xDefinition.FindNodeDef(uNodeID);
	if (pxNodeDef == nullptr)
	{
		return false;
	}
	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(pxNodeDef->m_strTypeName.c_str());
	if (pxInfo == nullptr || pxInfo->m_pfnCreate == nullptr || pxInfo->m_pfnGetPinTable == nullptr)
	{
		return false;	// unregistered here, or opaque: there is no pin to type
	}
	const Zenith_GraphPinTable* pxPins = pxInfo->m_pfnGetPinTable();
	if (pxPins == nullptr || uPinIndex >= pxPins->GetPinCount())
	{
		return false;
	}
	const Zenith_GraphPinDesc& xDesc = pxPins->GetPinAt(uPinIndex);

	// A STATIC type needs neither an instance nor the property table, so the
	// allocation the header warns about is paid only where it is unavoidable.
	if (!xDesc.m_bInstanceResolved && !PinHasTypeFromVariable(xDesc))
	{
		eOut = xDesc.m_eType;
		return true;
	}

	const Zenith_PropertyTable* pxProperties = pxInfo->m_pfnGetPropertyTable
		? pxInfo->m_pfnGetPropertyTable() : nullptr;
	Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
	xDefinition.ApplyNodeParams(uNodeID, pxTemp, *pxInfo);
	bool bDeclined = false;
	eOut = ResolveOnePinType(xDefinition, xDesc, uPinIndex, pxProperties, pxTemp, bDeclined);
	delete pxTemp;
	return true;	// a DECLINED instance answer is still an answer: the pin is ANY
}

const char* Zenith_GraphDefinitionValidator::GetRuleName(Zenith_GraphValidationRule eRule)
{
	switch (eRule)
	{
	case GRAPH_VALIDATION_RULE_UNDECLARED_READ:          return "UNDECLARED_READ";
	case GRAPH_VALIDATION_RULE_TYPE_MISMATCH:            return "TYPE_MISMATCH";
	case GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE:         return "PIN_OUT_OF_RANGE";
	case GRAPH_VALIDATION_RULE_ORPHAN_EDGE:              return "ORPHAN_EDGE";
	case GRAPH_VALIDATION_RULE_DECLARED_UNUSED:          return "DECLARED_UNUSED";
	case GRAPH_VALIDATION_RULE_LIST_NAME:                return "LIST_NAME";
	case GRAPH_VALIDATION_RULE_SELF_READWRITE:           return "SELF_READWRITE";
	case GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED: return "INSTANCE_TYPE_UNRESOLVED";
	case GRAPH_VALIDATION_RULE_PIN_BINDING_INVALID:      return "PIN_BINDING_INVALID";
	case GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE:    return "DUPLICATE_EXEC_SOURCE";
	case GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT:     return "DUPLICATE_DATA_INPUT";
	case GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED:      return "DATA_EDGE_MALFORMED";
	case GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN:         return "WIRE_PIN_UNKNOWN";
	case GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH:       return "WIRE_ROLE_MISMATCH";
	case GRAPH_VALIDATION_RULE_EXEC_INTO_PURE:           return "EXEC_INTO_PURE";
	case GRAPH_VALIDATION_RULE_DATA_CYCLE:               return "DATA_CYCLE";
	case GRAPH_VALIDATION_RULE_DOMINANCE:                return "DOMINANCE";
	case GRAPH_VALIDATION_RULE_PURE_UNCONSUMED:          return "PURE_UNCONSUMED";
	case GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING:        return "IN_PLACE_ALIASING";
	default:                                             return "UNKNOWN";
	}
}

const char* Zenith_GraphDefinitionValidator::GetSeverityName(Zenith_GraphValidationSeverity eSeverity)
{
	return eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR ? "ERROR" : "WARN";
}

// Unit tests for the pin table, the registry funnel, and this validator.
// Included unconditionally; the .inl guards its own body with ZENITH_TESTING.
#include "Scripting/Zenith_GraphDefinitionValidator.Tests.inl"
