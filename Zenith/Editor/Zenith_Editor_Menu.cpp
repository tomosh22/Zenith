#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_EditorActions.h"
#include "Zenith_EditorUI.h"
#include "Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Flux/Gizmos/Flux_GizmosImpl.h"

#include "Panels/Zenith_EditorPanel_Memory.h"
#include "Panels/Zenith_EditorPanel_RenderGraph.h"
#include "Panels/Zenith_EditorPanel_VariantEditor.h"

#include "imgui.h"

//==========================================================================
// Zenith_Editor — main menu bar
//
// Every item calls a Zenith_EditorActions verb, so the menu, the keyboard
// shortcut and the toolbar button for one operation are the same code. The
// shortcut labels here are documentation; the keys are bound in
// Zenith_Editor::UpdateEditorInput.
//==========================================================================

void Zenith_Editor::RenderMainMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		RenderFileMenu();
		RenderEditMenu();
		RenderEntityMenu();
		RenderViewMenu();
		RenderHelpMenu();

		ImGui::EndMenuBar();
	}
}

void Zenith_Editor::RenderFileMenu()
{
	if (!ImGui::BeginMenu("File"))
	{
		return;
	}

	if (ImGui::MenuItem("New Scene", "Ctrl+N"))
	{
		Zenith_EditorActions::NewScene();
	}
	if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
	{
		Zenith_EditorActions::OpenSceneDialog();
	}
	if (ImGui::MenuItem("Open Scene Additive..."))
	{
		Zenith_EditorActions::OpenSceneAdditiveDialog();
	}

	const Zenith_Vector<std::string>& axRecent = m_xEditorState.m_xPrefs.m_axRecentScenes;
	if (ImGui::BeginMenu("Open Recent", axRecent.GetSize() > 0))
	{
		for (u_int u = 0; u < axRecent.GetSize(); ++u)
		{
			const std::string& strPath = axRecent.Get(u);
			const size_t uSlash = strPath.find_last_of("/\\");
			const std::string strLabel = (uSlash == std::string::npos) ? strPath : strPath.substr(uSlash + 1);
			ImGui::PushID(static_cast<int>(u));
			if (ImGui::MenuItem(strLabel.c_str()))
			{
				Zenith_EditorActions::OpenScenePath(strPath);
			}
			ImGui::SetItemTooltip("%s", strPath.c_str());
			ImGui::PopID();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Clear Recent"))
		{
			m_xEditorState.m_xPrefs.m_axRecentScenes.Clear();
			m_xEditorState.m_xPrefs.Save();
		}
		ImGui::EndMenu();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
	{
		Zenith_EditorActions::SaveScene();
	}
	if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
	{
		Zenith_EditorActions::SaveSceneAs();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Exit", "Alt+F4"))
	{
		Zenith_EditorActions::RequestExit();
	}

	ImGui::EndMenu();
}

void Zenith_Editor::RenderEditMenu()
{
	if (!ImGui::BeginMenu("Edit"))
	{
		return;
	}

	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	const bool bCanUndo = xUndo.CanUndo();
	const bool bCanRedo = xUndo.CanRedo();

	// The item names carry the description so the menu reads "Undo Move Entity".
	char acLabel[160];
	snprintf(acLabel, sizeof(acLabel), "Undo %s", bCanUndo ? xUndo.GetUndoDescription() : "");
	if (ImGui::MenuItem(acLabel, "Ctrl+Z", false, bCanUndo))
	{
		xUndo.Undo();
	}
	snprintf(acLabel, sizeof(acLabel), "Redo %s", bCanRedo ? xUndo.GetRedoDescription() : "");
	if (ImGui::MenuItem(acLabel, "Ctrl+Y", false, bCanRedo))
	{
		xUndo.Redo();
	}

	ImGui::Separator();

	const bool bHasSelection = HasSelection();
	if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, bHasSelection))
	{
		Zenith_EditorActions::DuplicateSelection();
	}
	if (ImGui::MenuItem("Delete", "Del", false, bHasSelection))
	{
		Zenith_EditorActions::DeleteSelection();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Select All", "Ctrl+A"))
	{
		Zenith_EditorActions::SelectAll();
	}
	if (ImGui::MenuItem("Deselect All", "Esc", false, bHasSelection))
	{
		Zenith_EditorActions::DeselectAll();
	}
	if (ImGui::MenuItem("Focus Selection", "F", false, bHasSelection))
	{
		Zenith_EditorActions::FocusSelection();
	}

	ImGui::Separator();

	Zenith_EditorPrefs& xPrefs = m_xEditorState.m_xPrefs;
	bool bPrefsChanged = false;
	if (ImGui::BeginMenu("Camera"))
	{
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
		bPrefsChanged |= ImGui::DragFloat("Look sensitivity", &xPrefs.m_fLookSensitivity, 0.005f,
			fMIN_LOOK_SENSITIVITY, fMAX_LOOK_SENSITIVITY, "%.3f deg/px");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Degrees turned per pixel of pointer movement, for RMB look and Alt+LMB orbit.");
		}
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
		if (ImGui::DragFloat("Move speed", &xPrefs.m_fCameraMoveSpeed, 1.0f, 0.5f, 5000.0f, "%.0f m/s"))
		{
			// The live camera owns this one (the wheel changes it mid-flight),
			// so an edit here must reach it or the slider appears to do nothing.
			m_xEditorState.m_xCamera.m_fMoveSpeed = xPrefs.m_fCameraMoveSpeed;
			bPrefsChanged = true;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Snapping"))
	{
		bPrefsChanged |= ImGui::MenuItem("Snap Enabled", "Ctrl (hold)", &xPrefs.m_bSnapEnabled);
		ImGui::Separator();
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
		bPrefsChanged |= ImGui::DragFloat("Move step", &xPrefs.m_fSnapMove, 0.05f, 0.01f, 100.0f, "%.2f m");
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
		bPrefsChanged |= ImGui::DragFloat("Rotate step", &xPrefs.m_fSnapRotateDegrees, 1.0f, 1.0f, 90.0f, "%.0f deg");
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(110.0f));
		bPrefsChanged |= ImGui::DragFloat("Scale step", &xPrefs.m_fSnapScale, 0.01f, 0.01f, 10.0f, "%.2f");
		ImGui::EndMenu();
	}
	bPrefsChanged |= ImGui::MenuItem("Local Space Gizmo", "X", &xPrefs.m_bGizmoLocalSpace);
	if (bPrefsChanged)
	{
		xPrefs.Save();
	}

	ImGui::EndMenu();
}

void Zenith_Editor::RenderEntityMenu()
{
	if (!ImGui::BeginMenu("Entity"))
	{
		return;
	}

	using Zenith_EditorActions::CreateKind;
	const Zenith_EntityID xParent = HasSelection() ? GetSelectedEntityID() : INVALID_ENTITY_ID;

	if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N"))
	{
		Zenith_EditorActions::CreateEntity(CreateKind::Empty);
	}
	if (ImGui::MenuItem("Create Empty Child", nullptr, false, xParent.IsValid()))
	{
		Zenith_EditorActions::CreateEntity(CreateKind::Empty, xParent);
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Camera"))
	{
		Zenith_EditorActions::CreateEntity(CreateKind::Camera);
	}
	if (ImGui::BeginMenu("Light"))
	{
		if (ImGui::MenuItem("Point Light"))       Zenith_EditorActions::CreateEntity(CreateKind::PointLight);
		if (ImGui::MenuItem("Spot Light"))        Zenith_EditorActions::CreateEntity(CreateKind::SpotLight);
		if (ImGui::MenuItem("Directional Light")) Zenith_EditorActions::CreateEntity(CreateKind::DirectionalLight);
		ImGui::EndMenu();
	}

	ImGui::EndMenu();
}

void Zenith_Editor::RenderViewMenu()
{
	if (!ImGui::BeginMenu("Window"))
	{
		return;
	}

	Zenith_EditorPanelVisibility& xPanels = m_xEditorState.m_xPanels;
	ImGui::MenuItem("Hierarchy", nullptr, &xPanels.m_bShowHierarchy);
	ImGui::MenuItem("Properties", nullptr, &xPanels.m_bShowProperties);
	ImGui::MenuItem("Console", nullptr, &xPanels.m_bShowConsole);
	ImGui::MenuItem("Content Browser", nullptr, &xPanels.m_bShowContentBrowser);
	ImGui::MenuItem("Material Editor", nullptr, &m_xEditorState.m_xMaterial.m_bShowEditor);
	ImGui::MenuItem("Terrain Editor", nullptr, &xPanels.m_bShowTerrainEditor);

#if ZENITH_MEMORY_TRACKING_FULL
	if (ImGui::MenuItem("Memory Profiler", nullptr, Zenith_EditorPanelMemory::IsVisible()))
	{
		Zenith_EditorPanelMemory::SetVisible(!Zenith_EditorPanelMemory::IsVisible());
	}
#endif

	if (ImGui::MenuItem("Render Graph", nullptr, Zenith_EditorPanelRenderGraph::IsVisible()))
	{
		Zenith_EditorPanelRenderGraph::SetVisible(!Zenith_EditorPanelRenderGraph::IsVisible());
	}

	if (ImGui::MenuItem("Variant Editor", nullptr, Zenith_EditorPanelVariantEditor::IsVisible()))
	{
		Zenith_EditorPanelVariantEditor::SetVisible(!Zenith_EditorPanelVariantEditor::IsVisible());
	}

	ImGui::Separator();

	Zenith_EditorPrefs& xPrefs = m_xEditorState.m_xPrefs;
	bool bPrefsChanged = false;
	if (ImGui::BeginMenu("Viewport"))
	{
		bPrefsChanged |= ImGui::MenuItem("Statistics Overlay", nullptr, &xPrefs.m_bShowViewportStats);
		bPrefsChanged |= ImGui::MenuItem("Axis Widget", nullptr, &xPrefs.m_bShowViewportAxes);
		bPrefsChanged |= ImGui::MenuItem("Selection Bounds", nullptr, &xPrefs.m_bShowSelectionBounds);
		ImGui::Separator();
		ImGui::MenuItem("Gizmo Hit Bounds (debug)", nullptr, &g_xEngine.Gizmos().m_bDrawInteractionBounds);
		ImGui::EndMenu();
	}
	bPrefsChanged |= ImGui::MenuItem("Clear Console On Play", nullptr, &xPrefs.m_bClearConsoleOnPlay);
	if (bPrefsChanged)
	{
		xPrefs.Save();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Reset Layout"))
	{
		// Consumed by the next Render(): rebuilds the code-defined
		// default dock layout and recaptures floating windows.
		m_xEditorState.m_bResetDockLayout = true;
	}

	ImGui::EndMenu();
}

void Zenith_Editor::RenderHelpMenu()
{
	if (!ImGui::BeginMenu("Help"))
	{
		return;
	}
	ImGui::MenuItem("Keyboard Shortcuts", "F1", &m_xEditorState.m_xPanels.m_bShowShortcuts);
	ImGui::EndMenu();
}

#endif // ZENITH_TOOLS
