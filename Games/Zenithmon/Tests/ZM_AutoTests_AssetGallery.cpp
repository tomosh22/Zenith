#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_Screenshot.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Translucency/Flux_TranslucencyImpl.h"
#include "Flux/UnifiedMesh/Flux_UnifiedMeshImpl.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
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
#include "Zenithmon/Source/World/ZM_HumanBody.h"
#ifdef ZENITH_TOOLS
#include "../../../Tools/Zenith_Tools_HumanModelExport.h"
#include "../../../Tools/Zenith_Tools_GlbImport.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include "Zenithmon/Source/Gen/ZM_AuthoredCreature.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

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
		Zenith_Error(LOG_CATEGORY_UNITTEST,"[Authored asset review] %s",szReason);
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
	// entity -> place it at (X,y,rowZ) + uniform scale -> LoadModel. Props and
	// buildings use their authored feet origin; creatures use their measured
	// grounded/hover policy; humans are authored FEET-first like the props, and the
	// gallery lifts their model with the same model-space offset the game uses
	// (fZM_HUMAN_MODEL_OFFSET_Y) because the shared rig is hip-at-origin. Skinned
	// (creature/human) and static (building/prop) both load via LoadModel; the
	// skinned families render in their bind pose (no animator attached). Humans'
	// .zmodel references the SHARED ENGINE rig
	// engine:Meshes/StickFigure/StickFigure.zskel, which LoadModel resolves. The per-entry uniform scale normalises each model to the family's
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
		const float fSizeScale = ZM_ResolveCreatureRecipe(xEntry.m_eSpecies).m_fSizeScale;
		const float fScale = fZM_AG_CREATURE_HEIGHT / std::max(fSizeScale, fZM_AG_MIN_DENOM);
		// The gallery deliberately normalises family display size, but it must still
		// honour the same measured mesh-origin policy as the battle presenter or a
		// serpent can sink through the common floor while a floater drifts arbitrarily.
		auto& xModel = xEntity.AddComponent<Zenith_ModelComponent>(); xModel.LoadModel(szRef);
		Zenith_Maths::Vector3 xMin, xMax;
		const float fOffset = xModel.GetFirstMeshLocalBounds(xMin, xMax)
			? ZM_CreaturePresentationFloorY(ZM_GetCreatureOriginPolicy(ZM_GetSpeciesData(xEntry.m_eSpecies).m_eArchetype)) - xMin.y
			: ZM_CreaturePresentationOriginOffsetY(xEntry.m_eSpecies);
		xTransform.SetPosition({ fX, fOffset * fScale, fZM_AG_CREATURE_ROW_Z });
		xTransform.SetScale({ fScale, fScale, fScale });

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
		// Humans stand ON the gallery floor (ZM_HumanGen v6): the entity origin is
		// the FEET, like the props and buildings in the rows beside them.
		xTransform.SetPosition({ fX, 0.0f, fZM_AG_HUMAN_ROW_Z });

		// MEASURED, not nominal. The old fZM_AG_HUMAN_NOMINAL_M * m_fHeightScale
		// guessed at the loft's output; ZM_MeasureHumanBody reports what the loft
		// actually built, over the body prefix (a hat must not shrink its wearer).
		const ZM_HumanBodyMetrics xMetrics = ZM_MeasureHumanBody(eId);
		const float fNaturalHeight = xMetrics.m_fHeight;
		const float fScale = fZM_AG_HUMAN_HEIGHT / std::max(fNaturalHeight, fZM_AG_MIN_DENOM);
		xTransform.SetScale({ fScale, fScale, fScale });

		Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
		// ★ THE SHARED RIG IS HIP-AT-ORIGIN, so the model must be lifted by the depth
		// its soles hang below that origin or every human in the gallery stands buried
		// to the hip. Measured PER MODEL rather than reusing the game's canonical
		// fZM_HUMAN_MODEL_OFFSET_Y, because the gallery re-scales each human to a
		// common display height and the build variation is exactly what it is showing.
		xModel.SetModelSpaceOffset(Zenith_Maths::Vector3(0.0f, -xMetrics.m_fMinY, 0.0f));
		xModel.LoadModel(szRef);
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

// Full architecture review world. Every roster building is placed at its real
// metre scale on a separate plot, so new sources can be reviewed even before
// their story town has been authored. Dawnmere still loads Home/Lab normally.
namespace
{
	u_int g_uArchitectureShots = 0;
	std::string ArchitectureShot(u_int u)
	{
		const auto xRoot = std::filesystem::weakly_canonical(std::filesystem::path(GAME_ASSETS_DIR) / "../../..");
		return (xRoot / "Build/artifacts/authored_assets/architecture" /
			(std::string(ZM_GetBuildingName(static_cast<ZM_BUILDING_ID>(u))) + ".tga")).string();
	}
	Zenith_Maths::Vector3 ArchitecturePlot(u_int u)
	{
		return { static_cast<float>(u % 6u) * 65.0f, 0.0f, static_cast<float>(u / 6u) * 65.0f };
	}
	void SetupArchitecture()
	{
		g_bAGFailed = false; g_bAGActive = false; g_uArchitectureShots = 0;
		ApplyAGGraphicsOptions();
		Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
		g_xAGPreviousScene = g_xEngine.Scenes().GetActiveScene();
		g_xAGScene = g_xEngine.Scenes().LoadScene("ZM_AuthoredArchitecture", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		auto* pxScene = g_xEngine.Scenes().GetSceneData(g_xAGScene);
		if (!pxScene || !g_xEngine.Scenes().SetActiveScene(g_xAGScene)) { FailAG("architecture scene unavailable"); return; }
		g_bAGActive = true;
		auto xCamera = g_xEngine.Scenes().CreateEntity(pxScene, "ArchitectureCamera");
		auto& xCam = xCamera.AddComponent<Zenith_CameraComponent>();
		xCam.SetFOV(glm::radians(52.0f)); xCam.SetNearPlane(.1f); xCam.SetFarPlane(120.0f);
		Zenith_UnitTests::SetMainCameraForTest(pxScene, xCamera.GetEntityID());
		auto xSun = g_xEngine.Scenes().CreateEntity(pxScene, "ArchitectureSun");
		auto& xLight = xSun.AddComponent<Zenith_LightComponent>();
		xLight.SetLightType(LIGHT_TYPE_DIRECTIONAL); xLight.SetColor({1.0f,.96f,.88f});
		xLight.SetIntensity(2.5f); xLight.SetWorldDirection(glm::normalize(Zenith_Maths::Vector3(-.4f,-.8f,.35f)));
		g_xAGFloorGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
		g_xAGFloorMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		auto* pxMat = g_xAGFloorMaterial.GetDirect();
		pxMat->SetName("Architecture gravel plot"); pxMat->SetBaseColor({.24f,.24f,.21f,1});
		pxMat->SetRoughness(.92f); pxMat->SetMetallic(0);
		for (u_int u=0; u<ZM_BUILDING_COUNT; ++u)
		{
			const auto eId = static_cast<ZM_BUILDING_ID>(u);
			std::error_code xError;
			std::filesystem::create_directories(std::filesystem::path(ArchitectureShot(u)).parent_path(), xError);
			std::filesystem::remove(ArchitectureShot(u), xError);
			char acRef[512]; ZM_BuildingAssetPath(eId,ZM_BUILDING_ASSET_MODEL,acRef,sizeof(acRef));
			auto xSource = std::filesystem::path(Zenith_AssetRegistry::ResolvePath(acRef)); xSource.replace_extension(".glb");
			if (!DiskFilePresent(xSource.string()) || !ZM_EnsureBuildingBaked(eId)) { FailAG("authored building source/import missing"); return; }
			auto xEntity = g_xEngine.Scenes().CreateEntity(pxScene,ZM_GetBuildingName(eId));
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(ArchitecturePlot(u));
			auto& xModel=xEntity.AddComponent<Zenith_ModelComponent>(); xModel.LoadModel(acRef);
			if (xModel.GetModelPath()!=acRef || !xModel.GetNumMeshes()) { FailAG("authored building failed to load"); return; }
			auto xFloor=g_xEngine.Scenes().CreateEntity(pxScene,"ArchitecturePlot");
			xFloor.GetComponent<Zenith_TransformComponent>().SetPosition(ArchitecturePlot(u)+Zenith_Maths::Vector3(0,-.1f,0));
			xFloor.GetComponent<Zenith_TransformComponent>().SetScale({44,.2f,44});
			xFloor.AddComponent<Zenith_ModelComponent>().AddMeshEntry(*g_xAGFloorGeometry.GetDirect()->GetGeometry(),*pxMat);
		}
	}
	bool StepArchitecture(int iFrame)
	{
		if (!g_bAGActive || g_bAGFailed) { return false; }
		const u_int u=static_cast<u_int>(iFrame/150);
		if(u>=ZM_BUILDING_COUNT) { return false; }
		const auto& xData=ZM_GetBuildingData(static_cast<ZM_BUILDING_ID>(u));
		// Model bounds vary with architectural silhouette (spires, tank domes,
		// stepped tower). A generous plot-sized lens keeps the entire roof visible.
		const auto xBase=ArchitecturePlot(u);
		const float fDistance=(u==ZM_BUILDING_BATTLE_TOWER)?23.0f:std::max(10.0f,std::max(xData.m_fWidth,xData.m_fDepth)*1.25f);
		auto* pxCamera=FindAGCamera();
		if(!pxCamera) { FailAG("architecture camera missing"); return false; }
		pxCamera->SetFarPlane(fDistance*2.0f);
		AimCameraAt(*pxCamera,xBase+Zenith_Maths::Vector3(fDistance*.65f,fDistance*.50f,-fDistance),
			xBase+Zenith_Maths::Vector3(0,(u==ZM_BUILDING_BATTLE_TOWER)?8.0f:3.5f,0));
		if(iFrame%150==130) { Flux_Screenshot::RequestDump(ArchitectureShot(u).c_str()); ++g_uArchitectureShots; }
		return true;
	}
	bool VerifyArchitecture()
	{
		bool bOk=!g_bAGFailed && g_uArchitectureShots==ZM_BUILDING_COUNT && CountLoadedAGModels()==ZM_BUILDING_COUNT;
		for(u_int u=0;u<ZM_BUILDING_COUNT;++u) { bOk=DiskFilePresent(ArchitectureShot(u)) && bOk; }
		CleanupAGScene(); return bOk;
	}
}
static const Zenith_AutomatedTest g_xArchitectureCapture = {
	"ZM_AuthoredArchitectureCapture_Test", &SetupArchitecture, &StepArchitecture,
	&VerifyArchitecture, 4700, true, true, &CleanupAGScene,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xArchitectureCapture);

#ifdef ZENITH_TOOLS
namespace
{
	std::vector<Zenith_EntityID> g_xCastEntities;
	u_int g_uCastShots=0;
	std::string CastShot(u_int u, bool bWalk)
	{
		const auto xRoot=std::filesystem::weakly_canonical(std::filesystem::path(GAME_ASSETS_DIR)/"../../..");
		return (xRoot/"Build/artifacts/authored_assets/cast"/(std::string(ZM_GetHumanName(static_cast<ZM_HUMAN_ID>(u)))+(bWalk?"_walk.tga":"_idle.tga"))).string();
	}
	void SetupCast()
	{
		g_bAGFailed=false;g_bAGActive=false;g_uCastShots=0;g_xCastEntities.clear();
		ApplyAGGraphicsOptions();Zenith_InputSimulator::SetFixedDt(1.0f/60.0f);
		g_xAGPreviousScene=g_xEngine.Scenes().GetActiveScene();
		g_xAGScene=g_xEngine.Scenes().LoadScene("ZM_AuthoredCast",SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		auto* pxScene=g_xEngine.Scenes().GetSceneData(g_xAGScene);
		if(!pxScene||!g_xEngine.Scenes().SetActiveScene(g_xAGScene)){FailAG("cast scene unavailable");return;}
		g_bAGActive=true;
		auto xCamera=g_xEngine.Scenes().CreateEntity(pxScene,"CastCamera");
		auto& xCam=xCamera.AddComponent<Zenith_CameraComponent>();xCam.SetFOV(glm::radians(45.0f));xCam.SetNearPlane(.05f);xCam.SetFarPlane(8);
		Zenith_UnitTests::SetMainCameraForTest(pxScene,xCamera.GetEntityID());
		auto xSun=g_xEngine.Scenes().CreateEntity(pxScene,"CastKey");auto& xLight=xSun.AddComponent<Zenith_LightComponent>();
		xLight.SetLightType(LIGHT_TYPE_DIRECTIONAL);xLight.SetColor({1,.96f,.90f});xLight.SetIntensity(2.5f);xLight.SetWorldDirection(glm::normalize(Zenith_Maths::Vector3(-.4f,-.7f,-.5f)));
		g_xAGFloorGeometry=Zenith_MeshGeometryAsset::CreateUnitCube();g_xAGFloorMaterial=Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		auto* pxMat=g_xAGFloorMaterial.GetDirect();pxMat->SetName("Cast review floor");pxMat->SetBaseColor({.20f,.21f,.19f,1});pxMat->SetRoughness(.9f);
		auto xFloor=g_xEngine.Scenes().CreateEntity(pxScene,"CastFloor");
		xFloor.GetComponent<Zenith_TransformComponent>().SetPosition({85,-.1f,0});xFloor.GetComponent<Zenith_TransformComponent>().SetScale({180,.2f,10});
		xFloor.AddComponent<Zenith_ModelComponent>().AddMeshEntry(*g_xAGFloorGeometry.GetDirect()->GetGeometry(),*pxMat);
		for(u_int u=0;u<ZM_HUMAN_COUNT;++u)
		{
			const auto eId=static_cast<ZM_HUMAN_ID>(u);const char* szRef=ZM_HumanImportedModelRef(eId);
			if(!szRef){FailAG("cast imported path missing");return;}
			auto xModelFile=std::filesystem::path(Zenith_AssetRegistry::ResolvePath(szRef));auto xSource=xModelFile;xSource.replace_extension(".glb");
			std::error_code xError;
			if(!DiskFilePresent(xSource.string())){FailAG("cast source missing");return;}
			if(!DiskFilePresent(xModelFile.string()) || std::filesystem::last_write_time(xSource,xError)>std::filesystem::last_write_time(xModelFile,xError))
			{
				if(!Zenith_Tools_HumanModelExport::ExportBoundHumanModel(xSource.string()).m_bSuccess){FailAG("cast binding rejected");return;}
			}
			std::filesystem::create_directories(std::filesystem::path(CastShot(u,false)).parent_path(),xError);
			std::filesystem::remove(CastShot(u,false),xError);std::filesystem::remove(CastShot(u,true),xError);
			auto xEntity=g_xEngine.Scenes().CreateEntity(pxScene,ZM_GetHumanName(eId));
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({u*5.0f,0,0});
			xEntity.GetComponent<Zenith_TransformComponent>().SetScale({fZM_HUMAN_VISUAL_SCALE,fZM_HUMAN_VISUAL_SCALE,fZM_HUMAN_VISUAL_SCALE});
			auto& xModel=xEntity.AddComponent<Zenith_ModelComponent>();xModel.LoadModel(szRef);
			if(xModel.GetModelPath()!=szRef || !xModel.GetSkeletonInstance()){FailAG("cast model has no valid shared rig");return;}
			const auto* pxAsset=Zenith_AssetRegistry::GetView<Zenith_ModelAsset>(szRef);
			if(!pxAsset||!pxAsset->GetNumMeshes()){FailAG("cast model binding missing");return;}
			const auto* pxMesh=Zenith_AssetRegistry::GetView<Zenith_MeshAsset>(pxAsset->GetMeshBinding(0).GetMeshPath());
			if(!pxMesh){FailAG("cast mesh missing");return;}
			xModel.SetModelSpaceOffset({0,-pxMesh->GetBoundsMin().y,0});
			auto& xAnimator=xEntity.AddComponent<Zenith_AnimatorComponent>();xAnimator.GetController().Initialize(xModel.GetSkeletonInstance());
			// Imported humans bind the shared rig; the game loads its five used
			// clips explicitly rather than reading the model's animation list.
			for(u_int c=0;c<ZM_HUMAN_CLIP_COUNT;++c)
			{
				char acClip[512];
				if(!ZM_HumanSharedAssetPath(static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE+c),acClip,sizeof(acClip)) ||
					!xAnimator.AddClipFromFile(Zenith_AssetRegistry::ResolvePath(acClip))){FailAG("cast clip failed to load");return;}
			}
			g_xCastEntities.push_back(xEntity.GetEntityID());
		}
	}
	bool StepCast(int iFrame)
	{
		if(g_bAGFailed||!g_bAGActive){return false;}
		const u_int u=static_cast<u_int>(iFrame/240);if(u>=g_xCastEntities.size()){return false;}
		auto xEntity=g_xEngine.Scenes().ResolveEntity(g_xCastEntities[u]);auto& xAnimator=xEntity.GetComponent<Zenith_AnimatorComponent>();
		const int iLocal=iFrame%240;
		if(iLocal==2)xAnimator.PlayAnimation(ZM_HumanClipName(ZM_HUMAN_CLIP_IDLE),0);
		if(iLocal==122)xAnimator.PlayAnimation(ZM_HumanClipName(ZM_HUMAN_CLIP_WALK),0);
		auto* pxCamera=FindAGCamera();if(!pxCamera){FailAG("cast camera missing");return false;}
		const Zenith_Maths::Vector3 xBase(u*5.0f,0,0);
		AimCameraAt(*pxCamera,xBase+Zenith_Maths::Vector3(.85f,1.18f,3.2f),xBase+Zenith_Maths::Vector3(0,.93f,0));
		if(iLocal==110||iLocal==230){Flux_Screenshot::RequestDump(CastShot(u,iLocal==230).c_str());++g_uCastShots;}
		return true;
	}
	bool VerifyCast()
	{
		bool bOk=!g_bAGFailed&&g_uCastShots==ZM_HUMAN_COUNT*2u;
		for(u_int u=0;u<ZM_HUMAN_COUNT;++u)bOk=DiskFilePresent(CastShot(u,false))&&DiskFilePresent(CastShot(u,true))&&bOk;
		CleanupAGScene();g_xCastEntities.clear();return bOk;
	}
}
static const Zenith_AutomatedTest g_xCastCapture={"ZM_AuthoredCastCapture_Test",&SetupCast,&StepCast,&VerifyCast,8600,true,true,&CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCastCapture);

// Development review of skinned sources. This harness publishes its separate
// bundle in Build/artifacts; production source installation is an explicit step.
namespace
{
	Zenith_EntityID g_xCreatureStudyEntity;
	Zenith_EntityID g_xCreatureStudyShinyEntity;
	u_int g_uCreatureStudyShots = 0;
	std::string g_strCreatureStudyDirectory = "FernfawnReference", g_strCreatureStudyName = "Fernfawn";
	float g_fCreatureStudyHeight = .8f, g_fCreatureStudySize = .8f, g_fCreatureStudyOffset = 0;
	std::filesystem::path CreatureStudyPath(const std::string& strSuffix)
	{
		const auto xRoot = std::filesystem::weakly_canonical(std::filesystem::path(GAME_ASSETS_DIR) / "../../..");
		return xRoot / "Build/artifacts/authored_assets/creature_work" / g_strCreatureStudyDirectory / (g_strCreatureStudyName + strSuffix);
	}
	std::string CreatureStudyShot(u_int u)
	{
		if (u == ZM_ANIM_CLIP_COUNT) return CreatureStudyPath("_engine_Shiny_Idle.tga").string();
		return CreatureStudyPath(std::string("_engine_") + ZM_CreatureClipName(static_cast<ZM_ANIM_CLIP>(u)) + ".tga").string();
	}
	void SetupCreatureStudyForSpecies(ZM_SPECIES_ID eSpecies, const std::string& strDirectory)
	{
		g_strCreatureStudyName = ZM_GetSpeciesName(eSpecies); g_strCreatureStudyDirectory = strDirectory;
		g_bAGFailed = false; g_bAGActive = false; g_uCreatureStudyShots = 0;
		const auto strSource = CreatureStudyPath(".glb").string();
		if (!DiskFilePresent(strSource)) { Zenith_AutomatedTestRunner::RequestSkip("Requested creature authoring source is absent"); return; }
		const auto strModel = CreatureStudyPath(".zmodel").string();
		const auto strShinyModel = CreatureStudyPath("_shiny.zmodel").string();
		bool bImported = false;
		if (!ZM_AuthoredCreature::Try(eSpecies, strModel, true, bImported) || !bImported)
			{ FailAG("authored creature bundle import failed"); return; }
		// A warm boot must preserve the published bundle, including the authored
		// collapse, instead of running either the static importer or the generator.
		std::error_code xError;
		const auto xModelTime = std::filesystem::last_write_time(strModel, xError);
		const auto xClipTime = std::filesystem::last_write_time(CreatureStudyPath("_Faint.zanim"), xError);
		if (xError || !ZM_AuthoredCreature::Try(eSpecies, strModel, false, bImported) || !bImported ||
			std::filesystem::last_write_time(strModel, xError) != xModelTime ||
			std::filesystem::last_write_time(CreatureStudyPath("_Faint.zanim"), xError) != xClipTime || xError)
			{ FailAG("authored creature warm import changed the bundle"); return; }
		for (u_int u = 0; u <= ZM_ANIM_CLIP_COUNT; ++u) std::filesystem::remove(CreatureStudyShot(u), xError);
		std::filesystem::remove(CreatureStudyPath("_engine_Faint_033.tga"), xError);
		std::filesystem::remove(CreatureStudyPath("_engine_Faint_067.tga"), xError);
		ApplyAGGraphicsOptions(); Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
		g_xAGPreviousScene = g_xEngine.Scenes().GetActiveScene();
		g_xAGScene = g_xEngine.Scenes().LoadScene("ZM_CreatureStudy", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		auto* pxScene = g_xEngine.Scenes().GetSceneData(g_xAGScene);
		if (!pxScene || !g_xEngine.Scenes().SetActiveScene(g_xAGScene)) { FailAG("creature study scene unavailable"); return; }
		g_bAGActive = true;
		auto xCamera = g_xEngine.Scenes().CreateEntity(pxScene, "CreatureStudyCamera");
		auto& xCam = xCamera.AddComponent<Zenith_CameraComponent>(); xCam.SetFOV(glm::radians(40.f)); xCam.SetNearPlane(.02f); xCam.SetFarPlane(15);
		Zenith_UnitTests::SetMainCameraForTest(pxScene, xCamera.GetEntityID());
		AimCameraAt(xCam, {51.12f,.78f,1.65f}, {50,.44f,.03f});
		auto xSun = g_xEngine.Scenes().CreateEntity(pxScene, "CreatureStudySun"); auto& xLight = xSun.AddComponent<Zenith_LightComponent>();
		xLight.SetLightType(LIGHT_TYPE_DIRECTIONAL); xLight.SetColor({1,.96f,.90f}); xLight.SetIntensity(2.5f);
		xLight.SetWorldDirection(glm::normalize(Zenith_Maths::Vector3(-.4f,-.7f,-.5f)));
		if (strDirectory.rfind("Windseed/", 0) == 0)
		{
			// The dense seed crown shades its stem. A low studio fill exposes
			// leaf and pod response for review without changing source materials.
			auto xFill = g_xEngine.Scenes().CreateEntity(pxScene, "WindseedStudyFill");
			auto& xFillLight = xFill.AddComponent<Zenith_LightComponent>();
			xFillLight.SetLightType(LIGHT_TYPE_DIRECTIONAL); xFillLight.SetColor({.85f,.9f,1.f});
			xFillLight.SetIntensity(1.f);
			xFillLight.SetWorldDirection(glm::normalize(Zenith_Maths::Vector3(.6f,-.25f,-.7f)));
		}
		g_xAGFloorGeometry = Zenith_MeshGeometryAsset::CreateUnitCube(); g_xAGFloorMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		auto* pxMat = g_xAGFloorMaterial.GetDirect(); pxMat->SetName("Creature review ground"); pxMat->SetBaseColor({.16f,.19f,.12f,1}); pxMat->SetRoughness(.96f);
		// Separate large normal/shiny models beyond the camera far plane. A tall
		// antlered creature's wider framing could otherwise reveal the other pose.
		for (u_int v = 0; v < 2; ++v)
		{
			auto xFloor = g_xEngine.Scenes().CreateEntity(pxScene, v ? "CreatureStudyShinyGround" : "CreatureStudyGround");
			xFloor.GetComponent<Zenith_TransformComponent>().SetPosition({50+v*50.f,-.052f,0}); xFloor.GetComponent<Zenith_TransformComponent>().SetScale({32,.1f,12});
			xFloor.AddComponent<Zenith_ModelComponent>().AddMeshEntry(*g_xAGFloorGeometry.GetDirect()->GetGeometry(), *pxMat);
		}
		for (u_int v = 0; v < 2; ++v)
		{
			auto xEntity = g_xEngine.Scenes().CreateEntity(pxScene, (g_strCreatureStudyName + (v ? "ShinyStudy" : "Study")).c_str());
			(v ? g_xCreatureStudyShinyEntity : g_xCreatureStudyEntity) = xEntity.GetEntityID();
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({50 + v * 50.f,0,0});
			auto& xModel = xEntity.AddComponent<Zenith_ModelComponent>(); xModel.LoadModel(v ? strShinyModel : strModel);
			if (!xModel.GetSkeletonInstance() || !xModel.GetNumMeshes()) { FailAG("creature study has no runtime skin"); return; }
			Zenith_Maths::Vector3 lo, hi;
			if (!xModel.GetFirstMeshLocalBounds(lo, hi)) { FailAG("creature study has no bounds"); return; }
			g_fCreatureStudyHeight = hi.y;
			g_fCreatureStudyOffset = ZM_CreaturePresentationFloorY(ZM_GetCreatureOriginPolicy(ZM_ResolveCreatureRecipe(eSpecies).m_eArchetype)) - lo.y;
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({50 + v * 50.f,g_fCreatureStudyOffset,0});
			g_fCreatureStudySize = std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
			auto& xAnimator = xEntity.AddComponent<Zenith_AnimatorComponent>(); xAnimator.GetController().Initialize(xModel.GetSkeletonInstance());
			for (u_int u = 0; u < ZM_ANIM_CLIP_COUNT; ++u)
			{
				const auto strClip = CreatureStudyPath(std::string("_") + ZM_CreatureClipName(static_cast<ZM_ANIM_CLIP>(u)) + ".zanim").string();
				if (!xAnimator.AddClipFromFile(strClip)) { FailAG("creature study clip cannot load"); return; }
			}
		}
	}
	void SetupCreatureStudy() { SetupCreatureStudyForSpecies(ZM_SPECIES_FERNFAWN, "FernfawnReference"); }
	bool CheckSkinningWithoutOpaqueDraws()
	{
		// Exercise the production prepare with real animated fish, but no opaque
		// draw items, then no skin jobs. Restore before this frame's normal gather.
		auto& xRenderer = g_xEngine.FluxRenderer();
		auto& xUnified = g_xEngine.UnifiedMesh();
		const u_int uJobs = xRenderer.GetUnifiedSkinJobs().GetSize();
		if (!uJobs) return false;
		auto xScene = std::move(xRenderer.m_xUnifiedGPUScene);
		xUnified.m_uSkinJobCount = 0;
		xUnified.GatherUnifiedPacket(nullptr);
		const bool bSkinOnly = xUnified.m_uSkinJobCount == uJobs && xUnified.m_uTotalDrawItems == 0;
		auto xJobs = std::move(xRenderer.m_axUnifiedSkinJobs);
		xUnified.GatherUnifiedPacket(nullptr);
		const bool bEmpty = xUnified.m_uSkinJobCount == 0;
		xRenderer.m_axUnifiedSkinJobs = std::move(xJobs);
		xRenderer.m_xUnifiedGPUScene = std::move(xScene);
		xUnified.GatherUnifiedPacket(nullptr);
		return bSkinOnly && bEmpty && xUnified.m_uSkinJobCount == uJobs;
	}
	bool CheckShoalfishTranslucency(Flux_ModelInstance& xModel)
	{
		const auto& xPacket = g_xEngine.Translucency().m_axDrawPackets[kuFluxViewSlotMain];
		u_int uExpected = 0;
		for (u_int m = 0; m < xModel.GetNumMeshes(); ++m)
		{
			Flux_SkinnedInstanceKey xKey;
			xKey.m_pvSkeleton = xModel.GetSkeletonInstance();
			xKey.m_pvMeshAsset = xModel.GetMeshInstance(m)->GetSourceAsset();
			xKey.m_uSubmeshSlot = m;
			const auto* pxSkin = g_xEngine.FluxRenderer().FindUnifiedSkinnedDraw(xKey);
			for (u_int s = 0; s < xModel.GetNumDrawSections(m); ++s)
			{
				Flux_MeshDrawSection xSection;
				if (!xModel.GetDrawSection(m, s, xSection)) return false;
				auto* pxMat = xModel.GetMeshMaterial(m, xSection.m_uMaterialSlot);
				if (!pxMat || pxMat->GetResolved().m_xParams.m_eBlendMode != MATERIAL_BLEND_TRANSLUCENT) continue;
				const auto* pxAlbedo = pxMat->GetDiffuseTexture();
				if (!pxAlbedo || pxAlbedo->GetFormat() != TEXTURE_FORMAT_BC3_RGBA_SRGB) return false;
				++uExpected;
				bool bFound = false;
				for (u_int i = 0; i < xPacket.GetSize(); ++i)
				{
					const auto& xDraw = xPacket.Get(i);
					if (pxSkin && xDraw.m_pxMaterial == pxMat && xDraw.m_bSkinned &&
						xDraw.m_pxMeshInstance == pxSkin->m_pxMesh && xDraw.m_uVertexOffset == pxSkin->m_uVertexOffset &&
						xDraw.m_uFirstIndex == xSection.m_uFirstIndex && xDraw.m_uIndexCount == xSection.m_uIndexCount)
						bFound = true;
				}
				if (!bFound) return false;
			}
		}
		return uExpected > 0;
	}
	bool StepCreatureStudy(int iFrame)
	{
		if (!g_bAGActive || g_bAGFailed) return false;
		const u_int u = static_cast<u_int>(iFrame / 150); if (u > ZM_ANIM_CLIP_COUNT) return false;
		const bool bShiny = u == ZM_ANIM_CLIP_COUNT;
		auto xEntity = g_xEngine.Scenes().ResolveEntity(bShiny ? g_xCreatureStudyShinyEntity : g_xCreatureStudyEntity); auto& xAnimator = xEntity.GetComponent<Zenith_AnimatorComponent>();
		auto* pxCamera = FindAGCamera(); if (!pxCamera) { FailAG("creature study camera missing"); return false; }
		const float fBase = bShiny ? 100.f : 50.f;
		const bool bScanReview = g_strCreatureStudyDirectory == "MineralBeetlesScan/Prismite";
		const bool bShoalfishReview = g_strCreatureStudyDirectory.rfind("Shoalfish/", 0) == 0;
		if (bShoalfishReview && iFrame == 15 && !CheckSkinningWithoutOpaqueDraws())
			{ FailAG("skin-only or empty scene retained incorrect skin jobs"); return false; }
		AimCameraAt(*pxCamera, {fBase+g_fCreatureStudySize*(bScanReview ? .8f : 1.15f),
			g_fCreatureStudyOffset+(bScanReview ? g_fCreatureStudySize*.85f : g_fCreatureStudyHeight*.95f),
			g_fCreatureStudySize*(bScanReview ? 1.3f : bShoalfishReview ? .65f : 2.1f)},
			{fBase,g_fCreatureStudyOffset+g_fCreatureStudyHeight*.48f,.03f});
		const auto eClip = bShiny ? ZM_ANIM_CLIP_IDLE : static_cast<ZM_ANIM_CLIP>(u);
		if (iFrame % 150 == 2) { xAnimator.PlayAnimation(ZM_CreatureClipName(eClip), 0); xAnimator.SetPlaybackSpeed(0); }
		const float afPosePhases[] = {.3f, .25f, .5f, .5f, .25f, 1.f};
		const int iLocal = iFrame % 150;
		// Check the previous rendered packet after the pose/camera has settled.
		// A bind-pose fallback or silently skipped membrane must fail the capture.
		if (bShoalfishReview && (iLocal == 55 || iLocal == 80 || iLocal == 110) &&
			!CheckShoalfishTranslucency(*xEntity.GetComponent<Zenith_ModelComponent>().GetModelInstance()))
			{ FailAG("shoalfish membrane did not reach the animated translucent draw packet"); return false; }
		const float fPhase = eClip == ZM_ANIM_CLIP_FAINT ? (iLocal < 60 ? 1.f/3 : iLocal < 85 ? 2.f/3 : 1.f) : afPosePhases[eClip];
		if (iLocal >= 3 && !xAnimator.GetController().SeekDirectPlay(ZM_CreatureClipDurationSeconds(eClip) * fPhase))
			{ FailAG("creature study clip cannot be sampled"); return false; }
		if (iFrame % 150 == 110) { Flux_Screenshot::RequestDump(CreatureStudyShot(u).c_str()); ++g_uCreatureStudyShots; }
		if (eClip == ZM_ANIM_CLIP_FAINT && (iLocal == 55 || iLocal == 80))
		{
			Flux_Screenshot::RequestDump(CreatureStudyPath(iLocal == 55 ? "_engine_Faint_033.tga" : "_engine_Faint_067.tga").string().c_str());
			++g_uCreatureStudyShots;
		}
		return true;
	}
	bool VerifyCreatureStudy()
	{
		bool bOk = !g_bAGFailed && g_uCreatureStudyShots == ZM_ANIM_CLIP_COUNT + 3;
		for (u_int u = 0; u <= ZM_ANIM_CLIP_COUNT; ++u) bOk = DiskFilePresent(CreatureStudyShot(u)) && bOk;
		bOk = DiskFilePresent(CreatureStudyPath("_engine_Faint_033.tga").string()) && DiskFilePresent(CreatureStudyPath("_engine_Faint_067.tga").string()) && bOk;
		CleanupAGScene(); return bOk;
	}
}
static const Zenith_AutomatedTest g_xCreatureStudyCapture = {"ZM_AuthoredCreatureStudyCapture_Test", &SetupCreatureStudy, &StepCreatureStudy, &VerifyCreatureStudy, 1150, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCreatureStudyCapture);

namespace
{
	constexpr ZM_SPECIES_ID aeStoneSpecies[] = {ZM_SPECIES_RUBBLET,ZM_SPECIES_CAIRNGO,ZM_SPECIES_MONOLODE};
	constexpr ZM_SPECIES_ID aeFungalSpecies[] = {ZM_SPECIES_SPORELING,ZM_SPECIES_DUSKCAP,ZM_SPECIES_FUNGROVE};
	constexpr ZM_SPECIES_ID aeGroveDeerSpecies[] = {ZM_SPECIES_THICKETBUCK,ZM_SPECIES_SYLVASTAG};
	constexpr ZM_SPECIES_ID aeLancefishSpecies[] = {ZM_SPECIES_FINLET,ZM_SPECIES_MARLANCE,ZM_SPECIES_TIDESABRE};
	constexpr ZM_SPECIES_ID aeMineralBeetleSpecies[] = {ZM_SPECIES_PRISMITE,ZM_SPECIES_GEOBORER,ZM_SPECIES_CRYSTALLISK};
	constexpr ZM_SPECIES_ID aeShoalfishSpecies[] = {ZM_SPECIES_MINNET,ZM_SPECIES_SHOALFIN,ZM_SPECIES_TORRENTFIN};
	constexpr ZM_SPECIES_ID aeWindseedSpecies[] = {ZM_SPECIES_PUFFSEED,ZM_SPECIES_DANDELIFT,ZM_SPECIES_ZEPHYRBLOOM};
	constexpr ZM_SPECIES_ID aeEmberBipedSpecies[] = {ZM_SPECIES_KINDLET,ZM_SPECIES_SCORCHEL,ZM_SPECIES_PYROCLAST};
	constexpr ZM_SPECIES_ID aeLarkSpecies[] = {ZM_SPECIES_PIPWIT,ZM_SPECIES_TRILLARK,ZM_SPECIES_STRATAVIS};
	const ZM_SPECIES_ID* g_peStudyFamily = aeStoneSpecies;
	std::string g_strStudyFamily = "StoneLine/";
	u_int g_uFamilySpecies = 0, g_uStudyFamilySize = 3;
	bool g_bFamilyCaptureFailed = false;
	void SetupStoneLineCapture()
	{
		g_peStudyFamily = aeStoneSpecies; g_strStudyFamily = "StoneLine/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
		SetupCreatureStudyForSpecies(aeStoneSpecies[0], "StoneLine/Rubblet");
	}
	void SetupFungalLineCapture()
	{
		g_peStudyFamily = aeFungalSpecies; g_strStudyFamily = "FungalLine/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
		SetupCreatureStudyForSpecies(aeFungalSpecies[0], "FungalLine/Sporeling");
	}
	void SetupGroveDeerCapture()
	{
		g_peStudyFamily = aeGroveDeerSpecies; g_strStudyFamily = "GroveDeer/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 2;
		SetupCreatureStudyForSpecies(aeGroveDeerSpecies[0], "GroveDeer/Thicketbuck");
	}
	void SetupLancefishCapture()
	{
		g_peStudyFamily = aeLancefishSpecies; g_strStudyFamily = "Lancefish/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
		SetupCreatureStudyForSpecies(aeLancefishSpecies[0], "Lancefish/Finlet");
	}
	void SetupMineralBeetleCapture()
	{
		g_peStudyFamily = aeMineralBeetleSpecies; g_strStudyFamily = "MineralBeetles/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
		SetupCreatureStudyForSpecies(aeMineralBeetleSpecies[0], "MineralBeetles/Prismite");
	}
	void SetupScannedBeetleCapture()
	{
		SetupCreatureStudyForSpecies(ZM_SPECIES_PRISMITE, "MineralBeetlesScan/Prismite");
	}
	void SetupShoalfishCapture()
	{
		g_peStudyFamily = aeShoalfishSpecies; g_strStudyFamily = "Shoalfish/";
		g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
		SetupCreatureStudyForSpecies(aeShoalfishSpecies[0], "Shoalfish/Minnet");
	}
	bool StepCreatureFamilyCapture(int iFrame)
	{
		if (g_bFamilyCaptureFailed || g_bAGFailed) return false;
		const u_int uSpecies = static_cast<u_int>(iFrame / 1050);
		if (uSpecies >= g_uStudyFamilySize) return false;
		if (uSpecies != g_uFamilySpecies)
		{
			if (!VerifyCreatureStudy()) { g_bFamilyCaptureFailed = true; return false; }
			g_uFamilySpecies = uSpecies;
			SetupCreatureStudyForSpecies(g_peStudyFamily[uSpecies], g_strStudyFamily + ZM_GetSpeciesName(g_peStudyFamily[uSpecies]));
		}
		return StepCreatureStudy(iFrame % 1050);
	}
	bool VerifyCreatureFamilyCapture()
	{
		const bool bFinal = VerifyCreatureStudy();
		return !g_bFamilyCaptureFailed && g_uFamilySpecies + 1 == g_uStudyFamilySize && bFinal;
	}
}
static void SetupLarkCapture()
{
	g_peStudyFamily = aeLarkSpecies; g_strStudyFamily = "LarkLine/";
	g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
	SetupCreatureStudyForSpecies(aeLarkSpecies[0], "LarkLine/Pipwit");
}
static const Zenith_AutomatedTest g_xLarkCapture = {"ZM_AuthoredLarkCapture_Test", &SetupLarkCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xLarkCapture);

static void SetupEmberBipedCapture()
{
	g_peStudyFamily = aeEmberBipedSpecies; g_strStudyFamily = "EmberBipeds/";
	g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
	SetupCreatureStudyForSpecies(aeEmberBipedSpecies[0], "EmberBipeds/Kindlet");
}
static const Zenith_AutomatedTest g_xEmberBipedCapture = {"ZM_AuthoredEmberBipedCapture_Test", &SetupEmberBipedCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xEmberBipedCapture);

static void SetupWindseedCapture()
{
	g_peStudyFamily = aeWindseedSpecies; g_strStudyFamily = "Windseed/";
	g_uFamilySpecies = 0; g_bFamilyCaptureFailed = false; g_uStudyFamilySize = 3;
	SetupCreatureStudyForSpecies(aeWindseedSpecies[0], "Windseed/Puffseed");
}
static const Zenith_AutomatedTest g_xWindseedCapture = {"ZM_AuthoredWindseedCapture_Test", &SetupWindseedCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xWindseedCapture);

static void SetupZephyrbloomStudyCapture()
{
	SetupCreatureStudyForSpecies(ZM_SPECIES_ZEPHYRBLOOM, "Windseed/Zephyrbloom");
}
static const Zenith_AutomatedTest g_xZephyrbloomStudyCapture = {"ZM_AuthoredZephyrbloomStudyCapture_Test", &SetupZephyrbloomStudyCapture,
	&StepCreatureStudy, &VerifyCreatureStudy, 1150, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZephyrbloomStudyCapture);
static const Zenith_AutomatedTest g_xStoneLineCapture = {"ZM_AuthoredStoneLineCapture_Test", &SetupStoneLineCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xStoneLineCapture);
static const Zenith_AutomatedTest g_xFungalLineCapture = {"ZM_AuthoredFungalLineCapture_Test", &SetupFungalLineCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFungalLineCapture);
static const Zenith_AutomatedTest g_xGroveDeerCapture = {"ZM_AuthoredGroveDeerCapture_Test", &SetupGroveDeerCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 2200, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGroveDeerCapture);
static const Zenith_AutomatedTest g_xLancefishCapture = {"ZM_AuthoredLancefishCapture_Test", &SetupLancefishCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xLancefishCapture);
static const Zenith_AutomatedTest g_xMineralBeetleCapture = {"ZM_AuthoredMineralBeetleCapture_Test", &SetupMineralBeetleCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMineralBeetleCapture);
static const Zenith_AutomatedTest g_xScannedBeetleCapture = {"ZM_AuthoredScannedBeetleCapture_Test", &SetupScannedBeetleCapture,
	&StepCreatureStudy, &VerifyCreatureStudy, 1150, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xScannedBeetleCapture);
static const Zenith_AutomatedTest g_xShoalfishCapture = {"ZM_AuthoredShoalfishCapture_Test", &SetupShoalfishCapture,
	&StepCreatureFamilyCapture, &VerifyCreatureFamilyCapture, 3250, true, true, &CleanupAGScene};
ZENITH_AUTOMATED_TEST_REGISTER(g_xShoalfishCapture);
#endif

#endif // ZENITH_INPUT_SIMULATOR
