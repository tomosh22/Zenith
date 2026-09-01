#pragma once

// ★★ SEE `Games/Zenithmon/Docs/MapLayoutPlaybook.md` sections 3.4-3.6 before
// editing the drive-leg table or the tree clumps. Two rules that are not
// discoverable from this file alone: a leg belongs in the table only if some
// test actually DRIVES it (one inferred from the map sterilised an 84 m strip
// and failed an NPC that was never on anything), and a TREE is not subject to
// the prop keep-out at all -- the tree brush is a terrain tool that cannot see
// this file, and tree trunks carry colliders.
//

#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"   // fZM_SIGHT_MAX_DISTANCE -- the rival's keep-out IS his sight range
#include "Zenithmon/Source/Data/ZM_PropData.h"              // ZM_PROP_ID -- the authored outdoor prop table

#include <cstring>   // strcmp -- the outdoor prop mapping is an EXACT name match

// ============================================================================
// ZM_DawnmereDressing (ZM-D-217) -- the SCENERY layer of Dawnmere: instanced
// rocks, deadwood, bushes and woodland, plus the ONE piece of geometry that
// makes any of it safe to author, the town KEEP-OUT.
//
// ★★ WHY A KEEP-OUT EXISTS AT ALL, AND IT IS NOT ABOUT TASTE. Every automated
// traversal in this game drives the player with DriveTowardXZ, which has NO
// obstacle avoidance: it holds a direction until the target is reached or the
// frame cap expires. A 1.8 m static body on such a leg stops the capsule dead
// (the step assist is 0.40 m) and the suite dies at its frame cap with a timeout
// that names a DISTANCE, never the boulder. Boulders, shards, stumps, logs and
// tree trunks ALL carry per-instance colliders -- that is what makes them read
// as world rather than as decals -- so a scatter with no keep-out would be a
// randomised minefield laid across ZM_PlayerHomeRoundTrip_Test,
// ZM_LabRoundTrip_Test, ZM_NpcTalk_Test and the north-seam walk at once.
//
// The Dawnmere authoring block in Zenithmon.cpp has carried that warning as
// PROSE since S6 ("THE OTHER TWO MUST STAY OFF THE HOME DRIVE CORRIDOR"). Prose
// in front of a check is the defect; this file is the check.
//
// PURE, and deliberately so: no ECS, no scene, no physics, no g_xEngine, no
// allocation and no ZENITH_TOOLS guard on anything above the authoring step at
// the bottom. The keep-out and both tables have to be visible to boot units that
// run in a headless CI build where Project_RegisterEditorAutomationSteps is
// compiled out entirely -- which is what lets ZM_Tests_DawnmereDressing.cpp
// assert "no tree clump overlaps a drive corridor" with no assets and no GPU.
// ============================================================================

// ---- The keep-out ----------------------------------------------------------
//
// ★ IT IS DERIVED, NEVER MIRRORED. The pad and path discs come from
// ZM_GetDawnmereTerrainRecipe() and the anchors from ZM_DawnmerePlacement.h, so
// moving a pad or an NPC moves the keep-out with it in the same edit. The only
// thing spelled HERE is the set of BLIND DRIVE LEGS, because those exist solely
// in test code and nothing else in the shipped data describes them.

// One drive leg a test walks without avoidance. Y is deliberately absent:
// DriveTowardXZ is XZ-only.
struct ZM_DawnmereDriveLeg
{
	const char* m_szName;
	float m_fAX;
	float m_fAZ;
	float m_fBX;
	float m_fBZ;
};

// The radius every drive leg is protected by. A player capsule is 0.4 m; 6 m
// leaves room for the drive's own cross-track error plus a prop's own footprint
// without making the town's outskirts unreachable by scenery.
inline constexpr float fZM_DAWNMERE_DRIVE_LEG_RADIUS = 6.0f;

// Anchors (NPCs and patrol waypoints) and arrival markers get their own discs:
// they are points a body stands on or is warped onto, and neither is necessarily
// inside a pad.
inline constexpr float fZM_DAWNMERE_KEEPOUT_ANCHOR_RADIUS = 5.0f;
inline constexpr float fZM_DAWNMERE_KEEPOUT_MARKER_RADIUS = 8.0f;

// ★★ THE ARMED TRAINER NEEDS HIS WHOLE SIGHT RANGE CLEAR OF COLLIDERS, NOT JUST
// HIS BODY, AND FINDING THAT OUT COST A WHOLE DEBUGGING PASS.
// ZM_Interactable::TickTrainerSight gates spotting on TWO things: the pure cone,
// and then -- only once the cone passes -- an OCCLUSION RAYCAST,
// ZM_ProbeTrainerSightLine, from the trainer to the player. A boulder, a shard, a
// stump, a fallen log or a tree trunk standing anywhere on that line makes
// m_bSightLineClear false and the rival simply never reacts.
//
// The symptom is nothing like the cause. ZM_RivalVesperAuthored_Test reported
// "the walk-up STALLED", with the player driven to within 0.005 m of the rival,
// `sightEnabled=true`, `state=WATCHING`, `facingAbsDot=1.00000` and every
// placement clause green -- i.e. a correctly placed, correctly facing, armed
// trainer with the player standing on top of him and no spot. Nothing in that
// failure mentions scenery, and the scatter is a stream of RNG draws nobody can
// read off the source.
//
// So the keep-out radius for the RIVAL is his SIGHT RANGE plus a margin, not the
// 5 m every other anchor gets, and it is spelled against
// fZM_SIGHT_MAX_DISTANCE rather than as a literal: raising the sight range and
// leaving this behind would silently re-open the same hole. The 4 m on top covers
// a prop's own half-width, since the scatter is sampled at instance ORIGINS.
//
// ★ IT APPLIES TO THE HARD KEEP-OUT ONLY. The probe is a PHYSICS ray, so only a
// prop with a collider can break the line; bushes, flagstones, pebbles and loose
// branches carry none and are free to grow right up to him, which is also the
// better look -- a rival half-hidden in the verge is the shot.
inline constexpr float fZM_DAWNMERE_KEEPOUT_TRAINER_RADIUS =
	fZM_SIGHT_MAX_DISTANCE + 4.0f;

// The extra margin the PROP scatter adds on top of the keep-out. Props are
// sampled point-wise, so this is the whole safety budget for a prop's own
// footprint.
inline constexpr float fZM_DAWNMERE_PROP_KEEPOUT_MARGIN = 2.0f;

// ...and the (larger) margin a TREE CLUMP is checked at. A clump is a DISC the
// engine's tree brush scatters inside with no keep-out awareness of its own, so
// the whole disc has to clear the keep-out rather than just its centre -- and
// trees carry the tallest collider of anything authored here.
inline constexpr float fZM_DAWNMERE_TREE_KEEPOUT_MARGIN = 4.0f;

// How far every dab and every prop stays off the terrain boundary.
inline constexpr float fZM_DAWNMERE_DRESSING_EDGE_MARGIN = 8.0f;

u_int ZM_GetDawnmereDriveLegCount();
// TOTAL: an out-of-range index returns a degenerate zero-length leg at the town
// centre under the name "UNKNOWN", never reads off the end of the table.
const ZM_DawnmereDriveLeg& ZM_GetDawnmereDriveLeg(u_int uLeg);

// ★★ THERE ARE TWO KEEP-OUTS, AND WHICH ONE A GROUP GETS IS DECIDED BY ITS
// COLLIDER, NOT BY ITS SIZE OR ITS FAMILY.
//
// The HARD keep-out is the safety one. It is what stops a boulder, a shard, a
// stump, a log or a tree trunk from standing on a line a test drives blind, and
// it treats a terrain path as its FLATTEN radius -- the whole graded corridor,
// because graded-but-unpainted ground beside a visible lane is exactly where a
// player walks.
//
// The SOFT keep-out is for props that carry NO COLLIDER: bushes, flagstones,
// pebble clusters, loose branches. A player walks THROUGH all of those, so they
// cannot wedge anything and the safety argument simply does not apply to them.
// What still applies is an aesthetic rule -- do not grow a shrub out of the
// paving -- so their version of a pad or a path is its DIRT radius, the part that
// is actually surfaced. Everything else (the blind drive legs, the NPC anchors,
// the arrival markers, the seam gate) is IDENTICAL in both, because those are
// about where a body stands rather than about what it can walk through.
//
// ★ WHY THIS IS WORTH THE SECOND FUNCTION. The Plaza pad is graded to 44 m and
// paved to 33 m, so the hard keep-out sterilises an 11 m ring of lawn all the way
// round the town square -- the exact band where verge planting belongs, and the
// difference between a village and a mown field with people standing in it.
//
// Both are TOTAL for every finite input; a non-finite one answers a large
// NEGATIVE distance, so a NaN can never be mistaken for a legal placement. Both
// are POSITIVE outside and NEGATIVE inside, in metres.
float ZM_DawnmereKeepOutClearance(float fX, float fZ);
float ZM_DawnmereSoftKeepOutClearance(float fX, float fZ);

// Convenience: is this point inside the keep-out expanded by fExtraMargin?
bool ZM_IsInsideDawnmereKeepOut(float fX, float fZ, float fExtraMargin);
bool ZM_IsInsideDawnmereSoftKeepOut(float fX, float fZ, float fExtraMargin);

// ★★ THE SAFETY HALF ON ITS OWN -- every point a body stands on or is warped
// onto: the blind drive legs, the NPC anchors, the four warp markers and the
// seam gate. Both keep-outs above are this ANDed with a graded-ground term, and
// the two are not the same KIND of rule: this one is why a traversal test does
// not die at its frame cap, while graded ground is a proxy for the judgement a
// randomly-drawn point cannot exercise.
//
// Exposed because an AUTHORED placement has that judgement already and needs
// only this half -- see the authored-prop table below for the full argument, and
// note that applying the graded-ground half there would refuse both building
// pads, which is precisely the ground a hand-placed prop belongs on.
//
// bIncludeTrainerSight adds the armed trainer's sight radius, and is a COLLIDER
// property: only a prop with a physics body can break ZM_ProbeTrainerSightLine.
// Pass true for anything that collides.
//
// TOTAL for every finite input; a non-finite one answers a large NEGATIVE
// distance. POSITIVE outside, NEGATIVE inside, in metres.
float ZM_DawnmereBodyAnchorClearance(float fX, float fZ, bool bIncludeTrainerSight);

// The floor a pad's or path's SOFT radius is held to, as a fraction of its
// flatten radius. Without it the "RouteGate" pad -- which paints no dirt at all
// (radius 0) -- would have no soft footprint whatsoever and the seam apron would
// grow bushes right up to the gate.
inline constexpr float fZM_DAWNMERE_SOFT_RADIUS_FLATTEN_FRACTION = 0.5f;

// ---- The woodland ----------------------------------------------------------
//
// One dab of the engine terrain editor's TREE BRUSH: it scatters trunk+leaves
// instance pairs inside the disc, rejecting on slope and spacing, and gives each
// trunk the shared capsule collider. Using the engine brush rather than a
// hand-rolled tree group is deliberate -- the trunk/leaves lockstep, the sway
// VAT phase and the collider are already tested there, and Dawnmere's only
// genuine difference from RenderTest's rings is WHERE the discs are.
struct ZM_DawnmereTreeClump
{
	const char* m_szName;
	float m_fX;
	float m_fZ;
	float m_fRadius;
};

// The brush settings every Dawnmere clump is painted with. Public because the
// unit that pins the woodland reads them rather than re-typing them.
inline constexpr int   iZM_DAWNMERE_TREES_PER_CLUMP   = 34;
inline constexpr float fZM_DAWNMERE_TREE_SCALE_MIN    = 0.85f;
inline constexpr float fZM_DAWNMERE_TREE_SCALE_MAX    = 1.60f;
inline constexpr float fZM_DAWNMERE_TREE_SPACING      = 6.0f;
inline constexpr float fZM_DAWNMERE_TREE_MAX_SLOPE_DEG = 40.0f;
inline constexpr int   iZM_DAWNMERE_TREE_SEED         = 0x5EED17;

u_int ZM_GetDawnmereTreeClumpCount();
// TOTAL: an out-of-range index returns a zero-RADIUS clump at the town centre
// under the name "UNKNOWN". Zero radius is the inert answer here -- a dab of
// radius 0 places nothing.
const ZM_DawnmereTreeClump& ZM_GetDawnmereTreeClump(u_int uClump);

// ---- The prop scatter ------------------------------------------------------
//
// One instance group per (mesh, material) pair, because an instance group is
// single-mesh and single-material by construction. Every asset named here is one
// of the SHARED engine sets under Zenith/Assets/Meshes/{Rocks,FallenTrees,Bushes},
// regenerated on every tools boot by their generators in Tools/ -- the same sets
// RenderTest dresses its campus with. Nothing in this table is Zenithmon-owned
// art.
struct ZM_DawnmereScatterGroup
{
	const char* m_szEntity;
	const char* m_szAssetDir;      // under ENGINE_ASSETS_DIR, e.g. "Meshes/Rocks/"
	const char* m_szMeshBase;
	const char* m_szMaterialFile;
	const char* m_szVATFile;       // "" = static prop; else a sway VAT beside the mesh
	float       m_fAnimDuration;   // seconds; read only when m_szVATFile is non-empty
	float       m_fBoundsCentreY;  // local-space cull sphere, scaled per instance
	float       m_fBoundsRadius;
	u_int       m_uCount;
	float       m_fPlazaMin;       // metres from the plaza centre before this may appear
	float       m_fMaxSlopeTan;    // rise/run ceiling for the site
	float       m_fSpacing;        // minimum metres between two of THIS prop
	float       m_fScaleMin;
	float       m_fScaleMax;
	float       m_fTiltDeg;        // max lean off the piece's rest attitude
	float       m_fLayDownDeg;     // 0 = stands as authored; 90 = tipped onto its side
	float       m_fLengthMetres;   // tipped rows only: how far ahead to read the ground
	float       m_fSinkFraction;   // metres sunk per unit of scale; NEGATIVE LIFTS
	u_int       m_uSeed;
	bool        m_bCollider;
	float       m_fColliderRadius;     // local (pre-scale), Jolt capsule
	float       m_fColliderHalfHeight;
	float       m_fColliderYOffset;
};

u_int ZM_GetDawnmereScatterGroupCount();
// TOTAL: an out-of-range index returns a zero-COUNT group named "UNKNOWN" whose
// every string field is a valid empty C string, so a caller that skipped the
// bound check places nothing rather than dereferencing a null.
const ZM_DawnmereScatterGroup& ZM_GetDawnmereScatterGroup(u_int uGroup);

// The total instances the table asks for, across every group. Used by the
// authoring log line and by the unit that pins the dressing's size.
u_int ZM_GetDawnmereScatterRequestedTotal();

// ---- Authored outdoor props -------------------------------------------------
//
// ★★ THE FIRST ROSTER PROP PLACED ANYWHERE OUTDOORS, and the ~500 instances the
// scatter above puts on this map are NOT a counter-example: they are a different
// system entirely -- shared ENGINE meshes under
// `Zenith/Assets/Meshes/{Rocks,FallenTrees,Bushes}`, none of it Zenithmon-owned
// art and none of it from the roster.
//
// ★★★ AND HALF THE ROSTER IS PLACED NOWHERE. COUNTED, because an earlier draft
// of this comment guessed and was wrong twice over -- it claimed "22 of the 28"
// and listed the six biome dressing sets among the unused, when
// `ZM_BattleArena.cpp` places all six. It also missed the ground items entirely.
// The real figure, by grepping every non-generator reference to each id:
//
//   USED (16): the six interior furniture rows (ZM_InteriorDressing.h), LampPost
//     and Barrel here, the six DRESSING_* sets (ZM_BattleArena.cpp, one per
//     battle-dome biome) and the three ITEM_* presentations (ZM_GroundItem.cpp).
//   PLACED NOWHERE (12): FenceWood, FenceStone, SignPost, TownBoard,
//     LanternPost, BridgePlank, BridgeStone, LedgeLow, LedgeHigh, RockSmall,
//     RockLarge, Boulder.
//
// ★ EVERY ONE OF THOSE TWELVE IS AN OUTDOOR FIXTURE OR SCATTER PIECE, which is
// not a coincidence: they are exactly the families this map gets from the shared
// engine sets instead. They cost nothing at runtime -- ZM_BakeAllAssets has no
// shipped caller and ZM_EnsurePropBaked is warm-safe -- but two automated tests
// (AssetGallery, BattleArena) bake the whole roster, which is why all 28 folders
// exist on disk. **A .glb dropped onto one of them replaces a model that nothing
// renders**, so importing art for one is only half a job; the other half is a
// placement row, which is what this table is.
//
// ★★ AND IT IS AUTHORED, NOT SCATTERED, WHICH IS THE WHOLE POINT. The scatter
// above draws a position, tests it and keeps it -- the right mechanism for
// things that GREW or FELL where they are. A barrel is put somewhere by a
// person: upright, against a wall, near a door. Rolling it into the scatter
// table would need slope ceilings, spacing and tilt to conspire into a placement
// that looks deliberate, which is a long way round to "x, z, against that wall".
//
// ★★★ SO IT TAKES A DIFFERENT KEEP-OUT, AND THAT IS THE ONE DECISION HERE WORTH
// ARGUING. The hard keep-out is two halves ANDed together: graded ground (pads
// and paths at their flatten radius) and body anchors (blind drive legs, NPC
// anchors, warp markers, the seam gate). The graded-ground half exists because a
// RANDOM point in town is overwhelmingly likely to be on a lane or in a doorway
// -- it is a proxy for judgement, not a safety property. A hand-placed prop has
// the judgement already, and applying that half here would refuse every square
// metre of the two building pads: i.e. exactly the ground a barrel belongs on,
// and nowhere else.
//
// The half that IS a safety property applies in full. `DriveTowardXZ` has no
// obstacle avoidance (map playbook 3.4), so a collider on a blind leg wedges a
// traversal test into its frame cap with a failure naming a DISTANCE rather than
// the blocker -- and that is true of an authored prop and a scattered one alike.
// `ZM_DawnmereBodyAnchorClearance` is that half, exposed for this table, and
// `ZM_Dressing/AuthoredProps_ClearEveryBodyAnchor` is the enforcement.
//
// ★ THE MARGIN IS THE PROP'S OWN FOOTPRINT PLUS SLACK, AND IT IS MEASURED FROM
// THE MESH RATHER THAN THE ROSTER. The barrel's row says 0.7 x 0.7; the imported
// model fits to 0.89 x 0.89, 27% over, because a uniform fit honours the longest
// axis only (ZM_PropFit.h). A margin derived from the roster row would be 0.1 m
// short on every barrel in the table.
inline constexpr float fZM_DAWNMERE_AUTHORED_PROP_MARGIN = 1.0f;

// ★★ WHAT A ROW CLAIMS ABOUT WHERE IT STANDS, stated per row rather than guessed
// from the prop's kind. The first version of this table held four barrels and its
// unit asserted "every outdoor prop stands against a building wall" -- true of
// barrels, false the moment a lamp post arrived (a lamp belongs PAST a corner,
// because the ground between is a blind drive leg), and false again for a rock.
// Inferring the rule from ZM_PROP_KIND fails too: a lamp post flanks a doorway
// and a lantern post lines a lane, and both are ZM_PROP_KIND_LAMP.
//
// So each row says which claim it is making, and the unit checks THAT claim.
// FREE is not an escape hatch -- it is the honest statement for scatter, ledges
// and crossings, which are deliberately in open ground and whose only real
// constraint is the keep-out every row is checked against anyway.
enum ZM_DAWNMERE_PROP_ANCHOR : u_int
{
	ZM_DAWNMERE_ANCHOR_FREE,             // open ground; the keep-out is the whole rule
	ZM_DAWNMERE_ANCHOR_BUILDING_WALL,    // inside a building's X footprint, just off its face
	ZM_DAWNMERE_ANCHOR_BUILDING_CORNER,  // just past a corner, level with the frontage
};

struct ZM_DawnmereProp
{
	const char* m_szEntityName;
	ZM_PROP_ID  m_eProp;
	float       m_fX;
	float       m_fZ;
	// Yaw as a FROZEN quaternion pair (w, y) from ZM_PropData.h's block -- never
	// computed, for the ZM-D-183 reason recorded there.
	float       m_fQuatW;
	float       m_fQuatY;
	ZM_DAWNMERE_PROP_ANCHOR m_eAnchor;
};

// ★★★ EVERY PROP THE GAME GENERATES IS PLACED, WHICH IS A RULING RATHER THAN A
// PREFERENCE (2026-09-01). Twelve of the roster's twenty-eight rows were being
// generated, baked and rendered nowhere -- all of them outdoor fixtures or
// scatter, because this map takes those families from the shared ENGINE sets
// instead. A generated asset nothing places is a bake nobody sees and a
// commission nobody can review; worse, dropping a `.glb` onto one of those rows
// replaced a model that still did not appear, which is how AB-PROP-07 arrived
// looking finished while being invisible.
//
// `ZM_PropPlacement.h` is the mechanical half: it answers where each prop is
// placed and `ZM_Dressing/EveryGeneratedPropIsPlacedInGame` refuses a roster row
// that is placed nowhere. This table is where the outdoor half of that answer
// lives.
//
// ★ THE COORDINATES WERE CHOSEN AGAINST A MODEL OF THE KEEP-OUT, not by eye and
// not by build-and-see. An offline replica of ZM_DawnmereBodyAnchorClearance was
// VALIDATED first -- it reproduces all eight of the barrel and lamp-post figures
// the engine had already reported, to 0.4 mm -- and every row below was then
// picked to clear the margin before a single one was compiled. The clearance unit
// prints what the engine actually measures, which is what the rows are signed off
// against.
inline constexpr ZM_DawnmereProp axZM_DAWNMERE_PROPS[] =
{
	// ---- Against the two building walls -------------------------------------
	{ "DawnmereHomeBarrelWest", ZM_PROP_BARREL,  84.30f,  99.30f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_WALL },
	{ "DawnmereHomeBarrelEast", ZM_PROP_BARREL,  99.70f,  99.30f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_WALL },
	{ "DawnmereLabBarrelWest",  ZM_PROP_BARREL, 139.20f, 103.30f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_WALL },
	{ "DawnmereLabBarrelEast",  ZM_PROP_BARREL, 156.80f, 103.30f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_WALL },

	// ---- Lamp posts off the two front corners -------------------------------
	//
	// ★ THE X VALUES ARE WHAT THE KEEP-OUT LEAVES. A first draft at +/-5.5 m off
	// each building's axis was INSIDE the 6 m blind drive leg and the 8 m arrival
	// marker; level with a frontage a lamp has to be ~10 m off the axis to clear
	// both, which happens to put it just past the corner where a lamp belongs.
	//
	// ★ THEIR YAW IS FREE BY SYMMETRY rather than by measurement, unlike the
	// barrel's: the lantern head is a flare of revolution about the post's axis
	// and the model carries no bracket or arm to point.
	{ "DawnmereHomeLampWest",   ZM_PROP_LAMP_POST,  82.00f,  98.50f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_CORNER },
	{ "DawnmereHomeLampEast",   ZM_PROP_LAMP_POST, 102.00f,  98.50f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_CORNER },
	{ "DawnmereLabLampWest",    ZM_PROP_LAMP_POST, 136.00f, 102.50f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_CORNER },
	{ "DawnmereLabLampEast",    ZM_PROP_LAMP_POST, 160.00f, 102.50f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_BUILDING_CORNER },

	// ---- The village's own signage, on the plaza's north edge ----------------
	//
	// All three face -Z (YAW180 from a model whose front is +X), which is the
	// direction a player arriving from the spawn reads them from.
	{ "DawnmereTownBoard",      ZM_PROP_TOWN_BOARD, 106.00f,  98.00f,
		fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereSignHome",       ZM_PROP_SIGN_POST,  104.00f,  92.00f,
		fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereSignRoute",      ZM_PROP_SIGN_POST,  132.00f, 124.00f,
		fZM_INTERIOR_YAW180_W, fZM_INTERIOR_YAW180_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- Lantern posts lining the route lane --------------------------------
	//
	// ★ NOT AT A BUILDING, WHICH IS WHY THE ANCHOR RULE CANNOT BE READ OFF THE
	// KIND. These are ZM_PROP_KIND_LAMP exactly like the four lamp posts above and
	// belong somewhere completely different: the lamp posts light two doorways,
	// these light the walk north to the seam gate.
	{ "DawnmereLanternWest",    ZM_PROP_LANTERN_POST, 110.00f, 120.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereLanternEast",    ZM_PROP_LANTERN_POST, 130.00f, 136.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- A timber field boundary west of the home ---------------------------
	//
	// ★★ THE SPACING IS THE PROP'S OWN LENGTH, WHICH IS WHAT "TILES END TO END"
	// MEANS. A fence row is 2.0 m wide in the roster and ZM_ComputePropFit scales
	// every delivery onto exactly that, so centres 2.0 m apart abut with no gap
	// and no overlap however the mesh was authored. Change the roster width and
	// this spacing must change with it -- FenceRunSpacingMatchesTheRosterLength
	// is what refuses the mismatch.
	{ "DawnmereFenceWood0",     ZM_PROP_FENCE_WOOD,  68.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceWood1",     ZM_PROP_FENCE_WOOD,  70.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceWood2",     ZM_PROP_FENCE_WOOD,  72.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceWood3",     ZM_PROP_FENCE_WOOD,  74.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- A dry-stone wall east of the lab, same tiling rule -----------------
	{ "DawnmereFenceStone0",    ZM_PROP_FENCE_STONE, 166.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceStone1",    ZM_PROP_FENCE_STONE, 168.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceStone2",    ZM_PROP_FENCE_STONE, 170.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereFenceStone3",    ZM_PROP_FENCE_STONE, 172.00f,  92.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- Stone scatter on the outskirts -------------------------------------
	//
	// ★ THESE DUPLICATE NOTHING. The ~500 instanced rocks this map already
	// scatters are the shared ENGINE set under Zenith/Assets/Meshes/Rocks; these
	// three are Zenithmon's OWN roster rows, which had never been rendered.
	{ "DawnmereRockSmallWest",  ZM_PROP_ROCK_SMALL,  56.00f, 110.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereRockSmallEast",  ZM_PROP_ROCK_SMALL, 192.00f, 118.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereRockLargeWest",  ZM_PROP_ROCK_LARGE,  60.00f, 122.00f,
		fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereRockLargeEast",  ZM_PROP_ROCK_LARGE, 188.00f, 106.00f,
		fZM_INTERIOR_YAW90_W, fZM_INTERIOR_YAW90_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBoulder",        ZM_PROP_BOULDER,    200.00f,  86.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- Traversal ledges on the rising ground south of town ----------------
	//
	// The town sits on a ~24 m shelf and the hills crest at 32-36 m, so the south
	// flanks are where a step-down reads as one.
	{ "DawnmereLedgeLowWest",   ZM_PROP_LEDGE_LOW,   66.00f,  40.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereLedgeLowEast",   ZM_PROP_LEDGE_LOW,  176.00f,  40.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereLedgeHighWest",  ZM_PROP_LEDGE_HIGH,  58.00f,  32.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereLedgeHighEast",  ZM_PROP_LEDGE_HIGH, 184.00f,  32.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },

	// ---- Two crossings flanking the route lane ------------------------------
	//
	// ★★ AND THIS MAP HAS NO WATERCOURSE, WHICH IS RECORDED RATHER THAN DRESSED
	// UP. `ZM_GetDawnmereTerrainRecipe` has four pads and three paths and not one
	// water feature, so these two runs are dry crossings over the low ground
	// either side of the lane -- honest as a culvert or a boardwalk over boggy
	// ground, thin as a "bridge". They are placed because the ruling is that a
	// generated asset is placed, and this is where they read least oddly; the
	// right home is a stream, the day the terrain has one.
	//
	// ★ SPACING IS EACH ROW'S OWN DEPTH, not a shared number: BridgePlank is 4.0 m
	// deep and BridgeStone 5.0, and both tile ALONG that axis.
	{ "DawnmereBridgePlank0",   ZM_PROP_BRIDGE_PLANK,  82.00f, 140.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBridgePlank1",   ZM_PROP_BRIDGE_PLANK,  82.00f, 144.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBridgePlank2",   ZM_PROP_BRIDGE_PLANK,  82.00f, 148.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBridgeStone0",   ZM_PROP_BRIDGE_STONE, 160.00f, 140.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBridgeStone1",   ZM_PROP_BRIDGE_STONE, 160.00f, 145.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
	{ "DawnmereBridgeStone2",   ZM_PROP_BRIDGE_STONE, 160.00f, 150.00f,
		fZM_INTERIOR_YAW0_W, fZM_INTERIOR_YAW0_Y, ZM_DAWNMERE_ANCHOR_FREE },
};

inline u_int ZM_GetDawnmerePropCount()
{
	return (u_int)(sizeof(axZM_DAWNMERE_PROPS) / sizeof(axZM_DAWNMERE_PROPS[0]));
}

// TOTAL: an out-of-range index answers the first row rather than reading off the
// end, matching ZM_GetInteriorProp's contract.
inline const ZM_DawnmereProp& ZM_GetDawnmereProp(u_int uIndex)
{
	return axZM_DAWNMERE_PROPS[uIndex < ZM_GetDawnmerePropCount() ? uIndex : 0u];
}

// Which prop an authored outdoor entity wears, by name.
//
// ★ A SECOND RESOLVER RATHER THAN ONE THAT WALKS BOTH. Making
// ZM_PropForInteriorPropEntity also read this table would make the INTERIOR
// dressing header include the Dawnmere one, which is backwards -- an interior
// room knows nothing about a town. Each table owns its own resolver and the
// consumer (ZM_InteriorFurniture, which sees both) composes them.
//
// TOTAL: any other name answers ZM_PROP_NONE.
inline ZM_PROP_ID ZM_PropForDawnmerePropEntity(const char* szEntityName)
{
	if (szEntityName == nullptr)
	{
		return ZM_PROP_NONE;
	}
	for (u_int u = 0u; u < ZM_GetDawnmerePropCount(); ++u)
	{
		if (strcmp(szEntityName, axZM_DAWNMERE_PROPS[u].m_szEntityName) == 0)
		{
			return axZM_DAWNMERE_PROPS[u].m_eProp;
		}
	}
	return ZM_PROP_NONE;
}

// ★ THE AUTHORING STEP IS NOT DECLARED HERE, and the asymmetry with
// ZM_ScatterDawnmerePropsStep just above is deliberate. That one needs only
// engine components, so it lives in this file's .cpp. This one needs
// ZM_ResolvePropFit (which measures the baked mesh) and the ZM_InteriorFurniture
// component, both file-local to Zenithmon.cpp -- so it lives there, beside the
// other custom steps, exactly as the interior dressing's authoring does. What
// stays HERE is the table, which is what the headless units read.
#ifdef ZENITH_TOOLS
// Zenith_EditorAutomation step (a captureless void(*)()): scatter every group in
// the table into the ACTIVE scene, reading heights from a standalone terrain
// editor session. Runs inside the Dawnmere authoring block, after the scene and
// its terrain exist and before the save.
void ZM_ScatterDawnmerePropsStep();
#endif
