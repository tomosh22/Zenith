#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3

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
// ★ THE OTHER THREE INNER FACES (+/-X and +Z) ARE DELIBERATELY ABSENT. Nothing
// read them, and an unread constant is not a check -- it is a number that LOOKS
// like one. Every clearance claim in this room measures against
// ZM_GetProfLabBlock's Min()/Max() faces instead, which are the same geometry
// with an actual reader. Add one back only in the same change as the code that
// reads it.
inline constexpr float fZM_PROFLAB_INNER_MIN_Z =
	-fZM_PROFLAB_HALF_DEPTH + fZM_PROFLAB_WALL_THICKNESS * 0.5f;

// The floor's top face, and therefore every FEET height in this scene.
inline constexpr float fZM_PROFLAB_FLOOR_TOP_Y = 0.0f;

// ---- The player capsule -----------------------------------------------------
// The same 0.8 x 1.8 x 0.8 human capsule Dawnmere and PlayerHome author. Hoisted
// because the camera clearance contract is stated in terms of the half-extent
// this scale produces (0.4 m radius + 0.5 m half cylinder = 0.9 m), so a test
// that re-spelled the scale could quietly disagree with the authoring.
inline constexpr float fZM_PROFLAB_PLAYER_SCALE_X = 0.8f;
inline constexpr float fZM_PROFLAB_PLAYER_SCALE_Y = 1.8f;
inline constexpr float fZM_PROFLAB_PLAYER_SCALE_Z = 0.8f;

// MIRRORED, not re-derived: this must equal
// ZM_PlayerController::CalculateCapsuleHalfExtent({ the three scales above }).
// It is spelled here rather than called because this header is PURE and that
// function lives on a component.
//
// THE MIRROR IS A BOOT UNIT, NOT A PROMISE: clause (0) of
// ZM_WorldTraversal/ProfLab_DoorSpawnStandsOnTheFloorWithCameraClearance in
// Tests/ZM_Tests_ProfLabPlacement.cpp calls the shipped formula with the three
// scales above and compares it against this constant. It has to: the authoring
// writes ZM_GetProfLabPlayerCenter (which reads THIS number) while the automated
// arrival clause compares against ZM_GameStateManager::CalculateSpawnCenter
// (which reads the REAL formula), so a controller re-tune would otherwise split
// the authored body from the arrival point with every other unit still green.
inline constexpr float fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT = 0.9f;

// The capsule's XZ half-width -- half the scale's X. A clearance stated against
// this cannot be satisfied by a body that would actually graze the wall.
inline constexpr float fZM_PROFLAB_PLAYER_RADIUS =
	fZM_PROFLAB_PLAYER_SCALE_X * 0.5f;

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
// authored rotation; and the player is a CAPSULE because it also has to move. So
// AABB is not a shortcut here, it is the correct shape, and "upgrading" these to
// OBB would rewrite the committed .zscen for zero behavioural gain. If a future
// prop in this room DOES need to face somewhere, that prop -- and only that prop
// -- gets OBB.
// ============================================================================
