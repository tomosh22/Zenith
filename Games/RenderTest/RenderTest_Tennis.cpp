#include "Zenith.h"
#include "RenderTest/RenderTest_Tennis.h"

#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#ifdef ZENITH_TOOLS
// Offline (CPU) texture export. The header lives under the engine /Tools tree,
// which is not on a game's include search path, so reach it with a relative path
// (mirrors Zenith_EditorPanel_ContentBrowser.cpp).
#include "../../Tools/Zenith_Tools_TextureExport.h"
#endif
#include "RenderTest/Components/RenderTest_GameplayState.h"

#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <filesystem>

using namespace RenderTest_Tennis;
using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// Tennis testbed asset production.
//
// Everything the tennis court needs to render (the court/net/tape/racket meshes,
// the ball sphere, the grass+lines court texture, the net mesh texture, and the
// materials that bind them) is baked OFFLINE here into CPU assets on disk:
//
//   .zasset  (Zenith_MeshAsset)   - geometry, no GPU upload
//   .zmodel  (Zenith_ModelAsset)  - bundles a mesh + its material(s) by path
//   .zmtrl   (Zenith_MaterialAsset) - PBR params + texture references by path
//   .ztxtr   (Zenith_TextureAsset)  - raw RGBA8 pixel data
//
// The authored scene (RenderTest.cpp) loads these models and creates the
// court/net/ball/NPC/racket/match entities + colliders itself; this file no
// longer touches the scene or the GPU. The box/quad geometry + the court/net
// pixel buffers are generated the same as the old runtime path — only the OUTPUT
// target moved from a live Flux_MeshGeometry / GPU texture to a disk asset. Two
// deliberate, benign deltas vs the old path: the masked net .ztxtr now gets a
// runtime mip chain when loaded through the asset registry (was single-mip — the
// alpha-tested cords soften slightly at distance, which also reduces shimmer);
// and the racket/net-tape share the testbed vertex-colour material (the planned
// consolidation), so their specular response differs from the old per-mesh mats.
//=============================================================================
namespace
{
	// Session-lifetime owners for the export-time material/model handles, so they
	// outlive the export and are released cleanly at shutdown (mirrors the
	// jetpack/guns export pattern).
	std::vector<MaterialHandle> g_axMaterials;
	std::vector<ModelHandle>    g_axModels;

	bool RT_TennisHasFlag(const char* szFlag)
	{
#ifdef ZENITH_WINDOWS
		for (int i = 1; i < __argc; i++)
			if (std::strcmp(szFlag, __argv[i]) == 0)
				return true;
#else
		(void)szFlag;
#endif
		return false;
	}

	// Optional float override from a "--prefix=<value>" CLI arg (capture/tuning aid).
	float RT_TennisArgFloat(const char* szPrefix, float fDefault)
	{
#ifdef ZENITH_WINDOWS
		const size_t ulLen = std::strlen(szPrefix);
		for (int i = 1; i < __argc; i++)
			if (std::strncmp(__argv[i], szPrefix, ulLen) == 0)
				return static_cast<float>(std::atof(__argv[i] + ulLen));
#else
		(void)szPrefix;
#endif
		return fDefault;
	}

	// --- Deterministic on-disk asset paths -----------------------------------
	// Function-local statics give stable storage whose c_str() is safe to hand to
	// LoadModel (GAME_ASSETS_DIR-relative, like EnsureUnitCubeModelExists).
	const std::string& TennisCourtMeshPath()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Court"  ZENITH_MESH_ASSET_EXT; return s; }
	const std::string& TennisNetMeshPath()     { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Net"    ZENITH_MESH_ASSET_EXT; return s; }
	const std::string& TennisTapeMeshPath()    { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_NetTape" ZENITH_MESH_ASSET_EXT; return s; }
	const std::string& TennisRacketMeshPath()  { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Racket" ZENITH_MESH_ASSET_EXT; return s; }
	const std::string& TennisBallMeshPath()    { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Ball"   ZENITH_MESH_ASSET_EXT; return s; }

	const std::string& TennisCourtModelPathStr()  { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Court"  ZENITH_MODEL_EXT; return s; }
	const std::string& TennisNetModelPathStr()    { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Net"    ZENITH_MODEL_EXT; return s; }
	const std::string& TennisTapeModelPathStr()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_NetTape" ZENITH_MODEL_EXT; return s; }
	const std::string& TennisRacketModelPathStr() { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Racket" ZENITH_MODEL_EXT; return s; }
	const std::string& TennisBallModelPathStr()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Tennis_Ball"   ZENITH_MODEL_EXT; return s; }

	const std::string& TennisCourtMaterialPath() { static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/Tennis_Court" ZENITH_MATERIAL_EXT; return s; }
	const std::string& TennisNetMaterialPath()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/Tennis_Net"   ZENITH_MATERIAL_EXT; return s; }
	const std::string& TennisBallMaterialPath()  { static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/Tennis_Ball"  ZENITH_MATERIAL_EXT; return s; }

	const std::string& TennisCourtTexturePath() { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Court" ZENITH_TEXTURE_EXT; return s; }
	const std::string& TennisCourtNormalPath()  { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Court_Normal" ZENITH_TEXTURE_EXT; return s; }
	const std::string& TennisCourtRMPath()      { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Court_RM"     ZENITH_TEXTURE_EXT; return s; }
	const std::string& TennisCourtAOPath()      { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Court_AO"     ZENITH_TEXTURE_EXT; return s; }
	const std::string& TennisNetTexturePath()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Net"   ZENITH_TEXTURE_EXT; return s; }
	const std::string& TennisBallTexturePath()  { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Ball"  ZENITH_TEXTURE_EXT; return s; }

	// Accumulating triangle-mesh builder. Quads use the same CCW winding as
	// GenerateUnitCube (0-2-1, 1-2-3) so front faces survive back-face culling.
	// p0=bottom-left, p1=bottom-right, p2=top-left, p3=top-right in the face's
	// own frame. Drains into a CPU Zenith_MeshAsset for offline export (no GPU).
	struct GeomBuilder
	{
		std::vector<Vector3> m_xPos, m_xNrm, m_xTan, m_xBit;
		std::vector<Vector2> m_xUV;
		std::vector<Vector4> m_xCol;
		std::vector<uint32_t> m_xIdx;

		void AddQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
			const Vector3& xNormal,
			const Vector2& uv0, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3,
			const Vector4& xColor)
		{
			const uint32_t uBase = static_cast<uint32_t>(m_xPos.size());
			Vector3 xTangent = p1 - p0;
			const float fLen = glm::length(xTangent);
			xTangent = (fLen > 1e-6f) ? (xTangent / fLen) : Vector3(1.0f, 0.0f, 0.0f);
			const Vector3 xBitangent = glm::cross(xNormal, xTangent);

			auto Push = [&](const Vector3& p, const Vector2& uv)
			{
				m_xPos.push_back(p);
				m_xNrm.push_back(xNormal);
				m_xTan.push_back(xTangent);
				m_xBit.push_back(xBitangent);
				m_xUV.push_back(uv);
				m_xCol.push_back(xColor);
			};
			Push(p0, uv0); Push(p1, uv1); Push(p2, uv2); Push(p3, uv3);

			m_xIdx.push_back(uBase + 0); m_xIdx.push_back(uBase + 2); m_xIdx.push_back(uBase + 1);
			m_xIdx.push_back(uBase + 1); m_xIdx.push_back(uBase + 2); m_xIdx.push_back(uBase + 3);
		}

		// Drain the accumulated vertices/indices into a CPU Zenith_MeshAsset for
		// offline export (no GPU upload). AddVertex doesn't take a bitangent, so the
		// analytic bitangent is pushed in parallel to keep all six arrays the same
		// length (matches Zenith_MeshAsset::GenerateUnitSphere / the jetpack export).
		void BuildAsset(Zenith_MeshAsset& xOut) const
		{
			xOut.Reset();
			const uint32_t uNV = static_cast<uint32_t>(m_xPos.size());
			const uint32_t uNI = static_cast<uint32_t>(m_xIdx.size());
			xOut.Reserve(uNV, uNI);
			for (uint32_t u = 0; u < uNV; ++u)
			{
				xOut.AddVertex(m_xPos[u], m_xNrm[u], m_xUV[u], m_xTan[u], m_xCol[u]);
				xOut.m_xBitangents.PushBack(m_xBit[u]);
			}
			for (uint32_t u = 0; u + 3 <= uNI; u += 3)
			{
				xOut.AddTriangle(m_xIdx[u], m_xIdx[u + 1], m_xIdx[u + 2]);
			}
			xOut.ComputeBounds();
		}
	};

	// --- Procedural textures (CPU pixel buffers) -----------------------------

	//=========================================================================
	// The court surface.
	//
	// U maps to court width (X), V to court length (Z); the painted court is
	// inset by the grass apron. Everything below is a PURE function of (u, v),
	// which is what lets the four maps be generated in four separate passes and
	// still describe the same surface -- a paint edge in the colour map is the
	// same paint edge in the normal, RM and AO maps by construction, not by
	// coincidence.
	//
	// Three things separate this from the flat green-with-lines it replaces:
	//
	// 1. ANTIALIASED LINES. A line is filled by analytic BOX COVERAGE of the
	//    texel against the line rect, not by a nearest-texel rect fill. At the
	//    old 384x832 a court line was ~4 texels wide and its edge was a hard
	//    step, which crawls under any camera motion and cannot be fixed
	//    downstream -- TAA sharpens it, the mip chain smears it. Coverage
	//    weighting is what makes the same line hold still.
	//
	// 2. MOW STRIPES. Real grass courts are cut in alternating directions and
	//    the bands are the single strongest cue that a surface is mown grass
	//    rather than green paint. +/-6% value, ~1.5 m bands running along the
	//    court's length (so they alternate across U).
	//
	// 3. WEAR. Grass dies where players stand: behind the baselines and,
	//    less so, in the service boxes. Worn grass is PALER and patchier, not
	//    just darker, so the mask drives a straw tint as well as a value lift.
	//=========================================================================
	constexpr uint32_t uCOURT_TEX_W = 1024;
	constexpr uint32_t uCOURT_TEX_H = 2048;
	// Half the colour resolution: neither map carries detail the colour map
	// does not, and RM/AO are the two the eye is least able to localise.
	constexpr uint32_t uCOURT_DATA_W = 512;
	constexpr uint32_t uCOURT_DATA_H = 1024;

	// Normalised court geometry over the slab footprint. ONE definition, read
	// by every map generator and by the units -- a second copy drifts the first
	// time the apron changes.
	struct RT_CourtLayout
	{
		float m_fApronU = 0.0f;
		float m_fApronV = 0.0f;
		float m_fLeftDoubles = 0.0f;
		float m_fRightDoubles = 0.0f;
		float m_fLeftSingles = 0.0f;
		float m_fRightSingles = 0.0f;
		float m_fNearBase = 0.0f;
		float m_fFarBase = 0.0f;
		float m_fNearService = 0.0f;
		float m_fFarService = 0.0f;
		float m_fLineHalfU = 0.0f;
		float m_fLineHalfV = 0.0f;
	};

	RT_CourtLayout RT_MakeCourtLayout()
	{
		RT_CourtLayout x;
		x.m_fApronU = fAPRON / (2.0f * fSLAB_HALF_WIDTH);
		x.m_fApronV = fAPRON / (2.0f * fSLAB_HALF_LENGTH);
		const float fSinglesInsetU = 1.37f / (2.0f * fSLAB_HALF_WIDTH);
		const float fServiceV = fSERVICE_LINE_OFFSET / (2.0f * fSLAB_HALF_LENGTH);
		// Line half-thickness in U/V (~5 cm physical, as before).
		x.m_fLineHalfU = 0.06f / (2.0f * fSLAB_HALF_WIDTH);
		x.m_fLineHalfV = 0.06f / (2.0f * fSLAB_HALF_LENGTH);
		x.m_fLeftDoubles = x.m_fApronU;
		x.m_fRightDoubles = 1.0f - x.m_fApronU;
		x.m_fLeftSingles = x.m_fApronU + fSinglesInsetU;
		x.m_fRightSingles = 1.0f - x.m_fApronU - fSinglesInsetU;
		x.m_fNearBase = x.m_fApronV;
		x.m_fFarBase = 1.0f - x.m_fApronV;
		x.m_fNearService = 0.5f - fServiceV;
		x.m_fFarService = 0.5f + fServiceV;
		return x;
	}

	struct RT_CourtRect
	{
		float m_fU0, m_fV0, m_fU1, m_fV1;
	};

	// Every painted rect on the court, in one list. Coverage of the union is
	// the max over the list: the rects only ever overlap where two lines cross,
	// and a crossing is solid paint under either.
	void RT_BuildCourtLineRects(const RT_CourtLayout& xL, std::vector<RT_CourtRect>& xOut)
	{
		const float fLwU = xL.m_fLineHalfU;
		const float fLwV = xL.m_fLineHalfV;
		xOut.clear();
		// Sidelines (full court length).
		xOut.push_back({ xL.m_fLeftDoubles - fLwU,  xL.m_fNearBase - fLwV, xL.m_fLeftDoubles + fLwU,  xL.m_fFarBase + fLwV });
		xOut.push_back({ xL.m_fRightDoubles - fLwU, xL.m_fNearBase - fLwV, xL.m_fRightDoubles + fLwU, xL.m_fFarBase + fLwV });
		xOut.push_back({ xL.m_fLeftSingles - fLwU,  xL.m_fNearBase - fLwV, xL.m_fLeftSingles + fLwU,  xL.m_fFarBase + fLwV });
		xOut.push_back({ xL.m_fRightSingles - fLwU, xL.m_fNearBase - fLwV, xL.m_fRightSingles + fLwU, xL.m_fFarBase + fLwV });
		// Baselines (full doubles width).
		xOut.push_back({ xL.m_fLeftDoubles, xL.m_fNearBase - fLwV, xL.m_fRightDoubles, xL.m_fNearBase + fLwV });
		xOut.push_back({ xL.m_fLeftDoubles, xL.m_fFarBase - fLwV,  xL.m_fRightDoubles, xL.m_fFarBase + fLwV });
		// Service lines (singles width).
		xOut.push_back({ xL.m_fLeftSingles, xL.m_fNearService - fLwV, xL.m_fRightSingles, xL.m_fNearService + fLwV });
		xOut.push_back({ xL.m_fLeftSingles, xL.m_fFarService - fLwV,  xL.m_fRightSingles, xL.m_fFarService + fLwV });
		// Centre service line (between the two service lines).
		xOut.push_back({ 0.5f - fLwU, xL.m_fNearService, 0.5f + fLwU, xL.m_fFarService });
		// Centre marks on the baselines.
		xOut.push_back({ 0.5f - fLwU, xL.m_fNearBase, 0.5f + fLwU, xL.m_fNearBase + 6.0f * fLwV });
		xOut.push_back({ 0.5f - fLwU, xL.m_fFarBase - 6.0f * fLwV, 0.5f + fLwU, xL.m_fFarBase });
	}

	// Fraction of ONE TEXEL covered by the rect. This is the antialiasing: a
	// texel straddling a paint edge gets the exact area fraction, so the edge
	// carries intermediate values instead of stepping between two constants.
	float RT_RectTexelCoverage(const RT_CourtRect& xR,
		float fTU0, float fTV0, float fTU1, float fTV1)
	{
		const float fOverlapU = std::max(0.0f, std::min(xR.m_fU1, fTU1) - std::max(xR.m_fU0, fTU0));
		const float fOverlapV = std::max(0.0f, std::min(xR.m_fV1, fTV1) - std::max(xR.m_fV0, fTV0));
		const float fTexelU = std::max(fTU1 - fTU0, 1e-9f);
		const float fTexelV = std::max(fTV1 - fTV0, 1e-9f);
		return (fOverlapU / fTexelU) * (fOverlapV / fTexelV);
	}

	// Paint coverage [0,1] of the texel (ix, iy) in a map of (uW x uH).
	float RT_CourtPaintCoverage(const std::vector<RT_CourtRect>& xRects,
		uint32_t uW, uint32_t uH, uint32_t x, uint32_t y)
	{
		const float fTU0 = static_cast<float>(x) / uW;
		const float fTU1 = static_cast<float>(x + 1) / uW;
		const float fTV0 = static_cast<float>(y) / uH;
		const float fTV1 = static_cast<float>(y + 1) / uH;
		float fCoverage = 0.0f;
		for (const RT_CourtRect& xR : xRects)
		{
			fCoverage = std::max(fCoverage, RT_RectTexelCoverage(xR, fTU0, fTV0, fTU1, fTV1));
		}
		return std::min(fCoverage, 1.0f);
	}

	float RT_CourtHash01(uint32_t x, uint32_t y, uint32_t uSeed)
	{
		uint32_t uHash = (x * 374761393u + y * 668265263u + uSeed * 2246822519u);
		uHash = (uHash ^ (uHash >> 13)) * 1274126177u;
		uHash ^= uHash >> 16;
		return static_cast<float>(uHash & 0x00FFFFFFu) * (1.0f / 16777216.0f);
	}

	float RT_CourtSmoothStep(float fEdge0, float fEdge1, float fX)
	{
		const float fT = glm::clamp((fX - fEdge0) / (fEdge1 - fEdge0), 0.0f, 1.0f);
		return fT * fT * (3.0f - 2.0f * fT);
	}

	// Smooth value noise on a lattice of uCells x uCells over the whole map.
	// Used for the wear patches and the tuft field -- a plain per-texel hash is
	// grain, and grain alone never reads as clumped grass.
	float RT_CourtValueNoise(float fU, float fV, uint32_t uCellsU, uint32_t uCellsV, uint32_t uSeed)
	{
		const float fX = fU * uCellsU;
		const float fY = fV * uCellsV;
		const uint32_t uX0 = static_cast<uint32_t>(fX);
		const uint32_t uY0 = static_cast<uint32_t>(fY);
		const float fTX = RT_CourtSmoothStep(0.0f, 1.0f, fX - static_cast<float>(uX0));
		const float fTY = RT_CourtSmoothStep(0.0f, 1.0f, fY - static_cast<float>(uY0));
		const float fV00 = RT_CourtHash01(uX0, uY0, uSeed);
		const float fV10 = RT_CourtHash01(uX0 + 1u, uY0, uSeed);
		const float fV01 = RT_CourtHash01(uX0, uY0 + 1u, uSeed);
		const float fV11 = RT_CourtHash01(uX0 + 1u, uY0 + 1u, uSeed);
		const float fTop = fV00 + (fV10 - fV00) * fTX;
		const float fBottom = fV01 + (fV11 - fV01) * fTX;
		return fTop + (fBottom - fTop) * fTY;
	}

	// The mow-stripe multiplier: +/-6% value in ~1.5 m bands running ALONG the
	// court (so they alternate across U). A cosine rather than a square wave --
	// a mower leaves a soft boundary, and a hard one would alias.
	float RT_CourtMowStripe(float fU)
	{
		constexpr float fBAND_METRES = 1.5f;
		const float fSlabWidth = 2.0f * fSLAB_HALF_WIDTH;
		const float fBands = fSlabWidth / fBAND_METRES;
		const float fPhase = std::cos(fU * fBands * 6.28318530718f);
		return 1.0f + 0.06f * fPhase;
	}

	// How worn the turf is at (u, v): 1 = bare, patchy, pale. Heaviest in the
	// two baseline bands where a player stands and pivots all match, lighter in
	// the service boxes where they only run through.
	float RT_CourtWear(const RT_CourtLayout& xL, float fU, float fV)
	{
		// Baseline bands: a metre or so either side of each baseline, and only
		// across the width a player actually covers.
		const float fBandV = 1.30f / (2.0f * fSLAB_HALF_LENGTH);
		const float fNear = 1.0f - RT_CourtSmoothStep(0.0f, fBandV, std::fabs(fV - xL.m_fNearBase));
		const float fFar = 1.0f - RT_CourtSmoothStep(0.0f, fBandV, std::fabs(fV - xL.m_fFarBase));
		float fBaseline = std::max(fNear, fFar);
		// Players stand between the singles lines, mostly near the centre.
		const float fAcross = 1.0f - RT_CourtSmoothStep(0.18f, 0.46f, std::fabs(fU - 0.5f));
		fBaseline *= fAcross;

		// Service boxes: run through, not stood in -- a third of the wear.
		float fService = 0.0f;
		if (fV > xL.m_fNearService && fV < xL.m_fFarService &&
			fU > xL.m_fLeftSingles && fU < xL.m_fRightSingles)
		{
			fService = 0.32f;
		}

		// Patchiness: wear is never a clean band. Two lattice scales so the
		// patches have patches.
		//
		// ★ AVERAGING TWO OCTAVES CONCENTRATES THE RESULT ABOUT 0.5. Measured
		// along a baseline, the raw blend spanned only 0.08 — which renders as a
		// uniform painted band, the exact thing wear is here to avoid. The
		// contrast curve re-spreads that concentrated middle over the full range
		// before it scales the band.
		// The FINER octave carries most of the weight: along a single baseline the
		// coarse 14x26 lattice barely moves (measured spread 0.04 raw), and it is
		// variation ALONG the line that stops the band reading as paint.
		const float fPatchRaw = RT_CourtValueNoise(fU, fV, 14u, 26u, 771u) * 0.45f +
			RT_CourtValueNoise(fU, fV, 47u, 88u, 913u) * 0.55f;
		// Window measured, not assumed: a 0.26-wide window turns that 0.04 of raw
		// spread into ~0.15 of wear, which is the difference between "patchy" and
		// "a painted stripe".
		const float fPatch = RT_CourtSmoothStep(0.38f, 0.64f, fPatchRaw);
		const float fWear = std::max(fBaseline, fService) * (0.30f + 1.05f * fPatch);
		return glm::clamp(fWear, 0.0f, 1.0f);
	}

	// The surface height the normal and AO maps are built from: fine grass
	// tufts, flattened where the turf is worn, plus the slight relief of the
	// paint itself sitting on top of the blades.
	float RT_CourtSurfaceHeight(const RT_CourtLayout& xL, uint32_t uW, uint32_t uH,
		uint32_t x, uint32_t y, const std::vector<RT_CourtRect>& xRects)
	{
		const float fU = (static_cast<float>(x) + 0.5f) / uW;
		const float fV = (static_cast<float>(y) + 0.5f) / uH;
		// Tufts at roughly 3 cm and 9 cm -- the two scales you actually see on
		// mown turf from a standing camera.
		//
		// The lattice sizes are ABSOLUTE, not derived from the map resolution:
		// the normal map is 1024x2048 and the RM/AO pair is half that, and a
		// resolution-relative lattice would make them describe two DIFFERENT
		// surfaces -- occlusion that does not sit in the hollows its own normal
		// map carves.
		//
		// They are also deliberately COARSER than the texel grid: a lattice at
		// 2 texels per cell sits on Nyquist, which aliases into a shimmering
		// normal map rather than resolving as tufts.
		constexpr uint32_t uTUFT_FINE_U = 256u;    // ~6.2 cm across a 15.97 m slab (4 texels/cell)
		constexpr uint32_t uTUFT_FINE_V = 461u;    // ~6.2 cm across a 28.77 m slab
		constexpr uint32_t uTUFT_COARSE_U = 85u;   // ~19 cm
		constexpr uint32_t uTUFT_COARSE_V = 153u;
		const float fTuftFine = RT_CourtValueNoise(fU, fV, uTUFT_FINE_U, uTUFT_FINE_V, 401u);
		const float fTuftCoarse = RT_CourtValueNoise(fU, fV, uTUFT_COARSE_U, uTUFT_COARSE_V, 577u);
		float fHeight = fTuftFine * 0.62f + fTuftCoarse * 0.38f;

		// Worn turf is flatter: fewer blades, pressed down.
		const float fWear = RT_CourtWear(xL, fU, fV);
		fHeight *= 1.0f - 0.55f * fWear;

		// Paint fills between the blades and sits slightly proud of them.
		const float fPaint = RT_CourtPaintCoverage(xRects, uW, uH, x, y);
		fHeight = fHeight * (1.0f - 0.72f * fPaint) + fPaint * 0.62f;
		return glm::clamp(fHeight, 0.0f, 1.0f);
	}

	// Court base colour: mown, worn grass with antialiased painted lines.
	// Returns the RGBA8 pixel buffer (uW/uH set) so the caller can export it.
	std::vector<uint8_t> RT_MakeCourtTexture(uint32_t& uWOut, uint32_t& uHOut)
	{
		constexpr uint32_t uW = uCOURT_TEX_W;
		constexpr uint32_t uH = uCOURT_TEX_H;
		const RT_CourtLayout xL = RT_MakeCourtLayout();
		std::vector<RT_CourtRect> xRects;
		RT_BuildCourtLineRects(xL, xRects);

		std::vector<uint8_t> xPx(static_cast<size_t>(uW) * uH * 4);
		for (uint32_t y = 0; y < uH; y++)
		{
			const float fV = (static_cast<float>(y) + 0.5f) / uH;
			for (uint32_t x = 0; x < uW; x++)
			{
				const float fU = (static_cast<float>(x) + 0.5f) / uW;

				// Healthy turf -> worn straw.
				const float fWear = RT_CourtWear(xL, fU, fV);
				float fR = 0.149f + (0.454f - 0.149f) * fWear;
				float fG = 0.431f + (0.443f - 0.431f) * fWear;
				float fB = 0.180f + (0.243f - 0.180f) * fWear;

				// Mow stripes, then per-texel grain (the old hash noise, kept:
				// it is what stops flat turf reading as a solid fill).
				const float fMow = RT_CourtMowStripe(fU);
				const float fGrain = (RT_CourtHash01(x, y, 0u) - 0.5f) * 0.12f;
				// Clump-scale colour variation, so the grain is not the only
				// structure in the green.
				const float fClump = (RT_CourtValueNoise(fU, fV, 60u, 110u, 233u) - 0.5f) * 0.10f;
				fR = fR * fMow + fGrain * 0.5f + fClump * 0.5f;
				fG = fG * fMow + fGrain + fClump;
				fB = fB * fMow + fGrain * 0.5f + fClump * 0.4f;

				// Paint, COVERAGE-WEIGHTED. Court paint is not pure white and it
				// picks up the turf under it, so it gets its own faint grain.
				const float fPaint = RT_CourtPaintCoverage(xRects, uW, uH, x, y);
				if (fPaint > 0.0f)
				{
					const float fPaintGrain = (RT_CourtHash01(x, y, 91u) - 0.5f) * 0.05f;
					const float fPaintV = 0.918f + fPaintGrain;
					fR = fR + (fPaintV - fR) * fPaint;
					fG = fG + (fPaintV - fG) * fPaint;
					fB = fB + (fPaintV * 0.985f - fB) * fPaint;
				}

				uint8_t* p = &xPx[(static_cast<size_t>(y) * uW + x) * 4];
				p[0] = static_cast<uint8_t>(glm::clamp(fR, 0.0f, 1.0f) * 255.0f);
				p[1] = static_cast<uint8_t>(glm::clamp(fG, 0.0f, 1.0f) * 255.0f);
				p[2] = static_cast<uint8_t>(glm::clamp(fB, 0.0f, 1.0f) * 255.0f);
				p[3] = 255;
			}
		}

		uWOut = uW;
		uHOut = uH;
		return xPx;
	}

	// Court normal map: fine grass tufts + the slight relief of the paint.
	// RGBA8 for the BC5 writer (which keeps R+G; the shader rebuilds Z).
	// CLAMPED differences -- the court texture is mapped 1:1 over the slab and
	// does not tile, so a wrapped difference would join the two touchlines.
	std::vector<uint8_t> RT_MakeCourtNormalTexture(uint32_t& uWOut, uint32_t& uHOut)
	{
		constexpr uint32_t uW = uCOURT_TEX_W;
		constexpr uint32_t uH = uCOURT_TEX_H;
		const RT_CourtLayout xL = RT_MakeCourtLayout();
		std::vector<RT_CourtRect> xRects;
		RT_BuildCourtLineRects(xL, xRects);

		std::vector<float> xHeight(static_cast<size_t>(uW) * uH);
		for (uint32_t y = 0; y < uH; y++)
		{
			for (uint32_t x = 0; x < uW; x++)
			{
				xHeight[static_cast<size_t>(y) * uW + x] =
					RT_CourtSurfaceHeight(xL, uW, uH, x, y, xRects);
			}
		}

		std::vector<uint8_t> xPx(static_cast<size_t>(uW) * uH * 4);
		for (uint32_t y = 0; y < uH; y++)
		{
			for (uint32_t x = 0; x < uW; x++)
			{
				const uint32_t xp = std::min(x + 1u, uW - 1u);
				const uint32_t xm = (x > 0u) ? (x - 1u) : 0u;
				const uint32_t yp = std::min(y + 1u, uH - 1u);
				const uint32_t ym = (y > 0u) ? (y - 1u) : 0u;
				const float fDX = (xHeight[static_cast<size_t>(y) * uW + xp] -
					xHeight[static_cast<size_t>(y) * uW + xm]) * 1.6f;
				const float fDY = (xHeight[static_cast<size_t>(yp) * uW + x] -
					xHeight[static_cast<size_t>(ym) * uW + x]) * 1.6f;
				const Vector3 xN = glm::normalize(Vector3(-fDX, -fDY, 1.0f));
				uint8_t* p = &xPx[(static_cast<size_t>(y) * uW + x) * 4];
				p[0] = static_cast<uint8_t>((xN.x * 0.5f + 0.5f) * 255.0f);
				p[1] = static_cast<uint8_t>((xN.y * 0.5f + 0.5f) * 255.0f);
				p[2] = static_cast<uint8_t>((xN.z * 0.5f + 0.5f) * 255.0f);
				p[3] = 255;
			}
		}
		uWOut = uW;
		uHOut = uH;
		return xPx;
	}

	// Court RM (glTF layout: G = roughness, B = metallic) + AO, generated
	// together because both are functions of the same paint/wear fields.
	// Paint is a sealed, slightly polished film; turf is not. Getting that one
	// difference right is most of what makes a wet-looking court read as grass
	// with lines on it rather than as a painted board.
	void RT_MakeCourtDataTextures(std::vector<uint8_t>& xRMOut, std::vector<uint8_t>& xAOOut,
		uint32_t& uWOut, uint32_t& uHOut)
	{
		constexpr uint32_t uW = uCOURT_DATA_W;
		constexpr uint32_t uH = uCOURT_DATA_H;
		const RT_CourtLayout xL = RT_MakeCourtLayout();
		std::vector<RT_CourtRect> xRects;
		RT_BuildCourtLineRects(xL, xRects);

		xRMOut.assign(static_cast<size_t>(uW) * uH * 4, 0);
		xAOOut.assign(static_cast<size_t>(uW) * uH * 4, 0);
		for (uint32_t y = 0; y < uH; y++)
		{
			const float fV = (static_cast<float>(y) + 0.5f) / uH;
			for (uint32_t x = 0; x < uW; x++)
			{
				const float fU = (static_cast<float>(x) + 0.5f) / uW;
				const float fPaint = RT_CourtPaintCoverage(xRects, uW, uH, x, y);
				const float fWear = RT_CourtWear(xL, fU, fV);
				const float fHeight = RT_CourtSurfaceHeight(xL, uW, uH, x, y, xRects);

				// Turf is very rough and dry turf is rougher still; the paint
				// film is markedly smoother than either.
				const float fGrassRough = 0.93f + 0.04f * fWear;
				const float fRough = glm::clamp(fGrassRough + (0.52f - fGrassRough) * fPaint, 0.05f, 1.0f);
				uint8_t* pRM = &xRMOut[(static_cast<size_t>(y) * uW + x) * 4];
				pRM[0] = 0;
				pRM[1] = static_cast<uint8_t>(fRough * 255.0f);
				pRM[2] = 0;   // grass and line paint are both dielectric
				pRM[3] = 255;

				// Occlusion between the blades: deepest in tall turf, released
				// where the surface is worn flat or sealed under paint.
				float fAO = 1.0f - (1.0f - fHeight) * 0.26f;
				fAO += 0.05f * fWear;
				fAO = fAO + (0.99f - fAO) * fPaint;
				const uint8_t ucAO = static_cast<uint8_t>(glm::clamp(fAO, 0.0f, 1.0f) * 255.0f);
				uint8_t* pAO = &xAOOut[(static_cast<size_t>(y) * uW + x) * 4];
				pAO[0] = ucAO;
				pAO[1] = ucAO;
				pAO[2] = ucAO;
				pAO[3] = 255;
			}
		}
		uWOut = uW;
		uHOut = uH;
	}

	// Net texture: a coarse mesh — dark cord on the grid lines (opaque), holes
	// transparent. Tiled across the net panel by the material UV tiling. Returns
	// the RGBA8 pixel buffer (uS square) so the caller can export it to disk.
	std::vector<uint8_t> RT_MakeNetTexture(uint32_t& uSizeOut)
	{
		constexpr uint32_t uS = 64;
		constexpr uint32_t uCell = 8;     // 8 cells across the texture
		constexpr uint32_t uLine = 2;     // cord thickness (px)
		std::vector<uint8_t> xPx(static_cast<size_t>(uS) * uS * 4);
		for (uint32_t y = 0; y < uS; y++)
		{
			for (uint32_t x = 0; x < uS; x++)
			{
				const bool bCord = (x % uCell) < uLine || (y % uCell) < uLine;
				uint8_t* p = &xPx[(static_cast<size_t>(y) * uS + x) * 4];
				p[0] = 30; p[1] = 32; p[2] = 34;
				p[3] = bCord ? 255 : 0;
			}
		}
		uSizeOut = uS;
		return xPx;
	}

	// Tennis-ball felt with the classic curved white seam. Equirectangular so the
	// unit sphere's lat/long UVs wrap it; the seam + felt shading make the ball's
	// SPIN visible as it rotates (a flat-yellow analytic sphere shows no rotation).
	// Returns the RGBA8 buffer (W x H) so the caller exports it to .ztxtr.
	std::vector<uint8_t> RT_MakeBallTexture(uint32_t& uWOut, uint32_t& uHOut)
	{
		constexpr uint32_t uW = 256, uH = 256;
		constexpr float fTwoPi = 6.28318530718f;
		std::vector<uint8_t> xPx(static_cast<size_t>(uW) * uH * 4);
		for (uint32_t y = 0; y < uH; y++)
		{
			const float fV = static_cast<float>(y) / static_cast<float>(uH - 1);   // latitude 0..1
			for (uint32_t x = 0; x < uW; x++)
			{
				const float fU = static_cast<float>(x) / static_cast<float>(uW - 1);   // longitude 0..1
				// Two mirrored sinusoids approximate the figure-eight tennis seam.
				const float fSeamA = 0.5f + 0.20f * std::sin(fU * fTwoPi * 2.0f);
				const float fSeamB = 0.5f - 0.20f * std::sin(fU * fTwoPi * 2.0f);
				const float fDist = std::min(std::fabs(fV - fSeamA), std::fabs(fV - fSeamB));

				// Base tennis felt (slightly shaded by longitude so curvature reads).
				const float fShade = 0.92f + 0.08f * std::sin(fU * fTwoPi);
				uint8_t r = static_cast<uint8_t>(199.0f * fShade);
				uint8_t g = static_cast<uint8_t>(224.0f * fShade);
				uint8_t b = static_cast<uint8_t>(41.0f  * fShade);
				if (fDist < 0.018f)        { r = 240; g = 240; b = 235; }   // white seam line
				else if (fDist < 0.045f)   { r = static_cast<uint8_t>(r * 0.7f); g = static_cast<uint8_t>(g * 0.7f); b = static_cast<uint8_t>(b * 0.7f); }   // darker felt either side

				uint8_t* p = &xPx[(static_cast<size_t>(y) * uW + x) * 4];
				p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
			}
		}
		uWOut = uW;
		uHOut = uH;
		return xPx;
	}

	// --- Court + net geometry (unit meshes; scaled by the entity transform so
	//     OBB colliders, which read transform scale, match the visual) --------

	// Unit box [-0.5,0.5]^3. Top (+Y) face carries the full court texture; the
	// other five faces sample a fixed grass pixel so the slab edges read as grass.
	void RT_BuildCourtSlab(Zenith_MeshAsset& xOut)
	{
		GeomBuilder xB;
		const Vector4 xWhite(1.0f);
		const Vector2 xGrass(0.02f, 0.02f);   // a plain-grass texel for the non-top faces
		const float h = 0.5f;

		// Top (+Y): U->+X, V->-Z, full [0,1] court texture (matches GenerateUnitCube top).
		xB.AddQuad({ -h, h, h }, { h, h, h }, { -h, h, -h }, { h, h, -h }, { 0, 1, 0 },
			{ 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, xWhite);
		// Bottom (-Y).
		xB.AddQuad({ -h, -h, -h }, { h, -h, -h }, { -h, -h, h }, { h, -h, h }, { 0, -1, 0 },
			xGrass, xGrass, xGrass, xGrass, xWhite);
		// +Z / -Z / +X / -X (all grass).
		xB.AddQuad({ -h, -h, h }, { h, -h, h }, { -h, h, h }, { h, h, h }, { 0, 0, 1 },
			xGrass, xGrass, xGrass, xGrass, xWhite);
		xB.AddQuad({ h, -h, -h }, { -h, -h, -h }, { h, h, -h }, { -h, h, -h }, { 0, 0, -1 },
			xGrass, xGrass, xGrass, xGrass, xWhite);
		xB.AddQuad({ h, -h, h }, { h, -h, -h }, { h, h, h }, { h, h, -h }, { 1, 0, 0 },
			xGrass, xGrass, xGrass, xGrass, xWhite);
		xB.AddQuad({ -h, -h, -h }, { -h, -h, h }, { -h, h, -h }, { -h, h, h }, { -1, 0, 0 },
			xGrass, xGrass, xGrass, xGrass, xWhite);
		xB.BuildAsset(xOut);
	}

	// Unit quad in the XY plane ([-0.5,0.5] x [-0.5,0.5], Z=0), normal +Z. Scaled
	// to the net dimensions; the two-sided material makes the back visible.
	void RT_BuildNetQuad(Zenith_MeshAsset& xOut)
	{
		GeomBuilder xB;
		const float h = 0.5f;
		xB.AddQuad({ -h, -h, 0.0f }, { h, -h, 0.0f }, { -h, h, 0.0f }, { h, h, 0.0f }, { 0, 0, 1 },
			{ 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 0 }, Vector4(1.0f));
		xB.BuildAsset(xOut);
	}

	// Add an axis-aligned box [xMin,xMax] to a builder with a flat vertex colour
	// (UVs unused — the racket/tape are vertex-coloured, not textured).
	void RT_AddBox(GeomBuilder& xB, const Vector3& xMin, const Vector3& xMax, const Vector4& xColor)
	{
		const Vector2 z(0.0f, 0.0f);
		const float x0 = xMin.x, y0 = xMin.y, z0 = xMin.z, x1 = xMax.x, y1 = xMax.y, z1 = xMax.z;
		// Faces mirror GenerateUnitCube's winding (BL,BR,TL,TR + normal).
		xB.AddQuad({ x0,y0,z1 }, { x1,y0,z1 }, { x0,y1,z1 }, { x1,y1,z1 }, { 0,0,1 }, z, z, z, z, xColor);   // +Z
		xB.AddQuad({ x1,y0,z0 }, { x0,y0,z0 }, { x1,y1,z0 }, { x0,y1,z0 }, { 0,0,-1 }, z, z, z, z, xColor);   // -Z
		xB.AddQuad({ x0,y1,z1 }, { x1,y1,z1 }, { x0,y1,z0 }, { x1,y1,z0 }, { 0,1,0 }, z, z, z, z, xColor);   // +Y
		xB.AddQuad({ x0,y0,z0 }, { x1,y0,z0 }, { x0,y0,z1 }, { x1,y0,z1 }, { 0,-1,0 }, z, z, z, z, xColor);   // -Y
		xB.AddQuad({ x1,y0,z1 }, { x1,y0,z0 }, { x1,y1,z1 }, { x1,y1,z0 }, { 1,0,0 }, z, z, z, z, xColor);   // +X
		xB.AddQuad({ x0,y0,z0 }, { x0,y0,z1 }, { x0,y1,z0 }, { x0,y1,z1 }, { -1,0,0 }, z, z, z, z, xColor);   // -X
	}

	// White net tape along the top edge (the iconic tennis-net band). Opaque,
	// visual only — built at real size, centred (the entity transform places it
	// at the net top). Vertex-coloured white.
	void RT_BuildNetTape(Zenith_MeshAsset& xOut)
	{
		GeomBuilder xB;
		RT_AddBox(xB, Vector3(-fNET_HALF_WIDTH, -0.03f, -0.03f), Vector3(fNET_HALF_WIDTH, 0.03f, 0.03f),
			Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		xB.BuildAsset(xOut);
	}

	// Tennis racket: a dark grip/handle + a light oval string-bed head, built in
	// its own local frame with the grip at the origin (+Y = up the racket). One
	// mesh, two-tone via vertex colour (material base = white). Attached to the
	// hand bone by Zenith_AttachmentComponent (authoring).
	void RT_BuildRacket(Zenith_MeshAsset& xOut)
	{
		GeomBuilder xB;
		const Vector4 xFrame(0.12f, 0.12f, 0.14f, 1.0f);   // dark grip/frame
		const Vector4 xString(0.85f, 0.85f, 0.80f, 1.0f);  // light string bed
		// Handle: a thin square column from the grip up to the throat.
		RT_AddBox(xB, { -0.018f, 0.0f, -0.018f }, { 0.018f, 0.26f, 0.018f }, xFrame);
		// Head frame: a flat slab (the oval is approximated by a rounded-ish box).
		RT_AddBox(xB, { -0.135f, 0.26f, -0.012f }, { 0.135f, 0.30f, 0.012f }, xFrame);   // bottom rim
		RT_AddBox(xB, { -0.135f, 0.54f, -0.012f }, { 0.135f, 0.58f, 0.012f }, xFrame);   // top rim
		RT_AddBox(xB, { -0.135f, 0.30f, -0.012f }, { -0.105f, 0.54f, 0.012f }, xFrame);  // left rim
		RT_AddBox(xB, { 0.105f, 0.30f, -0.012f }, { 0.135f, 0.54f, 0.012f }, xFrame);   // right rim
		// String bed: a thin light panel inside the frame.
		RT_AddBox(xB, { -0.105f, 0.30f, -0.004f }, { 0.105f, 0.54f, 0.004f }, xString);
		xB.BuildAsset(xOut);
	}

#ifdef ZENITH_TOOLS
	// Create + register a material handle (kept alive for the session in
	// g_axMaterials), name it, and return it for property setup + SaveToFile.
	Zenith_MaterialAsset* RT_NewMaterial(const char* szName)
	{
		MaterialHandle xHandle;
		xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		g_axMaterials.push_back(xHandle);
		Zenith_MaterialAsset* pxMat = xHandle.GetDirect();
		pxMat->SetName(szName);
		return pxMat;
	}

	// Export a CPU mesh asset + a bundling .zmodel referencing the given material
	// paths. Overwrites every tools run so geometry/material edits propagate (the
	// EnsureStickFigureModelExists generation policy). The model handle is tracked
	// in g_axModels for clean release at shutdown.
	void RT_ExportMeshModel(const char* szModelName, void (*pfnBuild)(Zenith_MeshAsset&),
		const std::string& strMeshPath, const std::string& strModelPath,
		const Zenith_Vector<std::string>& xMaterialPaths)
	{
		std::filesystem::create_directories(std::filesystem::path(strMeshPath).parent_path());

		Zenith_MeshAsset xMesh;
		pfnBuild(xMesh);
		xMesh.Export(strMeshPath.c_str());

		auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xhModel.GetDirect();
		pxModel->SetName(szModelName);
		pxModel->AddMeshByPath(strMeshPath, xMaterialPaths);
		pxModel->Export(strModelPath.c_str());

		ModelHandle xHandle;
		xHandle.Set(pxModel);
		g_axModels.push_back(xHandle);

		Zenith_Log(LOG_CATEGORY_MESH, "[Tennis] exported %s", strModelPath.c_str());
	}
#endif
}

//=============================================================================
// Public path getters (used by both the export write target + the authoring
// LoadModel reference).
//=============================================================================
const char* RenderTest_TennisCourtModelPath()  { return TennisCourtModelPathStr().c_str(); }
const char* RenderTest_TennisNetModelPath()    { return TennisNetModelPathStr().c_str(); }
const char* RenderTest_TennisTapeModelPath()   { return TennisTapeModelPathStr().c_str(); }
const char* RenderTest_TennisRacketModelPath() { return TennisRacketModelPathStr().c_str(); }
const char* RenderTest_TennisBallModelPath()   { return TennisBallModelPathStr().c_str(); }

// (The racket's 180deg-about-X mount is baked directly by the authoring step
// AddStep_AttachToBone(..., 180,0,0) — BuildEulerOffsetMatrix produces the identical
// transform — so no dedicated mount helper is needed here.)

//=============================================================================
// CLI parsing (spectator / follow / camera / IK-showcase). Sets the same
// RenderTest_GameplayState flags the runtime spawn used to set.
//=============================================================================
void RenderTest_ParseTennisCLI()
{
	// Spectator camera (capture aid): a fixed vantage behind the near baseline,
	// elevated, looking down the court (+Z). Defaults overlook the whole court;
	// each is overridable from the CLI (--tenniscam-x/y/z/yaw/pitch=) for close-up
	// capture.
	RenderTest_GameplayState::s_fTennisCamX = RT_TennisArgFloat("--tenniscam-x=", fCOURT_CX);
	RenderTest_GameplayState::s_fTennisCamY = RT_TennisArgFloat("--tenniscam-y=", fSURFACE_Y + 16.0f);
	RenderTest_GameplayState::s_fTennisCamZ = RT_TennisArgFloat("--tenniscam-z=", fBASELINE_NEAR_Z - 14.0f);
	RenderTest_GameplayState::s_fTennisCamYaw = RT_TennisArgFloat("--tenniscam-yaw=", 0.0f);
	RenderTest_GameplayState::s_fTennisCamPitch = RT_TennisArgFloat("--tenniscam-pitch=", -0.5f);
	if (RT_TennisHasFlag("--rendertest-tennis-spectator"))
		RenderTest_GameplayState::s_bTennisSpectatorActive = true;

	// Follow-cam mode: --rendertest-tennis-follow[=near|far] tracks one NPC up
	// close so the strokes + IK + racket are clearly visible. Implies spectator
	// mode (the camera takes over). Defaults to the near player.
#ifdef ZENITH_WINDOWS
	for (int i = 1; i < __argc; i++)
	{
		if (std::strncmp(__argv[i], "--rendertest-tennis-follow", 26) == 0)
		{
			RenderTest_GameplayState::s_bTennisSpectatorActive = true;
			RenderTest_GameplayState::s_bTennisFollowActive = true;
			RenderTest_GameplayState::s_iTennisFollowSide =
				(std::strstr(__argv[i], "far") != nullptr) ? 1 : 0;
		}
		// IK showcase: --rendertest-tennis-ikshowcase=serve|forehand|backhand.
		// Repeats one stroke against a frozen ball; auto-follows the near player.
		if (std::strncmp(__argv[i], "--rendertest-tennis-ikshowcase", 30) == 0)
		{
			RenderTest_GameplayState::s_bTennisIkShowcase = true;
			RenderTest_GameplayState::s_bTennisSpectatorActive = true;
			RenderTest_GameplayState::s_bTennisFollowActive = true;
			RenderTest_GameplayState::s_iTennisFollowSide = 0;   // near player
			RenderTest_GameplayState::s_iTennisShowcaseStroke =
				(std::strstr(__argv[i], "backhand") != nullptr) ? 2 :
				(std::strstr(__argv[i], "forehand") != nullptr) ? 1 : 0;
		}
		// Telemetry: --rendertest-tennis-telemetry[=<base-path>]. Records the
		// match to disk (Zenith_Telemetry) for offline analytics. Optional
		// value overrides the output base path (.ztlm/.json/_*.csv appended).
		if (std::strncmp(__argv[i], "--rendertest-tennis-telemetry", 29) == 0)
		{
			RenderTest_GameplayState::s_bTennisTelemetry = true;
			const char* pszEq = std::strchr(__argv[i], '=');
			if (pszEq != nullptr && pszEq[1] != '\0')
				RenderTest_GameplayState::s_strTennisTelemetryPath = (pszEq + 1);
		}
	}
#endif
}

//=============================================================================
// Tools asset export
//=============================================================================
#ifdef ZENITH_TOOLS
void RenderTest_ExportTennisAssets(const char* szVtxColorMaterialPath)
{
	std::filesystem::create_directories(std::filesystem::path(TennisCourtTexturePath()).parent_path());
	std::filesystem::create_directories(std::filesystem::path(TennisCourtMaterialPath()).parent_path());

	// --- Textures (CPU pixel buffers -> .ztxtr; no GPU upload) ---
	{
		uint32_t uW = 0, uH = 0;
		// sRGB with a full offline mip chain, uncompressed. sRGB because this is
		// displayed colour (the old RGBA8_UNORM export handed the shader
		// gamma-encoded values as if they were linear, which is why the turf sat
		// too bright against everything else on the campus); uncompressed
		// because BC1 blocks smear a 1-texel-wide antialiased paint edge back
		// into the hard step the antialiasing exists to remove.
		const std::vector<uint8_t> xCourtPx = RT_MakeCourtTexture(uW, uH);
		Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(xCourtPx.data(), TennisCourtTexturePath(),
			static_cast<int32_t>(uW), static_cast<int32_t>(uH), TEXTURE_FORMAT_RGBA8_SRGB);

		// Normal: BC5 (R+G; the shader rebuilds Z), linear, full mip chain.
		uint32_t uNW = 0, uNH = 0;
		const std::vector<uint8_t> xCourtNrm = RT_MakeCourtNormalTexture(uNW, uNH);
		Zenith_Tools_TextureExport::ExportFromDataCompressed(xCourtNrm.data(), TennisCourtNormalPath(),
			static_cast<int32_t>(uNW), static_cast<int32_t>(uNH),
			TextureCompressionMode::BC5, TextureColourSpace::Linear);

		// RM + AO: linear data, uncompressed so the roughness step at a paint
		// edge survives exactly.
		std::vector<uint8_t> xCourtRM, xCourtAO;
		uint32_t uDW = 0, uDH = 0;
		RT_MakeCourtDataTextures(xCourtRM, xCourtAO, uDW, uDH);
		Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(xCourtRM.data(), TennisCourtRMPath(),
			static_cast<int32_t>(uDW), static_cast<int32_t>(uDH), TEXTURE_FORMAT_RGBA8_UNORM);
		Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(xCourtAO.data(), TennisCourtAOPath(),
			static_cast<int32_t>(uDW), static_cast<int32_t>(uDH), TEXTURE_FORMAT_RGBA8_UNORM);

		uint32_t uS = 0;
		const std::vector<uint8_t> xNetPx = RT_MakeNetTexture(uS);
		Zenith_Tools_TextureExport::ExportFromData(xNetPx.data(), TennisNetTexturePath(),
			static_cast<int32_t>(uS), static_cast<int32_t>(uS), TEXTURE_FORMAT_RGBA8_UNORM);

		uint32_t uBW = 0, uBH = 0;
		const std::vector<uint8_t> xBallPx = RT_MakeBallTexture(uBW, uBH);
		Zenith_Tools_TextureExport::ExportFromData(xBallPx.data(), TennisBallTexturePath(),
			static_cast<int32_t>(uBW), static_cast<int32_t>(uBH), TEXTURE_FORMAT_RGBA8_UNORM);
	}

	// --- Court material: grass + painted lines, textured by the .ztxtr above ---
	{
		Zenith_MaterialAsset* pxCourtMat = RT_NewMaterial("Tennis_Court");
		pxCourtMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		// With an RM map bound, roughness/metallic are MULTIPLIERS on the
		// sampled channels rather than absolute values: 1.0 passes the map's
		// per-texel paint-vs-turf roughness through untouched, and a literal
		// 0.9 here would quietly scale it back down.
		pxCourtMat->SetRoughness(1.0f);
		pxCourtMat->SetMetallic(0.0f);
		pxCourtMat->SetNormalStrength(1.0f);
		pxCourtMat->SetOcclusionStrength(1.0f);
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(TennisCourtTexturePath()));
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_NORMAL, TextureHandle(TennisCourtNormalPath()));
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_ROUGHNESS_METALLIC, TextureHandle(TennisCourtRMPath()));
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_OCCLUSION, TextureHandle(TennisCourtAOPath()));
		pxCourtMat->SaveToFile(TennisCourtMaterialPath());
	}

	// --- Net material: alpha-tested, two-sided, UV-tiled (preserves every prop
	//     the runtime material set) ---
	{
		Zenith_MaterialAsset* pxNetMat = RT_NewMaterial("Tennis_Net");
		pxNetMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxNetMat->SetRoughness(0.7f);
		pxNetMat->SetMetallic(0.0f);
		pxNetMat->SetBlendMode(MATERIAL_BLEND_MASKED);
		pxNetMat->SetAlphaCutoff(0.5f);
		pxNetMat->SetTwoSided(true);
		pxNetMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(TennisNetTexturePath()));
		// Tile the coarse net texture so the holes read at ~12 cm.
		pxNetMat->SetUVTiling(Vector2(2.0f * fNET_HALF_WIDTH, fNET_HEIGHT));
		pxNetMat->SaveToFile(TennisNetMaterialPath());
	}

	// --- Ball material: tennis-felt texture with the curved seam so the ball's
	//     SPIN is visible as it rotates (base colour tints the felt) ---
	{
		Zenith_MaterialAsset* pxBallMat = RT_NewMaterial("Tennis_Ball");
		pxBallMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxBallMat->SetRoughness(0.55f);
		pxBallMat->SetMetallic(0.0f);
		pxBallMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(TennisBallTexturePath()));
		pxBallMat->SaveToFile(TennisBallMaterialPath());
	}

	// --- Meshes + bundling models ---
	// Court + net bundle their textured materials; tape + racket bundle the shared
	// vertex-colour material; the ball bundles its dedicated yellow material.
	{
		Zenith_Vector<std::string> xCourtMat;  xCourtMat.PushBack(TennisCourtMaterialPath());
		RT_ExportMeshModel("RenderTest_Tennis_Court", &RT_BuildCourtSlab,
			TennisCourtMeshPath(), TennisCourtModelPathStr(), xCourtMat);

		Zenith_Vector<std::string> xNetMat;    xNetMat.PushBack(TennisNetMaterialPath());
		RT_ExportMeshModel("RenderTest_Tennis_Net", &RT_BuildNetQuad,
			TennisNetMeshPath(), TennisNetModelPathStr(), xNetMat);

		Zenith_Vector<std::string> xVtxMat;    xVtxMat.PushBack(szVtxColorMaterialPath);
		RT_ExportMeshModel("RenderTest_Tennis_NetTape", &RT_BuildNetTape,
			TennisTapeMeshPath(), TennisTapeModelPathStr(), xVtxMat);
		RT_ExportMeshModel("RenderTest_Tennis_Racket", &RT_BuildRacket,
			TennisRacketMeshPath(), TennisRacketModelPathStr(), xVtxMat);

		// Ball: a unit sphere (radius 0.5; bundle the yellow ball material). The
		// authoring scales it to the ball diameter; a sphere collider sized from
		// the same scale lines up with the mesh.
		Zenith_Vector<std::string> xBallMat;   xBallMat.PushBack(TennisBallMaterialPath());
		RT_ExportMeshModel("RenderTest_Tennis_Ball",
			[](Zenith_MeshAsset& xOut) { Zenith_MeshAsset::GenerateUnitSphere(xOut, 20); },   // 20 segs == original runtime CreateUnitSphere(20)
			TennisBallMeshPath(), TennisBallModelPathStr(), xBallMat);
	}
}
#endif

#include "RenderTest/RenderTest_TennisCourtTexture.Tests.inl"

void RenderTest_TennisShutdown()
{
	// Release the export-time material/model handles while the AssetRegistry is
	// still alive (mirrors RenderTest_JetpackShutdown / RenderTest_GunsShutdown).
	g_axMaterials.clear();
	g_axModels.clear();
}
