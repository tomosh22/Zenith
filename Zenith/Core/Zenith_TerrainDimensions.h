#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>

// ============================================================================
// Zenith_TerrainDimensions
//
// THE per-terrain shape: how wide a chunk is in world metres, how many quads
// span that chunk edge (i.e. the vertex spacing), and how many chunks the
// active grid carries in X and Z. Everything dimensional about a terrain --
// its world extent, its quantisation box, the split count the exporter uses,
// the image-to-world scale every authoring map is read through -- derives from
// these four numbers.
//
// WHY IT EXISTS. These used to be compile-time constants welded together
// across roughly eight sites: Flux_TerrainConfig's CHUNK_GRID_SIZE /
// CHUNK_SIZE_WORLD / TERRAIN_SIZE, the exporter's own TERRAIN_SIZE and
// TERRAIN_SCALE defines, the chunk layout's uCHUNK_QUADS_PER_EDGE, the editor
// session's fTERRAIN_WORLD_SIZE, and the quantisation box itself. One terrain
// size fitted every game because there was no way to spell a second one -- so
// Zenithmon shipped a 1024x1024m map using 14% of its area and RenderTest
// baked 12,298 files for a campus occupying 3% of them.
//
// WHY IT LIVES IN Core, beside Zenith_TerrainChunkLayout.h: it is read from
// the same two directions the layout is. Flux consumes it to size iteration
// and fill GPU constants; the asset side (the exporter in ZenithTools, the
// component's chunk loader, the editor session) consumes it as part of the
// FILE CONTRACT -- a baked chunk cannot be decoded without the box these
// dimensions produce.
//
// CAPACITY vs EXTENT. The grid is configurable up to a FIXED capacity of
// 64x64. GPU-side arrays -- the indirect argument buffer, the residency and
// AABB tables, the reset dispatch, the culling shader's slot count -- stay
// sized for the full 4096, and the flat chunk index keeps its stride-64
// spelling so a chunk's slot never moves when a grid shrinks. Slots outside
// the active grid are zero-count no-op records; that is exactly how the
// existing sparse bakes (a 16x16 region of a 64x64 grid) already behave.
//
// Kept dependency-light (<cstdint>/<cmath>/<cstring> only, no Flux and no
// engine headers) so it is unit-testable in every configuration -- the same
// shape as Flux/Terrain/Flux_TerrainSourceGrid.h and Flux_TerrainExportRect.h.
// ============================================================================

namespace Zenith_TerrainDimensionsLimits
{
	// The FIXED capacity every GPU-side terrain array is sized for. An active
	// grid may be smaller in either axis; it may never be larger, because the
	// flat chunk index is a stride-64 spelling shared with the culling shader.
	inline constexpr uint32_t uCHUNK_GRID_CAPACITY = 64u;
	inline constexpr uint32_t uTOTAL_CHUNK_CAPACITY = uCHUNK_GRID_CAPACITY * uCHUNK_GRID_CAPACITY;

	// Quads per chunk edge must stay a power of two so the exporter's density
	// divisors (1 for HIGH, 4 for LOW and physics) divide it exactly, and must
	// stay at least 4 so the divisor-4 bakes keep at least one quad per edge.
	inline constexpr uint32_t uMIN_QUADS_PER_CHUNK_EDGE = 4u;
	inline constexpr uint32_t uMAX_QUADS_PER_CHUNK_EDGE = 256u;

	// The density divisors the bake pipeline actually emits. LOW LOD and the
	// physics mesh share divisor 4.
	inline constexpr uint32_t uHIGH_DENSITY_DIVISOR = 1u;
	inline constexpr uint32_t uLOW_DENSITY_DIVISOR = 4u;
	inline constexpr uint32_t uPHYSICS_DENSITY_DIVISOR = 4u;
}

struct Zenith_TerrainDimensions
{
	// World metres spanned by one chunk edge.
	float m_fChunkWorldSize = 64.0f;
	// HIGH-LOD quads across that chunk edge -- the density knob. Vertex spacing
	// is m_fChunkWorldSize / m_uQuadsPerChunkEdge.
	uint32_t m_uQuadsPerChunkEdge = 64u;
	// Active grid extent, in chunks. Bounded by uCHUNK_GRID_CAPACITY.
	uint32_t m_uGridChunksX = 64u;
	uint32_t m_uGridChunksZ = 64u;

	// The historical shape every terrain baked before the knobs existed:
	// 64 chunks of 64m at 64 quads each == a 4096m grid at 1m vertex spacing.
	// Every arithmetic path below must be EXACTLY value-identical at these
	// defaults, so a default-dimensioned re-bake is byte-identical to the
	// bakes on disk today.
	static constexpr Zenith_TerrainDimensions Default() { return {}; }

	constexpr float WorldSizeX() const { return m_fChunkWorldSize * static_cast<float>(m_uGridChunksX); }
	constexpr float WorldSizeZ() const { return m_fChunkWorldSize * static_cast<float>(m_uGridChunksZ); }

	// The SQUARE AUTHORING DOMAIN. Every per-set image (heightfield, splatmap,
	// grass density/type) keeps its fixed square resolution and spans
	// [0, MaxWorldSize()] in both axes; the terrain occupies the
	// [0, WorldSizeX] x [0, WorldSizeZ] corner of it. One scalar per terrain
	// therefore converts every image coordinate to world metres and back.
	constexpr float MaxWorldSize() const
	{
		const float fX = WorldSizeX();
		const float fZ = WorldSizeZ();
		return fX > fZ ? fX : fZ;
	}

	constexpr float VertexSpacing() const
	{
		return m_uQuadsPerChunkEdge != 0u
			? m_fChunkWorldSize / static_cast<float>(m_uQuadsPerChunkEdge)
			: 0.0f;
	}

	constexpr uint32_t ChunkCount() const { return m_uGridChunksX * m_uGridChunksZ; }

	// Quads across a chunk edge at one bake density. Divisor 1 is HIGH, 4 is
	// LOW and physics.
	constexpr uint32_t QuadsPerChunkEdge(uint32_t uDensityDivisor) const
	{
		return uDensityDivisor != 0u ? m_uQuadsPerChunkEdge / uDensityDivisor : 0u;
	}

	constexpr uint32_t ChunkVertexCount(uint32_t uDensityDivisor) const
	{
		const uint32_t uQuads = QuadsPerChunkEdge(uDensityDivisor);
		return (uQuads + 1u) * (uQuads + 1u);
	}

	constexpr uint32_t ChunkIndexCount(uint32_t uDensityDivisor) const
	{
		const uint32_t uQuads = QuadsPerChunkEdge(uDensityDivisor);
		return uQuads * uQuads * 6u;
	}

	// CAPACITY stride, deliberately NOT m_uGridChunksZ: this must agree with
	// Flux_TerrainConfig::ChunkCoordsToIndex and with the culling shader, both
	// of which address the full 64x64 slot table. Shrinking a grid must not
	// renumber the chunks that remain.
	constexpr uint32_t FlatChunkIndex(uint32_t uChunkX, uint32_t uChunkZ) const
	{
		return uChunkX * Zenith_TerrainDimensionsLimits::uCHUNK_GRID_CAPACITY + uChunkZ;
	}

	constexpr bool ContainsChunk(uint32_t uChunkX, uint32_t uChunkZ) const
	{
		return uChunkX < m_uGridChunksX && uChunkZ < m_uGridChunksZ;
	}

	// World-space centre of one chunk's XZ footprint (Y is the caller's).
	constexpr float ChunkCentreX(uint32_t uChunkX) const
	{
		return (static_cast<float>(uChunkX) + 0.5f) * m_fChunkWorldSize;
	}

	constexpr float ChunkCentreZ(uint32_t uChunkZ) const
	{
		return (static_cast<float>(uChunkZ) + 0.5f) * m_fChunkWorldSize;
	}

	// World metres per pixel of a square authoring image of uImageSize texels.
	// EXACTLY 1.0f for a 4096px heightfield at default dimensions, which is
	// what keeps default-dimensioned authoring floating-point-identical to the
	// 1m/px arithmetic it replaces.
	constexpr float WorldPerImagePixel(uint32_t uImageSize) const
	{
		return uImageSize != 0u ? MaxWorldSize() / static_cast<float>(uImageSize) : 0.0f;
	}

	constexpr float ImagePixelPerWorld(uint32_t uImageSize) const
	{
		const float fMax = MaxWorldSize();
		return fMax != 0.0f ? static_cast<float>(uImageSize) / fMax : 0.0f;
	}

	bool IsValid() const
	{
		if (!(m_fChunkWorldSize > 0.0f) || !std::isfinite(m_fChunkWorldSize))
		{
			return false;
		}
		if (m_uQuadsPerChunkEdge < Zenith_TerrainDimensionsLimits::uMIN_QUADS_PER_CHUNK_EDGE ||
			m_uQuadsPerChunkEdge > Zenith_TerrainDimensionsLimits::uMAX_QUADS_PER_CHUNK_EDGE)
		{
			return false;
		}
		if ((m_uQuadsPerChunkEdge & (m_uQuadsPerChunkEdge - 1u)) != 0u)
		{
			return false;
		}
		if (m_uGridChunksX == 0u || m_uGridChunksZ == 0u ||
			m_uGridChunksX > Zenith_TerrainDimensionsLimits::uCHUNK_GRID_CAPACITY ||
			m_uGridChunksZ > Zenith_TerrainDimensionsLimits::uCHUNK_GRID_CAPACITY)
		{
			return false;
		}
		return true;
	}

	// ADVISORY, for the editor UI only -- nothing refuses a bake over it. The
	// peak HIGH-LOD residency is every chunk whose centre falls inside the
	// streaming radius, capped by the grid itself.
	uint64_t EstimatedHighLodStreamingBytes(uint32_t uVertexStrideBytes, float fHighLodRadiusMetres) const
	{
		if (m_fChunkWorldSize <= 0.0f)
		{
			return 0ull;
		}
		const float fChunksPerAxis = (2.0f * fHighLodRadiusMetres) / m_fChunkWorldSize;
		uint32_t uAxis = static_cast<uint32_t>(fChunksPerAxis) + 1u;
		if (uAxis > Zenith_TerrainDimensionsLimits::uCHUNK_GRID_CAPACITY)
		{
			uAxis = Zenith_TerrainDimensionsLimits::uCHUNK_GRID_CAPACITY;
		}
		const uint32_t uResidentX = uAxis < m_uGridChunksX ? uAxis : m_uGridChunksX;
		const uint32_t uResidentZ = uAxis < m_uGridChunksZ ? uAxis : m_uGridChunksZ;
		return static_cast<uint64_t>(uResidentX) * static_cast<uint64_t>(uResidentZ) *
			static_cast<uint64_t>(ChunkVertexCount(Zenith_TerrainDimensionsLimits::uHIGH_DENSITY_DIVISOR)) *
			static_cast<uint64_t>(uVertexStrideBytes);
	}

	bool FitsStreamingBudget(uint32_t uVertexStrideBytes, float fHighLodRadiusMetres, uint64_t ulBudgetBytes) const
	{
		return EstimatedHighLodStreamingBytes(uVertexStrideBytes, fHighLodRadiusMetres) <= ulBudgetBytes;
	}

	bool operator==(const Zenith_TerrainDimensions& xOther) const = default;
};

// The default spec must reproduce, exactly, the constants every baked terrain
// on disk was authored against. These are the pins that make "commit A rebakes
// byte-identically" a compile-time claim rather than a hope.
static_assert(Zenith_TerrainDimensions::Default().WorldSizeX() == 4096.0f &&
	Zenith_TerrainDimensions::Default().WorldSizeZ() == 4096.0f,
	"Default terrain dimensions must span the historical 4096m grid");
static_assert(Zenith_TerrainDimensions::Default().MaxWorldSize() == 4096.0f,
	"The default square authoring domain must remain 4096m");
static_assert(Zenith_TerrainDimensions::Default().VertexSpacing() == 1.0f,
	"Default terrain dimensions must retain 1m vertex spacing");
static_assert(Zenith_TerrainDimensions::Default().WorldPerImagePixel(4096u) == 1.0f,
	"A 4096px authoring image over the default domain must remain exactly 1m per pixel");
static_assert(Zenith_TerrainDimensions::Default().ChunkVertexCount(1u) == 4225u &&
	Zenith_TerrainDimensions::Default().ChunkIndexCount(1u) == 24576u,
	"Default HIGH chunks must retain 65x65 vertices and 64x64 quads");
static_assert(Zenith_TerrainDimensions::Default().ChunkVertexCount(4u) == 289u &&
	Zenith_TerrainDimensions::Default().ChunkIndexCount(4u) == 1536u,
	"Default LOW/physics chunks must retain their density-divisor-4 topology");
static_assert(Zenith_TerrainDimensions::Default().ChunkCount() ==
	Zenith_TerrainDimensionsLimits::uTOTAL_CHUNK_CAPACITY,
	"The default grid must still fill the fixed chunk capacity");

// ============================================================================
// Zenith_TerrainDimsManifest -- the baked set's copy of its own dimensions.
//
// A baked terrain set is a directory of chunk meshes whose bytes are
// MEANINGLESS without the dimensions they were exported at: the same
// Render_3_4.zgeom decodes to different world positions under a different
// quantisation box. The manifest is written beside the chunks so a set can be
// recognised as stale rather than silently decoded wrong.
//
// STRICT POLICY: a set with no manifest, or one whose manifest disagrees with
// the component's own dimensions, is a STALE BAKE and is rejected through the
// same path as a chunk-count mismatch -- loudly, and without a physics body.
// ============================================================================

namespace Zenith_TerrainDimsManifestFormat
{
	// The four bytes Z, T, D, M read little-endian.
	inline constexpr uint32_t uMAGIC = 0x4D44545Au;
	inline constexpr uint32_t uVERSION = 1u;
	inline constexpr const char* szFILENAME = "TerrainDims.zdata";
}

struct Zenith_TerrainDimsManifest
{
	uint32_t m_uMagic = Zenith_TerrainDimsManifestFormat::uMAGIC;
	uint32_t m_uVersion = Zenith_TerrainDimsManifestFormat::uVERSION;
	float m_fChunkWorldSize = 64.0f;
	uint32_t m_uQuadsPerChunkEdge = 64u;
	uint32_t m_uGridChunksX = 64u;
	uint32_t m_uGridChunksZ = 64u;
	// The square authoring heightfield the set was baked from. Carried so a
	// re-bake at a different image resolution is caught as stale too.
	uint32_t m_uHeightmapImageSize = 4096u;

	static Zenith_TerrainDimsManifest FromDimensions(const Zenith_TerrainDimensions& xDims, uint32_t uHeightmapImageSize)
	{
		Zenith_TerrainDimsManifest xManifest;
		xManifest.m_fChunkWorldSize = xDims.m_fChunkWorldSize;
		xManifest.m_uQuadsPerChunkEdge = xDims.m_uQuadsPerChunkEdge;
		xManifest.m_uGridChunksX = xDims.m_uGridChunksX;
		xManifest.m_uGridChunksZ = xDims.m_uGridChunksZ;
		xManifest.m_uHeightmapImageSize = uHeightmapImageSize;
		return xManifest;
	}

	Zenith_TerrainDimensions ToDimensions() const
	{
		Zenith_TerrainDimensions xDims;
		xDims.m_fChunkWorldSize = m_fChunkWorldSize;
		xDims.m_uQuadsPerChunkEdge = m_uQuadsPerChunkEdge;
		xDims.m_uGridChunksX = m_uGridChunksX;
		xDims.m_uGridChunksZ = m_uGridChunksZ;
		return xDims;
	}

	bool DescribesDimensions(const Zenith_TerrainDimensions& xDims) const
	{
		return ToDimensions() == xDims;
	}

	bool operator==(const Zenith_TerrainDimsManifest& xOther) const = default;
};

// All uint32/float members: the struct is tightly packed with no padding, so
// the on-disk image is a straight memcpy and the size is part of the format.
inline constexpr uint32_t uZENITH_TERRAIN_DIMS_MANIFEST_BYTES = 28u;
static_assert(sizeof(Zenith_TerrainDimsManifest) == uZENITH_TERRAIN_DIMS_MANIFEST_BYTES,
	"The terrain dimensions manifest is a fixed-size on-disk record");

namespace Zenith_TerrainDimsManifestFormat
{
	// Pure byte serialisation, so the format is testable without a filesystem.
	// pOut must have room for uZENITH_TERRAIN_DIMS_MANIFEST_BYTES.
	inline void Write(const Zenith_TerrainDimsManifest& xManifest, void* pOut)
	{
		std::memcpy(pOut, &xManifest, uZENITH_TERRAIN_DIMS_MANIFEST_BYTES);
	}

	// Rejects a short buffer, a foreign magic, an unknown version, and any
	// record whose dimensions do not validate -- a corrupt manifest must read
	// as "stale bake", never as a usable spec.
	inline bool Read(const void* pData, uint64_t ulSize, Zenith_TerrainDimsManifest& xOut)
	{
		if (pData == nullptr || ulSize < uZENITH_TERRAIN_DIMS_MANIFEST_BYTES)
		{
			return false;
		}

		Zenith_TerrainDimsManifest xCandidate;
		std::memcpy(&xCandidate, pData, uZENITH_TERRAIN_DIMS_MANIFEST_BYTES);
		if (xCandidate.m_uMagic != uMAGIC || xCandidate.m_uVersion != uVERSION)
		{
			return false;
		}
		if (!xCandidate.ToDimensions().IsValid() || xCandidate.m_uHeightmapImageSize == 0u)
		{
			return false;
		}

		xOut = xCandidate;
		return true;
	}
}
