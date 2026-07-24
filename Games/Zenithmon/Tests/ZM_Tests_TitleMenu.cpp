#include "Zenith.h"

// ============================================================================
// ZM_Tests_TitleMenu -- S7 item 2 SC5 pure tests for the FrontEnd title model.
// No ECS, scene, graphics, input or disk: fixed slot-status snapshots are handed
// to the by-value presenter, so Continue visibility / focus policy is exhaustive
// and a damaged slot can never be confused with an empty one.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"

#include <cstring>

namespace
{
	constexpr u_int uTITLE_STATUS_COUNT = static_cast<u_int>(ZM_SAVE_SLOT_COUNT);

	void FillStatuses(ZM_SAVE_SLOT_STATUS* paeStatuses, ZM_SAVE_SLOT_STATUS eStatus)
	{
		for (u_int u = 0u; u < uTITLE_STATUS_COUNT; ++u)
		{
			paeStatuses[u] = eStatus;
		}
	}
}

ZENITH_TEST(ZM_Title, TitleMenu_ItemNamesRoundTripAndRejectForeignNames)
{
	ZENITH_ASSERT_EQ((u_int)ZM_TITLE_ITEM_COUNT, 2u,
		"the title has exactly Continue above New Game");

	for (u_int u = 0u; u < (u_int)ZM_TITLE_ITEM_COUNT; ++u)
	{
		const char* szName = ZM_UI_TitleMenu::ItemElementName(static_cast<ZM_TITLE_ITEM>(u));
		ZENITH_ASSERT_NOT_NULL(szName, "title item %u has an element name", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "title item %u's element name is non-empty", u);
		ZENITH_ASSERT_EQ(ZM_UI_TitleMenu::ItemIndexFromElementName(szName), (int)u,
			"title item %u round-trips through its authored element name", u);
	}

	ZENITH_ASSERT_NE(std::strcmp(ZM_UI_TitleMenu::ItemElementName(ZM_TITLE_ITEM_CONTINUE),
		ZM_UI_TitleMenu::ItemElementName(ZM_TITLE_ITEM_NEW_GAME)), 0,
		"Continue and New Game must never alias the same authored control");
	ZENITH_ASSERT_STREQ(ZM_UI_TitleMenu::ItemElementName(ZM_TITLE_ITEM_COUNT), "",
		"the out-of-range item maps to the safe empty-name sentinel");
	ZENITH_ASSERT_EQ(ZM_UI_TitleMenu::ItemIndexFromElementName(nullptr), -1,
		"a null name is not a title item");
	ZENITH_ASSERT_EQ(ZM_UI_TitleMenu::ItemIndexFromElementName(""), -1,
		"the empty sentinel is not a title item");
	ZENITH_ASSERT_EQ(ZM_UI_TitleMenu::ItemIndexFromElementName(ZM_UI_TitleMenu::szPANEL_NAME), -1,
		"the title panel is not selectable");
	ZENITH_ASSERT_EQ(ZM_UI_TitleMenu::ItemIndexFromElementName("Menu_TitleForeign"), -1,
		"a foreign authored name is not a title item");
}

ZENITH_TEST(ZM_Title, TitleMenu_ActionMapIsTotalByFocusedName)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_TitleMenu::ResolveAction(ZM_UI_TitleMenu::szCONTINUE_NAME),
		(u_int)ZM_TITLE_ACTION_OPEN_LOAD,
		"Continue resolves to the explicit OPEN_LOAD action");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_TitleMenu::ResolveAction(ZM_UI_TitleMenu::szNEW_GAME_NAME),
		(u_int)ZM_TITLE_ACTION_NEW_GAME,
		"New Game resolves to NEW_GAME");
	ZENITH_ASSERT_NE((u_int)ZM_TITLE_ACTION_OPEN_LOAD, (u_int)ZM_TITLE_ACTION_NEW_GAME,
		"Continue and New Game remain distinct actions");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_TitleMenu::ResolveAction(nullptr),
		(u_int)ZM_TITLE_ACTION_NONE, "null focus resolves to NONE");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_TitleMenu::ResolveAction(ZM_UI_TitleMenu::szPANEL_NAME),
		(u_int)ZM_TITLE_ACTION_NONE, "the panel resolves to NONE");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_TitleMenu::ResolveAction("Menu_TitleForeign"),
		(u_int)ZM_TITLE_ACTION_NONE, "a foreign control resolves to NONE");
}

ZENITH_TEST(ZM_Title, TitleMenu_AllEmptyHidesContinueAndFocusesNewGame)
{
	ZM_SAVE_SLOT_STATUS aeStatuses[uTITLE_STATUS_COUNT];
	FillStatuses(aeStatuses, ZM_SAVE_SLOT_EMPTY);

	ZM_UI_TitleMenu xTitle;
	xTitle.Open(aeStatuses, uTITLE_STATUS_COUNT);

	ZENITH_ASSERT_FALSE(xTitle.HasOccupiedSlot(),
		"four EMPTY slots mean there is nothing Continue may surface");
	ZENITH_ASSERT_FALSE(xTitle.HasReadySlot(), "four EMPTY slots contain no loadable save");
	ZENITH_ASSERT_FALSE(xTitle.IsItemVisible(ZM_TITLE_ITEM_CONTINUE),
		"Continue is hidden when every slot is EMPTY");
	ZENITH_ASSERT_FALSE(xTitle.IsItemFocusable(ZM_TITLE_ITEM_CONTINUE),
		"a hidden Continue control is absent from focus navigation");
	ZENITH_ASSERT_TRUE(xTitle.IsItemVisible(ZM_TITLE_ITEM_NEW_GAME),
		"New Game remains visible when no save exists");
	ZENITH_ASSERT_TRUE(xTitle.IsItemFocusable(ZM_TITLE_ITEM_NEW_GAME),
		"New Game remains focusable when no save exists");
	ZENITH_ASSERT_EQ((u_int)xTitle.GetDefaultFocusItem(), (u_int)ZM_TITLE_ITEM_NEW_GAME,
		"New Game owns initial focus when Continue is hidden");
	ZENITH_ASSERT_EQ((u_int)xTitle.ResolveConfirm(ZM_UI_TitleMenu::szCONTINUE_NAME),
		(u_int)ZM_TITLE_ACTION_NONE,
		"even a stale direct Continue name cannot dispatch while the control is hidden");
	ZENITH_ASSERT_EQ((u_int)xTitle.ResolveConfirm(ZM_UI_TitleMenu::szNEW_GAME_NAME),
		(u_int)ZM_TITLE_ACTION_NEW_GAME, "New Game remains live in the empty state");
}

ZENITH_TEST(ZM_Title, TitleMenu_DamagedOnlyStillSurfacesContinueWithoutClaimingReady)
{
	ZM_SAVE_SLOT_STATUS aeStatuses[uTITLE_STATUS_COUNT];
	FillStatuses(aeStatuses, ZM_SAVE_SLOT_EMPTY);
	aeStatuses[0] = ZM_SAVE_SLOT_DAMAGED;

	ZM_UI_TitleMenu xTitle;
	xTitle.Open(aeStatuses, uTITLE_STATUS_COUNT);

	ZENITH_ASSERT_TRUE(xTitle.HasOccupiedSlot(),
		"DAMAGED counts as occupied so Continue cannot disappear and invite a silent overwrite");
	ZENITH_ASSERT_FALSE(xTitle.HasReadySlot(),
		"a damaged-only snapshot must not claim that a loadable slot exists");
	ZENITH_ASSERT_TRUE(xTitle.IsItemVisible(ZM_TITLE_ITEM_CONTINUE),
		"Continue stays visible so the damaged row can be surfaced on the LOAD screen");
	ZENITH_ASSERT_TRUE(xTitle.IsItemFocusable(ZM_TITLE_ITEM_CONTINUE),
		"the visible Continue control participates in focus navigation");
	ZENITH_ASSERT_EQ((u_int)xTitle.GetDefaultFocusItem(), (u_int)ZM_TITLE_ITEM_CONTINUE,
		"Continue owns initial focus whenever any slot is occupied");
	ZENITH_ASSERT_EQ((u_int)xTitle.ResolveConfirm(ZM_UI_TitleMenu::szCONTINUE_NAME),
		(u_int)ZM_TITLE_ACTION_OPEN_LOAD,
		"damaged-only Continue opens LOAD so the damage is visible; it does not load the row");
}

ZENITH_TEST(ZM_Title, TitleMenu_DamagedFirstDoesNotHideLaterReadySlot)
{
	ZM_SAVE_SLOT_STATUS aeStatuses[uTITLE_STATUS_COUNT];
	FillStatuses(aeStatuses, ZM_SAVE_SLOT_EMPTY);
	aeStatuses[0] = ZM_SAVE_SLOT_DAMAGED;
	aeStatuses[2] = ZM_SAVE_SLOT_READY;

	ZM_UI_TitleMenu xTitle;
	xTitle.Open(aeStatuses, uTITLE_STATUS_COUNT);

	ZENITH_ASSERT_TRUE(xTitle.HasOccupiedSlot(), "the mixed snapshot is occupied");
	ZENITH_ASSERT_TRUE(xTitle.HasReadySlot(),
		"the READY slot after a DAMAGED slot is still discovered (the scan must not stop early)");
	ZENITH_ASSERT_TRUE(xTitle.IsItemVisible(ZM_TITLE_ITEM_CONTINUE),
		"a leading damaged slot never hides Continue from a later READY slot");
	ZENITH_ASSERT_TRUE(xTitle.IsItemFocusable(ZM_TITLE_ITEM_CONTINUE),
		"Continue stays navigable for the mixed damaged + ready snapshot");
	ZENITH_ASSERT_EQ((u_int)xTitle.ResolveConfirm(ZM_UI_TitleMenu::szCONTINUE_NAME),
		(u_int)ZM_TITLE_ACTION_OPEN_LOAD,
		"the mixed snapshot exposes the LOAD path that can select the later READY slot");
}

ZENITH_TEST(ZM_Title, TitleMenu_ReopenRefreshesAvailabilityAndNeverFocusesHiddenContinue)
{
	ZM_SAVE_SLOT_STATUS aeStatuses[uTITLE_STATUS_COUNT];
	FillStatuses(aeStatuses, ZM_SAVE_SLOT_EMPTY);
	aeStatuses[ZM_SAVE_SLOT_AUTO] = ZM_SAVE_SLOT_READY;

	ZM_UI_TitleMenu xTitle;
	xTitle.Open(aeStatuses, uTITLE_STATUS_COUNT);
	ZENITH_ASSERT_TRUE(xTitle.HasReadySlot(), "precondition: the first open sees READY Auto");
	ZENITH_ASSERT_EQ((u_int)xTitle.GetDefaultFocusItem(), (u_int)ZM_TITLE_ITEM_CONTINUE,
		"the first open focuses its available Continue control");

	FillStatuses(aeStatuses, ZM_SAVE_SLOT_EMPTY);
	xTitle.Open(aeStatuses, uTITLE_STATUS_COUNT);
	ZENITH_ASSERT_FALSE(xTitle.HasOccupiedSlot(),
		"reopening re-evaluates the new EMPTY snapshot instead of retaining stale availability");
	ZENITH_ASSERT_FALSE(xTitle.HasReadySlot(),
		"reopening clears the stale READY latch");
	ZENITH_ASSERT_FALSE(xTitle.IsItemVisible(ZM_TITLE_ITEM_CONTINUE),
		"Continue is hidden after the READY slot disappears");
	ZENITH_ASSERT_FALSE(xTitle.IsItemFocusable(ZM_TITLE_ITEM_CONTINUE),
		"the newly hidden Continue control is removed from navigation");
	ZENITH_ASSERT_EQ((u_int)xTitle.GetDefaultFocusItem(), (u_int)ZM_TITLE_ITEM_NEW_GAME,
		"focus is repaired onto New Game rather than left on hidden Continue");
	ZENITH_ASSERT_EQ((u_int)xTitle.ResolveConfirm(ZM_UI_TitleMenu::szCONTINUE_NAME),
		(u_int)ZM_TITLE_ACTION_NONE,
		"a stale Continue focus name is inert after refresh hides it");

	// A malformed refresh must also fail closed and clear old availability, never preserve
	// the previous snapshot merely because the caller supplied no status array.
	xTitle.Open(nullptr, 0u);
	ZENITH_ASSERT_FALSE(xTitle.HasOccupiedSlot(), "a null/empty snapshot folds to all EMPTY");
	ZENITH_ASSERT_EQ((u_int)xTitle.GetDefaultFocusItem(), (u_int)ZM_TITLE_ITEM_NEW_GAME,
		"the malformed refresh cannot resurrect focus on hidden Continue");
}
