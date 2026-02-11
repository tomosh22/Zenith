#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Toolbar.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

void Zenith_EditorPanelToolbar::Render(EditorMode& eEditorMode, EditorGizmoMode& eGizmoMode)
{
	ImGui::Begin("Toolbar");

	ImVec2 xButtonSize(80.0f, 32.0f);
	float fSpacing = ImGui::GetStyle().ItemSpacing.x;
	float fWindowWidth = ImGui::GetContentRegionAvail().x;

	// Row 1: Play/Pause + Stop buttons, centered
	float fPlayStopWidth = xButtonSize.x * 2.0f + fSpacing;
	float fPlayStartX = (fWindowWidth - fPlayStopWidth) * 0.5f;
	if (fPlayStartX < 0.0f)
		fPlayStartX = 0.0f;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fPlayStartX);

	const char* szPlayText = (eEditorMode == EditorMode::Playing) ? "Pause" : "Play";
	if (ImGui::Button(szPlayText, xButtonSize))
	{
		if (eEditorMode == EditorMode::Stopped)
		{
			Zenith_Editor::SetEditorMode(EditorMode::Playing);
		}
		else if (eEditorMode == EditorMode::Playing)
		{
			Zenith_Editor::SetEditorMode(EditorMode::Paused);
		}
		else if (eEditorMode == EditorMode::Paused)
		{
			Zenith_Editor::SetEditorMode(EditorMode::Playing);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Stop", xButtonSize))
	{
		Zenith_Editor::SetEditorMode(EditorMode::Stopped);
	}

	ImGui::Separator();

	// Scene selector dropdown
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();

		// Get active scene name for preview
		std::string strActiveSceneName = "No Scene";
		Zenith_SceneData* pxActiveData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxActiveData)
		{
			strActiveSceneName = pxActiveData->GetName();
			if (strActiveSceneName.empty())
				strActiveSceneName = "Untitled";
		}

		// Disable during play mode
		bool bDisabled = (eEditorMode != EditorMode::Stopped);
		if (bDisabled)
			ImGui::BeginDisabled();

		ImGui::Text("Active Scene:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::BeginCombo("##ActiveScene", strActiveSceneName.c_str()))
		{
			uint32_t uSceneCount = Zenith_SceneManager::GetLoadedSceneCount();
			for (uint32_t i = 0; i < uSceneCount; ++i)
			{
				Zenith_Scene xScene = Zenith_SceneManager::GetSceneAt(i);
				if (!xScene.IsValid())
					continue;
				if (xScene == xPersistentScene)
					continue;

				Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
				if (!pxSceneData)
					continue;

				std::string strName = pxSceneData->GetName();
				if (strName.empty())
					strName = "Untitled";

				bool bIsSelected = (xScene == xActiveScene);
				if (ImGui::Selectable(strName.c_str(), bIsSelected))
				{
					Zenith_SceneManager::SetActiveScene(xScene);
				}
				if (bIsSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// Registered Scenes dropdown (shows all build-index-registered scenes)
		uint32_t uBuildSceneCount = Zenith_SceneManager::GetBuildSceneCount();
		if (uBuildSceneCount > 0)
		{
			ImGui::Text("Load Scene:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(200.0f);
			if (ImGui::BeginCombo("##RegisteredScenes", "Select scene..."))
			{
				uint32_t uRegistrySize = Zenith_SceneManager::GetBuildIndexRegistrySize();
				for (uint32_t i = 0; i < uRegistrySize; ++i)
				{
					const std::string& strPath = Zenith_SceneManager::GetRegisteredScenePath(static_cast<int>(i));
					if (strPath.empty())
						continue;

					// Extract filename without extension for display
					std::string strDisplayName = strPath;
					size_t uLastSlash = strPath.find_last_of("/\\");
					if (uLastSlash != std::string::npos)
						strDisplayName = strPath.substr(uLastSlash + 1);
					size_t uLastDot = strDisplayName.find_last_of('.');
					if (uLastDot != std::string::npos)
						strDisplayName = strDisplayName.substr(0, uLastDot);

					if (ImGui::Selectable(strDisplayName.c_str(), false))
					{
						Zenith_Editor::RequestLoadRegisteredScene(static_cast<int>(i));
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", strPath.c_str());
					}
				}
				ImGui::EndCombo();
			}
		}

		if (bDisabled)
			ImGui::EndDisabled();
	}

	ImGui::Separator();

	// Row 2: Gizmo mode radio buttons, centered
	float fRadioWidth = 0.0f;
	{
		const char* aszLabels[] = { "Translate", "Rotate", "Scale" };
		for (int i = 0; i < 3; i++)
		{
			fRadioWidth += ImGui::GetFrameHeight() + fSpacing + ImGui::CalcTextSize(aszLabels[i]).x;
			if (i < 2)
				fRadioWidth += fSpacing;
		}
	}
	float fRadioStartX = (fWindowWidth - fRadioWidth) * 0.5f;
	if (fRadioStartX < 0.0f)
		fRadioStartX = 0.0f;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fRadioStartX);

	if (ImGui::RadioButton("Translate", eGizmoMode == EditorGizmoMode::Translate))
	{
		Zenith_Editor::SetGizmoMode(EditorGizmoMode::Translate);
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Rotate", eGizmoMode == EditorGizmoMode::Rotate))
	{
		Zenith_Editor::SetGizmoMode(EditorGizmoMode::Rotate);
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Scale", eGizmoMode == EditorGizmoMode::Scale))
	{
		Zenith_Editor::SetGizmoMode(EditorGizmoMode::Scale);
	}

	ImGui::End();
}

#endif // ZENITH_TOOLS
