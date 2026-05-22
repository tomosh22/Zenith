#pragma once

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"

class Zenith_SceneOperation;

// Phase 5d: per-Engine state for the async scene-operation pipeline.
// Replaces the 10 statics on Zenith_SceneOperationQueue (operation map +
// active-operation list + load/unload job queues + config knobs +
// re-entrancy depth counters). Members are public; the static facade
// reads/writes them via g_xEngine.SceneOperations().m_xXxx.
class Zenith_SceneOperationQueueImpl
{
public:
	Zenith_SceneOperationQueueImpl() = default;
	~Zenith_SceneOperationQueueImpl() = default;

	Zenith_SceneOperationQueueImpl(const Zenith_SceneOperationQueueImpl&) = delete;
	Zenith_SceneOperationQueueImpl& operator=(const Zenith_SceneOperationQueueImpl&) = delete;

	Zenith_Vector<Zenith_SceneOperation*>                          m_axActiveOperations;
	Zenith_Vector<Zenith_SceneOperationQueue::OperationMapEntry>   m_axOperationMap;
	uint64_t                                                       m_ulNextOperationID = 1;
	Zenith_Vector<Zenith_SceneOperationQueue::AsyncLoadJob*>       m_axAsyncJobs;
	bool                                                           m_bAsyncJobsNeedSort = false;
	Zenith_Vector<Zenith_SceneOperationQueue::AsyncUnloadJob*>     m_axAsyncUnloadJobs;
	uint32_t                                                       m_uAsyncUnloadBatchSize    = 50;
	uint32_t                                                       m_uMaxConcurrentAsyncLoads = 8;

	// Re-entrancy depths for ProcessPendingAsyncLoads /
	// ProcessPendingAsyncUnloads (RAII-bumped at entry, released at exit).
	uint32_t                                                       m_uProcessingAsyncLoadsDepth   = 0;
	uint32_t                                                       m_uProcessingAsyncUnloadsDepth = 0;
};
