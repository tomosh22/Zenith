#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3 / Quat / AngleAxis
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE human body contract

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
