#pragma once
/**
 * Survival_Behaviour.h - Main game coordinator
 *
 * This is the central behavior that orchestrates all survival game systems:
 *
 * Engine Features Demonstrated:
 * - Zenith_ScriptBehaviour lifecycle (OnAwake, OnStart, OnUpdate)
 * - Zenith_Task / Zenith_TaskArray for background processing
 * - Zenith_EventDispatcher for custom game events
 * - Zenith_Query for multi-component entity queries
 * - Custom components and systems
 *
 * Module Dependencies:
 * - Survival_PlayerController.h  - Movement and interaction input
 * - Survival_ResourceNode.h      - Harvestable resources
 * - Survival_Inventory.h         - Item storage
 * - Survival_CraftingSystem.h    - Recipe processing
 * - Survival_TaskProcessor.h     - Background task management
 * - Survival_WorldQuery.h        - Entity queries
 * - Survival_EventBus.h          - Game events
 * - Survival_UIManager.h         - HUD updates
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Prefab/Zenith_Prefab.h"

// Include all game modules
#include "Survival_Config.h"
#include "Survival_EventBus.h"
#include "Survival_Inventory.h"
#include "Survival_ResourceNode.h"
#include "Survival_PlayerController.h"
#include "Survival_WorldQuery.h"
#include "Survival_CraftingSystem.h"
#include "Survival_TaskProcessor.h"
#include "Survival_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// Survival Resources - Global access
// Defined in Survival.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
class Zenith_Prefab;

namespace Survival
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern Flux_MeshGeometry* g_pxCapsuleGeometry;

	extern Flux_MaterialAsset* g_pxPlayerMaterial;
	extern Flux_MaterialAsset* g_pxGroundMaterial;
	extern Flux_MaterialAsset* g_pxTreeMaterial;
	extern Flux_MaterialAsset* g_pxRockMaterial;
	extern Flux_MaterialAsset* g_pxBerryMaterial;
	extern Flux_MaterialAsset* g_pxWoodMaterial;
	extern Flux_MaterialAsset* g_pxStoneMaterial;

	extern Zenith_Prefab* g_pxPlayerPrefab;
	extern Zenith_Prefab* g_pxTreePrefab;
	extern Zenith_Prefab* g_pxRockPrefab;
	extern Zenith_Prefab* g_pxBerryBushPrefab;
	extern Zenith_Prefab* g_pxDroppedItemPrefab;
}

// ============================================================================
// Game Configuration
// ============================================================================
static constexpr float s_fPlayerHeight = 1.6f;
static constexpr float s_fDefaultMoveSpeed = 8.0f;
static constexpr float s_fDefaultInteractionRange = 3.0f;
static constexpr float s_fStatusMessageDuration = 2.0f;

// ============================================================================
// Main Behavior Class
// ============================================================================
class Survival_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Survival_Behaviour)

	Survival_Behaviour() = delete;
	Survival_Behaviour(Zenith_Entity& xParentEntity)
		: m_uPlayerEntityID(INVALID_ENTITY_ID)
		, m_uGroundEntityID(INVALID_ENTITY_ID)
		, m_fStatusMessageTimer(0.f)
		, m_fMoveSpeed(s_fDefaultMoveSpeed)
		, m_fInteractionRange(s_fDefaultInteractionRange)
		, m_fCameraDistance(15.f)
		, m_fCameraHeight(10.f)
		, m_fCameraSmoothSpeed(5.f)
		, m_fAxeBonus(2.0f)
		, m_fPickaxeBonus(2.0f)
		, m_xRng(std::random_device{}())
	{
	}

	~Survival_Behaviour()
	{
		// Shutdown task processor
		Survival_TaskProcessor::Shutdown();

		// Unsubscribe from events
		for (Zenith_EventHandle uHandle : m_axEventHandles)
		{
			Survival_EventBus::Unsubscribe(uHandle);
		}
	}

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	/**
	 * OnAwake - Called when behavior is attached at RUNTIME
	 */
	void OnAwake() ZENITH_FINAL override
	{
		// Initialize task processor with resource manager
		Survival_TaskProcessor::Initialize(&m_xResourceManager);

		// Subscribe to events
		SubscribeToEvents();

		// Find pre-created entities and populate resource manager
		FindSceneEntities();
		PopulateResourceManagerFromScene();

		// Reset inventory and crafting for fresh start
		m_xInventory.Reset();
		m_xCrafting.CancelCrafting();
	}

	/**
	 * OnStart - Called before first OnUpdate
	 */
	void OnStart() ZENITH_FINAL override
	{
		// Ensure entities are found if loaded from scene
		if (m_uPlayerEntityID == INVALID_ENTITY_ID)
		{
			FindSceneEntities();
			PopulateResourceManagerFromScene();
		}
	}

	/**
	 * OnUpdate - Main game loop
	 */
	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Process deferred events from background tasks
		Survival_EventBus::ProcessDeferredEvents();

		// Handle reset
		if (Survival_PlayerController::WasResetPressed())
		{
			ResetGame();
			return;
		}

		// Handle input
		HandleMovement(fDt);
		HandleInteraction();
		HandleCrafting();

		// Update game systems
		UpdateCrafting(fDt);
		UpdateResourceNodes(fDt);

		// Update camera
		Survival_CameraController::UpdateCamera(
			m_uPlayerEntityID,
			m_fCameraDistance,
			m_fCameraHeight,
			m_fCameraSmoothSpeed,
			fDt);

		// Update visuals
		m_xResourceManager.UpdateNodeVisuals();

		// Update status message timer
		if (m_fStatusMessageTimer > 0.f)
		{
			m_fStatusMessageTimer -= fDt;
			if (m_fStatusMessageTimer <= 0.f)
			{
				ClearStatusMessage();
			}
		}

		// Update UI
		UpdateUI();
	}

	/**
	 * RenderPropertiesPanel - Editor UI (tools build only)
	 */
	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("Survival Game");
		ImGui::Separator();

		// Inventory display
		ImGui::Text("Inventory:");
		ImGui::Text("  Wood: %u", m_xInventory.GetWood());
		ImGui::Text("  Stone: %u", m_xInventory.GetStone());
		ImGui::Text("  Berries: %u", m_xInventory.GetBerries());
		ImGui::Text("  Axes: %u", m_xInventory.GetAxeCount());
		ImGui::Text("  Pickaxes: %u", m_xInventory.GetPickaxeCount());

		ImGui::Separator();

		// Resource stats
		ImGui::Text("World Stats:");
		ImGui::Text("  Total Nodes: %u", m_xResourceManager.GetCount());
		ImGui::Text("  Active: %u", m_xResourceManager.GetActiveCount());
		ImGui::Text("  Depleted: %u", m_xResourceManager.GetDepletedCount());

		// Query demo
		ImGui::Text("  Renderable Entities: %u", Survival_WorldQuery::CountRenderableEntities());

		ImGui::Separator();

		// Crafting state
		if (m_xCrafting.IsCrafting())
		{
			ImGui::Text("Crafting: %s (%.0f%%)",
				GetItemName(m_xCrafting.GetCurrentCrafting()),
				m_xCrafting.GetProgress() * 100.f);
		}
		else
		{
			ImGui::Text("Crafting: Idle");
		}

		ImGui::Separator();

		// Settings
		if (ImGui::CollapsingHeader("Settings"))
		{
			ImGui::DragFloat("Move Speed", &m_fMoveSpeed, 0.5f, 1.f, 20.f);
			ImGui::DragFloat("Interaction Range", &m_fInteractionRange, 0.5f, 1.f, 10.f);
			ImGui::DragFloat("Camera Distance", &m_fCameraDistance, 0.5f, 5.f, 30.f);
			ImGui::DragFloat("Camera Height", &m_fCameraHeight, 0.5f, 3.f, 20.f);
		}

		ImGui::Separator();

		// Actions
		if (ImGui::Button("Reset Game"))
		{
			ResetGame();
		}

		if (ImGui::Button("Give Resources"))
		{
			m_xInventory.AddItem(ITEM_TYPE_WOOD, 10);
			m_xInventory.AddItem(ITEM_TYPE_STONE, 10);
		}
#endif
	}

	// ========================================================================
	// Serialization
	// ========================================================================

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fMoveSpeed;
		xStream << m_fInteractionRange;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_fMoveSpeed;
			xStream >> m_fInteractionRange;
		}
	}

private:
	// ========================================================================
	// Event Handling
	// ========================================================================

	void SubscribeToEvents()
	{
		// Subscribe to resource harvested event
		m_axEventHandles.push_back(
			Survival_EventBus::SubscribeLambda<Survival_Event_ResourceHarvested>(
				[this](const Survival_Event_ResourceHarvested& xEvent)
				{
					OnResourceHarvested(xEvent);
				}));

		// Subscribe to resource respawned event
		m_axEventHandles.push_back(
			Survival_EventBus::SubscribeLambda<Survival_Event_ResourceRespawned>(
				[this](const Survival_Event_ResourceRespawned& xEvent)
				{
					OnResourceRespawned(xEvent);
				}));

		// Subscribe to crafting complete event
		m_axEventHandles.push_back(
			Survival_EventBus::SubscribeLambda<Survival_Event_CraftingComplete>(
				[this](const Survival_Event_CraftingComplete& xEvent)
				{
					OnCraftingComplete(xEvent);
				}));
	}

	void OnResourceHarvested(const Survival_Event_ResourceHarvested& xEvent)
	{
		// Add items to inventory
		m_xInventory.AddItem(xEvent.m_eItemType, xEvent.m_uAmount);

		// Show feedback
		ShowStatusMessage(xEvent.m_eItemType, xEvent.m_uAmount);
	}

	void OnResourceRespawned(const Survival_Event_ResourceRespawned& xEvent)
	{
		// Could show visual feedback, play sound, etc.
	}

	void OnCraftingComplete(const Survival_Event_CraftingComplete& xEvent)
	{
		if (xEvent.m_bSuccess)
		{
			// Collect the crafted item
			m_xCrafting.CollectCraftedItem(m_xInventory);

			// Show message
			if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
			{
				Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
				Survival_UIManager::ShowCraftingComplete(xUI, xEvent.m_eItemType);
				m_fStatusMessageTimer = s_fStatusMessageDuration;
			}
		}
	}

	// ========================================================================
	// Input Handling
	// ========================================================================

	void HandleMovement(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!xScene.EntityExists(m_uPlayerEntityID))
			return;

		Zenith_Maths::Vector3 xCamPos = Survival_CameraController::GetCameraPosition();
		Zenith_Maths::Vector3 xPlayerPos = Survival_PlayerController::GetPlayerPosition(m_uPlayerEntityID);

		Zenith_Maths::Vector3 xDirection = Survival_PlayerController::GetMovementDirection(xCamPos, xPlayerPos);

		Survival_PlayerController::ApplyMovement(m_uPlayerEntityID, xDirection, m_fMoveSpeed, fDt);
	}

	void HandleInteraction()
	{
		if (!Survival_PlayerController::WasInteractPressed())
			return;

		// Find nearest resource
		Zenith_Maths::Vector3 xPlayerPos = Survival_PlayerController::GetPlayerPosition(m_uPlayerEntityID);
		Survival_WorldQuery::QueryResult xNearest =
			Survival_WorldQuery::FindNearestResourceInRange(xPlayerPos, m_fInteractionRange, m_xResourceManager);

		if (xNearest.m_uNodeIndex == static_cast<uint32_t>(-1))
			return;

		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(xNearest.m_uNodeIndex);
		if (!pNode || pNode->m_bDepleted)
			return;

		// Calculate bonus from tools
		float fBonus = 1.0f;
		if (pNode->m_eResourceType == RESOURCE_TYPE_TREE && m_xInventory.HasAxe())
		{
			fBonus = m_fAxeBonus;
		}
		else if (pNode->m_eResourceType == RESOURCE_TYPE_ROCK && m_xInventory.HasPickaxe())
		{
			fBonus = m_fPickaxeBonus;
		}

		// Hit the node (fires events internally)
		pNode->Hit(fBonus);
	}

	void HandleCrafting()
	{
		if (m_xCrafting.IsCrafting())
			return;

		// Check for crafting input
		if (Survival_PlayerController::WasCraftAxePressed())
		{
			if (m_xCrafting.CanCraft(ITEM_TYPE_AXE, m_xInventory))
			{
				m_xCrafting.StartCrafting(ITEM_TYPE_AXE, m_xInventory);
			}
			else
			{
				ShowNotEnoughMaterials();
			}
		}
		else if (Survival_PlayerController::WasCraftPickaxePressed())
		{
			if (m_xCrafting.CanCraft(ITEM_TYPE_PICKAXE, m_xInventory))
			{
				m_xCrafting.StartCrafting(ITEM_TYPE_PICKAXE, m_xInventory);
			}
			else
			{
				ShowNotEnoughMaterials();
			}
		}
	}

	// ========================================================================
	// Game Systems Update
	// ========================================================================

	void UpdateCrafting(float fDt)
	{
		SurvivalItemType eCompleted = m_xCrafting.Update(fDt);
		// Completion handled by event
	}

	void UpdateResourceNodes(float fDt)
	{
		// Use task system for background node updates
		// Submit parallel update task
		uint32_t uNodeCount = m_xResourceManager.GetCount();
		if (uNodeCount > 0)
		{
			// For demonstration, use TaskArray for parallel processing
			Survival_TaskProcessor::SubmitParallelNodeUpdate(fDt, uNodeCount);

			// Wait for completion (in production, might let it run async)
			Survival_TaskProcessor::WaitForParallelNodeUpdate();
		}
	}

	// ========================================================================
	// UI Management
	// ========================================================================

	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Find nearest resource for interaction prompt
		Zenith_Maths::Vector3 xPlayerPos = Survival_PlayerController::GetPlayerPosition(m_uPlayerEntityID);
		Survival_WorldQuery::QueryResult xNearest =
			Survival_WorldQuery::FindNearestResourceInRange(xPlayerPos, m_fInteractionRange, m_xResourceManager);

		Survival_ResourceNodeData* pNearestNode = nullptr;
		bool bCanInteract = false;
		if (xNearest.m_uNodeIndex != static_cast<uint32_t>(-1))
		{
			pNearestNode = m_xResourceManager.GetNode(xNearest.m_uNodeIndex);
			bCanInteract = (pNearestNode && !pNearestNode->m_bDepleted);
		}

		Survival_UIManager::UpdateAllUI(xUI, m_xInventory, m_xCrafting, pNearestNode, bCanInteract || (pNearestNode != nullptr));
	}

	void ShowStatusMessage(SurvivalItemType eItemType, uint32_t uAmount)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Survival_UIManager::ShowHarvestFeedback(xUI, eItemType, uAmount);
		m_fStatusMessageTimer = s_fStatusMessageDuration;
	}

	void ShowNotEnoughMaterials()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Survival_UIManager::ShowNotEnoughMaterials(xUI);
		m_fStatusMessageTimer = s_fStatusMessageDuration;
	}

	void ClearStatusMessage()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Survival_UIManager::ClearStatusMessage(xUI);
	}

	// ========================================================================
	// Entity Lookup (find pre-created entities from Project_LoadInitialScene)
	// ========================================================================

	void FindSceneEntities()
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Find player entity
		Zenith_Entity xPlayer = xScene.FindEntityByName("Player");
		Zenith_Assert(xPlayer.IsValid(), "Player entity not found in scene - ensure scene was saved after Project_LoadInitialScene created entities");
		m_uPlayerEntityID = xPlayer.GetEntityID();

		// Find ground entity
		Zenith_Entity xGround = xScene.FindEntityByName("Ground");
		Zenith_Assert(xGround.IsValid(), "Ground entity not found in scene");
		m_uGroundEntityID = xGround.GetEntityID();
	}

	void PopulateResourceManagerFromScene()
	{
		static constexpr uint32_t s_uTreeCount = 15;
		static constexpr uint32_t s_uRockCount = 10;
		static constexpr uint32_t s_uBerryCount = 8;

		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Find and register all tree entities
		for (uint32_t i = 0; i < s_uTreeCount; i++)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "Tree_%u", i);
			Zenith_Entity xTree = xScene.FindEntityByName(szName);
			if (xTree.IsValid())
			{
				Zenith_TransformComponent& xTransform = xTree.GetComponent<Zenith_TransformComponent>();
				Zenith_Maths::Vector3 xPos;
				xTransform.GetPosition(xPos);
				Zenith_Maths::Vector3 xScale;
				xTransform.GetScale(xScale);

				Survival_ResourceNodeData xNode;
				xNode.m_uEntityID = xTree.GetEntityID();
				xNode.m_eResourceType = RESOURCE_TYPE_TREE;
				xNode.m_eYieldType = ITEM_TYPE_WOOD;
				xNode.m_uMaxHits = 3;
				xNode.m_uCurrentHits = 3;
				xNode.m_uYieldAmount = 3;
				xNode.m_fRespawnDuration = 30.f;
				xNode.m_xPosition = Zenith_Maths::Vector3(xPos.x, 0.f, xPos.z);
				xNode.m_xOriginalScale = xScale;

				m_xResourceManager.AddNode(xNode);
			}
		}

		// Find and register all rock entities
		for (uint32_t i = 0; i < s_uRockCount; i++)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "Rock_%u", i);
			Zenith_Entity xRock = xScene.FindEntityByName(szName);
			if (xRock.IsValid())
			{
				Zenith_TransformComponent& xTransform = xRock.GetComponent<Zenith_TransformComponent>();
				Zenith_Maths::Vector3 xPos;
				xTransform.GetPosition(xPos);
				Zenith_Maths::Vector3 xScale;
				xTransform.GetScale(xScale);

				Survival_ResourceNodeData xNode;
				xNode.m_uEntityID = xRock.GetEntityID();
				xNode.m_eResourceType = RESOURCE_TYPE_ROCK;
				xNode.m_eYieldType = ITEM_TYPE_STONE;
				xNode.m_uMaxHits = 4;
				xNode.m_uCurrentHits = 4;
				xNode.m_uYieldAmount = 2;
				xNode.m_fRespawnDuration = 45.f;
				xNode.m_xPosition = Zenith_Maths::Vector3(xPos.x, 0.f, xPos.z);
				xNode.m_xOriginalScale = xScale;

				m_xResourceManager.AddNode(xNode);
			}
		}

		// Find and register all berry bush entities
		for (uint32_t i = 0; i < s_uBerryCount; i++)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "BerryBush_%u", i);
			Zenith_Entity xBush = xScene.FindEntityByName(szName);
			if (xBush.IsValid())
			{
				Zenith_TransformComponent& xTransform = xBush.GetComponent<Zenith_TransformComponent>();
				Zenith_Maths::Vector3 xPos;
				xTransform.GetPosition(xPos);
				Zenith_Maths::Vector3 xScale;
				xTransform.GetScale(xScale);

				Survival_ResourceNodeData xNode;
				xNode.m_uEntityID = xBush.GetEntityID();
				xNode.m_eResourceType = RESOURCE_TYPE_BERRY_BUSH;
				xNode.m_eYieldType = ITEM_TYPE_BERRIES;
				xNode.m_uMaxHits = 1;
				xNode.m_uCurrentHits = 1;
				xNode.m_uYieldAmount = 5;
				xNode.m_fRespawnDuration = 20.f;
				xNode.m_xPosition = Zenith_Maths::Vector3(xPos.x, 0.f, xPos.z);
				xNode.m_xOriginalScale = xScale;

				m_xResourceManager.AddNode(xNode);
			}
		}
	}

	void ResetGame()
	{
		// Reset player position
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (xScene.EntityExists(m_uPlayerEntityID))
		{
			Zenith_Entity xPlayer = xScene.GetEntity(m_uPlayerEntityID);
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(0.f, s_fPlayerHeight * 0.5f, 0.f));
		}

		// Reset all resource nodes to full health
		m_xResourceManager.ForEach([](Survival_ResourceNodeData& xNode, uint32_t uIndex)
		{
			xNode.m_bDepleted = false;
			xNode.m_uCurrentHits = xNode.m_uMaxHits;
			xNode.m_fRespawnTimer = 0.f;
		});

		// Update visuals to show full health
		m_xResourceManager.UpdateNodeVisuals();

		// Reset inventory and crafting
		m_xInventory.Reset();
		m_xCrafting.CancelCrafting();
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Entity IDs
	Zenith_EntityID m_uPlayerEntityID;
	Zenith_EntityID m_uGroundEntityID;

	// Game systems
	Survival_Inventory m_xInventory;
	Survival_ResourceNodeManager m_xResourceManager;
	Survival_CraftingSystem m_xCrafting;

	// Event subscription handles
	std::vector<Zenith_EventHandle> m_axEventHandles;

	// UI state
	float m_fStatusMessageTimer;

	// Configuration
	float m_fMoveSpeed;
	float m_fInteractionRange;
	float m_fCameraDistance;
	float m_fCameraHeight;
	float m_fCameraSmoothSpeed;
	float m_fAxeBonus;
	float m_fPickaxeBonus;

	// Random
	std::mt19937 m_xRng;
};
