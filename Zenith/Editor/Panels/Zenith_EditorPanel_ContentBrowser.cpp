#include "Zenith.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_ContentBrowser.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Editor/Zenith_UndoSystem.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
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
	{ ZENITH_SCRIPT_EXT,     "[SCR]", "Script",    DRAGDROP_PAYLOAD_SCRIPT_ASSET },
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
	Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(strPath);
	if (pxTexture && pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
	{
		xEntry.m_xImGuiHandle = Flux_ImGuiIntegration::RegisterTexture(
			pxTexture->m_xSRV,
			g_xEngine.FluxGraphics().m_xClampSampler
		);
	}

	s_xThumbnailCache[strPath] = xEntry;
	return xEntry.m_xImGuiHandle;
}

//=============================================================================
// Extracted helper: MatchesAssetTypeFilter
//=============================================================================
bool Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(int iFilterIndex, const std::string& strExtension)
{
	switch (iFilterIndex)
	{
	case 0: return true; // All Types
	case 1: return (strExtension == ZENITH_TEXTURE_EXT);
	case 2: return (strExtension == ZENITH_MATERIAL_EXT);
	case 3: return (strExtension == ZENITH_MESH_EXT);
	case 4: return (strExtension == ZENITH_MODEL_EXT);
	case 5: return (strExtension == ZENITH_PREFAB_EXT);
	case 6: return (strExtension == ZENITH_SCENE_EXT);
	case 7: return (strExtension == ZENITH_ANIMATION_EXT);
	default: return false;
	}
}

//=============================================================================
// Extracted helper: GenerateUniqueFilename
//=============================================================================
std::string Zenith_EditorPanelContentBrowser::GenerateUniqueFilename(const std::string& strBasePath, const std::string& strSuffix)
{
	std::string strResult = strBasePath + strSuffix;
	int iCounter = 1;
	while (std::filesystem::exists(strResult))
	{
		strResult = strBasePath + "_" + std::to_string(iCounter++) + strSuffix;
	}
	return strResult;
}

//=============================================================================
// Extracted helper: RenderItemContextMenu
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderItemContextMenu(const ContentBrowserEntry& xEntry, ContentBrowserState& xState)
{
	if (!ImGui::BeginPopupContextItem())
		return;

	RenderCommonContextItems(xEntry);
	if (xEntry.m_bIsDirectory)
		RenderFolderContextMenu(xEntry, xState);
	else
		RenderFileContextMenu(xEntry, xState);

	ImGui::EndPopup();
}

void Zenith_EditorPanelContentBrowser::RenderCommonContextItems(const ContentBrowserEntry& xEntry)
{
	if (ImGui::MenuItem("Show in Explorer"))
	{
#ifdef _WIN32
		std::string strCmd = "explorer /select,\"" + xEntry.m_strFullPath + "\"";
		system(strCmd.c_str());
#endif
	}
}

void Zenith_EditorPanelContentBrowser::RenderFileContextMenu(const ContentBrowserEntry& xEntry, ContentBrowserState& xState)
{
	if (ImGui::MenuItem("Delete"))
	{
		if (std::filesystem::remove(xEntry.m_strFullPath))
		{
			std::string strMetaPath = xEntry.m_strFullPath + ZENITH_META_EXT;
			std::filesystem::remove(strMetaPath);
			xState.m_bDirectoryNeedsRefresh = true;
		}
	}
	if (ImGui::MenuItem("Duplicate"))
	{
		std::filesystem::path xPath(xEntry.m_strFullPath);
		std::string strBasePath = xPath.parent_path().string() + "/" + xPath.stem().string() + "_copy";
		std::string strNewPath = GenerateUniqueFilename(strBasePath, xPath.extension().string());
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

void Zenith_EditorPanelContentBrowser::RenderFolderContextMenu(const ContentBrowserEntry& xEntry, ContentBrowserState& xState)
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

//=============================================================================
// Extracted helper: RenderEntryDragDropSource
// Begins a drag-drop source for a file entry using the file-type registry to
// pick the payload type. No-op for directories. Shared between list/grid.
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderEntryDragDropSource(const ContentBrowserEntry& xEntry)
{
	if (xEntry.m_bIsDirectory)
	{
		return;
	}

	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		DragDropFilePayload xPayload;
		strncpy_s(xPayload.m_szFilePath, sizeof(xPayload.m_szFilePath), xEntry.m_strFullPath.c_str(), _TRUNCATE);

		const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
		const char* szPayloadType = pxTypeInfo ? pxTypeInfo->m_szDragDropType : DRAGDROP_PAYLOAD_FILE_GENERIC;

		ImGui::SetDragDropPayload(szPayloadType, &xPayload, sizeof(xPayload));
		ImGui::Text("Drag: %s", xEntry.m_strName.c_str());
		ImGui::EndDragDropSource();
	}
}

//=============================================================================
// Extracted helper: HandleEntryDoubleClickOpen
// Shared double-click open dispatch for file entries (materials/scenes).
// Directory navigation is handled by the caller (list: navigates on
// double-click; grid: navigates on button click).
//=============================================================================
void Zenith_EditorPanelContentBrowser::HandleEntryDoubleClickOpen(const ContentBrowserEntry& xEntry)
{
	if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
	{
		Zenith_MaterialAsset* pMaterial =
			Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(xEntry.m_strFullPath);
		if (pMaterial)
		{
			g_xEngine.Editor().SelectMaterial(pMaterial);
		}
	}
	else if (xEntry.m_strExtension == ZENITH_SCENE_EXT)
	{
		g_xEngine.Editor().RequestLoadSceneFromFile(xEntry.m_strFullPath);
	}
}

//=============================================================================
// Back / Forward / Parent / Refresh — drives navigation history. The breadcrumb
// trail follows on the same line via the existing RenderBreadcrumbs helper.
static void RenderNavButtons(ContentBrowserState& xState)
{
	const bool bCanGoBack = xState.m_iHistoryIndex > 0;
	const bool bCanGoForward = xState.m_iHistoryIndex >= 0 &&
		xState.m_iHistoryIndex < static_cast<int>(xState.m_axNavigationHistory.size()) - 1;

	ImGui::BeginDisabled(!bCanGoBack);
	if (ImGui::Button("<"))
	{
		xState.m_iHistoryIndex--;
		Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xState.m_axNavigationHistory[xState.m_iHistoryIndex], false);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Back");

	ImGui::SameLine();
	ImGui::BeginDisabled(!bCanGoForward);
	if (ImGui::Button(">"))
	{
		xState.m_iHistoryIndex++;
		Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xState.m_axNavigationHistory[xState.m_iHistoryIndex], false);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Forward");

	ImGui::SameLine();
	if (ImGui::Button("^"))
	{
		Zenith_EditorPanelContentBrowser::NavigateToParent(xState);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to Parent Folder");

	ImGui::SameLine();
	if (ImGui::Button("Refresh"))
	{
		xState.m_bDirectoryNeedsRefresh = true;
	}

	ImGui::SameLine();
	RenderBreadcrumbs(xState);
}

// Search input + asset-type combo + thumbnail-size slider (with Ctrl+Scroll
// shortcut). Returns true if the search text or type filter changed this frame
// so the caller can re-run filtering.
static bool RenderSearchAndFilter(ContentBrowserState& xState)
{
	ImGui::SetNextItemWidth(200.0f);
	const bool bSearchChanged = ImGui::InputTextWithHint("##Search", "Search...", xState.m_szSearchBuffer, xState.m_uSearchBufferSize);

	ImGui::SameLine();

	const char* aszFilterTypes[] = { "All Types", "Textures", "Materials", "Meshes", "Models", "Prefabs", "Scenes", "Animations" };
	ImGui::SetNextItemWidth(120.0f);
	const bool bFilterChanged = ImGui::Combo("##TypeFilter", &xState.m_iAssetTypeFilter, aszFilterTypes, IM_ARRAYSIZE(aszFilterTypes));

	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderFloat("##ThumbnailSize", &xState.m_fThumbnailSize, 40.0f, 200.0f, "%.0f");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thumbnail Size (Ctrl+Scroll)");

	if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl)
	{
		const float fScroll = ImGui::GetIO().MouseWheel;
		if (fScroll != 0.0f)
		{
			xState.m_fThumbnailSize = std::clamp(xState.m_fThumbnailSize + fScroll * 10.0f, 40.0f, 200.0f);
		}
	}

	return bSearchChanged || bFilterChanged;
}

// Grid / List view toggle, with the active button styled with the
// ImGuiCol_ButtonActive colour so the current mode is visible.
static void RenderViewModeToggle(ContentBrowserState& xState)
{
	const bool bGridSelected = (xState.m_eViewMode == ContentBrowserViewMode::Grid);
	if (bGridSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("Grid")) xState.m_eViewMode = ContentBrowserViewMode::Grid;
	if (bGridSelected) ImGui::PopStyleColor();

	ImGui::SameLine();

	const bool bListSelected = (xState.m_eViewMode == ContentBrowserViewMode::List);
	if (bListSelected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	if (ImGui::Button("List")) xState.m_eViewMode = ContentBrowserViewMode::List;
	if (bListSelected) ImGui::PopStyleColor();
}

// Re-walk the directory contents and apply the current search + type filters.
// Called when the search/filter UI changes or when the filtered list is empty
// (initial population).
static void ApplyContentFilter(ContentBrowserState& xState)
{
	xState.m_xFilteredContents.clear();
	std::string strSearch(xState.m_szSearchBuffer);
	std::transform(strSearch.begin(), strSearch.end(), strSearch.begin(), ::tolower);

	for (const auto& xEntry : xState.m_xDirectoryContents)
	{
		if (!strSearch.empty())
		{
			std::string strNameLower = xEntry.m_strName;
			std::transform(strNameLower.begin(), strNameLower.end(), strNameLower.begin(), ::tolower);
			if (strNameLower.find(strSearch) == std::string::npos) continue;
		}

		// Directories always pass the type filter.
		if (xState.m_iAssetTypeFilter > 0 && !xEntry.m_bIsDirectory)
		{
			if (!Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(xState.m_iAssetTypeFilter, xEntry.m_strExtension)) continue;
		}

		xState.m_xFilteredContents.push_back(xEntry);
	}
}

// Extracted top bar: nav buttons + breadcrumb + search/filter + view toggle,
// then re-apply the filter if the user changed any inputs (or the list is
// empty for a freshly opened directory).
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderTopBar(ContentBrowserState& xState)
{
	RenderNavButtons(xState);

	ImGui::Separator();

	const bool bFilterInputsChanged = RenderSearchAndFilter(xState);

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	RenderViewModeToggle(xState);

	if (bFilterInputsChanged || xState.m_xFilteredContents.empty())
	{
		ApplyContentFilter(xState);
	}

	ImGui::Separator();
}

//=============================================================================
// Extracted helper: RenderCreateContextMenu
// Right-click context menu on empty area: Create Folder / Create Material.
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderCreateContextMenu(ContentBrowserState& xState)
{
	// Context menu for creating new assets (right-click on empty area)
	if (ImGui::BeginPopupContextWindow("ContentBrowserContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::BeginMenu("Create"))
		{
			if (ImGui::MenuItem("Folder"))
			{
				// Create new folder
				std::string strNewFolder = GenerateUniqueFilename(xState.m_strCurrentDirectory + "/NewFolder", "");
				std::filesystem::create_directory(strNewFolder);
				xState.m_bDirectoryNeedsRefresh = true;
			}
			if (ImGui::MenuItem("Material"))
			{
				// Create new material
				std::string strNewMaterial = GenerateUniqueFilename(xState.m_strCurrentDirectory + "/NewMaterial", ZENITH_MATERIAL_EXT);
				Zenith_MaterialAsset* pxNewMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
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
}

//=============================================================================
// Extracted helper: RenderFileListEntry
// Renders a single row inside the RenderFileList table.
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderFileListEntry(const ContentBrowserEntry& xEntry, ContentBrowserState& xState)
{
	ImGui::TableNextRow();

	// Name column + selectable spanning all columns
	ImGui::TableNextColumn();
	const std::string strIcon = xEntry.m_bIsDirectory ? "[DIR] " : "";
	const std::string strLabel = strIcon + xEntry.m_strName;

	const ImGuiSelectableFlags eFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
	if (ImGui::Selectable(strLabel.c_str(), false, eFlags) && ImGui::IsMouseDoubleClicked(0))
	{
		if (xEntry.m_bIsDirectory)
			NavigateToDirectory(xState, xEntry.m_strFullPath);
		else
			HandleEntryDoubleClickOpen(xEntry);
	}

	RenderEntryDragDropSource(xEntry);
	RenderItemContextMenu(xEntry, xState);

	// Type column
	ImGui::TableNextColumn();
	if (xEntry.m_bIsDirectory)
	{
		ImGui::TextDisabled("Folder");
	}
	else if (const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension))
	{
		ImGui::Text("%s", pxTypeInfo->m_szDisplayName);
	}
	else
	{
		ImGui::TextDisabled("File");
	}

	// Size column
	ImGui::TableNextColumn();
	if (!xEntry.m_bIsDirectory && xEntry.m_ulFileSize > 0)
	{
		char acBuffer[32];
		FormatFileSize(xEntry.m_ulFileSize, acBuffer, sizeof(acBuffer));
		ImGui::Text("%s", acBuffer);
	}

	// Extension column (skip leading dot)
	ImGui::TableNextColumn();
	if (!xEntry.m_bIsDirectory && !xEntry.m_strExtension.empty())
	{
		ImGui::Text("%s", xEntry.m_strExtension.c_str() + 1);
	}
}

//=============================================================================
// Extracted helper: RenderFileList
// List (table) view of filtered directory contents.
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderFileList(ContentBrowserState& xState)
{
	if (!ImGui::BeginTable("ContentBrowserList", 4,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
		ImVec2(0, 0)))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn("Extension", ImGuiTableColumnFlags_WidthFixed, 70.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (size_t i = 0; i < xState.m_xFilteredContents.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		RenderFileListEntry(xState.m_xFilteredContents[i], xState);
		ImGui::PopID();
	}

	ImGui::EndTable();
}

// Resolve the icon text for a grid cell: directories show [DIR]; known file
// types pull from the registry; unknown extensions render as [EXT] using the
// uppercased extension. The caller's buffer holds the [EXT] text when needed
// so the returned pointer stays valid.
static const char* ResolveGridItemIcon(const ContentBrowserEntry& xEntry, char* szExtBuf, size_t uExtBufSize)
{
	if (xEntry.m_bIsDirectory) return "[DIR]";

	const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
	if (pxTypeInfo) return pxTypeInfo->m_szIconText;

	std::string strExt = xEntry.m_strExtension;
	if (!strExt.empty() && strExt[0] == '.') strExt = strExt.substr(1);
	std::transform(strExt.begin(), strExt.end(), strExt.begin(), ::toupper);
	snprintf(szExtBuf, uExtBufSize, "[%s]", strExt.c_str());
	return szExtBuf;
}

// Render the icon / thumbnail / clickable button for a grid cell. Directories
// navigate on click; texture files show a thumbnail (with fallback to text
// icon if the thumbnail isn't ready); other file types show the text icon and
// support drag-drop + double-click to open.
static void RenderGridItemIcon(const ContentBrowserEntry& xEntry, const char* szIcon,
	const ImVec2& xIconSize, ContentBrowserState& xState)
{
	if (xEntry.m_bIsDirectory)
	{
		if (ImGui::Button(szIcon, xIconSize))
		{
			Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xEntry.m_strFullPath);
		}
		return;
	}

	bool bShowedImage = false;
	if (xEntry.m_strExtension == ZENITH_TEXTURE_EXT)
	{
		Flux_ImGuiTextureHandle xThumbHandle = GetTextureThumbnail(xEntry.m_strFullPath);
		if (xThumbHandle.IsValid())
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.5f, 0.5f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.4f, 0.7f));
			ImGui::ImageButton("##texthumb",
				(ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xThumbHandle),
				xIconSize);
			ImGui::PopStyleColor(3);
			bShowedImage = true;
		}
	}
	if (!bShowedImage)
	{
		ImGui::Button(szIcon, xIconSize);
	}

	Zenith_EditorPanelContentBrowser::RenderEntryDragDropSource(xEntry);
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
	{
		Zenith_EditorPanelContentBrowser::HandleEntryDoubleClickOpen(xEntry);
	}
}

// Hover tooltip: name + type/size for files, "Folder" for directories.
static void RenderGridItemTooltip(const ContentBrowserEntry& xEntry)
{
	if (!ImGui::IsItemHovered()) return;
	ImGui::BeginTooltip();
	ImGui::Text("%s", xEntry.m_strName.c_str());
	if (!xEntry.m_bIsDirectory)
	{
		const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
		if (pxTypeInfo)
		{
			ImGui::Text("Type: %s", pxTypeInfo->m_szDisplayName);
		}
		else if (!xEntry.m_strExtension.empty())
		{
			ImGui::Text("Type: %s", xEntry.m_strExtension.c_str() + 1);
		}
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

//=============================================================================
// Extracted helper: RenderFileGrid
// Grid (thumbnail) view of filtered directory contents.
//=============================================================================
void Zenith_EditorPanelContentBrowser::RenderFileGrid(ContentBrowserState& xState, float fPanelWidth, float fCellSize)
{
	const int iColumnCount = std::max(1, (int)(fPanelWidth / fCellSize));
	if (!ImGui::BeginTable("ContentBrowserTable", iColumnCount)) return;

	for (size_t i = 0; i < xState.m_xFilteredContents.size(); ++i)
	{
		const ContentBrowserEntry& xEntry = xState.m_xFilteredContents[i];

		ImGui::TableNextColumn();
		ImGui::PushID((int)i);

		char acExtIcon[16];
		const char* szIcon = ResolveGridItemIcon(xEntry, acExtIcon, sizeof(acExtIcon));

		ImGui::BeginGroup();
		const ImVec2 xIconSize(fCellSize - 10, fCellSize - 30);
		RenderGridItemIcon(xEntry, szIcon, xIconSize, xState);

		RenderItemContextMenu(xEntry, xState);

		// Truncate filename to fit cell width (~7 px per char at default font).
		std::string strDisplayName = xEntry.m_strName;
		size_t uMaxChars = static_cast<size_t>((fCellSize - 10) / 7.0f);
		uMaxChars = std::max(uMaxChars, static_cast<size_t>(8));
		if (strDisplayName.length() > uMaxChars)
		{
			strDisplayName = strDisplayName.substr(0, uMaxChars - 3) + "...";
		}
		ImGui::TextWrapped("%s", strDisplayName.c_str());

		RenderGridItemTooltip(xEntry);

		ImGui::EndGroup();
		ImGui::PopID();
	}

	ImGui::EndTable();
}

//-----------------------------------------------------------------------------
// Render - Main content browser UI. Delegates to per-section helpers so each
// section stays focused on its own concern.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelContentBrowser::Render(ContentBrowserState& xState)
{
	ImGui::Begin("Content Browser");

	// Refresh directory contents if needed
	if (xState.m_bDirectoryNeedsRefresh)
	{
		RefreshDirectoryContents(xState);
		xState.m_bDirectoryNeedsRefresh = false;
	}

	RenderTopBar(xState);

	RenderCreateContextMenu(xState);

	// Display directory contents
	float fPanelWidth = ImGui::GetContentRegionAvail().x;
	float fCellSize = xState.m_fThumbnailSize;

	if (xState.m_eViewMode == ContentBrowserViewMode::List)
	{
		RenderFileList(xState);
	}
	else
	{
		RenderFileGrid(xState, fPanelWidth, fCellSize);
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
