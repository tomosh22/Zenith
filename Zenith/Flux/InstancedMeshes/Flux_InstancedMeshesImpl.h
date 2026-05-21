#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include <vector>

class Flux_InstanceGroup;

// Phase 7e: per-Engine state for InstancedMeshes subsystem.
class Flux_InstancedMeshesImpl
{
public:
	Flux_InstancedMeshesImpl() = default;
	~Flux_InstancedMeshesImpl() = default;

	Flux_InstancedMeshesImpl(const Flux_InstancedMeshesImpl&) = delete;
	Flux_InstancedMeshesImpl& operator=(const Flux_InstancedMeshesImpl&) = delete;

	std::vector<Flux_InstanceGroup*> m_apxInstanceGroups;

	Flux_Shader                m_xGBufferShader;
	Flux_Pipeline              m_xGBufferPipeline;
	Flux_Shader                m_xShadowShader;
	Flux_Pipeline              m_xShadowPipeline;

	Flux_Shader                m_xCullingShader;
	Flux_Pipeline              m_xCullingPipeline;
	Flux_RootSig               m_xCullingRootSig;
	Flux_DynamicConstantBuffer m_xCullingConstantsBuffer;
	bool                       m_bCullingInitialized = false;
	bool                       m_bCullingEnabled     = true;

	uint32_t                   m_uTotalInstances     = 0;
	uint32_t                   m_uVisibleInstances   = 0;
};
