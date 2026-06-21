#include "Zenith.h"
#include "RenderTest/RenderTest_Jetpack.h"

#include "Core/Zenith_CommandLine.h"
#include "Maths/Zenith_Maths.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "RenderTest/RenderTest_Jetpack.h"

#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <filesystem>

using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

//=============================================================================
// Jetpack backpack testbed. The geometry is now baked OFFLINE (tools) into a
// CPU Zenith_MeshAsset + a bundling .zmodel on disk; the authored scene loads
// that model and the runtime bind happens via the serialized
// Zenith_AttachmentComponent. (Previously the mesh was built as a runtime
// GPU Flux_MeshGeometry and spawned post-load — that path is gone.)
//=============================================================================
namespace
{
	// Session-lifetime owner for the exported model asset handle, so it outlives
	// the export and is released cleanly at shutdown.
	std::vector<ModelHandle> g_axModels;

	// Deterministic on-disk paths for the jetpack assets (GAME_ASSETS_DIR-relative,
	// like EnsureUnitCubeModelExists). Function-local statics give stable storage
	// whose c_str() is safe to hand to AddStep_LoadModel.
	const std::string& JetpackMeshPath()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Jetpack" ZENITH_MESH_ASSET_EXT;
		return s;
	}
	const std::string& JetpackModelPathStr()
	{
		static const std::string s = std::string(GAME_ASSETS_DIR) + "Meshes/RenderTest/Jetpack" ZENITH_MODEL_EXT;
		return s;
	}

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
	const Vector4 SHELL  (0.16f, 0.17f, 0.20f, 1.0f);   // dark frame/backplate
	const Vector4 TANK   (0.62f, 0.36f, 0.10f, 1.0f);   // orange fuel tanks
	const Vector4 STEEL  (0.30f, 0.32f, 0.36f, 1.0f);   // metal caps/struts
	const Vector4 NOZZLE (0.08f, 0.08f, 0.09f, 1.0f);   // scorched nozzle rims

	// Backpack built local: +Z = toward the wearer's back (mounting plane at z=0),
	// body extends -Z (behind the player), +Y up, -Y down (nozzles at the bottom).
	// Built so the two nozzle mouths land at (+/-0.085, -0.28, -0.10) — matching the
	// canonical RenderTest_JetpackComponent::Spec default the exhaust trail reads from.
	void RT_BuildJetpackMeshAsset(Zenith_MeshAsset& xOut)
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
		xB.BuildAsset(xOut);
	}
}

// Parse the showcase + mount-tuning CLI args into RenderTest_JetpackTuning.
void RenderTest_ParseJetpackCLI()
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

// Mount transform (bone-local): translate the backpack onto the upper back + orient
// it. Default is a pure translation behind/above the Spine bone (also baked into the
// scene by AddStep_AttachToBone); the --jetpack-mount-* knobs override each channel
// for screenshot calibration (re-applied at runtime by the bootstrap component).
Zenith_Maths::Matrix4 RenderTest_BuildJetpackMount()
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

const char* RenderTest_JetpackModelPath()
{
	return JetpackModelPathStr().c_str();
}

//=============================================================================
// Tools asset export
//=============================================================================
#ifdef ZENITH_TOOLS
void RenderTest_ExportJetpackAssets(const char* szVtxColorMaterialPath)
{
	// CPU mesh -> .zasset (stack-local, no GPU; mirrors EnsureUnitCubeModelExists).
	std::filesystem::create_directories(std::filesystem::path(JetpackMeshPath()).parent_path());

	Zenith_MeshAsset xMesh;
	RT_BuildJetpackMeshAsset(xMesh);
	xMesh.Export(JetpackMeshPath().c_str());

	// Bundle into a .zmodel referencing the shared vertex-colour material. Overwrite
	// every tools run so geometry edits propagate (the EnsureStickFigureModelExists
	// generation policy).
	Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
	pxModel->SetName("RenderTest_Jetpack");
	Zenith_Vector<std::string> xMaterials;
	xMaterials.PushBack(szVtxColorMaterialPath);
	pxModel->AddMeshByPath(JetpackMeshPath(), xMaterials);
	pxModel->Export(JetpackModelPathStr().c_str());

	ModelHandle xHandle;
	xHandle.Set(pxModel);
	g_axModels.push_back(xHandle);

	Zenith_Log(LOG_CATEGORY_MESH, "[Jetpack] exported %s", JetpackModelPathStr().c_str());
}
#endif

void RenderTest_JetpackShutdown()
{
	// Release the export-time model handle while the AssetRegistry is still alive
	// (mirrors RenderTest_GunsShutdown).
	g_axModels.clear();
}
