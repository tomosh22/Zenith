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
	// Pin state is DERIVED from these properties (var names, the const pointer, the
	// instance-resolved slot type), so a param write invalidates it. The graph's own
	// BuildPinState always follows this call; a directly-configured instance rebuilds
	// on its next accessor.
	pxNode->m_bPinStateBuilt = false;
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
	//
	// ★ THE REGISTRY IS DRAINED FIRST AND ALWAYS PASSED, so this tier's answer is
	// DETERMINISTIC rather than boot-phase dependent. Two of its checks (the
	// pure-data cycle and the static-vs-static wire mismatch) need the node
	// library; asking "is the registry initialised yet" would make an asset read
	// before the registrar is installed answer differently from the same read
	// afterwards. A build with no registrar (the Sentinel link proofs) gets an
	// initialised-but-EMPTY registry: every node is unregistered, so neither
	// registry-dependent check has anything resolved to look at and the tier is
	// exactly B-1's.
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	Zenith_Vector<Zenith_GraphValidationFinding> axLoadSafety;
	Zenith_GraphDefinitionValidator::ValidateLoadSafety(*this, xRegistry, axLoadSafety);
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

	// Every instance exists now, so the pin state can be built (a data edge may
	// name a node that appears later in the definition).
	for (u_int u = 0; u < m_axNodes.GetSize(); ++u)
	{
		BuildPinState(xDefinition, m_axNodes.Get(u));
	}

	for (u_int u = 0; u < xDefinition.GetEdgeCount(); ++u)
	{
		const Zenith_GraphEdge& xEdge = xDefinition.GetEdgeAt(u);

		// An exec edge INTO a pure node is DROPPED rather than copied: a pure
		// node has no exec input and no chain lifecycle, so walking into one
		// would run it outside any gather and fire OnEnter/OnExit on a node whose
		// whole contract says it has neither. The chain simply ends here
		// (FindSuccessor finds nothing -> SUCCESS). B-3 makes it an author-time
		// error; today it is one line and a dropped wire.
		const NodeInstance* pxDst = FindInstance(xEdge.m_uDstNodeID);
		if (pxDst != nullptr && pxDst->m_pxTypeInfo != nullptr && pxDst->m_pxTypeInfo->m_bPureNode)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] exec edge (node %u, pin %u) -> node %u dropped: '%s' is a PURE node and has no exec input",
				xEdge.m_uSrcNodeID, xEdge.m_uSrcPin, xEdge.m_uDstNodeID,
				pxDst->m_pxTypeInfo->m_strTypeName.c_str());
			continue;
		}

		m_axEdges.PushBack(xEdge);
	}

	ResolveDataEdges(xDefinition);

	for (u_int u = 0; u < xDefinition.GetVariableCount(); ++u)
	{
		const Zenith_GraphVariableDecl& xDecl = xDefinition.GetVariableAt(u);
		m_xBlackboard.SetValue(xDecl.m_strName, xDecl.m_xDefault);
	}

	return true;
}

//------------------------------------------------------------------------------
// Pin resolution (B-2)
//------------------------------------------------------------------------------

void Zenith_BehaviourGraph::BuildPinState(const Zenith_GraphDefinition& xDefinition, NodeInstance& xInstance)
{
	if (xInstance.m_pxNode == nullptr || xInstance.m_pxTypeInfo == nullptr)
	{
		return;	// unresolved node: no instance to carry state
	}
	const Zenith_GraphPinTable* pxPins = xInstance.m_pxTypeInfo->m_pfnGetPinTable
		? xInstance.m_pxTypeInfo->m_pfnGetPinTable() : nullptr;
	if (pxPins == nullptr || pxPins->GetPinCount() == 0)
	{
		return;	// OPAQUE node: the arrays stay empty and cost nothing
	}
	// ★ THE REGISTRY'S TABLE, not the node's GetPropertyTableVirtual(). For an
	// inheriting family the virtual resolves to the PIN-TABLE OWNER's property
	// table, which is the right answer for self-binding (it has nothing else) but
	// is not necessarily the table the registry recorded for this TYPE. The graph
	// knows the type; the node does not.
	const Zenith_PropertyTable* pxProperties = xInstance.m_pxTypeInfo->m_pfnGetPropertyTable
		? xInstance.m_pxTypeInfo->m_pfnGetPropertyTable() : nullptr;

	xInstance.m_pxNode->BuildPinStateFromTables(*pxPins, pxProperties, &xDefinition,
		xInstance.m_pxTypeInfo->m_bVariadicNameCollision);
}

// The builder core, on the NODE so a directly-constructed instance can run it
// against its own tables (EnsurePinState). Behaviour with a definition is
// identical to the Zenith_BehaviourGraph member this was lifted out of - the B-2
// and B-3 units prove it.
void Zenith_GraphNode::BuildPinStateFromTables(const Zenith_GraphPinTable& xPins,
	const Zenith_PropertyTable* pxProperties, const Zenith_GraphDefinition* pxDefinition,
	bool bVariadicNameCollision)
{
	// ★ CLEARED, not merely reserved: this may be the SECOND build of one instance
	// (ApplyNodeParams resets the flag, and the graph's build always follows a
	// self-binding one), and appending would leave pins 0..N-1 pointing at stale
	// bindings while the wires resolved against the new tail.
	m_axInputs.Clear();
	m_axOutputs.Clear();
	m_axVariadicInputs.Clear();
	m_bPinStateBuilt = true;

	Zenith_GraphNode& xNode = *this;
	const u_int uPinCount = xPins.GetPinCount();
	xNode.m_axInputs.Reserve(uPinCount);
	xNode.m_axOutputs.Reserve(uPinCount);

	for (u_int uPin = 0; uPin < uPinCount; ++uPin)
	{
		const Zenith_GraphPinDesc& xDesc = xPins.GetPinAt(uPin);

		// The bound variable name, read EXACTLY the way the validator reads it
		// (one lifted helper), including the empty-primary fallback.
		std::string strVar;
		if (Zenith_GraphPin_ReadStringProperty(pxProperties, &xNode, xDesc.m_szVarNameProperty, strVar)
			== GRAPH_PIN_READ_PROPERTY_INVALID)
		{
			strVar.clear();
		}
		if (strVar.empty())
		{
			std::string strFallback;
			if (Zenith_GraphPin_ReadStringProperty(pxProperties, &xNode, xDesc.m_szFallbackVarNameProperty, strFallback)
				== GRAPH_PIN_READ_PROPERTY_OK)
			{
				strVar = strFallback;
			}
		}

		Zenith_GraphNode::InputBinding xBinding;
		Zenith_GraphNode::OutputSlot xSlot;

		if (xDesc.m_eRole == GRAPH_PIN_ROLE_INPUT)
		{
			xBinding.m_bIsInput = true;
			xBinding.m_strVarName = strVar;
			if (pxProperties != nullptr && xDesc.m_szConstProperty != nullptr && xDesc.m_szConstProperty[0] != '\0')
			{
				const Zenith_ReflectedProperty* pxConst = pxProperties->FindProperty(xDesc.m_szConstProperty);
				if (pxConst != nullptr && pxConst->m_pfnGet != nullptr)
				{
					xBinding.m_pxConstProperty = pxConst;
				}
			}
		}
		else if (xDesc.m_eRole == GRAPH_PIN_ROLE_OUTPUT)
		{
			xSlot.m_bIsOutput = true;
			xSlot.m_strVarName = strVar;
			xSlot.m_eDeclaredType = xDesc.m_eType;
			if (xDesc.m_szTypeFromVarNameProperty != nullptr && xDesc.m_szTypeFromVarNameProperty[0] != '\0')
			{
				// ★ THE TYPE COMES FROM THE DECLARATION, not from the live
				// blackboard: an ApplyOverridesFrom override can carry a different
				// tag, and a slot typed off one would disagree with every consumer
				// the validator checked. An undeclared variable leaves the slot ANY
				// (the declare-or-error rule reports the read itself) - and so does a
				// NULL definition, which is the self-binding case: a
				// directly-constructed node can see no declarations at all.
				std::string strTypeVar;
				xSlot.m_eDeclaredType = eGRAPH_PIN_TYPE_ANY;
				if (pxDefinition != nullptr
					&& Zenith_GraphPin_ReadStringProperty(pxProperties, &xNode, xDesc.m_szTypeFromVarNameProperty, strTypeVar)
					== GRAPH_PIN_READ_PROPERTY_OK)
				{
					for (u_int uVar = 0; uVar < pxDefinition->GetVariableCount(); ++uVar)
					{
						if (pxDefinition->GetVariableAt(uVar).m_strName == strTypeVar)
						{
							xSlot.m_eDeclaredType = pxDefinition->GetVariableAt(uVar).m_xDefault.GetType();
							break;
						}
					}
				}
			}
			else if (xDesc.m_bInstanceResolved)
			{
				// Asked ONCE, here, exactly as the validator asks it. A node that
				// DECLINES leaves the slot ANY - never a fabricated type.
				Zenith_PropertyType eResolved = eGRAPH_PIN_TYPE_ANY;
				xSlot.m_eDeclaredType = (xNode.GetPinType(uPin, eResolved) && eResolved < PROPERTY_TYPE_COUNT)
					? eResolved : eGRAPH_PIN_TYPE_ANY;
			}
			if (xSlot.m_eDeclaredType != eGRAPH_PIN_TYPE_ANY)
			{
				// A TYPED slot starts at its declared default, STAMPED with its
				// own type (an untyped zero would DebugBreak a typed consumer).
				// ANY / declined-instance-resolved slots have no zero and start
				// UNSET, which is what makes "the producer has not run yet"
				// observable.
				xSlot.m_xValue = Zenith_GraphPin_MakeZeroValue(xSlot.m_eDeclaredType);
				xSlot.m_bSet = true;
			}
		}

		xNode.m_axInputs.PushBack(xBinding);
		xNode.m_axOutputs.PushBack(xSlot);

		// A variadic family expands into <count> ordinal members, addressed
		// through the ordinal overload of GetInput. The count is read off the
		// PARAM-APPLIED instance, so it is whatever the node was configured with.
		if (xDesc.m_eRole == GRAPH_PIN_ROLE_INPUT && xDesc.m_bVariadic && !bVariadicNameCollision)
		{
			// Clamped exactly like the exec-pin count
			// (Zenith_GraphNodeRegistry::GetExecOutputCount): a node that answers
			// a nonsense count must cost a bounded amount of memory, not an
			// unbounded one.
			int32_t iCount = xNode.GetDynamicDataInputCount();
			if (iCount > 255)
			{
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphPin] node %u:%s reports %d variadic members on pin '%s'; clamped to 255",
					xNode.m_uNodeID, xNode.GetTypeName(), iCount,
					xDesc.m_szName ? xDesc.m_szName : "(null)");
				iCount = 255;
			}
			for (int32_t i = 0; i < iCount; ++i)
			{
				Zenith_GraphNode::VariadicInput xMember;
				xMember.m_xBinding = xBinding;
				xMember.m_uPinIndex = uPin;
				xMember.m_uOrdinal = static_cast<u_int>(i);
				xNode.m_axVariadicInputs.PushBack(xMember);
			}
		}
	}
}

void Zenith_BehaviourGraph::ResolveDataEdges(const Zenith_GraphDefinition& xDefinition)
{
	for (u_int u = 0; u < xDefinition.GetDataEdgeCount(); ++u)
	{
		const Zenith_GraphDataEdge& xEdge = xDefinition.GetDataEdgeAt(u);

		// Every refusal below leaves the edge in the DEFINITION untouched (a
		// build that merely lacks a node version must still round-trip the asset)
		// and reports exactly once, naming both endpoints.
		NodeInstance* pxDst = FindInstance(xEdge.m_uDstNodeID);
		NodeInstance* pxSrc = FindInstance(xEdge.m_uSrcNodeID);
		if (pxDst == nullptr || pxDst->m_pxNode == nullptr || pxSrc == nullptr || pxSrc->m_pxNode == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: an endpoint is unresolved in this build",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}

		const Zenith_GraphPinTable* pxDstPins = pxDst->m_pxTypeInfo->m_pfnGetPinTable
			? pxDst->m_pxTypeInfo->m_pfnGetPinTable() : nullptr;
		const Zenith_GraphPinTable* pxSrcPins = pxSrc->m_pxTypeInfo->m_pfnGetPinTable
			? pxSrc->m_pxTypeInfo->m_pfnGetPinTable() : nullptr;
		if (pxDstPins == nullptr || pxDstPins->GetPinCount() == 0
			|| pxSrcPins == nullptr || pxSrcPins->GetPinCount() == 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: an endpoint type is OPAQUE (declares no pin table)",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}

		// --- the SOURCE pin: an exact OUTPUT name, never an ordinal -----------
		const u_int uSrcPin = pxSrcPins->FindPinIndex(xEdge.m_strSrcPin.c_str());
		if (uSrcPin >= pxSrcPins->GetPinCount())
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: '%s' declares no pin '%s'",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
				pxSrc->m_pxTypeInfo->m_strTypeName.c_str(), xEdge.m_strSrcPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}
		if (pxSrcPins->GetPinAt(uSrcPin).m_eRole != GRAPH_PIN_ROLE_OUTPUT)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: source pin '%s' is not an OUTPUT",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
				xEdge.m_strSrcPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}

		// --- the DESTINATION pin: exact name first, then a variadic ordinal ---
		u_int uDstPin = pxDstPins->FindPinIndex(xEdge.m_strDstPin.c_str());
		u_int uDstOrdinal = Zenith_GraphNode::uGRAPH_PIN_NO_ORDINAL;
		if (uDstPin < pxDstPins->GetPinCount() && pxDstPins->GetPinAt(uDstPin).m_bVariadic)
		{
			// The BARE family name ("in" rather than "in0"). A family has no
			// non-ordinal member, so binding it would store a wire on a binding
			// no accessor can ever address: refused, not silently accepted.
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: '%s' is a variadic FAMILY name; a wire must name a member ('%s0', '%s1', ...)",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
				xEdge.m_strDstPin.c_str(), xEdge.m_strDstPin.c_str(), xEdge.m_strDstPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}
		if (uDstPin >= pxDstPins->GetPinCount())
		{
			// Split a trailing decimal ordinal; the remaining prefix must EXACTLY
			// name a variadic family, and the ordinal must be inside the
			// param-applied instance's member count.
			const std::string& strName = xEdge.m_strDstPin;
			size_t uDigitStart = strName.size();
			while (uDigitStart > 0 && strName[uDigitStart - 1] >= '0' && strName[uDigitStart - 1] <= '9')
			{
				--uDigitStart;
			}
			if (uDigitStart == 0 || uDigitStart == strName.size())
			{
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphPin] data edge %u:%s -> %u:%s skipped: '%s' declares no pin '%s'",
					xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
					pxDst->m_pxTypeInfo->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str());
				++m_uResolutionSkipCount;
				continue;
			}
			const std::string strFamily = strName.substr(0, uDigitStart);
			const u_int uFamilyPin = pxDstPins->FindPinIndex(strFamily.c_str());
			if (uFamilyPin >= pxDstPins->GetPinCount() || !pxDstPins->GetPinAt(uFamilyPin).m_bVariadic
				|| pxDst->m_pxTypeInfo->m_bVariadicNameCollision)
			{
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphPin] data edge %u:%s -> %u:%s skipped: '%s' declares no pin '%s' and no variadic family '%s'",
					xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
					pxDst->m_pxTypeInfo->m_strTypeName.c_str(), xEdge.m_strDstPin.c_str(), strFamily.c_str());
				++m_uResolutionSkipCount;
				continue;
			}
			// Clamped exactly as BuildPinState clamps the expansion, so the
			// ordinal check and the bindings that exist cannot disagree.
			int32_t iCount = pxDst->m_pxNode->GetDynamicDataInputCount();
			if (iCount > 255)
			{
				iCount = 255;
			}
			u_int uOrdinal = 0;
			bool bOrdinalOverflowed = false;
			for (size_t uAt = uDigitStart; uAt < strName.size(); ++uAt)
			{
				if (uOrdinal > 0xFFFFFFu)
				{
					bOrdinalOverflowed = true;	// far past any member count; treated as past the end
					break;
				}
				uOrdinal = uOrdinal * 10u + static_cast<u_int>(strName[uAt] - '0');
			}
			if (bOrdinalOverflowed || iCount < 0 || uOrdinal >= static_cast<u_int>(iCount))
			{
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphPin] data edge %u:%s -> %u:%s skipped: variadic family '%s' has %d members, ordinal %u is past the end",
					xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
					strFamily.c_str(), iCount, uOrdinal);
				++m_uResolutionSkipCount;
				continue;
			}
			uDstPin = uFamilyPin;
			uDstOrdinal = uOrdinal;
		}
		if (pxDstPins->GetPinAt(uDstPin).m_eRole != GRAPH_PIN_ROLE_INPUT)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: destination pin '%s' is not an INPUT",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str(),
				xEdge.m_strDstPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}

		Zenith_GraphNode::InputBinding* pxBinding = pxDst->m_pxNode->FindInputBinding(uDstPin, uDstOrdinal);
		if (pxBinding == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] data edge %u:%s -> %u:%s skipped: the destination pin has no resolved binding",
				xEdge.m_uSrcNodeID, xEdge.m_strSrcPin.c_str(), xEdge.m_uDstNodeID, xEdge.m_strDstPin.c_str());
			++m_uResolutionSkipCount;
			continue;
		}
		pxBinding->m_bConnected = true;
		pxBinding->m_uSrcNodeID = xEdge.m_uSrcNodeID;
		pxBinding->m_uSrcSlot = uSrcPin;
	}
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
	m_ulCurrentGather = 0;
	m_ulNextGather = 0;
	m_uResolutionSkipCount = 0;
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
	// A fresh GATHER token for this Execute (B-2): any pure node this source
	// pulls is evaluated once for it, and the outer token is restored after.
	const u_int64 ulOuterGather = m_ulCurrentGather;
	m_ulCurrentGather = ++m_ulNextGather;
	const GraphNodeStatus eGate = xSource.m_pxNode->Execute(xContext);
	m_ulCurrentGather = ulOuterGather;
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
		// A fresh GATHER token per Execute (B-2). OnEnter/OnExit are OUTSIDE it
		// on purpose: a pull is legal only from Execute.
		const u_int64 ulOuterGather = m_ulCurrentGather;
		m_ulCurrentGather = ++m_ulNextGather;
		const GraphNodeStatus eStatus = pxInstance->m_pxNode->Execute(xContext);
		m_ulCurrentGather = ulOuterGather;
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
			const u_int64 ulOuterGather = m_ulCurrentGather;	// a fresh GATHER token per Execute (B-2)
			m_ulCurrentGather = ++m_ulNextGather;
			const GraphNodeStatus eGate = pxSource->m_pxNode->Execute(xContext);
			m_ulCurrentGather = ulOuterGather;
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

//==============================================================================
// The pull evaluator (B-2)
//==============================================================================

const Zenith_PropertyValue* Zenith_BehaviourGraph::PullSlot(u_int uSrcNodeID, u_int uSrcSlot, Zenith_GraphContext& xContext)
{
	NodeInstance* pxSrc = FindInstance(uSrcNodeID);
	if (pxSrc == nullptr || pxSrc->m_pxNode == nullptr || pxSrc->m_pxTypeInfo == nullptr)
	{
		return nullptr;	// unresolved source: the consumer takes its default
	}
	Zenith_GraphNode& xNode = *pxSrc->m_pxNode;

	if (pxSrc->m_pxTypeInfo->m_bPureNode)
	{
		if (xNode.m_bEvaluating)
		{
			// A runtime data CYCLE. One warning per instance (a node with two
			// cyclic inputs names one of them), then the consumer takes its pin
			// default - never a hang and never an assert.
			if (xNode.m_uCycleWarningCount == 0)
			{
				Zenith_Log(LOG_CATEGORY_CORE,
					"[GraphPin] CYCLE node=%u:%s is already evaluating; the pull yields the consumer's pin default",
					uSrcNodeID, pxSrc->m_pxTypeInfo->m_strTypeName.c_str());
				++xNode.m_uCycleWarningCount;
			}
			return nullptr;
		}

		// MEMO. A token only ever increases, so ">=" means "already computed for
		// this gather, or for one nested inside it": a flow node that pulls, runs
		// a sub-chain that pulls the same source, and pulls again gets TWO
		// evaluations (its own and the child's), not three.
		const bool bMemoHit = xNode.m_bMemoValid && m_ulCurrentGather != 0 && xNode.m_ulMemoGather >= m_ulCurrentGather;
		if (!bMemoHit)
		{
			// No OnEnter/OnExit/OnAbort: a pure node has no chain lifecycle.
			xNode.m_bEvaluating = true;
			const GraphNodeStatus eStatus = xNode.Execute(xContext);
			xNode.m_bEvaluating = false;
			if (eStatus != GRAPH_NODE_STATUS_SUCCESS)
			{
				// RUNNING is refused the same way as FAILURE: a pure node has no
				// cursor to suspend on, so "not SUCCESS" means "this slot must
				// not be read".
				if (xNode.m_uPureStatusWarningCount == 0)
				{
					Zenith_Log(LOG_CATEGORY_CORE,
						"[GraphPin] STATUS node=%u:%s pure evaluation returned %u (not SUCCESS); the pull yields the consumer's pin default",
						uSrcNodeID, pxSrc->m_pxTypeInfo->m_strTypeName.c_str(), static_cast<u_int>(eStatus));
					++xNode.m_uPureStatusWarningCount;
				}
				return nullptr;
			}
			xNode.m_ulMemoGather = m_ulCurrentGather;
			xNode.m_bMemoValid = true;
		}

		// ★ A PULLED PURE NODE JOINS THE EXECUTION TRACE (B-4). The editor's live
		// highlighting reads m_auRecentlyExecuted, so without this a pure node that
		// really did run - every frame, on demand - was the one kind of node that
		// never lit up, and an author debugging a wire had no way to see whether
		// their producer was reached at all.
		//
		// On an evaluation AND on a MEMO HIT: the node is feeding this frame's
		// consumers either way, and lighting it only on the frames the memo happens
		// to miss would flicker for reasons the author cannot see. The non-SUCCESS
		// and CYCLE paths above returned already - those pulls yield the consumer's
		// default, so nothing of this node reached anybody.
		//
		// DEDUPLICATED, unlike RunChainFromPin's push: one pure source commonly
		// feeds many consumers in one frame, and the reader scans this vector
		// LINEARLY for one id - N duplicate entries would crowd out the cap for no
		// added information. Same cap 64, and ungated exactly like the exec push
		// (the trace is cheap and the editor is not the only reader).
		bool bAlreadyTraced = false;
		for (u_int u = 0; u < m_auRecentlyExecuted.GetSize() && !bAlreadyTraced; ++u)
		{
			bAlreadyTraced = m_auRecentlyExecuted.Get(u) == uSrcNodeID;
		}
		if (!bAlreadyTraced && m_auRecentlyExecuted.GetSize() < 64)
		{
			// ★ A PULLED PRODUCER LANDS AFTER ITS CONSUMER IN THE TRACE. The pull
			// happens from inside the consumer's Execute, which was pushed first.
			// The trace is "what ran", not a topological order, and nothing may read
			// it as one.
			m_auRecentlyExecuted.PushBack(uSrcNodeID);
		}
	}

	if (uSrcSlot >= xNode.m_axOutputs.GetSize())
	{
		return nullptr;
	}
	const Zenith_GraphNode::OutputSlot& xSlot = xNode.m_axOutputs.Get(uSrcSlot);
	if (!xSlot.m_bIsOutput || !xSlot.m_bSet)
	{
		return nullptr;	// UNSET: an ANY producer that has not written yet
	}
	return &xSlot.m_xValue;
}

//==============================================================================
// Zenith_GraphNode - the pin accessors (B-2)
//
// Bodies live HERE rather than in a Zenith_GraphNode.cpp: they need
// Zenith_BehaviourGraph::PullSlot and the blackboard, and the node header only
// forward-declares both. Node TUs therefore include nothing new.
//==============================================================================

// LAZY SELF-BINDING (B-6.1). PERMANENT runtime behaviour, not a transitional
// path: a node the graph never resolved, but whose class declares a pin table,
// binds itself the first time an Execute touches an accessor. It gets EXACTLY
// what an unwired graph node gets - the var-name fallback with the const as the
// default, and the dual-write on SetOutput.
//
// The BAD-ACCESS path therefore survives for, and only for: an OPAQUE node (no
// pin table at all - the virtual answers null), an out-of-range or wrong-role
// pin, a CONNECTED pin read through a context with a null m_pxGraph, and a
// var-bound pin read through a context with a null m_pxBlackboard.
//
// Temp instances (the registry's dynamic-pin probe, GetExecOutputCount, the
// validator, the builder, AddNode, the editor's param panel) never call an
// accessor, so the zero-capacity invariant still holds for every one of them. A
// future caller that DOES touch an accessor on a temp instance pays one build.
void Zenith_GraphNode::EnsurePinState()
{
	if (m_bPinStateBuilt)
	{
		return;
	}
	const Zenith_GraphPinTable* pxPins = GetPinTableVirtual();
	if (pxPins == nullptr || pxPins->GetPinCount() == 0)
	{
		// OPAQUE: the arrays stay empty and every accessor bad-accesses. The
		// flag is still set, so an opaque node pays one bool test per call like
		// everyone else instead of a virtual dispatch forever.
		m_bPinStateBuilt = true;
		return;
	}
	// No definition: a from-variable OUTPUT slot has no declaration to read and
	// stays ANY (and therefore UNSET). No collision flag either - that is a
	// property of the registered TYPE, which a bare instance cannot see.
	BuildPinStateFromTables(*pxPins, GetPropertyTableVirtual(), nullptr, false);
}

Zenith_GraphNode::InputBinding* Zenith_GraphNode::FindInputBinding(u_int uPinIndex, u_int uOrdinal)
{
	if (uOrdinal != uGRAPH_PIN_NO_ORDINAL)
	{
		for (u_int u = 0; u < m_axVariadicInputs.GetSize(); ++u)
		{
			VariadicInput& xMember = m_axVariadicInputs.Get(u);
			if (xMember.m_uPinIndex == uPinIndex && xMember.m_uOrdinal == uOrdinal)
			{
				return &xMember.m_xBinding;
			}
		}
		return nullptr;
	}
	// Bounds-checked BEFORE the index: Zenith_Vector::Get asserts, and an assert
	// must be unreachable from every accessor path (an opaque node and every temp
	// instance carry an EMPTY array).
	if (uPinIndex >= m_axInputs.GetSize())
	{
		return nullptr;
	}
	InputBinding& xBinding = m_axInputs.Get(uPinIndex);
	return xBinding.m_bIsInput ? &xBinding : nullptr;
}

const Zenith_GraphNode::InputBinding* Zenith_GraphNode::FindInputBinding(u_int uPinIndex, u_int uOrdinal) const
{
	return const_cast<Zenith_GraphNode*>(this)->FindInputBinding(uPinIndex, uOrdinal);
}

const Zenith_PropertyValue* Zenith_GraphNode::FindTestOverride(u_int uPinIndex, u_int uOrdinal) const
{
	for (u_int u = 0; u < m_axTestOverrides.GetSize(); ++u)
	{
		const TestOverride& xOverride = m_axTestOverrides.Get(u);
		if (xOverride.m_uPinIndex == uPinIndex && xOverride.m_uOrdinal == uOrdinal)
		{
			return &xOverride.m_xValue;
		}
	}
	return nullptr;
}

void Zenith_GraphNode::WarnBadAccess(u_int uPinIndex)
{
	if (m_uBadAccessWarningCount != 0)
	{
		return;	// one line per instance: a hot chain must not spam the log
	}
	Zenith_Log(LOG_CATEGORY_CORE,
		"[GraphPin] BADACCESS node=%u:%s pin=%u is not a resolved pin of the right role (or the context carries no graph/blackboard); the default is used",
		m_uNodeID, GetTypeName(), uPinIndex);
	++m_uBadAccessWarningCount;
}

const Zenith_PropertyValue* Zenith_GraphNode::CheckedExtract(InputBinding& xBinding, const Zenith_PropertyValue& xValue,
	Zenith_PropertyType eExpected, u_int uPinIndex)
{
	if (xValue.GetType() == eExpected)
	{
		return &xValue;
	}
	// A DATA problem, not an engine defect: the validator reports it at author
	// time (B-3), so this is a Zenith_Log and never a Zenith_Error - and never
	// the tagged getter, which would DebugBreak.
	if (xBinding.m_uMismatchWarningCount == 0)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"[GraphPin] MISMATCH node=%u:%s pin=%u expected type %u but the wire carries %u; the pin default is used",
			m_uNodeID, GetTypeName(), uPinIndex, static_cast<u_int>(eExpected), static_cast<u_int>(xValue.GetType()));
		++xBinding.m_uMismatchWarningCount;
	}
	return nullptr;
}

const Zenith_PropertyValue* Zenith_GraphNode::ResolveInput(Zenith_GraphContext& xContext, u_int uPinIndex,
	u_int uOrdinal, Zenith_PropertyType eExpected)
{
	EnsurePinState();
	InputBinding* pxBinding = FindInputBinding(uPinIndex, uOrdinal);
	if (pxBinding == nullptr)
	{
		WarnBadAccess(uPinIndex);
		return nullptr;
	}

	// (a) A test override behaves exactly like a connected wire carrying that
	//     value, mismatch path included.
	const Zenith_PropertyValue* pxOverride = FindTestOverride(uPinIndex, uOrdinal);
	if (pxOverride != nullptr)
	{
		return CheckedExtract(*pxBinding, *pxOverride, eExpected, uPinIndex);
	}

	// (b) Connected: pull the producer's slot.
	if (pxBinding->m_bConnected)
	{
		if (xContext.m_pxGraph == nullptr)
		{
			WarnBadAccess(uPinIndex);
			return nullptr;
		}
		const Zenith_PropertyValue* pxValue =
			xContext.m_pxGraph->PullSlot(pxBinding->m_uSrcNodeID, pxBinding->m_uSrcSlot, xContext);
		if (pxValue == nullptr)
		{
			return nullptr;	// UNSET / cycle / failed pure source -> the pin default
		}
		return CheckedExtract(*pxBinding, *pxValue, eExpected, uPinIndex);
	}

	// (c) TRANSITIONAL var-name fallback - DELETED IN C-1, together with the
	//     dual-write in SetOutput. This IS today's read, exactly: a typed
	//     blackboard getter defaults on a MISSING name and on a type mismatch
	//     alike, with the pin default as its default, and warns about neither.
	if (!pxBinding->m_strVarName.empty())
	{
		if (xContext.m_pxBlackboard == nullptr)
		{
			WarnBadAccess(uPinIndex);
			return nullptr;
		}
		// The census line C-1 waits on: only a MIGRATED node reaches this path,
		// so "no [GraphPin] FALLBACK line in a boot log" is the precondition for
		// deleting it. Once per (instance, pin) - a hot chain must not spam.
		if (pxBinding->m_uFallbackUseCount == 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphPin] FALLBACK node=%u:%s pin=%u var=%s",
				m_uNodeID, GetTypeName(), uPinIndex, pxBinding->m_strVarName.c_str());
			++pxBinding->m_uFallbackUseCount;
		}
		const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(pxBinding->m_strVarName);
		if (pxValue == nullptr || pxValue->GetType() != eExpected)
		{
			return nullptr;
		}
		return pxValue;
	}

	return nullptr;	// unconnected, unbound: the pin default
}

Zenith_PropertyValue Zenith_GraphNode::MakePinDefault(u_int uPinIndex, u_int uOrdinal, Zenith_PropertyType eExpected) const
{
	// ONE definition of "the pin default": the const property's CURRENT value
	// when the descriptor declares one, else the type's zero. Output slots are
	// initialised through the same rule.
	const InputBinding* pxBinding = FindInputBinding(uPinIndex, uOrdinal);
	if (pxBinding != nullptr && pxBinding->m_pxConstProperty != nullptr && pxBinding->m_pxConstProperty->m_pfnGet != nullptr)
	{
		Zenith_PropertyValue xValue;
		pxBinding->m_pxConstProperty->m_pfnGet(this, xValue);
		if (xValue.GetType() == eExpected)
		{
			return xValue;
		}
	}
	return Zenith_GraphPin_MakeZeroValue(eExpected);
}

u_int64 Zenith_GraphNode::GetInputPackedEntityID(Zenith_GraphContext& xContext, u_int uPinIndex)
{
	// Zenith_PropertyTraits has no u_int64 specialisation (Core stays
	// ECS-agnostic), so this is the non-template form of the same contract.
	const Zenith_PropertyValue* pxValue = ResolveInput(xContext, uPinIndex, uGRAPH_PIN_NO_ORDINAL, PROPERTY_TYPE_ENTITY_ID);
	if (pxValue != nullptr)
	{
		return pxValue->GetPackedEntityID();
	}
	return MakePinDefault(uPinIndex, uGRAPH_PIN_NO_ORDINAL, PROPERTY_TYPE_ENTITY_ID).GetPackedEntityID();
}

bool Zenith_GraphNode::TryGetInput(Zenith_GraphContext& xContext, u_int uPinIndex, const Zenith_PropertyValue*& pxOut)
{
	return TryGetInput(xContext, uPinIndex, uGRAPH_PIN_NO_ORDINAL, pxOut);
}

bool Zenith_GraphNode::TryGetInput(Zenith_GraphContext& xContext, u_int uPinIndex, u_int uOrdinal,
	const Zenith_PropertyValue*& pxOut)
{
	// PRESENCE, not agreement: false means "there is no value here". A connected
	// slot whose tag disagrees comes back TRUE with the raw value - the wildcard
	// consumer owns that check (see the truth table in Scripting/CLAUDE.md).
	pxOut = nullptr;

	EnsurePinState();
	InputBinding* pxBinding = FindInputBinding(uPinIndex, uOrdinal);
	if (pxBinding == nullptr)
	{
		WarnBadAccess(uPinIndex);
		return false;
	}

	const Zenith_PropertyValue* pxOverride = FindTestOverride(uPinIndex, uOrdinal);
	if (pxOverride != nullptr)
	{
		pxOut = pxOverride;
		return true;
	}

	if (pxBinding->m_bConnected)
	{
		if (xContext.m_pxGraph == nullptr)
		{
			WarnBadAccess(uPinIndex);
			return false;
		}
		pxOut = xContext.m_pxGraph->PullSlot(pxBinding->m_uSrcNodeID, pxBinding->m_uSrcSlot, xContext);
		return pxOut != nullptr;
	}

	if (!pxBinding->m_strVarName.empty())
	{
		if (xContext.m_pxBlackboard == nullptr)
		{
			WarnBadAccess(uPinIndex);
			return false;
		}
		// The SAME transitional path ResolveInput takes, so it carries the SAME
		// census line - a node migrated onto TryGetInput rather than GetInput must
		// not be invisible to C-1's "zero FALLBACK lines" precondition.
		if (pxBinding->m_uFallbackUseCount == 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE, "[GraphPin] FALLBACK node=%u:%s pin=%u var=%s",
				m_uNodeID, GetTypeName(), uPinIndex, pxBinding->m_strVarName.c_str());
			++pxBinding->m_uFallbackUseCount;
		}
		pxOut = xContext.m_pxBlackboard->TryGetValue(pxBinding->m_strVarName);
		return pxOut != nullptr;
	}

	if (pxBinding->m_pxConstProperty != nullptr && pxBinding->m_pxConstProperty->m_pfnGet != nullptr)
	{
		// A const IS a value. The scratch is per-binding and refreshed on every
		// call, so the pointer is valid until the next call on this pin.
		pxBinding->m_pxConstProperty->m_pfnGet(this, pxBinding->m_xConstScratch);
		pxOut = &pxBinding->m_xConstScratch;
		return true;
	}

	return false;
}

void Zenith_GraphNode::SetOutput(Zenith_GraphContext& xContext, u_int uPinIndex, const Zenith_PropertyValue& xValue)
{
	EnsurePinState();
	if (uPinIndex >= m_axOutputs.GetSize())
	{
		WarnBadAccess(uPinIndex);
		return;
	}
	OutputSlot& xSlot = m_axOutputs.Get(uPinIndex);
	if (!xSlot.m_bIsOutput)
	{
		WarnBadAccess(uPinIndex);
		return;
	}

	// A node writing a type its own declared pin does not carry is an ENGINE
	// defect, not a data problem - so the slot is left UNCHANGED rather than
	// re-tagged under a consumer's feet. ANY slots accept any tag by definition.
	if (xSlot.m_eDeclaredType != eGRAPH_PIN_TYPE_ANY && xValue.GetType() != xSlot.m_eDeclaredType)
	{
		if (xSlot.m_uMismatchWarningCount == 0)
		{
			Zenith_Log(LOG_CATEGORY_CORE,
				"[GraphPin] OUTMISMATCH node=%u:%s pin=%u declares type %u but the node wrote %u; the slot is unchanged",
				m_uNodeID, GetTypeName(), uPinIndex,
				static_cast<u_int>(xSlot.m_eDeclaredType), static_cast<u_int>(xValue.GetType()));
			++xSlot.m_uMismatchWarningCount;
		}
		return;
	}

	xSlot.m_xValue = xValue;
	xSlot.m_bSet = true;

	// TRANSITIONAL dual-write - DELETED IN C-1, together with the var-name
	// fallback in ResolveInput. While the descriptor still binds a var name, a
	// downstream node that has NOT been migrated to GetInput still reads this
	// result off the blackboard exactly as it does today.
	if (!xSlot.m_strVarName.empty() && xContext.m_pxBlackboard != nullptr)
	{
		xContext.m_pxBlackboard->SetValue(xSlot.m_strVarName, xValue);
	}
}

void Zenith_GraphNode::SetInputForTest(u_int uPinIndex, const Zenith_PropertyValue& xValue)
{
	SetInputForTest(uPinIndex, uGRAPH_PIN_NO_ORDINAL, xValue);
}

void Zenith_GraphNode::SetInputForTest(u_int uPinIndex, u_int uOrdinal, const Zenith_PropertyValue& xValue)
{
	for (u_int u = 0; u < m_axTestOverrides.GetSize(); ++u)
	{
		TestOverride& xExisting = m_axTestOverrides.Get(u);
		if (xExisting.m_uPinIndex == uPinIndex && xExisting.m_uOrdinal == uOrdinal)
		{
			xExisting.m_xValue = xValue;
			return;
		}
	}
	TestOverride xOverride;
	xOverride.m_xValue = xValue;
	xOverride.m_uPinIndex = uPinIndex;
	xOverride.m_uOrdinal = uOrdinal;
	m_axTestOverrides.PushBack(xOverride);
}

Zenith_PropertyType Zenith_GraphNode::GetOutputPinType(u_int uPinIndex) const
{
	if (uPinIndex >= m_axOutputs.GetSize())
	{
		return eGRAPH_PIN_TYPE_ANY;
	}
	const OutputSlot& xSlot = m_axOutputs.Get(uPinIndex);
	return xSlot.m_bIsOutput ? xSlot.m_eDeclaredType : eGRAPH_PIN_TYPE_ANY;
}

const Zenith_PropertyValue* Zenith_GraphNode::GetOutputForTest(u_int uPinIndex) const
{
	if (uPinIndex >= m_axOutputs.GetSize())
	{
		return nullptr;
	}
	const OutputSlot& xSlot = m_axOutputs.Get(uPinIndex);
	return (xSlot.m_bIsOutput && xSlot.m_bSet) ? &xSlot.m_xValue : nullptr;
}

u_int Zenith_GraphNode::GetMismatchWarningCountForTest(u_int uPinIndex) const
{
	const InputBinding* pxBinding = FindInputBinding(uPinIndex, uGRAPH_PIN_NO_ORDINAL);
	return pxBinding ? pxBinding->m_uMismatchWarningCount : 0u;
}

u_int Zenith_GraphNode::GetOutputMismatchWarningCountForTest(u_int uPinIndex) const
{
	if (uPinIndex >= m_axOutputs.GetSize())
	{
		return 0u;
	}
	return m_axOutputs.Get(uPinIndex).m_uMismatchWarningCount;
}

u_int Zenith_GraphNode::GetFallbackUseCountForTest(u_int uPinIndex) const
{
	const InputBinding* pxBinding = FindInputBinding(uPinIndex, uGRAPH_PIN_NO_ORDINAL);
	return pxBinding ? pxBinding->m_uFallbackUseCount : 0u;
}

#include "Scripting/Zenith_Scripting.Tests.inl"
