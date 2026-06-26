#pragma once

// GPU render resources: cubemap attachments, sampled textures, raw buffers, and
// the builder that allocates/destroys render attachments. Split out of Flux.h.
#include "Flux/Flux_Types.h"   // Flux_RenderAttachment, Flux_RenderAttachmentCube deps:
                               // Flux_SurfaceInfo, Flux_VRAMHandle, view types, enums, FLUX_MAX_MIPS

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
	// Array layers for a 2D array attachment (e.g. the 4-cascade CSM depth array).
	// Honored by BuildDepthStencil today (per-layer DSVs + a whole-array SRV);
	// the colour/cubemap paths set their own layer count. 1 = ordinary 2D.
	uint32_t m_uNumLayers = 1;

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

private:
	// Shared body of BuildColour / BuildColourFromAliasedVRAM. Caller passes a
	// fully-formed VRAM handle — freshly allocated or borrowed from an aliased
	// pool. szDiagPrefix is "" for the standard variant or "Aliased " for the
	// aliased one; it's interpolated into the diagnostic log line.
	void BuildColourImpl(Flux_RenderAttachment& xAttachment, const std::string& strName, Flux_VRAMHandle xVRAM, const char* szDiagPrefix);
};
