#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"

//------------------------------------------------------------------------------
// Zenith_GraphEditorPanel - the Behaviour Graph node editor.
//
// A dockable/floating ImGui window editing one .bgraph asset at a time:
//   - palette (registered node types by category; click to place),
//   - canvas (drag nodes, drag output pin -> input pin to connect, right-click
//     an output pin to disconnect, Delete to remove the selected node),
//   - blackboard variables (add/remove/edit defaults),
//   - selected-node parameters (the Phase 0 reflected-property auto panel),
//   - Save (writes the asset + queues live hot reload via Zenith_GraphReload),
//   - live execution highlighting while playing (recently-executed nodes of
//     the selected entity's matching graph slot glow).
//
// Hand-rolled on ImGui drawlists (no third-party node library): the flagship
// automated test drives this panel through simulated mouse input, which needs
// exact, deterministic hit-rects - every interactable records its live screen
// rect each frame for the ZENITH_TESTING accessors below.
//------------------------------------------------------------------------------
class Zenith_GraphEditorPanel
{
public:
	// Renders the window when open. Called from Zenith_Editor::Render().
	static void Render();

	// Opens (or creates, if the file doesn't exist) the given .bgraph asset.
	// Positions the window deterministically on open.
	static void OpenAsset(const char* szAssetPath);
	static void Close();
	static bool IsOpen();

	// Saves the open asset and queues hot reload of live graph instances.
	static void Save();

	static const char* GetOpenAssetPath();

#ifdef ZENITH_TESTING
	// Live screen-position accessors (centre of the rect recorded during the
	// most recent Render). Tests must query AFTER a rendered frame.
	static bool GetPaletteEntryScreenPos(const char* szTypeName, Zenith_Maths::Vector2& xOut);
	static bool GetNodeScreenPos(u_int uNodeID, Zenith_Maths::Vector2& xOut);
	static bool GetPinScreenPos(u_int uNodeID, u_int uPin, bool bInputPin, Zenith_Maths::Vector2& xOut);
	static bool GetToolbarButtonScreenPos(const char* szLabel, Zenith_Maths::Vector2& xOut);
	static bool GetPropertyRowScreenPos(const char* szPropertyName, Zenith_Maths::Vector2& xOut);
	// Full rect (min/max) - sliders are set by clicking at a fraction of their width.
	static bool GetPropertyRowScreenRect(const char* szPropertyName, Zenith_Maths::Vector2& xOutMin, Zenith_Maths::Vector2& xOutMax);
	static u_int GetNodeCount();
	static u_int GetEdgeCount();
	static u_int GetSelectedNodeID();
	static u_int FindNodeIDByType(const char* szTypeName, u_int uOccurrence = 0);
	static bool IsDirty();
	// Reads a float param off the panel's live param-edit instance (selected node).
	static bool GetSelectedNodeParamFloat(const char* szPropertyName, float& fOut);
#endif
};

#endif // ZENITH_TOOLS
