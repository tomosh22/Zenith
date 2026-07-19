#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_Party.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"    // ZM_GetAbilityName
#include "Zenithmon/Source/Data/ZM_MoveData.h"       // ZM_GetMoveName / ZM_MOVE_NONE
#include "Zenithmon/Source/Data/ZM_NatureData.h"     // ZM_GetNatureName
#include "Zenithmon/Source/Party/ZM_Monster.h"       // ZM_Monster + ZM_MoveSlot + uZM_MAX_MOVES
#include "Zenithmon/Source/Party/ZM_Party.h"         // ZM_Party + uZM_MAX_PARTY_SIZE
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"     // FormatHpPanel (the ONE HP-row formatter)

#include <cstring>

// ============================================================================
// ZM_UI_Party (S6 item 2 SC4). The party list + summary model, plus the
// best-effort presentation onto the persistent ZM_MenuRoot canvas. The row string
// is NOT re-derived here -- both the list row and the summary header call
// ZM_UI_BattleHUD::FormatHpPanel, the same formatter the battle HP panels use.
// ============================================================================

// The screen is authored with exactly one slot widget per party slot; a divergence
// would silently hide members (or focus a slot the party can never fill).
static_assert(ZM_UI_Party::uMAX_SLOTS == uZM_MAX_PARTY_SIZE,
	"ZM_UI_Party::uMAX_SLOTS must match the party capacity");

namespace
{
	// The persistent record's major status as a player-facing label. There is no
	// ZM_Get*Name accessor for ZM_MAJOR_STATUS anywhere in the data tables (unlike
	// species / nature / ability / move), so the mapping lives here, next to its only
	// consumer, rather than becoming a new table-layer surface for one caller.
	const char* ZM_PartyStatusLabel(ZM_MAJOR_STATUS eStatus)
	{
		switch (eStatus)
		{
		case ZM_MAJOR_STATUS_SLEEP:     return "Asleep";
		case ZM_MAJOR_STATUS_POISON:    return "Poisoned";
		case ZM_MAJOR_STATUS_TOXIC:     return "Badly Poisoned";
		case ZM_MAJOR_STATUS_BURN:      return "Burned";
		case ZM_MAJOR_STATUS_PARALYSIS: return "Paralyzed";
		case ZM_MAJOR_STATUS_FREEZE:    return "Frozen";
		// NONE and any out-of-range value read as healthy -- never a crash, never "".
		default:                        return "OK";
		}
	}

	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolvePartyUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_Party::SlotElementName(u_int uSlot)
{
	// A switch over string LITERALS (never a built std::string): the authoring steps
	// call this at bake time and the returned pointer outlives every caller.
	switch (uSlot)
	{
	case 0u: return "Menu_PartySlot0";
	case 1u: return "Menu_PartySlot1";
	case 2u: return "Menu_PartySlot2";
	case 3u: return "Menu_PartySlot3";
	case 4u: return "Menu_PartySlot4";
	case 5u: return "Menu_PartySlot5";
	default: return "";
	}
}

int ZM_UI_Party::SlotIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uMAX_SLOTS; ++u)
	{
		if (std::strcmp(szElementName, SlotElementName(u)) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

u_int ZM_UI_Party::VisibleSlotCount(u_int uPartyCount)
{
	return (uPartyCount < uMAX_SLOTS) ? uPartyCount : uMAX_SLOTS;
}

std::string ZM_UI_Party::FormatPartyRow(ZM_SPECIES_ID eSpecies, u_int uLevel,
	u_int uCurHp, u_int uMaxHp)
{
	std::string strRow = ZM_UI_BattleHUD::FormatHpPanel(eSpecies, uLevel, uCurHp, uMaxHp);
	if (uCurHp == 0u)
	{
		strRow += "  FAINTED";
	}
	return strRow;
}

std::string ZM_UI_Party::FormatSummary(const ZM_Monster& xRecord)
{
	std::string strBody = FormatPartyRow(
		xRecord.m_eSpecies, xRecord.m_uLevel, xRecord.m_uCurrentHp, xRecord.GetMaxHP());
	strBody += "\nNature: ";
	strBody += ZM_GetNatureName(xRecord.m_eNature);
	strBody += "\nAbility: ";
	strBody += ZM_GetAbilityName(xRecord.m_eAbility);
	strBody += "\nStatus: ";
	strBody += ZM_PartyStatusLabel(xRecord.m_eStatus);

	// One line per NON-EMPTY slot: the moveset is GAPPED (an empty slot can sit before
	// a filled one), so this skips rather than stopping at the first hole.
	for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
	{
		const ZM_MoveSlot& xSlot = xRecord.m_axMoves[u];
		if (xSlot.m_eMove == ZM_MOVE_NONE)
		{
			continue;
		}
		strBody += "\n";
		strBody += ZM_GetMoveName(xSlot.m_eMove);
		strBody += "  PP ";
		strBody += std::to_string(xSlot.m_uCurPP);
		strBody += "/";
		strBody += std::to_string(xSlot.m_uMaxPP);
	}
	return strBody;
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_Party::Reset()
{
	m_iCursor = 0;
	m_bSummaryOpen = false;
}

void ZM_UI_Party::Confirm()
{
	if (m_iCursor < 0)
	{
		// No member sits under the cursor (an empty party, or the screen is not
		// presented at all): opening the summary here would raise a flag that Present
		// suppresses, so the player would see nothing change and the NEXT cancel would
		// be silently eaten by Cancel() closing an invisible summary.
		return;
	}
	m_bSummaryOpen = !m_bSummaryOpen;
}

bool ZM_UI_Party::Cancel()
{
	if (m_bSummaryOpen)
	{
		m_bSummaryOpen = false;
		return true;   // consumed: the first Escape only closes the summary
	}
	return false;      // the menu stack pops the party screen
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_Party::Present(Zenith_Entity& xRootEntity, const ZM_Party& xParty)
{
	// The "no slot to sit on" verdict comes from the MODEL (the party count), so it is
	// settled BEFORE any UI resolve -- otherwise a headless present would leave the
	// cursor claiming slot 0 over an empty party and Confirm's guard could never fire.
	const u_int uVisible = VisibleSlotCount(xParty.Count());
	if (uVisible == 0u)
	{
		m_iCursor = -1;
	}

	Zenith_UIComponent* pxUI = ZM_ResolvePartyUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the screen
	}
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();

	// Re-resolve by NAME every frame (never cache across frames).
	if (Zenith_UI::Zenith_UIRect* pxPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		pxPanel->SetVisible(true);
	}

	for (u_int u = 0u; u < uMAX_SLOTS; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxSlot =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(SlotElementName(u));
		if (pxSlot == nullptr)
		{
			continue;
		}
		const bool bFilled = (u < uVisible);
		pxSlot->SetVisible(bFilled);
		pxSlot->SetFocusable(bFilled);   // nav collects only visible + focusable elements
		if (!bFilled)
		{
			continue;   // ZM_Party::Get asserts past the count -- never read an empty slot
		}
		const ZM_Monster& xRecord = xParty.Get(u);
		const std::string strRow = FormatPartyRow(
			xRecord.m_eSpecies, xRecord.m_uLevel, xRecord.m_uCurrentHp, xRecord.GetMaxHP());
		if (pxSlot->GetText() != strRow)
		{
			pxSlot->SetText(strRow);
		}
	}

	// Ensure the canvas focus sits on a VISIBLE slot (freshly opened, or returned from
	// a screen that cleared it), otherwise MIRROR the engine-navigated focus.
	if (uVisible == 0u)
	{
		xCanvas.SetFocusedElement(nullptr);   // an empty party has nothing to focus (cursor already -1)
	}
	else
	{
		Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
		const int iFocused = (pxFocused != nullptr)
			? SlotIndexFromElementName(pxFocused->GetName().c_str())
			: -1;
		if (iFocused < 0 || static_cast<u_int>(iFocused) >= uVisible)
		{
			// Resolve FIRST: if slot 0 is missing (an unbaked / stale scene) the focus is
			// cleared, so claiming cursor 0 would report a focused member that does not
			// exist -- and the windowed test asserts on the cursor, so it would pass in
			// exactly the case it exists to catch. Mirror the real outcome instead.
			Zenith_UI::Zenith_UIElement* pxFirstSlot = pxUI->FindElement(SlotElementName(0u));
			xCanvas.SetFocusedElement(pxFirstSlot);
			m_iCursor = (pxFirstSlot != nullptr) ? 0 : -1;
		}
		else
		{
			m_iCursor = iFocused;
		}
	}

	// The summary shows only while open AND over a real member (an empty party or a
	// cleared cursor leaves it down rather than indexing past the count).
	const bool bSummary = m_bSummaryOpen
		&& m_iCursor >= 0
		&& static_cast<u_int>(m_iCursor) < xParty.Count();
	if (Zenith_UI::Zenith_UIRect* pxSummaryPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szSUMMARY_PANEL_NAME))
	{
		pxSummaryPanel->SetVisible(bSummary);
	}
	if (Zenith_UI::Zenith_UIText* pxSummaryText =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szSUMMARY_TEXT_NAME))
	{
		pxSummaryText->SetVisible(bSummary);
		if (bSummary)
		{
			const std::string strSummary = FormatSummary(xParty.Get(static_cast<u_int>(m_iCursor)));
			// SetText rebuilds the word wrap, so only write it when it actually changed.
			if (pxSummaryText->GetText() != strSummary)
			{
				pxSummaryText->SetText(strSummary);
			}
		}
	}
}

void ZM_UI_Party::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no focused slot, so the cursor mirror must say so (GetCursor
	// contracts -1 while not presented). This is unconditional -- the widgets may fail
	// to resolve, but the screen is not presented either way, and a stale non-negative
	// cursor would let Confirm open a summary over a member nothing is showing.
	m_iCursor = -1;

	Zenith_UIComponent* pxUI = ZM_ResolvePartyUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	if (Zenith_UI::Zenith_UIElement* pxPanel = pxUI->FindElement(szPANEL_NAME))
	{
		pxPanel->SetVisible(false);
	}
	for (u_int u = 0u; u < uMAX_SLOTS; ++u)
	{
		if (Zenith_UI::Zenith_UIElement* pxSlot = pxUI->FindElement(SlotElementName(u)))
		{
			pxSlot->SetVisible(false);
			pxSlot->SetFocusable(false);   // a hidden slot must never stay nav-reachable
		}
	}
	if (Zenith_UI::Zenith_UIElement* pxSummaryPanel = pxUI->FindElement(szSUMMARY_PANEL_NAME))
	{
		pxSummaryPanel->SetVisible(false);
	}
	if (Zenith_UI::Zenith_UIElement* pxSummaryText = pxUI->FindElement(szSUMMARY_TEXT_NAME))
	{
		pxSummaryText->SetVisible(false);
	}
}
