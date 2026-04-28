#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_UndoSystem.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#include "Panels/Zenith_EditorPanel_Memory.h"
#include "Panels/Zenith_EditorPanel_RenderGraph.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//==========================================================================
// Zenith_Editor — main menu bar
//
// Carved out of Zenith_Editor.cpp so each top-level menu (File/Edit/View)
// lives next to the dispatcher, rather than one end of a 1000+ LOC file.
// All symbols used here — ShowOpenFileDialog, ShowSaveFileDialog,
// Zenith_Editor::s_uGameCameraEntity, ClearSelection, ResetEditorCameraToDefaults,
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
			Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
			if (xActiveScene.IsValid())
			{
				Zenith_SceneManager::UnloadSceneForced(xActiveScene);
			}
			Zenith_Scene xNewScene = Zenith_SceneManager::CreateEmptyScene("Untitled");
			Zenith_SceneManager::SetActiveScene(xNewScene);

			ClearSelection();
			s_uGameCameraEntity = INVALID_ENTITY_ID;
			ResetEditorCameraToDefaults();
			Zenith_UndoSystem::Clear();
			Zenith_Log(LOG_CATEGORY_EDITOR, "New scene created (handle=%d, name='Untitled')", xNewScene.m_iHandle);
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
				Zenith_SceneManager::LoadSceneBlocking_ToolsOnly(strFilePath, SCENE_LOAD_ADDITIVE);
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
				Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
				Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
				if (pxSceneData)
				{
					pxSceneData->SaveToFile(strFilePath);
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
		bool bCanUndo = Zenith_UndoSystem::CanUndo();
		bool bCanRedo = Zenith_UndoSystem::CanRedo();

		if (ImGui::MenuItem("Undo", "Ctrl+Z", false, bCanUndo))
		{
			Zenith_UndoSystem::Undo();
		}

		// Show tooltip with undo description
		if (bCanUndo && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Undo: %s", Zenith_UndoSystem::GetUndoDescription());
		}

		if (ImGui::MenuItem("Redo", "Ctrl+Y", false, bCanRedo))
		{
			Zenith_UndoSystem::Redo();
		}

		// Show tooltip with redo description
		if (bCanRedo && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Redo: %s", Zenith_UndoSystem::GetRedoDescription());
		}

		ImGui::EndMenu();
	}
}

void Zenith_Editor::RenderViewMenu()
{
	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::MenuItem("Hierarchy", nullptr, s_bShowHierarchyPanel))
		{
			s_bShowHierarchyPanel = !s_bShowHierarchyPanel;
		}

		if (ImGui::MenuItem("Properties", nullptr, s_bShowPropertiesPanel))
		{
			s_bShowPropertiesPanel = !s_bShowPropertiesPanel;
		}

		if (ImGui::MenuItem("Console", nullptr, s_bShowConsolePanel))
		{
			s_bShowConsolePanel = !s_bShowConsolePanel;
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

		ImGui::Separator();

		if (ImGui::MenuItem("Animation State Machine Editor"))
		{
			// Zenith_AnimationStateMachineEditor::Toggle();  // TEMPORARILY DISABLED
		}

		ImGui::EndMenu();
	}
}

#endif // ZENITH_TOOLS
