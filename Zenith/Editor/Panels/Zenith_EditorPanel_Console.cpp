#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Console.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Editor/Zenith_EditorUI.h"

#include "imgui.h"

#include <cctype>
#include <cstring>

namespace Zenith_EditorPanelConsole
{

ImU32 LevelColour(ConsoleLogEntry::LogLevel eLevel)
{
	const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
	switch (eLevel)
	{
	case ConsoleLogEntry::LogLevel::Warning: return xP.m_uWarning;
	case ConsoleLogEntry::LogLevel::Error:   return xP.m_uError;
	default:                                 return xP.m_uText;
	}
}

Zenith_EditorIcon LevelIcon(ConsoleLogEntry::LogLevel eLevel)
{
	switch (eLevel)
	{
	case ConsoleLogEntry::LogLevel::Warning: return Zenith_EditorIcon::Warning;
	case ConsoleLogEntry::LogLevel::Error:   return Zenith_EditorIcon::Error;
	default:                                 return Zenith_EditorIcon::Info;
	}
}

namespace
{
	bool LevelEnabled(ConsoleLogEntry::LogLevel eLevel, const Zenith_EditorConsoleState& xState)
	{
		switch (eLevel)
		{
		case ConsoleLogEntry::LogLevel::Info:    return xState.m_bShowInfo;
		case ConsoleLogEntry::LogLevel::Warning: return xState.m_bShowWarnings;
		case ConsoleLogEntry::LogLevel::Error:   return xState.m_bShowErrors;
		}
		return false;
	}

	struct LevelCounts
	{
		u_int m_uInfo = 0;
		u_int m_uWarnings = 0;
		u_int m_uErrors = 0;
	};

	LevelCounts CountLevels(const Zenith_Vector<ConsoleLogEntry>& xLogs)
	{
		LevelCounts xCounts;
		for (u_int u = 0; u < xLogs.GetSize(); ++u)
		{
			switch (xLogs.Get(u).m_eLevel)
			{
			case ConsoleLogEntry::LogLevel::Info:    ++xCounts.m_uInfo; break;
			case ConsoleLogEntry::LogLevel::Warning: ++xCounts.m_uWarnings; break;
			case ConsoleLogEntry::LogLevel::Error:   ++xCounts.m_uErrors; break;
			}
		}
		return xCounts;
	}

	// A toggle drawn as an icon + count pill; dimmed when off.
	void LevelToggle(const char* szID, Zenith_EditorIcon eIcon, ImU32 uColour, u_int uCount, bool& bEnabled, const char* szTooltip)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		char acLabel[32];
		snprintf(acLabel, sizeof(acLabel), "%u", uCount);
		const float fHeight = ImGui::GetFrameHeight();
		const float fIcon = fHeight * 0.6f;
		const float fWidth = fIcon + ImGui::CalcTextSize(acLabel).x + Zenith_EditorUI::Px(16.0f);
		const ImVec2 xMin = ImGui::GetCursorScreenPos();
		ImGui::PushID(szID);
		const bool bClicked = ImGui::InvisibleButton("##toggle", ImVec2(fWidth, fHeight));
		const bool bHovered = ImGui::IsItemHovered();
		ImGui::PopID();
		if (bClicked) bEnabled = !bEnabled;

		ImDrawList* pxDraw = ImGui::GetWindowDrawList();
		const ImU32 uBg = bEnabled ? (bHovered ? xP.m_uFrameHover : xP.m_uFrame) : (bHovered ? xP.m_uFrame : 0);
		if (uBg != 0) pxDraw->AddRectFilled(xMin, ImVec2(xMin.x + fWidth, xMin.y + fHeight), uBg, fHeight * 0.5f);
		const ImU32 uFg = bEnabled ? uColour : xP.m_uTextDim;
		Zenith_EditorUI::DrawIcon(pxDraw, eIcon, ImVec2(xMin.x + Zenith_EditorUI::Px(6.0f) + fIcon * 0.5f, xMin.y + fHeight * 0.5f), fIcon, uFg);
		pxDraw->AddText(ImVec2(xMin.x + Zenith_EditorUI::Px(10.0f) + fIcon, xMin.y + (fHeight - ImGui::GetFontSize()) * 0.5f), uFg, acLabel);
		if (bHovered) ImGui::SetTooltip("%s", szTooltip);
	}

	void RenderToolbar(Zenith_EditorConsoleState& xState)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		Zenith_EditorIconButtonOptions xOpts;
		xOpts.m_fSize = ImGui::GetFrameHeight();

		if (Zenith_EditorUI::IconButton("clear", Zenith_EditorIcon::Trash, "Clear the console", xOpts))
		{
			xState.m_xLogs.Clear();
		}
		ImGui::SameLine();
		xOpts.m_bSelected = xState.m_bCollapse;
		if (Zenith_EditorUI::IconButton("collapse", Zenith_EditorIcon::Copy, "Collapse repeated messages", xOpts))
		{
			xState.m_bCollapse = !xState.m_bCollapse;
		}
		ImGui::SameLine();
		xOpts.m_bSelected = xState.m_bAutoScroll;
		if (Zenith_EditorUI::IconButton("autoscroll", Zenith_EditorIcon::ArrowDown, "Auto-scroll to the newest message", xOpts))
		{
			xState.m_bAutoScroll = !xState.m_bAutoScroll;
		}
		xOpts.m_bSelected = false;

		Zenith_EditorUI::ToolbarSeparator();

		const LevelCounts xCounts = CountLevels(xState.m_xLogs);
		LevelToggle("info", Zenith_EditorIcon::Info, xP.m_uInfo, xCounts.m_uInfo, xState.m_bShowInfo, "Show info messages");
		ImGui::SameLine();
		LevelToggle("warn", Zenith_EditorIcon::Warning, xP.m_uWarning, xCounts.m_uWarnings, xState.m_bShowWarnings, "Show warnings");
		ImGui::SameLine();
		LevelToggle("err", Zenith_EditorIcon::Error, xP.m_uError, xCounts.m_uErrors, xState.m_bShowErrors, "Show errors");

		Zenith_EditorUI::ToolbarSeparator();

		if (ImGui::Button("Categories"))
		{
			ImGui::OpenPopup("CategoryFilterPopup");
		}
		if (ImGui::BeginPopup("CategoryFilterPopup"))
		{
			if (ImGui::Button("All"))  { xState.m_xCategoryFilters.set(); }
			ImGui::SameLine();
			if (ImGui::Button("None")) { xState.m_xCategoryFilters.reset(); }
			ImGui::Separator();
			for (u_int8 i = 0; i < LOG_CATEGORY_COUNT; ++i)
			{
				bool bEnabled = xState.m_xCategoryFilters.test(i);
				if (ImGui::Checkbox(Zenith_LogCategoryNames[i], &bEnabled))
				{
					xState.m_xCategoryFilters.set(i, bEnabled);
				}
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		const float fSearchWidth = ImGui::GetContentRegionAvail().x;
		if (fSearchWidth > Zenith_EditorUI::Px(80.0f))
		{
			Zenith_EditorUI::SearchBox("search", xState.m_szSearch, sizeof(xState.m_szSearch), "Filter messages...", fSearchWidth);
		}
	}

	// A displayed row: the entry plus how many consecutive duplicates it stands for.
	struct DisplayRow
	{
		const ConsoleLogEntry* m_pxEntry;
		u_int m_uRepeat;
	};

	void CollectRows(const Zenith_EditorConsoleState& xState, Zenith_Vector<DisplayRow>& axRows)
	{
		axRows.Clear();
		for (u_int u = 0; u < xState.m_xLogs.GetSize(); ++u)
		{
			const ConsoleLogEntry& xEntry = xState.m_xLogs.Get(u);
			if (!PassesFilters(xEntry, xState))
			{
				continue;
			}
			if (xState.m_bCollapse && axRows.GetSize() > 0)
			{
				DisplayRow& xLast = axRows.GetBack();
				if (xLast.m_pxEntry->m_eLevel == xEntry.m_eLevel && xLast.m_pxEntry->m_strMessage == xEntry.m_strMessage)
				{
					xLast.m_pxEntry = &xEntry;   // show the newest timestamp
					++xLast.m_uRepeat;
					continue;
				}
			}
			axRows.PushBack(DisplayRow{ &xEntry, 1 });
		}
	}

	void RenderRow(const DisplayRow& xRow, int iIndex, int& iSelected)
	{
		const Zenith_EditorPalette& xP = Zenith_EditorUI::Palette();
		const ConsoleLogEntry& xEntry = *xRow.m_pxEntry;
		ImGui::TableNextRow();

		// Level + time
		ImGui::TableNextColumn();
		const float fIcon = ImGui::GetFontSize() * 0.9f;
		const ImVec2 xPos = ImGui::GetCursorScreenPos();
		Zenith_EditorUI::DrawIcon(ImGui::GetWindowDrawList(), LevelIcon(xEntry.m_eLevel), ImVec2(xPos.x + fIcon * 0.5f, xPos.y + ImGui::GetTextLineHeight() * 0.5f), fIcon, LevelColour(xEntry.m_eLevel));
		ImGui::Dummy(ImVec2(fIcon, ImGui::GetTextLineHeight()));
		ImGui::SameLine(0.0f, Zenith_EditorUI::Px(4.0f));
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xP.m_uTextDim), "%s", xEntry.m_strTimestamp.c_str());

		// Category
		ImGui::TableNextColumn();
		ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(xP.m_uTextDim), "%s", Zenith_GetLogCategoryName(xEntry.m_eCategory));

		// Message (selectable across the row)
		ImGui::TableNextColumn();
		ImGui::PushID(iIndex);
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(LevelColour(xEntry.m_eLevel)));
		if (ImGui::Selectable(xEntry.m_strMessage.c_str(), iSelected == iIndex, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
		{
			iSelected = iIndex;
		}
		ImGui::PopStyleColor();
		if (ImGui::BeginPopupContextItem("rowctx"))
		{
			if (ImGui::MenuItem("Copy message"))
			{
				ImGui::SetClipboardText(xEntry.m_strMessage.c_str());
			}
			ImGui::EndPopup();
		}
		if (xRow.m_uRepeat > 1)
		{
			ImGui::SameLine();
			char acRepeat[16];
			snprintf(acRepeat, sizeof(acRepeat), "x%u", xRow.m_uRepeat);
			Zenith_EditorUI::Badge(acRepeat, xP.m_uFrameActive, xP.m_uTextBright);
		}
		ImGui::PopID();
	}

	void RenderLogList(const Zenith_EditorConsoleState& xState)
	{
		static int s_iSelected = -1;
		static Zenith_Vector<DisplayRow> s_axRows;
		CollectRows(xState, s_axRows);

		const ImGuiTableFlags eFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody;
		if (!ImGui::BeginTable("ConsoleRows", 3, eFlags, ImVec2(0, 0)))
		{
			return;
		}
		ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

		ImGuiListClipper xClipper;
		xClipper.Begin(static_cast<int>(s_axRows.GetSize()));
		while (xClipper.Step())
		{
			for (int i = xClipper.DisplayStart; i < xClipper.DisplayEnd; ++i)
			{
				RenderRow(s_axRows.Get(static_cast<u_int>(i)), i, s_iSelected);
			}
		}

		// Ctrl+C copies the selected row.
		if (s_iSelected >= 0 && s_iSelected < static_cast<int>(s_axRows.GetSize())
			&& ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
		{
			ImGui::SetClipboardText(s_axRows.Get(static_cast<u_int>(s_iSelected)).m_pxEntry->m_strMessage.c_str());
		}

		if (xState.m_bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
		{
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::EndTable();
	}
}

bool PassesFilters(const ConsoleLogEntry& xEntry, const Zenith_EditorConsoleState& xState)
{
	if (!LevelEnabled(xEntry.m_eLevel, xState))
		return false;
	if (!xState.m_xCategoryFilters.test(static_cast<size_t>(xEntry.m_eCategory)))
		return false;
	if (xState.m_szSearch[0] != '\0' && !Zenith_EditorUI::ContainsCaseInsensitive(xEntry.m_strMessage.c_str(), xState.m_szSearch))
		return false;
	return true;
}

void Render(Zenith_EditorConsoleState& xState)
{
	ImGui::Begin(szEDITOR_WINDOW_CONSOLE);
	RenderToolbar(xState);
	ImGui::Spacing();
	RenderLogList(xState);
	ImGui::End();
}

} // namespace Zenith_EditorPanelConsole

#endif // ZENITH_TOOLS
