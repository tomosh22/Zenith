#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerData -- integrity of the authored trainer roster (category
// ZM_Data), S7 item 3 SC2. The table is CONTENT, so every failure mode here is
// an authoring mistake that would otherwise surface as a broken battle: a
// trainer with no team, a team the engine cannot build, a prize that pays
// nothing, a defeat flag that writes no story beat, or an ZM_AI_TIER_NONE
// opponent that cannot choose an action.
//
// Everything is PURE: a compiled const table plus pure free functions. No ECS,
// no scene, no UI, no disk, no baked assets -- so no RequestSkip is needed.
//
// Every "walk everything" unit GUARDS its walk with a non-zero total first: a
// loop bounded by a count that is itself zero passes vacuously and would keep
// passing after the content it exists to police was deleted.
//
// Every assert below must be reddable by an edit to ZM_TrainerData.{h,cpp} and
// by nothing else. A check that only a defect in ANOTHER table could break does
// not belong here: it cannot catch an authoring mistake in this one, and when it
// does fire it blames the trainer row for a bug that unit already owns. The
// species-name round-trip (ZM_Tests_DataRegistry) and the "every species learns
// something early" learnset guarantee (ZM_Tests_Learnsets) are both owned
// elsewhere, over their FULL domain, and are deliberately not re-asserted here.
//
// The accessor units drive ZM_GetTrainerData with garbage ids ON PURPOSE. That
// path logs a non-fatal Zenith_Error and returns the UNKNOWN row -- the error
// lines in the boot log are EXPECTED output of this suite, not failures.
// ============================================================================

#include <cstring>   // strcmp (display-name distinctness)

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"                 // the totality proof
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"            // ZM_AI_TIER_COUNT / _NONE
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"           // ZM_SPECIES_COUNT + the authored species ids
#include "Zenithmon/Source/Data/ZM_StatCalc.h"              // uZM_MIN_LEVEL / uZM_MAX_LEVEL
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"            // ZM_STORY_FLAG_COUNT / _NONE
#include "Zenithmon/Source/Data/ZM_TrainerData.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"            // uZM_MONEY_CAP

namespace
{
	// Spelled in the TEST, not read back off the table, so "the roster changed"
	// is a failure rather than a silently-agreeing tautology.
	constexpr u_int uEXPECTED_TRAINERS = 2u;

	// The authored values, spelled here so each pin cannot degenerate into
	// "whatever the row happens to say".
	constexpr u_int uEXPECTED_VESPER_PARTY   = 1u;
	constexpr u_int uEXPECTED_VESPER_LEVEL   = 5u;
	constexpr u_int uEXPECTED_VESPER_PRIZE   = 500u;
	constexpr u_int uEXPECTED_RAMBLER_PARTY  = 2u;
	constexpr u_int uEXPECTED_RAMBLER_LEVEL  = 4u;
	constexpr u_int uEXPECTED_RAMBLER_PRIZE  = 120u;

	const ZM_TrainerData& Trainer(u_int i) { return ZM_GetTrainerData((ZM_TRAINER_ID)i); }

	// Every rows-x-party walk opens by asserting this is non-zero. Without it such
	// a walk runs ZERO iterations -- and so keeps passing -- the moment the parties
	// it exists to police are emptied.
	u_int TotalPartyMembers()
	{
		u_int uTotal = 0u;
		for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
		{
			uTotal += Trainer(i).m_uPartyCount;
		}
		return uTotal;
	}

	// The ids no roster may ever name: the sentinel, one just past it, and two
	// pieces of outright garbage.
	const ZM_TRAINER_ID aeUNREGISTERED[] =
	{
		ZM_TRAINER_NONE,
		(ZM_TRAINER_ID)((u_int)ZM_TRAINER_COUNT + 7u),
		(ZM_TRAINER_ID)9999u,
		(ZM_TRAINER_ID)~0u
	};
	constexpr u_int uUNREGISTERED_COUNT =
		(u_int)(sizeof(aeUNREGISTERED) / sizeof(aeUNREGISTERED[0]));

	// A row's party pointer is only safe to walk once the count says it is
	// populated AND the pointer is non-null; the assert macros RECORD AND CONTINUE,
	// so a walk that trusted a failed assert would dereference null and take the
	// whole boot unit run with it.
	bool PartyIsWalkable(const ZM_TrainerData& x)
	{
		return x.m_paxParty != nullptr && x.m_uPartyCount > 0u
			&& x.m_uPartyCount <= uZM_TRAINER_MAX_PARTY;
	}

	// Likewise for a species id: ZM_GetSpeciesData ASSERTS on an out-of-range id
	// (it is not one of the total accessors), so nothing may reach it until the
	// range check has passed. ZM_SPECIES_NONE aliases ZM_SPECIES_COUNT, so this
	// one comparison rejects the unauthored sentinel and every garbage value.
	bool SpeciesIsResolvable(ZM_SPECIES_ID eSpecies)
	{
		return (u_int)eSpecies < (u_int)ZM_SPECIES_COUNT;
	}
}

// ---- Table shape ------------------------------------------------------------

ZENITH_TEST(ZM_Data, Trainer_CountMatchesEnum)
{
	ZENITH_ASSERT_EQ(ZM_GetTrainerCount(), uEXPECTED_TRAINERS,
		"the roster changed size; every pinned row below must be reviewed with it");
	ZENITH_ASSERT_EQ((u_int)ZM_TRAINER_COUNT, uEXPECTED_TRAINERS,
		"the enum gained or lost an id");
	ZENITH_ASSERT_EQ((u_int)ZM_TRAINER_NONE, (u_int)ZM_TRAINER_COUNT,
		"the sentinel must keep aliasing COUNT -- ZM_IsRegisteredTrainer rejects both "
		"with one compare");
}

ZENITH_TEST(ZM_Data, Trainer_EveryRowIdMatchesItsIndex)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes every walk in this suite vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Trainer(i).m_eId, i,
			"trainer row %u has mismatched m_eId", i);
	}
}

ZENITH_TEST(ZM_Data, Trainer_TableHoldsAtLeastTwoRowsWithDistinctIds)
{
	ZENITH_ASSERT_GE(ZM_GetTrainerCount(), 2u,
		"a one-row roster leaves every accessor only ever seeing index 0");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		for (u_int j = i + 1u; j < (u_int)ZM_TRAINER_COUNT; ++j)
		{
			ZENITH_ASSERT_NE((u_int)Trainer(i).m_eId, (u_int)Trainer(j).m_eId,
				"rows %u and %u claim the same trainer id", i, j);
		}
	}
}

// TWO PASSES, and the ordering is deliberate. The assert macros record a failure
// without aborting the body, so a single interleaved loop would strcmp a name
// that had not yet been null-checked -- and an enumerator added with no matching
// row zero-inits to null, turning a named failure into a hard UB crash during
// units-at-boot.
ZENITH_TEST(ZM_Data, Trainer_DisplayNamesNonEmptyAndUnique)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes both passes below vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const char* szName = Trainer(i).m_szDisplayName;
		ZENITH_ASSERT_NOT_NULL(szName, "trainer row %u has no display name", i);
		ZENITH_ASSERT_TRUE(szName != nullptr && szName[0] != '\0',
			"trainer row %u has an empty display name", i);
	}

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const char* szA = Trainer(i).m_szDisplayName;
		if (szA == nullptr || szA[0] == '\0')
		{
			continue;
		}
		for (u_int j = i + 1u; j < (u_int)ZM_TRAINER_COUNT; ++j)
		{
			const char* szB = Trainer(j).m_szDisplayName;
			if (szB == nullptr || szB[0] == '\0')
			{
				continue;
			}
			ZENITH_ASSERT_NE(strcmp(szA, szB), 0,
				"trainer rows %u and %u share the display name '%s'", i, j, szA);
		}
	}
}

// ---- Party integrity --------------------------------------------------------

ZENITH_TEST(ZM_Data, Trainer_EveryPartyIsNonEmptyAndWithinTheEngineCap)
{
	ZENITH_ASSERT_GT(TotalPartyMembers(), 0u,
		"no trainer in the roster brings a team at all");
	// NOT re-asserted here: "uZM_TRAINER_MAX_PARTY == uZM_MAX_PARTY_SIZE". The
	// header DEFINES the former as the latter, so the two names are one value by
	// construction and no edit to this game's data can separate them -- it would be
	// a check that can never fire, dressed as a drift guard.

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		ZENITH_ASSERT_NOT_NULL(x.m_paxParty,
			"%s claims a party but has no array", ZM_GetTrainerName(x.m_eId));
		ZENITH_ASSERT_GT(x.m_uPartyCount, 0u,
			"%s brings no monsters and cannot battle", ZM_GetTrainerName(x.m_eId));
		ZENITH_ASSERT_LE(x.m_uPartyCount, uZM_TRAINER_MAX_PARTY,
			"%s brings %u monsters, past the engine party cap of %u",
			ZM_GetTrainerName(x.m_eId), x.m_uPartyCount, uZM_TRAINER_MAX_PARTY);
	}
}

ZENITH_TEST(ZM_Data, Trainer_EveryPartySpeciesResolves)
{
	ZENITH_ASSERT_GT(TotalPartyMembers(), 0u,
		"no trainer brings a team, so the walk below is vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		if (!PartyIsWalkable(x))
		{
			continue;
		}
		for (u_int uSlot = 0u; uSlot < x.m_uPartyCount; ++uSlot)
		{
			const ZM_SPECIES_ID eSpecies = x.m_paxParty[uSlot].m_eSpecies;
			// The ONE species check this table can actually break: ZM_SPECIES_NONE
			// aliases ZM_SPECIES_COUNT, so an unauthored / mis-typed slot fails here.
			// A name round-trip is NOT added on top -- ZM_Tests_DataRegistry already
			// pins it for every id this guard admits, so it could only red on a
			// ZM_SpeciesData defect while naming the trainer row as the culprit.
			ZENITH_ASSERT_TRUE(SpeciesIsResolvable(eSpecies),
				"%s party slot %u names a non-species (%u)",
				ZM_GetTrainerName(x.m_eId), uSlot, (u_int)eSpecies);
		}
	}
}

ZENITH_TEST(ZM_Data, Trainer_EveryPartyLevelIsInRange)
{
	ZENITH_ASSERT_GT(TotalPartyMembers(), 0u,
		"no trainer brings a team, so the walk below is vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		if (!PartyIsWalkable(x))
		{
			continue;
		}
		for (u_int uSlot = 0u; uSlot < x.m_uPartyCount; ++uSlot)
		{
			const u_int uLevel = x.m_paxParty[uSlot].m_uLevel;
			// These are the exact bounds ZM_BuildBattleMonster asserts on at battle
			// start, so an out-of-band authored level would not be a red unit later --
			// it would be a process break the first time SC5 started the fight.
			ZENITH_ASSERT_GE(uLevel, uZM_MIN_LEVEL,
				"%s party slot %u is below level %u",
				ZM_GetTrainerName(x.m_eId), uSlot, uZM_MIN_LEVEL);
			ZENITH_ASSERT_LE(uLevel, uZM_MAX_LEVEL,
				"%s party slot %u is above level %u",
				ZM_GetTrainerName(x.m_eId), uSlot, uZM_MAX_LEVEL);
		}
	}
}

// NOT a unit here: "every party member knows a move at its authored level". That
// property is a THEOREM of the two units above plus checks another suite owns, so
// no edit to this table could ever red it. ZM_BuildWildEnemySpec copies species
// and level straight through (ZM_BattleDirectorCore.cpp), and it fills move slot 0
// from any learnset entry at or below the level -- while ZM_Learnsets always emits
// a level-1 STAB pick and ZM_Tests_Learnsets::Learnset_HasStabAndEarlyDamage pins
// that EVERY species has one. So for any in-range species at any level in
// [uZM_MIN_LEVEL, uZM_MAX_LEVEL] -- exactly what the two units above already
// require -- slot 0 is non-empty unconditionally. Asserting it here would add a
// second red to a ZM_Learnsets/ZM_SpeciesData defect, worded as if the trainer
// roster were at fault.

// ---- Reward + consequence columns -------------------------------------------

ZENITH_TEST(ZM_Data, Trainer_EveryPrizeIsPositiveAndUnderTheMoneyCap)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the walk below vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		ZENITH_ASSERT_GT(x.m_uPrizeMoney, 0u,
			"%s pays nothing -- beating a trainer must be worth something",
			ZM_GetTrainerName(x.m_eId));
		// The upper bound matters because ZM_GameState::AddMoney is a NO-OP while
		// over cap rather than a clamp, so an over-cap prize is a silent non-credit.
		ZENITH_ASSERT_LE(x.m_uPrizeMoney, uZM_MONEY_CAP,
			"%s pays more than the money ceiling, so the credit would be a silent no-op",
			ZM_GetTrainerName(x.m_eId));
	}
}

ZENITH_TEST(ZM_Data, Trainer_EveryDefeatFlagIsRegisteredOrNone)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the walk below vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		// Cross-table integrity: an unregistered flag would make ZM_SetStoryFlag
		// return false and the defeat would write nothing at all, silently.
		ZENITH_ASSERT_TRUE(x.m_eDefeatFlag == ZM_STORY_FLAG_NONE
			|| (u_int)x.m_eDefeatFlag < (u_int)ZM_STORY_FLAG_COUNT,
			"%s names story flag %u, which the registry does not know",
			ZM_GetTrainerName(x.m_eId), (u_int)x.m_eDefeatFlag);
	}
}

ZENITH_TEST(ZM_Data, Trainer_DefeatFlagsCoverBothTheFlaggedAndUnflaggedShape)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the counts below vacuous");

	u_int uFlagged = 0u;
	u_int uUnflagged = 0u;
	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		if (Trainer(i).m_eDefeatFlag == ZM_STORY_FLAG_NONE) { ++uUnflagged; }
		else                                                { ++uFlagged; }
	}

	ZENITH_ASSERT_GT(uFlagged, 0u,
		"no trainer writes a story flag -- the column is dead content");
	ZENITH_ASSERT_GT(uUnflagged, 0u,
		"every trainer writes a story flag -- the NONE arm SC5 must handle is "
		"unexercised by any row");

	ZENITH_ASSERT_EQ((u_int)ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER).m_eDefeatFlag,
		(u_int)ZM_STORY_FLAG_RIVAL1_DEFEATED,
		"the rival's defeat must be the flag SC5/SC6 gate on");
}

ZENITH_TEST(ZM_Data, Trainer_EveryAITierIsARealTier)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the walk below vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		const ZM_TrainerData& x = Trainer(i);
		// The tier is passed straight into ZM_BattleDirectorCore::Begin's 7th
		// argument, so this is the only place a bad tier is caught before it
		// reaches ZM_ChooseAction.
		ZENITH_ASSERT_LT((u_int)x.m_eAITier, (u_int)ZM_AI_TIER_COUNT,
			"%s carries AI tier %u -- ZM_AI_TIER_NONE and beyond name no chooser, so "
			"the opponent could not act",
			ZM_GetTrainerName(x.m_eId), (u_int)x.m_eAITier);
	}
}

// ---- The two authored rows, column by column --------------------------------

ZENITH_TEST(ZM_Data, Vesper_AuthoredValuesAreExact)
{
	const ZM_TrainerData& x = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER);

	ZENITH_ASSERT_EQ((u_int)x.m_eId, (u_int)ZM_TRAINER_RIVAL_VESPER,
		"the rival row does not identify as the rival");
	ZENITH_ASSERT_STREQ(x.m_szDisplayName, "Vesper", "the rival's name changed");
	ZENITH_ASSERT_NOT_NULL(x.m_paxParty, "the rival brings no party array");
	ZENITH_ASSERT_EQ(x.m_uPartyCount, uEXPECTED_VESPER_PARTY,
		"the rival's party size changed");

	if (PartyIsWalkable(x) && x.m_uPartyCount >= uEXPECTED_VESPER_PARTY)
	{
		ZENITH_ASSERT_EQ((u_int)x.m_paxParty[0].m_eSpecies, (u_int)ZM_SPECIES_KINDLET,
			"the rival must bring the Fire counterpart to the player's FERNFAWN starter");
		ZENITH_ASSERT_EQ(x.m_paxParty[0].m_uLevel, uEXPECTED_VESPER_LEVEL,
			"the rival's monster must match the starter's level");
	}

	ZENITH_ASSERT_EQ(x.m_uPrizeMoney, uEXPECTED_VESPER_PRIZE, "the rival's prize changed");
	ZENITH_ASSERT_EQ((u_int)x.m_eDefeatFlag, (u_int)ZM_STORY_FLAG_RIVAL1_DEFEATED,
		"the rival's defeat must write RIVAL1_DEFEATED");
	ZENITH_ASSERT_EQ((u_int)x.m_eAITier, (u_int)ZM_AI_TIER_GREEDY,
		"Q-F pins the early rival at GREEDY");
	ZENITH_ASSERT_STREQ(ZM_GetTrainerName(ZM_TRAINER_RIVAL_VESPER), "Vesper",
		"the name accessor and the rival row disagree");
}

ZENITH_TEST(ZM_Data, Rambler_AuthoredValuesAreExact)
{
	const ZM_TrainerData& x = ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER);
	const ZM_TrainerData& xVesper = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER);

	ZENITH_ASSERT_EQ((u_int)x.m_eId, (u_int)ZM_TRAINER_ROUTE1_RAMBLER,
		"the generic row does not identify as the rambler");
	ZENITH_ASSERT_STREQ(x.m_szDisplayName, "Rambler Perrin", "the rambler's name changed");
	ZENITH_ASSERT_EQ(x.m_uPartyCount, uEXPECTED_RAMBLER_PARTY,
		"the rambler's party size changed");

	if (PartyIsWalkable(x) && x.m_uPartyCount >= uEXPECTED_RAMBLER_PARTY)
	{
		ZENITH_ASSERT_EQ((u_int)x.m_paxParty[0].m_eSpecies, (u_int)ZM_SPECIES_NIBBIN,
			"the rambler's lead is not the authored Route 1 species");
		ZENITH_ASSERT_EQ(x.m_paxParty[0].m_uLevel, uEXPECTED_RAMBLER_LEVEL,
			"the rambler's lead level changed");
		ZENITH_ASSERT_EQ((u_int)x.m_paxParty[1].m_eSpecies, (u_int)ZM_SPECIES_PIPWIT,
			"the rambler's second is not the authored Route 1 species");
		ZENITH_ASSERT_EQ(x.m_paxParty[1].m_uLevel, uEXPECTED_RAMBLER_LEVEL,
			"the rambler's second level changed");
	}

	ZENITH_ASSERT_EQ(x.m_uPrizeMoney, uEXPECTED_RAMBLER_PRIZE, "the rambler's prize changed");
	ZENITH_ASSERT_EQ((u_int)x.m_eDefeatFlag, (u_int)ZM_STORY_FLAG_NONE,
		"a generic route trainer must write no story flag");
	ZENITH_ASSERT_EQ((u_int)x.m_eAITier, (u_int)ZM_AI_TIER_RANDOM,
		"the rambler's tier changed");

	// The cross-row difference claims that make the second row worth having.
	ZENITH_ASSERT_NE(x.m_uPartyCount, xVesper.m_uPartyCount,
		"both rows carry the same party size -- the count column is never exercised "
		"as a variable");
	ZENITH_ASSERT_NE((u_int)x.m_eAITier, (u_int)xVesper.m_eAITier,
		"both rows carry the same AI tier -- the tier column is never exercised as a "
		"variable");
}

// ---- Accessor totality ------------------------------------------------------

ZENITH_TEST(ZM_Data, Accessor_GetTrainerDataIsTotalForSentinelAndGarbage)
{
	// The reachability of this whole unit depends on the accessor NOT asserting --
	// a Zenith_Assert there would break the process in every configuration and end
	// the boot unit run.
	const ZM_TrainerData* pxFirst = nullptr;

	for (u_int u = 0u; u < uUNREGISTERED_COUNT; ++u)
	{
		const ZM_TrainerData& x = ZM_GetTrainerData(aeUNREGISTERED[u]);

		ZENITH_ASSERT_EQ((u_int)x.m_eId, (u_int)ZM_TRAINER_NONE,
			"unregistered id %u did not yield the UNKNOWN row's id", (u_int)aeUNREGISTERED[u]);
		ZENITH_ASSERT_STREQ(x.m_szDisplayName, "UNKNOWN",
			"unregistered id %u did not yield the UNKNOWN row", (u_int)aeUNREGISTERED[u]);
		ZENITH_ASSERT_NULL(x.m_paxParty, "the UNKNOWN row must carry no party array");
		ZENITH_ASSERT_EQ(x.m_uPartyCount, 0u, "the UNKNOWN row must carry no party");
		ZENITH_ASSERT_EQ(x.m_uPrizeMoney, 0u, "the UNKNOWN row must pay nothing");
		ZENITH_ASSERT_EQ((u_int)x.m_eDefeatFlag, (u_int)ZM_STORY_FLAG_NONE,
			"the UNKNOWN row must write no story flag");
		ZENITH_ASSERT_EQ((u_int)x.m_eAITier, (u_int)ZM_AI_TIER_NONE,
			"the UNKNOWN row must name no chooser");

		if (pxFirst == nullptr) { pxFirst = &x; }
		ZENITH_ASSERT_EQ(&x, pxFirst,
			"every unregistered id must share ONE fallback row, never a copy");

		for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
		{
			ZENITH_ASSERT_NE(&x, &Trainer(i),
				"unregistered id %u resolved to registered row %u",
				(u_int)aeUNREGISTERED[u], i);
		}
	}
}

ZENITH_TEST(ZM_Data, Accessor_GetTrainerNameIsTotalAndNeverNull)
{
	ZENITH_ASSERT_STREQ(ZM_GetTrainerName(ZM_TRAINER_NONE), "NONE",
		"the sentinel is a legal value to name, not garbage");
	ZENITH_ASSERT_STREQ(
		ZM_GetTrainerName((ZM_TRAINER_ID)((u_int)ZM_TRAINER_COUNT + 9u)), "UNKNOWN",
		"an id just past the roster must name UNKNOWN");
	ZENITH_ASSERT_STREQ(ZM_GetTrainerName((ZM_TRAINER_ID)9999u), "UNKNOWN",
		"a garbage id must name UNKNOWN");
	ZENITH_ASSERT_STREQ(ZM_GetTrainerName((ZM_TRAINER_ID)~0u), "UNKNOWN",
		"the largest garbage id must name UNKNOWN");

	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the walk below vacuous");
	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		// Never-null matters because every caller is a log/printf format argument.
		ZENITH_ASSERT_NOT_NULL(ZM_GetTrainerName((ZM_TRAINER_ID)i),
			"the name accessor returned null for trainer %u", i);
		ZENITH_ASSERT_STREQ(ZM_GetTrainerName((ZM_TRAINER_ID)i), Trainer(i).m_szDisplayName,
			"the name accessor and the row disagree for trainer %u", i);
	}
}

ZENITH_TEST(ZM_Data, Accessor_IsRegisteredTrainerRejectsSentinelAndGarbage)
{
	ZENITH_ASSERT_GT((u_int)ZM_TRAINER_COUNT, 0u,
		"an empty roster makes the walk below vacuous");

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_COUNT; ++i)
	{
		ZENITH_ASSERT_TRUE(ZM_IsRegisteredTrainer((ZM_TRAINER_ID)i),
			"trainer %u is in the table but does not register", i);
	}

	// The sentinel case is the load-bearing one: ZM_TRAINER_NONE numerically EQUALS
	// ZM_TRAINER_COUNT, so a predicate written with <= would classify it as a real
	// row and index one past the end of the table.
	ZENITH_ASSERT_FALSE(ZM_IsRegisteredTrainer(ZM_TRAINER_NONE),
		"the NONE sentinel aliases COUNT and must never register");
	ZENITH_ASSERT_FALSE(
		ZM_IsRegisteredTrainer((ZM_TRAINER_ID)((u_int)ZM_TRAINER_COUNT + 7u)),
		"an id just past the roster must not register");
	ZENITH_ASSERT_FALSE(ZM_IsRegisteredTrainer((ZM_TRAINER_ID)9999u),
		"a garbage id must not register");
	ZENITH_ASSERT_FALSE(ZM_IsRegisteredTrainer((ZM_TRAINER_ID)~0u),
		"the largest garbage id must not register");
}

// THE totality proof. Written in the local-hit-count form for two mandatory
// reasons: (1) NO ZENITH_ASSERT_* may appear INSIDE the scope -- while a capture
// scope is active Zenith_TestRunner::HandleFailure swallows framework failures
// and merely bumps the hit count, so an in-scope assertion could never red this
// test; (2) the count MUST be copied to a local before the closing brace, because
// ~Zenith_AssertCaptureScope restores the previous hit count. Scopes do not nest.
ZENITH_TEST(ZM_Data, Accessor_NoAccessorAssertsOnAnyGarbageId)
{
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < uUNREGISTERED_COUNT; ++u)
		{
			const ZM_TrainerData& x = ZM_GetTrainerData(aeUNREGISTERED[u]);
			const char* szName = ZM_GetTrainerName(aeUNREGISTERED[u]);
			const bool bRegistered = ZM_IsRegisteredTrainer(aeUNREGISTERED[u]);
			(void)x;
			(void)szName;
			(void)bRegistered;
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"an accessor asserted on a garbage id -- Zenith_Assert breaks in EVERY config, "
		"so this would kill the whole boot unit run rather than fail one test");

	// ...and the answers are still the defined ones outside the scope.
	ZENITH_ASSERT_STREQ(ZM_GetTrainerName((ZM_TRAINER_ID)9999u), "UNKNOWN",
		"the name accessor stopped answering UNKNOWN for garbage");
	ZENITH_ASSERT_STREQ(ZM_GetTrainerData((ZM_TRAINER_ID)9999u).m_szDisplayName, "UNKNOWN",
		"the row accessor stopped answering the UNKNOWN row for garbage");
}
