#pragma once

/*
 * Flux_FroxelFog - Froxel-based Volumetric Fog
 *
 * Technique: Camera-aligned 3D grid (froxels) storing density and lighting
 *
 * Pipeline:
 *   1. Inject Pass (compute): Sample density from height/noise into froxel grid
 *   2. Light Pass (compute): Accumulate lighting per froxel from sun + point lights
 *   3. Apply Pass (fragment): Ray march through froxels, accumulate and blend
 *
 * Resources:
 *   - s_xFroxelDensityGrid (3D RGBA16F, 160x90x64)
 *   - s_xFroxelLightingGrid (3D RGBA16F, 160x90x64)
 *
 * Debug Modes: 3-8 (density slice, max proj, lighting, scattering, extinction, shadows)
 *
 * Performance: 1-3ms at 1080p depending on grid resolution
 *
 * References:
 *   - Unreal Engine volumetric fog
 *   - Bart Wronski SIGGRAPH 2014
 */

class Flux_FroxelFog
{
public:
	Flux_FroxelFog() = delete;
	~Flux_FroxelFog() = delete;

	static void Initialise();
	static void Reset();

	// Submit render tasks for the three passes
	static void SubmitInjectTask();
	static void SubmitLightTask();
	static void SubmitApplyTask();

	// Wait for tasks to complete
	static void WaitForInjectTask();
	static void WaitForLightTask();
	static void WaitForApplyTask();

	// Main render function (calls all passes)
	static void Render(void* pData = nullptr);

	// Access froxel grid for debug visualization
	static struct Flux_RenderAttachment& GetDensityGrid();
	static struct Flux_RenderAttachment& GetLightingGrid();

	// Debug slice visualization
	static struct Flux_RenderAttachment& GetDebugSliceTexture();

	// Get froxel depth range (for temporal fog)
	static float GetNearZ();
	static float GetFarZ();
};
