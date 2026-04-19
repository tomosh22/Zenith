#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include <vector>
#include <string>

//=============================================================================
// Content Browser Panel
//
// File browser for game assets with drag-drop support.
//=============================================================================

// Content browser state structure to encapsulate all state variables
struct ContentBrowserState
{
	std::string& m_strCurrentDirectory;
	std::vector<ContentBrowserEntry>& m_xDirectoryContents;
	std::vector<ContentBrowserEntry>& m_xFilteredContents;
	bool& m_bDirectoryNeedsRefresh;
	char* m_szSearchBuffer;
	size_t m_uSearchBufferSize;
	int& m_iAssetTypeFilter;
	int& m_iSelectedContentIndex;
	float& m_fThumbnailSize;
	std::vector<std::string>& m_axNavigationHistory;
	int& m_iHistoryIndex;
	ContentBrowserViewMode& m_eViewMode;
};

namespace Zenith_EditorPanelContentBrowser
{
	/**
	 * Render the content browser panel
	 *
	 * @param xState Reference to content browser state
	 */
	void Render(ContentBrowserState& xState);

	/**
	 * Refresh directory contents
	 *
	 * @param xState Reference to content browser state
	 */
	void RefreshDirectoryContents(ContentBrowserState& xState);

	/**
	 * Navigate to a directory
	 *
	 * @param xState Reference to content browser state
	 * @param strPath Path to navigate to
	 * @param bAddToHistory Whether to add this navigation to history (default true)
	 */
	void NavigateToDirectory(ContentBrowserState& xState, const std::string& strPath, bool bAddToHistory = true);

	/**
	 * Navigate to parent directory
	 *
	 * @param xState Reference to content browser state
	 */
	void NavigateToParent(ContentBrowserState& xState);

	/**
	 * Render the right-click context menu for a content browser entry
	 * Shared between list and grid views
	 *
	 * @param xEntry The content browser entry to render the context menu for
	 * @param xState Reference to content browser state
	 */
	void RenderItemContextMenu(const ContentBrowserEntry& xEntry, ContentBrowserState& xState);

	//-------------------------------------------------------------------------
	// Private helpers used by Render() to keep the main dispatcher thin.
	// Declared here so they can be forward-referenced from the header-level
	// namespace; definitions live in the .cpp file.
	//-------------------------------------------------------------------------

	/**
	 * Render the top bar: navigation buttons, breadcrumbs, search input,
	 * asset type filter, thumbnail size slider, and view toggle buttons.
	 * Also applies filtering when search/filter changes.
	 *
	 * @param xState Reference to content browser state
	 */
	void RenderTopBar(ContentBrowserState& xState);

	/**
	 * Render the right-click context menu for empty space (Create Folder /
	 * Create Material).
	 *
	 * @param xState Reference to content browser state
	 */
	void RenderCreateContextMenu(ContentBrowserState& xState);

	/**
	 * Render the list (table) view of the current directory contents.
	 *
	 * @param xState Reference to content browser state
	 */
	void RenderFileList(ContentBrowserState& xState);

	/**
	 * Render the grid (thumbnail) view of the current directory contents.
	 *
	 * @param xState Reference to content browser state
	 * @param fPanelWidth Width of the content region available to the grid
	 * @param fCellSize Thumbnail cell size
	 */
	void RenderFileGrid(ContentBrowserState& xState, float fPanelWidth, float fCellSize);

	/**
	 * Begin a drag-drop source for a file entry (shared between list/grid).
	 * No-op for directories.
	 *
	 * @param xEntry The content browser entry to use as drag source
	 */
	void RenderEntryDragDropSource(const ContentBrowserEntry& xEntry);

	/**
	 * Handle double-click open semantics for a file entry (materials/scenes).
	 * Shared between list and grid views.
	 *
	 * @param xEntry The content browser entry that was double-clicked
	 */
	void HandleEntryDoubleClickOpen(const ContentBrowserEntry& xEntry);

	/**
	 * Generate a unique filename by appending _1, _2, etc. until no collision
	 *
	 * @param strBasePath Base file path without extension (e.g., "C:/assets/NewFolder")
	 * @param strSuffix File suffix including extension (e.g., ".zmtrl") or empty for folders
	 * @return A unique path that does not yet exist on disk
	 */
	std::string GenerateUniqueFilename(const std::string& strBasePath, const std::string& strSuffix);

	/**
	 * Check if a file extension matches the current asset type filter
	 *
	 * @param iFilterIndex Current filter dropdown index (0 = All, 1 = Textures, etc.)
	 * @param strExtension File extension to check (e.g., ".ztxtr")
	 * @return true if the file should be shown under this filter
	 */
	bool MatchesAssetTypeFilter(int iFilterIndex, const std::string& strExtension);
}

#endif // ZENITH_TOOLS
