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
