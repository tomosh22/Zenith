#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Combat/Components/Combat_GameComponent.h"
#include "Combat/Components/Combat_PlayerComponent.h"
#include "Combat/Components/Combat_EnemyComponent.h"
#include "Combat/Components/Combat_GraphNodes.h"
#include "Combat/Components/Combat_Config.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "EntityComponent/Zenith_GraphOps.h"
#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Physics/Zenith_Physics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/Flux.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

// ============================================================================
// Component registration (meta registry: serialization orders 100-102,
// lifecycle hooks, scene save/load).
//
// File-scope static-init macros, matching the RenderTest precedent.
// The macros enqueue thunks at static init; Zenith_Engine::InitialiseECS's seal
// (EnsureInitialized -> Finalize) drains them into the sorted dispatch/
// serialization list. (Direct RegisterComponent calls from
// Project_RegisterGameComponents also work now - the registry re-finalizes on
// post-seal registration - but the macro form is preferred for consistency.)
// Dead-strip safe: this TU defines the Project_* entry points the engine
// references, so its static initializers always run. Orders 100+ keep game
// components after every engine built-in (Transform=0 ... ParticleEmitter=85,
// AIAgent=90) - in particular the player's and enemies' OnUpdate runs after the
// Animator's component update.
// ============================================================================
ZENITH_REGISTER_COMPONENT(Combat_GameComponent, "CombatGame", 100u)
ZENITH_REGISTER_COMPONENT(Combat_PlayerComponent, "CombatPlayer", 101u)
ZENITH_REGISTER_COMPONENT(Combat_EnemyComponent, "CombatEnemy", 102u)

// ============================================================================
// Combat Resources - Global access for game components
// ============================================================================
namespace Combat
{
	static CombatResources g_xResources;
	CombatResources& Resources() { return g_xResources; }

	const char* g_aszEnemyVariantNames[uENEMY_VARIANT_COUNT] = { "EnemyWeak", "EnemyNormal", "EnemyStrong" };
	const float g_afEnemyVariantScales[uENEMY_VARIANT_COUNT] = { 0.7f, 0.9f, 1.1f };
}

// Lazy initialization of stick figure model asset.
// Called from InitializeCombatResources and again from CreateArena if assets
// were not available at init time (unit tests create them after init).
void Combat::TryInitializeStickFigureModel()
{
	// Already initialized
	if (!Resources().m_strStickFigureModelPath.empty())
		return;

	std::string strStickFigureMeshGeomPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_EXT;
	std::string strStickFigureMeshAssetPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_ASSET_EXT;
	std::string strStickFigureSkeletonPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;

	if (std::filesystem::exists(strStickFigureMeshAssetPath) && std::filesystem::exists(strStickFigureSkeletonPath))
	{
		// Load the mesh geometry through registry
		if (std::filesystem::exists(strStickFigureMeshGeomPath))
		{
			if (Zenith_MeshGeometryAsset* pxGeom = Zenith_AssetRegistry::GetView<Zenith_MeshGeometryAsset>(strStickFigureMeshGeomPath))
			{
				Resources().m_xStickFigureGeometryAsset.Set(pxGeom);
				Resources().m_pxStickFigureGeometry = pxGeom->GetGeometry();
				Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Loaded stick figure mesh from %s", strStickFigureMeshGeomPath.c_str());
			}
		}

		// Use the canonical .zmodel exported by GenerateStickFigureAssets every
		// boot (mesh + skeleton + painted-atlas body material). Only build a
		// fallback bundle when the generator was skipped — re-exporting here
		// unconditionally used to clobber the material binding.
		Resources().m_strStickFigureModelPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MODEL_EXT;
		if (!std::filesystem::exists(Resources().m_strStickFigureModelPath))
		{
			auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
			Zenith_ModelAsset* pxModel = xhModel.GetDirect();
			pxModel->SetName("StickFigure");
			pxModel->SetSkeletonPath(strStickFigureSkeletonPath);

			Zenith_Vector<std::string> xMaterials;
			xMaterials.PushBack("engine:Meshes/StickFigure/StickFigure_Body.zmtrl");
			pxModel->AddMeshByPath(strStickFigureMeshAssetPath, xMaterials);

			pxModel->Export(Resources().m_strStickFigureModelPath.c_str());
			Resources().m_xStickFigureModelAsset.Set(pxModel);
			Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Created model asset at %s", Resources().m_strStickFigureModelPath.c_str());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Using model asset at %s", Resources().m_strStickFigureModelPath.c_str());
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Stick figure assets not found (.zasset=%s, .zskel=%s), using capsule fallback",
			std::filesystem::exists(strStickFigureMeshAssetPath) ? "exists" : "MISSING",
			std::filesystem::exists(strStickFigureSkeletonPath) ? "exists" : "MISSING");

		// Use capsule as fallback geometry only if not already set
		if (!Resources().m_pxStickFigureGeometry)
		{
			Resources().m_xStickFigureGeometryAsset = Resources().m_xCapsuleAsset;
			Resources().m_pxStickFigureGeometry = Resources().m_pxCapsuleGeometry;
		}
	}
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Resource Cleanup (called at shutdown)
// ============================================================================
static void CleanupCombatResources()
{
	using namespace Combat;

	if (!s_bResourcesInitialized)
		return;

	// Delete particle configs
	delete Resources().m_pxHitSparkConfig;
	Resources().m_pxHitSparkConfig = nullptr;
	Resources().m_uHitSparkEmitterID = INVALID_ENTITY_ID;

	delete Resources().m_pxFlameConfig;
	Resources().m_pxFlameConfig = nullptr;

	// Drop prefab handle refs (registry now owns these and deletes them on its own teardown)
	Resources().m_xPlayerPrefab.Clear();
	Resources().m_xEnemyPrefab.Clear();
	for (u_int u = 0; u < uENEMY_VARIANT_COUNT; ++u)
	{
		Resources().m_axEnemyVariants[u].Clear();
	}
	Resources().m_xArenaPrefab.Clear();
	Resources().m_xArenaWallPrefab.Clear();

	// Drop model + mesh-geometry handle refs
	Resources().m_xStickFigureModelAsset.Clear();
	Resources().m_xCapsuleAsset.Clear();
	Resources().m_xCubeAsset.Clear();
	Resources().m_xConeAsset.Clear();
	Resources().m_xStickFigureGeometryAsset.Clear();

	// Clear material handles
	Resources().m_xPlayerMaterial.Clear();
	Resources().m_xEnemyMaterial.Clear();
	Resources().m_xArenaMaterial.Clear();
	Resources().m_xWallMaterial.Clear();
	Resources().m_xCandleMaterial.Clear();

	// Clear convenience geometry pointers (handles already cleared above own the lifetime)
	Resources().m_pxStickFigureGeometry = nullptr;
	Resources().m_pxCapsuleGeometry = nullptr;
	Resources().m_pxCubeGeometry = nullptr;
	Resources().m_pxConeGeometry = nullptr;

	// Note: Textures and materials are managed by Zenith_AssetRegistry

	s_bResourcesInitialized = false;
	Zenith_Log(LOG_CATEGORY_ASSET, "[Combat] Resources cleaned up");
}

// ============================================================================
// Procedural Texture Generation
// ============================================================================

// Export a 1x1 colored texture to disk and return a TextureHandle with its path
static TextureHandle ExportColoredTexture(const std::string& strPath, uint8_t uR, uint8_t uG, uint8_t uB)
{
	// Create texture data
	uint8_t aucPixelData[] = { uR, uG, uB, 255 };

	// Write to .ztxtr file format (same as Zenith_Tools_TextureExport::ExportFromData)
	Zenith_DataStream xStream;
	xStream << (int32_t)1;  // width
	xStream << (int32_t)1;  // height
	xStream << (int32_t)1;  // depth
	xStream << (TextureFormat)TEXTURE_FORMAT_RGBA8_UNORM;
	xStream << (size_t)4;   // data size (1x1x4 bytes)
	xStream.WriteData(aucPixelData, 4);
	xStream.WriteToFile(strPath.c_str());

	// Convert absolute path to prefixed relative path for portability
	std::string strRelativePath = Zenith_AssetRegistry::MakeRelativePath(strPath);
	if (strRelativePath.empty())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "[Combat] Failed to make relative path for texture: %s", strPath.c_str());
		return TextureHandle();
	}

	// Create TextureHandle with the prefixed path
	return TextureHandle(strRelativePath);
}

// ============================================================================
// Resource Initialization
// ============================================================================
static void InitializeCombatResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Combat;

	// Create directory for procedural meshes
	std::string strMeshDir = std::string(GAME_ASSETS_DIR) + "/Meshes";
	std::filesystem::create_directories(strMeshDir);

	// Create capsule geometry (for characters) - custom size, tracked through registry
	{
		auto xhCapsuleAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Zenith_MeshGeometryAsset* pxCapsuleAsset = xhCapsuleAsset.GetDirect();
		Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
		Flux_MeshGeometry::GenerateCapsule(*pxCapsule, 0.5f, 1.0f, 16, 16);
		pxCapsuleAsset->SetGeometry(pxCapsule);
		Resources().m_xCapsuleAsset.Set(pxCapsuleAsset);
		Resources().m_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule" ZENITH_MESH_EXT;
	Resources().m_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	Resources().m_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create cube geometry (for arena) - use registry's cached unit cube
	Resources().m_xCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube" ZENITH_MESH_EXT;
	Resources().m_pxCubeGeometry->Export(strCubePath.c_str());
	Resources().m_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	// Create cone geometry (for candles on walls) - custom size, tracked through registry
	{
		auto xhConeAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Zenith_MeshGeometryAsset* pxConeAsset = xhConeAsset.GetDirect();
		Flux_MeshGeometry* pxCone = new Flux_MeshGeometry();
		Flux_MeshGeometry::GenerateCone(*pxCone, 0.08f, 0.25f, 12);
		pxConeAsset->SetGeometry(pxCone);
		Resources().m_xConeAsset.Set(pxConeAsset);
		Resources().m_pxConeGeometry = pxConeAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strConePath = strMeshDir + "/Cone" ZENITH_MESH_EXT;
	Resources().m_pxConeGeometry->Export(strConePath.c_str());
	Resources().m_pxConeGeometry->m_strSourcePath = strConePath;
#endif

	// Try to load stick figure assets (may not exist yet on first run - unit tests create them)
	Combat::TryInitializeStickFigureModel();

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureHandles
	// SSR VERIFICATION: Using bright distinctive colors for walls and player
	TextureHandle xPlayerTextureHandle = ExportColoredTexture(strTexturesDir + "/Player" ZENITH_TEXTURE_EXT, 0, 255, 255);      // CYAN player for SSR detection
	TextureHandle xEnemyTextureHandle = ExportColoredTexture(strTexturesDir + "/Enemy" ZENITH_TEXTURE_EXT, 204, 51, 51);        // Red enemies
	TextureHandle xArenaTextureHandle = ExportColoredTexture(strTexturesDir + "/Arena" ZENITH_TEXTURE_EXT, 77, 77, 89);         // Gray arena floor
	TextureHandle xWallTextureHandle = ExportColoredTexture(strTexturesDir + "/Wall" ZENITH_TEXTURE_EXT, 255, 0, 255);          // MAGENTA walls for SSR detection
	TextureHandle xCandleTextureHandle = ExportColoredTexture(strTexturesDir + "/Candle" ZENITH_TEXTURE_EXT, 240, 220, 180);    // Cream candle color

	// Create materials with texture paths (properly serializable)

	Resources().m_xPlayerMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xPlayerMaterial.GetDirect()->SetName("CombatPlayer");
	Resources().m_xPlayerMaterial.GetDirect()->SetDiffuseTexture(xPlayerTextureHandle);
	Resources().m_xPlayerMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - player should be REFLECTED, not reflecting

	Resources().m_xEnemyMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xEnemyMaterial.GetDirect()->SetName("CombatEnemy");
	Resources().m_xEnemyMaterial.GetDirect()->SetDiffuseTexture(xEnemyTextureHandle);
	Resources().m_xEnemyMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - enemies should be REFLECTED, not reflecting

	Resources().m_xArenaMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xArenaMaterial.GetDirect()->SetName("CombatArena");
	Resources().m_xArenaMaterial.GetDirect()->SetDiffuseTexture(xArenaTextureHandle);
	Resources().m_xArenaMaterial.GetDirect()->SetRoughness(0.15f);  // LOW roughness - floor IS the reflective surface

	Resources().m_xWallMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xWallMaterial.GetDirect()->SetName("CombatWall");
	Resources().m_xWallMaterial.GetDirect()->SetDiffuseTexture(xWallTextureHandle);
	Resources().m_xWallMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - walls should be REFLECTED, not reflecting

	Resources().m_xCandleMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xCandleMaterial.GetDirect()->SetName("CombatCandle");
	Resources().m_xCandleMaterial.GetDirect()->SetDiffuseTexture(xCandleTextureHandle);
	Resources().m_xCandleMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - candles should be REFLECTED, not reflecting

	// Create flame particle config for wall candles
	Resources().m_pxFlameConfig = new Flux_ParticleEmitterConfig();
	Resources().m_pxFlameConfig->m_fSpawnRate = 30.0f;                    // Dense flame
	Resources().m_pxFlameConfig->m_uBurstCount = 0;
	Resources().m_pxFlameConfig->m_uMaxParticles = 64;                    // More particles per candle
	Resources().m_pxFlameConfig->m_fSpawnRadius = 0.05f;                  // Slight position variation
	Resources().m_pxFlameConfig->m_fLifetimeMin = 0.4f;
	Resources().m_pxFlameConfig->m_fLifetimeMax = 0.9f;
	Resources().m_pxFlameConfig->m_fSpeedMin = 0.3f;
	Resources().m_pxFlameConfig->m_fSpeedMax = 1.0f;
	Resources().m_pxFlameConfig->m_fSpreadAngleDegrees = 25.0f;           // Wider spread
	Resources().m_pxFlameConfig->m_xEmitDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	Resources().m_pxFlameConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f);  // Strong updraft
	Resources().m_pxFlameConfig->m_fDrag = 0.8f;
	Resources().m_pxFlameConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, 0.9f);  // Bright yellow
	Resources().m_pxFlameConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 0.2f, 0.0f, 0.0f);    // Red->transparent
	Resources().m_pxFlameConfig->m_fSizeStart = 0.15f;
	Resources().m_pxFlameConfig->m_fSizeEnd = 0.04f;
	Resources().m_pxFlameConfig->m_bAdditiveBlending = true;              // Glow effect
	Resources().m_pxFlameConfig->m_fTurbulence = 1.5f;                    // Flickering motion
	Resources().m_pxFlameConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_Flame", Resources().m_pxFlameConfig);

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates - components added after transform is set.
	// Use the persistent scene here rather than GetActiveScene(): InitializeCombatResources
	// runs before the initial scene is loaded, and (post-A6) GetActiveScene returns INVALID
	// until that happens. The persistent scene is always available and these template
	// entities are destroyed before any gameplay begins.
	Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xPersistentScene);

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "PlayerTemplate");
		auto xhPlayer = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxPlayer = xhPlayer.GetDirect();
		pxPlayer->CreateFromEntity(xPlayerTemplate, "Player");
		Resources().m_xPlayerPrefab.Set(pxPlayer);
		xPlayerTemplate.Destroy();
	}

	// Enemy prefab + three Scale variants demonstrating the variant override system.
	// The base prefab is saved to disk and re-loaded through the asset registry so
	// the variants get a proper path-based PrefabHandle that resolves on
	// Instantiate. Each variant overrides Transform.Scale to a different value
	// (0.7 / 0.9 / 1.1) so the three enemy tiers visibly differ in size.
	{
		Zenith_Entity xEnemyTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "EnemyTemplate");
		auto xhEnemy = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxEnemy = xhEnemy.GetDirect();
		pxEnemy->CreateFromEntity(xEnemyTemplate, "Enemy");
		Resources().m_xEnemyPrefab.Set(pxEnemy);
		xEnemyTemplate.Destroy();

		// Persist the base to disk so PrefabHandle("EnemyBase.zpfb") resolves
		// through the registry. Cheap relative-path write; the file is owned by
		// the launch and effectively transient.
		const std::string strBasePath = "EnemyBase.zpfb";
		pxEnemy->SaveToFile(strBasePath);
		Zenith_AssetRegistry::GetView<Zenith_Prefab>(strBasePath);

		// Build the three Scale variants in memory.
		PrefabHandle xBaseHandle(strBasePath);
		for (u_int u = 0; u < uENEMY_VARIANT_COUNT; ++u)
		{
			auto xhVariant = Zenith_AssetRegistry::Create<Zenith_Prefab>();
			Zenith_Prefab* pxVariant = xhVariant.GetDirect();
			pxVariant->CreateAsVariant(xBaseHandle, g_aszEnemyVariantNames[u]);

			Zenith_PropertyOverride xOv;
			xOv.m_strComponentName = "Transform";
			xOv.m_strPropertyPath  = "Scale";
			const float f = g_afEnemyVariantScales[u];
			xOv.m_xValue << Zenith_Maths::Vector3(f, f, f);
			pxVariant->AddOverride(std::move(xOv));

			Resources().m_axEnemyVariants[u].Set(pxVariant);
		}
	}

	// Arena prefab (for floor)
	{
		Zenith_Entity xArenaTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ArenaTemplate");
		auto xhArena = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxArena = xhArena.GetDirect();
		pxArena->CreateFromEntity(xArenaTemplate, "Arena");
		Resources().m_xArenaPrefab.Set(pxArena);
		xArenaTemplate.Destroy();
	}

	// ArenaWall prefab with collider and particle emitter
	// NOTE: ModelComponent is NOT included because mesh/material pointers don't serialize.
	// ModelComponent is added after instantiation in CreateArena().
	{
		Zenith_Entity xWallTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ArenaWallTemplate");

		// Add ColliderComponent for wall collision
		xWallTemplate.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		// Add ParticleEmitterComponent for candle flame
		Zenith_ParticleEmitterComponent& xEmitter = xWallTemplate.AddComponent<Zenith_ParticleEmitterComponent>();
		xEmitter.SetConfig(Resources().m_pxFlameConfig);
		xEmitter.SetEmitting(true);

		auto xhWall = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxWall = xhWall.GetDirect();
		pxWall->CreateFromEntity(xWallTemplate, "ArenaWall");
		Resources().m_xArenaWallPrefab.Set(pxWall);
		xWallTemplate.Destroy();
	}

	// Create hit spark particle config
	Resources().m_pxHitSparkConfig = new Flux_ParticleEmitterConfig();
	Resources().m_pxHitSparkConfig->m_uBurstCount = 20;
	Resources().m_pxHitSparkConfig->m_fSpawnRate = 0.0f;
	Resources().m_pxHitSparkConfig->m_uMaxParticles = 256;
	Resources().m_pxHitSparkConfig->m_fLifetimeMin = 0.2f;
	Resources().m_pxHitSparkConfig->m_fLifetimeMax = 0.4f;
	Resources().m_pxHitSparkConfig->m_fSpeedMin = 8.0f;
	Resources().m_pxHitSparkConfig->m_fSpeedMax = 15.0f;
	Resources().m_pxHitSparkConfig->m_fSpreadAngleDegrees = 60.0f;
	Resources().m_pxHitSparkConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, -5.0f, 0.0f);
	Resources().m_pxHitSparkConfig->m_fDrag = 2.0f;
	Resources().m_pxHitSparkConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.6f, 0.1f, 1.0f);
	Resources().m_pxHitSparkConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 1.0f, 0.2f, 0.0f);
	Resources().m_pxHitSparkConfig->m_fSizeStart = 0.3f;
	Resources().m_pxHitSparkConfig->m_fSizeEnd = 0.1f;
	Resources().m_pxHitSparkConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_HitSpark", Resources().m_pxHitSparkConfig);

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Combat";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Meta-registry registration happens via the ZENITH_REGISTER_COMPONENT macros
	// at the top of this file (see the note there - the registry is sealed before
	// this hook runs). This function remains as the per-game lifecycle hook for
	// early CPU-only resource initialization that must happen before any scene
	// load (TOOLS or non-TOOLS builds). Tools builds additionally mirror the
	// components into the editor "Add Component" registry here (display names
	// used by AddStep_AddComponent and the editor menu; this registry is
	// append-anytime, not sealed).
	InitializeCombatResources();
	Combat_RegisterGraphNodes();

#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<Combat_GameComponent>("CombatGame");
	xEditorRegistry.RegisterComponent<Combat_PlayerComponent>("CombatPlayer");
	xEditorRegistry.RegisterComponent<Combat_EnemyComponent>("CombatEnemy");
#endif
}

void Project_Shutdown()
{
	CleanupCombatResources();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All combat resources initialized in Project_RegisterGameComponents via InitializeCombatResources
}

// ============================================================================
// Behaviour Graph builders (boot-authored via AddStep_GraphBuild).
// ============================================================================

// Combat_PlayerAttack.bgraph - decomposed CombatAttackFlow. The old mega-node
// ran three independent guards top-to-bottom each AttackTick; each becomes a
// chain under its own "AttackTick" OnCustomEvent source. The sources fire in
// node order, so guard order is preserved; the first chain runs
// CombatQueryAttackState to populate the blackboard before the later guards
// read it.
static void BuildGraph_CombatPlayerAttack(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Guard 1: query attack state, then activate the hitbox on attack start.
	Zenith_GraphChain xEvt1 = xB.OnCustomEvent("AttackTick");
	const u_int uQuery = xB.Node("CombatQueryAttackState");
	const u_int uBranchStart = xB.Branch("attackJustStarted");
	const u_int uActivate = xB.Node("CombatActivateHitbox");
	xEvt1.Then(uQuery).Then(uBranchStart).ThenPin(0, uActivate);

	// Guard 2: on the attack hit-frame, register hits; push combo on a landed hit.
	Zenith_GraphChain xEvt2 = xB.OnCustomEvent("AttackTick");
	const u_int uBranchHit = xB.Branch("hitFrameReady");
	const u_int uRegister = xB.Node("CombatRegisterHits");
	const u_int uCmpHits = xB.CompareInt("uHits", GRAPH_COMPARE_INT_OP_GREATER, 0, "uHitsPositive");
	const u_int uBranchHits = xB.Branch("uHitsPositive");
	const u_int uNotify = xB.Node("CombatNotifyComboHit");
	xEvt2.Then(uBranchHit).ThenPin(0, uRegister).Then(uCmpHits).Then(uBranchHits).ThenPin(0, uNotify);

	// Guard 3: deactivate the hitbox when no longer attacking (false pin).
	Zenith_GraphChain xEvt3 = xB.OnCustomEvent("AttackTick");
	const u_int uBranchAtk = xB.Branch("isAttacking");
	const u_int uDeactivate = xB.Node("CombatDeactivateHitbox");
	xEvt3.Then(uBranchAtk).ThenPin(1, uDeactivate);
}

// Combat_RoundFlow.bgraph - combo-timer tick (kept as one node; dt rides the
// payload) then the decomposed win/lose decision. Two "RoundTick" chains run in
// node order: chain 1 = tick + VICTORY check, chain 2 = GAME_OVER check,
// preserving the old VICTORY-before-GAME_OVER, both-independent evaluation.
static void BuildGraph_CombatRoundFlow(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Chain 1: combo-timer tick, count living enemies, VICTORY if all dead and
	// at least one enemy was registered.
	const u_int uEvt1 = xB.OnCustomEvent("RoundTick");
	const u_int uTick = xB.Node("CombatTickComboTimer");
	xB.ParamString(uTick, "m_strDtVar", "payload");
	const u_int uCount = xB.Node("CombatCountAliveEnemies");
	const u_int uCmpZero = xB.CompareInt("aliveCount", GRAPH_COMPARE_INT_OP_EQUAL, 0, "aliveIsZero");
	const u_int uBranchZero = xB.Branch("aliveIsZero");
	const u_int uBranchHas = xB.Branch("hasEnemies");
	const u_int uWin = xB.Node("CombatSetGameState");
	xB.ParamEnum(uWin, "m_iState", Combat_GameState::VICTORY);
	xB.Chain(uEvt1, uTick).Chain(uTick, uCount).Chain(uCount, uCmpZero).Chain(uCmpZero, uBranchZero);
	xB.Edge(uBranchZero, 0, uBranchHas);
	xB.Edge(uBranchHas, 0, uWin);

	// Chain 2: GAME_OVER if the player is dead (runs after chain 1 in node order).
	const u_int uEvt2 = xB.OnCustomEvent("RoundTick");
	const u_int uCheckDead = xB.Node("CombatCheckPlayerDead");
	const u_int uBranchDead = xB.Branch("playerDead");
	const u_int uLose = xB.Node("CombatSetGameState");
	xB.ParamEnum(uLose, "m_iState", Combat_GameState::GAME_OVER);
	xB.Chain(uEvt2, uCheckDead).Chain(uCheckDead, uBranchDead);
	xB.Edge(uBranchDead, 0, uLose);
}

// Combat_PlayerState.bgraph - attached alongside Combat_PlayerAttack on the
// player. The deleted Combat_PlayerController::Update decision switch becomes a
// StateMachine(playerState). Two "PlayerTick" chains run in node order: chain 1
// = PreTick then per-state dispatch; chain 2 = PostTick. Per-pin node instances
// avoid exec fan-in (IDLE/WALKING share the movement handler; the four attack
// states share the attack handler).
static void BuildGraph_CombatPlayerState(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xVal;
	xVal.SetInt32(static_cast<int32_t>(Combat_PlayerState::IDLE));
	xBuilder.Variable("playerState", xVal);
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Chain 1: pre-tick + StateMachine dispatch.
	Zenith_GraphChain xTick = xB.OnCustomEvent("PlayerTick");
	const u_int uPre = xB.Node("CombatPlayerPreTick");
	const u_int uSM = xB.StateMachine("playerState", 9, "Idle,Walking,LightAttack1,LightAttack2,LightAttack3,HeavyAttack,Dodging,HitStun,Dead");
	xTick.Then(uPre).Then(uSM);

	const u_int uIdle = xBuilder.Node("CombatPlayerMovementTick");
	const u_int uWalk = xBuilder.Node("CombatPlayerMovementTick");
	const u_int uLA1 = xBuilder.Node("CombatPlayerAttackTick");
	const u_int uLA2 = xBuilder.Node("CombatPlayerAttackTick");
	const u_int uLA3 = xBuilder.Node("CombatPlayerAttackTick");
	const u_int uHeavy = xBuilder.Node("CombatPlayerAttackTick");
	const u_int uDodge = xBuilder.Node("CombatPlayerDodgeTick");
	const u_int uHitStun = xBuilder.Node("CombatPlayerHitStunTick");
	xBuilder.Edge(uSM, 0, uIdle);    // IDLE
	xBuilder.Edge(uSM, 1, uWalk);    // WALKING
	xBuilder.Edge(uSM, 2, uLA1);     // LIGHT_ATTACK_1
	xBuilder.Edge(uSM, 3, uLA2);     // LIGHT_ATTACK_2
	xBuilder.Edge(uSM, 4, uLA3);     // LIGHT_ATTACK_3
	xBuilder.Edge(uSM, 5, uHeavy);   // HEAVY_ATTACK
	xBuilder.Edge(uSM, 6, uDodge);   // DODGING
	xBuilder.Edge(uSM, 7, uHitStun); // HIT_STUN
	// pin 8 (DEAD) intentionally unwired - no-op, matching the old switch.

	// Chain 2: post-tick (recompute state-changed) after the handler ran.
	Zenith_GraphChain xTick2 = xB.OnCustomEvent("PlayerTick");
	const u_int uPost = xB.Node("CombatPlayerPostTick");
	xTick2.Then(uPost);
}

// Combat_EnemyBrain.bgraph - runtime-attached per enemy. The deleted
// Combat_EnemyAI::Update decision switch becomes a StateMachine(enemyState).
// Two "EnemyBrainTick" chains run in node order: chain 1 = PreTick then the
// per-state handler dispatch; chain 2 = PostTick (anim + IK).
static void BuildGraph_CombatEnemyBrain(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xVal;
	xVal.SetInt32(static_cast<int32_t>(Combat_EnemyState::IDLE));
	xBuilder.Variable("enemyState", xVal);
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Chain 1: pre-tick + StateMachine dispatch.
	Zenith_GraphChain xTick = xB.OnCustomEvent("EnemyBrainTick");
	const u_int uPre = xB.Node("CombatEnemyPreTick");
	const u_int uSM = xB.StateMachine("enemyState", 5, "Idle,Chasing,Attacking,HitStun,Dead");
	// No m_strEventPrefix: the old switch fired no enter/exit events.
	const u_int uIdle = xB.Node("CombatEnemyIdleTick");
	const u_int uChase = xB.Node("CombatEnemyChaseTick");
	const u_int uAttack = xB.Node("CombatEnemyAttackTick");
	const u_int uHitStun = xB.Node("CombatEnemyHitStunTick");
	xTick.Then(uPre).Then(uSM);
	xB.Edge(uSM, 0, uIdle);    // IDLE
	xB.Edge(uSM, 1, uChase);   // CHASING
	xB.Edge(uSM, 2, uAttack);  // ATTACKING
	xB.Edge(uSM, 3, uHitStun); // HIT_STUN
	// pin 4 (DEAD) intentionally unwired - no-op, matching the old switch.

	// Chain 2: post-tick (anim + IK) after the handler ran.
	Zenith_GraphChain xTick2 = xB.OnCustomEvent("EnemyBrainTick");
	const u_int uPost = xB.Node("CombatEnemyPostTick");
	xTick2.Then(uPost);
}

// ---- Combat_GameFlow, split (Phase 3) into per-input-source sub-builders.
// Each source (menu Play/focus, P, R, Escape) authors an independent chain
// sharing only the gameState variable; called in original order they preserve
// node-creation + edge-add order -> byte-identical authoring.

static void BuildCombatGameFlow_MenuInput(Zenith_EngineGraphBuilder& xB)
{
	// Menu Play button -> load the arena (SINGLE mode = old OnPlayClicked).
	const u_int uPlay = xB.Node("OnUIButtonClicked");
	xB.ParamString(uPlay, "m_strButton", "MenuPlay");
	const u_int uLoad = xB.Node("LoadSceneByIndex");
	xB.ParamInt(uLoad, "m_iSceneIndex", 1);
	xB.Chain(uPlay, uLoad);

	// Menu focus (single button; the node no-ops in the arena).
	const u_int uFocusTick = xB.OnUpdate();
	const u_int uFocus = xB.Node("CombatFocusPlayButton");
	xB.Chain(uFocusTick, uFocus);
}

static void BuildCombatGameFlow_PauseResume(Zenith_EngineGraphBuilder& xB)
{
	// P: pause (from PLAYING) / resume (from PAUSED).
	const u_int uOnP = xB.OnKeyPressed(ZENITH_KEY_P);
	const u_int uGetP = xB.Node("CombatGetGameState");
	const u_int uSwP = xB.SwitchOnInt("gameState", 5);
	xB.Chain(uOnP, uGetP).Chain(uGetP, uSwP);
	const u_int uPause = xB.Node("CombatSetScenePaused");
	xB.ParamBool(uPause, "m_bPaused", true);
	const u_int uToPaused = xB.Node("CombatSetGameState");
	xB.ParamEnum(uToPaused, "m_iState", Combat_GameState::PAUSED);
	xB.Edge(uSwP, static_cast<u_int>(Combat_GameState::PLAYING), uPause);
	xB.Chain(uPause, uToPaused);
	const u_int uUnpause = xB.Node("CombatSetScenePaused");
	xB.ParamBool(uUnpause, "m_bPaused", false);
	const u_int uToPlaying = xB.Node("CombatSetGameState");
	xB.ParamEnum(uToPlaying, "m_iState", Combat_GameState::PLAYING);
	xB.Edge(uSwP, static_cast<u_int>(Combat_GameState::PAUSED), uUnpause);
	xB.Chain(uUnpause, uToPlaying);
}

static void BuildCombatGameFlow_Restart(Zenith_EngineGraphBuilder& xB)
{
	// R: restart (from PLAYING / VICTORY / GAME_OVER). Per-pin node instances.
	const u_int uOnR = xB.OnKeyPressed(ZENITH_KEY_R);
	const u_int uGetR = xB.Node("CombatGetGameState");
	const u_int uSwR = xB.SwitchOnInt("gameState", 5);
	xB.Chain(uOnR, uGetR).Chain(uGetR, uSwR);
	const u_int uResetP = xB.Node("CombatResetGame");
	xB.Edge(uSwR, static_cast<u_int>(Combat_GameState::PLAYING), uResetP);
	const u_int uResetV = xB.Node("CombatResetGame");
	xB.Edge(uSwR, static_cast<u_int>(Combat_GameState::VICTORY), uResetV);
	const u_int uResetG = xB.Node("CombatResetGame");
	xB.Edge(uSwR, static_cast<u_int>(Combat_GameState::GAME_OVER), uResetG);
}

static void BuildCombatGameFlow_ReturnToMenu(Zenith_EngineGraphBuilder& xB)
{
	// Escape: return to menu (from PLAYING / PAUSED / VICTORY / GAME_OVER).
	const u_int uOnEsc = xB.OnKeyPressed(ZENITH_KEY_ESCAPE);
	const u_int uGetE = xB.Node("CombatGetGameState");
	const u_int uSwE = xB.SwitchOnInt("gameState", 5);
	xB.Chain(uOnEsc, uGetE).Chain(uGetE, uSwE);
	const u_int uMenuPl = xB.Node("CombatReturnToMenu");
	xB.Edge(uSwE, static_cast<u_int>(Combat_GameState::PLAYING), uMenuPl);
	const u_int uMenuPa = xB.Node("CombatReturnToMenu");
	xB.Edge(uSwE, static_cast<u_int>(Combat_GameState::PAUSED), uMenuPa);
	const u_int uMenuV = xB.Node("CombatReturnToMenu");
	xB.Edge(uSwE, static_cast<u_int>(Combat_GameState::VICTORY), uMenuV);
	const u_int uMenuG = xB.Node("CombatReturnToMenu");
	xB.Edge(uSwE, static_cast<u_int>(Combat_GameState::GAME_OVER), uMenuG);
}

// Combat_GameFlow.bgraph - attached to both GameManagers. Handles the
// menu/pause/reset input DECISIONS at order 60 (before CombatGame's systems at
// order 100). s_eGameState stays the static source of truth; the P/R/Escape
// chains read it via CombatGetGameState and dispatch by state with SwitchOnInt.
// ACCEPTED divergence (humanly unreachable): P/R/Escape
// are independent OnKeyPressed sources, so pressing two distinct keys on the
// exact SAME frame lets both fire (the second re-reads the state the first set),
// where the old if/else-if/return switch was mutually exclusive per frame.
static void BuildGraph_CombatGameFlow(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xVal;
	xVal.SetInt32(static_cast<int32_t>(Combat_GameState::MAIN_MENU));
	xBuilder.Variable("gameState", xVal);
	Zenith_EngineGraphBuilder xB(xBuilder);

	BuildCombatGameFlow_MenuInput(xB);
	BuildCombatGameFlow_PauseResume(xB);
	BuildCombatGameFlow_Restart(xB);
	BuildCombatGameFlow_ReturnToMenu(xB);
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- Behaviour graphs (regenerated every boot, like the scenes) --------
	// Wave-2 conversions: the attack flow + round flow DECISIONS live here;
	// the C++ components fire the driving custom events ("AttackTick" /
	// "RoundTick") at exactly the points the old bodies ran, and the nodes
	// execute systems back through the components.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// Player attack flow: hitbox lifecycle + hit registration + combo push,
	// decomposed into single-action nodes (see BuildGraph_CombatPlayerAttack).
	xAuto.AddStep_GraphBuild("game:Graphs/Combat_PlayerAttack.bgraph", &BuildGraph_CombatPlayerAttack);

	// Round flow: combo-timer tick then the decomposed win/lose decision
	// (see BuildGraph_CombatRoundFlow).
	xAuto.AddStep_GraphBuild("game:Graphs/Combat_RoundFlow.bgraph", &BuildGraph_CombatRoundFlow);

	// Player state machine: attached alongside the attack graph on the player.
	xAuto.AddStep_GraphBuild("game:Graphs/Combat_PlayerState.bgraph", &BuildGraph_CombatPlayerState);

	// Enemy brain: per-enemy StateMachine (runtime-attached in SpawnEnemies).
	xAuto.AddStep_GraphBuild("game:Graphs/Combat_EnemyBrain.bgraph", &BuildGraph_CombatEnemyBrain);

	// Game flow: menu/pause/reset input (attached to both GameManagers below).
	xAuto.AddStep_GraphBuild("game:Graphs/Combat_GameFlow.bgraph", &BuildGraph_CombatGameFlow);

	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.0f, 12.0f, -15.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.0f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "COMBAT ARENA");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.0f, -120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 72.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 1.0f, 0.2f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.0f, 50.0f);
	g_xEngine.EditorAutomation().AddStep_AddComponent("CombatGame");
	// Game-flow graph on the MainMenu GameManager: OnUIButtonClicked("MenuPlay")
	// -> LoadSceneByIndex(1), plus the menu-focus tick.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Combat_GameFlow.bgraph");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Arena gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Arena");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.0f, 12.0f, -15.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.0f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// PlayerHealth: TopLeft, x=30, y=30+24*3=102, size=15*3=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PlayerHealth", "Health: 100 / 100");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PlayerHealth", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PlayerHealth", 30.0f, 102.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PlayerHealth", 45.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PlayerHealth", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PlayerHealth", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PlayerHealth", false);

	// PlayerHealthBar: TopLeft, x=30, y=30+24*4=126, size=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PlayerHealthBar", "[||||||||||||||||||||]");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PlayerHealthBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PlayerHealthBar", 30.0f, 126.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PlayerHealthBar", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PlayerHealthBar", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PlayerHealthBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PlayerHealthBar", false);

	// EnemyCount: TopLeft, x=30, y=30+24*6=174, size=15*3=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("EnemyCount", "Enemies: 3 / 3");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("EnemyCount", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("EnemyCount", 30.0f, 174.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("EnemyCount", 45.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("EnemyCount", 0.8f, 0.8f, 0.8f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("EnemyCount", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("EnemyCount", false);

	// ComboCount: Center, x=0, y=-100, size=15*8=120
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ComboCount", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ComboCount", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ComboCount", 0.0f, -100.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ComboCount", 120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ComboCount", 1.0f, 0.8f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ComboCount", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ComboCount", false);

	// ComboText: Center, x=0, y=-60, size=15*4=60
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ComboText", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ComboText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ComboText", 0.0f, -60.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ComboText", 60.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ComboText", 1.0f, 0.8f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ComboText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ComboText", false);

	// Controls: BottomLeft, x=30, y=30, size=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Controls", "WASD: Move | LMB: Attack | RMB: Heavy | Space: Dodge | R: Reset | Esc: Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Controls", 30.0f, 30.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Controls", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Controls", false);

	// Status: Center, x=0, y=0, size=15*8=120
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

	g_xEngine.EditorAutomation().AddStep_AddComponent("CombatGame");
	// Round-flow graph on the Arena GameManager: CombatGame fires "RoundTick"
	// into it from the PLAYING branch of its OnUpdate.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Combat_RoundFlow.bgraph");
	// Game-flow graph on the Arena GameManager too: the P/R/Escape input chains
	// read s_eGameState and pause/restart/return-to-menu.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Combat_GameFlow.bgraph");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Arena" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Arena" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
