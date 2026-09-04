#include "Zenith.h"

// ============================================================================
// ZM_HumanMesh -- the S4 SC2/SC3 per-model human mesh loft. The authored ring rows
// ARE the StickFigure golden torso/head/arm/leg tables, and generator v6 keeps the
// loft in StickFigure's own bind space so the mesh can be skinned directly against
// the shared engine rig. BUILD girth and the fixed MESH-domain draw stream vary
// radii only; the separate modest recipe height scales authored Y about the feet.
// The shared rig, Cx/Cz centres and bone indices never vary.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"

namespace
{
	// Private mirror of the SHARED StickFigure rig's first sixteen bone indices --
	// Root..RightFoot, in that rig's own emit order (Tools/Zenith_Tools_TestAssetExport.cpp
	// declares the same sixteen as STICK_BONE_ROOT..STICK_BONE_RIGHT_FOOT). Keeping
	// the mesh bind local to this TU makes every authored weight visibly target one
	// of those exact indices.
	//
	// Zenithmon's body loft weights to the CORE SIXTEEN ONLY: it lofts no fingers,
	// toes, jaw or eyes, so StickFigure's bones 16..50 simply carry no weight here.
	// That is a strict subset, not a mismatch -- unweighted bones still animate, they
	// just move no Zenithmon vertices. ZM_HumanRigMatchesStickFigure_Test loads the
	// real .zskel and pins that these sixteen names still sit at these sixteen
	// indices, so a StickFigure reorder reds this game rather than silently
	// re-skinning every human onto the wrong bones.
	enum : u_int
	{
		HB_ROOT = 0u, HB_SPINE, HB_NECK, HB_HEAD,
		HB_LUARM, HB_LLARM, HB_LHAND,
		HB_RUARM, HB_RLARM, HB_RHAND,
		HB_LULEG, HB_LLLEG, HB_LFOOT,
		HB_RULEG, HB_RLLEG, HB_RFOOT
	};
	static_assert(HB_RFOOT + 1u == uZM_HUMAN_CORE_BONE_COUNT,
		"human mesh bone indices must match the shared StickFigure rig's core prefix");
	static_assert(uZM_HUMAN_CORE_BONE_COUNT <= uZM_STICKFIGURE_BONE_COUNT,
		"the core prefix cannot be longer than the rig it is a prefix of");

	constexpr u_int uZM_HUMAN_RING_SUBDIV = 4u;

	// BUILD's full girth factor. It is applied directly to the torso, then
	// attenuated for the head and limbs so the fixed skeleton stays inside them.
	float ZM_HumanBuildWidthScale(ZM_HUMAN_BUILD eBuild)
	{
		switch (eBuild)
		{
		case ZM_HUMAN_BUILD_SLIGHT:  return 0.85f;
		case ZM_HUMAN_BUILD_AVERAGE: return 1.00f;
		case ZM_HUMAN_BUILD_STOCKY:  return 1.25f;
		case ZM_HUMAN_BUILD_TALL:    return 1.00f;
		default:                     return 1.00f;
		}
	}

	float ZM_HumanBuildSuperEllipse(ZM_HUMAN_BUILD eBuild)
	{
		return (eBuild == ZM_HUMAN_BUILD_STOCKY) ? 0.82f : 1.00f;
	}

	float ZM_HumanAttenuateBuild(float fBuildWidth, float fAmount)
	{
		return 1.0f + (fBuildWidth - 1.0f) * fAmount;
	}

	float ZM_HumanClampSuperEllipse(float fValue)
	{
		if (fValue <= 0.0f) { return 0.01f; }
		if (fValue > 1.0f)  { return 1.00f; }
		return fValue;
	}

	// The authored rows ARE StickFigure's bind space (root at y=0, feet at y=-1),
	// and that is the space the mesh stays in -- it is the space the shared
	// StickFigure .zskel poses its bones in, and a mesh may only be skinned against
	// a skeleton expressed in its own bind space.
	//
	// The recipe's intentionally modest height varies about the FEET, not the root,
	// so a taller human grows upward from the floor rather than sinking. That is the
	// +1 / -1 round trip below: lift to feet-at-zero, scale, drop back. At the
	// canonical fHeightScale of 1.0 it is exactly the identity, so the canonical
	// model's mesh IS StickFigure's authored space bit for bit. Cx/Cz never move.
	void ZM_PrepareHumanRings(ZM_LoftRing* pxRings, u_int uNumRings,
		float fHeightScale, float fRxScale, float fRzScale, float fSuperEllipse)
	{
		for (u_int u = 0u; u < uNumRings; ++u)
		{
			ZM_LoftRing& xRing = pxRings[u];
			xRing.m_fY = (xRing.m_fY + 1.0f) * fHeightScale - 1.0f;
			xRing.m_fRx *= fRxScale;
			xRing.m_fRz *= fRzScale;
			xRing.m_fSuperEllipse = fSuperEllipse;
		}
	}

	void ZM_AppendHumanPart(ZM_GenMesh& xMesh, const ZM_LoftRing* pxRings,
		u_int uNumRings, u_int uSegs, const ZM_GenUVIsland& xIsland,
		bool bCapStart, bool bCapEnd)
	{
		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = pxRings;
		xPart.m_uNumRings = uNumRings;
		xPart.m_uSegs     = uSegs;
		xPart.m_xIsland   = xIsland;
		xPart.m_bCapStart = bCapStart;
		xPart.m_bCapEnd   = bCapEnd;
		xPart.m_uSubdiv   = uZM_HUMAN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);
	}

	void ZM_AppendHumanTorso(ZM_GenMesh& xMesh, float fHeightScale,
		float fRxScale, float fRzScale, float fSuperEllipse)
	{
		// StickFigure golden rows, top -> bottom.
		ZM_LoftRing axRings[] =
		{
			// y       cx    cz       rx      rz      boneA    boneB    blend
			{  1.198f, 0.0f, -0.006f, 0.070f, 0.068f, HB_SPINE, HB_SPINE, 0.00f },
			{  1.172f, 0.0f, -0.006f, 0.116f, 0.094f, HB_SPINE, HB_SPINE, 0.00f },
			{  1.145f, 0.0f, -0.004f, 0.150f, 0.092f, HB_SPINE, HB_SPINE, 0.00f },
			{  1.060f, 0.0f, -0.006f, 0.235f, 0.112f, HB_SPINE, HB_SPINE, 0.00f },
			{  0.950f, 0.0f, -0.005f, 0.224f, 0.124f, HB_SPINE, HB_SPINE, 0.00f },
			{  0.800f, 0.0f,  0.002f, 0.202f, 0.124f, HB_SPINE, HB_SPINE, 0.00f },
			{  0.620f, 0.0f,  0.004f, 0.178f, 0.111f, HB_SPINE, HB_SPINE, 0.00f },
			{  0.450f, 0.0f,  0.002f, 0.162f, 0.101f, HB_ROOT,  HB_SPINE, 0.80f },
			{  0.300f, 0.0f,  0.000f, 0.155f, 0.097f, HB_ROOT,  HB_SPINE, 0.50f },
			{  0.180f, 0.0f,  0.000f, 0.160f, 0.108f, HB_ROOT,  HB_SPINE, 0.22f },
			{  0.060f, 0.0f,  0.000f, 0.198f, 0.118f, HB_ROOT,  HB_ROOT,  0.00f },
			{ -0.040f, 0.0f,  0.000f, 0.206f, 0.120f, HB_ROOT,  HB_ROOT,  0.00f },
			{ -0.120f, 0.0f,  0.000f, 0.182f, 0.110f, HB_ROOT,  HB_ROOT,  0.00f },
		};
		constexpr u_int uRINGS = sizeof(axRings) / sizeof(axRings[0]);
		static_assert(uRINGS == 13u, "human torso must retain all 13 golden rings");
		ZM_PrepareHumanRings(axRings, uRINGS, fHeightScale, fRxScale, fRzScale, fSuperEllipse);
		ZM_AppendHumanPart(xMesh, axRings, uRINGS, 48u, xZM_HUMAN_UV_TORSO, true, true);
	}

	void ZM_AppendHumanHeadNeck(ZM_GenMesh& xMesh, float fHeightScale, float fRadiusScale)
	{
		// StickFigure golden rows, crown -> neck base.
		ZM_LoftRing axRings[] =
		{
			// y      cx    cz       rx       rz       boneA    boneB    blend
			{ 1.575f, 0.0f, -0.008f, 0.0667f, 0.0678f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.525f, 0.0f, -0.004f, 0.0966f, 0.0975f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.465f, 0.0f,  0.000f, 0.0989f, 0.1018f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.400f, 0.0f,  0.006f, 0.0920f, 0.0975f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.340f, 0.0f,  0.008f, 0.0828f, 0.0869f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.300f, 0.0f,  0.004f, 0.0725f, 0.0742f, HB_HEAD,  HB_HEAD, 0.00f },
			{ 1.270f, 0.0f,  0.002f, 0.0644f, 0.0616f, HB_NECK,  HB_HEAD, 0.60f },
			{ 1.200f, 0.0f,  0.000f, 0.0638f, 0.0638f, HB_NECK,  HB_NECK, 0.00f },
			{ 1.130f, 0.0f, -0.002f, 0.0694f, 0.0682f, HB_SPINE, HB_NECK, 0.60f },
		};
		constexpr u_int uRINGS = sizeof(axRings) / sizeof(axRings[0]);
		static_assert(uRINGS == 9u, "human head/neck must retain all 9 golden rings");
		ZM_PrepareHumanRings(axRings, uRINGS, fHeightScale, fRadiusScale, fRadiusScale, 1.0f);
		// Crown closes; the neck base remains buried in the torso like StickFigure.
		ZM_AppendHumanPart(xMesh, axRings, uRINGS, 64u, xZM_HUMAN_UV_HEAD, true, false);
	}

	void ZM_AppendHumanArm(ZM_GenMesh& xMesh, float fSide, u_int uUpper,
		u_int uLower, u_int uHand, const ZM_GenUVIsland& xIsland,
		float fHeightScale, float fRadiusScale)
	{
		ZM_LoftRing axRings[] =
		{
			// y      cx              cz       rx      rz      boneA    boneB    blend
			{ 1.150f, fSide * 0.205f, -0.004f, 0.102f, 0.094f, HB_SPINE, uUpper, 0.15f },
			{ 1.095f, fSide * 0.248f,  0.000f, 0.096f, 0.086f, HB_SPINE, uUpper, 0.50f },
			{ 1.020f, fSide * 0.290f,  0.000f, 0.080f, 0.072f, uUpper,   uUpper, 0.00f },
			{ 0.920f, fSide * 0.300f,  0.000f, 0.066f, 0.061f, uUpper,   uUpper, 0.00f },
			{ 0.790f, fSide * 0.300f,  0.000f, 0.053f, 0.050f, uUpper,   uUpper, 0.00f },
			{ 0.748f, fSide * 0.300f,  0.000f, 0.049f, 0.047f, uUpper,   uLower, 0.20f },
			{ 0.715f, fSide * 0.300f,  0.000f, 0.046f, 0.045f, uUpper,   uLower, 0.50f },
			{ 0.682f, fSide * 0.300f,  0.000f, 0.049f, 0.047f, uUpper,   uLower, 0.80f },
			{ 0.640f, fSide * 0.300f,  0.000f, 0.055f, 0.053f, uLower,   uLower, 0.00f },
			{ 0.520f, fSide * 0.300f,  0.000f, 0.044f, 0.043f, uLower,   uLower, 0.00f },
			{ 0.435f, fSide * 0.300f,  0.000f, 0.031f, 0.033f, uLower,   uHand,  0.45f },
		};
		constexpr u_int uRINGS = sizeof(axRings) / sizeof(axRings[0]);
		static_assert(uRINGS == 11u, "human arm must retain all 11 golden rings");
		ZM_PrepareHumanRings(axRings, uRINGS, fHeightScale, fRadiusScale, fRadiusScale, 1.0f);
		ZM_AppendHumanPart(xMesh, axRings, uRINGS, 28u, xIsland, true, true);
	}

	void ZM_AppendHumanLeg(ZM_GenMesh& xMesh, float fSide, u_int uUpper,
		u_int uLower, u_int uFoot, const ZM_GenUVIsland& xIsland,
		float fHeightScale, float fRadiusScale)
	{
		ZM_LoftRing axRings[] =
		{
			// y       cx              cz       rx      rz      boneA   boneB    blend
			{  0.075f, fSide * 0.128f,  0.004f, 0.058f, 0.066f, HB_ROOT, uUpper, 0.18f },
			{ -0.020f, fSide * 0.140f,  0.004f, 0.094f, 0.104f, HB_ROOT, uUpper, 0.50f },
			{ -0.120f, fSide * 0.146f,  0.004f, 0.093f, 0.103f, HB_ROOT, uUpper, 0.88f },
			{ -0.250f, fSide * 0.150f,  0.002f, 0.089f, 0.096f, uUpper,  uUpper, 0.00f },
			{ -0.400f, fSide * 0.150f,  0.000f, 0.073f, 0.080f, uUpper,  uUpper, 0.00f },
			{ -0.445f, fSide * 0.150f,  0.000f, 0.066f, 0.071f, uUpper,  uLower, 0.20f },
			{ -0.480f, fSide * 0.150f,  0.000f, 0.063f, 0.068f, uUpper,  uLower, 0.50f },
			{ -0.515f, fSide * 0.150f, -0.001f, 0.064f, 0.070f, uUpper,  uLower, 0.80f },
			{ -0.560f, fSide * 0.150f, -0.002f, 0.064f, 0.070f, uLower,  uLower, 0.00f },
			{ -0.660f, fSide * 0.150f, -0.008f, 0.071f, 0.079f, uLower,  uLower, 0.00f },
			{ -0.800f, fSide * 0.150f, -0.004f, 0.052f, 0.058f, uLower,  uLower, 0.00f },
			{ -0.920f, fSide * 0.150f,  0.000f, 0.040f, 0.044f, uLower,  uFoot,  0.40f },
			{ -0.975f, fSide * 0.150f,  0.000f, 0.037f, 0.041f, uFoot,   uFoot,  0.00f },
		};
		constexpr u_int uRINGS = sizeof(axRings) / sizeof(axRings[0]);
		static_assert(uRINGS == 13u, "human leg must retain all 13 golden rings");
		ZM_PrepareHumanRings(axRings, uRINGS, fHeightScale, fRadiusScale, fRadiusScale, 1.0f);
		ZM_AppendHumanPart(xMesh, axRings, uRINGS, 36u, xIsland, true, true);
	}
}

namespace
{
	// The rig-space build, shared by ZM_BuildHumanMesh and ZM_MeasureHumanBody so the
	// metrics can never be measured against a different mesh than the one that ships.
	// Returns the BODY VERTEX PREFIX: the vertex count captured immediately before
	// ZM_AppendHumanAppearanceMesh, i.e. the six body loft parts and nothing else.
	// Hair and attachments live past it, which is exactly why a hat cannot decide how
	// tall a person is.
	u_int ZM_BuildHumanMeshInRigSpace(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
	{
		xMesh.Reset();

		// ALL randomness is consumed up front in this fixed order. Left/right limbs
		// share their girth draws so the bind-pose silhouette stays mirrored.
		ZM_GenRNG xMeshRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_MESH);
		const float fTorsoRxJ       = xMeshRng.NextFloatRange(0.96f, 1.04f); // 1 side girth
		const float fTorsoRzJ       = xMeshRng.NextFloatRange(0.96f, 1.04f); // 2 front/back girth
		const float fTorsoSuperJ    = xMeshRng.NextFloatRange(0.96f, 1.00f); // 3 torso roundness
		const float fHeadSizeJ      = xMeshRng.NextFloatRange(0.97f, 1.03f); // 4 head size
		const float fArmGirthJ      = xMeshRng.NextFloatRange(0.94f, 1.06f); // 5 shared arm girth
		const float fLegGirthJ      = xMeshRng.NextFloatRange(0.94f, 1.06f); // 6 shared leg girth

		const float fBuildWidth = ZM_HumanBuildWidthScale(xRecipe.m_eBuild);
		const float fHeadBuild  = ZM_HumanAttenuateBuild(fBuildWidth, 0.25f);
		const float fLimbBuild  = ZM_HumanAttenuateBuild(fBuildWidth, 0.65f);
		const float fTorsoSuper = ZM_HumanClampSuperEllipse(
			ZM_HumanBuildSuperEllipse(xRecipe.m_eBuild) * fTorsoSuperJ);

		// ★ NO BONES ARE EMITTED. The rig is the shared engine asset
		// (engine:Meshes/StickFigure/StickFigure.zskel); this mesh only ever REFERS to
		// it, by index, and ZM_GenBakeMeshWithSharedSkeleton writes that reference
		// rather than a skeleton. ZM_GenMesh::m_xBones therefore stays empty.
		ZM_AppendHumanTorso(xMesh, xRecipe.m_fHeightScale,
			fBuildWidth * fTorsoRxJ, fBuildWidth * fTorsoRzJ, fTorsoSuper);
		ZM_AppendHumanHeadNeck(xMesh, xRecipe.m_fHeightScale, fHeadBuild * fHeadSizeJ);
		ZM_AppendHumanArm(xMesh, -1.0f, HB_LUARM, HB_LLARM, HB_LHAND, xZM_HUMAN_UV_ARM_L,
			xRecipe.m_fHeightScale, fLimbBuild * fArmGirthJ);
		ZM_AppendHumanArm(xMesh,  1.0f, HB_RUARM, HB_RLARM, HB_RHAND, xZM_HUMAN_UV_ARM_R,
			xRecipe.m_fHeightScale, fLimbBuild * fArmGirthJ);
		ZM_AppendHumanLeg(xMesh, -1.0f, HB_LULEG, HB_LLLEG, HB_LFOOT, xZM_HUMAN_UV_LEG_L,
			xRecipe.m_fHeightScale, fLimbBuild * fLegGirthJ);
		ZM_AppendHumanLeg(xMesh,  1.0f, HB_RULEG, HB_RLLEG, HB_RFOOT, xZM_HUMAN_UV_LEG_R,
			xRecipe.m_fHeightScale, fLimbBuild * fLegGirthJ);

		const u_int uBodyVertexPrefix = xMesh.GetNumVerts();
		ZM_AppendHumanAppearanceMesh(xRecipe, xMesh);
		return uBodyVertexPrefix;
	}
}

// ============================================================================
// Per-model mesh builder: exactly six body loft parts and exactly six MESH-domain
// proportion draws, followed by categorical SC3 appearance parts, then the v2
// centre anchor. No SKELETON-domain draws are permitted because every model binds
// the same fixed skeleton.
// ============================================================================
void ZM_BuildHumanMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	ZM_BuildHumanMeshInRigSpace(xRecipe, xMesh);

	// ★ NO ANCHOR PASS. Generator v6 leaves the mesh in the shared StickFigure rig's
	// OWN bind space, because that is the only space it can legally be skinned in --
	// the bones it weights to are that rig's bones, posed about that rig's pivots. A
	// translation applied here (v2's centre anchor) moved the vertices WITHOUT
	// moving the pivots, which is exactly the desync that made a game-owned copy of
	// the skeleton necessary in the first place.
	//
	// Where the model then sits relative to a Zenithmon entity is a GAME statement,
	// and it is made once, in metres, by fZM_HUMAN_MODEL_OFFSET_Y in ZM_HumanBody.h.

	// EmitRing already wrote analytic loft normals; never regenerate them. This is
	// the sole finalisation sequence and is intentionally byte-idempotent.
	ZM_GenGenerateTangents(xMesh);
	ZM_GenNormalizeSkinWeights(xMesh);
}

// ============================================================================
// Body metrics -- measured over the body vertex prefix, in the shared rig's bind
// space. Deliberately NOT derived from the finished mesh, which carries
// hair/attachment vertices that must not define the body: a hat does not decide
// how tall someone is.
//
// These are the numbers fZM_HUMAN_MODEL_OFFSET_Y is derived from, so they are
// measured in the SAME space the model is placed in -- m_fMinY is literally "how
// far below the rig's origin this body's feet reach".
// ============================================================================
ZM_HumanBodyMetrics ZM_MeasureHumanBody(ZM_HUMAN_ID eId)
{
	return ZM_MeasureHumanBody(ZM_ResolveHumanRecipe(eId));
}

ZM_HumanBodyMetrics ZM_MeasureHumanBody(const ZM_HumanRecipe& xRecipe)
{
	ZM_GenMesh xMesh;
	const u_int uBodyVertexPrefix = ZM_BuildHumanMeshInRigSpace(xRecipe, xMesh);

	ZM_HumanBodyMetrics xMetrics;
	if (uBodyVertexPrefix == 0u)
	{
		return xMetrics;   // defined answer for an impossible mesh; never a read past the end
	}

	float fMinY = xMesh.m_xPositions.Get(0u).y;
	float fMaxY = fMinY;
	for (u_int u = 1u; u < uBodyVertexPrefix; ++u)
	{
		const float fY = xMesh.m_xPositions.Get(u).y;
		if (fY < fMinY) { fMinY = fY; }
		if (fY > fMaxY) { fMaxY = fY; }
	}

	xMetrics.m_fMinY            = fMinY;
	xMetrics.m_fMaxY            = fMaxY;
	xMetrics.m_fHeight          = fMaxY - fMinY;
	xMetrics.m_fCentreY         = 0.5f * (fMinY + fMaxY);
	xMetrics.m_uBodyVertexCount = uBodyVertexPrefix;
	return xMetrics;
}
