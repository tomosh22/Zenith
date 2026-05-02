// =============================================================================
// Zenith_SceneManagerGuards.h
// -----------------------------------------------------------------------------
// Nested RAII scope-guard types for Zenith_SceneManager. This header is meant
// to be included *inside* the body of `class Zenith_SceneManager` from
// Zenith_SceneManager.h — it does NOT have its own #pragma once because the
// types it declares are intentionally nested members of that class so existing
// call sites like `Zenith_SceneManager::PrefabInstantiationGuard` continue to
// resolve unchanged.
//
// Each guard wraps a state flag in Zenith_SceneLifecycleScheduler with
// save-on-construct / restore-on-destruct semantics. Definitions live in
// Zenith_SceneManager.cpp (except LifecycleDeferralGuard which is small enough
// to inline here).
// =============================================================================

/**
 * RAII guard for lifecycle-deferral flags. Save/restore semantics: stores the
 * flag's prior value on construction and restores it on destruction, so nested
 * guards stack correctly (scene loading nested inside a deferral, prefab
 * instantiation inside a load, etc.).
 * Usage: Zenith_SceneManager::LifecycleDeferralGuard xGuard(s_bIsLoadingScene);
 */
struct LifecycleDeferralGuard
{
	bool& m_bFlag;
	bool  m_bPrevValue;
	LifecycleDeferralGuard(bool& bFlag) : m_bFlag(bFlag), m_bPrevValue(bFlag) { m_bFlag = true; }
	~LifecycleDeferralGuard() { m_bFlag = m_bPrevValue; }
	LifecycleDeferralGuard(const LifecycleDeferralGuard&) = delete;
	LifecycleDeferralGuard& operator=(const LifecycleDeferralGuard&) = delete;
};

/**
 * RAII guard for prefab instantiation. Sets IsPrefabInstantiating=true on
 * construction and restores the prior value on destruction. Save/restore
 * semantics support nested prefab instantiation (a prefab whose components
 * spawn child prefabs).
 *
 * After A6.3, this is the only writer of Zenith_SceneLifecycleScheduler::
 * s_bIsPrefabInstantiating in production code.
 */
struct PrefabInstantiationGuard
{
	PrefabInstantiationGuard();
	~PrefabInstantiationGuard();
	PrefabInstantiationGuard(const PrefabInstantiationGuard&) = delete;
	PrefabInstantiationGuard& operator=(const PrefabInstantiationGuard&) = delete;
private:
	bool m_bPrevValue;
};

/**
 * RAII guard for the engine's frame-update phase. Sets IsUpdating=true on
 * construction and restores the prior value on destruction. Used by
 * Zenith_Core to mark the UI-update span where script-driven scene loads
 * must defer to the next frame.
 *
 * After A6.3, this is the only writer of Zenith_SceneLifecycleScheduler::
 * s_bIsUpdating in production code.
 */
struct SceneUpdateDeferralGuard
{
	SceneUpdateDeferralGuard();
	~SceneUpdateDeferralGuard();
	SceneUpdateDeferralGuard(const SceneUpdateDeferralGuard&) = delete;
	SceneUpdateDeferralGuard& operator=(const SceneUpdateDeferralGuard&) = delete;
private:
	bool m_bPrevValue;
};

/**
 * RAII guard that pushes a scene onto the explicit creation-target stack.
 * While in scope, GetDefaultCreationScene() returns this scene (or the
 * top-most nested scope) instead of the active scene. Wraps the load /
 * deserialization / activation paths so entities created as a side effect
 * of those operations land in the scene being materialized rather than
 * leaking into whatever scene happened to be active when the load began.
 *
 * Stack-based, so nested loads (a SceneLoaded callback that itself loads
 * another scene) push/pop correctly without losing the outer target.
 */
struct SceneCreationTargetScope
{
	explicit SceneCreationTargetScope(Zenith_Scene xScene);
	~SceneCreationTargetScope();
	SceneCreationTargetScope(const SceneCreationTargetScope&) = delete;
	SceneCreationTargetScope& operator=(const SceneCreationTargetScope&) = delete;
};
