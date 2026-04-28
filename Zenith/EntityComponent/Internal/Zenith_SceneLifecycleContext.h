#pragma once

// Zenith_SceneLifecycleContext — read-only cross-subsystem state surface.
//
// This is the dependency contract between the scene-system subsystems
// (Zenith_SceneManager facade, future Zenith_SceneRegistry, Zenith_SceneCallbackBus,
// Zenith_SceneOperationQueue, Zenith_SceneLifecycleScheduler, Zenith_SceneEntityOwnership):
//
//   * All cross-subsystem READS go through this header. Subsystems must not
//     reach into each other's private statics.
//   * All cross-subsystem WRITES go through public RAII types declared in
//     Zenith_SceneManager.h (LifecycleDeferralGuard, PrefabInstantiationGuard,
//     SceneUpdateDeferralGuard, PendingBuildIndexGuard, ActiveSceneChangeSuppressionScope,
//     SceneCreationTargetScope). No `Set*_Internal` writers exist in this header
//     by design — every cross-subsystem mutation is visible at its call site as
//     an RAII declaration.
//
// During Phase A extraction this header forwards to the existing
// Zenith_SceneManager statics. As subsystems are carved out, each accessor
// is repointed at its new owner without changing the call surface.

#include <cstdint>
#include <string>

#include "EntityComponent/Zenith_Scene.h"

namespace Zenith_SceneLifecycleContext
{
	// Scheduler-owned (after A4); read by queue, ownership, manager facade.
	bool IsLoadingScene();
	bool IsPrefabInstantiating();
	bool IsUpdating();

	// Set once by Zenith_Core when Zenith_MainLoop starts; cleared on shutdown.
	// Read by LoadSceneBlockingForBootstrap to assert bootstrap-only invocation
	// (B4). Stub returns false until B4 wires the flag through Zenith_Core.
	bool IsMainLoopRunning();

	// Returns -1 when no LoadSceneByIndex / LoadSceneAsyncByIndex is currently
	// staging a build-index value into the scene-being-created.
	int GetPendingBuildIndex();

	// True when the canonical path is already being loaded (in either the
	// file-I/O stack or the lifecycle dispatch stack). Used to detect circular
	// LoadScene-during-Awake recursion.
	bool IsCircularLoadDependency(const std::string& strCanonicalPath);

	// Bus-owned (after A1); true between the start of a SCENE_LOAD_SINGLE
	// teardown and the consolidated old->new ActiveSceneChanged dispatch.
	// Read by code that must skip intermediate active-scene-changed callbacks.
	bool IsActiveSceneSuppressed();

	// Scheduler-owned (after B1); top of the explicit creation-target stack.
	// Returns Zenith_Scene::INVALID_SCENE when no SceneCreationTargetScope is
	// active. Read by Zenith_SceneManager::GetDefaultCreationScene() to honour
	// Unity's contract that entities created during scene activation /
	// deserialization land in the loading scene rather than the active scene.
	Zenith_Scene GetCurrentCreationTarget();
}
