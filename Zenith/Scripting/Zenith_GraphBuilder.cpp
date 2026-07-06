#include "Zenith.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Collections/Zenith_HashMap.h"

Zenith_GraphBuilder::Zenith_GraphBuilder(Zenith_GraphDefinition& xDefinition)
	: m_xDefinition(xDefinition)
{
}

Zenith_GraphBuilder::~Zenith_GraphBuilder()
{
	// Build() frees committed instances; anything left here is an
	// abandoned-without-Build builder (or a failed Node()) - just free.
	for (u_int u = 0; u < m_axPending.GetSize(); ++u)
	{
		delete m_axPending.Get(u).m_pxInstance;
	}
	m_axPending.Clear();
}

Zenith_GraphBuilder& Zenith_GraphBuilder::Variable(const char* szName, const Zenith_PropertyValue& xDefault)
{
	m_xDefinition.DeclareVariable(szName, xDefault);
	return *this;
}

u_int Zenith_GraphBuilder::Node(const char* szTypeName)
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(szTypeName);
	if (pxInfo == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Zenith_GraphBuilder: unknown node type '%s'", szTypeName ? szTypeName : "<null>");
		m_bErrors = true;
		return 0;
	}
	const u_int uNodeID = m_xDefinition.AddNode(szTypeName);
	if (uNodeID == 0)
	{
		m_bErrors = true;
		return 0;
	}

	PendingNode xPending;
	xPending.m_uNodeID = uNodeID;
	xPending.m_pxInfo = pxInfo;
	xPending.m_pxInstance = pxInfo->m_pfnCreate();	// defaults; Param* configures
	m_axPending.PushBack(xPending);
	return uNodeID;
}

Zenith_GraphBuilder::PendingNode* Zenith_GraphBuilder::FindPending(u_int uNodeID)
{
	for (u_int u = 0; u < m_axPending.GetSize(); ++u)
	{
		if (m_axPending.Get(u).m_uNodeID == uNodeID)
		{
			return &m_axPending.Get(u);
		}
	}
	return nullptr;
}

Zenith_GraphBuilder& Zenith_GraphBuilder::Param(u_int uNodeID, const char* szProperty, const Zenith_PropertyValue& xValue)
{
	PendingNode* pxPending = uNodeID != 0 ? FindPending(uNodeID) : nullptr;
	if (pxPending == nullptr)
	{
		// A 0 ID from a failed Node() flows through silently error-latched -
		// only report NEW information (a live ID we never made).
		if (uNodeID != 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "Zenith_GraphBuilder: Param on unknown node ID %u", uNodeID);
		}
		m_bErrors = true;
		return *this;
	}
	const Zenith_PropertyTable* pxTable = pxPending->m_pxInfo->m_pfnGetPropertyTable
		? pxPending->m_pxInfo->m_pfnGetPropertyTable() : nullptr;
	const Zenith_ReflectedProperty* pxProperty = pxTable ? pxTable->FindProperty(szProperty) : nullptr;
	if (pxProperty == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Zenith_GraphBuilder: node '%s' has no property '%s'",
			pxPending->m_pxInfo->m_strTypeName.c_str(), szProperty ? szProperty : "<null>");
		m_bErrors = true;
		return *this;
	}
	if (xValue.GetType() != pxProperty->m_eType)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Zenith_GraphBuilder: type mismatch setting '%s' on node '%s'",
			szProperty, pxPending->m_pxInfo->m_strTypeName.c_str());
		m_bErrors = true;
		return *this;
	}
	// SetPropertyValue also returns false for a NO-CHANGE set (value equals
	// the declared default) - a legal authoring no-op, not an error.
	Zenith_PropertySystem::SetPropertyValue(pxPending->m_pxInstance, *pxProperty, xValue);
	return *this;
}

Zenith_GraphBuilder& Zenith_GraphBuilder::ParamFloat(u_int uNodeID, const char* szProperty, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return Param(uNodeID, szProperty, xValue);
}

Zenith_GraphBuilder& Zenith_GraphBuilder::ParamInt(u_int uNodeID, const char* szProperty, int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	return Param(uNodeID, szProperty, xValue);
}

Zenith_GraphBuilder& Zenith_GraphBuilder::ParamBool(u_int uNodeID, const char* szProperty, bool bValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetBool(bValue);
	return Param(uNodeID, szProperty, xValue);
}

Zenith_GraphBuilder& Zenith_GraphBuilder::ParamString(u_int uNodeID, const char* szProperty, const char* szValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetString(szValue ? szValue : "");
	return Param(uNodeID, szProperty, xValue);
}

Zenith_GraphBuilder& Zenith_GraphBuilder::ParamVec3(u_int uNodeID, const char* szProperty, const Zenith_Maths::Vector3& xValue)
{
	Zenith_PropertyValue xWrapped;
	xWrapped.SetVector3(xValue);
	return Param(uNodeID, szProperty, xWrapped);
}

Zenith_GraphBuilder& Zenith_GraphBuilder::Edge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID)
{
	if (uSrcNodeID == 0 || uDstNodeID == 0)
	{
		m_bErrors = true;	// upstream Node() already reported
		return *this;
	}
	if (!m_xDefinition.AddEdge(uSrcNodeID, uSrcPin, uDstNodeID, 0))
	{
		m_bErrors = true;	// AddEdge logs the rejection reason
	}
	return *this;
}

Zenith_GraphBuilder& Zenith_GraphBuilder::Chain(u_int uFrom, u_int uTo)
{
	return Edge(uFrom, 0, uTo);
}

void Zenith_GraphBuilder::AssignEditorPositions()
{
	// Column = longest edge-path depth from any in-degree-0 node; row = order
	// within the column. Enough for a legible left-to-right editor layout.
	const u_int uNodeCount = m_xDefinition.GetNodeCount();
	Zenith_HashMap<u_int, u_int> xDepths;	// nodeID -> depth
	for (u_int u = 0; u < uNodeCount; ++u)
	{
		xDepths[m_xDefinition.GetNodeAt(u).m_uNodeID] = 0;
	}

	// Relax edges nodeCount times (graphs are small; cycles are impossible -
	// AddEdge rejects self-loops and chains are forward-built).
	for (u_int uPass = 0; uPass < uNodeCount; ++uPass)
	{
		bool bChanged = false;
		for (u_int u = 0; u < m_xDefinition.GetEdgeCount(); ++u)
		{
			const Zenith_GraphEdge& xEdge = m_xDefinition.GetEdgeAt(u);
			const u_int* puSrcDepth = xDepths.TryGet(xEdge.m_uSrcNodeID);
			u_int* puDstDepth = xDepths.TryGet(xEdge.m_uDstNodeID);
			if (puSrcDepth && puDstDepth && *puDstDepth < *puSrcDepth + 1)
			{
				*puDstDepth = *puSrcDepth + 1;
				bChanged = true;
			}
		}
		if (!bChanged)
		{
			break;
		}
	}

	Zenith_HashMap<u_int, u_int> xRowsUsed;	// depth -> rows used
	for (u_int u = 0; u < uNodeCount; ++u)
	{
		const u_int uNodeID = m_xDefinition.GetNodeAt(u).m_uNodeID;
		const u_int uDepth = *xDepths.TryGet(uNodeID);
		u_int* puRow = xRowsUsed.TryGet(uDepth);
		const u_int uRow = puRow ? *puRow : 0;
		xRowsUsed[uDepth] = uRow + 1;
		m_xDefinition.SetNodeEditorPos(uNodeID,
			Zenith_Maths::Vector2(60.0f + 240.0f * static_cast<float>(uDepth), 60.0f + 130.0f * static_cast<float>(uRow)));
	}
}

bool Zenith_GraphBuilder::Build()
{
	if (m_bBuilt)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Zenith_GraphBuilder: Build() called twice");
		m_bErrors = true;
		return false;
	}
	m_bBuilt = true;

	for (u_int u = 0; u < m_axPending.GetSize(); ++u)
	{
		PendingNode& xPending = m_axPending.Get(u);
		// Parameterless nodes have no property table - nothing to commit
		// (SetNodeParamsFromInstance rejects them by design).
		if (xPending.m_pxInfo->m_pfnGetPropertyTable != nullptr
			&& !m_xDefinition.SetNodeParamsFromInstance(xPending.m_uNodeID, xPending.m_pxInstance))
		{
			m_bErrors = true;
		}
		delete xPending.m_pxInstance;
		xPending.m_pxInstance = nullptr;
	}
	m_axPending.Clear();

	AssignEditorPositions();
	return !m_bErrors;
}
