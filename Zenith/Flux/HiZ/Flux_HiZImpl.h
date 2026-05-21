#pragma once

#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 7a: per-Engine state for the Hi-Z (hierarchical-Z) subsystem.
// Replaces the file-statics in Flux_HiZ.cpp (mip count, init flag,
// transient handle, graph back-ref, compute shader / pipeline / root
// sig).
class Flux_HiZImpl
{
public:
	Flux_HiZImpl() = default;
	~Flux_HiZImpl() = default;

	Flux_HiZImpl(const Flux_HiZImpl&) = delete;
	Flux_HiZImpl& operator=(const Flux_HiZImpl&) = delete;

	u_int                 m_uMipCount    = 0;
	bool                  m_bInitialised = false;

	// Graph-owned transient + back-ref.
	Flux_TransientHandle  m_xHiZBufferHandle;
	Flux_RenderGraph*     m_pxGraph = nullptr;

	// Compute shader + pipeline + root signature.
	Flux_Shader           m_xComputeShader;
	Flux_Pipeline         m_xComputePipeline;
	Flux_RootSig          m_xComputeRootSig;
};
