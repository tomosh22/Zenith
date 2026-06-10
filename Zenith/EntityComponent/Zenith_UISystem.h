#pragma once

// ============================================================================
// Zenith_UISystem -- engine-side per-frame UI orchestrator.
//
// Owns the main loop's UI step: updating every Zenith_UIComponent across all
// loaded scenes, then re-collecting and rendering them (which submits to
// Flux_Quads / Flux_Text). Lives in EntityComponent/ -- NOT UI/ -- because it
// names the concrete Zenith_UIComponent and walks scenes via
// Zenith_SceneSystem; UI/ sits below EntityComponent/ in the layer DAG
// (Zenith_UIComponent.h includes UI/Zenith_UI.h), so hosting this there would
// create a module cycle. Same engine-side-glue placement as
// Zenith_CameraResolve.
//
// Accessed via g_xEngine.UI(); allocated and wired by Zenith_Engine.
// ============================================================================

class Zenith_SceneSystem;

class Zenith_UISystem
{
public:
	// Dependency-injection seam (Physics::Initialise(Zenith_Input&) pattern):
	// Zenith_Engine passes the scene system in so this TU never names the
	// engine singleton.
	void Initialise(Zenith_SceneSystem& xScenes);

	// The whole per-frame UI step. The Update pass and the Render pass live
	// in ONE entry point because the boundary between them is load-bearing
	// (a deferred LoadScene queued by a button click drains between the
	// passes) -- callers must not be able to split or reorder them.
	void Update(float fDt);

private:
	Zenith_SceneSystem* m_pxScenes = nullptr;
};
