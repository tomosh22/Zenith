#pragma once

#include <bit>     // bit_cast -- the trainers' FROZEN facing (ZM-D-183)
#include <cmath>   // cos/sin -- the settled camera pose, which nothing authored reads

#include "Maths/Zenith_Maths.h"   // Vector3 + Quat
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // the compiled world table -- READ by the gate resolvers, NEVER mirrored
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE human body contract

// ============================================================================
// ZM_Route1Placement (S8 item 2, R1-1) -- the authored Route 1 anchors that BOTH
// the tools scene authoring and the tests must agree on, in ONE place.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely. HEADER-ONLY: every accessor is `inline`, so there is no
// paired .cpp to keep in step (Dawnmere's .cpp exists only because it owns
// MEASURED tables; nothing here is measured yet -- see the PROVISIONAL star).
//
// ★ NEVER RE-SPELL A LITERAL FROM THIS FILE AT A CALL SITE. A constant spelled
// twice cannot red a drift -- both sides move together and the assertion becomes
// decorative. If you need a number here, include this header and read it.
//
// ★ NO BUILD INDEX AND NO SPAWN TAG IS SPELLED IN THIS FILE. Route 1's build
// index, its scene kind, its connection edges and the tags they ask their targets
// for all live in the compiled world table -- Source/Data/ZM_WorldSpec.cpp, row
// ZM_SCENE_ROUTE1. Duplicating any of them here would create a second inventory
// nothing reconciles. That is why the two gate accessors below are RESOLVERS:
// they WALK that row's connection list and hand back what the TABLE says, so a
// table edit that re-pointed or re-tagged an edge moves the authoring and the
// boot units together. Reading the table is the opposite of mirroring it.
//
// ★ WHY THAT MATTERS MORE HERE THAN ANYWHERE ELSE IN THE GAME.
// ZM_GameStateManager::IsWarpDestinationValid consults ONLY the compiled table --
// never the scene registry, never the destination scene -- so a warp at a scene
// that does not exist is ACCEPTED, and the machine then parks in
// ZM_WARP_TRANSITION_WAITING_FOR_SCENE / _WAITING_FOR_SPAWN, NEITHER OF WHICH HAS
// A TIMEOUT: a permanent black screen behind an opaque fade with the player
// frozen. Not a crash, not a red test. Source/World/ZM_SceneRegistry.h is the
// other half of closing that hole; this file is the half that names the gates.
//
// ★ THIS HEADER DOES NOT INCLUDE ZM_TerrainAuthoring.h, DELIBERATELY. That header
// drags <filesystem>, <string> and Zenith_Vector into every consumer, and this one
// is included by leaf test TUs. The anchors below are RECONCILED against the
// Route 1 terrain recipe by the boot units in Tests/ZM_Tests_Route1Placement.cpp,
// which include both -- so the two tables are compared rather than merged.
//
// ★ AND IT DOES NOT INCLUDE ZM_ThornacrePlacement.h. The two scenes' only shared
// seam is the compiled world table, which both headers read independently; an
// include either way would make one town's anchors reachable from the other's
// authoring by accident.
//
// ★ BYTE-NEEDLE WARNING, RECORDED HERE BECAUSE THE TRAP IS IN A LATER SLICE. The
// committed-bytes needles in Tests/ZM_Tests_CommittedSceneBytes.cpp search BARE
// NAME BYTES with no NUL over the whole .zscen blob. Do NOT build a `== 1` needle
// on szZM_ROUTE1_TERRAIN_ENTITY_NAME: Zenith_TerrainComponent's load path names
// its rebuilt materials <entityName> + "_Terrain_Mat" + slot
// (Zenith/EntityComponent/Components/Zenith_TerrainComponent.cpp:866-889), so a
// scene that is ever LOADED-then-RE-SAVED can carry that name more than once.
// Needle the gate and arrival names instead -- and see the note on the player
// name below, which is a strict substring of a component TYPE name and therefore
// needs the STRICTLY-MORE clause rather than an equality.
// ============================================================================

// ---- The scene, and every entity Route 1 authors ----------------------------
// The scene NAME handed to AddStep_CreateScene. The SAVE PATH is deliberately not
// here: the shipped registrations build it from GAME_ASSETS_DIR and
// ZENITH_SCENE_EXT, which compile-time literal concatenation cannot consume a
// const char* for. The FILE STEM lives once, in Source/World/ZM_SceneRegistry.h.
inline constexpr const char* szZM_ROUTE1_SCENE_NAME = "Route1";

// The Terrain + Collider + ZM_TerrainGrass host entity. See the byte-needle star
// in the file header before you point a committed-bytes count at this one.
inline constexpr const char* szZM_ROUTE1_TERRAIN_ENTITY_NAME = "Route1Terrain";

// ★ THE PLAYER IS NAMED "Player", IN EVERY SCENE, AND THIS ONE IS NOT NEGOTIABLE.
// ZM_FollowCamera::ResolveTarget looks its subject up as
// pxSceneData->FindEntityByName("Player") (Components/ZM_FollowCamera.cpp:390) --
// production, scene-agnostic code, and the ONLY FindEntityByName call in the whole
// of Games/Zenithmon/Components/ and Games/Zenithmon/Source/. On a miss it clears
// m_xTargetEntityID and hands back an invalid entity, and
// ZM_GameStateManager::PollForCameraAndBeginFadeIn then bare-returns while the
// camera has no target -- another barrier with NO TIMEOUT, i.e. the same permanent
// black screen this slice exists to make structurally impossible. A scene-unique
// "Route1Player" would look tidier and would ship exactly that defect.
// Source/World/ZM_ProfLabPlacement.h:69 spells the same constant for the same
// reason. DO NOT "fix" this to a scene-unique name.
//
// The cost, stated rather than hidden: "Player" IS a strict substring of the
// serialized component type name "ZM_PlayerController", so a committed-bytes
// needle on it must use the STRICTLY-MORE clause the shipped tests already carry,
// never a `== 1` equality, and it must be excluded from any "no entity name is a
// substring of a component type name" battery.
inline constexpr const char* szZM_ROUTE1_PLAYER_ENTITY_NAME = "Player";

// The follow camera. Deliberately NOT "Route1PlayerCamera": a name that CONTAINED
// another authored name is the FromLab/FromLabSpawn trap, where a needle on the
// shorter name silently counts the longer one too.
inline constexpr const char* szZM_ROUTE1_CAMERA_ENTITY_NAME = "Route1Camera";

// The ZM_SpawnPoint the Dawnmere -> Route 1 warp lands on, and the one the
// Thornacre -> Route 1 warp lands on.
//
// ★ NEITHER NAME CONTAINS THE SPAWN TAG IT CARRIES, DELIBERATELY. Tags land in the
// same .zscen byte range that entity names do, so a marker named after its own tag
// inflates every future tag needle by one and forces the strictly-more clause on a
// claim that ought to be a plain equality. Named ...Arrival for that reason alone;
// the tag itself is the world table's property and is resolved, never spelled.
inline constexpr const char* szZM_ROUTE1_SOUTH_ARRIVAL_ENTITY_NAME =
	"Route1SouthArrival";
inline constexpr const char* szZM_ROUTE1_NORTH_ARRIVAL_ENTITY_NAME =
	"Route1NorthArrival";

// The two ZM_WarpTrigger sensors: south back to Dawnmere, north on to Thornacre.
inline constexpr const char* szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME = "Route1SouthGate";
inline constexpr const char* szZM_ROUTE1_NORTH_GATE_ENTITY_NAME = "Route1NorthGate";

// The two roadside trainers. The Npc_<Who> prefix matches every shipped NPC
// (Npc_RivalVesper, Npc_ProfAster), so a reader recognises them on sight, and it
// is the key the committed-bytes tripwire will look them up by.
inline constexpr const char* szZM_ROUTE1_TRAINER_RAMBLER_ENTITY_NAME =
	"Npc_Route1Rambler";
inline constexpr const char* szZM_ROUTE1_TRAINER_FORAGER_ENTITY_NAME =
	"Npc_Route1Forager";

// ---- The two gate edges, RESOLVED from the compiled world table ---------------
//
// ★ WHY THESE ARE FUNCTIONS AND NOT A PAIR OF BUILD INDICES AND A PAIR OF TAG
// STRINGS. See the third star in the file header: both are the table's property.
// These walk the ZM_SCENE_ROUTE1 row for the edge that targets the scene asked
// for, exactly as ZM_GetProfLabExitTargetBuildIndex / ZM_GetProfLabExitSpawnTag
// do for the lab door.
//
// ★ AND THE WALK IS BY TARGET, NOT BY INDEX 0 OR 1. m_pxConnections[0] would be a
// magic index that silently returns the wrong edge the day Route 1 gains a third
// connection -- which is the kind of change a later stage makes without reading
// this file. Route 1's two shipped edges happen to ask BOTH of their targets for a
// tag of the same name, so an index mix-up would not even change the tag: it would
// hand a south gate Thornacre's build index and read perfectly.

// The answer when the compiled table carries no such edge at all. Deliberately a
// build index no scene can hold rather than a plausible one, so a caller that
// skips the resolution check authors an obviously dead warp instead of a subtly
// wrong one. The boot units assert this value is never actually produced.
inline constexpr u_int uZM_ROUTE1_GATE_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL, in this file's house style: a table with no such edge yields nullptr
// rather than UB, and the accessors below turn that into their own stated
// sentinels. NOTHING HERE ASSERTS -- Zenith_Assert calls Zenith_DebugBreak() in
// EVERY configuration and the whole unit suite runs at boot, so an assert on an
// argument a unit deliberately feeds does not fail one test, it ends the run.
// ZM_GetWorldSpec ITSELF asserts fatally out of range (ZM_WorldSpec.cpp:76),
// which is why every call to it below is guarded first.
inline const ZM_SceneConnection* ZM_GetRoute1GateConnection(ZM_SCENE_ID eTarget)
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
	if (xRow.m_pxConnections == nullptr)
	{
		return nullptr;
	}
	for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
	{
		if (xRow.m_pxConnections[uEdge].m_eTarget == eTarget)
		{
			return &xRow.m_pxConnections[uEdge];
		}
	}
	return nullptr;
}

// South is Dawnmere and north is Thornacre -- a fact about the ROUTE's geometry
// (the south arrival sits at low Z, the north one at high Z), which is this
// file's business, unlike the indices and tags those two scenes own.
inline ZM_SCENE_ID ZM_GetRoute1SouthGateTargetScene()
{
	return ZM_GetRoute1GateConnection(ZM_SCENE_DAWNMERE) != nullptr
		? ZM_SCENE_DAWNMERE
		: ZM_SCENE_NONE;
}

inline ZM_SCENE_ID ZM_GetRoute1NorthGateTargetScene()
{
	return ZM_GetRoute1GateConnection(ZM_SCENE_THORNACRE) != nullptr
		? ZM_SCENE_THORNACRE
		: ZM_SCENE_NONE;
}

// TOTAL: the guard is on m_eTarget, not on the pointer alone -- ZM_SCENE_NONE (and
// anything past it) must never reach ZM_GetWorldSpec.
inline u_int ZM_GetRoute1GateTargetBuildIndex(ZM_SCENE_ID eTarget)
{
	const ZM_SceneConnection* pxEdge = ZM_GetRoute1GateConnection(eTarget);
	return pxEdge != nullptr && pxEdge->m_eTarget < ZM_SCENE_COUNT
		? ZM_GetWorldSpec(pxEdge->m_eTarget).m_uBuildIndex
		: uZM_ROUTE1_GATE_TARGET_UNRESOLVED;
}

inline u_int ZM_GetRoute1SouthGateTargetBuildIndex()
{
	return ZM_GetRoute1GateTargetBuildIndex(ZM_SCENE_DAWNMERE);
}

inline u_int ZM_GetRoute1NorthGateTargetBuildIndex()
{
	return ZM_GetRoute1GateTargetBuildIndex(ZM_SCENE_THORNACRE);
}

// The tag a gate asks its destination for -- and therefore ALSO the tag that
// destination's arrival marker must carry. ONE spelling for both sides of the seam
// is the whole point: IsWarpDestinationValid consults only the table and never the
// scene, so a marker tagged with anything else passes validation and then parks the
// warp machine in WAITING_FOR_SPAWN forever.
//
// TOTAL: "" on a miss, NEVER nullptr -- a caller may index [0] without checking.
inline const char* ZM_GetRoute1GateSpawnTag(ZM_SCENE_ID eTarget)
{
	const ZM_SceneConnection* pxEdge = ZM_GetRoute1GateConnection(eTarget);
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

inline const char* ZM_GetRoute1SouthGateSpawnTag()
{
	return ZM_GetRoute1GateSpawnTag(ZM_SCENE_DAWNMERE);
}

inline const char* ZM_GetRoute1NorthGateSpawnTag()
{
	return ZM_GetRoute1GateSpawnTag(ZM_SCENE_THORNACRE);
}

// ---- The ground plane --------------------------------------------------------
//
// ★★ PROVISIONAL / UNMEASURED -- RE-MEASURE IN R1-2 (see the raycast oracle
// ZM_DawnmereNpcGroundTruth_Test, Tests/ZM_AutoTests_CameraClearance.cpp).
//
// This is the Route 1 recipe's m_fTargetHeight, i.e. the height every FLATTEN dab
// drives its paths and pads TO -- not a surface anybody has measured. Route 1 has
// no ground-truth oracle at all today; Dawnmere's is a raycast against a loaded
// scene, and no Route1.zscen exists to raycast.
//
// It is legitimate here ONLY because every anchor below is asserted to lie inside a
// flattened pad or inside the lane's flatten corridor, where the bake really does
// drive the height to this value. Move an anchor out of a flattened region and this
// number becomes a lie that no compiled-constant unit can see -- the arriving body
// either floats or spawns inside the terrain. R1-2 lands the raycast oracle and
// re-freezes these as MEASURED literals.
inline constexpr float fZM_ROUTE1_PROVISIONAL_GROUND_Y = 26.0f;

// ---- The two arrival markers -------------------------------------------------
//
// Both sit on the Route 1 recipe's landmarks of the same name as the inbound tag,
// inside the SouthGate / NorthGate pads (flatten radius 30 m each), so the ground
// under them is genuinely levelled to the height above. The boot unit
// Route1_ArrivalMarkersSitOnTheRecipeLandmarksTheTerrainFlattened reconciles these
// four numbers against the recipe -- two independently authored tables compared,
// which is why it is a check and not a re-spelling.
inline constexpr float fZM_ROUTE1_SOUTH_ARRIVAL_X = 512.0f;
inline constexpr float fZM_ROUTE1_SOUTH_ARRIVAL_Z = 112.0f;
inline constexpr float fZM_ROUTE1_NORTH_ARRIVAL_X = 512.0f;
inline constexpr float fZM_ROUTE1_NORTH_ARRIVAL_Z = 1424.0f;

// ★ A ZM_SpawnPoint's AUTHORED TRANSFORM IS THE FEET, NOT THE BODY CENTRE.
// ZM_GameStateManager::CalculateSpawnCenter adds fZM_HUMAN_BODY_HALF_HEIGHT at warp
// time, so authoring a body centre here would warp every arriving player half a
// body high and drop them. Same vocabulary as fZM_PROFLAB_SPAWN_FEET_Y.
inline constexpr float fZM_ROUTE1_ARRIVAL_FEET_Y = fZM_ROUTE1_PROVISIONAL_GROUND_Y;

inline Zenith_Maths::Vector3 ZM_GetRoute1SouthArrivalFeet()
{
	return Zenith_Maths::Vector3(
		fZM_ROUTE1_SOUTH_ARRIVAL_X,
		fZM_ROUTE1_ARRIVAL_FEET_Y,
		fZM_ROUTE1_SOUTH_ARRIVAL_Z);
}

inline Zenith_Maths::Vector3 ZM_GetRoute1NorthArrivalFeet()
{
	return Zenith_Maths::Vector3(
		fZM_ROUTE1_NORTH_ARRIVAL_X,
		fZM_ROUTE1_ARRIVAL_FEET_Y,
		fZM_ROUTE1_NORTH_ARRIVAL_Z);
}

// The RESTING body centre a placed player occupies at the south marker: feet plus
// one half height, exactly what CalculateSpawnCenter computes at warp time.
//
// ★ THERE IS NO NORTH EQUIVALENT, DELIBERATELY. Nothing reads one -- the authored
// player, the camera and the trainer facings are all measured against the SOUTH
// arrival, because that is the door a player first walks in through. An unread
// accessor is not a check, it is a function that LOOKS like one; add the north twin
// in the same change as the code that reads it.
inline Zenith_Maths::Vector3 ZM_GetRoute1SouthArrivalBodyCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetRoute1SouthArrivalFeet();
	xCentre.y += fZM_HUMAN_BODY_HALF_HEIGHT;
	return xCentre;
}

// ---- The authored player -----------------------------------------------------
//
// ★ ZM-D-184: AN AUTHORED **DYNAMIC** BODY SPAWNS WITH CLEARANCE, NEVER AT EXACT
// CONTACT. Vesper's fall-through was an unbounded physics substep burst triggered
// by a body authored resting exactly on the surface; the fix capped the substeps
// AND made every authored dynamic body start one half-extent clear. One extra half
// height, the same rule ZM_DawnmereTrainerSpawnY / ZM_DawnmereWandererSpawnY follow.
inline constexpr float fZM_ROUTE1_AUTHORED_PLAYER_CLEARANCE =
	fZM_HUMAN_BODY_HALF_HEIGHT;

// The value AddStep_SetTransformPosition takes in R1-2: the resting centre above,
// plus that one clearance -- i.e. the south marker's feet plus TWO half heights.
inline Zenith_Maths::Vector3 ZM_GetRoute1AuthoredPlayerCentre()
{
	Zenith_Maths::Vector3 xCentre = ZM_GetRoute1SouthArrivalBodyCentre();
	xCentre.y += fZM_ROUTE1_AUTHORED_PLAYER_CLEARANCE;
	return xCentre;
}

// ---- The camera --------------------------------------------------------------
//
// ★ ZM-D-173, restated for a route. ZM_FollowCamera keeps the yaw the SCENE
// authored and the spring places the camera BEHIND its subject, and camera forward
// at yaw 0 is +Z (ZM_ForwardFromRotation rotates the +Z basis). The player enters
// Route 1 from the south and walks +Z -- Dawnmere is behind them, Thornacre ahead --
// so yaw 0 looks straight up the route with the camera trailing at -Z. Author any
// other yaw and every clearance figure below is void.
inline constexpr float fZM_ROUTE1_CAMERA_YAW = 0.0f;

// Tilted DOWN. Chosen independently for this route rather than mirrored from
// another scene's camera: an interior camera and a 1536 m corridor have no reason
// to share a pitch, and a mirror would silently move both if either were re-tuned.
inline constexpr float fZM_ROUTE1_CAMERA_PITCH = -0.22f;   // radians

inline constexpr float fZM_ROUTE1_CAMERA_ARM           = 6.0f;   // pivot -> camera
inline constexpr float fZM_ROUTE1_CAMERA_PIVOT_HEIGHT  = 0.60f;  // above the body CENTRE
inline constexpr float fZM_ROUTE1_CAMERA_FOV_DEGREES   = 65.0f;
inline constexpr float fZM_ROUTE1_CAMERA_NEAR          = 0.1f;

// ★ THE FAR PLANE IS A ROUTE-LENGTH DECISION, NOT A DEFAULT. Route 1 is 1536 m deep
// on Z (the recipe's world bounds), so the 100 m far plane the two interiors ship
// with would clip the world away a few strides ahead of the player. The boot unit
// Route1_SettledCameraStandsAboveGroundBehindTheArrival asserts this value covers
// the recipe's own Z extent rather than trusting the comment.
inline constexpr float fZM_ROUTE1_CAMERA_FAR    = 2400.0f;
inline constexpr float fZM_ROUTE1_CAMERA_ASPECT = 16.0f / 9.0f;

// The floor the camera unit enforces: how far above the ground plane the settled
// eye must sit. A SIGN ERROR ON THE PITCH puts the eye underground -- the camera
// swings below the pivot instead of above it -- and nothing else in the game would
// notice, which is exactly why this is a named constant with a reader.
inline constexpr float fZM_ROUTE1_CAMERA_MIN_GROUND_CLEARANCE = 1.0f;

// The pose the arriving player is actually FILMED FROM: the arm swung back from a
// pivot that sits fZM_ROUTE1_CAMERA_PIVOT_HEIGHT above the arriving body's centre.
//
// ★ THE TRIGONOMETRY LIVES INSIDE THIS ACCESSOR AND NOWHERE ELSE, AND ITS RESULT IS
// NEVER AUTHORED (ZM-D-183). std::cos/std::sin disagree by 1-2 ULP between MSVC
// Debug and Release codegen, so a libm result must not decide bytes in a committed
// .zscen. This is a CHECK a boot unit runs, not a value AddStep_* ever takes -- the
// authored camera entity gets its own dyadic constants in R1-2.
inline Zenith_Maths::Vector3 ZM_GetRoute1SettledCameraPosition()
{
	Zenith_Maths::Vector3 xPivot = ZM_GetRoute1SouthArrivalBodyCentre();
	xPivot.y += fZM_ROUTE1_CAMERA_PIVOT_HEIGHT;

	// Forward at (yaw, pitch), in the same convention ZM_ForwardFromRotation
	// implies: yaw 0 is +Z, a negative pitch looks DOWN.
	const float fCosPitch = std::cos(fZM_ROUTE1_CAMERA_PITCH);
	const Zenith_Maths::Vector3 xForward(
		std::sin(fZM_ROUTE1_CAMERA_YAW) * fCosPitch,
		std::sin(fZM_ROUTE1_CAMERA_PITCH),
		std::cos(fZM_ROUTE1_CAMERA_YAW) * fCosPitch);

	return xPivot - xForward * fZM_ROUTE1_CAMERA_ARM;
}

// ---- The two gate volumes ----------------------------------------------------

// ★ BOOKED DUPLICATION, not an oversight: this struct is line-for-line identical to
// ZM_ProfLabBlockout in the sibling Source/World/ZM_ProfLabPlacement.h (which itself
// duplicates ZM_DawnmereBlockout and ZM_PlayerHomeBlockout). Consolidating all four
// into a shared Source/World/ZM_Blockout.h would touch the Dawnmere, ProfLab and
// PlayerHome authoring blocks and their test suites, which is out of R1-1's scope.
// Do the merge as its own change, not as a drive-by.
struct ZM_Route1Volume
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// (1) X SPAN -- wide enough that the corridor cannot be walked around. The DirtLane
//     lays 9 m of dirt radius (an 18 m visible path) inside a 16 m flatten radius,
//     so 48 m centred on the lane leaves no gap a player can slip through without
//     leaving the walked corridor entirely. Side blockers are a later slice's job;
//     this number is what makes the gate un-side-stepped in the meantime.
inline constexpr float fZM_ROUTE1_GATE_SCALE_X = 48.0f;

// (2) Y SPAN -- taller than a person (fZM_HUMAN_BODY_HEIGHT is 1.8), so the sensor
//     cannot be stepped or jumped over.
inline constexpr float fZM_ROUTE1_GATE_SCALE_Y = 4.0f;

// (3) DEPTH -- a collision sensor is sampled at the physics tick, so it has to be
//     several ticks deep rather than a plane a running capsule steps over between
//     two ticks. Six metres is ~26 ticks at a 4 m/s walk and 60 Hz, and the contact
//     window is a further body radius either side (ZM_WarpTrigger fires from
//     OnCollisionEnter, i.e. body-vs-body).
inline constexpr float fZM_ROUTE1_GATE_SCALE_Z = 6.0f;

// ★ THE BOX SITS **ON** THE GROUND, WHICH IS WHY ITS CENTRE IS NOT ITS ANCHOR.
// Route 1's ground plane is 26 m up. A gate authored at world Y 0 is a tunnel 26 m
// BELOW the player: a sensor that never fires, with every compiled scale constant
// looking perfectly correct. Identical in form to Dawnmere's trigger rule (its own
// ground plus 1.25 for a 2.5-tall box); the boot unit
// Route1_GateVolumesAdmitNoStepOverAndSitOnTheGround asserts Min().y lands exactly
// on the ground plane.
inline constexpr float fZM_ROUTE1_GATE_CENTRE_Y =
	fZM_ROUTE1_PROVISIONAL_GROUND_Y + fZM_ROUTE1_GATE_SCALE_Y * 0.5f;

// Each gate stands BEYOND its own marker -- south gate at lower Z than the south
// arrival, north gate at higher Z than the north arrival -- so the sensor is the
// last thing between the player and the edge of the world, and an arriving player
// is never already inside it. Both centres sit inside their pad's flattened disc.
inline constexpr float fZM_ROUTE1_SOUTH_GATE_X = 512.0f;
inline constexpr float fZM_ROUTE1_SOUTH_GATE_Z = 100.0f;
inline constexpr float fZM_ROUTE1_NORTH_GATE_X = 512.0f;
inline constexpr float fZM_ROUTE1_NORTH_GATE_Z = 1436.0f;

// ★★ THE NEAR FACE MUST CLEAR THE ARRIVING BODY, OR THE GATE IS AN INFINITE LOOP.
// A player warps IN onto the marker; if the arriving capsule overlapped the sensor,
// ZM_WarpTrigger would fire on the first contact tick and send them straight back,
// whose gate would send them straight here again, forever, with no input accepted
// between the two.
//
// ★ AND ARRIVAL CLEARANCE IS NOT THE WHOLE CLAIM -- Q-2026-08-15-001. An arriving
// player does not stand on the marker: they walk off it immediately, and the eight-
// way MOVE drive means the natural walk is a 45-degree diagonal, which SPENDS DEPTH
// AS FAST AS IT SPENDS WIDTH. That is the defect that shipped in ProfLab with every
// arrival clause green. This floor is stated on the near FACE, not on the centre,
// for that reason. The actual clearance both gates hold is 9 m -- the marker sits
// 12 m past the gate centre and the box reaches 3 m back toward it.
inline constexpr float fZM_ROUTE1_GATE_ARRIVAL_CLEARANCE_MIN = 4.0f;

inline ZM_Route1Volume ZM_GetRoute1SouthGate()
{
	return {
		Zenith_Maths::Vector3(
			fZM_ROUTE1_SOUTH_GATE_X,
			fZM_ROUTE1_GATE_CENTRE_Y,
			fZM_ROUTE1_SOUTH_GATE_Z),
		Zenith_Maths::Vector3(
			fZM_ROUTE1_GATE_SCALE_X,
			fZM_ROUTE1_GATE_SCALE_Y,
			fZM_ROUTE1_GATE_SCALE_Z) };
}

inline ZM_Route1Volume ZM_GetRoute1NorthGate()
{
	return {
		Zenith_Maths::Vector3(
			fZM_ROUTE1_NORTH_GATE_X,
			fZM_ROUTE1_GATE_CENTRE_Y,
			fZM_ROUTE1_NORTH_GATE_Z),
		Zenith_Maths::Vector3(
			fZM_ROUTE1_GATE_SCALE_X,
			fZM_ROUTE1_GATE_SCALE_Y,
			fZM_ROUTE1_GATE_SCALE_Z) };
}

// ---- The two trainer stations ------------------------------------------------
//
// In AUTHORING ORDER. The authoring loop walks [0, ZM_ROUTE1_TRAINER_COUNT), so
// this order is part of the contract: appending a station is fine, REORDERING
// REWRITES THE SCENE BYTES (ZM-D-148 dense authoring-order file indices).
enum ZM_ROUTE1_TRAINER_ID : u_int
{
	ZM_ROUTE1_TRAINER_RAMBLER,
	ZM_ROUTE1_TRAINER_FORAGER,

	ZM_ROUTE1_TRAINER_COUNT
};

struct ZM_Route1TrainerStation
{
	const char* m_szEntityName;
	float m_fX;
	float m_fZ;
};

// ★★ THE HARD CONSTRAINT ON THE TWO ANCHORS BELOW -- DO NOT "TIDY" THEM.
//
// Source/Interaction/ZM_TrainerSightLogic.h ships fZM_SIGHT_MAX_DISTANCE = 8.0 and
// fZM_SIGHT_MIN_FACING_DOT = 0.8660254 (cos 30 degrees, i.e. a 30-degree HALF
// angle). For a trainer staring down the route at a player walking the lane at
// lateral offset d, being seen needs the along-forward component to be at least
// cos(30) of the range -- i.e. the player must be at least d/tan(30) = d*sqrt(3)
// ahead -- WHILE the range stays inside 8 m. Those two windows only overlap for
//
//     d <= 8.0 * sin(30 degrees) = 4.0 m
//
// so a trainer parked more than 4 m off the walked line can NEVER be triggered, in
// any playthrough, forever -- and the terrain, the scene bytes and every other
// placement check would all be perfectly green. That is the failure mode the boot
// unit Route1_TrainerStationsCanActuallySeeAPlayerWalkingTheLane exists for: it
// samples the recipe's own DirtLane polyline through the SHIPPED sight predicate
// and counts the in-cone samples, with a probe station displaced to 6 m lateral as
// its anti-vacuity arm (which yields zero at every sampling density tried).
//
// ★ AND THE SIGHT CONSTANTS ARE OUT OF SCOPE -- do not widen the cone to fit an
// anchor. Move the anchor.
//
// ★ ONE OF THESE TWO ANCHORS WAS ALREADY DEAD ONCE. A draft forager at (528, 950)
// stood EAST of the lane while the perpendicular foot of the lane fell ~0.56 m
// north of him; facing -Z, his cone window and his range window were disjoint and
// the station produced ZERO in-cone samples at 200, 2000 and 20000 samples per
// segment. He is now WEST of the lane, like the rambler. Side symmetry is not worth
// a trainer nobody can meet.
//
// Measured against the shipped DirtLane polyline
// {(512,64),(480,300),(540,560),(500,820),(550,1080),(512,1472)}:
//   rambler  perpendicular offset 2.1288 m, ~2.2 m of in-cone walked arc
//   forager  perpendicular offset 2.4551 m, a comfortably wider window
// Both sit inside the lane's 16 m FLATTEN radius, so the ground under them is
// levelled to fZM_ROUTE1_PROVISIONAL_GROUND_Y **and** zeroed by the GRASS_ERASE
// phase -- a trainer can never be standing in encounter grass.
inline constexpr float fZM_ROUTE1_TRAINER_RAMBLER_X = 524.0f;
inline constexpr float fZM_ROUTE1_TRAINER_RAMBLER_Z = 650.0f;
inline constexpr float fZM_ROUTE1_TRAINER_FORAGER_X = 522.5f;
inline constexpr float fZM_ROUTE1_TRAINER_FORAGER_Z = 950.0f;

// The clear air a walking player is owed past both bodies once they have passed
// each other. Named so the floor below is a DERIVATION rather than a number picked
// to sit under the anchors.
inline constexpr float fZM_ROUTE1_TRAINER_CLEAR_WALKING_GAP = 1.2f;

// FLOOR: the trainer's own half footprint, plus the walking capsule's radius, plus
// that clear air. Closer than this and a static-adjacent body crowds the walking
// line itself.
inline constexpr float fZM_ROUTE1_TRAINER_MIN_LANE_OFFSET =
	fZM_HUMAN_BODY_FOOTPRINT * 0.5f
	+ fZM_HUMAN_BODY_CAPSULE_RADIUS
	+ fZM_ROUTE1_TRAINER_CLEAR_WALKING_GAP;

// CEILING: inside the 4.0 m geometric limit derived above, with room for an anchor
// to be nudged without silently killing its sight window. The wider of the two
// anchors sits at 2.4551, so this leaves a metre of authoring headroom and still
// half a metre of margin against the limit itself.
inline constexpr float fZM_ROUTE1_TRAINER_MAX_LANE_OFFSET = 3.5f;

// ★ BOTH TRAINERS FACE THE EXACT HALF TURN ABOUT +Y -- forward -Z, i.e. back down
// the route toward the player walking up it.
//
// ★ WHY A HALF TURN AND NOT AN OBLIQUE YAW (ZM-D-183). A +/-90-degree facing needs
// sin(45), which is a libm result MSVC Debug and Release codegen disagree on by 1-2
// ULP -- so it would be a MEASUREMENT, needing Vesper's measure-and-freeze procedure
// and a re-derivation unit, and a committed .zscen that ping-pongs in git if anyone
// skipped either. A half turn about +Y is
//     (x, y, z, w) = (0, sin(pi/2), 0, cos(pi/2)) = (0, 1, 0, 0)
// EXACTLY, in every float format there is: there is no libm result to freeze and no
// configuration that can disagree. The four bit patterns are simply the IEEE-754
// spellings of 0.0f and 1.0f. They are written as std::bit_cast anyway, to the same
// shape as ZM_DawnmereVesperFacing and ZM_ProfLabAsterFacing, so that "an authored
// rotation in a committed scene is a frozen bit pattern" has ONE form in this game.
//
// Declared independently here rather than read out of ProfLab's header: the two
// scenes are unrelated, and an include for four zeros and a one would create a
// dependency a later Aster re-facing could drag through.
inline constexpr u_int uZM_ROUTE1_TRAINER_FACING_X_BITS = 0x00000000u;
inline constexpr u_int uZM_ROUTE1_TRAINER_FACING_Y_BITS = 0x3F800000u;
inline constexpr u_int uZM_ROUTE1_TRAINER_FACING_Z_BITS = 0x00000000u;
inline constexpr u_int uZM_ROUTE1_TRAINER_FACING_W_BITS = 0x00000000u;

// ★ COLLIDER NOTE FOR R1-2, AND IT IS LOAD-BEARING (ZM-D-193). These trainers must
// be authored COLLISION_VOLUME_TYPE_CAPSULE + RIGIDBODY_TYPE_DYNAMIC -- the
// ZM_QueueDawnmereTrainerNpc shape -- and their rotation emitted with
// AddStep_SetTransformRotationQuat(x, y, z, w) BETWEEN the scale step and the
// collider step. A COLLISION_VOLUME_TYPE_AABB body forces JPH::Quat::sIdentity()
// (Zenith_ColliderComponent.cpp:801) and the physics->transform sync then writes
// that identity straight back over the authored rotation and into the SAVED BYTES,
// with every compiled-constant unit in this file still green because the units read
// the constants and the damage lives in the file.
//
// TOTAL: an out-of-range station returns the IDENTITY quat, and that choice is
// deliberate. Identity forward is +Z -- back turned, facing away from every player
// who will ever walk up the route -- which is exactly the value the facing unit's
// anti-vacuity arm proves must FAIL. A degenerate read therefore cannot silently
// ship a plausible-looking trainer.
inline Zenith_Maths::Quat ZM_Route1TrainerFacing(ZM_ROUTE1_TRAINER_ID eStation)
{
	static_assert(std::bit_cast<u_int>(
		std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_Y_BITS)) ==
		uZM_ROUTE1_TRAINER_FACING_Y_BITS,
		"the frozen y component does not round-trip through float");
	static_assert(std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_Y_BITS) == 1.0f,
		"the frozen y component is not exactly 1.0f, so this is no longer a half "
		"turn about +Y and the -Z facing claim above is void");
	static_assert(std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_W_BITS) == 0.0f,
		"the frozen w component is not exactly 0.0f, so this is no longer a half turn");

	if (eStation >= ZM_ROUTE1_TRAINER_COUNT)
	{
		// glm::quat's constructor is (w, x, y, z) -- this is the identity.
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	return Zenith_Maths::Quat(
		std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_W_BITS),
		std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_X_BITS),
		std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_Y_BITS),
		std::bit_cast<float>(uZM_ROUTE1_TRAINER_FACING_Z_BITS));
}

// ============================================================================
// EVERY ACCESSOR BELOW IS TOTAL, in the ZM_GetProfLabBlock house style: no argument
// value, however degenerate, is UB, and none of them calls Zenith_Assert.
//
// The out-of-range answers are deliberately DEGENERATE rather than plausible: a
// name no authored entity carries, at the world's (0, _, 0) corner, which is
// outside every pad, every path corridor and the lane's flatten radius -- so a
// caller that mistakenly indexes past the end fails loudly at its own assertion (or
// at the offset clauses of Route1_TrainerStationsStandOnFlattenedGrassFreeGroundBesideTheLane)
// instead of silently matching a real station.
//
// ZM_GetRoute1TrainerStation RETURNS THE DECLARED NAME CONSTANTS rather than second
// copies of the literals -- one spelling each, so the table and the constants can
// never disagree.
// ============================================================================
inline ZM_Route1TrainerStation ZM_GetRoute1TrainerStation(
	ZM_ROUTE1_TRAINER_ID eStation)
{
	switch (eStation)
	{
	case ZM_ROUTE1_TRAINER_RAMBLER:
		return {
			szZM_ROUTE1_TRAINER_RAMBLER_ENTITY_NAME,
			fZM_ROUTE1_TRAINER_RAMBLER_X,
			fZM_ROUTE1_TRAINER_RAMBLER_Z };

	case ZM_ROUTE1_TRAINER_FORAGER:
		return {
			szZM_ROUTE1_TRAINER_FORAGER_ENTITY_NAME,
			fZM_ROUTE1_TRAINER_FORAGER_X,
			fZM_ROUTE1_TRAINER_FORAGER_Z };

	default: break;
	}
	return { "Route1InvalidTrainerStation", 0.0f, 0.0f };
}

// Where a station's FEET land: on the flattened ground the lane's flatten corridor
// levels for him. Same vocabulary as the arrival markers.
inline Zenith_Maths::Vector3 ZM_GetRoute1TrainerFeet(ZM_ROUTE1_TRAINER_ID eStation)
{
	const ZM_Route1TrainerStation xStation = ZM_GetRoute1TrainerStation(eStation);
	return Zenith_Maths::Vector3(
		xStation.m_fX, fZM_ROUTE1_PROVISIONAL_GROUND_Y, xStation.m_fZ);
}

// His RESTING body centre -- the sight cone's origin, and the point every distance
// in the sight derivation above is measured from. Zenithmon's authored position for
// a human is the body CENTRE, never the feet (ZM_HumanBody.h).
inline Zenith_Maths::Vector3 ZM_GetRoute1TrainerCentre(ZM_ROUTE1_TRAINER_ID eStation)
{
	Zenith_Maths::Vector3 xCentre = ZM_GetRoute1TrainerFeet(eStation);
	xCentre.y += fZM_HUMAN_BODY_HALF_HEIGHT;
	return xCentre;
}

// ...and the value AddStep_SetTransformPosition takes in R1-2: that resting centre
// plus the ZM-D-184 spawn clearance, because these are DYNAMIC capsules and a
// dynamic body authored at exact contact bursts substeps and falls through the
// terrain. The sight geometry is derived at the RESTING centre above, not here --
// the body settles onto it within a frame.
inline Zenith_Maths::Vector3 ZM_GetRoute1TrainerAuthoredCentre(
	ZM_ROUTE1_TRAINER_ID eStation)
{
	Zenith_Maths::Vector3 xCentre = ZM_GetRoute1TrainerCentre(eStation);
	xCentre.y += fZM_ROUTE1_AUTHORED_PLAYER_CLEARANCE;
	return xCentre;
}
