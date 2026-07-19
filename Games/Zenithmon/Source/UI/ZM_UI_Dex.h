#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID / ZM_SPECIES_COUNT (by value in the pure formatters)

#include <string>

class Zenith_Entity;
struct ZM_SpeciesSet;

// ============================================================================
// ZM_UI_Dex (S6 item 2 SC5) -- the overworld dex screen: a paged 5x6 grid of
// species entries, a completion header, and two page buttons. The FIRST consumer
// of the S6 item 1 engine widget Zenith_UIGridLayoutGroup (ZM-D-113).
//
// It is a small NON-ECS presentation class OWNED BY VALUE by ZM_UI_MenuStack (the
// ZM_UI_DialogueBox / ZM_UI_Party seam): NO order, NO component registration, NO
// editor mirror. Its panel / header / two page buttons are authored at bake time
// onto the persistent ZM_MenuRoot entity's Zenith_UIComponent
// (ZM_ConfigureMenuRoot); the GRID and its 30 cells are built ONCE AT RUNTIME by
// Present (there is deliberately no CreateGridLayoutGroup / AddStep_* engine
// surface -- adding one would be an engine change needing cross-game regression).
//
// The instance state is PODs only (a page + a cursor), so the owning component's
// defaulted noexcept move stays well-formed when the ECS pool relocates it.
//
// Traversal inside the grid is the ENGINE SPATIAL focus-navigation
// (Zenith_UICanvas::FindNearestFocusable) -- there is NO hand-rolled grid cursor
// arithmetic and NO SetNavigation link between cells; the spatial search is the
// whole point of using a grid. Present only ENSURES a focused live cell and
// MIRRORS the engine-navigated focus back into m_iCursor.
// ============================================================================
class ZM_UI_Dex
{
public:
	static constexpr u_int uCOLUMNS    = 5u;
	static constexpr u_int uROWS       = 6u;
	static constexpr u_int uCELL_COUNT = uCOLUMNS * uROWS;   // 30 entries per page

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution (never cache element pointers -- the canvas may relocate them).
	// szGRID_NAME + the cell names are NOT authored: the runtime build-once routine
	// creates them, and FindElement(szGRID_NAME) is that routine's idempotence guard.
	static constexpr const char* szPANEL_NAME  = "Menu_DexPanel";
	static constexpr const char* szGRID_NAME   = "Menu_DexGrid";
	static constexpr const char* szHEADER_NAME = "Menu_DexHeader";
	static constexpr const char* szPREV_NAME   = "Menu_DexPrevPage";
	static constexpr const char* szNEXT_NAME   = "Menu_DexNextPage";

	// ---- Geometry, shared by BOTH placement sites -------------------------------
	// The grid + cells are placed here (runtime build) and the panel / header / page
	// buttons in ZM_ConfigureMenuRoot, so the box these two agree on lives in ONE
	// place. All Y values are offsets from the panel's centre (origin top-left,
	// +Y DOWN), the panel itself being screen-centred.
	static constexpr float fCELL_WIDTH     = 176.0f;
	static constexpr float fCELL_HEIGHT    = 40.0f;
	static constexpr float fCELL_SPACING_X = 8.0f;
	static constexpr float fCELL_SPACING_Y = 6.0f;
	// The grid box is FIXED (fit-to-content OFF): the trailing partial page hides
	// cells, and an auto-sizing grid would re-centre itself when it does, sliding the
	// whole page under the player between pages.
	static constexpr float fGRID_WIDTH = static_cast<float>(uCOLUMNS) * fCELL_WIDTH
		+ static_cast<float>(uCOLUMNS - 1u) * fCELL_SPACING_X;   // 912
	static constexpr float fGRID_HEIGHT = static_cast<float>(uROWS) * fCELL_HEIGHT
		+ static_cast<float>(uROWS - 1u) * fCELL_SPACING_Y;      // 270
	static constexpr float fGRID_CENTRE_Y = 10.0f;
	// 960x400 clears the 912x270 grid with a margin, and leaves a header band above
	// it and a page-button band below it INSIDE the panel (nothing bleeds outside the
	// box it is drawn over -- the S5 visual-gate lesson, ZM-D-112).
	static constexpr float fPANEL_WIDTH  = 960.0f;
	static constexpr float fPANEL_HEIGHT = 400.0f;
	static constexpr float fHEADER_CENTRE_Y = -170.0f;
	static constexpr float fHEADER_WIDTH    = 900.0f;
	static constexpr float fHEADER_HEIGHT   = 40.0f;
	// Below the grid (its bottom edge sits at +145), so the engine's spatial nav walks
	// down off the last grid row onto them and back up again -- no explicit links.
	static constexpr float fPAGE_BUTTON_CENTRE_Y = 175.0f;
	static constexpr float fPAGE_BUTTON_CENTRE_X = 140.0f;
	static constexpr float fPAGE_BUTTON_WIDTH    = 200.0f;
	static constexpr float fPAGE_BUTTON_HEIGHT   = 40.0f;

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The element name for a grid cell ("Menu_DexCell0".."Menu_DexCell29"); "" for an
	// out-of-range cell. Returns string LITERALS (the RootItemElementName idiom) --
	// never allocates, never dangles.
	static const char* CellElementName(u_int uCell);
	// The cell index for an element name, or -1 when it is not a dex cell.
	static int CellIndexFromElementName(const char* szElementName);
	// Pages needed to show uSpeciesCount entries. ALWAYS >= 1: an empty dex still
	// shows one blank page rather than leaving the screen with no page to clamp to.
	static u_int PageCount(u_int uSpeciesCount);
	// Clamp a (possibly negative or past-the-end) page into [0, PageCount - 1].
	static int ClampPage(int iPage, u_int uSpeciesCount);
	// The absolute species index shown in cell uCell of page uPage, or -1 when that
	// cell is past the end of the dex (the trailing partial page) / out of range.
	static int SpeciesIndexForCell(u_int uPage, u_int uCell, u_int uSpeciesCount);
	// How many cells on uPage map to a real species: uCELL_COUNT except on the
	// trailing partial page (and 0 for a page past the end).
	static u_int VisibleCellCount(u_int uPage, u_int uSpeciesCount);
	// One cell label. CAUGHT -> "#001 Fernfawn" (the 1-based dex number, zero-padded
	// to three digits, resolved through ZM_GetSpeciesName). NOT caught -> "#001 -----":
	// the number is always readable but the NAME stays hidden until the species is
	// caught, which is the whole point of a dex.
	static std::string FormatCell(ZM_SPECIES_ID eSpecies, bool bCaught);
	// The completion header, "Dex <caught>/<total>".
	static std::string FormatCompletion(u_int uCaught, u_int uTotal);

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// Page 0, cursor 0.
	void Reset();
	// Confirm dispatches BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick with a
	// component pointer, which dangles when the ECS pool relocates the host. The two
	// page buttons page (clamped at both ends); a CELL is deliberately INERT -- the
	// per-species detail panel is out of scope for SC5. Returns true only when the
	// page actually changed.
	bool Confirm(const char* szFocusedElementName, u_int uSpeciesCount);

	int GetPage() const { return m_iPage; }
	// The focused CELL mirror: -1 while the focus sits on a page button, and while the
	// screen is not presented at all.
	int GetCursor() const { return m_iCursor; }

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the screen: builds the grid + its cells ONCE if they are absent,
	// then fills the current page's cells from the caught set and refreshes the
	// completion header. A missing UI component or element is skipped silently --
	// presentation never crashes the menu. The only model state it writes is the page
	// clamp and the cursor mirror.
	void Present(Zenith_Entity& xRootEntity, const ZM_SpeciesSet& xCaught);
	// Hide the panel, the grid (and every cell), the header and both page buttons, and
	// clear the cursor to -1 (nothing is presented, so no cell is focused). Deliberately
	// does NOT reset the PAGE -- dropping the session state is Reset's job, which the
	// menu stack owns.
	void Hide(Zenith_Entity& xRootEntity);

private:
	int m_iPage   = 0;
	int m_iCursor = 0;
};
