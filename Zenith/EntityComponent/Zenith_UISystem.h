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

class Zenith_InputActions;
class Zenith_Pointers;
class Zenith_SceneSystem;

class Zenith_UISystem
{
public:
	// Dependency-injection seam (Physics::Initialise(Zenith_Input&) pattern):
	// Zenith_Engine passes the scene system in so this TU never names the
	// engine singleton.
	void Initialise(Zenith_SceneSystem& xScenes);

	// Frame contract step 10: the UI INPUT phase, run before game logic and
	// entirely separately from Update() below.
	//
	// It is its own phase for two reasons. The visual pass is skipped whenever
	// the frame submits no render work (scene transitions), and input that only
	// happens on rendered frames is input that silently disappears; and gameplay
	// must not act on a press a widget is about to consume, which can only be
	// decided before game logic runs.
	//
	// The three sub-steps, in the order the frame contract fixes:
	//   10a/10b: refresh each canvas's layout, then close the engine-RESERVED UI
	//            actions and let the canvases consume them for focus navigation
	//            — BEFORE any widget has taken a claim, which is what makes UI
	//            navigation claim-independent.
	//   10c:     the standard-control capture walk (claims are taken here).
	//   10d:     the VIRTUAL producers — B9's on-screen controls claim and
	//            publish, after 10c so a standard widget wins a contested
	//            pointer and before 10e so their transitions replay this frame.
	//   10e:     close every remaining action, with pointer-sourced bindings
	//            suppressed on any pointer a widget just claimed.
	//
	// Takes the action layer and the pointer table by reference rather than
	// reaching for g_xEngine, keeping this TU (and every widget it drives) off
	// the engine singleton.
	void UpdateInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt);

	// The whole per-frame UI step. The Update pass and the Render pass live
	// in ONE entry point because the boundary between them is load-bearing
	// (a deferred LoadScene queued by a button click drains between the
	// passes) -- callers must not be able to split or reorder them.
	void Update(float fDt);

private:
	Zenith_SceneSystem* m_pxScenes = nullptr;
};
