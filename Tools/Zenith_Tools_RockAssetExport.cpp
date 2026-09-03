#include "Zenith.h"

//=============================================================================
// Zenith_Tools_RockAssetExport
//
// The SHARED procedural rock set — engine assets, not a game's, so any game can
// scatter the same stone without re-authoring it. Generated at every tools boot
// (from GenerateTestAssets), entirely from fixed seeds, so repeated boots
// regenerate byte-identical assets.
//
//   ENGINE_ASSETS_DIR/Meshes/Rocks/
//     Rock_Boulder.{zasset,zmesh,zmodel}   — rounded, weathered field boulder
//     Rock_Slab.{zasset,zmesh,zmodel}      — flat fractured flagstone
//     Rock_Shard.{zasset,zmesh,zmodel}     — upright angular standing stone
//     Rock_Pebbles.{zasset,zmesh,zmodel}   — a cluster of six small stones
//     Rock_Granite_{Albedo,Normal,RM,AO,Height}.ztxtr
//     Rock_Sandstone_{Albedo,Normal,RM,AO,Height}.ztxtr
//     Rock_Detail_{Albedo,Normal}.ztxtr   — shared mineral grit, tiled 6x
//     Rock_Granite.zmtrl  /  Rock_Sandstone.zmtrl
//
// Three decisions carry most of the visual result, and each of them is the
// reason a naive version of this file looks like a lumpy potato:
//
// 1. HALF-SPACE FACET CUTS. Displaced noise on a sphere gives blobs; real stone
//    is fractured. After displacement each rock is clipped by a handful of
//    random half-spaces (vertices on the outside are PROJECTED onto the plane),
//    which produces genuinely flat, sharp-edged facets rather than a smooth
//    surface pretending to have them via a normal map.
//
// 2. BOX-PROJECTED, WORLD-SCALE UVs, EMITTED PER TRIANGLE. A sphere unwrap
//    pinches at the poles and seams down the wrap meridian, and both are plainly
//    visible on a 2 m boulder. Each triangle is instead projected along its own
//    dominant axis at a fixed metres-per-tile, so texel density is near-uniform
//    and there is no pole and no seam. That REQUIRES the textures to tile, which
//    is why the generators below sample a wrapped integer lattice
//    (TileableFBM) rather than Zenith_TerrainNoise::FBM.
//
// 3. PER-TRIANGLE NORMALS BLENDED TOWARD THE SMOOTH ONE. Vertices are duplicated
//    per triangle anyway (for 2), so the blend weight is free and is what
//    separates a weathered boulder (mostly smooth) from a fresh shard (mostly
//    faceted) using the same geometry core.
//
// The mesh/material/model writers are the ordinary asset APIs — nothing here is
// rock-specific infrastructure, so a new stone type is a new RockShapeParams row.
//=============================================================================

#ifndef ZENITH_TOOLS

// Asset generation is a tools-build capability (the mesh Export APIs only exist
// there); non-tools builds get a no-op so GenerateTestAssets links.
void GenerateProceduralRockAssets()
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

//=============================================================================
// Export version. Every boot regenerates this set unconditionally, so this is
// not a staleness gate — it is the number a log line (and a bug report) can
// name when a capture and the current generator disagree. BUMP IT whenever the
// emitted bytes change.
//
// 2: height map exported + POM enabled, shared mineral-grit detail pair.
//=============================================================================
constexpr u_int uROCK_ASSET_EXPORT_VERSION = 2u;

//=============================================================================
// Shape description. One row per stone type; everything else is shared.
//=============================================================================
struct RockShapeParams
{
	u_int                 m_uSubdivisions   = 3u;     // 0 = 20 tris, 3 = 1280 tris
	Zenith_Maths::Vector3 m_xAxisScale      = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
	float                 m_fLumpAmplitude  = 0.34f;  // silhouette-scale displacement
	float                 m_fLumpFrequency  = 1.60f;
	float                 m_fBumpAmplitude  = 0.13f;  // hand-scale
	float                 m_fBumpFrequency  = 4.50f;
	float                 m_fGrainAmplitude = 0.045f; // finger-scale (the normal map does the rest)
	float                 m_fGrainFrequency = 11.0f;
	u_int                 m_uFacetPlanes    = 7u;
	float                 m_fFacetDepthMin  = 0.03f;  // fraction of the support distance removed
	float                 m_fFacetDepthMax  = 0.16f;
	float                 m_fBaseFlatten    = 0.16f;  // fraction of total height cut off the bottom
	float                 m_fNormalSmooth   = 0.70f;  // 0 = fully faceted, 1 = fully smooth
	float                 m_fWidthMetres    = 2.40f;  // target horizontal extent
	float                 m_fUVTileMetres   = 1.30f;  // world metres per texture tile
};

// The tileable noise these textures are built on lives in Zenith_TerrainNoise
// (Maths/Zenith_Noise.h) -- it is shared with the fallen-tree generator, and the
// wrapped lattice is the whole reason the box-projected UVs below have no seam.
using Zenith_TerrainNoise::TileableValueNoise;
using Zenith_TerrainNoise::TileableFBM;
using Zenith_TerrainNoise::TileableRidged;

float SmoothStepF(float fEdge0, float fEdge1, float fX)
{
	const float fT = std::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

//=============================================================================
// Icosphere — a subdivided icosahedron. Uniform triangle density with no pole
// and no wrap seam, which is exactly what a rock shell wants (a UV sphere would
// bunch its vertices at the poles and waste them there).
//=============================================================================
struct MidpointEntry
{
	u_int m_uA = 0u;
	u_int m_uB = 0u;
	u_int m_uMid = 0u;
};

u_int FindOrCreateMidpoint(Zenith_Vector<Zenith_Maths::Vector3>& xVerts,
	Zenith_Vector<MidpointEntry>& xCache, u_int uA, u_int uB)
{
	const u_int uLo = std::min(uA, uB);
	const u_int uHi = std::max(uA, uB);
	for (u_int u = 0; u < xCache.GetSize(); u++)
	{
		const MidpointEntry& xEntry = xCache.Get(u);
		if (xEntry.m_uA == uLo && xEntry.m_uB == uHi)
		{
			return xEntry.m_uMid;
		}
	}
	xVerts.PushBack(glm::normalize(xVerts.Get(uA) + xVerts.Get(uB)));
	MidpointEntry xNew;
	xNew.m_uA = uLo;
	xNew.m_uB = uHi;
	xNew.m_uMid = xVerts.GetSize() - 1u;
	xCache.PushBack(xNew);
	return xNew.m_uMid;
}

void BuildIcosphere(u_int uSubdivisions, Zenith_Vector<Zenith_Maths::Vector3>& xVertsOut,
	Zenith_Vector<u_int>& xIndicesOut)
{
	const float fT = (1.0f + sqrtf(5.0f)) * 0.5f;
	const float afSeed[12][3] = {
		{ -1.0f,   fT, 0.0f }, {  1.0f,   fT, 0.0f }, { -1.0f,  -fT, 0.0f }, {  1.0f,  -fT, 0.0f },
		{  0.0f, -1.0f,   fT }, {  0.0f,  1.0f,   fT }, {  0.0f, -1.0f,  -fT }, {  0.0f,  1.0f,  -fT },
		{    fT,  0.0f, -1.0f }, {    fT,  0.0f,  1.0f }, {   -fT,  0.0f, -1.0f }, {   -fT,  0.0f,  1.0f },
	};
	for (u_int u = 0; u < 12u; u++)
	{
		xVertsOut.PushBack(glm::normalize(
			Zenith_Maths::Vector3(afSeed[u][0], afSeed[u][1], afSeed[u][2])));
	}

	const u_int auSeedFaces[20][3] = {
		{ 0, 11,  5 }, { 0,  5,  1 }, { 0,  1,  7 }, { 0,  7, 10 }, { 0, 10, 11 },
		{ 1,  5,  9 }, { 5, 11,  4 }, { 11, 10,  2 }, { 10,  7,  6 }, { 7,  1,  8 },
		{ 3,  9,  4 }, { 3,  4,  2 }, { 3,  2,  6 }, { 3,  6,  8 }, { 3,  8,  9 },
		{ 4,  9,  5 }, { 2,  4, 11 }, { 6,  2, 10 }, { 8,  6,  7 }, { 9,  8,  1 },
	};
	for (u_int u = 0; u < 20u; u++)
	{
		xIndicesOut.PushBack(auSeedFaces[u][0]);
		xIndicesOut.PushBack(auSeedFaces[u][1]);
		xIndicesOut.PushBack(auSeedFaces[u][2]);
	}

	for (u_int uLevel = 0; uLevel < uSubdivisions; uLevel++)
	{
		Zenith_Vector<u_int> xNext;
		Zenith_Vector<MidpointEntry> xCache;
		for (u_int uTri = 0; uTri < xIndicesOut.GetSize(); uTri += 3u)
		{
			const u_int uA = xIndicesOut.Get(uTri);
			const u_int uB = xIndicesOut.Get(uTri + 1u);
			const u_int uC = xIndicesOut.Get(uTri + 2u);
			const u_int uAB = FindOrCreateMidpoint(xVertsOut, xCache, uA, uB);
			const u_int uBC = FindOrCreateMidpoint(xVertsOut, xCache, uB, uC);
			const u_int uCA = FindOrCreateMidpoint(xVertsOut, xCache, uC, uA);

			const u_int auNew[4][3] = {
				{ uA, uAB, uCA }, { uB, uBC, uAB }, { uC, uCA, uBC }, { uAB, uBC, uCA },
			};
			for (u_int uSub = 0; uSub < 4u; uSub++)
			{
				xNext.PushBack(auNew[uSub][0]);
				xNext.PushBack(auNew[uSub][1]);
				xNext.PushBack(auNew[uSub][2]);
			}
		}
		xIndicesOut = xNext;
	}

	// Normalise the winding to the ENGINE convention -- cross(C-A, B-A) outward --
	// before anything downstream reads it. The canonical icosahedron face list is
	// published the other way round (cross(B-A, C-A) outward), and the difference
	// is not cosmetic: ComputeShellNormals accumulates cross(C-A, B-A), so an
	// unnormalised shell hands every vertex an INWARD smooth normal. The emitter
	// then blends the face normal toward it, and at m_fNormalSmooth 0.62 the
	// boulder's shading normal ends up pointing mostly INTO the rock -- which
	// renders as a dark, featureless lump that still silhouettes correctly, so it
	// reads as "the albedo is too dark" rather than as an inverted normal. It cost
	// one round of albedo tuning before RockAssets.NormalsAndTangentsAreOrthonormal-
	// AndOutward named it.
	for (u_int uTri = 0; uTri < xIndicesOut.GetSize(); uTri += 3u)
	{
		const Zenith_Maths::Vector3& xA = xVertsOut.Get(xIndicesOut.Get(uTri));
		const Zenith_Maths::Vector3& xB = xVertsOut.Get(xIndicesOut.Get(uTri + 1u));
		const Zenith_Maths::Vector3& xC = xVertsOut.Get(xIndicesOut.Get(uTri + 2u));
		// The shell is still the unit sphere here, so the centroid IS the outward
		// direction and the test needs no reference point.
		if (glm::dot(glm::cross(xC - xA, xB - xA), xA + xB + xC) < 0.0f)
		{
			const u_int uSwap = xIndicesOut.Get(uTri + 1u);
			xIndicesOut.Get(uTri + 1u) = xIndicesOut.Get(uTri + 2u);
			xIndicesOut.Get(uTri + 2u) = uSwap;
		}
	}
}

//=============================================================================
// Shaping: displacement, facet cuts, base flatten, and the fit to a target size.
//=============================================================================
void DisplaceRockShell(const RockShapeParams& xParams, u_int uSeed,
	Zenith_Vector<Zenith_Maths::Vector3>& xVerts, Zenith_Vector<float>& xShellRadiusOut)
{
	xShellRadiusOut.Resize(xVerts.GetSize(), 1.0f);
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		const Zenith_Maths::Vector3 xDir = xVerts.Get(u);
		// Three octave bands, sampled on the DIRECTION so there is no seam. Each
		// is re-centred on zero (the noise returns [0,1]) so the mean radius
		// stays 1 and m_fWidthMetres still means what it says.
		const float fLump = Zenith_TerrainNoise::FBM3D(
			xDir.x * xParams.m_fLumpFrequency, xDir.y * xParams.m_fLumpFrequency,
			xDir.z * xParams.m_fLumpFrequency, uSeed, 3u, 2.0f, 0.5f) - 0.5f;
		const float fBump = Zenith_TerrainNoise::FBM3D(
			xDir.x * xParams.m_fBumpFrequency, xDir.y * xParams.m_fBumpFrequency,
			xDir.z * xParams.m_fBumpFrequency, uSeed + 977u, 3u, 2.0f, 0.5f) - 0.5f;
		const float fGrain = Zenith_TerrainNoise::ValueNoise3D(
			xDir.x * xParams.m_fGrainFrequency, xDir.y * xParams.m_fGrainFrequency,
			xDir.z * xParams.m_fGrainFrequency, uSeed + 4241u) - 0.5f;

		const float fRadius = 1.0f
			+ fLump * 2.0f * xParams.m_fLumpAmplitude
			+ fBump * 2.0f * xParams.m_fBumpAmplitude
			+ fGrain * 2.0f * xParams.m_fGrainAmplitude;

		xShellRadiusOut.Get(u) = fRadius;
		xVerts.Get(u) = Zenith_Maths::Vector3(
			xDir.x * fRadius * xParams.m_xAxisScale.x,
			xDir.y * fRadius * xParams.m_xAxisScale.y,
			xDir.z * fRadius * xParams.m_xAxisScale.z);
	}
}

// Clip the shell by m_uFacetPlanes random half-spaces. A vertex outside a plane
// is PROJECTED onto it, so the cut produces a genuinely flat face; triangles
// that end up entirely inside the removed slab collapse to zero area and are
// dropped at emit time.
void CutRockFacets(const RockShapeParams& xParams, u_int uSeed,
	Zenith_Vector<Zenith_Maths::Vector3>& xVerts, Zenith_Vector<float>& xRecessOut)
{
	// The accumulated projection distance IS the shading cue: a vertex a cut moved
	// a long way inward sits in a hollow between facets. Measuring it here beats
	// inferring it from |p - centre| afterwards, which on an elongated body (the
	// shard) is dominated by the LONG axis and so darkens the whole stone almost
	// uniformly -- flat loss, no variation. That is what the first pass shipped,
	// and together with an over-dark albedo it is why the rocks came out near-black.
	xRecessOut.Resize(xVerts.GetSize(), 0.0f);
	Zenith_TerrainNoise::XorShift32 xRng(uSeed * 2654435761u + 1u);
	for (u_int uPlane = 0; uPlane < xParams.m_uFacetPlanes; uPlane++)
	{
		// Uniform direction on the sphere, then folded AWAY from straight down:
		// the underside is flattened separately and a cut there is never seen.
		const float fZ = xRng.NextFloat01() * 2.0f - 1.0f;
		const float fPhi = xRng.NextFloat01() * 6.2831853f;
		const float fR = sqrtf(std::max(0.0f, 1.0f - fZ * fZ));
		Zenith_Maths::Vector3 xNormal(fR * cosf(fPhi), fZ, fR * sinf(fPhi));
		if (xNormal.y < -0.25f)
		{
			xNormal.y = -xNormal.y;
		}
		xNormal = glm::normalize(xNormal);

		float fSupport = -1.0e30f;
		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			fSupport = std::max(fSupport, glm::dot(xVerts.Get(u), xNormal));
		}
		const float fDepth = xParams.m_fFacetDepthMin +
			(xParams.m_fFacetDepthMax - xParams.m_fFacetDepthMin) * xRng.NextFloat01();
		const float fPlaneD = fSupport * (1.0f - fDepth);

		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			const float fDist = glm::dot(xVerts.Get(u), xNormal);
			if (fDist > fPlaneD)
			{
				xVerts.Get(u) = xVerts.Get(u) - xNormal * (fDist - fPlaneD);
				xRecessOut.Get(u) += fDist - fPlaneD;
			}
		}
	}
}

// Flatten the underside, drop the origin onto that flat, and scale so the
// horizontal extent matches the requested width. Origin-at-base is what lets a
// scatter place an instance at the sampled terrain height with no per-mesh
// offset table.
void SettleRockOntoBase(const RockShapeParams& xParams,
	Zenith_Vector<Zenith_Maths::Vector3>& xVerts)
{
	float fMinY = 1.0e30f;
	float fMaxY = -1.0e30f;
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		fMinY = std::min(fMinY, xVerts.Get(u).y);
		fMaxY = std::max(fMaxY, xVerts.Get(u).y);
	}
	const float fCutY = fMinY + (fMaxY - fMinY) * xParams.m_fBaseFlatten;
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		Zenith_Maths::Vector3& xP = xVerts.Get(u);
		xP.y = std::max(xP.y, fCutY) - fCutY;
	}

	float fMaxHorizontal = 0.0f;
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		const Zenith_Maths::Vector3& xP = xVerts.Get(u);
		fMaxHorizontal = std::max(fMaxHorizontal, sqrtf(xP.x * xP.x + xP.z * xP.z));
	}
	if (fMaxHorizontal > 1.0e-4f)
	{
		const float fScale = (xParams.m_fWidthMetres * 0.5f) / fMaxHorizontal;
		for (u_int u = 0; u < xVerts.GetSize(); u++)
		{
			xVerts.Get(u) = xVerts.Get(u) * fScale;
		}
	}
}

void ComputeShellNormals(const Zenith_Vector<Zenith_Maths::Vector3>& xVerts,
	const Zenith_Vector<u_int>& xIndices, Zenith_Vector<Zenith_Maths::Vector3>& xNormalsOut)
{
	xNormalsOut.Resize(xVerts.GetSize(), Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	for (u_int uTri = 0; uTri < xIndices.GetSize(); uTri += 3u)
	{
		const u_int uA = xIndices.Get(uTri);
		const u_int uB = xIndices.Get(uTri + 1u);
		const u_int uC = xIndices.Get(uTri + 2u);
		// Engine convention: cross(C-A, B-A) points OUTWARD for a front-facing
		// triangle (anchored on Zenith_MeshAsset::GenerateUnitCube). Left
		// un-normalized, so the accumulation is area-weighted.
		const Zenith_Maths::Vector3 xFace = glm::cross(
			xVerts.Get(uC) - xVerts.Get(uA), xVerts.Get(uB) - xVerts.Get(uA));
		xNormalsOut.Get(uA) = xNormalsOut.Get(uA) + xFace;
		xNormalsOut.Get(uB) = xNormalsOut.Get(uB) + xFace;
		xNormalsOut.Get(uC) = xNormalsOut.Get(uC) + xFace;
	}
	for (u_int u = 0; u < xNormalsOut.GetSize(); u++)
	{
		const Zenith_Maths::Vector3& xN = xNormalsOut.Get(u);
		xNormalsOut.Get(u) = (glm::dot(xN, xN) > 1.0e-12f)
			? glm::normalize(xN)
			: glm::normalize(xVerts.Get(u) + Zenith_Maths::Vector3(0.0f, 0.001f, 0.0f));
	}
}

//=============================================================================
// Emission — one rock's triangles appended to a mesh asset, box-UV mapped, with
// per-triangle vertices so the facet normals survive.
//=============================================================================
void AppendRockGeometry(Zenith_MeshAsset& xMesh, const RockShapeParams& xParams,
	u_int uSeed, const Zenith_Maths::Vector3& xOffset, float fYawRadians, float fScale)
{
	Zenith_Vector<Zenith_Maths::Vector3> xVerts;
	Zenith_Vector<u_int> xIndices;
	Zenith_Vector<float> xShellRadius;
	Zenith_Vector<float> xRecess;
	BuildIcosphere(xParams.m_uSubdivisions, xVerts, xIndices);
	DisplaceRockShell(xParams, uSeed, xVerts, xShellRadius);
	CutRockFacets(xParams, uSeed, xVerts, xRecess);
	SettleRockOntoBase(xParams, xVerts);

	// Self-normalising, so every stone gets the full tint range regardless of how
	// deep its own cuts happened to be.
	float fMaxRecess = 1.0e-4f;
	for (u_int u = 0; u < xRecess.GetSize(); u++)
	{
		fMaxRecess = std::max(fMaxRecess, xRecess.Get(u));
	}

	// Place: uniform scale, yaw about Y, then translate. Done here rather than by
	// the caller so a cluster is N calls into one mesh.
	const float fCosYaw = cosf(fYawRadians);
	const float fSinYaw = sinf(fYawRadians);
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		const Zenith_Maths::Vector3 xP = xVerts.Get(u) * fScale;
		xVerts.Get(u) = Zenith_Maths::Vector3(
			xP.x * fCosYaw + xP.z * fSinYaw, xP.y,
			-xP.x * fSinYaw + xP.z * fCosYaw) + xOffset;
	}

	Zenith_Vector<Zenith_Maths::Vector3> xSmoothNormals;
	ComputeShellNormals(xVerts, xIndices, xSmoothNormals);

	// Cavity tint, baked into vertex colour: hollows between facets, plus the dirt
	// line at the base. This is the large-scale variation a tiling texture cannot
	// supply -- and it costs nothing extra, because the vertices are already being
	// duplicated. It DARKENS ONLY, from an unmodulated 1.0 baseline: a tint that
	// darkens everywhere is not shading, it is an albedo cut with extra steps.
	Zenith_Maths::Vector3 xCentre(0.0f, 0.0f, 0.0f);
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		xCentre = xCentre + xVerts.Get(u);
	}
	xCentre = xCentre / static_cast<float>(xVerts.GetSize());

	float fMaxY = 1.0e-4f;
	for (u_int u = 0; u < xVerts.GetSize(); u++)
	{
		fMaxY = std::max(fMaxY, xVerts.Get(u).y - xOffset.y);
	}

	const float fInvTile = 1.0f / std::max(0.05f, xParams.m_fUVTileMetres);
	const u_int uBaseVert = xMesh.GetNumVerts();
	u_int uEmitted = 0u;

	for (u_int uTri = 0; uTri < xIndices.GetSize(); uTri += 3u)
	{
		u_int auIdx[3] = { xIndices.Get(uTri), xIndices.Get(uTri + 1u), xIndices.Get(uTri + 2u) };
		Zenith_Maths::Vector3 xA = xVerts.Get(auIdx[0]);
		Zenith_Maths::Vector3 xB = xVerts.Get(auIdx[1]);
		Zenith_Maths::Vector3 xC = xVerts.Get(auIdx[2]);

		// Guarantee the engine's winding rule regardless of what the cuts did:
		// cross(C-A, B-A) must point away from the body's centre.
		Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
		const Zenith_Maths::Vector3 xCentroid = (xA + xB + xC) / 3.0f;
		if (glm::dot(xFace, xCentroid - xCentre) < 0.0f)
		{
			std::swap(auIdx[1], auIdx[2]);
			std::swap(xB, xC);
			xFace = glm::cross(xC - xA, xB - xA);
		}
		const float fAreaSq = glm::dot(xFace, xFace);
		if (fAreaSq < 1.0e-12f)
		{
			continue;   // collapsed by a facet cut — nothing to draw
		}
		const Zenith_Maths::Vector3 xFaceNormal = xFace / sqrtf(fAreaSq);

		// Box projection along the face's dominant axis: uniform texel density,
		// no pole, no wrap seam. The tangent is that projection's own u axis.
		const float fAX = fabsf(xFaceNormal.x);
		const float fAY = fabsf(xFaceNormal.y);
		const float fAZ = fabsf(xFaceNormal.z);
		const int iAxis = (fAY >= fAX && fAY >= fAZ) ? 1 : ((fAX >= fAZ) ? 0 : 2);

		const Zenith_Maths::Vector3 axCorners[3] = { xA, xB, xC };
		for (u_int uCorner = 0; uCorner < 3u; uCorner++)
		{
			const Zenith_Maths::Vector3& xP = axCorners[uCorner];
			Zenith_Maths::Vector2 xUV;
			Zenith_Maths::Vector3 xTangent;
			if (iAxis == 1)
			{
				xUV = Zenith_Maths::Vector2(xP.x * fInvTile, xP.z * fInvTile);
				xTangent = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			}
			else if (iAxis == 0)
			{
				xUV = Zenith_Maths::Vector2(xP.z * fInvTile, xP.y * fInvTile);
				xTangent = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
			}
			else
			{
				xUV = Zenith_Maths::Vector2(xP.x * fInvTile, xP.y * fInvTile);
				xTangent = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			}

			const Zenith_Maths::Vector3 xSmooth = xSmoothNormals.Get(auIdx[uCorner]);
			Zenith_Maths::Vector3 xNormal =
				xFaceNormal * (1.0f - xParams.m_fNormalSmooth) + xSmooth * xParams.m_fNormalSmooth;
			xNormal = (glm::dot(xNormal, xNormal) > 1.0e-12f) ? glm::normalize(xNormal) : xFaceNormal;

			// Gram-Schmidt, so the shader's TBN is orthonormal to the SHADING
			// normal rather than to the face's.
			xTangent = xTangent - xNormal * glm::dot(xNormal, xTangent);
			xTangent = (glm::dot(xTangent, xTangent) > 1.0e-8f)
				? glm::normalize(xTangent)
				: glm::normalize(glm::cross(xNormal, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

			const float fCavity = xRecess.Get(auIdx[uCorner]) / fMaxRecess;
			const float fLump = std::clamp((1.0f - xShellRadius.Get(auIdx[uCorner])) / 0.35f, 0.0f, 1.0f);
			const float fLift = SmoothStepF(0.0f, 0.18f, (xP.y - xOffset.y) / fMaxY);
			const float fTint = (1.0f - 0.20f * fCavity) * (1.0f - 0.11f * fLump) * (0.88f + 0.12f * fLift);
			xMesh.AddVertex(xP, xNormal, xUV, xTangent,
				Zenith_Maths::Vector4(fTint, fTint, fTint, 1.0f));
		}

		const u_int uV = uBaseVert + uEmitted * 3u;
		xMesh.AddTriangle(uV, uV + 1u, uV + 2u);
		uEmitted++;
	}
}

//=============================================================================
// Textures.
//=============================================================================
struct RockTextureParams
{
	const char*           m_szName         = "Granite";
	Zenith_Maths::Vector3 m_xCreviceColour = Zenith_Maths::Vector3(0.085f, 0.082f, 0.080f);
	Zenith_Maths::Vector3 m_xRidgeColour   = Zenith_Maths::Vector3(0.400f, 0.395f, 0.385f);
	float                 m_fSpeckle       = 1.0f;   // mineral grain contrast
	float                 m_fBedding       = 0.0f;   // horizontal sedimentary banding
	float                 m_fRoughRidge    = 0.62f;
	float                 m_fRoughCrevice  = 0.94f;
	Zenith_Maths::Vector3 m_xStainColour   = Zenith_Maths::Vector3(0.20f, 0.24f, 0.12f);
	float                 m_fStainAmount   = 0.55f;
	u_int                 m_uSeed          = 5501u;
};

// The single height field every other map derives from, sampled over [0,1)^2.
float RockSurfaceHeight(const RockTextureParams& xParams, float fU, float fV, float& fCrackOut)
{
	const float fCoarse = TileableFBM(fU, fV, 5, 3u, xParams.m_uSeed);
	const float fMedium = TileableFBM(fU, fV, 17, 3u, xParams.m_uSeed + 311u);
	const float fFine   = TileableValueNoise(fU, fV, 96, xParams.m_uSeed + 733u);
	float fHeight = fCoarse * 0.52f + fMedium * 0.33f + fFine * 0.15f;

	// Sedimentary bedding: near-horizontal bands, wobbled by the coarse field so
	// they read as strata rather than as stripes.
	if (xParams.m_fBedding > 0.0f)
	{
		const float fBand = 0.5f + 0.5f * sinf((fV * 9.0f + fCoarse * 1.4f) * 6.2831853f);
		fHeight = fHeight * (1.0f - xParams.m_fBedding * 0.55f) + fBand * xParams.m_fBedding * 0.55f;
	}

	// Cracks cut into the surface; also handed back so albedo/AO can darken there.
	fCrackOut = SmoothStepF(0.70f, 0.88f, TileableRidged(fU, fV, 7, 4u, xParams.m_uSeed + 1289u));
	fHeight -= fCrackOut * 0.40f;
	return std::clamp(fHeight, 0.0f, 1.0f);
}

void GenerateRockTextureSet(const std::string& strDir, const RockTextureParams& xParams)
{
	constexpr int32_t iCOLOUR_SIZE = 1024;
	constexpr int32_t iDATA_SIZE = 512;

	Zenith_Vector<float> xHeight;
	Zenith_Vector<float> xCrack;
	xHeight.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE, 0.0f);
	xCrack.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE, 0.0f);
	for (int32_t iY = 0; iY < iCOLOUR_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iCOLOUR_SIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iCOLOUR_SIZE;
			const float fV = static_cast<float>(iY) / iCOLOUR_SIZE;
			float fCrack = 0.0f;
			xHeight.Get(iY * iCOLOUR_SIZE + iX) = RockSurfaceHeight(xParams, fU, fV, fCrack);
			xCrack.Get(iY * iCOLOUR_SIZE + iX) = fCrack;
		}
	}

	// --- Height (linear grey, BC1) -------------------------------------------
	// The generator has always COMPUTED this field and thrown it away; exported,
	// it is what lets the material's parallax occlusion step actually shift the
	// UVs, so a crevice occludes the plate beside it at a grazing angle instead
	// of being a painted line that slides across a flat facet.
	//
	// BC1 rather than a single-mip R8: POM samples the height minified along the
	// whole ray, and a chain-less height map aliases into stair-stepping.
	{
		Zenith_Vector<u_int8> xHeightTex;
		xHeightTex.Resize(iCOLOUR_SIZE * iCOLOUR_SIZE * 4, 0);
		for (int32_t i = 0; i < iCOLOUR_SIZE * iCOLOUR_SIZE; i++)
		{
			const u_int8 ucH = static_cast<u_int8>(std::clamp(xHeight.Get(i), 0.0f, 1.0f) * 255.0f);
			u_int8* pucH = &xHeightTex.Get(i * 4);
			pucH[0] = ucH;
			pucH[1] = ucH;
			pucH[2] = ucH;
			pucH[3] = 255;
		}
		Zenith_Tools_TextureExport::ExportFromDataCompressed(
			xHeightTex.GetDataPointer(), strDir + "Rock_" + xParams.m_szName + "_Height" ZENITH_TEXTURE_EXT,
			iCOLOUR_SIZE, iCOLOUR_SIZE, TextureCompressionMode::BC1, TextureColourSpace::Linear);
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
				xParams.m_xCreviceColour + (xParams.m_xRidgeColour - xParams.m_xCreviceColour) * fH;

			// Mineral grains: three classes off one per-texel hash. This is what
			// makes granite read as granite rather than as grey noise — quartz
			// glints bright and cool, feldspar is warm, biotite is near-black.
			const float fGrain = Zenith_TerrainNoise::HashToFloat01(
				Zenith_TerrainNoise::HashCoords(iX, iY, xParams.m_uSeed + 61u));
			if (fGrain > 0.885f)
			{
				xColour = xColour + Zenith_Maths::Vector3(0.24f, 0.25f, 0.27f) * xParams.m_fSpeckle;
			}
			else if (fGrain > 0.640f)
			{
				xColour = xColour + Zenith_Maths::Vector3(0.10f, 0.085f, 0.065f) * xParams.m_fSpeckle;
			}
			else if (fGrain < 0.085f)
			{
				xColour = xColour - Zenith_Maths::Vector3(0.055f, 0.055f, 0.050f) * xParams.m_fSpeckle;
			}

			// Organic staining: lichen on granite, iron oxide on sandstone.
			const float fStain = SmoothStepF(0.56f, 0.80f,
				TileableFBM(fU, fV, 3, 4u, xParams.m_uSeed + 91u)) * xParams.m_fStainAmount;
			xColour = xColour + (xParams.m_xStainColour - xColour) * fStain;

			// Crevices are darker than the plates they separate.
			xColour = xColour * (1.0f - 0.32f * xCrack.Get(iIdx));

			u_int8* pucA = &xAlbedo.Get(iIdx * 4);
			pucA[0] = static_cast<u_int8>(std::clamp(xColour.x, 0.0f, 1.0f) * 255.0f);
			pucA[1] = static_cast<u_int8>(std::clamp(xColour.y, 0.0f, 1.0f) * 255.0f);
			pucA[2] = static_cast<u_int8>(std::clamp(xColour.z, 0.0f, 1.0f) * 255.0f);
			pucA[3] = 255;
		}
	}
	// Full offline mip chain (sRGB-filtered): a single-mip albedo shimmers into
	// noise at the distance a rock is actually seen from.
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xAlbedo.GetDataPointer(), strDir + "Rock_" + xParams.m_szName + "_Albedo" ZENITH_TEXTURE_EXT, iCOLOUR_SIZE, iCOLOUR_SIZE, TEXTURE_FORMAT_RGBA8_SRGB);

	// --- Normal (linear): central differences on the WRAPPED height field -----
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
			const float fDX = (xHeight.Get(iY * iCOLOUR_SIZE + iXP) - xHeight.Get(iY * iCOLOUR_SIZE + iXM)) * 6.0f;
			const float fDY = (xHeight.Get(iYP * iCOLOUR_SIZE + iX) - xHeight.Get(iYM * iCOLOUR_SIZE + iX)) * 6.0f;
			const Zenith_Maths::Vector3 xN =
				glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			u_int8* pucN = &xNormal.Get((iY * iCOLOUR_SIZE + iX) * 4);
			pucN[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
			pucN[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
			pucN[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
			pucN[3] = 255;
		}
	}
	// BC5 (R,G; the shader rebuilds Z) with a full mip chain, linear.
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xNormal.GetDataPointer(), strDir + "Rock_" + xParams.m_szName + "_Normal" ZENITH_TEXTURE_EXT, iCOLOUR_SIZE, iCOLOUR_SIZE,
		TextureCompressionMode::BC5, TextureColourSpace::Linear);

	// --- RM + AO (half res; neither carries detail the colour maps don't) -----
	// RM is the glTF layout the engine samples: G = roughness, B = metallic.
	// Stone is a dielectric, so B stays 0 AND the material's metallic multiplier
	// is 0 — two independent ways of saying the same thing, deliberately.
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
			float fCrack = 0.0f;
			const float fH = RockSurfaceHeight(xParams, fU, fV, fCrack);

			const float fRoughness = std::clamp(
				xParams.m_fRoughCrevice + (xParams.m_fRoughRidge - xParams.m_fRoughCrevice) * fH,
				0.05f, 1.0f);
			u_int8* pucRM = &xRM.Get((iY * iDATA_SIZE + iX) * 4);
			pucRM[0] = 0;
			pucRM[1] = static_cast<u_int8>(fRoughness * 255.0f);
			pucRM[2] = 0;
			pucRM[3] = 255;

			// Cavity occlusion: the low parts of the height field plus the cracks.
			const float fAO = std::clamp((0.72f + 0.28f * fH) * (1.0f - 0.28f * fCrack), 0.0f, 1.0f);
			const u_int8 ucAO = static_cast<u_int8>(fAO * 255.0f);
			u_int8* pucAO = &xAO.Get((iY * iDATA_SIZE + iX) * 4);
			pucAO[0] = ucAO;
			pucAO[1] = ucAO;
			pucAO[2] = ucAO;
			pucAO[3] = 255;
		}
	}
	// Linear data with full mip chains; uncompressed so the roughness/AO values
	// survive exactly.
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xRM.GetDataPointer(), strDir + "Rock_" + xParams.m_szName + "_RM" ZENITH_TEXTURE_EXT, iDATA_SIZE, iDATA_SIZE, TEXTURE_FORMAT_RGBA8_UNORM);
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xAO.GetDataPointer(), strDir + "Rock_" + xParams.m_szName + "_AO" ZENITH_TEXTURE_EXT, iDATA_SIZE, iDATA_SIZE, TEXTURE_FORMAT_RGBA8_UNORM);
}

//=============================================================================
// THE shared mineral-grit detail pair, generated once for the whole set.
//
// A 1024^2 map over a 1.3 m tile is ~1.3 mm per texel, which sounds fine until
// a camera is half a metre from a boulder: the surface goes smooth exactly
// where a real stone shows the most grain. The detail pair is tiled 6x on top
// (SetDetailTiling below), so it re-introduces sub-millimetre grain at contact
// range without a 4K base map, and it fades out with distance for free because
// its own mips converge on mid-grey / flat.
//
// The albedo is centred on MID-GREY on purpose: the shader's detail combine is
// the Unity convention, base * detail * 2, so 0.5 is the identity value and a
// detail map centred anywhere else silently rescales every rock's brightness.
//=============================================================================
constexpr int32_t iROCK_DETAIL_SIZE = 512;
constexpr u_int uROCK_DETAIL_SEED = 40961u;

// THE detail height field. Split out from the writers so the units read the
// same field the .ztxtr does rather than a re-derivation of it.
float RockDetailGrit(float fU, float fV)
{
	// Tileable at every octave — the pair is tiled 6x over surfaces whose own
	// UVs already tile, so a non-wrapping field would print a visible grid.
	const float fCoarse = TileableFBM(fU, fV, 11, 3u, uROCK_DETAIL_SEED);
	const float fFine = TileableValueNoise(fU, fV, 61, uROCK_DETAIL_SEED + 17u);
	return std::clamp(fCoarse * 0.55f + fFine * 0.45f, 0.0f, 1.0f);
}

void BuildRockDetailField(Zenith_Vector<float>& xGrit)
{
	xGrit.Clear();
	xGrit.Resize(iROCK_DETAIL_SIZE * iROCK_DETAIL_SIZE, 0.0f);
	for (int32_t iY = 0; iY < iROCK_DETAIL_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iROCK_DETAIL_SIZE; iX++)
		{
			const float fU = static_cast<float>(iX) / iROCK_DETAIL_SIZE;
			const float fV = static_cast<float>(iY) / iROCK_DETAIL_SIZE;
			xGrit.Get(iY * iROCK_DETAIL_SIZE + iX) = RockDetailGrit(fU, fV);
		}
	}
}

void BuildRockDetailAlbedoPixels(const Zenith_Vector<float>& xGrit, Zenith_Vector<u_int8>& xOut)
{
	xOut.Clear();
	xOut.Resize(iROCK_DETAIL_SIZE * iROCK_DETAIL_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iROCK_DETAIL_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iROCK_DETAIL_SIZE; iX++)
		{
			const int32_t iIdx = iY * iROCK_DETAIL_SIZE + iX;
			// +/- 12% around MID-GREY. The shader's detail combine is the Unity
			// convention, base * detail * 2, so 0.5 is the identity value: a map
			// centred anywhere else rescales every rock's brightness, which reads
			// as a lighting change rather than as a texture bug. Wider than +/-12%
			// and tiling it 6x makes the stone read as dusty rather than grainy.
			const float fValue = 0.5f + (xGrit.Get(iIdx) - 0.5f) * 0.24f;
			// A per-texel hash sparkle: the crystal glint the base map is too
			// coarse to hold at contact range.
			const float fSparkle = Zenith_TerrainNoise::HashToFloat01(
				Zenith_TerrainNoise::HashCoords(iX, iY, uROCK_DETAIL_SEED + 83u));
			const float fLift = fSparkle > 0.972f ? 0.10f : 0.0f;
			const u_int8 ucV = static_cast<u_int8>(std::clamp(fValue + fLift, 0.0f, 1.0f) * 255.0f);
			u_int8* pucA = &xOut.Get(iIdx * 4);
			pucA[0] = ucV;
			pucA[1] = ucV;
			pucA[2] = ucV;
			pucA[3] = 255;
		}
	}
}

void BuildRockDetailNormalPixels(const Zenith_Vector<float>& xGrit, Zenith_Vector<u_int8>& xOut)
{
	xOut.Clear();
	xOut.Resize(iROCK_DETAIL_SIZE * iROCK_DETAIL_SIZE * 4, 0);
	for (int32_t iY = 0; iY < iROCK_DETAIL_SIZE; iY++)
	{
		for (int32_t iX = 0; iX < iROCK_DETAIL_SIZE; iX++)
		{
			// WRAPPED differences: this map tiles, unlike a foliage card.
			const int32_t iXP = (iX + 1) % iROCK_DETAIL_SIZE;
			const int32_t iXM = (iX + iROCK_DETAIL_SIZE - 1) % iROCK_DETAIL_SIZE;
			const int32_t iYP = (iY + 1) % iROCK_DETAIL_SIZE;
			const int32_t iYM = (iY + iROCK_DETAIL_SIZE - 1) % iROCK_DETAIL_SIZE;
			const float fDX = (xGrit.Get(iY * iROCK_DETAIL_SIZE + iXP) -
				xGrit.Get(iY * iROCK_DETAIL_SIZE + iXM)) * 2.2f;
			const float fDY = (xGrit.Get(iYP * iROCK_DETAIL_SIZE + iX) -
				xGrit.Get(iYM * iROCK_DETAIL_SIZE + iX)) * 2.2f;
			const Zenith_Maths::Vector3 xN =
				glm::normalize(Zenith_Maths::Vector3(-fDX, -fDY, 1.0f));
			u_int8* pucN = &xOut.Get((iY * iROCK_DETAIL_SIZE + iX) * 4);
			pucN[0] = static_cast<u_int8>((xN.x * 0.5f + 0.5f) * 255.0f);
			pucN[1] = static_cast<u_int8>((xN.y * 0.5f + 0.5f) * 255.0f);
			pucN[2] = static_cast<u_int8>((xN.z * 0.5f + 0.5f) * 255.0f);
			pucN[3] = 255;
		}
	}
}

void GenerateRockDetailTextures(const std::string& strDir)
{
	Zenith_Vector<float> xGrit;
	BuildRockDetailField(xGrit);

	Zenith_Vector<u_int8> xPixels;
	BuildRockDetailAlbedoPixels(xGrit, xPixels);
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xPixels.GetDataPointer(), strDir + "Rock_Detail_Albedo" ZENITH_TEXTURE_EXT,
		iROCK_DETAIL_SIZE, iROCK_DETAIL_SIZE, TextureCompressionMode::BC1, TextureColourSpace::SRGB);

	BuildRockDetailNormalPixels(xGrit, xPixels);
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xPixels.GetDataPointer(), strDir + "Rock_Detail_Normal" ZENITH_TEXTURE_EXT,
		iROCK_DETAIL_SIZE, iROCK_DETAIL_SIZE, TextureCompressionMode::BC5, TextureColourSpace::Linear);
}

void GenerateRockMaterial(const std::string& strDir, const char* szName)
{
	const std::string strStem = std::string("engine:Meshes/Rocks/Rock_") + szName;
	auto xhMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxMaterial = xhMaterial.GetDirect();
	pxMaterial->SetName(std::string("Rock") + szName);
	pxMaterial->SetDiffuseTexture(TextureHandle(strStem + "_Albedo" ZENITH_TEXTURE_EXT));
	pxMaterial->SetNormalTexture(TextureHandle(strStem + "_Normal" ZENITH_TEXTURE_EXT));
	pxMaterial->SetRoughnessMetallicTexture(TextureHandle(strStem + "_RM" ZENITH_TEXTURE_EXT));
	pxMaterial->SetOcclusionTexture(TextureHandle(strStem + "_AO" ZENITH_TEXTURE_EXT));
	// With an RM map bound these two are MULTIPLIERS on the sampled channels, not
	// absolute values: 1.0 passes the authored roughness through unchanged, and 0
	// pins metallic off whatever the texture's B channel happens to hold.
	pxMaterial->SetRoughness(1.0f);
	pxMaterial->SetMetallic(0.0f);
	pxMaterial->SetOcclusionStrength(1.0f);
	pxMaterial->SetNormalStrength(1.0f);
	// POM is enabled by the PAIR — a bound height map AND a non-zero height
	// scale. BuildMaterialDrawConstants tests both, so setting one alone is a
	// silent no-op. 3 cm of relief over a ~1.3 m tile: enough for a crevice to
	// occlude at a graze, small enough that the flat facet never gives itself
	// away at the silhouette (POM cannot move a silhouette).
	pxMaterial->SetTexture(MATERIAL_TEXTURE_HEIGHT, TextureHandle(strStem + "_Height" ZENITH_TEXTURE_EXT));
	pxMaterial->SetHeightScale(0.03f);
	// Shared mineral grit, tiled 6x over the base UVs, for the contact-range
	// detail a 1024^2 base map cannot hold. Detail maps are enabled by the
	// presence of either detail texture, and the unbound detail MASK samples as
	// white, so the pair applies over the whole surface.
	pxMaterial->SetTexture(MATERIAL_TEXTURE_DETAIL_ALBEDO,
		TextureHandle("engine:Meshes/Rocks/Rock_Detail_Albedo" ZENITH_TEXTURE_EXT));
	pxMaterial->SetTexture(MATERIAL_TEXTURE_DETAIL_NORMAL,
		TextureHandle("engine:Meshes/Rocks/Rock_Detail_Normal" ZENITH_TEXTURE_EXT));
	pxMaterial->SetDetailTiling(Zenith_Maths::Vector2(6.0f, 6.0f));
	pxMaterial->SetBlendMode(MATERIAL_BLEND_OPAQUE);
	pxMaterial->SaveToFile(strDir + "Rock_" + szName + ZENITH_MATERIAL_EXT);
}

//=============================================================================
// The variant table. ONE definition of what each stone is, read by the exporter
// AND by the unit tests — a test that re-declares the production parameters is
// only testing its own copy of them, and the copy drifts the first time a knob
// is tuned (this one drifted within the hour it existed).
//=============================================================================
enum RockVariant
{
	ROCK_VARIANT_BOULDER = 0,
	ROCK_VARIANT_SLAB,
	ROCK_VARIANT_SHARD,
	ROCK_VARIANT_PEBBLES,
	ROCK_VARIANT_COUNT
};

struct RockVariantSpec
{
	const char*     m_szName = "Rock_Boulder";
	const char*     m_szMaterial = "Granite";
	u_int           m_uSeed = 0u;
	u_int           m_uClusterStones = 1u;   // >1 = several stones merged into one mesh
	RockShapeParams m_xShape;
};

RockVariantSpec RockVariantAt(int iVariant)
{
	RockVariantSpec xSpec;
	switch (iVariant)
	{
	case ROCK_VARIANT_BOULDER:
		// The workhorse. Rounded and weathered, wider than it is tall.
		xSpec.m_szName = "Rock_Boulder";
		xSpec.m_szMaterial = "Granite";
		xSpec.m_uSeed = 20261u;
		xSpec.m_xShape.m_uSubdivisions = 3u;
		xSpec.m_xShape.m_xAxisScale = Zenith_Maths::Vector3(1.15f, 0.82f, 1.00f);
		xSpec.m_xShape.m_uFacetPlanes = 7u;
		xSpec.m_xShape.m_fFacetDepthMin = 0.02f;
		xSpec.m_xShape.m_fFacetDepthMax = 0.14f;
		xSpec.m_xShape.m_fNormalSmooth = 0.62f;
		xSpec.m_xShape.m_fWidthMetres = 2.40f;
		xSpec.m_xShape.m_fUVTileMetres = 1.30f;
		break;

	case ROCK_VARIANT_SLAB:
		// A low fractured flagstone. Strong cuts, little smoothing.
		xSpec.m_szName = "Rock_Slab";
		xSpec.m_szMaterial = "Sandstone";
		xSpec.m_uSeed = 71219u;
		xSpec.m_xShape.m_uSubdivisions = 3u;
		xSpec.m_xShape.m_xAxisScale = Zenith_Maths::Vector3(1.50f, 0.30f, 1.15f);
		xSpec.m_xShape.m_fLumpAmplitude = 0.26f;
		xSpec.m_xShape.m_uFacetPlanes = 8u;
		xSpec.m_xShape.m_fFacetDepthMin = 0.05f;
		xSpec.m_xShape.m_fFacetDepthMax = 0.20f;
		xSpec.m_xShape.m_fBaseFlatten = 0.22f;
		xSpec.m_xShape.m_fNormalSmooth = 0.32f;
		xSpec.m_xShape.m_fWidthMetres = 3.30f;
		xSpec.m_xShape.m_fUVTileMetres = 1.55f;
		break;

	case ROCK_VARIANT_SHARD:
		// An upright standing stone. Angular, tall, narrow.
		xSpec.m_szName = "Rock_Shard";
		xSpec.m_szMaterial = "Granite";
		xSpec.m_uSeed = 33413u;
		xSpec.m_xShape.m_uSubdivisions = 3u;
		xSpec.m_xShape.m_xAxisScale = Zenith_Maths::Vector3(0.55f, 1.65f, 0.62f);
		xSpec.m_xShape.m_fLumpAmplitude = 0.22f;
		xSpec.m_xShape.m_fBumpAmplitude = 0.10f;
		xSpec.m_xShape.m_uFacetPlanes = 9u;
		xSpec.m_xShape.m_fFacetDepthMin = 0.04f;
		xSpec.m_xShape.m_fFacetDepthMax = 0.19f;
		xSpec.m_xShape.m_fBaseFlatten = 0.10f;
		xSpec.m_xShape.m_fNormalSmooth = 0.22f;
		xSpec.m_xShape.m_fWidthMetres = 1.30f;
		xSpec.m_xShape.m_fUVTileMetres = 1.10f;
		break;

	default:
		// Six small stones in ONE mesh, so a scatter places a natural group for the
		// cost of a single instance rather than six.
		xSpec.m_szName = "Rock_Pebbles";
		xSpec.m_szMaterial = "Granite";
		xSpec.m_uSeed = 90211u;
		xSpec.m_uClusterStones = 6u;
		xSpec.m_xShape.m_uSubdivisions = 2u;
		xSpec.m_xShape.m_xAxisScale = Zenith_Maths::Vector3(1.10f, 0.70f, 1.00f);
		xSpec.m_xShape.m_uFacetPlanes = 4u;
		xSpec.m_xShape.m_fNormalSmooth = 0.62f;
		xSpec.m_xShape.m_fWidthMetres = 1.00f;
		xSpec.m_xShape.m_fUVTileMetres = 0.70f;
		break;
	}
	return xSpec;
}

// Build one variant's whole mesh: a single stone, or a golden-angle cluster of
// them so they neither stack nor line up.
void BuildRockVariantMesh(Zenith_MeshAsset& xMesh, const RockVariantSpec& xSpec)
{
	if (xSpec.m_uClusterStones <= 1u)
	{
		AppendRockGeometry(xMesh, xSpec.m_xShape, xSpec.m_uSeed,
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f, 1.0f);
		return;
	}

	Zenith_TerrainNoise::XorShift32 xRng(xSpec.m_uSeed);
	for (u_int uStone = 0; uStone < xSpec.m_uClusterStones; uStone++)
	{
		const float fAngle = static_cast<float>(uStone) * 2.399963f;
		const float fRadius = 0.22f + 0.46f *
			(static_cast<float>(uStone) / static_cast<float>(xSpec.m_uClusterStones - 1u));
		const Zenith_Maths::Vector3 xOffset(cosf(fAngle) * fRadius, 0.0f, sinf(fAngle) * fRadius);
		const float fScale = 0.42f + 0.52f * xRng.NextFloat01();
		AppendRockGeometry(xMesh, xSpec.m_xShape, xSpec.m_uSeed + uStone * 137u, xOffset,
			xRng.NextFloat01() * 6.2831853f, fScale);
	}
}

//=============================================================================
// Export one stone: .zasset (what the instanced-mesh component loads), .zgeom
// (static geometry) and .zmodel (what AddStep_LoadModel / a ModelComponent
// reference).
//=============================================================================
void ExportRock(const std::string& strDir, const char* szName, Zenith_MeshAsset* pxMesh,
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
	xMaterials.PushBack(std::string("engine:Meshes/Rocks/Rock_") + szMaterialName + ZENITH_MATERIAL_EXT);
	pxModel->AddMeshByPath(strAssetPath, xMaterials);
	pxModel->Export((strDir + szName + ZENITH_MODEL_EXT).c_str());

	Zenith_Log(LOG_CATEGORY_ASSET, "  %s: %u verts, %u tris (%s)",
		szName, pxMesh->GetNumVerts(), pxMesh->GetNumIndices() / 3u, szMaterialName);
}

} // namespace

//=============================================================================
// Entry point — called from GenerateTestAssets() at every editor boot.
//=============================================================================
void GenerateProceduralRockAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET,
		"Generating shared Rock assets v%u (4 stone meshes, granite + sandstone PBR sets, POM height + shared detail pair)...",
		uROCK_ASSET_EXPORT_VERSION);

	const std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/Rocks/";
	std::filesystem::create_directories(strOutputDir);

	// --- Textures + materials first: the models below reference them by path. --
	{
		// The shared detail pair FIRST: both stone materials reference it by path.
		GenerateRockDetailTextures(strOutputDir);

		RockTextureParams xGranite;
		xGranite.m_szName = "Granite";
		xGranite.m_xCreviceColour = Zenith_Maths::Vector3(0.205f, 0.202f, 0.198f);
		xGranite.m_xRidgeColour   = Zenith_Maths::Vector3(0.660f, 0.651f, 0.632f);
		xGranite.m_fSpeckle = 1.0f;
		xGranite.m_fBedding = 0.0f;
		xGranite.m_fRoughRidge = 0.44f;
		xGranite.m_fRoughCrevice = 0.95f;
		xGranite.m_xStainColour = Zenith_Maths::Vector3(0.135f, 0.160f, 0.080f);    // lichen
		xGranite.m_fStainAmount = 0.50f;
		xGranite.m_uSeed = 5501u;
		GenerateRockTextureSet(strOutputDir, xGranite);
		GenerateRockMaterial(strOutputDir, "Granite");

		RockTextureParams xSandstone;
		xSandstone.m_szName = "Sandstone";
		xSandstone.m_xCreviceColour = Zenith_Maths::Vector3(0.310f, 0.238f, 0.168f);
		xSandstone.m_xRidgeColour   = Zenith_Maths::Vector3(0.760f, 0.600f, 0.425f);
		xSandstone.m_fSpeckle = 0.35f;                                              // grains, not crystals
		xSandstone.m_fBedding = 0.65f;
		xSandstone.m_fRoughRidge = 0.62f;
		xSandstone.m_fRoughCrevice = 0.97f;
		xSandstone.m_xStainColour = Zenith_Maths::Vector3(0.300f, 0.150f, 0.060f);  // iron oxide
		xSandstone.m_fStainAmount = 0.42f;
		xSandstone.m_uSeed = 8837u;
		GenerateRockTextureSet(strOutputDir, xSandstone);
		GenerateRockMaterial(strOutputDir, "Sandstone");
	}

	// --- The stones themselves, straight off the variant table. --------------
	for (int iVariant = 0; iVariant < ROCK_VARIANT_COUNT; iVariant++)
	{
		const RockVariantSpec xSpec = RockVariantAt(iVariant);
		Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
		pxMesh->Reserve(8192u, 24576u);
		BuildRockVariantMesh(*pxMesh, xSpec);
		ExportRock(strOutputDir, xSpec.m_szName, pxMesh, xSpec.m_szMaterial);
		delete pxMesh;
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "Rock assets generated at: %s", strOutputDir.c_str());
}

#include "Zenith_Tools_RockAssetExport.Tests.inl"

#endif // ZENITH_TOOLS
