#pragma once

// =============================================================================
// Zenith_SceneSystemGuards.h
// -----------------------------------------------------------------------------
// Top-level RAII scope-guard types for the scene system. Each guard wraps a
// state flag on Zenith_SceneLifecycleScheduler with save-on-construct /
// restore-on-destruct semantics so nested guards stack correctly.
//
// These were previously nested types of `class Zenith_SceneManager`. Phase 5e
// moved them out of the manager class because the manager class is being
// deleted entirely; the guards' state lives on Zenith_SceneLifecycleScheduler
// so making them top-level types matches the post-refactor ownership.
//
// Definitions live in Zenith_SceneSystemGuards.cpp.
// =============================================================================

#include "EntityComponent/Zenith_Scene.h"

/**
 * RAII guard for lifecycle-deferral flags. Save/restore semantics: stores the
 * flag's prior value on construction and restores it on destruction, so nested
 * guards stack correctly (scene loading nested inside a deferral, prefab
 * instantiation inside a load, etc.).
 *
 * Usage: Zenith_LifecycleDeferralGuard xGuard(g_xEngine.SceneLifecycle().m_bIsLoadingScene);
 */
struct Zenith_LifecycleDeferralGuard
{
	bool& m_bFlag;
	bool  m_bPrevValue;
	Zenith_LifecycleDeferralGuard(bool& bFlag) : m_bFlag(bFlag), m_bPrevValue(bFlag) { m_bFlag = true; }
	~Zenith_LifecycleDeferralGuard() { m_bFlag = m_bPrevValue; }
	Zenith_LifecycleDeferralGuard(const Zenith_LifecycleDeferralGuard&) = delete;
	Zenith_LifecycleDeferralGuard& operator=(const Zenith_LifecycleDeferralGuard&) = delete;
};

/**
 * RAII guard for prefab instantiation. Sets m_bIsPrefabInstantiating=true on
 * construction and restores the prior value on destruction. Supports nesting
 * (a prefab whose components spawn child prefabs).
 *
 * Only writer of g_xEngine.SceneLifecycle().m_bIsPrefabInstantiating in production code.
 */
struct Zenith_PrefabInstantiationGuard
{
	Zenith_PrefabInstantiationGuard();
	~Zenith_PrefabInstantiationGuard();
	Zenith_PrefabInstantiationGuard(const Zenith_PrefabInstantiationGuard&) = delete;
	Zenith_PrefabInstantiationGuard& operator=(const Zenith_PrefabInstantiationGuard&) = delete;
private:
	bool m_bPrevValue;
};

/**
 * RAII guard for the engine's frame-update phase. Sets m_bIsUpdating=true on
 * construction and restores the prior value on destruction. Used by
 * Zenith_Core to mark the UI-update span where script-driven scene loads
 * must defer to the next frame.
 *
 * Only writer of g_xEngine.SceneLifecycle().m_bIsUpdating in production code.
 */
struct Zenith_SceneUpdateDeferralGuard
{
	Zenith_SceneUpdateDeferralGuard();
	~Zenith_SceneUpdateDeferralGuard();
	Zenith_SceneUpdateDeferralGuard(const Zenith_SceneUpdateDeferralGuard&) = delete;
	Zenith_SceneUpdateDeferralGuard& operator=(const Zenith_SceneUpdateDeferralGuard&) = delete;
private:
	bool m_bPrevValue;
};

/**
 * RAII guard that pushes a scene onto the explicit creation-target stack on
 * Zenith_SceneLifecycleScheduler. While in scope,
 * g_xEngine.SceneLifecycle().GetDefaultCreationScene() returns this scene
 * instead of the active scene. Stack-based — nested scopes push/pop correctly.
 */
struct Zenith_SceneCreationTargetScope
{
	explicit Zenith_SceneCreationTargetScope(Zenith_Scene xScene);
	~Zenith_SceneCreationTargetScope();
	Zenith_SceneCreationTargetScope(const Zenith_SceneCreationTargetScope&) = delete;
	Zenith_SceneCreationTargetScope& operator=(const Zenith_SceneCreationTargetScope&) = delete;
};
