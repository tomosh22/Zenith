#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_GraphReload.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_Engine.h"
#include "Collections/Zenith_HashMap.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <filesystem>

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

	// Pin key: (node << 9) | (pin << 1) | input-bit.
	u_int64 MakePinKey(u_int uNodeID, u_int uPin, bool bInput)
	{
		return (static_cast<u_int64>(uNodeID) << 9) | (static_cast<u_int64>(uPin & 0xFFu) << 1) | (bInput ? 1u : 0u);
	}

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

		char m_acNewVarName[64] = {};
		int m_iNewVarType = 0;

		// Live rects recorded each Render for interaction + test accessors.
		Zenith_HashMap<std::string, PanelRect> m_xPaletteRects;
		Zenith_HashMap<u_int, PanelRect> m_xNodeRects;
		Zenith_HashMap<u_int64, PanelRect> m_xPinRects;
		Zenith_HashMap<std::string, PanelRect> m_xToolbarRects;
		Zenith_HashMap<std::string, PanelRect> m_xPropertyRowRects;
	};

	GraphEditorState g_xGraphEditor;

	//--------------------------------------------------------------------------
	// Helpers
	//--------------------------------------------------------------------------

	Zenith_GraphDefinition* GetOpenDefinition()
	{
		return g_xGraphEditor.m_pxAsset ? &g_xGraphEditor.m_pxAsset->GetDefinition() : nullptr;
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
		if (pxNodeDef->m_xParamBlob.GetCursor() > 0)
		{
			Zenith_DataStream xRead(const_cast<void*>(pxNodeDef->m_xParamBlob.GetData()), pxNodeDef->m_xParamBlob.GetCursor());
			Zenith_PropertySystem::ReadProperties(g_xGraphEditor.m_pxParamInstance, *pxInfo->m_pfnGetPropertyTable(), xRead);
		}
		g_xGraphEditor.m_uParamInstanceNodeID = g_xGraphEditor.m_uSelectedNodeID;
	}

	void OnSelectedNodeParamChanged(void* /*pxUserData*/, const char* /*szPropertyName*/)
	{
		Zenith_GraphDefinition* pxDef = GetOpenDefinition();
		if (pxDef && g_xGraphEditor.m_pxParamInstance && g_xGraphEditor.m_uParamInstanceNodeID != 0)
		{
			pxDef->SetNodeParamsFromInstance(g_xGraphEditor.m_uParamInstanceNodeID, g_xGraphEditor.m_pxParamInstance);
			g_xGraphEditor.m_bDirty = true;
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
				g_xGraphEditor.m_xPaletteRects[xInfo.m_strTypeName] = MakeRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
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

	// Effective exec-pin count for a node def. Variable-pin flow nodes
	// (Switch/StateMachine/Selector) report their configured count through
	// GetDynamicExecOutputCount on a param-applied temp instance; every other
	// type uses the registered static count. Editor-scale cost (one temp
	// instance per dynamic node per query).
	u_int GetNodeExecOutputCount(const Zenith_GraphNodeDef& xNodeDef, const Zenith_GraphNodeTypeInfo* pxInfo)
	{
		if (!pxInfo)
		{
			return 1;
		}
		Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
		if (pxTemp->GetDynamicExecOutputCount() < 0)
		{
			delete pxTemp;
			return pxInfo->m_uExecOutputCount;	// static-pin type
		}
		if (pxInfo->m_pfnGetPropertyTable && xNodeDef.m_xParamBlob.GetCursor() > 0)
		{
			Zenith_DataStream xParamRead(const_cast<void*>(xNodeDef.m_xParamBlob.GetData()), xNodeDef.m_xParamBlob.GetCursor());
			Zenith_PropertySystem::ReadProperties(pxTemp, *pxInfo->m_pfnGetPropertyTable(), xParamRead);
		}
		const int32_t iDynamic = pxTemp->GetDynamicExecOutputCount();
		delete pxTemp;
		return iDynamic < 0 ? pxInfo->m_uExecOutputCount : static_cast<u_int>(iDynamic > 255 ? 255 : iDynamic);
	}

	struct PinPos
	{
		ImVec2 m_xInput;
		Zenith_Vector<ImVec2> m_axOutputs;
	};

	// Pin positions derived from node defs - the single source the edge pass,
	// node pass, and pending-link pass all share.
	void BuildPinPositions(const Zenith_GraphDefinition& xDef, const ImVec2& xOrigin, Zenith_HashMap<u_int, PinPos>& xOut)
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		for (u_int u = 0; u < xDef.GetNodeCount(); ++u)
		{
			const Zenith_GraphNodeDef& xNodeDef = xDef.GetNodeAt(u);
			Zenith_Maths::Vector2 xPos(30.0f, 30.0f);
			xDef.GetNodeEditorPos(xNodeDef.m_uNodeID, xPos);
			const ImVec2 xMin(xOrigin.x + xPos.x, xOrigin.y + xPos.y);

			const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find(xNodeDef.m_strTypeName.c_str());
			const u_int uOutputs = GetNodeExecOutputCount(xNodeDef, pxInfo);

			PinPos xPins;
			xPins.m_xInput = ImVec2(xMin.x, xMin.y + fHEADER_HEIGHT + 10.0f);
			for (u_int uPin = 0; uPin < uOutputs; ++uPin)
			{
				xPins.m_axOutputs.PushBack(ImVec2(xMin.x + fNODE_WIDTH, xMin.y + fHEADER_HEIGHT + 10.0f + static_cast<float>(uPin) * fPIN_SPACING));
			}
			xOut[xNodeDef.m_uNodeID] = xPins;
		}
	}

	void RenderCanvasEdges(ImDrawList* pxDrawList, const Zenith_GraphDefinition& xDef, const Zenith_HashMap<u_int, PinPos>& xPinPositions)
	{
		const ImU32 uEDGE_COLOUR = IM_COL32(200, 200, 120, 255);
		for (u_int u = 0; u < xDef.GetEdgeCount(); ++u)
		{
			const Zenith_GraphEdge& xEdge = xDef.GetEdgeAt(u);
			const PinPos* pxSrc = xPinPositions.TryGet(xEdge.m_uSrcNodeID);
			const PinPos* pxDst = xPinPositions.TryGet(xEdge.m_uDstNodeID);
			if (!pxSrc || !pxDst || xEdge.m_uSrcPin >= pxSrc->m_axOutputs.GetSize())
			{
				continue;
			}
			const ImVec2 xFrom = pxSrc->m_axOutputs.Get(xEdge.m_uSrcPin);
			const ImVec2 xTo = pxDst->m_xInput;
			pxDrawList->AddBezierCubic(xFrom, ImVec2(xFrom.x + 50.0f, xFrom.y), ImVec2(xTo.x - 50.0f, xTo.y), xTo, uEDGE_COLOUR, 2.0f);
		}
	}

	ImU32 NodeHeaderColour(const Zenith_GraphNodeTypeInfo* pxInfo)
	{
		// Events green, flow orange, unresolved red, actions blue.
		if (!pxInfo)
		{
			return IM_COL32(170, 60, 60, 255);
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

	// Pin visuals + interaction for one node (input pin accepts pending links;
	// output pins start links / right-click-disconnect).
	void RenderNodePins(ImDrawList* pxDrawList, Zenith_GraphDefinition& xDef, u_int uNodeID, const PinPos& xPins)
	{
		const ImVec2& xPinCentre = xPins.m_xInput;
		pxDrawList->AddCircleFilled(xPinCentre, fPIN_RADIUS, IM_COL32(220, 220, 220, 255));
		g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, 0, true)] = MakeRect(
			ImVec2(xPinCentre.x - 8.0f, xPinCentre.y - 8.0f), ImVec2(xPinCentre.x + 8.0f, xPinCentre.y + 8.0f));
		ImGui::SetCursorScreenPos(ImVec2(xPinCentre.x - 8.0f, xPinCentre.y - 8.0f));
		ImGui::InvisibleButton("pin_in", ImVec2(16.0f, 16.0f));
		if (g_xGraphEditor.m_bLinking && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			if (xDef.AddEdge(g_xGraphEditor.m_uLinkSrcNodeID, g_xGraphEditor.m_uLinkSrcPin, uNodeID, 0))
			{
				g_xGraphEditor.m_bDirty = true;
			}
			g_xGraphEditor.m_bLinking = false;
		}

		for (u_int uPin = 0; uPin < xPins.m_axOutputs.GetSize(); ++uPin)
		{
			const ImVec2& xOutCentre = xPins.m_axOutputs.Get(uPin);
			pxDrawList->AddCircleFilled(xOutCentre, fPIN_RADIUS, IM_COL32(160, 220, 160, 255));
			g_xGraphEditor.m_xPinRects[MakePinKey(uNodeID, uPin, false)] = MakeRect(
				ImVec2(xOutCentre.x - 8.0f, xOutCentre.y - 8.0f), ImVec2(xOutCentre.x + 8.0f, xOutCentre.y + 8.0f));
			ImGui::PushID(static_cast<int>(uPin));
			ImGui::SetCursorScreenPos(ImVec2(xOutCentre.x - 8.0f, xOutCentre.y - 8.0f));
			ImGui::InvisibleButton("pin_out", ImVec2(16.0f, 16.0f));
			if (ImGui::IsItemActivated())
			{
				g_xGraphEditor.m_bLinking = true;
				g_xGraphEditor.m_uLinkSrcNodeID = uNodeID;
				g_xGraphEditor.m_uLinkSrcPin = uPin;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && xDef.RemoveEdge(uNodeID, uPin))
			{
				g_xGraphEditor.m_bDirty = true;
			}
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
		const u_int uOutputs = GetNodeExecOutputCount(xNodeDef, pxInfo);
		const float fNodeHeight = fHEADER_HEIGHT + 14.0f + static_cast<float>(uOutputs > 0 ? uOutputs - 1 : 0) * fPIN_SPACING + 10.0f;

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

		const PinPos* pxPins = xPinPositions.TryGet(uNodeID);
		if (pxPins)
		{
			RenderNodePins(pxDrawList, xDef, uNodeID, *pxPins);
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
		if (pxSrc && g_xGraphEditor.m_uLinkSrcPin < pxSrc->m_axOutputs.GetSize())
		{
			const ImVec2 xFrom = pxSrc->m_axOutputs.Get(g_xGraphEditor.m_uLinkSrcPin);
			const ImVec2 xTo = ImGui::GetIO().MousePos;
			pxDrawList->AddBezierCubic(xFrom, ImVec2(xFrom.x + 50.0f, xFrom.y), ImVec2(xTo.x - 50.0f, xTo.y), xTo,
				IM_COL32(255, 255, 255, 160), 2.0f);
		}
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
		{
			g_xGraphEditor.m_bLinking = false;
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

	if (g_xGraphEditor.m_bPositionWindowNextRender)
	{
		// Deterministic placement for automated tests + a sane default. Tall
		// enough that the whole left column (properties + variables + palette)
		// renders unclipped - clipped rows are not interactable, which would
		// silently break simulated-input authoring.
		ImGui::SetNextWindowPos(ImVec2(60.0f, 40.0f));
		ImGui::SetNextWindowSize(ImVec2(1100.0f, 980.0f));
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
	ImGui::BeginChild("GraphLeftColumn", ImVec2(280.0f, 0), true);
	RenderSelectedNodeProperties();
	ImGui::Spacing();
	RenderVariables();
	ImGui::Spacing();
	RenderPalette();
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

	Zenith_BehaviourGraphAsset* pxCached = Zenith_AssetRegistry::Get<Zenith_BehaviourGraphAsset>(g_xGraphEditor.m_strAssetPath);
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
	g_xGraphEditor.m_bDirty = false;
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
		Zenith_BehaviourGraphAsset* pxCached = Zenith_AssetRegistry::Get<Zenith_BehaviourGraphAsset>(g_xGraphEditor.m_strAssetPath);
		if (pxCached && pxCached != g_xGraphEditor.m_pxAsset)
		{
			Zenith_DataStream xCopy;
			g_xGraphEditor.m_pxAsset->GetDefinition().WriteToDataStream(xCopy);
			xCopy.SetCursor(0);
			pxCached->GetDefinition().ReadFromDataStream(xCopy);
			delete g_xGraphEditor.m_pxAsset;
			g_xGraphEditor.m_pxAsset = pxCached;
			g_xGraphEditor.m_bOwnsAsset = false;
			DestroyParamInstance();
		}
	}

	// Queue live hot reload (drained at the main loop's safe point).
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
	Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	if (!pxDef)
	{
		return false;
	}
	const u_int uSrcNodeID = ResolveNodeByTypeOccurrence(szSrcTypeName, uSrcOccurrence);
	const u_int uDstNodeID = ResolveNodeByTypeOccurrence(szDstTypeName, uDstOccurrence);
	if (uSrcNodeID == 0 || uDstNodeID == 0)
	{
		return false;
	}
	const Zenith_GraphNodeDef* pxSrcDef = pxDef->FindNodeDef(uSrcNodeID);
	const Zenith_GraphNodeTypeInfo* pxSrcInfo = pxSrcDef
		? Zenith_GraphNodeRegistry::Get().Find(pxSrcDef->m_strTypeName.c_str()) : nullptr;
	const u_int uSrcOutputs = pxSrcDef ? GetNodeExecOutputCount(*pxSrcDef, pxSrcInfo) : 1;
	if (uSrcPin >= uSrcOutputs)
	{
		return false;
	}
	if (!pxDef->AddEdge(uSrcNodeID, uSrcPin, uDstNodeID, 0))
	{
		return false;
	}
	g_xGraphEditor.m_bDirty = true;
	return true;
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
	}
}

//------------------------------------------------------------------------------
// Test accessors
//------------------------------------------------------------------------------

#ifdef ZENITH_TESTING

namespace
{
	bool RectCentre(const PanelRect* pxRect, Zenith_Maths::Vector2& xOut)
	{
		if (!pxRect)
		{
			return false;
		}
		xOut = pxRect->Centre();
		return true;
	}
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
	return RectCentre(g_xGraphEditor.m_xPinRects.TryGet(MakePinKey(uNodeID, uPin, bInputPin)), xOut);
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

#endif // ZENITH_TOOLS
