#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Console.h"
#include "Core/Zenith_EditorWindowNames.h"
#include "Editor/Zenith_Editor.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//=============================================================================
// Console Panel Implementation
//
// Displays log messages with filtering by level and category.
// Supports auto-scroll and clear functionality.
//=============================================================================

namespace Zenith_EditorPanelConsole
{

namespace
{
	// Canonical level color used in both the toolbar filter checkboxes and the
	// log-text rendering. Keeps the two in lockstep.
	ImVec4 LevelColor(ConsoleLogEntry::LogLevel eLevel)
	{
		switch (eLevel)
		{
		case ConsoleLogEntry::LogLevel::Info:    return ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
		case ConsoleLogEntry::LogLevel::Warning: return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		case ConsoleLogEntry::LogLevel::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
		}
		return ImVec4(1, 1, 1, 1);
	}

	bool LevelEnabled(ConsoleLogEntry::LogLevel eLevel, bool bShowInfo, bool bShowWarnings, bool bShowErrors)
	{
		switch (eLevel)
		{
		case ConsoleLogEntry::LogLevel::Info:    return bShowInfo;
		case ConsoleLogEntry::LogLevel::Warning: return bShowWarnings;
		case ConsoleLogEntry::LogLevel::Error:   return bShowErrors;
		}
		return false;
	}

	void DrawColoredCheckbox(const char* szLabel, bool& bValue, ImVec4 xColor)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, xColor);
		ImGui::Checkbox(szLabel, &bValue);
		ImGui::PopStyleColor();
		ImGui::SameLine();
	}

	void RenderToolbar(Zenith_Vector<ConsoleLogEntry>& xLogs, bool& bAutoScroll,
		bool& bShowInfo, bool& bShowWarnings, bool& bShowErrors,
		std::bitset<LOG_CATEGORY_COUNT>& xCategoryFilters)
	{
		if (ImGui::Button("Clear"))
		{
			xLogs.Clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &bAutoScroll);
		ImGui::SameLine();
		ImGui::Separator();
		ImGui::SameLine();

		DrawColoredCheckbox("Info",     bShowInfo,     LevelColor(ConsoleLogEntry::LogLevel::Info));
		DrawColoredCheckbox("Warnings", bShowWarnings, LevelColor(ConsoleLogEntry::LogLevel::Warning));
		DrawColoredCheckbox("Errors",   bShowErrors,   LevelColor(ConsoleLogEntry::LogLevel::Error));

		ImGui::Separator();
		ImGui::SameLine();

		if (ImGui::Button("Categories..."))
		{
			ImGui::OpenPopup("CategoryFilterPopup");
		}
		if (ImGui::BeginPopup("CategoryFilterPopup"))
		{
			if (ImGui::Button("All"))  { xCategoryFilters.set(); }
			ImGui::SameLine();
			if (ImGui::Button("None")) { xCategoryFilters.reset(); }
			ImGui::Separator();
			for (u_int8 i = 0; i < LOG_CATEGORY_COUNT; ++i)
			{
				bool bEnabled = xCategoryFilters.test(i);
				if (ImGui::Checkbox(Zenith_LogCategoryNames[i], &bEnabled))
				{
					xCategoryFilters.set(i, bEnabled);
				}
			}
			ImGui::EndPopup();
		}
	}

	void RenderLogList(const Zenith_Vector<ConsoleLogEntry>& xLogs, bool bAutoScroll,
		bool bShowInfo, bool bShowWarnings, bool bShowErrors,
		const std::bitset<LOG_CATEGORY_COUNT>& xCategoryFilters)
	{
		ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		for (const auto& xEntry : xLogs)
		{
			if (!LevelEnabled(xEntry.m_eLevel, bShowInfo, bShowWarnings, bShowErrors))
				continue;
			if (!xCategoryFilters.test(static_cast<size_t>(xEntry.m_eCategory)))
				continue;

			ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(xEntry.m_eLevel));
			ImGui::TextUnformatted(("[" + xEntry.m_strTimestamp + "] " + xEntry.m_strMessage).c_str());
			ImGui::PopStyleColor();
		}

		if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
	}
}

void Render(
	Zenith_Vector<ConsoleLogEntry>& xLogs,
	bool& bAutoScroll,
	bool& bShowInfo,
	bool& bShowWarnings,
	bool& bShowErrors,
	std::bitset<LOG_CATEGORY_COUNT>& xCategoryFilters)
{
	ImGui::Begin(szEDITOR_WINDOW_CONSOLE);
	RenderToolbar(xLogs, bAutoScroll, bShowInfo, bShowWarnings, bShowErrors, xCategoryFilters);
	ImGui::Separator();
	RenderLogList(xLogs, bAutoScroll, bShowInfo, bShowWarnings, bShowErrors, xCategoryFilters);
	ImGui::End();
}

} // namespace Zenith_EditorPanelConsole

#endif // ZENITH_TOOLS
