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
#include <atomic>

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
struct Flux_WorkDistribution;   // RecordFrame param (reference — forward decl suffices)
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
	void InitialisePerFrameResources(); // Must be called after g_xEngine.FluxMemory().Initialise()
	void ShutdownScratchBuffer();    // Must be called before g_xEngine.FluxMemory().Shutdown() destroys the VMA allocator
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

	// Phase 5.1: persistent spine descriptor sets for THIS frame-in-flight — set 0
	// (GLOBAL) + set 1 (VIEW). Allocated once at init from the backend persistent pool
	// (NOT the per-worker pools reset each frame), rewritten once per frame in
	// Zenith_Vulkan::PreparePersistentSets before any worker records. One set per frame
	// slot means frame N's descriptors are never overwritten while N-1 is in flight.
	vk::DescriptorSet m_xGlobalSet;
	vk::DescriptorSet m_xViewSet;

	// Deferred destruction of per-frame Vulkan objects
	void DeferDestroyFramebuffer(vk::Framebuffer xFramebuffer);
	void DeferDestroyRenderPass(vk::RenderPass xRenderPass);

	// Scratch buffer for push constant replacement (1MB total, 128KB per worker)
	static constexpr u_int uSCRATCH_BUFFER_SIZE = 1 * 1024 * 1024;
	static constexpr u_int uWORKER_PARTITION_SIZE = uSCRATCH_BUFFER_SIZE / NUM_WORKER_THREADS;

#ifdef ZENITH_FLUX_PROFILING
	// ---- GPU per-pass timestamp profiling ----------------------------------
	// One timestamp query PAIR per recorded render-graph pass, claimed atomically
	// during parallel recording (workers race on m_uGPUTimerCount). The pool is
	// cmd-reset at the head of worker 0's command buffer (submitted first, so the
	// reset precedes every pass's writes on the GPU timeline) and read back
	// deferred — MAX_FRAMES_IN_FLIGHT frames later in BeginFrame, guarded by this
	// slot's fence (the same "prior GPU work for this slot is done" guarantee the
	// deferred-deletion drains rely on). Results feed the CPU profiler's GPU
	// channel (per-pass GPU milliseconds). Disabled cleanly when the graphics
	// queue reports timestampValidBits == 0.
	static constexpr u_int uMAX_GPU_TIMERS = 256;            // 512 timestamp queries / frame slot
	u_int ClaimGPUTimer(const char* szName, u_int uExecutionIndex);  // atomic; timer idx or UINT32_MAX when full
	vk::QueryPool GetTimestampQueryPool() const { return m_xTimestampQueryPool; }
	void CmdResetGPUTimers(Zenith_Vulkan_CommandBuffer& xCmd);  // worker-0, head of frame
	void ReadbackGPUTimers();                                // deferred read → profiler GPU channel
	void CreateTimestampQueryPool();                         // called from Initialise when supported

	vk::QueryPool      m_xTimestampQueryPool;
	std::atomic<u_int> m_uGPUTimerCount{ 0 };                // claims this frame (workers fetch_add)
	u_int              m_uGPUTimerReadbackCount = 0;          // claims from this slot's last recording
	const char*        m_aszGPUTimerNames[uMAX_GPU_TIMERS] = {};
	u_int              m_auGPUTimerExecIndex[uMAX_GPU_TIMERS] = {};  // pass execution-order index per claimed slot
#endif

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
	// NEAREST/point filter, NO anisotropy, clamp addressing. For data textures
	// whose texels must be read EXACTLY (e.g. VAT vertex-animation position
	// textures: adjacent columns are different mesh vertices, adjacent rows are
	// different frames — any linear cross-blend corrupts positions).
	static void InitialisePointClamp(Zenith_Vulkan_Sampler& xSampler);

	vk::Sampler m_xSampler;
};

// Per-Engine state + behaviour for the Vulkan backend. Replaces both
// the static-facade `class Zenith_Vulkan` and the data-only
// `Zenith_VulkanImpl` that used to live in Zenith_VulkanImpl.h. Methods
// moved off the facade; data members moved off the Impl. Accessed via
// g_xEngine.FluxBackend().
class Zenith_Vulkan
{
public:
	Zenith_Vulkan() = default;
	~Zenith_Vulkan() = default;
	Zenith_Vulkan(const Zenith_Vulkan&) = delete;
	Zenith_Vulkan& operator=(const Zenith_Vulkan&) = delete;

	// ===== Format / descriptor helpers (called without a Vulkan instance) =====
	// These are pure functions but kept as members so call sites consistently
	// use g_xEngine.FluxBackend().X() rather than mixing static + instance forms.
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
	void InitialisePerFrameResources(); // Must be called after g_xEngine.FluxMemory().Initialise()
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
	// Queries VkPhysicalDeviceDescriptorIndexingProperties, clamps the bindless
	// table to the device's update-after-bind limits, and asserts the min-spec
	// floor. Must run after CreateDevice() and before CreateBindlessTexturesDescriptorPool().
	void QueryDescriptorIndexingLimits();
	void CreateBindlessTexturesDescriptorPool();
	// Phase 5.1: create the persistent GLOBAL/VIEW set layouts + pool and allocate the
	// per-frame-in-flight sets (called once at init, after the bindless pool).
	void CreatePersistentDescriptorSets();

#ifdef ZENITH_FLUX_PROFILING
	// GPU per-pass timestamp profiling accessors. Caps are read at device
	// creation (CreatePhysicalDevice / CreateQueueFamilies); GetCurrentFrame()
	// lets the command recorder reach the active slot's query pool / claim slot.
	Zenith_Vulkan_PerFrame* GetCurrentFrame() { return m_pxCurrentFrame; }
	bool  IsGPUTimestampsSupported() const { return m_bGPUTimestampsSupported; }
	float GetTimestampPeriod() const { return m_fTimestampPeriod; }
#endif

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

	// Per-frame begin work, called directly each frame by
	// Flux_RendererImpl::BeginFrame via the neutral Flux_PlatformAPI alias.
	// Wraps the wait-fence / reset-pools / drain-typed-deletion-queues logic.
	// uRingIndex is the current slot from the per-frame ring.
	void PerFrameBegin(u_int uRingIndex);

	// Record every queued render pass directly into the per-worker command
	// buffers (parallel worker task). Driven from Flux_RenderGraph::Execute via
	// the FluxBackendDevice contract, before the frame memory submit.
	void RecordFrame(const Flux_WorkDistribution& xWorkDistribution);

	// Phase 5.1: rewrite the current frame's persistent GLOBAL/VIEW descriptor sets
	// from the spine constant buffers — called once per frame on the main thread
	// (Flux_RendererImpl::RecordFrame), before any worker binds (write-before-bind).
	void PreparePersistentSets(Flux_BufferDescriptorHandle xGlobalCBV, Flux_BufferDescriptorHandle xMaterialsSSBO, Flux_BufferDescriptorHandle xViewCBV);

	// Phase 5.4: rewrite one VIEW-set image SRV (combined image sampler) into the
	// current frame's persistent VIEW set — called once per frame per promoted
	// view-frequency resource (CSM, etc.), right after PreparePersistentSets. The
	// descriptor is a stable pointer to the resource's view; the render graph's
	// per-pass Read() declarations (enforced by Flux_ViewSetBinding) drive the
	// layout/contents barriers, so writing it once at frame start is safe.
	void WritePersistentViewImage(u_int uBinding, const Flux_ShaderResourceView& xSRV, const Zenith_Vulkan_Sampler& xSampler);

	void EndFrame(bool bSubmitRenderWork);

	// Wait for GPU to finish all work (blocks until idle)
	// WARNING: This is expensive — only use for critical synchronization
	// (scene transitions, shutdown, etc.)
	void WaitForGPUIdle();

	// Task-system entry point. Stays static so it can be passed as a
	// Zenith_DataParallelTaskFunction; the body resolves the engine singleton.
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
	// Phase 5.1: shared persistent GLOBAL/VIEW set layouts (borrowed by every pipeline's
	// RootSig so sets 0/1 are layout-identical = prefix-compatible) + the current
	// frame-in-flight's persistent sets (bound by the command recorder).
	vk::DescriptorSetLayout GetGlobalSetLayout() const { return m_xGlobalSetLayout; }
	vk::DescriptorSetLayout GetViewSetLayout()   const { return m_xViewSetLayout; }
	vk::DescriptorSet GetCurrentGlobalSet() const { return m_pxCurrentFrame->m_xGlobalSet; }
	vk::DescriptorSet GetCurrentViewSet()   const { return m_pxCurrentFrame->m_xViewSet; }
	// Device-clamped bindless table capacity (set by QueryDescriptorIndexingLimits).
	// Drives Flux_BindlessAllocator's capacity. Backend-neutral surface (mirrored by
	// the D3D12 null backend).
	uint32_t GetBindlessTableSize() const { return m_uBindlessTableSize; }
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
	GPUCapabilities               m_xGPUCapabilities = {};
	uint32_t                      m_auQueueIndices[COMMANDTYPE_MAX] = {};
	vk::Device                    m_xDevice;
	vk::Queue                     m_axQueues[COMMANDTYPE_MAX];
	vk::CommandPool               m_axCommandPools[COMMANDTYPE_MAX];

	// Default + bindless descriptor pools.
	vk::DescriptorPool            m_xDefaultDescriptorPool;
	vk::DescriptorPool            m_xBindlessTexturesDescriptorPool;
	vk::DescriptorSet             m_xBindlessTexturesDescriptorSet;
	vk::DescriptorSetLayout       m_xBindlessTexturesDescriptorSetLayout;
	// Device-clamped capacity of the bindless table (set by QueryDescriptorIndexingLimits;
	// defaults to the legacy 1000 so any pre-query read is safe). Drives the pool size,
	// the layout descriptorCount, and the WriteBindlessDescriptor bounds check.
	uint32_t                      m_uBindlessTableSize = FLUX_BINDLESS_TABLE_SIZE_MIN;

	// Phase 5.1: persistent descriptor pool + GLOBAL/VIEW set layouts. The per-frame
	// sets (Zenith_Vulkan_PerFrame::m_xGlobalSet/m_xViewSet) are allocated from this pool
	// once at init and excluded from the per-frame reset.
	vk::DescriptorPool            m_xPersistentDescriptorPool;
	vk::DescriptorSetLayout       m_xGlobalSetLayout;
	vk::DescriptorSetLayout       m_xViewSetLayout;

	// VRAM registry + freelist of slots.
	std::vector<Zenith_Vulkan_VRAM*> m_xVRAMRegistry;
	std::vector<uint32_t>            m_xFreeVRAMHandles;

	// Pending command buffers awaiting submission (Phase 2 of graph execute).
	std::vector<const Zenith_Vulkan_CommandBuffer*> m_xPendingCommandBuffers;

	// Per-frame ring state (fences, semaphores, command-pool slots).
	Zenith_Vulkan_PerFrame        m_axPerFrame[MAX_FRAMES_IN_FLIGHT];
	Zenith_Vulkan_PerFrame*       m_pxCurrentFrame = nullptr;

#ifdef ZENITH_FLUX_PROFILING
	// GPU timestamp capabilities (read once at device creation). validBits == 0
	// means the graphics queue can't write timestamps, so GPU profiling stays a
	// clean no-op (no pool created, no writes, no readback).
	float m_fTimestampPeriod = 0.0f;        // ns per tick (limits.timestampPeriod)
	u_int m_uTimestampValidBits = 0;        // graphics-queue timestampValidBits
	bool  m_bGPUTimestampsSupported = false;
#endif

	// Transfer command buffer used for staging-buffer flushes.
	Zenith_Vulkan_CommandBuffer*  m_pxMemoryUpdateCmdBuf = nullptr;

	// Injected cross-subsystem dependencies (wired in Initialise). Stored as
	// member pointers so instance methods route through them rather than
	// reaching back through g_xEngine. NOT used by the static
	// RecordCommandBuffersTask callback — it has no 'this' and recovers the
	// singleton directly.
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