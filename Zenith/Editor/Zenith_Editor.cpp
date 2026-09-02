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
#include "Panels/Zenith_EditorPanel_StatusBar.h"
#include "Panels/Zenith_EditorPanel_VariantEditor.h"
#include "Panels/Zenith_EditorPanel_Viewport.h"

#include "Zenith_EditorUI.h"
#include "Zenith_EditorActions.h"
#include "Zenith_EditorCommands.h"
#include "Zenith_EditorPrefs.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Windows/Zenith_Windows_Window.h"

#include "imgui.h"
// DockBuilder API (code-built default dock layout) lives in the internal
// header by design — see BuildDefaultDockLayout below.
#include "imgui_internal.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_EditorWindowNames.h"

#include <filesystem>
#include <algorithm>

// Windows file dialog support
#ifdef _WIN32
#include "Core/Zenith_Win32.h"   // <windows.h> with the APIENTRY/LEAN guards
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
	// A Null build is a headless/CI run by construction and must never read or
	// write a user ini; the other two arms stay runtime (an automated test or an
	// explicit opt-out can happen in a windowed build too).
	if (Zenith_IsNullRenderer()
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
	static char s_acImGuiIniPath[ZENITH_MAX_PATH_LENGTH] = {};
	const std::string strDir = Zenith_EditorPrefs::GetUserDataDirectory();
	if (strDir.empty())
	{
		xIO.IniFilename = nullptr;
		return;
	}
	snprintf(s_acImGuiIniPath, sizeof(s_acImGuiIniPath), "%s/imgui.ini", strDir.c_str());
	xIO.IniFilename = s_acImGuiIniPath;
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

	// The toolbar is a fixed strip in the host window (Zenith_Editor::Render),
	// never a dock node. The Viewport hides its tab bar so it reads as the
	// scene, not as a document.
	if (ImGuiDockNode* pxCentreNode = ImGui::DockBuilderGetNode(uCentre))
	{
		pxCentreNode->SetLocalFlags(ImGuiDockNodeFlags_HiddenTabBar);
	}

	ImGui::DockBuilderDockWindow(szEDITOR_WINDOW_HIERARCHY,       uLeft);
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

	Zenith_EditorUI::ApplyTheme();

	// User preferences: recent scenes, fly speed, snapping ... (defaults in
	// automated / headless runs, where Load is a no-op).
	m_xEditorState.m_xPrefs.Load();
	m_xEditorState.m_xCamera.m_fMoveSpeed = m_xEditorState.m_xPrefs.m_fCameraMoveSpeed;

	// An interactive editor session opens maximised, like every other editor.
	// Automated runs and screenshot captures keep the requested window size so
	// their frames stay reproducible.
	m_xEditorState.m_bMaximiseRequested = !Zenith_IsNullRenderer()
		&& !Zenith_CommandLine::IsAutomatedTestRun()
		&& Zenith_CommandLine::GetScreenshotPath() == nullptr;

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
		Zenith_Profiling::ScopeZone xRenderProfileScope(ZENITH_PROFILE_ZONE("ImGUI Profiling"));
		m_pxProfiling->RenderToImGui();
#if ZENITH_MEMORY_TRACKING_ANY
		// Always-on memory HUD overlay (toggled by the dbg_bShowMemoryHUD debug variable).
		m_pxProfiling->RenderMemoryHUD();
#endif
	}

	// Finalize ImGui rendering data - this MUST be called before submitting the render task
	ImGui::Render();
}

void Zenith_Editor::Shutdown()
{
	m_xEditorState.m_xPrefs.m_fCameraMoveSpeed = m_xEditorState.m_xCamera.m_fMoveSpeed;
	m_xEditorState.m_xPrefs.Save();
	EndCameraGestures();

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
	// playtesting. The null check is defensive: Initialise() injects the pointer
	// in every config now, but Update() can run before it on an early frame.
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

	// Keyboard shortcuts run in every mode (Ctrl+P stops a playing scene; the
	// gizmo/entity ones no-op while playing).
	UpdateEditorInput();

	// Update editor camera controls (when not playing), with the real frame dt.
	UpdateEditorCamera(m_pxFrame != nullptr ? m_pxFrame->GetDt() : (1.0f / 60.0f));

	if (m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		return true;
	}

	SyncGizmoSettings();

	// A camera gesture (look / orbit / pan) owns the mouse: no gizmo, no picking.
	if (IsCameraNavigating())
	{
		g_xEngine.Gizmos().ClearHover();
		return true;
	}

	// Terrain editor gets first claim on viewport input: while a terrain
	// editing session is armed over the viewport (or mid-stroke), gizmo
	// interaction and object picking are skipped for the frame. RMB camera
	// look is unaffected (UpdateEditorCamera ran above). Null check — see the
	// ServiceUpdate note above.
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

// In-frame entry point. NO GPU sync: Update() runs before render-task
// submission so no render tasks are active, and every GPU resource the scene
// teardown frees is queued through QueueVRAMDeletion's MAX_FRAMES_IN_FLIGHT+1
// grace period — the same contract the runtime LoadScene teardown relies on
// mid-play. Stalling here would be a per-load hitch for no benefit.
bool Zenith_Editor::HandlePendingSceneLoad()
{
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = false;

	LoadPendingSceneIntoActiveScene(false, "");

	return false;
}

void Zenith_Editor::OpenTerrainEditor(Zenith_EntityID uTerrainEntity)
{
	m_pxTerrainEditor->Open(uTerrainEntity);
	m_xEditorState.m_xPanels.m_bShowTerrainEditor = true;
}

namespace
{
	// ImGui owns the keyboard while a text field is focused: no shortcut may fire.
	bool KeyboardIsFree()
	{
		return ImGui::GetCurrentContext() == nullptr || !ImGui::GetIO().WantTextInput;
	}
}

void Zenith_Editor::UpdateEditorInput()
{
	if (!KeyboardIsFree())
	{
		return;
	}
	Zenith_Input& xInput = g_xEngine.Input();
	const bool bCtrl = xInput.IsKeyDown(ZENITH_KEY_LEFT_CONTROL) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);
	const bool bShift = xInput.IsKeyDown(ZENITH_KEY_LEFT_SHIFT) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_SHIFT);
	const bool bAlt = xInput.IsKeyDown(ZENITH_KEY_LEFT_ALT) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_ALT);

	HandleGlobalShortcuts(xInput, bCtrl, bShift);

	if (m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		return;   // W/E/R, Delete, F ... belong to the game while it runs
	}

	// Entity keys are scoped to the scene views (viewport + hierarchy), so a
	// Delete pressed in the console or content browser never removes an entity.
	if (m_xEditorState.m_xViewport.m_bFocused || m_xEditorState.m_xHierarchy.m_bFocused)
	{
		HandleEntityShortcuts(xInput, bCtrl);
	}
	// Viewport-only keys (they collide with typing elsewhere)
	if (m_xEditorState.m_xViewport.m_bFocused && !bCtrl && !bAlt && !m_xEditorState.m_xCamera.m_bLooking)
	{
		HandleViewportShortcuts(xInput);
	}
}

// Chords that work from any panel: file, undo, play, F1.
void Zenith_Editor::HandleGlobalShortcuts(Zenith_Input& xInput, bool bCtrl, bool bShift)
{
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_F1))
	{
		m_xEditorState.m_xPanels.m_bShowShortcuts = !m_xEditorState.m_xPanels.m_bShowShortcuts;
	}
	if (!bCtrl)
	{
		return;
	}
	const bool bZ = xInput.WasKeyPressedThisFrame(ZENITH_KEY_Z);
	if (bZ && !bShift)
	{
		g_xEngine.UndoSystem().Undo();
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_Y) || (bZ && bShift))
	{
		g_xEngine.UndoSystem().Redo();
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_S))
	{
		if (bShift)
		{
			Zenith_EditorActions::SaveSceneAs();
		}
		else
		{
			Zenith_EditorActions::SaveScene();
		}
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_O) && !bShift)
	{
		Zenith_EditorActions::OpenSceneDialog();
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_N))
	{
		if (bShift)
		{
			Zenith_EditorActions::CreateEntity(Zenith_EditorActions::CreateKind::Empty);
		}
		else
		{
			Zenith_EditorActions::NewScene();
		}
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_P))
	{
		if (bShift)
		{
			Zenith_EditorActions::TogglePause();
		}
		else
		{
			Zenith_EditorActions::TogglePlay();
		}
	}
}

// Selection verbs: duplicate, select all, delete, deselect, focus.
void Zenith_Editor::HandleEntityShortcuts(Zenith_Input& xInput, bool bCtrl)
{
	if (bCtrl && xInput.WasKeyPressedThisFrame(ZENITH_KEY_D))
	{
		Zenith_EditorActions::DuplicateSelection();
	}
	if (bCtrl && xInput.WasKeyPressedThisFrame(ZENITH_KEY_A))
	{
		Zenith_EditorActions::SelectAll();
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_DELETE))
	{
		Zenith_EditorActions::DeleteSelection();
	}
	if (!bCtrl && xInput.WasKeyPressedThisFrame(ZENITH_KEY_F))
	{
		Zenith_EditorActions::FocusSelection();
	}
	const bool bPopupOpen = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE) && HasSelection() && !bPopupOpen)
	{
		Zenith_EditorActions::DeselectAll();
	}
}

// Gizmo mode and space keys.
void Zenith_Editor::HandleViewportShortcuts(Zenith_Input& xInput)
{
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_W))
	{
		SetGizmoMode(EditorGizmoMode::Translate);
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_E))
	{
		SetGizmoMode(EditorGizmoMode::Rotate);
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_R))
	{
		SetGizmoMode(EditorGizmoMode::Scale);
	}
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_X))
	{
		m_xEditorState.m_xPrefs.m_bGizmoLocalSpace = !m_xEditorState.m_xPrefs.m_bGizmoLocalSpace;
		m_xEditorState.m_xPrefs.Save();
	}
}

void Zenith_Editor::SyncGizmoSettings()
{
	const Zenith_EditorPrefs& xPrefs = m_xEditorState.m_xPrefs;
	Zenith_Input& xInput = g_xEngine.Input();
	Flux_GizmoSnapSettings xSnap;
	// The toolbar toggle, or Ctrl held for one drag.
	xSnap.m_bEnabled = xPrefs.m_bSnapEnabled || xInput.IsKeyDown(ZENITH_KEY_LEFT_CONTROL) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);
	xSnap.m_fMoveStep = xPrefs.m_fSnapMove;
	xSnap.m_fRotateStepDegrees = xPrefs.m_fSnapRotateDegrees;
	xSnap.m_fScaleStep = xPrefs.m_fSnapScale;
	g_xEngine.Gizmos().SetSnapSettings(xSnap);
	g_xEngine.Gizmos().SetLocalSpace(xPrefs.m_bGizmoLocalSpace);
}

void Zenith_Editor::UpdateWindowTitle()
{
	std::string strTitle = Zenith_EditorActions::GetActiveSceneDisplayName();
	if (Zenith_EditorActions::HasUnsavedChanges())
	{
		strTitle += "*";
	}
	strTitle += "  -  ";
	strTitle += Project_GetName();
	strTitle += "  -  Zenith Editor";
	if (m_xEditorState.m_eEditorMode == EditorMode::Playing) strTitle += "  [Playing]";
	else if (m_xEditorState.m_eEditorMode == EditorMode::Paused) strTitle += "  [Paused]";
	if (strTitle != m_xEditorState.m_strWindowTitle)
	{
		m_xEditorState.m_strWindowTitle = strTitle;
		Zenith_Window::GetInstance()->SetTitle(strTitle.c_str());
	}
}

void Zenith_Editor::Render()
{
	if (m_xEditorState.m_bMaximiseRequested)
	{
		m_xEditorState.m_bMaximiseRequested = false;
		Zenith_Window::GetInstance()->Maximize();
	}
	UpdateWindowTitle();

	const bool bPlayTint = (m_xEditorState.m_eEditorMode != EditorMode::Stopped);
	if (bPlayTint)
	{
		Zenith_EditorUI::PushPlayModeTint();
	}

	// The host window: menu bar, then the toolbar strip, then the dockspace,
	// then the status bar — the strips are part of the host so they can never be
	// docked away, closed or clipped.
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin(szEDITOR_WINDOW_DOCKSPACE_HOST, nullptr, window_flags);
	ImGui::PopStyleVar(3);

	RenderMainMenuBar();

	const float fToolbarHeight = Zenith_EditorPanelToolbar::GetHeight();
	const float fStatusHeight = Zenith_EditorPanelStatusBar::GetHeight();
	Zenith_EditorPanelToolbar::Render(*this, fToolbarHeight);

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
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, -fStatusHeight), ImGuiDockNodeFlags_PassthruCentralNode);

	Zenith_EditorPanelStatusBar::Render(*this, fStatusHeight);
	ImGui::End();

	// Render editor panels
	if (m_xEditorState.m_xPanels.m_bShowHierarchy) RenderHierarchyPanel();
	if (m_xEditorState.m_xPanels.m_bShowProperties) RenderPropertiesPanel();
	RenderViewport();
	if (m_xEditorState.m_xPanels.m_bShowContentBrowser) RenderContentBrowser();
	if (m_xEditorState.m_xPanels.m_bShowConsole) RenderConsolePanel();
	RenderMaterialEditorPanel();

#if ZENITH_MEMORY_TRACKING_FULL
	Zenith_EditorPanelMemory::Render();
#endif

	Zenith_EditorPanelRenderGraph::Render();
	Zenith_EditorPanelVariantEditor::Render();
	Zenith_GraphEditorPanel::Render();
	if (m_pxTerrainEditor != nullptr)
	{
		Zenith_EditorPanelTerrainEditor::Render(*m_pxTerrainEditor, m_xEditorState.m_xPanels.m_bShowTerrainEditor);
	}

	RenderPrompts();
	RenderShortcutsWindow();

	// Render gizmos and overlays (after viewport so they appear on top)
	RenderGizmos();

	if (bPlayTint)
	{
		Zenith_EditorUI::PopPlayModeTint();
	}
}

// The "Save changes?" modal. Opened by Zenith_EditorActions when an action
// would drop unsaved work; the chosen answer re-enters that action.
void Zenith_Editor::RenderPrompts()
{
	Zenith_EditorPromptState& xPrompt = m_xEditorState.m_xPrompt;
	if (xPrompt.m_bOpenRequested)
	{
		ImGui::OpenPopup("Save changes?");
		xPrompt.m_bOpenRequested = false;
	}
	ImGui::SetNextWindowSize(ImVec2(Zenith_EditorUI::Px(420.0f), 0.0f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal("Save changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		return;
	}

	const std::string strScene = Zenith_EditorActions::GetActiveSceneDisplayName();
	ImGui::TextWrapped("The scene '%s' has unsaved changes.", strScene.c_str());
	ImGui::TextDisabled("Your changes will be lost if you don't save them.");
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const Zenith_EditorPromptState::Action eAction = xPrompt.m_eAction;
	const std::string strPath = xPrompt.m_strPath;
	auto Proceed = [this, eAction, strPath]()
	{
		// Consume the pending action, then re-run it: the dirty flag is now
		// either cleared (saved) or deliberately ignored (discarded).
		m_xEditorState.m_xPrompt.m_eAction = Zenith_EditorPromptState::Action::None;
		switch (eAction)
		{
		case Zenith_EditorPromptState::Action::NewScene:  Zenith_EditorActions::NewScene(); break;
		case Zenith_EditorPromptState::Action::OpenScene: Zenith_EditorActions::OpenScenePath(strPath); break;
		case Zenith_EditorPromptState::Action::Exit:      Zenith_EditorActions::RequestExit(); break;
		default: break;
		}
	};

	const float fButton = Zenith_EditorUI::Px(110.0f);
	if (ImGui::Button("Save", ImVec2(fButton, 0)))
	{
		ImGui::CloseCurrentPopup();
		if (Zenith_EditorActions::SaveScene())
		{
			Proceed();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Don't Save", ImVec2(fButton, 0)))
	{
		ImGui::CloseCurrentPopup();
		// Discarding: drop the flags so the re-entered action does not re-prompt.
		for (uint32_t i = 0; ; ++i)
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
			if (!xScene.IsValid()) break;
			if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene))
			{
				Zenith_EditorSceneAccess::ClearDirty(pxSceneData);
			}
		}
		Proceed();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(fButton, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		m_xEditorState.m_xPrompt.m_eAction = Zenith_EditorPromptState::Action::None;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void Zenith_Editor::RenderShortcutsWindow()
{
	bool& bShow = m_xEditorState.m_xPanels.m_bShowShortcuts;
	if (!bShow)
	{
		return;
	}
	ImGui::SetNextWindowSize(ImVec2(Zenith_EditorUI::Px(520.0f), Zenith_EditorUI::Px(460.0f)), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(szEDITOR_WINDOW_SHORTCUTS, &bShow))
	{
		ImGui::End();
		return;
	}
	struct Row { const char* m_szKeys; const char* m_szAction; };
	struct Group { const char* m_szTitle; const Row* m_pxRows; int m_iCount; };
	static const Row axViewport[] =
	{
		{ "RMB drag", "Look around" }, { "RMB + WASD / Q E", "Fly (Shift: 3x, wheel: speed)" },
		{ "Alt + LMB drag", "Orbit the selection" }, { "MMB drag", "Pan" }, { "Wheel", "Dolly" },
		{ "F", "Focus the selection" }, { "LMB", "Select (Ctrl: toggle, Shift: add)" },
	};
	static const Row axGizmo[] =
	{
		{ "W / E / R", "Move / Rotate / Scale" }, { "X", "Local / world space" }, { "Ctrl (hold)", "Snap while dragging" },
	};
	static const Row axEntities[] =
	{
		{ "Delete", "Delete selection" }, { "Ctrl + D", "Duplicate selection" }, { "F2 / double-click", "Rename" },
		{ "Ctrl + A", "Select all" }, { "Esc", "Deselect" }, { "Ctrl + Shift + N", "Create empty entity" },
	};
	static const Row axScene[] =
	{
		{ "Ctrl + N", "New scene" }, { "Ctrl + O", "Open scene" }, { "Ctrl + S", "Save scene" }, { "Ctrl + Shift + S", "Save scene as" },
		{ "Ctrl + Z / Ctrl + Y", "Undo / Redo" }, { "Ctrl + P", "Play / Stop" }, { "Ctrl + Shift + P", "Pause / Resume" }, { "F1", "This window" },
	};
	const Group axGroups[] =
	{
		{ "Viewport", axViewport, IM_ARRAYSIZE(axViewport) }, { "Gizmo", axGizmo, IM_ARRAYSIZE(axGizmo) },
		{ "Entities", axEntities, IM_ARRAYSIZE(axEntities) }, { "Scene", axScene, IM_ARRAYSIZE(axScene) },
	};
	for (const Group& xGroup : axGroups)
	{
		ImGui::SeparatorText(xGroup.m_szTitle);
		if (ImGui::BeginTable(xGroup.m_szTitle, 2, ImGuiTableFlags_SizingStretchProp))
		{
			for (int i = 0; i < xGroup.m_iCount; ++i)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Zenith_EditorUI::Palette().m_uAccentHover), "%s", xGroup.m_pxRows[i].m_szKeys);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(xGroup.m_pxRows[i].m_szAction);
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}

// RenderMainMenuBar + per-menu helpers (RenderFileMenu / RenderEditMenu / RenderViewMenu)
// live in Zenith_Editor_Menu.cpp.

void Zenith_Editor::RenderHierarchyPanel()
{
	Zenith_EditorPanelHierarchy::Render(m_xEditorState.m_xHierarchy, m_xEditorState.m_xCamera.m_uGameCameraEntity);
}

void Zenith_Editor::RenderPropertiesPanel()
{
	Zenith_EditorPanelProperties::Render(m_xEditorState.m_xInspector, GetSelectedEntity(), GetSelectionCount());
}

void Zenith_Editor::RenderViewport()
{
	Zenith_EditorPanelViewport::Render(*this);
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

	Zenith_Input& xInput = g_xEngine.Input();
	const bool bCtrl = xInput.IsKeyDown(ZENITH_KEY_LEFT_CONTROL) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_CONTROL);
	const bool bShift = xInput.IsKeyDown(ZENITH_KEY_LEFT_SHIFT) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_SHIFT);
	if (uHitEntityID != INVALID_ENTITY_ID)
	{
		if (bCtrl)       ToggleEntitySelection(uHitEntityID);
		else if (bShift) SelectEntity(uHitEntityID, true);
		else             SelectEntity(uHitEntityID);
		m_xEditorState.m_xHierarchy.m_xScrollTo = uHitEntityID;
	}
	else if (!bCtrl && !bShift)
	{
		ClearSelection();
	}
}

void Zenith_Editor::DrawSelectionBounds()
{
	if (!m_xEditorState.m_xPrefs.m_bShowSelectionBounds || m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		return;
	}
	const Zenith_Maths::Vector3 xColour(0.98f, 0.62f, 0.18f);
	for (const Zenith_EntityID& xID : m_xEditorState.m_xSelection.m_xSelectedEntityIDs)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		if (!xEntity.IsValid())
		{
			continue;
		}
		const BoundingBox xBox = g_xEngine.Selection().GetEntityBoundingBox(&xEntity);
		if (xBox.m_xMin.x > xBox.m_xMax.x)
		{
			continue;
		}
		const Zenith_Maths::Vector3 xCentre = (xBox.m_xMin + xBox.m_xMax) * 0.5f;
		const Zenith_Maths::Vector3 xHalf = glm::max((xBox.m_xMax - xBox.m_xMin) * 0.5f, Zenith_Maths::Vector3(0.05f));
		g_xEngine.Primitives().AddWireframeCube(xCentre, xHalf, xColour);
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

	DrawSelectionBounds();

	// Gizmos are now part of the render graph - no separate task submission needed
}

void Zenith_Editor::HandleGizmoInteraction()
{
	Flux_GizmosImpl& xGizmos = g_xEngine.Gizmos();
	// Only handle in Stopped or Paused mode, with something to manipulate
	if (m_xEditorState.m_eEditorMode == EditorMode::Playing || !HasSelection())
	{
		xGizmos.ClearHover();
		return;
	}
	// A drag that started over the viewport keeps ownership until release even
	// if the cursor leaves the panel; a new drag needs the viewport hovered.
	if (!m_xEditorState.m_xViewport.m_bHovered && !xGizmos.IsInteracting())
	{
		xGizmos.ClearHover();
		return;
	}

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

	Zenith_Input& xInput = g_xEngine.Input();
	Zenith_EditorGizmoState& xDrag = m_xEditorState.m_xGizmo;

	// Press: begin a drag if a handle is under the cursor
	if (xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT) && m_xEditorState.m_xViewport.m_bHovered)
	{
		xGizmos.BeginInteraction(xRayOrigin, xRayDir);
		if (xGizmos.IsInteracting())
		{
			xDrag.m_bDragActive = true;
			xDrag.m_xDragEntity = GetSelectedEntityID();
		}
	}

	// Drag: update every frame the button is held (same frame as the press too)
	if (xInput.IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT) && xGizmos.IsInteracting())
	{
		xGizmos.UpdateInteraction(xRayOrigin, xRayDir);
	}

	// Release: end the drag and record it as ONE undo step
	if (!xInput.IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT) && xGizmos.IsInteracting())
	{
		xGizmos.EndInteraction();
		if (xDrag.m_bDragActive)
		{
			RecordGizmoDragUndo(xDrag.m_xDragEntity);
		}
		xDrag.m_bDragActive = false;
		xDrag.m_xDragEntity = INVALID_ENTITY_ID;
	}

	HandleGizmoHover(xRayOrigin, xRayDir);
}

void Zenith_Editor::HandleGizmoHover(const Zenith_Maths::Vector3& xRayOrigin, const Zenith_Maths::Vector3& xRayDir)
{
	Flux_GizmosImpl& xGizmos = g_xEngine.Gizmos();
	if (xGizmos.IsInteracting())
	{
		return;
	}
	if (m_xEditorState.m_xViewport.m_bHovered)
	{
		xGizmos.UpdateHover(xRayOrigin, xRayDir);
	}
	else
	{
		xGizmos.ClearHover();
	}
}

// (initial, final) transform of a finished gizmo drag -> one TransformEdit
// command, recorded without re-executing (the entity is already there).
void Zenith_Editor::RecordGizmoDragUndo(Zenith_EntityID xEntityID)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	if (!xEntity.IsValid())
	{
		return;
	}
	Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
	if (pxTransform == nullptr)
	{
		return;
	}
	Zenith_Maths::Vector3 xNewPos, xNewScale;
	Zenith_Maths::Quat xNewRot;
	pxTransform->GetPosition(xNewPos);
	pxTransform->GetRotation(xNewRot);
	pxTransform->GetScale(xNewScale);

	Flux_GizmosImpl& xGizmos = g_xEngine.Gizmos();
	const Zenith_Maths::Vector3& xOldPos = xGizmos.GetInitialPosition();
	const Zenith_Maths::Quat& xOldRot = xGizmos.GetInitialRotation();
	const Zenith_Maths::Vector3& xOldScale = xGizmos.GetInitialScale();
	if (xNewPos == xOldPos && xNewRot == xOldRot && xNewScale == xOldScale)
	{
		return;   // a click without movement
	}
	g_xEngine.UndoSystem().Record(new Zenith_UndoCommand_TransformEdit(xEntityID, xOldPos, xOldRot, xOldScale, xNewPos, xNewRot, xNewScale));
	Zenith_EditorActions::MarkSceneDirtyForEntity(xEntityID);
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


	// Update Flux_Gizmos target entity (primary selection)
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity)
	{
		g_xEngine.Gizmos().SetTargetEntity(pxEntity);
	}
	RefreshCameraPivotFromSelection();
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

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
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
	RefreshCameraPivotFromSelection();
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
	Zenith_Entity xResolved = g_xEngine.Scenes().ResolveEntity(m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
	if (!xResolved.IsValid())
	{
		// Entity no longer exists in any scene - remove from selection
		m_xEditorState.m_xSelection.m_xSelectedEntityIDs.erase(m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID);
		m_xEditorState.m_xSelection.m_uPrimarySelectedEntityID = m_xEditorState.m_xSelection.m_xSelectedEntityIDs.empty() ?
			INVALID_ENTITY_ID : *m_xEditorState.m_xSelection.m_xSelectedEntityIDs.begin();
		return nullptr;
	}

	// Return pointer to static entity handle (valid until next call)
	static Zenith_Entity s_xSelectedEntity;
	s_xSelectedEntity = xResolved;
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

namespace
{
	// THE PUBLISH AUDIT — what is left of the headless publish guard (ZEN-6).
	//
	// There USED to be a refusal here: on Zenith_IsNullRenderer() a save that would
	// CHANGE an existing .zscen was rejected, because a Null-backend boot authored a
	// strict SUBSET of the windowed world and serializing that subset silently DELETED
	// content (it dropped RenderTest's two instanced-tree entities, ~323 KB of a 361 KB
	// file, on every headless boot). The refusal was correct for the world as it was,
	// and it is gone because the world changed: the authoring steps it protected
	// against — Zenith_TerrainEditor::EnsureTreeEntities first among them — now create
	// their entities and components on EVERY backend and skip only the GPU allocation,
	// so a Null boot's in-memory scene is entity-complete and there is nothing left to
	// protect. A headless run may now CHANGE a committed scene, which is the point:
	// re-authoring an asset no longer requires a machine with a graphics driver.
	//
	// The COMPARISON stays, for two reasons that outlive the refusal:
	//
	//   1. IDENTICAL is the completeness PROOF. "A Null boot re-authors a committed
	//      scene to the same bytes" is exactly the assertion that the Null authoring
	//      path is not missing anything, and it needs no windowed run to check. That
	//      verdict is logged on every publish, on every backend, with a stable marker.
	//   2. A save that publishes FEWER entities than the asset held is still worth
	//      saying out loud. It is no longer refused — deleting an entity is a
	//      legitimate authoring change — but it is the exact shape of the defect above,
	//      so it is reported with both counts rather than passing in silence.
	//
	// A byte-identical save is skipped rather than rewritten. That is not the guard:
	// it writes the same bytes either way, and skipping keeps a no-op re-author from
	// touching the file's mtime. It now applies on every backend, not just Null.
	//
	// Returns true when the caller should go ahead and write.
	bool AuditScenePublish(Zenith_SceneData* pxData, const char* szPath)
	{
		const Zenith_ScenePublishDelta xDelta = Zenith_EditorSceneAccess::CompareWithFile(pxData, szPath);

		if (xDelta.m_eResult == Zenith_ScenePublishDelta::NO_FILE)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR,
				"[ScenePublish] CREATED '%s' — no asset on disk (%u entities/%llu bytes)",
				szPath, xDelta.m_uPendingEntityCount, xDelta.m_ulPendingBytes);
			return true;
		}

		if (xDelta.m_eResult == Zenith_ScenePublishDelta::IDENTICAL)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR,
				"[ScenePublish] IDENTICAL '%s' — this boot re-authored the committed bytes exactly "
				"(%u entities/%llu bytes); write skipped",
				szPath, xDelta.m_uOnDiskEntityCount, xDelta.m_ulOnDiskBytes);
			return false;
		}

		if (xDelta.WouldDropEntities())
		{
			Zenith_Error(LOG_CATEGORY_EDITOR,
				"[ScenePublish] CHANGED '%s' and it DROPS ENTITIES: this boot authored %u entities but the "
				"asset on disk holds %u, so %u are about to be deleted (%llu -> %llu bytes). Publishing "
				"anyway — confirm this is an intended authoring change and not an authoring step that "
				"failed to run on this backend.",
				szPath, xDelta.m_uPendingEntityCount, xDelta.m_uOnDiskEntityCount,
				xDelta.m_uOnDiskEntityCount - xDelta.m_uPendingEntityCount,
				xDelta.m_ulOnDiskBytes, xDelta.m_ulPendingBytes);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR,
				"[ScenePublish] CHANGED '%s' (%u entities/%llu bytes -> %u entities/%llu bytes)",
				szPath, xDelta.m_uOnDiskEntityCount, xDelta.m_ulOnDiskBytes,
				xDelta.m_uPendingEntityCount, xDelta.m_ulPendingBytes);
		}
		return true;
	}
}

void Zenith_Editor::SaveActiveScene(const char* szPath)
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Assert(pxData, "No active scene data");

	if (!AuditScenePublish(pxData, szPath))
	{
		return;
	}

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
	Zenith_EditorPanelContentBrowser::Render(m_xEditorState.m_xContentBrowser);
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
	Zenith_EditorPanelConsole::Render(m_xEditorState.m_xConsole);
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
