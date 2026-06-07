#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// =====================================================================
// Flux_LightClustering — implementation.
//
// One thread per cluster, [numthreads(16,9,1)] dispatched as (1,1,24)
// — covers all 3456 clusters with one workgroup per Z slice. Each
// thread reconstructs its cluster's view-space AABB from the inverse
// projection, then iterates every light, transforming the position
// into view space and testing sphere-vs-AABB.
//
// Clustering must run BEFORE deferred shading. Ordering between the
// two passes is enforced by graph-tracked .WritesBuffer/.ReadsBuffer
// declarations on the cluster-output buffers (counts + indices). The
// LightBuffer is intentionally NOT graph-tracked — see PrepareLightClustering
// for the rationale.
// =====================================================================



// Per-cluster outputs. GPU-resident — written by compute, read by
// fragment, never touched by CPU.

// Push constants — light count for the iteration loop. The shader's
// inner loop bound depends on a runtime value, so it goes through a CB.
struct LightClusteringPushConstants
{
	u_int m_uLightCount;
	u_int m_uPad0;
	u_int m_uPad1;
	u_int m_uPad2;
};

void Flux_LightClusteringImpl::BuildPipelines()
{
	m_xComputeShader.Initialise(FluxShaderProgram::LightClustering);
	Flux_RootSigBuilder::FromReflection(m_xComputeRootSig, m_xComputeShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xComputePipeline, m_xComputeShader, m_xComputeRootSig);
}

void Flux_LightClusteringImpl::Initialise(Zenith_Vulkan_MemoryManager& xVulkanMemory)
{
	m_pxVulkanMemory = &xVulkanMemory;

	// Cluster light counts: 3456 * 4 bytes ≈ 14 KB.
	const u_int64 ulCountBufferSize = uCLUSTER_COUNT * sizeof(u_int);
	// Cluster light indices: 3456 * 64 * 4 bytes ≈ 884 KB.
	const u_int64 ulIndexBufferSize = uCLUSTER_COUNT * uMAX_LIGHTS_PER_CLUSTER * sizeof(u_int);

	// Zero-initialise both. Frame 0 reads will see counts = 0 and the
	// cluster loop in the fragment shader will run zero iterations.
	Zenith_Vector<u_int> xZeroedCounts(uCLUSTER_COUNT);
	for (u_int u = 0; u < uCLUSTER_COUNT; ++u) xZeroedCounts.PushBack(0);
	m_pxVulkanMemory->InitialiseReadWriteBuffer(xZeroedCounts.GetDataPointer(), ulCountBufferSize, m_xClusterLightCounts);

	Zenith_Vector<u_int> xZeroedIndices(uCLUSTER_COUNT * uMAX_LIGHTS_PER_CLUSTER);
	for (u_int u = 0; u < uCLUSTER_COUNT * uMAX_LIGHTS_PER_CLUSTER; ++u) xZeroedIndices.PushBack(0);
	m_pxVulkanMemory->InitialiseReadWriteBuffer(xZeroedIndices.GetDataPointer(), ulIndexBufferSize, m_xClusterLightIndices);

	BuildPipelines();

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::LightClustering,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.LightClustering().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	m_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_LightClustering initialised (%ux%ux%u clusters, %u max lights/cluster)",
		uCLUSTER_DIM_X, uCLUSTER_DIM_Y, uCLUSTER_DIM_Z, uMAX_LIGHTS_PER_CLUSTER);
}

void Flux_LightClusteringImpl::Shutdown()
{
	if (!m_bInitialised) return;

	m_pxVulkanMemory->DestroyReadWriteBuffer(m_xClusterLightCounts);
	m_pxVulkanMemory->DestroyReadWriteBuffer(m_xClusterLightIndices);

	m_pxVulkanMemory = nullptr;
	m_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_LightClustering shut down");
}

// Prepare callback — runs on the main thread before the GPU pass
// executes. Walks the ECS and uploads the unified light buffer.
//
// The light buffer is intentionally NOT declared as a graph-tracked
// resource (no .ReadsBuffer / no MarkBufferHostWritten) because it's a
// frame-indexed Flux_DynamicReadWriteBuffer — its GetBuffer() returns
// a different physical pointer per frame, so any pointer captured at
// SetupRenderGraph time would refer to the wrong buffer in subsequent
// frames. Visibility of the host upload is covered by vkQueueSubmit's
// implicit host-write barrier, and frame indexing prevents cross-frame
// races. Same rationale as the terrain chunk-data buffer (see
// Zenith_TerrainComponent.cpp's MarkBufferHostWritten note).
static void PrepareLightClustering(void* /*pUserData*/)
{
	// Trampoline (non-capturing graph callback): recover the subsystem singleton
	// up front, then route through it. GatherLightsFromScene lives on the sibling
	// DynamicLights subsystem, which is reached directly here.
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (!xLightClustering.IsInitialised()) return;
	g_xEngine.DynamicLights().GatherLightsFromScene();
}

static void ExecuteLightClustering(Flux_CommandList* pxCommandList, void* /*pUserData*/)
{
	// Trampoline (non-capturing graph callback): recover the subsystem singleton
	// up front, then route every LightClustering reach through it. DynamicLights
	// and FluxGraphics are sibling subsystems, reached directly here.
	Flux_LightClusteringImpl& xLightClustering = g_xEngine.LightClustering();
	if (!xLightClustering.IsInitialised()) return;

	// Always dispatch — even when no lights exist or dynamic lights are
	// disabled. The shader writes count = 0 to every cluster, ensuring
	// the cluster-count buffer has fresh zeroes (no stale carry-over
	// from a previous frame). Skipping the dispatch when the count is
	// zero would leave the buffer dirty.
	const u_int uLightCount = g_xEngine.DynamicLights().GetLightCount();

	pxCommandList->AddCommand<Flux_CommandBindComputePipeline>(&xLightClustering.m_xComputePipeline);

	Flux_ShaderBinder xBinder(*pxCommandList);

	// Inputs.
	xBinder.BindCBV(xLightClustering.m_xComputeShader, "FrameConstants",
		&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
	xBinder.BindSRV_Buffer(xLightClustering.m_xComputeShader, "LightBuffer",
		g_xEngine.DynamicLights().GetLightBufferSRV());

	// Outputs (UAVs).
	xBinder.BindUAV_Buffer(xLightClustering.m_xComputeShader, "ClusterLightCounts",
		&xLightClustering.m_xClusterLightCounts.GetUAV());
	xBinder.BindUAV_Buffer(xLightClustering.m_xComputeShader, "ClusterLightIndices",
		&xLightClustering.m_xClusterLightIndices.GetUAV());

	LightClusteringPushConstants xConstants;
	xConstants.m_uLightCount = uLightCount;
	xConstants.m_uPad0 = 0;
	xConstants.m_uPad1 = 0;
	xConstants.m_uPad2 = 0;
	xBinder.BindDrawConstants(xLightClustering.m_xComputeShader, "PushConstants",
		&xConstants, sizeof(LightClusteringPushConstants));

	// Total threads = 16 × 9 × 24 = 3456, one per cluster. The compute
	// kernel declares [numthreads(16, 9, 1)] (= 144 threads / workgroup,
	// covering one full XY plane of clusters). We dispatch CLUSTER_DIM_Z
	// = 24 workgroups along Z so each Z slice is its own workgroup.
	pxCommandList->AddCommand<Flux_CommandDispatch>(1, 1, Flux_LightClusteringImpl::uCLUSTER_DIM_Z);
}

void Flux_LightClusteringImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	if (!m_bInitialised) return;

	// Only the cluster-output buffers are graph-tracked. The LightBuffer
	// is host-uploaded each frame from PrepareLightClustering, and
	// declaring a frame-indexed buffer in the graph here would capture a
	// frame-0 pointer that's stale on every subsequent frame (its
	// GetBuffer() returns a different physical buffer per frame). See
	// the comment on PrepareLightClustering for the full rationale.
	Flux_Buffer& xCountsBuffer = m_xClusterLightCounts.GetBuffer();
	Flux_Buffer& xIndicesBuf   = m_xClusterLightIndices.GetBuffer();

	xGraph.AddPass("Light Clustering", ExecuteLightClustering)
		.Prepare(PrepareLightClustering)
		.WritesBuffer(xCountsBuffer, RESOURCE_ACCESS_WRITE_UAV)
		.WritesBuffer(xIndicesBuf,   RESOURCE_ACCESS_WRITE_UAV);
}

Flux_ShaderResourceView_Buffer& Flux_LightClusteringImpl::GetClusterLightCountsSRV()
{
	return m_xClusterLightCounts.GetSRV();
}

Flux_ShaderResourceView_Buffer& Flux_LightClusteringImpl::GetClusterLightIndicesSRV()
{
	return m_xClusterLightIndices.GetSRV();
}


