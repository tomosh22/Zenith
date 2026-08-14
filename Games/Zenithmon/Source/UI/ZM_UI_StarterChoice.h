#pragma once

#include "Zenithmon/Source/Party/ZM_StarterChoice.h"   // ZM_STARTER_CHOICE + the SHIPPED table accessors

class Zenith_Entity;

// ============================================================================
// ZM_UI_StarterChoice (S8) -- the starter-choice SCREEN: a header and ONE CELL
// PER STARTER, in table order. The player picks their first partner here and
// nowhere else.
//
// ZM-D-188 (binding): this is the REAL three-way picker, not a chain of the
// shipped two-label yes/no prompt, and it is VERTICAL. Vertical is not a taste
// call -- it rides the engine's existing focus navigation (MENU_UP / MENU_DOWN +
// the d-pad + the pointer table) and therefore needs ZERO new input actions. A
// horizontal picker would need LEFT/RIGHT actions and would move ZM_ACTION_COUNT
// 8 -> 10 against boot units that static_assert their walk on it.
//
// MODELLED CELL-FOR-CELL ON ZM_UI_SaveSlots, the closest shipped analogue: a
// fixed-N row picker whose instance state is PODs only (one selection mirror), so
// the owning component's defaulted noexcept move stays well-formed when the ECS
// pool relocates it. The whole DECISION surface -- the element-name <-> cell <->
// choice maps and the label -- is PURE, TOTAL and unit-tested with no scene and no
// graphics; Present / Hide are the only impure reaches and they re-resolve every
// element BY NAME every frame (never cache a pointer).
//
// ★ IT GRANTS NOTHING. The one grant path in this game is the SHIPPED
// ZM_ApplyStarterChoice (ZM_StarterChoice.h); this class resolves a focused
// element NAME to a ZM_STARTER_CHOICE and hands it over. Re-implementing the
// party append / dex mark here would be a second starter grant with its own
// full-party and validation behaviour, which is exactly what SC3 collapsed.
//
// ★ CANCEL IS IGNORED (CancelIsIgnored below, the contract spelled as code, the
// ZM_UI_SaveSlots::RowIsAlwaysShown idiom). The starter arm must NOT pop on cancel
// -- it behaves like the DIALOGUE arm. A picker that popped on Escape would leave a
// PARTYLESS save, and ZM_CanEnterBattle (which keys on emptiness alone) then
// refuses every battle for the rest of that file. There is deliberately NO Back
// element on this screen for the same reason.
// ============================================================================

class ZM_UI_StarterChoice
{
public:
	// DERIVED from the shipped starter table, never re-spelled: one cell per starter,
	// so a fourth starter never leaves a cell unrendered
	// (StarterScreen_CellCountEqualsStarterChoiceCount is the tripwire).
	static constexpr u_int uCELL_COUNT = static_cast<u_int>(ZM_STARTER_CHOICE_COUNT);   // 3
	// The caller-owned buffer size Present hands FormatCellLabel; comfortably larger
	// than any species name in the dex table.
	static constexpr u_int uLABEL_CAPACITY = 32u;

	// The authored element names -- the SINGLE SOURCE of the
	// ZM_ConfigureMenuRootStarterScreen contract in Zenithmon.cpp (create/place these),
	// of this class's runtime re-resolution, AND of the committed-bytes needles in
	// Tests/ZM_Tests_CommittedSceneBytes.cpp. There is NO Back / Cancel element: see
	// CancelIsIgnored.
	static constexpr const char* szPANEL_NAME  = "Menu_StarterPanel";
	static constexpr const char* szHEADER_NAME = "Menu_StarterHeader";
	// Written at RUNTIME by Present. Generic wording only (no franchise terms --
	// Scope.md's zero-Nintendo-IP guard on UI copy).
	static constexpr const char* szHEADER_TEXT = "Choose your first partner.";

	// Authored geometry, kept HERE (the ZM_UI_Shop f*_ idiom) so the authoring site and
	// the presenter can never drift apart. The cells form ONE VERTICAL COLUMN at x == 0.
	static constexpr float fPANEL_WIDTH        = 420.0f;
	static constexpr float fPANEL_HEIGHT       = 260.0f;   // spans y [-130, +130]
	static constexpr float fHEADER_CENTRE_Y    = -104.0f;
	static constexpr float fHEADER_WIDTH       = 380.0f;
	static constexpr float fHEADER_HEIGHT      = 32.0f;
	static constexpr float fCELL_FIRST_CENTRE_Y = -42.0f;
	static constexpr float fCELL_PITCH_Y       = 52.0f;    // three cells span [-64, +84]
	static constexpr float fCELL_WIDTH         = 380.0f;
	static constexpr float fCELL_HEIGHT        = 44.0f;

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The element name for a cell ("Menu_StarterCell0".."Menu_StarterCell2"); "" for an
	// out-of-range cell. Returns string LITERALS (the RowElementName idiom) -- never
	// allocates, never dangles, so bake-time authoring may call it.
	static const char* CellElementName(u_int uCell);
	// The cell index for an element name, or -1 when it is not a cell. EXACT compare
	// (never a prefix match), null-safe.
	static int         CellIndexFromElementName(const char* szName);
	// The starter a cell shows. Cells are authored in TABLE ORDER, so cell N is choice N;
	// TOTAL -- an out-of-range cell yields ZM_STARTER_CHOICE_NONE.
	static ZM_STARTER_CHOICE CellChoice(u_int uCell);
	// The starter a confirmed element NAME means. The panel, the header, a foreign name
	// and null all yield ZM_STARTER_CHOICE_NONE -- which ZM_IsRegisteredStarterChoice
	// rejects, so a stray confirm can never reach the grant.
	static ZM_STARTER_CHOICE ChoiceFromElementName(const char* szName);

	// PURE and TOTAL. Writes a null-terminated label into a caller-owned buffer; no
	// allocation, never exceeds uCapacity. The label is the SPECIES NAME, resolved from
	// the shipped table through ZM_ResolvePlayerStarterSpecies.
	//
	// ★ AN UNREGISTERED CHOICE YIELDS AN EMPTY LABEL AND NEVER REACHES A SPECIES TABLE.
	// ZM_GetSpeciesName itself is total (it answers the literal "NONE"), so without the
	// guard the cell would read "NONE" to the player; its neighbours are bounds-ASSERTED,
	// and Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration, so anything
	// reaching further off that path would end the whole boot-unit run rather than fail
	// one assertion. The full reasoning is at the implementation. Fail closed here.
	static void FormatCellLabel(ZM_STARTER_CHOICE eChoice, char* pszOut, u_int uCapacity);

	// The ZM_UI_SaveSlots::RowIsAlwaysShown idiom -- a contract spelled as code so a unit
	// can pin it. Every cell stays VISIBLE and FOCUSABLE (there is nothing to disarm: all
	// three are always confirmable), so the authored nav links never point at a hidden
	// target, which the engine drops with NO spatial fallback.
	static bool CellIsAlwaysShown() { return true; }
	// ZM-D-188 CONTRACT as code: the starter arm ignores CANCEL and never pops on it.
	// See the header comment -- popping would leave a partyless save.
	static bool CancelIsIgnored() { return true; }

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// Back to the opening state: no selection. There is nothing else to clear -- unlike
	// the save screen this presenter probes no disk and carries no mode.
	void Reset();
	// PURE selection mirror. An out-of-range cell (including the -1 a failed name lookup
	// yields) clears the selection. Present drives this; it is public so the mirror policy
	// is unit-testable with no canvas.
	void SelectCell(int iCell);
	// The focused CELL mirror: -1 while the screen is not presented, and whenever the
	// canvas focus could not be settled onto a cell.
	int  GetSelectedCell() const { return m_iSelectedCell; }

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the panel, header and every cell, and ensure the canvas focus sits on
	// a cell (or mirror the engine-navigated focus). A missing UI component or element is
	// skipped silently -- presentation never crashes the menu.
	void Present(Zenith_Entity& xRootEntity);
	// Hide the panel, header and every cell, and clear the selection to -1.
	void Hide(Zenith_Entity& xRootEntity);

private:
	int m_iSelectedCell = -1;
};
