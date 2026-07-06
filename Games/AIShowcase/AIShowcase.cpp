#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "AIShowcase/Components/AIShowcase_GameComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "AIShowcase/Components/AIShowcase_GraphNodes.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "UI/Zenith_UIButton.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

// ============================================================================
// AIShowcase Resources - Global access for behaviours
// ============================================================================
namespace AIShowcase
{
	static AIShowcaseResources g_xResources;
	AIShowcaseResources& Resources() { return g_xResources; }
}

static bool s_bResourcesInitialized = false;

static void InitializeAIShowcaseResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace AIShowcase;

	// Create geometries using registry's cached primitives
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	Resources().m_xSphereAsset.Set(Zenith_MeshGeometryAsset::CreateUnitSphere(16));
	Resources().m_pxSphereGeometry = Resources().m_xSphereAsset.GetDirect()->GetGeometry();

	Resources().m_xCylinderAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCylinder(16));
	Resources().m_pxCylinderGeometry = Resources().m_xCylinderAsset.GetDirect()->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with grid texture and BaseColor
	Resources().m_xFloorMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xFloorMaterial.GetDirect()->SetName("AIShowcase_Floor");
	Resources().m_xFloorMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xFloorMaterial.GetDirect()->SetBaseColor({ 64.f/255.f, 64.f/255.f, 64.f/255.f, 1.f });

	Resources().m_xWallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xWallMaterial.GetDirect()->SetName("AIShowcase_Wall");
	Resources().m_xWallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xWallMaterial.GetDirect()->SetBaseColor({ 128.f/255.f, 96.f/255.f, 64.f/255.f, 1.f });

	Resources().m_xObstacleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xObstacleMaterial.GetDirect()->SetName("AIShowcase_Obstacle");
	Resources().m_xObstacleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xObstacleMaterial.GetDirect()->SetBaseColor({ 96.f/255.f, 96.f/255.f, 96.f/255.f, 1.f });

	Resources().m_xPlayerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xPlayerMaterial.GetDirect()->SetName("AIShowcase_Player");
	Resources().m_xPlayerMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xPlayerMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	Resources().m_xEnemyMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xEnemyMaterial.GetDirect()->SetName("AIShowcase_Enemy");
	Resources().m_xEnemyMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xEnemyMaterial.GetDirect()->SetBaseColor({ 230.f/255.f, 77.f/255.f, 77.f/255.f, 1.f });

	Resources().m_xLeaderMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xLeaderMaterial.GetDirect()->SetName("AIShowcase_Leader");
	Resources().m_xLeaderMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xLeaderMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xFlankerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xFlankerMaterial.GetDirect()->SetName("AIShowcase_Flanker");
	Resources().m_xFlankerMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xFlankerMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 128.f/255.f, 0.f/255.f, 1.f });

	// Cover/patrol point materials
	Resources().m_xCoverPointMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCoverPointMaterial.GetDirect()->SetName("AIShowcase_CoverPoint");
	Resources().m_xCoverPointMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCoverPointMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xPatrolPointMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xPatrolPointMaterial.GetDirect()->SetName("AIShowcase_PatrolPoint");
	Resources().m_xPatrolPointMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xPatrolPointMaterial.GetDirect()->SetBaseColor({ 153.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	s_bResourcesInitialized = true;
}
// ============================================================================

const char* Project_GetName()
{
	return "AIShowcase";
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
	// Initialize resources at startup
	InitializeAIShowcaseResources();

	// Initialize AI systems
	Zenith_PerceptionSystem::Initialise();
	Zenith_SquadManager::Initialise();
	Zenith_TacticalPointSystem::Initialise();

#ifdef ZENITH_TOOLS
	// Register AI debug variables
	Zenith_AIDebugVariables::Initialise();
#endif

	// Register the AIShowcase game component with the component-meta registry
	// (serialization/lifecycle) and, in tools builds, the editor "Add Component"
	// registry (display name used by AddStep_AddComponent / the editor menu).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.RegisterComponent<AIShowcase_GameComponent>("AIShowcaseGame", 100);
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<AIShowcase_GameComponent>("AIShowcaseGame");
#endif

	// Behaviour Graph node library (GameFlow menu/state/verbs + per-enemy brain).
	AIShowcase_RegisterGraphNodes();
}

void Project_Shutdown()
{
	// Shutdown AI systems
	Zenith_TacticalPointSystem::Shutdown();
	Zenith_SquadManager::Shutdown();
	Zenith_PerceptionSystem::Shutdown();

	// Cleanup NavMesh
	delete AIShowcase::Resources().m_pxArenaNavMesh;
	AIShowcase::Resources().m_pxArenaNavMesh = nullptr;

	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	AIShowcase::Resources().m_xCubeAsset.Clear();
	AIShowcase::Resources().m_xSphereAsset.Clear();
	AIShowcase::Resources().m_xCylinderAsset.Clear();
	AIShowcase::Resources().m_xFloorMaterial.Clear();
	AIShowcase::Resources().m_xWallMaterial.Clear();
	AIShowcase::Resources().m_xObstacleMaterial.Clear();
	AIShowcase::Resources().m_xPlayerMaterial.Clear();
	AIShowcase::Resources().m_xEnemyMaterial.Clear();
	AIShowcase::Resources().m_xLeaderMaterial.Clear();
	AIShowcase::Resources().m_xFlankerMaterial.Clear();
	AIShowcase::Resources().m_xCoverPointMaterial.Clear();
	AIShowcase::Resources().m_xPatrolPointMaterial.Clear();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All resources initialized in Project_RegisterGameComponents
}

// ============================================================================
// Behaviour Graph builders (boot-authored via AddStep_GraphBuild).
// ============================================================================

// AIShowcase_GameFlow - menu/state machine + the discrete player verbs. Lives
// on BOTH GameManagers (menu + gameplay); gameState defaults MENU, and the
// gameplay StartGame writes PLAYING. The menu chains (OnUIButtonClicked / Focus)
// are inert in the gameplay scene (no MenuPlay button), and the PLAYING-gated
// verbs (Space/1-5/R) are inert in the menu scene.
static void BuildGraph_AIShowcaseGameFlow(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(static_cast<int32_t>(AIShowcaseGameState::MAIN_MENU));
	xBuilder.Variable("gameState", xValue);

	// Menu: Play click -> gameplay scene (SINGLE); keep the button focused.
	const u_int uPlayClicked = xBuilder.Node("OnUIButtonClicked");
	xBuilder.ParamString(uPlayClicked, "m_strButton", "MenuPlay");
	const u_int uLoadGame = xBuilder.Node("LoadSceneByIndex");
	xBuilder.ParamInt(uLoadGame, "m_iSceneIndex", 1);
	xBuilder.Chain(uPlayClicked, uLoadGame);

	const u_int uFocusTick = xBuilder.Node("OnUpdate");
	const u_int uFocus = xBuilder.Node("AIShowcaseFocusPlayButton");
	xBuilder.Chain(uFocusTick, uFocus);

	// P: PLAYING <-> PAUSED (arena pause/unpause is the StateMachine's job).
	const u_int uOnP = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uOnP, "m_iKeyCode", ZENITH_KEY_P);
	const u_int uSwitchP = xBuilder.Node("SwitchOnInt");
	xBuilder.ParamString(uSwitchP, "m_strVar", "gameState");
	xBuilder.ParamInt(uSwitchP, "m_iCaseCount", 3);
	xBuilder.Chain(uOnP, uSwitchP);
	const u_int uToPaused = xBuilder.Node("SetBlackboardInt");
	xBuilder.ParamString(uToPaused, "m_strVariable", "gameState");
	xBuilder.ParamInt(uToPaused, "m_iValue", static_cast<int32_t>(AIShowcaseGameState::PAUSED));
	xBuilder.Edge(uSwitchP, static_cast<u_int>(AIShowcaseGameState::PLAYING), uToPaused);
	const u_int uToPlaying = xBuilder.Node("SetBlackboardInt");
	xBuilder.ParamString(uToPlaying, "m_strVariable", "gameState");
	xBuilder.ParamInt(uToPlaying, "m_iValue", static_cast<int32_t>(AIShowcaseGameState::PLAYING));
	xBuilder.Edge(uSwitchP, static_cast<u_int>(AIShowcaseGameState::PAUSED), uToPlaying);

	// Esc: from PLAYING or PAUSED -> return to menu + gameState = MENU (setting
	// MENU here is REQUIRED so the systems component reads MENU this same frame
	// and skips UpdateAISystems - ReturnToMenu shut the AI systems down).
	const u_int uOnEsc = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uOnEsc, "m_iKeyCode", ZENITH_KEY_ESCAPE);
	const u_int uSwitchEsc = xBuilder.Node("SwitchOnInt");
	xBuilder.ParamString(uSwitchEsc, "m_strVar", "gameState");
	xBuilder.ParamInt(uSwitchEsc, "m_iCaseCount", 3);
	xBuilder.Chain(uOnEsc, uSwitchEsc);
	for (int32_t iFromState = static_cast<int32_t>(AIShowcaseGameState::PLAYING);
		iFromState <= static_cast<int32_t>(AIShowcaseGameState::PAUSED); ++iFromState)
	{
		const u_int uRet = xBuilder.Node("AIShowcaseReturnToMenu");
		const u_int uToMenu = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uToMenu, "m_strVariable", "gameState");
		xBuilder.ParamInt(uToMenu, "m_iValue", static_cast<int32_t>(AIShowcaseGameState::MAIN_MENU));
		xBuilder.Edge(uSwitchEsc, static_cast<u_int>(iFromState), uRet);
		xBuilder.Chain(uRet, uToMenu);
	}

	// Space / R: single PLAYING-gated verbs.
	const u_int uOnSpace = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uOnSpace, "m_iKeyCode", ZENITH_KEY_SPACE);
	const u_int uSpaceGate = xBuilder.Node("CompareBlackboardInt");
	xBuilder.ParamString(uSpaceGate, "m_strVar", "gameState");
	xBuilder.ParamInt(uSpaceGate, "m_iCompareTo", static_cast<int32_t>(AIShowcaseGameState::PLAYING));
	xBuilder.ParamString(uSpaceGate, "m_strResultVar", "isPlaying");
	const u_int uSpaceBranch = xBuilder.Node("Branch");
	xBuilder.ParamString(uSpaceBranch, "m_strConditionVar", "isPlaying");
	const u_int uEmit = xBuilder.Node("AIShowcaseEmitPlayerSound");
	xBuilder.Chain(uOnSpace, uSpaceGate).Chain(uSpaceGate, uSpaceBranch);
	xBuilder.Edge(uSpaceBranch, 0, uEmit);

	const u_int uOnR = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uOnR, "m_iKeyCode", ZENITH_KEY_R);
	const u_int uRGate = xBuilder.Node("CompareBlackboardInt");
	xBuilder.ParamString(uRGate, "m_strVar", "gameState");
	xBuilder.ParamInt(uRGate, "m_iCompareTo", static_cast<int32_t>(AIShowcaseGameState::PLAYING));
	xBuilder.ParamString(uRGate, "m_strResultVar", "isPlaying");
	const u_int uRBranch = xBuilder.Node("Branch");
	xBuilder.ParamString(uRBranch, "m_strConditionVar", "isPlaying");
	const u_int uReset = xBuilder.Node("AIShowcaseResetDemo");
	xBuilder.Chain(uOnR, uRGate).Chain(uRGate, uRBranch);
	xBuilder.Edge(uRBranch, 0, uReset);

	// 1-5: PLAYING-gated formation select.
	const int32_t aiFormationKeys[] = { ZENITH_KEY_1, ZENITH_KEY_2, ZENITH_KEY_3, ZENITH_KEY_4, ZENITH_KEY_5 };
	for (int32_t iFormation = 0; iFormation < 5; ++iFormation)
	{
		const u_int uKey = xBuilder.Node("OnKeyPressed");
		xBuilder.ParamInt(uKey, "m_iKeyCode", aiFormationKeys[iFormation]);
		const u_int uGate = xBuilder.Node("CompareBlackboardInt");
		xBuilder.ParamString(uGate, "m_strVar", "gameState");
		xBuilder.ParamInt(uGate, "m_iCompareTo", static_cast<int32_t>(AIShowcaseGameState::PLAYING));
		xBuilder.ParamString(uGate, "m_strResultVar", "isPlaying");
		const u_int uBranch = xBuilder.Node("Branch");
		xBuilder.ParamString(uBranch, "m_strConditionVar", "isPlaying");
		const u_int uForm = xBuilder.Node("AIShowcaseSetFormation");
		xBuilder.ParamInt(uForm, "m_iFormation", iFormation);
		xBuilder.Chain(uKey, uGate).Chain(uGate, uBranch);
		xBuilder.Edge(uBranch, 0, uForm);
	}

	// Pause enter/exit -> arena pause (decoupled from the key chains).
	const u_int uEnterPaused = xBuilder.Node("OnCustomEvent");
	xBuilder.ParamString(uEnterPaused, "m_strEventName", "AISEnter_Paused");
	const u_int uPauseArena = xBuilder.Node("AIShowcaseSetArenaPaused");
	xBuilder.ParamBool(uPauseArena, "m_bPaused", true);
	xBuilder.Chain(uEnterPaused, uPauseArena);
	const u_int uExitPaused = xBuilder.Node("OnCustomEvent");
	xBuilder.ParamString(uExitPaused, "m_strEventName", "AISExit_Paused");
	const u_int uUnpauseArena = xBuilder.Node("AIShowcaseSetArenaPaused");
	xBuilder.ParamBool(uUnpauseArena, "m_bPaused", false);
	xBuilder.Chain(uExitPaused, uUnpauseArena);

	// StateMachine LAST in node order: key chains set gameState first, so a
	// transition fires AISEnter_/AISExit_<state> the same dispatch (Marble pattern).
	const u_int uStateMachine = xBuilder.Node("StateMachine");
	xBuilder.ParamString(uStateMachine, "m_strStateVar", "gameState");
	xBuilder.ParamInt(uStateMachine, "m_iStateCount", 3);
	xBuilder.ParamString(uStateMachine, "m_strStateNames", "Menu,Playing,Paused");
	xBuilder.ParamString(uStateMachine, "m_strEventPrefix", "AIS");
	const u_int uSmTick = xBuilder.Node("OnUpdate");
	xBuilder.Chain(uSmTick, uStateMachine);
}

// AIShowcase_EnemyBrain - per-enemy decision, driven by "EnemyBrainTick" fired
// from the C++ driver's Phase 1. Reactive Selector: chase the perceived player
// (share with the squad, set the nav destination) else patrol a waypoint. No
// RUNNING leaves, so the Selector fully re-evaluates each tick (tennis pattern);
// abortPreempted = false per the shared-nav-agent R3 guidance.
static void BuildGraph_AIShowcaseEnemyBrain(Zenith_GraphBuilder& xBuilder)
{
	Zenith_PropertyValue xValue;
	xValue.SetPackedEntityID(0);	// playerTarget - seeded per-instance in CreateEnemy
	xBuilder.Variable("playerTarget", xValue);
	xValue.SetInt32(0);				// enemyIndex - seeded per-instance
	xBuilder.Variable("enemyIndex", xValue);
	xValue.SetVector3(Zenith_Maths::Vector3(0.0f));
	xBuilder.Variable("chaseDest", xValue);

	const u_int uTick = xBuilder.Node("OnCustomEvent");
	xBuilder.ParamString(uTick, "m_strEventName", "EnemyBrainTick");

	const u_int uSelector = xBuilder.Node("Selector");
	xBuilder.ParamInt(uSelector, "m_iBranchCount", 2);
	xBuilder.ParamBool(uSelector, "m_bAbortPreempted", false);
	xBuilder.Chain(uTick, uSelector);

	// Branch 0 (highest priority) - chase.
	const u_int uSense = xBuilder.Node("AIShowcaseSensePlayer");
	const u_int uShare = xBuilder.Node("AIShowcaseShareTargetWithSquad");
	const u_int uSetDest = xBuilder.Node("SetNavDestination");
	xBuilder.ParamString(uSetDest, "m_strDestinationVar", "chaseDest");
	xBuilder.Edge(uSelector, 0, uSense);
	xBuilder.Chain(uSense, uShare).Chain(uShare, uSetDest);

	// Branch 1 (fallback) - patrol.
	const u_int uPatrol = xBuilder.Node("AIShowcasePatrol");
	xBuilder.Edge(uSelector, 1, uPatrol);
}

void Project_RegisterEditorAutomationSteps()
{
	// Build the behaviour graphs (GameFlow attached to the GameManagers below;
	// EnemyBrain attached at runtime in CreateEnemy via AddGraphByAssetPath).
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.AddStep_GraphBuild("game:Graphs/AIShowcase_GameFlow.bgraph", &BuildGraph_AIShowcaseGameFlow);
	xAuto.AddStep_GraphBuild("game:Graphs/AIShowcase_EnemyBrain.bgraph", &BuildGraph_AIShowcaseEnemyBrain);

	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("MenuManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 30.f, -35.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(500.f);
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "AI SHOWCASE");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 48.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 0.2f, 0.6f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_AddComponent("AIShowcaseGame");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/AIShowcase_GameFlow.bgraph");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- AIShowcase gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("AIShowcase");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 30.f, -35.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(500.f);
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// HUD UI: margin=20, textSize=14, lineHeight=22
	// Title: TopLeft, (20, 20), fontSize=42, white, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Title", "AI SHOWCASE");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Title", 20.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Title", 42.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Title", false);

	// ControlsHeader: TopLeft, (20, 64), fontSize=33.6, yellow, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ControlsHeader", "Controls:");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ControlsHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ControlsHeader", 20.f, 64.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ControlsHeader", 33.6f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ControlsHeader", 0.9f, 0.9f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ControlsHeader", false);

	// Control lines: TopLeft, (20, 86+22*i), fontSize=28, gray, hidden
	{
		static const char* s_aszControlNames[] = { "Control0", "Control1", "Control2", "Control3", "Control4" };
		static const char* s_aszControlTexts[] = {
			"WASD: Move player", "Space: Attack/Make sound",
			"1-5: Change formation", "R: Reset demo", "Esc: Menu"
		};
		for (uint32_t u = 0; u < 5; ++u)
		{
			float fY = 86.f + 22.f * static_cast<float>(u);
			g_xEngine.EditorAutomation().AddStep_CreateUIText(s_aszControlNames[u], s_aszControlTexts[u]);
			g_xEngine.EditorAutomation().AddStep_SetUIAnchor(s_aszControlNames[u], static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
			g_xEngine.EditorAutomation().AddStep_SetUIPosition(s_aszControlNames[u], 20.f, fY);
			g_xEngine.EditorAutomation().AddStep_SetUIFontSize(s_aszControlNames[u], 28.f);
			g_xEngine.EditorAutomation().AddStep_SetUIColor(s_aszControlNames[u], 0.8f, 0.8f, 0.8f, 1.f);
			g_xEngine.EditorAutomation().AddStep_SetUIVisible(s_aszControlNames[u], false);
		}
	}

	// Status: BottomLeft, (20, -20), fontSize=28, blue-ish, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "Enemies: 0 | Squads: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 20.f, -20.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 28.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.6f, 0.8f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

	// Game component
	g_xEngine.EditorAutomation().AddStep_AddComponent("AIShowcaseGame");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/AIShowcase_GameFlow.bgraph");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/AIShowcase" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/AIShowcase" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
