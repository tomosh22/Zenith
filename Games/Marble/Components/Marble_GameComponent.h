#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Marble_GameComponent.h - Marble's systems coordinator (W1 graph conversion)
 *
 * This component is the SYSTEMS half of the game. All gameplay DECISIONS -
 * the state machine (menu/playing/paused/won/lost), the timer countdown, the
 * score/win/loss flow, the pause/reset/escape key handling, and the menu
 * focus logic - live in two boot-authored behaviour graphs:
 *
 *   Marble_LevelFlow.bgraph - on the gameplay GameManager. OWNS the game
 *       state: blackboard vars gameState (int, MarbleGameState values),
 *       timeRemaining (float), score (int), collected (int). Chains: stage
 *       frame results -> timer -> collection -> fall (the old same-frame
 *       decision order), P/Esc SwitchOnInt key dispatch, R reset, and a
 *       reactive StateMachine whose MarbleEnter_Paused / MarbleExit_Paused
 *       transition events drive the level-scene pause.
 *   Marble_GameFlow.bgraph  - on the menu GameManager. OnUIButtonClicked
 *       (MenuPlay) -> LoadSceneByIndex(1); W/S/Up/Down focus-toggle chains +
 *       per-frame MarbleApplyMenuFocus. gameState defaults to MAIN_MENU.
 *
 * C++ (this file + the modules) keeps: physics input, camera follow, level
 * generation, collectible detection/animation, fall query, HUD text writes -
 * and READS the graph state through the accessors below ("state moves to the
 * graph blackboard, shim accessor reads it").
 *
 * Modules:
 * - Marble_Input.h             - Camera-relative input handling
 * - Marble_PhysicsController.h - Physics-based ball movement
 * - Marble_CameraFollow.h      - Smooth camera following
 * - Marble_LevelGenerator.h    - Procedural level generation
 * - Marble_CollectibleSystem.h - Pickup detection and scoring
 * - Marble_UIManager.h         - HUD management
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
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
#include "imgui.h"
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
 * Marble_GameComponent - systems coordinator
 *
 * Architecture:
 * - GameManager entity (camera + UI + game component + graph) per scene
 * - Level scene created/destroyed on transitions via CreateEmptyScene/UnloadScene
 *
 * State machine (graph-owned): MAIN_MENU -> PLAYING -> PAUSED / WON / LOST -> MAIN_MENU
 */
class Marble_GameComponent
{
public:
	Marble_GameComponent() = delete;
	Marble_GameComponent(Zenith_Entity& xParentEntity)
		: m_xRng(std::random_device{}())
		, m_xParentEntity(xParentEntity)
	{
	}
	~Marble_GameComponent() = default;

	// ========================================================================
	// Graph-state accessors - the probe surface. State lives on the attached
	// graph's blackboard (Marble_LevelFlow / Marble_GameFlow declare the
	// vars); C++ (HUD, camera gating, characterization tests) READS it here.
	// ========================================================================
	MarbleGameState GetGameState()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		if (pxBlackboard == nullptr)
		{
			return MarbleGameState::MAIN_MENU;	// pre-resolve fallback
		}
		int32_t iState = pxBlackboard->GetInt32("gameState", 0);
		iState = iState < 0 ? 0 : (iState > 4 ? 4 : iState);
		return static_cast<MarbleGameState>(iState);
	}
	float GetTimeRemaining()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		return pxBlackboard ? pxBlackboard->GetFloat("timeRemaining", s_fInitialTime) : s_fInitialTime;
	}
	uint32_t GetScore()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		const int32_t iScore = pxBlackboard ? pxBlackboard->GetInt32("score", 0) : 0;
		return iScore < 0 ? 0u : static_cast<uint32_t>(iScore);
	}
	uint32_t GetCollectedCount()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		const int32_t iCollected = pxBlackboard ? pxBlackboard->GetInt32("collected", 0) : 0;
		return iCollected < 0 ? 0u : static_cast<uint32_t>(iCollected);
	}

	// ========================================================================
	// Graph-facing systems surface (the node seams)
	// ========================================================================
	const Marble_CollectibleSystem::CollectionResult& GetLastCollection() const { return m_xLastCollection; }
	bool HasBallFallen() const { return m_bBallFellThisFrame; }

	// Level (re)creation: unload any existing level scene, create a fresh one,
	// run the generator. State/timer/score resets are GRAPH-side (the R chain).
	void RegenerateLevel()
	{
		ClearEntityReferences();
		if (m_xLevelScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}

		m_xLevelScene = g_xEngine.Scenes().LoadScene("Level", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xLevelScene);

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
	}

	// Level teardown (the escape-to-menu systems body; the menu-scene load is
	// an engine LoadSceneByIndex node at the end of the graph chain).
	void UnloadLevel()
	{
		ClearEntityReferences();
		if (m_xLevelScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xLevelScene);
			m_xLevelScene = Zenith_Scene();
		}
	}

	// Pause/resume the LEVEL scene only - the GameManager's scene keeps
	// updating so the graphs still see the resume key.
	void SetLevelPaused(bool bPaused)
	{
		if (m_xLevelScene.IsValid())
		{
			g_xEngine.Scenes().SetScenePaused(m_xLevelScene, bPaused);
		}
	}

	// Characterization-test seam: the next ComputeCollectibles consumes this
	// injected result INSTEAD of computing one (deterministic collection/win
	// probing without driving the ball). Conversion-neutral: the decision
	// consumers read m_xLastCollection identically before and after the wave.
	void Test_InjectCollection(uint32_t uCount, uint32_t uScore, bool bAllCollected)
	{
		m_xInjectedCollection.uCollectedCount = uCount;
		m_xInjectedCollection.uScoreGained = uScore;
		m_xInjectedCollection.bAllCollected = bAllCollected;
		m_bHasInjectedCollection = true;
	}

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

		// The menu scene's GameManager owns the Play/Quit canvas (its clicks
		// are the graph's OnUIButtonClicked source - no C++ wiring). The
		// gameplay scene's GameManager has no menu: generate the level
		// directly; its state comes from Marble_LevelFlow's declared
		// gameState default (PLAYING).
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			bHasMenu = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay") != nullptr;
		}
		if (!bHasMenu)
		{
			RegenerateLevel();
		}
	}

	// Per-state SYSTEMS dispatch - reads the graph-owned state, runs the
	// systems passes, and fires "LevelTick" (dt payload) into the graph. All
	// transitions (keys, timer, win/loss, menu) are graph chains.
	void OnUpdate(const float fDt)
	{
		switch (GetGameState())
		{
		case MarbleGameState::MAIN_MENU:
			// Menu focus/click handling is fully graph + UI-system side.
			break;

		case MarbleGameState::PLAYING:
			HandleInput(fDt);
			ComputeCollectibles(fDt);
			ComputeFallState();
			// Decisions (timer -> collection -> fall, the old same-frame
			// order) live in Marble_LevelFlow; dt rides the payload.
			FireLevelTick(fDt);
			UpdateCamera(fDt);
			UpdateUI();
			break;

		case MarbleGameState::PAUSED:
			UpdateUI();
			break;

		case MarbleGameState::WON:
		case MarbleGameState::LOST:
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
		const MarbleGameState eGameState = GetGameState();
		ImGui::Text("State: %s", szStates[static_cast<int>(eGameState)]);

		if (eGameState != MarbleGameState::MAIN_MENU)
		{
			ImGui::Text("Score: %u", GetScore());
			ImGui::Text("Time: %.1f", GetTimeRemaining());
			ImGui::Text("Collected: %u / %u", GetCollectedCount(),
				static_cast<uint32_t>(m_xLevelEntities.axCollectibleEntityIDs.size()) + GetCollectedCount());
		}

		ImGui::TextWrapped("Flow decisions live in Marble_LevelFlow / Marble_GameFlow; "
			"state is on the graph blackboard (gameState/timeRemaining/score/collected).");
	}
#endif

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload kept byte-identical to the pre-conversion block.
		// The RUNTIME timer now lives on the graph blackboard; this field is
		// the authored initial value only.
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fSerializedTimeRemaining;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_fSerializedTimeRemaining;
		}
	}

private:
	// The attached graph's blackboard (first slot): Marble_LevelFlow on the
	// gameplay GameManager, Marble_GameFlow on the menu GameManager.
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

	void ClearEntityReferences()
	{
		m_xLevelEntities.uBallEntityID = INVALID_ENTITY_ID;
		m_xLevelEntities.uGoalEntityID = INVALID_ENTITY_ID;
		m_xLevelEntities.axPlatformEntityIDs.clear();
		m_xLevelEntities.axCollectibleEntityIDs.clear();
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
		Zenith_ColliderComponent* pxCollider = xBall.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr)
			return;

		Zenith_ColliderComponent& xCollider = *pxCollider;

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

	// Systems query only - the fell -> LOST decision is a Branch in the graph
	// (MarbleStageFrameResults publishes this as the "ballFell" var).
	void ComputeFallState()
	{
		m_bBallFellThisFrame = false;
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		m_bBallFellThisFrame = Marble_PhysicsController::HasFallenOff(xBallPos);
	}

	void FireLevelTick(float fDt)
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		Zenith_PropertyValue xDt;
		xDt.SetFloat(fDt);
		pxGraph->FireCustomEvent("LevelTick", &xDt);
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
	// Systems pass only - detection + the destroy + the spin animation. The
	// score/collected accumulation and the all-collected -> WON decision are
	// engine nodes in the graph, fed by MarbleStageFrameResults.
	void ComputeCollectibles(float fDt)
	{
		m_xLastCollection = Marble_CollectibleSystem::CollectionResult();
		if (m_bHasInjectedCollection)
		{
			// Test seam (see Test_InjectCollection): consume the injected
			// result verbatim this frame.
			m_xLastCollection = m_xInjectedCollection;
			m_bHasInjectedCollection = false;
			return;
		}
		if (!m_xLevelScene.IsValid())
			return;
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(m_xLevelScene);
		if (!m_xLevelEntities.uBallEntityID.IsValid() || !pxSceneData->EntityExists(m_xLevelEntities.uBallEntityID))
			return;

		Zenith_Entity xBall = pxSceneData->GetEntity(m_xLevelEntities.uBallEntityID);
		Zenith_Maths::Vector3 xBallPos;
		xBall.GetComponent<Zenith_TransformComponent>().GetPosition(xBallPos);

		// Check for collections (uses GetActiveScene internally - still works)
		m_xLastCollection =
			Marble_CollectibleSystem::CheckCollectibles(xBallPos, m_xLevelEntities.axCollectibleEntityIDs, GetCollectedCount());

		// Animate collectibles (uses GetActiveScene internally - still works)
		Marble_CollectibleSystem::UpdateCollectibleRotation(m_xLevelEntities.axCollectibleEntityIDs, fDt);
	}

	// ========================================================================
	// UI (delegates to Marble_UIManager) - HUD text is a systems WRITE that
	// READS the graph-owned state through the accessors.
	// ========================================================================
	void UpdateUI()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		const uint32_t uCollected = GetCollectedCount();
		const uint32_t uTotalCollectibles = static_cast<uint32_t>(m_xLevelEntities.axCollectibleEntityIDs.size()) + uCollected;

		Marble_UIManager::UpdateUI(
			xUI,
			GetScore(),
			GetTimeRemaining(),
			uCollected,
			uTotalCollectibles,
			GetGameState());
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Level entities (managed by Marble_LevelGenerator)
	Marble_LevelGenerator::LevelEntities m_xLevelEntities;

	// Scene handle for the level scene
	Zenith_Scene m_xLevelScene;

	// Per-frame systems results consumed by the Marble_LevelFlow graph nodes.
	Marble_CollectibleSystem::CollectionResult m_xLastCollection;
	bool m_bBallFellThisFrame = false;

	// Characterization-test injection seam (Test_InjectCollection).
	Marble_CollectibleSystem::CollectionResult m_xInjectedCollection;
	bool m_bHasInjectedCollection = false;

	// Authored initial-timer value: serialization ballast only (the runtime
	// timer is the graph blackboard's "timeRemaining").
	float m_fSerializedTimeRemaining = s_fInitialTime;

	// Random number generator
	std::mt19937 m_xRng;

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
