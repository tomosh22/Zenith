#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_Dex.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIGridLayoutGroup.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"     // iMENU_*_SORT_ORDER (the shared menu sort band)
#include "Zenithmon/Source/Party/ZM_GameState.h"      // ZM_SpeciesSet

#include <cstring>

// ============================================================================
// ZM_UI_Dex (S6 item 2 SC5). The paged dex model plus the best-effort presentation
// onto the persistent ZM_MenuRoot canvas, and the ONE-TIME construction of the
// Zenith_UIGridLayoutGroup + its 30 cells.
//
// OWNERSHIP (the load-bearing part -- getting it wrong leaks or double-renders):
//   * Zenith_UICanvas::AddElement pushes into BOTH m_xAllElements and
//     m_xRootElements; Clear()/the dtor delete m_xAllElements ONLY.
//   * Zenith_UIElement::AddChild does NOT add to m_xAllElements, so an element that
//     is only AddChild'd is never deleted -- a LEAK -- and an AddElement'd-then-
//     AddChild'd element stays in m_xRootElements too, so Update/Render would walk
//     it TWICE.
//   * Zenith_UICanvas::ReparentElement is the correct path: it erases the child from
//     m_xRootElements and AddChild's it while LEAVING it in m_xAllElements (still
//     owned). That is why every cell goes CreateButton -> ReparentElement.
// ============================================================================

// Cell 0 of any in-range page maps to page * uCELL_COUNT < the dex size, so Present's
// "focus cell 0" fallback can never land on a dead cell. An empty dex would break that.
static_assert(ZM_SPECIES_COUNT > 0u, "the dex must hold at least one species");

namespace
{
	// The ZM_MenuRoot entity's UI component, or null (best-effort presentation).
	Zenith_UIComponent* ZM_ResolveDexUI(Zenith_Entity& xRootEntity)
	{
		return xRootEntity.IsValid()
			? xRootEntity.TryGetComponent<Zenith_UIComponent>()
			: nullptr;
	}

	// Build the grid + its 30 cells ONCE. Guarded on the grid resolving by name, so a
	// second call is a no-op: this runs on the PERSISTENT ZM_MenuRoot canvas, which
	// survives every scene load, so the whole routine executes once per process.
	// There is deliberately no Zenith_UIComponent::CreateGridLayoutGroup /
	// AddStep_CreateUIGridLayoutGroup to author this at bake time -- adding one is an
	// ENGINE change (cross-game regression), which SC5 is not.
	void ZM_EnsureDexGridBuilt(Zenith_UIComponent& xUI)
	{
		if (xUI.FindElement(ZM_UI_Dex::szGRID_NAME) != nullptr)
		{
			return;
		}

		Zenith_UI::Zenith_UIGridLayoutGroup* pxGrid =
			new Zenith_UI::Zenith_UIGridLayoutGroup(ZM_UI_Dex::szGRID_NAME);
		xUI.AddElement(pxGrid);   // the CANVAS now OWNS it -- never delete it here

		pxGrid->SetColumns(ZM_UI_Dex::uCOLUMNS);
		pxGrid->SetCellSize(ZM_UI_Dex::fCELL_WIDTH, ZM_UI_Dex::fCELL_HEIGHT);
		pxGrid->SetSpacing(ZM_UI_Dex::fCELL_SPACING_X, ZM_UI_Dex::fCELL_SPACING_Y);
		pxGrid->SetPadding(0.0f, 0.0f, 0.0f, 0.0f);
		// FIXED box (see fGRID_WIDTH): an auto-sized grid would re-centre itself when the
		// trailing page hides cells.
		pxGrid->SetFitToContent(false);
		pxGrid->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
		pxGrid->SetAnchor(Zenith_UI::AnchorPreset::Center);
		pxGrid->SetPivot(Zenith_UI::AnchorPreset::Center);
		pxGrid->SetPosition(0.0f, ZM_UI_Dex::fGRID_CENTRE_Y);
		pxGrid->SetSize(ZM_UI_Dex::fGRID_WIDTH, ZM_UI_Dex::fGRID_HEIGHT);
		pxGrid->SetVisible(false);   // Present raises it; the screen starts hidden

		for (u_int u = 0u; u < ZM_UI_Dex::uCELL_COUNT; ++u)
		{
			// NOTE the argument order: CreateButton takes (NAME, TEXT) while the
			// Zenith_UIButton ctor it wraps takes (TEXT, NAME).
			Zenith_UI::Zenith_UIButton* pxCell =
				xUI.CreateButton(ZM_UI_Dex::CellElementName(u), "");
			// Un-root + parent + STILL owned by the canvas (see the ownership block above).
			xUI.GetCanvas().ReparentElement(pxCell, pxGrid);

			pxCell->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxCell->SetFontSize(16.0f);
			// The grid owns the cell's size and position (row-major, local to the grid's
			// top-left) -- setting either here would just be overwritten on the next solve.
			pxCell->SetFocusable(false);
			pxCell->SetVisible(false);
		}
	}
}

// ---- PURE statics -----------------------------------------------------------

const char* ZM_UI_Dex::CellElementName(u_int uCell)
{
	// A table of string LITERALS (never a built std::string): the returned pointer
	// outlives every caller, so even bake-time authoring may call this.
	static const char* const aszCELL_NAMES[uCELL_COUNT] =
	{
		"Menu_DexCell0",  "Menu_DexCell1",  "Menu_DexCell2",  "Menu_DexCell3",  "Menu_DexCell4",
		"Menu_DexCell5",  "Menu_DexCell6",  "Menu_DexCell7",  "Menu_DexCell8",  "Menu_DexCell9",
		"Menu_DexCell10", "Menu_DexCell11", "Menu_DexCell12", "Menu_DexCell13", "Menu_DexCell14",
		"Menu_DexCell15", "Menu_DexCell16", "Menu_DexCell17", "Menu_DexCell18", "Menu_DexCell19",
		"Menu_DexCell20", "Menu_DexCell21", "Menu_DexCell22", "Menu_DexCell23", "Menu_DexCell24",
		"Menu_DexCell25", "Menu_DexCell26", "Menu_DexCell27", "Menu_DexCell28", "Menu_DexCell29",
	};
	return (uCell < uCELL_COUNT) ? aszCELL_NAMES[uCell] : "";
}

int ZM_UI_Dex::CellIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		if (std::strcmp(szElementName, CellElementName(u)) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

u_int ZM_UI_Dex::PageCount(u_int uSpeciesCount)
{
	if (uSpeciesCount == 0u)
	{
		return 1u;   // an empty dex still shows one blank page (never 0 -- ClampPage divides by this)
	}
	return (uSpeciesCount + uCELL_COUNT - 1u) / uCELL_COUNT;
}

int ZM_UI_Dex::ClampPage(int iPage, u_int uSpeciesCount)
{
	if (iPage < 0)
	{
		return 0;
	}
	const int iLast = static_cast<int>(PageCount(uSpeciesCount)) - 1;
	return (iPage > iLast) ? iLast : iPage;
}

int ZM_UI_Dex::SpeciesIndexForCell(u_int uPage, u_int uCell, u_int uSpeciesCount)
{
	if (uCell >= uCELL_COUNT || uPage >= PageCount(uSpeciesCount))
	{
		return -1;
	}
	// uPage < PageCount, so uPage * uCELL_COUNT <= uSpeciesCount + uCELL_COUNT - 1 --
	// the multiply cannot overflow for any count a u_int can hold a page count for.
	const u_int uAbsolute = uPage * uCELL_COUNT + uCell;
	return (uAbsolute < uSpeciesCount) ? static_cast<int>(uAbsolute) : -1;
}

u_int ZM_UI_Dex::VisibleCellCount(u_int uPage, u_int uSpeciesCount)
{
	if (uPage >= PageCount(uSpeciesCount))
	{
		return 0u;
	}
	const u_int uFirst = uPage * uCELL_COUNT;
	const u_int uRemaining = (uFirst < uSpeciesCount) ? (uSpeciesCount - uFirst) : 0u;
	return (uRemaining < uCELL_COUNT) ? uRemaining : uCELL_COUNT;
}

std::string ZM_UI_Dex::FormatCell(ZM_SPECIES_ID eSpecies, bool bCaught)
{
	const u_int uNumber = static_cast<u_int>(eSpecies) + 1u;   // dex numbers are 1-based

	std::string strLabel = "#";
	if (uNumber < 100u) { strLabel += "0"; }
	if (uNumber < 10u)  { strLabel += "0"; }
	strLabel += std::to_string(uNumber);
	strLabel += " ";

	// The NAME is the reward for catching it; an unseen entry shows only its number.
	// The range guard keeps a stray id off ZM_GetSpeciesName's bounds assert.
	const bool bNameShown = bCaught && static_cast<u_int>(eSpecies) < static_cast<u_int>(ZM_SPECIES_COUNT);
	strLabel += bNameShown ? ZM_GetSpeciesName(eSpecies) : "-----";
	return strLabel;
}

std::string ZM_UI_Dex::FormatCompletion(u_int uCaught, u_int uTotal)
{
	std::string strHeader = "Dex ";
	strHeader += std::to_string(uCaught);
	strHeader += "/";
	strHeader += std::to_string(uTotal);
	return strHeader;
}

// ---- Instance drive ---------------------------------------------------------

void ZM_UI_Dex::Reset()
{
	m_iPage = 0;
	m_iCursor = 0;
}

bool ZM_UI_Dex::Confirm(const char* szFocusedElementName, u_int uSpeciesCount)
{
	if (szFocusedElementName == nullptr)
	{
		return false;
	}

	int iWanted = m_iPage;
	if (std::strcmp(szFocusedElementName, szNEXT_NAME) == 0)
	{
		iWanted = m_iPage + 1;
	}
	else if (std::strcmp(szFocusedElementName, szPREV_NAME) == 0)
	{
		iWanted = m_iPage - 1;
	}
	else
	{
		// A CELL (or any other element) is INERT: the per-species detail panel is out of
		// scope for SC5, and a silent no-op is what a not-yet-implemented entry needs.
		return false;
	}

	const int iClamped = ClampPage(iWanted, uSpeciesCount);
	if (iClamped == m_iPage)
	{
		return false;   // already at the end the player pressed towards
	}
	m_iPage = iClamped;
	// m_iCursor is a pure MIRROR of the canvas focus (written by Present / Hide) -- the
	// focus is still parked on the page button that was just confirmed, so it is not
	// touched here.
	return true;
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_Dex::Present(Zenith_Entity& xRootEntity, const ZM_SpeciesSet& xCaught)
{
	// Settled from the MODEL before any UI resolve: the dex size is a compile-time
	// constant, but clamping every frame keeps the page honest if a Reset was missed.
	m_iPage = ClampPage(m_iPage, static_cast<u_int>(ZM_SPECIES_COUNT));
	const u_int uPage = static_cast<u_int>(m_iPage);

	Zenith_UIComponent* pxUI = ZM_ResolveDexUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the screen
	}
	ZM_EnsureDexGridBuilt(*pxUI);
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();

	// Re-resolve by NAME every frame (never cache across frames -- FindElement walks the
	// whole hierarchy, so the cells parented under the grid are still findable).
	if (Zenith_UI::Zenith_UIRect* pxPanel =
		pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		pxPanel->SetVisible(true);
	}
	if (Zenith_UI::Zenith_UIElement* pxGrid = pxUI->FindElement(szGRID_NAME))
	{
		// The grid must be visible for the engine's focus collection to descend into its
		// cells at all (CollectFocusableElements stops at a hidden ancestor).
		pxGrid->SetVisible(true);
	}
	const char* const aszPageButtons[2] = { szPREV_NAME, szNEXT_NAME };
	for (const char* szPageButton : aszPageButtons)
	{
		if (Zenith_UI::Zenith_UIElement* pxButton = pxUI->FindElement(szPageButton))
		{
			pxButton->SetVisible(true);
			pxButton->SetFocusable(true);
		}
	}

	if (Zenith_UI::Zenith_UIText* pxHeader =
		pxUI->FindElement<Zenith_UI::Zenith_UIText>(szHEADER_NAME))
	{
		pxHeader->SetVisible(true);
		const std::string strHeader =
			FormatCompletion(xCaught.Count(), static_cast<u_int>(ZM_SPECIES_COUNT));
		// SetText rebuilds the word wrap, so only write it when it actually changed.
		if (pxHeader->GetText() != strHeader)
		{
			pxHeader->SetText(strHeader);
		}
	}

	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		Zenith_UI::Zenith_UIButton* pxCell =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>(CellElementName(u));
		if (pxCell == nullptr)
		{
			continue;
		}
		const int iSpecies = SpeciesIndexForCell(uPage, u, static_cast<u_int>(ZM_SPECIES_COUNT));
		const bool bLive = (iSpecies >= 0);
		// A dead trailing cell must be BOTH hidden and non-focusable: the engine's nav
		// collects visible + focusable elements, so leaving it focusable would let the
		// arrows park on a blank entry (the exact watch-out SC4 paid for). Hiding it is
		// also what keeps the row-major layout correct -- the grid places VISIBLE
		// children only, so a hidden cell leaves no gap.
		//
		// Only write the visibility when it actually CHANGES: SetVisible unconditionally
		// notifies the parent, and the grid re-dirties its layout on that notify, so an
		// unconditional write would re-solve all 30 cells every single frame and defeat the
		// widget's own convergence guard. SetFocusable is a plain assignment (no notify),
		// so it needs no such guard.
		if (pxCell->IsVisible() != bLive)
		{
			pxCell->SetVisible(bLive);
		}
		pxCell->SetFocusable(bLive);
		if (!bLive)
		{
			continue;
		}
		const ZM_SPECIES_ID eSpecies = static_cast<ZM_SPECIES_ID>(iSpecies);
		const std::string strLabel = FormatCell(eSpecies, xCaught.IsSet(eSpecies));
		if (pxCell->GetText() != strLabel)
		{
			pxCell->SetText(strLabel);
		}
	}

	// Ensure the canvas focus sits somewhere this screen owns (freshly opened, or
	// returned from a screen that cleared it), otherwise MIRROR the engine-navigated
	// focus. Cell 0 of an in-range page is always live (see the static_assert above).
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
	const int iFocusedCell = CellIndexFromElementName(szFocusedName);
	const bool bOnLiveCell = iFocusedCell >= 0
		&& SpeciesIndexForCell(uPage, static_cast<u_int>(iFocusedCell),
			static_cast<u_int>(ZM_SPECIES_COUNT)) >= 0;
	const bool bOnPageButton = szFocusedName != nullptr
		&& (std::strcmp(szFocusedName, szPREV_NAME) == 0
			|| std::strcmp(szFocusedName, szNEXT_NAME) == 0);

	if (bOnLiveCell)
	{
		m_iCursor = iFocusedCell;
	}
	else if (bOnPageButton)
	{
		m_iCursor = -1;   // a page button is not a cell (GetCursor's contract)
	}
	else
	{
		// Resolve FIRST, then mirror what actually happened: when cell 0 does not resolve
		// the focus is CLEARED, and claiming cursor 0 anyway would report a focused entry
		// that nothing is drawing -- a lie ZM_UI_MenuStack::GetCursor carries straight up.
		Zenith_UI::Zenith_UIElement* pxFirstCell = pxUI->FindElement(CellElementName(0u));
		xCanvas.SetFocusedElement(pxFirstCell);
		m_iCursor = (pxFirstCell != nullptr) ? 0 : -1;
	}
}

void ZM_UI_Dex::Hide(Zenith_Entity& xRootEntity)
{
	// A hidden screen owns no focused cell, so the cursor mirror must say so
	// (GetCursor contracts -1 while not presented). Unconditional: the widgets may fail
	// to resolve, but the screen is not presented either way.
	m_iCursor = -1;

	Zenith_UIComponent* pxUI = ZM_ResolveDexUI(xRootEntity);
	if (pxUI == nullptr)
	{
		return;
	}
	const char* const aszHiddenElements[3] = { szPANEL_NAME, szGRID_NAME, szHEADER_NAME };
	for (const char* szName : aszHiddenElements)
	{
		if (Zenith_UI::Zenith_UIElement* pxElement = pxUI->FindElement(szName))
		{
			pxElement->SetVisible(false);
		}
	}
	const char* const aszPageButtons[2] = { szPREV_NAME, szNEXT_NAME };
	for (const char* szPageButton : aszPageButtons)
	{
		if (Zenith_UI::Zenith_UIElement* pxButton = pxUI->FindElement(szPageButton))
		{
			pxButton->SetVisible(false);
			pxButton->SetFocusable(false);   // a hidden button must never stay nav-reachable
		}
	}
	// The cells are hidden explicitly too, not just by the grid going down: a hidden
	// ancestor stops the focus collection, but the grid could be re-shown by a stale
	// path and must never bring a focusable blank cell back with it.
	// Hide runs EVERY frame the dex is not the top screen, so the visibility write is
	// change-guarded for the same reason it is in Present: SetVisible notifies the grid,
	// which re-dirties its layout, and an already-hidden cell must not keep it re-solving.
	for (u_int u = 0u; u < uCELL_COUNT; ++u)
	{
		if (Zenith_UI::Zenith_UIElement* pxCell = pxUI->FindElement(CellElementName(u)))
		{
			if (pxCell->IsVisible())
			{
				pxCell->SetVisible(false);
			}
			pxCell->SetFocusable(false);
		}
	}
}
