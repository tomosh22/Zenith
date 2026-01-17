#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_MaterialEditor.h"
#include "../Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Collections/Zenith_Vector.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <filesystem>

// Windows file dialog support
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

static std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt)
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = szDefaultExt;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

static std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename)
{
	char szFilePath[MAX_PATH] = { 0 };
	if (szDefaultFilename)
	{
		strncpy(szFilePath, szDefaultFilename, MAX_PATH - 1);
	}

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = *szDefaultExt == '.' ? szDefaultExt+1 : szDefaultExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}
#endif // _WIN32

void Zenith_EditorPanelMaterialEditor::Render(MaterialEditorState& xState)
{
	if (!xState.m_bShowMaterialEditor)
		return;

	ImGui::Begin("Material Editor", &xState.m_bShowMaterialEditor);

	// Create New Material button
	if (ImGui::Button("Create New Material"))
	{
		Zenith_MaterialAsset* pNewMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
		if (pNewMaterial)
		{
			Zenith_Editor::SelectMaterial(pNewMaterial);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Created new material: %s", pNewMaterial->GetName().c_str());
		}
	}

	ImGui::SameLine();

	// Load Material button
	if (ImGui::Button("Load Material"))
	{
#ifdef _WIN32
		std::string strFilePath = ShowOpenFileDialog(
			"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
			ZENITH_MATERIAL_EXT);
		if (!strFilePath.empty())
		{
			Zenith_MaterialAsset* pMaterial = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(strFilePath);
			if (pMaterial)
			{
				Zenith_Editor::SelectMaterial(pMaterial);
				Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Loaded material: %s", strFilePath.c_str());
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] ERROR: Failed to load material: %s", strFilePath.c_str());
			}
		}
#endif
	}

	ImGui::Separator();

	// TODO: Re-implement material list using Zenith_AssetRegistry API when needed
	// The registry currently doesn't expose GetAllOfType<T>() method
	if (ImGui::CollapsingHeader("Loaded Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Use Content Browser to select materials");
		ImGui::TextDisabled("Or use Create New/Load buttons above");
	}

	ImGui::Separator();

	// Material properties editor
	if (xState.m_pxSelectedMaterial)
	{
		Zenith_MaterialAsset* pMat = xState.m_pxSelectedMaterial;

		ImGui::Text("Editing: %s", pMat->GetName().c_str());

		if (!pMat->GetPath().empty() && !pMat->IsProcedural())
		{
			ImGui::TextDisabled("Path: %s", pMat->GetPath().c_str());
		}
		else
		{
			ImGui::TextDisabled("(Unsaved)");
		}

		ImGui::Separator();

		// Name
		char szNameBuffer[256];
		strncpy(szNameBuffer, pMat->GetName().c_str(), sizeof(szNameBuffer));
		szNameBuffer[sizeof(szNameBuffer) - 1] = '\0';
		if (ImGui::InputText("Name", szNameBuffer, sizeof(szNameBuffer)))
		{
			pMat->SetName(szNameBuffer);
		}

		ImGui::Separator();
		ImGui::Text("Material Properties");

		// Use shared utility for material properties
		Zenith_Editor_MaterialUI::RenderMaterialProperties(pMat, "MaterialEditor");

		ImGui::Separator();
		ImGui::Text("Textures");

		// Use shared utility for texture slots (no preview in this panel for cleaner look)
		Zenith_Editor_MaterialUI::RenderAllTextureSlots(*pMat, false);

		ImGui::Separator();

		// Save button
		if (ImGui::Button("Save Material"))
		{
			if (pMat->GetPath().empty() || pMat->IsProcedural())
			{
				// Show save dialog for new material
#ifdef _WIN32
				std::string strFilePath = ShowSaveFileDialog(
					"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
					ZENITH_MATERIAL_EXT,
					(pMat->GetName() + ZENITH_MATERIAL_EXT).c_str());
				if (!strFilePath.empty())
				{
					if (pMat->SaveToFile(strFilePath))
					{
						Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material to: %s", strFilePath.c_str());
					}
				}
#endif
			}
			else
			{
				// Save to existing path
				if (pMat->SaveToFile(pMat->GetPath()))
				{
					Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material: %s", pMat->GetPath().c_str());
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("Save As..."))
		{
#ifdef _WIN32
			std::string strFilePath = ShowSaveFileDialog(
				"Zenith Material Files (*" ZENITH_MATERIAL_EXT ")\0 * " ZENITH_MATERIAL_EXT "\0All Files (*.*)\0 * .*\0",
				ZENITH_MATERIAL_EXT,
				(pMat->GetName() + ZENITH_MATERIAL_EXT).c_str());
			if (!strFilePath.empty())
			{
				if (pMat->SaveToFile(strFilePath))
				{
					Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material to: %s", strFilePath.c_str());
				}
			}
#endif
		}

		ImGui::SameLine();

		if (ImGui::Button("Reload") && !pMat->GetPath().empty() && !pMat->IsProcedural())
		{
			pMat->LoadFromFile(pMat->GetPath());
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Reloaded material: %s", pMat->GetName().c_str());
		}
	}
	else
	{
		ImGui::TextDisabled("No material selected");
		ImGui::TextDisabled("Create a new material or load an existing one");
	}

	ImGui::End();
}

#endif // ZENITH_TOOLS
