#pragma once

#include <cstdint>

// The SOURCE-SAMPLE GRID the terrain exporter splits into baked chunks, expressed
// as pure arithmetic.
//
// WHY THIS EXISTS. A quad grid always needs one more SAMPLE per edge than it has
// CELLS. The exporter's source grid used to carry exactly (heightmap width x
// density) samples for (chunk grid x cells-per-chunk) cells -- i.e. it was one
// sample short in each axis. Every chunk closes itself by stitching the FIRST
// sample column/row of its +X / +Z neighbour, so the shortfall landed entirely on
// the LAST chunk column (x == 63) and row (z == 63): they had no neighbour to
// stitch from, and ExportChunkBatch simply skipped their stitch behind an
// `if (x < uNumSplitsX - 1)` guard. Those 127 of 4096 chunks baked with unwritten
// stitch vertices (uninitialised memory, serialized straight into the asset) and
// (0,0,0) index triples. Zenith_TerrainComponent's chunk-topology validator
// rejects exactly that, so all 127 were silently dropped from the always-resident
// LOW LOD *and* from the combined physics mesh -- no geometry and no collision
// along the outer +X/+Z strip of every full-grid terrain.
//
// Stating the grid as functions makes the load-bearing invariant testable without
// a heightmap, a task system or a file: a chunk's closing sample column IS its
// neighbour's first, and the last chunk's closing sample must still be in range.
//
// Kept dependency-light (<cstdint> only, no Flux headers) so it is unit-testable in
// every configuration -- same shape as Flux_TerrainExportRect.h and
// Flux_TerrainPipelineSelect.h in this directory.
namespace Flux_TerrainSourceGrid
{
	// Samples needed to span uCellsPerEdge quads. The +1 is the whole point.
	constexpr uint32_t SampleCountForCells(uint32_t uCellsPerEdge)
	{
		return uCellsPerEdge + 1u;
	}

	// Samples per edge of the source grid feeding a uChunkGridSize x uChunkGridSize
	// split at uCellsPerChunk quads per chunk. This is the ROW STRIDE of the flat
	// sample array -- note it is NOT uChunkGridSize * uCellsPerChunk.
	constexpr uint32_t SampleCountPerEdge(uint32_t uChunkGridSize, uint32_t uCellsPerChunk)
	{
		return SampleCountForCells(uChunkGridSize * uCellsPerChunk);
	}

	// Flat row-major index of one chunk-local sample within the source grid.
	// uSubX / uSubZ range over [0, uCellsPerChunk] INCLUSIVE -- the upper bound is
	// the closing column/row, which is shared with the +X / +Z neighbour chunk.
	constexpr uint32_t SampleIndex(uint32_t uChunkX, uint32_t uChunkZ,
		uint32_t uSubX, uint32_t uSubZ,
		uint32_t uCellsPerChunk, uint32_t uSamplesPerEdge)
	{
		return ((((uChunkZ * uCellsPerChunk) + uSubZ) * uSamplesPerEdge)
			+ (uChunkX * uCellsPerChunk) + uSubX);
	}

	// Slots a COMPLETE baked chunk carries. Must agree with the on-disk contract in
	// Core/Zenith_TerrainChunkLayout.h -- pinned by unit test.
	constexpr uint32_t ChunkVertexCount(uint32_t uCellsPerChunk)
	{
		return SampleCountForCells(uCellsPerChunk) * SampleCountForCells(uCellsPerChunk);
	}

	constexpr uint32_t ChunkIndexCount(uint32_t uCellsPerChunk)
	{
		return uCellsPerChunk * uCellsPerChunk * 6u;
	}

	// ---- sample <-> world <-> authoring-image mapping ------------------------
	//
	// The exporter used to weld three things together: one heightmap PIXEL was
	// one source SAMPLE was one world METRE. That only held because every
	// terrain was 4096 metres wide baked from a 4096px heightfield. With
	// per-terrain dimensions the three come apart, and the arithmetic that
	// separates them is stated here -- as pure doubles, so the fractional
	// bilinear tap it produces is testable without a heightmap, a task system
	// or a file, exactly like the sample-count functions above.
	//
	// EXACTNESS AT DEFAULTS IS LOAD-BEARING. At 1m vertex spacing and a 4096px
	// image over a 4096m domain, dSampleStepWorld is 1.0 (or the divisor) and
	// dImagePixelsPerWorldUnit is 1.0, so every result below is bit-identical
	// to the integer arithmetic it replaces and a default re-bake reproduces
	// the assets already on disk.

	// World metres between adjacent source-grid samples at one bake density.
	// A divisor-4 bake steps four times as far as the HIGH bake does.
	constexpr double SampleStepWorld(double dVertexSpacing, uint32_t uDensityDivisor)
	{
		return dVertexSpacing * static_cast<double>(uDensityDivisor);
	}

	// World coordinate of sample uSample along one axis of the source grid.
	constexpr double WorldForSample(uint32_t uSample, double dSampleStepWorld)
	{
		return static_cast<double>(uSample) * dSampleStepWorld;
	}

	// The authoring-image coordinate that sample lands on. FRACTIONAL in
	// general -- the bilinear tap at the call site is what consumes it, and its
	// closing sample still clamps to the last texel.
	constexpr double ImageCoordForSample(uint32_t uSample, double dSampleStepWorld,
		double dImagePixelsPerWorldUnit)
	{
		return WorldForSample(uSample, dSampleStepWorld) * dImagePixelsPerWorldUnit;
	}
}
