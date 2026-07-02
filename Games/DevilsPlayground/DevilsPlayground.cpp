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
		// must be torn down. Matches the Marble/Combat template pattern.
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

			Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::Create<Zenith_Prefab>();
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
		// LIFETIME GOTCHA: AddStep_* stores its const char* args BY POINTER
		// until the boot drain executes the queue — every other caller in
		// this file passes string literals (immortal), but generated names
		// from a stack buffer DANGLE by drain time (all 36 buttons collapsed
		// into one garbage-named element before this used static storage).
		static std::string s_astrHeaderNames[3];
		static std::string s_astrNodeNames[3 * 12];
		static std::string s_astrNodeLabels[3 * 12];

		const char* aszTrackNames[3] = { "Wynstan's Forge", "Mereworth's Eye", "Old Bett's Breath" };
		const float afColumnX[3] = { -420.0f, 0.0f, 420.0f };
		for (uint32_t uTrack = 0; uTrack < 3; ++uTrack)
		{
			char szScratch[48];
			std::snprintf(szScratch, sizeof(szScratch), "LiminalTrack%u", uTrack);
			s_astrHeaderNames[uTrack] = szScratch;
			const char* szHeader = s_astrHeaderNames[uTrack].c_str();
			xAuto.AddStep_CreateUIText(szHeader, aszTrackNames[uTrack]);
			xAuto.AddStep_SetUIAnchor(szHeader, static_cast<int>(Zenith_UI::AnchorPreset::Center));
			xAuto.AddStep_SetUIAlignment(szHeader, static_cast<int>(Zenith_UI::TextAlignment::Center));
			xAuto.AddStep_SetUISize(szHeader, 320.0f, 32.0f);
			xAuto.AddStep_SetUIPosition(szHeader, afColumnX[uTrack], -255.0f);
			xAuto.AddStep_SetUIFontSize(szHeader, 26.0f);
			xAuto.AddStep_SetUIColor(szHeader, 0.9f, 0.85f, 0.7f, 1.0f);

			for (uint32_t uNode = 0; uNode < 12; ++uNode)
			{
				const uint32_t uIdx = uTrack * 12 + uNode;
				std::snprintf(szScratch, sizeof(szScratch), "LiminalNode_T%u_N%u", uTrack, uNode);
				s_astrNodeNames[uIdx] = szScratch;
				std::snprintf(szScratch, sizeof(szScratch), "Node %u  (%u Knots)", uNode + 1, 2u + uNode);
				s_astrNodeLabels[uIdx] = szScratch;
				const char* szName = s_astrNodeNames[uIdx].c_str();
				xAuto.AddStep_CreateUIButton(szName, s_astrNodeLabels[uIdx].c_str());
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
static void AuthorBehaviourGraphs()
{
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// ---- Front-end menu: Play -> load ProcLevel, Quit -> request quit ------
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_MainMenu.bgraph");
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "MenuPlay");
	xAuto.AddStep_GraphAddNode("LoadSceneByIndex");
	xAuto.AddStep_GraphSelectNode("LoadSceneByIndex", 0);
	xAuto.AddStep_GraphSetNodeParamInt("m_iSceneIndex", 1);
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "LoadSceneByIndex", 0);
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 1);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "MenuQuit");
	xAuto.AddStep_GraphAddNode("DPRequestQuit");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 1, 0, "DPRequestQuit", 0);
	// Metagame v1: Liminal (hermit shrines) entry — third menu button.
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 2);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "MenuLiminal");
	xAuto.AddStep_GraphAddNode("LoadSceneByIndex");
	xAuto.AddStep_GraphSelectNode("LoadSceneByIndex", 1);
	xAuto.AddStep_GraphSetNodeParamInt("m_iSceneIndex", 2);
	xAuto.AddStep_GraphConnect("OnCustomEvent", 2, 0, "LoadSceneByIndex", 1);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

	// ---- Pentagram: win-state reset on start; interact deposits objective --
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_Pentagram.bgraph");
	xAuto.AddStep_GraphAddNode("OnStart");
	xAuto.AddStep_GraphAddNode("DPResetWinState");
	xAuto.AddStep_GraphConnect("OnStart", 0, 0, "DPResetWinState", 0);
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "Interact");
	xAuto.AddStep_GraphAddNode("DPDepositHeldObjective");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "DPDepositHeldObjective", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

	// ---- Chest: interact opens; per-frame lid progress ---------------------
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_Chest.bgraph");
	xAuto.AddStep_GraphAddVariable("isOpen", "bool", 0.0f);
	xAuto.AddStep_GraphAddVariable("openT", "float", 0.0f);
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "Interact");
	xAuto.AddStep_GraphAddNode("DPOpenChest");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "DPOpenChest", 0);
	xAuto.AddStep_GraphAddNode("OnUpdate");
	xAuto.AddStep_GraphAddNode("DPAdvanceChestLid");
	xAuto.AddStep_GraphConnect("OnUpdate", 0, 0, "DPAdvanceChestLid", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

	// ---- Noise machine: interact emits the priest-bait stimulus ------------
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_NoiseMachine.bgraph");
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "Interact");
	xAuto.AddStep_GraphAddNode("DPEmitNoise");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "DPEmitNoise", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

	// ---- Double door: key-gated open; per-frame leaf animation -------------
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_DoubleDoor.bgraph");
	xAuto.AddStep_GraphAddVariable("isOpen", "bool", 0.0f);
	xAuto.AddStep_GraphAddVariable("openT", "float", 0.0f);
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "Interact");
	xAuto.AddStep_GraphAddNode("DPTryOpenDoor");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "DPTryOpenDoor", 0);
	xAuto.AddStep_GraphAddNode("OnUpdate");
	xAuto.AddStep_GraphAddNode("DPAnimateDoorLeaves");
	xAuto.AddStep_GraphConnect("OnUpdate", 0, 0, "DPAnimateDoorLeaves", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

	// ---- Single-leaf door: key-gated state machine + swing animation -------
	// (wave 2) Decisions live here (DPDoorHandleInteract / DPDoorAdvanceAnim);
	// systems execution (navmesh, collider, tint, rotation) stays on the
	// DPDoor_Component shim, called synchronously by the nodes. anim is the
	// DoorAnim int (0=Closed 1=Opening 2=Open 3=Closing); requiredKey is the
	// DP_ItemTag int seeded per-door by the bootstrap AFTER graph attach.
	xAuto.AddStep_GraphOpenFresh("game:Graphs/DP_Door.bgraph");
	xAuto.AddStep_GraphAddVariable("anim", "int", 0.0f);
	xAuto.AddStep_GraphAddVariable("openT", "float", 0.0f);
	xAuto.AddStep_GraphAddVariable("requiredKey", "int", 0.0f);
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "Interact");
	xAuto.AddStep_GraphAddNode("DPDoorHandleInteract");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "DPDoorHandleInteract", 0);
	xAuto.AddStep_GraphAddNode("OnUpdate");
	xAuto.AddStep_GraphAddNode("DPDoorAdvanceAnim");
	xAuto.AddStep_GraphConnect("OnUpdate", 0, 0, "DPDoorAdvanceAnim", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();
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
