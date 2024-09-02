#pragma once

#include "Flux/Flux_Enums.h"

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
	uint32_t _Offset;
	uint32_t _Size;
	ShaderDataType _Type;
	bool _Normalized;
	bool _Instanced = false;
	unsigned int m_uDivisor = 0;
	void* m_pData = nullptr;
	unsigned int _numEntries = 0;

	Flux_BufferElement(ShaderDataType type, bool normalized = false, bool instanced = false, unsigned int divisor = 0, void* data = nullptr, unsigned int numEntries = 0) : _Type(type), _Size(Flux_ShaderDataTypeSize(type)), _Offset(0), _Normalized(normalized), _Instanced(instanced), m_uDivisor(divisor), m_pData(data), _numEntries(numEntries)
	{
	}

	Flux_BufferElement() {};

	uint32_t GetComponentCount() const
	{
		switch (_Type)
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
	std::vector<Flux_BufferElement>& GetElements() { return m_xElements; };
	const std::vector<Flux_BufferElement>& GetElements() const { return m_xElements; };
	const uint32_t GetStride() const { return m_uStride; }
	void CalculateOffsetsAndStrides()
	{
		uint32_t uOffset = 0;
		m_uStride = 0;
		for (Flux_BufferElement& xElement : m_xElements)
		{
			xElement._Offset = uOffset;
			uOffset += xElement._Size;
			m_uStride += xElement._Size;
		}
	}
private:
	uint32_t m_uStride = 0;
	std::vector<Flux_BufferElement> m_xElements;
};

struct Flux_VertexInputDescription
{
	MeshTopology m_eTopology;
	Flux_BufferLayout m_xPerVertexLayout;
	Flux_BufferLayout m_xPerInstanceLayout;
};

static uint32_t ColourFormatBitsPerPixel(ColourFormat eFormat)
{
	switch (eFormat)
	{
	case COLOUR_FORMAT_RGBA8_UNORM:
		return 32u;
	case COLOUR_FORMAT_BGRA8_SRGB:
		return 32u;
	case COLOUR_FORMAT_BGRA8_UNORM:
		return 32u;
	case COLOUR_FORMAT_R16G16B16A16_SFLOAT:
		return 64u;
	case COLOUR_FORMAT_R16G16B16A16_UNORM:
		return 64u;
	default:
		Zenith_Assert(false, "Unrecognised colour format");
		return 0u;
	}
}

static uint32_t ColourFormatBytesPerPixel(ColourFormat eFormat)
{
	return ColourFormatBitsPerPixel(eFormat) / 8u;
}

static uint32_t DepthStencilFormatBitsPerPixel(DepthStencilFormat eFormat)
{
	switch (eFormat)
	{
	case DEPTHSTENCIL_FORMAT_D32_SFLOAT:
		return 32;
	default:
		Zenith_Assert(false, "Unrecognised depth/stencil format");
		return 0u;
	}
}