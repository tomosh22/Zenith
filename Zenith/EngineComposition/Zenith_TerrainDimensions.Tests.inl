//------------------------------------------------------------------------------
// Zenith_TerrainDimensions unit tests.
// Included at the bottom of Zenith_Core.cpp (the module-owns-its-tests pattern);
// the header itself is header-only, so it has no TU of its own to host them.
//
// Focus: the four configurable knobs and everything derived from them. Two
// claims carry the most weight here and neither is visible at a call site:
//
//   1. DEFAULT EQUIVALENCE. Every derived quantity at Default() must reproduce
//      EXACTLY the constant it replaced (4096m world, 1m spacing, 4225/24576
//      HIGH, 289/1536 LOW+physics, exactly 1.0f world-per-heightfield-pixel).
//      A default-dimensioned re-bake has to be byte-identical to the assets on
//      disk, and that identity is float-exactness, not near-equality.
//   2. CAPACITY vs EXTENT. FlatChunkIndex keeps its stride-64 spelling whatever
//      the active grid is, so shrinking a grid never renumbers a chunk slot.
//
// Non-square and non-default cases are exercised deliberately even though no
// shipped terrain uses them yet: the knobs are proven here or nowhere.
//
// HEADLESS-SAFE -- pure arithmetic over stack values and one 28-byte buffer.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_TerrainDimensions.h"
#include "Core/Zenith_TerrainChunkLayout.h"

#include <limits>

#ifdef ZENITH_TESTING

namespace Zenith_TerrainDimensionsTestHelpers
{
	inline Zenith_TerrainDimensions Make(float fChunkSize, uint32_t uQuads, uint32_t uGridX, uint32_t uGridZ)
	{
		Zenith_TerrainDimensions xDims;
		xDims.m_fChunkWorldSize = fChunkSize;
		xDims.m_uQuadsPerChunkEdge = uQuads;
		xDims.m_uGridChunksX = uGridX;
		xDims.m_uGridChunksZ = uGridZ;
		return xDims;
	}
}

ZENITH_TEST(ZenithTerrainDimensions, DefaultReproducesTheHistoricalConstantsExactly)
{
	const Zenith_TerrainDimensions xDims = Zenith_TerrainDimensions::Default();

	// Float EQUALITY, not near-equality: a default re-bake must be byte-identical,
	// and a scale factor that is 0.9999999 instead of 1.0 moves every vertex.
	ZENITH_ASSERT_TRUE(xDims.WorldSizeX() == 4096.0f, "default world X must be exactly 4096m");
	ZENITH_ASSERT_TRUE(xDims.WorldSizeZ() == 4096.0f, "default world Z must be exactly 4096m");
	ZENITH_ASSERT_TRUE(xDims.MaxWorldSize() == 4096.0f, "the default authoring domain must be exactly 4096m");
	ZENITH_ASSERT_TRUE(xDims.VertexSpacing() == 1.0f, "default vertex spacing must be exactly 1m");
	ZENITH_ASSERT_TRUE(xDims.WorldPerImagePixel(4096u) == 1.0f,
		"a 4096px heightfield over the default domain must be exactly 1m per pixel");
	ZENITH_ASSERT_TRUE(xDims.ImagePixelPerWorld(4096u) == 1.0f,
		"the inverse scale must be exactly 1px per metre at defaults");

	ZENITH_ASSERT_EQ(xDims.ChunkCount(), 4096u, "the default grid must carry 4096 chunks");
	ZENITH_ASSERT_EQ(xDims.ChunkVertexCount(1u), Zenith_TerrainChunkLayout::uHIGH_CHUNK_VERTEX_COUNT,
		"the spec's HIGH vertex count must agree with the on-disk chunk layout");
	ZENITH_ASSERT_EQ(xDims.ChunkIndexCount(1u), Zenith_TerrainChunkLayout::uHIGH_CHUNK_INDEX_COUNT,
		"the spec's HIGH index count must agree with the on-disk chunk layout");
	ZENITH_ASSERT_EQ(xDims.ChunkVertexCount(4u), Zenith_TerrainChunkLayout::uLOW_CHUNK_VERTEX_COUNT,
		"the spec's LOW vertex count must agree with the on-disk chunk layout");
	ZENITH_ASSERT_EQ(xDims.ChunkIndexCount(4u), Zenith_TerrainChunkLayout::uLOW_CHUNK_INDEX_COUNT,
		"the spec's LOW index count must agree with the on-disk chunk layout");
	ZENITH_ASSERT_EQ(xDims.m_uQuadsPerChunkEdge, Zenith_TerrainChunkLayout::uCHUNK_QUADS_PER_EDGE,
		"the default quad count must be the layout's canonical quads per chunk edge");
}

ZENITH_TEST(ZenithTerrainDimensions, DerivedQuantitiesTrackTheKnobs)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	// Half-size chunks at the same quad count: half the spacing, quarter the area.
	const Zenith_TerrainDimensions xDense = Make(32.0f, 64u, 16u, 16u);
	ZENITH_ASSERT_TRUE(xDense.VertexSpacing() == 0.5f, "32m over 64 quads must be 0.5m spacing");
	ZENITH_ASSERT_TRUE(xDense.WorldSizeX() == 512.0f, "16 chunks of 32m must span 512m");
	ZENITH_ASSERT_EQ(xDense.ChunkCount(), 256u, "a 16x16 grid carries 256 chunks");

	// Same chunk size, fewer quads: coarser spacing, unchanged extent.
	const Zenith_TerrainDimensions xCoarse = Make(64.0f, 16u, 16u, 16u);
	ZENITH_ASSERT_TRUE(xCoarse.VertexSpacing() == 4.0f, "64m over 16 quads must be 4m spacing");
	ZENITH_ASSERT_TRUE(xCoarse.WorldSizeX() == 1024.0f, "16 chunks of 64m must span 1024m");
	ZENITH_ASSERT_EQ(xCoarse.ChunkVertexCount(1u), 289u, "16 quads per edge gives a 17x17 HIGH chunk");
	ZENITH_ASSERT_EQ(xCoarse.ChunkIndexCount(1u), 1536u, "16x16 quads at six indices per quad");
	ZENITH_ASSERT_EQ(xCoarse.ChunkVertexCount(4u), 25u, "divisor 4 leaves 4 quads, i.e. a 5x5 chunk");
	ZENITH_ASSERT_EQ(xCoarse.ChunkIndexCount(4u), 96u, "4x4 quads at six indices per quad");
}

ZENITH_TEST(ZenithTerrainDimensions, NonSquareGridsKeepTheirAxesApart)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	// Route1's shape: a long narrow ribbon. The X/Z asymmetry is the case that
	// catches an axis swapped somewhere downstream (exporter splits, per-axis
	// quant box, editor scales), so it is pinned before any game uses it.
	const Zenith_TerrainDimensions xRibbon = Make(64.0f, 64u, 4u, 24u);
	ZENITH_ASSERT_TRUE(xRibbon.WorldSizeX() == 256.0f, "4 chunks of 64m span 256m in X");
	ZENITH_ASSERT_TRUE(xRibbon.WorldSizeZ() == 1536.0f, "24 chunks of 64m span 1536m in Z");
	ZENITH_ASSERT_TRUE(xRibbon.MaxWorldSize() == 1536.0f,
		"the square authoring domain must be the LONGER axis, so no content falls outside the images");
	ZENITH_ASSERT_EQ(xRibbon.ChunkCount(), 96u, "a 4x24 grid carries 96 chunks");

	// The authoring images stay square over the longer axis, so one scalar
	// converts both axes and the terrain sits in the corner of the domain.
	ZENITH_ASSERT_TRUE(xRibbon.WorldPerImagePixel(4096u) == 1536.0f / 4096.0f,
		"one world-per-pixel scale must serve both axes");

	// The mirrored spec must mirror its extents and share the domain.
	const Zenith_TerrainDimensions xMirror = Make(64.0f, 64u, 24u, 4u);
	ZENITH_ASSERT_TRUE(xMirror.WorldSizeX() == 1536.0f, "the mirrored ribbon is long in X");
	ZENITH_ASSERT_TRUE(xMirror.WorldSizeZ() == 256.0f, "the mirrored ribbon is short in Z");
	ZENITH_ASSERT_TRUE(xMirror.MaxWorldSize() == xRibbon.MaxWorldSize(),
		"both orientations share the same square authoring domain");
}

ZENITH_TEST(ZenithTerrainDimensions, FlatChunkIndexIsCapacityStridedNotGridStrided)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	// THE capacity invariant. A chunk's GPU slot is a property of its
	// coordinates, never of how big the grid around it happens to be -- the
	// culling shader and the indirect argument buffer address the full 64x64
	// table whatever the active grid is.
	const Zenith_TerrainDimensions xSmall = Make(64.0f, 64u, 6u, 9u);
	const Zenith_TerrainDimensions xFull = Zenith_TerrainDimensions::Default();

	ZENITH_ASSERT_EQ(xSmall.FlatChunkIndex(3u, 4u), xFull.FlatChunkIndex(3u, 4u),
		"shrinking the grid must not renumber a chunk slot");
	ZENITH_ASSERT_EQ(xSmall.FlatChunkIndex(3u, 4u), 3u * 64u + 4u,
		"the flat index must keep its stride-64 spelling");
	ZENITH_ASSERT_LT(xSmall.FlatChunkIndex(5u, 8u), Zenith_TerrainDimensionsLimits::uTOTAL_CHUNK_CAPACITY,
		"every in-grid chunk must address a slot inside the fixed capacity");

	// Containment is the ACTIVE grid, and it is the only thing that shrinks.
	ZENITH_ASSERT_TRUE(xSmall.ContainsChunk(5u, 8u), "(5,8) is the far corner of a 6x9 grid");
	ZENITH_ASSERT_FALSE(xSmall.ContainsChunk(6u, 8u), "(6,8) is past the 6-chunk X extent");
	ZENITH_ASSERT_FALSE(xSmall.ContainsChunk(5u, 9u), "(5,9) is past the 9-chunk Z extent");
}

ZENITH_TEST(ZenithTerrainDimensions, ChunkCentresLandOnTheGrid)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	const Zenith_TerrainDimensions xDims = Make(64.0f, 64u, 6u, 9u);
	ZENITH_ASSERT_TRUE(xDims.ChunkCentreX(0u) == 32.0f, "chunk 0 is centred half a chunk in");
	ZENITH_ASSERT_TRUE(xDims.ChunkCentreZ(8u) == 544.0f, "chunk 8 of a 64m grid is centred at 544m");
	ZENITH_ASSERT_TRUE(xDims.ChunkCentreZ(xDims.m_uGridChunksZ - 1u) < xDims.WorldSizeZ(),
		"the last chunk's centre must fall inside the terrain");
}

ZENITH_TEST(ZenithTerrainDimensions, ValidationRejectsEveryShapeTheBakerCannotHonour)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	ZENITH_ASSERT_TRUE(Zenith_TerrainDimensions::Default().IsValid(), "the default spec must validate");
	ZENITH_ASSERT_TRUE(Make(64.0f, 64u, 1u, 1u).IsValid(), "a single-chunk terrain is legal");
	ZENITH_ASSERT_TRUE(Make(64.0f, 64u, 64u, 64u).IsValid(), "a full-capacity grid is legal");
	ZENITH_ASSERT_TRUE(Make(8.0f, 4u, 4u, 24u).IsValid(), "the minimum quad count is legal");
	ZENITH_ASSERT_TRUE(Make(256.0f, 256u, 2u, 2u).IsValid(), "the maximum quad count is legal");

	// Quads per edge: power of two, so divisor 4 divides it exactly.
	ZENITH_ASSERT_FALSE(Make(64.0f, 48u, 8u, 8u).IsValid(), "a non-power-of-two quad count must be refused");
	ZENITH_ASSERT_FALSE(Make(64.0f, 2u, 8u, 8u).IsValid(), "fewer than 4 quads cannot survive divisor 4");
	ZENITH_ASSERT_FALSE(Make(64.0f, 512u, 8u, 8u).IsValid(), "more than 256 quads per edge must be refused");
	ZENITH_ASSERT_FALSE(Make(64.0f, 0u, 8u, 8u).IsValid(), "zero quads must be refused");

	// Grid: at least one chunk per axis, never past the fixed capacity.
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 0u, 8u).IsValid(), "an empty X grid must be refused");
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 8u, 0u).IsValid(), "an empty Z grid must be refused");
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 65u, 8u).IsValid(), "an X grid past capacity must be refused");
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 8u, 65u).IsValid(), "a Z grid past capacity must be refused");

	// Chunk size: positive and finite.
	ZENITH_ASSERT_FALSE(Make(0.0f, 64u, 8u, 8u).IsValid(), "a zero-size chunk must be refused");
	ZENITH_ASSERT_FALSE(Make(-64.0f, 64u, 8u, 8u).IsValid(), "a negative chunk size must be refused");
	ZENITH_ASSERT_FALSE(Make(std::numeric_limits<float>::infinity(), 64u, 8u, 8u).IsValid(),
		"an infinite chunk size must be refused");
	ZENITH_ASSERT_FALSE(Make(std::numeric_limits<float>::quiet_NaN(), 64u, 8u, 8u).IsValid(),
		"a NaN chunk size must be refused");

	// A valid grid can never overflow the fixed chunk capacity.
	ZENITH_ASSERT_LE(Make(64.0f, 64u, 64u, 64u).ChunkCount(),
		Zenith_TerrainDimensionsLimits::uTOTAL_CHUNK_CAPACITY,
		"the largest legal grid must still fit the fixed capacity");
}

ZENITH_TEST(ZenithTerrainDimensions, StreamingAdvisoryTracksBothKnobs)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	const uint32_t uStride = Zenith_TerrainChunkLayout::uVERTEX_STRIDE;
	const uint64_t ulBudget = 256ull * 1024ull * 1024ull;

	// The shipped shape fits with room to spare.
	ZENITH_ASSERT_TRUE(Zenith_TerrainDimensions::Default().FitsStreamingBudget(uStride, 1000.0f, ulBudget),
		"the default terrain must fit the 256MB streaming vertex budget");

	// A grid smaller than the streaming radius is capped by the grid, not the radius.
	const Zenith_TerrainDimensions xSmall = Make(64.0f, 64u, 6u, 9u);
	ZENITH_ASSERT_EQ(xSmall.EstimatedHighLodStreamingBytes(uStride, 1000.0f),
		static_cast<uint64_t>(6u * 9u) * 4225ull * static_cast<uint64_t>(uStride),
		"a grid smaller than the streaming radius must cost only its own chunks");

	// Quadrupling the density quadruples the bytes for the same footprint.
	const Zenith_TerrainDimensions xDense = Make(64.0f, 128u, 6u, 9u);
	ZENITH_ASSERT_GT(xDense.EstimatedHighLodStreamingBytes(uStride, 1000.0f),
		xSmall.EstimatedHighLodStreamingBytes(uStride, 1000.0f),
		"doubling quads per edge must raise the streaming estimate");

	// The advisory has to be able to say no, or the editor UI is decorative.
	const Zenith_TerrainDimensions xExtreme = Make(64.0f, 256u, 64u, 64u);
	ZENITH_ASSERT_FALSE(xExtreme.FitsStreamingBudget(uStride, 1000.0f, ulBudget),
		"a full grid at maximum density must exceed the streaming vertex budget");
}

ZENITH_TEST(ZenithTerrainDimensions, EqualityComparesEveryKnob)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	ZENITH_ASSERT_TRUE(Make(64.0f, 64u, 64u, 64u) == Zenith_TerrainDimensions::Default(),
		"an explicitly built default must compare equal to Default()");
	ZENITH_ASSERT_FALSE(Make(32.0f, 64u, 64u, 64u) == Zenith_TerrainDimensions::Default(),
		"a different chunk size must not compare equal");
	ZENITH_ASSERT_FALSE(Make(64.0f, 32u, 64u, 64u) == Zenith_TerrainDimensions::Default(),
		"a different quad count must not compare equal");
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 32u, 64u) == Zenith_TerrainDimensions::Default(),
		"a different X grid must not compare equal");
	ZENITH_ASSERT_FALSE(Make(64.0f, 64u, 64u, 32u) == Zenith_TerrainDimensions::Default(),
		"a different Z grid must not compare equal");
}

ZENITH_TEST(ZenithTerrainDimsManifest, RoundtripsThroughItsOnDiskImage)
{
	using namespace Zenith_TerrainDimensionsTestHelpers;

	const Zenith_TerrainDimensions xDims = Make(32.0f, 128u, 4u, 24u);
	const Zenith_TerrainDimsManifest xWritten = Zenith_TerrainDimsManifest::FromDimensions(xDims, 2048u);

	uint8_t auBytes[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
	Zenith_TerrainDimsManifestFormat::Write(xWritten, auBytes);

	Zenith_TerrainDimsManifest xRead;
	ZENITH_ASSERT_TRUE(Zenith_TerrainDimsManifestFormat::Read(auBytes, sizeof(auBytes), xRead),
		"a manifest this process just wrote must read back");
	ZENITH_ASSERT_TRUE(xRead == xWritten, "the roundtrip must preserve every field");
	ZENITH_ASSERT_TRUE(xRead.ToDimensions() == xDims, "the roundtrip must preserve the dimensions");
	ZENITH_ASSERT_TRUE(xRead.DescribesDimensions(xDims), "the manifest must recognise its own dimensions");
	ZENITH_ASSERT_EQ(xRead.m_uHeightmapImageSize, 2048u, "the authoring image size must survive the roundtrip");

	// The whole point of the manifest: a set baked at other dimensions is stale.
	ZENITH_ASSERT_FALSE(xRead.DescribesDimensions(Zenith_TerrainDimensions::Default()),
		"a manifest must reject dimensions it was not baked at");
}

ZENITH_TEST(ZenithTerrainDimsManifest, RejectsEveryCorruptRecordAsAStaleBake)
{
	const Zenith_TerrainDimsManifest xValid =
		Zenith_TerrainDimsManifest::FromDimensions(Zenith_TerrainDimensions::Default(), 4096u);

	uint8_t auBytes[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
	Zenith_TerrainDimsManifestFormat::Write(xValid, auBytes);

	Zenith_TerrainDimsManifest xRead;
	ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(nullptr, sizeof(auBytes), xRead),
		"a null buffer must not read as a manifest");
	ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(auBytes, sizeof(auBytes) - 1u, xRead),
		"a truncated file must not read as a manifest");

	{
		Zenith_TerrainDimsManifest xBadMagic = xValid;
		xBadMagic.m_uMagic = 0xDEADBEEFu;
		uint8_t auCorrupt[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
		Zenith_TerrainDimsManifestFormat::Write(xBadMagic, auCorrupt);
		ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(auCorrupt, sizeof(auCorrupt), xRead),
			"a foreign magic must be refused");
	}

	{
		Zenith_TerrainDimsManifest xBadVersion = xValid;
		xBadVersion.m_uVersion = Zenith_TerrainDimsManifestFormat::uVERSION + 1u;
		uint8_t auCorrupt[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
		Zenith_TerrainDimsManifestFormat::Write(xBadVersion, auCorrupt);
		ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(auCorrupt, sizeof(auCorrupt), xRead),
			"an unknown version must be refused rather than guessed at");
	}

	{
		// A record that survives magic and version but describes an impossible
		// terrain is still a stale bake -- it must never reach the loader as a spec.
		Zenith_TerrainDimsManifest xBadDims = xValid;
		xBadDims.m_uGridChunksX = 200u;
		uint8_t auCorrupt[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
		Zenith_TerrainDimsManifestFormat::Write(xBadDims, auCorrupt);
		ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(auCorrupt, sizeof(auCorrupt), xRead),
			"a manifest describing an invalid grid must be refused");
	}

	{
		Zenith_TerrainDimsManifest xBadImage = xValid;
		xBadImage.m_uHeightmapImageSize = 0u;
		uint8_t auCorrupt[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
		Zenith_TerrainDimsManifestFormat::Write(xBadImage, auCorrupt);
		ZENITH_ASSERT_FALSE(Zenith_TerrainDimsManifestFormat::Read(auCorrupt, sizeof(auCorrupt), xRead),
			"a manifest with no authoring image must be refused");
	}
}

#endif // ZENITH_TESTING
