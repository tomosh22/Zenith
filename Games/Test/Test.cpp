#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Core/Zenith_GraphicsOptions.h"

// Returns the project name - used by Tools code to construct asset paths
// The build system provides ZENITH_ROOT, and paths are constructed as:
// ZENITH_ROOT + "Games/" + Project_GetName() + "/Assets/"
const char* Project_GetName()
{
	return "Test";
}

// Returns the game assets directory - called by Zenith engine code
// GAME_ASSETS_DIR is defined by the build system for each game
const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

#include "Test/Components/Test_PlayerControllerComponent.h"
#include "Test/Components/Test_GraphNodes.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "UI/Zenith_UI.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Input/Zenith_KeyCodes.h"
#endif

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Register the game components with the component-meta registry
	// (orders 100+ are unique per game, after the engine built-ins).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.RegisterComponent<Test_PlayerControllerComponent>("TestPlayerController", 100);

	// Behaviour Graph node library (used by the boot-authored physics-toy graphs).
	Test_RegisterGraphNodes();

#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<Test_PlayerControllerComponent>("TestPlayerController");
#endif
}

void Project_Shutdown()
{
	// Test game has no resources that need explicit cleanup
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Test game has no resources that need initialization
}

namespace
{
	// Test_Spinner: the retired TestSpinPlatform mega-node decomposed into
	// engine physics nodes - same call order (angular velocity, then the
	// zero-linear-velocity anchor).
	void BuildGraph_TestSpinner(Zenith_GraphBuilder& xBuilder)
	{
		const u_int uSource = xBuilder.Node("OnUpdate");
		const u_int uAngular = xBuilder.Node("SetAngularVelocity");
		xBuilder.ParamVec3(uAngular, "m_xAngularVelocity", Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		// SetVelocity's defaults ((0,0,0), all axes) ARE the anchor.
		const u_int uAnchor = xBuilder.Node("SetVelocity");
		xBuilder.Chain(uSource, uAngular).Chain(uAngular, uAnchor);
	}

	// Test_Spring: the retired TestHookesForce mega-node decomposed - read
	// own position, force = target - position (Hooke with k = 1), apply.
	// The target is a declared blackboard variable (per-entity tunable).
	void BuildGraph_TestSpring(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xTarget;
		xTarget.SetVector3(Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f));
		xBuilder.Variable("springTarget", xTarget);

		const u_int uSource = xBuilder.Node("OnUpdate");
		const u_int uReadPos = xBuilder.Node("ReadEntityPosition");
		xBuilder.ParamString(uReadPos, "m_strResultVar", "springPos");
		const u_int uForce = xBuilder.Node("MathBlackboardVector3");
		xBuilder.ParamString(uForce, "m_strVar", "springTarget");
		xBuilder.ParamInt(uForce, "m_iOp", 1);	// sub: target - position
		xBuilder.ParamString(uForce, "m_strOperandVar", "springPos");
		xBuilder.ParamString(uForce, "m_strResultVar", "springForce");
		const u_int uApply = xBuilder.Node("ApplyForce");
		xBuilder.ParamString(uApply, "m_strForceVar", "springForce");
		xBuilder.Chain(uSource, uReadPos).Chain(uReadPos, uForce).Chain(uForce, uApply);
	}

	// Player actions: every decision the controller's OnUpdate used to make -
	// the E shoot binding, C fly-cam toggle, 1-6 slot selection, T/H health
	// demo, and the per-tick compass update. The shim nodes gate themselves
	// on walk mode (the old early-return semantics); movement/camera systems
	// stay C++.
	void BuildGraph_TestPlayerActions(Zenith_GraphBuilder& xBuilder)
	{
		// Programmatic seam: FireCustomEvent("Shoot") still spawns.
		const u_int uShootEvent = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uShootEvent, "m_strEventName", "Shoot");
		const u_int uShootAction = xBuilder.Node("TestSpawnProjectile");
		xBuilder.Chain(uShootEvent, uShootAction);

		// E-press drives the same action through the real input path.
		const u_int uShootKey = xBuilder.Node("OnKeyPressed");
		xBuilder.ParamInt(uShootKey, "m_iKeyCode", ZENITH_KEY_E);
		const u_int uShootAction2 = xBuilder.Node("TestSpawnProjectile");
		xBuilder.Chain(uShootKey, uShootAction2);

		const u_int uToggleKey = xBuilder.Node("OnKeyPressed");
		xBuilder.ParamInt(uToggleKey, "m_iKeyCode", ZENITH_KEY_C);
		const u_int uToggle = xBuilder.Node("TestToggleFlyCam");
		xBuilder.Chain(uToggleKey, uToggle);

		const u_int uDamageKey = xBuilder.Node("OnKeyPressed");
		xBuilder.ParamInt(uDamageKey, "m_iKeyCode", ZENITH_KEY_T);
		const u_int uDamage = xBuilder.Node("TestModifyHealth");
		xBuilder.ParamFloat(uDamage, "m_fDelta", -10.0f);
		xBuilder.Chain(uDamageKey, uDamage);

		const u_int uHealKey = xBuilder.Node("OnKeyPressed");
		xBuilder.ParamInt(uHealKey, "m_iKeyCode", ZENITH_KEY_H);
		const u_int uHeal = xBuilder.Node("TestModifyHealth");
		xBuilder.ParamFloat(uHeal, "m_fDelta", 15.0f);
		xBuilder.Chain(uHealKey, uHeal);

		for (int32_t i = 0; i < 6; ++i)
		{
			const u_int uSlotKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uSlotKey, "m_iKeyCode", ZENITH_KEY_1 + i);
			const u_int uSlot = xBuilder.Node("TestSelectSlot");
			xBuilder.ParamInt(uSlot, "m_iSlot", i);
			xBuilder.Chain(uSlotKey, uSlot);
		}

		const u_int uTick = xBuilder.Node("OnUpdate");
		const u_int uCompass = xBuilder.Node("TestUpdateCompass");
		xBuilder.Chain(uTick, uCompass);
	}
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("MenuManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "TEST");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 72.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Behaviour graphs (regenerated every boot through the programmatic
	// ---- builder - the W1 conversion's decomposed engine-node graphs) ----
	g_xEngine.EditorAutomation().AddStep_GraphBuild("game:Graphs/Test_Spinner.bgraph", &BuildGraph_TestSpinner);
	g_xEngine.EditorAutomation().AddStep_GraphBuild("game:Graphs/Test_Spring.bgraph", &BuildGraph_TestSpring);
	g_xEngine.EditorAutomation().AddStep_GraphBuild("game:Graphs/Test_PlayerActions.bgraph", &BuildGraph_TestPlayerActions);

	// ---- Test gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Test");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));

	// Spinning platform driven by the Test_Spinner graph (constant angular
	// velocity through the physics body - the retired TestRotation logic).
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Spinner");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(0.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
	g_xEngine.EditorAutomation().AddStep_AddComponent("Graph");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Test_Spinner.bgraph");

	// Spring-tethered body driven by the Test_Spring graph (Hooke's-law force
	// toward a target - the retired TestHookesLaw logic).
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Spring");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(3.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
	g_xEngine.EditorAutomation().AddStep_AddComponent("Graph");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Test_Spring.bgraph");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
