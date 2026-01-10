#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include <string>

// Forward declarations
struct Zenith_EditorState;
class Flux_MaterialAsset;

//=============================================================================
// Zenith_EditorPanel
//
// Base interface for editor UI panels. Each panel is responsible for
// rendering a specific portion of the editor UI (hierarchy, properties,
// viewport, console, etc.)
//
// Panels access shared state through references passed to their Render()
// methods, avoiding global state access.
//=============================================================================

class Zenith_EditorPanel
{
public:
	virtual ~Zenith_EditorPanel() = default;

	// Render the panel UI. Called each frame.
	virtual void Render() = 0;

	// Panel identification
	virtual const char* GetName() const = 0;
	virtual const char* GetWindowID() const { return GetName(); }

	// Visibility control
	bool IsVisible() const { return m_bVisible; }
	void SetVisible(bool bVisible) { m_bVisible = bVisible; }
	void ToggleVisible() { m_bVisible = !m_bVisible; }

protected:
	bool m_bVisible = true;
};

//=============================================================================
// Panel Factory Functions
//
// Static functions to render each panel. These are used during the transition
// from the monolithic Zenith_Editor.cpp to separate panel classes.
// Eventually these will be replaced with panel class instances.
//=============================================================================

namespace Zenith_EditorPanels
{
	// Core panels
	void RenderMainMenuBar();
	void RenderToolbar();
	void RenderHierarchyPanel();
	void RenderPropertiesPanel();
	void RenderViewport();

	// Content panels
	void RenderContentBrowser();
	void RenderConsolePanel();
	void RenderMaterialEditorPanel();
}

#endif // ZENITH_TOOLS
