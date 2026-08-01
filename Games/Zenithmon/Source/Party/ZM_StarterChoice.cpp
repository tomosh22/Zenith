#include "Zenith.h"

#include "Zenithmon/Source/Party/ZM_StarterChoice.h"

#include "Zenithmon/Source/Party/ZM_Monster.h"   // ZM_BuildMonsterRecord

// ============================================================================
// ZM_StarterChoice (S8 item 1 SC3) -- the authored starter trio and the two
// operations over it: grant the chosen one, and ask whether the player owns
// anything at all. See the header for the contract; this file is the ORDER.
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON ITS ARGUMENTS. Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration -- there is no build in which it
// degrades to a log. These functions are TOTAL by contract and the boot units
// pin that totality by feeding them the sentinel and out-of-range choices on
// purpose, so a defensive assert here does not catch a bug: it kills the process
// partway through the unit run at boot and takes the whole gate down with it.
// ============================================================================

namespace
{
	// The bound is DEDUCED, never spelled: with an explicit
	// [ZM_STARTER_CHOICE_COUNT] the static_assert below would be true by
	// construction and a forgotten row would zero-initialise the tail into a
	// nameless starter. Deduced, a missing or extra row is a COMPILE error here.
	const ZM_StarterChoiceData s_axStarterChoices[] =
	{
		// F01 Grass, beaten by F02 Fire.   F02 Fire, beaten by F03 Water.   F03 Water, beaten by F01 Grass.
		{ ZM_STARTER_CHOICE_FERNFAWN, ZM_SPECIES_FERNFAWN, ZM_STARTER_CHOICE_KINDLET  },
		{ ZM_STARTER_CHOICE_KINDLET,  ZM_SPECIES_KINDLET,  ZM_STARTER_CHOICE_FINLET   },
		{ ZM_STARTER_CHOICE_FINLET,   ZM_SPECIES_FINLET,   ZM_STARTER_CHOICE_FERNFAWN },
	};

	static_assert(sizeof(s_axStarterChoices) / sizeof(s_axStarterChoices[0]) == ZM_STARTER_CHOICE_COUNT,
		"the starter table must have exactly one row per ZM_STARTER_CHOICE");

	// Handed back for an unregistered choice. The alternative -- indexing anyway
	// after an assert -- reads off the end of the table the moment the break is
	// stepped past. Every field is the INERT answer, so a caller that ignores the
	// id check still cannot build a monster out of it.
	const ZM_StarterChoiceData s_xInvalidStarterChoice =
	{
		ZM_STARTER_CHOICE_NONE, ZM_SPECIES_NONE, ZM_STARTER_CHOICE_NONE
	};
}

bool ZM_IsRegisteredStarterChoice(ZM_STARTER_CHOICE eChoice)
{
	// ZM_STARTER_CHOICE_NONE aliases ZM_STARTER_CHOICE_COUNT, so this single
	// comparison rejects the sentinel and every garbage value together.
	return (u_int)eChoice < (u_int)ZM_STARTER_CHOICE_COUNT;
}

u_int ZM_GetStarterChoiceCount()
{
	return (u_int)ZM_STARTER_CHOICE_COUNT;
}

const ZM_StarterChoiceData& ZM_GetStarterChoice(ZM_STARTER_CHOICE eChoice)
{
	if (!ZM_IsRegisteredStarterChoice(eChoice))
	{
		// A row lookup for a choice the table does not name is mis-authored data or
		// a mis-typed caller, so it is SAID OUT LOUD -- but non-fatally, because the
		// function is total and returning the INVALID row is the contract.
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_StarterChoice] ZM_GetStarterChoice: choice %u is not a registered "
			"starter -- returning the INVALID row", (u_int)eChoice);
		return s_xInvalidStarterChoice;
	}
	return s_axStarterChoices[(u_int)eChoice];
}

ZM_SPECIES_ID ZM_ResolvePlayerStarterSpecies(ZM_STARTER_CHOICE eChoice)
{
	// The INVALID row carries ZM_SPECIES_NONE, so the unregistered answer needs no
	// second guard here -- and the one Zenith_Error is logged by the accessor.
	return ZM_GetStarterChoice(eChoice).m_eSpecies;
}

ZM_SPECIES_ID ZM_ResolveCounterStarterSpecies(ZM_STARTER_CHOICE eChoice)
{
	const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(eChoice);
	// FAIL CLOSED TWICE. The INVALID row's m_eCounteredBy is the sentinel, so an
	// unregistered argument stops here; a REGISTERED row whose counter column was
	// mis-authored stops here too, rather than indexing the table a second time.
	if (!ZM_IsRegisteredStarterChoice(xRow.m_eCounteredBy))
	{
		return ZM_SPECIES_NONE;
	}
	return ZM_GetStarterChoice(xRow.m_eCounteredBy).m_eSpecies;
}

bool ZM_CanEnterBattle(const ZM_GameState& xState)
{
	// EMPTINESS ALONE -- see the header. A fainted-aware form would fire during the
	// post-loss pre-heal window and is therefore a live behaviour change, not an
	// inert one.
	return !xState.m_xParty.IsEmpty();
}

bool ZM_ApplyStarterChoice(ZM_GameState& xState, ZM_STARTER_CHOICE eChoice)
{
	// BEFORE ZM_BuildMonsterRecord, not after: that function Zenith_Asserts on its
	// species and then FALLS THROUGH into three more asserting table reads, so a
	// bad choice reaching it does not fail this call -- it ends the boot unit run.
	if (!ZM_IsRegisteredStarterChoice(eChoice))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_StarterChoice] ZM_ApplyStarterChoice: choice %u is not a registered "
			"starter -- granting nothing", (u_int)eChoice);
		return false;
	}

	const ZM_StarterChoiceData& xRow = ZM_GetStarterChoice(eChoice);
	const ZM_Monster xStarter = ZM_BuildMonsterRecord(xRow.m_eSpecies, uZM_STARTER_LEVEL);

	// The dex mark is AFTER the party append and gated on it: ZM_Party::Add is a
	// strict no-op when full, and a dex that recorded a species the party does not
	// hold would be a durable lie no later heal or write-back could undo.
	if (!xState.m_xParty.Add(xStarter))
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM_StarterChoice] ZM_ApplyStarterChoice: the party is already full -- "
			"granting nothing");
		return false;
	}

	xState.MarkCaught(xRow.m_eSpecies);   // caught implies seen
	return true;
}
