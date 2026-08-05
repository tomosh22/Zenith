#pragma once

#include "Core/Zenith_VertexAttributeTypes.h"

#include <cstdint>

// ============================================================================
// Zenith_TerrainChunkLayout
//
// THE on-disk contract for a baked terrain chunk: the element table a
// Render_X_Y / Physics_X_Y .zmesh serializes, the packed vertex stride, and
// the per-density vertex/index counts. Every static_assert below freezes a
// value that baked assets on disk already depend on -- changing one is a
// breaking asset change (see Flux/Terrain/CLAUDE.md on bake stamps).
//
// WHY THIS LIVES IN Core: it is read from two directions. Flux consumes it as
// a GPU vertex layout, and the asset side (Zenith_TerrainComponent's chunk
// loader, the terrain exporter in ZenithTools) consumes it as a FILE FORMAT --
// to validate a chunk's serialized element descriptors and to walk vertex data
// at the right stride. Owning it Flux-side forced the EntityComponent chunk
// loader to take an edge into Flux purely to learn the shape of the bytes it
// was reading. It describes the bytes, so it lives with the format.
// ============================================================================

namespace Zenith_TerrainChunkLayout
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

	inline constexpr uint32_t uCHUNK_QUADS_PER_EDGE = 64u;

	constexpr uint32_t CalculateChunkVertexCount(uint32_t uDensityDivisor)
	{
		const uint32_t uQuadsPerEdge = uCHUNK_QUADS_PER_EDGE / uDensityDivisor;
		return (uQuadsPerEdge + 1u) * (uQuadsPerEdge + 1u);
	}

	constexpr uint32_t CalculateChunkIndexCount(uint32_t uDensityDivisor)
	{
		const uint32_t uQuadsPerEdge = uCHUNK_QUADS_PER_EDGE / uDensityDivisor;
		return uQuadsPerEdge * uQuadsPerEdge * 6u;
	}

	inline constexpr uint32_t uHIGH_CHUNK_VERTICES_PER_EDGE = uCHUNK_QUADS_PER_EDGE + 1u;
	inline constexpr uint32_t uHIGH_CHUNK_VERTEX_COUNT = CalculateChunkVertexCount(1u);
	inline constexpr uint32_t uHIGH_CHUNK_INDEX_COUNT = CalculateChunkIndexCount(1u);
	inline constexpr uint32_t uLOW_CHUNK_VERTEX_COUNT = CalculateChunkVertexCount(4u);
	inline constexpr uint32_t uLOW_CHUNK_INDEX_COUNT = CalculateChunkIndexCount(4u);
	// Collision deliberately remains lower density than the nearby HIGH render
	// mesh, but an 8 m grid is too coarse for a player to read as grounded on
	// Zenithmon's uneven terrain. Four-metre quads retain the collision budget
	// advantage while keeping the collision surface close to what is rendered.
	inline constexpr uint32_t uPHYSICS_CHUNK_VERTEX_COUNT = CalculateChunkVertexCount(4u);
	inline constexpr uint32_t uPHYSICS_CHUNK_INDEX_COUNT = CalculateChunkIndexCount(4u);
	static_assert(uHIGH_CHUNK_VERTEX_COUNT == 4225u, "HIGH terrain chunks must retain 65x65 vertices");
	static_assert(uHIGH_CHUNK_INDEX_COUNT == 24576u, "HIGH terrain chunks must retain 64x64 quads at six indices per quad");
	static_assert(uLOW_CHUNK_VERTEX_COUNT == 289u && uLOW_CHUNK_INDEX_COUNT == 1536u,
		"LOW terrain chunks must retain their density-divisor-4 topology");
	static_assert(uPHYSICS_CHUNK_VERTEX_COUNT == 289u && uPHYSICS_CHUNK_INDEX_COUNT == 1536u,
		"Physics terrain chunks must retain their density-divisor-4 topology");
}
