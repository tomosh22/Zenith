#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Flux_LightClustering — clustered deferred lighting compute pass.
//
// Builds the per-cluster light-index lists each frame. For each cluster
// (16x9x24 = 3456 total, exponential-Z), the compute shader walks the
// dynamic light list (uploaded by Flux_DynamicLights), tests sphere-vs-
// cluster-AABB, and writes the matching indices into a compact
// per-cluster array. The deferred shading fragment shader then reads
// only the lights that affect each pixel — eliminating the overdraw
// and redundant G-buffer sampling that the old per-light-volume
// rasterisation suffered from.
//
// Also owns the gather-step Prepare callback: GatherLightsFromScene()
// runs on the main thread before the compute dispatch.
//
// Resources:
//   ClusterLightCounts  : RWStructuredBuffer<uint> [3456]
//   ClusterLightIndices : RWStructuredBuffer<uint> [3456 * 64]
// Both are GPU-resident (Flux_ReadWriteBuffer) since the compute
// shader writes them and the fragment shader reads them — no CPU
// involvement, so host-visible memory would just waste bandwidth.
class Flux_LightClustering
{
public:
	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();

	// Registers the clustering pass with the graph. Must run before
	// Flux_DeferredShading::SetupRenderGraph (the deferred pass declares
	// reads on the cluster buffers, so the graph would order it correctly
	// either way, but registering in this order keeps the source readable).
	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Read-only access for downstream consumers (Flux_DeferredShading
	// binds these as SRVs in its fragment shader).
	static Flux_ShaderResourceView_Buffer& GetClusterLightCountsSRV();
	static Flux_ShaderResourceView_Buffer& GetClusterLightIndicesSRV();

	// Underlying buffer references for render-graph access declarations
	// (.ReadsBuffer / .WritesBuffer in pass setup).
	static Flux_ReadWriteBuffer& GetClusterLightCountsBuffer();
	static Flux_ReadWriteBuffer& GetClusterLightIndicesBuffer();

	// Cluster grid dimensions — must match Common.Lighting.slang.
	static constexpr u_int uCLUSTER_DIM_X      = 16;
	static constexpr u_int uCLUSTER_DIM_Y      = 9;
	static constexpr u_int uCLUSTER_DIM_Z      = 24;
	static constexpr u_int uCLUSTER_COUNT      = uCLUSTER_DIM_X * uCLUSTER_DIM_Y * uCLUSTER_DIM_Z;
	static constexpr u_int uMAX_LIGHTS_PER_CLUSTER = 64;

	static bool IsInitialised() { return s_bInitialised; }

private:
	static bool s_bInitialised;
};
