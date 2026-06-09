#pragma once

/*
 * Zenith_GameRenderFeatures
 *
 * Generic game-side extension point into the Flux render graph. Replaces the
 * single hardcoded Zenith_GameRenderHook::RegisterPostFogPass callback with a
 * full render-FEATURE model: a game registers a feature with a lifecycle
 * (Initialise / SetupRenderGraph / Shutdown) plus an ANCHOR — the name of an
 * engine setup step its passes should be declared right after. The engine's
 * setup walk (Flux_FeatureRegistry::RunSetup) fires each engine step, then
 * invokes every game feature anchored after it, so a game feature lands at the
 * exact declaration index it needs (and therefore the correct same-resource
 * write-chain position) without any hardcoded stage.
 *
 * Positioning primitives mirror the engine's own:
 *   - m_szRunAfter names an engine setup step (see Flux_FeatureRegistry's
 *     setup-step names, e.g. "Fog", "SSAO"). v1 resolves engine steps only;
 *     to order relative to ANOTHER game feature, share the same anchor (ties
 *     break on registration order) or use DependsOn(xGraph.FindPass(...)).
 *   - Reads/Writes/DependsOn on the passes you AddPass during SetupRenderGraph.
 *
 * Generic disable: a feature can force-disable any engine pass (by name) or any
 * engine feature group (by owner — the setup-step name) via the render graph's
 * SetOwnerForceDisabled / SetPassForceDisabled overlay, which masks passes
 * WITHOUT mutating their base enable bit (non-destructive; lifting the override
 * restores the engine's own state).
 *
 * Lifetime: m_szName, m_szRunAfter and every name passed to the graph must be
 * STATIC-lifetime strings (string literals) — only the pointers are stored.
 *
 * Threading: main-thread only (Register/Unregister run during project init or
 * behaviour OnAwake; the engine-internal calls run on the main thread inside the
 * Flux setup / lifecycle walk).
 */

class Flux_RenderGraph;

// One game render feature. Captureless free-function trampolines only (no
// std::function). m_pfnInitialise / m_pfnShutdown are nullable; m_szName and
// m_pfnSetupRenderGraph are required. m_szRunAfter names the engine setup step
// this feature's passes are declared after (nullptr = not anchored; passes land
// at the end of the walk's tail in registration order).
struct Zenith_GameRenderFeatureDesc
{
	const char* m_szName                              = nullptr;
	void      (*m_pfnInitialise)()                    = nullptr;
	void      (*m_pfnSetupRenderGraph)(Flux_RenderGraph&) = nullptr;
	void      (*m_pfnShutdown)()                      = nullptr;
	const char* m_szRunAfter                          = nullptr;
};

namespace Zenith_GameRenderFeatures
{
	// Register a game render feature. Idempotent BY NAME: re-registering the same
	// name with an IDENTICAL desc (same callbacks + same runAfter) is a no-op;
	// the same name with DIFFERENT callbacks/runAfter trips Zenith_Check (no
	// silent replacement — Unregister first). If Flux is already initialised when
	// this is called (the common late-registration path), the feature's Initialise
	// runs immediately and a graph rebuild is requested so its setup runs before
	// the next frame. If Flux isn't up yet, InitialiseAllPending() runs it later.
	void Register(const Zenith_GameRenderFeatureDesc& xDesc);

	// Remove a previously registered feature by name. If it was initialised, its
	// Shutdown runs first. Order-preserving erase (relative registration order of
	// the remaining features is load-bearing). Requests a graph rebuild if Flux is up.
	void Unregister(const char* szName);

	// Drop EVERY registration (Shutdown initialised features in reverse order,
	// then clear). For test resets / hard project teardown.
	void ResetAll();

	// --- Engine-internal (driven by Flux) ----------------------------------
	// Fire every feature anchored on szStepName, in registration order, tagging
	// each feature's passes with its name as the graph owner.
	void InvokeFeaturesAnchoredAfter(const char* szStepName, Flux_RenderGraph& xGraph);
	// Initialise every not-yet-initialised feature (called from Flux LateInitialise
	// after the engine feature-init loop, before the first SetupRenderGraph).
	void InitialiseAllPending();
	// Shutdown every initialised feature in reverse registration order (called from
	// Flux Shutdown after the render graph is destroyed). Keeps the registrations.
	void ShutdownAll();

#ifdef ZENITH_RUNTIME_CHECKS
	// Assert every feature's m_szRunAfter resolves to a real engine setup step.
	// Analogue of Flux_FeatureRegistry::VerifyInitDependencies; called once at the
	// top of RunSetup.
	void VerifyGameFeatureAnchors();
#endif
}
