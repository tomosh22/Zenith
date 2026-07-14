#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Avian -- the S4 AVIAN archetype builder: a bird-like
// creature (rounded upright torso + head with a forward beak + two wings + two
// legs + a short tail) assembled from the ZM_CreatureArchetypeCommon appendage
// kit, plus ONE archetype-local helper (ZM_AvianAppendWing) for the flat wing
// blade the round-tube kit limb cannot express. Mirrors the QUADRUPED reference
// builder's structure exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The torso is a VERTICAL egg (belly
// at low Y, chest/withers at high Y); "front" is +Z, matching the kit's ring
// angle convention (ang=pi faces +Z). The head sits above and forward of the
// chest; the beak juts FORWARD (+Z) and slightly up from the head; the two
// wings drape DOWN the flanks as thin wide blades (thin in X, broad chord in Z);
// the two legs drop DOWN (-Y) to the ground; and the short tail sweeps back
// (-Z) and down from the pelvis.
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine02) parents every appendage. The
// bone set -- Spine00..Spine02, Head, Beak, WingL, WingR, the four leg bones
// (LegLUp/Lo, LegRUp/Lo) and Tail00..Tail01 -- is emitted in an IDENTICAL order
// with identical names for EVERY elaboration tier. Elaboration
// (recipe.m_uElaboration, 0..2) grows only the WING chord and the TAIL reach; it
// NEVER adds/removes/reorders a bone. Total bones: 3 + 1 + 1 + 2 + 4 + 2 = 13
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
// This builder only APPENDS mesh + skeleton (the driver ZM_BuildCreatureMesh
// runs the single finalise pass -- tangents + weight-normalise); it must NOT
// finalise or bake, and it leaves the loft's analytic ring normals untouched.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"             // ZM_CreatureRecipe, ZM_MakeGenRNG, ZM_GEN_DOMAIN_*
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"  // the appendage kit
#include "Collections/Zenith_Vector.h"

namespace
{
	// A non-overlapping normalized atlas island per part (SC-scope uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_AvianIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (belly at u=0, chest at u=count-1) so
	// appendages can be parented at the correct parent-bone world positions.
	Zenith_Maths::Vector3 ZM_AvianSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// Add a bone at a MODEL-space (world) position, computing its local offset from
	// the parent's world position (identity rotation, unit scale) -- a builder-local
	// mirror of the kit's private ZM_KitAddBoneWorld (that helper lives in the kit's
	// anonymous namespace and is not linkable here).
	u_int ZM_AvianAddBoneWorld(ZM_GenMesh& xMesh, const char* szName, int iParent,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xThisWorld)
	{
		const Zenith_Maths::Vector3 xLocal = (iParent < 0)
			? xThisWorld
			: (xThisWorld - xParentWorld);
		return ZM_GenAddBone(xMesh, szName, iParent, xLocal,
			glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
	}

	// ARCHETYPE-LOCAL: a flat wing blade on ONE bone. The kit's ZM_AppendLimb emits
	// a round tube (Rx == Rz per ring); a wing needs a THIN cross-section (small Rx)
	// with a BROAD chord (large Rz), so this helper builds the same kind of Y-swept
	// tube as the kit but with independent Rx/Rz per ring. Rings sweep DOWN the flank
	// from the shoulder (high Y, open where it meets the body) to a rounded, capped
	// wingtip (low Y). Every authored ring binds to the SINGLE wing bone, so the
	// loft's Catmull-Rom subdivision never exceeds a 2-bone blend. Attaches to
	// uParentBone (an upper spine node). Kept local per the SC2 rule (no kit edits).
	void ZM_AvianAppendWing(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xShoulder,
		const Zenith_Maths::Vector3& xTip, float fChordHalf, float fThickHalf,
		const ZM_GenUVIsland& xIsland, const char* szName)
	{
		Zenith_Assert(xShoulder.y > xTip.y,
			"ZM_AvianAppendWing: shoulder must be above the tip in Y (Y-swept blade)");
		Zenith_Assert(fChordHalf > 0.0f && fThickHalf > 0.0f,
			"ZM_AvianAppendWing: chord/thickness half-extents must be positive");

		const u_int uBone = ZM_AvianAddBoneWorld(xMesh, szName, (int)uParentBone,
			xParentWorld, xShoulder);

		// Four rings from shoulder to tip. Chord (Rz) widest near the shoulder and
		// tapering to a rounded tip; thickness (Rx) stays thin the whole way, so the
		// panel reads as a folded wing draped against the body's side.
		constexpr u_int uRINGS = 4u;
		Zenith_Maths::Vector3 axCentre[uRINGS];
		float                 afRx[uRINGS];
		float                 afRz[uRINGS];
		u_int                 auBone[uRINGS];
		for (u_int u = 0; u < uRINGS; u++)
		{
			const float fT = (float)u / (float)(uRINGS - 1u);
			axCentre[u] = xShoulder + (xTip - xShoulder) * fT;
			afRx[u] = fThickHalf * (1.0f - 0.45f * fT);           // stays thin
			afRz[u] = fChordHalf * (1.0f - 0.72f * fT * fT);      // broad chord, tapers to the tip
			auBone[u] = uBone;
		}

		// Build the loft ring table (single-bone authored rings) and sweep it. This
		// intentionally mirrors the kit's private ZM_KitEmitTube assembly so the wing
		// obeys the same no-flat-washer + outward-winding invariants.
		Zenith_Vector<ZM_LoftRing> xRings;
		xRings.Reserve(uRINGS);
		for (u_int u = 0; u < uRINGS; u++)
		{
			ZM_LoftRing xRing;
			xRing.m_fY  = axCentre[u].y;
			xRing.m_fCx = axCentre[u].x;
			xRing.m_fCz = axCentre[u].z;
			xRing.m_fRx = afRx[u];
			xRing.m_fRz = afRz[u];
			xRing.m_uBoneA  = auBone[u];
			xRing.m_uBoneB  = auBone[u];   // single-bone authored ring; subdivision blends
			xRing.m_fBlendB = 0.0f;
			xRing.m_fSuperEllipse = 1.0f;
			xRings.PushBack(xRing);

			if (u > 0u)
			{
				Zenith_Assert(axCentre[u].y != axCentre[u - 1u].y,
					"ZM_AvianAppendWing: consecutive rings share Y (degenerate flat washer)");
			}
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = xRings.GetDataPointer();
		xPart.m_uNumRings = xRings.GetSize();
		xPart.m_uSegs     = 8u;
		xPart.m_xIsland   = xIsland;
		xPart.m_bCapStart = false;   // open where it meets the body (hidden inside the torso)
		xPart.m_bCapEnd   = true;    // rounded wingtip
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}

	// One leg, in the fixed L,R draw order. Straight thin vertical tube from the hip
	// (belly height) to the foot (ground), slightly forward (+Z) and narrow in stance.
	void ZM_AvianAppendLeg(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, float fX, float fZ,
		float fBellyY, float fRadiusScale, float fSizeScale, const char* szName)
	{
		ZM_KitLimbParams xLimb;
		xLimb.m_iParentBone  = (int)uParentBone;
		xLimb.m_xParentWorld = xParentWorld;
		xLimb.m_xHip  = Zenith_Maths::Vector3(fX, fBellyY,        fZ);
		xLimb.m_xKnee = Zenith_Maths::Vector3(fX, fBellyY * 0.5f, fZ);
		xLimb.m_xFoot = Zenith_Maths::Vector3(fX, 0.0f,           fZ);
		xLimb.m_fRadiusTop  = 0.045f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusMid  = 0.038f * fRadiusScale * fSizeScale;
		xLimb.m_fRadiusFoot = 0.032f * fRadiusScale * fSizeScale;
		xLimb.m_uSegs       = 8u;
		xLimb.m_xIsland     = ZM_AvianIsland(0.62f, 0.42f, 0.98f, 0.72f);
		xLimb.m_szName      = szName;
		ZM_AppendLimb(xMesh, xLimb);
	}
}

// ---------------------------------------------------------------------------
// The AVIAN builder. Signature matches the frozen ZM_CreatureGen.h declaration
// exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the dispatch
// switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Avian(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
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
	const float fBeakLenJ    = xMeshRng.NextFloatRange(0.85f, 1.20f);   // 6 beak reach
	const float fWingChordJ  = xMeshRng.NextFloatRange(0.88f, 1.14f);   // 7 wing chord
	const float fLegRadiusJ  = xMeshRng.NextFloatRange(0.88f, 1.12f);   // 8 leg thickness
	const float fTailLenJ    = xMeshRng.NextFloatRange(0.85f, 1.15f);   // 9 tail reach

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fLegSplayJ = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 1 how far legs splay in X
	const float fHeadFwdJ  = xSkelRng.NextFloatRange(0.92f, 1.10f);     // 2 how far the head projects forward
	const float fWingDropJ = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 3 how far the wings drape down

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. ----
	const float fBellyY   = 0.30f * fS;                        // belly height == leg length
	const float fBodyLen  = 0.35f * fBodyLenJ * fS;            // torso vertical span (belly -> chest)
	const float fHalfWide = 0.17f * fBodyWidthJ * fS;          // torso Rx (side girth)
	const float fHalfDeep = 0.20f * fBodyDepthJ * fS;          // torso Rz (front-back length)
	const Zenith_Maths::Vector3 xBelly(0.0f, fBellyY, 0.0f);
	const u_int uTier = xRecipe.m_uElaboration;                // 0, 1, or 2

	// ---- 1. Spine tube (creates the single root). FIXED 3 nodes for every tier. ----
	ZM_KitSpineParams xSpine;
	xSpine.m_iParentBone  = -1;                                // create the root here
	xSpine.m_xBellyCentre = xBelly;
	xSpine.m_fLength      = fBodyLen;
	xSpine.m_uSegments    = 3u;
	xSpine.m_fHalfWidth   = fHalfWide;
	xSpine.m_fHalfDepth   = fHalfDeep;
	xSpine.m_fEndTaper    = 0.55f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_AvianIsland(0.02f, 0.02f, 0.60f, 0.60f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 3u, "ZM_BuildArchetype_Avian: spine needs >= 3 nodes");

	const u_int uPelvisBone = auSpine[0];
	const u_int uMidBone    = auSpine[1];
	const u_int uChestBone  = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xPelvisWorld = ZM_AvianSpineWorld(xBelly, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xMidWorld    = ZM_AvianSpineWorld(xBelly, fBodyLen, uSpineCount, 1u);
	const Zenith_Maths::Vector3 xChestWorld  = ZM_AvianSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Head (ellipsoid), above and forward (+Z) of the chest. ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.13f * fHeadSizeJ * fS,
		0.13f * fHeadSizeJ * fS,
		0.14f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fBellyY + fBodyLen + xHeadHalf.y * 0.55f,
		0.10f * fHeadFwdJ * fS);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uChestBone;
	xHead.m_xParentWorld = xChestWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_AvianIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	const ZM_KitAppendResult xHeadRes = ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Beak: a small forward-and-up cone off the head front (ZM_AppendHorn,
	//      which sweeps UP -- the beak base sits low-front, its tip forward + higher). ----
	const Zenith_Maths::Vector3 xBeakBase(
		0.0f,
		xHeadCentre.y - xHeadHalf.y * 0.15f,
		xHeadCentre.z + xHeadHalf.z * 0.75f);
	const float fBeakFwd = (0.10f + 0.04f * (float)uTier) * fBeakLenJ * fS;
	ZM_KitHornParams xBeak;
	xBeak.m_iParentBone  = (int)xHeadRes.m_uTipBone;
	xBeak.m_xParentWorld = xHeadCentre;
	xBeak.m_xBase = xBeakBase;
	xBeak.m_xTip  = Zenith_Maths::Vector3(
		0.0f,
		xHeadCentre.y + xHeadHalf.y * 0.05f,   // tip.y > base.y (horn precondition)
		xBeakBase.z + fBeakFwd);
	xBeak.m_fRadiusBase = 0.040f * fHeadSizeJ * fS;
	xBeak.m_uSegs       = 6u;
	xBeak.m_xIsland     = ZM_AvianIsland(0.02f, 0.62f, 0.30f, 0.98f);
	xBeak.m_szName      = "Beak";
	ZM_AppendHorn(xMesh, xBeak);

	// ---- 4. Two wings, FIXED order L, R -- flat blades draping down the flanks off
	//      the mid spine node. Chord grows with elaboration tier (size only). ----
	const float fShoulderY  = fBellyY + fBodyLen * 0.55f;
	const float fWingTipY   = fShoulderY - fBodyLen * 0.50f * fWingDropJ;
	const float fWingChord  = (0.11f + 0.03f * (float)uTier) * fWingChordJ * fS;
	const float fWingThick  = 0.028f * fS;
	const ZM_GenUVIsland xWingIsland = ZM_AvianIsland(0.30f, 0.62f, 0.60f, 0.98f);
	for (u_int uW = 0; uW < 2u; uW++)
	{
		const float fSide = (uW == 0u) ? -1.0f : 1.0f;
		const Zenith_Maths::Vector3 xShoulder(fSide * fHalfWide * 0.90f, fShoulderY, 0.0f);
		const Zenith_Maths::Vector3 xTip(fSide * (fHalfWide + 0.04f * fS), fWingTipY, -0.02f * fS);
		ZM_AvianAppendWing(xMesh, uMidBone, xMidWorld, xShoulder, xTip,
			fWingChord, fWingThick, xWingIsland, (uW == 0u) ? "WingL" : "WingR");
	}

	// ---- 5. Two legs, FIXED order L, R. Thin, narrow stance, slightly forward. ----
	const float fSplayX = 0.075f * fLegSplayJ * fS;
	const float fLegZ   = 0.03f * fS;
	ZM_AvianAppendLeg(xMesh, uPelvisBone, xPelvisWorld, -fSplayX, fLegZ, fBellyY, fLegRadiusJ, fS, "LegL");
	ZM_AvianAppendLeg(xMesh, uPelvisBone, xPelvisWorld,  fSplayX, fLegZ, fBellyY, fLegRadiusJ, fS, "LegR");

	// ---- 6. Short tail: from the rump (back-low, -Z) sweeping down and back. Reach
	//      grows with elaboration tier (size only). ----
	ZM_KitTailParams xTail;
	xTail.m_iParentBone  = (int)uPelvisBone;
	xTail.m_xParentWorld = xPelvisWorld;
	xTail.m_xBase = Zenith_Maths::Vector3(0.0f, fBellyY + fBodyLen * 0.10f, -fHalfDeep * 0.70f);
	xTail.m_xTip  = Zenith_Maths::Vector3(0.0f, fBellyY * 0.50f,
		-(fHalfDeep * 0.70f + (0.12f + 0.05f * (float)uTier) * fTailLenJ * fS));
	xTail.m_uSegments   = 2u;
	xTail.m_fRadiusBase = 0.050f * fS;
	xTail.m_fRadiusTip  = 0.015f * fS;
	xTail.m_uSegs       = 7u;
	xTail.m_xIsland     = ZM_AvianIsland(0.62f, 0.74f, 0.98f, 0.98f);
	xTail.m_szNamePrefix = "Tail";
	ZM_AppendTail(xMesh, xTail);

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Avian: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
