#pragma once

#include "Collections/Zenith_Vector.h"
#include "Core/ZenithConfig.h"  // FLUX_MAX_MIPS — used by Flux_RenderAttachment below
#include "Flux/Flux_Enums.h"
#include <string>

class Flux_VRAMHandle
{
public:
	Flux_VRAMHandle() = default;

	void SetValue(const u_int uVal) { m_uVRAMHandle = uVal; }

	u_int AsUInt() const { return m_uVRAMHandle; }
	bool IsValid() const { return m_uVRAMHandle != UINT32_MAX; }

	bool operator==(const Flux_VRAMHandle& rhs) const { return m_uVRAMHandle == rhs.m_uVRAMHandle; }
	bool operator!=(const Flux_VRAMHandle& rhs) const { return m_uVRAMHandle != rhs.m_uVRAMHandle; }
private:
	u_int m_uVRAMHandle = UINT32_MAX;
};

// Opaque handle to an image view (SRV/RTV/DSV/UAV)
// Follows the same registry pattern as Flux_VRAMHandle
class Flux_ImageViewHandle
{
public:
	Flux_ImageViewHandle() = default;

	void SetValue(const u_int uVal) { m_uHandle = uVal; }

	u_int AsUInt() const { return m_uHandle; }
	bool IsValid() const { return m_uHandle != UINT32_MAX; }
private:
	u_int m_uHandle = UINT32_MAX;
};

// Opaque handle to buffer descriptor info (CBV/UAV buffers)
// Follows the same registry pattern as Flux_VRAMHandle
class Flux_BufferDescriptorHandle
{
public:
	Flux_BufferDescriptorHandle() = default;

	void SetValue(const u_int uVal) { m_uHandle = uVal; }

	u_int AsUInt() const { return m_uHandle; }
	bool IsValid() const { return m_uHandle != UINT32_MAX; }
private:
	u_int m_uHandle = UINT32_MAX;
};

// Identifies one shader binding: the descriptor group (set) + the binding index
// within that group. Replaces the old stateful BeginBind(group) + per-bind index
// model -- each bind now carries its full slot, so the backend never has to track
// a "current group". Implicitly constructible from a bare binding index (group 0)
// so the common single-group call sites stay terse; multi-group sites (e.g. the
// shader binder resolving a reflected set) pass {group, binding} explicitly.
struct Flux_BindingSlot
{
	u_int m_uGroup = 0;
	u_int m_uBinding = 0;
	// When true, the backend clears the group's accumulated bindings before
	// applying this one -- i.e. this bind starts a fresh sequence for the group.
	// Set by the shader binder on a descriptor-set change (and by direct bind
	// sites at the start of their sequence), replacing the old BeginBind(group)
	// per-sequence clear. A no-op backend simply ignores it.
	bool m_bResetGroup = false;

	Flux_BindingSlot() = default;
	Flux_BindingSlot(u_int uBinding) : m_uGroup(0), m_uBinding(uBinding) {}
	Flux_BindingSlot(u_int uGroup, u_int uBinding) : m_uGroup(uGroup), m_uBinding(uBinding) {}
	Flux_BindingSlot(u_int uGroup, u_int uBinding, bool bResetGroup) : m_uGroup(uGroup), m_uBinding(uBinding), m_bResetGroup(bResetGroup) {}
};

// A range of image subresources (mip levels + array layers) for a resource
// barrier. Defaults cover mip 0 / layer 0 only. Replaces the four loose
// base-mip / mip-count / base-layer / layer-count parameters the old
// ImageTransition overloads took.
struct Flux_SubresourceRange
{
	u_int m_uBaseMip = 0;
	u_int m_uMipCount = 1;
	u_int m_uBaseLayer = 0;
	u_int m_uLayerCount = 1;
};

static uint32_t Flux_ShaderDataTypeSize(ShaderDataType t)
{
	switch (t)
	{
	case SHADER_DATA_TYPE_FLOAT:	return sizeof(float);
	case SHADER_DATA_TYPE_FLOAT2:	return sizeof(float) * 2;
	case SHADER_DATA_TYPE_FLOAT3:	return sizeof(float) * 3;
	case SHADER_DATA_TYPE_FLOAT4:	return sizeof(float) * 4;
	case SHADER_DATA_TYPE_INT:		return sizeof(int32_t);
	case SHADER_DATA_TYPE_INT2:		return sizeof(int32_t) * 2;
	case SHADER_DATA_TYPE_INT3:		return sizeof(int32_t) * 3;
	case SHADER_DATA_TYPE_INT4:		return sizeof(int32_t) * 4;
	case SHADER_DATA_TYPE_UINT:		return sizeof(uint32_t);
	case SHADER_DATA_TYPE_UINT2:	return sizeof(uint32_t) * 2;
	case SHADER_DATA_TYPE_UINT3:	return sizeof(uint32_t) * 3;
	case SHADER_DATA_TYPE_UINT4:	return sizeof(uint32_t) * 4;
	case SHADER_DATA_TYPE_MAT3:		return sizeof(float) * 3 * 3;
	case SHADER_DATA_TYPE_MAT4:		return sizeof(float) * 4 * 4;
	case SHADER_DATA_TYPE_BOOL:		return sizeof(bool);
	case SHADER_DATA_TYPE_HALF2:				return 4;
	case SHADER_DATA_TYPE_SNORM10_10_10_2:		return 4;
	case SHADER_DATA_TYPE_NONE: break;
	}
	Zenith_Assert(false, "Trying to calculate size of ShaderDataType::None");
	return 0;
}

struct Flux_BufferElement
{
	uint32_t m_uOffset;
	uint32_t m_uSize;
	ShaderDataType m_eType;

	Flux_BufferElement(ShaderDataType type, bool /*normalized*/ = false, bool /*instanced*/ = false, unsigned int /*divisor*/ = 0, void* /*data*/ = nullptr, unsigned int /*numEntries*/ = 0) : m_eType(type), m_uSize(Flux_ShaderDataTypeSize(type)), m_uOffset(0)
	{
	}

	Flux_BufferElement() {};

	bool operator==(const Flux_BufferElement& other) const
	{
		return m_eType == other.m_eType && m_uSize == other.m_uSize && m_uOffset == other.m_uOffset;
	}

	uint32_t GetComponentCount() const
	{
		switch (m_eType)
		{
		case SHADER_DATA_TYPE_FLOAT:	return 1;
		case SHADER_DATA_TYPE_FLOAT2:	return 2;
		case SHADER_DATA_TYPE_FLOAT3:	return 3;
		case SHADER_DATA_TYPE_FLOAT4:	return 4;
		case SHADER_DATA_TYPE_INT:		return 1;
		case SHADER_DATA_TYPE_INT2:		return 2;
		case SHADER_DATA_TYPE_INT3:		return 3;
		case SHADER_DATA_TYPE_INT4:		return 4;
		case SHADER_DATA_TYPE_UINT:		return 1;
		case SHADER_DATA_TYPE_UINT2:	return 2;
		case SHADER_DATA_TYPE_UINT3:	return 3;
		case SHADER_DATA_TYPE_UINT4:	return 4;
		case SHADER_DATA_TYPE_MAT3:		return 3 * 3;
		case SHADER_DATA_TYPE_MAT4:		return 4 * 4;
		case SHADER_DATA_TYPE_BOOL:		return 1;
		case SHADER_DATA_TYPE_HALF2:				return 2;
		case SHADER_DATA_TYPE_SNORM10_10_10_2:		return 4;
		case SHADER_DATA_TYPE_NONE: break;
		}
		Zenith_Assert(false, "Unknown ShaderDataType");
		return 0;
	}
};

class Flux_BufferLayout
{
public:
	Flux_BufferLayout() = default;

	bool operator==(const Flux_BufferLayout& other) const
	{
		if (m_xElements.GetSize() != other.m_xElements.GetSize())
		{
			return false;
		}

		for (uint32_t u = 0; u < m_xElements.GetSize(); u++)
		{
			if (m_xElements.Get(u) != other.m_xElements.Get(u))
			{
				return false;
			}
		}
		return true;
	}

	void Reset() { m_xElements.Clear(); }
	Zenith_Vector<Flux_BufferElement>& GetElements() { return m_xElements; };
	const Zenith_Vector<Flux_BufferElement>& GetElements() const { return m_xElements; };
	const uint32_t GetStride() const { return m_uStride; }
	void CalculateOffsetsAndStrides()
	{
		uint32_t uOffset = 0;
		m_uStride = 0;
		for (u_int u = 0; u < m_xElements.GetSize(); ++u)
		{
			Flux_BufferElement& xElement = m_xElements.Get(u);
			xElement.m_uOffset = uOffset;
			uOffset += xElement.m_uSize;
			m_uStride += xElement.m_uSize;
		}
	}
private:
	uint32_t m_uStride = 0;
	Zenith_Vector<Flux_BufferElement> m_xElements;
};

struct Flux_VertexInputDescription
{
	MeshTopology m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	Flux_BufferLayout m_xPerVertexLayout;
	Flux_BufferLayout m_xPerInstanceLayout;
};

bool IsCompressedFormat(TextureFormat eFormat);
uint32_t CompressedFormatBytesPerBlock(TextureFormat eFormat);
size_t CalculateCompressedTextureSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight);
uint32_t ColourFormatBitsPerPixel(TextureFormat eFormat);
uint32_t ColourFormatBytesPerPixel(TextureFormat eFormat);
uint32_t DepthStencilFormatBitsPerPixel(TextureFormat eFormat);

// Per-mip / mip-chain byte-size helpers. SINGLE SOURCE OF TRUTH for the packed
// mip-chain layout shared by the .ztxtr exporter, the loader, and the GPU upload
// path (the exporter packs in this order, the loader allocates this total, the
// upload computes per-mip buffer offsets from these). Per-mip dimensions are
// max(1, dim >> uMip). Depth is treated as 1 (these cover 2D / compressed-2D;
// the only textures that carry a pre-baked chain).
size_t CalculateMipDataSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight, uint32_t uMip);
size_t CalculateTotalMipChainSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight, uint32_t uNumMips);

// How CreateTextureVRAM should populate the texture's mip chain. Replaces the
// old bool bCreateMips, which conflated "allocate a mip chain" with "blit-
// generate it after uploading mip 0" — the source of the BC-mips-black bug.
//   NONE             : single mip (mip 0 only); no chain.
//   GENERATE_RUNTIME : allocate a full chain, upload mip 0, blit-generate 1..N
//                      (uncompressed only — Vulkan can't blit-write BC images).
//   PREBAKED         : allocate a full chain; pData already holds every mip
//                      packed contiguously (CalculateTotalMipChainSize); upload
//                      all mips, NO blit. The only correct mode for BC textures.
enum TextureUploadMipMode
{
	TEXTURE_MIPS_NONE,
	TEXTURE_MIPS_GENERATE_RUNTIME,
	TEXTURE_MIPS_PREBAKED,
};

struct Flux_BlendState
{
	BlendFactor m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	BlendFactor m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
	BlendFactor m_eSrcAlphaBlendFactor = BLEND_FACTOR_SRCALPHA;
	BlendFactor m_eDstAlphaBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
	bool m_bBlendEnabled = true;
	uint8_t m_uColorWriteMask = 0xF;
};

struct Flux_BindingGroupEntry
{
	BindingType m_eType = BINDING_TYPE_MAX;
};

struct Flux_BindingGroupLayout
{
	Flux_BindingGroupEntry m_axBindings[FLUX_MAX_BINDINGS_PER_GROUP];
};

struct Flux_PipelineLayout
{
	u_int m_uNumBindingGroups = 0;
	Flux_BindingGroupLayout m_axBindingGroups[FLUX_MAX_BINDING_GROUPS];
};

// ============================================================================
// Surface + resource view structs
// Lived in Flux.h previously but moved here so backend-level headers (e.g.
// Zenith_Vulkan_MemoryManager.h) can use them without pulling in Flux.h,
// which transitively re-includes the platform graphics header and formed a
// three-file include cycle (MemoryManager.h -> Flux.h -> PlatformGraphics_Include.h -> MemoryManager.h).
// ============================================================================
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

struct Flux_ShaderResourceView
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	bool m_bIsDepthStencil = false;
	u_int m_uBaseMip = 0;
	u_int m_uMipCount = 1;
};

struct Flux_UnorderedAccessView_Texture
{
	Flux_ImageViewHandle m_xImageViewHandle;
	Flux_VRAMHandle m_xVRAMHandle;
	u_int m_uMipLevel = 0;
};

struct Flux_UnorderedAccessView_Buffer
{
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};

// Read-only structured-buffer view (StructuredBuffer<T> in Slang). Distinct
// type from Flux_UnorderedAccessView_Buffer so the binder dispatch and the
// render-graph access classification can tell read-only from read-write at
// compile time, even though the underlying Vulkan descriptor is the same
// (vk::DescriptorType::eStorageBuffer).
struct Flux_ShaderResourceView_Buffer
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

// Lived in Flux.h previously. Hoisted here so backend-level headers that need
// the complete type (e.g. Zenith_Vulkan_Swapchain.h's m_axColourAttachments
// value-array member) can use it without pulling Flux.h — Flux.h transitively
// re-includes Zenith_PlatformGraphics_Include.h which re-includes the
// swapchain header, forming a cycle.
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

struct Flux_ConstantBufferView
{
	Flux_BufferDescriptorHandle m_xBufferDescHandle;
	Flux_VRAMHandle m_xVRAMHandle;
};
