#include "Zenith.h"
#include "RenderTest/RenderTest_Guns.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_CommandLine.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_Types.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "RenderTest/Components/RenderTest_GunComponent.h"

#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdio>

using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// Procedural gun meshes (vertex-coloured boxes, like the tennis racket) + the
// per-type specs + the spawn. Everything is built at runtime and leaked for the
// session (the procedural-game convention; the geometry must outlive the
// component). Windowed-only — procedural Flux_MeshGeometry is GPU-dependent.
//=============================================================================
namespace
{
	// Session-lifetime owners so attached geometry/material stay alive.
	std::vector<Flux_MeshGeometry*> g_apxGeoms;
	std::vector<MaterialHandle>     g_axMaterials;

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

	template <typename T>
	T* RT_Alloc(uint32_t uCount)
	{
		return static_cast<T*>(Zenith_MemoryManagement::Allocate(uCount * sizeof(T)));
	}

	// Minimal accumulating box-mesh builder. AddBox mirrors GenerateUnitCube's
	// per-face winding + outward normals so faces survive back-face culling and
	// light correctly (see the lit-primitive winding note — normals must point
	// out, not just be commented as such). UVs are unused (guns are vertex-coloured;
	// material base = white).
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

	// Palette.
	const Vector4 GUNMETAL(0.10f, 0.10f, 0.12f, 1.0f);
	const Vector4 STEEL   (0.22f, 0.23f, 0.26f, 1.0f);
	const Vector4 WOOD    (0.34f, 0.20f, 0.09f, 1.0f);
	const Vector4 POLYMER (0.06f, 0.06f, 0.07f, 1.0f);
	const Vector4 ACCENT  (0.55f, 0.45f, 0.10f, 1.0f);

	// Each gun is built grip-at-origin, +Z = barrel forward, +Y = up. The right
	// hand holds the origin (the attachment mount is identity). Boxes are sized so
	// the muzzle/foregrip points in the GunSpec land on the geometry.

	Flux_MeshGeometry* RT_BuildPistol()
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.020f, -0.115f, -0.055f }, { 0.020f, 0.005f, 0.005f }, POLYMER);   // grip (below origin, back)
		xB.AddBox({ -0.022f,  0.005f, -0.070f }, { 0.022f, 0.050f, 0.150f }, GUNMETAL);  // slide/frame (above origin, forward)
		xB.AddBox({ -0.012f,  0.012f,  0.150f }, { 0.012f, 0.040f, 0.190f }, STEEL);     // barrel tip
		xB.AddBox({ -0.014f, -0.060f, -0.020f }, { 0.014f, 0.005f, 0.012f }, GUNMETAL);  // trigger guard underside
		return xB.Build();
	}

	Flux_MeshGeometry* RT_BuildSMG()
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.120f, -0.060f }, { 0.022f, 0.005f, 0.000f }, POLYMER);   // grip
		xB.AddBox({ -0.028f,  0.000f, -0.110f }, { 0.028f, 0.065f, 0.150f }, GUNMETAL);  // body/receiver
		xB.AddBox({ -0.014f,  0.018f,  0.150f }, { 0.014f, 0.050f, 0.250f }, STEEL);     // barrel + foregrip shroud
		xB.AddBox({ -0.020f, -0.150f,  0.010f }, { 0.020f, 0.000f, 0.055f }, POLYMER);   // magazine (forward of grip)
		xB.AddBox({ -0.026f,  0.065f, -0.090f }, { 0.026f, 0.085f, -0.020f }, GUNMETAL); // top rail/stock fold
		return xB.Build();
	}

	Flux_MeshGeometry* RT_BuildRifle()
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.120f, -0.100f }, { 0.022f, 0.005f, -0.030f }, POLYMER);  // pistol grip
		xB.AddBox({ -0.032f,  0.000f, -0.190f }, { 0.032f, 0.075f, 0.090f }, GUNMETAL);  // receiver
		xB.AddBox({ -0.028f, -0.020f, -0.320f }, { 0.028f, 0.060f, -0.190f }, POLYMER);  // stock
		xB.AddBox({ -0.016f,  0.018f,  0.090f }, { 0.016f, 0.052f, 0.300f }, STEEL);     // handguard
		xB.AddBox({ -0.011f,  0.026f,  0.300f }, { 0.011f, 0.046f, 0.430f }, GUNMETAL);  // barrel
		xB.AddBox({ -0.022f, -0.170f, -0.070f }, { 0.022f, 0.000f, 0.005f }, GUNMETAL);  // magazine
		xB.AddBox({ -0.026f,  0.075f, -0.150f }, { 0.026f, 0.098f, -0.060f }, ACCENT);   // optic/carry handle
		return xB.Build();
	}

	Flux_MeshGeometry* RT_BuildShotgun()
	{
		GunGeomBuilder xB;
		xB.AddBox({ -0.022f, -0.115f, -0.110f }, { 0.022f, 0.005f, -0.040f }, WOOD);     // grip
		xB.AddBox({ -0.030f,  0.000f, -0.200f }, { 0.030f, 0.070f, 0.060f }, GUNMETAL);  // receiver
		xB.AddBox({ -0.028f, -0.020f, -0.340f }, { 0.028f, 0.058f, -0.200f }, WOOD);     // stock
		xB.AddBox({ -0.018f,  0.026f,  0.060f }, { 0.018f, 0.060f, 0.460f }, GUNMETAL);  // barrel
		xB.AddBox({ -0.024f, -0.060f,  0.120f }, { 0.024f, -0.005f, 0.280f }, WOOD);     // pump foregrip (below barrel)
		return xB.Build();
	}

	Flux_MeshGeometry* RT_BuildGunMesh(RenderTest_Guns::GunType eType)
	{
		switch (eType)
		{
		case RenderTest_Guns::GunType::Pistol:  return RT_BuildPistol();
		case RenderTest_Guns::GunType::SMG:     return RT_BuildSMG();
		case RenderTest_Guns::GunType::Rifle:   return RT_BuildRifle();
		case RenderTest_Guns::GunType::Shotgun: return RT_BuildShotgun();
		default:                                return RT_BuildPistol();
		}
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

	// Parse the showcase + pose-tuning CLI args into RenderTest_GunTuning. Called
	// once from the spawn.
	void RT_ParseGunCLI()
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
}

const RenderTest_Guns::GunSpec& RenderTest_Guns::GetSpec(RenderTest_Guns::GunType eType)
{
	return SpecTable(eType);
}

//=============================================================================
// Spawn entry point
//=============================================================================
void RenderTest_SpawnGuns()
{
	if (Zenith_CommandLine::IsHeadless())
		return;   // procedural GPU meshes — windowed only.

	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Guns] no active scene; skipping gun spawn");
		return;
	}

	RT_ParseGunCLI();

	// Shared vertex-coloured material (white base; per-vertex colour shows through),
	// like the tennis racket material.
	MaterialHandle xGunMatHandle;
	xGunMatHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_axMaterials.push_back(xGunMatHandle);
	Zenith_MaterialAsset* pxGunMat = xGunMatHandle.GetDirect();
	pxGunMat->SetName("RenderTest_Gun");
	pxGunMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	pxGunMat->SetRoughness(0.45f);
	pxGunMat->SetMetallic(0.55f);

	// Spawn platform (CenterPlatform): centre (256, 48.5, 256), scale (30,0.5,30)
	// => top deck at Y = 48.75. Lay the guns in a row a few metres in front of the
	// player spawn (player faces +Z), clear of the IK step cubes (around Z 243/269).
	constexpr float fDeckTopY = 48.75f;
	constexpr float fRowZ = 261.0f;
	constexpr float fSpacing = 2.0f;
	const int iCount = static_cast<int>(RenderTest_Guns::GunType::COUNT);

	// Rest pose: lay each gun flat on its side. rotZ(+90deg) sends the gun's up
	// (+Y) to model +X and keeps the barrel (+Z) horizontal, so it rests on a side
	// face; lift by the half-width so it sits on the deck rather than through it.
	const Zenith_Maths::Quat xRestRot =
		glm::angleAxis(glm::radians(90.0f), Vector3(0.0f, 0.0f, 1.0f));
	constexpr float fRestLift = 0.05f;

	for (int i = 0; i < iCount; i++)
	{
		const RenderTest_Guns::GunType eType = static_cast<RenderTest_Guns::GunType>(i);
		const RenderTest_Guns::GunSpec& xSpec = RenderTest_Guns::GetSpec(eType);

		Flux_MeshGeometry* pxGeom = RT_BuildGunMesh(eType);

		char szName[64];
		std::snprintf(szName, sizeof(szName), "Gun_%s", xSpec.m_szName);

		const float fX = 256.0f + (static_cast<float>(i) - (iCount - 1) * 0.5f) * fSpacing;
		const Vector3 xPos(fX, fDeckTopY + fRestLift, fRowZ);

		Zenith_Entity xGun = g_xEngine.Scenes().CreateEntity(xScene, szName);
		Zenith_TransformComponent& xT = xGun.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(xPos);
		xT.SetRotation(xRestRot);

		Zenith_ModelComponent& xModel = xGun.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxGeom, *pxGunMat);

		// Attachment idle until the player picks it up.
		xGun.AddComponent<Zenith_AttachmentComponent>();

		RenderTest_GunComponent& xGunComp = xGun.AddComponent<RenderTest_GunComponent>();
		xGunComp.Init(xSpec);
	}

	// Showcase: the player auto-equips the chosen gun on its first update and
	// re-asserts a front photo camera every frame (RenderTest_PlayerComponent::
	// AssertShowcaseCamera) so the held pose + hand IK can be screenshotted —
	// nothing camera-related to set here.

	Zenith_Log(LOG_CATEGORY_CORE, "[Guns] spawned %d guns on the platform (Z=%.1f); showcase=%d",
		iCount, fRowZ, RenderTest_GunTuning::s_bShowcaseActive ? 1 : 0);
}

void RenderTest_GunsShutdown()
{
	// Drop the registry refcounts while the AssetRegistry is still alive (mirrors
	// RenderTest_TennisShutdown). The procedural Flux_MeshGeometry objects are
	// intentionally leaked for the session — not registry assets, so they don't
	// trip the asset-refcount assertion.
	g_axMaterials.clear();
}
