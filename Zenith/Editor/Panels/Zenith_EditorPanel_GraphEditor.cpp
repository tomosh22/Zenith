#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_GraphReload.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_Engine.h"
#include "Collections/Zenith_HashMap.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <filesystem>
#include <cstdio>	// snprintf - the connect-refusal text

namespace
{
	struct PanelRect
	{
		float m_fMinX = 0.0f, m_fMinY = 0.0f, m_fMaxX = 0.0f, m_fMaxY = 0.0f;
		Zenith_Maths::Vector2 Centre() const { return Zenith_Maths::Vector2((m_fMinX + m_fMaxX) * 0.5f, (m_fMinY + m_fMaxY) * 0.5f); }
	};

	PanelRect MakeRect(const ImVec2& xMin, const ImVec2& xMax)
	{
		PanelRect xRect;
		xRect.m_fMinX = xMin.x; xRect.m_fMinY = xMin.y;
		xRect.m_fMaxX = xMax.x; xRect.m_fMaxY = xMax.y;
		return xRect;
	}

	// Pin key: (node << 10) | kind-bit | (pin << 1) | input-bit.
	//
	// ★ THE KIND BIT (bit 9) IS WHY THE NODE SHIFT MOVED from 9 to 10. An exec
	// pin and a data pin at the same ordinal on the same node are DIFFERENT
	// interactables with different rects; without the bit they would collide in
	// m_xPinRects and the later pass would silently overwrite the earlier one.
	// The `pin & 0xFF` mask is a hard CEILING of 255 drawn pins per side per kind
	// - past it two pins alias onto one key. Nothing in the node library is
	// within two orders of magnitude of that, and the exec-pin count is already
	// clamped to 255 by the chain-cursor key, so the mask is a documented limit
	// rather than a lurking defect.
	u_int64 MakePinKey(u_int uNodeID, u_int uPin, bool bInput, bool bData)
	{
		return (static_cast<u_int64>(uNodeID) << 10)
			| (bData ? (1ull << 9) : 0ull)
			| (static_cast<u_int64>(uPin & 0xFFu) << 1)
			| (bInput ? 1u : 0u);
	}

	// The DRAWN data-pin names of one node, in drawn order (inputs and outputs
	// listed separately, pin-table order, variadic ordinals expanded in place).
	// A data pin's KEY INDEX is its position here - so name <-> index is one
	// lookup, and nothing outside this file has to know the mapping.
	struct DrawnDataPinNames
	{
		Zenith_Vector<std::string> m_axInputs;
		Zenith_Vector<std::string> m_axOutputs;
	};

	// One node's whole drawn pin list, built ONCE per invalidation.
	//
	// ★ WHY THIS IS CACHED AT ALL. Zenith_GraphDefinitionValidator::ResolvePinType
	// is a QUERY that ALLOCATES a temp instance per instance-resolved or
	// from-variable pin (its header says so), and GetDynamicDataInputCount needs a
	// param-applied instance too. Calling either per pin per frame is an
	// allocation per pin per frame. The cache is keyed by node id and cleared
	// WHOLE - see InvalidateNodePinCache and its call sites.
	struct NodePinCacheEntry
	{
		Zenith_Vector<std::string> m_axInputNames;
		Zenith_Vector<std::string> m_axOutputNames;
		Zenith_Vector<Zenith_PropertyType> m_aeInputTypes;
		Zenith_Vector<Zenith_PropertyType> m_aeOutputTypes;
		bool m_bPure = false;
	};

	// All mutable panel state, one anonymous-namespace aggregate.
	struct GraphEditorState
	{
		bool m_bOpen = false;
		bool m_bPositionWindowNextRender = false;
		std::string m_strAssetPath;					// normalized prefixed path
		Zenith_BehaviourGraphAsset* m_pxAsset = nullptr;
		bool m_bOwnsAsset = false;					// true until the registry has a cached copy
		bool m_bDirty = false;

		u_int m_uSelectedNodeID = 0;
		Zenith_GraphNode* m_pxParamInstance = nullptr;	// temp instance backing the param panel
		u_int m_uParamInstanceNodeID = 0;

		bool m_bLinking = false;
		u_int m_uLinkSrcNodeID = 0;
		u_int m_uLinkSrcPin = 0;
		// The pending drag's source DATA pin, by NAME ("" = this is an EXEC
		// link). By name and never by index, because a variadic member's drawn
		// index moves with the member count while the wire stores the name.
		std::string m_strLinkSrcDataPin;

		char m_acNewVarName[64] = {};
		int m_iNewVarType = 0;

		// Why the last connect attempt was refused ("" = the last one landed).
		// Displayed near the toolbar: a refused drag that says nothing is
		// indistinguishable from a missed drop.
		std::string m_strConnectRefusal;
		// FULL-tier validation, refreshed on open / param edit / successful
		// connect. ADVISORY: an asset with ERROR findings still opens, edits
		// and saves - refusing would trap the author inside the mistake.
		Zenith_Vector<Zenith_GraphValidationFinding> m_axValidationFindings;

#ifdef ZENITH_TESTING
		// Pending ScrollPaletteEntryIntoView request, consumed by the next
		// RenderPalette pass that reaches the named row.
		std::string m_strScrollToPaletteEntry;
#endif

		// Live rects recorded each Render for interaction + test accessors.
		Zenith_HashMap<std::string, PanelRect> m_xPaletteRects;
		Zenith_HashMap<u_int, PanelRect> m_xNodeRects;
		Zenith_HashMap<u_int64, PanelRect> m_xPinRects;
		Zenith_HashMap<std::string, PanelRect> m_xToolbarRects;
		Zenith_HashMap<std::string, PanelRect> m_xPropertyRowRects;
		// Per-FRAME, like the rect maps: what BuildPinPositions actually drew,
		// so a name can be turned into the key index the rect was recorded under.
		Zenith_HashMap<u_int, DrawnDataPinNames> m_xDrawnDataPinNames;
		// Edges the last frame drew with the dashed-red fallback because an
		// endpoint pin could not be located. Reset with the rect maps.
		u_int m_uUnresolvableEdgeDrawCount = 0;

		// Per-INVALIDATION, not per frame (see NodePinCacheEntry).
		Zenith_HashMap<u_int, NodePinCacheEntry> m_xNodePinCache;
		u_int m_uPinTypeCacheFillCount = 0;
	};

	GraphEditorState g_xGraphEditor;

	//--------------------------------------------------------------------------
	// Helpers
	//--------------------------------------------------------------------------

	float Clampf(float fValue, float fMin, float fMax)
	{
		if (fValue < fMin) return fMin;
		if (fValue > fMax) return fMax;
		return fValue;
	}

	Zenith_GraphDefinition* GetOpenDefinition()
	{
		return g_xGraphEditor.m_pxAsset ? &g_xGraphEditor.m_pxAsset->GetDefinition() : nullptr;
	}

	// Re-runs the FULL-tier report over the open definition. ADVISORY: errors
	// are reported AS errors (and drawn in the error colour), but nothing here
	// refuses an edit or a save - a panel that locked the author out of a graph
	// with a mistake in it would leave them no way to fix the mistake. Called
	// on asset open, after a parameter edit commits, and after a connect lands.
	void ValidateOpenGraph()
	{
		g_xGraphEditor.m_axValidationFindings.Clear();
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			return;
		}
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		Zenith_GraphDefinitionValidator::Validate(*pxDef, xRegistry,
			g_xGraphEditor.m_strAssetPath.c_str(), g_xGraphEditor.m_axValidationFindings);
	}

	//--------------------------------------------------------------------------
	// The per-node pin cache (see NodePinCacheEntry) + the pin colour table.
	//--------------------------------------------------------------------------

	// Cleared WHOLE, never per node: a variable retype moves the resolved type of
	// every from-variable pin in the graph, and a param edit can move a variadic
	// member count, so "which nodes did that affect" is not a question this panel
	// can answer cheaply or safely. Call sites: OpenAsset / OpenAssetFresh /
	// Close, every COMMITTED param edit, node add + remove, variable add +
	// remove, and Save (which queues a hot reload).
	void InvalidateNodePinCache()
	{
		g_xGraphEditor.m_xNodePinCache.Clear();
	}

	// The DRAWN pin list of one node, from the cache, building it on a miss.
	// Null = there is nothing to draw (no such node, unregistered type, or no pin
	// table) - and that answer is deliberately NOT cached: a per-game node
	// library registered after this frame would otherwise stay pin-less forever.
	const NodePinCacheEntry* GetNodePinCacheEntry(const Zenith_GraphDefinition& xDef, u_int uNodeID)
	{
		if (const NodePinCacheEntry* pxCached = g_xGraphEditor.m_xNodePinCache.TryGet(uNodeID))
		{
			return pxCached;
		}

		const Zenith_GraphNodeDef* pxNodeDef = xDef.FindNodeDef(uNodeID);
		if (!pxNodeDef)
		{
			return nullptr;
		}
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(pxNodeDef->m_strTypeName.c_str());
		if (!pxInfo || !pxInfo->m_pfnGetPinTable || !pxInfo->m_pfnCreate)
		{
			return nullptr;
		}
		const Zenith_GraphPinTable* pxPins = pxInfo->m_pfnGetPinTable();
		if (!pxPins || pxPins->GetPinCount() == 0)
		{
			return nullptr;
		}

		// The VARIADIC member count comes from ONE param-applied temp instance
		// built here, inside the cache fill - never from a per-frame temp.
		int32_t iVariadicMembers = -1;
		{
			Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
			xDef.ApplyNodeParams(uNodeID, pxTemp, *pxInfo);
			iVariadicMembers = pxTemp->GetDynamicDataInputCount();
			delete pxTemp;
		}

		NodePinCacheEntry xEntry;
		xEntry.m_bPure = pxInfo->m_bPureNode;
		bool bEveryPinResolved = true;
		for (u_int uPin = 0; uPin < pxPins->GetPinCount(); ++uPin)
		{
			const Zenith_GraphPinDesc& xDesc = pxPins->GetPinAt(uPin);
			if (xDesc.m_eRole != GRAPH_PIN_ROLE_INPUT && xDesc.m_eRole != GRAPH_PIN_ROLE_OUTPUT)
			{
				continue;	// SELECTOR_*/TARGET_REF/LIST stay properties in the param panel
			}

			// ★ THE DRAWN INDEX -> TABLE INDEX MAPPING. ResolvePinType is indexed
			// by TABLE index and takes no ordinal, because every member of a
			// variadic family shares the family's one type. So a family expands to
			// N drawn pins that all resolve through the family's table index.
			Zenith_PropertyType eType = eGRAPH_PIN_TYPE_ANY;
			if (!Zenith_GraphDefinitionValidator::ResolvePinType(xDef, Zenith_GraphNodeRegistry::Get(), uNodeID, uPin, eType))
			{
				// A FAILED answer is never cached (see above); grey it and say so
				// by refusing to store the entry.
				eType = eGRAPH_PIN_TYPE_ANY;
				bEveryPinResolved = false;
			}

			if (xDesc.m_eRole == GRAPH_PIN_ROLE_OUTPUT)
			{
				xEntry.m_axOutputNames.PushBack(std::string(xDesc.m_szName ? xDesc.m_szName : ""));
				xEntry.m_aeOutputTypes.PushBack(eType);
				continue;
			}

			if (!xDesc.m_bVariadic)
			{
				xEntry.m_axInputNames.PushBack(std::string(xDesc.m_szName ? xDesc.m_szName : ""));
				xEntry.m_aeInputTypes.PushBack(eType);
				continue;
			}
			// A FAMILY has no non-ordinal member: an unconfigured family (-1) draws
			// no pins at all, which is exactly what a wire naming it would find.
			const u_int uMembers = iVariadicMembers > 0 ? static_cast<u_int>(iVariadicMembers) : 0u;
			for (u_int uOrdinal = 0; uOrdinal < uMembers; ++uOrdinal)
			{
				char acMember[80];
				snprintf(acMember, sizeof(acMember), "%s%u", xDesc.m_szName ? xDesc.m_szName : "", uOrdinal);
				xEntry.m_axInputNames.PushBack(std::string(acMember));
				xEntry.m_aeInputTypes.PushBack(eType);
			}
		}

		if (!bEveryPinResolved)
		{
			// Answered for THIS frame, remembered for none of them.
			static NodePinCacheEntry ls_xUncacheable;
			ls_xUncacheable = xEntry;
			return &ls_xUncacheable;
		}
		g_xGraphEditor.m_xNodePinCache[uNodeID] = xEntry;
		++g_xGraphEditor.m_uPinTypeCacheFillCount;
		return g_xGraphEditor.m_xNodePinCache.TryGet(uNodeID);
	}

	// Pin colour BY RESOLVED TYPE - the whole affordance for "will this wire
	// carry what the consumer expects", since a bare circle says nothing.
	// eGRAPH_PIN_TYPE_ANY (and an unresolvable pin, explicitly) is GREY: the
	// panel never fabricates a type it could not resolve.
	//
	// Every pair differs by at least 24 on at least one channel - a palette whose
	// members are merely "not equal" is one a human cannot read apart.
	ImU32 PinTypeColour(Zenith_PropertyType eType)
	{
		switch (eType)
		{
		case PROPERTY_TYPE_FLOAT:		return IM_COL32( 80, 210, 100, 255);	// green
		case PROPERTY_TYPE_INT32:		return IM_COL32( 70, 200, 220, 255);	// cyan
		case PROPERTY_TYPE_UINT32:		return IM_COL32( 40, 150, 140, 255);	// teal
		case PROPERTY_TYPE_BOOL:		return IM_COL32(220,  80,  80, 255);	// red
		case PROPERTY_TYPE_VECTOR2:		return IM_COL32(230, 230, 110, 255);	// yellow
		case PROPERTY_TYPE_VECTOR3:		return IM_COL32(200, 180,  60, 255);	// yellow, darker
		case PROPERTY_TYPE_VECTOR4:		return IM_COL32(160, 140,  20, 255);	// yellow, darkest
		case PROPERTY_TYPE_STRING:		return IM_COL32(220,  90, 210, 255);	// magenta
		case PROPERTY_TYPE_ENTITY_ID:	return IM_COL32( 80, 120, 230, 255);	// blue
		case PROPERTY_TYPE_GUID:		return IM_COL32(240, 150,  50, 255);	// orange
		default:						return IM_COL32(150, 150, 150, 255);	// ANY / unresolved
		}
	}

	// How many of the last run's findings are ERRORs. The toolbar splits the
	// count on this, and the findings block colours on it.
	u_int CountValidationFindingsOfSeverity(Zenith_GraphValidationSeverity eSeverity)
	{
		u_int uCount = 0;
		for (u_int u = 0; u < g_xGraphEditor.m_axValidationFindings.GetSize(); ++u)
		{
			if (g_xGraphEditor.m_axValidationFindings.Get(u).m_eSeverity == eSeverity)
			{
				++uCount;
			}
		}
		return uCount;
	}

	void DestroyParamInstance()
	{
		delete g_xGraphEditor.m_pxParamInstance;
		g_xGraphEditor.m_pxParamInstance = nullptr;
		g_xGraphEditor.m_uParamInstanceNodeID = 0;
	}

	// (Re)builds the temp instance the parameter panel edits, from the selected
	// node's param blob.
	void RefreshParamInstanceForSelection()
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef || g_xGraphEditor.m_uSelectedNodeID == 0)
		{
			DestroyParamInstance();
			return;
		}
		if (g_xGraphEditor.m_pxParamInstance && g_xGraphEditor.m_uParamInstanceNodeID == g_xGraphEditor.m_uSelectedNodeID)
		{
			return;
		}
		DestroyParamInstance();

		const Zenith_GraphNodeDef* pxNodeDef = pxDef->FindNodeDef(g_xGraphEditor.m_uSelectedNodeID);
		if (!pxNodeDef)
		{
			return;
		}
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(pxNodeDef->m_strTypeName.c_str());
		if (!pxInfo || !pxInfo->m_pfnGetPropertyTable)
		{
			return;	// unresolved or parameterless
		}
		g_xGraphEditor.m_pxParamInstance = pxInfo->m_pfnCreate();
		pxDef->ApplyNodeParams(g_xGraphEditor.m_uSelectedNodeID, g_xGraphEditor.m_pxParamInstance, *pxInfo);
		g_xGraphEditor.m_uParamInstanceNodeID = g_xGraphEditor.m_uSelectedNodeID;
	}

	void OnSelectedNodeParamChanged(void* /*pxUserData*/, const char* /*szPropertyName*/)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (pxDef && g_xGraphEditor.m_pxParamInstance && g_xGraphEditor.m_uParamInstanceNodeID != 0)
		{
			pxDef->SetNodeParamsFromInstance(g_xGraphEditor.m_uParamInstanceNodeID, g_xGraphEditor.m_pxParamInstance);
			g_xGraphEditor.m_bDirty = true;
			// A committed param edit can move an instance-resolved pin's TYPE, a
			// from-variable pin's TYPE (the property naming the variable IS a
			// param) and a variadic family's MEMBER COUNT - so the drawn pin list
			// itself is stale, not just its colours.
			InvalidateNodePinCache();
			// A var-name edit is exactly the edit that breaks a binding, so the
			// report is refreshed HERE, after the commit - before it the blob
			// still holds the old value.
			ValidateOpenGraph();
		}
	}

	void OnPropertyRowRect(void* /*pxUserData*/, const char* szPropertyName, float fMinX, float fMinY, float fMaxX, float fMaxY)
	{
		PanelRect xRect;
		xRect.m_fMinX = fMinX; xRect.m_fMinY = fMinY; xRect.m_fMaxX = fMaxX; xRect.m_fMaxY = fMaxY;
		g_xGraphEditor.m_xPropertyRowRects[std::string(szPropertyName)] = xRect;
	}

	bool AddNodeAtFreeSpot(const char* szTypeName)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			return false;
		}
		const u_int uNodeID = pxDef->AddNode(szTypeName);
		if (uNodeID == 0)
		{
			return false;
		}
		const u_int uIndex = pxDef->GetNodeCount() - 1;
		pxDef->SetNodeEditorPos(uNodeID, Zenith_Maths::Vector2(
			30.0f + static_cast<float>(uIndex % 3) * 220.0f,
			30.0f + static_cast<float>(uIndex / 3) * 150.0f));
		g_xGraphEditor.m_uSelectedNodeID = uNodeID;
		g_xGraphEditor.m_bDirty = true;
		InvalidateNodePinCache();
		return true;
	}

	// Node addressing for the atomic editor actions: type name + occurrence in
	// creation order - the way a human picks a node out of the canvas visually.
	u_int ResolveNodeByTypeOccurrence(const char* szTypeName, u_int uOccurrence)
	{
		const Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef || !szTypeName)
		{
			return 0;
		}
		u_int uSeen = 0;
		for (u_int u = 0; u < pxDef->GetNodeCount(); ++u)
		{
			if (pxDef->GetNodeAt(u).m_strTypeName == szTypeName)
			{
				if (uSeen == uOccurrence)
				{
					return pxDef->GetNodeAt(u).m_uNodeID;
				}
				++uSeen;
			}
		}
		return 0;
	}

	// Looks up the live graph instance on the selected entity matching the open
	// asset (for execution highlighting while playing).
	const Zenith_BehaviourGraph* FindLiveGraphForHighlight()
	{
		if (g_xEngine.Editor().GetEditorMode() != EditorMode::Playing)
		{
			return nullptr;
		}
		Zenith_Entity* pxSelected = g_xEngine.Editor().GetSelectedEntity();
		if (!pxSelected || !pxSelected->IsValid())
		{
			return nullptr;
		}
		Zenith_GraphComponent* pxComponent = pxSelected->TryGetComponent<Zenith_GraphComponent>();
		if (pxComponent == nullptr)
		{
			return nullptr;
		}
		Zenith_GraphComponent& xComponent = *pxComponent;
		for (u_int u = 0; u < xComponent.GetGraphCount(); ++u)
		{
			if (g_xGraphEditor.m_strAssetPath == xComponent.GetGraphAssetPathAt(u) && xComponent.GetGraphAt(u))
			{
				return xComponent.GetGraphAt(u);
			}
		}
		return nullptr;
	}

	//--------------------------------------------------------------------------
	// Sections
	//--------------------------------------------------------------------------

	void RenderToolbarRow()
	{
		const char* aszButtons[] = { "Save" };
		for (u_int u = 0; u < 1; ++u)
		{
			if (u > 0)
			{
				ImGui::SameLine();
			}
			const bool bClicked = ImGui::Button(aszButtons[u]);
			g_xGraphEditor.m_xToolbarRects[std::string(aszButtons[u])] = MakeRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
			if (bClicked && std::strcmp(aszButtons[u], "Save") == 0)
			{
				Zenith_GraphEditorPanel::Save();
			}
		}

		ImGui::SameLine();
		ImGui::Text("%s%s", g_xGraphEditor.m_strAssetPath.c_str(), g_xGraphEditor.m_bDirty ? " *" : "");

		// Last hot-reload status (the designer-facing error console line).
		const char* szLastReload = Zenith_GraphReload::GetLastStatusLine();
		if (szLastReload && szLastReload[0] != '\0')
		{
			ImGui::SameLine();
			ImGui::TextDisabled("| %s", szLastReload);
		}

		// A refused connection, said out loud. Precedent: the terrain editor's
		// status line (Zenith_EditorPanel_TerrainEditor.cpp).
		if (!g_xGraphEditor.m_strConnectRefusal.empty())
		{
			ImGui::TextWrapped("%s", g_xGraphEditor.m_strConnectRefusal.c_str());
		}

		// The validation summary + the first few findings. ERRORs are counted
		// separately and drawn in a distinct colour; nothing here blocks an
		// edit or a save (see ValidateOpenGraph).
		const u_int uFindings = g_xGraphEditor.m_axValidationFindings.GetSize();
		if (uFindings > 0)
		{
			constexpr u_int uMAX_DISPLAYED_FINDINGS = 5;
			// Red-ish for a defect, amber for advice - the two must not read the
			// same at a glance, which is the whole point of latching errors.
			const ImVec4 xERROR_COLOUR(0.95f, 0.32f, 0.28f, 1.0f);
			const ImVec4 xWARNING_COLOUR(0.90f, 0.75f, 0.30f, 1.0f);

			const u_int uErrors = CountValidationFindingsOfSeverity(GRAPH_VALIDATION_SEVERITY_ERROR);
			const u_int uWarnings = uFindings - uErrors;
			ImGui::PushStyleColor(ImGuiCol_Text, uErrors > 0 ? xERROR_COLOUR : xWARNING_COLOUR);
			ImGui::TextWrapped("Validation: %u errors / %u warnings", uErrors, uWarnings);
			ImGui::PopStyleColor();

			for (u_int u = 0; u < uFindings && u < uMAX_DISPLAYED_FINDINGS; ++u)
			{
				const Zenith_GraphValidationFinding& xFinding = g_xGraphEditor.m_axValidationFindings.Get(u);
				const bool bError = xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR;
				ImGui::PushStyleColor(ImGuiCol_Text, bError ? xERROR_COLOUR : xWARNING_COLOUR);
				ImGui::TextWrapped("  [%s] [%s] node %u %s: %s",
					Zenith_GraphDefinitionValidator::GetSeverityName(xFinding.m_eSeverity),
					Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
					xFinding.m_uNodeID,
					xFinding.m_strTypeName.empty() ? "-" : xFinding.m_strTypeName.c_str(),
					xFinding.m_strWhat.c_str());
				ImGui::PopStyleColor();
			}
			if (uFindings > uMAX_DISPLAYED_FINDINGS)
			{
				ImGui::TextDisabled("  ... %u more", uFindings - uMAX_DISPLAYED_FINDINGS);
			}
		}
	}

	void RenderPalette()
	{
		ImGui::TextUnformatted("Palette");
		ImGui::Separator();

		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();

		// Group by category: collect distinct categories first (small N).
		Zenith_Vector<std::string> axCategories;
		for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
		{
			const std::string& strCategory = xRegistry.GetTypeAt(u).m_strCategory;
			bool bKnown = false;
			for (u_int uCat = 0; uCat < axCategories.GetSize(); ++uCat)
			{
				if (axCategories.Get(uCat) == strCategory)
				{
					bKnown = true;
					break;
				}
			}
			if (!bKnown)
			{
				axCategories.PushBack(strCategory);
			}
		}
		std::sort(axCategories.begin(), axCategories.end());

		for (u_int uCat = 0; uCat < axCategories.GetSize(); ++uCat)
		{
			const std::string& strCategory = axCategories.Get(uCat);
			if (!ImGui::TreeNodeEx(strCategory.empty() ? "(misc)" : strCategory.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				continue;
			}
			for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
			{
				const Zenith_GraphNodeTypeInfo& xInfo = xRegistry.GetTypeAt(u);
				if (xInfo.m_strCategory != strCategory)
				{
					continue;
				}
				const bool bClicked = ImGui::Selectable(xInfo.m_strTypeName.c_str());
#ifdef ZENITH_TESTING
				// Honour a pending scroll-into-view request for this row.
				if (g_xGraphEditor.m_strScrollToPaletteEntry == xInfo.m_strTypeName)
				{
					ImGui::SetScrollHereY(0.5f);
					g_xGraphEditor.m_strScrollToPaletteEntry.clear();
				}
#endif
				// Recorded ONLY while unclipped. The palette lists EVERY registered
				// node type, so the left column is far taller than the window (and
				// than the display) and most rows are scrolled out of view at any
				// moment -- and a clipped ImGui item is not interactable. Recording
				// those too would hand a caller screen coordinates that a click can
				// never land on, which is precisely the silent failure this guard
				// exists to prevent: Test_GraphEditorLiveAuthoring was clicking
				// y=1768 on a 720-tall display and reporting only "the nodes were
				// not created".
				if (ImGui::IsItemVisible())
				{
					g_xGraphEditor.m_xPaletteRects[xInfo.m_strTypeName] = MakeRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
				}
				if (bClicked)
				{
					AddNodeAtFreeSpot(xInfo.m_strTypeName.c_str());
				}
			}
			ImGui::TreePop();
		}
	}

	void RenderVariableDefaultWidget(Zenith_GraphVariableDecl& xDecl)
	{
		Zenith_PropertyValue& xValue = xDecl.m_xDefault;
		switch (xValue.GetType())
		{
		case PROPERTY_TYPE_FLOAT:
		{
			float fValue = xValue.GetFloat();
			if (ImGui::DragFloat("##default", &fValue, 0.01f))
			{
				xValue.SetFloat(fValue);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_INT32:
		{
			int32_t iValue = xValue.GetInt32();
			if (ImGui::DragInt("##default", &iValue))
			{
				xValue.SetInt32(iValue);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_BOOL:
		{
			bool bValue = xValue.GetBool();
			if (ImGui::Checkbox("##default", &bValue))
			{
				xValue.SetBool(bValue);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_STRING:
		{
			char acBuffer[128];
			const std::string& strValue = xValue.GetString();
			const size_t uLen = std::min(strValue.length(), sizeof(acBuffer) - 1);
			std::memcpy(acBuffer, strValue.c_str(), uLen);
			acBuffer[uLen] = '\0';
			if (ImGui::InputText("##default", acBuffer, sizeof(acBuffer)))
			{
				xValue.SetString(std::string(acBuffer));
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_VECTOR3:
		{
			Zenith_Maths::Vector3 xVec = xValue.GetVector3();
			if (ImGui::DragFloat3("##default", &xVec.x, 0.01f))
			{
				xValue.SetVector3(xVec);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_VECTOR2:
		{
			Zenith_Maths::Vector2 xVec = xValue.GetVector2();
			if (ImGui::DragFloat2("##default", &xVec.x, 0.01f))
			{
				xValue.SetVector2(xVec);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_VECTOR4:
		{
			Zenith_Maths::Vector4 xVec = xValue.GetVector4();
			if (ImGui::DragFloat4("##default", &xVec.x, 0.01f))
			{
				xValue.SetVector4(xVec);
				g_xGraphEditor.m_bDirty = true;
			}
			break;
		}
		case PROPERTY_TYPE_ENTITY_ID:
			// Entity references are runtime-only wiring - no meaningful editable
			// default (0 = invalid until a node/override stores a live ID).
			ImGui::TextDisabled("(entity)");
			break;
		default:
			ImGui::TextDisabled("(type %u)", xValue.GetType());
			break;
		}
	}

	void RenderVariables()
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			return;
		}

		ImGui::TextUnformatted("Variables");
		ImGui::Separator();

		std::string strRemove;
		for (u_int u = 0; u < pxDef->GetVariableCount(); ++u)
		{
			// Mutable access for default editing.
			Zenith_GraphVariableDecl* pxDecl = pxDef->FindVariableMutable(pxDef->GetVariableAt(u).m_strName.c_str());
			if (!pxDecl)
			{
				continue;
			}
			ImGui::PushID(pxDecl->m_strName.c_str());
			ImGui::Text("%s", pxDecl->m_strName.c_str());
			ImGui::SameLine(120.0f);
			ImGui::SetNextItemWidth(110.0f);
			RenderVariableDefaultWidget(*pxDecl);
			ImGui::SameLine();
			if (ImGui::SmallButton("X"))
			{
				strRemove = pxDecl->m_strName;
			}
			ImGui::PopID();
		}
		if (!strRemove.empty())
		{
			pxDef->RemoveVariable(strRemove.c_str());
			g_xGraphEditor.m_bDirty = true;
			// A from-variable pin takes the DECLARED type of the variable it
			// names, so removing a declaration retypes pins on other nodes.
			InvalidateNodePinCache();
		}

		// Add row.
		ImGui::SetNextItemWidth(110.0f);
		ImGui::InputText("##newvarname", g_xGraphEditor.m_acNewVarName, sizeof(g_xGraphEditor.m_acNewVarName));
		ImGui::SameLine();
		const char* aszTypes[] = { "float", "int", "bool", "string", "vector3", "vector2", "vector4", "entity" };
		ImGui::SetNextItemWidth(70.0f);
		ImGui::Combo("##newvartype", &g_xGraphEditor.m_iNewVarType, aszTypes, 8);
		ImGui::SameLine();
		if (ImGui::SmallButton("Add Var") && g_xGraphEditor.m_acNewVarName[0] != '\0')
		{
			Zenith_PropertyValue xDefault;
			switch (g_xGraphEditor.m_iNewVarType)
			{
			case 0: xDefault.SetFloat(0.0f); break;
			case 1: xDefault.SetInt32(0); break;
			case 2: xDefault.SetBool(false); break;
			case 3: xDefault.SetString(std::string()); break;
			case 4: xDefault.SetVector3(Zenith_Maths::Vector3(0.0f)); break;
			case 5: xDefault.SetVector2(Zenith_Maths::Vector2(0.0f)); break;
			case 6: xDefault.SetVector4(Zenith_Maths::Vector4(0.0f)); break;
			case 7: xDefault.SetPackedEntityID(0); break;
			default: break;
			}
			pxDef->DeclareVariable(g_xGraphEditor.m_acNewVarName, xDefault);
			g_xGraphEditor.m_acNewVarName[0] = '\0';
			g_xGraphEditor.m_bDirty = true;
			InvalidateNodePinCache();	// see RemoveVariable above
		}
	}

	void RenderSelectedNodeProperties()
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef || g_xGraphEditor.m_uSelectedNodeID == 0)
		{
			return;
		}
		const Zenith_GraphNodeDef* pxNodeDef = pxDef->FindNodeDef(g_xGraphEditor.m_uSelectedNodeID);
		if (!pxNodeDef)
		{
			return;
		}

		ImGui::TextUnformatted("Node Properties");
		ImGui::Separator();
		ImGui::Text("%s (node %u)", pxNodeDef->m_strTypeName.c_str(), pxNodeDef->m_uNodeID);

		RefreshParamInstanceForSelection();
		if (!g_xGraphEditor.m_pxParamInstance)
		{
			ImGui::TextDisabled("(no parameters)");
			return;
		}
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(pxNodeDef->m_strTypeName.c_str());
		if (!pxInfo || !pxInfo->m_pfnGetPropertyTable)
		{
			return;
		}
		// Fixed widget width so the recorded row rects map deterministically onto
		// the slider FRAME (the item rect otherwise includes the trailing label,
		// which automated slider clicks must not hit).
		ImGui::PushItemWidth(160.0f);
		Zenith_PropertySystem::RenderPropertyPanel(g_xGraphEditor.m_pxParamInstance, *pxInfo->m_pfnGetPropertyTable(),
			&OnSelectedNodeParamChanged, nullptr, &OnPropertyRowRect, nullptr);
		ImGui::PopItemWidth();
	}

	// Canvas layout constants shared by the canvas passes below.
	constexpr float fNODE_WIDTH = 180.0f;
	constexpr float fHEADER_HEIGHT = 24.0f;
	constexpr float fPIN_SPACING = 18.0f;
	constexpr float fPIN_RADIUS = 5.0f;

	// Sentinel for "this node has no failure pin" in the pin-drawing pass.
	constexpr u_int uNO_FAILURE_PIN = 0xFFFFFFFFu;

	// The index of a node type's routable "On Failure" exec pin, or
	// uNO_FAILURE_PIN. One past the last normal output, and only ever on a
	// static-pin non-flow type (Zenith_GraphNodeRegistry::Register refuses the
	// flag on anything else), which is why this needs no param-applied instance.
	u_int GetFailurePinIndex(const Zenith_GraphNodeTypeInfo* pxInfo)
	{
		return (pxInfo && pxInfo->m_bHasFailurePin) ? pxInfo->m_uExecOutputCount : uNO_FAILURE_PIN;
	}

	// Effective exec-pin count for a node.
	//
	// ★ THE ARITHMETIC MOVED. It now lives in
	// Zenith_GraphNodeRegistry::GetExecOutputCount (Scripting), because the
	// definition VALIDATOR needs the same answer and a second copy of it is how
	// "what is drawn" and "what is accepted" drift apart. This is the panel's
	// one-line adapter, and it is still the ONE thing the panel asks -
	// BuildPinPositions (drawing + hit rects), RenderCanvasNode (box height) and
	// TryConnect (connect validation) all come through here.
	u_int GetNodeExecOutputCount(const Zenith_GraphDefinition& xDef, u_int uNodeID)
	{
		return Zenith_GraphNodeRegistry::Get().GetExecOutputCount(xDef, uNodeID);
	}

	// THE connect funnel: every gesture that creates an exec edge - the canvas
	// drag-drop and the atomic Action_Connect - goes through this one
	// ImGui-free body, so the pin-range check cannot be present in one and
	// absent in the other (it was: the drop handler called AddEdge inline with
	// no else branch at all).
	//
	// Sets the refusal text on failure and CLEARS it on success. That string is
	// the whole visible affordance: a refused drag used to change nothing and
	// say nothing.
	bool TryConnect(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			g_xGraphEditor.m_strConnectRefusal = "Connect refused: no graph is open.";
			return false;
		}

		char acRefusal[256];
		if (uSrcNodeID == 0 || uDstNodeID == 0
			|| pxDef->FindNodeDef(uSrcNodeID) == nullptr || pxDef->FindNodeDef(uDstNodeID) == nullptr)
		{
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: node %u -> node %u names a node that is not in this graph.", uSrcNodeID, uDstNodeID);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}

		// ★ EXEC_INTO_PURE MUST NOT BE AUTHORABLE FROM THE PANEL. B-3 makes an
		// exec edge into a pure node an author-time ERROR and the runtime DROPS it
		// at instantiation - so the chain would simply end there, silently, and the
		// wire the author drew would be a lie. A pure node has no exec input to
		// draw either (see BuildPinPositions), which means this can only be reached
		// through Action_Connect - and that is exactly why it is checked in the
		// FUNNEL rather than in the drop handler.
		{
			const Zenith_GraphNodeDef* pxDstDef = pxDef->FindNodeDef(uDstNodeID);
			const Zenith_GraphNodeTypeInfo* pxDstInfo = pxDstDef
				? Zenith_GraphNodeRegistry::Get().Find(pxDstDef->m_strTypeName.c_str()) : nullptr;
			if (pxDstInfo && pxDstInfo->m_bPureNode)
			{
				snprintf(acRefusal, sizeof(acRefusal),
					"Connect refused: node %u is a PURE node - it has no exec input and evaluates on demand when a wire pulls it.",
					uDstNodeID);
				g_xGraphEditor.m_strConnectRefusal = acRefusal;
				return false;
			}
		}

		const u_int uSrcOutputs = GetNodeExecOutputCount(*pxDef, uSrcNodeID);
		if (uSrcPin >= uSrcOutputs)
		{
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: node %u has %u exec output pin(s); pin %u does not exist.",
				uSrcNodeID, uSrcOutputs, uSrcPin);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}

		if (!pxDef->AddEdge(uSrcNodeID, uSrcPin, uDstNodeID))
		{
			// AddEdge logs its own reason; this is the on-screen half.
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: (node %u, pin %u) already has an outgoing edge, or the edge is a self-loop - exec chains are linear.",
				uSrcNodeID, uSrcPin);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}

		g_xGraphEditor.m_strConnectRefusal.clear();
		g_xGraphEditor.m_bDirty = true;
		ValidateOpenGraph();
		return true;
	}

	//--------------------------------------------------------------------------
	// The DATA connect + disconnect funnels.
	//--------------------------------------------------------------------------

	// One ERROR finding's identity, for the before/after set difference below.
	// The RULE, the node, the pin and the TEXT: two findings of the same rule on
	// the same pin can still describe different mistakes.
	std::string MakeErrorFindingKey(const Zenith_GraphValidationFinding& xFinding)
	{
		char acKey[64];
		snprintf(acKey, sizeof(acKey), "%u|%u|",
			static_cast<u_int>(xFinding.m_eRule), xFinding.m_uNodeID);
		return std::string(acKey) + xFinding.m_strPin + "|" + xFinding.m_strWhat;
	}

	void SnapshotValidationErrorKeys(Zenith_Vector<std::string>& axOut)
	{
		axOut.Clear();
		for (u_int u = 0; u < g_xGraphEditor.m_axValidationFindings.GetSize(); ++u)
		{
			const Zenith_GraphValidationFinding& xFinding = g_xGraphEditor.m_axValidationFindings.Get(u);
			if (xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR)
			{
				axOut.PushBack(MakeErrorFindingKey(xFinding));
			}
		}
	}

	// THE data connect funnel: the canvas drop AND Action_ConnectData run this one
	// ImGui-free body, so "what a drag can author" and "what the atomic verb can
	// author" cannot diverge (the exec pair did, and only one of them checked its
	// pin range).
	//
	// ★ THE REFUSAL IS AN ERROR-SET DIFFERENCE, NEVER AN ATTRIBUTION MATCH.
	// A finding is not reliably attributed to the node whose pin an author just
	// dropped on: an opaque endpoint is blamed on the OPAQUE node, a DATA_CYCLE on
	// the DFS entry node, a role error on whichever side is wrong. Matching on
	// "an error naming this dst node and this pin" would therefore let a cycle
	// through, and would also revert a good wire whenever the destination already
	// carried an unrelated error. So: snapshot the ERROR set, add the wire,
	// re-validate, and revert if and only if an error appeared that was not there
	// before - whatever node it names. Warnings (DOMINANCE, PURE_UNCONSUMED) never
	// revert anything: they are advice about ORDER, not about this wire's
	// legality.
	bool TryConnectData(u_int uSrcNodeID, const char* szSrcPin, u_int uDstNodeID, const char* szDstPin)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			g_xGraphEditor.m_strConnectRefusal = "Connect refused: no graph is open.";
			return false;
		}

		char acRefusal[512];
		if (szSrcPin == nullptr || szSrcPin[0] == '\0' || szDstPin == nullptr || szDstPin[0] == '\0')
		{
			g_xGraphEditor.m_strConnectRefusal = "Connect refused: a data wire needs a source pin name and a destination pin name.";
			return false;
		}
		if (uSrcNodeID == 0 || uDstNodeID == 0
			|| pxDef->FindNodeDef(uSrcNodeID) == nullptr || pxDef->FindNodeDef(uDstNodeID) == nullptr)
		{
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: data wire node %u -> node %u names a node that is not in this graph.", uSrcNodeID, uDstNodeID);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}

		// The BEFORE set, measured on the definition as it stands right now - not
		// on whatever the last displayed report happened to describe (adding a node
		// is not a re-validation trigger, so that report can be several edits old).
		ValidateOpenGraph();
		Zenith_Vector<std::string> axErrorsBefore;
		SnapshotValidationErrorKeys(axErrorsBefore);

		// (a) the structural invariants, which the definition owns: an empty name,
		//     a self-loop, an unknown endpoint, a SECOND wire into one input.
		//     AddDataEdge logs its own reason; this is the on-screen half.
		if (!pxDef->AddDataEdge(uSrcNodeID, szSrcPin, uDstNodeID, szDstPin))
		{
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: (node %u, pin '%s') already has an incoming wire, or the wire is a self-loop - an input takes ONE wire.",
				uDstNodeID, szDstPin);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}

		// (b) the FULL tier, which owns everything about PINS: an unknown name, a
		//     wrong role, disagreeing types, a closed data cycle.
		ValidateOpenGraph();
		for (u_int u = 0; u < g_xGraphEditor.m_axValidationFindings.GetSize(); ++u)
		{
			const Zenith_GraphValidationFinding& xFinding = g_xGraphEditor.m_axValidationFindings.Get(u);
			if (xFinding.m_eSeverity != GRAPH_VALIDATION_SEVERITY_ERROR)
			{
				continue;
			}
			const std::string strKey = MakeErrorFindingKey(xFinding);
			bool bPreexisting = false;
			for (u_int uBefore = 0; uBefore < axErrorsBefore.GetSize(); ++uBefore)
			{
				if (axErrorsBefore.Get(uBefore) == strKey)
				{
					// CONSUMED, so a SECOND identical error is still a new one.
					axErrorsBefore.Get(uBefore).clear();
					bPreexisting = true;
					break;
				}
			}
			if (bPreexisting)
			{
				continue;
			}
			// A NEW error: this wire caused it. Revert, then re-validate AGAIN so
			// the displayed report describes the definition the author is actually
			// looking at rather than the one that existed for two statements.
			pxDef->RemoveDataEdge(uDstNodeID, szDstPin);
			snprintf(acRefusal, sizeof(acRefusal), "Connect refused [%s]: %s",
				Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule), xFinding.m_strWhat.c_str());
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			ValidateOpenGraph();
			return false;
		}

		g_xGraphEditor.m_strConnectRefusal.clear();
		g_xGraphEditor.m_bDirty = true;
		return true;
	}

	// THE data disconnect funnel: the right-click handler AND
	// Action_DisconnectData run this one ImGui-free body.
	//
	// ★ THE GESTURE IS ASYMMETRIC WITH EXEC, BY THE DATA MODEL. An exec edge is
	// keyed by its SOURCE (node, pin) - one outgoing edge per pin - so it is
	// removed by right-clicking the OUTPUT. A data edge is keyed by its
	// DESTINATION (node, pin name) - one incoming wire per input, unbounded
	// fan-out - so it is removed by right-clicking the INPUT. Right-clicking a
	// data OUTPUT is deliberately inert: "which of my N wires did you mean" has no
	// answer.
	bool TryDisconnectData(u_int uDstNodeID, const char* szDstPin)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef || !pxDef->RemoveDataEdge(uDstNodeID, szDstPin))
		{
			return false;
		}
		g_xGraphEditor.m_bDirty = true;
		ValidateOpenGraph();
		return true;
	}

	//--------------------------------------------------------------------------
	// The PENDING LINK funnels. A drag is started in one place and completed in
	// one of two, and the CROSS-KIND refusals live here rather than in the ImGui
	// handlers - a refusal only a mouse can reach is a refusal no headless unit
	// can prove.
	//--------------------------------------------------------------------------

	// szSrcDataPin null/"" = an EXEC link.
	void BeginPendingLink(u_int uSrcNodeID, u_int uSrcPin, const char* szSrcDataPin)
	{
		g_xGraphEditor.m_bLinking = true;
		g_xGraphEditor.m_uLinkSrcNodeID = uSrcNodeID;
		g_xGraphEditor.m_uLinkSrcPin = uSrcPin;
		g_xGraphEditor.m_strConnectRefusal.clear();
		g_xGraphEditor.m_strLinkSrcDataPin = szSrcDataPin ? szSrcDataPin : "";
	}

	bool IsPendingLinkData()
	{
		return !g_xGraphEditor.m_strLinkSrcDataPin.empty();
	}

	// Dropped on an EXEC input pin. Always ends the drag - a drop that changed
	// nothing and left the rubber band attached would look like a hung UI.
	bool CompletePendingLinkOnExecInput(u_int uDstNodeID)
	{
		const bool bWasData = IsPendingLinkData();
		const u_int uSrcNodeID = g_xGraphEditor.m_uLinkSrcNodeID;
		const u_int uSrcPin = g_xGraphEditor.m_uLinkSrcPin;
		const std::string strSrcDataPin = g_xGraphEditor.m_strLinkSrcDataPin;
		g_xGraphEditor.m_bLinking = false;
		g_xGraphEditor.m_strLinkSrcDataPin.clear();
		if (bWasData)
		{
			char acRefusal[320];
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: '%s' is a DATA pin and node %u's input is an EXEC pin - a value wire and a control wire are not interchangeable.",
				strSrcDataPin.c_str(), uDstNodeID);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}
		return TryConnect(uSrcNodeID, uSrcPin, uDstNodeID);
	}

	// Dropped on an INPUT DATA pin.
	bool CompletePendingLinkOnDataInput(u_int uDstNodeID, const char* szDstPin)
	{
		const bool bWasData = IsPendingLinkData();
		const u_int uSrcNodeID = g_xGraphEditor.m_uLinkSrcNodeID;
		const std::string strSrcDataPin = g_xGraphEditor.m_strLinkSrcDataPin;
		g_xGraphEditor.m_bLinking = false;
		g_xGraphEditor.m_strLinkSrcDataPin.clear();
		if (!bWasData)
		{
			char acRefusal[320];
			snprintf(acRefusal, sizeof(acRefusal),
				"Connect refused: an EXEC output cannot feed the DATA input '%s' on node %u - a control wire carries no value.",
				szDstPin ? szDstPin : "", uDstNodeID);
			g_xGraphEditor.m_strConnectRefusal = acRefusal;
			return false;
		}
		return TryConnectData(uSrcNodeID, strSrcDataPin.c_str(), uDstNodeID, szDstPin);
	}

	struct PinPos
	{
		ImVec2 m_xInput;
		// False for a PURE node: it has no exec input at all, so none is drawn,
		// none is keyed, and an exec edge aimed at it draws as the dashed-red
		// fallback instead of landing on a circle that is not there.
		bool m_bHasExecInput = true;
		Zenith_Vector<ImVec2> m_axOutputs;
		// The DATA pins, in drawn order. Names and resolved TYPES are copied out
		// of the per-node cache so no pass holds a pointer into it.
		Zenith_Vector<ImVec2> m_axDataInputs;
		Zenith_Vector<ImVec2> m_axDataOutputs;
		Zenith_Vector<std::string> m_axDataInputNames;
		Zenith_Vector<std::string> m_axDataOutputNames;
		Zenith_Vector<Zenith_PropertyType> m_aeDataInputTypes;
		Zenith_Vector<Zenith_PropertyType> m_aeDataOutputTypes;
		// Where an UNRESOLVABLE edge is drawn from/to: the node header's centre,
		// which exists for every node whether or not its pins resolve.
		ImVec2 m_xHeaderCentre;
		// The box height, computed HERE from the same row counts the pin rows are
		// laid out on - so a node can never be shorter than its own pins.
		float m_fNodeHeight = 0.0f;
	};

	// Row r's pin-centre Y inside a node whose box starts at fNodeMinY. Row 0 is
	// the exec input (left) / the first exec output (right); everything else steps
	// down by one spacing.
	float PinRowY(float fNodeMinY, u_int uRow)
	{
		return fNodeMinY + fHEADER_HEIGHT + 10.0f + static_cast<float>(uRow) * fPIN_SPACING;
	}

	// ★ THE ZERO-ROW GUARD IS LOAD-BEARING and was before B-4 too: a PURE type has
	// ZERO exec outputs, and a pure node with no data pins at all would otherwise
	// underflow the (rows - 1) term. A node with no pin table has exactly the rows
	// it had before data pins existed - 1 left (its exec input) and E right - so
	// its box height is unchanged to the float.
	float NodeBoxHeight(u_int uLeftRows, u_int uRightRows)
	{
		const u_int uRows = uLeftRows > uRightRows ? uLeftRows : uRightRows;
		return fHEADER_HEIGHT + 14.0f + static_cast<float>(uRows > 0 ? uRows - 1 : 0) * fPIN_SPACING + 10.0f;
	}

	// Pin positions derived from node defs - the single source the edge pass,
	// node pass, and pending-link pass all share.
	//
	// LAYOUT. Left column: row 0 is the exec input (a PURE node has none, so its
	// first DATA input takes row 0), then one row per drawn data input. Right
	// column: rows 0..E-1 are the exec outputs, then one row per drawn data
	// output. The drawn data-pin NAMES are recorded on the panel state here, in
	// drawn order, because that order IS the key index the rects go in under.
	void BuildPinPositions(const Zenith_GraphDefinition& xDef, const ImVec2& xOrigin, Zenith_HashMap<u_int, PinPos>& xOut)
	{
		for (u_int u = 0; u < xDef.GetNodeCount(); ++u)
		{
			const Zenith_GraphNodeDef& xNodeDef = xDef.GetNodeAt(u);
			Zenith_Maths::Vector2 xPos(30.0f, 30.0f);
			xDef.GetNodeEditorPos(xNodeDef.m_uNodeID, xPos);
			const ImVec2 xMin(xOrigin.x + xPos.x, xOrigin.y + xPos.y);

			const u_int uOutputs = GetNodeExecOutputCount(xDef, xNodeDef.m_uNodeID);

			PinPos xPins;
			// Copied OUT of the cache immediately: the miss path can answer from a
			// shared scratch entry, so no pass may hold the pointer.
			const NodePinCacheEntry* pxCache = GetNodePinCacheEntry(xDef, xNodeDef.m_uNodeID);
			if (pxCache)
			{
				xPins.m_bHasExecInput = !pxCache->m_bPure;
				xPins.m_axDataInputNames = pxCache->m_axInputNames;
				xPins.m_axDataOutputNames = pxCache->m_axOutputNames;
				xPins.m_aeDataInputTypes = pxCache->m_aeInputTypes;
				xPins.m_aeDataOutputTypes = pxCache->m_aeOutputTypes;
			}

			const u_int uDataInputs = xPins.m_axDataInputNames.GetSize();
			const u_int uDataOutputs = xPins.m_axDataOutputNames.GetSize();
			const u_int uLeftRows = (xPins.m_bHasExecInput ? 1u : 0u) + uDataInputs;
			const u_int uRightRows = uOutputs + uDataOutputs;
			xPins.m_fNodeHeight = NodeBoxHeight(uLeftRows, uRightRows);
			xPins.m_xHeaderCentre = ImVec2(xMin.x + fNODE_WIDTH * 0.5f, xMin.y + fHEADER_HEIGHT * 0.5f);

			// Exec input: row 0 left, and ALWAYS row 0 - an existing exec test
			// reads its position, and a data pin must never push it down.
			xPins.m_xInput = ImVec2(xMin.x, PinRowY(xMin.y, 0u));
			for (u_int uPin = 0; uPin < uOutputs; ++uPin)
			{
				xPins.m_axOutputs.PushBack(ImVec2(xMin.x + fNODE_WIDTH, PinRowY(xMin.y, uPin)));
			}
			for (u_int uPin = 0; uPin < uDataInputs; ++uPin)
			{
				xPins.m_axDataInputs.PushBack(ImVec2(xMin.x, PinRowY(xMin.y, (xPins.m_bHasExecInput ? 1u : 0u) + uPin)));
			}
			for (u_int uPin = 0; uPin < uDataOutputs; ++uPin)
			{
				xPins.m_axDataOutputs.PushBack(ImVec2(xMin.x + fNODE_WIDTH, PinRowY(xMin.y, uOutputs + uPin)));
			}

			DrawnDataPinNames xNames;
			xNames.m_axInputs = xPins.m_axDataInputNames;
			xNames.m_axOutputs = xPins.m_axDataOutputNames;
			g_xGraphEditor.m_xDrawnDataPinNames[xNodeDef.m_uNodeID] = xNames;

			xOut[xNodeDef.m_uNodeID] = xPins;
		}
	}

	// The drawn index of one data pin by NAME, or GetSize() when this frame drew
	// no such pin.
	u_int FindDrawnDataPinIndex(const Zenith_Vector<std::string>& axNames, const std::string& strName)
	{
		for (u_int u = 0; u < axNames.GetSize(); ++u)
		{
			if (axNames.Get(u) == strName)
			{
				return u;
			}
		}
		return axNames.GetSize();
	}

	// ★ AN EDGE WHOSE ENDPOINT PIN CANNOT BE LOCATED IS DRAWN ANYWAY, dashed and
	// red, between the two node HEADERS. A wire that exists in the asset but is
	// invisible on the canvas is the worst of the three options: the author sees a
	// graph that does not match the file, and the findings panel's
	// "WIRE_PIN_UNKNOWN on node 7" names something they cannot see. Counted, so a
	// headless unit can prove the fallback RAN rather than that nothing crashed.
	void DrawUnresolvableEdge(ImDrawList* pxDrawList, const ImVec2& xFrom, const ImVec2& xTo)
	{
		const ImU32 uBROKEN_COLOUR = IM_COL32(230, 70, 70, 255);
		constexpr u_int uDASHES = 12;
		for (u_int uDash = 0; uDash < uDASHES; ++uDash)
		{
			// Every other segment drawn = a dashed line, on a draw list that has no
			// dash pattern of its own.
			if ((uDash & 1u) != 0u)
			{
				continue;
			}
			const float fA = static_cast<float>(uDash) / static_cast<float>(uDASHES);
			const float fB = static_cast<float>(uDash + 1u) / static_cast<float>(uDASHES);
			pxDrawList->AddLine(
				ImVec2(xFrom.x + (xTo.x - xFrom.x) * fA, xFrom.y + (xTo.y - xFrom.y) * fA),
				ImVec2(xFrom.x + (xTo.x - xFrom.x) * fB, xFrom.y + (xTo.y - xFrom.y) * fB),
				uBROKEN_COLOUR, 2.0f);
		}
		++g_xGraphEditor.m_uUnresolvableEdgeDrawCount;
	}

	void RenderCanvasEdges(ImDrawList* pxDrawList, const Zenith_GraphDefinition& xDef, const Zenith_HashMap<u_int, PinPos>& xPinPositions)
	{
		const ImU32 uEDGE_COLOUR = IM_COL32(200, 200, 120, 255);
		for (u_int u = 0; u < xDef.GetEdgeCount(); ++u)
		{
			const Zenith_GraphEdge& xEdge = xDef.GetEdgeAt(u);
			const PinPos* pxSrc = xPinPositions.TryGet(xEdge.m_uSrcNodeID);
			const PinPos* pxDst = xPinPositions.TryGet(xEdge.m_uDstNodeID);
			if (!pxSrc || !pxDst)
			{
				continue;	// an ORPHAN endpoint: there is no node box to draw between
			}
			if (xEdge.m_uSrcPin >= pxSrc->m_axOutputs.GetSize() || !pxDst->m_bHasExecInput)
			{
				// A pin the source does not have, or a PURE destination with no exec
				// input at all (only reachable from a loaded asset - the panel
				// refuses to author it, see TryConnect).
				DrawUnresolvableEdge(pxDrawList, pxSrc->m_xHeaderCentre, pxDst->m_xHeaderCentre);
				continue;
			}
			const ImVec2 xFrom = pxSrc->m_axOutputs.Get(xEdge.m_uSrcPin);
			const ImVec2 xTo = pxDst->m_xInput;
			pxDrawList->AddBezierCubic(xFrom, ImVec2(xFrom.x + 50.0f, xFrom.y), ImVec2(xTo.x - 50.0f, xTo.y), xTo, uEDGE_COLOUR, 2.0f);
		}

		// DATA wires, in the SOURCE's type colour: the value's type is the
		// producer's answer, and a consumer pin the validator disagrees with is
		// reported as a finding rather than recoloured into agreement.
		for (u_int u = 0; u < xDef.GetDataEdgeCount(); ++u)
		{
			const Zenith_GraphDataEdge& xEdge = xDef.GetDataEdgeAt(u);
			const PinPos* pxSrc = xPinPositions.TryGet(xEdge.m_uSrcNodeID);
			const PinPos* pxDst = xPinPositions.TryGet(xEdge.m_uDstNodeID);
			if (!pxSrc || !pxDst)
			{
				continue;
			}
			const u_int uSrcPin = FindDrawnDataPinIndex(pxSrc->m_axDataOutputNames, xEdge.m_strSrcPin);
			const u_int uDstPin = FindDrawnDataPinIndex(pxDst->m_axDataInputNames, xEdge.m_strDstPin);
			if (uSrcPin >= pxSrc->m_axDataOutputs.GetSize() || uDstPin >= pxDst->m_axDataInputs.GetSize())
			{
				DrawUnresolvableEdge(pxDrawList, pxSrc->m_xHeaderCentre, pxDst->m_xHeaderCentre);
				continue;
			}
			const ImVec2 xFrom = pxSrc->m_axDataOutputs.Get(uSrcPin);
			const ImVec2 xTo = pxDst->m_axDataInputs.Get(uDstPin);
			pxDrawList->AddBezierCubic(xFrom, ImVec2(xFrom.x + 50.0f, xFrom.y), ImVec2(xTo.x - 50.0f, xTo.y), xTo,
				PinTypeColour(pxSrc->m_aeDataOutputTypes.Get(uSrcPin)), 2.0f);
		}
	}

	ImU32 NodeHeaderColour(const Zenith_GraphNodeTypeInfo* pxInfo)
	{
		// Events green, flow orange, unresolved red, PURE purple, actions blue.
		if (!pxInfo)
		{
			return IM_COL32(170, 60, 60, 255);
		}
		// PURE first, and it cannot collide with the two below: the registry
		// refuses the pure flag on a flow node and on an event source.
		//
		// A pure node LOOKS different because it BEHAVES differently - no exec
		// pins, evaluated on demand by whoever pulls it - and a node with no
		// visible exec pins would otherwise just read as broken.
		if (pxInfo->m_bPureNode)
		{
			return IM_COL32(130, 90, 190, 255);
		}
		if (pxInfo->m_eEventType != GRAPH_EVENT_NONE)
		{
			return IM_COL32(70, 140, 80, 255);
		}
		if (pxInfo->m_bFlowNode)
		{
			return IM_COL32(180, 120, 50, 255);
		}
		return IM_COL32(70, 100, 160, 255);
	}

	bool IsNodeRecentlyExecuted(const Zenith_BehaviourGraph* pxLiveGraph, u_int uNodeID)
	{
		if (!pxLiveGraph)
		{
			return false;
		}
		const Zenith_Vector<u_int>& auRecent = pxLiveGraph->GetRecentlyExecuted();
		for (u_int u = 0; u < auRecent.GetSize(); ++u)
		{
			if (auRecent.Get(u) == uNodeID)
			{
				return true;
			}
		}
		return false;
	}

	// Pin visuals + interaction for one node (input pins accept pending links;
	// output pins start links; an exec output right-click-disconnects, and so does
	// a data INPUT - see TryDisconnectData for why the sides differ).
	//
	// uFailurePinIndex (uNO_FAILURE_PIN when the type has none) only changes the
	// exec pin's COLOUR: exec pins are bare circles with no labels, so a red-ish
	// "On Failure" pin is the whole affordance. Its rect, its key and its drag
	// behaviour are those of any other output pin. DATA pins, by contrast, carry
	// both a label and a type colour.
	void RenderNodePins(ImDrawList* pxDrawList, Zenith_GraphDefinition& xDef, u_int uNodeID, const PinPos& xPins,
		u_int uFailurePinIndex, const ImVec2& xNodeMax)
	{
		// A PURE node has NO exec input: none is drawn, none is keyed, and
		// GetPinScreenPos(node, 0, true) therefore answers false for it.
		if (xPins.m_bHasExecInput)
		{
			const ImVec2& xPinCentre = xPins.m_xInput;
			pxDrawList->AddCircleFilled(xPinCentre, fPIN_RADIUS, IM_COL32(220, 220, 220, 255));
			g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, 0, true, false)] = MakeRect(
				ImVec2(xPinCentre.x - 8.0f, xPinCentre.y - 8.0f), ImVec2(xPinCentre.x + 8.0f, xPinCentre.y + 8.0f));
			ImGui::SetCursorScreenPos(ImVec2(xPinCentre.x - 8.0f, xPinCentre.y - 8.0f));
			ImGui::InvisibleButton("pin_in", ImVec2(16.0f, 16.0f));
			if (g_xGraphEditor.m_bLinking && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				// THE one exec-drop funnel: it refuses a DATA link with text rather
				// than silently connecting the wrong kind of wire.
				CompletePendingLinkOnExecInput(uNodeID);
			}
		}

		for (u_int uPin = 0; uPin < xPins.m_axOutputs.GetSize(); ++uPin)
		{
			const ImVec2& xOutCentre = xPins.m_axOutputs.Get(uPin);
			const ImU32 uPinColour = (uPin == uFailurePinIndex)
				? IM_COL32(220, 100, 90, 255)		// On Failure
				: IM_COL32(160, 220, 160, 255);		// normal exec output
			pxDrawList->AddCircleFilled(xOutCentre, fPIN_RADIUS, uPinColour);
			g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, uPin, false, false)] = MakeRect(
				ImVec2(xOutCentre.x - 8.0f, xOutCentre.y - 8.0f), ImVec2(xOutCentre.x + 8.0f, xOutCentre.y + 8.0f));
			ImGui::PushID(static_cast<int>(uPin));
			ImGui::SetCursorScreenPos(ImVec2(xOutCentre.x - 8.0f, xOutCentre.y - 8.0f));
			ImGui::InvisibleButton("pin_out", ImVec2(16.0f, 16.0f));
			if (ImGui::IsItemActivated())
			{
				BeginPendingLink(uNodeID, uPin, nullptr);
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && xDef.RemoveEdge(uNodeID, uPin))
			{
				g_xGraphEditor.m_bDirty = true;
			}
			ImGui::PopID();
		}

		// DATA INPUTS, left, below the exec input. ImGui ids are offset by 1000 so
		// a data pin's id can never collide with an exec output's PushID(uPin) -
		// two items sharing an id is one hit-test, and the second pin would be
		// undraggable for reasons nothing would report.
		for (u_int uPin = 0; uPin < xPins.m_axDataInputs.GetSize(); ++uPin)
		{
			const ImVec2& xCentre = xPins.m_axDataInputs.Get(uPin);
			const std::string& strName = xPins.m_axDataInputNames.Get(uPin);
			pxDrawList->AddCircleFilled(xCentre, fPIN_RADIUS, PinTypeColour(xPins.m_aeDataInputTypes.Get(uPin)));
			pxDrawList->AddText(ImVec2(xCentre.x + fPIN_RADIUS + 4.0f, xCentre.y - 7.0f),
				IM_COL32(225, 225, 225, 255), strName.c_str());
			g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, uPin, true, true)] = MakeRect(
				ImVec2(xCentre.x - 8.0f, xCentre.y - 8.0f), ImVec2(xCentre.x + 8.0f, xCentre.y + 8.0f));
			ImGui::PushID(1000 + static_cast<int>(uPin));
			ImGui::SetCursorScreenPos(ImVec2(xCentre.x - 8.0f, xCentre.y - 8.0f));
			ImGui::InvisibleButton("dpin_in", ImVec2(16.0f, 16.0f));
			if (g_xGraphEditor.m_bLinking && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
			{
				CompletePendingLinkOnDataInput(uNodeID, strName.c_str());
			}
			// Disconnect on the DESTINATION, because a data wire is KEYED by it.
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
			{
				TryDisconnectData(uNodeID, strName.c_str());
			}
			ImGui::PopID();
		}

		// DATA OUTPUTS, right, below the exec outputs. Labels are right-aligned
		// inside the box so they cannot run out over the canvas.
		for (u_int uPin = 0; uPin < xPins.m_axDataOutputs.GetSize(); ++uPin)
		{
			const ImVec2& xCentre = xPins.m_axDataOutputs.Get(uPin);
			const std::string& strName = xPins.m_axDataOutputNames.Get(uPin);
			pxDrawList->AddCircleFilled(xCentre, fPIN_RADIUS, PinTypeColour(xPins.m_aeDataOutputTypes.Get(uPin)));
			const ImVec2 xTextSize = ImGui::CalcTextSize(strName.c_str());
			pxDrawList->AddText(ImVec2(xNodeMax.x - fPIN_RADIUS - 4.0f - xTextSize.x, xCentre.y - 7.0f),
				IM_COL32(225, 225, 225, 255), strName.c_str());
			g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, uPin, false, true)] = MakeRect(
				ImVec2(xCentre.x - 8.0f, xCentre.y - 8.0f), ImVec2(xCentre.x + 8.0f, xCentre.y + 8.0f));
			ImGui::PushID(1000 + static_cast<int>(uPin));
			ImGui::SetCursorScreenPos(ImVec2(xCentre.x - 8.0f, xCentre.y - 8.0f));
			ImGui::InvisibleButton("dpin_out", ImVec2(16.0f, 16.0f));
			if (ImGui::IsItemActivated())
			{
				BeginPendingLink(uNodeID, 0, strName.c_str());
			}
			// Right-click is deliberately INERT here: an output fans out to any
			// number of wires, so "which one" has no answer. See TryDisconnectData.
			ImGui::PopID();
		}
	}

	// One node: visuals (header colour, selection/execution outlines), body
	// interaction (click-select via the return value, drag-move), and pins.
	u_int RenderCanvasNode(ImDrawList* pxDrawList, Zenith_GraphDefinition& xDef, const Zenith_GraphNodeDef& xNodeDef,
		const Zenith_HashMap<u_int, PinPos>& xPinPositions, const Zenith_BehaviourGraph* pxLiveGraph, const ImVec2& xOrigin)
	{
		const u_int uNodeID = xNodeDef.m_uNodeID;
		Zenith_Maths::Vector2 xPos(30.0f, 30.0f);
		xDef.GetNodeEditorPos(uNodeID, xPos);

		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(xNodeDef.m_strTypeName.c_str());
		// The height comes from the SAME pass that laid the pin rows out, so a box
		// can never be shorter than its own pins (a second copy of the row
		// arithmetic here is exactly how that would happen).
		const PinPos* pxPins = xPinPositions.TryGet(uNodeID);
		const float fNodeHeight = pxPins ? pxPins->m_fNodeHeight : NodeBoxHeight(1u, GetNodeExecOutputCount(xDef, uNodeID));

		const ImVec2 xMin(xOrigin.x + xPos.x, xOrigin.y + xPos.y);
		const ImVec2 xMax(xMin.x + fNODE_WIDTH, xMin.y + fNodeHeight);

		pxDrawList->AddRectFilled(xMin, xMax, IM_COL32(45, 45, 50, 255), 5.0f);
		pxDrawList->AddRectFilled(xMin, ImVec2(xMax.x, xMin.y + fHEADER_HEIGHT), NodeHeaderColour(pxInfo), 5.0f);
		pxDrawList->AddText(ImVec2(xMin.x + 6.0f, xMin.y + 4.0f), IM_COL32(255, 255, 255, 255), xNodeDef.m_strTypeName.c_str());

		if (uNodeID == g_xGraphEditor.m_uSelectedNodeID)
		{
			pxDrawList->AddRect(xMin, xMax, IM_COL32(255, 255, 255, 255), 5.0f, 0, 2.0f);
		}
		if (IsNodeRecentlyExecuted(pxLiveGraph, uNodeID))
		{
			pxDrawList->AddRect(ImVec2(xMin.x - 2.0f, xMin.y - 2.0f), ImVec2(xMax.x + 2.0f, xMax.y + 2.0f),
				IM_COL32(255, 220, 60, 255), 6.0f, 0, 3.0f);
		}
		if (!pxInfo)
		{
			pxDrawList->AddText(ImVec2(xMin.x + 6.0f, xMin.y + fHEADER_HEIGHT + 4.0f), IM_COL32(255, 120, 120, 255), "UNRESOLVED");
		}

		g_xGraphEditor.m_xNodeRects[uNodeID] = MakeRect(xMin, xMax);

		ImGui::SetCursorScreenPos(xMin);
		ImGui::PushID(static_cast<int>(uNodeID));
		ImGui::InvisibleButton("node", ImVec2(fNODE_WIDTH, fNodeHeight));
		const bool bClicked = ImGui::IsItemActivated();
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			const ImVec2 xDelta = ImGui::GetIO().MouseDelta;
			xDef.SetNodeEditorPos(uNodeID, Zenith_Maths::Vector2(xPos.x + xDelta.x, xPos.y + xDelta.y));
			g_xGraphEditor.m_bDirty = true;
		}

		if (pxPins)
		{
			RenderNodePins(pxDrawList, xDef, uNodeID, *pxPins, GetFailurePinIndex(pxInfo), xMax);
		}
		ImGui::PopID();

		return bClicked ? uNodeID : 0;
	}

	void RenderPendingLink(ImDrawList* pxDrawList, const Zenith_HashMap<u_int, PinPos>& xPinPositions)
	{
		if (!g_xGraphEditor.m_bLinking)
		{
			return;
		}
		const PinPos* pxSrc = xPinPositions.TryGet(g_xGraphEditor.m_uLinkSrcNodeID);
		// The rubber band starts at the pin the drag started on, and a DATA drag
		// holds its source by NAME - indexing the exec-output array with
		// m_uLinkSrcPin (which is 0 for every data drag) would draw the band from
		// the wrong pin, or from no pin at all on a pure node.
		bool bHaveFrom = false;
		ImVec2 xFrom(0.0f, 0.0f);
		if (pxSrc && IsPendingLinkData())
		{
			const u_int uIndex = FindDrawnDataPinIndex(pxSrc->m_axDataOutputNames, g_xGraphEditor.m_strLinkSrcDataPin);
			if (uIndex < pxSrc->m_axDataOutputs.GetSize())
			{
				xFrom = pxSrc->m_axDataOutputs.Get(uIndex);
				bHaveFrom = true;
			}
		}
		else if (pxSrc && g_xGraphEditor.m_uLinkSrcPin < pxSrc->m_axOutputs.GetSize())
		{
			xFrom = pxSrc->m_axOutputs.Get(g_xGraphEditor.m_uLinkSrcPin);
			bHaveFrom = true;
		}
		if (bHaveFrom)
		{
			const ImVec2 xTo = ImGui::GetIO().MousePos;
			pxDrawList->AddBezierCubic(xFrom, ImVec2(xFrom.x + 50.0f, xFrom.y), ImVec2(xTo.x - 50.0f, xTo.y), xTo,
				IM_COL32(255, 255, 255, 160), 2.0f);
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
		{
			g_xGraphEditor.m_bLinking = false;
			g_xGraphEditor.m_strLinkSrcDataPin.clear();
		}
	}

	void HandleCanvasDeleteKey(Zenith_GraphDefinition& xDef)
	{
		if (g_xGraphEditor.m_uSelectedNodeID != 0 && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)
			&& ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			xDef.RemoveNode(g_xGraphEditor.m_uSelectedNodeID);
			g_xGraphEditor.m_uSelectedNodeID = 0;
			DestroyParamInstance();
			g_xGraphEditor.m_bDirty = true;
			// Node ids are not reused within a session, but the cache is cleared
			// whole anyway: a removed node's entry would otherwise outlive it.
			InvalidateNodePinCache();
		}
	}

	void RenderCanvas()
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef)
		{
			return;
		}

		ImGui::BeginChild("GraphCanvas", ImVec2(0, 0), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImDrawList* pxDrawList = ImGui::GetWindowDrawList();
		const ImVec2 xOrigin = ImGui::GetCursorScreenPos();
		const Zenith_BehaviourGraph* pxLiveGraph = FindLiveGraphForHighlight();

		Zenith_HashMap<u_int, PinPos> xPinPositions;
		BuildPinPositions(*pxDef, xOrigin, xPinPositions);
		RenderCanvasEdges(pxDrawList, *pxDef, xPinPositions);

		u_int uClickedNodeID = 0;
		for (u_int u = 0; u < pxDef->GetNodeCount(); ++u)
		{
			const u_int uClicked = RenderCanvasNode(pxDrawList, *pxDef, pxDef->GetNodeAt(u), xPinPositions, pxLiveGraph, xOrigin);
			if (uClicked != 0)
			{
				uClickedNodeID = uClicked;
			}
		}

		if (uClickedNodeID != 0 && uClickedNodeID != g_xGraphEditor.m_uSelectedNodeID)
		{
			g_xGraphEditor.m_uSelectedNodeID = uClickedNodeID;
			RefreshParamInstanceForSelection();
		}

		RenderPendingLink(pxDrawList, xPinPositions);
		HandleCanvasDeleteKey(*pxDef);

		ImGui::EndChild();
	}
}

//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

void Zenith_GraphEditorPanel::Render()
{
	if (!g_xGraphEditor.m_bOpen)
	{
		return;
	}

	// Cleared each frame; rebuilt by the sections below.
	g_xGraphEditor.m_xPaletteRects.Clear();
	g_xGraphEditor.m_xNodeRects.Clear();
	g_xGraphEditor.m_xPinRects.Clear();
	g_xGraphEditor.m_xToolbarRects.Clear();
	g_xGraphEditor.m_xPropertyRowRects.Clear();
	// Per-FRAME like the rects above (the per-INVALIDATION pin cache is NOT
	// cleared here - that is the whole point of it).
	g_xGraphEditor.m_xDrawnDataPinNames.Clear();
	g_xGraphEditor.m_uUnresolvableEdgeDrawCount = 0;

	if (g_xGraphEditor.m_bPositionWindowNextRender)
	{
		// Deterministic placement for automated tests + a sane default, CLAMPED
		// to the viewport so the window is never partly off-screen.
		//
		// This used to be a flat 1100x980 at (60,40), chosen to be "tall enough
		// that the whole left column renders unclipped". That stopped being true
		// once the node registry grew (the Behaviour Graph adoption): the left
		// column is now several thousand pixels tall, so no window size can
		// contain it, and at 1280x720 the old size also hung 300px below the
		// screen. Sizing to fit and SCROLLING the palette is the durable answer --
		// see ScrollPaletteEntryIntoView.
		const ImGuiViewport* pxViewport = ImGui::GetMainViewport();
		const ImVec2 xWork = pxViewport ? pxViewport->WorkSize : ImVec2(1280.0f, 720.0f);
		const ImVec2 xOffset(60.0f, 40.0f);
		const float fWidth  = Clampf(xWork.x - xOffset.x - 20.0f, 320.0f, 1100.0f);
		const float fHeight = Clampf(xWork.y - xOffset.y - 20.0f, 240.0f, 980.0f);
		ImGui::SetNextWindowPos(ImVec2(
			(pxViewport ? pxViewport->WorkPos.x : 0.0f) + xOffset.x,
			(pxViewport ? pxViewport->WorkPos.y : 0.0f) + xOffset.y));
		ImGui::SetNextWindowSize(ImVec2(fWidth, fHeight));
		g_xGraphEditor.m_bPositionWindowNextRender = false;
	}

	if (!ImGui::Begin(szEDITOR_WINDOW_GRAPH_EDITOR, &g_xGraphEditor.m_bOpen))
	{
		ImGui::End();
		return;
	}

	RenderToolbarRow();
	ImGui::Separator();

	// Properties + variables FIRST (small, always visible); the long palette
	// last so its overflow is what scrolls.
	//
	// The palette gets its OWN scrolling child, and that is load-bearing rather
	// than cosmetic. Sharing one scroll region with the properties made the
	// "always visible" above a lie: the palette lists every registered node type
	// and is thousands of pixels tall, so scrolling far enough to reach an entry
	// pushed the property rows clean off the TOP of the screen (observed:
	// property row at screen y = -2488). Separate regions mean reaching any
	// palette entry never moves the properties.
	ImGui::BeginChild("GraphLeftColumn", ImVec2(280.0f, 0), true);
	RenderSelectedNodeProperties();
	ImGui::Spacing();
	RenderVariables();
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::BeginChild("GraphPaletteScroll", ImVec2(0.0f, 0.0f), true);
	RenderPalette();
	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::SameLine();
	RenderCanvas();

	ImGui::End();
}

void Zenith_GraphEditorPanel::OpenAsset(const char* szAssetPath)
{
	if (!szAssetPath || szAssetPath[0] == '\0')
	{
		return;
	}

	Close();

	g_xGraphEditor.m_strAssetPath = Zenith_AssetRegistry::NormalizeAssetPath(szAssetPath);

	Zenith_BehaviourGraphAsset* pxCached = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(g_xGraphEditor.m_strAssetPath);
	if (pxCached && pxCached->LoadedOk())
	{
		g_xGraphEditor.m_pxAsset = pxCached;
		g_xGraphEditor.m_bOwnsAsset = false;
	}
	else
	{
		// New (or unloadable) asset: edit an owned empty definition; the first
		// Save writes it to disk and swaps to the registry-cached instance.
		g_xGraphEditor.m_pxAsset = new Zenith_BehaviourGraphAsset();
		g_xGraphEditor.m_bOwnsAsset = true;
	}

	g_xGraphEditor.m_bOpen = true;
	g_xGraphEditor.m_bPositionWindowNextRender = true;
	g_xGraphEditor.m_bDirty = false;
	g_xGraphEditor.m_uSelectedNodeID = 0;
	g_xGraphEditor.m_strConnectRefusal.clear();
	// A different definition entirely: every cached pin list belongs to the old
	// one, and node ids collide across assets.
	InvalidateNodePinCache();

	// Validation on LOAD: an asset whose bindings went stale while nobody was
	// looking says so the moment it is opened. Advisory - it still opens.
	ValidateOpenGraph();
}

void Zenith_GraphEditorPanel::Close()
{
	DestroyParamInstance();
	if (g_xGraphEditor.m_bOwnsAsset)
	{
		delete g_xGraphEditor.m_pxAsset;
	}
	g_xGraphEditor.m_pxAsset = nullptr;
	g_xGraphEditor.m_bOwnsAsset = false;
	g_xGraphEditor.m_bOpen = false;
	g_xGraphEditor.m_strAssetPath.clear();
	g_xGraphEditor.m_uSelectedNodeID = 0;
	g_xGraphEditor.m_bLinking = false;
	g_xGraphEditor.m_strLinkSrcDataPin.clear();
	g_xGraphEditor.m_bDirty = false;
	g_xGraphEditor.m_strConnectRefusal.clear();
	g_xGraphEditor.m_axValidationFindings.Clear();
	InvalidateNodePinCache();
	g_xGraphEditor.m_xDrawnDataPinNames.Clear();
#ifdef ZENITH_TESTING
	g_xGraphEditor.m_strScrollToPaletteEntry.clear();
#endif
}

bool Zenith_GraphEditorPanel::IsOpen()
{
	return g_xGraphEditor.m_bOpen;
}

void Zenith_GraphEditorPanel::Save()
{
	if (!g_xGraphEditor.m_pxAsset || g_xGraphEditor.m_strAssetPath.empty())
	{
		return;
	}

	// First save into a game whose Graphs/ asset directory doesn't exist yet
	// must create it (file-open would otherwise fail silently).
	{
		std::error_code xEC;
		std::filesystem::create_directories(
			std::filesystem::path(Zenith_AssetRegistry::ResolvePath(g_xGraphEditor.m_strAssetPath)).parent_path(), xEC);
	}

	if (!Zenith_AssetRegistry::Save(g_xGraphEditor.m_pxAsset, g_xGraphEditor.m_strAssetPath))
	{
		Zenith_Error(LOG_CATEGORY_EDITOR, "GraphEditor: failed to save '%s'", g_xGraphEditor.m_strAssetPath.c_str());
		return;
	}
	g_xGraphEditor.m_bDirty = false;

	// If we were editing an owned (new) asset, hand over to the registry-cached
	// instance via a serialize-copy so live components and the panel share one
	// definition from now on.
	if (g_xGraphEditor.m_bOwnsAsset)
	{
		Zenith_BehaviourGraphAsset* pxCached = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(g_xGraphEditor.m_strAssetPath);
		if (pxCached && pxCached != g_xGraphEditor.m_pxAsset)
		{
			Zenith_DataStream xCopy;
			g_xGraphEditor.m_pxAsset->GetDefinition().WriteToDataStream(xCopy);
			xCopy.SetCursor(0);
			// These bytes came out of the writer one line up, so this cannot fail
			// today - which is exactly why the return is checked: a future format
			// defect should be loud here rather than leave the cached definition
			// silently EMPTY (ReadFromDataStream clears on refusal).
			if (!pxCached->GetDefinition().ReadFromDataStream(xCopy))
			{
				Zenith_Error(LOG_CATEGORY_EDITOR,
					"GraphEditor: the definition just written for '%s' could not be read back - the cached asset is now empty",
					g_xGraphEditor.m_strAssetPath.c_str());
			}
			delete g_xGraphEditor.m_pxAsset;
			g_xGraphEditor.m_pxAsset = pxCached;
			g_xGraphEditor.m_bOwnsAsset = false;
			DestroyParamInstance();
		}
	}

	// Queue live hot reload (drained at the main loop's safe point). The pin cache
	// goes with it: a save can hand the panel a DIFFERENT definition object (the
	// owned-to-cached swap above), and a reload re-reads the asset.
	InvalidateNodePinCache();
	Zenith_GraphReload::NotifyAssetChanged(g_xGraphEditor.m_strAssetPath.c_str());
	Zenith_Log(LOG_CATEGORY_EDITOR, "GraphEditor: saved '%s'", g_xGraphEditor.m_strAssetPath.c_str());
}

const char* Zenith_GraphEditorPanel::GetOpenAssetPath()
{
	return g_xGraphEditor.m_strAssetPath.c_str();
}

//------------------------------------------------------------------------------
// Open-editor hook (Core/Zenith_GraphEditorHook.h): engine-side panels open
// this editor through the constant-initialised fn ptr below - no layer-up
// include, and referencing the pointer pulls this TU in.
//------------------------------------------------------------------------------
#include "Core/Zenith_GraphEditorHook.h"

namespace
{
	void OpenGraphEditorThunk(const char* szAssetPath)
	{
		Zenith_GraphEditorPanel::OpenAsset(szAssetPath);
	}
}

Zenith_OpenGraphEditorFn g_pfnZenithOpenGraphEditor = &OpenGraphEditorThunk;

//------------------------------------------------------------------------------
// Atomic editor actions (Zenith_EditorAutomation drives these; each is the
// exact operation the matching UI handler runs)
//------------------------------------------------------------------------------

bool Zenith_GraphEditorPanel::Action_AddNode(const char* szTypeName)
{
	// == the palette-entry click handler.
	return AddNodeAtFreeSpot(szTypeName);
}

bool Zenith_GraphEditorPanel::Action_Connect(const char* szSrcTypeName, u_int uSrcOccurrence, u_int uSrcPin,
                                             const char* szDstTypeName, u_int uDstOccurrence)
{
	// == the pin drag-drop completion handler (drop is only ever onto a node's
	// single input pin; the source pin must be one the canvas actually renders).
	// The two used to be divergent copies - this resolves the occurrences and
	// then runs the SAME TryConnect body the canvas drop does.
	const u_int uSrcNodeID = ResolveNodeByTypeOccurrence(szSrcTypeName, uSrcOccurrence);
	const u_int uDstNodeID = ResolveNodeByTypeOccurrence(szDstTypeName, uDstOccurrence);
	return TryConnect(uSrcNodeID, uSrcPin, uDstNodeID);
}

bool Zenith_GraphEditorPanel::Action_SelectNode(const char* szTypeName, u_int uOccurrence)
{
	// == the canvas node click handler.
	const u_int uNodeID = ResolveNodeByTypeOccurrence(szTypeName, uOccurrence);
	if (uNodeID == 0)
	{
		return false;
	}
	g_xGraphEditor.m_uSelectedNodeID = uNodeID;
	RefreshParamInstanceForSelection();
	return true;
}

namespace
{
	// Shared body of the param-edit actions: look up the property on the
	// SELECTED node's param instance, set it through the reflected table, and
	// commit - exactly what an ImGui edit in the property panel does.
	bool SetSelectedParamThroughTable(const char* szPropertyName, const Zenith_PropertyValue& xValue, Zenith_PropertyType eExpected)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (!pxDef || g_xGraphEditor.m_uSelectedNodeID == 0)
		{
			return false;
		}
		RefreshParamInstanceForSelection();
		if (!g_xGraphEditor.m_pxParamInstance || g_xGraphEditor.m_uParamInstanceNodeID == 0)
		{
			return false;
		}
		const Zenith_GraphNodeDef* pxNodeDef = pxDef->FindNodeDef(g_xGraphEditor.m_uParamInstanceNodeID);
		if (!pxNodeDef)
		{
			return false;
		}
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(pxNodeDef->m_strTypeName.c_str());
		if (!pxInfo || !pxInfo->m_pfnGetPropertyTable)
		{
			return false;
		}
		const Zenith_ReflectedProperty* pxProperty = pxInfo->m_pfnGetPropertyTable()->FindProperty(szPropertyName);
		if (!pxProperty || pxProperty->m_eType != eExpected || !pxProperty->m_pfnSet)
		{
			return false;
		}
		pxProperty->m_pfnSet(g_xGraphEditor.m_pxParamInstance, xValue);
		OnSelectedNodeParamChanged(nullptr, szPropertyName);
		return true;
	}
}

bool Zenith_GraphEditorPanel::Action_SetSelectedNodeParamFloat(const char* szPropertyName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return SetSelectedParamThroughTable(szPropertyName, xValue, PROPERTY_TYPE_FLOAT);
}

bool Zenith_GraphEditorPanel::Action_SetSelectedNodeParamString(const char* szPropertyName, const char* szValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetString(std::string(szValue ? szValue : ""));
	return SetSelectedParamThroughTable(szPropertyName, xValue, PROPERTY_TYPE_STRING);
}

bool Zenith_GraphEditorPanel::Action_SetSelectedNodeParamVec3(const char* szPropertyName, float fX, float fY, float fZ)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(Zenith_Maths::Vector3(fX, fY, fZ));
	return SetSelectedParamThroughTable(szPropertyName, xValue, PROPERTY_TYPE_VECTOR3);
}

bool Zenith_GraphEditorPanel::Action_SetSelectedNodeParamInt(const char* szPropertyName, int iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	return SetSelectedParamThroughTable(szPropertyName, xValue, PROPERTY_TYPE_INT32);
}

bool Zenith_GraphEditorPanel::Action_SetSelectedNodeParamBool(const char* szPropertyName, bool bValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetBool(bValue);
	return SetSelectedParamThroughTable(szPropertyName, xValue, PROPERTY_TYPE_BOOL);
}

bool Zenith_GraphEditorPanel::Action_AddVariable(const char* szName, const char* szTypeName, float fDefaultNumeric)
{
	// == the "Add Var" button handler (same name/type/default semantics).
	Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	if (!pxDef || !szName || szName[0] == '\0' || !szTypeName)
	{
		return false;
	}
	Zenith_PropertyValue xDefault;
	if (std::strcmp(szTypeName, "float") == 0)        { xDefault.SetFloat(fDefaultNumeric); }
	else if (std::strcmp(szTypeName, "int") == 0)     { xDefault.SetInt32(static_cast<int32_t>(fDefaultNumeric)); }
	else if (std::strcmp(szTypeName, "bool") == 0)    { xDefault.SetBool(fDefaultNumeric != 0.0f); }
	else if (std::strcmp(szTypeName, "string") == 0)  { xDefault.SetString(std::string()); }
	else if (std::strcmp(szTypeName, "vector3") == 0) { xDefault.SetVector3(Zenith_Maths::Vector3(0.0f)); }
	else if (std::strcmp(szTypeName, "vector2") == 0) { xDefault.SetVector2(Zenith_Maths::Vector2(0.0f)); }
	else if (std::strcmp(szTypeName, "vector4") == 0) { xDefault.SetVector4(Zenith_Maths::Vector4(0.0f)); }
	else if (std::strcmp(szTypeName, "entity") == 0)  { xDefault.SetPackedEntityID(0); }
	else { return false; }
	pxDef->DeclareVariable(szName, xDefault);
	g_xGraphEditor.m_bDirty = true;
	InvalidateNodePinCache();	// a from-variable pin takes the DECLARED type
	return true;
}

void Zenith_GraphEditorPanel::OpenAssetFresh(const char* szAssetPath)
{
	OpenAsset(szAssetPath);
	Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	if (pxDef)
	{
		pxDef->Clear();
		g_xGraphEditor.m_uSelectedNodeID = 0;
		DestroyParamInstance();
		g_xGraphEditor.m_bDirty = true;
		InvalidateNodePinCache();
		// The definition OpenAsset validated no longer exists - re-run over the
		// empty one so the panel never displays a report for a discarded graph.
		ValidateOpenGraph();
	}
}

//------------------------------------------------------------------------------
// Refusals + validation report
//------------------------------------------------------------------------------

const char* Zenith_GraphEditorPanel::GetConnectRefusalText()
{
	return g_xGraphEditor.m_strConnectRefusal.c_str();
}

u_int Zenith_GraphEditorPanel::GetValidationFindingCount()
{
	return g_xGraphEditor.m_axValidationFindings.GetSize();
}

u_int Zenith_GraphEditorPanel::GetValidationErrorCount()
{
	return CountValidationFindingsOfSeverity(GRAPH_VALIDATION_SEVERITY_ERROR);
}

const Zenith_GraphValidationFinding* Zenith_GraphEditorPanel::GetValidationFindingAt(u_int uIndex)
{
	if (uIndex >= g_xGraphEditor.m_axValidationFindings.GetSize())
	{
		return nullptr;
	}
	return &g_xGraphEditor.m_axValidationFindings.Get(uIndex);
}

//------------------------------------------------------------------------------
// Test accessors
//------------------------------------------------------------------------------

#ifdef ZENITH_TESTING

namespace
{
	// A rect whose centre lies outside the display is one no simulated click can
	// ever land on, so reporting it is worse than reporting nothing: the caller
	// clicks into space and the failure surfaces far away as "the thing I asked
	// for did not happen". Both of this panel's scroll regions can park content
	// off-screen in either direction (a palette row below the bottom, a property
	// row above the top), so every accessor is gated on this.
	bool IsOnScreen(const Zenith_Maths::Vector2& xPoint)
	{
		const ImGuiIO& xIO = ImGui::GetIO();
		return xPoint.x >= 0.0f && xPoint.y >= 0.0f
		    && xPoint.x <= xIO.DisplaySize.x && xPoint.y <= xIO.DisplaySize.y;
	}

	bool RectCentre(const PanelRect* pxRect, Zenith_Maths::Vector2& xOut)
	{
		if (!pxRect)
		{
			return false;
		}
		const Zenith_Maths::Vector2 xCentre = pxRect->Centre();
		if (!IsOnScreen(xCentre))
		{
			return false;
		}
		xOut = xCentre;
		return true;
	}
}

bool Zenith_GraphEditorPanel::ScrollPaletteEntryIntoView(const char* szTypeName)
{
	if (!szTypeName || szTypeName[0] == '\0')
	{
		return false;
	}
	if (Zenith_GraphNodeRegistry::Get().Find(szTypeName) == nullptr)
	{
		return false;
	}
	g_xGraphEditor.m_strScrollToPaletteEntry = szTypeName;
	return true;
}

bool Zenith_GraphEditorPanel::GetPaletteEntryScreenPos(const char* szTypeName, Zenith_Maths::Vector2& xOut)
{
	return RectCentre(g_xGraphEditor.m_xPaletteRects.TryGet(std::string(szTypeName ? szTypeName : "")), xOut);
}

bool Zenith_GraphEditorPanel::GetNodeScreenPos(u_int uNodeID, Zenith_Maths::Vector2& xOut)
{
	return RectCentre(g_xGraphEditor.m_xNodeRects.TryGet(uNodeID), xOut);
}

bool Zenith_GraphEditorPanel::GetPinScreenPos(u_int uNodeID, u_int uPin, bool bInputPin, Zenith_Maths::Vector2& xOut)
{
	return RectCentre(g_xGraphEditor.m_xPinRects.TryGet(MakePinKey(uNodeID, uPin, bInputPin, false)), xOut);
}

bool Zenith_GraphEditorPanel::GetDataPinScreenPos(u_int uNodeID, const char* szPinName, bool bInputPin, Zenith_Maths::Vector2& xOut)
{
	const DrawnDataPinNames* pxNames = g_xGraphEditor.m_xDrawnDataPinNames.TryGet(uNodeID);
	if (!pxNames || szPinName == nullptr || szPinName[0] == '\0')
	{
		return false;
	}
	const Zenith_Vector<std::string>& axNames = bInputPin ? pxNames->m_axInputs : pxNames->m_axOutputs;
	const u_int uIndex = FindDrawnDataPinIndex(axNames, std::string(szPinName));
	if (uIndex >= axNames.GetSize())
	{
		return false;	// this frame drew no such pin
	}
	return RectCentre(g_xGraphEditor.m_xPinRects.TryGet(MakePinKey(uNodeID, uIndex, bInputPin, true)), xOut);
}

bool Zenith_GraphEditorPanel::GetNodeScreenRect(u_int uNodeID, Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax)
{
	const PanelRect* pxRect = g_xGraphEditor.m_xNodeRects.TryGet(uNodeID);
	if (!pxRect || !IsOnScreen(pxRect->Centre()))
	{
		return false;
	}
	xOutMin = Zenith_Maths::Vector2(pxRect->m_fMinX, pxRect->m_fMinY);
	xOutMax = Zenith_Maths::Vector2(pxRect->m_fMaxX, pxRect->m_fMaxY);
	return true;
}

bool Zenith_GraphEditorPanel::GetToolbarButtonScreenPos(const char* szLabel, Zenith_Maths::Vector2& xOut)
{
	return RectCentre(g_xGraphEditor.m_xToolbarRects.TryGet(std::string(szLabel ? szLabel : "")), xOut);
}

bool Zenith_GraphEditorPanel::GetPropertyRowScreenPos(const char* szPropertyName, Zenith_Maths::Vector2& xOut)
{
	return RectCentre(g_xGraphEditor.m_xPropertyRowRects.TryGet(std::string(szPropertyName ? szPropertyName : "")), xOut);
}

bool Zenith_GraphEditorPanel::GetPropertyRowScreenRect(const char* szPropertyName, Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax)
{
	const PanelRect* pxRect = g_xGraphEditor.m_xPropertyRowRects.TryGet(std::string(szPropertyName ? szPropertyName : ""));
	if (!pxRect)
	{
		return false;
	}
	// Same off-screen gate as RectCentre - a scrolled-away property row is not
	// clickable, and handing its coordinates out produces a click into space.
	if (!IsOnScreen(pxRect->Centre()))
	{
		return false;
	}
	xOutMin = Zenith_Maths::Vector2(pxRect->m_fMinX, pxRect->m_fMinY);
	xOutMax = Zenith_Maths::Vector2(pxRect->m_fMaxX, pxRect->m_fMaxY);
	return true;
}

u_int Zenith_GraphEditorPanel::GetNodeCount()
{
	const Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	return pxDef ? pxDef->GetNodeCount() : 0;
}

u_int Zenith_GraphEditorPanel::GetEdgeCount()
{
	const Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	return pxDef ? pxDef->GetEdgeCount() : 0;
}

u_int Zenith_GraphEditorPanel::GetDataEdgeCount()
{
	const Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	return pxDef ? pxDef->GetDataEdgeCount() : 0;
}

u_int Zenith_GraphEditorPanel::GetPinTypeCacheFillCountForTest()
{
	return g_xGraphEditor.m_uPinTypeCacheFillCount;
}

u_int Zenith_GraphEditorPanel::GetUnresolvableEdgeDrawCountForTest()
{
	return g_xGraphEditor.m_uUnresolvableEdgeDrawCount;
}

bool Zenith_GraphEditorPanel::IsNodeHighlightedForTest(u_int uNodeID)
{
	// The SAME two calls the canvas outline makes, in the same order - so this
	// cannot answer true for a node the canvas would not glow.
	return IsNodeRecentlyExecuted(FindLiveGraphForHighlight(), uNodeID);
}

bool Zenith_GraphEditorPanel::Action_ConnectData(const char* szSrcTypeName, u_int uSrcOccurrence, const char* szSrcPin,
                                                 const char* szDstTypeName, u_int uDstOccurrence, const char* szDstPin)
{
	// == the data-pin drag-drop completion handler: resolve the occurrences, then
	// run the SAME TryConnectData body the canvas drop does.
	const u_int uSrcNodeID = ResolveNodeByTypeOccurrence(szSrcTypeName, uSrcOccurrence);
	const u_int uDstNodeID = ResolveNodeByTypeOccurrence(szDstTypeName, uDstOccurrence);
	return TryConnectData(uSrcNodeID, szSrcPin, uDstNodeID, szDstPin);
}

bool Zenith_GraphEditorPanel::Action_DisconnectData(const char* szDstTypeName, u_int uDstOccurrence, const char* szDstPin)
{
	// == right-clicking that input data pin.
	return TryDisconnectData(ResolveNodeByTypeOccurrence(szDstTypeName, uDstOccurrence), szDstPin);
}

u_int Zenith_GraphEditorPanel::GetSelectedNodeID()
{
	return g_xGraphEditor.m_uSelectedNodeID;
}

u_int Zenith_GraphEditorPanel::FindNodeIDByType(const char* szTypeName, u_int uOccurrence)
{
	return ResolveNodeByTypeOccurrence(szTypeName, uOccurrence);
}

bool Zenith_GraphEditorPanel::IsDirty()
{
	return g_xGraphEditor.m_bDirty;
}

bool Zenith_GraphEditorPanel::GetSelectedNodeParamFloat(const char* szPropertyName, float& fOut)
{
	Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	if (!pxDef || !g_xGraphEditor.m_pxParamInstance || g_xGraphEditor.m_uParamInstanceNodeID == 0)
	{
		return false;
	}
	const Zenith_GraphNodeDef* pxNodeDef = pxDef->FindNodeDef(g_xGraphEditor.m_uParamInstanceNodeID);
	if (!pxNodeDef)
	{
		return false;
	}
	const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(pxNodeDef->m_strTypeName.c_str());
	if (!pxInfo || !pxInfo->m_pfnGetPropertyTable)
	{
		return false;
	}
	const Zenith_ReflectedProperty* pxProperty = pxInfo->m_pfnGetPropertyTable()->FindProperty(szPropertyName);
	if (!pxProperty || pxProperty->m_eType != PROPERTY_TYPE_FLOAT)
	{
		return false;
	}
	Zenith_PropertyValue xValue;
	pxProperty->m_pfnGet(g_xGraphEditor.m_pxParamInstance, xValue);
	fOut = xValue.GetFloat();
	return true;
}

#endif // ZENITH_TESTING

// Unit tests for this panel. Included unconditionally (the .inl guards its own
// body with ZENITH_TESTING) and INSIDE the ZENITH_TOOLS block, because
// everything it drives - the panel, its Action_* verbs, its rect accessors -
// exists only in a tools build.
#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.Tests.inl"

#endif // ZENITH_TOOLS
