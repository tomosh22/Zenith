#pragma once
/**
 * Marble_Behaviour.h - Main game coordinator
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
 * - Zenith_ScriptBehaviour lifecycle (OnAwake, OnStart, OnUpdate)
 * - Jolt Physics integration via Zenith_ColliderComponent
 * - Camera-relative input with continuous polling (IsKeyHeld)
 * - Prefab-based entity instantiation
 * - Component order dependencies (Transform before Collider)
 * - Multi-scene architecture (persistent GameManager + level scene)
 * - Zenith_UIButton for clickable/tappable menu
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
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
// Marble Resources - Global access
// Defined in Marble.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
class Zenith_Prefab;

namespace Marble
{
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern MaterialHandle g_xBallMaterial;
	extern MaterialHandle g_xPlatformMaterial;
	extern MaterialHandle g_xGoalMaterial;
	extern MaterialHandle g_xCollectibleMaterial;
	extern MaterialHandle g_xFloorMaterial;

	// Prefabs for runtime instantiation
	extern Zenith_Prefab* g_pxBallPrefab;
	extern Zenith_Prefab* g_pxPlatformPrefab;
	extern Zenith_Prefab* g_pxGoalPrefab;
	extern Zenith_Prefab* g_pxCollectiblePrefab;
}

// ============================================================================
// Game Configuration
// ============================================================================
static constexpr float s_fBallRadius = 0.5f;
static constexpr float s_fInitialTime = 60.0f;

// MarbleGameState is defined in Marble_UIManager.h

/**
 * Marble_Behaviour - Main game coordinator
 *
 * Architecture:
 * - Persistent GameManager entity (camera + UI + script) in DontDestroyOnLoad scene
 * - Level scene created/destroyed on transitions via CreateEmptyScene/UnloadScene
 *
 * State machine: MAIN_MENU -> PLAYING -> PAUSED / WON / LOST -> MAIN_MENU
 */
class Marble_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Marble_Behaviour)

	Marble_Behaviour() = delete;
	Marble_Behaviour(Zenith_Entity&)
		: m_eGameState(MarbleGameState::MAIN_MENU)
		, m_uScore(0)
		, m_fTimeRemaining(s_fInitialTime)
		, m_uCollectedCount(0)
		, m_xRng(std::random_device{}())
		, m_iFocusIndex(0)
	{
	}
	~Marble_Behaviour() = default;

	void OnAwake() ZENITH_FINAL override
	{
		// Cache resource pointers
		m_pxSphereGeometry = Marble::g_pxSphereGeometry;
		m_pxCubeGeometry = Marble::g_pxCubeGeometry;
		m_xBallMaterial = Marble::g_xBallMaterial;
		m_xPlatformMaterial = Marble::g_xPlatformMaterial;
		m_xGoalMaterial = Marble::g_xGoalMaterial;
		m_xCollectibleMaterial = Marble::g_xCollectibleMaterial;
		m_xFloorMaterial = Marble::g_xFloorMaterial;

		// Wire menu button callbacks
		bool bHasMenu = false;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay)
			{
				pxPlay->SetOnClick(&OnPlayClicked, this);
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

	void OnStart() ZENITH_FINAL override
	{
		if (m_eGameState == MarbleGameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
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
				Zenith_SceneManager::SetScenePaused(m_xLevelScene, true);
				UpdateUI();
				return;
			}
			if (Marble_Input::WasResetPressed())
			{
				ResetLevel();
				return;
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
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
				Zenith_SceneManager::SetScenePaused(m_xLevelScene, false);
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
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
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateCamera(fDt);
			UpdateUI();
			break;
		}
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
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
#endif
	}

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fTimeRemaining;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
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
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// State Transitions
	// ========================================================================
	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create level scene
		m_xLevelScene = Zenith_SceneManager::CreateEmptyScene("Level");
		Zenith_SceneManager::SetActiveScene(m_xLevelScene);

		// Generate level (uses GetActiveScene internally)
		Marble_LevelGenerator::GenerateLevel(
			m_xLevelEntities,
			m_xRng,
			Marble::g_pxBallPrefab,
			Marble::g_pxPlatformPrefab,
			Marble::g_pxGoalPrefab,
			Marble::g_pxCollectiblePrefab,
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xBallMaterial.Get(),
			m_xPlatformMaterial.Get(),
			m_xGoalMaterial.Get(),
			m_xCollectibleMaterial.Get());

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
			Zenith_SceneManager::UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ResetLevel()
	{
		ClearEntityReferences();

		if (m_xLevelScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}

		// Create fresh level scene
		m_xLevelScene = Zenith_SceneManager::CreateEmptyScene("Level");
		Zenith_SceneManager::SetActiveScene(m_xLevelScene);

		// Generate level
		Marble_LevelGenerator::GenerateLevel(
			m_xLevelEntities,
			m_xRng,
			Marble::g_pxBallPrefab,
			Marble::g_pxPlatformPrefab,
			Marble::g_pxGoalPrefab,
			Marble::g_pxCollectiblePrefab,
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xBallMaterial.Get(),
			m_xPlatformMaterial.Get(),
			m_xGoalMaterial.Get(),
			m_xCollectibleMaterial.Get());

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

		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
			m_iFocusIndex = (m_iFocusIndex - 1 + s_iButtonCount) % s_iButtonCount;
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S))
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
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		if (!xBall.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();

		// Get camera from persistent scene
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
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
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xLevelScene);
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
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
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
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xLevelScene);
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
