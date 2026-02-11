#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_ContentBrowser.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Editor/Zenith_UndoSystem.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/Flux_Graphics.h"
#include "../../../Tools/Zenith_Tools_TextureExport.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <filesystem>
#include <algorithm>
#include <unordered_map>

//=============================================================================
// Zenith File Type Registry
//=============================================================================
static const EditorFileTypeInfo s_axKnownFileTypes[] = {
	{ ZENITH_TEXTURE_EXT,    "[TEX]", "Texture",   DRAGDROP_PAYLOAD_TEXTURE },
	{ ZENITH_MATERIAL_EXT,   "[MAT]", "Material",  DRAGDROP_PAYLOAD_MATERIAL },
	{ ZENITH_MESH_EXT,       "[MSH]", "Mesh",      DRAGDROP_PAYLOAD_MESH },
	{ ZENITH_MODEL_EXT,      "[MDL]", "Model",     DRAGDROP_PAYLOAD_MODEL },
	{ ZENITH_PREFAB_EXT,     "[PRE]", "Prefab",    DRAGDROP_PAYLOAD_PREFAB },
	{ ZENITH_SCENE_EXT,      "[SCN]", "Scene",     DRAGDROP_PAYLOAD_FILE_GENERIC },
	{ ZENITH_ANIMATION_EXT,  "[ANM]", "Animation", DRAGDROP_PAYLOAD_ANIMATION },
};

const EditorFileTypeInfo* GetFileTypeInfo(const std::string& strExtension)
{
	for (const auto& xType : s_axKnownFileTypes)
	{
		if (strExtension == xType.m_szExtension)
		{
			return &xType;
		}
	}
	return nullptr;
}

static void FormatFileSize(uint64_t ulBytes, char* pBuffer, size_t uBufferSize)
{
	if (ulBytes < 1024)
	{
		snprintf(pBuffer, uBufferSize, "%llu B", ulBytes);
	}
	else if (ulBytes < 1024 * 1024)
	{
		snprintf(pBuffer, uBufferSize, "%.1f KB", ulBytes / 1024.0);
	}
	else if (ulBytes < 1024ULL * 1024 * 1024)
	{
		snprintf(pBuffer, uBufferSize, "%.2f MB", ulBytes / (1024.0 * 1024.0));
	}
	else
	{
		snprintf(pBuffer, uBufferSize, "%.2f GB", ulBytes / (1024.0 * 1024.0 * 1024.0));
	}
}

static void RenderBreadcrumbs(ContentBrowserState& xState)
{
	std::string strAssetsRoot = Project_GetGameAssetsDirectory();
	// Normalize trailing slash
	if (!strAssetsRoot.empty() && (strAssetsRoot.back() == '/' || strAssetsRoot.back() == '\\'))
	{
		strAssetsRoot.pop_back();
	}

	std::filesystem::path xCurrentPath(xState.m_strCurrentDirectory);
	std::filesystem::path xRootPath(strAssetsRoot);

	// Build path segments
	std::vector<std::pair<std::string, std::string>> axSegments;

	// Add root as "Assets"
	axSegments.push_back({ "Assets", strAssetsRoot });

	// Build relative path components
	try
	{
		std::filesystem::path xRelPath = std::filesystem::relative(xCurrentPath, xRootPath);
		std::filesystem::path xBuildPath = xRootPath;

		for (const auto& xPart : xRelPath)
		{
			std::string strPart = xPart.string();
			if (strPart == "." || strPart.empty())
			{
				continue;
			}
			xBuildPath /= xPart;
			axSegments.push_back({ strPart, xBuildPath.string() });
		}
	}
	catch (...) {}

	// Render breadcrumbs
	for (size_t i = 0; i < axSegments.size(); ++i)
	{
		if (i > 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled(">");
			ImGui::SameLine();
		}

		// Last segment is non-clickable (current folder)
		if (i == axSegments.size() - 1)
		{
			ImGui::Text("%s", axSegments[i].first.c_str());
		}
		else
		{
			if (ImGui::SmallButton(axSegments[i].first.c_str()))
			{
				Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, axSegments[i].second);
			}
		}
	}
}

//=============================================================================
// Texture Preview Cache for Content Browser
//=============================================================================
namespace
{
	struct TextureThumbnailEntry
	{
		TextureHandle m_xTexture;  // Handle manages ref counting
		Flux_ImGuiTextureHandle m_xImGuiHandle;
		bool m_bLoadAttempted = false;
	};

	// Cache of loaded texture thumbnails keyed by file path
	static std::unordered_map<std::string, TextureThumbnailEntry> s_xThumbnailCache;

	// Maximum number of thumbnails to keep loaded (LRU would be ideal, but simple cap for now)
	static constexpr size_t MAX_CACHED_THUMBNAILS = 100;
}

//-----------------------------------------------------------------------------
// Get or load a texture thumbnail for the content browser
//-----------------------------------------------------------------------------
static Flux_ImGuiTextureHandle GetTextureThumbnail(const std::string& strPath)
{
	auto it = s_xThumbnailCache.find(strPath);
	if (it != s_xThumbnailCache.end())
	{
		if (it->second.m_xImGuiHandle.IsValid())
		{
			return it->second.m_xImGuiHandle;
		}
		// Already tried loading and failed
		if (it->second.m_bLoadAttempted)
		{
			return Flux_ImGuiTextureHandle();
		}
	}

	// Limit cache size
	if (s_xThumbnailCache.size() >= MAX_CACHED_THUMBNAILS)
	{
		// Simple eviction: just don't load more for now
		// A proper implementation would use LRU eviction
		return Flux_ImGuiTextureHandle();
	}

	// Try to load the texture via registry using handle (manages ref counting)
	TextureThumbnailEntry xEntry;
	xEntry.m_bLoadAttempted = true;

	xEntry.m_xTexture.SetPath(strPath);
	Zenith_TextureAsset* pxTexture = xEntry.m_xTexture.Get();
	if (pxTexture && pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
	{
		xEntry.m_xImGuiHandle = Flux_ImGuiIntegration::RegisterTexture(
			pxTexture->m_xSRV,
			Flux_Graphics::s_xClampSampler
		);
	}

	s_xThumbnailCache[strPath] = xEntry;
	return xEntry.m_xImGuiHandle;
}

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
	bool bCanGoBack = xState.m_iHistoryIndex > 0;
	bool bCanGoForward = xState.m_iHistoryIndex >= 0 &&
		xState.m_iHistoryIndex < static_cast<int>(xState.m_axNavigationHistory.size()) - 1;

	// Back button
	ImGui::BeginDisabled(!bCanGoBack);
	if (ImGui::Button("<"))
	{
		xState.m_iHistoryIndex--;
		NavigateToDirectory(xState, xState.m_axNavigationHistory[xState.m_iHistoryIndex], false);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("Back");
	}

	ImGui::SameLine();

	// Forward button
	ImGui::BeginDisabled(!bCanGoForward);
	if (ImGui::Button(">"))
	{
		xState.m_iHistoryIndex++;
		NavigateToDirectory(xState, xState.m_axNavigationHistory[xState.m_iHistoryIndex], false);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("Forward");
	}

	ImGui::SameLine();

	// Parent folder button
	if (ImGui::Button("^"))
	{
		NavigateToParent(xState);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Go to Parent Folder");
	}

	ImGui::SameLine();

	// Refresh button
	if (ImGui::Button("Refresh"))
	{
		xState.m_bDirectoryNeedsRefresh = true;
	}

	ImGui::SameLine();

	// Breadcrumb navigation
	RenderBreadcrumbs(xState);

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

	ImGui::SameLine();

	// Thumbnail size slider
	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderFloat("##ThumbnailSize", &xState.m_fThumbnailSize, 40.0f, 200.0f, "%.0f");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Thumbnail Size (Ctrl+Scroll)");
	}

	// Ctrl+Scroll to adjust thumbnail size
	if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl)
	{
		float fScroll = ImGui::GetIO().MouseWheel;
		if (fScroll != 0.0f)
		{
			xState.m_fThumbnailSize = std::clamp(
				xState.m_fThumbnailSize + fScroll * 10.0f,
				40.0f, 200.0f);
		}
	}

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	// View mode toggle buttons
	bool bGridSelected = (xState.m_eViewMode == ContentBrowserViewMode::Grid);
	if (bGridSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	}
	if (ImGui::Button("Grid"))
	{
		xState.m_eViewMode = ContentBrowserViewMode::Grid;
	}
	if (bGridSelected)
	{
		ImGui::PopStyleColor();
	}

	ImGui::SameLine();

	bool bListSelected = (xState.m_eViewMode == ContentBrowserViewMode::List);
	if (bListSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	}
	if (ImGui::Button("List"))
	{
		xState.m_eViewMode = ContentBrowserViewMode::List;
	}
	if (bListSelected)
	{
		ImGui::PopStyleColor();
	}

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
				case 6: bPassFilter = (xEntry.m_strExtension == ZENITH_SCENE_EXT); break;
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
				Zenith_MaterialAsset* pxNewMat = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
				if (pxNewMat)
				{
					pxNewMat->SetName("NewMaterial");
					pxNewMat->SaveToFile(strNewMaterial);
					xState.m_bDirectoryNeedsRefresh = true;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	// Display directory contents
	float fPanelWidth = ImGui::GetContentRegionAvail().x;
	float fCellSize = xState.m_fThumbnailSize;

	if (xState.m_eViewMode == ContentBrowserViewMode::List)
	{
		// List view
		if (ImGui::BeginTable("ContentBrowserList", 4,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
			ImVec2(0, 0)))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableSetupColumn("Extension", ImGuiTableColumnFlags_WidthFixed, 70.0f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < xState.m_xFilteredContents.size(); ++i)
			{
				const ContentBrowserEntry& xEntry = xState.m_xFilteredContents[i];
				ImGui::PushID(static_cast<int>(i));

				ImGui::TableNextRow();

				// Name column
				ImGui::TableNextColumn();
				std::string strIcon = xEntry.m_bIsDirectory ? "[DIR] " : "";
				std::string strLabel = strIcon + xEntry.m_strName;

				ImGuiSelectableFlags eFlags = ImGuiSelectableFlags_SpanAllColumns |
					ImGuiSelectableFlags_AllowDoubleClick;

				if (ImGui::Selectable(strLabel.c_str(), false, eFlags))
				{
					if (ImGui::IsMouseDoubleClicked(0))
					{
						if (xEntry.m_bIsDirectory)
						{
							NavigateToDirectory(xState, xEntry.m_strFullPath);
						}
						else if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
						{
							Zenith_MaterialAsset* pMaterial =
								Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(xEntry.m_strFullPath);
							if (pMaterial)
							{
								Zenith_Editor::SelectMaterial(pMaterial);
							}
						}
						else if (xEntry.m_strExtension == ZENITH_SCENE_EXT)
						{
							Zenith_Editor::RequestLoadSceneFromFile(xEntry.m_strFullPath);
						}
					}
				}

				// Drag source
				if (!xEntry.m_bIsDirectory &&
					ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					DragDropFilePayload xPayload;
					strncpy(xPayload.m_szFilePath, xEntry.m_strFullPath.c_str(),
						sizeof(xPayload.m_szFilePath) - 1);
					xPayload.m_szFilePath[sizeof(xPayload.m_szFilePath) - 1] = '\0';

					const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
					const char* szPayloadType = pxTypeInfo ? pxTypeInfo->m_szDragDropType : DRAGDROP_PAYLOAD_FILE_GENERIC;

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());
					ImGui::EndDragDropSource();
				}

				// Context menu
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
								std::string strMetaPath = xEntry.m_strFullPath + ".zmeta";
								std::filesystem::remove(strMetaPath);
								xState.m_bDirectoryNeedsRefresh = true;
							}
						}
					}
					ImGui::EndPopup();
				}

				// Type column
				ImGui::TableNextColumn();
				if (xEntry.m_bIsDirectory)
				{
					ImGui::TextDisabled("Folder");
				}
				else
				{
					const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
					if (pxTypeInfo)
					{
						ImGui::Text("%s", pxTypeInfo->m_szDisplayName);
					}
					else
					{
						ImGui::TextDisabled("File");
					}
				}

				// Size column
				ImGui::TableNextColumn();
				if (!xEntry.m_bIsDirectory && xEntry.m_ulFileSize > 0)
				{
					char acBuffer[32];
					FormatFileSize(xEntry.m_ulFileSize, acBuffer, sizeof(acBuffer));
					ImGui::Text("%s", acBuffer);
				}

				// Extension column
				ImGui::TableNextColumn();
				if (!xEntry.m_bIsDirectory && !xEntry.m_strExtension.empty())
				{
					ImGui::Text("%s", xEntry.m_strExtension.c_str() + 1);
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
	else
	{
		// Grid view
		int iColumnCount = std::max(1, (int)(fPanelWidth / fCellSize));

		if (ImGui::BeginTable("ContentBrowserTable", iColumnCount))
		{
		for (size_t i = 0; i < xState.m_xFilteredContents.size(); ++i)
		{
			const ContentBrowserEntry& xEntry = xState.m_xFilteredContents[i];

			ImGui::TableNextColumn();
			ImGui::PushID((int)i);

			// Icon representation
			const char* szIcon = "[DIR]";
			static char szExtIcon[16];  // Static buffer for unknown extension icons
			if (!xEntry.m_bIsDirectory)
			{
				const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
				if (pxTypeInfo)
				{
					szIcon = pxTypeInfo->m_szIconText;
				}
				else
				{
					// Unknown type - show uppercase extension
					std::string strExt = xEntry.m_strExtension;
					if (!strExt.empty() && strExt[0] == '.')
					{
						strExt = strExt.substr(1);
					}
					std::transform(strExt.begin(), strExt.end(), strExt.begin(), ::toupper);
					snprintf(szExtIcon, sizeof(szExtIcon), "[%s]", strExt.c_str());
					szIcon = szExtIcon;
				}
			}

			ImGui::BeginGroup();

			// Calculate icon/thumbnail size
			ImVec2 xIconSize(fCellSize - 10, fCellSize - 30);
			bool bShowedImage = false;

			if (xEntry.m_bIsDirectory)
			{
				// Directory - click to enter
				if (ImGui::Button(szIcon, xIconSize))
				{
					NavigateToDirectory(xState, xEntry.m_strFullPath);
				}
			}
			else
			{
				// For texture files, try to show a thumbnail
				if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT)
				{
					Flux_ImGuiTextureHandle xThumbHandle = GetTextureThumbnail(xEntry.m_strFullPath);
					if (xThumbHandle.IsValid())
					{
						// Show texture thumbnail as an image button
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.5f, 0.5f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.4f, 0.7f));
						ImGui::ImageButton(
							"##texthumb",
							(ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xThumbHandle),
							xIconSize
						);
						ImGui::PopStyleColor(3);
						bShowedImage = true;
					}
				}

				// Fallback to text button if no image
				if (!bShowedImage)
				{
					ImGui::Button(szIcon, xIconSize);
				}

				// Drag source for files
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					DragDropFilePayload xPayload;
					strncpy(xPayload.m_szFilePath, xEntry.m_strFullPath.c_str(),
						sizeof(xPayload.m_szFilePath) - 1);
					xPayload.m_szFilePath[sizeof(xPayload.m_szFilePath) - 1] = '\0';

					// Determine payload type from registry
					const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
					const char* szPayloadType = pxTypeInfo ? pxTypeInfo->m_szDragDropType : DRAGDROP_PAYLOAD_FILE_GENERIC;

					ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));

					// Drag preview tooltip
					ImGui::Text("Drag: %s", xEntry.m_strName.c_str());

					ImGui::EndDragDropSource();
				}

				// Double-click to open files in editor
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
					{
						Zenith_MaterialAsset* pMaterial = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(xEntry.m_strFullPath);
						if (pMaterial)
						{
							Zenith_Editor::SelectMaterial(pMaterial);
						}
					}
					else if (xEntry.m_strExtension == ZENITH_SCENE_EXT)
					{
						Zenith_Editor::RequestLoadSceneFromFile(xEntry.m_strFullPath);
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

					// Export image files to .ztxtr
					static const char* aszExportableExtensions[] = { ".png", ".jpg", ".jpeg", ".tif", ".tiff" };
					bool bCanExport = false;
					for (const char* szExt : aszExportableExtensions)
					{
						if (xEntry.m_strExtension == szExt)
						{
							bCanExport = true;
							break;
						}
					}

					if (bCanExport && ImGui::MenuItem("Export to .ztxtr"))
					{
						if (xEntry.m_strExtension == ".tif" || xEntry.m_strExtension == ".tiff")
						{
							Zenith_Tools_TextureExport::ExportFromTifFile(xEntry.m_strFullPath);
						}
						else
						{
							// PNG/JPG - use existing export (extension without dot)
							Zenith_Tools_TextureExport::ExportFromFile(
								xEntry.m_strFullPath,
								xEntry.m_strExtension.c_str() + 1,
								TextureCompressionMode::Uncompressed);
						}
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
			// Calculate max chars based on cell width (~7 pixels per char at default font)
			std::string strDisplayName = xEntry.m_strName;
			size_t uMaxChars = static_cast<size_t>((fCellSize - 10) / 7.0f);
			uMaxChars = std::max(uMaxChars, static_cast<size_t>(8));  // Minimum 8 chars
			if (strDisplayName.length() > uMaxChars)
			{
				strDisplayName = strDisplayName.substr(0, uMaxChars - 3) + "...";
			}
			ImGui::TextWrapped("%s", strDisplayName.c_str());

			// Enhanced tooltip with file info
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("%s", xEntry.m_strName.c_str());
				if (!xEntry.m_bIsDirectory)
				{
					// Show type name from registry or raw extension
					const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
					if (pxTypeInfo)
					{
						ImGui::Text("Type: %s", pxTypeInfo->m_szDisplayName);
					}
					else if (!xEntry.m_strExtension.empty())
					{
						ImGui::Text("Type: %s", xEntry.m_strExtension.c_str() + 1);
					}

					// Show file size
					char acSizeBuffer[32];
					FormatFileSize(xEntry.m_ulFileSize, acSizeBuffer, sizeof(acSizeBuffer));
					ImGui::Text("Size: %s", acSizeBuffer);
				}
				else
				{
					ImGui::TextDisabled("Folder");
				}
				ImGui::EndTooltip();
			}

			ImGui::EndGroup();
			ImGui::PopID();
		}

		ImGui::EndTable();
		}
	}  // End Grid view else block

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

			// Get file size for files
			if (!xBrowserEntry.m_bIsDirectory)
			{
				try
				{
					xBrowserEntry.m_ulFileSize = std::filesystem::file_size(xEntry.path());
				}
				catch (...) {}
			}

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

void Zenith_EditorPanelContentBrowser::NavigateToDirectory(ContentBrowserState& xState, const std::string& strPath, bool bAddToHistory)
{
	// Clear thumbnail cache when changing directories to avoid memory buildup
	// Unregister all ImGui texture handles before clearing
	for (auto& [path, entry] : s_xThumbnailCache)
	{
		if (entry.m_xImGuiHandle.IsValid())
		{
			Flux_ImGuiIntegration::UnregisterTexture(entry.m_xImGuiHandle);
		}
	}
	s_xThumbnailCache.clear();

	// Add to navigation history if requested
	if (bAddToHistory)
	{
		// Trim forward history when navigating to new location
		if (xState.m_iHistoryIndex >= 0 &&
			xState.m_iHistoryIndex < static_cast<int>(xState.m_axNavigationHistory.size()) - 1)
		{
			xState.m_axNavigationHistory.resize(xState.m_iHistoryIndex + 1);
		}

		xState.m_axNavigationHistory.push_back(strPath);

		// Limit history size
		constexpr int MAX_HISTORY_SIZE = 50;
		while (xState.m_axNavigationHistory.size() > static_cast<size_t>(MAX_HISTORY_SIZE))
		{
			xState.m_axNavigationHistory.erase(xState.m_axNavigationHistory.begin());
		}

		xState.m_iHistoryIndex = static_cast<int>(xState.m_axNavigationHistory.size()) - 1;
	}

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
		// Clear thumbnail cache when changing directories
		for (auto& [path, entry] : s_xThumbnailCache)
		{
			if (entry.m_xImGuiHandle.IsValid())
			{
				Flux_ImGuiIntegration::UnregisterTexture(entry.m_xImGuiHandle);
			}
		}
		s_xThumbnailCache.clear();

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
