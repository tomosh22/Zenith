#pragma once

/*
 * Flux_GodRaysFog - Screen-Space God Rays (Light Shafts)
 *
 * Technique: Screen-space radial blur from light source position
 *
 * Pipeline:
 *   1. Render Pass (fragment): Radial blur sampling toward light with depth occlusion
 *
 * Resources:
 *   - Depth buffer for occlusion testing
 *   - Frame constants for sun direction/position
 *
 * Debug Modes: 21-23 (light mask, occlusion, radial weights)
 *
 * Performance: <1ms at 1080p
 *
 * References:
 *   - GPU Gems 3: Volumetric Light Scattering
 *   - Andrew Gotow's Screen-space Volumetric Shadowing
 */

class Flux_GodRaysFog
{
public:
	static void Initialise();
	static void Reset();
	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();
};
