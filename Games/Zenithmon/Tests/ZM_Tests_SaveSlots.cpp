#include "Zenith.h"

// ============================================================================
// ZM_Tests_SaveSlots -- S7 item 2 SC2 contract for the typed slot/disk layer
// that sits OVER the frozen ZM_SaveSchema codec (ZM-D-136).
//
// What this file pins, and why each part exists:
//
//   * The four slot roles (Save0-2 manual + Auto milestone) map to four
//     DISTINCT file names, and every name map is TOTAL -- an out-of-range id
//     can never reach Zenith_SaveData::BuildSlotPath, which performs ZERO name
//     sanitisation (Zenith_SaveData.cpp:95-98).
//   * The 4-byte little-endian LENGTH PREFIX that this layer writes INSIDE the
//     engine payload, immediately before the ZMSV blob. It exists because
//     ZM_SaveSchema::Read demands an EXACT ulByteLength and the engine's two
//     Load paths DISAGREE about GetCapacity(): the disk path wraps an exactly-
//     payload-sized buffer (Zenith_SaveData.cpp:317-319) while the staged
//     readback path hands the callback a default-constructed OWNING stream whose
//     capacity is the 1024-byte ALLOCATION (Zenith_SaveData.cpp:229-235).
//     Slot_RecordedPayloadIsPrefixPlusBlob / Slot_PrefixIsLittleEndian /
//     Slot_StagedReadbackDecodesWithoutTouchingDisk are the ONLY guards on that
//     trap; the last one is deliberately driven through the REAL
//     Zenith_SaveData readback seam, never a hand-rolled buffer, because a
//     hand-rolled buffer would bypass the very capacity ambiguity it defends.
//   * That a save is proven by a RE-PROBE and never by a return value:
//     Zenith_SaveData::Save returns true UNCONDITIONALLY (Zenith_SaveData.cpp:204).
//   * That a DAMAGED slot is reported and left EXACTLY as it is
//     (Docs/SaveFormat.md:318-321) -- never repaired, deleted or overwritten.
//   * That the four write failure modes are DISTINGUISHABLE, not merely detected.
//
// Boot-unit legality: Zenith_SaveData::Initialise("Zenithmon") runs inside
// Project_RegisterGameComponents() (Zenithmon.cpp:1208, the Initialise call at
// :1238), which Zenith_Engine.cpp:741 calls BEFORE RunAllTests() at :765. Disk-
// touching boot units are therefore legal and need no init fallback. Initialise
// also create_directories() the save folder (Zenith_SaveData.cpp:119-122), so the
// hand-built-file units below can write into it whatever order the runner picks.
//
// ---------------------------------------------------------------------------
// DISK SAFETY (read before editing any unit below)
// ---------------------------------------------------------------------------
// ZENITH_TESTs run at EVERY boot -- the CI unit gate boots with
// `--headless --exit-after-frames 120` (Tools/run_unit_gate.ps1) and a developer
// simply running the game passes no flags at all. NEITHER sets
// Zenith_CommandLine::IsAutomatedTestRun(), and the one command that DOES pass
// --all-automated-tests (`zenith test <Game>`) also passes --skip-unit-tests, so
// this suite never runs there at all. If the "_Test" name indirection were keyed on
// that flag alone it would be OFF in every run that can actually execute these
// units, which leaves exactly two outcomes: skip everything (a skip counts as a
// PASS in the gate, so EVERY unit below that constructs a ZM_SlotDiskScope -- 27 of
// the 33 in this file as it stands, i.e. all but the six pure name/policy units --
// would be green-by-skip forever and the whole storage half of this SC would have
// zero live coverage), or delete and overwrite
// the player's real Save0/1/2/Auto .zsave on every boot -- permanently, because
// Zenith_SaveData::ClearForTest does NOT delete disk files (Zenith_SaveData.h:119).
//
// The interlock is therefore EXPLICIT, never inferred from a process flag:
// ZM_SlotDiskScope calls ZM_SaveSlots::SetTestSlotNamesForTests(true) as the very
// FIRST thing its constructor does, VERIFIES that all four names actually moved off
// their shipping stems BEFORE it touches disk at all, and clears the redirection in
// its destructor. Every disk unit constructs the scope BEFORE any slot name is
// resolved and then HARD-ASSERTS (never skips) that the redirection is live. A unit
// that cannot redirect is a FAILED unit, because a silent skip is what let this
// whole file be green-by-skip in the first place.
//
// WHAT EACH DISK UNIT LEAVES BEHIND: nothing. Every one of them is wrapped in
// ZM_SlotDiskScope, which calls ZM_SaveSlots::DeleteAllSlotsForTests() +
// Zenith_SaveData::ClearForTest() at BOTH entry and exit -- so on exit the four
// files Save0_Test.zsave / Save1_Test.zsave / Save2_Test.zsave / Auto_Test.zsave
// are gone from %APPDATA%/Zenith/Zenithmon/ and the in-memory write log + readback
// stash are empty. Entry hygiene is what makes a rerun on a DIRTY machine
// deterministic (a previous crashed run may have left files, and a developer's real
// saves may be sitting alongside -- those are never addressed and never touched);
// exit hygiene is what stops this suite leaking a file into the next unit or the
// next process. No unit assumes any pre-existing file: each one establishes the
// disk state it needs after the scope has cleared it. The shipping-named
// Save0/1/2/Auto .zsave files are never opened, written or deleted by any unit here.
// ============================================================================

#include <cstdio>
#include <cstring>
#include <vector>

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_ErrorCode.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"

namespace
{
	// ---- literal expectations -------------------------------------------------
	// Spelled as LITERALS, never derived from the production tables: a derived
	// expectation moves with the code under test and can never fail.

	const ZM_SAVE_SLOT aeALL_SLOTS[] =
	{
		ZM_SAVE_SLOT_0,
		ZM_SAVE_SLOT_1,
		ZM_SAVE_SLOT_2,
		ZM_SAVE_SLOT_AUTO,
	};
	constexpr u_int uALL_SLOT_COUNT = (u_int)(sizeof(aeALL_SLOTS) / sizeof(aeALL_SLOTS[0]));

	// The ON-DISK file stems. These are a compatibility surface: renaming one
	// orphans every existing save, and aliasing two makes Save1 silently overwrite
	// Save0.
	const char* const aszEXPECTED_SHIPPING_NAMES[uALL_SLOT_COUNT] =
	{
		"Save0",
		"Save1",
		"Save2",
		"Auto",
	};

	// The expected "_Test" aliases, spelled as LITERALS. Never derived by appending a
	// suffix to the shipping table -- a derived expectation moves with the production
	// table and can never fail.
	const char* const aszEXPECTED_TEST_NAMES[uALL_SLOT_COUNT] =
	{
		"Save0_Test",
		"Save1_Test",
		"Save2_Test",
		"Auto_Test",
	};

	// Player-facing copy. Pinned literally, which is also the Docs/Scope.md
	// "original names everywhere / no Nintendo text" guard on UI strings.
	const char* const aszEXPECTED_DISPLAY_NAMES[uALL_SLOT_COUNT] =
	{
		"Slot 1",
		"Slot 2",
		"Slot 3",
		"Auto",
	};

	// The frozen v1 golden payload length (Tests/ZM_Tests_SaveMigration.cpp:19,75).
	// A LITERAL on purpose -- the whole point of tests 16/17 is that the framing
	// carries a known blob length, not "whatever the codec happened to emit".
	constexpr u_int uGOLDEN_BLOB_BYTES = 824u;

	// The prefix width, as a literal. ZM_SaveSlots::uLENGTH_PREFIX_BYTES is
	// asserted AGAINST this, never used in place of it.
	constexpr u_int uEXPECTED_PREFIX_BYTES = 4u;

	// An out-of-range slot id that is not NONE either.
	constexpr u_int uGARBAGE_SLOT_ID = 99u;

	// Everything from here to the matching #endif exists only for the disk-touching
	// units, which need Zenith_SaveData's ZENITH_INPUT_SIMULATOR instrumentation
	// (DeleteAllSlotsForTests / GetWrittenSlotsForTest / SetReadbackForTest /
	// ClearForTest). Scoped together so no helper is left unreferenced when that
	// instrumentation is compiled out.
#ifdef ZENITH_INPUT_SIMULATOR

	// True only when EVERY slot name is indirected away from its shipping name, so
	// the disk units cannot possibly address a live save file.
	bool TestSlotNamesAreActive()
	{
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
			const char* szShipping = ZM_SaveSlots::SlotShippingName(aeALL_SLOTS[u]);
			if (szLive == nullptr || szShipping == nullptr) { return false; }
			if (strcmp(szLive, szShipping) == 0) { return false; }
		}
		return true;
	}

	void AssertStatus(const Zenith_Status& xStatus, Zenith_ErrorCode eExpected,
		const char* szContext)
	{
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)eExpected,
			"%s returned the wrong status", szContext);
	}

	// ---- pure fixtures ---------------------------------------------------------

	void SetNickname(ZM_Monster& xMonster, const char* szNickname)
	{
		memset(xMonster.m_szNickname, 0, sizeof(xMonster.m_szNickname));
		const size_t uLength = (std::min)(strlen(szNickname),
			(size_t)uZM_MONSTER_NICKNAME_CAPACITY - 1u);
		memcpy(xMonster.m_szNickname, szNickname, uLength);
	}

	ZM_Monster MakeMonster(ZM_SPECIES_ID eSpecies, u_int uLevel, const char* szNickname)
	{
		ZM_Monster xMonster = ZM_BuildMonsterRecord(eSpecies, uLevel);
		SetNickname(xMonster, szNickname);
		return xMonster;
	}

	// The exact state behind the frozen 824-byte v1 golden
	// (Tests/ZM_Tests_SaveMigration.cpp:77-123). Duplicated rather than shared
	// because Tests/ is .cpp-only with no shared header. If the golden ever moves,
	// this copy and that one must move together -- and ZM_SaveSchema is FROZEN, so
	// neither should ever move at all.
	ZM_GameState MakeGoldenV1State()
	{
		ZM_GameState xState;
		ZM_Monster xMonster;
		xMonster.m_eSpecies = ZM_SPECIES_FERNFAWN;
		xMonster.m_uLevel = 1u;
		xMonster.m_uCurrentExp = 0u;
		xMonster.m_auIV[0] = 0u;
		xMonster.m_auIV[1] = 1u;
		xMonster.m_auIV[2] = 2u;
		xMonster.m_auIV[3] = 3u;
		xMonster.m_auIV[4] = 4u;
		xMonster.m_auIV[5] = 5u;
		xMonster.m_auEV[0] = 6u;
		xMonster.m_auEV[1] = 7u;
		xMonster.m_auEV[2] = 8u;
		xMonster.m_auEV[3] = 9u;
		xMonster.m_auEV[4] = 10u;
		xMonster.m_auEV[5] = 11u;
		xMonster.m_eNature = ZM_NATURE_FERAL;
		xMonster.m_eAbility = ZM_ABILITY_GRAZER;
		xMonster.m_eStatus = ZM_MAJOR_STATUS_POISON;
		for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
		{
			xMonster.m_axMoves[uMove].m_eMove = ZM_MOVE_NONE;
			xMonster.m_axMoves[uMove].m_uCurPP = 0u;
			xMonster.m_axMoves[uMove].m_uMaxPP = 0u;
		}
		xMonster.m_uCurrentHp = 0u;
		xMonster.m_eGender = ZM_GENDER_FEMALE;
		xMonster.m_uFriendship = 0x7Fu;
		xMonster.m_uFlags = uZM_MONSTER_FLAG_IS_SHINY;
		xMonster.m_szNickname[0] = 'A';

		xState.m_xParty.Add(xMonster);
		xState.MarkCaught(ZM_SPECIES_FERNFAWN);
		xState.m_xStoryFlags.Set(9u, true);
		xState.m_uBadgeMask = 0x81u;
		xState.m_xBag.Add(ZM_ITEM_CATCHORB, 2u);
		xState.m_uMoney = 0x12345678u;
		xState.m_xTowerRun.m_uCurrentStreak = 1u;
		xState.m_xTowerRun.m_uBestStreak = 2u;
		xState.m_xTowerRun.m_ulSeed = 0x0102030405060708ull;
		xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		return xState;
	}

	// A state carrying a NON-DEFAULT value in ALL ELEVEN modules, so a framing bug
	// that silently drops one module is visible. uSalt perturbs every module so two
	// calls with different salts are distinguishable field-for-field (used by the
	// per-slot-independence units).
	ZM_GameState MakeMaximalState(u_int uSalt)
	{
		ZM_GameState xState;

		// 1 party -- renamed, levelled, damaged, statused, shiny, spent PP.
		ZM_Monster xLead = MakeMonster(ZM_SPECIES_FERNFAWN, 17u + uSalt, "Rift");
		xLead.m_eStatus = ZM_MAJOR_STATUS_POISON;
		xLead.m_uFlags |= uZM_MONSTER_FLAG_IS_SHINY;
		xLead.m_uFriendship = 200u + uSalt;
		xLead.m_uCurrentHp = 1u + uSalt;
		if (xLead.m_axMoves[0].m_eMove != ZM_MOVE_NONE && xLead.m_axMoves[0].m_uMaxPP > 0u)
		{
			xLead.m_axMoves[0].m_uCurPP = xLead.m_axMoves[0].m_uMaxPP - 1u;
		}
		xState.m_xParty.Add(xLead);
		xState.m_xParty.Add(MakeMonster(ZM_SPECIES_KINDLET, 8u + uSalt, "Second"));

		// 2 boxes -- one occupant in a non-zero box and a non-zero slot.
		xState.m_xBoxes.StoreAt(2u, 5u, MakeMonster(ZM_SPECIES_NIBBIN, 9u + uSalt, "Boxed"));

		// 3 dex.
		xState.MarkSeen(ZM_SPECIES_NIBBIN);
		xState.MarkCaught(ZM_SPECIES_FERNFAWN);

		// 4 story flags -- three set, one of them the model's HIGHEST index, which
		// forces module 4 to its full ceil(4096/8) width.
		xState.m_xStoryFlags.Set(0u, true);
		xState.m_xStoryFlags.Set(11u + uSalt, true);
		xState.m_xStoryFlags.Set(uZM_MAX_STORY_FLAGS - 1u, true);

		// 5 badges.
		xState.AwardBadge(1u);
		xState.AwardBadge(6u);

		// 6 bag -- two entries in different pockets.
		xState.m_xBag.Add(ZM_ITEM_CATCHORB, 4u + uSalt);
		xState.m_xBag.Add(ZM_ITEM_SALVE, 9u);

		// 7 money.
		xState.m_uMoney = 4321u + uSalt;

		// 8 daycare -- two parents plus a live egg.
		xState.m_xDaycare.m_uParentCount = 2u;
		xState.m_xDaycare.m_axParents[0] = MakeMonster(ZM_SPECIES_FERNFAWN, 30u, "Dam");
		xState.m_xDaycare.m_axParents[1] = MakeMonster(ZM_SPECIES_KINDLET, 31u, "Sire");
		xState.m_xDaycare.m_bEggPresent = true;
		xState.m_xDaycare.m_xEgg = MakeMonster(ZM_SPECIES_NIBBIN, 1u, "Egg");
		xState.m_xDaycare.m_xEgg.m_uFlags |= uZM_MONSTER_FLAG_IS_EGG;
		xState.m_xDaycare.m_uEggStepsRemaining = 77u + uSalt;

		// 9 tower.
		xState.m_xTowerRun.m_uCurrentStreak = 3u + uSalt;
		xState.m_xTowerRun.m_uBestStreak = 12u + uSalt;
		xState.m_xTowerRun.m_ulSeed = 0x0badc0dedeadbeefull + uSalt;

		// 10 world position -- Dawnmere (build index 2), a spawn tag that scene
		// actually offers, non-zero xyz and a non-zero yaw.
		xState.m_xWorldPosition.m_uSceneBuildIndex = 2u;
		memcpy(xState.m_xWorldPosition.m_szSpawnTag, "TownCenter", 11u);
		xState.m_xWorldPosition.m_afPosition[0] = 512.25f + (float)uSalt;
		xState.m_xWorldPosition.m_afPosition[1] = 26.5f;
		xState.m_xWorldPosition.m_afPosition[2] = 480.75f;
		xState.m_xWorldPosition.m_fYaw = -1.25f;

		// 11 options.
		xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		return xState;
	}

	void AssertMonsterEqual(const ZM_Monster& xExpected, const ZM_Monster& xActual,
		const char* szContext)
	{
		ZENITH_ASSERT_EQ((u_int)xActual.m_eSpecies, (u_int)xExpected.m_eSpecies,
			"%s species", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uLevel, xExpected.m_uLevel, "%s level", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uCurrentExp, xExpected.m_uCurrentExp, "%s exp", szContext);
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_auIV[u], xExpected.m_auIV[u], "%s IV %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_auEV[u], xExpected.m_auEV[u], "%s EV %u", szContext, u);
		}
		ZENITH_ASSERT_EQ((u_int)xActual.m_eNature, (u_int)xExpected.m_eNature,
			"%s nature", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eAbility, (u_int)xExpected.m_eAbility,
			"%s ability", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eStatus, (u_int)xExpected.m_eStatus,
			"%s status", szContext);
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			ZENITH_ASSERT_EQ((u_int)xActual.m_axMoves[u].m_eMove,
				(u_int)xExpected.m_axMoves[u].m_eMove, "%s move %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[u].m_uCurPP,
				xExpected.m_axMoves[u].m_uCurPP, "%s current PP %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[u].m_uMaxPP,
				xExpected.m_axMoves[u].m_uMaxPP, "%s max PP %u", szContext, u);
		}
		ZENITH_ASSERT_EQ(xActual.m_uCurrentHp, xExpected.m_uCurrentHp, "%s HP", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eGender, (u_int)xExpected.m_eGender,
			"%s gender", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uFriendship, xExpected.m_uFriendship,
			"%s friendship", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uFlags, xExpected.m_uFlags, "%s flags", szContext);
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_szNickname, xExpected.m_szNickname,
			uZM_MONSTER_NICKNAME_CAPACITY) == 0, "%s nickname", szContext);
	}

	// Field-for-field over all ELEVEN durable modules. Check-then-use throughout:
	// the assert macros RECORD and CONTINUE, so a failed count check must never
	// fall through into an indexed read.
	void AssertStateEqual(const ZM_GameState& xExpected, const ZM_GameState& xActual,
		const char* szContext)
	{
		// 1 party
		ZENITH_ASSERT_EQ(xActual.m_xParty.Count(), xExpected.m_xParty.Count(),
			"%s party count", szContext);
		const u_int uComparableParty = (std::min)(xExpected.m_xParty.Count(),
			xActual.m_xParty.Count());
		for (u_int u = 0u; u < uComparableParty; ++u)
		{
			AssertMonsterEqual(xExpected.m_xParty.Get(u), xActual.m_xParty.Get(u), szContext);
		}

		// 2 boxes
		ZENITH_ASSERT_EQ(xActual.m_xBoxes.Count(), xExpected.m_xBoxes.Count(),
			"%s box count", szContext);
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				const ZM_Monster* pxExpected = xExpected.m_xBoxes.TryGet(uBox, uSlot);
				const ZM_Monster* pxActual = xActual.m_xBoxes.TryGet(uBox, uSlot);
				ZENITH_ASSERT_EQ(pxActual != nullptr, pxExpected != nullptr,
					"%s box %u slot %u occupancy", szContext, uBox, uSlot);
				if (pxExpected != nullptr && pxActual != nullptr)
				{
					AssertMonsterEqual(*pxExpected, *pxActual, szContext);
				}
			}
		}

		// 3 dex
		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_xSeen.m_abFlags[u], xExpected.m_xSeen.m_abFlags[u],
				"%s seen %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_xCaught.m_abFlags[u], xExpected.m_xCaught.m_abFlags[u],
				"%s caught %u", szContext, u);
		}

		// 4 story flags
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_xStoryFlags.m_auFlags,
			xExpected.m_xStoryFlags.m_auFlags, uZM_STORY_FLAG_BYTE_COUNT) == 0,
			"%s story flags", szContext);

		// 5 badges
		ZENITH_ASSERT_EQ((u_int)xActual.m_uBadgeMask, (u_int)xExpected.m_uBadgeMask,
			"%s badges", szContext);

		// 6 bag
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_xBag.GetCount((ZM_ITEM_ID)u),
				xExpected.m_xBag.GetCount((ZM_ITEM_ID)u), "%s bag item %u", szContext, u);
		}

		// 7 money
		ZENITH_ASSERT_EQ(xActual.m_uMoney, xExpected.m_uMoney, "%s money", szContext);

		// 8 daycare
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_uParentCount,
			xExpected.m_xDaycare.m_uParentCount, "%s daycare parents", szContext);
		const u_int uComparableParents = (std::min)(xExpected.m_xDaycare.m_uParentCount,
			xActual.m_xDaycare.m_uParentCount);
		for (u_int u = 0u; u < uComparableParents; ++u)
		{
			AssertMonsterEqual(xExpected.m_xDaycare.m_axParents[u],
				xActual.m_xDaycare.m_axParents[u], szContext);
		}
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_bEggPresent,
			xExpected.m_xDaycare.m_bEggPresent, "%s daycare egg presence", szContext);
		if (xExpected.m_xDaycare.m_bEggPresent && xActual.m_xDaycare.m_bEggPresent)
		{
			AssertMonsterEqual(xExpected.m_xDaycare.m_xEgg, xActual.m_xDaycare.m_xEgg, szContext);
		}
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_uEggStepsRemaining,
			xExpected.m_xDaycare.m_uEggStepsRemaining, "%s daycare steps", szContext);

		// 9 tower
		ZENITH_ASSERT_EQ(xActual.m_xTowerRun.m_uCurrentStreak,
			xExpected.m_xTowerRun.m_uCurrentStreak, "%s tower current", szContext);
		ZENITH_ASSERT_EQ(xActual.m_xTowerRun.m_uBestStreak,
			xExpected.m_xTowerRun.m_uBestStreak, "%s tower best", szContext);
		ZENITH_ASSERT_TRUE(xActual.m_xTowerRun.m_ulSeed == xExpected.m_xTowerRun.m_ulSeed,
			"%s tower seed", szContext);

		// 10 world position
		ZENITH_ASSERT_EQ(xActual.m_xWorldPosition.m_uSceneBuildIndex,
			xExpected.m_xWorldPosition.m_uSceneBuildIndex, "%s scene", szContext);
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_xWorldPosition.m_szSpawnTag,
			xExpected.m_xWorldPosition.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY) == 0,
			"%s spawn tag", szContext);
		for (u_int u = 0u; u < 3u; ++u)
		{
			ZENITH_ASSERT_TRUE(xActual.m_xWorldPosition.m_afPosition[u]
				== xExpected.m_xWorldPosition.m_afPosition[u],
				"%s position %u", szContext, u);
		}
		ZENITH_ASSERT_TRUE(xActual.m_xWorldPosition.m_fYaw == xExpected.m_xWorldPosition.m_fYaw,
			"%s yaw", szContext);

		// 11 options
		ZENITH_ASSERT_EQ((u_int)xActual.m_xOptions.m_eTextSpeed,
			(u_int)xExpected.m_xOptions.m_eTextSpeed, "%s text speed", szContext);
	}

	// Encode with the FROZEN codec into a local owning stream. This is an
	// INDEPENDENT oracle for the framing layer under test: the codec is not what
	// SC2 changes, so using it to learn a blob's true length does not make any
	// assertion self-referential.
	std::vector<uint8_t> EncodeBlob(const ZM_GameState& xState, const char* szContext)
	{
		Zenith_DataStream xStream;
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(), "%s fixture failed to encode (error %u)",
			szContext, (u_int)xStatus.Error());
		if (!xStatus.IsOk()) { return {}; }
		std::vector<uint8_t> xBytes((size_t)xStream.GetCursor());
		if (!xBytes.empty())
		{
			memcpy(xBytes.data(), xStream.GetData(), xBytes.size());
		}
		return xBytes;
	}

	uint32_t DecodeLE32(const std::vector<uint8_t>& xBytes, size_t uOffset)
	{
		if (uOffset + 4u > xBytes.size()) { return 0u; }
		return (uint32_t)xBytes[uOffset]
			| ((uint32_t)xBytes[uOffset + 1u] << 8u)
			| ((uint32_t)xBytes[uOffset + 2u] << 16u)
			| ((uint32_t)xBytes[uOffset + 3u] << 24u);
	}

	// A raw byte snapshot of the destination object, padding included. Copy-
	// constructing a ZM_GameState and memcmp-ing the two would compare
	// indeterminate padding; memcpy-ing the SAME object's bytes does not.
	std::vector<uint8_t> SnapshotState(const ZM_GameState& xState)
	{
		std::vector<uint8_t> xBytes(sizeof(ZM_GameState));
		memcpy(xBytes.data(), &xState, sizeof(ZM_GameState));
		return xBytes;
	}

	void AssertStateByteIdentical(const std::vector<uint8_t>& xBefore,
		const ZM_GameState& xAfter, const char* szContext)
	{
		ZENITH_ASSERT_EQ(xBefore.size(), sizeof(ZM_GameState), "%s snapshot size", szContext);
		if (xBefore.size() != sizeof(ZM_GameState)) { return; }
		ZENITH_ASSERT_TRUE(memcmp(xBefore.data(), &xAfter, sizeof(ZM_GameState)) == 0,
			"%s mutated the destination on failure", szContext);
	}

	// ---- disk helpers ----------------------------------------------------------

	// Redirection + entry/exit hygiene for every disk-touching unit (the
	// Tests/Test_P1MetaSave_Suite.cpp:224-228 precedent for the hygiene half).
	//
	// ORDER IS THE SAFETY PROPERTY. The constructor turns the "_Test" name
	// redirection ON before ANYTHING else, then VERIFIES it took effect, and only
	// then deletes files. If SetTestSlotNamesForTests ever stopped working, the
	// verification fails, m_bActive stays false and this scope performs NO disk
	// operation whatsoever -- so a broken redirection can never make the hygiene
	// sweep itself delete the player's real Save0/1/2/Auto .zsave. The unit then
	// hard-fails via RequireTestSlotNames below instead of running.
	//
	// ClearForTest wipes the in-memory write log + readback stash but NOT disk files
	// (Zenith_SaveData.h:119), so DeleteAllSlotsForTests is what actually makes a
	// rerun on a dirty machine deterministic -- and what guarantees this suite leaves
	// nothing behind.
	struct ZM_SlotDiskScope
	{
		ZM_SlotDiskScope()
		{
			ZM_SaveSlots::SetTestSlotNamesForTests(true);
			// Verify BEFORE the first disk touch, not after.
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
			// Released unconditionally: a leaked redirection would silently repoint
			// every LATER save in this process at the _Test files.
			ZM_SaveSlots::SetTestSlotNamesForTests(false);
		}

		bool IsActive() const { return m_bActive; }

	private:
		bool m_bActive = false;
	};

	// HARD requirement, never a ZENITH_SKIP: a disk unit that cannot redirect its
	// slot names has zero coverage AND would be addressing live files, and a skip is
	// counted as a PASS by the gate. Returns the verdict so the caller can bail out
	// before its first disk call -- the assert macros RECORD AND CONTINUE, so the
	// caller MUST honour the false.
	bool RequireTestSlotNames(const ZM_SlotDiskScope& xScope, const char* szUnit)
	{
		ZENITH_ASSERT_TRUE(xScope.IsActive(),
			"%s: ZM_SaveSlots is still serving LIVE slot names inside a disk scope. "
			"ZM_SaveSlots::SetTestSlotNamesForTests(true) did not take effect, so this "
			"unit refuses to touch the player's real .zsave files (see the DISK SAFETY "
			"note at the top of ZM_Tests_SaveSlots.cpp)", szUnit);
		return xScope.IsActive();
	}

	void BuildSlotFilePath(ZM_SAVE_SLOT eSlot, char* szOut, size_t uOutSize)
	{
		snprintf(szOut, uOutSize, "%s%s%s", Zenith_SaveData::GetSaveDirectory(),
			ZM_SaveSlots::SlotName(eSlot), ZENITH_SAVE_EXT);
	}

	bool ReadSlotFileBytes(ZM_SAVE_SLOT eSlot, std::vector<uint8_t>& xOut)
	{
		xOut.clear();
		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotFilePath(eSlot, szPath, sizeof(szPath));
		if (!Zenith_FileAccess::FileExists(szPath)) { return false; }
		uint64_t ulSize = 0u;
		char* pData = Zenith_FileAccess::ReadFile(szPath, ulSize);
		if (pData == nullptr) { return false; }
		xOut.resize((size_t)ulSize);
		if (ulSize > 0u) { memcpy(xOut.data(), pData, (size_t)ulSize); }
		Zenith_FileAccess::FreeFileData(pData);
		return true;
	}

	// Flip one byte INSIDE the engine payload (past the fixed-size
	// Zenith_SaveFileHeader), which breaks the engine's CRC32 and makes LoadEx
	// report CORRUPT_DATA without disturbing magic or format version.
	bool CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT eSlot)
	{
		std::vector<uint8_t> xBytes;
		if (!ReadSlotFileBytes(eSlot, xBytes)) { return false; }
		if (xBytes.size() <= sizeof(Zenith_SaveFileHeader)) { return false; }
		xBytes[xBytes.size() - 1u] = (uint8_t)(xBytes[xBytes.size() - 1u] ^ 0xffu);
		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotFilePath(eSlot, szPath, sizeof(szPath));
		Zenith_FileAccess::WriteFile(szPath, xBytes.data(), (uint64_t)xBytes.size());
		return true;
	}

	// Hand-build a COMPLETE, engine-valid save FILE for a slot and write it straight
	// to disk, bypassing Zenith_SaveData::Save entirely.
	//
	// WHY THIS EXISTS: CorruptSlotPayloadOnDisk above can only produce files the write
	// path could itself have produced (it preserves the declared payload length and
	// merely breaks the CRC), so it can never manufacture a payload SHORTER than the
	// 4-byte length prefix. Only a hand-built file can -- and a hand-edited or
	// partially-flushed .zsave in the wild is exactly that shape.
	//
	// Mirrors Zenith_SaveData.cpp:178-199 field for field. The engine writes its
	// header with WriteData(&xHeader, sizeof(Zenith_SaveFileHeader)) -- a raw struct
	// dump -- so a raw struct dump here is byte-identical, and the CRC is taken with
	// the engine's OWN ComputeCRC32 over the same bytes LoadEx will checksum
	// (Zenith_SaveData.cpp:302-314). A file from this helper therefore clears every
	// engine gate and DOES reach the read callback.
	void WriteHandBuiltSlotFile(ZM_SAVE_SLOT eSlot, const uint8_t* pPayload,
		uint64_t ulPayloadBytes)
	{
		Zenith_SaveFileHeader xHeader = {};
		xHeader.uMagic         = uZENITH_SAVE_MAGIC;
		xHeader.uFormatVersion = uZENITH_SAVE_FORMAT_VERSION;
		xHeader.uGameVersion   = ZM_SaveSlots::uGAME_VERSION;
		xHeader.uChecksum      = (ulPayloadBytes > 0u)
			? Zenith_SaveData::ComputeCRC32(pPayload, ulPayloadBytes)
			: 0u;
		xHeader.ulPayloadSize  = ulPayloadBytes;
		xHeader.ulTimestamp    = 0u;   // metadata only; LoadEx never inspects it

		std::vector<uint8_t> xFile(sizeof(Zenith_SaveFileHeader) + (size_t)ulPayloadBytes);
		memcpy(xFile.data(), &xHeader, sizeof(Zenith_SaveFileHeader));
		if (ulPayloadBytes > 0u && pPayload != nullptr)
		{
			memcpy(xFile.data() + sizeof(Zenith_SaveFileHeader), pPayload,
				(size_t)ulPayloadBytes);
		}

		char szPath[ZENITH_MAX_PATH_LENGTH];
		BuildSlotFilePath(eSlot, szPath, sizeof(szPath));
		Zenith_FileAccess::WriteFile(szPath, xFile.data(), (uint64_t)xFile.size());
	}

	// The payload bytes the engine recorded for the most recent Save() call --
	// i.e. exactly what this layer handed the engine: [u32 length][ZMSV blob].
	bool CaptureLastWrittenPayload(std::vector<uint8_t>& xOut)
	{
		xOut.clear();
		const Zenith_Vector<Zenith_SaveData::WrittenSlot>& xLog =
			Zenith_SaveData::GetWrittenSlotsForTest();
		if (xLog.GetSize() == 0u) { return false; }
		const Zenith_SaveData::WrittenSlot& xEntry = xLog.GetBack();
		xOut.resize((size_t)xEntry.m_xPayload.GetSize());
		for (u_int u = 0u; u < xEntry.m_xPayload.GetSize(); ++u)
		{
			xOut[(size_t)u] = xEntry.m_xPayload.Get(u);
		}
		return true;
	}

#endif // ZENITH_INPUT_SIMULATOR

	// ---- save-permission policy expectations ----------------------------------
	// All SIXTEEN boolean combinations, spelled out by hand in the documented
	// top-to-bottom precedence order. Nothing here is computed by calling
	// ResolveSaveBlocker.
	struct ZM_BlockerCase
	{
		bool m_bOverworld;
		bool m_bBattle;
		bool m_bWarp;
		bool m_bWhiteout;
		ZM_SaveSlots::ZM_SAVE_BLOCKER m_eExpected;
	};

	const ZM_BlockerCase axBLOCKER_CASES[] =
	{
		// !overworld dominates every other condition.
		{ false, false, false, false, ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, false, false, true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, false, true,  false, ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, false, true,  true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, true,  false, false, ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, true,  false, true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, true,  true,  false, ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		{ false, true,  true,  true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_NOT_OVERWORLD },
		// In the overworld: battle > warp > whiteout > none.
		{ true,  false, false, false, ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE },
		{ true,  false, false, true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_PENDING_WHITEOUT },
		{ true,  false, true,  false, ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP },
		{ true,  false, true,  true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_WARP },
		{ true,  true,  false, false, ZM_SaveSlots::ZM_SAVE_BLOCKER_BATTLE },
		{ true,  true,  false, true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_BATTLE },
		{ true,  true,  true,  false, ZM_SaveSlots::ZM_SAVE_BLOCKER_BATTLE },
		{ true,  true,  true,  true,  ZM_SaveSlots::ZM_SAVE_BLOCKER_BATTLE },
	};
	constexpr u_int uBLOCKER_CASE_COUNT =
		(u_int)(sizeof(axBLOCKER_CASES) / sizeof(axBLOCKER_CASES[0]));
}

// ============================================================================
// The redirection tripwire. Declared FIRST on purpose: Zenith_TestRunner::
// RegisterTest PREPENDS to its list (Zenith_TestFramework.cpp:29), so within this
// translation unit the DECLARATION order is the REVERSE of the execution order --
// declaring this first makes it the LAST unit of this file to run, which is the
// only position from which it can catch a redirection leaked by any of the units
// below it.
// ============================================================================

ZENITH_TEST(ZM_Save, Slot_TestNameRedirectionDefaultsOffOutsideAnyScope)
{
	// NO ZM_SlotDiskScope here -- that is the entire point. Outside a scope the
	// redirection must be OFF, so SlotName() is the shipping stem and any later save
	// in this process lands where the player expects it. If a disk unit ever leaves
	// ZM_SaveSlots::SetTestSlotNamesForTests(true) set (a scope destructor removed, a
	// unit that pokes the setter by hand), this unit reds instead of the leak
	// silently repointing the game's real saves at the _Test files.
	//
	// Touches no disk and reads no file, so it is safe to run without a scope.
	if (Zenith_CommandLine::IsAutomatedTestRun())
	{
		// A harness run legitimately forces the aliases from the command line, so
		// there the expectation INVERTS: every name must be the _Test alias. Spelling
		// both arms out is what stops this unit becoming a no-op in either world.
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
			ZENITH_ASSERT_NOT_NULL(szLive, "SlotName(%u) is null", u);
			if (szLive == nullptr) { continue; }
			ZENITH_ASSERT_STREQ(szLive, aszEXPECTED_TEST_NAMES[u],
				"slot %u must serve its _Test alias under --all-automated-tests", u);
		}
		return;
	}

	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
		const char* szShipping = ZM_SaveSlots::SlotShippingName(aeALL_SLOTS[u]);
		ZENITH_ASSERT_NOT_NULL(szLive, "SlotName(%u) is null", u);
		ZENITH_ASSERT_NOT_NULL(szShipping, "SlotShippingName(%u) is null", u);
		if (szLive == nullptr || szShipping == nullptr) { continue; }
		// Against the LITERAL as well as against the production stem: the literal
		// catches "both maps were renamed together", the stem catches a leaked
		// redirect. Reds if any unit above leaks the redirection, or if the default
		// of s_bForceTestSlotNames (ZM_SaveSlots.cpp) is flipped to true.
		ZENITH_ASSERT_STREQ(szLive, aszEXPECTED_SHIPPING_NAMES[u],
			"slot %u must serve its SHIPPING stem outside a test scope -- a leaked "
			"redirection would repoint the player's saves", u);
		ZENITH_ASSERT_STREQ(szLive, szShipping, "slot %u leaked _Test redirection", u);
	}
}

// ============================================================================
// The pure name/classification maps. No disk, no instrumentation.
// ============================================================================

ZENITH_TEST(ZM_Save, Slot_NamesAreDistinctNonEmptyAndTotal)
{
	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		const char* szName = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
		ZENITH_ASSERT_NOT_NULL(szName, "SlotName(%u) is null", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "SlotName(%u) is empty", u);
	}

	// O(n^2) distinctness: two slots sharing a name means one silently overwrites
	// the other's file.
	for (u_int uA = 0u; uA < uALL_SLOT_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uALL_SLOT_COUNT; ++uB)
		{
			const char* szA = ZM_SaveSlots::SlotName(aeALL_SLOTS[uA]);
			const char* szB = ZM_SaveSlots::SlotName(aeALL_SLOTS[uB]);
			if (szA == nullptr || szB == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
				"slots %u and %u alias the same file name '%s'", uA, uB, szA);
		}
	}

	// TOTAL: an out-of-range id must yield "" so it can never build a path
	// (Zenith_SaveData::BuildSlotPath does zero sanitisation).
	const char* szNone = ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_NONE);
	ZENITH_ASSERT_NOT_NULL(szNone, "SlotName(NONE) is null");
	if (szNone != nullptr)
	{
		ZENITH_ASSERT_TRUE(szNone[0] == '\0', "SlotName(NONE) must be empty, got '%s'", szNone);
	}
	const char* szGarbage = ZM_SaveSlots::SlotName((ZM_SAVE_SLOT)uGARBAGE_SLOT_ID);
	ZENITH_ASSERT_NOT_NULL(szGarbage, "SlotName(garbage) is null");
	if (szGarbage != nullptr)
	{
		ZENITH_ASSERT_TRUE(szGarbage[0] == '\0',
			"SlotName(garbage) must be empty, got '%s'", szGarbage);
	}
}

ZENITH_TEST(ZM_Save, Slot_TestRunNameDiffersFromShippingName)
{
	// The SHIPPING stems are a compatibility surface and are pinned literally.
	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		const char* szShipping = ZM_SaveSlots::SlotShippingName(aeALL_SLOTS[u]);
		ZENITH_ASSERT_NOT_NULL(szShipping, "SlotShippingName(%u) is null", u);
		if (szShipping == nullptr) { continue; }
		ZENITH_ASSERT_STREQ(szShipping, aszEXPECTED_SHIPPING_NAMES[u],
			"slot %u shipping stem", u);
	}

	// The redirection is EXERCISED, not merely observed. Everything below runs INSIDE
	// a scope, so the interesting branch of ZM_SaveSlots::SlotName (the one that picks
	// s_aszTestNames) actually executes -- outside a scope, on a boot run with no
	// command-line flags, that branch is never taken and every comparison here would
	// be trivially true.
#ifdef ZENITH_INPUT_SIMULATOR
	{
		ZM_SlotDiskScope xScope;
		if (!RequireTestSlotNames(xScope, "Slot_TestRunNameDiffersFromShippingName"))
		{
			return;
		}

		u_int uIndirectedCount = 0u;
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
			const char* szShipping = ZM_SaveSlots::SlotShippingName(aeALL_SLOTS[u]);
			ZENITH_ASSERT_NOT_NULL(szLive, "SlotName(%u) is null", u);
			if (szLive == nullptr || szShipping == nullptr) { continue; }

			// Against the LITERAL alias table: reds if the suffix changes, if two
			// slots are swapped, or if the map is re-derived from the shipping table
			// at runtime.
			ZENITH_ASSERT_STREQ(szLive, aszEXPECTED_TEST_NAMES[u],
				"slot %u redirected name", u);

			// ...and the alias is still the shipping stem plus a SUFFIX, never a
			// rename, which would orphan every existing save.
			const size_t uShippingLength = strlen(szShipping);
			ZENITH_ASSERT_TRUE(strncmp(szLive, szShipping, uShippingLength) == 0,
				"slot %u live name '%s' does not begin with its shipping stem '%s'",
				u, szLive, szShipping);
			ZENITH_ASSERT_TRUE(strlen(szLive) > uShippingLength,
				"slot %u live name '%s' must be strictly longer than its stem", u, szLive);
			if (strcmp(szLive, szShipping) != 0) { ++uIndirectedCount; }
		}

		// The indirection is all-or-nothing. A half-applied branch would mix a test
		// Save0 with a LIVE Save1 -- the worst possible outcome, because the damage is
		// invisible in the one slot the author happened to check. Reds if
		// ZM_SaveSlots::SlotName's ternary is applied to only some ordinals.
		ZENITH_ASSERT_EQ(uIndirectedCount, uALL_SLOT_COUNT,
			"the _Test indirection is applied to %u of %u slots inside a disk scope",
			uIndirectedCount, uALL_SLOT_COUNT);

		// The aliases must be distinct from each other too -- an alias table that
		// collapsed two slots onto one name would make Save1 overwrite Save0 in every
		// disk unit below, which is exactly the class of bug those units exist to see.
		for (u_int uA = 0u; uA < uALL_SLOT_COUNT; ++uA)
		{
			for (u_int uB = uA + 1u; uB < uALL_SLOT_COUNT; ++uB)
			{
				const char* szA = ZM_SaveSlots::SlotName(aeALL_SLOTS[uA]);
				const char* szB = ZM_SaveSlots::SlotName(aeALL_SLOTS[uB]);
				if (szA == nullptr || szB == nullptr) { continue; }
				ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
					"redirected slots %u and %u alias the same file name '%s'", uA, uB, szA);
			}
		}

		// TOTAL under redirection too: a bad id must still produce "", or it would
		// build a path from garbage.
		const char* szNone = ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_NONE);
		ZENITH_ASSERT_NOT_NULL(szNone, "SlotName(NONE) is null under redirection");
		if (szNone != nullptr)
		{
			ZENITH_ASSERT_TRUE(szNone[0] == '\0',
				"SlotName(NONE) must be empty under redirection, got '%s'", szNone);
		}
	}

	// ...and the scope RELEASED it again. Both directions in one unit: deleting the
	// destructor's SetTestSlotNamesForTests(false) reds here.
	if (!Zenith_CommandLine::IsAutomatedTestRun())
	{
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			const char* szLive = ZM_SaveSlots::SlotName(aeALL_SLOTS[u]);
			if (szLive == nullptr) { continue; }
			ZENITH_ASSERT_STREQ(szLive, aszEXPECTED_SHIPPING_NAMES[u],
				"slot %u kept its _Test alias after the scope closed", u);
		}
	}
#else
	// No instrumentation means no way to drive SetTestSlotNamesForTests, so only the
	// shipping half above is checkable. Unreachable in every configuration this repo
	// builds: Zenith.h:255 defines ZENITH_INPUT_SIMULATOR unconditionally.
	ZENITH_SKIP("save-slot test-name redirection is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ManualClassification)
{
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT_0), "Save0 is manual");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT_1), "Save1 is manual");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT_2), "Save2 is manual");
	// SaveFormat.md:44-45 -- Auto is written ONLY by milestone triggers.
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT_AUTO),
		"Auto must never be offered as a manual slot");
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT_NONE), "NONE is not manual");
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::IsManualSlot((ZM_SAVE_SLOT)uGARBAGE_SLOT_ID),
		"an out-of-range id is not manual");
}

ZENITH_TEST(ZM_Save, Slot_DisplayNamesAreTotalAndOriginal)
{
	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		const char* szDisplay = ZM_SaveSlots::SlotDisplayName(aeALL_SLOTS[u]);
		ZENITH_ASSERT_NOT_NULL(szDisplay, "SlotDisplayName(%u) is null", u);
		if (szDisplay == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szDisplay[0] != '\0', "SlotDisplayName(%u) is empty", u);
		ZENITH_ASSERT_STREQ(szDisplay, aszEXPECTED_DISPLAY_NAMES[u], "slot %u display copy", u);
	}

	for (u_int uA = 0u; uA < uALL_SLOT_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uALL_SLOT_COUNT; ++uB)
		{
			const char* szA = ZM_SaveSlots::SlotDisplayName(aeALL_SLOTS[uA]);
			const char* szB = ZM_SaveSlots::SlotDisplayName(aeALL_SLOTS[uB]);
			if (szA == nullptr || szB == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
				"slots %u and %u share display copy '%s'", uA, uB, szA);
		}
	}

	// TOTAL and never a real slot's copy: a bad id must not render as "Slot 1".
	const ZM_SAVE_SLOT aeBadSlots[] = { ZM_SAVE_SLOT_NONE, (ZM_SAVE_SLOT)uGARBAGE_SLOT_ID };
	for (u_int uBad = 0u; uBad < 2u; ++uBad)
	{
		const char* szDisplay = ZM_SaveSlots::SlotDisplayName(aeBadSlots[uBad]);
		ZENITH_ASSERT_NOT_NULL(szDisplay, "SlotDisplayName(bad %u) is null", uBad);
		if (szDisplay == nullptr) { continue; }
		for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
		{
			ZENITH_ASSERT_TRUE(strcmp(szDisplay, aszEXPECTED_DISPLAY_NAMES[u]) != 0,
				"an out-of-range slot renders as the real copy '%s'", szDisplay);
		}
	}
}

// ============================================================================
// The slot/disk behaviour. Every one of these constructs ZM_SlotDiskScope FIRST
// (redirection + entry/exit hygiene) and then HARD-REQUIRES that the redirection
// is live -- none of them can skip.
// ============================================================================

ZENITH_TEST(ZM_Save, Slot_ProbeIsEmptyBeforeAnyWrite)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ProbeIsEmptyBeforeAnyWrite")) { return; }

	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(aeALL_SLOTS[u]),
			(u_int)ZM_SAVE_SLOT_EMPTY, "slot %u must probe EMPTY with no file on disk", u);
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(aeALL_SLOTS[u])),
			"slot %u file still on disk after DeleteAllSlotsForTests", u);
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteThenProbeIsReady)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteThenProbeIsReady")) { return; }

	const ZM_GameState xStarter = ZM_MakeStarterGameState();
	const Zenith_Status xStatus = ZM_SaveSlots::WriteState(xStarter, ZM_SAVE_SLOT_0);
	AssertStatus(xStatus, Zenith_ErrorCode::SUCCESS, "WriteState(starter, Save0)");

	// A file must actually exist -- Zenith_SaveData::Save returns true even when
	// nothing landed, so the file check is the real evidence.
	ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0)),
		"WriteState reported success but no file landed on disk");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_READY, "a freshly written slot must probe READY");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_MaximalStateRoundTripsFieldForField)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_MaximalStateRoundTripsFieldForField")) { return; }

	const ZM_GameState xSource = MakeMaximalState(0u);
	AssertStatus(ZM_SaveSlots::WriteState(xSource, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Save0)");

	// Read into a SEPARATE default-constructed destination. Reading back into the
	// object that was written from would only prove an object equals itself.
	ZM_GameState xDestination;
	const Zenith_Status xRead = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	AssertStatus(xRead, Zenith_ErrorCode::SUCCESS, "ReadState(Save0)");
	if (!xRead.IsOk()) { return; }

	AssertStateEqual(xSource, xDestination, "maximal slot round trip");

	// Non-vacuity: a default-constructed state must NOT already match the fixture,
	// otherwise the comparison above would pass on a read that published nothing.
	const ZM_GameState xUntouched;
	ZENITH_ASSERT_TRUE(xUntouched.m_xParty.Count() != xSource.m_xParty.Count()
		|| xUntouched.m_uMoney != xSource.m_uMoney,
		"the maximal fixture is indistinguishable from a default state");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadOfAMissingSlotIsFileNotFoundAndLeavesDestinationByteIdentical)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadOfAMissingSlotIsFileNotFoundAndLeavesDestinationByteIdentical")) { return; }

	ZM_GameState xDestination = MakeMaximalState(1u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);

	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_1, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::FILE_NOT_FOUND, "ReadState(absent Save1)");
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(absent Save1)");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadOfACorruptedPayloadLeavesDestinationByteIdentical)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadOfACorruptedPayloadLeavesDestinationByteIdentical")) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(MakeMaximalState(0u), ZM_SAVE_SLOT_2),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Save2)");
	ZENITH_ASSERT_TRUE(CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT_2),
		"could not corrupt the Save2 file for the test");

	ZM_GameState xDestination = MakeMaximalState(2u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);

	// The SPECIFIC status, not merely "not success": flipping a payload byte breaks
	// the ENGINE's CRC32, which LoadEx reports as CORRUPT_DATA
	// (Zenith_SaveData.cpp:312). Reds if ZM_SaveSlots::ReadState ever flattens the
	// engine's reason into a generic failure -- the FrontEnd needs the distinction
	// between "damaged" and "wrong version" to write the right message.
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_2, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::CORRUPT_DATA, "ReadState(corrupt Save2)");
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(corrupt Save2)");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ProbeReportsEmptyReadyAndDamaged)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ProbeReportsEmptyReadyAndDamaged")) { return; }

	// EMPTY
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_EMPTY, "no file must probe EMPTY");

	// READY
	AssertStatus(ZM_SaveSlots::WriteState(MakeMaximalState(0u), ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Save0)");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_READY, "a decodable file must probe READY");

	// DAMAGED -- a present-but-unreadable file is a THIRD state, never conflated
	// with "no file" (which would let the UI offer New Game over a recoverable save).
	ZENITH_ASSERT_TRUE(CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT_0),
		"could not corrupt the Save0 file for the test");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_DAMAGED, "an undecodable file must probe DAMAGED, not EMPTY");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ProbeNeverMutatesADamagedSlot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ProbeNeverMutatesADamagedSlot")) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(MakeMaximalState(0u), ZM_SAVE_SLOT_1),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Save1)");
	ZENITH_ASSERT_TRUE(CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT_1),
		"could not corrupt the Save1 file for the test");

	std::vector<uint8_t> xBefore;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_1, xBefore),
		"the damaged Save1 file could not be read back");
	ZENITH_ASSERT_GT(xBefore.size(), (size_t)16u, "damaged fixture is implausibly small");
	if (xBefore.size() <= 16u) { return; }

	// Probe it TWICE -- a "repair on first sight" would still be caught, and an
	// uncached probe must give the same answer both times.
	const ZM_SAVE_SLOT_STATUS eFirst = ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1);
	const ZM_SAVE_SLOT_STATUS eSecond = ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1);
	ZENITH_ASSERT_EQ((u_int)eFirst, (u_int)ZM_SAVE_SLOT_DAMAGED, "first probe");
	ZENITH_ASSERT_EQ((u_int)eSecond, (u_int)ZM_SAVE_SLOT_DAMAGED, "second probe");

	// Docs/SaveFormat.md:318-321 -- a damaged slot is SURFACED, never auto-deleted,
	// auto-overwritten or "repaired".
	ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_1)),
		"probing a damaged slot deleted its file");
	std::vector<uint8_t> xAfter;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_1, xAfter),
		"the damaged file disappeared across a probe");
	ZENITH_ASSERT_EQ(xAfter.size(), xBefore.size(),
		"probing a damaged slot changed its byte length");
	// EVERY byte, not just the header: a "repair" that rewrote only the payload (or
	// recomputed the CRC in place) would leave the length and the leading 16 bytes
	// untouched and slip past a prefix-only comparison.
	if (xAfter.size() != xBefore.size()) { return; }
	ZENITH_ASSERT_TRUE(memcmp(xAfter.data(), xBefore.data(), xBefore.size()) == 0,
		"probing a damaged slot rewrote its contents");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteIsIndependentPerSlot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteIsIndependentPerSlot")) { return; }

	const ZM_GameState xFirst = MakeMaximalState(0u);
	const ZM_GameState xSecond = MakeMaximalState(3u);
	// Non-vacuity: the two fixtures must genuinely differ, or a hardcoded slot name
	// would go undetected.
	ZENITH_ASSERT_TRUE(xFirst.m_uMoney != xSecond.m_uMoney,
		"the two fixtures must differ for this unit to mean anything");

	AssertStatus(ZM_SaveSlots::WriteState(xFirst, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(first, Save0)");
	AssertStatus(ZM_SaveSlots::WriteState(xSecond, ZM_SAVE_SLOT_2),
		Zenith_ErrorCode::SUCCESS, "WriteState(second, Save2)");

	ZM_GameState xReadFirst;
	ZM_GameState xReadSecond;
	const Zenith_Status xA = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xReadFirst);
	const Zenith_Status xB = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_2, xReadSecond);
	AssertStatus(xA, Zenith_ErrorCode::SUCCESS, "ReadState(Save0)");
	AssertStatus(xB, Zenith_ErrorCode::SUCCESS, "ReadState(Save2)");
	if (!xA.IsOk() || !xB.IsOk()) { return; }

	AssertStateEqual(xFirst, xReadFirst, "Save0 kept its own state");
	AssertStateEqual(xSecond, xReadSecond, "Save2 kept its own state");
	ZENITH_ASSERT_TRUE(xReadFirst.m_uMoney != xReadSecond.m_uMoney,
		"both slots read back the same state -- the slot argument is being ignored");

	// The untouched middle slot must still be empty.
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_1),
		(u_int)ZM_SAVE_SLOT_EMPTY, "writing Save0/Save2 created a Save1 file");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_AutoIsIndependentOfEveryManualSlot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_AutoIsIndependentOfEveryManualSlot")) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(MakeMaximalState(0u), ZM_SAVE_SLOT_AUTO),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Auto)");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO),
		(u_int)ZM_SAVE_SLOT_READY, "Auto must be READY after its own write");

	const ZM_SAVE_SLOT aeManual[] = { ZM_SAVE_SLOT_0, ZM_SAVE_SLOT_1, ZM_SAVE_SLOT_2 };
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(aeManual[u]),
			(u_int)ZM_SAVE_SLOT_EMPTY,
			"an autosave wrote into manual slot %u -- Auto shares its file name", u);
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(aeManual[u])),
			"an autosave created a file for manual slot %u", u);
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_AutoIsAnOrdinarySlotToThisLayer)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_AutoIsAnOrdinarySlotToThisLayer")) { return; }

	// The manual-vs-milestone POLICY lives above this layer; storage must treat
	// AUTO exactly like Save0. Run the identical cycle on both and compare.
	const ZM_SAVE_SLOT aeSubjects[] = { ZM_SAVE_SLOT_0, ZM_SAVE_SLOT_AUTO };
	u_int auWriteError[2] = { 0u, 0u };
	u_int auProbeAfterWrite[2] = { 0u, 0u };
	u_int auReadError[2] = { 0u, 0u };
	u_int auProbeAfterDelete[2] = { 0u, 0u };
	bool abDeleted[2] = { false, false };
	bool abStateMatched[2] = { false, false };

	const ZM_GameState xSource = MakeMaximalState(0u);
	for (u_int u = 0u; u < 2u; ++u)
	{
		const Zenith_Status xWrite = ZM_SaveSlots::WriteState(xSource, aeSubjects[u]);
		auWriteError[u] = (u_int)xWrite.Error();
		auProbeAfterWrite[u] = (u_int)ZM_SaveSlots::ProbeSlot(aeSubjects[u]);

		ZM_GameState xRead;
		const Zenith_Status xReadStatus = ZM_SaveSlots::ReadState(aeSubjects[u], xRead);
		auReadError[u] = (u_int)xReadStatus.Error();
		abStateMatched[u] = xReadStatus.IsOk()
			&& xRead.m_uMoney == xSource.m_uMoney
			&& xRead.m_xParty.Count() == xSource.m_xParty.Count()
			&& xRead.m_xWorldPosition.m_uSceneBuildIndex
				== xSource.m_xWorldPosition.m_uSceneBuildIndex;

		abDeleted[u] = ZM_SaveSlots::DeleteSlotFile(aeSubjects[u]);
		auProbeAfterDelete[u] = (u_int)ZM_SaveSlots::ProbeSlot(aeSubjects[u]);
	}

	// ABSOLUTE expectations only. There is deliberately no follow-up
	// "auWriteError[1] == auWriteError[0]" style cross-subject comparison: once both
	// subjects are pinned to SUCCESS / READY / SUCCESS / EMPTY below, an equality
	// between them is true by construction and can never red on its own. Pinning
	// each subject absolutely is strictly stronger -- it also catches "both wrong the
	// same way", which the equality form passes.
	for (u_int u = 0u; u < 2u; ++u)
	{
		ZENITH_ASSERT_EQ(auWriteError[u], (u_int)Zenith_ErrorCode::SUCCESS,
			"subject %u write", u);
		ZENITH_ASSERT_EQ(auProbeAfterWrite[u], (u_int)ZM_SAVE_SLOT_READY,
			"subject %u probe after write", u);
		ZENITH_ASSERT_EQ(auReadError[u], (u_int)Zenith_ErrorCode::SUCCESS,
			"subject %u read", u);
		ZENITH_ASSERT_TRUE(abStateMatched[u], "subject %u read back the wrong state", u);
		ZENITH_ASSERT_TRUE(abDeleted[u], "subject %u delete reported failure", u);
		ZENITH_ASSERT_EQ(auProbeAfterDelete[u], (u_int)ZM_SAVE_SLOT_EMPTY,
			"subject %u probe after delete", u);
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_OverwriteReplacesRatherThanAppends)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_OverwriteReplacesRatherThanAppends")) { return; }

	// ZM_SaveSchema::Write APPENDS at the destination cursor (ZM_SaveSchema.cpp:1084),
	// so a write path that reuses a stream without resetting produces A+B.
	const ZM_GameState xFirst = MakeMaximalState(0u);
	const ZM_GameState xSecond = ZM_MakeStarterGameState();
	const std::vector<uint8_t> xSecondBlob = EncodeBlob(xSecond, "second state");
	ZENITH_ASSERT_GT(xSecondBlob.size(), (size_t)0u, "second fixture encoded to nothing");
	if (xSecondBlob.empty()) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(xFirst, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "first WriteState");
	AssertStatus(ZM_SaveSlots::WriteState(xSecond, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "second WriteState");

	std::vector<uint8_t> xPayload;
	ZENITH_ASSERT_TRUE(CaptureLastWrittenPayload(xPayload),
		"no Save() was recorded for the overwrite");
	ZENITH_ASSERT_EQ(xPayload.size(), (size_t)uEXPECTED_PREFIX_BYTES + xSecondBlob.size(),
		"the second write carries more than the second state alone");

	ZM_GameState xRead;
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xRead);
	AssertStatus(xStatus, Zenith_ErrorCode::SUCCESS, "ReadState after overwrite");
	if (!xStatus.IsOk()) { return; }
	AssertStateEqual(xSecond, xRead, "overwrite left the second state");
	ZENITH_ASSERT_TRUE(xRead.m_uMoney != xFirst.m_uMoney,
		"the overwrite did not replace the first state");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_RecordedPayloadIsPrefixPlusBlob)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_RecordedPayloadIsPrefixPlusBlob")) { return; }

	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::uLENGTH_PREFIX_BYTES, uEXPECTED_PREFIX_BYTES,
		"the length prefix must stay a 4-byte field");

	const ZM_GameState xGolden = MakeGoldenV1State();
	// Tripwire: the frozen v1 payload for this state is 824 bytes. If this fires,
	// the CODEC moved, not the framing.
	const std::vector<uint8_t> xBlob = EncodeBlob(xGolden, "golden v1 state");
	ZENITH_ASSERT_EQ(xBlob.size(), (size_t)uGOLDEN_BLOB_BYTES,
		"the frozen v1 golden payload length changed");

	AssertStatus(ZM_SaveSlots::WriteState(xGolden, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(golden, Save0)");

	ZENITH_ASSERT_EQ(Zenith_SaveData::GetWrittenSlotsForTest().GetSize(), 1u,
		"one WriteState must produce exactly one engine Save() call");

	std::vector<uint8_t> xPayload;
	ZENITH_ASSERT_TRUE(CaptureLastWrittenPayload(xPayload), "no Save() was recorded");
	ZENITH_ASSERT_EQ(xPayload.size(),
		(size_t)uEXPECTED_PREFIX_BYTES + (size_t)uGOLDEN_BLOB_BYTES,
		"the engine payload is not [4-byte prefix][824-byte ZMSV blob]");
	if (xPayload.size() < (size_t)uEXPECTED_PREFIX_BYTES + 4u) { return; }

	// The ZMSV magic must begin at byte 4 -- i.e. the prefix is FIRST, not appended
	// after the blob.
	ZENITH_ASSERT_EQ((u_int)xPayload[4], (u_int)'Z', "payload byte 4");
	ZENITH_ASSERT_EQ((u_int)xPayload[5], (u_int)'M', "payload byte 5");
	ZENITH_ASSERT_EQ((u_int)xPayload[6], (u_int)'S', "payload byte 6");
	ZENITH_ASSERT_EQ((u_int)xPayload[7], (u_int)'V', "payload byte 7");

	// And the framed blob is byte-for-byte the codec's own output.
	if (xPayload.size() == (size_t)uEXPECTED_PREFIX_BYTES + xBlob.size())
	{
		ZENITH_ASSERT_TRUE(memcmp(xPayload.data() + uEXPECTED_PREFIX_BYTES,
			xBlob.data(), xBlob.size()) == 0,
			"the framed blob differs from the codec's own bytes");
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_PrefixIsLittleEndian)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_PrefixIsLittleEndian")) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(MakeGoldenV1State(), ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(golden, Save0)");

	std::vector<uint8_t> xPayload;
	ZENITH_ASSERT_TRUE(CaptureLastWrittenPayload(xPayload), "no Save() was recorded");
	if (xPayload.size() < (size_t)uEXPECTED_PREFIX_BYTES) { return; }

	// Byte-by-byte, so a memcpy of a struct or a byte-swap is caught explicitly.
	ZENITH_ASSERT_EQ((u_int)xPayload[0], (u_int)(uGOLDEN_BLOB_BYTES & 0xffu), "prefix byte 0");
	ZENITH_ASSERT_EQ((u_int)xPayload[1], (u_int)((uGOLDEN_BLOB_BYTES >> 8u) & 0xffu),
		"prefix byte 1");
	ZENITH_ASSERT_EQ((u_int)xPayload[2], (u_int)((uGOLDEN_BLOB_BYTES >> 16u) & 0xffu),
		"prefix byte 2");
	ZENITH_ASSERT_EQ((u_int)xPayload[3], (u_int)((uGOLDEN_BLOB_BYTES >> 24u) & 0xffu),
		"prefix byte 3");
	ZENITH_ASSERT_EQ((u_int)DecodeLE32(xPayload, 0u), uGOLDEN_BLOB_BYTES,
		"the little-endian prefix does not decode to the 824-byte blob length");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_StagedReadbackDecodesWithoutTouchingDisk)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_StagedReadbackDecodesWithoutTouchingDisk")) { return; }

	// THE unit that pins the whole length-prefix decision. The staged-readback path
	// hands the read callback a default-constructed OWNING stream whose GetCapacity()
	// is the 1024-byte ALLOCATION (Zenith_SaveData.cpp:229-235), NOT the 828 staged
	// bytes. A read callback "simplified" to
	//     ZM_SaveSchema::Read(xStream, xStream.GetCapacity(), ...)
	// therefore hands the codec 1024 and is rejected with trailingBytes. It has to be
	// driven through the REAL engine seam: a hand-rolled buffer would size itself
	// exactly and bypass the very ambiguity this defends against.
	const ZM_GameState xGolden = MakeGoldenV1State();
	AssertStatus(ZM_SaveSlots::WriteState(xGolden, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(golden, Save0)");

	std::vector<uint8_t> xPayload;
	ZENITH_ASSERT_TRUE(CaptureLastWrittenPayload(xPayload), "no Save() was recorded");
	ZENITH_ASSERT_EQ(xPayload.size(),
		(size_t)uEXPECTED_PREFIX_BYTES + (size_t)uGOLDEN_BLOB_BYTES, "captured payload size");
	if (xPayload.size() != (size_t)uEXPECTED_PREFIX_BYTES + (size_t)uGOLDEN_BLOB_BYTES)
	{
		return;
	}
	// The staged buffer must be smaller than the stream's 1024-byte allocation for
	// the capacity ambiguity to exist at all.
	ZENITH_ASSERT_LT(xPayload.size(), (size_t)1024u,
		"the fixture no longer exercises the capacity-vs-length ambiguity");

	// Remove every file, then serve the bytes from the stash.
	ZM_SaveSlots::DeleteAllSlotsForTests();
	ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0)),
		"the Save0 file survived DeleteAllSlotsForTests");

	Zenith_SaveData::SetReadbackForTest(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0),
		ZM_SaveSlots::uGAME_VERSION, xPayload.data(), (uint64_t)xPayload.size());

	ZM_GameState xDestination;
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::SUCCESS, "ReadState(staged readback)");
	if (!xStatus.IsOk()) { return; }
	AssertStateEqual(xGolden, xDestination, "staged readback");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadRejectsAPrefixLongerThanTheStagedPayload)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadRejectsAPrefixLongerThanTheStagedPayload")) { return; }

	// WHICH BRANCH THIS ACTUALLY PINS. Not the "too small to hold a prefix" guard --
	// that one is structurally UNREACHABLE from a staged readback, because the staged
	// path always hands the callback a default-constructed OWNING stream whose
	// GetCapacity() is the 1024-byte ALLOCATION (Zenith_SaveData.cpp:229-235,
	// Zenith_DataStream.h:7/402), never the 2 staged bytes. `capacity - cursor` is
	// therefore 1024 here and can never be < 4. What this unit reaches is the LENGTH-
	// BOUNDS branch below it: a prefix that claims more bytes than the stream can
	// serve. The genuinely-too-small case needs the DISK path and is covered by
	// Slot_ReadRejectsADiskPayloadTooSmallForAPrefix.
	//
	// Only TWO bytes are staged, so the allocation's bytes 2..3 are whatever it
	// happens to contain and are NOT zeroed. Both staged bytes are 0xff, so the
	// reassembled little-endian u32 is (garbageHigh16 << 16) | 0xffff and is therefore
	// AT LEAST 65535 no matter what the high half holds -- which cannot fit the 1020
	// bytes remaining after the prefix. The verdict is decided entirely by the two
	// bytes this test controls; the indeterminate half cannot influence it either way.
	// (The previous 0x00,0x00 fixture reached the same verdict but only by reasoning
	// over BOTH possible values of the garbage, i.e. its outcome genuinely depended on
	// an uninitialised read -- a sanitizer build flags exactly that.)
	const uint8_t auTruncated[2] = { 0xffu, 0xffu };
	Zenith_SaveData::SetReadbackForTest(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0),
		ZM_SaveSlots::uGAME_VERSION, auTruncated, 2ull);

	ZM_GameState xDestination = MakeMaximalState(1u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);

	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::CORRUPT_DATA, "ReadState(overlong prefix)");
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(overlong prefix)");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadRejectsADiskPayloadTooSmallForAPrefix)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadRejectsADiskPayloadTooSmallForAPrefix")) { return; }

	// THE unit for ZM_SaveSlots.cpp's "payload is too small to hold the 4-byte length
	// prefix" guard -- the `(ulCapacity - ulCursor) < uLENGTH_PREFIX_BYTES` clause.
	// NOTHING else in this file reaches it:
	//   * every staged-readback unit is served a default-constructed OWNING stream
	//     whose capacity is the 1024-byte ALLOCATION (Zenith_SaveData.cpp:229-235), so
	//     `capacity - cursor` is 1024 there and the `< 4` clause is unreachable by
	//     construction;
	//   * every CorruptSlotPayloadOnDisk unit preserves the declared payload length
	//     and breaks the ENGINE's CRC32 instead, so LoadEx bails at
	//     Zenith_SaveData.cpp:312 and never invokes the read callback at all.
	// Only the DISK path can present a sub-prefix payload, because LoadEx wraps the
	// callback's stream with capacity == the header's DECLARED payload size
	// (Zenith_SaveData.cpp:317-319). Hence the hand-built file below.
	//
	// WHAT REDS THIS UNIT (both mutations are observable, and only from here):
	//   1. Weakening the guard's verdict -- returning SUCCESS, or any status other
	//      than CORRUPT_DATA, from that arm. ProbeSlot then answers READY and
	//      ReadState returns the wrong code; the two assertions at the bottom red.
	//   2. Deleting the clause outright. ReadPayloadCallback then asks a 2-byte stream
	//      for 4 bytes via xStream.ReadData, tripping Zenith_DataStream's bounds
	//      assert (Zenith_DataStream.h:154) -- fatal in every configuration. That hard
	//      abort on a hand-edited .zsave is precisely the hazard the guard prevents,
	//      and it is exactly as loud as a red.

	// ---- CONTROL ARM: prove the hand-built file format is ENGINE-VALID -----------
	// Without this, a helper that emitted a bad magic or a wrong CRC would make the
	// subject arm below pass for entirely the wrong reason -- LoadEx would reject the
	// file itself and report CORRUPT_DATA without the layer's guard ever running.
	const ZM_GameState xGolden = MakeGoldenV1State();
	const std::vector<uint8_t> xBlob = EncodeBlob(xGolden, "golden v1 state");
	ZENITH_ASSERT_EQ(xBlob.size(), (size_t)uGOLDEN_BLOB_BYTES, "golden blob length");
	if (xBlob.size() != (size_t)uGOLDEN_BLOB_BYTES) { return; }

	std::vector<uint8_t> xWellFormed;
	xWellFormed.push_back((uint8_t)(uGOLDEN_BLOB_BYTES & 0xffu));
	xWellFormed.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 8u) & 0xffu));
	xWellFormed.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 16u) & 0xffu));
	xWellFormed.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 24u) & 0xffu));
	xWellFormed.insert(xWellFormed.end(), xBlob.begin(), xBlob.end());
	WriteHandBuiltSlotFile(ZM_SAVE_SLOT_0, xWellFormed.data(), (uint64_t)xWellFormed.size());

	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_READY,
		"the hand-built control file was not accepted -- the fixture builder is wrong, "
		"so the subject arm below would prove nothing");
	ZM_GameState xControl;
	const Zenith_Status xControlRead = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xControl);
	AssertStatus(xControlRead, Zenith_ErrorCode::SUCCESS, "ReadState(hand-built control)");
	if (xControlRead.IsOk())
	{
		AssertStateEqual(xGolden, xControl, "hand-built control file");
	}

	// ---- SUBJECT ARM: a payload of TWO bytes, where four are needed --------------
	// Two bytes cannot hold the prefix under ANY reading, so the length is never even
	// decoded -- the guard rejects before the first ReadData.
	const uint8_t auSubPrefix[2] = { 0x02u, 0x00u };
	WriteHandBuiltSlotFile(ZM_SAVE_SLOT_0, auSubPrefix, 2ull);

	// The file really is header + exactly two payload bytes...
	std::vector<uint8_t> xOnDisk;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_0, xOnDisk),
		"the hand-built sub-prefix file is not on disk");
	ZENITH_ASSERT_EQ(xOnDisk.size(), sizeof(Zenith_SaveFileHeader) + (size_t)2u,
		"the hand-built sub-prefix file is not header + 2 bytes");
	// ...and check-then-use: the assert macros RECORD AND CONTINUE, so the header read
	// below must not run on a short buffer.
	if (xOnDisk.size() != sizeof(Zenith_SaveFileHeader) + (size_t)2u) { return; }

	// EVERY engine gate is satisfied by this file, asserted directly rather than
	// assumed. This is what makes the CORRUPT_DATA verdict attributable to THIS
	// layer's guard and not to Zenith_SaveData::LoadEx's own header/CRC checks.
	Zenith_SaveFileHeader xOnDiskHeader = {};
	memcpy(&xOnDiskHeader, xOnDisk.data(), sizeof(Zenith_SaveFileHeader));
	ZENITH_ASSERT_EQ((u_int)xOnDiskHeader.uMagic, (u_int)uZENITH_SAVE_MAGIC,
		"the fixture would be rejected at the engine's magic gate");
	ZENITH_ASSERT_EQ((u_int)xOnDiskHeader.uFormatVersion, (u_int)uZENITH_SAVE_FORMAT_VERSION,
		"the fixture would be rejected at the engine's format-version gate");
	ZENITH_ASSERT_TRUE(xOnDiskHeader.ulPayloadSize == 2ull,
		"the fixture must DECLARE a 2-byte payload, which is what sizes the callback's "
		"stream (Zenith_SaveData.cpp:317-319)");
	ZENITH_ASSERT_EQ((u_int)xOnDiskHeader.uChecksum,
		(u_int)Zenith_SaveData::ComputeCRC32(auSubPrefix, 2ull),
		"the fixture would be rejected at the engine's CRC gate before the callback runs");

	// DAMAGED, never EMPTY: a file IS present, and conflating the two is how a New
	// Game silently clobbers a recoverable save (Docs/SaveFormat.md:318-321).
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_DAMAGED,
		"a payload too small to hold the length prefix must probe DAMAGED");

	ZM_GameState xDestination = MakeMaximalState(1u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::CORRUPT_DATA, "ReadState(sub-prefix payload)");
	// TRANSACTIONAL: the caller's state survives a file this malformed untouched.
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(sub-prefix payload)");

	// ...and neither the probe nor the read repaired, truncated or deleted it.
	ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0)),
		"reading a sub-prefix slot deleted its file");
	std::vector<uint8_t> xAfter;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_0, xAfter),
		"the sub-prefix file disappeared across a probe + read");
	ZENITH_ASSERT_EQ(xAfter.size(), xOnDisk.size(),
		"the sub-prefix file changed length across a probe + read");
	if (xAfter.size() == xOnDisk.size())
	{
		ZENITH_ASSERT_TRUE(memcmp(xAfter.data(), xOnDisk.data(), xOnDisk.size()) == 0,
			"the sub-prefix file was rewritten across a probe + read");
	}
	// The scope's destructor deletes all four _Test files, so this unit leaves NOTHING
	// on disk -- not the control file and not the sub-prefix one.
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadRejectsAnOversizedPrefix)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadRejectsAnOversizedPrefix")) { return; }

	// 0xFFFFFFFF followed by a perfectly VALID blob: the blob is fine, only the
	// prefix lies.
	//
	// HONEST SCOPE: this pins DEFENCE IN DEPTH, not the only thing between a lying
	// prefix and a buffer overrun. The frozen codec applies an essentially identical
	// predicate at the same cursor (ZM_SaveSchema.cpp:1096-1101 rejects a length
	// greater than the remaining capacity, or zero, as Header/payloadLength) and
	// publishes into the destination only after full validation (:1162). So deleting
	// ZM_SaveSlots' own `ulLength > ulRemaining` branch would leave this unit GREEN --
	// the CORRUPT_DATA and the untouched destination would then come from the codec.
	// What this unit does pin is the OUTCOME contract of the layer as a whole: that a
	// lying prefix is reported as CORRUPT_DATA (not BAD_MAGIC, not SUCCESS) and that
	// the caller's state survives. It reds if the layer forwards a bogus length as a
	// different status, or pre-clears/partially publishes the destination.
	const std::vector<uint8_t> xBlob = EncodeBlob(MakeGoldenV1State(), "golden v1 state");
	ZENITH_ASSERT_EQ(xBlob.size(), (size_t)uGOLDEN_BLOB_BYTES, "golden blob length");
	if (xBlob.empty()) { return; }

	std::vector<uint8_t> xStaged;
	xStaged.push_back(0xffu);
	xStaged.push_back(0xffu);
	xStaged.push_back(0xffu);
	xStaged.push_back(0xffu);
	xStaged.insert(xStaged.end(), xBlob.begin(), xBlob.end());

	Zenith_SaveData::SetReadbackForTest(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0),
		ZM_SaveSlots::uGAME_VERSION, xStaged.data(), (uint64_t)xStaged.size());

	ZM_GameState xDestination = MakeMaximalState(2u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);

	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::CORRUPT_DATA, "ReadState(oversized prefix)");
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(oversized prefix)");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadRejectsAStagedPayloadWithACorruptInnerMagic)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadRejectsAStagedPayloadWithACorruptInnerMagic")) { return; }

	// WHY A STAGED READBACK RATHER THAN A BYTE FLIP ON DISK: every other damaged-slot
	// unit in this file manufactures its damage with CorruptSlotPayloadOnDisk, which
	// breaks the ENGINE's CRC32 -- LoadEx then returns CORRUPT_DATA at
	// Zenith_SaveData.cpp:312 and NEVER invokes the read callback. That leaves THIS
	// layer's own payload-rejection path (ReadPayloadCallback -> ZM_SaveSchema::Read
	// -> ProbeSlot's `xUser.m_xStatus.IsOk()` arm) completely unexecuted, so reducing
	// ZM_SaveSlots to engine-status-only logic would leave every one of those units
	// green. LoadEx consults the readback stash BEFORE it touches the file at all
	// (Zenith_SaveData.cpp:219-238) and DOES invoke pfnReadPayload on that path (:235),
	// which is the only way to reach the codec with the file CRC bypassed.

	// Write a GOOD save first so the slot genuinely exists: ProbeSlot short-circuits
	// to EMPTY at its Zenith_SaveData::SlotExists check (ZM_SaveSlots.cpp) and would
	// never reach LoadEx without a file, making the DAMAGED verdict unreachable.
	const ZM_GameState xGolden = MakeGoldenV1State();
	AssertStatus(ZM_SaveSlots::WriteState(xGolden, ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(golden, Save0)");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_READY, "the fixture must be READY before the payload is poisoned");

	std::vector<uint8_t> xDiskBefore;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_0, xDiskBefore),
		"the fixture file could not be read back");

	std::vector<uint8_t> xBlob = EncodeBlob(xGolden, "golden v1 state");
	ZENITH_ASSERT_EQ(xBlob.size(), (size_t)uGOLDEN_BLOB_BYTES, "golden blob length");
	if (xBlob.size() != (size_t)uGOLDEN_BLOB_BYTES) { return; }

	// Byte 0 of a ZMSV blob is the low byte of ZM_SaveSchema::uMAGIC. Pinned, so this
	// unit fails loudly rather than silently corrupting some unrelated field if the
	// blob layout ever moves.
	const uint8_t uOriginalMagicByte = xBlob[0];
	ZENITH_ASSERT_EQ((u_int)uOriginalMagicByte, (u_int)(ZM_SaveSchema::uMAGIC & 0xffu),
		"byte 0 of a ZMSV blob is no longer the magic's low byte");
	xBlob[0] = (uint8_t)(uOriginalMagicByte ^ 0xffu);
	ZENITH_ASSERT_NE((u_int)xBlob[0], (u_int)uOriginalMagicByte,
		"the magic byte was not actually mutated");

	// [u32 LE length][blob] -- exactly the framing WriteState produces, with a HONEST
	// length. Only the magic is wrong, so any rejection can only come from the
	// codec's magic gate, never from the prefix bounds check above it.
	std::vector<uint8_t> xStaged;
	xStaged.push_back((uint8_t)(uGOLDEN_BLOB_BYTES & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 8u) & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 16u) & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 24u) & 0xffu));
	xStaged.insert(xStaged.end(), xBlob.begin(), xBlob.end());
	Zenith_SaveData::SetReadbackForTest(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0),
		ZM_SaveSlots::uGAME_VERSION, xStaged.data(), (uint64_t)xStaged.size());

	// The FILE is intact and its CRC still verifies, so LoadEx would report SUCCESS on
	// the disk path: a layer that only forwarded the engine's status reports READY
	// here. The DAMAGED verdict can only come from this layer's own inspection.
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_DAMAGED,
		"a payload the codec rejects must probe DAMAGED even though the file is intact");

	ZM_GameState xDestination = MakeMaximalState(1u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_0, xDestination);
	// The codec's SPECIFIC reason, forwarded verbatim (ZM_SaveSchema.cpp:1107-1111).
	// Reds if ReadState collapses the callback status into CORRUPT_DATA, or answers
	// from Zenith_SaveData::LoadEx's return value alone (which is SUCCESS here).
	AssertStatus(xStatus, Zenith_ErrorCode::BAD_MAGIC, "ReadState(bad inner magic)");
	AssertStateByteIdentical(xBefore, xDestination, "ReadState(bad inner magic)");

	// ...and neither call repaired, truncated or deleted the file it rejected.
	std::vector<uint8_t> xDiskAfter;
	ZENITH_ASSERT_TRUE(ReadSlotFileBytes(ZM_SAVE_SLOT_0, xDiskAfter),
		"the rejected slot's file disappeared");
	ZENITH_ASSERT_EQ(xDiskAfter.size(), xDiskBefore.size(),
		"the rejected slot's file changed length");
	if (!xDiskBefore.empty() && xDiskAfter.size() == xDiskBefore.size())
	{
		ZENITH_ASSERT_TRUE(
			memcmp(xDiskAfter.data(), xDiskBefore.data(), xDiskBefore.size()) == 0,
			"the rejected slot's file was rewritten");
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_RecordedGameVersionIsTheDeclaredConstant)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_RecordedGameVersionIsTheDeclaredConstant")) { return; }

	AssertStatus(ZM_SaveSlots::WriteState(MakeGoldenV1State(), ZM_SAVE_SLOT_1),
		Zenith_ErrorCode::SUCCESS, "WriteState(golden, Save1)");

	const Zenith_Vector<Zenith_SaveData::WrittenSlot>& xLog =
		Zenith_SaveData::GetWrittenSlotsForTest();
	ZENITH_ASSERT_EQ(xLog.GetSize(), 1u, "exactly one Save() must have been recorded");
	if (xLog.GetSize() != 1u) { return; }

	// The declared constant is itself pinned to a LITERAL first. Without this, the
	// comparison below has the production constant on BOTH sides and survives any
	// change to it -- it would only catch WriteState passing something else entirely.
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::uGAME_VERSION, 1u,
		"the engine-header game version for this layer is v1");

	const Zenith_SaveData::WrittenSlot& xEntry = xLog.GetBack();
	// The engine header's uGameVersion is metadata only: LoadEx never inspects it
	// (Zenith_SaveData.cpp:321) and the REAL gate lives inside the ZMSV payload. It
	// must carry the layer's own declared constant.
	ZENITH_ASSERT_EQ(xEntry.m_uGameVersion, ZM_SaveSlots::uGAME_VERSION,
		"the engine header carries the wrong game version");
	// ...stamped against the slot that was actually asked for. Compared to the
	// LITERAL alias, not to SlotName(ZM_SAVE_SLOT_1): with the production map on both
	// sides, a WriteState that resolved the name from the WRONG ordinal would still
	// match. Reds if WriteState passes SlotShippingName, a hardcoded slot, or any
	// other ordinal to Zenith_SaveData::Save.
	ZENITH_ASSERT_STREQ(xEntry.m_strSlotName.c_str(), aszEXPECTED_TEST_NAMES[1],
		"the recorded slot name is not the slot that was written");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteRejectsAnOutOfRangeSlot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteRejectsAnOutOfRangeSlot")) { return; }

	const ZM_GameState xState = ZM_MakeStarterGameState();
	const ZM_SAVE_SLOT aeBadSlots[] = { ZM_SAVE_SLOT_NONE, (ZM_SAVE_SLOT)uGARBAGE_SLOT_ID };
	for (u_int u = 0u; u < 2u; ++u)
	{
		const Zenith_Status xStatus = ZM_SaveSlots::WriteState(xState, aeBadSlots[u]);
		AssertStatus(xStatus, Zenith_ErrorCode::INVALID_ARGUMENT, "WriteState(bad slot)");
	}

	// An out-of-range id must never reach Zenith_SaveData::Save at all -- BuildSlotPath
	// performs zero sanitisation, so a path would be built from garbage.
	ZENITH_ASSERT_EQ(Zenith_SaveData::GetWrittenSlotsForTest().GetSize(), 0u,
		"a rejected slot id still reached the engine save path");
	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(aeALL_SLOTS[u])),
			"a rejected write created a file for slot %u", u);
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_ReadRejectsAnOutOfRangeSlot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The scope is not strictly needed (a rejected id never
	// reaches disk) -- it is here so that a REGRESSION which does reach disk hits the
	// _Test files rather than the player's.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_ReadRejectsAnOutOfRangeSlot")) { return; }

	// The write side of this is covered by Slot_WriteRejectsAnOutOfRangeSlot; the READ
	// side had no unit at all, so ReadState's documented INVALID_ARGUMENT return was
	// entirely unexecuted.
	ZM_GameState xDestination = MakeMaximalState(1u);
	const std::vector<uint8_t> xBefore = SnapshotState(xDestination);

	const ZM_SAVE_SLOT aeBadSlots[] = { ZM_SAVE_SLOT_NONE, (ZM_SAVE_SLOT)uGARBAGE_SLOT_ID };
	for (u_int u = 0u; u < 2u; ++u)
	{
		// INVALID_ARGUMENT specifically, never FILE_NOT_FOUND: deleting the empty-name
		// guard would send "" to Zenith_SaveData::LoadEx, whose BuildSlotPath
		// snprintfs it straight into "<savedir>.zsave" with zero sanitisation
		// (Zenith_SaveData.cpp:95-98) and then reports FILE_NOT_FOUND. Pinning the
		// exact code is what distinguishes "refused" from "looked and found nothing".
		const Zenith_Status xStatus = ZM_SaveSlots::ReadState(aeBadSlots[u], xDestination);
		AssertStatus(xStatus, Zenith_ErrorCode::INVALID_ARGUMENT, "ReadState(bad slot)");
		// TRANSACTIONAL even on the reject path: nothing is read, so nothing is
		// published -- not even a pre-clear of the destination.
		AssertStateByteIdentical(xBefore, xDestination, "ReadState(bad slot)");
	}

	// A rejected id must never have addressed the engine at all.
	ZENITH_ASSERT_EQ(Zenith_SaveData::GetWrittenSlotsForTest().GetSize(), 0u,
		"a read of a rejected slot id reached the engine save path");
	for (u_int u = 0u; u < uALL_SLOT_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(aeALL_SLOTS[u])),
			"a rejected read created a file for slot %u", u);
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteRejectsAnInvalidStateWithoutCreatingAFile)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteRejectsAnInvalidStateWithoutCreatingAFile")) { return; }

	// A party count past uZM_MAX_PARTY_SIZE -- ZM_SaveSchema::ValidateState's very
	// first check. m_uCount is a public field, so this shape is reachable exactly the
	// way a corrupted or mis-authored state would be.
	ZM_GameState xInvalid = ZM_MakeStarterGameState();
	xInvalid.m_xParty.m_uCount = 7u;

	const Zenith_Status xStatus = ZM_SaveSlots::WriteState(xInvalid, ZM_SAVE_SLOT_0);
	AssertStatus(xStatus, Zenith_ErrorCode::INVALID_ARGUMENT, "WriteState(invalid state)");

	// The codec validates BEFORE producing a byte, so a rejected state must leave the
	// slot exactly as it was -- no truncated file, no empty file.
	ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_0)),
		"a rejected state left a file on disk");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_0),
		(u_int)ZM_SAVE_SLOT_EMPTY, "a rejected state left the slot non-empty");
	ZENITH_ASSERT_EQ(Zenith_SaveData::GetWrittenSlotsForTest().GetSize(), 0u,
		"a rejected state still reached the engine save path");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteAnswersFromTheVerifyProbeNotFromSaveReturn)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteAnswersFromTheVerifyProbeNotFromSaveReturn")) { return; }

	// THE unit that pins WriteState's verify RE-PROBE. Nothing else does: every other
	// write unit here writes a good state to a working disk, so deleting the re-probe
	// and returning `true` immediately after Zenith_SaveData::Save -- which returns
	// the literal `true` on EVERY path (Zenith_SaveData.cpp:204), because
	// Zenith_DataStream::WriteToFile is void -- leaves all of them green.
	//
	// The lever: stage a payload the codec rejects for the TARGET slot BEFORE calling
	// WriteState. The verify probe runs through ProbeSlot -> Zenith_SaveData::LoadEx,
	// and LoadEx consults the readback stash before the file (Zenith_SaveData.cpp:
	// 219-238), so the probe reads the poisoned bytes while the file that actually
	// landed on disk is perfectly good. Only a layer that BELIEVES its re-probe can
	// report corruption here.
	const ZM_GameState xGolden = MakeGoldenV1State();
	std::vector<uint8_t> xBlob = EncodeBlob(xGolden, "golden v1 state");
	ZENITH_ASSERT_EQ(xBlob.size(), (size_t)uGOLDEN_BLOB_BYTES, "golden blob length");
	if (xBlob.size() != (size_t)uGOLDEN_BLOB_BYTES) { return; }
	xBlob[0] = (uint8_t)(xBlob[0] ^ 0xffu);   // break the ZMSV magic; honest length

	std::vector<uint8_t> xStaged;
	xStaged.push_back((uint8_t)(uGOLDEN_BLOB_BYTES & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 8u) & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 16u) & 0xffu));
	xStaged.push_back((uint8_t)((uGOLDEN_BLOB_BYTES >> 24u) & 0xffu));
	xStaged.insert(xStaged.end(), xBlob.begin(), xBlob.end());
	Zenith_SaveData::SetReadbackForTest(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_1),
		ZM_SaveSlots::uGAME_VERSION, xStaged.data(), (uint64_t)xStaged.size());

	// CORRUPT_DATA, not SUCCESS: the write "succeeded" by every signal except the one
	// this layer is documented to trust.
	const Zenith_Status xStatus = ZM_SaveSlots::WriteState(xGolden, ZM_SAVE_SLOT_1);
	AssertStatus(xStatus, Zenith_ErrorCode::CORRUPT_DATA,
		"WriteState must answer from its verify probe");

	// Non-vacuity, all three parts. (a) The write really did reach the engine, so
	// this is not a state/slot rejection dressed up as corruption.
	ZENITH_ASSERT_EQ(Zenith_SaveData::GetWrittenSlotsForTest().GetSize(), 1u,
		"WriteState never reached the engine save path");
	// (b) A file really did land, so the CORRUPT_DATA arm was taken and not the
	// FILE_NOT_FOUND one.
	ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_1)),
		"WriteState created no file, so the verdict came from the wrong branch");
	// (c) The bytes handed to the engine are the GOOD ones -- the poison is a
	// read-side fixture and must never have been written. If this fired, the unit
	// would be proving something else entirely.
	std::vector<uint8_t> xPayload;
	ZENITH_ASSERT_TRUE(CaptureLastWrittenPayload(xPayload), "no Save() was recorded");
	ZENITH_ASSERT_EQ(xPayload.size(),
		(size_t)uEXPECTED_PREFIX_BYTES + (size_t)uGOLDEN_BLOB_BYTES, "written payload size");
	if (xPayload.size() >= (size_t)uEXPECTED_PREFIX_BYTES + 1u)
	{
		ZENITH_ASSERT_EQ((u_int)xPayload[uEXPECTED_PREFIX_BYTES],
			(u_int)(ZM_SaveSchema::uMAGIC & 0xffu),
			"the poisoned read fixture leaked into the bytes that were written");
	}
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_DeleteRemovesTheFileAndReportsFalseWhenAlreadyAbsent)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_DeleteRemovesTheFileAndReportsFalseWhenAlreadyAbsent")) { return; }

	// Phase 1 -- a present file is deleted and reported as deleted.
	AssertStatus(ZM_SaveSlots::WriteState(ZM_MakeStarterGameState(), ZM_SAVE_SLOT_2),
		Zenith_ErrorCode::SUCCESS, "WriteState(starter, Save2)");
	ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_2)),
		"the fixture file was not created");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::DeleteSlotFile(ZM_SAVE_SLOT_2),
		"deleting a present slot reported failure");
	ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(ZM_SaveSlots::SlotName(ZM_SAVE_SLOT_2)),
		"the file survived DeleteSlotFile");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2),
		(u_int)ZM_SAVE_SLOT_EMPTY, "a deleted slot must probe EMPTY");

	// Phase 2 -- deleting again must report FALSE, or a UI would claim it erased
	// something that was never there.
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::DeleteSlotFile(ZM_SAVE_SLOT_2),
		"deleting an absent slot reported success");
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::DeleteSlotFile(ZM_SAVE_SLOT_1),
		"deleting a never-written slot reported success");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_AnySlotOccupiedCountsDamagedButAnySlotReadyDoesNot)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_AnySlotOccupiedCountsDamagedButAnySlotReadyDoesNot")) { return; }

	// Phase 1 -- nothing on disk.
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::AnySlotOccupied(), "empty disk reported as occupied");
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::AnySlotReady(), "empty disk reported as ready");

	// Phase 2 -- exactly ONE damaged file, in a slot that is NOT Save0, so a
	// predicate that only ever checks Save0 is caught too.
	AssertStatus(ZM_SaveSlots::WriteState(MakeMaximalState(0u), ZM_SAVE_SLOT_2),
		Zenith_ErrorCode::SUCCESS, "WriteState(maximal, Save2)");
	ZENITH_ASSERT_TRUE(CorruptSlotPayloadOnDisk(ZM_SAVE_SLOT_2),
		"could not corrupt the Save2 file for the test");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2),
		(u_int)ZM_SAVE_SLOT_DAMAGED, "the fixture slot is not DAMAGED");

	// A DAMAGED slot COUNTS AS OCCUPIED. If it did not, FrontEnd would hide Continue
	// and let New Game silently clobber a recoverable file.
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::AnySlotOccupied(),
		"a damaged save must keep the slot OCCUPIED");
	ZENITH_ASSERT_FALSE(ZM_SaveSlots::AnySlotReady(),
		"a damaged save must NOT count as ready");

	// Phase 3 -- add a good Save0 alongside it.
	AssertStatus(ZM_SaveSlots::WriteState(ZM_MakeStarterGameState(), ZM_SAVE_SLOT_0),
		Zenith_ErrorCode::SUCCESS, "WriteState(starter, Save0)");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::AnySlotOccupied(), "a good save is not occupied");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::AnySlotReady(), "a good save is not ready");
	// ...and the damaged neighbour was not repaired along the way.
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_2),
		(u_int)ZM_SAVE_SLOT_DAMAGED, "writing another slot altered the damaged one");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

ZENITH_TEST(ZM_Save, Slot_WriteThenProbeThenReadComposes)
{
#ifdef ZENITH_INPUT_SIMULATOR
	// Scope FIRST: it redirects SlotName onto the _Test aliases BEFORE this unit
	// resolves a single name, then deletes all four _Test files. So this unit assumes
	// NO pre-existing file, starts from a known-empty disk even on a dirty machine,
	// and leaves nothing behind. The redirection being live is a HARD requirement --
	// a skip here would be counted as a PASS by the gate.
	ZM_SlotDiskScope xScope;
	if (!RequireTestSlotNames(xScope, "Slot_WriteThenProbeThenReadComposes")) { return; }

	// The whole happy path in ONE unit, so a refactor that breaks the seam BETWEEN
	// write and probe (or probe and read) is caught even when each half is green.
	const ZM_GameState xSource = MakeMaximalState(4u);

	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO),
		(u_int)ZM_SAVE_SLOT_EMPTY, "the slot must start EMPTY");
	AssertStatus(ZM_SaveSlots::WriteState(xSource, ZM_SAVE_SLOT_AUTO),
		Zenith_ErrorCode::SUCCESS, "WriteState");
	ZENITH_ASSERT_EQ((u_int)ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT_AUTO),
		(u_int)ZM_SAVE_SLOT_READY, "the slot must be READY after the write");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::AnySlotOccupied(), "AnySlotOccupied missed the write");
	ZENITH_ASSERT_TRUE(ZM_SaveSlots::AnySlotReady(), "AnySlotReady missed the write");

	ZM_GameState xDestination;
	const Zenith_Status xStatus = ZM_SaveSlots::ReadState(ZM_SAVE_SLOT_AUTO, xDestination);
	AssertStatus(xStatus, Zenith_ErrorCode::SUCCESS, "ReadState");
	if (!xStatus.IsOk()) { return; }
	AssertStateEqual(xSource, xDestination, "write -> probe -> read");
#else
	ZENITH_SKIP("save-slot disk instrumentation is unavailable in this configuration");
#endif
}

// ============================================================================
// The pure save-permission policy. No disk, no instrumentation.
// ============================================================================

ZENITH_TEST(ZM_Save, Blocker_PrecedenceIsFixedAndTotal)
{
	ZENITH_ASSERT_EQ(uBLOCKER_CASE_COUNT, 16u,
		"the truth table must enumerate all 16 boolean combinations");

	u_int uNoneCount = 0u;
	for (u_int u = 0u; u < uBLOCKER_CASE_COUNT; ++u)
	{
		const ZM_BlockerCase& xCase = axBLOCKER_CASES[u];
		const ZM_SaveSlots::ZM_SAVE_BLOCKER eActual = ZM_SaveSlots::ResolveSaveBlocker(
			xCase.m_bOverworld, xCase.m_bBattle, xCase.m_bWarp, xCase.m_bWhiteout);
		ZENITH_ASSERT_EQ((u_int)eActual, (u_int)xCase.m_eExpected,
			"case %u (overworld=%u battle=%u warp=%u whiteout=%u)",
			u, (u_int)xCase.m_bOverworld, (u_int)xCase.m_bBattle,
			(u_int)xCase.m_bWarp, (u_int)xCase.m_bWhiteout);
		if (eActual == ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE) { ++uNoneCount; }
	}

	// NONE is reachable from EXACTLY ONE combination. An `&&` that became an `||`
	// (or a dropped condition) shows up here as an extra permissive case.
	ZENITH_ASSERT_EQ(uNoneCount, 1u,
		"only (overworld, no battle, no warp, no whiteout) may permit a save");
	ZENITH_ASSERT_EQ(
		(u_int)ZM_SaveSlots::ResolveSaveBlocker(true, false, false, false),
		(u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE,
		"a quiet overworld must permit a save");
}

ZENITH_TEST(ZM_Save, Blocker_NameIsTotalAndNeverNull)
{
	// The exact diagnostic copy, pinned literally and in ordinal order. Without this
	// the unit only proves the names are non-null and distinct -- which a table of
	// "A"/"B"/"C"/"D" would also satisfy. Reds if a switch arm is deleted (its enum
	// value then falls through to "UNKNOWN") or if two arms are swapped.
	const char* const aszEXPECTED_BLOCKER_NAMES[(u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT] =
	{
		"NONE",
		"NOT_OVERWORLD",
		"BATTLE",
		"WARP",
		"PENDING_WHITEOUT",
	};

	for (u_int u = 0u; u < (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT; ++u)
	{
		const char* szName =
			ZM_SaveSlots::SaveBlockerName((ZM_SaveSlots::ZM_SAVE_BLOCKER)u);
		ZENITH_ASSERT_NOT_NULL(szName, "SaveBlockerName(%u) is null", u);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "SaveBlockerName(%u) is empty", u);
		ZENITH_ASSERT_STREQ(szName, aszEXPECTED_BLOCKER_NAMES[u], "blocker %u name", u);
	}

	// Distinct, so a diagnostic can actually identify which blocker fired.
	for (u_int uA = 0u; uA < (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT; ++uB)
		{
			const char* szA = ZM_SaveSlots::SaveBlockerName((ZM_SaveSlots::ZM_SAVE_BLOCKER)uA);
			const char* szB = ZM_SaveSlots::SaveBlockerName((ZM_SaveSlots::ZM_SAVE_BLOCKER)uB);
			if (szA == nullptr || szB == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szA, szB) != 0,
				"blockers %u and %u share the name '%s'", uA, uB, szA);
		}
	}

	// TOTAL: an out-of-range cast must not index the table unguarded.
	const char* szOutOfRange = ZM_SaveSlots::SaveBlockerName(
		(ZM_SaveSlots::ZM_SAVE_BLOCKER)((u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT + 5u));
	ZENITH_ASSERT_NOT_NULL(szOutOfRange, "SaveBlockerName(out of range) is null");
	if (szOutOfRange != nullptr)
	{
		for (u_int u = 0u; u < (u_int)ZM_SaveSlots::ZM_SAVE_BLOCKER_COUNT; ++u)
		{
			const char* szReal = ZM_SaveSlots::SaveBlockerName((ZM_SaveSlots::ZM_SAVE_BLOCKER)u);
			if (szReal == nullptr) { continue; }
			ZENITH_ASSERT_TRUE(strcmp(szOutOfRange, szReal) != 0,
				"an out-of-range blocker renders as the real name '%s'", szReal);
		}
	}
}
