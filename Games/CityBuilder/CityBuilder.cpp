#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Behaviour headers are included here so their ZENITH_BEHAVIOUR_TYPE_NAME
// static registrars are pulled into the link. MSVC dead-strips object files
// that no other TU references; a header-only behaviour only registers if some
// compiled .cpp includes it. CityBuilder.cpp is that anchor.
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Components/CB_CityCamera_Behaviour.h"
#include "CityBuilder/Components/CB_DayNightCycle_Behaviour.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
// Offline terrain bake (engine lib, ZENITH_TOOLS): heightmap -> chunk meshes.
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);
#endif

// ============================================================================
// CityBuilder — Project entry points
//
// A single "City" scene (build index 0) holds the persistent CityManager entity
// (Camera + UI + the coordinator behaviours), a sun light, and the ground
// terrain — a real Zenith_TerrainComponent (not a debug primitive).
//
// Terrain alignment: the city grid is 1024 cells * 4m = 4096m wide from origin
// (0,0). The exporter bakes a 4096px heightmap at TERRAIN_SCALE=1.0 into a
// 4096-unit-wide mesh anchored at the origin, so a flat (all-zero) heightmap
// lands the ground at y=0 — exactly where CB_TerrainHeightfield reports height,
// so roads and buildings sit on it. Four ground materials (grass / meadow /
// dirt / pale grass) are blended by a smooth procedural splatmap so the ground
// reads as natural terrain rather than a flat colour. The setup mirrors the
// RenderTest / Exploration terrain pattern.
// ============================================================================

namespace
{
	// Four terrain material slots, re-created every launch (tools + runtime) so
	// the saved scene's terrain entity resolves them by identity on load. The
	// referenced textures are written to disk by the ZENITH_TOOLS bake below.
	MaterialHandle g_axCBTerrainMaterials[4];

	const char* CB_TerrainMatName(int iSlot)
	{
		static const char* s_aszNames[4] = { "CBTerrainGrass", "CBTerrainMeadow", "CBTerrainDirt", "CBTerrainPale" };
		return s_aszNames[iSlot];
	}

	// Absolute on-disk path for a material slot's texture (channel suffix e.g. "_d").
	std::string CB_TerrainTexPath(int iSlot, const char* szSuffix)
	{
		return std::string(GAME_ASSETS_DIR) + "Terrain/Textures/" + CB_TerrainMatName(iSlot) + szSuffix + ZENITH_TEXTURE_EXT;
	}

	// Per-slot base diffuse colour (a natural grassland palette) + roughness.
	struct CB_TerrainMatDef { int iR, iG, iB, iRough; };
	const CB_TerrainMatDef& CB_TerrainMatDefFor(int iSlot)
	{
		static const CB_TerrainMatDef s_axDefs[4] = {
			{  70, 120,  55, 205 },  // 0 grass   (mid green)
			{ 104, 132,  58, 210 },  // 1 meadow  (yellow-green)
			{ 112,  88,  60, 220 },  // 2 dirt    (brown)
			{ 138, 142,  92, 200 },  // 3 pale    (dry/sandy green)
		};
		return s_axDefs[iSlot];
	}
}

// Create the four terrain materials (no disk writes) — runs every launch so the
// loaded scene's terrain resolves its material slots by identity.
static void CB_InitTerrainResources()
{
	static bool ls_bDone = false;
	if (ls_bDone)
	{
		return;
	}
	for (int i = 0; i < 4; ++i)
	{
		g_axCBTerrainMaterials[i].Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
		Zenith_MaterialAsset* pxMat = g_axCBTerrainMaterials[i].GetDirect();
		pxMat->SetName(CB_TerrainMatName(i));
		pxMat->SetDiffuseTexture          (TextureHandle(CB_TerrainTexPath(i, "_d")));
		pxMat->SetNormalTexture           (TextureHandle(CB_TerrainTexPath(i, "_n")));
		pxMat->SetRoughnessMetallicTexture(TextureHandle(CB_TerrainTexPath(i, "_rm")));
		pxMat->SetOcclusionTexture        (TextureHandle(CB_TerrainTexPath(i, "_ao")));
		// Terrain UV ~= heightmap pixel coords * 0.07 (~[0,286] across 4096m).
		// Tiling 2.0 => the ground texture repeats every ~7m, fine grass detail
		// at building scale.
		pxMat->SetUVTiling(Zenith_Maths::Vector2(2.0f, 2.0f));
	}
	ls_bDone = true;
}

#ifdef ZENITH_TOOLS
namespace
{
	// Write an iSize*iSize RGBA8 .ztxtr.
	void CB_WriteRGBA8Texture(const std::string& strAbsPath, int iSize, const std::vector<uint8_t>& xData)
	{
		Zenith_DataStream xStream;
		xStream << static_cast<int32_t>(iSize);
		xStream << static_cast<int32_t>(iSize);
		xStream << static_cast<int32_t>(1);
		xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
		xStream << static_cast<size_t>(xData.size());
		xStream.WriteData(const_cast<uint8_t*>(xData.data()), xData.size());
		xStream.WriteToFile(strAbsPath.c_str());
	}

	uint8_t CB_Clamp8(int iValue)
	{
		return static_cast<uint8_t>(iValue < 0 ? 0 : (iValue > 255 ? 255 : iValue));
	}

	// Diffuse texture: per-pixel brightness noise around (r,g,b) for a textured
	// (not flat-colour) ground. Deterministic LCG so the bake is reproducible.
	void CB_WriteNoiseDiffuse(const std::string& strAbsPath, int iSize, int iR, int iG, int iB, int iVariation, uint32_t uSeed)
	{
		std::vector<uint8_t> xData(static_cast<size_t>(iSize) * iSize * 4);
		uint32_t uState = uSeed ? uSeed : 1u;
		const uint32_t uSpan = static_cast<uint32_t>(2 * iVariation + 1);
		for (int i = 0; i < iSize * iSize; ++i)
		{
			uState = uState * 1664525u + 1013904223u;
			const int iN = static_cast<int>((uState >> 16) % uSpan) - iVariation;
			xData[i * 4 + 0] = CB_Clamp8(iR + iN);
			xData[i * 4 + 1] = CB_Clamp8(iG + iN + iN / 2);  // bias green for a grassy feel
			xData[i * 4 + 2] = CB_Clamp8(iB + iN);
			xData[i * 4 + 3] = 255;
		}
		CB_WriteRGBA8Texture(strAbsPath, iSize, xData);
	}

	// Solid iSize*iSize RGBA8 (normal / RM / AO maps).
	void CB_WriteSolid(const std::string& strAbsPath, int iSize, int iR, int iG, int iB, int iA)
	{
		std::vector<uint8_t> xData(static_cast<size_t>(iSize) * iSize * 4);
		for (int i = 0; i < iSize * iSize; ++i)
		{
			xData[i * 4 + 0] = static_cast<uint8_t>(iR);
			xData[i * 4 + 1] = static_cast<uint8_t>(iG);
			xData[i * 4 + 2] = static_cast<uint8_t>(iB);
			xData[i * 4 + 3] = static_cast<uint8_t>(iA);
		}
		CB_WriteRGBA8Texture(strAbsPath, iSize, xData);
	}

	// Smooth procedural splatmap: large-scale grass/meadow/dirt/pale variation so
	// the flat ground reads as natural terrain. R=slot0 .. A=slot3, normalised.
	void CB_WriteSplatmap(const std::string& strAbsPath)
	{
		const int iSize = 512;  // ~8m / texel across the 4096m terrain
		std::vector<uint8_t> xData(static_cast<size_t>(iSize) * iSize * 4);
		const float fTau = 6.2831853f;
		for (int z = 0; z < iSize; ++z)
		{
			for (int x = 0; x < iSize; ++x)
			{
				const float fx = static_cast<float>(x) / iSize;
				const float fz = static_cast<float>(z) / iSize;
				const float fA = 0.5f + 0.5f * std::sin(fx * fTau * 2.0f + 0.7f) * std::sin(fz * fTau * 1.5f);
				const float fB = 0.5f + 0.5f * std::sin(fx * fTau * 3.5f) * std::cos(fz * fTau * 2.5f + 1.1f);
				auto Sat = [](float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); };
				float fGrass  = 0.60f + 0.30f * fA;          // dominant
				float fMeadow = Sat(fB - 0.45f) * 0.55f;     // patches
				float fDirt   = Sat(fA * fB - 0.42f) * 0.70f;// scattered dirt
				float fPale   = Sat(0.50f - fA) * 0.35f;     // dry edges
				const float fTot = fGrass + fMeadow + fDirt + fPale;
				const float fInv = (fTot > 0.0001f) ? (255.0f / fTot) : 0.0f;
				const int i = (z * iSize + x) * 4;
				xData[i + 0] = CB_Clamp8(static_cast<int>(fGrass  * fInv + 0.5f));
				xData[i + 1] = CB_Clamp8(static_cast<int>(fMeadow * fInv + 0.5f));
				xData[i + 2] = CB_Clamp8(static_cast<int>(fDirt   * fInv + 0.5f));
				xData[i + 3] = CB_Clamp8(static_cast<int>(fPale   * fInv + 0.5f));
			}
		}
		CB_WriteRGBA8Texture(strAbsPath, iSize, xData);
	}

	// Rolling-hills heightmap (R32_SFLOAT) from the shared CB_TerrainGen function.
	// 4096px maps 1:1 to the 4096m terrain (origin 0): pixel (px,pz) = world (px,pz)
	// metres. At TERRAIN_SCALE=1.0 the baked mesh is 4096m wide + chunk-splittable
	// (4096 % 64 == 0 for all LOD divisors). The runtime CB_TerrainHeightfield is
	// shaped with the SAME function so roads/buildings sit on this mesh.
	void CB_WriteHillHeightmap(const std::string& strAbsPath, int iSize)
	{
		std::vector<float> xHeights(static_cast<size_t>(iSize) * iSize, 0.0f);
		for (int pz = 0; pz < iSize; ++pz)
		{
			for (int px = 0; px < iSize; ++px)
			{
				xHeights[static_cast<size_t>(pz) * iSize + px] =
					CB_TerrainGen::HillNorm(static_cast<float>(px), static_cast<float>(pz));
			}
		}
		Zenith_DataStream xStream;
		xStream << static_cast<int32_t>(iSize);
		xStream << static_cast<int32_t>(iSize);
		xStream << static_cast<int32_t>(1);
		xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_R32_SFLOAT);
		xStream << static_cast<size_t>(xHeights.size() * sizeof(float));
		xStream.WriteData(xHeights.data(), xHeights.size() * sizeof(float));
		xStream.WriteToFile(strAbsPath.c_str());
	}
}

// Generate the terrain's on-disk assets (material textures + splatmap + flat
// heightmap + baked chunk meshes). Runs as a scene-automation step before
// SaveScene. The slow heightmap bake is skipped when the chunks already exist.
static void CB_EnsureTerrainAssets()
{
	const std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	const std::string strTexDir     = strTerrainDir + "Textures/";
	std::filesystem::create_directories(strTexDir);

	// Material textures (idempotent on the diffuse map's presence).
	for (int i = 0; i < 4; ++i)
	{
		const std::string strDiffuse = CB_TerrainTexPath(i, "_d");
		if (!std::filesystem::exists(strDiffuse))
		{
			const CB_TerrainMatDef& xDef = CB_TerrainMatDefFor(i);
			CB_WriteNoiseDiffuse(strDiffuse, 64, xDef.iR, xDef.iG, xDef.iB, 16, static_cast<uint32_t>(i * 911u + 7u));
			CB_WriteSolid(CB_TerrainTexPath(i, "_n"),  4, 128, 128, 255, 255);  // flat normal
			CB_WriteSolid(CB_TerrainTexPath(i, "_rm"), 4, 0, xDef.iRough, 0, 255);  // G=roughness, B=metallic(0)
			CB_WriteSolid(CB_TerrainTexPath(i, "_ao"), 4, 255, 255, 255, 255);  // unoccluded
		}
	}

	// Splatmap (idempotent).
	const std::string strSplat = strTerrainDir + "Splatmap_RGBA" ZENITH_TEXTURE_EXT;
	if (!std::filesystem::exists(strSplat))
	{
		CB_WriteSplatmap(strSplat);
	}

	// Hills heightmap -> chunk bake. Slow; gated on a version marker so it re-bakes
	// once when the terrain shape changes, then the on-disk chunk meshes (which now
	// carry the hill geometry) are reused.
	const std::string strHillMarker = strTerrainDir + "terrain_hills_v1.marker";
	if (!std::filesystem::exists(strHillMarker))
	{
		const std::string strHeightmap = strTerrainDir + "CityHeightmap" ZENITH_TEXTURE_EXT;
		CB_WriteHillHeightmap(strHeightmap, 4096);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[CityBuilder] Baking rolling-hills terrain (4096px @ scale 1.0 -> 4096m). This takes a few minutes...");
		ExportHeightmapFromPaths(strHeightmap, strTerrainDir);
		Zenith_DataStream xMark; xMark << static_cast<int32_t>(1); xMark.WriteToFile(strHillMarker.c_str());
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[CityBuilder] Terrain bake complete.");
	}
}
#endif // ZENITH_TOOLS

const char* Project_GetName() { return "CityBuilder"; }
const char* Project_GetGameAssetsDirectory() { return GAME_ASSETS_DIR; }
const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOpts)
{
	// The city is drawn with debug primitives (roads/buildings) over a real
	// lit terrain. The screen-space passes (reflections / SSGI / SSAO / Hi-Z /
	// grass) buy little here. Skybox + IBL stay on so the terrain has a sky and
	// ambient fill (it is real lit G-buffer geometry now, not a flat-colour box);
	// shadows / fog stay off for a cheap, reliable frame.
	xOpts.m_bSSREnabled     = false;
	xOpts.m_bSSGIEnabled    = false;
	xOpts.m_bSSAOEnabled    = false;
	xOpts.m_bHiZEnabled     = false;
	xOpts.m_bGrassEnabled   = false;
	xOpts.m_bShadowsEnabled = false;
	xOpts.m_bFogEnabled     = false;
}

void Project_RegisterScriptBehaviours()
{
	// Behaviours auto-register via ZENITH_BEHAVIOUR_TYPE_NAME (headers above).
	// Terrain materials are created here (tools + runtime) so the loaded scene's
	// terrain entity resolves its material slots.
	CB_InitTerrainResources();
}

void Project_Shutdown()
{
	for (MaterialHandle& xMat : g_axCBTerrainMaterials)
	{
		xMat = MaterialHandle{};
	}
}

void Project_LoadInitialScene(); // forward declaration for the automation step below

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	CB_InitTerrainResources();
}

void Project_RegisterEditorAutomationSteps()
{
	// Terrain is a render-only feature. Skip it (bake + entity) entirely in
	// headless runs: Zenith_TerrainComponent::InitializeCullingResources()
	// allocates GPU culling buffers and asserts ("Invalid buffer VRAM handle")
	// without a render device, which would break the headless logic-test gate.
	// The flat ground isn't needed for the pure-logic sim tests anyway.
	const bool bHeadless = Zenith_CommandLine::IsHeadless();

	// Generate the terrain's textures / splatmap / heightmap + bake the chunk
	// meshes on disk before the saved scene's terrain entity references them.
	if (!bHeadless)
	{
		g_xEngine.EditorAutomation().AddStep_Custom(&CB_EnsureTerrainAssets);
	}

	// ---- City scene (build index 0) ----
	// The CityManager entity is persistent: it carries the main camera, the UI
	// canvas (future HUD), and the two coordinator behaviours. The camera
	// behaviour drives the camera component every frame from its orbit params;
	// the camera fields set here are just the saved initial state.
	g_xEngine.EditorAutomation().AddStep_CreateScene("City");

	g_xEngine.EditorAutomation().AddStep_CreateEntity("CityManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(2048.f, 400.f, 1648.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.95f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(60.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(12000.f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// HUD readouts (updated each frame by CB_CityManager_Behaviour::UpdateHUD).
	struct CB_HUDLine { const char* szName; const char* szInit; float fY; };
	static const CB_HUDLine axHUD[] = {
		{ "CB_Pop",    "Population: 0",    24.f },
		{ "CB_Money",  "Treasury: $0",     58.f },
		{ "CB_Happy",  "Happiness: 0%",    92.f },
		{ "CB_Demand", "Demand R0 C0 I0", 126.f },
		{ "CB_Speed",  "PLAYING [P]",     160.f },
		{ "CB_Tool",   "Tool: None [1-9]",194.f },
	};
	for (const CB_HUDLine& xLine : axHUD)
	{
		g_xEngine.EditorAutomation().AddStep_CreateUIText(xLine.szName, xLine.szInit);
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor(xLine.szName, static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition(xLine.szName, 24.f, xLine.fY);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize(xLine.szName, 26.f);
		g_xEngine.EditorAutomation().AddStep_SetUIColor(xLine.szName, 0.95f, 0.97f, 1.0f, 1.0f);
	}

	g_xEngine.EditorAutomation().AddStep_AttachScript("CB_CityCamera_Behaviour");
	g_xEngine.EditorAutomation().AddStep_AttachScript("CB_CityManager_Behaviour");
	g_xEngine.EditorAutomation().AddStep_AttachScript("CB_DayNightCycle_Behaviour");

	// Ground terrain — a real Zenith_TerrainComponent (flat, 4096m, y=0). Not
	// transient so it persists in the saved scene; the chunk meshes + materials +
	// splatmap were produced by CB_EnsureTerrainAssets above. Windowed-only (see
	// the headless guard above).
	if (!bHeadless)
	{
		g_xEngine.EditorAutomation().AddStep_CreateEntity("CityTerrain");
		g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
		g_xEngine.EditorAutomation().AddStep_AddComponent("Terrain");
		g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(0, g_axCBTerrainMaterials[0].GetDirect());
		g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(1, g_axCBTerrainMaterials[1].GetDirect());
		g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(2, g_axCBTerrainMaterials[2].GetDirect());
		g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(3, g_axCBTerrainMaterials[3].GetDirect());
		g_xEngine.EditorAutomation().AddStep_SetTerrainSplatmapPath("game:Terrain/Splatmap_RGBA" ZENITH_TEXTURE_EXT);
	}

	// A sun light over the city. The deferred renderer expects at least one light;
	// a separate entity keeps it off the CityManager.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("CB_Sun");
	g_xEngine.EditorAutomation().AddStep_AddComponent("Light");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(2048.f, 1500.f, 2048.f);
	g_xEngine.EditorAutomation().AddStep_SetTransformYaw(0.7f);
	g_xEngine.EditorAutomation().AddStep_SetLightColor(1.0f, 0.96f, 0.90f);
	g_xEngine.EditorAutomation().AddStep_SetLightIntensity(4.0f);
	g_xEngine.EditorAutomation().AddStep_SetLightRange(10000.f);

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/City" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif // ZENITH_TOOLS

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/City" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
