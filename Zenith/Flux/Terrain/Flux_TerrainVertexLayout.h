#pragma once

#include "Flux/Flux_Enums.h"

#include <cstdint>

namespace Flux_TerrainVertexLayout
{
	struct Element
	{
		ShaderDataType m_eType;
		uint32_t m_uSize;
	};

	inline constexpr Element axELEMENTS[] =
	{
		{ SHADER_DATA_TYPE_FLOAT3, 12u },
		{ SHADER_DATA_TYPE_FLOAT2, 8u },
		{ SHADER_DATA_TYPE_SNORM10_10_10_2, 4u },
		{ SHADER_DATA_TYPE_SNORM10_10_10_2, 4u },
	};

	inline constexpr uint32_t uELEMENT_COUNT = static_cast<uint32_t>(sizeof(axELEMENTS) / sizeof(axELEMENTS[0]));

	constexpr uint32_t CalculateVertexStride()
	{
		uint32_t uStride = 0;
		for (const Element& xElement : axELEMENTS)
			uStride += xElement.m_uSize;
		return uStride;
	}

	inline constexpr uint32_t uVERTEX_STRIDE = CalculateVertexStride();
	static_assert(uELEMENT_COUNT == 4u, "Terrain vertex layout must retain its four canonical elements");
	static_assert(axELEMENTS[0].m_eType == SHADER_DATA_TYPE_FLOAT3 && axELEMENTS[0].m_uSize == 12u,
		"Terrain vertex element 0 must remain FLOAT3 position");
	static_assert(axELEMENTS[1].m_eType == SHADER_DATA_TYPE_FLOAT2 && axELEMENTS[1].m_uSize == 8u,
		"Terrain vertex element 1 must remain FLOAT2 UV");
	static_assert(axELEMENTS[2].m_eType == SHADER_DATA_TYPE_SNORM10_10_10_2 && axELEMENTS[2].m_uSize == 4u,
		"Terrain vertex element 2 must remain packed normal");
	static_assert(axELEMENTS[3].m_eType == SHADER_DATA_TYPE_SNORM10_10_10_2 && axELEMENTS[3].m_uSize == 4u,
		"Terrain vertex element 3 must remain packed tangent/sign");
	static_assert(uVERTEX_STRIDE == 28u, "Terrain vertex layout must retain its locked 28-byte stride");

	inline constexpr uint32_t uHIGH_CHUNK_VERTICES_PER_EDGE = 65u;
	inline constexpr uint32_t uHIGH_CHUNK_VERTEX_COUNT = uHIGH_CHUNK_VERTICES_PER_EDGE * uHIGH_CHUNK_VERTICES_PER_EDGE;
	inline constexpr uint32_t uHIGH_CHUNK_INDEX_COUNT = (uHIGH_CHUNK_VERTICES_PER_EDGE - 1u) * (uHIGH_CHUNK_VERTICES_PER_EDGE - 1u) * 6u;
	static_assert(uHIGH_CHUNK_VERTEX_COUNT == 4225u, "HIGH terrain chunks must retain 65x65 vertices");
	static_assert(uHIGH_CHUNK_INDEX_COUNT == 24576u, "HIGH terrain chunks must retain 64x64 quads at six indices per quad");
}
