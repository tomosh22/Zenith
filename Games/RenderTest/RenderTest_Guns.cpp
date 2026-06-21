#include "Zenith.h"
#include "RenderTest/RenderTest_Guns.h"

#include "Core/Zenith_CommandLine.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <filesystem>

using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// FPS gun pickup/drop testbed. The four gun meshes are now baked OFFLINE (tools)
// into CPU Zenith_MeshAssets + bundling .zmodels on disk; the authored scene
// loads those models and the runtime pickup/drop bind happens via the serialized
// Zenith_AttachmentComponent. (Previously each mesh was built as a runtime GPU
// Flux_MeshGeometry and spawned post-load — that path is gone.)
//=============================================================================
namespace
{
	// Session-lifetime owner for the exported model asset handles, so they outlive
	// the export and are released cleanly at shutdown.
	std::vector<ModelHandle> g_axModels;

	// Read a "--prefix=<value>" float CLI arg if present, else return fDefault.
	float RT_GunArgFloat(const char* szPrefix, float fDefault)
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

	// Minimal accumulating box-mesh builder — identical to the jetpack's
	// JetGeomBuilder (per-face outward normals + winding mirroring GenerateUnitCube
	// so faces survive back-face culling and light correctly). UVs are unused (guns
	// are vertex-coloured; material base = white).
	struct GunGeomBuilder
	{
		std::vector<Vector3> m_xPos, m_xNrm, m_xTan, m_xBit;
		std::vector<Vector2> m_xUV;
		std::vector<Vector4> m_xCol;
		std::vector<uint32_t> m_xIdx;

		void AddQuad(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3,
			const Vector3& xNormal, const Vector4& xColor)
		{
			const uint32_t uBase = static_cast<uint32_t>(m_xPos.size());
			Vector3 xTangent = p1 - p0;
			const float fLen = glm::length(xTangent);
			xTangent = (fLen > 1e-6f) ? (xTangent / fLen) : Vector3(1.0f, 0.0f, 0.0f);
			const Vector3 xBitangent = glm::cross(xNormal, xTangent);
			auto Push = [&](const Vector3& p)
			{
				m_xPos.push_back(p);
				m_xNrm.push_back(xNormal);
				m_xTan.push_back(xTangent);
				m_xBit.push_back(xBitangent);
				m_xUV.push_back(Vector2(0.0f, 0.0f));
				m_xCol.push_back(xColor);
			};
			Push(p0); Push(p1); Push(p2); Push(p3);
			m_xIdx.push_back(uBase + 0); m_xIdx.push_back(uBase + 2); m_xIdx.push_back(uBase + 1);
			m_xIdx.push_back(uBase + 1); m_xIdx.push_back(uBase + 2); m_xIdx.push_back(uBase + 3);
		}

		// Axis-aligned box [xMin,xMax], flat vertex colour. Face winding/normals
		// copy RT_AddBox in RenderTest_Tennis.cpp (mirrors GenerateUnitCube).
		void AddBox(const Vector3& xMin, const Vector3& xMax, const Vector4& xColor)
		{
			const float x0 = xMin.x, y0 = xMin.y, z0 = xMin.z, x1 = xMax.x, y1 = xMax.y, z1 = xMax.z;
			AddQuad({ x0,y0,z1 }, { x1,y0,z1 }, { x0,y1,z1 }, { x1,y1,z1 }, { 0,0,1 }, xColor);    // +Z
			AddQuad({ x1,y0,z0 }, { x0,y0,z0 }, { x1,y1,z0 }, { x0,y1,z0 }, { 0,0,-1 }, xColor);   // -Z
			AddQuad({ x0,y1,z1 }, { x1,y1,z1 }, { x0,y1,z0 }, { x1,y1,z0 }, { 0,1,0 }, xColor);    // +Y
			AddQuad({ x0,y0,z0 }, { x1,y0,z0 }, { x0,y0,z1 }, { x1,y0,z1 }, { 0,-1,0 }, xColor);   // -Y
			AddQuad({ x1,y0,z1 }, { x1,y0,z0 }, { x1,y1,z1 }, { x1,y1,z0 }, { 1,0,0 }, xColor);    // +X
			AddQuad({ x0,y0,z0 }, { x0,y0,z1 }, { x0,y1,z0 }, { x0,y1,z1 }, { -1,0,0 }, xColor);   // -X
		}

		// Drain the accumulated vertices/indices into a CPU Zenith_MeshAsset for
		// offline export (no GPU upload). AddVertex doesn't take a bitangent, so the
		// analytic bitangent is pushed in parallel to keep all six arrays the same
		// length (matches Zenith_MeshAsset::GenerateUnitSphere).
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

	// Palette.
	const Vector4 GUNMETAL(0.10f, 0.10f, 0.12f, 1.0f);
	const Vector4 STEEL   (0.22f, 0.23f, 0.26f, 1.0f);
	const Vector4 WOOD    (0.34f, 0.20f, 0.09f, 1.0f);
	const Vector4 POLYMER (0.06f, 0.06f, 0.07f, 1.0f);
	const Vector4 ACCENT  (0.55f, 0.45f, 0.10f, 1.0f);

	// Each gun is built grip-at-origin, +Z = barrel forward, +Y = up. The right
	// hand holds the origin (the attachment mount is identity). Boxes are sized so
	// the muzzle/foregrip points in the GunSpec land on the geometry.

	void RT_BuildPistol(Zenith_MeshAsset& xOut)
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.020f, -0.115f, -0.055f }, { 0.020f, 0.005f, 0.005f }, POLYMER);   // grip (below origin, back)
		xB.AddBox({ -0.022f,  0.005f, -0.070f }, { 0.022f, 0.050f, 0.150f }, GUNMETAL);  // slide/frame (above origin, forward)
		xB.AddBox({ -0.012f,  0.012f,  0.150f }, { 0.012f, 0.040f, 0.190f }, STEEL);     // barrel tip
		xB.AddBox({ -0.014f, -0.060f, -0.020f }, { 0.014f, 0.005f, 0.012f }, GUNMETAL);  // trigger guard underside
		xB.BuildAsset(xOut);
	}

	void RT_BuildSMG(Zenith_MeshAsset& xOut)
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.120f, -0.060f }, { 0.022f, 0.005f, 0.000f }, POLYMER);   // grip
		xB.AddBox({ -0.028f,  0.000f, -0.110f }, { 0.028f, 0.065f, 0.150f }, GUNMETAL);  // body/receiver
		xB.AddBox({ -0.014f,  0.018f,  0.150f }, { 0.014f, 0.050f, 0.250f }, STEEL);     // barrel + foregrip shroud
		xB.AddBox({ -0.020f, -0.150f,  0.010f }, { 0.020f, 0.000f, 0.055f }, POLYMER);   // magazine (forward of grip)
		xB.AddBox({ -0.026f,  0.065f, -0.090f }, { 0.026f, 0.085f, -0.020f }, GUNMETAL); // top rail/stock fold
		xB.BuildAsset(xOut);
	}

	void RT_BuildRifle(Zenith_MeshAsset& xOut)
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.120f, -0.100f }, { 0.022f, 0.005f, -0.030f }, POLYMER);  // pistol grip
		xB.AddBox({ -0.032f,  0.000f, -0.190f }, { 0.032f, 0.075f, 0.090f }, GUNMETAL);  // receiver
		xB.AddBox({ -0.028f, -0.020f, -0.320f }, { 0.028f, 0.060f, -0.190f }, POLYMER);  // stock
		xB.AddBox({ -0.016f,  0.018f,  0.090f }, { 0.016f, 0.052f, 0.300f }, STEEL);     // handguard
		xB.AddBox({ -0.011f,  0.026f,  0.300f }, { 0.011f, 0.046f, 0.430f }, GUNMETAL);  // barrel
		xB.AddBox({ -0.022f, -0.170f, -0.070f }, { 0.022f, 0.000f, 0.005f }, GUNMETAL);  // magazine
		xB.AddBox({ -0.026f,  0.075f, -0.150f }, { 0.026f, 0.098f, -0.060f }, ACCENT);   // optic/carry handle
		xB.BuildAsset(xOut);
	}

	void RT_BuildShotgun(Zenith_MeshAsset& xOut)
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.115f, -0.110f }, { 0.022f, 0.005f, -0.040f }, WOOD);     // grip
		xB.AddBox({ -0.030f,  0.000f, -0.200f }, { 0.030f, 0.070f, 0.060f }, GUNMETAL);  // receiver
		xB.AddBox({ -0.028f, -0.020f, -0.340f }, { 0.028f, 0.058f, -0.200f }, WOOD);     // stock
		xB.AddBox({ -0.018f,  0.026f,  0.060f }, { 0.018f, 0.060f, 0.460f }, GUNMETAL);  // barrel
		xB.AddBox({ -0.024f, -0.060f,  0.120f }, { 0.024f, -0.005f, 0.280f }, WOOD);     // pump foregrip (below barrel)
		xB.BuildAsset(xOut);
	}

	// Fill a CPU Zenith_MeshAsset with the per-type gun geometry (no GPU upload).
	void RT_BuildGunMeshAsset(RenderTest_Guns::GunType eType, Zenith_MeshAsset& xOut)
	{
		switch (eType)
		{
		case RenderTest_Guns::GunType::Pistol:  RT_BuildPistol(xOut);  break;
		case RenderTest_Guns::GunType::SMG:     RT_BuildSMG(xOut);     break;
		case RenderTest_Guns::GunType::Rifle:   RT_BuildRifle(xOut);   break;
		case RenderTest_Guns::GunType::Shotgun: RT_BuildShotgun(xOut); break;
		default:                                RT_BuildPistol(xOut);  break;
		}
	}

	// Stable per-type name fragment used to build the deterministic asset paths.
	const char* RT_GunTypeFileName(RenderTest_Guns::GunType eType)
	{
		switch (eType)
		{
		case RenderTest_Guns::GunType::Pistol:  return "Pistol";
		case RenderTest_Guns::GunType::SMG:     return "SMG";
		case RenderTest_Guns::GunType::Rifle:   return "Rifle";
		case RenderTest_Guns::GunType::Shotgun: return "Shotgun";
		default:                                return "Pistol";
		}
	}

	// Deterministic on-disk paths for each gun's assets (GAME_ASSETS_DIR-relative,
	// like EnsureUnitCubeModelExists). Function-local statics give stable storage
	// whose c_str() is safe to hand to AddStep_LoadModel.
	const std::string& GunMeshPath(RenderTest_Guns::GunType eType)
	{
		static const std::string s_aPaths[static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT)] = {
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Pistol"  ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_SMG"     ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Rifle"   ZENITH_MESH_ASSET_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Shotgun" ZENITH_MESH_ASSET_EXT,
		};
		const uint32_t u = static_cast<uint32_t>(eType);
		return s_aPaths[(u < static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT)) ? u : 0];
	}
	const std::string& GunModelPathStr(RenderTest_Guns::GunType eType)
	{
		static const std::string s_aPaths[static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT)] = {
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Pistol"  ZENITH_MODEL_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_SMG"     ZENITH_MODEL_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Rifle"   ZENITH_MODEL_EXT,
			std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Gun_Shotgun" ZENITH_MODEL_EXT,
		};
		const uint32_t u = static_cast<uint32_t>(eType);
		return s_aPaths[(u < static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT)) ? u : 0];
	}

	// Static spec table. Anchors are in the player's model space; the reachability
	// budget (right shoulder at model (0.3,1.1,0), left shoulder at (-0.3,1.1,0),
	// arm reach ~0.7) is verified per type in RenderTest_Guns.h's overview.
	RenderTest_Guns::GunSpec MakeSpec(RenderTest_Guns::GunType eType)
	{
		using GT = RenderTest_Guns::GunType;
		RenderTest_Guns::GunSpec x;
		x.m_eType = eType;
		switch (eType)
		{
		case GT::Pistol:
			x.m_szName = "Pistol";
			x.m_bTwoHanded = false;
			x.m_xHoldAnchorModel = Vector3(0.12f, 1.18f, 0.52f);
			x.m_xMuzzleLocal = Vector3(0.0f, 0.026f, 0.19f);
			x.m_uMagSize = 12; x.m_uReserve = 48; x.m_fFireInterval = 0.20f;
			break;
		case GT::SMG:
			x.m_szName = "SMG";
			x.m_bTwoHanded = true;
			x.m_xHoldAnchorModel = Vector3(0.02f, 1.16f, 0.40f);
			x.m_xForegripLocal = Vector3(0.0f, 0.0f, 0.20f);
			x.m_xMuzzleLocal = Vector3(0.0f, 0.034f, 0.25f);
			x.m_uMagSize = 30; x.m_uReserve = 120; x.m_fFireInterval = 0.08f;
			break;
		case GT::Rifle:
			x.m_szName = "Rifle";
			x.m_bTwoHanded = true;
			x.m_xHoldAnchorModel = Vector3(0.02f, 1.20f, 0.42f);
			x.m_xForegripLocal = Vector3(0.0f, 0.02f, 0.18f);
			x.m_xMuzzleLocal = Vector3(0.0f, 0.036f, 0.43f);
			x.m_uMagSize = 30; x.m_uReserve = 120; x.m_fFireInterval = 0.11f;
			break;
		case GT::Shotgun:
			x.m_szName = "Shotgun";
			x.m_bTwoHanded = true;
			x.m_xHoldAnchorModel = Vector3(0.02f, 1.20f, 0.42f);
			x.m_xForegripLocal = Vector3(0.0f, -0.03f, 0.20f);
			x.m_xMuzzleLocal = Vector3(0.0f, 0.040f, 0.46f);
			x.m_uMagSize = 6; x.m_uReserve = 36; x.m_fFireInterval = 0.55f;
			break;
		default: break;
		}
		return x;
	}

	const RenderTest_Guns::GunSpec& SpecTable(RenderTest_Guns::GunType eType)
	{
		static const RenderTest_Guns::GunSpec s_axSpecs[] = {
			MakeSpec(RenderTest_Guns::GunType::Pistol),
			MakeSpec(RenderTest_Guns::GunType::SMG),
			MakeSpec(RenderTest_Guns::GunType::Rifle),
			MakeSpec(RenderTest_Guns::GunType::Shotgun),
		};
		const uint32_t u = static_cast<uint32_t>(eType);
		return s_axSpecs[(u < static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT)) ? u : 0];
	}
}

const RenderTest_Guns::GunSpec& RenderTest_Guns::GetSpec(RenderTest_Guns::GunType eType)
{
	return SpecTable(eType);
}

// Parse the showcase + pose-tuning CLI args into RenderTest_GunTuning. Called by
// the RenderTest bootstrap component in OnAwake (previously inlined in the
// now-deleted runtime spawn).
void RenderTest_ParseGunCLI()
{
#ifdef ZENITH_WINDOWS
	static const char* const szShowcasePrefix = "--rendertest-gun-showcase";
	const size_t ulShowcaseLen = std::strlen(szShowcasePrefix);
	for (int i = 1; i < __argc; i++)
	{
		if (std::strncmp(__argv[i], szShowcasePrefix, ulShowcaseLen) == 0)
		{
			RenderTest_GunTuning::s_bShowcaseActive = true;
			const char* p = __argv[i];
			// "floor" => spawn the guns, DON'T auto-equip, and frame the gun row
			// on the deck (sentinel == COUNT). Otherwise auto-equip the named type.
			RenderTest_GunTuning::s_uShowcaseType =
				std::strstr(p, "floor")   ? static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT) :
				std::strstr(p, "shotgun") ? static_cast<uint32_t>(RenderTest_Guns::GunType::Shotgun) :
				std::strstr(p, "rifle")   ? static_cast<uint32_t>(RenderTest_Guns::GunType::Rifle) :
				std::strstr(p, "smg")     ? static_cast<uint32_t>(RenderTest_Guns::GunType::SMG) :
				                            static_cast<uint32_t>(RenderTest_Guns::GunType::Pistol);
		}
	}
#endif
	RenderTest_GunTuning::s_fAnchorX     = RT_GunArgFloat("--gun-anchor-x=", RenderTest_GunTuning::s_fAnchorX);
	RenderTest_GunTuning::s_fAnchorY     = RT_GunArgFloat("--gun-anchor-y=", RenderTest_GunTuning::s_fAnchorY);
	RenderTest_GunTuning::s_fAnchorZ     = RT_GunArgFloat("--gun-anchor-z=", RenderTest_GunTuning::s_fAnchorZ);
	RenderTest_GunTuning::s_fAimPitchDeg = RT_GunArgFloat("--gun-aim-pitch=", RenderTest_GunTuning::s_fAimPitchDeg);
	RenderTest_GunTuning::s_fAimYawDeg   = RT_GunArgFloat("--gun-aim-yaw=",   RenderTest_GunTuning::s_fAimYawDeg);
	RenderTest_GunTuning::s_fAimRollDeg  = RT_GunArgFloat("--gun-aim-roll=",  RenderTest_GunTuning::s_fAimRollDeg);
	RenderTest_GunTuning::s_fForegripY   = RT_GunArgFloat("--gun-foregrip-y=", RenderTest_GunTuning::s_fForegripY);
	RenderTest_GunTuning::s_fForegripZ   = RT_GunArgFloat("--gun-foregrip-z=", RenderTest_GunTuning::s_fForegripZ);
}

const char* RenderTest_GunModelPath(RenderTest_Guns::GunType eType)
{
	return GunModelPathStr(eType).c_str();
}

//=============================================================================
// Tools asset export
//=============================================================================
#ifdef ZENITH_TOOLS
void RenderTest_ExportGunAssets(const char* szVtxColorMaterialPath)
{
	const uint32_t uCount = static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT);
	for (uint32_t u = 0; u < uCount; ++u)
	{
		const RenderTest_Guns::GunType eType = static_cast<RenderTest_Guns::GunType>(u);

		// CPU mesh -> .zasset (stack-local, no GPU; mirrors EnsureUnitCubeModelExists).
		std::filesystem::create_directories(std::filesystem::path(GunMeshPath(eType)).parent_path());

		Zenith_MeshAsset xMesh;
		RT_BuildGunMeshAsset(eType, xMesh);
		xMesh.Export(GunMeshPath(eType).c_str());

		// Bundle into a .zmodel referencing the shared vertex-colour material.
		// Overwrite every tools run so geometry edits propagate (the
		// EnsureStickFigureModelExists generation policy).
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		char szName[64];
		std::snprintf(szName, sizeof(szName), "RenderTest_Gun_%s", RT_GunTypeFileName(eType));
		pxModel->SetName(szName);
		Zenith_Vector<std::string> xMaterials;
		xMaterials.PushBack(szVtxColorMaterialPath);
		pxModel->AddMeshByPath(GunMeshPath(eType), xMaterials);
		pxModel->Export(GunModelPathStr(eType).c_str());

		ModelHandle xHandle;
		xHandle.Set(pxModel);
		g_axModels.push_back(xHandle);

		Zenith_Log(LOG_CATEGORY_MESH, "[Guns] exported %s", GunModelPathStr(eType).c_str());
	}
}
#endif

void RenderTest_GunsShutdown()
{
	// Release the export-time model handles while the AssetRegistry is still alive
	// (mirrors RenderTest_JetpackShutdown).
	g_axModels.clear();
}
