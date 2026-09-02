#pragma once

#ifdef ZENITH_TOOLS

// ============================================================================
// Zenith_ImGuiWidgets
//
// Small, dependency-light ImGui widgets shared by EVERY inspector, including the
// component property panels in EntityComponent (layer 2) which must not include
// Editor/ (layer 4). Lives in Core for exactly that reason — same precedent as
// Core/Zenith_EditorWindowNames.h and Core/Zenith_EditorQuery.h.
//
// The widgets are plain functions over the current ImGui context; the editor's
// theme (Editor/Zenith_EditorUI) decides the colours they read from the style.
// ============================================================================

namespace Zenith_ImGuiWidgets
{
	// Unity/Unreal-style XYZ row: a label column, then three colour-tagged axis
	// buttons each followed by a drag field. Clicking an axis tag resets that
	// component to fResetValue. Returns true when any component changed this frame.
	// pfValues points at 3 floats and is written in place.
	bool Vec3Field(const char* szLabel, float* pfValues, float fSpeed, float fResetValue,
		const char* szFormat = "%.3f", float fMin = 0.0f, float fMax = 0.0f);

	// Width of the label column Vec3Field / PropertyLabel use, in pixels at the
	// current font scale. Inspectors that lay out their own rows can align to it.
	float GetLabelColumnWidth();

	// Label on the left of an inspector row: draws szLabel in the label column and
	// leaves the cursor at the value column with the next item stretched to the
	// remaining width. Pair with any single ImGui widget using a "##" label.
	void PropertyLabel(const char* szLabel);

	// A muted single-line note ("(Not implemented)", "3 instances") — TextDisabled
	// wrapped to the available width.
	void Note(const char* szText);
}

#endif // ZENITH_TOOLS
