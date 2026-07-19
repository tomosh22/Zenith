#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID (by value in the pure formatters)

#include <string>

class Zenith_Entity;
struct ZM_Party;
struct ZM_Monster;

// ============================================================================
// ZM_UI_Party (S6 item 2 SC4) -- the overworld party screen: a list of up to six
// party rows, plus a per-member summary panel toggled by confirm.
//
// It is a small NON-ECS presentation class OWNED BY VALUE by ZM_UI_MenuStack (the
// ZM_UI_DialogueBox seam): NO order, NO component registration, NO editor mirror.
// Its panel / six slot buttons / summary panel + text are authored at bake time
// onto the persistent ZM_MenuRoot entity's Zenith_UIComponent (ZM_ConfigureMenuRoot)
// and the menu stack drives it Confirm / Cancel / Present / Hide.
//
// The instance state is PODs only (a cursor + a flag), so the owning component's
// defaulted noexcept move stays well-formed when the ECS pool relocates it.
//
// Traversal is the ENGINE focus-navigation (the slot buttons carry explicit
// up/down links, exactly like the ROOT entries) -- Present only ENSURES a focused
// visible slot and MIRRORS the engine-navigated focus back into m_iCursor. There
// is no hand-rolled cursor movement.
// ============================================================================
class ZM_UI_Party
{
public:
	// Must equal uZM_MAX_PARTY_SIZE -- static_asserted in the .cpp (the header cannot
	// see the battle types without dragging the whole party model into every includer).
	static constexpr u_int uMAX_SLOTS = 6u;

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution (never cache element pointers -- the canvas may relocate them).
	static constexpr const char* szPANEL_NAME         = "Menu_PartyPanel";
	static constexpr const char* szSUMMARY_PANEL_NAME = "Menu_PartySummaryPanel";
	static constexpr const char* szSUMMARY_TEXT_NAME  = "Menu_PartySummaryText";

	// ---- PURE statics (no scene / graphics -- unit-tested verbatim) ----

	// The authored element name for a slot ("Menu_PartySlot0".."Menu_PartySlot5");
	// "" for an out-of-range slot. Returns string LITERALS (the RootItemElementName
	// idiom) -- never allocates, never dangles, so authoring may call it at bake time.
	static const char* SlotElementName(u_int uSlot);
	// The slot index for an element name, or -1 when it is not a party slot.
	static int SlotIndexFromElementName(const char* szElementName);
	// How many slot widgets are shown for a party of uPartyCount: min(count, uMAX_SLOTS).
	static u_int VisibleSlotCount(u_int uPartyCount);
	// One list row: the shared "<Species>  Lv<n>  HP <cur>/<max>" panel string
	// (ZM_UI_BattleHUD::FormatHpPanel -- the ONE formatter, never re-derived) with
	// "  FAINTED" appended at zero current HP.
	static std::string FormatPartyRow(ZM_SPECIES_ID eSpecies, u_int uLevel,
		u_int uCurHp, u_int uMaxHp);
	// The per-member summary body, '\n'-separated: the FormatPartyRow row, then
	// "Nature: <name>", "Ability: <name>", "Status: <name>", then one
	// "<MoveName>  PP <cur>/<max>" line per NON-EMPTY move slot. A record with no
	// moves yields exactly the four header lines (never a crash).
	static std::string FormatSummary(const ZM_Monster& xRecord);

	// ---- Instance drive (called only by ZM_UI_MenuStack) ----

	// Cursor 0, summary closed.
	void Reset();
	// Confirm TOGGLES the summary for the focused slot: it opens on the list and
	// closes when already open. There is no per-member action menu yet -- switch /
	// use-item / cancel-target arrive with SC6 / SC7. A NO-OP while no member sits
	// under the cursor (an empty party, or the screen is not presented), so the flag
	// can never claim a summary that nothing is drawing.
	void Confirm();
	// True when the SCREEN consumed the cancel (an open summary closed); false when
	// the menu stack should POP the party screen.
	bool Cancel();

	bool IsSummaryOpen() const { return m_bSummaryOpen; }
	// The focused slot mirror; -1 when the screen is not presented (or the party is
	// empty, so there is no slot to focus).
	int  GetCursor() const { return m_iCursor; }

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the panel + the visible slot rows from the live party, plus the
	// summary panel while it is open. A missing UI component or element is skipped
	// silently -- presentation never crashes the menu. The only model state it writes
	// is the cursor mirror (the focused slot, or -1 for an empty party).
	void Present(Zenith_Entity& xRootEntity, const ZM_Party& xParty);
	// Hide the panel, every slot, and the summary, and clear the cursor to -1 (nothing
	// is presented, so no slot is focused). Deliberately does NOT clear the summary
	// FLAG -- dropping the session state is Reset's job, which the stack owns.
	void Hide(Zenith_Entity& xRootEntity);

private:
	int  m_iCursor = 0;
	bool m_bSummaryOpen = false;
};
