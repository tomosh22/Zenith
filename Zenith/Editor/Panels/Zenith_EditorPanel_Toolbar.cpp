#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Toolbar.h"

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
