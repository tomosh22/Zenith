#pragma once
/**
 * Combat_Behaviour.h - Main game coordinator
 *
 * Demonstrates:
 * - Zenith_ScriptBehaviour lifecycle (OnAwake, OnStart, OnUpdate)
 * - Coordinator pattern delegating to specialized modules
 * - Animation state machine integration
 * - IK system integration
 * - Event-based damage system
 *
 * This behavior orchestrates all combat game systems:
 * - Player controller for input and movement
 * - Animation controller for combat animations
 * - IK controller for foot placement and look-at
 * - Hit detection for attack registration
 * - Enemy AI management
 * - UI updates
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "Maths/Zenith_Maths.h"

// Include combat modules
#include "Combat_Config.h"
#include "Combat_PlayerController.h"
#include "Combat_AnimationController.h"
#include "Combat_IKController.h"
#include "Combat_HitDetection.h"
#include "Combat_DamageSystem.h"
#include "Combat_EnemyAI.h"
#include "Combat_QueryHelper.h"
#include "Combat_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// Combat Resources - Global access
// Defined in Combat.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================

namespace Combat
{
	extern Flux_MeshGeometry* g_pxCapsuleGeometry;
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MaterialAsset* g_pxPlayerMaterial;
	extern Flux_MaterialAsset* g_pxEnemyMaterial;
	extern Flux_MaterialAsset* g_pxArenaMaterial;
	extern Flux_MaterialAsset* g_pxWallMaterial;

	extern Zenith_Prefab* g_pxPlayerPrefab;
	extern Zenith_Prefab* g_pxEnemyPrefab;
	extern Zenith_Prefab* g_pxArenaPrefab;
}

// ============================================================================
// Combat Level Entities
// ============================================================================

struct Combat_LevelEntities
{
	Zenith_EntityID m_uPlayerEntityID = INVALID_ENTITY_ID;
	std::vector<Zenith_EntityID> m_axEnemyEntityIDs;
	Zenith_EntityID m_uArenaFloorEntityID = INVALID_ENTITY_ID;
	std::vector<Zenith_EntityID> m_axArenaWallEntityIDs;
};

// ============================================================================
// Main Behavior Class
// ============================================================================

class Combat_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Combat_Behaviour)

	Combat_Behaviour() = delete;
	Combat_Behaviour(Zenith_Entity& xParentEntity)
		: m_eGameState(Combat_GameState::PLAYING)
		, m_uTotalEnemies(3)
		, m_uComboCount(0)
		, m_fComboTimer(0.0f)
		, m_xRng(std::random_device{}())
	{
	}

	~Combat_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
		// Initialize damage system
		Combat_DamageSystem::Initialize();

		// Subscribe to damage events for hit reactions
		m_uDamageEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DamageEvent>(
			[this](const Combat_DamageEvent& xEvent)
			{
				OnDamageEvent(xEvent);
			});

		// Subscribe to death events
		m_uDeathEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DeathEvent>(
			[this](const Combat_DeathEvent& xEvent)
			{
				OnDeathEvent(xEvent);
			});

		// Store resource pointers
		m_pxCapsuleGeometry = Combat::g_pxCapsuleGeometry;
		m_pxCubeGeometry = Combat::g_pxCubeGeometry;
		m_pxPlayerMaterial = Combat::g_pxPlayerMaterial;
		m_pxEnemyMaterial = Combat::g_pxEnemyMaterial;
		m_pxArenaMaterial = Combat::g_pxArenaMaterial;
		m_pxWallMaterial = Combat::g_pxWallMaterial;

		// Create the arena and spawn entities
		CreateArena();
		SpawnPlayer();
		SpawnEnemies();

		// Initialize player systems
		m_xPlayerController.m_fMoveSpeed = 5.0f;
		m_xPlayerController.m_fLightAttackDuration = 0.3f;
		m_xPlayerController.m_fHeavyAttackDuration = 0.6f;
		m_xPlayerAnimController.Initialize();
		m_xPlayerHitDetection.SetOwner(m_xLevelEntities.m_uPlayerEntityID);
	}

	void OnStart() ZENITH_FINAL override
	{
		// Ensure level is created if loaded from scene
		if (m_xLevelEntities.m_uPlayerEntityID == INVALID_ENTITY_ID)
		{
			CreateArena();
			SpawnPlayer();
			SpawnEnemies();
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Handle pause input
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			TogglePause();
		}

		// Handle reset input
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			ResetGame();
			return;
		}

		// Paused - only update UI
		if (m_eGameState == Combat_GameState::PAUSED)
		{
			UpdateUI();
			return;
		}

		// Game over states - only update UI and camera
		if (m_eGameState == Combat_GameState::VICTORY ||
			m_eGameState == Combat_GameState::GAME_OVER)
		{
			UpdateCamera(fDt);
			UpdateUI();
			return;
		}

		// Update damage system timers
		Combat_DamageSystem::Update(fDt);

		// Update player
		UpdatePlayer(fDt);

		// Update enemies
		m_xEnemyManager.Update(fDt);

		// Update combo timer
		UpdateComboTimer(fDt);

		// Check win/lose conditions
		CheckGameState();

		// Update camera
		UpdateCamera(fDt);

		// Update UI
		UpdateUI();
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Combat Arena Game");
		ImGui::Separator();

		const char* szStates[] = { "PLAYING", "PAUSED", "VICTORY", "GAME OVER" };
		ImGui::Text("State: %s", szStates[static_cast<int>(m_eGameState)]);

		ImGui::Text("Player Health: %.0f", Combat_DamageSystem::GetHealth(m_xLevelEntities.m_uPlayerEntityID));
		ImGui::Text("Enemies Alive: %u / %u", m_xEnemyManager.GetAliveCount(), m_uTotalEnemies);
		ImGui::Text("Combo: %u", m_uComboCount);

		ImGui::Separator();
		if (ImGui::Button("Reset Game"))
		{
			ResetGame();
		}

		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD: Move");
		ImGui::Text("  Left Click: Light Attack");
		ImGui::Text("  Right Click: Heavy Attack");
		ImGui::Text("  Space: Dodge");
		ImGui::Text("  P/Esc: Pause");
		ImGui::Text("  R: Reset");
#endif
	}

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_uTotalEnemies;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_uTotalEnemies;
		}
	}

private:
	// ========================================================================
	// Arena Creation
	// ========================================================================

	void CreateArena()
	{
		static constexpr float s_fArenaRadius = 15.0f;
		static constexpr float s_fArenaWallHeight = 2.0f;
		static constexpr uint32_t s_uWallSegments = 24;

		// Create floor
		Zenith_Entity xFloor = Zenith_Scene::Instantiate(*Combat::g_pxArenaPrefab, "ArenaFloor");

		Zenith_TransformComponent& xFloorTransform = xFloor.GetComponent<Zenith_TransformComponent>();
		xFloorTransform.SetPosition(Zenith_Maths::Vector3(0.0f, -0.5f, 0.0f));
		xFloorTransform.SetScale(Zenith_Maths::Vector3(s_fArenaRadius * 2.0f, 1.0f, s_fArenaRadius * 2.0f));

		Zenith_ModelComponent& xFloorModel = xFloor.AddComponent<Zenith_ModelComponent>();
		xFloorModel.AddMeshEntry(*m_pxCubeGeometry, *m_pxArenaMaterial);

		xFloor.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		m_xLevelEntities.m_uArenaFloorEntityID = xFloor.GetEntityID();

		// Create wall segments
		for (uint32_t i = 0; i < s_uWallSegments; i++)
		{
			float fAngle = (static_cast<float>(i) / s_uWallSegments) * 6.28318f;
			float fX = cos(fAngle) * s_fArenaRadius;
			float fZ = sin(fAngle) * s_fArenaRadius;

			Zenith_Entity xWall = Zenith_Scene::Instantiate(*Combat::g_pxArenaPrefab, "ArenaWall");

			Zenith_TransformComponent& xWallTransform = xWall.GetComponent<Zenith_TransformComponent>();
			xWallTransform.SetPosition(Zenith_Maths::Vector3(fX, s_fArenaWallHeight * 0.5f, fZ));
			xWallTransform.SetScale(Zenith_Maths::Vector3(2.0f, s_fArenaWallHeight, 1.0f));

			// Rotate to face center
			float fYaw = fAngle + 1.5708f;  // 90 degrees
			xWallTransform.SetRotation(glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

			Zenith_ModelComponent& xWallModel = xWall.AddComponent<Zenith_ModelComponent>();
			xWallModel.AddMeshEntry(*m_pxCubeGeometry, *m_pxWallMaterial);

			xWall.AddComponent<Zenith_ColliderComponent>()
				.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

			m_xLevelEntities.m_axArenaWallEntityIDs.push_back(xWall.GetEntityID());
		}
	}

	void SpawnPlayer()
	{
		Zenith_Entity xPlayer = Zenith_Scene::Instantiate(*Combat::g_pxPlayerPrefab, "Player");

		Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xTransform.SetScale(Zenith_Maths::Vector3(0.8f, 2.0f, 0.8f));  // Capsule proportions

		Zenith_ModelComponent& xModel = xPlayer.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCapsuleGeometry, *m_pxPlayerMaterial);

		xPlayer.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

		m_xLevelEntities.m_uPlayerEntityID = xPlayer.GetEntityID();

		// Register with damage system
		Combat_DamageSystem::RegisterEntity(xPlayer.GetEntityID(), 100.0f, 0.2f);

		// Initialize hit detection
		m_xPlayerHitDetection.SetOwner(xPlayer.GetEntityID());
	}

	void SpawnEnemies()
	{
		static constexpr float s_fSpawnRadius = 10.0f;

		std::uniform_real_distribution<float> xAngleDist(0.0f, 6.28318f);
		std::uniform_real_distribution<float> xRadiusDist(5.0f, s_fSpawnRadius);

		for (uint32_t i = 0; i < m_uTotalEnemies; i++)
		{
			float fAngle = xAngleDist(m_xRng);
			float fRadius = xRadiusDist(m_xRng);
			float fX = cos(fAngle) * fRadius;
			float fZ = sin(fAngle) * fRadius;

			char szName[32];
			snprintf(szName, sizeof(szName), "Enemy_%u", i);

			Zenith_Entity xEnemy = Zenith_Scene::Instantiate(*Combat::g_pxEnemyPrefab, szName);

			Zenith_TransformComponent& xTransform = xEnemy.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(Zenith_Maths::Vector3(fX, 1.0f, fZ));
			xTransform.SetScale(Zenith_Maths::Vector3(0.7f, 1.8f, 0.7f));  // Slightly smaller than player

			Zenith_ModelComponent& xModel = xEnemy.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxCapsuleGeometry, *m_pxEnemyMaterial);

			xEnemy.AddComponent<Zenith_ColliderComponent>()
				.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

			m_xLevelEntities.m_axEnemyEntityIDs.push_back(xEnemy.GetEntityID());

			// Register with damage system
			Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f, 0.0f);

			// Register with enemy manager
			Combat_EnemyConfig xConfig;
			xConfig.m_fMoveSpeed = 3.0f;
			xConfig.m_fAttackDamage = 15.0f;
			xConfig.m_fAttackRange = 1.5f;
			xConfig.m_fAttackCooldown = 1.5f;
			m_xEnemyManager.RegisterEnemy(xEnemy.GetEntityID(), xConfig);
		}
	}

	// ========================================================================
	// Player Update
	// ========================================================================

	void UpdatePlayer(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
			return;

		Zenith_Entity xPlayer = xScene.GetEntityByID(m_xLevelEntities.m_uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_TransformComponent>() ||
			!xPlayer.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		Zenith_ColliderComponent& xCollider = xPlayer.GetComponent<Zenith_ColliderComponent>();

		// Check if player is dead
		if (Combat_DamageSystem::IsDead(m_xLevelEntities.m_uPlayerEntityID))
		{
			m_xPlayerController.TriggerDeath();
		}

		// Update player controller
		m_xPlayerController.Update(xTransform, xCollider, fDt);

		// Update animation
		m_xPlayerAnimController.UpdateFromPlayerState(m_xPlayerController, fDt);

		// Update IK
		bool bCanUseIK = !m_xPlayerController.IsDodging() &&
			m_xPlayerController.GetState() != Combat_PlayerState::DEAD;
		m_xPlayerIKController.UpdateWithAutoTarget(xTransform, m_xLevelEntities.m_uPlayerEntityID, 0.0f, bCanUseIK, fDt);

		// Handle attack hit detection
		UpdatePlayerAttack(xTransform);
	}

	void UpdatePlayerAttack(Zenith_TransformComponent& xTransform)
	{
		// Start hitbox when attack begins
		if (m_xPlayerController.WasAttackJustStarted())
		{
			Combat_AttackType eType = m_xPlayerController.GetCurrentAttackType();
			float fDamage = (eType == Combat_AttackType::HEAVY) ? 25.0f : 10.0f;
			float fRange = (eType == Combat_AttackType::HEAVY) ? 2.0f : 1.5f;
			uint32_t uCombo = m_xPlayerController.GetComboCount();

			m_xPlayerHitDetection.ActivateHitbox(fDamage, fRange, uCombo, uCombo > 1);
		}

		// Check for hits during attack
		if (m_xPlayerController.IsAttacking() && m_xPlayerAnimController.IsAttackHitFrame())
		{
			uint32_t uHits = m_xPlayerHitDetection.Update(xTransform);
			if (uHits > 0)
			{
				// Extend combo timer on hit
				m_uComboCount = m_xPlayerController.GetComboCount();
				m_fComboTimer = 2.0f;
			}
		}

		// Deactivate hitbox when attack ends
		if (!m_xPlayerController.IsAttacking())
		{
			m_xPlayerHitDetection.DeactivateHitbox();
		}
	}

	// ========================================================================
	// Event Handlers
	// ========================================================================

	void OnDamageEvent(const Combat_DamageEvent& xEvent)
	{
		// Trigger hit stun on player
		if (xEvent.m_uTargetEntityID == m_xLevelEntities.m_uPlayerEntityID)
		{
			m_xPlayerController.TriggerHitStun();
		}
		else
		{
			// Trigger hit stun on enemies
			m_xEnemyManager.TriggerHitStunForEntity(xEvent.m_uTargetEntityID);
		}
	}

	void OnDeathEvent(const Combat_DeathEvent& xEvent)
	{
		// Player died
		if (xEvent.m_uEntityID == m_xLevelEntities.m_uPlayerEntityID)
		{
			m_eGameState = Combat_GameState::GAME_OVER;
		}
	}

	// ========================================================================
	// Camera Update
	// ========================================================================

	void UpdateCamera(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID)
			return;

		Zenith_Entity xCamEntity = xScene.GetEntityByID(uCamID);
		if (!xCamEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Get player position for camera target
		Zenith_Maths::Vector3 xPlayerPos(0.0f);
		if (xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntityByID(m_xLevelEntities.m_uPlayerEntityID);
			xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
		}

		// Isometric-style camera position
		static constexpr float s_fCamDistance = 15.0f;
		static constexpr float s_fCamHeight = 12.0f;
		static constexpr float s_fCamPitch = -0.7f;

		Zenith_Maths::Vector3 xCamTarget = xPlayerPos;
		Zenith_Maths::Vector3 xCamPos = xCamTarget + Zenith_Maths::Vector3(0.0f, s_fCamHeight, -s_fCamDistance);

		// Smooth follow
		Zenith_Maths::Vector3 xCurrentPos;
		xCamera.GetPosition(xCurrentPos);
		xCamPos = glm::mix(xCurrentPos, xCamPos, fDt * 5.0f);

		xCamera.SetPosition(xCamPos);
		xCamera.SetPitch(s_fCamPitch);
		xCamera.SetYaw(0.0f);
	}

	// ========================================================================
	// Game State
	// ========================================================================

	void UpdateComboTimer(float fDt)
	{
		if (m_fComboTimer > 0.0f)
		{
			m_fComboTimer -= fDt;
			if (m_fComboTimer <= 0.0f)
			{
				m_uComboCount = 0;
			}
		}
	}

	void CheckGameState()
	{
		// Check for victory
		if (m_xEnemyManager.GetAliveCount() == 0)
		{
			m_eGameState = Combat_GameState::VICTORY;
		}

		// Check for game over
		if (Combat_DamageSystem::IsDead(m_xLevelEntities.m_uPlayerEntityID))
		{
			m_eGameState = Combat_GameState::GAME_OVER;
		}
	}

	void TogglePause()
	{
		if (m_eGameState == Combat_GameState::PLAYING)
		{
			m_eGameState = Combat_GameState::PAUSED;
		}
		else if (m_eGameState == Combat_GameState::PAUSED)
		{
			m_eGameState = Combat_GameState::PLAYING;
		}
	}

	void ResetGame()
	{
		// Destroy all level entities
		DestroyLevel();

		// Reset damage system
		Combat_DamageSystem::Reset();

		// Reset enemy manager
		m_xEnemyManager.Reset();

		// Reset player systems
		m_xPlayerController.Reset();
		m_xPlayerAnimController.Reset();
		m_xPlayerIKController.Reset();
		m_xPlayerHitDetection.DeactivateHitbox();

		// Reset game state
		m_eGameState = Combat_GameState::PLAYING;
		m_uComboCount = 0;
		m_fComboTimer = 0.0f;

		// Recreate level
		CreateArena();
		SpawnPlayer();
		SpawnEnemies();
	}

	void DestroyLevel()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Destroy player
		if (m_xLevelEntities.m_uPlayerEntityID != INVALID_ENTITY_ID &&
			xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
		{
			Zenith_Scene::Destroy(m_xLevelEntities.m_uPlayerEntityID);
		}
		m_xLevelEntities.m_uPlayerEntityID = INVALID_ENTITY_ID;

		// Destroy enemies
		for (Zenith_EntityID uID : m_xLevelEntities.m_axEnemyEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_xLevelEntities.m_axEnemyEntityIDs.clear();

		// Destroy arena
		if (m_xLevelEntities.m_uArenaFloorEntityID != INVALID_ENTITY_ID &&
			xScene.EntityExists(m_xLevelEntities.m_uArenaFloorEntityID))
		{
			Zenith_Scene::Destroy(m_xLevelEntities.m_uArenaFloorEntityID);
		}
		m_xLevelEntities.m_uArenaFloorEntityID = INVALID_ENTITY_ID;

		for (Zenith_EntityID uID : m_xLevelEntities.m_axArenaWallEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_xLevelEntities.m_axArenaWallEntityIDs.clear();
	}

	// ========================================================================
	// UI Update
	// ========================================================================

	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		float fPlayerHealth = Combat_DamageSystem::GetHealth(m_xLevelEntities.m_uPlayerEntityID);
		float fPlayerMaxHealth = Combat_DamageSystem::GetMaxHealth(m_xLevelEntities.m_uPlayerEntityID);

		Combat_UIManager::UpdateAll(
			xUI,
			fPlayerHealth,
			fPlayerMaxHealth,
			m_uComboCount,
			m_fComboTimer,
			m_xEnemyManager.GetAliveCount(),
			m_uTotalEnemies,
			m_eGameState);
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	Combat_GameState m_eGameState;
	uint32_t m_uTotalEnemies;
	uint32_t m_uComboCount;
	float m_fComboTimer;

	std::mt19937 m_xRng;

	// Level entities
	Combat_LevelEntities m_xLevelEntities;

	// Player systems
	Combat_PlayerController m_xPlayerController;
	Combat_AnimationController m_xPlayerAnimController;
	Combat_IKController m_xPlayerIKController;
	Combat_HitDetection m_xPlayerHitDetection;

	// Enemy manager
	Combat_EnemyManager m_xEnemyManager;

	// Event handles
	Zenith_EventHandle m_uDamageEventHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_uDeathEventHandle = INVALID_EVENT_HANDLE;

public:
	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* m_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* m_pxEnemyMaterial = nullptr;
	Flux_MaterialAsset* m_pxArenaMaterial = nullptr;
	Flux_MaterialAsset* m_pxWallMaterial = nullptr;
};
