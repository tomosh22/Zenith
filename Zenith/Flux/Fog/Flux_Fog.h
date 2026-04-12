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
	static bool s_bEnabled;

	static void Initialise();

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

private:
	static void ExecuteSimpleFog(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelInject(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelLight(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteFroxelApply(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteRaymarch(Flux_CommandList* pxCommandList, void* pUserData);
	static void ExecuteGodRays(Flux_CommandList* pxCommandList, void* pUserData);
};