#pragma once

#include "Zenithmon/Source/Save/ZM_SaveSlots.h"   // ZM_SAVE_SLOT / ZM_SAVE_SLOT_STATUS (by value in the pure resolvers)

class Zenith_Entity;

// ============================================================================
// ZM_UI_SaveSlots (S7 item 2 SC4) -- the SAVE / LOAD slot screen: a header, four
// always-visible slot rows (Save0-2 + Auto) and a Back button. ONE presenter
// serves BOTH modes -- pick a slot to WRITE from the pause menu, or (SC5) pick a
// slot to READ from the title screen -- the ZM-D-122 "six-screens-one-order"
// precedent applied a SEVENTH time: it consumes NO new ECS order and is owned BY
// VALUE by ZM_UI_MenuStack (order 112), exactly like ZM_UI_Bag / ZM_UI_Shop.
//
// The instance state is PODs only (a mode + four cached statuses + a selection),
// so the owning component's defaulted noexcept move stays well-formed when the
// ECS pool relocates it. The whole DECISION surface -- which action confirming a
// row means, the row label, the name<->index maps -- is PURE and TOTAL and unit-
// tested with no scene / graphics; Present / Hide are the only impure reaches and
// they re-resolve every element BY NAME every frame (never cache a pointer).
//
// ZM-D-119 CONTRACT (spelled as code below, RowIsAlwaysShown): every slot row
// stays VISIBLE and FOCUSABLE regardless of its status, so the authored
// SetNavigation links never point at a hidden target (which the engine drops with
// NO spatial fallback, silently swallowing the press). A non-confirmable row is
// disarmed by ResolveRowAction returning NONE -- NEVER by SetFocusable(false).
// ============================================================================

// Which side of the slot screen is showing.
enum ZM_SAVE_SCREEN_MODE : u_int
{
	ZM_SAVE_SCREEN_MODE_SAVE = 0u,   // pick a slot to WRITE (pause menu)
	ZM_SAVE_SCREEN_MODE_LOAD,        // pick a slot to READ (title screen / Continue, SC5)

	ZM_SAVE_SCREEN_MODE_COUNT
};

// What confirming a row MEANS. ONE total resolver rather than two predicates plus
// caller-side branching, so the whole policy is unit-pinnable in one place.
enum ZM_SAVE_ROW_ACTION : u_int
{
	ZM_SAVE_ROW_ACTION_NONE = 0u,      // not confirmable in this mode
	ZM_SAVE_ROW_ACTION_WRITE,          // SAVE + EMPTY manual slot -> write straight away
	ZM_SAVE_ROW_ACTION_CONFIRM_WRITE,  // SAVE + READY/DAMAGED manual slot -> yes/no FIRST
	ZM_SAVE_ROW_ACTION_CONFIRM_LOAD,   // LOAD + READY -> yes/no first (discards live progress)

	ZM_SAVE_ROW_ACTION_COUNT
};

class ZM_UI_SaveSlots
{
public:
	// DERIVED from ZM_SAVE_SLOT_COUNT, never re-spelled: one row per slot, so a fifth
	// slot never leaves a row unrendered (SaveScreen_RowCountEqualsSlotCount).
	static constexpr u_int uROW_COUNT = static_cast<u_int>(ZM_SAVE_SLOT_COUNT);   // 4
	// The caller-owned buffer size Present hands FormatRowLabel; comfortably larger than
	// any "<Slot> -- <Status>" label.
	static constexpr u_int uLABEL_CAPACITY = 64u;

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution.
	static constexpr const char* szPANEL_NAME  = "Menu_SavePanel";
	static constexpr const char* szHEADER_NAME = "Menu_SaveHeader";
	static constexpr const char* szCANCEL_NAME = "Menu_SaveCancel";

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The element name for a slot row ("Menu_SaveRow0".."Menu_SaveRow3"); "" for an
	// out-of-range row. Returns string LITERALS (the RowElementName idiom) -- never
	// allocates, never dangles, so bake-time authoring may call it.
	static const char* RowElementName(u_int uRow);
	// The row index for an element name, or -1 when it is not a slot row. EXACT compare
	// (never a prefix match), null-safe.
	static int         RowIndexFromElementName(const char* szName);
	// True iff szName is the Back element (EXACT compare, null-safe).
	static bool        IsCancelElementName(const char* szName);

	// PURE and TOTAL. The manual flow may NEVER write Auto (SaveFormat.md:42-45); a
	// DAMAGED slot is never loaded and never silently reset (SaveFormat.md:318-321) --
	// overwriting one is possible but ALWAYS goes through the confirm prompt. An
	// out-of-range mode / slot / status is never confirmable (returns NONE).
	static ZM_SAVE_ROW_ACTION ResolveRowAction(ZM_SAVE_SCREEN_MODE eMode,
		ZM_SAVE_SLOT eSlot, ZM_SAVE_SLOT_STATUS eStatus);

	// PURE and TOTAL. Writes a null-terminated label into a caller-owned buffer; no
	// allocation, never exceeds uCapacity. "Slot 1 -- Empty" / "Slot 1 -- Ready" /
	// "Slot 1 -- Damaged" / "Auto -- ...". The slot half is ZM_SaveSlots::SlotDisplayName,
	// never a hand-typed string (the Scope.md:65-66 zero-Nintendo-IP guard on UI copy).
	static void FormatRowLabel(ZM_SAVE_SLOT eSlot, ZM_SAVE_SLOT_STATUS eStatus,
		char* pszOut, u_int uCapacity);

	// ZM-D-119 CONTRACT as code: every row stays VISIBLE and FOCUSABLE regardless of
	// status. See the header comment.
	static bool RowIsAlwaysShown() { return true; }

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// SAVE mode, all statuses EMPTY, no selection.
	void Reset();
	// Set the mode (out-of-range folds to SAVE) and RE-PROBE all four slots (uncached,
	// four ~830-byte disk reads -- see ZM_SaveSlots::ProbeSlot). Clears the selection.
	void Open(ZM_SAVE_SCREEN_MODE eMode);

	ZM_SAVE_SCREEN_MODE GetMode() const { return m_eMode; }
	// The cached status of a row (EMPTY for an out-of-range row).
	ZM_SAVE_SLOT_STATUS GetRowStatus(u_int uRow) const;
	// The focused ROW mirror: -1 while the Back button holds the focus, and while the
	// screen is not presented.
	int GetSelectedRow() const { return m_iSelectedRow; }

	// Resolve a confirmed element NAME to (action, slot). A slot row yields its action +
	// its slot; Back, a foreign name and null all yield NONE + ZM_SAVE_SLOT_NONE.
	ZM_SAVE_ROW_ACTION ResolveConfirm(const char* szFocusedElementName,
		ZM_SAVE_SLOT& eSlotOut) const;

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the panel, header, four rows and Back from the cached statuses, and
	// ensure the canvas focus sits on a row (or mirror the engine-navigated focus). A
	// missing UI component or element is skipped silently -- presentation never crashes
	// the menu. Deliberately does NOT re-probe: Open owns the disk read.
	void Present(Zenith_Entity& xRootEntity);
	// Hide the panel, header, every row and Back, and clear the selection to -1.
	void Hide(Zenith_Entity& xRootEntity);

private:
	ZM_SAVE_SCREEN_MODE m_eMode = ZM_SAVE_SCREEN_MODE_SAVE;
	ZM_SAVE_SLOT_STATUS m_aeStatus[uROW_COUNT] = {};   // value-init == all ZM_SAVE_SLOT_EMPTY (0)
	int                 m_iSelectedRow = -1;
};
