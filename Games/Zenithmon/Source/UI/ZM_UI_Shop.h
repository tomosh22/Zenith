#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"     // ZM_ITEM_ID (by value in the pure formatters)
#include "Zenithmon/Source/Shop/ZM_ShopLogic.h"    // ZM_SHOP_RESULT (Confirm's return + the report line)

#include <string>

class Zenith_Entity;
class Zenith_UIComponent;
struct ZM_Bag;

// ============================================================================
// ZM_UI_Shop (S6 item 2 SC7) -- the mart screen: a BUY list over the shop's
// configured stock and a SELL list over everything the player is carrying, plus a
// quantity stepper and a Confirm control that runs the transaction through
// ZM_ShopLogic against the LIVE ZM_GameState.
//
// Like ZM_UI_Party / ZM_UI_Dex / ZM_UI_Bag it is a small NON-ECS presentation class
// owned BY VALUE by ZM_UI_MenuStack: no order, no component registration, no editor
// mirror. Its instance state is PODs only (a mode, a page, a quantity, a cursor, a
// last result and a fixed id array), so the owning component's defaulted noexcept
// move stays well-formed when the ECS pool relocates it.
//
// It is a 1-D paged LIST, authored WHOLE at bake time by ZM_ConfigureMenuRoot -- the
// SC6 bag shape, deliberately not the SC5 dex's runtime-built grid. NOTHING it draws
// carries explicit navigation links: Zenith_UICanvas::NavigateDown consults the
// explicit link FIRST and only falls back to the spatial search when it is null, so a
// bake-time link into a row this class hides on a partial page would swallow the
// press outright. The Confirm control sits alone on the row column's x, directly
// below the list, so ONE Down off the last live row reaches it on the spatial search
// no matter how short the page is.
//
// Cursor contract, and the ONE place this screen differs from the bag: m_iCursor is
// the SELECTED ROW on the CURRENT PAGE (0..uROWS_PER_PAGE-1) -- NOT the flat list
// index, which it only coincides with on page 0; combine it with GetPage, or just call
// GetSelectedEntryIndex, to reach the entry itself. Unlike the bag's row mirror it
// SURVIVES the focus moving onto a control -- the player walks off the list onto
// Confirm, and the entry they picked must still be the one bought. It is -1 only while
// the list is empty or the screen is not presented.
// ============================================================================

// Which list the screen is showing. Not serialized (the shop is session state), so
// this is a plain UI mode rather than a save-stable enum.
enum ZM_SHOP_MODE : u_int
{
	ZM_SHOP_MODE_BUY = 0u,
	ZM_SHOP_MODE_SELL,

	ZM_SHOP_MODE_COUNT
};

class ZM_UI_Shop
{
public:
	// The most item ids one shop may stock, and the list rows on screen at once.
	static constexpr u_int uMAX_INVENTORY = 16u;
	static constexpr u_int uROWS_PER_PAGE = 6u;

	// Quantity bounds. 1 because a zero-quantity transaction is meaningless (and
	// ZM_ShopLogic rejects it), 99 because a stack caps at uZM_BAG_MAX_STACK_COUNT and
	// a two-digit stepper is what the row band has space for.
	static constexpr int iMIN_QUANTITY = 1;
	static constexpr int iMAX_QUANTITY = 99;

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution (never cache element pointers -- the canvas may relocate them).
	static constexpr const char* szPANEL_NAME     = "Menu_ShopPanel";
	static constexpr const char* szHEADER_NAME    = "Menu_ShopHeader";   // mode + money + qty + the last report
	static constexpr const char* szBUY_TAB_NAME   = "Menu_ShopBuyTab";
	static constexpr const char* szSELL_TAB_NAME  = "Menu_ShopSellTab";
	static constexpr const char* szPREV_PAGE_NAME = "Menu_ShopPrevPage";
	static constexpr const char* szNEXT_PAGE_NAME = "Menu_ShopNextPage";
	static constexpr const char* szQTY_DOWN_NAME  = "Menu_ShopQtyDown";
	static constexpr const char* szQTY_UP_NAME    = "Menu_ShopQtyUp";
	static constexpr const char* szCONFIRM_NAME   = "Menu_ShopConfirm";
	static constexpr const char* szEXIT_NAME      = "Menu_ShopExit";

	// The eight controls, in the order ZM_ConfigureMenuRoot places them (band 1 then
	// band 2, left to right). Present shows them, Hide hides them, and the focus policy
	// tests membership against this same list.
	static constexpr u_int uCONTROL_COUNT = 8u;
	// The control element name at uIndex, or "" past the end (string LITERALS, so
	// bake-time authoring may call this).
	static const char* ControlElementName(u_int uIndex);
	// True when szElementName is one of this screen's eight controls.
	static bool IsControlElementName(const char* szElementName);
	// True only for the Exit control. The menu stack tests this BEFORE dispatching a
	// confirm, because leaving is the STACK's business, not the presenter's.
	static bool IsExitElementName(const char* szElementName);

	// ---- Geometry, shared with the ONE placement site (ZM_ConfigureMenuRoot) ----
	// All Y values are offsets from the panel's centre (origin top-left, +Y DOWN), the
	// panel itself being screen-centred. The header band spans [-232,-180], the row
	// stack [-170,+90] and the two control bands [+110,+146] / [+172,+208] -- every one
	// of them inside the 480-tall panel's [-240,+240], so nothing the screen draws
	// bleeds outside the box it is drawn over (the S5 visual-gate lesson, ZM-D-112).
	static constexpr float fPANEL_WIDTH  = 880.0f;
	static constexpr float fPANEL_HEIGHT = 480.0f;

	static constexpr float fHEADER_CENTRE_Y = -206.0f;
	static constexpr float fHEADER_WIDTH    = 800.0f;
	// 52 tall (not 40): the header carries the mode, the money, the quantity AND the
	// last transaction report, which wraps to a second line on a long refusal.
	static constexpr float fHEADER_HEIGHT   = 52.0f;

	static constexpr float fROW_WIDTH          = 760.0f;
	static constexpr float fROW_HEIGHT         = 40.0f;
	static constexpr float fROW_FIRST_CENTRE_Y = -150.0f;
	static constexpr float fROW_PITCH_Y        = 44.0f;

	// Two control bands below the list. CONFIRM sits ALONE at x == 0 in the first band,
	// directly under the row column: Zenith_UICanvas scores spatial candidates on raw
	// squared distance, so from any live row (they all share x == 0) the nearest element
	// below is always Confirm -- one Down press reaches the primary action even when the
	// page holds a single row.
	static constexpr float fCONTROL_WIDTH        = 150.0f;
	static constexpr float fCONTROL_HEIGHT       = 36.0f;
	static constexpr float fCONTROL_BAND1_Y      = 128.0f;
	static constexpr float fCONTROL_BAND2_Y      = 190.0f;
	static constexpr float fCONTROL_INNER_X      = 180.0f;
	static constexpr float fCONTROL_OUTER_X      = 340.0f;

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The element name for a list row ("Menu_ShopRow0".."Menu_ShopRow5"); "" for an
	// out-of-range row. Returns string LITERALS -- never allocates, never dangles.
	static const char* RowElementName(u_int uRow);
	// The row index for an element name, or -1 when it is not a shop row.
	static int RowIndexFromElementName(const char* szElementName);
	// Pages needed for uEntryCount entries. ALWAYS >= 1: an empty list still shows one
	// blank page rather than leaving ClampPage nothing to clamp to.
	static u_int PageCount(u_int uEntryCount);
	// Clamp a (possibly negative or past-the-end) page into [0, PageCount - 1].
	static int ClampPage(int iPage, u_int uEntryCount);
	// The flat list index shown in row uRow of page uPage, or -1 when that row is past
	// the end of the list (or out of range).
	static int StackIndexForRow(u_int uPage, u_int uRow, u_int uEntryCount);
	// How many rows on uPage map to a real entry: uROWS_PER_PAGE except on the trailing
	// partial page (and 0 for an empty list / a page past the end).
	static u_int VisibleRowCount(u_int uPage, u_int uEntryCount);
	// Clamp a quantity into [iMIN_QUANTITY, iMAX_QUANTITY].
	static int ClampQuantity(int iQuantity);

	// "BUY" / "SELL" (the word the header carries); "" for an out-of-range mode.
	static const char* ModeToString(ZM_SHOP_MODE eMode);
	// One BUY row: the item name and what it costs. The name and the price both come
	// from the item table (ZM_GetItemName / ZM_ShopBuyPrice), never a literal.
	static std::string FormatBuyRow(ZM_ITEM_ID eItem);
	// One SELL row: the item name, how many the player holds, and what it fetches.
	static std::string FormatSellRow(ZM_ITEM_ID eItem, u_int uHeld);
	// The header's standing part: the mode word, the money balance and the quantity.
	static std::string FormatHeader(ZM_SHOP_MODE eMode, u_int uMoney, u_int uQuantity);
	// The player-facing report for a transaction result. TOTAL: every ZM_SHOP_RESULT
	// (and any stray value) maps to a distinct NON-EMPTY line -- a silent refusal would
	// read as the screen simply ignoring the player.
	static const char* FormatResult(ZM_SHOP_RESULT eResult);
	// The FULL line written to the header element: FormatHeader, plus the FormatResult
	// report once a transaction has actually been attempted. There is no separate
	// report widget -- the report rides in the header (a dedicated element would have to
	// be authored, shown and hidden for one line of text).
	static std::string FormatHeaderLine(ZM_SHOP_MODE eMode, u_int uMoney, u_int uQuantity,
		bool bHasResult, ZM_SHOP_RESULT eResult);
	// The notice shown when the current list has nothing in it at all.
	static std::string FormatEmptyList();

	// The stack listed at flat SELL index uIndex -- every pocket in category order, then
	// stack order within the pocket (ZM_Bag keeps each pocket sorted, so this ordering is
	// deterministic). False when the index is past the end. Pure over the bag.
	static bool SellEntryAt(const ZM_Bag& xBag, u_int uIndex, ZM_ITEM_ID& eItemOut, u_int& uHeldOut);

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// BUY mode, page 0, quantity 1, row 0, no report -- AND an empty stock list. Reset
	// drops the configured inventory too: a shop is a per-visit session, so leaving one
	// and never entering another must not leave the last mart's stock resolvable.
	void Reset();

	// Configure the stock this shop sells. ALL-OR-NOTHING: a null pointer, a zero or
	// over-capacity count, or ANY out-of-range id is rejected whole and leaves the
	// previous inventory intact. On success the session state (mode / page / quantity /
	// cursor / report) is reset too -- this IS entering a shop.
	//
	// It deliberately does NOT reject an item priced 0: refusing unbuyable stock here
	// would make ZM_ShopBuy's NOT_PURCHASABLE guard unreachable, and that guard is the
	// one standing between a mis-authored mart and a free Badge Case.
	bool SetInventory(const ZM_ITEM_ID* paeInventory, u_int uCount);

	// Dispatch a confirm BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick with a
	// component pointer, which dangles when the ECS pool relocates the host. The tabs
	// switch mode (resetting page AND quantity), the page buttons page, the quantity
	// buttons step, and CONFIRM runs the transaction on the selected row through
	// ZM_ShopLogic. A row (or any other name) only selects -- confirming one is inert,
	// so a stray press can never spend money.
	//
	// The bag and the money are taken BY REFERENCE and are the LIVE ZM_GameState's, so
	// a purchase actually persists. Returns the transaction result (ZM_SHOP_OK for a
	// press that was not a transaction at all -- GetLastResult reports only real ones).
	ZM_SHOP_RESULT Confirm(const char* szFocusedElementName, ZM_Bag& xBag, u_int& uMoneyInOut);

	ZM_SHOP_MODE GetMode() const;
	int          GetPage() const { return m_iPage; }
	u_int        GetQuantity() const { return (u_int)ClampQuantity(m_iQuantity); }
	// The SELECTED ROW on the CURRENT page, 0..uROWS_PER_PAGE-1 (see the cursor contract
	// in the header comment) -- NOT the flat list index, which it only coincides with on
	// page 0. -1 only while the list is empty or the screen is not presented.
	int          GetCursor() const { return m_iCursor; }
	// The FLAT index into the CURRENT mode's list that the selection points at, or -1
	// when nothing is selected. This -- not GetCursor -- is what a caller wanting the
	// picked ITEM needs, and it is the exact index Confirm transacts on. It needs the
	// live bag because the SELL list's length is the bag's.
	int          GetSelectedEntryIndex(const ZM_Bag& xBag) const;
	u_int        GetInventoryCount() const { return m_uInventoryCount; }
	// The stocked id at uIndex, or ZM_ITEM_NONE past the end.
	ZM_ITEM_ID   GetInventoryItem(u_int uIndex) const;
	// The result of the last TRANSACTION attempt (ZM_SHOP_OK before the first one --
	// HasResult() says which).
	ZM_SHOP_RESULT GetLastResult() const { return (ZM_SHOP_RESULT)m_uLastResult; }
	bool           HasResult() const { return m_bHasResult; }

	// How many entries the CURRENT mode lists: the stocked ids in BUY, every stack the
	// player is carrying in SELL.
	u_int EntryCount(const ZM_Bag& xBag) const;
	// The item + held count listed at flat index uIndex in the CURRENT mode. False when
	// the index is past the end of the list.
	bool EntryAt(const ZM_Bag& xBag, u_int uIndex, ZM_ITEM_ID& eItemOut, u_int& uHeldOut) const;

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the screen from the live bag + money: clamps the page and the
	// quantity, fills the current page's rows, refreshes the header, and settles the
	// selection / focus. A missing UI component or element is skipped silently --
	// presentation never crashes the menu, and it never mutates the bag or the money.
	void Present(Zenith_Entity& xRootEntity, const ZM_Bag& xBag, u_int uMoney);
	// Hide the panel, the header, every row and all eight controls, and clear the
	// cursor to -1. Deliberately does NOT drop the mode / page / quantity / inventory --
	// that is Reset's job, which the menu stack owns.
	void Hide(Zenith_Entity& xRootEntity);

private:
	// The focus / selection settle, split out of Present so the (long) element pass
	// stays readable. Returns the cursor to mirror.
	int SettleFocus(Zenith_UIComponent& xUI, u_int uPage, u_int uEntryCount);

	// PODs only (the ECS pool move-constructs the owning component).
	ZM_ITEM_ID m_aeInventory[uMAX_INVENTORY] = {};
	u_int      m_uInventoryCount = 0u;
	int        m_iMode        = (int)ZM_SHOP_MODE_BUY;   // a ZM_SHOP_MODE held as an int (POD-simple)
	int        m_iPage        = 0;
	int        m_iQuantity    = iMIN_QUANTITY;
	int        m_iCursor      = 0;
	u_int      m_uLastResult  = (u_int)ZM_SHOP_OK;
	bool       m_bHasResult   = false;
};
