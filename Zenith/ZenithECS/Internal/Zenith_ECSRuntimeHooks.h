#pragma once

// =============================================================================
// Zenith_ECSRuntimeHooks -- leaf-safe runtime hooks for the ECS core.
//
// The ECS core (Zenith/EntityComponent minus Components/) is a low-level leaf:
// it must not reach up into engine-side subsystems (Flux, Physics, AssetHandling)
// nor reach the g_xEngine singleton. A handful of ECS-internal operations are
// nonetheless leaf-UNSAFE: a SCENE_LOAD_SINGLE teardown must reset the renderer,
// unload unused assets, and reset physics; thread-affinity asserts must know
// whether the caller is on the main thread. Those concrete actions live in
// engine-side subsystems, so the ECS cannot call them directly without breaking
// the leaf boundary.
//
// This struct decouples that: the engine installs captureless function pointers
// (via Zenith_SceneSystem::SetRuntimeHooks) that forward to the concrete
// subsystems, and the ECS core invokes only these pointers. The pointers are
// captureless (plain function pointers, NO std::function) so the struct is a POD
// and the hooks are zero-overhead. Every pointer defaults to nullptr; the
// documented null-semantics below make an un-installed hook a safe no-op (or a
// permissive "true" for the main-thread predicate), so the ECS behaves
// identically whether or not the engine has wired the hooks yet.
// =============================================================================

class Zenith_Entity;

struct Zenith_ECSRuntimeHooks
{
	bool (*m_pfnIsMainThread)() = nullptr;             // null => treat as true
	void (*m_pfnResetRenderSystems)() = nullptr;       // null => no-op
	void (*m_pfnUnloadUnusedAssets)() = nullptr;       // null => no-op
	void (*m_pfnResetPhysics)() = nullptr;             // null => no-op
	void (*m_pfnAddDefaultComponents)(Zenith_Entity&) = nullptr; // null => no default components added; Phase 3 wires this (engine adds its default component(s))
	// Fired at LoadScene completion (file-backed loads, after the new scene's
	// Awake/OnEnable dispatch, while re-entrant loads still defer). The engine
	// wires this to the behaviour-graph "__SceneLoaded" broadcast. null => no-op.
	void (*m_pfnSceneLoaded)(const char* szCanonicalPath, int iBuildIndex) = nullptr;
};
