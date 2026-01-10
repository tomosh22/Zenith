#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_ContentBrowser.h"
#include "Flux/Flux_MaterialAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <filesystem>
#include <algorithm>

void Zenith_EditorPanelContentBrowser::Render(ContentBrowserState& xState)
{
	ImGui::Begin("Content Browser");

	// Refresh directory contents if needed
	if (xState.m_bDirectoryNeedsRefresh)
	{
		RefreshDirectoryContents(xState);
		xState.m_bDirectoryNeedsRefresh = false;
	}

	// Navigation buttons
	if (ImGui::Button("<- Back"))
	{
		NavigateToParent(xState);
	}
	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		xState.m_bDirectoryNeedsRefresh = true;
	}
	ImGui::SameLine();

	// Display current path (truncated if too long)
	std::string strDisplayPath = xState.m_strCurrentDirectory;
	if (strDisplayPath.length() > 50)
	{
		strDisplayPath = "..." + strDisplayPath.substr(strDisplayPath.length() - 47);
	}
	ImGui::Text("Path: %s", strDisplayPath.c_str());

	// Search and Filter bar
	ImGui::Separator();

	// Search input
	ImGui::SetNextItemWidth(200.0f);
	bool bSearchChanged = ImGui::InputTextWithHint("##Search", "Search...", xState.m_szSearchBuffer, xState.m_uSearchBufferSize);

	ImGui::SameLine();

	// Asset type filter dropdown
	const char* aszFilterTypes[] = { "All Types", "Textures", "Materials", "Meshes", "Models", "Prefabs", "Scenes", "Animations" };
	ImGui::SetNextItemWidth(120.0f);
	bool bFilterChanged = ImGui::Combo("##TypeFilter", &xState.m_iAssetTypeFilter, aszFilterTypes, IM_ARRAYSIZE(aszFilterTypes));

	// Apply filtering
	if (bSearchChanged || bFilterChanged || xState.m_xFilteredContents.empty())
	{
		xState.m_xFilteredContents.clear();
		std::string strSearch(xState.m_szSearchBuffer);

		// Convert search to lowercase for case-insensitive matching
		std::transform(strSearch.begin(), strSearch.end(), strSearch.begin(), ::tolower);

		for (const auto& xEntry : xState.m_xDirectoryContents)
		{
			// Search filter
			if (!strSearch.empty())
			{
				std::string strNameLower = xEntry.m_strName;
				std::transform(strNameLower.begin(), strNameLower.end(), strNameLower.begin(), ::tolower);
				if (strNameLower.find(strSearch) == std::string::npos)
				{
					continue;
				}
			}

			// Type filter (directories always pass)
			if (xState.m_iAssetTypeFilter > 0 && !xEntry.m_bIsDirectory)
			{
				bool bPassFilter = false;
				switch (xState.m_iAssetTypeFilter)
				{
				case 1: bPassFilter = (xEntry.m_strExtension == ZENITH_TEXTURE_EXT); break;
				case 2: bPassFilter = (xEntry.m_strExtension == ZENITH_MATERIAL_EXT); break;
				case 3: bPassFilter = (xEntry.m_strExtension == ZENITH_MESH_EXT); break;
				case 4: bPassFilter = (xEntry.m_strExtension == ZENITH_MODEL_EXT); break;
				case 5: bPassFilter = (xEntry.m_strExtension == ZENITH_PREFAB_EXT); break;
				case 6: bPassFilter = (xEntry.m_strExtension == ".zscn"); break;
				case 7: bPassFilter = (xEntry.m_strExtension == ZENITH_ANIMATION_EXT); break;
				}
				if (!bPassFilter)
				{
					continue;
				}
			}

			xState.m_xFilteredContents.push_back(xEntry);
		}
	}

	ImGui::Separator();

	// Context menu for creating new assets (right-click on empty area)
	if (ImGui::BeginPopupContextWindow("ContentBrowserContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Folder"))
			{
				// Create new folder
				std::string strNewFolder = xState.m_strCurrentDirectory + "/NewFolder";
				int iCounter = 1;
				while (std::filesystem::exists(strNewFolder))
				{
					strNewFolder = xState.m_strCurrentDirectory + "/NewFolder" + std::to_string(iCounter++);
				}
				std::filesystem::create_directory(strNewFolder);
				xState.m_bDirectoryNeedsRefresh = true;
			}
			if (ImGui::MenuItem("Material"))
			{
				// Create new material
				std::string strNewMaterial = xState.m_strCurrentDirectory + "/NewMaterial" + ZENITH_MATERIAL_EXT;
				int iCounter = 1;
				while (std::filesystem::exists(strNewMaterial))
				{
					strNewMaterial = xState.m_strCurrentDirectory + "/NewMaterial" + std::to_string(iCounter++) + ZENITH_MATERIAL_EXT;
				}
				Flux_MaterialAsset* pxNewMat = Flux_MaterialAsset::Create("NewMaterial");
				if (pxNewMat)
				{
					pxNewMat->SaveToFile(strNewMaterial);
					xState.m_bDirectoryNeedsRefresh = true;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	// Display directory contents in a table/grid
	float fPanelWidth = ImGui::GetContentRegionAvail().x;
	float fCellSize = 80.0f;  // Size of each item cell
	int iColumnCount = std::max(1, (int)(fPanelWidth / fCellSize));

	if (ImGui::BeginTable("ContentBrowserTable", iColumnCount))
	{
		for (size_t i = 0; i < xState.m_xFilteredContents.size(); ++i)
		{
			const ContentBrowserEntry& xEntry = xState.m_xFilteredContents[i];

			ImGui::TableNextColumn();
			ImGui::PushID((int)i);

			// Icon representation (using text for now)
			const char* szIcon = xEntry.m_bIsDirectory ? "[DIR]" : "[FILE]";

			// File type specific icons
			if (!xEntry.m_bIsDirectory)
			{
				if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT) szIcon = "[TEX]";
				else if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT) szIcon = "[MAT]";
				else if (xEntry.m_strExtension == ZENITH_MESH_EXT) szIcon = "[MSH]";
				else if (xEntry.m_strExtension == ZENITH_MODEL_EXT) szIcon = "[MDL]";
				else if (xEntry.m_strExtension == ZENITH_PREFAB_EXT) szIcon = "[PRE]";
				else if (xEntry.m_strExtension == ".zscn") szIcon = "[SCN]";
				else if (xEntry.m_strExtension == ZENITH_ANIMATION_EXT) szIcon = "[ANM]";
			}

			ImGui::BeginGroup();

			if (xEntry.m_bIsDirectory)
			{
				// Directory - click to enter
				if (ImGui::Button(szIcon, ImVec2(fCellSize - 10, fCellSize - 30)))
				{
					NavigateToDirectory(xState, xEntry.m_strFullPath);
				}
			}
			else
			{
				// File - can be dragged
				ImGui::Button(szIcon, ImVec2(fCellSize - 10, fCellSize - 30));

				// Drag source for files
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					DragDropFilePayload xPayload;
					strncpy(xPayload.m_szFilePath, xEntry.m_strFullPath.c_str(),
						sizeof(xPayload.m_szFilePath) - 1);
					xPayload.m_szFilePath[sizeof(xPayload.m_szFilePath) - 1] = '\0';

					// Determine payload type based on extension
					const char* szPayloadType = DRAGDROP_PAYLOAD_FILE_GENERIC;
					if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_TEXTURE;
					}
					else if (xEntry.m_strExtension == ZENITH_MESH_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MESH;
					}
					else if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MATERIAL;
					}
					else if (xEntry.m_strExtension == ZENITH_PREFAB_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_PREFAB;
					}
					else if (xEntry.m_strExtension == ZENITH_MODEL_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_MODEL;
					}
					else if (xEntry.m_strExtension == ZENITH_ANIMATION_EXT)
					{
						szPayloadType = DRAGDROP_PAYLOAD_ANIMATION;
					}

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));

					// Drag preview tooltip
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());

					ImGui::EndDragDropSource();
				}

				// Double-click to open material files in editor
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
					{
						Flux_MaterialAsset* pMaterial = Flux_MaterialAsset::LoadFromFile(xEntry.m_strFullPath);
						if (pMaterial)
						{
							Zenith_Editor::SelectMaterial(pMaterial);
						}
					}
				}
			}

			// Context menu for individual items
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Show in Explorer"))
				{
#ifdef _WIN32
					std::string strCmd = "explorer /select,\"" + xEntry.m_strFullPath + "\"";
					system(strCmd.c_str());
#endif
				}
				if (!xEntry.m_bIsDirectory)
				{
					if (ImGui::MenuItem("Delete"))
					{
						if (std::filesystem::remove(xEntry.m_strFullPath))
						{
							// Also try to remove the .zmeta file
							std::string strMetaPath = xEntry.m_strFullPath + ".zmeta";
							std::filesystem::remove(strMetaPath);
							xState.m_bDirectoryNeedsRefresh = true;
						}
					}
					if (ImGui::MenuItem("Duplicate"))
					{
						std::filesystem::path xPath(xEntry.m_strFullPath);
						std::string strNewPath = xPath.parent_path().string() + "/" +
							xPath.stem().string() + "_copy" + xPath.extension().string();
						int iCounter = 1;
						while (std::filesystem::exists(strNewPath))
						{
							strNewPath = xPath.parent_path().string() + "/" +
								xPath.stem().string() + "_copy" + std::to_string(iCounter++) + xPath.extension().string();
						}
						std::filesystem::copy(xEntry.m_strFullPath, strNewPath);
						xState.m_bDirectoryNeedsRefresh = true;
					}
				}
				else
				{
					if (ImGui::MenuItem("Delete Folder"))
					{
						// Only delete empty folders for safety
						if (std::filesystem::is_empty(xEntry.m_strFullPath))
						{
							std::filesystem::remove(xEntry.m_strFullPath);
							xState.m_bDirectoryNeedsRefresh = true;
						}
						else
						{
							Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Cannot delete non-empty folder");
						}
					}
				}
				ImGui::EndPopup();
			}

			// Display truncated filename below icon
			std::string strDisplayName = xEntry.m_strName;
			if (strDisplayName.length() > 10)
			{
				strDisplayName = strDisplayName.substr(0, 7) + "...";
			}
			ImGui::TextWrapped("%s", strDisplayName.c_str());

			// Tooltip with full filename
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", xEntry.m_strName.c_str());
			}

			ImGui::EndGroup();
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::End();
}

void Zenith_EditorPanelContentBrowser::RefreshDirectoryContents(ContentBrowserState& xState)
{
	xState.m_xDirectoryContents.clear();
	xState.m_xFilteredContents.clear();

	try
	{
		for (const auto& xEntry : std::filesystem::directory_iterator(xState.m_strCurrentDirectory))
		{
			ContentBrowserEntry xBrowserEntry;
			xBrowserEntry.m_strFullPath = xEntry.path().string();
			xBrowserEntry.m_strName = xEntry.path().filename().string();
			xBrowserEntry.m_strExtension = xEntry.path().extension().string();
			xBrowserEntry.m_bIsDirectory = xEntry.is_directory();

			xState.m_xDirectoryContents.push_back(xBrowserEntry);
		}

		// Sort: directories first, then files, alphabetically within each group
		std::sort(xState.m_xDirectoryContents.begin(), xState.m_xDirectoryContents.end(),
			[](const ContentBrowserEntry& a, const ContentBrowserEntry& b) {
				if (a.m_bIsDirectory != b.m_bIsDirectory)
					return a.m_bIsDirectory > b.m_bIsDirectory;
				return a.m_strName < b.m_strName;
			});

		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Refreshed directory: %s (%zu items)",
			xState.m_strCurrentDirectory.c_str(), xState.m_xDirectoryContents.size());
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Error reading directory: %s", e.what());
	}
}

void Zenith_EditorPanelContentBrowser::NavigateToDirectory(ContentBrowserState& xState, const std::string& strPath)
{
	xState.m_strCurrentDirectory = strPath;
	xState.m_bDirectoryNeedsRefresh = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Navigated to: %s", strPath.c_str());
}

void Zenith_EditorPanelContentBrowser::NavigateToParent(ContentBrowserState& xState)
{
	std::filesystem::path xPath(xState.m_strCurrentDirectory);
	std::filesystem::path xParent = xPath.parent_path();

	// Don't navigate above game assets directory
	std::string strAssetsRoot = Project_GetGameAssetsDirectory();
	// Remove trailing slash if present for comparison
	if (!strAssetsRoot.empty() && (strAssetsRoot.back() == '/' || strAssetsRoot.back() == '\\'))
	{
		strAssetsRoot.pop_back();
	}

	if (xParent.string().length() >= strAssetsRoot.length())
	{
		xState.m_strCurrentDirectory = xParent.string();
		xState.m_bDirectoryNeedsRefresh = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Navigated to parent: %s", xState.m_strCurrentDirectory.c_str());
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[ContentBrowser] Already at root directory");
	}
}

#endif // ZENITH_TOOLS
