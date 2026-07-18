#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIButton.h"                                // SC5 menu buttons (SetText / SetFocused / SetVisible)
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"   // core read API + ZM_BattlePresentationOp + ZM_MapEventToOp + ZM_InstantBattlesEnabled
#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"          // GetEvent / GetEventCount over the presented range
#include "Zenithmon/Source/Battle/ZM_BattleEvent.h"           // ZM_BattleEvent + ZM_BATTLE_EVENT kinds
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"         // ZM_BattleMonster / ZM_MoveSlot / uZM_MAX_MOVES (menu move table)
#include "Zenithmon/Source/Battle/ZM_BattleState.h"           // Side(...).Active() chain (SC5 move read)
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"           // ZM_SIDE, ZM_BattleAction, ZM_ACTION_*
#include "Zenithmon/Source/Data/ZM_MoveData.h"                // ZM_GetMoveName / ZM_MOVE_ID
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"             // ZM_GetSpeciesName / ZM_SPECIES_ID
#include "Zenithmon/Source/ZM_InputActions.h"                 // ReadMenuVertical / ReadConfirmPressed / ReadCancelPressed (SC5 input)

#include <cmath>
#include <string>

// ============================================================================
// ZM_UI_BattleHUD (S5 item 4 SC4). The director-owned battle HUD: a typewriter
// text log + two HP panels + two HP bars, authored onto the BattleDirector
// entity's Zenith_UIComponent at bake time and driven Setup / Update / Hide.
// ZM-D-102 (the SC3 director) owns this instance by value.
// ============================================================================

namespace
{
	// Reveal speed for the typewriter log line (glyphs per wall-clock second). Only
	// consulted when zm_instant_battles is OFF; instant collapses the whole line.
	constexpr float fCHARS_PER_SEC = 45.0f;

	// The five authored element names (mirrored by the windowed test + the authoring
	// step ZM_ConfigureBattleHUD in Zenithmon.cpp).
	constexpr const char* szLOG_NAME          = "BattleHUD_Log";
	constexpr const char* szPLAYER_PANEL_NAME = "BattleHUD_PlayerPanel";
	constexpr const char* szENEMY_PANEL_NAME  = "BattleHUD_EnemyPanel";
	constexpr const char* szPLAYER_HPBAR_NAME = "BattleHUD_PlayerHPBar";
	constexpr const char* szENEMY_HPBAR_NAME  = "BattleHUD_EnemyHPBar";

	// The event's subject species name, prefixed "Foe " on the enemy side. The
	// subject is the active species of the event's side (the panel/anim subject).
	std::string ZM_HudSubjectName(const ZM_BattleEvent& xEvent,
		ZM_SPECIES_ID ePlayerActiveSpecies, ZM_SPECIES_ID eEnemyActiveSpecies)
	{
		const ZM_SPECIES_ID eSpecies = (xEvent.m_uSide == ZM_SIDE_PLAYER)
			? ePlayerActiveSpecies
			: eEnemyActiveSpecies;
		std::string strName = ZM_GetSpeciesName(eSpecies);
		if (xEvent.m_uSide == ZM_SIDE_ENEMY)
		{
			return std::string("Foe ") + strName;
		}
		return strName;
	}

	// Set a named element's visibility if it resolves (best-effort).
	void ZM_SetHudElementVisible(Zenith_UIComponent& xUI, const char* szName, bool bVisible)
	{
		Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(szName);
		if (pxElement != nullptr)
		{
			pxElement->SetVisible(bVisible);
		}
	}

	// Re-resolve both HP panels + bars each call (never cache) and write the current
	// species / level / HP into them.
	void ZM_RefreshHudHpPanels(Zenith_UIComponent& xUI, const ZM_BattleDirectorCore& xCore)
	{
		struct SidePanel
		{
			ZM_SIDE     m_eSide;
			const char* m_szPanel;
			const char* m_szBar;
		};
		const SidePanel axSides[2] =
		{
			{ ZM_SIDE_PLAYER, szPLAYER_PANEL_NAME, szPLAYER_HPBAR_NAME },
			{ ZM_SIDE_ENEMY,  szENEMY_PANEL_NAME,  szENEMY_HPBAR_NAME  },
		};

		for (const SidePanel& xSide : axSides)
		{
			const u_int uCurHp  = xCore.SideActiveHP(xSide.m_eSide);
			const u_int uMaxHp  = xCore.SideActiveMaxHP(xSide.m_eSide);

			Zenith_UI::Zenith_UIText* pxPanel =
				xUI.FindElement<Zenith_UI::Zenith_UIText>(xSide.m_szPanel);
			if (pxPanel != nullptr)
			{
				pxPanel->SetText(ZM_UI_BattleHUD::FormatHpPanel(
					xCore.SideActiveSpecies(xSide.m_eSide),
					xCore.SideActiveLevel(xSide.m_eSide),
					uCurHp, uMaxHp));
			}

			Zenith_UI::Zenith_UIRect* pxBar =
				xUI.FindElement<Zenith_UI::Zenith_UIRect>(xSide.m_szBar);
			if (pxBar != nullptr)
			{
				pxBar->SetFillAmount(ZM_UI_BattleHUD::ComputeHpFraction(uCurHp, uMaxHp));
			}
		}
	}

	// --- SC5 battle menu elements (mirrored by the authoring step ZM_ConfigureBattleHUD
	//     in Zenithmon.cpp + the parallel Test Author). Seven total: one panel rect +
	//     Fight/Run root buttons + four move buttons. ---
	constexpr const char* szMENU_PANEL_NAME   = "BattleHUD_MenuPanel";
	constexpr const char* szACTION_FIGHT_NAME = "BattleHUD_ActionFight";
	constexpr const char* szACTION_RUN_NAME   = "BattleHUD_ActionRun";
	constexpr const char* szMOVE_NAMES[uZM_MAX_MOVES] =
	{
		"BattleHUD_Move0", "BattleHUD_Move1", "BattleHUD_Move2", "BattleHUD_Move3"
	};

	// Focus tint for the button under the cursor; plain white otherwise.
	const Zenith_Maths::Vector4 xMENU_FOCUS_COLOUR  = { 1.0f, 0.85f, 0.30f, 1.0f };
	const Zenith_Maths::Vector4 xMENU_NORMAL_COLOUR = { 1.0f, 1.0f, 1.0f, 1.0f };

	// Best-effort per-frame refresh of one menu button: optionally overwrite its text
	// (szText == nullptr keeps the authored label), set visibility, and highlight it
	// (keyboard focus + tint) when it is the cursor target.
	void ZM_ApplyMenuButton(Zenith_UI::Zenith_UIButton* pxButton, bool bVisible, bool bFocused, const char* szText)
	{
		if (pxButton == nullptr)
		{
			return;
		}
		if (szText != nullptr)
		{
			pxButton->SetText(szText);
		}
		pxButton->SetVisible(bVisible);
		pxButton->SetFocused(bVisible && bFocused);
		pxButton->SetTextColor((bVisible && bFocused) ? xMENU_FOCUS_COLOUR : xMENU_NORMAL_COLOUR);
	}

	// Re-resolve + refresh all seven menu elements for the current screen + cursor
	// (never cache). ROOT shows the panel + Fight/Run; MOVE_SELECT shows the panel +
	// the move buttons for the filled slots; anything else hides them all. The move
	// buttons' text is set every frame regardless of visibility.
	void ZM_RefreshBattleMenuElements(Zenith_UIComponent& xUI, ZM_BattleMenuScreen eScreen,
		int iMenuCursor, const char* const* paszMoveName, int iMoveCount)
	{
		const bool bRoot = (eScreen == ZM_BATTLE_MENU_ACTION_ROOT);
		const bool bMove = (eScreen == ZM_BATTLE_MENU_MOVE_SELECT);

		ZM_SetHudElementVisible(xUI, szMENU_PANEL_NAME, bRoot || bMove);

		ZM_ApplyMenuButton(xUI.FindElement<Zenith_UI::Zenith_UIButton>(szACTION_FIGHT_NAME),
			bRoot, bRoot && iMenuCursor == (int)ZM_BATTLE_MENU_FIGHT, nullptr);
		ZM_ApplyMenuButton(xUI.FindElement<Zenith_UI::Zenith_UIButton>(szACTION_RUN_NAME),
			bRoot, bRoot && iMenuCursor == (int)ZM_BATTLE_MENU_RUN, nullptr);

		for (int i = 0; i < (int)uZM_MAX_MOVES; ++i)
		{
			const bool bSlotShown = bMove && (i < iMoveCount);
			ZM_ApplyMenuButton(xUI.FindElement<Zenith_UI::Zenith_UIButton>(szMOVE_NAMES[i]),
				bSlotShown, bSlotShown && iMenuCursor == i, paszMoveName[i]);
		}
	}
}

void ZM_UI_BattleHUD::Setup(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore)
{
	m_strShownLine.clear();
	m_fLineElapsedSeconds = 0.0f;

	Zenith_UIComponent* pxUI = xDirectorEntity.IsValid()
		? xDirectorEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never aborts the battle
	}

	// Reveal every element the authoring step created hidden.
	ZM_SetHudElementVisible(*pxUI, szLOG_NAME,          true);
	ZM_SetHudElementVisible(*pxUI, szPLAYER_PANEL_NAME, true);
	ZM_SetHudElementVisible(*pxUI, szENEMY_PANEL_NAME,  true);
	ZM_SetHudElementVisible(*pxUI, szPLAYER_HPBAR_NAME, true);
	ZM_SetHudElementVisible(*pxUI, szENEMY_HPBAR_NAME,  true);

	// Seed the panels from the opening state (the intro will overwrite the log).
	ZM_RefreshHudHpPanels(*pxUI, xCore);
}

void ZM_UI_BattleHUD::Update(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore, float fDeltaSeconds)
{
	Zenith_UIComponent* pxUI = xDirectorEntity.IsValid()
		? xDirectorEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;
	}

	// Re-resolve the log element every frame (the canvas may relocate elements).
	Zenith_UI::Zenith_UIText* pxLog = pxUI->FindElement<Zenith_UI::Zenith_UIText>(szLOG_NAME);

	// Show the most-recently-PRESENTED text-carrying line. Scanning the presented range
	// [0, PresentedEventCount()) -- rather than only CurrentEvent() -- is ESSENTIAL under
	// zm_instant_battles: there a single Tick drains every op, so CurrentEvent() is
	// already null by the time we sample and the final line (the winner banner, a faint)
	// would never reach the log. In timed play the presented range grows op by op, so the
	// current line still shows and glyph-reveals normally. Read + format + discard within
	// this call (never cache event pointers across frames).
	const ZM_BattleEngine& xEngine = xCore.GetEngine();
	u_int uPresented = xCore.PresentedEventCount();
	if (uPresented > xEngine.GetEventCount())
	{
		uPresented = xEngine.GetEventCount();   // defensive clamp
	}
	const ZM_BattleEvent* pxLine = nullptr;
	for (u_int i = uPresented; i-- > 0u; )
	{
		const ZM_BattleEvent& xCandidate = xEngine.GetEvent(i);
		if (ZM_MapEventToOp(xCandidate).m_bCarriesText)
		{
			pxLine = &xCandidate;
			break;
		}
	}
	if (pxLine != nullptr)
	{
		const std::string strLine = FormatBattleLogLine(
			*pxLine,
			xCore.SideActiveSpecies(ZM_SIDE_PLAYER),
			xCore.SideActiveSpecies(ZM_SIDE_ENEMY));
		if (!strLine.empty() && strLine != m_strShownLine)
		{
			m_strShownLine = strLine;
			m_fLineElapsedSeconds = 0.0f;
			if (pxLog != nullptr)
			{
				pxLog->SetText(strLine);
			}
		}
	}
	// (Deliberately do NOT clear the latched line when no text event is present yet -- the
	// last line stays revealed between ops.)

	// Advance the typewriter reveal of the currently-shown line.
	m_fLineElapsedSeconds += fDeltaSeconds;
	if (pxLog != nullptr)
	{
		pxLog->SetVisibleGlyphCount(ComputeVisibleGlyphCount(
			pxLog->GetTotalGlyphCount(), m_fLineElapsedSeconds, ZM_InstantBattlesEnabled()));
	}

	// Refresh both HP panels + bars from live state every frame.
	ZM_RefreshHudHpPanels(*pxUI, xCore);
}

void ZM_UI_BattleHUD::Hide(Zenith_Entity& xDirectorEntity)
{
	Zenith_UIComponent* pxUI = xDirectorEntity.IsValid()
		? xDirectorEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;
	}
	ZM_SetHudElementVisible(*pxUI, szLOG_NAME,          false);
	ZM_SetHudElementVisible(*pxUI, szPLAYER_PANEL_NAME, false);
	ZM_SetHudElementVisible(*pxUI, szENEMY_PANEL_NAME,  false);
	ZM_SetHudElementVisible(*pxUI, szPLAYER_HPBAR_NAME, false);
	ZM_SetHudElementVisible(*pxUI, szENEMY_HPBAR_NAME,  false);
}

// ============================================================================
// SC5 -- the interactive Fight/Run battle menu (the first player-driven input).
// ============================================================================

int ZM_UI_BattleHUD::MenuItemCount(ZM_BattleMenuScreen eScreen, int iMoveCount)
{
	switch (eScreen)
	{
	case ZM_BATTLE_MENU_ACTION_ROOT:
		return (int)ZM_BATTLE_MENU_ROOT_COUNT;   // 2 (Fight, Run)
	case ZM_BATTLE_MENU_MOVE_SELECT:
		return iMoveCount;
	case ZM_BATTLE_MENU_HIDDEN:
	default:
		return 0;
	}
}

int ZM_UI_BattleHUD::MenuMoveCursor(int iCursor, int iDelta, int iItemCount)
{
	if (iItemCount <= 0)
	{
		return 0;
	}
	int iNext = iCursor + iDelta;
	if (iNext < 0)
	{
		iNext = 0;               // clamp low (no wrap)
	}
	if (iNext > iItemCount - 1)
	{
		iNext = iItemCount - 1;  // clamp high (no wrap)
	}
	return iNext;
}

ZM_BattleMenuConfirmResult ZM_UI_BattleHUD::MenuConfirm(ZM_BattleMenuScreen eScreen, int iCursor,
	const bool* pbMoveSelectable, int iMoveCount)
{
	ZM_BattleMenuConfirmResult xResult;   // default {NONE, action{}, next=HIDDEN, cursor 0}
	switch (eScreen)
	{
	case ZM_BATTLE_MENU_ACTION_ROOT:
		if (iCursor == (int)ZM_BATTLE_MENU_FIGHT)
		{
			xResult.m_eKind       = ZM_BATTLE_MENU_CONFIRM_OPEN_MOVES;
			xResult.m_eNextScreen = ZM_BATTLE_MENU_MOVE_SELECT;
			xResult.m_iNextCursor = 0;
		}
		else if (iCursor == (int)ZM_BATTLE_MENU_RUN)
		{
			xResult.m_eKind           = ZM_BATTLE_MENU_CONFIRM_SUBMIT;
			xResult.m_xAction.m_eKind = ZM_ACTION_RUN;
			xResult.m_eNextScreen     = ZM_BATTLE_MENU_HIDDEN;
			xResult.m_iNextCursor     = 0;
		}
		return xResult;   // any other cursor -> {NONE}
	case ZM_BATTLE_MENU_MOVE_SELECT:
		if (pbMoveSelectable != nullptr && iCursor >= 0 && iCursor < iMoveCount && pbMoveSelectable[iCursor])
		{
			xResult.m_eKind               = ZM_BATTLE_MENU_CONFIRM_SUBMIT;
			xResult.m_xAction.m_eKind     = ZM_ACTION_MOVE;
			xResult.m_xAction.m_uMoveSlot = (u_int)iCursor;
			xResult.m_eNextScreen         = ZM_BATTLE_MENU_HIDDEN;
			xResult.m_iNextCursor         = 0;
		}
		// else: {NONE} -- stay on MOVE_SELECT, cursor unchanged (UpdateMenu ignores next*)
		return xResult;
	case ZM_BATTLE_MENU_HIDDEN:
	default:
		return xResult;   // {NONE, next=HIDDEN}
	}
}

ZM_BattleMenuScreen ZM_UI_BattleHUD::MenuCancel(ZM_BattleMenuScreen eScreen)
{
	if (eScreen == ZM_BATTLE_MENU_MOVE_SELECT)
	{
		return ZM_BATTLE_MENU_ACTION_ROOT;
	}
	return eScreen;
}

bool ZM_UI_BattleHUD::UpdateMenu(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore,
	ZM_BattleAction& xOut)
{
	// A fresh AWAIT_INPUT turn opens the root menu; subsequent frames keep the cursor.
	if (m_eMenuScreen == ZM_BATTLE_MENU_HIDDEN)
	{
		m_eMenuScreen = ZM_BATTLE_MENU_ACTION_ROOT;
		m_iMenuCursor = 0;
	}

	// Build the player active's move table: selectable iff a real move with PP left.
	// The active always has >=1 move, so iMoveCount is normally >=1.
	bool        abSelectable[uZM_MAX_MOVES] = {};
	const char* aszMoveName[uZM_MAX_MOVES]  = { "", "", "", "" };
	int         iMoveCount = 0;
	{
		const ZM_BattleMonster& xActive = xCore.GetEngine().GetState().Side(ZM_SIDE_PLAYER).Active();
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			const ZM_MoveSlot& xSlot = xActive.m_axMoves[i];
			if (xSlot.m_eMove != ZM_MOVE_NONE)
			{
				abSelectable[i] = (xSlot.m_uCurPP > 0u);
				aszMoveName[i]  = ZM_GetMoveName(xSlot.m_eMove);
				++iMoveCount;
			}
		}
	}

	// Edge input: nav FIRST, then confirm, then cancel (Down+Enter in one frame moves
	// then confirms).
	const int  iNav     = ZM_InputActions::ReadMenuVertical();
	const bool bConfirm = ZM_InputActions::ReadConfirmPressed();
	const bool bCancel  = ZM_InputActions::ReadCancelPressed();

	m_iMenuCursor = MenuMoveCursor(m_iMenuCursor, iNav, MenuItemCount(m_eMenuScreen, iMoveCount));

	bool bSubmitted = false;
	if (bConfirm)
	{
		const ZM_BattleMenuConfirmResult xR = MenuConfirm(m_eMenuScreen, m_iMenuCursor, abSelectable, iMoveCount);
		if (xR.m_eKind == ZM_BATTLE_MENU_CONFIRM_SUBMIT)
		{
			xOut          = xR.m_xAction;
			m_eMenuScreen = xR.m_eNextScreen;
			m_iMenuCursor = xR.m_iNextCursor;
			bSubmitted    = true;
		}
		else if (xR.m_eKind == ZM_BATTLE_MENU_CONFIRM_OPEN_MOVES)
		{
			m_eMenuScreen = xR.m_eNextScreen;
			m_iMenuCursor = xR.m_iNextCursor;
		}
	}
	else if (bCancel)
	{
		m_eMenuScreen = MenuCancel(m_eMenuScreen);
		m_iMenuCursor = 0;
	}

	// Refresh the authored buttons (best-effort: a missing UI component skips the
	// visuals, but the state machine above already ran so a headless drive still
	// submits). Re-resolve every element each frame -- never cache.
	Zenith_UIComponent* pxUI = xDirectorEntity.IsValid()
		? xDirectorEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI != nullptr)
	{
		ZM_RefreshBattleMenuElements(*pxUI, m_eMenuScreen, m_iMenuCursor, aszMoveName, iMoveCount);
	}

	return bSubmitted;
}

void ZM_UI_BattleHUD::HideMenu(Zenith_Entity& xDirectorEntity)
{
	m_eMenuScreen = ZM_BATTLE_MENU_HIDDEN;
	m_iMenuCursor = 0;

	Zenith_UIComponent* pxUI = xDirectorEntity.IsValid()
		? xDirectorEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;
	}
	ZM_SetHudElementVisible(*pxUI, szMENU_PANEL_NAME,   false);
	ZM_SetHudElementVisible(*pxUI, szACTION_FIGHT_NAME, false);
	ZM_SetHudElementVisible(*pxUI, szACTION_RUN_NAME,   false);
	for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
	{
		ZM_SetHudElementVisible(*pxUI, szMOVE_NAMES[i], false);
	}
}

std::string ZM_UI_BattleHUD::FormatBattleLogLine(const ZM_BattleEvent& xEvent,
	ZM_SPECIES_ID ePlayerActiveSpecies, ZM_SPECIES_ID eEnemyActiveSpecies)
{
	switch (xEvent.m_eKind)
	{
	// -- Framing events never spam the log (EXACTLY ""). --
	case ZM_BATTLE_EVENT_BATTLE_BEGIN:
	case ZM_BATTLE_EVENT_TURN_BEGIN:
	case ZM_BATTLE_EVENT_TURN_END:
	case ZM_BATTLE_EVENT_EVOLUTION_QUEUED:   // the evolution cutscene is post-battle, not in-battle
		return "";

	case ZM_BATTLE_EVENT_SWITCH_IN:
	{
		const std::string strName = ZM_GetSpeciesName((ZM_SPECIES_ID)xEvent.m_uSpeciesId);
		const char* szPrefix = (xEvent.m_uSide == ZM_SIDE_ENEMY) ? "Foe " : "";
		return std::string(szPrefix) + strName + " appeared!";
	}

	case ZM_BATTLE_EVENT_MOVE_USED:
	{
		const std::string strSubject = ZM_HudSubjectName(xEvent, ePlayerActiveSpecies, eEnemyActiveSpecies);
		const std::string strMove    = ZM_GetMoveName((ZM_MOVE_ID)xEvent.m_uMoveId);
		return strSubject + " used " + strMove + "!";
	}

	case ZM_BATTLE_EVENT_MOVE_MISSED:
	{
		const std::string strSubject = ZM_HudSubjectName(xEvent, ePlayerActiveSpecies, eEnemyActiveSpecies);
		return strSubject + "'s attack missed!";
	}

	case ZM_BATTLE_EVENT_CRIT:
		return "A critical hit!";

	case ZM_BATTLE_EVENT_SUPER_EFFECTIVE:
		return "It's super effective!";

	case ZM_BATTLE_EVENT_NOT_EFFECTIVE:
		return "It's not very effective...";

	case ZM_BATTLE_EVENT_IMMUNE:
	{
		const std::string strSubject = ZM_HudSubjectName(xEvent, ePlayerActiveSpecies, eEnemyActiveSpecies);
		return "It doesn't affect " + strSubject + "...";
	}

	case ZM_BATTLE_EVENT_DAMAGE_DEALT:
	{
		// Damage is shown via the HP bar (op carries no text), but map it sensibly.
		const std::string strSubject = ZM_HudSubjectName(xEvent, ePlayerActiveSpecies, eEnemyActiveSpecies);
		return strSubject + " took damage!";
	}

	case ZM_BATTLE_EVENT_FAINT:
	{
		const std::string strSubject = ZM_HudSubjectName(xEvent, ePlayerActiveSpecies, eEnemyActiveSpecies);
		return strSubject + " fainted!";
	}

	case ZM_BATTLE_EVENT_BATTLE_END:
	{
		// Three mutually distinct, non-empty winner strings.
		if (xEvent.m_iAmount == (int)ZM_SIDE_PLAYER)
		{
			return "You won the battle!";
		}
		if (xEvent.m_iAmount == (int)ZM_SIDE_ENEMY)
		{
			return "You lost the battle...";
		}
		return "The battle ended in a draw.";
	}

	// -- Reserved kinds (inert this SC): concise, sensible, non-crashing lines. --
	case ZM_BATTLE_EVENT_NO_PP:
		return "But there was no PP left!";
	case ZM_BATTLE_EVENT_MOVE_FAILED:
		return "But it failed!";
	case ZM_BATTLE_EVENT_STATUS_APPLIED:
		return "A status condition set in!";
	case ZM_BATTLE_EVENT_STATUS_DAMAGE:
		return "It was hurt by its status!";
	case ZM_BATTLE_EVENT_STATUS_CURED:
		return "Its status returned to normal.";
	case ZM_BATTLE_EVENT_STAT_STAGE_CHANGED:
		return "A stat changed!";
	case ZM_BATTLE_EVENT_VOLATILE_APPLIED:
		return "A condition took hold!";
	case ZM_BATTLE_EVENT_VOLATILE_ENDED:
		return "A condition ended.";
	case ZM_BATTLE_EVENT_FLINCH:
		return "It flinched and couldn't move!";
	case ZM_BATTLE_EVENT_HEAL:
		return "Its HP was restored!";
	case ZM_BATTLE_EVENT_DRAIN:
		return "It drained some HP!";
	case ZM_BATTLE_EVENT_RECOIL:
		return "It was hurt by recoil!";
	case ZM_BATTLE_EVENT_MULTI_HIT:
		return "It struck multiple times!";
	case ZM_BATTLE_EVENT_ABILITY_TRIGGER:
		return "An ability triggered!";
	case ZM_BATTLE_EVENT_WEATHER_CHANGED:
		return "The weather changed!";
	case ZM_BATTLE_EVENT_WEATHER_DAMAGE:
		return "It was buffeted by the weather!";
	case ZM_BATTLE_EVENT_SCREEN_SET:
		return "A protective screen went up!";
	case ZM_BATTLE_EVENT_SCREEN_EXPIRED:
		return "A protective screen wore off.";
	case ZM_BATTLE_EVENT_EXP_GAINED:
		return "Gained experience!";
	case ZM_BATTLE_EVENT_LEVEL_UP:
		return "It grew to a new level!";
	case ZM_BATTLE_EVENT_MOVE_LEARNED:
		return "It learned a new move!";
	case ZM_BATTLE_EVENT_CATCH_SHAKE:
		return "The capsule shakes...";
	case ZM_BATTLE_EVENT_CATCH_RESULT:
		return "The capsule clicks shut!";
	case ZM_BATTLE_EVENT_FLEE:
		return "Got away safely!";
	case ZM_BATTLE_EVENT_FLEE_FAILED:
		return "Couldn't get away!";

	// -- Totality: ZM_BATTLE_EVENT_COUNT and any out-of-range value map to "". --
	default:
		return "";
	}
}

std::string ZM_UI_BattleHUD::FormatHpPanel(ZM_SPECIES_ID eSpecies, u_int uLevel, u_int uCurHp, u_int uMaxHp)
{
	std::string strPanel = ZM_GetSpeciesName(eSpecies);
	strPanel += "  Lv";
	strPanel += std::to_string(uLevel);
	strPanel += "  HP ";
	strPanel += std::to_string(uCurHp);
	strPanel += "/";
	strPanel += std::to_string(uMaxHp);
	return strPanel;
}

float ZM_UI_BattleHUD::ComputeHpFraction(u_int uCurHp, u_int uMaxHp)
{
	if (uMaxHp == 0u)
	{
		return 0.0f;
	}
	const float fFraction = (float)uCurHp / (float)uMaxHp;
	if (fFraction < 0.0f)
	{
		return 0.0f;
	}
	if (fFraction > 1.0f)
	{
		return 1.0f;
	}
	return fFraction;
}

int ZM_UI_BattleHUD::ComputeVisibleGlyphCount(int iTotalGlyphs, float fLineElapsedSeconds, bool bInstant)
{
	if (bInstant)
	{
		return iTotalGlyphs;
	}
	if (iTotalGlyphs <= 0)
	{
		return 0;
	}
	const float fGlyphs = std::floor(fLineElapsedSeconds * fCHARS_PER_SEC);
	if (fGlyphs <= 0.0f)
	{
		return 0;   // negative / zero elapsed reveals nothing yet
	}
	if (fGlyphs >= (float)iTotalGlyphs)
	{
		return iTotalGlyphs;   // huge elapsed clamps to the whole line
	}
	return (int)fGlyphs;
}
