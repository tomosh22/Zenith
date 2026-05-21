#pragma once

#include "Flux/Decals/Flux_Decals.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Phase 7d: per-Engine state for Decals subsystem. The CPU staging
// types (CpuDecalSlot / DecalInstance) live as file-local POD in the
// .cpp -- the array storage stays there, only the counts + pipelines
// + graph state move here.
class Flux_DecalsImpl
{
public:
	Flux_DecalsImpl() = default;
	~Flux_DecalsImpl() = default;

	Flux_DecalsImpl(const Flux_DecalsImpl&) = delete;
	Flux_DecalsImpl& operator=(const Flux_DecalsImpl&) = delete;

	u_int                       m_uNextSlot         = 0;
	u_int                       m_uActiveDecalCount = 0;

	Flux_RenderGraph*           m_pxGraph = nullptr;
	Flux_TransientHandle        m_xNormalsCopyHandle;
	Flux_PassHandle             m_xNormalsCopyPass;
	Flux_PassHandle             m_xApplyPass;

	Flux_Shader                 m_xNormalsCopyShader;
	Flux_Shader                 m_xApplyShader;
	Flux_Pipeline               m_xNormalsCopyPipeline;
	Flux_Pipeline               m_xApplyPipeline;

	Flux_DynamicReadWriteBuffer m_xDecalBuffer;
	Flux_IndexBuffer            m_xDecalIndexBuffer;
};
