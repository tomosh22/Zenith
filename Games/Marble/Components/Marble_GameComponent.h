#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Marble_GameComponent.h - Main game coordinator
 *
 * This file orchestrates the Marble Ball Game using modular components.
 * Each module handles a specific responsibility:
 *
 * - Marble_Input.h           - Camera-relative input handling
 * - Marble_PhysicsController.h - Physics-based ball movement
 * - Marble_CameraFollow.h    - Smooth camera following
 * - Marble_LevelGenerator.h  - Procedural level generation
 * - Marble_CollectibleSystem.h - Pickup detection and scoring
 * - Marble_UIManager.h       - HUD management
 *
 * Key Engine Features Demonstrated:
 * - Game ECS component lifecycle hooks (OnAwake, OnStart, OnUpdate -
 *   concept-detected by the component-meta registry)
 * - Jolt Physics integration via Zenith_ColliderComponent
 * - Camera-relative input with continuous polling (IsKeyDown)
 * - Prefab-based entity instantiation
 * - Component order dependencies (Transform before Collider)
 * - Multi-scene architecture (persistent GameManager + level scene)
 * - Zenith_UIButton for clickable/tappable menu
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "UI/Zenith_UIButton.h"

// Include extracted modules
#include "Marble_Input.h"
#include "Marble_PhysicsController.h"
#include "Marble_CameraFollow.h"
#include "Marble_LevelGenerator.h"
#include "Marble_CollectibleSystem.h"
#include "Marble_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// Marble Resources - Phase 8 per-game ProjectResources struct.
// ============================================================================
class Zenith_Prefab;

namespace Marble
{
	struct MarbleResources
	{
		MeshGeometryHandle  m_xSphereAsset;
		MeshGeometryHandle  m_xCubeAsset;
		Flux_MeshGeometry*  m_pxSphereGeometry = nullptr;
		Flux_MeshGeometry*  m_pxCubeGeometry   = nullptr;
		MaterialHandle      m_xBallMaterial;
		MaterialHandle      m_xPlatformMaterial;
		MaterialHandle      m_xGoalMaterial;
		MaterialHandle      m_xCollectibleMaterial;
		MaterialHandle      m_xFloorMaterial;

		PrefabHandle        m_xBallPrefab;
		PrefabHandle        m_xPlatformPrefab;
		PrefabHandle        m_xGoalPrefab;
		PrefabHandle        m_xCollectiblePrefab;
	};

	MarbleResources& Resources();
}

// ============================================================================
// Game Configuration
// ============================================================================
static constexpr float s_fBallRadius = 0.5f;
static constexpr float s_fInitialTime = 60.0f;

// MarbleGameState is defined in Marble_UIManager.h

/**
 * Marble_GameComponent - Main game coordinator
 *
 * Architecture:
 * - GameManager entity (camera + UI + game component) per scene
 * - Level scene created/destroyed on transitions via CreateEmptyScene/UnloadScene
 *
 * State machine: MAIN_MENU -> PLAYING -> PAUSED / WON / LOST -> MAIN_MENU
 */
class Marble_GameComponent
{
public:
	Marble_GameComponent() = delete;
	Marble_GameComponent(Zenith_Entity& xParentEntity)
		: m_eGameState(MarbleGameState::MAIN_MENU)
		, m_uScore(0)
		, m_fTimeRemaining(s_fInitialTime)
		, m_uCollectedCount(0)
		, m_xRng(std::random_device{}())
		, m_iFocusIndex(0)
		, m_xParentEntity(xParentEntity)
	{
	}
	~Marble_GameComponent() = default;

	void OnAwake()
	{
		// Cache resource pointers
		m_pxSphereGeometry = Marble::Resources().m_pxSphereGeometry;
		m_pxCubeGeometry = Marble::Resources().m_pxCubeGeometry;
		m_xBallMaterial = Marble::Resources().m_xBallMaterial;
		m_xPlatformMaterial = Marble::Resources().m_xPlatformMaterial;
		m_xGoalMaterial = Marble::Resources().m_xGoalMaterial;
		m_xCollectibleMaterial = Marble::Resources().m_xCollectibleMaterial;
		m_xFloorMaterial = Marble::Resources().m_xFloorMaterial;

		// Wire menu button callbacks
		bool bHasMenu = false;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay)
			{
				pxPlay->SetOnClick(&OnPlayClicked, nullptr);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			// Start in menu state
			m_eGameState = MarbleGameState::MAIN_MENU;
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	void OnStart()
	{
		if (m_eGameState == MarbleGameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDt)
	{
		switch (m_eGameState)
		{
		case MarbleGameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case MarbleGameState::PLAYING:
			if (Marble_Input::WasPausePressed())
			{
				m_eGameState = MarbleGameState::PAUSED;
				g_xEngine.Scenes().SetScenePaused(m_xLevelScene, true);
				UpdateUI();
				return;
			}
			if (Marble_Input::WasResetPressed())
			{
				ResetLevel();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}

			// Timer
			m_fTimeRemaining -= fDt;
			if (m_fTimeRemaining <= 0.0f)
			{
				m_fTimeRemaining = 0.0f;
				m_eGameState = MarbleGameState::LOST;
			}

			HandleInput(fDt);
			HandleCollectibles(fDt);
			CheckFallCondition();
			UpdateCamera(fDt);
			UpdateUI();
			break;

		case MarbleGameState::PAUSED:
			if (Marble_Input::WasPausePressed())
			{
				m_eGameState = MarbleGameState::PLAYING;
				g_xEngine.Scenes().SetScenePaused(m_xLevelScene, false);
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;

		case MarbleGameState::WON:
		case MarbleGameState::LOST:
			if (Marble_Input::WasResetPressed())
			{
				ResetLevel();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateCamera(fDt);
			UpdateUI();
			break;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Marble Ball Game");
		ImGui::Separator();

		const char* szStates[] = { "MENU", "PLAYING", "PAUSED", "WON", "LOST" };
		ImGui::Text("State: %s", szStates[static_cast<int>(m_eGameState)]);

		if (m_eGameState != MarbleGameState::MAIN_MENU)
		{
			ImGui::Text("Score: %u", m_uScore);
			ImGui::Text("Time: %.1f", m_fTimeRemaining);
			ImGui::Text("Collected: %u / %u", m_uCollectedCount,
				static_cast<uint32_t>(m_xLevelEntities.axCollectibleEntityIDs.size()) + m_uCollectedCount);
		}

		if (m_eGameState == MarbleGameState::MAIN_MENU)
		{
			if (ImGui::Button("Start Game"))
				StartGame();
		}
		else
		{
			if (ImGui::Button("Reset Level"))
				ResetLevel();
			if (ImGui::Button("Return to Menu"))
				ReturnToMenu();
		}
	}
#endif

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload (byte-identical to the pre-migration parameter block,
		// including its own internal version field).
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fTimeRemaining;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_fTimeRemaining;
		}
	}

private:
	// ========================================================================
	// Menu Button Callbacks
	// ========================================================================
	static void OnPlayClicked(void* /*pxUserData*/)
	{
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// State Transitions
	// ========================================================================
	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create level scene
		m_xLevelScene = g_xEngine.Scenes().LoadScene("Level", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xLevelScene);

		// Generate level (uses GetActiveScene internally)
		Marble_LevelGenerator::GenerateLevel(
			m_xLevelEntities,
			m_xRng,
			Marble::Resources().m_xBallPrefab.GetDirect(),
			Marble::Resources().m_xPlatformPrefab.GetDirect(),
			Marble::Resources().m_xGoalPrefab.GetDirect(),
			Marble::Resources().m_xCollectiblePrefab.GetDirect(),
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xBallMaterial.GetDirect(),
			m_xPlatformMaterial.GetDirect(),
			m_xGoalMaterial.GetDirect(),
			m_xCollectibleMaterial.GetDirect());

		// Reset game state
		m_eGameState = MarbleGameState::PLAYING;
		m_uScore = 0;
		m_fTimeRemaining = s_fInitialTime;
		m_uCollectedCount = 0;
	}

	void ReturnToMenu()
	{
		ClearEntityReferences();

		if (m_xLevelScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}

		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ResetLevel()
	{
		ClearEntityReferences();

		if (m_xLevelScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}

		// Create fresh level scene
		m_xLevelScene = g_xEngine.Scenes().LoadScene("Level", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xLevelScene);

		// Generate level
		Marble_LevelGenerator::GenerateLevel(
			m_xLevelEntities,
			m_xRng,
			Marble::Resources().m_xBallPrefab.GetDirect(),
			Marble::Resources().m_xPlatformPrefab.GetDirect(),
			Marble::Resources().m_xGoalPrefab.GetDirect(),
			Marble::Resources().m_xCollectiblePrefab.GetDirect(),
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xBallMaterial.GetDirect(),
			m_xPlatformMaterial.GetDirect(),
			m_xGoalMaterial.GetDirect(),
			m_xCollectibleMaterial.GetDirect());

		m_eGameState = MarbleGameState::PLAYING;
		m_uScore = 0;
		m_fTimeRemaining = s_fInitialTime;
		m_uCollectedCount = 0;
	}

	void ClearEntityReferences()
	{
		m_xLevelEntities.uBallEntityID = INVALID_ENTITY_ID;
		m_xLevelEntities.uGoalEntityID = INVALID_ENTITY_ID;
		m_xLevelEntities.axPlatformEntityIDs.clear();
		m_xLevelEntities.axCollectibleEntityIDs.clear();
	}

	// ========================================================================
	// Menu UI
	// ========================================================================
	void SetMenuVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
		Zenith_UI::Zenith_UIButton* pxQuit = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit");
		if (pxQuit) pxQuit->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszElements[] = { "Title", "Score", "Time", "Collected", "Controls", "Status" };
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		static constexpr int32_t s_iButtonCount = 2;

		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_UP) || g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_W))
			m_iFocusIndex = (m_iFocusIndex - 1 + s_iButtonCount) % s_iButtonCount;
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_DOWN) || g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_S))
			m_iFocusIndex = (m_iFocusIndex + 1) % s_iButtonCount;

		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		Zenith_UI::Zenith_UIButton* pxQuit = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit");
		if (pxPlay) pxPlay->SetFocused(m_iFocusIndex == 0);
		if (pxQuit) pxQuit->SetFocused(m_iFocusIndex == 1);
	}

	// ========================================================================
	// Input and Physics (delegates to modules)
	// ========================================================================
	void HandleInput(float /*fDt*/)
	{
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		if (!xBall.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();

		// Get camera from persistent scene
		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		// Get positions for input calculation
		Zenith_Maths::Vector3 xCamPos, xBallPos;
		pxCamera->GetPosition(xCamPos);
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		// Get camera-relative movement direction
		Zenith_Maths::Vector3 xDirection = Marble_Input::GetMovementDirection(xCamPos, xBallPos);

		// Apply movement
		Marble_PhysicsController::ApplyMovement(xCollider, xDirection);

		// Handle jump
		if (Marble_Input::WasJumpPressed())
		{
			Marble_PhysicsController::TryJump(xCollider);
		}
	}

	void CheckFallCondition()
	{
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		if (Marble_PhysicsController::HasFallenOff(xBallPos))
		{
			m_eGameState = MarbleGameState::LOST;
		}
	}

	// ========================================================================
	// Camera (delegates to Marble_CameraFollow)
	// ========================================================================
	void UpdateCamera(float fDt)
	{
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		Marble_CameraFollow::Update(*pxCamera, xBallPos, fDt);
	}

	// ========================================================================
	// Collectibles (delegates to Marble_CollectibleSystem)
	// ========================================================================
	void HandleCollectibles(float fDt)
	{
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		// Check for collections (uses GetActiveScene internally - still works)
		Marble_CollectibleSystem::CollectionResult xResult =
			Marble_CollectibleSystem::CheckCollectibles(xBallPos, m_xLevelEntities.axCollectibleEntityIDs, m_uCollectedCount);

		m_uScore += xResult.uScoreGained;
		m_uCollectedCount += xResult.uCollectedCount;

		if (xResult.bAllCollected)
		{
			m_eGameState = MarbleGameState::WON;
		}

		// Animate collectibles (uses GetActiveScene internally - still works)
		Marble_CollectibleSystem::UpdateCollectibleRotation(m_xLevelEntities.axCollectibleEntityIDs, fDt);
	}

	// ========================================================================
	// UI (delegates to Marble_UIManager)
	// ========================================================================
	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		uint32_t uTotalCollectibles = static_cast<uint32_t>(m_xLevelEntities.axCollectibleEntityIDs.size()) + m_uCollectedCount;

		Marble_UIManager::UpdateUI(
			xUI,
			m_uScore,
			m_fTimeRemaining,
			m_uCollectedCount,
			uTotalCollectibles,
			m_eGameState);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	MarbleGameState m_eGameState;
	uint32_t m_uScore;
	float m_fTimeRemaining;
	uint32_t m_uCollectedCount;

	// Level entities (managed by Marble_LevelGenerator)
	Marble_LevelGenerator::LevelEntities m_xLevelEntities;

	// Scene handle for the level scene
	Zenith_Scene m_xLevelScene;

	// Random number generator
	std::mt19937 m_xRng;

	// Menu keyboard focus
	int32_t m_iFocusIndex;

	// Owning entity (explicit member now - was provided by the old script base)
	Zenith_Entity m_xParentEntity;

public:
	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	MaterialHandle m_xBallMaterial;
	MaterialHandle m_xPlatformMaterial;
	MaterialHandle m_xGoalMaterial;
	MaterialHandle m_xCollectibleMaterial;
	MaterialHandle m_xFloorMaterial;
};
