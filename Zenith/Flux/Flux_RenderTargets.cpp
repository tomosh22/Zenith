#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RenderTargets.h"

// -----------------------------------------------------------------------------
// Attachment teardown helpers
//
// Exposed as Flux_RenderAttachmentBuilder::Destroy(...) so both Build* (for
// idempotent rebuild on window resize) and external subsystem Shutdown()
// methods share a single correct teardown path. Any pre-existing VRAM
// allocation and image views owned by the attachment are queued for deferred
// deletion; deferred deletion is mandatory because the old VRAM / views may
// still be in flight on the GPU from the previous frame — see
// Vulkan/CLAUDE.md "GPU Resource Lifecycle".
//
// After release the attachment is reset to a default-constructed state so the
// subsequent fresh Build code (or next observer) sees a clean slate.
// -----------------------------------------------------------------------------

void Flux_RenderAttachmentBuilder::Destroy(Flux_RenderAttachment& xAttachment)
{
	if (!xAttachment.m_xVRAMHandle.IsValid()) return;

	const u_int uNumMips = xAttachment.m_xSurfaceInfo.m_uNumMips;
	auto& xVulkanMemory = g_xEngine.FluxMemory();

	// The 2D builder creates up to one RTV per mip and up to one per-mip SRV
	// per mip. QueueVRAMDeletion accepts at most four individual view handles,
	// so everything beyond mip 0 (RTV / UAV) and everything in m_axMipSRVs
	// must be queued individually via QueueImageViewDeletion.
	for (u_int uMip = 1; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axRTVs[uMip].m_xImageViewHandle);
	}
	for (u_int uMip = 0; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axMipSRVs[uMip].m_xImageViewHandle);
	}
	for (u_int uMip = 1; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axUAVs[uMip].m_xImageViewHandle);
	}

	// Per-layer DSVs for an array depth attachment (CSM cascades). Invalid handles
	// are a no-op, so this is safe for single-layer attachments (which use m_xDSV).
	for (u_int uLayer = 0; uLayer < xAttachment.m_xSurfaceInfo.m_uNumLayers && uLayer < FLUX_MAX_ATTACHMENT_LAYERS; uLayer++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axLayerDSVs[uLayer].m_xImageViewHandle);
	}

	// VRAM + the "primary" views (first of each type) go in one bundle.
	// QueueVRAMDeletion auto-invalidates xAttachment.m_xVRAMHandle.
	xVulkanMemory.QueueVRAMDeletion(xAttachment.m_xVRAMHandle,
		xAttachment.m_axRTVs[0].m_xImageViewHandle,
		xAttachment.m_xDSV.m_xImageViewHandle,
		xAttachment.m_xSRV.m_xImageViewHandle,
		xAttachment.m_axUAVs[0].m_xImageViewHandle);

	// Zero the attachment so the caller's fresh Build code sees a clean slate
	// (all view handles default-constructed / invalid, surface info defaulted).
	xAttachment = Flux_RenderAttachment{};
}

void Flux_RenderAttachmentBuilder::Destroy(Flux_RenderAttachmentCube& xAttachment)
{
	if (!xAttachment.m_xVRAMHandle.IsValid()) return;

	const u_int uNumMips = xAttachment.m_xSurfaceInfo.m_uNumMips;
	auto& xVulkanMemory = g_xEngine.FluxMemory();

	// Per-mip slice SRVs (one per mip, all populated by BuildColourCubemap).
	for (u_int uMip = 0; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axSliceSRVs[uMip].m_xImageViewHandle);
	}

	// Per-mip whole-cube RTVs (mip 0 goes with the VRAM bundle below).
	for (u_int uMip = 1; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axRTVs[uMip].m_xImageViewHandle);
	}

	// Per-(mip, face) single-layer RTVs — the render graph's IBL pass workhorses.
	for (u_int uMip = 0; uMip < uNumMips; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			xVulkanMemory.QueueImageViewDeletion(xAttachment.m_aaxMipFaceRTVs[uMip][uFace].m_xImageViewHandle);
		}
	}

	// Per-mip UAVs (only created when UNORDERED_ACCESS is set; invalid handles
	// are a no-op inside QueueImageViewDeletion).
	for (u_int uMip = 1; uMip < uNumMips; uMip++)
	{
		xVulkanMemory.QueueImageViewDeletion(xAttachment.m_axUAVs[uMip].m_xImageViewHandle);
	}

	// VRAM + the "primary" views (first of each type) in one bundle. Cubes never
	// have a DSV, so the DSV slot is passed as an invalid default.
	xVulkanMemory.QueueVRAMDeletion(xAttachment.m_xVRAMHandle,
		xAttachment.m_axRTVs[0].m_xImageViewHandle,
		Flux_ImageViewHandle(),
		xAttachment.m_xSRV.m_xImageViewHandle,
		xAttachment.m_axUAVs[0].m_xImageViewHandle);

	xAttachment = Flux_RenderAttachmentCube{};
}

Flux_ShaderResourceView& Flux_RenderAttachment::SRV()
{
	return m_xSRV;
}

const Flux_ShaderResourceView& Flux_RenderAttachment::SRV() const
{
	return m_xSRV;
}

Flux_ShaderResourceView& Flux_RenderAttachment::SRV(u_int uMip)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axMipSRVs[uMip];
}

const Flux_ShaderResourceView& Flux_RenderAttachment::SRV(u_int uMip) const
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axMipSRVs[uMip];
}

Flux_UnorderedAccessView_Texture& Flux_RenderAttachment::UAV(u_int uMip)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::UAV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axUAVs[uMip];
}

Flux_RenderTargetView& Flux_RenderAttachment::RTV(u_int uMip)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachment::RTV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axRTVs[uMip];
}

Flux_DepthStencilView& Flux_RenderAttachment::DSV(u_int uLayer)
{
	Zenith_Assert(uLayer < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::DSV: layer %u out of bounds (num layers = %u)", uLayer, m_xSurfaceInfo.m_uNumLayers);
	Zenith_Assert(uLayer < FLUX_MAX_ATTACHMENT_LAYERS, "Flux_RenderAttachment::DSV: layer %u exceeds FLUX_MAX_ATTACHMENT_LAYERS (%u)", uLayer, FLUX_MAX_ATTACHMENT_LAYERS);
	return m_axLayerDSVs[uLayer];
}

const Flux_DepthStencilView& Flux_RenderAttachment::DSV(u_int uLayer) const
{
	Zenith_Assert(uLayer < m_xSurfaceInfo.m_uNumLayers, "Flux_RenderAttachment::DSV: layer %u out of bounds (num layers = %u)", uLayer, m_xSurfaceInfo.m_uNumLayers);
	Zenith_Assert(uLayer < FLUX_MAX_ATTACHMENT_LAYERS, "Flux_RenderAttachment::DSV: layer %u exceeds FLUX_MAX_ATTACHMENT_LAYERS (%u)", uLayer, FLUX_MAX_ATTACHMENT_LAYERS);
	return m_axLayerDSVs[uLayer];
}

Flux_ShaderResourceView& Flux_RenderAttachmentCube::SRV()
{
	// Whole-cube SRV: all mips, all 6 layers. This is what shaders bind for cube
	// sampling (e.g. textureLod(prefiltered, R, roughness*MAX_LOD)).
	return m_xSRV;
}

const Flux_ShaderResourceView& Flux_RenderAttachmentCube::SRV() const
{
	return m_xSRV;
}

Flux_ShaderResourceView& Flux_RenderAttachmentCube::SRV(u_int uMip)
{
	// Single-mip cube-slice SRV: still cube-typed (6 layers) but restricted to one
	// mip. Populated by BuildColourCubemap into m_axSliceSRVs. Use this for debug
	// viewers that step through the roughness mip chain. For normal shader sampling
	// across all mips, call SRV() with no argument instead.
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axSliceSRVs[uMip];
}

const Flux_ShaderResourceView& Flux_RenderAttachmentCube::SRV(u_int uMip) const
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::SRV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axSliceSRVs[uMip];
}

Flux_UnorderedAccessView_Texture& Flux_RenderAttachmentCube::UAV(u_int uMip)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::UAV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axUAVs[uMip];
}

// Whole-mip layered RTV: a single view that covers all 6 cube faces of this mip
// (viewType = 2D_ARRAY, layerCount = 6 — see Flux_MemoryManager::CreateRenderTargetView).
// Suitable for multi-view rendering where a single draw writes every face; NOT used
// by the IBL render graph today, which binds one face at a time — see the two-arg
// overload below.
Flux_RenderTargetView& Flux_RenderAttachmentCube::RTV(u_int uMip)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::RTV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	return m_axRTVs[uMip];
}

// Per-(mip, face) RTV: a single-layer 2D view of one face of one mip (viewType = 2D,
// layerCount = 1, baseArrayLayer = uFace — see CreateRenderTargetViewForLayer).
// This is what the render graph binds as the colour attachment for each of the 48
// IBL face passes (6 irradiance + 7 mips × 6 prefilter faces), one vkCmdBeginRenderPass
// per face.
Flux_RenderTargetView& Flux_RenderAttachmentCube::RTV(u_int uMip, u_int uFace)
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::RTV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	Zenith_Assert(uFace < 6, "Flux_RenderAttachmentCube::RTV: face index %u out of bounds (cubemap has 6 faces)", uFace);
	return m_aaxMipFaceRTVs[uMip][uFace];
}

const Flux_RenderTargetView& Flux_RenderAttachmentCube::RTV(u_int uMip, u_int uFace) const
{
	Zenith_Assert(uMip < m_xSurfaceInfo.m_uNumMips, "Flux_RenderAttachmentCube::RTV: mip index %u out of bounds (num mips = %u)", uMip, m_xSurfaceInfo.m_uNumMips);
	Zenith_Assert(uFace < 6, "Flux_RenderAttachmentCube::RTV: face index %u out of bounds (cubemap has 6 faces)", uFace);
	return m_aaxMipFaceRTVs[uMip][uFace];
}

// Shared colour-attachment build core. Caller supplies a fully-formed
// Flux_VRAMHandle — either freshly allocated (BuildColour) or borrowed from
// an aliased pool (BuildColourFromAliasedVRAM). All view creation, surface
// info population, and diagnostic logging is identical between the two paths.
void Flux_RenderAttachmentBuilder::BuildColourImpl(Flux_RenderAttachment& xAttachment, const std::string& strName, Flux_VRAMHandle xVRAM, const char* szDiagPrefix)
{
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth       = m_uWidth;
	xInfo.m_uHeight      = m_uHeight;
	xInfo.m_uDepth       = m_uDepth;
	xInfo.m_eFormat      = m_eFormat;
	xInfo.m_eTextureType = m_eTextureType;
	xInfo.m_uNumMips     = m_uNumMips;
	xInfo.m_uNumLayers   = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	xAttachment.m_xVRAMHandle = xVRAM;
	xAttachment.m_xSurfaceInfo = xInfo;
#ifdef ZENITH_TOOLS
	xAttachment.m_strName = strName;
#else
	(void)strName;
#endif

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	// Per-mip RTV creation — each RTV is a single-layer, single-mip view of the
	// attachment at mip `uMip`. Before this fix, the mip argument was omitted
	// and every entry of m_axRTVs[] pointed at mip 0, silently redirecting
	// every "draw into mip N" operation to mip 0.
	for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
	{
		xAttachment.m_axRTVs[uMip] = xVulkanMemory.CreateRenderTargetView(xAttachment.m_xVRAMHandle, xInfo, uMip);
	}

	xAttachment.m_xSRV = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, 0, m_uNumMips);
	// Per-mip SRVs — each SRV views a single mip (uBaseMip = uMip, uMipCount = 1)
	// so `SRV(uMip)` / `GetMipSRV(uMip)` actually return that mip. Previous code
	// always created SRVs on mip 0 which broke multi-mip sampling (Hi-Z pyramid
	// downsampling, SSR mip-based reflections, anything calling SRV(N) for N>0).
	// m_uNumMips is always >= 1 by construction — the old `if (m_uNumMips > 0)`
	// guard was dead.
	for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
	{
		xAttachment.m_axMipSRVs[uMip] = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, uMip, 1);
	}

	if (m_uMemoryFlags & (1 << MEMORY_FLAGS__UNORDERED_ACCESS))
	{
		for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
		{
			xAttachment.m_axUAVs[uMip] = xVulkanMemory.CreateUnorderedAccessView(
				xAttachment.m_xVRAMHandle, xInfo, uMip);
		}
	}

	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: %sColour Attachment '%s' VRAM=%u %ux%u mips=%u",
			szDiagPrefix,
			strName.c_str(),
			xAttachment.m_xVRAMHandle.AsUInt(),
			xInfo.m_uWidth, xInfo.m_uHeight, m_uNumMips);
	}
}

void Flux_RenderAttachmentBuilder::BuildColour(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	// Idempotent rebuild: if this attachment was built previously (e.g. first-
	// frame init) and is now being re-built (e.g. window resize), queue the
	// prior VRAM + all image views for deferred GPU-safe deletion first.
	Destroy(xAttachment);

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth       = m_uWidth;
	xInfo.m_uHeight      = m_uHeight;
	xInfo.m_uDepth       = m_uDepth;
	xInfo.m_eFormat      = m_eFormat;
	xInfo.m_eTextureType = m_eTextureType;
	xInfo.m_uNumMips     = m_uNumMips;
	xInfo.m_uNumLayers   = 1;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	BuildColourImpl(xAttachment, strName, g_xEngine.FluxMemory().CreateRenderTargetVRAM(xInfo), "");
}

void Flux_RenderAttachmentBuilder::BuildColourFromAliasedVRAM(Flux_RenderAttachment& xAttachment, const std::string& strName, Flux_VRAMHandle xAliasedVRAM)
{
	// Aliased-image variant: skip CreateRenderTargetVRAM; the VRAM was created
	// separately via Flux_MemoryManager::CreateAliasedImageVRAM and is bound
	// to a pool's allocation. All view creation logic is identical to the
	// non-aliased BuildColour.
	Zenith_Assert(xAliasedVRAM.IsValid(),
		"Flux_RenderAttachmentBuilder::BuildColourFromAliasedVRAM: invalid aliased VRAM handle for '%s'", strName.c_str());
	Destroy(xAttachment);

	BuildColourImpl(xAttachment, strName, xAliasedVRAM, "Aliased ");
}

void Flux_RenderAttachmentBuilder::BuildColourCubemap(Flux_RenderAttachmentCube& xAttachment, const std::string& strName)
{
	// Idempotent rebuild — see BuildColour for rationale. Cubemap cleanup also
	// has to iterate all 6 faces of the per-(mip, face) RTV grid.
	Destroy(xAttachment);

	Zenith_Assert(m_uNumMips <= FLUX_MAX_MIPS, "Flux_RenderAttachmentBuilder::BuildColourCubemap: m_uNumMips (%u) exceeds FLUX_MAX_MIPS (%u)", m_uNumMips, FLUX_MAX_MIPS);

	// Cubemap-specific surface info: 6 layers (one per face), cube texture type so
	// CreateRenderTargetVRAM sets eCubeCompatible and CreateShaderResourceView emits
	// a vk::ImageViewType::eCube for shader sampling.
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_uDepth = 1;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_eTextureType = TEXTURE_TYPE_CUBE;
	xInfo.m_uNumMips = m_uNumMips;
	xInfo.m_uNumLayers = 6;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	xAttachment.m_xVRAMHandle = xVulkanMemory.CreateRenderTargetVRAM(xInfo);
	xAttachment.m_xSurfaceInfo = xInfo;
#ifdef ZENITH_TOOLS
	xAttachment.m_strName = strName;
#else
	(void)strName;
#endif

	// Whole-cube SRV spanning every mip and all 6 layers. This is the view that shaders
	// bind for cube sampling (e.g. textureLod(cube, R, roughness) in IBL).
	xAttachment.m_xSRV = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, 0, m_uNumMips);

	// Per-mip cube-slice SRVs: cube view restricted to a single mip. Useful for debug
	// visualisations (e.g. IBL_DEBUG_PREFILTERED_MIPS stepping through roughness mips).
	for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
	{
		xAttachment.m_axSliceSRVs[uMip] = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo, uMip, 1);
	}

	// Per-mip whole-cube RTVs: all 6 layers at a given mip, bound as a layered attachment.
	// Kept for parity with the 2D builder; the IBL render graph uses the per-(mip, face)
	// RTVs below instead, one face at a time.
	for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
	{
		xAttachment.m_axRTVs[uMip] = xVulkanMemory.CreateRenderTargetView(xAttachment.m_xVRAMHandle, xInfo, uMip);
	}

	// Per-(mip, face) RTVs: 2D single-layer single-mip views. The render graph selects
	// one of these for each IBL face pass so the fragment shader writes to exactly one
	// cube face at one mip level.
	for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
	{
		for (u_int uFace = 0; uFace < 6; uFace++)
		{
			xAttachment.m_aaxMipFaceRTVs[uMip][uFace] = xVulkanMemory.CreateRenderTargetViewForLayer(xAttachment.m_xVRAMHandle, xInfo, uFace, uMip);
		}
	}

	// Per-mip UAVs only if the caller asked for unordered-access storage.
	if (m_uMemoryFlags & (1 << MEMORY_FLAGS__UNORDERED_ACCESS))
	{
		for (u_int uMip = 0; uMip < m_uNumMips; uMip++)
		{
			xAttachment.m_axUAVs[uMip] = xVulkanMemory.CreateUnorderedAccessView(xAttachment.m_xVRAMHandle, xInfo, uMip);
		}
	}

	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: Cubemap Attachment '%s' VRAM=%u %ux%u mips=%u layers=6",
			strName.c_str(),
			xAttachment.m_xVRAMHandle.AsUInt(),
			xInfo.m_uWidth, xInfo.m_uHeight, m_uNumMips);
	}
}

void Flux_RenderAttachmentBuilder::BuildDepthStencil(Flux_RenderAttachment& xAttachment, const std::string& strName)
{
	// Idempotent rebuild — see BuildColour for rationale. Depth attachments
	// only own a single DSV + a single SRV, so release is simple.
	Destroy(xAttachment);

	const u_int uNumLayers = (m_uNumLayers == 0) ? 1u : m_uNumLayers;
	Zenith_Assert(uNumLayers <= FLUX_MAX_ATTACHMENT_LAYERS,
		"Flux_RenderAttachmentBuilder::BuildDepthStencil: m_uNumLayers (%u) exceeds FLUX_MAX_ATTACHMENT_LAYERS (%u)", uNumLayers, FLUX_MAX_ATTACHMENT_LAYERS);

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = m_uWidth;
	xInfo.m_uHeight = m_uHeight;
	xInfo.m_eFormat = m_eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uNumLayers = uNumLayers;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	xAttachment.m_xVRAMHandle = xVulkanMemory.CreateRenderTargetVRAM(xInfo);
	xAttachment.m_xSurfaceInfo = xInfo;
#ifdef ZENITH_TOOLS
	xAttachment.m_strName = strName;
#else
	(void)strName;
#endif

	if (uNumLayers > 1)
	{
		// Array depth (e.g. CSM cascades): one single-slice DSV per layer so a
		// render pass can write exactly one cascade; the whole-array SRV (e2DArray)
		// is what shaders sample as Sampler2DArray. m_xDSV stays invalid — array
		// attachments render through the per-layer DSVs, never the whole-array view.
		for (u_int u = 0; u < uNumLayers; u++)
		{
			xAttachment.m_axLayerDSVs[u] = xVulkanMemory.CreateDepthStencilViewForLayer(xAttachment.m_xVRAMHandle, xInfo, u, 0);
		}
	}
	else
	{
		xAttachment.m_xDSV = xVulkanMemory.CreateDepthStencilView(xAttachment.m_xVRAMHandle, xInfo, 0);
	}
	xAttachment.m_xSRV = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo);

	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: DepthStencil Attachment '%s' VRAM=%u %ux%u layers=%u",
			strName.c_str(),
			xAttachment.m_xVRAMHandle.AsUInt(),
			xInfo.m_uWidth, xInfo.m_uHeight, uNumLayers);
	}
}

void Flux_RenderAttachmentBuilder::BuildDepthStencilFromAliasedVRAM(Flux_RenderAttachment& xAttachment, const std::string& strName, Flux_VRAMHandle xAliasedVRAM)
{
	// Aliased-image variant of BuildDepthStencil — parallel to
	// BuildColourFromAliasedVRAM. VRAM creation is skipped; all other steps
	// (surface info, DSV + SRV) run identically.
	Zenith_Assert(xAliasedVRAM.IsValid(),
		"Flux_RenderAttachmentBuilder::BuildDepthStencilFromAliasedVRAM: invalid aliased VRAM handle for '%s'", strName.c_str());
	Destroy(xAttachment);

	const u_int uNumLayers = (m_uNumLayers == 0) ? 1u : m_uNumLayers;
	Zenith_Assert(uNumLayers <= FLUX_MAX_ATTACHMENT_LAYERS,
		"Flux_RenderAttachmentBuilder::BuildDepthStencilFromAliasedVRAM: m_uNumLayers (%u) exceeds FLUX_MAX_ATTACHMENT_LAYERS (%u)", uNumLayers, FLUX_MAX_ATTACHMENT_LAYERS);

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth       = m_uWidth;
	xInfo.m_uHeight      = m_uHeight;
	xInfo.m_eFormat      = m_eFormat;
	xInfo.m_uNumMips     = 1;
	xInfo.m_uNumLayers   = uNumLayers;
	xInfo.m_uMemoryFlags = m_uMemoryFlags;

	xAttachment.m_xVRAMHandle = xAliasedVRAM;
	xAttachment.m_xSurfaceInfo = xInfo;
#ifdef ZENITH_TOOLS
	xAttachment.m_strName = strName;
#else
	(void)strName;
#endif

	auto& xVulkanMemory = g_xEngine.FluxMemory();

	if (uNumLayers > 1)
	{
		for (u_int u = 0; u < uNumLayers; u++)
		{
			xAttachment.m_axLayerDSVs[u] = xVulkanMemory.CreateDepthStencilViewForLayer(xAttachment.m_xVRAMHandle, xInfo, u, 0);
		}
	}
	else
	{
		xAttachment.m_xDSV = xVulkanMemory.CreateDepthStencilView(xAttachment.m_xVRAMHandle, xInfo, 0);
	}
	xAttachment.m_xSRV = xVulkanMemory.CreateShaderResourceView(xAttachment.m_xVRAMHandle, xInfo);

	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "DIAG: Aliased DepthStencil Attachment '%s' VRAM=%u %ux%u",
			strName.c_str(),
			xAttachment.m_xVRAMHandle.AsUInt(),
			xInfo.m_uWidth, xInfo.m_uHeight);
	}
}
