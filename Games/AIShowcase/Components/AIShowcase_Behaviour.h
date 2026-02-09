#pragma once
/**
 * AIShowcase_Behaviour.h - Main AI demonstration coordinator
 *
 * Demonstrates all AI system features:
 * - NavMesh navigation and pathfinding
 * - Behavior tree decision-making
 * - Perception system (sight, hearing, damage awareness)
 * - Squad tactics and formations
 * - Tactical point system (cover, flanking)
 * - Debug visualization
 * - Multi-scene architecture (persistent GameManager + arena scene)
 *
 * Key lifecycle hooks:
 * - OnAwake()  - Called at RUNTIME creation
 * - OnStart()  - Called before first OnUpdate
 * - OnUpdate() - Called every frame
 * - RenderPropertiesPanel() - Editor UI (tools build)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "UI/Zenith_UIButton.h"

// AI System includes
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "AI/BehaviorTree/Zenith_BTActions.h"
#include "AI/BehaviorTree/Zenith_BTConditions.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_Formation.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// AIShowcase Resources - Global access
// Defined in AIShowcase.cpp
// ============================================================================
namespace AIShowcase
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern Flux_MeshGeometry* g_pxCylinderGeometry;

	extern MaterialHandle g_xFloorMaterial;
	extern MaterialHandle g_xWallMaterial;
	extern MaterialHandle g_xObstacleMaterial;
	extern MaterialHandle g_xPlayerMaterial;
	extern MaterialHandle g_xEnemyMaterial;
	extern MaterialHandle g_xLeaderMaterial;
	extern MaterialHandle g_xFlankerMaterial;
	extern MaterialHandle g_xCoverPointMaterial;
	extern MaterialHandle g_xPatrolPointMaterial;

	extern Zenith_NavMesh* g_pxArenaNavMesh;
}

// ============================================================================
// Game State
// ============================================================================
enum class AIShowcaseGameState : uint8_t
{
	MAIN_MENU,
	PLAYING,
	PAUSED
};

// ============================================================================
// Configuration
// ============================================================================
static constexpr float s_fArenaWidth = 40.0f;
static constexpr float s_fArenaHeight = 30.0f;
static constexpr float s_fWallHeight = 3.0f;
static constexpr float s_fObstacleHeight = 2.0f;
static constexpr uint32_t s_uMaxEnemies = 6;
static constexpr uint32_t s_uEnemiesPerSquad = 3;
static constexpr float s_fPlayerMoveSpeed = 8.0f;

// ============================================================================
// Main Behavior Class
// ============================================================================
class AIShowcase_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(AIShowcase_Behaviour)

	AIShowcase_Behaviour() = delete;
	AIShowcase_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_xPlayerPos(0.0f)
		, m_fPlayerYaw(0.0f)
		, m_uCurrentFormation(0)
		, m_uEnemyCount(0)
		, m_eGameState(AIShowcaseGameState::MAIN_MENU)
		, m_iFocusIndex(0)
	{
	}

	~AIShowcase_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
		// Wire menu button callback
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay)
		{
			pxPlay->SetOnClick(&OnPlayClicked, this);
			pxPlay->SetFocused(true);
		}

		// Start in menu state
		m_eGameState = AIShowcaseGameState::MAIN_MENU;
		SetMenuVisible(true);
		SetHUDVisible(false);
	}

	void OnStart() ZENITH_FINAL override
	{
		// Ensure AI systems are initialized (may have been shut down by unit tests)
		Zenith_PerceptionSystem::Initialise();
		Zenith_SquadManager::Initialise();
		Zenith_TacticalPointSystem::Initialise();
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		switch (m_eGameState)
		{
		case AIShowcaseGameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case AIShowcaseGameState::PLAYING:
			HandlePlayerInput(fDt);
			UpdateAISystems(fDt);
			UpdateUI();

#ifdef ZENITH_TOOLS
			if (Zenith_AIDebugVariables::s_bEnableAllAIDebug)
			{
				DrawDebugVisualization();
			}
#endif
			break;

		case AIShowcaseGameState::PAUSED:
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P))
			{
				m_eGameState = AIShowcaseGameState::PLAYING;
				Zenith_SceneManager::SetScenePaused(m_xArenaScene, false);
			}
			else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				Zenith_SceneManager::SetScenePaused(m_xArenaScene, false);
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
		ImGui::Text("AI Showcase Demo");
		ImGui::Separator();

		const char* astrStates[] = { "Main Menu", "Playing", "Paused" };
		ImGui::Text("State: %s", astrStates[static_cast<int>(m_eGameState)]);

		ImGui::Text("Player Position: (%.1f, %.1f, %.1f)",
			m_xPlayerPos.x, m_xPlayerPos.y, m_xPlayerPos.z);

		ImGui::Separator();
		ImGui::Text("Enemies: %u / %u", m_uEnemyCount, s_uMaxEnemies);

		if (m_pxSquadAlpha)
		{
			ImGui::Text("Squad Alpha: %u members", m_pxSquadAlpha->GetAliveMemberCount());
		}
		if (m_pxSquadBravo)
		{
			ImGui::Text("Squad Bravo: %u members", m_pxSquadBravo->GetAliveMemberCount());
		}

		ImGui::Separator();
		ImGui::Checkbox("Debug Visualization", &Zenith_AIDebugVariables::s_bEnableAllAIDebug);

		if (ImGui::Button("Toggle NavMesh"))
		{
			Zenith_AIDebugVariables::s_bDrawNavMeshEdges = !Zenith_AIDebugVariables::s_bDrawNavMeshEdges;
		}

		if (ImGui::Button("Toggle Perception"))
		{
			Zenith_AIDebugVariables::s_bDrawSightCones = !Zenith_AIDebugVariables::s_bDrawSightCones;
			Zenith_AIDebugVariables::s_bDrawHearingRadius = !Zenith_AIDebugVariables::s_bDrawHearingRadius;
		}

		if (ImGui::Button("Toggle Formations"))
		{
			Zenith_AIDebugVariables::s_bDrawFormationPositions = !Zenith_AIDebugVariables::s_bDrawFormationPositions;
			Zenith_AIDebugVariables::s_bDrawSquadLinks = !Zenith_AIDebugVariables::s_bDrawSquadLinks;
		}

		ImGui::Separator();
		ImGui::Text("Formations:");
		const char* astrFormations[] = { "Line", "Wedge", "Column", "Circle", "Skirmish" };
		for (uint32_t u = 0; u < 5; ++u)
		{
			if (ImGui::RadioButton(astrFormations[u], m_uCurrentFormation == u))
			{
				SetFormation(u);
			}
		}

		if (m_eGameState == AIShowcaseGameState::PLAYING && ImGui::Button("Reset Demo"))
		{
			ResetDemo();
		}
#endif
	}

private:
	// ========================================================================
	// Menu / State Management
	// ========================================================================

	static void OnPlayClicked(void* pxUserData)
	{
		AIShowcase_Behaviour* pxSelf = static_cast<AIShowcase_Behaviour*>(pxUserData);
		pxSelf->StartGame();
	}

	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create arena scene
		m_xArenaScene = Zenith_SceneManager::CreateEmptyScene("Arena");
		Zenith_SceneManager::SetActiveScene(m_xArenaScene);

		// Initialize arena content
		InitializeArena();
		InitializePlayer();
		InitializeEnemySquads();
		GenerateNavMesh();
		SetupTacticalPoints();

		m_eGameState = AIShowcaseGameState::PLAYING;
	}

	void ReturnToMenu()
	{
		CleanupArena();

		SetHUDVisible(false);
		SetMenuVisible(true);

		// Re-focus the play button
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay)
			pxPlay->SetFocused(true);

		m_eGameState = AIShowcaseGameState::MAIN_MENU;
	}

	void ResetDemo()
	{
		CleanupArena();

		// Re-initialize AI systems
		Zenith_PerceptionSystem::Initialise();
		Zenith_SquadManager::Initialise();
		Zenith_TacticalPointSystem::Initialise();

		// Create fresh arena scene
		m_xArenaScene = Zenith_SceneManager::CreateEmptyScene("Arena");
		Zenith_SceneManager::SetActiveScene(m_xArenaScene);

		// Reinitialize everything
		InitializeArena();
		InitializePlayer();
		InitializeEnemySquads();
		GenerateNavMesh();
		SetupTacticalPoints();

		m_xPlayerPos = Zenith_Maths::Vector3(0.0f, 0.5f, 10.0f);
	}

	void CleanupArena()
	{
		// Reset NavMeshAgents before deleting NavMesh
		for (uint32_t u = 0; u < s_uMaxEnemies; ++u)
			m_axNavAgents[u] = Zenith_NavMeshAgent();

		// Shutdown AI systems (clears registered agents, targets, squads, tactical points)
		Zenith_TacticalPointSystem::Shutdown();
		Zenith_SquadManager::Shutdown();
		Zenith_PerceptionSystem::Shutdown();

		// Delete NavMesh
		delete AIShowcase::g_pxArenaNavMesh;
		AIShowcase::g_pxArenaNavMesh = nullptr;

		// Clear member state
		m_xPlayerEntity = Zenith_EntityID();
		m_uEnemyCount = 0;
		m_uObstacleCount = 0;
		m_pxSquadAlpha = nullptr;
		m_pxSquadBravo = nullptr;
		m_fPatrolTimer = 0.0f;
		for (uint32_t u = 0; u < s_uMaxEnemies; ++u)
			m_axEnemyIDs[u] = Zenith_EntityID();
		for (uint32_t u = 0; u < 32; ++u)
			m_axObstacleIDs[u] = Zenith_EntityID();

		// Unload arena scene
		if (m_xArenaScene.IsValid())
		{
			Zenith_SceneManager::UnloadScene(m_xArenaScene);
			m_xArenaScene = Zenith_Scene();
		}
	}

	void SetMenuVisible(bool bVisible)
	{
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxTitle) pxTitle->SetVisible(bVisible);
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		const char* aszElements[] = { "Title", "ControlsHeader", "Control0", "Control1", "Control2", "Control3", "Control4", "Status" };
		for (uint32_t u = 0; u < sizeof(aszElements) / sizeof(aszElements[0]); ++u)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(aszElements[u]);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		// Single button - keep it focused for keyboard activation
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay)
			pxPlay->SetFocused(true);
	}

	// ========================================================================
	// Arena Setup
	// ========================================================================
	void InitializeArena()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		// Create floor
		CreateFloor(pxSceneData);

		// Create walls
		CreateWalls(pxSceneData);

		// Create obstacles for cover
		CreateObstacles(pxSceneData);
	}

	void CreateFloor(Zenith_SceneData* pxSceneData)
	{
		Zenith_Entity xFloor(pxSceneData, "Floor");
		xFloor.SetTransient(false);

		Zenith_TransformComponent& xTransform = xFloor.GetComponent<Zenith_TransformComponent>();
		xTransform.SetScale(Zenith_Maths::Vector3(s_fArenaWidth, 0.1f, s_fArenaHeight));
		xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, -0.05f, 0.0f));

		Zenith_ModelComponent& xModel = xFloor.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*AIShowcase::g_pxCubeGeometry, *AIShowcase::g_xFloorMaterial.Get());

		// Add static collider for NavMesh generation
		Zenith_ColliderComponent& xCollider = xFloor.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
	}

	void CreateWalls(Zenith_SceneData* pxSceneData)
	{
		const float fHalfWidth = s_fArenaWidth * 0.5f;
		const float fHalfHeight = s_fArenaHeight * 0.5f;
		const float fWallThickness = 1.0f;

		// Wall definitions: position, scale
		struct WallDef
		{
			Zenith_Maths::Vector3 m_xPos;
			Zenith_Maths::Vector3 m_xScale;
		};

		WallDef aWalls[] = {
			// North wall
			{ {0.0f, s_fWallHeight * 0.5f, -fHalfHeight}, {s_fArenaWidth, s_fWallHeight, fWallThickness} },
			// South wall
			{ {0.0f, s_fWallHeight * 0.5f, fHalfHeight}, {s_fArenaWidth, s_fWallHeight, fWallThickness} },
			// East wall
			{ {fHalfWidth, s_fWallHeight * 0.5f, 0.0f}, {fWallThickness, s_fWallHeight, s_fArenaHeight} },
			// West wall
			{ {-fHalfWidth, s_fWallHeight * 0.5f, 0.0f}, {fWallThickness, s_fWallHeight, s_fArenaHeight} }
		};

		for (uint32_t u = 0; u < 4; ++u)
		{
			char szName[32];
			sprintf(szName, "Wall_%u", u);
			Zenith_Entity xWall(pxSceneData, szName);
			xWall.SetTransient(false);

			Zenith_TransformComponent& xTransform = xWall.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(aWalls[u].m_xPos);
			xTransform.SetScale(aWalls[u].m_xScale);

			Zenith_ModelComponent& xModel = xWall.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*AIShowcase::g_pxCubeGeometry, *AIShowcase::g_xWallMaterial.Get());

			Zenith_ColliderComponent& xCollider = xWall.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
		}
	}

	void CreateObstacles(Zenith_SceneData* pxSceneData)
	{
		// Place obstacles for tactical gameplay
		struct ObstacleDef
		{
			Zenith_Maths::Vector3 m_xPos;
			Zenith_Maths::Vector3 m_xScale;
		};

		ObstacleDef aObstacles[] = {
			// Center obstacles
			{ {-8.0f, s_fObstacleHeight * 0.5f, -5.0f}, {4.0f, s_fObstacleHeight, 2.0f} },
			{ {8.0f, s_fObstacleHeight * 0.5f, -5.0f}, {4.0f, s_fObstacleHeight, 2.0f} },
			{ {0.0f, s_fObstacleHeight * 0.5f, 5.0f}, {6.0f, s_fObstacleHeight, 2.0f} },
			// Corner obstacles
			{ {-12.0f, s_fObstacleHeight * 0.5f, 8.0f}, {3.0f, s_fObstacleHeight, 3.0f} },
			{ {12.0f, s_fObstacleHeight * 0.5f, 8.0f}, {3.0f, s_fObstacleHeight, 3.0f} },
			{ {-12.0f, s_fObstacleHeight * 0.5f, -8.0f}, {3.0f, s_fObstacleHeight, 3.0f} },
			{ {12.0f, s_fObstacleHeight * 0.5f, -8.0f}, {3.0f, s_fObstacleHeight, 3.0f} },
			// Pillar obstacles
			{ {0.0f, s_fObstacleHeight * 0.5f, -8.0f}, {2.0f, s_fObstacleHeight, 2.0f} },
			{ {-5.0f, s_fObstacleHeight * 0.5f, 0.0f}, {2.0f, s_fObstacleHeight, 2.0f} },
			{ {5.0f, s_fObstacleHeight * 0.5f, 0.0f}, {2.0f, s_fObstacleHeight, 2.0f} },
		};

		for (uint32_t u = 0; u < sizeof(aObstacles) / sizeof(aObstacles[0]); ++u)
		{
			char szName[32];
			sprintf(szName, "Obstacle_%u", u);
			Zenith_Entity xObstacle(pxSceneData, szName);
			xObstacle.SetTransient(false);

			Zenith_TransformComponent& xTransform = xObstacle.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(aObstacles[u].m_xPos);
			xTransform.SetScale(aObstacles[u].m_xScale);

			Zenith_ModelComponent& xModel = xObstacle.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*AIShowcase::g_pxCubeGeometry, *AIShowcase::g_xObstacleMaterial.Get());

			Zenith_ColliderComponent& xCollider = xObstacle.AddComponent<Zenith_ColliderComponent>();
			xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

			m_axObstacleIDs[u] = xObstacle.GetEntityID();
		}
		m_uObstacleCount = sizeof(aObstacles) / sizeof(aObstacles[0]);
	}

	// ========================================================================
	// Player Setup
	// ========================================================================
	void InitializePlayer()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		Zenith_Entity xPlayer(pxSceneData, "Player");
		xPlayer.SetTransient(false);

		m_xPlayerPos = Zenith_Maths::Vector3(0.0f, 0.5f, 10.0f);

		Zenith_TransformComponent& xTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(m_xPlayerPos);
		xTransform.SetScale(Zenith_Maths::Vector3(0.8f, 1.0f, 0.8f));

		Zenith_ModelComponent& xModel = xPlayer.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*AIShowcase::g_pxCylinderGeometry, *AIShowcase::g_xPlayerMaterial.Get());

		Zenith_ColliderComponent& xCollider = xPlayer.AddComponent<Zenith_ColliderComponent>();
		xCollider.AddCapsuleCollider(0.4f, 0.5f, RIGIDBODY_TYPE_DYNAMIC);

		m_xPlayerEntity = xPlayer.GetEntityID();

		// Register player as perception target
		Zenith_PerceptionSystem::RegisterTarget(m_xPlayerEntity);
	}

	// ========================================================================
	// Enemy Squad Setup
	// ========================================================================
	void InitializeEnemySquads()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		// Create Squad Alpha
		m_pxSquadAlpha = Zenith_SquadManager::CreateSquad("Alpha");
		CreateEnemySquad(pxSceneData, m_pxSquadAlpha,
			Zenith_Maths::Vector3(-10.0f, 0.0f, -10.0f));

		// Create Squad Bravo
		m_pxSquadBravo = Zenith_SquadManager::CreateSquad("Bravo");
		CreateEnemySquad(pxSceneData, m_pxSquadBravo,
			Zenith_Maths::Vector3(10.0f, 0.0f, -10.0f));

		// Set initial formation
		m_pxSquadAlpha->SetFormation(Zenith_Formation::GetWedge());
		m_pxSquadBravo->SetFormation(Zenith_Formation::GetLine());
	}

	void CreateEnemySquad(Zenith_SceneData* pxSceneData, Zenith_Squad* pxSquad,
		const Zenith_Maths::Vector3& xBasePos)
	{
		for (uint32_t u = 0; u < s_uEnemiesPerSquad; ++u)
		{
			if (m_uEnemyCount >= s_uMaxEnemies)
				break;

			SquadRole eRole;
			Zenith_MaterialAsset* pxMaterial;
			switch (u)
			{
			case 0:
				eRole = SquadRole::LEADER;
				pxMaterial = AIShowcase::g_xLeaderMaterial.Get();
				break;
			case 1:
				eRole = SquadRole::ASSAULT;
				pxMaterial = AIShowcase::g_xEnemyMaterial.Get();
				break;
			case 2:
				eRole = SquadRole::FLANKER;
				pxMaterial = AIShowcase::g_xFlankerMaterial.Get();
				break;
			default:
				eRole = SquadRole::ASSAULT;
				pxMaterial = AIShowcase::g_xEnemyMaterial.Get();
				break;
			}

			Zenith_EntityID xEnemyID = CreateEnemy(pxSceneData, xBasePos, u, pxMaterial);
			pxSquad->AddMember(xEnemyID, eRole);

			if (u == 0)
			{
				pxSquad->SetLeader(xEnemyID);
			}

			m_axEnemyIDs[m_uEnemyCount++] = xEnemyID;
		}
	}

	Zenith_EntityID CreateEnemy(Zenith_SceneData* pxSceneData, const Zenith_Maths::Vector3& xBasePos,
		uint32_t uIndex, Zenith_MaterialAsset* pxMaterial)
	{
		char szName[32];
		sprintf(szName, "Enemy_%u", m_uEnemyCount);
		Zenith_Entity xEnemy(pxSceneData, szName);
		xEnemy.SetTransient(false);

		// Offset position based on index
		float fOffset = static_cast<float>(uIndex) * 2.0f;
		Zenith_Maths::Vector3 xPos = xBasePos + Zenith_Maths::Vector3(fOffset, 0.5f, 0.0f);

		Zenith_TransformComponent& xTransform = xEnemy.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(Zenith_Maths::Vector3(0.8f, 1.0f, 0.8f));

		Zenith_ModelComponent& xModel = xEnemy.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*AIShowcase::g_pxCylinderGeometry, *pxMaterial);

		// Add AI components
		Zenith_AIAgentComponent& xAI = xEnemy.AddComponent<Zenith_AIAgentComponent>();

		// Assign NavMeshAgent to the AI component (index matches m_uEnemyCount which is incremented after this call)
		xAI.SetNavMeshAgent(&m_axNavAgents[m_uEnemyCount]);

		// Configure perception via PerceptionSystem
		Zenith_SightConfig xSightConfig;
		xSightConfig.m_fMaxRange = 20.0f;
		xSightConfig.m_fFOVAngle = 90.0f;
		xSightConfig.m_bRequireLineOfSight = true;

		// Register with perception system
		Zenith_PerceptionSystem::RegisterAgent(xEnemy.GetEntityID());
		Zenith_PerceptionSystem::SetSightConfig(xEnemy.GetEntityID(), xSightConfig);

		return xEnemy.GetEntityID();
	}

	// ========================================================================
	// NavMesh Generation
	// ========================================================================
	void GenerateNavMesh()
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		NavMeshGenerationConfig xConfig;
		xConfig.m_fAgentRadius = 0.4f;
		xConfig.m_fAgentHeight = 1.8f;
		xConfig.m_fMaxSlope = 45.0f;
		xConfig.m_fMaxStepHeight = 0.3f;
		xConfig.m_fCellSize = 0.3f;

		AIShowcase::g_pxArenaNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(*pxSceneData, xConfig);

		if (AIShowcase::g_pxArenaNavMesh)
		{
			Zenith_Log(LOG_CATEGORY_AI, "AIShowcase: NavMesh generated with %u polygons",
				AIShowcase::g_pxArenaNavMesh->GetPolygonCount());
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_AI, "AIShowcase: NavMesh generation FAILED");
		}

		// Assign NavMesh to all NavMeshAgents
		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			m_axNavAgents[u].SetNavMesh(AIShowcase::g_pxArenaNavMesh);
		}
	}

	// ========================================================================
	// Tactical Points
	// ========================================================================
	void SetupTacticalPoints()
	{
		// Register cover points around obstacles
		for (uint32_t u = 0; u < m_uObstacleCount; ++u)
		{
			Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
			if (!pxSceneData->EntityExists(m_axObstacleIDs[u]))
				continue;

			Zenith_Entity xObstacle = pxSceneData->GetEntity(m_axObstacleIDs[u]);
			Zenith_Maths::Vector3 xPos;
			xObstacle.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			Zenith_Maths::Vector3 xScale;
			xObstacle.GetComponent<Zenith_TransformComponent>().GetScale(xScale);

			// Add cover points on each side of obstacle
			float fOffset = 1.5f;
			Zenith_TacticalPointSystem::RegisterPoint(
				xPos + Zenith_Maths::Vector3(xScale.x * 0.5f + fOffset, 0.0f, 0.0f),
				TacticalPointType::COVER_FULL,
				Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f),  // Face towards obstacle
				m_axObstacleIDs[u]);
			Zenith_TacticalPointSystem::RegisterPoint(
				xPos + Zenith_Maths::Vector3(-xScale.x * 0.5f - fOffset, 0.0f, 0.0f),
				TacticalPointType::COVER_FULL,
				Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),  // Face towards obstacle
				m_axObstacleIDs[u]);
		}

		// Register patrol waypoints (using default facing and no owner)
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(-15.0f, 0.0f, 0.0f),
			TacticalPointType::PATROL_WAYPOINT);
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(15.0f, 0.0f, 0.0f),
			TacticalPointType::PATROL_WAYPOINT);
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(0.0f, 0.0f, -12.0f),
			TacticalPointType::PATROL_WAYPOINT);
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f),
			TacticalPointType::PATROL_WAYPOINT);

		// Register flank positions around the arena (enables Flank Positions visualization)
		// These are positions suitable for attacking from the sides
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(-10.0f, 0.0f, 5.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));  // Face right
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(10.0f, 0.0f, 5.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f)); // Face left
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(-10.0f, 0.0f, -5.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(10.0f, 0.0f, -5.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f));
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(0.0f, 0.0f, 8.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f)); // Face toward center
		Zenith_TacticalPointSystem::RegisterPoint(
			Zenith_Maths::Vector3(0.0f, 0.0f, -10.0f),
			TacticalPointType::FLANK_POSITION,
			Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));  // Face toward center
	}

	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandlePlayerInput(float fDt)
	{
		// Pause
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_P))
		{
			m_eGameState = AIShowcaseGameState::PAUSED;
			Zenith_SceneManager::SetScenePaused(m_xArenaScene, true);
			UpdateUI();
			return;
		}

		// Escape - return to menu
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
		{
			ReturnToMenu();
			return;
		}

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (!pxSceneData->EntityExists(m_xPlayerEntity))
			return;

		// Movement
		Zenith_Maths::Vector3 xMoveDir(0.0f);
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W))
			xMoveDir.z += 1.0f;  // Forward = +Z (away from camera)
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S))
			xMoveDir.z -= 1.0f;  // Backward = -Z (toward camera)
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A))
			xMoveDir.x -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D))
			xMoveDir.x += 1.0f;

		if (Zenith_Maths::LengthSq(xMoveDir) > 0.01f)
		{
			xMoveDir = Zenith_Maths::Normalize(xMoveDir);
			m_xPlayerPos += xMoveDir * s_fPlayerMoveSpeed * fDt;

			// Clamp to arena bounds
			float fMaxX = s_fArenaWidth * 0.5f - 1.0f;
			float fMaxZ = s_fArenaHeight * 0.5f - 1.0f;
			m_xPlayerPos.x = std::max(-fMaxX, std::min(fMaxX, m_xPlayerPos.x));
			m_xPlayerPos.z = std::max(-fMaxZ, std::min(fMaxZ, m_xPlayerPos.z));

			// Update facing direction
			m_fPlayerYaw = atan2f(-xMoveDir.x, -xMoveDir.z);

			// Update entity
			Zenith_Entity xPlayer = pxSceneData->GetEntity(m_xPlayerEntity);
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(m_xPlayerPos);
		}

		// Attack / Make sound
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE))
		{
			// Emit sound stimulus for hearing test
			Zenith_PerceptionSystem::EmitSoundStimulus(
				m_xPlayerPos, 1.0f, 15.0f, m_xPlayerEntity);
		}

		// Formation switching (1-5 keys)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_1))
			SetFormation(0);
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_2))
			SetFormation(1);
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_3))
			SetFormation(2);
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_4))
			SetFormation(3);
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_5))
			SetFormation(4);

		// Reset
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			ResetDemo();
			return;
		}
	}

	void SetFormation(uint32_t uFormation)
	{
		m_uCurrentFormation = uFormation;

		const Zenith_Formation* pxFormation = nullptr;
		switch (uFormation)
		{
		case 0: pxFormation = Zenith_Formation::GetLine(); break;
		case 1: pxFormation = Zenith_Formation::GetWedge(); break;
		case 2: pxFormation = Zenith_Formation::GetColumn(); break;
		case 3: pxFormation = Zenith_Formation::GetCircle(); break;
		case 4: pxFormation = Zenith_Formation::GetSkirmish(); break;
		default: pxFormation = Zenith_Formation::GetLine(); break;
		}

		if (m_pxSquadAlpha)
			m_pxSquadAlpha->SetFormation(pxFormation);
		if (m_pxSquadBravo)
			m_pxSquadBravo->SetFormation(pxFormation);
	}

	// ========================================================================
	// AI System Updates
	// ========================================================================
	void UpdateAISystems(float fDt)
	{
		// Update perception for all agents
		Zenith_PerceptionSystem::Update(fDt);

		// Update squads
		Zenith_SquadManager::Update(fDt);

		// Update tactical points
		Zenith_TacticalPointSystem::Update(fDt);

		// Update individual AI agents
		UpdateEnemyAI(fDt);
	}

	void UpdateEnemyAI(float fDt)
	{
		Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_AGENT_UPDATE);

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		// Phase 1: Determine destinations for all agents (sets m_bPathPending = true)
		// This must happen BEFORE batch pathfinding so that pending requests exist
		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			if (!pxSceneData->EntityExists(m_axEnemyIDs[u]))
				continue;

			Zenith_Entity xEnemy = pxSceneData->GetEntity(m_axEnemyIDs[u]);
			Zenith_Maths::Vector3 xPos;
			xEnemy.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);

			// Set start position for batch pathfinding
			m_axNavAgents[u].SetStartPosition(xPos);

			// Check perception for player
			const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
				Zenith_PerceptionSystem::GetPerceivedTargets(m_axEnemyIDs[u]);

			bool bSeesPlayer = false;
			if (pxTargets && pxTargets->GetSize() > 0)
			{
				// If we see the player, move towards them
				for (uint32_t t = 0; t < pxTargets->GetSize(); ++t)
				{
					if (pxTargets->Get(t).m_xEntityID == m_xPlayerEntity)
					{
						bSeesPlayer = true;
						const Zenith_PerceivedTarget& xTarget = pxTargets->Get(t);

						// Share target info with squad (enables Shared Targets visualization)
						Zenith_Squad* pxSquad = Zenith_SquadManager::GetSquadForEntity(m_axEnemyIDs[u]);
						if (pxSquad)
						{
							pxSquad->ShareTargetInfo(m_xPlayerEntity, xTarget.m_xLastKnownPosition, m_axEnemyIDs[u]);
						}

						// Set destination to player position (sets m_bPathPending = true)
						m_axNavAgents[u].SetDestination(xTarget.m_xLastKnownPosition);
						break;
					}
				}
			}

			// If no target, patrol between waypoints
			if (!bSeesPlayer)
			{
				Zenith_NavMeshAgent& xNav = m_axNavAgents[u];
				if (!xNav.HasPath() || xNav.HasReachedDestination())
				{
					// Pick a patrol waypoint
					static const Zenith_Maths::Vector3 s_axPatrolPoints[] = {
						Zenith_Maths::Vector3(-15.0f, 0.0f, 0.0f),
						Zenith_Maths::Vector3(15.0f, 0.0f, 0.0f),
						Zenith_Maths::Vector3(0.0f, 0.0f, -12.0f),
						Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f),
					};
					uint32_t uPatrolIdx = (u + static_cast<uint32_t>(m_fPatrolTimer * 0.5f)) % 4;
					xNav.SetDestination(s_axPatrolPoints[uPatrolIdx]);
				}
			}
		}

		// Phase 2: Process batch pathfinding for all agents that need paths
		// Now m_bPathPending is true for agents that called SetDestination above
		ProcessBatchPathfinding(pxSceneData);

		// Phase 3: Update AI components and agent movement (paths already computed)
		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			if (!pxSceneData->EntityExists(m_axEnemyIDs[u]))
				continue;

			Zenith_Entity xEnemy = pxSceneData->GetEntity(m_axEnemyIDs[u]);
			if (!xEnemy.HasComponent<Zenith_AIAgentComponent>())
				continue;

			Zenith_AIAgentComponent& xAI = xEnemy.GetComponent<Zenith_AIAgentComponent>();
			xAI.OnUpdate(fDt);
		}

		m_fPatrolTimer += fDt;
	}

	void ProcessBatchPathfinding(Zenith_SceneData* pxSceneData)
	{
		(void)pxSceneData;  // Start positions already set in Phase 1

		// Collect path requests from all agents that need paths
		Zenith_Pathfinding::PathRequest axRequests[s_uMaxEnemies];
		uint32_t auAgentIndices[s_uMaxEnemies];  // Map request index to agent index
		uint32_t uNumRequests = 0;

		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			Zenith_NavMeshAgent& xNav = m_axNavAgents[u];

			if (!xNav.NeedsPath())
				continue;

			// Start position was already set in Phase 1, just build the request
			Zenith_Maths::Vector3 xStart, xEnd;
			if (xNav.GetPendingPathRequest(xStart, xEnd))
			{
				axRequests[uNumRequests].m_pxNavMesh = AIShowcase::g_pxArenaNavMesh;
				axRequests[uNumRequests].m_xStart = xStart;
				axRequests[uNumRequests].m_xEnd = xEnd;
				auAgentIndices[uNumRequests] = u;
				++uNumRequests;
			}
		}

		// Process all path requests in parallel
		if (uNumRequests > 0)
		{
			Zenith_Pathfinding::FindPathsBatch(axRequests, uNumRequests);

			// Apply results back to agents
			for (uint32_t r = 0; r < uNumRequests; ++r)
			{
				uint32_t uAgentIdx = auAgentIndices[r];
				m_axNavAgents[uAgentIdx].SetPathResult(axRequests[r].m_xResult);
			}
		}
	}

	// ========================================================================
	// UI Updates
	// ========================================================================
	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Update status text
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			uint32_t uSquadCount = 0;
			if (m_pxSquadAlpha && m_pxSquadAlpha->GetAliveMemberCount() > 0)
				uSquadCount++;
			if (m_pxSquadBravo && m_pxSquadBravo->GetAliveMemberCount() > 0)
				uSquadCount++;

			char szStatus[64];
			sprintf(szStatus, "Enemies: %u | Squads: %u", m_uEnemyCount, uSquadCount);
			pxStatus->SetText(szStatus);
		}
	}

	// ========================================================================
	// Debug Visualization
	// ========================================================================
	void DrawDebugVisualization()
	{
#ifdef ZENITH_TOOLS
		Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_DEBUG_DRAW);

		if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
			return;

		// NavMesh visualization (checks its own flags internally)
		if (AIShowcase::g_pxArenaNavMesh)
		{
			AIShowcase::g_pxArenaNavMesh->DebugDraw();
		}

		// Perception visualization for each enemy (sight cones, hearing, detection lines, memory)
		DrawPerceptionVisualization();

		// Agent path visualization
		DrawAgentPathVisualization();

		// Squad formation visualization (checks its own flags internally)
		if (m_pxSquadAlpha)
			m_pxSquadAlpha->DebugDraw();
		if (m_pxSquadBravo)
			m_pxSquadBravo->DebugDraw();

		// Tactical points (checks its own flags internally)
		Zenith_TacticalPointSystem::DebugDraw();

		// Draw player indicator
		Flux_Primitives::AddCircle(m_xPlayerPos, 1.5f,
			Zenith_Maths::Vector3(0.2f, 0.6f, 1.0f));
#endif
	}

	void DrawPerceptionVisualization()
	{
#ifdef ZENITH_TOOLS
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		static constexpr float s_fHearingRange = 15.0f;  // Match EmitSoundStimulus radius

		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			if (!pxSceneData->EntityExists(m_axEnemyIDs[u]))
				continue;

			Zenith_Entity xEnemy = pxSceneData->GetEntity(m_axEnemyIDs[u]);
			Zenith_Maths::Vector3 xPos;
			xEnemy.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			Zenith_Maths::Vector3 xEyePos = xPos + Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

			// Draw sight cone outline (controlled by s_bDrawSightCones)
			if (Zenith_AIDebugVariables::s_bDrawSightCones)
			{
				Zenith_Maths::Quaternion xRot;
				xEnemy.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);
				Zenith_Maths::Vector3 xForward = Zenith_Maths::RotateVector(
					Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f), xRot);

				// Note: Actual FOV is 90 degrees (45 half-angle), but we draw a narrower
				// cone (25 degrees) for better visual clarity - full FOV would be too wide
				Flux_Primitives::AddConeOutline(
					xEyePos,
					xForward,
					25.0f,  // Visualization half-angle (narrower than actual FOV for clarity)
					20.0f,  // Range
					Zenith_Maths::Vector3(1.0f, 0.8f, 0.2f));  // Yellow
			}

			// Draw hearing radius (controlled by s_bDrawHearingRadius)
			if (Zenith_AIDebugVariables::s_bDrawHearingRadius)
			{
				Flux_Primitives::AddCircle(xPos, s_fHearingRange,
					Zenith_Maths::Vector3(0.3f, 0.3f, 0.8f));  // Blue
			}

			// Draw detection lines and memory positions
			const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
				Zenith_PerceptionSystem::GetPerceivedTargets(m_axEnemyIDs[u]);

			if (pxTargets)
			{
				for (uint32_t t = 0; t < pxTargets->GetSize(); ++t)
				{
					const Zenith_PerceivedTarget& xTarget = pxTargets->Get(t);

					// Draw detection line (controlled by s_bDrawDetectionLines)
					if (Zenith_AIDebugVariables::s_bDrawDetectionLines)
					{
						// Color based on awareness (green = low, red = high)
						float fAwareness = xTarget.m_fAwareness;
						Zenith_Maths::Vector3 xColor(fAwareness, 1.0f - fAwareness, 0.0f);

						Flux_Primitives::AddLine(xEyePos, xTarget.m_xLastKnownPosition, xColor);
					}

					// Draw memory position marker (controlled by s_bDrawMemoryPositions)
					if (Zenith_AIDebugVariables::s_bDrawMemoryPositions && !xTarget.m_bCurrentlyVisible)
					{
						// Draw remembered position with fading based on time since seen
						float fFade = std::max(0.2f, 1.0f - xTarget.m_fTimeSinceLastSeen * 0.1f);
						Zenith_Maths::Vector3 xMemoryColor(1.0f, 0.5f, 0.0f);  // Orange
						xMemoryColor = xMemoryColor * fFade;

						Flux_Primitives::AddSphere(xTarget.m_xLastKnownPosition, 0.3f, xMemoryColor);
						// Question mark indicator for "lost" target
						Flux_Primitives::AddLine(
							xTarget.m_xLastKnownPosition + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f),
							xTarget.m_xLastKnownPosition + Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
							xMemoryColor);
					}
				}
			}
		}
#endif
	}

	void DrawAgentPathVisualization()
	{
#ifdef ZENITH_TOOLS
		if (!Zenith_AIDebugVariables::s_bDrawAgentPaths && !Zenith_AIDebugVariables::s_bDrawPathWaypoints)
			return;

		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

		for (uint32_t u = 0; u < m_uEnemyCount; ++u)
		{
			if (!pxSceneData->EntityExists(m_axEnemyIDs[u]))
				continue;

			Zenith_Entity xEnemy = pxSceneData->GetEntity(m_axEnemyIDs[u]);
			Zenith_Maths::Vector3 xPos;
			xEnemy.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			m_axNavAgents[u].DebugDraw(xPos);
		}
#endif
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Game state
	AIShowcaseGameState m_eGameState;
	Zenith_Scene m_xArenaScene;
	int32_t m_iFocusIndex;

	// Player
	Zenith_EntityID m_xPlayerEntity;
	Zenith_Maths::Vector3 m_xPlayerPos;
	float m_fPlayerYaw;

	// Enemies
	Zenith_EntityID m_axEnemyIDs[s_uMaxEnemies];
	uint32_t m_uEnemyCount;

	// Obstacles
	Zenith_EntityID m_axObstacleIDs[32];
	uint32_t m_uObstacleCount = 0;

	// NavMesh agents (one per enemy for pathfinding)
	Zenith_NavMeshAgent m_axNavAgents[s_uMaxEnemies];

	// Squads
	Zenith_Squad* m_pxSquadAlpha = nullptr;
	Zenith_Squad* m_pxSquadBravo = nullptr;

	// State
	uint32_t m_uCurrentFormation;
	float m_fPatrolTimer = 0.0f;
};
