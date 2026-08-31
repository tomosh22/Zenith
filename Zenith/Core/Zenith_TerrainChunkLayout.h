#pragma once

#include "Core/Zenith_VertexAttributeTypes.h"

#include <cstdint>

// ============================================================================
// Zenith_TerrainChunkLayout
//
// THE on-disk contract for a baked terrain chunk: the element table a
// Render_X_Y / Physics_X_Y .zgeom serializes, the packed vertex stride, the
// byte offset of each element, the quantisation box the packed position and UV
// are meaningless without, and the per-density vertex/index counts. Every
// static_assert below freezes a value that baked assets on disk already depend
// on -- changing one is a breaking asset change (see Flux/Terrain/CLAUDE.md on
// bake stamps). Flux/Terrain/Flux_TerrainVertexQuant.h is the one place that
// joins the box below to the codec that applies it.
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
		{ SHADER_DATA_TYPE_SNORM16X4, 8u },
		{ SHADER_DATA_TYPE_UNORM16X2, 4u },
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

	// Byte offset of element uIndex — the running sum of the sizes above, which is
	// exactly how both producers tight-pack. Every hook that reaches into a packed
	// vertex takes its offsets from here rather than spelling 0/8/12/16, so a
	// layout edit moves the writers with the table.
	constexpr uint32_t CalculateElementOffset(uint32_t uIndex)
	{
		uint32_t uOffset = 0;
		for (uint32_t u = 0; u < uIndex; u++)
			uOffset += axELEMENTS[u].m_uSize;
		return uOffset;
	}

	inline constexpr uint32_t uVERTEX_STRIDE = CalculateVertexStride();
	inline constexpr uint32_t uPOSITION_OFFSET = CalculateElementOffset(0u);
	inline constexpr uint32_t uUV_OFFSET       = CalculateElementOffset(1u);
	inline constexpr uint32_t uNORMAL_OFFSET   = CalculateElementOffset(2u);
	inline constexpr uint32_t uTANGENT_OFFSET  = CalculateElementOffset(3u);
	static_assert(uELEMENT_COUNT == 4u, "Terrain vertex layout must retain its four canonical elements");
	static_assert(axELEMENTS[0].m_eType == SHADER_DATA_TYPE_SNORM16X4 && axELEMENTS[0].m_uSize == 8u,
		"Terrain vertex element 0 must remain a quantised SNORM16x4 position");
	static_assert(axELEMENTS[1].m_eType == SHADER_DATA_TYPE_UNORM16X2 && axELEMENTS[1].m_uSize == 4u,
		"Terrain vertex element 1 must remain a normalised UNORM16x2 UV");
	static_assert(axELEMENTS[2].m_eType == SHADER_DATA_TYPE_SNORM10_10_10_2 && axELEMENTS[2].m_uSize == 4u,
		"Terrain vertex element 2 must remain packed normal");
	static_assert(axELEMENTS[3].m_eType == SHADER_DATA_TYPE_SNORM10_10_10_2 && axELEMENTS[3].m_uSize == 4u,
		"Terrain vertex element 3 must remain packed tangent/sign");
	static_assert(uVERTEX_STRIDE == 20u, "Terrain vertex layout must retain its locked 20-byte stride");

	// ---- the dequantisation box -----------------------------------------------
	// A snorm16 position is meaningless without the box it was normalised against,
	// so the box is part of the FILE FORMAT. What lives here is the DEFAULT box --
	// the one every terrain baked before Zenith_TerrainDimensions existed. A
	// terrain's ACTUAL box derives from its own dimensions, per-axis:
	//   { 0, 0, 0 } .. { dims.WorldSizeX(), MAX_TERRAIN_HEIGHT, dims.WorldSizeZ() }
	// and Flux/Terrain/Flux_TerrainVertexQuant.h is the one place that builds it.
	// A set's dimensions travel with it in TerrainDims.zdata, because a chunk
	// cannot be decoded without them.
	//
	// It is the AUTHORED terrain extent, never a per-chunk fit, and that is
	// deliberate and load-bearing: adjacent chunks share their stitch vertices by
	// VALUE, and ONE box per terrain makes a shared world position quantise to the
	// same word in both chunks, so a seam cannot open. A per-chunk AABB would give
	// the two chunks different boxes and re-introduce cracks along every border.
	// Per-TERRAIN is safe for exactly the reason per-chunk is not: every chunk of
	// one terrain shares one box.
	//
	// XZ is [0, 4096] at DEFAULT dimensions -- the closing-sample fix landed the
	// grid's outer boundary on chunk size * grid size, so this is authored, never
	// data-derived. Y is [0, MAX_TERRAIN_HEIGHT] for every terrain: the exporter's
	// heights are a normalised heightmap sample scaled by that constant, and height
	// is deliberately NOT per-terrain. A live edit (editor sculpt, CityBuilder's
	// carve/terraform) that leaves the box is CLAMPED to it rather than wrapped.
	inline constexpr float afPOSITION_BOX_MIN[3] = { 0.0f, 0.0f, 0.0f };
	inline constexpr float afPOSITION_BOX_MAX[3] = { 4096.0f, 512.0f, 4096.0f };

	// Vertex UVs are AUTHORED WORLD XZ IN METRES over the terrain's square
	// authoring domain; the unorm16 lane carries that divided through by the same
	// extent. This constant is the DEFAULT domain -- a terrain's own is
	// dims.MaxWorldSize(). At 4096m over a 4096px heightfield the two readings
	// (metres and heightmap pixels) coincide, which is why they were
	// indistinguishable until terrains could differ in size.
	//
	// UNORM16 over the smallest domain a game is likely to author (a few hundred
	// metres) is finer than a hundredth of a metre, so the precision argument
	// below only gets stronger as terrains shrink.
	inline constexpr float fUV_BOX_MAX = 4096.0f;

	// One quantisation step, in world units. snorm16 spends 65534 steps across the
	// box ([-1,1] mapped onto [-32767,32767]); unorm16 spends 65535 across [0,1].
	// These are the tolerance any "does the packed stream agree with the authored
	// value" check must use -- comparing them for equality is comparing a rounded
	// value against its input.
	constexpr float PositionQuantStep(uint32_t uAxis)
	{
		return (afPOSITION_BOX_MAX[uAxis] - afPOSITION_BOX_MIN[uAxis]) / 65534.0f;
	}

	inline constexpr float fUV_QUANT_STEP = fUV_BOX_MAX / 65535.0f;

	// The DEFAULT quads per chunk edge. A terrain's own count is
	// Zenith_TerrainDimensions::m_uQuadsPerChunkEdge, and the counts below are
	// this default's -- kept as pins, because every chunk baked before the knob
	// existed carries exactly these topologies.
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
