#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Marble/Components/Marble_GameComponent.h"
#include "Marble/Components/Marble_GraphNodes.h"
#include "Marble/Components/Marble_Config.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Input/Zenith_KeyCodes.h"
#endif

// ============================================================================
// Marble Resources - Global access for behaviours
// ============================================================================
namespace Marble
{
	static MarbleResources g_xResources;
	MarbleResources& Resources() { return g_xResources; }
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural UV Sphere Generation
// ============================================================================
static void GenerateUVSphere(Flux_MeshGeometry& xGeometryOut, float fRadius, uint32_t uSlices, uint32_t uStacks)
{
	uint32_t uNumVerts = (uStacks + 1) * (uSlices + 1);
	uint32_t uNumIndices = uStacks * uSlices * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	uint32_t uVertIdx = 0;

	// Generate vertices
	for (uint32_t uStack = 0; uStack <= uStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uStacks) * 3.14159265f;
		float fY = cos(fPhi) * fRadius;
		float fStackRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::length(xPos) > 0.001f ? glm::normalize(xPos) : Zenith_Maths::Vector3(0.f, 1.f, 0.f);

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uStacks)
			);

			// Simple tangent/bitangent calculation for sphere
			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);

			uVertIdx++;
		}
	}

	// Generate indices
	uint32_t uIdxIdx = 0;
	for (uint32_t uStack = 0; uStack < uStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			// Counter-clockwise winding for Vulkan
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	// Generate buffer layout and vertex data
	xGeometryOut.GenerateLayoutAndVertexData();

	// Upload to GPU
	g_xEngine.FluxMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.FluxMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Resource Initialization
// ============================================================================
static void InitializeMarbleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Marble;

	// Create sphere geometry (custom radius - tracked through registry)
	{
		auto xhSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Zenith_MeshGeometryAsset* pxSphereAsset = xhSphereAsset.GetDirect();
		Flux_MeshGeometry* pxSphereGeom = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphereGeom, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphereGeom);
		Resources().m_xSphereAsset.Set(pxSphereAsset);
		Resources().m_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}

	// Create cube geometry (uses cached unit cube)
	Resources().m_xCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials.
	// Copying the handle by value AddRefs; each material owns a ref via its own handle copy.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with grid texture and BaseColor
	Resources().m_xBallMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xBallMaterial.GetDirect()->SetName("MarbleBall");
	Resources().m_xBallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xBallMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	Resources().m_xPlatformMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xPlatformMaterial.GetDirect()->SetName("MarblePlatform");
	Resources().m_xPlatformMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xPlatformMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 102.f/255.f, 102.f/255.f, 1.f });

	Resources().m_xGoalMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xGoalMaterial.GetDirect()->SetName("MarbleGoal");
	Resources().m_xGoalMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xGoalMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xCollectibleMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xCollectibleMaterial.GetDirect()->SetName("MarbleCollectible");
	Resources().m_xCollectibleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCollectibleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	Resources().m_xFloorMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xFloorMaterial.GetDirect()->SetName("MarbleFloor");
	Resources().m_xFloorMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xFloorMaterial.GetDirect()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates with only TransformComponent
	// ModelComponent and ColliderComponent are added AFTER setting position/scale
	// (ColliderComponent creates physics bodies - must be added after transform is set)
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Ball prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xBallTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "BallTemplate");

		auto xhBall = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxBall = xhBall.GetDirect();
		pxBall->CreateFromEntity(xBallTemplate, "Ball");
		Resources().m_xBallPrefab.Set(pxBall);

		xBallTemplate.Destroy();
	}

	// Platform prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xPlatformTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "PlatformTemplate");

		auto xhPlatform = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxPlatform = xhPlatform.GetDirect();
		pxPlatform->CreateFromEntity(xPlatformTemplate, "Platform");
		Resources().m_xPlatformPrefab.Set(pxPlatform);

		xPlatformTemplate.Destroy();
	}

	// Goal prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xGoalTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "GoalTemplate");

		auto xhGoal = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxGoal = xhGoal.GetDirect();
		pxGoal->CreateFromEntity(xGoalTemplate, "Goal");
		Resources().m_xGoalPrefab.Set(pxGoal);

		xGoalTemplate.Destroy();
	}

	// Collectible prefab - basic entity (ModelComponent added at runtime, no collider - uses distance check)
	{
		Zenith_Entity xCollectibleTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "CollectibleTemplate");

		auto xhCollectible = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxCollectible = xhCollectible.GetDirect();
		pxCollectible->CreateFromEntity(xCollectibleTemplate, "Collectible");
		Resources().m_xCollectiblePrefab.Set(pxCollectible);

		xCollectibleTemplate.Destroy();
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Marble";
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
	InitializeMarbleResources();

	// Register the Marble game component with the component-meta registry
	// (serialization/lifecycle) and, in tools builds, the editor "Add Component"
	// registry (display name used by AddStep_AddComponent / the editor menu).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.RegisterComponent<Marble_GameComponent>("MarbleGame", 100);
	Marble_RegisterGraphNodes();
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<Marble_GameComponent>("MarbleGame");
#endif
}

void Project_Shutdown()
{
	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	Marble::Resources().m_xSphereAsset.Clear();
	Marble::Resources().m_xCubeAsset.Clear();
	Marble::Resources().m_xBallMaterial.Clear();
	Marble::Resources().m_xPlatformMaterial.Clear();
	Marble::Resources().m_xGoalMaterial.Clear();
	Marble::Resources().m_xCollectibleMaterial.Clear();
	Marble::Resources().m_xFloorMaterial.Clear();
	Marble::Resources().m_xBallPrefab.Clear();
	Marble::Resources().m_xPlatformPrefab.Clear();
	Marble::Resources().m_xGoalPrefab.Clear();
	Marble::Resources().m_xCollectiblePrefab.Clear();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All Marble resources initialized in Project_RegisterGameComponents
}

namespace
{
	// Marble_LevelFlow: the gameplay GameManager's graph. OWNS the level's
	// game state (gameState/timeRemaining/score/collected declared here);
	// C++ reads it back through the Marble_GameComponent accessors.
	//
	// Anchors, in dispatch order:
	//   OnStart              - show the HUD (the old StartGame visibility).
	//   OnCustomEvent chains - fired by the component once per PLAYING frame
	//       ("LevelTick", dt payload). Custom-event sources run in node
	//       order, preserving the old same-frame decision order:
	//       stage frame facts -> timer -> collection -> fall.
	//   OnKeyPressed chains  - the old top-of-PLAYING key decisions (run on
	//       the graph's ON_UPDATE dispatch, which precedes the component's
	//       update - same-frame, keys-before-tick like the C++).
	//   StateMachine (LAST)  - reactive dispatcher on gameState; its
	//       MarbleEnter_Paused / MarbleExit_Paused transition events drive
	//       the level-scene pause. Placed after the key chains so a key-
	//       driven transition's side effects land the same dispatch.
	void BuildGraph_MarbleLevelFlow(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(MarbleGameState::PLAYING));
		xBuilder.Variable("gameState", xValue);
		xValue.SetFloat(s_fInitialTime);
		xBuilder.Variable("timeRemaining", xValue);
		xValue.SetInt32(0);
		xBuilder.Variable("score", xValue);
		xBuilder.Variable("collected", xValue);

		// ---- OnStart: show the HUD ----------------------------------------
		const char* aszHUD[] = { "Title", "Score", "Time", "Collected", "Controls", "Status" };
		u_int uPrevious = xBuilder.Node("OnStart");
		for (const char* szElement : aszHUD)
		{
			const u_int uShow = xBuilder.Node("SetUIVisible");
			xBuilder.ParamString(uShow, "m_strElement", szElement);
			xBuilder.ParamBool(uShow, "m_bVisible", true);
			xBuilder.Chain(uPrevious, uShow);
			uPrevious = uShow;
		}

		// ---- LevelTick 1: publish the frame's systems results --------------
		const u_int uTickStage = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickStage, "m_strEventName", "LevelTick");
		const u_int uStage = xBuilder.Node("MarbleStageFrameResults");
		xBuilder.Chain(uTickStage, uStage);

		// ---- LevelTick 2: timer countdown -> LOST at expiry -----------------
		// (custom-event dispatch carries no dt; the payload var is the dt)
		const u_int uTickTimer = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickTimer, "m_strEventName", "LevelTick");
		const u_int uSubDt = xBuilder.Node("MathBlackboardFloat");
		xBuilder.ParamString(uSubDt, "m_strVar", "timeRemaining");
		xBuilder.ParamInt(uSubDt, "m_iOp", 0);	// sub
		xBuilder.ParamString(uSubDt, "m_strOperandVar", "payload");
		const u_int uClamp = xBuilder.Node("ClampBlackboardFloat");
		xBuilder.ParamString(uClamp, "m_strVar", "timeRemaining");
		xBuilder.ParamFloat(uClamp, "m_fMin", 0.0f);
		xBuilder.ParamFloat(uClamp, "m_fMax", 3600.0f);
		const u_int uCmpExpired = xBuilder.Node("CompareBlackboardFloat");
		xBuilder.ParamString(uCmpExpired, "m_strVar", "timeRemaining");
		xBuilder.ParamFloat(uCmpExpired, "m_fCompareTo", 0.0f);
		xBuilder.ParamInt(uCmpExpired, "m_iOp", 1);	// lessEqual
		xBuilder.ParamString(uCmpExpired, "m_strResultVar", "timeExpired");
		const u_int uBrExpired = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrExpired, "m_strConditionVar", "timeExpired");
		const u_int uTimerLost = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uTimerLost, "m_strVariable", "gameState");
		xBuilder.ParamInt(uTimerLost, "m_iValue", static_cast<int32_t>(MarbleGameState::LOST));
		xBuilder.Chain(uTickTimer, uSubDt).Chain(uSubDt, uClamp).Chain(uClamp, uCmpExpired).Chain(uCmpExpired, uBrExpired);
		xBuilder.Edge(uBrExpired, 0, uTimerLost);

		// ---- LevelTick 3: collection accumulation -> WON --------------------
		const u_int uTickCollect = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickCollect, "m_strEventName", "LevelTick");
		const u_int uAddScore = xBuilder.Node("AddBlackboardInt");
		xBuilder.ParamString(uAddScore, "m_strVariable", "score");
		xBuilder.ParamString(uAddScore, "m_strDeltaVar", "scoreGained");
		const u_int uAddCollected = xBuilder.Node("AddBlackboardInt");
		xBuilder.ParamString(uAddCollected, "m_strVariable", "collected");
		xBuilder.ParamString(uAddCollected, "m_strDeltaVar", "collectedDelta");
		const u_int uBrAll = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrAll, "m_strConditionVar", "allCollected");
		const u_int uWon = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uWon, "m_strVariable", "gameState");
		xBuilder.ParamInt(uWon, "m_iValue", static_cast<int32_t>(MarbleGameState::WON));
		xBuilder.Chain(uTickCollect, uAddScore).Chain(uAddScore, uAddCollected).Chain(uAddCollected, uBrAll);
		xBuilder.Edge(uBrAll, 0, uWon);

		// ---- LevelTick 4: fall -> LOST (last word, like the old chain end) --
		const u_int uTickFall = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickFall, "m_strEventName", "LevelTick");
		const u_int uBrFell = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrFell, "m_strConditionVar", "ballFell");
		const u_int uFallLost = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uFallLost, "m_strVariable", "gameState");
		xBuilder.ParamInt(uFallLost, "m_iValue", static_cast<int32_t>(MarbleGameState::LOST));
		xBuilder.Chain(uTickFall, uBrFell);
		xBuilder.Edge(uBrFell, 0, uFallLost);

		// ---- P: pause toggle (PLAYING <-> PAUSED) ---------------------------
		// The scene pause itself is the StateMachine's transition side effect.
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_P);
			const u_int uSwitch = xBuilder.Node("SwitchOnInt");
			xBuilder.ParamString(uSwitch, "m_strVar", "gameState");
			xBuilder.ParamInt(uSwitch, "m_iCaseCount", 5);
			xBuilder.Chain(uKey, uSwitch);
			const u_int uToPaused = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uToPaused, "m_strVariable", "gameState");
			xBuilder.ParamInt(uToPaused, "m_iValue", static_cast<int32_t>(MarbleGameState::PAUSED));
			xBuilder.Edge(uSwitch, static_cast<u_int>(MarbleGameState::PLAYING), uToPaused);
			const u_int uToPlaying = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uToPlaying, "m_strVariable", "gameState");
			xBuilder.ParamInt(uToPlaying, "m_iValue", static_cast<int32_t>(MarbleGameState::PLAYING));
			xBuilder.Edge(uSwitch, static_cast<u_int>(MarbleGameState::PAUSED), uToPlaying);
		}

		// ---- Esc: pause from PLAYING (the WasPausePressed P-or-Esc quirk);
		// ---- menu from PAUSED/WON/LOST --------------------------------------
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_ESCAPE);
			const u_int uSwitch = xBuilder.Node("SwitchOnInt");
			xBuilder.ParamString(uSwitch, "m_strVar", "gameState");
			xBuilder.ParamInt(uSwitch, "m_iCaseCount", 5);
			xBuilder.Chain(uKey, uSwitch);

			const u_int uToPaused = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uToPaused, "m_strVariable", "gameState");
			xBuilder.ParamInt(uToPaused, "m_iValue", static_cast<int32_t>(MarbleGameState::PAUSED));
			xBuilder.Edge(uSwitch, static_cast<u_int>(MarbleGameState::PLAYING), uToPaused);

			// PAUSED: resume first (the C++ fell through WasPausePressed),
			// then teardown + menu. SINGLE loads sit at the END of the chain.
			const u_int uResume = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uResume, "m_strVariable", "gameState");
			xBuilder.ParamInt(uResume, "m_iValue", static_cast<int32_t>(MarbleGameState::PLAYING));
			const u_int uUnloadPaused = xBuilder.Node("MarbleUnloadLevel");
			const u_int uMenuPaused = xBuilder.Node("LoadSceneByIndex");
			xBuilder.ParamInt(uMenuPaused, "m_iSceneIndex", 0);
			xBuilder.Edge(uSwitch, static_cast<u_int>(MarbleGameState::PAUSED), uResume);
			xBuilder.Chain(uResume, uUnloadPaused).Chain(uUnloadPaused, uMenuPaused);

			const u_int auEndStates[] = {
				static_cast<u_int>(MarbleGameState::WON), static_cast<u_int>(MarbleGameState::LOST) };
			for (const u_int uState : auEndStates)
			{
				const u_int uUnload = xBuilder.Node("MarbleUnloadLevel");
				const u_int uMenu = xBuilder.Node("LoadSceneByIndex");
				xBuilder.ParamInt(uMenu, "m_iSceneIndex", 0);
				xBuilder.Edge(uSwitch, uState, uUnload);
				xBuilder.Chain(uUnload, uMenu);
			}
		}

		// ---- R: reset (PLAYING/WON/LOST - every level state except PAUSED) --
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_R);
			const u_int uCanReset = xBuilder.Node("CompareBlackboardInt");
			xBuilder.ParamString(uCanReset, "m_strVar", "gameState");
			xBuilder.ParamInt(uCanReset, "m_iCompareTo", static_cast<int32_t>(MarbleGameState::PAUSED));
			xBuilder.ParamInt(uCanReset, "m_iOp", 5);	// notEqual
			xBuilder.ParamString(uCanReset, "m_strResultVar", "canReset");
			const u_int uBrReset = xBuilder.Node("Branch");
			xBuilder.ParamString(uBrReset, "m_strConditionVar", "canReset");
			const u_int uRegen = xBuilder.Node("MarbleRegenerateLevel");
			const u_int uResetTime = xBuilder.Node("SetBlackboardFloat");
			xBuilder.ParamString(uResetTime, "m_strVariable", "timeRemaining");
			xBuilder.ParamFloat(uResetTime, "m_fValue", s_fInitialTime);
			const u_int uResetScore = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uResetScore, "m_strVariable", "score");
			xBuilder.ParamInt(uResetScore, "m_iValue", 0);
			const u_int uResetCollected = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uResetCollected, "m_strVariable", "collected");
			xBuilder.ParamInt(uResetCollected, "m_iValue", 0);
			const u_int uResetState = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uResetState, "m_strVariable", "gameState");
			xBuilder.ParamInt(uResetState, "m_iValue", static_cast<int32_t>(MarbleGameState::PLAYING));
			xBuilder.Chain(uKey, uCanReset).Chain(uCanReset, uBrReset);
			xBuilder.Edge(uBrReset, 0, uRegen);
			xBuilder.Chain(uRegen, uResetTime).Chain(uResetTime, uResetScore)
				.Chain(uResetScore, uResetCollected).Chain(uResetCollected, uResetState);
		}

		// ---- Pause side effects: StateMachine transition events -------------
		{
			const u_int uEnterPaused = xBuilder.Node("OnCustomEvent");
			xBuilder.ParamString(uEnterPaused, "m_strEventName", "MarbleEnter_Paused");
			const u_int uDoPause = xBuilder.Node("MarbleSetLevelPaused");
			xBuilder.ParamBool(uDoPause, "m_bPaused", true);
			xBuilder.Chain(uEnterPaused, uDoPause);

			const u_int uExitPaused = xBuilder.Node("OnCustomEvent");
			xBuilder.ParamString(uExitPaused, "m_strEventName", "MarbleExit_Paused");
			const u_int uDoResume = xBuilder.Node("MarbleSetLevelPaused");
			xBuilder.ParamBool(uDoResume, "m_bPaused", false);
			xBuilder.Chain(uExitPaused, uDoResume);
		}

		// ---- Reactive StateMachine, LAST in the ON_UPDATE dispatch ----------
		// State bodies stay empty (per-state per-frame work is the C++
		// systems dispatch); the machine exists for its transition events.
		{
			const u_int uTick = xBuilder.Node("OnUpdate");
			const u_int uMachine = xBuilder.Node("StateMachine");
			xBuilder.ParamString(uMachine, "m_strStateVar", "gameState");
			xBuilder.ParamInt(uMachine, "m_iStateCount", 5);
			xBuilder.ParamString(uMachine, "m_strStateNames", "Menu,Playing,Paused,Won,Lost");
			xBuilder.ParamString(uMachine, "m_strEventPrefix", "Marble");
			xBuilder.Chain(uTick, uMachine);
		}
	}

	// Marble_GameFlow: the menu GameManager's graph. gameState pinned at
	// MAIN_MENU for the shim accessors; the Play button drives the scene
	// load through the engine's UIButton trampoline source; W/S/Up/Down
	// toggle the keyboard focus (two buttons - every direction is a toggle,
	// exactly the old (i +- 1 + 2) % 2). MenuQuit stays deliberately
	// unwired - the pre-conversion quirk, preserved structurally.
	void BuildGraph_MarbleGameFlow(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(MarbleGameState::MAIN_MENU));
		xBuilder.Variable("gameState", xValue);
		xValue.SetInt32(0);
		xBuilder.Variable("focusIndex", xValue);

		// Play click -> gameplay scene (SINGLE load at the end of the chain).
		const u_int uPlayClicked = xBuilder.Node("OnUIButtonClicked");
		xBuilder.ParamString(uPlayClicked, "m_strButton", "MenuPlay");
		const u_int uLoadGame = xBuilder.Node("LoadSceneByIndex");
		xBuilder.ParamInt(uLoadGame, "m_iSceneIndex", 1);
		xBuilder.Chain(uPlayClicked, uLoadGame);

		// Focus toggles: each key flips focusIndex 0 <-> 1.
		const int32_t aiKeys[] = { ZENITH_KEY_W, ZENITH_KEY_UP, ZENITH_KEY_S, ZENITH_KEY_DOWN };
		for (const int32_t iKey : aiKeys)
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", iKey);
			const u_int uIsZero = xBuilder.Node("CompareBlackboardInt");
			xBuilder.ParamString(uIsZero, "m_strVar", "focusIndex");
			xBuilder.ParamInt(uIsZero, "m_iCompareTo", 0);
			xBuilder.ParamInt(uIsZero, "m_iOp", 4);	// equal
			xBuilder.ParamString(uIsZero, "m_strResultVar", "focusIsZero");
			const u_int uBranch = xBuilder.Node("Branch");
			xBuilder.ParamString(uBranch, "m_strConditionVar", "focusIsZero");
			const u_int uSetOne = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uSetOne, "m_strVariable", "focusIndex");
			xBuilder.ParamInt(uSetOne, "m_iValue", 1);
			const u_int uSetZero = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uSetZero, "m_strVariable", "focusIndex");
			xBuilder.ParamInt(uSetZero, "m_iValue", 0);
			xBuilder.Chain(uKey, uIsZero).Chain(uIsZero, uBranch);
			xBuilder.Edge(uBranch, 0, uSetOne);
			xBuilder.Edge(uBranch, 1, uSetZero);
		}

		// Per-frame focus visuals (the SetFocused half of UpdateMenuInput).
		const u_int uTick = xBuilder.Node("OnUpdate");
		const u_int uApply = xBuilder.Node("MarbleApplyMenuFocus");
		xBuilder.Chain(uTick, uApply);
	}
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- Behaviour graphs (regenerated every boot through the programmatic
	// ---- builder - the W1 conversion's decomposed engine-node graphs) ----
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.AddStep_GraphBuild("game:Graphs/Marble_LevelFlow.bgraph", &BuildGraph_MarbleLevelFlow);
	xAuto.AddStep_GraphBuild("game:Graphs/Marble_GameFlow.bgraph", &BuildGraph_MarbleGameFlow);

	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 8.f, -12.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.4f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "MARBLE ROLL");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 90.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 0.4f, 0.6f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuQuit", "Quit");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuQuit", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuQuit", 0.f, 70.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuQuit", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_AddComponent("MarbleGame");
	// Menu flow graph (Play click + focus toggles + MAIN_MENU state).
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Marble_GameFlow.bgraph");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Marble gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Marble");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 8.f, -12.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.4f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// UI layout constants (matching original): marginLeft=30, marginTop=30, baseTextSize=15, lineHeight=24
	// Title: y=30+0=30, fontSize=15*4.8=72
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Title", "MARBLE ROLL");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Title", 30.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Title", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Title", 72.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);

	// Score: y=30+72=102, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Score", "Score: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Score", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Score", 30.f, 102.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Score", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Score", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Score", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Score", 0.6f, 0.8f, 1.f, 1.f);

	// Time: y=30+96=126, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Time", "Time: 60.0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Time", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Time", 30.f, 126.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Time", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Time", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Time", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Time", 0.6f, 0.8f, 1.f, 1.f);

	// Collected: y=30+120=150, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Collected", "Collected: 0 / 5");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Collected", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Collected", 30.f, 150.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Collected", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Collected", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Collected", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Collected", 0.6f, 0.8f, 1.f, 1.f);

	// Controls: y=30+168=198, fontSize=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Controls", "WASD: Move | Space: Jump | R: Reset | Esc: Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Controls", 30.f, 198.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Controls", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Controls", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.f);

	// Status: Center anchor, (0,0), Center alignment, fontSize=15*6=90
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 90.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.2f, 1.f, 0.2f, 1.f);

	g_xEngine.EditorAutomation().AddStep_AddComponent("MarbleGame");
	// Level-flow graph on the gameplay GameManager: MarbleGame fires
	// "LevelTick" into it from the PLAYING branch of its OnUpdate.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Marble_LevelFlow.bgraph");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Marble" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Marble" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
