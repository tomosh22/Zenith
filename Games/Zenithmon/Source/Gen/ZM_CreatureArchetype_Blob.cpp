#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Blob -- the S4 (SC4) BLOB archetype builder: an amorphous
// gelatinous creature that is essentially ONE rounded body -- the LOW-BONE
// extreme of the roster. There are NO distinct limbs / head / tail as separate
// long appendages: the whole silhouette is a single soft box-rounded blob with,
// at most, a tiny eye-stalk nub bump on top. Assembled ENTIRELY from the
// ZM_CreatureArchetypeCommon appendage kit (ZM_AppendSpineTube for the body +
// ZM_AppendHorn for the nub), mirroring the QUADRUPED / AVIAN reference builders'
// structure exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The body is a VERTICAL blob (belly at
// low Y, crown at high Y), rings fullest at the middle node so the silhouette
// reads as a rounded dome rather than a barrel. The body uses the ZM_LoftRing
// super-ellipse field (m_fBellyRound < 1) to round the cross-section TOWARD a box,
// giving the soft, slightly-squared gelatinous look. The single nub is a small
// upward ZM_AppendHorn cone rising from the crown (an eye-stalk / antenna),
// centred on X and slightly forward (+Z). "Front" is +Z (the kit's ring angle
// convention: ang=pi faces +Z).
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine02) that IS the body, plus ONE crown
// nub bone ("Nub"). The bone set -- Spine00, Spine01, Spine02, Nub -- is emitted
// in an IDENTICAL order with identical names for EVERY elaboration tier.
// Elaboration (recipe.m_uElaboration, 0..2) grows ONLY the nub's reach/girth
// (size, never topology); it NEVER adds/removes/reorders a bone, and there are
// NO limbs (no "...Up" bones -- a blob has no legs). Total bones: 3 + 1 = 4,
// the low-bone extreme (asserted in [2,4]; well under the creature cap of 30).
//
// DETERMINISM (AssetManifest 6.2): the ONLY randomness sources are
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_MESH) for geometry proportion jitter and
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_SKELETON) for nub-placement jitter. Every
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
	ZM_GenUVIsland ZM_BlobIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (belly at u=0, crown at u=count-1) so
	// the nub can be parented at the correct parent-bone world position.
	Zenith_Maths::Vector3 ZM_BlobSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}
}

// ---------------------------------------------------------------------------
// The BLOB builder. Signature matches the frozen ZM_CreatureGen.h declaration
// exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the dispatch
// switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Blob(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;
	const u_int uTier = xRecipe.m_uElaboration;               // 0, 1, or 2

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 body vertical span
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back girth
	const float fBoxRoundJ   = xMeshRng.NextFloatRange(0.60f, 0.82f);   // 4 super-ellipse (<1 rounds toward a box)
	const float fNubSizeJ    = xMeshRng.NextFloatRange(0.85f, 1.20f);   // 5 nub reach/girth

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fNubFwdJ  = xSkelRng.NextFloatRange(0.90f, 1.10f);      // 1 how far the nub sits forward (+Z)
	const float fNubTiltJ = xSkelRng.NextFloatRange(0.90f, 1.10f);      // 2 how far the nub tip leans forward

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. The
	//      body is roughly as wide/deep as it is tall -- a squat gelatinous dome. ----
	const float fBellyY   = 0.06f * fS;                        // lowest ring height (sits near ground)
	const float fBodyLen  = 0.46f * fBodyLenJ * fS;            // body vertical span (belly -> crown)
	const float fHalfWide = 0.34f * fBodyWidthJ * fS;          // body Rx (side girth)
	const float fHalfDeep = 0.34f * fBodyDepthJ * fS;          // body Rz (front-back girth)
	const Zenith_Maths::Vector3 xBelly(0.0f, fBellyY, 0.0f);

	// ---- 1. Body: a single rounded spine tube (creates the single root). FIXED 3
	//      nodes for every tier so the mid node bulges the silhouette into a dome;
	//      the super-ellipse (< 1) softly squares the cross-section. ----
	ZM_KitSpineParams xBody;
	xBody.m_iParentBone  = -1;                                 // create the root here
	xBody.m_xBellyCentre = xBelly;
	xBody.m_fLength      = fBodyLen;
	xBody.m_uSegments    = 3u;
	xBody.m_fHalfWidth   = fHalfWide;
	xBody.m_fHalfDepth   = fHalfDeep;
	xBody.m_fEndTaper    = 0.62f;                              // fat, rounded ends (not a pinched barrel)
	xBody.m_fBellyRound  = fBoxRoundJ;                         // super-ellipse: soft box-rounding
	xBody.m_uSegs        = 12u;
	xBody.m_xIsland      = ZM_BlobIsland(0.02f, 0.02f, 0.72f, 0.72f);
	xBody.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xBody, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 2u, "ZM_BuildArchetype_Blob: body needs >= 2 nodes");

	const u_int uCrownBone = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xCrownWorld =
		ZM_BlobSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Crown nub: one tiny upward eye-stalk / antenna cone rising from the
	//      top of the body, centred on X and slightly forward (+Z). Grows with the
	//      elaboration tier (size ONLY -- always present so bone topology is fixed).
	//      ZM_AppendHorn sweeps UP and tapers to a near-zero tip, giving the nub. ----
	const Zenith_Maths::Vector3 xNubBase(
		0.0f,
		fBellyY + fBodyLen * 0.88f,                            // just below the crown surface
		0.03f * fNubFwdJ * fS);
	const float fNubLen = (0.05f + 0.03f * (float)uTier) * fNubSizeJ * fS;
	ZM_KitHornParams xNub;
	xNub.m_iParentBone  = (int)uCrownBone;
	xNub.m_xParentWorld = xCrownWorld;
	xNub.m_xBase = xNubBase;
	xNub.m_xTip  = Zenith_Maths::Vector3(
		xNubBase.x,
		xNubBase.y + fNubLen,                                  // tip.y > base.y (horn precondition)
		xNubBase.z + 0.02f * fNubTiltJ * fS);
	xNub.m_fRadiusBase = (0.045f + 0.010f * (float)uTier) * fNubSizeJ * fS;
	xNub.m_uSegs       = 6u;
	xNub.m_xIsland     = ZM_BlobIsland(0.74f, 0.02f, 0.98f, 0.40f);
	xNub.m_szName      = "Nub";
	ZM_AppendHorn(xMesh, xNub);

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() >= 2u && xMesh.GetNumBones() <= 4u,
		"ZM_BuildArchetype_Blob: bone count %u outside the blob low-bone range [2,4]",
		xMesh.GetNumBones());
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Blob: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
