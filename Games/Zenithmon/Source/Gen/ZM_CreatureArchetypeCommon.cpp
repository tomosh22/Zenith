#include "Zenith.h"

// ============================================================================
// ZM_CreatureArchetypeCommon -- shared appendage kit implementation. See the
// header for the geometry model + invariants. Every helper builds a small local
// ZM_LoftRing table (each authored ring bound to a SINGLE bone so the loft's
// Catmull-Rom subdivision never exceeds a 2-bone blend) and sweeps it through
// ZM_MeshLoft::AppendPart, after adding its bones parent-before-child.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"
#include "Collections/Zenith_Vector.h"

#include <cstdio>    // snprintf
#include <cmath>     // sinf, sqrtf, fabsf

namespace
{
	constexpr float fZM_KIT_PI = 3.14159265358979323846f;

	// Add a bone at a MODEL-space (world) position, computing its local offset
	// from the parent's world position (identity rotation, unit scale). iParent<0
	// makes a root whose local pos is its world pos.
	u_int ZM_KitAddBoneWorld(ZM_GenMesh& xMesh, const char* szName, int iParent,
		const Zenith_Maths::Vector3& xParentWorld, const Zenith_Maths::Vector3& xThisWorld)
	{
		const Zenith_Maths::Vector3 xLocal = (iParent < 0)
			? xThisWorld
			: (xThisWorld - xParentWorld);
		return ZM_GenAddBone(xMesh, szName, iParent, xLocal,
			glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
	}

	// Build + sweep a tube from parallel per-ring arrays: centre (model space, its
	// Y is the sweep coordinate), half-extents (Rx,Rz), and the SINGLE bone each
	// authored ring binds to. Returns the first appended vertex index.
	u_int ZM_KitEmitTube(ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3* pxCentres, const float* pfRx, const float* pfRz,
		const u_int* puBoneForRing, float fSuperEllipse, u_int uRings,
		u_int uSegs, const ZM_GenUVIsland& xIsland, bool bCapStart, bool bCapEnd)
	{
		Zenith_Assert(uRings >= 2u, "ZM_KitEmitTube needs at least 2 rings (uRings=%u)", uRings);

		Zenith_Vector<ZM_LoftRing> xRings;
		xRings.Reserve(uRings);
		for (u_int u = 0; u < uRings; u++)
		{
			ZM_LoftRing xRing;
			xRing.m_fY  = pxCentres[u].y;
			xRing.m_fCx = pxCentres[u].x;
			xRing.m_fCz = pxCentres[u].z;
			xRing.m_fRx = pfRx[u];
			xRing.m_fRz = pfRz[u];
			xRing.m_uBoneA  = puBoneForRing[u];
			xRing.m_uBoneB  = puBoneForRing[u];   // single-bone authored ring; subdivision blends
			xRing.m_fBlendB = 0.0f;
			xRing.m_fSuperEllipse = fSuperEllipse;
			xRings.PushBack(xRing);

			// Distinct consecutive ring Y (the loft's no-flat-washer precondition).
			if (u > 0u)
			{
				Zenith_Assert(fabsf(pxCentres[u].y - pxCentres[u - 1u].y) > 1.0e-5f,
					"ZM_KitEmitTube: consecutive rings share Y (degenerate flat washer)");
			}
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = xRings.GetDataPointer();
		xPart.m_uNumRings = xRings.GetSize();
		xPart.m_uSegs     = uSegs;
		xPart.m_xIsland   = xIsland;
		xPart.m_bCapStart = bCapStart;
		xPart.m_bCapEnd   = bCapEnd;
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		return ZM_MeshLoft::AppendPart(xMesh, xPart);
	}
}

// ---------------------------------------------------------------------------
// (P3) size-class world scale -- GOLDEN.
// ---------------------------------------------------------------------------
float ZM_SizeClassScale(ZM_SIZE_CLASS eSize)
{
	switch (eSize)
	{
	case ZM_SIZE_TINY:   return 0.45f;
	case ZM_SIZE_SMALL:  return 0.70f;
	case ZM_SIZE_MEDIUM: return 1.00f;
	case ZM_SIZE_LARGE:  return 1.50f;
	case ZM_SIZE_HUGE:   return 2.20f;
	default:
		Zenith_Assert(false, "ZM_SizeClassScale: size class %u out of range", (u_int)eSize);
		return 1.0f;
	}
}

// ---------------------------------------------------------------------------
void ZM_FormatBoneName(char* szOut, u_int uCap, const char* szBase, int iIndex)
{
	Zenith_Assert(szOut != nullptr && uCap > 0u, "ZM_FormatBoneName: bad output buffer");
	if (szBase == nullptr) { szBase = ""; }
	if (iIndex < 0)
	{
		snprintf(szOut, (size_t)uCap, "%s", szBase);
	}
	else
	{
		snprintf(szOut, (size_t)uCap, "%s%02d", szBase, iIndex);
	}
}

// ---------------------------------------------------------------------------
u_int ZM_AppendSpineTube(ZM_GenMesh& xMesh, const ZM_KitSpineParams& xParams,
	u_int* pauOutBones, u_int uMaxOutBones)
{
	Zenith_Assert(pauOutBones != nullptr && uMaxOutBones >= 2u,
		"ZM_AppendSpineTube: need room for at least 2 out bones");
	Zenith_Assert(xParams.m_fLength > 1.0e-4f, "ZM_AppendSpineTube: length must be > 0");

	u_int uNodes = xParams.m_uSegments;
	if (uNodes < 2u) { uNodes = 2u; }
	if (uNodes > 8u) { uNodes = 8u; }
	if (uNodes > uMaxOutBones) { uNodes = uMaxOutBones; }

	Zenith_Maths::Vector3 axCentre[8];
	float                 afRx[8];
	float                 afRz[8];
	u_int                 auBone[8];

	// Bones + ring nodes: sweep belly (t=0) -> back (t=1), radii fullest at mid.
	Zenith_Maths::Vector3 xPrevWorld = xParams.m_xParentWorld;
	int iPrevBone = xParams.m_iParentBone;
	for (u_int u = 0; u < uNodes; u++)
	{
		const float fT = (uNodes > 1u) ? ((float)u / (float)(uNodes - 1u)) : 0.0f;
		const float fY = xParams.m_xBellyCentre.y + fT * xParams.m_fLength;
		const Zenith_Maths::Vector3 xWorld(xParams.m_xBellyCentre.x, fY, xParams.m_xBellyCentre.z);

		// Fullest at t=0.5, tapering to m_fEndTaper at both ends.
		const float fProfile = xParams.m_fEndTaper
			+ (1.0f - xParams.m_fEndTaper) * sinf(fZM_KIT_PI * fT);

		char acName[uZM_GEN_BONE_NAME_MAX];
		ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, xParams.m_szNamePrefix, (int)u);
		const u_int uBone = ZM_KitAddBoneWorld(xMesh, acName, iPrevBone, xPrevWorld, xWorld);

		axCentre[u] = xWorld;
		afRx[u]     = xParams.m_fHalfWidth * fProfile;
		afRz[u]     = xParams.m_fHalfDepth * fProfile;
		auBone[u]   = uBone;
		pauOutBones[u] = uBone;

		xPrevWorld = xWorld;
		iPrevBone  = (int)uBone;
	}

	ZM_KitEmitTube(xMesh, axCentre, afRx, afRz, auBone, xParams.m_fBellyRound,
		uNodes, xParams.m_uSegs, xParams.m_xIsland, /*capStart*/ true, /*capEnd*/ true);

	return uNodes;
}

// ---------------------------------------------------------------------------
ZM_KitAppendResult ZM_AppendLimb(ZM_GenMesh& xMesh, const ZM_KitLimbParams& xParams)
{
	Zenith_Assert(xParams.m_xHip.y > xParams.m_xKnee.y && xParams.m_xKnee.y > xParams.m_xFoot.y,
		"ZM_AppendLimb: hip/knee/foot must strictly descend in Y");

	char acUp[uZM_GEN_BONE_NAME_MAX];
	char acLo[uZM_GEN_BONE_NAME_MAX];
	snprintf(acUp, sizeof(acUp), "%sUp", xParams.m_szName);
	snprintf(acLo, sizeof(acLo), "%sLo", xParams.m_szName);

	const u_int uUp = ZM_KitAddBoneWorld(xMesh, acUp, xParams.m_iParentBone,
		xParams.m_xParentWorld, xParams.m_xHip);
	const u_int uLo = ZM_KitAddBoneWorld(xMesh, acLo, (int)uUp,
		xParams.m_xHip, xParams.m_xKnee);

	const Zenith_Maths::Vector3 axCentre[3] = { xParams.m_xHip, xParams.m_xKnee, xParams.m_xFoot };
	const float                 afRx[3]     = { xParams.m_fRadiusTop, xParams.m_fRadiusMid, xParams.m_fRadiusFoot };
	const float                 afRz[3]     = { xParams.m_fRadiusTop, xParams.m_fRadiusMid, xParams.m_fRadiusFoot };
	const u_int                 auBone[3]   = { uUp, uLo, uLo };

	const u_int uFirstVert = ZM_KitEmitTube(xMesh, axCentre, afRx, afRz, auBone, 1.0f,
		3u, xParams.m_uSegs, xParams.m_xIsland, /*capStart*/ false, /*capEnd*/ true);

	ZM_KitAppendResult xResult;
	xResult.m_uFirstBone = uUp;
	xResult.m_uTipBone   = uLo;
	xResult.m_xTipWorld  = xParams.m_xFoot;   // geometric extremity, for chaining
	xResult.m_uFirstVert = uFirstVert;
	return xResult;
}

// ---------------------------------------------------------------------------
ZM_KitAppendResult ZM_AppendTail(ZM_GenMesh& xMesh, const ZM_KitTailParams& xParams)
{
	Zenith_Assert(fabsf(xParams.m_xBase.y - xParams.m_xTip.y) > 1.0e-4f,
		"ZM_AppendTail: base and tip must differ in Y");

	u_int uNodes = xParams.m_uSegments;
	if (uNodes < 2u) { uNodes = 2u; }
	if (uNodes > 6u) { uNodes = 6u; }

	Zenith_Maths::Vector3 axCentre[6];
	float                 afRx[6];
	float                 afRz[6];
	u_int                 auBone[6];

	Zenith_Maths::Vector3 xPrevWorld = xParams.m_xParentWorld;
	int iPrevBone = xParams.m_iParentBone;
	for (u_int u = 0; u < uNodes; u++)
	{
		const float fT = (float)u / (float)(uNodes - 1u);
		const Zenith_Maths::Vector3 xWorld = xParams.m_xBase + (xParams.m_xTip - xParams.m_xBase) * fT;
		const float fRadius = xParams.m_fRadiusBase + (xParams.m_fRadiusTip - xParams.m_fRadiusBase) * fT;

		char acName[uZM_GEN_BONE_NAME_MAX];
		ZM_FormatBoneName(acName, uZM_GEN_BONE_NAME_MAX, xParams.m_szNamePrefix, (int)u);
		const u_int uBone = ZM_KitAddBoneWorld(xMesh, acName, iPrevBone, xPrevWorld, xWorld);

		axCentre[u] = xWorld;
		afRx[u]     = fRadius;
		afRz[u]     = fRadius;
		auBone[u]   = uBone;

		xPrevWorld = xWorld;
		iPrevBone  = (int)uBone;
	}

	const u_int uFirstVert = ZM_KitEmitTube(xMesh, axCentre, afRx, afRz, auBone, 1.0f,
		uNodes, xParams.m_uSegs, xParams.m_xIsland, /*capStart*/ false, /*capEnd*/ true);

	ZM_KitAppendResult xResult;
	xResult.m_uFirstBone = auBone[0];
	xResult.m_uTipBone   = auBone[uNodes - 1u];
	xResult.m_xTipWorld  = xParams.m_xTip;
	xResult.m_uFirstVert = uFirstVert;
	return xResult;
}

// ---------------------------------------------------------------------------
ZM_KitAppendResult ZM_AppendHorn(ZM_GenMesh& xMesh, const ZM_KitHornParams& xParams)
{
	Zenith_Assert(xParams.m_xTip.y > xParams.m_xBase.y,
		"ZM_AppendHorn: tip must be above the base in Y");

	const u_int uBone = ZM_KitAddBoneWorld(xMesh, xParams.m_szName, xParams.m_iParentBone,
		xParams.m_xParentWorld, xParams.m_xBase);

	constexpr u_int uRINGS = 4u;
	Zenith_Maths::Vector3 axCentre[uRINGS];
	float                 afRx[uRINGS];
	float                 afRz[uRINGS];
	u_int                 auBone[uRINGS];
	for (u_int u = 0; u < uRINGS; u++)
	{
		const float fT = (float)u / (float)(uRINGS - 1u);
		axCentre[u] = xParams.m_xBase + (xParams.m_xTip - xParams.m_xBase) * fT;
		const float fRadius = xParams.m_fRadiusBase * (1.0f - 0.92f * fT);   // taper toward the tip
		afRx[u]   = fRadius;
		afRz[u]   = fRadius;
		auBone[u] = uBone;
	}

	const u_int uFirstVert = ZM_KitEmitTube(xMesh, axCentre, afRx, afRz, auBone, 1.0f,
		uRINGS, xParams.m_uSegs, xParams.m_xIsland, /*capStart*/ false, /*capEnd*/ true);

	ZM_KitAppendResult xResult;
	xResult.m_uFirstBone = uBone;
	xResult.m_uTipBone   = uBone;
	xResult.m_xTipWorld  = xParams.m_xTip;
	xResult.m_uFirstVert = uFirstVert;
	return xResult;
}

// ---------------------------------------------------------------------------
ZM_KitAppendResult ZM_AppendEllipsoidHead(ZM_GenMesh& xMesh, const ZM_KitHeadParams& xParams)
{
	Zenith_Assert(xParams.m_xHalfExtents.x > 0.0f && xParams.m_xHalfExtents.y > 0.0f
		&& xParams.m_xHalfExtents.z > 0.0f, "ZM_AppendEllipsoidHead: half-extents must be positive");

	u_int uRings = xParams.m_uRings;
	if (uRings < 2u) { uRings = 2u; }
	if (uRings > 8u) { uRings = 8u; }

	const u_int uBone = ZM_KitAddBoneWorld(xMesh, xParams.m_szName, xParams.m_iParentBone,
		xParams.m_xParentWorld, xParams.m_xCentre);

	Zenith_Maths::Vector3 axCentre[8];
	float                 afRx[8];
	float                 afRz[8];
	u_int                 auBone[8];
	for (u_int u = 0; u < uRings; u++)
	{
		// Rings at t in (0,1) exclusive: never a zero-radius pole ring; the poles
		// are closed by the end caps instead.
		const float fT = (float)(u + 1u) / (float)(uRings + 1u);
		const float fY = xParams.m_xCentre.y - xParams.m_xHalfExtents.y
			+ 2.0f * xParams.m_xHalfExtents.y * fT;
		const float fSpan = 1.0f - (2.0f * fT - 1.0f) * (2.0f * fT - 1.0f);
		const float fFactor = sqrtf(fSpan > 0.0f ? fSpan : 0.0f);

		axCentre[u] = Zenith_Maths::Vector3(xParams.m_xCentre.x, fY, xParams.m_xCentre.z);
		afRx[u]   = xParams.m_xHalfExtents.x * fFactor;
		afRz[u]   = xParams.m_xHalfExtents.z * fFactor;
		auBone[u] = uBone;
	}

	const u_int uFirstVert = ZM_KitEmitTube(xMesh, axCentre, afRx, afRz, auBone, 1.0f,
		uRings, xParams.m_uSegs, xParams.m_xIsland, /*capStart*/ true, /*capEnd*/ true);

	ZM_KitAppendResult xResult;
	xResult.m_uFirstBone = uBone;
	xResult.m_uTipBone   = uBone;
	xResult.m_xTipWorld  = xParams.m_xCentre;
	xResult.m_uFirstVert = uFirstVert;
	return xResult;
}
