#include "Zenith.h"
#include "RenderTest/RenderTest_Tennis.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_CommandLine.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Types.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "RenderTest/Components/RenderTest_GameplayState.h"
#include "RenderTest/Components/RenderTest_TennisMatchComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"

#include <vector>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace RenderTest_Tennis;
using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// Procedural geometry/material/texture helpers for the tennis testbed.
//
// Everything is built at runtime and attached via Zenith_ModelComponent::
// AddMeshEntry (procedural meshes do not serialize), so the spawn runs once
// post scene load and is windowed-only. Geometries are leaked for the process
// lifetime (the session-long convention used by the other procedural games) —
// the caller must keep the Flux_MeshGeometry alive for the component lifetime,
// and these courts/players live until the process exits.
//=============================================================================
namespace
{
	// Session-lifetime owners so attached geometry/material/texture stay alive.
	std::vector<Flux_MeshGeometry*> g_apxGeoms;
	std::vector<MaterialHandle>     g_axMaterials;
	std::vector<TextureHandle>      g_axTextures;

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

	template <typename T>
	T* RT_Alloc(uint32_t uCount)
	{
		return static_cast<T*>(Zenith_MemoryManagement::Allocate(uCount * sizeof(T)));
	}

	// Accumulating triangle-mesh builder. Quads use the same CCW winding as
	// Flux_MeshGeometry::GenerateUnitCube (0-2-1, 1-2-3) so front faces survive
	// Vulkan back-face culling. p0=bottom-left, p1=bottom-right, p2=top-left,
	// p3=top-right in the face's own frame.
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

		// Finalize into a heap Flux_MeshGeometry (GPU-uploaded). Leaked for the
		// session; tracked in g_apxGeoms so nothing reclaims it.
		Flux_MeshGeometry* Build()
		{
			Flux_MeshGeometry* pxGeom = new Flux_MeshGeometry();
			const uint32_t uNV = static_cast<uint32_t>(m_xPos.size());
			const uint32_t uNI = static_cast<uint32_t>(m_xIdx.size());
			pxGeom->m_uNumVerts = uNV;
			pxGeom->m_uNumIndices = uNI;
			pxGeom->m_pxPositions  = RT_Alloc<Vector3>(uNV);
			pxGeom->m_pxNormals    = RT_Alloc<Vector3>(uNV);
			pxGeom->m_pxTangents   = RT_Alloc<Vector3>(uNV);
			pxGeom->m_pxBitangents = RT_Alloc<Vector3>(uNV);
			pxGeom->m_pxUVs        = RT_Alloc<Vector2>(uNV);
			pxGeom->m_pxColors     = RT_Alloc<Vector4>(uNV);
			pxGeom->m_puIndices    = RT_Alloc<Flux_MeshGeometry::IndexType>(uNI);
			std::memcpy(pxGeom->m_pxPositions,  m_xPos.data(), uNV * sizeof(Vector3));
			std::memcpy(pxGeom->m_pxNormals,    m_xNrm.data(), uNV * sizeof(Vector3));
			std::memcpy(pxGeom->m_pxTangents,   m_xTan.data(), uNV * sizeof(Vector3));
			std::memcpy(pxGeom->m_pxBitangents, m_xBit.data(), uNV * sizeof(Vector3));
			std::memcpy(pxGeom->m_pxUVs,        m_xUV.data(),  uNV * sizeof(Vector2));
			std::memcpy(pxGeom->m_pxColors,     m_xCol.data(), uNV * sizeof(Vector4));
			std::memcpy(pxGeom->m_puIndices,    m_xIdx.data(), uNI * sizeof(Flux_MeshGeometry::IndexType));
			pxGeom->GenerateLayoutAndVertexData();
			pxGeom->UploadToGPU();
			g_apxGeoms.push_back(pxGeom);
			return pxGeom;
		}
	};

	// --- Procedural textures -------------------------------------------------

	Zenith_TextureAsset* RT_MakeTexture(uint32_t uW, uint32_t uH, const std::vector<uint8_t>& xPixels)
	{
		Zenith_TextureAsset* pxTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
		Flux_SurfaceInfo xInfo;
		xInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		xInfo.m_eTextureType = TEXTURE_TYPE_2D;
		xInfo.m_uWidth = uW;
		xInfo.m_uHeight = uH;
		xInfo.m_uDepth = 1;
		xInfo.m_uNumMips = 1;
		xInfo.m_uNumLayers = 1;
		xInfo.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		pxTex->CreateFromData(xPixels.data(), xInfo, /*bCreateMips*/ false);
		return pxTex;
	}

	// Court texture: green grass (with a little noise) + white painted lines.
	// U maps to court width (X), V to court length (Z). The painted court is
	// inset by the grass apron. Lines: doubles + singles sidelines, baselines,
	// the two service lines, and the centre service line.
	Zenith_TextureAsset* RT_MakeCourtTexture()
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

		return RT_MakeTexture(uW, uH, xPx);
	}

	// Net texture: a coarse mesh — dark cord on the grid lines (opaque), holes
	// transparent. Tiled across the net panel by the material UV tiling.
	Zenith_TextureAsset* RT_MakeNetTexture()
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
		return RT_MakeTexture(uS, uS, xPx);
	}

	// --- Materials -----------------------------------------------------------

	Zenith_MaterialAsset* RT_NewMaterial(const char* szName)
	{
		MaterialHandle xHandle;
		xHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		g_axMaterials.push_back(xHandle);
		Zenith_MaterialAsset* pxMat = xHandle.GetDirect();
		pxMat->SetName(szName);
		return pxMat;
	}

	TextureHandle RT_TrackTexture(Zenith_TextureAsset* pxTex)
	{
		TextureHandle xHandle(pxTex);
		g_axTextures.push_back(xHandle);
		return xHandle;
	}

	// --- Court + net geometry (unit meshes; scaled by the entity transform so
	//     OBB colliders, which read transform scale, match the visual) --------

	// Unit box [-0.5,0.5]^3. Top (+Y) face carries the full court texture; the
	// other five faces sample a fixed grass pixel so the slab edges read as grass.
	Flux_MeshGeometry* RT_BuildCourtSlab()
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
		return xB.Build();
	}

	// Unit quad in the XY plane ([-0.5,0.5] x [-0.5,0.5], Z=0), normal +Z. Scaled
	// to the net dimensions; the two-sided material makes the back visible.
	Flux_MeshGeometry* RT_BuildQuad()
	{
		GeomBuilder xB;
		const float h = 0.5f;
		xB.AddQuad({ -h, -h, 0.0f }, { h, -h, 0.0f }, { -h, h, 0.0f }, { h, h, 0.0f }, { 0, 0, 1 },
			{ 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 0 }, Vector4(1.0f));
		return xB.Build();
	}

	// Add an axis-aligned box [xMin,xMax] to a builder with a flat vertex colour
	// (UVs unused — the racket is vertex-coloured, not textured).
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

	// Tennis racket: a dark grip/handle + a light oval string-bed head, built in
	// its own local frame with the grip at the origin (+Y = up the racket). One
	// mesh, two-tone via vertex colour (material base = white). Attached to the
	// hand bone by Zenith_AttachmentComponent.
	Flux_MeshGeometry* RT_BuildRacket()
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
		return xB.Build();
	}

	// --- Entity spawn helpers ------------------------------------------------

	Zenith_Entity RT_SpawnMeshEntity(Zenith_Scene xScene, const char* szName,
		const Vector3& xPos, const Vector3& xScale,
		Flux_MeshGeometry* pxGeom, Zenith_MaterialAsset* pxMat)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(xScene, szName);
		Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(xPos);
		xT.SetScale(xScale);
		Zenith_ModelComponent& xModel = xEnt.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxGeom, *pxMat);
		return xEnt;
	}
}

//=============================================================================
// Spawn entry point
//=============================================================================
void RenderTest_SpawnTennisCourt()
{
	if (Zenith_CommandLine::IsHeadless())
		return;   // procedural meshes/textures are GPU-dependent — windowed only.

	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Tennis] no active scene; skipping court spawn");
		return;
	}

	// --- Court slab: grass + painted lines, static OBB collider for the bounce ---
	{
		Zenith_MaterialAsset* pxCourtMat = RT_NewMaterial("Tennis_Court");
		pxCourtMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxCourtMat->SetRoughness(0.9f);
		pxCourtMat->SetMetallic(0.0f);
		pxCourtMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, RT_TrackTexture(RT_MakeCourtTexture()));

		Flux_MeshGeometry* pxSlab = RT_BuildCourtSlab();
		const Vector3 xScale(2.0f * fSLAB_HALF_WIDTH, fSLAB_THICKNESS, 2.0f * fSLAB_HALF_LENGTH);
		const Vector3 xPos(fCOURT_CX, fSURFACE_Y - fSLAB_THICKNESS * 0.5f, fCOURT_CZ);
		Zenith_Entity xCourt = RT_SpawnMeshEntity(xScene, "Tennis_Court", xPos, xScale, pxSlab, pxCourtMat);
		Zenith_ColliderComponent& xCol = xCourt.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
	}

	// --- Net: alpha-tested, two-sided, thin static collider ---
	{
		Zenith_MaterialAsset* pxNetMat = RT_NewMaterial("Tennis_Net");
		pxNetMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		pxNetMat->SetRoughness(0.7f);
		pxNetMat->SetMetallic(0.0f);
		pxNetMat->SetBlendMode(MATERIAL_BLEND_MASKED);
		pxNetMat->SetAlphaCutoff(0.5f);
		pxNetMat->SetTwoSided(true);
		pxNetMat->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, RT_TrackTexture(RT_MakeNetTexture()));
		// Tile the coarse net texture so the holes read at ~12 cm.
		pxNetMat->SetUVTiling(Vector2(2.0f * fNET_HALF_WIDTH, fNET_HEIGHT));

		Flux_MeshGeometry* pxQuad = RT_BuildQuad();
		const Vector3 xScale(2.0f * fNET_HALF_WIDTH, fNET_HEIGHT, 0.06f);
		const Vector3 xPos(fCOURT_CX, fSURFACE_Y + fNET_HEIGHT * 0.5f, fCOURT_CZ);
		Zenith_Entity xNet = RT_SpawnMeshEntity(xScene, "Tennis_Net", xPos, xScale, pxQuad, pxNetMat);
		Zenith_ColliderComponent& xCol = xNet.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		// White net tape along the top edge (the iconic tennis-net band). Opaque,
		// visual only — built at real size, centred, positioned at the net top.
		GeomBuilder xTapeB;
		RT_AddBox(xTapeB, Vector3(-fNET_HALF_WIDTH, -0.03f, -0.03f), Vector3(fNET_HALF_WIDTH, 0.03f, 0.03f),
			Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		Flux_MeshGeometry* pxTape = xTapeB.Build();
		Zenith_MaterialAsset* pxTapeMat = RT_NewMaterial("Tennis_NetTape");
		pxTapeMat->SetBaseColor(Vector4(0.96f, 0.96f, 0.96f, 1.0f));
		pxTapeMat->SetRoughness(0.6f);
		pxTapeMat->SetMetallic(0.0f);
		Zenith_Entity xTape = g_xEngine.Scenes().CreateEntity(xScene, "Tennis_NetTape");
		xTape.GetComponent<Zenith_TransformComponent>().SetPosition(Vector3(fCOURT_CX, fSURFACE_Y + fNET_HEIGHT, fCOURT_CZ));
		xTape.AddComponent<Zenith_ModelComponent>().AddMeshEntry(*pxTape, *pxTapeMat);
	}

	// --- Ball: dynamic sphere (tennis yellow). Restitution/friction are applied
	//     at runtime by the match component (they don't serialize). ---
	{
		Zenith_MaterialAsset* pxBallMat = RT_NewMaterial("Tennis_Ball");
		pxBallMat->SetBaseColor(Vector4(0.78f, 0.88f, 0.16f, 1.0f));
		pxBallMat->SetRoughness(0.55f);
		pxBallMat->SetMetallic(0.0f);

		// Registry-cached unit sphere (radius 0.5); scale to the ball diameter.
		Flux_MeshGeometry* pxBallGeom = Zenith_MeshGeometryAsset::CreateUnitSphere(20)->GetGeometry();
		const Vector3 xScale(2.0f * fBALL_RADIUS);
		const Vector3 xPos(fCOURT_CX, fSURFACE_Y + 3.0f, fBASELINE_NEAR_Z + 1.0f);
		Zenith_Entity xBall = RT_SpawnMeshEntity(xScene, "Tennis_Ball", xPos, xScale, pxBallGeom, pxBallMat);
		// Set scale before AddCollider (sphere radius is derived from it).
		Zenith_ColliderComponent& xCol = xBall.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	}

	// --- NPC players + their rackets ---
	// Two StickFigure NPCs on opposite baselines; each holds a racket attached to
	// its right hand by the engine attachment component.
	{
		const std::string strStickModel = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MODEL_EXT;

		Flux_MeshGeometry* pxRacketGeom = RT_BuildRacket();
		Zenith_MaterialAsset* pxRacketMat = RT_NewMaterial("Tennis_Racket");
		pxRacketMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));   // vertex colour shows through
		pxRacketMat->SetRoughness(0.5f);
		pxRacketMat->SetMetallic(0.1f);

		// Racket mount: rotate 180deg about X so the head extends along the hand's
		// continuation, with the grip seated in the palm. (Tuning knob.)
		const Zenith_Maths::Matrix4 xRacketOffset =
			glm::rotate(Zenith_Maths::Matrix4(1.0f), static_cast<float>(Zenith_Maths::Pi), Vector3(1.0f, 0.0f, 0.0f));

		auto SpawnPlayer = [&](const char* szNpcName, const char* szRacketName, bool bNear)
		{
			const float fZ = bNear ? fBASELINE_NEAR_Z : fBASELINE_FAR_Z;
			// Capsule (half-extent ~1.05) rests on the court with the model's feet
			// near the surface.
			const Vector3 xPos(fCOURT_CX, fSURFACE_Y + 1.05f, fZ);

			Zenith_Entity xNpc = g_xEngine.Scenes().CreateEntity(xScene, szNpcName);
			xNpc.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
			Zenith_ModelComponent& xModel = xNpc.AddComponent<Zenith_ModelComponent>();
			xModel.LoadModel(strStickModel);
			xNpc.AddComponent<Zenith_AnimatorComponent>();
			xNpc.AddComponent<RenderTest_TennisPlayerComponent>().Init(bNear);

			Zenith_Entity xRacket = g_xEngine.Scenes().CreateEntity(xScene, szRacketName);
			// Seat the racket at the player's position so frame 0 (before the bone
			// resolves in OnLateUpdate) isn't at the world origin.
			xRacket.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
			Zenith_ModelComponent& xRModel = xRacket.AddComponent<Zenith_ModelComponent>();
			xRModel.AddMeshEntry(*pxRacketGeom, *pxRacketMat);
			Zenith_AttachmentComponent& xAttach = xRacket.AddComponent<Zenith_AttachmentComponent>();
			xAttach.AttachToBone(xNpc, "RightHand", xRacketOffset);
		};

		SpawnPlayer("Tennis_NPC_Near", "Tennis_Racket_Near", true);
		SpawnPlayer("Tennis_NPC_Far",  "Tennis_Racket_Far",  false);
	}

	// --- Match manager: owns the ball + the players, scoring, score text. ---
	{
		Zenith_Entity xMatch = g_xEngine.Scenes().CreateEntity(xScene, "Tennis_Match");
		xMatch.AddComponent<RenderTest_TennisMatchComponent>();
	}

	// --- Spectator camera (capture aid) ---
	// A fixed vantage behind the near baseline, elevated, looking down the court
	// (+Z). The follow camera honours these when the flag is set; the tennis
	// match also runs in Play mode, so the autonomous rally is framed.
	// Defaults overlook the whole court from behind the near baseline; each is
	// overridable from the CLI (--tenniscam-x/y/z/yaw/pitch=) for close-up capture.
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

	Zenith_Log(LOG_CATEGORY_CORE,
		"[Tennis] spawned court (%.1f x %.1f) + net at (%.0f, %.0f, %.0f); spectator=%d",
		fCOURT_WIDTH, fCOURT_LENGTH, fCOURT_CX, fSURFACE_Y, fCOURT_CZ,
		RenderTest_GameplayState::s_bTennisSpectatorActive ? 1 : 0);
}

void RenderTest_TennisShutdown()
{
	// Drop the registry refcounts while the AssetRegistry is still alive (mirrors
	// RenderTest::Project_Shutdown's handling of its own Resources handles). The
	// procedural Flux_MeshGeometry objects in g_apxGeoms are intentionally leaked
	// for the session (same convention as the material showcase); they are not
	// registry assets, so they don't trip the asset-refcount assertion.
	g_axMaterials.clear();
	g_axTextures.clear();
}
