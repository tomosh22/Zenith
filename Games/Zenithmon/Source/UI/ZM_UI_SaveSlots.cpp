#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// ZM_UI_SaveSlots (S7 item 2 SC4). The PURE row/label/action policy plus the
// best-effort presentation onto the persistent ZM_MenuRoot canvas. Every widget
// is authored at bake time by ZM_ConfigureMenuRoot (see the header), so there is
// no runtime construction here -- Present only re-resolves by name, writes labels,
// and toggles visibility / focus. The cached slot statuses are filled ONLY by
// Open (an uncached disk re-probe); Present never touches disk.
// ============================================================================

static_assert(ZM_UI_SaveSlots::uROW_COUNT == static_cast<u_int>(ZM_SAVE_SLOT_COUNT),
	"one row per save slot (SaveScreen_RowCountEqualsSlotCount)");

namespace
{
	// The word shown for a slot's status. TOTAL: an out-of-range status reads "Unknown"
	// rather than indexing anything, and the three real statuses each read distinctly so
	// a player can never mistake a DAMAGED slot for a good one and overwrite it.
	const char* ZM_SaveStatusWord(ZM_SAVE_SLOT_STATUS eStatus)
	{
		switch (eStatus)
		{
		case ZM_SAVE_SLOT_EMPTY:   return "Empty";
		case ZM_SAVE_SLOT_READY:   return "Ready";
		case ZM_SAVE_SLOT_DAMAGED: return "Damaged";
		default:                   return "Unknown";
		}
	}

	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolveSaveUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_SaveSlots::RowElementName(u_int uRow)
{
	// A table of string LITERALS (never a built std::string): the returned pointer
	// outlives every caller, so even bake-time authoring may call this.
	static const char* const aszROW_NAMES[uROW_COUNT] =
	{
		"Menu_SaveRow0", "Menu_SaveRow1", "Menu_SaveRow2", "Menu_SaveRow3",
	};
	return (uRow < uROW_COUNT) ? aszROW_NAMES[uRow] : "";
}

int ZM_UI_SaveSlots::RowIndexFromElementName(const char* szName)
{
	if (szName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uROW_COUNT; ++u)
	{
		if (std::strcmp(szName, RowElementName(u)) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

bool ZM_UI_SaveSlots::IsCancelElementName(const char* szName)
{
	return szName != nullptr && std::strcmp(szName, szCANCEL_NAME) == 0;
}

ZM_SAVE_ROW_ACTION ZM_UI_SaveSlots::ResolveRowAction(ZM_SAVE_SCREEN_MODE eMode,
	ZM_SAVE_SLOT eSlot, ZM_SAVE_SLOT_STATUS eStatus)
{
	// TOTAL: a bad mode / slot / status is never confirmable.
	if (static_cast<u_int>(eMode) >= static_cast<u_int>(ZM_SAVE_SCREEN_MODE_COUNT)) { return ZM_SAVE_ROW_ACTION_NONE; }
	if (static_cast<u_int>(eSlot) >= static_cast<u_int>(ZM_SAVE_SLOT_COUNT)) { return ZM_SAVE_ROW_ACTION_NONE; }
	if (static_cast<u_int>(eStatus) >= static_cast<u_int>(ZM_SAVE_SLOT_STATUS_COUNT)) { return ZM_SAVE_ROW_ACTION_NONE; }

	if (eMode == ZM_SAVE_SCREEN_MODE_SAVE)
	{
		// The manual flow may NEVER write Auto (SaveFormat.md:42-45): only Save0-2 are
		// writable, Auto belongs to the milestone path alone.
		if (!ZM_SaveSlots::IsManualSlot(eSlot))
		{
			return ZM_SAVE_ROW_ACTION_NONE;
		}
		switch (eStatus)
		{
		case ZM_SAVE_SLOT_EMPTY:
			return ZM_SAVE_ROW_ACTION_WRITE;
		// A READY or DAMAGED slot is overwritten ONLY through the yes/no confirm. A damaged
		// slot is surfaced and overwrite-via-confirm, NEVER silently reset or loaded
		// (SaveFormat.md:318-321) -- so it maps to CONFIRM_WRITE, never straight WRITE.
		case ZM_SAVE_SLOT_READY:
		case ZM_SAVE_SLOT_DAMAGED:
			return ZM_SAVE_ROW_ACTION_CONFIRM_WRITE;
		default:
			return ZM_SAVE_ROW_ACTION_NONE;
		}
	}

	// LOAD mode: only a READY slot loads (any slot, Auto included), always behind the
	// confirm because loading discards live progress. EMPTY and DAMAGED are not loadable.
	if (eStatus == ZM_SAVE_SLOT_READY)
	{
		return ZM_SAVE_ROW_ACTION_CONFIRM_LOAD;
	}
	return ZM_SAVE_ROW_ACTION_NONE;
}

void ZM_UI_SaveSlots::FormatRowLabel(ZM_SAVE_SLOT eSlot, ZM_SAVE_SLOT_STATUS eStatus,
	char* pszOut, u_int uCapacity)
{
	if (pszOut == nullptr || uCapacity == 0u)
	{
		return;
	}
	// snprintf always writes at most uCapacity-1 chars plus a terminator, so this never
	// overruns even the tiny capacities the unit tests pass.
	std::snprintf(pszOut, static_cast<size_t>(uCapacity), "%s -- %s",
		ZM_SaveSlots::SlotDisplayName(eSlot), ZM_SaveStatusWord(eStatus));
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_SaveSlots::Reset()
{
	m_eMode = ZM_SAVE_SCREEN_MODE_SAVE;
	for (u_int u = 0u; u < uROW_COUNT; ++u)
	{
		m_aeStatus[u] = ZM_SAVE_SLOT_EMPTY;
	}
	m_iSelectedRow = -1;
}

void ZM_UI_SaveSlots::Open(ZM_SAVE_SCREEN_MODE eMode)
{
	// An out-of-range mode folds to SAVE rather than being stored raw -- otherwise
	// ResolveRowAction would hit its default arm on every row.
	m_eMode = (static_cast<u_int>(eMode) < static_cast<u_int>(ZM_SAVE_SCREEN_MODE_COUNT))
		? eMode
		: ZM_SAVE_SCREEN_MODE_SAVE;
	// Re-probe uncached: the slot screen must show the CURRENT disk state every time it
	// opens (another process, or this session's earlier save, may have changed it).
	for (u_int u = 0u; u < uROW_COUNT; ++u)
	{
		m_aeStatus[u] = ZM_SaveSlots::ProbeSlot(static_cast<ZM_SAVE_SLOT>(u));
	}
	m_iSelectedRow = -1;
}

ZM_SAVE_SLOT_STATUS ZM_UI_SaveSlots::GetRowStatus(u_int uRow) const
{
	return (uRow < uROW_COUNT) ? m_aeStatus[uRow] : ZM_SAVE_SLOT_EMPTY;
}

ZM_SAVE_ROW_ACTION ZM_UI_SaveSlots::ResolveConfirm(const char* szFocusedElementName,
	ZM_SAVE_SLOT& eSlotOut) const
{
	eSlotOut = ZM_SAVE_SLOT_NONE;
	const int iRow = RowIndexFromElementName(szFocusedElementName);
	if (iRow < 0)
	{
		// Back, a foreign name and null all resolve to NONE + NONE.
		return ZM_SAVE_ROW_ACTION_NONE;
	}
	// uROW_COUNT == ZM_SAVE_SLOT_COUNT, so the row index IS the slot ordinal.
	const ZM_SAVE_SLOT eSlot = static_cast<ZM_SAVE_SLOT>(iRow);
	eSlotOut = eSlot;
	return ResolveRowAction(m_eMode, eSlot, m_aeStatus[iRow]);
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_SaveSlots::Present(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = ZM_ResolveSaveUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the screen
	}
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();

	// Panel.
	if (Zenith_UI::Zenith_UIRect* pxPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		if (!pxPanel->IsVisible())
		{
			pxPanel->SetVisible(true);
		}
	}

	// Header: names the mode. Generic wording only (no Nintendo IP, Scope.md:65-66).
	if (Zenith_UI::Zenith_UIText* pxHeader =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szHEADER_NAME))
	{
		if (!pxHeader->IsVisible())
		{
			pxHeader->SetVisible(true);
		}
		const char* szTitle = (m_eMode == ZM_SAVE_SCREEN_MODE_LOAD) ? "Load Game" : "Save Game";
		if (pxHeader->GetText() != szTitle)
		{
			pxHeader->SetText(szTitle);
		}
	}

	// The four rows. Every one stays VISIBLE + FOCUSABLE regardless of status (ZM-D-119):
	// the authored nav links point only at rows that are never hidden, so no press is ever
	// swallowed. A non-confirmable row is disarmed by ResolveRowAction, not by hiding it.
	for (u_int u = 0u; u < uROW_COUNT; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxRow =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(RowElementName(u));
		if (pxRow == nullptr)
		{
			continue;
		}
		if (!pxRow->IsVisible())
		{
			pxRow->SetVisible(true);
		}
		pxRow->SetFocusable(true);
		char acLabel[uLABEL_CAPACITY];
		FormatRowLabel(static_cast<ZM_SAVE_SLOT>(u), m_aeStatus[u], acLabel, uLABEL_CAPACITY);
		if (pxRow->GetText() != acLabel)
		{
			pxRow->SetText(acLabel);
		}
	}

	// Back.
	if (Zenith_UI::Zenith_UIButton* pxCancel =
		pxUI->FindElement<Zenith_UI::Zenith_UIButton>(szCANCEL_NAME))
	{
		if (!pxCancel->IsVisible())
		{
			pxCancel->SetVisible(true);
		}
		pxCancel->SetFocusable(true);
	}

	// Ensure the canvas focus sits somewhere this screen owns (freshly opened, or returned
	// from a screen that cleared it), otherwise MIRROR the engine-navigated focus.
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
	const int iFocusedRow = RowIndexFromElementName(szFocusedName);
	if (iFocusedRow >= 0)
	{
		m_iSelectedRow = iFocusedRow;
	}
	else if (IsCancelElementName(szFocusedName))
	{
		m_iSelectedRow = -1;   // the Back button is not a row (GetSelectedRow's contract)
	}
	else
	{
		// Resolve FIRST, then mirror what actually happened (the ZM_UI_Bag idiom): claiming
		// a row that did not resolve would report a focused entry nothing is drawing.
		Zenith_UI::Zenith_UIElement* pxFirstRow = pxUI->FindElement(RowElementName(0u));
		xCanvas.SetFocusedElement(pxFirstRow);
		m_iSelectedRow = (pxFirstRow != nullptr) ? 0 : -1;
	}
}

void ZM_UI_SaveSlots::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no focused row, so the selection mirror must say so.
	// Unconditional: the widgets may fail to resolve, but the screen is not presented
	// either way.
	m_iSelectedRow = -1;

	Zenith_UIComponent* pxUI = ZM_ResolveSaveUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	// Hide runs EVERY frame the save screen is not the top screen, so the visibility
	// writes are change-guarded (SetVisible notifies the parent).
	const char* const aszSimpleHidden[2] = { szPANEL_NAME, szHEADER_NAME };
	for (const char* szName : aszSimpleHidden)
	{
		if (Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szName))
		{
			if (pxElement->IsVisible())
			{
				pxElement->SetVisible(false);
			}
		}
	}
	for (u_int u = 0u; u < uROW_COUNT; ++u)
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
	if (Zenith_UI::Zenith_UIElement* pxCancel = pxUI->FindElement(szCANCEL_NAME))
	{
		if (pxCancel->IsVisible())
		{
			pxCancel->SetVisible(false);
		}
		pxCancel->SetFocusable(false);
	}
}
