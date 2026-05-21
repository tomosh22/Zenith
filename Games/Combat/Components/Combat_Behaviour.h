#pragma once
/**
 * Combat_Behaviour.h - Main game coordinator
 *
 * Demonstrates:
 * - Zenith_ScriptBehaviour lifecycle (OnAwake, OnStart, OnUpdate)
 * - Multi-scene architecture: persistent GameManager + arena scene
 * - DontDestroyOnLoad for persistent entities
 * - CreateEmptyScene / UnloadScene for level transitions
 * - Zenith_UIButton for menu interaction
 * - Coordinator pattern delegating to specialized modules
 * - Animation state machine integration
 * - IK system integration
 * - Event-based damage system
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Input/Zenith_InputImpl.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Prefab/Zenith_Prefab.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "UI/Zenith_UIButton.h"

// Include combat modules
#include "Combat_Config.h"
#include "Combat_DamageSystem.h"
#include "Combat_EnemyAI.h"             // for Combat_EnemyConfig (passed to per-enemy script)
#include "Combat_QueryHelper.h"
#include "Combat_UIManager.h"
#include "Combat_PlayerBehaviour.h"     // attached to player entity in CreateArena
#include "Combat_EnemyBehaviour.h"      // attached to each enemy in SpawnEnemies
#include "EntityComponent/Components/Zenith_LightComponent.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// Combat Resources - Phase 8 per-game ProjectResources struct.
// ============================================================================

class Flux_ParticleEmitterConfig;

namespace Combat
{
	// Enemy variant prefabs — three Scale tiers (weak/normal/strong) created
	// from m_xEnemyPrefab via the prefab variant override system.
	static constexpr u_int uENEMY_VARIANT_COUNT = 3;

	struct CombatResources
	{
		MeshGeometryHandle  m_xCapsuleAsset;
		MeshGeometryHandle  m_xCubeAsset;
		MeshGeometryHandle  m_xConeAsset;
		MeshGeometryHandle  m_xStickFigureGeometryAsset;
		Flux_MeshGeometry*  m_pxCapsuleGeometry     = nullptr;
		Flux_MeshGeometry*  m_pxCubeGeometry        = nullptr;
		Flux_MeshGeometry*  m_pxConeGeometry        = nullptr;
		Flux_MeshGeometry*  m_pxStickFigureGeometry = nullptr;
		ModelHandle         m_xStickFigureModelAsset;
		std::string         m_strStickFigureModelPath;
		MaterialHandle      m_xPlayerMaterial;
		MaterialHandle      m_xEnemyMaterial;
		MaterialHandle      m_xArenaMaterial;
		MaterialHandle      m_xWallMaterial;
		MaterialHandle      m_xCandleMaterial;

		PrefabHandle        m_xPlayerPrefab;
		PrefabHandle        m_xEnemyPrefab;
		PrefabHandle        m_xArenaPrefab;
		PrefabHandle        m_xArenaWallPrefab;

		PrefabHandle        m_axEnemyVariants[uENEMY_VARIANT_COUNT];

		Flux_ParticleEmitterConfig* m_pxHitSparkConfig    = nullptr;
		Zenith_EntityID             m_uHitSparkEmitterID  = INVALID_ENTITY_ID;
		Flux_ParticleEmitterConfig* m_pxFlameConfig       = nullptr;
	};

	CombatResources& Resources();

	// Constant tables for enemy variants (.cpp-static -- shared, immutable).
	extern const char* g_aszEnemyVariantNames[uENEMY_VARIANT_COUNT];
	extern const float g_afEnemyVariantScales[uENEMY_VARIANT_COUNT];

	// Lazy init for stick figure model (assets may be created after first init attempt)
	void TryInitializeStickFigureModel();
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
	Combat_Behaviour(Zenith_Entity&)
		: m_uTotalEnemies(3)
		, m_iFocusIndex(0)
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
		s_uPlayerEntityID = INVALID_ENTITY_ID;
		s_axEnemyEntityIDs.clear();
		s_uComboCount = 0;
		s_fComboTimer = 0.0f;
		m_xLevelEntities = Combat_LevelEntities();

		s_axDeferredDamageEvents.clear();
		s_axDeferredDeathEvents.clear();

		// Unsubscribe old event handles
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

		// Subscribe to events (static queues avoid captured 'this')
		s_uDamageEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DamageEvent>(
			[](const Combat_DamageEvent& xEvent)
			{
				s_axDeferredDamageEvents.push_back(xEvent);
			});

		s_uDeathEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DeathEvent>(
			[](const Combat_DeathEvent& xEvent)
			{
				s_axDeferredDeathEvents.push_back(xEvent);
			});

		// Cache resource pointers
		m_pxCapsuleGeometry = Combat::Resources().m_pxCapsuleGeometry;
		m_pxCubeGeometry = Combat::Resources().m_pxCubeGeometry;
		m_pxStickFigureGeometry = Combat::Resources().m_pxStickFigureGeometry;
		m_xPlayerMaterial = Combat::Resources().m_xPlayerMaterial;
		m_xEnemyMaterial = Combat::Resources().m_xEnemyMaterial;
		m_xArenaMaterial = Combat::Resources().m_xArenaMaterial;
		m_xWallMaterial = Combat::Resources().m_xWallMaterial;

		Zenith_Assert(m_xEnemyMaterial.GetDirect() != nullptr,
			"Combat::Resources().m_xEnemyMaterial was not properly initialized - check InitializeCombatResources()");
		Zenith_Assert(m_xPlayerMaterial.GetDirect() != nullptr,
			"Combat::Resources().m_xPlayerMaterial was not properly initialized - check InitializeCombatResources()");

		// Wire menu button callback
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxPlayButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlayButton)
		{
			pxPlayButton->SetOnClick(&OnPlayClicked, this);
			// Start in menu state
			SetGameState(Combat_GameState::MAIN_MENU);
			m_iFocusIndex = 0;
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
		if (s_eGameState == Combat_GameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Wall lights animate always during gameplay for ambiance
		if (m_xArenaScene.IsValid())
			UpdateWallLights(fDt);

		switch (s_eGameState)
		{
		case Combat_GameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case Combat_GameState::PLAYING:
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				SetGameState(Combat_GameState::PAUSED);
				Zenith_SceneManager::SetScenePaused(m_xArenaScene, true);
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

			// GameManager only ticks GAME-WIDE state. Per-entity work (player movement,
			// enemy AI) lives on Combat_PlayerBehaviour and Combat_EnemyBehaviour and is
			// dispatched via their own OnUpdate hooks - the engine routes those for us.
			Combat_DamageSystem::Update(fDt);
			ProcessDeferredEvents();
			UpdateComboTimer(fDt);
			CheckGameState();
			UpdateCamera(fDt);
			UpdateUI();
			UpdateEntityOverheadDisplay();
			break;

		case Combat_GameState::PAUSED:
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				SetGameState(Combat_GameState::PLAYING);
				Zenith_SceneManager::SetScenePaused(m_xArenaScene, false);
			}
			else if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			UpdateUI();
			break;

		case Combat_GameState::VICTORY:
		case Combat_GameState::GAME_OVER:
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
			UpdateCamera(fDt);
			UpdateUI();
			break;
		}
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Combat Arena Game");
		ImGui::Separator();

		const char* szStates[] = { "MENU", "PLAYING", "PAUSED", "VICTORY", "GAME OVER" };
		ImGui::Text("State: %s", szStates[static_cast<int>(s_eGameState)]);

		if (s_eGameState != Combat_GameState::MAIN_MENU)
		{
			ImGui::Text("Player Health: %.0f", Combat_DamageSystem::GetHealth(s_uPlayerEntityID));
			ImGui::Text("Enemies Alive: %u / %u", CountAliveEnemies(), m_uTotalEnemies);
			ImGui::Text("Combo: %u", s_uComboCount);
		}

		ImGui::Separator();
		if (s_eGameState == Combat_GameState::MAIN_MENU)
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

		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD: Move");
		ImGui::Text("  Left Click: Light Attack");
		ImGui::Text("  Right Click: Heavy Attack");
		ImGui::Text("  Space: Dodge");
		ImGui::Text("  P: Pause");
		ImGui::Text("  R: Reset");
		ImGui::Text("  Esc: Menu");
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
	// Menu Callbacks
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

		// Create arena scene
		m_xArenaScene = Zenith_SceneManager::CreateEmptyScene("Arena");
		Zenith_SceneManager::SetActiveScene(m_xArenaScene);
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		Combat_DamageSystem::Initialize();

		// Per-entity Combat_PlayerBehaviour and Combat_EnemyBehaviour are attached to
		// each spawned entity in CreateArena/SpawnEnemies, so the central script no longer
		// needs to maintain controller/animation/IK state itself.
		CreateArena(pxSceneData);
		SpawnEnemies();

		SetGameState(Combat_GameState::PLAYING);
		s_uComboCount = 0;
		s_fComboTimer = 0.0f;
		m_fWallLightTime = 0.0f;
	}

	void ReturnToMenu()
	{
		ClearEntityReferences();

		if (m_xArenaScene.IsValid())
			Zenith_SceneManager::UnloadScene(m_xArenaScene);
		m_xArenaScene = Zenith_Scene();

		// The per-entity scripts unregister themselves on OnDestroy when the arena scene
		// unloads. Clearing the registry here is just belt-and-braces in case unload is
		// asynchronous and the next StartGame() arrives before the destroys settle.
		s_uPlayerEntityID = INVALID_ENTITY_ID;
		s_axEnemyEntityIDs.clear();
		s_uComboCount = 0;
		s_fComboTimer = 0.0f;
		Combat_DamageSystem::Reset();
		s_axDeferredDamageEvents.clear();
		s_axDeferredDeathEvents.clear();

		SetGameState(Combat_GameState::MAIN_MENU);
		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ResetGame()
	{
		ClearEntityReferences();

		if (m_xArenaScene.IsValid())
			Zenith_SceneManager::UnloadScene(m_xArenaScene);

		m_xArenaScene = Zenith_SceneManager::CreateEmptyScene("Arena");
		Zenith_SceneManager::SetActiveScene(m_xArenaScene);
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		Combat_DamageSystem::Reset();
		s_uPlayerEntityID = INVALID_ENTITY_ID;
		s_axEnemyEntityIDs.clear();
		s_uComboCount = 0;
		s_fComboTimer = 0.0f;
		s_axDeferredDamageEvents.clear();
		s_axDeferredDeathEvents.clear();

		Combat_DamageSystem::Initialize();
		CreateArena(pxSceneData);
		SpawnEnemies();

		SetGameState(Combat_GameState::PLAYING);
		m_fWallLightTime = 0.0f;
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
		if (pxPlay)
		{
			pxPlay->SetVisible(bVisible);
			pxPlay->SetFocused(bVisible);
		}
	}

	void SetHUDVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszNames[] = {
			"PlayerHealth", "PlayerHealthBar", "EnemyCount",
			"ComboCount", "ComboText", "Controls", "Status"
		};

		for (const char* szName : aszNames)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		// Keyboard focus (only 1 button, but follow the pattern)
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay)
			pxPlay->SetFocused(true);
	}

	// ========================================================================
	// Arena Creation (moved from Project_LoadInitialScene)
	// ========================================================================

	void CreateArena(Zenith_SceneData* pxSceneData)
	{
		static constexpr float s_fArenaRadius = 15.0f;
		static constexpr float s_fArenaWallHeight = 2.0f;
		static constexpr uint32_t s_uWallSegments = 24;

		// Create arena floor
		Zenith_Entity xFloor = Combat::Resources().m_xArenaPrefab.GetDirect()->Instantiate(pxSceneData, "ArenaFloor");

		Zenith_TransformComponent& xFloorTransform = xFloor.GetComponent<Zenith_TransformComponent>();
		xFloorTransform.SetPosition(Zenith_Maths::Vector3(0.0f, -0.5f, 0.0f));
		xFloorTransform.SetScale(Zenith_Maths::Vector3(s_fArenaRadius * 2.0f, 1.0f, s_fArenaRadius * 2.0f));

		Zenith_ModelComponent& xFloorModel = xFloor.AddComponent<Zenith_ModelComponent>();
		xFloorModel.AddMeshEntry(*m_pxCubeGeometry, *m_xArenaMaterial.GetDirect());

		xFloor.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		m_xLevelEntities.m_uArenaFloorEntityID = xFloor.GetEntityID();

		// Create wall segments
		for (uint32_t i = 0; i < s_uWallSegments; i++)
		{
			float fAngle = (static_cast<float>(i) / s_uWallSegments) * 6.28318f;
			float fX = cos(fAngle) * s_fArenaRadius;
			float fZ = sin(fAngle) * s_fArenaRadius;

			char szName[32];
			snprintf(szName, sizeof(szName), "ArenaWall_%u", i);
			Zenith_Entity xWall(pxSceneData, szName);

			Zenith_TransformComponent& xWallTransform = xWall.GetComponent<Zenith_TransformComponent>();
			xWallTransform.SetPosition(Zenith_Maths::Vector3(fX, s_fArenaWallHeight * 0.5f, fZ));
			xWallTransform.SetScale(Zenith_Maths::Vector3(2.0f, s_fArenaWallHeight, 1.0f));

			float fYaw = fAngle + 1.5708f;
			xWallTransform.SetRotation(glm::angleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

			Zenith_ModelComponent& xWallModel = xWall.AddComponent<Zenith_ModelComponent>();
			xWallModel.AddMeshEntry(*m_pxCubeGeometry, *m_xWallMaterial.GetDirect());
			xWallModel.AddMeshEntry(*Combat::Resources().m_pxConeGeometry, *Combat::Resources().m_xCandleMaterial.GetDirect());

			xWall.AddComponent<Zenith_ColliderComponent>()
				.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

			Zenith_ParticleEmitterComponent& xFlameEmitter = xWall.AddComponent<Zenith_ParticleEmitterComponent>();
			xFlameEmitter.SetConfig(Combat::Resources().m_pxFlameConfig);
			xFlameEmitter.SetEmitting(true);
			xFlameEmitter.SetEmitPosition(Zenith_Maths::Vector3(fX, s_fArenaWallHeight + 0.1f, fZ));
			xFlameEmitter.SetEmitDirection(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

			Zenith_LightComponent& xLight = xWall.AddComponent<Zenith_LightComponent>();
			xLight.SetLightType(LIGHT_TYPE_SPOT);
			xLight.SetColor(Zenith_Maths::Vector3(1.0f, 0.5f, 0.1f));
			xLight.SetIntensity(1000.0f);
			xLight.SetRange(s_fArenaRadius * 3.0f);
			xLight.SetSpotInnerAngle(glm::radians(25.0f));
			xLight.SetSpotOuterAngle(glm::radians(45.0f));

			Zenith_Maths::Vector3 xWallPos(fX, s_fArenaWallHeight * 0.5f, fZ);
			Zenith_Maths::Vector3 xFloorCenter(0.0f, 0.0f, 0.0f);
			Zenith_Maths::Vector3 xTargetDir = glm::normalize(xFloorCenter - xWallPos);
			xLight.SetWorldDirection(xTargetDir);

			m_xLevelEntities.m_axArenaWallEntityIDs.push_back(xWall.GetEntityID());
		}

		// Create player
		Zenith_Entity xPlayer = Combat::Resources().m_xPlayerPrefab.GetDirect()->Instantiate(pxSceneData, "Player");

		Zenith_TransformComponent& xPlayerTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		xPlayerTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xPlayerTransform.SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

		Zenith_ModelComponent& xPlayerModel = xPlayer.AddComponent<Zenith_ModelComponent>();

		// Retry model asset creation if it wasn't available at init time
		// (unit tests create stick figure assets after InitializeCombatResources)
		Combat::TryInitializeStickFigureModel();

		bool bUsingModelInstance = false;
		if (!Combat::Resources().m_strStickFigureModelPath.empty())
		{
			xPlayerModel.LoadModel(Combat::Resources().m_strStickFigureModelPath);
			if (xPlayerModel.GetModelInstance() && xPlayerModel.HasSkeleton())
			{
				xPlayerModel.GetModelInstance()->SetMaterial(0, m_xPlayerMaterial.GetDirect());
				bUsingModelInstance = true;
			}
		}
		if (!bUsingModelInstance)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] Player model instance fallback to static mesh (modelPath empty=%s, hasModelInst=%s, hasSkeleton=%s)",
				Combat::Resources().m_strStickFigureModelPath.empty() ? "yes" : "no",
				xPlayerModel.HasModel() ? "yes" : "no",
				xPlayerModel.HasModel() && xPlayerModel.HasSkeleton() ? "yes" : "no");
			xPlayerModel.AddMeshEntry(*m_pxStickFigureGeometry, *m_xPlayerMaterial.GetDirect());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Combat] Player using model instance with skeleton");
		}

		// Add AnimatorComponent for skeletal animation (auto-discovers skeleton from ModelComponent)
		xPlayer.AddComponent<Zenith_AnimatorComponent>();

		Zenith_ColliderComponent& xPlayerCollider = xPlayer.AddComponent<Zenith_ColliderComponent>();
		xPlayerCollider.AddCapsuleCollider(0.3f, 0.6f, RIGIDBODY_TYPE_DYNAMIC);
		g_xEngine.Physics().LockRotation(xPlayerCollider.GetBodyID(), true, false, true);

		m_xLevelEntities.m_uPlayerEntityID = xPlayer.GetEntityID();
		Combat_DamageSystem::RegisterEntity(xPlayer.GetEntityID(), 100.0f, 0.2f);

		// Attach the per-entity player script. Its OnAwake registers with this GameManager
		// (Combat_Behaviour::RegisterPlayer), and OnUpdate ticks the player's controller /
		// animation / IK / hit detection - the GameManager no longer drives those.
		xPlayer.AddComponent<Zenith_ScriptComponent>().AddScript<Combat_PlayerBehaviour>();

		// Create hit spark emitter in arena scene
		Zenith_Entity xHitSparkEmitter(pxSceneData, "HitSparkEmitter");
		Zenith_ParticleEmitterComponent& xEmitter = xHitSparkEmitter.AddComponent<Zenith_ParticleEmitterComponent>();
		xEmitter.SetConfig(Combat::Resources().m_pxHitSparkConfig);
		Combat::Resources().m_uHitSparkEmitterID = xHitSparkEmitter.GetEntityID();
	}

	void ClearEntityReferences()
	{
		m_xLevelEntities = Combat_LevelEntities();
		Combat::Resources().m_uHitSparkEmitterID = INVALID_ENTITY_ID;
	}

	// (InitializePlayerAnimation removed - Combat_PlayerBehaviour::OnStart now owns
	// initialising the player's animation controller from its own AnimatorComponent.)

	void SpawnEnemies()
	{
		static constexpr float s_fSpawnRadius = 10.0f;

		std::uniform_real_distribution<float> xAngleDist(0.0f, 6.28318f);
		std::uniform_real_distribution<float> xRadiusDist(5.0f, s_fSpawnRadius);

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		for (uint32_t i = 0; i < m_uTotalEnemies; i++)
		{
			float fAngle = xAngleDist(m_xRng);
			float fRadius = xRadiusDist(m_xRng);
			float fX = cos(fAngle) * fRadius;
			float fZ = sin(fAngle) * fRadius;

			// Cycle through the three enemy variants (Weak / Normal / Strong).
			// Each is a prefab variant of g_pxEnemyPrefab with a Transform.Scale
			// override (0.7 / 0.9 / 1.1 respectively) — no explicit SetScale here
			// because the variant's override applies it during Instantiate.
			const u_int uVariantIdx = i % Combat::uENEMY_VARIANT_COUNT;
			Zenith_Prefab* pxVariant = Combat::Resources().m_axEnemyVariants[uVariantIdx].GetDirect();

			char szName[32];
			snprintf(szName, sizeof(szName), "Enemy_%u_%s", i, Combat::g_aszEnemyVariantNames[uVariantIdx]);

			Zenith_Entity xEnemy = pxVariant->Instantiate(pxSceneData, szName);

			Zenith_TransformComponent& xTransform = xEnemy.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(Zenith_Maths::Vector3(fX, 1.0f, fZ));

			Zenith_ModelComponent& xModel = xEnemy.AddComponent<Zenith_ModelComponent>();

			bool bUsingEnemyModel = false;
			if (!Combat::Resources().m_strStickFigureModelPath.empty())
			{
				xModel.LoadModel(Combat::Resources().m_strStickFigureModelPath);
				if (xModel.GetModelInstance() && xModel.HasSkeleton())
				{
					xModel.GetModelInstance()->SetMaterial(0, m_xEnemyMaterial.GetDirect());
					bUsingEnemyModel = true;
				}
			}
			if (!bUsingEnemyModel)
			{
				xModel.AddMeshEntry(*m_pxStickFigureGeometry, *m_xEnemyMaterial.GetDirect());
			}

			// Add AnimatorComponent for skeletal animation (auto-discovers skeleton from ModelComponent)
			xEnemy.AddComponent<Zenith_AnimatorComponent>();

			Zenith_ColliderComponent& xCollider = xEnemy.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCapsuleCollider(0.27f, 0.54f, RIGIDBODY_TYPE_DYNAMIC);
			g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, false, true);

			m_xLevelEntities.m_axEnemyEntityIDs.push_back(xEnemy.GetEntityID());

			Combat_DamageSystem::RegisterEntity(xEnemy.GetEntityID(), 50.0f, 0.0f);

			// Attach the per-entity enemy script. Its OnAwake registers with this GameManager
			// (Combat_Behaviour::RegisterEnemy) and OnUpdate ticks one Combat_EnemyAI - the
			// GameManager no longer holds a Combat_EnemyManager.
			Combat_EnemyConfig xConfig;
			xConfig.m_fMoveSpeed = 3.0f;
			xConfig.m_fAttackDamage = 15.0f;
			xConfig.m_fAttackRange = 1.5f;
			xConfig.m_fAttackCooldown = 1.5f;

			Combat_EnemyBehaviour* pxEnemyScript = xEnemy.AddComponent<Zenith_ScriptComponent>().AddScript<Combat_EnemyBehaviour>();
			if (pxEnemyScript)
			{
				pxEnemyScript->SetConfig(xConfig);
			}
		}
	}

	// ========================================================================
	// Player Update
	// ========================================================================

	// (UpdatePlayer / UpdatePlayerAttack removed - Combat_PlayerBehaviour::OnUpdate
	//  now owns these per-frame responsibilities for the player entity directly.)

	// ========================================================================
	// Event Handlers
	// ========================================================================

	void ProcessDeferredEvents()
	{
		for (const Combat_DamageEvent& xEvent : s_axDeferredDamageEvents)
		{
			OnDamageEvent(xEvent);
		}
		s_axDeferredDamageEvents.clear();

		for (const Combat_DeathEvent& xEvent : s_axDeferredDeathEvents)
		{
			OnDeathEvent(xEvent);
		}
		s_axDeferredDeathEvents.clear();
	}

	void OnDamageEvent(const Combat_DamageEvent& xEvent)
	{
		// Per-entity scripts (Combat_PlayerBehaviour / Combat_EnemyBehaviour) subscribe
		// to Combat_DamageEvent themselves and handle their own hit-stun. The GameManager
		// only owns the world-effect side of damage: spawning hit particles.
		SpawnHitParticles(xEvent);
	}

	void SpawnHitParticles(const Combat_DamageEvent& xEvent)
	{
		if (!m_xArenaScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		Zenith_Maths::Vector3 xHitPos = xEvent.m_xHitPoint;
		if (glm::length(xHitPos) < 0.001f && pxSceneData->EntityExists(xEvent.m_uTargetEntityID))
		{
			Zenith_Entity xTarget = pxSceneData->GetEntity(xEvent.m_uTargetEntityID);
			if (xTarget.HasComponent<Zenith_TransformComponent>())
			{
				xTarget.GetComponent<Zenith_TransformComponent>().GetPosition(xHitPos);
				xHitPos.y += 1.0f;
			}
		}

		Zenith_Maths::Vector3 xHitDir = xEvent.m_xHitDirection;
		if (glm::length(xHitDir) < 0.001f)
		{
			xHitDir = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		}

		if (Combat::Resources().m_uHitSparkEmitterID != INVALID_ENTITY_ID &&
			pxSceneData->EntityExists(Combat::Resources().m_uHitSparkEmitterID))
		{
			Zenith_Entity xEmitterEntity = pxSceneData->GetEntity(Combat::Resources().m_uHitSparkEmitterID);
			if (xEmitterEntity.HasComponent<Zenith_ParticleEmitterComponent>())
			{
				Zenith_ParticleEmitterComponent& xEmitter = xEmitterEntity.GetComponent<Zenith_ParticleEmitterComponent>();
				xEmitter.SetEmitPosition(xHitPos);
				xEmitter.SetEmitDirection(xHitDir);

				uint32_t uCount = static_cast<uint32_t>(10 + xEvent.m_fDamage * 0.5f);
				xEmitter.Emit(uCount);
			}
		}
	}

	void OnDeathEvent(const Combat_DeathEvent& xEvent)
	{
		if (xEvent.m_uEntityID == s_uPlayerEntityID)
		{
			SetGameState(Combat_GameState::GAME_OVER);
		}
		else if (m_xArenaScene.IsValid())
		{
			// Timed destruction for dead enemies (corpse auto-cleanup after 3s).
			// The Combat_EnemyBehaviour's OnDestroy will unregister it from s_axEnemyEntityIDs.
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);
			if (pxSceneData && pxSceneData->EntityExists(xEvent.m_uEntityID))
			{
				Zenith_Entity xDeadEntity = pxSceneData->GetEntity(xEvent.m_uEntityID);
				Zenith_SceneManager::Destroy(xDeadEntity, 3.0f);
			}
		}
	}

	// ========================================================================
	// Camera Update
	// ========================================================================

	void UpdateCamera(float fDt)
	{
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		// Get player position from arena scene
		Zenith_Maths::Vector3 xPlayerPos(0.0f);
		if (m_xArenaScene.IsValid() && s_uPlayerEntityID != INVALID_ENTITY_ID)
		{
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);
			if (pxSceneData && pxSceneData->EntityExists(s_uPlayerEntityID))
			{
				Zenith_Entity xPlayer = pxSceneData->GetEntity(s_uPlayerEntityID);
				xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
			}
		}

		static constexpr float s_fCamDistance = 15.0f;
		static constexpr float s_fCamHeight = 12.0f;
		static constexpr float s_fCamPitch = -0.7f;

		Zenith_Maths::Vector3 xCamTarget = xPlayerPos;
		Zenith_Maths::Vector3 xCamPos = xCamTarget + Zenith_Maths::Vector3(0.0f, s_fCamHeight, -s_fCamDistance);

		Zenith_Maths::Vector3 xCurrentPos;
		pxCamera->GetPosition(xCurrentPos);
		xCamPos = glm::mix(xCurrentPos, xCamPos, fDt * 5.0f);

		pxCamera->SetPosition(xCamPos);
		pxCamera->SetPitch(s_fCamPitch);
		pxCamera->SetYaw(0.0f);
	}

	// ========================================================================
	// Game State
	// ========================================================================

	void UpdateComboTimer(float fDt)
	{
		// Combat_PlayerBehaviour pushes combo state via NotifyComboHit after a successful
		// hit; we just tick its timer down here.
		TickComboTimer(fDt);
	}

	// Game state mutator. Static so per-entity scripts (Combat_PlayerBehaviour /
	// Combat_EnemyBehaviour) can read it via IsInPlayingState() / GetGameState() without
	// needing a GameManager instance handle.
	static void SetGameState(Combat_GameState eState)
	{
		s_eGameState = eState;
	}

	uint32_t CountAliveEnemies() const
	{
		uint32_t uAlive = 0;
		for (Zenith_EntityID uID : s_axEnemyEntityIDs)
		{
			if (!Combat_DamageSystem::IsDead(uID))
			{
				++uAlive;
			}
		}
		return uAlive;
	}

	void CheckGameState()
	{
		if (CountAliveEnemies() == 0 && !s_axEnemyEntityIDs.empty())
		{
			SetGameState(Combat_GameState::VICTORY);
		}

		if (s_uPlayerEntityID != INVALID_ENTITY_ID && Combat_DamageSystem::IsDead(s_uPlayerEntityID))
		{
			SetGameState(Combat_GameState::GAME_OVER);
		}
	}

	// ========================================================================
	// UI Update
	// ========================================================================

	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		float fPlayerHealth = Combat_DamageSystem::GetHealth(s_uPlayerEntityID);
		float fPlayerMaxHealth = Combat_DamageSystem::GetMaxHealth(s_uPlayerEntityID);

		Combat_UIManager::UpdateAll(
			xUI,
			fPlayerHealth,
			fPlayerMaxHealth,
			s_uComboCount,
			CountAliveEnemies(),
			m_uTotalEnemies,
			s_eGameState);
	}

	// ========================================================================
	// Animation State Labels (disabled - Zenith_TextComponent removed)
	// ========================================================================

	void UpdateAnimationStateLabels()
	{
	}

	// ========================================================================
	// Health Bar Rendering
	// ========================================================================

	bool WorldToScreen(const Zenith_Maths::Vector3& xWorldPos,
		const Zenith_Maths::Matrix4& xViewMatrix,
		const Zenith_Maths::Matrix4& xProjMatrix,
		Zenith_Maths::Vector2& xScreenPos)
	{
		Zenith_Maths::Vector4 xClipPos = xProjMatrix * xViewMatrix * Zenith_Maths::Vector4(xWorldPos, 1.0f);

		if (xClipPos.w <= 0.0f)
			return false;

		Zenith_Maths::Vector3 xNDC = Zenith_Maths::Vector3(xClipPos) / xClipPos.w;

		int32_t iWidth, iHeight;
		Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
		xScreenPos.x = (xNDC.x * 0.5f + 0.5f) * static_cast<float>(iWidth);
		xScreenPos.y = (xNDC.y * 0.5f + 0.5f) * static_cast<float>(iHeight);

		return true;
	}

	void RenderHealthBarQuad(const Zenith_Maths::Vector2& xScreenPos, float fHealthPercent, uint32_t uBarWidth = 60, uint32_t uBarHeight = 8)
	{
		fHealthPercent = glm::clamp(fHealthPercent, 0.0f, 1.0f);

		uint32_t uX = static_cast<uint32_t>(xScreenPos.x - uBarWidth / 2);
		uint32_t uY = static_cast<uint32_t>(xScreenPos.y);

		Flux_QuadsImpl::Quad xBgQuad;
		xBgQuad.m_xPosition_Size = Zenith_Maths::UVector4(uX, uY, uBarWidth, uBarHeight);
		xBgQuad.m_xColour = Zenith_Maths::Vector4(0.15f, 0.15f, 0.15f, 0.9f);
		xBgQuad.m_uTexture = 0;
		xBgQuad.m_xUVMult_UVAdd = Zenith_Maths::Vector2(0.0f, 0.0f);
		g_xEngine.Quads().UploadQuad(xBgQuad);

		if (fHealthPercent > 0.0f)
		{
			uint32_t uFgWidth = static_cast<uint32_t>(uBarWidth * fHealthPercent);
			if (uFgWidth > 0)
			{
				Zenith_Maths::Vector4 xFgColor;
				if (fHealthPercent > 0.6f)
					xFgColor = Zenith_Maths::Vector4(0.2f, 0.9f, 0.2f, 1.0f);
				else if (fHealthPercent > 0.3f)
					xFgColor = Zenith_Maths::Vector4(0.9f, 0.8f, 0.2f, 1.0f);
				else
					xFgColor = Zenith_Maths::Vector4(0.9f, 0.2f, 0.2f, 1.0f);

				Flux_QuadsImpl::Quad xFgQuad;
				xFgQuad.m_xPosition_Size = Zenith_Maths::UVector4(uX + 1, uY + 1, uFgWidth - 2, uBarHeight - 2);
				xFgQuad.m_xColour = xFgColor;
				xFgQuad.m_uTexture = 0;
				xFgQuad.m_xUVMult_UVAdd = Zenith_Maths::Vector2(0.0f, 0.0f);
				g_xEngine.Quads().UploadQuad(xFgQuad);
			}
		}
	}

	void UpdateHealthBars()
	{
		if (!m_xArenaScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		// Get camera for world-to-screen projection
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (!pxCamera)
			return;

		Zenith_Maths::Matrix4 xViewMatrix, xProjMatrix;
		pxCamera->BuildViewMatrix(xViewMatrix);
		pxCamera->BuildProjectionMatrix(xProjMatrix);

		static constexpr float fBarHeightOffset = 2.3f;

		// Player health bar (uses static registry: Combat_PlayerBehaviour::OnAwake registers itself)
		if (s_uPlayerEntityID != INVALID_ENTITY_ID && pxSceneData->EntityExists(s_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = pxSceneData->GetEntity(s_uPlayerEntityID);
			if (xPlayer.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xWorldPos;
				xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xWorldPos);
				xWorldPos.y += fBarHeightOffset;

				Zenith_Maths::Vector2 xScreenPos;
				if (WorldToScreen(xWorldPos, xViewMatrix, xProjMatrix, xScreenPos))
				{
					float fHealthPercent = Combat_DamageSystem::GetHealthPercent(s_uPlayerEntityID);
					RenderHealthBarQuad(xScreenPos, fHealthPercent, 80, 10);
				}
			}
		}

		// Enemy health bars - iterate the registry (alive enemies registered by their behaviours).
		for (Zenith_EntityID uEnemyID : s_axEnemyEntityIDs)
		{
			if (!pxSceneData->EntityExists(uEnemyID))
				continue;
			if (Combat_DamageSystem::IsDead(uEnemyID))
				continue;

			Zenith_Entity xEnemyEntity = pxSceneData->GetEntity(uEnemyID);
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

	void UpdateEntityOverheadDisplay()
	{
		UpdateAnimationStateLabels();
		UpdateHealthBars();
	}

	// ========================================================================
	// Wall Light Animation
	// ========================================================================

	void UpdateWallLights(float fDt)
	{
		static constexpr float fOSCILLATION_SPEED = 0.75f;
		static constexpr float fMAX_ANGLE_DEGREES = 20.0f;
		static constexpr float fCOLOR_SPEED = 1.0f;

		m_fWallLightTime += fDt;

		if (!m_xArenaScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xArenaScene);

		float fAngleOffset = sinf(m_fWallLightTime * fOSCILLATION_SPEED * 2.0f * 3.14159f) * glm::radians(fMAX_ANGLE_DEGREES);

		float fColorT = (sinf(m_fWallLightTime * fCOLOR_SPEED * 2.0f * 3.14159f) + 1.0f) * 0.5f;
		Zenith_Maths::Vector3 xLightColor(1.0f, fColorT, 0.0f);

		for (Zenith_EntityID uWallID : m_xLevelEntities.m_axArenaWallEntityIDs)
		{
			if (!pxSceneData->EntityExists(uWallID))
				continue;

			Zenith_Entity xWall = pxSceneData->GetEntity(uWallID);
			if (!xWall.HasComponent<Zenith_LightComponent>() ||
				!xWall.HasComponent<Zenith_TransformComponent>())
				continue;

			Zenith_LightComponent& xLight = xWall.GetComponent<Zenith_LightComponent>();
			Zenith_TransformComponent& xTransform = xWall.GetComponent<Zenith_TransformComponent>();

			Zenith_Maths::Vector3 xWallPos;
			xTransform.GetPosition(xWallPos);

			Zenith_Maths::Vector3 xFloorCenter(0.0f, 0.0f, 0.0f);
			Zenith_Maths::Vector3 xBaseDir = glm::normalize(xFloorCenter - xWallPos);

			float fCos = cosf(fAngleOffset);
			float fSin = sinf(fAngleOffset);

			Zenith_Maths::Vector3 xRotatedDir;
			xRotatedDir.x = xBaseDir.x * fCos - xBaseDir.z * fSin;
			xRotatedDir.y = xBaseDir.y;
			xRotatedDir.z = xBaseDir.x * fSin + xBaseDir.z * fCos;

			xLight.SetWorldDirection(xRotatedDir);
			xLight.SetColor(xLightColor);
		}
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	uint32_t m_uTotalEnemies;
	float m_fWallLightTime = 0.0f;
	int32_t m_iFocusIndex;

	std::mt19937 m_xRng;

	// Scene handle for the arena
	Zenith_Scene m_xArenaScene;

	// Level entities (in arena scene). The player ID and enemy IDs in here are
	// mirrored by the static registry below (s_uPlayerEntityID / s_axEnemyEntityIDs)
	// which is the canonical source of truth - this struct is kept for the wall-light
	// list and the spawner's bookkeeping.
	Combat_LevelEntities m_xLevelEntities;

	// Static event handles
	static inline Zenith_EventHandle s_uDamageEventHandle = INVALID_EVENT_HANDLE;
	static inline Zenith_EventHandle s_uDeathEventHandle = INVALID_EVENT_HANDLE;

	// Static event queues for deferred processing
	static inline std::vector<Combat_DamageEvent> s_axDeferredDamageEvents;
	static inline std::vector<Combat_DeathEvent> s_axDeferredDeathEvents;

public:
	// ========================================================================
	// Static GameManager state, shared with per-entity scripts
	// ========================================================================
	//
	// Per-entity scripts (Combat_PlayerBehaviour, Combat_EnemyBehaviour) call into these
	// statics from their OnAwake / OnUpdate / OnDestroy hooks to register themselves and
	// to gate work on the game being in PLAYING. Combat_Behaviour writes the state from
	// its lifecycle methods (StartGame, ResetGame, ReturnToMenu, OnUpdate's pause/resume).
	//
	// Static rather than per-instance because (a) there's only ever one Combat_Behaviour
	// in a scene by design, and (b) the per-entity scripts need to read the state without
	// holding a pointer to the GameManager instance.

	static inline Zenith_EntityID s_uPlayerEntityID = INVALID_ENTITY_ID;
	static inline std::vector<Zenith_EntityID> s_axEnemyEntityIDs;
	static inline Combat_GameState s_eGameState = Combat_GameState::MAIN_MENU;
	static inline uint32_t s_uComboCount = 0;
	static inline float    s_fComboTimer = 0.0f;

	static void RegisterPlayer(Zenith_EntityID uEntityID) { s_uPlayerEntityID = uEntityID; }
	static void UnregisterPlayer(Zenith_EntityID uEntityID)
	{
		if (s_uPlayerEntityID == uEntityID)
		{
			s_uPlayerEntityID = INVALID_ENTITY_ID;
		}
	}
	static Zenith_EntityID GetPlayerEntityID() { return s_uPlayerEntityID; }

	static void RegisterEnemy(Zenith_EntityID uEntityID) { s_axEnemyEntityIDs.push_back(uEntityID); }
	static void UnregisterEnemy(Zenith_EntityID uEntityID)
	{
		for (size_t i = 0; i < s_axEnemyEntityIDs.size(); ++i)
		{
			if (s_axEnemyEntityIDs[i] == uEntityID)
			{
				s_axEnemyEntityIDs.erase(s_axEnemyEntityIDs.begin() + i);
				return;
			}
		}
	}
	static const std::vector<Zenith_EntityID>& GetEnemyEntityIDs() { return s_axEnemyEntityIDs; }

	static bool IsInPlayingState() { return s_eGameState == Combat_GameState::PLAYING; }
	static Combat_GameState GetGameState() { return s_eGameState; }

	static void NotifyComboHit(uint32_t uComboCount, float fTimer = 2.0f)
	{
		s_uComboCount = uComboCount;
		s_fComboTimer = fTimer;
	}
	static uint32_t GetComboCount() { return s_uComboCount; }
	static float    GetComboTimer() { return s_fComboTimer; }
	static void     TickComboTimer(float fDt)
	{
		if (s_fComboTimer > 0.0f)
		{
			s_fComboTimer -= fDt;
			if (s_fComboTimer <= 0.0f)
			{
				s_uComboCount = 0;
			}
		}
	}

	// Resource pointers (set in OnAwake from globals)
	Flux_MeshGeometry* m_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxStickFigureGeometry = nullptr;
	MaterialHandle m_xPlayerMaterial;
	MaterialHandle m_xEnemyMaterial;
	MaterialHandle m_xArenaMaterial;
	MaterialHandle m_xWallMaterial;
};
