#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Insectoid -- the S4 (SC4) INSECTOID archetype builder: a
// bug (a SEGMENTED thorax->abdomen body + a head with two antennae + SIX legs,
// three per side) assembled ENTIRELY from the ZM_CreatureArchetypeCommon
// appendage kit -- NO archetype-local loft helper is needed, because every
// insect shape (segmented body tube, head ellipsoid, round leg tubes, tapering
// antennae) is a round Y-swept part the kit already expresses. Mirrors the
// QUADRUPED / AVIAN reference builders' structure exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The body is therefore a VERTICAL
// egg (abdomen/belly at low Y, thorax/back at high Y), fullest through the
// middle; "front" is +Z (the kit's ang=pi ring convention). The head projects
// FORWARD (+Z) from the thorax; the two antennae sit on the head and sweep UP
// and slightly forward; the six legs drop DOWN (-Y) to the ground, splayed out
// in +/-X and staggered in Z (a front / mid / rear row on each side).
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine03 -- FOUR body segments) parents the
// head and all six legs. The bone set -- Spine00..Spine03, Head, the twelve leg
// bones (LegL0Up/Lo, LegL1Up/Lo, LegL2Up/Lo, LegR0Up/Lo, LegR1Up/Lo, LegR2Up/Lo)
// and the two antennae AntennaL / AntennaR -- is emitted in an IDENTICAL order
// with identical names for EVERY elaboration tier. This is the HIGHEST-limb-count
// archetype, so bones are budgeted carefully: 4 + 1 + 12 + 2 = 19 (comfortably
// <= the creature cap of 30). Elaboration (recipe.m_uElaboration, 0..2) grows
// ONLY the antennae reach and the body segment size; it NEVER adds/removes/
// reorders a bone.
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
	ZM_GenUVIsland ZM_InsectIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (abdomen at u=0, thorax at u=count-1)
	// so appendages can be parented at the correct parent-bone world positions.
	Zenith_Maths::Vector3 ZM_InsectSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// One leg (a thin round tube on two bones, "<name>Up"/"<name>Lo"), splaying out
	// from the body side (hip) through a bent knee to the ground (foot). Called in
	// the fixed L0,L1,L2,R0,R1,R2 draw order; each supplies the '...Up' leg-root
	// bone the structural gate counts.
	void ZM_InsectAppendLeg(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, float fSide, float fZ,
		float fHipX, float fKneeX, float fFootX, float fBellyY,
		float fRadiusScale, float fSizeScale, const char* szName)
	{
		ZM_KitLimbParams xLimb;
		xLimb.m_iParentBone  = (int)uParentBone;
		xLimb.m_xParentWorld = xParentWorld;
		xLimb.m_xHip  = Zenith_Maths::Vector3(fSide * fHipX,  fBellyY,         fZ);
		xLimb.m_xKnee = Zenith_Maths::Vector3(fSide * fKneeX, fBellyY * 0.55f, fZ);
		xLimb.m_xFoot = Zenith_Maths::Vector3(fSide * fFootX, 0.0f,            fZ);
		xLimb.m_fRadiusTop  = 0.045f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusMid  = 0.035f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusFoot = 0.022f * fRadiusScale * fSizeScale;
		xLimb.m_uSegs       = 6u;
		xLimb.m_xIsland     = ZM_InsectIsland(0.62f, 0.42f, 0.98f, 0.72f);
		xLimb.m_szName      = szName;
		ZM_AppendLimb(xMesh, xLimb);
	}

	// One antenna (a thin upward cone on ONE bone) rooted on the head, sweeping UP
	// and slightly forward + out. Called in the fixed L,R order; grows with the
	// elaboration tier (size only, never topology). Uses the horn kit (which sweeps
	// UP: base low, tip high).
	void ZM_InsectAppendAntenna(ZM_GenMesh& xMesh, u_int uHeadBone,
		const Zenith_Maths::Vector3& xHeadCentre, const Zenith_Maths::Vector3& xHeadHalf,
		float fSide, float fLength, float fRadius, float fSizeScale, const char* szName)
	{
		const Zenith_Maths::Vector3 xBase(
			xHeadCentre.x + fSide * xHeadHalf.x * 0.45f,
			xHeadCentre.y + xHeadHalf.y * 0.55f,
			xHeadCentre.z + xHeadHalf.z * 0.35f);

		ZM_KitHornParams xAnt;
		xAnt.m_iParentBone  = (int)uHeadBone;
		xAnt.m_xParentWorld = xHeadCentre;
		xAnt.m_xBase = xBase;
		xAnt.m_xTip  = Zenith_Maths::Vector3(
			xBase.x + fSide * 0.06f * fSizeScale,
			xBase.y + fLength,
			xBase.z + 0.05f * fSizeScale);
		xAnt.m_fRadiusBase = fRadius;
		xAnt.m_uSegs       = 5u;
		xAnt.m_xIsland     = ZM_InsectIsland(0.02f, 0.62f, 0.30f, 0.98f);
		xAnt.m_szName      = szName;
		ZM_AppendHorn(xMesh, xAnt);
	}
}

// ---------------------------------------------------------------------------
// The INSECTOID builder. Signature matches the frozen ZM_CreatureGen.h
// declaration exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the
// dispatch switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Insectoid(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 body vertical span
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back length
	const float fBellyRoundJ = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier segment)
	const float fHeadSizeJ   = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 head scale
	const float fLegRadiusJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 6 leg thickness
	const float fAntennaLenJ = xMeshRng.NextFloatRange(0.85f, 1.20f);   // 7 antenna reach

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fLegSplayJ = xSkelRng.NextFloatRange(0.90f, 1.15f);     // 1 how far legs splay in X
	const float fHeadFwdJ  = xSkelRng.NextFloatRange(0.92f, 1.10f);     // 2 how far the head projects forward
	const float fLegRowZJ  = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 3 front/rear leg row Z spread

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. Bugs
	//      sit low (short legs) with an elongated front-back body. ----
	const float fBellyY   = 0.24f * fS;                        // belly height == leg length
	const float fBodyLen  = 0.42f * fBodyLenJ * fS;            // body vertical span (abdomen -> thorax)
	const float fHalfWide = 0.19f * fBodyWidthJ * fS;          // body Rx (side girth)
	const float fHalfDeep = 0.30f * fBodyDepthJ * fS;          // body Rz (front-back length)
	const Zenith_Maths::Vector3 xBelly(0.0f, fBellyY, 0.0f);
	const u_int uTier = xRecipe.m_uElaboration;                // 0, 1, or 2

	// ---- 1. Segmented body tube (creates the single root). FIXED 4 segments for
	//      every tier -- the thorax (upper nodes) + abdomen (lower nodes). Segment
	//      size grows a touch with elaboration tier (size only, topology fixed). ----
	const float fSegSize = 1.0f + 0.06f * (float)uTier;
	ZM_KitSpineParams xSpine;
	xSpine.m_iParentBone  = -1;                                // create the root here
	xSpine.m_xBellyCentre = xBelly;
	xSpine.m_fLength      = fBodyLen;
	xSpine.m_uSegments    = 4u;
	xSpine.m_fHalfWidth   = fHalfWide * fSegSize;
	xSpine.m_fHalfDepth   = fHalfDeep * fSegSize;
	xSpine.m_fEndTaper    = 0.50f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_InsectIsland(0.02f, 0.02f, 0.60f, 0.60f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 4u, "ZM_BuildArchetype_Insectoid: spine needs >= 4 nodes");

	const u_int uThoraxBone = auSpine[uSpineCount - 1u];        // front (head end)
	const u_int uMidBone    = auSpine[uSpineCount - 2u];
	const u_int uRearBone   = auSpine[uSpineCount - 3u];
	const Zenith_Maths::Vector3 xThoraxWorld = ZM_InsectSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);
	const Zenith_Maths::Vector3 xMidWorld    = ZM_InsectSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 2u);
	const Zenith_Maths::Vector3 xRearWorld   = ZM_InsectSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 3u);

	// ---- 2. Head (ellipsoid), projecting forward (+Z) from the thorax front. ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.14f * fHeadSizeJ * fS,
		0.13f * fHeadSizeJ * fS,
		0.15f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fBellyY + fBodyLen * 0.85f,
		(fHalfDeep * 0.60f + 0.08f * fHeadFwdJ) * fS);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uThoraxBone;
	xHead.m_xParentWorld = xThoraxWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_InsectIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	const ZM_KitAppendResult xHeadRes = ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Six legs (three rows -- front / mid / rear -- on each side), FIXED
	//      draw order L0,L1,L2,R0,R1,R2. Each row parents a distinct thorax/mid
	//      spine bone; legs splay out in X (a spider-like stance) and stagger in Z.
	//      These twelve bones (six "...Up" roots) are the archetype's limb bones. ----
	const float fHipX   = fHalfWide * 0.85f;
	const float fKneeX  = fHalfWide * (1.45f * fLegSplayJ);
	const float fFootX  = fHalfWide * (1.70f * fLegSplayJ);
	const float fRowZ   = fHalfDeep * 0.55f * fLegRowZJ;       // front row +Z, rear row -Z

	const u_int auRowParent[3] = { uThoraxBone, uMidBone, uRearBone };
	const Zenith_Maths::Vector3 axRowParentWorld[3] = { xThoraxWorld, xMidWorld, xRearWorld };
	const float afRowZ[3] = { fRowZ, 0.0f, -fRowZ };
	const char* aszLegNames[6] = { "LegL0", "LegL1", "LegL2", "LegR0", "LegR1", "LegR2" };

	for (u_int uLeg = 0u; uLeg < 6u; ++uLeg)
	{
		const u_int uRow  = uLeg % 3u;                         // 0=front,1=mid,2=rear
		const float fSide = (uLeg < 3u) ? -1.0f : 1.0f;        // first three left, next three right
		ZM_InsectAppendLeg(xMesh, auRowParent[uRow], axRowParentWorld[uRow],
			fSide, afRowZ[uRow], fHipX, fKneeX, fFootX, fBellyY, fLegRadiusJ, fS, aszLegNames[uLeg]);
	}

	// ---- 4. Two antennae on the head, FIXED order L, R. Reach grows with the
	//      elaboration tier (size only -> fixed topology). ----
	const float fAntLen = (0.09f + 0.06f * (float)uTier) * fAntennaLenJ * fS;
	const float fAntRad = (0.020f + 0.006f * (float)uTier) * fS;
	ZM_InsectAppendAntenna(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf, -1.0f, fAntLen, fAntRad, fS, "AntennaL");
	ZM_InsectAppendAntenna(xMesh, xHeadRes.m_uTipBone, xHeadCentre, xHeadHalf,  1.0f, fAntLen, fAntRad, fS, "AntennaR");

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	// The INSECTOID plan is the closest to the bone cap; assert it stays within.
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Insectoid: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
