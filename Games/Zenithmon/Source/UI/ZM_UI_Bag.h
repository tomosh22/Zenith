#pragma once

#include "Zenithmon/Source/Data/ZM_ItemData.h"   // ZM_ITEM_ID / ZM_ITEM_CATEGORY (by value in the pure formatters)

#include <string>

class Zenith_Entity;
struct ZM_Bag;

// ============================================================================
// ZM_UI_Bag (S6 item 2 SC6) -- the overworld bag screen: a pocket-tabbed, paged
// list of item stacks with a header carrying the pocket name and the money
// balance. The FIRST screen that renders the SC3 model (ZM_Bag +
// ZM_GameState::m_uMoney).
//
// It is a small NON-ECS presentation class OWNED BY VALUE by ZM_UI_MenuStack (the
// ZM_UI_DialogueBox / ZM_UI_Party / ZM_UI_Dex seam): NO order, NO component
// registration, NO editor mirror. The instance state is PODs only (a pocket + a
// page + a cursor), so the owning component's defaulted noexcept move stays
// well-formed when the ECS pool relocates it.
//
// EVERY widget -- panel, header, the eight rows and the four nav buttons -- is
// authored at BAKE TIME by ZM_ConfigureMenuRoot. This is deliberately NOT the SC5
// dex shape: the dex needed a runtime-built Zenith_UIGridLayoutGroup because a 2-D
// grid has no authoring step, but the bag is a 1-D LIST, so it authors its whole
// widget pool like ZM_UI_Party does. Do not "fix" this into a grid -- doing so
// would buy nothing and re-introduce the SC5 runtime-construction ownership hazard
// (AddElement vs AddChild vs ReparentElement).
//
// There are NINE pockets (ZM_ITEM_CATEGORY_COUNT), too many for a tab row, so the
// pocket is changed with prev/next buttons and named in the header. Both axes are
// navigable: the POCKET (which pocket) and the PAGE (which slice of that pocket's
// stacks).
//
// Traversal is the ENGINE focus-navigation, entirely on the SPATIAL search: NOTHING
// on this screen carries explicit navigation links. That is forced by the engine,
// not a style choice -- Zenith_UICanvas::NavigateDown consults GetNavDown() first
// and only falls back to the spatial search when it is NULL, so a link pointing at a
// row this class has hidden (every partial page) would swallow the press outright.
// The spatial search collects only visible + focusable elements, so it reads the
// live row set correctly every frame. Present only ENSURES a focused live row and
// MIRRORS the engine-navigated focus back into m_iCursor -- there is no hand-rolled
// cursor movement.
// ============================================================================
class ZM_UI_Bag
{
public:
	static constexpr u_int uROWS_PER_PAGE = 8u;

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution (never cache element pointers -- the canvas may relocate them).
	static constexpr const char* szPANEL_NAME       = "Menu_BagPanel";
	static constexpr const char* szHEADER_NAME      = "Menu_BagHeader";   // pocket name + money
	static constexpr const char* szPREV_POCKET_NAME = "Menu_BagPrevPocket";
	static constexpr const char* szNEXT_POCKET_NAME = "Menu_BagNextPocket";
	static constexpr const char* szPREV_PAGE_NAME   = "Menu_BagPrevPage";
	static constexpr const char* szNEXT_PAGE_NAME   = "Menu_BagNextPage";

	// ---- Geometry, shared with the ONE placement site (ZM_ConfigureMenuRoot) ----
	// All Y values are offsets from the panel's centre (origin top-left, +Y DOWN),
	// the panel itself being screen-centred. The row stack spans
	// [-170, +178] and the nav band [+192, +228], both comfortably inside the
	// 480-tall panel's [-240, +240] -- nothing the screen draws bleeds outside the
	// box it is drawn over (the S5 visual-gate lesson, ZM-D-112).
	static constexpr float fPANEL_WIDTH  = 760.0f;
	static constexpr float fPANEL_HEIGHT = 480.0f;

	static constexpr float fHEADER_CENTRE_Y = -205.0f;
	static constexpr float fHEADER_WIDTH    = 700.0f;
	static constexpr float fHEADER_HEIGHT   = 40.0f;

	static constexpr float fROW_WIDTH          = 660.0f;
	static constexpr float fROW_HEIGHT         = 40.0f;
	static constexpr float fROW_FIRST_CENTRE_Y = -150.0f;
	static constexpr float fROW_PITCH_Y        = 44.0f;

	// The four nav buttons sit BELOW the rows in one band: the two POCKET buttons on
	// the left, the two PAGE buttons on the right. Like the rows they carry no explicit
	// links, so the spatial search walks down off the LAST LIVE row onto them and back.
	static constexpr float fNAV_BUTTON_CENTRE_Y = 210.0f;
	static constexpr float fNAV_BUTTON_WIDTH    = 170.0f;
	static constexpr float fNAV_BUTTON_HEIGHT   = 36.0f;
	static constexpr float fNAV_BUTTON_INNER_X  = 95.0f;    // the two INNER buttons, +/-
	static constexpr float fNAV_BUTTON_OUTER_X  = 285.0f;   // the two OUTER buttons, +/-

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The element name for a list row ("Menu_BagRow0".."Menu_BagRow7"); "" for an
	// out-of-range row. Returns string LITERALS (the RootItemElementName idiom) --
	// never allocates, never dangles, so bake-time authoring may call it.
	static const char* RowElementName(u_int uRow);
	// The row index for an element name, or -1 when it is not a bag row.
	static int RowIndexFromElementName(const char* szElementName);
	// Pages needed for uStackCount stacks in a pocket. ALWAYS >= 1: an EMPTY pocket
	// still shows one blank page rather than leaving ClampPage nothing to clamp to.
	static u_int PageCount(u_int uStackCount);
	// Clamp a (possibly negative or past-the-end) page into [0, PageCount - 1].
	static int ClampPage(int iPage, u_int uStackCount);
	// Bring a pocket index into range by CYCLING (next past the last wraps to the
	// first, prev past the first wraps to the last). Nine pockets with clamping
	// would strand the player at either end of the row -- so this wraps, unlike
	// ClampPage, which genuinely clamps.
	static int ClampPocket(int iPocket);
	// Step a pocket by iDelta, cycling (see ClampPocket).
	static ZM_ITEM_CATEGORY StepPocket(ZM_ITEM_CATEGORY eCategory, int iDelta);
	// The stack index WITHIN the pocket shown in row uRow of page uPage, or -1 when
	// that row is past the end of the pocket (or out of range).
	static int StackIndexForRow(u_int uPage, u_int uRow, u_int uStackCount);
	// How many rows on uPage map to a real stack: uROWS_PER_PAGE except on the
	// trailing partial page (and 0 for an empty pocket / a page past the end).
	static u_int VisibleRowCount(u_int uPage, u_int uStackCount);
	// One row label, "<Item Name>  x<count>". The name is resolved through
	// ZM_GetItemName -- never a hard-coded string.
	static std::string FormatRow(ZM_ITEM_ID eItem, u_int uCount);
	// The header, "<POCKET NAME>    Money <n>" -- the pocket name via
	// ZM_ItemCategoryToString plus the live money figure.
	static std::string FormatHeader(ZM_ITEM_CATEGORY eCategory, u_int uMoney);
	// The label shown when the selected pocket holds nothing at all.
	static std::string FormatEmptyPocket();

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// Pocket 0 (BALL), page 0, row 0.
	void Reset();
	// Confirm dispatches BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick with a
	// component pointer, which dangles when the ECS pool relocates the host. The four
	// nav buttons page / change pocket; a ROW is deliberately INERT (using or tossing
	// an item is SC7+ territory). Changing POCKET resets the page to 0, because the
	// new pocket may have fewer pages than the old one. Returns true only when the
	// pocket or the page actually changed.
	bool Confirm(const char* szFocusedElementName, const ZM_Bag& xBag);

	ZM_ITEM_CATEGORY GetPocket() const;
	int GetPage() const { return m_iPage; }
	// The focused ROW mirror: -1 while the focus sits on a nav button, while the
	// pocket is empty, and while the screen is not presented at all.
	int GetCursor() const { return m_iCursor; }

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the screen from the live bag: clamps the pocket + page, fills the
	// current page's rows, and refreshes the header from the pocket name + money. A
	// missing UI component or element is skipped silently -- presentation never
	// crashes the menu. The only model state it writes is the pocket / page clamp and
	// the cursor mirror.
	void Present(Zenith_Entity& xRootEntity, const ZM_Bag& xBag, u_int uMoney);
	// Hide the panel, the header, every row and all four nav buttons, and clear the
	// cursor to -1 (nothing is presented, so no row is focused). Deliberately does NOT
	// reset the POCKET or the PAGE -- dropping the session state is Reset's job, which
	// the menu stack owns.
	void Hide(Zenith_Entity& xRootEntity);

private:
	int m_iPocket = 0;   // a ZM_ITEM_CATEGORY held as an int so the state stays POD-simple
	int m_iPage   = 0;
	int m_iCursor = 0;
};
