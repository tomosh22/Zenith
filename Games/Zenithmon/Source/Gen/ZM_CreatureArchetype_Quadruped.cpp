#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Quadruped -- the S4 (SC1) reference archetype builder: a
// convincing generic quadruped (rounded torso + head + 4 legs + tail + antler-
// style horns) assembled ENTIRELY from the ZM_CreatureArchetypeCommon appendage
// kit, so every kit helper (ZM_AppendSpineTube / ZM_AppendEllipsoidHead /
// ZM_AppendLimb / ZM_AppendTail / ZM_AppendHorn) is exercised. This is the sole
// wired builder in SC1 (ZM_GetArchetypeBuilder returns it only for QUADRUPED).
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The torso is therefore a VERTICAL
// egg (belly at low Y, back/withers at high Y); the animal's front->back extent
// is the torso DEPTH (Rz) and its side girth is the torso WIDTH (Rx). Everything
// else is positioned in X/Z around that egg: the head projects FORWARD (+Z), the
// tail projects BACKWARD (-Z) and droops, and the four legs drop DOWN (-Y) to the
// ground at +/-X, front legs at +Z and hind legs at -Z. "Front" is +Z, matching
// the kit's ring angle convention (ang=pi faces +Z).
//
// SKELETON (fixed topology across all evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine03, the pelvis/back) parents every
// appendage. The CORE named bones -- the spine chain, Head, the eight leg bones
// (LegFLUp/Lo, LegFRUp/Lo, LegHLUp/Lo, LegHRUp/Lo), and Tail00..Tail02 -- plus
// the two horn bones (HornL, HornR) are emitted in an IDENTICAL order and with
// identical names for every elaboration tier, so a clip authored against one
// stage's skeleton maps by name/index onto any other stage. Elaboration
// (recipe.m_uElaboration, 0..2) grows the HORNS from small nubs to full antlers
// WITHOUT changing bone topology -- the strongest reading of the clip-transfer
// invariant. Total bones: 4 + 1 + 8 + 3 + 2 = 18 (<= the creature cap of 30).
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
	// A non-overlapping normalized atlas island per part (SC1 uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_QuadIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (belly at u=0, back at u=count-1) so
	// appendages can be parented at the correct parent-bone world positions.
	Zenith_Maths::Vector3 ZM_QuadSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// One leg, in the fixed FL,FR,HL,HR draw order. Straight vertical tube from the
	// hip (belly height) to the foot (ground); front legs sit at +Z, hind at -Z.
	void ZM_QuadAppendLeg(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, float fX, float fZ,
		float fBellyY, float fRadiusScale, float fSizeScale, const char* szName)
	{
		ZM_KitLimbParams xLimb;
		xLimb.m_iParentBone  = (int)uParentBone;
		xLimb.m_xParentWorld = xParentWorld;
		xLimb.m_xHip  = Zenith_Maths::Vector3(fX, fBellyY,         fZ);
		xLimb.m_xKnee = Zenith_Maths::Vector3(fX, fBellyY * 0.5f,  fZ);
		xLimb.m_xFoot = Zenith_Maths::Vector3(fX, 0.0f,            fZ);
		xLimb.m_fRadiusTop  = 0.090f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusMid  = 0.075f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusFoot = 0.060f * fRadiusScale * fSizeScale;
		xLimb.m_uSegs       = 8u;
		xLimb.m_xIsland     = ZM_QuadIsland(0.62f, 0.42f, 0.98f, 0.72f);
		xLimb.m_szName      = szName;
		ZM_AppendLimb(xMesh, xLimb);
	}

	// One horn/antler on the head bone, sweeping straight up (tip above base). Grows
	// with elaboration tier; present at every tier so bone topology stays fixed.
	void ZM_QuadAppendHorn(ZM_GenMesh& xMesh, u_int uHeadBone,
		const Zenith_Maths::Vector3& xHeadCentre, const Zenith_Maths::Vector3& xHeadHalf,
		float fSide, float fHornLen, float fHornRad, float fSizeScale, const char* szName)
	{
		const Zenith_Maths::Vector3 xBase(
			xHeadCentre.x + fSide * xHeadHalf.x * 0.5f,
			xHeadCentre.y + xHeadHalf.y * 0.65f,
			xHeadCentre.z - xHeadHalf.z * 0.15f);

		ZM_KitHornParams xHorn;
		xHorn.m_iParentBone  = (int)uHeadBone;
		xHorn.m_xParentWorld = xHeadCentre;
		xHorn.m_xBase = xBase;
		xHorn.m_xTip  = Zenith_Maths::Vector3(
			xBase.x + fSide * 0.02f * fSizeScale,
			xBase.y + fHornLen,
			xBase.z - 0.03f * fSizeScale);
		xHorn.m_fRadiusBase = fHornRad;
		xHorn.m_uSegs       = 6u;
		xHorn.m_xIsland     = ZM_QuadIsland(0.02f, 0.62f, 0.30f, 0.98f);
		xHorn.m_szName      = szName;
		ZM_AppendHorn(xMesh, xHorn);
	}
}

// ---------------------------------------------------------------------------
// The QUADRUPED reference builder. Signature matches the frozen ZM_CreatureGen.h
// declaration exactly (ZM_GenMesh& then const ZM_CreatureRecipe&).
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Quadruped(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 torso vertical span
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back length
	const float fBellyRoundJ = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier belly)
	const float fHeadSizeJ   = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 head scale
	const float fLegRadiusJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 6 leg thickness
	const float fTailLenJ    = xMeshRng.NextFloatRange(0.85f, 1.15f);   // 7 tail reach

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fLegSplayJ = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 1 how far legs splay in X
	const float fHeadFwdJ  = xSkelRng.NextFloatRange(0.92f, 1.10f);     // 2 how far the head projects forward

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. ----
	const float fBellyY   = 0.45f * fS;                        // belly height == leg length
	const float fBodyLen  = 0.45f * fBodyLenJ * fS;            // torso vertical span (belly -> back)
	const float fHalfWide = 0.26f * fBodyWidthJ * fS;          // torso Rx (side girth)
	const float fHalfDeep = 0.40f * fBodyDepthJ * fS;          // torso Rz (front-back length)
	const Zenith_Maths::Vector3 xBelly(0.0f, fBellyY, 0.0f);

	// ---- 1. Spine tube (creates the single root). FIXED 4 nodes for every tier. ----
	ZM_KitSpineParams xSpine;
	xSpine.m_iParentBone  = -1;                                // create the root here
	xSpine.m_xBellyCentre = xBelly;
	xSpine.m_fLength      = fBodyLen;
	xSpine.m_uSegments    = 4u;
	xSpine.m_fHalfWidth   = fHalfWide;
	xSpine.m_fHalfDepth   = fHalfDeep;
	xSpine.m_fEndTaper    = 0.55f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_QuadIsland(0.02f, 0.02f, 0.60f, 0.60f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 2u, "ZM_BuildArchetype_Quadruped: spine needs >= 2 nodes");

	const u_int uPelvisBone = auSpine[0];
	const u_int uChestBone  = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xPelvisWorld = ZM_QuadSpineWorld(xBelly, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xChestWorld  = ZM_QuadSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Head (ellipsoid), projecting forward (+Z) from the chest/withers. ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.20f * fHeadSizeJ * fS,
		0.18f * fHeadSizeJ * fS,
		0.22f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fBellyY + fBodyLen * 0.75f,
		0.50f * fHeadFwdJ * fS);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uChestBone;
	xHead.m_xParentWorld = xChestWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_QuadIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	const ZM_KitAppendResult xHeadRes = ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Four legs, FIXED draw order FL, FR, HL, HR. Front legs parent the
	//      chest (+Z), hind legs the pelvis (-Z). ----
	const float fSplayX = 0.20f * fLegSplayJ * fS;
	const float fFrontZ =  0.26f * fS;
	const float fHindZ  = -0.26f * fS;
	ZM_QuadAppendLeg(xMesh, uChestBone,  xChestWorld,  -fSplayX, fFrontZ, fBellyY, fLegRadiusJ, fS, "LegFL");
	ZM_QuadAppendLeg(xMesh, uChestBone,  xChestWorld,   fSplayX, fFrontZ, fBellyY, fLegRadiusJ, fS, "LegFR");
	ZM_QuadAppendLeg(xMesh, uPelvisBone, xPelvisWorld, -fSplayX, fHindZ,  fBellyY, fLegRadiusJ, fS, "LegHL");
	ZM_QuadAppendLeg(xMesh, uPelvisBone, xPelvisWorld,  fSplayX, fHindZ,  fBellyY, fLegRadiusJ, fS, "LegHR");

	// ---- 4. Tail: from the rump (back-top, -Z) sweeping down and back. ----
	ZM_KitTailParams xTail;
	xTail.m_iParentBone  = (int)uChestBone;
	xTail.m_xParentWorld = xChestWorld;
	xTail.m_xBase = Zenith_Maths::Vector3(0.0f, fBellyY + fBodyLen * 0.60f, -0.30f * fS);
	xTail.m_xTip  = Zenith_Maths::Vector3(0.0f, fBellyY * 0.55f, -(0.30f + 0.35f * fTailLenJ) * fS);
	xTail.m_uSegments   = 3u;
	xTail.m_fRadiusBase = 0.070f * fS;
	xTail.m_fRadiusTip  = 0.020f * fS;
	xTail.m_uSegs       = 7u;
	xTail.m_xIsland     = ZM_QuadIsland(0.62f, 0.74f, 0.98f, 0.98f);
	xTail.m_szNamePrefix = "Tail";
	ZM_AppendTail(xMesh, xTail);

	// ---- 5. Horns/antlers on the head, FIXED order L, R. Grow with elaboration
	//      tier (0 -> nub, 2 -> full antler) but ALWAYS present -> fixed topology. ----
	const u_int  uTier    = xRecipe.m_uElaboration;             // 0, 1, or 2
	const float  fHornLen = (0.05f + 0.10f * (float)uTier) * fHeadSizeJ * fS;
	const float  fHornRad = (0.025f + 0.015f * (float)uTier) * fS;
	ZM_QuadAppendHorn(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf, -1.0f, fHornLen, fHornRad, fS, "HornL");
	ZM_QuadAppendHorn(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf,  1.0f, fHornLen, fHornRad, fS, "HornR");

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Quadruped: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
