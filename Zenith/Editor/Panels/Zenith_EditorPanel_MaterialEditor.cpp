#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_MaterialEditor.h"
#include "Flux/Flux_MaterialAsset.h"
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
		Flux_MaterialAsset* pNewMaterial = Flux_MaterialAsset::Create();
		Zenith_Editor::SelectMaterial(pNewMaterial);
		Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Created new material: %s", pNewMaterial->GetName().c_str());
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
			Flux_MaterialAsset* pMaterial = Flux_MaterialAsset::LoadFromFile(strFilePath);
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

	// Display ALL materials (both file-cached and runtime-created)
	Zenith_Vector<Flux_MaterialAsset*> allMaterials;
	Flux_MaterialAsset::GetAllMaterials(allMaterials);

	if (ImGui::CollapsingHeader("All Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Total: %u materials", allMaterials.GetSize());
		ImGui::Separator();

		for (Zenith_Vector<Flux_MaterialAsset*>::Iterator xIt(allMaterials); !xIt.Done(); xIt.Next())
		{
			Flux_MaterialAsset* pMat = xIt.GetData();
			if (pMat)
			{
				bool bIsSelected = (xState.m_pxSelectedMaterial == pMat);
				std::string strDisplayName = pMat->GetName();
				if (pMat->IsDirty())
				{
					strDisplayName += " *";  // Unsaved changes indicator
				}

				// Show file path indicator for saved materials
				if (!pMat->GetAssetPath().empty())
				{
					strDisplayName += " [saved]";
				}

				if (ImGui::Selectable(strDisplayName.c_str(), bIsSelected))
				{
					Zenith_Editor::SelectMaterial(pMat);
				}

				// Tooltip with more details
				if (ImGui::IsItemHovered())
				{
					std::string strTooltip = "Name: " + pMat->GetName();
					if (!pMat->GetAssetPath().empty())
					{
						strTooltip += "\nPath: " + pMat->GetAssetPath();
					}
					else
					{
						strTooltip += "\n(Runtime-created, not saved to file)";
					}
					ImGui::SetTooltip("%s", strTooltip.c_str());
				}
			}
		}

		if (allMaterials.GetSize() == 0)
		{
			ImGui::TextDisabled("No materials loaded");
		}
	}

	ImGui::Separator();

	// Material properties editor
	if (xState.m_pxSelectedMaterial)
	{
		Flux_MaterialAsset* pMat = xState.m_pxSelectedMaterial;

		ImGui::Text("Editing: %s", pMat->GetName().c_str());

		if (!pMat->GetAssetPath().empty())
		{
			ImGui::TextDisabled("Path: %s", pMat->GetAssetPath().c_str());
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

		// Base Color
		Zenith_Maths::Vector4 xBaseColor = pMat->GetBaseColor();
		float fColor[4] = { xBaseColor.x, xBaseColor.y, xBaseColor.z, xBaseColor.w };
		if (ImGui::ColorEdit4("Base Color", fColor))
		{
			pMat->SetBaseColor({ fColor[0], fColor[1], fColor[2], fColor[3] });
		}

		// Metallic
		float fMetallic = pMat->GetMetallic();
		if (ImGui::SliderFloat("Metallic", &fMetallic, 0.0f, 1.0f))
		{
			pMat->SetMetallic(fMetallic);
		}

		// Roughness
		float fRoughness = pMat->GetRoughness();
		if (ImGui::SliderFloat("Roughness", &fRoughness, 0.0f, 1.0f))
		{
			pMat->SetRoughness(fRoughness);
		}

		// Emissive
		Zenith_Maths::Vector3 xEmissive = pMat->GetEmissiveColor();
		float fEmissiveColor[3] = { xEmissive.x, xEmissive.y, xEmissive.z };
		if (ImGui::ColorEdit3("Emissive Color", fEmissiveColor))
		{
			pMat->SetEmissiveColor({ fEmissiveColor[0], fEmissiveColor[1], fEmissiveColor[2] });
		}

		float fEmissiveIntensity = pMat->GetEmissiveIntensity();
		if (ImGui::SliderFloat("Emissive Intensity", &fEmissiveIntensity, 0.0f, 10.0f))
		{
			pMat->SetEmissiveIntensity(fEmissiveIntensity);
		}

		// Transparency
		bool bTransparent = pMat->IsTransparent();
		if (ImGui::Checkbox("Transparent", &bTransparent))
		{
			pMat->SetTransparent(bTransparent);
		}

		if (bTransparent)
		{
			float fAlphaCutoff = pMat->GetAlphaCutoff();
			if (ImGui::SliderFloat("Alpha Cutoff", &fAlphaCutoff, 0.0f, 1.0f))
			{
				pMat->SetAlphaCutoff(fAlphaCutoff);
			}
		}

		ImGui::Separator();
		ImGui::Text("UV Controls");

		// UV Tiling
		Zenith_Maths::Vector2 xTiling = pMat->GetUVTiling();
		float fTiling[2] = { xTiling.x, xTiling.y };
		if (ImGui::DragFloat2("UV Tiling", fTiling, 0.01f, 0.01f, 100.0f))
		{
			pMat->SetUVTiling({ fTiling[0], fTiling[1] });
		}

		// UV Offset
		Zenith_Maths::Vector2 xOffset = pMat->GetUVOffset();
		float fOffset[2] = { xOffset.x, xOffset.y };
		if (ImGui::DragFloat2("UV Offset", fOffset, 0.01f, -100.0f, 100.0f))
		{
			pMat->SetUVOffset({ fOffset[0], fOffset[1] });
		}

		ImGui::Separator();
		ImGui::Text("Rendering Options");

		// Occlusion Strength
		float fOccStrength = pMat->GetOcclusionStrength();
		if (ImGui::SliderFloat("Occlusion Strength", &fOccStrength, 0.0f, 1.0f))
		{
			pMat->SetOcclusionStrength(fOccStrength);
		}

		// Two-Sided
		bool bTwoSided = pMat->IsTwoSided();
		if (ImGui::Checkbox("Two-Sided", &bTwoSided))
		{
			pMat->SetTwoSided(bTwoSided);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Render both sides of polygons (disables backface culling)");
		}

		// Unlit
		bool bUnlit = pMat->IsUnlit();
		if (ImGui::Checkbox("Unlit (No Lighting)", &bUnlit))
		{
			pMat->SetUnlit(bUnlit);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Skip lighting calculations - material will display at full brightness");
		}

		ImGui::Separator();
		ImGui::Text("Textures");

		// Texture slots with drag-drop support
		RenderMaterialTextureSlot("Diffuse", pMat, pMat->GetDiffuseTextureRef().GetPath(),
			[](Flux_MaterialAsset* p, const std::string& s) { TextureRef xRef; xRef.SetFromPath(s); p->SetDiffuseTextureRef(xRef); });
		RenderMaterialTextureSlot("Normal", pMat, pMat->GetNormalTextureRef().GetPath(),
			[](Flux_MaterialAsset* p, const std::string& s) { TextureRef xRef; xRef.SetFromPath(s); p->SetNormalTextureRef(xRef); });
		RenderMaterialTextureSlot("Roughness/Metallic", pMat, pMat->GetRoughnessMetallicTextureRef().GetPath(),
			[](Flux_MaterialAsset* p, const std::string& s) { TextureRef xRef; xRef.SetFromPath(s); p->SetRoughnessMetallicTextureRef(xRef); });
		RenderMaterialTextureSlot("Occlusion", pMat, pMat->GetOcclusionTextureRef().GetPath(),
			[](Flux_MaterialAsset* p, const std::string& s) { TextureRef xRef; xRef.SetFromPath(s); p->SetOcclusionTextureRef(xRef); });
		RenderMaterialTextureSlot("Emissive", pMat, pMat->GetEmissiveTextureRef().GetPath(),
			[](Flux_MaterialAsset* p, const std::string& s) { TextureRef xRef; xRef.SetFromPath(s); p->SetEmissiveTextureRef(xRef); });

		ImGui::Separator();

		// Save button
		if (ImGui::Button("Save Material"))
		{
			if (pMat->GetAssetPath().empty())
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
				if (pMat->SaveToFile(pMat->GetAssetPath()))
				{
					Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Saved material: %s", pMat->GetAssetPath().c_str());
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

		if (ImGui::Button("Reload"))
		{
			pMat->Reload();
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

void Zenith_EditorPanelMaterialEditor::RenderMaterialTextureSlot(const char* szLabel, Flux_MaterialAsset* pMaterial,
	const std::string& strCurrentPath,
	void (*SetPathFunc)(Flux_MaterialAsset*, const std::string&))
{
	ImGui::PushID(szLabel);

	std::string strDisplayName = "(none)";
	if (!strCurrentPath.empty())
	{
		std::filesystem::path xPath(strCurrentPath);
		strDisplayName = xPath.filename().string();
	}

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	// Drop zone button
	ImVec2 xButtonSize(200, 20);
	ImGui::Button(strDisplayName.c_str(), xButtonSize);

	// Drag-drop target
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			SetPathFunc(pMaterial, pFilePayload->m_szFilePath);
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Set %s texture: %s", szLabel, pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	// Tooltip
	if (ImGui::IsItemHovered())
	{
		if (!strCurrentPath.empty())
		{
			ImGui::SetTooltip("Path: %s\nDrop a .ztxtr texture here to change", strCurrentPath.c_str());
		}
		else
		{
			ImGui::SetTooltip("Drop a .ztxtr texture here");
		}
	}

	// Clear button
	if (!strCurrentPath.empty())
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			SetPathFunc(pMaterial, "");
			Zenith_Log(LOG_CATEGORY_EDITOR, "[MaterialEditor] Cleared %s texture", szLabel);
		}
	}

	ImGui::PopID();
}

#endif // ZENITH_TOOLS
