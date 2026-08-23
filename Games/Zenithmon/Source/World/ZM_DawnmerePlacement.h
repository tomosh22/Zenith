#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3 / Quat / AngleAxis
// The compiled world table. R1-3's north seam gate RESOLVES its target build
// index and its outbound spawn tag by walking the ZM_SCENE_DAWNMERE row, exactly
// as ZM_ProfLabPlacement.h's exit resolver walks the ZM_SCENE_PROFLAB one -- so
// this header READS the table and never mirrors it. It arrived here transitively
// through ZM_ProfLabPlacement.h below for a long time; it is named explicitly now
// because this file has its own reason to need it.
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE human body contract
// The ProfLab INTERIOR contract. The S8 SC-D lab block at the bottom of this
// file DERIVES the exterior's aperture from it rather than re-spelling 6.0 and
// 3.0 -- a constant spelled twice cannot red a drift. Pure, header-only, and it
// does NOT include this file back (only names it in a comment), so there is no
// cycle. Contrast the older Home block, which predates that rule and spells its
// jamb X as literals; ZM_Interaction/HomeExterior_... is what binds those.
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

// ============================================================================
// ZM_DawnmerePlacement (S7 item 3 SC8) -- the authored world coordinates that
// BOTH the tools scene authoring and the tests must agree on, in ONE place.
//
// PURE. No ECS, no scene, no physics, no g_xEngine, no allocation, no I/O, and
// no ZENITH_TOOLS guard -- these constants have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely.
//
// WHY THIS FILE EXISTS. Dawnmere is re-authored on every warm windowed tools
// boot, so the committed .zscen bytes must be reproducible from compiled
// constants rather than from anything measured. Before SC8 those constants lived
// only inside a #ifdef ZENITH_TOOLS block in Zenithmon.cpp, which meant no unit
// could assert anything about them: "the rival cannot spot the player standing on
// the whiteout respawn" was an argument in a comment. Here it is a boot unit.
//
// The town-centre anchor is MIGRATED VERBATIM from Zenithmon.cpp's local
// xTownCenterFeet -- identical literals, so no existing authored entity moves by
// one bit.
//
// KNOWN-LIMIT W5 EXTENDED IT. The file now owns the anchor, the rival AND the
// full six-row Dawnmere NPC anchor table with per-NPC MEASURED feet heights (see
// the W5 block at the bottom). Their corridor clearances and their separations are
// boot units rather than comment arithmetic as a result.
// ============================================================================

// ---- The town-centre anchor (the TownCenterSpawn marker's FEET) -------------
// The one sampled terrain surface every Dawnmere placement is derived from, and
// the warp target ZM_GameStateManager uses after a whiteout.
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_X      = 512.0f;
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_FEET_Y = 25.99055f;
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_Z      = 480.0f;

// ---- Rival Vesper (S7 item 4) ----------------------------------------------
//
// DERIVED, NOT EYEBALLED. The two flank NPCs nearest the spawn are the villager
// (512, 490) and the caretaker (498, 498); their midpoint (505, 494) is the
// widest gate out of the plaza core. Take the ray from the TownCenter spawn
// (512, 480) through that midpoint -- direction (-7, +14) -- and extend it by
// exactly 22/7:  (512 - 22, 480 + 44) = (490, 524). That centres the approach
// lane between the two solid static AABBs (~5.0 m clearance each), which matters
// because the tests' DriveTowardXZ has NO obstacle avoidance and a 1.8 m body
// stops the player capsule dead (the step assist is 0.40 m).
//
// Separations, against an 8 m sight range and a 2.9 m interact reach:
//   caretaker (498,498)  27.20 m      warden   (478,498)  28.64 m
//   villager  (512,490)  40.50 m      clerk    (526,498)  44.41 m
//   wanderer patrol nearest endpoint (540,484)  64.03 m
//   TownCenter spawn (512,480)        49.19 m   <-- the whiteout clearance
//   z=480 Home corridor               44.00 m   <-- driven BLIND by
//                                                   ZM_PlayerHomeRoundTrip_Test
//   x=512 spawn-to-villager corridor  40.50 m
// Every one is more than 3x the sight range.
//
// GROUND. Both the spawn (32.0 m from the Plaza pad centre (512,512)) and Vesper
// (25.06 m) lie inside that pad's 45 m dirt radius and 60 m flatten radius
// (ZM_TerrainAuthoring.cpp:67), so both are flattened toward the same target
// height; neither sits inside any path's flatten band (Home 19.96 m vs radius 13,
// Lab 25.06 vs 13, Route 23.19 vs 18). Height therefore reuses the one authored
// xPlayerCenter.y like every other NPC -- and ZM_RivalVesperAuthored_Test MEASURES
// the resulting |dy| against fZM_SIGHT_MAX_VERTICAL rather than trusting it.
//
// ★ GDD DEVIATION (Q-2026-07-24-002 Q-D). GameDesignDocument.md places rival
// battle 1 on "Route 1 (L5, scripted first battle)". Route 1 does not exist in S7;
// Dawnmere is the only authored scene. When a real Route 1 is authored, MOVE HIM
// THERE and re-derive every figure above from scratch -- none of them carries over
// -- exactly as the warden's block in Zenithmon.cpp instructs for the same reason.
inline constexpr float fZM_DAWNMERE_VESPER_X = 490.0f;
inline constexpr float fZM_DAWNMERE_VESPER_Z = 524.0f;

// The yaw that points Vesper back down the approach bearing, at the town centre.
//
// atan2(dx, dz) -- X FIRST, Z SECOND. That argument order is the +Z-forward
// convention ZM_ForwardFromRotation uses (it rotates the +Z basis vector), and
// transposing it silently turns him 90 degrees. Never derived via
// glm::eulerAngles(quat).y, which collapses past 90 degrees off +Z and has already
// cost this repo a full debugging cycle.
//
// ★ THIS IS THE DERIVATION, NOT THE AUTHORED VALUE (ZM-D-183). Nothing that
// writes the scene calls it any more -- see the frozen block below for why. It
// survives as the ORACLE the frozen constant is re-checked against, and as the
// readable statement of where that constant came from.
float ZM_DawnmereVesperYaw();

// ============================================================================
// ZM-D-183 -- THE RIVAL'S FACING IS A FROZEN BIT PATTERN, NOT A COMPUTED VALUE
//
// ★ THE DEFECT THIS CLOSES, AND IT IS NOT THE ONE ZM-D-179 CLOSED. This header's
// opening contract is that the committed Dawnmere.zscen bytes are reproducible
// from COMPILED constants. The facing was NOT one: it was std::atan2 at runtime,
// fed to glm::angleAxis, whose sin/cos of the half angle ran again inside the
// authoring step. Those are libm calls, and MSVC's Debug and Release codegen do
// not agree on them to the last bit. Measured, on the same source at the same
// commit:
//
//     Vulkan_vs2022_Debug_Win64_True     y=0x3F7926D9  w=0x3E6B4456
//     Vulkan_vs2022_Release_Win64_True   y=0x3F7926D8  w=0x3E6B444C
//
// So a Release tools boot re-authored the scene with a rotation 1 ULP off in y
// and ~10 in w, a Debug tools boot put it back, and Dawnmere.zscen ping-ponged
// between two values in git status forever -- which is a permanently disabled
// tripwire, exactly the state ZM-D-179 was written to end.
//
// ★ WHY THE ZM-D-179 GUARD CANNOT SEE THIS, AND WHY NO TOLERANCE TEST CAN EITHER.
// ZM_VerifyAuthoredRivalFacingStep bit-compares the serialized bytes against
// ZM_DawnmereVesperFacing() evaluated IN THE SAME BINARY, so both sides moved
// together and it logged a clean authored == serialised == liveBody while writing
// bytes that differed from the committed ones. The automated test compares |dot|
// against 0.999 and the drift lands at 1 - |dot| ~ 1e-14. Both were green
// throughout. The ONLY thing that can catch a per-config authoring drift is a
// comparison against a value that does not move with the config -- i.e. this
// block, and ZM_Tests_CommittedSceneBytes.cpp checking the committed file.
//
// ★ WHICH BITS THESE ARE, AND WHY. They are the DEBUG build's -- i.e. the bytes
// already committed at HEAD. Neither config is more numerically correct; what
// matters is that ONE value is canonical. Choosing the committed one means the
// freeze needs no re-author and moves no tracked asset.
//
// ★ THE AUTHORING STEP HAD TO CHANGE TOO. AddStep_SetTransformYaw runs
// glm::angleAxis engine-side, so freezing only the yaw would have left the drift
// exactly where it was. The rival is authored through
// AddStep_SetTransformRotationQuat, which performs no math at all. Read that
// step's comment before authoring any other committed entity with a rotation.
//
// ★ WHAT RE-DERIVES THESE. Move fZM_DAWNMERE_VESPER_X/Z or the town centre and
// these four numbers are stale. Vesper_FrozenFacingStillEncodesTheDerivedBearing
// reds when that happens (it re-runs the atan2 derivation against them to a
// tolerance, which is the ONLY comparison that stays valid across configs). To
// re-freeze: read the `authored=` bits off the [ZM Authoring] line of a windowed
// DEBUG tools boot, paste them below, re-author Dawnmere, and commit the .zscen
// in the SAME commit.
// ============================================================================
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_X_BITS = 0x00000000u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_Y_BITS = 0x3F7926D9u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_Z_BITS = 0x00000000u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_W_BITS = 0x3E6B4456u;

// The frozen facing as a quaternion. Bit-identical in every build configuration,
// which is the entire point -- a test may compare an authored transform, or the
// committed scene bytes, against it EXACTLY rather than to a tolerance.
Zenith_Maths::Quat ZM_DawnmereVesperFacing();

// ============================================================================
// KNOWN-LIMIT W5 -- THE PER-NPC AUTHORED FEET HEIGHTS
//
// THE DEFECT THIS CLOSES. Before W5 all six authored Dawnmere NPCs -- the four
// townsfolk, the wanderer and rival Vesper -- reused ONE height, the town-centre
// anchor's fZM_DAWNMERE_TOWN_CENTER_FEET_Y, because the authoring wrote the same
// `xPlayerCenter.y` expression at all six call sites. That made the +/-2 m
// vertical band of BOTH the interact picker and the trainer sight cone
// (fZM_SIGHT_MAX_VERTICAL) an inference from one out-of-band measurement rather
// than a per-NPC fact. An NPC standing on ground more than 2 m off the town
// centre would be permanently un-talkable and, for a trainer, permanently blind.
//
// WHY THESE ARE COMPILED CONSTANTS AND NOT A LIVE TERRAIN SAMPLE. Two independent
// reasons, both hard:
//   1. This header's contract (top of file): the committed Dawnmere.zscen bytes
//      must be reproducible from COMPILED constants. Sampling the terrain during
//      authoring would make the tracked scene file a function of a GITIGNORED
//      terrain bake -- a fresh clone would author different bytes.
//   2. There is no terrain physics body to sample at authoring time anyway. The
//      editor add path uses the deserialization constructor, which never calls
//      LoadCombinedPhysicsGeometry, so an authoring-time raycast would MISS.
// So the heights are MEASURED ONCE, off the live baked heightfield, by
// ZM_DawnmereNpcGroundTruth_Test (Tests/ZM_AutoTests_NpcTalk.cpp), and frozen
// here. That test is also the ORACLE that re-checks them: it casts a real
// downward ray at each anchor's XZ and reds if any compiled row has drifted from
// the surface the capsule actually rests on.
//
// ★ WHAT RE-MEASURES THEM. Regenerate the Dawnmere heightmap (change a terrain
// recipe, a seed, or the flatten radii in ZM_TerrainAuthoring.cpp) and these SEVEN
// numbers are stale. Run ZM_DawnmereNpcGroundTruth_Test, read the six
// `name=... measured=...` pairs it logs at INFO on every run, paste them into the
// block below, rebuild, and re-author Dawnmere from a windowed tools boot.
//
// ★ THE TOWN-CENTRE ANCHOR IS NOT IN THIS BLOCK, AND MOVING IT COSTS A SECOND
// TRACKED ASSET. fZM_DAWNMERE_TOWN_CENTER_FEET_Y feeds Source/Nav/ZM_NavEval.cpp's
// nav grid, and ZM_BakeDawnmereNavmeshStep runs on every windowed tools boot, so
// changing it re-bakes the committed Dawnmere.znavmesh. W5 itself did NOT move it.
// ZM-D-173 did, by 4.8 mm, because the terrain under it genuinely moved and this
// file's whole premise is that authored heights are measured rather than assumed;
// the navmesh consequence is accepted and recorded rather than avoided by leaving
// a known-stale number in place.
// ============================================================================

// ==== W5 MEASURED HEIGHTS ====
// ONE VALUE PER LINE, entity named in the comment. Each is the `measured=` figure
// ZM_DawnmereNpcGroundTruth_Test logged for that entity: a REAL downward raycast
// at that anchor's XZ against the baked Dawnmere terrain body, taken headless on
// the Null backend. They are OBSERVED, never derived -- re-measure by running
// that test and reading its `MEASURED FEET Y` line, never by arithmetic off the
// town-centre anchor.
//
// ★ RE-MEASURED 2026-08-07 (ZM-D-186) -- AND THE HEIGHTMAP NEVER MOVED. ZM-D-182
// took terrain COLLISION from 8 m to 4 m quads. The heightmap is untouched by that;
// what changes is the triangle a downward ray lands on, so every anchor NOT sitting
// on a shared grid vertex is now interpolated across a different, finer quad.
// ZM-D-182 re-measured the Home block below (which is why every Home row reads
// tableError 0.00000) and did NOT re-measure this one, so these seven rows stayed on
// their 8 m-era values for five days.
//
// ★ THE PATTERN IS ARITHMETIC, NOT NOISE, AND IT PREDICTS ITSELF. 4 divides both 512
// and 480, so the TOWN CENTRE is a shared vertex of both meshes and reads bit-identical
// -- which is the only reason this re-measure does not re-bake the committed navmesh.
// Rows at mid-quad XZ moved most (warden (478, 498), both axes on a half-quad: 98.8 mm,
// two thirds of the oracle's tolerance); rows that landed on a 4 m vertex but a former
// 8 m mid-quad moved by the interpolation error being removed (wanderer (540, 476):
// 22.3 mm). Nothing here was BROKEN -- every row was inside the 0.15 m tolerance -- but
// the property this block maintains is that the constants EQUAL the measurement, not
// that they are close enough to pass.
//
// ★ WHAT THIS COSTS A FUTURE CHANGE. A collision-density change re-measures BOTH
// blocks in this file, never one. They are separate tables with separate oracles and
// nothing ties them together, which is exactly how ZM-D-182 moved one and left the
// other; the containment is that both oracles run in the local batch.
//
// ★ HISTORICAL, 2026-07-31 (ZM-D-173): moving the Home terrain PAD regenerated the
// whole Dawnmere heightmap, because the recipe's hydraulic erosion pass is region-wide
// rather than pad-local. Every row moved by 0.4 mm to 9.7 mm. That is the OTHER way
// these constants go stale -- a real heightmap change -- and it moves the town centre
// and the navmesh with it, unlike the collision-density case above.
//
// ★ WHAT THE ORIGINAL W5 MEASUREMENT FOUND, and it is worse than "an inference
// plus one measured value": the ONE shared town-centre height then in use left
// the WARDEN 1.368 m and the CARETAKER 1.095 m above their own ground. The
// figures in that sentence are HISTORICAL -- they are about the collapsed table
// W5 replaced, not about the rows below. What is still LIVE is the property they
// established, and it is re-measured on every run: the terrain spread under the
// six-NPC roster is 1.85737 m (min 24.52141 warden, max 26.37878 wanderer, as of
// ZM-D-186). Dawnmere's town square is not remotely flat, and one shared height
// cannot describe it.
inline constexpr float fZM_DAWNMERE_FEET_Y_VILLAGER     = 25.68112f;   // Npc_Villager       (512, 490)
inline constexpr float fZM_DAWNMERE_FEET_Y_CLERK        = 25.53937f;   // Npc_TradePostClerk (526, 498)
inline constexpr float fZM_DAWNMERE_FEET_Y_CARETAKER    = 24.89114f;   // Npc_Caretaker      (498, 498)
inline constexpr float fZM_DAWNMERE_FEET_Y_WARDEN       = 24.52141f;   // Npc_Warden         (478, 498)   <-- the lowest ground under the roster
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDERER     = 26.37878f;   // Npc_Wanderer       (540, 476)   <-- the highest; also wander waypoint 0
inline constexpr float fZM_DAWNMERE_FEET_Y_RIVAL_VESPER = 25.85455f;   // Npc_RivalVesper    (490, 524)
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDER_WP1   = 26.19232f;   // WanderWaypoint1    (540, 484)
// ==== END W5 MEASURED HEIGHTS ====
//
// Waypoint 0 deliberately has NO constant of its own: it stands at the SAME XZ as
// the wanderer's spawn anchor, so it reuses fZM_DAWNMERE_FEET_Y_WANDERER by
// construction. Two independently editable numbers for one point on the
// heightfield is a drift the table should not be able to express.

// Every authored Dawnmere NPC, in anchor-table order. Row index == id, pinned by
// the boot units. These ordinals are NOT persisted anywhere -- they exist only to
// index the compiled table -- but keep them grouped by role for readability.
enum ZM_DAWNMERE_NPC_ID : u_int
{
	ZM_DAWNMERE_NPC_VILLAGER,           // the walk-up target (ZM_NpcTalk_Test)
	ZM_DAWNMERE_NPC_TRADE_POST_CLERK,
	ZM_DAWNMERE_NPC_CARETAKER,
	ZM_DAWNMERE_NPC_WARDEN,             // the story-gated lane warden
	ZM_DAWNMERE_NPC_WANDERER,           // the ONE dynamic body: a two-point patrol
	ZM_DAWNMERE_NPC_RIVAL_VESPER,       // the authored trainer

	ZM_DAWNMERE_NPC_COUNT
};

// One authored ground anchor: WHERE a body stands and WHAT the terrain surface is
// under it. The entity NAME is carried so a test can resolve the authored entity
// from the committed scene bytes without re-spelling a name-string that a grep for
// the id would never find.
//
// m_fFeetY is the TERRAIN SURFACE, never a body centre. Callers that need a centre
// go through ZM_DawnmereNpcCentreY / ZM_DawnmereWandererSpawnY so the capsule
// arithmetic exists in exactly one place.
struct ZM_DawnmereNpcAnchor
{
	const char* m_szEntityName;
	float       m_fX;
	float       m_fZ;
	float       m_fFeetY;
};

// EVERY accessor below is TOTAL, in the ZM_GetTrainerData house style: no argument
// value, however degenerate, is UB, and none of them calls Zenith_Assert.
// Zenith_Assert calls Zenith_DebugBreak() in EVERY configuration and the whole unit
// suite runs at boot, so an assert on an argument a unit deliberately feeds does
// not fail one test -- it ends the boot unit run and takes the whole gate down.
// An id that indicates mis-authored data is diagnosed with a NON-FATAL
// Zenith_Error instead, exactly as ZM_GetTrainerData does.

// True iff uNpc names a row of the anchor table.
bool ZM_IsDawnmereNpcId(u_int uNpc);

// The anchor row for uNpc. An out-of-range id returns a DEFINED sentinel row
// standing on the town centre and named "UNKNOWN" -- distinguishable from every
// roster row by BOTH its name and its XZ, so a caller that skipped the id check
// cannot mistake it for content.
const ZM_DawnmereNpcAnchor& ZM_GetDawnmereNpcAnchor(u_int uNpc);

// The terrain surface under uNpc. Out of range -> the sentinel's height.
float ZM_DawnmereNpcFeetY(u_int uNpc);

// The authored BODY CENTRE for uNpc: its feet plus one capsule half-extent.
// TOTAL in the half-extent too: a non-finite or negative half-extent is treated as
// zero and the feet height is returned. Fail closed -- a NaN that reached a
// transform would poison a body, a model matrix and every distance derived from
// them, and no assertion downstream would name this function.
float ZM_DawnmereNpcCentreY(u_int uNpc, float fCapsuleHalfExtent);

// The RIVAL's authored SPAWN height: his resting centre plus ONE capsule
// half-extent of air -- the same treatment, for the same reason, that
// ZM_DawnmereWandererSpawnY has always given the wanderer.
//
// ★ WHY HE NEEDS IT (ZM-D-184). He used to be authored at his resting centre
// exactly (feet + one half-extent), i.e. with ~13 mm of clearance, and he fell
// through the world intermittently. MEASURED cause: the first two frames after a
// scene load take ~0.49 s, which the physics accumulator drained as ~29
// consecutive 1/60 s substeps; a body resting exactly on the surface free-falls
// through that burst before contact resolution catches it, and once the capsule's
// LOWER SPHERE CENTRE passes below the terrain's one-sided mesh the contact normal
// inverts and the solver expels it downward. He sank 0.61 m by frame 2 and never
// came back; the player, authored the same way, cleared it by ~2 cm on the same
// load -- which is why this looked like a one-character bug.
//
// The engine-side substep cap (Zenith_Physics::Update) is the ROOT fix and makes
// this margin unnecessary in that specific case. This clearance is kept anyway,
// because it removes the zero-margin authoring that made him the canary: an
// authored body should not depend on the solver catching it on the first tick.
// Belt AND braces, deliberately.
float ZM_DawnmereTrainerSpawnY(float fCapsuleHalfExtent);

// The wanderer's authored SPAWN height, which is its centre plus ONE EXTRA capsule
// half-extent of air. It was the FIRST dynamic body in Dawnmere -- rival Vesper
// became the second at S7 item 1 SC3 and needed the identical treatment for the
// identical reason (ZM-D-184, ZM_DawnmereTrainerSpawnY above) -- and the local
// terrain there is ~0.4 m above the town centre; spawning it at its resting centre
// puts the capsule inside the mesh, so it is authored clear of the surface and
// gravity settles it from the FRONT side. Named rather than open-coded at the call
// site precisely so that special case is greppable. Same totality rules: a
// degenerate half-extent returns the feet height.
float ZM_DawnmereWandererSpawnY(float fCapsuleHalfExtent);

// The wanderer's authored patrol endpoints. Their Y is serialized only as an
// AUTHORED REFERENCE -- ZM_StepWalker is explicitly XZ-only and the dynamic capsule
// owns Y -- but it is still derived from a measured feet height rather than from
// the town centre, so a reader cannot mistake it for a live target height.
u_int ZM_GetDawnmereWanderWaypointCount();

// TOTAL: an out-of-range index returns the same "UNKNOWN" town-centre sentinel row
// ZM_GetDawnmereNpcAnchor hands back.
const ZM_DawnmereNpcAnchor& ZM_GetDawnmereWanderWaypoint(u_int uIndex);

// ============================================================================
// ZM-D-173 -- THE HOME BLOCKOUT, ITS APPROACH, AND THE CAMERA CLEARANCE RULE
//
// WHY THIS BLOCK EXISTS. Before ZM-D-173 the Home shell, its two door jambs,
// its lintel, the door sensor, the FromHome spawn marker and the traversal
// test's two drive waypoints were EIGHT separate sets of literals spread across
// Zenithmon.cpp's ZENITH_TOOLS authoring block and three test TUs. Nothing could
// state a relationship between them, so "does the camera fit behind the player at
// the doorway?" was a sentence in a comment. Here it is arithmetic a boot unit
// runs.
//
// ★ THE CLEARANCE CONTRACT THE HOME PLACEMENT EXISTS TO SATISFY. The overworld
// camera keeps the yaw captured from the scene (fZM_DAWNMERE_AUTHORED_CAMERA_YAW)
// and resolves occlusion by raycasting pivot -> desired position. With that yaw:
//   player capsule centre = feet + 0.9                (the scale-derived extent)
//   pivot                 = centre + 0.6  = feet + 1.5 (ZM_FollowCamera::GetPivotHeight)
//   desired camera        = centre + 3.0 and 5.5 m BEHIND the authored forward,
//                           i.e. feet + 3.9 vertically  (GetCameraHeight/GetArmLength)
//   pivot -> camera       = sqrt(5.5^2 + 2.4^2) ~= 6.0008 m
// At every authoritative sample the CLAMPED arm must stay >= 50% of that, i.e.
// ~3.0004 m. Collision padding is a LONGITUDINAL 0.20 m subtraction off the hit
// distance -- NOT a widened collision volume -- so a solid hit before ~3.2004 m
// violates the rule. ZM_FollowCamera::ClampArmDistance is the authority; the
// tests call it rather than re-implementing it.
//
// ★ WHAT THE OLD PLACEMENT GOT WRONG. The shell used to sit at z 436..476 with
// its entrance on the +Z face, while the camera trails toward -Z. Standing at the
// doorway put the whole 16x40 m shell BEHIND the player, collapsing the arm to
// its 1.0 m floor. ZM-D-173 moves the complete shell +40 m in Z (z 476..516) and
// moves the terrain pad with it, so the entrance faces -Z and the camera trails
// into the open forecourt.
//
// ★ AND THE COVERAGE BOUNDARY, STATED PLAINLY. The clearance guard enforces the
// contract at the NAMED sample table in ZM_AutoTests_CameraClearance.cpp -- the
// critical routes and the actor-free interaction approaches. It does NOT prove
// every mathematically standable point in Dawnmere. A newly authored region must
// add ITS primary traversal paths, warp approaches and interaction approaches to
// that table as part of authoring it.
// ============================================================================

// The yaw Dawnmere's camera entity is authored with, and therefore the yaw
// ZM_FollowCamera captures and keeps for the whole scene. Every clearance figure
// above is derived at this heading, so a scene edit that rotates the camera
// invalidates the sample DIRECTIONS -- which is why the real-scene guard asserts
// the captured yaw equals this value before trusting any of its samples.
inline constexpr float fZM_DAWNMERE_AUTHORED_CAMERA_YAW = 0.0f;

// The XZ half-width the corridor clearances expand the Home shell by: the human
// capsule's RADIUS. A route that stays outside the expanded box cannot graze the
// real box with the real body.
//
// ★ IT IS THE BODY CONTRACT'S RADIUS, NOT A FUNCTION OF THE AUTHORED SCALE. The
// clearance figures below are stated against a 0.9 m capsule half-extent (0.4 m
// radius + 0.5 m half cylinder), and that body is now installed from the compiled
// contract in Source/World/ZM_HumanBody.h rather than derived from the transform
// -- which is what lets a human be authored at a uniform MODEL scale without the
// capsule collapsing into a sphere.
inline constexpr float fZM_DAWNMERE_PLAYER_RADIUS = fZM_HUMAN_BODY_CAPSULE_RADIUS;

// ---- Home XZ and scales (the parts that are NOT terrain-derived) -----------
// The shell, both door jambs, the lintel, the sensor, the spawn marker and both
// drive waypoints all share one X centreline.
inline constexpr float fZM_DAWNMERE_HOME_X = 384.0f;

// The PlayerHome interior is authoritative: its 16 x 12 m clear room plus
// 0.5 m perimeter walls has a 16.5 x 12.5 m outer envelope. This exterior is
// the deliberately rounded-up 17 x 13 m envelope, with a 4 m facade/roof mass.
// It occupies z 476..489. Keeping the -Z entrance at z=476 preserves the open
// forecourt, fixed-yaw camera direction, trigger, and return route established
// by ZM-D-173 while removing the former 16 x 6 x 40 m false depth.
inline constexpr float fZM_DAWNMERE_HOME_SHELL_Z       = 482.5f;
inline constexpr float fZM_DAWNMERE_HOME_SHELL_SCALE_X = 17.0f;
inline constexpr float fZM_DAWNMERE_HOME_SHELL_SCALE_Y = 4.0f;
inline constexpr float fZM_DAWNMERE_HOME_SHELL_SCALE_Z = 13.0f;

// The entrance decoration plane: both door jambs and the lintel stand on it, and
// it coincides with one of the shell's two Z faces.
// ★ THE ENTRANCE IS A FRAME, NOT A DOOR PANEL. Its 4.0 x 2.5 m opening is the
// exact PlayerHome interior aperture, flanked by two 0.5 x 2.5 x 0.5 m jambs
// and bridged by a 5.0 x 0.5 x 0.5 m lintel. Nothing swings and nothing closes:
// the warp is the sensor 2 m out, so the player is taken through before reaching
// the gap. This makes the separate exterior and interior portal read as one
// deliberately wide home entrance instead of two unrelated blockout scales.
inline constexpr float fZM_DAWNMERE_HOME_ENTRANCE_Z    = 476.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_LEFT_X   = 381.75f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_RIGHT_X  = 386.25f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_SCALE_X  = 0.5f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_SCALE_Y  = 2.5f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_SCALE_Z  = 0.5f;
inline constexpr float fZM_DAWNMERE_HOME_LINTEL_SCALE_X = 5.0f;
inline constexpr float fZM_DAWNMERE_HOME_LINTEL_SCALE_Y = 0.5f;
inline constexpr float fZM_DAWNMERE_HOME_LINTEL_SCALE_Z = 0.5f;

// The warp sensor, 2 m in FRONT of the solid entrance face so a returning
// player overlaps it before physical contact -- and so it is not COPLANAR
// with that face, which is where it used to sit. A sensor centred in the
// wall is both the thing the camera ray has to see past and a trigger whose
// "overlap before contact" property was an accident of its own box depth.
inline constexpr float fZM_DAWNMERE_HOME_TRIGGER_Z       = 474.0f;
inline constexpr float fZM_DAWNMERE_HOME_TRIGGER_SCALE_X = 4.0f;
inline constexpr float fZM_DAWNMERE_HOME_TRIGGER_SCALE_Y = 2.5f;
inline constexpr float fZM_DAWNMERE_HOME_TRIGGER_SCALE_Z = 2.0f;

// Where a player returning OUT of the house is placed (a FEET anchor), and the
// two waypoints ZM_PlayerHomeRoundTrip_Test drives through on the way IN.
// All three are SOUTH of the -Z entrance since ZM-D-173. Staging aligns with
// the doorway while the capsule is still clear of the solid entrance face;
// the target is a short +Z move from staging into the sensor. The claim that
// the town-centre -> staging leg never crosses the player-radius-expanded
// shell is a boot unit, not a comment: see
// ZM_Interaction/HomeApproachIsClearOfTheDriveCorridor.
inline constexpr float fZM_DAWNMERE_FROM_HOME_SPAWN_Z   = 468.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_STAGING_Z = 470.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_TARGET_Z  = 474.0f;

// ============================================================================
// THE MEASURED GROUND UNDER THE HOME PLACEMENT
//
// Same contract as the W5 block above and for the same two reasons: the committed
// Dawnmere.zscen bytes must be reproducible from COMPILED constants (not from a
// gitignored terrain bake), and there is no terrain physics body during authoring
// to sample anyway. So every height below is MEASURED ONCE off the live baked
// heightfield -- by ZM_DawnmereHomeGroundTruth_Test
// (Tests/ZM_AutoTests_CameraClearance.cpp), which logs every value at INFO on
// every run and REDS if a compiled row has drifted from the real surface.
//
// ★ WHAT RE-MEASURES THEM: regenerating the Dawnmere heightmap OR changing its
// collision density -- a terrain recipe/seed/flatten-radius change in
// ZM_TerrainAuthoring.cpp, or a physics divisor change in the terrain exporter.
// Moving the Home PAD is exactly such a change.
// ============================================================================

enum ZM_DAWNMERE_HOME_SAMPLE : u_int
{
	// The four shell-footprint corners. The shell's authored Y is derived from
	// the MINIMUM of these so no corner of the box can float off the ground.
	ZM_DAWNMERE_HOME_SAMPLE_SHELL_MINX_MINZ,
	ZM_DAWNMERE_HOME_SAMPLE_SHELL_MAXX_MINZ,
	ZM_DAWNMERE_HOME_SAMPLE_SHELL_MINX_MAXZ,
	ZM_DAWNMERE_HOME_SAMPLE_SHELL_MAXX_MAXZ,
	// The two door jambs stand on their OWN ground, independently, because the
	// entrance plane is 4 m wide and Dawnmere's square is not flat.
	ZM_DAWNMERE_HOME_SAMPLE_DOOR_LEFT,
	ZM_DAWNMERE_HOME_SAMPLE_DOOR_RIGHT,
	ZM_DAWNMERE_HOME_SAMPLE_TRIGGER,
	ZM_DAWNMERE_HOME_SAMPLE_SPAWN,
	ZM_DAWNMERE_HOME_SAMPLE_STAGING,
	ZM_DAWNMERE_HOME_SAMPLE_TOWN_CENTER,

	ZM_DAWNMERE_HOME_SAMPLE_COUNT
};

// One measured column: WHERE the probe went down and WHAT it found. Reuses
// ZM_DawnmereNpcAnchor's shape so both oracles read the same three fields.
u_int ZM_GetDawnmereHomeSampleCount();

// TOTAL: an out-of-range id returns the same "UNKNOWN" town-centre sentinel row
// ZM_GetDawnmereNpcAnchor hands back, and says so with a non-fatal Zenith_Error.
const ZM_DawnmereNpcAnchor& ZM_GetDawnmereHomeSample(u_int uSample);

// The measured surface at one sample. Out of range -> the sentinel's height.
float ZM_DawnmereHomeSampleFeetY(u_int uSample);

// ---- The authored blockout, centre + scale in one value --------------------
struct ZM_DawnmereBlockout
{
	Zenith_Maths::Vector3 m_xCenter;
	Zenith_Maths::Vector3 m_xScale;

	// Half-extents, i.e. what the AABB arithmetic actually wants.
	Zenith_Maths::Vector3 HalfExtent() const { return m_xScale * 0.5f; }
	Zenith_Maths::Vector3 Min() const { return m_xCenter - HalfExtent(); }
	Zenith_Maths::Vector3 Max() const { return m_xCenter + HalfExtent(); }
};

// Each Y below is DERIVED from the measured table by a FIXED formula, spelled
// once here so no consumer can re-derive it differently:
//   shell   = min(the four corner grounds) + 2.0 - 0.05   (half-height, minus a
//             deliberate 0.05 m embed so no visible gap opens under the box)
//   doors   = their OWN measured ground + 1.25            (half of the 2.5 m jamb)
//   lintel  = max(the two door grounds)   + 2.75          (clears the 2.5 m opening)
//   trigger = its measured ground         + 1.25          (half of the 2.5 m sensor)
ZM_DawnmereBlockout ZM_GetDawnmereHomeShell();
ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorLeft();
ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorRight();
ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorLintel();
ZM_DawnmereBlockout ZM_GetDawnmereHomeDoorTrigger();

// The FromHome spawn marker's FEET position -- the measured terrain surface, not
// a body centre. Callers that need a centre add a capsule half-extent, exactly as
// ZM_DawnmereNpcCentreY does for the roster.
Zenith_Maths::Vector3 ZM_GetDawnmereFromHomeSpawnFeet();

// The traversal route's two drive waypoints. Y is deliberately 0: DriveTowardXZ is
// XZ-only and the dynamic capsule owns Y, so a height here would be a lie.
Zenith_Maths::Vector3 ZM_GetDawnmereHomeDoorStagingXZ();
Zenith_Maths::Vector3 ZM_GetDawnmereHomeDoorTargetXZ();

// ============================================================================
// S8 SC-D -- THE DAWNMERE LAB SITE: GROUND TRUTH + EXTERIOR ARITHMETIC
//
// ★ THIS BLOCK PLACES NOTHING, AND THAT IS THE WHOLE SCOPE OF SC-D. Not one
// entity in the committed Dawnmere.zscen reads it. It is the NUMBERS S8 SC-E
// authors the lab exterior from -- the shell, its two door jambs, the lintel,
// the door sensor and the FromLab arrival marker -- plus the tests that lock
// those numbers BEFORE any of it is built. SC-E must author from these
// accessors and re-spell none of them, in the one sub-commit that re-writes
// Dawnmere.zscen (which cannot be done headless -- ZM-D-190).
//
// ★ THE SITE IS ALREADY RESERVED, AND NOTHING HERE MAY MOVE IT.
// ZM_TerrainAuthoring.cpp's Dawnmere recipe carries a "Lab" PAD at (640, 552)
// (48 m flatten radius, 38 m dirt radius, 4 dirt passes), a "Lab" PATH from the
// plaza through (574, 526) to that pad centre, and the "FromLab" LANDMARK at
// (640, 520). Editing ANY of them regenerates the WHOLE Dawnmere heightmap --
// the recipe's hydraulic erosion pass is region-wide rather than pad-local, as
// ZM-D-173 found the expensive way -- which would re-measure every table in this
// file and re-bake the committed navmesh. So every coordinate below is DERIVED
// TO FIT INSIDE the reserved site, and the binding is a boot unit rather than a
// comment: ZM_Interaction/LabPlacement_SitsInsideTheReservedPadAndOnItsLandmark
// reads the recipe by name and asserts both containments.
//
// ★ THE ENTRANCE FACES -Z, FOR THE ZM-D-173 REASON AND NO OTHER. ZM_FollowCamera
// keeps the yaw the scene authored (fZM_DAWNMERE_AUTHORED_CAMERA_YAW = 0) and
// trails the player toward -Z, so the building must lie on the +Z side of every
// point a player stands at while approaching it. Put the entrance on the +Z face
// and the whole 21 x 17 m shell sits BEHIND the player at the doorway, which is
// exactly the defect ZM-D-173 moved the Home 40 m to fix.
// ============================================================================

// The lab's X centreline: the reserved pad's centre X. The shell, both jambs,
// the lintel, the sensor, the arrival marker and both drive waypoints share it.
inline constexpr float fZM_DAWNMERE_LAB_X = 640.0f;

// ...and the reserved pad's centre Z. This one is NOT a placement -- nothing is
// authored here -- it is the site's reference column, sampled by the ground table
// below and used by the containment unit. Both this and the X above MIRROR the
// terrain recipe rather than reading it (this header is pure and must not depend
// on ZM_TerrainAuthoring), which is exactly why
// LabPlacement_SitsInsideTheReservedPadAndOnItsLandmark asserts the mirror
// against the recipe's "Lab" pad instead of trusting it.
inline constexpr float fZM_DAWNMERE_LAB_PAD_CENTER_Z = 552.0f;

// ---- The shell envelope ----------------------------------------------------
// The ProfLab interior is authoritative: its 20 x 16 m hall plus 0.5 m perimeter
// walls has a 20.5 x 16.5 m OUTER envelope (fZM_PROFLAB_HALF_WIDTH/HALF_DEPTH
// and fZM_PROFLAB_WALL_THICKNESS). This exterior is that envelope rounded up to
// a clean 21 x 17 m blockout -- 0.5 m of deliberately cosmetic spare on each
// PLAN axis, which is what LabExterior_EnvelopeAndEntranceMatchProfLabContract
// holds to under one metre.
//
// ★ HEIGHT IS NOT AN ENVELOPE-WRAP AXIS, AND 5.5 IS NOT A ROUNDING OF 3.5. That
// "under one metre of spare" clause is PLAN-ONLY, for the reason this paragraph
// spends. The facade has to enclose the 3.5 m entrance FRAME (a 3.0 m jamb under
// a 0.5 m lintel) that stands on the DOOR ground, while the box is seated on the
// LOWEST of its four corner grounds minus the 0.05 m embed -- and the reserved
// site is not flat, so those two grounds are not the same number:
//     roofline  = min(the four corner grounds) + SCALE_Y - 0.05
//     frame top = max(the two door grounds)    + 3.0 + 0.5
//   => SCALE_Y  > 3.5 + 0.05 + (door ground - lowest corner ground)
// i.e. the height this constant needs is FIXED FRAME + EMBED + the site's own
// relief, and only the first two terms are design values.
//
// ★ AND THE FROZEN MEASUREMENTS PUT THAT RELIEF AT 1.4048 m, WHICH IS WHY 4.5
// WAS NOT ENOUGH. The graded pad still falls ~1.5 m across a 21 x 17 m footprint
// (corner MinX/MinZ 25.88701 down to MinX/MaxZ 24.37600) while the higher door
// column reads 25.78080, so:
//     lowest corner 24.37600 | higher door ground 25.78080 | relief 1.40480
//     minimum viable SCALE_Y = 3.5 + 0.05 + 1.40480 = 4.95480
// At 4.5 the roofline sat at 28.8260 with the lintel top at 29.2808 -- the frame
// stood 0.4548 m PROUD of the roof, and
// LabExterior_EnvelopeAndEntranceMatchProfLabContract went RED on exactly that
// clause the moment the ground table was frozen, exactly as its note predicted.
//
// ★ 5.5 IS CHOSEN FOR MARGIN, NOT FOR THE FLOOR. It puts the roofline at 29.8260,
// i.e. 0.5452 m of clearance over the lintel, where the bare minimum would leave
// none. These grounds are MEASUREMENTS: the largest single-row movement this file
// has ever recorded from a re-measure is 98.8 mm (ZM-D-186, the collision-density
// change), and 0.5452 m survives roughly five times that with the corner row and
// the door row moving in OPPOSITE directions at once. For scale, the shipped Home
// carries a 4.0 m facade over a 3.0 m frame on 0.626 m of relief -- 0.324 m of
// margin. This site's relief is 2.2x the Home's, so its facade is taller in
// proportion rather than by taste.
// ★ IF THIS CLAUSE EVER REDS AGAIN, RAISE THIS CONSTANT, DO NOT WEAKEN THE CLAUSE
// -- a red there means the reserved site's relief genuinely pushes the lintel
// through the roof, which is a modelling defect a player would look straight at.
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_X = 21.0f;
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_Y = 5.5f;
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_Z = 17.0f;

// ★ THE ENTRANCE PLANE IS THE ONE NUMBER IN THIS BLOCK THAT WAS DERIVED FROM A
// CAMERA CLEARANCE RATHER THAN FROM A BUILDING, AND IT MUST NOT BE ROUNDED
// "TIDILY" BACK TO 528. The authored Lab dirt path runs (574, 526) -> (640, 552)
// and therefore crosses the shell's X band (629.5 .. 650.5) at
// (629.5, 547.8636) -- a point a player walks, with the camera trailing 5.5 m
// toward -Z straight into the building's BACK face. The contract
// (fCC_MIN_ARM_FRACTION, the ZM-D-173 block above) needs at least
//   0.5 * |pivot->camera| + collisionPadding   of ray, i.e. 2.9333 m of
// HORIZONTAL gap at this heading. An entrance at 528 puts the back face at 545
// and leaves 2.864 m -- a VIOLATION, and one that would only appear once SC-E
// authored the shell, in a test that never mentions the lab. At 527 the back
// face is 544 and the gap is 3.8636 m, i.e. 0.93 m of margin.
// ZM_Interaction/LabDirtPath_ClearsTheShellByTheShippedCameraClamp runs that
// arithmetic through ZM_FollowCamera::ClampArmDistance itself, so moving the
// entrance, the shell depth or the terrain path reds a UNIT rather than shipping
// a camera that clips into a wall.
inline constexpr float fZM_DAWNMERE_LAB_ENTRANCE_Z = 527.0f;

// ...and the shell centre follows the entrance, never the other way round. Both
// terms are dyadic, so this sum is exact in every configuration (the ZM-D-183
// rule applied to a value that will land in a COMMITTED scene file).
inline constexpr float fZM_DAWNMERE_LAB_SHELL_Z =
	fZM_DAWNMERE_LAB_ENTRANCE_Z + fZM_DAWNMERE_LAB_SHELL_SCALE_Z * 0.5f;

// ---- The entrance frame ----------------------------------------------------
// ★ A FRAME, NOT A DOOR PANEL, and its opening is the ProfLab aperture EXACTLY:
// 6.0 x 3.0 m, two 0.5 x 3.0 x 0.5 m jambs flanking it and a 7.0 x 0.5 x 0.5 m
// lintel bridging their outer faces. Nothing swings and nothing closes -- the
// warp is the sensor 2 m out, so the player is taken through before reaching the
// gap. The jamb X values are DERIVED from fZM_PROFLAB_APERTURE_HALF_WIDTH so the
// interior and the exterior cannot part; the Home's equivalents are literals
// bound by a test, which is the older and weaker of the two patterns.
inline constexpr float fZM_DAWNMERE_LAB_DOOR_SCALE_X = 0.5f;
inline constexpr float fZM_DAWNMERE_LAB_DOOR_SCALE_Y = fZM_PROFLAB_APERTURE_HEIGHT;
inline constexpr float fZM_DAWNMERE_LAB_DOOR_SCALE_Z = 0.5f;

// Each jamb's INNER face lands exactly on the aperture edge, so the clear
// opening between them is 2 * fZM_PROFLAB_APERTURE_HALF_WIDTH to the bit.
inline constexpr float fZM_DAWNMERE_LAB_DOOR_LEFT_X = fZM_DAWNMERE_LAB_X
	- (fZM_PROFLAB_APERTURE_HALF_WIDTH + fZM_DAWNMERE_LAB_DOOR_SCALE_X * 0.5f);
inline constexpr float fZM_DAWNMERE_LAB_DOOR_RIGHT_X = fZM_DAWNMERE_LAB_X
	+ (fZM_PROFLAB_APERTURE_HALF_WIDTH + fZM_DAWNMERE_LAB_DOOR_SCALE_X * 0.5f);

// The lintel spans both jambs' OUTER faces: the aperture plus one jamb width
// either side. Same shape as the Home's 5.0 m lintel over its 4.0 m opening.
inline constexpr float fZM_DAWNMERE_LAB_LINTEL_SCALE_X =
	fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f + fZM_DAWNMERE_LAB_DOOR_SCALE_X * 2.0f;
inline constexpr float fZM_DAWNMERE_LAB_LINTEL_SCALE_Y = 0.5f;
inline constexpr float fZM_DAWNMERE_LAB_LINTEL_SCALE_Z = 0.5f;

// ---- The warp sensor -------------------------------------------------------
// 2 m in FRONT of the solid entrance face, exactly like the Home's, so an
// approaching player overlaps it before physical contact and so it is not
// COPLANAR with that face. Its 2 m depth therefore leaves 1 m of air between its
// far face and the wall. It covers exactly the visible opening in X and Y.
inline constexpr float fZM_DAWNMERE_LAB_TRIGGER_Z =
	fZM_DAWNMERE_LAB_ENTRANCE_Z - 2.0f;
inline constexpr float fZM_DAWNMERE_LAB_TRIGGER_SCALE_X =
	fZM_PROFLAB_APERTURE_HALF_WIDTH * 2.0f;
inline constexpr float fZM_DAWNMERE_LAB_TRIGGER_SCALE_Y = fZM_PROFLAB_APERTURE_HEIGHT;
inline constexpr float fZM_DAWNMERE_LAB_TRIGGER_SCALE_Z = 2.0f;

// ---- The arrival marker and the two drive waypoints ------------------------
// ★ THE SPAWN Z IS NOT A CHOICE. It is the reserved "FromLab" landmark's Z, so
// the marker SC-E authors stands where the terrain recipe already says the
// return spawn is -- the same relationship the Home keeps with its "FromHome"
// landmark at (384, 468). A boot unit asserts the equality against the recipe;
// this constant exists so no consumer has to reach into terrain authoring for it.
// It leaves a 7 m forecourt between the arrival point and the entrance face.
inline constexpr float fZM_DAWNMERE_FROM_LAB_SPAWN_Z = 520.0f;

// Staging aligns with the doorway while the capsule is still well clear of the
// solid entrance face; the target is a short +Z step from staging INTO the
// sensor, and is therefore the sensor's own centre (the Home does the same).
inline constexpr float fZM_DAWNMERE_LAB_DOOR_STAGING_Z = 522.0f;
inline constexpr float fZM_DAWNMERE_LAB_DOOR_TARGET_Z = fZM_DAWNMERE_LAB_TRIGGER_Z;

// ============================================================================
// THE MEASURED GROUND UNDER THE LAB PLACEMENT
//
// Same contract as the ZM-D-173 Home block above, for the same two hard reasons:
// the committed Dawnmere.zscen bytes must be reproducible from COMPILED
// constants (never from a GITIGNORED terrain bake), and there is no terrain
// physics body during authoring to sample anyway -- the editor add path uses the
// deserialization constructor, which never calls LoadCombinedPhysicsGeometry, so
// an authoring-time raycast would MISS. So every height is MEASURED ONCE off the
// live baked heightfield, by ZM_DawnmereLabGroundTruth_Test
// (Tests/ZM_AutoTests_CameraClearance.cpp), and frozen here.
//
// ★★ THE TABLE BELOW SHIPS AS AN EXPLICIT, INVALID PLACEHOLDER. Every row still
// reads fZM_DAWNMERE_LAB_GROUND_UNMEASURED, which is NOT a height and is nowhere
// near one. THE SLICE IS NOT COMPLETE UNTIL THOSE TEN ROWS HOLD REAL
// MEASUREMENTS, and the following boot units are RED until they do, deliberately
// and by design:
//     ZM_Interaction/LabGroundSamples_AreTenMeasurementsInsideTheGradedBand
//     ZM_Interaction/LabGroundSamples_NoRowSilentlyRepeatsAnother
// TO FREEZE THEM: run ZM_DawnmereLabGroundTruth_Test on a WINDOWED tools boot
// with a warm Dawnmere terrain bake (a cold tree needs one bake boot first, and
// a Null boot authors nothing -- ZM-D-190), read the ten `PASTE` lines it logs at
// INFO on EVERY run, replace each row's fZM_DAWNMERE_LAB_GROUND_UNMEASURED with
// the printed literal, rebuild, and re-run the oracle until it is green.
//
// ★ WHAT RE-MEASURES THEM AFTERWARDS: regenerating the Dawnmere heightmap OR
// changing its collision density -- a terrain recipe/seed/flatten-radius change
// in ZM_TerrainAuthoring.cpp, or a physics divisor change in the terrain
// exporter. ZM-D-182 and ZM-D-186 are both worked examples, and note that they
// moved the two EXISTING tables in this file independently: there are now THREE
// tables with three oracles and nothing ties them together, so a density change
// re-measures ALL THREE or leaves a silent staleness behind.
// ============================================================================

// The initialiser every row of the lab ground table still carries. Chosen to be
// unmistakable rather than merely wrong: it is FINITE (so no accessor can hand a
// NaN to a transform while the table is unfrozen), a million metres below any
// Dawnmere surface, and it appears verbatim in the failure message of the two
// boot units above.
inline constexpr float fZM_DAWNMERE_LAB_GROUND_UNMEASURED = -1000000.0f;

// Every measured lab column, in table order. Row index == id.
//
// ★ WHY EACH ROW EXISTS, i.e. what SC-E cannot author without it:
//   the four SHELL corners  -- the shell's authored Y is derived from the
//       MINIMUM of them, so no corner of a 21 x 17 m box hangs in the air. Four
//       samples, not one centre: the pad is graded, not flat.
//   DOOR_LEFT / DOOR_RIGHT  -- each jamb stands on its OWN ground. The opening
//       is 6 m wide and the two ends of it are not at the same height.
//   TRIGGER                 -- the warp sensor's own column; its centre is that
//       ground plus half its height, so a mis-shared height would bury the
//       sensor or float it above a walking capsule.
//   SPAWN                   -- the FromLab arrival marker's FEET. This is the
//       row a mis-paste hurts most: ZM_GameStateManager::CalculateSpawnCenter
//       adds the capsule half-extent to it at warp time, so a wrong value warps
//       the player in embedded in the ground or falling out of the air.
//   STAGING                 -- the drive waypoint a traversal test aims at.
//   PAD_CENTER              -- the reserved pad's own centre, which is also the
//       Lab dirt path's endpoint. It measures nothing SC-E authors; it is the
//       site's reference height, and having it in the SAME run is what lets the
//       band clause say "this table came from the graded pad" at all.
enum ZM_DAWNMERE_LAB_SAMPLE : u_int
{
	ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MINZ,
	ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MINZ,
	ZM_DAWNMERE_LAB_SAMPLE_SHELL_MINX_MAXZ,
	ZM_DAWNMERE_LAB_SAMPLE_SHELL_MAXX_MAXZ,
	ZM_DAWNMERE_LAB_SAMPLE_DOOR_LEFT,
	ZM_DAWNMERE_LAB_SAMPLE_DOOR_RIGHT,
	ZM_DAWNMERE_LAB_SAMPLE_TRIGGER,
	ZM_DAWNMERE_LAB_SAMPLE_SPAWN,
	ZM_DAWNMERE_LAB_SAMPLE_STAGING,
	ZM_DAWNMERE_LAB_SAMPLE_PAD_CENTER,

	ZM_DAWNMERE_LAB_SAMPLE_COUNT
};

// ★ THE LAB GETS ITS OWN ENUM AND ITS OWN ARRAY, AND APPENDING THESE ROWS TO THE
// HOME TABLE WOULD HAVE BEEN A SILENT TRUNCATION. The ground-truth probe in
// Tests/ZM_AutoTests_CameraClearance.cpp holds a FIXED 16-slot probe array whose
// static_assert only checks the HOME count, and every loop over it is bounded by
// `u < uCount && u < SLOTS` -- so ten extra rows in ZM_DAWNMERE_HOME_SAMPLE would
// have been measured for the first six and dropped for the rest, with every test
// still green. Separate enum, separate array, separate count, separate slot
// bound, separate static_assert.
u_int ZM_GetDawnmereLabSampleCount();

// TOTAL: an out-of-range id returns the same "UNKNOWN" town-centre sentinel row
// ZM_GetDawnmereNpcAnchor hands back, and says so with a non-fatal Zenith_Error.
const ZM_DawnmereNpcAnchor& ZM_GetDawnmereLabSample(u_int uSample);

// The measured surface at one lab sample. Out of range -> the sentinel's height.
float ZM_DawnmereLabSampleFeetY(u_int uSample);

// ---- The authored blockouts SC-E will write --------------------------------
// Each Y below is DERIVED from the measured table by the SAME FIXED formulas the
// Home block uses, spelled once here so no consumer can re-derive them
// differently, and checked by
// ZM_Interaction/LabBlockoutY_FollowsTheFixedDerivationFromTheMeasuredTable:
//   shell   = min(the four corner grounds) + 2.75 - 0.05  (half-height, minus a
//             deliberate 0.05 m embed so no visible gap opens under the box)
//   doors   = their OWN measured ground + 1.5             (half of the 3.0 m jamb)
//   lintel  = max(the two door grounds)   + 3.25          (clears the 3.0 m opening)
//   trigger = its measured ground         + 1.5           (half of the 3.0 m sensor)
ZM_DawnmereBlockout ZM_GetDawnmereLabShell();
ZM_DawnmereBlockout ZM_GetDawnmereLabDoorLeft();
ZM_DawnmereBlockout ZM_GetDawnmereLabDoorRight();
ZM_DawnmereBlockout ZM_GetDawnmereLabDoorLintel();
ZM_DawnmereBlockout ZM_GetDawnmereLabDoorTrigger();

// The FromLab spawn marker's FEET position -- the measured terrain surface, not
// a body centre. Callers that need a centre add a capsule half-extent, exactly as
// ZM_GameStateManager::CalculateSpawnCenter does at warp time.
Zenith_Maths::Vector3 ZM_GetDawnmereFromLabSpawnFeet();

// The lab route's two drive waypoints. Y is deliberately 0, for the same reason
// the Home's are: DriveTowardXZ is XZ-only and the dynamic capsule owns Y, so a
// height here would be a lie.
Zenith_Maths::Vector3 ZM_GetDawnmereLabDoorStagingXZ();
Zenith_Maths::Vector3 ZM_GetDawnmereLabDoorTargetXZ();

// ============================================================================
// R1-2 STEP 1 -- THE ROUTE 1 ARRIVAL SEAM: THE MEASURED GROUND, MEASURED FIRST
//
// ★ THIS BLOCK PLACES NOTHING, AND THAT IS THE WHOLE POINT OF LANDING IT ALONE.
// Slice R1-2 will author a "FromRoute1" arrival marker into Dawnmere so the
// Route 1 return leg has somewhere to land. Dawnmere.zscen is a COMMITTED asset
// that has already drifted twice (ZM-D-179, ZM-D-183), so the order of
// operations is deliberate: MEASURE the column while the committed scene is
// still untouched, and only then author into it. Authoring first would move a
// tracked file before anyone knew the column was usable, with "revert a
// committed asset" as the only recovery. The measurement does not depend on the
// marker, so it goes first.
//
// ★ THE COLUMN IS NOT A CHOICE. It is the Dawnmere terrain recipe's "FromRoute1"
// LANDMARK, at (512, 864) -- the same relationship the Home keeps with its
// "FromHome" landmark (384, 468) and the lab with "FromLab" (640, 520). The two
// constants below MIRROR that landmark rather than reading it (this header is
// pure and must not depend on ZM_TerrainAuthoring), which is exactly why the
// mirror is a boot unit rather than a comment:
// ZM_Interaction/RouteSeamGround_StandsOnTheFromRoute1LandmarkAndIsMeasured.
// ★ THE NAME "FromRoute1" IS NOT UNIQUE ACROSS RECIPES -- Thornacre carries one
// too, at (512, 112) -- so that lookup reads the DAWNMERE recipe by name and
// nothing here may be checked against "the FromRoute1 landmark" in the abstract.
//
// ★ WHY THIS COLUMN IS EXPECTED TO BE LEVELLED GROUND, WRITTEN DOWN BEFORE THE
// MEASUREMENT SO THE MEASUREMENT CAN CONTRADICT IT. The recipe's "Route" path
// runs (512, 928) -> (500, 760) -> (524, 620) -> (512, 512) with an 18 m FLATTEN
// radius and a 10 m dirt radius. (512, 864) lies 4.56 m from the first of those
// segments (closest point (507.451, 864.320)), i.e. deep inside both corridors,
// so it is graded lane rather than natural hillside. The "RouteGate" PAD at
// (512, 896) does NOT contribute: its 30 m flatten radius falls 2 m short of the
// 32 m to this column, and its dirt radius is 0. No Dawnmere landform reaches it
// either -- the nearest, (224, 650) with a 180 m radius, is 359 m away.
//
// So the EXPECTATION is a surface in the ~25.6 .. 26.5 m band every other
// measured Dawnmere column on graded ground reads: 1.5 to 2.6 m ABOVE the
// recipe's 24 m flatten target, because the region-wide hydraulic erosion pass
// deposits on top of the grade (the town centre reads 25.99, the ten Home
// columns 25.59 - 26.54, the ten lab columns 24.32 - 26.04).
//
// ★★ AND IF THE REAL MEASUREMENT LANDS FAR OUTSIDE THAT, IT IS A GENUINE FINDING
// FOR R1-3, NOT A BAND TO WIDEN. A seam column that is not on the graded lane
// means the Route corridor does not actually reach the arrival point, which is a
// terrain-recipe question -- and a terrain-recipe change regenerates the WHOLE
// Dawnmere heightmap and re-measures every table in this file. Absorbing it by
// loosening the boot unit below would hide exactly the thing this slice was
// sequenced to find out.
//
// ★★ THE ROW SHIPS AS THE FILE'S UNMEASURED SENTINEL, exactly as the lab table
// above shipped before its 2026-08-14 freeze. It is NOT a height and is nowhere
// near one, and the boot unit below is RED BY DESIGN until it is replaced.
// TO FREEZE IT: run ZM_DawnmereRouteSeamGroundTruth_Test
// (Tests/ZM_AutoTests_CameraClearance.cpp) on a boot with a warm Dawnmere
// terrain bake, read the single `paste=` literal it logs at
// LOG_CATEGORY_UNITTEST on EVERY run, replace the row's
// fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED initialiser in
// Source/World/ZM_DawnmerePlacement.cpp with it, rebuild, and re-run the oracle
// until it is green.
//
// ★ WHAT RE-MEASURES IT AFTERWARDS: the same two things that re-measure the
// other two tables in this file -- regenerating the Dawnmere heightmap (a
// recipe/seed/flatten-radius change in ZM_TerrainAuthoring.cpp) or changing its
// collision density (a physics divisor change in the terrain exporter). There
// are now THREE ground tables here with three separate oracles and nothing ties
// them together, so such a change re-measures ALL THREE or leaves a silent
// staleness behind. ZM-D-182 and ZM-D-186 are both worked examples of exactly
// that going wrong.
// ============================================================================

// The arrival column, MIRRORING the Dawnmere recipe's "FromRoute1" landmark.
inline constexpr float fZM_DAWNMERE_FROM_ROUTE1_X = 512.0f;
inline constexpr float fZM_DAWNMERE_FROM_ROUTE1_Z = 864.0f;

// The initialiser the route-seam row still carries. Deliberately the SAME value
// the lab table shipped on -- aliased rather than re-typed, because a sentinel
// spelled twice is a sentinel two tables can disagree about. It is FINITE (so no
// accessor can hand a NaN to a transform while the table is unfrozen), a million
// metres below any Dawnmere surface, and it appears verbatim in the failure
// message of the boot unit that is red until the freeze lands.
inline constexpr float fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED =
	fZM_DAWNMERE_LAB_GROUND_UNMEASURED;

// ★ ITS OWN ENUM, ITS OWN ARRAY, ITS OWN COUNT AND ITS OWN PROBE SLOT BOUND, for
// the reason spelled out on the lab enum above: appending a row to another
// table's enum measures it up to that table's fixed slot bound and silently
// DROPS it beyond, with every test still green.
enum ZM_DAWNMERE_ROUTE_SEAM_SAMPLE : u_int
{
	// The FromRoute1 arrival column itself. This is the row a mis-paste hurts
	// most for the same reason the lab's SPAWN row is: a return-leg warp adds the
	// capsule half-extent to it, so a wrong value puts the arriving player
	// embedded in the ground or falling out of the air.
	ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1,

	ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_COUNT
};

u_int ZM_GetDawnmereRouteSeamSampleCount();

// TOTAL: an out-of-range id returns the same "UNKNOWN" town-centre sentinel row
// ZM_GetDawnmereNpcAnchor hands back, and says so with a non-fatal Zenith_Error.
const ZM_DawnmereNpcAnchor& ZM_GetDawnmereRouteSeamSample(u_int uSample);

// The measured surface at one route-seam sample. Out of range -> the sentinel's
// height.
float ZM_DawnmereRouteSeamSampleFeetY(u_int uSample);

// ============================================================================
// R1-2 STEP 3 -- THE AUTHORED "FromRoute1" ARRIVAL MARKER
//
// ★ WHY THE NAME LIVES IN THIS HEADER AND NOT AT THE AUTHORING SITE. Both sides
// of the seam have to read the SAME symbol: Zenithmon.cpp's Dawnmere block
// authors the entity, and Tests/ZM_Tests_CommittedSceneBytes.cpp needles the
// committed blob for it. A literal at either site would move with a rename on
// the other and pin nothing -- the whole reason szZM_DAWNMERE_FROM_LAB_SPAWN_
// ENTITY_NAME sits in ZM_ProfLabPlacement.h rather than in the authoring.
//
// It is spelled to match the two SHIPPED Dawnmere arrival markers, FromHomeSpawn
// and FromLabSpawn, rather than to a new convention -- a reader who knows those
// recognises this one.
//
// ★★ AND THAT SPELLING KNOWINGLY WALKS INTO THE FromLab / FromLabSpawn TRAP.
// "FromRoute1" is the INBOUND tag this marker carries and is a strict PREFIX of
// this name, so a committed-bytes needle on the tag counts every occurrence of
// the NAME as well. The consequence is a rule, not an inconvenience: the tag
// clause must assert STRICTLY MORE tag hits than name hits, never a plain
// equality -- an equality would be satisfied by a marker that was created and
// never tagged, which is precisely the WAITING_FOR_SPAWN stall (ZM-D-200) the
// needle exists to catch. R1-1's newer Route1/Thornacre markers dodge the trap
// by being named ...Arrival; this one keeps the shipped Dawnmere vocabulary
// because the strictly-more clause is already written, understood and running
// on FromLabSpawn one entity away.
//
// ★ IT MUST ALSO EQUAL THE MEASURED ROW'S OWN LABEL. The route-seam row in
// ZM_DawnmerePlacement.cpp is named "FromRoute1Spawn" and
// ZM_DawnmereRouteSeamGroundTruth_Test prints that label on the `paste=` line a
// re-measure is copied from, so a divergence would leave the oracle naming an
// entity the committed scene does not contain. That equality is a TEST CLAUSE
// rather than a comment -- see ZM_CommittedSceneBytes/
// DawnmereCarriesTheRoute1ArrivalMarkerAndItsInboundTag -- because this header
// cannot reach that row's name at compile time and a second spelling nothing
// compares is exactly the drift this file's banner forbids.
inline constexpr const char* szZM_DAWNMERE_FROM_ROUTE1_SPAWN_ENTITY_NAME =
	"FromRoute1Spawn";

// The FromRoute1 arrival marker's FEET position -- the MEASURED terrain surface
// at (fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z), never a body
// centre. ZM_GameStateManager::CalculateSpawnCenter adds the capsule half-extent
// at warp time, so authoring a centre here would drop every player arriving off
// Route 1 in from half a body up. Same shape, and the same reasoning, as the
// shipped ZM_GetDawnmereFromHomeSpawnFeet / ZM_GetDawnmereFromLabSpawnFeet.
//
// ★ INLINE HERE RATHER THAN IN THE .cpp, unlike its two siblings, purely because
// it needs nothing the .cpp owns: the XZ are this header's own constants and the
// height comes through the public route-seam accessor. Nothing is re-spelled.
inline Zenith_Maths::Vector3 ZM_GetDawnmereFromRoute1SpawnFeet()
{
	return Zenith_Maths::Vector3(fZM_DAWNMERE_FROM_ROUTE1_X,
		ZM_DawnmereRouteSeamSampleFeetY(ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1),
		fZM_DAWNMERE_FROM_ROUTE1_Z);
}

// ============================================================================
// R1-3 -- DAWNMERE'S NORTH SEAM GATE: the fourth and last of the four triggers
//
// ★ THE OTHER THREE LIVE IN THEIR OWN SCENES' HEADERS. Route 1's two gates are
// spelled in Source/World/ZM_Route1Placement.h and Thornacre's return gate in
// Source/World/ZM_ThornacrePlacement.h, each beside the arrival marker it stands
// beyond. This block is the Dawnmere-side twin of those, and it is a SEAM gate
// rather than a DOOR: the shipped HomeDoorTrigger / LabDoorTrigger take the
// player into an interior a few metres away, whereas this one hands them off to
// another region. That is why it is named for its scene and its compass bearing
// (the R1-1 convention -- Route1SouthGate, Route1NorthGate, ThornacreSouthGate)
// rather than for a building.
//
// ★★ ALL FOUR TRIGGERS LAND IN ONE COMMIT, AND THAT IS A SAFETY RULING, NOT
// TIDINESS. ZM_GameStateManager::IsWarpDestinationValid consults ONLY the
// compiled ZM_WorldSpec tag list -- never the destination scene -- so a gate
// shipped before its far-side arrival marker exists is ACCEPTED, and the warp
// machine then sits in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN behind a fully
// opaque fade until that barrier's frame budget expires (ZM-D-200), then errors
// out into a region the player never arrived in. R1-2 landed every marker and
// zero triggers for exactly that reason; this slice closes the other half.
// ============================================================================

// ★ THE NAME LIVES HERE, NOT AT THE AUTHORING SITE, for the reason
// szZM_DAWNMERE_FROM_ROUTE1_SPAWN_ENTITY_NAME states above: Zenithmon.cpp
// authors the entity and Tests/ZM_Tests_CommittedSceneBytes.cpp needles the
// committed blob for it, and a literal at either site would move with a rename
// on the other and pin nothing.
//
// ★ IT DELIBERATELY CONTAINS NO SPAWN TAG AND IS NOT A SUBSTRING OF -- NOR DOES
// IT CONTAIN -- ANY OTHER AUTHORED DAWNMERE NAME. The gate's OUTBOUND tag is
// "FromDawnmere" (resolved below, never spelled), which is a string the
// committed Dawnmere.zscen has never carried; keeping the two disjoint is what
// lets the committed-bytes clauses be plain `== 1` equalities instead of the
// strictly-more form FromLabSpawn and FromRoute1Spawn both need.
inline constexpr const char* szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME =
	"DawnmereNorthGate";

// ---- The outbound edge, RESOLVED from the compiled world table --------------
//
// ★ WHY THESE ARE FUNCTIONS AND NOT `= 20` AND `= "FromDawnmere"`. The build
// index and the spawn tag are the TABLE's property (Source/Data/ZM_WorldSpec.cpp,
// row ZM_SCENE_DAWNMERE); a second spelling here would be an inventory nothing
// reconciles. Same shape as ZM_GetProfLabExitTargetBuildIndex /
// ZM_GetProfLabExitSpawnTag and as Route 1's and Thornacre's gate resolvers.
//
// ★ AND THE WALK IS BY TARGET, NOT BY INDEX 0. Dawnmere carries THREE edges --
// Route 1, PlayerHome and ProfLab -- so m_pxConnections[0] would be a magic
// index that silently hands back a doorway the day the table is reordered.

// The answer when the compiled table carries no Dawnmere -> Route1 edge at all.
// Deliberately a build index no scene can hold rather than a plausible one, so a
// caller that skips the resolution check authors an obviously dead warp instead
// of a subtly wrong one.
inline constexpr u_int uZM_DAWNMERE_NORTH_GATE_TARGET_UNRESOLVED = 0xFFFFFFFFu;

// TOTAL, in the house style of every accessor in this file: a table with no such
// edge yields nullptr rather than UB, and the three accessors below turn that
// into their own stated sentinels. NOTHING HERE ASSERTS -- Zenith_Assert calls
// Zenith_DebugBreak() in EVERY configuration and the whole unit suite runs at
// boot, so an assert on an argument a unit deliberately feeds does not fail one
// test, it ends the run. ZM_GetWorldSpec ITSELF asserts fatally out of range,
// which is why every call to it below is range-guarded first.
inline const ZM_SceneConnection* ZM_GetDawnmereNorthGateConnection()
{
	const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
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

inline ZM_SCENE_ID ZM_GetDawnmereNorthGateTargetScene()
{
	const ZM_SceneConnection* pxEdge = ZM_GetDawnmereNorthGateConnection();
	return pxEdge != nullptr ? pxEdge->m_eTarget : ZM_SCENE_NONE;
}

// ★ THE RANGE GUARD IS NOT DEFENSIVE DECORATION -- see the totality note above.
inline u_int ZM_GetDawnmereNorthGateTargetBuildIndex()
{
	const ZM_SCENE_ID eTarget = ZM_GetDawnmereNorthGateTargetScene();
	return eTarget < ZM_SCENE_COUNT
		? ZM_GetWorldSpec(eTarget).m_uBuildIndex
		: uZM_DAWNMERE_NORTH_GATE_TARGET_UNRESOLVED;
}

// The tag this gate asks ROUTE 1 for -- and therefore ALSO the tag Route 1's
// south arrival marker must already carry (it does; R1-2 authored it from the
// same row). ONE spelling for both sides of the seam is the whole point.
//
// TOTAL: "" on a miss, NEVER nullptr -- a caller may index [0] without checking,
// and ZM_WarpTrigger::Configure rejects an empty tag, so the authoring assertion
// fires instead of a silently dead sensor shipping.
inline const char* ZM_GetDawnmereNorthGateSpawnTag()
{
	const ZM_SceneConnection* pxEdge = ZM_GetDawnmereNorthGateConnection();
	return pxEdge != nullptr && pxEdge->m_szSpawnTag != nullptr
		? pxEdge->m_szSpawnTag
		: "";
}

// ---- The gate's column ------------------------------------------------------
//
// 12 m NORTH of the FromRoute1 arrival marker at (512, 864), i.e. between the
// arriving player and the northern edge of the region -- the same relationship,
// at the same 12 m separation, that Route 1's two gates and Thornacre's return
// gate keep with their own markers. The sensor is therefore the last thing a
// player walking north crosses, and an ARRIVING player is never already inside
// it (see the clearance floor below).
//
// ★ BOTH CONTAINMENTS ARE ARITHMETIC, NOT TASTE, and both are stated here so a
// later reader can re-check them rather than trust them. Against the Dawnmere
// terrain recipe (Source/World/ZM_TerrainAuthoring.cpp):
//   * the "RouteGate" PAD is at (512, 896) with a 30 m FLATTEN radius; this
//     column is 20 m from its centre, i.e. inside it -- unlike the arrival
//     column 12 m south, which at 32 m falls just outside;
//   * the "Route" PATH runs (512, 928) -> (500, 760) -> ... with an 18 m flatten
//     radius; this column lies ~3.7 m from that first segment.
// So the ground under this gate is graded lane inside a graded pad, which is the
// condition under which a flatten dab drives ground TO the recipe target.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_X = fZM_DAWNMERE_FROM_ROUTE1_X;
inline constexpr float fZM_DAWNMERE_NORTH_GATE_Z =
	fZM_DAWNMERE_FROM_ROUTE1_Z + 12.0f;

// ---- The gate volume --------------------------------------------------------
//
// The three scales match Route 1's and Thornacre's gate boxes, and they are
// declared INDEPENDENTLY here rather than read out of ZM_Route1Placement.h.
// Dawnmere's header must not depend on another region's placement -- Route 1's
// own banner refuses the mirror-image include for the same reason -- and the
// numbers are re-derived below rather than copied on faith:
//
// (1) X SPAN. The Route corridor lays a 10 m dirt radius (a 20 m visible lane)
//     inside an 18 m flatten radius, and the RouteGate pad is 30 m across the
//     radius here. 48 m centred on the lane leaves no gap a player can slip
//     through without leaving the walked corridor entirely.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_SCALE_X = 48.0f;

// (2) Y SPAN -- taller than fZM_HUMAN_BODY_HEIGHT (1.8), so the sensor cannot be
//     stepped or jumped over.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_SCALE_Y = 4.0f;

// (3) DEPTH. ZM_WarpTrigger fires from OnCollisionEnter -- a body-vs-body
//     contact sampled at the physics tick -- so the box has to be several ticks
//     deep rather than a plane a running capsule steps over between two of them.
//     Six metres is ~26 ticks at a 4 m/s walk and 60 Hz, plus a body radius of
//     contact window either side.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_SCALE_Z = 6.0f;

// ★★ THE NEAR FACE MUST CLEAR THE ARRIVING BODY, OR THE SEAM IS AN INFINITE
// LOOP. A player warps IN onto FromRoute1Spawn; if the arriving capsule
// overlapped this sensor it would fire on the first contact tick and send them
// straight back to Route 1, whose south gate would send them straight here
// again, forever, with no input accepted in between. The floor is stated on the
// near FACE rather than on the centre because an arriving player does not stand
// still: they walk off the marker immediately, and the eight-way MOVE drive
// makes the natural walk a 45-degree diagonal that SPENDS DEPTH AS FAST AS IT
// SPENDS WIDTH (Q-2026-08-15-001, the defect that shipped in ProfLab with every
// arrival clause green). The clearance this placement actually holds is 9 m --
// the marker sits 12 m from the gate centre and the box reaches 3 m back toward
// it -- identical to both Route 1 gates.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_ARRIVAL_CLEARANCE_MIN = 4.0f;

// ★ THE TWO FLOORS ABOVE ARE CHECKED HERE, AT COMPILE TIME, RATHER THAN LEFT TO A
// UNIT. Route 1's and Thornacre's equivalents are read by boot units in their own
// placement suites; this gate's live in the header because every term is a dyadic
// constexpr float and a static_assert cannot be skipped, forgotten or left in
// another file's slice. An unread constant is a decoration, and this file's own
// rule against those is the reason these two lines exist.
static_assert(
	fZM_DAWNMERE_NORTH_GATE_Z - fZM_DAWNMERE_NORTH_GATE_SCALE_Z * 0.5f
		- fZM_DAWNMERE_FROM_ROUTE1_Z
			>= fZM_DAWNMERE_NORTH_GATE_ARRIVAL_CLEARANCE_MIN,
	"the north gate's near face does not clear the FromRoute1 arrival marker by "
	"fZM_DAWNMERE_NORTH_GATE_ARRIVAL_CLEARANCE_MIN -- an arriving player would be "
	"warped straight back out, and Route 1's south gate would return them here, "
	"forever, with no input accepted between the two");
static_assert(fZM_DAWNMERE_NORTH_GATE_SCALE_Y > fZM_HUMAN_BODY_HEIGHT,
	"the north gate is not taller than a person, so the sensor can be stepped over");

// ★★ THE GATE'S CENTRE Y IS DERIVED FROM THE **ARRIVAL** COLUMN, AND THAT IS A
// KNOWN DEVIATION FROM THE ONE-COLUMN-PER-ANCHOR RULE THE ROUTE 1 AND THORNACRE
// HEADERS BOTH STATE. Write down why, because the rule is right and this is a
// scoped exception rather than a disagreement with it:
//
//   * Route 1 and Thornacre each measure their gate column separately, and their
//     frozen tables show the ground genuinely moves over 12 m -- Thornacre's two
//     columns differ by 0.254 m, Route 1's south pair by 0.475 m and its north
//     pair by 0.962 m. So a dedicated column IS the correct long-term answer
//     here too.
//   * What R1-3 cannot do is PRODUCE one. A new row in the route-seam table has
//     to be a real downward raycast against the baked Dawnmere heightfield; an
//     unmeasured row would ship as a placeholder that
//     ZM_DawnmereRouteSeamGroundTruth_Test reds on (0.15 m tolerance) and that
//     no source-only change can close. The table is also pinned at exactly ONE
//     row by ZM_Interaction/RouteSeamGround_StandsOnTheFromRoute1LandmarkAndIsMeasured,
//     which is outside this slice's file list.
//   * What the deviation COSTS is bounded and small: a 4 m-tall box seated on a
//     neighbouring column is at worst ~1 m out of plumb against a 1.8 m body, so
//     the sensor still spans the walked capsule from below its feet to well over
//     its head. It is a cosmetic seating error, not a sensor that can be missed.
//
// ★ THE OWED FOLLOW-UP, NAMED SO IT IS NOT LOST: measure (512, 876) against a
// warm Dawnmere bake, give it its own route-seam row, and re-point the
// derivation below at that row. That change re-authors Dawnmere.zscen (this Y
// lands in the committed bytes), so it belongs with a slice that is already
// doing a windowed re-author.
inline float ZM_GetDawnmereNorthGateCentreY()
{
	return ZM_DawnmereRouteSeamSampleFeetY(
			ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1)
		+ fZM_DAWNMERE_NORTH_GATE_SCALE_Y * 0.5f;
}

// The authored sensor, in the same centre+scale vocabulary as every other
// Dawnmere blockout.
inline ZM_DawnmereBlockout ZM_GetDawnmereNorthGate()
{
	return {
		Zenith_Maths::Vector3(
			fZM_DAWNMERE_NORTH_GATE_X,
			ZM_GetDawnmereNorthGateCentreY(),
			fZM_DAWNMERE_NORTH_GATE_Z),
		Zenith_Maths::Vector3(
			fZM_DAWNMERE_NORTH_GATE_SCALE_X,
			fZM_DAWNMERE_NORTH_GATE_SCALE_Y,
			fZM_DAWNMERE_NORTH_GATE_SCALE_Z) };
}
