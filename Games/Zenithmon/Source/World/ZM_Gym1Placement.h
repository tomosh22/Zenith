#pragma once

#include <cmath>   // sqrt -- the arrival standoff, which nothing authored reads

#include "Maths/Zenith_Maths.h"   // Vector3
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // the compiled world table -- READ by the exit resolver, NEVER mirrored
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // fZM_INTERACT_MAX_DISTANCE (the reach Fenna must stay outside of)
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE human body contract

// ============================================================================
// ZM_Gym1Placement (S8, Gym 1 -- slice G1-1) -- the authored Thornacre Gym
// interior coordinates that BOTH the tools scene authoring (G1-2 / ZM-69) and the
// tests must agree on, in ONE place.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely. HEADER-ONLY: every accessor is `inline`, so there is no
// paired .cpp to keep in step. It follows Source/World/ZM_ProfLabPlacement.h and
// Source/World/ZM_PlayerHomePlacement.h (the two INTERIOR headers) rather than the
// outdoor ones: a gym interior has no terrain, no terrain recipe and no encounter
// table, and ZM_Tests_TerrainRecipeSet already asserts ZM_SCENE_GYM1 names none.
//
// ★ THIS SLICE AUTHORS NOTHING (ZM-D-209). G1-1 is the PURE half: this header,
// Fenna's roster row, the Bloom Badge index and the Verdant Lash TM. NO .zscen
// byte moves here. Gym1.zscen -- and the Thornacre door that reaches it -- are
// G1-2 (ZM-69), and the badge/teach-move award on a leader win is G1-3 (ZM-70).
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. A constant spelled
// twice cannot red a drift -- both sides move together and the assertion becomes
// decorative. If you need a number here, include this header and read it.
//
// ★ THE BUILD INDEX IS NOT HERE. Gym 1's build index (and its scene kind,
// connections and offered spawn tags) live in the compiled world table --
// Source/Data/ZM_WorldSpec.cpp, row ZM_SCENE_GYM1. Read it with
// ZM_GetWorldSpec(ZM_SCENE_GYM1).m_uBuildIndex. Duplicating it here would create a
// second inventory nothing reconciles.
//
// ★ AND THAT IS WHY THE EXIT ACCESSORS BELOW ARE RESOLVERS, NOT CONSTANTS. They
// WALK the compiled ZM_SCENE_GYM1 row's connection list for the edge that targets
// Thornacre and hand back what the TABLE says, exactly as ZM_ProfLabPlacement.h
// does for its Dawnmere edge.
//
// ---- WHAT THE DESIGN DOCUMENT SAYS, AND WHERE ------------------------------
// Nothing about this room is invented; every claim below is recorded in
// Docs/GameDesignDocument.md:
//   * Thornacre Town is a "Hedgerow farming town ringed by berry fields and
//     drystone walls", carrying "Gym 1 -- Fenna (Grass)" (GDD 94, GDD 134).
//   * Fenna is "Thornacre's youngest-ever leader; grows her own gym maze" (GDD
//     205) -- so the puzzle IS a hedge maze, not a decoration on one.
//   * Her badge is the Bloom Badge and her teach-move reward is Verdant Lash
//     (GDD 194); both ship in this slice as DATA (Source/Data/ZM_BadgeData.h,
//     Source/Data/ZM_ItemData.h).
//   * Her team is level 13, against Routes 1-2 at L2-8 (GDD 433 -- the pacing
//     table, which is the ONLY row carrying a level; GDD 194 is the badge table
//     and states leader/type/town/badge/TM and no level at all) -- the step up,
//     which is why the walk to her is a puzzle rather than a corridor.
// ============================================================================

// ---- The scene, and the entities that are not shell blocks ------------------
// The scene NAME handed to AddStep_CreateScene. The SAVE PATH is deliberately
// not here: GAME_ASSETS_DIR "Scenes/Gym1" ZENITH_SCENE_EXT is compile-time
// literal concatenation (no std::string, no runtime path build), so it cannot
// consume a const char* constant. The seven shipped registrations share that
// shape; this one matches them.
inline constexpr const char* szZM_GYM1_SCENE_NAME = "Gym1";

// The single arrival marker's TAG. Mirrored from the compiled world table --
// s_aszTagsGym1[] = { "Door" } in Source/Data/ZM_WorldSpec.cpp -- and the boot unit
// Gym1_HeaderSpawnTagMatchesTheWorldSpecRow asserts the mirror, so a table edit
// that renamed the tag cannot leave this authoring behind.
inline constexpr const char* szZM_GYM1_SPAWN_TAG = "Door";

inline constexpr const char* szZM_GYM1_SPAWN_ENTITY_NAME  = "Gym1DoorSpawn";
inline constexpr const char* szZM_GYM1_PLAYER_ENTITY_NAME = "Player";
inline constexpr const char* szZM_GYM1_CAMERA_ENTITY_NAME = "Gym1Camera";

// The room's ONE inhabitant: the leader, whose data layer shipped ahead of this
// placement -- ZM_HUMAN_LEADER_FENNA's appearance row (ZM_HumanData.cpp, outfit
// LEADER / hair DYED, palette themed on ZM_TYPE_GRASS in ZM_HumanAppearance.cpp)
// and, as of this slice, ZM_TRAINER_GYM1_FENNA's roster row. The NAME follows the
// authored "Npc_<Who>" convention that Npc_RivalVesper and Npc_ProfAster already
// use, so a reader recognises it on sight.
inline constexpr const char* szZM_GYM1_FENNA_ENTITY_NAME = "Npc_LeaderFenna";

// The exit sensor: the ONE entity in this room that takes the player out of it.
inline constexpr const char* szZM_GYM1_EXIT_TRIGGER_ENTITY_NAME = "Gym1ExitTrigger";

// ============================================================================
// ★ THE THORNACRE SIDE OF THIS DOOR IS DELIBERATELY NOT NAMED HERE, and that is
// the one place this header departs from ZM_ProfLabPlacement.h.
//
// ZM_ProfLabPlacement.h carries the DAWNMERE names (the lab shell, the jambs, the
// FromLabSpawn marker, the LabDoorTrigger) because both halves of that door landed
// in ONE commit. Gym 1's two halves are SPLIT by ZM-D-209: G1-2 (ZM-69) authors
// Gym1.zscen AND the Thornacre door together. Spelling Thornacre-side names in this
// slice would put an inventory here that nothing reads and nothing reconciles --
// the header's own rule about unread constants. G1-2 adds them, in whichever of the
// two headers the include direction allows.
//
// ★★ AND THE WARNING THAT COMES WITH THAT SPLIT IS BINDING ON G1-2.
// ZM_GameStateManager::IsWarpDestinationValid consults ONLY the compiled
// ZM_WorldSpec tag list and never the destination scene. "FromGym" has been a
// compiled Thornacre tag since S1 (s_aszTagsThornacre[]), so
// RequestWarp(<Thornacre>, "FromGym") returns TRUE with no marker in that scene at
// all: the machine advances to ZM_WARP_TRANSITION_WAITING_FOR_SPAWN and sits there
// behind a fully opaque fade with the player frozen until the barrier's frame
// budget expires (ZM-D-200), then escapes with a Zenith_Error naming the tag. Not a
// crash, not a red test -- a black screen that ends and says why. So the Gym 1 exit
// sensor, the Thornacre FromGymSpawn marker and the Thornacre gym-door trigger MUST
// land in the same commit. Tests/ZM_Tests_ThornacrePlacement.cpp currently asserts
// the Thornacre -> Gym1 edge is UNBACKED on purpose; that assertion is G1-2's to
// move, not this slice's.
// ============================================================================

// ---- The exit edge, RESOLVED from the compiled world table -------------------

// The answer when the compiled table carries NO Gym1 -> Thornacre edge at all.
// Deliberately a build index no scene can hold rather than a plausible one, so a
// caller that skips the resolution check authors an obviously dead warp instead of
// a subtly wrong one.
inline constexpr u_int uZM_GYM1_EXIT_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL, in this file's house style: a table with no such edge yields nullptr
// rather than UB, and the two accessors below turn that into their own stated
// sentinels. Nothing here asserts -- Zenith_Assert breaks in every configuration
// and the whole boot-unit suite runs before the scene loads.
//
// ★ THE WALK IS BY TARGET, NOT BY INDEX 0. `m_pxConnections[0]` would be a magic
// index that silently returns the wrong edge the day Gym 1 gains a second
// connection.
inline const ZM_SceneConnection* ZM_GetGym1ExitConnection()
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_GYM1);
	if (xRow.m_pxConnections == nullptr)
	{
		return nullptr;
	}
	for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
	{
		if (xRow.m_pxConnections[uEdge].m_eTarget == ZM_SCENE_THORNACRE)
		{
			return &xRow.m_pxConnections[uEdge];
		}
	}
	return nullptr;
}

inline u_int ZM_GetGym1ExitTargetBuildIndex()
{
	const ZM_SceneConnection* pxEdge = ZM_GetGym1ExitConnection();
	return pxEdge != nullptr
		? ZM_GetWorldSpec(pxEdge->m_eTarget).m_uBuildIndex
		: uZM_GYM1_EXIT_TARGET_UNRESOLVED;
}

// The tag the exit asks Thornacre for -- and therefore ALSO the tag the Thornacre
// arrival marker G1-2 authors must carry. ONE spelling for both sides of the seam.
inline const char* ZM_GetGym1ExitSpawnTag()
{
	const ZM_SceneConnection* pxEdge = ZM_GetGym1ExitConnection();
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

// ---- The room, as the numbers everything else is derived from ---------------
//
// Gym 1 is a 24 x 20 m hall -- deliberately NOT ProfLab's 20 x 16 m hall and NOT
// PlayerHome's 16 x 12 m room, so a registration pointed at the wrong .zscen
// disagrees on GEOMETRY as well as on entity names. It is also the largest
// interior in the game so far, which is what a maze needs: the serpentine below
// spends its difficulty on WIDTH, and a narrow room would make the three
// switchbacks read as a corridor with kinks in it.
//
// The floor's TOP FACE is exactly y = 0, so a spawn marker's FEET height is simply
// 0 and no consumer has to know the slab thickness to place a body.
//
// ★ EVERY DERIVED VALUE IN THIS FILE IS A DYADIC RATIONAL, computed by exact
// halves, quarters and sums. That is the ZM-D-183 rule applied ahead of time: G1-2
// authors a COMMITTED Gym1.zscen from these numbers, and a value whose bits depend
// on how MSVC associates a Debug vs a Release expression is how a tracked scene
// file ping-pongs in git forever. Keep every new constant here dyadic.
inline constexpr float fZM_GYM1_HALF_WIDTH      = 12.0f;   // +/- X inner extent
inline constexpr float fZM_GYM1_HALF_DEPTH      = 10.0f;   // +/- Z inner extent
inline constexpr float fZM_GYM1_WALL_THICKNESS  = 0.5f;
inline constexpr float fZM_GYM1_WALL_HEIGHT     = 4.0f;    // taller than ProfLab's 3.5 -- a hall, not a room
inline constexpr float fZM_GYM1_FLOOR_THICKNESS = 0.5f;

// The entrance is an ABSENCE of geometry, not a door panel: the +Z wall is
// authored as two side panels flanking a gap, with a lintel bridging the top.
// Nothing swings, nothing closes, and nothing carries a rotation -- see the
// closing note at the bottom of this file about AABB vs OBB.
inline constexpr float fZM_GYM1_APERTURE_HALF_WIDTH = 3.0f;
inline constexpr float fZM_GYM1_APERTURE_HEIGHT     = 3.0f;

// The floor's top face, and therefore every FEET height in this scene.
inline constexpr float fZM_GYM1_FLOOR_TOP_Y = 0.0f;

// The four inner faces of the shell -- the volume a body may actually occupy.
// Derived once here so no consumer re-derives "minus half the wall thickness"
// differently, and READ by ZM_Gym1ShellBlocksBody below, which the maze
// solvability unit runs on every cell it visits.
inline constexpr float fZM_GYM1_INNER_MIN_X =
	-fZM_GYM1_HALF_WIDTH + fZM_GYM1_WALL_THICKNESS * 0.5f;
inline constexpr float fZM_GYM1_INNER_MAX_X =
	fZM_GYM1_HALF_WIDTH - fZM_GYM1_WALL_THICKNESS * 0.5f;
inline constexpr float fZM_GYM1_INNER_MIN_Z =
	-fZM_GYM1_HALF_DEPTH + fZM_GYM1_WALL_THICKNESS * 0.5f;
inline constexpr float fZM_GYM1_INNER_MAX_Z =
	fZM_GYM1_HALF_DEPTH - fZM_GYM1_WALL_THICKNESS * 0.5f;

// ---- The player capsule -----------------------------------------------------
// The same human body every scene installs. NOT re-spelled here and NOT derived
// from a transform scale: both figures come straight from the ONE compiled body
// contract, so the authoring and the arrival (ZM_GameStateManager::
// CalculateSpawnPosition) cannot drift apart.
inline constexpr float fZM_GYM1_PLAYER_CAPSULE_HALF_EXTENT = fZM_HUMAN_BODY_HALF_HEIGHT;

// The capsule's XZ half-width. EVERY clearance in this file -- and every cell the
// maze solvability unit tests -- is stated against this, so a gap that satisfies
// them cannot be one the player's shoulders would graze.
inline constexpr float fZM_GYM1_PLAYER_RADIUS = fZM_HUMAN_BODY_CAPSULE_RADIUS;

// ---- The arrival marker -----------------------------------------------------
// A FEET anchor, standing just inside the +Z aperture on the shared X centreline.
// The authored Player body sits ON this marker -- and since the entity origin IS
// the feet, that is now an identity rather than a conversion.
//
// ★ THE SIGN OF fZM_GYM1_SPAWN_Z IS LOAD-BEARING. The follow camera trails toward
// -Z (see the camera block below), so the room body -- and therefore the maze --
// must lie on the -Z side of the arrival point. Moving the spawn to the far end of
// the hall parks the camera behind the back wall and puts the maze off screen.
//
// ★ AND ITS DISTANCE FROM THE DOORWAY WALL IS DERIVED, NOT PICKED: it leaves the
// same 2.75 m walk-in span ProfLab leaves, so the exit sensor below can use the
// identical last-quarter derivation and the two doorways behave the same way under
// the player's feet.
inline constexpr float fZM_GYM1_SPAWN_X      = 0.0f;
inline constexpr float fZM_GYM1_SPAWN_FEET_Y = fZM_GYM1_FLOOR_TOP_Y;
inline constexpr float fZM_GYM1_SPAWN_Z      = 7.0f;

// ---- The camera -------------------------------------------------------------
//
// ★ ZM-D-173, restated for an interior. ZM_FollowCamera keeps the yaw the SCENE
// authored (GetAuthoredYaw), camera forward at yaw 0 is +Z (ZM_ForwardFromRotation
// rotates the +Z basis), and the spring places the camera BEHIND its subject -- on
// the subject's -Z side. So the open space has to be at -Z of the arrival point and
// the entrance has to be the +Z face.
inline constexpr float fZM_GYM1_CAMERA_YAW   = 0.0f;
inline constexpr float fZM_GYM1_CAMERA_PITCH = 0.0f;

// MIRRORED from ZM_FollowCamera's private tuning (GetArmLength / GetCameraHeight /
// GetFOVDegrees / GetPivotHeight), exactly as ZM_ProfLabPlacement.h mirrors them
// and for the same purity reason: a pure boot unit cannot construct a camera
// component. Every one of them is asserted against the real getter by the boot
// unit Gym1_FollowCameraLooksOverTheFirstHedgeAtTheArrival.
inline constexpr float fZM_GYM1_CAMERA_ARM          = 5.5f;
inline constexpr float fZM_GYM1_CAMERA_HEIGHT       = 3.0f;
inline constexpr float fZM_GYM1_CAMERA_FOV_DEGREES  = 65.0f;
inline constexpr float fZM_GYM1_CAMERA_NEAR         = 0.1f;
inline constexpr float fZM_GYM1_CAMERA_FAR          = 100.0f;
inline constexpr float fZM_GYM1_CAMERA_PIVOT_HEIGHT = 0.60f;

// A zero arm would make the sight-line accessor at the bottom of this file divide
// by zero. The arm is a MIRROR, so this can only fire if the shipped camera changes
// under it -- which is precisely when a silent division by zero would be worst.
static_assert(fZM_GYM1_CAMERA_ARM > 0.0f,
	"the mirrored follow-camera arm is not positive, so the arrival sight line has "
	"no length and the sight-clearance accessors at the bottom of this file divide "
	"by zero");

// ---- The shell blockout -----------------------------------------------------

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line identical to
// ZM_ProfLabBlockout in Source/World/ZM_ProfLabPlacement.h and
// ZM_PlayerHomeBlockout in Source/World/ZM_PlayerHomePlacement.h (both of which
// already book the same debt against ZM_DawnmereBlockout). Consolidating all FOUR
// into a shared Source/World/ZM_Blockout.h would touch three authoring blocks and
// three test suites, which is out of G1-1's scope. Do the merge as its own change,
// not as a drive-by.
struct ZM_Gym1Blockout
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// The seven pieces of the shell, in AUTHORING ORDER. G1-2's authoring loop walks
// [0, ZM_GYM1_BLOCK_COUNT), so this order is part of the contract: appending a
// block is fine, REORDERING REWRITES THE SCENE BYTES (ZM-D-148 dense
// authoring-order file indices).
enum ZM_GYM1_BLOCK : u_int
{
	ZM_GYM1_BLOCK_FLOOR,
	ZM_GYM1_BLOCK_BACK_WALL,
	ZM_GYM1_BLOCK_LEFT_WALL,
	ZM_GYM1_BLOCK_RIGHT_WALL,
	ZM_GYM1_BLOCK_FRONT_LEFT,
	ZM_GYM1_BLOCK_FRONT_RIGHT,
	ZM_GYM1_BLOCK_LINTEL,

	ZM_GYM1_BLOCK_COUNT
};

// ============================================================================
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetProfLabBlock / ZM_GetTrainerData
// house style: no argument value, however degenerate, is UB, and none of them
// calls Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY
// configuration and the whole unit suite runs at boot, so an assert on an argument
// a unit deliberately feeds does not fail one test -- it ENDS the boot unit run and
// takes the whole gate down.
//
// The out-of-range answers are deliberately DEGENERATE rather than plausible: an
// all-zero blockout and a name no authored entity carries, so a caller that
// mistakenly indexes past the end fails loudly at its own assertion instead of
// silently matching a real block.
// ============================================================================

// The floor is the block a committed-bytes walk would look up by name first, so its
// name is pinned separately for a caller that wants it without spelling an
// enumerator. ZM_GetGym1BlockName RETURNS THIS CONSTANT rather than a second copy
// of the literal -- one spelling, so the two can never disagree.
inline constexpr const char* szZM_GYM1_FLOOR_ENTITY_NAME = "Gym1Floor";

inline const char* ZM_GetGym1BlockName(ZM_GYM1_BLOCK eBlock)
{
	switch (eBlock)
	{
	case ZM_GYM1_BLOCK_FLOOR:       return szZM_GYM1_FLOOR_ENTITY_NAME;
	case ZM_GYM1_BLOCK_BACK_WALL:   return "Gym1BackWall";
	case ZM_GYM1_BLOCK_LEFT_WALL:   return "Gym1LeftWall";
	case ZM_GYM1_BLOCK_RIGHT_WALL:  return "Gym1RightWall";
	case ZM_GYM1_BLOCK_FRONT_LEFT:  return "Gym1FrontLeft";
	case ZM_GYM1_BLOCK_FRONT_RIGHT: return "Gym1FrontRight";
	case ZM_GYM1_BLOCK_LINTEL:      return "Gym1Lintel";
	default: break;
	}
	return "Gym1InvalidBlock";
}

// Centre + scale for one shell piece. Every value is DERIVED from the room numbers
// above by the SAME formulas ZM_GetProfLabBlock and ZM_GetPlayerHomeBlock spell,
// verbatim:
//   floor       centre y = -floorThickness/2          (top face lands on y = 0)
//   walls       centre y =  wallHeight/2              (foot on the floor's top)
//   side walls  centre x = +/- halfWidth, depth = 2*halfDepth
//   end walls   centre z = +/- halfDepth, width = 2*halfWidth
//   front pair  width    =  halfWidth - apertureHalfWidth, centred on the
//                           remaining span either side of the opening
//   lintel      spans the aperture and fills wallHeight - apertureHeight above it
inline ZM_Gym1Blockout ZM_GetGym1Block(ZM_GYM1_BLOCK eBlock)
{
	constexpr float fFULL_WIDTH = fZM_GYM1_HALF_WIDTH * 2.0f;
	constexpr float fFULL_DEPTH = fZM_GYM1_HALF_DEPTH * 2.0f;
	constexpr float fWALL_CENTRE_Y = fZM_GYM1_WALL_HEIGHT * 0.5f;
	constexpr float fFRONT_PANEL_WIDTH =
		fZM_GYM1_HALF_WIDTH - fZM_GYM1_APERTURE_HALF_WIDTH;
	constexpr float fFRONT_PANEL_X =
		fZM_GYM1_APERTURE_HALF_WIDTH + fFRONT_PANEL_WIDTH * 0.5f;
	constexpr float fLINTEL_HEIGHT =
		fZM_GYM1_WALL_HEIGHT - fZM_GYM1_APERTURE_HEIGHT;
	constexpr float fLINTEL_CENTRE_Y =
		fZM_GYM1_APERTURE_HEIGHT + fLINTEL_HEIGHT * 0.5f;

	switch (eBlock)
	{
	case ZM_GYM1_BLOCK_FLOOR:
		return {
			Zenith_Maths::Vector3(
				0.0f, -fZM_GYM1_FLOOR_THICKNESS * 0.5f, 0.0f),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_GYM1_FLOOR_THICKNESS, fFULL_DEPTH) };

	case ZM_GYM1_BLOCK_BACK_WALL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fWALL_CENTRE_Y, -fZM_GYM1_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFULL_WIDTH, fZM_GYM1_WALL_HEIGHT, fZM_GYM1_WALL_THICKNESS) };

	case ZM_GYM1_BLOCK_LEFT_WALL:
		return {
			Zenith_Maths::Vector3(
				-fZM_GYM1_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_GYM1_WALL_THICKNESS, fZM_GYM1_WALL_HEIGHT, fFULL_DEPTH) };

	case ZM_GYM1_BLOCK_RIGHT_WALL:
		return {
			Zenith_Maths::Vector3(
				fZM_GYM1_HALF_WIDTH, fWALL_CENTRE_Y, 0.0f),
			Zenith_Maths::Vector3(
				fZM_GYM1_WALL_THICKNESS, fZM_GYM1_WALL_HEIGHT, fFULL_DEPTH) };

	case ZM_GYM1_BLOCK_FRONT_LEFT:
		return {
			Zenith_Maths::Vector3(
				-fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_GYM1_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_GYM1_WALL_HEIGHT,
				fZM_GYM1_WALL_THICKNESS) };

	case ZM_GYM1_BLOCK_FRONT_RIGHT:
		return {
			Zenith_Maths::Vector3(
				fFRONT_PANEL_X, fWALL_CENTRE_Y, fZM_GYM1_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fFRONT_PANEL_WIDTH, fZM_GYM1_WALL_HEIGHT,
				fZM_GYM1_WALL_THICKNESS) };

	case ZM_GYM1_BLOCK_LINTEL:
		return {
			Zenith_Maths::Vector3(
				0.0f, fLINTEL_CENTRE_Y, fZM_GYM1_HALF_DEPTH),
			Zenith_Maths::Vector3(
				fZM_GYM1_APERTURE_HALF_WIDTH * 2.0f, fLINTEL_HEIGHT,
				fZM_GYM1_WALL_THICKNESS) };

	default: break;
	}
	return { Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f) };
}

// ============================================================================
// ---- THE HEDGE MAZE ---------------------------------------------------------
//
// GDD 205: Fenna "grows her own gym maze". The brief for the room is PUZZLE-LITE,
// so this is three hedge bands with one opening each, arranged as a SERPENTINE --
// the classic gym-one shape. It is not a labyrinth, and deliberately so: the S8
// mini-playthrough has to walk this room on every run, and a maze whose solution
// takes a search rather than a glance turns a slice gate into a timeout.
//
//   band       z       opening side      what the player does
//   FIRST     +4.0        -X             arrive, turn LEFT, cross the room
//   SECOND     0.0        +X             turn RIGHT, cross the room
//   THIRD     -4.0        -X             turn LEFT, cross the room, meet Fenna
//
// Every band spans the full inner width MINUS one opening, and the opening sits
// against a side wall. That is what makes each band a genuine wall rather than a
// prop: there is exactly ONE way past each of them, so the route is forced.
//
// ★★ THE MAZE'S SOLVABILITY IS A PROPERTY OF THIS DATA, AND IT IS PROVEN, NOT
// CLAIMED. Tests/ZM_Tests_Gym1Placement.cpp lays a lattice over the inner floor,
// marks a cell walkable with ZM_Gym1PositionIsWalkable (the SHIPPED predicate
// below, at the SHIPPED body radius -- not a re-derivation), and breadth-first
// searches from the arrival cell to Fenna's cell. FOUR arms, because the first one
// alone would pass over an EMPTY ROOM:
//   (1) SOLVABLE          -- a path exists with every hedge present;
//   (2) NOT VACUOUS       -- the straight line from the arrival to Fenna is
//                            BLOCKED, by ZM_GYM1_HEDGE_FIRST specifically, and
//                            removing that one hedge unblocks that same point;
//   (3) THE MAZE COSTS    -- the shortest path through the maze is strictly longer
//                            than the shortest path with every hedge removed;
//   (4) THE ROUTE IS FORCED TO THE WALLS -- restricted to the middle HALF of the
//                            room the maze is UNSOLVABLE, while the same
//                            restriction over an empty room is crossable.
// Arm (2) is what "at least one wall genuinely blocks a shorter route" means
// mechanically.
//
// ★★ AND ARM (4) IS THE ONE THAT BOUNDS THE OPENING WIDTH FROM ABOVE. Arm (3) does
// NOT: a maze with 11.75 m openings -- half the room's inner width, three token
// stubs -- still yields a route longer than the straight run down the shared column,
// so it passes (1), (2) and (3) unchanged. Arm (4)'s restriction is a fraction of
// the SHELL, so widening a band's gap walks it into the restricted strip and reds.
// The width it flips at is COMPUTED in the unit, not promised in a comment.
//
// ★ A HEDGE IS 2.0 m TALL, AND THAT NUMBER IS DOING TWO JOBS AT ONCE.
//   * It is TALLER than a person (fZM_HUMAN_BODY_HEIGHT = 1.8 m), so the player
//     genuinely cannot see the route from the floor -- without that the maze is a
//     decoration and there is no puzzle.
//   * It is SHORTER than the settled follow camera, which flies at
//     fZM_GYM1_CAMERA_HEIGHT + the capsule half extent = 3.9 m and looks DOWN at
//     the arrival pivot. ZM_GetGym1CameraSightClearanceOverFirstHedge measures the
//     sight line where it crosses the first hedge's FAR face -- the face farthest
//     from the camera, which is where a line descending toward the pivot crosses
//     LOWEST -- and a boot unit asserts it is positive, because the follow camera's
//     arm is 5.5 m and the arrival stands 2.5 m in front of that hedge, so the
//     camera SETTLES OVER THE FIRST CORRIDOR OF THE MAZE. That is not a mistake to
//     fix by shortening the arm (the arm is a mirror of the shipped component, not
//     a free parameter); it is the reason the hedge may not be given the shell's
//     4.0 m wall height.
// ============================================================================

// Seated on the floor's top face, so the centre is half the height.
inline constexpr float fZM_GYM1_HEDGE_HEIGHT = 2.0f;

// How deep a band is in Z. Deep enough to read as a hedgerow rather than a fence,
// and -- the load-bearing half -- deep enough that the corridors between bands are
// still comfortably wider than a body: the bands sit 4.0 m apart, so a 1.0 m band
// leaves 3.0 m of clear corridor for a 0.8 m body.
inline constexpr float fZM_GYM1_HEDGE_THICKNESS = 1.0f;

// The clear opening each band leaves against one side wall. 3.75 m for a 0.8 m
// body: nearly five body widths, so the gap is a doorway the player walks through
// rather than a slot they have to line up on. The solvability unit re-derives the
// usable channel from the SHIPPED body radius rather than trusting this figure.
inline constexpr float fZM_GYM1_HEDGE_OPENING = 3.75f;

// The three bands, in AUTHORING ORDER -- the same contract ZM_GYM1_BLOCK carries:
// appending is fine, REORDERING REWRITES THE SCENE BYTES G1-2 will author.
enum ZM_GYM1_HEDGE : u_int
{
	ZM_GYM1_HEDGE_FIRST,    // z = +4, opening on -X: the first turn, and the one the anti-vacuity arm names
	ZM_GYM1_HEDGE_SECOND,   // z =  0, opening on +X
	ZM_GYM1_HEDGE_THIRD,    // z = -4, opening on -X: the last wall between the player and the leader

	ZM_GYM1_HEDGE_COUNT,
	ZM_GYM1_HEDGE_NONE = ZM_GYM1_HEDGE_COUNT   // "no hedge" sentinel -- also "skip nothing"
};

// TOTAL. True only for 0..ZM_GYM1_HEDGE_COUNT-1; ZM_GYM1_HEDGE_NONE aliases
// ZM_GYM1_HEDGE_COUNT, so the sentinel and every garbage value are rejected by one
// comparison -- the ZM_IsRegisteredTrainer shape.
inline bool ZM_IsGym1Hedge(ZM_GYM1_HEDGE eHedge)
{
	return (u_int)eHedge < (u_int)ZM_GYM1_HEDGE_COUNT;
}

inline const char* ZM_GetGym1HedgeName(ZM_GYM1_HEDGE eHedge)
{
	switch (eHedge)
	{
	case ZM_GYM1_HEDGE_FIRST:  return "Gym1HedgeFirst";
	case ZM_GYM1_HEDGE_SECOND: return "Gym1HedgeSecond";
	case ZM_GYM1_HEDGE_THIRD:  return "Gym1HedgeThird";
	default: break;
	}
	return "Gym1InvalidHedge";
}

// The Z centreline of one band. Spelled as a switch rather than an arithmetic
// progression on purpose: an author who moves ONE band should not silently move the
// other two, and a progression cannot express an irregular maze if a later stage
// wants one.
inline float ZM_GetGym1HedgeZ(ZM_GYM1_HEDGE eHedge)
{
	switch (eHedge)
	{
	case ZM_GYM1_HEDGE_FIRST:  return 4.0f;
	case ZM_GYM1_HEDGE_SECOND: return 0.0f;
	case ZM_GYM1_HEDGE_THIRD:  return -4.0f;
	default: break;
	}
	return 0.0f;
}

// Which side of the room the band's opening is on: -1 for the -X wall, +1 for +X.
// The ALTERNATION is the whole puzzle -- three bands that all opened on the same
// side would be a straight run down one edge of the room, which passes every
// solvability arm and is not a maze.
inline float ZM_GetGym1HedgeOpeningSide(ZM_GYM1_HEDGE eHedge)
{
	switch (eHedge)
	{
	case ZM_GYM1_HEDGE_FIRST:  return -1.0f;
	case ZM_GYM1_HEDGE_SECOND: return 1.0f;
	case ZM_GYM1_HEDGE_THIRD:  return -1.0f;
	default: break;
	}
	return 0.0f;
}

// One band, as a CENTRE and a SCALE -- exactly what the authoring's transform steps
// take, and the same vocabulary the shell pieces speak.
//
// The two derivations, spelled once here:
//   length = 2 * innerHalfWidth - opening   (the full inner span, less the doorway)
//   centre = opening / 2, signed AWAY FROM the opening's side
// With inner half width 11.75 and opening 3.75 that is a 19.75 m band centred at
// +/- 1.875 -- both exact binary fractions, per the dyadic rule at the top of this
// file.
//
// ★ THE SIGN IS THE NEGATION OF THE OPENING SIDE, AND GETTING IT BACKWARDS IS
// SILENT. A band whose opening is on -X occupies [innerMin + opening, innerMax] =
// [-8.0, +11.75], i.e. it is pushed TOWARD +X and centred at +1.875. Flip the sign
// and every band still spans the right LENGTH and still leaves an opening of the
// right WIDTH -- it is just on the wrong side, which turns the serpentine into a
// straight run down one edge of the room. That is why
// Gym1_HedgeBandsAreWallsWithExactlyOneOpeningEach checks WHICH inner wall each band
// reaches rather than only how long it is.
inline ZM_Gym1Blockout ZM_GetGym1Hedge(ZM_GYM1_HEDGE eHedge)
{
	if (!ZM_IsGym1Hedge(eHedge))
	{
		return { Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f) };
	}

	constexpr float fINNER_WIDTH = fZM_GYM1_INNER_MAX_X - fZM_GYM1_INNER_MIN_X;
	constexpr float fLENGTH      = fINNER_WIDTH - fZM_GYM1_HEDGE_OPENING;
	constexpr float fCENTRE_MAG  = fZM_GYM1_HEDGE_OPENING * 0.5f;

	return {
		Zenith_Maths::Vector3(
			-ZM_GetGym1HedgeOpeningSide(eHedge) * fCENTRE_MAG,
			fZM_GYM1_FLOOR_TOP_Y + fZM_GYM1_HEDGE_HEIGHT * 0.5f,
			ZM_GetGym1HedgeZ(eHedge)),
		Zenith_Maths::Vector3(
			fLENGTH, fZM_GYM1_HEDGE_HEIGHT, fZM_GYM1_HEDGE_THICKNESS) };
}

// ---- The walkability predicate the solvability proof runs -------------------
//
// ★ THESE THREE LIVE HERE, NOT IN THE TEST, FOR THE HEADER'S OWN REASON: G1-2 has
// to place a scene's worth of geometry against the same question, and a predicate
// written twice is a predicate that can disagree with itself. The test supplies the
// LATTICE and the SEARCH; the geometry answer comes from here.
//
// The overlap test is OPEN (strict inequalities), so a body that exactly grazes a
// face is NOT blocked. Every clearance in this room is designed with metres to
// spare, so no authored position sits on a boundary and the choice cannot decide a
// result -- it is stated only so a reader does not have to guess.

// TOTAL. False for the sentinel and for every garbage id, so "skip nothing" is
// simply passing ZM_GYM1_HEDGE_NONE.
inline bool ZM_Gym1HedgeBlocksBody(ZM_GYM1_HEDGE eHedge, float fX, float fZ, float fRadius)
{
	if (!ZM_IsGym1Hedge(eHedge))
	{
		return false;
	}
	const ZM_Gym1Blockout xHedge = ZM_GetGym1Hedge(eHedge);
	const Zenith_Maths::Vector3 xMin = xHedge.Min();
	const Zenith_Maths::Vector3 xMax = xHedge.Max();
	return fX + fRadius > xMin.x && fX - fRadius < xMax.x
		&& fZ + fRadius > xMin.z && fZ - fRadius < xMax.z;
}

// Every band EXCEPT eSkip. Passing ZM_GYM1_HEDGE_NONE skips nothing, which is the
// live maze; passing a real band is what the anti-vacuity arm uses to show that
// band is load-bearing.
inline bool ZM_Gym1MazeBlocksBody(float fX, float fZ, float fRadius, ZM_GYM1_HEDGE eSkip)
{
	for (u_int uHedge = 0u; uHedge < (u_int)ZM_GYM1_HEDGE_COUNT; ++uHedge)
	{
		const ZM_GYM1_HEDGE eHedge = (ZM_GYM1_HEDGE)uHedge;
		if (eHedge == eSkip)
		{
			continue;
		}
		if (ZM_Gym1HedgeBlocksBody(eHedge, fX, fZ, fRadius))
		{
			return true;
		}
	}
	return false;
}

// The shell, as the same question. A body is blocked when any part of it would be
// outside the four inner faces -- the floor's own extent is not consulted, because
// the walls stand ON the floor and are the tighter bound.
inline bool ZM_Gym1ShellBlocksBody(float fX, float fZ, float fRadius)
{
	return fX - fRadius < fZM_GYM1_INNER_MIN_X
		|| fX + fRadius > fZM_GYM1_INNER_MAX_X
		|| fZ - fRadius < fZM_GYM1_INNER_MIN_Z
		|| fZ + fRadius > fZM_GYM1_INNER_MAX_Z;
}

// "Could a body of fRadius stand here?" -- the ONE question the solvability search
// asks of every cell it visits.
inline bool ZM_Gym1PositionIsWalkable(float fX, float fZ, float fRadius, ZM_GYM1_HEDGE eSkip)
{
	return !ZM_Gym1ShellBlocksBody(fX, fZ, fRadius)
		&& !ZM_Gym1MazeBlocksBody(fX, fZ, fRadius, eSkip);
}

// ---- The derived placements the authoring writes ----------------------------

// The arrival marker's FEET position. Callers that need a body CENTRE add the
// capsule half-extent, exactly as ZM_GameStateManager::CalculateSpawnPosition does at
// warp time.
inline Zenith_Maths::Vector3 ZM_GetGym1SpawnFeet()
{
	return Zenith_Maths::Vector3(
		fZM_GYM1_SPAWN_X, fZM_GYM1_SPAWN_FEET_Y, fZM_GYM1_SPAWN_Z);
}

// The authored Player body's CENTRE: the marker's feet plus the capsule half
// extent, so the authored body and a warped-in body land on the same point.
inline Zenith_Maths::Vector3 ZM_GetGym1PlayerFeet()
{
	return Zenith_Maths::Vector3(
		fZM_GYM1_SPAWN_X,
		fZM_GYM1_SPAWN_FEET_Y,
		fZM_GYM1_SPAWN_Z);
}

// The authored camera position: the arm length straight back along -Z from the
// arrival point, at fZM_GYM1_CAMERA_HEIGHT above the FLOOR (y = 0).
//
// ★ THIS IS THE SETTLED POINT IN X AND Z ONLY -- NOT IN Y, and the difference is
// not an accident. ZM_FollowCamera::ComputeDesiredPosition adds fCAMERA_HEIGHT to
// the PLAYER'S CENTRE, not to the floor, so the spring settles one capsule half
// extent HIGHER than the value below (3.0 authored vs 3.9 settled). Do not "fix"
// that by writing the settled Y here. The gap is behaviourally invisible because
// ZM_FollowCamera::OnStart clears its spring state, so the first OnLateUpdate after
// the scene loads SNAPS to ComputeDesiredPosition; the authored Y is only ever the
// pose of a camera that has not ticked yet. ProfLab and PlayerHome author theirs
// the same way.
inline Zenith_Maths::Vector3 ZM_GetGym1CameraPosition()
{
	return Zenith_Maths::Vector3(
		fZM_GYM1_SPAWN_X,
		fZM_GYM1_CAMERA_HEIGHT,
		fZM_GYM1_SPAWN_Z - fZM_GYM1_CAMERA_ARM);
}

// The pose the arriving player is actually FILMED FROM, which is NOT the authored
// camera entity (see the note above).
inline Zenith_Maths::Vector3 ZM_GetGym1SettledCameraPosition()
{
	Zenith_Maths::Vector3 xSettled = ZM_GetGym1CameraPosition();
	xSettled.y += fZM_GYM1_PLAYER_CAPSULE_HALF_EXTENT;
	return xSettled;
}

// ...and the point it AIMS at: the arriving body's centre, lifted by the follow
// camera's pivot height.
inline Zenith_Maths::Vector3 ZM_GetGym1ArrivalPivot()
{
	return Zenith_Maths::Vector3(
		fZM_GYM1_SPAWN_X,
		fZM_GYM1_SPAWN_FEET_Y + fZM_GYM1_PLAYER_CAPSULE_HALF_EXTENT
			+ fZM_GYM1_CAMERA_PIVOT_HEIGHT,
		fZM_GYM1_SPAWN_Z);
}

// ★ THE NUMBER THE HEDGE HEIGHT IS CHOSEN AGAINST. The settled camera sits between
// the first two hedge bands (arrival 7.0 minus a 5.5 m arm is z = 1.5, and the
// first corridor runs z in [0.5, 3.5]), so the arrival shot looks back over
// ZM_GYM1_HEDGE_FIRST at the player. This measures the sight line -- settled camera
// to arrival pivot -- where it crosses that hedge's FAR face, minus a hedge top at
// fHedgeHeight. POSITIVE means the camera sees the player over the hedge.
//
// ★ THE FAR FACE IS Max().z = 4.5, AND IT IS THE CONSERVATIVE ONE, WHICH IS WHY IT
// IS THE ONE MEASURED. The camera is at z = 1.5, so 3.5 is the face CLOSEST to it
// and 4.5 the face FARTHEST -- the opposite of what this comment used to say. The
// arithmetic was always right: the sight line runs from a settled camera at y = 3.9
// DOWN to an arrival pivot at y = 1.5, so y falls as z rises and the line is at its
// LOWEST over the band at the far face (y = 2.591 there against y = 3.027 at the
// near one). Checking the near face instead would test the sight line where it has
// the most room, which is no test at all.
//
// TOTAL in fHedgeHeight: any float answers, nothing asserts, and the caller decides
// what "the hedge" is. That parameter exists so the anti-vacuity arm can run the
// SHIPPED function at the shell's own wall height rather than re-spelling the
// interpolation beside it -- a predicate written twice is a predicate that can
// disagree with itself.
inline float ZM_GetGym1CameraSightClearanceOverFirstHedgeAtHeight(float fHedgeHeight)
{
	const Zenith_Maths::Vector3 xCamera = ZM_GetGym1SettledCameraPosition();
	const Zenith_Maths::Vector3 xPivot  = ZM_GetGym1ArrivalPivot();
	const float fCrossZ = ZM_GetGym1Hedge(ZM_GYM1_HEDGE_FIRST).Max().z;

	// The denominator is the camera arm, which the static_assert above keeps
	// non-zero.
	const float fT = (fCrossZ - xCamera.z) / (xPivot.z - xCamera.z);
	const float fSightY = xCamera.y + fT * (xPivot.y - xCamera.y);
	return fSightY - (fZM_GYM1_FLOOR_TOP_Y + fHedgeHeight);
}

// The AUTHORED hedge's clearance -- the number the boot unit asserts a floor on.
//
// Arithmetic a boot unit RUNS, not a claim this file makes on its own behalf:
// Gym1_FollowCameraLooksOverTheFirstHedgeAtTheArrival calls this and asserts the
// floor, then calls the height-parameterised form above with the shell's own 4.0 m
// wall height as its anti-vacuity arm -- a hedge that tall would put the arriving
// frame inside a hedge, and the check has to be able to say so.
inline float ZM_GetGym1CameraSightClearanceOverFirstHedge()
{
	return ZM_GetGym1CameraSightClearanceOverFirstHedgeAtHeight(fZM_GYM1_HEDGE_HEIGHT);
}

// ---- The leader's station ---------------------------------------------------
//
// Fenna stands on the X centreline at the far end of the hall, BEHIND the last
// hedge band -- which is the entire point of the room: the only way to challenge
// her is to solve the maze. Her X is the centreline, so she is framed by the
// opening the player finally steps through AND by the arrival camera's own axis.
//
// ★ HER Z IS -7.0, NEAR THE MIDDLE OF THE CLEAR SPAN BEHIND THE LAST BAND RATHER
// THAN EXACTLY AT IT. That span runs from the band's far face (-4.5) to the inner
// face of the back wall (-9.75), whose midpoint is -7.125. Both figures are dyadic,
// so either is safe under the ZM-D-183 rule; -7.0 is chosen because it makes her
// standoff from the arrival exactly 14.0 m -- a whole number a reader can check in
// their head -- while still standing 2.5 m off the last band and 2.75 m off the
// wall. Measured from her BODY rather than her anchor those become 2.1 m and
// 2.35 m of clear floor (she is a 0.8 m footprint), and it is the BODY figures the
// two accessors below return and Gym1_LeaderStationStandsClearBehindTheLastHedge
// asserts -- an anchor clearance a body does not actually have is exactly the kind
// of number that looks like a check and is not one.
inline constexpr float fZM_GYM1_FENNA_X = 0.0f;
inline constexpr float fZM_GYM1_FENNA_Z = -7.0f;

// Her authored entity position: the FEET, the same vocabulary every human in this
// game is authored in (ZM_HumanBody.h), standing ON the floor's top face. That is
// now literal rather than arithmetic -- she stands where the floor is.
inline Zenith_Maths::Vector3 ZM_GetGym1FennaFeet()
{
	return Zenith_Maths::Vector3(
		fZM_GYM1_FENNA_X,
		fZM_GYM1_FLOOR_TOP_Y,
		fZM_GYM1_FENNA_Z);
}

// ★★ SHE IS AUTHORED AT IDENTITY ROTATION, AND THAT IS A DESIGN DECISION, NOT AN
// OMISSION. Identity forward is +Z (ZM_ForwardFromRotation rotates the +Z basis),
// the arrival stands at z = +7.0 and she stands at z = -7.0, so identity already
// points her straight down the hall at the player -- which is exactly the defect
// ZM_ProfLabPlacement.h had to pay a frozen half-turn to fix, avoided here by
// PLACING her on the camera's axis instead of correcting for having placed her off
// it. Gym1_LeaderFacesTheArrivalAtIdentity runs the SHIPPED ZM_ForwardFromRotation
// over the identity quaternion and dots the result against the direction to the
// arrival pivot, so this is checked rather than asserted in a comment.
//
// ★ WHICH ALSO MEANS SHE MAY WEAR COLLISION_VOLUME_TYPE_AABB. Identity is exact in
// every configuration -- it is the one rotation that survives the ZM-D-183 hazard
// with no bit_cast freeze at all, which is why the four shipped Dawnmere townsfolk
// never surfaced it. THE MOMENT ANYONE TURNS HER, both of those change together:
// she needs four frozen std::bit_cast constants fed to
// AddStep_SetTransformRotationQuat, and COLLISION_VOLUME_TYPE_OBB, because an AABB
// collider forces its Jolt body to identity and the physics->transform sync writes
// that identity straight back into the SAVED SCENE BYTES with every pure unit still
// green. See the closing note at the bottom of this file.

// The per-NPC reach BONUS a stationary talker is authored with: that NPC's OWN AABB
// half-width, so the global reach is not silently spent crossing her own body.
// Spelled from the body contract rather than as a literal 0.4.
inline constexpr float fZM_GYM1_FENNA_REACH_BONUS = fZM_HUMAN_BODY_FOOTPRINT * 0.5f;

// ...and the total reach her arrival standoff has to BEAT. ZM_PickInteractTarget
// accepts a candidate at XZ distance <= m_fMaxDistance + probe radius, INCLUSIVE at
// the boundary, so "outside reach" means strictly greater than this.
inline constexpr float fZM_GYM1_FENNA_EFFECTIVE_REACH =
	fZM_INTERACT_MAX_DISTANCE + fZM_GYM1_FENNA_REACH_BONUS;

// How far the arriving player stands from her, in the XZ plane the picker measures
// in (Y is ignored there). This room's answer is a whole 14.0 m, but it is COMPUTED
// rather than spelled: the interesting claim is not the number, it is that the
// maze's own geometry keeps her out of reach on arrival, and a spelled 14.0 would
// stop moving the day either anchor did.
inline float ZM_GetGym1FennaArrivalStandoff()
{
	const float fDeltaX = fZM_GYM1_FENNA_X - fZM_GYM1_SPAWN_X;
	const float fDeltaZ = fZM_GYM1_FENNA_Z - fZM_GYM1_SPAWN_Z;
	return std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
}

// The clear floor between her body's rear face and the inner face of the back wall,
// and between her body's front face and the last hedge band. Named so the boot unit
// can state them rather than re-derive them.
inline float ZM_GetGym1FennaBackWallClearance()
{
	const float fRearFace = fZM_GYM1_FENNA_Z - fZM_HUMAN_BODY_FOOTPRINT * 0.5f;
	return fRearFace - fZM_GYM1_INNER_MIN_Z;
}

inline float ZM_GetGym1FennaLastHedgeClearance()
{
	const float fFrontFace = fZM_GYM1_FENNA_Z + fZM_HUMAN_BODY_FOOTPRINT * 0.5f;
	return ZM_GetGym1Hedge(ZM_GYM1_HEDGE_THIRD).Min().z - fFrontFace;
}

// ---- The exit sensor --------------------------------------------------------
//
// The same five-number derivation ZM_ProfLabPlacement.h spells, applied to this
// room's aperture -- and the walk-in span works out to the identical 2.75 m, which
// is why the arrival clearance below matches ProfLab's to the millimetre.
//
// (1) X SPAN = the aperture, exactly. The sensor has to be un-walk-aroundable.
// (2) Y SPAN = the aperture height, seated on the floor's top face.
// (3) FAR FACE = fZM_GYM1_INNER_MAX_Z, the inner plane of the doorway wall, so
//     nothing can reach the aperture without having crossed the sensor first.
// (4) NEAR FACE = the LAST QUARTER of the walk-in span.
// (5) DEPTH = whatever (3) and (4) leave, which is that same quarter span.
//
// ★★ THE NEAR FACE MUST CLEAR THE ARRIVING BODY, OR THE DOOR IS AN INFINITE LOOP.
// The player warps IN to fZM_GYM1_SPAWN_Z on the "Door" tag. If the arriving capsule
// -- feet marker plus fZM_GYM1_PLAYER_RADIUS of body -- overlapped this sensor,
// ZM_WarpTrigger would fire on the very first contact tick and send the player
// straight back to Thornacre, whose gym-door trigger would send them back here,
// forever, with no input accepted between the two.
//
// ★ AND UNLIKE PROFLAB, THE WALK-UP CANNOT REACH IT. Q-2026-08-15-001 was a
// 45-degree walk toward an NPC standing 1.375 m into a 2.75 m span; here the only
// thing worth walking to is Fenna, 14 m away and BEHIND three hedges, so every
// walk-up path leads AWAY from this sensor. That is a property of the maze, not
// luck: ZM_GYM1_HEDGE_FIRST spans the whole inner width except the -X opening, so
// there is no reachable floor between the arrival and the doorway wall at all
// except the arrival's own pocket.
inline constexpr float fZM_GYM1_EXIT_TRIGGER_SCALE_X =
	fZM_GYM1_APERTURE_HALF_WIDTH * 2.0f;
inline constexpr float fZM_GYM1_EXIT_TRIGGER_SCALE_Y = fZM_GYM1_APERTURE_HEIGHT;

// The walk-in span: arrival marker to the inner face of the wall the player came
// through.
inline constexpr float fZM_GYM1_WALK_IN_SPAN =
	fZM_GYM1_INNER_MAX_Z - fZM_GYM1_SPAWN_Z;

// ...of which the sensor owns the last quarter, and nothing more.
inline constexpr float fZM_GYM1_EXIT_TRIGGER_SCALE_Z = fZM_GYM1_WALK_IN_SPAN * 0.25f;

// The near face, as a named constant rather than a subtraction every reader repeats.
inline constexpr float fZM_GYM1_EXIT_TRIGGER_NEAR_Z =
	fZM_GYM1_INNER_MAX_Z - fZM_GYM1_EXIT_TRIGGER_SCALE_Z;

// The far face lands ON the doorway wall's inner plane, so the centre is half a
// depth back from it. Both terms are dyadic, so this sum is exact in every
// configuration.
inline constexpr float fZM_GYM1_EXIT_TRIGGER_Z =
	fZM_GYM1_INNER_MAX_Z - fZM_GYM1_EXIT_TRIGGER_SCALE_Z * 0.5f;

// The clearance the first star above spends its length on, named so the boot unit
// can state it rather than re-derive it.
inline float ZM_GetGym1ExitTriggerArrivalClearance()
{
	return fZM_GYM1_EXIT_TRIGGER_NEAR_Z
		- (fZM_GYM1_SPAWN_Z + fZM_GYM1_PLAYER_RADIUS);
}

// The sensor as a blockout, in the same vocabulary as the shell pieces: a CENTRE
// and a SCALE, which is exactly what the authoring's transform steps take.
inline ZM_Gym1Blockout ZM_GetGym1ExitTrigger()
{
	return {
		Zenith_Maths::Vector3(
			fZM_GYM1_SPAWN_X,
			fZM_GYM1_FLOOR_TOP_Y + fZM_GYM1_EXIT_TRIGGER_SCALE_Y * 0.5f,
			fZM_GYM1_EXIT_TRIGGER_Z),
		Zenith_Maths::Vector3(
			fZM_GYM1_EXIT_TRIGGER_SCALE_X,
			fZM_GYM1_EXIT_TRIGGER_SCALE_Y,
			fZM_GYM1_EXIT_TRIGGER_SCALE_Z) };
}

// ============================================================================
// ★ CLOSING NOTE -- WHY EVERY STATIC COLLIDER IN GYM 1 IS AABB, WITH NO EXCEPTION.
//
// An AABB collider forces its body to identity rotation, and the
// physics->transform sync writes that identity straight back into the SAVED SCENE
// BYTES with every unit still green -- so anything that must FACE a direction needs
// COLLISION_VOLUME_TYPE_OBB (the same box shape, differing only in that it applies
// the rotation). ZM_ProfLabPlacement.h had to pay that for Professor Aster.
//
// NOTHING IN GYM 1 FACES ANYWHERE OFF AXIS. The entrance is an ABSENCE of geometry
// between two axis-aligned panels; no shell block and no hedge band carries an
// authored rotation (the maze is a serpentine of axis-aligned walls, which is
// exactly what makes ZM_Gym1HedgeBlocksBody a two-axis interval test rather than a
// separating-axis one); the exit sensor is an axis-aligned box spanning an
// axis-aligned aperture; the player is a CAPSULE because it also has to move; and
// the leader stands at IDENTITY on the room's own centreline, facing +Z at the
// arrival. For all of those AABB is not a shortcut, it is the correct shape.
//
// ★ THE TRIPWIRE, STATED ONCE: the day anyone turns Fenna, angles a hedge, or gives
// this room a prop that has to face somewhere, THAT entity -- and only that entity
// -- takes OBB and a frozen std::bit_cast quaternion authored via
// AddStep_SetTransformRotationQuat. AddStep_SetTransformYaw and
// ...RotationEuler build their quaternion with libm AT AUTHORING TIME, MSVC Debug
// and Release disagree on those sin/cos by 1-2 ULP, and Gym1.zscen will be COMMITTED
// and re-authored on every tools boot -- so a yaw step makes the file ping-pong in
// git forever, INVISIBLY to any same-binary pre-save guard. That defect has already
// cost this project two separate incidents (ZM-D-179, ZM-D-183).
// ============================================================================
