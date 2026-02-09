#pragma once
/**
 * Runner_Behaviour.h - Main game coordinator
 *
 * This file orchestrates the Runner Game using modular components.
 * Each module handles a specific responsibility:
 *
 * - Runner_Config.h              - DataAsset for game configuration
 * - Runner_CharacterController.h - Lane-based movement, jump, slide
 * - Runner_AnimationDriver.h     - Animation state machine control
 * - Runner_TerrainManager.h      - Terrain entity management
 * - Runner_CollectibleSpawner.h  - Obstacles and collectibles
 * - Runner_ParticleManager.h     - Visual particle effects
 * - Runner_UIManager.h           - HUD management
 *
 * Key Engine Features Demonstrated:
 * - Multi-scene architecture (persistent GameManager + game scene)
 * - Zenith_UIButton for clickable/tappable menu
 * - Lane-based endless runner mechanics
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "UI/Zenith_UIButton.h"

// Include modules
#include "Runner_Config.h"
#include "Runner_CharacterController.h"
#include "Runner_AnimationDriver.h"
#include "Runner_TerrainManager.h"
#include "Runner_CollectibleSpawner.h"
#include "Runner_ParticleManager.h"
#include "Runner_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// Runner Resources - Global access
// Defined in Runner.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
class Zenith_Prefab;

namespace Runner
{
	extern Flux_MeshGeometry* g_pxCapsuleGeometry;
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;

	extern MaterialHandle g_xCharacterMaterial;
	extern MaterialHandle g_xGroundMaterial;
	extern MaterialHandle g_xObstacleMaterial;
	extern MaterialHandle g_xCollectibleMaterial;
	extern MaterialHandle g_xDustMaterial;
	extern MaterialHandle g_xCollectParticleMaterial;

	extern Zenith_Prefab* g_pxCharacterPrefab;
	extern Zenith_Prefab* g_pxGroundPrefab;
	extern Zenith_Prefab* g_pxObstaclePrefab;
	extern Zenith_Prefab* g_pxCollectiblePrefab;
	extern Zenith_Prefab* g_pxParticlePrefab;
}

/**
 * Runner_Behaviour - Main game coordinator
 *
 * Architecture:
 * - Persistent GameManager entity (camera + UI + script) in DontDestroyOnLoad scene
 * - Game scene created/destroyed on transitions via CreateEmptyScene/UnloadScene
 *
 * State machine: MAIN_MENU -> PLAYING -> PAUSED / GAME_OVER -> MAIN_MENU
 */
class Runner_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Runner_Behaviour)

	Runner_Behaviour() = delete;
	Runner_Behaviour(Zenith_Entity& xParentEntity)
		: m_eGameState(RunnerGameState::MAIN_MENU)
		, m_uScore(0)
		, m_uHighScore(0)
		, m_xRng(std::random_device{}())
		, m_iFocusIndex(0)
	{
	}
	~Runner_Behaviour() = default;

	void OnAwake() ZENITH_FINAL override
	{
		// Cache resource pointers
		m_pxCapsuleGeometry = Runner::g_pxCapsuleGeometry;
		m_pxCubeGeometry = Runner::g_pxCubeGeometry;
		m_pxSphereGeometry = Runner::g_pxSphereGeometry;
		m_xCharacterMaterial = Runner::g_xCharacterMaterial;
		m_xGroundMaterial = Runner::g_xGroundMaterial;
		m_xObstacleMaterial = Runner::g_xObstacleMaterial;
		m_xCollectibleMaterial = Runner::g_xCollectibleMaterial;
		m_xDustMaterial = Runner::g_xDustMaterial;
		m_xCollectParticleMaterial = Runner::g_xCollectParticleMaterial;

		// Wire menu button callback
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay)
				pxPlay->SetOnClick(&OnPlayClicked, this);
		}

		m_eGameState = RunnerGameState::MAIN_MENU;
		SetMenuVisible(true);
		SetHUDVisible(false);
	}

	void OnStart() ZENITH_FINAL override
	{
		if (m_eGameState == RunnerGameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		switch (m_eGameState)
		{
		case RunnerGameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case RunnerGameState::PLAYING:
		{
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				m_eGameState = RunnerGameState::PAUSED;
				Zenith_SceneManager::SetScenePaused(m_xGameScene, true);
				UpdateUI();
				return;
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				ResetGame();
				return;
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}

			UpdatePlaying(fDt);
			break;
		}

		case RunnerGameState::PAUSED:
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				m_eGameState = RunnerGameState::PLAYING;
				Zenith_SceneManager::SetScenePaused(m_xGameScene, false);
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;

		case RunnerGameState::GAME_OVER:
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				ResetGame();
				return;
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;
		}
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Endless Runner");
		ImGui::Separator();

		const char* szStates[] = { "MENU", "PLAYING", "PAUSED", "GAME_OVER" };
		ImGui::Text("State: %s", szStates[static_cast<int>(m_eGameState)]);

		if (m_eGameState != RunnerGameState::MAIN_MENU)
		{
			float fDistance = Runner_CharacterController::GetDistanceTraveled();
			float fSpeed = Runner_CharacterController::GetCurrentSpeed();
			ImGui::Text("Distance: %.1f m", fDistance);
			ImGui::Text("Score: %u", m_uScore);
			ImGui::Text("High Score: %u", m_uHighScore);
			ImGui::Text("Speed: %.1f", fSpeed);

			const char* szCharStates[] = { "RUNNING", "JUMPING", "SLIDING", "DEAD" };
			ImGui::Text("Character: %s", szCharStates[static_cast<int>(Runner_CharacterController::GetState())]);
			ImGui::Text("Lane: %d", Runner_CharacterController::GetCurrentLane());
		}

		if (m_eGameState == RunnerGameState::MAIN_MENU)
		{
			if (ImGui::Button("Start Game"))
				StartGame();
		}
		else
		{
			if (ImGui::Button("Reset Game"))
				ResetGame();
			if (ImGui::Button("Return to Menu"))
				ReturnToMenu();
		}
#endif
	}

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_uHighScore;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_uHighScore;
		}
	}

private:
	// ========================================================================
	// Menu Button Callbacks
	// ========================================================================
	static void OnPlayClicked(void* pxUserData)
	{
		Runner_Behaviour* pxSelf = static_cast<Runner_Behaviour*>(pxUserData);
		pxSelf->StartGame();
	}

	// ========================================================================
	// State Transitions
	// ========================================================================
	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create game scene
		m_xGameScene = Zenith_SceneManager::CreateEmptyScene("Run");
		Zenith_SceneManager::SetActiveScene(m_xGameScene);

		// Initialize all systems (uses GetActiveScene internally)
		InitializeGame();

		m_eGameState = RunnerGameState::PLAYING;
		m_uScore = 0;
	}

	void ReturnToMenu()
	{
		// Update high score before leaving
		if (m_uScore > m_uHighScore)
			m_uHighScore = m_uScore;

		m_uCharacterEntityID = INVALID_ENTITY_ID;

		if (m_xGameScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}

		m_eGameState = RunnerGameState::MAIN_MENU;
		m_iFocusIndex = 0;
		SetMenuVisible(true);
		SetHUDVisible(false);
	}

	void ResetGame()
	{
		// Update high score
		if (m_uScore > m_uHighScore)
			m_uHighScore = m_uScore;

		m_uCharacterEntityID = INVALID_ENTITY_ID;

		if (m_xGameScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}

		// Create fresh game scene
		m_xGameScene = Zenith_SceneManager::CreateEmptyScene("Run");
		Zenith_SceneManager::SetActiveScene(m_xGameScene);

		// Re-initialize all systems
		InitializeGame();

		m_eGameState = RunnerGameState::PLAYING;
		m_uScore = 0;
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
	}

	void SetHUDVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszElements[] = { "Title", "Distance", "Score", "HighScore", "Speed", "Controls", "Status" };
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		// Only one button, but still support keyboard focus
		Zenith_UI::Zenith_UIButton* pxPlay = nullptr;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		}
		if (pxPlay) pxPlay->SetFocused(true);
	}

	// ========================================================================
	// Game Logic
	// ========================================================================
	void InitializeGame()
	{
		// Initialize character controller
		Runner_CharacterController::Config xCharConfig;
		Runner_CharacterController::Initialize(xCharConfig);

		// Initialize animation driver
		Runner_AnimationDriver::Config xAnimConfig;
		Runner_AnimationDriver::Initialize(xAnimConfig);

		// Initialize terrain manager
		Runner_TerrainManager::Config xTerrainConfig;
		Runner_TerrainManager::Initialize(
			xTerrainConfig,
			Runner::g_pxGroundPrefab,
			m_pxCubeGeometry,
			m_xGroundMaterial.Get());

		// Initialize collectible spawner
		Runner_CollectibleSpawner::Config xSpawnConfig;
		Runner_CollectibleSpawner::Initialize(
			xSpawnConfig,
			Runner::g_pxCollectiblePrefab,
			Runner::g_pxObstaclePrefab,
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xCollectibleMaterial.Get(),
			m_xObstacleMaterial.Get(),
			m_xRng);

		// Initialize particle manager
		Runner_ParticleManager::Config xParticleConfig;
		Runner_ParticleManager::Initialize(
			xParticleConfig,
			Runner::g_pxParticlePrefab,
			m_pxSphereGeometry,
			m_xDustMaterial.Get(),
			m_xCollectParticleMaterial.Get());

		// Create character entity
		CreateCharacter();
	}

	void CreateCharacter()
	{
		if (Runner::g_pxCharacterPrefab == nullptr || m_pxCapsuleGeometry == nullptr || !m_xCharacterMaterial)
			return;

		if (!m_xGameScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xGameScene);
		Zenith_Entity xCharacter = Runner::g_pxCharacterPrefab->Instantiate(pxSceneData, "Runner");

		Zenith_TransformComponent& xTransform = xCharacter.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xTransform.SetScale(Zenith_Maths::Vector3(0.8f, 1.8f, 0.8f));

		Zenith_ModelComponent& xModel = xCharacter.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCapsuleGeometry, *m_xCharacterMaterial.Get());

		m_uCharacterEntityID = xCharacter.GetEntityID();
	}

	void UpdatePlaying(float fDt)
	{
		if (!m_xGameScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xGameScene);
		if (!m_uCharacterEntityID.IsValid() || !pxSceneData->EntityExists(m_uCharacterEntityID))
			return;

		Zenith_Entity xCharacter = pxSceneData->GetEntity(m_uCharacterEntityID);
		Zenith_TransformComponent& xTransform = xCharacter.GetComponent<Zenith_TransformComponent>();

		// Get terrain height at player position
		float fPlayerZ = Runner_CharacterController::GetDistanceTraveled();
		float fTerrainHeight = Runner_TerrainManager::GetTerrainHeightAt(fPlayerZ);

		// Update character controller
		Runner_CharacterController::Update(fDt, xTransform, fTerrainHeight);

		// Update animation driver
		Runner_AnimationDriver::Update(fDt, xTransform);

		// Get current player position for other systems
		Zenith_Maths::Vector3 xPlayerPos;
		xTransform.GetPosition(xPlayerPos);

		// Update terrain
		Runner_TerrainManager::Update(fPlayerZ);

		// Update collectibles and obstacles
		Runner_CollectibleSpawner::Update(fDt, fPlayerZ);

		// Check collectible pickups
		float fPlayerRadius = 0.4f;
		Runner_CollectibleSpawner::CollectionResult xCollectResult =
			Runner_CollectibleSpawner::CheckCollectibles(xPlayerPos, fPlayerRadius);

		if (xCollectResult.m_uCollectedCount > 0)
		{
			m_uScore += xCollectResult.m_uPointsGained;
			for (uint32_t i = 0; i < xCollectResult.m_uCollectedCount; i++)
			{
				Runner_ParticleManager::SpawnCollectEffect(xPlayerPos);
			}
		}

		// Check obstacle collision
		float fPlayerHeight = Runner_CharacterController::GetCurrentCharacterHeight();
		bool bIsSliding = Runner_CharacterController::IsSliding();

		if (Runner_CollectibleSpawner::CheckObstacleCollision(xPlayerPos, fPlayerRadius, fPlayerHeight, bIsSliding))
		{
			Runner_CharacterController::OnObstacleHit();
			m_eGameState = RunnerGameState::GAME_OVER;
			if (m_uScore > m_uHighScore)
				m_uHighScore = m_uScore;
		}

		// Check if character is dead from falling
		if (Runner_CharacterController::GetState() == RunnerCharacterState::DEAD)
		{
			m_eGameState = RunnerGameState::GAME_OVER;
		}

		// Update particles
		bool bIsRunning = Runner_CharacterController::GetState() == RunnerCharacterState::RUNNING;
		bool bIsGrounded = Runner_CharacterController::IsGrounded();
		Runner_ParticleManager::Update(fDt, xPlayerPos, bIsRunning, bIsGrounded);

		// Update camera
		UpdateCamera(fDt, xPlayerPos);

		// Update UI
		UpdateUI();
	}

	// ========================================================================
	// Camera
	// ========================================================================
	void UpdateCamera(float fDt, const Zenith_Maths::Vector3& xPlayerPos)
	{
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		static constexpr float s_fCameraDistance = 8.0f;
		static constexpr float s_fCameraHeight = 4.0f;
		static constexpr float s_fCameraLookAhead = 5.0f;
		static constexpr float s_fCameraSmoothSpeed = 5.0f;

		Zenith_Maths::Vector3 xCurrentPos;
		pxCamera->GetPosition(xCurrentPos);

		Zenith_Maths::Vector3 xTargetPos(
			xPlayerPos.x * 0.3f,
			xPlayerPos.y + s_fCameraHeight,
			xPlayerPos.z - s_fCameraDistance
		);

		Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetPos, s_fCameraSmoothSpeed * fDt);
		pxCamera->SetPosition(xNewPos);

		Zenith_Maths::Vector3 xLookAt = xPlayerPos + Zenith_Maths::Vector3(0.0f, 0.0f, s_fCameraLookAhead);
		Zenith_Maths::Vector3 xDir = glm::normalize(xLookAt - xNewPos);

		float fPitch = -asin(xDir.y);
		float fYaw = atan2(xDir.x, xDir.z);

		pxCamera->SetPitch(fPitch);
		pxCamera->SetYaw(fYaw);
	}

	// ========================================================================
	// UI
	// ========================================================================
	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		float fDistance = Runner_CharacterController::GetDistanceTraveled();
		float fSpeed = Runner_CharacterController::GetCurrentSpeed();
		static constexpr float s_fMaxSpeed = 35.0f;

		Runner_UIManager::UpdateUI(xUI, fDistance, m_uScore, fSpeed, s_fMaxSpeed, m_eGameState);
		Runner_UIManager::UpdateHighScore(xUI, m_uHighScore);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	RunnerGameState m_eGameState;
	uint32_t m_uScore;
	uint32_t m_uHighScore;

	Zenith_EntityID m_uCharacterEntityID = INVALID_ENTITY_ID;

	// Scene handle for the game scene
	Zenith_Scene m_xGameScene;

	// Random number generator
	std::mt19937 m_xRng;

	// Menu keyboard focus
	int32_t m_iFocusIndex;

	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	MaterialHandle m_xCharacterMaterial;
	MaterialHandle m_xGroundMaterial;
	MaterialHandle m_xObstacleMaterial;
	MaterialHandle m_xCollectibleMaterial;
	MaterialHandle m_xDustMaterial;
	MaterialHandle m_xCollectParticleMaterial;
};
