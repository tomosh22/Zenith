#pragma once

// One-line free function exposing g_xEngine.Scenes().AreRenderTasksActive()
// without dragging the full Zenith_SceneSystem.h include in.
//
// SceneData.h's template assertion bodies need to know whether render tasks
// are in flight (so they can permit concurrent reads). Including
// Zenith_SceneSystem.h from SceneData.h to call the accessor would close a
// textual include cycle (SceneSystem.h pulls in SceneData.h for its template
// bodies). This header carries just the free-function forwarder; callers that
// already have Zenith_SceneSystem.h use g_xEngine.Scenes().AreRenderTasksActive()
// directly.
//
// Defined in Internal/Zenith_SceneSystem_Lifecycle.cpp.
bool Zenith_AreRenderTasksActive();

// WS10: same cycle-break pattern for the sparse-set query read toggle. The
// templated Zenith_Query<Ts...> in Zenith_Query.h must read the toggle without
// including Zenith_SceneSystem.h (that would close the include cycle, since
// SceneSystem.h pulls in SceneData.h -> Query types). This free-function
// forwards to g_xEngine.Scenes().AreSparseQueryReadsEnabled(). Query.h includes
// THIS header and calls the forwarder.
//
// Defined in Internal/Zenith_SceneSystem_Lifecycle.cpp, beside the one above.
bool Zenith_AreSparseQueryReadsEnabled();

// Main-thread predicate for the ECS core's thread-affinity asserts. Forwards to
// the ECS runtime hook (Zenith_ECSRuntimeHooks::m_pfnIsMainThread); returns true
// when no hook is installed (permissive, so an un-bootstrapped ECS asserts as if
// on the main thread). Lets leaf headers (Zenith_Query.h, Zenith_EventSystem.h)
// assert thread affinity without naming g_xEngine or including the engine header.
//
// Defined in Internal/Zenith_SceneSystem_Lifecycle.cpp, beside the two above.
bool Zenith_ECS_IsMainThread();

// Leaf forwarder to the SceneSystem-owned process-wide entity store; mirrors the
// other forwarders here. Lets ECS HEADERS (Zenith_SceneData.h, Zenith_Entity.inl)
// reach the entity store without including Zenith_SceneSystem.h (that would close
// the include cycle, since SceneSystem.h pulls in SceneData.h). Returns the exact
// same store instance the SceneSystem owns.
//
// Defined in Internal/Zenith_SceneSystem_Lifecycle.cpp, beside the ones above.
class Zenith_EntityStore;
Zenith_EntityStore& Zenith_ECS_EntityStore();
