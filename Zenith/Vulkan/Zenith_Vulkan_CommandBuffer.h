#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"

class Flux_VertexBuffer;
struct Flux_Texture;
class Flux_DynamicVertexBuffer;
class Flux_IndirectBuffer;
class Flux_IndexBuffer;
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_Sampler;
class Zenith_Vulkan;
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_Swapchain;
class Flux_GraphicsImpl;
class Zenith_Profiling;
struct Flux_Buffer;
class Flux_ReadWriteBuffer;
struct Flux_ConstantBufferView;
struct Flux_ShaderResourceView;
struct Flux_ShaderResourceView_Buffer;
struct Flux_UnorderedAccessView_Texture;
struct Flux_UnorderedAccessView_Buffer;
struct Flux_RenderTargetView;
struct Flux_DepthStencilView;
struct Flux_RenderAttachment;
struct Flux_RenderGraph_AttachmentRef;
struct Flux_RenderingBeginInfo;
class Flux_GraphResource;

// Translate the engine-facing ResourceAccess enum into the Vulkan (layout,
// access mask, pipeline stage) triple required by a pipeline barrier.
// bIsDepth selects the depth variants for the depth-attachment layouts.
void Flux_ResourceAccessToVulkan(ResourceAccess eAccess, bool bIsDepth,
	vk::ImageLayout& eOutLayout, vk::AccessFlags& eOutAccess, vk::PipelineStageFlags& eOutStage);

struct ScratchBufferBinding {
	u_int m_uOffset = 0;
	u_int m_uSize = 0;
	u_int m_uBinding = 0;
	bool m_bValid = false;
};

struct DescSetBindings {
	const Flux_ShaderResourceView* m_xSRVs[FLUX_MAX_BINDINGS_PER_GROUP];
	// Read-only structured-buffer SSBO slots. Stored by value (not pointer) so
	// the staging is self-contained even if the source Flux_ReadWriteBuffer's
	// lifetime ends between BindSRV_Buffer and the next descriptor update.
	Flux_ShaderResourceView_Buffer m_xSRV_Buffers[FLUX_MAX_BINDINGS_PER_GROUP];
	bool                           m_abSRV_BuffersActive[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_UnorderedAccessView_Texture* m_xUAV_Textures[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_UnorderedAccessView_Buffer* m_xUAV_Buffers[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_ConstantBufferView* m_xCBVs[FLUX_MAX_BINDINGS_PER_GROUP];

	Zenith_Vulkan_Sampler* m_apxSamplers[FLUX_MAX_BINDINGS_PER_GROUP];

	ScratchBufferBinding m_xScratchBuffer;

	// A zero-filled Flux_VRAMHandle (0) reports IsValid()==true (its "unset"
	// sentinel is UINT32_MAX), so cleared m_xSRV_Buffers slots are gated by
	// m_abSRV_BuffersActive rather than handle validity.
	void Clear() { memset(this, 0, sizeof(*this)); }

	bool operator==(const DescSetBindings& other) const
	{
		return memcmp(this, &other, sizeof(DescSetBindings)) == 0;
	}
};

class Zenith_Vulkan_CommandBuffer
{
public:
	Zenith_Vulkan_CommandBuffer() : m_uWorkerIndex(0) {}
	void Initialise(CommandType eType = COMMANDTYPE_GRAPHICS);
	void InitialiseWithCustomPool(const vk::CommandPool& xCustomPool, u_int uWorkerIndex, CommandType eType = COMMANDTYPE_GRAPHICS);
	void BeginRecording();
	void EndRendering();
	void EndRecording(bool bEndPass = true);
	void EndAndCpuWait(bool bEndPass);
	void SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	// Bind a compute-written read-write buffer as a vertex stream (Stage-5 skinned-vertex arena).
	// The buffer must have been created with vertex-buffer usage (InitialiseReadWriteBuffer's
	// bAlsoVertexBuffer). uByteOffset addresses a sub-range so per-instance skinned buckets share
	// one arena buffer (each binds at its slice base in bytes).
	void SetVertexBuffer(const Flux_ReadWriteBuffer& xVertexBuffer, uint32_t uBindPoint = 0, size_t uByteOffset = 0);
	void SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer);
	void Draw(uint32_t uNumVerts);
	void DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances = 1, uint32_t uVertexOffset = 0, uint32_t uIndexOffset = 0, uint32_t uInstanceOffset = 0);
	void DrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, uint32_t uDrawCount, uint32_t uOffset = 0, uint32_t uStride = 20);
	void DrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, uint32_t uMaxDrawCount, uint32_t uIndirectOffset = 0, uint32_t uCountOffset = 0, uint32_t uStride = 20);
	void BeginRendering(const Flux_RenderingBeginInfo& xInfo);
	void SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	
	void BindSRV(const Flux_ShaderResourceView* pxSRV, const Flux_BindingSlot& xSlot, Zenith_Vulkan_Sampler* pxSampler = nullptr);
	void BindSRV_Buffer(const Flux_ShaderResourceView_Buffer& xSRV, const Flux_BindingSlot& xSlot);
	void BindUAV_Texture(const Flux_UnorderedAccessView_Texture* pxUAV, const Flux_BindingSlot& xSlot);
	void BindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* pxUAV, const Flux_BindingSlot& xSlot);
	void BindCBV(const Flux_ConstantBufferView* pxCBV, const Flux_BindingSlot& xSlot);

	void BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint);
	void BindDrawConstants(const void* pData, size_t uSize, const Flux_BindingSlot& xSlot);
	void SetDepthBias(float fConstant, float fSlope, float fClamp);
	void SetShoudClear(const bool bClear);

	void UseBindlessTextures(const uint32_t uSet);

	vk::CommandBuffer& GetCurrentCmdBuffer() { return m_xCurrentCmdBuffer; }
	void* Platform_GetCurrentCmdBuffer() const { return (void*)&m_xCurrentCmdBuffer; }
	
	u_int GetWorkerIndex() const { return m_uWorkerIndex; }

	void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel = 0, int uLayer = 0);
	void ImageTransitionBarrierRange(vk::Image xImage,
		vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout,
		vk::ImageAspectFlags eAspect,
		vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage,
		vk::AccessFlags eSrcAccess, vk::AccessFlags eDstAccess,
		uint32_t uBaseMip, uint32_t uMipCount,
		uint32_t uBaseLayer, uint32_t uLayerCount);

	void BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	void Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ);
	void ImageBarrier(Flux_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout);

	void ImageTransition(Flux_RenderAttachment* pxAttachment,
		uint32_t uBaseMip, uint32_t uMipCount,
		uint32_t uBaseLayer, uint32_t uLayerCount,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess);
	// Polymorphic overload — handles both 2D and cube attachments via the
	// graph-resource tag. Used by the render-graph prologue-barrier emitter.
	void ImageTransition(const Flux_GraphResource& xResource,
		uint32_t uBaseMip, uint32_t uMipCount,
		uint32_t uBaseLayer, uint32_t uLayerCount,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess);

	// Buffer-side equivalent of ImageTransition: emits a single
	// vk::BufferMemoryBarrier covering the whole buffer with stage / access
	// masks derived from Flux_ResourceAccessToVulkan. Buffers have no layout
	// transitions, so this is a pure memory + execution barrier — RAW, WAW,
	// or WAR ordering on UAV / indirect-arg / vertex / index buffers between
	// passes. Used by the render-graph prologue-barrier emitter for buffer
	// entries in m_xPrologueBarriers (see Flux_RenderGraph_Barrier doc).
	// pxBuffer must be non-null; its m_xVRAMHandle must reference a valid
	// VRAM allocation.
	void BufferBarrier(Flux_Buffer* pxBuffer,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess);

	// FluxBackendSync concept entry point. Dispatches on xResource.GetKind() to
	// ImageTransition (image) or BufferBarrier (buffer) -- the single neutral
	// barrier call the render-graph prologue emitter uses. ImageTransition /
	// BufferBarrier above are now backend-internal helpers, not concept methods.
	void ResourceBarrier(const Flux_GraphResource& xResource,
		const Flux_SubresourceRange& xRange,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess);

	void RenderImGui();
	void SetCurrentRenderPass(vk::RenderPass xRenderPass) { m_xCurrentRenderPass = xRenderPass; }

#ifdef ZENITH_FLUX_PROFILING
	void BeginDebugMarker(const char* szName);
	void EndDebugMarker();
	// GPU per-pass timestamp brackets. BeginGPUTimer claims a query slot from the
	// active frame and writes the start timestamp, returning the timer index;
	// EndGPUTimer writes the end timestamp for that index. UINT32_MAX means GPU
	// timestamps are unsupported or the per-frame budget is exhausted (untimed).
	// uExecutionIndex (the pass's topological position) is stored with the timer so
	// the readback can order passes by execution order, not record-race order.
	u_int BeginGPUTimer(const char* szName, u_int uExecutionIndex);
	void  EndGPUTimer(u_int uTimerIdx);
#endif

	vk::CommandBuffer m_xCurrentCmdBuffer;
	vk::RenderPass m_xCurrentRenderPass;
private:
	void UpdateDescriptorSets();
	// The three stages UpdateDescriptorSets drives, extracted so each stays
	// independently readable: (1) ZENITH_DEBUG-only staged-binding + VIEW-set
	// graph-Read() validation, (2) binding the persistent GLOBAL/VIEW spine sets,
	// (3) the per-set cache/allocate/write/bind loop.
	void ValidateDescriptorBindingsDebug(u_int uNumDescSets);
	void BindPersistentSpineSets(u_int uNumDescSets);
	void AllocateAndBindDescriptorSets(u_int uNumDescSets, const vk::Device& xDevice);
	// Walk the bindings for descriptor-set uDescSet and append per-binding write
	// records into the caller-provided stack buffers. Counts are updated in place.
	// Extracted from UpdateDescriptorSets so the outer cache/allocate/bind dance
	// stays tight; this helper owns the per-binding-type → vk write mapping.
	void BuildDescriptorWritesForSet(
		u_int uDescSet,
		vk::DescriptorBufferInfo* axBufferInfos, u_int& uNumBufferWrites,
		vk::DescriptorImageInfo* axTexInfos, u_int& uNumTexWrites,
		vk::WriteDescriptorSet* axWrites, u_int& uNumWrites);

	std::vector<vk::CommandBuffer> m_xCmdBuffers;

	Zenith_Vulkan_Pipeline* m_pxCurrentPipeline;
	vk::PipelineBindPoint m_eCurrentBindPoint = vk::PipelineBindPoint::eGraphics;

	DescSetBindings m_xBindings[FLUX_MAX_BINDING_GROUPS];

	CommandType m_eCommandType;
	u_int m_uWorkerIndex;

	// Injected subsystem dependencies (self-wired from g_xEngine once in
	// Initialise / InitialiseWithCustomPool). These replace scattered
	// g_xEngine.X() reaches throughout the recorder. Both init entry points
	// set them because worker / copy command buffers go through
	// InitialiseWithCustomPool, not Initialise; the call-sites (Zenith_Vulkan.cpp,
	// Zenith_Vulkan_MemoryManager.cpp, Zenith_Vulkan_Swapchain.cpp) are not the
	// Zenith_Engine.cpp composition root, so the signatures stay unchanged.
	Zenith_Vulkan*               m_pxVulkan       = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory = nullptr;
	Zenith_Vulkan_Swapchain*     m_pxVulkanSwapchain = nullptr;
	Flux_GraphicsImpl*           m_pxFluxGraphics = nullptr;
	Zenith_Profiling*            m_pxProfiling    = nullptr;

	vk::DescriptorSet m_axCurrentDescSet[FLUX_MAX_BINDING_GROUPS] = { VK_NULL_HANDLE };
	// Phase 5.1: per-bind-point tracking of the currently-bound persistent spine sets
	// ([0=graphics/1=compute][FluxFrequencyClass]), so GLOBAL/VIEW are bound once per
	// command buffer and a pipeline switch never forces a redundant rebind (sets 0/1/2
	// are layout-identical across pipelines = prefix-compatible). Reset in BeginRecording.
	vk::DescriptorSet m_axCurrentPersistentSet[2][4] = {};
	u_int m_uDescriptorDirty = true;

	vk::Viewport m_xViewport;
	vk::Rect2D m_xScissor;

	bool m_bShouldClear = false;
	
	struct DescriptorSetCacheEntry
	{
		DescSetBindings bindings;
		vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;
		vk::DescriptorSetLayout layout = VK_NULL_HANDLE;
	};

	DescriptorSetCacheEntry m_axDescriptorSetCache[FLUX_MAX_BINDING_GROUPS];
};