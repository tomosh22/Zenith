#include "Zenith.h"

// ============================================================================
// ZM_Tests_DawnmerePlacement (S7 item 3 SC8) -- the boot units that turn the
// authored rival's PLACEMENT from a comment into a checked property.
//
// PURE: no scene, no entity, no physics, no assets, no graphics. These run in the
// headless CI unit gate, which is precisely where the argument is needed -- the
// automated test that walks up to him needs baked terrain and can RequestSkip, so
// it cannot be the only place the whiteout-softlock claim lives.
// ============================================================================

#include <cmath>
#include <cstring>   // strcmp (the entity-name uniqueness walk)
#include <limits>    // quiet_NaN / infinity (the half-extent totality sweep)

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_AssertCapture.h"                     // the totality proofs
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // ZM_ForwardFromRotation
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"    // ZM_TrainerSightFsmTuning -- the walk-up standoff
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

namespace
{
	// The two XZ points the units reason about, built at the SAME Y so the vertical
	// band can never be what quietly saves a bad placement.
	constexpr float fPLACEMENT_TEST_Y = 26.0f;

	Zenith_Maths::Vector3 VesperPoint()
	{
		return Zenith_Maths::Vector3(
			fZM_DAWNMERE_VESPER_X, fPLACEMENT_TEST_Y, fZM_DAWNMERE_VESPER_Z);
	}

	Zenith_Maths::Vector3 SpawnPoint()
	{
		return Zenith_Maths::Vector3(
			fZM_DAWNMERE_TOWN_CENTER_X, fPLACEMENT_TEST_Y, fZM_DAWNMERE_TOWN_CENTER_Z);
	}

	float PlanarDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDeltaX = xA.x - xB.x;
		const float fDeltaZ = xA.z - xB.z;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	// ---- Known-limit W5 fixtures ------------------------------------------------

	// The minimum feet-height SPREAD across the six anchors. It is a floor on
	// "these are six independent measurements", not a claim about the terrain: at
	// 0.05 m it is far below any real Dawnmere relief (the town neighbourhood rolls
	// by roughly half a metre over the roster's 60 m footprint) and far above float
	// noise, so the only thing that can drive it under is the rows COLLAPSING back
	// onto one shared value.
	constexpr float fW5_MIN_FEET_SPREAD = 0.05f;

	// "This row is not just the town-centre anchor again." Generous by float
	// standards and tiny by terrain standards.
	constexpr float fW5_ANCHOR_EPSILON = 1.0e-4f;

	// How many of the six must differ from the town-centre anchor. FOUR, not six:
	// two NPCs genuinely standing on ground that samples to the anchor's height is
	// legitimate content, while four or more collapsing onto it is the partial
	// revert this clause exists to catch.
	constexpr u_int uW5_MIN_ROWS_OFF_THE_ANCHOR = 4u;

	// The shipped Dawnmere human capsule half-extent: scale (0.8, 1.8, 0.8) through
	// ZM_PlayerController::CalculateCapsuleHalfExtent gives radius 0.4 + half
	// cylinder 0.5. Restated as a literal rather than called, because this file is
	// PURE -- naming the component would drag the ECS into the CI unit gate.
	constexpr float fW5_CAPSULE_HALF_EXTENT = 0.9f;

	// The three out-of-range ids every totality sweep feeds: one past the last row,
	// two past it, and the all-ones garbage value.
	constexpr u_int auW5_BAD_IDS[] = {
		(u_int)ZM_DAWNMERE_NPC_COUNT,
		(u_int)ZM_DAWNMERE_NPC_COUNT + 1u,
		0xffffffffu,
	};
	constexpr u_int uW5_BAD_ID_COUNT =
		(u_int)(sizeof(auW5_BAD_IDS) / sizeof(auW5_BAD_IDS[0]));

	bool W5AnchorFieldsFinite(const ZM_DawnmereNpcAnchor& xAnchor)
	{
		return xAnchor.m_szEntityName != nullptr
			&& xAnchor.m_szEntityName[0] != '\0'
			&& std::isfinite(xAnchor.m_fX)
			&& std::isfinite(xAnchor.m_fZ)
			&& std::isfinite(xAnchor.m_fFeetY);
	}

	// The sentinel is identified by the two fields that CANNOT be a roster row: its
	// name, and the town-centre XZ that no authored NPC occupies. Deliberately NOT
	// by its feet height -- in the pass-1 placeholder state every roster row still
	// carries the anchor's height, so a height-based test of "is this the sentinel?"
	// would answer yes for all six and prove nothing.
	bool W5IsSentinelAnchor(const ZM_DawnmereNpcAnchor& xAnchor)
	{
		return xAnchor.m_szEntityName != nullptr
			&& std::strcmp(xAnchor.m_szEntityName, "UNKNOWN") == 0
			&& xAnchor.m_fX == fZM_DAWNMERE_TOWN_CENTER_X
			&& xAnchor.m_fZ == fZM_DAWNMERE_TOWN_CENTER_Z;
	}

	float W5PlanarSeparation(
		const ZM_DawnmereNpcAnchor& xA, const ZM_DawnmereNpcAnchor& xB)
	{
		const float fDeltaX = xA.m_fX - xB.m_fX;
		const float fDeltaZ = xA.m_fZ - xB.m_fZ;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}
}

// ============================================================================
// KNOWN-LIMIT W5 -- the per-NPC authored feet heights.
//
// ★ WHAT THESE SIX UNITS DO AND DO NOT PROVE. They are SELF-CONSISTENCY claims
// about COMPILED CONSTANTS: that six independent heights exist, that they have not
// collapsed back onto the town-centre anchor, that the centre/spawn arithmetic is
// what it says, and that every accessor is total. NONE OF THEM CAN PROVE THE
// HEIGHTS MATCH THE TERRAIN -- they never touch a heightfield. Do not read their
// greenness as "the NPC heights are correct".
//
// The claim they cannot make is ZM_DawnmereNpcGroundTruth_Test's job
// (Tests/ZM_AutoTests_NpcTalk.cpp): it loads the COMMITTED Dawnmere, casts a real
// downward ray at each anchor's XZ against the baked terrain body, and reds if any
// compiled row has drifted from the surface the capsule actually rests on. That
// test needs a terrain bake and can RequestSkip, which is exactly why these units
// exist as well -- and exactly why they must not be mistaken for it.
// ============================================================================

// THE PREMISE OF W5, stated as a property. Dawnmere is not flat under the roster,
// so ONE shared height was an approximation. Collapse the table back to a single
// value and this is exactly zero.
ZENITH_TEST(ZM_Interaction, DawnmereNpcFeetHeights_SpreadProvesTheyAreNotOneSharedValue)
{
	float fMin = ZM_DawnmereNpcFeetY(0u);
	float fMax = fMin;
	for (u_int u = 1u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		const float fFeet = ZM_DawnmereNpcFeetY(u);
		if (fFeet < fMin) { fMin = fFeet; }
		if (fFeet > fMax) { fMax = fFeet; }
	}
	const float fSpread = fMax - fMin;

	ZENITH_ASSERT_GE(fSpread, fW5_MIN_FEET_SPREAD,
		"the six Dawnmere NPC feet heights span only %.5f m (min %.5f, max %.5f) -- "
		"they are still ONE shared value, so W5's per-NPC measurement has not landed",
		fSpread, fMin, fMax);

	// ANTI-NONSENSE, and it is the half that makes the clause above safe to tighten.
	// A mis-pasted measurement (a digit dropped, two rows swapped between scenes)
	// would satisfy any lower bound while putting an NPC a storey off its
	// neighbours -- and the picker's and the sight cone's vertical bands are both
	// fZM_SIGHT_MAX_VERTICAL, so that NPC would be permanently un-talkable.
	ZENITH_ASSERT_LT(fSpread, fZM_SIGHT_MAX_VERTICAL,
		"the Dawnmere NPC feet heights span %.5f m, which is at or beyond the shipped "
		"vertical band of %.3f m -- at least one row is a mis-pasted measurement",
		fSpread, fZM_SIGHT_MAX_VERTICAL);
}

// THE PARTIAL-REVERT GUARD. The spread clause alone survives five rows collapsing
// back onto the town centre while one keeps a measured value; this does not.
ZENITH_TEST(ZM_Interaction, DawnmereNpcFeetHeights_MostRowsDifferFromTheTownCentreAnchor)
{
	u_int uOffTheAnchor = 0u;
	for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		if (std::fabs(ZM_DawnmereNpcFeetY(u) - fZM_DAWNMERE_TOWN_CENTER_FEET_Y)
			> fW5_ANCHOR_EPSILON)
		{
			++uOffTheAnchor;
		}
	}

	ZENITH_ASSERT_GE(uOffTheAnchor, uW5_MIN_ROWS_OFF_THE_ANCHOR,
		"only %u of the %u Dawnmere NPC rows carry a height of their own -- the rest "
		"are still the town-centre anchor %.5f, which is the very approximation W5 "
		"exists to remove",
		uOffTheAnchor, (u_int)ZM_DAWNMERE_NPC_COUNT, fZM_DAWNMERE_TOWN_CENTER_FEET_Y);

	// The wander waypoints are part of the same measurement, and endpoint 0 shares
	// the wanderer's row BY CONSTRUCTION, so only endpoint 1 is an independent value.
	ZENITH_ASSERT_GT(
		std::fabs(ZM_GetDawnmereWanderWaypoint(1u).m_fFeetY
			- fZM_DAWNMERE_TOWN_CENTER_FEET_Y),
		fW5_ANCHOR_EPSILON,
		"the wanderer's far patrol endpoint still reports the town-centre height");
}

// TOTALITY over IDS. Nothing here may Zenith_Assert: these functions are walked
// with out-of-range ids on purpose, and Zenith_Assert calls Zenith_DebugBreak() in
// EVERY configuration -- the whole unit suite runs at boot, so one such assert does
// not fail a test, it ends the boot unit run and takes the gate down.
//
// NO ZENITH_ASSERT_* MAY APPEAR INSIDE THE CAPTURE SCOPE: while one is active,
// Zenith_TestRunner::HandleFailure swallows framework failures and merely bumps the
// hit count, so an in-scope assertion could never red this test. Everything is
// collected into locals and asserted after the scope closes. The hit count must
// likewise be copied out BEFORE the closing brace -- the destructor restores the
// previous count, and scopes do not nest.
ZENITH_TEST(ZM_Interaction, DawnmereNpcAccessors_AreTotalOnEveryDegenerateId)
{
	u_int uHits = 0u;
	u_int uValidRowsSeen      = 0u;
	u_int uValidRowsFinite    = 0u;
	u_int uValidRowsClaimed   = 0u;   // ZM_IsDawnmereNpcId said true
	u_int uValidRowsMistakenForSentinel = 0u;
	u_int uBadIdsClaimed      = 0u;   // ZM_IsDawnmereNpcId said true -- must stay 0
	u_int uBadIdsGaveSentinel = 0u;
	u_int uBadIdsFiniteFeet   = 0u;
	u_int uBadWaypointsGaveSentinel = 0u;
	u_int uValidWaypointsFinite     = 0u;
	const u_int uWaypointCount = ZM_GetDawnmereWanderWaypointCount();

	{
		Zenith_AssertCaptureScope xCapture;

		for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			++uValidRowsSeen;
			if (W5AnchorFieldsFinite(xAnchor)
				&& std::isfinite(ZM_DawnmereNpcFeetY(u))
				&& std::isfinite(ZM_DawnmereNpcCentreY(u, fW5_CAPSULE_HALF_EXTENT)))
			{
				++uValidRowsFinite;
			}
			if (ZM_IsDawnmereNpcId(u)) { ++uValidRowsClaimed; }
			if (W5IsSentinelAnchor(xAnchor)) { ++uValidRowsMistakenForSentinel; }
		}

		for (u_int u = 0u; u < uW5_BAD_ID_COUNT; ++u)
		{
			const u_int uBad = auW5_BAD_IDS[u];
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(uBad);
			if (ZM_IsDawnmereNpcId(uBad)) { ++uBadIdsClaimed; }
			if (W5IsSentinelAnchor(xAnchor) && W5AnchorFieldsFinite(xAnchor))
			{
				++uBadIdsGaveSentinel;
			}
			if (std::isfinite(ZM_DawnmereNpcFeetY(uBad))
				&& std::isfinite(ZM_DawnmereNpcCentreY(uBad, fW5_CAPSULE_HALF_EXTENT)))
			{
				++uBadIdsFiniteFeet;
			}
		}

		for (u_int u = 0u; u < uWaypointCount; ++u)
		{
			if (W5AnchorFieldsFinite(ZM_GetDawnmereWanderWaypoint(u)))
			{
				++uValidWaypointsFinite;
			}
		}
		// The same three degenerate indices, re-aimed at the waypoint accessor. The
		// NPC COUNT is not one past the last WAYPOINT, so the first two are derived
		// from the waypoint count instead; only the all-ones garbage value is shared.
		const u_int auBadWaypoints[] = {
			uWaypointCount, uWaypointCount + 1u, 0xffffffffu };
		static_assert(sizeof(auBadWaypoints) / sizeof(auBadWaypoints[0])
			== uW5_BAD_ID_COUNT,
			"the two degenerate-index inventories must stay the same length -- the "
			"count assertion below is written against uW5_BAD_ID_COUNT");
		for (u_int u = 0u; u < uW5_BAD_ID_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xAnchor =
				ZM_GetDawnmereWanderWaypoint(auBadWaypoints[u]);
			if (W5IsSentinelAnchor(xAnchor) && W5AnchorFieldsFinite(xAnchor))
			{
				++uBadWaypointsGaveSentinel;
			}
		}

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"a Dawnmere placement accessor asserted on an argument -- Zenith_Assert breaks "
		"in EVERY config and the whole unit suite runs at boot, so this would kill the "
		"boot unit run rather than fail one test");

	ZENITH_ASSERT_EQ(uValidRowsSeen, (u_int)ZM_DAWNMERE_NPC_COUNT,
		"the roster walk did not visit every row");
	ZENITH_ASSERT_EQ(uValidRowsFinite, (u_int)ZM_DAWNMERE_NPC_COUNT,
		"a roster row carries a null/empty name or a non-finite coordinate");
	ZENITH_ASSERT_EQ(uValidRowsClaimed, (u_int)ZM_DAWNMERE_NPC_COUNT,
		"ZM_IsDawnmereNpcId rejected an id the table actually holds");
	ZENITH_ASSERT_EQ(uValidRowsMistakenForSentinel, 0u,
		"a REAL roster row is indistinguishable from the UNKNOWN sentinel -- a caller "
		"that skipped the id check could not tell content from a bad lookup");

	ZENITH_ASSERT_EQ(uBadIdsClaimed, 0u,
		"ZM_IsDawnmereNpcId accepted an out-of-range id");
	ZENITH_ASSERT_EQ(uBadIdsGaveSentinel, uW5_BAD_ID_COUNT,
		"an out-of-range id did not return the DEFINED town-centre sentinel row");
	ZENITH_ASSERT_EQ(uBadIdsFiniteFeet, uW5_BAD_ID_COUNT,
		"an out-of-range id produced a non-finite height -- a NaN reaching a transform "
		"poisons the body, the model matrix and every distance derived from them");

	ZENITH_ASSERT_GT(uWaypointCount, 1u,
		"the wanderer's patrol must have at least two endpoints to be a patrol");
	ZENITH_ASSERT_EQ(uValidWaypointsFinite, uWaypointCount,
		"a wander waypoint carries a null/empty name or a non-finite coordinate");
	ZENITH_ASSERT_EQ(uBadWaypointsGaveSentinel, uW5_BAD_ID_COUNT,
		"an out-of-range waypoint index did not return the DEFINED sentinel row");
}

// TOTALITY over the HALF-EXTENT, plus the arithmetic it feeds. Both halves matter:
// the equality is what a future refactor would break silently, and the degenerate
// sweep is what stops a NaN scale reaching an authored transform.
ZENITH_TEST(ZM_Interaction, DawnmereNpcCentre_IsFeetPlusHalfExtentAndFailsClosed)
{
	for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(
			ZM_DawnmereNpcCentreY(u, fW5_CAPSULE_HALF_EXTENT),
			ZM_DawnmereNpcFeetY(u) + fW5_CAPSULE_HALF_EXTENT, 1.0e-5f,
			"row %u's authored centre is no longer its feet plus ONE capsule "
			"half-extent", u);
	}

	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();
	const float afDegenerate[] = { fNaN, fInf, -fInf, -1.0f, -0.0f };
	constexpr u_int uDEGENERATE_COUNT =
		(u_int)(sizeof(afDegenerate) / sizeof(afDegenerate[0]));

	// Results are collected inside the capture scope and judged outside it: no
	// ZENITH_ASSERT_* may fire while a scope is active (it would be swallowed).
	float afCentre[(u_int)ZM_DAWNMERE_NPC_COUNT * uDEGENERATE_COUNT] = {};
	float afWandererSpawn[uDEGENERATE_COUNT] = {};
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
		{
			for (u_int uBad = 0u; uBad < uDEGENERATE_COUNT; ++uBad)
			{
				afCentre[u * uDEGENERATE_COUNT + uBad] =
					ZM_DawnmereNpcCentreY(u, afDegenerate[uBad]);
			}
		}
		for (u_int uBad = 0u; uBad < uDEGENERATE_COUNT; ++uBad)
		{
			afWandererSpawn[uBad] = ZM_DawnmereWandererSpawnY(afDegenerate[uBad]);
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"a degenerate capsule half-extent made a Dawnmere placement accessor assert");

	for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		for (u_int uBad = 0u; uBad < uDEGENERATE_COUNT; ++uBad)
		{
			// EXACT equality, tolerance 0: "treated as zero" means the feet height
			// comes back untouched, not approximately.
			ZENITH_ASSERT_EQ_FLOAT(afCentre[u * uDEGENERATE_COUNT + uBad],
				ZM_DawnmereNpcFeetY(u), 0.0f,
				"row %u did not fail closed to its feet height on degenerate "
				"half-extent index %u", u, uBad);
		}
	}
	for (u_int uBad = 0u; uBad < uDEGENERATE_COUNT; ++uBad)
	{
		ZENITH_ASSERT_EQ_FLOAT(afWandererSpawn[uBad],
			ZM_DawnmereNpcFeetY(ZM_DAWNMERE_NPC_WANDERER), 0.0f,
			"the wanderer's spawn height did not fail closed to its feet height on "
			"degenerate half-extent index %u", uBad);
	}
}

// The ONE NPC whose authored Y is not its centre. The extra half-extent of air is
// what lets gravity settle the only DYNAMIC body in Dawnmere from the FRONT side
// instead of from inside the mesh, so it is a shipped placement property rather
// than a convenience.
ZENITH_TEST(ZM_Interaction, DawnmereWandererSpawn_CarriesExactlyOneExtraHalfExtent)
{
	const float fFeet =
		ZM_DawnmereNpcFeetY(ZM_DAWNMERE_NPC_WANDERER);
	const float fCentre =
		ZM_DawnmereNpcCentreY(ZM_DAWNMERE_NPC_WANDERER, fW5_CAPSULE_HALF_EXTENT);
	const float fSpawn = ZM_DawnmereWandererSpawnY(fW5_CAPSULE_HALF_EXTENT);

	ZENITH_ASSERT_EQ_FLOAT(fSpawn - fCentre, fW5_CAPSULE_HALF_EXTENT, 1.0e-5f,
		"the wanderer's spawn no longer clears its resting centre by EXACTLY one "
		"capsule half-extent (spawn %.5f, centre %.5f)", fSpawn, fCentre);
	ZENITH_ASSERT_EQ_FLOAT(fSpawn - fFeet, 2.0f * fW5_CAPSULE_HALF_EXTENT, 1.0e-5f,
		"the wanderer's spawn is no longer its measured feet plus TWO half-extents");

	// STRICTLY greater, and it is not implied by the equalities above: a build in
	// which the half-extent term vanished would satisfy "spawn - centre == half"
	// only by also making half zero, and this clause is what says out loud that the
	// air is real.
	ZENITH_ASSERT_GT(fSpawn, fCentre,
		"the wanderer is authored AT or BELOW its resting centre -- the capsule "
		"starts inside the terrain mesh and gravity cannot settle it from the front");
	ZENITH_ASSERT_GT(fCentre, fFeet,
		"the wanderer's resting centre is not above its own feet height");

	// The extra air must not be so large that the drop is a fall: one half-extent is
	// 0.9 m, which settles in about a third of a second.
	ZENITH_ASSERT_LT(fSpawn - fFeet, fZM_SIGHT_MAX_VERTICAL,
		"the wanderer is authored more than a full vertical band above its own "
		"ground -- it would fall through the sight/interact band on the way down");
}

// The XZ half of the table: the derivations the comments claim, the separations the
// picker depends on, and the name uniqueness a test resolving entities by name
// depends on. Feet heights are deliberately NOT touched here -- the two units above
// own those, so a height regression names a height unit.
ZENITH_TEST(ZM_Interaction, DawnmereNpcAnchors_KeepTheirAuthoredXZDerivations)
{
	const ZM_DawnmereNpcAnchor& xVillager =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_VILLAGER);
	const ZM_DawnmereNpcAnchor& xClerk =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_TRADE_POST_CLERK);
	const ZM_DawnmereNpcAnchor& xCaretaker =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_CARETAKER);
	const ZM_DawnmereNpcAnchor& xWarden =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WARDEN);
	const ZM_DawnmereNpcAnchor& xWanderer =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WANDERER);
	const ZM_DawnmereNpcAnchor& xVesper =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER);

	// The derivations, spelled as ARITHMETIC on the town-centre anchor rather than
	// as re-typed literals: a moved town centre must move the plaza roster with it.
	ZENITH_ASSERT_EQ_FLOAT(xVillager.m_fX, fZM_DAWNMERE_TOWN_CENTER_X, 0.0f,
		"the villager left the x = 512 spawn-to-villager corridor");
	ZENITH_ASSERT_EQ_FLOAT(xVillager.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z + 10.0f, 0.0f,
		"the villager is no longer 10 m straight +Z of the spawn");
	ZENITH_ASSERT_EQ_FLOAT(xClerk.m_fX, fZM_DAWNMERE_TOWN_CENTER_X + 14.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xClerk.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xCaretaker.m_fX, fZM_DAWNMERE_TOWN_CENTER_X - 14.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xCaretaker.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xWarden.m_fX, fZM_DAWNMERE_TOWN_CENTER_X - 34.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xWarden.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z + 18.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xWanderer.m_fX, fZM_DAWNMERE_TOWN_CENTER_X + 28.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xWanderer.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z - 4.0f, 0.0f);
	// The rival's XZ is derived in this file's header; the table must not restate it.
	ZENITH_ASSERT_EQ_FLOAT(xVesper.m_fX, fZM_DAWNMERE_VESPER_X, 0.0f,
		"the anchor table and ZM_DawnmerePlacement.h disagree about where the rival "
		"stands -- his yaw derivation reads the header, so they would silently part");
	ZENITH_ASSERT_EQ_FLOAT(xVesper.m_fZ, fZM_DAWNMERE_VESPER_Z, 0.0f);

	// ★★ THE THREE FLANK NPCs MUST STAY OFF z = 480. A solid STATIC AABB on that
	// line wedges ZM_PlayerHomeRoundTrip_Test, which drives from (512, 480) to
	// (384, 0, 480) with NO obstacle avoidance and dies at its frame cap naming a
	// distance rather than naming the NPC that blocked it.
	const ZM_DawnmereNpcAnchor* apxOffCorridor[] = { &xClerk, &xCaretaker, &xWarden };
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_GT(
			std::fabs(apxOffCorridor[u]->m_fZ - fZM_DAWNMERE_TOWN_CENTER_Z), 10.0f,
			"'%s' has drifted onto the z = %.1f Home traversal corridor -- it will "
			"wedge a suite that never mentions it",
			apxOffCorridor[u]->m_szEntityName, fZM_DAWNMERE_TOWN_CENTER_Z);
	}

	// THE CLOSEST PAIR. The picker resolves the NEAREST FACED candidate, so two NPCs
	// within reach of each other make "which one answered?" a function of sub-metre
	// walk error and the walk-up tests can no longer assert a winner BY ENTITY ID.
	// The authored minimum is villager <-> clerk / caretaker at sqrt(14^2 + 8^2) =
	// 16.1 m, against a 2.5 m global reach.
	float fClosest = -1.0f;
	const char* szClosestA = "<none>";
	const char* szClosestB = "<none>";
	u_int uNameCollisions = 0u;
	for (u_int uA = 0u; uA < (u_int)ZM_DAWNMERE_NPC_COUNT; ++uA)
	{
		const ZM_DawnmereNpcAnchor& xA = ZM_GetDawnmereNpcAnchor(uA);
		for (u_int uB = uA + 1u; uB < (u_int)ZM_DAWNMERE_NPC_COUNT; ++uB)
		{
			const ZM_DawnmereNpcAnchor& xB = ZM_GetDawnmereNpcAnchor(uB);
			const float fSeparation = W5PlanarSeparation(xA, xB);
			if (fClosest < 0.0f || fSeparation < fClosest)
			{
				fClosest = fSeparation;
				szClosestA = xA.m_szEntityName;
				szClosestB = xB.m_szEntityName;
			}
			if (std::strcmp(xA.m_szEntityName, xB.m_szEntityName) == 0)
			{
				++uNameCollisions;
			}
		}
	}
	ZENITH_ASSERT_GT(fClosest, fZM_INTERACT_MAX_DISTANCE * 5.0f,
		"the closest authored NPC pair is '%s' <-> '%s' at %.3f m, inside 5x the "
		"global interact reach -- the nearest-faced-candidate picker can now be made "
		"to answer with either of them",
		szClosestA, szClosestB, fClosest);
	// The same separation against the WIDER of the two shipped ranges: being SPOTTED
	// happens at 8 m, so two trainers inside that of each other would double-engage.
	ZENITH_ASSERT_GT(fClosest, fZM_SIGHT_MAX_DISTANCE,
		"the closest authored NPC pair is inside the shipped trainer sight range");

	// Entity NAMES are how every windowed test resolves these bodies out of the
	// committed scene bytes, and FindEntityByName answers with the first match.
	ZENITH_ASSERT_EQ(uNameCollisions, 0u,
		"two authored Dawnmere NPCs share an entity name -- every test that resolves "
		"one by name would silently measure whichever the scene stored first");

	// The patrol, and the binding that keeps ONE point on the heightfield from
	// acquiring two independently editable heights.
	ZENITH_ASSERT_EQ(ZM_GetDawnmereWanderWaypointCount(), 2u,
		"the authored patrol is written as exactly two endpoints in Zenithmon.cpp");
	const ZM_DawnmereNpcAnchor& xWaypoint0 = ZM_GetDawnmereWanderWaypoint(0u);
	const ZM_DawnmereNpcAnchor& xWaypoint1 = ZM_GetDawnmereWanderWaypoint(1u);
	ZENITH_ASSERT_EQ_FLOAT(xWaypoint0.m_fX, xWanderer.m_fX, 0.0f,
		"patrol endpoint 0 is no longer the wanderer's own spawn anchor");
	ZENITH_ASSERT_EQ_FLOAT(xWaypoint0.m_fZ, xWanderer.m_fZ, 0.0f,
		"patrol endpoint 0 is no longer the wanderer's own spawn anchor");
	ZENITH_ASSERT_EQ_FLOAT(xWaypoint0.m_fFeetY, xWanderer.m_fFeetY, 0.0f,
		"patrol endpoint 0 and the wanderer's spawn share one point on the "
		"heightfield but now carry DIFFERENT measured heights");
	ZENITH_ASSERT_EQ_FLOAT(xWaypoint1.m_fX, xWanderer.m_fX, 0.0f,
		"the patrol is authored as a north/south loop on one x line");
	ZENITH_ASSERT_EQ_FLOAT(xWaypoint1.m_fZ, fZM_DAWNMERE_TOWN_CENTER_Z + 4.0f, 0.0f,
		"the far patrol endpoint has moved off townCentre.z + 4");
	ZENITH_ASSERT_EQ_FLOAT(
		std::fabs(xWaypoint1.m_fZ - xWaypoint0.m_fZ), 8.0f, 0.0f,
		"the patrol is no longer the authored 8 m leg");
}

// THE WHITEOUT-SOFTLOCK GUARD, and it is not hypothetical. Losing a trainer battle
// routes through the WILD write-back (m_bPendingWhiteout), heals, and warps the
// player to the Dawnmere TownCenter (ZM_GameStateManager.cpp:177-186) WITHOUT
// setting RIVAL1_DEFEATED -- and ZM_MayTrainerEngage's flagged arm ignores the
// session latch (ZM_TrainerSightFsm.cpp:83-86). So a rival whose cone covered the
// respawn would re-challenge forever and the player could never reach the Care
// Center. RANGE is what prevents that, which is why clause (b) exists.
ZENITH_TEST(ZM_Interaction, Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget)
{
	const ZM_TrainerSightTuning xTuning;   // default-constructed IS the shipped tuning

	// (a) THE PROPERTY. A player who just whited out is NOT inside his cone.
	ZENITH_ASSERT_FALSE(
		ZM_IsTargetInTrainerSightFromRotation(
			VesperPoint(), ZM_DawnmereVesperFacing(), SpawnPoint(), xTuning),
		"the authored rival can see the whiteout respawn point -- an honest loss "
		"would softlock the game in a forced-battle loop");

	// (b) ANTI-VACUITY, and the whole point of the unit. He IS facing the spawn, so
	//     clause (a) is carried by RANGE alone rather than by an accidental facing
	//     that a future re-derivation could silently flip.
	const Zenith_Maths::Vector3 xForward =
		ZM_ForwardFromRotation(ZM_DawnmereVesperFacing());
	Zenith_Maths::Vector3 xToSpawn(
		SpawnPoint().x - VesperPoint().x, 0.0f, SpawnPoint().z - VesperPoint().z);
	const float fLength = std::sqrt(xToSpawn.x * xToSpawn.x + xToSpawn.z * xToSpawn.z);
	ZENITH_ASSERT_GT(fLength, 0.0f, "the rival is standing on the spawn");
	xToSpawn /= fLength;
	const float fDot = xForward.x * xToSpawn.x + xForward.z * xToSpawn.z;
	ZENITH_ASSERT_GT(fDot, fZM_SIGHT_MIN_FACING_DOT,
		"the rival is NOT facing the spawn (dot %.4f), so clause (a) above proves "
		"nothing about range -- re-derive the placement", fDot);

	// (c) the margin, spelled against the SHIPPED sight range rather than a literal.
	ZENITH_ASSERT_GT(PlanarDistance(VesperPoint(), SpawnPoint()),
		fZM_SIGHT_MAX_DISTANCE * 4.0f,
		"the rival must clear the whiteout respawn by a real margin");

	// (d) the z = 480 Home corridor ZM_PlayerHomeRoundTrip_Test drives BLIND.
	ZENITH_ASSERT_GT(
		std::fabs(fZM_DAWNMERE_VESPER_Z - fZM_DAWNMERE_TOWN_CENTER_Z),
		fZM_SIGHT_MAX_DISTANCE * 4.0f,
		"the rival is close enough to the z=480 traversal corridor to hijack a "
		"suite that never mentions him");
}

// The facing is DERIVED from the two authored anchors, and the argument order of
// that atan2 is load-bearing: atan2(x, z), matching ZM_ForwardFromRotation's +Z
// convention. Transposing it turns him 90 degrees, the approach never enters his
// cone, and the only symptom would be an automated test that times out naming a
// distance.
ZENITH_TEST(ZM_Interaction, Vesper_FacingIsDerivedFromTheTownCentreBearing)
{
	const float fExpectedYaw = std::atan2(
		fZM_DAWNMERE_TOWN_CENTER_X - fZM_DAWNMERE_VESPER_X,
		fZM_DAWNMERE_TOWN_CENTER_Z - fZM_DAWNMERE_VESPER_Z);
	ZENITH_ASSERT_EQ_FLOAT(ZM_DawnmereVesperYaw(), fExpectedYaw, 0.0001f,
		"the rival's yaw is no longer the town-centre bearing");

	// The quaternion really is that yaw about +Y (never eulerAngles).
	const Zenith_Maths::Vector3 xForward =
		ZM_ForwardFromRotation(ZM_DawnmereVesperFacing());
	ZENITH_ASSERT_EQ_FLOAT(xForward.x, std::sin(fExpectedYaw), 0.0005f,
		"the facing quaternion does not match the derived yaw");
	ZENITH_ASSERT_EQ_FLOAT(xForward.z, std::cos(fExpectedYaw), 0.0005f,
		"the facing quaternion does not match the derived yaw");

	// A point one metre in FRONT of him is in sight; the same metre BEHIND is not.
	// Without this the two clauses above would agree with any self-consistent
	// convention, including a transposed one.
	const Zenith_Maths::Vector3 xFront(
		VesperPoint().x + xForward.x, fPLACEMENT_TEST_Y, VesperPoint().z + xForward.z);
	const Zenith_Maths::Vector3 xBehind(
		VesperPoint().x - xForward.x, fPLACEMENT_TEST_Y, VesperPoint().z - xForward.z);
	const ZM_TrainerSightTuning xTuning;
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSightFromRotation(
		VesperPoint(), ZM_DawnmereVesperFacing(), xFront, xTuning));
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSightFromRotation(
		VesperPoint(), ZM_DawnmereVesperFacing(), xBehind, xTuning));
}

// ============================================================================
// S7 item 1 SC3 -- THE RIVAL NOW MOVES, so every clearance in this file stopped
// being a statement about a POINT and became one about a DISC.
//
// Before SC3 the authored rival was a static box: his separations were fixed and
// the whiteout-softlock guard above was a claim about one coordinate. He now walks
// at anyone who enters his cone, which means every geometric argument in
// ZM_DawnmerePlacement.h has to survive him moving up to
//   fZM_SIGHT_MAX_DISTANCE - m_fApproachStandoffMetres
// metres in ANY direction -- that is the exact reach of the walk, because he only
// starts when the target is inside the cone and stops on the standoff ring.
//
// (The OTHER bound on the walk -- that his speed can actually cross that distance
// inside the fail-open timeout -- is proven by simulation in
// Approach_WalkConvergesIntoTheStandoffRingBeforeTheTimeout, and is deliberately
// not restated here: this file may not name a component, so it cannot see the
// speed. The two bounds are independent and each is asserted where it is visible.)
// ============================================================================
ZENITH_TEST(ZM_Interaction, Vesper_ApproachStandoffClearsEveryAuthoredNeighbour)
{
	const ZM_TrainerSightFsmTuning xTuning;   // default-constructed IS the shipped tuning

	// THE WALK'S REACH. Derived from the two shipped numbers rather than typed, so
	// widening the cone or shrinking the standoff moves this claim automatically.
	const float fWalkRadius =
		fZM_SIGHT_MAX_DISTANCE - xTuning.m_fApproachStandoffMetres;
	ZENITH_ASSERT_GT(fWalkRadius, 0.0f,
		"the standoff is at least as wide as the sight range, so the rival can never "
		"take a step -- the walk-up is dead content and every clause below is vacuous");

	// The closest any authored body may come to any point the rival can occupy: his
	// own standoff ring (he must never try to stand inside someone else) plus the
	// global interact reach (two bodies inside that of each other make "which NPC
	// answered?" a function of sub-metre walk error, and every walk-up test in this
	// repo asserts its winner BY ENTITY ID).
	const float fRequiredClearance =
		xTuning.m_fApproachStandoffMetres + fZM_INTERACT_MAX_DISTANCE;

	// ---- (a) EVERY authored neighbour, and both patrol endpoints -------------
	const ZM_DawnmereNpcAnchor& xVesper =
		ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER);
	float fClosest = -1.0f;
	const char* szClosest = "<none>";
	for (u_int u = 0u; u < (u_int)ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		if (u == (u_int)ZM_DAWNMERE_NPC_RIVAL_VESPER)
		{
			continue;
		}
		const ZM_DawnmereNpcAnchor& xOther = ZM_GetDawnmereNpcAnchor(u);
		const float fSeparation = W5PlanarSeparation(xVesper, xOther);
		if (fClosest < 0.0f || fSeparation < fClosest)
		{
			fClosest = fSeparation;
			szClosest = xOther.m_szEntityName;
		}
		ZENITH_ASSERT_GT(fSeparation, fWalkRadius + fRequiredClearance,
			"'%s' is %.3f m from the rival's anchor, and the walk-up can carry him "
			"%.3f m of that -- his standoff ring would close to within %.3f m of an "
			"authored body. Re-derive the placement in ZM_DawnmerePlacement.h",
			xOther.m_szEntityName, fSeparation, fWalkRadius, fRequiredClearance);
	}
	// The patrol endpoints are the one MOVING neighbour, so they are walked too: the
	// wanderer is not where his anchor says at any given instant.
	const u_int uWaypointCount = ZM_GetDawnmereWanderWaypointCount();
	for (u_int u = 0u; u < uWaypointCount; ++u)
	{
		const ZM_DawnmereNpcAnchor& xWaypoint = ZM_GetDawnmereWanderWaypoint(u);
		ZENITH_ASSERT_GT(W5PlanarSeparation(xVesper, xWaypoint),
			fWalkRadius + fRequiredClearance,
			"patrol endpoint '%s' (%u) is inside the rival's reachable disc plus its "
			"clearance -- the two dynamic capsules in this scene could meet",
			xWaypoint.m_szEntityName, u);
	}
	// ANTI-VACUITY: the loop above really walked a roster. A run in which every
	// neighbour was skipped would satisfy each per-anchor clause by never evaluating
	// one, and the nearest name is what makes the margin readable in the log.
	ZENITH_ASSERT_GT(fClosest, 0.0f,
		"no authored neighbour was measured at all (nearest reported as '%s') -- the "
		"clearance clauses above are vacuous", szClosest);

	// ---- (b) THE THREE RECORDED CLEARANCES, so the written figures cannot rot --
	// These are the numbers Shortfalls 1.8 item 1 and this header quote in prose.
	// Asserting them here means moving an anchor invalidates the PROSE loudly rather
	// than leaving a comment that quietly describes a different town.
	ZENITH_ASSERT_EQ_FLOAT(
		W5PlanarSeparation(xVesper, ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_CARETAKER)),
		27.20f, 0.01f, "the recorded caretaker clearance (27.2 m) has drifted");
	ZENITH_ASSERT_EQ_FLOAT(
		W5PlanarSeparation(xVesper, ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WARDEN)),
		28.64f, 0.01f, "the recorded warden clearance (28.6 m) has drifted");
	ZENITH_ASSERT_EQ_FLOAT(
		W5PlanarSeparation(xVesper, ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_VILLAGER)),
		40.50f, 0.01f, "the recorded villager clearance (40.5 m) has drifted");

	// ---- (c) THE WHITEOUT SOFTLOCK, RE-ARGUED UNDER MOTION -------------------
	// Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget proves he cannot see the
	// respawn FROM HIS ANCHOR. He can now leave that anchor, so the guard has to hold
	// from the WORST point of his reachable disc: a whited-out player must still be
	// outside the cone of a rival who has walked as far toward the spawn as the
	// walk-up permits. Losing to him writes no defeat flag, so a rival who could
	// re-engage on the respawn would loop the player out of the game forever.
	const float fSpawnSeparation = PlanarDistance(VesperPoint(), SpawnPoint());
	ZENITH_ASSERT_GT(fSpawnSeparation - fWalkRadius, fZM_SIGHT_MAX_DISTANCE,
		"the rival stands %.3f m from the whiteout respawn and the walk-up can carry "
		"him %.3f m of it, leaving %.3f m against a %.3f m sight range -- an honest "
		"loss would softlock the game in a forced-battle loop",
		fSpawnSeparation, fWalkRadius, fSpawnSeparation - fWalkRadius,
		fZM_SIGHT_MAX_DISTANCE);

	// ---- (d) THE z = 480 HOME CORRIDOR, same treatment ----------------------
	// ZM_PlayerHomeRoundTrip_Test drives that line BLIND with no obstacle avoidance,
	// and the rival is now a DYNAMIC body that can be walked into rather than a
	// static wall that would at least stop the capsule predictably.
	ZENITH_ASSERT_GT(
		std::fabs(fZM_DAWNMERE_VESPER_Z - fZM_DAWNMERE_TOWN_CENTER_Z) - fWalkRadius,
		fZM_SIGHT_MAX_DISTANCE,
		"the rival can walk close enough to the z = %.1f traversal corridor to hijack "
		"a suite that never mentions him", fZM_DAWNMERE_TOWN_CENTER_Z);
}
