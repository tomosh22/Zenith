#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 9: state + behaviour for LightClustering subsystem.
class Flux_LightClusteringImpl
{
public:
	Flux_LightClusteringImpl() = default;
	~Flux_LightClusteringImpl() = default;

	Flux_LightClusteringImpl(const Flux_LightClusteringImpl&) = delete;
	Flux_LightClusteringImpl& operator=(const Flux_LightClusteringImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_ShaderResourceView_Buffer& GetClusterLightCountsSRV();
	Flux_ShaderResourceView_Buffer& GetClusterLightIndicesSRV();
	Flux_ReadWriteBuffer& GetClusterLightCountsBuffer()  { return m_xClusterLightCounts; }
	Flux_ReadWriteBuffer& GetClusterLightIndicesBuffer() { return m_xClusterLightIndices; }

	bool IsInitialised() const { return m_bInitialised; }

	// Cluster grid dimensions — must match Common.Lighting.slang.
	static constexpr u_int uCLUSTER_DIM_X      = 16;
	static constexpr u_int uCLUSTER_DIM_Y      = 9;
	static constexpr u_int uCLUSTER_DIM_Z      = 24;
	static constexpr u_int uCLUSTER_COUNT      = uCLUSTER_DIM_X * uCLUSTER_DIM_Y * uCLUSTER_DIM_Z;
	static constexpr u_int uMAX_LIGHTS_PER_CLUSTER = 64;

	bool                 m_bInitialised = false;
	Flux_Shader          m_xComputeShader;
	Flux_Pipeline        m_xComputePipeline;
	Flux_RootSig         m_xComputeRootSig;
	Flux_ReadWriteBuffer m_xClusterLightCounts;
	Flux_ReadWriteBuffer m_xClusterLightIndices;
};
