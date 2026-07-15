#include "Zenith.h"

// ============================================================================
// ZM_HumanMesh -- the S4 SC2 per-model human mesh loft. The authored ring rows
// are the StickFigure golden torso/head/arm/leg tables, translated +1Y so the
// shared feet remain near world y=0. BUILD girth and the fixed MESH-domain draw
// stream vary radii only; the separate modest recipe height scales authored Y.
// The shared 16-bone skeleton, Cx/Cz centres and bone indices never vary.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanGen.h"

namespace
{
	// Private mirror of ZM_AppendSharedHumanBones' frozen emit order. Keeping the
	// mesh bind local to this TU makes every authored weight visibly target one of
	// the exact 16 shared indices.
	enum : u_int
	{
		HB_ROOT = 0u, HB_SPINE, HB_NECK, HB_HEAD,
		HB_LUARM, HB_LLARM, HB_LHAND,
		HB_RUARM, HB_RLARM, HB_RHAND,
		HB_LULEG, HB_LLLEG, HB_LFOOT,
		HB_RULEG, HB_RLLEG, HB_RFOOT
	};
	static_assert(HB_RFOOT + 1u == uZM_HUMAN_BONE_COUNT,
		"human mesh bone indices must match the frozen shared skeleton");

	constexpr u_int uZM_HUMAN_RING_SUBDIV = 4u;

	// StickFigure's golden 1024^2 atlas islands. SC3 paints these same normalized
	// rectangles; SC2 already fixes the topology and UV contract.
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_HEAD  { 0.005f, 0.005f, 0.325f, 0.420f };
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_TORSO { 0.335f, 0.005f, 0.660f, 0.420f };
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_ARM_L { 0.670f, 0.005f, 0.825f, 0.420f };
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_ARM_R { 0.835f, 0.005f, 0.990f, 0.420f };
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_LEG_L { 0.005f, 0.430f, 0.230f, 0.900f };
	constexpr ZM_GenUVIsland xZM_HUMAN_UV_LEG_R { 0.240f, 0.430f, 0.465f, 0.900f };

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

	// Convert authored StickFigure space (root at y=0, feet at y=-1) to the
	// shared-human space (root at y=1, feet at y=0), then apply the recipe's
	// intentionally modest grounded height. Cx/Cz and the skeleton never move.
	void ZM_PrepareHumanRings(ZM_LoftRing* pxRings, u_int uNumRings,
		float fHeightScale, float fRxScale, float fRzScale, float fSuperEllipse)
	{
		for (u_int u = 0u; u < uNumRings; ++u)
		{
			ZM_LoftRing& xRing = pxRings[u];
			xRing.m_fY = (xRing.m_fY + 1.0f) * fHeightScale;
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

// ============================================================================
// Per-model mesh builder: exactly six loft parts and exactly six MESH-domain
// proportion draws. No SKELETON-domain draws are permitted because every model
// binds the same fixed skeleton.
// ============================================================================
void ZM_BuildHumanMesh(const ZM_HumanRecipe& xRecipe, ZM_GenMesh& xMesh)
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

	ZM_AppendSharedHumanBones(xMesh);

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

	// EmitRing already wrote analytic loft normals; never regenerate them. This is
	// the sole finalisation sequence and is intentionally byte-idempotent.
	ZM_GenGenerateTangents(xMesh);
	ZM_GenNormalizeSkinWeights(xMesh);
}
