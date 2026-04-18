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

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_CommandList;

class Flux_FroxelFog
{
public:
	Flux_FroxelFog() = delete;
	~Flux_FroxelFog() = delete;

	static void Initialise();
	static void Reset();

	// Called from Flux_Fog::SetupRenderGraph to create transients and set state.
	static void SetupTransients(Flux_RenderGraph& xGraph);

	// Individual pass functions for render graph integration
	static void RenderInject(Flux_CommandList* pxCommandList);
	static void RenderLight(Flux_CommandList* pxCommandList);
	static void RenderApply(Flux_CommandList* pxCommandList);

	// Access froxel grid (routes through transient or owned path automatically)
	static struct Flux_RenderAttachment& GetDensityGrid();
	static struct Flux_RenderAttachment& GetLightingGrid();
	static struct Flux_RenderAttachment& GetScatteringGrid();

	// Transient handles for render graph declaration in Flux_Fog.cpp
	static Flux_TransientHandle GetDensityGridHandle();
	static Flux_TransientHandle GetLightingGridHandle();
	static Flux_TransientHandle GetScatteringGridHandle();

	// Debug slice visualization
	static struct Flux_RenderAttachment& GetDebugSliceTexture();

	// Get froxel depth range (for depth linearization)
	static float GetNearZ();
	static float GetFarZ();
};
