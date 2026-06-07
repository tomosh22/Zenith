#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "vulkan/vulkan.hpp"
#pragma warning(push, 0)
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "vma/vk_mem_alloc.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Flux/Flux_Types.h"
#include "Zenith_Vulkan_CommandBuffer.h"

#include <vector>

// VkUnwrap: extract value from vk::ResultValue<T>
// Some Vulkan calls (e.g. createGraphicsPipeline) return ResultValue<T> on all platforms
// because they have non-error success codes like VK_PIPELINE_COMPILE_REQUIRED
template<typename T>
inline T VkUnwrap(vk::ResultValue<T> xResult)
{
	Zenith_Assert(xResult.result == vk::Result::eSuccess, "Vulkan error: %d", static_cast<int>(xResult.result));
	return std::move(xResult.value);
}

#ifdef VULKAN_HPP_NO_EXCEPTIONS
// When exceptions are disabled, all vulkan.hpp calls return ResultValue<T>
inline void VkCheckImpl(vk::Result eResult)
{
	Zenith_Assert(eResult == vk::Result::eSuccess, "Vulkan error: %d", static_cast<int>(eResult));
}
#define VkCheck(expr) VkCheckImpl(expr)
#else
// With exceptions enabled, most calls return T directly — pass through unchanged
template<typename T>
inline T VkUnwrap(T xValue) { return xValue; }
#define VkCheck(expr) (expr)
#endif

class Zenith_Vulkan_CommandBuffer;
class Zenith_Vulkan_VRAM;
class Flux_CommandList;
// Injected cross-subsystem dependencies (stored as raw pointers, wired in
// Initialise() by the composition root — see Zenith_Vulkan.cpp::Initialise).
class Flux_RendererImpl;
class Zenith_TaskSystem;
class Zenith_Vulkan_Swapchain;
class Zenith_Vulkan_MemoryManager;

class Zenith_Vulkan_PerFrame
{
public:
	Zenith_Vulkan_PerFrame() = default;

	void Initialise();
	void InitialiseScratchBuffers(); // Must be called after g_xEngine.VulkanMemory().Initialise()
	void ShutdownScratchBuffer();    // Must be called before g_xEngine.VulkanMemory().Shutdown() destroys the VMA allocator
	void BeginFrame();
	const vk::DescriptorPool& GetDescriptorPoolForWorkerIndex(u_int uWorkerIndex);
	const vk::CommandPool& GetCommandPoolForWorkerIndex(u_int uWorkerIndex);
	Zenith_Vulkan_CommandBuffer& GetWorkerCommandBuffer(u_int uWorkerIndex);
	const vk::Semaphore& GetMemorySemaphore() const { return m_xMemorySemaphore; }

	// Scratch buffer access
	const vk::Buffer& GetScratchBuffer() const { return m_xScratchBuffer; }
	void* GetScratchBufferMappedPtr() const { return m_pScratchBufferMapped; }
	u_int AllocateScratchBuffer(u_int uSize, u_int uWorkerIndex);

	vk::Fence m_xFence;
	vk::Semaphore m_xMemorySemaphore;

	static constexpr u_int NUM_WORKER_THREADS = FLUX_NUM_WORKER_THREADS;
	vk::DescriptorPool m_axDescriptorPools[NUM_WORKER_THREADS];
	vk::CommandPool m_axCommandPools[NUM_WORKER_THREADS];
	Zenith_Vulkan_CommandBuffer m_axWorkerCommandBuffers[NUM_WORKER_THREADS];

	// Deferred destruction of per-frame Vulkan objects
	void DeferDestroyFramebuffer(vk::Framebuffer xFramebuffer);
	void DeferDestroyRenderPass(vk::RenderPass xRenderPass);

	// Scratch buffer for push constant replacement (1MB total, 128KB per worker)
	static constexpr u_int uSCRATCH_BUFFER_SIZE = 1 * 1024 * 1024;
	static constexpr u_int uWORKER_PARTITION_SIZE = uSCRATCH_BUFFER_SIZE / NUM_WORKER_THREADS;

private:
	std::vector<vk::Framebuffer> m_axPendingFramebuffers;
	std::vector<vk::RenderPass> m_axPendingRenderPasses;
	Zenith_Mutex m_xDeferredDestroyMutex;
	vk::Buffer m_xScratchBuffer;
	VmaAllocation m_xScratchAllocation = VK_NULL_HANDLE;
	void* m_pScratchBufferMapped = nullptr;
	u_int m_auWorkerScratchOffsets[NUM_WORKER_THREADS] = {};
	u_int m_uMinAlignment = 256; // minUniformBufferOffsetAlignment
};

class Zenith_Vulkan_Sampler
{
public:
	const vk::Sampler& GetSampler() const { return m_xSampler; }

	static void InitialiseRepeat(Zenith_Vulkan_Sampler& xSampler);
	static void InitialiseClamp(Zenith_Vulkan_Sampler& xSampler);

	vk::Sampler m_xSampler;
};

// Per-Engine state + behaviour for the Vulkan backend. Replaces both
// the static-facade `class Zenith_Vulkan` and the data-only
// `Zenith_VulkanImpl` that used to live in Zenith_VulkanImpl.h. Methods
// moved off the facade; data members moved off the Impl. Accessed via
// g_xEngine.Vulkan().
class Zenith_Vulkan
{
public:
	Zenith_Vulkan() = default;
	~Zenith_Vulkan() = default;
	Zenith_Vulkan(const Zenith_Vulkan&) = delete;
	Zenith_Vulkan& operator=(const Zenith_Vulkan&) = delete;

	// ===== Format / descriptor helpers (called without a Vulkan instance) =====
	// These are pure functions but kept as members so call sites consistently
	// use g_xEngine.Vulkan().X() rather than mixing static + instance forms.
	vk::Format ShaderDataTypeToVulkanFormat(ShaderDataType t);
	vk::DescriptorSet CreateDescriptorSet(const vk::DescriptorSetLayout& xLayout, const vk::DescriptorPool& xPool);

	// ===== Bootstrap =====
	// The one-shot singleton boot seam (called once from the composition root
	// during engine boot). Cross-subsystem dependencies are injected here and
	// stored as member pointers so steady-state methods route through them
	// instead of reaching back through g_xEngine. (The *Impl objects are all
	// allocated up-front, so storing a not-yet-Initialised sibling pointer here
	// is safe — each reach still fires at exactly the same moment it did before.)
	void Initialise(); // no-arg per FluxBackendDevice concept; self-wires deps from g_xEngine
	void InitialiseScratchBuffers(); // Must be called after g_xEngine.VulkanMemory().Initialise()
	void CreateInstance();
#ifdef ZENITH_DEBUG
	void CreateDebugMessenger();
#endif
#ifdef ZENITH_FLUX_PROFILING
	vk::DispatchLoaderDynamic& GetDispatchLoader();
#endif
	void CreateSurface();
	void CreatePhysicalDevice();
	void LogFormatSupport();
	void CreateQueueFamilies();
	void CreateDevice();
	void CreateCommandPools();
	void CreateDefaultDescriptorPool();
	void CreateBindlessTexturesDescriptorPool();

#ifdef ZENITH_TOOLS
	void InitialiseImGui();
	void InitialiseImGuiRenderPass();
	void ShutdownImGui();
	void ImGuiBeginFrame();
	const vk::DescriptorPool& GetImGuiDescriptorPool();

	// ImGui memory tracking
	u_int64 GetImGuiMemoryAllocated();
	u_int64 GetImGuiAllocationCount();

	// Engine-typed wrapper that builds an ImGui-compatible texture identifier
	// from a Flux SRV + sampler. Returns a uint64 that can be cast to
	// ImTextureID for ImGui::Image.
	uint64_t CreateImGuiTextureID(const Flux_ShaderResourceView& xView, const Zenith_Vulkan_Sampler& xSampler);
#endif

	// Per-frame callback registered with Flux_RendererImpl at Initialise time.
	// Wraps the wait-fence / reset-pools / drain-typed-deletion-queues logic.
	// The first parameter is the ring index from the per-frame ring; the
	// second is the user data pointer supplied to RegisterBeginFrameCallback.
	static void OnFluxPerFrameBegin(u_int uRingIndex, void* pUserData);

	void EndFrame(bool bSubmitRenderWork);

	// Wait for GPU to finish all work (blocks until idle)
	// WARNING: This is expensive — only use for critical synchronization
	// (scene transitions, shutdown, etc.)
	void WaitForGPUIdle();

	// Task-system entry point. Stays static so it can be passed as a
	// Zenith_TaskArrayFunction; the body resolves the engine singleton.
	static void RecordCommandBuffersTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations);

	const vk::Instance& GetInstance();
	const vk::PhysicalDevice& GetPhysicalDevice();
	const vk::Device& GetDevice();
	const vk::CommandPool& GetCommandPool(CommandType eType);
	const vk::CommandPool& GetWorkerCommandPool(u_int uThreadIndex);
	const vk::Queue& GetQueue(CommandType eType);
	const vk::DescriptorPool& GetPerFrameDescriptorPool(u_int uWorkerIndex);
	const vk::SurfaceKHR& GetSurface();
	const uint32_t GetQueueIndex(CommandType eType);
	const vk::DescriptorPool& GetDefaultDescriptorPool();
	vk::Fence& GetCurrentInFlightFence();

	const bool ShouldSubmitDrawCalls();
	const bool ShouldUseDescSetCache();
	const bool ShouldOnlyUpdateDirtyDescriptors();
	#ifdef ZENITH_DEBUG_VARIABLES
	void IncrementDescriptorSetAllocations();
	#endif

	vk::DescriptorSet& GetBindlessTexturesDescriptorSet();
	vk::DescriptorSetLayout& GetBindlessTexturesDescriptorSetLayout();
	void WriteBindlessDescriptor(uint32_t uIndex, vk::ImageView xImageView, vk::Sampler xSampler);

	// Engine-typed wrapper around WriteBindlessDescriptor. Engine code (asset
	// system, etc.) calls this rather than reaching for vk::ImageView /
	// vk::Sampler directly. The wrapper extracts the native handles
	// internally so callers stay portable.
	void WriteBindlessTextureSlot(uint32_t uIndex, const Flux_ShaderResourceView& xView, const Zenith_Vulkan_Sampler& xSampler);

	// VRAM Registry
	Flux_VRAMHandle RegisterVRAM(Zenith_Vulkan_VRAM* pxVRAM);
	Zenith_Vulkan_VRAM* GetVRAM(const Flux_VRAMHandle xHandle);
	void ReleaseVRAMHandle(const Flux_VRAMHandle xHandle);

	// Format conversion utilities
	vk::Format ConvertToVkFormat_Colour(TextureFormat eFormat);
	vk::Format ConvertToVkFormat_DepthStencil(TextureFormat eFormat);
	vk::AttachmentLoadOp ConvertToVkLoadAction(LoadAction eAction);
	vk::AttachmentStoreOp ConvertToVkStoreAction(StoreAction eAction);

	// GPUCapabilities struct, nested for scoping.
	struct GPUCapabilities {
		uint32_t m_uMaxTextureWidth;
		uint32_t m_uMaxTextureHeight;
		uint32_t m_uMaxFramebufferWidth;
		uint32_t m_uMaxFramebufferHeight;
	};

	// ===== Data members (was Zenith_Vulkan) =====

	// Instance / surface / device.
	vk::Instance                  m_xInstance;
	vk::DebugUtilsMessengerEXT    m_xDebugMessenger;
#ifdef ZENITH_FLUX_PROFILING
	vk::DispatchLoaderDynamic     m_xDispatchLoader;
#endif
	vk::SurfaceKHR                m_xSurface;
	vk::PhysicalDevice            m_xPhysicalDevice;
	GPUCapabilities               m_xGPUCapabilties = {};
	uint32_t                      m_auQueueIndices[COMMANDTYPE_MAX] = {};
	vk::Device                    m_xDevice;
	vk::Queue                     m_axQueues[COMMANDTYPE_MAX];
	vk::CommandPool               m_axCommandPools[COMMANDTYPE_MAX];

	// Default + bindless descriptor pools.
	vk::DescriptorPool            m_xDefaultDescriptorPool;
	vk::DescriptorPool            m_xBindlessTexturesDescriptorPool;
	vk::DescriptorSet             m_xBindlessTexturesDescriptorSet;
	vk::DescriptorSetLayout       m_xBindlessTexturesDescriptorSetLayout;

	// VRAM registry + freelist of slots.
	std::vector<Zenith_Vulkan_VRAM*> m_xVRAMRegistry;
	std::vector<uint32_t>            m_xFreeVRAMHandles;

	// Pending command buffers awaiting submission (Phase 2 of graph execute).
	std::vector<const Zenith_Vulkan_CommandBuffer*> m_xPendingCommandBuffers;

	// Per-frame ring state (fences, semaphores, command-pool slots).
	Zenith_Vulkan_PerFrame        m_axPerFrame[MAX_FRAMES_IN_FLIGHT];
	Zenith_Vulkan_PerFrame*       m_pxCurrentFrame = nullptr;

	// Transfer command buffer used for staging-buffer flushes.
	Zenith_Vulkan_CommandBuffer*  m_pxMemoryUpdateCmdBuf = nullptr;

	// Injected cross-subsystem dependencies (wired in Initialise). Stored as
	// member pointers so instance methods route through them rather than
	// reaching back through g_xEngine. NOT used by the static callbacks
	// (OnFluxPerFrameBegin / RecordCommandBuffersTask) — those have no 'this'
	// and recover the singleton directly.
	Flux_RendererImpl*            m_pxFluxRenderer    = nullptr;
	Zenith_TaskSystem*            m_pxTasks           = nullptr;
	Zenith_Vulkan_Swapchain*      m_pxVulkanSwapchain = nullptr;
	Zenith_Vulkan_MemoryManager*  m_pxVulkanMemory    = nullptr;

#ifdef ZENITH_TOOLS
	// ImGui integration resources.
	vk::RenderPass                m_xImGuiRenderPass;
	vk::DescriptorPool            m_xImGuiDescriptorPool;
	vk::DescriptorSetLayout       m_xImGuiPreviewLayout;
#endif

private:
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT eMessageSeverity,
		vk::DebugUtilsMessageTypeFlagsEXT eMessageType,
		const vk::DebugUtilsMessengerCallbackDataEXT* pxCallbackData,
		void* pUserData);
};

class Zenith_Vulkan_VRAM
{
public:
	Zenith_Vulkan_VRAM(const vk::Image xImage, const VmaAllocation xAllocation, VmaAllocator xAllocator);

	Zenith_Vulkan_VRAM(const vk::Buffer xBuffer, const VmaAllocation xAllocation, VmaAllocator xAllocator, const u_int uSize);

	// Aliased-image constructor: the image is bound to an allocation owned by
	// a *different* pool VRAM. m_bAliased is set true so the destructor
	// destroys only the image (the allocation is freed when the pool VRAM is
	// destroyed). m_xAllocation is retained for debug/inspection but not
	// treated as owned.
	struct AliasedImageTag {};
	Zenith_Vulkan_VRAM(AliasedImageTag, const vk::Image xImage, const VmaAllocation xPoolAllocation, VmaAllocator xAllocator);

	// Pool constructor: owns an allocation sized for multiple aliased images
	// but has no image/buffer of its own. Destruction frees the allocation.
	struct PoolTag {};
	Zenith_Vulkan_VRAM(PoolTag, const VmaAllocation xAllocation, VmaAllocator xAllocator, u_int64 ulPoolSize);

	~Zenith_Vulkan_VRAM();

	VmaAllocation GetAllocation() const { return m_xAllocation; }
	VmaAllocator GetAllocator() const { return m_xAllocator; }
	vk::Image GetImage() const { return m_xImage; }
	vk::Buffer GetBuffer() const { return m_xBuffer; }
	u_int GetBufferSize() const { return m_uBufferSize; }
	bool IsAliased() const { return m_bAliased; }
	bool IsPool() const    { return m_bPool; }

private:
	VmaAllocation m_xAllocation = VK_NULL_HANDLE;
	VmaAllocator m_xAllocator = VK_NULL_HANDLE;

	vk::Buffer m_xBuffer = VK_NULL_HANDLE;
	u_int m_uBufferSize = 0;

	vk::Image m_xImage = VK_NULL_HANDLE;

	// Aliased image: the VkImage is bound to someone else's VmaAllocation
	// (a pool's allocation). On destruction, destroy only the image —
	// NOT the allocation. The pool's own VRAM frees the allocation.
	bool m_bAliased = false;

	// Pool VRAM: owns an allocation sized for multiple aliased images but
	// has no image/buffer of its own. On destruction, vmaFreeMemory the
	// allocation. Images aliased into this pool must have been destroyed
	// before the pool (the frame-in-flight deferred-deletion ring ensures
	// this since aliased-image VRAM queues deletion first in AllocateTransients).
	bool m_bPool = false;

	// Size of the pool allocation (pool VRAMs only). Tracked here for the
	// memory-usage accounting that the non-pool path infers from m_xAllocation->GetSize.
	u_int64 m_ulPoolSize = 0;
};