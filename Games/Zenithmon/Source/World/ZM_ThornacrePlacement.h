#pragma once

#include "Maths/Zenith_Maths.h"                      // Vector3

#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID / ZM_SceneConnection
#include "Zenithmon/Source/World/ZM_HumanBody.h"     // fZM_HUMAN_BODY_HALF_HEIGHT

#include <cmath>                                     // cos/sin -- the settled camera only

// ============================================================================
// ZM_ThornacrePlacement (S8 item 2, R1-1) -- the compiled anchors Thornacre is
// authored from, and the ONE place its entity names are spelled.
//
// ★★ THORNACRE IS A TRAVERSAL STUB IN THIS MILESTONE, BY RULING (ZM-D-196). It
// gets terrain, ONE arrival marker, a player, a camera and a return trigger --
// and NO GYM DOOR. The compiled world table already carries the
// Thornacre -> Gym1 ("Door") edge; that edge is deliberately UNBACKED here.
// Nothing in this file may name a gym entity, and a later slice lands a byte
// needle asserting the committed Thornacre.zscen contains no gym door at all.
// DO NOT "FINISH" THE TOWN IN THIS HEADER. The five-name inventory below and
// uZM_THORNACRE_PLACEMENT_ENTITY_COUNT are the tripwire: a sixth name has to
// move a literal, which reds a boot unit and forces a conscious decision
// instead of quietly authoring a door into a room nobody built.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless Null_* build where Project_RegisterEditorAutomationSteps is
// compiled out entirely. HEADER-ONLY: every accessor is `inline`, so there is no
// paired .cpp to keep in step (Dawnmere's .cpp exists only because it owns
// measured tables; nothing here is measured yet).
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. A constant spelled
// twice cannot red a drift -- both sides move together and the assertion becomes
// decorative. If you need a number here, include this header and read it.
//
// ★ THE BUILD INDEX IS NOT HERE, AND NEITHER IS THE SPAWN TAG. Thornacre's build
// index, scene kind, connections and offered tags live in the compiled world
// table -- Source/Data/ZM_WorldSpec.cpp, row ZM_SCENE_THORNACRE. Read the index
// with ZM_GetWorldSpec(ZM_SCENE_THORNACRE).m_uBuildIndex. Duplicating either
// here would create a second inventory nothing reconciles, which is why the
// return-edge accessors below are RESOLVERS that WALK the table rather than
// constants that mirror it -- the same shape ZM_GetProfLabExitTargetBuildIndex /
// ZM_GetProfLabExitSpawnTag already ship in the sibling header.
//
// ★ THIS HEADER DOES NOT INCLUDE ZM_TerrainAuthoring.h. The Thornacre recipe
// drags <filesystem>, <string> and Zenith_Vector into every consumer, and this
// file is included by leaf test TUs. The anchors below are stated here and
// RECONCILED against the recipe's landmarks and pads by the boot units, which
// may include it. It does not include ZM_Route1Placement.h either: Route 1 and
// Thornacre share exactly one seam -- the compiled world table -- and both read
// it independently.
//
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetProfLabBlock house style: no
// argument value, however degenerate, is UB, and NONE OF THEM CALLS
// Zenith_Assert. Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration,
// and the whole unit suite runs at boot before any scene loads, so an assert
// here would not fail one test -- it would end the run. Note in particular that
// ZM_GetWorldSpec ASSERTS FATALLY on an out-of-range id, so every resolver below
// guards BEFORE calling it.
// ============================================================================

// ---- The scene, and the five entities the stub authors ----------------------
//
// The name AddStep_CreateScene will take. The SAVE PATH is deliberately not
// here: GAME_ASSETS_DIR "Scenes/Thornacre" ZENITH_SCENE_EXT is compile-time
// literal concatenation, which cannot consume a const char* constant. The file
// STEM this name matches lives in Source/World/ZM_SceneRegistry.h, which is the
// one enumerable inventory of "which scene has a .zscen, under what stem".
//
// ★ NOT AN ENTITY NAME. "Thornacre" is a strict substring of every entity name
// below, so it must never join a name battery or a byte needle -- see the
// comment on ZM_GetThornacrePlacementEntityName.
inline constexpr const char* szZM_THORNACRE_SCENE_NAME = "Thornacre";

// The Terrain + Collider + ZM_TerrainGrass host entity.
//
// ★ DO NOT BUILD A "== 1" BYTE NEEDLE ON THIS NAME. Zenith_TerrainComponent's
// load path names its rebuilt materials from the host entity's name, so a scene
// that is ever LOADED-then-RE-SAVED can carry this string more than once. Needle
// the player, arrival or gate names instead.
inline constexpr const char* szZM_THORNACRE_TERRAIN_ENTITY_NAME = "ThornacreTerrain";

// ★★ THE PLAYER ENTITY IS NAMED "Player", AND IT MUST STAY THAT WAY.
// ZM_FollowCamera::AcquireTarget resolves its subject with
// FindEntityByName("Player") (Components/ZM_FollowCamera.cpp:390) -- production,
// scene-agnostic code, and the only FindEntityByName call in the whole game
// layer. On a miss it clears its target and returns an invalid entity, and
// ZM_GameStateManager::PollForCameraAndBeginFadeIn then bare-returns while the
// camera has no target -- a barrier with NO TIMEOUT, i.e. a permanent black
// screen behind an opaque fade with the player frozen. Not a crash, not a red
// test. The shipped interiors spell it the same way
// (szZM_PROFLAB_PLAYER_ENTITY_NAME = "Player"), and a scene-unique rename here
// would be exactly the defect this slice exists to make impossible.
//
// The consequence for byte needles, stated so nobody re-derives it: "Player" IS
// a substring of the component type name "ZM_PlayerController", so a later
// needle on it uses the STRICTLY-MORE clause ZM_Tests_CommittedSceneBytes.cpp
// already ships, never a "== 1" equality -- and it is excluded from the
// type-name battery entirely.
inline constexpr const char* szZM_THORNACRE_PLAYER_ENTITY_NAME = "Player";

// The follow camera. Deliberately NOT "ThornacrePlayerCamera", which would make
// another authored name a strict prefix of it (the FromLab / FromLabSpawn trap).
inline constexpr const char* szZM_THORNACRE_CAMERA_ENTITY_NAME = "ThornacreCamera";

// The ZM_SpawnPoint the Route1 -> Thornacre warp lands on.
//
// ★ THE NAME DELIBERATELY DOES NOT CONTAIN ITS TAG. The tag is the world
// table's, resolved by ZM_GetThornacreReturnSpawnTag's opposite number on the
// Route 1 side and never spelled in this file -- and a marker whose NAME
// embedded that tag would make every future tag byte-needle count the entity
// name too. Named this way, a later slice can assert tagHits == 1 AND
// nameHits == 1 as plain equalities instead of reproducing the strictly-more
// clause.
inline constexpr const char* szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME =
	"ThornacreSouthArrival";

// The return ZM_WarpTrigger: the ONE entity that takes the player back out of
// this town, and the whole reason the stub is traversable rather than a dead end.
inline constexpr const char* szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME =
	"ThornacreSouthGate";

// ★ THE RULING-2 TRIPWIRE. Five entities, and this literal is what a sixth has
// to move. It is spelled as a number rather than derived from an enum on
// purpose: adding a gym door means a human edits this line, and the boot unit
// Thornacre_StubAuthorsNoGymDoorAndItsNamesAreUniqueLookupKeys spells 5u on its
// own side, so the edit reds instead of shipping.
inline constexpr u_int uZM_THORNACRE_PLACEMENT_ENTITY_COUNT = 5u;

// The five ENTITY names, walkable by index so a name battery can enumerate them
// instead of re-spelling them. The ORDER is contract (the units assert this
// accessor returns the i-th declared constant); appending is a ruling-level act,
// reordering is merely a diff nobody can read.
//
// ★ THE SCENE NAME IS NOT IN THIS LIST, DELIBERATELY. "Thornacre" is a strict
// substring of three of the five, so including it would red the pairwise
// non-substring clause on the first pair it reached -- and the scene name is not
// an entity anyway.
//
// TOTAL: "" (never nullptr) out of range, so a caller that walks past the end
// fails its own lookup instead of silently matching a real entity.
inline const char* ZM_GetThornacrePlacementEntityName(u_int uIndex)
{
	switch (uIndex)
	{
	case 0u: return szZM_THORNACRE_TERRAIN_ENTITY_NAME;
	case 1u: return szZM_THORNACRE_PLAYER_ENTITY_NAME;
	case 2u: return szZM_THORNACRE_CAMERA_ENTITY_NAME;
	case 3u: return szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME;
	case 4u: return szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME;
	default: break;
	}
	return "";
}

// ---- The return edge, RESOLVED from the compiled world table -----------------
//
// ★ WHY THESE ARE FUNCTIONS AND NOT A BUILD-INDEX LITERAL AND A TAG LITERAL.
// See the fourth star in the file banner: the build index and the spawn tag are
// the world table's property, and a second spelling of either here would be an
// inventory nothing reconciles. These walk the ZM_SCENE_THORNACRE row for the
// edge that targets Route 1 and return what the TABLE says, so a table edit that
// re-pointed or re-tagged the edge moves the authoring, the boot units and the
// live-scene clause together.
//
// ★ AND THE WALK IS BY TARGET, NOT BY INDEX 0. Thornacre already carries TWO
// edges -- the Route 1 return and the gym door this stub deliberately does not
// back -- so m_pxConnections[0] would be a magic index that silently hands back
// the gym the day the table is reordered.
//
// ★ THE GYM EDGE HAS NO ACCESSOR HERE, AND THAT IS THE RULING. A
// ZM_GetThornacreGymConnection() in this header would be a gym-named surface in
// a file whose whole point is that the gym does not exist yet. The unit that
// documents the deliberate asymmetry walks the row for it locally, in
// Tests/ZM_Tests_ThornacrePlacement.cpp.

// The answer when the compiled table carries NO Thornacre -> Route1 edge at all.
// Deliberately a build index no scene can hold rather than a plausible one, so a
// caller that skips the resolution check authors an obviously dead warp instead
// of a subtly wrong one.
inline constexpr u_int uZM_THORNACRE_RETURN_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL: a table with no such edge (or no connection array at all) yields
// nullptr rather than UB, and the three accessors below turn that into their own
// stated sentinels.
inline const ZM_SceneConnection* ZM_GetThornacreReturnConnection()
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
	if (xRow.m_pxConnections == nullptr)
	{
		return nullptr;
	}
	for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
	{
		if (xRow.m_pxConnections[uEdge].m_eTarget == ZM_SCENE_ROUTE1)
		{
			return &xRow.m_pxConnections[uEdge];
		}
	}
	return nullptr;
}

inline ZM_SCENE_ID ZM_GetThornacreReturnTargetScene()
{
	const ZM_SceneConnection* pxEdge = ZM_GetThornacreReturnConnection();
	return pxEdge != nullptr ? pxEdge->m_eTarget : ZM_SCENE_NONE;
}

// ★ THE RANGE GUARD IS NOT DEFENSIVE DECORATION. ZM_GetWorldSpec ASSERTS FATALLY
// on an out-of-range id and Zenith_Assert breaks in every configuration, so a
// table that ever handed back ZM_SCENE_NONE here would end the whole boot-unit
// run rather than report one failure.
inline u_int ZM_GetThornacreReturnTargetBuildIndex()
{
	const ZM_SCENE_ID eTarget = ZM_GetThornacreReturnTargetScene();
	return eTarget < ZM_SCENE_COUNT
		? ZM_GetWorldSpec(eTarget).m_uBuildIndex
		: uZM_THORNACRE_RETURN_TARGET_UNRESOLVED;
}

// The tag the return trigger asks Route 1 for -- and therefore ALSO the tag the
// Route 1 arrival marker must carry. ONE spelling for both sides of the seam is
// the whole point: ZM_GameStateManager::IsWarpDestinationValid consults only this
// table and never the destination scene, so a marker tagged with anything else
// passes validation and then parks the warp machine in
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN forever.
//
// TOTAL: "" on a miss, NEVER nullptr.
inline const char* ZM_GetThornacreReturnSpawnTag()
{
	const ZM_SceneConnection* pxEdge = ZM_GetThornacreReturnConnection();
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

// ---- The ground the stub stands on ------------------------------------------
//
// ★ PROVISIONAL -- re-measure in R1-3 (see ZM_DawnmereNpcGroundTruth_Test).
// This is the Thornacre recipe's m_fTargetHeight, i.e. the value every FLATTEN
// dab levels its pads and lanes to -- NOT a measured surface. No ground-truth
// oracle exists for Thornacre yet (Dawnmere's is a raycast oracle; this town has
// none). It is legitimate here ONLY because every anchor below is asserted to lie
// inside the flattened "RouteGate" pad, where the bake really does drive the
// height to this value. R1-3 adds the raycast oracle and re-freezes these as
// measured literals.
inline constexpr float fZM_THORNACRE_PROVISIONAL_GROUND_Y = 28.0f;

// ---- The arrival marker ------------------------------------------------------
//
// The XZ of the Thornacre recipe's ROUTE-ARRIVAL landmark -- the one named after
// the inbound Route 1 edge -- which sits inside the flattened "RouteGate" pad.
// Both claims are reconciled against the recipe by the boot unit; nothing here
// reads the recipe itself (see the banner), and the landmark's name is not
// spelled here because it is also a spawn tag.
inline constexpr float fZM_THORNACRE_SOUTH_ARRIVAL_X = 512.0f;
inline constexpr float fZM_THORNACRE_SOUTH_ARRIVAL_Z = 112.0f;

// ★ A ZM_SpawnPoint's AUTHORED TRANSFORM IS THE FEET, NOT THE BODY CENTRE.
// ZM_GameStateManager::CalculateSpawnCenter adds fZM_HUMAN_BODY_HALF_HEIGHT at
// warp time, so authoring a body centre on a marker would warp every arriving
// player half a body into the air.
inline constexpr float fZM_THORNACRE_ARRIVAL_FEET_Y =
	fZM_THORNACRE_PROVISIONAL_GROUND_Y;

inline Zenith_Maths::Vector3 ZM_GetThornacreSouthArrivalFeet()
{
	return Zenith_Maths::Vector3(
		fZM_THORNACRE_SOUTH_ARRIVAL_X,
		fZM_THORNACRE_ARRIVAL_FEET_Y,
		fZM_THORNACRE_SOUTH_ARRIVAL_Z);
}

// The RESTING body centre a placed or warped-in player occupies -- the
// vocabulary every human position in this game is stated in (ZM_HumanBody.h).
// Read by the gate-clearance and camera arithmetic below.
inline Zenith_Maths::Vector3 ZM_GetThornacreSouthArrivalBodyCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetThornacreSouthArrivalFeet();
	xCentre.y += fZM_HUMAN_BODY_HALF_HEIGHT;
	return xCentre;
}

// ---- The authored player -----------------------------------------------------
//
// ★ ZM-D-184: AN AUTHORED DYNAMIC BODY SPAWNS WITH CLEARANCE, NEVER AT EXACT
// CONTACT. A body authored touching the terrain bursts physics substeps on the
// first frame and falls THROUGH the ground (that is how Vesper vanished). One
// extra half-extent is the rule ZM_DawnmereTrainerSpawnY / ZM_DawnmereWandererSpawnY
// already follow, and the boot unit pins the gap at exactly one half-extent --
// not zero, not two.
inline constexpr float fZM_THORNACRE_AUTHORED_PLAYER_CLEARANCE =
	fZM_HUMAN_BODY_HALF_HEIGHT;

// The value AddStep_SetTransformPosition takes for the Player entity: the resting
// centre, lifted by one clearance.
inline Zenith_Maths::Vector3 ZM_GetThornacreAuthoredPlayerCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetThornacreSouthArrivalBodyCentre();
	xCentre.y += fZM_THORNACRE_AUTHORED_PLAYER_CLEARANCE;
	return xCentre;
}

// ---- The camera --------------------------------------------------------------
//
// ★ ZM-D-173, restated for an outdoor arrival. ZM_FollowCamera keeps the yaw the
// SCENE authored and the spring places the camera BEHIND its subject; camera
// forward at yaw 0 is +Z, because ZM_ForwardFromRotation rotates the +Z basis.
// The player arrives at the town's SOUTH edge off Route 1 and walks NORTH into
// it, so yaw 0 looks into town and the camera trails to -Z, out over the route
// gate, where there is nothing to clip into.
inline constexpr float fZM_THORNACRE_CAMERA_YAW   = 0.0f;

// Radians, tilted DOWN. Chosen independently for this arrival; it is not a mirror
// of any other camera in the game, and the sign is load-bearing -- flip it and
// the eye ends up under the terrain (which the camera unit's anti-vacuity arm
// runs through this same arithmetic to prove).
inline constexpr float fZM_THORNACRE_CAMERA_PITCH = -0.22f;

inline constexpr float fZM_THORNACRE_CAMERA_ARM          = 6.0f;   // pivot -> camera
inline constexpr float fZM_THORNACRE_CAMERA_PIVOT_HEIGHT = 0.60f;  // above body centre
inline constexpr float fZM_THORNACRE_CAMERA_FOV_DEGREES  = 65.0f;
inline constexpr float fZM_THORNACRE_CAMERA_NEAR         = 0.1f;

// The town is a 1024 x 1024 m region and the camera looks up its length from the
// southern gate, so a 100 m far plane (the interiors' figure) would clip the
// world away in front of the player.
inline constexpr float fZM_THORNACRE_CAMERA_FAR          = 2400.0f;
inline constexpr float fZM_THORNACRE_CAMERA_ASPECT       = 16.0f / 9.0f;

// The floor the camera unit enforces above the provisional ground: a pitch sign
// error buries the eye in the terrain and nothing else in the game would notice.
inline constexpr float fZM_THORNACRE_CAMERA_MIN_GROUND_CLEARANCE = 1.0f;

// The pose the arriving player is actually FILMED FROM: the pivot (the arrival
// body centre, lifted by the follow camera's pivot height) with the arm swung
// back along the camera's forward vector.
//
// ★ THE TRIGONOMETRY LIVES INSIDE THIS ACCESSOR AND NOWHERE ELSE, AND NOTHING
// IT PRODUCES IS EVER FROZEN INTO A SCENE. std::cos / std::sin are libm results
// that MSVC Debug and Release codegen disagree on by 1-2 ULP, which is exactly
// how ZM-D-183 made a committed .zscen ping-pong in git forever. This value is
// read by boot units and by the authoring's clearance reasoning; the authored
// camera ENTITY carries no rotation built from it.
inline Zenith_Maths::Vector3 ZM_GetThornacreSettledCameraPosition()
{
	Zenith_Maths::Vector3 xPivot = ZM_GetThornacreSouthArrivalBodyCentre();
	xPivot.y += fZM_THORNACRE_CAMERA_PIVOT_HEIGHT;

	// Forward at (yaw, pitch), in the convention ZM_ForwardFromRotation states:
	// yaw 0 is +Z, and a NEGATIVE pitch looks down.
	const float fCosPitch = std::cos(fZM_THORNACRE_CAMERA_PITCH);
	const Zenith_Maths::Vector3 xForward(
		std::sin(fZM_THORNACRE_CAMERA_YAW) * fCosPitch,
		std::sin(fZM_THORNACRE_CAMERA_PITCH),
		std::cos(fZM_THORNACRE_CAMERA_YAW) * fCosPitch);

	return xPivot - xForward * fZM_THORNACRE_CAMERA_ARM;
}

// ---- The return gate volume ---------------------------------------------------

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line identical
// to ZM_PlayerHomeBlockout in the sibling Source/World/ZM_PlayerHomePlacement.h
// (which itself duplicates ZM_ProfLabBlockout and ZM_DawnmereBlockout).
// Consolidating all of them into a shared Source/World/ZM_Blockout.h would touch
// the Dawnmere, ProfLab and PlayerHome authoring blocks and their test suites,
// which is out of R1-1's scope. Do the merge as its own change, not as a
// drive-by.
struct ZM_ThornacreVolume
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// Wide enough that the walked corridor has no way past it. The route gate sits in
// a 30 m-radius flattened pad on the town's south edge; 48 m centred on the lane
// leaves no walk-around inside that corridor. Side blockers are a later slice's
// business, not this one's.
inline constexpr float fZM_THORNACRE_GATE_SCALE_X = 48.0f;

// Taller than fZM_HUMAN_BODY_HEIGHT (1.8), so the sensor cannot be stepped over.
inline constexpr float fZM_THORNACRE_GATE_SCALE_Y = 4.0f;

// Depth. ZM_WarpTrigger fires from OnCollisionEnter -- a body-vs-body contact --
// so the window a walker spends touching the box is this depth plus two capsule
// radii, which is many physics ticks rather than a plane a running capsule can
// step over between them.
inline constexpr float fZM_THORNACRE_GATE_SCALE_Z = 6.0f;

// ★ THE GATE SITS **ON** THE GROUND, AND THAT IS THE WHOLE POINT OF THIS LINE.
// A trigger authored at world Y 0 is a tunnel 28 m below the player: it never
// fires, the return warp is dead, and every compiled scale constant above still
// looks perfectly correct. The rule is identical in form to Dawnmere's triggers
// (its ground + half its height).
inline constexpr float fZM_THORNACRE_GATE_CENTRE_Y =
	fZM_THORNACRE_PROVISIONAL_GROUND_Y + fZM_THORNACRE_GATE_SCALE_Y * 0.5f;

// 12 m SOUTH of the arrival marker, still inside the flattened "RouteGate" pad.
inline constexpr float fZM_THORNACRE_SOUTH_GATE_X = 512.0f;
inline constexpr float fZM_THORNACRE_SOUTH_GATE_Z = 100.0f;

// ★ THE Q-2026-08-15-001 LESSON, AS A NUMBER. An arriving player walks off the
// marker immediately, and a diagonal walk-up spends depth as fast as it spends
// width -- which is how a professor's body came to clip an exit sensor that
// "obviously" cleared him. The near face of a gate must clear the arrival body
// centre by at least this, or arriving in town instantly re-warps the player back
// to the route: an infinite ping-pong between two scenes, with every other unit
// green.
inline constexpr float fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN = 4.0f;

inline ZM_ThornacreVolume ZM_GetThornacreSouthGate()
{
	return {
		Zenith_Maths::Vector3(
			fZM_THORNACRE_SOUTH_GATE_X,
			fZM_THORNACRE_GATE_CENTRE_Y,
			fZM_THORNACRE_SOUTH_GATE_Z),
		Zenith_Maths::Vector3(
			fZM_THORNACRE_GATE_SCALE_X,
			fZM_THORNACRE_GATE_SCALE_Y,
			fZM_THORNACRE_GATE_SCALE_Z) };
}

// ============================================================================
// ★ CLOSING NOTE -- NOTHING IN THIS STUB CARRIES AN AUTHORED ROTATION, AND THAT
// IS WHY THERE ARE NO FROZEN BITS IN THIS FILE.
//
// The terrain, the arrival marker and the return gate are all axis-aligned; the
// player is a CAPSULE because it also has to move; the camera's pose is computed,
// never serialized from trigonometry. So every static collider Thornacre authors
// is correctly COLLISION_VOLUME_TYPE_AABB, and "upgrading" one to OBB would
// rewrite committed bytes for zero behavioural gain.
//
// ★ IF A LATER SLICE ADDS SOMETHING THAT MUST FACE A DIRECTION -- an NPC, a sign,
// a shopkeeper -- READ ZM-D-193 FIRST. An AABB body forces JPH::Quat::sIdentity()
// and the physics->transform sync writes that identity straight back into the
// SAVED SCENE BYTES with every compiled-constant unit still green, so
// AddStep_SetTransformRotationQuat on an AABB entity is a SILENT NO-OP. Such an
// entity takes OBB (or a dynamic capsule if it walks), and its rotation is a
// FROZEN std::bit_cast constant emitted BETWEEN the scale step and the collider
// step -- never AddStep_SetTransformYaw, which builds its quaternion with libm at
// authoring time and made a committed scene differ Debug vs Release (ZM-D-183).
//
// And it is still not a gym door. See the top of this file.
// ============================================================================
