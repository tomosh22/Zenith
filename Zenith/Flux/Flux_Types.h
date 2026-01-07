#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_Enums.h"

class Flux_VRAMHandle
{
public:
	Flux_VRAMHandle() = default;

	void SetValue(const u_int uVal) { m_uVRAMHandle = uVal; }

	u_int AsUInt() const { return m_uVRAMHandle; }
	bool IsValid() const { return m_uVRAMHandle != UINT32_MAX; }
private:
	u_int m_uVRAMHandle = UINT32_MAX;
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
	}
	Zenith_Assert(false, "Trying to calculate size of ShaderDataType::None");
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
		}
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

static bool IsCompressedFormat(TextureFormat eFormat)
{
	return eFormat == TEXTURE_FORMAT_BC1_RGB_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC1_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC3_RGBA_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC5_RG_UNORM ||
		   eFormat == TEXTURE_FORMAT_BC7_RGBA_UNORM;
}

// Returns bytes per 4x4 block for compressed formats
static uint32_t CompressedFormatBytesPerBlock(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_BC1_RGB_UNORM:
	case TEXTURE_FORMAT_BC1_RGBA_UNORM:
		return 8u;  // 8 bytes per 4x4 block
	case TEXTURE_FORMAT_BC3_RGBA_UNORM:
	case TEXTURE_FORMAT_BC5_RG_UNORM:
	case TEXTURE_FORMAT_BC7_RGBA_UNORM:
		return 16u; // 16 bytes per 4x4 block
	default:
		return 0u;
	}
}

// Calculate total size in bytes for a compressed texture
static size_t CalculateCompressedTextureSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight)
{
	const uint32_t uBlocksX = (uWidth + 3) / 4;
	const uint32_t uBlocksY = (uHeight + 3) / 4;
	return static_cast<size_t>(uBlocksX) * uBlocksY * CompressedFormatBytesPerBlock(eFormat);
}

static uint32_t ColourFormatBitsPerPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_RGBA8_UNORM:
		return 32u;
	case TEXTURE_FORMAT_BGRA8_SRGB:
		return 32u;
	case TEXTURE_FORMAT_BGRA8_UNORM:
		return 32u;
	case TEXTURE_FORMAT_R16G16B16A16_SFLOAT:
		return 64u;
	case TEXTURE_FORMAT_R16G16B16A16_UNORM:
		return 64u;
	case TEXTURE_FORMAT_R32G32B32A32_SFLOAT:
		return 128u;
	case TEXTURE_FORMAT_R32G32B32_SFLOAT:
		return 96u;
	default:
		Zenith_Assert(false, "Unrecognised colour format");
		return 0u;
	}
}

static uint32_t ColourFormatBytesPerPixel(TextureFormat eFormat)
{
	return ColourFormatBitsPerPixel(eFormat) / 8u;
}

static uint32_t DepthStencilFormatBitsPerPixel(TextureFormat eFormat)
{
	switch (eFormat)
	{
	case TEXTURE_FORMAT_D32_SFLOAT:
		return 32;
	default:
		Zenith_Assert(false, "Unrecognised depth/stencil format");
		return 0u;
	}
}

struct Flux_BlendState
{
	BlendFactor m_eSrcBlendFactor = BLEND_FACTOR_SRCALPHA;
	BlendFactor m_eDstBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;
	bool m_bBlendEnabled = true;
};

struct Flux_DescriptorBinding
{
	DescriptorType m_eType = DESCRIPTOR_TYPE_MAX;
};

struct Flux_DescriptorSetLayout
{
	Flux_DescriptorBinding m_axBindings[FLUX_MAX_DESCRIPTOR_BINDINGS];
};

struct Flux_PipelineLayout
{
	u_int m_uNumDescriptorSets = 0;
	Flux_DescriptorSetLayout m_axDescriptorSetLayouts[FLUX_MAX_DESCRIPTOR_SET_LAYOUTS];
};
