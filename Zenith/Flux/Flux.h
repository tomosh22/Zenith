#pragma once

#include "Collections/Zenith_Vector.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_CommandList.h"
#include "Multithreading/Zenith_Multithreading.h"

struct Flux_SurfaceInfo
{
	TextureFormat m_eFormat = TEXTURE_FORMAT_NONE;
	TextureType m_eTextureType = TEXTURE_TYPE_2D;
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	u_int m_uDepth = 1;  // Used for 3D textures
	u_int m_uNumMips = 1;
	u_int m_uNumLayers = 1;  // Minimum 1 for valid Vulkan image
	u_int m_uBaseLayer = 0;  // Base array layer for render target views (used for cubemap faces)
	u_int m_uBaseMip = 0;    // Base mip level for render target views (used for roughness mip chain)
	u_int m_uMemoryFlags = MEMORY_FLAGS__NONE;
};

// View structures for Direct3D-style resource views
// Use opaque handles to abstract away Vulkan types from Flux layer
struct Flux_ShaderResourceView {
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	bool m_bIsDepthStencil = false;  // True if this SRV is for a depth/stencil texture
	u_int m_uBaseMip = 0;            // Base mip level this SRV covers (for barrier tracking)
	u_int m_uMipCount = 1;           // Number of mip levels this SRV covers
};

struct Flux_UnorderedAccessView_Texture {
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	u_int m_uMipLevel = 0;  // Mip level this UAV targets (for barrier tracking)
};

struct Flux_UnorderedAccessView_Buffer {
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderTargetView {
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_DepthStencilView {
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_ConstantBufferView {
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderAttachment {
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

	// Views for different usage patterns
	Flux_ShaderResourceView m_pxSRV;  // For reading in shaders
	Flux_UnorderedAccessView_Texture m_pxUAV; // For compute shader read/write
	Flux_RenderTargetView m_pxRTV;     // For rendering (color attachments)
	Flux_DepthStencilView m_pxDSV;     // For depth/stencil attachments

	// Cubemap face RTVs (for rendering to individual cubemap faces)
	// Only valid when m_xSurfaceInfo.m_eTextureType == TEXTURE_TYPE_CUBE
	Flux_RenderTargetView m_axFaceRTVs[6];

	// Cubemap face SRVs (for debug display of individual faces)
	// Only valid when m_xSurfaceInfo.m_eTextureType == TEXTURE_TYPE_CUBE
	Flux_ShaderResourceView m_axFaceSRVs[6];
};

struct Flux_Texture
{
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

	Flux_ShaderResourceView m_xSRV;

	// Source path for serialization (set when loaded from file)
	std::string m_strSourcePath;
};

struct Flux_Buffer
{
	Flux_VRAMHandle m_xVRAMHandle;
	u_int64 m_ulSize = 0;
};


class Flux_RenderAttachmentBuilder {
public:
	Flux_RenderAttachmentBuilder() = default;
	TextureFormat m_eFormat = TEXTURE_FORMAT_NONE;
	TextureType m_eTextureType = TEXTURE_TYPE_2D;
	u_int m_uMemoryFlags = MEMORY_FLAGS__NONE;

	uint32_t m_uWidth = 1;
	uint32_t m_uHeight = 1;
	uint32_t m_uDepth = 1;  // For 3D textures
	uint32_t m_uNumMips = 1;  // Number of mip levels (for cubemaps with roughness mips)

	void BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName);
	void BuildColourCubemap(Flux_RenderAttachment& xAttachment, const std::string& strName);
	void BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName);
};

struct Flux_TargetSetup {
	Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS];

	//#TO not owned by this
	Flux_RenderAttachment* m_pxDepthStencil = nullptr;

	std::string m_strName;

	void AssignDepthStencil(Flux_RenderAttachment* pxDS);

	uint32_t GetNumColourAttachments() const;

	bool operator==(const Flux_TargetSetup& xOther) const
	{
		for (u_int u = 0; u < FLUX_MAX_TARGETS; u++)
		{
			if (m_axColourAttachments[u].m_xVRAMHandle.AsUInt() != xOther.m_axColourAttachments[u].m_xVRAMHandle.AsUInt())
			{
				return false;
			}
			// Also compare RTV handles to distinguish cubemap faces (same VRAM, different layer views)
			if (m_axColourAttachments[u].m_pxRTV.m_xImageViewHandle.AsUInt() != xOther.m_axColourAttachments[u].m_pxRTV.m_xImageViewHandle.AsUInt())
			{
				return false;
			}
		}
		return m_pxDepthStencil == xOther.m_pxDepthStencil;
	}

	bool operator!=(const Flux_TargetSetup& xOther) const
	{
		return !(*this == xOther);
	}
};

struct Flux_RenderGraph_Pass;

// Entry for a command list submitted by the render graph. All fields are
// non-owning pointers into graph-owned storage — lifetimes span a single frame.
struct Flux_CommandListEntry
{
	const Flux_CommandList* m_pxCmdList = nullptr;
	const Flux_TargetSetup* m_pxTargetSetup = nullptr;
	const Flux_RenderGraph_Pass* m_pxPass = nullptr;  // Carries precomputed prologue barriers
	bool m_bClearTargets = false;                     // Single source of truth for clear
	bool m_bDepthIsReadOnly = false;                  // Pass declares depth as a read attachment (no writes)
};

// Work distribution indices for parallel Vulkan command buffer recording
struct Flux_WorkDistribution
{
	u_int auStartIndex[FLUX_NUM_WORKER_THREADS];
	u_int auEndIndex[FLUX_NUM_WORKER_THREADS];
	u_int uTotalCommandCount;

	void Clear()
	{
		for (u_int i = 0; i < FLUX_NUM_WORKER_THREADS; i++)
		{
			auStartIndex[i] = 0;
			auEndIndex[i] = 0;
		}
		uTotalCommandCount = 0;
	}
};

class Flux_RenderGraph;

class Flux
{
public:
	Flux() = delete;
	~Flux() = delete;
	static void EarlyInitialise();
	static void LateInitialise();
	static void Shutdown();

	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }

	// Submit a command list for Vulkan recording. Only called from
	// Flux_RenderGraph::Execute Phase 2, sequentially on the main thread —
	// the source pass pointer carries the precomputed barriers the platform
	// layer consumes via ConsumeGraphPrologueBarriers. No bypass path exists.
	static void SubmitCommandList(const Flux_CommandList* pxCmdList, const Flux_TargetSetup& xTargetSetup,
		bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass)
	{
		Zenith_Assert(pxCmdList != nullptr, "SubmitCommandList: Command list is null");
		Zenith_Assert(pxPass != nullptr, "SubmitCommandList: pass pointer is null — bypass path no longer supported");
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SubmitCommandList: must be called from the main thread (Flux_RenderGraph::Execute Phase 2)");
		Flux_CommandListEntry xEntry;
		xEntry.m_pxCmdList = pxCmdList;
		xEntry.m_pxTargetSetup = &xTargetSetup;
		xEntry.m_pxPass = pxPass;
		xEntry.m_bClearTargets = bClearTargets;
		xEntry.m_bDepthIsReadOnly = bDepthIsReadOnly;
		s_xPendingCommandLists.PushBack(xEntry);
	}

	// Prepare frame for rendering - distributes work across worker threads
	// Returns false if there is no work to do
	static bool PrepareFrame(Flux_WorkDistribution& xOutDistribution);

	static void AddResChangeCallback(void(*pfnCallback)()) { s_xResChangeCallbacks.PushBack(pfnCallback); }
	static void OnResChange();

	// Clear all pending command lists. CALLER GUARANTEES that no worker thread
	// is currently submitting (i.e. graph Execute is not in flight) and that
	// the GPU has finished consuming the previous frame's lists. Called from:
	//   - Main thread between frames during res change / scene transition.
	// This function intentionally does NOT take s_xPendingCommandListMutex
	// because the contract above means there is no contender to lock against.
	static void ClearPendingCommandLists()
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread(),
			"ClearPendingCommandLists: main-thread only");
		s_xPendingCommandLists.Clear();
	}

	static Flux_RenderGraph& GetRenderGraph() { return *s_pxRenderGraph; }
	static void SetupRenderGraph();

	// Public access to pending command lists for platform layer.
	// Inserted in topological order by Flux_RenderGraph::Execute Phase 2 only.
	static Zenith_Vector<Flux_CommandListEntry> s_xPendingCommandLists;
private:
	friend class Flux_PlatformAPI;

	static uint32_t s_uFrameCounter;
	static Zenith_Vector<void(*)()> s_xResChangeCallbacks;
	static Flux_RenderGraph* s_pxRenderGraph;
};

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
	TextureFormat m_eDepthStencilFormat;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	struct Flux_TargetSetup* m_pxTargetSetup;
	LoadAction m_eColourLoadAction;
	StoreAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	StoreAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;

	CullMode m_eCullMode = CULL_MODE_NONE;  // No culling by default (matches previous hardcoded behavior)

	bool m_bDepthBias = false;
	float m_fDepthBiasConstant = 0.0f;
	float m_fDepthBiasSlope = 0.0f;
	float m_fDepthBiasClamp = 0.0f;
};

// Helper to reduce boilerplate when creating fullscreen post-processing pipelines.
// The pattern of init shader -> create spec -> populate layout -> build pipeline
// is repeated 10+ times across Flux subsystems.
class Flux_PipelineHelper
{
public:
	Flux_PipelineHelper() = delete;

	// Initialises a shader and builds a fullscreen pipeline with no depth test/write.
	// Covers the common case used by HDR, SSR, SSGI, IBL, etc.
	static void BuildFullscreenPipeline(
		Flux_Shader& xShader,
		Flux_Pipeline& xPipeline,
		const char* szFragShader,
		Flux_TargetSetup* pxTargetSetup,
		const char* szVertShader = "Flux_Fullscreen_UV.vert");

	// Creates a pre-populated fullscreen spec without building.
	// Use when you need to customise blend states or other settings before building.
	static Flux_PipelineSpecification CreateFullscreenSpec(
		Flux_Shader& xShader,
		const char* szFragShader,
		Flux_TargetSetup* pxTargetSetup,
		const char* szVertShader = "Flux_Fullscreen_UV.vert");
};
