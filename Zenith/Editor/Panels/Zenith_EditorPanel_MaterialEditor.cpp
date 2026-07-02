#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_MaterialEditor.h"
#include "Editor/Zenith_Editor.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/Zenith_DragDropPayloads.h"
#include "../Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/RenderViews/Flux_MaterialPreviewController.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "imgui.h"

#include <filesystem>

// Windows file dialog support
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

static std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt)
{
	char szFilePath[MAX_PATH] = { 0 };
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = szDefaultExt;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn)) return std::string(szFilePath);
	return "";
}

static std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename)
{
	char szFilePath[MAX_PATH] = { 0 };
	if (szDefaultFilename) strncpy_s(szFilePath, sizeof(szFilePath), szDefaultFilename, _TRUNCATE);
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = *szDefaultExt == '.' ? szDefaultExt + 1 : szDefaultExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	if (GetSaveFileNameA(&ofn)) return std::string(szFilePath);
	return "";
}
#endif // _WIN32

//=============================================================================
// Panel state. The "active material" is the editor's current selection
// (Zenith_Editor::GetSelectedMaterial) so Content Browser / Properties panel
// double-clicks still drive this panel. The preview-image ImGui handle and the
// ZENITH_TESTING screen rects are panel-local.
//=============================================================================
namespace
{
	Flux_ImGuiTextureHandle ls_xPreviewImageHandle;
	bool ls_bPreviewHandleRegistered = false;

#ifdef ZENITH_TESTING
	struct ScreenRect { Zenith_Maths::Vector2 m_xMin{ 0,0 }; Zenith_Maths::Vector2 m_xMax{ 0,0 }; bool m_bValid = false; };
	ScreenRect ls_axParamRowRects[MATERIAL_PARAM_COUNT];
	ScreenRect ls_axTextureSlotRects[MATERIAL_TEXTURE_SLOT_COUNT];
	ScreenRect ls_xPreviewImageRect;
	// Toolbar buttons by label (small fixed set).
	struct ToolbarRect { const char* m_szLabel; ScreenRect m_xRect; };
	ToolbarRect ls_axToolbarRects[4] = { {"New",{}}, {"Open",{}}, {"Save",{}}, {"Save As",{}} };

	void RecordItemRect(ScreenRect& xRect)
	{
		ImVec2 xMin = ImGui::GetItemRectMin();
		ImVec2 xMax = ImGui::GetItemRectMax();
		xRect.m_xMin = { xMin.x, xMin.y };
		xRect.m_xMax = { xMax.x, xMax.y };
		xRect.m_bValid = true;
	}
	void RecordToolbarRect(const char* szLabel)
	{
		for (ToolbarRect& xT : ls_axToolbarRects)
		{
			if (strcmp(xT.m_szLabel, szLabel) == 0) { RecordItemRect(xT.m_xRect); return; }
		}
	}
#else
	void RecordParamRow(MaterialParamID) {}
#endif

	Zenith_MaterialAsset* ActiveMaterial()
	{
		return g_xEngine.Editor().GetSelectedMaterial();
	}

	// UE-style pin gating: is this parameter row meaningful for the material's
	// current state?
	bool ParamVisible(Zenith_MaterialAsset& xMat, const Zenith_MaterialParamDesc& xDesc)
	{
		const Zenith_MaterialParams& xP = xMat.GetParams();
		switch (xDesc.m_eVisibility)
		{
			case MATERIAL_PARAM_VISIBLE_MASKED_ONLY:  return xP.m_eBlendMode == MATERIAL_BLEND_MASKED;
			case MATERIAL_PARAM_VISIBLE_LIT_ONLY:     return xP.m_eShadingModel == MATERIAL_SHADING_DEFAULT_LIT;
			case MATERIAL_PARAM_VISIBLE_HEIGHT_TEX:   return static_cast<bool>(xMat.GetTextureHandle(MATERIAL_TEXTURE_HEIGHT));
			case MATERIAL_PARAM_VISIBLE_DETAIL_TEX:   return static_cast<bool>(xMat.GetTextureHandle(MATERIAL_TEXTURE_DETAIL_ALBEDO)) ||
			                                                 static_cast<bool>(xMat.GetTextureHandle(MATERIAL_TEXTURE_DETAIL_NORMAL));
			case MATERIAL_PARAM_VISIBLE_CLEARCOAT_ON: return xP.m_fClearCoatStrength > 0.0f;
			case MATERIAL_PARAM_VISIBLE_ALWAYS:
			default:                                  return true;
		}
	}

	// Render the override checkbox + reset arrow that precede each row when the
	// material is an instance. Returns the cursor to the same line for the value
	// widget. Returns true if the value widget should be enabled (it always is;
	// non-overridden rows show the inherited value but editing auto-overrides).
	void RenderOverrideDecoration(Zenith_MaterialAsset& xMat, MaterialParamID eID, bool bHasParent)
	{
		if (!bHasParent) return;
		ImGui::PushID(static_cast<int>(eID) + 5000);
		bool bOverridden = xMat.HasParamOverride(eID);
		if (ImGui::Checkbox("##ovr", &bOverridden))
		{
			xMat.SetOverride(eID, bOverridden);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Override this parameter (else inherit from parent)");
		ImGui::SameLine();
		ImGui::PopID();
	}

	// Render one parameter row using the widget appropriate for its type, with
	// the table's range. Records the row's screen rect for tests.
	void RenderParamRow(Zenith_MaterialAsset& xMat, const Zenith_MaterialParamDesc& xDesc, bool bHasParent)
	{
		ImGui::PushID(xDesc.m_szName);
		RenderOverrideDecoration(xMat, xDesc.m_eID, bHasParent);

		switch (xDesc.m_eType)
		{
			case MATERIAL_PARAM_TYPE_FLOAT:
			{
				float f = Zenith_MaterialParamTable::GetParamFloat(xMat.GetParams(), xDesc.m_eID);
				if (ImGui::SliderFloat(xDesc.m_szDisplayName, &f, xDesc.m_fMin, xDesc.m_fMax))
				{
					Zenith_MaterialParamTable::SetParamFloat(xMat.ModifyParams(), xDesc.m_eID, f);
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			case MATERIAL_PARAM_TYPE_FLOAT2:
			{
				Zenith_Maths::Vector4 x = Zenith_MaterialParamTable::GetParamVector(xMat.GetParams(), xDesc.m_eID);
				float af[2] = { x.x, x.y };
				if (ImGui::DragFloat2(xDesc.m_szDisplayName, af, 0.01f, xDesc.m_fMin, xDesc.m_fMax))
				{
					Zenith_MaterialParamTable::SetParamVector(xMat.ModifyParams(), xDesc.m_eID, Zenith_Maths::Vector4(af[0], af[1], 0.0f, 0.0f));
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			case MATERIAL_PARAM_TYPE_COLOR3_HDR:
			{
				Zenith_Maths::Vector4 x = Zenith_MaterialParamTable::GetParamVector(xMat.GetParams(), xDesc.m_eID);
				float af[3] = { x.x, x.y, x.z };
				if (ImGui::ColorEdit3(xDesc.m_szDisplayName, af, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float))
				{
					Zenith_MaterialParamTable::SetParamVector(xMat.ModifyParams(), xDesc.m_eID, Zenith_Maths::Vector4(af[0], af[1], af[2], 0.0f));
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			case MATERIAL_PARAM_TYPE_COLOR4:
			{
				Zenith_Maths::Vector4 x = Zenith_MaterialParamTable::GetParamVector(xMat.GetParams(), xDesc.m_eID);
				float af[4] = { x.x, x.y, x.z, x.w };
				if (ImGui::ColorEdit4(xDesc.m_szDisplayName, af))
				{
					Zenith_MaterialParamTable::SetParamVector(xMat.ModifyParams(), xDesc.m_eID, Zenith_Maths::Vector4(af[0], af[1], af[2], af[3]));
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			case MATERIAL_PARAM_TYPE_BOOL:
			{
				bool b = Zenith_MaterialParamTable::GetParamInt(xMat.GetParams(), xDesc.m_eID) != 0;
				if (ImGui::Checkbox(xDesc.m_szDisplayName, &b))
				{
					Zenith_MaterialParamTable::SetParamInt(xMat.ModifyParams(), xDesc.m_eID, b ? 1u : 0u);
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			case MATERIAL_PARAM_TYPE_ENUM:
			{
				int iValue = static_cast<int>(Zenith_MaterialParamTable::GetParamInt(xMat.GetParams(), xDesc.m_eID));
				if (ImGui::Combo(xDesc.m_szDisplayName, &iValue, xDesc.m_aszEnumLabels, static_cast<int>(xDesc.m_uNumEnumLabels)))
				{
					Zenith_MaterialParamTable::SetParamInt(xMat.ModifyParams(), xDesc.m_eID, static_cast<u_int>(iValue));
					if (bHasParent) xMat.SetOverride(xDesc.m_eID, true);
				}
				break;
			}
			default: break;
		}

#ifdef ZENITH_TESTING
		RecordItemRect(ls_axParamRowRects[xDesc.m_eID]);
#endif
		ImGui::PopID();
	}

	void RenderTextureSlotRow(Zenith_MaterialAsset& xMat, const Zenith_MaterialTextureSlotDesc& xSlot, bool bHasParent)
	{
		ImGui::PushID(xSlot.m_szName);

		const TextureHandle& xHandle = xMat.GetTextureHandle(xSlot.m_eSlot);
		Zenith_TextureAsset* pxTex = xMat.GetTexture(xSlot.m_eSlot);

		// Thumbnail.
		Flux_ImGuiTextureHandle xThumb = g_xEngine.EditorMaterialUI().GetOrCreateTexturePreviewHandle(pxTex);
		if (xThumb.IsValid())
		{
			ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xThumb), ImVec2(48, 48));
		}
		else
		{
			ImGui::Button("...", ImVec2(48, 48));
		}
#ifdef ZENITH_TESTING
		RecordItemRect(ls_axTextureSlotRects[xSlot.m_eSlot]);
#endif

		// Drag-drop target on the thumbnail.
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
			{
				const DragDropFilePayload* pFile = static_cast<const DragDropFilePayload*>(pPayload->Data);
				xMat.SetTexture(xSlot.m_eSlot, TextureHandle(pFile->m_szFilePath));
				if (bHasParent) xMat.SetOverride(uMATERIAL_TEXTURE_OVERRIDE_BIT_BASE + xSlot.m_eSlot, true);
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::Text("%s", xSlot.m_szDisplayName);
		ImGui::TextDisabled("%s", xSlot.m_szChannelHint);
		const std::string& strPath = xHandle.GetPath();
		if (!strPath.empty())
		{
			std::filesystem::path xP(strPath);
			ImGui::TextDisabled("%s", xP.filename().string().c_str());

			// [Normal]-style validation: a slot named *Normal* should carry a
			// normal-map-looking texture.
			if ((xSlot.m_eSlot == MATERIAL_TEXTURE_NORMAL || xSlot.m_eSlot == MATERIAL_TEXTURE_DETAIL_NORMAL))
			{
				std::string strLower = strPath;
				for (char& c : strLower) c = static_cast<char>(tolower(c));
				if (strLower.find("normal") == std::string::npos && strLower.find("_n.") == std::string::npos)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
					ImGui::TextWrapped("! Expected a normal map (name lacks 'normal'/'_n')");
					ImGui::PopStyleColor();
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear"))
			{
				xMat.SetTexture(xSlot.m_eSlot, TextureHandle());
			}
		}
		else
		{
			ImGui::TextDisabled("(drop a .ztxtr here)");
		}
		ImGui::EndGroup();

		ImGui::PopID();
		ImGui::Separator();
	}
}

//=============================================================================
// Render — split into per-section file-static helpers so the orchestrator stays
// flat (the param-group double-loop + four-button toolbar + #ifdef'd test-rect
// recording otherwise push Render's cognitive complexity past the gate).
//=============================================================================
namespace
{
	// Pop the panel to the front of its dock-tab group when the selected
	// material changes (UE-style: selecting a material focuses its editor).
	// Uses the BY-NAME SetWindowFocus (the form the editor's own default-tab
	// fronting at Zenith_Editor.cpp uses) — SetNextWindowFocus-before-Begin
	// does not reliably select a docked tab here. A short countdown re-fronts
	// for a few frames so a competing one-shot focus can't leave it behind.
	void MaybeFrontMaterialEditor()
	{
		static Zenith_MaterialAsset* ls_pxLastSeen = nullptr;
		static u_int ls_uFocusCountdown = 0;
		Zenith_MaterialAsset* pxCurrent = ActiveMaterial();
		if (pxCurrent != nullptr && pxCurrent != ls_pxLastSeen) ls_uFocusCountdown = 4;
		ls_pxLastSeen = pxCurrent;
		if (ls_uFocusCountdown > 0)
		{
			ImGui::SetWindowFocus(szEDITOR_WINDOW_MATERIAL_EDITOR);
			--ls_uFocusCountdown;
		}
	}

	// Save / Save As share this: Save As always opens a dialog; Save opens one
	// only when the material has no on-disk path yet (else saves in place).
	void SaveButton(const char* szLabel, Zenith_MaterialAsset* pxMat, bool bForceDialog)
	{
		const bool bClicked = ImGui::Button(szLabel) && pxMat;
#ifdef ZENITH_TESTING
		RecordToolbarRect(szLabel);
#endif
		if (!bClicked) return;
#ifdef _WIN32
		if (bForceDialog || pxMat->GetPath().empty() || pxMat->IsProcedural())
		{
			std::string strPath = ShowSaveFileDialog(
				"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0*" ZENITH_MATERIAL_EXT "\0All Files (*.*)\0*.*\0",
				ZENITH_MATERIAL_EXT, (pxMat->GetName() + ZENITH_MATERIAL_EXT).c_str());
			if (!strPath.empty()) Zenith_MaterialEditorPanel::Action_SaveMaterial(strPath.c_str());
		}
		else
		{
			Zenith_MaterialEditorPanel::Action_SaveMaterial(nullptr);
		}
#else
		(void)bForceDialog;
#endif
	}

	void RenderToolbar()
	{
		if (ImGui::Button("New")) Zenith_MaterialEditorPanel::Action_CreateMaterial("");
#ifdef ZENITH_TESTING
		RecordToolbarRect("New");
#endif
		ImGui::SameLine();
		if (ImGui::Button("Open"))
		{
#ifdef _WIN32
			std::string strPath = ShowOpenFileDialog(
				"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0*" ZENITH_MATERIAL_EXT "\0All Files (*.*)\0*.*\0",
				ZENITH_MATERIAL_EXT);
			if (!strPath.empty()) Zenith_MaterialEditorPanel::Action_OpenMaterial(strPath.c_str());
#endif
		}
#ifdef ZENITH_TESTING
		RecordToolbarRect("Open");
#endif
		Zenith_MaterialAsset* pxMat = ActiveMaterial();
		ImGui::SameLine();
		SaveButton("Save", pxMat, /*bForceDialog*/ false);
		ImGui::SameLine();
		SaveButton("Save As", pxMat, /*bForceDialog*/ true);
	}

	// Live IBL preview image + orbit/light/zoom input + the mesh selector.
	void RenderPreviewPane(Flux_MaterialPreviewImpl& xPreview, Zenith_MaterialAsset* pxMat)
	{
		xPreview.SetActive(true);
		xPreview.SetMaterial(pxMat);

		if (!ls_bPreviewHandleRegistered)
		{
			ls_xPreviewImageHandle = Flux_ImGuiIntegration::RegisterTexture(
				xPreview.GetPreviewSRV(), g_xEngine.FluxGraphics().m_xClampSampler);
			ls_bPreviewHandleRegistered = true;
		}

		const float fPreviewSize = 256.0f;
		if (ls_xPreviewImageHandle.IsValid())
		{
			ImGui::Image((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(ls_xPreviewImageHandle), ImVec2(fPreviewSize, fPreviewSize));
#ifdef ZENITH_TESTING
			RecordItemRect(ls_xPreviewImageRect);
#endif
			// Orbit (LMB-drag), light (L+drag, UE convention), zoom (wheel).
			if (ImGui::IsItemHovered())
			{
				const ImGuiIO& xIO = ImGui::GetIO();
				if (xIO.MouseWheel != 0.0f) xPreview.ZoomCamera(xIO.MouseWheel * 0.2f);
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
				{
					const float fDX = xIO.MouseDelta.x * 0.01f;
					const float fDY = xIO.MouseDelta.y * 0.01f;
					if (ImGui::IsKeyDown(ImGuiKey_L)) xPreview.OrbitLight(-fDX, -fDY);
					else                              xPreview.OrbitCamera(-fDX, -fDY);
				}
			}
		}

		static const char* const s_aszMeshNames[] = { "Sphere", "Cube", "Plane", "Cylinder" };
		int iMesh = static_cast<int>(xPreview.GetPreviewMesh());
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::Combo("Preview Mesh", &iMesh, s_aszMeshNames, MATERIAL_PREVIEW_MESH_COUNT))
			xPreview.SetPreviewMesh(static_cast<MaterialPreviewMesh>(iMesh));
		ImGui::SameLine();
		ImGui::TextDisabled("(drag = orbit, L+drag = light, wheel = zoom)");
	}

	// Name field + parent (instance-authoring) slot with drag-drop.
	void RenderHeaderAndParent(Zenith_MaterialAsset* pxMat)
	{
		char szName[256];
		strncpy_s(szName, sizeof(szName), pxMat->GetName().c_str(), _TRUNCATE);
		if (ImGui::InputText("Name", szName, sizeof(szName))) pxMat->SetName(szName);
		if (!pxMat->GetPath().empty() && !pxMat->IsProcedural())
			ImGui::TextDisabled("Path: %s", pxMat->GetPath().c_str());
		else
			ImGui::TextDisabled("(unsaved)");

		const bool bHasParent = pxMat->HasParent();
		ImGui::Text("Parent:");
		ImGui::SameLine();
		ImGui::Button(bHasParent ? pxMat->GetParentHandle().GetPath().c_str() : "(none — drop a material to make an instance)", ImVec2(280, 0));
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MATERIAL))
			{
				const DragDropFilePayload* pFile = static_cast<const DragDropFilePayload*>(pPayload->Data);
				Zenith_MaterialEditorPanel::Action_SetParent(pFile->m_szFilePath);
			}
			ImGui::EndDragDropTarget();
		}
		if (bHasParent)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear Parent")) pxMat->ClearParent();
		}
	}

	// Auto-generated grouped property foldouts (pin-gated visibility).
	void RenderParamGroups(Zenith_MaterialAsset* pxMat, bool bHasParent)
	{
		const u_int uParamCount = Zenith_MaterialParamTable::GetParamCount();
		for (u_int uGroup = 0; uGroup < MATERIAL_GROUP_COUNT; uGroup++)
		{
			const MaterialParamGroup eGroup = static_cast<MaterialParamGroup>(uGroup);

			bool bAnyVisible = false;
			for (u_int u = 0; u < uParamCount; u++)
			{
				const Zenith_MaterialParamDesc& xDesc = Zenith_MaterialParamTable::GetParamDesc(static_cast<MaterialParamID>(u));
				if (xDesc.m_eGroup == eGroup && ParamVisible(*pxMat, xDesc)) { bAnyVisible = true; break; }
			}
			if (!bAnyVisible) continue;

			if (ImGui::CollapsingHeader(Zenith_MaterialParamTable::GetGroupDisplayName(eGroup), ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (u_int u = 0; u < uParamCount; u++)
				{
					const Zenith_MaterialParamDesc& xDesc = Zenith_MaterialParamTable::GetParamDesc(static_cast<MaterialParamID>(u));
					if (xDesc.m_eGroup != eGroup) continue;
					if (!ParamVisible(*pxMat, xDesc)) continue;
					RenderParamRow(*pxMat, xDesc, bHasParent);
				}
			}
		}
	}

	void RenderTextureSlots(Zenith_MaterialAsset* pxMat, bool bHasParent)
	{
		if (!ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) return;
		const u_int uSlotCount = Zenith_MaterialParamTable::GetTextureSlotCount();
		for (u_int u = 0; u < uSlotCount; u++)
		{
			const Zenith_MaterialTextureSlotDesc& xSlot = Zenith_MaterialParamTable::GetTextureSlotDesc(static_cast<MaterialTextureSlot>(u));
			RenderTextureSlotRow(*pxMat, xSlot, bHasParent);
		}
	}

	void RenderStatsLine(Zenith_MaterialAsset* pxMat, bool bHasParent)
	{
		const Zenith_MaterialResolved& xResolved = pxMat->GetResolved();
		u_int uBoundTextures = 0;
		for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
			if (static_cast<bool>(*xResolved.m_apxTextures[u])) uBoundTextures++;
		static const char* const s_aszBlend[] = { "Opaque", "Masked", "Translucent", "Additive" };
		static const char* const s_aszShading[] = { "Default Lit", "Unlit" };
		ImGui::Separator();
		ImGui::TextDisabled("Blend: %s | Shading: %s | Textures bound: %u/%u%s",
			s_aszBlend[xResolved.m_xParams.m_eBlendMode],
			s_aszShading[xResolved.m_xParams.m_eShadingModel],
			uBoundTextures, static_cast<u_int>(MATERIAL_TEXTURE_SLOT_COUNT),
			bHasParent ? " | INSTANCE" : "");
	}
}

void Zenith_MaterialEditorPanel::Render()
{
	bool& bShow = g_xEngine.Editor().GetMaterialEditorShowFlag();
	Flux_MaterialPreviewImpl& xPreview = g_xEngine.MaterialPreview();

	if (!bShow)
	{
		xPreview.SetActive(false);
		return;
	}

	MaybeFrontMaterialEditor();

	if (!ImGui::Begin(szEDITOR_WINDOW_MATERIAL_EDITOR, &bShow))
	{
		// Collapsed window or an unfocused dock tab: ImGui renders no content, so
		// don't refresh the preview's liveness either — the full-pipeline preview
		// view (its ~40 passes + 512² target chain) tears down after the grace
		// window instead of rendering an invisible image forever.
		ImGui::End();
		return;
	}

	RenderToolbar();
	ImGui::Separator();

	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	if (!pxMat)
	{
		ImGui::TextDisabled("No material selected.");
		ImGui::TextDisabled("Create a new material or load one from the Content Browser.");
		xPreview.SetActive(false);
		ImGui::End();
		return;
	}

	RenderPreviewPane(xPreview, pxMat);
	ImGui::Separator();
	RenderHeaderAndParent(pxMat);
	const bool bHasParent = pxMat->HasParent();
	ImGui::Separator();
	RenderParamGroups(pxMat, bHasParent);
	RenderTextureSlots(pxMat, bHasParent);
	RenderStatsLine(pxMat, bHasParent);

	ImGui::End();
}

//=============================================================================
// Atomic actions
//=============================================================================
bool Zenith_MaterialEditorPanel::Action_CreateMaterial(const char* szAssetPath)
{
	Zenith_MaterialAsset* pxMat = (szAssetPath && szAssetPath[0])
		? Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(Zenith_AssetRegistry::NormalizeAssetPath(szAssetPath))
		: Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	if (!pxMat) return false;
	g_xEngine.Editor().SelectMaterial(pxMat);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Created material: %s", pxMat->GetName().c_str());
	return true;
}

bool Zenith_MaterialEditorPanel::Action_OpenMaterial(const char* szAssetPath)
{
	if (!szAssetPath || !szAssetPath[0]) return false;
	Zenith_MaterialAsset* pxMat = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(szAssetPath);
	if (!pxMat) { Zenith_Error(LOG_CATEGORY_EDITOR, "[MaterialEditor] Failed to open: %s", szAssetPath); return false; }
	g_xEngine.Editor().SelectMaterial(pxMat);
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SaveMaterial(const char* szAssetPath)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	if (!pxMat) return false;
	const std::string strPath = (szAssetPath && szAssetPath[0]) ? std::string(szAssetPath) : pxMat->GetPath();
	if (strPath.empty()) return false;

	// Materials use their own .zmtrl format via SaveToFile (NOT the registry's
	// ZDATA envelope), so resolve the prefixed path to an absolute one and
	// ensure the parent directory exists before writing.
	const std::string strAbsolute = Zenith_AssetRegistry::ResolvePath(strPath);
	std::error_code xEC;
	std::filesystem::create_directories(std::filesystem::path(strAbsolute).parent_path(), xEC);
	return pxMat->SaveToFile(strAbsolute);
}

bool Zenith_MaterialEditorPanel::Action_SetParamFloat(const char* szParamName, float fValue)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName(szParamName);
	if (!pxMat || !pxDesc || pxDesc->m_eType != MATERIAL_PARAM_TYPE_FLOAT) return false;
	Zenith_MaterialParamTable::SetParamFloat(pxMat->ModifyParams(), pxDesc->m_eID, fValue);
	if (pxMat->HasParent()) pxMat->SetOverride(pxDesc->m_eID, true);
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetParamColor(const char* szParamName, float fR, float fG, float fB, float fA)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName(szParamName);
	if (!pxMat || !pxDesc) return false;
	if (pxDesc->m_eType != MATERIAL_PARAM_TYPE_COLOR3_HDR && pxDesc->m_eType != MATERIAL_PARAM_TYPE_COLOR4 && pxDesc->m_eType != MATERIAL_PARAM_TYPE_FLOAT2) return false;
	Zenith_MaterialParamTable::SetParamVector(pxMat->ModifyParams(), pxDesc->m_eID, Zenith_Maths::Vector4(fR, fG, fB, fA));
	if (pxMat->HasParent()) pxMat->SetOverride(pxDesc->m_eID, true);
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetParamInt(const char* szParamName, int iValue)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName(szParamName);
	if (!pxMat || !pxDesc) return false;
	if (pxDesc->m_eType != MATERIAL_PARAM_TYPE_ENUM && pxDesc->m_eType != MATERIAL_PARAM_TYPE_BOOL) return false;
	Zenith_MaterialParamTable::SetParamInt(pxMat->ModifyParams(), pxDesc->m_eID, static_cast<u_int>(iValue));
	if (pxMat->HasParent()) pxMat->SetOverride(pxDesc->m_eID, true);
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetBlendMode(int iBlendMode)
{
	return Action_SetParamInt("BlendMode", iBlendMode);
}

bool Zenith_MaterialEditorPanel::Action_SetTexture(const char* szSlotName, const char* szTexturePath)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	const Zenith_MaterialTextureSlotDesc* pxSlot = Zenith_MaterialParamTable::FindTextureSlotByName(szSlotName);
	if (!pxMat || !pxSlot) return false;
	pxMat->SetTexture(pxSlot->m_eSlot, (szTexturePath && szTexturePath[0]) ? TextureHandle(szTexturePath) : TextureHandle());
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetParent(const char* szParentAssetPath)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	if (!pxMat) return false;
	if (!szParentAssetPath || !szParentAssetPath[0]) { pxMat->ClearParent(); return true; }
	return pxMat->SetParent(MaterialHandle(szParentAssetPath));
}

bool Zenith_MaterialEditorPanel::Action_SetOverride(const char* szParamName, bool bOverridden)
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName(szParamName);
	if (!pxMat || !pxDesc) return false;
	pxMat->SetOverride(pxDesc->m_eID, bOverridden);
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetPreviewMesh(int iMesh)
{
	if (iMesh < 0 || iMesh >= MATERIAL_PREVIEW_MESH_COUNT) return false;
	g_xEngine.MaterialPreview().SetPreviewMesh(static_cast<MaterialPreviewMesh>(iMesh));
	return true;
}

bool Zenith_MaterialEditorPanel::Action_SetPreviewLight(float fYaw, float fPitch)
{
	g_xEngine.MaterialPreview().SetLightAngles(fYaw, fPitch);
	return true;
}

#ifdef ZENITH_TESTING
bool Zenith_MaterialEditorPanel::GetParamRowScreenRect(const char* szParamName, Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax)
{
	const Zenith_MaterialParamDesc* pxDesc = Zenith_MaterialParamTable::FindParamByName(szParamName);
	if (!pxDesc) return false;
	const ScreenRect& xR = ls_axParamRowRects[pxDesc->m_eID];
	if (!xR.m_bValid) return false;
	xOutMin = xR.m_xMin; xOutMax = xR.m_xMax;
	return true;
}

bool Zenith_MaterialEditorPanel::GetTextureSlotScreenPos(const char* szSlotName, Zenith_Maths::Vector2& xOut)
{
	const Zenith_MaterialTextureSlotDesc* pxSlot = Zenith_MaterialParamTable::FindTextureSlotByName(szSlotName);
	if (!pxSlot) return false;
	const ScreenRect& xR = ls_axTextureSlotRects[pxSlot->m_eSlot];
	if (!xR.m_bValid) return false;
	xOut = { (xR.m_xMin.x + xR.m_xMax.x) * 0.5f, (xR.m_xMin.y + xR.m_xMax.y) * 0.5f };
	return true;
}

bool Zenith_MaterialEditorPanel::GetToolbarButtonScreenPos(const char* szLabel, Zenith_Maths::Vector2& xOut)
{
	for (const ToolbarRect& xT : ls_axToolbarRects)
	{
		if (strcmp(xT.m_szLabel, szLabel) == 0 && xT.m_xRect.m_bValid)
		{
			xOut = { (xT.m_xRect.m_xMin.x + xT.m_xRect.m_xMax.x) * 0.5f, (xT.m_xRect.m_xMin.y + xT.m_xRect.m_xMax.y) * 0.5f };
			return true;
		}
	}
	return false;
}

bool Zenith_MaterialEditorPanel::GetPreviewImageScreenRect(Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax)
{
	if (!ls_xPreviewImageRect.m_bValid) return false;
	xOutMin = ls_xPreviewImageRect.m_xMin; xOutMax = ls_xPreviewImageRect.m_xMax;
	return true;
}

bool Zenith_MaterialEditorPanel::IsOpen()
{
	return g_xEngine.Editor().GetMaterialEditorShowFlag() && ActiveMaterial() != nullptr;
}

const char* Zenith_MaterialEditorPanel::GetOpenMaterialPath()
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	return pxMat ? pxMat->GetPath().c_str() : "";
}

bool Zenith_MaterialEditorPanel::IsDirty()
{
	Zenith_MaterialAsset* pxMat = ActiveMaterial();
	return pxMat && pxMat->IsDirty();
}
#endif // ZENITH_TESTING

#endif // ZENITH_TOOLS
