#include "Zenith.h"

// ============================================================================
// ZM_Tests_CareCenter -- S6 item 2 SC8 unit tests for the PURE Care Center
// surface: the prompt lines, the "does this party need it at all" predicate, and
// the heal applied to a ZM_GameState. All hermetic -- no ECS, no scene, no
// graphics, no baked assets, no RNG -- so no RequestSkip is needed.
//
// SC9 adds the two units behind the "the heal is not SILENT" follow-up: the
// confirmation line is queueable and distinct, and ZM_ApplyCareCenterHeal's return
// really distinguishes "actually healed" from "already healthy" -- which is the one
// bit ZM_UI_MenuStack::ApplyDialogueChoice branches on when it decides whether to
// show that line instead of popping.
//
// Every fixture is built with the REAL ZM_BuildMonsterRecord and mutated through
// the REAL fields (m_uCurrentHp / m_eStatus / m_axMoves[].m_uCurPP), never a
// hand-rolled stand-in: the predicate's whole job is to read those three, and a
// fake record would let a field rename pass green. Category ZM_CareCenter.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"        // the TryOpenCareCenterPrompt seam
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"      // ZM_MAJOR_STATUS_* / uZM_MAX_MOVES
#include "Zenithmon/Source/CareCenter/ZM_CareCenter.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"           // ZM_MOVE_NONE (the empty-slot sentinel)
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"        // ZM_SPECIES_FERNFAWN / ZM_SPECIES_KINDLET
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_Party.h"

#include <cstring>

namespace
{
	constexpr u_int uFIXTURE_LEVEL = 5u;

	// A two-member, FULL-HEALTH party (the state a Care Center leaves behind).
	ZM_Party ZM_MakeHealthyFixtureParty()
	{
		ZM_Party xParty;
		xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, uFIXTURE_LEVEL));
		xParty.Add(ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, uFIXTURE_LEVEL));
		return xParty;
	}

	// The first move slot that actually holds a move (uZM_MAX_MOVES when none does).
	u_int ZM_FirstRealMoveSlot(const ZM_Monster& xMember)
	{
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			if (xMember.m_axMoves[u].m_eMove != ZM_MOVE_NONE && xMember.m_axMoves[u].m_uMaxPP > 0u)
			{
				return u;
			}
		}
		return uZM_MAX_MOVES;
	}
}

// ---- The prompt lines -------------------------------------------------------

ZENITH_TEST(ZM_CareCenter, PromptLines_AreNonEmptyAndDistinct)
{
	// Every one of them is written into a UI element; an empty string would draw a blank
	// question or an unreadable button (and ArmChoice would reject the pair outright).
	const char* szPrompt = ZM_CareCenterPromptLine();
	const char* szYes    = ZM_CareCenterYesLabel();
	const char* szNo     = ZM_CareCenterNoLabel();
	const char* szHealed = ZM_CareCenterHealedLine();

	ZENITH_ASSERT_TRUE(szPrompt != nullptr && szPrompt[0] != '\0', "the prompt line is non-empty");
	ZENITH_ASSERT_TRUE(szYes != nullptr && szYes[0] != '\0', "the YES label is non-empty");
	ZENITH_ASSERT_TRUE(szNo != nullptr && szNo[0] != '\0', "the NO label is non-empty");
	ZENITH_ASSERT_TRUE(szHealed != nullptr && szHealed[0] != '\0', "the healed line is non-empty");
	ZENITH_ASSERT_TRUE(std::strcmp(szYes, szNo) != 0,
		"the two answer labels differ -- identical buttons would be unanswerable");
	ZENITH_ASSERT_TRUE(std::strcmp(szPrompt, szHealed) != 0,
		"the question and the confirmation are different lines");
}

// ---- The healed CONFIRMATION line (SC9: the heal is not silent) -------------
//
// ZM_UI_MenuStack::ApplyDialogueChoice queues ZM_CareCenterHealedLine() onto the box
// -- instead of popping -- when, and only when, ZM_ApplyCareCenterHeal reports that
// something actually needed healing. That wiring is proved end to end by the windowed
// ZM_CareCenterHeal_Test; what CAN be pinned purely is the two things it depends on:
// the line is queueable at all, and the return value really distinguishes the two
// cases. Both live here because both are hermetic.

ZENITH_TEST(ZM_CareCenter, HealedLine_IsQueueableAndDistinctFromThePrompt)
{
	const char* szHealed = ZM_CareCenterHealedLine();
	const char* szPrompt = ZM_CareCenterPromptLine();

	// ZM_UI_DialogueBox::QueueLine REJECTS a null or empty line, so an empty confirmation
	// would silently fail to queue and the heal would go straight back to being silent.
	ZENITH_ASSERT_TRUE(szHealed != nullptr, "the healed line is a real string");
	ZENITH_ASSERT_TRUE(szHealed[0] != '\0',
		"the healed line is non-empty -- QueueLine rejects an empty line, which would put the "
		"heal straight back to closing on a silent button");
	// It replaces the question in the SAME text element, so an identical string would be
	// indistinguishable on screen from the prompt that was just answered.
	ZENITH_ASSERT_TRUE(std::strcmp(szHealed, szPrompt) != 0,
		"the confirmation is not the question repeated back");
	// String LITERALS: the host holds the pointer only long enough to copy it into the
	// queue, but the contract is process-lifetime, so a second call must return the same
	// storage rather than a rebuilt buffer.
	ZENITH_ASSERT_TRUE(ZM_CareCenterHealedLine() == szHealed,
		"the healed line is a stable literal, not a per-call temporary");
}

ZENITH_TEST(ZM_CareCenter, ApplyHeal_ReturnGatesTheConfirmationLine)
{
	// The BRANCH the host reads: a true return is what makes it queue the confirmation and
	// hold the box open; a false return makes it pop immediately. Pinning the two returns
	// against the SAME fixture (damaged vs already-healthy) is what stops the host's "did
	// this heal do anything?" question from quietly becoming unanswerable.
	ZM_GameState xState;
	xState.m_xParty = ZM_MakeHealthyFixtureParty();
	ZENITH_ASSERT_FALSE(ZM_ApplyCareCenterHeal(xState),
		"a healthy party reports FALSE -- the host pops without a confirmation line");

	ZM_Monster& xLead = xState.m_xParty.Get(0u);
	ZENITH_ASSERT_TRUE(xLead.GetMaxHP() > 1u, "the fixture has room to be damaged");
	xLead.m_uCurrentHp = xLead.GetMaxHP() - 1u;
	ZENITH_ASSERT_TRUE(ZM_ApplyCareCenterHeal(xState),
		"the SAME party, one HP down, reports TRUE -- the host shows the confirmation line");

	// ...and the heal is idempotent, so a second YES on an already-serviced party falls
	// back to the silent pop rather than showing a confirmation for nothing.
	ZENITH_ASSERT_FALSE(ZM_ApplyCareCenterHeal(xState),
		"healing an already-healed party reports FALSE again");
}

// ---- ZM_PartyNeedsHealing ---------------------------------------------------

ZENITH_TEST(ZM_CareCenter, NeedsHealing_FalseForAFreshFullHealthParty)
{
	const ZM_Party xParty = ZM_MakeHealthyFixtureParty();
	ZENITH_ASSERT_EQ(xParty.Count(), 2u, "the fixture party holds two members");
	ZENITH_ASSERT_FALSE(ZM_PartyNeedsHealing(xParty),
		"a freshly built party is at full HP, unstatused and at full PP -- nothing to heal");
}

ZENITH_TEST(ZM_CareCenter, NeedsHealing_FalseForAnEmptyParty)
{
	// The predicate must never index Get() (which ASSERTS on an out-of-range slot), so
	// an empty party has to fall through the count guard rather than probe slot 0.
	const ZM_Party xEmpty;
	ZENITH_ASSERT_EQ(xEmpty.Count(), 0u, "the fixture party is empty");
	ZENITH_ASSERT_FALSE(ZM_PartyNeedsHealing(xEmpty), "an empty party needs no healing");
}

ZENITH_TEST(ZM_CareCenter, NeedsHealing_TrueForDamagedHP)
{
	ZM_Party xParty = ZM_MakeHealthyFixtureParty();
	// The SECOND member, so the walk is proved to cover the whole party rather than
	// just the lead.
	ZM_Monster& xMember = xParty.Get(1u);
	ZENITH_ASSERT_TRUE(xMember.GetMaxHP() > 1u, "the fixture has room to be damaged");
	xMember.m_uCurrentHp = xMember.GetMaxHP() - 1u;
	ZENITH_ASSERT_TRUE(ZM_PartyNeedsHealing(xParty), "one HP below max needs healing");
}

ZENITH_TEST(ZM_CareCenter, NeedsHealing_TrueForAMajorStatus)
{
	ZM_Party xParty = ZM_MakeHealthyFixtureParty();
	xParty.Get(0u).m_eStatus = ZM_MAJOR_STATUS_POISON;
	ZENITH_ASSERT_EQ(xParty.Get(0u).m_uCurrentHp, xParty.Get(0u).GetMaxHP(),
		"HP is untouched, so the status is the ONLY reason this party needs healing");
	ZENITH_ASSERT_TRUE(ZM_PartyNeedsHealing(xParty), "a major status needs healing");
}

ZENITH_TEST(ZM_CareCenter, NeedsHealing_TrueForASpentPPSlot)
{
	ZM_Party xParty = ZM_MakeHealthyFixtureParty();
	ZM_Monster& xMember = xParty.Get(0u);
	const u_int uSlot = ZM_FirstRealMoveSlot(xMember);
	ZENITH_ASSERT_TRUE(uSlot < uZM_MAX_MOVES, "the fixture knows at least one move with PP");
	xMember.m_axMoves[uSlot].m_uCurPP = xMember.m_axMoves[uSlot].m_uMaxPP - 1u;
	ZENITH_ASSERT_EQ(xMember.m_uCurrentHp, xMember.GetMaxHP(), "HP is untouched");
	ZENITH_ASSERT_EQ((u_int)xMember.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE, "status is untouched");
	ZENITH_ASSERT_TRUE(ZM_PartyNeedsHealing(xParty),
		"one point of spent PP is enough -- PP is part of what a Care Center restores");
}

// ---- ZM_ApplyCareCenterHeal -------------------------------------------------

ZENITH_TEST(ZM_CareCenter, ApplyHeal_RestoresHPStatusAndPPForEveryMember)
{
	ZM_GameState xState;
	xState.m_xParty = ZM_MakeHealthyFixtureParty();
	// Damage BOTH members, each in a different way, so a per-member loop that healed only
	// the lead (or only the HP) is caught.
	ZM_Monster& xLead = xState.m_xParty.Get(0u);
	ZM_Monster& xBench = xState.m_xParty.Get(1u);
	xLead.m_uCurrentHp = 0u;                       // fainted
	xLead.m_eStatus = ZM_MAJOR_STATUS_POISON;
	const u_int uBenchSlot = ZM_FirstRealMoveSlot(xBench);
	ZENITH_ASSERT_TRUE(uBenchSlot < uZM_MAX_MOVES, "the bench member knows a move with PP");
	xBench.m_axMoves[uBenchSlot].m_uCurPP = 0u;
	xBench.m_uCurrentHp = 1u;

	ZENITH_ASSERT_TRUE(ZM_ApplyCareCenterHeal(xState),
		"a damaged party reports that the heal was actually needed");

	for (u_int u = 0u; u < xState.m_xParty.Count(); ++u)
	{
		const ZM_Monster& xMember = xState.m_xParty.Get(u);
		ZENITH_ASSERT_EQ(xMember.m_uCurrentHp, xMember.GetMaxHP(), "member %u is at full HP", u);
		ZENITH_ASSERT_EQ((u_int)xMember.m_eStatus, (u_int)ZM_MAJOR_STATUS_NONE,
			"member %u has no major status", u);
		for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
		{
			ZENITH_ASSERT_EQ(xMember.m_axMoves[uMove].m_uCurPP, xMember.m_axMoves[uMove].m_uMaxPP,
				"member %u move %u is at full PP", u, uMove);
		}
	}
	ZENITH_ASSERT_FALSE(ZM_PartyNeedsHealing(xState.m_xParty),
		"the healed party no longer needs healing");
}

ZENITH_TEST(ZM_CareCenter, ApplyHeal_ReportsFalseAndChangesNothingOnAHealthyParty)
{
	ZM_GameState xState;
	xState.m_xParty = ZM_MakeHealthyFixtureParty();
	const ZM_Monster xLeadBefore = xState.m_xParty.Get(0u);

	ZENITH_ASSERT_FALSE(ZM_ApplyCareCenterHeal(xState),
		"an already-healthy party reports that nothing needed healing");

	const ZM_Monster& xLeadAfter = xState.m_xParty.Get(0u);
	ZENITH_ASSERT_EQ(xLeadAfter.m_uCurrentHp, xLeadBefore.m_uCurrentHp, "curHP is unchanged");
	ZENITH_ASSERT_EQ((u_int)xLeadAfter.m_eStatus, (u_int)xLeadBefore.m_eStatus, "status is unchanged");
	ZENITH_ASSERT_EQ((u_int)xLeadAfter.m_eSpecies, (u_int)xLeadBefore.m_eSpecies,
		"the heal never touches identity");
	ZENITH_ASSERT_EQ(xLeadAfter.m_uLevel, xLeadBefore.m_uLevel, "the heal never touches level");
	for (u_int uMove = 0u; uMove < uZM_MAX_MOVES; ++uMove)
	{
		ZENITH_ASSERT_EQ(xLeadAfter.m_axMoves[uMove].m_uCurPP, xLeadBefore.m_axMoves[uMove].m_uCurPP,
			"move %u PP is unchanged", uMove);
	}
}

ZENITH_TEST(ZM_CareCenter, ApplyHeal_OnAnEmptyPartyIsASafeNoOp)
{
	ZM_GameState xState;   // default-constructed: an EMPTY party
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u, "the default game state has no party members");
	ZENITH_ASSERT_FALSE(ZM_ApplyCareCenterHeal(xState),
		"healing an empty party changes nothing and reports it was not needed");
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 0u, "...and never grew the party");
}

// ---- The raise seam (headless half) ----------------------------------------

ZENITH_TEST(ZM_CareCenter, TryOpenCareCenterPrompt_WithoutASingletonIsRejected)
{
	// Unit tests run at boot BEFORE any scene loads, so no ZM_MenuRoot singleton has
	// claimed itself yet -- the static seam must report failure instead of resolving a
	// stale entity id (a Care Center attendant with no menu root gets a clean `false`).
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::TryOpenCareCenterPrompt(),
		"TryOpenCareCenterPrompt reports failure when there is no live ZM_UI_MenuStack singleton");
}
