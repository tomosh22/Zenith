#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#include "vma/vk_mem_alloc.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Types.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#define ZENITH_VULKAN_PER_FRAME_DESC_SET 0
#define ZENITH_VULKAN_PER_DRAW_DESC_SET 1
#define ZENITH_VULKAN_BINDLESS_TEXTURES_DESC_SET 2

class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_VRAM;
class Flux_CommandList;

class Zenith_Vulkan_PerFrame
{
public:
	Zenith_Vulkan_PerFrame() = default;

	void Initialise();
	void BeginFrame();
	const vk::DescriptorPool& GetDescriptorPoolForWorkerIndex(u_int uWorkerIndex);
	const vk::CommandPool& GetCommandPoolForWorkerIndex(u_int uWorkerIndex);
	Zenith_Vulkan_CommandBuffer& GetWorkerCommandBuffer(u_int uWorkerIndex);
	const vk::Semaphore& GetMemorySemaphore() const { return m_xMemorySemaphore; }

	vk::Fence m_xFence;
	vk::Semaphore m_xMemorySemaphore;
	
	static constexpr u_int NUM_WORKER_THREADS = FLUX_NUM_WORKER_THREADS;
	vk::DescriptorPool m_axDescriptorPools[NUM_WORKER_THREADS];
	vk::CommandPool m_axCommandPools[NUM_WORKER_THREADS];
	Zenith_Vulkan_CommandBuffer m_axWorkerCommandBuffers[NUM_WORKER_THREADS];
};

class Zenith_Vulkan_Sampler
{
public:
	const vk::Sampler& GetSampler() const { return m_xSampler; }

	static void InitialiseRepeat(Zenith_Vulkan_Sampler& xSampler);
	static void InitialiseClamp(Zenith_Vulkan_Sampler& xSampler);

	vk::Sampler m_xSampler;
};

class Zenith_Vulkan
{
public:

	static vk::Format ShaderDataTypeToVulkanFormat(ShaderDataType t);
	static vk::DescriptorSet CreateDescriptorSet(const vk::DescriptorSetLayout& xLayout, const vk::DescriptorPool& xPool);

	static void Initialise();
	static void CreateInstance();
#ifdef ZENITH_DEBUG
	static void CreateDebugMessenger();
#endif
	static void CreateSurface();
	static void CreatePhysicalDevice();
	static void CreateQueueFamilies();
	static void CreateDevice();
	static void CreateCommandPools();
	static void CreateDefaultDescriptorPool();
	static void CreateBindlessTexturesDescriptorPool();

#ifdef ZENITH_TOOLS
	static vk::RenderPass s_xImGuiRenderPass;
	static vk::DescriptorPool s_xImGuiDescriptorPool;
	static void InitialiseImGui();
	static void InitialiseImGuiRenderPass();
	static void ShutdownImGui();
	static void ImGuiBeginFrame();
	static const vk::DescriptorPool& GetImGuiDescriptorPool() { return s_xImGuiDescriptorPool; }
#endif

	static void BeginFrame();
	static void EndFrame(bool bSubmitRenderWork);

	// Wait for GPU to finish all work (blocks until idle)
	// WARNING: This is expensive - only use for critical synchronization (scene transitions, shutdown, etc.)
	static void WaitForGPUIdle();

	static void RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations);

	static const vk::Instance& GetInstance() { return s_xInstance; }
	static const vk::PhysicalDevice& GetPhysicalDevice() { return s_xPhysicalDevice; }
	static const vk::Device& GetDevice() { return s_xDevice; }
	static const vk::CommandPool& GetCommandPool(CommandType eType) { return s_axCommandPools[eType]; }
	static const vk::CommandPool& GetWorkerCommandPool(u_int uThreadIndex);
	static const vk::Queue& GetQueue(CommandType eType) { return s_axQueues[eType]; }
	static const vk::DescriptorPool& GetPerFrameDescriptorPool(u_int uWorkerIndex);
	static const vk::SurfaceKHR& GetSurface() { return s_xSurface; }
	static const uint32_t GetQueueIndex(CommandType eType) { return s_auQueueIndices[eType]; }
	static const vk::DescriptorPool& GetDefaultDescriptorPool() { return s_xDefaultDescriptorPool; }
	static vk::Fence& GetCurrentInFlightFence();

	static const bool ShouldSubmitDrawCalls();
	static const bool ShouldUseDescSetCache();
	static const bool ShouldOnlyUpdateDirtyDescriptors();
	#ifdef ZENITH_DEBUG_VARIABLES
	static void IncrementDescriptorSetAllocations();
	#endif

	static Zenith_Vulkan_CommandBuffer* s_pxMemoryUpdateCmdBuf;

	static vk::DescriptorSet& GetBindlessTexturesDescriptorSet() { return s_xBindlessTexturesDescriptorSet; }
	static vk::DescriptorSetLayout& GetBindlessTexturesDescriptorSetLayout() { return s_xBindlessTexturesDescriptorSetLayout; }

	// VRAM Registry
	static Flux_VRAMHandle RegisterVRAM(Zenith_Vulkan_VRAM* pxVRAM);
	static Zenith_Vulkan_VRAM* GetVRAM(const Flux_VRAMHandle xHandle);
	static void ReleaseVRAMHandle(const Flux_VRAMHandle xHandle);

	// Format conversion utilities
	static vk::Format ConvertToVkFormat_Colour(TextureFormat eFormat);
	static vk::Format ConvertToVkFormat_DepthStencil(TextureFormat eFormat);
	static vk::AttachmentLoadOp ConvertToVkLoadAction(LoadAction eAction);
	static vk::AttachmentStoreOp ConvertToVkStoreAction(StoreAction eAction);

private:
	static vk::Instance s_xInstance;
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT eMessageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData,
		void* pUserData);
	static vk::DebugUtilsMessengerEXT s_xDebugMessenger;
	static vk::SurfaceKHR s_xSurface;
	static vk::PhysicalDevice s_xPhysicalDevice;
	static struct GPUCapabilities {
		uint32_t m_uMaxTextureWidth;
		uint32_t m_uMaxTextureHeight;
		uint32_t m_uMaxFramebufferWidth;
		uint32_t m_uMaxFramebufferHeight;
	} s_xGPUCapabilties;
	static uint32_t s_auQueueIndices[COMMANDTYPE_MAX];
	static vk::Device s_xDevice;
	static vk::Queue s_axQueues[COMMANDTYPE_MAX];
	static vk::CommandPool s_axCommandPools[COMMANDTYPE_MAX];
	
	static vk::DescriptorPool s_xDefaultDescriptorPool;

	static vk::DescriptorPool s_xBindlessTexturesDescriptorPool;
	static vk::DescriptorSet s_xBindlessTexturesDescriptorSet;
	static vk::DescriptorSetLayout s_xBindlessTexturesDescriptorSetLayout;

	public:
	static std::vector<Zenith_Vulkan_VRAM*> s_xVRAMRegistry;
	static std::vector<uint32_t> s_xFreeVRAMHandles;

	static std::vector<const Zenith_Vulkan_CommandBuffer*> s_xPendingCommandBuffers[RENDER_ORDER_MAX];
	static Zenith_Vulkan_PerFrame s_axPerFrame[MAX_FRAMES_IN_FLIGHT];
	static Zenith_Vulkan_PerFrame* s_pxCurrentFrame;
};

class Zenith_Vulkan_VRAM
{
public:
	Zenith_Vulkan_VRAM(const vk::Image xImage, const VmaAllocation xAllocation, VmaAllocator xAllocator);

	Zenith_Vulkan_VRAM(const vk::Buffer xBuffer, const VmaAllocation xAllocation, VmaAllocator xAllocator, const u_int uSize);

	~Zenith_Vulkan_VRAM();

	VmaAllocation GetAllocation() const { return m_xAllocation; }
	vk::Image GetImage() const { return m_xImage; }
	vk::Buffer GetBuffer() const { return m_xBuffer; }
	u_int GetBufferSize() const { return m_uBufferSize; }

private:
	VmaAllocation m_xAllocation = VK_NULL_HANDLE;
	VmaAllocator m_xAllocator = VK_NULL_HANDLE;

	vk::Buffer m_xBuffer = VK_NULL_HANDLE;
	u_int m_uBufferSize = 0;

	vk::Image m_xImage = VK_NULL_HANDLE;
};