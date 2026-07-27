#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerSight -- S7 item 3 SC3 unit tests for the PURE trainer
// sight-cone predicate: "can this trainer see the player from where they are
// standing and which way they are facing?".
//
// Everything here is PURE: no ECS, no scene, no physics body, no UI, no baked
// asset, no engine instance -- just plain maths types in and one bool out. Every
// fixture is deterministic and hermetic, so no RequestSkip is needed.
//
// Category ZM_Interaction -- the SAME category as the SC1/SC2 interaction-logic
// units, because this is the same feature area (the shared XZ cone) and the
// extracted primitive those units already police lives one call away.
//
// OCCLUSION IS DELIBERATELY UNTESTED HERE because it deliberately does not
// exist: a trainer whose line of sight is blocked by a wall still satisfies this
// predicate. The raycast enters at SC6 as a FILTER in the impure glue that owns
// the scene, precisely so this picker stays pure. A unit asserting "a wall
// blocks sight" would be asserting against the design.
//
// Every fixture below puts the trainer at the WORLD ORIGIN looking down +Z, so a
// target's raw position IS its offset from the trainer and each unit reads as a
// picture.
// ============================================================================

#include <limits>    // quiet_NaN / infinity (the fail-closed table)

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_AssertCapture.h"                        // the totality proof
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"      // ZM_FlattenXZ + the interact tuning it is compared against
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"

namespace
{
	constexpr float fSIGHT_TEST_EPSILON = 0.001f;

	// The SHIPPED tuning. A default-constructed ZM_TrainerSightTuning already IS
	// the shipped tuning, so this exists purely to say so at every call site.
	ZM_TrainerSightTuning MakeShippedTuning()
	{
		return ZM_TrainerSightTuning();
	}

	// A cone that admits EVERY direction (the parameter is a cosine, so anything
	// at or below -1 is "all directions"). Used wherever the unit under test is a
	// guard OTHER than the angular one: with this tuning the cone comparison can
	// never be what rejects, so a FALSE answer indicts exactly one guard.
	ZM_TrainerSightTuning MakeAllDirectionsTuning()
	{
		ZM_TrainerSightTuning xTuning;
		xTuning.m_fMinFacingDot = -1.5f;
		return xTuning;
	}

	Zenith_Maths::Vector3 MakeTrainerAtOrigin()
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	Zenith_Maths::Vector3 MakeForwardPlusZ()
	{
		return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	// A 3-4-5 triangle scaled by 1/4: offset (0.75, 0, 1.0) has XZ length EXACTLY
	// 1.25 and therefore a facing dot of exactly 1.0 / 1.25 == 0.8f against +Z.
	// Every term is a dyadic rational, so the predicate's dot and this constant are
	// bit-identical and the inclusive-boundary units cannot flake. The SAME fixture
	// the shipped ZM_Tests_Interaction boundary units use, deliberately: both suites
	// now exercise ONE primitive, so they must agree on where its edge is.
	constexpr float fBOUNDARY_TARGET_X = 0.75f;
	constexpr float fBOUNDARY_TARGET_Z = 1.0f;
	constexpr float fBOUNDARY_FACING_DOT = 0.8f;
}

// ---- The baseline every other unit is a delta from ---------------------------

ZENITH_TEST(ZM_Interaction, Sight_DeadCentreTargetIsSeen)
{
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), MakeShippedTuning()),
		"a target four units straight ahead, well inside the shipped range, band and "
		"cone, must be seen -- if this fails the predicate sees nothing at all");
}

// ---- The angular boundary, both arms -----------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_ExactlyAtMinFacingDotIsSeen)
{
	// The shipped cone is NARROWER than this fixture's cone, so the fixture must
	// widen the threshold to test the boundary rather than test the shipped value.
	// Asserted, so a retune that crosses 0.8 turns this into a loud failure instead
	// of a unit quietly testing something else.
	ZENITH_ASSERT_GT(fZM_SIGHT_MIN_FACING_DOT, fBOUNDARY_FACING_DOT,
		"the shipped sight cone must stay narrower than the dyadic boundary fixture, "
		"or this unit stops testing the boundary it claims to test");

	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMinFacingDot = fBOUNDARY_FACING_DOT;

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(fBOUNDARY_TARGET_X, 0.0f, fBOUNDARY_TARGET_Z), xTuning),
		"the cone test is INCLUSIVE: a target exactly on the cone edge is seen");
}

ZENITH_TEST(ZM_Interaction, Sight_JustBelowMinFacingDotIsNotSeen)
{
	// The reject arm. Without it the accept arm above would pass on a predicate
	// that returned true unconditionally.
	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMinFacingDot = fBOUNDARY_FACING_DOT + 0.001f;

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(fBOUNDARY_TARGET_X, 0.0f, fBOUNDARY_TARGET_Z), xTuning),
		"one thousandth outside the cone is OUTSIDE the cone");
}

// ---- The range boundary, both arms -------------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_ExactlyAtMaxDistanceIsSeen)
{
	// Built FROM the shipped constant, so raising fZM_SIGHT_MAX_DISTANCE moves the
	// fixture rather than breaking it. 8.0 and 64.0 are both exactly representable,
	// so the squared comparison is bit-exact and this cannot flake.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, fZM_SIGHT_MAX_DISTANCE), MakeShippedTuning()),
		"the range test is INCLUSIVE: a target exactly at max distance is seen");
}

ZENITH_TEST(ZM_Interaction, Sight_JustBeyondMaxDistanceIsNotSeen)
{
	// Dead centre in the cone and inside the band, so RANGE is the only thing that
	// can reject it.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, fZM_SIGHT_MAX_DISTANCE + 0.001f), MakeShippedTuning()),
		"one thousandth beyond max distance is OUT of range, cone or no cone");
}

// ---- Direction: behind and beside --------------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_TargetBehindTrainerIsNotSeen)
{
	// Well inside range and band, dot exactly -1. A symmetric double cone (the
	// classic sight bug, fabs() around the dot) would report this as seen.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f), MakeShippedTuning()),
		"a trainer must not see through the back of their own head");

	// CONTROL, so "rejects everything" cannot pass this unit: the MIRRORED target,
	// identical in every respect but its sign, IS seen.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), MakeShippedTuning()),
		"...but the mirrored target in front, at the same distance, IS seen");
}

ZENITH_TEST(ZM_Interaction, Sight_TargetBesideTrainerIsNotSeen)
{
	// Perpendicular: dot exactly 0, inside range and band. This is the ONLY unit
	// that pins the SHIPPED cone as genuinely narrower than a hemisphere -- every
	// other angular unit overrides the threshold.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f), MakeShippedTuning()),
		"the SHIPPED cone must be narrower than a hemisphere: a target at exactly "
		"90 degrees off the facing is not seen");
}

// ---- The Y policy: planar range, separate absolute band ----------------------

ZENITH_TEST(ZM_Interaction, Sight_DistanceIgnoresYComponent)
{
	// Adversarial: the XZ distance is EXACTLY the shipped range, but the 3D
	// distance is beyond it. A 3D range test cannot pass this unit.
	const ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	const Zenith_Maths::Vector3 xTrainer = MakeTrainerAtOrigin();
	const Zenith_Maths::Vector3 xTarget(0.0f, 1.5f, fZM_SIGHT_MAX_DISTANCE);

	ZENITH_ASSERT_GT(glm::length(xTarget - xTrainer), xTuning.m_fMaxDistance,
		"fixture precondition: this target's 3D distance must EXCEED the range, so "
		"only an XZ range test can accept it");

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(xTrainer, MakeForwardPlusZ(), xTarget, xTuning),
		"range is measured in XZ with Y dropped -- a trainer on a slope must still "
		"spot a player at the same planar distance");
}

ZENITH_TEST(ZM_Interaction, Sight_VerticalBandIsSymmetricAndInclusive)
{
	// Every target below is dead centre in the cone and two units away in XZ, so the
	// BAND is the only thing that can move any answer here.

	// ABOVE, outside: three units up against a two-unit band.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 3.0f, 2.0f), MakeShippedTuning()),
		"a target a storey above the trainer is outside the vertical band");

	// BELOW, outside. This arm is what makes the band ABSOLUTE: drop the fabs() and
	// the comparison becomes `dY > band`, which admits a target arbitrarily far
	// BELOW the trainer -- i.e. a trainer spotting the player through the floor,
	// exactly the failure the band exists to prevent. No other unit in either suite
	// feeds a negative dY, so this assertion is that mutation's only tripwire.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, -3.0f, 2.0f), MakeShippedTuning()),
		"...and a target a storey BELOW is outside it too -- the band is fabs(dY), "
		"not a one-sided ceiling test");

	// BOTH EDGES, inclusive. Built FROM the shipped constant so a retune moves the
	// fixture rather than breaking it; 2.0 is exactly representable, so `fabs(dY) >
	// band` is bit-exact here and this cannot flake. Turning the comparison into
	// `>=` reds both of these and nothing else.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, fZM_SIGHT_MAX_VERTICAL, 2.0f), MakeShippedTuning()),
		"the band is INCLUSIVE: a target exactly one band-height ABOVE is seen");
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, -fZM_SIGHT_MAX_VERTICAL, 2.0f), MakeShippedTuning()),
		"...and so is one exactly one band-height BELOW");

	// CONTROL: a small step up, well inside the band, IS seen -- so this unit proves
	// the BAND specifically rather than "nothing off-axis is ever seen".
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 1.0f, 2.0f), MakeShippedTuning()),
		"...and a small step up, inside the band, IS seen");
}

// ---- Degenerate facings -------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_ZeroLengthForwardIsNotSeen)
{
	// The ALL-DIRECTIONS tuning is load-bearing: with it the cone comparison cannot
	// be what rejects, so the no-facing guard is the ONLY thing under test.
	const ZM_TrainerSightTuning xTuning = MakeAllDirectionsTuning();

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(),
		Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"no facing means no cone: a zero forward must FAIL CLOSED, never see everything");

	// CONTROL: the same all-directions tuning with a real forward DOES see the target.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"...and the same tuning with a real facing DOES see it, so this unit is not "
		"passing merely because everything is rejected");
}

ZENITH_TEST(ZM_Interaction, Sight_StraightUpForwardIsNotSeen)
{
	// Non-zero, but its XZ projection collapses -- only the flatten policy can
	// reject it, since the cone admits every direction here.
	const Zenith_Maths::Vector3 xForwardUp(0.0f, 1.0f, 0.0f);

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), xForwardUp,
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), MakeAllDirectionsTuning()),
		"a straight-up facing has no XZ direction, so it faces nothing");

	// The shared flatten must return EXACTLY zero, not a NaN. Both float assert
	// macros are NaN-PERMISSIVE (they fail only when a difference EXCEEDS the
	// epsilon, and every comparison against NaN is false), so an exact == 0 triple
	// is what actually catches an unguarded normalise here.
	const Zenith_Maths::Vector3 xFlat = ZM_FlattenXZ(xForwardUp);
	ZENITH_ASSERT_TRUE((xFlat.x == 0.0f) && (xFlat.y == 0.0f) && (xFlat.z == 0.0f),
		"ZM_FlattenXZ must return EXACTLY zero for a collapsing facing -- a NaN "
		"compares false against every threshold and would sail through the guard");
}

// ---- Coincident, and the two range degeneracies -------------------------------

ZENITH_TEST(ZM_Interaction, Sight_CoincidentTargetIsSeen)
{
	// There is no direction to test and the player is standing on the trainer. This
	// matches the interact picker's coincident carve-out exactly (the shipped
	// Pick_CoincidentProbeIsInRangeAndFacedAndWins), which is the point: ONE cone.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f), MakeShippedTuning()),
		"a target standing exactly on the trainer is seen, not divided by zero");

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), MakeShippedTuning()),
		"and one offset only in Y, still inside the band, is coincident in XZ too");
}

ZENITH_TEST(ZM_Interaction, Sight_NegativeRangeSeesNothing)
{
	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMaxDistance = -1.0f;

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"a negative range must never SQUARE its way back into a positive reach");

	// The squared comparison alone would accept this one (0 > 1 is false), so the
	// coincident case is what actually proves the negative-range guard exists.
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f), xTuning),
		"not even a coincident target is seen at a negative range");
}

ZENITH_TEST(ZM_Interaction, Sight_ZeroRangeSeesOnlyACoincidentTarget)
{
	// The documented boundary between "negative range sees nothing" and the normal
	// case: at zero range the INCLUSIVE range test admits distance 0 and nothing else.
	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMaxDistance = 0.0f;

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f), xTuning),
		"zero range still admits distance zero -- the range test is inclusive");

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xTuning),
		"...and nothing else: one unit away is already out of a zero range");
}

// ---- The cosine parameter has no out-of-domain value -------------------------

ZENITH_TEST(ZM_Interaction, Sight_ImpossibleConeSeesNothingButACoincidentTarget)
{
	// A half-angle at or below zero: no direction can satisfy it. Pins that the
	// parameter is NOT clamped into [-1, 1] and does NOT assert -- it is a cosine,
	// so it has no out-of-domain value.
	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMinFacingDot = 1.5f;

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"a dead-centre target has a dot of exactly 1.0, which is still short of 1.5");

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f), xTuning),
		"...but a coincident target skips the angular test by construction");
}

ZENITH_TEST(ZM_Interaction, Sight_FullCircleConeStillObeysRange)
{
	// A half-angle at or beyond 180 degrees: the angular test admits everything,
	// and range must still gate. Also proves no clamp / assert intercepts a cosine
	// below -1.
	const ZM_TrainerSightTuning xTuning = MakeAllDirectionsTuning();

	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f), xTuning),
		"an all-directions cone sees a target directly BEHIND, in range");

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, -(fZM_SIGHT_MAX_DISTANCE + 1.0f)), xTuning),
		"...but widening the CONE must not disable the RANGE test");
}

ZENITH_TEST(ZM_Interaction, Sight_NegativeVerticalBandSeesNothing)
{
	// fabs(0) > -1, so even a target at the trainer's exact height is rejected. A
	// defensive fabs() around the threshold would silently resurrect the band.
	ZM_TrainerSightTuning xTuning = MakeShippedTuning();
	xTuning.m_fMaxVertical = -1.0f;

	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"a negative vertical band admits nothing, not even a target at the exact "
		"same height -- fabs(0) is still greater than -1");

	// CONTROL: the identical fixture with the shipped band IS seen.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), MakeShippedTuning()),
		"...and the same fixture with the shipped band IS seen");
}

// ---- Non-finite input fails CLOSED -------------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_NonFiniteInputFailsClosed)
{
	// SC6 will feed this live physics positions. One body that goes non-finite must
	// not make every trainer in the scene see the player: WITHOUT the finite guard a
	// NaN sails through range (NaN > r*r is false), through the band (NaN > v is
	// false) and into the coincident arm (NaN > epsilon is false) and reports "seen".
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	struct ZM_SightPoison
	{
		Zenith_Maths::Vector3 m_xTrainer;
		Zenith_Maths::Vector3 m_xForward;
		Zenith_Maths::Vector3 m_xTarget;
		float                 m_fMaxDistance;
		float                 m_fMinFacingDot;
		float                 m_fMaxVertical;
		const char*           m_szWhat;
	};

	// The base row is the KNOWN-TRUE dead-centre case; each row below poisons
	// exactly ONE component of it.
	const Zenith_Maths::Vector3 xBaseTrainer(0.0f);
	const Zenith_Maths::Vector3 xBaseForward(0.0f, 0.0f, 1.0f);
	const Zenith_Maths::Vector3 xBaseTarget(0.0f, 0.0f, 4.0f);
	const ZM_TrainerSightTuning xBaseTuning = MakeShippedTuning();

	const ZM_SightPoison axPoisoned[] =
	{
		{ { fNaN, 0.0f, 0.0f }, xBaseForward, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "trainer x NaN" },
		{ { 0.0f, fInf, 0.0f }, xBaseForward, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "trainer y +inf" },
		{ { 0.0f, 0.0f, -fInf }, xBaseForward, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "trainer z -inf" },
		{ xBaseTrainer, { fNaN, 0.0f, 1.0f }, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "forward x NaN" },
		{ xBaseTrainer, { 0.0f, fInf, 1.0f }, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "forward y +inf" },
		{ xBaseTrainer, { 0.0f, 0.0f, fNaN }, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "forward z NaN" },
		{ xBaseTrainer, xBaseForward, { fNaN, 0.0f, 4.0f }, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "target x NaN" },
		{ xBaseTrainer, xBaseForward, { 0.0f, fNaN, 4.0f }, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "target y NaN" },
		{ xBaseTrainer, xBaseForward, { 0.0f, 0.0f, fInf }, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "target z +inf" },
		{ xBaseTrainer, xBaseForward, xBaseTarget, fNaN, xBaseTuning.m_fMinFacingDot, xBaseTuning.m_fMaxVertical, "tuning max distance NaN" },
		{ xBaseTrainer, xBaseForward, xBaseTarget, xBaseTuning.m_fMaxDistance, fNaN, xBaseTuning.m_fMaxVertical, "tuning min facing dot NaN" },
		{ xBaseTrainer, xBaseForward, xBaseTarget, xBaseTuning.m_fMaxDistance, xBaseTuning.m_fMinFacingDot, fInf, "tuning max vertical +inf" }
	};
	const u_int uPoisonedCount = (u_int)(sizeof(axPoisoned) / sizeof(axPoisoned[0]));

	// A loop bounded by a count that is itself zero passes VACUOUSLY, so the count
	// is guarded before the walk.
	ZENITH_ASSERT_GT(uPoisonedCount, 0u,
		"the poisoned fixture table must not be empty, or the walk below proves nothing");

	// CONTROL: the un-poisoned base row IS seen, so "returns false for everything"
	// cannot pass this unit.
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(xBaseTrainer, xBaseForward, xBaseTarget, xBaseTuning),
		"fixture precondition: the un-poisoned base row must be SEEN");

	for (u_int u = 0u; u < uPoisonedCount; ++u)
	{
		const ZM_SightPoison& xRow = axPoisoned[u];
		ZM_TrainerSightTuning xTuning;
		xTuning.m_fMaxDistance  = xRow.m_fMaxDistance;
		xTuning.m_fMinFacingDot = xRow.m_fMinFacingDot;
		xTuning.m_fMaxVertical  = xRow.m_fMaxVertical;

		ZENITH_ASSERT_FALSE(
			ZM_IsTargetInTrainerSight(xRow.m_xTrainer, xRow.m_xForward, xRow.m_xTarget, xTuning),
			"non-finite input must FAIL CLOSED (%s): one body that goes non-finite must "
			"not make every trainer in the scene see the player", xRow.m_szWhat);
	}
}

// ---- The flatten is not merely the identity ----------------------------------

ZENITH_TEST(ZM_Interaction, Sight_UnnormalisedPitchedForwardStillConesCorrectly)
{
	// THE anti-vacuity unit. Every other fixture looks along an already-flat,
	// already-unit +Z, so the flatten/normalise is only ever exercised as the
	// IDENTITY -- delete it, pass the raw forward, and they all still pass. This
	// forward is steeply pitched with an XZ magnitude far BELOW one, so an
	// un-normalised dot for the target below would be about 0.1, well under the
	// shipped 0.866 threshold, and the trainer would see nothing.
	const Zenith_Maths::Vector3 xPitched(0.0f, 10.0f, 0.1f);
	const Zenith_Maths::Vector3 xTarget(0.0f, 0.0f, 4.0f);

	const bool bPitched = ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), xPitched,
		xTarget, MakeShippedTuning());
	ZENITH_ASSERT_TRUE(bPitched,
		"a pitched, short-in-XZ forward must flatten+normalise to the same facing "
		"every other unit uses -- a raw dot here is about 0.1 and sees nothing");

	const bool bFlat = ZM_IsTargetInTrainerSight(MakeTrainerAtOrigin(), MakeForwardPlusZ(),
		xTarget, MakeShippedTuning());
	ZENITH_ASSERT_EQ(bPitched, bFlat,
		"and it must answer EXACTLY what the already-flattened forward answers");
}

// ---- The rotation entry point -------------------------------------------------

ZENITH_TEST(ZM_Interaction, Sight_RotationFormMatchesForwardFormIncludingHalfTurn)
{
	const Zenith_Maths::Vector3 xTrainer = MakeTrainerAtOrigin();
	const ZM_TrainerSightTuning xTuning = MakeShippedTuning();

	const Zenith_Maths::Quat axRotations[] =
	{
		// glm::quat's scalar-first constructor: (w, x, y, z).
		Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		Zenith_Maths::AngleAxis(glm::radians(90.0f),  Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
		Zenith_Maths::AngleAxis(glm::radians(180.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))
	};
	const Zenith_Maths::Vector3 axTargets[] =
	{
		Zenith_Maths::Vector3(0.0f, 0.0f,  4.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f),
		Zenith_Maths::Vector3(4.0f, 0.0f,  0.0f)
	};
	const u_int uRotationCount = (u_int)(sizeof(axRotations) / sizeof(axRotations[0]));
	const u_int uTargetCount   = (u_int)(sizeof(axTargets)   / sizeof(axTargets[0]));

	// Guarded, so neither walk can pass vacuously.
	ZENITH_ASSERT_GT(uRotationCount, 0u, "the rotation table must not be empty");
	ZENITH_ASSERT_GT(uTargetCount,   0u, "the target table must not be empty");

	for (u_int uR = 0u; uR < uRotationCount; ++uR)
	{
		for (u_int uT = 0u; uT < uTargetCount; ++uT)
		{
			const bool bFromRotation = ZM_IsTargetInTrainerSightFromRotation(
				xTrainer, axRotations[uR], axTargets[uT], xTuning);
			const bool bFromForward = ZM_IsTargetInTrainerSight(
				xTrainer, ZM_ForwardFromRotation(axRotations[uR]), axTargets[uT], xTuning);
			ZENITH_ASSERT_EQ(bFromRotation, bFromForward,
				"rotation %u / target %u: the quaternion form must DERIVE its facing with "
				"ZM_ForwardFromRotation and forward, never re-derive one of its own",
				uR, uT);
		}
	}

	// Agreement alone would still pass if BOTH forms were rewritten in terms of
	// glm::eulerAngles(quat).y, which collapses past 90 degrees off +Z and has
	// already cost this repo a full debugging cycle. So the half-turn is pinned
	// against the CONCRETE expectation too.
	const Zenith_Maths::Quat& xHalfTurn = axRotations[2];
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSightFromRotation(xTrainer, xHalfTurn,
		Zenith_Maths::Vector3(0.0f, 0.0f, -4.0f), xTuning),
		"a half-turned trainer faces -Z and sees a target there");
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSightFromRotation(xTrainer, xHalfTurn,
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), xTuning),
		"...and does NOT see the target behind them at +Z");
}

// ---- Totality -----------------------------------------------------------------

// THE totality proof, in the ZM_Tests_TrainerData local-hit-count form. Written
// that way for two mandatory reasons: (1) NO ZENITH_ASSERT_* may appear INSIDE
// the scope -- while a capture scope is active Zenith_TestRunner::HandleFailure
// swallows framework failures and merely bumps the hit count, so an in-scope
// assertion could never red this test; (2) the count MUST be copied to a local
// before the closing brace, because ~Zenith_AssertCaptureScope restores the
// previous hit count. Scopes do not nest.
ZENITH_TEST(ZM_Interaction, Sight_NeverAssertsOnAnyDegenerateInput)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		ZM_TrainerSightTuning xGarbageTuning;
		xGarbageTuning.m_fMaxDistance  = -1.0e30f;
		xGarbageTuning.m_fMinFacingDot = 12345.0f;
		xGarbageTuning.m_fMaxVertical  = -fInf;

		ZM_TrainerSightTuning xNaNTuning;
		xNaNTuning.m_fMaxDistance  = fNaN;
		xNaNTuning.m_fMinFacingDot = fNaN;
		xNaNTuning.m_fMaxVertical  = fNaN;

		const bool b0 = ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(fNaN, fNaN, fNaN),
			Zenith_Maths::Vector3(fInf, 0.0f, fInf), Zenith_Maths::Vector3(fNaN),
			xGarbageTuning);
		const bool b1 = ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f), xNaNTuning);
		const bool b2 = ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
			xGarbageTuning);
		const bool b3 = ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f),
			Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f),
			xGarbageTuning);
		// The rotation form, including a NaN quaternion: it must DERIVE a non-finite
		// forward and be rejected by the finite guard, not assert on the way through.
		const bool b4 = ZM_IsTargetInTrainerSightFromRotation(Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Quat(fNaN, fNaN, fNaN, fNaN), Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f),
			MakeShippedTuning());
		const bool b5 = ZM_IsTargetInTrainerSightFromRotation(Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Quat(0.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f),
			xNaNTuning);
		// The shared primitives directly, with the same garbage.
		const Zenith_Maths::Vector3 x6 = ZM_FlattenXZ(Zenith_Maths::Vector3(fNaN, fInf, fNaN));
		const bool b7 = ZM_IsFacingXZ(Zenith_Maths::Vector3(fNaN), Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Vector3(fInf), fNaN);

		(void)b0; (void)b1; (void)b2; (void)b3; (void)b4; (void)b5; (void)x6; (void)b7;

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the sight predicate asserted on an argument -- Zenith_Assert breaks in EVERY "
		"config and the whole unit suite runs at boot, so this would kill the whole "
		"boot unit run rather than fail one test");

	// ...and the answers are still the DEFINED ones outside the scope, so this unit
	// is not merely "nothing crashed".
	ZENITH_ASSERT_FALSE(ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(fNaN, 0.0f, 4.0f), MakeShippedTuning()),
		"a non-finite target is still not seen");
	ZENITH_ASSERT_TRUE(ZM_IsTargetInTrainerSight(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f), MakeShippedTuning()),
		"and a coincident target still IS seen");
}

// ---- The shipped numbers, pinned as RELATIONS --------------------------------

ZENITH_TEST(ZM_Interaction, Sight_ShippedTuningIsANarrowForwardCone)
{
	// Pinned as relations to the interact constants rather than as magic numbers,
	// so SC6 playtesting can retune the sight values without touching a single
	// boundary unit -- but cannot silently turn the cone into a sphere.
	ZENITH_ASSERT_GT(fZM_SIGHT_MIN_FACING_DOT, 0.0f,
		"the sight cone must be a real FORWARD cone, narrower than a hemisphere");
	ZENITH_ASSERT_LT(fZM_SIGHT_MIN_FACING_DOT, 1.0f,
		"...and not an impossible cone that nothing can ever satisfy");
	ZENITH_ASSERT_GT(fZM_SIGHT_MIN_FACING_DOT, fZM_INTERACT_MIN_FACING_DOT,
		"a trainer STARES DOWN A LANE: sight must stay narrower than the talk cone");
	ZENITH_ASSERT_GT(fZM_SIGHT_MAX_DISTANCE, fZM_INTERACT_MAX_DISTANCE,
		"being spotted must be able to happen BEFORE the player is close enough to talk");
	ZENITH_ASSERT_GT(fZM_SIGHT_MAX_VERTICAL, 0.0f,
		"a non-positive vertical band would make every trainer blind");

	// The default-constructed tuning IS the shipped tuning -- every unit above that
	// starts from MakeShippedTuning() depends on it.
	const ZM_TrainerSightTuning xDefault;
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fMaxDistance, fZM_SIGHT_MAX_DISTANCE, fSIGHT_TEST_EPSILON,
		"a default-constructed tuning must carry the shipped range");
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fMinFacingDot, fZM_SIGHT_MIN_FACING_DOT, fSIGHT_TEST_EPSILON,
		"a default-constructed tuning must carry the shipped cone");
	ZENITH_ASSERT_EQ_FLOAT(xDefault.m_fMaxVertical, fZM_SIGHT_MAX_VERTICAL, fSIGHT_TEST_EPSILON,
		"a default-constructed tuning must carry the shipped vertical band");
}
