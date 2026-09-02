#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_StatusBar.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Core/FrameContext.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_EditorUI.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Panels/Zenith_EditorPanel_Console.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace
{
	u_int CountEntitiesInLoadedScenes()
	{
		u_int uTotal = 0;
		for (uint32_t i = 0; ; ++i)
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
			if (!xScene.IsValid()) break;
			if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene))
			{
				uTotal += pxSceneData->GetActiveEntities().GetSize();
			}
		}
		return uTotal;
	}

	void RenderLatestLog(Zenith_Editor& xEditor, float fWidth)
	{
		const Zenith_Vector<ConsoleLogEntry>& xLogs = xEditor.m_xEditorState.m_xConsole.m_xLogs;
		if (xLogs.GetSize() == 0)
		{
			ImGui::TextDisabled("Ready");
			return;
		}
		const ConsoleLogEntry& xEntry = xLogs.GetBack();
		const float fIcon = ImGui::GetFontSize();
		const ImVec2 xStart = ImGui::GetCursorScreenPos();

		// One click on the message reveals the Console.
		fWidth = ImMax(fWidth, 1.0f);
		ImGui::InvisibleButton("##latestlog", ImVec2(fWidth, ImGui::GetFrameHeight()));
		if (ImGui::IsItemClicked())
		{
			xEditor.m_xEditorState.m_xPanels.m_bShowConsole = true;
			ImGui::SetWindowFocus(szEDITOR_WINDOW_CONSOLE);
		}
		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		const ImU32 uColour = (xEntry.m_eLevel == ConsoleLogEntry::LogLevel::Info)
			? Zenith_EditorUI::Palette().m_uTextDim
			: Zenith_EditorPanelConsole::LevelColour(xEntry.m_eLevel);
		const float fMidY = xStart.y + ImGui::GetFrameHeight() * 0.5f;
		Zenith_EditorUI::DrawIcon(pxDraw, Zenith_EditorPanelConsole::LevelIcon(xEntry.m_eLevel), ImVec2(xStart.x + fIcon * 0.5f, fMidY), fIcon * 0.8f, uColour);

		const ImVec2 xTextPos(xStart.x + fIcon + Zenith_EditorUI::Px(6.0f), fMidY - ImGui::GetFontSize() * 0.5f);
		const ImVec4 xClip(xStart.x, xStart.y, xStart.x + fWidth, xStart.y + ImGui::GetFrameHeight());
		pxDraw->AddText(nullptr, 0.0f, xTextPos, uColour, xEntry.m_strMessage.c_str(), nullptr, 0.0f, &xClip);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("[%s] %s\n%s", xEntry.m_strTimestamp.c_str(), Zenith_GetLogCategoryName(xEntry.m_eCategory), xEntry.m_strMessage.c_str());
		}
	}
}

namespace Zenith_EditorPanelStatusBar
{

float GetHeight()
{
	return Zenith_EditorUI::Px(24.0f);
}

void Render(Zenith_Editor& xEditor, float fHeight)
{
	const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
	const ImVec2 xMin = ImGui::GetCursorScreenPos();
	const float fWidth = ImGui::GetContentRegionAvail().x;
	ImDrawList* pxDraw = ImGui::GetWindowDrawList();
	pxDraw->AddRectFilled(xMin, ImVec2(xMin.x + fWidth, xMin.y + fHeight), xP.m_uStatusBg);
	pxDraw->AddLine(xMin, ImVec2(xMin.x + fWidth, xMin.y), xP.m_uBorder, 1.0f);

	Zenith_EditorUI::PushSmallFont();
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Zenith_EditorUI::Px(6.0f), (fHeight - ImGui::GetFontSize()) * 0.5f));

	// Right-hand facts are laid out first (measured), the log gets the rest.
	char acFacts[256];
	const u_int uEntities = CountEntitiesInLoadedScenes();
	const size_t uSelected = xEditor.GetSelectionCount();
	const float fMs = Zenith_EditorUI::SmoothedFrameMs(g_xEngine.Frame().GetDt());
	const float fFps = fMs > 0.0f ? 1000.0f / fMs : 0.0f;
	Zenith_UndoSystem& xUndo = g_xEngine.UndoSystem();
	snprintf(acFacts, sizeof(acFacts), "%u entities   %zu selected   %.0f fps  %.1f ms   undo %u / redo %u",
		uEntities, uSelected, fFps, fMs, xUndo.GetUndoStackSize(), xUndo.GetRedoStackSize());
	const float fFactsWidth = ImGui::CalcTextSize(acFacts).x + Zenith_EditorUI::Px(16.0f);

	ImGui::SetCursorScreenPos(ImVec2(xMin.x + Zenith_EditorUI::Px(8.0f), xMin.y + (fHeight - ImGui::GetFrameHeight()) * 0.5f));
	RenderLatestLog(xEditor, fWidth - fFactsWidth - Zenith_EditorUI::Px(16.0f));

	ImGui::SameLine(fWidth - fFactsWidth);
	ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xP.m_uTextDim), "%s", acFacts);

	ImGui::PopStyleVar();
	Zenith_EditorUI::PopFont();
}

} // namespace Zenith_EditorPanelStatusBar

#endif // ZENITH_TOOLS
