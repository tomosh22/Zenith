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
	//
	// Slot validity uses the parallel bool array below, NOT
	// m_xVRAMHandle.IsValid(): BeginRecording / BeginBind clear this struct
	// via memset(0), which sets every byte to 0 — and a Flux_VRAMHandle of 0
	// reports IsValid()==true (its sentinel for "unset" is UINT32_MAX, set
	// only by the default constructor). With memset zeroing, every slot
	// would otherwise look "valid" and the descriptor-update path would
	// emit storage-buffer writes for binding slots the layout reserves for
	// uniform buffers, tripping VK_DESCRIPTOR_TYPE mismatch validation.
	Flux_ShaderResourceView_Buffer m_xSRV_Buffers[FLUX_MAX_BINDINGS_PER_GROUP];
	bool                           m_abSRV_BuffersActive[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_UnorderedAccessView_Texture* m_xUAV_Textures[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_UnorderedAccessView_Buffer* m_xUAV_Buffers[FLUX_MAX_BINDINGS_PER_GROUP];
	const Flux_ConstantBufferView* m_xCBVs[FLUX_MAX_BINDINGS_PER_GROUP];

	Zenith_Vulkan_Sampler* m_apxSamplers[FLUX_MAX_BINDINGS_PER_GROUP];

	ScratchBufferBinding m_xScratchBuffer;

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
	void EndRenderPass();
	void EndRecording(bool bEndPass = true);
	void EndAndCpuWait(bool bEndPass);
	void SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer);
	void Draw(uint32_t uNumVerts);
	void DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances = 1, uint32_t uVertexOffset = 0, uint32_t uIndexOffset = 0, uint32_t uInstanceOffset = 0);
	void DrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, uint32_t uDrawCount, uint32_t uOffset = 0, uint32_t uStride = 20);
	void DrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, uint32_t uMaxDrawCount, uint32_t uIndirectOffset = 0, uint32_t uCountOffset = 0, uint32_t uStride = 20);
	void BeginRenderPass(const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour, const Flux_RenderGraph_AttachmentRef& rxDepthStencil, bool bClearColour = false, bool bClearDepth = false, bool bClearStencil = false, bool bDepthIsReadOnly = false);
	void SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	
	void BindSRV(const Flux_ShaderResourceView* pxSRV, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler = nullptr);
	void BindSRV_Buffer(const Flux_ShaderResourceView_Buffer& xSRV, uint32_t uBindPoint);
	void BindUAV_Texture(const Flux_UnorderedAccessView_Texture* pxUAV, uint32_t uBindPoint);
	void BindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* pxUAV, uint32_t uBindPoint);
	void BindCBV(const Flux_ConstantBufferView* pxCBV, uint32_t uBindPoint);

	void BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint);
	void BindDrawConstants(void* pData, size_t uSize, u_int uBinding);
	void SetCullMode(CullMode eCullMode);
	void SetDepthBias(float fConstant, float fSlope, float fClamp);
	void SetShoudClear(const bool bClear);

	void UseBindlessTextures(const uint32_t uSet);

	void BeginBind(u_int uDescSet);

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

	void RenderImGui();
	void SetCurrentRenderPass(vk::RenderPass xRenderPass) { m_xCurrentRenderPass = xRenderPass; }

#ifdef ZENITH_FLUX_PROFILING
	void BeginDebugMarker(const char* szName);
	void EndDebugMarker();
#endif

	vk::CommandBuffer m_xCurrentCmdBuffer;
	vk::RenderPass m_xCurrentRenderPass;
private:
	void UpdateDescriptorSets();
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
	u_int m_uCurrentBindFreq = FLUX_MAX_BINDING_GROUPS;

	CommandType m_eCommandType;
	u_int m_uWorkerIndex;

	vk::DescriptorSet m_axCurrentDescSet[FLUX_MAX_BINDING_GROUPS] = { VK_NULL_HANDLE };
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