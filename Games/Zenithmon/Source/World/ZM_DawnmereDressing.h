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

#ifdef ZENITH_TOOLS
// Zenith_EditorAutomation step (a captureless void(*)()): scatter every group in
// the table into the ACTIVE scene, reading heights from a standalone terrain
// editor session. Runs inside the Dawnmere authoring block, after the scene and
// its terrain exist and before the save.
void ZM_ScatterDawnmerePropsStep();
#endif
