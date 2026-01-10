#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_EditorPanel_Console.h"
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

void Render(
	std::vector<ConsoleLogEntry>& xLogs,
	bool& bAutoScroll,
	bool& bShowInfo,
	bool& bShowWarnings,
	bool& bShowErrors,
	std::bitset<LOG_CATEGORY_COUNT>& xCategoryFilters)
{
	ImGui::Begin("Console");

	// Toolbar
	if (ImGui::Button("Clear"))
	{
		xLogs.clear();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &bAutoScroll);
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	// Filter checkboxes
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
	ImGui::Checkbox("Info", &bShowInfo);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
	ImGui::Checkbox("Warnings", &bShowWarnings);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
	ImGui::Checkbox("Errors", &bShowErrors);
	ImGui::PopStyleColor();
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();

	// Category filter dropdown
	if (ImGui::Button("Categories..."))
	{
		ImGui::OpenPopup("CategoryFilterPopup");
	}
	if (ImGui::BeginPopup("CategoryFilterPopup"))
	{
		if (ImGui::Button("All"))
		{
			xCategoryFilters.set();
		}
		ImGui::SameLine();
		if (ImGui::Button("None"))
		{
			xCategoryFilters.reset();
		}
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

	ImGui::Separator();

	// Log entries
	ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	for (const auto& xEntry : xLogs)
	{
		// Filter by log level
		bool bShow = false;
		ImVec4 xColor;
		switch (xEntry.m_eLevel)
		{
		case ConsoleLogEntry::LogLevel::Info:
			bShow = bShowInfo;
			xColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
			break;
		case ConsoleLogEntry::LogLevel::Warning:
			bShow = bShowWarnings;
			xColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
			break;
		case ConsoleLogEntry::LogLevel::Error:
			bShow = bShowErrors;
			xColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
			break;
		}

		// Also filter by category
		if (bShow && !xCategoryFilters.test(static_cast<size_t>(xEntry.m_eCategory)))
		{
			bShow = false;
		}

		if (bShow)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, xColor);
			ImGui::TextUnformatted(("[" + xEntry.m_strTimestamp + "] " + xEntry.m_strMessage).c_str());
			ImGui::PopStyleColor();
		}
	}

	// Auto-scroll to bottom
	if (bAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::End();
}

} // namespace Zenith_EditorPanelConsole

#endif // ZENITH_TOOLS
