// Unit tests for the autonomous-tennis cores. Included at the bottom of
// RenderTest.cpp so the ZENITH_TEST cases land in the RenderTest test runner.
//
// Phase 0 section: the PURE logic headers (Types/CourtGeometry/Rng/Spin/Decision)
// — engine-free, headless. Later phases append engine-bound tests (player getters,
// brain relocation/serialisation, BT leaves, the integration fixture) below.

#include "RenderTest/Components/RenderTest_TennisTypes.h"
#include "RenderTest/Components/RenderTest_TennisCourtGeometry.h"
#include "RenderTest/Components/RenderTest_TennisRng.h"
#include "RenderTest/Components/RenderTest_TennisSpin.h"
#include "RenderTest/Components/RenderTest_TennisDecision.h"
#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_TennisBTNodes.h"
#include "RenderTest/Components/RenderTest_TennisTelemetry.h"
#include "RenderTest/Components/RenderTest_TennisMatchComponent.h"
#include "RenderTest/RenderTest_Tennis.h"

#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "DataStream/Zenith_DataStream.h"

#include <cmath>
#include <cstdio>
#include <utility>

using namespace RenderTest_Tennis;

// ============================================================================
// CourtGeometry
// ============================================================================

ZENITH_TEST(RenderTestTennis, DefaultCourtMatchesAuthoredConstants)
{
	const TennisCourt xC = DefaultCourt();
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fCenterX,          RenderTest_Tennis::fCOURT_CX, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fNetZ,             RenderTest_Tennis::fCOURT_CZ, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fSurfaceY,         RenderTest_Tennis::fSURFACE_Y, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fSinglesHalfWidth, RenderTest_Tennis::fHALF_WIDTH - 1.37f, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fDoublesHalfWidth, RenderTest_Tennis::fHALF_WIDTH, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fHalfLength,       RenderTest_Tennis::fHALF_LENGTH, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fSlabHalfWidth,    RenderTest_Tennis::fSLAB_HALF_WIDTH, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fSlabHalfLength,   RenderTest_Tennis::fSLAB_HALF_LENGTH, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fServiceLineOffset,RenderTest_Tennis::fSERVICE_LINE_OFFSET, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fNetHeight,        RenderTest_Tennis::fNET_HEIGHT, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xC.m_fBallRadius,       RenderTest_Tennis::fBALL_RADIUS, 1e-3f);
}

ZENITH_TEST(RenderTestTennis, SideAndBaselineRoundTrip)
{
	const TennisCourt xC = DefaultCourt();
	ZENITH_ASSERT_EQ(static_cast<int>(OtherSide(TENNIS_SIDE_NEAR)), static_cast<int>(TENNIS_SIDE_FAR));
	ZENITH_ASSERT_EQ(static_cast<int>(OtherSide(TENNIS_SIDE_FAR)), static_cast<int>(TENNIS_SIDE_NEAR));
	ZENITH_ASSERT_EQ(OtherSideIndex(0), 1);
	ZENITH_ASSERT_EQ(OtherSideIndex(1), 0);
	ZENITH_ASSERT_LT(xC.BaselineZ(TENNIS_SIDE_NEAR), xC.m_fNetZ);
	ZENITH_ASSERT_GT(xC.BaselineZ(TENNIS_SIDE_FAR), xC.m_fNetZ);
	ZENITH_ASSERT_EQ(static_cast<int>(xC.SideOfZ(xC.m_fNetZ - 1.0f)), static_cast<int>(TENNIS_SIDE_NEAR));
	ZENITH_ASSERT_EQ(static_cast<int>(xC.SideOfZ(xC.m_fNetZ + 1.0f)), static_cast<int>(TENNIS_SIDE_FAR));
}

ZENITH_TEST(RenderTestTennis, IsInBoundsSinglesLines)
{
	const TennisCourt xC = DefaultCourt();
	// Centre of the far half: in.
	ZENITH_ASSERT_TRUE(IsInBounds(xC, Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f), TENNIS_SIDE_FAR));
	// Long (past the far baseline): out.
	ZENITH_ASSERT_FALSE(IsInBounds(xC, Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ + xC.m_fHalfLength + 1.0f), TENNIS_SIDE_FAR));
	// Wide (past the singles sideline + radius): out.
	ZENITH_ASSERT_FALSE(IsInBounds(xC, Zenith_Maths::Vector3(xC.m_fCenterX + xC.m_fSinglesHalfWidth + 1.0f, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f), TENNIS_SIDE_FAR));
	// Within a ball-radius of the sideline: still in.
	ZENITH_ASSERT_TRUE(IsInBounds(xC, Zenith_Maths::Vector3(xC.m_fCenterX + xC.m_fSinglesHalfWidth + xC.m_fBallRadius * 0.5f, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f), TENNIS_SIDE_FAR));
}

ZENITH_TEST(RenderTestTennis, ServiceBoxCorrectDiagonal)
{
	const TennisCourt xC = DefaultCourt();
	// Near server, deuce court -> far receiver's -X box, between net and far service line.
	const Zenith_Maths::Vector3 xDeuce(xC.m_fCenterX - 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 3.0f);
	ZENITH_ASSERT_TRUE (IsInServiceBox(xC, xDeuce, TENNIS_SIDE_NEAR, /*deuce*/ true));
	// Same point is NOT in the ad box (wrong-court rejection).
	ZENITH_ASSERT_FALSE(IsInServiceBox(xC, xDeuce, TENNIS_SIDE_NEAR, /*deuce*/ false));
	// Service-long (past the service line): out of the box.
	ZENITH_ASSERT_FALSE(IsInServiceBox(xC, Zenith_Maths::Vector3(xC.m_fCenterX - 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + xC.m_fServiceLineOffset + 2.0f), TENNIS_SIDE_NEAR, true));
	// Service-wide.
	ZENITH_ASSERT_FALSE(IsInServiceBox(xC, Zenith_Maths::Vector3(xC.m_fCenterX - xC.m_fSinglesHalfWidth - 1.0f, xC.m_fSurfaceY, xC.m_fNetZ + 3.0f), TENNIS_SIDE_NEAR, true));
	// Near server, ad court -> far +X box.
	ZENITH_ASSERT_TRUE (IsInServiceBox(xC, Zenith_Maths::Vector3(xC.m_fCenterX + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 3.0f), TENNIS_SIDE_NEAR, /*deuce*/ false));
	// Far server, deuce court -> near receiver's +X box (between near service line and net).
	ZENITH_ASSERT_TRUE (IsInServiceBox(xC, Zenith_Maths::Vector3(xC.m_fCenterX + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ - 3.0f), TENNIS_SIDE_FAR, /*deuce*/ true));
	ZENITH_ASSERT_FALSE(IsInServiceBox(xC, Zenith_Maths::Vector3(xC.m_fCenterX + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ - 3.0f), TENNIS_SIDE_FAR, /*deuce*/ false));
}

ZENITH_TEST(RenderTestTennis, ProjectToSlabClamps)
{
	const TennisCourt xC = DefaultCourt();
	const Zenith_Maths::Vector3 xFar(xC.m_fCenterX + 100.0f, 99.0f, xC.m_fNetZ + 100.0f);
	const Zenith_Maths::Vector3 xClamped = ProjectToSlab(xC, xFar);
	ZENITH_ASSERT_EQ_FLOAT(xClamped.x, xC.m_fCenterX + xC.m_fSlabHalfWidth, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xClamped.z, xC.m_fNetZ + xC.m_fSlabHalfLength, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xClamped.y, 99.0f, 1e-3f);   // Y untouched
	// An in-slab point is unchanged.
	const Zenith_Maths::Vector3 xIn(xC.m_fCenterX, 70.0f, xC.m_fNetZ);
	const Zenith_Maths::Vector3 xInOut = ProjectToSlab(xC, xIn);
	ZENITH_ASSERT_NEAR_VEC3(xInOut, xIn, 1e-3f);
}

// Plan unit-test #8: nav destinations clamp to the ERODED slab (fSlabHalf - margin),
// so SetDestination doesn't fail-stop on the very edge of the generated navmesh.
ZENITH_TEST(RenderTestTennis, ProjectToSlabErodesByMargin)
{
	const TennisCourt xC = DefaultCourt();
	const float fMargin = 1.0f;
	const Zenith_Maths::Vector3 xFar(xC.m_fCenterX + 100.0f, 99.0f, xC.m_fNetZ + 100.0f);
	const Zenith_Maths::Vector3 xEroded = ProjectToSlab(xC, xFar, fMargin);
	// Clamped strictly inside the un-eroded edge, by exactly the margin.
	ZENITH_ASSERT_EQ_FLOAT(xEroded.x, xC.m_fCenterX + xC.m_fSlabHalfWidth  - fMargin, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xEroded.z, xC.m_fNetZ    + xC.m_fSlabHalfLength - fMargin, 1e-3f);
	ZENITH_ASSERT_LT(xEroded.x, xC.m_fCenterX + xC.m_fSlabHalfWidth);
	ZENITH_ASSERT_LT(xEroded.z, xC.m_fNetZ    + xC.m_fSlabHalfLength);
	// A pathological margin can't invert the bounds (floored at the centre/net line).
	const Zenith_Maths::Vector3 xHuge = ProjectToSlab(xC, xFar, 1.0e6f);
	ZENITH_ASSERT_EQ_FLOAT(xHuge.x, xC.m_fCenterX, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xHuge.z, xC.m_fNetZ, 1e-3f);
}

ZENITH_TEST(RenderTestTennis, NetPlaneCrossingAndClearance)
{
	const TennisCourt xC = DefaultCourt();
	ZENITH_ASSERT_TRUE (CrossedNetPlane(xC.m_fNetZ - 1.0f, xC.m_fNetZ + 1.0f, xC.m_fNetZ));
	ZENITH_ASSERT_FALSE(CrossedNetPlane(xC.m_fNetZ - 2.0f, xC.m_fNetZ - 1.0f, xC.m_fNetZ));
	ZENITH_ASSERT_TRUE (ClearsNet(xC.m_fSurfaceY + xC.m_fNetHeight + 0.5f, xC.m_fSurfaceY + xC.m_fNetHeight));
	ZENITH_ASSERT_FALSE(ClearsNet(xC.m_fSurfaceY + 0.2f, xC.m_fSurfaceY + xC.m_fNetHeight));

	// Interpolated crossing height: prev below->above straddling the net midway.
	const Zenith_Maths::Vector3 xPrev(xC.m_fCenterX, xC.m_fSurfaceY + 1.0f, xC.m_fNetZ - 1.0f);
	const Zenith_Maths::Vector3 xCur (xC.m_fCenterX, xC.m_fSurfaceY + 3.0f, xC.m_fNetZ + 1.0f);
	const TennisNetCrossing xX = NetCrossingClearance(xPrev, xCur, xC.m_fNetZ);
	ZENITH_ASSERT_TRUE(xX.m_bCrossed);
	ZENITH_ASSERT_EQ_FLOAT(xX.m_fHeightAtCross, xC.m_fSurfaceY + 2.0f, 1e-3f);   // midpoint height
	// A segment that never straddles the net reports no crossing.
	const TennisNetCrossing xNo = NetCrossingClearance(xPrev, Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY + 2.0f, xC.m_fNetZ - 0.5f), xC.m_fNetZ);
	ZENITH_ASSERT_FALSE(xNo.m_bCrossed);
}

// ============================================================================
// Rng
// ============================================================================

ZENITH_TEST(RenderTestTennis, RngRangeAndDeterminism)
{
	TennisRng xA(0xABCDEFu);
	TennisRng xB(0xABCDEFu);
	for (int i = 0; i < 64; ++i)
	{
		const float fUa = xA.NextUnit();
		const float fUb = xB.NextUnit();
		ZENITH_ASSERT_EQ_FLOAT(fUa, fUb, 1e-9f);     // same seed -> identical stream
		ZENITH_ASSERT_GE(fUa, 0.0f);
		ZENITH_ASSERT_LT(fUa, 1.0f);
	}
	// Different seed -> diverges.
	TennisRng xC(0x111111u), xD(0x222222u);
	ZENITH_ASSERT_NE(xC.Next(), xD.Next());
	// Signed range.
	TennisRng xE(0x9u);
	for (int i = 0; i < 64; ++i)
	{
		const float fS = xE.NextSigned();
		ZENITH_ASSERT_GE(fS, -1.0f);
		ZENITH_ASSERT_LT(fS, 1.0f);
	}
	// Disc radius.
	TennisRng xF(0x55u);
	for (int i = 0; i < 64; ++i)
	{
		const Zenith_Maths::Vector2 xP = xF.NextInDisc(2.0f);
		ZENITH_ASSERT_LE(std::sqrt(xP.x * xP.x + xP.y * xP.y), 2.0f + 1e-4f);
	}
}

// ============================================================================
// Spin
// ============================================================================

ZENITH_TEST(RenderTestTennis, MagnusSignsAndProperties)
{
	const Zenith_Maths::Vector3 xVelFwd(0.0f, 0.0f, 8.0f);   // +Z
	const Zenith_Maths::Vector3 xTop   = SpinAngVelForShot(TENNIS_SHOT_TYPE_TOPSPIN, Zenith_Maths::Vector3(0,0,1), 12.0f);
	const Zenith_Maths::Vector3 xSlice = SpinAngVelForShot(TENNIS_SHOT_TYPE_SLICE,   Zenith_Maths::Vector3(0,0,1), 12.0f);

	// Topspin dips (-Y), backspin lifts (+Y).
	ZENITH_ASSERT_LT(MagnusDeltaV(xVelFwd, xTop,   0.012f, 1.0f).y, 0.0f);
	ZENITH_ASSERT_GT(MagnusDeltaV(xVelFwd, xSlice, 0.012f, 1.0f).y, 0.0f);

	// Sidespin (about +Y) deflects in X, not Y.
	const Zenith_Maths::Vector3 xSide(0.0f, 10.0f, 0.0f);
	const Zenith_Maths::Vector3 xDvSide = MagnusDeltaV(xVelFwd, xSide, 0.012f, 1.0f);
	ZENITH_ASSERT_GT(std::fabs(xDvSide.x), 0.01f);
	ZENITH_ASSERT_LT(std::fabs(xDvSide.y), 1e-4f);

	// Zero when spin is parallel to velocity.
	const Zenith_Maths::Vector3 xDvPar = MagnusDeltaV(xVelFwd, Zenith_Maths::Vector3(0, 0, 5), 0.012f, 1.0f);
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xDvPar), 1e-5f);

	// Linear in dt.
	const Zenith_Maths::Vector3 xDv1 = MagnusDeltaV(xVelFwd, xTop, 0.012f, 0.02f);
	const Zenith_Maths::Vector3 xDv2 = MagnusDeltaV(xVelFwd, xTop, 0.012f, 0.04f);
	ZENITH_ASSERT_NEAR_VEC3(xDv2, xDv1 * 2.0f, 1e-5f);
}

ZENITH_TEST(RenderTestTennis, SpinAngVelAxisAndDecay)
{
	const Zenith_Maths::Vector3 xDir(0, 0, 1);
	const Zenith_Maths::Vector3 xTopAxis = Zenith_Maths::Cross(Zenith_Maths::Vector3(0, 1, 0), xDir);
	const Zenith_Maths::Vector3 xTop   = SpinAngVelForShot(TENNIS_SHOT_TYPE_TOPSPIN, xDir, 12.0f);
	const Zenith_Maths::Vector3 xSlice = SpinAngVelForShot(TENNIS_SHOT_TYPE_SLICE,   xDir, 12.0f);
	const Zenith_Maths::Vector3 xFlat  = SpinAngVelForShot(TENNIS_SHOT_TYPE_FLAT,    xDir, 12.0f);
	ZENITH_ASSERT_GT(Zenith_Maths::Dot(xTop,   xTopAxis), 0.0f);   // topspin along +axis
	ZENITH_ASSERT_LT(Zenith_Maths::Dot(xSlice, xTopAxis), 0.0f);   // slice opposite
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xFlat), Zenith_Maths::Length(xTop));   // flat is the weakest

	// Decay: shrinks magnitude, never flips sign, clamps to zero on a huge dt.
	const Zenith_Maths::Vector3 xS(0.0f, 12.0f, 0.0f);
	const Zenith_Maths::Vector3 xD = ApplySpinDecay(xS, 0.25f, 0.1f);
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xD), Zenith_Maths::Length(xS));
	ZENITH_ASSERT_GT(xD.y, 0.0f);
	const Zenith_Maths::Vector3 xZeroed = ApplySpinDecay(xS, 0.25f, 100.0f);
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xZeroed), 1e-5f);
}

ZENITH_TEST(RenderTestTennis, BounceVelocitySpinEffects)
{
	const Zenith_Maths::Vector3 xVelIn(0.0f, -10.0f, 8.0f);   // descending, moving +Z
	const Zenith_Maths::Vector3 xNoSpin(0.0f);
	const Zenith_Maths::Vector3 xTop   = SpinAngVelForShot(TENNIS_SHOT_TYPE_TOPSPIN, Zenith_Maths::Vector3(0,0,1), 16.0f);
	const Zenith_Maths::Vector3 xSlice = SpinAngVelForShot(TENNIS_SHOT_TYPE_SLICE,   Zenith_Maths::Vector3(0,0,1), 16.0f);

	const Zenith_Maths::Vector3 xFlat  = BounceVelocity(xVelIn, xNoSpin, 0.7f, 0.25f, 0.35f, 0.30f);
	// vy flips and scales by restitution; horizontal retains (1-friction).
	ZENITH_ASSERT_EQ_FLOAT(xFlat.y, 7.0f, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(xFlat.z, 8.0f * 0.75f, 1e-3f);

	const Zenith_Maths::Vector3 xTopB   = BounceVelocity(xVelIn, xTop,   0.7f, 0.25f, 0.35f, 0.30f);
	const Zenith_Maths::Vector3 xSliceB = BounceVelocity(xVelIn, xSlice, 0.7f, 0.25f, 0.35f, 0.30f);
	ZENITH_ASSERT_GT(xTopB.z, xFlat.z);     // topspin kicks forward
	ZENITH_ASSERT_GT(xTopB.y, xFlat.y);     // ...and higher
	ZENITH_ASSERT_LT(xSliceB.y, xFlat.y);   // slice stays low
}

// ============================================================================
// Decision
// ============================================================================

namespace
{
	// A representative near-side receiver state at the baseline centre.
	inline TennisPlayerState MakeNearState()
	{
		const TennisCourt xC = DefaultCourt();
		TennisPlayerState xS;
		xS.m_eMySide = TENNIS_SIDE_NEAR;
		xS.m_xMyPos  = Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.BaselineZ(TENNIS_SIDE_NEAR));
		xS.m_xOppPos = Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.BaselineZ(TENNIS_SIDE_FAR));
		xS.m_fBalance = 1.0f;
		return xS;
	}
}

ZENITH_TEST(RenderTestTennis, InBoundsMarginCenterSaferThanLine)
{
	// InBoundsMargin (a live SelectShot input): a central aim has a larger in-bounds
	// margin than one hugging the singles sideline.
	const TennisCourt xC = DefaultCourt();
	const float fCtr  = InBoundsMargin(xC, Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f), TENNIS_SIDE_FAR);
	const float fEdge = InBoundsMargin(xC, Zenith_Maths::Vector3(xC.m_fCenterX + xC.m_fSinglesHalfWidth - 0.1f, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f), TENNIS_SIDE_FAR);
	ZENITH_ASSERT_GT(fCtr, fEdge);
}

ZENITH_TEST(RenderTestTennis, AggressionResponds)
{
	TennisPlayerState xLead = MakeNearState();   xLead.m_iMyPoints = 3; xLead.m_iOppPoints = 0;
	TennisPlayerState xTrail = MakeNearState();  xTrail.m_iMyPoints = 0; xTrail.m_iOppPoints = 3;
	ZENITH_ASSERT_GT(ComputeAggression(xLead, 0.5f), ComputeAggression(xTrail, 0.5f));        // lead -> bolder
	ZENITH_ASSERT_GT(ComputeAggression(xLead, 0.9f), ComputeAggression(xLead, 0.1f));         // inside baseline -> bolder
	TennisPlayerState xOff = MakeNearState();  xOff.m_fBalance = 0.1f;
	TennisPlayerState xSet = MakeNearState();  xSet.m_fBalance = 1.0f;
	ZENITH_ASSERT_GT(ComputeAggression(xSet, 0.5f), ComputeAggression(xOff, 0.5f));           // off-balance -> safer
}

ZENITH_TEST(RenderTestTennis, CourtPositionFactorRisesTowardNet)
{
	// CourtPositionFactor (a live SelectShot/aggression input): higher when crept up to
	// the net, ~0 standing at the baseline.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xAtNet = MakeNearState();
	xAtNet.m_xMyPos.z = xC.m_fNetZ - 1.0f;                  // up at the net
	const TennisPlayerState xAtBaseline = MakeNearState();  // default: at the baseline
	ZENITH_ASSERT_GT(CourtPositionFactor(xC, xAtNet), CourtPositionFactor(xC, xAtBaseline));
}

// Situational shot selection (realistic tactics). Each branch picks a distinct shot.
ZENITH_TEST(RenderTestTennis, SelectShotOpenCourtWinnerWhenInControl)
{
	// In control (high balance) + opponent pulled wide to +X -> FLAT winner into the
	// OPEN (-X) court, away from the opponent. Deterministic for a fixed seed.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xS = MakeNearState();        // balance 1.0
	xS.m_xOppPos.x = xC.m_fCenterX + 4.0f;         // opponent pulled wide to +X
	TennisRng xR1(0x1234567u), xR2(0x1234567u);
	const TennisShotDecision xD1 = SelectShot(xC, xS, xR1);
	const TennisShotDecision xD2 = SelectShot(xC, xS, xR2);
	ZENITH_ASSERT_LT(xD1.m_xAim.x, xC.m_fCenterX);                                     // into the open court
	ZENITH_ASSERT_EQ(static_cast<int>(xD1.m_eType), static_cast<int>(TENNIS_SHOT_TYPE_FLAT));
	ZENITH_ASSERT_FALSE(xD1.m_bArmed);
	ZENITH_ASSERT_NEAR_VEC3(xD1.m_xAim, xD2.m_xAim, 1e-4f);                            // deterministic
}

ZENITH_TEST(RenderTestTennis, SelectShotDefensiveSliceWhenStretched)
{
	// Stretched (low balance) -> defensive SLICE deep + central to reset the point.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xS = MakeNearState();
	xS.m_fBalance = 0.2f;
	TennisRng xR(0x1234567u);
	const TennisShotDecision xD = SelectShot(xC, xS, xR);
	ZENITH_ASSERT_EQ(static_cast<int>(xD.m_eType), static_cast<int>(TENNIS_SHOT_TYPE_SLICE));
	ZENITH_ASSERT_GT(xD.m_xAim.z, xC.m_fNetZ + xC.m_fHalfLength * 0.5f);              // deep on the opponent's half
}

ZENITH_TEST(RenderTestTennis, SelectShotLobsTheNetRusher)
{
	// Opponent crept inside the service line -> LOB deep over them.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xS = MakeNearState();
	xS.m_xOppPos.z = xC.m_fNetZ + 3.0f;             // opponent at the net (inside service line)
	TennisRng xR(0x1234567u);
	const TennisShotDecision xD = SelectShot(xC, xS, xR);
	ZENITH_ASSERT_EQ(static_cast<int>(xD.m_eType), static_cast<int>(TENNIS_SHOT_TYPE_LOB));
	ZENITH_ASSERT_GT(xD.m_xAim.z, xC.m_fNetZ + xC.m_fHalfLength * 0.5f);              // deep, over the rusher
}

ZENITH_TEST(RenderTestTennis, SelectShotNeutralTopspinRally)
{
	// Neutral (moderate balance, opponent at the baseline centre) -> TOPSPIN deep: the
	// staple rally shot, placed moderately so the rally sustains.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xS = MakeNearState();
	xS.m_fBalance = 0.5f;                            // neither stretched nor in full control
	TennisRng xR(0x1234567u);
	const TennisShotDecision xD = SelectShot(xC, xS, xR);
	ZENITH_ASSERT_EQ(static_cast<int>(xD.m_eType), static_cast<int>(TENNIS_SHOT_TYPE_TOPSPIN));
	ZENITH_ASSERT_GT(xD.m_xAim.z, xC.m_fNetZ + xC.m_fHalfLength * 0.5f);              // deep
	ZENITH_ASSERT_FALSE(xD.m_bArmed);
}

ZENITH_TEST(RenderTestTennis, SelectShotDropAgainstDeepOpponent)
{
	// Set (balance in (0.60, 0.70]) + opponent parked deep behind their baseline -> the
	// occasional, RNG-gated DROP shot. The gate is NextUnit() < 0.30, so sweep seeds:
	// when it fires the shot must be a DROP landing SHORT (inside the service line); an
	// ungated draw must fall through to the neutral TOPSPIN rally.
	const TennisCourt xC = DefaultCourt();
	TennisPlayerState xS = MakeNearState();   // opponent at the far baseline (deep)
	xS.m_fBalance = 0.65f;

	bool bSawDrop = false;
	bool bSawTopspin = false;
	for (uint32_t uSeed = 1u; uSeed <= 64u; ++uSeed)
	{
		TennisRng xR(uSeed);
		const TennisShotDecision xD = SelectShot(xC, xS, xR);
		if (xD.m_eType == TENNIS_SHOT_TYPE_DROP)
		{
			bSawDrop = true;
			ZENITH_ASSERT_LT(std::fabs(xD.m_xAim.z - xC.m_fNetZ), xC.m_fServiceLineOffset);   // lands short
		}
		else if (xD.m_eType == TENNIS_SHOT_TYPE_TOPSPIN)
		{
			bSawTopspin = true;
		}
	}
	ZENITH_ASSERT_TRUE(bSawDrop);      // the DROP branch is reachable + correctly typed
	ZENITH_ASSERT_TRUE(bSawTopspin);   // ungated draws fall through to the rally shot
}

ZENITH_TEST(RenderTestTennis, ServeCourtParityAndServeLandsInBox)
{
	const TennisCourt xC = DefaultCourt();
	ZENITH_ASSERT_TRUE (ServeCourtIsDeuce(0, 0));   // even total -> deuce
	ZENITH_ASSERT_FALSE(ServeCourtIsDeuce(1, 0));   // odd -> ad
	ZENITH_ASSERT_TRUE (ServeCourtIsDeuce(2, 2));

	// Serves land in the parity-correct diagonal box, both sides, both attempts.
	const int aiSide[2] = { TENNIS_SIDE_NEAR, TENNIS_SIDE_FAR };
	for (int s = 0; s < 2; ++s)
	{
		TennisPlayerState xS = MakeNearState();
		xS.m_eMySide = static_cast<TennisSide>(aiSide[s]);
		xS.m_xMyPos.z = xC.BaselineZ(xS.m_eMySide);
		for (int d = 0; d < 2; ++d)
		{
			const bool bDeuce = (d == 0);
			for (int a = 0; a < 2; ++a)
			{
				const bool bSecond = (a == 1);
				TennisRng xR(0x2468ACEu);
				const TennisShotDecision xServe = SelectServe(xC, xS, SERVE_RESULT_GOOD, bDeuce, bSecond, xR);
				ZENITH_ASSERT_TRUE(IsInServiceBox(xC, xServe.m_xAim, xS.m_eMySide, bDeuce));
				ZENITH_ASSERT_FALSE(xServe.m_bArmed);
				// Aimed DEEP in the box (toward the service line) so the post-bounce serve
				// carries to the receiver — farther from the net than the box centre.
				const TennisBox xBox = xC.ServiceBox(xS.m_eMySide, bDeuce);
				const float fBoxCenterDepth = std::fabs((xBox.m_fMinZ + xBox.m_fMaxZ) * 0.5f - xC.m_fNetZ);
				ZENITH_ASSERT_GT(std::fabs(xServe.m_xAim.z - xC.m_fNetZ), fBoxCenterDepth);
			}
		}
	}
}

ZENITH_TEST(RenderTestTennis, PredictInterceptReachability)
{
	const TennisCourt xC = DefaultCourt();
	const float fStrike = StrikeHeight(xC);
	const Zenith_Maths::Vector3 xMyPos(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ - 12.0f);

	// A gentle ball descending toward me on the near side: reachable.
	const Zenith_Maths::Vector3 xBallPos(xC.m_fCenterX, xC.m_fSurfaceY + 4.0f, xC.m_fNetZ - 10.0f);
	const TennisInterceptResult xReach = PredictIntercept(xC, xBallPos, Zenith_Maths::Vector3(0, 0, -1.0f),
		Zenith_Maths::Vector3(0.0f), TENNIS_SIDE_NEAR, fStrike, 1.2f, xMyPos, 6.0f);
	ZENITH_ASSERT_TRUE(xReach.m_bReachable);
	ZENITH_ASSERT_LT(xReach.m_xStrikePoint.z, xC.m_fNetZ);   // on my side

	// Same ball flung fast across court: unreachable.
	const TennisInterceptResult xFar = PredictIntercept(xC, xBallPos, Zenith_Maths::Vector3(40.0f, 0, -1.0f),
		Zenith_Maths::Vector3(0.0f), TENNIS_SIDE_NEAR, fStrike, 1.2f, xMyPos, 6.0f);
	ZENITH_ASSERT_FALSE(xFar.m_bReachable);

	// Topspin dips the ball, so it reaches the strike plane sooner than with no spin.
	const Zenith_Maths::Vector3 xTop = SpinAngVelForShot(TENNIS_SHOT_TYPE_TOPSPIN, Zenith_Maths::Vector3(0, 0, -1), 14.0f);
	const TennisInterceptResult xNoSpin = PredictIntercept(xC, xBallPos, Zenith_Maths::Vector3(0, 2.0f, -3.0f),
		Zenith_Maths::Vector3(0.0f), TENNIS_SIDE_NEAR, fStrike, 1.2f, xMyPos, 6.0f);
	const TennisInterceptResult xSpin = PredictIntercept(xC, xBallPos, Zenith_Maths::Vector3(0, 2.0f, -3.0f),
		xTop, TENNIS_SIDE_NEAR, fStrike, 1.2f, xMyPos, 6.0f);
	ZENITH_ASSERT_TRUE(xNoSpin.m_bReachable && xSpin.m_bReachable);
	ZENITH_ASSERT_LT(xSpin.m_fTimeToStrike, xNoSpin.m_fTimeToStrike);
}

ZENITH_TEST(RenderTestTennis, PredictInterceptCatchesLowFlatDrive)
{
	// A flat drive that stays BELOW the strike height (1.0 m) but above the net
	// (0.91 m), descending toward me on my side. It never crosses DOWN through the
	// strike height from above, so only the low-band branch can catch it — this test
	// exercises that branch exclusively (reverting it makes the ball unreachable).
	const TennisCourt xC = DefaultCourt();
	const float fStrike = StrikeHeight(xC);                          // surface + 1.0
	const Zenith_Maths::Vector3 xBallPos(xC.m_fCenterX, xC.m_fSurfaceY + 0.9f, xC.m_fNetZ - 5.0f);  // y below strike, near side
	const Zenith_Maths::Vector3 xMyPos(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ - 6.0f);
	const TennisInterceptResult xHit = PredictIntercept(xC, xBallPos, Zenith_Maths::Vector3(0.0f, -0.3f, -2.0f),
		Zenith_Maths::Vector3(0.0f), TENNIS_SIDE_NEAR, fStrike, 1.5f, xMyPos, 6.0f);
	ZENITH_ASSERT_TRUE(xHit.m_bReachable);
	ZENITH_ASSERT_LT(xHit.m_xStrikePoint.z, xC.m_fNetZ);            // struck on my (near) side
	ZENITH_ASSERT_LE(xHit.m_xStrikePoint.y, fStrike + 1e-3f);       // in the low band, not above strike height
}

ZENITH_TEST(RenderTestTennis, LaunchVelocityRoundTripAndPace)
{
	const TennisCourt xC = DefaultCourt();
	const Zenith_Maths::Vector3 xFrom(xC.m_fCenterX, xC.m_fSurfaceY + 1.0f, xC.BaselineZ(TENNIS_SIDE_NEAR));
	const Zenith_Maths::Vector3 xAim (xC.m_fCenterX + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 8.0f);

	float fT = 0.0f;
	const Zenith_Maths::Vector3 xV0 = ComputeLaunchVelocity(xFrom, xAim, Zenith_Maths::Vector3(0.0f), 14.0f, fT);
	// Re-integrate with the solved velocity; it lands near the aim.
	Zenith_Maths::Vector3 xPos = xFrom, xVel = xV0;
	const float fDt = 1.0f / 120.0f;
	float fElapsed = 0.0f;
	while (fElapsed < fT) { IntegrateStep(xPos, xVel, Zenith_Maths::Vector3(0.0f), fDt, k_fDecisionMagnusK); fElapsed += fDt; }
	ZENITH_ASSERT_NEAR_VEC3(xPos, xAim, 0.75f);

	// Higher pace -> shorter flight time.
	float fSlow = 0.0f, fFast = 0.0f;
	ComputeLaunchVelocity(xFrom, xAim, Zenith_Maths::Vector3(0.0f), 10.0f, fSlow);
	ComputeLaunchVelocity(xFrom, xAim, Zenith_Maths::Vector3(0.0f), 16.0f, fFast);
	ZENITH_ASSERT_LT(fFast, fSlow);
}

ZENITH_TEST(RenderTestTennis, JitterMonotonicAndDeterministic)
{
	const Zenith_Maths::Vector3 xAim(10.0f, 0.0f, 20.0f);
	// Aim scatter grows with risk+difficulty (same seed -> identical unit sample).
	TennisRng xLowA(0x5u), xLowB(0x5u), xHigh(0x5u);
	const Zenith_Maths::Vector3 xLo1 = JitterAim(xAim, 0.1f, 0.1f, xLowA);
	const Zenith_Maths::Vector3 xLo2 = JitterAim(xAim, 0.1f, 0.1f, xLowB);
	const Zenith_Maths::Vector3 xHi  = JitterAim(xAim, 0.9f, 0.9f, xHigh);
	ZENITH_ASSERT_NEAR_VEC3(xLo1, xLo2, 1e-6f);   // deterministic
	ZENITH_ASSERT_GE(Zenith_Maths::Length(xHi - xAim), Zenith_Maths::Length(xLo1 - xAim));

	// Pace scatter magnitude grows with risk+difficulty.
	TennisRng xPa(0x7u), xPb(0x7u);
	const float fLoDev = std::fabs(JitterPace(12.0f, 0.1f, 0.1f, xPa) - 12.0f);
	const float fHiDev = std::fabs(JitterPace(12.0f, 0.9f, 0.9f, xPb) - 12.0f);
	ZENITH_ASSERT_GE(fHiDev, fLoDev);
}

ZENITH_TEST(RenderTestTennis, BalanceFootworkAndContact)
{
	// Balance: 1 at the ready spot, lower when stretched.
	const Zenith_Maths::Vector3 xReady(0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(ComputeBalance(xReady, xReady, 4.0f), 1.0f, 1e-4f);
	ZENITH_ASSERT_LT(ComputeBalance(Zenith_Maths::Vector3(2.0f, 0, 0), xReady, 4.0f), 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(ComputeBalance(Zenith_Maths::Vector3(4.0f, 0, 0), xReady, 4.0f), 0.0f, 1e-4f);

	// Footwork: clamped, signed toward the target.
	ZENITH_ASSERT_GT(ComputeFootworkVelocityX(0.0f, 5.0f, 6.0f), 0.0f);
	ZENITH_ASSERT_LT(ComputeFootworkVelocityX(5.0f, 0.0f, 6.0f), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(ComputeFootworkVelocityX(0.0f, 100.0f, 6.0f), 6.0f, 1e-4f);   // clamp +
	ZENITH_ASSERT_EQ_FLOAT(ComputeFootworkVelocityX(0.0f, -100.0f, 6.0f), -6.0f, 1e-4f); // clamp -
	ZENITH_ASSERT_TRUE(ShouldDriveFootwork(false));
	ZENITH_ASSERT_FALSE(ShouldDriveFootwork(true));

	// Goalward progress (P2): the PLAYER's displacement projected at the destination —
	// immune to the goal itself moving (the BT rewrites the destination every tick).
	const Zenith_Maths::Vector3 xAt(5.0f, 0.0f, 5.0f);
	// Reviewer's case: a STATIONARY (blocked) player scores 0 progress even as the goal
	// repeatedly slides closer — so distance-reduction can't fake advancement.
	ZENITH_ASSERT_EQ_FLOAT(ProgressTowardDestination(xAt, xAt, Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f)), 0.0f, 1e-4f);
	ZENITH_ASSERT_EQ_FLOAT(ProgressTowardDestination(xAt, xAt, Zenith_Maths::Vector3( 8.0f, 0.0f, 5.0f)), 0.0f, 1e-4f); // goal moved closer: still 0
	ZENITH_ASSERT_GT(ProgressTowardDestination(xAt, Zenith_Maths::Vector3(6.0f, 0.0f, 5.0f), Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f)), 0.9f); // stepped toward
	ZENITH_ASSERT_LT(ProgressTowardDestination(xAt, Zenith_Maths::Vector3(4.0f, 0.0f, 5.0f), Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f)), 0.0f); // stepped away
	ZENITH_ASSERT_EQ_FLOAT(ProgressTowardDestination(xAt, Zenith_Maths::Vector3(5.0f, 0.0f, 6.0f), Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f)), 0.0f, 1e-3f); // sideways

	// Nav stall (P2): not-reached + no goalward ADVANCE = stalled, REGARDLESS of HasPath.
	// The arg is the player's goalward displacement (above), not raw motion / distance
	// change — so blocked-in-place (incl. a goal sliding toward it) reads as a stall.
	ZENITH_ASSERT_TRUE (IsNavStalled(/*reached*/false, /*progress*/0.0f, 0.01f));  // no advance -> stalled
	ZENITH_ASSERT_TRUE (IsNavStalled(false,  0.005f, 0.01f));                       // negligible advance -> stalled
	ZENITH_ASSERT_TRUE (IsNavStalled(false, -0.5f,   0.01f));                       // moving AWAY/sideways -> stalled
	ZENITH_ASSERT_FALSE(IsNavStalled(false,  0.5f,   0.01f));                       // advancing -> progress
	ZENITH_ASSERT_FALSE(IsNavStalled(/*reached*/true, 0.0f, 0.01f));                // arrived + idle -> not stalled

	// Contact range.
	const Zenith_Maths::Vector3 xSpot(1.0f, 2.0f, 3.0f);
	ZENITH_ASSERT_TRUE (IsWithinContactRange(Zenith_Maths::Vector3(1.2f, 2.0f, 3.0f), xSpot, 0.4f));
	ZENITH_ASSERT_FALSE(IsWithinContactRange(Zenith_Maths::Vector3(2.0f, 2.0f, 3.0f), xSpot, 0.4f));
}

// Telemetry name resolvers (used by the JSON/CSV exporters): every defined event type
// and point reason must resolve to a non-null name, and the _Count sentinel must be null
// (so the exporter's "unknown -> numeric fallback" path is reachable).
ZENITH_TEST(RenderTestTennis, TelemetryNameResolversCoverEveryValue)
{
	using namespace RenderTest_TennisTelemetry;
	for (uint16_t u = 0; u < static_cast<uint16_t>(EventType::_Count); ++u)
		ZENITH_ASSERT_NOT_NULL(EventTypeToString(u), "event type resolves to a name");
	ZENITH_ASSERT_NULL(EventTypeToString(static_cast<uint16_t>(EventType::_Count)));
	for (int32_t i = 0; i < static_cast<int32_t>(PointReason::_Count); ++i)
		ZENITH_ASSERT_NOT_NULL(PointReasonToString(i), "point reason resolves to a name");
}

// Ready-position policy: a receiver awaiting a serve stands UP at the service line (so the
// short-bouncing serve is returnable); the server, and both players in a LIVE rally, hold
// the baseline. Guards the serve-return positioning that makes rallies start.
ZENITH_TEST(RenderTestTennis, ReadyZStandsReceiverInForServeReturn)
{
	const TennisCourt xC = DefaultCourt();
	const float fNearBase = xC.BaselineZ(TENNIS_SIDE_NEAR);
	const float fNearReady = ComputeReadyZ(xC, TENNIS_SIDE_NEAR, POINT_PHASE_SERVING, /*isServer*/false);
	// Receiver stands strictly forward of its baseline (closer to the net) but not at it.
	ZENITH_ASSERT_LT(std::fabs(fNearReady - xC.m_fNetZ), std::fabs(fNearBase - xC.m_fNetZ));
	ZENITH_ASSERT_GT(std::fabs(fNearReady - xC.m_fNetZ), 0.5f);
	ZENITH_ASSERT_GT((fNearReady - xC.m_fNetZ) * (fNearBase - xC.m_fNetZ), 0.0f);   // same side of the net as its baseline
	// Server (during SERVING) and anyone during LIVE hold the baseline.
	ZENITH_ASSERT_EQ_FLOAT(ComputeReadyZ(xC, TENNIS_SIDE_NEAR, POINT_PHASE_SERVING, /*isServer*/true), fNearBase, 1e-3f);
	ZENITH_ASSERT_EQ_FLOAT(ComputeReadyZ(xC, TENNIS_SIDE_NEAR, POINT_PHASE_LIVE, false), fNearBase, 1e-3f);
	// Far side mirrors (forward of its baseline, correct side of the net).
	const float fFarBase = xC.BaselineZ(TENNIS_SIDE_FAR);
	const float fFarReady = ComputeReadyZ(xC, TENNIS_SIDE_FAR, POINT_PHASE_SERVING, false);
	ZENITH_ASSERT_LT(std::fabs(fFarReady - xC.m_fNetZ), std::fabs(fFarBase - xC.m_fNetZ));
	ZENITH_ASSERT_GT((fFarReady - xC.m_fNetZ) * (fFarBase - xC.m_fNetZ), 0.0f);
}

// The referee's contact gate decision (plan unit-test #7). This is the heart of the
// "no magical contact" rule: an out-of-range swing scores the opponent, an in-range
// swing with no armed shot is discarded (ball stays live), only in-range + armed
// launches. Pinning it pure means inverting the gate in HandleEligibleContact (the
// regression the human reviewer flagged) flips an assertion here.
ZENITH_TEST(RenderTestTennis, ClassifyContactOutcomeGate)
{
	// Out of range = a genuine miss -> opponent point, whether or not a shot is armed.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyContactOutcome(false, false)), static_cast<int>(CONTACT_OUTCOME_OPPONENT_POINT));
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyContactOutcome(false, true)),  static_cast<int>(CONTACT_OUTCOME_OPPONENT_POINT));
	// In range but unarmed = stale/mistimed swing -> discard, keep the ball live.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyContactOutcome(true, false)),  static_cast<int>(CONTACT_OUTCOME_DISCARD));
	// In range + armed = a real strike -> launch.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyContactOutcome(true, true)),   static_cast<int>(CONTACT_OUTCOME_LAUNCH));
}

// ============================================================================
// Rules classifiers + termination
// ============================================================================

ZENITH_TEST(RenderTestTennis, ClassifyServeRules)
{
	const TennisCourt xC = DefaultCourt();
	// Near server, deuce: a point in the far -X box.
	const Zenith_Maths::Vector3 xInBox(xC.m_fCenterX - 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 3.0f);
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyServe(xC, xInBox, TENNIS_SIDE_NEAR, true, false, /*cleared*/ true)), static_cast<int>(SERVE_RESULT_GOOD));
	// In box but netted (didn't clear legally) -> fault / double fault.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyServe(xC, xInBox, TENNIS_SIDE_NEAR, true, false, /*cleared*/ false)), static_cast<int>(SERVE_RESULT_FAULT));
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyServe(xC, xInBox, TENNIS_SIDE_NEAR, true, true,  /*cleared*/ false)), static_cast<int>(SERVE_RESULT_DOUBLE_FAULT));
	// Wrong-court box (deuce serve into the ad box) -> fault even though it cleared.
	const Zenith_Maths::Vector3 xAdBox(xC.m_fCenterX + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 3.0f);
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyServe(xC, xAdBox, TENNIS_SIDE_NEAR, true, false, true)), static_cast<int>(SERVE_RESULT_FAULT));
	// A good second serve is still GOOD.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyServe(xC, xInBox, TENNIS_SIDE_NEAR, true, true, true)), static_cast<int>(SERVE_RESULT_GOOD));
}

ZENITH_TEST(RenderTestTennis, ClassifyBounceRules)
{
	const TennisCourt xC = DefaultCourt();
	const Zenith_Maths::Vector3 xFarIn(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f);
	const Zenith_Maths::Vector3 xNearOwn(xC.m_fCenterX, xC.m_fSurfaceY, xC.m_fNetZ - 6.0f);
	const Zenith_Maths::Vector3 xFarOut(xC.m_fCenterX + xC.m_fSinglesHalfWidth + 2.0f, xC.m_fSurfaceY, xC.m_fNetZ + 6.0f);

	// 2nd bounce anywhere -> hitter wins.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyBounceOutcome(xC, xFarIn, TENNIS_SIDE_FAR, TENNIS_SIDE_NEAR, 2, true)), static_cast<int>(POINT_OUTCOME_HITTER_WINS));
	// First bounce on the hitter's own side -> hitter loses.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyBounceOutcome(xC, xNearOwn, TENNIS_SIDE_NEAR, TENNIS_SIDE_NEAR, 1, true)), static_cast<int>(POINT_OUTCOME_HITTER_LOSES));
	// Didn't cross the net legally -> hitter loses.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyBounceOutcome(xC, xFarIn, TENNIS_SIDE_FAR, TENNIS_SIDE_NEAR, 1, false)), static_cast<int>(POINT_OUTCOME_HITTER_LOSES));
	// Out (vs singles lines) -> hitter loses.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyBounceOutcome(xC, xFarOut, TENNIS_SIDE_FAR, TENNIS_SIDE_NEAR, 1, true)), static_cast<int>(POINT_OUTCOME_HITTER_LOSES));
	// Legal first bounce in the opponent's court -> continue.
	ZENITH_ASSERT_EQ(static_cast<int>(ClassifyBounceOutcome(xC, xFarIn, TENNIS_SIDE_FAR, TENNIS_SIDE_NEAR, 1, true)), static_cast<int>(POINT_OUTCOME_CONTINUE));
}

ZENITH_TEST(RenderTestTennis, ResolveStallWinnerPolicy)
{
	// Not crossed -> the hitter loses (opponent wins).
	ZENITH_ASSERT_EQ(ResolveStallWinner(0, false), 1);
	ZENITH_ASSERT_EQ(ResolveStallWinner(1, false), 0);
	// Crossed legally then stalled on the receiver's side -> hitter wins.
	ZENITH_ASSERT_EQ(ResolveStallWinner(0, true), 0);
	ZENITH_ASSERT_EQ(ResolveStallWinner(1, true), 1);
}

ZENITH_TEST(RenderTestTennis, ResolveOffSlabSettleWinnerAndCause)
{
	// Bounced in (>=1) then carried off the slab: the OPPONENT failed to return a good
	// ball -> the HITTER wins. iBallSide is where the ball exited (the receiver's side),
	// so the winner is OtherSideIndex(iBallSide) = the hitter. Cause depends on rally length.
	const TennisSettleResolution xA = ResolveOffSlabSettle(/*bounce*/ 1, /*ballSide*/ TENNIS_SIDE_FAR, /*lastHitter*/ 0, /*rally*/ 4);
	ZENITH_ASSERT_EQ(xA.m_iWinnerSide, 0);                                                  // hitter (near)
	ZENITH_ASSERT_EQ(static_cast<int>(xA.m_eCause), static_cast<int>(TENNIS_SETTLE_DOUBLE_BOUNCE));
	const TennisSettleResolution xServe = ResolveOffSlabSettle(1, TENNIS_SIDE_FAR, 0, /*rally*/ 1);
	ZENITH_ASSERT_EQ(static_cast<int>(xServe.m_eCause), static_cast<int>(TENNIS_SETTLE_SERVE_UNRETURNED));

	// Overshoot (never bounced in): the HITTER'S own error -> the OPPONENT wins.
	const TennisSettleResolution xB = ResolveOffSlabSettle(/*bounce*/ 0, /*ballSide*/ TENNIS_SIDE_FAR, /*lastHitter*/ 0, /*rally*/ 5);
	ZENITH_ASSERT_EQ(xB.m_iWinnerSide, 1);                                                  // opponent (far)
	ZENITH_ASSERT_EQ(static_cast<int>(xB.m_eCause), static_cast<int>(TENNIS_SETTLE_LANDED_OUT));
}

ZENITH_TEST(RenderTestTennis, ResolveDeadLowSettleWinnerAndCause)
{
	// Died on the hitter's OWN side -> hitter loses (own netted/short shot).
	const TennisSettleResolution xOwn = ResolveDeadLowSettle(/*settleSide*/ 0, /*lastHitter*/ 0, /*rally*/ 5);
	ZENITH_ASSERT_EQ(xOwn.m_iWinnerSide, 1);                                                // opponent wins
	ZENITH_ASSERT_EQ(static_cast<int>(xOwn.m_eCause), static_cast<int>(TENNIS_SETTLE_INTO_NET_OR_OWN_SIDE));

	// Died on the OPPONENT's side -> they failed to return it; the HITTER wins.
	const TennisSettleResolution xOpp = ResolveDeadLowSettle(/*settleSide*/ 1, /*lastHitter*/ 0, /*rally*/ 5);
	ZENITH_ASSERT_EQ(xOpp.m_iWinnerSide, 0);                                                // hitter (near) wins
	ZENITH_ASSERT_EQ(static_cast<int>(xOpp.m_eCause), static_cast<int>(TENNIS_SETTLE_DOUBLE_BOUNCE));
	const TennisSettleResolution xServe = ResolveDeadLowSettle(/*settleSide*/ 1, /*lastHitter*/ 0, /*rally*/ 1);
	ZENITH_ASSERT_EQ(static_cast<int>(xServe.m_eCause), static_cast<int>(TENNIS_SETTLE_SERVE_UNRETURNED));
}

ZENITH_TEST(RenderTestTennis, ComputeEligibleStrikerGating)
{
	// SERVING: only the server, and ONLY before the serve is struck (lastHitter<0);
	// once struck (lastHitter>=0) NOBODY may strike the in-flight serve (the fix that
	// stops the server re-hitting / re-flailing at its own serve).
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_SERVING, -1, 0, 1), 0);
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_SERVING,  0, 0, 1), -1);   // serve struck -> nobody
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_SERVING, -1, 1, 0), 1);    // far server
	// LIVE: the expected receiver, regardless of last hitter.
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_LIVE, 0, 0, 1), 1);
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_LIVE, 1, 1, 0), 0);
	// Non-play phases: nobody.
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_WARMUP, -1, 0, 1), -1);
	ZENITH_ASSERT_EQ(ComputeEligibleStriker(POINT_PHASE_POINT_OVER, 0, 0, 1), -1);
}

// ============================================================================
// Player seam — racket sweet spot (pure; the engine getter just resolves the
// hand matrix and calls this, so the math is tested against synthetic poses).
// ============================================================================

ZENITH_TEST(RenderTestTennis, RacketSweetSpotTracksPosedHand)
{
	const float fReach = 0.42f;

	// Identity hand: the shaft runs down the bone's local -Y.
	const Zenith_Maths::Vector3 xId = ComputeRacketSweetSpot(Zenith_Maths::Matrix4(1.0f), fReach);
	ZENITH_ASSERT_NEAR_VEC3(xId, Zenith_Maths::Vector3(0.0f, -fReach, 0.0f), 1e-4f);

	// Translation: the sweet spot tracks the hand position.
	const Zenith_Maths::Matrix4 xT = glm::translate(Zenith_Maths::Matrix4(1.0f), Zenith_Maths::Vector3(5.0f, 2.0f, 3.0f));
	ZENITH_ASSERT_NEAR_VEC3(ComputeRacketSweetSpot(xT, fReach), Zenith_Maths::Vector3(5.0f, 2.0f - fReach, 3.0f), 1e-4f);

	// Three distinct hand ORIENTATIONS (standing in for serve / forehand /
	// backhand poses) yield three distinct sweet spots that each track the
	// rotated shaft.
	const Zenith_Maths::Matrix4 xServe = glm::rotate(Zenith_Maths::Matrix4(1.0f), static_cast<float>(Zenith_Maths::Pi), Zenith_Maths::Vector3(1, 0, 0));      // shaft -> +Y (overhead)
	const Zenith_Maths::Matrix4 xFore  = glm::rotate(Zenith_Maths::Matrix4(1.0f), static_cast<float>(Zenith_Maths::Pi * 0.5), Zenith_Maths::Vector3(0, 0, 1)); // shaft -> +X
	const Zenith_Maths::Matrix4 xBack  = glm::rotate(Zenith_Maths::Matrix4(1.0f), static_cast<float>(-Zenith_Maths::Pi * 0.5), Zenith_Maths::Vector3(0, 0, 1));// shaft -> -X
	const Zenith_Maths::Vector3 xSv = ComputeRacketSweetSpot(xServe, fReach);
	const Zenith_Maths::Vector3 xFh = ComputeRacketSweetSpot(xFore,  fReach);
	const Zenith_Maths::Vector3 xBh = ComputeRacketSweetSpot(xBack,  fReach);
	ZENITH_ASSERT_NEAR_VEC3(xSv, Zenith_Maths::Vector3(0.0f, fReach, 0.0f), 1e-4f);
	ZENITH_ASSERT_NEAR_VEC3(xFh, Zenith_Maths::Vector3(fReach, 0.0f, 0.0f), 1e-4f);
	ZENITH_ASSERT_NEAR_VEC3(xBh, Zenith_Maths::Vector3(-fReach, 0.0f, 0.0f), 1e-4f);
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xSv - xFh), 0.1f);
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xFh - xBh), 0.1f);
}

// ============================================================================
// Brain (RenderTest_TennisAgentComponent) — move-safety, ownership, epoch gate,
// serialisation. Engine-bound: a fresh empty scene, brains added to entities so
// the component pool actually grows / swap-and-pops (move-constructs).
// ============================================================================

namespace
{
	struct TennisBrainFixture
	{
		Zenith_Scene xScene;
		Zenith_SceneData* pxSceneData = nullptr;

		TennisBrainFixture()
		{
			xScene = g_xEngine.Scenes().LoadScene("TennisBrainTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		}
		~TennisBrainFixture()
		{
			g_xEngine.Scenes().UnloadSceneForced(xScene);
		}

		// A bare entity carrying just a brain (no AIAgent), so the brain owns its
		// tree with nobody borrowing it — clean for move/relocation assertions.
		Zenith_EntityID MakeBrainEntity(const char* szName)
		{
			Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
			xE.AddComponent<RenderTest_TennisAgentComponent>();
			return xE.GetEntityID();
		}
		Zenith_EntityID MakeBareEntity(const char* szName)
		{
			return g_xEngine.Scenes().CreateEntity(pxSceneData, szName).GetEntityID();
		}
		RenderTest_TennisMatchComponent& Referee(Zenith_EntityID xID)
		{
			return pxSceneData->GetEntity(xID).GetComponent<RenderTest_TennisMatchComponent>();
		}
		// An NPC entity carrying AIAgent + body + brain (no animator: RequestServe/
		// RequestSwing therefore return false, exercising the "arm only on a
		// confirmed stroke start" path).
		Zenith_EntityID MakeAgentEntity(const char* szName, bool bNear)
		{
			Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
			xE.AddComponent<Zenith_AIAgentComponent>();
			xE.AddComponent<RenderTest_TennisPlayerComponent>().Init(bNear);
			xE.AddComponent<RenderTest_TennisAgentComponent>();
			return xE.GetEntityID();
		}
		RenderTest_TennisAgentComponent& Brain(Zenith_EntityID xID)
		{
			return pxSceneData->GetEntity(xID).GetComponent<RenderTest_TennisAgentComponent>();
		}
	};
}

ZENITH_TEST(RenderTestTennis, TennisBrainSurvivesPoolGrowth)
{
	TennisBrainFixture xFix;
	const char* aszEarly[4] = { "B0", "B1", "B2", "B3" };
	Zenith_EntityID axEarly[4];
	for (int i = 0; i < 4; ++i)
		axEarly[i] = xFix.MakeBrainEntity(aszEarly[i]);

	// Build trees on the early brains, then record the heap tree pointers.
	Zenith_BehaviorTree* apxTree[4];
	for (int i = 0; i < 4; ++i)
	{
		xFix.Brain(axEarly[i]).OnStart();
		apxTree[i] = xFix.Brain(axEarly[i]).GetTree();
		ZENITH_ASSERT_NOT_NULL(apxTree[i], "brain OnStart must build a tree");
	}

	// Force the brain pool to grow (move-construct every live brain) by adding many
	// more brains AFTER the early ones already own trees.
	for (int j = 0; j < 48; ++j)
	{
		char acName[16];
		snprintf(acName, sizeof(acName), "Filler%d", j);
		xFix.MakeBrainEntity(acName);
	}

	// The early brains' trees survived relocation: same heap pointer, still owned.
	for (int i = 0; i < 4; ++i)
	{
		Zenith_BehaviorTree* pxNow = xFix.Brain(axEarly[i]).GetTree();
		ZENITH_ASSERT_NOT_NULL(pxNow, "tree must survive pool growth");
		ZENITH_ASSERT_TRUE(pxNow == apxTree[i], "moved brain keeps the same heap tree (no realloc/dup)");
	}
}

ZENITH_TEST(RenderTestTennis, TennisBrainSwapAndPopNoDoubleFree)
{
	TennisBrainFixture xFix;
	Zenith_EntityID axID[6];
	for (int i = 0; i < 6; ++i)
	{
		char acName[16];
		snprintf(acName, sizeof(acName), "S%d", i);
		axID[i] = xFix.MakeBrainEntity(acName);
		xFix.Brain(axID[i]).OnStart();
	}
	Zenith_BehaviorTree* pxLastTree = xFix.Brain(axID[5]).GetTree();

	// Remove a middle brain -> swap-and-pop moves the last brain into the gap.
	xFix.pxSceneData->GetEntity(axID[2]).RemoveComponent<RenderTest_TennisAgentComponent>();

	ZENITH_ASSERT_FALSE(xFix.pxSceneData->GetEntity(axID[2]).HasComponent<RenderTest_TennisAgentComponent>(),
		"removed brain is gone");
	// The moved (formerly-last) brain kept its tree; neighbours intact.
	ZENITH_ASSERT_TRUE(xFix.Brain(axID[5]).GetTree() == pxLastTree, "swap-moved brain keeps its tree");
	ZENITH_ASSERT_NOT_NULL(xFix.Brain(axID[0]).GetTree(), "neighbour brain intact");
	ZENITH_ASSERT_NOT_NULL(xFix.Brain(axID[4]).GetTree(), "neighbour brain intact");
}

ZENITH_TEST(RenderTestTennis, TennisBrainMoveCtorTransfers)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("MoveCtor"));
	RenderTest_TennisAgentComponent xSrc(xE);
	xSrc.OnStart();
	Zenith_BehaviorTree* pxTree = xSrc.GetTree();
	ZENITH_ASSERT_NOT_NULL(pxTree, "source owns a tree");

	RenderTest_TennisAgentComponent xDst(std::move(xSrc));
	ZENITH_ASSERT_TRUE(xDst.GetTree() == pxTree, "move ctor transfers the tree");
	ZENITH_ASSERT_NULL(xSrc.GetTree(), "moved-from source is nulled (dtor becomes a no-op)");
}

ZENITH_TEST(RenderTestTennis, TennisBrainMoveAssignReleasesOld)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE1 = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("MA1"));
	Zenith_Entity xE2 = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("MA2"));
	RenderTest_TennisAgentComponent xDst(xE1);
	RenderTest_TennisAgentComponent xSrc(xE2);
	xDst.OnStart();
	xSrc.OnStart();
	Zenith_BehaviorTree* pxDstOld = xDst.GetTree();
	Zenith_BehaviorTree* pxSrcTree = xSrc.GetTree();
	ZENITH_ASSERT_TRUE(pxDstOld != nullptr && pxSrcTree != nullptr && pxDstOld != pxSrcTree, "two distinct trees");

	// Move-assign into a LIVE brain: its old tree is released first, then it adopts
	// the source's tree; the source is nulled. (No crash == old tree freed once.)
	xDst = std::move(xSrc);
	ZENITH_ASSERT_TRUE(xDst.GetTree() == pxSrcTree, "destination adopts the source tree");
	ZENITH_ASSERT_NULL(xSrc.GetTree(), "source is neutralised");
}

ZENITH_TEST(RenderTestTennis, TennisOnDestroyThenDtorNoDoubleFree)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("Destroy"));
	{
		RenderTest_TennisAgentComponent xB(xE);
		xB.OnStart();
		ZENITH_ASSERT_NOT_NULL(xB.GetTree(), "tree built");
		xB.OnDestroy();
		ZENITH_ASSERT_NULL(xB.GetTree(), "OnDestroy deletes + nulls the tree");
		// Scope exit runs the dtor: it must be a no-op (no double free).
	}
	ZENITH_ASSERT_TRUE(true, "OnDestroy-then-dtor did not double free");
}

ZENITH_TEST(RenderTestTennis, TryGetDecidedShotRejectsStaleEpoch)
{
	TennisBrainFixture xFix;
	Zenith_Entity xEpochEnt = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("Epoch"));
	RenderTest_TennisAgentComponent xB(xEpochEnt);

	TennisShotDecision xShot;
	xShot.m_xAim = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
	xB.SetDecidedShot(xShot);
	ZENITH_ASSERT_FALSE(xB.IsArmed(), "SetDecidedShot stores UNARMED");

	xB.ArmDecidedShot(5u);
	TennisShotDecision xOut;
	ZENITH_ASSERT_TRUE (xB.TryGetDecidedShot(5u, xOut));   // armed + matching epoch
	ZENITH_ASSERT_FALSE(xB.TryGetDecidedShot(6u, xOut));   // stale epoch rejected

	// Per-ball reset clears the guard.
	xB.ResetForNewBall(6u);
	ZENITH_ASSERT_FALSE(xB.IsArmed(), "ResetForNewBall clears the arm guard");
	ZENITH_ASSERT_FALSE(xB.TryGetDecidedShot(5u, xOut));
}

ZENITH_TEST(RenderTestTennis, TennisAgentSerialisationRoundTrips)
{
	TennisBrainFixture xFix;
	Zenith_Entity xSerEnt = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("Ser"));
	RenderTest_TennisAgentComponent xB(xSerEnt);
	Zenith_DataStream xStream;
	xB.WriteToDataStream(xStream);
	const uint64_t ulWritten = xStream.GetCursor();
	ZENITH_ASSERT_GT(ulWritten, 0u);                 // a version tag was written
	xStream.SetCursor(0);                            // rewind to read back what we just wrote
	Zenith_Entity xSer2Ent = xFix.pxSceneData->GetEntity(xFix.MakeBrainEntity("Ser2"));
	RenderTest_TennisAgentComponent xB2(xSer2Ent);
	xB2.ReadFromDataStream(xStream);
	// The read must consume EXACTLY the bytes written (the version tag round-trips) —
	// a real assertion, not a tautology.
	ZENITH_ASSERT_EQ(xStream.GetCursor(), ulWritten);
}

// ============================================================================
// Referee (RenderTest_TennisMatchComponent) — heap-nav ownership + move safety
// + getters. Bare entities (no NPCs/ball): OnStart still builds the slab navmesh
// + two heap nav agents (owned by the referee, borrowed by nobody), so the move
// assertions are clean.
// ============================================================================

ZENITH_TEST(RenderTestTennis, TennisRefereeGettersAndNavBuild)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("Ref"));
	RenderTest_TennisMatchComponent xRef(xE);
	xRef.OnStart();
	ZENITH_ASSERT_EQ(static_cast<int>(xRef.GetPhase()), static_cast<int>(POINT_PHASE_WARMUP));
	ZENITH_ASSERT_TRUE(xRef.GetServeFromDeuceCourt());        // 0-0 -> even -> deuce
	ZENITH_ASSERT_EQ(xRef.GetSidePoints(0), 0u);
	ZENITH_ASSERT_TRUE(xRef.IsNavMeshValid());                // slab quad -> polygons
	ZENITH_ASSERT_NOT_NULL(xRef.GetNavMesh(), "navmesh built");
	ZENITH_ASSERT_NOT_NULL(xRef.GetNavAgent(0), "nav agent 0 built");
	ZENITH_ASSERT_NOT_NULL(xRef.GetNavAgent(1), "nav agent 1 built");
	xRef.OnDestroy();   // explicit teardown before scope dtor (no double free)
}

ZENITH_TEST(RenderTestTennis, TennisRefereeNavPointersStableOnMove)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("RefMove"));
	RenderTest_TennisMatchComponent xSrc(xE);
	xSrc.OnStart();
	const Zenith_NavMesh* pxMesh = xSrc.GetNavMesh();
	const Zenith_NavMeshAgent* pxA0 = xSrc.GetNavAgent(0);
	ZENITH_ASSERT_NOT_NULL(pxMesh, "source built a navmesh");

	RenderTest_TennisMatchComponent xDst(std::move(xSrc));
	ZENITH_ASSERT_TRUE(xDst.GetNavMesh() == pxMesh, "move transfers the navmesh (stable heap address)");
	ZENITH_ASSERT_TRUE(xDst.GetNavAgent(0) == pxA0, "move transfers the nav agents");
	ZENITH_ASSERT_NULL(xSrc.GetNavMesh(), "moved-from source nav nulled (dtor is a no-op)");
	ZENITH_ASSERT_NULL(xSrc.GetNavAgent(0), "moved-from source nav agent nulled");
}

ZENITH_TEST(RenderTestTennis, TennisRefereeMoveAssignReleasesOld)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE1 = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("RefMA1"));
	Zenith_Entity xE2 = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("RefMA2"));
	RenderTest_TennisMatchComponent xDst(xE1);
	RenderTest_TennisMatchComponent xSrc(xE2);
	xDst.OnStart();
	xSrc.OnStart();
	const Zenith_NavMesh* pxDstOld = xDst.GetNavMesh();
	const Zenith_NavMesh* pxSrcMesh = xSrc.GetNavMesh();
	ZENITH_ASSERT_TRUE(pxDstOld != nullptr && pxSrcMesh != nullptr && pxDstOld != pxSrcMesh, "two distinct navmeshes");

	xDst = std::move(xSrc);   // releases dst's own navmesh first, then adopts src's
	ZENITH_ASSERT_TRUE(xDst.GetNavMesh() == pxSrcMesh, "destination adopts the source navmesh");
	ZENITH_ASSERT_NULL(xSrc.GetNavMesh(), "source is neutralised");
}

ZENITH_TEST(RenderTestTennis, TennisBrainMoveAssignClearsLiveBorrow)
{
	// Move-assigning into a brain whose entity's AIAgent BORROWS the (about-to-be-freed)
	// tree must null that borrow first, or the AIAgent dangles. Dest entity has an
	// AIAgent (the live borrow); source is agentless (so its tree has no borrow and the
	// scope-exit dtor frees it without leaving a pooled AIAgent pointing at it).
	TennisBrainFixture xFix;
	Zenith_Entity xE1 = g_xEngine.Scenes().CreateEntity(xFix.pxSceneData, "MA_Borrow1");
	xE1.AddComponent<Zenith_AIAgentComponent>();
	Zenith_Entity xE2 = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("MA_Borrow2"));
	RenderTest_TennisAgentComponent xDst(xE1);  xDst.OnStart();   // wires E1's AIAgent BT borrow
	RenderTest_TennisAgentComponent xSrc(xE2);  xSrc.OnStart();   // agentless -> owns tree, no borrow
	Zenith_BehaviorTree* pxSrcTree = xSrc.GetTree();
	ZENITH_ASSERT_TRUE(xE1.GetComponent<Zenith_AIAgentComponent>().GetBehaviorTree() == xDst.GetTree());

	xDst = std::move(xSrc);   // must null E1's borrow BEFORE freeing xDst's old tree

	ZENITH_ASSERT_NULL(xE1.GetComponent<Zenith_AIAgentComponent>().GetBehaviorTree(),
		"move-assign clears the destination entity's stale BT borrow before freeing the old tree");
	ZENITH_ASSERT_TRUE(xDst.GetTree() == pxSrcTree);
	ZENITH_ASSERT_NULL(xSrc.GetTree());
}

ZENITH_TEST(RenderTestTennis, TennisRefereeSurvivesPoolGrowth)
{
	TennisBrainFixture xFix;
	Zenith_EntityID axID[3];
	const Zenith_NavMesh* apxMesh[3];
	for (int i = 0; i < 3; ++i)
	{
		char acName[16];
		snprintf(acName, sizeof(acName), "RG%d", i);
		Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(xFix.pxSceneData, acName);
		xE.AddComponent<RenderTest_TennisMatchComponent>();
		axID[i] = xE.GetEntityID();
		xFix.Referee(axID[i]).OnStart();
		apxMesh[i] = xFix.Referee(axID[i]).GetNavMesh();
		ZENITH_ASSERT_NOT_NULL(apxMesh[i], "referee built its navmesh");
	}
	// Force the referee pool to grow (move-construct the live referees).
	for (int j = 0; j < 16; ++j)
	{
		char acName[16];
		snprintf(acName, sizeof(acName), "RGfill%d", j);
		Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(xFix.pxSceneData, acName);
		xE.AddComponent<RenderTest_TennisMatchComponent>();
	}
	for (int i = 0; i < 3; ++i)
		ZENITH_ASSERT_TRUE(xFix.Referee(axID[i]).GetNavMesh() == apxMesh[i],
			"referee navmesh survives pool growth (heap-stable, transferred)");
}

// ============================================================================
// BT leaves — one dedicated test each. Every leaf reports SUCCESS or FAILURE,
// NEVER RUNNING (the root selector re-evaluates each tick). Decide leaves leave
// the brain UNARMED; arm leaves arm only on a confirmed stroke start (no animator
// in the fixture -> RequestServe/Swing return false -> stays unarmed).
// ============================================================================

namespace
{
	// A blackboard with the keys the leaves read, defaulted to a benign WARMUP.
	inline void SeedLeafBB(Zenith_Blackboard& xBB, int iPhase, bool bServer, bool bMyBall, int iSide)
	{
		xBB.SetInt(RenderTest_TennisBB::k_szPhase, iPhase);
		xBB.SetInt(RenderTest_TennisBB::k_szBallEpoch, 7);
		xBB.SetInt(RenderTest_TennisBB::k_szMySide, iSide);
		xBB.SetBool(RenderTest_TennisBB::k_szIsServer, bServer);
		xBB.SetBool(RenderTest_TennisBB::k_szServeFromDeuce, true);
		xBB.SetBool(RenderTest_TennisBB::k_szServeBallParked, iPhase == RenderTest_Tennis::POINT_PHASE_SERVING);
		xBB.SetBool(RenderTest_TennisBB::k_szIsSecondServe, false);
		xBB.SetBool(RenderTest_TennisBB::k_szIsMyBall, bMyBall);
	}
}

ZENITH_TEST(RenderTestTennis, BTLeaf_IsMyServeAndBallParked)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_Serve", true));
	RenderTest_BTCond_IsMyServeAndBallParked xLeaf;

	Zenith_Blackboard xBB;
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, /*server*/ true, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, /*server*/ false, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::FAILURE));
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, /*server*/ true, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::FAILURE));
	// Serve already STRUCK (ball in flight, not parked) -> FAILURE even when SERVING +
	// server, so the serve branch can't re-flail a phantom swing.
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, /*server*/ true, false, 0);
	xBB.SetBool(RenderTest_TennisBB::k_szServeBallParked, false);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::FAILURE));
}

ZENITH_TEST(RenderTestTennis, BTLeaf_BallIsMineFailsUnlessLive)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_Mine", true));
	RenderTest_BTCond_BallIsMine xLeaf;

	Zenith_Blackboard xBB;
	// SERVING: receiver must NOT arm -> FAILURE regardless of IsMyBall.
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, false, true, 0);
	BTNodeStatus eServing = xLeaf.Execute(xE, xBB, 0.0f);
	ZENITH_ASSERT_EQ(static_cast<int>(eServing), static_cast<int>(BTNodeStatus::FAILURE));
	ZENITH_ASSERT_NE(static_cast<int>(eServing), static_cast<int>(BTNodeStatus::RUNNING));
	// LIVE but not my ball -> FAILURE.
	SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, false, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::FAILURE));
}

ZENITH_TEST(RenderTestTennis, BTLeaf_BallIsMineReachesAwarenessGate)
{
	// Phase==LIVE, IsMyBall, and a reachable descending ball are ALL satisfied (so the
	// leaf gets past the phase / IsMyBall / reachability checks), yet with no perception
	// awareness of the ball the "seen" gate still FAILs the condition — proving the
	// awareness gate is wired and reached. The full all-gates-pass SUCCESS path (which
	// needs live perception awareness) is exercised in the windowed _False run, where
	// receivers commit + return.
	TennisBrainFixture xFix;
	const TennisCourt xC = DefaultCourt();
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_MineAw", true));
	xE.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.BaselineZ(0)));
	Zenith_Entity xBall = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("L_MineAw_Ball"));
	xBall.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY + 1.5f, xC.BaselineZ(0) + 1.0f));

	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, false, true, 0);
	xBB.SetEntityID(RenderTest_TennisBB::k_szBallEntity, xBall.GetEntityID());
	RenderTest_BTCond_BallIsMine xLeaf;
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::FAILURE));
}

ZENITH_TEST(RenderTestTennis, BTLeaf_DecideServeLeavesUnarmed)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_DecS", true));
	RenderTest_BTAction_DecideServe xLeaf;
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, true, false, 0);

	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
	ZENITH_ASSERT_FALSE(xE.GetComponent<RenderTest_TennisAgentComponent>().IsArmed(),
		"DecideServe stores the decision UNARMED");
}

ZENITH_TEST(RenderTestTennis, BTLeaf_PositionForServeSucceeds)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_Pos", true));
	RenderTest_BTAction_PositionForServe xLeaf;
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, true, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
}

ZENITH_TEST(RenderTestTennis, BTLeaf_ArmServeNoAnimatorStaysUnarmed)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_ArmS", true));
	// Decide first so there is a shot to arm.
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_SERVING, true, false, 0);
	RenderTest_BTAction_DecideServe xDecide;  xDecide.Execute(xE, xBB, 0.0f);

	RenderTest_BTAction_ArmServe xArm;
	const BTNodeStatus e = xArm.Execute(xE, xBB, 0.0f);
	ZENITH_ASSERT_NE(static_cast<int>(e), static_cast<int>(BTNodeStatus::RUNNING));
	// No animator -> RequestServe returns false -> brain stays UNARMED.
	ZENITH_ASSERT_FALSE(xE.GetComponent<RenderTest_TennisAgentComponent>().IsArmed(),
		"ArmServe must not arm when the stroke can't start");
}

ZENITH_TEST(RenderTestTennis, BTLeaf_MoveToInterceptSucceeds)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_Move", true));
	RenderTest_BTAction_MoveToIntercept xLeaf;
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, false, true, 0);
	// No ball entity resolvable -> still SUCCESS (positioning is best-effort).
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
}

ZENITH_TEST(RenderTestTennis, BTLeaf_DecideShotLeavesUnarmed)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_DecH", true));
	RenderTest_BTAction_DecideShot xLeaf;
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, false, true, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
	ZENITH_ASSERT_FALSE(xE.GetComponent<RenderTest_TennisAgentComponent>().IsArmed(),
		"DecideShot stores the decision UNARMED");
}

ZENITH_TEST(RenderTestTennis, BTLeaf_ArmSwingNeverRunsAndStaysUnarmedWithoutAnimator)
{
	// Leaf-level smoke over the full ArmSwing path with a resolvable, reachable ball
	// (it runs BallState + PredictIntercept without crashing): the leaf never returns
	// RUNNING and, with no animator (RequestSwing returns false), never arms. The
	// reachability MATH itself is verified observably by the pure PredictIntercept*
	// tests above; the arm-only-on-true-RequestSwing rule is verified observably by
	// the ArmServe test (which has no reachability gate to obscure it).
	TennisBrainFixture xFix;
	const TennisCourt xC = DefaultCourt();
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_ArmH", true));
	xE.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY, xC.BaselineZ(0)));
	Zenith_Entity xBall = xFix.pxSceneData->GetEntity(xFix.MakeBareEntity("L_ArmH_Ball"));
	xBall.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(xC.m_fCenterX, xC.m_fSurfaceY + 1.5f, xC.BaselineZ(0) + 1.0f));

	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_LIVE, false, true, 0);
	xBB.SetEntityID(RenderTest_TennisBB::k_szBallEntity, xBall.GetEntityID());
	xE.GetComponent<RenderTest_TennisAgentComponent>().SetDecidedShot(TennisShotDecision());

	RenderTest_BTAction_ArmSwing xLeaf;
	const BTNodeStatus e = xLeaf.Execute(xE, xBB, 0.0f);
	ZENITH_ASSERT_NE(static_cast<int>(e), static_cast<int>(BTNodeStatus::RUNNING));
	ZENITH_ASSERT_EQ(static_cast<int>(e), static_cast<int>(BTNodeStatus::SUCCESS));
	ZENITH_ASSERT_FALSE(xE.GetComponent<RenderTest_TennisAgentComponent>().IsArmed());
}

ZENITH_TEST(RenderTestTennis, BTLeaf_RecoverAlwaysSucceeds)
{
	TennisBrainFixture xFix;
	Zenith_Entity xE = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("L_Rec", true));
	RenderTest_BTAction_RecoverToReady xLeaf;
	Zenith_Blackboard xBB; SeedLeafBB(xBB, RenderTest_Tennis::POINT_PHASE_POINT_OVER, false, false, 0);
	ZENITH_ASSERT_EQ(static_cast<int>(xLeaf.Execute(xE, xBB, 0.0f)), static_cast<int>(BTNodeStatus::SUCCESS));
}

// ============================================================================
// Integration fixture — a minimal authored scene (ball + 2 NPCs with
// AIAgent/body/brain + the referee), driven through the load-order lifecycle.
// Validates the cross-component wiring the pure tests can't: the brain builds +
// wires its BT, the referee builds the navmesh + wires it onto the AIAgents +
// publishes the blackboard, and AdvanceBallEpoch resets BOTH brains. (Contact /
// launch is animator+physics driven and is verified in the windowed run.)
// ============================================================================

namespace
{
	struct TennisMatchFixture
	{
		Zenith_Scene xScene;
		Zenith_SceneData* pxSceneData = nullptr;
		Zenith_EntityID xBallID = INVALID_ENTITY_ID;
		Zenith_EntityID xNpcID[2] = { INVALID_ENTITY_ID, INVALID_ENTITY_ID };
		Zenith_EntityID xMatchID = INVALID_ENTITY_ID;

		TennisMatchFixture()
		{
			xScene = g_xEngine.Scenes().LoadScene("TennisMatchTestScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			g_xEngine.Scenes().SetActiveScene(xScene);
			pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);

			xBallID = g_xEngine.Scenes().CreateEntity(pxSceneData, "Tennis_Ball").GetEntityID();

			const char* aszNpc[2] = { "Tennis_NPC_Near", "Tennis_NPC_Far" };
			for (int i = 0; i < 2; ++i)
			{
				Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(pxSceneData, aszNpc[i]);
				xE.AddComponent<Zenith_AIAgentComponent>();
				xE.AddComponent<RenderTest_TennisPlayerComponent>().Init(i == 0);
				xE.AddComponent<RenderTest_TennisAgentComponent>();
				xNpcID[i] = xE.GetEntityID();
			}
			xMatchID = g_xEngine.Scenes().CreateEntity(pxSceneData, "Tennis_Match").GetEntityID();
			pxSceneData->GetEntity(xMatchID).AddComponent<RenderTest_TennisMatchComponent>();

			// Authored-order lifecycle: AIAgent OnAwake (perception register) -> the
			// brains' OnStart (build + wire the BT) -> the referee's OnStart (nav +
			// perception targets), matching Tennis_Match being authored last.
			for (int i = 0; i < 2; ++i)
				AIAgent(i).OnAwake();
			for (int i = 0; i < 2; ++i)
				Brain(i).OnStart();
			Referee().OnStart();
		}
		~TennisMatchFixture()
		{
			g_xEngine.Scenes().UnloadSceneForced(xScene);
		}

		Zenith_AIAgentComponent& AIAgent(int i) { return pxSceneData->GetEntity(xNpcID[i]).GetComponent<Zenith_AIAgentComponent>(); }
		RenderTest_TennisAgentComponent& Brain(int i) { return pxSceneData->GetEntity(xNpcID[i]).GetComponent<RenderTest_TennisAgentComponent>(); }
		RenderTest_TennisMatchComponent& Referee() { return pxSceneData->GetEntity(xMatchID).GetComponent<RenderTest_TennisMatchComponent>(); }
	};
}

ZENITH_TEST(RenderTestTennis, IntegrationLoadWiring)
{
	TennisMatchFixture xFix;
	// Each brain built + wired its BT onto the sibling AIAgent.
	for (int i = 0; i < 2; ++i)
	{
		ZENITH_ASSERT_NOT_NULL(xFix.Brain(i).GetTree(), "brain owns a tree");
		ZENITH_ASSERT_TRUE(xFix.AIAgent(i).GetBehaviorTree() == xFix.Brain(i).GetTree(),
			"AIAgent borrows the brain's tree");
		// Referee wired its heap nav agent onto each AIAgent.
		ZENITH_ASSERT_NOT_NULL(xFix.AIAgent(i).GetNavMeshAgent(), "referee wired a nav agent");
		// Brain seeded the blackboard with the ball entity.
		ZENITH_ASSERT_TRUE(xFix.AIAgent(i).GetBlackboard().GetEntityID(RenderTest_TennisBB::k_szBallEntity) == xFix.xBallID,
			"brain seeded the ball entity into the blackboard");
	}
	ZENITH_ASSERT_TRUE(xFix.Referee().IsNavMeshValid(), "referee built a valid slab navmesh");
}

ZENITH_TEST(RenderTestTennis, IntegrationBlackboardPublish)
{
	TennisMatchFixture xFix;
	xFix.Referee().OnLateUpdate(1.0f / 60.0f);
	// The referee publishes its phase into BOTH NPCs' blackboards.
	for (int i = 0; i < 2; ++i)
		ZENITH_ASSERT_EQ(
			xFix.AIAgent(i).GetBlackboard().GetInt(RenderTest_TennisBB::k_szPhase, -1),
			static_cast<int>(xFix.Referee().GetPhase()));
}

ZENITH_TEST(RenderTestTennis, IntegrationEpochResetsBothBrains)
{
	TennisMatchFixture xFix;
	// Arm both brains under the current epoch.
	const uint32_t uEpoch0 = xFix.Referee().GetBallEpoch();
	for (int i = 0; i < 2; ++i)
	{
		TennisShotDecision xShot; xShot.m_xAim = Zenith_Maths::Vector3(1, 2, 3);
		xFix.Brain(i).SetDecidedShot(xShot);
		xFix.Brain(i).ArmDecidedShot(uEpoch0);
		ZENITH_ASSERT_TRUE(xFix.Brain(i).IsArmed());
	}
	// Expire warmup (1.5 s) -> StartPoint -> StartServe -> AdvanceBallEpoch, which
	// synchronously resets BOTH brains' arm guards + bumps the epoch.
	xFix.Referee().OnLateUpdate(2.0f);
	ZENITH_ASSERT_GT(xFix.Referee().GetBallEpoch(), uEpoch0);
	for (int i = 0; i < 2; ++i)
		ZENITH_ASSERT_FALSE(xFix.Brain(i).IsArmed(), "AdvanceBallEpoch cleared the arm guard on both brains");
}

ZENITH_TEST(RenderTestTennis, IntegrationPhaseTransitionParksSameFrame)
{
	TennisMatchFixture xFix;
	// Warmup is a non-play phase -> agents parked (disabled).
	xFix.Referee().OnLateUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_FALSE(xFix.AIAgent(0).IsEnabled());
	ZENITH_ASSERT_FALSE(xFix.AIAgent(1).IsEnabled());
	// One OnLateUpdate expires the 1.5 s warmup and transitions WARMUP -> SERVING.
	// Because ApplyPhaseAgentState runs AGAIN after the phase transition (F3), the
	// agents are enabled THIS frame, not one frame late.
	xFix.Referee().OnLateUpdate(2.0f);
	ZENITH_ASSERT_EQ(static_cast<int>(xFix.Referee().GetPhase()), static_cast<int>(RenderTest_Tennis::POINT_PHASE_SERVING));
	ZENITH_ASSERT_TRUE(xFix.AIAgent(0).IsEnabled());
	ZENITH_ASSERT_TRUE(xFix.AIAgent(1).IsEnabled());
}

ZENITH_TEST(RenderTestTennis, IntegrationNavStallFallsBackToFootwork)
{
	// P2: the fallback is PROGRESS-based, not path-based. This fixture only drives the
	// REFEREE (the AIAgents' BTs never tick), so the nav agents never reach a goal AND
	// their bodies never move — exactly the "stalled" signature (covers both a failed
	// path and a valid-but-blocked path; CheckNavFallback only sees no-progress). After
	// ~k_uNavStuckFrames play frames the referee hands each agent to footwork (so it
	// isn't left frozen with footwork disabled).
	TennisMatchFixture xFix;
	xFix.Referee().OnLateUpdate(2.0f);   // warmup -> serving (a play phase)
	ZENITH_ASSERT_EQ(static_cast<int>(xFix.Referee().GetPhase()), static_cast<int>(RenderTest_Tennis::POINT_PHASE_SERVING));
	ZENITH_ASSERT_FALSE(xFix.Referee().IsNavFallback(0));   // not yet
	for (int f = 0; f < 35; ++f)
		xFix.Referee().OnLateUpdate(1.0f / 60.0f);
	ZENITH_ASSERT_TRUE(xFix.Referee().IsNavFallback(0));
	ZENITH_ASSERT_TRUE(xFix.Referee().IsNavFallback(1));
}

ZENITH_TEST(RenderTestTennis, ServeTossIsIndependentOfRacket)
{
	// P1 invariant: the un-struck serve must park at an INDEPENDENT world point above the
	// server's body — NOT the racket sweet spot. Gluing the ball to the racket makes the
	// contact gate tautological (zero distance, no serve can miss). The toss height must
	// be the fixed serve-toss height above the surface (so the racket has to reach UP to
	// it), decoupled from the body/sweet-spot Y. Reverting ServeTossPos to return the
	// sweet spot fails this (the toss Y would track the body, not the fixed height).
	TennisMatchFixture xFix;
	const int iServer = xFix.Referee().GetServerSide();
	ZENITH_ASSERT_TRUE(iServer == 0 || iServer == 1);

	// Put the server's body at surface level so its (unposed) sweet-spot Y == surface.
	const RenderTest_Tennis::TennisCourt xC = DefaultCourt();
	const Zenith_Maths::Vector3 xServerPos(120.0f, xC.m_fSurfaceY, 150.0f);
	Zenith_Entity xServer = xFix.pxSceneData->GetEntity(xFix.xNpcID[iServer]);
	xServer.GetComponent<Zenith_TransformComponent>().SetPosition(xServerPos);

	const Zenith_Maths::Vector3 xSweet =
		xServer.GetComponent<RenderTest_TennisPlayerComponent>().GetRacketSweetSpotPos();
	const Zenith_Maths::Vector3 xToss = xFix.Referee().GetServeTossPos();

	// The toss sits ABOVE the body at the fixed serve-toss height — NOT at the sweet spot.
	ZENITH_ASSERT_GT(xToss.y, xSweet.y + 0.5f);                  // decoupled from the racket/body Y
	ZENITH_ASSERT_GT(xToss.y, xC.m_fSurfaceY + 0.5f);           // a real height above the surface
	// XZ stays near the server (the racket must reach it), within a small forward offset.
	ZENITH_ASSERT_LT(xToss.x, xServerPos.x + 1.0f);
	ZENITH_ASSERT_GT(xToss.x, xServerPos.x - 1.0f);
	ZENITH_ASSERT_LT(xToss.z, xServerPos.z + 1.0f);
	ZENITH_ASSERT_GT(xToss.z, xServerPos.z - 1.0f);
}

// Plan unit-test #7 + the contact-DISPATCH wiring (the class of regression the human
// reviewer flagged: a gate that awards the wrong side / launches on a miss). The launch
// physics is windowed-verified (no valid body headless), but the non-physics dispatch —
// miss -> the OTHER side scores, unarmed -> discard, in-range+armed -> launch — runs
// through the SAME ResolveContactOutcome the production path calls.
ZENITH_TEST(RenderTestTennis, ContactDispatchOutOfRangeAwardsOpponent)
{
	TennisMatchFixture xFix;
	// Striker 0 swings out of range -> a genuine miss -> the OPPONENT (side 1) scores,
	// no launch. (Catches a ResolvePoint(iStriker) mutation: winner must be 1, not 0.)
	ZENITH_ASSERT_FALSE(xFix.Referee().ResolveContactOutcome(0, /*inRange*/false, /*armed*/true));
	ZENITH_ASSERT_EQ(static_cast<int>(xFix.Referee().GetPhase()), static_cast<int>(POINT_PHASE_POINT_OVER));
	ZENITH_ASSERT_EQ(xFix.Referee().GetPendingWinner(), 1);
}

ZENITH_TEST(RenderTestTennis, ContactDispatchOtherStrikerAwardsOpponent)
{
	TennisMatchFixture xFix;
	// Symmetric: striker 1's miss awards side 0.
	ZENITH_ASSERT_FALSE(xFix.Referee().ResolveContactOutcome(1, false, true));
	ZENITH_ASSERT_EQ(xFix.Referee().GetPendingWinner(), 0);
}

ZENITH_TEST(RenderTestTennis, ContactDispatchUnarmedDiscardsKeepsBallLive)
{
	TennisMatchFixture xFix;
	const int iPhase0 = static_cast<int>(xFix.Referee().GetPhase());
	// In range but no armed shot -> discard: no launch, no point, phase unchanged.
	ZENITH_ASSERT_FALSE(xFix.Referee().ResolveContactOutcome(0, /*inRange*/true, /*armed*/false));
	ZENITH_ASSERT_EQ(static_cast<int>(xFix.Referee().GetPhase()), iPhase0);
}

ZENITH_TEST(RenderTestTennis, ContactDispatchInRangeArmedLaunches)
{
	TennisMatchFixture xFix;
	const int iPhase0 = static_cast<int>(xFix.Referee().GetPhase());
	// In range + armed -> the caller launches (returns true); no point resolved.
	ZENITH_ASSERT_TRUE(xFix.Referee().ResolveContactOutcome(0, true, true));
	ZENITH_ASSERT_EQ(static_cast<int>(xFix.Referee().GetPhase()), iPhase0);
}

// Plan unit-test #5 (D5 trap): an authored AIAgent with an empty BT-asset string and a
// tree wired by the brain must NOT self-disable at OnStart (a self-disabled agent skips
// its nav update and freezes on its last velocity). The brain pre-builds + wires the
// tree before the AIAgent's OnStart in authored order, so the asset stays empty.
ZENITH_TEST(RenderTestTennis, AIAgentDoesNotSelfDisableWithEmptyAsset)
{
	TennisMatchFixture xFix;
	// Fixture drove OnAwake -> Brain.OnStart (wires the tree) already; now run the
	// AIAgent's own OnStart and confirm it stays enabled.
	for (int i = 0; i < 2; ++i)
	{
		xFix.AIAgent(i).SetEnabled(true);   // play phase: agent enabled
		xFix.AIAgent(i).OnStart();          // must leave it enabled (empty asset + wired tree)
		ZENITH_ASSERT_TRUE(xFix.AIAgent(i).GetBehaviorTree() != nullptr, "tree wired before AIAgent OnStart");
		ZENITH_ASSERT_TRUE(xFix.AIAgent(i).IsEnabled(), "AIAgent must not self-disable with empty asset + non-null tree");
	}
}

// New body getter (plan Phase-1): IsFacingPositiveZ tracks the side passed to Init.
ZENITH_TEST(RenderTestTennis, PlayerFacingMatchesInitSide)
{
	TennisBrainFixture xFix;
	Zenith_Entity xNear = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("F_Near", /*near*/true));
	Zenith_Entity xFar  = xFix.pxSceneData->GetEntity(xFix.MakeAgentEntity("F_Far",  /*near*/false));
	// Near side faces +Z (toward the far baseline); far side faces -Z.
	ZENITH_ASSERT_TRUE(xNear.GetComponent<RenderTest_TennisPlayerComponent>().IsFacingPositiveZ());
	ZENITH_ASSERT_FALSE(xFar.GetComponent<RenderTest_TennisPlayerComponent>().IsFacingPositiveZ());
}

// Referee analogue of TennisBrainMoveAssignClearsLiveBorrow: move-ASSIGNING into a live
// referee must null its old NPCs' nav-agent borrows before freeing the agents (P3). This
// is the only ClearNavBorrows branch production pooling never exercises (it move-CONSTRUCTs
// only), so pin it here. Survival = no crash / no dangling borrow at scope exit.
ZENITH_TEST(RenderTestTennis, RefereeMoveAssignClearsLiveNavBorrow)
{
	TennisMatchFixture xFix;
	// The referee wired heap nav agents onto both AIAgents in OnStart.
	ZENITH_ASSERT_NOT_NULL(xFix.AIAgent(0).GetNavMeshAgent(), "nav borrow live before move-assign");

	RenderTest_TennisMatchComponent& xRef = xFix.Referee();
	Zenith_Entity xTmpEnt = xFix.pxSceneData->GetEntity(xFix.xMatchID);
	RenderTest_TennisMatchComponent xOther(xTmpEnt);   // empty (no nav, no NPCs resolved)
	xRef = std::move(xOther);                           // must ClearNavBorrows() before DeleteNav()
	// The live referee's NPCs' borrows were nulled (not left dangling at freed agents).
	ZENITH_ASSERT_TRUE(xFix.AIAgent(0).GetNavMeshAgent() == nullptr, "move-assign nulled the stale nav borrow");
	ZENITH_ASSERT_TRUE(xFix.AIAgent(1).GetNavMeshAgent() == nullptr, "move-assign nulled the stale nav borrow");
}

ZENITH_TEST(RenderTestTennis, IntegrationCleanTeardown)
{
	// Build + tear down the full mini-scene: OnDestroy frees the nav + unregisters
	// perception + releases the BTs with no double-free (scope exit = the test).
	{
		TennisMatchFixture xFix;
		xFix.Referee().OnLateUpdate(1.0f / 60.0f);
	}
	ZENITH_ASSERT_TRUE(true, "full tennis scene built + torn down cleanly");
}
