#include "Zenith.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_SceneSystem.h"	// ResolveTargetEntity (leaf-legal: ZenithECS is a Scripting dependency)

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

bool Zenith_GraphDefinition::ApplyNodeParams(u_int uNodeID, Zenith_GraphNode* pxNode, const Zenith_GraphNodeTypeInfo& xInfo) const
{
	if (pxNode == nullptr || xInfo.m_pfnGetPropertyTable == nullptr)
	{
		return false;
	}
	const Zenith_GraphNodeDef* pxDef = FindNodeDef(uNodeID);
	if (pxDef == nullptr || pxDef->m_xParamBlob.GetCursor() == 0)
	{
		return false;
	}
	// Wrap the blob (no copy, no ownership) and apply the params.
	Zenith_DataStream xParamRead(const_cast<void*>(pxDef->m_xParamBlob.GetData()), pxDef->m_xParamBlob.GetCursor());
	Zenith_PropertySystem::ReadProperties(pxNode, *xInfo.m_pfnGetPropertyTable(), xParamRead);
	return true;
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
			for (u_int uEdge = m_axDataEdges.GetSize(); uEdge > 0; --uEdge)
			{
				const Zenith_GraphDataEdge& xEdge = m_axDataEdges.Get(uEdge - 1);
				if (xEdge.m_uSrcNodeID == uNodeID || xEdge.m_uDstNodeID == uNodeID)
				{
					m_axDataEdges.Remove(uEdge - 1);
				}
			}
			m_xEditorPositions.Remove(uNodeID);
			return true;
		}
	}
	return false;
}

bool Zenith_GraphDefinition::AddEdge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID)
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
	m_axEdges.PushBack(xEdge);
	return true;
}

bool Zenith_GraphDefinition::AddDataEdge(u_int uSrcNodeID, const char* szSrcPin, u_int uDstNodeID, const char* szDstPin)
{
	if (szSrcPin == nullptr || szSrcPin[0] == '\0' || szDstPin == nullptr || szDstPin[0] == '\0')
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: data edge with an empty pin name rejected (%u -> %u)",
			uSrcNodeID, uDstNodeID);
		return false;
	}
	// A data self-loop is the NODE pair, whatever the pin names say: a node
	// feeding its own input is a one-node cycle either way.
	if (uSrcNodeID == uDstNodeID)
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: self-loop data edge rejected (node %u)", uSrcNodeID);
		return false;
	}
	if (!FindNodeDef(uSrcNodeID) || !FindNodeDef(uDstNodeID))
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: data edge endpoints unknown (%u -> %u)", uSrcNodeID, uDstNodeID);
		return false;
	}
	if (FindDataEdgeInto(uDstNodeID, szDstPin) != nullptr)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"GraphDefinition: (node %u, pin '%s') already has an incoming data edge - one wire per input",
			uDstNodeID, szDstPin);
		return false;
	}

	Zenith_GraphDataEdge xEdge;
	xEdge.m_uSrcNodeID = uSrcNodeID;
	xEdge.m_strSrcPin = szSrcPin;
	xEdge.m_uDstNodeID = uDstNodeID;
	xEdge.m_strDstPin = szDstPin;
	m_axDataEdges.PushBack(xEdge);
	return true;
}

bool Zenith_GraphDefinition::RemoveDataEdge(u_int uDstNodeID, const char* szDstPin)
{
	if (szDstPin == nullptr || szDstPin[0] == '\0')
	{
		return false;
	}
	for (u_int u = 0; u < m_axDataEdges.GetSize(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = m_axDataEdges.Get(u);
		if (xEdge.m_uDstNodeID == uDstNodeID && xEdge.m_strDstPin == szDstPin)
		{
			m_axDataEdges.Remove(u);
			return true;
		}
	}
	return false;
}

const Zenith_GraphDataEdge* Zenith_GraphDefinition::FindDataEdgeInto(u_int uDstNodeID, const char* szDstPin) const
{
	if (szDstPin == nullptr || szDstPin[0] == '\0')
	{
		return nullptr;
	}
	for (u_int u = 0; u < m_axDataEdges.GetSize(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = m_axDataEdges.Get(u);
		if (xEdge.m_uDstNodeID == uDstNodeID && xEdge.m_strDstPin == szDstPin)
		{
			return &m_axDataEdges.Get(u);
		}
	}
	return nullptr;
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
	m_axDataEdges.Clear();
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

	// Exec edges - three u_ints each (a node has one exec input, so there is no
	// destination pin to store).
	const u_int uEdgeCount = m_axEdges.GetSize();
	xStream << uEdgeCount;
	for (u_int u = 0; u < uEdgeCount; ++u)
	{
		const Zenith_GraphEdge& xEdge = m_axEdges.Get(u);
		xStream << xEdge.m_uSrcNodeID;
		xStream << xEdge.m_uSrcPin;
		xStream << xEdge.m_uDstNodeID;
	}

	// Data edges - a plain count, exactly like the exec block. Deliberately NOT
	// length-framed: a frame exists to let a reader SKIP a block it cannot parse,
	// and there is no such reader (the version check is strict equality and no
	// v1 reader exists).
	const u_int uDataEdgeCount = m_axDataEdges.GetSize();
	xStream << uDataEdgeCount;
	for (u_int u = 0; u < uDataEdgeCount; ++u)
	{
		const Zenith_GraphDataEdge& xEdge = m_axDataEdges.Get(u);
		xStream << xEdge.m_uSrcNodeID;
		xStream << xEdge.m_strSrcPin;
		xStream << xEdge.m_uDstNodeID;
		xStream << xEdge.m_strDstPin;
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

bool Zenith_GraphDefinition::ReadFromDataStream(Zenith_DataStream& xStream,
	Zenith_Vector<Zenith_GraphValidationFinding>* pxOutLoadSafetyFindings)
{
	Clear();
	if (pxOutLoadSafetyFindings != nullptr)
	{
		pxOutLoadSafetyFindings->Clear();
	}

	// ★ THE CALLER'S STREAM MUST BE POSITIONED AT THE DEFINITION WITH A CLEAN
	// READ-FAILURE FLAG. Every caller reaches here through SetCursor(0) or
	// ReadFromFile, both of which clear it (Zenith_DataStream.h) - verified for
	// Zenith_AssetRegistry.cpp, Zenith_GraphReload.cpp and the graph editor's
	// serialize-copy. A stream handed over with the flag already set would be
	// refused below as if THIS payload were corrupt.

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

	// Every count below is BUDGETED against the bytes that remain before it is
	// used as a loop bound: an overrun read asserts (which DebugBreaks a headless
	// batch) long before it reports, so a corrupt count must never reach the loop.
	// The minimums are the smallest legal encoding of one record.
	u_int uVariableCount = 0;
	xStream >> uVariableCount;
	if (uVariableCount > xStream.GetRemainingBytes() / sizeof(u_int))
	{
		xStream.MarkCorrupt("GraphDefinition: variable count exceeds the bytes that remain");
		Clear();
		return false;
	}
	for (u_int u = 0; u < uVariableCount; ++u)
	{
		Zenith_GraphVariableDecl xDecl;
		xStream >> xDecl.m_strName;
		xStream >> xDecl.m_xDefault;
		m_axVariables.PushBack(xDecl);
	}

	u_int uNodeCount = 0;
	xStream >> uNodeCount;
	if (uNodeCount > xStream.GetRemainingBytes() / (4u * sizeof(u_int)))	// id + name length + version + blob length
	{
		xStream.MarkCorrupt("GraphDefinition: node count exceeds the bytes that remain");
		Clear();
		return false;
	}
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
			// ★ BUDGET BEFORE THE COPY. This memcpy reads straight out of the
			// source buffer, and the SkipBytes below CLAMPS without reporting, so
			// an over-long blob length would be an unbounded overread that nothing
			// ever flagged.
			if (uBlobBytes > xStream.GetRemainingBytes())
			{
				xStream.MarkCorrupt("GraphDefinition: node param blob is longer than the bytes that remain");
				Clear();
				return false;
			}
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
	if (uEdgeCount > xStream.GetRemainingBytes() / (3u * sizeof(u_int)))
	{
		xStream.MarkCorrupt("GraphDefinition: exec edge count exceeds the bytes that remain");
		Clear();
		return false;
	}
	for (u_int u = 0; u < uEdgeCount; ++u)
	{
		Zenith_GraphEdge xEdge;
		xStream >> xEdge.m_uSrcNodeID;
		xStream >> xEdge.m_uSrcPin;
		xStream >> xEdge.m_uDstNodeID;
		m_axEdges.PushBack(xEdge);
	}

	u_int uDataEdgeCount = 0;
	xStream >> uDataEdgeCount;
	if (uDataEdgeCount > xStream.GetRemainingBytes() / (4u * sizeof(u_int)))	// src + name length + dst + name length
	{
		xStream.MarkCorrupt("GraphDefinition: data edge count exceeds the bytes that remain");
		Clear();
		return false;
	}
	for (u_int u = 0; u < uDataEdgeCount; ++u)
	{
		Zenith_GraphDataEdge xEdge;
		xStream >> xEdge.m_uSrcNodeID;
		xStream >> xEdge.m_strSrcPin;
		xStream >> xEdge.m_uDstNodeID;
		xStream >> xEdge.m_strDstPin;
		m_axDataEdges.PushBack(xEdge);
	}

	u_int uLayoutBytes = 0;
	xStream >> uLayoutBytes;
	if (uLayoutBytes > xStream.GetRemainingBytes())
	{
		xStream.MarkCorrupt("GraphDefinition: editor layout block is longer than the bytes that remain");
		Clear();
		return false;
	}
	const uint64_t ulLayoutEnd = xStream.GetCursor() + uLayoutBytes;
	u_int uLayoutCount = 0;
	xStream >> uLayoutCount;
	if (uLayoutCount > xStream.GetRemainingBytes() / (3u * sizeof(u_int)))
	{
		xStream.MarkCorrupt("GraphDefinition: editor layout entry count exceeds the bytes that remain");
		Clear();
		return false;
	}
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

	// ★ LATCH THE FLAG BEFORE THE SEEK. SetCursor is one of the stream's two
	// read-failure RESET points, so asking after the seek would always answer
	// "clean" however badly the payload above parsed.
	const bool bFailedBeforeSeek = xStream.HasReadFailure();
	if (xStream.GetCursor() != ulLayoutEnd)
	{
		xStream.SetCursor(ulLayoutEnd);
	}
	if (bFailedBeforeSeek || xStream.HasReadFailure())
	{
		Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: the stream reported a read failure - refusing a half-built definition");
		Clear();
		return false;
	}

	// LOAD_SAFETY tier: crash-class or silently-ambiguous structure only. An
	// ORPHAN edge is deliberately NOT here - it is inert at runtime
	// (FindSuccessor returns 0) and the FULL tier reports it.
	Zenith_Vector<Zenith_GraphValidationFinding> axLoadSafety;
	Zenith_GraphDefinitionValidator::ValidateLoadSafety(*this, axLoadSafety);
	if (axLoadSafety.GetSize() > 0)
	{
		for (u_int u = 0; u < axLoadSafety.GetSize(); ++u)
		{
			const Zenith_GraphValidationFinding& xFinding = axLoadSafety.Get(u);
			Zenith_Error(LOG_CATEGORY_CORE, "GraphDefinition: load-safety refusal node=%u rule=%s | %s",
				xFinding.m_uNodeID,
				Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
				xFinding.m_strWhat.c_str());
			if (pxOutLoadSafetyFindings != nullptr)
			{
				pxOutLoadSafetyFindings->PushBack(xFinding);
			}
		}
		Clear();
		return false;
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
			xDefinition.ApplyNodeParams(xDef.m_uNodeID, xInstance.m_pxNode, *xInstance.m_pxTypeInfo);
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
	m_bChainStepCapHit = false;
}

bool Zenith_BehaviourGraph::HasEventSource(GraphEventType eEvent) const
{
	Zenith_Assert(eEvent < GRAPH_EVENT_COUNT, "BehaviourGraph: bad event type %u", eEvent);
	return m_auEventSources[eEvent].GetSize() > 0;
}

bool Zenith_BehaviourGraph::NeedsUpdateDispatch() const
{
	return m_auEventSources[GRAPH_EVENT_ON_UPDATE].GetSize() > 0
		|| m_auEventSources[GRAPH_EVENT_TIMER].GetSize() > 0
		|| m_auSuspendedOneShotAnchors.GetSize() > 0
		|| m_xChainCursors.GetSize() > 0;
}

void Zenith_BehaviourGraph::RunSourceNode(NodeInstance& xSource, Zenith_GraphContext& xContext)
{
	const u_int64 ulKey = MakeChainKey(xSource.m_uNodeID, 0);

	// A suspended chain resumes in place of re-firing its source. That makes
	// this a RESUME drive: the cursor node is re-executed WITHOUT OnEnter, so a
	// fan-out flow node (Sequence) needs the flag to tell "resume the branch
	// still running" from "fire every branch again". The two periodic anchors
	// are excluded - an OnUpdate/OnFixedUpdate tick IS the fire, and Blueprint's
	// Event Tick -> Sequence fires every pin every tick. TIMER is deliberately
	// INCLUDED (a timer occurrence is one occurrence, resumed until it
	// finishes), so this predicate is NOT the three-way periodic test below.
	// The context object is caller-owned and reused across sources and frames,
	// hence the save/restore: a leak into the next source is the bug.
	if (m_xChainCursors.Contains(ulKey))
	{
		const bool bPreviousResumeDrive = xContext.m_bResumeDrive;
		xContext.m_bResumeDrive = xSource.m_pxTypeInfo != nullptr
			&& xSource.m_pxTypeInfo->m_eEventType != GRAPH_EVENT_ON_UPDATE
			&& xSource.m_pxTypeInfo->m_eEventType != GRAPH_EVENT_ON_FIXED_UPDATE;
		RunChainFromPin(xSource.m_uNodeID, 0, xContext);
		xContext.m_bResumeDrive = bPreviousResumeDrive;
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
		// Every anchor in this list is one-shot (the periodic ones are never
		// pushed into it), so every drive from here is a resume drive - no
		// per-anchor predicate. Restored afterwards: the context is the
		// caller's and is reused by the next source and the next frame.
		const bool bPreviousResumeDrive = xContext.m_bResumeDrive;
		xContext.m_bResumeDrive = true;

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

		xContext.m_bResumeDrive = bPreviousResumeDrive;
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
	// A resuming chain re-executes its suspended node WITHOUT re-entering it -
	// one OnEnter per run of a node, however many RUNNING ticks it spans.
	bool bResuming = false;
	if (puResume)
	{
		uCurrent = *puResume;
		bResuming = true;
	}
	else
	{
		uCurrent = FindSuccessor(uNodeID, uPin);
	}

	if (uCurrent == 0)
	{
		return GRAPH_NODE_STATUS_SUCCESS;	// empty chain
	}

	u_int uSteps = 0;
	while (uCurrent != 0)
	{
		// Cycle guard - see uGRAPH_MAX_CHAIN_STEPS. Counted per WALK, so a chain
		// that legitimately runs thousands of nodes over many fires is unaffected.
		if (++uSteps > uGRAPH_MAX_CHAIN_STEPS)
		{
			if (!m_bChainStepCapHit)
			{
				Zenith_Error(LOG_CATEGORY_CORE,
					"BehaviourGraph: chain from (node %u, pin %u) exceeded %u steps - a cyclic exec wiring; aborting the walk",
					uNodeID, uPin, uGRAPH_MAX_CHAIN_STEPS);
				m_bChainStepCapHit = true;
			}
			m_xChainCursors.Remove(ulKey);
			return GRAPH_NODE_STATUS_FAILURE;
		}

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
		if (!bResuming)
		{
			pxInstance->m_pxNode->OnEnter(xContext);
		}
		bResuming = false;	// only the resume-target node skips OnEnter
		const GraphNodeStatus eStatus = pxInstance->m_pxNode->Execute(xContext);
		if (eStatus != GRAPH_NODE_STATUS_RUNNING)
		{
			pxInstance->m_pxNode->OnExit(xContext);
		}
		m_uExecutingNodeID = uPreviousExecuting;

		if (eStatus == GRAPH_NODE_STATUS_RUNNING)
		{
			m_xChainCursors[ulKey] = uCurrent;
			return GRAPH_NODE_STATUS_RUNNING;
		}
		if (eStatus == GRAPH_NODE_STATUS_FAILURE)
		{
			// Routable failure. The flag is read off the SOURCE TYPE, never
			// inferred from the edge: an edge at this index on an unflagged type
			// is inert (today's abort), so a stray wire can never quietly change
			// how a node fails. m_pxNode non-null implies m_pxTypeInfo non-null
			// (InitialiseFromDefinition only creates the node when the type
			// resolved), so no null check here.
			//
			// The failing node has ALREADY had OnExit (above) - failing is a
			// completed run of that node, whatever happens to the chain next.
			const u_int uHandler = pxInstance->m_pxTypeInfo->m_bHasFailurePin
				? FindSuccessor(uCurrent, pxInstance->m_pxTypeInfo->m_uExecOutputCount) : 0u;
			if (uHandler == 0)
			{
				m_xChainCursors.Remove(ulKey);
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// Continue the SAME walk under the SAME key (ulKey is the anchor's,
			// captured before the loop), and deliberately do NOT clear the cursor:
			// a handler that returns RUNNING must write it below, and a handler
			// chain that completes clears it at the bottom. The chain's status is
			// therefore the continuation's terminal status.
			uCurrent = uHandler;
			continue;
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

void Zenith_BehaviourGraph::AbortChain(u_int uNodeID, u_int uPin, Zenith_GraphContext& xContext)
{
	const u_int64 ulKey = MakeChainKey(uNodeID, uPin);
	const u_int* puCursor = m_xChainCursors.TryGet(ulKey);
	if (!puCursor)
	{
		return;	// nothing suspended on this pin
	}
	const u_int uCursorNode = *puCursor;

	// Clear the cursor BEFORE the hook so a fresh run of this pin starts from
	// the chain head even if OnAbort itself re-enters graph machinery.
	m_xChainCursors.Remove(ulKey);

	NodeInstance* pxInstance = FindInstance(uCursorNode);
	if (pxInstance && pxInstance->m_pxNode)
	{
		// Suspended flow nodes forward the abort into their active pins from
		// their OnAbort override (the cascade that resets a whole sub-tree).
		pxInstance->m_pxNode->OnAbort(xContext);
	}

	// An aborted one-shot anchor must not be re-driven by the next ON_UPDATE.
	if (uPin == 0)
	{
		for (u_int u = 0; u < m_auSuspendedOneShotAnchors.GetSize(); ++u)
		{
			if (m_auSuspendedOneShotAnchors.Get(u) == uNodeID)
			{
				m_auSuspendedOneShotAnchors.Remove(u);
				break;
			}
		}
	}
}

void Zenith_BehaviourGraph::AbortAllChains(Zenith_GraphContext& xContext)
{
	// Snapshot keys first - AbortChain mutates the cursor map.
	Zenith_Vector<u_int64> aulKeys;
	for (Zenith_HashMap<u_int64, u_int>::Iterator xIt(m_xChainCursors); !xIt.Done(); xIt.Next())
	{
		aulKeys.PushBack(xIt.GetKey());
	}
	for (u_int u = 0; u < aulKeys.GetSize(); ++u)
	{
		const u_int64 ulKey = aulKeys.Get(u);
		AbortChain(static_cast<u_int>(ulKey >> 8), static_cast<u_int>(ulKey & 0xFFull), xContext);
	}
	m_auSuspendedOneShotAnchors.Clear();
}

GraphNodeStatus Zenith_BehaviourGraph::RunGraphCall(Zenith_GraphContext& xContext)
{
	Zenith_Vector<u_int> auSources;
	for (u_int u = 0; u < m_auEventSources[GRAPH_EVENT_ON_GRAPH_CALL].GetSize(); ++u)
	{
		auSources.PushBack(m_auEventSources[GRAPH_EVENT_ON_GRAPH_CALL].Get(u));
	}
	if (auSources.GetSize() == 0)
	{
		return GRAPH_NODE_STATUS_SUCCESS;	// callable graph without an entry anchor = no-op
	}

	bool bAnyRunning = false;
	bool bAnyCompleted = false;
	for (u_int u = 0; u < auSources.GetSize(); ++u)
	{
		NodeInstance* pxSource = FindInstance(auSources.Get(u));
		if (!pxSource || !pxSource->m_pxNode)
		{
			continue;
		}
		GraphNodeStatus eStatus;
		if (m_xChainCursors.Contains(MakeChainKey(pxSource->m_uNodeID, 0)))
		{
			// A suspended call resumes in place of re-firing its anchor.
			eStatus = RunChainFromPin(pxSource->m_uNodeID, 0, xContext);
		}
		else
		{
			m_uExecutingNodeID = pxSource->m_uNodeID;
			const GraphNodeStatus eGate = pxSource->m_pxNode->Execute(xContext);
			m_uExecutingNodeID = 0;
			if (eGate != GRAPH_NODE_STATUS_SUCCESS)
			{
				continue;
			}
			eStatus = RunChainFromPin(pxSource->m_uNodeID, 0, xContext);
		}
		if (eStatus == GRAPH_NODE_STATUS_RUNNING)
		{
			bAnyRunning = true;
		}
		else if (eStatus == GRAPH_NODE_STATUS_SUCCESS)
		{
			bAnyCompleted = true;
		}
	}
	if (bAnyRunning)
	{
		return GRAPH_NODE_STATUS_RUNNING;
	}
	return bAnyCompleted ? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
}

Zenith_Entity Zenith_GraphContext::ResolveTargetEntity(const std::string& strTargetVar) const
{
	if (strTargetVar.empty())
	{
		return m_xSelf;
	}
	if (m_pxBlackboard == nullptr)
	{
		return Zenith_Entity();
	}
	const u_int64 ulPacked = m_pxBlackboard->GetPackedEntityID(strTargetVar, 0);
	if (ulPacked == 0)
	{
		return Zenith_Entity();	// missing var / wrong type / never-stored
	}
	// Resolves the entity's own owning scene (correct across additive scenes +
	// DontDestroyOnLoad); stale generations come back invalid.
	return Zenith_SceneSystem::Get().ResolveEntity(Zenith_EntityID::FromPacked(ulPacked));
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
