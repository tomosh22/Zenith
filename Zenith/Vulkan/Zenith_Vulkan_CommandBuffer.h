#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Enums.h"

#include "Vulkan/Zenith_Vulkan_Texture.h"

#define MAX_BINDINGS 16

class Zenith_Vulkan_Buffer;
class Flux_VertexBuffer;
class Flux_DynamicVertexBuffer;
class Flux_IndexBuffer;
class Zenith_Vulkan_Pipeline;
struct Flux_TargetSetup;

struct DescSetBindings {
	Zenith_Vulkan_Buffer* m_xBuffers[MAX_BINDINGS];
	std::pair<Zenith_Vulkan_Texture*, Zenith_Vulkan_Sampler*> m_xTextures[MAX_BINDINGS];
};

class Zenith_Vulkan_CommandBuffer
{
public:
	Zenith_Vulkan_CommandBuffer() {}
	void Initialise(CommandType eType = COMMANDTYPE_GRAPHICS);
	void BeginRecording();
	void EndRenderPass();
	void EndRecording(RenderOrder eOrder, bool bEndPass = true);
	void CreateChild(Zenith_Vulkan_CommandBuffer& xChild);
	void ExecuteChild(Zenith_Vulkan_CommandBuffer& xChild);
	void EndAndCpuWait(bool bEndPass);
	void SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer);
	void Draw(uint32_t uNumVerts);
	void DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances = 1, uint32_t uVertexOffset = 0, uint32_t uIndexOffset = 0, uint32_t uInstanceOffset = 0);
	void BeginRenderPass(Flux_TargetSetup& xTargetSetup, bool bClearColour = false, bool bClearDepth = false, bool bClearStencil = false);
	void SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	void BindTexture(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler = nullptr);
	void BindBuffer(Zenith_Vulkan_Buffer* pxBuffer, uint32_t uBindPoint);
	void BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint);
	void PushConstant(void* pData, size_t uSize);
	void UploadUniformData(void* pData, size_t uSize);
	void SetShoudClear(const bool bClear);

	void UseBindlessTextures(const uint32_t uSet);

	void BeginBind(u_int uDescSet);

	vk::CommandBuffer& GetCurrentCmdBuffer() { return m_xCurrentCmdBuffer; }
	void* Platform_GetCurrentCmdBuffer() const { return (void*)&m_xCurrentCmdBuffer; }

	void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel = 0, int uLayer = 0);
	void CopyBufferToBuffer(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Buffer* pxDst, size_t uSize, size_t uSrcOffset);
	void CopyBufferToTexture(Zenith_Vulkan_Buffer* pxSrc, Zenith_Vulkan_Texture* pxDst, size_t uSrcOffset, uint32_t uNumLayers);
	void BlitTextureToTexture(Zenith_Vulkan_Texture* pxSrc, Zenith_Vulkan_Texture* pxDst, uint32_t uDstMip, uint32_t uDstLayer);

	// Compute methods
	void BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	void BindStorageImage(Zenith_Vulkan_Texture* pxTexture, uint32_t uBindPoint);
	void Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ);
	void ImageBarrier(Zenith_Vulkan_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout);

	bool IsRecording() const { return m_bIsRecording; }

	vk::CommandBuffer m_xCurrentCmdBuffer;
	vk::RenderPass m_xCurrentRenderPass;
private:
	void PrepareDrawCallDescriptors();
	void TransitionComputeResourcesBefore();
	void TransitionComputeResourcesAfter();
	std::vector<vk::CommandBuffer> m_xCmdBuffers;

	vk::Framebuffer m_xCurrentFramebuffer;

	std::vector<vk::Semaphore> m_xCompleteSems;

	Zenith_Vulkan_Pipeline* m_pxCurrentPipeline;
	vk::PipelineBindPoint m_eCurrentBindPoint = vk::PipelineBindPoint::eGraphics;

	DescSetBindings m_xBindings[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS];
	u_int m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;

	CommandType m_eCommandType;

	bool m_bIsRecording = false;

	Zenith_Vulkan_Texture* m_aapxTextureCache[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS][MAX_BINDINGS];
	Zenith_Vulkan_Buffer* m_aapxBufferCache[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS][MAX_BINDINGS];
	//#TO_TODO: accel struct cache

	vk::DescriptorSet m_axCurrentDescSet[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS] = { VK_NULL_HANDLE };
	u_int m_uDescriptorDirty = true;

	bool m_bIsParent = false;
	Zenith_Vulkan_CommandBuffer* m_pxParent = nullptr;

	vk::Viewport m_xViewport;
	vk::Rect2D m_xScissor;

	bool m_bShouldClear = false;
};
