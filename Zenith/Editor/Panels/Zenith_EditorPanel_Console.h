#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include <vector>
#include <bitset>

//=============================================================================
// Console Panel
//
// Displays log messages with filtering capabilities.
//=============================================================================

namespace Zenith_EditorPanelConsole
{
	/**
	 * Render the console panel
	 *
	 * @param xLogs Reference to the log entries vector
	 * @param bAutoScroll Reference to auto-scroll toggle
	 * @param bShowInfo Reference to show info messages toggle
	 * @param bShowWarnings Reference to show warnings toggle
	 * @param bShowErrors Reference to show errors toggle
	 * @param xCategoryFilters Reference to category filter bitset
	 */
	void Render(
		std::vector<ConsoleLogEntry>& xLogs,
		bool& bAutoScroll,
		bool& bShowInfo,
		bool& bShowWarnings,
		bool& bShowErrors,
		std::bitset<LOG_CATEGORY_COUNT>& xCategoryFilters);
}

#endif // ZENITH_TOOLS
