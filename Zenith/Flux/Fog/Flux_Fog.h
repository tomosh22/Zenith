#pragma once

/*
 * Flux_Fog - Volumetric Fog Orchestrator
 *
 * Manages multiple volumetric fog rendering techniques with runtime switching.
 * Technique selection via debug variable: Render/Volumetric Fog/Technique
 *
 * Available Techniques:
 *   0 - Simple exponential fog (original)
 *   1 - Froxel-based volumetric fog
 *   2 - Ray marching with noise
 *   3 - Light Propagation Volumes (LPV)
 *   4 - Froxel + Temporal reprojection
 *   5 - Screen-space god rays
 *
 * See Fog/CLAUDE.md for full documentation.
 */

class Flux_Fog
{
public:
	static void Initialise();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

private:
	static void RenderSimpleFog();  // Original simple exponential fog
};