#pragma once

// ============================================================================
// ZM_CreatureArchetypeCommon -- the SHARED appendage kit every ZM_CreatureGen
// archetype builder (QUADRUPED / BIPED / AVIAN / SERPENT / AQUATIC / INSECTOID /
// BLOB / FLOATER_PLANTOID) composes. Each archetype builder lives in its OWN
// disjoint ZM_CreatureArchetype_<Name>.cpp and calls into this kit; this file is
// the SEAM those authors depend on, so its signatures are kept minimal but
// sufficient for a full quadruped (central body + 4 limbs + tail + head +
// optional horn).
//
// GEOMETRY MODEL (load-bearing -- inherited from the ZM_GenCommon loft toolkit):
// the loft sweeps rings along the WORLD Y axis ONLY. A ring authored at model-
// space centre C with half-extents (Rx,Rz) lies in the horizontal plane y=C.y;
// EmitRing maps it to (C.x + Rx*sin, C.y, C.z - Rz*cos). Every appendage below is
// therefore a vertical sweep: bodies/heads/horns sweep UP (+Y), limbs/tails
// sweep DOWN (-Y), each part offset in X/Z via its ring centres. Both sweep
// directions produce correct OUTWARD winding (EmitAndStitch flips the wall
// winding by ascending/descending Y and orients the caps to match).
//
// INVARIANTS every helper here upholds (the S4 mesh contract, enforced by
// ZM_ValidateGenMesh at cap 30 in the driver):
//   * distinct consecutive ring Y (no degenerate flat washer) -- callers must
//     pass parts with non-zero vertical span; helpers assert it,
//   * outward winding (delegated to ZM_MeshLoft),
//   * bones added parent-before-child, exactly one root across the whole build
//     (only ZM_AppendSpineTube may create the root; every other helper attaches
//     to an existing parent bone),
//   * <=2-bone ring skin: each AUTHORED ring binds to a SINGLE bone; the loft's
//     Catmull-Rom subdivision blends between the two nearest bones on the
//     interpolated rings (never more than 2 influences),
//   * <=30 bones per creature (the archetype clip-transfer cap) -- helpers keep
//     their bone counts small; the driver asserts the total.
//
// All positions are MODEL space. Bone local offsets are computed as
// (childWorld - parentWorld) so a future ZM_CreatureAnimGen pose is natural;
// bind-pose skinning is identity regardless of bone placement.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"      // ZM_GenMesh, ZM_MeshLoft, ZM_GenAddBone
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"    // ZM_SIZE_CLASS
#include "Maths/Zenith_Maths.h"

// ---------------------------------------------------------------------------
// (P3) Monotonic per-size-class world scale. GOLDEN-PINNED: these five values
// are baked into every creature's mesh extents, so changing one is a generator-
// version bump + cold re-bake. Strictly increasing TINY < SMALL < MEDIUM <
// LARGE < HUGE with MEDIUM == 1.0 (the reference scale).
// ---------------------------------------------------------------------------
float ZM_SizeClassScale(ZM_SIZE_CLASS eSize);

// ---------------------------------------------------------------------------
// Deterministic bone-name formatter: writes szBase, then a 2-digit zero-padded
// index when iIndex >= 0 (e.g. "Spine", "Spine00", "Spine07"). Always NUL-
// terminates within uCap; truncates rather than overflowing. uCap should be
// uZM_GEN_BONE_NAME_MAX.
// ---------------------------------------------------------------------------
void ZM_FormatBoneName(char* szOut, u_int uCap, const char* szBase, int iIndex);

// ---------------------------------------------------------------------------
// Common result of an appendage build: the bone range it created and its first
// appended vertex, so a caller can chain a child onto the tip bone.
// ---------------------------------------------------------------------------
struct ZM_KitAppendResult
{
	u_int                 m_uFirstBone = 0u;    // first bone this call added
	u_int                 m_uTipBone   = 0u;    // tip/last bone (attach children here)
	Zenith_Maths::Vector3 m_xTipWorld  = Zenith_Maths::Vector3(0.0f);   // model-space pos of the tip bone
	u_int                 m_uFirstVert = 0u;    // first mesh vertex this call appended
};

// ---------------------------------------------------------------------------
// ZM_AppendSpineTube -- the central body. A vertical sweep from the belly (low
// Y) to the back/withers (high Y), rings fullest near the middle, giving the
// rounded egg-shaped torso a quadruped hangs its limbs/head/tail from. Creates
// m_uSegments bones evenly along the sweep (the first becomes the SINGLE ROOT
// when m_iParentBone < 0); ring i binds to the nearest of those bones. Fills
// pauOutBones with the created bone indices (so the caller can pick specific
// "vertebrae" to attach a leg / head / tail to) and RETURNS the count written
// (== min(m_uSegments, uMaxOutBones)). Both ends are capped (a closed body).
// ---------------------------------------------------------------------------
struct ZM_KitSpineParams
{
	int                   m_iParentBone  = -1;                              // < 0 => create the root here
	Zenith_Maths::Vector3 m_xParentWorld = Zenith_Maths::Vector3(0.0f);     // parent bone world pos (ignored if root)
	Zenith_Maths::Vector3 m_xBellyCentre = Zenith_Maths::Vector3(0.0f);     // model-space centre of the lowest ring
	float                 m_fLength      = 1.0f;                            // vertical span belly -> back (> 0)
	u_int                 m_uSegments    = 4u;                              // ring/bone nodes along the sweep (2..8)
	float                 m_fHalfWidth   = 0.5f;                            // Rx at the fullest ring (side girth)
	float                 m_fHalfDepth   = 0.7f;                            // Rz at the fullest ring (front-back length)
	float                 m_fEndTaper    = 0.55f;                           // end-ring radius as a fraction of the fullest (0..1]
	float                 m_fBellyRound  = 1.0f;                            // ring super-ellipse (<1 rounds toward a box)
	u_int                 m_uSegs        = 10u;                             // radial segments (>= 3)
	ZM_GenUVIsland        m_xIsland;                                        // atlas island for the body
	const char*           m_szNamePrefix = "Spine";
};
u_int ZM_AppendSpineTube(ZM_GenMesh& xMesh, const ZM_KitSpineParams& xParams,
	u_int* pauOutBones, u_int uMaxOutBones);

// ---------------------------------------------------------------------------
// ZM_AppendLimb -- one leg/arm: a downward (hip -> foot) tapering tube on two
// bones ("<name>Up" at the hip, "<name>Lo" at the knee), the foot end capped,
// the hip end open where it meets the body. Attaches to m_iParentBone. Returns
// the lower (foot) bone as the tip.
// ---------------------------------------------------------------------------
struct ZM_KitLimbParams
{
	int                   m_iParentBone = 0;
	Zenith_Maths::Vector3 m_xParentWorld = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xHip  = Zenith_Maths::Vector3(0.0f);    // top (highest Y)
	Zenith_Maths::Vector3 m_xKnee = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xFoot = Zenith_Maths::Vector3(0.0f);    // bottom (lowest Y)
	float                 m_fRadiusTop  = 0.16f;
	float                 m_fRadiusMid  = 0.13f;
	float                 m_fRadiusFoot = 0.11f;
	u_int                 m_uSegs = 8u;
	ZM_GenUVIsland        m_xIsland;
	const char*           m_szName = "Leg";
};
ZM_KitAppendResult ZM_AppendLimb(ZM_GenMesh& xMesh, const ZM_KitLimbParams& xParams);

// ---------------------------------------------------------------------------
// ZM_AppendTail -- a tapering tail: rings from the base (attach) sweeping DOWN
// and back to the tip, on m_uSegments bones ("<prefix>00".."<prefix>NN"), the
// tip capped. Attaches to m_iParentBone (typically the rearmost spine bone).
// Returns the tip bone.
// ---------------------------------------------------------------------------
struct ZM_KitTailParams
{
	int                   m_iParentBone = 0;
	Zenith_Maths::Vector3 m_xParentWorld = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xBase = Zenith_Maths::Vector3(0.0f);    // highest Y
	Zenith_Maths::Vector3 m_xTip  = Zenith_Maths::Vector3(0.0f);    // lowest Y (must differ in Y from base)
	u_int                 m_uSegments   = 3u;                        // bones along the tail (1..6)
	float                 m_fRadiusBase = 0.12f;
	float                 m_fRadiusTip  = 0.03f;
	u_int                 m_uSegs = 7u;
	ZM_GenUVIsland        m_xIsland;
	const char*           m_szNamePrefix = "Tail";
};
ZM_KitAppendResult ZM_AppendTail(ZM_GenMesh& xMesh, const ZM_KitTailParams& xParams);

// ---------------------------------------------------------------------------
// ZM_AppendHorn -- a small upward cone on ONE bone: rings from a wide base
// sweeping UP to a near-zero tip, tip capped. Attaches to m_iParentBone
// (typically the head). Returns the horn bone.
// ---------------------------------------------------------------------------
struct ZM_KitHornParams
{
	int                   m_iParentBone = 0;
	Zenith_Maths::Vector3 m_xParentWorld = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xBase = Zenith_Maths::Vector3(0.0f);    // lowest Y
	Zenith_Maths::Vector3 m_xTip  = Zenith_Maths::Vector3(0.0f);    // highest Y (must differ in Y from base)
	float                 m_fRadiusBase = 0.08f;
	u_int                 m_uSegs = 6u;
	ZM_GenUVIsland        m_xIsland;
	const char*           m_szName = "Horn";
};
ZM_KitAppendResult ZM_AppendHorn(ZM_GenMesh& xMesh, const ZM_KitHornParams& xParams);

// ---------------------------------------------------------------------------
// ZM_AppendEllipsoidHead -- an ellipsoid head on ONE bone: rings sweeping UP
// through the ellipsoid (poles closed by caps, never a zero-radius ring), skin
// entirely bound to the head bone. Attaches to m_iParentBone (typically the
// front spine bone). Returns the head bone.
// ---------------------------------------------------------------------------
struct ZM_KitHeadParams
{
	int                   m_iParentBone = 0;
	Zenith_Maths::Vector3 m_xParentWorld = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xCentre      = Zenith_Maths::Vector3(0.0f);   // ellipsoid centre (model space)
	Zenith_Maths::Vector3 m_xHalfExtents = Zenith_Maths::Vector3(0.3f);   // radii (x,y,z), all > 0
	u_int                 m_uRings = 5u;                                   // vertical rings (>= 2)
	u_int                 m_uSegs  = 10u;
	ZM_GenUVIsland        m_xIsland;
	const char*           m_szName = "Head";
};
ZM_KitAppendResult ZM_AppendEllipsoidHead(ZM_GenMesh& xMesh, const ZM_KitHeadParams& xParams);
