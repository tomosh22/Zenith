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
