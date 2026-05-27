#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Toolbar.h"

#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//-----------------------------------------------------------------------------
// Render - top-level dispatcher. Composes the toolbar from per-section helpers
// so each helper owns one coherent group of controls.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::Render(EditorMode& eEditorMode, EditorGizmoMode& eGizmoMode)
{
	ImGui::Begin("Toolbar");

	ImVec2 xButtonSize(80.0f, 32.0f);
	float fSpacing = ImGui::GetStyle().ItemSpacing.x;
	float fWindowWidth = ImGui::GetContentRegionAvail().x;

	RenderPlayModeButtons(eEditorMode, xButtonSize.x, xButtonSize.y, fSpacing, fWindowWidth);

	ImGui::Separator();

	RenderSceneSelectors(eEditorMode);

	ImGui::Separator();

	RenderGizmoModeRadios(eGizmoMode, fSpacing, fWindowWidth);

	ImGui::End();
}

//-----------------------------------------------------------------------------
// Row 1: Play/Pause and Stop buttons, centered. Clicking Play cycles through
// Stopped -> Playing -> Paused -> Playing.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::RenderPlayModeButtons(EditorMode& eEditorMode, float fButtonWidth, float fButtonHeight, float fSpacing, float fWindowWidth)
{
	ImVec2 xButtonSize(fButtonWidth, fButtonHeight);

	float fPlayStopWidth = fButtonWidth * 2.0f + fSpacing;
	float fPlayStartX = (fWindowWidth - fPlayStopWidth) * 0.5f;
	if (fPlayStartX < 0.0f)
		fPlayStartX = 0.0f;

	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + fPlayStartX);

	const char* szPlayText = (eEditorMode == EditorMode::Playing) ? "Pause" : "Play";
	if (ImGui::Button(szPlayText, xButtonSize))
	{
		if (eEditorMode == EditorMode::Stopped)
		{
			g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
		}
		else if (eEditorMode == EditorMode::Playing)
		{
			g_xEngine.Editor().SetEditorMode(EditorMode::Paused);
		}
		else if (eEditorMode == EditorMode::Paused)
		{
			g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Stop", xButtonSize))
	{
		g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	}
}

//-----------------------------------------------------------------------------
// Scene selectors block. Both combos are disabled while the editor is in Play
// or Pause mode (scene swaps are only permitted from Stopped).
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::RenderSceneSelectors(EditorMode& eEditorMode)
{
	bool bDisabled = (eEditorMode != EditorMode::Stopped);
	if (bDisabled)
		ImGui::BeginDisabled();

	RenderActiveSceneCombo();

	if (g_xEngine.SceneRegistry().GetBuildSceneCount() > 0)
	{
		RenderRegisteredScenesCombo();
	}

	if (bDisabled)
		ImGui::EndDisabled();
}

//-----------------------------------------------------------------------------
// Active scene combo: pick among currently-loaded, non-persistent scenes.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::RenderActiveSceneCombo()
{
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_Scene xPersistentScene = g_xEngine.SceneRegistry().GetPersistentScene();

	// Get active scene name for preview
	std::string strActiveSceneName = "No Scene";
	Zenith_SceneData* pxActiveData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	if (pxActiveData)
	{
		strActiveSceneName = pxActiveData->GetName();
		if (strActiveSceneName.empty())
			strActiveSceneName = "Untitled";
	}

	ImGui::Text("Active Scene:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##ActiveScene", strActiveSceneName.c_str()))
	{
		uint32_t uSceneCount = g_xEngine.SceneRegistry().GetLoadedSceneCount();
		for (uint32_t i = 0; i < uSceneCount; ++i)
		{
			Zenith_Scene xScene = g_xEngine.SceneRegistry().GetSceneAt(i);
			if (!xScene.IsValid())
				continue;
			if (xScene == xPersistentScene)
				continue;

			Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xScene);
			if (!pxSceneData)
				continue;

			std::string strName = pxSceneData->GetName();
			if (strName.empty())
				strName = "Untitled";

			bool bIsSelected = (xScene == xActiveScene);
			if (ImGui::Selectable(strName.c_str(), bIsSelected))
			{
				g_xEngine.SceneRegistry().SetActiveScene(xScene);
			}
			if (bIsSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

//-----------------------------------------------------------------------------
// Registered scenes combo: triggers a deferred load by build-index path.
// Display name is the filename without extension; tooltip shows the full path.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::RenderRegisteredScenesCombo()
{
	ImGui::Text("Load Scene:");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	if (ImGui::BeginCombo("##RegisteredScenes", "Select scene..."))
	{
		uint32_t uRegistrySize = g_xEngine.SceneRegistry().GetBuildIndexRegistrySize();
		for (uint32_t i = 0; i < uRegistrySize; ++i)
		{
			const std::string& strPath = g_xEngine.SceneRegistry().GetRegisteredScenePath(static_cast<int>(i));
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
				g_xEngine.Editor().RequestLoadRegisteredScene(static_cast<int>(i));
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", strPath.c_str());
			}
		}
		ImGui::EndCombo();
	}
}

//-----------------------------------------------------------------------------
// Row 2: Translate/Rotate/Scale radio buttons, centered. Total width is
// precomputed so the row can be centered in the toolbar.
//-----------------------------------------------------------------------------
void Zenith_EditorPanelToolbar::RenderGizmoModeRadios(EditorGizmoMode& eGizmoMode, float fSpacing, float fWindowWidth)
{
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
		g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Translate);
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Rotate", eGizmoMode == EditorGizmoMode::Rotate))
	{
		g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Rotate);
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Scale", eGizmoMode == EditorGizmoMode::Scale))
	{
		g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Scale);
	}
}

#endif // ZENITH_TOOLS
