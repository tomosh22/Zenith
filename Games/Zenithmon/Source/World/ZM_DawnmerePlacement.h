#pragma once

// ★★ MOVING ANYTHING IN THIS FILE? READ
// `Games/Zenithmon/Docs/MapLayoutPlaybook.md` FIRST -- particularly its
// "what re-derives what" table. Moving one anchor here routinely invalidates a
// measured ground column, a frozen quaternion, a keep-out disc, a tree clump and
// two tracked assets, and nothing warns you about the ones you miss.
//

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
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_X      = 120.0f;
// ★ MEASURED, never the recipe's nominal 24.0 target -- and 1.36 m below it,
// which is what a partially-graded square looks like. See the W5 block below.
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_FEET_Y = 25.05666f;
inline constexpr float fZM_DAWNMERE_TOWN_CENTER_Z      = 60.0f;

// ---- Rival Vesper (S7 item 4) ----------------------------------------------
//
// DERIVED, NOT EYEBALLED, AND v8 HAD TO REPLACE THE DERIVATION RATHER THAN
// RESCALE IT. Through v7 he stood on the ray from the TownCenter spawn through
// the midpoint of the two flank NPCs -- "the widest gate out of the plaza core".
// ★★ THAT RAY IS NOW THE HOME DRIVE CORRIDOR. Compacting the town (ZM-D-218)
// brought the plaza to within 45 m of the Home, and the line from the spawn
// through the west flank gate and the line from the spawn to the Home doorway
// converged: the v7 derivation, re-run on the v8 anchors, lands 1.45 m from a leg
// ZM_PlayerHomeRoundTrip_Test drives BLIND, and puts an armed trainer's 8 m sight
// cone across it. A derivation that was sound at one town scale can become the
// worst possible answer at another, and rescaling it would have shipped that.
//
// ★★ THE v8 DERIVATION IS A FIXED POINT OF THE TEST'S DRIVER, AND FINDING THAT
// OUT COST THE WHOLE OF ZM-D-218's DEBUGGING BUDGET. He stands DUE WEST of the
// town-centre spawn, on the same Z, 40 m out:
//
//     (fZM_DAWNMERE_TOWN_CENTER_X - 40, fZM_DAWNMERE_TOWN_CENTER_Z)
//
// WHY DUE WEST AND NOT SOMEWHERE PRETTIER. The walk-up in
// ZM_AutoTests_RivalVesper.cpp is driven by DriveTowardXZ, which is CAMERA-RELATIVE
// and QUANTISED TO EIGHT DIRECTIONS -- it holds W/A/S/D, never a steering angle --
// while the camera swings to follow the player's own heading. The player therefore
// does NOT walk the straight line this facing is derived from: he walks a pursuit
// curve that cuts the corner, and the LATERAL offset that opens up grows with the
// length of the walk.
//
// That offset is what the sight cone actually measures. The first v8 placement put
// him at (80, 88), a 48.8 m diagonal, and the player arrived 4.44 m to one side --
// which is a 31-degree bearing error at the 8 m sight range, against a cone that
// admits 30. MEASURED, closing in:
//
//     gap 24.0 m  coneDot 0.974      gap 10.3 m  coneDot 0.853  <-- already out
//     gap 12.0 m  coneDot 0.888      gap  5.0 m  coneDot 0.787
//     gap  8.5 m  coneDot 0.853      gap  0.8 m  coneDot 0.543
//                                    (the cone admits >= 0.86603)
//
// The dot DECAYS as he closes, because the player is approaching from the side he
// drifted to. So the rival was correctly placed, exactly facing, armed, WATCHING,
// with the player driven to 0.077 m of him -- and permanently blind. Every clause
// the test already printed was green; three plausible hypotheses (props on the
// sight line, the approach being too long, unflattened ground under a dynamic
// capsule) were each expensive to falsify and none of them was it.
//
// ★ A DUE-WEST TARGET IS A FIXED POINT OF THAT DRIVER, so the curve never opens.
// With the camera resting on +Z the target is pure -X: fForwardAmount is 0, inside
// the dead zone, so only A is held and the motion is exactly -X. Once the player
// has turned and the camera has swung onto -X, the target is dead ahead: only W is
// held, and the motion is exactly -X again. BOTH camera orientations drive the
// same straight line, so the lateral offset stays at zero and the cone dot stays
// at 1. The 45-degree diagonals are fixed points for the same reason; the ordinary
// bearings in between are not.
//
// ★ THE APPROACH LENGTH IS A SECOND CONSTRAINT, and it is easy to miss. The
// whiteout clauses set a FLOOR on it (he must not be able to see the respawn --
// fRV_MIN_SPAWN_SEPARATION is 10 m) and ZM_RivalVesperAuthored_Test's walk-up sets
// a CEILING (the player is driven the whole way inside one phase deadline). v6 held
// 49.2 m and v7 46.7 m; 40 m sits comfortably inside both bounds.
//
// He is on the 4 m physics lattice by construction, which every measured anchor in
// this file needs -- see ZM-D-186, where mid-quad rows moved 98.8 mm under a
// collision-density change and lattice rows moved zero.
//
// Separations, against an 8 m sight range and a 2.9 m interact reach:
//   caretaker (96,78)    24.08 m      warden   (100,56)  20.40 m
//   villager  (120,70)   41.23 m      clerk    (144,78)  66.48 m
//   wanderer  (140,56)   60.13 m
//   TownCenter spawn (120,60)         40.00 m   <-- the whiteout clearance
//   townCentre->homeStaging corridor  30.98 m   <-- driven BLIND by
//                                                   ZM_PlayerHomeRoundTrip_Test
//   townCentre->villager corridor     40.00 m
// The tightest blind corridor is 31.0 m, i.e. 3.9x his sight range -- which is what
// clause (d) of Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget now measures,
// having stopped pretending a Z gap describes a diagonal.
//
// ★ THE WARDEN STANDS 4 m OFF THE APPROACH LINE, at (100, 56), and that is fine
// but is the tightest thing about this placement. He is a body, not an occluder of
// the segment: the ray the sight probe casts runs along z = 60 and he is never on
// it. Move him and re-check, because a body ON that line would wedge the walk-up
// with a stall that names a distance rather than naming him.
//
// GROUND. He is 44.7 m from the Plaza pad centre (120, 80) against its 34 m
// flatten radius, so he is NOT on the square itself; his own SetHeight shelf in
// ZM_TerrainAuthoring.cpp is what levels him, and it exists for physics rather than
// for looks -- see its note there. Height reuses the one authored xPlayerCenter.y
// like every other NPC -- and ZM_RivalVesperAuthored_Test MEASURES the resulting
// |dy| against fZM_SIGHT_MAX_VERTICAL rather than trusting it.
//
// ★ GDD DEVIATION (Q-2026-07-24-002 Q-D). GameDesignDocument.md places rival
// battle 1 on "Route 1 (L5, scripted first battle)". Route 1 does not exist in S7;
// Dawnmere is the only authored scene. When a real Route 1 is authored, MOVE HIM
// THERE and re-derive every figure above from scratch -- none of them carries over
// -- exactly as the warden's block in Zenithmon.cpp instructs for the same reason.
inline constexpr float fZM_DAWNMERE_VESPER_X = 80.0f;
inline constexpr float fZM_DAWNMERE_VESPER_Z = 60.0f;

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
// these four numbers are stale. Vesper_FacingIsDerivedFromTheTownCentreBearing
// reds when that happens (it re-runs the atan2 derivation against them to a
// tolerance, which is the ONLY comparison that stays valid across configs). To
// re-freeze: read the `authored=` bits off the [ZM Authoring] line of a windowed
// DEBUG tools boot, paste them below, re-author Dawnmere, and commit the .zscen
// in the SAME commit.
//
// ★★ RE-FROZEN 2026-08-30 (ZM-D-218) FOR THE STREET LAYOUT. Vesper moved from
// (168, 168) to (80, 60) and the town centre from (192, 128) to (120, 60), so the
// v7 bits pointed 149.0 degrees down a bearing that no longer exists. He now
// stands DUE WEST of the town-centre spawn on the same Z, so the bearing is a
// right angle exactly: yaw = atan2f(40, 0) = pi/2 (90.000 deg), and the
// quaternion is angleAxis(yaw, +Y) = (w = cos(yaw/2), 0, sin(yaw/2), 0):
//     w = 0.70710677  0x3F3504F3
//     y = 0.70710677  0x3F3504F3
// Both components are cos(pi/4), so the two frozen words are IDENTICAL. That is a
// coincidence of this bearing, not an invariant -- do not "simplify" the pair
// into one constant.
//
// ★ THESE WERE COMPUTED OFFLINE IN float32 RATHER THAN READ OFF A BOOT, and that
// is now the better procedure rather than a shortcut. The authoring step is
// AddStep_SetTransformRotationQuat(ZM_DawnmereVesperFacing()) -- it writes THESE
// BITS verbatim and performs no math -- so the pre-save guard compares the frozen
// value against itself and can never disagree, whatever is pasted here. The only
// thing a boot could add is a second opportunity to mis-transcribe. What still
// has to hold is that the bits ENCODE the derived bearing, and that is exactly
// what the tolerance oracle above checks.
// ============================================================================
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_X_BITS = 0x00000000u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_Y_BITS = 0x3F3504F3u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_Z_BITS = 0x00000000u;
inline constexpr u_int uZM_DAWNMERE_VESPER_FACING_W_BITS = 0x3F3504F3u;

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

// ★★ RE-MEASURED 2026-08-30 (ZM-D-217). The v7 shrink moved every anchor in
// this table AND regenerated the heightmap under it, so all seven were stale in
// both coordinates at once. The live spread under the roster is 1.24270 m
// (min 22.75343 wanderer, max 23.99613 warden), which is the figure clause (d)
// of ZM_DawnmereNpcGroundTruth_Test polices.
//
// ★★ GETTING THERE TOOK TWO BAKES AND BOTH ARE WORTH KNOWING ABOUT.
//
// (1) THE FIRST v7 BAKE MEASURED 20.85 .. 23.98 -- a 3.15 m spread across a town
// square, with only the two rows that happened to sit inside a PATH's flatten
// radius reading anywhere near target. The cause is not the shrink:
// Zenith_TerrainEditor's FLATTEN kernel moves a texel
// `(target - h) * falloff * strength * 0.35` per dab and a PAD contributes
// exactly two dabs (the pre- and post-erosion passes), so a pad converges at most
// 58% of the way to its target even dead at its centre, and less than a third of
// the way at half its radius. **The Plaza pad has never flattened the town
// square.** v6 read within 0.85 m of target because its base noise happened to
// sit near 24 m there; v7's frequency change moved that noise field and exposed
// it. So do not "fix" a future spread by widening a pad -- widen or add a
// SetHeight SHELF (the v7 landform table), which is the kernel that actually
// assigns a height.
//
// (2) THE SECOND BAKE PUT THOSE SHELVES IN AT FULL STRENGTH AND EVERY ROW CAME
// BACK 24.00000 EXACTLY -- and clause (d) went red saying that W5's premise no
// longer held and six measured constants bought nothing. It was right, and the
// clause was left alone: the shelves are at partial strength now (SetHeight
// applies `min(1, falloff * strength * 2)`, so 0.42 removes 84% of the deviation
// rather than all of it), which is also the correct LOOK -- real graded ground
// keeps a few tenths of undulation and a dead-level 120 m disc does not.
//
// ★ THE LESSON GENERALISES: an anti-vacuity clause can be falsified by a change
// that is otherwise an improvement, and the answer is to ask which of the two is
// wrong rather than to reach for the tolerance.
//
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
// ★ THE PATTERN IS ARITHMETIC, NOT NOISE, AND IT PREDICTS ITSELF. 4 divides both 280
// and 160, so the TOWN CENTRE is a shared vertex of both meshes and reads bit-identical
// -- which is the only reason this re-measure does not re-bake the committed navmesh.
// (It divided 512 and 480 before the map shrank, and the shrink's X offset was
// chosen as -232 rather than -230 precisely to keep it: see the translate note in
// ZM_TerrainAuthoring.cpp. An offset off the 4 m lattice would have put every
// anchor in this file mid-quad and turned every row below into an interpolation.)
// Rows at mid-quad XZ moved most (warden (246, 178), both axes on a half-quad: 98.8 mm,
// two thirds of the oracle's tolerance); rows that landed on a 4 m vertex but a former
// 8 m mid-quad moved by the interpolation error being removed (wanderer (308, 156):
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
inline constexpr float fZM_DAWNMERE_FEET_Y_VILLAGER     = 24.37563f;   // Npc_Villager       (120,  70)
inline constexpr float fZM_DAWNMERE_FEET_Y_CLERK        = 23.82428f;   // Npc_TradePostClerk (144,  78)
inline constexpr float fZM_DAWNMERE_FEET_Y_CARETAKER    = 23.66973f;   // Npc_Caretaker      ( 96,  78)
inline constexpr float fZM_DAWNMERE_FEET_Y_WARDEN       = 24.41651f;   // Npc_Warden         (100,  56)
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDERER     = 25.22138f;   // Npc_Wanderer       (140,  56)   <-- also wander waypoint 0
inline constexpr float fZM_DAWNMERE_FEET_Y_RIVAL_VESPER = 23.86221f;   // Npc_RivalVesper    ( 80,  60)   <-- ON the rival shelf
inline constexpr float fZM_DAWNMERE_FEET_Y_WANDER_WP1   = 23.83243f;   // WanderWaypoint1    (140,  64)
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
// ★ WHAT THE OLD PLACEMENT GOT WRONG. The shell used to sit at z 116..156 with
// its entrance on the +Z face, while the camera trails toward -Z. Standing at the
// doorway put the whole 16x40 m shell BEHIND the player, collapsing the arm to
// its 1.0 m floor. ZM-D-173 moves the complete shell +40 m in Z (z 156..196) and
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
inline constexpr float fZM_DAWNMERE_HOME_X = 92.0f;

// The PlayerHome interior is authoritative: its 16 x 12 m clear room plus
// 0.5 m perimeter walls has a 16.5 x 12.5 m outer envelope. This exterior is
// the deliberately rounded-up 17 x 13 m envelope, with a 4 m facade/roof mass.
// It occupies z 100..113. Keeping the -Z entrance on the shell's own -Z face
// preserves the open forecourt, fixed-yaw camera direction, trigger, and return
// route established by ZM-D-173 while removing the former 16 x 6 x 40 m false
// depth.
//
// ★★ THE ENVELOPE HAS SURVIVED TWO SHRINKS UNTOUCHED AND MUST KEEP DOING SO:
// 17 x 13 is dictated by the PlayerHome INTERIOR, not by the town's scale. Only
// the whole Home group's ORIGIN moves -- by (-24, -28) at v7 and (-36, -28) at
// v8. It is also HALF OF WHY THE HOUSE AND THE LAB CANNOT STAND CLOSER THAN
// ~50 m: 8.5 m of this shell plus 10.5 m of the Lab's plus the route lane
// between them is arithmetic, not layout.
inline constexpr float fZM_DAWNMERE_HOME_SHELL_Z       = 106.5f;
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
inline constexpr float fZM_DAWNMERE_HOME_ENTRANCE_Z    = 100.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_LEFT_X   = 89.75f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_RIGHT_X  = 94.25f;
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
inline constexpr float fZM_DAWNMERE_HOME_TRIGGER_Z       = 98.0f;
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
inline constexpr float fZM_DAWNMERE_FROM_HOME_SPAWN_Z   = 92.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_STAGING_Z = 94.0f;
inline constexpr float fZM_DAWNMERE_HOME_DOOR_TARGET_Z  = 98.0f;

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
// ZM_TerrainAuthoring.cpp's Dawnmere recipe carries a "Lab" PAD at (408, 232)
// (48 m flatten radius, 38 m dirt radius, 4 dirt passes), a "Lab" PATH from the
// plaza through (342, 206) to that pad centre, and the "FromLab" LANDMARK at
// (408, 200). Editing ANY of them regenerates the WHOLE Dawnmere heightmap --
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
inline constexpr float fZM_DAWNMERE_LAB_X = 148.0f;

// ...and the reserved pad's centre Z. This one is NOT a placement -- nothing is
// authored here -- it is the site's reference column, sampled by the ground table
// below and used by the containment unit. Both this and the X above MIRROR the
// terrain recipe rather than reading it (this header is pure and must not depend
// on ZM_TerrainAuthoring), which is exactly why
// LabPlacement_SitsInsideTheReservedPadAndOnItsLandmark asserts the mirror
// against the recipe's "Lab" pad instead of trusting it.
inline constexpr float fZM_DAWNMERE_LAB_PAD_CENTER_Z = 108.0f;

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
// ★ THE v6 MEASUREMENTS PUT THAT RELIEF AT 1.4048 m, WHICH IS WHY 4.5 WAS NOT
// ENOUGH. The graded pad still fell ~1.5 m across the 21 x 17 m footprint
// (corner MinX/MinZ 25.88701 down to MinX/MaxZ 24.37600) while the higher door
// column read 25.78080, so:
//     lowest corner 24.37600 | higher door ground 25.78080 | relief 1.40480
//     minimum viable SCALE_Y = 3.5 + 0.05 + 1.40480 = 4.95480
// At 4.5 the roofline sat at 28.8260 with the lintel top at 29.2808 -- the frame
// stood 0.4548 m PROUD of the roof, and
// LabExterior_EnvelopeAndEntranceMatchProfLabContract went RED on exactly that
// clause the moment the ground table was frozen, exactly as its note predicted.
//
// ★★ v7 (ZM-D-217) TOOK THE RELIEF TO 0.73490 m AND 5.5 STAYS. The site is
// re-measured on a v7 bake with a SetHeight "Lab shelf" under it (see the W5
// block above for why a PAD never graded anything), and the east flank ridge's
// foot still tilts the pad: the two -Z corners read 24.515 / 24.791 against
// 24.040 / 24.244 on the +Z pair, and the higher door column reads 24.775. So
//     lowest corner 24.03970 | higher door ground 24.77460 | relief 0.73490
//     minimum viable SCALE_Y = 3.5 + 0.05 + 0.73490 = 4.28490
// and 5.5 puts the roofline at 29.4897 against a 28.2746 frame top -- 1.2151 m
// of clearance, where v6 held 0.5452 m. The constant is NOT cut to match the
// lower floor: the whole point of the arithmetic above is that the floor MOVES
// with the terrain, and 1.2151 m of headroom is what makes the next re-measure
// a re-measure rather than a re-model.
//
// ★ 5.5 IS THEREFORE STILL CHOSEN FOR MARGIN, NOT FOR THE FLOOR. For scale, the
// shipped Home carries a 4.0 m facade over a 3.0 m frame on 0.578 m of relief --
// 0.372 m of margin, where v6's 0.626 m of Home relief left it 0.324 m. This
// site's relief is 1.3x the Home's, so its facade is taller in proportion
// rather than by taste.
// ★ IF THIS CLAUSE EVER REDS AGAIN, RAISE THIS CONSTANT, DO NOT WEAKEN THE CLAUSE
// -- a red there means the reserved site's relief genuinely pushes the lintel
// through the roof, which is a modelling defect a player would look straight at.
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_X = 21.0f;
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_Y = 5.5f;
inline constexpr float fZM_DAWNMERE_LAB_SHELL_SCALE_Z = 17.0f;

// ★★ THE ENTRANCE PLANE IS NO LONGER DERIVED FROM A CAMERA CLEARANCE, AND THAT
// IS THE SINGLE CHANGE THAT LET THE LAB COME CLOSE TO THE HOUSE (ZM-D-218).
//
// THROUGH v7 THE LAB LANE RAN PAST THE BUILDING TO A PAD CENTRE BEHIND IT. That
// put a walked point in the shell's X band NORTH of it, with the camera trailing
// 5.5 m toward -Z straight into the building's BACK face, so the entrance plane
// had to be pushed far enough south that
//   0.5 * |pivot->camera| + collisionPadding = 2.9333 m
// of horizontal ray survived. The inversion is worth keeping written down:
//   entrance <= crossingZ - 2.9333 - SHELL_SCALE_Z.
// ★ AND ITS REAL COST WAS NOT THE ENTRANCE -- IT WAS THE WHOLE SITE. For that
// crossing to land north of the building the lane had to travel a long way in X
// while climbing in Z, which forced the lab far out along a diagonal from the
// plaza. That is why v7's lab sat 76 m from the town centre and 135 m from the
// player's front door, and why its pad had to be 48/40 -- the largest on the map
// -- to reach an arrival marker 36 m away on the far side of the building.
//
// v8's Lab lane ends at the FORECOURT, (148, 96), which is 8 m in FRONT of the
// door, exactly as the Home's always has. No authored walkway passes behind the
// building, the camera never has the shell between it and the player on any
// authored route, and the entrance plane is free to sit wherever the SITE wants
// it. 104 puts the shell at z 104..121 with its four corners and both jambs
// inside a 26/20 pad, and the two doors 56.1 m apart.
//
// ★ THE CLEARANCE UNIT WAS RE-DERIVED, NOT DELETED.
// ZM_Interaction/LabDirtPath_ClearsTheShellByTheShippedCameraClamp still runs the
// inversion through ZM_FollowCamera::ClampArmDistance itself and still refuses
// any walked point that has the shell within the required gap on its -Z side;
// what changed is that its old anti-vacuity clause ("the walkway must pass north
// of the shell") is now the WRONG shape, and has been replaced by one that says
// the lane leads to the door FROM THE FRONT. Read that test before moving the
// entrance, the shell depth or the terrain path.
inline constexpr float fZM_DAWNMERE_LAB_ENTRANCE_Z = 104.0f;

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
// landmark at (152, 148). A boot unit asserts the equality against the recipe;
// this constant exists so no consumer has to reach into terrain authoring for it.
// It leaves a 7 m forecourt between the arrival point and the entrance face.
inline constexpr float fZM_DAWNMERE_FROM_LAB_SPAWN_Z = 97.0f;

// Staging aligns with the doorway while the capsule is still well clear of the
// solid entrance face; the target is a short +Z step from staging INTO the
// sensor, and is therefore the sensor's own centre (the Home does the same).
inline constexpr float fZM_DAWNMERE_LAB_DOOR_STAGING_Z = 99.0f;
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
//       row a mis-paste hurts most: ZM_GameStateManager::CalculateSpawnPosition
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
// ZM_GameStateManager::CalculateSpawnPosition does at warp time.
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
// LANDMARK, at (192, 288) -- the same relationship the Home keeps with its
// "FromHome" landmark (128, 120) and the lab with "FromLab" (256, 164). The two
// constants below MIRROR that landmark rather than reading it (this header is
// pure and must not depend on ZM_TerrainAuthoring), which is exactly why the
// mirror is a boot unit rather than a comment:
// ZM_Interaction/RouteSeamGround_EachRowStandsOnItsOwnAnchorAndIsMeasured (the
// rename is ZM-65's -- see the R1-3 block below for the second row it added).
// ★ THE NAME "FromRoute1" IS NOT UNIQUE ACROSS RECIPES -- Thornacre carries one
// too, at (412, 96) IN THORNACRE'S OWN WORLD, which is a different terrain with a
// different origin and different extents -- so that lookup reads the DAWNMERE
// recipe by name and
// nothing here may be checked against "the FromRoute1 landmark" in the abstract.
//
// ★ WHY THIS COLUMN IS EXPECTED TO BE LEVELLED GROUND, WRITTEN DOWN BEFORE THE
// MEASUREMENT SO THE MEASUREMENT CAN CONTRADICT IT. The recipe's "Route" path
// runs (120, 208) -> (114, 170) -> (122, 124) -> (120, 80) with a 9 m FLATTEN
// radius and a 5 m dirt radius. (120, 144) lies 1.5 m from the second of those
// segments, i.e. deep inside both corridors, so it is graded lane rather than
// natural hillside. The "RouteGate" PAD at (120, 176) does NOT contribute: its
// 22 m flatten radius falls 10 m short of the 32 m to this column, and its dirt
// radius is 0. No Dawnmere landform reaches it either -- the nearest, the
// north-west ridge at (40, 218) with a 38 m radius, is 105 m away, so its foot
// stops 67 m short. (The v6 clause named a 90 m hill at (90, 545), and the v5 one a 180 m
// hill at (224, 650): the landforms are RE-AUTHORED at each shrink rather than
// translated, so every one of these figures is re-derived, never shifted.)
//
// So the EXPECTATION is a surface within a few tenths of the recipe's 24 m
// flatten target, which is the band every other measured Dawnmere column reads.
// ★ THE COMPARATIVE FIGURES THAT USED TO BE QUOTED HERE (town centre 24.10, ten
// Home columns 23.88-24.34, ten lab columns 24.08-25.58) WERE THE v6 MAP'S AND
// ARE DELETED RATHER THAN CARRIED FORWARD. v7 regenerated the heightmap under
// all 23 of them; re-state the band from the v7 run that re-freezes the tables.
//
// ★★ THAT BAND USED TO BE ~25.6 .. 26.5, AND WHY IT MOVED IS THE INTERESTING
// PART. On the 1024 m map the erosion disc was centred at the map's middle and
// the town sat off to one side of it, so most columns here were UNFLATTENED
// ground carrying the hydraulic pass's deposit -- ~+2 m on top of the grade. The
// shrink re-centred the erosion on the plaza and scaled its radius with the map,
// so the town is now inside the eroded region rather than beside it, and the
// deposit no longer piles up over it. Two roster columns now read slightly BELOW
// target (the clerk at 23.696, the villager at 23.765), which is the erosion
// CARVING rather than depositing -- a state the old map never produced here.
//
// ★★ AND IF THE REAL MEASUREMENT LANDS FAR OUTSIDE THAT, IT IS A GENUINE FINDING
// FOR R1-3, NOT A BAND TO WIDEN. A seam column that is not on the graded lane
// means the Route corridor does not actually reach the arrival point, which is a
// terrain-recipe question -- and a terrain-recipe change regenerates the WHOLE
// Dawnmere heightmap and re-measures every table in this file. Absorbing it by
// loosening the boot unit below would hide exactly the thing this slice was
// sequenced to find out.
//
// ★★ THIS ROW SHIPPED AS THE FILE'S UNMEASURED SENTINEL and was FROZEN at
// 24.36592 on 2026-08-15, exactly as the lab table above was on 2026-08-14. The
// sentinel is NOT a height and is nowhere near one, and the boot unit below is
// RED BY DESIGN whenever a row still holds it -- which no row does today
// (ZM-65 froze the second row, DawnmereNorthGate, on 2026-08-24).
// (This paragraph read "THE ROW SHIPS AS … RED BY DESIGN until it is replaced"
// for nine days after the freeze that falsified it. The procedure below is what
// a RE-measure follows, not a description of the current state.)
// TO (RE-)FREEZE A ROW: run ZM_DawnmereRouteSeamGroundTruth_Test
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
inline constexpr float fZM_DAWNMERE_FROM_ROUTE1_X = 120.0f;
inline constexpr float fZM_DAWNMERE_FROM_ROUTE1_Z = 144.0f;

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
	// The north seam gate's OWN column (ZM-D-203 §5, closed by ZM-65). Before
	// this row existed, ZM_GetDawnmereNorthGateCentreY() borrowed the row above
	// -- 12 m south of the gate -- because a new route-seam row cannot be
	// produced without a real raycast against a warm bake. See that function,
	// further down this file, for the sign-dependence the borrowing's cost claim
	// used to omit.
	ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_NORTH_GATE,

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
// centre. ZM_GameStateManager::CalculateSpawnPosition adds the capsule half-extent
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
// 12 m NORTH of the FromRoute1 arrival marker at (280, 544), i.e. between the
// arriving player and the northern edge of the region -- the same relationship,
// at the same 12 m separation, that Route 1's two gates and Thornacre's return
// gate keep with their own markers. The sensor is therefore the last thing a
// player walking north crosses, and an ARRIVING player is never already inside
// it (see the clearance floor below).
//
// ★ BOTH CONTAINMENTS ARE ARITHMETIC, NOT TASTE, and both are stated here so a
// later reader can re-check them rather than trust them. Against the Dawnmere
// terrain recipe (Source/World/ZM_TerrainAuthoring.cpp):
//   * the "RouteGate" PAD is at (120, 176) with a 22 m FLATTEN radius; this
//     column is 20 m from its centre, i.e. inside it -- unlike the arrival
//     column 12 m south, which at 32 m falls outside;
//   * the "Route" PATH runs (120, 208) -> (114, 170) -> (122, 124) -> ... with a
//     9 m flatten radius; this column lies ~3.6 m from the second segment.
// So the ground under this gate is graded lane inside a graded pad.
//
// ★★ WHAT THAT DOES **NOT** MEAN, AND THIS SENTENCE USED TO SAY IT DID: "a
// flatten dab drives ground TO the recipe target". It does not.
// Zenith_TerrainEditor's FLATTEN kernel moves a texel
// `(target - h) * falloff * strength * 0.35` per dab, so being inside a corridor
// buys a fraction of the correction per overlapping dab and nothing more. v7
// measures this column at 22.681 against a 24.0 target -- 1.3 m out, inside two
// graded footprints -- while every column that sits on a SetHeight SHELF reads
// within a few tenths of it. The containments above are still worth stating:
// they are why this column is 1.3 m out rather than 3 m. They are not a
// guarantee of level ground, and nothing derived from this gate may assume one.
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
// (1) X SPAN. The Route corridor lays a 5 m dirt radius (a 10 m visible lane)
//     inside a 9 m flatten radius, and the RouteGate pad reaches 9.2 m either
//     side of the lane at this column (sqrt(22^2 - 20^2)). 32 m centred on the
//     lane covers all of that with 6.8 m to spare either side and leaves no gap
//     a player can slip through without leaving the walked corridor entirely.
//     ★ IT DID NOT SHRINK AT v7 AND IT DID AT v8, AND BOTH WERE RIGHT. The span
//     is set by how wide the GRADED ground is at this column, not by the town's
//     scale: v7 kept its 48 m because the RouteGate pad kept its 30 m radius, and
//     v8 cuts it to 32 because the corridor genuinely narrowed (route flatten
//     14 -> 9, gate pad 30 -> 22). WIDEN EITHER OF THOSE AND WIDEN THIS WITH
//     THEM -- a gate narrower than its own graded corridor is a gate a player
//     walks around.
inline constexpr float fZM_DAWNMERE_NORTH_GATE_SCALE_X = 32.0f;

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

// ★★ THE GATE'S CENTRE Y USED TO BE DERIVED FROM THE **ARRIVAL** COLUMN -- A
// KNOWN, SCOPED DEVIATION FROM THE ONE-COLUMN-PER-ANCHOR RULE THE ROUTE 1 AND
// THORNACRE HEADERS BOTH STATE (ZM-D-203 §5). ZM-65 closes it STRUCTURALLY: the
// gate now reads its OWN row, ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_NORTH_GATE, below.
// The history stays, because it explains a claim this file made and got only
// half right:
//
//   * Route 1 and Thornacre each measure their gate column separately, and their
//     frozen tables show the ground genuinely moves over 12 m -- Thornacre's two
//     columns differ by 0.254 m, Route 1's south pair by 0.475 m and its north
//     pair by 0.962 m. So a dedicated column was always the correct long-term
//     answer here too.
//   * What R1-3 could not do was PRODUCE one on the spot. A new row in the
//     route-seam table has to be a real downward raycast against the baked
//     Dawnmere heightfield; an unmeasured row ships as a placeholder that
//     ZM_DawnmereRouteSeamGroundTruth_Test reds on (0.15 m tolerance) and that
//     no source-only change can close. R1-3 therefore borrowed the FromRoute1
//     row 12 m south instead of leaving the gate unseated.
//   * ★★ WHAT THE BORROWING COST WAS SIGN-DEPENDENT, AND THE ORIGINAL NOTE DID
//     NOT SAY SO. It read: "a 4 m-tall box seated on a neighbouring column is at
//     worst ~1 m out of plumb against a 1.8 m body, so the sensor still spans
//     the walked capsule." That holds ONLY when the borrowed column sits AT OR
//     ABOVE the gate's own true ground. If the gate's real ground had been
//     1.8 m LOWER than the borrowed FromRoute1 column, there would have been NO
//     overlap between the box and a capsule standing on the real ground, and
//     the sensor would never have fired -- not a cosmetic seating error but a
//     dead trigger. Nothing before ZM-65 checked which side of that line the
//     real ground was on.
//   * ★ THE EXPECTED SIGN IS FAVOURABLE, WHICH IS WHY THE UNCHECKED CLAIM
//     HAPPENED TO BE SAFE. (280, 556) sits inside BOTH the "RouteGate" pad's
//     30 m flatten radius and the "Route" path's 18 m flatten radius (3.71 m
//     from the path), while the borrowed (280, 544) column sits in the path
//     corridor ALONE, outside the pad's radius -- the two containments stated
//     above this block. A column flattened by two dabs is expected to land
//     closer to the recipe's target height than one flattened by a single dab,
//     so the gate's real ground is PREDICTED around -0.37 m against the
//     FromRoute1 measurement (24.36592) -- the favourable direction, nowhere
//     near the >= 1.8 m adverse threshold above. This is a PREDICTION, stated
//     before the measurement so the measurement can contradict it, exactly as
//     the FromRoute1 row's own pre-measurement prediction was stated in the
//     R1-2 block above (and was itself corrected once the real raycast
//     landed).
//
// ★ STATUS: FROZEN 2026-08-24 at 24.29772 (ZM-65). The row was authored at
// fZM_DAWNMERE_ROUTE_SEAM_GROUND_UNMEASURED, exactly as the FromRoute1 row was
// before its 2026-08-15 freeze, and ZM_DawnmereRouteSeamGroundTruth_Test plus
// the route-seam pin unit (Tests/ZM_Tests_DawnmerePlacement.cpp,
// ZM_Interaction/RouteSeamGround_EachRowStandsOnItsOwnAnchorAndIsMeasured -- the
// rename is ZM-65's, since the old name no longer fit a two-row table) were RED
// BY DESIGN until the measurement landed. `ZM_GetDawnmereNorthGateCentreY()` now
// reads this row's OWN column; the ZM-D-203 §5 deviation is CLOSED.
//
// ★★ THE PREDICTION ABOVE WAS RIGHT IN SIGN AND WRONG BY 5x IN MAGNITUDE, AND
// THE SIGN IS THE HALF THAT MATTERED. The measurement is 24.29772 against the
// FromRoute1 row's 24.36592 -- **-0.068 m**, not the predicted -0.37 m. The
// two-dabs-land-closer-to-target reasoning holds directionally; what it
// over-estimated is how far a SECOND flatten dab moves ground that a single dab
// had already driven to the same 24.0 m target. Both columns are graded; only
// the erosion pass separates them.
//
// ★ SO THE DEVIATION WAS NEVER DETECTABLE BY WATCHING THE NUMBER. 0.068 m is
// INSIDE this oracle's own 0.150 m tolerance, so a derived row would have passed
// the ground-truth check had one ever pointed at this column. The deviation was
// only ever visible as a MISSING ROW -- which is precisely ZM-D-203 Decision 1's
// argument, and the reason this was worth a ticket rather than a comment.
//
// This Y lands in the committed Dawnmere bytes, so the freeze and the windowed
// re-author belong to the same commit.
inline float ZM_GetDawnmereNorthGateCentreY()
{
	return ZM_DawnmereRouteSeamSampleFeetY(
			ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_NORTH_GATE)
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
