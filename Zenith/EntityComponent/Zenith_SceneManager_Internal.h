#pragma once

// Internal shared symbols used across Zenith_SceneManager.cpp and the
// EntityComponent/Internal/ subsystem translation units (Zenith_SceneRegistry,
// Zenith_SceneCallbackBus, Zenith_SceneOperationQueue, Zenith_SceneLifecycleScheduler,
// Zenith_SceneEntityOwnership). These used to be file-static inside
// Zenith_SceneManager.cpp before the Phase A subsystem extraction; the carve-up
// forced them to have external linkage. Keep this header out of the public
// SceneManager API — callers should continue to go through Zenith_SceneManager's
// static members.

#include <cstdint>
#include <string>
#include "EntityComponent/Zenith_Scene.h"

namespace Zenith_SceneManagerDetail
{
	// A1: ActiveSceneChanged suppression flags now live as private statics inside
	// Zenith_SceneCallbackBus.
	// A4: s_iPendingBuildIndex moved to Zenith_SceneLifecycleScheduler.

	// Path-to-name helper used by scene-creation code paths (sync and async).
	std::string ExtractSceneNameFromPath(const std::string& strPath);

	// Async load progress milestones — the 0.0-1.0 contract callers rely on.
	// Single source of truth: previously these were duplicated between
	// Zenith_SceneManager.cpp and the now-retired Zenith_SceneManager_AsyncLoad.cpp.
	// Now consumed by Zenith_SceneOperationQueue's Phase 1/Phase 2 helpers.
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
