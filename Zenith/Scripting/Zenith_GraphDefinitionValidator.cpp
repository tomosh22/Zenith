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
	};

	struct ValidatorNode
	{
		u_int m_uNodeID = 0;
		std::string m_strTypeName;
		bool m_bOpaque = true;
		Zenith_Vector<ValidatorPin> m_axPins;
	};

	struct ValidatorWriter
	{
		std::string m_strVar;
		u_int m_uNodeID = 0;
		Zenith_PropertyType m_eType = eGRAPH_PIN_TYPE_ANY;
	};

	enum ReadPropertyResult : u_int8
	{
		READ_PROPERTY_NOT_BOUND = 0,	// the descriptor names no property - legal, skipped
		READ_PROPERTY_OK,
		READ_PROPERTY_INVALID			// the named property is missing or is not a string
	};

	// Reads a declared std::string property off a live instance THROUGH the
	// property table. The tagged getters ASSERT on a type mismatch
	// (Zenith_PropertySystem.h) and an assert DebugBreaks a developer, so the tag
	// is checked BEFORE GetString: a mis-declared pin table yields a finding,
	// never a break.
	ReadPropertyResult ReadStringProperty(const Zenith_PropertyTable* pxTable, const Zenith_GraphNode* pxNode,
		const char* szProperty, std::string& strOut)
	{
		if (szProperty == nullptr || szProperty[0] == '\0')
		{
			return READ_PROPERTY_NOT_BOUND;
		}
		if (pxTable == nullptr || pxNode == nullptr)
		{
			return READ_PROPERTY_INVALID;
		}
		const Zenith_ReflectedProperty* pxProperty = pxTable->FindProperty(szProperty);
		if (pxProperty == nullptr || pxProperty->m_pfnGet == nullptr || pxProperty->m_eType != PROPERTY_TYPE_STRING)
		{
			return READ_PROPERTY_INVALID;
		}
		Zenith_PropertyValue xValue;
		pxProperty->m_pfnGet(pxNode, xValue);
		if (xValue.GetType() != PROPERTY_TYPE_STRING)
		{
			return READ_PROPERTY_INVALID;
		}
		strOut = xValue.GetString();
		return READ_PROPERTY_OK;
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

void Zenith_GraphDefinitionValidator::AppendLoadSafetyFindings(const Zenith_GraphDefinition& xDefinition,
	Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
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
}

void Zenith_GraphDefinitionValidator::ValidateLoadSafety(const Zenith_GraphDefinition& xDefinition,
	Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
{
	axOut.Clear();
	AppendLoadSafetyFindings(xDefinition, axOut);
}

void Zenith_GraphDefinitionValidator::Validate(const Zenith_GraphDefinition& xDefinition,
	const Zenith_GraphNodeRegistry& xRegistry, const char* szGraphName,
	Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
{
	(void)szGraphName;	// the graph name travels on the LOG line, not on a per-node finding
	axOut.Clear();

	// The LOAD_SAFETY subset first, through the APPENDING helper - one report
	// covers both tiers, and the clearing entry point is never called from here.
	AppendLoadSafetyFindings(xDefinition, axOut);

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
		const Zenith_GraphPinTable* pxPins = (pxInfo && pxInfo->m_pfnCreate && pxInfo->m_pfnGetPinTable)
			? pxInfo->m_pfnGetPinTable() : nullptr;

		if (pxPins != nullptr && pxPins->GetPinCount() > 0)
		{
			xNode.m_bOpaque = false;

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
					}
				}
				xPin.m_strVar = strVar;

				// --- the pin's resolved type ---
				xPin.m_eType = xDesc.m_eType;
				if (xDesc.m_bInstanceResolved)
				{
					Zenith_PropertyType eResolved = eGRAPH_PIN_TYPE_ANY;
					if (pxTemp->GetPinType(uPin, eResolved) && eResolved < PROPERTY_TYPE_COUNT)
					{
						xPin.m_eType = eResolved;
					}
					else
					{
						// Never fabricate a type: ANY plus one warning naming the type.
						// An UNBOUND pin (empty var name) takes part in no later pass,
						// so it earns no warning either - a blank in-place result var
						// is the normal shape, not a defect.
						xPin.m_eType = eGRAPH_PIN_TYPE_ANY;
						if (!xPin.m_strVar.empty())
						AddFinding(axOut, false, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED,
							xNode.m_uNodeID, xNode.m_strTypeName.c_str(), xDesc.m_szName, xPin.m_strVar.c_str(),
							"instance-resolved pin: '%s' declined to answer GetPinType, treating the pin as ANY",
							xNode.m_strTypeName.c_str());
					}
				}

				xNode.m_axPins.PushBack(xPin);
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
