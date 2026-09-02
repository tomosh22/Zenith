#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_EditorWindowNames.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_ContentBrowser.h"
#include "Zenith_EditorPanel_GraphEditor.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorUI.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "../../../Tools/Zenith_Tools_TextureExport.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>
#include <algorithm>
#include "Collections/Zenith_HashMap.h"

//=============================================================================
// Zenith File Type Registry
//=============================================================================
static const EditorFileTypeInfo s_axKnownFileTypes[] = {
	{ ZENITH_TEXTURE_EXT,    "Texture",   DRAGDROP_PAYLOAD_TEXTURE },
	{ ZENITH_MATERIAL_EXT,   "Material",  DRAGDROP_PAYLOAD_MATERIAL },
	{ ZENITH_MESH_EXT,       "Mesh",      DRAGDROP_PAYLOAD_MESH },
	// Flux_MeshGeometry's own format -- terrain chunks and the shared prop sets.
	{ ZENITH_GEOMETRY_EXT,   "Geometry",  DRAGDROP_PAYLOAD_MESH },
	{ ZENITH_MODEL_EXT,      "Model",     DRAGDROP_PAYLOAD_MODEL },
	{ ZENITH_PREFAB_EXT,     "Prefab",    DRAGDROP_PAYLOAD_PREFAB },
	{ ZENITH_SCENE_EXT,      "Scene",     DRAGDROP_PAYLOAD_FILE_GENERIC },
	{ ZENITH_ANIMATION_EXT,  "Animation", DRAGDROP_PAYLOAD_ANIMATION },
	{ ZENITH_BGRAPH_EXT,     "Graph",     DRAGDROP_PAYLOAD_GRAPH_ASSET },
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

namespace
{
	void FormatFileSize(uint64_t ulBytes, char* pBuffer, size_t uBufferSize)
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

	std::string AssetsRoot()
	{
		std::string strRoot = Project_GetGameAssetsDirectory();
		if (!strRoot.empty() && (strRoot.back() == '/' || strRoot.back() == '\\'))
		{
			strRoot.pop_back();
		}
		return strRoot;
	}

	bool SamePath(const std::string& strA, const std::string& strB)
	{
		std::error_code xError;
		return std::filesystem::equivalent(strA, strB, xError);
	}

	//=========================================================================
	// Texture thumbnail cache
	//=========================================================================
	struct TextureThumbnailEntry
	{
		TextureHandle m_xTexture;  // Handle manages ref counting
		Flux_ImGuiTextureHandle m_xImGuiHandle;
		bool m_bLoadAttempted = false;
	};

	Zenith_HashMap<std::string, TextureThumbnailEntry>& ThumbnailCache()
	{
		static Zenith_HashMap<std::string, TextureThumbnailEntry> s_xCache;
		return s_xCache;
	}

	constexpr size_t MAX_CACHED_THUMBNAILS = 100;

	Flux_ImGuiTextureHandle GetTextureThumbnail(const std::string& strPath)
	{
		Zenith_HashMap<std::string, TextureThumbnailEntry>& xCache = ThumbnailCache();
		TextureThumbnailEntry* pxExisting = xCache.TryGet(strPath);
		if (pxExisting != nullptr)
		{
			return pxExisting->m_xImGuiHandle;   // invalid when the load failed
		}
		if (xCache.GetSize() >= MAX_CACHED_THUMBNAILS)
		{
			return Flux_ImGuiTextureHandle();
		}

		TextureThumbnailEntry xEntry;
		xEntry.m_bLoadAttempted = true;
		xEntry.m_xTexture.SetPath(strPath);
		Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(strPath);
		if (pxTexture && pxTexture->m_xSRV.m_xImageViewHandle.IsValid())
		{
			xEntry.m_xImGuiHandle = Flux_ImGuiIntegration::RegisterTexture(pxTexture->m_xSRV, g_xEngine.FluxGraphics().m_xClampSampler);
		}
		xCache[strPath] = xEntry;
		return xEntry.m_xImGuiHandle;
	}

	void ClearThumbnailCache()
	{
		Zenith_HashMap<std::string, TextureThumbnailEntry>& xCache = ThumbnailCache();
		for (Zenith_HashMap<std::string, TextureThumbnailEntry>::Iterator xIt(xCache); !xIt.Done(); xIt.Next())
		{
			TextureThumbnailEntry& xEntry = xIt.GetValueMutable();
			if (xEntry.m_xImGuiHandle.IsValid())
			{
				Flux_ImGuiIntegration::UnregisterTexture(xEntry.m_xImGuiHandle);
			}
		}
		xCache.Clear();
	}

	//=========================================================================
	// Folder tree cache — rebuilt on refresh, not every frame.
	//=========================================================================
	struct FolderNode
	{
		std::string m_strName;
		std::string m_strPath;
		Zenith_Vector<FolderNode> m_axChildren;
	};

	struct FolderTreeCache
	{
		FolderNode m_xRoot;
		bool m_bBuilt = false;
	};

	FolderTreeCache& TreeCache()
	{
		static FolderTreeCache s_xCache;
		return s_xCache;
	}

	void BuildFolderNode(FolderNode& xNode, int iDepth)
	{
		xNode.m_axChildren.Clear();
		if (iDepth > 6)
		{
			return;
		}
		std::error_code xError;
		for (const auto& xEntry : std::filesystem::directory_iterator(xNode.m_strPath, xError))
		{
			if (!xEntry.is_directory(xError))
			{
				continue;
			}
			FolderNode xChild;
			xChild.m_strName = xEntry.path().filename().string();
			xChild.m_strPath = xEntry.path().string();
			xNode.m_axChildren.PushBack(xChild);
		}
		std::sort(xNode.m_axChildren.begin(), xNode.m_axChildren.end(),
			[](const FolderNode& xA, const FolderNode& xB) { return xA.m_strName < xB.m_strName; });
		for (u_int u = 0; u < xNode.m_axChildren.GetSize(); ++u)
		{
			BuildFolderNode(xNode.m_axChildren.Get(u), iDepth + 1);
		}
	}

	void EnsureFolderTree()
	{
		FolderTreeCache& xCache = TreeCache();
		if (xCache.m_bBuilt)
		{
			return;
		}
		xCache.m_xRoot.m_strName = "Assets";
		xCache.m_xRoot.m_strPath = AssetsRoot();
		BuildFolderNode(xCache.m_xRoot, 0);
		xCache.m_bBuilt = true;
	}

	// True when strCurrent is xNode's path or lies under it (keeps the branch open).
	bool ContainsPath(const FolderNode& xNode, const std::string& strCurrent)
	{
		std::error_code xError;
		const std::filesystem::path xRel = std::filesystem::relative(strCurrent, xNode.m_strPath, xError);
		if (xError) return false;
		const std::string strRel = xRel.string();
		return !strRel.empty() && strRel.rfind("..", 0) != 0;
	}

	void RenderFolderNode(const FolderNode& xNode, Zenith_EditorContentBrowserState& xState, bool bRoot)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const bool bCurrent = SamePath(xNode.m_strPath, xState.m_strCurrentDirectory);
		ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (bCurrent) eFlags |= ImGuiTreeNodeFlags_Selected;
		if (xNode.m_axChildren.GetSize() == 0) eFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (bRoot || ContainsPath(xNode, xState.m_strCurrentDirectory)) ImGui::SetNextItemOpen(true, ImGuiCond_Once);

		ImGui::PushID(xNode.m_strPath.c_str());
		const bool bOpen = ImGui::TreeNodeEx("##folder", eFlags, "     %s", xNode.m_strName.c_str());
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xNode.m_strPath);
		}
		const ImVec2 xRowMin = ImGui::GetItemRectMin();
		const ImVec2 xRowMax = ImGui::GetItemRectMax();
		const float fIcon = ImGui::GetFontSize();
		const float fArrowSpace = fIcon + ImGui::GetStyle().FramePadding.x * 2.0f;
		Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), (bOpen && xNode.m_axChildren.GetSize() > 0) ? Zenith_EditorIcon::FolderOpen : Zenith_EditorIcon::Folder,
			ImVec2(xRowMin.x + fArrowSpace + fIcon * 0.5f, (xRowMin.y + xRowMax.y) * 0.5f), fIcon * 0.9f, xP.m_uTypeFolder);

		if (bOpen && xNode.m_axChildren.GetSize() > 0)
		{
			for (u_int u = 0; u < xNode.m_axChildren.GetSize(); ++u)
			{
				RenderFolderNode(xNode.m_axChildren.Get(u), xState, false);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	//=========================================================================
	// Top bar pieces
	//=========================================================================
	void RenderBreadcrumbs(Zenith_EditorContentBrowserState& xState)
	{
		const std::string strAssetsRoot = AssetsRoot();
		std::filesystem::path xCurrentPath(xState.m_strCurrentDirectory);
		std::filesystem::path xRootPath(strAssetsRoot);

		Zenith_Vector<std::pair<std::string, std::string>> axSegments;
		axSegments.PushBack({ "Assets", strAssetsRoot });
		std::error_code xError;
		const std::filesystem::path xRelPath = std::filesystem::relative(xCurrentPath, xRootPath, xError);
		if (!xError)
		{
			std::filesystem::path xBuildPath = xRootPath;
			for (const auto& xPart : xRelPath)
			{
				const std::string strPart = xPart.string();
				if (strPart == "." || strPart.empty() || strPart == "..") continue;
				xBuildPath /= xPart;
				axSegments.PushBack({ strPart, xBuildPath.string() });
			}
		}

		for (u_int i = 0; i < axSegments.GetSize(); ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine(0.0f, Zenith_EditorUI::Px(2.0f));
				ImGui::TextDisabled("/");
				ImGui::SameLine(0.0f, Zenith_EditorUI::Px(2.0f));
			}
			const bool bLast = (i == axSegments.GetSize() - 1);
			if (bLast)
			{
				ImGui::TextUnformatted(axSegments.Get(i).first.c_str());
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, 0);
				if (ImGui::SmallButton(axSegments.Get(i).first.c_str()))
				{
					Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, axSegments.Get(i).second);
				}
				ImGui::PopStyleColor();
			}
		}
	}

	void RenderNavButtons(Zenith_EditorContentBrowserState& xState)
	{
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_fSize = ImGui::GetFrameHeight();

		xOpts.m_bEnabled = xState.m_iHistoryIndex > 0;
		if (Zenith_EditorUI::IconButton("back", Zenith_EditorIcon::ArrowLeft, "Back", xOpts))
		{
			xState.m_iHistoryIndex--;
			Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xState.m_axNavigationHistory.Get(static_cast<u_int>(xState.m_iHistoryIndex)), false);
		}
		ImGui::SameLine();
		xOpts.m_bEnabled = xState.m_iHistoryIndex >= 0 && xState.m_iHistoryIndex < static_cast<int>(xState.m_axNavigationHistory.GetSize()) - 1;
		if (Zenith_EditorUI::IconButton("fwd", Zenith_EditorIcon::ArrowRight, "Forward", xOpts))
		{
			xState.m_iHistoryIndex++;
			Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xState.m_axNavigationHistory.Get(static_cast<u_int>(xState.m_iHistoryIndex)), false);
		}
		ImGui::SameLine();
		xOpts.m_bEnabled = !SamePath(xState.m_strCurrentDirectory, AssetsRoot());
		if (Zenith_EditorUI::IconButton("up", Zenith_EditorIcon::ArrowUp, "Parent folder", xOpts))
		{
			Zenith_EditorPanelContentBrowser::NavigateToParent(xState);
		}
		ImGui::SameLine();
		xOpts.m_bEnabled = true;
		if (Zenith_EditorUI::IconButton("refresh", Zenith_EditorIcon::Refresh, "Refresh", xOpts))
		{
			xState.m_bDirectoryNeedsRefresh = true;
			TreeCache().m_bBuilt = false;
		}
		ImGui::SameLine();
		xOpts.m_bSelected = xState.m_bShowFolderTree;
		if (Zenith_EditorUI::IconButton("tree", Zenith_EditorIcon::Layout, "Show folder tree", xOpts))
		{
			xState.m_bShowFolderTree = !xState.m_bShowFolderTree;
		}
		ImGui::SameLine(0.0f, Zenith_EditorUI::Px(10.0f));
		RenderBreadcrumbs(xState);
	}

	// Search, type filter, view toggles and the size slider, right-aligned.
	// Returns true when the search text or the type filter changed.
	bool RenderSearchAndFilter(Zenith_EditorContentBrowserState& xState)
	{
		const float fSearchWidth = Zenith_EditorUI::Px(180.0f);
		const float fFilterWidth = Zenith_EditorUI::Px(110.0f);
		const float fSlider = Zenith_EditorUI::Px(90.0f);
		const float fButton = ImGui::GetFrameHeight();
		const float fGap = ImGui::GetStyle().ItemSpacing.x;
		const float fTotal = fSearchWidth + fGap + fFilterWidth + fGap + fButton + fGap + fButton + fGap + fSlider;
		const float fAvail = ImGui::GetContentRegionAvail().x;
		if (fAvail > fTotal)
		{
			ImGui::SameLine(ImGui::GetCursorPosX() + fAvail - fTotal);
		}
		else
		{
			ImGui::NewLine();
		}

		bool bChanged = Zenith_EditorUI::SearchBox("search", xState.m_szSearchBuffer, sizeof(xState.m_szSearchBuffer), "Search...", fSearchWidth);

		ImGui::SameLine();
		const char* aszFilterTypes[] = { "All Types", "Textures", "Materials", "Meshes", "Models", "Prefabs", "Scenes", "Animations" };
		ImGui::SetNextItemWidth(fFilterWidth);
		bChanged |= ImGui::Combo("##TypeFilter", &xState.m_iAssetTypeFilter, aszFilterTypes, IM_ARRAYSIZE(aszFilterTypes));

		ImGui::SameLine();
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_fSize = fButton;
		xOpts.m_bSelected = (xState.m_eViewMode == ContentBrowserViewMode::Grid);
		if (Zenith_EditorUI::IconButton("grid", Zenith_EditorIcon::Grid, "Tile view", xOpts)) xState.m_eViewMode = ContentBrowserViewMode::Grid;
		ImGui::SameLine();
		xOpts.m_bSelected = (xState.m_eViewMode == ContentBrowserViewMode::List);
		if (Zenith_EditorUI::IconButton("list", Zenith_EditorIcon::Text, "Detail view", xOpts)) xState.m_eViewMode = ContentBrowserViewMode::List;

		ImGui::SameLine();
		ImGui::SetNextItemWidth(fSlider);
		ImGui::SliderFloat("##ThumbnailSize", &xState.m_fThumbnailSize, 48.0f, 200.0f, "%.0f");
		ImGui::SetItemTooltip("Tile size (Ctrl+Scroll)");
		return bChanged;
	}

	void ApplyContentFilter(Zenith_EditorContentBrowserState& xState)
	{
		xState.m_xFilteredContents.Clear();
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
			if (xState.m_iAssetTypeFilter > 0 && !xEntry.m_bIsDirectory
				&& !Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(xState.m_iAssetTypeFilter, xEntry.m_strExtension))
			{
				continue;
			}
			xState.m_xFilteredContents.PushBack(xEntry);
		}
	}

	//=========================================================================
	// Tiles
	//=========================================================================
	// Draws one tile at the cursor: background, badge/thumbnail, label. Returns
	// true when the tile was clicked (selection), handles double-click, drag
	// and context menu itself.
	void RenderTile(const ContentBrowserEntry& xEntry, int iIndex, float fCell, Zenith_EditorContentBrowserState& xState)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const Zenith_EditorAssetTypeStyle xStyle = Zenith_EditorUI::GetAssetTypeStyle(xEntry.m_strExtension.c_str(), xEntry.m_bIsDirectory);
		const float fLabelHeight = ImGui::GetTextLineHeight() * 2.0f + Zenith_EditorUI::Px(6.0f);
		const ImVec2 xSize(fCell, fCell + fLabelHeight);
		const ImVec2 xMin = ImGui::GetCursorScreenPos();
		const ImVec2 xMax(xMin.x + xSize.x, xMin.y + xSize.y);

		ImGui::PushID(iIndex);
		ImGui::InvisibleButton("##tile", xSize);
		const bool bHovered = ImGui::IsItemHovered();
		const bool bSelected = (xState.m_iSelectedContentIndex == iIndex);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			xState.m_iSelectedContentIndex = iIndex;
		}
		if (bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (xEntry.m_bIsDirectory) Zenith_EditorPanelContentBrowser::NavigateToDirectory(xState, xEntry.m_strFullPath);
			else Zenith_EditorPanelContentBrowser::HandleEntryDoubleClickOpen(xEntry);
		}
		Zenith_EditorPanelContentBrowser::RenderEntryDragDropSource(xEntry);
		Zenith_EditorPanelContentBrowser::RenderItemContextMenu(xEntry, xState);

		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		const float fRound = Zenith_EditorUI::Px(6.0f);
		if (bSelected)      pxDraw->AddRectFilled(xMin, xMax, xP.m_uSelection, fRound);
		else if (bHovered)  pxDraw->AddRectFilled(xMin, xMax, xP.m_uFrameHover, fRound);
		else                pxDraw->AddRectFilled(xMin, xMax, xP.m_uPanelBgAlt, fRound);

		// Preview area: thumbnail for textures, a type-tinted plate + icon otherwise.
		const float fInset = Zenith_EditorUI::Px(6.0f);
		const ImVec2 xPrevMin(xMin.x + fInset, xMin.y + fInset);
		const ImVec2 xPrevMax(xMax.x - fInset, xMin.y + fCell - fInset);
		bool bDrewImage = false;
		if (!xEntry.m_bIsDirectory && xEntry.m_strExtension == ZENITH_TEXTURE_EXT)
		{
			const Flux_ImGuiTextureHandle xThumb = GetTextureThumbnail(xEntry.m_strFullPath);
			if (xThumb.IsValid())
			{
				pxDraw->AddImageRounded((ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xThumb), xPrevMin, xPrevMax, ImVec2(0, 0), ImVec2(1, 1), 0xFFFFFFFF, fRound * 0.6f);
				bDrewImage = true;
			}
		}
		if (!bDrewImage)
		{
			const ImU32 uPlate = (xStyle.m_uColour & 0x00FFFFFF) | 0x30000000;
			pxDraw->AddRectFilled(xPrevMin, xPrevMax, uPlate, fRound * 0.6f);
			const float fIconSize = (xPrevMax.y - xPrevMin.y) * (xEntry.m_bIsDirectory ? 0.7f : 0.55f);
			Zenith_EditorUI::DrawIcon(pxDraw, xStyle.m_eIcon, ImVec2((xPrevMin.x + xPrevMax.x) * 0.5f, (xPrevMin.y + xPrevMax.y) * 0.5f), fIconSize, xStyle.m_uColour);
		}
		// Type badge in the corner (files only).
		if (!xEntry.m_bIsDirectory && xStyle.m_szShortLabel[0] != '\0')
		{
			Zenith_EditorUI::PushSmallFont();
			const ImVec2 xBadgeSize = ImGui::CalcTextSize(xStyle.m_szShortLabel);
			const float fPad = Zenith_EditorUI::Px(4.0f);
			const ImVec2 xBadgeMin(xPrevMin.x + fPad, xPrevMax.y - xBadgeSize.y - fPad * 2.0f - fPad);
			pxDraw->AddRectFilled(xBadgeMin, ImVec2(xBadgeMin.x + xBadgeSize.x + fPad * 2.0f, xBadgeMin.y + xBadgeSize.y + fPad), xStyle.m_uColour, fRound * 0.4f);
			pxDraw->AddText(ImVec2(xBadgeMin.x + fPad, xBadgeMin.y + fPad * 0.5f), Zenith_EditorUI::Colour(0.06f, 0.06f, 0.06f), xStyle.m_szShortLabel);
			Zenith_EditorUI::PopFont();
		}

		// Label: up to two lines, ellipsised.
		const ImVec2 xLabelMin(xMin.x + fInset, xMin.y + fCell);
		const ImVec2 xLabelMax(xMax.x - fInset, xMax.y - Zenith_EditorUI::Px(4.0f));
		const std::string strLabel = xEntry.m_bIsDirectory ? xEntry.m_strName : std::filesystem::path(xEntry.m_strName).stem().string();
		ImGui::RenderTextEllipsis(pxDraw, xLabelMin, xLabelMax, xLabelMax.x, strLabel.c_str(), nullptr, nullptr);

		if (bHovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(xEntry.m_strName.c_str());
			if (!xEntry.m_bIsDirectory)
			{
				const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension);
				char acSize[32];
				FormatFileSize(xEntry.m_ulFileSize, acSize, sizeof(acSize));
				ImGui::TextDisabled("%s   %s", pxTypeInfo ? pxTypeInfo->m_szDisplayName : (xEntry.m_strExtension.empty() ? "File" : xEntry.m_strExtension.c_str() + 1), acSize);
			}
			ImGui::EndTooltip();
		}
		ImGui::PopID();
	}

	// Splitter between the folder tree and the content area.
	void RenderSplitter(Zenith_EditorContentBrowserState& xState, float fHeight)
	{
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::InvisibleButton("##splitter", ImVec2(Zenith_EditorUI::Px(6.0f), ImMax(fHeight, 1.0f)));
		if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		if (ImGui::IsItemActive())
		{
			xState.m_fFolderTreeWidth = std::clamp(xState.m_fFolderTreeWidth + ImGui::GetIO().MouseDelta.x / Zenith_EditorUI::GetUIScale(), 100.0f, 500.0f);
		}
		ImGui::SameLine(0.0f, 0.0f);
	}
}

namespace Zenith_EditorPanelContentBrowser
{

//=============================================================================
// Pure helpers
//=============================================================================
bool MatchesAssetTypeFilter(int iFilterIndex, const std::string& strExtension)
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

std::string GenerateUniqueFilename(const std::string& strBasePath, const std::string& strSuffix)
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
// Context menus
//=============================================================================
void RenderItemContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState)
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

void RenderCommonContextItems(const ContentBrowserEntry& xEntry)
{
	if (ImGui::MenuItem("Show in Explorer"))
	{
#ifdef _WIN32
		std::string strCmd = "explorer /select,\"" + xEntry.m_strFullPath + "\"";
		system(strCmd.c_str());
#endif
	}
	if (ImGui::MenuItem("Copy Path"))
	{
		ImGui::SetClipboardText(xEntry.m_strFullPath.c_str());
	}
}

void RenderFileContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState)
{
	if (xEntry.m_strExtension == ZENITH_SCENE_EXT)
	{
		if (ImGui::MenuItem("Open Scene")) Zenith_EditorActions::OpenScenePath(xEntry.m_strFullPath);
		if (ImGui::MenuItem("Open Scene Additive")) g_xEngine.Scenes().LoadScene(xEntry.m_strFullPath, SCENE_LOAD_ADDITIVE);
		ImGui::Separator();
	}
	if (ImGui::MenuItem("Duplicate"))
	{
		std::filesystem::path xPath(xEntry.m_strFullPath);
		std::string strBasePath = xPath.parent_path().string() + "/" + xPath.stem().string() + "_copy";
		std::string strNewPath = GenerateUniqueFilename(strBasePath, xPath.extension().string());
		std::filesystem::copy(xEntry.m_strFullPath, strNewPath);
		xState.m_bDirectoryNeedsRefresh = true;
	}
	if (ImGui::MenuItem("Delete"))
	{
		if (std::filesystem::remove(xEntry.m_strFullPath))
		{
			std::string strMetaPath = xEntry.m_strFullPath + ZENITH_META_EXT;
			std::filesystem::remove(strMetaPath);
			xState.m_bDirectoryNeedsRefresh = true;
		}
	}

	// Export image files to .ztxtr
	static const char* aszExportableExtensions[] = { ".png", ".jpg", ".jpeg" };
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
		if (xEntry.m_strExtension == ".png")
		{
			// PNG - preserve bit depth (16-bit heightmaps -> R16_UNORM, etc.)
			Zenith_Tools_TextureExport::ExportFromHeightmapImageFile(xEntry.m_strFullPath);
		}
		else
		{
			Zenith_Tools_TextureExport::ExportFromFile(xEntry.m_strFullPath, xEntry.m_strExtension.c_str() + 1, TextureCompressionMode::Uncompressed);
		}
		xState.m_bDirectoryNeedsRefresh = true;
	}
}

void RenderFolderContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState)
{
	if (ImGui::MenuItem("Open"))
	{
		NavigateToDirectory(xState, xEntry.m_strFullPath);
	}
	if (ImGui::MenuItem("Delete Folder"))
	{
		// Only delete empty folders for safety
		if (std::filesystem::is_empty(xEntry.m_strFullPath))
		{
			std::filesystem::remove(xEntry.m_strFullPath);
			xState.m_bDirectoryNeedsRefresh = true;
			TreeCache().m_bBuilt = false;
		}
		else
		{
			Zenith_Warning(LOG_CATEGORY_EDITOR, "[ContentBrowser] Cannot delete non-empty folder '%s'", xEntry.m_strName.c_str());
		}
	}
}

void RenderCreateContextMenu(Zenith_EditorContentBrowserState& xState)
{
	if (!ImGui::BeginPopupContextWindow("ContentBrowserContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		return;
	}
	if (ImGui::BeginMenu("Create"))
	{
		if (ImGui::MenuItem("Folder"))
		{
			std::string strNewFolder = GenerateUniqueFilename(xState.m_strCurrentDirectory + "/NewFolder", "");
			std::filesystem::create_directory(strNewFolder);
			xState.m_bDirectoryNeedsRefresh = true;
			TreeCache().m_bBuilt = false;
		}
		if (ImGui::MenuItem("Material"))
		{
			std::string strNewMaterial = GenerateUniqueFilename(xState.m_strCurrentDirectory + "/NewMaterial", ZENITH_MATERIAL_EXT);
			auto xhNewMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxNewMat = xhNewMat.GetDirect();
			if (pxNewMat)
			{
				pxNewMat->SetName("NewMaterial");
				pxNewMat->SaveToFile(strNewMaterial);
				xState.m_bDirectoryNeedsRefresh = true;
			}
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Show in Explorer"))
	{
#ifdef _WIN32
		std::string strCmd = "explorer \"" + xState.m_strCurrentDirectory + "\"";
		system(strCmd.c_str());
#endif
	}
	if (ImGui::MenuItem("Refresh"))
	{
		xState.m_bDirectoryNeedsRefresh = true;
		TreeCache().m_bBuilt = false;
	}
	ImGui::EndPopup();
}

//=============================================================================
// Drag / open
//=============================================================================
void RenderEntryDragDropSource(const ContentBrowserEntry& xEntry)
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
		const Zenith_EditorAssetTypeStyle xStyle = Zenith_EditorUI::GetAssetTypeStyle(xEntry.m_strExtension.c_str(), false);
		Zenith_EditorUI::IconLabel(xStyle.m_eIcon, xEntry.m_strName.c_str(), xStyle.m_uColour);
		ImGui::EndDragDropSource();
	}
}

void HandleEntryDoubleClickOpen(const ContentBrowserEntry& xEntry)
{
	if (xEntry.m_strExtension == ZENITH_MATERIAL_EXT)
	{
		if (Zenith_MaterialAsset* pMaterial = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(xEntry.m_strFullPath))
		{
			g_xEngine.Editor().SelectMaterial(pMaterial);
		}
	}
	else if (xEntry.m_strExtension == ZENITH_SCENE_EXT)
	{
		Zenith_EditorActions::OpenScenePath(xEntry.m_strFullPath);
	}
	else if (xEntry.m_strExtension == ZENITH_BGRAPH_EXT)
	{
		Zenith_GraphEditorPanel::OpenAsset(Zenith_AssetRegistry::NormalizeAssetPath(xEntry.m_strFullPath).c_str());
	}
}

//=============================================================================
// Views
//=============================================================================
void RenderTopBar(Zenith_EditorContentBrowserState& xState)
{
	RenderNavButtons(xState);
	const bool bFilterInputsChanged = RenderSearchAndFilter(xState);

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::GetIO().KeyCtrl)
	{
		const float fScroll = ImGui::GetIO().MouseWheel;
		if (fScroll != 0.0f)
		{
			xState.m_fThumbnailSize = std::clamp(xState.m_fThumbnailSize + fScroll * 10.0f, 48.0f, 200.0f);
		}
	}

	if (bFilterInputsChanged || (xState.m_xFilteredContents.GetSize() == 0 && xState.m_xDirectoryContents.GetSize() > 0))
	{
		ApplyContentFilter(xState);
	}
}

void RenderFolderTree(Zenith_EditorContentBrowserState& xState)
{
	EnsureFolderTree();
	RenderFolderNode(TreeCache().m_xRoot, xState, true);
}

void RenderFileListEntry(const ContentBrowserEntry& xEntry, int iIndex, Zenith_EditorContentBrowserState& xState)
{
	const Zenith_EditorAssetTypeStyle xStyle = Zenith_EditorUI::GetAssetTypeStyle(xEntry.m_strExtension.c_str(), xEntry.m_bIsDirectory);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	const ImVec2 xPos = ImGui::GetCursorScreenPos();
	const float fIcon = ImGui::GetFontSize();
	Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), xStyle.m_eIcon, ImVec2(xPos.x + fIcon * 0.5f, xPos.y + ImGui::GetTextLineHeight() * 0.5f), fIcon * 0.9f, xStyle.m_uColour);
	ImGui::Dummy(ImVec2(fIcon, ImGui::GetTextLineHeight()));
	ImGui::SameLine(0.0f, Zenith_EditorUI::Px(6.0f));

	const ImGuiSelectableFlags eFlags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap;
	if (ImGui::Selectable(xEntry.m_strName.c_str(), xState.m_iSelectedContentIndex == iIndex, eFlags))
	{
		xState.m_iSelectedContentIndex = iIndex;
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (xEntry.m_bIsDirectory) NavigateToDirectory(xState, xEntry.m_strFullPath);
			else HandleEntryDoubleClickOpen(xEntry);
		}
	}
	RenderEntryDragDropSource(xEntry);
	RenderItemContextMenu(xEntry, xState);

	ImGui::TableNextColumn();
	if (xEntry.m_bIsDirectory)
	{
		ImGui::TextDisabled("Folder");
	}
	else if (const EditorFileTypeInfo* pxTypeInfo = GetFileTypeInfo(xEntry.m_strExtension))
	{
		ImGui::TextUnformatted(pxTypeInfo->m_szDisplayName);
	}
	else
	{
		ImGui::TextDisabled("%s", xEntry.m_strExtension.empty() ? "File" : xEntry.m_strExtension.c_str() + 1);
	}

	ImGui::TableNextColumn();
	if (!xEntry.m_bIsDirectory && xEntry.m_ulFileSize > 0)
	{
		char acBuffer[32];
		FormatFileSize(xEntry.m_ulFileSize, acBuffer, sizeof(acBuffer));
		ImGui::TextDisabled("%s", acBuffer);
	}
}

void RenderFileList(Zenith_EditorContentBrowserState& xState)
{
	if (!ImGui::BeginTable("ContentBrowserList", 3,
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV,
		ImVec2(0, 0)))
	{
		return;
	}
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Zenith_EditorUI::Px(90.0f));
	ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, Zenith_EditorUI::Px(80.0f));
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (u_int i = 0; i < xState.m_xFilteredContents.GetSize(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		RenderFileListEntry(xState.m_xFilteredContents.Get(i), static_cast<int>(i), xState);
		ImGui::PopID();
	}
	ImGui::EndTable();
}

void RenderFileGrid(Zenith_EditorContentBrowserState& xState, float fPanelWidth, float fCellSize)
{
	const float fGap = Zenith_EditorUI::Px(8.0f);
	const int iColumnCount = std::max(1, static_cast<int>((fPanelWidth + fGap) / (fCellSize + fGap)));
	if (xState.m_xFilteredContents.GetSize() == 0)
	{
		ImGui::Dummy(ImVec2(0, Zenith_EditorUI::Px(20.0f)));
		const char* szText = xState.m_szSearchBuffer[0] != '\0' ? "No assets match the search" : "This folder is empty";
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(szText).x) * 0.5f);
		ImGui::TextDisabled("%s", szText);
		return;
	}
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(fGap, fGap));
	for (u_int i = 0; i < xState.m_xFilteredContents.GetSize(); ++i)
	{
		if (i % iColumnCount != 0)
		{
			ImGui::SameLine();
		}
		RenderTile(xState.m_xFilteredContents.Get(i), static_cast<int>(i), fCellSize, xState);
	}
	ImGui::PopStyleVar();
}

//-----------------------------------------------------------------------------
// Render - Main content browser UI.
//-----------------------------------------------------------------------------
void Render(Zenith_EditorContentBrowserState& xState)
{
	ImGui::Begin(szEDITOR_WINDOW_CONTENT_BROWSER);

	if (xState.m_bDirectoryNeedsRefresh)
	{
		RefreshDirectoryContents(xState);
		xState.m_bDirectoryNeedsRefresh = false;
	}

	RenderTopBar(xState);
	ImGui::Spacing();

	const float fBodyHeight = ImGui::GetContentRegionAvail().y;
	if (xState.m_bShowFolderTree)
	{
		ImGui::BeginChild("FolderTree", ImVec2(Zenith_EditorUI::Px(xState.m_fFolderTreeWidth), fBodyHeight), ImGuiChildFlags_None);
		RenderFolderTree(xState);
		ImGui::EndChild();
		RenderSplitter(xState, fBodyHeight);
	}

	ImGui::BeginChild("ContentArea", ImVec2(0, fBodyHeight), ImGuiChildFlags_None);
	RenderCreateContextMenu(xState);
	const float fPanelWidth = ImGui::GetContentRegionAvail().x;
	if (xState.m_eViewMode == ContentBrowserViewMode::List)
	{
		RenderFileList(xState);
	}
	else
	{
		RenderFileGrid(xState, fPanelWidth, Zenith_EditorUI::Px(xState.m_fThumbnailSize));
	}
	ImGui::EndChild();

	ImGui::End();
}

void RefreshDirectoryContents(Zenith_EditorContentBrowserState& xState)
{
	xState.m_xDirectoryContents.Clear();
	xState.m_xFilteredContents.Clear();
	xState.m_iSelectedContentIndex = -1;

	std::error_code xError;
	for (const auto& xEntry : std::filesystem::directory_iterator(xState.m_strCurrentDirectory, xError))
	{
		ContentBrowserEntry xBrowserEntry;
		xBrowserEntry.m_strFullPath = xEntry.path().string();
		xBrowserEntry.m_strName = xEntry.path().filename().string();
		xBrowserEntry.m_strExtension = xEntry.path().extension().string();
		xBrowserEntry.m_bIsDirectory = xEntry.is_directory(xError);
		if (!xBrowserEntry.m_bIsDirectory)
		{
			xBrowserEntry.m_ulFileSize = std::filesystem::file_size(xEntry.path(), xError);
			if (xError) xBrowserEntry.m_ulFileSize = 0;
		}
		xState.m_xDirectoryContents.PushBack(xBrowserEntry);
	}
	if (xError)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "[ContentBrowser] Error reading directory '%s': %s", xState.m_strCurrentDirectory.c_str(), xError.message().c_str());
	}

	// Directories first, then files, alphabetically within each group
	std::sort(xState.m_xDirectoryContents.begin(), xState.m_xDirectoryContents.end(),
		[](const ContentBrowserEntry& a, const ContentBrowserEntry& b) {
			if (a.m_bIsDirectory != b.m_bIsDirectory)
				return a.m_bIsDirectory > b.m_bIsDirectory;
			return a.m_strName < b.m_strName;
		});
	ApplyContentFilter(xState);
}

void NavigateToDirectory(Zenith_EditorContentBrowserState& xState, const std::string& strPath, bool bAddToHistory)
{
	ClearThumbnailCache();

	if (bAddToHistory)
	{
		// Trim forward history when navigating to a new location.
		if (xState.m_iHistoryIndex >= 0 && xState.m_iHistoryIndex < static_cast<int>(xState.m_axNavigationHistory.GetSize()) - 1)
		{
			const u_int uKeepCount = static_cast<u_int>(xState.m_iHistoryIndex + 1);
			while (xState.m_axNavigationHistory.GetSize() > uKeepCount)
			{
				xState.m_axNavigationHistory.PopBack();
			}
		}
		xState.m_axNavigationHistory.PushBack(strPath);
		while (xState.m_axNavigationHistory.GetSize() > static_cast<u_int>(Zenith_EditorContentBrowserState::MAX_HISTORY_SIZE))
		{
			xState.m_axNavigationHistory.Remove(0);
		}
		xState.m_iHistoryIndex = static_cast<int>(xState.m_axNavigationHistory.GetSize()) - 1;
	}

	xState.m_strCurrentDirectory = strPath;
	xState.m_bDirectoryNeedsRefresh = true;
}

void NavigateToParent(Zenith_EditorContentBrowserState& xState)
{
	std::filesystem::path xPath(xState.m_strCurrentDirectory);
	std::filesystem::path xParent = xPath.parent_path();
	const std::string strAssetsRoot = AssetsRoot();
	// Don't navigate above the game assets directory
	if (xParent.string().length() >= strAssetsRoot.length() && !SamePath(xState.m_strCurrentDirectory, strAssetsRoot))
	{
		NavigateToDirectory(xState, xParent.string());
	}
}

} // namespace Zenith_EditorPanelContentBrowser

#endif // ZENITH_TOOLS
