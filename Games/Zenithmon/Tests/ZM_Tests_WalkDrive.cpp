#include "Zenith.h"

// ============================================================================
// ZM_Tests_WalkDrive (ZM-D-218) -- the boot units for the ONE walk driver every
// automated test steers the player with.
//
// ★★ WHAT THIS SUITE EXISTS TO PREVENT. The driver is camera-relative and
// QUANTISED TO EIGHT DIRECTIONS, so the player never walks the straight line to
// the target: the path bows away from it, and the lateral offset grows with the
// length of the walk. That is harmless while a test only asks "did the player
// ARRIVE" and it is fatal the moment something measures a BEARING -- an armed
// trainer's sight cone, a facing check, a camera clamp. Rival Vesper was
// correctly placed, exactly facing, armed and WATCHING with the player driven to
// 0.077 m of him and permanently blind, reported as "the walk-up STALLED".
//
// The FIXED POINTS below are what made that fixable: a target due N/S/E/W of the
// walk's origin, or on an exact 45-degree diagonal, drives a straight line under
// every camera orientation. Before this suite that was a paragraph in a decision
// log. It is a compiled fact now, which is the whole reason a placement rule is
// allowed to depend on it.
//
// PURE by construction: ZM_ComputeWalkDriveKeys takes the camera basis as an
// ARGUMENT, so every bearing is reachable without booting a scene, and none of
// these units touch input, physics or the ECS.
// ============================================================================

#include "Zenithmon/Tests/ZM_TestWalkDrive.h"

#include <cmath>
#include <limits>

namespace
{
	Zenith_Maths::Vector3 WD(float fX, float fZ)
	{
		return Zenith_Maths::Vector3(fX, 0.0f, fZ);
	}

	// The four world-axis camera orientations, which is every resting
	// orientation ZM_FollowCamera settles into when the player stops turning.
	const Zenith_Maths::Vector3 axWD_CARDINAL_CAMERAS[4] =
	{
		Zenith_Maths::Vector3( 0.0f, 0.0f,  1.0f),
		Zenith_Maths::Vector3( 1.0f, 0.0f,  0.0f),
		Zenith_Maths::Vector3( 0.0f, 0.0f, -1.0f),
		Zenith_Maths::Vector3(-1.0f, 0.0f,  0.0f),
	};

	u_int WDHeldCount(const ZM_WalkDriveKeys& xKeys)
	{
		u_int uCount = 0u;
		for (int i = 0; i < ZM_WALK_DRIVE_KEY_COUNT; ++i)
		{
			uCount += xKeys.m_abKeys[i] ? 1u : 0u;
		}
		return uCount;
	}

	// The world-space direction the held keys actually move the player, given
	// the camera basis they were chosen against. This is the arithmetic
	// ZM_PlayerController::BuildCameraRelativeDirection performs, restated here
	// because that path needs a live scene and this suite must not.
	Zenith_Maths::Vector3 WDMotion(const ZM_WalkDriveKeys& xKeys,
		const Zenith_Maths::Vector3& xCameraForward)
	{
		Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
		const float fLenSq = xForward.x * xForward.x + xForward.z * xForward.z;
		if (fLenSq <= 0.000001f)
		{
			xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		}
		else
		{
			xForward /= std::sqrt(fLenSq);
		}
		const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);

		Zenith_Maths::Vector3 xMotion(0.0f, 0.0f, 0.0f);
		if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_W]) { xMotion += xForward; }
		if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_S]) { xMotion -= xForward; }
		if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_D]) { xMotion += xRight; }
		if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_A]) { xMotion -= xRight; }
		return xMotion;
	}

	// cos(angle) between the motion the driver produces and the true bearing to
	// the target. 1.0 means it is walking straight at it. Returns 1.0 when there
	// is no motion, so an "arrived" frame never reads as a steering error.
	float WDMotionAlignment(const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xTarget,
		const Zenith_Maths::Vector3& xCameraForward)
	{
		const ZM_WalkDriveKeys xKeys =
			ZM_ComputeWalkDriveKeys(xPosition, xTarget, xCameraForward);
		const Zenith_Maths::Vector3 xMotion = WDMotion(xKeys, xCameraForward);
		const float fMotionLen =
			std::sqrt(xMotion.x * xMotion.x + xMotion.z * xMotion.z);
		if (fMotionLen <= 0.000001f)
		{
			return 1.0f;
		}
		const float fDX = xTarget.x - xPosition.x;
		const float fDZ = xTarget.z - xPosition.z;
		const float fWantLen = std::sqrt(fDX * fDX + fDZ * fDZ);
		if (fWantLen <= 0.000001f)
		{
			return 1.0f;
		}
		return (xMotion.x * fDX + xMotion.z * fDZ) / (fMotionLen * fWantLen);
	}
}

// ---------------------------------------------------------------------------
// (1) The basis. W/A/S/D mean what the CAMERA says they mean, not what the world
//     does -- the defect ZM-D-130/131 fixed, and the reason this file cannot
//     just compare world dx/dz.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, KeysAreChosenInTheCameraBasisNotTheWorldBasis)
{
	// Target due +X of the player. With the camera on +Z that is the player's
	// RIGHT, so D. With the camera on +X it is straight AHEAD, so W. A driver
	// reading raw world dx/dz would answer the same key both times.
	const ZM_WalkDriveKeys xOnZ =
		ZM_ComputeWalkDriveKeys(WD(0.0f, 0.0f), WD(10.0f, 0.0f), WD(0.0f, 1.0f));
	ZENITH_ASSERT_TRUE(xOnZ.m_abKeys[ZM_WALK_DRIVE_KEY_D],
		"a target due +X with the camera on +Z is the player's RIGHT, so D");
	ZENITH_ASSERT_FALSE(xOnZ.m_abKeys[ZM_WALK_DRIVE_KEY_W],
		"...and NOT forward");

	const ZM_WalkDriveKeys xOnX =
		ZM_ComputeWalkDriveKeys(WD(0.0f, 0.0f), WD(10.0f, 0.0f), WD(1.0f, 0.0f));
	ZENITH_ASSERT_TRUE(xOnX.m_abKeys[ZM_WALK_DRIVE_KEY_W],
		"the SAME target with the camera on +X is straight ahead, so W");
	ZENITH_ASSERT_FALSE(xOnX.m_abKeys[ZM_WALK_DRIVE_KEY_D],
		"...and NOT a strafe. If both of these hold the same key, the driver has "
		"regressed to raw world dx/dz -- see ZM-D-130/131");
}

// ---------------------------------------------------------------------------
// (2) ★★ THE FIXED POINTS. This is the unit a map layout is allowed to lean on.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, FixedPointsDriveAStraightLine)
{
	// Every world-axis bearing and every 45-degree diagonal, against every
	// resting camera orientation. The claim is exact: the motion the driver
	// produces is EXACTLY the bearing to the target, so the lateral offset that
	// bows an ordinary approach never opens at all.
	const Zenith_Maths::Vector3 axFixedPointTargets[8] =
	{
		WD( 40.0f,   0.0f), WD(-40.0f,   0.0f),
		WD(  0.0f,  40.0f), WD(  0.0f, -40.0f),
		WD( 40.0f,  40.0f), WD( 40.0f, -40.0f),
		WD(-40.0f,  40.0f), WD(-40.0f, -40.0f),
	};

	for (u_int uCam = 0u; uCam < 4u; ++uCam)
	{
		for (u_int uTgt = 0u; uTgt < 8u; ++uTgt)
		{
			const float fAlignment = WDMotionAlignment(
				WD(0.0f, 0.0f), axFixedPointTargets[uTgt], axWD_CARDINAL_CAMERAS[uCam]);
			ZENITH_ASSERT_GT(fAlignment, 0.9999f,
				"target (%.0f, %.0f) is on a world axis or an exact 45-degree "
				"diagonal, so it MUST be a fixed point of the driver -- the motion "
				"should be exactly the bearing to it. Alignment is %.5f against "
				"camera (%.0f, %.0f). Rival Vesper's placement depends on this: "
				"see MapLayoutPlaybook.md 3.3",
				axFixedPointTargets[uTgt].x, axFixedPointTargets[uTgt].z, fAlignment,
				axWD_CARDINAL_CAMERAS[uCam].x, axWD_CARDINAL_CAMERAS[uCam].z);
		}
	}
}

// ---------------------------------------------------------------------------
// (3) ...and the ANTI-VACUITY half: ordinary bearings are NOT fixed points.
//     Without this, (2) would still pass if the driver were silently replaced by
//     an exact steering controller -- and the placement rule it justifies would
//     become cargo cult.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, AnOrdinaryBearingIsNotAFixedPointAndTheErrorIsBounded)
{
	const Zenith_Maths::Vector3 xCamera = WD(0.0f, 1.0f);
	float fWorstAlignment = 2.0f;
	float fWorstDegrees = 0.0f;
	for (u_int u = 0u; u < 360u; ++u)
	{
		const float fRadians = 0.01745329f * (float)u;
		const Zenith_Maths::Vector3 xTarget =
			WD(std::cos(fRadians) * 40.0f, std::sin(fRadians) * 40.0f);
		const float fAlignment = WDMotionAlignment(WD(0.0f, 0.0f), xTarget, xCamera);
		if (fAlignment < fWorstAlignment)
		{
			fWorstAlignment = fAlignment;
			fWorstDegrees = (float)u;
		}
	}

	// It really does misalign...
	ZENITH_ASSERT_LT(fWorstAlignment, 0.999f,
		"NO bearing misaligned the driver (worst %.5f at %.0f degrees). Either the "
		"quantisation is gone -- in which case the fixed-point rule this game's "
		"trainer placement depends on is now cargo cult and MapLayoutPlaybook.md "
		"3.3 must be rewritten -- or this sweep is not reaching the driver",
		fWorstAlignment, fWorstDegrees);

	// ★★ ...and the bound is 45 DEGREES, NOT 22.5, WHICH IS THE WHOLE MECHANISM.
	// A first draft of this clause asserted 22.5 -- the bound you get if you
	// picture the driver choosing the NEAREST of eight directions -- and it went
	// red at 44.0 degrees. It does not choose the nearest of anything. Each axis
	// independently compares its own remaining error against a dead zone measured
	// in METRES, so as soon as BOTH axes are outside it the driver commits to a
	// full 45-degree diagonal however lopsided the two errors are. A target 40 m
	// ahead and 0.5 m to the side is 0.7 degrees off the axis and gets a diagonal.
	//
	// THAT is what bows the path: the player runs 45 degrees off, closes the small
	// axis in a fraction of a second, drops to the single-key direction, drifts
	// back out, and repeats. cos(45) = 0.70711 is the supremum and is never
	// reached, because reaching it needs the small axis to sit exactly ON the dead
	// zone.
	ZENITH_ASSERT_GT(fWorstAlignment, 0.70711f,
		"the worst misalignment is %.5f at %.0f degrees, WORSE than the 45 degrees "
		"(cos = 0.70711) that a per-axis dead zone can account for. The driver is "
		"choosing a direction that is not one of the eight, which is a different "
		"and much more serious defect than the quantisation",
		fWorstAlignment, fWorstDegrees);
	ZENITH_ASSERT_LT(fWorstAlignment, 0.7500f,
		"the worst misalignment is only %.5f at %.0f degrees -- the sweep never "
		"found the near-45-degree case, so this clause is no longer measuring the "
		"mechanism it describes",
		fWorstAlignment, fWorstDegrees);
}

// ---------------------------------------------------------------------------
// (3b) The mechanism itself, on one constructed case, so the sweep above is not
//      the only thing standing between a reader and the explanation.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, ATinyLateralErrorStillBuysAFullDiagonal)
{
	const Zenith_Maths::Vector3 xCamera = WD(0.0f, 1.0f);

	// 40 m ahead, half a metre to the side: 0.7 degrees off dead ahead.
	const ZM_WalkDriveKeys xKeys =
		ZM_ComputeWalkDriveKeys(WD(0.0f, 0.0f), WD(0.5f, 40.0f), xCamera);
	ZENITH_ASSERT_TRUE(xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_W],
		"forward key not held for a target 40 m ahead");
	ZENITH_ASSERT_TRUE(xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_D],
		"a 0.5 m lateral error is %.1fx the %.2f m dead zone, so the strafe key IS "
		"held -- and the player therefore runs a full 45-degree diagonal for a "
		"target 0.7 degrees off dead ahead. If this ever stops being true the "
		"driver has grown a proportional or angular term, and the pursuit-curve "
		"argument in MapLayoutPlaybook.md 3.3 needs re-deriving from scratch",
		0.5f / fZM_WALK_DRIVE_DEAD_ZONE, fZM_WALK_DRIVE_DEAD_ZONE);

	const float fAlignment =
		WDMotionAlignment(WD(0.0f, 0.0f), WD(0.5f, 40.0f), xCamera);
	ZENITH_ASSERT_LT(fAlignment, 0.7200f,
		"a target 0.7 degrees off dead ahead should be walked at ~45 degrees off "
		"(alignment ~0.7124), not %.5f", fAlignment);

	// ...and shrinking the lateral error INSIDE the dead zone straightens it out
	// completely. This is the pair that makes the dead zone the deciding term.
	const float fStraight = WDMotionAlignment(
		WD(0.0f, 0.0f), WD(fZM_WALK_DRIVE_DEAD_ZONE * 0.5f, 40.0f), xCamera);
	ZENITH_ASSERT_GT(fStraight, 0.9999f,
		"with the lateral error INSIDE the dead zone the walk must be exactly "
		"straight; alignment is %.5f", fStraight);
}

// ---------------------------------------------------------------------------
// (4) The dead zone IS the arrival condition -- the driver has no other one.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, TheDeadZoneIsTheArrivalCondition)
{
	const Zenith_Maths::Vector3 xCamera = WD(0.0f, 1.0f);

	const ZM_WalkDriveKeys xCoincident =
		ZM_ComputeWalkDriveKeys(WD(5.0f, 5.0f), WD(5.0f, 5.0f), xCamera);
	ZENITH_ASSERT_TRUE(xCoincident.m_bArrived,
		"standing on the target must report ARRIVED");
	ZENITH_ASSERT_EQ(WDHeldCount(xCoincident), 0u,
		"standing on the target must hold no key");

	// Just inside the dead zone on both axes.
	const float fInside = fZM_WALK_DRIVE_DEAD_ZONE * 0.5f;
	const ZM_WalkDriveKeys xInside = ZM_ComputeWalkDriveKeys(
		WD(0.0f, 0.0f), WD(fInside, fInside), xCamera);
	ZENITH_ASSERT_TRUE(xInside.m_bArrived,
		"a target %.3f m away on each axis is inside the %.3f m dead zone and must "
		"report ARRIVED -- a caller polling a distance instead of this flag will "
		"wait forever", fInside, fZM_WALK_DRIVE_DEAD_ZONE);

	// Just outside on one axis only.
	const float fOutside = fZM_WALK_DRIVE_DEAD_ZONE * 2.0f;
	const ZM_WalkDriveKeys xOutside = ZM_ComputeWalkDriveKeys(
		WD(0.0f, 0.0f), WD(fInside, fOutside), xCamera);
	ZENITH_ASSERT_FALSE(xOutside.m_bArrived,
		"one axis outside the dead zone is NOT arrival");
	ZENITH_ASSERT_EQ(WDHeldCount(xOutside), 1u,
		"one axis outside the dead zone holds exactly one key");
	ZENITH_ASSERT_TRUE(xOutside.m_abKeys[ZM_WALK_DRIVE_KEY_W],
		"...and with the camera on +Z that key is W");
}

// ---------------------------------------------------------------------------
// (5) Never more than one key per axis -- W and S, or A and D, together would
//     cancel and the player would stand still while the watchdog called it a
//     stall.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, OpposedKeysAreNeverHeldTogether)
{
	for (u_int uCam = 0u; uCam < 4u; ++uCam)
	{
		for (u_int u = 0u; u < 360u; ++u)
		{
			const float fRadians = 0.01745329f * (float)u;
			const ZM_WalkDriveKeys xKeys = ZM_ComputeWalkDriveKeys(
				WD(0.0f, 0.0f),
				WD(std::cos(fRadians) * 25.0f, std::sin(fRadians) * 25.0f),
				axWD_CARDINAL_CAMERAS[uCam]);
			ZENITH_ASSERT_FALSE(
				xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_W] && xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_S],
				"W and S are held together at %u degrees -- the player would stand "
				"still and the phase watchdog would report a stall", u);
			ZENITH_ASSERT_FALSE(
				xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_A] && xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_D],
				"A and D are held together at %u degrees -- same failure, sideways", u);
			ZENITH_ASSERT_LE(WDHeldCount(xKeys), 2u,
				"more than two keys held at %u degrees", u);
			ZENITH_ASSERT_GE(WDHeldCount(xKeys), 1u,
				"NO key held at %u degrees for a target 25 m away -- the driver has "
				"stopped driving", u);
		}
	}
}

// ---------------------------------------------------------------------------
// (6) Totality. The units feed it degenerate values on purpose; nothing here may
//     assert, and nothing may hold a key it cannot justify.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, EveryDegenerateInputFailsClosed)
{
	const float fNAN = std::numeric_limits<float>::quiet_NaN();
	const float fINF = std::numeric_limits<float>::infinity();
	const Zenith_Maths::Vector3 xCamera = WD(0.0f, 1.0f);

	const Zenith_Maths::Vector3 axBad[4] =
	{
		WD(fNAN, 0.0f), WD(0.0f, fNAN), WD(fINF, 0.0f), WD(0.0f, -fINF),
	};
	for (u_int u = 0u; u < 4u; ++u)
	{
		const ZM_WalkDriveKeys xFromPos =
			ZM_ComputeWalkDriveKeys(axBad[u], WD(40.0f, 40.0f), xCamera);
		ZENITH_ASSERT_EQ(WDHeldCount(xFromPos), 0u,
			"a non-finite POSITION (case %u) held a key -- the player would run in "
			"a fixed direction until the phase deadline, which reads as a stall", u);
		ZENITH_ASSERT_FALSE(xFromPos.m_bArrived,
			"a non-finite POSITION (case %u) reported ARRIVED. Holding no key and "
			"having arrived are different answers and only one of them is true", u);

		const ZM_WalkDriveKeys xFromTgt =
			ZM_ComputeWalkDriveKeys(WD(0.0f, 0.0f), axBad[u], xCamera);
		ZENITH_ASSERT_EQ(WDHeldCount(xFromTgt), 0u,
			"a non-finite TARGET (case %u) held a key", u);
		ZENITH_ASSERT_FALSE(xFromTgt.m_bArrived,
			"a non-finite TARGET (case %u) reported ARRIVED", u);

		const ZM_WalkDriveKeys xFromCam =
			ZM_ComputeWalkDriveKeys(WD(0.0f, 0.0f), WD(40.0f, 40.0f), axBad[u]);
		ZENITH_ASSERT_EQ(WDHeldCount(xFromCam), 0u,
			"a non-finite CAMERA FORWARD (case %u) held a key", u);
		ZENITH_ASSERT_FALSE(xFromCam.m_bArrived,
			"a non-finite CAMERA FORWARD (case %u) reported ARRIVED", u);
	}

	// A camera with no XZ facing at all -- looking straight down, or absent.
	// This one does NOT fail closed: it degenerates to +Z, ZM_FollowCamera's
	// resting orientation, because a test whose camera has not settled yet still
	// has to be able to walk.
	const ZM_WalkDriveKeys xStraightDown = ZM_ComputeWalkDriveKeys(
		WD(0.0f, 0.0f), WD(0.0f, 40.0f), Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));
	ZENITH_ASSERT_TRUE(xStraightDown.m_abKeys[ZM_WALK_DRIVE_KEY_W],
		"a camera with no XZ facing must fall back to +Z and still drive, not "
		"freeze the player");
}

// ---------------------------------------------------------------------------
// (7) Y is ignored on every argument. A target on a rooftop is the same walk as
//     one on the ground; height is somebody else's problem.
// ---------------------------------------------------------------------------
ZENITH_TEST(ZM_WalkDrive, HeightIsIgnoredOnEveryArgument)
{
	const ZM_WalkDriveKeys xFlat = ZM_ComputeWalkDriveKeys(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(12.0f, 0.0f, 7.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	const ZM_WalkDriveKeys xTall = ZM_ComputeWalkDriveKeys(
		Zenith_Maths::Vector3(0.0f, -400.0f, 0.0f),
		Zenith_Maths::Vector3(12.0f, 900.0f, 7.0f),
		Zenith_Maths::Vector3(0.0f, 0.9f, 1.0f));
	for (int i = 0; i < ZM_WALK_DRIVE_KEY_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ(xFlat.m_abKeys[i] ? 1u : 0u, xTall.m_abKeys[i] ? 1u : 0u,
			"key %d differs between an XZ-identical pair that differ only in Y -- "
			"this driver is planar and a Y term has crept in", i);
	}
}
