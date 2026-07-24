#pragma once

#include "Zenithmon/Source/Save/ZM_SaveSlots.h"   // ZM_SAVE_SLOT_STATUS snapshots (by value only)

class Zenith_Entity;

// ============================================================================
// ZM_UI_TitleMenu (S7 item 2 SC5) -- the FrontEnd Continue / New Game model.
//
// The presenter is owned BY VALUE by ZM_UI_MenuStack. Its instance state is two
// bools derived from the latest four-slot snapshot, so it is trivially/noexcept
// movable when the ECS pool relocates its owning component. It owns no entity or
// UI pointers: Present / Hide re-resolve every authored element BY NAME.
//
// Continue availability deliberately uses OCCUPANCY, not readiness. DAMAGED is
// occupied and therefore keeps Continue visible so the LOAD screen can surface
// the damaged row; only a READY row may subsequently load. When every slot is
// EMPTY, Continue is hidden, unfocusable, absent from live navigation and inert.
// ============================================================================

// Authored visual/focus order, top to bottom.
enum ZM_TITLE_ITEM : u_int
{
	ZM_TITLE_ITEM_CONTINUE = 0u,
	ZM_TITLE_ITEM_NEW_GAME,

	ZM_TITLE_ITEM_COUNT
};

// What confirming a title control asks the owning menu stack to do.
enum ZM_TITLE_ACTION : u_int
{
	ZM_TITLE_ACTION_NONE = 0u,
	ZM_TITLE_ACTION_OPEN_LOAD,
	ZM_TITLE_ACTION_NEW_GAME,

	ZM_TITLE_ACTION_COUNT
};

class ZM_UI_TitleMenu
{
public:
	// The single source of truth shared by FrontEnd authoring and runtime lookup.
	static constexpr const char* szPANEL_NAME    = "Menu_TitlePanel";
	static constexpr const char* szCONTINUE_NAME = "Menu_TitleContinue";
	static constexpr const char* szNEW_GAME_NAME = "Menu_TitleNewGame";

	// ---- PURE name/action policy (boot-unit tested) --------------------------

	// The authored element name for a title item; "" for an out-of-range item.
	static const char* ItemElementName(ZM_TITLE_ITEM eItem);
	// Exact, null-safe reverse lookup; -1 for panel/foreign/out-of-range names.
	static int ItemIndexFromElementName(const char* szElementName);
	// Raw name -> action map. Availability is applied by ResolveConfirm below.
	static ZM_TITLE_ACTION ResolveAction(const char* szFocusedElementName);

	// ---- Instance model ------------------------------------------------------

	// Replace the entire availability snapshot. A null pointer, zero count or any
	// count other than exactly ZM_SAVE_SLOT_COUNT fails closed to all-empty. No
	// state is retained from the previous open.
	void Open(const ZM_SAVE_SLOT_STATUS* paeStatuses, u_int uCount);

	bool HasOccupiedSlot() const { return m_bHasOccupiedSlot; }
	bool HasReadySlot() const { return m_bHasReadySlot; }
	// New Game is always live. Continue is live iff ANY slot is non-EMPTY,
	// including a damaged-only snapshot.
	bool IsItemVisible(ZM_TITLE_ITEM eItem) const;
	bool IsItemFocusable(ZM_TITLE_ITEM eItem) const;
	ZM_TITLE_ITEM GetDefaultFocusItem() const;
	// Availability-aware confirm. A stale hidden Continue name resolves to NONE.
	ZM_TITLE_ACTION ResolveConfirm(const char* szFocusedElementName) const;

	// ---- Best-effort presentation -------------------------------------------

	// Show the panel + currently-live controls, repair focus onto the default when
	// it points outside this screen, and rebuild explicit vertical navigation so no
	// link ever targets hidden Continue. Re-resolves every pointer by name.
	void Present(Zenith_Entity& xRootEntity);
	// Hide/disarm the whole title surface and clear title-owned focus/navigation.
	void Hide(Zenith_Entity& xRootEntity);

private:
	bool m_bHasOccupiedSlot = false;
	bool m_bHasReadySlot = false;
};
