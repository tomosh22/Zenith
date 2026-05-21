#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Phase 7c: per-Engine state for LightClustering subsystem.
class Flux_LightClusteringImpl
{
public:
	Flux_LightClusteringImpl() = default;
	~Flux_LightClusteringImpl() = default;

	Flux_LightClusteringImpl(const Flux_LightClusteringImpl&) = delete;
	Flux_LightClusteringImpl& operator=(const Flux_LightClusteringImpl&) = delete;

	Flux_Shader          m_xComputeShader;
	Flux_Pipeline        m_xComputePipeline;
	Flux_RootSig         m_xComputeRootSig;
	Flux_ReadWriteBuffer m_xClusterLightCounts;
	Flux_ReadWriteBuffer m_xClusterLightIndices;
};
