#pragma once
/**
 * Survival_TaskProcessor.h - Background task processing using Zenith_TaskSystem
 *
 * Demonstrates the Zenith_Task and Zenith_TaskArray systems for
 * parallel background processing.
 *
 * Key features:
 * - Zenith_Task for single background operations
 * - Zenith_TaskArray for parallel processing across multiple items
 * - Thread-safe event queuing from background tasks
 * - Integration with game systems
 *
 * Usage:
 *   // Initialize
 *   Survival_TaskProcessor::Initialize(&xResourceManager);
 *
 *   // Submit world update task
 *   Survival_TaskProcessor::SubmitWorldUpdateTask(fDeltaTime);
 *
 *   // Wait for completion (or let it run async)
 *   Survival_TaskProcessor::WaitForWorldUpdate();
 */

#include "TaskSystem/Zenith_TaskSystem.h"
#include "Profiling/Zenith_Profiling.h"
#include "Survival_ResourceNode.h"
#include "Survival_EventBus.h"
#include <atomic>

/**
 * WorldUpdateTaskData - Data passed to world update task
 */
struct WorldUpdateTaskData
{
	Survival_ResourceNodeManager* m_pxResourceManager = nullptr;
	float m_fDeltaTime = 0.f;
	std::atomic<uint32_t> m_uNodesUpdated{0};
	std::atomic<uint32_t> m_uNodesRespawned{0};
};

/**
 * NodeUpdateTaskData - Data for parallel node update task array
 */
struct NodeUpdateTaskData
{
	Survival_ResourceNodeManager* m_pxResourceManager = nullptr;
	float m_fDeltaTime = 0.f;
	std::atomic<uint32_t> m_uNodesRespawned{0};
};

/**
 * Survival_TaskProcessor - Manages background tasks for the survival game
 */
class Survival_TaskProcessor
{
public:
	/**
	 * Initialize - Set up task processor with resource manager reference
	 */
	static void Initialize(Survival_ResourceNodeManager* pxResourceManager)
	{
		s_xWorldUpdateData.m_pxResourceManager = pxResourceManager;
		s_xNodeUpdateData.m_pxResourceManager = pxResourceManager;
		s_bInitialized = true;
	}

	/**
	 * Shutdown - Clean up task processor
	 */
	static void Shutdown()
	{
		// Wait for any pending tasks
		if (s_pxWorldUpdateTask)
		{
			s_pxWorldUpdateTask->WaitUntilComplete();
			delete s_pxWorldUpdateTask;
			s_pxWorldUpdateTask = nullptr;
		}

		if (s_pxNodeUpdateTaskArray)
		{
			s_pxNodeUpdateTaskArray->WaitUntilComplete();
			delete s_pxNodeUpdateTaskArray;
			s_pxNodeUpdateTaskArray = nullptr;
		}

		s_bInitialized = false;
	}

	// ========================================================================
	// Single Task: World Update
	// Demonstrates Zenith_Task for a single background operation
	// ========================================================================

	/**
	 * SubmitWorldUpdateTask - Submit task to update world state
	 *
	 * This demonstrates using Zenith_Task for a single background operation.
	 * The task updates resource node respawn timers.
	 */
	static void SubmitWorldUpdateTask(float fDeltaTime)
	{
		if (!s_bInitialized || !s_xWorldUpdateData.m_pxResourceManager)
			return;

		// Wait for previous task if still running
		if (s_pxWorldUpdateTask)
		{
			s_pxWorldUpdateTask->WaitUntilComplete();
			delete s_pxWorldUpdateTask;
		}

		// Reset atomic counters
		s_xWorldUpdateData.m_fDeltaTime = fDeltaTime;
		s_xWorldUpdateData.m_uNodesUpdated.store(0);
		s_xWorldUpdateData.m_uNodesRespawned.store(0);

		// Create and submit task
		s_pxWorldUpdateTask = new Zenith_Task(
			ZENITH_PROFILE_INDEX__SCENE_UPDATE,  // Use scene update profile index
			WorldUpdateTaskFunction,
			&s_xWorldUpdateData
		);

		Zenith_TaskSystem::SubmitTask(s_pxWorldUpdateTask);
	}

	/**
	 * WaitForWorldUpdate - Block until world update task completes
	 */
	static void WaitForWorldUpdate()
	{
		if (s_pxWorldUpdateTask)
		{
			s_pxWorldUpdateTask->WaitUntilComplete();
		}
	}

	/**
	 * IsWorldUpdateComplete - Check if world update is done (non-blocking)
	 */
	static bool IsWorldUpdateComplete()
	{
		// If no task or task completed
		return s_pxWorldUpdateTask == nullptr;
	}

	// ========================================================================
	// Task Array: Parallel Node Update
	// Demonstrates Zenith_TaskArray for parallel processing
	// ========================================================================

	/**
	 * SubmitParallelNodeUpdate - Update nodes in parallel using TaskArray
	 *
	 * This demonstrates using Zenith_TaskArray for distributing work
	 * across multiple worker threads.
	 *
	 * @param fDeltaTime Time step for respawn timer updates
	 * @param uNumNodes Number of nodes to process
	 */
	static void SubmitParallelNodeUpdate(float fDeltaTime, uint32_t uNumNodes)
	{
		if (!s_bInitialized || !s_xNodeUpdateData.m_pxResourceManager || uNumNodes == 0)
			return;

		// Wait for previous task if still running
		if (s_pxNodeUpdateTaskArray)
		{
			s_pxNodeUpdateTaskArray->WaitUntilComplete();
			delete s_pxNodeUpdateTaskArray;
		}

		// Reset data
		s_xNodeUpdateData.m_fDeltaTime = fDeltaTime;
		s_xNodeUpdateData.m_uNodesRespawned.store(0);

		// Create task array with one invocation per node
		// Worker threads will each grab work items atomically
		s_pxNodeUpdateTaskArray = new Zenith_TaskArray(
			ZENITH_PROFILE_INDEX__SCENE_UPDATE,
			ParallelNodeUpdateFunction,
			&s_xNodeUpdateData,
			uNumNodes,
			true  // Submitting thread joins (main thread helps with work)
		);

		Zenith_TaskSystem::SubmitTaskArray(s_pxNodeUpdateTaskArray);
	}

	/**
	 * WaitForParallelNodeUpdate - Block until parallel update completes
	 */
	static void WaitForParallelNodeUpdate()
	{
		if (s_pxNodeUpdateTaskArray)
		{
			s_pxNodeUpdateTaskArray->WaitUntilComplete();
		}
	}

	/**
	 * GetLastUpdateStats - Get statistics from last world update
	 */
	static void GetLastUpdateStats(uint32_t& uNodesUpdated, uint32_t& uNodesRespawned)
	{
		uNodesUpdated = s_xWorldUpdateData.m_uNodesUpdated.load();
		uNodesRespawned = s_xWorldUpdateData.m_uNodesRespawned.load();
	}

	/**
	 * GetParallelUpdateRespawnCount - Get respawn count from parallel update
	 */
	static uint32_t GetParallelUpdateRespawnCount()
	{
		return s_xNodeUpdateData.m_uNodesRespawned.load();
	}

private:
	/**
	 * WorldUpdateTaskFunction - Task function for world update
	 *
	 * This runs on a worker thread. Note: Must be careful with thread safety.
	 * Uses thread-safe event queue for communication back to main thread.
	 */
	static void WorldUpdateTaskFunction(void* pData)
	{
		WorldUpdateTaskData* pTaskData = static_cast<WorldUpdateTaskData*>(pData);
		if (!pTaskData || !pTaskData->m_pxResourceManager)
			return;

		Survival_ResourceNodeManager* pResourceMgr = pTaskData->m_pxResourceManager;
		float fDt = pTaskData->m_fDeltaTime;

		uint32_t uUpdated = 0;
		uint32_t uRespawned = 0;

		// Update all resource nodes
		// Note: This is safe because we're only reading/writing node data,
		// not modifying the scene graph
		pResourceMgr->ForEach([&](Survival_ResourceNodeData& xNode, uint32_t uIndex)
		{
			uUpdated++;

			if (xNode.m_bDepleted)
			{
				xNode.m_fRespawnTimer -= fDt;
				if (xNode.m_fRespawnTimer <= 0.f)
				{
					xNode.m_bDepleted = false;
					xNode.m_uCurrentHits = xNode.m_uMaxHits;
					xNode.m_fRespawnTimer = 0.f;
					uRespawned++;

					// Queue event for main thread (thread-safe)
					Survival_EventBus::QueueEvent(Survival_Event_ResourceRespawned{
						xNode.m_uEntityID,
						xNode.m_eResourceType
					});
				}
			}
		});

		// Store results
		pTaskData->m_uNodesUpdated.store(uUpdated);
		pTaskData->m_uNodesRespawned.store(uRespawned);

		// Queue completion event
		Survival_EventBus::QueueEvent(Survival_Event_WorldUpdateComplete{
			uUpdated,
			fDt
		});
	}

	/**
	 * ParallelNodeUpdateFunction - Task array function for parallel node updates
	 *
	 * Each invocation processes one node. Multiple worker threads call this
	 * with different invocation indices.
	 *
	 * @param pData Pointer to NodeUpdateTaskData
	 * @param uInvocationIndex Which work item (node index) this invocation handles
	 * @param uNumInvocations Total number of invocations (nodes)
	 */
	static void ParallelNodeUpdateFunction(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
	{
		NodeUpdateTaskData* pTaskData = static_cast<NodeUpdateTaskData*>(pData);
		if (!pTaskData || !pTaskData->m_pxResourceManager)
			return;

		// Get the node for this invocation
		Survival_ResourceNodeData* pNode = pTaskData->m_pxResourceManager->GetNode(uInvocationIndex);
		if (!pNode)
			return;

		float fDt = pTaskData->m_fDeltaTime;

		// Process this single node
		if (pNode->m_bDepleted)
		{
			pNode->m_fRespawnTimer -= fDt;
			if (pNode->m_fRespawnTimer <= 0.f)
			{
				pNode->m_bDepleted = false;
				pNode->m_uCurrentHits = pNode->m_uMaxHits;
				pNode->m_fRespawnTimer = 0.f;

				// Atomically increment respawn counter
				pTaskData->m_uNodesRespawned.fetch_add(1);

				// Queue event for main thread
				Survival_EventBus::QueueEvent(Survival_Event_ResourceRespawned{
					pNode->m_uEntityID,
					pNode->m_eResourceType
				});
			}
		}
	}

	// Static data
	inline static bool s_bInitialized = false;

	// World update task (single task)
	inline static Zenith_Task* s_pxWorldUpdateTask = nullptr;
	inline static WorldUpdateTaskData s_xWorldUpdateData;

	// Parallel node update (task array)
	inline static Zenith_TaskArray* s_pxNodeUpdateTaskArray = nullptr;
	inline static NodeUpdateTaskData s_xNodeUpdateData;
};
