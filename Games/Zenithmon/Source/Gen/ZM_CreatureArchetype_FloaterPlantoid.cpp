#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetype_FloaterPlantoid -- the S4 (SC5) FLOATER-PLANTOID
// archetype builder: a floating plant / jellyfish-like creature -- a central
// buoyant BULB/POD body drifting ABOVE the ground with a small crown on top and
// a symmetric RADIAL SKIRT of hanging tendrils around its underside, and NO
// ground legs. Assembled from the ZM_CreatureArchetypeCommon appendage kit
// (ZM_AppendSpineTube for the bulb + ZM_AppendEllipsoidHead for the crown), plus
// ONE archetype-local helper (ZM_FloaterAppendTendril) for the thin round
// tapering tendril the kit's multi-bone ZM_AppendTail cannot express on a single
// bone. Mirrors the QUADRUPED / AVIAN / AQUATIC reference builders' structure
// exactly.
//
// GEOMETRY MODEL (inherited from the kit -- see ZM_CreatureArchetypeCommon.h):
// the loft sweeps rings along WORLD Y ONLY. The bulb is a VERTICAL egg (belly at
// low Y, crown at high Y) whose LOWEST vertex sits WELL ABOVE the ground plane --
// the creature FLOATS: every model-space Y coordinate is a strictly positive
// multiple of the size scale, so the mesh's min-Y bound is above 0 for every
// size class (there is no foot at the ground, unlike the grounded quadruped /
// biped / insectoid). "Front" is +Z (the kit's ring angle convention: ang=pi
// faces +Z). Around the bulb's underside hang N=6 TENDRILS, placed at FIXED even
// angular steps (k * 360/6 degrees) around the Y axis so the skirt is radially
// symmetric; each tendril is a ROUND tapering tube (Rx == Rz, so it reads the
// same at every angle -- the axis-aligned loft ring cannot rotate, so a flat
// blade would look different per compass direction, whereas a round tube is
// rotation-invariant and gives clean N-fold symmetry). Every tendril's tip stays
// above the ground, preserving the float.
//
// SKELETON (fixed topology across ALL evo stages so archetype clips transfer):
// a single root spine chain (Spine00..Spine02) that IS the bulb, one crown
// ("Head") bone, and N=6 tendril bones ("Tendril0".."Tendril5"). The bone set is
// emitted in an IDENTICAL order with identical names for EVERY elaboration tier.
// Elaboration (recipe.m_uElaboration, 0..2) grows ONLY the tendril reach + crown
// rise (size, never topology); it NEVER adds/removes/reorders a bone, and there
// are NO limbs (no "...Up" bones -- a floater has no legs). Total bones:
// 3 + 1 + 6 = 10 (<= the creature cap of 30).
//
// DETERMINISM (AssetManifest 6.2): the ONLY randomness sources are
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_MESH) for geometry proportion jitter and
// ZM_MakeGenRNG(recipe, ZM_GEN_DOMAIN_SKELETON) for float/placement jitter. Every
// draw is taken UP FRONT in a FIXED order (all MESH draws, then all SKELETON
// draws) so the stream is order-stable regardless of build sequencing. The N=6
// radial tendril angles are FIXED CONSTANTS (k * 360/6), never drawn per-run. No
// clock / pointer / global-RNG / container-iteration entropy. All extents scale
// by recipe.m_fSizeScale.
//
// This builder only APPENDS mesh + skeleton (the driver ZM_BuildCreatureMesh runs
// the single finalise pass -- tangents + weight-normalise); it must NOT finalise
// or bake, and it leaves the loft's analytic ring normals untouched (it never
// sculpts vertex positions directly, so it never regenerates normals).
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"             // ZM_CreatureRecipe, ZM_MakeGenRNG, ZM_GEN_DOMAIN_*
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"  // the appendage kit
#include "Collections/Zenith_Vector.h"

#include <cmath>   // cosf, sinf (bit-consistent with the loft's own ring trig)

namespace
{
	// The FIXED radial appendage count -- how many tendrils hang symmetrically
	// around the bulb. Golden-fixed across ALL evo tiers (topology never varies by
	// tier). 3 spine + 1 crown + this = the total bone count.
	constexpr u_int uFLOATER_TENDRIL_COUNT = 6u;

	// File-local 2*pi (the kit's fZM_KIT_PI / ZM_GenCommon's fZM_GEN_TWO_PI both
	// live in un-linkable anonymous namespaces, so this builder carries its own,
	// matching the module convention). Used ONLY to derive the FIXED radial tendril
	// angles -- never a random draw.
	constexpr float fZM_FLOATER_TWO_PI = 6.28318530717959f;

	// A non-overlapping normalized atlas island per part (SC-scope uses a whole-body
	// albedo synth, so islands mostly matter for future per-part texturing; these
	// rects are golden-fixed but not yet load-bearing).
	ZM_GenUVIsland ZM_FloaterIsland(float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xIsland;
		xIsland.m_fU0 = fU0; xIsland.m_fV0 = fV0;
		xIsland.m_fU1 = fU1; xIsland.m_fV1 = fV1;
		return xIsland;
	}

	// The model-space world position of spine node u, reproducing the kit's own
	// ZM_AppendSpineTube node formula EXACTLY (belly at u=0, crown at u=count-1) so
	// appendages can be parented at the correct parent-bone world positions.
	Zenith_Maths::Vector3 ZM_FloaterSpineWorld(const Zenith_Maths::Vector3& xBelly,
		float fBodyLen, u_int uCount, u_int u)
	{
		const float fT = (uCount > 1u) ? ((float)u / (float)(uCount - 1u)) : 0.0f;
		return Zenith_Maths::Vector3(xBelly.x, xBelly.y + fT * fBodyLen, xBelly.z);
	}

	// Add a bone at a MODEL-space (world) position, computing its local offset from
	// the parent's world position (identity rotation, unit scale) -- a builder-local
	// mirror of the kit's private ZM_KitAddBoneWorld (that helper lives in the kit's
	// anonymous namespace and is not linkable here).
	u_int ZM_FloaterAddBoneWorld(ZM_GenMesh& xMesh, const char* szName, int iParent,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xThisWorld)
	{
		const Zenith_Maths::Vector3 xLocal = (iParent < 0)
			? xThisWorld
			: (xThisWorld - xParentWorld);
		return ZM_GenAddBone(xMesh, szName, iParent, xLocal,
			glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
	}

	// ARCHETYPE-LOCAL: a thin ROUND tapering tendril on ONE bone. The kit's
	// ZM_AppendTail builds a MULTI-bone tapering tube (m_uSegments bones), which
	// would blow the fixed radial-skirt bone budget; a tendril here is exactly ONE
	// bone, so this helper builds the same kind of Y-swept tube as the kit but with
	// a SINGLE-bone ring skin and a ROUND cross-section (Rx == Rz per ring, so the
	// tendril reads identically at every radial angle -- the axis-aligned loft ring
	// cannot rotate, so only a round section gives clean N-fold symmetry). Rings
	// sweep DOWN from the base (open, buried inside the bulb's underside) to a
	// rounded, capped tip that stays above the ground. Every authored ring binds to
	// the SINGLE tendril bone, so the loft's Catmull-Rom subdivision never exceeds a
	// 2-bone blend. Attaches to uParentBone (a spine node). Modeled EXACTLY on the
	// AVIAN ZM_AvianAppendWing / AQUATIC ZM_AquaticAppendFin pattern; kept local per
	// the SC-authoring rule (no shared-kit edits).
	void ZM_FloaterAppendTendril(ZM_GenMesh& xMesh, u_int uParentBone,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xBase,
		const Zenith_Maths::Vector3& xTip, float fRadiusBase, float fRadiusTip,
		const ZM_GenUVIsland& xIsland, const char* szName)
	{
		Zenith_Assert(xBase.y > xTip.y,
			"ZM_FloaterAppendTendril: base must be above the tip in Y (Y-swept, hanging tendril)");
		Zenith_Assert(fRadiusBase > 0.0f && fRadiusTip > 0.0f,
			"ZM_FloaterAppendTendril: base/tip radii must be positive");

		const u_int uBone = ZM_FloaterAddBoneWorld(xMesh, szName, (int)uParentBone,
			xParentWorld, xBase);

		// Four rings from base to tip. The round radius (Rx == Rz) tapers linearly
		// from the base girth to the thin tip, so the panel reads as a soft hanging
		// strand rather than a barrel.
		constexpr u_int uRINGS = 4u;
		Zenith_Maths::Vector3 axCentre[uRINGS];
		float                 afR[uRINGS];
		u_int                 auBone[uRINGS];
		for (u_int u = 0; u < uRINGS; u++)
		{
			const float fT = (float)u / (float)(uRINGS - 1u);
			axCentre[u] = xBase + (xTip - xBase) * fT;
			afR[u]      = fRadiusBase + (fRadiusTip - fRadiusBase) * fT;   // linear round taper
			auBone[u]   = uBone;
		}

		// Build the loft ring table (single-bone authored rings) and sweep it. This
		// intentionally mirrors the kit's private tube assembly so the tendril obeys
		// the same no-flat-washer + outward-winding invariants.
		Zenith_Vector<ZM_LoftRing> xRings;
		xRings.Reserve(uRINGS);
		for (u_int u = 0; u < uRINGS; u++)
		{
			ZM_LoftRing xRing;
			xRing.m_fY  = axCentre[u].y;
			xRing.m_fCx = axCentre[u].x;
			xRing.m_fCz = axCentre[u].z;
			xRing.m_fRx = afR[u];          // round cross-section: Rx == Rz
			xRing.m_fRz = afR[u];
			xRing.m_uBoneA  = auBone[u];
			xRing.m_uBoneB  = auBone[u];   // single-bone authored ring; subdivision blends
			xRing.m_fBlendB = 0.0f;
			xRing.m_fSuperEllipse = 1.0f;
			xRings.PushBack(xRing);

			if (u > 0u)
			{
				Zenith_Assert(axCentre[u].y != axCentre[u - 1u].y,
					"ZM_FloaterAppendTendril: consecutive rings share Y (degenerate flat washer)");
			}
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = xRings.GetDataPointer();
		xPart.m_uNumRings = xRings.GetSize();
		xPart.m_uSegs     = 6u;
		xPart.m_xIsland   = xIsland;
		xPart.m_bCapStart = false;   // open where it meets the bulb (hidden inside the body)
		xPart.m_bCapEnd   = true;    // rounded tendril tip
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}
}

// ---------------------------------------------------------------------------
// The FLOATER-PLANTOID builder. Signature matches the frozen ZM_CreatureGen.h
// declaration exactly (ZM_GenMesh& then const ZM_CreatureRecipe&). Wired into the
// dispatch switch (ZM_GetArchetypeBuilder) by the orchestrator, NOT here.
// ---------------------------------------------------------------------------
void ZM_BuildArchetype_FloaterPlantoid(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe)
{
	const float fS    = xRecipe.m_fSizeScale;
	const u_int uTier = xRecipe.m_uElaboration;               // 0, 1, or 2

	// ---- Randomness: two independent streams, ALL draws taken up front in a
	//      FIXED order so the sequence is stable regardless of build ordering. ----
	ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
	const float fBodyLenJ     = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 1 bulb vertical span
	const float fBodyWidthJ   = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 2 side girth
	const float fBodyDepthJ   = xMeshRng.NextFloatRange(0.90f, 1.10f);   // 3 front-back girth
	const float fBellyRoundJ  = xMeshRng.NextFloatRange(0.85f, 1.00f);   // 4 super-ellipse (<1 boxier bulb)
	const float fHeadSizeJ    = xMeshRng.NextFloatRange(0.90f, 1.12f);   // 5 crown scale
	const float fTendrilLenJ  = xMeshRng.NextFloatRange(0.85f, 1.15f);   // 6 tendril reach (drop)
	const float fTendrilThickJ= xMeshRng.NextFloatRange(0.88f, 1.12f);   // 7 tendril thickness

	ZM_GenRNG xSkelRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SKELETON);
	const float fFloatJ      = xSkelRng.NextFloatRange(0.92f, 1.10f);    // 1 how high the bulb floats
	const float fTendrilRingJ= xSkelRng.NextFloatRange(0.90f, 1.12f);    // 2 tendril attach-ring radius
	const float fCrownRiseJ  = xSkelRng.NextFloatRange(0.92f, 1.10f);    // 3 how high the crown sits

	// ---- Core proportions (reference = MEDIUM at fS==1), all scaled by fS. The
	//      bulb floats: fBodyBottomY is a strictly positive multiple of fS, so the
	//      lowest body ring sits well above the ground. The bulb is roughly as wide
	//      as it is deep -- a rounded pod. ----
	const float fBodyBottomY = 0.62f * fFloatJ * fS;          // lowest bulb ring height (floats off the ground)
	const float fBodyLen     = 0.34f * fBodyLenJ * fS;        // bulb vertical span (belly -> crown)
	const float fHalfWide    = 0.24f * fBodyWidthJ * fS;      // bulb Rx (side girth)
	const float fHalfDeep    = 0.24f * fBodyDepthJ * fS;      // bulb Rz (front-back girth)
	const Zenith_Maths::Vector3 xBelly(0.0f, fBodyBottomY, 0.0f);

	// ---- 1. Bulb body: a single rounded spine tube (creates the single root).
	//      FIXED 3 nodes for every tier so the mid node bulges the silhouette into a
	//      pod; the super-ellipse (< 1) softly squares the cross-section. ----
	ZM_KitSpineParams xBody;
	xBody.m_iParentBone  = -1;                                // create the root here
	xBody.m_xBellyCentre = xBelly;
	xBody.m_fLength      = fBodyLen;
	xBody.m_uSegments    = 3u;
	xBody.m_fHalfWidth   = fHalfWide;
	xBody.m_fHalfDepth   = fHalfDeep;
	xBody.m_fEndTaper    = 0.60f;                             // fat, rounded ends (not a pinched barrel)
	xBody.m_fBellyRound  = fBellyRoundJ;
	xBody.m_uSegs        = 12u;
	xBody.m_xIsland      = ZM_FloaterIsland(0.02f, 0.02f, 0.66f, 0.66f);
	xBody.m_szNamePrefix = "Spine";

	u_int auSpine[8];
	const u_int uSpineCount = ZM_AppendSpineTube(xMesh, xBody, auSpine, 8u);
	Zenith_Assert(uSpineCount >= 2u, "ZM_BuildArchetype_FloaterPlantoid: bulb needs >= 2 nodes");

	const u_int uBellyBone = auSpine[0];
	const u_int uCrownBone = auSpine[uSpineCount - 1u];
	const Zenith_Maths::Vector3 xBellyWorld =
		ZM_FloaterSpineWorld(xBelly, fBodyLen, uSpineCount, 0u);
	const Zenith_Maths::Vector3 xCrownWorld =
		ZM_FloaterSpineWorld(xBelly, fBodyLen, uSpineCount, uSpineCount - 1u);

	// ---- 2. Crown (ellipsoid), a small cap resting atop the bulb. Rise grows with
	//      the elaboration tier (size only -- always present so bone topology is
	//      fixed). Sits entirely above the ground. ----
	const Zenith_Maths::Vector3 xCrownHalf(
		0.11f * fHeadSizeJ * fS,
		0.10f * fHeadSizeJ * fS,
		0.11f * fHeadSizeJ * fS);
	const Zenith_Maths::Vector3 xCrownCentre(
		0.0f,
		fBodyBottomY + fBodyLen + xCrownHalf.y * (0.35f + 0.10f * (float)uTier) * fCrownRiseJ,
		0.0f);

	ZM_KitHeadParams xCrown;
	xCrown.m_iParentBone  = (int)uCrownBone;
	xCrown.m_xParentWorld = xCrownWorld;
	xCrown.m_xCentre      = xCrownCentre;
	xCrown.m_xHalfExtents = xCrownHalf;
	xCrown.m_uRings       = 4u;
	xCrown.m_uSegs        = 10u;
	xCrown.m_xIsland      = ZM_FloaterIsland(0.68f, 0.02f, 0.98f, 0.40f);
	xCrown.m_szName       = "Head";
	ZM_AppendEllipsoidHead(xMesh, xCrown);

	// ---- 3. Radial tendril skirt: N=6 thin round tendrils hanging from the bulb's
	//      underside, placed at FIXED even angular steps (k * 360/6) around Y so the
	//      skirt is radially symmetric. Each tendril is ONE bone; reach grows with
	//      the elaboration tier (size only -- topology fixed). Every tip stays above
	//      the ground (a strictly positive multiple of fS), preserving the float. ----
	const float fAttachRadius = fHalfWide * 0.80f * fTendrilRingJ;                 // ring on the lower bulb
	const float fTipRadius    = fAttachRadius + 0.10f * fS;                        // splays outward as it drops
	const float fBaseY        = fBodyBottomY + fBodyLen * 0.15f;                   // attach just above the belly
	const float fReach        = (0.22f + 0.04f * (float)uTier) * fTendrilLenJ * fS;// how far it hangs down
	const float fTipY         = fBodyBottomY - fReach;                             // stays > 0 for all size classes
	const float fTendrilBaseR = 0.045f * fTendrilThickJ * fS;
	const float fTendrilTipR  = 0.018f * fTendrilThickJ * fS;

	// Fixed per-tendril names (distinct so a clip / structural test can resolve each
	// radial appendage; count is golden-fixed at uFLOATER_TENDRIL_COUNT).
	static const char* const s_aszTendrilNames[uFLOATER_TENDRIL_COUNT] =
	{
		"Tendril0", "Tendril1", "Tendril2", "Tendril3", "Tendril4", "Tendril5",
	};
	const ZM_GenUVIsland xTendrilIsland = ZM_FloaterIsland(0.02f, 0.70f, 0.60f, 0.98f);
	for (u_int uT = 0; uT < uFLOATER_TENDRIL_COUNT; uT++)
	{
		// FIXED constant angle -- NOT drawn from the RNG (radial symmetry is
		// deterministic, keyed only on the tendril index).
		const float fAngle = (float)uT * (fZM_FLOATER_TWO_PI / (float)uFLOATER_TENDRIL_COUNT);
		const float fCos    = cosf(fAngle);
		const float fSin    = sinf(fAngle);

		const Zenith_Maths::Vector3 xTendrilBase(fAttachRadius * fCos, fBaseY, fAttachRadius * fSin);
		const Zenith_Maths::Vector3 xTendrilTip (fTipRadius    * fCos, fTipY,  fTipRadius    * fSin);
		ZM_FloaterAppendTendril(xMesh, uBellyBone, xBellyWorld, xTendrilBase, xTendrilTip,
			fTendrilBaseR, fTendrilTipR, xTendrilIsland, s_aszTendrilNames[uT]);
	}

	// NB: no finalise (tangents / weight-normalise) and no bake here -- the driver
	// ZM_BuildCreatureMesh owns the single finalise order after this returns. The
	// loft's analytic ring normals are left in place (no direct vertex sculpt).
	const u_int uExpectedBones = uSpineCount + 1u + uFLOATER_TENDRIL_COUNT;   // bulb + crown + tendrils
	Zenith_Assert(xMesh.GetNumBones() == uExpectedBones,
		"ZM_BuildArchetype_FloaterPlantoid: bone count %u != expected %u (bulb+crown+tendrils)",
		xMesh.GetNumBones(), uExpectedBones);
	Zenith_Assert(xMesh.GetNumBones() <= uZM_GEN_CREATURE_BONE_CAP,
		"ZM_BuildArchetype_FloaterPlantoid: bone count %u exceeds the creature cap %u",
		xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP);
}
