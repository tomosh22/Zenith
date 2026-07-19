#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_Bag.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"        // ZM_Bag + ZM_ItemStack (the model this renders)

#include <cstring>

// ============================================================================
// ZM_UI_Bag (S6 item 2 SC6). The pocket + page model plus the best-effort
// presentation onto the persistent ZM_MenuRoot canvas. Every widget is authored at
// bake time (see the header for WHY this is a list and not a grid), so there is no
// runtime construction here at all -- Present only re-resolves by name, writes
// labels, and toggles visibility / focusability.
// ============================================================================

// ClampPocket takes a modulo by this, and Reset parks on pocket 0.
static_assert(ZM_ITEM_CATEGORY_COUNT > 0u, "the bag must have at least one pocket");

namespace
{
	// The four nav buttons, in one place: Present shows them, Hide hides them, and the
	// focus policy tests membership against the same list.
	const char* const aszBAG_NAV_BUTTONS[4] =
	{
		ZM_UI_Bag::szPREV_POCKET_NAME,
		ZM_UI_Bag::szNEXT_POCKET_NAME,
		ZM_UI_Bag::szPREV_PAGE_NAME,
		ZM_UI_Bag::szNEXT_PAGE_NAME,
	};

	bool ZM_IsBagNavButtonName(const char* szElementName)
	{
		if (szElementName == nullptr)
		{
			return false;
		}
		for (const char* szNav : aszBAG_NAV_BUTTONS)
		{
			if (std::strcmp(szElementName, szNav) == 0)
			{
				return true;
			}
		}
		return false;
	}

	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolveBagUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_Bag::RowElementName(u_int uRow)
{
	// A table of string LITERALS (never a built std::string): the returned pointer
	// outlives every caller, so even bake-time authoring may call this.
	static const char* const aszROW_NAMES[uROWS_PER_PAGE] =
	{
		"Menu_BagRow0", "Menu_BagRow1", "Menu_BagRow2", "Menu_BagRow3",
		"Menu_BagRow4", "Menu_BagRow5", "Menu_BagRow6", "Menu_BagRow7",
	};
	return (uRow < uROWS_PER_PAGE) ? aszROW_NAMES[uRow] : "";
}

int ZM_UI_Bag::RowIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uROWS_PER_PAGE; ++u)
	{
		if (std::strcmp(szElementName, RowElementName(u)) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

u_int ZM_UI_Bag::PageCount(u_int uStackCount)
{
	if (uStackCount == 0u)
	{
		return 1u;   // an empty pocket still shows one blank page (never 0 -- ClampPage needs a page)
	}
	return (uStackCount + uROWS_PER_PAGE - 1u) / uROWS_PER_PAGE;
}

int ZM_UI_Bag::ClampPage(int iPage, u_int uStackCount)
{
	if (iPage < 0)
	{
		return 0;
	}
	const int iLast = static_cast<int>(PageCount(uStackCount)) - 1;
	return (iPage > iLast) ? iLast : iPage;
}

int ZM_UI_Bag::ClampPocket(int iPocket)
{
	// CYCLES rather than clamping (see the header): nine pockets with a hard clamp
	// would strand the player on the first / last pocket.
	const int iCount = static_cast<int>(ZM_ITEM_CATEGORY_COUNT);
	int iWrapped = iPocket % iCount;
	if (iWrapped < 0)
	{
		iWrapped += iCount;   // C++ '%' truncates towards zero, so a negative input stays negative
	}
	return iWrapped;
}

ZM_ITEM_CATEGORY ZM_UI_Bag::StepPocket(ZM_ITEM_CATEGORY eCategory, int iDelta)
{
	// The incoming category is normalised FIRST: an out-of-range value read as a huge
	// u_int would cast to a negative int, and adding the delta to that before wrapping
	// could land anywhere.
	const int iBase = ClampPocket(static_cast<int>(eCategory));
	return static_cast<ZM_ITEM_CATEGORY>(ClampPocket(iBase + iDelta));
}

int ZM_UI_Bag::StackIndexForRow(u_int uPage, u_int uRow, u_int uStackCount)
{
	if (uRow >= uROWS_PER_PAGE || uPage >= PageCount(uStackCount))
	{
		return -1;
	}
	// uPage < PageCount, so the multiply cannot overflow for any count a u_int can
	// hold a page count for.
	const u_int uAbsolute = uPage * uROWS_PER_PAGE + uRow;
	return (uAbsolute < uStackCount) ? static_cast<int>(uAbsolute) : -1;
}

u_int ZM_UI_Bag::VisibleRowCount(u_int uPage, u_int uStackCount)
{
	if (uPage >= PageCount(uStackCount))
	{
		return 0u;
	}
	const u_int uFirst = uPage * uROWS_PER_PAGE;
	const u_int uRemaining = (uFirst < uStackCount) ? (uStackCount - uFirst) : 0u;
	return (uRemaining < uROWS_PER_PAGE) ? uRemaining : uROWS_PER_PAGE;
}

std::string ZM_UI_Bag::FormatRow(ZM_ITEM_ID eItem, u_int uCount)
{
	// ZM_GetItemName bounds-guards itself ("NONE" out of range), so a stray id reads
	// as an empty slot rather than crashing the screen.
	std::string strRow = ZM_GetItemName(eItem);
	strRow += "  x";
	strRow += std::to_string(uCount);
	return strRow;
}

std::string ZM_UI_Bag::FormatHeader(ZM_ITEM_CATEGORY eCategory, u_int uMoney)
{
	std::string strHeader = ZM_ItemCategoryToString(eCategory);
	strHeader += "    Money ";
	strHeader += std::to_string(uMoney);
	return strHeader;
}

std::string ZM_UI_Bag::FormatEmptyPocket()
{
	return "(empty)";
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_Bag::Reset()
{
	m_iPocket = 0;   // ZM_ITEM_CATEGORY_BALL
	m_iPage   = 0;
	m_iCursor = 0;
}

ZM_ITEM_CATEGORY ZM_UI_Bag::GetPocket() const
{
	return static_cast<ZM_ITEM_CATEGORY>(ClampPocket(m_iPocket));
}

bool ZM_UI_Bag::Confirm(const char* szFocusedElementName, const ZM_Bag& xBag)
{
	if (szFocusedElementName == nullptr)
	{
		return false;
	}

	// The POCKET buttons first: a pocket change always lands somewhere new (nine
	// pockets, a +/-1 cycle), and it MUST reset the page -- the new pocket may hold
	// fewer pages, and a stale page would show a blank list until the next clamp.
	const bool bNextPocket = std::strcmp(szFocusedElementName, szNEXT_POCKET_NAME) == 0;
	const bool bPrevPocket = std::strcmp(szFocusedElementName, szPREV_POCKET_NAME) == 0;
	if (bNextPocket || bPrevPocket)
	{
		m_iPocket = static_cast<int>(StepPocket(GetPocket(), bNextPocket ? 1 : -1));
		m_iPage = 0;
		// m_iCursor is a pure MIRROR of the canvas focus (written by Present / Hide) --
		// the focus is still parked on the button that was just confirmed.
		return true;
	}

	int iWantedPage = m_iPage;
	if (std::strcmp(szFocusedElementName, szNEXT_PAGE_NAME) == 0)
	{
		iWantedPage = m_iPage + 1;
	}
	else if (std::strcmp(szFocusedElementName, szPREV_PAGE_NAME) == 0)
	{
		iWantedPage = m_iPage - 1;
	}
	else
	{
		// A ROW (or any other element) is INERT: using / tossing an item needs an action
		// menu, which is SC7+ territory. A silent no-op is what a not-yet-implemented
		// entry needs.
		return false;
	}

	const int iClamped = ClampPage(iWantedPage, xBag.PocketStackCount(GetPocket()));
	if (iClamped == m_iPage)
	{
		return false;   // already at the end the player pressed towards
	}
	m_iPage = iClamped;
	return true;
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_Bag::Present(Zenith_Entity& xRootEntity, const ZM_Bag& xBag, u_int uMoney)
{
	// Settled from the MODEL before any UI resolve, every frame: the pocket's stack
	// count changes under the screen whenever an item is gained or spent, so a page
	// that was valid last frame may be past the end this one.
	m_iPocket = ClampPocket(m_iPocket);
	const ZM_ITEM_CATEGORY eCategory = static_cast<ZM_ITEM_CATEGORY>(m_iPocket);
	const u_int uStackCount = xBag.PocketStackCount(eCategory);
	m_iPage = ClampPage(m_iPage, uStackCount);
	const u_int uPage = static_cast<u_int>(m_iPage);

	Zenith_UIComponent* pxUI = ZM_ResolveBagUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the screen
	}
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();

	// Re-resolve by NAME every frame (never cache across frames -- the canvas may
	// relocate its elements).
	if (Zenith_UI::Zenith_UIRect* pxPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		// Only write the visibility when it actually CHANGES: SetVisible notifies the
		// parent, and this runs every frame the screen is on top (the SC5 lesson).
		if (!pxPanel->IsVisible())
		{
			pxPanel->SetVisible(true);
		}
	}
	for (const char* szNav : aszBAG_NAV_BUTTONS)
	{
		if (Zenith_UI::Zenith_UIElement* pxButton = pxUI->FindElement(szNav))
		{
			if (!pxButton->IsVisible())
			{
				pxButton->SetVisible(true);
			}
			pxButton->SetFocusable(true);   // a plain assignment (no notify) -- no guard needed
		}
	}

	if (Zenith_UI::Zenith_UIText* pxHeader =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szHEADER_NAME))
	{
		if (!pxHeader->IsVisible())
		{
			pxHeader->SetVisible(true);
		}
		const std::string strHeader = FormatHeader(eCategory, uMoney);
		// SetText rebuilds the word wrap, so only write it when it actually changed.
		if (pxHeader->GetText() != strHeader)
		{
			pxHeader->SetText(strHeader);
		}
	}

	// An EMPTY pocket shows the "(empty)" notice on ROW 0, left VISIBLE but NOT
	// focusable: it is a message, not an item, so the nav must never be able to select
	// it. (The alternative -- writing the notice into the header -- would bury it
	// alongside the money and leave the list area blank.)
	const bool bEmptyPocket = (uStackCount == 0u);
	for (u_int u = 0u; u < uROWS_PER_PAGE; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxRow =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(RowElementName(u));
		if (pxRow == nullptr)
		{
			continue;
		}
		const int iStack = StackIndexForRow(uPage, u, uStackCount);
		const bool bLive = (iStack >= 0);
		const bool bNotice = bEmptyPocket && (u == 0u);
		const bool bVisible = bLive || bNotice;
		// A dead row must be BOTH hidden and non-focusable: the engine's nav collects
		// visible + focusable elements, so leaving it focusable would let the arrows park
		// on a blank entry (the watch-out SC4 and SC5 both paid for).
		if (pxRow->IsVisible() != bVisible)
		{
			pxRow->SetVisible(bVisible);
		}
		pxRow->SetFocusable(bLive);
		if (!bVisible)
		{
			continue;
		}
		std::string strLabel = FormatEmptyPocket();
		if (bLive)
		{
			const ZM_ItemStack& xStack = xBag.PocketStack(eCategory, static_cast<u_int>(iStack));
			strLabel = FormatRow(xStack.m_eItem, xStack.m_uCount);
		}
		if (pxRow->GetText() != strLabel)
		{
			pxRow->SetText(strLabel);
		}
	}

	// Ensure the canvas focus sits somewhere this screen owns (freshly opened, or
	// returned from a screen that cleared it), otherwise MIRROR the engine-navigated
	// focus.
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
	const int iFocusedRow = RowIndexFromElementName(szFocusedName);
	const bool bOnLiveRow = iFocusedRow >= 0
		&& StackIndexForRow(uPage, static_cast<u_int>(iFocusedRow), uStackCount) >= 0;

	if (bOnLiveRow)
	{
		m_iCursor = iFocusedRow;
	}
	else if (ZM_IsBagNavButtonName(szFocusedName))
	{
		m_iCursor = -1;   // a nav button is not a row (GetCursor's contract)
	}
	else if (StackIndexForRow(uPage, 0u, uStackCount) >= 0)
	{
		// Resolve FIRST, then mirror what actually happened: when row 0 does not resolve
		// the focus is CLEARED, and claiming cursor 0 anyway would report a focused entry
		// that nothing is drawing -- a lie ZM_UI_MenuStack::GetCursor carries straight up.
		Zenith_UI::Zenith_UIElement* pxFirstRow = pxUI->FindElement(RowElementName(0u));
		xCanvas.SetFocusedElement(pxFirstRow);
		m_iCursor = (pxFirstRow != nullptr) ? 0 : -1;
	}
	else
	{
		// An empty pocket has no focusable row at all, so the focus goes to a nav button
		// -- otherwise the arrows would drive nothing and the player would be stuck.
		xCanvas.SetFocusedElement(pxUI->FindElement(szNEXT_POCKET_NAME));
		m_iCursor = -1;
	}
}

void ZM_UI_Bag::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no focused row, so the cursor mirror must say so (GetCursor
	// contracts -1 while not presented). Unconditional: the widgets may fail to
	// resolve, but the screen is not presented either way.
	m_iCursor = -1;

	Zenith_UIComponent* pxUI = ZM_ResolveBagUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	// Hide runs EVERY frame the bag is not the top screen, so the visibility writes are
	// change-guarded for the same reason they are in Present.
	const char* const aszHiddenElements[2] = { szPANEL_NAME, szHEADER_NAME };
	for (const char* szName : aszHiddenElements)
	{
		if (Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szName))
		{
			if (pxElement->IsVisible())
			{
				pxElement->SetVisible(false);
			}
		}
	}
	for (u_int u = 0u; u < uROWS_PER_PAGE; ++u)
	{
		if (Zenith_UI::Zenith_UIElement* pxRow = pxUI->FindElement(RowElementName(u)))
		{
			if (pxRow->IsVisible())
			{
				pxRow->SetVisible(false);
			}
			pxRow->SetFocusable(false);   // a hidden row must never stay nav-reachable
		}
	}
	for (const char* szNav : aszBAG_NAV_BUTTONS)
	{
		if (Zenith_UI::Zenith_UIElement* pxButton = pxUI->FindElement(szNav))
		{
			if (pxButton->IsVisible())
			{
				pxButton->SetVisible(false);
			}
			pxButton->SetFocusable(false);
		}
	}
}
