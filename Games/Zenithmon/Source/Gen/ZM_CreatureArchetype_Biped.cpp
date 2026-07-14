#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Biped -- the S4 BIPED archetype builder: an upright,
// two-legged monster (rounded vertical torso + head + 2 arms + 2 legs + a small
// dorsal crest) assembled ENTIRELY from the ZM_CreatureArchetypeCommon appendage
// kit, exactly like the QUADRUPED reference builder. It composes ZM_AppendSpineTube
// (torso, the single root), ZM_AppendEllipsoidHead (head), ZM_AppendLimb x4 (2
// arms + 2 legs) and ZM_AppendHorn (the crest). The orchestrator wires this into
// ZM_GetArchetypeBuilder for ZM_ARCHETYPE_BIPED (this file does NOT touch the
// switch).
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The torso is therefore a VERTICAL egg
// standing on end: the belly/pelvis ring is at low Y (hip height) and the
// back/shoulders ring is at high Y. "Front" is +Z (the kit's ang=pi faces +Z),
// so the head projects slightly FORWARD (+Z) above the shoulders. The two arms
// drop DOWN (-Y) from the shoulders at +/-X and hang at the sides; the two legs
// drop DOWN (-Y) from the pelvis at +/-X to the ground (y==0). Everything is
// positioned in X/Z around that vertical torso egg.
//
// SKELETON (fixed topology across all evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine03, pelvis -> shoulders) parents every
// appendage. The named bones -- the spine chain, Head, the four arm bones
// (ArmLUp/Lo, ArmRUp/Lo), the four leg bones (LegLUp/Lo, LegRUp/Lo) and the single
// Crest -- are emitted in an IDENTICAL order with identical names for EVERY
// elaboration tier, so a clip authored against one stage's skeleton maps by
// name/index onto any other stage. Elaboration (recipe.m_uElaboration, 0..2) grows
// the CREST from a small nub to a full crest WITHOUT changing bone topology.
// Total bones: 4 + 1 + 4 + 4 + 1 = 14 (<= the creature cap of 30).
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
	// A non-overlapping normalized atlas island per part (SC2 uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_BipedIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (pelvis at u=0, shoulders at
	// u=count-1) so appendages can be parented at the correct parent-bone world
	// positions.
	Zenith_Maths::Vector3 ZM_BipedSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// One limb (arm or leg): a downward tapering tube from top (highest Y) through
	// mid to bottom (lowest Y). ZM_AppendLimb names the two bones "<name>Up" (top)
	// and "<name>Lo" (bottom). Front (+Z) elbow/knee/foot bias is folded into the
	// mid/bottom Z so the joint reads as bending forward.
	void ZM_BipedAppendLimb(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld,
		const Zenith_Maths::Vector3& xTop, const Zenith_Maths::Vector3& xMid,
		const Zenith_Maths::Vector3& xBottom,
		float fRadTop, float fRadMid, float fRadFoot,
		const ZM_GenUVIsland& xIsland, const char* szName)
	{
		ZM_KitLimbParams xLimb;
		xLimb.m_iParentBone  = (int)uParentBone;
		xLimb.m_xParentWorld = xParentWorld;
		xLimb.m_xHip  = xTop;
		xLimb.m_xKnee = xMid;
		xLimb.m_xFoot = xBottom;
		xLimb.m_fRadiusTop  = fRadTop;
		xLimb.m_fRadiusMid  = fRadMid;
		xLimb.m_fRadiusFoot = fRadFoot;
		xLimb.m_uSegs       = 8u;
		xLimb.m_xIsland     = xIsland;
		xLimb.m_szName      = szName;
		ZM_AppendLimb(xMesh, xLimb);
	}
}

// ---------------------------------------------------------------------------
// The BIPED builder. Signature matches the frozen ZM_CreatureGen.h declaration
// exactly (ZM_GenMesh& then const ZM_CreatureRecipe&).
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Biped(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 torso vertical span
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back thickness
	const float fBellyRoundJ = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier torso)
	const float fHeadSizeJ   = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 head scale
	const float fArmRadiusJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 6 arm thickness
	const float fLegRadiusJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 7 leg thickness

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fShoulderWidthJ = xSkelRng.NextFloatRange(0.92f, 1.12f); // 1 how wide the arms splay
	const float fHipWidthJ      = xSkelRng.NextFloatRange(0.90f, 1.12f); // 2 how wide the legs stand
	const float fHeadFwdJ       = xSkelRng.NextFloatRange(0.85f, 1.15f); // 3 how far the head leans forward

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. ----
	const float fHipY     = 0.55f * fS;                        // hip height == leg length
	const float fBodyLen  = 0.55f * fBodyLenJ * fS;            // torso vertical span (pelvis -> shoulders)
	const float fHalfWide = 0.22f * fBodyWidthJ * fS;          // torso Rx (side girth)
	const float fHalfDeep = 0.17f * fBodyDepthJ * fS;          // torso Rz (front-back thickness)
	const float fShoulderY = fHipY + fBodyLen;                 // top of the torso
	const Zenith_Maths::Vector3 xBelly(0.0f, fHipY, 0.0f);

	// ---- 1. Spine tube (creates the single root). FIXED 4 nodes for every tier. ----
	ZM_KitSpineParams xSpine;
	xSpine.m_iParentBone  = -1;                                // create the root here
	xSpine.m_xBellyCentre = xBelly;
	xSpine.m_fLength      = fBodyLen;
	xSpine.m_uSegments    = 4u;
	xSpine.m_fHalfWidth   = fHalfWide;
	xSpine.m_fHalfDepth   = fHalfDeep;
	xSpine.m_fEndTaper    = 0.60f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_BipedIsland(0.02f, 0.02f, 0.60f, 0.60f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 2u, "ZM_BuildArchetype_Biped: spine needs >= 2 nodes");

	const u_int uPelvisBone   = auSpine[0];
	const u_int uShoulderBone = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xPelvisWorld   = ZM_BipedSpineWorld(xBelly, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xShoulderWorld = ZM_BipedSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Head (ellipsoid), sitting above the shoulders and leaning forward (+Z). ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.17f * fHeadSizeJ * fS,
		0.19f * fHeadSizeJ * fS,
		0.17f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fShoulderY + xHeadHalf.y * 0.90f,
		0.05f * fHeadFwdJ * fS);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uShoulderBone;
	xHead.m_xParentWorld = xShoulderWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_BipedIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	const ZM_KitAppendResult xHeadRes = ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Two arms, FIXED draw order L, R. They parent the shoulder (top spine)
	//      bone and hang DOWN at +/-X, elbow/hand biased slightly forward (+Z). ----
	const float fShoulderX = (fHalfWide + 0.06f * fS) * fShoulderWidthJ;
	const float fArmTopY   = fShoulderY - 0.03f * fS;
	const float fArmLen    = 0.50f * fS;
	const ZM_GenUVIsland xArmIsland = ZM_BipedIsland(0.62f, 0.42f, 0.80f, 0.72f);
	const float fArmRadTop  = 0.070f * fArmRadiusJ * fS;
	const float fArmRadMid  = 0.055f * fArmRadiusJ * fS;
	const float fArmRadHand = 0.048f * fArmRadiusJ * fS;
	ZM_BipedAppendLimb(xMesh, uShoulderBone, xShoulderWorld,
		Zenith_Maths::Vector3(-fShoulderX, fArmTopY,               0.0f),
		Zenith_Maths::Vector3(-fShoulderX, fArmTopY - fArmLen*0.5f, 0.02f * fS),
		Zenith_Maths::Vector3(-fShoulderX, fArmTopY - fArmLen,      0.03f * fS),
		fArmRadTop, fArmRadMid, fArmRadHand, xArmIsland, "ArmL");
	ZM_BipedAppendLimb(xMesh, uShoulderBone, xShoulderWorld,
		Zenith_Maths::Vector3( fShoulderX, fArmTopY,               0.0f),
		Zenith_Maths::Vector3( fShoulderX, fArmTopY - fArmLen*0.5f, 0.02f * fS),
		Zenith_Maths::Vector3( fShoulderX, fArmTopY - fArmLen,      0.03f * fS),
		fArmRadTop, fArmRadMid, fArmRadHand, xArmIsland, "ArmR");

	// ---- 4. Two legs, FIXED draw order L, R. They parent the pelvis (root spine)
	//      bone and drop to the ground (y==0), knee/foot biased slightly forward. ----
	const float fHipX = 0.10f * fHipWidthJ * fS;
	const ZM_GenUVIsland xLegIsland = ZM_BipedIsland(0.80f, 0.42f, 0.98f, 0.72f);
	const float fLegRadTop  = 0.100f * fLegRadiusJ * fS;
	const float fLegRadMid  = 0.085f * fLegRadiusJ * fS;
	const float fLegRadFoot = 0.070f * fLegRadiusJ * fS;
	ZM_BipedAppendLimb(xMesh, uPelvisBone, xPelvisWorld,
		Zenith_Maths::Vector3(-fHipX, fHipY,       0.0f),
		Zenith_Maths::Vector3(-fHipX, fHipY * 0.5f, 0.02f * fS),
		Zenith_Maths::Vector3(-fHipX, 0.0f,         0.04f * fS),
		fLegRadTop, fLegRadMid, fLegRadFoot, xLegIsland, "LegL");
	ZM_BipedAppendLimb(xMesh, uPelvisBone, xPelvisWorld,
		Zenith_Maths::Vector3( fHipX, fHipY,       0.0f),
		Zenith_Maths::Vector3( fHipX, fHipY * 0.5f, 0.02f * fS),
		Zenith_Maths::Vector3( fHipX, 0.0f,         0.04f * fS),
		fLegRadTop, fLegRadMid, fLegRadFoot, xLegIsland, "LegR");

	// ---- 5. Dorsal crest on the head, sweeping straight UP. Grows with the
	//      elaboration tier (0 -> nub, 2 -> full crest) but ALWAYS present, so the
	//      bone topology stays fixed across evo stages. ----
	const u_int  uTier     = xRecipe.m_uElaboration;            // 0, 1, or 2
	const float  fCrestLen = (0.04f + 0.09f * (float)uTier) * fHeadSizeJ * fS;
	const float  fCrestRad = (0.020f + 0.012f * (float)uTier) * fS;
	const Zenith_Maths::Vector3 xCrestBase(
		xHeadCentre.x,
		xHeadCentre.y + xHeadHalf.y * 0.85f,
		xHeadCentre.z - xHeadHalf.z * 0.10f);
	ZM_KitHornParams xCrest;
	xCrest.m_iParentBone  = (int)xHeadRes.m_uTipBone;
	xCrest.m_xParentWorld = xHeadCentre;
	xCrest.m_xBase = xCrestBase;
	xCrest.m_xTip  = Zenith_Maths::Vector3(
		xCrestBase.x,
		xCrestBase.y + fCrestLen,
		xCrestBase.z - 0.02f * fS);
	xCrest.m_fRadiusBase = fCrestRad;
	xCrest.m_uSegs       = 6u;
	xCrest.m_xIsland     = ZM_BipedIsland(0.02f, 0.62f, 0.30f, 0.98f);
	xCrest.m_szName      = "Crest";
	ZM_AppendHorn(xMesh, xCrest);

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Biped: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
