#pragma once

// ============================================================================
// Zenith_EditorWindowNames
//
// The single source of truth for every dockable editor window title. The
// code-built default dock layout (Zenith_Editor's DockBuilder pass) docks
// windows BY NAME via DockBuilderDockWindow — a mismatched string silently
// no-ops and the window floats — so every ImGui::Begin for a dockable window
// and every DockBuilderDockWindow call must use these constants.
//
// Lives in Core (not Editor) because non-editor TUs own some of these
// windows (Zenith_Profiling.cpp owns "Profiling") and must not pull editor
// headers — same precedent as Core/Zenith_DragDropPayloads.h.
// ============================================================================

constexpr const char* szEDITOR_WINDOW_VIEWPORT         = "Viewport";
constexpr const char* szEDITOR_WINDOW_HIERARCHY        = "Hierarchy";
constexpr const char* szEDITOR_WINDOW_PROPERTIES       = "Properties";
constexpr const char* szEDITOR_WINDOW_TOOLBAR          = "Toolbar";
constexpr const char* szEDITOR_WINDOW_CONSOLE          = "Console";
constexpr const char* szEDITOR_WINDOW_CONTENT_BROWSER  = "Content Browser";
constexpr const char* szEDITOR_WINDOW_MATERIAL_EDITOR  = "Material Editor";
constexpr const char* szEDITOR_WINDOW_TERRAIN_EDITOR   = "Terrain Editor";
constexpr const char* szEDITOR_WINDOW_GRAPH_EDITOR     = "Graph Editor";
constexpr const char* szEDITOR_WINDOW_MEMORY_PROFILER  = "Memory Profiler";
constexpr const char* szEDITOR_WINDOW_RENDER_GRAPH     = "Render Graph";
constexpr const char* szEDITOR_WINDOW_VARIANT_EDITOR   = "Variant Editor";
constexpr const char* szEDITOR_WINDOW_ZENITH_TOOLS     = "Zenith Tools";
constexpr const char* szEDITOR_WINDOW_PROFILING        = "Profiling";

// The fullscreen host window that carries the menu bar + dockspace, and the
// dockspace's ImGui ID string.
constexpr const char* szEDITOR_WINDOW_DOCKSPACE_HOST   = "DockSpace";
constexpr const char* szEDITOR_DOCKSPACE_ID            = "MainDockSpace";
