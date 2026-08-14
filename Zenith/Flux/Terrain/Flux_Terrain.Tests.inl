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
#include "Flux/Terrain/Flux_TerrainSourceGrid.h"
#include "Flux/Terrain/Flux_TerrainVertexQuant.h"
// Phase 2 of the terrain indirect-count compatibility plan: the terrain
// indirect-command allocation/seed and the per-frame zero-tail invariant
// ride the shared 20-byte / five-word ABI defined in
// Flux/Backend/Flux_IndirectDraw.h. These tests pin the terrain-specific
// arithmetic that lives on top of that ABI.
#include "Flux/Backend/Flux_IndirectDraw.h"

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

ZENITH_TEST(FluxTerrain, TerrainConstantsFillMatchesAuthoredBox)
{
	// The CB fill in Initialise is the ONE bridge from the authored box to the three
	// shaders' Flux_DequantPosition, and nothing else validates it — the
	// static_asserts beside the struct pin layout, not values. A dropped fill leaves
	// scale at its 1.0f default (terrain collapses to a metre cube at the origin);
	// a dropped .w makes every terrain UV 4096x too small. Unit tests run after
	// engine init, so the file-static has been filled by the time this executes.
	const Flux_PosQuant xExpected = Flux_MakeTerrainPosQuant();
	for (int i = 0; i < 3; i++)
	{
		ZENITH_ASSERT_EQ_FLOAT(s_xTerrainConstants.m_afPosQuantScale[i], xExpected.m_xScale[i], 1.0e-4f,
			"CB dequant scale axis %d must be the authored box extent", i);
		ZENITH_ASSERT_EQ_FLOAT(s_xTerrainConstants.m_afPosQuantBias[i], xExpected.m_xBias[i], 1.0e-4f,
			"CB dequant bias axis %d must be the authored box min", i);
	}
	ZENITH_ASSERT_EQ_FLOAT(s_xTerrainConstants.m_afPosQuantScale[3],
		Zenith_TerrainChunkLayout::fUV_BOX_MAX, 1.0e-4f,
		"CB scale.w must carry the UV dequant extent the shaders multiply the unorm16 UV back up by");
}

ZENITH_TEST(FluxTerrain, TerrainVertexQuantWritesLandAtShaderOffsets)
{
	// The bridge writes through the NAMED offset constants and the shader fetches at
	// its REFLECTED offsets; the static_asserts in Flux_TerrainStreamingManager.cpp
	// pin those equal at compile time, and this pins the bytes at runtime: decode at
	// the GENERATED offsets and the authored values must come back. Against a 0xCD
	// sentinel fill, a transposed offset decodes wild values, not near-misses.
	const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
	const Zenith_Maths::Vector3 xPos(123.0f, 45.0f, 678.0f);
	const Zenith_Maths::Vector2 xUV(321.0f, 87.0f);
	const u_int uNormalWord = 0xA1B2C3D4u;
	const u_int uTangentWord = 0x1F2E3D4Cu;

	u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE];
	std::memset(aucVertex, 0xCD, sizeof(aucVertex));
	Flux_WriteTerrainVertexPosition(aucVertex, xPos, xQuant);
	Flux_WriteTerrainVertexUV(aucVertex, xUV);
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
	const Zenith_Maths::Vector2 xDecodedUV =
		Flux_UnpackUnorm16x2(uUVWord) * Zenith_TerrainChunkLayout::fUV_BOX_MAX;
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
	const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant();
	u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE] = {};
	for (u_int u = 0; u < 97u; u++)
	{
		// Off-lattice authored positions across the whole box — the first pack
		// rounds arbitrarily; the identity under test is the SECOND pack.
		const float fT = static_cast<float>(u) / 96.0f;
		const Zenith_Maths::Vector3 xAuthored(
			4096.0f * fT, 512.0f * (1.0f - fT), 4096.0f * fT * fT);
		Flux_WriteTerrainVertexPosition(aucVertex, xAuthored, xQuant);
		u_int64 ulFirst = 0u;
		std::memcpy(&ulFirst, aucVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, sizeof(ulFirst));

		const Zenith_Maths::Vector3 xDecoded = Flux_ReadTerrainVertexPosition(aucVertex, xQuant);
		Flux_WriteTerrainVertexPosition(aucVertex, xDecoded, xQuant);
		u_int64 ulSecond = 0u;
		std::memcpy(&ulSecond, aucVertex + Zenith_TerrainChunkLayout::uPOSITION_OFFSET, sizeof(ulSecond));

		ZENITH_ASSERT_EQ(ulFirst, ulSecond,
			"decode->re-encode must be the identity on the packed word (case %u) — "
			"a sculpted chunk's untouched XZ words have to survive the round trip", u);
	}
}

ZENITH_TEST(FluxTerrain, TerrainVertexQuantUVRoundTrip)
{
	// The UV pair is the one asymmetric couple in the bridge (write divides by the
	// extent, read multiplies it back) and nothing else exercises the read side.
	// Integer pixel coordinates — all the exporter ever authors — must come back
	// within one unorm16 quantum AND snap back to the exact integer, which is the
	// property the sculpt hook's std::round of the decoded UV stands on.
	u_int8 aucVertex[Zenith_TerrainChunkLayout::uVERTEX_STRIDE] = {};
	for (u_int u = 0; u <= 16u; u++)
	{
		const float fPixel = Zenith_TerrainChunkLayout::fUV_BOX_MAX * static_cast<float>(u) / 16.0f;
		const Zenith_Maths::Vector2 xAuthored(fPixel, Zenith_TerrainChunkLayout::fUV_BOX_MAX - fPixel);
		Flux_WriteTerrainVertexUV(aucVertex, xAuthored);
		const Zenith_Maths::Vector2 xDecoded = Flux_ReadTerrainVertexUV(aucVertex);
		for (int i = 0; i < 2; i++)
		{
			ZENITH_ASSERT_TRUE(
				std::fabs(xDecoded[i] - xAuthored[i]) <= Zenith_TerrainChunkLayout::fUV_QUANT_STEP,
				"decoded UV axis %d must sit within one unorm16 quantum of the authored pixel (case %u)", i, u);
			ZENITH_ASSERT_EQ_FLOAT(std::round(xDecoded[i]), xAuthored[i], 1.0e-4f,
				"an integer authored UV must snap back exactly (case %u axis %d)", u, i);
		}
	}
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

#endif // ZENITH_TESTING
