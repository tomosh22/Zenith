#pragma once

#include <cmath>   // sqrt -- the arrival standoff, which nothing authored reads

#include "Maths/Zenith_Maths.h"   // Vector3
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // fZM_INTERACT_MAX_DISTANCE (the reach Aster must stay outside of)
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE human body contract

// ============================================================================
// ZM_ProfLabPlacement (S8 SC1) -- the authored ProfLab interior coordinates that
// BOTH the tools scene authoring and the tests must agree on, in ONE place.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely. HEADER-ONLY: every accessor is `inline`, so there is no
// paired .cpp to keep in step.
//
// WHY THIS FILE EXISTS. The shipped PlayerHome interior has NO placement header:
// every one of its coordinates is a raw float literal typed inline in
// Zenithmon.cpp's anonymous namespace, which is unnameable from any Tests/ TU.
// That makes "the camera fits behind the player at this doorway" an argument in
// a comment rather than a property a unit checks. ProfLab does not repeat that:
// the authoring READS these values, the boot units READ these values, and the
// automated arrival test compares the COMMITTED .zscen bytes against them.
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. A constant spelled
// twice cannot red a drift -- both sides move together and the assertion becomes
// decorative. If you need a number here, include this header and read it.
//
// ★ THE BUILD INDEX IS NOT HERE. ProfLab's build index (and its scene kind,
// connections and offered spawn tags) live in the compiled world table --
// Source/Data/ZM_WorldSpec.cpp, row ZM_SCENE_PROFLAB. Read it with
// ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex. Duplicating it here would
// create a second inventory nothing reconciles, which is exactly the class of
// defect SC1 exists to close.
// ============================================================================

// ---- The scene, and the entities that are not shell blocks ------------------
// The scene NAME handed to AddStep_CreateScene. The SAVE PATH is deliberately
// not here: GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT is compile-time
// literal concatenation (no std::string, no runtime path build), so it cannot
// consume a const char* constant. The four shipped registrations share that
// shape; ProfLab's matches them.
inline constexpr const char* szZM_PROFLAB_SCENE_NAME = "ProfLab";

// The single arrival marker's TAG. Mirrored from the compiled world table --
// s_aszTagsProfLab[] = { "Door" } in Source/Data/ZM_WorldSpec.cpp -- and the
// boot unit ProfLab_HeaderSpawnTagMatchesTheWorldSpecRow asserts the mirror,
// so a table edit that renamed the tag cannot leave this authoring behind.
inline constexpr const char* szZM_PROFLAB_SPAWN_TAG = "Door";

inline constexpr const char* szZM_PROFLAB_SPAWN_ENTITY_NAME = "ProfLabDoorSpawn";
inline constexpr const char* szZM_PROFLAB_PLAYER_ENTITY_NAME = "Player";
inline constexpr const char* szZM_PROFLAB_CAMERA_ENTITY_NAME = "ProfLabCamera";

// The interior's ONE inhabitant: Professor Aster (GDD 3.1), whose data layer --
// ZM_HUMAN_PROF_ASTER's appearance row and ZM_NPC_PROF_ASTER's TALKER row --
// shipped ahead of this placement. The NAME follows Dawnmere's authored
// "Npc_<Who>" convention, so a reader who knows Npc_RivalVesper recognises this
// one on sight, and it is the key BOTH the automated arrival clause and the
// committed-bytes tripwire look him up by.
inline constexpr const char* szZM_PROFLAB_ASTER_ENTITY_NAME = "Npc_ProfAster";

// ---- The room, as the seven numbers everything else is derived from ---------
//
// ProfLab ("Aster's Lab") is a 20 x 16 m hall -- deliberately NOT PlayerHome's
// 16 x 12 m room, so a registration pointed at the wrong .zscen disagrees on
// geometry as well as on entity names.
//
// The floor's TOP FACE is exactly y = 0, so a spawn marker's FEET height is
// simply 0 and no consumer has to know the slab thickness to place a body.
inline constexpr float fZM_PROFLAB_HALF_WIDTH        = 10.0f;   // +/- X inner extent
inline constexpr float fZM_PROFLAB_HALF_DEPTH        = 8.0f;    // +/- Z inner extent
inline constexpr float fZM_PROFLAB_WALL_THICKNESS    = 0.5f;
inline constexpr float fZM_PROFLAB_WALL_HEIGHT       = 3.5f;
inline constexpr float fZM_PROFLAB_FLOOR_THICKNESS   = 0.5f;

// The entrance is an ABSENCE of geometry, not a door panel: the +Z wall is
// authored as two side panels flanking a gap, with a lintel bridging the top.
// Nothing swings, nothing closes, and nothing carries a rotation -- see the
// closing note at the bottom of this file about AABB vs OBB.
inline constexpr float fZM_PROFLAB_APERTURE_HALF_WIDTH = 3.0f;
inline constexpr float fZM_PROFLAB_APERTURE_HEIGHT     = 3.0f;

// The inner face of the shell at the BACK of the hall -- the -Z limit of the
// volume a body may actually occupy. Derived once here so no consumer re-derives
// "minus half the wall thickness" differently, and READ by
// ZM_GetProfLabCameraBackClearance below, which a boot unit runs.
//
// ★ THE OTHER TWO INNER FACES (+/-X) ARE DELIBERATELY ABSENT. Nothing reads
// them, and an unread constant is not a check -- it is a number that LOOKS like
// one. Every clearance claim in this room measures against ZM_GetProfLabBlock's
// Min()/Max() faces instead, which are the same geometry with an actual reader.
// Add one back only in the same change as the code that reads it.
inline constexpr float fZM_PROFLAB_INNER_MIN_Z =
	-fZM_PROFLAB_HALF_DEPTH + fZM_PROFLAB_WALL_THICKNESS * 0.5f;

// ...and the +Z inner face, i.e. the plane of the doorway wall a player walks
// through on arrival. Added under the rule above and NOT before it: its reader is
// fZM_PROFLAB_ASTER_Z below, which places the professor midway between where the
// player lands and this face.
inline constexpr float fZM_PROFLAB_INNER_MAX_Z =
	fZM_PROFLAB_HALF_DEPTH - fZM_PROFLAB_WALL_THICKNESS * 0.5f;

// The floor's top face, and therefore every FEET height in this scene.
inline constexpr float fZM_PROFLAB_FLOOR_TOP_Y = 0.0f;

// ---- The player capsule -----------------------------------------------------
// The same human body every scene installs. NOT re-spelled here and NOT derived
// from a transform scale: both figures come straight from the ONE compiled body
// contract, so the authoring (which writes ZM_GetProfLabPlayerCenter off the half
// extent) and the arrival (ZM_GameStateManager::CalculateSpawnCenter) cannot
// drift apart -- they now read the same constant rather than two mirrors of it.
inline constexpr float fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT =
	fZM_HUMAN_BODY_HALF_HEIGHT;

// The capsule's XZ half-width. A clearance stated against this cannot be
// satisfied by a body that would actually graze the wall.
inline constexpr float fZM_PROFLAB_PLAYER_RADIUS = fZM_HUMAN_BODY_CAPSULE_RADIUS;

// ---- The arrival marker -----------------------------------------------------
// A FEET anchor (ZM_GameStateManager::CalculateSpawnCenter adds the capsule
// half-extent at warp time), standing just inside the +Z aperture on the shared
// X centreline. The authored Player body sits ON this marker.
//
// ★ THE SIGN OF fZM_PROFLAB_SPAWN_Z IS LOAD-BEARING. The follow camera trails
// toward -Z (see the camera block below), so the room body must lie on the -Z
// side of the arrival point. Moving the spawn to the far end of the hall parks
// the camera behind the back wall.
inline constexpr float fZM_PROFLAB_SPAWN_X      = 0.0f;
inline constexpr float fZM_PROFLAB_SPAWN_FEET_Y = fZM_PROFLAB_FLOOR_TOP_Y;
inline constexpr float fZM_PROFLAB_SPAWN_Z      = 5.0f;

// ---- The camera -------------------------------------------------------------
//
// ★ ZM-D-173, restated for an interior. ZM_FollowCamera keeps the yaw the SCENE
// authored (GetAuthoredYaw), camera forward at yaw 0 is +Z, and the spring
// places the camera BEHIND its subject -- i.e. on the subject's -Z side. So the
// open space has to be at -Z of the arrival point and the entrance has to be the
// +Z face. Author the camera at any other yaw and every clearance figure in this
// file is void, which is why the automated arrival test asserts the CAPTURED yaw
// equals fZM_PROFLAB_CAMERA_YAW before trusting anything else.
inline constexpr float fZM_PROFLAB_CAMERA_YAW   = 0.0f;
inline constexpr float fZM_PROFLAB_CAMERA_PITCH = 0.0f;

// MIRRORED from ZM_FollowCamera's private tuning (GetArmLength / GetCameraHeight
// / GetFOVDegrees), for the same purity reason as the capsule half-extent above.
// ALL THREE are asserted against the real getters by clauses (1a)..(1c) of the
// boot unit ZM_WorldTraversal/ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw
// in Tests/ZM_Tests_ProfLabPlacement.cpp. The authored camera Z is DERIVED from
// the arm, so shortening the arm here moves the authored entity -- which is
// exactly what makes the mirror worth checking.
inline constexpr float fZM_PROFLAB_CAMERA_ARM         = 5.5f;
inline constexpr float fZM_PROFLAB_CAMERA_HEIGHT      = 3.0f;
inline constexpr float fZM_PROFLAB_CAMERA_FOV_DEGREES = 65.0f;
inline constexpr float fZM_PROFLAB_CAMERA_NEAR        = 0.1f;
inline constexpr float fZM_PROFLAB_CAMERA_FAR         = 100.0f;

// ...and the PIVOT the settled camera looks AT, mirrored from
// ZM_FollowCamera::GetPivotHeight() for the same reason as the three above, and
// asserted against the real getter by clause (1d) of the same boot unit. It is a
// lift on the player's body CENTRE, not on the floor -- the camera aims at chest
// height, which is what tilts the arrival frustum and therefore decides what is
// on screen.
inline constexpr float fZM_PROFLAB_CAMERA_PIVOT_HEIGHT = 0.60f;

// Zenith_CameraComponent's shipped default aspect ratio. MIRRORED rather than
// read: the field is private, and a pure boot unit cannot construct a camera
// component (it needs a live entity, and these units create none). Its only
// consumer is the arrival-frustum unit, which for that reason ALSO runs the same
// containment test at an aspect of 1.0 -- a square viewport is the NARROWEST
// horizontal field any aspect >= 1 can give, so passing that arm is a claim no
// window shape can invalidate, whatever this constant says.
inline constexpr float fZM_PROFLAB_CAMERA_ASPECT = 16.0f / 9.0f;

// ---- The shell blockout -----------------------------------------------------

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line identical
// to ZM_DawnmereBlockout in the sibling Source/World/ZM_DawnmerePlacement.h.
// Consolidating both into a shared Source/World/ZM_Blockout.h would touch the
// Dawnmere authoring block and ZM_Tests_DawnmerePlacement.cpp, which is out of
// SC1's scope. Do the merge as its own change, not as a drive-by.
struct ZM_ProfLabBlockout
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// The seven pieces of the shell, in AUTHORING ORDER. The automated arrival test
// walks [0, ZM_PROFLAB_BLOCK_COUNT) and compares each committed entity's
// transform against ZM_GetProfLabBlock's answer, so this order is part of the
// contract: appending a block is fine, reordering rewrites the scene bytes.
enum ZM_PROFLAB_BLOCK : u_int
{
	ZM_PROFLAB_BLOCK_FLOOR,
	ZM_PROFLAB_BLOCK_BACK_WALL,
	ZM_PROFLAB_BLOCK_LEFT_WALL,
	ZM_PROFLAB_BLOCK_RIGHT_WALL,
	ZM_PROFLAB_BLOCK_FRONT_LEFT,
	ZM_PROFLAB_BLOCK_FRONT_RIGHT,
	ZM_PROFLAB_BLOCK_LINTEL,

	ZM_PROFLAB_BLOCK_COUNT
};

// ============================================================================
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetTrainerData house style: no
// argument value, however degenerate, is UB, and none of them calls
// Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration
// and the whole unit suite runs at boot, so an assert on an argument a unit
// deliberately feeds does not fail one test -- it ends the boot unit run and
// takes the whole gate down.
//
// The out-of-range answers are deliberately DEGENERATE rather than plausible: an
// all-zero blockout and a name no authored entity carries, so a caller that
// mistakenly indexes past the end fails loudly at its own assertion instead of
// silently matching a real block.
// ============================================================================

// The floor is the block the automated arrival test looks up by name first, so
// its name is pinned separately for a caller that wants it without spelling an
// enumerator. ZM_GetProfLabBlockName RETURNS THIS CONSTANT rather than a second
// copy of the literal -- one spelling, so the two can never disagree.
inline constexpr const char* szZM_PROFLAB_FLOOR_ENTITY_NAME = "ProfLabFloor";

inline const char* ZM_GetProfLabBlockName(ZM_PROFLAB_BLOCK eBlock)
{
	switch (eBlock)
	{
	case ZM_PROFLAB_BLOCK_FLOOR:       return szZM_PROFLAB_FLOOR_ENTITY_NAME;
	case ZM_PROFLAB_BLOCK_BACK_WALL:   return "ProfLabBackWall";
	case ZM_PROFLAB_BLOCK_LEFT_WALL:   return "ProfLabLeftWall";
	case ZM_PROFLAB_BLOCK_RIGHT_WALL:  return "ProfLabRightWall";
	case ZM_PROFLAB_BLOCK_FRONT_LEFT:  return "ProfLabFrontLeft";
	case ZM_PROFLAB_BLOCK_FRONT_RIGHT: return "ProfLabFrontRight";
	case ZM_PROFLAB_BLOCK_LINTEL:      return "ProfLabLintel";
	default: break;
	}
	return "ProfLabInvalidBlock";
}

// Centre + scale for one shell piece. Every value is DERIVED from the room
// numbers above by a formula spelled once, right here:
//   floor       centre y = -floorThickness/2          (top face lands on y = 0)
//   walls       centre y =  wallHeight/2              (foot on the floor's top)
//   side walls  centre x = +/- halfWidth, depth = 2*halfDepth
//   end walls   centre z = +/- halfDepth, width = 2*halfWidth
//   front pair  width    =  halfWidth - apertureHalfWidth, centred on the
//                           remaining span either side of the opening
//   lintel      spans the aperture and fills wallHeight - apertureHeight above it
inline ZM_ProfLabBlockout ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK eBlock)
{
	constexpr float fFULL_WIDTH = fZM_PROFLAB_HALF_WIDTH * 2.0f;
	constexpr float fFULL_DEPTH = fZM_PROFLAB_HALF_DEPTH * 2.0f;
	constexpr float fWALL_CENTRE_Y = fZM_PROFLAB_WALL_HEIGHT * 0.5f;
	constexpr float fFRONT_PANEL_WIDTH =
		fZM_PROFLAB_HALF_WIDTH - fZM_PROFLAB_APERTURE_HALF_WIDTH;
	constexpr float fFRONT_PANEL_X =
		fZM_PROFLAB_APERTURE_HALF_WIDTH + fFRONT_PANEL_WIDTH * 0.5f;
	constexpr float fLINTEL_HEIGHT =
		fZM_PROFLAB_WALL_HEIGHT - fZM_PROFLAB_APERTURE_HEIGHT;
	constexpr float fLINTEL_CENTRE_Y =
		fZM_PROFLAB_APERTURE_HEIGHT + fLINTEL_HEIGHT * 0.5f;

	switch (eBlock)
	{
	case ZM_PROFLAB_BLOCK_FLOOR:
		return {
			Zenith_Maths::Vector3(
				0.0f, -fZM_PROFLAB_FLOOR_THICKNESS * 0.5f, 0.0f),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_PROFLAB_FLOOR_THICKNESS, fFULL_DEPTH) };

	case ZM_PROFLAB_BLOCK_BACK_WALL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fWALL_CENTRE_Y, -fZM_PROFLAB_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_PROFLAB_WALL_HEIGHT,
				fZM_PROFLAB_WALL_THICKNESS) };

	case ZM_PROFLAB_BLOCK_LEFT_WALL:
		return {
			Zenith_Maths::Vector3(
				-fZM_PROFLAB_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_PROFLAB_WALL_THICKNESS, fZM_PROFLAB_WALL_HEIGHT,
				fFULL_DEPTH) };

	case ZM_PROFLAB_BLOCK_RIGHT_WALL:
		return {
			Zenith_Maths::Vector3(
				fZM_PROFLAB_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_PROFLAB_WALL_THICKNESS, fZM_PROFLAB_WALL_HEIGHT,
				fFULL_DEPTH) };

	case ZM_PROFLAB_BLOCK_FRONT_LEFT:
		return {
			Zenith_Maths::Vector3(
				-fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_PROFLAB_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_PROFLAB_WALL_HEIGHT,
				fZM_PROFLAB_WALL_THICKNESS) };

	case ZM_PROFLAB_BLOCK_FRONT_RIGHT:
		return {
			Zenith_Maths::Vector3(
				fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_PROFLAB_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_PROFLAB_WALL_HEIGHT,
				fZM_PROFLAB_WALL_THICKNESS) };

	case ZM_PROFLAB_BLOCK_LINTEL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fLINTEL_CENTRE_Y, fZM_PROFLAB_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f, fLINTEL_HEIGHT,
				fZM_PROFLAB_WALL_THICKNESS) };

	default: break;
	}
	return { Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f) };
}

// ---- The three derived placements the authoring writes ----------------------

// The arrival marker's FEET position. Callers that need a body CENTRE add the
// capsule half-extent, exactly as ZM_GameStateManager::CalculateSpawnCenter does
// at warp time.
inline Zenith_Maths::Vector3 ZM_GetProfLabSpawnFeet()
{
	return Zenith_Maths::Vector3(
		fZM_PROFLAB_SPAWN_X, fZM_PROFLAB_SPAWN_FEET_Y, fZM_PROFLAB_SPAWN_Z);
}

// The authored Player body's CENTRE: the marker's feet plus the capsule
// half-extent, so the authored body and a warped-in body land on the same point.
inline Zenith_Maths::Vector3 ZM_GetProfLabPlayerCenter()
{
	return Zenith_Maths::Vector3(
		fZM_PROFLAB_SPAWN_X,
		fZM_PROFLAB_SPAWN_FEET_Y + fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT,
		fZM_PROFLAB_SPAWN_Z);
}

// The authored camera position: the arm length straight back along -Z from the
// arrival point, at fZM_PROFLAB_CAMERA_HEIGHT above the FLOOR (y = 0).
//
// ★ THIS IS THE SETTLED POINT IN X AND Z ONLY -- NOT IN Y, AND THE DIFFERENCE IS
// NOT AN ACCIDENT. ZM_FollowCamera::ComputeDesiredPosition adds fCAMERA_HEIGHT to
// the PLAYER'S CENTRE, not to the floor, so the spring settles one capsule
// half-extent HIGHER than the value below (3.0 authored vs 3.9 settled, at
// today's numbers). Do not "fix" that by writing the settled Y here and do not
// write an assertion that expects the two to match in Y -- it would red.
//
// The gap is behaviourally invisible because ZM_FollowCamera::OnStart clears its
// spring state, so the FIRST OnLateUpdate after the scene loads SNAPS the spring
// to ComputeDesiredPosition rather than easing toward it; the authored Y is only
// ever the pose of a camera that has not ticked yet. The shipped PlayerHome
// authors its camera the same way (Zenithmon.cpp, "PlayerHomeCamera" at y = 3.0),
// so this matches the interior that has been running since S1.
inline Zenith_Maths::Vector3 ZM_GetProfLabCameraPosition()
{
	return Zenith_Maths::Vector3(
		fZM_PROFLAB_SPAWN_X,
		fZM_PROFLAB_CAMERA_HEIGHT,
		fZM_PROFLAB_SPAWN_Z - fZM_PROFLAB_CAMERA_ARM);
}

// How much room is left between the authored camera column and the inner face of
// the back wall. Positive means the camera sits inside the hall; a value at or
// below the player radius plus ZM_FollowCamera's collision padding means the arm
// clamp would pull the camera in on arrival and the authored pose is a lie.
//
// This is arithmetic a boot unit RUNS: clause (6) of
// ZM_WorldTraversal/ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw in
// Tests/ZM_Tests_ProfLabPlacement.cpp calls this function and asserts exactly
// that floor, so it is not a claim this file makes on its own behalf.
inline float ZM_GetProfLabCameraBackClearance()
{
	return ZM_GetProfLabCameraPosition().z - fZM_PROFLAB_INNER_MIN_Z;
}

// The pose the arriving player is actually FILMED FROM, which is NOT the authored
// camera entity. ZM_FollowCamera::OnStart clears the spring, so the first late
// update after the scene loads SNAPS the camera to
// ComputeDesiredPosition(bodyCentre, yaw) -- one capsule half-extent above the
// authored Y (see the note on ZM_GetProfLabCameraPosition). Everything that asks
// "what does the player see when they warp in?" has to ask about THIS point, and
// clause (5) of ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw already
// pins it against the shipped ComputeDesiredPosition.
inline Zenith_Maths::Vector3 ZM_GetProfLabSettledCameraPosition()
{
	Zenith_Maths::Vector3 xSettled = ZM_GetProfLabCameraPosition();
	xSettled.y += fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT;
	return xSettled;
}

// ...and the point it AIMS at: the arriving body's centre, lifted by the follow
// camera's pivot height. The settled position and this pivot together are the
// whole arrival frustum -- position, look direction, and (with the FOV/near/far
// above) its opening.
inline Zenith_Maths::Vector3 ZM_GetProfLabArrivalPivot()
{
	return Zenith_Maths::Vector3(
		fZM_PROFLAB_SPAWN_X,
		fZM_PROFLAB_SPAWN_FEET_Y + fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT
			+ fZM_PROFLAB_CAMERA_PIVOT_HEIGHT,
		fZM_PROFLAB_SPAWN_Z);
}

// ---- Professor Aster --------------------------------------------------------
//
// ★ THE CONSTRAINT THAT HAD NO OWNER: BEING ON SCREEN AT ALL. The player warps
// in, the follow camera SNAPS to the settled pose above on its first late update,
// and anything outside THAT frustum is, to the player, an empty room. An anchor
// picked by eye fails this in total silence -- an earlier draft of this placement
// stood him at roughly (-4.5, +1.0), which is about 71.6 degrees off the camera's
// plan-view axis against a ~48.5 degree horizontal half-angle at 16:9, i.e. off
// screen, with every placement unit in this file green. Both coordinates below
// are therefore DERIVED from the constants above, and the boot unit
// ZM_WorldTraversal/ProfLab_AsterStandsInsideTheArrivalFrustum projects the
// result into the frustum built from those SAME constants and asserts it lands
// inside -- feeding the rejected (-4.5, +1.0) pair through the identical
// predicate as its anti-vacuity arm, so the check is known to be able to red.
//
// ★ HE IS AUTHORED AT IDENTITY ROTATION, AND THE AUTHORING EMITS NO ROTATION STEP
// AT ALL (ZM-D-183). AddStep_SetTransformYaw / ...RotationEuler build their
// quaternion with libm AT AUTHORING TIME and MSVC Debug and Release codegen
// disagree by 1-2 ULP, so ProfLab.zscen -- a COMMITTED file re-authored on every
// tools boot -- would ping-pong in git forever, invisible to the same-binary
// pre-save guard. Identity is bit-exact in every configuration, which is why the
// four shipped Dawnmere townsfolk never surfaced this. Aster faces nowhere in
// particular, so he takes no yaw, and that is ALSO what makes his
// COLLISION_VOLUME_TYPE_AABB body legal here: an AABB forces its Jolt body to
// identity and the physics->transform sync writes that identity back into the
// saved bytes, which destroys an authored rotation but cannot destroy an absent
// one (contrast ZM_QueueDawnmereTrainerNpc, where AABB is forbidden forever).

// (1) HIS X -- clear of the walk-in corridor. His CENTRE stands one full body
//     footprint outside the doorway's clear opening, so his own half-width still
//     leaves half a body of gap beside the straight line a player walks in and
//     out along. ONE addition of two named constants, deliberately: a derivation
//     with no association to choose is one that /fp:fast cannot re-order, and
//     re-ordered floating point in an AUTHORED value is exactly how ZM-D-183's
//     sibling finding made committed bytes differ Debug vs Release.
inline constexpr float fZM_PROFLAB_ASTER_X =
	-(fZM_PROFLAB_APERTURE_HALF_WIDTH + fZM_HUMAN_BODY_FOOTPRINT);

// (2) HIS Z -- midway between where the player lands and the inner face of the
//     wall they came through: the greeting spot just inside the entrance, and the
//     furthest ALONG THE CAMERA'S FORWARD AXIS he can stand without crowding the
//     door. Depth is the coordinate that does the work here: the half-width the
//     frustum offers grows linearly with distance from the camera, so a step
//     deeper BUYS lateral room while a step sideways SPENDS it. Every term is
//     dyadic, so this result is exact whatever order the compiler associates in.
inline constexpr float fZM_PROFLAB_ASTER_Z =
	(fZM_PROFLAB_SPAWN_Z + fZM_PROFLAB_INNER_MAX_Z) * 0.5f;

// His authored entity position: a body CENTRE, the same vocabulary every human in
// this game is authored in (ZM_HumanBody.h), standing on the floor's top face.
inline Zenith_Maths::Vector3 ZM_GetProfLabAsterCenter()
{
	return Zenith_Maths::Vector3(
		fZM_PROFLAB_ASTER_X,
		fZM_PROFLAB_FLOOR_TOP_Y + fZM_HUMAN_BODY_HALF_HEIGHT,
		fZM_PROFLAB_ASTER_Z);
}

// The per-NPC reach BONUS every stationary talker is authored with: that NPC's
// OWN AABB half-width, so the global reach is not silently spent crossing his own
// body. Spelled from the body contract rather than as a literal 0.4 -- the
// AUTHORED value is fZM_NPC_AUTHORED_RADIUS in Zenithmon.cpp, and the automated
// arrival clause compares Aster's LIVE ZM_Interactable radius against this
// constant, so the two are pinned to one another rather than merely equal today.
inline constexpr float fZM_PROFLAB_ASTER_REACH_BONUS =
	fZM_HUMAN_BODY_FOOTPRINT * 0.5f;

// ...and the total reach his arrival standoff has to BEAT. ZM_PickInteractTarget
// accepts a candidate at XZ distance <= m_fMaxDistance + probe radius, INCLUSIVE
// at the boundary, so "outside reach" means strictly greater than this.
inline constexpr float fZM_PROFLAB_ASTER_EFFECTIVE_REACH =
	fZM_INTERACT_MAX_DISTANCE + fZM_PROFLAB_ASTER_REACH_BONUS;

// How far the arriving player stands from him, in the XZ plane the picker
// measures in (Y is ignored there). The Dawnmere convention is that you WALK UP
// to talk: arriving already inside reach would let a stray interact press open
// his dialogue from the doormat, before the player has taken a step.
inline float ZM_GetProfLabAsterArrivalStandoff()
{
	const float fDeltaX = fZM_PROFLAB_ASTER_X - fZM_PROFLAB_SPAWN_X;
	const float fDeltaZ = fZM_PROFLAB_ASTER_Z - fZM_PROFLAB_SPAWN_Z;
	return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
}

// ============================================================================
// ★ CLOSING NOTE -- WHY EVERY STATIC COLLIDER IN PROFLAB IS AABB, AND MUST STAY
// AABB.
//
// An AABB collider forces its body to identity rotation, and the
// physics->transform sync writes that identity straight back into the SAVED
// SCENE BYTES with every unit still green -- so anything that must FACE a
// direction needs COLLISION_VOLUME_TYPE_OBB (the same box shape, differing only
// in that it applies the rotation).
//
// NOTHING IN PROFLAB FACES ANYWHERE. The entrance is an ABSENCE of geometry
// between two axis-aligned panels, not a hinged panel; no shell block carries an
// authored rotation; the player is a CAPSULE because it also has to move; and
// Professor Aster is a STATIONARY TALKER with no sight cone and no walk-up, so
// there is no direction for him to hold either -- which is precisely why he can
// wear AABB where the Dawnmere rival never may. So
// AABB is not a shortcut here, it is the correct shape, and "upgrading" these to
// OBB would rewrite the committed .zscen for zero behavioural gain. If a future
// prop in this room DOES need to face somewhere, that prop -- and only that prop
// -- gets OBB.
// ============================================================================
