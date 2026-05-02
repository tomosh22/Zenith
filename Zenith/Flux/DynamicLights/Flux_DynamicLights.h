#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Flux_DynamicLights — gather + upload front-end for clustered deferred
// lighting. The actual rendering happens inside Flux_DeferredShading
// (per-pixel cluster loop) using the per-cluster light index lists
// built by Flux_LightClustering.
//
// Each frame:
//   1. GatherLightsFromScene() walks the ECS, frustum-culls, priority-
//      sorts if over the cap, and uploads a single GPU buffer of
//      LightInstance structs (point + spot + directional unified).
//   2. Flux_LightClustering reads the buffer + count and builds the
//      per-cluster index lists.
//   3. Flux_DeferredShading reads the lists and evaluates per pixel.
//
// Replaces the old per-light-volume rasterisation path (which was
// GPU-bound from disabled depth testing + redundant G-buffer sampling
// across overlapping volumes).
class Flux_DynamicLights
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();

	// Called each frame to gather lights from the scene and upload them.
	// Invoked from Flux_LightClustering's render-graph pass execute callback.
	static void GatherLightsFromScene();

	// Read-only access to the unified light buffer for downstream passes.
	// Used by Flux_LightClustering (compute) and Flux_DeferredShading
	// (fragment) — both bind via BindSRV_Buffer.
	static Flux_ShaderResourceView_Buffer& GetLightBufferSRV();
	// Returns a mutable reference because the render-graph API
	// (ReadBuffer / WritesBuffer / MarkBufferHostWritten) takes the
	// buffer non-const so it can mutate the graph's per-buffer
	// tracking state. The buffer's contents are still managed solely
	// by GatherLightsFromScene().
	static Flux_DynamicReadWriteBuffer& GetLightBuffer();
	static u_int GetLightCount();

	static bool IsInitialised() { return s_bInitialised; }

	static constexpr u_int uMAX_LIGHTS = 256;

private:
	static bool s_bInitialised;
};
