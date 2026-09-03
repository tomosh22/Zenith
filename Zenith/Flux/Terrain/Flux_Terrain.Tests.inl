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
// Also: the packed-vertex quantisation bridge (Flux_TerrainVertexQuant.h) and the
// TerrainConstants CB fill -- the two halves of the 20-byte flip that nothing else
// pins at runtime (the static_asserts pin layout, these pin VALUES and bytes).
//
// HEADLESS-SAFE -- assertions are over constexpr integers, pure codec math on
// stack buffers, and one already-filled file-static. No device, no pipeline
// object, no command buffer is touched.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Core/Zenith_TerrainDimensions.h"
#include "Flux/Terrain/Flux_TerrainSourceGrid.h"
#include "Flux/Terrain/Flux_TerrainVertexQuant.h"
// Phase 2 of the terrain indirect-count compatibility plan: the terrain
// indirect-command allocation/seed and the per-frame zero-tail invariant
// ride the shared 20-byte / five-word ABI defined in
// Flux/Backend/Flux_IndirectDraw.h. These tests pin the terrain-specific
// arithmetic that lives on top of that ABI.
#include "Flux/Backend/Flux_IndirectDraw.h"
// Terrain shadow casting: the pure slot arithmetic + caster LOD policy, the
// per-cascade cull block builder, the feature table (declaration order) and the
// render graph (a stack graph exercising the reset -> cull -> cascade shape).
#include "Flux/Terrain/Flux_TerrainShadowCull.h"
#include "Flux/Terrain/Flux_TerrainGPUStructs.h"
#include "Flux/Flux_FeatureRegistry.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"

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

// ---- the packed-vertex quantisation bridge ----------------------------------

ZENITH_TEST(FluxTerrain, TerrainConstantsFillMatchesTheTerrainsOwnBox)
{
	// The CB fill is the ONE bridge from a terrain's authored box to the three
	// shaders' Flux_DequantPosition, and nothing else validates it — the
	// static_asserts beside the struct pin layout, not values. A dropped fill
	// leaves scale at its 1.0f default (terrain collapses to a metre cube at the
	// origin); a dropped .w makes every terrain UV MaxWorldSize-times too small.
	//
	// It is filled PER TERRAIN now, so the test drives the filler directly across
	// several specs rather than inspecting one process-wide instance — which is
	// also the only way to catch a fill that reads a global instead of its
	// argument, the exact regression the per-terrain box introduces the risk of.
	auto CheckSpec = [](const Zenith_TerrainDimensions& xDims, const char* szWhat)
	{
		TerrainConstants xFilled;
		Flux_FillTerrainConstants(xDims, xFilled);

		const Flux_PosQuant xExpected = Flux_MakeTerrainPosQuant(xDims);
		for (int i = 0; i < 3; i++)
		{
			ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afPosQuantScale[i], xExpected.m_xScale[i], 1.0e-4f,
				"%s: CB dequant scale axis %d must be this terrain's box extent", szWhat, i);
			ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afPosQuantBias[i], xExpected.m_xBias[i], 1.0e-4f,
				"%s: CB dequant bias axis %d must be this terrain's box min", szWhat, i);
		}
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afPosQuantScale[3], xDims.MaxWorldSize(), 1.0e-4f,
			"%s: CB scale.w must carry the UV dequant extent the shaders multiply the unorm16 UV back up by", szWhat);
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afTerrainDims[0], xDims.m_fChunkWorldSize, 1.0e-4f,
			"%s: the dims lane must carry the chunk world size the debug modes draw the grid with", szWhat);
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afTerrainDims[1], xDims.WorldSizeX(), 1.0e-4f,
			"%s: the dims lane must carry the terrain's world X extent", szWhat);
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afTerrainDims[2], xDims.WorldSizeZ(), 1.0e-4f,
			"%s: the dims lane must carry the terrain's world Z extent", szWhat);
	};

	// DEFAULT: reproduces the historical 4096/512 box exactly. This is the arm
	// that keeps a default-dimensioned terrain rendering where it always did.
	const Zenith_TerrainDimensions xDefault = Zenith_TerrainDimensions::Default();
	CheckSpec(xDefault, "default");
	{
		TerrainConstants xFilled;
		Flux_FillTerrainConstants(xDefault, xFilled);
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afPosQuantScale[3],
			Zenith_TerrainChunkLayout::fUV_BOX_MAX, 1.0e-4f,
			"the default UV extent must still be the layout header's historical constant");
	}

	// NON-DEFAULT and NON-SQUARE. The narrow spec is Route1's shape: its X box is
	// six times tighter than its Z box, which is the whole point of a per-axis
	// box, and its UV extent is the LONGER axis so the square authoring images
	// still cover it.
	Zenith_TerrainDimensions xNarrow;
	xNarrow.m_fChunkWorldSize = 64.0f;
	xNarrow.m_uQuadsPerChunkEdge = 64u;
	xNarrow.m_uGridChunksX = 4u;
	xNarrow.m_uGridChunksZ = 24u;
	CheckSpec(xNarrow, "narrow 4x24");
	{
		TerrainConstants xFilled;
		Flux_FillTerrainConstants(xNarrow, xFilled);
		ZENITH_ASSERT_LT(xFilled.m_afPosQuantScale[0], xFilled.m_afPosQuantScale[2],
			"a narrow terrain must spend LESS box on X than on Z — that is the per-axis precision win");
		ZENITH_ASSERT_EQ_FLOAT(xFilled.m_afPosQuantScale[3], 1536.0f, 1.0e-4f,
			"the UV extent must be the LONGER axis, so the square authoring maps still cover the terrain");
	}

	// A denser, smaller terrain: both knobs off their defaults at once.
	Zenith_TerrainDimensions xDense;
	xDense.m_fChunkWorldSize = 32.0f;
	xDense.m_uQuadsPerChunkEdge = 64u;
	xDense.m_uGridChunksX = 10u;
	xDense.m_uGridChunksZ = 14u;
	CheckSpec(xDense, "dense 10x14 @ 32m");
}

ZENITH_TEST(FluxTerrain, TerrainConstantsAreIndependentAcrossSpecs)
{
	// TWO terrains in one scene, filled back to back. The fill must be a pure
	// function of its argument: if it ever reads a shared instance again, the
	// second fill would overwrite the first and both terrains would decode
	// against one box — geometry in the wrong place, with nothing failing.
	Zenith_TerrainDimensions xSmall;
	xSmall.m_fChunkWorldSize = 64.0f;
	xSmall.m_uQuadsPerChunkEdge = 64u;
	xSmall.m_uGridChunksX = 6u;
	xSmall.m_uGridChunksZ = 9u;

	TerrainConstants xA;
	TerrainConstants xB;
	Flux_FillTerrainConstants(xSmall, xA);
	Flux_FillTerrainConstants(Zenith_TerrainDimensions::Default(), xB);

	// Re-fill A's spec into a third block AFTER B: A's values must be unchanged.
	TerrainConstants xARepeat;
	Flux_FillTerrainConstants(xSmall, xARepeat);
	for (int i = 0; i < 4; i++)
	{
		ZENITH_ASSERT_EQ_FLOAT(xARepeat.m_afPosQuantScale[i], xA.m_afPosQuantScale[i], 1.0e-4f,
			"filling a second terrain must not disturb the first terrain's box (lane %d)", i);
	}
	ZENITH_ASSERT_LT(xA.m_afPosQuantScale[0], xB.m_afPosQuantScale[0],
		"a 384m-wide terrain's X box must be tighter than a 4096m one's");
	ZENITH_ASSERT_EQ_FLOAT(xB.m_afPosQuantScale[0], 4096.0f, 1.0e-4f,
		"the default terrain's X box must remain the historical 4096m");
}

ZENITH_TEST(FluxTerrain, TerrainVertexQuantWritesLandAtShaderOffsets)
{
	// The bridge writes through the NAMED offset constants and the shader fetches at
	// its REFLECTED offsets; the static_asserts in Flux_TerrainStreamingManager.cpp
	// pin those equal at compile time, and this pins the bytes at runtime: decode at
	// the GENERATED offsets and the authored values must come back. Against a 0xCD
	// sentinel fill, a transposed offset decodes wild values, not near-misses.
	const Zenith_TerrainDimensions xDims = Zenith_TerrainDimensions::Default();
	const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant(xDims);
	const float fUVBoxMax = Flux_TerrainUVBoxMax(xDims);
	const Zenith_Maths::Vector3 xPos(123.0f, 45.0f, 678.0f);
	const Zenith_Maths::Vector2 xUV(321.0f, 87.0f);
	const u_int uNormalWord = 0xA1B2C3D4u;
	const u_int uTangentWord = 0x1F2E3D4Cu;

	u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE];
	std::memset(aucVertex, 0xCD, sizeof(aucVertex));
	Flux_WriteTerrainVertexPosition(aucVertex, xPos, xQuant);
	Flux_WriteTerrainVertexUV(aucVertex, xUV, fUVBoxMax);
	Flux_WriteTerrainVertexNormalWord(aucVertex, uNormalWord);
	Flux_WriteTerrainVertexTangentWord(aucVertex, uTangentWord);

	const Flux_VertexLayoutElement* paxShader =
		Flux_Generated_Terrain::Terrain_ToGBuffer::kVertexLayout.m_paxElements;

	u_int64 ulPosWord = 0u;
	std::memcpy(&ulPosWord, aucVertex + paxShader[0].m_uOffset, sizeof(ulPosWord));
	const Zenith_Maths::Vector3 xDecodedPos = Flux_UnpackSnorm16x4Position(ulPosWord, xQuant);
	for (int i = 0; i < 3; i++)
	{
		ZENITH_ASSERT_EQ_FLOAT(xDecodedPos[i], xPos[i],
			Zenith_TerrainChunkLayout::PositionQuantStep(static_cast<uint32_t>(i)),
			"the bytes at the shader's POSITION offset must decode to the authored position (axis %d)", i);
	}

	u_int uUVWord = 0u;
	std::memcpy(&uUVWord, aucVertex + paxShader[1].m_uOffset, sizeof(uUVWord));
	const Zenith_Maths::Vector2 xDecodedUV = Flux_UnpackUnorm16x2(uUVWord) * fUVBoxMax;
	for (int i = 0; i < 2; i++)
	{
		ZENITH_ASSERT_EQ_FLOAT(xDecodedUV[i], xUV[i], Zenith_TerrainChunkLayout::fUV_QUANT_STEP,
			"the bytes at the shader's TEXCOORD offset must decode to the authored UV (axis %d)", i);
	}

	u_int uReadNormal = 0u, uReadTangent = 0u;
	std::memcpy(&uReadNormal, aucVertex + paxShader[2].m_uOffset, sizeof(uReadNormal));
	std::memcpy(&uReadTangent, aucVertex + paxShader[3].m_uOffset, sizeof(uReadTangent));
	ZENITH_ASSERT_EQ(uReadNormal, uNormalWord,
		"the normal word must sit exactly at the shader's NORMAL offset");
	ZENITH_ASSERT_EQ(uReadTangent, uTangentWord,
		"the tangent word must sit exactly at the shader's TANGENT offset");
}

ZENITH_TEST(FluxTerrain, TerrainVertexQuantReencodeIsIdentity)
{
	// The sculpt hook and the CityBuilder carve decode a packed vertex and re-encode
	// it with only Y rewritten — seam-safe ONLY if unpack->pack reproduces the word
	// bit-exactly (a quantum representative must be its own fixed point), because
	// the un-brushed neighbour chunk keeps its original baked words along the shared
	// border. Words are integers, so exact equality is /fp:fast-safe.
	//
	// Run over THREE specs: the identity has to hold for whatever box a terrain
	// carries, not just the historical one, because the sculpt hook re-encodes
	// against the terrain it is attached to.
	Zenith_TerrainDimensions axSpecs[3];
	axSpecs[0] = Zenith_TerrainDimensions::Default();
	axSpecs[1] = { 64.0f, 64u, 6u, 9u };
	axSpecs[2] = { 32.0f, 128u, 4u, 24u };

	for (u_int uSpec = 0; uSpec < 3u; uSpec++)
	{
		const Zenith_TerrainDimensions& xDims = axSpecs[uSpec];
		const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant(xDims);
		u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE] = {};
		for (u_int u = 0; u < 97u; u++)
		{
			// Off-lattice authored positions across the whole box — the first pack
			// rounds arbitrarily; the identity under test is the SECOND pack.
			const float fT = static_cast<float>(u) / 96.0f;
			const Zenith_Maths::Vector3 xAuthored(
				xDims.WorldSizeX() * fT, 512.0f * (1.0f - fT), xDims.WorldSizeZ() * fT * fT);
			Flux_WriteTerrainVertexPosition(aucVertex, xAuthored, xQuant);
			u_int64 ulFirst = 0u;
			std::memcpy(&ulFirst, aucVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, sizeof(ulFirst));

			const Zenith_Maths::Vector3 xDecoded = Flux_ReadTerrainVertexPosition(aucVertex, xQuant);
			Flux_WriteTerrainVertexPosition(aucVertex, xDecoded, xQuant);
			u_int64 ulSecond = 0u;
			std::memcpy(&ulSecond, aucVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, sizeof(ulSecond));

			ZENITH_ASSERT_EQ(ulFirst, ulSecond,
				"decode->re-encode must be the identity on the packed word (spec %u case %u) — "
				"a sculpted chunk's untouched XZ words have to survive the round trip", uSpec, u);
		}
	}
}

ZENITH_TEST(FluxTerrain, TerrainVertexQuantUVRoundTrip)
{
	// The UV pair is the one asymmetric couple in the bridge (write divides by the
	// extent, read multiplies it back) and nothing else exercises the read side.
	// Integer pixel coordinates — all the exporter ever authors — must come back
	// within one unorm16 quantum AND snap back to the exact integer, which is the
	// property the sculpt hook's std::round of the decoded UV stands on.
	// Two specs. The default one keeps the historical 4096m extent (where a UV is
	// both a metre and a heightfield pixel); the small one is where the two
	// readings come apart, and where the quantum gets FINER rather than coarser.
	Zenith_TerrainDimensions axSpecs[2];
	axSpecs[0] = Zenith_TerrainDimensions::Default();
	axSpecs[1] = { 64.0f, 64u, 6u, 9u };

	u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE] = {};
	for (u_int uSpec = 0; uSpec < 2u; uSpec++)
	{
		const float fUVBoxMax = Flux_TerrainUVBoxMax(axSpecs[uSpec]);
		const float fQuantStep = fUVBoxMax / 65535.0f;
		for (u_int u = 0; u <= 16u; u++)
		{
			// Integer metres, which is what the exporter authors at any spacing
			// that divides a metre.
			const float fMetres = std::floor(fUVBoxMax * static_cast<float>(u) / 16.0f);
			const Zenith_Maths::Vector2 xAuthored(fMetres, std::floor(fUVBoxMax) - fMetres);
			Flux_WriteTerrainVertexUV(aucVertex, xAuthored, fUVBoxMax);
			const Zenith_Maths::Vector2 xDecoded = Flux_ReadTerrainVertexUV(aucVertex, fUVBoxMax);
			for (int i = 0; i < 2; i++)
			{
				ZENITH_ASSERT_TRUE(
					std::fabs(xDecoded[i] - xAuthored[i]) <= fQuantStep,
					"decoded UV axis %d must sit within one unorm16 quantum of the authored metre "
					"(spec %u case %u)", i, uSpec, u);
				ZENITH_ASSERT_EQ_FLOAT(std::round(xDecoded[i]), xAuthored[i], 1.0e-4f,
					"an integer authored UV must snap back exactly (spec %u case %u axis %d)", uSpec, u, i);
			}
		}
	}

	// The precision claim in the layout header: a SMALLER terrain gets a FINER
	// UV quantum, so shrinking a terrain can never cost UV precision.
	ZENITH_ASSERT_LT(Flux_TerrainUVBoxMax(axSpecs[1]) / 65535.0f,
		Flux_TerrainUVBoxMax(axSpecs[0]) / 65535.0f,
		"a smaller square authoring domain must give a finer unorm16 UV quantum");
}

//------------------------------------------------------------------------------
// Terrain indirect-command ABI (Phase 2 of the terrain indirect-count
// compatibility plan). The terrain allocation/seed in
// Zenith_TerrainComponent::InitializeCullingResources sizes the persistent
// per-terrain argument buffer as TOTAL_CHUNKS * the shared 20-byte stride, and
// the GPU reset pass clears every record to the legal no-op every frame so a
// fixed indexed-indirect draw over [0, TOTAL_CHUNKS) is valid even when the
// count buffer's value is smaller. These tests pin:
//   - the allocation size equals TOTAL_CHUNKS * stride (the seeded byte size);
//   - the last record ends exactly at the allocation boundary;
//   - the recorded TOTAL_CHUNKS matches the shipping 4096.
//
// Pure arithmetic over constexpr constants — no device, no buffer, no VRAM.
//------------------------------------------------------------------------------

ZENITH_TEST(FluxTerrain, IndirectBufferAllocationMatchesAbiStride)
{
	// The allocation in Zenith_TerrainComponent::InitializeCullingResources
	// sizes the persistent argument buffer as TOTAL_CHUNKS * stride, with
	// stride = uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE = 20. This pins that
	// the C++ allocation, the Slang shared include's uFLUX_TERRAIN_INDIRECT_
	// BYTE_STRIDE, and the test baselines all refer to one definition; a drift
	// here would let the allocation over-/under-flow the records the reset
	// shader clears.
	constexpr uint64_t ulAllocationBytes =
		static_cast<uint64_t>(Flux_TerrainConfig::TOTAL_CHUNKS) * uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE;
	ZENITH_ASSERT_EQ(ulAllocationBytes, static_cast<uint64_t>(4096u * 20u),
		"terrain indirect-buffer allocation must be TOTAL_CHUNKS * 20 = 81920 bytes");
	ZENITH_ASSERT_EQ(ulAllocationBytes,
		static_cast<uint64_t>(Flux_TerrainConfig::TOTAL_CHUNKS) * sizeof(Flux_IndirectDrawIndexedCommand),
		"the allocation equals TOTAL_CHUNKS * sizeof(Flux_IndirectDrawIndexedCommand)");
}

ZENITH_TEST(FluxTerrain, IndirectBufferLastRecordEndsAtAllocationBoundary)
{
	// The last record in the seeded buffer ends at byte offset
	//   TOTAL_CHUNKS * uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE
	// == the allocation size. A furter tail-bounds read by the reset shader
	// or by the indexed-indirect command would be out of range; the planner's
	// last-record test in Flux_IndirectDraw.Tests.inl pins the batch end.
	constexpr uint32_t uTOTAL = Flux_TerrainConfig::TOTAL_CHUNKS;
	constexpr uint32_t uSTRIDE = uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE;
	constexpr uint32_t uLastOffset = (uTOTAL - 1u) * uSTRIDE;
	constexpr uint32_t uLastEnd = uLastOffset + uSTRIDE;
	ZENITH_ASSERT_EQ(uLastEnd, uTOTAL * uSTRIDE,
		"the last record's end byte must equal TOTAL_CHUNKS * stride — exactly the allocation boundary");
}

ZENITH_TEST(FluxTerrainSourceGrid, SampleToWorldToImageMappingSeparatesThreeUnits)
{
	// The exporter used to weld three quantities into one number: a heightmap
	// PIXEL was a source SAMPLE was a world METRE. These functions are where they
	// come apart, and they are the arithmetic a fractional bilinear tap stands on.
	using namespace Flux_TerrainSourceGrid;

	// DEFAULT dimensions over a 4096px heightfield: the step is exactly the
	// divisor and the image scale is exactly 1.0, so every result is the integer
	// the old code produced. This arm is what makes a default re-bake
	// byte-identical, so it asserts EXACT equality.
	{
		const Zenith_TerrainDimensions xDims = Zenith_TerrainDimensions::Default();
		const double dSpacing = static_cast<double>(xDims.VertexSpacing());
		const double dImagePerWorld = static_cast<double>(xDims.ImagePixelPerWorld(4096u));
		ZENITH_ASSERT_TRUE(dSpacing == 1.0, "default vertex spacing must be exactly 1m");
		ZENITH_ASSERT_TRUE(dImagePerWorld == 1.0, "a 4096px map over 4096m must be exactly 1px per metre");

		ZENITH_ASSERT_TRUE(SampleStepWorld(dSpacing, 1u) == 1.0, "the HIGH bake steps one metre per sample");
		ZENITH_ASSERT_TRUE(SampleStepWorld(dSpacing, 4u) == 4.0, "a divisor-4 bake steps four metres per sample");
		ZENITH_ASSERT_TRUE(WorldForSample(1000u, 1.0) == 1000.0, "sample 1000 is 1000m in at 1m spacing");
		ZENITH_ASSERT_TRUE(WorldForSample(1000u, 4.0) == 4000.0, "sample 1000 is 4000m in at 4m spacing");
		ZENITH_ASSERT_TRUE(ImageCoordForSample(1000u, 1.0, dImagePerWorld) == 1000.0,
			"at 1m/px the image coordinate IS the sample index — the identity the old code assumed");
		ZENITH_ASSERT_TRUE(ImageCoordForSample(1000u, 4.0, dImagePerWorld) == 4000.0,
			"a divisor-4 sample taps four times as far into the image");
	}

	// A SMALLER terrain over the SAME 4096px heightfield: the image is now
	// oversampled, and the tap coordinate is FRACTIONAL. This is the case a
	// closing sample has to clamp and an integer-only tap would silently truncate.
	{
		const Zenith_TerrainDimensions xDims{ 64.0f, 64u, 6u, 9u };   // 384 x 576 m
		ZENITH_ASSERT_TRUE(xDims.MaxWorldSize() == 576.0f, "the square domain is the longer axis");
		const double dSpacing = static_cast<double>(xDims.VertexSpacing());
		const double dImagePerWorld = static_cast<double>(xDims.ImagePixelPerWorld(4096u));
		ZENITH_ASSERT_TRUE(dSpacing == 1.0, "spacing is unchanged — only the extent shrank");

		// 4096 px over 576 m is a hair over 7.1 px per metre: fractional, and the
		// bilinear tap has to interpolate rather than land on a texel.
		const double dCoord = ImageCoordForSample(1u, SampleStepWorld(dSpacing, 1u), dImagePerWorld);
		ZENITH_ASSERT_GT(dCoord, 7.0, "one metre must tap more than seven pixels in on a 576m domain");
		ZENITH_ASSERT_LT(dCoord, 7.2, "...and not more than about 7.11");
		ZENITH_ASSERT_TRUE(dCoord != static_cast<double>(static_cast<int>(dCoord)),
			"the tap coordinate must be genuinely fractional — an integer-only tap would truncate it");

		// The LAST sample of the grid lands exactly on the image's far edge, which
		// is where the closing sample's clamp takes over.
		const uint32_t uLastSample = SampleCountPerEdge(xDims.m_uGridChunksZ, xDims.m_uQuadsPerChunkEdge) - 1u;
		ZENITH_ASSERT_EQ(uLastSample, 576u, "a 9x64-quad axis closes on sample 576");
		ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(WorldForSample(uLastSample, SampleStepWorld(dSpacing, 1u))),
			xDims.WorldSizeZ(), 1.0e-3f,
			"the closing sample must land exactly on the terrain's outer boundary");
		ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(ImageCoordForSample(uLastSample,
			SampleStepWorld(dSpacing, 1u), dImagePerWorld)), 4096.0f, 1.0e-2f,
			"the closing sample taps one texel PAST the image, which is what the clamp exists for");
	}

	// A DENSER terrain: the sample step drops below a metre, which is the knob
	// nothing shipped uses yet and would therefore never be exercised otherwise.
	{
		const Zenith_TerrainDimensions xDims{ 64.0f, 128u, 8u, 8u };   // 0.5m spacing
		ZENITH_ASSERT_TRUE(xDims.VertexSpacing() == 0.5f, "128 quads over a 64m chunk is 0.5m spacing");
		ZENITH_ASSERT_TRUE(SampleStepWorld(0.5, 1u) == 0.5, "a HIGH sample advances half a metre");
		ZENITH_ASSERT_TRUE(SampleStepWorld(0.5, 4u) == 2.0, "a divisor-4 sample advances two metres");
		// Twice the samples across the same extent.
		ZENITH_ASSERT_EQ(SampleCountPerEdge(xDims.m_uGridChunksX, xDims.m_uQuadsPerChunkEdge), 1025u,
			"8 chunks of 128 quads need 1025 samples per edge");
	}
}

ZENITH_TEST(FluxTerrain, TotalChunksPinnedToFourThousandNinetySix)
{
	// The shipping 64x64 chunk grid. The GPU reset shader's bounds check
	// derives from GetDimensions on the bound argument buffer, so changing
	// TOTAL_CHUNKS changes the dispatch group count in Flux_Terrain.cpp's
	// ExecuteResetCounters (currently ceil(4096/64) = 64 groups). A change
	// here that fails this test means the reset's Dispatch() call must be
	// re-derived in lockstep.
	ZENITH_ASSERT_EQ(Flux_TerrainConfig::TOTAL_CHUNKS, 4096u,
		"TOTAL_CHUNKS is the shipping 64x64 grid — change requires regenerating shaders and updating the reset dispatch count in lockstep");
}

//------------------------------------------------------------------------------
// Terrain shadow casting — the pure parts.
//
// Terrain casts into the CSM cascades by giving each cascade its own SLOT of a
// second indirect/count buffer pair, filled by the one terrain culling dispatch
// (rows y = 1 + cascade) and drawn by each "Shadow Cascade N" pass with two byte
// offsets. Everything the GPU side relies on is arithmetic the CPU can pin:
//   - the slot layout (contiguous, disjoint, ends exactly at the allocation),
//   - the caster LOD policy (camera-matched below the force threshold, LOW at/above),
//   - the per-cascade cull block (planes == the shared frustum extraction, and an
//     ortho box actually classifies a chunk AABB the way the shader will),
//   - the render-graph shape (the cascade READ links to the EARLIER cull WRITE, and
//     the barrier synthesised is compute-write -> indirect-arg-read),
//   - the feature table order that makes that link exist at all.
// HEADLESS-SAFE: constexpr arithmetic, glm on the stack, and a stack render graph
// with buffer-only passes (no transients, no device).
//------------------------------------------------------------------------------

ZENITH_TEST(FluxTerrain, ShadowCullSlotCountIsTheCascadeCount)
{
	// One slot per cascade — the shader's TERRAIN_SHADOW_CULL_VIEWS row count, the
	// pure header's view count and the CSM count are one number (also static_asserted
	// in Flux_Terrain.cpp; pinned here so the value itself is visible).
	ZENITH_ASSERT_EQ(uFLUX_TERRAIN_SHADOW_CULL_VIEWS, 4u, "four cascade slots");
	ZENITH_ASSERT_EQ(uFLUX_TERRAIN_SHADOW_CULL_VIEWS, static_cast<uint32_t>(ZENITH_FLUX_NUM_CSMS),
		"slot count must equal ZENITH_FLUX_NUM_CSMS");
	ZENITH_ASSERT_EQ(uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER, uFLUX_TERRAIN_SHADOW_CULL_VIEWS,
		"'never force LOW' is spelled as the slot count so every cascade index compares below it");
}

ZENITH_TEST(FluxTerrain, ShadowCullSlotsAreContiguousDisjointAndEndAtTheAllocation)
{
	constexpr uint32_t uCAP    = Flux_TerrainConfig::TOTAL_CHUNKS;
	constexpr uint32_t uSTRIDE = uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE;
	constexpr uint32_t uVIEWS  = uFLUX_TERRAIN_SHADOW_CULL_VIEWS;

	// Slot 0 is the camera draw's shape exactly (offset 0) — the cascade draw IS the
	// camera draw with two offsets, which is what lets the recorder's ZERO_PADDED_TO_MAX
	// contract carry over unchanged.
	ZENITH_ASSERT_EQ(Flux_TerrainShadowCullIndirectByteOffset(0u, uCAP, uSTRIDE), 0u, "cascade 0's slot starts at byte 0");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowCullCountByteOffset(0u), 0u, "cascade 0's count is the first uint");

	// Each slot is a full chunk-capacity range and the next begins where it ends.
	for (uint32_t c = 0; c < uVIEWS; ++c)
	{
		const uint32_t uBegin = Flux_TerrainShadowCullIndirectByteOffset(c, uCAP, uSTRIDE);
		const uint32_t uEnd   = uBegin + uCAP * uSTRIDE;
		ZENITH_ASSERT_EQ(uBegin, c * uCAP * uSTRIDE, "cascade %u slot begins at c * capacity * stride", c);
		ZENITH_ASSERT_EQ(uBegin % 4u, 0u, "cascade %u indirect offset must be 4-byte aligned (VUID 02710)", c);
		ZENITH_ASSERT_EQ(Flux_TerrainShadowCullCountByteOffset(c), c * 4u, "cascade %u count offset is c * sizeof(uint32)", c);
		if (c + 1u < uVIEWS)
		{
			ZENITH_ASSERT_EQ(uEnd, Flux_TerrainShadowCullIndirectByteOffset(c + 1u, uCAP, uSTRIDE),
				"cascade %u's slot must end exactly where cascade %u's begins (no gap, no overlap)", c, c + 1u);
		}
	}

	// The LAST record of the LAST slot ends exactly at the allocation boundary — the
	// recorder preflights [offset, offset + (max-1)*stride + 20] against the buffer
	// size, so an off-by-one here fails closed (zero terrain shadow) at runtime.
	const uint64_t ulAlloc = Flux_TerrainShadowCullIndirectBufferBytes(uVIEWS, uCAP, uSTRIDE);
	const uint64_t ulLastRecordEnd =
		static_cast<uint64_t>(Flux_TerrainShadowCullIndirectByteOffset(uVIEWS - 1u, uCAP, uSTRIDE)) +
		static_cast<uint64_t>(uCAP - 1u) * uSTRIDE + uSTRIDE;
	ZENITH_ASSERT_EQ(ulLastRecordEnd, ulAlloc, "the last cascade's last record must end at the allocation boundary");
	ZENITH_ASSERT_EQ(ulAlloc, static_cast<uint64_t>(4u * 4096u * 20u), "4 slots x 4096 records x 20 bytes = 327680");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowCullCountBufferBytes(uVIEWS), static_cast<uint64_t>(uVIEWS * sizeof(uint32_t)),
		"one uint32 per cascade");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowCullCountByteOffset(uVIEWS - 1u) + 4u,
		static_cast<uint32_t>(Flux_TerrainShadowCullCountBufferBytes(uVIEWS)),
		"the last cascade's count read [offset, +4) must fit the count buffer (VUID 02716)");
}

ZENITH_TEST(FluxTerrain, TerrainShadowCastingTogglesOffTheCullNotJustTheDraw)
{
	// The shipping state: shadows on, terrain casting on, every cascade active.
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(true, true, 4u), 4u,
		"all four cascades are filled when both switches are on");

	// EITHER switch off means ZERO slots. Zero is what makes this a real toggle:
	// Flux_BuildTerrainShadowCullData zeroes every plane and writes 0 into the
	// params, the cull rows early-out, the reset leaves every count at zero, and
	// the per-cascade draws have nothing to issue. An early-out in the draw alone
	// would still pay four cascades of chunk culling every frame.
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(false, true, 4u), 0u,
		"shadows off casts nothing");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(true, false, 4u), 0u,
		"'Render/Shadows/Terrain Casts Shadows' off casts nothing");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(false, false, 4u), 0u,
		"both off casts nothing");

	// A partially-activated cascade prefix is passed through, and the slot count is
	// the ceiling — a view registry reporting more cascades than there are slots
	// must not index past the allocation.
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(true, true, 0u), 0u, "no active cascade views, nothing to fill");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(true, true, 2u), 2u, "a two-cascade prefix fills two slots");
	ZENITH_ASSERT_EQ(Flux_TerrainShadowActiveCascades(true, true, 9u), uFLUX_TERRAIN_SHADOW_CULL_VIEWS,
		"never more slots than the allocation holds");

	// And the toggle must actually reach the GPU block: zero active cascades has to
	// leave every plane zeroed and the param word zero, or the cull would keep
	// culling against last frame's planes.
	Zenith_TerrainShadowCullGPU xOff;
	Zenith_Maths::Matrix4 axIdentity[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	for (u_int u = 0; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++u) axIdentity[u] = Zenith_Maths::Matrix4(1.0f);
	Flux_BuildTerrainShadowCullData(axIdentity,
		Flux_TerrainShadowActiveCascades(true, false, 4u), 3u, xOff);
	ZENITH_ASSERT_EQ(xOff.m_xParams.x, 0u, "the toggled-off block reports zero active cascades");
	for (u_int u = 0; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(xOff.m_axFrustumPlanes[u].m_xNormalAndDistance.w, 0.0f, 0.0f,
			"plane %u must be zeroed when terrain casting is off", u);
	}
}

ZENITH_TEST(FluxTerrain, ShadowCasterLODIsCameraMatchedBelowTheForceThresholdAndLowAtOrAbove)
{
	constexpr uint32_t uHIGH = Flux_TerrainConfig::LOD_HIGH;
	constexpr uint32_t uLOW  = Flux_TerrainConfig::LOD_LOW;

	// Shipping default: force from cascade 3 — only the far cascade casts LOW.
	for (uint32_t c = 0; c < 3u; ++c)
	{
		ZENITH_ASSERT_EQ(Flux_TerrainShadowCasterLOD(c, uHIGH, 3u, uLOW), uHIGH, "cascade %u casts the camera's HIGH", c);
		ZENITH_ASSERT_EQ(Flux_TerrainShadowCasterLOD(c, uLOW,  3u, uLOW), uLOW,  "cascade %u casts the camera's LOW (far chunk)", c);
	}
	ZENITH_ASSERT_EQ(Flux_TerrainShadowCasterLOD(3u, uHIGH, 3u, uLOW), uLOW, "cascade 3 casts LOW even for a HIGH chunk");

	// NEVER: every cascade camera-matched.
	for (uint32_t c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		ZENITH_ASSERT_EQ(Flux_TerrainShadowCasterLOD(c, uHIGH, uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER, uLOW), uHIGH,
			"with the threshold at NEVER, cascade %u stays camera-matched", c);
	}
	// 0: every cascade LOW.
	for (uint32_t c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		ZENITH_ASSERT_EQ(Flux_TerrainShadowCasterLOD(c, uHIGH, 0u, uLOW), uLOW,
			"with the threshold at 0, cascade %u casts LOW", c);
	}
	// The policy never invents a third level.
	ZENITH_ASSERT_TRUE(Flux_TerrainShadowCasterLOD(1u, uHIGH, 1u, uLOW) < Flux_TerrainConfig::LOD_COUNT, "result is a valid LOD index");
}

namespace
{
	// The shader's conservative AABB-vs-planes test (Akenine-Moller), run on the
	// GPU-format planes exactly as Flux_TerrainCulling.slang runs it.
	bool TerrainShadowTest_AABBPassesPlanes(const Zenith_FrustumPlaneGPU* paxPlanes, const Zenith_AABB& xAABB)
	{
		const Zenith_Maths::Vector3 xCenter  = xAABB.GetCenter();
		const Zenith_Maths::Vector3 xExtents = xAABB.GetExtents();
		for (int i = 0; i < 6; ++i)
		{
			const Zenith_Maths::Vector4& xP = paxPlanes[i].m_xNormalAndDistance;
			const Zenith_Maths::Vector3 xN(xP.x, xP.y, xP.z);
			const float fRadius   = glm::dot(xExtents, glm::abs(xN));
			const float fDistance = glm::dot(xN, xCenter) + xP.w;
			if (fDistance < -fRadius) return false;
		}
		return true;
	}

	// A cascade-shaped ortho light box: eye pushed back along -dir from the centre,
	// [-R, R]^2 laterally, depth [0, fDepth]. Same helpers UpdateShadowMatrices uses.
	Zenith_Maths::Matrix4 TerrainShadowTest_OrthoViewProj(const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xSunDir, float fRadius, float fBackDist, float fDepth)
	{
		const Zenith_Maths::Vector3 xEye  = xCenter - xSunDir * fBackDist;
		const Zenith_Maths::Matrix4 xView = glm::lookAt(xEye, xCenter, Zenith_Maths::Vector3(0.f, 1.f, 0.f));
		const Zenith_Maths::Matrix4 xProj = Zenith_Maths::OrthographicProjection(-fRadius, fRadius, -fRadius, fRadius, 0.f, fDepth);
		return xProj * xView;
	}
}

ZENITH_TEST(FluxTerrain, ShadowCullDataCarriesEachCascadesOwnFrustumAndClassifiesChunks)
{
	// Four distinct cascades: growing radii around centres marching away from the
	// origin, all lit by the same low-ish sun.
	const Zenith_Maths::Vector3 xSunDir = glm::normalize(Zenith_Maths::Vector3(0.6f, -0.5f, 0.4f));
	Zenith_Maths::Matrix4 axViewProj[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	Zenith_Maths::Vector3 axCenter[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	float afRadius[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	for (uint32_t c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		afRadius[c] = 20.f * static_cast<float>(1u << c);              // 20, 40, 80, 160
		axCenter[c] = Zenith_Maths::Vector3(1000.f + 300.f * c, 50.f, 1000.f);
		axViewProj[c] = TerrainShadowTest_OrthoViewProj(axCenter[c], xSunDir, afRadius[c], 2.f * afRadius[c], 3.f * afRadius[c]);
	}

	Zenith_TerrainShadowCullGPU xData;
	Flux_BuildTerrainShadowCullData(axViewProj, uFLUX_TERRAIN_SHADOW_CULL_VIEWS, 3u, xData);

	ZENITH_ASSERT_EQ(xData.m_xParams.x, uFLUX_TERRAIN_SHADOW_CULL_VIEWS, "params.x = active cascade count");
	ZENITH_ASSERT_EQ(xData.m_xParams.y, 3u, "params.y = force-LOW-from cascade");
	ZENITH_ASSERT_EQ(xData.m_xParams.z, 0u, "params.z reserved");
	ZENITH_ASSERT_EQ(xData.m_xParams.w, 0u, "params.w reserved");

	for (uint32_t c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		// (1) Byte-for-byte the shared extraction, in the shared plane order, at the
		//     cascade-major offset the shader indexes (c*6 + plane).
		Zenith_Frustum xRef;
		xRef.ExtractFromViewProjection(axViewProj[c]);
		for (uint32_t p = 0; p < 6u; ++p)
		{
			const Zenith_Maths::Vector4& xGPU = xData.m_axFrustumPlanes[c * 6u + p].m_xNormalAndDistance;
			ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(xGPU.x, xGPU.y, xGPU.z), xRef.m_axPlanes[p].m_xNormal, 1e-6f,
				"cascade %u plane %u normal must be Zenith_Frustum's", c, p);
			ZENITH_ASSERT_EQ_FLOAT(xGPU.w, xRef.m_axPlanes[p].m_fDistance, 1e-4f,
				"cascade %u plane %u distance must be Zenith_Frustum's", c, p);
		}

		// (2) The planes classify chunk AABBs the way the cascade needs: a 64m chunk
		//     at the cascade centre is IN; one 20 radii sideways is OUT; one behind
		//     the light eye (further back than the ortho near plane) is OUT.
		const Zenith_FrustumPlaneGPU* paxPlanes = &xData.m_axFrustumPlanes[c * 6u];
		const Zenith_Maths::Vector3 xHalf(32.f, 8.f, 32.f);
		const Zenith_AABB xInside(axCenter[c] - xHalf, axCenter[c] + xHalf);
		ZENITH_ASSERT_TRUE(TerrainShadowTest_AABBPassesPlanes(paxPlanes, xInside), "cascade %u: a chunk at the centre is inside its box", c);

		// A lateral direction perpendicular to the sun.
		const Zenith_Maths::Vector3 xSide = glm::normalize(glm::cross(xSunDir, Zenith_Maths::Vector3(0.f, 1.f, 0.f)));
		const Zenith_Maths::Vector3 xFarSide = axCenter[c] + xSide * (20.f * afRadius[c]);
		const Zenith_AABB xOutsideSide(xFarSide - xHalf, xFarSide + xHalf);
		ZENITH_ASSERT_FALSE(TerrainShadowTest_AABBPassesPlanes(paxPlanes, xOutsideSide), "cascade %u: a chunk 20 radii sideways is culled", c);

		const Zenith_Maths::Vector3 xBehindEye = axCenter[c] - xSunDir * (10.f * afRadius[c]);
		const Zenith_AABB xOutsideBack(xBehindEye - xHalf, xBehindEye + xHalf);
		ZENITH_ASSERT_FALSE(TerrainShadowTest_AABBPassesPlanes(paxPlanes, xOutsideBack), "cascade %u: a chunk behind the light eye is culled", c);

		// (3) Cascades are DISTINCT: cascade c's centre chunk is not inside cascade 0's
		//     box for c >= 2 (300m+ away, radius 20m) — a cascade-major indexing slip
		//     that reused cascade 0's planes would show here.
		if (c >= 2u)
		{
			ZENITH_ASSERT_FALSE(TerrainShadowTest_AABBPassesPlanes(&xData.m_axFrustumPlanes[0], xInside),
				"cascade %u's centre chunk must NOT be inside cascade 0's box", c);
		}
	}
}

ZENITH_TEST(FluxTerrain, ShadowCullDataZeroesInactiveCascadesAndClampsTheCount)
{
	Zenith_Maths::Matrix4 axViewProj[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	for (uint32_t c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		axViewProj[c] = TerrainShadowTest_OrthoViewProj(Zenith_Maths::Vector3(0.f), glm::normalize(Zenith_Maths::Vector3(0.f, -1.f, 0.1f)), 10.f, 20.f, 30.f);
	}

	// Two active: cascades 2-3 are zero planes, count says 2.
	{
		Zenith_TerrainShadowCullGPU xData;
		// Poison first so "zero" is an assertion about the builder, not about stack luck.
		for (uint32_t u = 0; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u; ++u) xData.m_axFrustumPlanes[u].m_xNormalAndDistance = Zenith_Maths::Vector4(7.f);
		Flux_BuildTerrainShadowCullData(axViewProj, 2u, uFLUX_TERRAIN_SHADOW_FORCE_LOW_NEVER, xData);
		ZENITH_ASSERT_EQ(xData.m_xParams.x, 2u, "two active cascades");
		for (uint32_t u = 2u * 6u; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u; ++u)
		{
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xData.m_axFrustumPlanes[u].m_xNormalAndDistance), 0.f, 1e-6f,
				"inactive cascade plane %u must be zeroed", u);
		}
		ZENITH_ASSERT_TRUE(glm::length(xData.m_axFrustumPlanes[0].m_xNormalAndDistance) > 0.f, "active cascade 0 has real planes");
	}

	// Zero active (shadows off / pre-first-frame seed): null matrices are legal and
	// nothing is read; every plane zero, count zero.
	{
		Zenith_TerrainShadowCullGPU xData;
		Flux_BuildTerrainShadowCullData(nullptr, 0u, 3u, xData);
		ZENITH_ASSERT_EQ(xData.m_xParams.x, 0u, "zero active cascades");
		ZENITH_ASSERT_EQ(xData.m_xParams.y, 3u, "force threshold still carried");
		for (uint32_t u = 0; u < uFLUX_TERRAIN_SHADOW_CULL_VIEWS * 6u; ++u)
		{
			ZENITH_ASSERT_EQ_FLOAT(glm::length(xData.m_axFrustumPlanes[u].m_xNormalAndDistance), 0.f, 1e-6f, "plane %u zero", u);
		}
	}

	// Over-count clamps to the slot count (the shader indexes rows by it).
	{
		Zenith_TerrainShadowCullGPU xData;
		Flux_BuildTerrainShadowCullData(axViewProj, 99u, 3u, xData);
		ZENITH_ASSERT_EQ(xData.m_xParams.x, uFLUX_TERRAIN_SHADOW_CULL_VIEWS, "active count clamps to the slot count");
	}

	// The GPU block is the reflected CB, byte for byte.
	ZENITH_ASSERT_EQ(u_int(sizeof(Zenith_TerrainShadowCullGPU)), 400u, "24 float4 planes + one uint4 = 400 bytes");
	{
		Zenith_TerrainShadowCullGPU xLayout;
		const u_int uParamsOffset = static_cast<u_int>(
			reinterpret_cast<const char*>(&xLayout.m_xParams) - reinterpret_cast<const char*>(&xLayout));
		ZENITH_ASSERT_EQ(uParamsOffset, 384u, "params follow the plane array with no padding");
	}
}

ZENITH_TEST(FluxTerrain, TerrainIsDeclaredBeforeShadows)
{
	// The live feature table this build ships. The setup walk IS the render-graph
	// declaration order, and every "Shadow Cascade N" pass READs the terrain shadow
	// cull slot buffers whose only writers are the terrain reset/cull passes — so
	// Terrain must be declared first or those reads form no edge (the exact defect
	// UnifiedMesh and Grass already pin for themselves).
	const Flux_FeatureRegistry& xReg = Flux_FeatureRegistry::Get();
	const u_int uTerrain = xReg.FindSetupStepIndex("Terrain");
	const u_int uShadows = xReg.FindSetupStepIndex("Shadows");

	ZENITH_ASSERT_TRUE(uTerrain != UINT32_MAX, "setup step 'Terrain' must exist, or this ordering assertion is vacuous");
	ZENITH_ASSERT_TRUE(uShadows != UINT32_MAX, "setup step 'Shadows' must exist, or this ordering assertion is vacuous");

	ZENITH_ASSERT_TRUE(uTerrain < uShadows,
		"'Terrain' must be declared BEFORE 'Shadows': each cascade READS a terrain's shadow indirect-args / count slot "
		"buffers and a reader only links to an EARLIER-declared writer. The other way round no edge forms and the cascade "
		"is free to draw from a slot the reset has not cleared or the cull has not filled (terrain %u, shadows %u)",
		uTerrain, uShadows);
}

namespace
{
	void TerrainShadowTest_EmptyRecord(Flux_CommandBuffer*, void*) {}
}

ZENITH_TEST(FluxTerrain, ShadowCascadeReadsOfTheTerrainSlotsOrderAfterTheCullAndBarrier)
{
	// The exact declaration shape Flux_TerrainImpl::SetupRenderGraph + Flux_ShadowsImpl::
	// SetupRenderGraph produce for one terrain, in the order RegisterDefaultFeatures
	// walks them (Terrain before Shadows): reset WRITES both slot buffers, cull WRITES
	// the args + READWRITES the counts, each cascade READs both as indirect args.
	Flux_RenderGraph xGraph;

	Flux_IndirectBuffer xShadowArgs;
	xShadowArgs.GetBuffer().m_xVRAMHandle.SetValue(7001u);
	xShadowArgs.GetBuffer().m_ulSize = Flux_TerrainShadowCullIndirectBufferBytes(
		uFLUX_TERRAIN_SHADOW_CULL_VIEWS, Flux_TerrainConfig::TOTAL_CHUNKS, uFLUX_INDIRECT_DRAW_INDEXED_BYTE_STRIDE);
	Flux_IndirectBuffer xShadowCounts;
	xShadowCounts.GetBuffer().m_xVRAMHandle.SetValue(7002u);
	xShadowCounts.GetBuffer().m_ulSize = Flux_TerrainShadowCullCountBufferBytes(uFLUX_TERRAIN_SHADOW_CULL_VIEWS);

	const Flux_PassHandle xReset = xGraph.AddPass("Terrain Reset Count and Indirect Arguments", TerrainShadowTest_EmptyRecord);
	xGraph.WriteBuffer(xReset, xShadowCounts.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xReset, xShadowArgs.GetBuffer(),   RESOURCE_ACCESS_WRITE_UAV);

	const Flux_PassHandle xCull = xGraph.AddPass("Terrain Culling Compute", TerrainShadowTest_EmptyRecord);
	xGraph.WriteBuffer(xCull, xShadowArgs.GetBuffer(),   RESOURCE_ACCESS_WRITE_UAV);
	xGraph.WriteBuffer(xCull, xShadowCounts.GetBuffer(), RESOURCE_ACCESS_READWRITE_UAV);

	static const char* const aszCascade[uFLUX_TERRAIN_SHADOW_CULL_VIEWS] = { "Shadow Cascade 0", "Shadow Cascade 1", "Shadow Cascade 2", "Shadow Cascade 3" };
	Flux_PassHandle axCascade[uFLUX_TERRAIN_SHADOW_CULL_VIEWS];
	for (u_int c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		axCascade[c] = xGraph.AddPass(aszCascade[c], TerrainShadowTest_EmptyRecord);
		xGraph.ReadBuffer(axCascade[c], xShadowArgs.GetBuffer(),   RESOURCE_ACCESS_READ_INDIRECT_ARG);
		xGraph.ReadBuffer(axCascade[c], xShadowCounts.GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);
	}

	xGraph.MarkDirty();
	ZENITH_ASSERT_TRUE(xGraph.Compile(), "a buffer-only reset -> cull -> 4 cascades graph must compile");

	// (1) Every cascade read found an earlier-declared writer — the check that caught
	//     the Shadows-before-UnifiedMesh regression stays at zero.
	ZENITH_ASSERT_EQ(xGraph.GetProducerBeforeConsumerViolationCount(), 0u,
		"with Terrain declared before the cascades, no cascade read may be left without a producer edge; got %u",
		xGraph.GetProducerBeforeConsumerViolationCount());

	// (2) Execution order: reset < cull < every cascade.
	const Zenith_Vector<u_int>& xOrder = xGraph.GetExecutionOrder();
	auto PositionOf = [&xOrder](u_int uPass) -> u_int
	{
		for (u_int i = 0; i < xOrder.GetSize(); ++i) { if (xOrder.Get(i) == uPass) return i; }
		return UINT32_MAX;
	};
	const u_int uResetPos = PositionOf(xReset.m_uIndex);
	const u_int uCullPos  = PositionOf(xCull.m_uIndex);
	ZENITH_ASSERT_TRUE(uResetPos != UINT32_MAX && uCullPos != UINT32_MAX, "reset and cull must be scheduled");
	ZENITH_ASSERT_TRUE(uResetPos < uCullPos, "the reset must run before the cull (reset %u, cull %u)", uResetPos, uCullPos);
	for (u_int c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		const u_int uPos = PositionOf(axCascade[c].m_uIndex);
		ZENITH_ASSERT_TRUE(uPos != UINT32_MAX, "cascade %u must be scheduled", c);
		ZENITH_ASSERT_TRUE(uCullPos < uPos, "cascade %u must run after the cull (cull %u, cascade %u)", c, uCullPos, uPos);
	}

	// (3) The barrier the cascade needs is the one the graph synthesised: the FIRST
	//     cascade to run gets compute-write -> indirect-arg-read on BOTH slot buffers
	//     (later cascades are read-after-read and may collapse to nothing).
	u_int uFirstCascadePos = UINT32_MAX; u_int uFirstCascade = 0;
	for (u_int c = 0; c < uFLUX_TERRAIN_SHADOW_CULL_VIEWS; ++c)
	{
		const u_int uPos = PositionOf(axCascade[c].m_uIndex);
		if (uPos < uFirstCascadePos) { uFirstCascadePos = uPos; uFirstCascade = c; }
	}
	const Flux_RenderGraph_Pass* pxFirst = xGraph.GetPasses().Get(axCascade[uFirstCascade].m_uIndex);
	bool bArgsBarrier = false, bCountBarrier = false;
	for (u_int u = 0; u < pxFirst->m_xPrologueBarriers.GetSize(); ++u)
	{
		const Flux_RenderGraph_Barrier& xB = pxFirst->m_xPrologueBarriers.Get(u);
		if (xB.m_xResource.GetKind() != Flux_GraphResourceKind::Buffer) continue;
		if (xB.m_eDstAccess != RESOURCE_ACCESS_READ_INDIRECT_ARG) continue;
		if (xB.m_xResource.AsBuffer() == &xShadowArgs.GetBuffer())
		{
			bArgsBarrier = true;
			ZENITH_ASSERT_EQ(xB.m_eSrcAccess, RESOURCE_ACCESS_WRITE_UAV, "slot args: source access must be the cull's UAV write");
		}
		if (xB.m_xResource.AsBuffer() == &xShadowCounts.GetBuffer())
		{
			bCountBarrier = true;
			ZENITH_ASSERT_EQ(xB.m_eSrcAccess, RESOURCE_ACCESS_READWRITE_UAV, "slot counts: source access must be the cull's UAV read-modify-write");
		}
	}
	ZENITH_ASSERT_TRUE(bArgsBarrier,  "the first cascade must carry a compute-write -> indirect-arg barrier on the slot args");
	ZENITH_ASSERT_TRUE(bCountBarrier, "the first cascade must carry a compute-write -> indirect-arg barrier on the slot counts");

	// (4) And the negative: the SAME reads declared BEFORE their writers (Shadows
	//     registered first) is exactly what the ordering check reports — one
	//     "Check failed" line is the check working, not a test failure.
	{
		Flux_RenderGraph xBad;
		Flux_IndirectBuffer xArgs; xArgs.GetBuffer().m_xVRAMHandle.SetValue(7003u); xArgs.GetBuffer().m_ulSize = 256;
		const Flux_PassHandle xCascadeFirst = xBad.AddPass("Shadow Cascade 0", TerrainShadowTest_EmptyRecord);
		xBad.ReadBuffer(xCascadeFirst, xArgs.GetBuffer(), RESOURCE_ACCESS_READ_INDIRECT_ARG);
		const Flux_PassHandle xCullLater = xBad.AddPass("Terrain Culling Compute", TerrainShadowTest_EmptyRecord);
		xBad.WriteBuffer(xCullLater, xArgs.GetBuffer(), RESOURCE_ACCESS_WRITE_UAV);
		xBad.MarkDirty();
		xBad.Compile();
		ZENITH_ASSERT_EQ(xBad.GetProducerBeforeConsumerViolationCount(), 1u,
			"a cascade declared before the terrain cull MUST be reported — this is the shape the registration order prevents; got %u",
			xBad.GetProducerBeforeConsumerViolationCount());
	}
}

#endif // ZENITH_TESTING
