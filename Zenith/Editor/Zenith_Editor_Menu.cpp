#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_Editor.h"
#include "Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Zenith_EditorSceneAccess.h"

#include "Panels/Zenith_EditorPanel_Memory.h"
#include "Panels/Zenith_EditorPanel_RenderGraph.h"
#include "Panels/Zenith_EditorPanel_VariantEditor.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//==========================================================================
// Zenith_Editor — main menu bar
//
// Carved out of Zenith_Editor.cpp so each top-level menu (File/Edit/View)
// lives next to the dispatcher, rather than one end of a 1000+ LOC file.
// All symbols used here — ShowOpenFileDialog, ShowSaveFileDialog,
// Zenith_Editor::g_xEngine.Editor().m_uGameCameraEntity, ClearSelection, ResetEditorCameraToDefaults,
// RequestLoadSceneFromFile — have external linkage already, so the extraction
// did not require any changes to the Zenith_Editor interface.
//==========================================================================

// Forward declarations for file dialog helpers defined in Zenith_Editor.cpp
// (they have external linkage even though they look like file-static helpers).
#ifdef _WIN32
std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt);
std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename);
#endif

void Zenith_Editor::RenderMainMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		RenderFileMenu();
		RenderEditMenu();
		RenderViewMenu();

		ImGui::EndMenuBar();
	}
}

void Zenith_Editor::RenderFileMenu()
{
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("New Scene"))
		{
			// C.10: route "New Scene" through the public SceneManager API instead of
			// reaching past it to pxSceneData->Reset(). The old path wiped entities in
			// place without firing any of SceneUnloading/SceneUnloaded/SceneLoaded/
			// ActiveSceneChanged, leaving subscribers unaware that the world had changed,
			// and leaving m_strName/m_strPath/m_iBuildIndex from the previous scene stuck
			// on the handle. Now we force-unload the old scene (UnloadSceneForced bypasses
			// CanUnloadScene's last-scene guard — we're about to create its replacement),
			// create a fresh empty scene, and make it active. All four callbacks fire in
			// the documented order; the handle recycles; the new scene has a clean identity.
			//
			// Safe to call directly from the menu callback because RenderImGui runs after
			// all render tasks have completed (see Zenith_Editor.cpp's main-loop comment).
			Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
			if (xActiveScene.IsValid())
			{
				g_xEngine.Scenes().UnloadSceneForced(xActiveScene);
			}
			Zenith_Scene xNewScene = g_xEngine.Scenes().LoadScene("Untitled", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xNewScene);

			ClearSelection();
			g_xEngine.Editor().m_uGameCameraEntity = INVALID_ENTITY_ID;
			ResetEditorCameraToDefaults();
			g_xEngine.UndoSystem().Clear();
			Zenith_Log(LOG_CATEGORY_EDITOR, "New scene created (handle=%d, name='Untitled')", xNewScene.GetHandle());
		}

		if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
		{
#ifdef _WIN32
			std::string strFilePath = ShowOpenFileDialog(
				"Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0",
				ZENITH_SCENE_EXT + 1);
			if (!strFilePath.empty())
			{
				RequestLoadSceneFromFile(strFilePath);
			}
#endif
		}

		if (ImGui::MenuItem("Open Scene Additive"))
		{
#ifdef _WIN32
			std::string strFilePath = ShowOpenFileDialog(
				"Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0",
				ZENITH_SCENE_EXT + 1);
			if (!strFilePath.empty())
			{
				g_xEngine.Scenes().LoadScene(strFilePath, SCENE_LOAD_ADDITIVE);
				Zenith_Log(LOG_CATEGORY_EDITOR, "Scene loaded additively: %s", strFilePath.c_str());
			}
#endif
		}

		if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		{
#ifdef _WIN32
			std::string strFilePath = ShowSaveFileDialog(
				"Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0",
				ZENITH_SCENE_EXT + 1,
				"scene" ZENITH_SCENE_EXT);
			if (!strFilePath.empty())
			{
				// Safe to call directly - no render tasks active
				Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
				Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
				if (pxSceneData)
				{
					Zenith_EditorSceneAccess::SaveToFile(pxSceneData, strFilePath);
					Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved to %s", strFilePath.c_str());
				}
			}
#endif
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Exit"))
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Exit - Not yet implemented");
		}

		ImGui::EndMenu();
	}
}

void Zenith_Editor::RenderEditMenu()
{
	if (ImGui::BeginMenu("Edit"))
	{
		bool bCanUndo = g_xEngine.UndoSystem().CanUndo();
		bool bCanRedo = g_xEngine.UndoSystem().CanRedo();

		if (ImGui::MenuItem("Undo", "Ctrl+Z", false, bCanUndo))
		{
			g_xEngine.UndoSystem().Undo();
		}

		// Show tooltip with undo description
		if (bCanUndo && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Undo: %s", g_xEngine.UndoSystem().GetUndoDescription());
		}

		if (ImGui::MenuItem("Redo", "Ctrl+Y", false, bCanRedo))
		{
			g_xEngine.UndoSystem().Redo();
		}

		// Show tooltip with redo description
		if (bCanRedo && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Redo: %s", g_xEngine.UndoSystem().GetRedoDescription());
		}

		ImGui::EndMenu();
	}
}

void Zenith_Editor::RenderViewMenu()
{
	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Hierarchy", nullptr, g_xEngine.Editor().m_bShowHierarchyPanel))
		{
			g_xEngine.Editor().m_bShowHierarchyPanel = !g_xEngine.Editor().m_bShowHierarchyPanel;
		}

		if (ImGui::MenuItem("Properties", nullptr, g_xEngine.Editor().m_bShowPropertiesPanel))
		{
			g_xEngine.Editor().m_bShowPropertiesPanel = !g_xEngine.Editor().m_bShowPropertiesPanel;
		}

		if (ImGui::MenuItem("Console", nullptr, g_xEngine.Editor().m_bShowConsolePanel))
		{
			g_xEngine.Editor().m_bShowConsolePanel = !g_xEngine.Editor().m_bShowConsolePanel;
		}

#ifdef ZENITH_MEMORY_MANAGEMENT_ENABLED
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

		if (ImGui::MenuItem("Animation State Machine Editor"))
		{
			// Zenith_AnimationStateMachineEditor::Toggle();  // TEMPORARILY DISABLED
		}

		ImGui::EndMenu();
	}
}

#endif // ZENITH_TOOLS
