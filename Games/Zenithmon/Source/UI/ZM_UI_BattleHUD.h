#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID (by value in the static formatters)
#include "Zenithmon/Source/Data/ZM_MoveData.h"       // ZM_MOVE_ID (by value in BuildFilledMoveMenu)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"  // ZM_BattleAction (by value in ZM_BattleMenuConfirmResult), uZM_MAX_MOVES

#include <string>

class Zenith_Entity;
class ZM_BattleDirectorCore;
struct ZM_BattleEvent;

// ============================================================================
// Battle menu state machine (S5 item 4 SC5) -- the first PLAYER-driven battle
// input. Fight -> move submenu -> SubmitPlayerAction({MOVE, slot}); Run ->
// SubmitPlayerAction({ZM_ACTION_RUN}). The enums / result struct below are the
// FROZEN CONTRACT unit-tested verbatim by the parallel Test Author.
// ============================================================================
enum ZM_BattleMenuScreen : u_int
{
    ZM_BATTLE_MENU_HIDDEN,
    ZM_BATTLE_MENU_ACTION_ROOT,   // cursor over the root entries (see ZM_BattleMenuRootItem)
    ZM_BATTLE_MENU_MOVE_SELECT,   // cursor over move slots [0..iMoveCount-1]
};
// The root entries' IDENTITIES. These are NOT cursor indices: the root list is
// DYNAMIC -- the Catch entry exists only when the battle config allows catching, so
// with catching OFF the list is [Fight, Run] and Run sits at cursor 1. Always map a
// cursor to an entry through MenuRootItemAtIndex; never compare a cursor to one of
// these values directly. ZM_BATTLE_MENU_ROOT_COUNT doubles as the "no entry here"
// sentinel MenuRootItemAtIndex returns for an out-of-range index.
enum ZM_BattleMenuRootItem : u_int { ZM_BATTLE_MENU_FIGHT = 0u, ZM_BATTLE_MENU_CATCH = 1u, ZM_BATTLE_MENU_RUN = 2u, ZM_BATTLE_MENU_ROOT_COUNT = 3u };
// The root stack's authored geometry, in the BottomRight anchor's pixel space (more
// negative == higher up the panel): the FIRST row's Y and the pitch between rows.
// Shared by BOTH sites on purpose. The authoring step (ZM_ConfigureBattleHUD in
// Zenithmon.cpp) lays the three buttons out with them, and the presenter RE-ANCHORS
// each VISIBLE button to the row of its RESOLVED cursor index -- because with the
// Catch entry gated out the list is [Fight, Run] and Run must move up into row 1, or
// the panel renders Fight, a blank row, then Run, with the highlighted "index 1"
// drawn in the third slot. If the two sites spelled these numbers separately, that
// re-anchoring would silently drift away from the authored layout.
inline constexpr float fZM_BATTLE_MENU_ROOT_FIRST_ROW_Y = -128.0f;
inline constexpr float fZM_BATTLE_MENU_ROOT_ROW_PITCH_Y =   48.0f;
enum ZM_BattleMenuConfirmKind : u_int { ZM_BATTLE_MENU_CONFIRM_NONE, ZM_BATTLE_MENU_CONFIRM_OPEN_MOVES, ZM_BATTLE_MENU_CONFIRM_SUBMIT };
struct ZM_BattleMenuConfirmResult
{
    ZM_BattleMenuConfirmKind m_eKind       = ZM_BATTLE_MENU_CONFIRM_NONE;
    ZM_BattleAction          m_xAction;                       // valid iff SUBMIT
    ZM_BattleMenuScreen      m_eNextScreen  = ZM_BATTLE_MENU_HIDDEN;
    int                      m_iNextCursor  = 0;
};

// ============================================================================
// ZM_UI_BattleHUD (S5 item 4 SC4) -- the first VISIBLE battle UI: a text log
// revealed by the E3 typewriter plus two HP panels (species / level / HP text +
// an HP bar). It is a small NON-ECS presentation class OWNED BY VALUE by
// ZM_BattleDirector (seam Option B): NO order, NO component registration, NO
// editor mirror. Its Zenith_UIComponent + elements are authored at bake time onto
// the existing BattleDirector entity (mirroring how BattleFade is authored on the
// ZM_BattleTransitionRoot entity), and the director drives it Setup / Update /
// Hide. Still AI-vs-AI, NO interaction (that lands SC5).
//
// The instance holds only the latched log line + its typewriter clock, so it is
// trivially movable (the director move-constructs via the ECS component pool).
// The static formatters are pure (no scene / graphics / core) and unit-tested.
// ============================================================================
class ZM_UI_BattleHUD
{
public:
	// --- Instance drive (called only by ZM_BattleDirector) ---

	// Reveal all five HUD elements on the director entity's Zenith_UIComponent and
	// seed the two HP panels from the core's opening state. A missing UI component
	// skips gracefully. Resets the latched log line + typewriter clock.
	void Setup(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore);
	// Re-resolve the elements each frame (never cache pointers across frames): latch
	// a new log line when the current event carries text and differs, advance the
	// typewriter reveal, and refresh both HP panels.
	void Update(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore, float fDeltaSeconds);
	// Hide all five HUD elements (so the end-fade never shows the HUD over black).
	void Hide(Zenith_Entity& xDirectorEntity);

	// --- Battle menu drive (SC5; called by ZM_BattleDirector in AWAIT_INPUT) ---

	// Advance the Fight/Run menu one frame. On a fresh AWAIT_INPUT turn (screen
	// HIDDEN) it opens the ACTION_ROOT. Reads edge input (nav, confirm, cancel) and
	// walks the state machine via the pure statics below. Re-resolves + refreshes the
	// authored menu elements each frame (never caches; best-effort if absent). Returns
	// true exactly when the player submitted an action this frame (xOut is then valid).
	bool UpdateMenu(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore, ZM_BattleAction& xOut);
	// Hide all eight menu elements and reset to ZM_BATTLE_MENU_HIDDEN.
	void HideMenu(Zenith_Entity& xDirectorEntity);
	ZM_BattleMenuScreen GetMenuScreen() const { return m_eMenuScreen; }
	int                 GetMenuCursor() const { return m_iMenuCursor; }

	// --- PURE menu statics (no scene / graphics / core -- unit-tested verbatim) ---

	// How many entries the ROOT screen shows: 3 (Fight/Catch/Run) when catching is
	// allowed, 2 (Fight/Run) when it is not.
	static int MenuRootItemCount(bool bCanCatch);
	// The root ENTRY at a cursor index. With catching allowed this is the identity
	// (0->Fight, 1->Catch, 2->Run); with catching disallowed the Catch entry does not
	// exist at all and Run closes the gap (0->Fight, 1->Run). Any index outside
	// [0, MenuRootItemCount(bCanCatch)) yields ZM_BATTLE_MENU_ROOT_COUNT -- never a
	// real entry, so an out-of-range cursor can NEVER submit an action.
	static ZM_BattleMenuRootItem MenuRootItemAtIndex(int iIndex, bool bCanCatch);
	// ACTION_ROOT -> MenuRootItemCount(bCanCatch); MOVE_SELECT -> iMoveCount; HIDDEN -> 0.
	// bCanCatch comes from ZM_BattleDirectorCore::IsCatchAllowed() at the live call
	// site; it is a REQUIRED argument on purpose -- a defaulted "true" is exactly how a
	// caller would silently re-offer Catch in a trainer battle.
	static int MenuItemCount(ZM_BattleMenuScreen eScreen, int iMoveCount, bool bCanCatch);
	// iItemCount <= 0 -> 0; else clamp iCursor + iDelta to [0, iItemCount-1] (NO wrap).
	static int MenuMoveCursor(int iCursor, int iDelta, int iItemCount);
	// Compact the (possibly GAPPED) 4-slot moveset into a dense menu (SC3 gapped-moveset
	// fix). For each successive NON-empty raw slot k in [0,uZM_MAX_MOVES) it writes, at
	// the next filled index: the move name (ZM_GetMoveName), selectable == (curPP > 0),
	// and the raw engine slot index k. Trailing unused entries are cleared ("" / false /
	// -1). Returns the filled count -- so the cursor addresses the k-th FILLED move and
	// its raw engine slot is paiRawSlotOut[cursor]. Pure (no scene / graphics / core).
	static int BuildFilledMoveMenu(const ZM_MOVE_ID (&aeMoves)[uZM_MAX_MOVES], const u_int (&auCurPP)[uZM_MAX_MOVES],
		const char* (&paszNameOut)[uZM_MAX_MOVES], bool (&pbSelectableOut)[uZM_MAX_MOVES], int (&paiRawSlotOut)[uZM_MAX_MOVES]);
	// The pure confirm resolution (see the SC5 contract for the exact per-screen cases).
	// bCanCatch gates the CATCH entry: with it false the root list has no Catch entry,
	// so no cursor position can produce an ITEM (catch) action -- which is what keeps
	// the presenter from submitting the action ZM_BattleEngine::SubmitAction asserts on.
	// paiRawMoveSlot (SC3) maps the compacted move cursor back to a raw engine slot; when
	// null the submit slot is the cursor itself (identity).
	static ZM_BattleMenuConfirmResult MenuConfirm(ZM_BattleMenuScreen eScreen, int iCursor,
		const bool* pbMoveSelectable, int iMoveCount, bool bCanCatch,
		const int* paiRawMoveSlot = nullptr);
	// MOVE_SELECT -> ACTION_ROOT; any other screen -> unchanged.
	static ZM_BattleMenuScreen MenuCancel(ZM_BattleMenuScreen eScreen);

	// --- PURE statics (no scene / graphics / core -- unit-tested) ---

	// The total event->log-line mapping. Defined for EVERY ZM_BATTLE_EVENT kind in
	// [0, ZM_BATTLE_EVENT_COUNT); framing events return "" (never spam the log); an
	// out-of-range value falls through the default to "". Never asserts / crashes.
	// The subject species for MOVE_USED / FAINT is the active species of the event's
	// side; enemy-side subjects are prefixed "Foe ".
	static std::string FormatBattleLogLine(const ZM_BattleEvent& xEvent,
	                                       ZM_SPECIES_ID ePlayerActiveSpecies,
	                                       ZM_SPECIES_ID eEnemyActiveSpecies);
	// "<SpeciesName>  Lv<level>  HP <cur>/<max>" -- contains the species name, the
	// level as decimal, and the "<cur>/<max>" substring.
	static std::string FormatHpPanel(ZM_SPECIES_ID eSpecies, u_int uLevel, u_int uCurHp, u_int uMaxHp);
	// cur/max clamped to [0,1]; 0 when max == 0 (avoids a divide-by-zero).
	static float       ComputeHpFraction(u_int uCurHp, u_int uMaxHp);
	// Glyphs revealed so far: the whole line when bInstant, else floor(elapsed *
	// fCHARS_PER_SEC) clamped to [0, total]. Negative / huge elapsed clamp to 0 / total.
	static int         ComputeVisibleGlyphCount(int iTotalGlyphs, float fLineElapsedSeconds, bool bInstant);

private:
	std::string         m_strShownLine;
	float               m_fLineElapsedSeconds = 0.0f;
	// SC5 menu state (trivially copyable PODs -- the pool's move-construct is preserved).
	ZM_BattleMenuScreen m_eMenuScreen = ZM_BATTLE_MENU_HIDDEN;
	int                 m_iMenuCursor = 0;
};
