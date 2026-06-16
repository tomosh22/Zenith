#include "Zenith.h"
#include "RenderTest/RenderTest_Jetpack.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_CommandLine.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_Types.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "RenderTest/Components/RenderTest_JetpackComponent.h"

#include <vector>
#include <cstring>
#include <cstdlib>

using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// Procedural jetpack mesh (vertex-coloured boxes, like the guns + tennis
// racket) + the spawn that attaches it to the player's back. Everything is
// built at runtime and leaked for the session (the procedural-game convention;
// the geometry must outlive the component). Windowed-only — procedural
// Flux_MeshGeometry is GPU-dependent.
//=============================================================================
namespace
{
	// Session-lifetime owners so attached geometry/material stay alive.
	std::vector<Flux_MeshGeometry*> g_apxGeoms;
	std::vector<MaterialHandle>     g_axMaterials;

	// Read a "--prefix=<value>" float CLI arg if present, else return fDefault.
	float RT_JetArgFloat(const char* szPrefix, float fDefault)
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

	// Minimal accumulating box-mesh builder — identical to the guns'
	// GunGeomBuilder (per-face outward normals + winding mirroring
	// GenerateUnitCube so faces survive back-face culling and light correctly).
	struct JetGeomBuilder
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

		// Axis-aligned box [xMin,xMax], flat vertex colour.
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
	const Vector4 SHELL  (0.16f, 0.17f, 0.20f, 1.0f);   // dark frame/backplate
	const Vector4 TANK   (0.62f, 0.36f, 0.10f, 1.0f);   // orange fuel tanks
	const Vector4 STEEL  (0.30f, 0.32f, 0.36f, 1.0f);   // metal caps/struts
	const Vector4 NOZZLE (0.08f, 0.08f, 0.09f, 1.0f);   // scorched nozzle rims

	// Backpack built local: +Z = toward the wearer's back (mounting plane at z=0),
	// body extends -Z (behind the player), +Y up, -Y down (nozzles at the bottom).
	// Built so the two nozzle mouths land at (+/-0.085, -0.28, -0.10) — see the
	// Spec the spawn passes to RenderTest_JetpackComponent::Init.
	Flux_MeshGeometry* RT_BuildJetpackMesh()
	{
		JetGeomBuilder xB;
		// Backplate that sits flush against the spine.
		xB.AddBox({ -0.15f, -0.18f, -0.04f }, { 0.15f, 0.24f, 0.02f }, SHELL);
		// Two fuel tanks bulging off the back.
		xB.AddBox({ -0.145f, -0.20f, -0.22f }, { -0.025f, 0.22f, -0.04f }, TANK);   // left tank
		xB.AddBox({  0.025f, -0.20f, -0.22f }, {  0.145f, 0.22f, -0.04f }, TANK);   // right tank
		// Rounded top + bottom caps on the tanks (steel rings).
		xB.AddBox({ -0.150f, 0.22f, -0.225f }, { -0.020f, 0.27f, -0.035f }, STEEL); // left top cap
		xB.AddBox({  0.020f, 0.22f, -0.225f }, {  0.150f, 0.27f, -0.035f }, STEEL); // right top cap
		xB.AddBox({ -0.150f, -0.24f, -0.225f }, { -0.020f, -0.20f, -0.035f }, STEEL); // left bottom cap
		xB.AddBox({  0.020f, -0.24f, -0.225f }, {  0.150f, -0.20f, -0.035f }, STEEL); // right bottom cap
		// Cross strut linking the tanks.
		xB.AddBox({ -0.03f, 0.02f, -0.16f }, { 0.03f, 0.10f, -0.08f }, STEEL);
		// Two downward thruster nozzles (tapered boxes) — mouths at y=-0.28.
		xB.AddBox({ -0.115f, -0.28f, -0.135f }, { -0.055f, -0.24f, -0.065f }, NOZZLE); // left nozzle
		xB.AddBox({  0.055f, -0.28f, -0.135f }, {  0.115f, -0.24f, -0.065f }, NOZZLE); // right nozzle
		return xB.Build();
	}

	// Parse the showcase + mount-tuning CLI args into RenderTest_JetpackTuning.
	void RT_ParseJetpackCLI()
	{
#ifdef ZENITH_WINDOWS
		static const char* const szShowcasePrefix = "--rendertest-jetpack-showcase";
		const size_t ulShowcaseLen = std::strlen(szShowcasePrefix);
		for (int i = 1; i < __argc; i++)
		{
			if (std::strncmp(__argv[i], szShowcasePrefix, ulShowcaseLen) == 0)
				RenderTest_JetpackTuning::s_bShowcaseActive = true;
		}
#endif
		RenderTest_JetpackTuning::s_fMountX     = RT_JetArgFloat("--jetpack-mount-x=",     RenderTest_JetpackTuning::s_fMountX);
		RenderTest_JetpackTuning::s_fMountY     = RT_JetArgFloat("--jetpack-mount-y=",     RenderTest_JetpackTuning::s_fMountY);
		RenderTest_JetpackTuning::s_fMountZ     = RT_JetArgFloat("--jetpack-mount-z=",     RenderTest_JetpackTuning::s_fMountZ);
		RenderTest_JetpackTuning::s_fMountPitch = RT_JetArgFloat("--jetpack-mount-pitch=", RenderTest_JetpackTuning::s_fMountPitch);
		RenderTest_JetpackTuning::s_fMountYaw   = RT_JetArgFloat("--jetpack-mount-yaw=",   RenderTest_JetpackTuning::s_fMountYaw);
		RenderTest_JetpackTuning::s_fMountRoll  = RT_JetArgFloat("--jetpack-mount-roll=",  RenderTest_JetpackTuning::s_fMountRoll);
	}

	// Mount transform (bone-local): translate the backpack onto the upper back +
	// orient it. Default is a pure translation behind/above the Spine bone; the
	// --jetpack-mount-* knobs override each channel for screenshot calibration.
	Zenith_Maths::Matrix4 RT_BuildJetpackMount()
	{
		using namespace RenderTest_JetpackTuning;
		const float fX     = IsSet(s_fMountX)     ? s_fMountX     :  0.0f;
		const float fY     = IsSet(s_fMountY)     ? s_fMountY     :  0.18f;
		const float fZ     = IsSet(s_fMountZ)     ? s_fMountZ     : -0.14f;
		const float fPitch = IsSet(s_fMountPitch) ? s_fMountPitch :  0.0f;
		const float fYaw   = IsSet(s_fMountYaw)   ? s_fMountYaw   :  0.0f;
		const float fRoll  = IsSet(s_fMountRoll)  ? s_fMountRoll  :  0.0f;

		Zenith_Maths::Matrix4 xM(1.0f);
		xM = glm::translate(xM, Vector3(fX, fY, fZ));
		xM = glm::rotate(xM, glm::radians(fYaw),   Vector3(0.0f, 1.0f, 0.0f));
		xM = glm::rotate(xM, glm::radians(fPitch), Vector3(1.0f, 0.0f, 0.0f));
		xM = glm::rotate(xM, glm::radians(fRoll),  Vector3(0.0f, 0.0f, 1.0f));
		return xM;
	}
}

//=============================================================================
// Spawn entry point
//=============================================================================
void RenderTest_SpawnJetpack()
{
	if (Zenith_CommandLine::IsHeadless())
		return;   // procedural GPU mesh — windowed only.

	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	if (!xScene.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Jetpack] no active scene; skipping jetpack spawn");
		return;
	}
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (!pxSceneData)
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Jetpack] no scene data; skipping jetpack spawn");
		return;
	}

	Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
	if (!xPlayer.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Jetpack] no Player entity; skipping jetpack spawn");
		return;
	}

	RT_ParseJetpackCLI();

	// Shared vertex-coloured material (white base; per-vertex colour shows
	// through), like the guns / tennis racket.
	MaterialHandle xMatHandle;
	xMatHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_axMaterials.push_back(xMatHandle);
	Zenith_MaterialAsset* pxMat = xMatHandle.GetDirect();
	pxMat->SetName("RenderTest_Jetpack");
	pxMat->SetBaseColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	pxMat->SetRoughness(0.4f);
	pxMat->SetMetallic(0.6f);

	Flux_MeshGeometry* pxGeom = RT_BuildJetpackMesh();

	// Seat the jetpack at the player's position so frame 0 (before the Spine bone
	// resolves in the attachment's OnLateUpdate) isn't at the world origin.
	Zenith_Maths::Vector3 xPlayerPos;
	xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);

	Zenith_Entity xJetpack = g_xEngine.Scenes().CreateEntity(xScene, "Jetpack");
	xJetpack.GetComponent<Zenith_TransformComponent>().SetPosition(xPlayerPos);

	Zenith_ModelComponent& xModel = xJetpack.AddComponent<Zenith_ModelComponent>();
	xModel.AddMeshEntry(*pxGeom, *pxMat);

	// Permanently attach to the player's upper back. The Spine bone's bind frame
	// matches the model axes (identity bind rotation), so the mount is a simple
	// translate behind/above it (tunable via --jetpack-mount-*).
	Zenith_AttachmentComponent& xAttach = xJetpack.AddComponent<Zenith_AttachmentComponent>();
	xAttach.AttachToBone(xPlayer, "Spine", RT_BuildJetpackMount());

	// Jet-trail emitter. The config is created+registered in
	// InitializeRenderTestResources; start NOT emitting (the player turns it on
	// while thrusting). Position/direction are overridden per-frame by the player.
	Zenith_ParticleEmitterComponent& xEmitter = xJetpack.AddComponent<Zenith_ParticleEmitterComponent>();
	if (Flux_ParticleEmitterConfig* pxConfig = Flux_ParticleEmitterConfig::Find("RenderTest_JetTrail"))
	{
		xEmitter.SetConfig(pxConfig);
	}
	else
	{
		Zenith_Warning(LOG_CATEGORY_CORE, "[Jetpack] RenderTest_JetTrail config not found; trail disabled");
	}
	xEmitter.SetEmitting(false);

	// Exhaust geometry matching the two drawn nozzle mouths (jetpack-local).
	RenderTest_JetpackComponent::Spec xSpec;
	xSpec.m_axNozzleLocal[0] = Vector3(-0.085f, -0.28f, -0.10f);
	xSpec.m_axNozzleLocal[1] = Vector3( 0.085f, -0.28f, -0.10f);
	xSpec.m_xExhaustLocalDir = Vector3(0.0f, -1.0f, -0.28f);
	xJetpack.AddComponent<RenderTest_JetpackComponent>().Init(xSpec);

	Zenith_Log(LOG_CATEGORY_CORE, "[Jetpack] spawned + attached to Player Spine; showcase=%d",
		RenderTest_JetpackTuning::s_bShowcaseActive ? 1 : 0);
}

void RenderTest_JetpackShutdown()
{
	// Drop the registry refcount while the AssetRegistry is still alive (mirrors
	// RenderTest_GunsShutdown). The procedural Flux_MeshGeometry is intentionally
	// leaked for the session.
	g_axMaterials.clear();
}
