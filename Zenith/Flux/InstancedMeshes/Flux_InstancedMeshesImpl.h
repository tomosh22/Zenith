#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Collections/Zenith_Vector.h"

class Flux_DynamicConstantBuffer;
class Flux_InstanceGroup;
class Flux_MeshInstance;
class Flux_ShaderBinder;
class Flux_CommandList;
class Flux_GraphicsImpl;
class Zenith_Vulkan_MemoryManager;

// Phase 9: state + behaviour for InstancedMeshes subsystem.
class Flux_InstancedMeshesImpl
{
public:
	Flux_InstancedMeshesImpl() = default;
	~Flux_InstancedMeshesImpl() = default;

	Flux_InstancedMeshesImpl(const Flux_InstancedMeshesImpl&) = delete;
	Flux_InstancedMeshesImpl& operator=(const Flux_InstancedMeshesImpl&) = delete;

	void Initialise(Zenith_Vulkan_MemoryManager& xVulkanMemory, Flux_GraphicsImpl& xFluxGraphics);
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void RegisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void ClearAllGroups();

	void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// WS7 keystone main-thread gather (mirrors Flux_StaticMeshesImpl::GatherDrawPacket).
	// Hung via .Prepare on the first instanced pass; CallPrepareCallbacks runs it on
	// the main thread BEFORE any record task dispatches. Walks m_apxInstanceGroups
	// once: per group it does the CPU dirty-bookkeeping + GPU-buffer sync that the
	// record callbacks used to do concurrently (UpdateGPUBuffers + ResetVisibleCount),
	// uploads the per-group culling constants, and pre-computes the m_uTotalInstances
	// / m_uVisibleInstances stats. After this returns, every group's state is frozen
	// and ExecuteCulling / ExecuteInstancedGBuffer are pure readers.
	void GatherInstancedPacket(void*);

	// Promoted from a file-static helper so the ExecuteInstancedGBuffer trampoline
	// can route material/instance-buffer binding through this subsystem's members
	// (m_xGBufferShader) and the injected graphics singleton.
	void BindBatchDescriptors(Flux_ShaderBinder& xBinder, Flux_InstanceGroup* pxGroup);

	uint32_t GetTotalInstanceCount() const   { return m_uTotalInstances; }
	uint32_t GetVisibleInstanceCount() const { return m_uVisibleInstances; }
	uint32_t GetGroupCount() const           { return static_cast<uint32_t>(m_apxInstanceGroups.GetSize()); }

	Zenith_Vector<Flux_InstanceGroup*> m_apxInstanceGroups;

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

	// Injected engine-infra singletons (de-globalization pass).
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory = nullptr;
	Flux_GraphicsImpl*           m_pxFluxGraphics = nullptr;
};
