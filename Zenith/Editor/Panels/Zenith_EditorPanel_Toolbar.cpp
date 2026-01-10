#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Toolbar.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

void Zenith_EditorPanelToolbar::Render(EditorMode& eEditorMode, EditorGizmoMode& eGizmoMode)
{
	ImGui::Begin("Toolbar");

	// Play/Pause/Stop buttons
	const char* playText = (eEditorMode == EditorMode::Playing) ? "Pause" : "Play";
	if (ImGui::Button(playText))
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

	if (ImGui::Button("Stop"))
	{
		Zenith_Editor::SetEditorMode(EditorMode::Stopped);
	}

	ImGui::Separator();

	// Gizmo mode buttons
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
