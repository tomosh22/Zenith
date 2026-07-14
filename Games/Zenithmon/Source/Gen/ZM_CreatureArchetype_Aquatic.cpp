#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_Aquatic -- the S4 (SC3) AQUATIC archetype builder: a
// fish / sea-creature (streamlined laterally-compressed body + a forward head +
// a caudal (tail) fin + a dorsal (top) fin + two pectoral (side) fins, NO legs)
// assembled from the ZM_CreatureArchetypeCommon appendage kit, plus ONE
// archetype-local helper (ZM_AquaticAppendFin) for the flat fin blades the
// round-tube kit limb cannot express. Mirrors the QUADRUPED / AVIAN reference
// builders' structure exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The body is therefore a VERTICAL egg
// (belly at low Y, dorsal ridge at high Y); to read as a fish it is THIN in X
// (side girth Rx, laterally compressed) and ELONGATED in Z (front-back depth Rz).
// "Front" is +Z, matching the kit's ring angle convention (ang=pi faces +Z). The
// head projects FORWARD (+Z); the dorsal fin rises UP (+Y) from the back ridge;
// the caudal fin sweeps UP and BACK (-Z) from the rear of the body; the two
// pectoral fins drape DOWN the flanks as thin flat blades at +/-X. Every fin is a
// flat panel: THIN in X, BROAD chord in Z, swept along Y (the ZM_AvianAppendWing
// pattern), so the round-tube kit cannot express it and a builder-local helper is
// required (kept local per the SC-authoring rule -- no shared-kit edits).
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine02) parents every appendage. The bone
// set -- Spine00..Spine02, Head, FinDorsal, FinPecL, FinPecR, FinCaudal -- is
// emitted in an IDENTICAL order with identical names for EVERY elaboration tier.
// Elaboration (recipe.m_uElaboration, 0..2) grows only the FIN chords/reach; it
// NEVER adds/removes/reorders a bone. Total bones: 3 + 1 + 1 + 2 + 1 = 8
// (<= the creature cap of 30). There are NO legs.
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
// or bake, and it leaves the loft's analytic ring normals untouched.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"             // ZM_CreatureRecipe, ZM_MakeGenRNG, ZM_GEN_DOMAIN_*
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"  // the appendage kit
#include "Collections/Zenith_Vector.h"

namespace
{
	// A non-overlapping normalized atlas island per part (SC-scope uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_AquaticIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (belly at u=0, dorsal ridge at
	// u=count-1) so appendages can be parented at the correct parent-bone world
	// positions.
	Zenith_Maths::Vector3 ZM_AquaticSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// Add a bone at a MODEL-space (world) position, computing its local offset from
	// the parent's world position (identity rotation, unit scale) -- a builder-local
	// mirror of the kit's private ZM_KitAddBoneWorld (that helper lives in the kit's
	// anonymous namespace and is not linkable here).
	u_int ZM_AquaticAddBoneWorld(ZM_GenMesh& xMesh, const char* szName, int iParent,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xThisWorld)
	{
		const Zenith_Maths::Vector3 xLocal = (iParent < 0)
			? xThisWorld
			: (xThisWorld - xParentWorld);
		return ZM_GenAddBone(xMesh, szName, iParent, xLocal,
			glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
	}

	// ARCHETYPE-LOCAL: a flat fin blade on ONE bone. The kit's ZM_AppendLimb emits a
	// round tube (Rx == Rz per ring); a fin needs a THIN cross-section (small Rx)
	// with a BROAD chord (large Rz), so this helper builds the same kind of Y-swept
	// tube as the kit but with independent Rx/Rz per ring. Rings sweep from the base
	// (open, buried inside the body) to a rounded, capped fin tip. The sweep may go
	// UP (dorsal fin) or DOWN (pectoral fins) -- the loft auto-orients winding + the
	// tip cap from the ring Y direction, so this helper only requires base/tip to
	// differ in Y. Every authored ring binds to the SINGLE fin bone, so the loft's
	// Catmull-Rom subdivision never exceeds a 2-bone blend. Attaches to uParentBone.
	// Modeled EXACTLY on the AVIAN ZM_AvianAppendWing pattern; kept local per the
	// SC-authoring rule (no kit edits).
	void ZM_AquaticAppendFin(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xBase,
		const Zenith_Maths::Vector3& xTip, float fChordHalf, float fThickHalf,
		const ZM_GenUVIsland& xIsland, const char* szName)
	{
		Zenith_Assert(xBase.y != xTip.y,
			"ZM_AquaticAppendFin: base and tip must differ in Y (Y-swept blade)");
		Zenith_Assert(fChordHalf > 0.0f && fThickHalf > 0.0f,
			"ZM_AquaticAppendFin: chord/thickness half-extents must be positive");

		const u_int uBone = ZM_AquaticAddBoneWorld(xMesh, szName, (int)uParentBone,
			xParentWorld, xBase);

		// Four rings from base to tip. Chord (Rz) widest at the base and tapering to a
		// rounded tip; thickness (Rx) stays thin the whole way, so the panel reads as
		// a flat fin blade.
		constexpr u_int uRINGS = 4u;
		Zenith_Maths::Vector3 axCentre[uRINGS];
		float                 afRx[uRINGS];
		float                 afRz[uRINGS];
		u_int                 auBone[uRINGS];
		for (u_int u = 0; u < uRINGS; u++)
		{
			const float fT = (float)u / (float)(uRINGS - 1u);
			axCentre[u] = xBase + (xTip - xBase) * fT;
			afRx[u] = fThickHalf * (1.0f - 0.45f * fT);           // stays thin
			afRz[u] = fChordHalf * (1.0f - 0.72f * fT * fT);      // broad chord, tapers to the tip
			auBone[u] = uBone;
		}

		// Build the loft ring table (single-bone authored rings) and sweep it. This
		// intentionally mirrors the kit's private tube assembly so the fin obeys the
		// same no-flat-washer + outward-winding invariants.
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
					"ZM_AquaticAppendFin: consecutive rings share Y (degenerate flat washer)");
			}
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = xRings.GetDataPointer();
		xPart.m_uNumRings = xRings.GetSize();
		xPart.m_uSegs     = 8u;
		xPart.m_xIsland   = xIsland;
		xPart.m_bCapStart = false;   // open where it meets the body (hidden inside the torso)
		xPart.m_bCapEnd   = true;    // rounded fin tip
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}
}

// ---------------------------------------------------------------------------
// The AQUATIC builder. Signature matches the frozen ZM_CreatureGen.h declaration
// exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the dispatch
// switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_Aquatic(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS = xRecipe.m_fSizeScale;

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ    = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 torso vertical span (body height)
	const float fBodyWidthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth (lateral thickness)
	const float fBodyDepthJ  = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back length
	const float fBellyRoundJ = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier belly)
	const float fHeadSizeJ   = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 head scale
	const float fDorsalJ     = xMeshRng.NextFloatRange(0.88f, 1.14f);   // 6 dorsal fin chord
	const float fPectoralJ   = xMeshRng.NextFloatRange(0.88f, 1.14f);   // 7 pectoral fin chord
	const float fCaudalJ     = xMeshRng.NextFloatRange(0.85f, 1.15f);   // 8 caudal fin reach

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fHeadFwdJ  = xSkelRng.NextFloatRange(0.92f, 1.10f);     // 1 how far the head projects forward
	const float fPecSplayJ = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 2 how far the pectorals splay in X
	const float fPecDropJ  = xSkelRng.NextFloatRange(0.90f, 1.12f);     // 3 how far the pectorals drape down

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. A fish
	//      is THIN in X (fHalfWide) and ELONGATED in Z (fHalfDeep). ----
	const float fBellyY   = 0.32f * fS;                        // body-centre height off the ground
	const float fBodyLen  = 0.30f * fBodyLenJ * fS;            // dorsal-ventral span (belly -> back ridge)
	const float fHalfWide = 0.12f * fBodyWidthJ * fS;          // torso Rx (side girth, laterally compressed)
	const float fHalfDeep = 0.34f * fBodyDepthJ * fS;          // torso Rz (front-back length, elongated)
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
	xSpine.m_fEndTaper    = 0.50f;
	xSpine.m_fBellyRound  = fBellyRoundJ;
	xSpine.m_uSegs        = 10u;
	xSpine.m_xIsland      = ZM_AquaticIsland(0.02f, 0.02f, 0.60f, 0.55f);
	xSpine.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xSpine, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 3u, "ZM_BuildArchetype_Aquatic: spine needs >= 3 nodes");

	const u_int uPelvisBone = auSpine[0];
	const u_int uMidBone    = auSpine[1];
	const u_int uBackBone   = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xPelvisWorld = ZM_AquaticSpineWorld(xBelly, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xMidWorld    = ZM_AquaticSpineWorld(xBelly, fBodyLen, uSpineCount, 1u);
	const Zenith_Maths::Vector3 xBackWorld   = ZM_AquaticSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Head (ellipsoid), projecting forward (+Z) at mid body height. ----
	const Zenith_Maths::Vector3 xHeadHalf(
		0.11f * fHeadSizeJ * fS,
		0.12f * fHeadSizeJ * fS,
		0.14f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xHeadCentre(
		0.0f,
		fBellyY + fBodyLen * 0.55f,
		(fHalfDeep * 0.85f) * fHeadFwdJ);

	ZM_KitHeadParams xHead;
	xHead.m_iParentBone  = (int)uBackBone;
	xHead.m_xParentWorld = xBackWorld;
	xHead.m_xCentre      = xHeadCentre;
	xHead.m_xHalfExtents = xHeadHalf;
	xHead.m_uRings       = 5u;
	xHead.m_uSegs        = 10u;
	xHead.m_xIsland      = ZM_AquaticIsland(0.62f, 0.02f, 0.98f, 0.40f);
	xHead.m_szName       = "Head";
	ZM_AppendEllipsoidHead(xMesh, xHead);

	// ---- 3. Dorsal fin: a thin flat sail rising UP (+Y) from the back ridge, its
	//      broad chord (Rz) along the back. Chord + rise grow with elaboration tier
	//      (size only). Base is buried in the body top (open ring hidden). ----
	const Zenith_Maths::Vector3 xDorsalBase(0.0f, fBellyY + fBodyLen * 0.85f, 0.0f);
	const Zenith_Maths::Vector3 xDorsalTip(
		0.0f,
		fBellyY + fBodyLen + (0.10f + 0.05f * (float)uTier) * fDorsalJ * fS,
		-0.02f * fS);
	const float fDorsalChord = (0.13f + 0.03f * (float)uTier) * fDorsalJ * fS;
	const float fDorsalThick = 0.020f * fS;
	ZM_AquaticAppendFin(xMesh, uBackBone, xBackWorld, xDorsalBase, xDorsalTip,
		fDorsalChord, fDorsalThick, ZM_AquaticIsland(0.02f, 0.60f, 0.32f, 0.98f), "FinDorsal");

	// ---- 4. Two pectoral fins, FIXED order L, R -- thin flat blades draping DOWN
	//      the flanks off the mid spine node, splaying outward in X. Chord grows with
	//      elaboration tier (size only). ----
	const float fPecShoulderY = fBellyY + fBodyLen * 0.45f;
	const float fPecTipY      = fPecShoulderY - fBodyLen * 0.45f * fPecDropJ;
	const float fPecChord     = (0.09f + 0.025f * (float)uTier) * fPectoralJ * fS;
	const float fPecThick     = 0.016f * fS;
	const ZM_GenUVIsland xPecIsland = ZM_AquaticIsland(0.34f, 0.60f, 0.64f, 0.98f);
	for (u_int uP = 0; uP < 2u; uP++)
	{
		const float fSide = (uP == 0u) ? -1.0f : 1.0f;
		const Zenith_Maths::Vector3 xShoulder(fSide * fHalfWide * 0.85f, fPecShoulderY, fHalfDeep * 0.10f);
		const Zenith_Maths::Vector3 xTip(
			fSide * (fHalfWide + 0.06f * fS) * fPecSplayJ, fPecTipY, -0.02f * fS);
		ZM_AquaticAppendFin(xMesh, uMidBone, xMidWorld, xShoulder, xTip,
			fPecChord, fPecThick, xPecIsland, (uP == 0u) ? "FinPecL" : "FinPecR");
	}

	// ---- 5. Caudal (tail) fin: a thin flat fan sweeping UP and BACK (-Z) from the
	//      rear of the body. Base is buried in the body rear (open ring hidden);
	//      reach grows with elaboration tier (size only). ----
	const Zenith_Maths::Vector3 xCaudalBase(0.0f, fBellyY + fBodyLen * 0.15f, -fHalfDeep * 0.65f);
	const Zenith_Maths::Vector3 xCaudalTip(
		0.0f,
		fBellyY + fBodyLen * 0.75f,
		-(fHalfDeep * 0.65f + (0.16f + 0.05f * (float)uTier) * fCaudalJ * fS));
	const float fCaudalChord = (0.12f + 0.04f * (float)uTier) * fCaudalJ * fS;
	const float fCaudalThick = 0.020f * fS;
	ZM_AquaticAppendFin(xMesh, uPelvisBone, xPelvisWorld, xCaudalBase, xCaudalTip,
		fCaudalChord, fCaudalThick, ZM_AquaticIsland(0.66f, 0.44f, 0.98f, 0.98f), "FinCaudal");

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_Aquatic: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
