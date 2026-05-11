#pragma once

/*
 * Flux_Fog - Volumetric Fog Orchestrator
 *
 * Manages multiple volumetric fog rendering techniques with runtime switching.
 * Technique selection via debug variable: Render/Volumetric Fog/Technique
 *
 * All techniques are spatial-only (no temporal effects, history buffers, or reprojection).
 *
 * Available Techniques:
 *   0 - Simple exponential fog (original)
 *   1 - Froxel-based volumetric fog
 *   2 - Ray marching with noise
 *   3 - Screen-space god rays
 *
 * See Fog/CLAUDE.md for full documentation.
 */

class Flux_CommandList;
class Flux_RenderGraph;

class Flux_Fog
{
public:
	static void Initialise();
	static void BuildPipelines();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Apply the current debug-variable technique selection to the graph by
	// enabling the active technique's passes and disabling the others. Must be
	// called from Zenith_Core::ExecuteRenderGraph BEFORE Compile() so any
	// resulting MarkDirty() takes effect on the same frame. Cannot live as a
	// pass OnPrepare callback because Phase 0 only fires OnPrepare for *enabled*
	// passes — if the previously-active pass is disabled, an OnPrepare-based
	// switcher would never run again. SetPassEnabled may take the cheap
	// m_bEnabledMaskDirty path on the toggled passes themselves, but the fog
	// passes share resources (depth buffer, HDR scene target) with other passes,
	// so the barrier graph must be fully recomputed; this function calls
	// MarkDirty() to force a full Compile() on technique change.
	static void ApplyTechniqueSelectionToGraph(Flux_RenderGraph& xGraph);

	// Game-side override: when set to true, the entire engine fog system is
	// short-circuited — ApplyTechniqueSelectionToGraph early-returns and all 6
	// fog passes are explicitly disabled on the active graph. Survives graph
	// rebuilds because ReapplyOverrideToCurrentGraph is invoked at the end of
	// SetupRenderGraph whenever the flag is set.
	//
	// Used by DevilsPlayground (and any future game) that ships its own
	// atmospheric/fog pass via Zenith_GameRenderHook::RegisterPostFogPass.
	// Setting back to false re-engages normal technique selection on the
	// next frame (does NOT blanket-enable all 6 passes — it lets
	// ApplyTechniqueSelectionToGraph pick whichever subset matches the
	// current technique).
	static void SetExternallyOverridden(bool bOverridden);

	// Read-side query for the override flag. True iff a game-side post-fog
	// pass has called SetExternallyOverridden(true). Used by tests to verify
	// the post-fog hook actually fired during render-graph setup (the hook
	// runs INSIDE SetupRenderGraph, so a missed callback never sets the flag).
	static bool IsExternallyOverridden();

	// Internal helper invoked at the end of Flux_Fog::SetupRenderGraph so that
	// a graph rebuild while the override is active still leaves the fog
	// passes disabled. Game code should not call this directly.
	static void ReapplyOverrideToCurrentGraph();

private:
	static void ExecuteSimpleFog(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelInject(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelLight(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelApply(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteRaymarch(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteGodRays(Flux_CommandList* pxCommandList, void* pUserData);
};