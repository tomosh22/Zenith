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
#include "Flux/Flux_ModelInstance.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Quads/Flux_Quads.h"

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
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// Combat Resources - Global access
// Defined in Combat.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================

class Flux_ParticleEmitterConfig;

namespace Combat
{
	extern Flux_MeshGeometry* g_pxCapsuleGeometry;
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxConeGeometry;  // Cone mesh for candles
	extern Flux_MeshGeometry* g_pxStickFigureGeometry;  // Animated humanoid mesh (skinned)
	extern Zenith_ModelAsset* g_pxStickFigureModelAsset;  // Model asset with skeleton
	extern std::string g_strStickFigureModelPath;  // Path to model asset for LoadModelFromFile
	extern Zenith_MaterialAsset* g_pxPlayerMaterial;
	extern Zenith_MaterialAsset* g_pxEnemyMaterial;
	extern Zenith_MaterialAsset* g_pxArenaMaterial;
	extern Zenith_MaterialAsset* g_pxWallMaterial;
	extern Zenith_MaterialAsset* g_pxCandleMaterial;  // Cream color for candles

	extern Zenith_Prefab* g_pxPlayerPrefab;
	extern Zenith_Prefab* g_pxEnemyPrefab;
	extern Zenith_Prefab* g_pxArenaPrefab;
	extern Zenith_Prefab* g_pxArenaWallPrefab;  // Wall segment with candle and flame

	// Particle effects
	extern Flux_ParticleEmitterConfig* g_pxHitSparkConfig;
	extern Zenith_EntityID g_uHitSparkEmitterID;
	extern Flux_ParticleEmitterConfig* g_pxFlameConfig;  // Candle flame particles
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
		// Clear stale state from previous play sessions
		// This is critical for Play/Stop/Play cycles in the editor
		m_xEnemyManager.Reset();
		m_xLevelEntities = Combat_LevelEntities();  // Reset to default state

		// Clear any stale events from previous sessions
		s_axDeferredDamageEvents.clear();
		s_axDeferredDeathEvents.clear();

		// Unsubscribe old event handles to prevent orphaned subscriptions
		// (can happen after Play/Stop cycle recreates the behaviour)
		if (s_uDamageEventHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(s_uDamageEventHandle);
			s_uDamageEventHandle = INVALID_EVENT_HANDLE;
		}
		if (s_uDeathEventHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(s_uDeathEventHandle);
			s_uDeathEventHandle = INVALID_EVENT_HANDLE;
		}

		// Subscribe to damage events for hit reactions
		// IMPORTANT: Use static queues to avoid captured 'this' pointer issues
		// The lambda only adds to the queue; processing happens in OnUpdate where 'this' is valid
		s_uDamageEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DamageEvent>(
			[](const Combat_DamageEvent& xEvent)
			{
				s_axDeferredDamageEvents.push_back(xEvent);
			});

		// Subscribe to death events
		s_uDeathEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DeathEvent>(
			[](const Combat_DeathEvent& xEvent)
			{
				s_axDeferredDeathEvents.push_back(xEvent);
			});

		// Store resource pointers
		m_pxCapsuleGeometry = Combat::g_pxCapsuleGeometry;
		m_pxCubeGeometry = Combat::g_pxCubeGeometry;
		m_pxStickFigureGeometry = Combat::g_pxStickFigureGeometry;
		m_pxPlayerMaterial = Combat::g_pxPlayerMaterial;
		m_pxEnemyMaterial = Combat::g_pxEnemyMaterial;
		m_pxArenaMaterial = Combat::g_pxArenaMaterial;
		m_pxWallMaterial = Combat::g_pxWallMaterial;
	}

	void OnStart() ZENITH_FINAL override
	{
		// Guard: Skip if already initialized
		if (m_xLevelEntities.m_uPlayerEntityID != INVALID_ENTITY_ID)
			return;

		// Initialize damage system (resets health data for new play session)
		Combat_DamageSystem::Initialize();

		// Find pre-created entities by name (created in Project_LoadInitialScene)
		FindSceneEntities();

		// Spawn enemies
		SpawnEnemies();

		// Log initialization state for debugging
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] OnStart complete: playerID=%u, enemyCount=%zu, managerSize=%zu",
			m_xLevelEntities.m_uPlayerEntityID.m_uIndex,
			m_xLevelEntities.m_axEnemyEntityIDs.size(),
			m_xEnemyManager.GetEnemies().size());

		// Initialize player systems
		m_xPlayerController.m_fMoveSpeed = 5.0f;
		m_xPlayerController.m_fLightAttackDuration = 0.3f;
		m_xPlayerController.m_fHeavyAttackDuration = 0.6f;

		// Initialize animation controller if player has skeleton
		InitializePlayerAnimation();

		m_xPlayerHitDetection.SetOwner(m_xLevelEntities.m_uPlayerEntityID);
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

		// Process deferred damage/death events AFTER updates complete
		// Events are queued during updates to avoid re-entrancy issues with captured 'this'
		ProcessDeferredEvents();

		// Update combo timer
		UpdateComboTimer(fDt);

		// Check win/lose conditions
		CheckGameState();

		// Update camera
		UpdateCamera(fDt);

		// Update UI
		UpdateUI();

		// Update animation state labels and health bars above entities
		UpdateEntityOverheadDisplay();
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
	// Entity Lookup (find pre-created entities from Project_LoadInitialScene)
	// ========================================================================

	void FindSceneEntities()
	{
		static constexpr uint32_t s_uWallSegments = 24;

		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Find player entity
		Zenith_Entity xPlayer = xScene.FindEntityByName("Player");
		Zenith_Assert(xPlayer.IsValid(), "Player entity not found in scene - ensure scene was saved after Project_LoadInitialScene created entities");
		m_xLevelEntities.m_uPlayerEntityID = xPlayer.GetEntityID();
		Combat_DamageSystem::RegisterEntity(xPlayer.GetEntityID(), 100.0f, 0.2f);

		// Verify player is properly registered
		bool bPlayerIsDead = Combat_DamageSystem::IsDead(xPlayer.GetEntityID());
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] FindSceneEntities: playerID=%u, isDead=%d (should be 0)",
			xPlayer.GetEntityID().m_uIndex, bPlayerIsDead);

		// Find arena floor
		Zenith_Entity xFloor = xScene.FindEntityByName("ArenaFloor");
		Zenith_Assert(xFloor.IsValid(), "ArenaFloor entity not found in scene");
		m_xLevelEntities.m_uArenaFloorEntityID = xFloor.GetEntityID();

		// Find walls
		for (uint32_t i = 0; i < s_uWallSegments; i++)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "ArenaWall_%u", i);
			Zenith_Entity xWall = xScene.FindEntityByName(szName);
			Zenith_Assert(xWall.IsValid(), "ArenaWall entity not found in scene: %s", szName);
			m_xLevelEntities.m_axArenaWallEntityIDs.push_back(xWall.GetEntityID());
		}
	}

	void InitializePlayerAnimation()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
			return;

		Zenith_Entity xPlayer = xScene.GetEntity(m_xLevelEntities.m_uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_ModelComponent>())
			return;

		Zenith_ModelComponent& xModel = xPlayer.GetComponent<Zenith_ModelComponent>();
		if (xModel.HasSkeleton())
		{
			Flux_SkeletonInstance* pxSkeleton = xModel.GetSkeletonInstance();
			if (pxSkeleton)
			{
				m_xPlayerAnimController.Initialize(pxSkeleton);
				Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] Player animation controller initialized with skeleton instance");
			}
		}
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

			Zenith_Entity xEnemy = Combat::g_pxEnemyPrefab->Instantiate(&Zenith_Scene::GetCurrentScene(), szName);

			Zenith_TransformComponent& xTransform = xEnemy.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(Zenith_Maths::Vector3(fX, 1.0f, fZ));  // Start above floor
			xTransform.SetScale(Zenith_Maths::Vector3(0.9f, 0.9f, 0.9f));  // Slightly smaller than player

			Zenith_ModelComponent& xModel = xEnemy.AddComponent<Zenith_ModelComponent>();

			// Use model instance system with skeleton for animated rendering
			bool bUsingEnemyModel = false;
			if (!Combat::g_strStickFigureModelPath.empty())
			{
				xModel.LoadModel(Combat::g_strStickFigureModelPath);
				// Check if model loaded successfully with a skeleton
				if (xModel.GetModelInstance() && xModel.HasSkeleton())
				{
					xModel.GetModelInstance()->SetMaterial(0, m_pxEnemyMaterial);
					bUsingEnemyModel = true;
				}
			}

			// Fallback to mesh entry if model instance failed
			if (!bUsingEnemyModel)
			{
				xModel.AddMeshEntry(*m_pxStickFigureGeometry, *m_pxEnemyMaterial);
			}

			// Use explicit capsule dimensions for humanoid enemy (slightly smaller than player)
			Zenith_ColliderComponent& xCollider = xEnemy.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCapsuleCollider(0.27f, 0.54f, RIGIDBODY_TYPE_DYNAMIC);

			// Lock X and Z rotation to prevent character from tipping over
			Zenith_Physics::LockRotation(xCollider.GetBodyID(), true, false, true);

			m_xLevelEntities.m_axEnemyEntityIDs.push_back(xEnemy.GetEntityID());

			// Register with damage system
			Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f, 0.0f);

			// Register with enemy manager - pass skeleton instance if available
			Combat_EnemyConfig xConfig;
			xConfig.m_fMoveSpeed = 3.0f;
			xConfig.m_fAttackDamage = 15.0f;
			xConfig.m_fAttackRange = 1.5f;  // Accounts for physics collision settling distance
			xConfig.m_fAttackCooldown = 1.5f;

			Flux_SkeletonInstance* pxSkeleton = xModel.HasSkeleton() ? xModel.GetSkeletonInstance() : nullptr;
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] Spawning enemy %u: hasSkel=%d, skelPtr=%p, bUsingModel=%d",
				xEnemy.GetEntityID().m_uIndex, xModel.HasSkeleton(), pxSkeleton, bUsingEnemyModel);
			m_xEnemyManager.RegisterEnemy(xEnemy.GetEntityID(), xConfig, pxSkeleton);
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

		Zenith_Entity xPlayer = xScene.GetEntity(m_xLevelEntities.m_uPlayerEntityID);
		if (!xPlayer.HasComponent<Zenith_TransformComponent>() ||
			!xPlayer.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		Zenith_ColliderComponent& xCollider = xPlayer.GetComponent<Zenith_ColliderComponent>();

		// Enforce upright orientation every frame (collision impulses can still tip characters)
		if (xCollider.HasValidBody())
		{
			Zenith_Physics::EnforceUpright(xCollider.GetBodyID());
		}

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

	/**
	 * ProcessDeferredEvents - Process damage and death events queued during updates
	 * This is called from OnUpdate where 'this' is guaranteed valid, avoiding
	 * issues with captured 'this' pointers in event callbacks becoming stale.
	 */
	void ProcessDeferredEvents()
	{
		// Process damage events
		for (const Combat_DamageEvent& xEvent : s_axDeferredDamageEvents)
		{
			OnDamageEvent(xEvent);
		}
		s_axDeferredDamageEvents.clear();

		// Process death events
		for (const Combat_DeathEvent& xEvent : s_axDeferredDeathEvents)
		{
			OnDeathEvent(xEvent);
		}
		s_axDeferredDeathEvents.clear();
	}

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

		// Spawn hit spark particles at the hit location
		SpawnHitParticles(xEvent);
	}

	void SpawnHitParticles(const Combat_DamageEvent& xEvent)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Use hit point from the event, or fall back to target position
		Zenith_Maths::Vector3 xHitPos = xEvent.m_xHitPoint;
		if (glm::length(xHitPos) < 0.001f && xScene.EntityExists(xEvent.m_uTargetEntityID))
		{
			Zenith_Entity xTarget = xScene.GetEntity(xEvent.m_uTargetEntityID);
			if (xTarget.HasComponent<Zenith_TransformComponent>())
			{
				xTarget.GetComponent<Zenith_TransformComponent>().GetPosition(xHitPos);
				xHitPos.y += 1.0f;  // Offset to chest height
			}
		}

		// Use hit direction from the event
		Zenith_Maths::Vector3 xHitDir = xEvent.m_xHitDirection;
		if (glm::length(xHitDir) < 0.001f)
		{
			xHitDir = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);  // Default up
		}

		// Spawn particles at the hit emitter
		if (Combat::g_uHitSparkEmitterID != INVALID_ENTITY_ID &&
			xScene.EntityExists(Combat::g_uHitSparkEmitterID))
		{
			Zenith_Entity xEmitterEntity = xScene.GetEntity(Combat::g_uHitSparkEmitterID);
			if (xEmitterEntity.HasComponent<Zenith_ParticleEmitterComponent>())
			{
				Zenith_ParticleEmitterComponent& xEmitter = xEmitterEntity.GetComponent<Zenith_ParticleEmitterComponent>();
				xEmitter.SetEmitPosition(xHitPos);
				xEmitter.SetEmitDirection(xHitDir);

				// Scale particle count based on damage (more damage = more particles)
				uint32_t uCount = static_cast<uint32_t>(10 + xEvent.m_fDamage * 0.5f);
				xEmitter.Emit(uCount);
			}
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

		Zenith_Entity xCamEntity = xScene.GetEntity(uCamID);
		if (!xCamEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		// Get player position for camera target
		Zenith_Maths::Vector3 xPlayerPos(0.0f);
		if (xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntity(m_xLevelEntities.m_uPlayerEntityID);
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
		// Destroy only enemies (arena and player are persistent)
		DestroyEnemies();

		// Reset damage system and re-register player
		Combat_DamageSystem::Reset();
		Combat_DamageSystem::RegisterEntity(m_xLevelEntities.m_uPlayerEntityID, 100.0f, 0.2f);

		// Reset enemy manager
		m_xEnemyManager.Reset();

		// Clear deferred event queues
		s_axDeferredDamageEvents.clear();
		s_axDeferredDeathEvents.clear();

		// Reset player systems
		m_xPlayerController.Reset();
		m_xPlayerAnimController.Reset();
		m_xPlayerIKController.Reset();
		m_xPlayerHitDetection.DeactivateHitbox();

		// Reset player position
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntity(m_xLevelEntities.m_uPlayerEntityID);
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		}

		// Reset game state
		m_eGameState = Combat_GameState::PLAYING;
		m_uComboCount = 0;
		m_fComboTimer = 0.0f;

		// Respawn enemies
		SpawnEnemies();
	}

	void DestroyEnemies()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Destroy enemies only (player and arena are persistent)
		for (Zenith_EntityID uID : m_xLevelEntities.m_axEnemyEntityIDs)
		{
			if (xScene.EntityExists(uID))
				Zenith_Scene::Destroy(uID);
		}
		m_xLevelEntities.m_axEnemyEntityIDs.clear();
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
	// Animation State Labels (disabled - Zenith_TextComponent removed)
	// ========================================================================

	void UpdateAnimationStateLabels()
	{
		// Disabled: Zenith_TextComponent has been removed from the engine.
		// Animation state labels were debug-only features.
	}

	// ========================================================================
	// Health Bar Rendering
	// ========================================================================

	/**
	 * WorldToScreen - Project a world position to screen-space pixel coordinates
	 * @param xWorldPos World position to project
	 * @param xViewMatrix Camera view matrix
	 * @param xProjMatrix Camera projection matrix
	 * @param xScreenPos Output screen position in pixels
	 * @return true if position is in front of camera, false if behind
	 */
	bool WorldToScreen(const Zenith_Maths::Vector3& xWorldPos,
		const Zenith_Maths::Matrix4& xViewMatrix,
		const Zenith_Maths::Matrix4& xProjMatrix,
		Zenith_Maths::Vector2& xScreenPos)
	{
		Zenith_Maths::Vector4 xClipPos = xProjMatrix * xViewMatrix * Zenith_Maths::Vector4(xWorldPos, 1.0f);

		// Behind camera check
		if (xClipPos.w <= 0.0f)
			return false;

		// Perspective divide to NDC
		Zenith_Maths::Vector3 xNDC = Zenith_Maths::Vector3(xClipPos) / xClipPos.w;

		// NDC to screen coordinates (Y=0 at bottom for Flux_Quads)
		int32_t iWidth, iHeight;
		Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
		xScreenPos.x = (xNDC.x * 0.5f + 0.5f) * static_cast<float>(iWidth);
		xScreenPos.y = (xNDC.y * 0.5f + 0.5f) * static_cast<float>(iHeight);

		return true;
	}

	/**
	 * RenderHealthBarQuad - Render a health bar at screen position using Flux_Quads
	 * @param xScreenPos Screen position (center of bar)
	 * @param fHealthPercent Health percentage (0-1)
	 * @param uBarWidth Width in pixels
	 * @param uBarHeight Height in pixels
	 */
	void RenderHealthBarQuad(const Zenith_Maths::Vector2& xScreenPos, float fHealthPercent, uint32_t uBarWidth = 60, uint32_t uBarHeight = 8)
	{
		fHealthPercent = glm::clamp(fHealthPercent, 0.0f, 1.0f);

		uint32_t uX = static_cast<uint32_t>(xScreenPos.x - uBarWidth / 2);
		uint32_t uY = static_cast<uint32_t>(xScreenPos.y);

		// Background bar (dark grey)
		Flux_Quads::Quad xBgQuad;
		xBgQuad.m_xPosition_Size = Zenith_Maths::UVector4(uX, uY, uBarWidth, uBarHeight);
		xBgQuad.m_xColour = Zenith_Maths::Vector4(0.15f, 0.15f, 0.15f, 0.9f);
		xBgQuad.m_uTexture = 0;  // No texture
		xBgQuad.m_xUVMult_UVAdd = Zenith_Maths::Vector2(0.0f, 0.0f);
		Flux_Quads::UploadQuad(xBgQuad);

		// Foreground bar (colored by health)
		if (fHealthPercent > 0.0f)
		{
			uint32_t uFgWidth = static_cast<uint32_t>(uBarWidth * fHealthPercent);
			if (uFgWidth > 0)
			{
				// Color based on health percentage
				Zenith_Maths::Vector4 xFgColor;
				if (fHealthPercent > 0.6f)
				{
					xFgColor = Zenith_Maths::Vector4(0.2f, 0.9f, 0.2f, 1.0f);  // Green
				}
				else if (fHealthPercent > 0.3f)
				{
					xFgColor = Zenith_Maths::Vector4(0.9f, 0.8f, 0.2f, 1.0f);  // Yellow
				}
				else
				{
					xFgColor = Zenith_Maths::Vector4(0.9f, 0.2f, 0.2f, 1.0f);  // Red
				}

				Flux_Quads::Quad xFgQuad;
				xFgQuad.m_xPosition_Size = Zenith_Maths::UVector4(uX + 1, uY + 1, uFgWidth - 2, uBarHeight - 2);
				xFgQuad.m_xColour = xFgColor;
				xFgQuad.m_uTexture = 0;
				xFgQuad.m_xUVMult_UVAdd = Zenith_Maths::Vector2(0.0f, 0.0f);
				Flux_Quads::UploadQuad(xFgQuad);
			}
		}
	}

	/**
	 * UpdateHealthBars - Render health bars above all entities using Flux_Quads
	 */
	void UpdateHealthBars()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Get camera matrices for world-to-screen projection
		Zenith_EntityID uCamID = xScene.GetMainCameraEntity();
		if (uCamID == INVALID_ENTITY_ID)
			return;

		Zenith_Entity xCamEntity = xScene.GetEntity(uCamID);
		if (!xCamEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCamera = xCamEntity.GetComponent<Zenith_CameraComponent>();

		Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
		xCamera.BuildViewMatrix(xViewMatrix);
		xCamera.BuildProjectionMatrix(xProjMatrix);

		// Health bar offset above entity
		static constexpr float fBarHeightOffset = 2.3f;

		// Player health bar
		if (xScene.EntityExists(m_xLevelEntities.m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntity(m_xLevelEntities.m_uPlayerEntityID);
			if (xPlayer.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xWorldPos;
				xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xWorldPos);
				xWorldPos.y += fBarHeightOffset;

				Zenith_Maths::Vector2 xScreenPos;
				if (WorldToScreen(xWorldPos, xViewMatrix, xProjMatrix, xScreenPos))
				{
					float fHealthPercent = Combat_DamageSystem::GetHealthPercent(m_xLevelEntities.m_uPlayerEntityID);
					RenderHealthBarQuad(xScreenPos, fHealthPercent, 80, 10);  // Larger bar for player
				}
			}
		}

		// Enemy health bars
		for (const Combat_EnemyAI& xEnemy : m_xEnemyManager.GetEnemies())
		{
			Zenith_EntityID uEnemyID = xEnemy.GetEntityID();
			if (!xScene.EntityExists(uEnemyID))
				continue;

			// Skip dead enemies
			if (!xEnemy.IsAlive())
				continue;

			Zenith_Entity xEnemyEntity = xScene.GetEntity(uEnemyID);
			if (!xEnemyEntity.HasComponent<Zenith_TransformComponent>())
				continue;

			Zenith_Maths::Vector3 xWorldPos;
			xEnemyEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xWorldPos);
			xWorldPos.y += fBarHeightOffset;

			Zenith_Maths::Vector2 xScreenPos;
			if (WorldToScreen(xWorldPos, xViewMatrix, xProjMatrix, xScreenPos))
			{
				float fHealthPercent = Combat_DamageSystem::GetHealthPercent(uEnemyID);
				RenderHealthBarQuad(xScreenPos, fHealthPercent, 60, 8);
			}
		}
	}

	/**
	 * UpdateEntityOverheadDisplay - Update animation labels and health bars
	 */
	void UpdateEntityOverheadDisplay()
	{
		UpdateAnimationStateLabels();
		UpdateHealthBars();
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

	// Static event handles (persist across behaviour instances for proper cleanup)
	static inline Zenith_EventHandle s_uDamageEventHandle = INVALID_EVENT_HANDLE;
	static inline Zenith_EventHandle s_uDeathEventHandle = INVALID_EVENT_HANDLE;

	// Static event queues for deferred processing
	// These are static so lambda callbacks don't need to capture 'this'
	static inline std::vector<Combat_DamageEvent> s_axDeferredDamageEvents;
	static inline std::vector<Combat_DeathEvent> s_axDeferredDeathEvents;

public:
	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxStickFigureGeometry = nullptr;  // Animated character mesh
	Zenith_MaterialAsset* m_pxPlayerMaterial = nullptr;
	Zenith_MaterialAsset* m_pxEnemyMaterial = nullptr;
	Zenith_MaterialAsset* m_pxArenaMaterial = nullptr;
	Zenith_MaterialAsset* m_pxWallMaterial = nullptr;
};
