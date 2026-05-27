#pragma once

// =============================================================================
// Zenith_SceneCallbackTypes.h
// -----------------------------------------------------------------------------
// Top-level callback-related typedefs for the scene system. Previously nested
// inside `class Zenith_SceneManager`; Phase 5e moves them out so the
// SceneManager class can be deleted while preserving the type names callers
// use.
//
// Old name (nested)                                  -> New top-level name
//   Zenith_SceneManager::CallbackHandle              -> Zenith_SceneCallbackHandle
//   Zenith_SceneManager::INVALID_CALLBACK_HANDLE     -> Zenith_INVALID_SCENE_CALLBACK_HANDLE
//   Zenith_SceneManager::SceneChangedCallback        -> Zenith_SceneChangedCallback
//   Zenith_SceneManager::SceneLoadedCallback         -> Zenith_SceneLoadedCallback
//   Zenith_SceneManager::SceneUnloadingCallback      -> Zenith_SceneUnloadingCallback
//   Zenith_SceneManager::SceneUnloadedCallback       -> Zenith_SceneUnloadedCallback
//   Zenith_SceneManager::SceneLoadStartedCallback    -> Zenith_SceneLoadStartedCallback
//   Zenith_SceneManager::EntityPersistentCallback    -> Zenith_EntityPersistentCallback
// =============================================================================

#include <cstdint>
#include <string>

struct Zenith_Scene;
class  Zenith_Entity;
enum   Zenith_SceneLoadMode : uint8_t;

using Zenith_SceneCallbackHandle = uint64_t;
inline constexpr Zenith_SceneCallbackHandle Zenith_INVALID_SCENE_CALLBACK_HANDLE = 0;

using Zenith_SceneChangedCallback        = void(*)(Zenith_Scene, Zenith_Scene);
using Zenith_SceneLoadedCallback         = void(*)(Zenith_Scene, Zenith_SceneLoadMode);
using Zenith_SceneUnloadingCallback      = void(*)(Zenith_Scene);  // BEFORE destruction
using Zenith_SceneUnloadedCallback       = void(*)(Zenith_Scene);  // AFTER destruction
using Zenith_SceneLoadStartedCallback    = void(*)(const std::string&);
using Zenith_EntityPersistentCallback    = void(*)(const Zenith_Entity&);
