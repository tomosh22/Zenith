#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Collections/Zenith_Vector.h"
#include <string>

//=============================================================================
// Content Browser Panel
//
// Unreal-style asset browser: a folder tree on the left, a tile grid (or
// detail list) on the right, breadcrumbs + search + type filter on top.
// Tiles carry a colour-coded type badge and, for textures, a live thumbnail.
// Files drag-drop into the scene / material slots — a .zanimctrl and a
// .zanimmask carry their OWN payload ids (WU-9.2), so the state machine panel's
// controller field and its per-layer mask field each accept only what they can
// use. Double-click opens scenes, materials, behaviour graphs, animator
// controllers, animation clips and bone masks.
//=============================================================================

namespace Zenith_EditorPanelContentBrowser
{
	void Render(Zenith_EditorContentBrowserState& xState);

	void RefreshDirectoryContents(Zenith_EditorContentBrowserState& xState);

	// Navigate to a directory; bAddToHistory=false when walking back/forward.
	void NavigateToDirectory(Zenith_EditorContentBrowserState& xState, const std::string& strPath, bool bAddToHistory = true);
	void NavigateToParent(Zenith_EditorContentBrowserState& xState);

	// Context menus (shared between list and grid views).
	void RenderItemContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState);
	void RenderCommonContextItems(const ContentBrowserEntry& xEntry);
	void RenderFileContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState);
	void RenderFolderContextMenu(const ContentBrowserEntry& xEntry, Zenith_EditorContentBrowserState& xState);
	void RenderCreateContextMenu(Zenith_EditorContentBrowserState& xState);

	// Views
	void RenderTopBar(Zenith_EditorContentBrowserState& xState);
	void RenderFolderTree(Zenith_EditorContentBrowserState& xState);
	void RenderFileList(Zenith_EditorContentBrowserState& xState);
	void RenderFileListEntry(const ContentBrowserEntry& xEntry, int iIndex, Zenith_EditorContentBrowserState& xState);
	void RenderFileGrid(Zenith_EditorContentBrowserState& xState, float fPanelWidth, float fCellSize);

	// Drag source for a file entry (no-op for directories) and the double-click
	// open dispatch (scenes, materials, graphs, animator controllers, and — via
	// the Animation Editor — animation clips and bone masks).
	void RenderEntryDragDropSource(const ContentBrowserEntry& xEntry);
	void HandleEntryDoubleClickOpen(const ContentBrowserEntry& xEntry);

	// Generate a unique filename by appending _1, _2, ... until no collision.
	std::string GenerateUniqueFilename(const std::string& strBasePath, const std::string& strSuffix);

	// PURE: whether an extension passes the type-filter combo index (0 = all).
	bool MatchesAssetTypeFilter(int iFilterIndex, const std::string& strExtension);

	// PURE: does the Animation Editor own this extension on double-click, and is
	// it a bone MASK (Action_MaskOpen) rather than a clip (OpenClip)? The rest of
	// HandleEntryDoubleClickOpen needs a live editor and its panels; this is the
	// half a headless unit can assert.
	bool OpensInAnimationPanel(const std::string& strExtension, bool& bOutIsMask);
}

#endif // ZENITH_TOOLS
