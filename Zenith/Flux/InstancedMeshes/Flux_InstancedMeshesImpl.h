#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include <vector>

class Flux_DynamicConstantBuffer;
class Flux_InstanceGroup;

// Phase 9: state + behaviour for InstancedMeshes subsystem.
class Flux_InstancedMeshesImpl
{
public:
	Flux_InstancedMeshesImpl() = default;
	~Flux_InstancedMeshesImpl() = default;

	Flux_InstancedMeshesImpl(const Flux_InstancedMeshesImpl&) = delete;
	Flux_InstancedMeshesImpl& operator=(const Flux_InstancedMeshesImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void RegisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void ClearAllGroups();

	void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	uint32_t GetTotalInstanceCount() const   { return m_uTotalInstances; }
	uint32_t GetVisibleInstanceCount() const { return m_uVisibleInstances; }
	uint32_t GetGroupCount() const           { return static_cast<uint32_t>(m_apxInstanceGroups.size()); }

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
