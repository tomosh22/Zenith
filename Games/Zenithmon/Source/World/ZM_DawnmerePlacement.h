#pragma once

#include "Maths/Zenith_Maths.h"   // Vector3 / Quat / AngleAxis

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
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_FEET_Y = 25.98577f;
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
float ZM_DawnmereVesperYaw();

// The same facing as a quaternion, built with AngleAxis about +Y. This is the
// EXACT rotation the authoring writes into the scene, so a test may compare an
// authored transform against it directly.
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
// ★ THE TOWN-CENTRE ANCHOR ITSELF IS NOT IN THIS BLOCK AND MUST NOT MOVE.
// fZM_DAWNMERE_TOWN_CENTER_FEET_Y feeds Source/Nav/ZM_NavEval.cpp's nav grid, and
// ZM_BakeDawnmereNavmeshStep runs on every windowed tools boot -- changing it
// re-bakes the committed Dawnmere.znavmesh, a SECOND tracked asset. W5 adds
// per-NPC constants; it does not move the anchor.
// ============================================================================

// ==== W5 MEASURED HEIGHTS ====
// ONE VALUE PER LINE, entity named in the comment. Each is the `measured=` figure
// ZM_DawnmereNpcGroundTruth_Test logged for that entity: a REAL downward raycast
// at that anchor's XZ against the baked Dawnmere terrain body, taken headless on
// the Null backend on 2026-07-29. They are OBSERVED, never derived -- re-measure
// by running that test and reading its `MEASURED FEET Y` line, never by arithmetic
// off the town-centre anchor.
//
// ★ WHAT THE MEASUREMENT ACTUALLY FOUND, and it is worse than "an inference plus
// one measured value": the shared 25.98577 left the WARDEN 1.368 m and the
// CARETAKER 1.095 m above their own ground, with a live terrain spread of
// 1.782 m under the six-NPC roster. Dawnmere's town square is not remotely flat.
inline constexpr float fZM_DAWNMERE_FEET_Y_VILLAGER     = 25.66591f;   // Npc_Villager       (512, 490)   -0.320 m vs the old shared value
inline constexpr float fZM_DAWNMERE_FEET_Y_CLERK        = 25.52359f;   // Npc_TradePostClerk (526, 498)   -0.462 m
inline constexpr float fZM_DAWNMERE_FEET_Y_CARETAKER    = 24.89095f;   // Npc_Caretaker      (498, 498)   -1.095 m
inline constexpr float fZM_DAWNMERE_FEET_Y_WARDEN       = 24.61798f;   // Npc_Warden         (478, 498)   -1.368 m  <-- the worst
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDERER     = 26.40014f;   // Npc_Wanderer       (540, 476)   +0.414 m  <-- also wander waypoint 0
inline constexpr float fZM_DAWNMERE_FEET_Y_RIVAL_VESPER = 25.86723f;   // Npc_RivalVesper    (490, 524)   -0.119 m
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDER_WP1   = 26.20189f;   // WanderWaypoint1    (540, 484)   +0.216 m
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

// The wanderer's authored SPAWN height, which is its centre plus ONE EXTRA capsule
// half-extent of air. It is the only DYNAMIC body in Dawnmere, and the local
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
