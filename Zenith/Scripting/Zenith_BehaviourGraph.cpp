#include "Zenith.h"
#include "Scripting/Zenith_BehaviourGraph.h"

//==============================================================================
// Zenith_GraphDefinition
//==============================================================================

void Zenith_GraphDefinition::DeclareVariable(const char* szName, const Zenith_PropertyValue& xDefault)
{
	if (!szName || szName[0] == '\0')
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: empty variable name");
		return;
	}
	for (u_int u = 0; u < m_axVariables.GetSize(); ++u)
	{
		if (m_axVariables.Get(u).m_strName == szName)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: duplicate variable '%s'", szName);
			return;
		}
	}
	Zenith_GraphVariableDecl xDecl;
	xDecl.m_strName = szName;
	xDecl.m_xDefault = xDefault;
	m_axVariables.PushBack(xDecl);
}

u_int Zenith_GraphDefinition::AddNode(const char* szTypeName)
{
	if (!szTypeName || szTypeName[0] == '\0')
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: empty node type name");
		return 0;
	}

	Zenith_GraphNodeDef xDef;
	xDef.m_uNodeID = m_uNextNodeID++;
	xDef.m_strTypeName = szTypeName;

	// Capture default params (and the current type version) so a fresh node
	// round-trips stably even if its defaults later change in code.
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(szTypeName);
	if (pxInfo)
	{
		xDef.m_uTypeVersion = pxInfo->m_uTypeVersion;
		if (pxInfo->m_pfnGetPropertyTable)
		{
			Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
			Zenith_PropertySystem::WriteProperties(pxTemp, *pxInfo->m_pfnGetPropertyTable(), xDef.m_xParamBlob);
			delete pxTemp;
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"GraphDefinition: node type '%s' not registered in this build; added as unresolved", szTypeName);
	}

	const u_int uNodeID = xDef.m_uNodeID;
	m_axNodes.PushBack(std::move(xDef));
	return uNodeID;
}

bool Zenith_GraphDefinition::SetNodeParamsFromInstance(u_int uNodeID, const Zenith_GraphNode* pxConfigured)
{
	if (!pxConfigured)
	{
		return false;
	}
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		Zenith_GraphNodeDef& xDef = m_axNodes.Get(u);
		if (xDef.m_uNodeID != uNodeID)
		{
			continue;
		}
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(xDef.m_strTypeName.c_str());
		if (!pxInfo || !pxInfo->m_pfnGetPropertyTable)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: cannot set params on '%s' (unregistered or parameterless)",
				xDef.m_strTypeName.c_str());
			return false;
		}
		xDef.m_xParamBlob = Zenith_DataStream();
		Zenith_PropertySystem::WriteProperties(pxConfigured, *pxInfo->m_pfnGetPropertyTable(), xDef.m_xParamBlob);
		return true;
	}
	return false;
}

bool Zenith_GraphDefinition::RemoveNode(u_int uNodeID)
{
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		if (m_axNodes.Get(u).m_uNodeID == uNodeID)
		{
			m_axNodes.Remove(u);
			for (u_int uEdge = m_axEdges.GetSize(); uEdge > 0; --uEdge)
			{
				const Zenith_GraphEdge& xEdge = m_axEdges.Get(uEdge - 1);
				if (xEdge.m_uSrcNodeID == uNodeID || xEdge.m_uDstNodeID == uNodeID)
				{
					m_axEdges.Remove(uEdge - 1);
				}
			}
			m_xEditorPositions.Remove(uNodeID);
			return true;
		}
	}
	return false;
}

bool Zenith_GraphDefinition::AddEdge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID, u_int uDstPin)
{
	if (uSrcNodeID == uDstNodeID)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: self-loop edge rejected (node %u)", uSrcNodeID);
		return false;
	}
	if (!FindNodeDef(uSrcNodeID) || !FindNodeDef(uDstNodeID))
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: edge endpoints unknown (%u -> %u)", uSrcNodeID, uDstNodeID);
		return false;
	}
	for (u_int u = 0; u < m_axEdges.GetSize(); ++u)
	{
		const Zenith_GraphEdge& xEdge = m_axEdges.Get(u);
		if (xEdge.m_uSrcNodeID == uSrcNodeID && xEdge.m_uSrcPin == uSrcPin)
		{
			Zenith_Error(LOG_CATEGORY_CORE,
				"GraphDefinition: (node %u, pin %u) already has an outgoing edge - exec chains are linear", uSrcNodeID, uSrcPin);
			return false;
		}
	}
	Zenith_GraphEdge xEdge;
	xEdge.m_uSrcNodeID = uSrcNodeID;
	xEdge.m_uSrcPin = uSrcPin;
	xEdge.m_uDstNodeID = uDstNodeID;
	xEdge.m_uDstPin = uDstPin;
	m_axEdges.PushBack(xEdge);
	return true;
}

bool Zenith_GraphDefinition::RemoveEdge(u_int uSrcNodeID, u_int uSrcPin)
{
	for (u_int u = 0; u < m_axEdges.GetSize(); ++u)
	{
		const Zenith_GraphEdge& xEdge = m_axEdges.Get(u);
		if (xEdge.m_uSrcNodeID == uSrcNodeID && xEdge.m_uSrcPin == uSrcPin)
		{
			m_axEdges.Remove(u);
			return true;
		}
	}
	return false;
}

void Zenith_GraphDefinition::Clear()
{
	m_axVariables.Clear();
	m_axNodes.Clear();
	m_axEdges.Clear();
	m_xEditorPositions.Clear();
	m_uNextNodeID = 1;
}

Zenith_GraphVariableDecl* Zenith_GraphDefinition::FindVariableMutable(const char* szName)
{
	if (!szName)
	{
		return nullptr;
	}
	for (u_int u = 0; u < m_axVariables.GetSize(); ++u)
	{
		if (m_axVariables.Get(u).m_strName == szName)
		{
			return &m_axVariables.Get(u);
		}
	}
	return nullptr;
}

bool Zenith_GraphDefinition::RemoveVariable(const char* szName)
{
	if (!szName)
	{
		return false;
	}
	for (u_int u = 0; u < m_axVariables.GetSize(); ++u)
	{
		if (m_axVariables.Get(u).m_strName == szName)
		{
			m_axVariables.Remove(u);
			return true;
		}
	}
	return false;
}

const Zenith_GraphNodeDef* Zenith_GraphDefinition::FindNodeDef(u_int uNodeID) const
{
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		if (m_axNodes.Get(u).m_uNodeID == uNodeID)
		{
			return &m_axNodes.Get(u);
		}
	}
	return nullptr;
}

bool Zenith_GraphDefinition::GetNodeEditorPos(u_int uNodeID, Zenith_Maths::Vector2& xOut) const
{
	const Zenith_Maths::Vector2* pxPos = m_xEditorPositions.TryGet(uNodeID);
	if (!pxPos)
	{
		return false;
	}
	xOut = *pxPos;
	return true;
}

void Zenith_GraphDefinition::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uGRAPH_MAGIC;
	xStream << uGRAPH_VERSION;

	// Variables
	const u_int uVariableCount = m_axVariables.GetSize();
	xStream << uVariableCount;
	for (u_int u = 0; u < uVariableCount; ++u)
	{
		const Zenith_GraphVariableDecl& xDecl = m_axVariables.Get(u);
		xStream << xDecl.m_strName;
		xStream << xDecl.m_xDefault;	// tagged (type + payload)
	}

	// Nodes - param blobs length-framed so any reader can skip any node.
	const u_int uNodeCount = m_axNodes.GetSize();
	xStream << uNodeCount;
	for (u_int u = 0; u < uNodeCount; ++u)
	{
		const Zenith_GraphNodeDef& xDef = m_axNodes.Get(u);
		xStream << xDef.m_uNodeID;
		xStream << xDef.m_strTypeName;
		xStream << xDef.m_uTypeVersion;
		const u_int uBlobBytes = static_cast<u_int>(xDef.m_xParamBlob.GetCursor());
		xStream << uBlobBytes;
		if (uBlobBytes > 0)
		{
			xStream.WriteData(xDef.m_xParamBlob.GetData(), uBlobBytes);
		}
	}

	// Edges
	const u_int uEdgeCount = m_axEdges.GetSize();
	xStream << uEdgeCount;
	for (u_int u = 0; u < uEdgeCount; ++u)
	{
		const Zenith_GraphEdge& xEdge = m_axEdges.Get(u);
		xStream << xEdge.m_uSrcNodeID;
		xStream << xEdge.m_uSrcPin;
		xStream << xEdge.m_uDstNodeID;
		xStream << xEdge.m_uDstPin;
	}

	// Editor layout - length-framed (runtime loaders may skip; we always read
	// it to preserve layout across non-tools round-trips).
	const uint64_t ulLayoutSizeFieldCursor = xStream.GetCursor();
	u_int uLayoutPlaceholder = 0;
	xStream << uLayoutPlaceholder;
	const uint64_t ulLayoutStartCursor = xStream.GetCursor();

	const u_int uLayoutCount = m_xEditorPositions.GetSize();
	xStream << uLayoutCount;
	for (Zenith_HashMap<u_int, Zenith_Maths::Vector2>::Iterator xIt(m_xEditorPositions); !xIt.Done(); xIt.Next())
	{
		xStream << xIt.GetKey();
		xStream << xIt.GetValue().x;
		xStream << xIt.GetValue().y;
	}

	const uint64_t ulLayoutEndCursor = xStream.GetCursor();
	const u_int uLayoutBytes = static_cast<u_int>(ulLayoutEndCursor - ulLayoutStartCursor);
	xStream.SetCursor(ulLayoutSizeFieldCursor);
	xStream << uLayoutBytes;
	xStream.SetCursor(ulLayoutEndCursor);
}

bool Zenith_GraphDefinition::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	u_int uMagic = 0;
	xStream >> uMagic;
	if (uMagic != uGRAPH_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: bad magic 0x%08X (not a behaviour graph)", uMagic);
		return false;
	}
	u_int uVersion = 0;
	xStream >> uVersion;
	if (uVersion != uGRAPH_VERSION)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: unsupported version %u (expected %u)", uVersion, uGRAPH_VERSION);
		return false;
	}

	u_int uVariableCount = 0;
	xStream >> uVariableCount;
	for (u_int u = 0; u < uVariableCount; ++u)
	{
		Zenith_GraphVariableDecl xDecl;
		xStream >> xDecl.m_strName;
		xStream >> xDecl.m_xDefault;
		m_axVariables.PushBack(xDecl);
	}

	u_int uNodeCount = 0;
	xStream >> uNodeCount;
	u_int uMaxNodeID = 0;
	for (u_int u = 0; u < uNodeCount; ++u)
	{
		Zenith_GraphNodeDef xDef;
		xStream >> xDef.m_uNodeID;
		xStream >> xDef.m_strTypeName;
		xStream >> xDef.m_uTypeVersion;
		u_int uBlobBytes = 0;
		xStream >> uBlobBytes;
		if (uBlobBytes > 0)
		{
			// Copy the param bytes verbatim into the def's own stream (cursor
			// marks the populated extent - the unresolved-preservation idiom).
			xDef.m_xParamBlob.WriteData(static_cast<const uint8_t*>(xStream.GetData()) + xStream.GetCursor(), uBlobBytes);
			xStream.SkipBytes(uBlobBytes);
		}
		uMaxNodeID = (xDef.m_uNodeID > uMaxNodeID) ? xDef.m_uNodeID : uMaxNodeID;
		m_axNodes.PushBack(std::move(xDef));
	}
	m_uNextNodeID = uMaxNodeID + 1;

	u_int uEdgeCount = 0;
	xStream >> uEdgeCount;
	for (u_int u = 0; u < uEdgeCount; ++u)
	{
		Zenith_GraphEdge xEdge;
		xStream >> xEdge.m_uSrcNodeID;
		xStream >> xEdge.m_uSrcPin;
		xStream >> xEdge.m_uDstNodeID;
		xStream >> xEdge.m_uDstPin;
		m_axEdges.PushBack(xEdge);
	}

	u_int uLayoutBytes = 0;
	xStream >> uLayoutBytes;
	const uint64_t ulLayoutEnd = xStream.GetCursor() + uLayoutBytes;
	u_int uLayoutCount = 0;
	xStream >> uLayoutCount;
	for (u_int u = 0; u < uLayoutCount; ++u)
	{
		u_int uNodeID = 0;
		float fX = 0.0f;
		float fY = 0.0f;
		xStream >> uNodeID;
		xStream >> fX;
		xStream >> fY;
		m_xEditorPositions[uNodeID] = Zenith_Maths::Vector2(fX, fY);
	}
	if (xStream.GetCursor() != ulLayoutEnd)
	{
		xStream.SetCursor(ulLayoutEnd);
	}

	return true;
}

//==============================================================================
// Zenith_BehaviourGraph
//==============================================================================

Zenith_BehaviourGraph::~Zenith_BehaviourGraph()
{
	Shutdown();
}

bool Zenith_BehaviourGraph::InitialiseFromDefinition(const Zenith_GraphDefinition& xDefinition)
{
	Shutdown();

	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();

	for (u_int u = 0; u < xDefinition.GetNodeCount(); ++u)
	{
		const Zenith_GraphNodeDef& xDef = xDefinition.GetNodeAt(u);

		NodeInstance xInstance;
		xInstance.m_uNodeID = xDef.m_uNodeID;
		xInstance.m_pxTypeInfo = xRegistry.Find(xDef.m_strTypeName.c_str());

		if (xInstance.m_pxTypeInfo)
		{
			xInstance.m_pxNode = xInstance.m_pxTypeInfo->m_pfnCreate();
			xInstance.m_pxNode->m_uNodeID = xDef.m_uNodeID;
			if (xInstance.m_pxTypeInfo->m_pfnGetPropertyTable && xDef.m_xParamBlob.GetCursor() > 0)
			{
				// Wrap the blob (no copy, no ownership) and apply the params.
				// Name+type-matched reading tolerates schema drift; unknown
				// params are skipped, never corrupted.
				Zenith_DataStream xParamRead(const_cast<void*>(xDef.m_xParamBlob.GetData()), xDef.m_xParamBlob.GetCursor());
				Zenith_PropertySystem::ReadProperties(xInstance.m_pxNode, *xInstance.m_pxTypeInfo->m_pfnGetPropertyTable(), xParamRead);
			}
		}
		else
		{
			++m_uUnresolvedCount;
			Zenith_Log(LOG_CATEGORY_CORE,
				"BehaviourGraph: node %u type '%s' not registered in this build; chains through it will fail",
				xDef.m_uNodeID, xDef.m_strTypeName.c_str());
		}

		m_axNodes.PushBack(xInstance);

		if (xInstance.m_pxTypeInfo && xInstance.m_pxTypeInfo->m_eEventType != GRAPH_EVENT_NONE)
		{
			m_auEventSources[xInstance.m_pxTypeInfo->m_eEventType].PushBack(xInstance.m_uNodeID);
		}
	}

	for (u_int u = 0; u < xDefinition.GetEdgeCount(); ++u)
	{
		m_axEdges.PushBack(xDefinition.GetEdgeAt(u));
	}

	for (u_int u = 0; u < xDefinition.GetVariableCount(); ++u)
	{
		const Zenith_GraphVariableDecl& xDecl = xDefinition.GetVariableAt(u);
		m_xBlackboard.SetValue(xDecl.m_strName, xDecl.m_xDefault);
	}

	return true;
}

void Zenith_BehaviourGraph::Shutdown()
{
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		delete m_axNodes.Get(u).m_pxNode;
	}
	m_axNodes.Clear();
	m_axEdges.Clear();
	m_xChainCursors.Clear();
	for (u_int u = 0; u < GRAPH_EVENT_COUNT; ++u)
	{
		m_auEventSources[u].Clear();
	}
	m_auSuspendedOneShotAnchors.Clear();
	m_xBlackboard.Clear();
	m_uUnresolvedCount = 0;
	m_uExecutingNodeID = 0;
}

bool Zenith_BehaviourGraph::HasEventSource(GraphEventType eEvent) const
{
	Zenith_Assert(eEvent < GRAPH_EVENT_COUNT, "BehaviourGraph: bad event type %u", eEvent);
	return m_auEventSources[eEvent].GetSize() > 0;
}

void Zenith_BehaviourGraph::RunSourceNode(NodeInstance& xSource, Zenith_GraphContext& xContext)
{
	const u_int64 ulKey = MakeChainKey(xSource.m_uNodeID, 0);

	// A suspended chain resumes in place of re-firing its source.
	if (m_xChainCursors.Contains(ulKey))
	{
		RunChainFromPin(xSource.m_uNodeID, 0, xContext);
		return;
	}

	// Sources gate themselves: default sources return SUCCESS every fire,
	// Timer accumulates dt and succeeds on interval, etc.
	m_uExecutingNodeID = xSource.m_uNodeID;
	const GraphNodeStatus eGate = xSource.m_pxNode->Execute(xContext);
	m_uExecutingNodeID = 0;
	if (eGate != GRAPH_NODE_STATUS_SUCCESS)
	{
		return;
	}

	const GraphNodeStatus eStatus = RunChainFromPin(xSource.m_uNodeID, 0, xContext);

	// One-shot anchors (collisions, custom events, OnStart...) that suspend are
	// re-driven by the ON_UPDATE dispatch until they finish. Periodic anchors
	// (Update/FixedUpdate/Timer) resume through their own next fire.
	if (eStatus == GRAPH_NODE_STATUS_RUNNING && xSource.m_pxTypeInfo)
	{
		const GraphEventType eType = xSource.m_pxTypeInfo->m_eEventType;
		if (eType != GRAPH_EVENT_ON_UPDATE && eType != GRAPH_EVENT_ON_FIXED_UPDATE && eType != GRAPH_EVENT_TIMER)
		{
			for (u_int u = 0; u < m_auSuspendedOneShotAnchors.GetSize(); ++u)
			{
				if (m_auSuspendedOneShotAnchors.Get(u) == xSource.m_uNodeID)
				{
					return;
				}
			}
			m_auSuspendedOneShotAnchors.PushBack(xSource.m_uNodeID);
		}
	}
}

void Zenith_BehaviourGraph::FireEvent(GraphEventType eEvent, Zenith_GraphContext& xContext)
{
	Zenith_Assert(eEvent < GRAPH_EVENT_COUNT, "BehaviourGraph: bad event type %u", eEvent);

	// The ON_UPDATE dispatch starts a fresh "recently executed" window for the
	// editor's live execution highlighting.
	if (eEvent == GRAPH_EVENT_ON_UPDATE)
	{
		m_auRecentlyExecuted.Clear();
	}

	// Snapshot the source list - node execution must not mutate graph
	// structure, but stay robust anyway (the hardened-dispatch lesson).
	Zenith_Vector<u_int> auSources;
	for (u_int u = 0; u < m_auEventSources[eEvent].GetSize(); ++u)
	{
		auSources.PushBack(m_auEventSources[eEvent].Get(u));
	}
	// Timer sources tick during the ON_UPDATE dispatch.
	if (eEvent == GRAPH_EVENT_ON_UPDATE)
	{
		for (u_int u = 0; u < m_auEventSources[GRAPH_EVENT_TIMER].GetSize(); ++u)
		{
			auSources.PushBack(m_auEventSources[GRAPH_EVENT_TIMER].Get(u));
		}
	}

	for (u_int u = 0; u < auSources.GetSize(); ++u)
	{
		NodeInstance* pxSource = FindInstance(auSources.Get(u));
		if (pxSource && pxSource->m_pxNode)
		{
			RunSourceNode(*pxSource, xContext);
		}
	}

	// Resume suspended one-shot chains.
	if (eEvent == GRAPH_EVENT_ON_UPDATE && m_auSuspendedOneShotAnchors.GetSize() > 0)
	{
		Zenith_Vector<u_int> auStillSuspended;
		for (u_int u = 0; u < m_auSuspendedOneShotAnchors.GetSize(); ++u)
		{
			const u_int uAnchor = m_auSuspendedOneShotAnchors.Get(u);
			const GraphNodeStatus eStatus = RunChainFromPin(uAnchor, 0, xContext);
			if (eStatus == GRAPH_NODE_STATUS_RUNNING)
			{
				auStillSuspended.PushBack(uAnchor);
			}
		}
		m_auSuspendedOneShotAnchors = std::move(auStillSuspended);
	}
}

void Zenith_BehaviourGraph::FireCustomEvent(const char* szName, Zenith_GraphContext& xContext)
{
	if (!szName)
	{
		return;
	}
	Zenith_Vector<u_int> auSources;
	for (u_int u = 0; u < m_auEventSources[GRAPH_EVENT_CUSTOM].GetSize(); ++u)
	{
		auSources.PushBack(m_auEventSources[GRAPH_EVENT_CUSTOM].Get(u));
	}
	for (u_int u = 0; u < auSources.GetSize(); ++u)
	{
		NodeInstance* pxSource = FindInstance(auSources.Get(u));
		if (pxSource && pxSource->m_pxNode && pxSource->m_pxNode->MatchesCustomEvent(szName))
		{
			RunSourceNode(*pxSource, xContext);
		}
	}
}

GraphNodeStatus Zenith_BehaviourGraph::RunChainFromPin(u_int uNodeID, u_int uPin, Zenith_GraphContext& xContext)
{
	const u_int64 ulKey = MakeChainKey(uNodeID, uPin);

	u_int uCurrent = 0;
	const u_int* puResume = m_xChainCursors.TryGet(ulKey);
	if (puResume)
	{
		uCurrent = *puResume;
	}
	else
	{
		uCurrent = FindSuccessor(uNodeID, uPin);
	}

	if (uCurrent == 0)
	{
		return GRAPH_NODE_STATUS_SUCCESS;	// empty chain
	}

	while (uCurrent != 0)
	{
		NodeInstance* pxInstance = FindInstance(uCurrent);
		if (!pxInstance || !pxInstance->m_pxNode)
		{
			if (pxInstance && !pxInstance->m_bWarnedUnresolved)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "BehaviourGraph: chain hit unresolved node %u; aborting chain", uCurrent);
				pxInstance->m_bWarnedUnresolved = true;
			}
			m_xChainCursors.Remove(ulKey);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		const u_int uPreviousExecuting = m_uExecutingNodeID;
		m_uExecutingNodeID = uCurrent;
		if (m_auRecentlyExecuted.GetSize() < 64)
		{
			m_auRecentlyExecuted.PushBack(uCurrent);
		}
		const GraphNodeStatus eStatus = pxInstance->m_pxNode->Execute(xContext);
		m_uExecutingNodeID = uPreviousExecuting;

		if (eStatus == GRAPH_NODE_STATUS_RUNNING)
		{
			m_xChainCursors[ulKey] = uCurrent;
			return GRAPH_NODE_STATUS_RUNNING;
		}
		if (eStatus == GRAPH_NODE_STATUS_FAILURE)
		{
			m_xChainCursors.Remove(ulKey);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		// SUCCESS. Flow nodes (Branch/Loop) drive their outputs from inside
		// Execute - their chain ends here. Plain nodes auto-continue via pin 0.
		if (pxInstance->m_pxTypeInfo->m_bFlowNode)
		{
			m_xChainCursors.Remove(ulKey);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		uCurrent = FindSuccessor(uCurrent, 0);
	}

	m_xChainCursors.Remove(ulKey);
	return GRAPH_NODE_STATUS_SUCCESS;
}

Zenith_GraphNode* Zenith_BehaviourGraph::FindNode(u_int uNodeID)
{
	NodeInstance* pxInstance = FindInstance(uNodeID);
	return pxInstance ? pxInstance->m_pxNode : nullptr;
}

void Zenith_BehaviourGraph::ResetChainState()
{
	m_xChainCursors.Clear();
	m_auSuspendedOneShotAnchors.Clear();
}

Zenith_BehaviourGraph::NodeInstance* Zenith_BehaviourGraph::FindInstance(u_int uNodeID)
{
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		if (m_axNodes.Get(u).m_uNodeID == uNodeID)
		{
			return &m_axNodes.Get(u);
		}
	}
	return nullptr;
}

u_int Zenith_BehaviourGraph::FindSuccessor(u_int uNodeID, u_int uPin) const
{
	for (u_int u = 0; u < m_axEdges.GetSize(); ++u)
	{
		const Zenith_GraphEdge& xEdge = m_axEdges.Get(u);
		if (xEdge.m_uSrcNodeID == uNodeID && xEdge.m_uSrcPin == uPin)
		{
			return xEdge.m_uDstNodeID;
		}
	}
	return 0;
}

#include "Scripting/Zenith_Scripting.Tests.inl"
