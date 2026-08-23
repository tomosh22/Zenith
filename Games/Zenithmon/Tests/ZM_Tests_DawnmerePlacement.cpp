#include "Zenith.h"

// ============================================================================
// ZM_Tests_DawnmerePlacement (S7 item 3 SC8) -- the boot units that turn the
// authored rival's PLACEMENT from a comment into a checked property.
//
// PURE: no scene, no entity, no physics, no assets, no graphics. These run in the
// headless CI unit gate, which is precisely where the argument is needed -- the
// automated test that walks up to him needs baked terrain and can RequestSkip, so
// it cannot be the only place the whiteout-softlock claim lives.
//
// ★ S8 SC-D WIDENED WHAT "PURE" READS HERE, WITHOUT WIDENING WHAT IT MEANS. The
// lab units at the bottom of this file additionally read (a) the COMPILED
// Dawnmere terrain RECIPE -- a static table in ZM_TerrainAuthoring.cpp, not a
// bake, so no asset is touched -- because that is the only way to bind the lab
// placement to the terrain site it was reserved on without re-spelling the pad's
// numbers, and (b) ZM_FollowCamera's PURE STATICS (nothing is constructed), so a
// camera-clearance margin is computed with the SHIPPED clamp rather than a
// re-derived one. Tests/ZM_Tests_ProfLabPlacement.cpp already reads that camera
// the same way. No entity is created here, which is a hard constraint rather
// than a style note: the boot unit suite runs before the initial scene load and
// allocates entity indices that scene authoring would then bake in.
// ============================================================================

#include <bit>       // bit_cast -- the ZM-D-183 frozen-facing bit clauses
#include <cmath>
#include <cstring>   // strcmp (the entity-name uniqueness walk)
#include <limits>    // quiet_NaN / infinity (the half-extent totality sweep)

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_AssertCapture.h"                     // the totality proofs
#include "Zenithmon/Components/ZM_FollowCamera.h"               // the camera's PURE statics (nothing is constructed)
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // ZM_ForwardFromRotation
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"    // ZM_TrainerSightFsmTuning -- the walk-up standoff
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"         // the INTERIOR the lab exterior must wrap
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"         // the RESERVED lab site (compiled recipe, no bake)

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

	// The shipped human capsule half-extent, read from the ONE compiled body
	// contract (radius 0.4 + half cylinder 0.5). It used to be restated as a
	// literal here because the only other spelling lived on an ECS component this
	// PURE file must not name; the contract header is pure, so it can be named.
	constexpr float fW5_CAPSULE_HALF_EXTENT = fZM_HUMAN_BODY_HALF_HEIGHT;

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
// session latch (ZM_TrainerSightFsm.cpp). So a rival whose cone covered the
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
//
// ★ ZM-D-183 CHANGED WHAT THIS TEST MEANS, WITHOUT CHANGING A LINE OF IT. The
// facing is now a FROZEN bit pattern rather than AngleAxis(ZM_DawnmereVesperYaw()),
// so the clauses below no longer check that a derivation is self-consistent -- they
// check that the FROZEN CONSTANT still equals the derivation the anchors imply.
// That makes this test the STALENESS ORACLE for the frozen block: move
// fZM_DAWNMERE_VESPER_X/Z or the town centre without re-freezing and it reds here.
// Keep the comparisons TOLERANCE-based; a bit-exact one would be permanently red
// in Release (see Vesper_FrozenFacingIsExactlyTheDeclaredBits below).
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
// ZM-D-184 -- AN AUTHORED DYNAMIC BODY MUST NOT SPAWN ON THE SURFACE
//
// ★ THIS TEST WOULD HAVE FAILED BEFORE ZM-D-184, WHICH IS THE POINT. The rival was
// authored at ZM_DawnmereNpcCentreY -- his RESTING centre, feet + one capsule
// half-extent -- so his capsule touched the ground with ~13 mm of margin, and he
// fell through the world intermittently.
//
// The measured mechanism (full detail in the ZM_DawnmereTrainerSpawnY block in
// ZM_DawnmerePlacement.h): the first two frames after a scene load take ~0.49 s,
// which the physics accumulator drained as ~29 consecutive 1/60 s substeps. A body
// resting exactly on the surface free-falls through that burst; once the capsule's
// lower SPHERE CENTRE passes below the terrain's one-sided mesh, the contact normal
// inverts and the solver expels it downward. Observed: 0.61 m of sink by frame 2,
// then a fall to y = -50 and gone.
//
// The engine substep cap is the root fix. This clause pins the SECOND half -- that
// no authored dynamic body is left depending on the solver catching it on the very
// first tick -- because that is a property of the CONTENT and nothing else in this
// file could state it.
// ============================================================================
ZENITH_TEST(ZM_Interaction, Vesper_SpawnsClearOfTheGroundNotOnIt)
{
	const float fHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
	const float fRestingCentre =
		ZM_DawnmereNpcCentreY(ZM_DAWNMERE_NPC_RIVAL_VESPER, fHalfExtent);
	const float fSpawn = ZM_DawnmereTrainerSpawnY(fHalfExtent);

	// (1) He spawns ABOVE his resting centre by a real margin -- not on it.
	ZENITH_ASSERT_GT(fSpawn, fRestingCentre,
		"the authored rival spawns AT his resting centre again. A dynamic capsule "
		"authored exactly on the surface falls through the world when a long frame "
		"drains many physics substeps at once -- see ZM-D-184.");

	// (2) The margin is a FULL capsule half-extent, the same clearance the wanderer
	// has always been given. Stated as an equality rather than "> 0" so a future
	// edit that shaves it to a token few millimetres reds here rather than silently
	// re-opening the defect.
	ZENITH_ASSERT_EQ_FLOAT(fSpawn - fRestingCentre, fHalfExtent, 1.0e-4f,
		"the rival's spawn clearance is no longer one capsule half-extent");

	// (3) The two spawn helpers agree in SHAPE. If someone re-derives one of them,
	// the other should move with it -- they exist for the same reason.
	const float fWandererClearance =
		ZM_DawnmereWandererSpawnY(fHalfExtent)
		- ZM_DawnmereNpcCentreY(ZM_DAWNMERE_NPC_WANDERER, fHalfExtent);
	ZENITH_ASSERT_EQ_FLOAT(fSpawn - fRestingCentre, fWandererClearance, 1.0e-4f,
		"the rival and the wanderer no longer get the same spawn clearance");

	// (4) TOTALITY, in this file's house style: a degenerate half-extent must not
	// produce a NaN that would poison a body and a transform.
	const float fDegenerate = ZM_DawnmereTrainerSpawnY(
		std::numeric_limits<float>::quiet_NaN());
	ZENITH_ASSERT_TRUE(std::isfinite(fDegenerate),
		"ZM_DawnmereTrainerSpawnY returned a non-finite height for a NaN half-extent");
}

// ============================================================================
// ZM-D-183 -- THE FROZEN FACING IS STILL THE DERIVED BEARING
//
// ZM_DawnmereVesperFacing() is now four frozen bit patterns rather than
// AngleAxis(atan2(...)), because the computed form authored DIFFERENT scene bytes
// from a Debug and a Release tools build (full argument in the header). Freezing
// removes the drift but introduces a new way to be wrong: the constant can go
// STALE. Move fZM_DAWNMERE_VESPER_X/Z or the town centre and the frozen quaternion
// silently keeps pointing down the OLD bearing, with every existing clause here
// still green -- they all reason about the frozen value's self-consistency, never
// about whether it still matches the anchors.
//
// ★ THIS TEST DOES NOT CHECK THE BEARING, AND THAT IS NOT AN OVERSIGHT.
// Vesper_FacingIsDerivedFromTheTownCentreBearing above ALREADY compares
// ZM_DawnmereVesperFacing()'s forward vector against the live atan2 derivation to
// 0.0005 -- so post-freeze it IS the staleness oracle, unchanged and for free.
// Duplicating it here with a second tolerance would just create two numbers that
// can disagree. What is genuinely NEW after the freeze, and unchecked anywhere
// else, is the integrity of the frozen value ITSELF: that the accessor really
// returns the declared constants, and that those constants are a legal rotation.
//
// ★ AND IT IS NOT BIT-EXACT AGAINST THE DERIVATION, WHICH WOULD BE A BUG. The
// whole finding behind ZM-D-183 is that the derivation's last bits are
// build-configuration dependent, so a bit-exact frozen-vs-derived assert would be
// PERMANENTLY RED in Release while green in Debug -- a worse tripwire than none.
// The bit-exact comparison belongs where the value does NOT move with the config:
// against the committed file, in ZM_Tests_CommittedSceneBytes.cpp. The three
// checks divide cleanly and none substitutes for another:
//   * ...IsDerivedFromTheTownCentreBearing -- STALE constant (right bits, wrong bearing)
//   * this test                            -- CORRUPT constant (not a rotation, or bypassed)
//   * ...CommittedSceneBytes               -- DRIFTED asset (right bearing, wrong bits)
// ============================================================================
ZENITH_TEST(ZM_Interaction, Vesper_FrozenFacingIsExactlyTheDeclaredBits)
{
	const Zenith_Maths::Quat xFrozen = ZM_DawnmereVesperFacing();

	// (1) The accessor really returns the declared constants. Without this, every
	// other clause in this file would still pass if someone quietly reverted the
	// body to the computed form and left the constants sitting unused in the
	// header -- which would silently restore the per-config drift while the
	// bearing oracle stayed green, because the computed form IS the right bearing.
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(xFrozen.x),
		uZM_DAWNMERE_VESPER_FACING_X_BITS,
		"ZM_DawnmereVesperFacing() no longer returns the frozen x bits");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(xFrozen.y),
		uZM_DAWNMERE_VESPER_FACING_Y_BITS,
		"ZM_DawnmereVesperFacing() no longer returns the frozen y bits");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(xFrozen.z),
		uZM_DAWNMERE_VESPER_FACING_Z_BITS,
		"ZM_DawnmereVesperFacing() no longer returns the frozen z bits");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(xFrozen.w),
		uZM_DAWNMERE_VESPER_FACING_W_BITS,
		"ZM_DawnmereVesperFacing() no longer returns the frozen w bits");

	// (2) The declared bits are a UNIT quaternion. Hand-editing a bit pattern is
	// the one way this could stop being one, and a non-unit rotation would be
	// renormalized by Jolt the instant a body was created from it -- re-opening
	// exactly the drift the freeze closes, at body-creation time instead of at
	// build-configuration time.
	const float fNorm = std::sqrt(
		xFrozen.x * xFrozen.x + xFrozen.y * xFrozen.y +
		xFrozen.z * xFrozen.z + xFrozen.w * xFrozen.w);
	ZENITH_ASSERT_EQ_FLOAT(fNorm, 1.0f, 1.0e-6f,
		"the frozen facing is not a unit quaternion -- Jolt would renormalize it "
		"on body creation and the authored bytes would drift again");

	// (3) It is a pure YAW. The authoring contract is a rotation about +Y only; a
	// non-zero x or z would tilt the sight cone out of the horizontal plane, which
	// no clause in this file measures and no automated test would name.
	ZENITH_ASSERT_EQ_FLOAT(xFrozen.x, 0.0f, 0.0f,
		"the frozen rival facing is not a pure yaw (x component non-zero)");
	ZENITH_ASSERT_EQ_FLOAT(xFrozen.z, 0.0f, 0.0f,
		"the frozen rival facing is not a pure yaw (z component non-zero)");
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

// The Dawnmere facade is a visual exterior for the separately loaded PlayerHome
// scene.  These values must share one real-world scale: the exterior may round
// the interior's wall envelope up to a clean blockout metre, but it must never
// become a deep placeholder building with an unrelated doorway.
ZENITH_TEST(ZM_Interaction, HomeExterior_EnvelopeAndEntranceMatchPlayerHomeContract)
{
	const ZM_DawnmereBlockout xShell = ZM_GetDawnmereHomeShell();
	const ZM_DawnmereBlockout xLeftJamb = ZM_GetDawnmereHomeDoorLeft();
	const ZM_DawnmereBlockout xRightJamb = ZM_GetDawnmereHomeDoorRight();
	const ZM_DawnmereBlockout xTrigger = ZM_GetDawnmereHomeDoorTrigger();

	const float fInteriorOuterWidth =
		fZM_PLAYERHOME_HALF_WIDTH * 2.0f + fZM_PLAYERHOME_WALL_THICKNESS;
	const float fInteriorOuterDepth =
		fZM_PLAYERHOME_HALF_DEPTH * 2.0f + fZM_PLAYERHOME_WALL_THICKNESS;

	// Rounded up, never rounded down: the interior's outer wall envelope fits
	// inside the facade with less than one deliberately cosmetic metre spare.
	ZENITH_ASSERT_GE(xShell.m_xScale.x, fInteriorOuterWidth,
		"the Dawnmere facade is %.2f m wide but PlayerHome's outer wall envelope is %.2f m",
		xShell.m_xScale.x, fInteriorOuterWidth);
	ZENITH_ASSERT_LT(xShell.m_xScale.x - fInteriorOuterWidth, 1.0f,
		"the Dawnmere facade has %.2f m of unexplained width beyond PlayerHome's %.2f m envelope",
		xShell.m_xScale.x - fInteriorOuterWidth, fInteriorOuterWidth);
	ZENITH_ASSERT_GE(xShell.m_xScale.z, fInteriorOuterDepth,
		"the Dawnmere facade is %.2f m deep but PlayerHome's outer wall envelope is %.2f m",
		xShell.m_xScale.z, fInteriorOuterDepth);
	ZENITH_ASSERT_LT(xShell.m_xScale.z - fInteriorOuterDepth, 1.0f,
		"the Dawnmere facade has %.2f m of unexplained depth beyond PlayerHome's %.2f m envelope",
		xShell.m_xScale.z - fInteriorOuterDepth, fInteriorOuterDepth);
	ZENITH_ASSERT_GE(xShell.m_xScale.y, fZM_PLAYERHOME_WALL_HEIGHT,
		"the Dawnmere facade is lower than PlayerHome's %.2f m walls",
		fZM_PLAYERHOME_WALL_HEIGHT);

	// The exterior entrance lives on the facade's -Z face, and its clear frame
	// plus sensor use the exact 4.0 x 2.5 m PlayerHome aperture.
	ZENITH_ASSERT_EQ_FLOAT(xShell.Min().z, fZM_DAWNMERE_HOME_ENTRANCE_Z, 0.0f,
		"the exterior entrance no longer lies on the Dawnmere facade's -Z face");
	const float fExteriorApertureWidth = xRightJamb.Min().x - xLeftJamb.Max().x;
	ZENITH_ASSERT_EQ_FLOAT(fExteriorApertureWidth,
		fZM_PLAYERHOME_APERTURE_HALF_WIDTH * 2.0f, 0.0f,
		"the exterior frame width no longer matches PlayerHome's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xLeftJamb.m_xScale.y, fZM_PLAYERHOME_APERTURE_HEIGHT, 0.0f,
		"the exterior jamb height no longer matches PlayerHome's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xRightJamb.m_xScale.y, fZM_PLAYERHOME_APERTURE_HEIGHT, 0.0f,
		"the exterior jamb height no longer matches PlayerHome's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.m_xScale.x, fExteriorApertureWidth, 0.0f,
		"the exterior portal sensor no longer covers exactly the visible entrance");
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.m_xScale.y, fZM_PLAYERHOME_APERTURE_HEIGHT, 0.0f,
		"the exterior portal sensor no longer matches the visible entrance height");
}

// ============================================================================
// S8 SC-D -- THE DAWNMERE LAB SITE
//
// ★ WHAT THESE EIGHT UNITS ARE FOR. SC-D produces NUMBERS and nothing else: no
// entity is authored until SC-E. So the ONLY thing standing between a wrong
// number and a re-authored Dawnmere.zscen is this file plus
// ZM_DawnmereLabGroundTruth_Test -- and that oracle needs a GITIGNORED terrain
// bake and therefore RequestSkips in CI, where a skip counts as a pass. These
// units carry the CI-visible weight for the whole slice.
//
// ★ WHAT THEY CAN AND CANNOT SEE, STATED BEFORE ANYONE READS THEM AS MORE. They
// are claims about COMPILED CONSTANTS: that the exterior wraps the interior, that
// the site is the reserved one, that the routes clear the building and the cast,
// that the accessors are total, and that the measured table is neither the
// placeholder nor one value stamped ten times. NONE of them touches a
// heightfield, so none can prove a row MATCHES the terrain -- and in particular a
// row holding a plausible-but-wrong height (its neighbour's, say) passes every
// clause here. Only the oracle can catch that. Do not read this file's greenness
// as "the lab ground table is correct".
//
// ★ TWO OF THEM ARE RED ON PURPOSE UNTIL THE TABLE IS FROZEN, and their messages
// say so: LabGroundSamples_AreTenMeasurementsInsideTheGradedBand and
// LabGroundSamples_NoRowSilentlyRepeatsAnother. They are the CI-visible statement
// that the measure -> freeze -> rebuild loop has not closed yet.
// ============================================================================

namespace
{
	// "These two floats are the same authored number." Generous by float
	// standards, tiny against every placement quantity the lab uses.
	constexpr float fLAB_EXACT_EPSILON = 1.0e-4f;

	// The camera contract's arm fraction, from the ZM-D-173 block in
	// Source/World/ZM_DawnmerePlacement.h: at every authoritative sample the
	// CLAMPED arm must keep at least half the authored pivot->camera distance.
	// The live enforcement is ZM_DawnmereCameraClearance_Test, which runs the
	// shipped ZM_FollowCamera::ClampArmDistance against the shipped physics world;
	// the units below run the SAME shipped clamp over compiled geometry, so this
	// fraction is the one number they mirror rather than compute.
	constexpr float fLAB_MIN_ARM_FRACTION = 0.5f;

	// ★ THE SLACK THE BOUNDARY CLAUSE IN LabDirtPath_... NEEDS, AND WHY A
	// ZERO-SLACK ORDERING COMPARISON THERE IS A BUG RATHER THAN A STRICTER TEST.
	// That clause feeds LabRequiredHorizontalGap()'s inversion straight back
	// through the shipped clamp, so the two sides are THE SAME NUMBER with a
	// multiply by (arm / desired) and a multiply by (desired / arm) applied in
	// between. A value compared against its own round-tripped inversion cannot
	// survive float rounding: measured, the left side lands ~1e-6 low (3.000416
	// against 3.000417), and an exact >= reds on that forever while nothing is
	// wrong. A guard that reds on a micron of rounding noise is a guard people
	// learn to ignore, which is worse than no guard.
	//
	// The disagreement the clause exists to catch -- an inversion that does not
	// actually invert the shipped clamp -- is CENTIMETRES wide; its paired clause
	// probes exactly that with a 5 cm step. 1e-4 m is 0.1 mm: ~100x the observed
	// rounding noise and ~500x smaller than the smallest real disagreement, so it
	// removes the noise without spending any of the property.
	constexpr float fLAB_ARM_ROUNDTRIP_EPSILON = 1.0e-4f;

	// An authored NPC's own AABB half-width, spelled from the body contract.
	constexpr float fLAB_NPC_HALF_WIDTH = fZM_HUMAN_BODY_FOOTPRINT * 0.5f;

	// How finely the lab routes are walked when measuring clearance against the
	// building. Four times finer than the camera-clearance table's 1 m route
	// spacing, so this unit cannot step over the crossing the entrance plane was
	// derived from.
	constexpr float fLAB_ROUTE_WALK_STEP = 0.25f;

	// ---- The measured table's guards ----------------------------------------

	// The band every lab ground sample must land in, as a half-width around the
	// Dawnmere recipe's TARGET HEIGHT -- the height its pads flatten toward. It is
	// deliberately NOT stated around any measured value in the table, because a
	// table pasted wholesale from the wrong run would then validate itself.
	// Calibration, from the two shipped Dawnmere tables: every measured Dawnmere
	// surface on a flattened pad sits 1.5 - 2.6 m ABOVE the 24 m target (the town
	// centre reads 25.99, the ten Home columns 25.59 - 26.54), because the
	// hydraulic erosion pass deposits. +/- 4 m clears all of them by >= 1.4 m
	// while excluding the recipe's landforms (34 - 46 m), an un-flattened field,
	// and every degenerate default (0, the placeholder, a Y from another scene).
	constexpr float fLAB_GROUND_BAND_HALF_WIDTH = 4.0f;

	// The minimum spread across the ten lab rows: a floor on "these are ten
	// independent measurements", not a claim about the terrain.
	constexpr float fLAB_MIN_GROUND_SPREAD = 0.05f;

	// ...and the maximum. The shipped Home table spreads 0.95 m over a comparable
	// 17 x 13 m footprint on a 36 m-radius pad; the lab sits on a 48 m-radius pad
	// with the same four flatten passes. 3 m is beyond any graded pad.
	// ★ IF A REAL MEASUREMENT EXCEEDS THIS, THE FINDING IS THAT THE RESERVED SITE
	// IS NOT FLAT AND THE PLACEMENT NEEDS RE-DERIVING -- not that the cap should be
	// raised.
	constexpr float fLAB_MAX_GROUND_SPREAD = 3.0f;

	// How many of the ten rows must be distinct values, and how many coincidences
	// are tolerated. Every row has its own XZ and eroded terrain is not flat, so
	// ties should not happen -- but two rows landing on the same height to within
	// the epsilon is legitimate content on a flattened pad, and a knife-edge
	// all-pairs-distinct clause would red on that rather than on a defect.
	constexpr u_int uLAB_MIN_DISTINCT_ROWS = 8u;
	constexpr u_int uLAB_MAX_TIED_PAIRS = 2u;

	// ---- Reserved-site lookups ----------------------------------------------
	// BY NAME, never by index: a recipe edit that reorders the pads must move
	// these with it rather than silently re-point them at another site.

	const ZM_TerrainPadSpec* LabFindPad(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
		{
			if (std::strcmp(xRecipe.m_pxPads[u].m_szName, szName) == 0)
			{
				return &xRecipe.m_pxPads[u];
			}
		}
		return nullptr;
	}

	const ZM_TerrainLandmarkSpec* LabFindLandmark(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int u = 0u; u < xRecipe.m_uLandmarkCount; ++u)
		{
			if (std::strcmp(xRecipe.m_pxLandmarks[u].m_szName, szName) == 0)
			{
				return &xRecipe.m_pxLandmarks[u];
			}
		}
		return nullptr;
	}

	const ZM_TerrainPathSpec* LabFindPath(const char* szName)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int u = 0u; u < xRecipe.m_uPathCount; ++u)
		{
			if (std::strcmp(xRecipe.m_pxPaths[u].m_szName, szName) == 0)
			{
				return &xRecipe.m_pxPaths[u];
			}
		}
		return nullptr;
	}

	// ---- Planar geometry -----------------------------------------------------

	float LabPlanarDistance(float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDeltaX = fAX - fBX;
		const float fDeltaZ = fAZ - fBZ;
		return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	}

	// Distance from a point to a SEGMENT (not to its infinite line): a route ends
	// where it ends, and the line would report a body behind the player as close.
	float LabDistanceToSegment(float fPX, float fPZ,
		float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDX = fBX - fAX;
		const float fDZ = fBZ - fAZ;
		const float fLengthSquared = fDX * fDX + fDZ * fDZ;
		if (!(fLengthSquared > 0.0f))
		{
			return LabPlanarDistance(fPX, fPZ, fAX, fAZ);
		}
		float fT = ((fPX - fAX) * fDX + (fPZ - fAZ) * fDZ) / fLengthSquared;
		fT = fT < 0.0f ? 0.0f : (fT > 1.0f ? 1.0f : fT);
		return LabPlanarDistance(fPX, fPZ, fAX + fDX * fT, fAZ + fDZ * fT);
	}

	// ---- The shipped camera, read rather than re-derived ---------------------

	// The authored pivot->camera distance: the arm's horizontal reach and the lift
	// from the pivot to the camera height, both from ZM_FollowCamera's own statics.
	float LabDesiredArm()
	{
		const float fHorizontal = ZM_FollowCamera::GetArmLength();
		const float fVertical =
			ZM_FollowCamera::GetCameraHeight() - ZM_FollowCamera::GetPivotHeight();
		return std::sqrt(fHorizontal * fHorizontal + fVertical * fVertical);
	}

	// The smallest HORIZONTAL gap between a standing player and a solid face that
	// still satisfies the contract. The ray rises as it goes back, so a horizontal
	// gap g is (desired / arm) * g of ray; the clamp subtracts the collision
	// padding from whatever it hits.
	//
	// ★ THE FACE'S HEIGHT DOES NOT ENTER THIS, WHICH IS WHY A SLOPE CANNOT INVALIDATE
	// IT. A blockout wall stands metres above its own ground (the lab shell 5.45 m,
	// the Home's 3.95 m) and the ray has risen under 1.7 m by the time it reaches
	// these distances, so ground relief between the sample and the wall moves where
	// the ray hits, never how far along it does.
	float LabRequiredHorizontalGap()
	{
		const float fDesired = LabDesiredArm();
		const float fRequiredHit = fDesired * fLAB_MIN_ARM_FRACTION
			+ ZM_FollowCamera::GetCollisionPadding();
		return fRequiredHit * (ZM_FollowCamera::GetArmLength() / fDesired);
	}

	// One lab route, for the sweeps below.
	struct LabRouteSegment
	{
		const char* m_szName;
		float m_fAX;
		float m_fAZ;
		float m_fBX;
		float m_fBZ;
	};
}

// The lab facade is the visual exterior for the separately loaded ProfLab scene,
// exactly as the Home facade is for PlayerHome. The two must share one real-world
// scale: the exterior may round the interior's wall envelope up to a clean
// blockout metre, but it must never become a placeholder box with an unrelated
// doorway. Unlike the Home's, this exterior DERIVES its aperture from the
// interior's constants -- so those clauses are structural rather than a
// coincidence to be re-checked, and what is actually load-bearing here is the
// ENVELOPE arithmetic and the frame-inside-the-facade relief allowance.
ZENITH_TEST(ZM_Interaction, LabExterior_EnvelopeAndEntranceMatchProfLabContract)
{
	const ZM_DawnmereBlockout xShell = ZM_GetDawnmereLabShell();
	const ZM_DawnmereBlockout xLeftJamb = ZM_GetDawnmereLabDoorLeft();
	const ZM_DawnmereBlockout xRightJamb = ZM_GetDawnmereLabDoorRight();
	const ZM_DawnmereBlockout xLintel = ZM_GetDawnmereLabDoorLintel();
	const ZM_DawnmereBlockout xTrigger = ZM_GetDawnmereLabDoorTrigger();

	const float fInteriorOuterWidth =
		fZM_PROFLAB_HALF_WIDTH * 2.0f + fZM_PROFLAB_WALL_THICKNESS;
	const float fInteriorOuterDepth =
		fZM_PROFLAB_HALF_DEPTH * 2.0f + fZM_PROFLAB_WALL_THICKNESS;

	// (a) Rounded up, never rounded down, and by less than one cosmetic metre.
	ZENITH_ASSERT_GE(xShell.m_xScale.x, fInteriorOuterWidth,
		"the Dawnmere lab facade is %.2f m wide but ProfLab's outer wall envelope "
		"is %.2f m -- the exterior no longer contains the interior",
		xShell.m_xScale.x, fInteriorOuterWidth);
	ZENITH_ASSERT_LT(xShell.m_xScale.x - fInteriorOuterWidth, 1.0f,
		"the lab facade has %.2f m of unexplained width beyond ProfLab's %.2f m envelope",
		xShell.m_xScale.x - fInteriorOuterWidth, fInteriorOuterWidth);
	ZENITH_ASSERT_GE(xShell.m_xScale.z, fInteriorOuterDepth,
		"the Dawnmere lab facade is %.2f m deep but ProfLab's outer wall envelope "
		"is %.2f m", xShell.m_xScale.z, fInteriorOuterDepth);
	ZENITH_ASSERT_LT(xShell.m_xScale.z - fInteriorOuterDepth, 1.0f,
		"the lab facade has %.2f m of unexplained depth beyond ProfLab's %.2f m envelope",
		xShell.m_xScale.z - fInteriorOuterDepth, fInteriorOuterDepth);
	ZENITH_ASSERT_GE(xShell.m_xScale.y, fZM_PROFLAB_WALL_HEIGHT,
		"the lab facade is lower than ProfLab's %.2f m walls",
		fZM_PROFLAB_WALL_HEIGHT);

	// (b) IT IS DEEPER THAN THE HOME AND WIDER THAN THE HOME, and that is content,
	// not decoration: two exteriors that were the same box would make "the player
	// walked into the wrong building" a bug no test could name.
	ZENITH_ASSERT_GT(xShell.m_xScale.x, fZM_DAWNMERE_HOME_SHELL_SCALE_X,
		"the lab facade is no wider than the Home's -- the two greybox exteriors "
		"have collapsed onto one shape");
	ZENITH_ASSERT_GT(xShell.m_xScale.z, fZM_DAWNMERE_HOME_SHELL_SCALE_Z,
		"the lab facade is no deeper than the Home's");

	// (c) THE ENTRANCE IS THE -Z FACE. ZM_FollowCamera trails toward -Z at the
	// authored yaw, so an entrance on any other face parks the whole shell behind
	// the player at the doorway -- the ZM-D-173 defect, restated for a new building.
	ZENITH_ASSERT_EQ_FLOAT(xShell.Min().z, fZM_DAWNMERE_LAB_ENTRANCE_Z, 0.0f,
		"the lab entrance no longer lies on the facade's -Z face");
	ZENITH_ASSERT_LT(fZM_DAWNMERE_FROM_LAB_SPAWN_Z, fZM_DAWNMERE_LAB_ENTRANCE_Z,
		"the FromLab arrival marker is no longer OUTSIDE the entrance face");
	ZENITH_ASSERT_LT(fZM_DAWNMERE_LAB_TRIGGER_Z, fZM_DAWNMERE_LAB_ENTRANCE_Z,
		"the lab warp sensor is no longer in front of the entrance face");
	// ★ AND THE ARRIVAL POINT IS CLEAR OF THE SENSOR BY A WHOLE BODY RADIUS, which
	// is the clause that stops a WARP LOOP rather than a clipped camera. A player
	// stepping out of the lab is PUT DOWN on this marker; if their capsule still
	// overlapped the trigger volume, the exit would immediately send them back in
	// and the only way out would be to close the game.
	ZENITH_ASSERT_LT(
		fZM_DAWNMERE_FROM_LAB_SPAWN_Z + fZM_DAWNMERE_PLAYER_RADIUS,
		xTrigger.Min().z,
		"a player standing on the FromLab arrival marker (z %.3f, radius %.3f) "
		"overlaps the lab warp sensor, whose near face is at z %.3f -- leaving the "
		"lab would re-enter it forever",
		fZM_DAWNMERE_FROM_LAB_SPAWN_Z, fZM_DAWNMERE_PLAYER_RADIUS,
		xTrigger.Min().z);

	// ...and the sensor is not COPLANAR with the wall it guards: it must be
	// overlapped before physical contact.
	ZENITH_ASSERT_LT(xTrigger.Max().z, fZM_DAWNMERE_LAB_ENTRANCE_Z,
		"the lab warp sensor now touches or crosses the solid entrance face");

	// The staging waypoint is on the outside of the sensor too, so a traversal
	// test that drives staging -> target crosses INTO the trigger rather than
	// starting inside it.
	ZENITH_ASSERT_LT(fZM_DAWNMERE_LAB_DOOR_STAGING_Z, xTrigger.Min().z,
		"the lab drive staging waypoint is already inside the warp sensor, so the "
		"approach it stages cannot be observed");
	ZENITH_ASSERT_GT(fZM_DAWNMERE_LAB_DOOR_TARGET_Z, xTrigger.Min().z,
		"the lab drive target no longer lands inside the warp sensor");
	ZENITH_ASSERT_LT(fZM_DAWNMERE_LAB_DOOR_TARGET_Z, xTrigger.Max().z,
		"the lab drive target no longer lands inside the warp sensor");

	// (d) THE APERTURE IS PROFLAB'S, EXACTLY. Derived rather than re-spelled, so
	// this is a structural claim -- but it is asserted anyway, because the
	// derivation is what a future "tidy-up" would replace with literals.
	const float fExteriorApertureWidth = xRightJamb.Min().x - xLeftJamb.Max().x;
	ZENITH_ASSERT_EQ_FLOAT(fExteriorApertureWidth,
		fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f, 0.0f,
		"the lab frame width no longer matches ProfLab's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xLeftJamb.m_xScale.y, fZM_PROFLAB_APERTURE_HEIGHT, 0.0f,
		"the lab jamb height no longer matches ProfLab's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xRightJamb.m_xScale.y, fZM_PROFLAB_APERTURE_HEIGHT, 0.0f,
		"the lab jamb height no longer matches ProfLab's entrance aperture");
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.m_xScale.x, fExteriorApertureWidth, 0.0f,
		"the lab portal sensor no longer covers exactly the visible entrance");
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.m_xScale.y, fZM_PROFLAB_APERTURE_HEIGHT, 0.0f,
		"the lab portal sensor no longer matches the visible entrance height");

	// The opening has to fit a body through it with room either side, or the
	// aperture equality above would be satisfied by a walled-up doorway.
	ZENITH_ASSERT_GT(fExteriorApertureWidth, fZM_HUMAN_BODY_FOOTPRINT * 2.0f,
		"the lab doorway is under two body-widths wide");
	ZENITH_ASSERT_GT(fZM_PROFLAB_APERTURE_HEIGHT, fZM_HUMAN_BODY_HEIGHT,
		"the lab doorway is shorter than the body that has to walk through it");

	// (e) The lintel spans BOTH jambs' outer faces and sits on top of them.
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Min().x, xLeftJamb.Min().x, fLAB_EXACT_EPSILON,
		"the lab lintel no longer reaches the left jamb's outer face");
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Max().x, xRightJamb.Max().x, fLAB_EXACT_EPSILON,
		"the lab lintel no longer reaches the right jamb's outer face");
	ZENITH_ASSERT_EQ_FLOAT(xLintel.m_xCenter.z, fZM_DAWNMERE_LAB_ENTRANCE_Z, 0.0f,
		"the lab lintel has left the entrance plane");

	// The whole entrance frame stands INSIDE the facade mass. Both jambs sit on
	// the DOOR ground while the shell is seated on the LOWEST of its four corner
	// grounds, so this is the clause the site's ground relief spends.
	// ★ IF THIS REDS AFTER THE GROUND TABLE IS FROZEN, RAISE
	// fZM_DAWNMERE_LAB_SHELL_SCALE_Y -- do NOT relax the clause. A red here means
	// the lintel would visibly poke through the roof of the authored building.
	// ★ AND THAT HAS ALREADY HAPPENED ONCE, WHICH IS THE EVIDENCE THIS CLAUSE
	// WORKS. Freezing the ten measurements put the site's relief at 1.4048 m, the
	// 4.5 m facade left the lintel 0.4548 m proud, and the fix was 4.5 -> 5.5, not
	// a softened comparison. The derivation is in ZM_DawnmerePlacement.h beside the
	// constant.
	ZENITH_ASSERT_GT(xShell.Max().y, xLintel.Max().y,
		"the lab entrance frame stands PROUD of the facade: lintel top %.4f vs "
		"roofline %.4f. The frame is measured from the door ground and the box from "
		"its lowest corner, so the site's relief has eaten the facade's height "
		"allowance -- raise fZM_DAWNMERE_LAB_SHELL_SCALE_Y",
		xLintel.Max().y, xShell.Max().y);
	ZENITH_ASSERT_GT(xShell.Max().y, xLeftJamb.Max().y,
		"the left lab jamb stands proud of the facade roofline");
	ZENITH_ASSERT_GT(xShell.Max().y, xRightJamb.Max().y,
		"the right lab jamb stands proud of the facade roofline");
}

// THE SITE IS RESERVED IN THE TERRAIN RECIPE, AND THIS IS WHERE THE TWO ARE TIED
// TOGETHER. Every coordinate in the lab block was chosen to fit inside the "Lab"
// pad and to stand on the "FromLab" landmark; both are read from the compiled
// recipe here rather than re-spelled, so moving the pad (which regenerates the
// WHOLE Dawnmere heightmap) reds this unit instead of silently leaving a building
// on ungraded ground.
ZENITH_TEST(ZM_Interaction, LabPlacement_SitsInsideTheReservedPadAndOnItsLandmark)
{
	const ZM_TerrainPadSpec* pxPad = LabFindPad("Lab");
	ZENITH_ASSERT_NOT_NULL(pxPad,
		"the Dawnmere terrain recipe no longer reserves a pad named 'Lab' -- the "
		"lab placement is standing on whatever the landform pass left behind");
	const ZM_TerrainLandmarkSpec* pxLandmark = LabFindLandmark("FromLab");
	ZENITH_ASSERT_NOT_NULL(pxLandmark,
		"the Dawnmere terrain recipe no longer carries the 'FromLab' landmark");
	if (pxPad == nullptr || pxLandmark == nullptr)
	{
		return;
	}

	// (a) THE MIRRORS. ZM_DawnmerePlacement.h is pure and cannot include terrain
	// authoring, so it MIRRORS the pad centre and the landmark. This is what makes
	// the mirror safe.
	ZENITH_ASSERT_EQ_FLOAT(pxPad->m_xCentre.m_fX, fZM_DAWNMERE_LAB_X, 0.0f,
		"fZM_DAWNMERE_LAB_X no longer mirrors the reserved 'Lab' pad centre X");
	ZENITH_ASSERT_EQ_FLOAT(pxPad->m_xCentre.m_fZ, fZM_DAWNMERE_LAB_PAD_CENTER_Z, 0.0f,
		"fZM_DAWNMERE_LAB_PAD_CENTER_Z no longer mirrors the reserved 'Lab' pad "
		"centre Z");
	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fX, fZM_DAWNMERE_LAB_X, 0.0f,
		"the FromLab arrival marker is no longer on the lab's X centreline");
	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fZ,
		fZM_DAWNMERE_FROM_LAB_SPAWN_Z, 0.0f,
		"the FromLab arrival marker no longer stands on the recipe's 'FromLab' "
		"landmark -- the terrain says the return spawn is somewhere else");

	// (b) THE WHOLE SITE IS ON PAVED, GRADED GROUND. The sample table enumerates
	// exactly the columns the placement occupies, so walking it walks the site.
	ZENITH_ASSERT_GT(pxPad->m_fFlattenRadius, pxPad->m_fDirtRadius,
		"the reserved Lab pad paints dirt beyond the ground it flattens");
	const u_int uCount = ZM_GetDawnmereLabSampleCount();
	ZENITH_ASSERT_EQ(uCount, (u_int)ZM_DAWNMERE_LAB_SAMPLE_COUNT,
		"the lab sample accessor disagrees with its own enum");
	float fFurthest = -1.0f;
	const char* szFurthest = "<none>";
	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
		const float fDistance = LabPlanarDistance(xSample.m_fX, xSample.m_fZ,
			pxPad->m_xCentre.m_fX, pxPad->m_xCentre.m_fZ);
		if (fDistance > fFurthest)
		{
			fFurthest = fDistance;
			szFurthest = xSample.m_szEntityName;
		}
		ZENITH_ASSERT_LT(fDistance, pxPad->m_fDirtRadius,
			"lab column '%s' is %.3f m from the reserved pad centre, outside its "
			"%.1f m dirt radius -- that part of the site is neither paved nor "
			"reliably graded, so a measured height there is a hillside, not a pad",
			xSample.m_szEntityName, fDistance, pxPad->m_fDirtRadius);
	}
	// ANTI-VACUITY: the loop really walked a site rather than a table of one point.
	ZENITH_ASSERT_GT(fFurthest, 10.0f,
		"the furthest lab column ('%s') is only %.3f m from the pad centre -- the "
		"sample table has collapsed onto a single column", szFurthest, fFurthest);
}

// ★ THE UNIT THAT EXISTS BECAUSE OF A DEFECT SC-E WOULD OTHERWISE HAVE SHIPPED.
// The authored Lab dirt path runs past the building's BACK face, and the follow
// camera trails 5.5 m toward -Z from wherever the player stands -- straight into
// that face. The first draft of this placement put the entrance at 528, which
// left 2.864 m of gap against the ~2.933 m the arm contract needs: a violation
// that would have appeared only once SC-E authored the shell, in
// ZM_DawnmereCameraClearance_Test, naming a route rather than a building. The
// entrance is at 527 instead, and this unit is why that number cannot drift back.
//
// It runs the SHIPPED ZM_FollowCamera::ClampArmDistance rather than re-deriving
// the clamp, so a change to the arm, the camera height, the pivot or the
// collision padding moves this claim automatically.
ZENITH_TEST(ZM_Interaction, LabDirtPath_ClearsTheShellByTheShippedCameraClamp)
{
	const ZM_TerrainPathSpec* pxPath = LabFindPath("Lab");
	ZENITH_ASSERT_NOT_NULL(pxPath,
		"the Dawnmere terrain recipe no longer carries a path named 'Lab'");
	if (pxPath == nullptr || pxPath->m_uPointCount < 2u)
	{
		return;
	}

	const ZM_DawnmereBlockout xShell = ZM_GetDawnmereLabShell();
	const float fShellMinX = xShell.Min().x;
	const float fShellMaxX = xShell.Max().x;
	const float fShellMinZ = xShell.Min().z;
	const float fShellMaxZ = xShell.Max().z;
	const float fRequiredGap = LabRequiredHorizontalGap();

	// The inversion above is only trustworthy if the shipped clamp agrees with it.
	// At exactly the required gap the clamp must still satisfy the contract, and a
	// hair under it must not -- which also proves this unit can red at all.
	//
	// ★ THE FIRST CLAUSE CARRIES A ROUNDING EPSILON AND THE SECOND DOES NOT, AND
	// THAT ASYMMETRY IS THE POINT. The first compares the contract minimum against
	// ITS OWN round-tripped inversion (multiplied by arm/desired inside
	// LabRequiredHorizontalGap, then by desired/arm again here), which is exact in
	// real arithmetic and ~1e-6 low in floats; see fLAB_ARM_ROUNDTRIP_EPSILON for
	// the measurement and the sizing. The second stands 5 cm off the boundary, so
	// no rounding can reach it and any slack there would genuinely weaken it.
	const float fDesiredArm = LabDesiredArm();
	const float fArmRatio = fDesiredArm / ZM_FollowCamera::GetArmLength();
	ZENITH_ASSERT_GE(
		ZM_FollowCamera::ClampArmDistance(fDesiredArm, true, fRequiredGap * fArmRatio),
		fDesiredArm * fLAB_MIN_ARM_FRACTION - fLAB_ARM_ROUNDTRIP_EPSILON,
		"the required-gap inversion disagrees with ZM_FollowCamera::ClampArmDistance "
		"at the boundary -- every margin below is measured against the wrong number");
	ZENITH_ASSERT_LT(
		ZM_FollowCamera::ClampArmDistance(
			fDesiredArm, true, (fRequiredGap - 0.05f) * fArmRatio),
		fDesiredArm * fLAB_MIN_ARM_FRACTION,
		"a gap 5 cm INSIDE the required one still satisfies the shipped clamp, so "
		"this unit cannot detect a building standing on the walkway");

	float fWorstGap = -1.0f;
	float fWorstX = 0.0f;
	float fWorstZ = 0.0f;
	u_int uPointsInBand = 0u;
	u_int uPointsInsideFootprint = 0u;
	float fInsideX = 0.0f;
	float fInsideZ = 0.0f;
	for (u_int uSegment = 0u; uSegment + 1u < pxPath->m_uPointCount; ++uSegment)
	{
		const ZM_TerrainPoint2& xA = pxPath->m_pxPoints[uSegment];
		const ZM_TerrainPoint2& xB = pxPath->m_pxPoints[uSegment + 1u];
		const float fLength =
			LabPlanarDistance(xA.m_fX, xA.m_fZ, xB.m_fX, xB.m_fZ);
		u_int uSteps = (u_int)std::ceil(fLength / fLAB_ROUTE_WALK_STEP);
		if (uSteps == 0u)
		{
			uSteps = 1u;
		}
		for (u_int uStep = 0u; uStep <= uSteps; ++uStep)
		{
			const float fT = (float)uStep / (float)uSteps;
			const float fX = xA.m_fX + (xB.m_fX - xA.m_fX) * fT;
			const float fZ = xA.m_fZ + (xB.m_fZ - xA.m_fZ) * fT;

			// A walkway point inside the body-expanded footprint is not a clearance
			// question at all -- it is a route through a wall.
			if (fX > fShellMinX - fZM_DAWNMERE_PLAYER_RADIUS
				&& fX < fShellMaxX + fZM_DAWNMERE_PLAYER_RADIUS
				&& fZ > fShellMinZ - fZM_DAWNMERE_PLAYER_RADIUS
				&& fZ < fShellMaxZ + fZM_DAWNMERE_PLAYER_RADIUS)
			{
				if (uPointsInsideFootprint == 0u)
				{
					fInsideX = fX;
					fInsideZ = fZ;
				}
				++uPointsInsideFootprint;
				continue;
			}

			// Only points NORTH of the building and inside its X band can have the
			// camera ray blocked by it: the ray runs straight along -Z at the
			// player's own X, and anything south of the shell has it in FRONT.
			if (fX >= fShellMinX && fX <= fShellMaxX && fZ > fShellMaxZ)
			{
				++uPointsInBand;
				const float fGap = fZ - fShellMaxZ;
				if (fWorstGap < 0.0f || fGap < fWorstGap)
				{
					fWorstGap = fGap;
					fWorstX = fX;
					fWorstZ = fZ;
				}
			}
		}
	}

	ZENITH_ASSERT_EQ(uPointsInsideFootprint, 0u,
		"the authored Lab walkway passes THROUGH the lab shell (first offending "
		"point (%.3f, %.3f)) -- the building is standing on the path, so that "
		"stretch is not walkable at all", fInsideX, fInsideZ);

	// The crossing this unit exists for. If it has gone, the entrance plane's
	// derivation in ZM_DawnmerePlacement.h is stale rather than merely satisfied.
	ZENITH_ASSERT_GT(uPointsInBand, 0u,
		"the authored Lab walkway no longer passes north of the lab shell's X band, "
		"so this clearance claim is vacuous -- the entrance plane was derived from "
		"that crossing, and it can now be re-derived");

	if (uPointsInBand == 0u)
	{
		return;
	}

	const float fWorstHitDistance = fWorstGap * fArmRatio;
	const float fClampedArm =
		ZM_FollowCamera::ClampArmDistance(fDesiredArm, true, fWorstHitDistance);
	ZENITH_ASSERT_GE(fClampedArm, fDesiredArm * fLAB_MIN_ARM_FRACTION,
		"a player walking the authored Lab path at (%.3f, %.3f) stands %.4f m from "
		"the lab shell's +Z face, which clamps the %.4f m camera arm to %.4f m -- "
		"under the %.4f m the ZM-D-173 contract requires. Move the ENTRANCE PLANE "
		"(and with it the shell) south, or the building will clip the camera on a "
		"route no test mentions",
		fWorstX, fWorstZ, fWorstGap, fDesiredArm, fClampedArm,
		fDesiredArm * fLAB_MIN_ARM_FRACTION);
}

// The lab approach routes run across the town square, and the square is full of
// authored bodies -- including the ONE dynamic patrol in the game. A route that
// passes close enough to an NPC for its capsule to enter the camera ray would
// make the (static-layout) clearance guard depend on where a walking NPC happens
// to be, which is exactly the nondeterminism ZM_AutoTests_CameraClearance
// deliberately keeps out of its table. This unit is what says that cannot happen.
ZENITH_TEST(ZM_Interaction, LabApproach_ClearsEveryAuthoredNpcAndPatrolEndpoint)
{
	// A body blocks the ray when it comes within the required horizontal gap PLUS
	// its own half-width. Conservative by construction: it forbids a body in a DISC
	// around the route, whereas the ray only sweeps the -Z side of it.
	const float fRequired = LabRequiredHorizontalGap() + fLAB_NPC_HALF_WIDTH;

	// The same routes ZM_DawnmereCameraClearance_Test samples for the lab: the
	// blind drive leg, the doorway approach, and every segment of the authored
	// walkway (read from the recipe, not re-typed).
	constexpr u_int uROUTE_CAPACITY = 8u;
	LabRouteSegment axRoutes[uROUTE_CAPACITY] = {};
	u_int uRouteCount = 0u;
	axRoutes[uRouteCount++] = { "townCentre->labStaging",
		fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
		fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_STAGING_Z };
	axRoutes[uRouteCount++] = { "labStaging->labTrigger",
		fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_STAGING_Z,
		fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_TARGET_Z };

	const ZM_TerrainPathSpec* pxPath = LabFindPath("Lab");
	ZENITH_ASSERT_NOT_NULL(pxPath,
		"the Dawnmere terrain recipe no longer carries a path named 'Lab', so the "
		"walkway half of this sweep would be silently empty");
	bool bPathFitted = true;
	if (pxPath != nullptr)
	{
		for (u_int u = 0u; u + 1u < pxPath->m_uPointCount; ++u)
		{
			if (uRouteCount >= uROUTE_CAPACITY)
			{
				bPathFitted = false;
				break;
			}
			axRoutes[uRouteCount++] = { "labDirtPath",
				pxPath->m_pxPoints[u].m_fX, pxPath->m_pxPoints[u].m_fZ,
				pxPath->m_pxPoints[u + 1u].m_fX, pxPath->m_pxPoints[u + 1u].m_fZ };
		}
	}
	ZENITH_ASSERT_TRUE(bPathFitted,
		"the authored Lab walkway has more segments than this sweep's %u-route "
		"budget -- coverage would be TRUNCATED rather than reported", uROUTE_CAPACITY);
	ZENITH_ASSERT_GT(uRouteCount, 2u,
		"only the two derived approach legs were swept -- the authored walkway "
		"contributed nothing, so half this claim is vacuous");

	float fClosest = -1.0f;
	const char* szClosestRoute = "<none>";
	const char* szClosestBody = "<none>";
	u_int uPairsMeasured = 0u;

	for (u_int uRoute = 0u; uRoute < uRouteCount; ++uRoute)
	{
		const LabRouteSegment& xRoute = axRoutes[uRoute];
		for (u_int uBody = 0u;
			uBody < (u_int)ZM_DAWNMERE_NPC_COUNT
				+ ZM_GetDawnmereWanderWaypointCount(); ++uBody)
		{
			// The roster first, then both patrol endpoints -- the wanderer is not
			// where its anchor says at any given instant, so the endpoints are the
			// positions that actually have to clear.
			const ZM_DawnmereNpcAnchor& xBody =
				uBody < (u_int)ZM_DAWNMERE_NPC_COUNT
					? ZM_GetDawnmereNpcAnchor(uBody)
					: ZM_GetDawnmereWanderWaypoint(
						uBody - (u_int)ZM_DAWNMERE_NPC_COUNT);
			const float fDistance = LabDistanceToSegment(xBody.m_fX, xBody.m_fZ,
				xRoute.m_fAX, xRoute.m_fAZ, xRoute.m_fBX, xRoute.m_fBZ);
			++uPairsMeasured;
			if (fClosest < 0.0f || fDistance < fClosest)
			{
				fClosest = fDistance;
				szClosestRoute = xRoute.m_szName;
				szClosestBody = xBody.m_szEntityName;
			}
			ZENITH_ASSERT_GT(fDistance, fRequired,
				"'%s' stands %.3f m from the lab route '%s', inside the %.3f m a body "
				"needs to keep clear of the camera ray -- the lab approach would "
				"either clip the camera or, for the wanderer, do so only sometimes",
				xBody.m_szEntityName, fDistance, xRoute.m_szName, fRequired);
		}
	}

	// ANTI-VACUITY: a run in which the roster loop never executed would satisfy
	// every per-body clause by never evaluating one.
	ZENITH_ASSERT_EQ(uPairsMeasured,
		uRouteCount * ((u_int)ZM_DAWNMERE_NPC_COUNT
			+ ZM_GetDawnmereWanderWaypointCount()),
		"the lab-route clearance sweep did not visit every route/body pair");
	ZENITH_ASSERT_GT(fClosest, 0.0f,
		"no authored body was measured against the lab routes at all (nearest "
		"reported as '%s' on '%s')", szClosestBody, szClosestRoute);
}

// ============================================================================
// THE MEASURED TABLE. Both units below are RED until the SC-D freeze lands, and
// their messages say so -- that is the point, not an accident.
// ============================================================================

// The table must be REAL MEASUREMENTS from the graded pad: not the shipped
// placeholder, not one value stamped ten times, and not heights from somewhere
// else in the world.
ZENITH_TEST(ZM_Interaction, LabGroundSamples_AreTenMeasurementsInsideTheGradedBand)
{
	const u_int uCount = ZM_GetDawnmereLabSampleCount();
	ZENITH_ASSERT_EQ(uCount, 10u,
		"the lab ground table no longer has the ten columns the placement needs");

	// (a) THE FREEZE TRIPWIRE. This is the clause that says, in CI, that the slice
	// has not finished.
	u_int uStillPlaceholder = 0u;
	for (u_int u = 0u; u < uCount; ++u)
	{
		if (std::fabs(ZM_DawnmereLabSampleFeetY(u)
			- fZM_DAWNMERE_LAB_GROUND_UNMEASURED) <= fLAB_EXACT_EPSILON)
		{
			++uStillPlaceholder;
		}
	}
	ZENITH_ASSERT_EQ(uStillPlaceholder, 0u,
		"%u of the %u lab ground rows still hold fZM_DAWNMERE_LAB_GROUND_UNMEASURED "
		"(%.1f), which is not a height. The S8 SC-D measure -> freeze -> rebuild loop "
		"has not closed: run ZM_DawnmereLabGroundTruth_Test on a WINDOWED tools boot "
		"with a warm Dawnmere terrain bake, paste its `paste=` literals into the S8 "
		"SC-D LAB GROUND block in Source/World/ZM_DawnmerePlacement.cpp, and rebuild",
		uStillPlaceholder, uCount, fZM_DAWNMERE_LAB_GROUND_UNMEASURED);

	// (b) THE BAND. Stated against the terrain recipe's flatten TARGET, which is
	// independent of anything in this table, so a table pasted from the wrong run
	// or the wrong scene cannot validate itself.
	const float fTarget = ZM_GetDawnmereTerrainRecipe().m_fTargetHeight;
	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
		ZENITH_ASSERT_LT(std::fabs(xSample.m_fFeetY - fTarget),
			fLAB_GROUND_BAND_HALF_WIDTH,
			"lab ground row '%s' reads %.5f, which is %.5f m from the Dawnmere "
			"recipe's %.2f m flatten target -- outside the +/-%.1f m band a graded "
			"pad can produce. It is a placeholder, a height from another part of the "
			"world, or the pad is no longer flattened",
			xSample.m_szEntityName, xSample.m_fFeetY,
			std::fabs(xSample.m_fFeetY - fTarget), fTarget,
			fLAB_GROUND_BAND_HALF_WIDTH);
	}

	// (c) THE SPREAD, both ends. Too little means the rows collapsed onto one
	// value; too much means the "graded pad" premise is wrong.
	float fMin = ZM_DawnmereLabSampleFeetY(0u);
	float fMax = fMin;
	for (u_int u = 1u; u < uCount; ++u)
	{
		const float fFeet = ZM_DawnmereLabSampleFeetY(u);
		if (fFeet < fMin) { fMin = fFeet; }
		if (fFeet > fMax) { fMax = fFeet; }
	}
	const float fSpread = fMax - fMin;
	ZENITH_ASSERT_GE(fSpread, fLAB_MIN_GROUND_SPREAD,
		"the ten lab ground rows span only %.5f m (min %.5f, max %.5f) -- they are "
		"ONE shared value, which is either the unfrozen placeholder or a single "
		"measurement pasted into every row",
		fSpread, fMin, fMax);
	ZENITH_ASSERT_LT(fSpread, fLAB_MAX_GROUND_SPREAD,
		"the ten lab ground rows span %.5f m, more than a four-pass flattened pad "
		"can produce (the shipped Home table spans 0.95 m over a comparable "
		"footprint) -- at least one row is a mis-pasted measurement, or the reserved "
		"site is not the graded ground this placement assumes",
		fSpread);
}

// ...and they must be TEN measurements rather than three copied around. This is
// the clause a plausible copy-paste survives the band and the spread with.
ZENITH_TEST(ZM_Interaction, LabGroundSamples_NoRowSilentlyRepeatsAnother)
{
	const u_int uCount = ZM_GetDawnmereLabSampleCount();

	u_int uDistinct = 0u;
	for (u_int uA = 0u; uA < uCount; ++uA)
	{
		bool bSeenEarlier = false;
		for (u_int uB = 0u; uB < uA; ++uB)
		{
			if (std::fabs(ZM_DawnmereLabSampleFeetY(uA)
				- ZM_DawnmereLabSampleFeetY(uB)) <= fLAB_EXACT_EPSILON)
			{
				bSeenEarlier = true;
				break;
			}
		}
		if (!bSeenEarlier)
		{
			++uDistinct;
		}
	}

	u_int uTiedPairs = 0u;
	const char* szTiedA = "<none>";
	const char* szTiedB = "<none>";
	for (u_int uA = 0u; uA < uCount; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uCount; ++uB)
		{
			if (std::fabs(ZM_DawnmereLabSampleFeetY(uA)
				- ZM_DawnmereLabSampleFeetY(uB)) <= fLAB_EXACT_EPSILON)
			{
				if (uTiedPairs == 0u)
				{
					szTiedA = ZM_GetDawnmereLabSample(uA).m_szEntityName;
					szTiedB = ZM_GetDawnmereLabSample(uB).m_szEntityName;
				}
				++uTiedPairs;
			}
		}
	}

	ZENITH_ASSERT_GE(uDistinct, uLAB_MIN_DISTINCT_ROWS,
		"only %u of the %u lab ground rows carry a value of their own (first tie "
		"'%s' == '%s') -- ten columns on eroded terrain do not share heights, so "
		"this is a copy-paste rather than a measurement",
		uDistinct, uCount, szTiedA, szTiedB);
	ZENITH_ASSERT_LE(uTiedPairs, uLAB_MAX_TIED_PAIRS,
		"%u pairs of lab ground rows hold the same height (first '%s' == '%s')",
		uTiedPairs, szTiedA, szTiedB);

	// ★ AND THE ROW A MIS-PASTE HURTS MOST GETS ITS OWN CLAUSE. The FromLab
	// arrival marker's feet decide where a warping player is PUT (the warp adds a
	// capsule half-extent to it), so a neighbour's height there spawns the player
	// embedded in the ground or dropping out of the air, in a scene nothing else
	// measures. It shares an XZ with no other row and has no geometric reason to
	// equal one.
	//
	// ★ WHAT THIS CANNOT SEE, SAID PLAINLY: a value that is wrong by centimetres
	// is still "different from every other row". Only
	// ZM_DawnmereLabGroundTruth_Test, which casts a real ray at this exact column,
	// can catch that -- and it SKIPS without a terrain bake. This clause bounds the
	// damage; it does not eliminate it.
	const float fSpawnFeet =
		ZM_DawnmereLabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_SPAWN);
	for (u_int u = 0u; u < uCount; ++u)
	{
		if (u == (u_int)ZM_DAWNMERE_LAB_SAMPLE_SPAWN)
		{
			continue;
		}
		ZENITH_ASSERT_GT(
			std::fabs(fSpawnFeet - ZM_DawnmereLabSampleFeetY(u)),
			fLAB_EXACT_EPSILON,
			"the FromLab arrival row holds exactly the same height as '%s'. Those "
			"are different columns on a graded but not flat pad, so this is a "
			"mis-paste -- and it is the one row that decides where a warping player "
			"is put down", ZM_GetDawnmereLabSample(u).m_szEntityName);
	}
}

// The five authored Y values SC-E writes are DERIVED from the measured table by
// fixed formulas spelled once in ZM_DawnmerePlacement.cpp. This unit checks the
// derivations themselves, which no other test can see: the oracle checks the
// TABLE against the terrain, and the automated clearance guard checks the RESULT
// against the physics world, but neither would notice a max() where a min()
// belongs until a corner of the building was visibly hanging in the air.
ZENITH_TEST(ZM_Interaction, LabBlockoutY_FollowsTheFixedDerivationFromTheMeasuredTable)
{
	const ZM_DawnmereBlockout xShell = ZM_GetDawnmereLabShell();
	const ZM_DawnmereBlockout xLeftJamb = ZM_GetDawnmereLabDoorLeft();
	const ZM_DawnmereBlockout xRightJamb = ZM_GetDawnmereLabDoorRight();
	const ZM_DawnmereBlockout xLintel = ZM_GetDawnmereLabDoorLintel();
	const ZM_DawnmereBlockout xTrigger = ZM_GetDawnmereLabDoorTrigger();

	const float fLeftGround =
		ZM_DawnmereLabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_LEFT);
	const float fRightGround =
		ZM_DawnmereLabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_DOOR_RIGHT);
	const float fHigherDoorGround =
		fLeftGround > fRightGround ? fLeftGround : fRightGround;

	// (1) EACH JAMB STANDS ON ITS OWN GROUND. Sharing one height across a 6 m
	// opening is the approximation this table exists to remove.
	ZENITH_ASSERT_EQ_FLOAT(xLeftJamb.Min().y, fLeftGround, fLAB_EXACT_EPSILON,
		"the left lab jamb's foot is not on its own measured ground");
	ZENITH_ASSERT_EQ_FLOAT(xRightJamb.Min().y, fRightGround, fLAB_EXACT_EPSILON,
		"the right lab jamb's foot is not on its own measured ground");

	// (2) THE LINTEL CLEARS BOTH JAMBS, i.e. it is derived off the HIGHER door
	// ground. Derived off the lower one and it intersects the taller jamb.
	ZENITH_ASSERT_EQ_FLOAT(xLintel.Min().y,
		fHigherDoorGround + fZM_PROFLAB_APERTURE_HEIGHT, fLAB_EXACT_EPSILON,
		"the lab lintel's underside is not one aperture height above the HIGHER of "
		"the two measured door grounds -- a lower derivation would bury it in a jamb");
	ZENITH_ASSERT_GE(xLintel.Min().y, xLeftJamb.Max().y - fLAB_EXACT_EPSILON,
		"the lab lintel now overlaps the left jamb");
	ZENITH_ASSERT_GE(xLintel.Min().y, xRightJamb.Max().y - fLAB_EXACT_EPSILON,
		"the lab lintel now overlaps the right jamb");

	// (3) THE SENSOR IS CENTRED ON ITS OWN COLUMN'S GROUND, so a walking capsule
	// passes through its middle rather than under or over it.
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.Min().y,
		ZM_DawnmereLabSampleFeetY(ZM_DAWNMERE_LAB_SAMPLE_TRIGGER),
		fLAB_EXACT_EPSILON,
		"the lab warp sensor's underside is not on its own measured ground");
	ZENITH_ASSERT_GT(xTrigger.m_xScale.y, fZM_HUMAN_BODY_HEIGHT * 0.5f,
		"the lab warp sensor is too shallow to be overlapped by a walking body");

	// (4) THE SHELL IS SEATED ON THE LOWEST CORNER AND EMBEDDED SLIGHTLY BELOW IT.
	// Derived off the maximum and one corner of a 21 x 17 m box hangs in the air;
	// derived with no embed and a hairline gap opens under it on a slope.
	float fLowestCorner = ZM_DawnmereLabSampleFeetY(
		ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MINZ);
	for (u_int u = (u_int)ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MINZ;
		u <= (u_int)ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MAXZ; ++u)
	{
		const float fCorner = ZM_DawnmereLabSampleFeetY(u);
		if (fCorner < fLowestCorner)
		{
			fLowestCorner = fCorner;
		}
	}
	const float fEmbed = fLowestCorner - xShell.Min().y;
	ZENITH_ASSERT_GT(fEmbed, 0.0f,
		"the lab shell's underside sits AT or ABOVE its lowest measured corner "
		"ground (%.5f vs %.5f) -- on a graded but not flat pad that opens a visible "
		"gap under the building", xShell.Min().y, fLowestCorner);
	ZENITH_ASSERT_LT(fEmbed, 0.10f,
		"the lab shell is embedded %.5f m below its lowest corner -- that is a sunken "
		"building rather than a seated one", fEmbed);
	for (u_int u = (u_int)ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MINZ;
		u <= (u_int)ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MAXZ; ++u)
	{
		ZENITH_ASSERT_LE(xShell.Min().y, ZM_DawnmereLabSampleFeetY(u),
			"lab shell corner %u stands above the box's underside, so that corner is "
			"floating -- the seat is derived off the wrong extreme", u);
	}

	// (5) The XZ half of the blockouts, which no measurement can move: every piece
	// shares the lab's X centreline except the two jambs, and both jambs and the
	// lintel stand on the entrance plane.
	ZENITH_ASSERT_EQ_FLOAT(xShell.m_xCenter.x, fZM_DAWNMERE_LAB_X, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xTrigger.m_xCenter.x, fZM_DAWNMERE_LAB_X, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xLintel.m_xCenter.x, fZM_DAWNMERE_LAB_X, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xLeftJamb.m_xCenter.z, fZM_DAWNMERE_LAB_ENTRANCE_Z, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xRightJamb.m_xCenter.z, fZM_DAWNMERE_LAB_ENTRANCE_Z, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(
		xLeftJamb.m_xCenter.x + xRightJamb.m_xCenter.x,
		fZM_DAWNMERE_LAB_X * 2.0f, fLAB_EXACT_EPSILON,
		"the two lab jambs are no longer symmetric about the lab's centreline");
}

// TOTALITY, in this file's house style. The lab accessors are walked with the
// out-of-range ids on purpose, and nothing they call may Zenith_Assert:
// Zenith_Assert breaks in EVERY configuration and the whole unit suite runs at
// boot, so one such assert does not fail a test -- it ends the boot unit run and
// takes the gate down.
//
// NO ZENITH_ASSERT_* MAY APPEAR INSIDE THE CAPTURE SCOPE: while one is active,
// framework failures are swallowed and merely counted, so an in-scope assertion
// could never red this test. Everything is collected into locals and asserted
// after the scope closes, hit count included.
ZENITH_TEST(ZM_Interaction, LabSampleAccessors_AreTotalOnEveryDegenerateId)
{
	const u_int uCount = ZM_GetDawnmereLabSampleCount();
	const u_int auBadIds[] = { uCount, uCount + 1u, 0xffffffffu };
	constexpr u_int uBAD_ID_COUNT =
		(u_int)(sizeof(auBadIds) / sizeof(auBadIds[0]));

	u_int uHits = 0u;
	u_int uRowsFinite = 0u;
	u_int uRowsMistakenForSentinel = 0u;
	u_int uBadIdsGaveSentinel = 0u;
	u_int uBadIdsFiniteFeet = 0u;
	u_int uNameCollisionsWithinLab = 0u;
	u_int uNameCollisionsWithHome = 0u;
	bool bBlockoutsFinite = false;

	{
		Zenith_AssertCaptureScope xCapture;

		for (u_int u = 0u; u < uCount; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(u);
			if (W5AnchorFieldsFinite(xSample)
				&& std::isfinite(ZM_DawnmereLabSampleFeetY(u)))
			{
				++uRowsFinite;
			}
			if (W5IsSentinelAnchor(xSample))
			{
				++uRowsMistakenForSentinel;
			}
			for (u_int uB = u + 1u; uB < uCount; ++uB)
			{
				if (std::strcmp(xSample.m_szEntityName,
					ZM_GetDawnmereLabSample(uB).m_szEntityName) == 0)
				{
					++uNameCollisionsWithinLab;
				}
			}
			// The two ground tables' rows are named in the SAME logs, by two
			// oracles that look alike; a shared name would make a re-measure round
			// paste a Home value into a lab row and never notice.
			for (u_int uHome = 0u; uHome < ZM_GetDawnmereHomeSampleCount(); ++uHome)
			{
				if (std::strcmp(xSample.m_szEntityName,
					ZM_GetDawnmereHomeSample(uHome).m_szEntityName) == 0)
				{
					++uNameCollisionsWithHome;
				}
			}
		}

		for (u_int u = 0u; u < uBAD_ID_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xSample = ZM_GetDawnmereLabSample(auBadIds[u]);
			if (W5IsSentinelAnchor(xSample) && W5AnchorFieldsFinite(xSample))
			{
				++uBadIdsGaveSentinel;
			}
			if (std::isfinite(ZM_DawnmereLabSampleFeetY(auBadIds[u])))
			{
				++uBadIdsFiniteFeet;
			}
		}

		// The derived accessors take no arguments, but they are built from the
		// table, so a degenerate row must not become a non-finite transform.
		const ZM_DawnmereBlockout axBlocks[] = {
			ZM_GetDawnmereLabShell(), ZM_GetDawnmereLabDoorLeft(),
			ZM_GetDawnmereLabDoorRight(), ZM_GetDawnmereLabDoorLintel(),
			ZM_GetDawnmereLabDoorTrigger() };
		bBlockoutsFinite = true;
		for (const ZM_DawnmereBlockout& xBlock : axBlocks)
		{
			bBlockoutsFinite = bBlockoutsFinite
				&& std::isfinite(xBlock.m_xCenter.x)
				&& std::isfinite(xBlock.m_xCenter.y)
				&& std::isfinite(xBlock.m_xCenter.z)
				&& std::isfinite(xBlock.m_xScale.x)
				&& std::isfinite(xBlock.m_xScale.y)
				&& std::isfinite(xBlock.m_xScale.z)
				&& xBlock.m_xScale.x > 0.0f
				&& xBlock.m_xScale.y > 0.0f
				&& xBlock.m_xScale.z > 0.0f;
		}
		const Zenith_Maths::Vector3 xSpawn = ZM_GetDawnmereFromLabSpawnFeet();
		const Zenith_Maths::Vector3 xStaging = ZM_GetDawnmereLabDoorStagingXZ();
		const Zenith_Maths::Vector3 xTarget = ZM_GetDawnmereLabDoorTargetXZ();
		bBlockoutsFinite = bBlockoutsFinite
			&& std::isfinite(xSpawn.x) && std::isfinite(xSpawn.y)
			&& std::isfinite(xSpawn.z) && std::isfinite(xStaging.z)
			&& std::isfinite(xTarget.z);

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"a lab placement accessor asserted on an argument -- Zenith_Assert breaks in "
		"EVERY config and the whole unit suite runs at boot, so this would kill the "
		"boot unit run rather than fail one test");
	ZENITH_ASSERT_EQ(uRowsFinite, uCount,
		"a lab ground row carries a null/empty name or a non-finite coordinate");
	ZENITH_ASSERT_EQ(uRowsMistakenForSentinel, 0u,
		"a REAL lab ground row is indistinguishable from the UNKNOWN sentinel -- a "
		"caller that skipped the id check could not tell content from a bad lookup");
	ZENITH_ASSERT_EQ(uBadIdsGaveSentinel, uBAD_ID_COUNT,
		"an out-of-range lab sample id did not return the DEFINED sentinel row");
	ZENITH_ASSERT_EQ(uBadIdsFiniteFeet, uBAD_ID_COUNT,
		"an out-of-range lab sample id produced a non-finite height");
	ZENITH_ASSERT_EQ(uNameCollisionsWithinLab, 0u,
		"two lab ground rows share a name, so the oracle's per-row log cannot be "
		"keyed back to a table row");
	ZENITH_ASSERT_EQ(uNameCollisionsWithHome, 0u,
		"a lab ground row shares its name with a HOME ground row -- the two oracles "
		"log in the same format, so a re-measure could paste one into the other");
	ZENITH_ASSERT_TRUE(bBlockoutsFinite,
		"a derived lab blockout or waypoint is non-finite or non-positive in scale, "
		"even before it reaches a transform");
}

// ============================================================================
// R1-2 STEP 1 -- THE ROUTE 1 ARRIVAL SEAM'S MEASURED GROUND
//
// ★★ THIS UNIT IS RED BY DESIGN UNTIL THE R1-2 FREEZE LANDS, AND THAT IS ITS JOB
// RATHER THAN A DEFECT. The route-seam row in Source/World/ZM_DawnmerePlacement.cpp
// ships at fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED -- a deliberate, invalid
// sentinel -- because nobody has ever measured the ground at (512, 864) and this
// slice deliberately measures it BEFORE anything is authored into the committed
// Dawnmere.zscen. Two clauses below red on that, loudly, and name the freeze that
// closes them:
//
//     RUN ZM_DawnmereRouteSeamGroundTruth_Test
//     (Games/Zenithmon/Tests/ZM_AutoTests_CameraClearance.cpp) AGAINST A WARM
//     DAWNMERE TERRAIN BAKE, PASTE ITS `paste=` LITERAL INTO THE
//     "R1-2 MEASURED ROUTE SEAM GROUND" BLOCK IN
//     Source/World/ZM_DawnmerePlacement.cpp, AND REBUILD.
//
// This is the same measure -> freeze -> rebuild loop S8 SC-D ran for the ten lab
// rows above; the two units that carried it there
// (LabGroundSamples_AreTenMeasurementsInsideTheGradedBand and
// LabGroundSamples_NoRowSilentlyRepeatsAnother) are the worked example.
//
// ★ WHAT THIS UNIT CAN AND CANNOT SEE. It is a claim about COMPILED CONSTANTS: it
// binds the row's XZ to the terrain recipe's reserved landmark and bounds its
// height against the recipe's own flatten target. It touches no heightfield, so it
// CANNOT prove the frozen value matches the terrain -- a plausible-but-wrong height
// inside the band passes every clause here. Only the oracle can catch that, and the
// oracle RequestSkips without a gitignored bake, which is precisely why this unit
// exists to carry the CI-visible weight.
// ============================================================================

namespace
{
	// The band the seam's measured height must land in, as a half-width around the
	// Dawnmere recipe's TARGET HEIGHT -- the height its paths and pads flatten
	// toward. Stated against the RECIPE, never against anything in the table, so a
	// value pasted from the wrong run or the wrong scene cannot validate itself.
	//
	// Calibrated exactly as fLAB_GROUND_BAND_HALF_WIDTH was, off the three shipped
	// Dawnmere tables: every measured Dawnmere surface on graded ground sits 0.3 -
	// 2.6 m ABOVE the 24 m target (the town centre 25.99, the ten Home columns
	// 25.59 - 26.54, the ten lab columns 24.32 - 26.04), because the region-wide
	// hydraulic erosion pass deposits on top of the grade. +/- 4 m clears all of
	// them by >= 1.4 m while still excluding the recipe's landforms (34 - 46 m), an
	// un-flattened field, and every degenerate default (0, the sentinel, a Y from
	// another scene).
	//
	// ★ IF A REAL MEASUREMENT LANDS OUTSIDE THIS, THE FINDING IS THAT THE ROUTE
	// CORRIDOR DOES NOT ACTUALLY REACH THE ARRIVAL COLUMN -- an R1-3 terrain-recipe
	// question -- NOT that the band should be widened. The corridor arithmetic this
	// expectation rests on is written out in the R1-2 block in
	// Source/World/ZM_DawnmerePlacement.h: (512, 864) sits 4.56 m from the "Route"
	// path's (512, 928) -> (500, 760) segment against an 18 m flatten radius, and the
	// "RouteGate" pad (r = 30 at (512, 896)) is 32 m away and does NOT reach it.
	constexpr float fROUTE_SEAM_GROUND_BAND_HALF_WIDTH = 4.0f;
}

ZENITH_TEST(ZM_Interaction, RouteSeamGround_StandsOnTheFromRoute1LandmarkAndIsMeasured)
{
	// (a) SHAPE. One row, and the accessor agrees with its own enum. A table that
	// grew or collapsed would make every clause below measure something else.
	const u_int uCount = ZM_GetDawnmereRouteSeamSampleCount();
	ZENITH_ASSERT_EQ(uCount, (u_int)ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT,
		"the route-seam sample accessor disagrees with its own enum");
	ZENITH_ASSERT_EQ(uCount, 1u,
		"the route-seam ground table no longer holds exactly the one arrival column "
		"R1-2 measures");

	// (b) THE SITE IS RESERVED IN THE TERRAIN RECIPE, AND THIS IS WHERE THE TWO ARE
	// TIED TOGETHER. ZM_DawnmerePlacement.h is pure and cannot include terrain
	// authoring, so it MIRRORS the landmark; this is what makes the mirror safe.
	// The lookup is BY NAME on the DAWNMERE recipe specifically -- Thornacre carries
	// a "FromRoute1" landmark of its own at (512, 112), so "the FromRoute1 landmark"
	// in the abstract names two different columns in two different worlds.
	const ZM_TerrainLandmarkSpec* pxLandmark = LabFindLandmark("FromRoute1");
	ZENITH_ASSERT_NOT_NULL(pxLandmark,
		"the Dawnmere terrain recipe no longer carries the 'FromRoute1' landmark -- "
		"the Route 1 return leg has no reserved arrival column, so there is nothing "
		"for this table to be a measurement OF");
	if (pxLandmark == nullptr)
	{
		return;
	}

	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fX,
		fZM_DAWNMERE_FROM_ROUTE1_X, 0.0f,
		"fZM_DAWNMERE_FROM_ROUTE1_X no longer mirrors the reserved 'FromRoute1' "
		"landmark X");
	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fZ,
		fZM_DAWNMERE_FROM_ROUTE1_Z, 0.0f,
		"fZM_DAWNMERE_FROM_ROUTE1_Z no longer mirrors the reserved 'FromRoute1' "
		"landmark Z");

	// ...and the ROW itself stands on that column, not merely the constants. A row
	// whose XZ drifted off the mirror would be measured, frozen and authored from
	// while every mirror clause above stayed green.
	const ZM_DawnmereNpcAnchor& xSeam =
		ZM_GetDawnmereRouteSeamSample(ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1);
	ZENITH_ASSERT_EQ_FLOAT(xSeam.m_fX, pxLandmark->m_xPosition.m_fX, 0.0f,
		"the route-seam row '%s' is measured at x=%.5f, not on the recipe's "
		"'FromRoute1' landmark x=%.5f", xSeam.m_szEntityName, xSeam.m_fX,
		pxLandmark->m_xPosition.m_fX);
	ZENITH_ASSERT_EQ_FLOAT(xSeam.m_fZ, pxLandmark->m_xPosition.m_fZ, 0.0f,
		"the route-seam row '%s' is measured at z=%.5f, not on the recipe's "
		"'FromRoute1' landmark z=%.5f", xSeam.m_szEntityName, xSeam.m_fZ,
		pxLandmark->m_xPosition.m_fZ);

	// (c) THE BAND'S CENTRE IS ONE NUMBER, NOT TWO. The recipe states its nominal
	// height twice -- once as the flatten TARGET and once as every landmark's
	// metadata Y -- and the band below is stated against the target. If those two
	// ever part, the band would be centred on something the landmark disagrees with,
	// so they are pinned to each other here rather than assumed.
	const float fTarget = ZM_GetDawnmereTerrainRecipe().m_fTargetHeight;
	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fY, fTarget, fLAB_EXACT_EPSILON,
		"the 'FromRoute1' landmark's metadata height %.5f is no longer the Dawnmere "
		"recipe's %.5f flatten target -- the two statements of the nominal ground "
		"height have parted, so the band below is centred on the wrong one",
		pxLandmark->m_xPosition.m_fY, fTarget);

	// (d) ★★ THE FREEZE TRIPWIRE. This is the clause that says, in CI, that R1-2
	// step 1 has not finished. It is RED ON PURPOSE while the row holds the
	// sentinel.
	//
	// Both height clauses read through ZM_DawnmereRouteSeamSampleFeetY rather than
	// off the row struct, so the id-taking accessor is exercised too: wired to the
	// wrong table it would hand back a lab or Home height, and only a clause that
	// actually calls it can see that.
	const float fSeamFeetY =
		ZM_DawnmereRouteSeamSampleFeetY(ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1);
	ZENITH_ASSERT_EQ_FLOAT(fSeamFeetY, xSeam.m_fFeetY, 0.0f,
		"ZM_DawnmereRouteSeamSampleFeetY disagrees with the row it is supposed to be "
		"reading -- one of the two route-seam accessors is wired to another table");
	ZENITH_ASSERT_GT(
		std::fabs(fSeamFeetY - fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED),
		fLAB_EXACT_EPSILON,
		"the route-seam row '%s' still holds "
		"fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED (%.1f), which is not a height. The "
		"R1-2 measure -> freeze -> rebuild loop has not closed: run "
		"ZM_DawnmereRouteSeamGroundTruth_Test against a warm Dawnmere terrain bake, "
		"paste its `paste=` literal into the R1-2 MEASURED ROUTE SEAM GROUND block in "
		"Source/World/ZM_DawnmerePlacement.cpp, and rebuild. NOTHING may be authored "
		"into Dawnmere from this row until it does",
		xSeam.m_szEntityName, fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED);

	// (e) THE BAND ITSELF, once frozen. A graded lane deposits 1.5 - 2.6 m above the
	// target; +/- 4 m admits that and nothing else.
	ZENITH_ASSERT_LT(std::fabs(fSeamFeetY - fTarget),
		fROUTE_SEAM_GROUND_BAND_HALF_WIDTH,
		"route-seam row '%s' reads %.5f, which is %.5f m from the Dawnmere recipe's "
		"%.2f m flatten target -- outside the +/-%.1f m band a graded lane can "
		"produce. It is the unfrozen sentinel, a height from another part of the "
		"world, or -- and this is the case R1-2 was sequenced to find out about -- the "
		"Route corridor does not actually level the arrival column. Read the oracle's "
		"measured value before doing anything: if it is a REAL height outside this "
		"band, that is an R1-3 terrain finding, NOT a band to widen",
		xSeam.m_szEntityName, fSeamFeetY, std::fabs(fSeamFeetY - fTarget),
		fTarget, fROUTE_SEAM_GROUND_BAND_HALF_WIDTH);
}

// ============================================================================
// R1-2 STEP 3 -- THE ARRIVAL ACCESSOR'S ASSEMBLY
//
// ★ THE THREE INGREDIENTS ARE PINNED ABOVE; NOTHING PINNED THE ASSEMBLY. The
// unit above binds each of ZM_GetDawnmereFromRoute1SpawnFeet()'s three inputs to
// the thing it is a measurement OF -- the two constants to the recipe's reserved
// "FromRoute1" landmark, the height to the measured route-seam row -- but no
// clause anywhere CALLS the accessor. Its only other caller in the repo is the
// TOOLS-ONLY authoring site in Zenithmon.cpp, which no headless run executes, so
// a transposed X/Z or a Y/Z swap inside its one-line constructor was caught by
// nothing at all. That matters more here than it would for a derived quantity:
// the value it returns is authored straight into the COMMITTED Dawnmere.zscen,
// and only a WINDOWED *_True re-author can move those bytes back.
//
// Both siblings already had this and this symbol did not:
// ZM_GetDawnmereFromHomeSpawnFeet is exercised by
// HomeApproachIsClearOfTheDriveCorridor (Tests/ZM_Tests_Interaction.cpp) and
// ZM_GetDawnmereFromLabSpawnFeet by LabSampleAccessors_AreTotalOnEveryDegenerateId
// above.
//
// ★ EVERY COMPONENT IS ASSERTED AGAINST ITS OWN SOURCE, one clause each, never
// against a second construction of the same three ingredients -- a clause built
// that way would transpose in lockstep with the accessor and pin nothing.
// ============================================================================

ZENITH_TEST(ZM_Interaction, FromRoute1SpawnFeet_AssemblesItsIngredientsWithoutTransposingThem)
{
	// (a) ★ THE ANTI-VACUITY CLAUSE, AND IT IS THE WHOLE REASON A SWAP IS VISIBLE
	// BELOW. This unit can only tell a transposed accessor from a correct one
	// because the arrival column's X and Z are DIFFERENT numbers -- 512 and 864.
	// If they were ever made equal (a square-symmetric relocation of the column
	// would do it) the .x and .z clauses would both still pass under a swap and
	// this test would silently stop testing the one thing it exists for. It fails
	// HERE instead, and the finding would then be that the accessor needs a
	// different oracle -- not that this clause should be dropped.
	ZENITH_ASSERT_GT(
		std::fabs(fZM_DAWNMERE_FROM_ROUTE1_X - fZM_DAWNMERE_FROM_ROUTE1_Z),
		fLAB_EXACT_EPSILON,
		"fZM_DAWNMERE_FROM_ROUTE1_X (%.5f) and fZM_DAWNMERE_FROM_ROUTE1_Z (%.5f) are "
		"the same number, so the component clauses below can no longer distinguish a "
		"transposed ZM_GetDawnmereFromRoute1SpawnFeet() from a correct one",
		fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z);

	const Zenith_Maths::Vector3 xFeet = ZM_GetDawnmereFromRoute1SpawnFeet();
	const float fSeamFeetY =
		ZM_DawnmereRouteSeamSampleFeetY(ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1);

	// (b) TOTALITY FIRST, because a NaN satisfies every EQ_FLOAT clause below:
	// AssertEqFloat fails on `fabs(a - b) > eps` and that comparison is FALSE for
	// NaN, so a non-finite ingredient would pass the assembly silently and then be
	// authored into a transform.
	ZENITH_ASSERT_TRUE(std::isfinite(xFeet.x) && std::isfinite(xFeet.y)
			&& std::isfinite(xFeet.z),
		"ZM_GetDawnmereFromRoute1SpawnFeet() returned (%.5f, %.5f, %.5f), which has a "
		"non-finite component -- authoring from it would put a non-finite transform "
		"into the committed Dawnmere.zscen",
		xFeet.x, xFeet.y, xFeet.z);

	// (c) THE THREE COMPONENT CLAUSES. Exact (epsilon 0) on purpose: each side is
	// the SAME compiled value, not a re-derivation of it, so any tolerance at all
	// would only be room for a wrong answer to hide in.
	ZENITH_ASSERT_EQ_FLOAT(xFeet.x, fZM_DAWNMERE_FROM_ROUTE1_X, 0.0f,
		"ZM_GetDawnmereFromRoute1SpawnFeet().x is %.5f, not the arrival column's "
		"fZM_DAWNMERE_FROM_ROUTE1_X (%.5f). If it reads %.5f the constructor's X and "
		"Z arguments are transposed.",
		xFeet.x, fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z);
	ZENITH_ASSERT_EQ_FLOAT(xFeet.z, fZM_DAWNMERE_FROM_ROUTE1_Z, 0.0f,
		"ZM_GetDawnmereFromRoute1SpawnFeet().z is %.5f, not the arrival column's "
		"fZM_DAWNMERE_FROM_ROUTE1_Z (%.5f). If it reads %.5f the constructor's X and "
		"Z arguments are transposed; if it reads the seam height the Y and Z "
		"arguments are.",
		xFeet.z, fZM_DAWNMERE_FROM_ROUTE1_Z, fZM_DAWNMERE_FROM_ROUTE1_X);
	ZENITH_ASSERT_EQ_FLOAT(xFeet.y, fSeamFeetY, 0.0f,
		"ZM_GetDawnmereFromRoute1SpawnFeet().y is %.5f, not the MEASURED route-seam "
		"surface ZM_DawnmereRouteSeamSampleFeetY(FROM_ROUTE1) reports (%.5f). This is "
		"a FEET position and ZM_GameStateManager::CalculateSpawnCenter adds the "
		"capsule half-extent at warp time, so a Y that is anything else drops every "
		"player arriving off Route 1 in from the wrong height.",
		xFeet.y, fSeamFeetY);
}
