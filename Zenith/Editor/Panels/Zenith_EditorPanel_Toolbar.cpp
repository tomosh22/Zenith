#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Toolbar.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorUI.h"
#include "Editor/Zenith_UndoSystem.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include "imgui.h"

namespace
{
	constexpr float fSTRIP_HEIGHT_1X = 38.0f;
	constexpr float fCOMBO_WIDTH_1X = 150.0f;

	// The transport group's total width, so it can be centred in the strip.
	float TransportGroupWidth(Zenith_Editor& xEditor)
	{
		const float fButton = Zenith_EditorUI::Px(28.0f);
		const float fGap = Zenith_EditorUI::Px(4.0f);
		const bool bStopped = xEditor.GetEditorMode() == EditorMode::Stopped;
		const float fPlayWidth = bStopped
			? Zenith_EditorUI::Px(8.0f) + fButton * 0.55f + Zenith_EditorUI::Px(6.0f) + ImGui::CalcTextSize("Play").x + Zenith_EditorUI::Px(8.0f)
			: fButton;
		return fPlayWidth + fGap + fButton + fGap + fButton;
	}
}

namespace Zenith_EditorPanelToolbar
{

float GetHeight()
{
	return Zenith_EditorUI::Px(fSTRIP_HEIGHT_1X);
}

//-----------------------------------------------------------------------------
// Render - lays the three groups out on one line: left group flows from the
// left edge, the transport group is centred, the scene selectors are pinned
// to the right edge.
//-----------------------------------------------------------------------------
void Render(Zenith_Editor& xEditor, float fHeight)
{
	const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
	const ImVec2 xMin = ImGui::GetCursorScreenPos();
	const float fWidth = ImGui::GetContentRegionAvail().x;
	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	pxDraw->AddRectFilled(xMin, ImVec2(xMin.x + fWidth, xMin.y + fHeight), xP.m_uToolbarBg);
	pxDraw->AddLine(ImVec2(xMin.x, xMin.y + fHeight - 1.0f), ImVec2(xMin.x + fWidth, xMin.y + fHeight - 1.0f), xP.m_uBorder, 1.0f);

	const float fButton = Zenith_EditorUI::Px(28.0f);
	const float fTop = xMin.y + (fHeight - fButton) * 0.5f;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(Zenith_EditorUI::Px(4.0f), 0.0f));

	ImGui::SetCursorScreenPos(ImVec2(xMin.x + Zenith_EditorUI::Px(8.0f), fTop));
	RenderHistoryButtons();
	Zenith_EditorUI::ToolbarSeparator();
	RenderGizmoButtons(xEditor);

	RenderTransportButtons(xEditor, fWidth, fTop);
	RenderSceneSelectors(xEditor, xMin.x + fWidth - Zenith_EditorUI::Px(8.0f), fTop);

	ImGui::PopStyleVar();

	// Leave the cursor below the strip for whatever the host draws next.
	ImGui::SetCursorScreenPos(ImVec2(xMin.x, xMin.y + fHeight));
}

//-----------------------------------------------------------------------------
// Undo / Redo
//-----------------------------------------------------------------------------
void RenderHistoryButtons()
{
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	Zenith_EditorIconButtonOptions xOpts;
	xOpts.m_fSize = Zenith_EditorUI::Px(28.0f);

	char acTooltip[160];
	xOpts.m_bEnabled = xUndo.CanUndo();
	snprintf(acTooltip, sizeof(acTooltip), xOpts.m_bEnabled ? "Undo %s (Ctrl+Z)" : "Nothing to undo", xUndo.GetUndoDescription());
	if (Zenith_EditorUI::IconButton("undo", Zenith_EditorIcon::Undo, acTooltip, xOpts))
	{
		xUndo.Undo();
	}
	ImGui::SameLine();
	xOpts.m_bEnabled = xUndo.CanRedo();
	snprintf(acTooltip, sizeof(acTooltip), xOpts.m_bEnabled ? "Redo %s (Ctrl+Y)" : "Nothing to redo", xUndo.GetRedoDescription());
	if (Zenith_EditorUI::IconButton("redo", Zenith_EditorIcon::Redo, acTooltip, xOpts))
	{
		xUndo.Redo();
	}
}

//-----------------------------------------------------------------------------
// Gizmo mode, space and snapping
//-----------------------------------------------------------------------------
void RenderGizmoButtons(Zenith_Editor& xEditor)
{
	Zenith_EditorIconButtonOptions xOpts;
	xOpts.m_fSize = Zenith_EditorUI::Px(28.0f);
	const EditorGizmoMode eMode = xEditor.GetGizmoMode();

	struct ModeButton { const char* m_szID; Zenith_EditorIcon m_eIcon; const char* m_szTip; EditorGizmoMode m_eMode; };
	const ModeButton axModes[] =
	{
		{ "translate", Zenith_EditorIcon::Translate, "Move (W)",   EditorGizmoMode::Translate },
		{ "rotate",    Zenith_EditorIcon::Rotate,    "Rotate (E)", EditorGizmoMode::Rotate },
		{ "scale",     Zenith_EditorIcon::Scale,     "Scale (R)",  EditorGizmoMode::Scale },
	};
	for (const ModeButton& xButton : axModes)
	{
		xOpts.m_bSelected = (eMode == xButton.m_eMode);
		if (Zenith_EditorUI::IconButton(xButton.m_szID, xButton.m_eIcon, xButton.m_szTip, xOpts))
		{
			xEditor.SetGizmoMode(xButton.m_eMode);
		}
		ImGui::SameLine();
	}

	Zenith_EditorUI::ToolbarSeparator();

	Zenith_EditorPrefs& xPrefs = xEditor.m_xEditorState.m_xPrefs;
	xOpts.m_bSelected = xPrefs.m_bGizmoLocalSpace;
	if (Zenith_EditorUI::IconButton("space", xPrefs.m_bGizmoLocalSpace ? Zenith_EditorIcon::Local : Zenith_EditorIcon::World,
		xPrefs.m_bGizmoLocalSpace ? "Local space: handles follow the entity (X)" : "World space: handles stay axis-aligned (X)", xOpts))
	{
		xPrefs.m_bGizmoLocalSpace = !xPrefs.m_bGizmoLocalSpace;
		xPrefs.Save();
	}
	ImGui::SameLine();

	xOpts.m_bSelected = xPrefs.m_bSnapEnabled;
	char acSnapTip[128];
	snprintf(acSnapTip, sizeof(acSnapTip), "Snapping %s: %.2g m, %.0f deg, %.2g scale (hold Ctrl for one drag; right-click to edit)",
		xPrefs.m_bSnapEnabled ? "on" : "off", xPrefs.m_fSnapMove, xPrefs.m_fSnapRotateDegrees, xPrefs.m_fSnapScale);
	if (Zenith_EditorUI::IconButton("snap", Zenith_EditorIcon::Snap, acSnapTip, xOpts))
	{
		xPrefs.m_bSnapEnabled = !xPrefs.m_bSnapEnabled;
		xPrefs.Save();
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup("SnapSettings");
	}
	if (ImGui::BeginPopup("SnapSettings"))
	{
		bool bChanged = false;
		ImGui::TextDisabled("Snap increments");
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
		bChanged |= ImGui::DragFloat("Move", &xPrefs.m_fSnapMove, 0.05f, 0.01f, 100.0f, "%.2f m");
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
		bChanged |= ImGui::DragFloat("Rotate", &xPrefs.m_fSnapRotateDegrees, 1.0f, 1.0f, 90.0f, "%.0f deg");
		ImGui::SetNextItemWidth(Zenith_EditorUI::Px(120.0f));
		bChanged |= ImGui::DragFloat("Scale", &xPrefs.m_fSnapScale, 0.01f, 0.01f, 10.0f, "%.2f");
		if (bChanged)
		{
			xPrefs.Save();
		}
		ImGui::EndPopup();
	}
}

//-----------------------------------------------------------------------------
// Play / Pause / Stop, centred. Play reads "Play" while stopped and turns into
// the green-lit play icon while running, like Unity's transport.
//-----------------------------------------------------------------------------
void RenderTransportButtons(Zenith_Editor& xEditor, float fStripWidth, float fTop)
{
	const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
	const EditorMode eMode = xEditor.GetEditorMode();
	const float fGroupWidth = TransportGroupWidth(xEditor);
	const float fStripLeft = ImGui::GetWindowPos().x;
	ImGui::SetCursorScreenPos(ImVec2(fStripLeft + (fStripWidth - fGroupWidth) * 0.5f, fTop));

	Zenith_EditorIconButtonOptions xOpts;
	xOpts.m_fSize = Zenith_EditorUI::Px(28.0f);
	xOpts.m_bFrameless = false;

	if (eMode == EditorMode::Stopped)
	{
		xOpts.m_uTint = xP.m_uPlay;
		if (Zenith_EditorUI::IconTextButton("play", Zenith_EditorIcon::Play, "Play", "Enter Play mode (Ctrl+P)", xOpts))
		{
			Zenith_EditorActions::TogglePlay();
		}
	}
	else
	{
		xOpts.m_bSelected = true;
		xOpts.m_uTint = xP.m_uPlay;
		if (Zenith_EditorUI::IconButton("play", Zenith_EditorIcon::Play, "Playing - click to stop (Ctrl+P)", xOpts))
		{
			Zenith_EditorActions::TogglePlay();
		}
		xOpts.m_bSelected = false;
	}
	ImGui::SameLine();

	xOpts.m_uTint = (eMode == EditorMode::Paused) ? xP.m_uPause : 0;
	xOpts.m_bSelected = (eMode == EditorMode::Paused);
	xOpts.m_bEnabled = (eMode != EditorMode::Stopped);
	if (Zenith_EditorUI::IconButton("pause", Zenith_EditorIcon::Pause, eMode == EditorMode::Paused ? "Resume (Ctrl+Shift+P)" : "Pause (Ctrl+Shift+P)", xOpts))
	{
		Zenith_EditorActions::TogglePause();
	}
	ImGui::SameLine();

	xOpts.m_uTint = (eMode != EditorMode::Stopped) ? xP.m_uStop : 0;
	xOpts.m_bSelected = false;
	if (Zenith_EditorUI::IconButton("stop", Zenith_EditorIcon::Stop, "Stop and restore the scene", xOpts))
	{
		Zenith_EditorActions::Stop();
	}
}

//-----------------------------------------------------------------------------
// Scene selectors, pinned to the right. Both combos are disabled while the
// editor is in Play or Pause mode (scene swaps are only permitted from Stopped).
//-----------------------------------------------------------------------------
void RenderSceneSelectors(Zenith_Editor& xEditor, float fRightEdge, float fRowTop)
{
	const bool bHasRegistry = g_xEngine.Scenes().GetBuildIndexRegistrySize() > 0;
	const float fCombo = Zenith_EditorUI::Px(fCOMBO_WIDTH_1X);
	const float fGap = Zenith_EditorUI::Px(6.0f);
	float fTotal = fCombo;
	if (bHasRegistry)
	{
		fTotal += fGap + Zenith_EditorUI::Px(36.0f);
	}
	const float fTop = fRowTop + (Zenith_EditorUI::Px(28.0f) - ImGui::GetFrameHeight()) * 0.5f;
	ImGui::SetCursorScreenPos(ImVec2(fRightEdge - fTotal, fTop));

	const bool bDisabled = (xEditor.GetEditorMode() != EditorMode::Stopped);
	if (bDisabled) ImGui::BeginDisabled();

	RenderActiveSceneCombo();
	if (bHasRegistry)
	{
		ImGui::SameLine(0.0f, fGap);
		RenderRegisteredScenesCombo();
	}

	if (bDisabled) ImGui::EndDisabled();
}

//-----------------------------------------------------------------------------
// Active scene combo: pick among currently-loaded, non-persistent scenes.
//-----------------------------------------------------------------------------
void RenderActiveSceneCombo()
{
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	Zenith_Scene xActiveScene = xScenes.GetActiveScene();
	Zenith_Scene xPersistentScene = xScenes.GetPersistentScene();

	std::string strActiveSceneName = "No Scene";
	if (xScenes.GetSceneData(xActiveScene))
	{
		const Zenith_SceneInfo xInfo = xScenes.GetSceneInfo(xActiveScene);
		strActiveSceneName = xInfo.m_strName.empty() ? "Untitled" : xInfo.m_strName;
		if (xInfo.m_bHasUnsavedChanges) strActiveSceneName += "*";
	}

	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(fCOMBO_WIDTH_1X));
	if (!ImGui::BeginCombo("##ActiveScene", strActiveSceneName.c_str()))
	{
		ImGui::SetItemTooltip("Active scene: new entities go here");
		return;
	}
	// GetSceneAt returns INVALID_SCENE past the last visible scene, so walk
	// slot order until that sentinel.
	for (uint32_t i = 0; ; ++i)
	{
		Zenith_Scene xScene = xScenes.GetSceneAt(i);
		if (!xScene.IsValid())
			break;
		if (xScene == xPersistentScene || !xScenes.GetSceneData(xScene))
			continue;

		std::string strName = xScenes.GetSceneInfo(xScene).m_strName;
		if (strName.empty())
			strName = "Untitled";

		const bool bIsSelected = (xScene == xActiveScene);
		if (ImGui::Selectable(strName.c_str(), bIsSelected))
		{
			xScenes.SetActiveScene(xScene);
		}
		if (bIsSelected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
}

//-----------------------------------------------------------------------------
// Registered scenes combo: triggers a deferred load by build-index path.
// Display name is the filename without extension; tooltip shows the full path.
//-----------------------------------------------------------------------------
void RenderRegisteredScenesCombo()
{
	Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
	ImGui::SetNextItemWidth(Zenith_EditorUI::Px(36.0f));
	if (!ImGui::BeginCombo("##RegisteredScenes", "", ImGuiComboFlags_NoPreview))
	{
		ImGui::SetItemTooltip("Load a registered scene");
		return;
	}
	ImGui::TextDisabled("Registered scenes");
	ImGui::Separator();
	const uint32_t uRegistrySize = xScenes.GetBuildIndexRegistrySize();
	for (uint32_t i = 0; i < uRegistrySize; ++i)
	{
		const std::string& strPath = xScenes.GetRegisteredScenePath(static_cast<int>(i));
		if (strPath.empty())
			continue;

		std::string strDisplayName = strPath;
		const size_t uLastSlash = strPath.find_last_of("/\\");
		if (uLastSlash != std::string::npos)
			strDisplayName = strPath.substr(uLastSlash + 1);
		const size_t uLastDot = strDisplayName.find_last_of('.');
		if (uLastDot != std::string::npos)
			strDisplayName = strDisplayName.substr(0, uLastDot);

		if (ImGui::Selectable(strDisplayName.c_str(), false))
		{
			g_xEngine.Editor().RequestLoadRegisteredScene(static_cast<int>(i));
		}
		ImGui::SetItemTooltip("%s", strPath.c_str());
	}
	ImGui::EndCombo();
}

} // namespace Zenith_EditorPanelToolbar

#endif // ZENITH_TOOLS
