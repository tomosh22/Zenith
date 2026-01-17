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
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"

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
	extern Zenith_MaterialAsset* g_pxBallMaterial;
	extern Zenith_MaterialAsset* g_pxPlatformMaterial;
	extern Zenith_MaterialAsset* g_pxGoalMaterial;
	extern Zenith_MaterialAsset* g_pxCollectibleMaterial;
	extern Zenith_MaterialAsset* g_pxFloorMaterial;

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
 * Lifecycle:
 * - OnAwake: Called when behavior is attached at runtime
 * - OnStart: Called before first update (for all entities)
 * - OnUpdate: Called every frame
 */
class Marble_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Marble_Behaviour)

	Marble_Behaviour() = delete;
	Marble_Behaviour(Zenith_Entity& xParentEntity)
		: m_eGameState(MarbleGameState::PLAYING)
		, m_uScore(0)
		, m_fTimeRemaining(s_fInitialTime)
		, m_uCollectedCount(0)
		, m_xRng(std::random_device{}())
	{
	}
	~Marble_Behaviour() = default;

	/**
	 * OnAwake - Called when behavior is attached at RUNTIME
	 */
	void OnAwake() ZENITH_FINAL override
	{
		// Store resource pointers from globals (properly initialized with GPU resources)
		m_pxSphereGeometry = Marble::g_pxSphereGeometry;
		m_pxCubeGeometry = Marble::g_pxCubeGeometry;
		m_pxBallMaterial = Marble::g_pxBallMaterial;
		m_pxPlatformMaterial = Marble::g_pxPlatformMaterial;
		m_pxGoalMaterial = Marble::g_pxGoalMaterial;
		m_pxCollectibleMaterial = Marble::g_pxCollectibleMaterial;
		m_pxFloorMaterial = Marble::g_pxFloorMaterial;

		GenerateLevel();
	}

	/**
	 * OnStart - Called before first update
	 */
	void OnStart() ZENITH_FINAL override
	{
		if (!m_xLevelEntities.uBallEntityID.IsValid())
		{
			GenerateLevel();
		}
	}

	/**
	 * OnUpdate - Main game loop
	 */
	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Handle pause input (always checked)
		if (Marble_Input::WasPausePressed())
		{
			TogglePause();
		}

		// Handle reset input (always checked)
		if (Marble_Input::WasResetPressed())
		{
			ResetLevel();
		}

		// Paused - don't update game logic
		if (m_eGameState == MarbleGameState::PAUSED)
		{
			UpdateUI();
			return;
		}

		// Playing state - full game update
		if (m_eGameState == MarbleGameState::PLAYING)
		{
			// Timer
			m_fTimeRemaining -= fDt;
			if (m_fTimeRemaining <= 0.0f)
			{
				m_fTimeRemaining = 0.0f;
				m_eGameState = MarbleGameState::LOST;
			}

			// Input and physics
			HandleInput(fDt);

			// Collectibles
			HandleCollectibles(fDt);

			// Check fall
			CheckFallCondition();
		}

		// Camera follow (runs even when paused/won/lost)
		UpdateCamera(fDt);

		// UI always updated
		UpdateUI();
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Marble Ball Game");
		ImGui::Separator();
		ImGui::Text("Score: %u", m_uScore);
		ImGui::Text("Time: %.1f", m_fTimeRemaining);
		ImGui::Text("Collected: %u / %u", m_uCollectedCount,
			static_cast<uint32_t>(m_xLevelEntities.axCollectibleEntityIDs.size()) + m_uCollectedCount);

		const char* szStates[] = { "PLAYING", "PAUSED", "WON", "LOST" };
		ImGui::Text("State: %s", szStates[static_cast<int>(m_eGameState)]);

		if (ImGui::Button("Reset Level"))
		{
			ResetLevel();
		}
		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD: Move ball");
		ImGui::Text("  Space: Jump");
		ImGui::Text("  P/Esc: Pause");
		ImGui::Text("  R: Reset");
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
	// Input and Physics (delegates to modules)
	// ========================================================================
	void HandleInput(float /*fDt*/)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !xScene.EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = xScene.GetEntity(m_xLevelEntities.uBallEntityID);
		if (!xBall.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();

		// Get camera for relative input
		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID || !xScene.EntityExists(uCamID))
			return;

		Zenith_Entity xCamEntity = xScene.GetEntity(uCamID);
		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Get positions for input calculation
		Zenith_Maths::Vector3 xCamPos, xBallPos;
		xCamera.GetPosition(xCamPos);
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		// Get camera-relative movement direction (Marble_Input)
		Zenith_Maths::Vector3 xDirection = Marble_Input::GetMovementDirection(xCamPos, xBallPos);

		// Apply movement (Marble_PhysicsController)
		Marble_PhysicsController::ApplyMovement(xCollider, xDirection);

		// Handle jump
		if (Marble_Input::WasJumpPressed())
		{
			Marble_PhysicsController::TryJump(xCollider);
		}
	}

	void CheckFallCondition()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !xScene.EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = xScene.GetEntity(m_xLevelEntities.uBallEntityID);
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
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !xScene.EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID || !xScene.EntityExists(uCamID))
			return;

		Zenith_Entity xBall = xScene.GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Entity xCamEntity = xScene.GetEntity(uCamID);

		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);
		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		Marble_CameraFollow::Update(xCamera, xBallPos, fDt);
	}

	// ========================================================================
	// Collectibles (delegates to Marble_CollectibleSystem)
	// ========================================================================
	void HandleCollectibles(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !xScene.EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = xScene.GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		// Check for collections
		Marble_CollectibleSystem::CollectionResult xResult =
			Marble_CollectibleSystem::CheckCollectibles(xBallPos, m_xLevelEntities.axCollectibleEntityIDs, m_uCollectedCount);

		// Update score and count
		m_uScore += xResult.uScoreGained;
		m_uCollectedCount += xResult.uCollectedCount;

		// Check win condition
		if (xResult.bAllCollected)
		{
			m_eGameState = MarbleGameState::WON;
		}

		// Animate collectibles
		Marble_CollectibleSystem::UpdateCollectibleRotation(m_xLevelEntities.axCollectibleEntityIDs, fDt);
	}

	// ========================================================================
	// Level Generation (delegates to Marble_LevelGenerator)
	// ========================================================================
	void GenerateLevel()
	{
		// Clean up existing level
		Marble_LevelGenerator::DestroyLevel(m_xLevelEntities);

		// Generate new level
		Marble_LevelGenerator::GenerateLevel(
			m_xLevelEntities,
			m_xRng,
			Marble::g_pxBallPrefab,
			Marble::g_pxPlatformPrefab,
			Marble::g_pxGoalPrefab,
			Marble::g_pxCollectiblePrefab,
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_pxBallMaterial,
			m_pxPlatformMaterial,
			m_pxGoalMaterial,
			m_pxCollectibleMaterial);

		// Reset game state
		m_eGameState = MarbleGameState::PLAYING;
		m_uScore = 0;
		m_fTimeRemaining = s_fInitialTime;
		m_uCollectedCount = 0;
	}

	void ResetLevel()
	{
		GenerateLevel();
	}

	// ========================================================================
	// Pause
	// ========================================================================
	void TogglePause()
	{
		if (m_eGameState == MarbleGameState::PLAYING)
		{
			m_eGameState = MarbleGameState::PAUSED;
		}
		else if (m_eGameState == MarbleGameState::PAUSED)
		{
			m_eGameState = MarbleGameState::PLAYING;
		}
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

	// Random number generator
	std::mt19937 m_xRng;

public:
	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Zenith_MaterialAsset* m_pxBallMaterial = nullptr;
	Zenith_MaterialAsset* m_pxPlatformMaterial = nullptr;
	Zenith_MaterialAsset* m_pxGoalMaterial = nullptr;
	Zenith_MaterialAsset* m_pxCollectibleMaterial = nullptr;
	Zenith_MaterialAsset* m_pxFloorMaterial = nullptr;
};
