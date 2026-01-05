#pragma once
/**
 * Survival_EventBus.h - Custom game events using Zenith_EventDispatcher
 *
 * Demonstrates the Zenith_EventSystem for type-safe game events.
 *
 * Key features:
 * - Custom game event definitions
 * - Immediate dispatch (on same frame)
 * - Deferred dispatch (thread-safe, for background tasks)
 * - Event subscription with lambdas or function pointers
 *
 * Usage:
 *   // Subscribe to event
 *   auto handle = Survival_EventBus::Subscribe<Survival_Event_ResourceHarvested>(
 *       [](const Survival_Event_ResourceHarvested& event) {
 *           // Handle resource harvested
 *       });
 *
 *   // Dispatch event
 *   Survival_EventBus::Dispatch(Survival_Event_ResourceHarvested{ uNodeID, ITEM_TYPE_WOOD, 3 });
 *
 *   // Queue event from background thread
 *   Survival_EventBus::QueueEvent(Survival_Event_CraftingComplete{ ITEM_TYPE_AXE });
 */

#include "EntityComponent/Zenith_EventSystem.h"
#include <cstdint>

// ============================================================================
// Item Types
// ============================================================================
enum SurvivalItemType : uint32_t
{
	ITEM_TYPE_NONE = 0,
	ITEM_TYPE_WOOD,
	ITEM_TYPE_STONE,
	ITEM_TYPE_BERRIES,
	ITEM_TYPE_AXE,
	ITEM_TYPE_PICKAXE,
	ITEM_TYPE_COUNT
};

inline const char* GetItemName(SurvivalItemType eType)
{
	static const char* s_aszNames[] = {
		"None",
		"Wood",
		"Stone",
		"Berries",
		"Axe",
		"Pickaxe"
	};
	if (eType < ITEM_TYPE_COUNT)
		return s_aszNames[eType];
	return "Unknown";
}

// ============================================================================
// Resource Node Types
// ============================================================================
enum SurvivalResourceType : uint32_t
{
	RESOURCE_TYPE_TREE = 0,
	RESOURCE_TYPE_ROCK,
	RESOURCE_TYPE_BERRY_BUSH,
	RESOURCE_TYPE_COUNT
};

inline const char* GetResourceName(SurvivalResourceType eType)
{
	static const char* s_aszNames[] = {
		"Tree",
		"Rock",
		"Berry Bush"
	};
	if (eType < RESOURCE_TYPE_COUNT)
		return s_aszNames[eType];
	return "Unknown";
}

// ============================================================================
// Game Events
// ============================================================================

/**
 * Survival_Event_ResourceHarvested - Fired when player harvests from a resource node
 */
struct Survival_Event_ResourceHarvested
{
	Zenith_EntityID m_uNodeEntityID = INVALID_ENTITY_ID;
	SurvivalItemType m_eItemType = ITEM_TYPE_NONE;
	uint32_t m_uAmount = 0;
};

/**
 * Survival_Event_ResourceDepleted - Fired when a resource node is fully depleted
 */
struct Survival_Event_ResourceDepleted
{
	Zenith_EntityID m_uNodeEntityID = INVALID_ENTITY_ID;
	SurvivalResourceType m_eResourceType = RESOURCE_TYPE_TREE;
};

/**
 * Survival_Event_ResourceRespawned - Fired when a resource node respawns
 */
struct Survival_Event_ResourceRespawned
{
	Zenith_EntityID m_uNodeEntityID = INVALID_ENTITY_ID;
	SurvivalResourceType m_eResourceType = RESOURCE_TYPE_TREE;
};

/**
 * Survival_Event_InventoryChanged - Fired when player inventory changes
 */
struct Survival_Event_InventoryChanged
{
	SurvivalItemType m_eItemType = ITEM_TYPE_NONE;
	int32_t m_iDelta = 0;  // Positive = gained, negative = lost
	uint32_t m_uNewTotal = 0;
};

/**
 * Survival_Event_CraftingStarted - Fired when crafting begins
 */
struct Survival_Event_CraftingStarted
{
	SurvivalItemType m_eItemType = ITEM_TYPE_NONE;
	float m_fDuration = 0.f;
};

/**
 * Survival_Event_CraftingProgress - Fired during crafting to update progress
 */
struct Survival_Event_CraftingProgress
{
	SurvivalItemType m_eItemType = ITEM_TYPE_NONE;
	float m_fProgress = 0.f;  // 0.0 to 1.0
};

/**
 * Survival_Event_CraftingComplete - Fired when crafting finishes (from background task)
 */
struct Survival_Event_CraftingComplete
{
	SurvivalItemType m_eItemType = ITEM_TYPE_NONE;
	bool m_bSuccess = true;
};

/**
 * Survival_Event_PlayerInteraction - Fired when player interacts with something
 */
struct Survival_Event_PlayerInteraction
{
	Zenith_EntityID m_uTargetEntityID = INVALID_ENTITY_ID;
	bool m_bStarted = true;  // true = started, false = ended
};

/**
 * Survival_Event_WorldUpdateComplete - Fired when background world update task finishes
 */
struct Survival_Event_WorldUpdateComplete
{
	uint32_t m_uNodesUpdated = 0;
	float m_fDeltaTime = 0.f;
};

// ============================================================================
// Event Bus - Static interface to Zenith_EventDispatcher
// ============================================================================
class Survival_EventBus
{
public:
	/**
	 * Subscribe - Register a callback for an event type
	 * @return Handle for unsubscribing
	 */
	template<typename TEvent>
	static Zenith_EventHandle Subscribe(void(*pfnCallback)(const TEvent&))
	{
		return Zenith_EventDispatcher::Get().Subscribe<TEvent>(pfnCallback);
	}

	/**
	 * SubscribeLambda - Register a lambda callback for an event type
	 * @return Handle for unsubscribing
	 */
	template<typename TEvent, typename TCallback>
	static Zenith_EventHandle SubscribeLambda(TCallback&& xCallback)
	{
		return Zenith_EventDispatcher::Get().SubscribeLambda<TEvent>(std::forward<TCallback>(xCallback));
	}

	/**
	 * Unsubscribe - Remove a subscription by handle
	 */
	static void Unsubscribe(Zenith_EventHandle uHandle)
	{
		Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	}

	/**
	 * Dispatch - Fire an event immediately to all subscribers
	 * @note Not thread-safe, call from main thread only
	 */
	template<typename TEvent>
	static void Dispatch(const TEvent& xEvent)
	{
		Zenith_EventDispatcher::Get().Dispatch(xEvent);
	}

	/**
	 * QueueEvent - Queue an event for deferred processing
	 * @note Thread-safe, can be called from background tasks
	 */
	template<typename TEvent>
	static void QueueEvent(const TEvent& xEvent)
	{
		Zenith_EventDispatcher::Get().QueueEvent(xEvent);
	}

	/**
	 * ProcessDeferredEvents - Process all queued events
	 * @note Call from main thread only (usually in OnUpdate)
	 */
	static void ProcessDeferredEvents()
	{
		Zenith_EventDispatcher::Get().ProcessDeferredEvents();
	}

	/**
	 * GetSubscriberCount - Get number of subscribers for an event type
	 */
	template<typename TEvent>
	static u_int GetSubscriberCount()
	{
		return Zenith_EventDispatcher::Get().GetSubscriberCount<TEvent>();
	}
};
