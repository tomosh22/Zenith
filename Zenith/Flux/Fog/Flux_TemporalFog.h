#pragma once

/*
 * Flux_TemporalFog - Temporal Reprojection for Volumetric Fog
 *
 * Technique: Accumulates fog samples over multiple frames using reprojection
 *
 * Pipeline:
 *   1. Apply sub-voxel jitter to fog sampling (integrated into froxel/raymarch)
 *   2. Resolve Pass (compute): Reproject history, blend with current frame
 *
 * Resources:
 *   - s_xHistoryBuffer[2] (ping-pong 3D RGBA16F)
 *   - Motion vectors from frame constants
 *
 * Debug Modes: 17-20 (motion vectors, history weight, jitter, disocclusion)
 *
 * Performance: +0.5ms overhead on top of base technique
 *
 * Works as enhancement to Froxel technique, not standalone
 *
 * References:
 *   - Temporal Reprojection Anti-Aliasing (TRAA)
 *   - Assassin's Creed Unity volumetric lighting
 */

class Flux_TemporalFog
{
public:
	Flux_TemporalFog() = delete;
	~Flux_TemporalFog() = delete;

	static void Initialise();
	static void Reset();

	// Submit temporal resolve task
	static void SubmitResolveTask();
	static void WaitForResolveTask();

	// Main render function
	static void Render(void* pData = nullptr);

	// Apply temporal jitter to fog sampling
	// Returns jitter offset in voxel space
	static Zenith_Maths::Vector2 GetTemporalJitter();

	// Get blended output for final application
	static struct Flux_RenderAttachment& GetResolvedOutput();

	// Access history for debug
	static struct Flux_RenderAttachment& GetHistoryBuffer();
	static struct Flux_RenderAttachment& GetDebugMotionVectors();
};
