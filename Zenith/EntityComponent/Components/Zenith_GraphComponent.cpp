#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"

namespace
{
	// Current GraphComponent serialization version.
	//   v1 = framed multi-slot blob: per slot path + framed blackboard overrides.
	// v1: framed slot blob (path + values-only blackboard override bytes).
	// v2: identical layout; the override bytes gain the blackboard's list
	//     section. v1 blobs still load (values only) - the ONLY on-disk format
	//     change of the behaviour-graph program.
	constexpr u_int uGRAPH_COMPONENT_VERSION = 2;
	constexpr u_int uGRAPH_COMPONENT_MIN_VERSION = 1;

	// File-scope dispatch depth (the hardened dispatch discipline): the component
	// instance can be relocated by a pool resize triggered from node logic, so
	// post-dispatch bookkeeping must not touch `this`.
	int32_t s_iGraphDispatchDepth = 0;

	void FlushPendingRemovalsViaEntity(Zenith_Entity xParent)
	{
		if (!xParent.IsValid())
		{
			return;
		}
		Zenith_GraphComponent* pxGraph = xParent.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		pxGraph->FlushPendingRemovals();
	}
}

bool Zenith_GraphComponent::IsDispatchInProgress()
{
	return s_iGraphDispatchDepth > 0;
}

//------------------------------------------------------------------------------
// Slots
//------------------------------------------------------------------------------

Zenith_BehaviourGraph* Zenith_GraphComponent::GetGraphAt(u_int uIndex)
{
	Zenith_Assert(uIndex < m_axSlots.GetSize(), "GraphComponent: GetGraphAt index out of range");
	return m_axSlots.Get(uIndex).m_pxGraph;
}

const char* Zenith_GraphComponent::GetGraphAssetPathAt(u_int uIndex) const
{
	Zenith_Assert(uIndex < m_axSlots.GetSize(), "GraphComponent: GetGraphAssetPathAt index out of range");
	return m_axSlots.Get(uIndex).m_strGraphAssetPath.c_str();
}

Zenith_BehaviourGraph* Zenith_GraphComponent::InstantiateSlotGraph(Zenith_GraphSlot& xSlot)
{
	// Anchors the asset TU (and its static asset-type registrar) against
	// /OPT:REF - see Zenith_BehaviourGraphAsset_ForceLink.
	static const bool ls_bAssetLinked = Zenith_BehaviourGraphAsset_ForceLink();
	(void)ls_bAssetLinked;

	Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(xSlot.m_strGraphAssetPath);
	if (!pxAsset || !pxAsset->LoadedOk())
	{
		Zenith_Log(LOG_CATEGORY_ECS,
			"GraphComponent: graph asset '%s' missing or invalid; slot kept unresolved",
			xSlot.m_strGraphAssetPath.c_str());
		return nullptr;
	}

	Zenith_BehaviourGraph* pxGraph = new Zenith_BehaviourGraph();
	pxGraph->InitialiseFromDefinition(pxAsset->GetDefinition());

	// Apply preserved per-entity blackboard overrides. Ad-hoc (undeclared)
	// variables restore verbatim; a type CONFLICT with a declared variable
	// drops the stale override (the asset's declaration wins - never
	// reinterpreted).
	if (xSlot.m_xPendingOverrides.GetCursor() > 0)
	{
		Zenith_DataStream xRead(const_cast<void*>(xSlot.m_xPendingOverrides.GetData()), xSlot.m_xPendingOverrides.GetCursor());
		Zenith_GraphBlackboard xOverrides;
		xOverrides.ReadFromDataStream(xRead, xSlot.m_bOverridesIncludeLists);
		pxGraph->GetBlackboard().ApplyOverridesFrom(xOverrides);
	}

	xSlot.m_pxGraph = pxGraph;
	return pxGraph;
}

Zenith_BehaviourGraph* Zenith_GraphComponent::AddGraphByAssetPath(const char* szAssetPath)
{
	if (!szAssetPath || szAssetPath[0] == '\0')
	{
		return nullptr;
	}

	Zenith_GraphSlot xSlot;
	xSlot.m_strGraphAssetPath = Zenith_AssetRegistry::NormalizeAssetPath(szAssetPath);
	m_axSlots.PushBack(std::move(xSlot));

	return InstantiateSlotGraph(m_axSlots.Get(m_axSlots.GetSize() - 1));
}

void Zenith_GraphComponent::RemoveGraphAt(u_int uIndex)
{
	if (uIndex >= m_axSlots.GetSize())
	{
		return;
	}
	Zenith_GraphSlot& xSlot = m_axSlots.Get(uIndex);
	if (xSlot.m_bMarkedForRemoval)
	{
		return;
	}
	if (IsDispatchInProgress())
	{
		xSlot.m_bMarkedForRemoval = true;
		return;
	}
	m_axSlots.Remove(uIndex);	// slot dtor deletes the graph
}

void Zenith_GraphComponent::RemoveAllGraphs()
{
	m_axSlots.Clear();
}

void Zenith_GraphComponent::FlushPendingRemovals()
{
	for (u_int u = m_axSlots.GetSize(); u > 0; --u)
	{
		const u_int uIndex = u - 1;
		if (m_axSlots.Get(uIndex).m_bMarkedForRemoval)
		{
			m_axSlots.Remove(uIndex);
		}
	}
}

//------------------------------------------------------------------------------
// Dispatch
//------------------------------------------------------------------------------

void Zenith_GraphComponent::FireEventOnSlots(GraphEventType eEvent, float fDt, const Zenith_PropertyValue* pxPayload, bool bReverse)
{
	// Snapshot heap-stable graph pointers before running user logic. ON_UPDATE
	// additionally drives Timer ticking + suspended-chain resumption, but only
	// for graphs that actually need it (NeedsUpdateDispatch) - a graph anchored
	// purely on collisions/custom events is skipped entirely, so idle graphs
	// cost no dispatch and no snapshot allocation.
	Zenith_Vector<Zenith_BehaviourGraph*> axGraphs;
	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_GraphSlot& xSlot = m_axSlots.Get(u);
		if (xSlot.m_bMarkedForRemoval || !xSlot.m_pxGraph)
		{
			continue;
		}
		const bool bWantsDispatch = (eEvent == GRAPH_EVENT_ON_UPDATE)
			? xSlot.m_pxGraph->NeedsUpdateDispatch()
			: xSlot.m_pxGraph->HasEventSource(eEvent);
		if (bWantsDispatch)
		{
			axGraphs.PushBack(xSlot.m_pxGraph);
		}
	}
	if (axGraphs.GetSize() == 0)
	{
		return;
	}

	const Zenith_Entity xParent = m_xParentEntity;	// copy - `this` may relocate

	++s_iGraphDispatchDepth;
	const u_int uCount = axGraphs.GetSize();
	for (u_int u = 0; u < uCount; ++u)
	{
		Zenith_BehaviourGraph* pxGraph = bReverse ? axGraphs.Get(uCount - 1 - u) : axGraphs.Get(u);
		Zenith_GraphContext xContext;
		xContext.m_xSelf = xParent;
		xContext.m_fDt = fDt;
		xContext.m_fTimeSeconds = g_xEngine.Frame().GetTimePassed();
		xContext.m_pxGraph = pxGraph;
		xContext.m_pxBlackboard = &pxGraph->GetBlackboard();
		xContext.m_pxEventPayload = pxPayload;
		pxGraph->FireEvent(eEvent, xContext);
	}
	--s_iGraphDispatchDepth;

	if (s_iGraphDispatchDepth == 0)
	{
		FlushPendingRemovalsViaEntity(xParent);
	}
}

void Zenith_GraphComponent::FireCustomEvent(const char* szName, const Zenith_PropertyValue* pxPayload)
{
	FireCustomEventWithArgs(szName, nullptr, 0, pxPayload);
}

void Zenith_GraphComponent::BroadcastCustomEvent(const char* szName, const Zenith_PropertyValue* pxPayload)
{
	if (!szName)
	{
		return;
	}
	// Snapshot the receiving components first (the hardened-dispatch lesson:
	// a fired chain may add/remove components mid-iteration).
	Zenith_Vector<Zenith_EntityID> axReceivers;
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	xScenes.QueryAllScenes<Zenith_GraphComponent>()
		.ForEach([&axReceivers](Zenith_EntityID xID, Zenith_GraphComponent&)
	{
		axReceivers.PushBack(xID);
	});
	for (u_int u = 0; u < axReceivers.GetSize(); ++u)
	{
		Zenith_Entity xEntity = xScenes.ResolveEntity(axReceivers.Get(u));
		if (!xEntity.IsValid())
		{
			continue;	// destroyed by an earlier receiver's chain
		}
		if (Zenith_GraphComponent* pxComponent = xEntity.TryGetComponent<Zenith_GraphComponent>())
		{
			pxComponent->FireCustomEvent(szName, pxPayload);
		}
	}
}

void Zenith_GraphComponent::FireCustomEventWithArgs(const char* szName, const Zenith_GraphEventArg* pxArgs, u_int uArgCount)
{
	// Arg 0's value doubles as the legacy single payload so existing
	// stash-the-payload sources keep working under the args form.
	FireCustomEventWithArgs(szName, pxArgs, uArgCount, uArgCount > 0 ? &pxArgs[0].m_xValue : nullptr);
}

void Zenith_GraphComponent::FireCustomEventWithArgs(const char* szName, const Zenith_GraphEventArg* pxArgs, u_int uArgCount, const Zenith_PropertyValue* pxPayload)
{
	if (!szName)
	{
		return;
	}
	// Cross-entity events nest through the depth counter; a hard cap kills
	// event ping-pong recursion (graph A fires at B fires at A ...) instead of
	// overflowing the stack. Reported once per offending cascade.
	if (s_iGraphDispatchDepth >= 16)
	{
		Zenith_Error(LOG_CATEGORY_CORE,
			"GraphComponent: custom event '%s' dropped at dispatch depth %d (event ping-pong recursion?)",
			szName, s_iGraphDispatchDepth);
		return;
	}
	Zenith_Vector<Zenith_BehaviourGraph*> axGraphs;
	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_GraphSlot& xSlot = m_axSlots.Get(u);
		if (!xSlot.m_bMarkedForRemoval && xSlot.m_pxGraph)
		{
			axGraphs.PushBack(xSlot.m_pxGraph);
		}
	}

	const Zenith_Entity xParent = m_xParentEntity;

	++s_iGraphDispatchDepth;
	for (u_int u = 0; u < axGraphs.GetSize(); ++u)
	{
		Zenith_GraphContext xContext;
		xContext.m_xSelf = xParent;
		xContext.m_fTimeSeconds = g_xEngine.Frame().GetTimePassed();
		xContext.m_pxGraph = axGraphs.Get(u);
		xContext.m_pxBlackboard = &axGraphs.Get(u)->GetBlackboard();
		xContext.m_pxEventPayload = pxPayload;
		xContext.m_pxEventArgs = pxArgs;
		xContext.m_uEventArgCount = uArgCount;
		axGraphs.Get(u)->FireCustomEvent(szName, xContext);
	}
	--s_iGraphDispatchDepth;

	if (s_iGraphDispatchDepth == 0)
	{
		FlushPendingRemovalsViaEntity(xParent);
	}
}

void Zenith_GraphComponent::OnStart()
{
	FireEventOnSlots(GRAPH_EVENT_ON_START, 0.0f, nullptr);
}

void Zenith_GraphComponent::OnEnable()
{
	FireEventOnSlots(GRAPH_EVENT_ON_ENABLE, 0.0f, nullptr);
}

void Zenith_GraphComponent::OnDisable()
{
	FireEventOnSlots(GRAPH_EVENT_ON_DISABLE, 0.0f, nullptr);
}

void Zenith_GraphComponent::OnUpdate(float fDt)
{
	FireEventOnSlots(GRAPH_EVENT_ON_UPDATE, fDt, nullptr);
}

void Zenith_GraphComponent::OnFixedUpdate(float fDt)
{
	FireEventOnSlots(GRAPH_EVENT_ON_FIXED_UPDATE, fDt, nullptr);
}

void Zenith_GraphComponent::OnDestroy()
{
	// Reverse slot order on destruction (the Unity-convention parity the old
	// system kept).
	FireEventOnSlots(GRAPH_EVENT_ON_DESTROY, 0.0f, nullptr, true);
}

void Zenith_GraphComponent::OnCollisionEnter(Zenith_Entity xOther)
{
	Zenith_PropertyValue xPayload;
	xPayload.SetPackedEntityID(xOther.GetEntityID().GetPacked());
	FireEventOnSlots(GRAPH_EVENT_ON_COLLISION_ENTER, 0.0f, &xPayload);
}

void Zenith_GraphComponent::OnCollisionStay(Zenith_Entity xOther)
{
	Zenith_PropertyValue xPayload;
	xPayload.SetPackedEntityID(xOther.GetEntityID().GetPacked());
	FireEventOnSlots(GRAPH_EVENT_ON_COLLISION_STAY, 0.0f, &xPayload);
}

void Zenith_GraphComponent::OnCollisionExit(Zenith_EntityID xOtherID)
{
	Zenith_PropertyValue xPayload;
	xPayload.SetPackedEntityID(xOtherID.GetPacked());
	FireEventOnSlots(GRAPH_EVENT_ON_COLLISION_EXIT, 0.0f, &xPayload);
}

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

void Zenith_GraphComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uGRAPH_COMPONENT_VERSION;

	const uint64_t ulBlobSizeFieldCursor = xStream.GetCursor();
	u_int uBlobPlaceholder = 0;
	xStream << uBlobPlaceholder;
	const uint64_t ulBlobStartCursor = xStream.GetCursor();

	u_int uWriteCount = 0;
	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		if (!m_axSlots.Get(u).m_bMarkedForRemoval)
		{
			++uWriteCount;
		}
	}
	xStream << uWriteCount;

	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_GraphSlot& xSlot = m_axSlots.Get(u);
		if (xSlot.m_bMarkedForRemoval)
		{
			continue;
		}

		xStream << xSlot.m_strGraphAssetPath;

		// Per-slot framed override blob: live slots serialize their CURRENT
		// blackboard (captures per-entity tweaks); unresolved slots emit their
		// preserved bytes verbatim.
		const uint64_t ulOverrideSizeFieldCursor = xStream.GetCursor();
		u_int uOverridePlaceholder = 0;
		xStream << uOverridePlaceholder;
		const uint64_t ulOverrideStartCursor = xStream.GetCursor();

		if (xSlot.m_pxGraph)
		{
			xSlot.m_pxGraph->GetBlackboard().WriteToDataStream(xStream);
		}
		else if (xSlot.m_xPendingOverrides.GetCursor() > 0)
		{
			xStream.WriteData(xSlot.m_xPendingOverrides.GetData(), xSlot.m_xPendingOverrides.GetCursor());
		}

		const uint64_t ulOverrideEndCursor = xStream.GetCursor();
		const u_int uOverrideBytes = static_cast<u_int>(ulOverrideEndCursor - ulOverrideStartCursor);
		xStream.SetCursor(ulOverrideSizeFieldCursor);
		xStream << uOverrideBytes;
		xStream.SetCursor(ulOverrideEndCursor);
	}

	const uint64_t ulBlobEndCursor = xStream.GetCursor();
	const u_int uBlobBytes = static_cast<u_int>(ulBlobEndCursor - ulBlobStartCursor);
	xStream.SetCursor(ulBlobSizeFieldCursor);
	xStream << uBlobBytes;
	xStream.SetCursor(ulBlobEndCursor);
}

void Zenith_GraphComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0;
	xStream >> uVersion;
	u_int uBlobBytes = 0;
	xStream >> uBlobBytes;
	const uint64_t ulBlobEnd = xStream.GetCursor() + uBlobBytes;

	if (uVersion < uGRAPH_COMPONENT_MIN_VERSION || uVersion > uGRAPH_COMPONENT_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_ECS,
			"GraphComponent: unsupported serialization version %u (supported %u..%u); skipping blob",
			uVersion, uGRAPH_COMPONENT_MIN_VERSION, uGRAPH_COMPONENT_VERSION);
		xStream.SkipBytes(uBlobBytes);
		return;
	}

	u_int uCount = 0;
	xStream >> uCount;
	for (u_int u = 0; u < uCount; ++u)
	{
		Zenith_GraphSlot xSlot;
		xStream >> xSlot.m_strGraphAssetPath;
		xSlot.m_bOverridesIncludeLists = (uVersion >= 2);

		u_int uOverrideBytes = 0;
		xStream >> uOverrideBytes;
		if (uOverrideBytes > 0)
		{
			xSlot.m_xPendingOverrides.WriteData(static_cast<const uint8_t*>(xStream.GetData()) + xStream.GetCursor(), uOverrideBytes);
			xStream.SkipBytes(uOverrideBytes);
		}

		m_axSlots.PushBack(std::move(xSlot));
		InstantiateSlotGraph(m_axSlots.Get(m_axSlots.GetSize() - 1));
	}

	if (xStream.GetCursor() != ulBlobEnd)
	{
		Zenith_Log(LOG_CATEGORY_ECS, "GraphComponent: read cursor mismatch; clamping to blob end");
		xStream.SetCursor(ulBlobEnd);
	}
}

//------------------------------------------------------------------------------
// Tools: hot reload + editor panel
//------------------------------------------------------------------------------

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "Core/Zenith_DragDropPayloads.h"
#include "Core/Zenith_GraphEditorHook.h"

u_int Zenith_GraphComponent::ReloadSlotsForAsset(const char* szNormalizedPath)
{
	Zenith_Assert(!IsDispatchInProgress(), "ReloadSlotsForAsset called during graph dispatch");
	if (!szNormalizedPath)
	{
		return 0;
	}

	u_int uReloaded = 0;
	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		Zenith_GraphSlot& xSlot = m_axSlots.Get(u);
		if (xSlot.m_strGraphAssetPath != szNormalizedPath)
		{
			continue;
		}

		Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(xSlot.m_strGraphAssetPath);
		if (!pxAsset || !pxAsset->LoadedOk())
		{
			continue;	// keep the old instance - atomic failure
		}

		Zenith_BehaviourGraph* pxNewGraph = new Zenith_BehaviourGraph();
		pxNewGraph->InitialiseFromDefinition(pxAsset->GetDefinition());

		if (xSlot.m_pxGraph)
		{
			// Hot-reload migration: exact name+type matches carry over; type
			// changes drop (reported by the blackboard).
			pxNewGraph->GetBlackboard().CopyMatchingFrom(xSlot.m_pxGraph->GetBlackboard());
			delete xSlot.m_pxGraph;
		}
		else if (xSlot.m_xPendingOverrides.GetCursor() > 0)
		{
			// Unresolved slot becoming resolved: apply the preserved overrides.
			Zenith_DataStream xRead(const_cast<void*>(xSlot.m_xPendingOverrides.GetData()), xSlot.m_xPendingOverrides.GetCursor());
			Zenith_GraphBlackboard xOverrides;
			xOverrides.ReadFromDataStream(xRead, xSlot.m_bOverridesIncludeLists);
			pxNewGraph->GetBlackboard().ApplyOverridesFrom(xOverrides);
		}

		xSlot.m_pxGraph = pxNewGraph;
		++uReloaded;
	}
	return uReloaded;
}

void Zenith_GraphComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Graph Component", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	if (m_iPendingRemoveIndex >= 0 && static_cast<u_int>(m_iPendingRemoveIndex) < m_axSlots.GetSize())
	{
		RemoveGraphAt(static_cast<u_int>(m_iPendingRemoveIndex));
		m_iPendingRemoveIndex = -1;
	}

	for (u_int u = 0; u < m_axSlots.GetSize(); ++u)
	{
		const Zenith_GraphSlot& xSlot = m_axSlots.Get(u);
		ImGui::PushID(static_cast<int>(u));
		if (xSlot.m_pxGraph)
		{
			ImGui::Text("%s  (%u nodes, %u vars%s)",
				xSlot.m_strGraphAssetPath.c_str(),
				xSlot.m_pxGraph->GetNodeCount(),
				xSlot.m_pxGraph->GetBlackboard().GetCount(),
				xSlot.m_pxGraph->GetUnresolvedCount() > 0 ? ", UNRESOLVED NODES" : "");
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s  (asset missing - slot preserved)",
				xSlot.m_strGraphAssetPath.c_str());
		}
		ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);
		if (ImGui::SmallButton("Edit") && g_pfnZenithOpenGraphEditor)
		{
			g_pfnZenithOpenGraphEditor(xSlot.m_strGraphAssetPath.c_str());
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			m_iPendingRemoveIndex = static_cast<int32_t>(u);
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::InputText("##GraphAssetPath", m_acAddGraphPathBuffer, sizeof(m_acAddGraphPathBuffer));
	ImGui::SameLine();
	if (ImGui::Button("Add Graph") && m_acAddGraphPathBuffer[0] != '\0')
	{
		AddGraphByAssetPath(m_acAddGraphPathBuffer);
		m_acAddGraphPathBuffer[0] = '\0';
	}

	// Drag-drop target for .bgraph files from the content browser (the
	// established editor drag-drop shape).
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* pxPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_GRAPH_ASSET);
		if (pxPayload && pxPayload->Data && pxPayload->DataSize >= static_cast<int>(sizeof(DragDropFilePayload)))
		{
			const DragDropFilePayload* pxFilePayload = static_cast<const DragDropFilePayload*>(pxPayload->Data);
			AddGraphByAssetPath(Zenith_AssetRegistry::NormalizeAssetPath(pxFilePayload->m_szFilePath).c_str());
		}
		ImGui::EndDragDropTarget();
	}
}
#endif

#include "EntityComponent/Components/Zenith_GraphComponent.Tests.inl"
