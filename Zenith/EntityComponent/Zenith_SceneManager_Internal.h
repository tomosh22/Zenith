#pragma once

// Internal shared symbols between Zenith_SceneManager.cpp and the sibling
// translation units carved out of it (currently just Zenith_SceneManager_AsyncLoad.cpp).
// These used to be file-static inside Zenith_SceneManager.cpp; extracting the async-load
// subsystem forced them to have external linkage. Keep this header out of the public
// SceneManager API — callers should continue to go through Zenith_SceneManager's
// static members.

#include <cstdint>
#include <string>
#include "EntityComponent/Zenith_Scene.h"

namespace Zenith_SceneManagerDetail
{
	// Suppression flags for consolidating the ActiveSceneChanged callback during a
	// SCENE_LOAD_SINGLE teardown/activation pair. Phase 1 sets them, Phase 2 fires
	// the single consolidated old->new transition, Phase 2 also clears them.
	extern bool s_bSuppressActiveSceneChanged;
	extern bool s_bHaveDeferredOldActive;
	extern Zenith_Scene s_xDeferredOldActive;

	// Pending build index plumb: LoadSceneByIndex / LoadSceneAsyncByIndex park the
	// caller-supplied build index here; the downstream scene-creation code consumes
	// it before firing SceneLoaded. Main-thread-only; an RAII guard is declared in
	// Zenith_SceneManager.cpp restores the prior value on scope exit.
	extern int s_iPendingBuildIndex;

	// Path-to-name helper used by scene-creation code paths (sync and async).
	std::string ExtractSceneNameFromPath(const std::string& strPath);

	// Async load progress milestones — the 0.0-1.0 contract callers rely on.
	// Single source of truth: previously these were duplicated between
	// Zenith_SceneManager.cpp and Zenith_SceneManager_AsyncLoad.cpp.
	inline constexpr float fPROGRESS_FILE_READ_START       = 0.1f;
	inline constexpr float fPROGRESS_FILE_READ_COMPLETE    = 0.7f;
	inline constexpr float fPROGRESS_SCENE_CREATED         = 0.75f;
	inline constexpr float fPROGRESS_DESERIALIZE_START     = 0.8f;
	inline constexpr float fPROGRESS_DESERIALIZE_COMPLETE  = 0.85f;
	inline constexpr float fPROGRESS_ACTIVATION_PAUSED     = 0.9f;
	inline constexpr float fPROGRESS_COMPLETE              = 1.0f;

	// Async unload weighting — 90% destruction, 10% cleanup.
	inline constexpr float fPROGRESS_DESTRUCTION_WEIGHT    = 0.9f;

	// How many frames a completed operation is kept alive so callers can still
	// read GetResultScene() after polling IsComplete().
	inline constexpr uint32_t uOPERATION_CLEANUP_DELAY_FRAMES = 60;
}
