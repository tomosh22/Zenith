#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_GetSpeciesName (the RUNTIME label source)

#include <cstdio>
#include <cstring>

// ============================================================================
// ZM_UI_StarterChoice (S8). The PURE cell/name/label policy plus the best-effort
// presentation onto the persistent ZM_MenuRoot canvas. Every widget is authored at
// bake time by ZM_ConfigureMenuRootStarterScreen (see the header), so there is no
// runtime construction here -- Present only re-resolves by name, writes labels, and
// toggles visibility / focus.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON A CHOICE. Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration, and the boot units feed the sentinel
// and out-of-range choices on purpose (that is the whole point of a TOTAL surface),
// so an assert here would not catch a bug -- it would end the unit run at boot and
// take the gate down with it. This is the same ruling ZM_StarterChoice.cpp states.
// ============================================================================

static_assert(ZM_UI_StarterChoice::uCELL_COUNT == static_cast<u_int>(ZM_STARTER_CHOICE_COUNT),
	"one cell per starter (StarterScreen_CellCountEqualsStarterChoiceCount)");

namespace
{
	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolveStarterUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_StarterChoice::CellElementName(u_int uCell)
{
	// A table of string LITERALS (never a built std::string): the returned pointer
	// outlives every caller, so even bake-time authoring may call this.
	//
	// The bound is DEDUCED, never spelled (the s_axStarterChoices idiom): with an
	// explicit [uCELL_COUNT] a fourth starter would zero-initialise the tail and hand
	// every caller a NULL name. Deduced, it is a COMPILE error here instead.
	static const char* const aszCELL_NAMES[] =
	{
		"Menu_StarterCell0", "Menu_StarterCell1", "Menu_StarterCell2",
	};
	static_assert(sizeof(aszCELL_NAMES) / sizeof(aszCELL_NAMES[0]) == uCELL_COUNT,
		"one authored cell name per starter -- add the name when the table grows");
	return (uCell < uCELL_COUNT) ? aszCELL_NAMES[uCell] : "";
}

int ZM_UI_StarterChoice::CellIndexFromElementName(const char* szName)
{
	if (szName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		// EXACT compare, never a prefix match: "Menu_StarterCell0Extra" is a foreign name,
		// and resolving it to cell 0 would grant a starter nobody picked.
		if (std::strcmp(szName, CellElementName(u)) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

ZM_STARTER_CHOICE ZM_UI_StarterChoice::CellChoice(u_int uCell)
{
	// The cells are authored in TABLE ORDER, so cell N is choice N. The cast is safe
	// precisely because of the bound: ZM_STARTER_CHOICE is dense 0..COUNT-1 and
	// ZM_STARTER_CHOICE_NONE aliases the count, which is what an out-of-range cell gets.
	return (uCell < uCELL_COUNT)
		? static_cast<ZM_STARTER_CHOICE>(uCell)
		: ZM_STARTER_CHOICE_NONE;
}

ZM_STARTER_CHOICE ZM_UI_StarterChoice::ChoiceFromElementName(const char* szName)
{
	const int iCell = CellIndexFromElementName(szName);
	// The panel, the header, a foreign name and null all land here. NONE is what
	// ZM_IsRegisteredStarterChoice rejects, so a stray confirm grants nothing.
	return (iCell < 0)
		? ZM_STARTER_CHOICE_NONE
		: CellChoice(static_cast<u_int>(iCell));
}

void ZM_UI_StarterChoice::FormatCellLabel(ZM_STARTER_CHOICE eChoice, char* pszOut, u_int uCapacity)
{
	if (pszOut == nullptr || uCapacity == 0u)
	{
		return;
	}
	// EMPTY FIRST, so every early return below leaves a null-terminated buffer.
	pszOut[0] = '\0';

	// ★ THE GUARD THAT KEEPS A SENTINEL OUT OF THE TABLES, for three reasons and not
	// just the obvious one:
	//   1. ZM_GetSpeciesName IS total today -- it answers the literal "NONE" out of range
	//      -- so WITHOUT this guard the cell would read "NONE" to the player rather than
	//      being blank. Its neighbours (ZM_GetSpeciesData and everything built on it) are
	//      bounds-ASSERTED, and Zenith_Assert calls Zenith_DebugBreak() in EVERY config,
	//      so anything that later reaches for a type or a sprite off this path would end
	//      the boot-unit run rather than fail one assertion. Fail closed at the boundary.
	//   2. ZM_GetStarterChoice logs a non-fatal Zenith_Error for an unregistered choice,
	//      and this runs EVERY FRAME the screen is up -- a per-frame error spew.
	//   3. It is what StarterScreen_FormatCellLabelIsEmptyForAnUnregisteredChoice pins,
	//      using the "NONE" string above as the observable proof the lookup never happened.
	if (!ZM_IsRegisteredStarterChoice(eChoice))
	{
		return;
	}

	// The SHIPPED resolver, not a re-read of the table. FAIL CLOSED TWICE (the
	// ZM_ResolveCounterStarterSpecies idiom): a registered row whose species column was
	// mis-authored to the sentinel stops here too, rather than being handed on.
	const ZM_SPECIES_ID eSpecies = ZM_ResolvePlayerStarterSpecies(eChoice);
	if (eSpecies == ZM_SPECIES_NONE)
	{
		return;
	}

	// snprintf always writes at most uCapacity-1 chars plus a terminator, so this never
	// overruns even the tiny capacities the unit tests pass.
	std::snprintf(pszOut, static_cast<size_t>(uCapacity), "%s", ZM_GetSpeciesName(eSpecies));
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_StarterChoice::Reset()
{
	m_iSelectedCell = -1;
}

void ZM_UI_StarterChoice::SelectCell(int iCell)
{
	// TOTAL: the -1 a failed name lookup yields, and any out-of-range cell, clear the
	// mirror rather than storing a selection nothing is drawing.
	m_iSelectedCell = (iCell >= 0 && static_cast<u_int>(iCell) < uCELL_COUNT) ? iCell : -1;
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_StarterChoice::Present(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = ZM_ResolveStarterUI(xRootEntity);
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

	// Header. Authored EMPTY and written here, like every other label on this screen.
	if (Zenith_UI::Zenith_UIText* pxHeader =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szHEADER_NAME))
	{
		if (!pxHeader->IsVisible())
		{
			pxHeader->SetVisible(true);
		}
		if (pxHeader->GetText() != szHEADER_TEXT)
		{
			pxHeader->SetText(szHEADER_TEXT);
		}
	}

	// The cells. Every one stays VISIBLE + FOCUSABLE (CellIsAlwaysShown): all three are
	// always confirmable, so there is nothing to disarm, and the authored nav links point
	// only at cells that are never hidden.
	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxCell =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(CellElementName(u));
		if (pxCell == nullptr)
		{
			continue;
		}
		if (!pxCell->IsVisible())
		{
			pxCell->SetVisible(true);
		}
		pxCell->SetFocusable(true);
		// The SPECIES NAME, written every frame from the compiled table -- which is why the
		// authored scene bytes carry an EMPTY label and no content.
		char acLabel[uLABEL_CAPACITY];
		FormatCellLabel(CellChoice(u), acLabel, uLABEL_CAPACITY);
		if (pxCell->GetText() != acLabel)
		{
			pxCell->SetText(acLabel);
		}
	}

	// Ensure the canvas focus sits on a cell (freshly opened, or returned from a screen
	// that cleared it), otherwise MIRROR the engine-navigated focus.
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
	const int iFocusedCell = CellIndexFromElementName(szFocusedName);
	if (iFocusedCell >= 0)
	{
		SelectCell(iFocusedCell);
	}
	else
	{
		// Resolve FIRST, then mirror what actually happened (the ZM_UI_Bag / ZM_UI_SaveSlots
		// idiom): claiming a cell that did not resolve would report a focused entry nothing
		// is drawing. SelectCell(-1) is the honest answer when the widget is missing.
		Zenith_UI::Zenith_UIElement* pxFirstCell = pxUI->FindElement(CellElementName(0u));
		xCanvas.SetFocusedElement(pxFirstCell);
		SelectCell((pxFirstCell != nullptr) ? 0 : -1);
	}
}

void ZM_UI_StarterChoice::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no focused cell, so the mirror must say so. Unconditional: the
	// widgets may fail to resolve, but the screen is not presented either way.
	m_iSelectedCell = -1;

	Zenith_UIComponent* pxUI = ZM_ResolveStarterUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	// Hide runs EVERY frame the starter screen is not the top screen, so the visibility
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
	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		if (Zenith_UI::Zenith_UIElement* pxCell = pxUI->FindElement(CellElementName(u)))
		{
			if (pxCell->IsVisible())
			{
				pxCell->SetVisible(false);
			}
			pxCell->SetFocusable(false);   // a hidden cell must never stay nav-reachable
		}
	}
}
