#pragma once

#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

struct Flux_TerrainStreamingState;

// Phase 7c: per-Engine state for the terrain streaming manager.
class Flux_TerrainStreamingManagerImpl
{
public:
	Flux_TerrainStreamingManagerImpl() = default;
	~Flux_TerrainStreamingManagerImpl() = default;

	Flux_TerrainStreamingManagerImpl(const Flux_TerrainStreamingManagerImpl&) = delete;
	Flux_TerrainStreamingManagerImpl& operator=(const Flux_TerrainStreamingManagerImpl&) = delete;

	bool                                       m_bInitialized = false;
	Zenith_Vector<Flux_TerrainStreamingState*> m_xRegistry;
	Zenith_Mutex                               m_xRegistryMutex;
};
