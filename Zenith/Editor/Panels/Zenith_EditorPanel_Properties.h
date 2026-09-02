#pragma once

#ifdef ZENITH_TOOLS

#include "ZenithECS/Zenith_Entity.h"

struct Zenith_EditorInspectorState;

//=============================================================================
// Properties (Inspector) Panel
//
// Shows the primary selected entity: a header (icon, name, enabled), one
// framed section per component (owned by the panel — the components draw only
// their bodies — with a remove button), and a searchable Add Component popup.
// Every edit made here is undoable through Zenith_EditorInspectorUndoTracker.
//=============================================================================

namespace Zenith_EditorPanelProperties
{
	// pxSelectedEntity may be nullptr (nothing selected).
	void Render(Zenith_EditorInspectorState& xState, Zenith_Entity* pxSelectedEntity, size_t uSelectionCount);

	// PURE: the Add Component filter (case-insensitive substring; empty = all).
	bool MatchesComponentSearch(const char* szDisplayName, const char* szQuery);
}

#endif // ZENITH_TOOLS
