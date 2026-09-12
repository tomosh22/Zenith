#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_BehaviourGraph.h"	// GetExecOutputCount reads a definition's node defs + param blobs

Zenith_GraphNodeRegistry& Zenith_GraphNodeRegistry::Get()
{
	// Construct-on-first-use - safe under static init order, same shape as
	// Zenith_ComponentMetaRegistry::Get().
	static Zenith_GraphNodeRegistry ls_xRegistry;
	return ls_xRegistry;
}

void Zenith_GraphNodeRegistry::Register(const Zenith_GraphNodeTypeInfo& xInfo)
{
	Zenith_Assert(!xInfo.m_strTypeName.empty(), "GraphNodeRegistry: empty type name");
	Zenith_Assert(xInfo.m_pfnCreate != nullptr, "GraphNodeRegistry: '%s' has no create function", xInfo.m_strTypeName.c_str());

	if (Find(xInfo.m_strTypeName.c_str()) != nullptr)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphNodeRegistry: duplicate node type '%s' ignored", xInfo.m_strTypeName.c_str());
		return;
	}

	// The failure-pin flag is VALIDATED here, not asserted: an assert
	// DebugBreaks a developer and vanishes in Release, so a refusal nothing can
	// observe would be indistinguishable from an honoured flag. A refused flag
	// is reported and FORCED false, and the stored type info is what every
	// caller (runtime + editor) reads back.
	Zenith_GraphNodeTypeInfo xValidated = xInfo;
	if (xValidated.m_bHasFailurePin)
	{
		if (xValidated.m_bFlowNode)
		{
			// A flow node's FAILURE is the propagated status of the sub-chain it
			// ran itself (Branch/Loop/Repeat/ForEach/StateMachine/Selector), so
			// "Branch > On Failure" would mean "the child failed" - a different
			// thing wearing the same wire.
			Zenith_Error(LOG_CATEGORY_CORE,
				"GraphNodeRegistry: '%s' requested a failure pin but is a flow node; flag refused",
				xValidated.m_strTypeName.c_str());
			xValidated.m_bHasFailurePin = false;
		}
		else if (xValidated.m_eEventType != GRAPH_EVENT_NONE)
		{
			// An event SOURCE gates itself: its own FAILURE returns from
			// RunSourceNode before any chain is walked, so a failure wire on it
			// could never be consulted - the editor would draw a pin that is
			// dead by construction.
			Zenith_Error(LOG_CATEGORY_CORE,
				"GraphNodeRegistry: '%s' requested a failure pin but is an event source; its FAILURE is a gate, not a chain outcome, flag refused",
				xValidated.m_strTypeName.c_str());
			xValidated.m_bHasFailurePin = false;
		}
		else if (xValidated.m_uExecOutputCount >= 255)
		{
			// The chain-cursor key packs the pin into its low byte
			// (Zenith_BehaviourGraph::MakeChainKey), and the editor's pin key
			// masks to 0xFF - index 255 and up cannot be addressed.
			Zenith_Error(LOG_CATEGORY_CORE,
				"GraphNodeRegistry: '%s' requested a failure pin at index %u; pins are capped at 255, flag refused",
				xValidated.m_strTypeName.c_str(), xValidated.m_uExecOutputCount);
			xValidated.m_bHasFailurePin = false;
		}
		else if (xValidated.m_pfnCreate == nullptr)
		{
			// Without a create fn the dynamic-pin probe below cannot run, and an
			// UNCHECKED flag must not read as an honoured one.
			Zenith_Error(LOG_CATEGORY_CORE,
				"GraphNodeRegistry: '%s' requested a failure pin but has no create fn to validate against; flag refused",
				xValidated.m_strTypeName.c_str());
			xValidated.m_bHasFailurePin = false;
		}
		else
		{
			// GetDynamicExecOutputCount is a non-static virtual, so the only way
			// to ask a TYPE is to build one. Registration-time only (once per
			// type, at boot), never per frame.
			Zenith_GraphNode* pxTemp = xValidated.m_pfnCreate();
			const int32_t iDynamic = pxTemp->GetDynamicExecOutputCount();
			delete pxTemp;
			if (iDynamic >= 0)
			{
				Zenith_Error(LOG_CATEGORY_CORE,
					"GraphNodeRegistry: '%s' requested a failure pin but reports %d dynamic exec pins; the failure index would move with the branch count, flag refused",
					xValidated.m_strTypeName.c_str(), iDynamic);
				xValidated.m_bHasFailurePin = false;
			}
		}
	}

	m_axTypes.PushBack(xValidated);
}

const Zenith_GraphNodeTypeInfo* Zenith_GraphNodeRegistry::Find(const char* szTypeName) const
{
	if (!szTypeName)
	{
		return nullptr;
	}
	for (u_int u = 0; u < m_axTypes.GetSize(); ++u)
	{
		if (m_axTypes.Get(u).m_strTypeName == szTypeName)
		{
			return &m_axTypes.Get(u);
		}
	}
	return nullptr;
}

u_int Zenith_GraphNodeRegistry::GetTypeCount() const
{
	return m_axTypes.GetSize();
}

const Zenith_GraphNodeTypeInfo& Zenith_GraphNodeRegistry::GetTypeAt(u_int uIndex) const
{
	Zenith_Assert(uIndex < m_axTypes.GetSize(), "GraphNodeRegistry: index %u out of range", uIndex);
	return m_axTypes.Get(uIndex);
}

u_int Zenith_GraphNodeRegistry::GetExecOutputCount(const Zenith_GraphDefinition& xDefinition, u_int uNodeID) const
{
	const Zenith_GraphNodeDef* pxNodeDef = xDefinition.FindNodeDef(uNodeID);
	const Zenith_GraphNodeTypeInfo* pxInfo = pxNodeDef ? Find(pxNodeDef->m_strTypeName.c_str()) : nullptr;
	if (!pxInfo || pxInfo->m_pfnCreate == nullptr)
	{
		return 1;
	}

	// Apply the params BEFORE the first GetDynamicExecOutputCount call and ask
	// exactly once. A node may CACHE what it derives from its params on that
	// first call (SwitchOnString parses m_strCases lazily and latches
	// m_bCasesParsed), so probing the default instance first and applying the
	// params afterwards reports the DEFAULT count forever - measured as three
	// PIN_OUT_OF_RANGE findings on every ST_Dispenser build, and one drawn pin
	// where the editor should draw four.
	Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
	xDefinition.ApplyNodeParams(uNodeID, pxTemp, *pxInfo);
	const int32_t iDynamic = pxTemp->GetDynamicExecOutputCount();
	delete pxTemp;
	if (iDynamic < 0)
	{
		// Static-pin type: + the routable failure pin when the type carries one.
		// Deliberately NOT added inside GetDynamicExecOutputCount - that is the
		// NODE's answer about its own branch count, and a dynamic-pin type
		// cannot carry the flag anyway.
		return pxInfo->m_uExecOutputCount + (pxInfo->m_bHasFailurePin ? 1u : 0u);
	}
	return static_cast<u_int>(iDynamic > 255 ? 255 : iDynamic);
}

void Zenith_GraphNodeRegistry::SetNodeRegistrar(void (*pfnRegistrar)())
{
	m_pfnRegistrar = pfnRegistrar;
}

void Zenith_GraphNodeRegistry::EnsureInitialized()
{
	if (m_bInitialized)
	{
		return;
	}
	m_bInitialized = true;	// set first - registrar bodies may query the registry
	if (m_pfnRegistrar)
	{
		m_pfnRegistrar();
	}
}

void Zenith_GraphNodeRegistry::ResetForTests()
{
	m_axTypes.Clear();
	m_bInitialized = false;
}
