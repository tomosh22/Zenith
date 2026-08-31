#include "Zenith.h"

//=============================================================================
// Zenith_Tools_FallenTreeAssetExport
//
// The SHARED deadwood set — fallen trunks, a broken stump and loose branches,
// engine assets so any game can litter a wood with them. Generated at every
// tools boot from fixed seeds, exactly like the rock set beside it
// (Zenith_Tools_RockAssetExport.cpp), and sharing its tileable-noise base.
//
//   ENGINE_ASSETS_DIR/Meshes/FallenTrees/
//     FallenTree_Log.{zasset,zmesh,zmodel}       — a long tapered fallen trunk
//     FallenTree_LogMossy.{...}                  — shorter, thicker, moss-grown
//     FallenTree_Stump.{...}                     — a broken stump with root flare
//     FallenTree_Branches.{...}                  — three crossed loose branches
//     FallenTree_{Bark,MossyBark}_{Albedo,Normal,RM,AO}.ztxtr
//     FallenTree_Bark.zmtrl / FallenTree_MossyBark.zmtrl
//
// ★ EVERY PIECE IS AUTHORED STANDING UP, along +Y with its origin on the butt
// end, and the SCATTER lays it down with the instance rotation. That is not a
// quirk — it is what makes the collider work. `Zenith_InstanceColliderConfig`
// can only describe a Y-aligned capsule, but `CreateInstanceBody` rotates the
// capsule (and its local Y offset) by the instance's own rotation, so a log
// modelled along +Y and tipped 90° by the scatter gets a correctly aligned
// HORIZONTAL capsule for free. Modelling the log lying down instead would need
// a per-instance shape axis in the component, i.e. a serialization bump, to say
// the same thing. The stump keeps the same convention and simply is not tipped.
//
// Three things carry the look:
//
// 1. ANISOTROPIC BARK. Bark ridges run ALONG the trunk, so the height field uses
//    a high lattice period across U and a low one along V. That is why the
//    tileable noise in Zenith_TerrainNoise takes per-axis periods.
//
// 2. CYLINDRICAL UVs AT WORLD SCALE, with a duplicated seam ring. A tube has an
//    exact unwrap; the box projection the rocks use would smear across a curved
//    flank. U is arc length, V is distance along the trunk, both in metres per
//    tile — which is why the maps have to tile (see the rock generator's note).
//
// 3. END GRAIN FOR FREE. The broken end caps map V RADIALLY instead of axially,
//    so the same anisotropic bark ridges come out as concentric rings, and the
//    vertex tint pales them toward heartwood. No second texture, no second
//    material, no second instance group.
//=============================================================================

#ifndef ZENITH_TOOLS

// Asset generation is a tools-build capability (the mesh Export APIs only exist
// there); non-tools builds get a no-op so GenerateTestAssets links.
void GenerateFallenTreeAssets()
{
}

#else

#include "Zenith_Tools_TestAssetExport.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "Zenith_Tools_TextureExport.h"   // the ONE .ztxtr writer
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_Noise.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{

using Zenith_TerrainNoise::TileableValueNoise;
using Zenith_TerrainNoise::TileableFBM;
using Zenith_TerrainNoise::TileableRidged;

float SmoothStep01(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

//=============================================================================
// Shape description. One row per piece; the tube builder is shared.
//=============================================================================
struct LogShapeParams
{
	u_int m_uAxialSegments   = 16u;
	u_int m_uRadialSegments  = 12u;
	float m_fLengthMetres    = 6.50f;
	float m_fButtRadius      = 0.34f;
	float m_fTipRadius       = 0.19f;
	float m_fBendMetres      = 0.45f;  // lateral sag of the trunk axis over its length
	float m_fBumpAmplitude   = 0.035f; // bark relief, as a fraction of the local radius
	u_int m_uBumpAxialPeriod = 8u;     // lattice cells ALONG the trunk (see SurfaceRadius)
	float m_fKnotStrength    = 0.16f;  // localized bulges where branches once were
	float m_fSplinterDepth   = 0.22f;  // how ragged the broken ends are, as a fraction of radius
	float m_fRootFlare       = 0.0f;   // stump only: extra radius at the very base
	u_int m_uBranchStubs     = 3u;
	float m_fStubLength      = 0.55f;
	float m_fUVTileMetres    = 0.85f;
};

//=============================================================================
// The trunk axis: a gentle bend so nothing reads as a extruded cylinder. Pure
// function of the parameter t in [0,1] so the caps and the stubs can query it.
//=============================================================================
Zenith_Maths::Vector3 LogAxisPoint(const LogShapeParams& xParams, float fT)
{
	// A quadratic sag in X plus a smaller one in Z: two different curvatures
	// read as an organic lean rather than an arc.
	const float fBendX = xParams.m_fBendMetres * (fT * fT);
	const float fBendZ = xParams.m_fBendMetres * 0.42f * (fT * (1.0f - fT) * 4.0f);
	return Zenith_Maths::Vector3(fBendX, xParams.m_fLengthMetres * fT, fBendZ);
}

float LogRadiusAt(const LogShapeParams& xParams, float fT)
{
	// Taper is not linear: a real trunk holds its girth then narrows toward the
	// crown break.
	const float fTaper = 1.0f - powf(fT, 1.45f);
	float fRadius = xParams.m_fTipRadius + (xParams.m_fButtRadius - xParams.m_fTipRadius) * fTaper;
	// Root flare: a fast swell over the bottom ~12% (stumps only).
	if (xParams.m_fRootFlare > 0.0f)
	{
		fRadius += xParams.m_fRootFlare * (1.0f - SmoothStep01(0.0f, 0.14f, fT));
	}
	return fRadius;
}

// A frame that follows the axis, so the rings stay perpendicular to a bent trunk.
void LogAxisFrame(const LogShapeParams& xParams, float fT,
	Zenith_Maths::Vector3& xTangentOut, Zenith_Maths::Vector3& xUOut, Zenith_Maths::Vector3& xVOut)
{
	const float fEps = 0.004f;
	const Zenith_Maths::Vector3 xA = LogAxisPoint(xParams, std::max(0.0f, fT - fEps));
	const Zenith_Maths::Vector3 xB = LogAxisPoint(xParams, std::min(1.0f, fT + fEps));
	xTangentOut = glm::normalize(xB - xA);
	const Zenith_Maths::Vector3 xRef = (fabsf(xTangentOut.z) < 0.95f)
		? Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xUOut = glm::normalize(glm::cross(xRef, xTangentOut));
	xVOut = glm::cross(xTangentOut, xUOut);
}

//=============================================================================
// Emission.
//
// Vertices are duplicated per triangle, as in the rock generator, so a facet's
// UV projection and its normal can differ from its neighbour's without a shared
// vertex having to average them. The cost is paid back by the cap trick below.
//=============================================================================
struct LogVertex
{
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xNormal;
	Zenith_Maths::Vector2 m_xUV;
	Zenith_Maths::Vector3 m_xTangent;
	float                 m_fTint = 1.0f;
};

void EmitLogTriangle(Zenith_MeshAsset& xMesh, const LogVertex& xA, const LogVertex& xB,
	const LogVertex& xC, const Zenith_Maths::Vector3& xOutwardHint, float fFaceNormalBlend = 0.0f)
{
	// Engine convention: cross(C-A, B-A) points OUTWARD for a front-facing
	// triangle. The hint is the surface's own outward direction (the ring normal,
	// or the cap's axis) rather than a body centre -- a bent, capped tube is not
	// star-shaped about any single point, so a centre test would flip the caps.
	LogVertex xV1 = xB;
	LogVertex xV2 = xC;
	Zenith_Maths::Vector3 xFace = glm::cross(xV2.m_xPosition - xA.m_xPosition,
		xV1.m_xPosition - xA.m_xPosition);
	if (glm::dot(xFace, xOutwardHint) < 0.0f)
	{
		LogVertex xSwap = xV1;
		xV1 = xV2;
		xV2 = xSwap;
		xFace = glm::cross(xV2.m_xPosition - xA.m_xPosition, xV1.m_xPosition - xA.m_xPosition);
	}
	if (glm::dot(xFace, xFace) < 1.0e-12f)
	{
		return;   // degenerate (a cap fan's centre sliver) -- nothing to draw
	}

	const u_int uBase = xMesh.GetNumVerts();
	const LogVertex* apxCorners[3] = { &xA, &xV1, &xV2 };
	for (u_int u = 0; u < 3u; u++)
	{
		const LogVertex& xVert = *apxCorners[u];
		Zenith_Maths::Vector3 xNormal = xVert.m_xNormal;
		xNormal = (glm::dot(xNormal, xNormal) > 1.0e-12f)
			? glm::normalize(xNormal)
			: glm::normalize(xFace);
		// A raked facet shaded with its ring's radial normal is lit as though it
		// were part of the smooth flank. The two broken-end segments are raked by
		// the splinter offsets far enough for that to be plainly wrong, so they
		// blend toward their own geometry; the body of the trunk stays smooth.
		if (fFaceNormalBlend > 0.0f)
		{
			const Zenith_Maths::Vector3 xFaceUnit = glm::normalize(xFace);
			xNormal = glm::normalize(xNormal * (1.0f - fFaceNormalBlend) + xFaceUnit * fFaceNormalBlend);
		}
		// Gram-Schmidt against the SHADING normal, so the shader's TBN is
		// orthonormal to what it actually shades with.
		Zenith_Maths::Vector3 xTangent = xVert.m_xTangent - xNormal * glm::dot(xNormal, xVert.m_xTangent);
		xTangent = (glm::dot(xTangent, xTangent) > 1.0e-8f)
			? glm::normalize(xTangent)
			: glm::normalize(glm::cross(xNormal, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		xMesh.AddVertex(xVert.m_xPosition, xNormal, xVert.m_xUV, xTangent,
			Zenith_Maths::Vector4(xVert.m_fTint, xVert.m_fTint, xVert.m_fTint, 1.0f));
	}
	xMesh.AddTriangle(uBase, uBase + 1u, uBase + 2u);
}

//=============================================================================
// One trunk (or stub, or stump) appended to a mesh.
//=============================================================================
void AppendLogGeometry(Zenith_MeshAsset& xMesh, const LogShapeParams& xParams, u_int uSeed,
	const Zenith_Maths::Vector3& xOffset, float fYawRadians, float fScale)
{
	const u_int uAxial = std::max(2u, xParams.m_uAxialSegments);
	const u_int uRadial = std::max(3u, xParams.m_uRadialSegments);
	const float fInvTile = 1.0f / std::max(0.05f, xParams.m_fUVTileMetres);
	const float fCosYaw = cosf(fYawRadians);
	const float fSinYaw = sinf(fYawRadians);

	// Place a generated point into the instance's own frame.
	auto Place = [&](const Zenith_Maths::Vector3& xLocal) -> Zenith_Maths::Vector3
	{
		const Zenith_Maths::Vector3 xP = xLocal * fScale;
		return Zenith_Maths::Vector3(xP.x * fCosYaw + xP.z * fSinYaw, xP.y,
			-xP.x * fSinYaw + xP.z * fCosYaw) + xOffset;
	};
	auto PlaceDir = [&](const Zenith_Maths::Vector3& xDir) -> Zenith_Maths::Vector3
	{
		return Zenith_Maths::Vector3(xDir.x * fCosYaw + xDir.z * fSinYaw, xDir.y,
			-xDir.x * fSinYaw + xDir.z * fCosYaw);
	};

	// Per-ring axial position, ragged at BOTH ends: a fallen trunk is broken, not
	// sawn. The raggedness is a per-radial-column offset so the break reads as
	// splintered rather than bevelled.
	auto SplinterOffset = [&](u_int uColumn, bool bTipEnd) -> float
	{
		const float fNoise = Zenith_TerrainNoise::HashToFloat01(Zenith_TerrainNoise::HashCoords(
			static_cast<int>(uColumn), bTipEnd ? 1 : 0, uSeed + 7717u));
		return (fNoise - 0.5f) * 2.0f * xParams.m_fSplinterDepth;
	};

	// Surface radius at (t, angle): taper + bark relief + knots.
	auto SurfaceRadius = [&](float fT, u_int uColumn) -> float
	{
		const float fBase = LogRadiusAt(xParams, fT);
		const float fAngleT = static_cast<float>(uColumn) / static_cast<float>(uRadial);
		// Wrapped in the ANGULAR axis so the seam column matches column 0 exactly;
		// the axial period only has to be large enough not to visibly repeat.
		// ★ It must also be > 1: a wrapped lattice of period 1 has a single cell,
		// so both interpolants are the same sample and the noise is CONSTANT.
		const float fBump = TileableValueNoise(fAngleT, fT,
			static_cast<int>(uRadial), static_cast<int>(std::max(2u, xParams.m_uBumpAxialPeriod)),
			uSeed + 331u) - 0.5f;
		float fRadius = fBase * (1.0f + fBump * 2.0f * xParams.m_fBumpAmplitude);
		if (xParams.m_fKnotStrength > 0.0f)
		{
			const float fKnot = TileableValueNoise(fAngleT, fT,
				static_cast<int>(uRadial), 3, uSeed + 913u);
			fRadius += fBase * xParams.m_fKnotStrength * SmoothStep01(0.78f, 0.97f, fKnot);
		}
		return fRadius;
	};

	// --- The tube ------------------------------------------------------------
	for (u_int uSeg = 0; uSeg < uAxial; uSeg++)
	{
		const float fT0 = static_cast<float>(uSeg) / static_cast<float>(uAxial);
		const float fT1 = static_cast<float>(uSeg + 1u) / static_cast<float>(uAxial);

		Zenith_Maths::Vector3 xTan0, xU0, xV0;
		Zenith_Maths::Vector3 xTan1, xU1, xV1;
		LogAxisFrame(xParams, fT0, xTan0, xU0, xV0);
		LogAxisFrame(xParams, fT1, xTan1, xU1, xV1);
		const Zenith_Maths::Vector3 xC0 = LogAxisPoint(xParams, fT0);
		const Zenith_Maths::Vector3 xC1 = LogAxisPoint(xParams, fT1);

		for (u_int uCol = 0; uCol < uRadial; uCol++)
		{
			const u_int uColNext = uCol + 1u;
			const float fA0 = 6.2831853f * static_cast<float>(uCol) / static_cast<float>(uRadial);
			const float fA1 = 6.2831853f * static_cast<float>(uColNext) / static_cast<float>(uRadial);

			// Column 0 and column uRadial are the SAME ring position but carry
			// different U, which is the seam a cylindrical unwrap needs.
			const float fR00 = SurfaceRadius(fT0, uCol % uRadial);
			const float fR01 = SurfaceRadius(fT0, uColNext % uRadial);
			const float fR10 = SurfaceRadius(fT1, uCol % uRadial);
			const float fR11 = SurfaceRadius(fT1, uColNext % uRadial);

			const Zenith_Maths::Vector3 xN00 = xU0 * cosf(fA0) + xV0 * sinf(fA0);
			const Zenith_Maths::Vector3 xN01 = xU0 * cosf(fA1) + xV0 * sinf(fA1);
			const Zenith_Maths::Vector3 xN10 = xU1 * cosf(fA0) + xV1 * sinf(fA0);
			const Zenith_Maths::Vector3 xN11 = xU1 * cosf(fA1) + xV1 * sinf(fA1);

			// ★ THE BROKEN ENDS ARE OFFSET PER COLUMN, NOT PER QUAD. One offset for
			// the whole quad is what the first version did, and it disagreed with
			// the CAP -- which has always been per column -- so every broken end
			// carried a crack between its last ring and its own rim. It also left
			// the UVs describing the un-splintered length, which stretched those
			// quads by up to ten to one.
			const bool bFirstSeg = (uSeg == 0u);
			const bool bLastSeg = (uSeg == uAxial - 1u);
			const float fEndA = bFirstSeg
				? SplinterOffset(uCol % uRadial, false) * LogRadiusAt(xParams, 0.0f) : 0.0f;
			const float fEndB = bFirstSeg
				? SplinterOffset(uColNext % uRadial, false) * LogRadiusAt(xParams, 0.0f) : 0.0f;
			const float fEndC = bLastSeg
				? -SplinterOffset(uCol % uRadial, true) * LogRadiusAt(xParams, 1.0f) : 0.0f;
			const float fEndD = bLastSeg
				? -SplinterOffset(uColNext % uRadial, true) * LogRadiusAt(xParams, 1.0f) : 0.0f;

			const float fArc0 = 6.2831853f * LogRadiusAt(xParams, fT0)
				* static_cast<float>(uCol) / static_cast<float>(uRadial);
			const float fArc1 = 6.2831853f * LogRadiusAt(xParams, fT0)
				* static_cast<float>(uColNext) / static_cast<float>(uRadial);
			// V follows the ACTUAL axial position, splinter included, or the raked
			// end quads get a UV span describing a length they do not have.
			const float fAlongBase0 = xParams.m_fLengthMetres * fT0;
			const float fAlongBase1 = xParams.m_fLengthMetres * fT1;

			LogVertex axQuad[4];
			axQuad[0].m_xPosition = Place(xC0 + xN00 * fR00 + xTan0 * fEndA);
			axQuad[0].m_xNormal   = PlaceDir(xN00);
			axQuad[0].m_xUV       = Zenith_Maths::Vector2(fArc0 * fInvTile, (fAlongBase0 + fEndA) * fInvTile);
			axQuad[0].m_xTangent  = PlaceDir(xTan0);

			axQuad[1].m_xPosition = Place(xC0 + xN01 * fR01 + xTan0 * fEndB);
			axQuad[1].m_xNormal   = PlaceDir(xN01);
			axQuad[1].m_xUV       = Zenith_Maths::Vector2(fArc1 * fInvTile, (fAlongBase0 + fEndB) * fInvTile);
			axQuad[1].m_xTangent  = PlaceDir(xTan0);

			axQuad[2].m_xPosition = Place(xC1 + xN10 * fR10 + xTan1 * fEndC);
			axQuad[2].m_xNormal   = PlaceDir(xN10);
			axQuad[2].m_xUV       = Zenith_Maths::Vector2(fArc0 * fInvTile, (fAlongBase1 + fEndC) * fInvTile);
			axQuad[2].m_xTangent  = PlaceDir(xTan1);

			axQuad[3].m_xPosition = Place(xC1 + xN11 * fR11 + xTan1 * fEndD);
			axQuad[3].m_xNormal   = PlaceDir(xN11);
			axQuad[3].m_xUV       = Zenith_Maths::Vector2(fArc1 * fInvTile, (fAlongBase1 + fEndD) * fInvTile);
			axQuad[3].m_xTangent  = PlaceDir(xTan1);

			// Bark darkens into the furrows; the very base picks up ground dirt.
			const float fEndBlend = (bFirstSeg || bLastSeg) ? 0.60f : 0.0f;
			for (u_int u = 0; u < 4u; u++)
			{
				const float fTHere = (u < 2u) ? fT0 : fT1;
				const float fRHere = (u == 0u) ? fR00 : (u == 1u) ? fR01 : (u == 2u) ? fR10 : fR11;
				const float fFurrow = std::clamp(
					(fRHere / std::max(0.001f, LogRadiusAt(xParams, fTHere)) - 0.94f) / 0.12f, 0.0f, 1.0f);
				axQuad[u].m_fTint = (0.82f + 0.18f * fFurrow) * (0.88f + 0.12f * SmoothStep01(0.0f, 0.10f, fTHere));
			}

			const Zenith_Maths::Vector3 xHint = PlaceDir(xN00 + xN11);
			EmitLogTriangle(xMesh, axQuad[0], axQuad[1], axQuad[2], xHint, fEndBlend);
			EmitLogTriangle(xMesh, axQuad[1], axQuad[3], axQuad[2], xHint, fEndBlend);
		}
	}

	// --- The two broken end caps --------------------------------------------
	// V maps RADIALLY here, so the bark's along-trunk ridges come out concentric:
	// end grain, from the same texture, for the price of a different UV.
	for (int iEnd = 0; iEnd < 2; iEnd++)
	{
		const bool bTip = (iEnd == 1);
		const float fT = bTip ? 1.0f : 0.0f;
		Zenith_Maths::Vector3 xTan, xU, xV;
		LogAxisFrame(xParams, fT, xTan, xU, xV);
		const Zenith_Maths::Vector3 xCentre = LogAxisPoint(xParams, fT);
		const Zenith_Maths::Vector3 xOutward = bTip ? xTan : -xTan;
		const float fBaseRadius = LogRadiusAt(xParams, fT);

		const Zenith_Maths::Vector3 xHubPos =
			Place(xCentre + xOutward * (fBaseRadius * xParams.m_fSplinterDepth * 0.35f));

		for (u_int uCol = 0; uCol < uRadial; uCol++)
		{
			const u_int uColNext = (uCol + 1u) % uRadial;
			const float fA0 = 6.2831853f * static_cast<float>(uCol) / static_cast<float>(uRadial);
			const float fA1 = 6.2831853f * static_cast<float>(uCol + 1u) / static_cast<float>(uRadial);
			const float fR0 = SurfaceRadius(fT, uCol);
			const float fR1 = SurfaceRadius(fT, uColNext);
			const float fEnd0 = SplinterOffset(uCol, bTip) * fBaseRadius * (bTip ? -1.0f : 1.0f);
			const float fEnd1 = SplinterOffset(uColNext, bTip) * fBaseRadius * (bTip ? -1.0f : 1.0f);

			LogVertex xHub;
			LogVertex axRim[2];
			xHub.m_xPosition = xHubPos;
			axRim[0].m_xPosition = Place(xCentre + (xU * cosf(fA0) + xV * sinf(fA0)) * fR0 + xTan * fEnd0);
			axRim[1].m_xPosition = Place(xCentre + (xU * cosf(fA1) + xV * sinf(fA1)) * fR1 + xTan * fEnd1);

			// ★ THE WEDGE GETS ITS OWN GEOMETRIC NORMAL, not the end's axis. The
			// splinter offsets tilt each wedge steeply -- on the stump, whose
			// m_fSplinterDepth is 0.40, far enough that an axis normal sits ~80
			// degrees off the facet it is supposed to shade. A fresh break IS flat
			// shaded, so the face normal is also the right look.
			Zenith_Maths::Vector3 xWedgeNormal = glm::cross(
				axRim[1].m_xPosition - xHub.m_xPosition,
				axRim[0].m_xPosition - xHub.m_xPosition);
			const Zenith_Maths::Vector3 xAxisWorld = PlaceDir(xOutward);
			if (glm::dot(xWedgeNormal, xAxisWorld) < 0.0f)
			{
				xWedgeNormal = -xWedgeNormal;
			}
			xWedgeNormal = (glm::dot(xWedgeNormal, xWedgeNormal) > 1.0e-12f)
				? glm::normalize(xWedgeNormal) : xAxisWorld;

			// ★ AND THE HUB TAKES THIS WEDGE'S OWN U. A fan hub cannot carry one U
			// for every wedge: with U = arc length, the wedge nearest the wrap sees
			// its rim at U ~ 2*pi*r while a shared hub sits at 0, which stretches
			// that one triangle's UVs by the whole circumference. Per-wedge, each
			// fan triangle is a small well-shaped UV triangle and the radial
			// mapping is isometric -- arc against arc, radius against radius.
			// ★ ARC ALONG THE SMOOTH PROFILE RADIUS, NOT THE PER-COLUMN SURFACE ONE.
			// U = angle * SurfaceRadius(column) is not monotonic in the column: the
			// bark bumps and knots vary the radius per column, so a rising angle
			// times a falling radius can hold U almost constant -- adjacent rim
			// vertices came out 0.014 apart in U where they should have been 0.15,
			// collapsing the wedge to a UV sliver and smearing the end grain about
			// tenfold. fBaseRadius is the smooth profile at this end, which is what
			// the TUBE has always used for its own arc.
			const float fHubU = (fA0 + fA1) * 0.5f * fBaseRadius * fInvTile;
			xHub.m_xNormal   = xWedgeNormal;
			xHub.m_xUV       = Zenith_Maths::Vector2(fHubU, 0.0f);
			xHub.m_xTangent  = PlaceDir(xU);
			xHub.m_fTint     = 1.0f;   // heartwood is the palest thing on the piece

			for (u_int u = 0; u < 2u; u++)
			{
				const float fAHere = (u == 0u) ? fA0 : fA1;
				axRim[u].m_xNormal  = xWedgeNormal;
				// U = arc, V = the wedge's ACTUAL 3D reach from the hub. Concentric
				// rings out of an along-trunk ridge either way, but measuring the
				// flat radius instead understates a splintered wedge's length by up
				// to ten to one -- the stump's 0.40 splinter depth splays its rim
				// far out of the end plane -- and smears the end grain across it.
				const float fReach = glm::length(axRim[u].m_xPosition - xHub.m_xPosition);
				axRim[u].m_xUV      = Zenith_Maths::Vector2(
					fAHere * fBaseRadius * fInvTile, fReach * fInvTile);
				axRim[u].m_xTangent = PlaceDir(xU);
				// Pale in the middle, bark-dark at the rim.
				axRim[u].m_fTint    = 0.80f;
			}
			EmitLogTriangle(xMesh, xHub, axRim[0], axRim[1], xWedgeNormal);
		}
	}

	// --- Branch stubs --------------------------------------------------------
	// Short tapered cones, angled up-and-out from the trunk. They are what stops
	// the silhouette reading as a pipe.
	for (u_int uStub = 0; uStub < xParams.m_uBranchStubs; uStub++)
	{
		const float fStubT = 0.22f + 0.56f * (static_cast<float>(uStub)
			/ std::max(1.0f, static_cast<float>(xParams.m_uBranchStubs)));
		const float fAngle = static_cast<float>(uStub) * 2.399963f
			+ Zenith_TerrainNoise::HashToFloat01(Zenith_TerrainNoise::HashCoords(
				static_cast<int>(uStub), 3, uSeed + 4051u)) * 1.2f;

		Zenith_Maths::Vector3 xTan, xU, xV;
		LogAxisFrame(xParams, fStubT, xTan, xU, xV);
		const Zenith_Maths::Vector3 xRoot = LogAxisPoint(xParams, fStubT);
		const Zenith_Maths::Vector3 xOut = xU * cosf(fAngle) + xV * sinf(fAngle);
		const Zenith_Maths::Vector3 xDir = glm::normalize(xOut + xTan * 0.55f);
		const float fTrunkRadius = LogRadiusAt(xParams, fStubT);
		const float fStubRadius = fTrunkRadius * 0.30f;
		const float fStubLen = xParams.m_fStubLength * (0.7f + 0.6f *
			Zenith_TerrainNoise::HashToFloat01(Zenith_TerrainNoise::HashCoords(
				static_cast<int>(uStub), 5, uSeed + 6113u)));

		// Start inside the trunk so the join has no gap.
		const Zenith_Maths::Vector3 xStart = xRoot + xDir * (fTrunkRadius * 0.55f);
		const Zenith_Maths::Vector3 xEnd = xRoot + xDir * (fTrunkRadius + fStubLen);
		Zenith_Maths::Vector3 xSU, xSV;
		{
			const Zenith_Maths::Vector3 xRef = (fabsf(xDir.y) < 0.95f)
				? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
				: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			xSU = glm::normalize(glm::cross(xRef, xDir));
			xSV = glm::cross(xDir, xSU);
		}

		constexpr u_int uSTUB_RADIAL = 6u;
		for (u_int uCol = 0; uCol < uSTUB_RADIAL; uCol++)
		{
			const float fA0 = 6.2831853f * static_cast<float>(uCol) / static_cast<float>(uSTUB_RADIAL);
			const float fA1 = 6.2831853f * static_cast<float>(uCol + 1u) / static_cast<float>(uSTUB_RADIAL);
			const Zenith_Maths::Vector3 xNA = xSU * cosf(fA0) + xSV * sinf(fA0);
			const Zenith_Maths::Vector3 xNB = xSU * cosf(fA1) + xSV * sinf(fA1);

			LogVertex axQuad[4];
			axQuad[0].m_xPosition = Place(xStart + xNA * fStubRadius);
			axQuad[1].m_xPosition = Place(xStart + xNB * fStubRadius);
			axQuad[2].m_xPosition = Place(xEnd + xNA * (fStubRadius * 0.35f));
			axQuad[3].m_xPosition = Place(xEnd + xNB * (fStubRadius * 0.35f));
			const Zenith_Maths::Vector3 axNormals[4] = { xNA, xNB, xNA, xNB };
			const float afArc[4] = { fA0 * fStubRadius, fA1 * fStubRadius,
				fA0 * fStubRadius, fA1 * fStubRadius };
			const float afAlong[4] = { 0.0f, 0.0f, fStubLen, fStubLen };
			for (u_int u = 0; u < 4u; u++)
			{
				axQuad[u].m_xNormal  = PlaceDir(axNormals[u]);
				axQuad[u].m_xUV      = Zenith_Maths::Vector2(afArc[u] * fInvTile, afAlong[u] * fInvTile);
				axQuad[u].m_xTangent = PlaceDir(xDir);
				axQuad[u].m_fTint    = 0.86f;
			}
			const Zenith_Maths::Vector3 xHint = PlaceDir(xNA + xNB);
			EmitLogTriangle(xMesh, axQuad[0], axQuad[1], axQuad[2], xHint);
			EmitLogTriangle(xMesh, axQuad[1], axQuad[3], axQuad[2], xHint);
		}
	}
}

//=============================================================================
// Textures. Same four maps and the same RM layout as the rock set (G roughness,
// B metallic), so a material wires up identically.
//=============================================================================
struct BarkTextureParams
{
	const char*           m_szName         = "Bark";
	Zenith_Maths::Vector3 m_xFurrowColour  = Zenith_Maths::Vector3(0.085f, 0.060f, 0.040f);
	Zenith_Maths::Vector3 m_xRidgeColour   = Zenith_Maths::Vector3(0.420f, 0.318f, 0.212f);
	Zenith_Maths::Vector3 m_xMossColour    = Zenith_Maths::Vector3(0.120f, 0.230f, 0.075f);
	float                 m_fMossAmount    = 0.0f;
	float                 m_fRoughRidge    = 0.72f;
	float                 m_fRoughFurrow   = 0.97f;
	u_int                 m_uSeed          = 3301u;
};

// The one height field every other map derives from. ANISOTROPIC: a high lattice
// period across U (around the trunk) against a low one along V (up it), which is
// what makes ridges rather than blobs -- and what the end caps reuse as rings.
float BarkSurfaceHeight(const BarkTextureParams& xParams, float fU, float fV, float& fFurrowOut)
{
	const float fRidges = TileableFBM(fU, fV, 18, 3, 3u, xParams.m_uSeed);
	const float fCoarse = TileableFBM(fU, fV, 6, 2, 2u, xParams.m_uSeed + 271u);
	const float fGrain  = TileableValueNoise(fU, fV, 96, 24, xParams.m_uSeed + 617u);
	float fHeight = fRidges * 0.55f + fCoarse * 0.31f + fGrain * 0.14f;

	// Deep furrows between the plates: a ridged field thresholded, stretched the
	// same way, so the cuts run with the grain instead of across it.
	fFurrowOut = SmoothStep01(0.62f, 0.86f, TileableRidged(fU, fV, 9, 2, 3u, xParams.m_uSeed + 1451u));
	fHeight -= fFurrowOut * 0.46f;
	return std::clamp(fHeight, 0.0f, 1.0f);
}

void GenerateBarkTextureSet(const std::string& strDir, const BarkTextureParams& xParams)
{
	constexpr int32_t iCOLOUR_SIZE = 1024;
	constexpr int32_t iDATA_SIZE = 512;

	Zenith_Vector<float> xHeight;
	Zenith_Vector<float> xFurrow;
	xHeight.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE, 0.0f);
	xFurrow.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE, 0.0f);
	for (int32_t iY = 0; iY < iCOLOUR_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iCOLOUR_SIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iCOLOUR_SIZE;
			const float fV = static_cast<float>(iY) / iCOLOUR_SIZE;
			float fFurrow = 0.0f;
			xHeight.Get(iY * iCOLOUR_SIZE + iX) = BarkSurfaceHeight(xParams, fU, fV, fFurrow);
			xFurrow.Get(iY * iCOLOUR_SIZE + iX) = fFurrow;
		}
	}

	// --- Albedo (sRGB) -------------------------------------------------------
	Zenith_Vector<u_int8> xAlbedo;
	xAlbedo.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iCOLOUR_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iCOLOUR_SIZE; iX++)
		{
			const int32_t iIdx = iY * iCOLOUR_SIZE + iX;
			const float fU = static_cast<float>(iX) / iCOLOUR_SIZE;
			const float fV = static_cast<float>(iY) / iCOLOUR_SIZE;
			const float fH = xHeight.Get(iIdx);

			Zenith_Maths::Vector3 xColour =
				xParams.m_xFurrowColour + (xParams.m_xRidgeColour - xParams.m_xFurrowColour) * fH;

			// Fine fibre speckle -- bark is not smooth even on a plate.
			const float fFibre = Zenith_TerrainNoise::HashToFloat01(
				Zenith_TerrainNoise::HashCoords(iX, iY, xParams.m_uSeed + 83u));
			xColour = xColour * (0.92f + 0.16f * fFibre);

			// Moss: patches that settle INTO the furrows, so the mask is biased by
			// the height field rather than being an independent blotch layer.
			if (xParams.m_fMossAmount > 0.0f)
			{
				const float fPatch = SmoothStep01(0.48f, 0.76f,
					TileableFBM(fU, fV, 4, 3, 4u, xParams.m_uSeed + 199u));
				const float fMoss = fPatch * xParams.m_fMossAmount * (1.0f - 0.55f * fH);
				xColour = xColour + (xParams.m_xMossColour - xColour) * fMoss;
			}

			xColour = xColour * (1.0f - 0.26f * xFurrow.Get(iIdx));

			u_int8* pucA = &xAlbedo.Get(iIdx * 4);
			pucA[0] = static_cast<u_int8>(std::clamp(xColour.x, 0.0f, 1.0f) * 255.0f);
			pucA[1] = static_cast<u_int8>(std::clamp(xColour.y, 0.0f, 1.0f) * 255.0f);
			pucA[2] = static_cast<u_int8>(std::clamp(xColour.z, 0.0f, 1.0f) * 255.0f);
			pucA[3] = 255;
		}
	}
	Zenith_Tools_TextureExport::ExportFromDataWithFormat(
		xAlbedo.GetDataPointer(), strDir + "FallenTree_" + xParams.m_szName + "_Albedo" ZENITH_TEXTURE_EXT, iCOLOUR_SIZE, iCOLOUR_SIZE, TEXTURE_FORMAT_RGBA8_SRGB,
		xAlbedo.GetSize() / (static_cast<size_t>(iCOLOUR_SIZE) * iCOLOUR_SIZE));

	// --- Normal (linear), wrapped central differences -------------------------
	Zenith_Vector<u_int8> xNormal;
	xNormal.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iCOLOUR_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iCOLOUR_SIZE; iX++)
		{
			const int32_t iXP = (iX + 1) % iCOLOUR_SIZE;
			const int32_t iXM = (iX + iCOLOUR_SIZE - 1) % iCOLOUR_SIZE;
			const int32_t iYP = (iY + 1) % iCOLOUR_SIZE;
			const int32_t iYM = (iY + iCOLOUR_SIZE - 1) % iCOLOUR_SIZE;
			// Bark relief is deep across the grain and shallow along it, so the two
			// derivatives are weighted differently on purpose.
			const float fDX = (xHeight.Get(iY * iCOLOUR_SIZE + iXP) - xHeight.Get(iY * iCOLOUR_SIZE + iXM)) * 5.0f;
			const float fDY = (xHeight.Get(iYP * iCOLOUR_SIZE + iX) - xHeight.Get(iYM * iCOLOUR_SIZE + iX)) * 2.5f;
			const Zenith_Maths::Vector3 xN =
				glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			u_int8* pucN = &xNormal.Get((iY * iCOLOUR_SIZE + iX) * 4);
			pucN[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
			pucN[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
			pucN[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
			pucN[3] = 255;
		}
	}
	Zenith_Tools_TextureExport::ExportFromDataWithFormat(
		xNormal.GetDataPointer(), strDir + "FallenTree_" + xParams.m_szName + "_Normal" ZENITH_TEXTURE_EXT, iCOLOUR_SIZE, iCOLOUR_SIZE, TEXTURE_FORMAT_RGBA8_UNORM,
		xNormal.GetSize() / (static_cast<size_t>(iCOLOUR_SIZE) * iCOLOUR_SIZE));

	// --- RM + AO --------------------------------------------------------------
	Zenith_Vector<u_int8> xRM;
	Zenith_Vector<u_int8> xAO;
	xRM.Resize(iDATA_SIZE * iDATA_SIZE * 4, 0);
	xAO.Resize(iDATA_SIZE * iDATA_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iDATA_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iDATA_SIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iDATA_SIZE;
			const float fV = static_cast<float>(iY) / iDATA_SIZE;
			float fFurrow = 0.0f;
			const float fH = BarkSurfaceHeight(xParams, fU, fV, fFurrow);

			const float fRoughness = std::clamp(
				xParams.m_fRoughFurrow + (xParams.m_fRoughRidge - xParams.m_fRoughFurrow) * fH,
				0.05f, 1.0f);
			u_int8* pucRM = &xRM.Get((iY * iDATA_SIZE + iX) * 4);
			pucRM[0] = 0;
			pucRM[1] = static_cast<u_int8>(fRoughness * 255.0f);
			pucRM[2] = 0;   // wood is a dielectric
			pucRM[3] = 255;

			const float fAO = std::clamp((0.76f + 0.24f * fH) * (1.0f - 0.26f * fFurrow), 0.0f, 1.0f);
			const u_int8 ucAO = static_cast<u_int8>(fAO * 255.0f);
			u_int8* pucAO = &xAO.Get((iY * iDATA_SIZE + iX) * 4);
			pucAO[0] = ucAO;
			pucAO[1] = ucAO;
			pucAO[2] = ucAO;
			pucAO[3] = 255;
		}
	}
	Zenith_Tools_TextureExport::ExportFromDataWithFormat(
		xRM.GetDataPointer(), strDir + "FallenTree_" + xParams.m_szName + "_RM" ZENITH_TEXTURE_EXT, iDATA_SIZE, iDATA_SIZE, TEXTURE_FORMAT_RGBA8_UNORM,
		xRM.GetSize() / (static_cast<size_t>(iDATA_SIZE) * iDATA_SIZE));
	Zenith_Tools_TextureExport::ExportFromDataWithFormat(
		xAO.GetDataPointer(), strDir + "FallenTree_" + xParams.m_szName + "_AO" ZENITH_TEXTURE_EXT, iDATA_SIZE, iDATA_SIZE, TEXTURE_FORMAT_RGBA8_UNORM,
		xAO.GetSize() / (static_cast<size_t>(iDATA_SIZE) * iDATA_SIZE));
}

void GenerateBarkMaterial(const std::string& strDir, const char* szName)
{
	const std::string strStem = std::string("engine:Meshes/FallenTrees/FallenTree_") + szName;
	auto xhMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxMaterial = xhMaterial.GetDirect();
	pxMaterial->SetName(std::string("FallenTree") + szName);
	pxMaterial->SetDiffuseTexture(TextureHandle(strStem + "_Albedo" ZENITH_TEXTURE_EXT));
	pxMaterial->SetNormalTexture(TextureHandle(strStem + "_Normal" ZENITH_TEXTURE_EXT));
	pxMaterial->SetRoughnessMetallicTexture(TextureHandle(strStem + "_RM" ZENITH_TEXTURE_EXT));
	pxMaterial->SetOcclusionTexture(TextureHandle(strStem + "_AO" ZENITH_TEXTURE_EXT));
	// Multipliers on the sampled channels, not absolute values (an RM map is bound).
	pxMaterial->SetRoughness(1.0f);
	pxMaterial->SetMetallic(0.0f);
	pxMaterial->SetOcclusionStrength(1.0f);
	pxMaterial->SetNormalStrength(1.0f);
	pxMaterial->SetBlendMode(MATERIAL_BLEND_OPAQUE);
	pxMaterial->SaveToFile(strDir + "FallenTree_" + szName + ZENITH_MATERIAL_EXT);
}

// Drop the whole piece so its lowest vertex sits exactly on y=0. The scatter
// relies on this: it places an instance AT the sampled terrain height with no
// per-mesh offset table, exactly as the rock set does. It cannot be done inside
// AppendLogGeometry, because a broken end's splinter offsets are what push
// geometry below the axis origin, and a cluster only knows its own floor once
// every piece has been appended.
void SettleMeshOntoGround(Zenith_MeshAsset& xMesh)
{
	if (xMesh.GetNumVerts() == 0u)
	{
		return;
	}
	float fMinY = 1.0e30f;
	for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
	{
		fMinY = std::min(fMinY, xMesh.m_xPositions.Get(u).y);
	}
	for (u_int u = 0; u < xMesh.GetNumVerts(); u++)
	{
		xMesh.m_xPositions.Get(u).y -= fMinY;
	}
}

//=============================================================================
// The variant table. ONE definition, read by the exporter AND the unit tests --
// same reasoning as the rock set's RockVariantAt.
//=============================================================================
enum FallenTreeVariant
{
	FALLEN_TREE_LOG = 0,
	FALLEN_TREE_LOG_MOSSY,
	FALLEN_TREE_STUMP,
	FALLEN_TREE_BRANCHES,
	FALLEN_TREE_VARIANT_COUNT
};

struct FallenTreeVariantSpec
{
	const char*    m_szName = "FallenTree_Log";
	const char*    m_szMaterial = "Bark";
	u_int          m_uSeed = 0u;
	u_int          m_uClusterPieces = 1u;
	LogShapeParams m_xShape;
};

FallenTreeVariantSpec FallenTreeVariantAt(int iVariant)
{
	FallenTreeVariantSpec xSpec;
	switch (iVariant)
	{
	case FALLEN_TREE_LOG:
		// The hero piece: a long trunk, snapped at both ends.
		xSpec.m_szName = "FallenTree_Log";
		xSpec.m_szMaterial = "Bark";
		xSpec.m_uSeed = 51043u;
		xSpec.m_xShape.m_uAxialSegments = 18u;
		xSpec.m_xShape.m_uRadialSegments = 12u;
		xSpec.m_xShape.m_fLengthMetres = 6.50f;
		xSpec.m_xShape.m_fButtRadius = 0.34f;
		xSpec.m_xShape.m_fTipRadius = 0.19f;
		xSpec.m_xShape.m_fBendMetres = 0.45f;
		xSpec.m_xShape.m_uBranchStubs = 3u;
		xSpec.m_xShape.m_fUVTileMetres = 0.85f;
		break;

	case FALLEN_TREE_LOG_MOSSY:
		// Shorter, thicker, further gone. Heavier bend and a bigger knot budget.
		xSpec.m_szName = "FallenTree_LogMossy";
		xSpec.m_szMaterial = "MossyBark";
		xSpec.m_uSeed = 27701u;
		xSpec.m_xShape.m_uAxialSegments = 16u;
		xSpec.m_xShape.m_uRadialSegments = 12u;
		xSpec.m_xShape.m_fLengthMetres = 4.60f;
		xSpec.m_xShape.m_fButtRadius = 0.42f;
		xSpec.m_xShape.m_fTipRadius = 0.27f;
		xSpec.m_xShape.m_fBendMetres = 0.62f;
		xSpec.m_xShape.m_fKnotStrength = 0.22f;
		xSpec.m_xShape.m_fSplinterDepth = 0.28f;
		xSpec.m_xShape.m_uBranchStubs = 2u;
		xSpec.m_xShape.m_fUVTileMetres = 0.95f;
		break;

	case FALLEN_TREE_STUMP:
		// NOT tipped over by the scatter -- a stump is a fallen tree's remainder,
		// and it keeps the same author-standing convention as everything here.
		xSpec.m_szName = "FallenTree_Stump";
		xSpec.m_szMaterial = "Bark";
		xSpec.m_uSeed = 88117u;
		xSpec.m_xShape.m_uAxialSegments = 10u;
		xSpec.m_xShape.m_uRadialSegments = 14u;
		xSpec.m_xShape.m_fLengthMetres = 1.25f;
		xSpec.m_xShape.m_fButtRadius = 0.40f;
		xSpec.m_xShape.m_fTipRadius = 0.33f;
		xSpec.m_xShape.m_fBendMetres = 0.06f;
		xSpec.m_xShape.m_fRootFlare = 0.22f;
		xSpec.m_xShape.m_fSplinterDepth = 0.40f;   // a snapped top, not a sawn one
		xSpec.m_xShape.m_uBranchStubs = 0u;
		xSpec.m_xShape.m_fUVTileMetres = 0.75f;
		break;

	default:
		// Loose branches: three thin pieces in ONE mesh, so a scatter places a
		// natural tangle for the cost of a single instance.
		xSpec.m_szName = "FallenTree_Branches";
		xSpec.m_szMaterial = "Bark";
		xSpec.m_uSeed = 63419u;
		xSpec.m_uClusterPieces = 3u;
		xSpec.m_xShape.m_uAxialSegments = 8u;
		xSpec.m_xShape.m_uRadialSegments = 7u;
		xSpec.m_xShape.m_fLengthMetres = 1.70f;
		xSpec.m_xShape.m_fButtRadius = 0.085f;
		xSpec.m_xShape.m_fTipRadius = 0.040f;
		xSpec.m_xShape.m_fBendMetres = 0.30f;
		xSpec.m_xShape.m_fKnotStrength = 0.0f;
		xSpec.m_xShape.m_fSplinterDepth = 0.35f;
		xSpec.m_xShape.m_uBranchStubs = 1u;
		xSpec.m_xShape.m_fStubLength = 0.28f;
		xSpec.m_xShape.m_fUVTileMetres = 0.40f;
		break;
	}
	return xSpec;
}

// One variant's whole mesh: a single piece, or a small tangle of them. The
// cluster pieces are laid down HERE (they are debris, and never get a collider),
// which is why they may be rotated off the +Y convention the single pieces keep.
void BuildFallenTreeVariantMesh(Zenith_MeshAsset& xMesh, const FallenTreeVariantSpec& xSpec)
{
	if (xSpec.m_uClusterPieces <= 1u)
	{
		AppendLogGeometry(xMesh, xSpec.m_xShape, xSpec.m_uSeed,
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);
		SettleMeshOntoGround(xMesh);
		return;
	}

	Zenith_TerrainNoise::XorShift32 xRng(xSpec.m_uSeed);
	for (u_int uPiece = 0; uPiece < xSpec.m_uClusterPieces; uPiece++)
	{
		LogShapeParams xShape = xSpec.m_xShape;
		xShape.m_fLengthMetres *= 0.70f + 0.55f * xRng.NextFloat01();
		const float fAngle = static_cast<float>(uPiece) * 2.399963f;
		const float fRadius = 0.10f + 0.22f * xRng.NextFloat01();
		const Zenith_Maths::Vector3 xOffset(
			cosf(fAngle) * fRadius,
			0.03f + 0.06f * static_cast<float>(uPiece),   // stacked, not co-planar
			sinf(fAngle) * fRadius);
		AppendLogGeometry(xMesh, xShape, xSpec.m_uSeed + uPiece * 271u, xOffset,
			xRng.NextFloat01() * 6.2831853f, 1.0f);
	}
	SettleMeshOntoGround(xMesh);
}

//=============================================================================
// Export: .zasset (instanced-mesh component), .zgeom (static geometry), .zmodel.
//=============================================================================
void ExportFallenTree(const std::string& strDir, const char* szName, Zenith_MeshAsset* pxMesh,
	const char* szMaterialName)
{
	pxMesh->AddSubmesh(0u, pxMesh->GetNumIndices(), 0u);
	pxMesh->ComputeBounds();

	const std::string strAssetPath = strDir + szName + ZENITH_MESH_ASSET_EXT;
	pxMesh->Export(strAssetPath.c_str());

	Flux_MeshGeometry* pxGeometry = Zenith_Tools_CreateStaticFluxMeshGeometry(pxMesh);
	pxGeometry->Export((strDir + szName + ZENITH_GEOMETRY_EXT).c_str());
	delete pxGeometry;

	auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
	Zenith_ModelAsset* pxModel = xhModel.GetDirect();
	pxModel->SetName(szName);
	Zenith_Vector<std::string> xMaterials;
	xMaterials.PushBack(std::string("engine:Meshes/FallenTrees/FallenTree_")
		+ szMaterialName + ZENITH_MATERIAL_EXT);
	pxModel->AddMeshByPath(strAssetPath, xMaterials);
	pxModel->Export((strDir + szName + ZENITH_MODEL_EXT).c_str());

	Zenith_Log(LOG_CATEGORY_ASSET, "  %s: %u verts, %u tris (%s)",
		szName, pxMesh->GetNumVerts(), pxMesh->GetNumIndices() / 3u, szMaterialName);
}

} // namespace

//=============================================================================
// Entry point — called from GenerateTestAssets() at every editor boot.
//=============================================================================
void GenerateFallenTreeAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET,
		"Generating shared FallenTree assets (4 deadwood meshes, bark + mossy-bark PBR sets)...");

	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/FallenTrees/";
	std::filesystem::create_directories(strOutputDir);

	// --- Textures + materials first: the models reference them by path. -------
	{
		BarkTextureParams xBark;
		xBark.m_szName = "Bark";
		xBark.m_xFurrowColour = Zenith_Maths::Vector3(0.150f, 0.108f, 0.072f);
		xBark.m_xRidgeColour  = Zenith_Maths::Vector3(0.620f, 0.478f, 0.322f);
		xBark.m_fMossAmount = 0.0f;
		xBark.m_fRoughRidge = 0.58f;
		xBark.m_fRoughFurrow = 0.97f;
		xBark.m_uSeed = 3301u;
		GenerateBarkTextureSet(strOutputDir, xBark);
		GenerateBarkMaterial(strOutputDir, "Bark");

		BarkTextureParams xMossy;
		xMossy.m_szName = "MossyBark";
		xMossy.m_xFurrowColour = Zenith_Maths::Vector3(0.130f, 0.120f, 0.075f);
		xMossy.m_xRidgeColour  = Zenith_Maths::Vector3(0.548f, 0.465f, 0.312f);
		xMossy.m_xMossColour   = Zenith_Maths::Vector3(0.215f, 0.360f, 0.140f);
		xMossy.m_fMossAmount = 0.62f;
		xMossy.m_fRoughRidge = 0.70f;
		xMossy.m_fRoughFurrow = 0.98f;
		xMossy.m_uSeed = 6607u;
		GenerateBarkTextureSet(strOutputDir, xMossy);
		GenerateBarkMaterial(strOutputDir, "MossyBark");
	}

	// --- The pieces, straight off the variant table. --------------------------
	for (int iVariant = 0; iVariant < FALLEN_TREE_VARIANT_COUNT; iVariant++)
	{
		const FallenTreeVariantSpec xSpec = FallenTreeVariantAt(iVariant);
		Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
		pxMesh->Reserve(8192u, 24576u);
		BuildFallenTreeVariantMesh(*pxMesh, xSpec);
		ExportFallenTree(strOutputDir, xSpec.m_szName, pxMesh, xSpec.m_szMaterial);
		delete pxMesh;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "FallenTree assets generated at: %s", strOutputDir.c_str());
}

#include "Zenith_Tools_FallenTreeAssetExport.Tests.inl"

#endif // ZENITH_TOOLS
