#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_Shop.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Party/ZM_Bag.h"   // ZM_Bag + ZM_ItemStack (the SELL list's model)

#include <cstring>

// ============================================================================
// ZM_UI_Shop (S6 item 2 SC7). The mode / page / quantity / selection model plus the
// best-effort presentation onto the persistent ZM_MenuRoot canvas. Every widget is
// authored at bake time (see the header for WHY this is a list and not a grid), so
// there is no runtime construction here at all -- Present only re-resolves by name,
// writes labels, and toggles visibility / focusability. The only mutation of the
// player's bag or money happens in Confirm, and it happens inside ZM_ShopLogic.
// ============================================================================

namespace
{
	// The eight controls, in the order ZM_ConfigureMenuRoot places them: band 1 left to
	// right (Buy / Sell / CONFIRM / Prev / Next), then band 2 (Qty- / Qty+ / Exit).
	const char* const aszSHOP_CONTROLS[ZM_UI_Shop::uCONTROL_COUNT] =
	{
		ZM_UI_Shop::szBUY_TAB_NAME,
		ZM_UI_Shop::szSELL_TAB_NAME,
		ZM_UI_Shop::szCONFIRM_NAME,
		ZM_UI_Shop::szPREV_PAGE_NAME,
		ZM_UI_Shop::szNEXT_PAGE_NAME,
		ZM_UI_Shop::szQTY_DOWN_NAME,
		ZM_UI_Shop::szQTY_UP_NAME,
		ZM_UI_Shop::szEXIT_NAME,
	};

	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolveShopUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}
}

// ---- The authored control contract ------------------------------------------

const char* ZM_UI_Shop::ControlElementName(u_int uIndex)
{
	return (uIndex < uCONTROL_COUNT) ? aszSHOP_CONTROLS[uIndex] : "";
}

bool ZM_UI_Shop::IsControlElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return false;
	}
	for (const char* szControl : aszSHOP_CONTROLS)
	{
		if (std::strcmp(szElementName, szControl) == 0)
		{
			return true;
		}
	}
	return false;
}

bool ZM_UI_Shop::IsExitElementName(const char* szElementName)
{
	return szElementName != nullptr && std::strcmp(szElementName, szEXIT_NAME) == 0;
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_Shop::RowElementName(u_int uRow)
{
	// A table of string LITERALS (never a built std::string): the returned pointer
	// outlives every caller, so even bake-time authoring may call this.
	static const char* const aszROW_NAMES[uROWS_PER_PAGE] =
	{
		"Menu_ShopRow0", "Menu_ShopRow1", "Menu_ShopRow2",
		"Menu_ShopRow3", "Menu_ShopRow4", "Menu_ShopRow5",
	};
	return (uRow < uROWS_PER_PAGE) ? aszROW_NAMES[uRow] : "";
}

int ZM_UI_Shop::RowIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uROWS_PER_PAGE; ++u)
	{
		if (std::strcmp(szElementName, RowElementName(u)) == 0)
		{
			return (int)u;
		}
	}
	return -1;
}

u_int ZM_UI_Shop::PageCount(u_int uEntryCount)
{
	if (uEntryCount == 0u)
	{
		return 1u;   // an empty list still shows one blank page (ClampPage needs a page)
	}
	return (uEntryCount + uROWS_PER_PAGE - 1u) / uROWS_PER_PAGE;
}

int ZM_UI_Shop::ClampPage(int iPage, u_int uEntryCount)
{
	if (iPage < 0)
	{
		return 0;
	}
	const int iLast = (int)PageCount(uEntryCount) - 1;
	return (iPage > iLast) ? iLast : iPage;
}

int ZM_UI_Shop::StackIndexForRow(u_int uPage, u_int uRow, u_int uEntryCount)
{
	if (uRow >= uROWS_PER_PAGE || uPage >= PageCount(uEntryCount))
	{
		return -1;
	}
	// uPage < PageCount, so the multiply cannot overflow for any count a u_int can hold
	// a page count for.
	const u_int uAbsolute = uPage * uROWS_PER_PAGE + uRow;
	return (uAbsolute < uEntryCount) ? (int)uAbsolute : -1;
}

u_int ZM_UI_Shop::VisibleRowCount(u_int uPage, u_int uEntryCount)
{
	if (uPage >= PageCount(uEntryCount))
	{
		return 0u;
	}
	const u_int uFirst = uPage * uROWS_PER_PAGE;
	const u_int uRemaining = (uFirst < uEntryCount) ? (uEntryCount - uFirst) : 0u;
	return (uRemaining < uROWS_PER_PAGE) ? uRemaining : uROWS_PER_PAGE;
}

int ZM_UI_Shop::ClampQuantity(int iQuantity)
{
	if (iQuantity < iMIN_QUANTITY) { return iMIN_QUANTITY; }
	if (iQuantity > iMAX_QUANTITY) { return iMAX_QUANTITY; }
	return iQuantity;
}

const char* ZM_UI_Shop::ModeToString(ZM_SHOP_MODE eMode)
{
	switch (eMode)
	{
	case ZM_SHOP_MODE_BUY:  return "BUY";
	case ZM_SHOP_MODE_SELL: return "SELL";
	default:                return "";
	}
}

std::string ZM_UI_Shop::FormatBuyRow(ZM_ITEM_ID eItem)
{
	// Both halves come from the item table (ZM_GetItemName bounds-guards itself, and
	// ZM_ShopBuyPrice reads m_uBuyPrice) -- never a literal price.
	std::string strRow = ZM_GetItemName(eItem);
	strRow += "   Buy ";
	strRow += std::to_string(ZM_ShopBuyPrice(eItem));
	return strRow;
}

std::string ZM_UI_Shop::FormatSellRow(ZM_ITEM_ID eItem, u_int uHeld)
{
	std::string strRow = ZM_GetItemName(eItem);
	strRow += "  x";
	strRow += std::to_string(uHeld);
	strRow += "   Sell ";
	strRow += std::to_string(ZM_ShopSellPrice(eItem));
	return strRow;
}

std::string ZM_UI_Shop::FormatHeader(ZM_SHOP_MODE eMode, u_int uMoney, u_int uQuantity)
{
	std::string strHeader = ModeToString(eMode);
	strHeader += "    Money ";
	strHeader += std::to_string(uMoney);
	strHeader += "    Qty ";
	strHeader += std::to_string(uQuantity);
	return strHeader;
}

const char* ZM_UI_Shop::FormatResult(ZM_SHOP_RESULT eResult)
{
	// TOTAL by construction: one arm per ZM_SHOP_RESULT, and the trailing return covers
	// any stray value. EVERY line is non-empty -- a blank report would read as the shop
	// ignoring the player, which is exactly what the refusals must never look like.
	switch (eResult)
	{
	case ZM_SHOP_OK:                    return "Thank you!";
	case ZM_SHOP_ERR_INVALID_ITEM:      return "There is nothing selected.";
	case ZM_SHOP_ERR_INVALID_QUANTITY:  return "Pick at least one.";
	case ZM_SHOP_ERR_NOT_PURCHASABLE:   return "That one is not for sale.";
	case ZM_SHOP_ERR_NOT_SELLABLE:      return "I cannot buy that from you.";
	case ZM_SHOP_ERR_CANNOT_AFFORD:     return "You cannot afford that.";
	case ZM_SHOP_ERR_NOT_ENOUGH_HELD:   return "You do not have that many.";
	case ZM_SHOP_ERR_NO_BAG_ROOM:       return "Your bag has no room for those.";
	case ZM_SHOP_ERR_MONEY_CAPPED:      return "You are carrying too much money.";
	default:                            break;
	}
	return "Something went wrong.";
}

std::string ZM_UI_Shop::FormatHeaderLine(ZM_SHOP_MODE eMode, u_int uMoney, u_int uQuantity,
	bool bHasResult, ZM_SHOP_RESULT eResult)
{
	std::string strLine = FormatHeader(eMode, uMoney, uQuantity);
	if (bHasResult)
	{
		// The report rides in the header rather than in a widget of its own: it is one
		// line, and a dedicated element would have to be authored, shown and hidden for it.
		strLine += "    ";
		strLine += FormatResult(eResult);
	}
	return strLine;
}

std::string ZM_UI_Shop::FormatEmptyList()
{
	return "(nothing here)";
}

bool ZM_UI_Shop::SellEntryAt(const ZM_Bag& xBag, u_int uIndex, ZM_ITEM_ID& eItemOut, u_int& uHeldOut)
{
	eItemOut = ZM_ITEM_NONE;
	uHeldOut = 0u;

	// Pocket order, then stack order inside the pocket. ZM_Bag keeps every pocket
	// sorted ascending by id, so this flattening is deterministic without a sort pass.
	u_int uSeen = 0u;
	for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
	{
		const ZM_ITEM_CATEGORY eCategory = (ZM_ITEM_CATEGORY)uCategory;
		const u_int uStacks = xBag.PocketStackCount(eCategory);
		if (uIndex < uSeen + uStacks)
		{
			const ZM_ItemStack& xStack = xBag.PocketStack(eCategory, uIndex - uSeen);
			eItemOut = xStack.m_eItem;
			uHeldOut = xStack.m_uCount;
			return true;
		}
		uSeen += uStacks;
	}
	return false;
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_Shop::Reset()
{
	for (ZM_ITEM_ID& eItem : m_aeInventory)
	{
		eItem = ZM_ITEM_NONE;
	}
	m_uInventoryCount = 0u;
	m_iMode       = (int)ZM_SHOP_MODE_BUY;
	m_iPage       = 0;
	m_iQuantity   = iMIN_QUANTITY;
	m_iCursor     = 0;
	m_uLastResult = (u_int)ZM_SHOP_OK;
	m_bHasResult  = false;
}

bool ZM_UI_Shop::SetInventory(const ZM_ITEM_ID* paeInventory, u_int uCount)
{
	// VALIDATE THE WHOLE LIST FIRST -- nothing is written until every id is known good,
	// so a rejected configuration leaves the previous shop's stock bit-identical.
	if (paeInventory == nullptr) { return false; }
	if (uCount == 0u || uCount > uMAX_INVENTORY) { return false; }
	for (u_int u = 0u; u < uCount; ++u)
	{
		// NONE is DEFINED as ZM_ITEM_COUNT, so this covers it.
		if ((u_int)paeInventory[u] >= (u_int)ZM_ITEM_COUNT) { return false; }
	}

	Reset();   // entering a shop IS a fresh session: mode / page / quantity / report
	for (u_int u = 0u; u < uCount; ++u)
	{
		m_aeInventory[u] = paeInventory[u];
	}
	m_uInventoryCount = uCount;
	return true;
}

ZM_SHOP_MODE ZM_UI_Shop::GetMode() const
{
	// Total: anything that is not SELL reads as BUY, so a stray value can never index
	// past the mode table.
	return (m_iMode == (int)ZM_SHOP_MODE_SELL) ? ZM_SHOP_MODE_SELL : ZM_SHOP_MODE_BUY;
}

ZM_ITEM_ID ZM_UI_Shop::GetInventoryItem(u_int uIndex) const
{
	return (uIndex < m_uInventoryCount) ? m_aeInventory[uIndex] : ZM_ITEM_NONE;
}

u_int ZM_UI_Shop::EntryCount(const ZM_Bag& xBag) const
{
	return (GetMode() == ZM_SHOP_MODE_SELL) ? xBag.TotalStackCount() : m_uInventoryCount;
}

bool ZM_UI_Shop::EntryAt(const ZM_Bag& xBag, u_int uIndex, ZM_ITEM_ID& eItemOut, u_int& uHeldOut) const
{
	eItemOut = ZM_ITEM_NONE;
	uHeldOut = 0u;

	if (GetMode() == ZM_SHOP_MODE_SELL)
	{
		return SellEntryAt(xBag, uIndex, eItemOut, uHeldOut);
	}
	if (uIndex >= m_uInventoryCount)
	{
		return false;
	}
	eItemOut = m_aeInventory[uIndex];
	uHeldOut = xBag.GetCount(eItemOut);   // what the player already carries of it
	return true;
}

int ZM_UI_Shop::GetSelectedEntryIndex(const ZM_Bag& xBag) const
{
	if (m_iCursor < 0)
	{
		return -1;
	}
	// The page is re-clamped here rather than trusted: the list shrinks under the screen
	// on every sale, so a page that was valid when it was set may be past the end now.
	const u_int uEntries = EntryCount(xBag);
	const u_int uPage = (u_int)ClampPage(m_iPage, uEntries);
	return StackIndexForRow(uPage, (u_int)m_iCursor, uEntries);
}

ZM_SHOP_RESULT ZM_UI_Shop::Confirm(const char* szFocusedElementName, ZM_Bag& xBag, u_int& uMoneyInOut)
{
	if (szFocusedElementName == nullptr)
	{
		return ZM_SHOP_OK;   // nothing focused: not a transaction, so nothing to report
	}

	// ---- The mode tabs. A mode switch re-lists everything, so the page AND the
	//      quantity go back to their defaults (a page 3 selection in a two-page list
	//      would show a blank list until the next clamp). ----
	const bool bBuyTab  = std::strcmp(szFocusedElementName, szBUY_TAB_NAME) == 0;
	const bool bSellTab = std::strcmp(szFocusedElementName, szSELL_TAB_NAME) == 0;
	if (bBuyTab || bSellTab)
	{
		const int iWanted = bSellTab ? (int)ZM_SHOP_MODE_SELL : (int)ZM_SHOP_MODE_BUY;
		if (iWanted != (int)GetMode())
		{
			m_iMode     = iWanted;
			m_iPage     = 0;
			m_iQuantity = iMIN_QUANTITY;
			m_iCursor   = 0;   // Present re-points it at a live row (or -1 on an empty list)
		}
		return ZM_SHOP_OK;
	}

	// ---- Paging (clamped against the CURRENT list, which the mode decides) ----
	if (std::strcmp(szFocusedElementName, szPREV_PAGE_NAME) == 0)
	{
		m_iPage = ClampPage(m_iPage - 1, EntryCount(xBag));
		return ZM_SHOP_OK;
	}
	if (std::strcmp(szFocusedElementName, szNEXT_PAGE_NAME) == 0)
	{
		m_iPage = ClampPage(m_iPage + 1, EntryCount(xBag));
		return ZM_SHOP_OK;
	}

	// ---- The quantity stepper ----
	if (std::strcmp(szFocusedElementName, szQTY_DOWN_NAME) == 0)
	{
		m_iQuantity = ClampQuantity(m_iQuantity - 1);
		return ZM_SHOP_OK;
	}
	if (std::strcmp(szFocusedElementName, szQTY_UP_NAME) == 0)
	{
		m_iQuantity = ClampQuantity(m_iQuantity + 1);
		return ZM_SHOP_OK;
	}

	// ---- The transaction. This is the ONLY element that moves money, which is why a
	//      row is inert: a stray confirm while the focus sits on the list must never
	//      buy anything. ----
	if (std::strcmp(szFocusedElementName, szCONFIRM_NAME) == 0)
	{
		// The SAME resolve every caller gets (m_iCursor is a page-relative ROW), so the
		// entry the screen reports as selected and the entry the money is spent on can
		// never diverge.
		const int iEntry = GetSelectedEntryIndex(xBag);

		ZM_ITEM_ID eItem = ZM_ITEM_NONE;
		u_int uHeld = 0u;
		if (iEntry < 0 || !EntryAt(xBag, (u_int)iEntry, eItem, uHeld))
		{
			// Nothing selected (an empty list). REPORTED rather than silently swallowed, so
			// the header tells the player why the press did nothing.
			m_uLastResult = (u_int)ZM_SHOP_ERR_INVALID_ITEM;
			m_bHasResult  = true;
			return ZM_SHOP_ERR_INVALID_ITEM;
		}

		const u_int uQuantity = GetQuantity();
		const ZM_SHOP_RESULT eResult = (GetMode() == ZM_SHOP_MODE_SELL)
			? ZM_ShopSell(xBag, uMoneyInOut, eItem, uQuantity)
			: ZM_ShopBuy(xBag, uMoneyInOut, eItem, uQuantity);
		m_uLastResult = (u_int)eResult;
		m_bHasResult  = true;
		return eResult;
	}

	// A ROW, the Exit control (the menu stack owns leaving) or any foreign name: inert.
	return ZM_SHOP_OK;
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_Shop::Present(Zenith_Entity& xRootEntity, const ZM_Bag& xBag, u_int uMoney)
{
	// Settled from the MODEL before any UI resolve, every frame: the list shrinks under
	// the screen on every sale, so a page (and a selection) that were valid last frame
	// may be past the end this one.
	m_iMode = (int)GetMode();
	const u_int uEntries = EntryCount(xBag);
	m_iPage = ClampPage(m_iPage, uEntries);
	m_iQuantity = ClampQuantity(m_iQuantity);
	const u_int uPage = (u_int)m_iPage;

	Zenith_UIComponent* pxUI = ZM_ResolveShopUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the screen
	}

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
	for (const char* szControl : aszSHOP_CONTROLS)
	{
		if (Zenith_UI::Zenith_UIElement* pxControl = pxUI->FindElement(szControl))
		{
			if (!pxControl->IsVisible())
			{
				pxControl->SetVisible(true);
			}
			pxControl->SetFocusable(true);   // a plain assignment (no notify) -- no guard needed
		}
	}

	if (Zenith_UI::Zenith_UIText* pxHeader =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szHEADER_NAME))
	{
		if (!pxHeader->IsVisible())
		{
			pxHeader->SetVisible(true);
		}
		const std::string strHeader = FormatHeaderLine(
			GetMode(), uMoney, GetQuantity(), m_bHasResult, GetLastResult());
		// SetText rebuilds the word wrap, so only write it when it actually changed.
		if (pxHeader->GetText() != strHeader)
		{
			pxHeader->SetText(strHeader);
		}
	}

	// An EMPTY list shows the notice on ROW 0, left VISIBLE but NOT focusable: it is a
	// message, not an entry, so the nav must never be able to select it (and Confirm can
	// then never resolve it into a transaction).
	const bool bEmptyList = (uEntries == 0u);
	for (u_int u = 0u; u < uROWS_PER_PAGE; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxRow =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(RowElementName(u));
		if (pxRow == nullptr)
		{
			continue;
		}
		const int iEntry = StackIndexForRow(uPage, u, uEntries);
		const bool bLive = (iEntry >= 0);
		const bool bNotice = bEmptyList && (u == 0u);
		const bool bVisible = bLive || bNotice;
		// A dead row must be BOTH hidden and non-focusable: the engine's nav collects
		// visible + focusable elements, so leaving it focusable would let the arrows park
		// on a blank entry (the watch-out SC4, SC5 and SC6 all paid for).
		if (pxRow->IsVisible() != bVisible)
		{
			pxRow->SetVisible(bVisible);
		}
		pxRow->SetFocusable(bLive);
		if (!bVisible)
		{
			continue;
		}

		std::string strLabel = FormatEmptyList();
		ZM_ITEM_ID eItem = ZM_ITEM_NONE;
		u_int uHeld = 0u;
		if (bLive && EntryAt(xBag, (u_int)iEntry, eItem, uHeld))
		{
			strLabel = (GetMode() == ZM_SHOP_MODE_SELL)
				? FormatSellRow(eItem, uHeld)
				: FormatBuyRow(eItem);
		}
		if (pxRow->GetText() != strLabel)
		{
			pxRow->SetText(strLabel);
		}
	}

	m_iCursor = SettleFocus(*pxUI, uPage, uEntries);
}

int ZM_UI_Shop::SettleFocus(Zenith_UIComponent& xUI, u_int uPage, u_int uEntryCount)
{
	Zenith_UI::Zenith_UICanvas& xCanvas = xUI.GetCanvas();
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;

	const int iFocusedRow = RowIndexFromElementName(szFocusedName);
	if (iFocusedRow >= 0 && StackIndexForRow(uPage, (u_int)iFocusedRow, uEntryCount) >= 0)
	{
		return iFocusedRow;   // the player is standing on a live row: that IS the selection
	}

	if (IsControlElementName(szFocusedName))
	{
		// THE SELECTION SURVIVES THE WALK onto a control -- otherwise the entry the player
		// picked would be forgotten by the time they reached Confirm, and Confirm would buy
		// whatever row 0 happened to be. It is only re-pointed when the list moved under it
		// (a sale erased the stack, a page turned).
		if (m_iCursor >= 0 && StackIndexForRow(uPage, (u_int)m_iCursor, uEntryCount) >= 0)
		{
			return m_iCursor;
		}
		return (StackIndexForRow(uPage, 0u, uEntryCount) >= 0) ? 0 : -1;
	}

	// Nothing of ours holds the focus (freshly opened, or returned from another screen):
	// park it. RESOLVE FIRST and mirror what actually happened -- claiming a selection
	// the canvas never took would report an entry that nothing is drawing.
	if (StackIndexForRow(uPage, 0u, uEntryCount) >= 0)
	{
		Zenith_UI::Zenith_UIElement* pxFirstRow = xUI.FindElement(RowElementName(0u));
		xCanvas.SetFocusedElement(pxFirstRow);
		return (pxFirstRow != nullptr) ? 0 : -1;
	}

	// An empty list has no focusable row at all, so the focus goes to a control --
	// Exit, which is always meaningful; otherwise the arrows would drive nothing.
	xCanvas.SetFocusedElement(xUI.FindElement(szEXIT_NAME));
	return -1;
}

void ZM_UI_Shop::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no selection, so the cursor mirror must say so.
	// Unconditional: the widgets may fail to resolve, but the screen is not presented
	// either way.
	m_iCursor = -1;

	Zenith_UIComponent* pxUI = ZM_ResolveShopUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	// Hide runs EVERY frame the shop is not the top screen, so the visibility writes are
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
	for (const char* szControl : aszSHOP_CONTROLS)
	{
		if (Zenith_UI::Zenith_UIElement* pxControl = pxUI->FindElement(szControl))
		{
			if (pxControl->IsVisible())
			{
				pxControl->SetVisible(false);
			}
			pxControl->SetFocusable(false);
		}
	}
}
