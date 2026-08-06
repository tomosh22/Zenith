//------------------------------------------------------------------------------
// Flux_Terrain unit tests.
// Included at the bottom of Flux_Terrain.cpp (the module-owns-its-tests pattern).
//
// Focus: the G-buffer pipeline VARIANT selection (Flux_TerrainPipelineSelect.h).
// The terrain "Wireframe" checkbox was dead in every default run -- the record-time
// selection let the TAA velocity latch beat the wireframe flag, and TAA ships ON, so
// the wireframe branch was never even evaluated. These pin the 2x2 as a total
// function and, separately, pin the invariant that justified the old collapse and
// turned out to be false: that toggling wireframe could change the pass's attachment
// count. It cannot -- only the velocity latch can.
//
// HEADLESS-SAFE -- every assertion is over constexpr integers plus one bool debug
// var. No device, no pipeline object, no command buffer is touched.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Flux/Terrain/Flux_TerrainSourceGrid.h"

#ifdef ZENITH_TESTING

ZENITH_TEST(FluxTerrain, GBufferVariantSelectionIsTheFullTwoByTwo)
{
	// The truth table, stated once. (velocity, wireframe) -> variant.
	ZENITH_ASSERT_TRUE(Flux_TerrainSelectGBufferVariant(false, false) == Flux_TerrainGBufferVariant::SOLID,
		"no velocity + no wireframe must select the base 4-MRT G-buffer pipeline");
	ZENITH_ASSERT_TRUE(Flux_TerrainSelectGBufferVariant(false, true) == Flux_TerrainGBufferVariant::WIREFRAME,
		"no velocity + wireframe must select the 4-MRT wireframe pipeline");
	ZENITH_ASSERT_TRUE(Flux_TerrainSelectGBufferVariant(true, false) == Flux_TerrainGBufferVariant::VELOCITY_SOLID,
		"velocity + no wireframe must select the 5-MRT velocity pipeline");
	ZENITH_ASSERT_TRUE(Flux_TerrainSelectGBufferVariant(true, true) == Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME,
		"velocity + wireframe must select the 5-MRT wireframe velocity pipeline");
}

ZENITH_TEST(FluxTerrain, WireframeSurvivesTheVelocityLatch)
{
	// THE REGRESSION CLAUSE, isolated so a re-break names itself. This is the corner
	// the old nested ternary answered with VELOCITY_SOLID: the velocity arm was taken
	// first and dbg_bWireframe was never read. Since TAA ships ON, this corner is the
	// DEFAULT one -- so the checkbox did nothing at all.
	const Flux_TerrainGBufferVariant eVariant = Flux_TerrainSelectGBufferVariant(true, true);

	ZENITH_ASSERT_TRUE(eVariant == Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME,
		"wireframe must be honoured while the TAA velocity latch is on");
	ZENITH_ASSERT_FALSE(eVariant == Flux_TerrainGBufferVariant::VELOCITY_SOLID,
		"velocity must NOT suppress wireframe (this was the dead-checkbox bug)");
	ZENITH_ASSERT_TRUE(Flux_TerrainGBufferVariantIsWireframe(eVariant),
		"the velocity+wireframe variant must classify as a wireframe variant");
}

ZENITH_TEST(FluxTerrain, EveryGBufferVariantIsReachable)
{
	// Surjectivity. Guards against a future edit collapsing two input combinations
	// onto one variant -- the exact shape of the original bug -- even if the truth
	// table above were relaxed.
	bool abSeen[static_cast<size_t>(Flux_TerrainGBufferVariant::COUNT)] = {};

	for (int iVelocity = 0; iVelocity < 2; iVelocity++)
	{
		for (int iWireframe = 0; iWireframe < 2; iWireframe++)
		{
			const Flux_TerrainGBufferVariant eVariant =
				Flux_TerrainSelectGBufferVariant(iVelocity != 0, iWireframe != 0);
			const size_t uIndex = static_cast<size_t>(eVariant);

			ZENITH_ASSERT_LT(uIndex, static_cast<size_t>(Flux_TerrainGBufferVariant::COUNT),
				"selection must never return COUNT or an out-of-range variant");
			ZENITH_ASSERT_FALSE(abSeen[uIndex],
				"variant %u was selected by two different (velocity=%d, wireframe=%d) inputs -- "
				"the 2x2 has collapsed", static_cast<u_int>(uIndex), iVelocity, iWireframe);
			abSeen[uIndex] = true;
		}
	}

	for (size_t u = 0; u < static_cast<size_t>(Flux_TerrainGBufferVariant::COUNT); u++)
	{
		ZENITH_ASSERT_TRUE(abSeen[u], "variant %u is unreachable -- no input selects it", static_cast<u_int>(u));
	}
}

ZENITH_TEST(FluxTerrain, WireframeNeverChangesTheAttachmentCount)
{
	// The render-graph contract. SetupRenderGraph picks the pass's .Writes set from the
	// velocity latch ALONE, so the wireframe axis must be attachment-count-neutral or the
	// bound pipeline would be render-pass-incompatible with the framebuffer. This is the
	// invariant the old "wireframe is a 4-attachment debug pipeline" comment got wrong.
	for (int iVelocity = 0; iVelocity < 2; iVelocity++)
	{
		const bool bVelocity = (iVelocity != 0);
		const uint32_t uSolid = Flux_TerrainGBufferAttachmentCountForVariant(
			Flux_TerrainSelectGBufferVariant(bVelocity, false));
		const uint32_t uWireframe = Flux_TerrainGBufferAttachmentCountForVariant(
			Flux_TerrainSelectGBufferVariant(bVelocity, true));

		ZENITH_ASSERT_EQ(uSolid, uWireframe,
			"toggling wireframe must not move the attachment count (velocity=%d)", iVelocity);
	}
}

ZENITH_TEST(FluxTerrain, VelocityLatchAloneDecidesTheAttachmentCount)
{
	// Ties the <cstdint>-only header to the real MRT enum at runtime as well as via the
	// static_asserts in Flux_Terrain.cpp.
	for (int iWireframe = 0; iWireframe < 2; iWireframe++)
	{
		const bool bWireframe = (iWireframe != 0);
		const Flux_TerrainGBufferVariant eNoVelocity = Flux_TerrainSelectGBufferVariant(false, bWireframe);
		const Flux_TerrainGBufferVariant eVelocity   = Flux_TerrainSelectGBufferVariant(true,  bWireframe);

		ZENITH_ASSERT_EQ(Flux_TerrainGBufferAttachmentCountForVariant(eNoVelocity),
			static_cast<uint32_t>(uFLUX_MRT_CORE_COUNT),
			"latch off must be the 4-MRT core set (wireframe=%d)", iWireframe);
		ZENITH_ASSERT_EQ(Flux_TerrainGBufferAttachmentCountForVariant(eVelocity),
			static_cast<uint32_t>(MRT_INDEX_COUNT),
			"latch on must be the 5-MRT set (wireframe=%d)", iWireframe);

		ZENITH_ASSERT_FALSE(Flux_TerrainGBufferVariantIsVelocity(eNoVelocity),
			"latch off must not classify as a velocity variant (wireframe=%d)", iWireframe);
		ZENITH_ASSERT_TRUE(Flux_TerrainGBufferVariantIsVelocity(eVelocity),
			"latch on must classify as a velocity variant (wireframe=%d)", iWireframe);
	}
}

ZENITH_TEST(FluxTerrain, WireframeDebugVarDrivesTheSelection)
{
	// End-to-end over the seam the editor actually writes: the bool& returned by
	// GetWireframeMode() is what Zenith_TerrainComponent's "Wireframe" checkbox and
	// RenderTest's --rendertest-wireframe both assign through. Pins checkbox -> flag ->
	// pipeline choice without a GPU.
	bool& bWireframe = g_xEngine.Terrain().GetWireframeMode();
	const bool bSaved = bWireframe;   // process-wide debug var: leave it as we found it

	bWireframe = true;
	for (int iVelocity = 0; iVelocity < 2; iVelocity++)
	{
		ZENITH_ASSERT_TRUE(Flux_TerrainGBufferVariantIsWireframe(
			Flux_TerrainSelectGBufferVariant(iVelocity != 0, g_xEngine.Terrain().GetWireframeMode())),
			"ticking the checkbox must select a wireframe variant (velocity=%d)", iVelocity);
	}

	bWireframe = false;
	for (int iVelocity = 0; iVelocity < 2; iVelocity++)
	{
		ZENITH_ASSERT_FALSE(Flux_TerrainGBufferVariantIsWireframe(
			Flux_TerrainSelectGBufferVariant(iVelocity != 0, g_xEngine.Terrain().GetWireframeMode())),
			"unticking the checkbox must select a solid variant (velocity=%d)", iVelocity);
	}

	bWireframe = bSaved;
}

//------------------------------------------------------------------------------
// Exporter SOURCE-GRID arithmetic (Flux_TerrainSourceGrid.h).
//
// The bug these pin: the source grid carried cells-per-edge samples where a quad
// grid needs cells+1, so the LAST chunk column (x == 63) and row (z == 63) had no
// +X / +Z neighbour sample to stitch from. ExportChunkBatch skipped their stitch
// behind a border guard, and the 127 resulting chunks baked with unwritten
// vertices + (0,0,0) index triples -- which the runtime chunk-topology validator
// rejects, dropping them from LOW LOD and physics alike (no always-resident
// geometry and no collision on the outer +X/+Z strip).
//
// HEADLESS-SAFE -- pure integer arithmetic. No heightmap, no task system, no file.
//------------------------------------------------------------------------------

namespace
{
	// The shipping grid: Flux_TerrainConfig's 64x64 chunks, at the two baked
	// densities (HIGH = divisor 1 -> 64 quads/chunk, LOW + physics = divisor 4 -> 16).
	constexpr uint32_t uTEST_CHUNK_GRID = 64u;
	constexpr uint32_t uTEST_CELLS_HIGH = 64u;
	constexpr uint32_t uTEST_CELLS_LOW  = 16u;
}

ZENITH_TEST(FluxTerrainSourceGrid, SourceGridCarriesAClosingSamplePerEdge)
{
	// THE REGRESSION CLAUSE. A quad grid needs one more sample than it has cells;
	// the old exporter provided exactly cells, which is what starved the last chunk.
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::SampleCountForCells(0u), 1u,
		"a zero-cell grid is still one sample wide");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::SampleCountForCells(4096u), 4097u,
		"4096 quads need 4097 samples");

	const uint32_t uHigh = Flux_TerrainSourceGrid::SampleCountPerEdge(uTEST_CHUNK_GRID, uTEST_CELLS_HIGH);
	const uint32_t uLow  = Flux_TerrainSourceGrid::SampleCountPerEdge(uTEST_CHUNK_GRID, uTEST_CELLS_LOW);
	ZENITH_ASSERT_EQ(uHigh, 4097u, "the HIGH source grid must be 4097 samples per edge, not 4096");
	ZENITH_ASSERT_EQ(uLow, 1025u, "the LOW/physics source grid must be 1025 samples per edge, not 1024");

	ZENITH_ASSERT_GT(uHigh, uTEST_CHUNK_GRID * uTEST_CELLS_HIGH,
		"the source grid must exceed cells*chunks -- equality is the starved grid that dropped 127 chunks");
	ZENITH_ASSERT_GT(uLow, uTEST_CHUNK_GRID * uTEST_CELLS_LOW,
		"the source grid must exceed cells*chunks at LOW density too");

	// The two spellings the exporter uses (GenerateFullTerrain counts cells,
	// ExportChunkBatch counts chunks) must agree, or the row stride would differ
	// between the producer and the consumer of the same array.
	ZENITH_ASSERT_EQ(uHigh, Flux_TerrainSourceGrid::SampleCountForCells(uTEST_CHUNK_GRID * uTEST_CELLS_HIGH),
		"SampleCountPerEdge and SampleCountForCells must describe the same grid");
}

ZENITH_TEST(FluxTerrainSourceGrid, ChunkClosingEdgeIsItsNeighboursFirstEdge)
{
	// The stitch identity, and the reason a chunk needs cells+1 samples: a chunk's
	// closing column IS its +X neighbour's first column, so adjacent baked chunks
	// share those vertices exactly and the terrain is seamless.
	const uint32_t uSamples = Flux_TerrainSourceGrid::SampleCountPerEdge(uTEST_CHUNK_GRID, uTEST_CELLS_LOW);

	for (uint32_t uChunk = 0u; uChunk + 1u < uTEST_CHUNK_GRID; uChunk++)
	{
		for (uint32_t uAlong = 0u; uAlong <= uTEST_CELLS_LOW; uAlong++)
		{
			ZENITH_ASSERT_EQ(
				Flux_TerrainSourceGrid::SampleIndex(uChunk, 7u, uTEST_CELLS_LOW, uAlong, uTEST_CELLS_LOW, uSamples),
				Flux_TerrainSourceGrid::SampleIndex(uChunk + 1u, 7u, 0u, uAlong, uTEST_CELLS_LOW, uSamples),
				"chunk %u's closing +X column must be chunk %u's first column", uChunk, uChunk + 1u);

			ZENITH_ASSERT_EQ(
				Flux_TerrainSourceGrid::SampleIndex(7u, uChunk, uAlong, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uSamples),
				Flux_TerrainSourceGrid::SampleIndex(7u, uChunk + 1u, uAlong, 0u, uTEST_CELLS_LOW, uSamples),
				"chunk %u's closing +Z row must be chunk %u's first row", uChunk, uChunk + 1u);
		}
	}
}

ZENITH_TEST(FluxTerrainSourceGrid, LastChunkClosingSampleStaysInRange)
{
	// The 127 chunks that used to be dropped: every chunk on the positive border,
	// plus the (63,63) corner whose closing sample is the very last in the grid.
	// If this ever fails, the border special case is back.
	const uint32_t uSamples = Flux_TerrainSourceGrid::SampleCountPerEdge(uTEST_CHUNK_GRID, uTEST_CELLS_LOW);
	const uint32_t uTotal = uSamples * uSamples;
	const uint32_t uLast = uTEST_CHUNK_GRID - 1u;

	for (uint32_t u = 0u; u < uTEST_CHUNK_GRID; u++)
	{
		// +X border chunk, its closing column.
		ZENITH_ASSERT_LT(
			Flux_TerrainSourceGrid::SampleIndex(uLast, u, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uSamples),
			uTotal, "last-column chunk (%u,%u) must have an in-range closing sample", uLast, u);
		// +Z border chunk, its closing row.
		ZENITH_ASSERT_LT(
			Flux_TerrainSourceGrid::SampleIndex(u, uLast, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uSamples),
			uTotal, "last-row chunk (%u,%u) must have an in-range closing sample", u, uLast);
	}

	ZENITH_ASSERT_EQ(
		Flux_TerrainSourceGrid::SampleIndex(uLast, uLast, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uTEST_CELLS_LOW, uSamples),
		uTotal - 1u,
		"the corner chunk's closing sample must be exactly the last sample in the grid");
}

ZENITH_TEST(FluxTerrainSourceGrid, EverySampleIsReachedAndNoChunkReadsOutOfRange)
{
	// Exhaustive coverage on a small grid: walk EVERY chunk's full (cells+1)^2 slot
	// set and assert (a) no read leaves the array and (b) the union is the whole
	// grid. Together these say the split is total -- no orphaned samples, and no
	// chunk depending on data that does not exist. That second half is precisely
	// what the border chunks used to violate.
	constexpr uint32_t uCHUNKS = 4u;
	constexpr uint32_t uCELLS = 2u;
	const uint32_t uSamples = Flux_TerrainSourceGrid::SampleCountPerEdge(uCHUNKS, uCELLS);   // 9
	const uint32_t uTotal = uSamples * uSamples;                                             // 81
	ZENITH_ASSERT_EQ(uTotal, 81u, "the fixture grid must be 9x9 samples");

	bool abSeen[81] = {};
	for (uint32_t uChunkZ = 0u; uChunkZ < uCHUNKS; uChunkZ++)
	{
		for (uint32_t uChunkX = 0u; uChunkX < uCHUNKS; uChunkX++)
		{
			uint32_t uSlots = 0u;
			for (uint32_t uSubZ = 0u; uSubZ <= uCELLS; uSubZ++)
			{
				for (uint32_t uSubX = 0u; uSubX <= uCELLS; uSubX++)
				{
					const uint32_t uIndex = Flux_TerrainSourceGrid::SampleIndex(
						uChunkX, uChunkZ, uSubX, uSubZ, uCELLS, uSamples);
					ZENITH_ASSERT_LT(uIndex, uTotal,
						"chunk (%u,%u) sample (%u,%u) reads sample %u, past the %u-sample grid",
						uChunkX, uChunkZ, uSubX, uSubZ, uIndex, uTotal);
					abSeen[uIndex] = true;
					uSlots++;
				}
			}
			ZENITH_ASSERT_EQ(uSlots, Flux_TerrainSourceGrid::ChunkVertexCount(uCELLS),
				"chunk (%u,%u) must fill exactly one vertex slot per source sample", uChunkX, uChunkZ);
		}
	}

	for (uint32_t u = 0u; u < uTotal; u++)
	{
		ZENITH_ASSERT_TRUE(abSeen[u], "source sample %u is not read by any chunk", u);
	}
}

ZENITH_TEST(FluxTerrainSourceGrid, ChunkSlotCountsMatchTheOnDiskContract)
{
	// The exporter sizes its sub-mesh from ChunkVertexCount/ChunkIndexCount while the
	// loader validates against Zenith_TerrainChunkLayout. A drift between the two is a
	// silently rejected bake, so tie them together here.
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkVertexCount(uTEST_CELLS_HIGH),
		Zenith_TerrainChunkLayout::uHIGH_CHUNK_VERTEX_COUNT,
		"HIGH chunk vertex count must match the on-disk contract");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkIndexCount(uTEST_CELLS_HIGH),
		Zenith_TerrainChunkLayout::uHIGH_CHUNK_INDEX_COUNT,
		"HIGH chunk index count must match the on-disk contract");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkVertexCount(uTEST_CELLS_LOW),
		Zenith_TerrainChunkLayout::uLOW_CHUNK_VERTEX_COUNT,
		"LOW chunk vertex count must match the on-disk contract");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkIndexCount(uTEST_CELLS_LOW),
		Zenith_TerrainChunkLayout::uLOW_CHUNK_INDEX_COUNT,
		"LOW chunk index count must match the on-disk contract");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkVertexCount(uTEST_CELLS_LOW),
		Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_VERTEX_COUNT,
		"physics chunk vertex count must match the on-disk contract");
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkIndexCount(uTEST_CELLS_LOW),
		Zenith_TerrainChunkLayout::uPHYSICS_CHUNK_INDEX_COUNT,
		"physics chunk index count must match the on-disk contract");

	// The stitch is exactly one column + one row + one corner on top of the interior.
	ZENITH_ASSERT_EQ(Flux_TerrainSourceGrid::ChunkVertexCount(uTEST_CELLS_LOW),
		(uTEST_CELLS_LOW * uTEST_CELLS_LOW) + uTEST_CELLS_LOW + uTEST_CELLS_LOW + 1u,
		"a chunk is its interior plus a +X column, a +Z row and one corner vertex");
}

#endif // ZENITH_TESTING
