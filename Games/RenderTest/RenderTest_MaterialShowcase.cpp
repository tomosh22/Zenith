#include "Zenith.h"
#include "RenderTest/RenderTest_MaterialShowcase.h"

#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "Physics/Zenith_Physics_Fwd.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
// Offline (CPU) texture export. The header lives under the engine /Tools tree, which
// is not on a game's include search path, so reach it with a relative path (mirrors
// RenderTest_Tennis.cpp).
#include "../../Tools/Zenith_Tools_TextureExport.h"
#endif

#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <filesystem>

//=============================================================================
// Material-showcase asset production + authoring.
//
// Everything the showcase needs to render — the five shape meshes, the three
// procedural textures (normal / checker / cutout), one material per grid cell (plus
// the shared brushed-gold instance parent + the platform slab material), and a
// bundling .zmodel per cell — is baked OFFLINE here into CPU assets on disk, exactly
// like the tennis / guns / jetpack testbeds. The authored scene (RenderTest.cpp)
// LoadModels these and creates the platform + cell entities + static per-primitive
// colliders itself; this file no longer touches the scene or the GPU.
//
// The single source of truth for "cell i -> shape + material" is RunRecipe(): it
// walks the row-major cell recipe once, recording each cell's shape/panel/model-path
// into g_axCells and (when bWrite) building + saving the material + model. The layout
// math + grid extents are derived from the recorded cell count.
//=============================================================================

// Grid framing read by the MaterialBattleTest capture (defined unconditionally so the
// header extern resolves in every config; only ever populated during the tools build).
namespace RenderTest_Showcase
{
	int   g_iColumns  = 0;
	float g_fGridMinX = 0.0f;
	float g_fGridMaxX = 0.0f;
}

namespace
{
	// ---- World framing constants (were RenderTest_Showcase:: in RenderTest.cpp) ----
	// Centred on the terrain with the rest of the campus; the showcase sits 44 m north
	// of the campus centre (2048, 2048). Co-planar with the player deck (top Y 48.75)
	// and the tennis court so the three platforms form one connected campus.
	constexpr float fPLATFORM_CX    = 2048.0f;
	constexpr float fPLATFORM_CZ    = 2048.0f + 44.0f;
	constexpr float fPLATFORM_TOP_Y = 48.75f;
	constexpr int   iROWS           = 5;
	constexpr float fCOL_SPACING    = 3.0f;
	constexpr float fROW_SPACING    = 3.6f;
	constexpr float fSHAPE_SCALE    = 1.6f;

	// Session-lifetime owners for the export-time material/model handles, so they
	// outlive the export and are released cleanly at shutdown (mirrors the tennis
	// export pattern).
	std::vector<MaterialHandle> g_axMaterials;
	std::vector<ModelHandle>    g_axModels;

	enum class ShapeId : uint8_t { Sphere, Cube, Cylinder, Cone, Capsule };

	struct CellMeta
	{
		ShapeId     m_eShape = ShapeId::Sphere;
		bool        m_bPanel = false;      // upright thin cube face (cutout / two-sided)
		std::string m_strModelPath;        // stable storage for AddStep_LoadModel
	};
	// Ordered list of cells (row-major), populated by RunRecipe. Kept file-scope so the
	// model-path strings referenced by AddStep_LoadModel stay alive until the automation
	// executes and for the lifetime of the process.
	std::vector<CellMeta> g_axCells;

#ifdef ZENITH_TOOLS
	//-------------------------------------------------------------------------
	// Deterministic on-disk paths (absolute GAME_ASSETS_DIR; the asset registry
	// normalises them to game: on load). Only referenced by the tools export /
	// authoring, so guarded to avoid an unreferenced-function warning in _False.
	//-------------------------------------------------------------------------
	const std::string& ShapeMeshPath(ShapeId eShape)
	{
		static const std::string s[5] = {
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Sphere"   ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Cube"     ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Cylinder" ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Cone"     ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Capsule"  ZENITH_MESH_ASSET_EXT,
		};
		return s[static_cast<int>(eShape)];
	}
	const std::string& InstanceParentMaterialPath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/MatShowcase_InstanceParent" ZENITH_MATERIAL_EXT;
		return s;
	}
	const std::string& PlatformMaterialPath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/MatShowcase_Platform" ZENITH_MATERIAL_EXT;
		return s;
	}
	const std::string& PlatformModelPath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Platform" ZENITH_MODEL_EXT;
		return s;
	}
	const std::string& NormalTexturePath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/MatShowcase_Normal" ZENITH_TEXTURE_EXT;
		return s;
	}
	const std::string& CheckerTexturePath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/MatShowcase_Checker" ZENITH_TEXTURE_EXT;
		return s;
	}
	const std::string& CutoutTexturePath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Textures/RenderTest/MatShowcase_Cutout" ZENITH_TEXTURE_EXT;
		return s;
	}
	std::string CellMaterialPath(int iCell)
	{
		char szIdx[8];
		std::snprintf(szIdx, sizeof(szIdx), "%02d", iCell);
		return std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/MatShowcase_Mat_" + szIdx + ZENITH_MATERIAL_EXT;
	}
	std::string CellModelPath(int iCell)
	{
		char szIdx[8];
		std::snprintf(szIdx, sizeof(szIdx), "%02d", iCell);
		return std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/MatShowcase_Cell_" + szIdx + ZENITH_MODEL_EXT;
	}
#endif // ZENITH_TOOLS
}

#ifdef ZENITH_TOOLS
namespace
{
	using Zenith_Maths::Vector4;

	//-------------------------------------------------------------------------
	// Procedural texture pixel buffers (CPU only -> .ztxtr). Same pixel maths as the
	// old runtime RT_Make* helpers, minus the GPU CreateFromData.
	//-------------------------------------------------------------------------
	// Tangent-space bump normal map (rounded sinusoidal dimples).
	void RT_NormalMapPixels(uint32_t uSize, std::vector<uint8_t>& xOut)
	{
		xOut.assign(static_cast<size_t>(uSize) * uSize * 4, 0);
		for (uint32_t y = 0; y < uSize; y++)
			for (uint32_t x = 0; x < uSize; x++)
			{
				const float fU = (x / float(uSize)) * 6.2831853f * 5.0f;
				const float fV = (y / float(uSize)) * 6.2831853f * 5.0f;
				Zenith_Maths::Vector3 xN = glm::normalize(Zenith_Maths::Vector3(0.55f * sinf(fU), 0.55f * sinf(fV), 1.0f));
				uint8_t* p = &xOut[(static_cast<size_t>(y) * uSize + x) * 4];
				p[0] = uint8_t((xN.x * 0.5f + 0.5f) * 255.0f);
				p[1] = uint8_t((xN.y * 0.5f + 0.5f) * 255.0f);
				p[2] = uint8_t((xN.z * 0.5f + 0.5f) * 255.0f);
				p[3] = 255;
			}
	}
	// Two-tone checker base-colour map (opaque).
	void RT_CheckerColourPixels(uint32_t uSize, std::vector<uint8_t>& xOut)
	{
		xOut.assign(static_cast<size_t>(uSize) * uSize * 4, 0);
		for (uint32_t y = 0; y < uSize; y++)
			for (uint32_t x = 0; x < uSize; x++)
			{
				const bool bOn = (((x / 8) + (y / 8)) & 1) == 0;
				uint8_t* p = &xOut[(static_cast<size_t>(y) * uSize + x) * 4];
				p[0] = bOn ? 230 : 30;
				p[1] = bOn ? 120 : 90;
				p[2] = bOn ? 40  : 170;
				p[3] = 255;
			}
	}
	// Checker with hard 0/255 alpha for alpha-cutout proofs.
	void RT_CheckerAlphaPixels(uint32_t uSize, std::vector<uint8_t>& xOut)
	{
		xOut.assign(static_cast<size_t>(uSize) * uSize * 4, 0);
		for (uint32_t y = 0; y < uSize; y++)
			for (uint32_t x = 0; x < uSize; x++)
			{
				const bool bOn = (((x / 8) + (y / 8)) & 1) == 0;
				uint8_t* p = &xOut[(static_cast<size_t>(y) * uSize + x) * 4];
				p[0] = p[1] = p[2] = 235;
				p[3] = bOn ? 255 : 0;
			}
	}

	//-------------------------------------------------------------------------
	// Material helpers (mirror the old spawn's lambdas).
	//-------------------------------------------------------------------------
	void SetF(Zenith_MaterialAsset* m, MaterialParamID e, float v)
	{
		Zenith_MaterialParamTable::SetParamFloat(m->ModifyParams(), e, v);
	}
	void SetV(Zenith_MaterialAsset* m, MaterialParamID e, float r, float g, float b, float a)
	{
		Zenith_MaterialParamTable::SetParamVector(m->ModifyParams(), e, Vector4(r, g, b, a));
	}
	void SetI(Zenith_MaterialAsset* m, MaterialParamID e, u_int v)
	{
		Zenith_MaterialParamTable::SetParamInt(m->ModifyParams(), e, v);
	}
	Zenith_MaterialAsset* NewMat(const char* szName)
	{
		MaterialHandle xHandle;
		xHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		g_axMaterials.push_back(xHandle);
		Zenith_MaterialAsset* m = xHandle.GetDirect();
		m->SetName(szName);
		SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.82f, 0.82f, 0.82f, 1.0f);
		SetF(m, MATERIAL_PARAM_METALLIC, 0.0f);
		SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.5f);
		SetF(m, MATERIAL_PARAM_SPECULAR, 0.5f);
		return m;
	}

	void ExportShapeMesh(ShapeId eShape, uint32_t uSeg)
	{
		Zenith_MeshAsset xMesh;
		switch (eShape)
		{
		case ShapeId::Sphere:   Zenith_MeshAsset::GenerateUnitSphere(xMesh, uSeg);   break;
		case ShapeId::Cube:     Zenith_MeshAsset::GenerateUnitCube(xMesh);           break;
		case ShapeId::Cylinder: Zenith_MeshAsset::GenerateUnitCylinder(xMesh, uSeg); break;
		case ShapeId::Cone:     Zenith_MeshAsset::GenerateUnitCone(xMesh, uSeg);     break;
		case ShapeId::Capsule:  Zenith_MeshAsset::GenerateUnitCapsule(xMesh, uSeg);  break;
		}
		xMesh.Export(ShapeMeshPath(eShape).c_str());
	}

	// Build + export a bundling .zmodel for cell iCell (shape mesh + one material path).
	void BundleCellModel(int iCell, ShapeId eShape, const std::string& strMatPath)
	{
		Zenith_Vector<std::string> xMats;
		xMats.PushBack(strMatPath);
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		char szName[64];
		std::snprintf(szName, sizeof(szName), "MatShowcase_Cell_%02d", iCell);
		pxModel->SetName(szName);
		pxModel->AddMeshByPath(ShapeMeshPath(eShape), xMats);
		pxModel->Export(g_axCells[iCell].m_strModelPath.c_str());
		ModelHandle xHandle;
		xHandle.Set(pxModel);
		g_axModels.push_back(xHandle);
	}

	//-------------------------------------------------------------------------
	// The single ordered cell recipe. Always records {shape, panel, model-path} into
	// g_axCells (+ grid extents); when bWrite, also builds + saves each cell's material
	// and bundling model, the shared instance parent, the platform material/model, the
	// three textures, and the five shape meshes.
	//-------------------------------------------------------------------------
	void RunRecipe(bool bWrite)
	{
		g_axCells.clear();

		if (bWrite)
		{
			std::filesystem::create_directories(std::filesystem::path(ShapeMeshPath(ShapeId::Sphere)).parent_path());
			std::filesystem::create_directories(std::filesystem::path(CellMaterialPath(0)).parent_path());
			std::filesystem::create_directories(std::filesystem::path(NormalTexturePath()).parent_path());

			// Textures (CPU pixel buffers -> single-mip .ztxtr).
			std::vector<uint8_t> xPx;
			RT_NormalMapPixels(128, xPx);
			Zenith_Tools_TextureExport::ExportFromData(xPx.data(), NormalTexturePath(), 128, 128, TEXTURE_FORMAT_RGBA8_UNORM);
			RT_CheckerColourPixels(64, xPx);
			Zenith_Tools_TextureExport::ExportFromData(xPx.data(), CheckerTexturePath(), 64, 64, TEXTURE_FORMAT_RGBA8_UNORM);
			RT_CheckerAlphaPixels(64, xPx);
			Zenith_Tools_TextureExport::ExportFromData(xPx.data(), CutoutTexturePath(), 64, 64, TEXTURE_FORMAT_RGBA8_UNORM);

			// Shape meshes (segment counts match the old runtime CreateUnit* calls).
			ExportShapeMesh(ShapeId::Sphere, 32);
			ExportShapeMesh(ShapeId::Cube, 0);
			ExportShapeMesh(ShapeId::Cylinder, 32);
			ExportShapeMesh(ShapeId::Cone, 28);
			ExportShapeMesh(ShapeId::Capsule, 24);

			// Shared brushed-gold instance parent (SetParent target for the instance
			// children). Saved BEFORE the recipe so the children resolve it by path.
			Zenith_MaterialAsset* pxParent = NewMat("Showcase_InstanceParent");
			SetV(pxParent, MATERIAL_PARAM_BASE_COLOR, 0.9f, 0.75f, 0.35f, 1.0f);
			SetF(pxParent, MATERIAL_PARAM_METALLIC, 1.0f);
			SetF(pxParent, MATERIAL_PARAM_ROUGHNESS, 0.45f);
			pxParent->SaveToFile(InstanceParentMaterialPath());

			// Platform slab material + model (unit cube mesh; sized by the entity scale).
			Zenith_MaterialAsset* pxPlatMat = NewMat("Showcase_PlatformMat");
			SetV(pxPlatMat, MATERIAL_PARAM_BASE_COLOR, 0.32f, 0.33f, 0.35f, 1.0f);
			SetF(pxPlatMat, MATERIAL_PARAM_ROUGHNESS, 0.8f);
			pxPlatMat->SaveToFile(PlatformMaterialPath());
			{
				Zenith_Vector<std::string> xMats;
				xMats.PushBack(PlatformMaterialPath());
				Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
				pxModel->SetName("MatShowcase_Platform");
				pxModel->AddMeshByPath(ShapeMeshPath(ShapeId::Cube), xMats);
				pxModel->Export(PlatformModelPath().c_str());
				ModelHandle xHandle;
				xHandle.Set(pxModel);
				g_axModels.push_back(xHandle);
			}
		}

		// Record cell i (shape + panel + stable model path) and return its index.
		auto Record = [&](ShapeId eShape, bool bPanel) -> int
		{
			const int iCell = static_cast<int>(g_axCells.size());
			CellMeta xMeta;
			xMeta.m_eShape = eShape;
			xMeta.m_bPanel = bPanel;
			xMeta.m_strModelPath = CellModelPath(iCell);
			g_axCells.push_back(std::move(xMeta));
			return iCell;
		};
		// Save cell iCell's material + bundling model (write path only).
		auto SaveCell = [&](int iCell, ShapeId eShape, Zenith_MaterialAsset* m)
		{
			const std::string strMatPath = CellMaterialPath(iCell);
			m->SaveToFile(strMatPath);
			BundleCellModel(iCell, eShape, strMatPath);
		};

		// ---- Row 0: dielectric roughness ladder (5) + metallic roughness ladder (5) ----
		const float afRough[5] = { 0.03f, 0.27f, 0.5f, 0.74f, 1.0f };
		for (int i = 0; i < 5; i++)
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_DielRough"); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.85f, 0.85f, 0.87f, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, afRough[i]); SaveCell(c, ShapeId::Sphere, m); }
		}
		for (int i = 0; i < 5; i++)
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_MetalRough"); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.72f, 0.74f, 0.78f, 1.0f); SetF(m, MATERIAL_PARAM_METALLIC, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, afRough[i]); SaveCell(c, ShapeId::Sphere, m); }
		}

		// ---- Row 1: coloured metals (6) + specular ladder (4) ----
		struct Col { float r, g, b; };
		const Col axMetals[6] = { {1.0f,0.78f,0.34f},{0.95f,0.64f,0.54f},{0.96f,0.96f,0.97f},{0.92f,0.70f,0.78f},{0.40f,0.55f,0.95f},{0.35f,0.82f,0.50f} };
		for (int i = 0; i < 6; i++)
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Metal"); SetV(m, MATERIAL_PARAM_BASE_COLOR, axMetals[i].r, axMetals[i].g, axMetals[i].b, 1.0f); SetF(m, MATERIAL_PARAM_METALLIC, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.16f); SaveCell(c, ShapeId::Sphere, m); }
		}
		const float afSpec[4] = { 0.0f, 0.35f, 0.7f, 1.0f };
		for (int i = 0; i < 4; i++)
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Specular"); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.20f, 0.42f, 0.82f, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.22f); SetF(m, MATERIAL_PARAM_SPECULAR, afSpec[i]); SaveCell(c, ShapeId::Sphere, m); }
		}

		// ---- Row 2: clear coat (4, capsules) + HDR emissive (6, cones) ----
		const Col axCoat[4] = { {0.6f,0.05f,0.05f},{0.05f,0.1f,0.5f},{0.02f,0.02f,0.02f},{0.7f,0.35f,0.02f} };
		for (int i = 0; i < 4; i++)
		{
			const int c = Record(ShapeId::Capsule, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_ClearCoat"); SetV(m, MATERIAL_PARAM_BASE_COLOR, axCoat[i].r, axCoat[i].g, axCoat[i].b, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.75f); SetF(m, MATERIAL_PARAM_CLEARCOAT_STRENGTH, 1.0f); SetF(m, MATERIAL_PARAM_CLEARCOAT_ROUGHNESS, 0.05f); SaveCell(c, ShapeId::Capsule, m); }
		}
		struct Em { float r, g, b, i; };
		const Em axEm[6] = { {1,0.1f,0.1f,6},{0.1f,1,0.2f,6},{0.2f,0.4f,1,6},{0.1f,0.9f,1,10},{1,0.2f,1,10},{1,1,1,14} };
		for (int i = 0; i < 6; i++)
		{
			const int c = Record(ShapeId::Cone, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Emissive"); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.02f, 0.02f, 0.02f, 1.0f); SetV(m, MATERIAL_PARAM_EMISSIVE_COLOR, axEm[i].r, axEm[i].g, axEm[i].b, 0.0f); SetF(m, MATERIAL_PARAM_EMISSIVE_INTENSITY, axEm[i].i); SaveCell(c, ShapeId::Cone, m); }
		}

		// ---- Row 3: unlit (3) + normal-mapped (4) + textured/detail (3) ----
		const Col axUnlit[3] = { {0.9f,0.15f,0.15f},{0.15f,0.85f,0.25f},{0.2f,0.35f,0.95f} };
		for (int i = 0; i < 3; i++)
		{
			const int c = Record(ShapeId::Cube, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Unlit"); SetI(m, MATERIAL_PARAM_SHADING_MODEL, MATERIAL_SHADING_UNLIT); SetV(m, MATERIAL_PARAM_BASE_COLOR, axUnlit[i].r, axUnlit[i].g, axUnlit[i].b, 1.0f); SaveCell(c, ShapeId::Cube, m); }
		}
		const float afNrm[4] = { 0.6f, 1.0f, 1.6f, 2.2f };
		for (int i = 0; i < 4; i++)
		{
			const ShapeId eShape = (i < 2) ? ShapeId::Cube : ShapeId::Cylinder;
			const int c = Record(eShape, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Normal"); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.55f, 0.55f, 0.6f, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.35f); m->SetTexture(MATERIAL_TEXTURE_NORMAL, TextureHandle(NormalTexturePath())); SetF(m, MATERIAL_PARAM_NORMAL_STRENGTH, afNrm[i]); SaveCell(c, eShape, m); }
		}
		{
			const int c = Record(ShapeId::Cube, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TexChecker"); m->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(CheckerTexturePath())); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.4f); SaveCell(c, ShapeId::Cube, m); }
		}
		{
			const int c = Record(ShapeId::Cube, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TexCheckerTiled"); m->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(CheckerTexturePath())); SetV(m, MATERIAL_PARAM_UV_TILING, 3.0f, 3.0f, 0.0f, 0.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.4f); SaveCell(c, ShapeId::Cube, m); }
		}
		{
			const int c = Record(ShapeId::Cylinder, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TexNormalCombo"); m->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(CheckerTexturePath())); m->SetTexture(MATERIAL_TEXTURE_NORMAL, TextureHandle(NormalTexturePath())); SetF(m, MATERIAL_PARAM_NORMAL_STRENGTH, 1.4f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.3f); SetF(m, MATERIAL_PARAM_METALLIC, 0.5f); SaveCell(c, ShapeId::Cylinder, m); }
		}

		// ---- Row 4: cutout (2 panels) + translucent (2) + additive (1) + instances (parent + 3) + two-sided (1 panel) ----
		{
			const int c = Record(ShapeId::Cube, true);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Cutout50"); SetI(m, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_MASKED); SetF(m, MATERIAL_PARAM_ALPHA_CUTOFF, 0.5f); m->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(CutoutTexturePath())); SaveCell(c, ShapeId::Cube, m); }
		}
		{
			const int c = Record(ShapeId::Cube, true);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Cutout30"); SetI(m, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_MASKED); SetF(m, MATERIAL_PARAM_ALPHA_CUTOFF, 0.3f); m->SetTexture(MATERIAL_TEXTURE_BASE_COLOR, TextureHandle(CutoutTexturePath())); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.9f, 0.7f, 0.2f, 1.0f); SaveCell(c, ShapeId::Cube, m); }
		}
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TransCyan"); SetI(m, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_TRANSLUCENT); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.1f, 0.7f, 0.9f, 0.4f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.1f); SaveCell(c, ShapeId::Sphere, m); }
		}
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TransOrange"); SetI(m, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_TRANSLUCENT); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.95f, 0.45f, 0.1f, 0.55f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.15f); SaveCell(c, ShapeId::Sphere, m); }
		}
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_Additive"); SetI(m, MATERIAL_PARAM_BLEND_MODE, MATERIAL_BLEND_ADDITIVE); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.2f, 0.8f, 1.0f, 0.6f); SetV(m, MATERIAL_PARAM_EMISSIVE_COLOR, 0.2f, 0.8f, 1.0f, 0.0f); SetF(m, MATERIAL_PARAM_EMISSIVE_INTENSITY, 3.0f); SaveCell(c, ShapeId::Sphere, m); }
		}
		// Instance parent CELL (shown as gold sphere): bundle the shared parent material.
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { BundleCellModel(c, ShapeId::Sphere, InstanceParentMaterialPath()); }
		}
		// Three instance children: parent the shared gold material, override one prop each.
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_InstColour"); m->SetParent(MaterialHandle(InstanceParentMaterialPath())); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.85f, 0.1f, 0.12f, 1.0f); m->SetOverride(MATERIAL_PARAM_BASE_COLOR, true); SaveCell(c, ShapeId::Sphere, m); }
		}
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_InstSmooth"); m->SetParent(MaterialHandle(InstanceParentMaterialPath())); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.04f); m->SetOverride(MATERIAL_PARAM_ROUGHNESS, true); SaveCell(c, ShapeId::Sphere, m); }
		}
		{
			const int c = Record(ShapeId::Sphere, false);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_InstEmissive"); m->SetParent(MaterialHandle(InstanceParentMaterialPath())); SetV(m, MATERIAL_PARAM_EMISSIVE_COLOR, 1.0f, 0.3f, 0.05f, 0.0f); SetF(m, MATERIAL_PARAM_EMISSIVE_INTENSITY, 6.0f); m->SetOverride(MATERIAL_PARAM_EMISSIVE_COLOR, true); m->SetOverride(MATERIAL_PARAM_EMISSIVE_INTENSITY, true); SaveCell(c, ShapeId::Sphere, m); }
		}
		{
			const int c = Record(ShapeId::Cube, true);
			if (bWrite) { Zenith_MaterialAsset* m = NewMat("Showcase_TwoSided"); SetI(m, MATERIAL_PARAM_TWO_SIDED, 1); SetV(m, MATERIAL_PARAM_BASE_COLOR, 0.9f, 0.2f, 0.6f, 1.0f); SetF(m, MATERIAL_PARAM_ROUGHNESS, 0.4f); SaveCell(c, ShapeId::Cube, m); }
		}

		// Grid layout / extents (row-major; columns derived from the recorded count).
		const int iN = static_cast<int>(g_axCells.size());
		const int iColumns = (iN + iROWS - 1) / iROWS;
		RenderTest_Showcase::g_iColumns = iColumns;
		const float fGridW = (iColumns - 1) * fCOL_SPACING;
		RenderTest_Showcase::g_fGridMinX = fPLATFORM_CX - fGridW * 0.5f;
		RenderTest_Showcase::g_fGridMaxX = fPLATFORM_CX + fGridW * 0.5f;

		Zenith_Log(LOG_CATEGORY_MESH, "[Showcase] %s %d material cells in %dx%d grid",
			bWrite ? "exported" : "tabled", iN, iColumns, iROWS);
	}

	// Populate g_axCells + grid extents without touching disk. Idempotent — a no-op once
	// the table exists (RunRecipe(true) from the export already filled it in the normal
	// flow; this only fires under --skip-tool-exports, where the export was skipped).
	void EnsureCellTable()
	{
		if (g_axCells.empty())
		{
			RunRecipe(false);
		}
	}
}

void RenderTest_ExportMaterialShowcaseAssets()
{
	RunRecipe(/*bWrite=*/true);
}

void RenderTest_AuthorMaterialShowcase(Zenith_EditorAutomation& xAuto)
{
	EnsureCellTable();

	const int   iColumns = RenderTest_Showcase::g_iColumns;
	const float fGridW   = (iColumns - 1) * fCOL_SPACING;
	const float fGridD   = (iROWS - 1) * fROW_SPACING;

	// Platform slab: a wide flat cube, static OBB collider (the walkable floor).
	xAuto.AddStep_CreateEntity("MatShowcase_Platform");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(fPLATFORM_CX, fPLATFORM_TOP_Y - 0.5f, fPLATFORM_CZ);
	xAuto.AddStep_SetTransformScale(fGridW + 6.0f, 1.0f, fGridD + 6.0f);
	xAuto.AddStep_AddModel();
	xAuto.AddStep_LoadModel(PlatformModelPath().c_str());
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

	// The capsule mesh is 2.0 units tall at unit scale, so scale it by half of the
	// nominal shape scale to keep the same ~1.6 m footprint as the other shapes; the
	// explicit capsule collider (radius = half-height = 0.5 * that scale) then fits it.
	const float fCapScale = fSHAPE_SCALE * 0.5f;

	for (size_t i = 0; i < g_axCells.size(); i++)
	{
		const CellMeta& xC = g_axCells[i];
		const int iCol = static_cast<int>(i) % iColumns;
		const int iRow = static_cast<int>(i) / iColumns;
		const float fX = fPLATFORM_CX + (iCol - (iColumns - 1) * 0.5f) * fCOL_SPACING;
		const float fZ = fPLATFORM_CZ + (iRow - (iROWS - 1) * 0.5f) * fROW_SPACING;

		char szName[64];
		std::snprintf(szName, sizeof(szName), "MatShowcase_Cell_%02zu", i);
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(fX, fPLATFORM_TOP_Y + fSHAPE_SCALE * 0.5f, fZ);

		if (xC.m_eShape == ShapeId::Capsule)
		{
			xAuto.AddStep_SetTransformScale(fCapScale, fCapScale, fCapScale);
		}
		else if (xC.m_bPanel)
		{
			// Upright thin panel: cutout holes + two-sided are clearest on a flat face.
			xAuto.AddStep_SetTransformScale(fSHAPE_SCALE, fSHAPE_SCALE, 0.08f);
		}
		else
		{
			xAuto.AddStep_SetTransformScale(fSHAPE_SCALE, fSHAPE_SCALE, fSHAPE_SCALE);
		}

		xAuto.AddStep_AddModel();
		xAuto.AddStep_LoadModel(xC.m_strModelPath.c_str());
		xAuto.AddStep_AddCollider();

		// Static per-primitive collider: sphere -> SPHERE, capsule -> explicit CAPSULE
		// (radius = half-height = 0.5 * scale), everything else (cube / cone / cylinder /
		// panel) -> OBB sized from the mesh bounds.
		switch (xC.m_eShape)
		{
		case ShapeId::Sphere:
			xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_STATIC);
			break;
		case ShapeId::Capsule:
			xAuto.AddStep_AddCapsuleCollider(0.5f * fCapScale, 0.5f * fCapScale, RIGIDBODY_TYPE_STATIC);
			break;
		default:
			xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			break;
		}
	}
}
#endif // ZENITH_TOOLS

void RenderTest_MaterialShowcaseShutdown()
{
	// Release the export-time material/model handles while the AssetRegistry is still
	// alive (mirrors RenderTest_TennisShutdown). Empty (no-op) in non-tools builds.
	g_axMaterials.clear();
	g_axModels.clear();
	g_axCells.clear();
}
