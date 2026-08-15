#pragma once

#include <bit>     // bit_cast -- Aster's FROZEN facing (ZM-D-183)
#include <cmath>   // sqrt -- the arrival standoff, which nothing authored reads

#include "Maths/Zenith_Maths.h"   // Vector3 + Quat
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // the compiled world table -- READ by the exit resolver, NEVER mirrored
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
//
// ★ AND THAT RULE IS WHY THE EXIT ACCESSORS BELOW ARE RESOLVERS, NOT CONSTANTS.
// SC-E gives this room a way OUT, and an exit needs a target build index and a
// spawn tag -- the two things the paragraph above forbids spelling here. So this
// header spells NEITHER: ZM_GetProfLabExitTargetBuildIndex /
// ZM_GetProfLabExitSpawnTag WALK the compiled ZM_SCENE_PROFLAB row's connection
// list for the edge that targets Dawnmere and hand back what the TABLE says.
// Reading the table is the opposite of mirroring it -- there is still exactly one
// inventory, and a table edit that re-pointed or re-tagged that edge moves the
// authoring, the boot units and the live-scene clause together. The include of
// ZM_WorldSpec.h that this needs costs the header nothing: the world table is a
// compiled const array with no ECS, no scene and no I/O behind it, so the purity
// contract above is intact.
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

// The exit sensor: the ONE entity in this room that takes the player out of it.
// Named alongside the others rather than in the authoring block because three
// separate readers look it up -- the tools authoring, the live-scene clause I4 of
// ZM_ProfLabWarp_Test, and the round-trip walk -- and a name spelled three times
// is a name that can drift twice.
inline constexpr const char* szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME =
	"ProfLabExitTrigger";

// ============================================================================
// ★ THE DAWNMERE SIDE OF THE SAME DOOR, AND WHY ITS NAMES LIVE IN THE PROFLAB
// HEADER.
//
// The lab door is ONE contract with TWO scenes on it: Dawnmere carries the shell,
// the entrance frame, the warp sensor and the FromLab arrival marker; ProfLab
// carries the exit sensor that sends the player back to that marker. Both halves
// have to be authored in the same change (see the block on ZM_GetProfLabExitTrigger
// below for what happens if they are not), and both halves are looked up BY NAME
// by the same tests.
//
// So the six names below are spelled ONCE, here, and the placement of that
// spelling follows the include direction rather than taste:
// Source/World/ZM_DawnmerePlacement.h ALREADY includes this file (it derives the
// lab exterior's jamb X values and facade height from this room's aperture), so
// this is the deeper of the two headers and the only one both sides can read. The
// reverse -- putting them in the Dawnmere header -- would need ProfLab to include
// Dawnmere and would close that dependency into a cycle.
//
// ★ THE SHELL'S NAME IS LOAD-BEARING BEYOND AUTHORING. SC-D's ground-truth oracle
// (ZM_DawnmereLabGroundTruth_Test, Tests/ZM_AutoTests_CameraClearance.cpp) looks
// "DawnmereLabShell" up by name so its POST-SC-E re-measure can ignore the shell
// body standing over the columns it probes. Rename this and that oracle silently
// re-measures with the shell UNIGNORED -- ten rows of ground truth quietly taken
// off the roof of a building instead of off the terrain, with every test green.
//
// The four block names follow the shipped "Dawnmere<Building><Piece>" convention
// the Home blocks already use, so a reader who knows DawnmereHomeShell recognises
// these on sight.
// ============================================================================
inline constexpr const char* szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME =
	"DawnmereLabShell";
inline constexpr const char* szZM_DAWNMERE_LAB_DOOR_LEFT_ENTITY_NAME =
	"DawnmereLabDoorLeft";
inline constexpr const char* szZM_DAWNMERE_LAB_DOOR_RIGHT_ENTITY_NAME =
	"DawnmereLabDoorRight";
inline constexpr const char* szZM_DAWNMERE_LAB_DOOR_LINTEL_ENTITY_NAME =
	"DawnmereLabDoorLintel";

// The arrival marker the ProfLab exit sends the player to, and the sensor that
// sends them the other way. Named to match the shipped Home pair (FromHomeSpawn /
// HomeDoorTrigger) rather than to a new convention.
inline constexpr const char* szZM_DAWNMERE_FROM_LAB_SPAWN_ENTITY_NAME =
	"FromLabSpawn";
inline constexpr const char* szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME =
	"LabDoorTrigger";

// ---- The exit edge, RESOLVED from the compiled world table -------------------
//
// ★ WHY THESE ARE FUNCTIONS AND NOT `= 2` AND `= "FromLab"`. See the second star
// in the file header: the build index and the spawn tag are the world table's
// property, and a second spelling of either here would be an inventory nothing
// reconciles. These walk the ZM_SCENE_PROFLAB row for the edge that targets
// Dawnmere and return what it says, so a table edit moves every reader at once.
//
// ★ AND THE WALK IS BY TARGET, NOT BY INDEX 0. `m_pxConnections[0]` would be a
// magic index that silently returns the wrong edge the day ProfLab gains a second
// connection -- which is the kind of change a later stage makes without reading
// this file.

// The answer when the compiled table carries NO ProfLab -> Dawnmere edge at all.
// Deliberately a build index no scene can hold rather than a plausible one, so a
// caller that skips the resolution check authors an obviously dead warp instead of
// a subtly wrong one. The boot unit
// ZM_WorldTraversal/ProfLab_ExitSensorTargetsDawnmereByTheCompiledConnection
// asserts this value is never actually produced.
inline constexpr u_int uZM_PROFLAB_EXIT_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL, in this file's house style: a table with no such edge yields nullptr
// rather than UB, and the two accessors below turn that into their own stated
// sentinels. Nothing here asserts -- Zenith_Assert breaks in every configuration
// and the whole boot-unit suite runs before the scene loads.
inline const ZM_SceneConnection* ZM_GetProfLabExitConnection()
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_PROFLAB);
	if (xRow.m_pxConnections == nullptr)
	{
		return nullptr;
	}
	for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
	{
		if (xRow.m_pxConnections[uEdge].m_eTarget == ZM_SCENE_DAWNMERE)
		{
			return &xRow.m_pxConnections[uEdge];
		}
	}
	return nullptr;
}

inline u_int ZM_GetProfLabExitTargetBuildIndex()
{
	const ZM_SceneConnection* pxEdge = ZM_GetProfLabExitConnection();
	return pxEdge != nullptr
		? ZM_GetWorldSpec(pxEdge->m_eTarget).m_uBuildIndex
		: uZM_PROFLAB_EXIT_TARGET_UNRESOLVED;
}

// The tag the exit asks Dawnmere for -- and therefore ALSO the tag the Dawnmere
// arrival marker must carry. ONE spelling for both sides of the seam is the whole
// point: ZM_GameStateManager::IsWarpDestinationValid consults only this table and
// never the scene, so a marker tagged with anything else passes validation and
// then parks the warp machine in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN forever.
inline const char* ZM_GetProfLabExitSpawnTag()
{
	const ZM_SceneConnection* pxEdge = ZM_GetProfLabExitConnection();
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

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

// ---- The exit sensor --------------------------------------------------------
//
// ★★ THIS SENSOR AND THE DAWNMERE "FromLab" MARKER ARE ONE CHANGE, AND SHIPPING
// EITHER ALONE IS WORSE THAN SHIPPING NEITHER. ZM_GameStateManager::
// IsWarpDestinationValid consults ONLY the compiled ZM_WorldSpec tag list and
// never the destination scene, and "FromLab" has been a compiled Dawnmere tag
// since S1 -- so RequestWarp(<Dawnmere>, "FromLab") returns TRUE with no marker in
// the scene at all. The machine then advances to
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, WHICH HAS NO TIMEOUT, behind a fully
// opaque fade with the player frozen. Not a crash, not a red test: a black screen
// forever. That is why this sensor lands in the same commit as the Dawnmere
// blockout, the FromLabSpawn marker and the LabDoorTrigger, and why the live-scene
// clause I4 of ZM_ProfLabWarp_Test exists to red if one of them is ever removed.
//
// ---- The five numbers, each DERIVED and each with a reader -------------------
//
// (1) X SPAN = the aperture, exactly. The sensor has to be un-walk-aroundable: the
//     doorway is a 6 m gap between two panels, so a narrower sensor leaves a strip
//     of gap a player can slip through and walk out of the world through, and a
//     wider one would poke through the panels either side.
// (2) Y SPAN = the aperture height, seated on the floor's top face, so its centre
//     is half that height. A short sensor could be jumped and a tall one would
//     stick through the lintel.
// (3) FAR FACE = fZM_PROFLAB_INNER_MAX_Z, the inner plane of the doorway wall. So
//     nothing can reach the aperture without having crossed the sensor first --
//     the sensor is the LAST thing between the player and the gap, which is the
//     property that makes "the player cannot leave except through the warp" true
//     rather than likely.
// (4) NEAR FACE = the LAST QUARTER of the walk-in span, i.e. 7.0625. This is the
//     one number here that is a CLEARANCE rather than a size, and it is the number
//     Q-2026-08-15-001 moved. Both stars below are about it.
// (5) DEPTH = whatever (3) and (4) leave, which is that same quarter span: 0.6875 m.
//     A collision sensor is sampled at the physics tick, so it has to be several
//     ticks deep rather than a plane a fast capsule steps over between ticks -- but
//     the quantity that decides that is NOT the box. ZM_WarpTrigger fires from
//     OnCollisionEnter, i.e. on a BODY-vs-BODY contact, and the player is a
//     0.8 m-wide capsule: contact opens a body radius BEFORE the centre reaches the
//     near face and persists a body radius past the far one. The window a walker
//     actually spends in contact is therefore
//         depth + 2 * fZM_PROFLAB_PLAYER_RADIUS = 1.4875 m
//     which at the round trip's 30 Hz dt is 11.2 ticks at ZM_PlayerController's
//     4 m/s walk (0.1333 m/tick) and still 6.4 at its 7 m/s run (0.2333 m/tick).
//     Both figures are ASSERTED rather than claimed -- clauses (4) and (4b) of
//     ProfLab_ExitSensorFillsTheApertureAndClearsTheArrivingBody.
//
// ★★ THE NEAR FACE MUST CLEAR THE ARRIVING BODY, OR THE DOOR IS AN INFINITE LOOP.
// The player warps IN to fZM_PROFLAB_SPAWN_Z on the "Door" tag. If the arriving
// capsule -- feet marker plus fZM_PROFLAB_PLAYER_RADIUS of body -- overlapped this
// sensor, ZM_WarpTrigger would fire on the very first contact tick and send the
// player straight back to Dawnmere, whose LabDoorTrigger would send them back
// here, forever, with no input accepted between the two. The gap is
//     7.0625 - (5.0 + 0.4) = 1.6625 m
// and it is asserted, not asserted-in-a-comment: clause (5) of the boot unit
// ZM_WorldTraversal/ProfLab_ExitSensorFillsTheApertureAndClearsTheArrivingBody.
//
// ★★ ...AND IT MUST ALSO CLEAR THE WALK-UP TO THE PROFESSOR. THAT IS WHAT THIS
// SENSOR SHIPPED 1.5 m DEEP FOR AND DID NOT DO (Q-2026-08-15-001, fixed here).
//
// The clause above measures the arrival point, which the player never stands on for
// long: they immediately walk at Aster. ZM_PlayerController is driven by a MOVE
// action whose keyboard scheme is W/A/S/D, so the natural "up and to the left toward
// the professor" input is W+A HELD TOGETHER -- a 45-degree line, not the bearing.
// A diagonal spends depth as fast as it spends width, so it arrives at the picker's
// reach ring MUCH deeper into the room than a bearing-following walk does:
//
//     direct bearing to Aster  -- closes at z 5.388, leading edge 5.789
//     45-degree W+A walk       -- closes at z 5.934, leading edge 6.334
//
// against a near face that used to be at 6.25. So the player was warped back out to
// Dawnmere at the exact moment the professor came into reach, and every clause in
// this file stayed green: they all measured the ARRIVAL, and the defect was in the
// walk. (The gain is derived, not eyeballed: at 45 degrees the depth gained equals
// the lateral travel, so solving |Aster - (spawn + t * D)| = the effective reach for
// the unit diagonal D gives t = 1.3205 m of travel and 0.9338 m of depth.)
//
// ★ THE FIX RETREATED THE SENSOR, AND DELIBERATELY NOT THE PROFESSOR. Pulling Aster
// toward the camera would have worked arithmetically and re-opened the bug SC-C
// closed: the arrival frustum's half-width grows LINEARLY with depth, so a step
// toward the camera narrows the frame at his depth and walks him back toward the
// edge of the picture (see ProfLab_AsterStandsInsideTheArrivalFrustum). His anchor
// is untouched; the sensor gave up depth instead.
//
// ★ AND THE NEW NEAR FACE IS DERIVED, NOT PICKED. The walk-in span from the arrival
// marker to the doorway wall is 2.75 m. Aster stands at its MIDPOINT (see
// fZM_PROFLAB_ASTER_Z); the sensor owns only its LAST QUARTER. That single
// derivation buys all four properties at once, structurally rather than numerically:
//   * the near face lands one quarter span (0.6875 m) PAST the professor, which is
//     more than his own half footprint (0.4 m), so his body is clear of the sensor
//     in DEPTH and no longer depends on being clear of it in width;
//   * the eight-way drive stops pressing W the moment the player draws level with
//     him, so the deepest leading edge any walk-up-to-Aster can produce is
//     fZM_PROFLAB_ASTER_Z + fZM_PROFLAB_PLAYER_RADIUS = 6.775 -- still inside the
//     near face by that same 0.2875 m, whatever the reach is re-tuned to;
//   * the measured 45-degree leading edge (6.334) clears it by 0.7287 m, i.e. by
//     nearly two body radii; and
//   * the arrival clearance TRIPLES (0.85 -> 1.6625 m).
// Every term is dyadic (2.75 and a quarter of it are both exact binary fractions),
// so the authored centre is bit-identical in Debug and Release -- the ZM-D-183 rule,
// which this value owes because it lands in the COMMITTED ProfLab.zscen.
//
// The 45-degree figure itself is a CHECK, never an authored value: it needs a square
// root, and a libm result must not decide bytes in a committed scene. The boot unit
// ZM_WorldTraversal/ProfLab_ExitSensorClearsTheDiagonalWalkUpToTheProfessor runs it
// and compares it against the constants below -- with the retired 1.5 m depth fed
// through the identical predicate as its anti-vacuity arm.
inline constexpr float fZM_PROFLAB_EXIT_TRIGGER_SCALE_X =
	fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f;
inline constexpr float fZM_PROFLAB_EXIT_TRIGGER_SCALE_Y =
	fZM_PROFLAB_APERTURE_HEIGHT;

// The walk-in span: arrival marker to the inner face of the wall the player came
// through. Named here rather than at the arrival block because THIS is where it is
// read -- the header's own rule about unread constants. fZM_PROFLAB_ASTER_Z is the
// midpoint of this same span, spelled in its own terms; the two are deliberately
// left as separate derivations so that moving one does not silently move the other.
inline constexpr float fZM_PROFLAB_WALK_IN_SPAN =
	fZM_PROFLAB_INNER_MAX_Z - fZM_PROFLAB_SPAWN_Z;

// ...of which the sensor owns the last quarter, and nothing more.
inline constexpr float fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z =
	fZM_PROFLAB_WALK_IN_SPAN * 0.25f;

// The near face, as a named constant rather than a subtraction every reader repeats.
// Three of them do -- the arrival clearance below, the boot units, and the intro
// beat's walk-up guard -- and a plane spelled three times is a plane that can drift
// twice.
inline constexpr float fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z =
	fZM_PROFLAB_INNER_MAX_Z - fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z;

// The far face lands ON the doorway wall's inner plane, so the centre is half a
// depth back from it. Both terms are dyadic, so this sum is exact in every
// configuration -- the ZM-D-183 rule applied to a value that lands in a COMMITTED
// scene file.
inline constexpr float fZM_PROFLAB_EXIT_TRIGGER_Z =
	fZM_PROFLAB_INNER_MAX_Z - fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z * 0.5f;

// The clearance the first star above spends its length on, named so the boot unit
// and the round-trip walk can both state it rather than re-derive it.
inline float ZM_GetProfLabExitTriggerArrivalClearance()
{
	return fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z
		- (fZM_PROFLAB_SPAWN_Z + fZM_PROFLAB_PLAYER_RADIUS);
}

// The sensor as a blockout, in the same vocabulary as the shell pieces: a CENTRE
// and a SCALE, which is exactly what the authoring's transform steps take.
inline ZM_ProfLabBlockout ZM_GetProfLabExitTrigger()
{
	return {
		Zenith_Maths::Vector3(
			fZM_PROFLAB_SPAWN_X,
			fZM_PROFLAB_FLOOR_TOP_Y + fZM_PROFLAB_EXIT_TRIGGER_SCALE_Y * 0.5f,
			fZM_PROFLAB_EXIT_TRIGGER_Z),
		Zenith_Maths::Vector3(
			fZM_PROFLAB_EXIT_TRIGGER_SCALE_X,
			fZM_PROFLAB_EXIT_TRIGGER_SCALE_Y,
			fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z) };
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
// ★ HE USED TO FACE +Z, WHICH IS TO SAY HE GREETED THE PLAYER WITH HIS BACK
// TURNED, AND NOT ONE TEST COULD SEE IT. He was authored at identity rotation,
// identity forward is +Z (ZM_ForwardFromRotation rotates the +Z basis), the player
// arrives at fZM_PROFLAB_SPAWN_Z = 5.0 and the camera settles further back still
// at z = -0.5 -- while he stands DEEPER into the room at fZM_PROFLAB_ASTER_Z =
// 6.375. So his forward pointed at the doorway wall behind him and the whole arrival
// framed the back of the professor's head. ProfLab_AsterStandsInsideTheArrivalFrustum
// asserts he is ON SCREEN and passed the entire time; nothing asserted he was facing
// anything, because until this change nothing in this room faced anywhere at all.
// The defect was found by LOOKING AT A SCREENSHOT, which is the honest provenance
// and worth recording: an on-screen check is not a looking-right check.
//
// ★★ AND THE OBVIOUS FIX IS FORBIDDEN HERE (ZM-D-183). AddStep_SetTransformYaw and
// ...RotationEuler build their quaternion with libm AT AUTHORING TIME, MSVC Debug
// and Release codegen disagree on those sin/cos by 1-2 ULP, and ProfLab.zscen is
// COMMITTED and re-authored on every tools boot -- so a yaw step would make the
// file ping-pong in git forever, INVISIBLY to the same-binary pre-save guard (which
// compares the serialized bytes against a value that moved with them). That is the
// defect that has already cost this project two separate incidents. He is therefore
// authored EXACTLY the way rival Vesper is: four FROZEN std::bit_cast constants fed
// verbatim to AddStep_SetTransformRotationQuat, which performs no math at all.
//
// ★ AND HIS COLLIDER HAD TO CHANGE WITH HIM -- COLLISION_VOLUME_TYPE_AABB IS NOW
// ILLEGAL ON HIM, for the reason the closing note at the bottom of this file has
// always stated and the reason ZM-D-156 was paid for on rival Vesper. An AABB is
// axis-aligned BY DEFINITION: Zenith_ColliderComponent forces its Jolt body to
// JPH::Quat::sIdentity(), and the physics->transform sync then writes that identity
// straight back over the authored rotation and into the SAVED BYTES, with every
// pure unit still green because the units read the constants and the damage lives
// in the file. He is a body that must hold a facing and must never move, which is
// exactly the case the closing note reserves COLLISION_VOLUME_TYPE_OBB for -- the
// same box shape, differing only in that it applies the rotation. He does NOT take
// rival Vesper's dynamic capsule: that shape exists because the rival WALKS, and a
// dynamic body could be shoved off the anchor every clearance figure in this file
// is derived at.
inline constexpr u_int uZM_PROFLAB_ASTER_FACING_X_BITS = 0x00000000u;
inline constexpr u_int uZM_PROFLAB_ASTER_FACING_Y_BITS = 0x3F800000u;
inline constexpr u_int uZM_PROFLAB_ASTER_FACING_Z_BITS = 0x00000000u;
inline constexpr u_int uZM_PROFLAB_ASTER_FACING_W_BITS = 0x00000000u;

// ★ WHERE THESE FOUR NUMBERS COME FROM, AND WHY THIS ONE FREEZE NEEDS NO ORACLE
// RUN. Vesper's frozen bits are a MEASUREMENT -- the Debug build's atan2/angleAxis
// output for a bearing at his anchors -- so his header carries a re-derivation unit
// and a re-freeze procedure. These are not: a half turn about +Y is
//     (x, y, z, w) = (0, sin(pi/2), 0, cos(pi/2)) = (0, 1, 0, 0)
// whose components are 0 and 1 EXACTLY, in every float format there is. There is no
// libm result to freeze, nothing to re-measure and no configuration that can
// disagree; the bit patterns are simply the IEEE-754 spellings of 0.0f and 1.0f
// (0x00000000 and 0x3F800000). They are written as std::bit_cast anyway, to the
// same shape as ZM_DawnmereVesperFacing, so that "an authored rotation in a
// committed scene is a frozen bit pattern" has ONE form in this game and a reader
// never has to decide which kind they are looking at.
//
// Rotating the +Z basis by a half turn about +Y gives -Z, i.e. the professor turns
// to face back down the hall -- toward the arrival marker, past it to where the
// follow camera settles, and therefore toward the player's eye. That claim is
// checked rather than asserted here: the pure unit
// ZM_WorldTraversal/ProfLab_AsterFacingIsTurnedTowardTheArrival runs the SHIPPED
// ZM_ForwardFromRotation over this quaternion and dots the result against the
// direction to ZM_GetProfLabArrivalPivot() and ZM_GetProfLabSettledCameraPosition(),
// and clause I3 of ZM_ProfLabWarp_Test repeats it against the LIVE transform in the
// loaded scene. Both are DOT PRODUCTS on purpose: a guard that re-spells the same
// quaternion the authoring spells cannot see the authoring move, and the property
// worth holding is "he faces the player", not "these 16 bytes equal those 16 bytes".
inline Zenith_Maths::Quat ZM_ProfLabAsterFacing()
{
	static_assert(std::bit_cast<u_int>(
		std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_Y_BITS)) ==
		uZM_PROFLAB_ASTER_FACING_Y_BITS,
		"the frozen y component does not round-trip through float");
	static_assert(std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_Y_BITS) == 1.0f,
		"the frozen y component is not exactly 1.0f, so this is no longer a half "
		"turn about +Y and the -Z facing claim above is void");
	static_assert(std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_W_BITS) == 0.0f,
		"the frozen w component is not exactly 0.0f, so this is no longer a half turn");

	// glm::quat's constructor is (w, x, y, z).
	return Zenith_Maths::Quat(
		std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_W_BITS),
		std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_X_BITS),
		std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_Y_BITS),
		std::bit_cast<float>(uZM_PROFLAB_ASTER_FACING_Z_BITS));
}

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
//
//     ★ DO NOT PULL HIM TOWARD THE DOOR TO SOLVE A DOORWAY PROBLEM. This anchor is
//     the MIDPOINT of fZM_PROFLAB_WALK_IN_SPAN, and Q-2026-08-15-001 -- a 45-degree
//     walk-up that clipped the exit sensor -- was fixed by retreating the SENSOR to
//     the last quarter of that span, precisely so this value could stay put. A step
//     toward the camera narrows the frame at his depth (the paragraph above) and
//     walks him back toward the edge of the picture, which is the failure SC-C
//     existed to close.
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

// ★ HOW FAR HIS BODY STANDS CLEAR OF THE EXIT SENSOR IN **DEPTH**, and why that is
// a quantity worth naming (Q-2026-08-15-001). Before the sensor was retreated his
// centre (6.375) stood INSIDE its depth span (near face 6.25) and the only thing
// keeping him out of the doorway warp was that his X put him outside the aperture --
// a clearance held by ONE axis, in a room where the other axis is the one the player
// walks along. It now holds on both: the near face sits a quarter of the walk-in
// span past him, which is more than the half footprint his box reaches forward by.
//
// Lives HERE and not in the sensor block above because it needs fZM_PROFLAB_ASTER_Z,
// which is declared below that block; the read direction is what fixes the position,
// exactly as it does for the lab-seam names at the top of this file.
inline float ZM_GetProfLabAsterExitSensorDepthClearance()
{
	const float fAsterFrontFace =
		fZM_PROFLAB_ASTER_Z + fZM_HUMAN_BODY_FOOTPRINT * 0.5f;
	return fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z - fAsterFrontFace;
}

// ============================================================================
// ★ CLOSING NOTE -- WHY EVERY STATIC COLLIDER IN PROFLAB IS AABB EXCEPT ONE.
//
// An AABB collider forces its body to identity rotation, and the
// physics->transform sync writes that identity straight back into the SAVED
// SCENE BYTES with every unit still green -- so anything that must FACE a
// direction needs COLLISION_VOLUME_TYPE_OBB (the same box shape, differing only
// in that it applies the rotation).
//
// ALMOST NOTHING IN PROFLAB FACES ANYWHERE. The entrance is an ABSENCE of geometry
// between two axis-aligned panels, not a hinged panel; no shell block carries an
// authored rotation; the exit sensor is an axis-aligned box spanning an
// axis-aligned aperture; and the player is a CAPSULE because it also has to move.
// For all of those AABB is not a shortcut, it is the correct shape, and
// "upgrading" them to OBB would rewrite the committed .zscen for zero behavioural
// gain.
//
// ★ THE ONE EXCEPTION IS PROFESSOR ASTER, and this note used to say he was not
// one. He was authored at identity and the argument ran "he faces nowhere in
// particular, so he can wear AABB" -- which was true of the AUTHORING and false of
// the ROOM: he stands deeper into the hall than the arrival point, so an identity
// forward of +Z turned his back on every player who ever walked in. He now carries
// a frozen half-turn (ZM_ProfLabAsterFacing) and therefore takes OBB, exactly the
// case the paragraph above reserves it for: a body that must hold a rotation and
// must never move. He is NOT given the rival's dynamic capsule -- that shape is
// for an NPC who WALKS, and a dynamic body could be shoved off the anchor every
// clearance figure in this file is derived at.
//
// If a future prop in this room DOES need to face somewhere, that prop -- and only
// that prop -- gets OBB too.
// ============================================================================
