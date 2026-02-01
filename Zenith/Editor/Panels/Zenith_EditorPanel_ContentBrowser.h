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
}

#endif // ZENITH_TOOLS
