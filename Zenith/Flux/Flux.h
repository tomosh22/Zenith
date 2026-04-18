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
struct Flux_ShaderResourceView
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	bool m_bIsDepthStencil = false;  // True if this SRV is for a depth/stencil texture
	u_int m_uBaseMip = 0;            // Base mip level this SRV covers (for barrier tracking)
	u_int m_uMipCount = 1;           // Number of mip levels this SRV covers
};

struct Flux_UnorderedAccessView_Texture
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	u_int m_uMipLevel = 0;  // Mip level this UAV targets (for barrier tracking)
};

struct Flux_UnorderedAccessView_Buffer
{
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderTargetView
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_DepthStencilView
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_ConstantBufferView
{
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

struct Flux_RenderAttachment
{
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

#ifdef ZENITH_TOOLS
	std::string m_strName;
#endif

	Flux_ShaderResourceView m_xSRV;
	Flux_ShaderResourceView m_axMipSRVs[FLUX_MAX_MIPS];
	Flux_UnorderedAccessView_Texture m_axUAVs[FLUX_MAX_MIPS];
	Flux_RenderTargetView m_axRTVs[FLUX_MAX_MIPS];
	Flux_DepthStencilView m_xDSV;

	Flux_ShaderResourceView& SRV();
	const Flux_ShaderResourceView& SRV() const;
	Flux_ShaderResourceView& SRV(u_int uMip);
	const Flux_ShaderResourceView& SRV(u_int uMip) const;
	Flux_UnorderedAccessView_Texture& UAV(u_int uMip);
	Flux_RenderTargetView& RTV(u_int uMip = 0);
	Flux_DepthStencilView& DSV() { return m_xDSV; }
	const Flux_DepthStencilView& DSV() const { return m_xDSV; }
};

struct Flux_RenderAttachmentCube
{
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

#ifdef ZENITH_TOOLS
	std::string m_strName;
#endif

	Flux_ShaderResourceView m_xSRV;
	Flux_ShaderResourceView m_axSliceSRVs[FLUX_MAX_MIPS];
	Flux_UnorderedAccessView_Texture m_axUAVs[FLUX_MAX_MIPS];
	// Whole-mip layered RTVs — one per mip level, each view covers all 6 cube faces
	// as a 2D array (layerCount = 6). Useful for multi-view rendering that writes
	// every face in one draw.
	Flux_RenderTargetView m_axRTVs[FLUX_MAX_MIPS];
	// Per-(mip, face) RTVs — single-layer 2D views (layerCount = 1, viewType = 2D).
	// The render graph binds one of these per IBL face pass so the Vulkan backend
	// can bind a single subresource as the colour attachment.
	Flux_RenderTargetView m_aaxMipFaceRTVs[FLUX_MAX_MIPS][6];
	Flux_DepthStencilView m_xDSV;

	// Whole-cube SRV spanning every mip and all 6 layers. This is the view shaders
	// bind for cube sampling (e.g. textureLod(prefiltered, R, roughness*MAX_LOD)).
	Flux_ShaderResourceView& SRV();
	const Flux_ShaderResourceView& SRV() const;
	// Single-mip cube-slice SRV (still cube-typed, 6 layers, but restricted to one mip).
	// Used for debug viewers that step through the roughness mip chain. No default
	// argument — callers that want the whole-cube view must call SRV() explicitly
	// so the per-mip vs whole-cube choice is never silently wrong.
	Flux_ShaderResourceView& SRV(u_int uMip);
	const Flux_ShaderResourceView& SRV(u_int uMip) const;
	Flux_UnorderedAccessView_Texture& UAV(u_int uMip = 0);
	// Whole-mip layered RTV (all 6 faces, viewType 2D_ARRAY). See m_axRTVs.
	Flux_RenderTargetView& RTV(u_int uMip = 0);
	// Per-(mip, face) single-layer 2D RTV. Used by the IBL render graph, one face
	// at a time. See m_aaxMipFaceRTVs.
	Flux_RenderTargetView& RTV(u_int uMip, u_int uFace);
	const Flux_RenderTargetView& RTV(u_int uMip, u_int uFace) const;
	Flux_DepthStencilView& DSV() { return m_xDSV; }
	const Flux_DepthStencilView& DSV() const { return m_xDSV; }
};

struct Flux_Texture
{
	Flux_SurfaceInfo m_xSurfaceInfo;

	Flux_VRAMHandle m_xVRAMHandle;

	Flux_ShaderResourceView m_xSRV;

#ifdef ZENITH_TOOLS
	// Source path for serialization (set when loaded from file). Tools-only — shipping builds
	// don't carry asset-author metadata on every texture.
	std::string m_strSourcePath;
#endif
};

struct Flux_Buffer
{
	Flux_VRAMHandle m_xVRAMHandle;
	u_int64 m_ulSize = 0;
};

// TODO: add ray tracing extensions — Flux_AccelerationStructure (BLAS/TLAS),
// Flux_RayTracingPipelineSpecification, RAY_GEN/CLOSEST_HIT/MISS shader stages,
// and ResourceAccess::READ_ACCELERATION_STRUCTURE for the render graph.

// ---- Transient resource descriptors (render-graph-owned) ----------------
// Subsystems declare these instead of allocating their own Flux_RenderAttachment.
// The graph allocates the backing resource at Compile() time and may alias
// non-overlapping lifetimes in a future version.
//
// #TODO: Add Flux_TransientBufferDesc + Flux_RenderGraph::CreateTransientBuffer
// once a concrete use case emerges. The graph already emits buffer barriers
// (see Flux_RenderGraph::SynthesizeBarriers), so the missing piece is just
// graph-owned buffer allocation + aliasing pools. Audit existing
// Flux_ReadWriteBuffer / Flux_IndirectBuffer usage first — most of today's
// instances (HDR histogram, instance-group transforms, particle ping-pong
// buffers, etc.) carry data ACROSS frames and are NOT transient candidates.
// CBVs are explicitly out of scope: the per-frame scratch UBO path covers
// that case already and aliasing tiny constant buffers wins nothing.

struct Flux_TransientTextureDesc
{
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	u_int m_uDepth = 1;       // >1 for 3D textures
	TextureFormat m_eFormat = TEXTURE_FORMAT_RGBA8_SRGB;
	TextureType m_eTextureType = TEXTURE_TYPE_2D;
	u_int m_uNumMips = 1;
	u_int m_uMemoryFlags = 0; // MemoryFlags bitmask (e.g., 1u << MEMORY_FLAGS__SHADER_READ)
	bool m_bIsDepthStencil = false; // true → uses BuildDepthStencil instead of BuildColour
};

struct Flux_TransientHandle
{
	u_int m_uIndex = UINT32_MAX;
	// Generation of the Flux_RenderGraph that issued this handle. Compared in
	// ReadTransient / WriteTransient / GetTransientAttachment and asserts if
	// the graph has been Clear()'d / rebuilt since — catches "cached handle
	// from last compile" bugs that would otherwise silently reference a
	// deallocated transient slot or the wrong slot after re-ordering.
	u_int m_uGeneration = 0;
	// Per-graph-instance ID (monotonic counter on Flux_RenderGraph). Ensures
	// two graphs that happen to share (index, generation) issue handles that
	// still compare unequal — matters for unit tests and any future path that
	// holds multiple graphs live concurrently. 0 is the "no graph" sentinel.
	u_int m_uGraphInstanceID = 0;
	bool IsValid() const { return m_uIndex != UINT32_MAX; }
	// Equality is by index + generation + graph-instance. Handles from
	// different graphs or different generations of the same graph never compare
	// equal even if they share an index.
	bool operator==(const Flux_TransientHandle& rhs) const { return m_uIndex == rhs.m_uIndex && m_uGeneration == rhs.m_uGeneration && m_uGraphInstanceID == rhs.m_uGraphInstanceID; }
	bool operator!=(const Flux_TransientHandle& rhs) const { return !(*this == rhs); }
};

// Opaque, type-safe pass handle. Replaces the raw `u_int uPassIndex` that
// Flux_RenderGraph::AddPass used to return. A Flux_PassHandle carries the
// graph's generation counter so that passing a stale handle (e.g. one saved
// across a graph rebuild on window resize) trips an assertion instead of
// silently addressing a different pass.
struct Flux_PassHandle
{
	u_int m_uIndex = UINT32_MAX;
	u_int m_uGeneration = 0;
	// Per-graph-instance ID (see Flux_TransientHandle::m_uGraphInstanceID).
	u_int m_uGraphInstanceID = 0;
	bool IsValid() const { return m_uIndex != UINT32_MAX; }
	// Equality is by index + generation + graph-instance. Two handles with the
	// same index but different generations (e.g. one from before Clear(), one
	// from after) are NOT equal; neither are handles from different graphs.
	bool operator==(const Flux_PassHandle& rhs) const { return m_uIndex == rhs.m_uIndex && m_uGeneration == rhs.m_uGeneration && m_uGraphInstanceID == rhs.m_uGraphInstanceID; }
	bool operator!=(const Flux_PassHandle& rhs) const { return !(*this == rhs); }
};

// ---- Render graph resource tagging --------------------------------------
// Defined in Flux.h (not Flux_RenderGraph.h) so that Flux_CommandListEntry
// can embed a Flux_RenderGraph_AttachmentRef carrier without introducing
// a circular dependency between Flux.h and the render graph header.

enum class Flux_GraphResourceKind : u_int
{
	Image,
	ImageCube,
	Buffer,
};

// Tagged-pointer wrapper so the render graph can uniformly track 2D images,
// cubemap images, and buffers. Hashing/equality is pointer-based (plus kind,
// to avoid the theoretical cross-kind pointer alias).
class Flux_GraphResource
{
public:
	Flux_GraphResource() = default;
	Flux_GraphResource(Flux_RenderAttachment& xImage) : m_eKind(Flux_GraphResourceKind::Image), m_pxImage(&xImage) {}
	Flux_GraphResource(Flux_RenderAttachmentCube& xImageCube) : m_eKind(Flux_GraphResourceKind::ImageCube), m_pxImageCube(&xImageCube) {}
	Flux_GraphResource(Flux_Buffer& xBuffer) : m_eKind(Flux_GraphResourceKind::Buffer), m_pxBuffer(&xBuffer) {}

	Flux_GraphResourceKind GetKind() const { return m_eKind; }
	bool IsImageLike() const { return m_eKind == Flux_GraphResourceKind::Image || m_eKind == Flux_GraphResourceKind::ImageCube; }
	bool IsValid() const { return m_pxImage != nullptr; } // any union member aliases the same bytes

	Flux_RenderAttachment* AsImage() const
	{
		Zenith_Assert(m_eKind == Flux_GraphResourceKind::Image, "Flux_GraphResource: called AsImage() on non-Image kind");
		return m_pxImage;
	}

	Flux_RenderAttachmentCube* AsImageCube() const
	{
		Zenith_Assert(m_eKind == Flux_GraphResourceKind::ImageCube, "Flux_GraphResource: called AsImageCube() on non-ImageCube kind");
		return m_pxImageCube;
	}

	Flux_Buffer* AsBuffer() const
	{
		Zenith_Assert(m_eKind == Flux_GraphResourceKind::Buffer, "Flux_GraphResource: called AsBuffer() on non-Buffer kind");
		return m_pxBuffer;
	}

	Flux_VRAMHandle GetVRAMHandle() const
	{
		Zenith_Assert(IsImageLike(), "Flux_GraphResource::GetVRAMHandle: only valid for image kinds");
		return m_eKind == Flux_GraphResourceKind::Image ? m_pxImage->m_xVRAMHandle : m_pxImageCube->m_xVRAMHandle;
	}

	const Flux_SurfaceInfo& GetSurfaceInfo() const
	{
		Zenith_Assert(IsImageLike(), "Flux_GraphResource::GetSurfaceInfo: only valid for image kinds");
		return m_eKind == Flux_GraphResourceKind::Image ? m_pxImage->m_xSurfaceInfo : m_pxImageCube->m_xSurfaceInfo;
	}

	const std::string& GetName() const
	{
		static const std::string s_strBufferName = "<buffer>";
#ifdef ZENITH_TOOLS
		if (m_eKind == Flux_GraphResourceKind::Image)     return m_pxImage->m_strName;
		if (m_eKind == Flux_GraphResourceKind::ImageCube) return m_pxImageCube->m_strName;
#else
		static const std::string s_strReleaseName = "<release>";
		if (IsImageLike()) return s_strReleaseName;
#endif
		return s_strBufferName;
	}

	void* GetVoidPtr() const
	{
		switch (m_eKind)
		{
			case Flux_GraphResourceKind::Image:     return static_cast<void*>(m_pxImage);
			case Flux_GraphResourceKind::ImageCube: return static_cast<void*>(m_pxImageCube);
			case Flux_GraphResourceKind::Buffer:    return static_cast<void*>(m_pxBuffer);
		}
		return nullptr;
	}

	u_int64 GetHash() const
	{
		// Mix the kind into the top byte so two kinds whose pointers alias
		// (extremely unlikely but theoretically possible) hash separately.
		return reinterpret_cast<u_int64>(GetVoidPtr()) ^ (static_cast<u_int64>(m_eKind) << 56);
	}

	bool operator==(const Flux_GraphResource& rhs) const
	{
		return m_eKind == rhs.m_eKind && GetVoidPtr() == rhs.GetVoidPtr();
	}
	bool operator!=(const Flux_GraphResource& rhs) const { return !(*this == rhs); }

private:
	Flux_GraphResourceKind m_eKind = Flux_GraphResourceKind::Image;
	union
	{
		Flux_RenderAttachment*     m_pxImage = nullptr;
		Flux_RenderAttachmentCube* m_pxImageCube;
		Flux_Buffer*               m_pxBuffer;
	};
};

// Carrier used by pass metadata and Flux_CommandListEntry to identify which
// single subresource of an attachment gets bound as a render target for this
// pass. For 2D attachments, m_uLayer is always 0.
struct Flux_RenderGraph_AttachmentRef
{
	Flux_GraphResource m_xResource;
	uint16_t m_uMip = 0;
	uint16_t m_uLayer = 0;

	Flux_RenderGraph_AttachmentRef() = default;
	Flux_RenderGraph_AttachmentRef(const Flux_GraphResource& xRes, u_int uMip = 0, u_int uLayer = 0)
		: m_xResource(xRes), m_uMip(static_cast<uint16_t>(uMip)), m_uLayer(static_cast<uint16_t>(uLayer)) {}

	bool IsValid() const { return m_xResource.IsValid(); }
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
	void BuildColourCubemap(Flux_RenderAttachmentCube& xAttachment, const std::string& strName);
	void BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName);

	// Aliased variants — skip the CreateRenderTargetVRAM call and use a
	// pre-created aliased-image VRAM handle (from CreateAliasedImageVRAM).
	// Used by the render graph's transient-aliasing path in AllocateTransients
	// when SetAliasingEnabled(true) and the backend reports aliasing support.
	// Everything else (view creation, surface info, tools name) runs identically
	// to the non-aliased path; the only difference is who owns the underlying
	// VkImage allocation.
	void BuildColourFromAliasedVRAM(Flux_RenderAttachment& xAttachment,
	                                const std::string& strName,
	                                Flux_VRAMHandle xAliasedVRAM);
	void BuildDepthStencilFromAliasedVRAM(Flux_RenderAttachment& xAttachment,
	                                       const std::string& strName,
	                                       Flux_VRAMHandle xAliasedVRAM);

	// Queue the attachment's VRAM and every owned image view for deferred GPU-safe
	// deletion and reset the attachment to a default-constructed state. Safe to
	// call on an already-cleared attachment (no-op). The Build* methods above use
	// these internally to make rebuild-on-resize idempotent; external callers
	// (e.g. subsystem Shutdown() methods) should use these instead of hand-rolling
	// QueueVRAMDeletion / QueueImageViewDeletion loops — missing a view in a hand-
	// rolled teardown silently leaks GPU memory.
	static void Destroy(Flux_RenderAttachment& xAttachment);
	static void Destroy(Flux_RenderAttachmentCube& xAttachment);
};

struct Flux_RenderGraph_Pass;

struct Flux_CommandListEntry
{
	const Flux_CommandList* m_pxCmdList = nullptr;
	Flux_RenderGraph_AttachmentRef m_axColourAttachments[FLUX_MAX_TARGETS];
	uint32_t m_uNumColourAttachments = 0;
	Flux_RenderGraph_AttachmentRef m_xDepthStencil;
	const Flux_RenderGraph_Pass* m_pxPass = nullptr;
	bool m_bClearTargets = false;
	bool m_bDepthIsReadOnly = false;
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
	// the source pass pointer carries the precomputed prologue barriers
	// (pxPass->m_xPrologueBarriers) the platform layer emits via
	// ImageTransition right before the pass executes. No bypass path exists.
	static void SubmitCommandList(const Flux_CommandList* pxCmdList,
		const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
		const Flux_RenderGraph_AttachmentRef& xDepthStencil,
		bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass)
	{
		Zenith_Assert(pxCmdList != nullptr, "SubmitCommandList: Command list is null");
		Zenith_Assert(pxPass != nullptr, "SubmitCommandList: pass pointer is null — bypass path no longer supported");
		Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SubmitCommandList: must be called from the main thread (Flux_RenderGraph::Execute Phase 2)");
		Flux_CommandListEntry xEntry;
		xEntry.m_pxCmdList = pxCmdList;
		for (uint32_t i = 0; i < uNumColour; i++) xEntry.m_axColourAttachments[i] = axColourAttachments[i];
		xEntry.m_uNumColourAttachments = uNumColour;
		xEntry.m_xDepthStencil = xDepthStencil;
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

	// Called every frame from Zenith_Core::ExecuteRenderGraph before Compile.
	// Forwards the current value of debug variables that the render graph
	// cares about (e.g. transient aliasing toggle) into the graph via their
	// respective setters; the setters MarkDirty on change, so toggling the
	// editor variable mid-frame takes effect on the next Compile rather
	// than waiting for the next SetupRenderGraph (which only runs on resize).
	static void SyncRenderGraphDebugToggles();

	// Request a full graph rebuild (Clear + SetupRenderGraph) at the start of
	// the next frame. Safe to call from execute callbacks — the rebuild is
	// deferred until before the next Compile()/Execute() cycle.
	static void RequestGraphRebuild() { s_bGraphRebuildRequested = true; }
	static bool ConsumeGraphRebuildRequest() { bool b = s_bGraphRebuildRequested; s_bGraphRebuildRequested = false; return b; }

	// Public access to pending command lists for platform layer.
	// Inserted in topological order by Flux_RenderGraph::Execute Phase 2 only.
	static Zenith_Vector<Flux_CommandListEntry> s_xPendingCommandLists;
private:
	friend class Flux_PlatformAPI;

	static uint32_t s_uFrameCounter;
	static Zenith_Vector<void(*)()> s_xResChangeCallbacks;
	static Flux_RenderGraph* s_pxRenderGraph;
	static bool s_bGraphRebuildRequested;
};

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
	TextureFormat m_eDepthStencilFormat = TEXTURE_FORMAT_NONE;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	TextureFormat m_aeColourAttachmentFormats[FLUX_MAX_TARGETS] = {};
	uint32_t m_uNumColourAttachments = 0;
	LoadAction m_eColourLoadAction;
	LoadAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	LoadAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;

	CullMode m_eCullMode = CULL_MODE_NONE;  // No culling by default (matches previous hardcoded behavior)

	bool m_bDepthBias = false;
	float m_fDepthBiasConstant = 0.0f;
	float m_fDepthBiasSlope = 0.0f;
	float m_fDepthBiasClamp = 0.0f;

	// Dynamic state flags — when set, the value in this spec is the initial/default,
	// but can be overridden per-draw via Flux_CommandSetCullMode / Flux_CommandSetDepthBias.
	// Requires VK_EXT_extended_dynamic_state (Vulkan 1.3 core).
	bool m_bDynamicCullMode = false;
	bool m_bDynamicDepthBias = false;
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
		TextureFormat eColourFormat,
		TextureFormat eDepthStencilFormat = TEXTURE_FORMAT_NONE,
		const char* szVertShader = "Flux_Fullscreen_UV.vert");

	// Creates a pre-populated fullscreen spec without building.
	// Use when you need to customise blend states or other settings before building.
	static Flux_PipelineSpecification CreateFullscreenSpec(
		Flux_Shader& xShader,
		const char* szFragShader,
		TextureFormat eColourFormat,
		TextureFormat eDepthStencilFormat = TEXTURE_FORMAT_NONE,
		const char* szVertShader = "Flux_Fullscreen_UV.vert");
};