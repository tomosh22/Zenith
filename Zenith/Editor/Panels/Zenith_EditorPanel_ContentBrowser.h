#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
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
	 */
	void NavigateToDirectory(ContentBrowserState& xState, const std::string& strPath);

	/**
	 * Navigate to parent directory
	 *
	 * @param xState Reference to content browser state
	 */
	void NavigateToParent(ContentBrowserState& xState);
}

#endif // ZENITH_TOOLS
