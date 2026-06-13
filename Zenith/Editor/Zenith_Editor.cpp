#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RendererImpl.h"
#include "Editor/Zenith_Editor.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Editor/Zenith_ImGuiInputBridge.h"
#endif
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_EditorState.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Core/Zenith_CommandLine.h"

// Bridge function called from Zenith_Log macro to add to editor console
// NOTE: Must be defined after including Zenith_Editor.h
void Zenith_EditorAddLogMessage(const char* szMessage, int eLevel, Zenith_LogCategory eCategory)
{
	// Convert int to log level enum
	ConsoleLogEntry::LogLevel xLevel = ConsoleLogEntry::LogLevel::Info;
	switch (eLevel)
	{
	case 0: xLevel = ConsoleLogEntry::LogLevel::Info; break;
	case 1: xLevel = ConsoleLogEntry::LogLevel::Warning; break;
	case 2: xLevel = ConsoleLogEntry::LogLevel::Error; break;
	}
	g_xEngine.Editor().AddLogMessage(szMessage, xLevel, eCategory);
}

#include "Zenith_EditorAutomation.h"
#include "Zenith_SelectionSystem.h"
#include "Zenith_Gizmo.h"
#include "Zenith_UndoSystem.h"
#include "Zenith_EditorSceneAccess.h"
#include "TerrainEditor/Zenith_TerrainEditor.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Core/FrameContext.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "Flux/Flux_BackendTypes.h"   // complete Flux_PlatformAPI type for ImGuiBeginFrame
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// Extracted panel implementations
#include "Panels/Zenith_EditorPanel_Console.h"
#include "Panels/Zenith_EditorPanel_ContentBrowser.h"
#include "Panels/Zenith_EditorPanel_Hierarchy.h"
#include "Panels/Zenith_EditorPanel_MaterialEditor.h"
#include "Panels/Zenith_EditorPanel_Memory.h"
#include "Panels/Zenith_EditorPanel_Properties.h"
#include "Panels/Zenith_EditorPanel_GraphEditor.h"
#include "Panels/Zenith_EditorPanel_RenderGraph.h"
#include "Panels/Zenith_EditorPanel_TerrainEditor.h"
#include "Panels/Zenith_EditorPanel_Toolbar.h"
#include "Panels/Zenith_EditorPanel_VariantEditor.h"
#include "Panels/Zenith_EditorPanel_Viewport.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
// DockBuilder API (code-built default dock layout) lives in the internal
// header by design — see BuildDefaultDockLayout below.
#include "imgui_internal.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_EditorWindowNames.h"

#include <filesystem>
#include <algorithm>

// Windows file dialog support
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
#pragma comment(lib, "Comdlg32.lib")

// Helper function to show Windows Open File dialog
// Returns empty string if cancelled
std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt)
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = szDefaultExt;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

// Helper function to show Windows Save File dialog
// Returns empty string if cancelled
std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename)
{
	char szFilePath[MAX_PATH] = { 0 };
	if (szDefaultFilename)
	{
		strncpy_s(szFilePath, sizeof(szFilePath), szDefaultFilename, _TRUNCATE);
	}

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = *szDefaultExt == '.' ? szDefaultExt+1 : szDefaultExt;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}
#endif // _WIN32

// Phase 5.5c: editor state lives on Zenith_Editor held by Zenith_Engine.
// The static definitions that used to live here -- 31 Zenith_Editor::s_* class
// statics + the file-static m_xEditorState + m_xCachedGameTextureHandle +
// m_xCachedImageViewHandle + m_xPendingDeletions -- moved onto the Impl
// (m_xXxx members). Inline getter forwarders below.

EditorMode Zenith_Editor::GetEditorMode() { return m_xEditorState.m_eEditorMode; }
Zenith_EntityID Zenith_Editor::GetSelectedEntityID() { return m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID; }
const std::unordered_set<Zenith_EntityID>& Zenith_Editor::GetSelectedEntityIDs() { return m_xEditorState.m_xSelection.m_xSelectedEntityIDs; }
size_t Zenith_Editor::GetSelectionCount() { return m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size(); }
bool Zenith_Editor::HasSelection() { return !m_xEditorState.m_xSelection.m_xSelectedEntityIDs.empty(); }
bool Zenith_Editor::HasMultiSelection() { return m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size() > 1; }
Zenith_EntityID Zenith_Editor::GetLastClickedEntityID() { return m_xEditorState.m_xSelection.m_uLastClickedEntityID; }
Zenith_Maths::Vector2 Zenith_Editor::GetViewportPos() { return m_xEditorState.m_xViewport.m_xPosition; }
Zenith_Maths::Vector2 Zenith_Editor::GetViewportSize() { return m_xEditorState.m_xViewport.m_xSize; }
EditorGizmoMode Zenith_Editor::GetGizmoMode() { return m_xEditorState.m_eGizmoMode; }
void Zenith_Editor::SetGizmoMode(EditorGizmoMode eMode) { m_xEditorState.m_eGizmoMode = eMode; }
Zenith_MaterialAsset* Zenith_Editor::GetSelectedMaterial() { return m_xEditorState.m_xMaterial.m_xSelectedMaterial.GetDirect(); }

void Zenith_Editor::ApplyEditorTheme()
{
	ImGuiStyle& xStyle = ImGui::GetStyle();

	// Layout
	xStyle.WindowPadding = ImVec2(8.0f, 8.0f);
	xStyle.FramePadding = ImVec2(5.0f, 4.0f);
	xStyle.ItemSpacing = ImVec2(6.0f, 4.0f);
	xStyle.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
	xStyle.IndentSpacing = 16.0f;
	xStyle.ScrollbarSize = 13.0f;
	xStyle.GrabMinSize = 9.0f;

	// Rounding
	xStyle.WindowRounding = 4.0f;
	xStyle.ChildRounding = 4.0f;
	xStyle.FrameRounding = 3.0f;
	xStyle.PopupRounding = 4.0f;
	xStyle.ScrollbarRounding = 9.0f;
	xStyle.GrabRounding = 3.0f;
	xStyle.TabRounding = 3.0f;

	// Borders
	xStyle.WindowBorderSize = 1.0f;
	xStyle.ChildBorderSize = 1.0f;
	xStyle.FrameBorderSize = 0.0f;
	xStyle.PopupBorderSize = 1.0f;
	xStyle.TabBorderSize = 0.0f;

	// Colors — neutral grays matching Unity's dark theme, blue accent for interactive elements only
	ImVec4* axColors = xStyle.Colors;

	// Text
	axColors[ImGuiCol_Text] = ImVec4(0.79f, 0.79f, 0.79f, 1.00f);
	axColors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	// Backgrounds
	axColors[ImGuiCol_WindowBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	axColors[ImGuiCol_ChildBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	axColors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 0.96f);

	// Borders
	axColors[ImGuiCol_Border] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	axColors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

	// Frames (input fields, checkboxes)
	axColors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	axColors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	axColors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

	// Title bar
	axColors[ImGuiCol_TitleBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	axColors[ImGuiCol_TitleBgActive] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	axColors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.16f, 0.16f, 0.16f, 0.75f);

	// Menu bar
	axColors[ImGuiCol_MenuBarBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

	// Scrollbar
	axColors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	axColors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	axColors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.37f, 0.37f, 0.37f, 1.00f);
	axColors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);

	// Checkmark, slider — blue accent
	axColors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	axColors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
	axColors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// Buttons
	axColors[ImGuiCol_Button] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	axColors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	axColors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);

	// Headers (collapsing headers, tree nodes, selectables)
	axColors[ImGuiCol_Header] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	axColors[ImGuiCol_HeaderHovered] = ImVec4(0.17f, 0.36f, 0.53f, 1.00f);
	axColors[ImGuiCol_HeaderActive] = ImVec4(0.17f, 0.36f, 0.53f, 1.00f);

	// Separators
	axColors[ImGuiCol_Separator] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	axColors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
	axColors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// Resize grip
	axColors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	axColors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	axColors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

	// Tabs
	axColors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	axColors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	axColors[ImGuiCol_TabSelected] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	axColors[ImGuiCol_TabSelectedOverline] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	axColors[ImGuiCol_TabDimmed] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
	axColors[ImGuiCol_TabDimmedSelected] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	axColors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.26f, 0.59f, 0.98f, 0.50f);

	// Docking
	axColors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	axColors[ImGuiCol_DockingEmptyBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

	// Table
	axColors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
	axColors[ImGuiCol_TableBorderStrong] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	axColors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	axColors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	axColors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

	// Drag-drop, nav
	axColors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.90f);
	axColors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	axColors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	axColors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	axColors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

	// Convert all colors from sRGB to linear space
	// The sRGB swapchain applies linear->sRGB conversion on write,
	// so we need linear input values to get the correct perceptual output
	for (int i = 0; i < ImGuiCol_COUNT; i++)
	{
		axColors[i].x = powf(axColors[i].x, 2.2f);
		axColors[i].y = powf(axColors[i].y, 2.2f);
		axColors[i].z = powf(axColors[i].z, 2.2f);
	}
}

void Zenith_Editor::ConfigureImGuiIniPath()
{
	// D3D12 null backend: no ImGui context is ever created — nothing to do.
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}

	ImGuiIO& xIO = ImGui::GetIO();

	// Automated runs must get the deterministic code-built dock layout: any
	// imgui.ini load would make windowed test layouts depend on whatever ini
	// the cwd or user profile happens to hold.
	if (Zenith_CommandLine::IsHeadless()
		|| Zenith_CommandLine::IsAutomatedTestRun()
		|| Zenith_CommandLine::IsImGuiIniDisabled())
	{
		xIO.IniFilename = nullptr;
		return;
	}

	// Interactive runs persist layout per-game OUTSIDE the repo:
	// %LOCALAPPDATA%/Zenith/<GameName>/imgui.ini (same shape as
	// Zenith_SaveData's %APPDATA%/Zenith/<GameName>/). ImGui stores the
	// IniFilename POINTER without copying — the buffer must outlive
	// DestroyContext, hence the file-scope static.
#ifdef ZENITH_WINDOWS
	extern const char* Project_GetName();
	static char s_acImGuiIniPath[ZENITH_MAX_PATH_LENGTH] = {};

	char acLocalAppData[ZENITH_MAX_PATH_LENGTH] = {};
	const DWORD uLen = GetEnvironmentVariableA("LOCALAPPDATA", acLocalAppData, sizeof(acLocalAppData));
	if (uLen == 0 || uLen >= sizeof(acLocalAppData))
	{
		xIO.IniFilename = nullptr;
		return;
	}

	char acDir[ZENITH_MAX_PATH_LENGTH] = {};
	snprintf(acDir, sizeof(acDir), "%s/Zenith/%s", acLocalAppData, Project_GetName());
	std::error_code xError;
	std::filesystem::create_directories(acDir, xError);
	if (xError)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "Failed to create ImGui ini dir '%s' (%s) — layout persistence disabled", acDir, xError.message().c_str());
		xIO.IniFilename = nullptr;
		return;
	}

	snprintf(s_acImGuiIniPath, sizeof(s_acImGuiIniPath), "%s/imgui.ini", acDir);
	xIO.IniFilename = s_acImGuiIniPath;
#else
	xIO.IniFilename = nullptr;
#endif
}

// Code-built default dock layout. Runs when no saved layout exists (fresh
// machine / ini disabled) or on View > Reset Layout. Split ratios are
// resolution-independent; the node size seed just resolves them. Every
// dockable window is docked — including default-hidden ones (Terrain Editor,
// Memory Profiler) so toggling them on lands in a sensible slot instead of
// floating. The desired FRONT tab of each group is docked LAST.
static void BuildDefaultDockLayout(ImGuiID uDockspaceID, const ImGuiViewport* pxViewport)
{
	ImGui::DockBuilderRemoveNode(uDockspaceID);
	// The DockSpace/NoTabBar enumerators live in the PRIVATE dock-node flag
	// enum — explicit casts to the int typedef avoid C5054 (mixed-enum '|')
	// under /WX.
	ImGui::DockBuilderAddNode(uDockspaceID,
		static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_DockSpace) | static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_PassthruCentralNode));
	ImGui::DockBuilderSetNodeSize(uDockspaceID, pxViewport->WorkSize);

	// Carve the outer columns first, then slice the centre.
	ImGuiID uCentre = uDockspaceID;
	const ImGuiID uLeft        = ImGui::DockBuilderSplitNode(uCentre, ImGuiDir_Left,  0.18f, nullptr, &uCentre);
	ImGuiID uRight             = ImGui::DockBuilderSplitNode(uCentre, ImGuiDir_Right, 0.25f, nullptr, &uCentre);
	const ImGuiID uRightBottom = ImGui::DockBuilderSplitNode(uRight,  ImGuiDir_Down,  0.45f, nullptr, &uRight);
	ImGuiID uBottom            = ImGui::DockBuilderSplitNode(uCentre, ImGuiDir_Down,  0.28f, nullptr, &uCentre);
	const ImGuiID uBottomRight = ImGui::DockBuilderSplitNode(uBottom, ImGuiDir_Right, 0.30f, nullptr, &uBottom);
	const ImGuiID uToolbar     = ImGui::DockBuilderSplitNode(uCentre, ImGuiDir_Up,    0.08f, nullptr, &uCentre);

	// Toolbar reads as a strip, not a tabbed window; Viewport hides its tab
	// bar (matches the layout the team has been using from the old DP ini).
	if (ImGuiDockNode* pxToolbarNode = ImGui::DockBuilderGetNode(uToolbar))
	{
		pxToolbarNode->SetLocalFlags(
			static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoTabBar)
			| static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoDockingSplit)
			| static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoDockingOverMe));
	}
	if (ImGuiDockNode* pxCentreNode = ImGui::DockBuilderGetNode(uCentre))
	{
		pxCentreNode->SetLocalFlags(ImGuiDockNodeFlags_HiddenTabBar);
	}

	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_HIERARCHY,       uLeft);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_TOOLBAR,         uToolbar);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_VIEWPORT,        uCentre);

	// Right column: Properties group (Terrain Editor + Material Editor tab with
	// Properties; Properties docked last so it fronts by default). The Material
	// Editor lives here (a tall panel, UE-style) rather than the short bottom
	// strip so its live IBL preview + grouped property foldouts have vertical
	// room; selecting a material fronts its tab. Tools group below.
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_TERRAIN_EDITOR,  uRight);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_MATERIAL_EDITOR, uRight);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_PROPERTIES,      uRight);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_RENDER_GRAPH,    uRightBottom);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_ZENITH_TOOLS,    uRightBottom);

	// Bottom strip: browser group (Content Browser fronts), Console right.
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_VARIANT_EDITOR,  uBottom);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_MEMORY_PROFILER, uBottom);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_PROFILING,       uBottom);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_CONTENT_BROWSER, uBottom);
	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_CONSOLE,         uBottomRight);

	ImGui::DockBuilderFinish(uDockspaceID);

	// Dock order alone doesn't decide which tab fronts — windows created
	// LATER in the frame (Profiling begins after Render()) steal selection
	// as they first dock in. Pin the front tab of each multi-tab group
	// explicitly; a root window's tab id is the hash of its name.
	if (ImGuiDockNode* pxBottomNode = ImGui::DockBuilderGetNode(uBottom))
	{
		pxBottomNode->SelectedTabId = ImHashStr(szEDITOR_WINDOW_CONTENT_BROWSER);
	}
	if (ImGuiDockNode* pxRightNode = ImGui::DockBuilderGetNode(uRight))
	{
		pxRightNode->SelectedTabId = ImHashStr(szEDITOR_WINDOW_PROPERTIES);
	}
	if (ImGuiDockNode* pxRightBottomNode = ImGui::DockBuilderGetNode(uRightBottom))
	{
		pxRightBottomNode->SelectedTabId = ImHashStr(szEDITOR_WINDOW_ZENITH_TOOLS);
	}
}

void Zenith_Editor::Initialise(Flux_PlatformAPI& xFluxBackend, Flux_GraphicsImpl& xFluxGraphics, FrameContext& xFrame,
	Zenith_DebugVariables& xDebugVariables, Zenith_Profiling& xProfiling, Zenith_TerrainEditor& xTerrainEditor)
{
	// Cache the injected frame deps for RenderImGuiFrame (see header).
	m_pxFluxBackend    = &xFluxBackend;
	m_pxFluxGraphics   = &xFluxGraphics;
	m_pxFrame          = &xFrame;
	m_pxDebugVariables = &xDebugVariables;
	m_pxProfiling      = &xProfiling;
	m_pxTerrainEditor  = &xTerrainEditor;

	// Must run between ImGui::CreateContext (Zenith_Init) and the first
	// NewFrame — ImGui reads io.IniFilename at first NewFrame.
	ConfigureImGuiIniPath();

	ApplyEditorTheme();

	// Initialize content browser to game assets directory
	m_xEditorState.m_xContentBrowser.m_strCurrentDirectory = Project_GetGameAssetsDirectory();

	m_xEditorState.m_eEditorMode = EditorMode::Stopped;
	m_xEditorState.m_xSelection.m_xSelectedEntityIDs.clear();
	m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	m_xEditorState.m_xSelection.m_uLastClickedEntityID = INVALID_ENTITY_ID;
	m_xEditorState.m_eGizmoMode = EditorGizmoMode::Translate;

	// Material system is now managed by Zenith_AssetRegistry

	// Initialize editor subsystems
	g_xEngine.Selection().Initialise();
	// Zenith_AnimationStateMachineEditor::Initialize();  // TEMPORARILY DISABLED

	// Editor camera initialisation is deferred to Update() - Initialise() runs
	// before InitialiseProject(), so the scene's main camera doesn't exist yet.
}

// File-local helper for RenderImGuiFrame: recursively draws the debug-variable
// tree into the legacy "Zenith Tools" window. (Relocated from Zenith_Core.cpp,
// where it leaked external linkage.)
static void TraverseTree(Zenith_DebugVariableTree::Node* pxNode, uint32_t uCurrentDepth)
{
	ImGui::PushID(pxNode);

	if (!ImGui::CollapsingHeader(pxNode->m_xName.Get(uCurrentDepth).c_str()))
	{
		ImGui::PopID();
		return;
	}

	ImGui::Indent();

	for (Zenith_DebugVariableTree::LeafNodeBase* pxLeaf : pxNode->m_xLeaves)
	{
		pxLeaf->ImGuiDisplay();
	}
	for (Zenith_DebugVariableTree::Node* pxChild : pxNode->m_xChildren)
	{
		TraverseTree(pxChild, uCurrentDepth + 1);
	}

	ImGui::Unindent();
	ImGui::PopID();
}

void Zenith_Editor::RenderImGuiFrame()
{
	// Deps are wired in Initialise(), which the engine only runs windowed --
	// the same condition under which the main loop reaches this call.
	Zenith_Assert(m_pxFluxBackend != nullptr, "RenderImGuiFrame called before Initialise");

	// NOTE: simulated input reaches ImGui via g_pfnZenithImGuiSimulatedInput,
	// invoked INSIDE ImGuiBeginFrame between the GLFW-backend NewFrame and
	// ImGui::NewFrame - after the backend's real-cursor poll, so simulated
	// events deterministically win while the simulator is enabled.
	m_pxFluxBackend->ImGuiBeginFrame();

	// Render the editor UI (includes docking, viewport, hierarchy, etc.)
	Render();

	// Also render the old debug tools window for backwards compatibility
	ImGui::Begin(szEDITOR_WINDOW_ZENITH_TOOLS);

	std::string strCamPosText = "Camera Position: " + std::to_string(static_cast<int32_t>(m_pxFluxGraphics->m_xFrameConstants.m_xCamPos_Pad.x)) + " " + std::to_string(static_cast<int32_t>(m_pxFluxGraphics->m_xFrameConstants.m_xCamPos_Pad.y)) + " " + std::to_string(static_cast<int32_t>(m_pxFluxGraphics->m_xFrameConstants.m_xCamPos_Pad.z));
	ImGui::Text(strCamPosText.c_str());

	std::string strFpsText = "FPS: " + std::to_string(1.f / m_pxFrame->GetDt());
	ImGui::Text(strFpsText.c_str());

	Zenith_DebugVariableTree& xTree = m_pxDebugVariables->m_xTree;
	Zenith_DebugVariableTree::Node* pxRoot = xTree.m_pxRoot;
	TraverseTree(pxRoot, 0);

	ImGui::End();

	// Render profiling window. Manual begin/end (rather than the
	// FUNCTION_WRAPPER macro) because RenderToImGui is a member
	// function and can't be passed as a free-function-style callable.
	{
		Zenith_Profiling::Scope xRenderProfileScope(ZENITH_PROFILE_INDEX__RENDER_IMGUI_PROFILING);
		m_pxProfiling->RenderToImGui();
	}

	// Finalize ImGui rendering data - this MUST be called before submitting the render task
	ImGui::Render();
}

void Zenith_Editor::Shutdown()
{
	// Process all pending deletions immediately on shutdown
	// At shutdown, we can safely assume all GPU work is done or will be waited for
	for (auto& pending : m_xPendingDeletions)
	{
		Flux_ImGuiIntegration::UnregisterTexture(pending.xHandle, 0); // Immediate deletion at shutdown
	}
	m_xPendingDeletions.Clear();

	// Free the cached ImGui texture handle
	if (m_xCachedGameTextureHandle.IsValid())
	{
		Flux_ImGuiIntegration::UnregisterTexture(m_xCachedGameTextureHandle, 0); // Immediate deletion at shutdown
		m_xCachedGameTextureHandle.Invalidate();
		m_xCachedImageViewHandle = Flux_ImageViewHandle();
	}

	// Close any live terrain-editing session (clears the stream-in hook).
	if (m_pxTerrainEditor != nullptr)
	{
		m_pxTerrainEditor->Close();
	}

	// Reset editor camera state
	m_xEditorState.m_xCamera.m_bInitialized = false;

	// Clear material selection (material system managed by Zenith_AssetRegistry)
	m_xEditorState.m_xMaterial.m_xSelectedMaterial.Clear();

	// Shutdown editor subsystems
	// Zenith_AnimationStateMachineEditor::Shutdown();  // TEMPORARILY DISABLED
	g_xEngine.Gizmos().Shutdown();
	g_xEngine.Selection().Shutdown();
}

bool Zenith_Editor::Update()
{
	// CRITICAL: Handle pending scene operations FIRST, before any rendering
	// This must happen here (not during RenderMainMenuBar) to avoid concurrent access
	// to scene data while render tasks are active.
	//
	// Scene loads destroy and rebuild scene data structures; if render tasks
	// were active while that happens, they would read destroyed pools.
	if (!ProcessDeferredSceneOperations())
	{
		return false;
	}

	// Process deferred ImGui texture deletions
	// We wait N frames before freeing to ensure GPU has finished using them.
	// Zenith_Vector has no iterator-erase, so walk by index and Remove() in place
	// (order-preserving); don't advance the index when an element is removed.
	{
		Zenith_Vector<PendingImGuiTextureDeletion>& xPendingDeletions = m_xPendingDeletions;
		for (u_int i = 0; i < xPendingDeletions.GetSize(); )
		{
			PendingImGuiTextureDeletion& xPending = xPendingDeletions.Get(i);
			if (xPending.uFramesUntilDeletion == 0)
			{
				// Safe to delete now - GPU has finished with this texture
				Flux_ImGuiIntegration::UnregisterTexture(xPending.xHandle, 0);
				xPendingDeletions.Remove(i);
			}
			else
			{
				// Decrement frame counter
				xPending.uFramesUntilDeletion--;
				++i;
			}
		}
	}

	// Terrain-editor service work (dirty-chunk evictions, paint-texture GPU
	// flushes, sliced erosion) runs in EVERY mode — and BEFORE the automation
	// early-return below, so automation-driven terrain editing (e.g. the
	// RenderTest terrain showcase) gets its live preview pumped while the
	// queue is still running. Unbaked edits also stay visible while
	// playtesting. NULL headless: Initialise() (which injects the pointer)
	// only runs windowed, but Update() still drives automation headless.
	if (m_pxTerrainEditor != nullptr)
	{
		m_pxTerrainEditor->ServiceUpdate();
	}

	// Execute one automation step per frame during scene generation.
	// Runs here (after pending ops, before rendering) so each step gets a full
	// frame tick — matching real editor behaviour (one action = one mouse click).
	// Returns early to skip editor camera sync and other editor logic — those
	// should only fire after automation completes and the real scene is loaded.
	if (g_xEngine.EditorAutomation().IsRunning())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
		return true;
	}

	// Lazily initialise the editor camera from the game camera. Can't happen in
	// Initialise() - that runs before InitialiseProject(), so the scene camera
	// doesn't exist yet. Also re-fires after anything resets camera state
	// (scene reset / New Scene), un-freezing the editor camera.
	// Placed after the automation check so the sync only fires once automation
	// is done and the real scene is loaded.
	if (!m_xEditorState.m_xCamera.m_bInitialized
		&& m_xEditorState.m_eEditorMode == EditorMode::Stopped)
	{
		InitializeEditorCamera();
	}

	// Update bounding boxes for all entities (needed for selection)
	g_xEngine.Selection().UpdateBoundingBoxes();

	// Update editor camera controls (when not playing)
	UpdateEditorCamera(1.0f / 60.0f);  // Assume 60fps for now, could use actual delta time

	if (m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		return true;
	}

	UpdateEditorInput();

	// Terrain editor gets first claim on viewport input: while a terrain
	// editing session is armed over the viewport (or mid-stroke), gizmo
	// interaction and object picking are skipped for the frame. RMB camera
	// look is unaffected (UpdateEditorCamera ran above). NULL headless — see
	// the ServiceUpdate note above.
	if (m_pxTerrainEditor != nullptr)
	{
		Zenith_TerrainEditorFrameContext xTerrainCtx;
		xTerrainCtx.m_bViewportHovered = m_xEditorState.m_xViewport.m_bHovered;
		xTerrainCtx.m_bViewportFocused = m_xEditorState.m_xViewport.m_bFocused;
		xTerrainCtx.m_xViewportPos = m_xEditorState.m_xViewport.m_xPosition;
		xTerrainCtx.m_xViewportSize = m_xEditorState.m_xViewport.m_xSize;
		BuildViewMatrix(xTerrainCtx.m_xViewMatrix);
		BuildProjectionMatrix(xTerrainCtx.m_xProjMatrix);
		Zenith_Maths::Vector4 xCameraPos;
		GetCameraPosition(xCameraPos);
		xTerrainCtx.m_xCameraPos = { xCameraPos.x, xCameraPos.y, xCameraPos.z };
		xTerrainCtx.m_bEditorStopped = (m_xEditorState.m_eEditorMode == EditorMode::Stopped);
		m_pxTerrainEditor->UpdatePerFrame(xTerrainCtx);
		if (m_pxTerrainEditor->ConsumedViewportInput())
		{
			return true;
		}
	}

	// Handle gizmo interaction first (before object picking)
	HandleGizmoInteraction();

	// Handle object picking (only when not manipulating gizmo)
	if (!g_xEngine.Gizmos().IsInteracting())
	{
		HandleObjectPicking();
	}

	return true;
}

bool Zenith_Editor::ProcessDeferredSceneOperations()
{
	// No GPU sync needed for any of these: Update() runs before render-task
	// submission so no render tasks are active, and every GPU resource the
	// scene teardown frees is queued through QueueVRAMDeletion's
	// MAX_FRAMES_IN_FLIGHT+1 grace period — the same contract the runtime
	// LoadScene teardown relies on mid-play.

	// Handle pending scene load (with backup-restore detection)
	if (m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad)
	{
		return HandlePendingSceneLoad();
	}

	// Handle pending registered scene load (from toolbar dropdown)
	if (m_xEditorState.m_xDeferredOps.m_bPendingRegisteredSceneLoad)
	{
		m_xEditorState.m_xDeferredOps.m_bPendingRegisteredSceneLoad = false;

		g_xEngine.Scenes().LoadSceneByIndex(m_xEditorState.m_xDeferredOps.m_iPendingRegisteredSceneBuildIndex, SCENE_LOAD_SINGLE);
		Zenith_Log(LOG_CATEGORY_EDITOR, "Registered scene (build index %d) loaded", m_xEditorState.m_xDeferredOps.m_iPendingRegisteredSceneBuildIndex);

		ClearSelection();
		g_xEngine.UndoSystem().Clear();
		m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;

		return false;
	}

	// Handle pending scene load from file path (content browser double-click)
	if (m_xEditorState.m_xDeferredOps.m_bPendingSceneLoadFromFile)
	{
		m_xEditorState.m_xDeferredOps.m_bPendingSceneLoadFromFile = false;

		g_xEngine.Scenes().LoadScene(m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadFromFilePath, SCENE_LOAD_SINGLE);
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene loaded from file: %s", m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadFromFilePath.c_str());

		ClearSelection();
		g_xEngine.UndoSystem().Clear();
		m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;
		m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadFromFilePath.clear();

		return false;
	}

	return true;
}

bool Zenith_Editor::HandlePendingSceneLoad()
{
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = false;

	// No GPU sync needed: Update() runs before render-task submission so no
	// render tasks are active, and the teardown below frees its GPU resources
	// through QueueVRAMDeletion's MAX_FRAMES_IN_FLIGHT+1 grace period — the
	// same contract the runtime LoadScene teardown relies on mid-play.

	bool bIsBackupRestore = m_xEditorState.m_xPlayBackup.m_bHasBackup && m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath == m_xEditorState.m_xPlayBackup.m_strBackupScenePath;

	// When restoring from backup (editor Stop), clean up all game scenes and persistent entities.
	// The backup handle may be stale - games using SCENE_LOAD_SINGLE during Play destroy
	// the original scene. We unload ALL non-persistent scenes unconditionally.
	if (bIsBackupRestore)
	{
		// 1. Reset all Flux render systems BEFORE destroying entities.
		// This clears Flux system state (registered meshes, particles, etc.)
		// so entity destructors don't interact with stale render system references.
		// Matches the SCENE_LOAD_SINGLE cleanup order (ResetAllRenderSystems -> UnloadAllNonPersistent).
		g_xEngine.Scenes().ResetAllRenderSystems();

		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();

		// 2. Force-unload all non-persistent scenes. Uses UnloadSceneForced to bypass
		// the "last scene" guard - after SCENE_LOAD_SINGLE during play, only one
		// game scene remains and UnloadScene would silently refuse to unload it.
		Zenith_Vector<Zenith_Scene> axScenesToUnload;
		// GetSceneAt returns INVALID_SCENE past the last visible scene, so walk
		// slot order until that sentinel (was bounded by GetLoadedSceneCount).
		for (uint32_t i = 0; ; ++i)
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
			if (!xScene.IsValid()) break;
			if (xScene == xPersistentScene) continue;
			axScenesToUnload.PushBack(xScene);
		}
		for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
		{
			g_xEngine.Scenes().UnloadSceneForced(axScenesToUnload.Get(i));
		}

		// 3. Reset persistent scene entities (destroys all entities, clears component pools)
		Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(xPersistentScene);
		if (pxPersistentData)
		{
			pxPersistentData->Reset();
		}

		// 4. Create fresh scene with the original name and restore metadata
		Zenith_Scene xRestoredScene = g_xEngine.Scenes().LoadScene(m_xEditorState.m_xPlayBackup.m_strBackupSceneName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(xRestoredScene);

		Zenith_SceneData* pxRestoredData = g_xEngine.Scenes().GetSceneData(xRestoredScene);
		if (pxRestoredData)
		{
			Zenith_EditorSceneAccess::Editor_SetPath(pxRestoredData, m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath);
			Zenith_EditorSceneAccess::Editor_SetBuildIndex(pxRestoredData, m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex);
		}
	}

	// Load the scene file into the active scene (backup restore or explicit scene load)
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (pxSceneData)
	{
		Zenith_EditorSceneAccess::LoadFromFile(pxSceneData, m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath);
	}
	Zenith_Log(LOG_CATEGORY_EDITOR, "Scene loaded from %s", m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.c_str());

	// Clear selection as entity pointers are now invalid
	ClearSelection();

	// Clear undo/redo history as entity IDs are now invalid
	g_xEngine.UndoSystem().Clear();

	// Clear game camera entity pointer as it's now invalid (entity from old scene)
	m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;

	if (bIsBackupRestore)
	{
		// Delete the temporary backup file
		std::filesystem::remove(m_xEditorState.m_xPlayBackup.m_strBackupScenePath);
		m_xEditorState.m_xPlayBackup.m_bHasBackup = false;
		m_xEditorState.m_xPlayBackup.m_strBackupScenePath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupSceneHandle = -1;
		m_xEditorState.m_xPlayBackup.m_strBackupSceneName = "";
		m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex = -1;
		Zenith_Log(LOG_CATEGORY_EDITOR, "Backup scene file cleaned up");
	}

	if (m_xEditorState.m_xCamera.m_bInitialized)
	{
		SwitchToEditorCamera();
	}

	m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.clear();

	return false;
}

void Zenith_Editor::OpenTerrainEditor(Zenith_EntityID uTerrainEntity)
{
	m_pxTerrainEditor->Open(uTerrainEntity);
	m_xEditorState.m_xPanels.m_bShowTerrainEditor = true;
}

void Zenith_Editor::UpdateEditorInput()
{
	// Handle gizmo mode keyboard shortcuts (when viewport is focused)
	if (m_xEditorState.m_xViewport.m_bFocused)
	{
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			SetGizmoMode(EditorGizmoMode::Translate);
			g_xEngine.Gizmos().SetGizmoMode(GizmoMode::Translate);
		}
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_E))
		{
			SetGizmoMode(EditorGizmoMode::Rotate);
			g_xEngine.Gizmos().SetGizmoMode(GizmoMode::Rotate);
		}
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			SetGizmoMode(EditorGizmoMode::Scale);
			g_xEngine.Gizmos().SetGizmoMode(GizmoMode::Scale);
		}
	}

	// Handle undo/redo keyboard shortcuts (Ctrl+Z / Ctrl+Y)
	// Check for Ctrl key being held down
	bool bCtrlDown = g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_CONTROL) ||
	                 g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);

	if (bCtrlDown)
	{
		// Ctrl+Z: Undo
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_Z))
		{
			g_xEngine.UndoSystem().Undo();
		}

		// Ctrl+Y: Redo
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_Y))
		{
			g_xEngine.UndoSystem().Redo();
		}
	}
}

void Zenith_Editor::Render()
{
	// Create the main docking space
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground;
	
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	
	ImGui::Begin(szEDITOR_WINDOW_DOCKSPACE_HOST, nullptr, window_flags);
	ImGui::PopStyleVar(3);

	// Create dockspace. When no layout exists yet (fresh machine, ini load
	// disabled for automated runs, or the user asked for a reset), build the
	// code-defined default — ini settings materialize into dock nodes during
	// the first NewFrame, so a missing root node here ⇔ "no saved layout".
	ImGuiID dockspace_id = ImGui::GetID(szEDITOR_DOCKSPACE_ID);
	if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || m_xEditorState.m_bResetDockLayout)
	{
		m_xEditorState.m_bResetDockLayout = false;
		BuildDefaultDockLayout(dockspace_id, viewport);
		// Windows created later in the build frame (Profiling begins after
		// Render()) steal tab selection as they dock in, overriding the
		// builder's SelectedTabId. Re-front the intended tab NEXT frame,
		// once every window exists (SetWindowFocus is by-name lookup).
		m_uFrontDefaultTabsCountdown = 2;
	}
	if (m_uFrontDefaultTabsCountdown > 0 && --m_uFrontDefaultTabsCountdown == 0)
	{
		ImGui::SetWindowFocus(szEDITOR_WINDOW_CONTENT_BROWSER);
	}
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	
	RenderMainMenuBar();
	ImGui::End();
	
	// Render editor panels
	RenderToolbar();
	if (m_xEditorState.m_xPanels.m_bShowHierarchy) RenderHierarchyPanel();
	if (m_xEditorState.m_xPanels.m_bShowProperties) RenderPropertiesPanel();
	RenderViewport();
	RenderContentBrowser();
	if (m_xEditorState.m_xPanels.m_bShowConsole) RenderConsolePanel();
	RenderMaterialEditorPanel();

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
	Zenith_EditorPanelMemory::Render();
#endif

	Zenith_EditorPanelRenderGraph::Render();
	Zenith_EditorPanelVariantEditor::Render();
	Zenith_GraphEditorPanel::Render();
	if (m_pxTerrainEditor != nullptr)
	{
		Zenith_EditorPanelTerrainEditor::Render(*m_pxTerrainEditor, m_xEditorState.m_xPanels.m_bShowTerrainEditor);
	}

	// Animation state machine editor
	// Zenith_AnimationStateMachineEditor::Render();  // TEMPORARILY DISABLED

	// Render gizmos and overlays (after viewport so they appear on top)
	RenderGizmos();
}

// RenderMainMenuBar + per-menu helpers (RenderFileMenu / RenderEditMenu / RenderViewMenu)
// live in Zenith_Editor_Menu.cpp.

void Zenith_Editor::RenderToolbar()
{
	Zenith_EditorPanelToolbar::Render(m_xEditorState.m_eEditorMode, m_xEditorState.m_eGizmoMode);
}

void Zenith_Editor::RenderHierarchyPanel()
{
	Zenith_EditorPanelHierarchy::Render(m_xEditorState.m_xCamera.m_uGameCameraEntity);
}

void Zenith_Editor::RenderPropertiesPanel()
{
	Zenith_EditorPanelProperties::Render(GetSelectedEntity(), m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
}

void Zenith_Editor::RenderViewport()
{
	ViewportState xState = {
		m_xEditorState.m_xViewport.m_xSize,
		m_xEditorState.m_xViewport.m_xPosition,
		m_xEditorState.m_xViewport.m_bHovered,
		m_xEditorState.m_xViewport.m_bFocused,
		m_xCachedGameTextureHandle,
		m_xCachedImageViewHandle,
		m_xPendingDeletions
	};
	Zenith_EditorPanelViewport::Render(xState);
}

void Zenith_Editor::HandleObjectPicking()
{
	// Only pick when viewport is hovered
	if (!m_xEditorState.m_xViewport.m_bHovered)
		return;

	// Only pick on left mouse button press (not held)
	if (!g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		return;

	// Get mouse position in screen space
	Zenith_Maths::Vector2_64 xGlobalMousePos;
	g_xEngine.Input().GetMousePosition(xGlobalMousePos);

	// Convert to viewport-relative coordinates
	Zenith_Maths::Vector2 xViewportMousePos = {
		static_cast<float>(xGlobalMousePos.x - m_xEditorState.m_xViewport.m_xPosition.x),
		static_cast<float>(xGlobalMousePos.y - m_xEditorState.m_xViewport.m_xPosition.y)
	};

	// Check if mouse is within viewport bounds
	if (xViewportMousePos.x < 0 || xViewportMousePos.x > m_xEditorState.m_xViewport.m_xSize.x ||
		xViewportMousePos.y < 0 || xViewportMousePos.y > m_xEditorState.m_xViewport.m_xSize.y)
		return;

	// Get camera matrices for ray casting
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	BuildViewMatrix(xViewMatrix);
	BuildProjectionMatrix(xProjMatrix);

	// Convert screen position to world-space ray
	Zenith_Maths::Vector3 xRayDir = g_xEngine.Gizmo().ScreenToWorldRay(
		xViewportMousePos,
		{ 0, 0 },  // Viewport relative, so offset is 0
		m_xEditorState.m_xViewport.m_xSize,
		xViewMatrix,
		xProjMatrix
	);

	// Ray origin is camera position
	Zenith_Maths::Vector4 xCameraPos;
	GetCameraPosition(xCameraPos);
	Zenith_Maths::Vector3 xRayOrigin(xCameraPos.x, xCameraPos.y, xCameraPos.z);

	// Perform raycast to find entity under mouse - now returns EntityID
	Zenith_EntityID uHitEntityID = g_xEngine.Selection().RaycastSelect(xRayOrigin, xRayDir);

	if (uHitEntityID != INVALID_ENTITY_ID)
	{
		SelectEntity(uHitEntityID);
	}
	else
	{
		ClearSelection();
	}
}

void Zenith_Editor::RenderGizmos()
{
	// Set target entity and gizmo mode for Flux_Gizmos
	// Task must always be submitted once per frame (even if null) for proper synchronization
	Zenith_Entity* pxSelectedEntity = nullptr;

	// Only render gizmos in Stopped or Paused mode (not during active play)
	if (m_xEditorState.m_eEditorMode != EditorMode::Playing)
	{
		pxSelectedEntity = GetSelectedEntity();
	}

	// CRITICAL: Only update target/mode when NOT interacting!
	// SetTargetEntity and SetGizmoMode reset s_bIsInteracting, which would
	// break mid-drag operations. Only update when safe to do so.
	if (!g_xEngine.Gizmos().IsInteracting())
	{
		g_xEngine.Gizmos().SetTargetEntity(pxSelectedEntity);
		g_xEngine.Gizmos().SetGizmoMode(static_cast<GizmoMode>(m_xEditorState.m_eGizmoMode));
	}

	// Gizmos are now part of the render graph - no separate task submission needed
}

void Zenith_Editor::HandleGizmoInteraction()
{
	// Only handle gizmo interaction when viewport is hovered and entity selected
	if (!m_xEditorState.m_xViewport.m_bHovered || !HasSelection())
		return;

	// Only handle in Stopped or Paused mode
	if (m_xEditorState.m_eEditorMode == EditorMode::Playing)
		return;

	// Get camera matrices for ray casting
	Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
	BuildViewMatrix(xViewMatrix);
	BuildProjectionMatrix(xProjMatrix);

	// Get mouse position
	Zenith_Maths::Vector2_64 xGlobalMousePos;
	g_xEngine.Input().GetMousePosition(xGlobalMousePos);

	Zenith_Maths::Vector2 xViewportMousePos = {
		static_cast<float>(xGlobalMousePos.x - m_xEditorState.m_xViewport.m_xPosition.x),
		static_cast<float>(xGlobalMousePos.y - m_xEditorState.m_xViewport.m_xPosition.y)
	};

	// Debug: log the mouse position roughly once a second while interacting.
	// Throttled off the engine frame index (FrameContext) rather than a local
	// counter — one frame-index variable engine-wide.
	if (g_xEngine.Gizmos().IsInteracting())
	{
		if (g_xEngine.Frame().GetFrameIndex() % 60 == 0)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Mouse: Global=(%.1f,%.1f), Viewport=(%.1f,%.1f)",
				xGlobalMousePos.x, xGlobalMousePos.y,
				xViewportMousePos.x, xViewportMousePos.y);
		}
	}

	// Convert screen position to world-space ray
	Zenith_Maths::Vector3 xRayDir = g_xEngine.Gizmo().ScreenToWorldRay(
		xViewportMousePos,
		{ 0, 0 },
		m_xEditorState.m_xViewport.m_xSize,
		xViewMatrix,
		xProjMatrix
	);

	// Ray origin is camera position
	Zenith_Maths::Vector4 xCameraPos;
	GetCameraPosition(xCameraPos);
	Zenith_Maths::Vector3 xRayOrigin(xCameraPos.x, xCameraPos.y, xCameraPos.z);

	// Handle mouse input for gizmo interaction
	if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Mouse left pressed - viewport hovered=%d, selected=%zu", m_xEditorState.m_xViewport.m_bHovered, m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size());
		g_xEngine.Gizmos().BeginInteraction(xRayOrigin, xRayDir);
		Zenith_Log(LOG_CATEGORY_EDITOR, "After BeginInteraction: IsInteracting=%d", g_xEngine.Gizmos().IsInteracting());
	}
	
	// Update interaction while dragging (can happen same frame as BeginInteraction)
	bool bIsKeyDown = g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT);
	bool bIsInteracting = g_xEngine.Gizmos().IsInteracting();
	
	if (bIsKeyDown || bIsInteracting)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Check UpdateInteraction: IsKeyDown=%d, IsInteracting=%d", bIsKeyDown, bIsInteracting);
	}
	
	if (bIsKeyDown && bIsInteracting)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Calling UpdateInteraction: ViewportMouse=(%.1f,%.1f)",
			xViewportMousePos.x, xViewportMousePos.y);
		g_xEngine.Gizmos().UpdateInteraction(xRayOrigin, xRayDir);
	}
	
	// End interaction when mouse released
	if (!g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT) && g_xEngine.Gizmos().IsInteracting())
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Ending interaction");
		g_xEngine.Gizmos().EndInteraction();
	}
}

// STOPPED -> PLAYING: backup scene state, locate the game camera, then dispatch
// Unity-style OnAwake / OnEnable / OnStart for every active entity (in three
// passes — OnAwake first across all, then OnEnable + MarkAwoken, then OnStart
// re-fetching the active list since OnAwake may have created entities).
// Returns false if no active scene data is loaded — caller must restore the
// prior mode.
bool Zenith_Editor::EnterPlayMode()
{
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Entering Play Mode");

	m_xEditorState.m_xPlayBackup.m_strBackupScenePath = std::filesystem::temp_directory_path().string() + "/zenith_scene_backup" ZENITH_SCENE_EXT;

	// Persistent entities only — transient entities have runtime-only resources
	// (procedural meshes) that can't serialise, and behaviour scripts will
	// regenerate them in OnStart (running below). Including them would
	// duplicate after OnStart re-creates them on restore.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: No active scene data, cannot enter Play Mode");
		return false;
	}

	Zenith_EditorSceneAccess::SaveToFile(pxSceneData, m_xEditorState.m_xPlayBackup.m_strBackupScenePath, false);
	m_xEditorState.m_xPlayBackup.m_bHasBackup = true;
	m_xEditorState.m_xPlayBackup.m_iBackupSceneHandle = xActiveScene.GetHandle();
	const Zenith_SceneInfo xSceneInfo = g_xEngine.Scenes().GetSceneInfo(xActiveScene);
	m_xEditorState.m_xPlayBackup.m_strBackupSceneName = xSceneInfo.m_strName;
	m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath = xSceneInfo.m_strPath;
	m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex = xSceneInfo.m_iBuildIndex;

	Zenith_Log(LOG_CATEGORY_EDITOR, "Scene state backed up to: %s", m_xEditorState.m_xPlayBackup.m_strBackupScenePath.c_str());

	m_xEditorState.m_xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();
	if (m_xEditorState.m_xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
	{
		Zenith_Vector<Zenith_CameraComponent*> xCameras;
		Zenith_EditorSceneAccess::GetAllOfComponentType<Zenith_CameraComponent>(pxSceneData, xCameras);

		for (Zenith_Vector<Zenith_CameraComponent*>::Iterator xIt(xCameras); !xIt.Done(); xIt.Next())
		{
			Zenith_CameraComponent* pxCam = xIt.GetData();
			Zenith_Entity* pxEntity = &pxCam->GetParentEntity();
			m_xEditorState.m_xCamera.m_uGameCameraEntity = pxEntity->GetEntityID();
			Zenith_EditorSceneAccess::SetMainCameraEntity(pxSceneData, m_xEditorState.m_xCamera.m_uGameCameraEntity);
			break;
		}
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Dispatching OnAwake/OnEnable for %u entities", Zenith_EditorSceneAccess::GetEntityCount(pxSceneData));
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	const Zenith_Vector<Zenith_EntityID>& xEntityIDs = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			xRegistry.DispatchOnAwake(xEntity);
		}
	}

	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnEnable(xEntity);
			}
			pxSceneData->MarkEntityAwoken(uID);
		}
	}

	// Third pass: OnStart for enabled entities (Unity-style: called before first
	// Update). Re-fetch the active entity list since OnAwake/OnEnable may have
	// created new entities.
	const Zenith_Vector<Zenith_EntityID>& xStartEntityIDs = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xStartEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xStartEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnStart(xEntity);
			}
			Zenith_EditorSceneAccess::Editor_MarkEntityStarted(pxSceneData, uID);
		}
	}
	return true;
}

// PLAYING/PAUSED -> STOPPED: queue a deferred scene-restore from backup.
// CRITICAL: must defer to next frame's Update(); loading mid-frame would have
// SubmitRenderTasks try to render new terrain components before render systems
// have registered them.
void Zenith_Editor::EnterStopMode()
{
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Stopping Play Mode");

	if (m_xEditorState.m_xPlayBackup.m_bHasBackup && !m_xEditorState.m_xPlayBackup.m_strBackupScenePath.empty())
	{
		m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = true;
		m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath = m_xEditorState.m_xPlayBackup.m_strBackupScenePath;
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene restore queued for next frame: %s", m_xEditorState.m_xPlayBackup.m_strBackupScenePath.c_str());
		// m_xEditorState.m_xPlayBackup.m_bHasBackup / m_xEditorState.m_xPlayBackup.m_strBackupScenePath cleared in Update() after the load completes.
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: No scene backup available to restore");
	}

	// Clear the game camera reference since scene will be reloaded.
	m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;
}

void Zenith_Editor::SetEditorMode(EditorMode eMode)
{
	if (m_xEditorState.m_eEditorMode == eMode) return;

	EditorMode oldMode = m_xEditorState.m_eEditorMode;
	m_xEditorState.m_eEditorMode = eMode;

	if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
	{
		// Dispatcher owns the mode revert on failure so EnterPlayMode is a
		// pure transition routine — single source of truth for mode state.
		if (!EnterPlayMode())
		{
			m_xEditorState.m_eEditorMode = oldMode;
			return;
		}
	}
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		EnterStopMode();
	}
	else if (eMode == EditorMode::Paused)
	{
		// Stay on game camera during pause so player can see game state.
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Pausing - physics and scene updates suspended");
	}
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Resuming - physics and scene updates resumed");
	}
}

void Zenith_Editor::RequestLoadRegisteredScene(int iBuildIndex)
{
	m_xEditorState.m_xDeferredOps.m_bPendingRegisteredSceneLoad = true;
	m_xEditorState.m_xDeferredOps.m_iPendingRegisteredSceneBuildIndex = iBuildIndex;
}

void Zenith_Editor::RequestLoadSceneFromFile(const std::string& strPath)
{
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoadFromFile = true;
	m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadFromFilePath = strPath;
}

// Synchronously flush the staging buffer, wait for GPU idle, drain deferred
// deletions, and clear pending command lists. Required before any operation
// that destroys GPU resources still in flight (scene reset, scene load).
void Zenith_Editor::WaitForGPUAndFlushDeferred(const char* szReason)
{
	// Headless mode (Zenith_CommandLine::IsHeadless()): Flux::EarlyInitialise is
	// skipped, so command buffers / allocator / device are never created. Every
	// call below would assert. The semantics ("ensure GPU is idle before
	// destroying resources") collapse to a no-op when there is no GPU.
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Flushing staging buffer...");
	g_xEngine.FluxMemory().Flush();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for GPU idle before %s...", szReason);
	g_xEngine.FluxBackend().WaitForGPUIdle();

	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		g_xEngine.FluxMemory().ProcessDeferredDeletions();
	}
	g_xEngine.FluxRenderer().ClearPendingCommandLists();
}

// Pending scene load: flush GPU; if this is the editor's stop-mode backup
// restore, force-unload every non-persistent scene and reset the persistent
// scene's entities (game's SCENE_LOAD_SINGLE may have destroyed the original
// scene during play, leaving a stale backup handle); then create a fresh
// scene and load the file into it. Also handles plain (non-backup) loads.
void Zenith_Editor::HandlePendingSceneLoadDeferred()
{
	if (!m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad) return;
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = false;

	WaitForGPUAndFlushDeferred("scene load");

	const bool bIsBackupRestore = m_xEditorState.m_xPlayBackup.m_bHasBackup && m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath == m_xEditorState.m_xPlayBackup.m_strBackupScenePath;

	if (bIsBackupRestore)
	{
		// 1. Reset all Flux render systems BEFORE destroying entities.
		g_xEngine.Scenes().ResetAllRenderSystems();

		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();

		// 2. Force-unload all non-persistent scenes. UnloadSceneForced bypasses
		// the "last scene" guard — after SCENE_LOAD_SINGLE during play only one
		// game scene remains and UnloadScene would silently refuse to unload it.
		Zenith_Vector<Zenith_Scene> axScenesToUnload;
		// GetSceneAt returns INVALID_SCENE past the last visible scene, so walk
		// slot order until that sentinel (was bounded by GetLoadedSceneCount).
		for (uint32_t i = 0; ; ++i)
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
			if (!xScene.IsValid()) break;
			if (xScene == xPersistentScene) continue;
			axScenesToUnload.PushBack(xScene);
		}
		for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
		{
			g_xEngine.Scenes().UnloadSceneForced(axScenesToUnload.Get(i));
		}

		// 3. Reset persistent scene entities (clears component pools).
		Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(xPersistentScene);
		if (pxPersistentData)
		{
			pxPersistentData->Reset();
		}

		// 4. Create a fresh scene with the original name and restore metadata.
		Zenith_Scene xRestoredScene = g_xEngine.Scenes().LoadScene(m_xEditorState.m_xPlayBackup.m_strBackupSceneName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(xRestoredScene);

		Zenith_SceneData* pxRestoredData = g_xEngine.Scenes().GetSceneData(xRestoredScene);
		if (pxRestoredData)
		{
			Zenith_EditorSceneAccess::Editor_SetPath(pxRestoredData, m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath);
			Zenith_EditorSceneAccess::Editor_SetBuildIndex(pxRestoredData, m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex);
		}
	}

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (pxSceneData)
	{
		Zenith_EditorSceneAccess::LoadFromFile(pxSceneData, m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath);
	}
	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Scene loaded from %s", m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.c_str());

	ClearSelection();
	g_xEngine.UndoSystem().Clear();
	m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;

	if (bIsBackupRestore)
	{
		std::filesystem::remove(m_xEditorState.m_xPlayBackup.m_strBackupScenePath);
		m_xEditorState.m_xPlayBackup.m_bHasBackup = false;
		m_xEditorState.m_xPlayBackup.m_strBackupScenePath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupSceneHandle = -1;
		m_xEditorState.m_xPlayBackup.m_strBackupSceneName = "";
		m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex = -1;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Backup scene file cleaned up");
	}

	if (m_xEditorState.m_xCamera.m_bInitialized)
	{
		SwitchToEditorCamera();
	}

	m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.clear();
}

void Zenith_Editor::FlushPendingSceneOperations()
{
	HandlePendingSceneLoadDeferred();
}

//------------------------------------------------------------------------------
// Multi-Select System Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::SelectEntity(Zenith_EntityID uEntityID, bool bAddToSelection)
{
	if (uEntityID == INVALID_ENTITY_ID)
	{
		return;
	}

	if (bAddToSelection)
	{
		// Add to existing selection
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.insert(uEntityID);
	}
	else
	{
		// Replace selection
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.clear();
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.insert(uEntityID);
	}

	// Update primary selection and last clicked
	m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = uEntityID;
	m_xEditorState.m_xSelection.m_uLastClickedEntityID = uEntityID;

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Selected entity %u (total: %zu)", uEntityID, m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size());

	// Update Flux_Gizmos target entity (primary selection)
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity)
	{
		g_xEngine.Gizmos().SetTargetEntity(pxEntity);
	}
}

void Zenith_Editor::SelectRange(Zenith_EntityID uEndEntityID)
{
	if (m_xEditorState.m_xSelection.m_uLastClickedEntityID == INVALID_ENTITY_ID || uEndEntityID == INVALID_ENTITY_ID)
	{
		// No start point for range, just select the end entity
		SelectEntity(uEndEntityID, false);
		return;
	}

	// For range selection, we need to select all entities "between" start and end
	// Since entity IDs may not be contiguous, we iterate through the active entities
	// and select all entities with indices in the range [min(start,end), max(start,end)]
	uint32_t uStartIndex = std::min(m_xEditorState.m_xSelection.m_uLastClickedEntityID.m_uIndex, uEndEntityID.m_uIndex);
	uint32_t uEndIndex = std::max(m_xEditorState.m_xSelection.m_uLastClickedEntityID.m_uIndex, uEndEntityID.m_uIndex);

	// Clear existing selection for shift+click (standard behavior)
	m_xEditorState.m_xSelection.m_xSelectedEntityIDs.clear();

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	// Select all entities in the index range that exist in the scene
	const Zenith_Vector<Zenith_EntityID>& xActiveEntities = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xActiveEntities.Get(u);
		if (xEntityID.m_uIndex >= uStartIndex && xEntityID.m_uIndex <= uEndIndex)
		{
			m_xEditorState.m_xSelection.m_xSelectedEntityIDs.insert(xEntityID);
		}
	}

	// Update primary selection to the end entity
	m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = uEndEntityID;
	// Keep m_xEditorState.m_xSelection.m_uLastClickedEntityID unchanged for further range selections

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Range selected %zu entities", m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size());

	// Update Flux_Gizmos target entity
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity)
	{
		g_xEngine.Gizmos().SetTargetEntity(pxEntity);
	}
}

void Zenith_Editor::ToggleEntitySelection(Zenith_EntityID uEntityID)
{
	if (uEntityID == INVALID_ENTITY_ID)
	{
		return;
	}

	auto it = m_xEditorState.m_xSelection.m_xSelectedEntityIDs.find(uEntityID);
	if (it != m_xEditorState.m_xSelection.m_xSelectedEntityIDs.end())
	{
		// Already selected - deselect
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.erase(it);

		// Update primary selection if we just removed it
		if (m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID == uEntityID)
		{
			m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = m_xEditorState.m_xSelection.m_xSelectedEntityIDs.empty() ?
				INVALID_ENTITY_ID : *m_xEditorState.m_xSelection.m_xSelectedEntityIDs.begin();
		}

		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Deselected entity %u (total: %zu)", uEntityID, m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size());
	}
	else
	{
		// Not selected - add to selection
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.insert(uEntityID);
		m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = uEntityID;

		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Added entity %u to selection (total: %zu)", uEntityID, m_xEditorState.m_xSelection.m_xSelectedEntityIDs.size());
	}

	// Update last clicked for range selection
	m_xEditorState.m_xSelection.m_uLastClickedEntityID = uEntityID;

	// Update Flux_Gizmos target entity
	Zenith_Entity* pxEntity = GetSelectedEntity();
	g_xEngine.Gizmos().SetTargetEntity(pxEntity);
}

void Zenith_Editor::ClearSelection()
{
	m_xEditorState.m_xSelection.m_xSelectedEntityIDs.clear();
	m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	m_xEditorState.m_xSelection.m_uLastClickedEntityID = INVALID_ENTITY_ID;
	g_xEngine.Gizmos().SetTargetEntity(nullptr);
}

void Zenith_Editor::DeselectEntity(Zenith_EntityID uEntityID)
{
	m_xEditorState.m_xSelection.m_xSelectedEntityIDs.erase(uEntityID);

	// Update primary selection if we deselected it
	if (m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID == uEntityID)
	{
		m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = m_xEditorState.m_xSelection.m_xSelectedEntityIDs.empty() ?
			INVALID_ENTITY_ID : *m_xEditorState.m_xSelection.m_xSelectedEntityIDs.begin();
	}

	// Update gizmo target
	Zenith_Entity* pxEntity = GetSelectedEntity();
	g_xEngine.Gizmos().SetTargetEntity(pxEntity);
}

bool Zenith_Editor::IsSelected(Zenith_EntityID uEntityID)
{
	return m_xEditorState.m_xSelection.m_xSelectedEntityIDs.find(uEntityID) != m_xEditorState.m_xSelection.m_xSelectedEntityIDs.end();
}

Zenith_Entity* Zenith_Editor::GetSelectedEntity()
{
	if (m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID == INVALID_ENTITY_ID)
		return nullptr;

	// Search all loaded scenes for the entity (not just active scene)
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
	if (!pxSceneData)
	{
		// Entity no longer exists in any scene - remove from selection
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.erase(m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
		m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = m_xEditorState.m_xSelection.m_xSelectedEntityIDs.empty() ?
			INVALID_ENTITY_ID : *m_xEditorState.m_xSelection.m_xSelectedEntityIDs.begin();
		return nullptr;
	}

	// Return pointer to static entity handle (valid until next call)
	static Zenith_Entity s_xSelectedEntity;
	s_xSelectedEntity = pxSceneData->GetEntity(m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
	return &s_xSelectedEntity;
}

//------------------------------------------------------------------------------
// Editor Operations (shared between ImGui panels and automation)
//------------------------------------------------------------------------------

Zenith_EntityID Zenith_Editor::CreateEntity(const char* szName)
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Assert(pxData, "No active scene data");

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxData, szName);
	xEntity.SetTransient(false);
	Zenith_EntityID uID = xEntity.GetEntityID();
	SelectEntity(uID);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Created entity '%s' (ID: %u)", szName, uID);
	return uID;
}

void Zenith_Editor::SelectEntityByName(const char* szName)
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Assert(pxData, "No active scene data");

	Zenith_Entity xEntity = pxData->FindEntityByName(szName);
	Zenith_Assert(xEntity.IsValid(), "Entity not found: %s", szName);
	SelectEntity(xEntity.GetEntityID());

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Selected entity '%s'", szName);
}

void Zenith_Editor::SetSelectedEntityTransient(bool bTransient)
{
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Zenith_Assert(pxEntity, "No entity selected");
	pxEntity->SetTransient(bTransient);
}

bool Zenith_Editor::AddComponentToSelected(const char* szDisplayName)
{
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Zenith_Assert(pxEntity, "No entity selected");

	Zenith_ComponentEditorRegistry& xRegistry = Zenith_ComponentEditorRegistry::Get();
	const auto& xEntries = xRegistry.GetEntries();

	for (u_int i = 0; i < xEntries.GetSize(); ++i)
	{
		if (xEntries.Get(i).m_strDisplayName == szDisplayName)
		{
			bool bSuccess = xRegistry.TryAddComponent(i, *pxEntity);
			if (bSuccess)
			{
				Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Added component '%s' to entity %u", szDisplayName, pxEntity->GetEntityID());
			}
			return bSuccess;
		}
	}

	Zenith_Error(LOG_CATEGORY_EDITOR, "[EditorOp] Component '%s' not found in registry", szDisplayName);
	return false;
}

void Zenith_Editor::SetSelectedAsMainCamera()
{
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Zenith_Assert(pxEntity, "No entity selected");

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(pxEntity->GetEntityID());
	Zenith_Assert(pxSceneData, "Entity not in any scene");
	if (!pxSceneData)
	{
		return;
	}

	Zenith_EditorSceneAccess::SetMainCameraEntity(pxSceneData, pxEntity->GetEntityID());
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Set entity '%s' as main camera", pxEntity->GetName().c_str());
}

void Zenith_Editor::AttachGraphToSelected(const char* szGraphAssetPath)
{
	Zenith_Entity* pxEntity = GetSelectedEntity();
	Zenith_Assert(pxEntity, "No entity selected");

	if (!pxEntity->HasComponent<Zenith_GraphComponent>())
	{
		pxEntity->AddComponent<Zenith_GraphComponent>();
	}

	Zenith_GraphComponent& xGraphComponent = pxEntity->GetComponent<Zenith_GraphComponent>();
	xGraphComponent.AddGraphByAssetPath(szGraphAssetPath);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Attached behaviour graph '%s' to entity '%s' (%u total)",
		szGraphAssetPath, pxEntity->GetName().c_str(), xGraphComponent.GetGraphCount());
}

void Zenith_Editor::CreateNewScene(const char* szName)
{
	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene(szName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xScene);
	ClearSelection();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Created scene '%s'", szName);
}

void Zenith_Editor::SaveActiveScene(const char* szPath)
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Assert(pxData, "No active scene data");

	Zenith_EditorSceneAccess::SaveToFile(pxData, szPath);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Saved scene to '%s'", szPath);
}

void Zenith_Editor::UnloadActiveScene()
{
	ClearSelection();
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	g_xEngine.Scenes().UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Unloaded active scene");
}

//------------------------------------------------------------------------------
// Content Browser Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::RenderContentBrowser()
{
	ContentBrowserState xState = {
		m_xEditorState.m_xContentBrowser.m_strCurrentDirectory,
		m_xEditorState.m_xContentBrowser.m_xDirectoryContents,
		m_xEditorState.m_xContentBrowser.m_xFilteredContents,
		m_xEditorState.m_xContentBrowser.m_bDirectoryNeedsRefresh,
		m_xEditorState.m_xContentBrowser.m_szSearchBuffer,
		sizeof(m_xEditorState.m_xContentBrowser.m_szSearchBuffer),
		m_xEditorState.m_xContentBrowser.m_iAssetTypeFilter,
		m_xEditorState.m_xContentBrowser.m_iSelectedContentIndex,
		m_xEditorState.m_xContentBrowser.m_fThumbnailSize,
		m_xEditorState.m_xContentBrowser.m_axNavigationHistory,
		m_xEditorState.m_xContentBrowser.m_iHistoryIndex,
		m_xEditorState.m_xContentBrowser.m_eViewMode
	};
	Zenith_EditorPanelContentBrowser::Render(xState);
}

//------------------------------------------------------------------------------
// Console Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::AddLogMessage(const char* szMessage, ConsoleLogEntry::LogLevel eLevel, Zenith_LogCategory eCategory)
{
	// Zenith_Log fires during static-init (e.g. ZENITH_REGISTER_COMPONENT),
	// well before Zenith_Engine::Initialise allocates m_pxEditor. Pre-init
	// logs don't have anywhere to land -- the console panel isn't rendered
	// until the main loop runs anyway -- so skip them.
	if (!g_xEngine.HasEditor()) return;

	ConsoleLogEntry xEntry;
	xEntry.m_eLevel = eLevel;
	xEntry.m_eCategory = eCategory;
	xEntry.m_strMessage = szMessage;

	// Get current time for timestamp
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	char timeBuffer[32];
	struct tm localTime;
	localtime_s(&localTime, &time);
	strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &localTime);
	xEntry.m_strTimestamp = timeBuffer;

	// Zenith_Log is called from worker threads (async asset loader,
	// task system) as well as the main thread. std::vector push_back +
	// erase from multiple threads is undefined behaviour and was the
	// likely root cause of the silent mid-load crashes in RenderTest's
	// smoke matrix. Lock-protected; the editor console panel only reads
	// this on the main thread between frames, so the cost is just the
	// CRITICAL_SECTION per log call.
	static Zenith_Mutex_NoProfiling s_xLogMutex;
	Zenith_ScopedMutexLock_T xLock(s_xLogMutex);

	m_xEditorState.m_xConsole.m_xLogs.PushBack(xEntry);

	// Limit console entries
	if (m_xEditorState.m_xConsole.m_xLogs.GetSize() > Zenith_EditorConsoleState::MAX_ENTRIES)
	{
		m_xEditorState.m_xConsole.m_xLogs.Remove(0);
	}
}

void Zenith_Editor::ClearConsole()
{
	m_xEditorState.m_xConsole.m_xLogs.Clear();
}

void Zenith_Editor::RenderConsolePanel()
{
	Zenith_EditorPanelConsole::Render(
		m_xEditorState.m_xConsole.m_xLogs,
		m_xEditorState.m_xConsole.m_bAutoScroll,
		m_xEditorState.m_xConsole.m_bShowInfo,
		m_xEditorState.m_xConsole.m_bShowWarnings,
		m_xEditorState.m_xConsole.m_bShowErrors,
		m_xEditorState.m_xConsole.m_xCategoryFilters);
}

//------------------------------------------------------------------------------
// Material Editor Implementation
//------------------------------------------------------------------------------

void Zenith_Editor::SelectMaterial(Zenith_MaterialAsset* pMaterial)
{
	// Set() AddRefs so the asset survives UnloadUnusedAssets while the
	// editor has it selected; the handle's dtor (and Clear() below)
	// Releases. Replaces an earlier raw-pointer assignment that would
	// dangle the moment a scene-load cycle freed the registry entry.
	m_xEditorState.m_xMaterial.m_xSelectedMaterial.Set(pMaterial);
	m_xEditorState.m_xMaterial.m_bShowEditor = true;
	if (pMaterial)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[Editor] Selected material: %s", pMaterial->GetName().c_str());
	}
}

void Zenith_Editor::ClearMaterialSelection()
{
	m_xEditorState.m_xMaterial.m_xSelectedMaterial.Clear();
}

void Zenith_Editor::RenderMaterialEditorPanel()
{
	Zenith_MaterialEditorPanel::Render();
}

// Editor Camera System is implemented in Zenith_EditorCamera.cpp

#ifdef ZENITH_TESTING
#include "Editor/Zenith_Editor.Tests.inl"
#endif

#endif // ZENITH_TOOLS
