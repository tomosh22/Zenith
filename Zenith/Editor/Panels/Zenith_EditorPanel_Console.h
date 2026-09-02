#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorUI.h"
#include "Collections/Zenith_Vector.h"
#include <bitset>

struct Zenith_EditorConsoleState;

//=============================================================================
// Console Panel
//
// Engine log with level toggles that carry live counts, category filters, a
// text search, a collapse mode for repeated lines, and copy-to-clipboard.
//=============================================================================

namespace Zenith_EditorPanelConsole
{
	void Render(Zenith_EditorConsoleState& xState);

	// PURE: whether an entry passes the level / category / search filters.
	bool PassesFilters(const ConsoleLogEntry& xEntry, const Zenith_EditorConsoleState& xState);

	// The icon and colour a log level renders with (shared with the status bar).
	ImU32 LevelColour(ConsoleLogEntry::LogLevel eLevel);
	Zenith_EditorIcon LevelIcon(ConsoleLogEntry::LogLevel eLevel);
}

#endif // ZENITH_TOOLS
