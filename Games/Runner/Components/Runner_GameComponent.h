#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Runner_GameComponent.h - Main game coordinator
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
 * - Game ECS component lifecycle hooks (concept-detected by the
 *   component-meta registry)
 * - Multi-scene architecture (persistent GameManager + game scene)
 * - Zenith_UIButton for clickable/tappable menu
 * - Lane-based endless runner mechanics
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
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
// Runner Resources - Phase 8 per-game ProjectResources struct.
// ============================================================================
class Zenith_Prefab;

namespace Runner
{
	struct RunnerResources
	{
		MeshGeometryHandle  m_xCapsuleAsset;
		MeshGeometryHandle  m_xCubeAsset;
		MeshGeometryHandle  m_xSphereAsset;
		Flux_MeshGeometry*  m_pxCapsuleGeometry = nullptr;
		Flux_MeshGeometry*  m_pxCubeGeometry    = nullptr;
		Flux_MeshGeometry*  m_pxSphereGeometry  = nullptr;

		MaterialHandle      m_xCharacterMaterial;
		MaterialHandle      m_xGroundMaterial;
		MaterialHandle      m_xObstacleMaterial;
		MaterialHandle      m_xCollectibleMaterial;
		MaterialHandle      m_xDustMaterial;
		MaterialHandle      m_xCollectParticleMaterial;

		PrefabHandle        m_xCharacterPrefab;
		PrefabHandle        m_xGroundPrefab;
		PrefabHandle        m_xObstaclePrefab;
		PrefabHandle        m_xCollectiblePrefab;
		PrefabHandle        m_xParticlePrefab;
	};

	RunnerResources& Resources();
}

/**
 * Runner_GameComponent - Main game coordinator
 *
 * Architecture:
 * - GameManager entity (camera + UI + game component) per scene
 * - Game scene created/destroyed on transitions via CreateEmptyScene/UnloadScene
 *
 * State machine: MAIN_MENU -> PLAYING -> PAUSED / GAME_OVER -> MAIN_MENU
 */
class Runner_GameComponent
{
public:
	Runner_GameComponent() = delete;
	Runner_GameComponent(Zenith_Entity& xParentEntity)
		: m_eGameState(RunnerGameState::MAIN_MENU)
		, m_uScore(0)
		, m_uHighScore(0)
		, m_xRng(std::random_device{}())
		, m_iFocusIndex(0)
		, m_xParentEntity(xParentEntity)
	{
	}
	~Runner_GameComponent() = default;

	// State probes for the characterization tests (read-only; same surface
	// before and after the wave-2 graph conversion).
	RunnerGameState GetGameState() const { return m_eGameState; }
	uint32_t GetScore() const { return m_uScore; }
	uint32_t GetHighScore() const { return m_uHighScore; }

	// Graph-facing surface (wave-2): the scoring / game-over DECISIONS live
	// in the boot-authored Runner_RunFlow graph; the nodes read the frame's
	// systems results and write the decisions back through these.
	const Runner_CollectibleSpawner::CollectionResult& GetLastCollection() const { return m_xLastCollection; }
	bool WasObstacleHit() const { return m_bObstacleHitThisFrame; }
	void AddScore(uint32_t uScore) { m_uScore += uScore; }
	void SetHighScore(uint32_t uScore) { m_uHighScore = uScore; }
	void SetGameStateFromGraph(RunnerGameState eState) { m_eGameState = eState; }

	void OnAwake()
	{
		// Cache resource pointers
		m_pxCapsuleGeometry = Runner::Resources().m_pxCapsuleGeometry;
		m_pxCubeGeometry = Runner::Resources().m_pxCubeGeometry;
		m_pxSphereGeometry = Runner::Resources().m_pxSphereGeometry;
		m_xCharacterMaterial = Runner::Resources().m_xCharacterMaterial;
		m_xGroundMaterial = Runner::Resources().m_xGroundMaterial;
		m_xObstacleMaterial = Runner::Resources().m_xObstacleMaterial;
		m_xCollectibleMaterial = Runner::Resources().m_xCollectibleMaterial;
		m_xDustMaterial = Runner::Resources().m_xDustMaterial;
		m_xCollectParticleMaterial = Runner::Resources().m_xCollectParticleMaterial;

		// Wire menu button callback
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay)
			{
				pxPlay->SetOnClick(&OnPlayClicked, nullptr);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			m_eGameState = RunnerGameState::MAIN_MENU;
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
		if (m_eGameState == RunnerGameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDt)
	{
		switch (m_eGameState)
		{
		case RunnerGameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case RunnerGameState::PLAYING:
		{
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				m_eGameState = RunnerGameState::PAUSED;
				g_xEngine.Scenes().SetScenePaused(m_xGameScene, true);
				UpdateUI();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				ResetGame();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}

			UpdatePlaying(fDt);
			break;
		}

		case RunnerGameState::PAUSED:
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				m_eGameState = RunnerGameState::PLAYING;
				g_xEngine.Scenes().SetScenePaused(m_xGameScene, false);
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;

		case RunnerGameState::GAME_OVER:
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				ResetGame();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
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
		xStream << m_uHighScore;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

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

		// Create game scene
		m_xGameScene = g_xEngine.Scenes().LoadScene("Run", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xGameScene);

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
			g_xEngine.Scenes().UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}

		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ResetGame()
	{
		// Update high score
		if (m_uScore > m_uHighScore)
			m_uHighScore = m_uScore;

		m_uCharacterEntityID = INVALID_ENTITY_ID;

		if (m_xGameScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}

		// Create fresh game scene
		m_xGameScene = g_xEngine.Scenes().LoadScene("Run", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xGameScene);

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
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;
		Zenith_UIComponent& xUI = *pxUI;

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;
		Zenith_UIComponent& xUI = *pxUI;

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
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
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
			Runner::Resources().m_xGroundPrefab.GetDirect(),
			m_pxCubeGeometry,
			m_xGroundMaterial.GetDirect());

		// Initialize collectible spawner
		Runner_CollectibleSpawner::Config xSpawnConfig;
		Runner_CollectibleSpawner::Initialize(
			xSpawnConfig,
			Runner::Resources().m_xCollectiblePrefab.GetDirect(),
			Runner::Resources().m_xObstaclePrefab.GetDirect(),
			m_pxSphereGeometry,
			m_pxCubeGeometry,
			m_xCollectibleMaterial.GetDirect(),
			m_xObstacleMaterial.GetDirect(),
			m_xRng);

		// Initialize particle manager
		Runner_ParticleManager::Config xParticleConfig;
		Runner_ParticleManager::Initialize(
			xParticleConfig,
			Runner::Resources().m_xParticlePrefab.GetDirect(),
			m_pxSphereGeometry,
			m_xDustMaterial.GetDirect(),
			m_xCollectParticleMaterial.GetDirect());

		// Create character entity
		CreateCharacter();
	}

	void CreateCharacter()
	{
		if (!Runner::Resources().m_xCharacterPrefab || m_pxCapsuleGeometry == nullptr || !m_xCharacterMaterial)
			return;

		if (!m_xGameScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xGameScene);
		Zenith_Entity xCharacter = Runner::Resources().m_xCharacterPrefab.GetDirect()->Instantiate(pxSceneData, "Runner");

		Zenith_TransformComponent& xTransform = xCharacter.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xTransform.SetScale(Zenith_Maths::Vector3(0.8f, 1.8f, 0.8f));

		Zenith_ModelComponent& xModel = xCharacter.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCapsuleGeometry, *m_xCharacterMaterial.GetDirect());

		m_uCharacterEntityID = xCharacter.GetEntityID();
	}

	void UpdatePlaying(float fDt)
	{
		if (!m_xGameScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xGameScene);
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

		// Check collectible pickups (detection + the particle bursts are
		// systems; the SCORING decision lives in the Runner_RunFlow graph).
		float fPlayerRadius = 0.4f;
		m_xLastCollection = Runner_CollectibleSpawner::CheckCollectibles(xPlayerPos, fPlayerRadius);
		for (uint32_t i = 0; i < m_xLastCollection.m_uCollectedCount; i++)
		{
			Runner_ParticleManager::SpawnCollectEffect(xPlayerPos);
		}

		// Obstacle collision (systems query; the game-over decision lives in
		// the graph).
		float fPlayerHeight = Runner_CharacterController::GetCurrentCharacterHeight();
		m_bObstacleHitThisFrame = Runner_CollectibleSpawner::CheckObstacleCollision(xPlayerPos, fPlayerRadius, fPlayerHeight);

		// Scoring + game-over decisions: fire the graph's driving event at
		// exactly the point the old decision block ran.
		FireRunTick();

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
		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
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
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		float fDistance = Runner_CharacterController::GetDistanceTraveled();
		float fSpeed = Runner_CharacterController::GetCurrentSpeed();
		static constexpr float s_fMaxSpeed = 35.0f;

		Runner_UIManager::UpdateUI(xUI, fDistance, m_uScore, fSpeed, s_fMaxSpeed, m_eGameState);
		Runner_UIManager::UpdateHighScore(xUI, m_uHighScore);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================
	void FireRunTick()
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		pxGraph->FireCustomEvent("RunTick");
	}

	// Per-frame systems results consumed by the Runner_RunFlow graph nodes.
	Runner_CollectibleSpawner::CollectionResult m_xLastCollection;
	bool m_bObstacleHitThisFrame = false;

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

	// Owning entity (explicit member now - was provided by the old script base)
	Zenith_Entity m_xParentEntity;
};
