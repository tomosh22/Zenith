#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPFogPass.h"
#include "Source/DPMaterials.h"
#include "Source/DPUI.h"
#include "Source/DP_Tuning.h"
#include "Source/DP_Archetypes.h"
#include "Source/DP_Reagents.h"
#include "Source/DPParticles.h"
#include "Source/DPTutorial.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "EntityComponent/Zenith_GraphOps.h"
#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "Source/DPResources.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Zenith_GraphicsOptions.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "SaveData/Zenith_SaveData.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "UI/Zenith_UIRect.h"

// Game component headers — included here so the file-scope
// ZENITH_REGISTER_COMPONENT macros below can name the complete types.
// This TU defines the Project_* entry points the engine references, so its
// static initializers always run (dead-strip safe).
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPPlayerController_Component.h"
#include "Components/DPOrbitCamera_Component.h"
#include "Components/DP_GraphNodes.h"
#include "Components/DPGraphInteractable_Component.h"
#include "Components/DPMenuRelay_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPItemSpawn_Component.h"
#include "Components/DPItemManager_Component.h"
#include "Components/DPInteractable_Base.h"
#include "Components/DPDoor_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/Priest_Component.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "Components/DPHUDController_Component.h"
#include "Components/DPPauseMenuController_Component.h"
#include "Components/DPFogPass_Component.h"
#include "Components/DPLiminalHub_Component.h"
#include "Components/DPMinimap_Component.h"
#include "Components/DPProcLevelBootstrap_Component.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

// ============================================================================
// Component registration (meta registry: serialization + lifecycle dispatch
// order, scene save/load). The macro enqueues a thunk at static init that the
// registry's seal drains, so the components land in the sorted list (the
// engine re-finalizes on post-seal registration too, so ordering is safe
// either way). Orders 100-117, unique per game, above the engine built-ins
// (Transform=0 .. AIAgent=90).
//
// 100-104 mirror the GameManager attach order of the old script slots
// (player, item manager, fog pass, HUD, orbit camera) so per-entity hook
// dispatch order is preserved. DPInteractable_Base is deliberately NOT
// registered — it is a plain C++ base class; only the concrete leaves are
// components.
// ============================================================================
ZENITH_REGISTER_COMPONENT(DPPlayerController_Component,  "DPPlayerController",  100u)
ZENITH_REGISTER_COMPONENT(DPItemManager_Component,       "DPItemManager",       101u)
ZENITH_REGISTER_COMPONENT(DPFogPass_Component,           "DPFogPass",           102u)
ZENITH_REGISTER_COMPONENT(DPHUDController_Component,     "DPHUDController",     103u)
ZENITH_REGISTER_COMPONENT(DPOrbitCamera_Component,       "DPOrbitCamera",       104u)
ZENITH_REGISTER_COMPONENT(DPMinimap_Component,           "DPMinimap",           105u)
ZENITH_REGISTER_COMPONENT(DPPauseMenuController_Component, "DPPauseMenuController", 106u)
ZENITH_REGISTER_COMPONENT(DPProcLevelBootstrap_Component, "DPProcLevelBootstrap", 107u)
ZENITH_REGISTER_COMPONENT(DPVillager_Component,          "DPVillager",          108u)
ZENITH_REGISTER_COMPONENT(Priest_Component,              "Priest",              109u)
ZENITH_REGISTER_COMPONENT(DPItemBase_Component,          "DPItemBase",          110u)
ZENITH_REGISTER_COMPONENT(DPItemSpawn_Component,         "DPItemSpawn",         111u)
ZENITH_REGISTER_COMPONENT(DPDoor_Component,              "DPDoor",              112u)
ZENITH_REGISTER_COMPONENT(DPLiminalHub_Component,        "DPLiminalHub",        113u)
ZENITH_REGISTER_COMPONENT(DPForge_Component,             "DPForge",             115u)
ZENITH_REGISTER_COMPONENT(DPGraphInteractable_Component, "DPGraphInteractable", 118u)
ZENITH_REGISTER_COMPONENT(DPMenuRelay_Component,         "DPMenuRelay",         119u)

// ============================================================================
// Forward decls — Project_LoadInitialScene is referenced by the editor
// automation chain ahead of its definition (matches Combat).
// ============================================================================
void Project_LoadInitialScene();

// ============================================================================
// Resource initialization (CPU-only state that must exist before any scene
// loads — material handles, prefab registry, etc). Wave 2 streams (B1 assets,
// B6 fog) extend this; W0 is a no-op stub.
// ============================================================================
namespace DevilsPlayground
{
	// ------------------------------------------------------------------------
	// Prefab template resources (see Source/DPResources.h). The single process-
	// wide instance + the Resources() accessor the header declares.
	// ------------------------------------------------------------------------
	static DPResources g_xResources;
	DPResources& Resources() { return g_xResources; }

	namespace
	{
		bool s_bPrefabsInit = false;

		// Build one prefab template in the PERSISTENT scene (the active scene is
		// INVALID before the first scene loads): Transform + Model(cube) + an
		// optional collider, captured into a prefab. The template entity is
		// destroyed immediately afterwards -- the prefab keeps the serialized
		// data, and a baked collider gives the template a live Jolt body that
		// must be torn down. Matches the Combat template pattern.
		void CreateDPTemplate(PrefabHandle& xOut, const char* szName,
			bool bWithCollider, CollisionVolumeType eVolume, RigidBodyType eBody)
		{
			Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xPersistent);
			if (pxScene == nullptr) return;

			Zenith_Entity xTemplate = g_xEngine.Scenes().CreateEntity(pxScene, std::string(szName) + "Template");
			xTemplate.AddComponent<Zenith_ModelComponent>().LoadModel(
				std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" + ZENITH_MODEL_EXT);
			if (bWithCollider)
			{
				xTemplate.AddComponent<Zenith_ColliderComponent>().AddCollider(eVolume, eBody);
			}

			auto xhPrefab = Zenith_AssetRegistry::Create<Zenith_Prefab>();
				Zenith_Prefab* pxPrefab = xhPrefab.GetDirect();
			pxPrefab->CreateFromEntity(xTemplate, szName);
			xOut.Set(pxPrefab);

			xTemplate.Destroy();
		}

		void InitializeDPPrefabs()
		{
			if (s_bPrefabsInit) return;
			CreateDPTemplate(Resources().m_xWallPrefab,         "DPWall",         true,  COLLISION_VOLUME_TYPE_OBB,     RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xVillagerPrefab,     "DPVillager",     true,  COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
			CreateDPTemplate(Resources().m_xPriestPrefab,       "DPPriest",       true,  COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
			CreateDPTemplate(Resources().m_xItemPrefab,         "DPItem",         true,  COLLISION_VOLUME_TYPE_SPHERE,  RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xDoorPrefab,         "DPDoor",         true,  COLLISION_VOLUME_TYPE_OBB,     RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xChestPrefab,        "DPChest",        true,  COLLISION_VOLUME_TYPE_AABB,    RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xPentagramPrefab,    "DPPentagram",    true,  COLLISION_VOLUME_TYPE_AABB,    RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xForgePrefab,        "DPForge",        false, COLLISION_VOLUME_TYPE_AABB,    RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xNoiseMachinePrefab, "DPNoiseMachine", false, COLLISION_VOLUME_TYPE_AABB,    RIGIDBODY_TYPE_STATIC);
			CreateDPTemplate(Resources().m_xHeldVisualPrefab,   "DPHeldVisual",   false, COLLISION_VOLUME_TYPE_AABB,    RIGIDBODY_TYPE_STATIC);
			s_bPrefabsInit = true;
		}

		void ShutdownDPPrefabs()
		{
			Resources().m_xWallPrefab.Clear();
			Resources().m_xVillagerPrefab.Clear();
			Resources().m_xPriestPrefab.Clear();
			Resources().m_xItemPrefab.Clear();
			Resources().m_xDoorPrefab.Clear();
			Resources().m_xChestPrefab.Clear();
			Resources().m_xPentagramPrefab.Clear();
			Resources().m_xForgePrefab.Clear();
			Resources().m_xNoiseMachinePrefab.Clear();
			Resources().m_xHeldVisualPrefab.Clear();
			s_bPrefabsInit = false;
		}
	}

	void InitializeResources()
	{
		// Load Config/Tuning.json into the in-process cache before any other
		// resource init so subsequent systems (materials, fog, behaviours)
		// can read tuning values during their own bring-up.
		DP_Tuning::Initialize();

		// MVP-0.2.1+2: Config/Archetypes.json into the archetype cache. Loaded
		// right after Tuning so DPVillager_Component::OnAwake can consult both.
		// Idempotent.
		DP_Archetypes::Initialize();

		// MVP-2.2.1: Config/Reagents.json into the reagent cache. Loaded
		// alongside archetypes so DPItemBase::OnAwake can look up the
		// pickup_channel_s + special_behaviour fields by tag name.
		// Idempotent.
		DP_Reagents::Initialize();

		// Author Zenith_MaterialAssets from the UE parameter dumps under
		// Assets/Materials/*.json. Idempotent — safe across Editor Stop/Play.
		DPMaterials::Initialize();

		// 2026-05-21: in-world particle telegraphs (forge sparks, door dust,
		// pentagram ritual swirl, BellSoul ring, BogWater steam, priest
		// "!" alert, etc). Initialize() registers the configs + subscribes
		// to DP events; the per-scene emitter entities are created by
		// DPProcLevelBootstrap_Component::OnAwake via
		// DP_Particles::EnsureEmittersInScene. Idempotent.
		DP_Particles::Initialize();

		// 2026-05-21: first-encounter tutorialisation. Subscribes to the
		// same DP events as the particles + telemetry, but uses them to
		// emit one-shot tips ("first time you pick up Iron, here's what
		// to do with it"). Tips display in the TutorialOverlay HUD
		// element for ~5 s. Shown-flag table is process-global; reset
		// per run via DP_Tutorial::ResetForNewRun.
		DP_Tutorial::Initialize();

		// Build the per-archetype prefab templates (persistent scene; physics is
		// already up at this point). Idempotent across Editor Stop/Play + batched
		// tests via the s_bPrefabsInit guard.
		InitializeDPPrefabs();

#ifdef ZENITH_INPUT_SIMULATOR
		// Tell the automated-test harness how to wipe DP-specific persistent
		// state between batched tests. The harness force-loads scene 0
		// before firing this hook, so entity-managed side-tables (held
		// items, scent, fog holes, item tags, possession state, win, night)
		// are already cleared via OnDestroy by the time we run. Only state
		// that has no entity owner needs explicit reset here.
		//
		// 2026-05-22 Phase 5.2: removed DP_Player::ResetForNewRun /
		// DP_Win::Reset / DP_Night::Reset from this hook -- their state
		// now lives on DPPlayerController_Component::m_x*, destroyed via
		// the scene-reload OnDestroy. DP_Fog::ClearAll* are also entity-
		// owned (DPFogPass_Component) but kept here defensively so the
		// hook is robust to any future code path that calls it on a scene
		// where the fog-pass component never spun up.
		Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
		{
			DP_Fog::ClearAllFogHoles();
			DP_Fog::ClearAllMemoryReveals();
			DP_AI::ResetLevelNavMesh();
			DPPauseMenuController_Component::ResetForTest();
			// Reset engine perception state (per-agent awareness, registered
			// targets, active sounds) between tests. It has no entity owner and
			// is never cleared on scene reload, so it would otherwise leak across
			// the batch (Reset() previously had zero callers anywhere).
			Zenith_PerceptionSystem::Reset();
			// Drop emitter entities + zero burst counts so the next test
			// gets a fresh particle slate. Configs + subscriptions stay
			// alive across tests (they're process-global).
			DP_Particles::ClearEmitterEntities();
			DP_Particles::ResetBurstCountsForTest();
			// Clear shown-flags so first-encounter tips fire for each
			// batched test independently. Subscriptions stay alive.
			DP_Tutorial::ResetForNewRun();
			// Metagame: per-run Knot tally/chain/banked latch + the meta-
			// save cache and the Zenith_SaveData test recording/readback
			// stash, so meta-progression can't leak across batched tests.
			DP_Knots::ResetForNewRun();
			DP_MetaSave::InvalidateCacheForTest();
			Zenith_SaveData::ClearForTest();
		});
#endif
	}

	void CleanupResources()
	{
		// Reset cross-session state so a re-launched game doesn't inherit win
		// flag / fog holes / held items from a previous run.
		DP_Win::Reset();
		DP_Fog::ClearAllFogHoles();
		DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
		DP_AI::ResetLevelNavMesh();
		// Particles: tear down emitters + free configs + unsubscribe events.
		// Run BEFORE the asset / material teardown -- the emitter entities
		// hold Zenith_ParticleEmitterComponent pointers to configs, and the
		// configs must outlive the emitters.
		DP_Particles::Shutdown();
		// Tutorial: unsubscribe events + clear flags. Order vs Particles
		// doesn't matter (no cross-dependency), but grouped here for
		// consistency.
		DP_Tutorial::Shutdown();
		// Drop the prefab template handles (releases the registry assets) and
		// reset the init guard so Editor Stop/Play + batched tests re-create them.
		ShutdownDPPrefabs();
		DPMaterials::Shutdown();

		// Drop the archetype cache before tuning -- archetypes don't depend on
		// tuning, but keep teardown order paired so future cross-deps are
		// surfaced by the Editor Stop/Play smoke runs. Idempotent.
		DP_Archetypes::Shutdown();

		// Drop the reagent cache paired with archetypes.
		DP_Reagents::Shutdown();

		// Drop the tuning cache last so materials/fog teardown above can still
		// query tuning values if they ever start to. Idempotent.
		DP_Tuning::Shutdown();
	}
}

// ============================================================================
// Project entry points (mirrors Combat's 9-hook layout: Combat.cpp:672–820).
// ============================================================================

const char* Project_GetName()
{
	return "DevilsPlayground";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
	// W0 stub. B6 may set fog technique here; the game disables engine fog
	// entirely via the render graph force-disable overlay
	// (SetOwnerForceDisabled("Fog", true)) inside DPFogPass / SetupDPFog.
}

void Project_RegisterGameComponents()
{
	// Meta-registry registration happens via the ZENITH_REGISTER_COMPONENT
	// macros at the top of this file. This hook remains as the per-game
	// lifecycle entry point for early CPU-only resource initialization that
	// must run in both TOOLS and non-TOOLS builds, plus (tools only) the
	// editor "Add Component" registry mirror — the display names used by
	// AddStep_AddComponent and the editor menu. That registry is
	// append-anytime, not sealed.
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<DPPlayerController_Component>("DPPlayerController");
	xEditorRegistry.RegisterComponent<DPItemManager_Component>("DPItemManager");
	xEditorRegistry.RegisterComponent<DPFogPass_Component>("DPFogPass");
	xEditorRegistry.RegisterComponent<DPHUDController_Component>("DPHUDController");
	xEditorRegistry.RegisterComponent<DPOrbitCamera_Component>("DPOrbitCamera");
	xEditorRegistry.RegisterComponent<DPMinimap_Component>("DPMinimap");
	xEditorRegistry.RegisterComponent<DPPauseMenuController_Component>("DPPauseMenuController");
	xEditorRegistry.RegisterComponent<DPProcLevelBootstrap_Component>("DPProcLevelBootstrap");
	xEditorRegistry.RegisterComponent<DPVillager_Component>("DPVillager");
	xEditorRegistry.RegisterComponent<Priest_Component>("Priest");
	xEditorRegistry.RegisterComponent<DPItemBase_Component>("DPItemBase");
	xEditorRegistry.RegisterComponent<DPItemSpawn_Component>("DPItemSpawn");
	xEditorRegistry.RegisterComponent<DPDoor_Component>("DPDoor");
	xEditorRegistry.RegisterComponent<DPLiminalHub_Component>("DPLiminalHub");
	xEditorRegistry.RegisterComponent<DPForge_Component>("DPForge");
	xEditorRegistry.RegisterComponent<DPGraphInteractable_Component>("DPGraphInteractable");
	xEditorRegistry.RegisterComponent<DPMenuRelay_Component>("DPMenuRelay");
#endif

	// Behaviour Graph node library (used by the boot-authored interactable +
	// menu graphs).
	DP_RegisterGraphNodes();

	// Metagame: meta-save persistence (DP_MetaSave writes the "meta" slot
	// under %APPDATA%/Zenith/DevilsPlayground/) + run-end Knot banking and
	// hand-off-chain reset subscriptions (captureless, process-lifetime;
	// idempotent).
	Zenith_SaveData::Initialise("DevilsPlayground");
	DP_Knots::Initialise();

	DevilsPlayground::InitializeResources();

	// Register the game's fog render feature (anchored after the engine fog step)
	// and force-disable engine fog. Idempotent — safe across Editor Stop/Play.
	DPFogPass::Init();
}

void Project_Shutdown()
{
	// Order matters: unregister the game render feature BEFORE engine resources
	// go away (ShutdownDPFog's override-lift is guarded against the render graph
	// already being torn down).
	DPFogPass::Shutdown();
	DevilsPlayground::CleanupResources();
}

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All DevilsPlayground resources are initialised in
	// Project_RegisterGameComponents via DevilsPlayground::InitializeResources().
}

namespace
{
	// ------------------------------------------------------------------
	// UE asset path  -> Zenith .zmodel path translator.
	//
	//   UE   /Game/DevilsPlayground/Assets/Blockout/Chest/ChestMain.ChestMain
	//   =>   GAME_ASSETS_DIR "Meshes/DevilsPlayground_Assets_Blockout_Chest_ChestMain.zmodel"
	//
	// /Engine/* assets aren't part of the game export pipeline; we substitute
	// the cube-prototype mesh so the entity still renders.  Also a few UE
	// blueprints reference the engine plane proxy ("/Engine/BasicShapes/Plane.Plane")
	// for chests; the chest mesh actually exported is ChestMain, so we
	// hard-redirect to that case.
	//
	// Returns a stable C-string interned in process-static storage so the
	// AddStep_LoadModel(string-pointer) contract survives the lazy AddStep
	// drain.  Returns nullptr for empty / NULL inputs.
	// ------------------------------------------------------------------
	const char* MakeMeshPath(const char* szUePath)
	{
		// Each unique input path yields a fresh heap-allocated buffer that
		// outlives the program.  Linear search of a small table is fine —
		// we expect ~16 unique meshes total.
		static Zenith_Vector<const char*> s_axKeys;
		static Zenith_Vector<const char*> s_axValues;

		if (!szUePath || szUePath[0] == '\0')
		{
			return nullptr;
		}

		for (u_int i = 0; i < s_axKeys.GetSize(); ++i)
		{
			if (std::strcmp(s_axKeys.Get(i), szUePath) == 0)
			{
				return s_axValues.Get(i);
			}
		}

		// Build the file stem (strip /Game/, drop the trailing .X duplicate, /->_).
		std::string strStem;
		if (std::strcmp(szUePath, "/Engine/BasicShapes/Plane.Plane") == 0)
		{
			// BP_ChestInteractable's root component originally pointed to
			// the engine Plane primitive; the actual chest visual lives in
			// a child component. Re-route to the exported Chest_ChestMain
			// mesh so chests render with their real geometry instead of a
			// flat plane fallback.
			strStem = "DevilsPlayground_Assets_Blockout_Chest_ChestMain";
		}
		else if (std::strncmp(szUePath, "/Engine/", 8) == 0)
		{
			// Other engine asset — fall back to a known-exported proxy mesh.
			strStem = "LevelPrototyping_Meshes_SM_Cube";
		}
		else
		{
			const char* szRest = szUePath;
			if (std::strncmp(szRest, "/Game/", 6) == 0)
			{
				szRest += 6;
			}
			strStem = szRest;
			size_t uDot = strStem.rfind('.');
			if (uDot != std::string::npos)
			{
				strStem.resize(uDot);
			}
			for (char& c : strStem)
			{
				if (c == '/') c = '_';
			}
		}

		std::string strFull = std::string(GAME_ASSETS_DIR) + "Meshes/" + strStem + ZENITH_MODEL_EXT;

		// Heap-allocate and intern. The leak is intentional and tiny — these
		// strings live for the full process lifetime.
		size_t uKeyLen = std::strlen(szUePath);
		char* pszKey = new char[uKeyLen + 1];
		std::memcpy(pszKey, szUePath, uKeyLen + 1);
		char* pszValue = new char[strFull.size() + 1];
		std::memcpy(pszValue, strFull.c_str(), strFull.size() + 1);
		s_axKeys.PushBack(pszKey);
		s_axValues.PushBack(pszValue);
		return pszValue;
	}

	// Heap-allocate a stable copy of an interpolated entity name. The
	// editor-automation queue stores const char* pointers (per
	// Zenith_EditorAction's contract — see the header) — passing a stack
	// buffer here would dangle once the calling scope exits, because the
	// queue drains lazily over later frames. The leak is intentional:
	// ~one small allocation per entity, freed at process exit.
	const char* InternEntityName(const char* szPrefix, uint32_t uIndex)
	{
		char* pszName = new char[64];
		std::snprintf(pszName, 64, "%s_%u", szPrefix, uIndex);
		return pszName;
	}


	// Author the model + (optional) collider on the most-recently-created
	// entity. szUePath may be empty/null/`/Engine/...`; the helper handles
	// the substitutions internally. bAddCollider drives whether we append a
	// physics body so click-to-possess raycast / Jolt sims actually fire.
	// eVolumeType picks the shape — defaults to AABB (cheap, exact-fit for
	// walls/chests). Pass COLLISION_VOLUME_TYPE_SPHERE for characters: the
	// sphere uses max(scale.x,y,z)*0.5 as its radius, which from a humanoid
	// scale of (0.6, 1.8, 0.6) gives a 0.9 m radius — generously wider than
	// the visible block on X/Z so clicks from a top-down camera have margin.
	void AuthorMeshAndCollider(const char* szUePath, bool bAddCollider,
		CollisionVolumeType eVolumeType = COLLISION_VOLUME_TYPE_AABB,
		RigidBodyType eBodyType = RIGIDBODY_TYPE_STATIC)
	{
		const char* szResolved = MakeMeshPath(szUePath);
		if (szResolved != nullptr)
		{
			g_xEngine.EditorAutomation().AddStep_AddModel();
			g_xEngine.EditorAutomation().AddStep_LoadModel(szResolved);
			if (Zenith_MaterialAsset* pxDefault = DPMaterials::GetDefaultMaterial())
			{
				g_xEngine.EditorAutomation().AddStep_SetModelMaterial(0, pxDefault);
			}
		}
		if (bAddCollider)
		{
			g_xEngine.EditorAutomation().AddStep_AddCollider();
			g_xEngine.EditorAutomation().AddStep_AddColliderShape(eVolumeType, eBodyType);
		}
	}

	void AuthorFrontEndScene()
	{
		g_xEngine.EditorAutomation().AddStep_CreateScene("FrontEnd");

		g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
		g_xEngine.EditorAutomation().AddStep_AddCamera();
		g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.0f, 5.0f, -10.0f);
		g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.3f);
		g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(60.0f));
		g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();

		g_xEngine.EditorAutomation().AddStep_AddUI();
		g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "DEVIL'S PLAYGROUND");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		// MVP-UI-polish: TextAlignment::Center is required for the text to
		// render symmetrically around the anchor point. Without it the
		// text flows left-aligned FROM the anchor and looks off-centre.
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.0f, -160.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", DPUI::fMENU_TITLE_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 0.9f, 0.2f, 0.2f, 1.0f);

		g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.0f, 0.0f);
		g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("MenuPlay", DPUI::fMENU_BTN_FONT);

		// Metagame v1: the Liminal (hermit shrines / Knot spending). Sits
		// below Play; DPMenuRelay fires "MenuLiminal" -> DP_MainMenu.bgraph
		// loads scene index 2.
		g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuLiminal", "The Liminal");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuLiminal", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuLiminal", 0.0f, DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING);
		g_xEngine.EditorAutomation().AddStep_SetUISize("MenuLiminal", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("MenuLiminal", DPUI::fMENU_BTN_FONT);

		// MVP-2.5.6: main-menu Quit button. Sits below the Liminal.
		g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuQuit", "Quit");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuQuit", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuQuit", 0.0f, 2.0f * (DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING));
		g_xEngine.EditorAutomation().AddStep_SetUISize("MenuQuit", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H);
		g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("MenuQuit", DPUI::fMENU_BTN_FONT);

		// 2026-05-16 instructional HUD: main-menu primer. Short summary
		// of the goal + the four most useful keys, plus a reminder that
		// [H] in-game opens the full help overlay. First thing a new
		// player reads after launching the game.
		//
		// Layout calculation (1920x1080 reference; Center anchor has
		// pivot 0.5,0.5 so position is measured to ELEMENT CENTRE):
		//   MenuQuit centre  = +128, half-height 48  -> bottom at +176
		//   MenuHowToTitle   = +230, half-height 25  -> top at +205 (~29px gap)
		//                        text top y = +205, font 36 -> bottom ~+241
		//   MenuHowToBody    = +380, half-height 110 -> top at +270 (~29px gap)
		//                        text top y = +270, ~6 lines * 33px -> bottom ~+468
		// Earlier values (+208 / +268) overlapped because the body's box
		// half-height (110) put its top edge ABOVE the title's bottom.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuHowToTitle", "How to play");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuHowToTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuHowToTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUISize("MenuHowToTitle", 1000.0f, 50.0f);
		// One extra button row (the Liminal) shifted the how-to block down
		// a slot vs the original 2-button layout — same 102/252 gaps below
		// the LAST button, which is now at 2*(H+SPACING).
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuHowToTitle", 0.0f, 2.0f * (DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING) + 102.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuHowToTitle", DPUI::fMENU_HOWTO_TITLE_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuHowToTitle", 1.0f, 0.85f, 0.6f, 1.0f);

		g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuHowToBody",
			"You are a demon. Possess villagers ([LMB]) and deliver 5 objectives\n"
			"from chests to the pentagram before dawn. Avoid the priest.\n"
			"\n"
			"[WASD] move    [Shift] sprint    [F] interact    [Esc] pause\n"
			"\n"
			"Press [H] in-game for the full controls + mechanics reference.");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuHowToBody", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuHowToBody", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUISize("MenuHowToBody", 1100.0f, 220.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuHowToBody", 0.0f, 2.0f * (DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING) + 252.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuHowToBody", DPUI::fMENU_HOWTO_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuHowToBody", 0.9f, 0.9f, 0.85f, 1.0f);

		// Menu logic lives in the boot-authored DP_MainMenu graph; the relay owns
	// only the UI-button wiring (fires MenuPlay/MenuQuit custom events).
	g_xEngine.EditorAutomation().AddStep_AddComponent("Graph");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/DP_MainMenu.bgraph");
	g_xEngine.EditorAutomation().AddStep_AddComponent("DPMenuRelay");

		g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
		g_xEngine.EditorAutomation().AddStep_UnloadScene();
	}

	// ============================================================================
	// AuthorLiminalScene — the between-Nights meta-progression hub (metagame
	// v1, GDD §5.4). Three hermit shrines (Wynstan's Forge / Mereworth's Eye /
	// Old Bett's Breath), each a 12-node unlock column, plus the Knot balance
	// + last-run readouts and a back button. All spend/refresh logic lives in
	// DPLiminalHub_Component (plain C++ — documented doctrine divergence; see
	// the component header). Saved as build index 2.
	// ============================================================================
	void AuthorLiminalScene()
	{
		Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
		xAuto.AddStep_CreateScene("Liminal");

		xAuto.AddStep_CreateEntity("GameManager");
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(0.0f, 5.0f, -10.0f);
		xAuto.AddStep_SetCameraPitch(-0.3f);
		xAuto.AddStep_SetCameraFOV(glm::radians(60.0f));
		xAuto.AddStep_SetAsMainCamera();

		xAuto.AddStep_AddUI();
		xAuto.AddStep_CreateUIText("LiminalTitle", "THE LIMINAL");
		xAuto.AddStep_SetUIAnchor("LiminalTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIAlignment("LiminalTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
		xAuto.AddStep_SetUIPosition("LiminalTitle", 0.0f, -330.0f);
		xAuto.AddStep_SetUIFontSize("LiminalTitle", DPUI::fMENU_TITLE_FONT);
		xAuto.AddStep_SetUIColor("LiminalTitle", 0.75f, 0.75f, 0.95f, 1.0f);

		// Balance + run-results readouts; text refreshed by the hub component.
		xAuto.AddStep_CreateUIText("LiminalKnotBalance", "Knots: 0");
		xAuto.AddStep_SetUIAnchor("LiminalKnotBalance", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIAlignment("LiminalKnotBalance", static_cast<int>(Zenith_UI::TextAlignment::Center));
		xAuto.AddStep_SetUIPosition("LiminalKnotBalance", -160.0f, -295.0f);
		xAuto.AddStep_SetUIFontSize("LiminalKnotBalance", 30.0f);
		xAuto.AddStep_SetUIColor("LiminalKnotBalance", 0.95f, 0.80f, 0.30f, 1.0f);

		xAuto.AddStep_CreateUIText("LiminalLastRun", "Last run: +0 Knots");
		xAuto.AddStep_SetUIAnchor("LiminalLastRun", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIAlignment("LiminalLastRun", static_cast<int>(Zenith_UI::TextAlignment::Center));
		xAuto.AddStep_SetUIPosition("LiminalLastRun", 160.0f, -295.0f);
		xAuto.AddStep_SetUIFontSize("LiminalLastRun", 24.0f);
		xAuto.AddStep_SetUIColor("LiminalLastRun", 0.85f, 0.85f, 0.9f, 1.0f);

		// Three shrine columns. Track order matches DP_MetaSave::HermitTrack.
		//
		// REGRESSION NOTE: this used to hold names in `static std::string`
		// arrays because Zenith_EditorAutomation::AddStep_* stored its
		// const char* args BY POINTER until the boot-time drain executed the
		// queue, so names generated into a stack buffer dangled by drain time
		// (all 36 buttons collapsed into one garbage-named element). Fixed
		// engine-side: Zenith_EditorAction now owns copies of its string args
		// (see Zenith/Editor/Zenith_EditorAutomation.h), so plain per-iteration
		// stack buffers are safe again.
		const char* aszTrackNames[3] = { "Wynstan's Forge", "Mereworth's Eye", "Old Bett's Breath" };
		const float afColumnX[3] = { -420.0f, 0.0f, 420.0f };
		for (uint32_t uTrack = 0; uTrack < 3; ++uTrack)
		{
			char szHeader[48];
			std::snprintf(szHeader, sizeof(szHeader), "LiminalTrack%u", uTrack);
			xAuto.AddStep_CreateUIText(szHeader, aszTrackNames[uTrack]);
			xAuto.AddStep_SetUIAnchor(szHeader, static_cast<int>(Zenith_UI::AnchorPreset::Center));
			xAuto.AddStep_SetUIAlignment(szHeader, static_cast<int>(Zenith_UI::TextAlignment::Center));
			xAuto.AddStep_SetUISize(szHeader, 320.0f, 32.0f);
			xAuto.AddStep_SetUIPosition(szHeader, afColumnX[uTrack], -255.0f);
			xAuto.AddStep_SetUIFontSize(szHeader, 26.0f);
			xAuto.AddStep_SetUIColor(szHeader, 0.9f, 0.85f, 0.7f, 1.0f);

			for (uint32_t uNode = 0; uNode < 12; ++uNode)
			{
				char szName[48];
				char szLabel[48];
				std::snprintf(szName, sizeof(szName), "LiminalNode_T%u_N%u", uTrack, uNode);
				std::snprintf(szLabel, sizeof(szLabel), "Node %u  (%u Knots)", uNode + 1, 2u + uNode);
				xAuto.AddStep_CreateUIButton(szName, szLabel);
				xAuto.AddStep_SetUIAnchor(szName, static_cast<int>(Zenith_UI::AnchorPreset::Center));
				xAuto.AddStep_SetUIPosition(szName, afColumnX[uTrack], -215.0f + 40.0f * static_cast<float>(uNode));
				xAuto.AddStep_SetUISize(szName, 260.0f, 36.0f);
				xAuto.AddStep_SetUIButtonFontSize(szName, 18.0f);
			}
		}

		xAuto.AddStep_CreateUIButton("LiminalBack", "Back");
		xAuto.AddStep_SetUIAnchor("LiminalBack", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		xAuto.AddStep_SetUIPosition("LiminalBack", 0.0f, 290.0f);
		xAuto.AddStep_SetUISize("LiminalBack", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H * 0.6f);
		xAuto.AddStep_SetUIButtonFontSize("LiminalBack", DPUI::fMENU_BTN_FONT);

		xAuto.AddStep_AddComponent("DPLiminalHub");

		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Liminal" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	// ============================================================================
	// AuthorDPGameSceneFrame - the GameManager + PauseManager scaffolding for
	// the gameplay scene. Used by AuthorProcLevelScene. Kept as a separate
	// helper so the HUD + global-controller wiring is in one place even though
	// there's only one consumer now (was shared with AuthorGameLevelScene
	// before that scene was removed 2026-05-19).
	//
	// Caller's contract: invoke between AddStep_CreateScene(...) and the
	// level-specific authoring (ground plane, entities, lights, ...). The
	// helper does NOT save or unload the scene -- the caller does that.
	//
	// Camera defaults (50, 35, -15) + pitch -0.85 + FOV 55° are starting
	// values; the procgen bootstrap overrides them at runtime via
	// DPOrbitCamera_Component's SetOrbitTarget / SetOrbitDistance setters
	// once the layout bounds are known.
	// ============================================================================
	void AuthorDPGameSceneFrame()
	{
		// ------ GameManager: hosts global controllers + HUD UI ----------------
		g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");

		g_xEngine.EditorAutomation().AddStep_AddCamera();
		// Pre-possession overview camera — positioned over the centre of the
		// authored playable area (X+Z range ≈ 0..100, so centre ≈ (50, 0, 50))
		// and pulled back/up so the player sees most of the map before
		// clicking a villager. Procgen overrides via DPOrbitCamera setters
		// at runtime once bounds are known.
		g_xEngine.EditorAutomation().AddStep_SetCameraPosition(50.0f, 35.0f, -15.0f);
		g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.85f);  // ~49° down
		g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(55.0f));
		g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();

		g_xEngine.EditorAutomation().AddStep_AddUI();
		// MVP-UI-polish: TopLeft / BottomLeft anchored text gets Left alignment
		// (default but explicit); TopRight gets Right; *Center anchors get
		// Center. This keeps text growing away from the screen edge instead
		// of toward it, fixing the "off the right hand side" bug.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("LifeBar", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LifeBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("LifeBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("LifeBar", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LifeBar", DPUI::fHUD_LIFEBAR_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("LifeBar", 0.3f, 1.0f, 0.3f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("LifeBar", false);

		// Held-item readout sits below the life bar — same anchor, offset down.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("HeldItem", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HeldItem", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("HeldItem", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("HeldItem", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 50.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("HeldItem", DPUI::fHUD_HELDITEM_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("HeldItem", 1.0f, 1.0f, 1.0f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("HeldItem", false);

		// Objective counter, top-right corner. Right-aligned so the text
		// grows leftward into the screen instead of off the right edge.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("Objectives", "Objectives: 0/5");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Objectives", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Objectives", static_cast<int>(Zenith_UI::TextAlignment::Right));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("Objectives", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Objectives", DPUI::fHUD_OBJECTIVES_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("Objectives", 0.95f, 0.7f, 0.7f, 1.0f);

		g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.0f, -80.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", DPUI::fHUD_STATUS_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.9f, 0.2f, 0.2f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

		// 2026-05-21: LockedDoorAlert. A short-lived warning under the Status
		// banner that fires on DP_OnDoorLockRejected. Pre-fix the player got
		// zero feedback that a door was locked -- F-press silently did
		// nothing. The HUD now flashes "LOCKED -- needs Key" or similar
		// for ~2 s. Hidden by default; the controller flips visibility.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("LockedDoorAlert", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LockedDoorAlert", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("LockedDoorAlert", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("LockedDoorAlert", 0.0f, -40.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LockedDoorAlert", DPUI::fHUD_STATUS_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("LockedDoorAlert", 0.95f, 0.30f, 0.20f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("LockedDoorAlert", false);

		// 2026-05-21: ScentBar. The companion bar visualisation to
		// ScentIndicator (which is a numeric "Scent: 0.42"). The bar
		// gives the player a glanceable readout of scent saturation
		// across the 0-1 range, with colour signalling when scent
		// crosses the hound-bark threshold (0.5).
		g_xEngine.EditorAutomation().AddStep_CreateUIText("ScentBar", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ScentBar", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ScentBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("ScentBar", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET - 25.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ScentBar", DPUI::fHUD_SCENT_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("ScentBar", 0.7f, 0.3f, 0.9f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("ScentBar", false);

		// 2026-05-21: ArchetypeStatus. A one-line description of the
		// possessed villager's archetype-specific gameplay rule.
		// Sits below VillagerInfo on the TopLeft stack. Hidden when
		// not possessing or possessing a Farmhand (baseline).
		g_xEngine.EditorAutomation().AddStep_CreateUIText("ArchetypeStatus", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ArchetypeStatus", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ArchetypeStatus", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("ArchetypeStatus", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 245.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ArchetypeStatus", DPUI::fHUD_VILLAGER_INFO_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("ArchetypeStatus", 0.95f, 0.85f, 0.65f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("ArchetypeStatus", false);

		// 2026-05-21: TutorialOverlay. Large centered text driven by
		// DP_Tutorial::GetActiveTipText. Shows first-encounter tips
		// (FirstPossession, FirstIronPickup, FirstLockedDoor, etc) for
		// ~5 s each, auto-clearing. Sits above the lower-third
		// WhisperLine + InteractHint stack so it doesn't compete for
		// the bottom-of-screen "current advice" space; uses the
		// vertical middle for the eye-catch-while-playing read.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialOverlay", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialOverlay", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialOverlay", 0.0f, 120.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialOverlay", DPUI::fHUD_WHISPER_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialOverlay", 1.0f, 0.95f, 0.75f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("TutorialOverlay", false);

		// MVP-2.5.4: Dawn sun-gauge -- text countdown at top-centre.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("DawnGauge", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("DawnGauge", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("DawnGauge", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("DawnGauge", 0.0f, DPUI::fEDGE_INSET);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("DawnGauge", DPUI::fHUD_DAWN_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("DawnGauge", 1.0f, 0.85f, 0.6f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("DawnGauge", false);

		// MVP-2.5.2: Scent indicator at bottom-left.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("ScentIndicator", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ScentIndicator", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ScentIndicator", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("ScentIndicator", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ScentIndicator", DPUI::fHUD_SCENT_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("ScentIndicator", 0.7f, 0.3f, 0.9f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("ScentIndicator", false);

		// MVP-2.5.1: Whisper line at bottom-centre.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("WhisperLine", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("WhisperLine", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("WhisperLine", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("WhisperLine", 0.0f, -100.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("WhisperLine", DPUI::fHUD_WHISPER_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("WhisperLine", 0.85f, 0.4f, 0.4f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("WhisperLine", false);

		// MVP-2.5.3: Aelfric awareness icon top-right, below the Objectives
		// counter. Right-aligned to grow leftward into the screen.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("AelfricAwareness", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("AelfricAwareness", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("AelfricAwareness", static_cast<int>(Zenith_UI::TextAlignment::Right));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("AelfricAwareness", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 60.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("AelfricAwareness", DPUI::fHUD_AWARENESS_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("AelfricAwareness", 0.95f, 0.6f, 0.3f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("AelfricAwareness", false);

		// --- DETAILED HUD (user feedback 2026-05-15) ---------------------
		// Eight extra readouts that fill in the gameplay picture:
		// possession info, movement mode, threat distance, run economy,
		// contextual hints. Each is anchored such that text grows AWAY
		// from the screen edge (Left anchors -> Left align, Right ->
		// Right, Centers -> Center).
		//
		// Vertical stack order on each edge (top to bottom):
		//   TopLeft:    LifeBar (40), LifeNumeric (90), HeldItem (130),
		//               ReagentHelp (175), VillagerInfo (215)
		//   TopRight:   Objectives (40), AelfricAwareness (100),
		//               VillagersAlive (155), PriestDistance (205)
		//   TopCenter:  DawnGauge (40), RunTimer (95)
		//   BottomLeft: ScentIndicator (-40), MovementMode (-85)
		//   BottomCenter: WhisperLine (-100), InteractHint (-160)

		// VillagerInfo -- archetype name when possessing.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("VillagerInfo", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VillagerInfo", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VillagerInfo", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("VillagerInfo", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 215.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VillagerInfo", DPUI::fHUD_VILLAGER_INFO_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("VillagerInfo", 0.85f, 0.85f, 1.0f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("VillagerInfo", false);

		// LifeNumeric -- "Life: 23.4 / 30.0 s" alongside the ASCII bar.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("LifeNumeric", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("LifeNumeric", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("LifeNumeric", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("LifeNumeric", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 90.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("LifeNumeric", DPUI::fHUD_LIFE_NUMERIC_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("LifeNumeric", 0.7f, 1.0f, 0.7f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("LifeNumeric", false);

		// ReagentHelp -- one-line description shown when holding a special
		// reagent (BellSoul / BogWater / SkeletonKey). Hidden otherwise.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("ReagentHelp", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ReagentHelp", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ReagentHelp", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("ReagentHelp", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 175.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ReagentHelp", DPUI::fHUD_REAGENT_HELP_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("ReagentHelp", 0.9f, 0.85f, 0.6f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("ReagentHelp", false);

		// MovementMode -- Sprint / Walk-Quiet / Move. Bottom-left, above ScentIndicator.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("MovementMode", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MovementMode", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MovementMode", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("MovementMode", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET - 50.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MovementMode", DPUI::fHUD_MOVEMENT_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("MovementMode", 0.85f, 0.85f, 0.85f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("MovementMode", false);

		// VillagersAlive -- count of live villagers. Top-right, below AelfricAwareness.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("VillagersAlive", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("VillagersAlive", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("VillagersAlive", static_cast<int>(Zenith_UI::TextAlignment::Right));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("VillagersAlive", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 120.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("VillagersAlive", DPUI::fHUD_VILLAGER_COUNT_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("VillagersAlive", 0.85f, 0.85f, 1.0f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("VillagersAlive", false);

		// PriestDistance -- meters to closest priest. Top-right, below VillagersAlive.
		// Controller recolours red/amber/grey based on distance.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("PriestDistance", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PriestDistance", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PriestDistance", static_cast<int>(Zenith_UI::TextAlignment::Right));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("PriestDistance", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 170.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PriestDistance", DPUI::fHUD_PRIEST_DIST_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("PriestDistance", 0.85f, 0.85f, 0.85f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("PriestDistance", false);

		// RunTimer -- mm:ss since first possession. Top-center, below DawnGauge.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("RunTimer", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("RunTimer", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("RunTimer", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("RunTimer", 0.0f, DPUI::fEDGE_INSET + 55.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("RunTimer", DPUI::fHUD_RUN_TIMER_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("RunTimer", 0.9f, 0.9f, 0.7f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("RunTimer", false);

		// InteractHint -- "F to interact with door/chest/forge/...".
		// Bottom-center, above WhisperLine. Visible only when near an interactable.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("InteractHint", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("InteractHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("InteractHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("InteractHint", 0.0f, -160.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("InteractHint", DPUI::fHUD_INTERACT_HINT_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("InteractHint", 1.0f, 0.95f, 0.5f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("InteractHint", false);

		// ----------------------------------------------------------------
		// Instructional HUD (user feedback 2026-05-16): tooltips, hotkeys,
		// and step-by-step instructions visible on the HUD itself so a
		// brand-new player understands the game just by looking.
		//
		// Three layers:
		//   ControlsHint  -- persistent hotkey cheat-sheet (BottomRight).
		//   TutorialHint  -- single-line context guidance (TopCenter).
		//   HelpOverlay   -- full-screen modal toggled with [H].
		//
		// IMPORTANT ordering: HelpBg rect is authored BEFORE HelpOverlay
		// text. The canvas iterates root elements in insertion order, so
		// the rect renders first (behind) and the text on top.
		// ----------------------------------------------------------------

		// ControlsHint -- always-visible hotkey cheat-sheet, BottomRight.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("ControlsHint",
			"[LMB] Possess villager\n"
			"[WASD] Move    [Q/E] Camera\n"
			"[Shift] Sprint  [Ctrl] Walk quiet\n"
			"[F] Interact   [G] Drop item\n"
			"[Space] Ability   [Wheel] Zoom\n"
			"[Esc] Pause   [R] Restart   [H] Help");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ControlsHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ControlsHint", static_cast<int>(Zenith_UI::TextAlignment::Right));
		g_xEngine.EditorAutomation().AddStep_SetUISize("ControlsHint", 550.0f, 220.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("ControlsHint", -DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ControlsHint", DPUI::fHUD_CONTROLS_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("ControlsHint", 0.85f, 0.85f, 0.85f, 0.9f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("ControlsHint", true);

		// TutorialHint -- single-line context guidance, TopCenter.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("TutorialHint", "");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("TutorialHint", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("TutorialHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUISize("TutorialHint", 1200.0f, 50.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("TutorialHint", 0.0f, DPUI::fEDGE_INSET + 110.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TutorialHint", DPUI::fHUD_TUTORIAL_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("TutorialHint", 1.0f, 0.95f, 0.7f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("TutorialHint", false);

		// HelpBg -- semi-transparent background for the HelpOverlay modal.
		// Authored BEFORE HelpOverlay so it renders behind (canvas
		// iterates root elements in insertion order).
		g_xEngine.EditorAutomation().AddStep_CreateUIRect("HelpBg");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HelpBg", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUISize("HelpBg", 1200.0f, 900.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("HelpBg", 0.0f, 0.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("HelpBg", 0.02f, 0.02f, 0.04f, 0.92f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("HelpBg", false);

		// HelpOverlay -- full-screen tutorial text, toggled with [H].
		g_xEngine.EditorAutomation().AddStep_CreateUIText("HelpOverlay",
			"HOW TO PLAY    (press [H] to close)\n"
			"\n"
			"GOAL\n"
			"  You are a demon possessing the villagers of Aelfric's Reach.\n"
			"  Each villager you possess lives 30 seconds before burning out.\n"
			"  Collect 5 objectives from chests and deliver each to the\n"
			"  pentagram in the centre. Survive until dawn.\n"
			"\n"
			"YOU LOSE IF\n"
			"  * Aelfric (the priest) catches a possessed villager.\n"
			"  * Dawn breaks before you deliver 5 objectives.\n"
			"  * Every villager runs out of life.\n"
			"\n"
			"CONTROLS\n"
			"  LMB           Possess villager under the cursor\n"
			"  WASD          Move the possessed villager\n"
			"  Shift         Sprint -- faster, louder, drains life quickly\n"
			"  Ctrl          Walk quiet -- slower, half-volume footsteps\n"
			"  F             Interact (door / chest / forge / pentagram)\n"
			"  G             Drop the held item at your feet\n"
			"  Space         Ability (archetype-specific)\n"
			"  Q / E         Rotate the orbit camera\n"
			"  Mouse Wheel   Zoom the camera\n"
			"  Esc           Pause\n"
			"  R             Restart after victory or loss\n"
			"  H             Toggle this help screen\n"
			"\n"
			"REAGENTS\n"
			"  BellSoul      Rings on pickup -- alerts every priest\n"
			"  BogWater      Evaporates 8 seconds after you drop it\n"
			"  SkeletonKey   Opens any locked door\n"
			"  Iron          Carry to a forge for a Skeleton Key");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HelpOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("HelpOverlay", static_cast<int>(Zenith_UI::TextAlignment::Left));
		g_xEngine.EditorAutomation().AddStep_SetUISize("HelpOverlay", 1150.0f, 870.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("HelpOverlay", 0.0f, 0.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("HelpOverlay", DPUI::fHUD_HELP_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("HelpOverlay", 0.98f, 0.95f, 0.85f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("HelpOverlay", false);

		// MVP-4.3.2: post-victory / post-run-lost restart prompt.
		g_xEngine.EditorAutomation().AddStep_CreateUIText("RestartPrompt", "Press R to restart, Q to quit");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("RestartPrompt", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("RestartPrompt", 0.0f, 60.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("RestartPrompt", 28.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("RestartPrompt", 1.0f, 1.0f, 1.0f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("RestartPrompt", false);

		// Attach the global coordinators. Each is independent — order doesn't
		// matter, but the convention is to attach the player first.
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPPlayerController");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPItemManager");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPFogPass");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPHUDController");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPOrbitCamera");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPMinimap");

		// ------ PauseManager: dedicated entity for the pause controller -------
		// MVP-1.1: the pause controller migrates itself to the persistent
		// scene in OnStart so it keeps ticking while the gameplay scene is
		// paused. Dedicated entity so MarkEntityPersistent doesn't drag
		// the camera / HUD / fog pass to the persistent scene with it.
		g_xEngine.EditorAutomation().AddStep_CreateEntity("PauseManager");
		g_xEngine.EditorAutomation().AddStep_AddUI();
		g_xEngine.EditorAutomation().AddStep_CreateUIText("PauseOverlay", "PAUSED\nEsc: Resume   R: Restart   Q: Quit");
		g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PauseOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PauseOverlay", static_cast<int>(Zenith_UI::TextAlignment::Center));
		g_xEngine.EditorAutomation().AddStep_SetUIPosition("PauseOverlay", 0.0f, 0.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PauseOverlay", DPUI::fHUD_PAUSE_FONT);
		g_xEngine.EditorAutomation().AddStep_SetUIColor("PauseOverlay", 1.0f, 1.0f, 1.0f, 1.0f);
		g_xEngine.EditorAutomation().AddStep_SetUIVisible("PauseOverlay", false);
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPPauseMenuController");
	}

	// (AuthorGameLevelScene removed 2026-05-19 -- the UE-exported GameLevel
	// scene is gone. Procgen-driven ProcLevel is now the only gameplay
	// surface. See AuthorProcLevelScene below.)

	// ============================================================================
	// ProcLevel scene -- the procgen-driven gameplay scene.
	//
	// Static authoring is intentionally lean: GameManager + UI scaffolding,
	// PauseManager, GroundPlane, a handful of corner lights, plus a single
	// "ProcLevelBootstrap" entity carrying DPProcLevelBootstrap_Component.
	//
	// At runtime, the bootstrap's OnAwake calls DPProcLevel::Generate() with
	// seed = m_uSeed (default 0) and then spawns the full level under the
	// active scene:
	//   * 48 wall entities (P4b)
	//   * 12 game elements: pentagram, forge, doors, chests, noise, iron + 5
	//     objectives (P4c)
	//   * 17 villagers + 1 priest (P4d)
	//
	// The saved .zscen file therefore stays small (a half-dozen authored
	// entities). Every level layout cell, item, character is conjured at
	// load-time -- so changing the seed produces a different level without
	// re-authoring anything.
	// ============================================================================
	void AuthorProcLevelScene()
	{
		g_xEngine.EditorAutomation().AddStep_CreateScene("ProcLevel");

		// GameManager + PauseManager + full HUD + global controller scripts.
		// The orbit camera's pose is overridden at runtime by
		// DPProcLevelBootstrap::FrameCameraToLevel once the layout bounds
		// are known.
		AuthorDPGameSceneFrame();

		// ------ Ground plane --------------------------------------------------
		// SM_Cube is a unit cube anchored at its (0, 0, 0) corner (mesh
		// bounds [0, 1]³ -- see the .gltf min/max). The visible mesh is
		// NOT auto-centred; entity position is the mesh's MIN corner.
		// So to cover the procgen XZ range [0, 100] with a 1 m thick
		// slab we anchor at (0, 0, 0) and scale (100, 1, 100). The
		// previous values (position 50,0,50 + scale 50,0.5,50) put the
		// floor in the top-right quadrant only -- reported 2026-05-18
		// by the user.
		g_xEngine.EditorAutomation().AddStep_CreateEntity("GroundPlane");
		g_xEngine.EditorAutomation().AddStep_SetTransformPosition(0.0f, 0.0f, 0.0f);
		g_xEngine.EditorAutomation().AddStep_SetTransformScale(100.0f, 1.0f, 100.0f);
		AuthorMeshAndCollider(
			"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true,
			COLLISION_VOLUME_TYPE_OBB,
			RIGIDBODY_TYPE_STATIC);
		// Dark cobblestone ground (overrides the default grey) so the floor reads
		// as a dim stone courtyard rather than a bright slab.
		if (Zenith_MaterialAsset* pxGround = DPMaterials::GetOrCreateNamedMaterial(
			"DP_Ground", Zenith_Maths::Vector3(0.16f, 0.15f, 0.14f), 0.92f, 0.0f,
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.0f))
		{
			g_xEngine.EditorAutomation().AddStep_SetModelMaterial(0, pxGround);
		}

		// ------ Four corner lights for general visibility ---------------------
		// Hand-authored point lights at high y so the procgen-spawned walls
		// + items light up across the bounds. Intensity tuned to a candle /
		// torch range so the scene isn't over-bloomed.
		const float afLightXZ[4][2] = {
			{ 25.0f, 25.0f }, { 75.0f, 25.0f },
			{ 25.0f, 75.0f }, { 75.0f, 75.0f },
		};
		for (uint32_t i = 0; i < 4; ++i)
		{
			g_xEngine.EditorAutomation().AddStep_CreateEntity(InternEntityName("ProcLight", i));
			g_xEngine.EditorAutomation().AddStep_SetTransformPosition(
				afLightXZ[i][0], 10.0f, afLightXZ[i][1]);
			g_xEngine.EditorAutomation().AddStep_AddComponent("Light");
			g_xEngine.EditorAutomation().AddStep_SetLightIntensity(2000.0f);
			g_xEngine.EditorAutomation().AddStep_SetLightRange(60.0f);
			g_xEngine.EditorAutomation().AddStep_SetLightColor(1.0f, 0.95f, 0.85f);
		}

		// ------ The bootstrap entity ------------------------------------------
		// Attaching this component is the ENTIRE level-content authoring. Every
		// wall, item, character is materialised by the component's OnAwake at
		// scene-load time. Changing m_uSeed (or the upcoming Tuning.json
		// seed source) produces a different level without re-authoring.
		g_xEngine.EditorAutomation().AddStep_CreateEntity("ProcLevelBootstrap");
		g_xEngine.EditorAutomation().AddStep_AddComponent("DPProcLevelBootstrap");

		g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/ProcLevel" ZENITH_SCENE_EXT);
		g_xEngine.EditorAutomation().AddStep_UnloadScene();
	}
}

// ============================================================================
// AuthorBehaviourGraphs - boot-time authoring of DP's gameplay graphs through
// the graph editor's atomic actions (each step = the exact operation a human
// performs in the editor: open, click palette entries, select nodes, edit
// property rows, drag pin connections, add variables, Save). Regenerated from
// scratch every boot, like the scenes.
//
// The graphs carry the gameplay logic the retired C++ interactable/menu
// components owned; DPGraphInteractable_Component / DPMenuRelay_Component fire
// "Interact" / "MenuPlay" / "MenuQuit" custom events into them.
// ============================================================================
// ---- DP_Villager decisions, split (Phase 3) into per-concern sub-builders.
// The four VillagerTick chains + the shared VillagerApplyDrain tail are
// independent (shared blackboard vars, no shared node ids); called in the
// original order they preserve node-creation + edge-add order -> byte-identical
// authoring. Custom-event source chains run in NODE ORDER, reproducing the
// retired OnUpdate's same-frame order: transitions -> movement booleans ->
// life drain/Kill -> footsteps.

static void BuildDPVillager_PossessionTransitions(Zenith_EngineGraphBuilder& xB)
{
	// T1: possession transitions (the retired OnUpdate switch).
	Zenith_GraphChain xTick = xB.OnCustomEvent("VillagerTick", "dt");
	const u_int uSwitch = xB.SwitchOnInt("state", 4);
	xTick.Then(uSwitch);

	// Idle + possessed -> Possessed (life bump to max).
	const u_int uIdleGate = xB.Gate("possessedNow");
	const u_int uToPossessed = xB.SetBlackboardInt("state", 1);
	const u_int uBumpLife = xB.Node("MathBlackboardFloat");	// remainingLife = maxLife
	xB.ParamString(uBumpLife, "m_strVar", "maxLife");
	xB.ParamEnum(uBumpLife, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamFloat(uBumpLife, "m_fOperand", 0.0f);
	xB.ParamString(uBumpLife, "m_strResultVar", "remainingLife");
	xB.Edge(uSwitch, 0, uIdleGate);
	xB.Chain(uIdleGate, uToPossessed).Chain(uToPossessed, uBumpLife);

	// Possessed + un-possessed -> Fainted (arm faint timer LIVE from
	// tuning, like the retired transition). Kill()'s burn-out path never
	// reaches this: it writes state=Dead synchronously mid-tick, so the
	// next tick dispatches the Dead pin instead (quirk preserved).
	const u_int uBrUnpossess = xB.Branch("possessedNow");
	const u_int uToFainted = xB.SetBlackboardInt("state", 2);
	const u_int uArmFaint = xB.Node("DPReadTuningFloat");
	xB.ParamString(uArmFaint, "m_strKey", "possession.voluntary_switch_faint_recovery_s");
	xB.ParamString(uArmFaint, "m_strVar", "faintRecovery");
	xB.Edge(uSwitch, 1, uBrUnpossess);
	xB.Edge(uBrUnpossess, 1, uToFainted);	// false pin = un-possessed
	xB.Chain(uToFainted, uArmFaint);

	// Fainted: system-path wake bypass, else recovery countdown -> Idle.
	const u_int uBrWake = xB.Branch("possessedNow");
	xB.Edge(uSwitch, 2, uBrWake);
	const u_int uWake = xB.SetBlackboardInt("state", 1);
	const u_int uWakeLife = xB.Node("MathBlackboardFloat");	// remainingLife = maxLife
	xB.ParamString(uWakeLife, "m_strVar", "maxLife");
	xB.ParamEnum(uWakeLife, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamFloat(uWakeLife, "m_fOperand", 0.0f);
	xB.ParamString(uWakeLife, "m_strResultVar", "remainingLife");
	const u_int uWakeClear = xB.SetBlackboardFloat("faintRecovery", 0.0f);
	xB.Edge(uBrWake, 0, uWake);
	xB.Chain(uWake, uWakeLife).Chain(uWakeLife, uWakeClear);

	const u_int uRecTick = xB.Node("MathBlackboardFloat");	// faintRecovery -= dt
	xB.ParamString(uRecTick, "m_strVar", "faintRecovery");
	xB.ParamEnum(uRecTick, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamString(uRecTick, "m_strOperandVar", "dt");
	const u_int uRecDone = xB.CompareFloat("faintRecovery", GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 0.0f, "faintExpired");
	const u_int uRecGate = xB.Gate("faintExpired");
	const u_int uRecClamp = xB.SetBlackboardFloat("faintRecovery", 0.0f);
	const u_int uToIdle = xB.SetBlackboardInt("state", 0);
	xB.Edge(uBrWake, 1, uRecTick);
	xB.Chain(uRecTick, uRecDone).Chain(uRecDone, uRecGate)
		.Chain(uRecGate, uRecClamp).Chain(uRecClamp, uToIdle);
	// Dead (pin 3): terminal, unwired.
}

static void BuildDPVillager_MovementModes(Zenith_EngineGraphBuilder& xB)
{
	// T2: movement-mode booleans (sprint wins Shift+Ctrl ties).
	Zenith_GraphChain xTick = xB.OnCustomEvent("VillagerTick", "dt");
	// POST-transition possession fact. Also the shim's movement-systems
	// gate: computed BEFORE the life-drain chain can Kill(), it matches
	// the retired mid-OnUpdate m_bIsPossessed sync (movement still runs
	// once on a burn-out death frame).
	const u_int uIsPoss = xB.CompareInt("state", GRAPH_COMPARE_INT_OP_EQUAL, 1, "stateIsPossessed");
	xTick.Then(uIsPoss);
	const u_int uBrPoss = xB.Branch("stateIsPossessed");
	xB.Chain(uIsPoss, uBrPoss);

	const u_int uBrMoving = xB.Branch("moving");
	xB.Edge(uBrPoss, 0, uBrMoving);

	const u_int uBrSprint = xB.Branch("sprintHeld");
	xB.Edge(uBrMoving, 0, uBrSprint);

	const u_int uSprOn = xB.SetBlackboardBool("sprinting", true);
	const u_int uSprQuietOff = xB.SetBlackboardBool("walkQuiet", false);
	const u_int uPingSprint = xB.Node("DPVillagerTutorialPing");
	xB.ParamInt(uPingSprint, "m_iKind", static_cast<int32_t>(DP_Tutorial::Kind::FirstSprintUse));
	xB.Edge(uBrSprint, 0, uSprOn);
	xB.Chain(uSprOn, uSprQuietOff).Chain(uSprQuietOff, uPingSprint);

	const u_int uSprOff = xB.SetBlackboardBool("sprinting", false);
	const u_int uBrQuiet = xB.Branch("quietHeld");
	xB.Edge(uBrSprint, 1, uSprOff);
	xB.Chain(uSprOff, uBrQuiet);
	const u_int uQuietOn = xB.SetBlackboardBool("walkQuiet", true);
	const u_int uPingQuiet = xB.Node("DPVillagerTutorialPing");
	xB.ParamInt(uPingQuiet, "m_iKind", static_cast<int32_t>(DP_Tutorial::Kind::FirstWalkQuietUse));
	xB.Edge(uBrQuiet, 0, uQuietOn);
	xB.Chain(uQuietOn, uPingQuiet);
	const u_int uQuietOff = xB.SetBlackboardBool("walkQuiet", false);
	xB.Edge(uBrQuiet, 1, uQuietOff);

	// Not moving / not possessed: both booleans off (two small tails -
	// chains can't rejoin; each Branch false-path gets its own pair).
	const u_int uIdleSprOff = xB.SetBlackboardBool("sprinting", false);
	const u_int uIdleQuietOff = xB.SetBlackboardBool("walkQuiet", false);
	xB.Edge(uBrMoving, 1, uIdleSprOff);
	xB.Chain(uIdleSprOff, uIdleQuietOff);

	const u_int uUnpossSprOff = xB.SetBlackboardBool("sprinting", false);
	const u_int uUnpossQuietOff = xB.SetBlackboardBool("walkQuiet", false);
	xB.Edge(uBrPoss, 1, uUnpossSprOff);
	xB.Chain(uUnpossSprOff, uUnpossQuietOff);
}

static void BuildDPVillager_LifeDrain(Zenith_EngineGraphBuilder& xB)
{
	// T3: life drain -> Kill (MVP-1.7 sprint cost, MVP-1.3.5 death).
	{
		Zenith_GraphChain xTick = xB.OnCustomEvent("VillagerTick", "dt");
		const u_int uGate = xB.Gate("stateIsPossessed");
		xTick.Then(uGate);
		const u_int uDrainBase = xB.Node("MathBlackboardFloat");	// drain = dt
		xB.ParamString(uDrainBase, "m_strVar", "dt");
		xB.ParamEnum(uDrainBase, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
		xB.ParamFloat(uDrainBase, "m_fOperand", 0.0f);
		xB.ParamString(uDrainBase, "m_strResultVar", "drain");
		xB.Chain(uGate, uDrainBase);
		const u_int uBrSprint = xB.Branch("sprinting");
		xB.Chain(uDrainBase, uBrSprint);
		// Sprinting: drain += sprintCostExtra * dt (the OnAwake-baked scale).
		const u_int uExtra = xB.Node("MathBlackboardFloat");	// extra = dt * sprintCostExtra
		xB.ParamString(uExtra, "m_strVar", "dt");
		xB.ParamEnum(uExtra, "m_iOp", GRAPH_MATH_FLOAT_OP_MULTIPLY);
		xB.ParamString(uExtra, "m_strOperandVar", "sprintCostExtra");
		xB.ParamString(uExtra, "m_strResultVar", "extra");
		const u_int uAddExtra = xB.Node("AddBlackboardFloat");
		xB.ParamString(uAddExtra, "m_strVariable", "drain");
		xB.ParamString(uAddExtra, "m_strDeltaVar", "extra");
		const u_int uFireSprint = xB.FireCustomEvent("VillagerApplyDrain");	// chain-reuse: apply at self
		xB.Edge(uBrSprint, 0, uExtra);
		xB.Chain(uExtra, uAddExtra).Chain(uAddExtra, uFireSprint);
		const u_int uFirePlain = xB.FireCustomEvent("VillagerApplyDrain");
		xB.Edge(uBrSprint, 1, uFirePlain);
	}
	{
		// Shared drain-apply tail (fired synchronously from T3's two paths).
		Zenith_GraphChain xApplyTick = xB.OnCustomEvent("VillagerApplyDrain");
		const u_int uApply = xB.Node("MathBlackboardFloat");	// remainingLife -= drain
		xB.ParamString(uApply, "m_strVar", "remainingLife");
		xB.ParamEnum(uApply, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
		xB.ParamString(uApply, "m_strOperandVar", "drain");
		const u_int uDepleted = xB.CompareFloat("remainingLife", GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 0.0f, "lifeDepleted");
		const u_int uGateDead = xB.Gate("lifeDepleted");
		const u_int uKill = xB.Node("DPVillagerKill");
		xApplyTick.Then(uApply).Then(uDepleted).Then(uGateDead).Then(uKill);
	}
}

static void BuildDPVillager_Footsteps(Zenith_EngineGraphBuilder& xB)
{
	// T4: footstep cadence (MVP-1.7.5).
	Zenith_GraphChain xTick = xB.OnCustomEvent("VillagerTick", "dt");
	const u_int uGate = xB.Gate("stateIsPossessed");
	xTick.Then(uGate);
	const u_int uBrMoving = xB.Branch("moving");
	xB.Chain(uGate, uBrMoving);
	// Moving: countdown -= dt; on expiry reset to the interval + emit.
	const u_int uCdTick = xB.Node("MathBlackboardFloat");
	xB.ParamString(uCdTick, "m_strVar", "footstepCountdown");
	xB.ParamEnum(uCdTick, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamString(uCdTick, "m_strOperandVar", "dt");
	const u_int uDue = xB.CompareFloat("footstepCountdown", GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 0.0f, "stepDue");
	const u_int uGateDue = xB.Gate("stepDue");
	const u_int uReset = xB.Node("MathBlackboardFloat");	// countdown = interval (exact reset, no overshoot carry)
	xB.ParamString(uReset, "m_strVar", "footstepInterval");
	xB.ParamEnum(uReset, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamFloat(uReset, "m_fOperand", 0.0f);
	xB.ParamString(uReset, "m_strResultVar", "footstepCountdown");
	const u_int uEmit = xB.Node("DPVillagerEmitFootstep");
	xB.Edge(uBrMoving, 0, uCdTick);
	xB.Chain(uCdTick, uDue).Chain(uDue, uGateDue)
		.Chain(uGateDue, uReset).Chain(uReset, uEmit);
	// Idle: hold at zero so the first step after movement is immediate.
	const u_int uCdZero = xB.SetBlackboardFloat("footstepCountdown", 0.0f);
	xB.Edge(uBrMoving, 1, uCdZero);
}

// BuildGraph_DPVillager - the W3 villager decisions graph (programmatic
// builder, the W1+ authoring path). Driven by "VillagerTick" custom events
// fired from DPVillager_Component::OnUpdate with dt as the payload (stashed
// to "dt"); the shim stages possessedNow/moving/sprintHeld/quietHeld before
// each fire and seeds maxLife + the cached movement tuning at OnAwake.
static void BuildGraph_DPVillager(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xI0; xI0.SetInt32(0);
	Zenith_PropertyValue xBF; xBF.SetBool(false);

	// Lifecycle state (MVP-1.4.1-3) + decision outputs the shim/tests read.
	xB.Variable("state", xI0);				// DPVillagerState as int
	xB.Variable("remainingLife", xF0);
	xB.Variable("maxLife", xF0);
	xB.Variable("faintRecovery", xF0);
	xB.Variable("sprinting", xBF);
	xB.Variable("walkQuiet", xBF);
	xB.Variable("stateIsPossessed", xBF);
	xB.Variable("footstepCountdown", xF0);
	// Per-frame staged facts (shim-written before each VillagerTick).
	xB.Variable("possessedNow", xBF);
	xB.Variable("moving", xBF);
	xB.Variable("sprintHeld", xBF);
	xB.Variable("quietHeld", xBF);
	// OnAwake-seeded tuning mirror (sprint cost keeps the MetaSave bake).
	xB.Variable("sprintCostExtra", xF0);
	xB.Variable("footstepInterval", xF0);
	xB.Variable("footstepLoudness", xF0);
	xB.Variable("footstepRadius", xF0);
	xB.Variable("quietLoudnessMult", xF0);

	BuildDPVillager_PossessionTransitions(xB);
	BuildDPVillager_MovementModes(xB);
	BuildDPVillager_LifeDrain(xB);
	BuildDPVillager_Footsteps(xB);
}

// BuildGraph_DPForge - forge craft decisions (W3). Driven by "Interact"
// (packed villager payload) fired by the DPForge shim's HandleInteract /
// CraftForTest. Recipe tags + craft count live on the blackboard so
// SetRecipe/GetCraftCount stay the component's public surface.
static void BuildGraph_DPForge(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xInput; xInput.SetInt32((int32_t)DP_ItemTag::Iron);
	Zenith_PropertyValue xOutput; xOutput.SetInt32((int32_t)DP_ItemTag::Key);
	Zenith_PropertyValue xZero; xZero.SetInt32(0);
	Zenith_EngineGraphBuilder xB(xBuilder);
	xB.Variable("recipeInput", xInput);
	xB.Variable("recipeOutput", xOutput);
	xB.Variable("craftCount", xZero);

	Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
	const u_int uCraft = xB.Node("DPForgeCraft");
	xInteract.Then(uCraft);
}

// BuildGraph_DPPlayerControl - input-dispatch decisions (W3). The controller
// stages clickPressed/dropPressed and fires "PlayerTick" once per frame at
// the retired handlers' call site; chains run in node order (click before
// drop, like the old HandleClickToPossess -> HandleDropItem sequence).
static void BuildGraph_DPPlayerControl(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xFalse; xFalse.SetBool(false);
	xB.Variable("clickPressed", xFalse);
	xB.Variable("dropPressed", xFalse);

	{
		Zenith_GraphChain xTick = xB.OnCustomEvent("PlayerTick");
		const u_int uGate = xB.Gate("clickPressed");
		const u_int uPick = xB.Node("DPPickVillagerUnderCursor");
		const u_int uPossess = xB.Node("DPTryPossess");
		xTick.Then(uGate).Then(uPick).Then(uPossess);
	}
	{
		Zenith_GraphChain xTick = xB.OnCustomEvent("PlayerTick");
		const u_int uGate = xB.Gate("dropPressed");
		const u_int uDrop = xB.Node("DPDropHeldItem");
		xTick.Then(uGate).Then(uDrop);
	}
}

// BuildGraph_DPPauseMenu - pause decisions (W3, risk R6). The shim stages
// escPressed/rPressed/qPressed and fires "PauseKeys" each frame; the graph
// reproduces the retired OnUpdate's early-return structure exactly:
//   if (shown || runOver) { R -> restart STOP; Q -> quit STOP; }
//   if (esc && overlay exists) { flip shown; apply. }
// Chain-reuse ("PauseRQ"/"PauseEsc" fired at self) stands in for the
// fall-throughs, since exec chains cannot rejoin.
static void BuildGraph_DPPauseMenu(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xFalse; xFalse.SetBool(false);
	xB.Variable("shown", xFalse);
	xB.Variable("runOver", xFalse);
	xB.Variable("escPressed", xFalse);
	xB.Variable("rPressed", xFalse);
	xB.Variable("qPressed", xFalse);

	// C1: route by (shown || runOver).
	{
		Zenith_GraphChain xTick = xB.OnCustomEvent("PauseKeys");
		const u_int uBrShown = xB.Branch("shown");
		xTick.Then(uBrShown);
		const u_int uFireRQ1 = xB.FireCustomEvent("PauseRQ");
		xB.Edge(uBrShown, 0, uFireRQ1);
		const u_int uBrOver = xB.Branch("runOver");
		xB.Edge(uBrShown, 1, uBrOver);
		const u_int uFireRQ2 = xB.FireCustomEvent("PauseRQ");
		xB.Edge(uBrOver, 0, uFireRQ2);
		const u_int uFireEsc1 = xB.FireCustomEvent("PauseEsc");
		xB.Edge(uBrOver, 1, uFireEsc1);
	}

	// C2: R before Q; neither pressed falls through to the Esc block
	// (the retired code only early-returned when a shortcut actually fired).
	{
		Zenith_GraphChain xRQ = xB.OnCustomEvent("PauseRQ");
		const u_int uBrR = xB.Branch("rPressed");
		xRQ.Then(uBrR);
		const u_int uRestart = xB.Node("DPPauseRestart");
		xB.Edge(uBrR, 0, uRestart);	// chain ends: the early return
		const u_int uBrQ = xB.Branch("qPressed");
		xB.Edge(uBrR, 1, uBrQ);
		const u_int uQuit = xB.Node("DPPauseQuit");
		xB.Edge(uBrQ, 0, uQuit);		// chain ends: the early return
		const u_int uFireEsc2 = xB.FireCustomEvent("PauseEsc");
		xB.Edge(uBrQ, 1, uFireEsc2);
	}

	// C3: the Esc toggle. Overlay-missing quirk gated by DPPauseCanToggle;
	// flip "shown" FIRST, then apply (visibility + scene pause + event).
	{
		Zenith_GraphChain xEsc = xB.OnCustomEvent("PauseEsc");
		const u_int uGate = xB.Gate("escPressed");
		const u_int uCan = xB.Node("DPPauseCanToggle");
		const u_int uBrFlip = xB.Branch("shown");
		xEsc.Then(uGate).Then(uCan).Then(uBrFlip);
		const u_int uHide = xB.SetBlackboardBool("shown", false);
		const u_int uApplyHide = xB.Node("DPPauseApplyToggle");
		xB.Edge(uBrFlip, 0, uHide);
		xB.Chain(uHide, uApplyHide);
		const u_int uShow = xB.SetBlackboardBool("shown", true);
		const u_int uApplyShow = xB.Node("DPPauseApplyToggle");
		xB.Edge(uBrFlip, 1, uShow);
		xB.Chain(uShow, uApplyShow);
	}
}

// ---- DP_Item pickup decision, split (Phase 3) into per-stage sub-builders.
// Each stage is self-contained (they hand off via ItemGate2/ItemGate3/
// ItemCommit custom events, sharing no node ids); called in original order
// they preserve node-creation + edge-add order -> byte-identical authoring.

static void BuildDPItem_Evaporate(Zenith_EngineGraphBuilder& xB)
{
	// T1: evaporate countdown (BEFORE the cooldown - the timer ticks
	// through the cooldown window; only the zero-crossing branch returns).
	Zenith_GraphChain xTick = xB.OnCustomEvent("ItemTick", "dt");
	const u_int uArmed = xB.CompareFloat("evaporateRemaining", GRAPH_COMPARE_FLOAT_OP_GREATER, 0.0f, "evapArmed");
	const u_int uBrArmed = xB.Branch("evapArmed");
	xTick.Then(uArmed).Then(uBrArmed);
	const u_int uTickDown = xB.Node("MathBlackboardFloat");
	xB.ParamString(uTickDown, "m_strVar", "evaporateRemaining");
	xB.ParamEnum(uTickDown, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamString(uTickDown, "m_strOperandVar", "dt");
	const u_int uDone = xB.CompareFloat("evaporateRemaining", GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 0.0f, "evapDone");
	const u_int uBrDone = xB.Branch("evapDone");
	xB.Edge(uBrArmed, 0, uTickDown);
	xB.Chain(uTickDown, uDone).Chain(uDone, uBrDone);
	const u_int uClamp = xB.SetBlackboardFloat("evaporateRemaining", 0.0f);
	const u_int uEvaporate = xB.Node("DPItemEvaporate");
	xB.Edge(uBrDone, 0, uClamp);
	xB.Chain(uClamp, uEvaporate);	// chain ends: the retired `return`
	const u_int uNext1 = xB.FireCustomEvent("ItemGate2");
	xB.Edge(uBrDone, 1, uNext1);
	const u_int uNext2 = xB.FireCustomEvent("ItemGate2");
	xB.Edge(uBrArmed, 1, uNext2);
}

static void BuildDPItem_Cooldown(Zenith_EngineGraphBuilder& xB)
{
	// T2: post-drop cooldown - while armed it decrements, clamps and
	// RETURNS (blocks all pickup logic that frame).
	Zenith_GraphChain xGate2 = xB.OnCustomEvent("ItemGate2");
	const u_int uArmed = xB.CompareFloat("postDropCooldown", GRAPH_COMPARE_FLOAT_OP_GREATER, 0.0f, "cdArmed");
	const u_int uBrArmed = xB.Branch("cdArmed");
	xGate2.Then(uArmed).Then(uBrArmed);
	const u_int uTickDown = xB.Node("MathBlackboardFloat");
	xB.ParamString(uTickDown, "m_strVar", "postDropCooldown");
	xB.ParamEnum(uTickDown, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamString(uTickDown, "m_strOperandVar", "dt");
	const u_int uUnder = xB.CompareFloat("postDropCooldown", GRAPH_COMPARE_FLOAT_OP_LESS, 0.0f, "cdUnder");
	const u_int uGateUnder = xB.Gate("cdUnder");
	const u_int uClamp = xB.SetBlackboardFloat("postDropCooldown", 0.0f);
	xB.Edge(uBrArmed, 0, uTickDown);
	xB.Chain(uTickDown, uUnder).Chain(uUnder, uGateUnder).Chain(uGateUnder, uClamp);
	const u_int uNext = xB.FireCustomEvent("ItemGate3");
	xB.Edge(uBrArmed, 1, uNext);
}

static void BuildDPItem_GatesChannel(Zenith_EngineGraphBuilder& xB)
{
	// T3: possessed/held/range/child gates + the reagent channel state
	// machine. Out-of-range simply ends the chain - the channel state is
	// left FROZEN, not reset (the pause-not-reset quirk).
	Zenith_GraphChain xGate3 = xB.OnCustomEvent("ItemGate3");
	const u_int uPossessed = xB.Gate("possessedValid");
	const u_int uEmpty = xB.Gate("handsEmpty");
	const u_int uRange = xB.Gate("inRange");
	const u_int uChild = xB.Node("DPItemChildRefusal");
	xGate3.Then(uPossessed).Then(uEmpty).Then(uRange).Then(uChild);
	const u_int uHasChannel = xB.CompareFloat("channelDuration", GRAPH_COMPARE_FLOAT_OP_GREATER, 0.0f, "hasChannel");
	const u_int uBrChannel = xB.Branch("hasChannel");
	xB.Chain(uChild, uHasChannel).Chain(uHasChannel, uBrChannel);
	// No channel (tools/objectives): commit immediately.
	const u_int uCommitNow = xB.FireCustomEvent("ItemCommit");
	xB.Edge(uBrChannel, 1, uCommitNow);
	// Channel: same-villager continuity by index+generation.
	const u_int uSame = xB.Node("CompareBlackboardEntity");
	xB.ParamString(uSame, "m_strVarA", "channelVillager");
	xB.ParamString(uSame, "m_strVarB", "possessedVillager");
	xB.ParamEnum(uSame, "m_iOp", GRAPH_ENTITY_COMPARE_OP_EQUAL);
	xB.ParamString(uSame, "m_strResultVar", "sameVillager");
	const u_int uBrSame = xB.Branch("sameVillager");
	xB.Edge(uBrChannel, 0, uSame);
	xB.Chain(uSame, uBrSame);
	// Different (or no) channeler: fresh arm; the arming frame is free.
	const u_int uArm = xB.Node("DPItemArmChannel");
	xB.Edge(uBrSame, 1, uArm);
	// Same villager: tick down; complete falls through to the commit.
	const u_int uTickDown = xB.Node("MathBlackboardFloat");
	xB.ParamString(uTickDown, "m_strVar", "channelRemaining");
	xB.ParamEnum(uTickDown, "m_iOp", GRAPH_MATH_FLOAT_OP_SUBTRACT);
	xB.ParamString(uTickDown, "m_strOperandVar", "dt");
	const u_int uStill = xB.CompareFloat("channelRemaining", GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 0.0f, "channelDone");
	const u_int uGateDone = xB.Gate("channelDone");
	const u_int uCommitChan = xB.FireCustomEvent("ItemCommit");
	xB.Edge(uBrSame, 0, uTickDown);
	xB.Chain(uTickDown, uStill).Chain(uStill, uGateDone).Chain(uGateDone, uCommitChan);
}

static void BuildDPItem_Commit(Zenith_EngineGraphBuilder& xB)
{
	// T4: commit + BellSoul special (guarded inside the node).
	Zenith_GraphChain xCommit = xB.OnCustomEvent("ItemCommit");
	const u_int uPickup = xB.Node("DPItemCommitPickup");
	const u_int uBell = xB.Node("DPItemRingBell");
	xCommit.Then(uPickup).Then(uBell);
}

// BuildGraph_DPItem - the item pickup decision chain (W3). The DPItemBase
// shim stages possessedValid/handsEmpty/inRange/possessedVillager + the
// reagent config mirror, and fires "ItemTick" (dt payload). The retired
// OnUpdate's early-return structure maps to chain-reuse events: a chain that
// ENDS is a `return`; falling through fires the next stage at self. Stages
// split into BuildDPItem_* sub-builders, called in node-creation order.
static void BuildGraph_DPItem(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xI0; xI0.SetInt32(0);
	Zenith_PropertyValue xBF; xBF.SetBool(false);
	Zenith_PropertyValue xSE; xSE.SetString("");
	// Mutable countdown state.
	xB.Variable("postDropCooldown", xF0);
	xB.Variable("channelRemaining", xF0);
	xB.Variable("evaporateRemaining", xF0);
	// Config mirror (shim-written at reagent resolve).
	xB.Variable("channelDuration", xF0);
	xB.Variable("tag", xI0);
	xB.Variable("specialBehaviour", xSE);
	Zenith_PropertyValue xNoEntity; xNoEntity.SetPackedEntityID(0);
	xB.Variable("channelVillager", xNoEntity);
	// Per-frame staged facts.
	xB.Variable("possessedValid", xBF);
	xB.Variable("handsEmpty", xBF);
	xB.Variable("inRange", xBF);

	BuildDPItem_Evaporate(xB);
	BuildDPItem_Cooldown(xB);
	BuildDPItem_GatesChannel(xB);
	BuildDPItem_Commit(xB);
}

// BuildGraph_DPPriest - the priest decision body (W3, risk R3): a REACTIVE
// Selector re-scanning apprehend > pursue > investigate > patrol every tick
// (subsumes the retired memory-selector + m_xTree.Reset() hack; the accepted
// deltas - channel may start at <=2.0m XZ mid-pursue, investigate can preempt
// patrol mid-branch - are gated by the priest characterizations + the seed
// matrix). Driven by "PriestTick" fired from Priest_Component::OnUpdate AFTER
// the C++ perception bridge writes this blackboard; the suspended Selector
// chain is ALSO re-driven by the engine ON_UPDATE dispatch each frame (real
// dt - that drive advances the timers; the PriestTick drive settles the
// decisions on fresh bridge data with dt 0).
static void BuildGraph_DPPriest(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xBF; xBF.SetBool(false);
	Zenith_PropertyValue xV0; xV0.SetVector3(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	Zenith_PropertyValue xNoEntity; xNoEntity.SetPackedEntityID(INVALID_ENTITY_ID.GetPacked());
	Zenith_PropertyValue xRadius; xRadius.SetFloat(15.0f);
	xB.Variable(DP_AI::BB_KEY_TARGET_WITH_DEVIL, xNoEntity);
	xB.Variable(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, xBF);
	xB.Variable(DP_AI::BB_KEY_INVESTIGATE_POS, xV0);
	xB.Variable(DP_AI::BB_KEY_PATROL_TARGET, xV0);
	xB.Variable(DP_AI::BB_KEY_SUSPICION_RADIUS, xRadius);
	xB.Variable(DP_AI::BB_KEY_HIGH_SCENT_TARGET, xNoEntity);

	Zenith_GraphChain xTick = xB.OnCustomEvent("PriestTick", "dt");
	const u_int uSelector = xB.Node("Selector");
	xB.ParamInt(uSelector, "m_iBranchCount", 4);
	// BT-parity preemption: the four branches drive ONE shared nav agent, so
	// a preemption abort (NavMoveTo::OnAbort -> Stop) would clobber the
	// preempting branch's SetDestination (the abort runs AFTER the new pin
	// executed) and ping-pong the selector. The retired BT never aborted
	// preempted branches either (composite OnAbort was a no-op) - stale
	// branch state resuming later is the DOCUMENTED old behaviour.
	xB.ParamBool(uSelector, "m_bAbortPreempted", false);
	xTick.Then(uSelector);

	// Pin 0 - apprehend: the channel node self-gates on target validity +
	// XZ range (its FAILURE falls the Selector through to pursue).
	const u_int uApprehend = xB.Node("DPPriestApprehendChannel");
	xB.Edge(uSelector, 0, uApprehend);

	// Pin 1 - pursue: HasTarget gate + the BT MoveToEntity parity mover
	// (acceptance 1.5 FULL 3D, repath 0.4s, live target var).
	const u_int uHasTarget = xB.Node("QueryEntityValid");
	xB.ParamString(uHasTarget, "m_strEntityVar", DP_AI::BB_KEY_TARGET_WITH_DEVIL);
	xB.ParamString(uHasTarget, "m_strResultVar", "hasDevilTarget");
	const u_int uGateTarget = xB.Gate("hasDevilTarget");
	const u_int uPursue = xB.Node("NavMoveTo");
	xB.ParamString(uPursue, "m_strDestinationVar", DP_AI::BB_KEY_TARGET_WITH_DEVIL);
	xB.ParamFloat(uPursue, "m_fAcceptanceRadius", 1.5f);
	xB.ParamFloat(uPursue, "m_fRepathInterval", 0.4f);
	xB.ParamBool(uPursue, "m_bXZDistance", false);
	xB.Edge(uSelector, 1, uHasTarget);
	xB.Chain(uHasTarget, uGateTarget).Chain(uGateTarget, uPursue);

	// Pin 2 - investigate: walk to the heard position, linger 2s, clear the
	// flag (the bridge re-arms it while the heard sound stays fresh).
	const u_int uGateInvestigate = xB.Gate(DP_AI::BB_KEY_HAS_INVESTIGATE_POS);
	const u_int uWalkNoise = xB.Node("NavMoveTo");
	xB.ParamString(uWalkNoise, "m_strDestinationVar", DP_AI::BB_KEY_INVESTIGATE_POS);
	xB.ParamFloat(uWalkNoise, "m_fAcceptanceRadius", 1.0f);
	xB.ParamFloat(uWalkNoise, "m_fRepathInterval", 60.0f);	// single-shot walk (BT MoveTo never repathed)
	xB.ParamBool(uWalkNoise, "m_bXZDistance", false);
	const u_int uLinger = xB.Node("Wait");
	xB.ParamFloat(uLinger, "m_fSeconds", 2.0f);
	const u_int uClearInvestigate = xB.SetBlackboardBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
	xB.Edge(uSelector, 2, uGateInvestigate);
	xB.Chain(uGateInvestigate, uWalkNoise).Chain(uWalkNoise, uLinger)
		.Chain(uLinger, uClearInvestigate);

	// Pin 3 - patrol: pick (scent-biased, 1-in-5 patrol-node cadence), walk,
	// pause 1s.
	const u_int uPick = xB.Node("DPPriestPickPatrolTarget");
	const u_int uWalkPatrol = xB.Node("NavMoveTo");
	xB.ParamString(uWalkPatrol, "m_strDestinationVar", DP_AI::BB_KEY_PATROL_TARGET);
	xB.ParamFloat(uWalkPatrol, "m_fAcceptanceRadius", 1.0f);
	xB.ParamFloat(uWalkPatrol, "m_fRepathInterval", 60.0f);
	xB.ParamBool(uWalkPatrol, "m_bXZDistance", false);
	const u_int uPause = xB.Node("Wait");
	xB.ParamFloat(uPause, "m_fSeconds", 1.0f);
	xB.Edge(uSelector, 3, uPick);
	xB.Chain(uPick, uWalkPatrol).Chain(uWalkPatrol, uPause);
}

// ---- The 6 wave-1/2 graphs, re-authored decomposed through the builder ----
// Same asset paths, same blackboard variable names (isOpen/openT/anim/
// requiredKey - tests + the DPDoor shim accessors read them), same event
// wiring; the retired mega-nodes (DPDepositHeldObjective / DPTryOpenDoor /
// DPOpenChest / DPAdvanceChestLid / DPDoorHandleInteract) are DELETED and
// their decision steps are engine nodes + single-action DP nodes.

static void BuildGraph_DPMainMenu(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	{
		Zenith_GraphChain xPlay = xB.OnCustomEvent("MenuPlay");
		const u_int uLoad = xB.Node("LoadSceneByIndex");
		xB.ParamInt(uLoad, "m_iSceneIndex", 1);
		xPlay.Then(uLoad);
	}
	{
		Zenith_GraphChain xQuit = xB.OnCustomEvent("MenuQuit");
		const u_int uRequest = xB.Node("DPRequestQuit");
		xQuit.Then(uRequest);
	}
	{
		// Metagame v1: Liminal (hermit shrines) entry — third menu button.
		Zenith_GraphChain xLiminal = xB.OnCustomEvent("MenuLiminal");
		const u_int uLoad = xB.Node("LoadSceneByIndex");
		xB.ParamInt(uLoad, "m_iSceneIndex", 2);
		xLiminal.Then(uLoad);
	}
}

static void BuildGraph_DPPentagram(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_GraphChain xStart = xB.OnStart();
	const u_int uReset = xB.Node("DPResetWinState");
	xStart.Then(uReset);

	// The deposit chain preserves the retired ordering invariants: gates
	// first (zero side effects on refusal), NOTIFY before consume (victory
	// can fire while the villager still holds the item), the placed event
	// strictly after the item destroy.
	Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
	const u_int uRead = xB.Node("DPReadHeldObjective");
	const u_int uCheck = xB.Node("DPWinCheckAlreadyCollected");
	const u_int uNotify = xB.Node("DPWinNotifyCollected");
	const u_int uConsume = xB.Node("DPConsumeHeldItem");
	const u_int uPlaced = xB.Node("DPDispatchObjectivePlaced");
	xInteract.Then(uRead).Then(uCheck).Then(uNotify).Then(uConsume).Then(uPlaced);
}

static void BuildGraph_DPChest(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xBF; xBF.SetBool(false);
	xB.Variable("isOpen", xBF);
	xB.Variable("openT", xF0);

	// Interact: already-open presses do nothing (unwired true pin); the
	// chest opens even on an invalid payload (quirk preserved - no gate).
	{
		Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
		const u_int uBrOpen = xB.Branch("isOpen");
		xInteract.Then(uBrOpen);
		const u_int uOpen = xB.SetBlackboardBool("isOpen", true);
		const u_int uEvent = xB.Node("DPDispatchChestOpened");
		xB.Edge(uBrOpen, 1, uOpen);
		xB.Chain(uOpen, uEvent);
	}

	// Per-frame lid progress: openT += dt / duration, clamped to 1, duration
	// read LIVE via the tuning stage node (the retired per-Execute read).
	{
		Zenith_GraphChain xUpdate = xB.OnUpdate();
		const u_int uGateOpen = xB.Gate("isOpen");
		xUpdate.Then(uGateOpen);
		const u_int uLidDone = xB.CompareFloat("openT", GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL, 1.0f, "lidDone");
		const u_int uBrDone = xB.Branch("lidDone");
		xB.Chain(uGateOpen, uLidDone).Chain(uLidDone, uBrDone);
		const u_int uDuration = xB.Node("DPReadTuningFloat");
		xB.ParamString(uDuration, "m_strKey", "interactables.chest_open_duration_s");
		xB.ParamString(uDuration, "m_strVar", "lidDuration");
		const u_int uStepReset = xB.SetBlackboardFloat("lidStep", 0.0f);
		const u_int uStepDt = xB.Node("AddBlackboardFloat");	// lidStep = dt
		xB.ParamString(uStepDt, "m_strVariable", "lidStep");
		xB.ParamFloat(uStepDt, "m_fDelta", 1.0f);
		xB.ParamBool(uStepDt, "m_bScaleByDt", true);
		const u_int uStepDiv = xB.Node("MathBlackboardFloat");	// lidStep /= duration
		xB.ParamString(uStepDiv, "m_strVar", "lidStep");
		xB.ParamEnum(uStepDiv, "m_iOp", GRAPH_MATH_FLOAT_OP_DIVIDE);
		xB.ParamString(uStepDiv, "m_strOperandVar", "lidDuration");
		const u_int uAdvance = xB.Node("AddBlackboardFloat");	// openT += lidStep
		xB.ParamString(uAdvance, "m_strVariable", "openT");
		xB.ParamString(uAdvance, "m_strDeltaVar", "lidStep");
		const u_int uClamp = xB.Node("MathBlackboardFloat");	// openT = min(openT, 1)
		xB.ParamString(uClamp, "m_strVar", "openT");
		xB.ParamEnum(uClamp, "m_iOp", GRAPH_MATH_FLOAT_OP_MIN);
		xB.ParamFloat(uClamp, "m_fOperand", 1.0f);
		xB.Edge(uBrDone, 1, uDuration);
		xB.Chain(uDuration, uStepReset).Chain(uStepReset, uStepDt)
			.Chain(uStepDt, uStepDiv).Chain(uStepDiv, uAdvance).Chain(uAdvance, uClamp);
	}
}

static void BuildGraph_DPNoiseMachine(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
	const u_int uNoise = xB.Node("DPEmitNoise");
	xInteract.Then(uNoise);
}

static void BuildGraph_DPDoubleDoor(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xBF; xBF.SetBool(false);
	xB.Variable("isOpen", xBF);
	xB.Variable("openT", xF0);

	// Interact: open-guard -> villager-valid gate -> key consume (hard-coded
	// Key, the retired semantics) -> flag -> event.
	{
		Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
		const u_int uBrOpen = xB.Branch("isOpen");
		xInteract.Then(uBrOpen);
		const u_int uValid = xB.Node("QueryEntityValid");
		xB.ParamString(uValid, "m_strEntityVar", "payload");
		xB.ParamString(uValid, "m_strResultVar", "payloadValid");
		const u_int uGateValid = xB.Gate("payloadValid");
		const u_int uKey = xB.Node("DPConsumeKeyForUnlock");
		const u_int uOpen = xB.SetBlackboardBool("isOpen", true);
		const u_int uEvent = xB.Node("DPDispatchDoorOpened");
		xB.Edge(uBrOpen, 1, uValid);
		xB.Chain(uValid, uGateValid).Chain(uGateValid, uKey)
			.Chain(uKey, uOpen).Chain(uOpen, uEvent);
	}
	{
		Zenith_GraphChain xUpdate = xB.OnUpdate();
		const u_int uLeaves = xB.Node("DPAnimateDoorLeaves");
		xUpdate.Then(uLeaves);
	}
}

static void BuildGraph_DPDoor(Zenith_GraphBuilder& xBuilder)
{
	// anim is the DoorAnim int (0=Closed 1=Opening 2=Open 3=Closing);
	// requiredKey is the DP_ItemTag int seeded per-door by the bootstrap
	// AFTER graph attach. Ordering invariants preserved by chain order:
	// key check before any state change; the anim write BEFORE
	// DPDoorStateChanged (it reads GetAnim() off this blackboard); state
	// change before noise before event dispatch. Opening/Closing pins are
	// unwired - mid-animation F-presses are ignored.
	Zenith_EngineGraphBuilder xB(xBuilder);
	Zenith_PropertyValue xF0; xF0.SetFloat(0.0f);
	Zenith_PropertyValue xI0; xI0.SetInt32(0);
	xB.Variable("anim", xI0);
	xB.Variable("openT", xF0);
	xB.Variable("requiredKey", xI0);

	{
		Zenith_GraphChain xInteract = xB.OnCustomEvent("Interact");
		const u_int uValid = xB.Node("QueryEntityValid");
		xB.ParamString(uValid, "m_strEntityVar", "payload");
		xB.ParamString(uValid, "m_strResultVar", "payloadValid");
		const u_int uGateValid = xB.Gate("payloadValid");
		const u_int uSwitch = xB.SwitchOnInt("anim", 4);
		xInteract.Then(uValid).Then(uGateValid).Then(uSwitch);

		// Closed -> Opening.
		const u_int uCheckKey = xB.Node("DPDoorCheckKey");
		const u_int uToOpening = xB.SetBlackboardInt("anim", 1);
		const u_int uSyncOpen = xB.Node("DPDoorStateChanged");
		const u_int uNoiseOpen = xB.Node("DPDoorEmitNoise");
		const u_int uEventOpen = xB.Node("DPDispatchDoorOpened");
		xB.Edge(uSwitch, 0, uCheckKey);
		xB.Chain(uCheckKey, uToOpening).Chain(uToOpening, uSyncOpen)
			.Chain(uSyncOpen, uNoiseOpen).Chain(uNoiseOpen, uEventOpen);

		// Open -> Closing (with the pentagram close-deferral).
		const u_int uDeferral = xB.Node("DPDoorPentagramDeferral");
		const u_int uToClosing = xB.SetBlackboardInt("anim", 3);
		const u_int uSyncClose = xB.Node("DPDoorStateChanged");
		const u_int uNoiseClose = xB.Node("DPDoorEmitNoise");
		const u_int uEventClose = xB.Node("DPDispatchDoorClosed");
		xB.Edge(uSwitch, 2, uDeferral);
		xB.Chain(uDeferral, uToClosing).Chain(uToClosing, uSyncClose)
			.Chain(uSyncClose, uNoiseClose).Chain(uNoiseClose, uEventClose);
	}
	{
		Zenith_GraphChain xUpdate = xB.OnUpdate();
		const u_int uAdvance = xB.Node("DPDoorAdvanceAnim");
		xUpdate.Then(uAdvance);
	}
}

static void AuthorBehaviourGraphs()
{
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// ---- Villager decisions (W3): programmatic builder ---------------------
	xAuto.AddStep_GraphBuild(DPVillager_Component::kszGraphAsset, &BuildGraph_DPVillager);

	// ---- Item pickup decisions (W3) -----------------------------------------
	xAuto.AddStep_GraphBuild(DPItemBase_Component::kszGraphAsset, &BuildGraph_DPItem);

	// ---- Forge craft decisions (W3) ----------------------------------------
	xAuto.AddStep_GraphBuild(DPForge_Component::kszGraphAsset, &BuildGraph_DPForge);

	// ---- Player input dispatch (W3) ----------------------------------------
	xAuto.AddStep_GraphBuild(DPPlayerController_Component::kszGraphAsset, &BuildGraph_DPPlayerControl);

	// ---- Pause menu decisions (W3, R6) --------------------------------------
	xAuto.AddStep_GraphBuild(DPPauseMenuController_Component::kszGraphAsset, &BuildGraph_DPPauseMenu);

	// ---- Priest decision body (W3, R3) --------------------------------------
	xAuto.AddStep_GraphBuild(Priest_Component::kszGraphAsset, &BuildGraph_DPPriest);

	// ---- The 6 wave-1/2 graphs, re-authored DECOMPOSED (W3) -----------------
	xAuto.AddStep_GraphBuild("game:Graphs/DP_MainMenu.bgraph", &BuildGraph_DPMainMenu);
	xAuto.AddStep_GraphBuild("game:Graphs/DP_Pentagram.bgraph", &BuildGraph_DPPentagram);
	xAuto.AddStep_GraphBuild("game:Graphs/DP_Chest.bgraph", &BuildGraph_DPChest);
	xAuto.AddStep_GraphBuild("game:Graphs/DP_NoiseMachine.bgraph", &BuildGraph_DPNoiseMachine);
	xAuto.AddStep_GraphBuild("game:Graphs/DP_DoubleDoor.bgraph", &BuildGraph_DPDoubleDoor);
	xAuto.AddStep_GraphBuild("game:Graphs/DP_Door.bgraph", &BuildGraph_DPDoor);
}

void Project_RegisterEditorAutomationSteps()
{
	AuthorBehaviourGraphs();  // .bgraph assets (regenerated every boot)
	AuthorFrontEndScene();    // build index 0
	AuthorProcLevelScene();   // build index 1
	AuthorLiminalScene();     // build index 2 (metagame hub)

	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif // ZENITH_TOOLS

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd"  ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/ProcLevel" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Liminal"   ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
