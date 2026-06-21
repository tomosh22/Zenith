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
	const std::string& TennisNetTexturePath()   { static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/Tennis_Net"   ZENITH_TEXTURE_EXT; return s; }

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

	// Court texture: green grass (with a little noise) + white painted lines.
	// U maps to court width (X), V to court length (Z). The painted court is
	// inset by the grass apron. Lines: doubles + singles sidelines, baselines,
	// the two service lines, and the centre service line. Returns the RGBA8 pixel
	// buffer (uW/uH set) so the caller can export it to disk.
	std::vector<uint8_t> RT_MakeCourtTexture(uint32_t& uWOut, uint32_t& uHOut)
	{
		constexpr uint32_t uW = 384;
		constexpr uint32_t uH = 832;   // ~ slabWidth : slabLength
		std::vector<uint8_t> xPx(static_cast<size_t>(uW) * uH * 4);

		// Grass base with subtle hash noise.
		for (uint32_t y = 0; y < uH; y++)
		{
			for (uint32_t x = 0; x < uW; x++)
			{
				uint32_t uHash = (x * 374761393u + y * 668265263u);
				uHash = (uHash ^ (uHash >> 13)) * 1274126177u;
				const int iN = static_cast<int>((uHash >> 24) & 0x1F) - 16;   // [-16,15]
				uint8_t* p = &xPx[(static_cast<size_t>(y) * uW + x) * 4];
				p[0] = static_cast<uint8_t>(glm::clamp(38 + iN / 2, 0, 255));
				p[1] = static_cast<uint8_t>(glm::clamp(110 + iN, 0, 255));
				p[2] = static_cast<uint8_t>(glm::clamp(46 + iN / 2, 0, 255));
				p[3] = 255;
			}
		}

		auto FillRect = [&](float fU0, float fV0, float fU1, float fV1)
		{
			const int ix0 = glm::clamp(static_cast<int>(fU0 * uW), 0, static_cast<int>(uW) - 1);
			const int ix1 = glm::clamp(static_cast<int>(fU1 * uW), 0, static_cast<int>(uW) - 1);
			const int iy0 = glm::clamp(static_cast<int>(fV0 * uH), 0, static_cast<int>(uH) - 1);
			const int iy1 = glm::clamp(static_cast<int>(fV1 * uH), 0, static_cast<int>(uH) - 1);
			for (int y = iy0; y <= iy1; y++)
				for (int x = ix0; x <= ix1; x++)
				{
					uint8_t* p = &xPx[(static_cast<size_t>(y) * uW + x) * 4];
					p[0] = 235; p[1] = 235; p[2] = 235; p[3] = 255;
				}
		};

		// Normalised court geometry over the slab footprint.
		const float fApronU = fAPRON / (2.0f * fSLAB_HALF_WIDTH);
		const float fApronV = fAPRON / (2.0f * fSLAB_HALF_LENGTH);
		const float fSinglesInsetU = 1.37f / (2.0f * fSLAB_HALF_WIDTH);
		const float fServiceV = fSERVICE_LINE_OFFSET / (2.0f * fSLAB_HALF_LENGTH);
		// Line half-thickness in U/V (~5 cm physical).
		const float fLwU = 0.06f / (2.0f * fSLAB_HALF_WIDTH);
		const float fLwV = 0.06f / (2.0f * fSLAB_HALF_LENGTH);

		const float fLeftDoubles  = fApronU;
		const float fRightDoubles = 1.0f - fApronU;
		const float fLeftSingles  = fApronU + fSinglesInsetU;
		const float fRightSingles = 1.0f - fApronU - fSinglesInsetU;
		const float fNearBase = fApronV;
		const float fFarBase  = 1.0f - fApronV;
		const float fNearService = 0.5f - fServiceV;
		const float fFarService  = 0.5f + fServiceV;

		// Sidelines (full court length).
		FillRect(fLeftDoubles - fLwU,  fNearBase, fLeftDoubles + fLwU,  fFarBase);
		FillRect(fRightDoubles - fLwU, fNearBase, fRightDoubles + fLwU, fFarBase);
		FillRect(fLeftSingles - fLwU,  fNearBase, fLeftSingles + fLwU,  fFarBase);
		FillRect(fRightSingles - fLwU, fNearBase, fRightSingles + fLwU, fFarBase);
		// Baselines (full doubles width).
		FillRect(fLeftDoubles, fNearBase - fLwV, fRightDoubles, fNearBase + fLwV);
		FillRect(fLeftDoubles, fFarBase - fLwV,  fRightDoubles, fFarBase + fLwV);
		// Service lines (singles width).
		FillRect(fLeftSingles, fNearService - fLwV, fRightSingles, fNearService + fLwV);
		FillRect(fLeftSingles, fFarService - fLwV,  fRightSingles, fFarService + fLwV);
		// Centre service line (between the two service lines).
		FillRect(0.5f - fLwU, fNearService, 0.5f + fLwU, fFarService);
		// Centre marks on the baselines.
		FillRect(0.5f - fLwU, fNearBase, 0.5f + fLwU, fNearBase + 6.0f * fLwV);
		FillRect(0.5f - fLwU, fFarBase - 6.0f * fLwV, 0.5f + fLwU, fFarBase);

		uWOut = uW;
		uHOut = uH;
		return xPx;
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
		xHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
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

		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
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
		const std::vector<uint8_t> xCourtPx = RT_MakeCourtTexture(uW, uH);
		Zenith_Tools_TextureExport::ExportFromData(xCourtPx.data(), TennisCourtTexturePath(),
			static_cast<int32_t>(uW), static_cast<int32_t>(uH), TEXTURE_FORMAT_RGBA8_UNORM);

		uint32_t uS = 0;
		const std::vector<uint8_t> xNetPx = RT_MakeNetTexture(uS);
		Zenith_Tools_TextureExport::ExportFromData(xNetPx.data(), TennisNetTexturePath(),
			static_cast<int32_t>(uS), static_cast<int32_t>(uS), TEXTURE_FORMAT_RGBA8_UNORM);
	}

	// --- Court material: grass + painted lines, textured by the .ztxtr above ---
	{
		Zenith_MaterialAsset* pxCourtMat = RT_NewMaterial("Tennis_Court");
		pxCourtMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxCourtMat->SetRoughness(0.9f);
		pxCourtMat->SetMetallic(0.0f);
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(TennisCourtTexturePath()));
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

	// --- Ball material: tennis yellow, vertex colour irrelevant (sphere is
	//     analytic-shaded; base colour drives the look) ---
	{
		Zenith_MaterialAsset* pxBallMat = RT_NewMaterial("Tennis_Ball");
		pxBallMat->SetBaseColor(Vector4(0.78f, 0.88f, 0.16f, 1.0f));
		pxBallMat->SetRoughness(0.55f);
		pxBallMat->SetMetallic(0.0f);
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

void RenderTest_TennisShutdown()
{
	// Release the export-time material/model handles while the AssetRegistry is
	// still alive (mirrors RenderTest_JetpackShutdown / RenderTest_GunsShutdown).
	g_axMaterials.clear();
	g_axModels.clear();
}
