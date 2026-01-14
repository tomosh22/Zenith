#pragma once

/*
 * Flux_LPVFog - Light Propagation Volumes for Volumetric Fog
 *
 * Technique: Inject virtual point lights (VPLs) into a 3D grid and propagate
 *            light iteratively to simulate multiple scattering
 *
 * Pipeline:
 *   1. Inject Pass (compute): Place VPLs from shadow map into LPV grid
 *   2. Propagate Pass (compute): Iteratively spread light to neighbors (N iterations)
 *   3. Apply Pass (fragment): Sample LPV and apply to fog
 *
 * Resources:
 *   - s_xLPVGrid[2] (ping-pong 3D RGBA16F, 32^3 per cascade)
 *   - 3 cascades for different distance ranges
 *   - SH coefficients for directional light storage (simplified to RGB for now)
 *
 * Debug Modes: 13-16 (injection points, propagation iter, cascade bounds, SH coefficients)
 *
 * Performance: 3-5ms at 1080p depending on cascade count and propagation iterations
 *
 * References:
 *   - Crytek Light Propagation Volumes (Kaplanyan, Dachsbacher)
 *   - Real-time Global Illumination by Propagating Surfaces
 */

class Flux_LPVFog
{
public:
	Flux_LPVFog() = delete;
	~Flux_LPVFog() = delete;

	static void Initialise();
	static void Reset();

	// Submit render tasks
	static void SubmitInjectTask();
	static void SubmitPropagateTask();
	static void SubmitApplyTask();

	// Wait for tasks
	static void WaitForInjectTask();
	static void WaitForPropagateTask();
	static void WaitForApplyTask();

	// Main render function (calls all passes)
	static void Render(void* pData = nullptr);

	// Access LPV grids for debug visualization
	static struct Flux_RenderAttachment& GetLPVGrid(u_int uCascade);
	static struct Flux_RenderAttachment& GetDebugInjectionPoints();
};
