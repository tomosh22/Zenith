#include "Zenith.h"

// ============================================================================
// ZM_Tests_SaveSlotScreen -- S7 item 2 SC4 unit tests for ZM_UI_SaveSlots, the
// by-value non-ECS presenter ZM_UI_MenuStack raises for BOTH the pause-menu SAVE
// flow and the (SC5) title-screen LOAD flow.
//
// WHAT THIS FILE PINS:
//   * The authored row-name contract (RowElementName <-> RowIndexFromElementName)
//     and the Cancel element, TOTAL over out-of-range and foreign names -- a
//     drift here silently misroutes a confirm to the WRONG slot.
//   * ResolveRowAction, the ONE total policy over MODE x SLOT x STATUS. It is
//     walked as a FULL CROSS PRODUCT and every expected action is recomputed
//     LONGHAND by ExpectedRowAction below -- NEVER by calling ResolveRowAction
//     itself, which would move consistently under any bug. The two contracts that
//     matter most: the manual flow may NEVER write Auto (SaveFormat.md:42-45), and
//     a READY / DAMAGED slot is overwritten only via the yes/no confirm, never
//     directly (SaveFormat.md, "Slot status and write semantics").
//   * FormatRowLabel writing into a caller buffer WITHOUT overflow and always
//     null-terminated, and reading DIFFERENTLY per status so a damaged slot cannot
//     masquerade as a good one.
//   * ZM-D-119: every row stays shown + focusable regardless of status
//     (RowIsAlwaysShown) so the authored nav links never point at a hidden target.
//   * Reset clearing mode + status + selection, so a batched test cannot inherit a
//     stale LOAD-mode presenter or stale slot statuses.
//
// PURITY / DISK. Most units here are PURE -- no ECS, no scene, no graphics. FOUR
// units (ResolveConfirmHonoursTheMode, ResolveConfirmOnADamagedRow...,
// ResetClearsModeStatusAndSelection, ModeSetterRejectsOutOfRange) drive the
// INSTANCE, whose per-row statuses come only from ZM_UI_SaveSlots::Open(), which
// probes disk (ZM_SaveSlots::ProbeSlot). Those four are wrapped in ZM_SlotDiskScope
// -- the SC2 idiom (Tests/ZM_Tests_SaveSlots.cpp) -- which redirects every slot
// name onto its "_Test" alias BEFORE any disk touch and deletes those files on
// entry AND exit, so they can never read, overwrite or delete the developer's real
// Save0/1/2/Auto .zsave. Boot-unit disk legality: Zenith_SaveData::Initialise runs
// inside Project_RegisterGameComponents(), which Zenith_Engine.cpp calls before
// RunAllTests() -- verified in SC2, no fallback needed. Category ZM_Save.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // near-miss ROOT names for the foreign-name fixtures
#include "Zenithmon/Source/Party/ZM_GameState.h"    // ZM_MakeStarterGameState (the disk fixtures)
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"

namespace
{
	// ---- the modes and the real slots, as literal tables (never derived) --------

	const ZM_SAVE_SCREEN_MODE aeALL_MODES[] =
	{
		ZM_SAVE_SCREEN_MODE_SAVE,
		ZM_SAVE_SCREEN_MODE_LOAD,
	};
	constexpr u_int uALL_MODE_COUNT = (u_int)(sizeof(aeALL_MODES) / sizeof(aeALL_MODES[0]));

	const ZM_SAVE_SLOT aeALL_SLOTS[] =
	{
		ZM_SAVE_SLOT_0,
		ZM_SAVE_SLOT_1,
		ZM_SAVE_SLOT_2,
		ZM_SAVE_SLOT_AUTO,
	};
	constexpr u_int uALL_SLOT_COUNT = (u_int)(sizeof(aeALL_SLOTS) / sizeof(aeALL_SLOTS[0]));

	// The MANUAL slots (Save0-2). Auto is deliberately excluded -- spelled as a
	// literal list, not computed from IsManualSlot, so this table is an independent
	// statement of SaveFormat.md:42-45.
	const ZM_SAVE_SLOT aeMANUAL_SLOTS[] =
	{
		ZM_SAVE_SLOT_0,
		ZM_SAVE_SLOT_1,
		ZM_SAVE_SLOT_2,
	};
	constexpr u_int uMANUAL_SLOT_COUNT =
		(u_int)(sizeof(aeMANUAL_SLOTS) / sizeof(aeMANUAL_SLOTS[0]));

	const ZM_SAVE_SLOT_STATUS aeALL_STATUSES[] =
	{
		ZM_SAVE_SLOT_EMPTY,
		ZM_SAVE_SLOT_READY,
		ZM_SAVE_SLOT_DAMAGED,
	};
	constexpr u_int uALL_STATUS_COUNT =
		(u_int)(sizeof(aeALL_STATUSES) / sizeof(aeALL_STATUSES[0]));

	// The player-facing status words. Pinned as LITERALS -- also the Scope.md:65-66
	// "original copy everywhere / no Nintendo text" guard on the label formatter.
	const char* StatusWord(ZM_SAVE_SLOT_STATUS eStatus)
	{
		switch (eStatus)
		{
		case ZM_SAVE_SLOT_EMPTY:   return "Empty";
		case ZM_SAVE_SLOT_READY:   return "Ready";
		case ZM_SAVE_SLOT_DAMAGED: return "Damaged";
		default:                   return "";
		}
	}

	// A denylist for the no-IP guard. If the label formatter ever grows hand-edited
	// franchise copy, this reds.
	const char* const aszFORBIDDEN_IP[] =
	{
		"Pokemon", "Pok\xC3\xA9mon", "Pok\xC3\xA9", "Nintendo", "Game Freak",
	};
	constexpr u_int uFORBIDDEN_IP_COUNT =
		(u_int)(sizeof(aszFORBIDDEN_IP) / sizeof(aszFORBIDDEN_IP[0]));

	bool Contains(const char* szHaystack, const char* szNeedle)
	{
		if (szHaystack == nullptr || szNeedle == nullptr) { return false; }
		return strstr(szHaystack, szNeedle) != nullptr;
	}

	// ---- The INDEPENDENT ResolveRowAction oracle --------------------------------
	// Recomputed LONGHAND from the documented policy, so it can never move in step
	// with a bug in the code under test:
	//   SAVE mode: only the manual slots (Save0-2) are writable; AUTO is NEVER
	//     manually writable (SaveFormat.md:42-45). EMPTY -> WRITE; a READY or DAMAGED
	//     slot -> CONFIRM_WRITE (overwrite only via the yes/no prompt; SaveFormat.md,
	//     "Slot status and write semantics").
	//   LOAD mode: any slot (manual OR auto) that is READY -> CONFIRM_LOAD; an EMPTY
	//     or DAMAGED slot is not loadable -> NONE.
	//   anything out of range (mode, slot or status) -> NONE.
	ZM_SAVE_ROW_ACTION ExpectedRowAction(ZM_SAVE_SCREEN_MODE eMode,
		ZM_SAVE_SLOT eSlot, ZM_SAVE_SLOT_STATUS eStatus)
	{
		if ((u_int)eSlot >= (u_int)ZM_SAVE_SLOT_COUNT) { return ZM_SAVE_ROW_ACTION_NONE; }
		if ((u_int)eStatus >= (u_int)ZM_SAVE_SLOT_STATUS_COUNT) { return ZM_SAVE_ROW_ACTION_NONE; }

		const bool bManual = (eSlot == ZM_SAVE_SLOT_0)
			|| (eSlot == ZM_SAVE_SLOT_1)
			|| (eSlot == ZM_SAVE_SLOT_2);

		if (eMode == ZM_SAVE_SCREEN_MODE_SAVE)
		{
			if (!bManual) { return ZM_SAVE_ROW_ACTION_NONE; }   // AUTO never manually writable
			if (eStatus == ZM_SAVE_SLOT_EMPTY) { return ZM_SAVE_ROW_ACTION_WRITE; }
			return ZM_SAVE_ROW_ACTION_CONFIRM_WRITE;            // READY or DAMAGED
		}
		if (eMode == ZM_SAVE_SCREEN_MODE_LOAD)
		{
			if (eStatus == ZM_SAVE_SLOT_READY) { return ZM_SAVE_ROW_ACTION_CONFIRM_LOAD; }
			return ZM_SAVE_ROW_ACTION_NONE;                     // EMPTY or DAMAGED not loadable
		}
		return ZM_SAVE_ROW_ACTION_NONE;                         // unknown mode
	}

	const char* ActionName(ZM_SAVE_ROW_ACTION eAction)
	{
		switch (eAction)
		{
		case ZM_SAVE_ROW_ACTION_NONE:          return "NONE";
		case ZM_SAVE_ROW_ACTION_WRITE:         return "WRITE";
		case ZM_SAVE_ROW_ACTION_CONFIRM_WRITE: return "CONFIRM_WRITE";
		case ZM_SAVE_ROW_ACTION_CONFIRM_LOAD:  return "CONFIRM_LOAD";
		default:                               return "<garbage>";
		}
	}

	// ---- disk plumbing for the four instance units (SC2 idiom) ------------------
#ifdef ZENITH_INPUT_SIMULATOR

	// True only when EVERY slot name is redirected off its shipping stem.
	bool TestSlotNamesAreActive()
	{
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
			const char* szShip = ZM_SaveSlots::SlotShippingName(aeALL_SLOTS[u]);
			if (szLive == nullptr || szShip == nullptr) { return false; }
			if (strcmp(szLive, szShip) == 0) { return false; }
		}
		return true;
	}

	// Redirect + entry/exit hygiene for every disk-touching unit. ORDER IS THE
	// SAFETY PROPERTY: turn the "_Test" redirection ON first, VERIFY it took, and
	// only then delete files -- so a broken redirection can never make the sweep
	// itself delete the player's real .zsave.
	struct ZM_SlotDiskScope
	{
		ZM_SlotDiskScope()
		{
			ZM_SaveSlots::SetTestSlotNamesForTests(true);
			m_bActive = TestSlotNamesAreActive();
			if (m_bActive)
			{
				ZM_SaveSlots::DeleteAllSlotsForTests();
				Zenith_SaveData::ClearForTest();
			}
		}
		~ZM_SlotDiskScope()
		{
			if (m_bActive)
			{
				ZM_SaveSlots::DeleteAllSlotsForTests();
				Zenith_SaveData::ClearForTest();
			}
			ZM_SaveSlots::SetTestSlotNamesForTests(false);
		}
		bool IsActive() const { return m_bActive; }
	private:
		bool m_bActive = false;
	};

	// HARD requirement, never a skip: a disk unit that cannot redirect its slot names
	// has zero coverage AND would address live files. Returns the verdict; the caller
	// MUST honour a false (the assert macros record-and-continue).
	bool RequireTestSlotNames(const ZM_SlotDiskScope& xScope, const char* szUnit)
	{
		ZENITH_ASSERT_TRUE(xScope.IsActive(),
			"%s: ZM_SaveSlots is still serving LIVE slot names inside a disk scope -- "
			"the unit refuses to touch the player's real .zsave files", szUnit);
		return xScope.IsActive();
	}

	void BuildSlotFilePath(ZM_SAVE_SLOT eSlot, char* szOut, size_t uOutSize)
	{
		snprintf(szOut, uOutSize, "%s%s%s", Zenith_SaveData::GetSaveDirectory(),
			ZM_SaveSlots::SlotName(eSlot), ZENITH_SAVE_EXT);
	}

	// Flip one byte inside the engine payload so LoadEx reports CORRUPT_DATA (the
	// slot probes DAMAGED) without disturbing the outer magic / format version.
	bool CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT eSlot)
	{
		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotFilePath(eSlot, szPath, sizeof(szPath));
		if (!Zenith_FileAccess::FileExists(szPath)) { return false; }
		uint64_t ulSize = 0u;
		char* pData = Zenith_FileAccess::ReadFile(szPath, ulSize);
		if (pData == nullptr) { return false; }
		if (ulSize <= (uint64_t)sizeof(Zenith_SaveFileHeader))
		{
			Zenith_FileAccess::FreeFileData(pData);
			return false;
		}
		Zenith_Vector<u_int8> xBytes;
		xBytes.Resize((u_int)ulSize);
		memcpy(xBytes.GetDataPointer(), pData, (size_t)ulSize);
		Zenith_FileAccess::FreeFileData(pData);
		xBytes.GetBack() = (u_int8)(xBytes.GetBack() ^ 0xffu);
		Zenith_FileAccess::WriteFile(szPath, xBytes.GetDataPointer(), (uint64_t)xBytes.GetSize());
		return true;
	}

	// Give row uRow of a fresh presenter a real on-disk status by writing / corrupting
	// the matching slot, then Open()-ing eMode so the probe fills m_aeStatus. Returns
	// false (with the reason logged) when the fixture could not be established, so the
	// caller can bail before asserting on a status it never actually got.
	bool WriteStarterSlot(ZM_SAVE_SLOT eSlot)
	{
		const ZM_GameState xState = ZM_MakeStarterGameState();
		const Zenith_Status xStatus = ZM_SaveSlots::WriteState(xState, eSlot);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(),
			"the disk fixture failed to write slot %u (error %u)",
			(u_int)eSlot, (u_int)xStatus.Error());
		if (!xStatus.IsOk()) { return false; }
		ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(eSlot), (u_int)ZM_SAVE_SLOT_READY,
			"the disk fixture slot %u must probe READY after a good write", (u_int)eSlot);
		return ZM_SaveSlots::ProbeSlot(eSlot) == ZM_SAVE_SLOT_READY;
	}

#endif // ZENITH_INPUT_SIMULATOR
}

// ============================================================================
// 1-5. The row-name / cancel contract (all PURE).
// ============================================================================

ZENITH_TEST(ZM_Save, SaveScreen_RowNamesAreDistinctAndRoundTrip)
{
	for (u_int u = 0u; u < ZM_UI_SaveSlots::uROW_COUNT; ++u)
	{
		const char* szName = ZM_UI_SaveSlots::RowElementName(u);
		ZENITH_ASSERT_NOT_NULL(szName, "row %u has a name", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "row %u's name is not the empty string", u);
		ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(szName), (int)u,
			"row %u round-trips through its element name", u);
		for (u_int v = 0u; v < u; ++v)
		{
			const char* szOther = ZM_UI_SaveSlots::RowElementName(v);
			if (szOther == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szName, szOther) != 0,
				"rows %u and %u must not share an element name '%s' (a shared name "
				"misroutes a confirm to the wrong slot)", u, v, szName);
		}
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_RowNameIsTotal)
{
	ZENITH_ASSERT_STREQ(ZM_UI_SaveSlots::RowElementName(ZM_UI_SaveSlots::uROW_COUNT), "",
		"the first out-of-range row has no element name");
	ZENITH_ASSERT_STREQ(ZM_UI_SaveSlots::RowElementName(9u), "",
		"a wildly out-of-range row has no element name (never a dangling pointer)");
}

ZENITH_TEST(ZM_Save, SaveScreen_RowIndexRejectsForeignNames)
{
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(nullptr), -1,
		"a null name is not a save row (never strcmp'd)");
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(""), -1,
		"the empty name is not a save row");
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(ZM_UI_MenuStack::szROOT_PARTY_NAME), -1,
		"a ROOT entry name is not a save row (the two contracts must not collide)");
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName("Menu_SaveRow9"), -1,
		"a one-past-the-end-looking row name resolves to no row (exact compare, not a prefix)");
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(ZM_UI_SaveSlots::szCANCEL_NAME), -1,
		"the Cancel element is not a save row -- Present relies on telling them apart");
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(ZM_UI_SaveSlots::szPANEL_NAME), -1,
		"the backing panel is not a save row");
}

ZENITH_TEST(ZM_Save, SaveScreen_CancelIsRecognisedAndIsNotARow)
{
	ZENITH_ASSERT_TRUE(ZM_UI_SaveSlots::IsCancelElementName(ZM_UI_SaveSlots::szCANCEL_NAME),
		"the authored Cancel element is recognised as Cancel");
	ZENITH_ASSERT_FALSE(ZM_UI_SaveSlots::IsCancelElementName(ZM_UI_SaveSlots::RowElementName(0u)),
		"row 0 is NOT the Cancel element (Escape on row 0 must never be read as Cancel, "
		"nor Cancel as saving over Slot 1)");
	ZENITH_ASSERT_FALSE(ZM_UI_SaveSlots::IsCancelElementName(nullptr),
		"a null focused name is not Cancel");
	ZENITH_ASSERT_FALSE(ZM_UI_SaveSlots::IsCancelElementName(ZM_UI_SaveSlots::szPANEL_NAME),
		"a foreign element is not Cancel");
	// The Cancel element must not resolve as a row either -- proven from the other side.
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::RowIndexFromElementName(ZM_UI_SaveSlots::szCANCEL_NAME), -1,
		"...and it is not a row");
}

ZENITH_TEST(ZM_Save, SaveScreen_RowCountEqualsSlotCount)
{
	// Pinned against a HAND-WRITTEN LITERAL, deliberately NOT the production constant.
	// uROW_COUNT IS static_cast<u_int>(ZM_SAVE_SLOT_COUNT) (ZM_UI_SaveSlots.h) and a
	// file-scope static_assert in ZM_UI_SaveSlots.cpp already pins that identity at compile
	// time, so asserting uROW_COUNT == ZM_SAVE_SLOT_COUNT would be the SAME expression on
	// both sides -- it can never fail at runtime. A literal 4 gives the unit independent
	// teeth: it reds when a slot is added or removed (ZM_SAVE_SLOT_COUNT changes, dragging
	// uROW_COUNT with it) so the screen's row count cannot silently follow the slot enum
	// without this test being revisited to confirm the screen really grew/shrank a row.
	ZENITH_ASSERT_EQ(ZM_UI_SaveSlots::uROW_COUNT, 4u,
		"the SAVE screen shows exactly four slot rows (Save0-2 + Auto)");

	// Row-naming boundary, exact at uROW_COUNT: the LAST in-range row carries a real name
	// while the first out-of-range row returns "" (empty, never null). Reds if
	// RowElementName's `uRow < uROW_COUNT` bound drifts (an off-by-one `<=` would index
	// past the literal name table and stop returning "" at uROW_COUNT) -- a drift
	// SaveScreen_RowNameIsTotal's fixed 9u probe sails clean over.
	const char* szLastRow = ZM_UI_SaveSlots::RowElementName(ZM_UI_SaveSlots::uROW_COUNT - 1u);
	ZENITH_ASSERT_NOT_NULL(szLastRow, "the last in-range row (uROW_COUNT-1) has a name");
	if (szLastRow != nullptr)
	{
		ZENITH_ASSERT_TRUE(szLastRow[0] != '\0',
			"the last in-range row (uROW_COUNT-1) has a NON-EMPTY element name");
	}
	ZENITH_ASSERT_STREQ(ZM_UI_SaveSlots::RowElementName(ZM_UI_SaveSlots::uROW_COUNT), "",
		"the first out-of-range row (uROW_COUNT) returns the empty string, never null");
}

// ============================================================================
// 6-16. ResolveRowAction: the total MODE x SLOT x STATUS policy (all PURE except
// 14 and 16, which drive the instance's disk-probed statuses).
// ============================================================================

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_SaveEmpty)
{
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_SAVE, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_EMPTY),
			(u_int)ZM_SAVE_ROW_ACTION_WRITE,
			"SAVE + an EMPTY manual slot writes straight away");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_SaveReady)
{
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_SAVE, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_READY),
			(u_int)ZM_SAVE_ROW_ACTION_CONFIRM_WRITE,
			"SAVE over a READY manual slot goes through the yes/no confirm first");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_SaveDamaged)
{
	// The load-bearing one: a DAMAGED slot may be OVERWRITTEN, but only via the
	// confirm -- never directly writable (SaveFormat.md, "Slot status and write semantics").
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_SAVE, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_DAMAGED),
			(u_int)ZM_SAVE_ROW_ACTION_CONFIRM_WRITE,
			"SAVE over a DAMAGED manual slot is a CONFIRM_WRITE, NEVER a direct WRITE");
		ZENITH_ASSERT_NE((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_SAVE, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_DAMAGED),
			(u_int)ZM_SAVE_ROW_ACTION_WRITE,
			"...and specifically not a direct WRITE that would stomp a recoverable file");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_LoadEmpty)
{
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_LOAD, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_EMPTY),
			(u_int)ZM_SAVE_ROW_ACTION_NONE,
			"LOAD of an EMPTY slot is not confirmable -- there is nothing to load");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_LoadReady)
{
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_LOAD, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_READY),
			(u_int)ZM_SAVE_ROW_ACTION_CONFIRM_LOAD,
			"LOAD of a READY slot confirms first (it discards live progress)");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowAction_LoadDamaged)
{
	// A damaged slot is never loaded (SaveFormat.md, "Slot status and write semantics") --
	// surfaced, not read.
	for (u_int u = 0u; u < uMANUAL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_LOAD, aeMANUAL_SLOTS[u], ZM_SAVE_SLOT_DAMAGED),
			(u_int)ZM_SAVE_ROW_ACTION_NONE,
			"LOAD of a DAMAGED slot is refused -- a damaged save is never loaded or reset");
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_AutoIsNeverManuallyWritable)
{
	// The manual flow may NEVER write Auto (SaveFormat.md:42-45). Reds if the slot
	// argument is dropped from ResolveRowAction.
	for (u_int u = 0u; u < uALL_STATUS_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
			ZM_SAVE_SCREEN_MODE_SAVE, ZM_SAVE_SLOT_AUTO, aeALL_STATUSES[u]),
			(u_int)ZM_SAVE_ROW_ACTION_NONE,
			"SAVE + the AUTO slot (status %s) is NEVER confirmable -- the manual flow "
			"must not be able to stomp the autosave slot", StatusWord(aeALL_STATUSES[u]));
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_AutoIsLoadableWhenReady)
{
	// Auto is WRITE-excluded, not blanket-excluded: a READY Auto save is loadable.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
		ZM_SAVE_SCREEN_MODE_LOAD, ZM_SAVE_SLOT_AUTO, ZM_SAVE_SLOT_READY),
		(u_int)ZM_SAVE_ROW_ACTION_CONFIRM_LOAD,
		"LOAD + a READY AUTO slot confirms a load (Auto is write-excluded, not "
		"load-excluded)");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
		ZM_SAVE_SCREEN_MODE_LOAD, ZM_SAVE_SLOT_AUTO, ZM_SAVE_SLOT_EMPTY),
		(u_int)ZM_SAVE_ROW_ACTION_NONE, "...but an EMPTY Auto slot has nothing to load");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
		ZM_SAVE_SCREEN_MODE_LOAD, ZM_SAVE_SLOT_AUTO, ZM_SAVE_SLOT_DAMAGED),
		(u_int)ZM_SAVE_ROW_ACTION_NONE, "...and a DAMAGED Auto slot is never loaded");
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveRowActionMatrixIsTotal)
{
	// The FULL cross product, including the NONE / out-of-range slot and every
	// status, each compared against the LONGHAND oracle. A new mode / slot / status
	// added without its arm reds here rather than returning garbage from a default.
	for (u_int uMode = 0u; uMode <= (u_int)ZM_SAVE_SCREEN_MODE_COUNT; ++uMode)
	{
		for (u_int uSlot = 0u; uSlot <= (u_int)ZM_SAVE_SLOT_COUNT; ++uSlot)
		{
			for (u_int uStatus = 0u; uStatus <= (u_int)ZM_SAVE_SLOT_STATUS_COUNT; ++uStatus)
			{
				const ZM_SAVE_SCREEN_MODE eMode = (ZM_SAVE_SCREEN_MODE)uMode;
				const ZM_SAVE_SLOT eSlot = (ZM_SAVE_SLOT)uSlot;
				const ZM_SAVE_SLOT_STATUS eStatus = (ZM_SAVE_SLOT_STATUS)uStatus;
				const ZM_SAVE_ROW_ACTION eActual =
					ZM_UI_SaveSlots::ResolveRowAction(eMode, eSlot, eStatus);
				ZENITH_ASSERT_TRUE((u_int)eActual < (u_int)ZM_SAVE_ROW_ACTION_COUNT,
					"ResolveRowAction(mode=%u slot=%u status=%u) returned an out-of-range "
					"action %u (a missing default arm)", uMode, uSlot, uStatus, (u_int)eActual);
				const ZM_SAVE_ROW_ACTION eExpected = ExpectedRowAction(eMode, eSlot, eStatus);
				ZENITH_ASSERT_EQ((u_int)eActual, (u_int)eExpected,
					"ResolveRowAction(mode=%u slot=%u status=%u) = %s, expected %s",
					uMode, uSlot, uStatus, ActionName(eActual), ActionName(eExpected));
			}
		}
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveConfirmHonoursTheMode)
{
	// The single unit proving the INSTANCE mode does anything: the SAME manual row
	// -- a READY Slot 2 (row 1) -- resolves CONFIRM_WRITE in SAVE mode and
	// CONFIRM_LOAD in LOAD mode. Reds if m_eMode is unread.
#ifdef ZENITH_INPUT_SIMULATOR
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "SaveScreen_ResolveConfirmHonoursTheMode")) { return; }
	if (!WriteStarterSlot(ZM_SAVE_SLOT_1)) { return; }

	const char* szRow1 = ZM_UI_SaveSlots::RowElementName(1u);
	ZENITH_ASSERT_NOT_NULL(szRow1, "row 1 has an element name");
	if (szRow1 == nullptr) { return; }

	ZM_UI_SaveSlots xScreen;
	xScreen.Open(ZM_SAVE_SCREEN_MODE_SAVE);
	ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(1u), (u_int)ZM_SAVE_SLOT_READY,
		"precondition: the written slot 1 probes READY through Open()");

	ZM_SAVE_SLOT eSaveSlot = ZM_SAVE_SLOT_NONE;
	const ZM_SAVE_ROW_ACTION eSaveAction = xScreen.ResolveConfirm(szRow1, eSaveSlot);
	ZENITH_ASSERT_EQ((u_int)eSaveAction, (u_int)ZM_SAVE_ROW_ACTION_CONFIRM_WRITE,
		"a READY row confirms as CONFIRM_WRITE in SAVE mode");
	ZENITH_ASSERT_EQ((u_int)eSaveSlot, (u_int)ZM_SAVE_SLOT_1,
		"...and it names the row's own slot");

	xScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
	ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(1u), (u_int)ZM_SAVE_SLOT_READY,
		"the re-probe still sees the READY slot after switching to LOAD mode");
	ZM_SAVE_SLOT eLoadSlot = ZM_SAVE_SLOT_NONE;
	const ZM_SAVE_ROW_ACTION eLoadAction = xScreen.ResolveConfirm(szRow1, eLoadSlot);
	ZENITH_ASSERT_EQ((u_int)eLoadAction, (u_int)ZM_SAVE_ROW_ACTION_CONFIRM_LOAD,
		"the SAME row confirms as CONFIRM_LOAD in LOAD mode -- so the mode genuinely "
		"changes the meaning of a confirm");
	ZENITH_ASSERT_EQ((u_int)eLoadSlot, (u_int)ZM_SAVE_SLOT_1, "...still naming its own slot");

	ZENITH_ASSERT_NE((u_int)eSaveAction, (u_int)eLoadAction,
		"the two modes must NOT resolve the same READY row identically");
#endif
}

ZENITH_TEST(ZM_Save, SaveScreen_ResolveConfirmOnADamagedRowDoesNotMutateTheModel)
{
	// The refusal path for a DAMAGED slot in LOAD mode is NONE. Confirming it must
	// NOT clear or reset the model's view of that slot (SaveFormat.md,
	// "Slot status and write semantics").
#ifdef ZENITH_INPUT_SIMULATOR
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope,
		"SaveScreen_ResolveConfirmOnADamagedRowDoesNotMutateTheModel")) { return; }
	if (!WriteStarterSlot(ZM_SAVE_SLOT_1)) { return; }
	ZENITH_ASSERT_TRUE(CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT_1),
		"the fixture corrupts the written slot 1 on disk");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1), (u_int)ZM_SAVE_SLOT_DAMAGED,
		"precondition: the corrupted slot 1 probes DAMAGED");
	if (ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1) != ZM_SAVE_SLOT_DAMAGED) { return; }

	const char* szRow1 = ZM_UI_SaveSlots::RowElementName(1u);
	if (szRow1 == nullptr) { return; }

	ZM_UI_SaveSlots xScreen;
	xScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
	ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(1u), (u_int)ZM_SAVE_SLOT_DAMAGED,
		"precondition: the presenter sees row 1 as DAMAGED");

	// Snapshot the whole stored-status view BEFORE the confirm.
	ZM_SAVE_SLOT_STATUS aeBefore[ZM_UI_SaveSlots::uROW_COUNT];
	for (u_int u = 0u; u < ZM_UI_SaveSlots::uROW_COUNT; ++u)
	{
		aeBefore[u] = xScreen.GetRowStatus(u);
	}

	ZM_SAVE_SLOT eSlot = ZM_SAVE_SLOT_1;
	const ZM_SAVE_ROW_ACTION eAction = xScreen.ResolveConfirm(szRow1, eSlot);
	ZENITH_ASSERT_EQ((u_int)eAction, (u_int)ZM_SAVE_ROW_ACTION_NONE,
		"a DAMAGED row in LOAD mode is refused (NONE), never loaded");

	// ...and the refusal changed NOTHING about the model's view of the slots.
	for (u_int u = 0u; u < ZM_UI_SaveSlots::uROW_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(u), (u_int)aeBefore[u],
			"ResolveConfirm must not mutate row %u's stored status (the damaged slot is "
			"surfaced, never reset)", u);
	}
	// The slot on disk is likewise untouched -- still DAMAGED, not deleted or repaired.
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1), (u_int)ZM_SAVE_SLOT_DAMAGED,
		"the DAMAGED file on disk is left exactly as it was");
#endif
}

// ============================================================================
// 17-20. FormatRowLabel (all PURE).
// ============================================================================

ZENITH_TEST(ZM_Save, SaveScreen_FormatRowLabelIsNullTerminatedWithinATinyCapacity)
{
	// Over all 12 slot x status pairs at capacities 1, 4 and 64, each with guard
	// bytes past the capacity. Reds on an unbounded sprintf.
	const u_int auCAPS[] = { 1u, 4u, 64u };
	constexpr u_int uCAP_COUNT = (u_int)(sizeof(auCAPS) / sizeof(auCAPS[0]));
	constexpr char cGUARD = (char)(unsigned char)0xABu; // via unsigned char: direct (char)0xAB is a truncating cast (C4310)
	constexpr u_int uBUF = 96u;

	for (u_int uS = 0u; uS < uALL_SLOT_COUNT; ++uS)
	{
		for (u_int uT = 0u; uT < uALL_STATUS_COUNT; ++uT)
		{
			for (u_int uC = 0u; uC < uCAP_COUNT; ++uC)
			{
				const u_int uCap = auCAPS[uC];
				char aBuf[uBUF];
				memset(aBuf, cGUARD, sizeof(aBuf));

				ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], aeALL_STATUSES[uT], aBuf, uCap);

				// A null terminator sits somewhere within [0, uCap).
				bool bNul = false;
				for (u_int u = 0u; u < uCap; ++u)
				{
					if (aBuf[u] == '\0') { bNul = true; break; }
				}
				ZENITH_ASSERT_TRUE(bNul,
					"FormatRowLabel(slot %u status %s) left no null terminator inside "
					"capacity %u", uS, StatusWord(aeALL_STATUSES[uT]), uCap);

				// Nothing at or past the capacity was touched.
				for (u_int u = uCap; u < uBUF; ++u)
				{
					ZENITH_ASSERT_TRUE(aBuf[u] == cGUARD,
						"FormatRowLabel(slot %u status %s) overran capacity %u at byte %u",
						uS, StatusWord(aeALL_STATUSES[uT]), uCap, u);
				}
			}
		}
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_FormatRowLabelDiffersPerStatus)
{
	// The three status labels for a slot must be pairwise distinct -- otherwise a
	// player cannot tell a DAMAGED slot from a good one and overwrites it. Reds if
	// the DAMAGED case falls through to the READY formatter.
	constexpr u_int uBUF = 96u;
	for (u_int uS = 0u; uS < uALL_SLOT_COUNT; ++uS)
	{
		char aEmpty[uBUF];
		char aReady[uBUF];
		char aDamaged[uBUF];
		ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], ZM_SAVE_SLOT_EMPTY, aEmpty, uBUF);
		ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], ZM_SAVE_SLOT_READY, aReady, uBUF);
		ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], ZM_SAVE_SLOT_DAMAGED, aDamaged, uBUF);

		ZENITH_ASSERT_TRUE(strcmp(aEmpty, aReady) != 0,
			"slot %u reads the SAME empty and ready ('%s')", uS, aEmpty);
		ZENITH_ASSERT_TRUE(strcmp(aEmpty, aDamaged) != 0,
			"slot %u reads the SAME empty and damaged ('%s')", uS, aEmpty);
		ZENITH_ASSERT_TRUE(strcmp(aReady, aDamaged) != 0,
			"slot %u reads the SAME ready and damaged ('%s' vs '%s') -- a player cannot "
			"tell them apart", uS, aReady, aDamaged);
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_FormatRowLabelNamesTheSlot)
{
	// The label carries the slot's own display name and its status word. Reds if
	// SlotDisplayName is not used, or if the status word is missing.
	constexpr u_int uBUF = 96u;
	for (u_int uS = 0u; uS < uALL_SLOT_COUNT; ++uS)
	{
		const char* szDisplay = ZM_SaveSlots::SlotDisplayName(aeALL_SLOTS[uS]);
		ZENITH_ASSERT_NOT_NULL(szDisplay, "slot %u has a display name", uS);
		if (szDisplay == nullptr || szDisplay[0] == '\0') { continue; }
		for (u_int uT = 0u; uT < uALL_STATUS_COUNT; ++uT)
		{
			char aBuf[uBUF];
			ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], aeALL_STATUSES[uT], aBuf, uBUF);
			ZENITH_ASSERT_TRUE(Contains(aBuf, szDisplay),
				"the label for slot %u names it '%s' ('%s')", uS, szDisplay, aBuf);
			ZENITH_ASSERT_TRUE(Contains(aBuf, StatusWord(aeALL_STATUSES[uT])),
				"...and carries the '%s' status ('%s')", StatusWord(aeALL_STATUSES[uT]), aBuf);
		}
	}
}

ZENITH_TEST(ZM_Save, SaveScreen_LabelsCarryNoNintendoIp)
{
	// Player-facing copy must be original (Scope.md:65-66). Each label must contain
	// only its own display name + the expected status word, and none of a denylist.
	constexpr u_int uBUF = 96u;
	for (u_int uS = 0u; uS < uALL_SLOT_COUNT; ++uS)
	{
		const char* szDisplay = ZM_SaveSlots::SlotDisplayName(aeALL_SLOTS[uS]);
		for (u_int uT = 0u; uT < uALL_STATUS_COUNT; ++uT)
		{
			char aBuf[uBUF];
			ZM_UI_SaveSlots::FormatRowLabel(aeALL_SLOTS[uS], aeALL_STATUSES[uT], aBuf, uBUF);
			for (u_int uF = 0u; uF < uFORBIDDEN_IP_COUNT; ++uF)
			{
				ZENITH_ASSERT_FALSE(Contains(aBuf, aszFORBIDDEN_IP[uF]),
					"the label '%s' must not carry the franchise term '%s'",
					aBuf, aszFORBIDDEN_IP[uF]);
			}
			// The status word this label must NOT carry either (a mis-mapped status
			// would carry a neighbour's word -- also the anti-IP allowlist bite).
			for (u_int uOther = 0u; uOther < uALL_STATUS_COUNT; ++uOther)
			{
				if (uOther == uT) { continue; }
				ZENITH_ASSERT_FALSE(Contains(aBuf, StatusWord(aeALL_STATUSES[uOther])),
					"the label for slot %u status %s must not carry the '%s' word ('%s')",
					uS, StatusWord(aeALL_STATUSES[uT]), StatusWord(aeALL_STATUSES[uOther]), aBuf);
			}
			if (szDisplay != nullptr && szDisplay[0] != '\0')
			{
				ZENITH_ASSERT_TRUE(Contains(aBuf, szDisplay),
					"...and it is one of the four allowlisted slot names ('%s')", aBuf);
			}
		}
	}
}

// ============================================================================
// 21-23. Reset, the mode setter, and the ZM-D-119 focusability contract.
// ============================================================================

ZENITH_TEST(ZM_Save, SaveScreen_ResetClearsModeStatusAndSelection)
{
	// A fresh presenter opens in SAVE mode with EMPTY statuses and no selection; a
	// mid-flight LOAD-mode presenter with a READY row must return to exactly that on
	// Reset, or a batched test inherits the wrong mode + stale statuses.
	ZM_UI_SaveSlots xFresh;
	ZENITH_ASSERT_EQ((u_int)xFresh.GetMode(), (u_int)ZM_SAVE_SCREEN_MODE_SAVE,
		"a fresh presenter defaults to SAVE mode");
	ZENITH_ASSERT_EQ(xFresh.GetSelectedRow(), -1, "...with no row selected");

#ifdef ZENITH_INPUT_SIMULATOR
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "SaveScreen_ResetClearsModeStatusAndSelection")) { return; }
	if (!WriteStarterSlot(ZM_SAVE_SLOT_1)) { return; }

	ZM_UI_SaveSlots xScreen;
	xScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
	ZENITH_ASSERT_EQ((u_int)xScreen.GetMode(), (u_int)ZM_SAVE_SCREEN_MODE_LOAD,
		"the fixture really is mid-flight in LOAD mode");
	ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(1u), (u_int)ZM_SAVE_SLOT_READY,
		"...with a READY status the reset must clear");

	xScreen.Reset();
	ZENITH_ASSERT_EQ((u_int)xScreen.GetMode(), (u_int)ZM_SAVE_SCREEN_MODE_SAVE,
		"Reset returns the mode to SAVE");
	ZENITH_ASSERT_EQ(xScreen.GetSelectedRow(), -1, "Reset clears the selection");
	for (u_int u = 0u; u < ZM_UI_SaveSlots::uROW_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)xScreen.GetRowStatus(u), (u_int)ZM_SAVE_SLOT_EMPTY,
			"Reset clears row %u's stored status back to EMPTY", u);
	}
#endif
}

ZENITH_TEST(ZM_Save, SaveScreen_ModeSetterRejectsOutOfRangeToExactlySave)
{
	// Open must not store a raw out-of-range mode (which would send ResolveRowAction
	// to its default arm), preserve a prior LOAD mode, or clamp down to LOAD. Every
	// invalid representation folds to EXACTLY SAVE.
#ifdef ZENITH_INPUT_SIMULATOR
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "SaveScreen_ModeSetterRejectsOutOfRangeToExactlySave")) { return; }

	ZM_UI_SaveSlots xScreen;
	// Start in LOAD so an invalid Open cannot pass by merely preserving the old mode.
	xScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
	ZENITH_ASSERT_EQ((u_int)xScreen.GetMode(), (u_int)ZM_SAVE_SCREEN_MODE_LOAD,
		"precondition: the presenter can enter LOAD before the invalid-mode probes");

	const ZM_SAVE_SCREEN_MODE aeInvalidModes[] =
	{
		ZM_SAVE_SCREEN_MODE_COUNT,
		(ZM_SAVE_SCREEN_MODE)((u_int)ZM_SAVE_SCREEN_MODE_COUNT + 1u),
		(ZM_SAVE_SCREEN_MODE)99u,
		(ZM_SAVE_SCREEN_MODE)0xFFFFFFFFu,
	};
	for (u_int u = 0u; u < (u_int)(sizeof(aeInvalidModes) / sizeof(aeInvalidModes[0])); ++u)
	{
		xScreen.Open(aeInvalidModes[u]);
		ZENITH_ASSERT_EQ((u_int)xScreen.GetMode(), (u_int)ZM_SAVE_SCREEN_MODE_SAVE,
			"invalid mode probe %u folds to EXACTLY SAVE (LOAD is not a valid fallback)", u);
		// Re-arm LOAD between probes so a no-op invalid setter cannot inherit SAVE from
		// the previous iteration and accidentally make the next assertion green.
		xScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
	}
#endif
}

ZENITH_TEST(ZM_Save, SaveScreen_EveryRowStaysFocusableRegardlessOfStatus)
{
	// ZM-D-119: every save-slot row stays VISIBLE + FOCUSABLE regardless of status,
	// so the authored SetNavigation links never point at a hidden target (which is
	// dropped with NO spatial fallback and silently swallows the press). A
	// non-confirmable row is disarmed by ResolveRowAction returning NONE, NEVER by
	// hiding the row.
	ZENITH_ASSERT_TRUE(ZM_UI_SaveSlots::RowIsAlwaysShown(),
		"every row must stay shown+focusable no matter its status -- hiding empty rows "
		"would leave the authored nav links pointing at hidden targets (ZM-D-119)");

	// Prove the DISARM is done by the ACTION, not by hiding: an EMPTY slot in LOAD
	// mode and a DAMAGED slot in LOAD mode are both non-confirmable (NONE), yet the
	// row is still shown -- exactly the states a naive "hide the useless rows"
	// optimisation would remove.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_SaveSlots::ResolveRowAction(
		ZM_SAVE_SCREEN_MODE_LOAD, ZM_SAVE_SLOT_0, ZM_SAVE_SLOT_EMPTY),
		(u_int)ZM_SAVE_ROW_ACTION_NONE,
		"an empty LOAD row is disarmed by the ACTION (NONE), while RowIsAlwaysShown "
		"keeps it on screen and focusable");
	ZENITH_ASSERT_TRUE(ZM_UI_SaveSlots::RowIsAlwaysShown(),
		"...and that disarm never turns into hiding the row");
}
