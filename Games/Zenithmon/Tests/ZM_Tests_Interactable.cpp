#include "Zenith.h"

// ============================================================================
// ZM_Tests_Interactable -- S6 item 3 SC4 unit tests for the live interaction
// wiring's HEADLESS half: the ZM_Interactable component's configuration /
// candidacy / serialization contract, the pure ZM_RaiseKindForRole map that
// decides WHICH ZM_UI_MenuStack seam a role talks through, and the
// ZM_InteractionRuntime latches the windowed tests (SC5-SC7) poll.
//
// Nothing here raises a screen, loads a scene or needs graphics: the component is
// constructed against an INVALID entity handle (it stores the handle by value and
// never dereferences it for any of the surface tested here), the role -> seam map
// is a pure switch, and the runtime's latch surface is exercised through its own
// documented reset seam. So no RequestSkip is needed. Category ZM_Interaction --
// the same category as the SC1/SC2 logic units, since this is the same feature.
//
// The latch units matter more than they look: ZM_InteractionRuntime's latches are
// process-GLOBAL (the between-tests hook in Zenithmon.cpp can only reach ownerless
// state), so the reset unit deliberately POPULATES a latch first and only then
// resets -- a reset unit whose fixture was never populated passes vacuously and
// would keep passing after the reset it exists to police stopped working.
// ============================================================================

#include <cstring>   // strcmp (raise-kind name distinctness)
#include <limits>    // quiet_NaN (the radius sanitiser's fixture)

#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_ComponentMeta.h"   // the registry the SC7 gate's NPCs deserialize through
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"

namespace
{
	constexpr float fTEST_EPSILON = 0.0001f;

	// An interactable built against an INVALID entity handle. Every member function
	// exercised below reads only the component's own PODs, so no scene is required.
	// (Interact() is the one member that reaches outside; it is covered by the
	// headless automated test ZM_NpcDispatch_Test, which has a real scene.)
	struct DetachedInteractable
	{
		Zenith_Entity   m_xEntity;
		ZM_Interactable m_xInteractable;

		DetachedInteractable() : m_xEntity(), m_xInteractable(m_xEntity) {}
	};

	// ---- Two SC7 fixtures, deliberately NOT one ----------------------------
	//
	// BEATS vs merely-PLACED. The consolidated gate (ZM_S6InteractGate_Test) proves
	// one BEAT per dispatch role -- talk / buy / heal -- and each beat presses E at
	// ONE specific authored NPC. Role coverage is asserted over THAT table only.
	//
	// The wider placed table must never be allowed to launder beat coverage, and
	// since S7 SC1 it demonstrably would: it carries the warden, a SECOND TALKER, so
	// "every role has a placed carrier" stays true after ZM_NPC_VILLAGER is re-rolled
	// to SHOPKEEP -- while the gate's talk beat silently starts raising a mart. That
	// is the exact hole splitting these two tables closes.

	// The gate's three beats. m_eExpectedKind is the seam that beat exists to press,
	// spelled HERE rather than read back off the row, so a re-rolled row disagrees
	// with the test instead of quietly agreeing with itself.
	struct GateBeat
	{
		ZM_NPC_ID         m_eNpc;
		const char*       m_szEntityName;    // the entity name Zenithmon.cpp authors
		const char*       m_szBeat;
		ZM_NPC_RAISE_KIND m_eExpectedKind;
	};

	constexpr u_int uGATE_BEAT_COUNT = 3u;
	const GateBeat axGATE_BEATS[uGATE_BEAT_COUNT] = {
		{ ZM_NPC_VILLAGER,         "Npc_Villager",       "talk", ZM_NPC_RAISE_DIALOGUE    },
		{ ZM_NPC_TRADE_POST_CLERK, "Npc_TradePostClerk", "buy",  ZM_NPC_RAISE_SHOP        },
		{ ZM_NPC_CARETAKER,        "Npc_Caretaker",      "heal", ZM_NPC_RAISE_CARE_CENTER },
	};

	// Every STATIONARY NPC Zenithmon.cpp stands in Dawnmere -- a SUPERSET of the beat
	// table (the warden is placed and walkable but carries no beat of his own yet).
	// ZM_NPC_WANDERER is excluded even though SC8 DOES author it
	// (ZM_QueueDawnmereWanderer places "Npc_Wanderer"): it MOVES, and everything here
	// is approached at a FIXED authored position, so reaching it takes the chase
	// machinery SC8's own walk-up test carries instead.
	constexpr u_int uPLACED_NPC_COUNT = 4u;
	const ZM_NPC_ID aePLACED_NPCS[uPLACED_NPC_COUNT] = {
		ZM_NPC_VILLAGER,           // "Npc_Villager"       -- the gate's talk beat
		ZM_NPC_TRADE_POST_CLERK,   // "Npc_TradePostClerk" -- the gate's buy beat
		ZM_NPC_CARETAKER,          // "Npc_Caretaker"      -- the gate's heal beat
		ZM_NPC_ROUTE_WARDEN,       // "Npc_Warden"         -- S7 SC1's flag-gated talker
	};

	// The registered NAME of the interaction component, spelled exactly as
	// Zenithmon.cpp's ZENITH_REGISTER_COMPONENT line spells it.
	constexpr const char* szINTERACTABLE_META_NAME = "ZM_Interactable";
}

// ---- Component configuration ------------------------------------------------

// An entity that carries the component but was never configured must be inert: if
// it were a candidate it would absorb the interact press and leave the player
// standing in front of something with nothing to say.
ZENITH_TEST(ZM_Interaction, Interactable_DefaultsAreSafe)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE,
		"a default-constructed interactable names no NPC row");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"a default-constructed interactable must not be a candidate");
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fDEFAULT_RADIUS, fTEST_EPSILON,
		"the default reach BONUS is zero -- exactly the global reach, never more");
}

ZENITH_TEST(ZM_Interaction, Interactable_SetGetRoundTrip)
{
	DetachedInteractable xFixture;

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_TRADE_POST_CLERK));
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(),
		(u_int)ZM_NPC_TRADE_POST_CLERK);

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetRadius(1.25f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(), 1.25f, fTEST_EPSILON);

	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"a configured row plus the authored flag makes a live candidate");
	xFixture.m_xInteractable.SetInteractable(false);
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable());
}

// Fail CLOSED: a bad id clears the row rather than keeping the previous one, so a
// mis-authored value can only ever produce an INERT NPC, never a wrong conversation.
ZENITH_TEST(ZM_Interaction, Interactable_RejectsOutOfRangeNpcId)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable());

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetNpcId((ZM_NPC_ID)ZM_NPC_COUNT),
		"ZM_NPC_COUNT is not an NPC");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE,
		"a rejected id must not leave the PREVIOUS row installed");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"clearing the row also drops candidacy");

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetNpcId((ZM_NPC_ID)9999u));
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
}

// A runaway radius would let one NPC swallow every interact press in the town, and
// a NaN one would poison the picker's distance comparison, so the setter clamps and
// sanitises rather than trusting its caller.
ZENITH_TEST(ZM_Interaction, Interactable_RadiusClampsAndSanitises)
{
	DetachedInteractable xFixture;

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(-1.0f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(), 0.0f, fTEST_EPSILON,
		"a negative reach bonus clamps to zero, never shrinks reach below it");

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(
		ZM_Interactable::fMAX_RADIUS + 100.0f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fMAX_RADIUS, fTEST_EPSILON);

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetRadius(0.5f));
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(
		std::numeric_limits<float>::quiet_NaN()));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fDEFAULT_RADIUS, fTEST_EPSILON,
		"a non-finite radius resets to the default rather than propagating a NaN");
}

// The candidacy answer is the CONJUNCTION of the authored flag and a real row --
// flipping the flag on an unconfigured component must not make it a candidate.
ZENITH_TEST(ZM_Interaction, Interactable_InteractableRequiresAConfiguredRow)
{
	DetachedInteractable xFixture;
	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"the flag alone, with no NPC row, is not enough to absorb the interact press");

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_CARETAKER));
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"configuring the row makes the already-set flag effective");
}

// OnStart re-validates whatever authoring / deserialization left behind.
// OnStart's row guard is REACHABLE, but not by the route its old name implied:
// ZM_NPC_NONE == ZM_NPC_COUNT, so `m_eNpcId >= ZM_NPC_COUNT` is true for any
// UNCONFIGURED component (and for one whose id SetNpcId refused). What the guard
// actually enforces is that such a row cannot be left live, which is what stops an
// unconfigured NPC entity from silently absorbing the player's interact press.
ZENITH_TEST(ZM_Interaction, Interactable_OnStartClearsAnUnconfiguredRow)
{
	DetachedInteractable xFixture;
	// Deliberately NOT SetNpcId: the row stays NONE, which is what an entity gets
	// when authoring forgets to assign one.
	xFixture.m_xInteractable.SetInteractable(true);
	xFixture.m_xInteractable.OnStart();

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"an unconfigured row must NOT survive OnStart as a live candidate");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
}

ZENITH_TEST(ZM_Interaction, Interactable_OnStartKeepsAValidRow)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_WANDERER));
	xFixture.m_xInteractable.SetInteractable(true);
	xFixture.m_xInteractable.OnStart();
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"a VALID row survives OnStart untouched");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_WANDERER);
}

// ---- Serialization ----------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Interactable_SerializeRoundTrip)
{
	DetachedInteractable xSource;
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetNpcId(ZM_NPC_TRADE_POST_CLERK));
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetRadius(1.75f));
	xSource.m_xInteractable.SetInteractable(true);

	Zenith_DataStream xStream;
	xSource.m_xInteractable.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	u_int uVersion = 0u;
	xStream >> uVersion;
	ZENITH_ASSERT_EQ(uVersion, ZM_Interactable::uSERIALIZATION_VERSION,
		"the component-contract leading version must be written first");

	xStream.SetCursor(0u);
	DetachedInteractable xTarget;
	xTarget.m_xInteractable.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(),
		(u_int)ZM_NPC_TRADE_POST_CLERK);
	ZENITH_ASSERT_EQ_FLOAT(xTarget.m_xInteractable.GetRadius(), 1.75f, fTEST_EPSILON);
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsInteractable(),
		"every field survives the round trip");
}

// A payload from a future / stale schema must leave an INERT component rather than
// a half-configured one that reads garbage as an NPC id.
ZENITH_TEST(ZM_Interaction, Interactable_DeserializeRejectsForeignVersion)
{
	Zenith_DataStream xStream;
	const u_int uForeignVersion = ZM_Interactable::uSERIALIZATION_VERSION + 1u;
	xStream << uForeignVersion;
	xStream.SetCursor(0u);

	DetachedInteractable xTarget;
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	xTarget.m_xInteractable.SetInteractable(true);
	xTarget.m_xInteractable.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
	ZENITH_ASSERT_FALSE(xTarget.m_xInteractable.IsInteractable(),
		"an unreadable payload leaves an inert component, never a stale live one");
}

// ---- Role -> seam mapping (pure; nothing is raised) --------------------------

ZENITH_TEST(ZM_Interaction, RaiseKind_TalkerMapsToDialogue)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_TALKER),
		(u_int)ZM_NPC_RAISE_DIALOGUE,
		"a talker talks through ZM_UI_MenuStack::TryPushDialogue");
}

ZENITH_TEST(ZM_Interaction, RaiseKind_ShopkeepMapsToShop)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_SHOPKEEP),
		(u_int)ZM_NPC_RAISE_SHOP,
		"a shopkeep opens the mart through ZM_UI_MenuStack::TryOpenShop");
}

ZENITH_TEST(ZM_Interaction, RaiseKind_CaretakerMapsToCareCenter)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_CARETAKER),
		(u_int)ZM_NPC_RAISE_CARE_CENTER,
		"a caretaker raises the heal yes/no prompt");
}

// Totality: EVERY enumerated role has a real seam behind it. A role added without a
// dispatch arm would otherwise ship as an NPC that silently does nothing.
ZENITH_TEST(ZM_Interaction, RaiseKind_EveryRoleMapsToARealSeam)
{
	ZENITH_ASSERT_GT((u_int)ZM_NPC_ROLE_COUNT, 0u,
		"the role enum must be non-empty, or the walk below is vacuous");
	for (u_int u = 0u; u < (u_int)ZM_NPC_ROLE_COUNT; ++u)
	{
		const ZM_NPC_RAISE_KIND eKind = ZM_RaiseKindForRole((ZM_NPC_ROLE)u);
		ZENITH_ASSERT_NE((u_int)eKind, (u_int)ZM_NPC_RAISE_NONE,
			"role %u has no raise seam", u);
		ZENITH_ASSERT_LT((u_int)eKind, (u_int)ZM_NPC_RAISE_COUNT,
			"role %u mapped outside the raise-kind enum", u);
	}
}

ZENITH_TEST(ZM_Interaction, RaiseKind_OutOfRangeRoleMapsToNone)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole((ZM_NPC_ROLE)ZM_NPC_ROLE_COUNT),
		(u_int)ZM_NPC_RAISE_NONE,
		"ZM_NPC_ROLE_COUNT is not a role -- it must fall through to NONE, which LOGS");
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole((ZM_NPC_ROLE)4242u),
		(u_int)ZM_NPC_RAISE_NONE);
}

ZENITH_TEST(ZM_Interaction, RaiseKind_NamesAreTotalAndDistinct)
{
	for (u_int u = 0u; u < (u_int)ZM_NPC_RAISE_COUNT; ++u)
	{
		const char* szName = ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)u);
		ZENITH_ASSERT_NOT_NULL(szName, "raise kind %u has no name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "raise kind %u has an EMPTY name", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_TRUE(
				std::strcmp(szName, ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)v)) != 0,
				"raise kinds %u and %u share a name", u, v);
		}
	}
	ZENITH_ASSERT_STREQ(ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)ZM_NPC_RAISE_COUNT),
		"UNKNOWN", "the name formatter is TOTAL -- it never indexes off the end");
}

// Content x mapping: every AUTHORED roster row lands on a real seam. This is the
// join the two halves of the feature meet at, and neither table alone can catch a
// row whose role has no arm.
// S7 item 2 SC1: the warden is the ONE live consumer of story-flag gating, and
// gating selects CONTENT -- it never re-routes which seam a role talks through. So
// his row has to keep landing on the dialogue seam specifically: given any other
// role he would open a mart or a heal prompt and his refusal lines, which are the
// whole demonstration, would never be spoken.
ZENITH_TEST(ZM_Interaction, Interactable_WardenRowIsDispatchableAsDialogue)
{
	const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(xRow.m_eRole),
		(u_int)ZM_NPC_RAISE_DIALOGUE,
		"'%s' has role %u, which does not talk through ZM_UI_MenuStack::TryPushDialogue",
		xRow.m_szDisplayName, (u_int)xRow.m_eRole);
}

ZENITH_TEST(ZM_Interaction, RaiseKind_EveryAuthoredNpcHasASeam)
{
	ZENITH_ASSERT_GT(ZM_GetNpcCount(), 0u,
		"an empty roster would make this walk vacuous");
	for (u_int u = 0u; u < ZM_GetNpcCount(); ++u)
	{
		const ZM_NpcData& xRow = ZM_GetNpcData((ZM_NPC_ID)u);
		ZENITH_ASSERT_NE((u_int)ZM_RaiseKindForRole(xRow.m_eRole),
			(u_int)ZM_NPC_RAISE_NONE,
			"authored NPC %u ('%s') has no raise seam", u, xRow.m_szDisplayName);
	}
}

// ---- ZM_InteractionRuntime latches ------------------------------------------

// The latches are process-GLOBAL, so "start" here means "after the documented
// reset seam" -- which is exactly the state the between-tests hook installs.
ZENITH_TEST(ZM_Interaction, Runtime_LatchesStartCleared)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	const ZM_InteractionRuntime xRuntime{};
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult(),
		"a cleared runtime reports that nothing has been attempted");
	ZENITH_ASSERT_EQ((u_int)xRuntime.GetLastResult(),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE);
	ZENITH_ASSERT_EQ(xRuntime.GetLastTarget(), INVALID_ENTITY_ID,
		"a cleared runtime names no target");
}

ZENITH_TEST(ZM_Interaction, Runtime_RaiseCountStartsZero)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	const ZM_InteractionRuntime xRuntime{};
	ZENITH_ASSERT_EQ(xRuntime.GetRaiseCount(), 0u,
		"nothing has been raised yet, so the monotonic raise count is zero");
}

// POPULATE, then reset, then assert cleared. Ticking with no interact edge is a
// legitimate decision (NO_INPUT_EDGE) and still sets the has-run flag, which is the
// observable that separates "cleared" from "ran and decided nothing" -- without it
// this unit would assert the reset against a fixture that was never dirty.
ZENITH_TEST(ZM_Interaction, Runtime_ResetClearsAPopulatedLatch)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	ZM_InteractionRuntime xRuntime;
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult());

	xRuntime.Tick(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_TRUE(xRuntime.HasLatchedResult(),
		"Tick must latch its decision EITHER WAY -- rejects are what walk-up tests poll");

	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult(),
		"the reset seam clears a latch that was genuinely populated");
	ZENITH_ASSERT_EQ((u_int)xRuntime.GetLastResult(),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE);
	ZENITH_ASSERT_EQ(xRuntime.GetRaiseCount(), 0u);
}

// The probe cap has to comfortably clear the authored roster, or the live site
// would be truncating candidates in a shipped town.
ZENITH_TEST(ZM_Interaction, Runtime_ProbeCapClearsTheAuthoredRoster)
{
	// NB this is a LOWER bound only. The cap bounds interactables present in one
	// SCENE, which is not the same quantity as the roster size -- a town may place
	// several entities per roster row -- so it is a sanity floor, not a proof the
	// cap is adequate. The `== 64u` assertion that used to sit here was removed: it
	// restated the constant back to itself, so it failed on ANY edit, right or
	// wrong, and carried no correctness signal.
	ZENITH_ASSERT_GT(uZM_MAX_INTERACT_PROBES, ZM_GetNpcCount(),
		"the probe cap must exceed the whole authored NPC roster");
}

// ---- SC7 gate preconditions -------------------------------------------------

// The SC7 consolidated gate (ZM_S6InteractGate_Test) proves one beat per ROLE, and
// because it is m_bRequiresGraphics it SKIPS (== passes) in headless CI -- so a role
// that quietly lost its beat is reported by NOTHING there. This unit is the headless
// stand-in, and it walks the BEAT table specifically.
//
// It deliberately does NOT walk the placed roster: that table carries the warden, a
// second TALKER, so coverage over it survives the very mutation this unit exists to
// catch. Nor is it a duplicate of ZM_Tests_NpcData's Npc_RolesCoverEveryDispatchArm,
// which walks the WHOLE roster (wanderer and warden included) and stays green in
// exactly the same case.
//
// The beats' ids are spelled as enumerators, so "names a real row" is true by
// construction and no range walk is written for it -- that assertion could not fail,
// and this file has already had one such tautology removed
// (Runtime_ProbeCapClearsTheAuthoredRoster).
ZENITH_TEST(ZM_Interaction, GateRoster_BeatNpcsCoverEveryRole)
{
	// REDS ON: adding a ZM_NPC_ROLE enumerator (or deleting a row from axGATE_BEATS).
	// One beat per role is what "the gate covers the dispatch switch" means, and a
	// new role with no beat is a shipped arm nothing ever presses. This also subsumes
	// the usual non-vacuity guard: an EMPTY role enum fails here rather than making
	// the walk below pass silently.
	ZENITH_ASSERT_EQ(uGATE_BEAT_COUNT, (u_int)ZM_NPC_ROLE_COUNT,
		"the gate authors %u beats for %u dispatch roles -- some role has no beat, and "
		"the gate SKIPS in headless CI so nothing else would report it",
		uGATE_BEAT_COUNT, (u_int)ZM_NPC_ROLE_COUNT);

	// REDS ON: re-rolling a beat NPC's m_eRole in ZM_NpcData.cpp -- precisely the
	// ZM_NPC_VILLAGER -> ZM_NPC_ROLE_SHOPKEEP mutation. The talk beat would walk up to
	// Npc_Villager, press E and be handed a MART; here that reads as the row's seam
	// (SHOP) disagreeing with the seam the beat was written against (DIALOGUE).
	for (u_int u = 0u; u < uGATE_BEAT_COUNT; ++u)
	{
		const GateBeat& xBeat = axGATE_BEATS[u];
		const ZM_NpcData& xRow = ZM_GetNpcData(xBeat.m_eNpc);
		const ZM_NPC_RAISE_KIND eActual = ZM_RaiseKindForRole(xRow.m_eRole);
		ZENITH_ASSERT_EQ((u_int)eActual, (u_int)xBeat.m_eExpectedKind,
			"the gate's %s beat walks up to %s ('%s', role %u), which now raises %s "
			"instead of %s", xBeat.m_szBeat, xBeat.m_szEntityName, xRow.m_szDisplayName,
			(u_int)xRow.m_eRole, ZM_NpcRaiseKindName(eActual),
			ZM_NpcRaiseKindName(xBeat.m_eExpectedKind));
	}

	// REDS ON: that same re-roll "fixed" by editing m_eExpectedKind above to agree
	// with the mutated row. The kinds would line up again, but the role the NPC
	// VACATED would then be carried by no beat at all, and this walk still reports it.
	//
	// Together with the count equality above, this also forces the three beats onto
	// three DISTINCT roles -- three carriers cannot double up on a role without
	// leaving another uncovered -- so no separate distinctness assertion is written.
	for (u_int uRole = 0u; uRole < (u_int)ZM_NPC_ROLE_COUNT; ++uRole)
	{
		bool bCovered = false;
		for (u_int u = 0u; u < uGATE_BEAT_COUNT && !bCovered; ++u)
		{
			bCovered = ((u_int)ZM_GetNpcData(axGATE_BEATS[u].m_eNpc).m_eRole == uRole);
		}
		ZENITH_ASSERT_TRUE(bCovered,
			"role %u is carried by NO gate BEAT, so the SC7 walk-up gate never presses E "
			"at an NPC that uses it -- give a beat NPC that role, or add a beat",
			uRole);
	}
}

// What the wider PLACED table still legitimately proves, now that role coverage has
// been taken off it. Every assertion here is about "an NPC standing in the town that
// a walk-up test can reach"; none of them speaks about roles as a SET, so none of
// them can stand in for the beat coverage above.
ZENITH_TEST(ZM_Interaction, GateRoster_PlacedNpcsAreStationaryAndIncludeEveryBeat)
{
	// REDS ON: emptying aePLACED_NPCS. Both walks below are bounded by this count, so
	// an empty table would make them pass while proving nothing.
	ZENITH_ASSERT_GT(uPLACED_NPC_COUNT, 0u,
		"the placed-NPC table is empty, so the walks below are vacuous");

	// REDS ON: setting m_bWanders on one of these rows in ZM_NpcData.cpp. Every entry
	// here is approached at a FIXED authored position; a row that started patrolling
	// would walk out from under that approach, and the walk-up test would time out
	// naming a distance rather than the patrol. ZM_Tests_NpcData's
	// Npc_ExactlyOneRowWanders cannot see this: MOVING the flag from the wanderer onto
	// the villager keeps its count at exactly one.
	for (u_int u = 0u; u < uPLACED_NPC_COUNT; ++u)
	{
		const ZM_NpcData& xRow = ZM_GetNpcData(aePLACED_NPCS[u]);
		ZENITH_ASSERT_FALSE(xRow.m_bWanders,
			"placed NPC '%s' now WANDERS, but this table's entries are walked up to at a "
			"fixed authored position", xRow.m_szDisplayName);
	}

	// REDS ON: dropping an NPC from aePLACED_NPCS while a beat still names it -- which
	// is exactly what removing its ZM_QueueDawnmereNpc call from Zenithmon.cpp is
	// supposed to look like here. A beat that presses E at an NPC nobody placed is a
	// gate beat that can only ever time out.
	for (u_int uBeat = 0u; uBeat < uGATE_BEAT_COUNT; ++uBeat)
	{
		bool bPlaced = false;
		for (u_int u = 0u; u < uPLACED_NPC_COUNT && !bPlaced; ++u)
		{
			bPlaced = (aePLACED_NPCS[u] == axGATE_BEATS[uBeat].m_eNpc);
		}
		ZENITH_ASSERT_TRUE(bPlaced,
			"the gate's %s beat walks up to %s, which no longer appears in the placed "
			"roster -- nothing stands in Dawnmere for that beat to reach",
			axGATE_BEATS[uBeat].m_szBeat, axGATE_BEATS[uBeat].m_szEntityName);
	}
}

// The gate reaches every NPC by loading the BAKED Dawnmere scene and resolving
// entities by name, which only yields an armed ZM_Interactable if the component
// deserializes -- and it only deserializes if it is in the component-meta registry
// under exactly this name (DeserializeEntityComponents keys on the type NAME and
// merely LOGS + skips an unknown one). Deleting the ZENITH_REGISTER_COMPONENT line
// therefore turns every NPC in the town silently inert, which the windowed gate
// reports as a boot timeout hundreds of frames later -- and never reports at all in
// headless CI, where it skips.
//
// The ORDER VALUE is deliberately NOT asserted equal to 113. Serialization is keyed
// by NAME (see above), the component header says in so many words that the number is
// a within-entity tiebreak that must not be "fixed" by renumbering, and an `== 113u`
// clause would restate Zenithmon.cpp's literal back to itself -- exactly the
// tautology that was already removed from Runtime_ProbeCapClearsTheAuthoredRoster.
// What IS asserted is the part the engine genuinely does not police: Finalize sorts
// by order and logs every pair but has NO duplicate detection, so two components
// silently sharing an order sort arbitrarily against each other.
ZENITH_TEST(ZM_Interaction, GateRoster_InteractableIsRegisteredExactlyOnce)
{
	const Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	ZENITH_ASSERT_TRUE(xRegistry.IsInitialized(),
		"the component-meta registry is not sealed yet -- the walk below would be over "
		"an empty list and the uniqueness check would pass vacuously");

	const Zenith_ComponentMeta* pxMeta = xRegistry.GetMetaByName(szINTERACTABLE_META_NAME);
	ZENITH_ASSERT_NOT_NULL(pxMeta,
		"'%s' is not in the component-meta registry -- it cannot deserialize out of the "
		"baked Dawnmere scene, so every authored NPC would be inert",
		szINTERACTABLE_META_NAME);
	// A failed assert RECORDS and CONTINUES, so the null case has to be guarded here
	// rather than trusted.
	if (pxMeta == nullptr)
	{
		return;
	}

	// ZM components claim the 100+ band (Zenithmon.cpp). Registering a game component
	// down in the engine's band is how an order collision gets introduced in the first
	// place.
	ZENITH_ASSERT_GE(pxMeta->m_uSerializationOrder, 100u,
		"'%s' is registered at order %u, below the 100+ band ZM components claim",
		szINTERACTABLE_META_NAME, pxMeta->m_uSerializationOrder);

	// OnStart is the hook that de-arms an unconfigured row (see
	// Interactable_OnStartClearsAnUnconfiguredRow). Concept detection dropping it
	// would leave a mis-authored NPC live and absorbing the player's interact press.
	ZENITH_ASSERT_NOT_NULL(pxMeta->m_pfnOnStart,
		"'%s' registered without its OnStart hook -- an unconfigured NPC row would stay "
		"a live interact candidate",
		szINTERACTABLE_META_NAME);

	const Zenith_Vector<const Zenith_ComponentMeta*>& xSorted = xRegistry.GetAllMetasSorted();
	ZENITH_ASSERT_GT(xSorted.GetSize(), 0u,
		"the sorted meta list is empty, so the uniqueness count below would be vacuous");
	u_int uAtSameOrder = 0u;
	for (u_int u = 0u; u < xSorted.GetSize(); ++u)
	{
		const Zenith_ComponentMeta* pxOther = xSorted.Get(u);
		if (pxOther == nullptr)
		{
			continue;
		}
		if (pxOther->m_uSerializationOrder == pxMeta->m_uSerializationOrder)
		{
			++uAtSameOrder;
		}
	}
	// Only the ORDER count is asserted: the registry stores metas in a map KEYED BY TYPE
	// NAME (RegisterComponent overwrites by name and Finalize pushes exactly one pointer
	// per map entry), so a by-NAME count is structurally 1 once GetMetaByName has already
	// resolved above -- it could not fail. Duplicate ORDERS are genuinely unpoliced.
	ZENITH_ASSERT_EQ(uAtSameOrder, 1u,
		"%u components share serialization order %u with '%s' -- the registry sorts "
		"duplicates arbitrarily against each other and warns about nothing",
		uAtSameOrder, pxMeta->m_uSerializationOrder, szINTERACTABLE_META_NAME);
}
