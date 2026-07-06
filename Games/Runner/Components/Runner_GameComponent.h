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
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
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
#include "Runner_CharacterShim.h"
#include "Runner_TerrainManager.h"
#include "Runner_CollectibleSpawner.h"
#include "Runner_ParticleManager.h"
#include "Runner_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
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
		: m_xRng(std::random_device{}())
		, m_xParentEntity(xParentEntity)
	{
	}
	~Runner_GameComponent() = default;

	// ========================================================================
	// Graph-state accessors - the probe surface. State lives on the attached
	// graph's blackboard (Runner_RunFlow declares gameState/score/highScore;
	// Runner_GameFlow pins gameState at MAIN_MENU); C++ (HUD, systems gating,
	// characterization tests) READS it here. score/highScore are FLOAT vars
	// so the accumulate/max arithmetic is pure engine float nodes.
	// ========================================================================
	RunnerGameState GetGameState()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		if (pxBlackboard == nullptr)
		{
			return RunnerGameState::MAIN_MENU;	// pre-resolve fallback
		}
		int32_t iState = pxBlackboard->GetInt32("gameState", 0);
		iState = iState < 0 ? 0 : (iState > 3 ? 3 : iState);
		return static_cast<RunnerGameState>(iState);
	}
	uint32_t GetScore()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		const float fScore = pxBlackboard ? pxBlackboard->GetFloat("score", 0.0f) : 0.0f;
		return fScore < 0.5f ? 0u : static_cast<uint32_t>(fScore + 0.5f);
	}
	uint32_t GetHighScore()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		const float fHighScore = pxBlackboard ? pxBlackboard->GetFloat("highScore", 0.0f) : 0.0f;
		return fHighScore < 0.5f ? 0u : static_cast<uint32_t>(fHighScore + 0.5f);
	}

	// ========================================================================
	// Graph-facing systems surface (the node seams)
	// ========================================================================
	const Runner_CollectibleSpawner::CollectionResult& GetLastCollection() const { return m_xLastCollection; }
	bool WasObstacleHit() const { return m_bObstacleHitThisFrame; }

	// Run (re)creation: unload any existing run scene, create a fresh one,
	// initialise every system + the character. State/score resets are
	// GRAPH-side (the R chain).
	void RegenerateRun()
	{
		m_uCharacterEntityID = INVALID_ENTITY_ID;
		if (m_xGameScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}

		m_xGameScene = g_xEngine.Scenes().LoadScene("Run", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xGameScene);

		InitializeGame();
	}

	// Run teardown (the escape-to-menu systems body; the menu-scene load is
	// an engine LoadSceneByIndex node at the end of the graph chain).
	void UnloadRun()
	{
		m_uCharacterEntityID = INVALID_ENTITY_ID;
		if (m_xGameScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xGameScene);
			m_xGameScene = Zenith_Scene();
		}
	}

	// Pause/resume the RUN scene only - the GameManager's scene keeps
	// updating so the graphs still see the resume key.
	void SetRunPaused(bool bPaused)
	{
		if (m_xGameScene.IsValid())
		{
			g_xEngine.Scenes().SetScenePaused(m_xGameScene, bPaused);
		}
	}

	// Characterization-test seam: the next PLAYING tick consumes this injected
	// collection result INSTEAD of computing one (deterministic scoring probes
	// without steering the auto-runner). Conversion-neutral: the decision
	// consumers read m_xLastCollection identically before and after the wave.
	void Test_InjectCollection(uint32_t uPoints, uint32_t uCount)
	{
		m_xInjectedCollection.m_uPointsGained = uPoints;
		m_xInjectedCollection.m_uCollectedCount = uCount;
		m_bHasInjectedCollection = true;
	}

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

		// The menu scene's GameManager owns the Play canvas (its clicks are
		// the graph's OnUIButtonClicked source - no C++ wiring). The gameplay
		// scene's GameManager has no menu: build the run directly; its state
		// comes from Runner_RunFlow's declared gameState default (PLAYING).
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			bHasMenu = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay") != nullptr;
		}
		if (!bHasMenu)
		{
			RegenerateRun();
		}
	}

	// Per-state SYSTEMS dispatch - reads the graph-owned state, runs the
	// systems passes, and fires "RunTick"/"CharTick" into the graphs. All
	// transitions (keys, scoring, game over, menu) are graph chains.
	void OnUpdate(const float fDt)
	{
		switch (GetGameState())
		{
		case RunnerGameState::MAIN_MENU:
			// Menu focus/click handling is fully graph + UI-system side.
			break;

		case RunnerGameState::PLAYING:
			UpdatePlaying(fDt);
			break;

		case RunnerGameState::PAUSED:
		case RunnerGameState::GAME_OVER:
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
		const RunnerGameState eGameState = GetGameState();
		ImGui::Text("State: %s", szStates[static_cast<int>(eGameState)]);

		if (eGameState != RunnerGameState::MAIN_MENU)
		{
			float fDistance = Runner_CharacterController::GetDistanceTraveled();
			float fSpeed = Runner_CharacterController::GetCurrentSpeed();
			ImGui::Text("Distance: %.1f m", fDistance);
			ImGui::Text("Score: %u", GetScore());
			ImGui::Text("High Score: %u", GetHighScore());
			ImGui::Text("Speed: %.1f", fSpeed);

			const char* szCharStates[] = { "RUNNING", "JUMPING", "SLIDING", "DEAD" };
			ImGui::Text("Character: %s", szCharStates[static_cast<int>(Runner_CharacterController::GetState())]);
			ImGui::Text("Lane: %d", Runner_CharacterController::GetCurrentLane());
		}

		ImGui::TextWrapped("Flow decisions live in Runner_RunFlow / Runner_GameFlow / "
			"Runner_CharacterActions; state is on the graph blackboard "
			"(gameState/score/highScore).");
	}
#endif

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload kept byte-identical to the pre-conversion block.
		// The RUNTIME high score now lives on the graph blackboard; this
		// field is the authored initial value only.
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_uSerializedHighScore;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_uSerializedHighScore;
		}
	}

private:
	// The attached graph's blackboard (first slot): Runner_RunFlow on the
	// gameplay GameManager, Runner_GameFlow on the menu GameManager.
	Zenith_GraphBlackboard* TryGetGraphBlackboard()
	{
		if (!m_xParentEntity.IsValid())
		{
			return nullptr;
		}
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr || pxGraph->GetGraphCount() == 0)
		{
			return nullptr;
		}
		Zenith_BehaviourGraph* pxBehaviour = pxGraph->GetGraphAt(0);
		return pxBehaviour ? &pxBehaviour->GetBlackboard() : nullptr;
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

		// The character's input/animation DECISIONS live on its own graph
		// (runtime-spawned entity pattern: AddGraphByAssetPath); the shim
		// forwards the graph-facing surface to the static-scope modules.
		xCharacter.AddComponent<Runner_CharacterShim>();
		xCharacter.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/Runner_CharacterActions.bgraph");

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

		// Update character controller (pure movement systems - the input and
		// slide-timer decisions run on the character's graph, dispatched
		// before this component updates)
		Runner_CharacterController::Update(fDt, xTransform, fTerrainHeight);

		// Character-frame decisions (slide-timer countdown, animation state
		// mapping): fired between the controller and the animation systems -
		// exactly where the old inline decisions sat. dt rides the payload.
		FireCharTick(xCharacter, fDt);

		// Update animation driver (systems: blend param, state time,
		// procedural application - the state DECISION arrived via CharTick)
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
		if (m_bHasInjectedCollection)
		{
			// Test seam (see Test_InjectCollection): consume the injected
			// result verbatim this frame.
			m_xLastCollection = m_xInjectedCollection;
			m_bHasInjectedCollection = false;
		}
		else
		{
			m_xLastCollection = Runner_CollectibleSpawner::CheckCollectibles(xPlayerPos, fPlayerRadius);
		}
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

		// HUD text is a systems WRITE that READS the graph-owned state.
		Runner_UIManager::UpdateUI(xUI, fDistance, GetScore(), fSpeed, s_fMaxSpeed, GetGameState());
		Runner_UIManager::UpdateHighScore(xUI, GetHighScore());
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

	void FireCharTick(Zenith_Entity& xCharacter, float fDt)
	{
		Zenith_GraphComponent* pxGraph = xCharacter.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		Zenith_PropertyValue xDt;
		xDt.SetFloat(fDt);
		pxGraph->FireCustomEvent("CharTick", &xDt);
	}

	// Per-frame systems results consumed by the Runner_RunFlow graph nodes.
	Runner_CollectibleSpawner::CollectionResult m_xLastCollection;
	bool m_bObstacleHitThisFrame = false;

	// Characterization-test injection seam (Test_InjectCollection).
	Runner_CollectibleSpawner::CollectionResult m_xInjectedCollection;
	bool m_bHasInjectedCollection = false;

	// Authored initial high score: serialization ballast only (the runtime
	// high score is the graph blackboard's "highScore").
	uint32_t m_uSerializedHighScore = 0;

	Zenith_EntityID m_uCharacterEntityID = INVALID_ENTITY_ID;

	// Scene handle for the game scene
	Zenith_Scene m_xGameScene;

	// Random number generator
	std::mt19937 m_xRng;

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
