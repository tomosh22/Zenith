#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Survival_GameComponent.h - Main game coordinator
 *
 * This is the central game component that orchestrates all survival game systems:
 *
 * Engine Features Demonstrated:
 * - Game ECS component lifecycle hooks (OnAwake, OnStart, OnUpdate, OnDestroy -
 *   concept-detected by the component-meta registry)
 * - Zenith_Task / Zenith_DataParallelTask for background processing
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
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_Vector.h"
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

#include "UI/Zenith_UIButton.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// Forward declaration for world content creation (defined in Survival.cpp)
extern void Survival_CreateWorldContent(Zenith_SceneData* pxSceneData);

// ============================================================================
// Survival Resources - Phase 8 per-game ProjectResources struct.
// ============================================================================
class Zenith_Prefab;

namespace Survival
{
	struct SurvivalResources
	{
		MeshGeometryHandle  m_xCubeAsset;
		MeshGeometryHandle  m_xSphereAsset;
		MeshGeometryHandle  m_xCapsuleAsset;
		Flux_MeshGeometry*  m_pxCubeGeometry    = nullptr;
		Flux_MeshGeometry*  m_pxSphereGeometry  = nullptr;
		Flux_MeshGeometry*  m_pxCapsuleGeometry = nullptr;

		MaterialHandle      m_xPlayerMaterial;
		MaterialHandle      m_xGroundMaterial;
		MaterialHandle      m_xTreeMaterial;
		MaterialHandle      m_xRockMaterial;
		MaterialHandle      m_xBerryMaterial;
		MaterialHandle      m_xWoodMaterial;
		MaterialHandle      m_xStoneMaterial;

		PrefabHandle        m_xPlayerPrefab;
		PrefabHandle        m_xTreePrefab;
		PrefabHandle        m_xRockPrefab;
		PrefabHandle        m_xBerryBushPrefab;
		PrefabHandle        m_xDroppedItemPrefab;
	};

	SurvivalResources& Resources();
}

// ============================================================================
// Game Configuration
// ============================================================================
static constexpr float s_fPlayerHeight = 1.6f;
static constexpr float s_fDefaultMoveSpeed = 8.0f;
static constexpr float s_fDefaultInteractionRange = 3.0f;
static constexpr float s_fStatusMessageDuration = 2.0f;

// ============================================================================
// Game State
// ============================================================================
enum class SurvivalGameState : uint32_t
{
	MAIN_MENU,
	PLAYING
};

// ============================================================================
// Main Game Component Class
// ============================================================================
class Survival_GameComponent
{
public:
	Survival_GameComponent() = delete;
	Survival_GameComponent(Zenith_Entity& xParentEntity)
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
		, m_eGameState(SurvivalGameState::MAIN_MENU)
		, m_iFocusIndex(0)
		, m_xParentEntity(xParentEntity)
	{
	}

	// Teardown lives in OnDestroy (below), NOT the destructor: component pools
	// move-construct + destruct instances on relocation, so destructor-side
	// teardown would fire on every pool move. The destructor stays trivial.
	~Survival_GameComponent() = default;

	// Component pools relocate components on resize / swap-and-pop / cross-scene
	// transfer (move-construct + destruct the source). The event-bus lambdas and
	// the task processor reach this component through s_pxInstance, so a move
	// must repoint it - hand-written moves, copies deleted.
	Survival_GameComponent(const Survival_GameComponent&) = delete;
	Survival_GameComponent& operator=(const Survival_GameComponent&) = delete;

	Survival_GameComponent(Survival_GameComponent&& xOther) noexcept
		: m_uPlayerEntityID(xOther.m_uPlayerEntityID)
		, m_uGroundEntityID(xOther.m_uGroundEntityID)
		, m_xInventory(std::move(xOther.m_xInventory))
		, m_xResourceManager(std::move(xOther.m_xResourceManager))
		, m_xCrafting(std::move(xOther.m_xCrafting))
		, m_axEventHandles(std::move(xOther.m_axEventHandles))
		, m_fStatusMessageTimer(xOther.m_fStatusMessageTimer)
		, m_fMoveSpeed(xOther.m_fMoveSpeed)
		, m_fInteractionRange(xOther.m_fInteractionRange)
		, m_fCameraDistance(xOther.m_fCameraDistance)
		, m_fCameraHeight(xOther.m_fCameraHeight)
		, m_fCameraSmoothSpeed(xOther.m_fCameraSmoothSpeed)
		, m_fAxeBonus(xOther.m_fAxeBonus)
		, m_fPickaxeBonus(xOther.m_fPickaxeBonus)
		, m_xRng(xOther.m_xRng)
		, m_eGameState(xOther.m_eGameState)
		, m_xWorldScene(xOther.m_xWorldScene)
		, m_iFocusIndex(xOther.m_iFocusIndex)
		, m_xParentEntity(xOther.m_xParentEntity)
	{
		FixUpInstancePointerAfterMove(xOther);
	}

	Survival_GameComponent& operator=(Survival_GameComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_uPlayerEntityID = xOther.m_uPlayerEntityID;
			m_uGroundEntityID = xOther.m_uGroundEntityID;
			m_xInventory = std::move(xOther.m_xInventory);
			m_xResourceManager = std::move(xOther.m_xResourceManager);
			m_xCrafting = std::move(xOther.m_xCrafting);
			m_axEventHandles = std::move(xOther.m_axEventHandles);
			m_fStatusMessageTimer = xOther.m_fStatusMessageTimer;
			m_fMoveSpeed = xOther.m_fMoveSpeed;
			m_fInteractionRange = xOther.m_fInteractionRange;
			m_fCameraDistance = xOther.m_fCameraDistance;
			m_fCameraHeight = xOther.m_fCameraHeight;
			m_fCameraSmoothSpeed = xOther.m_fCameraSmoothSpeed;
			m_fAxeBonus = xOther.m_fAxeBonus;
			m_fPickaxeBonus = xOther.m_fPickaxeBonus;
			m_xRng = xOther.m_xRng;
			m_eGameState = xOther.m_eGameState;
			m_xWorldScene = xOther.m_xWorldScene;
			m_iFocusIndex = xOther.m_iFocusIndex;
			m_xParentEntity = xOther.m_xParentEntity;
			FixUpInstancePointerAfterMove(xOther);
		}
		return *this;
	}

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	/**
	 * OnAwake - Called when the component's entity awakens at RUNTIME
	 */
	void OnAwake()
	{
		// Static instance pointer for the captureless event-bus lambdas (reset
		// every OnAwake so Play->Stop->Play cycles repoint it).
		s_pxInstance = this;

		// Initialize task processor with resource manager
		Survival_TaskProcessor::Initialize(&m_xResourceManager);

		// Subscribe to events
		SubscribeToEvents();

		// Detect the menu scene by its Play button. The click itself is wired by
		// the Survival_GameFlow graph's OnUIButtonClicked("MenuPlay") node (which
		// self-wires the button's onClick when the graph ticks) -> LoadSceneByIndex,
		// so no C++ SetOnClick is needed.
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Zenith_UI::Zenith_UIButton* pxPlay = static_cast<Zenith_UI::Zenith_UIButton*>(xUI.FindElement("MenuPlay"));
			if (pxPlay)
			{
				pxPlay->SetFocused(true);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			// Start in menu state
			m_eGameState = SurvivalGameState::MAIN_MENU;
			m_iFocusIndex = 0;
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	/**
	 * OnStart - Called before first OnUpdate
	 */
	void OnStart()
	{
		// Nothing to do here - world entities are found in StartGame
	}

	/**
	 * OnDestroy - Called exactly once, right before the component is removed
	 * (entity destruction / scene unload). Holds the teardown that used to live
	 * in the old behaviour's destructor.
	 */
	void OnDestroy()
	{
		// Shutdown task processor
		Survival_TaskProcessor::Shutdown();

		// Unsubscribe from events
		for (u_int u = 0; u < m_axEventHandles.GetSize(); ++u)
		{
			Survival_EventBus::Unsubscribe(m_axEventHandles.Get(u));
		}
		m_axEventHandles.Clear();

		if (s_pxInstance == this)
		{
			s_pxInstance = nullptr;
		}
	}

	/**
	 * OnUpdate - Main game loop
	 */
	void OnUpdate(const float fDt)
	{
		// The menu Play / Escape->menu / R->reset / menu-focus input DECISIONS live
		// in the Survival_GameFlow graph now (order 60, before this systems dispatch
		// at order 100). A transition it makes this frame lands before this switch
		// reads m_eGameState, reproducing the old early-return-skips-systems shape
		// (Escape's ReturnToMenu pre-clears world state so the rest of this tick is a
		// no-op; R's ResetGame runs the systems on the reset state one frame earlier -
		// the AIShowcase/Combat-precedented, unobservable divergence). This switch is
		// now systems-only for both states.
		switch (m_eGameState)
		{
		case SurvivalGameState::MAIN_MENU:
			break;

		case SurvivalGameState::PLAYING:
		{
			// Process deferred events from background tasks
			Survival_EventBus::ProcessDeferredEvents();

			// Handle input
			HandleMovement(fDt);
			// Harvest (E) + craft-start (1/2) DECISIONS live in the
			// Survival_PlayerActions graph now; fire its driving event at exactly
			// the point the old HandleInteraction/HandleCrafting calls sat, so the
			// order relative to movement and the crafting tick is byte-identical.
			FireGraphEvent("SurvivalPlayerTick", fDt);

			// Update game systems
			// Per-tick craft progression lives in the Survival_CraftTick graph;
			// fire its driving event (dt on the payload) where UpdateCrafting ran.
			FireGraphEvent("SurvivalCraftTick", fDt);
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
			break;
		}
		}
	}

#ifdef ZENITH_TOOLS
	/**
	 * RenderPropertiesPanel - Editor UI (tools build only)
	 */
	void RenderPropertiesPanel()
	{
		ImGui::Text("Survival Game");
		ImGui::Separator();

		// State display
		const char* aszStateNames[] = { "Main Menu", "Playing" };
		ImGui::Text("State: %s", aszStateNames[static_cast<uint32_t>(m_eGameState)]);

		if (m_eGameState == SurvivalGameState::MAIN_MENU)
		{
			if (ImGui::Button("Start Game"))
			{
				StartGame();
			}
		}
		else
		{
			if (ImGui::Button("Return to Menu"))
			{
				ReturnToMenu();
			}
		}

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

		if (m_eGameState == SurvivalGameState::PLAYING)
		{
			ImGui::Text("  Renderable Entities: %u", Survival_WorldQuery::CountRenderableEntities());
		}

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
		if (m_eGameState == SurvivalGameState::PLAYING)
		{
			if (ImGui::Button("Reset Game"))
			{
				ResetGame();
			}

			if (ImGui::Button("Give Resources"))
			{
				m_xInventory.AddItem(ITEM_TYPE_WOOD, 10);
				m_xInventory.AddItem(ITEM_TYPE_STONE, 10);
			}
		}
	}
#endif

	// ========================================================================
	// Serialization
	// ========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload (byte-identical to the pre-migration parameter block,
		// including its own internal version field).
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_fMoveSpeed;
		xStream << m_fInteractionRange;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_fMoveSpeed;
			xStream >> m_fInteractionRange;
		}
	}

	// ========================================================================
	// Characterization-test surface (read-only observers + test scaffolding).
	//
	// Behaviour-neutral: these expose the private game state the behaviour-graph
	// characterization tests assert against and provide test-only setup pokes
	// (place the player, seed inventory, arm a respawn). Added alongside the
	// wave-2 graph conversion so the tests probe surfaces that survive it
	// (mirrors Combat's Test_/Graph_ accessors). No production code calls them.
	// ========================================================================

	SurvivalGameState GetGameState() const { return m_eGameState; }
	int32_t Graph_GetGameStateInt() const { return static_cast<int32_t>(m_eGameState); }

	// The currently-live component's state, resolved through s_pxInstance. A
	// menu<->game transition swaps the whole scene (and thus the live instance);
	// this reads whichever manager is awake now (MenuManager=MAIN_MENU,
	// GameManager=PLAYING), so a test can watch a transition across the swap.
	static SurvivalGameState Test_GetLiveGameState()
	{
		return s_pxInstance ? s_pxInstance->m_eGameState : SurvivalGameState::MAIN_MENU;
	}

	// The currently-awake SurvivalGame component (the MenuManager's or the
	// GameManager's, depending on the active scene). Tests reach the instance
	// observers/pokes through this.
	static Survival_GameComponent* Test_GetLiveInstance() { return s_pxInstance; }

	uint32_t Test_GetItemCount(SurvivalItemType eType) const { return m_xInventory.GetCount(eType); }
	bool     Test_IsCrafting() const { return m_xCrafting.IsCrafting(); }
	int32_t  Test_GetCraftStateInt() const { return static_cast<int32_t>(m_xCrafting.GetState()); }
	float    Test_GetCraftProgress() const { return m_xCrafting.GetProgress(); }
	SurvivalItemType Test_GetCurrentCrafting() const { return m_xCrafting.GetCurrentCrafting(); }

	uint32_t Test_GetResourceNodeCount() { return m_xResourceManager.GetCount(); }
	uint32_t Test_GetActiveResourceCount() { return m_xResourceManager.GetActiveCount(); }
	uint32_t Test_GetDepletedResourceCount() { return m_xResourceManager.GetDepletedCount(); }

	uint32_t Test_GetResourceNodeHits(uint32_t uIndex)
	{
		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(uIndex);
		return pNode ? pNode->m_uCurrentHits : 0;
	}
	bool Test_IsResourceNodeDepleted(uint32_t uIndex)
	{
		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(uIndex);
		return pNode ? pNode->m_bDepleted : true;
	}
	int32_t Test_GetResourceNodeYieldType(uint32_t uIndex)
	{
		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(uIndex);
		return pNode ? static_cast<int32_t>(pNode->m_eYieldType) : static_cast<int32_t>(ITEM_TYPE_NONE);
	}
	bool Test_GetResourceNodePosition(uint32_t uIndex, Zenith_Maths::Vector3& xOut)
	{
		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(uIndex);
		if (pNode == nullptr) return false;
		xOut = pNode->m_xPosition;
		return true;
	}

	// Place the player at a world position (test scaffolding only - the harvest
	// range gate reads the player's transform, so this puts a chosen resource in
	// range without walking the whole (slow, Debug) navmesh-free path).
	bool Test_SetPlayerPosition(const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(m_uPlayerEntityID);
		if (!xPlayer.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xPlayer.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;
		pxTransform->SetPosition(xPos);
		return true;
	}

	// Seed the inventory directly (test scaffolding - e.g. give crafting mats).
	uint32_t Test_GiveItem(SurvivalItemType eType, uint32_t uAmount) { return m_xInventory.AddItem(eType, uAmount); }

	// Force a node into the depleted/respawning state with a chosen timer, so the
	// background-task respawn seam can be exercised without harvesting to zero
	// and waiting the full 30 s respawn duration.
	void Test_ArmRespawn(uint32_t uIndex, float fTimer)
	{
		Survival_ResourceNodeData* pNode = m_xResourceManager.GetNode(uIndex);
		if (pNode == nullptr) return;
		pNode->m_bDepleted = true;
		pNode->m_uCurrentHits = 0;
		pNode->m_fRespawnTimer = fTimer;
	}

	// ========================================================================
	// Behaviour-graph shims (wave-2 conversion).
	//
	// The DECISION bodies stay here verbatim; the behaviour-graph nodes
	// (Survival_GraphNodes.h) resolve the self component and call these
	// synchronously. Systems (movement, camera, visuals, HUD, respawn task,
	// scene management) stay in OnUpdate. m_eGameState remains the per-instance
	// source of truth (GetGameState unchanged) - SurvivalGetGameState only
	// mirrors it to the blackboard to gate the P/R/Escape chains.
	// ========================================================================

	// Survival_GameFlow.bgraph.
	void Graph_ReturnToMenu() { ReturnToMenu(); }
	void Graph_ResetGame() { ResetGame(); }
	void Graph_FocusPlayButton() { UpdateMenuInput(); }

	// Survival_PlayerActions.bgraph.
	void Graph_Harvest() { HandleInteraction(); }
	void Graph_HandleCrafting() { HandleCrafting(); }

	// Survival_CraftTick.bgraph (matches UpdateCrafting = m_xCrafting.Update(dt);
	// completion is delivered by the CraftingComplete event handler below).
	void Graph_AdvanceCraft(float fDt) { m_xCrafting.Update(fDt); }

	// Survival_WorldEvents.bgraph - the event-handler decision bodies verbatim.
	void Graph_OnResourceHarvested(SurvivalItemType eItemType, uint32_t uAmount)
	{
		// Add items to inventory
		m_xInventory.AddItem(eItemType, uAmount);
		// Show feedback
		ShowStatusMessage(eItemType, uAmount);
	}

	void Graph_OnResourceRespawned()
	{
		// Could show visual feedback, play sound, etc.
	}

	void Graph_OnCraftingComplete(SurvivalItemType eItemType, bool bSuccess)
	{
		if (bSuccess)
		{
			// Collect the crafted item
			m_xCrafting.CollectCraftedItem(m_xInventory);

			// Show message
			if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
			{
				Zenith_UIComponent& xUI = *pxUI;
				Survival_UIManager::ShowCraftingComplete(xUI, eItemType);
				m_fStatusMessageTimer = s_fStatusMessageDuration;
			}
		}
	}

	// Fires a custom event on this GameManager's graph component (the inline
	// C++->graph seam for "SurvivalPlayerTick" / "SurvivalCraftTick", and the
	// event-forwarder path). Nothing happens if no graph is attached.
	void FireGraphEvent(const char* szName, float fDt)
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
			return;
		Zenith_PropertyValue xDt;
		xDt.SetFloat(fDt);
		pxGraph->FireCustomEvent(szName, &xDt);
	}

	// Multi-field variant: the WorldEvents forwarders stage each event field as a
	// named arg (OnCustomEvent sources stash them verbatim to the graph blackboard
	// under the same names the SurvivalOn* nodes read).
	void FireGraphEventWithArgs(const char* szName, const Zenith_GraphEventArg* pxArgs, u_int uArgCount)
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
			return;
		pxGraph->FireCustomEventWithArgs(szName, pxArgs, uArgCount);
	}

private:
	// ========================================================================
	// Event Handling
	// ========================================================================

	void SubscribeToEvents()
	{
		// Captureless lambdas routed through s_pxInstance: components relocate
		// when their pool resizes / transfers, which would dangle a captured
		// `this`. The static instance pointer is fixed up by the move members
		// and OnAwake instead.

		// Subscribe to resource harvested event
		m_axEventHandles.PushBack(
			Survival_EventBus::SubscribeLambda<Survival_Event_ResourceHarvested>(
				[](const Survival_Event_ResourceHarvested& xEvent)
				{
					if (s_pxInstance)
					{
						s_pxInstance->OnResourceHarvested(xEvent);
					}
				}));

		// Subscribe to resource respawned event
		m_axEventHandles.PushBack(
			Survival_EventBus::SubscribeLambda<Survival_Event_ResourceRespawned>(
				[](const Survival_Event_ResourceRespawned&)
				{
					if (s_pxInstance)
					{
						s_pxInstance->OnResourceRespawned();
					}
				}));

		// Subscribe to crafting complete event
		m_axEventHandles.PushBack(
			Survival_EventBus::SubscribeLambda<Survival_Event_CraftingComplete>(
				[](const Survival_Event_CraftingComplete& xEvent)
				{
					if (s_pxInstance)
					{
						s_pxInstance->OnCraftingComplete(xEvent);
					}
				}));
	}

	// The three event handlers are now FORWARDERS into the Survival_WorldEvents
	// graph: they stage the event's fields as named args and fire the matching
	// custom event at exactly the point the C++ subscriber ran (harvest/craft
	// fires are nested inside the Hit()/Update() call the harvest/craft-tick nodes
	// made - a proven-safe re-entrant dispatch; respawn fires from the top-level
	// ProcessDeferredEvents drain). The decision bodies live verbatim in the
	// Graph_On* shims above, which the graph nodes call back.

	void OnResourceHarvested(const Survival_Event_ResourceHarvested& xEvent)
	{
		Zenith_GraphEventArg axArgs[2];
		axArgs[0].m_strName = "harvestItemType";
		axArgs[0].m_xValue.SetInt32(static_cast<int32_t>(xEvent.m_eItemType));
		axArgs[1].m_strName = "harvestAmount";
		axArgs[1].m_xValue.SetInt32(static_cast<int32_t>(xEvent.m_uAmount));
		FireGraphEventWithArgs("ResourceHarvested", axArgs, 2);
	}

	void OnResourceRespawned()
	{
		// The plan's headline: the respawn is produced by the C++ background task
		// and drained here on the main thread; forward it to the empty graph hook.
		FireGraphEvent("ResourceRespawned", 0.0f);
	}

	void OnCraftingComplete(const Survival_Event_CraftingComplete& xEvent)
	{
		Zenith_GraphEventArg axArgs[2];
		axArgs[0].m_strName = "craftItemType";
		axArgs[0].m_xValue.SetInt32(static_cast<int32_t>(xEvent.m_eItemType));
		axArgs[1].m_strName = "craftSuccess";
		axArgs[1].m_xValue.SetBool(xEvent.m_bSuccess);
		FireGraphEventWithArgs("CraftingComplete", axArgs, 2);
	}

	// ========================================================================
	// Input Handling
	// ========================================================================

	void HandleMovement(float fDt)
	{
		// C1: resolve owning scene from the player's entity id rather than
		// assuming it lives in the active scene (player may be persistent).
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(m_uPlayerEntityID);
		if (!pxSceneData)
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

	// (UpdateCrafting removed - the per-tick craft progression now lives in the
	//  Survival_CraftTick graph, driven by the "SurvivalCraftTick" event above;
	//  Graph_AdvanceCraft holds the m_xCrafting.Update(dt) body verbatim.)

	void UpdateResourceNodes(float fDt)
	{
		// Use task system for background node updates
		// Submit parallel update task
		uint32_t uNodeCount = m_xResourceManager.GetCount();
		if (uNodeCount > 0)
		{
			// For demonstration, use Zenith_DataParallelTask for parallel processing
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
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

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
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;
		Survival_UIManager::ShowHarvestFeedback(xUI, eItemType, uAmount);
		m_fStatusMessageTimer = s_fStatusMessageDuration;
	}

	void ShowNotEnoughMaterials()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;
		Survival_UIManager::ShowNotEnoughMaterials(xUI);
		m_fStatusMessageTimer = s_fStatusMessageDuration;
	}

	void ClearStatusMessage()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;
		Survival_UIManager::ClearStatusMessage(xUI);
	}

	// ========================================================================
	// Entity Lookup (find pre-created entities from Project_LoadInitialScene)
	// ========================================================================

	void FindSceneEntities()
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

		// Find player entity
		Zenith_Entity xPlayer = pxSceneData->FindEntityByName("Player");
		Zenith_Assert(xPlayer.IsValid(), "Player entity not found in scene - ensure scene was saved after Project_LoadInitialScene created entities");
		m_uPlayerEntityID = xPlayer.GetEntityID();

		// Find ground entity
		Zenith_Entity xGround = pxSceneData->FindEntityByName("Ground");
		Zenith_Assert(xGround.IsValid(), "Ground entity not found in scene");
		m_uGroundEntityID = xGround.GetEntityID();
	}

	void PopulateResourceManagerFromScene()
	{
		static constexpr uint32_t s_uTreeCount = 15;
		static constexpr uint32_t s_uRockCount = 10;
		static constexpr uint32_t s_uBerryCount = 8;

		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

		// Find and register all tree entities
		for (uint32_t i = 0; i < s_uTreeCount; i++)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "Tree_%u", i);
			Zenith_Entity xTree = pxSceneData->FindEntityByName(szName);
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
			Zenith_Entity xRock = pxSceneData->FindEntityByName(szName);
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
			Zenith_Entity xBush = pxSceneData->FindEntityByName(szName);
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

	// ========================================================================
	// Menu / Scene Transition
	// ========================================================================

	// (OnPlayClicked removed - the menu Play click is handled by the
	//  Survival_GameFlow graph's OnUIButtonClicked -> LoadSceneByIndex node.)

	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create world scene
		m_xWorldScene = g_xEngine.Scenes().LoadScene("World", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xWorldScene);
		Zenith_SceneData* pxWorldData = g_xEngine.Scenes().GetSceneData(m_xWorldScene);

		// Create world content (ground, player, resource nodes)
		Survival_CreateWorldContent(pxWorldData);

		// Initialize game systems for new world
		m_xResourceManager.Clear();
		FindSceneEntities();
		PopulateResourceManagerFromScene();

		m_xInventory.Reset();
		m_xCrafting.CancelCrafting();

		m_eGameState = SurvivalGameState::PLAYING;
	}

	void ReturnToMenu()
	{
		// Clear game systems
		m_xResourceManager.Clear();
		m_xCrafting.CancelCrafting();
		m_uPlayerEntityID = INVALID_ENTITY_ID;
		m_uGroundEntityID = INVALID_ENTITY_ID;

		// Unload the world scene (destroys all world entities)
		if (m_xWorldScene.IsValid())
		{
			g_xEngine.Scenes().UnloadScene(m_xWorldScene);
			m_xWorldScene = Zenith_Scene();
		}

		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void SetMenuVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		Zenith_UI::Zenith_UIElement* pxMenuTitle = xUI.FindElement("MenuTitle");
		Zenith_UI::Zenith_UIElement* pxMenuPlay = xUI.FindElement("MenuPlay");

		if (pxMenuTitle) pxMenuTitle->SetVisible(bVisible);
		if (pxMenuPlay) pxMenuPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		const char* aszHUDElements[] = {
			"Title", "ControlsHeader", "MoveInstr", "CraftInstr",
			"InventoryHeader", "WoodCount", "StoneCount", "BerriesCount",
			"CraftedHeader", "AxeCount", "PickaxeCount",
			"InteractPrompt", "CraftProgress", "Status"
		};

		for (const char* szName : aszHUDElements)
		{
			Zenith_UI::Zenith_UIElement* pxElement = xUI.FindElement(szName);
			if (pxElement)
				pxElement->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		// Single button - keep it focused
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Zenith_UI::Zenith_UIButton* pxPlay = static_cast<Zenith_UI::Zenith_UIButton*>(xUI.FindElement("MenuPlay"));
			if (pxPlay)
				pxPlay->SetFocused(true);
		}
	}

	void ResetGame()
	{
		if (m_eGameState != SurvivalGameState::PLAYING)
			return;

		// Reset player position
		// C1: resolve owning scene from the player's entity id.
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(m_uPlayerEntityID);
		if (xPlayer.IsValid())
		{
			xPlayer.GetComponent<Zenith_TransformComponent>().SetPosition(
				Zenith_Maths::Vector3(0.f, s_fPlayerHeight * 0.5f, 0.f));
		}

		// Reset all resource nodes to full health
		m_xResourceManager.ForEach([](Survival_ResourceNodeData& xNode, uint32_t)
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
	// Move Fix-Up
	// ========================================================================

	// Called by the move members after state transfer: repoints s_pxInstance
	// (and the task processor's resource-manager pointer) at the new location
	// when the moved-from object was the live instance.
	void FixUpInstancePointerAfterMove(Survival_GameComponent& xMovedFrom)
	{
		if (s_pxInstance == &xMovedFrom)
		{
			s_pxInstance = this;
			Survival_TaskProcessor::Initialize(&m_xResourceManager);
		}
	}

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Static instance pointer for the captureless event-bus lambdas. Set in
	// OnAwake, cleared in OnDestroy, repointed by the move members (components
	// relocate on pool resize). Single live instance per process by design.
	static inline Survival_GameComponent* s_pxInstance = nullptr;

	// Entity IDs
	Zenith_EntityID m_uPlayerEntityID;
	Zenith_EntityID m_uGroundEntityID;

	// Game systems
	Survival_Inventory m_xInventory;
	Survival_ResourceNodeManager m_xResourceManager;
	Survival_CraftingSystem m_xCrafting;

	// Event subscription handles
	Zenith_Vector<Zenith_EventHandle> m_axEventHandles;

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

	// Scene management
	SurvivalGameState m_eGameState;
	Zenith_Scene m_xWorldScene;
	int32_t m_iFocusIndex;

	// Owning entity (explicit member now - was provided by the old script base)
	Zenith_Entity m_xParentEntity;
};
