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
		Zenith_MeshGeometryAsset* pxSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxSphereGeom = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphereGeom, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphereGeom);
		Resources().m_xSphereAsset.Set(pxSphereAsset);
		Resources().m_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}

	// Create cube geometry (uses cached unit cube)
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials.
	// Copying the handle by value AddRefs; each material owns a ref via its own handle copy.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with grid texture and BaseColor
	Resources().m_xBallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xBallMaterial.GetDirect()->SetName("MarbleBall");
	Resources().m_xBallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xBallMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	Resources().m_xPlatformMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xPlatformMaterial.GetDirect()->SetName("MarblePlatform");
	Resources().m_xPlatformMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xPlatformMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 102.f/255.f, 102.f/255.f, 1.f });

	Resources().m_xGoalMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xGoalMaterial.GetDirect()->SetName("MarbleGoal");
	Resources().m_xGoalMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xGoalMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xCollectibleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCollectibleMaterial.GetDirect()->SetName("MarbleCollectible");
	Resources().m_xCollectibleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCollectibleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	Resources().m_xFloorMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
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

		Zenith_Prefab* pxBall = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxBall->CreateFromEntity(xBallTemplate, "Ball");
		Resources().m_xBallPrefab.Set(pxBall);

		xBallTemplate.Destroy();
	}

	// Platform prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xPlatformTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "PlatformTemplate");

		Zenith_Prefab* pxPlatform = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxPlatform->CreateFromEntity(xPlatformTemplate, "Platform");
		Resources().m_xPlatformPrefab.Set(pxPlatform);

		xPlatformTemplate.Destroy();
	}

	// Goal prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xGoalTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "GoalTemplate");

		Zenith_Prefab* pxGoal = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxGoal->CreateFromEntity(xGoalTemplate, "Goal");
		Resources().m_xGoalPrefab.Set(pxGoal);

		xGoalTemplate.Destroy();
	}

	// Collectible prefab - basic entity (ModelComponent added at runtime, no collider - uses distance check)
	{
		Zenith_Entity xCollectibleTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "CollectibleTemplate");

		Zenith_Prefab* pxCollectible = Zenith_AssetRegistry::Create<Zenith_Prefab>();
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

void Project_RegisterEditorAutomationSteps()
{
	// ---- Behaviour graphs (regenerated every boot, like the scenes) --------
	// Wave-2 conversion: the timer countdown + win/lose decisions live here;
	// Marble_GameComponent fires "LevelTick" (dt payload) once per PLAYING
	// frame after its systems pass, and the chain preserves the old
	// same-frame decision order (timer -> collection -> fall).
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.AddStep_GraphOpenFresh("game:Graphs/Marble_LevelFlow.bgraph");
	xAuto.AddStep_GraphAddNode("OnCustomEvent");
	xAuto.AddStep_GraphSelectNode("OnCustomEvent", 0);
	xAuto.AddStep_GraphSetNodeParamString("m_strEventName", "LevelTick");
	xAuto.AddStep_GraphAddNode("MarbleTickTimer");
	xAuto.AddStep_GraphConnect("OnCustomEvent", 0, 0, "MarbleTickTimer", 0);
	xAuto.AddStep_GraphAddNode("MarbleApplyCollection");
	xAuto.AddStep_GraphConnect("MarbleTickTimer", 0, 0, "MarbleApplyCollection", 0);
	xAuto.AddStep_GraphAddNode("MarbleCheckFall");
	xAuto.AddStep_GraphConnect("MarbleApplyCollection", 0, 0, "MarbleCheckFall", 0);
	xAuto.AddStep_GraphSave();
	xAuto.AddStep_GraphClose();

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
