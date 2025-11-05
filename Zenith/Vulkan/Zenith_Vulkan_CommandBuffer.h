#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_Types.h"

#define MAX_BINDINGS 16

class Flux_VertexBuffer;
class Flux_Texture;
class Flux_DynamicVertexBuffer;
class Flux_IndexBuffer;
class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_Sampler;
struct Flux_TargetSetup;
struct Flux_ConstantBufferView;
struct Flux_ShaderResourceView;
struct Flux_UnorderedAccessView;
struct Flux_RenderTargetView;
struct Flux_DepthStencilView;

struct DescSetBindings {
	const Flux_ShaderResourceView* m_xSRVs[MAX_BINDINGS];
	const Flux_UnorderedAccessView* m_xUAVs[MAX_BINDINGS];
	const Flux_ConstantBufferView* m_xCBVs[MAX_BINDINGS];

	Zenith_Vulkan_Sampler* m_apxSamplers[MAX_BINDINGS];
};

class Zenith_Vulkan_CommandBuffer
{
public:
	Zenith_Vulkan_CommandBuffer() {}
	void Initialise(CommandType eType = COMMANDTYPE_GRAPHICS);
	void BeginRecording();
	void EndRenderPass();
	void EndRecording(RenderOrder eOrder, bool bEndPass = true);
	void EndAndCpuWait(bool bEndPass);
	void SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0);
	void SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer);
	void Draw(uint32_t uNumVerts);
	void DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances = 1, uint32_t uVertexOffset = 0, uint32_t uIndexOffset = 0, uint32_t uInstanceOffset = 0);
	void BeginRenderPass(Flux_TargetSetup& xTargetSetup, bool bClearColour = false, bool bClearDepth = false, bool bClearStencil = false);
	void SetPipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	
	// Direct3D-style view binding methods
	void BindSRV(const Flux_ShaderResourceView* pxSRV, uint32_t uBindPoint, Zenith_Vulkan_Sampler* pxSampler = nullptr);
	void BindUAV(const Flux_UnorderedAccessView* pxUAV, uint32_t uBindPoint);
	void BindCBV(const Flux_ConstantBufferView* pxCBV, uint32_t uBindPoint);

	void BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint);
	void PushConstant(void* pData, size_t uSize);
	void SetShoudClear(const bool bClear);

	void UseBindlessTextures(const uint32_t uSet);

	void BeginBind(u_int uDescSet);

	vk::CommandBuffer& GetCurrentCmdBuffer() { return m_xCurrentCmdBuffer; }
	void* Platform_GetCurrentCmdBuffer() const { return (void*)&m_xCurrentCmdBuffer; }

	void ImageTransitionBarrier(vk::Image xImage, vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::ImageAspectFlags eAspect, vk::PipelineStageFlags eSrcStage, vk::PipelineStageFlags eDstStage, int uMipLevel = 0, int uLayer = 0);

	// Compute methods
	void BindComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline);
	void Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ);
	void ImageBarrier(Flux_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout);

	vk::CommandBuffer m_xCurrentCmdBuffer;
	vk::RenderPass m_xCurrentRenderPass;
private:
	void PrepareDrawCallDescriptors();
	void TransitionUAVs(vk::ImageLayout eOldLayout, vk::ImageLayout eNewLayout, vk::AccessFlags eSrcAccessFlags, vk::AccessFlags eDstAccessFlags, vk::PipelineStageFlags eSrcStages, vk::PipelineStageFlags eDstStages);
	std::vector<vk::CommandBuffer> m_xCmdBuffers;

	Zenith_Vulkan_Pipeline* m_pxCurrentPipeline;
	vk::PipelineBindPoint m_eCurrentBindPoint = vk::PipelineBindPoint::eGraphics;

	DescSetBindings m_xBindings[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS];
	u_int m_uCurrentBindFreq = FLUX_MAX_DESCRIPTOR_SET_LAYOUTS;

	CommandType m_eCommandType;

	vk::DescriptorSet m_axCurrentDescSet[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS] = { VK_NULL_HANDLE };
	u_int m_uDescriptorDirty = true;

	vk::Viewport m_xViewport;
	vk::Rect2D m_xScissor;

	bool m_bShouldClear = false;
};
