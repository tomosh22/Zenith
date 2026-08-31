#include "Zenith.h"
#include "Zenith_Tools_TerrainExport.h"
#include "Flux/Flux_VertexCodec.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Flux/Terrain/Flux_TerrainSourceGrid.h"
#include "Flux/Terrain/Flux_TerrainVertexQuant.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "AssetHandling/Zenith_Image.h"
#include "AssetHandling/Zenith_TextureAsset.h"
// stb_image declarations only — the single STB_IMAGE_IMPLEMENTATION lives in
// Zenith_Tools_TextureExport.cpp; these calls resolve to it at link time.
#pragma warning(push, 0)
#include "stb/stb_image.h"
#pragma warning(pop)

#include <cstring>
#include <cmath>
#include <set>

#include "TaskSystem/Zenith_TaskSystem.h"

// Peak terrain height in world units. Capped at the engine's AABB assumption
// (Flux_TerrainConfig::MAX_TERRAIN_HEIGHT = 512) so chunk frustum bounds stay
// valid. Was 4096, which only stayed in-bounds because of the buggy 0.1 XZ
// scale below (4096 * 0.1 = 409.6m); with the corrected 1.0 scale a value of
// 4096 would put vertices 8x above the culling AABB.
#define MAX_TERRAIN_HEIGHT 512

// The baked heights are a normalised heightmap sample scaled by the constant above,
// so that constant IS the Y extent of the quantisation box every position is packed
// against. If they part company the exporter silently CLAMPS every vertex above the
// box lid, which reads as a flat-topped terrain rather than as a build error.
static_assert(static_cast<float>(MAX_TERRAIN_HEIGHT) ==
	Zenith_TerrainChunkLayout::afPOSITION_BOX_MAX[1] - Zenith_TerrainChunkLayout::afPOSITION_BOX_MIN[1],
	"The exported terrain height scale must equal the position quantisation box's Y extent");

// Generate packed terrain vertex data (20 bytes/vertex)
// Layout: Position(SNORM16x4,8) + UV(UNORM16x2,4) + Normal(SNORM10,4) + Tangent+Sign(SNORM10,4)
//
// The position is quantised against the AUTHORED terrain box, never a per-chunk
// AABB: a chunk's closing edge IS its neighbour's first edge, and only a shared
// box makes those duplicated world positions produce identical words in both
// chunks. A per-chunk fit would open a crack along every border.
//
// UV is UNORM16 (not HALF2) because terrain UVs are authored world metres over
// the whole terrain (e.g. [0, 4096]). HALF only has 10 bits of mantissa, so
// values above 1024 lose sub-integer precision and above 2048 the step is 2 —
// causing pairs of adjacent vertices on the far half of the terrain to collapse
// onto the same UV, which shows as a stretched/compressed strip pattern at
// vertex spacing in any high-contrast diffuse. Unorm16 normalised by the same
// extent is uniform across the whole range, and gets FINER as a terrain shrinks.
static void GenerateTerrainLayoutAndVertexData(Flux_MeshGeometry& xMesh,
	const Zenith_TerrainDimensions& xDims)
{
	for (uint32_t uElement = 0; uElement < Zenith_TerrainChunkLayout::uELEMENT_COUNT; uElement++)
	{
		xMesh.m_xBufferLayout.GetElements().PushBack(
			{ Zenith_TerrainChunkLayout::axELEMENTS[uElement].m_eType });
	}
	xMesh.m_xBufferLayout.CalculateOffsetsAndStrides();

	const uint32_t uCalculatedStride = xMesh.m_xBufferLayout.GetStride();
	Zenith_Assert(uCalculatedStride == Zenith_TerrainChunkLayout::uVERTEX_STRIDE,
		"Terrain exporter layout must match the canonical HIGH terrain vertex stride");
	(void)uCalculatedStride;
	const uint32_t uStride = Zenith_TerrainChunkLayout::uVERTEX_STRIDE;

	xMesh.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * uStride));

	const Flux_PosQuant xQuant = Flux_MakeTerrainPosQuant(xDims);
	const float fUVBoxMax = Flux_TerrainUVBoxMax(xDims);

	for (uint32_t i = 0; i < xMesh.m_uNumVerts; i++)
	{
		u_int8* pVertex = xMesh.m_pVertexData + i * uStride;

		// Position: SNORM16x4 (8 bytes) against the authored box.
		Flux_WriteTerrainVertexPosition(pVertex, xMesh.m_pxPositions[i], xQuant);

		// UV: UNORM16x2 (4 bytes), authored world XZ metres in and out.
		Flux_WriteTerrainVertexUV(pVertex, xMesh.m_pxUVs[i], fUVBoxMax);

		// Normal: SNORM10:10:10:2 (4 bytes), w=0
		Flux_WriteTerrainVertexNormalWord(pVertex, Flux_PackSnorm10_10_10_2(
			xMesh.m_pxNormals[i].x,
			xMesh.m_pxNormals[i].y,
			xMesh.m_pxNormals[i].z,
			0.0f
		));

		// Tangent + BitangentSign: SNORM10:10:10:2 (4 bytes)
		float fBitangentSign = glm::dot(
			glm::cross(glm::vec3(xMesh.m_pxNormals[i]), glm::vec3(xMesh.m_pxTangents[i])),
			glm::vec3(xMesh.m_pxBitangents[i])
		) > 0.0f ? 1.0f : -1.0f;

		Flux_WriteTerrainVertexTangentWord(pVertex, Flux_PackSnorm10_10_10_2(
			xMesh.m_pxTangents[i].x,
			xMesh.m_pxTangents[i].y,
			xMesh.m_pxTangents[i].z,
			fBitangentSign
		));
	}
}

//-----------------------------------------------------------------------------
// Load heightmap from .ztxtr file and return as a single-channel float image
//-----------------------------------------------------------------------------
static Zenith_Image LoadHeightmapFromZtxtr(const std::string& strPath)
{
	// Single .ztxtr parser (no GPU upload). Heightmaps are single-mip R32/R16/RGBA8.
	Flux_SurfaceInfo xInfo;
	Zenith_Vector<uint8_t> xBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes).IsOk())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load .ztxtr file: %s", strPath.c_str());
		return Zenith_Image();
	}

	const int32_t iWidth = static_cast<int32_t>(xInfo.m_uWidth);
	const int32_t iHeight = static_cast<int32_t>(xInfo.m_uHeight);
	const TextureFormat eFormat = xInfo.m_eFormat;
	const void* pData = xBytes.GetDataPointer();

	Zenith_Log(LOG_CATEGORY_TOOLS, "Loading .ztxtr heightmap: %dx%d, format=%d, size=%zu",
		iWidth, iHeight, static_cast<int>(eFormat), static_cast<size_t>(xBytes.GetSize()));

	// Build a float image based on the source format
	Zenith_Image xResult;
	const u_int uCount = static_cast<u_int>(iWidth) * static_cast<u_int>(iHeight);
	if (eFormat == TEXTURE_FORMAT_R32_SFLOAT)
	{
		// 32-bit float single channel - copy directly
		xResult = Zenith_Image(iWidth, iHeight);
		memcpy(xResult.Row(0), pData, static_cast<size_t>(uCount) * sizeof(float));
	}
	else if (eFormat == TEXTURE_FORMAT_R16_UNORM)
	{
		// 16-bit unsigned single channel - normalize to float [0,1]
		xResult = Zenith_Image(iWidth, iHeight);
		const uint16_t* pu16 = static_cast<const uint16_t*>(pData);
		float* pfDst = xResult.Row(0);
		for (u_int u = 0; u < uCount; u++)
		{
			pfDst[u] = pu16[u] / 65535.0f;
		}
	}
	else if (eFormat == TEXTURE_FORMAT_RGBA8_UNORM)
	{
		// RGBA8 - use the red channel (byte 0), normalize to float [0,1]
		xResult = Zenith_Image(iWidth, iHeight);
		const uint8_t* pu8 = static_cast<const uint8_t*>(pData);
		float* pfDst = xResult.Row(0);
		for (u_int u = 0; u < uCount; u++)
		{
			pfDst[u] = pu8[u * 4 + 0] / 255.0f;
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Unsupported texture format for heightmap: %d", static_cast<int>(eFormat));
		return Zenith_Image();
	}

	return xResult;
}

//-----------------------------------------------------------------------------
// Load heightmap from either .ztxtr or a common image format (PNG/etc.)
//-----------------------------------------------------------------------------
static Zenith_Image LoadHeightmapAuto(const std::string& strPath)
{
	// Get file extension
	std::string strExt = strPath.substr(strPath.rfind('.'));

	if (strExt == ZENITH_TEXTURE_EXT)
	{
		return LoadHeightmapFromZtxtr(strPath);
	}

	// Decode common image formats via stb (TIFF was dropped with OpenCV). Force a
	// single channel and reproduce the old depth-based normalization exactly.
	int iWidth = 0, iHeight = 0, iChannels = 0;
	Zenith_Image xResult;

	if (stbi_is_hdr(strPath.c_str()))
	{
		// 32-bit float - use values as-is (old 32-bit-float passthrough)
		float* pfData = stbi_loadf(strPath.c_str(), &iWidth, &iHeight, &iChannels, 1);
		if (!pfData)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load heightmap: %s", strPath.c_str());
			return Zenith_Image();
		}
		xResult = Zenith_Image(iWidth, iHeight);
		memcpy(xResult.Row(0), pfData, static_cast<size_t>(iWidth) * iHeight * sizeof(float));
		stbi_image_free(pfData);
	}
	else if (stbi_is_16_bit(strPath.c_str()))
	{
		// 16-bit unsigned - normalize to float [0,1] (old 16-bit path)
		uint16_t* pu16 = stbi_load_16(strPath.c_str(), &iWidth, &iHeight, &iChannels, 1);
		if (!pu16)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load heightmap: %s", strPath.c_str());
			return Zenith_Image();
		}
		xResult = Zenith_Image(iWidth, iHeight);
		float* pfDst = xResult.Row(0);
		const u_int uCount = static_cast<u_int>(iWidth) * static_cast<u_int>(iHeight);
		for (u_int u = 0; u < uCount; u++)
		{
			pfDst[u] = pu16[u] / 65535.0f;
		}
		stbi_image_free(pu16);
	}
	else
	{
		// 8-bit LDR - normalize to float [0,1] (old 8-bit path). Use the byte loader
		// + manual divide rather than stbi_loadf, which would sRGB-decode the heights.
		uint8_t* pu8 = stbi_load(strPath.c_str(), &iWidth, &iHeight, &iChannels, 1);
		if (!pu8)
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load heightmap (use .ztxtr or PNG): %s", strPath.c_str());
			return Zenith_Image();
		}
		xResult = Zenith_Image(iWidth, iHeight);
		float* pfDst = xResult.Row(0);
		const u_int uCount = static_cast<u_int>(iWidth) * static_cast<u_int>(iHeight);
		for (u_int u = 0; u < uCount; u++)
		{
			pfDst[u] = pu8[u] / 255.0f;
		}
		stbi_image_free(pu8);
	}

	return xResult;
}

Zenith_Image Zenith_Tools_LoadHeightmapAuto(const std::string& strPath)
{
	return LoadHeightmapAuto(strPath);
}

// THE TERRAIN_SIZE / TERRAIN_SCALE DEFINES ARE GONE, and their absence is the
// point of this file's parameterisation. They welded three separate quantities
// into one number: heightmap PIXELS per chunk, world METRES per chunk, and
// SAMPLES per chunk edge. That weld is why a terrain could only ever be 4096m
// wide at 1m spacing. Each is now read from the caller's
// Zenith_TerrainDimensions -- m_uQuadsPerChunkEdge, m_fChunkWorldSize, and the
// image-to-world scale derived from MaxWorldSize().
//
// TERRAIN_SCALE was additionally a multiply by exactly 1.0f applied to every
// position. It is not replaced by a "scale of 1" anywhere: the sample step
// below IS the world step, so there is nothing left to rescale.

// ★ THE SOURCE GRID CARRIES ONE MORE SAMPLE PER EDGE THAN IT HAS CELLS, AND
// THAT CLOSING SAMPLE IS LOAD-BEARING.
//
// ExportChunkBatch below splits this mesh into uNumSplitsX x uNumSplitsZ chunks
// of uCells quads each, and every chunk closes itself by stitching the FIRST
// column/row of its +X / +Z neighbour. The grid therefore needs
// uNumSplits * uCells + 1 samples per edge. Without it the final chunk column
// and row have no neighbour to stitch from, and used to bake incomplete: their
// stitch vertices stayed UNINITIALISED and their stitch index triples stayed
// (0,0,0), which Zenith_TerrainComponent's chunk-topology validator rejects
// outright -- 127 of 4096 chunks silently dropped from LOW LOD *and* physics,
// i.e. no always-resident geometry and no collision along the outer +X/+Z strip
// of every full-grid terrain.
//
// The closing sample is taken with its heightmap coordinate CLAMPED to the last
// texel -- the same clamp the bilinear tap already applies -- so it costs no new
// sampling policy and lands the terrain's outer boundary exactly on
// (grid chunks * chunk world size), which is what the per-terrain quantisation
// box describes. Before this the terrain stopped one sample short of its own
// configured extent.
//
// The grid is sized in CHUNKS and SAMPLES; the heightmap is sized in PIXELS.
// Those used to be the same number. They are not any more, and a border special
// case must never be reintroduced on either count.
void GenerateFullTerrain(const Zenith_Image& xHeightmapImage, Flux_MeshGeometry& xMesh,
	u_int uDensityDivisor, const Zenith_TerrainDimensions& xDims)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");
	Zenith_Assert(xDims.IsValid(), "Terrain export needs valid terrain dimensions");

	u_int uWidth = xHeightmapImage.GetWidth();
	u_int uHeight = xHeightmapImage.GetHeight();

	// THE SOURCE GRID IS SIZED BY THE GRID, NOT BY THE IMAGE. It used to be
	// (image width * density), which silently made the heightmap resolution the
	// terrain's vertex density -- the weld this parameterisation removes. The
	// image is now a SAMPLED FIELD over the square authoring domain, tapped at
	// whatever fractional coordinate each world-space sample lands on.
	const u_int uSamplesX = Flux_TerrainSourceGrid::SampleCountForCells(
		xDims.m_uGridChunksX * xDims.QuadsPerChunkEdge(uDensityDivisor));
	const u_int uSamplesZ = Flux_TerrainSourceGrid::SampleCountForCells(
		xDims.m_uGridChunksZ * xDims.QuadsPerChunkEdge(uDensityDivisor));

	// World metres per source sample, and authoring-image pixels per world
	// metre. At default dimensions over a 4096px heightfield these are exactly
	// (1 * divisor) and exactly 1.0, so every product below is bit-identical to
	// the integer arithmetic it replaces.
	const double dSampleStep = Flux_TerrainSourceGrid::SampleStepWorld(
		static_cast<double>(xDims.VertexSpacing()), uDensityDivisor);
	const double dImagePerWorldX = static_cast<double>(xDims.ImagePixelPerWorld(uWidth));
	const double dImagePerWorldZ = static_cast<double>(xDims.ImagePixelPerWorld(uHeight));

	xMesh.m_uNumVerts = uSamplesX * uSamplesZ;
	xMesh.m_uNumIndices = (uSamplesX - 1) * (uSamplesZ - 1) * 6;
	xMesh.m_pxPositions = static_cast<glm::highp_vec3*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * sizeof(glm::highp_vec3)));
	xMesh.m_pxUVs = static_cast<glm::vec2*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * sizeof(glm::vec2)));
	xMesh.m_pxNormals = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * sizeof(glm::vec3)));
	xMesh.m_pxTangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * sizeof(glm::vec3)));
	xMesh.m_pxBitangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumVerts * sizeof(glm::vec3)));
	for (size_t i = 0; i < xMesh.m_uNumVerts; i++)
	{
		xMesh.m_pxPositions[i] = { 0,0,0 };
		xMesh.m_pxUVs[i] = { 0,0 };
		xMesh.m_pxNormals[i] = { 0,0,0 };
		xMesh.m_pxTangents[i] = { 0,0,0 };
		xMesh.m_pxBitangents[i] = { 0,0,0 };
	}
	xMesh.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(xMesh.m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

	for (u_int z = 0; z < uSamplesZ; ++z)
	{
		for (u_int x = 0; x < uSamplesX; ++x)
		{
			// The authoring-image coordinate this world-space sample taps.
			// FRACTIONAL in general; the bilinear filter below is what consumes
			// it, and both of its taps clamp.
			const double dWorldX = Flux_TerrainSourceGrid::WorldForSample(x, dSampleStep);
			const double dWorldZ = Flux_TerrainSourceGrid::WorldForSample(z, dSampleStep);
			glm::vec2 xUV = { dWorldX * dImagePerWorldX, dWorldZ * dImagePerWorldZ };
			u_int offset = (z * uSamplesX) + x;

			// BOTH taps clamp. The closing sample sits one texel past the
			// heightmap, so an unclamped floor() would read out of bounds; with
			// x0 == x1 the lerp below collapses onto the edge texel, which is
			// the CLAMP_TO_EDGE result we want.
			u_int x0 = std::min(static_cast<u_int>(std::floor(xUV.x)), uWidth - 1);
			u_int x1 = std::min(static_cast<u_int>(std::ceil(xUV.x)), uWidth - 1);
			u_int y0 = std::min(static_cast<u_int>(std::floor(xUV.y)), uHeight - 1);
			u_int y1 = std::min(static_cast<u_int>(std::ceil(xUV.y)), uHeight - 1);

			double dHeight;
			{
				float fTopLeft = xHeightmapImage.At(y0, x0);
				float fTopRight = xHeightmapImage.At(y0, x1);
				float fBottomLeft = xHeightmapImage.At(y1, x0);
				float fBottomRight = xHeightmapImage.At(y1, x1);

				double dWeightX = xUV.x - x0;
				double dWeightY = xUV.y - y0;

				double dTop = fTopRight * dWeightX + fTopLeft * (1.f - dWeightX);
				double dBottom = fBottomRight * dWeightX + fBottomLeft * (1.f - dWeightX);

				dHeight = dBottom * dWeightY + dTop * (1.f - dWeightY);

				// CLAMP the sample into the box's normalised range BEFORE it feeds
				// either stream. R32_SFLOAT / HDR heightmaps load unnormalised, and
				// this is the one site where an out-of-range sample could reach the
				// two position streams DIFFERENTLY: the packed snorm16 lane clamps at
				// the box lid on its own, but m_pxPositions would keep the raw value —
				// and at load the validator's cross-stream check rejects the whole
				// chunk (no LOW geometry, no physics body). Clamping here keeps both
				// streams identical, so an over-range heightmap degrades to the
				// documented flat-topped terrain instead of silent chunk rejection.
				dHeight = std::clamp(dHeight, 0.0, 1.0);
			}

			xMesh.m_pxPositions[offset] = glm::highp_vec3(dWorldX, dHeight * MAX_TERRAIN_HEIGHT, dWorldZ);
			// UVs are AUTHORED WORLD XZ IN METRES, not heightmap pixels. They
			// coincide at default dimensions (1m per pixel), which is why the
			// two spellings were indistinguishable until now; only the world
			// reading survives a terrain whose image and extent differ.
			xMesh.m_pxUVs[offset] = glm::vec2(dWorldX, dWorldZ);
		}
	}

	size_t i = 0;
	for (u_int z = 0; z < uSamplesZ - 1; ++z)
	{
		for (u_int x = 0; x < uSamplesX - 1; ++x)
		{
			u_int a = (z * uSamplesX) + x;
			u_int b = (z * uSamplesX) + x + 1;
			u_int c = ((z + 1) * uSamplesX) + x + 1;
			u_int d = ((z + 1) * uSamplesX) + x;
			xMesh.m_puIndices[i++] = a;
			xMesh.m_puIndices[i++] = c;
			xMesh.m_puIndices[i++] = b;
			xMesh.m_puIndices[i++] = c;
			xMesh.m_puIndices[i++] = a;
			xMesh.m_puIndices[i++] = d;
		}
	}

	xMesh.GenerateNormals();
	xMesh.GenerateTangents();
	xMesh.GenerateBitangents();
}

struct ChunkExportData
{
	const Flux_MeshGeometry* pxFullMesh;
	u_int uNumSplitsX;
	u_int uNumSplitsZ;
	u_int uTotalChunks;
	Flux_TerrainExportRect xRect;
	bool bUseRect;
	u_int uDensityDivisor;
	Zenith_TerrainDimensions xDims;
	std::string strOutputDir;
	std::string strName;
};

static void ResolveChunkExportCoordinates(const ChunkExportData& xData,
	u_int uChunkOrdinal, u_int& uChunkXOut, u_int& uChunkZOut)
{
	if (!xData.bUseRect)
	{
		uChunkXOut = uChunkOrdinal % xData.uNumSplitsX;
		uChunkZOut = uChunkOrdinal / xData.uNumSplitsX;
		return;
	}

	uint32_t uAbsoluteX = 0;
	uint32_t uAbsoluteZ = 0;
	const bool bMapped = xData.xRect.TryGetChunkCoords(
		static_cast<uint32_t>(uChunkOrdinal), uAbsoluteX, uAbsoluteZ);
	Zenith_Assert(bMapped, "Invalid compact terrain export ordinal");
	if (bMapped)
	{
		uChunkXOut = static_cast<u_int>(uAbsoluteX);
		uChunkZOut = static_cast<u_int>(uAbsoluteZ);
	}
}

static void ValidateHighChunkCounts(u_int uDensityDivisor,
	const Zenith_TerrainDimensions& xDims, const Flux_MeshGeometry& xMesh)
{
	if (uDensityDivisor != Zenith_TerrainDimensionsLimits::uHIGH_DENSITY_DIVISOR)
	{
		return;
	}

	// The counts come from the SPEC now, not from the layout header's default
	// pins -- the two agree only at default dimensions, and the runtime chunk
	// validator checks against the component's spec by the same route.
	Zenith_Assert(xMesh.m_uNumVerts == xDims.ChunkVertexCount(uDensityDivisor),
		"HIGH terrain exporter vertex count must match the terrain's own dimensions");
	Zenith_Assert(xMesh.m_uNumIndices == xDims.ChunkIndexCount(uDensityDivisor),
		"HIGH terrain exporter index count must match the terrain's own dimensions");
	(void)xMesh;
}

static void ExportChunkBatch(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	const ChunkExportData* pxData = static_cast<const ChunkExportData*>(pData);
	const Flux_MeshGeometry& xFullMesh = *pxData->pxFullMesh;
	const u_int uTotalChunks = pxData->uTotalChunks;
	const u_int uDensityDivisor = pxData->uDensityDivisor;
	const Zenith_TerrainDimensions& xDims = pxData->xDims;

	// Quads per chunk edge, and the row stride of the source grid
	// GenerateFullTerrain produced -- one MORE than the heightmap's column count
	// at this density (see the note there). That closing column/row is what lets
	// EVERY chunk, the last one included, stitch its +X / +Z edge; there is no
	// border special case below and there must never be one again.
	const u_int uCells = xDims.QuadsPerChunkEdge(uDensityDivisor);
	const u_int uSourceRowLength = Flux_TerrainSourceGrid::SampleCountPerEdge(pxData->uNumSplitsX, uCells);

	u_int uChunksPerInvocation = (uTotalChunks + uNumInvocations - 1) / uNumInvocations;
	u_int uStartChunk = uInvocationIndex * uChunksPerInvocation;
	u_int uEndChunk = std::min(uStartChunk + uChunksPerInvocation, uTotalChunks);

	for (u_int uChunkIndex = uStartChunk; uChunkIndex < uEndChunk; uChunkIndex++)
	{
	u_int x = 0;
	u_int z = 0;
	ResolveChunkExportCoordinates(*pxData, uChunkIndex, x, z);

	Flux_MeshGeometry xSubMesh;
	xSubMesh.m_uNumVerts = Flux_TerrainSourceGrid::ChunkVertexCount(uCells);
	xSubMesh.m_uNumIndices = Flux_TerrainSourceGrid::ChunkIndexCount(uCells);
	ValidateHighChunkCounts(uDensityDivisor, xDims, xSubMesh);
	xSubMesh.m_pxPositions = static_cast<glm::highp_vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::highp_vec3)));
	xSubMesh.m_pxUVs = static_cast<glm::vec2*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec2)));
	xSubMesh.m_pxNormals = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	xSubMesh.m_pxTangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	xSubMesh.m_pxBitangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	// Zero EVERY attribute, not just normals/tangents. The stitch passes below now
	// fill all (uCells + 1)^2 slots, but these buffers come from raw Allocate() and
	// a partially-written one used to serialize 0xCDCDCDCD straight into the asset
	// -- which then poisoned the chunk AABB via GenerateAABBFromVertices. An asset
	// file must never be able to carry uninitialised memory.
	for (size_t i = 0; i < xSubMesh.m_uNumVerts; i++)
	{
		xSubMesh.m_pxPositions[i] = { 0,0,0 };
		xSubMesh.m_pxUVs[i] = { 0,0 };
		xSubMesh.m_pxNormals[i] = { 0,0,0 };
		xSubMesh.m_pxTangents[i] = { 0,0,0 };
		xSubMesh.m_pxBitangents[i] = { 0,0,0 };
	}
	xSubMesh.m_puIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumIndices * sizeof(u_int)));
	memset(xSubMesh.m_puIndices, 0, xSubMesh.m_uNumIndices * sizeof(u_int));

	u_int uHeighestNewOffset = 0;
#ifdef ZENITH_ASSERT
	std::set<u_int> xFoundOldIndices;
	std::set<u_int> xFoundNewIndices;
#endif
	u_int* puRightEdgeIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(uCells * sizeof(u_int)));
	u_int* puTopEdgeIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(uCells * sizeof(u_int)));
	u_int uTopRightFromBoth = 0;

	for (u_int subZ = 0; subZ < uCells; subZ++)
	{
		for (u_int subX = 0; subX < uCells; subX++)
		{
			u_int uNewOffset = (subZ * uCells) + subX;

			Zenith_Assert((x * uCells) + subX < uSourceRowLength, "Chunk sample column ran past the end of the source row");
			u_int uOldOffset = Flux_TerrainSourceGrid::SampleIndex(x, z, subX, subZ, uCells, uSourceRowLength);

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subX == uCells - 1)
				puRightEdgeIndices[subZ] = uNewOffset;
			if (subZ == uCells - 1)
				puTopEdgeIndices[subX] = uNewOffset;
			if (subX == uCells - 1 && subZ == uCells - 1)
				uTopRightFromBoth = uNewOffset;

			// Load-bearing OUTSIDE the assert block: the stitch passes below
			// allocate their vertex slots by pre-incrementing this.
			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}
	}

	size_t indexIndex = 0;
	for (u_int indexZ = 0; indexZ < uCells - 1; indexZ++)
	{
		for (u_int indexX = 0; indexX < uCells - 1; indexX++)
		{
			u_int a = (indexZ * uCells) + indexX;
			u_int b = (indexZ * uCells) + indexX + 1;
			u_int c = ((indexZ + 1) * uCells) + indexX + 1;
			u_int d = ((indexZ + 1) * uCells) + indexX;
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = b;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = d;
			Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
		}
	}

	// +X stitch: the chunk's closing vertex column, taken from the first column of
	// the neighbouring chunk. UNCONDITIONAL -- the source grid carries a closing
	// column for the last chunk too.
	u_int uTopRightFromX = 0;
	{
		u_int subX = uCells;
		for (u_int subZ = 0; subZ < uCells; subZ++)
		{
			u_int uNewOffset = ++uHeighestNewOffset;

			Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

			Zenith_Assert((x * uCells) + subX < uSourceRowLength, "Chunk sample column ran past the end of the source row");
			u_int uOldOffset = Flux_TerrainSourceGrid::SampleIndex(x, z, subX, subZ, uCells, uSourceRowLength);

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subZ == uCells - 1)
				uTopRightFromX = uNewOffset;

			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}

		uHeighestNewOffset -= (uCells - 1);

		for (u_int indexZ = 0; indexZ < uCells - 1; indexZ++)
		{
			u_int a = puRightEdgeIndices[indexZ + 1];
			u_int c = uHeighestNewOffset++;
			u_int b = puRightEdgeIndices[indexZ];
			u_int d = uHeighestNewOffset;

			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = b;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = d;
			Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
		}
	}

	// +Z stitch: same, for the closing vertex row. Also unconditional.
	u_int uTopRightFromZ = 0;
	{
		u_int subZ = uCells;
		for (u_int subX = 0; subX < uCells; subX++)
		{
			u_int uNewOffset = ++uHeighestNewOffset;

			Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

			Zenith_Assert((x * uCells) + subX < uSourceRowLength, "Chunk sample column ran past the end of the source row");
			u_int uOldOffset = Flux_TerrainSourceGrid::SampleIndex(x, z, subX, subZ, uCells, uSourceRowLength);

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subX == uCells - 1)
				uTopRightFromZ = uNewOffset;

			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}

		uHeighestNewOffset -= (uCells - 1);

		for (u_int indexX = 0; indexX < uCells - 1; indexX++)
		{
			u_int c = puTopEdgeIndices[indexX + 1];
			u_int a = uHeighestNewOffset++;
			u_int b = puTopEdgeIndices[indexX];
			u_int d = uHeighestNewOffset;

			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = b;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = d;
			Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
		}
	}

	// The single corner vertex shared by both stitches, and the quad that closes
	// the chunk. Also unconditional.
	{
		u_int subZ = uCells;
		u_int subX = uCells;
		u_int uNewOffset = ++uHeighestNewOffset;

		Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

		Zenith_Assert((x * uCells) + subX < uSourceRowLength, "Chunk sample column ran past the end of the source row");
		u_int uOldOffset = Flux_TerrainSourceGrid::SampleIndex(x, z, subX, subZ, uCells, uSourceRowLength);

		Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

		Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
		Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

		xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
		xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
		xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
		xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
		xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

		uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
		xFoundOldIndices.insert(uOldOffset);
		xFoundNewIndices.insert(uNewOffset);
#endif

		u_int a = uTopRightFromX;
		u_int d = uTopRightFromBoth;
		u_int c = uTopRightFromZ;
		u_int b = uHeighestNewOffset;

		xSubMesh.m_puIndices[indexIndex++] = a;
		xSubMesh.m_puIndices[indexIndex++] = c;
		xSubMesh.m_puIndices[indexIndex++] = b;
		xSubMesh.m_puIndices[indexIndex++] = c;
		xSubMesh.m_puIndices[indexIndex++] = a;
		xSubMesh.m_puIndices[indexIndex++] = d;
		Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
	}

	// COMPLETENESS INVARIANT. Every chunk must have filled every index slot and
	// every vertex slot -- a partially-written chunk is exactly the defect the
	// border special cases used to produce (trailing (0,0,0) index triples +
	// uninitialised vertices), which the runtime chunk-topology validator then
	// rejects, silently dropping the chunk from LOW LOD and physics alike. Assert
	// it HERE, at bake time, where the coordinates are still in hand.
	Zenith_Assert(indexIndex == xSubMesh.m_uNumIndices,
		"Terrain chunk (%u,%u) wrote %llu of %u indices -- incomplete chunk",
		x, z, static_cast<unsigned long long>(indexIndex), xSubMesh.m_uNumIndices);
	Zenith_Assert(uHeighestNewOffset == xSubMesh.m_uNumVerts - 1,
		"Terrain chunk (%u,%u) wrote %u of %u vertices -- incomplete chunk",
		x, z, uHeighestNewOffset + 1, xSubMesh.m_uNumVerts);

	GenerateTerrainLayoutAndVertexData(xSubMesh, xDims);
	xSubMesh.Export((pxData->strOutputDir + pxData->strName + std::string("_") + std::to_string(x) + std::string("_") + std::to_string(z) + std::string(ZENITH_GEOMETRY_EXT)).c_str());

	Zenith_MemoryManagement::Deallocate(puRightEdgeIndices);
	Zenith_MemoryManagement::Deallocate(puTopEdgeIndices);
	} // end for uChunkIndex
}

static bool ExportMeshInternal(u_int uDensityDivisor, const std::string& strName,
	const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims, const Flux_TerrainExportRect* pxRect)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor-1)) == 0, "Density divisor must be a power of 2");

	Zenith_Assert(!xHeightmap.IsEmpty(), "Invalid heightmap image");

	u_int uImageWidth = xHeightmap.GetWidth();
	u_int uImageHeight = xHeightmap.GetHeight();

	// THE IMAGE NO LONGER SIZES THE GRID -- the spec does, and the image is a
	// field sampled over the SQUARE AUTHORING DOMAIN [0, MaxWorldSize()]^2 that
	// every per-set map (heightfield, splat, grass) shares. So the only thing
	// left to require of it is that it IS square; a non-square one would give
	// the two axes different world-per-pixel scales and shear the terrain.
	if (!xDims.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS, "Terrain export refused: invalid terrain dimensions");
		return false;
	}
	if (uImageWidth != uImageHeight)
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"Terrain export refused: heightmap %ux%u is not square, so it cannot span the square authoring domain",
			uImageWidth, uImageHeight);
		return false;
	}

	u_int uNumSplitsX = xDims.m_uGridChunksX;
	u_int uNumSplitsZ = xDims.m_uGridChunksZ;
	if (pxRect != nullptr && (!pxRect->IsValid() ||
		static_cast<u_int>(pxRect->GetMaxX()) >= uNumSplitsX ||
		static_cast<u_int>(pxRect->GetMaxY()) >= uNumSplitsZ))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"Terrain export rectangle exceeds the active chunk grid (%ux%u)",
			uNumSplitsX, uNumSplitsZ);
		return false;
	}

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xFullMesh, uDensityDivisor, xDims);

	u_int uTotalChunks = pxRect != nullptr
		? static_cast<u_int>(pxRect->ChunkCount())
		: uNumSplitsX * uNumSplitsZ;

	ChunkExportData xChunkData;
	xChunkData.pxFullMesh = &xFullMesh;
	xChunkData.uNumSplitsX = uNumSplitsX;
	xChunkData.uNumSplitsZ = uNumSplitsZ;
	xChunkData.uTotalChunks = uTotalChunks;
	xChunkData.bUseRect = pxRect != nullptr;
	if (pxRect != nullptr)
	{
		xChunkData.xRect = *pxRect;
	}
	xChunkData.uDensityDivisor = uDensityDivisor;
	xChunkData.xDims = xDims;
	xChunkData.strOutputDir = strOutputDir;
	xChunkData.strName = strName;

	u_int uNumInvocations = std::min(static_cast<u_int>(64), uTotalChunks);
	Zenith_DataParallelTask xChunkTask(ZENITH_PROFILE_ZONE("Flux Terrain"), ExportChunkBatch, &xChunkData, uNumInvocations, true);
	g_xEngine.Tasks().SubmitDataParallelTask(&xChunkTask);
	xChunkTask.WaitUntilComplete();
	return true;
}

static void ExportMesh(u_int uDensityDivisor, std::string strName,
	const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims)
{
	(void)ExportMeshInternal(uDensityDivisor, strName, xHeightmap, strOutputDir, xDims, nullptr);
}

// ---- the dimensions manifest ------------------------------------------------
//
// A baked set is a directory of quantised chunks whose bytes mean nothing
// without the dimensions they were packed against. The manifest is written LAST
// -- after all three bakes succeed -- so a half-written set never claims to be
// a complete one at these dimensions.

static std::string TerrainDimsManifestPath(const std::string& strOutputDir)
{
	return strOutputDir + Zenith_TerrainDimsManifestFormat::szFILENAME;
}

static void WriteTerrainDimsManifest(const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims, u_int uHeightmapImageSize)
{
	const Zenith_TerrainDimsManifest xManifest =
		Zenith_TerrainDimsManifest::FromDimensions(xDims, uHeightmapImageSize);
	u_int8 auBytes[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
	Zenith_TerrainDimsManifestFormat::Write(xManifest, auBytes);
	Zenith_FileAccess::WriteFile(TerrainDimsManifestPath(strOutputDir).c_str(),
		auBytes, uZENITH_TERRAIN_DIMS_MANIFEST_BYTES);

	Zenith_Log(LOG_CATEGORY_TOOLS,
		"[TerrainDims] wrote manifest: chunk=%.2fm quads=%u grid=%ux%u world=%.1fx%.1fm image=%upx",
		xDims.m_fChunkWorldSize, xDims.m_uQuadsPerChunkEdge,
		xDims.m_uGridChunksX, xDims.m_uGridChunksZ,
		xDims.WorldSizeX(), xDims.WorldSizeZ(), uHeightmapImageSize);
}

// A RECT bake writes SOME of a set's chunks. Mixing dimensions inside one
// directory produces a set that loads, validates, and renders a terrain torn in
// half -- which is exactly the failure mode a manifest exists to make impossible.
//
// So: an existing manifest that DISAGREES refuses the export outright. A
// directory with no manifest is one this export is ESTABLISHING (the editor's
// rect bake deletes the set's meshes and its manifest before it runs, and a
// recipe-driven bake prepares a fresh directory), so the manifest is written.
static bool EnsureTerrainDimsManifest(const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims, u_int uHeightmapImageSize)
{
	const std::string strPath = TerrainDimsManifestPath(strOutputDir);
	u_int8 auBytes[uZENITH_TERRAIN_DIMS_MANIFEST_BYTES] = {};
	if (!Zenith_FileAccess::ReadPrefix(strPath.c_str(), auBytes, uZENITH_TERRAIN_DIMS_MANIFEST_BYTES))
	{
		WriteTerrainDimsManifest(strOutputDir, xDims, uHeightmapImageSize);
		return true;
	}

	Zenith_TerrainDimsManifest xExisting;
	if (!Zenith_TerrainDimsManifestFormat::Read(auBytes, uZENITH_TERRAIN_DIMS_MANIFEST_BYTES, xExisting))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"[TerrainDims] rect export refused: %s carries a corrupt dimensions manifest. "
			"Delete it and re-bake the whole set.",
			strPath.c_str());
		return false;
	}

	if (!xExisting.DescribesDimensions(xDims) || xExisting.m_uHeightmapImageSize != uHeightmapImageSize)
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"[TerrainDims] rect export refused: set was baked at chunk=%.2fm quads=%u grid=%ux%u image=%upx, "
			"but this export is chunk=%.2fm quads=%u grid=%ux%u image=%upx. "
			"A partial re-bake at other dimensions would tear the terrain in half.",
			xExisting.m_fChunkWorldSize, xExisting.m_uQuadsPerChunkEdge,
			xExisting.m_uGridChunksX, xExisting.m_uGridChunksZ, xExisting.m_uHeightmapImageSize,
			xDims.m_fChunkWorldSize, xDims.m_uQuadsPerChunkEdge,
			xDims.m_uGridChunksX, xDims.m_uGridChunksZ, uHeightmapImageSize);
		return false;
	}

	return true;
}

static void ExportHeightmapInternal(const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims)
{
	Zenith_Assert(!xHeightmap.IsEmpty(), "Invalid heightmap");

	// Export HIGH detail render meshes (density divisor 1, streamed dynamically)
	ExportMesh(Zenith_TerrainDimensionsLimits::uHIGH_DENSITY_DIVISOR, "Render", xHeightmap, strOutputDir, xDims);

	// Export LOW detail render meshes (density divisor 4, always resident)
	ExportMesh(Zenith_TerrainDimensionsLimits::uLOW_DENSITY_DIVISOR, "Render_LOW", xHeightmap, strOutputDir, xDims);

	// Export physics mesh (density divisor 4). This remains lower-poly than the
	// HIGH render mesh, but gives characters a four-metre collision surface rather
	// than the visibly coarse eight-metre one.
	ExportMesh(Zenith_TerrainDimensionsLimits::uPHYSICS_DENSITY_DIVISOR, "Physics", xHeightmap, strOutputDir, xDims);

	WriteTerrainDimsManifest(strOutputDir, xDims, xHeightmap.GetWidth());
}

static bool ExportHeightmapRectInternal(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Zenith_TerrainDimensions& xDims,
	const Flux_TerrainExportRect& xRect)
{
	if (xHeightmap.IsEmpty() || !xRect.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"Rect terrain export requires a valid heightmap and export rectangle");
		return false;
	}

	if (!EnsureTerrainDimsManifest(strOutputDir, xDims, xHeightmap.GetWidth()))
	{
		return false;
	}

	return ExportMeshInternal(Zenith_TerrainDimensionsLimits::uHIGH_DENSITY_DIVISOR,
			"Render", xHeightmap, strOutputDir, xDims, &xRect) &&
		ExportMeshInternal(Zenith_TerrainDimensionsLimits::uLOW_DENSITY_DIVISOR,
			"Render_LOW", xHeightmap, strOutputDir, xDims, &xRect) &&
		ExportMeshInternal(Zenith_TerrainDimensionsLimits::uPHYSICS_DENSITY_DIVISOR,
			"Physics", xHeightmap, strOutputDir, xDims, &xRect);
}

void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Heightmap=%s, Output=%s, Grid=%ux%u @ %.2fm",
		strHeightmapPath.c_str(), strOutputDir.c_str(),
		xDims.m_uGridChunksX, xDims.m_uGridChunksZ, xDims.m_fChunkWorldSize);

	Zenith_Image xHeightmap = LoadHeightmapAuto(strHeightmapPath);
	ExportHeightmapInternal(xHeightmap, strOutputDir, xDims);

	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Export complete");
}

void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Zenith_TerrainDimensions& xDims)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Output=%s, Grid=%ux%u @ %.2fm",
		strOutputDir.c_str(), xDims.m_uGridChunksX, xDims.m_uGridChunksZ, xDims.m_fChunkWorldSize);
	ExportHeightmapInternal(xHeightmap, strOutputDir, xDims);
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Export complete");
}

bool ExportHeightmapFromMatRect(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Zenith_TerrainDimensions& xDims,
	const Flux_TerrainExportRect& xRect)
{
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"ExportHeightmapFromMatRect: Output=%s Bounds=[%d,%d]-[%d,%d] Chunks=%u Files=%u",
		strOutputDir.c_str(), xRect.GetMinX(), xRect.GetMinY(),
		xRect.GetMaxX(), xRect.GetMaxY(), xRect.ChunkCount(), xRect.ChunkCount() * 3);
	const bool bExported = ExportHeightmapRectInternal(xHeightmap, strOutputDir, xDims, xRect);
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMatRect: Export %s",
		bExported ? "complete" : "failed");
	return bExported;
}
