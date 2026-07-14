#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_Gallery -- S4 ZM_CreatureGen SC5c species-gallery VISUAL GATE.
//
// A windowed automated test that bakes a diverse sampled dozen of the ~150
// creature bundles, places their .zmodel models in a readable 4x3 grid under a
// framed camera + a directional key light, and dumps three swapchain TGAs (one
// per camera angle) to Build/artifacts/zenithmon/s4/visual/ for the human
// sign-off. It is a REAL regression test as well as a capture harness: it
// asserts every one of the twelve models loaded (bake -> disk -> LoadModel ->
// renderable instance) and that every capture file was written.
//
// GATING: m_bRequiresGraphics = true, so the headless CI batch skips it (no
// GPU, no bake). When the baked bundles are absent (fresh CI checkout -- the
// Assets/Creatures tree is git-ignored) the Setup RequestSkip()s so the batch
// stays green. Only a windowed *_True run actually bakes + renders + captures.
//
// SAMPLED DOZEN (>=1 per ZM_ARCHETYPE; ZENITHRAX shown as its SHINY variant):
//   Fernfawn     QUADRUPED           Zenithrax    SERPENT (SHINY)
//   Sylvastag    QUADRUPED           Monolode     BLOB
//   Pyroclast    BIPED               Zephyrbloom  FLOATER_PLANTOID
//   Tidesabre    AQUATIC             Auricorn     QUADRUPED (single-stage)
//   Stratavis    AVIAN               Emberkoi     AQUATIC (single-stage)
//   Aurelwing    INSECTOID           Verdantis    INSECTOID (mantis)
//
// ORIENTATION: the archetype builders put feet at Y=0, up = +Y, front = +Z, so
// creatures are placed feet-on-ground and the camera views them from +Z (front)
// and from the two +Z-side 3/4 angles. Per-entity scale = fNORM / fSizeScale
// fully normalises the size-class-baked extents, so every species renders at the
// same on-screen height regardless of TINY..HUGE class and framing is stable.
// ============================================================================

namespace
{
	// ---- The sampled dozen (species + whether to show the shiny .zmodel). ----
	struct ZM_GalleryEntry
	{
		ZM_SPECIES_ID m_eSpecies;
		bool          m_bShiny;
	};

	const ZM_GalleryEntry g_axGalleryEntries[] = {
		{ ZM_SPECIES_FERNFAWN,    false },   // QUADRUPED (starter base)
		{ ZM_SPECIES_SYLVASTAG,   false },   // QUADRUPED (starter final)
		{ ZM_SPECIES_PYROCLAST,   false },   // BIPED (fire final)
		{ ZM_SPECIES_TIDESABRE,   false },   // AQUATIC (blade-fish final)
		{ ZM_SPECIES_STRATAVIS,   false },   // AVIAN (songbird final)
		{ ZM_SPECIES_AURELWING,   false },   // INSECTOID (butterfly final)
		{ ZM_SPECIES_ZENITHRAX,   true  },   // SERPENT (pseudo-legendary) -- SHINY
		{ ZM_SPECIES_MONOLODE,    false },   // BLOB (walking cairn final)
		{ ZM_SPECIES_ZEPHYRBLOOM, false },   // FLOATER_PLANTOID (dandelion final)
		{ ZM_SPECIES_AURICORN,    false },   // QUADRUPED (gold antelope, single-stage)
		{ ZM_SPECIES_EMBERKOI,    false },   // AQUATIC (hot-spring koi, single-stage)
		{ ZM_SPECIES_VERDANTIS,   false },   // INSECTOID (pruning-blade mantis final)
	};
	constexpr u_int uZM_GALLERY_COUNT =
		static_cast<u_int>(sizeof(g_axGalleryEntries) / sizeof(g_axGalleryEntries[0]));

	// 4 columns x 3 rows grid with WIDE gaps so every creature is clearly
	// separated (none overlapping). Row 0 is nearest the +Z camera; row 2 farthest.
	const float g_afColumnX[4] = { -13.5f, -4.5f, 4.5f, 13.5f };   // 9u column pitch
	const float g_afRowZ[3]    = {   8.0f,  0.0f, -8.0f };          // 8u row pitch

	// Fully normalise size-class extents: final world height == fNORM * (the
	// baked medium/unit height) for EVERY species, since the archetype builders
	// scale extents by m_fSizeScale and we divide it back out here. Raised so each
	// creature occupies a meaningful, identifiable area of the frame.
	constexpr float fZM_GALLERY_NORM_SCALE = 2.3f;

	// Aim BELOW the grid centre (toward the floor at y=0) and RAISE the cameras so
	// each of the three +Z-front views tilts DOWN -- the reflective floor + the
	// creatures' reflections fill the lower frame while all 12 stay clearly visible
	// above them.
	const Zenith_Maths::Vector3 g_xViewAim  (0.0f, 0.8f, 0.0f);
	const Zenith_Maths::Vector3 g_xViewFront(0.0f, 12.5f, 31.0f);
	const Zenith_Maths::Vector3 g_xViewLeft (-26.0f, 11.0f, 23.0f);
	const Zenith_Maths::Vector3 g_xViewRight( 26.0f, 11.0f, 23.0f);

	// ---- Test state ----
	bool        g_bGalleryActive     = false;   // scene was built (Setup got past the skip)
	bool        g_bGalleryFailed     = false;
	const char* g_szGalleryFailure   = "test did not reach verification";
	u_int       g_uModelsLoadedCheck = 0u;
	bool        g_abShotRequested[3] = { false, false, false };
	std::string g_astrShotPath[3];
	Zenith_Scene g_xGalleryPreviousScene;
	Zenith_Scene g_xGalleryScene;

	// Procedural floor prop resources (kept alive for the component's lifetime, as
	// AddMeshEntry does not take ownership); released in cleanup.
	MeshGeometryHandle g_xFloorGeometry;
	MaterialHandle     g_xFloorMaterial;

	// ---- Scoped graphics-option overrides for a clean, colour-true gallery ----
	// Saved on apply, restored on cleanup. Rationale for each override:
	//  * auto-exposure OFF -> the reported colour wash was auto-exposure lifting the
	//                    mid/light albedos until AgX desaturated them toward white.
	//                    With it off the tonemap uses the FIXED manual exposure
	//                    (dbg_fHDRExposure == 1.0; that DEBUGVAR is file-static and
	//                    cannot be set from a test), so brightness is governed purely
	//                    by the lights below -- calibrated to the material-preview's
	//                    proven fixed-exposure-1.0 sun (intensity 3.0), erring dim.
	//  * bloom OFF    -> no bright-pixel bloom glow over the creatures.
	//  * skybox OFF   -> a neutral dark studio SOLID background (m_xSkyboxColour)
	//                    instead of the still-loaded FrontEnd's bright atmosphere.
	//  * text/quads OFF -> hides the still-loaded FrontEnd "Zenithmon" title overlay
	//                    (UI renders across ALL loaded scenes).
	//  * SSR ON       -> screen-space reflections, so the smooth metallic floor
	//                    mirrors the creatures. On by default, but forced + saved
	//                    here so the gate never depends on ambient config. Its
	//                    companions m_bHiZEnabled + m_bSSRRoughnessBlurEnabled are
	//                    also on by default and left untouched.
	// IBL ambient (the same global irradiance the material preview uses) stays ON,
	// so shadowed faces are filled rather than pure black.
	struct GalleryGraphicsSave
	{
		bool m_bSaved        = false;
		bool m_bBloom        = true;
		bool m_bAutoExposure = true;
		bool m_bSkybox       = true;
		bool m_bText         = true;
		bool m_bQuads        = true;
		bool m_bSSR          = true;
		Zenith_Maths::Vector3 m_xSkyColour = Zenith_Maths::Vector3(0.0f);
	};
	GalleryGraphicsSave g_xGraphicsSave;

	void ApplyGalleryGraphicsOptions()
	{
		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		g_xGraphicsSave.m_bBloom        = xOpts.m_bHDRBloomEnabled;
		g_xGraphicsSave.m_bAutoExposure = xOpts.m_bHDRAutoExposureEnabled;
		g_xGraphicsSave.m_bSkybox       = xOpts.m_bSkyboxEnabled;
		g_xGraphicsSave.m_bText         = xOpts.m_bTextEnabled;
		g_xGraphicsSave.m_bQuads        = xOpts.m_bQuadsEnabled;
		g_xGraphicsSave.m_bSSR          = xOpts.m_bSSREnabled;
		g_xGraphicsSave.m_xSkyColour    = xOpts.m_xSkyboxColour;
		g_xGraphicsSave.m_bSaved        = true;

		xOpts.m_bHDRBloomEnabled        = false;
		xOpts.m_bHDRAutoExposureEnabled = false;   // fixed exposure 1.0 (see note above)
		xOpts.m_bSkyboxEnabled          = false;
		xOpts.m_bTextEnabled            = false;
		xOpts.m_bQuadsEnabled           = false;
		xOpts.m_bSSREnabled             = true;    // mirror the creatures in the floor
		xOpts.m_xSkyboxColour           = Zenith_Maths::Vector3(0.06f, 0.06f, 0.07f);
	}

	void RestoreGalleryGraphicsOptions()
	{
		if (!g_xGraphicsSave.m_bSaved)
		{
			return;
		}
		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		xOpts.m_bHDRBloomEnabled        = g_xGraphicsSave.m_bBloom;
		xOpts.m_bHDRAutoExposureEnabled = g_xGraphicsSave.m_bAutoExposure;
		xOpts.m_bSkyboxEnabled          = g_xGraphicsSave.m_bSkybox;
		xOpts.m_bTextEnabled            = g_xGraphicsSave.m_bText;
		xOpts.m_bQuadsEnabled           = g_xGraphicsSave.m_bQuads;
		xOpts.m_bSSREnabled             = g_xGraphicsSave.m_bSSR;
		xOpts.m_xSkyboxColour           = g_xGraphicsSave.m_xSkyColour;
		g_xGraphicsSave.m_bSaved        = false;
	}

	void FailGallery(const char* szReason)
	{
		g_szGalleryFailure = szReason;
		g_bGalleryFailed = true;
	}

	// Canonical "game:Creatures/<Name>/<Name>[_shiny].zmodel" ref for LoadModel,
	// plus the on-disk absolute path (same name/case) for the existence guard.
	bool ResolveModelRefAndDisk(const ZM_GalleryEntry& xEntry,
		char* szRefOut, u_int uRefCap, std::string& strDiskOut)
	{
		const ZM_CREATURE_ASSET_KIND eKind = xEntry.m_bShiny
			? ZM_CREATURE_ASSET_MODEL_SHINY
			: ZM_CREATURE_ASSET_MODEL;
		if (!ZM_CreatureAssetPath(xEntry.m_eSpecies, eKind, szRefOut, uRefCap))
		{
			return false;
		}
		// Every ref is "game:<relative>"; the disk path is GAME_ASSETS_DIR + that
		// relative tail, reusing the exact name the bake wrote.
		const char* szPrefix = "game:";
		const size_t uPrefixLen = std::strlen(szPrefix);
		if (std::strncmp(szRefOut, szPrefix, uPrefixLen) != 0)
		{
			return false;
		}
		strDiskOut = std::string(GAME_ASSETS_DIR) + std::string(szRefOut + uPrefixLen);
		return true;
	}

	bool DiskFilePresent(const std::string& strPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(strPath, xError) || xError)
		{
			return false;
		}
		const std::uintmax_t ulSize = std::filesystem::file_size(strPath, xError);
		return !xError && ulSize != 0u;
	}

	bool AllCreatureBundlesPresent()
	{
		for (u_int i = 0; i < uZM_GALLERY_COUNT; ++i)
		{
			char szRef[256];
			std::string strDisk;
			if (!ResolveModelRefAndDisk(g_axGalleryEntries[i], szRef,
				static_cast<u_int>(sizeof(szRef)), strDisk))
			{
				return false;
			}
			if (!DiskFilePresent(strDisk))
			{
				return false;
			}
		}
		return true;
	}

	// Absolute Build/artifacts/zenithmon/s4/visual dir derived from GAME_ASSETS_DIR
	// (<repo>/Games/Zenithmon/Assets/ -> up three -> <repo>), so it resolves
	// regardless of the process working directory.
	std::filesystem::path GalleryVisualDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "s4" / "visual";
	}

	u_int CountLoadedGalleryModels()
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<Zenith_ModelComponent>().ForEach(
			[&uCount](Zenith_EntityID, Zenith_ModelComponent& xModel)
			{
				// Count only .zmodel-LOADED creatures (non-empty model path); the
				// procedural AddMeshEntry floor has an empty path and is excluded.
				if (xModel.HasModel() && xModel.GetNumMeshes() > 0u
					&& !xModel.GetModelPath().empty())
				{
					++uCount;
				}
			});
		return uCount;
	}

	Zenith_CameraComponent* FindGalleryCamera()
	{
		Zenith_CameraComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryActiveScene<Zenith_CameraComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_CameraComponent& xCamera)
			{
				if (pxFound == nullptr)
				{
					pxFound = &xCamera;
				}
			});
		return pxFound;
	}

	// Aim the camera FROM xPos AT xTarget using the engine's yaw/pitch->forward
	// convention: forward = (-sin(yaw)cos(pitch), sin(pitch), cos(yaw)cos(pitch)).
	void AimCameraAt(Zenith_CameraComponent& xCamera,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xTarget)
	{
		xCamera.SetPosition(xPos);
		Zenith_Maths::Vector3 xDir = xTarget - xPos;
		const float fLen = std::sqrt(xDir.x * xDir.x + xDir.y * xDir.y + xDir.z * xDir.z);
		if (fLen > 1.0e-4f)
		{
			xDir /= fLen;
		}
		const double fPitch = std::asin(static_cast<double>(std::clamp(xDir.y, -1.0f, 1.0f)));
		const double fYaw = std::atan2(static_cast<double>(-xDir.x), static_cast<double>(xDir.z));
		xCamera.SetPitch(fPitch);
		xCamera.SetYaw(fYaw);
	}

	void RequestShot(u_int uIndex)
	{
		if (uIndex >= 3u || g_astrShotPath[uIndex].empty())
		{
			return;
		}
		Flux_Screenshot::RequestDump(g_astrShotPath[uIndex].c_str());
		g_abShotRequested[uIndex] = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_Gallery] requested capture %u -> %s",
			uIndex, g_astrShotPath[uIndex].c_str());
	}

	void CleanupGalleryScene()
	{
		RestoreGalleryGraphicsOptions();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xGalleryPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xGalleryPreviousScene);
		}
		if (g_xGalleryScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xGalleryScene);
			g_xGalleryScene = Zenith_Scene();
		}
		g_xFloorGeometry = MeshGeometryHandle();
		g_xFloorMaterial = MaterialHandle();
		g_bGalleryActive = false;
	}

	void BuildGalleryCreature(Zenith_SceneData* pxSceneData, u_int uIndex)
	{
		const ZM_GalleryEntry& xEntry = g_axGalleryEntries[uIndex];
		char szRef[256];
		std::string strDisk;
		if (!ResolveModelRefAndDisk(xEntry, szRef, static_cast<u_int>(sizeof(szRef)), strDisk))
		{
			FailGallery("could not resolve a creature model ref");
			return;
		}

		char szName[96];
		std::snprintf(szName, sizeof(szName), "%s%s",
			ZM_GetSpeciesName(xEntry.m_eSpecies), xEntry.m_bShiny ? "_Shiny" : "");

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ g_afColumnX[uIndex % 4u], 0.0f, g_afRowZ[uIndex / 4u] });

		const float fSizeScale = ZM_ResolveCreatureRecipe(xEntry.m_eSpecies).m_fSizeScale;
		const float fEntityScale = (fSizeScale > 1.0e-3f)
			? (fZM_GALLERY_NORM_SCALE / fSizeScale)
			: fZM_GALLERY_NORM_SCALE;
		xTransform.SetScale({ fEntityScale, fEntityScale, fEntityScale });

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		xModel.LoadModel(szRef);
	}
}

static void Setup_ZMGallery()
{
	g_bGalleryActive = false;
	g_bGalleryFailed = false;
	g_szGalleryFailure = "test did not reach verification";
	g_uModelsLoadedCheck = 0u;
	g_abShotRequested[0] = g_abShotRequested[1] = g_abShotRequested[2] = false;
	g_xGalleryPreviousScene = Zenith_Scene();
	g_xGalleryScene = Zenith_Scene();

	// Bake the sampled dozen (tools only; a non-tools build's ZM_BakeCreature is a
	// no-op and relies on a prior *_True bake having produced the bundles).
#ifdef ZENITH_TOOLS
	for (u_int i = 0; i < uZM_GALLERY_COUNT; ++i)
	{
		ZM_BakeCreature(g_axGalleryEntries[i].m_eSpecies);
	}
#endif

	// Asset guard BEFORE any fixed-dt / scene state (RequestSkip bypasses Verify),
	// mirroring the Dawnmere/grass windowed tests: CI checkouts have no baked
	// Assets/Creatures tree, so skip rather than fail.
	if (!AllCreatureBundlesPresent())
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"baked creature bundles absent (run a *_True build to bake Assets/Creatures)");
		return;
	}

	// Capture destination: create it up front -- the swapchain dump uses fopen
	// directly and does NOT create parent directories.
	const std::filesystem::path xVisualDir = GalleryVisualDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xVisualDir, xDirError);
	g_astrShotPath[0] = (xVisualDir / "gallery_01.tga").string();
	g_astrShotPath[1] = (xVisualDir / "gallery_02.tga").string();
	g_astrShotPath[2] = (xVisualDir / "gallery_03.tga").string();

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);

	// Neutral background + bloom/UI off for a gate-ready capture (restored in cleanup).
	ApplyGalleryGraphicsOptions();

	// Isolated additive scene so the gallery never perturbs the persistent world.
	g_xGalleryPreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xGalleryScene = g_xEngine.Scenes().LoadScene(
		"ZM_CreatureGallery", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(g_xGalleryScene);
	if (pxSceneData == nullptr || !g_xEngine.Scenes().SetActiveScene(g_xGalleryScene))
	{
		FailGallery("could not create the isolated gallery scene");
		return;
	}
	g_bGalleryActive = true;

	// Directional KEY light, from the upper +X/front. With auto-exposure OFF and a
	// FIXED exposure of 1.0, a directional's intensity multiplies surface radiance
	// DIRECTLY (Common/Lighting.slang: directional fAttenuation == fIntensity, no
	// distance/unit rescale), so the value is O(1), NOT thousands of lux. Anchored
	// to the material preview's proven fixed-exposure-1.0 sun (intensity 3.0,
	// Flux_MaterialPreviewController.cpp) and set BELOW it (2.5) to err dim -- a
	// slightly dark but clearly-coloured creature over a washed-white one. Travels
	// down + toward -Z so it lights the +Z faces presented to the camera. (Created
	// FIRST so it is the first-gathered directional.)
	Zenith_Entity xLight = g_xEngine.Scenes().CreateEntity(pxSceneData, "GalleryKeyLight");
	Zenith_LightComponent& xLightComponent = xLight.AddComponent<Zenith_LightComponent>();
	xLightComponent.SetLightType(LIGHT_TYPE_DIRECTIONAL);
	xLightComponent.SetColor(Zenith_Maths::Vector3(1.0f, 0.97f, 0.90f));
	xLightComponent.SetIntensity(2.5f);
	xLightComponent.SetWorldDirection(
		glm::normalize(Zenith_Maths::Vector3(-0.35f, -0.85f, -0.40f)));

	// Gentle FILL from the opposite (-X) side, also lighting the +Z fronts, so the
	// key's shadowed faces are not pure black. IBL ambient adds further fill.
	Zenith_Entity xFill = g_xEngine.Scenes().CreateEntity(pxSceneData, "GalleryFillLight");
	Zenith_LightComponent& xFillLight = xFill.AddComponent<Zenith_LightComponent>();
	xFillLight.SetLightType(LIGHT_TYPE_DIRECTIONAL);
	xFillLight.SetColor(Zenith_Maths::Vector3(0.85f, 0.90f, 1.0f));
	xFillLight.SetIntensity(1.0f);
	xFillLight.SetWorldDirection(
		glm::normalize(Zenith_Maths::Vector3(0.50f, -0.35f, -0.40f)));

	// Framed gallery camera (main). Positioned to the initial front view.
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(pxSceneData, "GalleryCamera");
	Zenith_CameraComponent& xCameraComponent = xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetFOV(glm::radians(55.0f));
	xCameraComponent.SetNearPlane(0.1f);
	xCameraComponent.SetFarPlane(500.0f);
	AimCameraAt(xCameraComponent, g_xViewFront, g_xViewAim);
	Zenith_UnitTests::SetMainCameraForTest(pxSceneData, xCamera.GetEntityID());

	// Large smooth METALLIC floor slab at the creatures' feet (top face at y=0), so
	// SSR mirrors all 12 creatures below them. Built from the shared unit-cube
	// geometry scaled into a thin 60x60 slab + an in-memory dark-metal material and
	// attached via AddMeshEntry -- the same asset-free prop pattern as
	// ZM_GreyboxVisual (no .zmodel/.zmtrl disk bake for a one-off test prop). A
	// metallic surface reflects its environment tinted by base colour, so the dark
	// grey keeps the reflections contrasty rather than blown out.
	g_xFloorGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_xFloorMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MeshGeometryAsset* pxFloorGeometryAsset = g_xFloorGeometry.GetDirect();
	Zenith_MaterialAsset* pxFloorMaterial = g_xFloorMaterial.GetDirect();
	Flux_MeshGeometry* pxFloorGeometry =
		(pxFloorGeometryAsset != nullptr) ? pxFloorGeometryAsset->GetGeometry() : nullptr;
	if (pxFloorMaterial != nullptr && pxFloorGeometry != nullptr)
	{
		pxFloorMaterial->SetName("ZM_GalleryFloor");
		pxFloorMaterial->SetBaseColor({ 0.30f, 0.30f, 0.33f, 1.0f });   // dark neutral metal tint
		pxFloorMaterial->SetRoughness(0.06f);                            // smooth -> sharp mirror
		pxFloorMaterial->SetMetallic(1.0f);                              // full metallic reflectance

		Zenith_Entity xFloor = g_xEngine.Scenes().CreateEntity(pxSceneData, "GalleryFloor");
		Zenith_TransformComponent& xFloorTransform =
			xFloor.GetComponent<Zenith_TransformComponent>();
		// Unit cube is centred, so a 0.2-thick slab centred at y=-0.1 puts its top
		// face exactly at y=0 (the grounded creatures' feet); 60x60 in X/Z reaches
		// well beyond the grid so every creature's reflection lands on it.
		xFloorTransform.SetPosition({ 0.0f, -0.10f, 0.0f });
		xFloorTransform.SetScale({ 60.0f, 0.20f, 60.0f });
		Zenith_ModelComponent& xFloorModel = xFloor.AddComponent<Zenith_ModelComponent>();
		xFloorModel.AddMeshEntry(*pxFloorGeometry, *pxFloorMaterial);
	}

	// The dozen baked creatures, in a readable 4x3 grid, each facing the camera.
	for (u_int i = 0; i < uZM_GALLERY_COUNT && !g_bGalleryFailed; ++i)
	{
		BuildGalleryCreature(pxSceneData, i);
	}
}

static bool Step_ZMGallery(int iFrame)
{
	if (!g_bGalleryActive || g_bGalleryFailed)
	{
		return false;
	}

	Zenith_CameraComponent* pxCamera = FindGalleryCamera();
	if (pxCamera == nullptr)
	{
		FailGallery("gallery camera disappeared mid-run");
		return false;
	}

	// LoadModel is synchronous, so all twelve instances exist immediately; the
	// early frames only let the GPU upload settle before the first capture.
	if (iFrame == 30)
	{
		g_uModelsLoadedCheck = CountLoadedGalleryModels();
		if (g_uModelsLoadedCheck != uZM_GALLERY_COUNT)
		{
			FailGallery("not every sampled creature .zmodel produced a renderable instance");
			return false;
		}
	}

	// Set each viewpoint a few frames ahead of its capture so the new view
	// matrix is rendered before the swapchain consumes the dump.
	switch (iFrame)
	{
	case 60:  AimCameraAt(*pxCamera, g_xViewFront, g_xViewAim); break;
	case 66:  RequestShot(0); break;   // front elevated
	case 78:  AimCameraAt(*pxCamera, g_xViewLeft, g_xViewAim);  break;
	case 84:  RequestShot(1); break;   // left 3/4
	case 96:  AimCameraAt(*pxCamera, g_xViewRight, g_xViewAim); break;
	case 102: RequestShot(2); break;   // right 3/4
	case 250: return false;            // hold ~150 frames for an external capture, then end
	default:  break;
	}
	return true;
}

static bool Verify_ZMGallery()
{
	bool bPassed = true;
	if (g_bGalleryFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_Gallery] %s", g_szGalleryFailure);
		bPassed = false;
	}

	// The model + capture assertions are only meaningful once the scene was built.
	if (g_bGalleryActive)
	{
		if (g_uModelsLoadedCheck != uZM_GALLERY_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_Gallery] expected %u renderable creature models, saw %u",
				uZM_GALLERY_COUNT, g_uModelsLoadedCheck);
			bPassed = false;
		}
		for (u_int i = 0; i < 3u; ++i)
		{
			if (!g_abShotRequested[i])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_Gallery] capture %u was never requested", i);
				bPassed = false;
				continue;
			}
			if (!DiskFilePresent(g_astrShotPath[i]))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_Gallery] capture %u produced no file on disk: %s",
					i, g_astrShotPath[i].c_str());
				bPassed = false;
			}
		}
	}

	// Always restore graphics options + fixed dt + unload the scene (all guarded),
	// even if Setup applied the overrides but scene creation then failed.
	CleanupGalleryScene();
	return bPassed;
}

static const Zenith_AutomatedTest g_xZMGalleryTest = {
	"ZM_CreatureGallery_Test",
	&Setup_ZMGallery,
	&Step_ZMGallery,
	&Verify_ZMGallery,
	/* maxFrames */ 360,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMGalleryTest);

#endif // ZENITH_INPUT_SIMULATOR
