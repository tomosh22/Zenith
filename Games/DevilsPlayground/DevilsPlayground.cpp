#include "Zenith.h"

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

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "UI/Zenith_UIRect.h"

// Behaviour headers — including each here forces the
// ZENITH_BEHAVIOUR_TYPE_NAME static-init registration to run, so the
// scene-load path can resolve `game:Scripts/<TypeName>.zscript` to a valid
// factory. Combat does the equivalent.
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPPlayerController_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPItemSpawn_Behaviour.h"
#include "Components/DPItemManager_Behaviour.h"
#include "Components/DPInteractable_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPDoubleDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPMainMenuController_Behaviour.h"
#include "Components/DPPauseMenuController_Behaviour.h"
#include "Components/DPFogPass_Behaviour.h"
#include "Components/DPProcLevelBootstrap_Behaviour.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#endif

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

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
	void InitializeResources()
	{
		// Load Config/Tuning.json into the in-process cache before any other
		// resource init so subsequent systems (materials, fog, behaviours)
		// can read tuning values during their own bring-up.
		DP_Tuning::Initialize();

		// MVP-0.2.1+2: Config/Archetypes.json into the archetype cache. Loaded
		// right after Tuning so DPVillager_Behaviour::OnAwake can consult both.
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
		// DPProcLevelBootstrap_Behaviour::OnAwake via
		// DP_Particles::EnsureEmittersInScene. Idempotent.
		DP_Particles::Initialize();

		// 2026-05-21: first-encounter tutorialisation. Subscribes to the
		// same DP events as the particles + telemetry, but uses them to
		// emit one-shot tips ("first time you pick up Iron, here's what
		// to do with it"). Tips display in the TutorialOverlay HUD
		// element for ~5 s. Shown-flag table is process-global; reset
		// per run via DP_Tutorial::ResetForNewRun.
		DP_Tutorial::Initialize();

#ifdef ZENITH_INPUT_SIMULATOR
		// Tell the automated-test harness how to wipe DP-specific persistent
		// globals between batched tests. The harness force-loads scene 0
		// before firing this hook, so entity-managed side-tables (DP_Items,
		// DP_Fog) are already cleared via OnDestroy by the time we run; we
		// only need to reset state that has no entity owner.
		Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
		{
			DP_Player::ResetForNewRun();
			DP_Win::Reset();
			DP_Fog::ClearAllFogHoles();
			DP_Fog::ClearAllMemoryReveals();
			DP_AI::ResetLevelNavMesh();
			DP_Night::Reset();
			DPPauseMenuController_Behaviour::ResetForTest();
			// Drop emitter entities + zero burst counts so the next test
			// gets a fresh particle slate. Configs + subscriptions stay
			// alive across tests (they're process-global).
			DP_Particles::ClearEmitterEntities();
			DP_Particles::ResetBurstCountsForTest();
			// Clear shown-flags so first-encounter tips fire for each
			// batched test independently. Subscriptions stay alive.
			DP_Tutorial::ResetForNewRun();
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

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
	// W0 stub. B6 may set fog technique here; the game disables engine fog
	// entirely via g_xEngine.Fog().SetExternallyOverridden(true) inside DPFogPass.
}

void Project_RegisterScriptBehaviours()
{
	// Behaviour registration is automatic via the ZENITH_BEHAVIOUR_TYPE_NAME
	// macro's static initializer (runs at program startup before main()).
	// This hook remains as the per-game lifecycle entry point for early
	// CPU-only resource initialization that must run in both TOOLS and
	// non-TOOLS builds.
	DevilsPlayground::InitializeResources();

	// Register the game's post-fog render-graph pass and disable the engine
	// fog system. Idempotent — safe across Editor Stop/Play.
	DPFogPass::Init();
}

void Project_Shutdown()
{
	// Order matters: tear down the render hook BEFORE engine resources go
	// away (g_xEngine.Fog().SetExternallyOverridden is guarded against the
	// render graph already being torn down).
	DPFogPass::Shutdown();
	DevilsPlayground::CleanupResources();
}

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All DevilsPlayground resources are initialised in
	// Project_RegisterScriptBehaviours via DevilsPlayground::InitializeResources().
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
			Zenith_EditorAutomation::AddStep_AddModel();
			Zenith_EditorAutomation::AddStep_LoadModel(szResolved);
			if (Zenith_MaterialAsset* pxDefault = DPMaterials::GetDefaultMaterial())
			{
				Zenith_EditorAutomation::AddStep_SetModelMaterial(0, pxDefault);
			}
		}
		if (bAddCollider)
		{
			Zenith_EditorAutomation::AddStep_AddCollider();
			Zenith_EditorAutomation::AddStep_AddColliderShape(eVolumeType, eBodyType);
		}
	}

	void AuthorFrontEndScene()
	{
		Zenith_EditorAutomation::AddStep_CreateScene("FrontEnd");

		Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
		Zenith_EditorAutomation::AddStep_AddCamera();
		Zenith_EditorAutomation::AddStep_SetCameraPosition(0.0f, 5.0f, -10.0f);
		Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.3f);
		Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(60.0f));
		Zenith_EditorAutomation::AddStep_SetAsMainCamera();

		Zenith_EditorAutomation::AddStep_AddUI();
		Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "DEVIL'S PLAYGROUND");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		// MVP-UI-polish: TextAlignment::Center is required for the text to
		// render symmetrically around the anchor point. Without it the
		// text flows left-aligned FROM the anchor and looks off-centre.
		Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.0f, -160.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", DPUI::fMENU_TITLE_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.9f, 0.2f, 0.2f, 1.0f);

		Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H);
		Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("MenuPlay", DPUI::fMENU_BTN_FONT);

		// MVP-2.5.6: main-menu Quit button. Sits below Play.
		Zenith_EditorAutomation::AddStep_CreateUIButton("MenuQuit", "Quit");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuQuit", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuQuit", 0.0f, DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING);
		Zenith_EditorAutomation::AddStep_SetUISize("MenuQuit", DPUI::fMENU_BTN_W, DPUI::fMENU_BTN_H);
		Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("MenuQuit", DPUI::fMENU_BTN_FONT);

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
		Zenith_EditorAutomation::AddStep_CreateUIText("MenuHowToTitle", "How to play");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuHowToTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuHowToTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUISize("MenuHowToTitle", 1000.0f, 50.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuHowToTitle", 0.0f, DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING + 102.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuHowToTitle", DPUI::fMENU_HOWTO_TITLE_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("MenuHowToTitle", 1.0f, 0.85f, 0.6f, 1.0f);

		Zenith_EditorAutomation::AddStep_CreateUIText("MenuHowToBody",
			"You are a demon. Possess villagers ([LMB]) and deliver 5 objectives\n"
			"from chests to the pentagram before dawn. Avoid the priest.\n"
			"\n"
			"[WASD] move    [Shift] sprint    [F] interact    [Esc] pause\n"
			"\n"
			"Press [H] in-game for the full controls + mechanics reference.");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuHowToBody", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuHowToBody", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUISize("MenuHowToBody", 1100.0f, 220.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuHowToBody", 0.0f, DPUI::fMENU_BTN_H + DPUI::fMENU_BTN_SPACING + 252.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuHowToBody", DPUI::fMENU_HOWTO_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("MenuHowToBody", 0.9f, 0.9f, 0.85f, 1.0f);

		Zenith_EditorAutomation::AddStep_AttachScript("DPMainMenuController_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
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
	// DPOrbitCamera_Behaviour's SetOrbitTarget / SetOrbitDistance setters
	// once the layout bounds are known.
	// ============================================================================
	void AuthorDPGameSceneFrame()
	{
		// ------ GameManager: hosts global controllers + HUD UI ----------------
		Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");

		Zenith_EditorAutomation::AddStep_AddCamera();
		// Pre-possession overview camera — positioned over the centre of the
		// authored playable area (X+Z range ≈ 0..100, so centre ≈ (50, 0, 50))
		// and pulled back/up so the player sees most of the map before
		// clicking a villager. Procgen overrides via DPOrbitCamera setters
		// at runtime once bounds are known.
		Zenith_EditorAutomation::AddStep_SetCameraPosition(50.0f, 35.0f, -15.0f);
		Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.85f);  // ~49° down
		Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(55.0f));
		Zenith_EditorAutomation::AddStep_SetAsMainCamera();

		Zenith_EditorAutomation::AddStep_AddUI();
		// MVP-UI-polish: TopLeft / BottomLeft anchored text gets Left alignment
		// (default but explicit); TopRight gets Right; *Center anchors get
		// Center. This keeps text growing away from the screen edge instead
		// of toward it, fixing the "off the right hand side" bug.
		Zenith_EditorAutomation::AddStep_CreateUIText("LifeBar", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("LifeBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("LifeBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("LifeBar", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("LifeBar", DPUI::fHUD_LIFEBAR_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("LifeBar", 0.3f, 1.0f, 0.3f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("LifeBar", false);

		// Held-item readout sits below the life bar — same anchor, offset down.
		Zenith_EditorAutomation::AddStep_CreateUIText("HeldItem", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("HeldItem", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("HeldItem", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("HeldItem", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 50.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("HeldItem", DPUI::fHUD_HELDITEM_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("HeldItem", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("HeldItem", false);

		// Objective counter, top-right corner. Right-aligned so the text
		// grows leftward into the screen instead of off the right edge.
		Zenith_EditorAutomation::AddStep_CreateUIText("Objectives", "Objectives: 0/5");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("Objectives", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("Objectives", static_cast<int>(Zenith_UI::TextAlignment::Right));
		Zenith_EditorAutomation::AddStep_SetUIPosition("Objectives", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("Objectives", DPUI::fHUD_OBJECTIVES_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("Objectives", 0.95f, 0.7f, 0.7f, 1.0f);

		Zenith_EditorAutomation::AddStep_CreateUIText("Status", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 0.0f, -80.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", DPUI::fHUD_STATUS_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.9f, 0.2f, 0.2f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);

		// 2026-05-21: LockedDoorAlert. A short-lived warning under the Status
		// banner that fires on DP_OnDoorLockRejected. Pre-fix the player got
		// zero feedback that a door was locked -- F-press silently did
		// nothing. The HUD now flashes "LOCKED -- needs Key" or similar
		// for ~2 s. Hidden by default; the controller flips visibility.
		Zenith_EditorAutomation::AddStep_CreateUIText("LockedDoorAlert", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("LockedDoorAlert", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("LockedDoorAlert", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("LockedDoorAlert", 0.0f, -40.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("LockedDoorAlert", DPUI::fHUD_STATUS_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("LockedDoorAlert", 0.95f, 0.30f, 0.20f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("LockedDoorAlert", false);

		// 2026-05-21: ScentBar. The companion bar visualisation to
		// ScentIndicator (which is a numeric "Scent: 0.42"). The bar
		// gives the player a glanceable readout of scent saturation
		// across the 0-1 range, with colour signalling when scent
		// crosses the hound-bark threshold (0.5).
		Zenith_EditorAutomation::AddStep_CreateUIText("ScentBar", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("ScentBar", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("ScentBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("ScentBar", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET - 25.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("ScentBar", DPUI::fHUD_SCENT_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("ScentBar", 0.7f, 0.3f, 0.9f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("ScentBar", false);

		// 2026-05-21: ArchetypeStatus. A one-line description of the
		// possessed villager's archetype-specific gameplay rule.
		// Sits below VillagerInfo on the TopLeft stack. Hidden when
		// not possessing or possessing a Farmhand (baseline).
		Zenith_EditorAutomation::AddStep_CreateUIText("ArchetypeStatus", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("ArchetypeStatus", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("ArchetypeStatus", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("ArchetypeStatus", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 245.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("ArchetypeStatus", DPUI::fHUD_VILLAGER_INFO_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("ArchetypeStatus", 0.95f, 0.85f, 0.65f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("ArchetypeStatus", false);

		// 2026-05-21: TutorialOverlay. Large centered text driven by
		// DP_Tutorial::GetActiveTipText. Shows first-encounter tips
		// (FirstPossession, FirstIronPickup, FirstLockedDoor, etc) for
		// ~5 s each, auto-clearing. Sits above the lower-third
		// WhisperLine + InteractHint stack so it doesn't compete for
		// the bottom-of-screen "current advice" space; uses the
		// vertical middle for the eye-catch-while-playing read.
		Zenith_EditorAutomation::AddStep_CreateUIText("TutorialOverlay", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("TutorialOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("TutorialOverlay", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("TutorialOverlay", 0.0f, 120.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("TutorialOverlay", DPUI::fHUD_WHISPER_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("TutorialOverlay", 1.0f, 0.95f, 0.75f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("TutorialOverlay", false);

		// MVP-2.5.4: Dawn sun-gauge -- text countdown at top-centre.
		Zenith_EditorAutomation::AddStep_CreateUIText("DawnGauge", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("DawnGauge", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("DawnGauge", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("DawnGauge", 0.0f, DPUI::fEDGE_INSET);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("DawnGauge", DPUI::fHUD_DAWN_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("DawnGauge", 1.0f, 0.85f, 0.6f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("DawnGauge", false);

		// MVP-2.5.2: Scent indicator at bottom-left.
		Zenith_EditorAutomation::AddStep_CreateUIText("ScentIndicator", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("ScentIndicator", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("ScentIndicator", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("ScentIndicator", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("ScentIndicator", DPUI::fHUD_SCENT_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("ScentIndicator", 0.7f, 0.3f, 0.9f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("ScentIndicator", false);

		// MVP-2.5.1: Whisper line at bottom-centre.
		Zenith_EditorAutomation::AddStep_CreateUIText("WhisperLine", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("WhisperLine", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("WhisperLine", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("WhisperLine", 0.0f, -100.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("WhisperLine", DPUI::fHUD_WHISPER_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("WhisperLine", 0.85f, 0.4f, 0.4f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("WhisperLine", false);

		// MVP-2.5.3: Aelfric awareness icon top-right, below the Objectives
		// counter. Right-aligned to grow leftward into the screen.
		Zenith_EditorAutomation::AddStep_CreateUIText("AelfricAwareness", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("AelfricAwareness", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("AelfricAwareness", static_cast<int>(Zenith_UI::TextAlignment::Right));
		Zenith_EditorAutomation::AddStep_SetUIPosition("AelfricAwareness", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 60.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("AelfricAwareness", DPUI::fHUD_AWARENESS_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("AelfricAwareness", 0.95f, 0.6f, 0.3f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("AelfricAwareness", false);

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
		Zenith_EditorAutomation::AddStep_CreateUIText("VillagerInfo", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("VillagerInfo", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("VillagerInfo", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("VillagerInfo", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 215.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("VillagerInfo", DPUI::fHUD_VILLAGER_INFO_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("VillagerInfo", 0.85f, 0.85f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("VillagerInfo", false);

		// LifeNumeric -- "Life: 23.4 / 30.0 s" alongside the ASCII bar.
		Zenith_EditorAutomation::AddStep_CreateUIText("LifeNumeric", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("LifeNumeric", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("LifeNumeric", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("LifeNumeric", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 90.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("LifeNumeric", DPUI::fHUD_LIFE_NUMERIC_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("LifeNumeric", 0.7f, 1.0f, 0.7f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("LifeNumeric", false);

		// ReagentHelp -- one-line description shown when holding a special
		// reagent (BellSoul / BogWater / SkeletonKey). Hidden otherwise.
		Zenith_EditorAutomation::AddStep_CreateUIText("ReagentHelp", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("ReagentHelp", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("ReagentHelp", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("ReagentHelp", DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 175.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("ReagentHelp", DPUI::fHUD_REAGENT_HELP_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("ReagentHelp", 0.9f, 0.85f, 0.6f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("ReagentHelp", false);

		// MovementMode -- Sprint / Walk-Quiet / Move. Bottom-left, above ScentIndicator.
		Zenith_EditorAutomation::AddStep_CreateUIText("MovementMode", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MovementMode", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("MovementMode", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MovementMode", DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET - 50.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("MovementMode", DPUI::fHUD_MOVEMENT_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("MovementMode", 0.85f, 0.85f, 0.85f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("MovementMode", false);

		// VillagersAlive -- count of live villagers. Top-right, below AelfricAwareness.
		Zenith_EditorAutomation::AddStep_CreateUIText("VillagersAlive", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("VillagersAlive", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("VillagersAlive", static_cast<int>(Zenith_UI::TextAlignment::Right));
		Zenith_EditorAutomation::AddStep_SetUIPosition("VillagersAlive", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 120.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("VillagersAlive", DPUI::fHUD_VILLAGER_COUNT_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("VillagersAlive", 0.85f, 0.85f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("VillagersAlive", false);

		// PriestDistance -- meters to closest priest. Top-right, below VillagersAlive.
		// Controller recolours red/amber/grey based on distance.
		Zenith_EditorAutomation::AddStep_CreateUIText("PriestDistance", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("PriestDistance", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("PriestDistance", static_cast<int>(Zenith_UI::TextAlignment::Right));
		Zenith_EditorAutomation::AddStep_SetUIPosition("PriestDistance", -DPUI::fEDGE_INSET, DPUI::fEDGE_INSET + 170.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("PriestDistance", DPUI::fHUD_PRIEST_DIST_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("PriestDistance", 0.85f, 0.85f, 0.85f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("PriestDistance", false);

		// RunTimer -- mm:ss since first possession. Top-center, below DawnGauge.
		Zenith_EditorAutomation::AddStep_CreateUIText("RunTimer", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("RunTimer", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("RunTimer", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("RunTimer", 0.0f, DPUI::fEDGE_INSET + 55.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("RunTimer", DPUI::fHUD_RUN_TIMER_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("RunTimer", 0.9f, 0.9f, 0.7f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("RunTimer", false);

		// InteractHint -- "F to interact with door/chest/forge/...".
		// Bottom-center, above WhisperLine. Visible only when near an interactable.
		Zenith_EditorAutomation::AddStep_CreateUIText("InteractHint", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("InteractHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("InteractHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("InteractHint", 0.0f, -160.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("InteractHint", DPUI::fHUD_INTERACT_HINT_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("InteractHint", 1.0f, 0.95f, 0.5f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("InteractHint", false);

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
		Zenith_EditorAutomation::AddStep_CreateUIText("ControlsHint",
			"[LMB] Possess villager\n"
			"[WASD] Move    [Q/E] Camera\n"
			"[Shift] Sprint  [Ctrl] Walk quiet\n"
			"[F] Interact   [G] Drop item\n"
			"[Space] Ability   [Wheel] Zoom\n"
			"[Esc] Pause   [R] Restart   [H] Help");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("ControlsHint", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("ControlsHint", static_cast<int>(Zenith_UI::TextAlignment::Right));
		Zenith_EditorAutomation::AddStep_SetUISize("ControlsHint", 550.0f, 220.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("ControlsHint", -DPUI::fEDGE_INSET, -DPUI::fEDGE_INSET);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("ControlsHint", DPUI::fHUD_CONTROLS_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("ControlsHint", 0.85f, 0.85f, 0.85f, 0.9f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("ControlsHint", true);

		// TutorialHint -- single-line context guidance, TopCenter.
		Zenith_EditorAutomation::AddStep_CreateUIText("TutorialHint", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("TutorialHint", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("TutorialHint", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUISize("TutorialHint", 1200.0f, 50.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("TutorialHint", 0.0f, DPUI::fEDGE_INSET + 110.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("TutorialHint", DPUI::fHUD_TUTORIAL_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("TutorialHint", 1.0f, 0.95f, 0.7f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("TutorialHint", false);

		// HelpBg -- semi-transparent background for the HelpOverlay modal.
		// Authored BEFORE HelpOverlay so it renders behind (canvas
		// iterates root elements in insertion order).
		Zenith_EditorAutomation::AddStep_CreateUIRect("HelpBg");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("HelpBg", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUISize("HelpBg", 1200.0f, 900.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("HelpBg", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("HelpBg", 0.02f, 0.02f, 0.04f, 0.92f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("HelpBg", false);

		// HelpOverlay -- full-screen tutorial text, toggled with [H].
		Zenith_EditorAutomation::AddStep_CreateUIText("HelpOverlay",
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
		Zenith_EditorAutomation::AddStep_SetUIAnchor("HelpOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("HelpOverlay", static_cast<int>(Zenith_UI::TextAlignment::Left));
		Zenith_EditorAutomation::AddStep_SetUISize("HelpOverlay", 1150.0f, 870.0f);
		Zenith_EditorAutomation::AddStep_SetUIPosition("HelpOverlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("HelpOverlay", DPUI::fHUD_HELP_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("HelpOverlay", 0.98f, 0.95f, 0.85f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("HelpOverlay", false);

		// MVP-4.3.2: post-victory / post-run-lost restart prompt.
		Zenith_EditorAutomation::AddStep_CreateUIText("RestartPrompt", "Press R to restart, Q to quit");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("RestartPrompt", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("RestartPrompt", 0.0f, 60.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("RestartPrompt", 28.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("RestartPrompt", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("RestartPrompt", false);

		// Attach the global coordinators. Each is independent — order doesn't
		// matter, but the convention is to attach the player first.
		Zenith_EditorAutomation::AddStep_AttachScript("DPPlayerController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPItemManager_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPFogPass_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPHUDController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPOrbitCamera_Behaviour");

		// ------ PauseManager: dedicated entity for the pause controller -------
		// MVP-1.1: the pause controller migrates itself to the persistent
		// scene in OnStart so it keeps ticking while the gameplay scene is
		// paused. Dedicated entity so MarkEntityPersistent doesn't drag
		// the camera / HUD / fog pass to the persistent scene with it.
		Zenith_EditorAutomation::AddStep_CreateEntity("PauseManager");
		Zenith_EditorAutomation::AddStep_AddUI();
		Zenith_EditorAutomation::AddStep_CreateUIText("PauseOverlay", "PAUSED\nEsc: Resume   R: Restart   Q: Quit");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("PauseOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIAlignment("PauseOverlay", static_cast<int>(Zenith_UI::TextAlignment::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("PauseOverlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("PauseOverlay", DPUI::fHUD_PAUSE_FONT);
		Zenith_EditorAutomation::AddStep_SetUIColor("PauseOverlay", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("PauseOverlay", false);
		Zenith_EditorAutomation::AddStep_AttachScript("DPPauseMenuController_Behaviour");
	}

	// (AuthorGameLevelScene removed 2026-05-19 -- the UE-exported GameLevel
	// scene is gone. Procgen-driven ProcLevel is now the only gameplay
	// surface. See AuthorProcLevelScene below.)

	// ============================================================================
	// ProcLevel scene -- the procgen-driven gameplay scene.
	//
	// Static authoring is intentionally lean: GameManager + UI scaffolding,
	// PauseManager, GroundPlane, a handful of corner lights, plus a single
	// "ProcLevelBootstrap" entity carrying DPProcLevelBootstrap_Behaviour.
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
		Zenith_EditorAutomation::AddStep_CreateScene("ProcLevel");

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
		Zenith_EditorAutomation::AddStep_CreateEntity("GroundPlane");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(0.0f, 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetTransformScale(100.0f, 1.0f, 100.0f);
		AuthorMeshAndCollider(
			"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true,
			COLLISION_VOLUME_TYPE_OBB,
			RIGIDBODY_TYPE_STATIC);

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
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("ProcLight", i));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(
				afLightXZ[i][0], 10.0f, afLightXZ[i][1]);
			Zenith_EditorAutomation::AddStep_AddComponent("Light");
			Zenith_EditorAutomation::AddStep_SetLightIntensity(2000.0f);
			Zenith_EditorAutomation::AddStep_SetLightRange(60.0f);
			Zenith_EditorAutomation::AddStep_SetLightColor(1.0f, 0.95f, 0.85f);
		}

		// ------ The bootstrap entity ------------------------------------------
		// Attaching this script is the ENTIRE level-content authoring. Every
		// wall, item, character is materialised by the script's OnAwake at
		// scene-load time. Changing m_uSeed (or the upcoming Tuning.json
		// seed source) produces a different level without re-authoring.
		Zenith_EditorAutomation::AddStep_CreateEntity("ProcLevelBootstrap");
		Zenith_EditorAutomation::AddStep_AttachScript("DPProcLevelBootstrap_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/ProcLevel" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}
}

void Project_RegisterEditorAutomationSteps()
{
	AuthorFrontEndScene();    // build index 0
	AuthorProcLevelScene();   // build index 1

	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif // ZENITH_TOOLS

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/ProcLevel" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);
}
