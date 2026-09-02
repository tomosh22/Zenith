#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "imgui.h"

//=============================================================================
// Zenith_EditorUI — the editor's shared look and feel.
//
// One place for everything that makes the panels read as ONE application rather
// than a pile of ImGui windows:
//   - the font set (Roboto at a DPI-aware base size; the atlas is registered
//     through Core/Zenith_EditorFontHook.h before either backend builds it),
//   - the theme (a dark palette in sRGB, converted to linear for the sRGB
//     swapchain) and the play-mode tint,
//   - a vector icon set drawn straight into ImDrawLists — crisp at any DPI,
//     no icon font, no texture asset,
//   - the styled widgets the panels compose: icon buttons, toolbar toggles,
//     the search box, badges, the inspector component header.
//
// Nothing here touches engine state; it is pure presentation.
//=============================================================================

enum class Zenith_EditorIcon
{
	Play, Pause, Stop,
	Undo, Redo,
	Translate, Rotate, Scale,
	World, Local, Snap,
	Folder, FolderOpen, File,
	Search, Eye, EyeOff, Close, Plus, Minus, Refresh,
	ArrowLeft, ArrowRight, ArrowUp, ArrowDown,
	Camera, Light, Sun, Mesh, Terrain, Entity, Particles, UI, Graph, Collider, Animation, Attachment,
	Texture, Material, Scene, Prefab, Text,
	Info, Warning, Error,
	Trash, Copy, Save, Settings, Focus, Grid, Lock, Console, Layout,
	COUNT
};

// Pre-converted (linear) packed colours the panels draw with. Filled by ApplyTheme.
struct Zenith_EditorPalette
{
	ImU32 m_uAccent = 0;
	ImU32 m_uAccentHover = 0;
	ImU32 m_uAccentDim = 0;
	ImU32 m_uSelection = 0;
	ImU32 m_uText = 0;
	ImU32 m_uTextDim = 0;
	ImU32 m_uTextBright = 0;
	ImU32 m_uPlay = 0;
	ImU32 m_uPause = 0;
	ImU32 m_uStop = 0;
	ImU32 m_uSuccess = 0;
	ImU32 m_uWarning = 0;
	ImU32 m_uError = 0;
	ImU32 m_uInfo = 0;
	ImU32 m_uAxisX = 0;
	ImU32 m_uAxisY = 0;
	ImU32 m_uAxisZ = 0;
	ImU32 m_uPanelBg = 0;
	ImU32 m_uPanelBgAlt = 0;
	ImU32 m_uToolbarBg = 0;
	ImU32 m_uStatusBg = 0;
	ImU32 m_uFrame = 0;
	ImU32 m_uFrameHover = 0;
	ImU32 m_uFrameActive = 0;
	ImU32 m_uBorder = 0;
	ImU32 m_uOverlayBg = 0;
	ImU32 m_uTypeTexture = 0;
	ImU32 m_uTypeMaterial = 0;
	ImU32 m_uTypeMesh = 0;
	ImU32 m_uTypeModel = 0;
	ImU32 m_uTypePrefab = 0;
	ImU32 m_uTypeScene = 0;
	ImU32 m_uTypeAnimation = 0;
	ImU32 m_uTypeGraph = 0;
	ImU32 m_uTypeFolder = 0;
	ImU32 m_uTypeOther = 0;
	// Mixed INTO the frame/tab/title colours while the editor is Playing.
	ImVec4 m_xPlayTint = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
};

// What an asset file looks like in the content browser: a badge colour, a short
// label for the tile, and an icon.
struct Zenith_EditorAssetTypeStyle
{
	ImU32 m_uColour = 0;
	const char* m_szShortLabel = "";
	Zenith_EditorIcon m_eIcon = Zenith_EditorIcon::File;
};

struct Zenith_EditorIconButtonOptions
{
	bool m_bSelected = false;
	bool m_bEnabled = true;
	// 0 = the toolbar default (26 px at 1x DPI).
	float m_fSize = 0.0f;
	// 0 = the palette text colour (or the accent when selected).
	ImU32 m_uTint = 0;
	// Draw no background at all when idle (only on hover/press).
	bool m_bFrameless = true;
};

namespace Zenith_EditorUI
{
	//-------------------------------------------------------------------------
	// Fonts and scale
	//-------------------------------------------------------------------------

	// Registers the editor fonts with the atlas. Called by the backends through
	// Core/Zenith_EditorFontHook.h; idempotent.
	void LoadFonts();

	// DPI scale the UI is laid out at (1.0 on a 96-dpi display). Every hard-coded
	// pixel size in the editor multiplies by this.
	float GetUIScale();

	// Pixel helper: fPixelsAt1x * GetUIScale().
	float Px(float fPixelsAt1x);

	// The main UI font (nullptr before LoadFonts, or when the TTF was not found —
	// ImGui's default is then in use and every PushFont below is a size-only push).
	ImFont* GetFont();

	// Convenience font pushes for the two non-default sizes the editor uses.
	void PushSmallFont();
	void PushHeadingFont();
	void PopFont();

	//-------------------------------------------------------------------------
	// Theme
	//-------------------------------------------------------------------------

	// Installs the editor style + palette on the current ImGui context. Safe to
	// call more than once (a scale change re-applies it).
	void ApplyTheme();

	const Zenith_EditorPalette& Palette();

	// Converts an sRGB colour to the packed linear value the sRGB swapchain wants.
	ImU32 Colour(float fR, float fG, float fB, float fA = 1.0f);

	// Push/pop the "you are in Play mode" tint over the chrome colours.
	void PushPlayModeTint();
	void PopPlayModeTint();

	//-------------------------------------------------------------------------
	// Icons
	//-------------------------------------------------------------------------

	// Draws an icon centred on xCentre inside a fSize x fSize box.
	void DrawIcon(ImDrawList* pxDraw, Zenith_EditorIcon eIcon, ImVec2 xCentre, float fSize, ImU32 uColour);

	// The icon + colour + tile label for an asset file extension (".ztxtr" ...).
	// Directories pass an empty extension with bIsDirectory = true.
	Zenith_EditorAssetTypeStyle GetAssetTypeStyle(const char* szExtension, bool bIsDirectory);

	//-------------------------------------------------------------------------
	// Widgets
	//-------------------------------------------------------------------------

	// A square icon button. szTooltip may be nullptr.
	bool IconButton(const char* szID, Zenith_EditorIcon eIcon, const char* szTooltip,
		const Zenith_EditorIconButtonOptions& xOptions = Zenith_EditorIconButtonOptions());

	// An icon followed by a text label, in one button. Used for the Play button.
	bool IconTextButton(const char* szID, Zenith_EditorIcon eIcon, const char* szLabel, const char* szTooltip,
		const Zenith_EditorIconButtonOptions& xOptions = Zenith_EditorIconButtonOptions());

	// Thin vertical rule for toolbars (same line).
	void ToolbarSeparator();

	// Search field with a magnifier and a clear button. Returns true when the text changed.
	bool SearchBox(const char* szID, char* pcBuffer, size_t uBufferSize, const char* szHint, float fWidth);

	// Small inline pill ("PLAYING", "12", "Editor") submitted as an item.
	void Badge(const char* szText, ImU32 uBackground, ImU32 uForeground);

	// The same pill drawn straight into a draw list at xPos (top-left) with no
	// item and no cursor movement — for overlays and decorations on rows the
	// cursor must not be moved for. Returns the pill's size.
	ImVec2 DrawBadge(ImDrawList* pxDraw, ImVec2 xPos, const char* szText, ImU32 uBackground, ImU32 uForeground);

	// Inspector component header: framed, collapsible, with an icon and an
	// optional remove button at the right edge. Returns true when open.
	// *pbRemoveClicked is set when the remove button was pressed this frame.
	bool ComponentHeader(const char* szID, Zenith_EditorIcon eIcon, const char* szTitle,
		bool bAllowRemove, bool* pbRemoveClicked);

	// Draws icon + text as a plain (non-interactive) row; returns the width used.
	void IconLabel(Zenith_EditorIcon eIcon, const char* szText, ImU32 uIconColour = 0);

	// Rounded translucent panel behind viewport overlays.
	void DrawOverlayBackground(ImDrawList* pxDraw, ImVec2 xMin, ImVec2 xMax);

	//-------------------------------------------------------------------------
	// Small shared helpers
	//-------------------------------------------------------------------------

	// PURE: case-insensitive substring test; an empty needle matches anything,
	// a null haystack matches nothing (unless the needle is empty).
	bool ContainsCaseInsensitive(const char* szHaystack, const char* szNeedle);

	// Frame time in milliseconds, low-pass filtered over roughly half a second
	// so the overlays and the status bar show a readable number. One shared
	// filter: feed it once per frame from wherever draws first.
	float SmoothedFrameMs(float fDt);
}

#endif // ZENITH_TOOLS
