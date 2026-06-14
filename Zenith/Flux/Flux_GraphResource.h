#pragma once

// Render-graph resource tagging: the tagged-pointer wrapper the graph uses to
// uniformly track 2D images, cubemap images, and buffers, plus the per-pass
// attachment ref / begin-info carriers and the recorded command-list entry.
//
// Lives separately from Flux_RenderGraph.h so Flux_CommandListEntry can embed a
// Flux_RenderGraph_AttachmentRef without a circular dependency on the graph header.
#include "Flux/Flux_RenderResources.h"  // Flux_RenderAttachmentCube, Flux_Buffer (+ Flux_RenderAttachment via Flux_Types.h)

class Flux_CommandList;
struct Flux_RenderGraph_Pass;

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

// Parameters for beginning a render-pass / dynamic-rendering scope. Bundles the
// colour attachment refs, the depth-stencil ref, and the per-aspect clear +
// depth-read-only flags that the old 7-arg BeginRenderPass took loosely. Lives
// here (not Flux_Types.h) because it stores a Flux_RenderGraph_AttachmentRef by
// value, which is defined just above.
struct Flux_RenderingBeginInfo
{
	const Flux_RenderGraph_AttachmentRef* m_paxColour = nullptr;
	u_int                                 m_uNumColour = 0;
	Flux_RenderGraph_AttachmentRef        m_xDepthStencil;
	bool m_bClearColour   = false;
	bool m_bClearDepth    = false;
	bool m_bClearStencil  = false;
	bool m_bDepthReadOnly = false;
};

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
