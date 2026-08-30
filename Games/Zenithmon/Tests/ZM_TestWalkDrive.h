#pragma once

#include "Maths/Zenith_Maths.h"   // Zenith_Maths::Vector3

// ============================================================================
// ZM_TestWalkDrive -- THE ONE walk driver every automated test uses to steer the
// player toward a world-space XZ point.
//
// ★★ IT USED TO BE EIGHT COPIES. `DriveTowardXZ` was duplicated, byte-identical
// in shape, into ZM_AutoTests_{IntroBeat, LabRoundTrip, NpcServices, NpcTalk,
// RivalVesper, SaveResume, TrainerSight, WorldTraversal}.cpp. That is why the
// camera-basis fix at ZM-D-130/131 had to be applied three times (there were only
// three copies then, and the count in Shortfalls.md still said three when there
// were eight), and why the pursuit-curve finding at ZM-D-218 applied to seven
// suites nobody had looked at. One home, and the arithmetic is a PURE function
// with its own units.
//
// ★★ THIS DRIVER DOES NOT WALK IN A STRAIGHT LINE, AND THAT IS ITS DEFINING
// PROPERTY. It is CAMERA-RELATIVE and QUANTISED TO EIGHT DIRECTIONS -- it holds
// W/A/S/D, never a steering angle -- while ZM_FollowCamera swings to follow the
// player's own heading. So the basis rotates under it and the player walks a
// PURSUIT CURVE that cuts the corner; the lateral offset from the straight line
// grows with the length of the walk, measured at 4.44 m over a 48.8 m approach.
//
// That is invisible while a test only asks "did the player ARRIVE", and it stops
// being invisible the moment something measures a BEARING. An armed trainer's
// 8 m / 60-degree sight cone does: rival Vesper was correctly placed, exactly
// facing, armed and WATCHING with the player driven to 0.077 m of him -- and
// permanently blind, because the player came in 31 degrees off a 30-degree cone.
// The failure read as "the walk-up STALLED", which points at obstacles.
//
// ★ THE FIXED POINTS. A target due N/S/E/W of the walk's origin, or on an exact
// 45-degree diagonal, drives a straight line under BOTH camera orientations, so
// the lateral offset stays at zero. Ordinary bearings in between do not.
// ZM_WalkDrive_FixedPointsDriveAStraightLine and its neighbours in
// Tests/ZM_Tests_WalkDrive.cpp lock that, so it is a mechanical fact now rather
// than a paragraph. Games/Zenithmon/Docs/MapLayoutPlaybook.md section 3.3 has the
// measured decay table; anything a test must WALK UP TO wants a fixed point.
//
// ★ NO OBSTACLE AVOIDANCE, deliberately -- a real player has none either. A
// collider on the line wedges the walk and the suite dies naming a DISTANCE
// rather than naming the blocker, which is what ZM_DawnmereDressing's keep-out
// exists to prevent.
// ============================================================================

// The dead zone, in world units, on EACH camera-space axis. An axis whose
// remaining error is inside it holds no key -- which is also how "arrived" is
// spelled, since the driver has no other stopping condition.
inline constexpr float fZM_WALK_DRIVE_DEAD_ZONE = 0.08f;

// The four movement keys, in the index order every caller's `m_abHeldKeys[4]`
// already used before consolidation. Do not renumber: five suites log these
// straight into a failure message as `W=%d A=%d S=%d D=%d`.
enum ZM_WALK_DRIVE_KEY
{
	ZM_WALK_DRIVE_KEY_W = 0,
	ZM_WALK_DRIVE_KEY_A = 1,
	ZM_WALK_DRIVE_KEY_S = 2,
	ZM_WALK_DRIVE_KEY_D = 3,
	ZM_WALK_DRIVE_KEY_COUNT = 4
};

struct ZM_WalkDriveKeys
{
	bool m_abKeys[ZM_WALK_DRIVE_KEY_COUNT] = { false, false, false, false };
	// True iff BOTH camera-space axes are inside the dead zone, i.e. no key is
	// held because the target has been reached rather than because the input was
	// degenerate. A caller polling for arrival should read this rather than
	// re-deriving it from a distance, so its notion of "close enough" is the
	// driver's own.
	bool m_bArrived = false;

	bool AnyHeld() const
	{
		return m_abKeys[0] || m_abKeys[1] || m_abKeys[2] || m_abKeys[3];
	}

	// For the five suites that mirror the held keys into their own bool[4] so a
	// failure message can print them.
	void CopyTo(bool (&abOut)[ZM_WALK_DRIVE_KEY_COUNT]) const
	{
		for (int i = 0; i < ZM_WALK_DRIVE_KEY_COUNT; ++i)
		{
			abOut[i] = m_abKeys[i];
		}
	}
};

// ---- THE PURE HALF ---------------------------------------------------------
//
// Which keys a player would hold to move from xPosition toward xTarget, given
// the camera they are looking through. NO engine, NO input, NO camera lookup, NO
// statics, NO allocation -- so the quantisation this whole file is about can be
// unit-tested at every bearing without booting a scene.
//
// Y IS IGNORED on all three arguments; this is an XZ driver.
//
// TOTALITY -- the complete table. It never calls Zenith_Assert:
//   * a camera forward whose XZ projection is (near) zero -> falls back to +Z,
//     which is the resting orientation of ZM_FollowCamera.
//   * ANY non-finite component of the position, the target, or the camera
//     forward -> NO keys held and m_bArrived FALSE. Fail closed: a driver that
//     held a key on a NaN would run the player off in a fixed direction until
//     the phase deadline, which reads exactly like a stall. (This matches the
//     pre-consolidation behaviour exactly rather than changing it -- every
//     comparison against a NaN was already false -- but it is now DELIBERATE,
//     and m_bArrived correctly says "no" instead of inheriting "no key held".)
//   * coincident position and target -> no keys, m_bArrived TRUE.
ZM_WalkDriveKeys ZM_ComputeWalkDriveKeys(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xTarget,
	const Zenith_Maths::Vector3& xCameraForward);

// ---- THE IMPURE HALF -------------------------------------------------------
//
// Resolves the live main camera, computes the keys, and submits them through
// Zenith_InputSimulator. Returns what it held so a caller can mirror it into its
// own diagnostics.
//
// ★★ IT DOES NOT CLEAR INPUT, AND ITS CALLERS MUST. Each of the eight originals
// opened with its OWN Clear*Input(), and those clears are not interchangeable --
// they release different key sets (ZM_AutoTests_SaveResume.cpp's also releases
// RIGHT SHIFT, and two of them take a walk-state struct and reset its held-key
// mirror). Folding one of them in here would release keys another caller
// deliberately holds.
//
// So every wrapper keeps its own clear as its first statement. The first draft of
// this consolidation moved them out, on the assumption that the call sites did
// the clearing; they do not, last frame's keys stayed held, the walks curved away
// from their targets and SIX automated tests went red in one batch. The
// assumption was written into this comment as a fact before it was checked.
//
// bRun holds LEFT SHIFT for the whole frame. Seven of the eight original copies
// did; ZM_AutoTests_IntroBeat.cpp's deliberately did not, because the intro beat
// is timed against a WALKING pace.
ZM_WalkDriveKeys ZM_DriveWalkTowardXZ(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xTarget,
	bool bRun);
