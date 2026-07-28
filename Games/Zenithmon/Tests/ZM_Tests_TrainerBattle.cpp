#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerBattle -- S7 item 3 SC5 unit gate for the PURE half of a forced
// trainer battle (category ZM_TrainerBattle): ZM_BuildTrainerBattleSetup and
// ZM_DeriveTrainerBattleSeed.
//
// Everything here is hermetic: a compiled const roster row plus pure free
// functions. No ECS, no scene, no physics, no UI, no disk, no baked assets -- so
// no RequestSkip is needed and the whole enemy party, the AI tier and the battle
// seed are proven WITHOUT starting a battle or loading a scene.
//
// Every "walk the roster" case GUARDS its walk with a non-zero count first: a
// loop bounded by a count that is itself zero passes vacuously and would keep
// passing after the content it exists to police was deleted.
//
//   1. Setup_PartyMatchesTheAuthoredRowExactly -- the derived party IS the row.
//   2. Setup_TierIsTheRowsTier -- the row's ZM_AI_TIER reaches the setup.
//   3. Setup_UnregisteredTrainerIsAnInertTotalNoOp -- total, silent, assert-free.
//   4. Seed_IsDeterministicPerTrainerAndDisjointFromTheWildSeedSpace -- the fold.
//   5. Setup_FieldsEveryAuthoredMemberWithinTheEngineCap -- the retired clamp.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"                    // the totality proof
#include "Zenithmon/Components/ZM_BattleDirector.h"            // DeriveBattleSeed (the WILD fold, for disjointness)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"     // ZM_BuildWildEnemySpec (the derivation being reused)
#include "Zenithmon/Source/Battle/ZM_TrainerBattle.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"

namespace
{
	// The ids no roster may ever name: the sentinel, COUNT itself, and two pieces
	// of outright garbage. Mirrors ZM_Tests_TrainerData's battery.
	const ZM_TRAINER_ID aeTB_UNREGISTERED[] =
	{
		ZM_TRAINER_NONE,
		(ZM_TRAINER_ID)ZM_TRAINER_COUNT,
		(ZM_TRAINER_ID)999u,
		(ZM_TRAINER_ID)0xFFFFFFFFu
	};
	constexpr u_int uTB_UNREGISTERED_COUNT =
		(u_int)(sizeof(aeTB_UNREGISTERED) / sizeof(aeTB_UNREGISTERED[0]));

	// Field-for-field equality of two ZM_BattleMonsterSpec values. Spelled out
	// rather than memcmp'd: a padding byte would make memcmp flaky, and a named
	// field failure localises the drift.
	void AssertSpecEq(const ZM_BattleMonsterSpec& xExpected,
		const ZM_BattleMonsterSpec& xActual, const char* szLabel, u_int uMember)
	{
		ZENITH_ASSERT_EQ((u_int)xActual.m_eSpecies, (u_int)xExpected.m_eSpecies,
			"%s member %u species", szLabel, uMember);
		ZENITH_ASSERT_EQ(xActual.m_uLevel, xExpected.m_uLevel,
			"%s member %u level", szLabel, uMember);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eNature, (u_int)xExpected.m_eNature,
			"%s member %u nature", szLabel, uMember);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eAbility, (u_int)xExpected.m_eAbility,
			"%s member %u ability", szLabel, uMember);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eGender, (u_int)xExpected.m_eGender,
			"%s member %u gender", szLabel, uMember);
		ZENITH_ASSERT_EQ(xActual.m_uCurExp, xExpected.m_uCurExp,
			"%s member %u cumulative exp", szLabel, uMember);
		ZENITH_ASSERT_EQ(xActual.m_uCurHP, xExpected.m_uCurHP,
			"%s member %u curHP request", szLabel, uMember);
		ZENITH_ASSERT_EQ(xActual.m_bOverrideBaseStats ? 1u : 0u,
			xExpected.m_bOverrideBaseStats ? 1u : 0u,
			"%s member %u base-stat override flag", szLabel, uMember);
		for (u_int i = 0u; i < (u_int)ZM_STAT_COUNT; ++i)
		{
			ZENITH_ASSERT_EQ(xActual.m_auIV[i], xExpected.m_auIV[i],
				"%s member %u IV %u", szLabel, uMember, i);
			ZENITH_ASSERT_EQ(xActual.m_auEV[i], xExpected.m_auEV[i],
				"%s member %u EV %u", szLabel, uMember, i);
		}
		for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xActual.m_aeMoves[i], (u_int)xExpected.m_aeMoves[i],
				"%s member %u move %u", szLabel, uMember, i);
		}
	}
}

// ############################################################################
// 1. The built party IS the authored row, member for member
// ############################################################################

// The whole point of SC5's pure builder: a compiled {species, level} pair becomes
// a complete, reproducible ZM_BattleMonsterSpec through the SHIPPED
// ZM_BuildWildEnemySpec -- no second derivation, no reordering, no truncation.
ZENITH_TEST(ZM_TrainerBattle, Setup_PartyMatchesTheAuthoredRowExactly)
{
	// ANTI-VACUITY: the Rambler is the shipped multi-member row. If content ever
	// collapses back to one-monster trainers this proof no longer exercises forced
	// replacement and must fail loudly.
	ZENITH_ASSERT_GE(ZM_GetTrainerCount(), 2u,
		"the roster must carry at least two rows or this walk is near-vacuous");
	ZENITH_ASSERT_GT(ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER).m_uPartyCount, 1u,
		"the Rambler no longer supplies the multi-member forced-replacement fixture");
	ZENITH_ASSERT_EQ(uZM_TRAINER_BATTLEABLE_PARTY, uZM_TRAINER_MAX_PARTY,
		"the trainer builder still carries a hidden clamp below the engine party cap");
	ZENITH_ASSERT_EQ(uZM_TRAINER_BATTLEABLE_PARTY, uZM_MAX_PARTY_SIZE,
		"trainer and battle-engine party capacities drifted apart");

	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TRAINER_ID eId = (ZM_TRAINER_ID)u;
		const ZM_TrainerData& xRow = ZM_GetTrainerData(eId);
		const ZM_TrainerBattleSetup xSetup = ZM_BuildTrainerBattleSetup(eId);

		ZENITH_ASSERT_TRUE(xSetup.m_bValid,
			"trainer %u is registered but built no battle setup", u);
		ZENITH_ASSERT_EQ(xSetup.m_uEnemyCount, xRow.m_uPartyCount,
			"trainer %u's enemy count is not the complete authored party count", u);
		ZENITH_ASSERT_LE(xSetup.m_uEnemyCount, uZM_TRAINER_BATTLEABLE_PARTY,
			"trainer %u's authored party exceeds the shared engine capacity", u);
		ZENITH_ASSERT_GE(xSetup.m_uEnemyCount, 1u,
			"trainer %u built an EMPTY enemy party -- ZM_BattleEngine::Begin asserts on that", u);

		// The lead is member 0 and drives the enemy model placement.
		if (xRow.m_paxParty != nullptr && xRow.m_uPartyCount > 0u)
		{
			ZENITH_ASSERT_EQ((u_int)xSetup.m_eLeadSpecies, (u_int)xRow.m_paxParty[0].m_eSpecies,
				"trainer %u's lead species is not the row's first member", u);
		}

		for (u_int i = 0u; i < xSetup.m_uEnemyCount; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xSetup.m_axEnemyParty[i].m_eSpecies,
				(u_int)xRow.m_paxParty[i].m_eSpecies,
				"trainer %u member %u species drifted from the row", u, i);
			ZENITH_ASSERT_EQ(xSetup.m_axEnemyParty[i].m_uLevel, xRow.m_paxParty[i].m_uLevel,
				"trainer %u member %u level drifted from the row", u, i);

			// ...and NOTHING else was derived: the spec must equal the shipped
			// ZM_BuildWildEnemySpec of exactly this row's pair, field for field.
			AssertSpecEq(
				ZM_BuildWildEnemySpec(xRow.m_paxParty[i].m_eSpecies, xRow.m_paxParty[i].m_uLevel),
				xSetup.m_axEnemyParty[i], "trainer party", i);
		}
	}
}

// ############################################################################
// 2. The AI tier is the ROW's tier, not a hard-coded constant
// ############################################################################

// The wild arm passes a hard-coded ZM_AI_TIER_GREEDY. The trainer arm must pass
// the roster row's tier through to ZM_BattleDirectorCore::Begin's 7th argument;
// this pins the value where it is first computed.
ZENITH_TEST(ZM_TrainerBattle, Setup_TierIsTheRowsTier)
{
	ZENITH_ASSERT_GT(ZM_GetTrainerCount(), 0u, "an empty roster makes this walk vacuous");

	bool bSeenAny = false;
	ZM_AI_TIER eFirstTier = ZM_AI_TIER_NONE;
	bool bSeenTwoDistinct = false;

	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TRAINER_ID eId = (ZM_TRAINER_ID)u;
		const ZM_AI_TIER eSetupTier = ZM_BuildTrainerBattleSetup(eId).m_eEnemyTier;

		ZENITH_ASSERT_EQ((u_int)eSetupTier, (u_int)ZM_GetTrainerData(eId).m_eAITier,
			"trainer %u's setup tier is not the row's authored tier", u);
		ZENITH_ASSERT_TRUE((u_int)eSetupTier < (u_int)ZM_AI_TIER_COUNT,
			"trainer %u begins a battle with the ZM_AI_TIER_NONE sentinel -- an opponent "
			"with no action policy", u);

		if (!bSeenAny) { eFirstTier = eSetupTier; bSeenAny = true; }
		else if (eSetupTier != eFirstTier) { bSeenTwoDistinct = true; }
	}

	// ANTI-VACUITY: with one tier across the whole roster a constant-returning
	// implementation would pass every assert above.
	ZENITH_ASSERT_TRUE(bSeenTwoDistinct,
		"every roster row carries the SAME AI tier -- a constant-returning builder "
		"could not be distinguished from a row read");
}

// ############################################################################
// 3. An unregistered id is an inert, TOTAL, SILENT no-op
// ############################################################################

// Written in the local-hit-count form for two mandatory reasons: (1) NO
// ZENITH_ASSERT_* may appear INSIDE the capture scope -- while one is active
// Zenith_TestRunner::HandleFailure swallows framework failures and merely bumps
// the hit count, so an in-scope assertion could never red this test; (2) the
// count MUST be copied to a local before the closing brace, because
// ~Zenith_AssertCaptureScope restores the previous hit count. Scopes do not nest.
ZENITH_TEST(ZM_TrainerBattle, Setup_UnregisteredTrainerIsAnInertTotalNoOp)
{
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < uTB_UNREGISTERED_COUNT; ++u)
		{
			const ZM_TrainerBattleSetup xSetup = ZM_BuildTrainerBattleSetup(aeTB_UNREGISTERED[u]);
			const u_int64 ulSeed = ZM_DeriveTrainerBattleSeed(aeTB_UNREGISTERED[u]);
			(void)xSetup;
			(void)ulSeed;
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the trainer builder asserted on an unregistered id -- Zenith_Assert breaks in "
		"EVERY config, so this would kill the whole boot unit run rather than fail one test");

	// ...and the answers outside the scope are still the defined inert ones.
	for (u_int u = 0u; u < uTB_UNREGISTERED_COUNT; ++u)
	{
		const ZM_TrainerBattleSetup xSetup = ZM_BuildTrainerBattleSetup(aeTB_UNREGISTERED[u]);
		ZENITH_ASSERT_FALSE(xSetup.m_bValid,
			"unregistered id %u built a VALID setup", (u_int)aeTB_UNREGISTERED[u]);
		ZENITH_ASSERT_EQ(xSetup.m_uEnemyCount, 0u,
			"unregistered id %u built an enemy party", (u_int)aeTB_UNREGISTERED[u]);
		ZENITH_ASSERT_EQ((u_int)xSetup.m_eEnemyTier, (u_int)ZM_AI_TIER_NONE,
			"unregistered id %u named an AI tier", (u_int)aeTB_UNREGISTERED[u]);
		ZENITH_ASSERT_EQ((u_int)xSetup.m_eLeadSpecies, (u_int)ZM_SPECIES_NONE,
			"unregistered id %u named a lead species", (u_int)aeTB_UNREGISTERED[u]);
		ZENITH_ASSERT_EQ(xSetup.m_ulBattleSeed, (u_int64)0u,
			"unregistered id %u carried a battle seed -- the registered guard must run "
			"BEFORE anything is filled in", (u_int)aeTB_UNREGISTERED[u]);
	}
}

// ############################################################################
// 4. The seed: deterministic per trainer, and DISJOINT from the wild seed space
// ############################################################################

// A naive id-only FNV fold would be byte-identical to the wild fold of
// (species 0, level id), so a rival battle would replay a wild encounter's RNG
// stream. The trainer domain salt is what keeps the two spaces apart, and this
// case is what proves it.
ZENITH_TEST(ZM_TrainerBattle, Seed_IsDeterministicPerTrainerAndDisjointFromTheWildSeedSpace)
{
	ZENITH_ASSERT_GE(ZM_GetTrainerCount(), 2u,
		"the distinctness clause below needs at least two rows");

	// (a) DETERMINISM -- same id, same seed, and the setup carries that same seed.
	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TRAINER_ID eId = (ZM_TRAINER_ID)u;
		const u_int64 ulFirst  = ZM_DeriveTrainerBattleSeed(eId);
		const u_int64 ulSecond = ZM_DeriveTrainerBattleSeed(eId);
		ZENITH_ASSERT_EQ(ulSecond, ulFirst, "trainer %u's seed is not deterministic", u);
		ZENITH_ASSERT_NE(ulFirst, (u_int64)0u, "trainer %u folded to a zero seed", u);
		ZENITH_ASSERT_EQ(ZM_BuildTrainerBattleSetup(eId).m_ulBattleSeed, ulFirst,
			"trainer %u's setup seed is not the derived seed", u);
	}

	// (b) DISTINCTNESS -- two trainers never share a battle stream.
	ZENITH_ASSERT_NE(
		ZM_DeriveTrainerBattleSeed(ZM_TRAINER_RIVAL_VESPER),
		ZM_DeriveTrainerBattleSeed(ZM_TRAINER_ROUTE1_RAMBLER),
		"the two authored trainers fold to the SAME battle seed");

	// (c) DISJOINTNESS from the wild (species, level) space. Level 0 is INCLUDED
	// deliberately: it is the collision case a salt-free fold produces.
	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const u_int64 ulTrainer = ZM_DeriveTrainerBattleSeed((ZM_TRAINER_ID)u);
		for (u_int uSpecies = 0u; uSpecies < 4u; ++uSpecies)
		{
			for (u_int uLevel = 0u; uLevel < 9u; ++uLevel)
			{
				ZENITH_ASSERT_NE(ulTrainer,
					ZM_BattleDirector::DeriveBattleSeed((ZM_SPECIES_ID)uSpecies, uLevel),
					"trainer %u's seed collides with the WILD seed for (species %u, level %u) "
					"-- a rival battle would replay a wild encounter's RNG stream",
					u, uSpecies, uLevel);
			}
		}
	}
}

// ############################################################################
// 5. The built party fields every authored member within the shared engine cap
// ############################################################################

// This is the independent clamp-retirement pin: the builder and row cap must
// expose the engine's entire supported party, and every current row must arrive
// at Begin member-for-member rather than silently dropping its bench.
ZENITH_TEST(ZM_TrainerBattle, Setup_FieldsEveryAuthoredMemberWithinTheEngineCap)
{
	ZENITH_ASSERT_GT(ZM_GetTrainerCount(), 0u, "an empty roster makes this walk vacuous");
	ZENITH_ASSERT_LE(uZM_TRAINER_BATTLEABLE_PARTY, uZM_TRAINER_MAX_PARTY,
		"the battleable count outgrew the row cap -- m_axEnemyParty would overrun");
	ZENITH_ASSERT_GE(uZM_TRAINER_BATTLEABLE_PARTY, 1u,
		"a zero battleable count means NO trainer can ever be fielded");
	ZENITH_ASSERT_EQ(uZM_TRAINER_BATTLEABLE_PARTY, uZM_MAX_PARTY_SIZE,
		"the public trainer capacity must equal the battle engine's party capacity");

	for (u_int u = 0u; u < ZM_GetTrainerCount(); ++u)
	{
		const ZM_TRAINER_ID eId = (ZM_TRAINER_ID)u;
		const ZM_TrainerBattleSetup xSetup = ZM_BuildTrainerBattleSetup(eId);

		const ZM_TrainerData& xRow = ZM_GetTrainerData(eId);
		ZENITH_ASSERT_EQ(xSetup.m_uEnemyCount, xRow.m_uPartyCount,
			"trainer %u dropped authored bench members (%u fielded, %u authored)",
			u, xSetup.m_uEnemyCount, xRow.m_uPartyCount);
		ZENITH_ASSERT_LE(xSetup.m_uEnemyCount, uZM_TRAINER_BATTLEABLE_PARTY,
			"trainer %u fields %u monsters -- more than the shared capacity %u",
			u, xSetup.m_uEnemyCount, uZM_TRAINER_BATTLEABLE_PARTY);
		ZENITH_ASSERT_GE(xSetup.m_uEnemyCount, 1u,
			"trainer %u fields an EMPTY party -- ZM_BattleEngine::Begin asserts on that", u);

		for (u_int i = 0u; i < xRow.m_uPartyCount; ++i)
		{
			ZENITH_ASSERT_EQ((u_int)xSetup.m_axEnemyParty[i].m_eSpecies,
				(u_int)xRow.m_paxParty[i].m_eSpecies,
				"trainer %u member %u was reordered or dropped", u, i);
		}
	}
}
