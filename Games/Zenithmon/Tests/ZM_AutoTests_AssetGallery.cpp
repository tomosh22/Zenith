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
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include "Zenithmon/Source/Data/ZM_HumanData.h"
#include "Zenithmon/Source/Data/ZM_BuildingData.h"
#include "Zenithmon/Source/Data/ZM_PropData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

// ============================================================================
// ZM_AutoTests_AssetGallery -- the S4 ALL-FAMILIES visual sign-off gate. A
// windowed automated test that (in a tools build) warm-bakes every procedural
// asset family via ZM_BakeAllAssets, then places representatives from ALL FOUR
// families -- creatures + humans + buildings + props -- in one additive scene,
// each normalised to a common on-screen height, under a framed camera + a
// directional key/fill pair, and dumps three swapchain TGAs (one per camera
// angle) to Build/artifacts/zenithmon/s4/gallery/ for the human sign-off.
//
// It is a 4-family SUPERSET of ZM_CreatureGallery_Test (ZM_AutoTests_Gallery.cpp)
// and is deliberately INDEPENDENT of it: the scaffolding is copied (the helpers
// there live in a per-TU anonymous namespace), so the two tests share nothing at
// link time. Like the creature gallery it is a REAL regression test as well as a
// capture harness: it asserts every one of the 26 representative models loaded
// (bake -> disk -> LoadModel -> renderable instance) and that every capture file
// was written.
//
// ROSTER (26 = 8 creatures + 6 humans + 6 buildings + 6 props):
//   Creatures (8, one per ZM_ARCHETYPE): Fernfawn QUADRUPED, Pyroclast BIPED,
//     Tidesabre AQUATIC, Stratavis AVIAN, Aurelwing INSECTOID, Zenithrax SERPENT
//     (SHINY .zmodel), Monolode BLOB, Zephyrbloom FLOATER_PLANTOID.
//   Humans (6): PlayerM, Prof Aster, Rival Vesper, Leader Fenna, Champion Elara,
//     Town Caretaker.
//   Buildings (6): PlayerHome, Lab, Gym1, Gym2, CareCenter, Townhall.
//   Props (6): LampPost, SignPost, FenceWood, RockLarge, Barrel, DressingMeadow.
//
// LAYOUT: four Z-rows, camera on +Z, buildings farthest so the tall shells never
// occlude the small props nearest the lens. Each entry is uniform-scaled to a
// per-family target display height (props 2.5, creatures 3.0, humans 3.0,
// buildings 6.0) by dividing out the family's natural-size field, so every model
// reads at a consistent, framed size regardless of its baked extents.
//
// GATING: m_bRequiresGraphics = true, so the headless CI batch skips it (no GPU,
// no bake). Setup RequestSkip()s when the baked families are absent/stale -- in a
// tools build ZM_BakeAllAssets() must return true (it warm-checks the per-family
// manifests and re-bakes only the stale ones); in a non-tools build all four
// ZM_BakeManifestCheck()s must pass. So a fresh CI checkout (the Assets tree is
// git-ignored) skips rather than fails, and the CI unit baseline is unchanged.
// Only a windowed *_True run actually bakes + renders + captures.
// ============================================================================

namespace
{
	// ---- Representative rosters (all ids verified against the real headers) ----

	// Creatures span the 8 ZM_ARCHETYPE body plans (reused from the shipped
	// creature gallery); Zenithrax is shown as its SHINY .zmodel for variety.
	struct ZM_AGCreatureEntry
	{
		ZM_SPECIES_ID m_eSpecies;
		bool          m_bShiny;
	};
	const ZM_AGCreatureEntry g_axAGCreatures[] = {
		{ ZM_SPECIES_FERNFAWN,    false },   // QUADRUPED
		{ ZM_SPECIES_PYROCLAST,   false },   // BIPED
		{ ZM_SPECIES_TIDESABRE,   false },   // AQUATIC
		{ ZM_SPECIES_STRATAVIS,   false },   // AVIAN
		{ ZM_SPECIES_AURELWING,   false },   // INSECTOID
		{ ZM_SPECIES_ZENITHRAX,   true  },   // SERPENT -- SHINY
		{ ZM_SPECIES_MONOLODE,    false },   // BLOB
		{ ZM_SPECIES_ZEPHYRBLOOM, false },   // FLOATER_PLANTOID
	};

	const ZM_HUMAN_ID g_aeAGHumans[] = {
		ZM_HUMAN_PLAYER_M,        // "PlayerM"
		ZM_HUMAN_PROF_ASTER,      // "Aster"     professor
		ZM_HUMAN_RIVAL_VESPER,    // "Vesper"    rival
		ZM_HUMAN_LEADER_FENNA,    // "Fenna"     grass gym leader
		ZM_HUMAN_CHAMPION_ELARA,  // "Elara"     champion
		ZM_HUMAN_TOWN_CARETAKER,  // "Caretaker" townsfolk
	};

	const ZM_BUILDING_ID g_aeAGBuildings[] = {
		ZM_BUILDING_PLAYER_HOME,
		ZM_BUILDING_LAB,
		ZM_BUILDING_GYM_1,
		ZM_BUILDING_GYM_2,
		ZM_BUILDING_CARE_CENTER,
		ZM_BUILDING_TOWNHALL,
	};

	const ZM_PROP_ID g_aeAGProps[] = {
		ZM_PROP_LAMP_POST,
		ZM_PROP_SIGN_POST,
		ZM_PROP_FENCE_WOOD,
		ZM_PROP_ROCK_LARGE,
		ZM_PROP_BARREL,
		ZM_PROP_DRESSING_MEADOW,
	};

	constexpr u_int uZM_AG_CREATURES = static_cast<u_int>(sizeof(g_axAGCreatures) / sizeof(g_axAGCreatures[0]));
	constexpr u_int uZM_AG_HUMANS    = static_cast<u_int>(sizeof(g_aeAGHumans) / sizeof(g_aeAGHumans[0]));
	constexpr u_int uZM_AG_BUILDINGS = static_cast<u_int>(sizeof(g_aeAGBuildings) / sizeof(g_aeAGBuildings[0]));
	constexpr u_int uZM_AG_PROPS     = static_cast<u_int>(sizeof(g_aeAGProps) / sizeof(g_aeAGProps[0]));
	constexpr u_int uZM_AG_TOTAL     = uZM_AG_CREATURES + uZM_AG_HUMANS + uZM_AG_BUILDINGS + uZM_AG_PROPS;   // 26

	// ---- Layout: four Z-rows on +Z-facing camera; buildings farthest. ----------
	// Column X within a row of N: X(i,N) = (i - (N-1)/2) * pitch, centred on 0.
	constexpr float fZM_AG_PROP_ROW_Z     =  18.0f;
	constexpr float fZM_AG_CREATURE_ROW_Z =   6.0f;
	constexpr float fZM_AG_HUMAN_ROW_Z    =  -6.0f;
	constexpr float fZM_AG_BUILDING_ROW_Z = -22.0f;

	constexpr float fZM_AG_PROP_PITCH     =  8.0f;
	constexpr float fZM_AG_CREATURE_PITCH =  7.5f;
	constexpr float fZM_AG_HUMAN_PITCH    =  8.0f;
	constexpr float fZM_AG_BUILDING_PITCH = 14.0f;   // >= the building width budget (11) + margin

	// Per-family target on-screen display height (world units); the per-entry scale
	// divides out the family's natural-size field so each model hits this height.
	constexpr float fZM_AG_PROP_HEIGHT     = 3.0f;
	constexpr float fZM_AG_CREATURE_HEIGHT = 4.5f;   // hero content -- kept prominent
	constexpr float fZM_AG_HUMAN_HEIGHT    = 4.5f;   // hero content -- kept prominent
	constexpr float fZM_AG_BUILDING_HEIGHT = 6.5f;
	constexpr float fZM_AG_HUMAN_NOMINAL_M = 1.8f;    // a build-1.0 human is ~1.8m tall
	constexpr float fZM_AG_MIN_DENOM       = 1.0e-3f; // guard against a zero natural size

	// Per-column WIDTH budget for the families whose footprint width varies a lot vs
	// their height (buildings/props). AGFitScale (below) caps each model's SCALED
	// width at this so a wide, short model cannot spill into its neighbour's column.
	// WHY THIS EXISTS (ZM-D-087): buildings were originally scaled by HEIGHT ONLY, so a
	// wide 1-storey building (CareCenter 10x8x1, resolve keeps m_fStoreyHeight=3) got
	// scale = 6.5/3 ~= 2.17 -> ~21u wide, far past the old 12u pitch -> buildings
	// INTERSECTED. Creatures/humans are ~isotropic (roughly as wide as tall) so height
	// normalization alone keeps them inside their pitch; buildings/props are not, so
	// they MUST also honour a width budget. Each budget must be < its family's pitch.
	constexpr float fZM_AG_BUILDING_WIDTH_BUDGET  = 11.0f;   // < building pitch 14
	constexpr float fZM_AG_PROP_WIDTH_BUDGET      =  6.5f;   // < prop pitch 8
	constexpr float fZM_AG_BUILDING_ROOF_OVERHANG =  0.6f;   // eaves extend 0.3 each side (SC2 shell)

	// Raised, +Z cameras that tilt DOWN over the rows: the reflective floor mirrors
	// the models while all 26 stay clearly visible. Tune later against the first TGA.
	const Zenith_Maths::Vector3 g_xAGViewAim  (  0.0f,  2.5f, -5.0f);
	const Zenith_Maths::Vector3 g_xAGViewFront(  0.0f, 12.0f, 40.0f);
	const Zenith_Maths::Vector3 g_xAGViewLeft (-30.0f, 12.0f, 30.0f);
	const Zenith_Maths::Vector3 g_xAGViewRight( 30.0f, 12.0f, 30.0f);

	// ---- Test state ----
	bool        g_bAGActive           = false;   // scene was built (Setup got past the skip)
	bool        g_bAGFailed           = false;
	const char* g_szAGFailure         = "test did not reach verification";
	u_int       g_uAGModelsLoadedCheck = 0u;
	bool        g_abAGShotRequested[3] = { false, false, false };
	std::string g_astrAGShotPath[3];
	Zenith_Scene g_xAGPreviousScene;
	Zenith_Scene g_xAGScene;

	// Procedural floor prop resources (kept alive for the component's lifetime, as
	// AddMeshEntry does not take ownership); released in cleanup.
	MeshGeometryHandle g_xAGFloorGeometry;
	MaterialHandle     g_xAGFloorMaterial;

	// ---- Scoped graphics-option overrides for a clean, colour-true gallery ----
	// Saved on apply, restored on cleanup. Rationale mirrors the creature gallery:
	//  * auto-exposure OFF -> use the FIXED manual exposure (1.0) so brightness is
	//                    governed purely by the lights below (no AgX desaturation).
	//  * bloom OFF    -> no bright-pixel bloom glow.
	//  * skybox OFF   -> a neutral dark studio SOLID background instead of the
	//                    still-loaded FrontEnd's bright atmosphere.
	//  * text/quads OFF -> hides the still-loaded FrontEnd overlay (UI renders across
	//                    ALL loaded scenes).
	//  * SSR ON       -> screen-space reflections, so the smooth metallic floor
	//                    mirrors the models. Forced + saved so the gate never depends
	//                    on ambient config.
	// IBL ambient stays ON, so shadowed faces are filled rather than pure black.
	struct AGGraphicsSave
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
	AGGraphicsSave g_xAGGraphicsSave;

	void ApplyAGGraphicsOptions()
	{
		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		g_xAGGraphicsSave.m_bBloom        = xOpts.m_bHDRBloomEnabled;
		g_xAGGraphicsSave.m_bAutoExposure = xOpts.m_bHDRAutoExposureEnabled;
		g_xAGGraphicsSave.m_bSkybox       = xOpts.m_bSkyboxEnabled;
		g_xAGGraphicsSave.m_bText         = xOpts.m_bTextEnabled;
		g_xAGGraphicsSave.m_bQuads        = xOpts.m_bQuadsEnabled;
		g_xAGGraphicsSave.m_bSSR          = xOpts.m_bSSREnabled;
		g_xAGGraphicsSave.m_xSkyColour    = xOpts.m_xSkyboxColour;
		g_xAGGraphicsSave.m_bSaved        = true;

		xOpts.m_bHDRBloomEnabled        = false;
		xOpts.m_bHDRAutoExposureEnabled = false;   // fixed exposure 1.0 (see note above)
		xOpts.m_bSkyboxEnabled          = false;
		xOpts.m_bTextEnabled            = false;
		xOpts.m_bQuadsEnabled           = false;
		xOpts.m_bSSREnabled             = true;    // mirror the models in the floor
		xOpts.m_xSkyboxColour           = Zenith_Maths::Vector3(0.06f, 0.06f, 0.07f);
	}

	void RestoreAGGraphicsOptions()
	{
		if (!g_xAGGraphicsSave.m_bSaved)
		{
			return;
		}
		Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
		xOpts.m_bHDRBloomEnabled        = g_xAGGraphicsSave.m_bBloom;
		xOpts.m_bHDRAutoExposureEnabled = g_xAGGraphicsSave.m_bAutoExposure;
		xOpts.m_bSkyboxEnabled          = g_xAGGraphicsSave.m_bSkybox;
		xOpts.m_bTextEnabled            = g_xAGGraphicsSave.m_bText;
		xOpts.m_bQuadsEnabled           = g_xAGGraphicsSave.m_bQuads;
		xOpts.m_bSSREnabled             = g_xAGGraphicsSave.m_bSSR;
		xOpts.m_xSkyboxColour           = g_xAGGraphicsSave.m_xSkyColour;
		g_xAGGraphicsSave.m_bSaved      = false;
	}

	void FailAG(const char* szReason)
	{
		g_szAGFailure = szReason;
		g_bAGFailed = true;
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

	// Absolute Build/artifacts/zenithmon/s4/gallery dir derived from GAME_ASSETS_DIR
	// (<repo>/Games/Zenithmon/Assets/ -> up three -> <repo>), so it resolves
	// regardless of the process working directory.
	std::filesystem::path AGGalleryDir()
	{
		std::error_code xError;
		const std::filesystem::path xRepoRoot = std::filesystem::weakly_canonical(
			std::filesystem::path(GAME_ASSETS_DIR) / ".." / ".." / "..", xError);
		return xRepoRoot / "Build" / "artifacts" / "zenithmon" / "s4" / "gallery";
	}

	u_int CountLoadedAGModels()
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryActiveScene<Zenith_ModelComponent>().ForEach(
			[&uCount](Zenith_EntityID, Zenith_ModelComponent& xModel)
			{
				// Count only .zmodel-LOADED models (non-empty model path); the
				// procedural AddMeshEntry floor has an empty path and is excluded.
				if (xModel.HasModel() && xModel.GetNumMeshes() > 0u
					&& !xModel.GetModelPath().empty())
				{
					++uCount;
				}
			});
		return uCount;
	}

	Zenith_CameraComponent* FindAGCamera()
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
		if (uIndex >= 3u || g_astrAGShotPath[uIndex].empty())
		{
			return;
		}
		Flux_Screenshot::RequestDump(g_astrAGShotPath[uIndex].c_str());
		g_abAGShotRequested[uIndex] = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_AssetGallery] requested capture %u -> %s",
			uIndex, g_astrAGShotPath[uIndex].c_str());
	}

	void CleanupAGScene()
	{
		RestoreAGGraphicsOptions();
		Zenith_InputSimulator::ClearFixedDt();
		if (g_xAGPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xAGPreviousScene);
		}
		if (g_xAGScene.IsValid())
		{
			g_xEngine.Scenes().UnloadSceneForced(g_xAGScene);
			g_xAGScene = Zenith_Scene();
		}
		g_xAGFloorGeometry = MeshGeometryHandle();
		g_xAGFloorMaterial = MaterialHandle();
		g_bAGActive = false;
	}

	// One shared shape per family: resolve the family's MODEL ref -> create the
	// entity -> ground it at (X,0,rowZ) + uniform scale -> LoadModel. Skinned
	// (creature/human) and static (building/prop) both load via LoadModel; the
	// skinned families render in their bind pose (no animator attached). Humans'
	// .zmodel references the shared game:Humans/Shared/Human.zskel, which LoadModel
	// resolves. The per-entry uniform scale normalises each model to the family's
	// target display height by dividing out its baked natural-size field.

	void BuildAGCreature(Zenith_SceneData* pxSceneData, u_int uIndex)
	{
		const ZM_AGCreatureEntry& xEntry = g_axAGCreatures[uIndex];
		const ZM_CREATURE_ASSET_KIND eKind = xEntry.m_bShiny
			? ZM_CREATURE_ASSET_MODEL_SHINY
			: ZM_CREATURE_ASSET_MODEL;
		char szRef[256];
		if (!ZM_CreatureAssetPath(xEntry.m_eSpecies, eKind, szRef, static_cast<u_int>(sizeof(szRef))))
		{
			FailAG("could not resolve a creature model ref");
			return;
		}

		char szName[96];
		std::snprintf(szName, sizeof(szName), "%s%s",
			ZM_GetSpeciesName(xEntry.m_eSpecies), xEntry.m_bShiny ? "_Shiny" : "");

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		const float fX = (static_cast<float>(uIndex) - (static_cast<float>(uZM_AG_CREATURES) - 1.0f) * 0.5f)
			* fZM_AG_CREATURE_PITCH;
		xTransform.SetPosition({ fX, 0.0f, fZM_AG_CREATURE_ROW_Z });

		const float fSizeScale = ZM_ResolveCreatureRecipe(xEntry.m_eSpecies).m_fSizeScale;
		const float fScale = fZM_AG_CREATURE_HEIGHT / std::max(fSizeScale, fZM_AG_MIN_DENOM);
		xTransform.SetScale({ fScale, fScale, fScale });

		xEntity.AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
	}

	void BuildAGHuman(Zenith_SceneData* pxSceneData, u_int uIndex)
	{
		const ZM_HUMAN_ID eId = g_aeAGHumans[uIndex];
		char szRef[256];
		if (!ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_MODEL, szRef, static_cast<u_int>(sizeof(szRef))))
		{
			FailAG("could not resolve a human model ref");
			return;
		}

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, ZM_GetHumanName(eId));
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		const float fX = (static_cast<float>(uIndex) - (static_cast<float>(uZM_AG_HUMANS) - 1.0f) * 0.5f)
			* fZM_AG_HUMAN_PITCH;
		xTransform.SetPosition({ fX, 0.0f, fZM_AG_HUMAN_ROW_Z });

		const float fNaturalHeight = fZM_AG_HUMAN_NOMINAL_M * ZM_ResolveHumanRecipe(eId).m_fHeightScale;
		const float fScale = fZM_AG_HUMAN_HEIGHT / std::max(fNaturalHeight, fZM_AG_MIN_DENOM);
		xTransform.SetScale({ fScale, fScale, fScale });

		xEntity.AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
	}

	// Fit an instance into BOTH a column-width budget and a height cap -- the MIN of
	// the two scale ratios. This is the guard against the height-only-normalization
	// overlap (ZM-D-087): a wide, short model scaled by height alone would spill past
	// its column pitch and intersect its neighbour. fNaturalWidth = the model's X
	// footprint (incl. any roof overhang). Use for families with varying aspect ratios
	// (buildings/props); ~isotropic families (creatures/humans) may height-normalize.
	float AGFitScale(float fNaturalWidth, float fNaturalHeight, float fWidthBudget, float fHeightCap)
	{
		const float fWidthScale  = fWidthBudget / std::max(fNaturalWidth,  fZM_AG_MIN_DENOM);
		const float fHeightScale = fHeightCap   / std::max(fNaturalHeight, fZM_AG_MIN_DENOM);
		return std::min(fWidthScale, fHeightScale);
	}

	void BuildAGBuilding(Zenith_SceneData* pxSceneData, u_int uIndex)
	{
		const ZM_BUILDING_ID eId = g_aeAGBuildings[uIndex];
		char szRef[256];
		if (!ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MODEL, szRef, static_cast<u_int>(sizeof(szRef))))
		{
			FailAG("could not resolve a building model ref");
			return;
		}

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, ZM_GetBuildingName(eId));
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		const float fX = (static_cast<float>(uIndex) - (static_cast<float>(uZM_AG_BUILDINGS) - 1.0f) * 0.5f)
			* fZM_AG_BUILDING_PITCH;
		xTransform.SetPosition({ fX, 0.0f, fZM_AG_BUILDING_ROW_Z });

		const ZM_BuildingRecipe xRecipe = ZM_ResolveBuildingRecipe(eId);
		const float fNaturalHeight = xRecipe.m_fStoreyHeight * static_cast<float>(xRecipe.m_uStoreys);
		const float fNaturalWidth  = xRecipe.m_fWidth + fZM_AG_BUILDING_ROOF_OVERHANG;   // body + roof eaves
		// Fit BOTH the column-width budget AND the height cap so a wide low-storey
		// building can't scale up past the pitch and intersect its neighbour (ZM-D-087).
		const float fScale = AGFitScale(fNaturalWidth, fNaturalHeight,
			fZM_AG_BUILDING_WIDTH_BUDGET, fZM_AG_BUILDING_HEIGHT);
		xTransform.SetScale({ fScale, fScale, fScale });

		xEntity.AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
	}

	void BuildAGProp(Zenith_SceneData* pxSceneData, u_int uIndex)
	{
		const ZM_PROP_ID eId = g_aeAGProps[uIndex];
		char szRef[256];
		if (!ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, szRef, static_cast<u_int>(sizeof(szRef))))
		{
			FailAG("could not resolve a prop model ref");
			return;
		}

		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, ZM_GetPropName(eId));
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		const float fX = (static_cast<float>(uIndex) - (static_cast<float>(uZM_AG_PROPS) - 1.0f) * 0.5f)
			* fZM_AG_PROP_PITCH;
		xTransform.SetPosition({ fX, 0.0f, fZM_AG_PROP_ROW_Z });

		const ZM_PropRecipe xPropRecipe = ZM_ResolvePropRecipe(eId);
		// Same width-budget + height-cap fit as buildings (ZM-D-087): a wide prop
		// (e.g. FenceWood) can't scale up past its column pitch and intersect.
		const float fScale = AGFitScale(xPropRecipe.m_fWidth, xPropRecipe.m_fHeight,
			fZM_AG_PROP_WIDTH_BUDGET, fZM_AG_PROP_HEIGHT);
		xTransform.SetScale({ fScale, fScale, fScale });

		xEntity.AddComponent<Zenith_ModelComponent>().LoadModel(szRef);
	}
}

static void Setup_ZMAssetGallery()
{
	g_bAGActive = false;
	g_bAGFailed = false;
	g_szAGFailure = "test did not reach verification";
	g_uAGModelsLoadedCheck = 0u;
	g_abAGShotRequested[0] = g_abAGShotRequested[1] = g_abAGShotRequested[2] = false;
	g_xAGPreviousScene = Zenith_Scene();
	g_xAGScene = Zenith_Scene();

	// 1. Warm the baked families + skip guard FIRST (before any fixed-dt / scene /
	//    graphics state, since RequestSkip bypasses Verify). A tools build bakes
	//    the stale families and stamps them (fast when already warm); a non-tools
	//    build only checks the per-family manifests. CI checkouts have no baked
	//    Assets tree, so skip rather than fail.
#ifdef ZENITH_TOOLS
	const bool bWarm = ZM_BakeAllAssets();
#else
	const std::filesystem::path xRoot(GAME_ASSETS_DIR);
	const bool bWarm =
		ZM_BakeManifestCheck(ZM_ASSET_FAMILY_CREATURES, xRoot) &&
		ZM_BakeManifestCheck(ZM_ASSET_FAMILY_HUMANS,    xRoot) &&
		ZM_BakeManifestCheck(ZM_ASSET_FAMILY_BUILDINGS, xRoot) &&
		ZM_BakeManifestCheck(ZM_ASSET_FAMILY_PROPS,     xRoot);
#endif
	if (!bWarm)
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"baked asset families absent/stale (run a *_True build to bake Assets/)");
		return;
	}

	// 2. Capture destination: create it up front -- the swapchain dump uses fopen
	//    directly and does NOT create parent directories.
	const std::filesystem::path xGalleryDir = AGGalleryDir();
	std::error_code xDirError;
	std::filesystem::create_directories(xGalleryDir, xDirError);
	g_astrAGShotPath[0] = (xGalleryDir / "gallery_01.tga").string();
	g_astrAGShotPath[1] = (xGalleryDir / "gallery_02.tga").string();
	g_astrAGShotPath[2] = (xGalleryDir / "gallery_03.tga").string();

	// 3. Deterministic frame timing.
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);

	// 4. Neutral background + bloom/UI off for a gate-ready capture (restored in cleanup).
	ApplyAGGraphicsOptions();

	// 5. Isolated additive scene so the gallery never perturbs the persistent world.
	g_xAGPreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xAGScene = g_xEngine.Scenes().LoadScene(
		"ZM_AssetGallery", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(g_xAGScene);
	if (pxSceneData == nullptr || !g_xEngine.Scenes().SetActiveScene(g_xAGScene))
	{
		FailAG("could not create the isolated asset-gallery scene");
		return;
	}
	g_bAGActive = true;

	// 6a. Directional KEY light, from the upper +X/front. With auto-exposure OFF and
	//     a FIXED exposure of 1.0, a directional's intensity multiplies surface
	//     radiance DIRECTLY, so the value is O(1). Anchored to the material preview's
	//     proven fixed-exposure sun (intensity 2.5), travelling down + toward -Z so it
	//     lights the +Z faces presented to the camera. (Created FIRST so it is the
	//     first-gathered directional.)
	Zenith_Entity xLight = g_xEngine.Scenes().CreateEntity(pxSceneData, "AGKeyLight");
	Zenith_LightComponent& xLightComponent = xLight.AddComponent<Zenith_LightComponent>();
	xLightComponent.SetLightType(LIGHT_TYPE_DIRECTIONAL);
	xLightComponent.SetColor(Zenith_Maths::Vector3(1.0f, 0.97f, 0.90f));
	xLightComponent.SetIntensity(2.5f);
	xLightComponent.SetWorldDirection(
		glm::normalize(Zenith_Maths::Vector3(-0.35f, -0.85f, -0.40f)));

	// 6b. Gentle FILL from the opposite (-X) side, also lighting the +Z fronts, so the
	//     key's shadowed faces are not pure black. IBL ambient adds further fill.
	Zenith_Entity xFill = g_xEngine.Scenes().CreateEntity(pxSceneData, "AGFillLight");
	Zenith_LightComponent& xFillLight = xFill.AddComponent<Zenith_LightComponent>();
	xFillLight.SetLightType(LIGHT_TYPE_DIRECTIONAL);
	xFillLight.SetColor(Zenith_Maths::Vector3(0.85f, 0.90f, 1.0f));
	xFillLight.SetIntensity(1.0f);
	xFillLight.SetWorldDirection(
		glm::normalize(Zenith_Maths::Vector3(0.50f, -0.35f, -0.40f)));

	// 7. Framed gallery camera (main). Positioned to the initial front view.
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(pxSceneData, "AGCamera");
	Zenith_CameraComponent& xCameraComponent = xCamera.AddComponent<Zenith_CameraComponent>();
	xCameraComponent.SetFOV(glm::radians(55.0f));
	xCameraComponent.SetNearPlane(0.1f);
	xCameraComponent.SetFarPlane(500.0f);
	AimCameraAt(xCameraComponent, g_xAGViewFront, g_xAGViewAim);
	Zenith_UnitTests::SetMainCameraForTest(pxSceneData, xCamera.GetEntityID());

	// 8. Large smooth METALLIC floor slab at the models' feet (top face at y=0), so
	//    SSR mirrors every family below it. Built from the shared unit-cube geometry
	//    scaled into a thin 70x70 slab + an in-memory dark-metal material, attached
	//    via AddMeshEntry (the asset-free prop pattern; no .zmodel/.zmtrl disk bake).
	//    70x70 reaches beyond the widest row (buildings +/-30, creatures +/-26.25) and
	//    the Z span (-22..+18), so every model's reflection lands on it.
	g_xAGFloorGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_xAGFloorMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MeshGeometryAsset* pxFloorGeometryAsset = g_xAGFloorGeometry.GetDirect();
	Zenith_MaterialAsset* pxFloorMaterial = g_xAGFloorMaterial.GetDirect();
	Flux_MeshGeometry* pxFloorGeometry =
		(pxFloorGeometryAsset != nullptr) ? pxFloorGeometryAsset->GetGeometry() : nullptr;
	if (pxFloorMaterial != nullptr && pxFloorGeometry != nullptr)
	{
		pxFloorMaterial->SetName("ZM_AssetGalleryFloor");
		pxFloorMaterial->SetBaseColor({ 0.30f, 0.30f, 0.33f, 1.0f });   // dark neutral metal tint
		pxFloorMaterial->SetRoughness(0.06f);                            // smooth -> sharp mirror
		pxFloorMaterial->SetMetallic(1.0f);                              // full metallic reflectance

		Zenith_Entity xFloor = g_xEngine.Scenes().CreateEntity(pxSceneData, "AGFloor");
		Zenith_TransformComponent& xFloorTransform =
			xFloor.GetComponent<Zenith_TransformComponent>();
		// Unit cube is centred, so a 0.2-thick slab centred at y=-0.1 puts its top
		// face exactly at y=0 (the grounded models' feet).
		xFloorTransform.SetPosition({ 0.0f, -0.10f, 0.0f });
		xFloorTransform.SetScale({ 70.0f, 0.20f, 70.0f });
		Zenith_ModelComponent& xFloorModel = xFloor.AddComponent<Zenith_ModelComponent>();
		xFloorModel.AddMeshEntry(*pxFloorGeometry, *pxFloorMaterial);
	}

	// 9. The 26 representatives, family by family (props nearest, buildings farthest).
	for (u_int i = 0; i < uZM_AG_CREATURES && !g_bAGFailed; ++i)
	{
		BuildAGCreature(pxSceneData, i);
	}
	for (u_int i = 0; i < uZM_AG_HUMANS && !g_bAGFailed; ++i)
	{
		BuildAGHuman(pxSceneData, i);
	}
	for (u_int i = 0; i < uZM_AG_BUILDINGS && !g_bAGFailed; ++i)
	{
		BuildAGBuilding(pxSceneData, i);
	}
	for (u_int i = 0; i < uZM_AG_PROPS && !g_bAGFailed; ++i)
	{
		BuildAGProp(pxSceneData, i);
	}
}

static bool Step_ZMAssetGallery(int iFrame)
{
	if (!g_bAGActive || g_bAGFailed)
	{
		return false;
	}

	Zenith_CameraComponent* pxCamera = FindAGCamera();
	if (pxCamera == nullptr)
	{
		FailAG("asset-gallery camera disappeared mid-run");
		return false;
	}

	// LoadModel is synchronous, so all 26 instances exist immediately; the early
	// frames only let the GPU upload settle before the first capture.
	if (iFrame == 30)
	{
		g_uAGModelsLoadedCheck = CountLoadedAGModels();
		if (g_uAGModelsLoadedCheck != uZM_AG_TOTAL)
		{
			FailAG("not every representative .zmodel produced a renderable instance");
			return false;
		}
	}

	// Set each viewpoint a few frames ahead of its capture so the new view matrix
	// is rendered before the swapchain consumes the dump.
	switch (iFrame)
	{
	case 60:  AimCameraAt(*pxCamera, g_xAGViewFront, g_xAGViewAim); break;
	case 66:  RequestShot(0); break;   // front elevated
	case 78:  AimCameraAt(*pxCamera, g_xAGViewLeft, g_xAGViewAim);  break;
	case 84:  RequestShot(1); break;   // left 3/4
	case 96:  AimCameraAt(*pxCamera, g_xAGViewRight, g_xAGViewAim); break;
	case 102: RequestShot(2); break;   // right 3/4
	case 250: return false;            // hold for an external capture, then end
	default:  break;
	}
	return true;
}

static bool Verify_ZMAssetGallery()
{
	bool bPassed = true;
	if (g_bAGFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_AssetGallery] %s", g_szAGFailure);
		bPassed = false;
	}

	// The model + capture assertions are only meaningful once the scene was built.
	if (g_bAGActive)
	{
		if (g_uAGModelsLoadedCheck != uZM_AG_TOTAL)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_AssetGallery] expected %u renderable models, saw %u",
				uZM_AG_TOTAL, g_uAGModelsLoadedCheck);
			bPassed = false;
		}
		for (u_int i = 0; i < 3u; ++i)
		{
			if (!g_abAGShotRequested[i])
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_AssetGallery] capture %u was never requested", i);
				bPassed = false;
				continue;
			}
			if (!DiskFilePresent(g_astrAGShotPath[i]))
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_AssetGallery] capture %u produced no file on disk: %s",
					i, g_astrAGShotPath[i].c_str());
				bPassed = false;
			}
		}
	}

	// Always restore graphics options + fixed dt + unload the scene (all guarded),
	// even if Setup applied the overrides but scene creation then failed.
	CleanupAGScene();
	return bPassed;
}

static const Zenith_AutomatedTest g_xZMAssetGalleryTest = {
	"ZM_AssetGallery_Test",
	&Setup_ZMAssetGallery,
	&Step_ZMAssetGallery,
	&Verify_ZMAssetGallery,
	/* maxFrames */ 360,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMAssetGalleryTest);

#endif // ZENITH_INPUT_SIMULATOR
