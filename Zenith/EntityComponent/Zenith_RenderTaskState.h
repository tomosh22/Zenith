#pragma once

// One-line free function exposing Zenith_SceneManager::AreRenderTasksActive()
// without dragging the full Zenith_SceneManager.h include in.
//
// SceneData.h's template assertion bodies need to know whether render tasks
// are in flight (so they can permit concurrent reads). Including SceneManager.h
// from SceneData.h to call the static accessor would close the textual cycle
// SceneData.h ↔ SceneManager.h. This header carries just the free-function
// forwarder; the underlying flag storage and the static class accessor stay
// in Zenith_SceneManagerInternal.h, and the qualified
// `Zenith_SceneManager::AreRenderTasksActive()` form keeps working at all
// other call sites (~25 of them across SceneRegistry.cpp, Query.h, etc.).
//
// Defined in Zenith_SceneManager.cpp.
bool Zenith_AreRenderTasksActive();
