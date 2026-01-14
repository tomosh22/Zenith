#pragma once

/*
 * Flux_RaymarchFog - Ray Marching Volumetric Fog
 *
 * Technique: Per-pixel ray marching through 3D noise for density
 *
 * Pipeline:
 *   1. Render Pass (fragment): March along view ray, sample noise texture,
 *      accumulate scattering using Beer-Lambert law
 *
 * Resources:
 *   - 3D noise texture (Perlin-Worley) from Flux_VolumeFog
 *   - Blue noise texture for temporal jitter
 *   - Depth buffer for ray termination
 *
 * Debug Modes: 9-12 (step count, accumulated density, noise sample, jitter pattern)
 *
 * Performance: 2-4ms at 1080p depending on step count
 *
 * References:
 *   - Horizon Zero Dawn volumetric clouds
 *   - Maxime Heckel's raymarching tutorial
 */

class Flux_RaymarchFog
{
public:
	static void Initialise();
	static void Reset();
	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();
};
