#include "Zenith.h"

// ============================================================================
// ZM_Tests_NpcWalker -- S6 item 3 SC8 tests for the PURE authored-waypoint
// walker. No ECS, scene, physics body, UI, baked asset or process-global state is
// involved here: the component glue is covered by ZM_NpcWander_Test.
//
// The contract is deliberately small. One call consumes (waypoints, state,
// position, dt, halted, tuning), returns a horizontal direction + requested
// speed, and mutates only the explicit cursor/dwell state. Determinism is what
// makes the windowed rendezvous test bounded; no hidden RNG is permitted.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Interaction/ZM_NpcWalkerLogic.h"

#include <cmath>

namespace
{
	constexpr float fWALKER_TEST_EPSILON = 0.0001f;
	constexpr float fWALKER_TEST_DT = 0.1f;

	ZM_WalkerTuning MakeTuning(
		float fSpeed = 2.0f,
		float fArriveRadius = 0.25f,
		float fDwellSeconds = 1.5f)
	{
		ZM_WalkerTuning xTuning{};
		xTuning.m_fSpeed = fSpeed;
		xTuning.m_fArriveRadius = fArriveRadius;
		xTuning.m_fDwellSeconds = fDwellSeconds;
		return xTuning;
	}

	ZM_WalkerWaypoints MakeWaypoints(
		const Zenith_Maths::Vector3& xFirst,
		const Zenith_Maths::Vector3& xSecond,
		u_int uCount = 2u)
	{
		ZM_WalkerWaypoints xWaypoints{};
		xWaypoints.m_axPoints[0] = xFirst;
		xWaypoints.m_axPoints[1] = xSecond;
		xWaypoints.m_uCount = uCount;
		return xWaypoints;
	}

	void AssertStopped(const ZM_WalkerStep& xStep, const char* szContext)
	{
		ZENITH_ASSERT_NEAR_VEC3(xStep.m_xDirXZ, Zenith_Maths::Vector3(0.0f),
			fWALKER_TEST_EPSILON, "%s: stopped output must have a zero XZ direction", szContext);
		ZENITH_ASSERT_EQ_FLOAT(xStep.m_fSpeed, 0.0f, fWALKER_TEST_EPSILON,
			"%s: stopped output must request zero speed", szContext);
	}

	bool IsFinite(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}
}

ZENITH_TEST(ZM_NpcWalker, Walker_ZeroWaypointsProducesNoMotion)
{
	ZM_WalkerWaypoints xWaypoints{};
	ZM_WalkerState xState{};
	xState.m_uTargetIndex = 3u; // deliberately invalid: an empty walk must not index it

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, { 4.0f, 7.0f, -2.0f }, fWALKER_TEST_DT,
		false, MakeTuning());

	AssertStopped(xStep, "zero-waypoint patrol");
	ZENITH_ASSERT_FALSE(xStep.m_bArrivedThisStep,
		"an empty patrol cannot report an arrival");
}

ZENITH_TEST(ZM_NpcWalker, Walker_SingleWaypointParksOnArrival)
{
	const Zenith_Maths::Vector3 xPoint(3.0f, 9.0f, -4.0f);
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(xPoint, {}, 1u);
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, xPoint, fWALKER_TEST_DT, false, MakeTuning());

	AssertStopped(xStep, "single-waypoint arrival");
	ZENITH_ASSERT_TRUE(xStep.m_bArrivedThisStep,
		"being on the sole waypoint must report the arrival beat");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 0u,
		"a one-point patrol must remain parked on index zero");
}

ZENITH_TEST(ZM_NpcWalker, Walker_HeadsTowardCurrentWaypoint)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 3.0f, 50.0f, 4.0f }, { -2.0f, -7.0f, 1.0f });
	ZM_WalkerState xState{};
	const ZM_WalkerTuning xTuning = MakeTuning(2.5f);

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, -20.0f, 0.0f }, fWALKER_TEST_DT,
		false, xTuning);

	ZENITH_ASSERT_NEAR_VEC3(xStep.m_xDirXZ,
		Zenith_Maths::Vector3(0.6f, 0.0f, 0.8f), fWALKER_TEST_EPSILON,
		"the movement direction must be the normalised XZ direction to target index zero");
	ZENITH_ASSERT_EQ_FLOAT(xStep.m_fSpeed, 2.5f, fWALKER_TEST_EPSILON,
		"a moving step must carry the authored patrol speed");
	ZENITH_ASSERT_FALSE(xStep.m_bArrivedThisStep,
		"a distant target is not an arrival");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 0u,
		"moving toward a target must not advance the cursor early");
}

ZENITH_TEST(ZM_NpcWalker, Walker_ArrivalAdvancesIndex)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 1.0f, 2.0f, 3.0f }, { 7.0f, 8.0f, 9.0f });
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, xWaypoints.m_axPoints[0], fWALKER_TEST_DT,
		false, MakeTuning());

	ZENITH_ASSERT_TRUE(xStep.m_bArrivedThisStep,
		"entering the inclusive arrival radius must report one arrival beat");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 1u,
		"arrival at waypoint zero must advance to waypoint one");
	AssertStopped(xStep, "arrival beat");
}

ZENITH_TEST(ZM_NpcWalker, Walker_ArrivalStartsDwell)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 2.0f, 0.0f, 0.0f }, { -2.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};
	const ZM_WalkerTuning xTuning = MakeTuning(2.0f, 0.25f, 3.0f);

	ZM_StepWalker(xWaypoints, xState, xWaypoints.m_axPoints[0],
		fWALKER_TEST_DT, false, xTuning);

	ZENITH_ASSERT_EQ_FLOAT(xState.m_fDwellRemaining, 3.0f,
		fWALKER_TEST_EPSILON,
		"arrival must install the full authored dwell, not consume dt from it immediately");
}

ZENITH_TEST(ZM_NpcWalker, Walker_DwellSuppressesMotion)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 5.0f, 0.0f, 0.0f }, { -5.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};
	xState.m_fDwellRemaining = 0.75f;

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, 0.0f, 0.0f }, 0.25f,
		false, MakeTuning());

	AssertStopped(xStep, "active dwell");
	ZENITH_ASSERT_FALSE(xStep.m_bArrivedThisStep,
		"waiting out a dwell is not a second arrival");
	ZENITH_ASSERT_EQ_FLOAT(xState.m_fDwellRemaining, 0.5f,
		fWALKER_TEST_EPSILON, "an unhalted dwell must consume exactly dt");
}

ZENITH_TEST(ZM_NpcWalker, Walker_DwellExpiresAndResumes)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 4.0f, 0.0f, 0.0f }, { -4.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};
	xState.m_fDwellRemaining = 0.05f;
	const ZM_WalkerTuning xTuning = MakeTuning(1.75f);

	const ZM_WalkerStep xExpiryStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, 0.0f, 0.0f }, 0.1f,
		false, xTuning);
	AssertStopped(xExpiryStep, "dwell-expiry frame");
	ZENITH_ASSERT_EQ_FLOAT(xState.m_fDwellRemaining, 0.0f,
		fWALKER_TEST_EPSILON, "an overlong dt must clamp the dwell at zero");

	const ZM_WalkerStep xResumeStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, 0.0f, 0.0f }, 0.1f,
		false, xTuning);
	ZENITH_ASSERT_GT(xResumeStep.m_fSpeed, 0.0f,
		"the first frame after dwell expiry must resume patrol motion");
	ZENITH_ASSERT_NEAR_VEC3(xResumeStep.m_xDirXZ,
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), fWALKER_TEST_EPSILON,
		"the resumed step must still head toward the current target");
}

ZENITH_TEST(ZM_NpcWalker, Walker_IndexWrapsToZeroAtEnd)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ -3.0f, 0.0f, 0.0f }, { 3.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};
	xState.m_uTargetIndex = 1u;

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, xWaypoints.m_axPoints[1], fWALKER_TEST_DT,
		false, MakeTuning());

	ZENITH_ASSERT_TRUE(xStep.m_bArrivedThisStep,
		"the last waypoint must still report an arrival");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 0u,
		"the cursor must wrap from the final waypoint to zero");
}

ZENITH_TEST(ZM_NpcWalker, Walker_HaltedProducesNoMotionAndDoesNotAdvance)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 0.0f, 0.0f, 0.0f }, { 5.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, xWaypoints.m_axPoints[0], fWALKER_TEST_DT,
		true, MakeTuning());

	AssertStopped(xStep, "halted patrol");
	ZENITH_ASSERT_FALSE(xStep.m_bArrivedThisStep,
		"a halted walker must not consume an arrival underneath a dialogue");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 0u,
		"halt must leave the waypoint cursor unchanged even while standing on target");
}

ZENITH_TEST(ZM_NpcWalker, Walker_HaltDoesNotConsumeDwellTimer)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 5.0f, 0.0f, 0.0f }, { -5.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};
	xState.m_fDwellRemaining = 1.25f;

	for (u_int u = 0u; u < 60u; ++u)
	{
		const ZM_WalkerStep xStep = ZM_StepWalker(
			xWaypoints, xState, { 0.0f, 0.0f, 0.0f }, 1.0f / 60.0f,
			true, MakeTuning());
		AssertStopped(xStep, "halted dwell");
	}

	ZENITH_ASSERT_EQ_FLOAT(xState.m_fDwellRemaining, 1.25f,
		fWALKER_TEST_EPSILON,
		"opening a menu must PAUSE dwell time rather than silently consuming it");
}

ZENITH_TEST(ZM_NpcWalker, Walker_DirectionIgnoresYDifference)
{
	ZM_WalkerWaypoints xLow = MakeWaypoints(
		{ 3.0f, -1000.0f, 4.0f }, { 0.0f, 0.0f, 0.0f });
	ZM_WalkerWaypoints xHigh = xLow;
	xHigh.m_axPoints[0].y = 1000.0f;
	ZM_WalkerState xLowState{};
	ZM_WalkerState xHighState{};

	const ZM_WalkerStep xLowStep = ZM_StepWalker(
		xLow, xLowState, { 0.0f, -50.0f, 0.0f }, fWALKER_TEST_DT,
		false, MakeTuning());
	const ZM_WalkerStep xHighStep = ZM_StepWalker(
		xHigh, xHighState, { 0.0f, 75.0f, 0.0f }, fWALKER_TEST_DT,
		false, MakeTuning());

	ZENITH_ASSERT_NEAR_VEC3(xLowStep.m_xDirXZ, xHighStep.m_xDirXZ,
		fWALKER_TEST_EPSILON,
		"terrain-height differences must not tilt or rescale the horizontal patrol heading");
	ZENITH_ASSERT_EQ_FLOAT(xLowStep.m_xDirXZ.y, 0.0f,
		fWALKER_TEST_EPSILON, "the pure patrol direction must always be XZ-flat");
}

ZENITH_TEST(ZM_NpcWalker, Walker_ArriveRadiusBoundaryExactAccepted)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 1.0f, 400.0f, 0.0f }, { 3.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, -400.0f, 0.0f }, fWALKER_TEST_DT,
		false, MakeTuning(2.0f, 1.0f));

	ZENITH_ASSERT_TRUE(xStep.m_bArrivedThisStep,
		"distance exactly equal to arrive radius must be accepted inclusively");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 1u,
		"the exact-boundary arrival must advance the cursor");
}

ZENITH_TEST(ZM_NpcWalker, Walker_ArriveRadiusJustOutsideRejected)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 1.001f, 0.0f, 0.0f }, { 3.0f, 0.0f, 0.0f });
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, { 0.0f, 0.0f, 0.0f }, fWALKER_TEST_DT,
		false, MakeTuning(2.0f, 1.0f));

	ZENITH_ASSERT_FALSE(xStep.m_bArrivedThisStep,
		"a point just outside the radius must remain a movement step");
	ZENITH_ASSERT_GT(xStep.m_fSpeed, 0.0f,
		"a just-outside point must request motion rather than parking");
	ZENITH_ASSERT_EQ(xState.m_uTargetIndex, 0u,
		"a just-outside point must not advance the cursor");
}

ZENITH_TEST(ZM_NpcWalker, Walker_ClonedStateSequenceIsFieldExact)
{
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(
		{ 3.0f, 8.0f, 4.0f }, { -3.0f, -8.0f, -4.0f });
	ZM_WalkerState xStateA{};
	ZM_WalkerState xStateB = xStateA;
	const ZM_WalkerTuning xTuning = MakeTuning(2.25f, 0.2f, 0.75f);

	// A short cloned-state sequence exercises movement, arrival, dwell and halt.
	// Every field is compared EXACTLY after every call. This does not claim a
	// bytewise object comparison (padding is outside the test framework's reach),
	// but any hidden RNG or implicit process state changes a named field and fails.
	const Zenith_Maths::Vector3 axPositions[] = {
		{ 1.0f, 99.0f, 1.0f },
		{ 2.0f, -5.0f, 3.0f },
		{ 3.0f, 0.0f, 4.0f },   // arrive at waypoint zero; cursor advances
		{ 3.0f, 4.0f, 4.0f },   // dwell
		{ 3.0f, 4.0f, 4.0f },   // halted dwell (timer must freeze identically)
		{ 3.0f, 4.0f, 4.0f },
		{ 3.0f, 4.0f, 4.0f },
		{ 3.0f, 4.0f, 4.0f },
	};
	const float afDt[] = {
		0.1f, 0.1f, 0.1f, 0.2f, 0.25f, 0.2f, 0.2f, 0.2f,
	};
	const bool abHalted[] = {
		false, false, false, false, true, false, false, false,
	};
	constexpr u_int uPOSITION_COUNT =
		(u_int)(sizeof(axPositions) / sizeof(axPositions[0]));
	constexpr u_int uDT_COUNT = (u_int)(sizeof(afDt) / sizeof(afDt[0]));
	constexpr u_int uHALTED_COUNT =
		(u_int)(sizeof(abHalted) / sizeof(abHalted[0]));
	static_assert(uPOSITION_COUNT == uDT_COUNT);
	static_assert(uPOSITION_COUNT == uHALTED_COUNT);

	for (u_int u = 0u; u < uPOSITION_COUNT; ++u)
	{
		const ZM_WalkerStep xStepA = ZM_StepWalker(
			xWaypoints, xStateA, axPositions[u], afDt[u], abHalted[u], xTuning);
		const ZM_WalkerStep xStepB = ZM_StepWalker(
			xWaypoints, xStateB, axPositions[u], afDt[u], abHalted[u], xTuning);

		ZENITH_ASSERT_EQ(xStepA.m_xDirXZ.x, xStepB.m_xDirXZ.x,
			"step %u: cloned-state direction.x must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStepA.m_xDirXZ.y, xStepB.m_xDirXZ.y,
			"step %u: cloned-state direction.y must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStepA.m_xDirXZ.z, xStepB.m_xDirXZ.z,
			"step %u: cloned-state direction.z must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStepA.m_fSpeed, xStepB.m_fSpeed,
			"step %u: cloned-state speed must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStepA.m_bArrivedThisStep, xStepB.m_bArrivedThisStep,
			"step %u: cloned-state arrival bit must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStateA.m_uTargetIndex, xStateB.m_uTargetIndex,
			"step %u: cloned-state cursor must be exactly equal", u);
		ZENITH_ASSERT_EQ(xStateA.m_fDwellRemaining, xStateB.m_fDwellRemaining,
			"step %u: cloned-state dwell must be exactly equal", u);
	}
}

ZENITH_TEST(ZM_NpcWalker, Walker_CoincidentPositionProducesZeroDirectionNotNaN)
{
	const Zenith_Maths::Vector3 xPoint(4.0f, -20.0f, 7.0f);
	ZM_WalkerWaypoints xWaypoints = MakeWaypoints(xPoint, {}, 1u);
	ZM_WalkerState xState{};

	const ZM_WalkerStep xStep = ZM_StepWalker(
		xWaypoints, xState, xPoint, fWALKER_TEST_DT, false, MakeTuning());

	ZENITH_ASSERT_TRUE(IsFinite(xStep.m_xDirXZ),
		"coincident current/target positions must never normalise zero into NaN");
	AssertStopped(xStep, "coincident waypoint");
}

ZENITH_TEST(ZM_NpcWalker, PatrolVelocity_XZMatchesDirectionTimesSpeed)
{
	const Zenith_Maths::Vector3 xVelocity = ZM_BuildPatrolVelocity(
		{ 0.6f, 123.0f, -0.8f }, 2.5f, { 9.0f, -6.25f, 8.0f });

	ZENITH_ASSERT_EQ_FLOAT(xVelocity.x, 1.5f, fWALKER_TEST_EPSILON,
		"patrol X velocity must be direction.x * speed");
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.z, -2.0f, fWALKER_TEST_EPSILON,
		"patrol Z velocity must be direction.z * speed");
}

ZENITH_TEST(ZM_NpcWalker, PatrolVelocity_PreservesCurrentY)
{
	const Zenith_Maths::Vector3 xVelocity = ZM_BuildPatrolVelocity(
		{ 1.0f, 0.0f, 0.0f }, 3.0f, { 0.0f, -7.125f, 0.0f });

	ZENITH_ASSERT_EQ_FLOAT(xVelocity.y, -7.125f, fWALKER_TEST_EPSILON,
		"the motor must preserve vertical velocity verbatim so gravity and terrain following continue");
}

ZENITH_TEST(ZM_NpcWalker, PatrolVelocity_ZeroDirectionYieldsZeroXZAndKeepsY)
{
	const Zenith_Maths::Vector3 xVelocity = ZM_BuildPatrolVelocity(
		Zenith_Maths::Vector3(0.0f), 9.0f, { 4.0f, 2.75f, -8.0f });

	ZENITH_ASSERT_NEAR_VEC3(xVelocity,
		Zenith_Maths::Vector3(0.0f, 2.75f, 0.0f), fWALKER_TEST_EPSILON,
		"parking a walker must zero only horizontal velocity and preserve Y");
}
