#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Serpent -- the S4 (SC3) SERPENT archetype builder: a long,
// limbless, tapering snake/eel-like creature (a many-segment body column that
// narrows to a tail tip, topped by a head with small brow horns/frills) assembled
// ENTIRELY from the ZM_CreatureArchetypeCommon appendage kit -- NO archetype-local
// loft helper is needed, because every serpent shape (body tube, tail cone, head
// ellipsoid, brow horns) is a round Y-swept part the kit already expresses.
// Mirrors the QUADRUPED / AVIAN reference builders' structure exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY, so a horizontal ground-lying snake is
// not expressible; the serpent is posed as an UPRIGHT / rearing column of rings.
// The body is a vertical tube (belly/coil-base at low Y, neck at high Y), fullest
// through the middle and slender overall; "front" is +Z (the kit's ang=pi ring
// convention). The head sits above the neck and projects its snout FORWARD (+Z);
// the tail sweeps DOWN (-Y) from the coil-base, tapering smoothly to a fine point;
// the two small brow horns sit on the head, angled up and slightly out (+/-X).
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine05 -- SIX vertebrae, the many-node body
// a serpent needs) parents the head and the tail. The bone set --
// Spine00..Spine05, Head, Tail00..Tail02, and the two brow horns HornL / HornR --
// is emitted in an IDENTICAL order with identical names for EVERY elaboration
// tier. Elaboration (recipe.m_uElaboration, 0..2) grows ONLY the brow horns from
// small nubs to full frills; it NEVER adds/removes/reorders a bone. There are NO
// arms and NO legs (zero limb "...Up" bones). Total bones: 6 + 1 + 3 + 2 = 12
// (<= the creature cap of 30).
//
// DETERMINISM (AssetManifest 6.2): the ONLY randomness sources are
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_MESH) for geometry proportion jitter and
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_SKELETON) for bone-placement jitter. Every
// draw is taken UP FRONT in a FIXED order (all MESH draws, then all SKELETON
// draws) so the stream is order-stable regardless of build sequencing. No clock /
// pointer / global-RNG / container-iteration entropy. All extents scale by
// recipe.m_fSizeScale.
//
// This builder only APPENDS mesh + skeleton (the driver ZM_BuildCreatureMesh runs
// the single finalise pass -- tangents + weight-normalise); it must NOT finalise
// or bake, and it leaves the loft's analytic ring normals untouched (it never
// sculpts vertex positions directly, so it never regenerates normals).
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"             // ZM_CreatureRecipe, ZM_MakeGenRNG, ZM_GEN_DOMAIN_*
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"  // the appendage kit

namespace
{
	// A non-overlapping normalized atlas island per part (SC-scope uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_SerpentIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (coil-base at u=0, neck at u=count-1) so
	// appendages can be parented at the correct parent-bone world positions.
	Zenith_Maths::Vector3 ZM_SerpentSpineWorld(const Zenith_Maths::Vector3& xBase,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBase.x, xBase.y + fT * fBodyLen, xBase.z);
	}

	// One brow horn/frill on the head bone, sweeping straight up (tip above base).
	// Grows with elaboration tier; PRESENT at every tier so bone topology stays fixed
	// (the serpent equivalent of the quadruped's antler pair). Base sits on the upper
	// head, angled slightly outward (+/-X) and back (-Z).
	void ZM_SerpentAppendBrow(ZM_GenMesh& xMesh, u_int uHeadBone,
		const Zenith_Maths::Vector3& xHeadCentre, const Zenith_Maths::Vector3& xHeadHalf,
		float fSide, float fSplay, float fHornLen, float fHornRad, float fSizeScale,
		const char* szName)
	{
		const Zenith_Maths::Vector3 xBase(
			xHeadCentre.x + fSide * xHeadHalf.x * 0.55f * fSplay,
			xHeadCentre.y + xHeadHalf.y * 0.55f,
			xHeadCentre.z - xHeadHalf.z * 0.20f);

		ZM_KitHornParams xHorn;
		xHorn.m_iParentBone  = (int)uHeadBone;
		xHorn.m_xParentWorld = xHeadCentre;
		xHorn.m_xBase = xBase;
		xHorn.m_xTip  = Zenith_Maths::Vector3(
			xBase.x + fSide * 0.03f * fSizeScale,
			xBase.y + fHornLen,
			xBase.z - 0.02f * fSizeScale);
		xHorn.m_fRadiusBase = fHornRad;
		xHorn.m_uSegs       = 6u;
		xHorn.m_xIsland     = ZM_SerpentIsland(0.02f, 0.62f, 0.30f, 0.98f);
		xHorn.m_szName      = szName;
		ZM_AppendHorn(xMesh, xHorn);
	}
}

// ---------------------------------------------------------------------------
// The SERPENT builder. Signature matches the frozen ZM_CreatureGen.h declaration
// exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the dispatch
// switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Serpent(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.15f);   // 1 body vertical span
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 2 side girth
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.14f);   // 3 front-back girth
	const float fBellyRoundJ = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier body)
	const float fHeadSizeJ   = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 head scale
	const float fTailLenJ    = xMeshRng.NextFloatRange(0.90f, 1.20f);   // 6 tail reach
	const float fHornSizeJ   = xMeshRng.NextFloatRange(0.88f, 1.14f);   // 7 brow-horn scale

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fHeadFwdJ   = xSkelRng.NextFloatRange(0.92f, 1.10f);    // 1 how far the head projects forward
	const float fHornSplayJ = xSkelRng.NextFloatRange(0.90f, 1.12f);    // 2 how far the brow horns splay in X

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. A snake
	//      is slender (small girth) and tall (long body + long tapering tail). ----
	const float fBaseY    = 0.55f * fS;                        // coil-base height (tail drops below to ~0)
	const float fBodyLen  = 0.70f * fBodyLenJ * fS;            // body vertical span (base -> neck)
	const float fHalfWide = 0.125f * fBodyWidthJ * fS;         // body Rx (side girth)
	const float fHalfDeep = 0.150f * fBodyDepthJ * fS;         // body Rz (front-back girth)
	const Zenith_Maths::Vector3 xBase(0.0f, fBaseY, 0.0f);
	const u_int uTier = xRecipe.m_uElaboration;                // 0, 1, or 2

	// ---- 1. Body spine tube (creates the single root). FIXED 6 vertebrae for every
	//      tier -- the many-node chain a serpent needs. Both ends taper so the neck
	//      narrows toward the head and the coil-base blends into the tail. ----
	ZM_KitSpineParams xSpine;
	xSpine.m_iParentBone  = -1;                                // create the root here
	xSpine.m_xBellyCentre = xBase;
	xSpine.m_fLength      = fBodyLen;
	xSpine.m_uSegments    = 6u;
	xSpine.m_fHalfWidth   = fHalfWide;
	xSpine.m_fHalfDepth   = fHalfDeep;
	xSpine.m_fEndTaper    = 0.70f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_SerpentIsland(0.02f, 0.02f, 0.60f, 0.60f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 2u, "ZM_BuildArchetype_Serpent: spine needs >= 2 nodes");

	const u_int uCoilBone = auSpine[0];                        // coil-base (root, lowest Y)
	const u_int uNeckBone = auSpine[uSpineCount - 1u];         // neck (highest Y)
	const Zenith_Maths::Vector3 xCoilWorld = ZM_SerpentSpineWorld(xBase, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xNeckWorld = ZM_SerpentSpineWorld(xBase, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Head (ellipsoid), above the neck with the snout projecting forward (+Z).
	//      Elongated in Z for a serpent's tapered muzzle. ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.12f * fHeadSizeJ * fS,
		0.11f * fHeadSizeJ * fS,
		0.17f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fBaseY + fBodyLen + xHeadHalf.y * 0.60f,
		0.06f * fHeadFwdJ * fS);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uNeckBone;
	xHead.m_xParentWorld = xNeckWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_SerpentIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	const ZM_KitAppendResult xHeadRes = ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Tail: from the coil-base sweeping straight DOWN and tapering to a fine
	//      point, on THREE bones (Tail00..Tail02) -- the smooth thick-to-thin serpent
	//      taper. FIXED topology; reach jitters but never changes the bone count. ----
	const float fTailDrop = (fBaseY + 0.28f * fTailLenJ * fS);   // reaches below the ground plane to a point
	ZM_KitTailParams xTail;
	xTail.m_iParentBone  = (int)uCoilBone;
	xTail.m_xParentWorld = xCoilWorld;
	xTail.m_xBase = Zenith_Maths::Vector3(0.0f, fBaseY, 0.0f);
	xTail.m_xTip  = Zenith_Maths::Vector3(0.0f, fBaseY - fTailDrop, -0.05f * fS);
	xTail.m_uSegments   = 3u;
	xTail.m_fRadiusBase = fHalfWide * 0.85f;                     // matches the body's tapered coil-base girth
	xTail.m_fRadiusTip  = 0.015f * fS;
	xTail.m_uSegs       = 8u;
	xTail.m_xIsland     = ZM_SerpentIsland(0.62f, 0.74f, 0.98f, 0.98f);
	xTail.m_szNamePrefix = "Tail";
	ZM_AppendTail(xMesh, xTail);

	// ---- 4. Two brow horns/frills on the head, FIXED order L, R. Grow with the
	//      elaboration tier (0 -> nub, 2 -> full frill) but ALWAYS present -> fixed
	//      topology. The serpent has NO limbs, so these are its only head decoration. ----
	const float fHornLen = (0.045f + 0.075f * (float)uTier) * fHornSizeJ * fS;
	const float fHornRad = (0.022f + 0.012f * (float)uTier) * fHornSizeJ * fS;
	ZM_SerpentAppendBrow(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf, -1.0f, fHornSplayJ, fHornLen, fHornRad, fS, "HornL");
	ZM_SerpentAppendBrow(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf,  1.0f, fHornSplayJ, fHornLen, fHornRad, fS, "HornR");

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Serpent: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
