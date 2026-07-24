#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "ZenithECS/Zenith_Entity.h"

#include <cstring>

// ============================================================================
// ZM_UI_TitleMenu (S7 item 2 SC5). Open owns the complete status-snapshot refresh;
// Present owns only UI projection and never reads disk. This preserves one policy
// boundary for DAMAGED-vs-EMPTY while keeping the presenter deterministic in boot
// tests and pointer-safe across persistent-scene ECS relocations.
// ============================================================================

namespace
{
	Zenith_UIComponent* ZM_ResolveTitleUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}

	void ZM_SetTitleNavigation(Zenith_UI::Zenith_UIButton* pxContinue,
		Zenith_UI::Zenith_UIButton* pxNewGame, bool bContinueLive)
	{
		// Explicit navigation must name only live controls. In particular, New Game may
		// never retain an authored Up/Down link to Continue after Continue is hidden: the
		// engine drops an explicit hidden target without spatial fallback.
		Zenith_UI::Zenith_UIElement* pxLiveContinue = bContinueLive ? pxContinue : nullptr;
		if (pxContinue != nullptr)
		{
			Zenith_UI::Zenith_UIElement* pxOther = (bContinueLive && pxNewGame != nullptr)
				? static_cast<Zenith_UI::Zenith_UIElement*>(pxNewGame)
				: nullptr;
			pxContinue->SetNavigation(pxOther, pxOther, nullptr, nullptr);
		}
		if (pxNewGame != nullptr)
		{
			pxNewGame->SetNavigation(pxLiveContinue, pxLiveContinue, nullptr, nullptr);
		}
	}
}

// ---- PURE name/action policy -----------------------------------------------

const char* ZM_UI_TitleMenu::ItemElementName(ZM_TITLE_ITEM eItem)
{
	switch (eItem)
	{
	case ZM_TITLE_ITEM_CONTINUE: return szCONTINUE_NAME;
	case ZM_TITLE_ITEM_NEW_GAME: return szNEW_GAME_NAME;
	default:                     return "";
	}
}

int ZM_UI_TitleMenu::ItemIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < static_cast<u_int>(ZM_TITLE_ITEM_COUNT); ++u)
	{
		if (std::strcmp(szElementName,
			ItemElementName(static_cast<ZM_TITLE_ITEM>(u))) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

ZM_TITLE_ACTION ZM_UI_TitleMenu::ResolveAction(const char* szFocusedElementName)
{
	switch (ItemIndexFromElementName(szFocusedElementName))
	{
	case static_cast<int>(ZM_TITLE_ITEM_CONTINUE): return ZM_TITLE_ACTION_OPEN_LOAD;
	case static_cast<int>(ZM_TITLE_ITEM_NEW_GAME): return ZM_TITLE_ACTION_NEW_GAME;
	default:                                       return ZM_TITLE_ACTION_NONE;
	}
}

// ---- Instance model ---------------------------------------------------------

void ZM_UI_TitleMenu::Open(const ZM_SAVE_SLOT_STATUS* paeStatuses, u_int uCount)
{
	// Refresh is replacement, never accumulation: clearing FIRST prevents a removed
	// READY file from leaving stale Continue visibility or focus behind.
	m_bHasOccupiedSlot = false;
	m_bHasReadySlot = false;

	if (paeStatuses == nullptr || uCount != static_cast<u_int>(ZM_SAVE_SLOT_COUNT))
	{
		return;   // malformed/partial snapshots fail closed to all-empty
	}

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_SAVE_SLOT_STATUS eStatus = paeStatuses[u];
		m_bHasOccupiedSlot = m_bHasOccupiedSlot || eStatus != ZM_SAVE_SLOT_EMPTY;
		m_bHasReadySlot = m_bHasReadySlot || eStatus == ZM_SAVE_SLOT_READY;
	}
}

bool ZM_UI_TitleMenu::IsItemVisible(ZM_TITLE_ITEM eItem) const
{
	switch (eItem)
	{
	case ZM_TITLE_ITEM_CONTINUE: return m_bHasOccupiedSlot;
	case ZM_TITLE_ITEM_NEW_GAME: return true;
	default:                     return false;
	}
}

bool ZM_UI_TitleMenu::IsItemFocusable(ZM_TITLE_ITEM eItem) const
{
	// Title controls have no disabled-but-visible state: Continue either surfaces the
	// LOAD screen (including damaged-only) or is completely absent; New Game is live.
	return IsItemVisible(eItem);
}

ZM_TITLE_ITEM ZM_UI_TitleMenu::GetDefaultFocusItem() const
{
	return m_bHasOccupiedSlot ? ZM_TITLE_ITEM_CONTINUE : ZM_TITLE_ITEM_NEW_GAME;
}

ZM_TITLE_ACTION ZM_UI_TitleMenu::ResolveConfirm(const char* szFocusedElementName) const
{
	const int iItem = ItemIndexFromElementName(szFocusedElementName);
	if (iItem < 0 || !IsItemFocusable(static_cast<ZM_TITLE_ITEM>(iItem)))
	{
		return ZM_TITLE_ACTION_NONE;
	}
	return ResolveAction(szFocusedElementName);
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_TitleMenu::Present(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = ZM_ResolveTitleUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: an unbaked/missing UI never crashes the title
	}

	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();
	if (Zenith_UI::Zenith_UIRect* pxPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		if (!pxPanel->IsVisible())
		{
			pxPanel->SetVisible(true);
		}
	}

	Zenith_UI::Zenith_UIButton* pxContinue =
		pxUI->FindElement<Zenith_UI::Zenith_UIButton>(szCONTINUE_NAME);
	Zenith_UI::Zenith_UIButton* pxNewGame =
		pxUI->FindElement<Zenith_UI::Zenith_UIButton>(szNEW_GAME_NAME);

	const bool bContinueLive = IsItemVisible(ZM_TITLE_ITEM_CONTINUE);
	if (pxContinue != nullptr)
	{
		if (pxContinue->IsVisible() != bContinueLive)
		{
			pxContinue->SetVisible(bContinueLive);
		}
		pxContinue->SetFocusable(bContinueLive);
		if (pxContinue->GetText() != "Continue")
		{
			pxContinue->SetText("Continue");
		}
	}
	if (pxNewGame != nullptr)
	{
		if (!pxNewGame->IsVisible())
		{
			pxNewGame->SetVisible(true);
		}
		pxNewGame->SetFocusable(true);
		if (pxNewGame->GetText() != "New Game")
		{
			pxNewGame->SetText("New Game");
		}
	}

	ZM_SetTitleNavigation(pxContinue, pxNewGame, bContinueLive);

	// Keep focus only when it names a currently-live title control. This repairs the
	// exact READY->EMPTY refresh case where the canvas still points at hidden Continue,
	// and also rehomes focus when returning from the overlaid LOAD screen.
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const int iFocused = (pxFocused != nullptr)
		? ItemIndexFromElementName(pxFocused->GetName().c_str())
		: -1;
	if (iFocused >= 0 && IsItemFocusable(static_cast<ZM_TITLE_ITEM>(iFocused)))
	{
		return;
	}

	Zenith_UI::Zenith_UIElement* pxDefault =
		(GetDefaultFocusItem() == ZM_TITLE_ITEM_CONTINUE)
			? static_cast<Zenith_UI::Zenith_UIElement*>(pxContinue)
			: static_cast<Zenith_UI::Zenith_UIElement*>(pxNewGame);
	// Best-effort fallback for a stale bake missing the preferred control. Never focus
	// hidden Continue: New Game is the only fallback from it, while a missing New Game
	// may use Continue only when Continue is live.
	if (pxDefault == nullptr)
	{
		pxDefault = (pxNewGame != nullptr)
			? static_cast<Zenith_UI::Zenith_UIElement*>(pxNewGame)
			: (bContinueLive
				? static_cast<Zenith_UI::Zenith_UIElement*>(pxContinue)
				: nullptr);
	}
	xCanvas.SetFocusedElement(pxDefault);
}

void ZM_UI_TitleMenu::Hide(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = ZM_ResolveTitleUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}

	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const bool bTitleOwnsFocus = pxFocused != nullptr
		&& ItemIndexFromElementName(pxFocused->GetName().c_str()) >= 0;

	if (Zenith_UI::Zenith_UIElement* pxPanel = pxUI->FindElement(szPANEL_NAME))
	{
		if (pxPanel->IsVisible())
		{
			pxPanel->SetVisible(false);
		}
	}
	const char* const aszButtons[2] = { szCONTINUE_NAME, szNEW_GAME_NAME };
	for (const char* szName : aszButtons)
	{
		if (Zenith_UI::Zenith_UIElement* pxButton = pxUI->FindElement(szName))
		{
			if (pxButton->IsVisible())
			{
				pxButton->SetVisible(false);
			}
			pxButton->SetFocusable(false);
			pxButton->SetNavigation(nullptr, nullptr, nullptr, nullptr);
		}
	}
	if (bTitleOwnsFocus)
	{
		xCanvas.SetFocusedElement(nullptr);
	}
}
