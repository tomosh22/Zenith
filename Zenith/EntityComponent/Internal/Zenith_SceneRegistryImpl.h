#pragma once

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include <string>

class Zenith_SceneData;

// Phase 5b: per-Engine scene-registry state. The 8 statics on
// Zenith_SceneRegistry (s_axScenes / s_axSceneGenerations / s_axFreeHandles
// / s_iActiveSceneHandle / s_iPersistentSceneHandle / s_ulNextLoadTimestamp
// / s_axLoadedSceneNames / s_axBuildIndexToPath) move onto this Impl, held
// by Zenith_Engine. The static facade keeps its name and its method
// surface; method bodies now read/write g_xEngine.SceneRegistry().m_xXxx.
//
// Members are public for the same pragmatic reason as
// Zenith_ProfilingImpl / Zenith_PhysicsImpl: the static facade is the only
// caller, so wrapping each in a getter would be pure noise.
class Zenith_SceneRegistryImpl
{
public:
	Zenith_SceneRegistryImpl() = default;
	~Zenith_SceneRegistryImpl() = default;

	Zenith_SceneRegistryImpl(const Zenith_SceneRegistryImpl&) = delete;
	Zenith_SceneRegistryImpl& operator=(const Zenith_SceneRegistryImpl&) = delete;

	// Scene-slot table. m_axScenes[i] is the SceneData* for handle i (nullptr
	// when the slot is free). m_axSceneGenerations[i] increments on every
	// free/realloc cycle so cached Zenith_Scene handles can detect staleness.
	Zenith_Vector<Zenith_SceneData*>                       m_axScenes;
	Zenith_Vector<uint32_t>                                m_axSceneGenerations;
	Zenith_Vector<int>                                     m_axFreeHandles;

	// Active / persistent scene tracking.
	int                                                    m_iActiveSceneHandle     = -1;
	int                                                    m_iPersistentSceneHandle = -1;

	// Monotonic load-order counter (1-based; 0 == not loaded).
	uint64_t                                               m_ulNextLoadTimestamp    = 1;

	// Name-lookup cache. Parallel to m_axScenes; rebuilt by AddToSceneNameCache /
	// RemoveFromSceneNameCache as scenes load and unload.
	Zenith_Vector<Zenith_SceneRegistry::SceneNameEntry>    m_axLoadedSceneNames;

	// Build-index -> path registry, populated at editor automation time.
	Zenith_Vector<std::string>                             m_axBuildIndexToPath;
};
