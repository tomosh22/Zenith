#include "Zenith.h"
#include "Zenith_Tools_TerrainExport.h"
#include "Flux/Flux_VertexCodec.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Core/Zenith_TerrainChunkLayout.h"
#include "Flux/Terrain/Flux_TerrainSourceGrid.h"
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

// Generate packed terrain vertex data (28 bytes/vertex)
// Layout: Position(FLOAT3,12) + UV(FLOAT2,8) + Normal(SNORM10,4) + Tangent+Sign(SNORM10,4)
//
// UV is FLOAT2 (not HALF2) because terrain UVs are stored as full heightmap
// pixel coordinates (e.g. [0, 4095]). HALF only has 10 bits of mantissa, so
// values above 1024 lose sub-integer precision and above 2048 the step is 2 —
// causing pairs of adjacent vertices on the upper half of the terrain to
// collapse onto the same UV. That manifests as a stretched/compressed strip
// pattern at vertex spacing in any high-contrast diffuse texture sampled at
// those UVs. FLOAT32 has full precision across the whole range; +4 bytes per
// vertex is acceptable.
static void GenerateTerrainLayoutAndVertexData(Flux_MeshGeometry& xMesh)
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

	for (uint32_t i = 0; i < xMesh.m_uNumVerts; i++)
	{
		u_int8* pVertex = xMesh.m_pVertexData + i * uStride;
		uint32_t uOffset = 0;

		// Position: float3 (12 bytes)
		float* pPos = reinterpret_cast<float*>(pVertex + uOffset);
		pPos[0] = xMesh.m_pxPositions[i].x;
		pPos[1] = xMesh.m_pxPositions[i].y;
		pPos[2] = xMesh.m_pxPositions[i].z;
		uOffset += Zenith_TerrainChunkLayout::axELEMENTS[0].m_uSize;

		// UV: float2 (8 bytes) — full 32-bit precision so heightmap-pixel-scale
		// UVs don't collapse on the upper half of the terrain.
		float* pUV = reinterpret_cast<float*>(pVertex + uOffset);
		pUV[0] = xMesh.m_pxUVs[i].x;
		pUV[1] = xMesh.m_pxUVs[i].y;
		uOffset += Zenith_TerrainChunkLayout::axELEMENTS[1].m_uSize;

		// Normal: SNORM10:10:10:2 (4 bytes), w=0
		uint32_t* pNormal = reinterpret_cast<uint32_t*>(pVertex + uOffset);
		*pNormal = Flux_PackSnorm10_10_10_2(
			xMesh.m_pxNormals[i].x,
			xMesh.m_pxNormals[i].y,
			xMesh.m_pxNormals[i].z,
			0.0f
		);
		uOffset += Zenith_TerrainChunkLayout::axELEMENTS[2].m_uSize;

		// Tangent + BitangentSign: SNORM10:10:10:2 (4 bytes)
		float fBitangentSign = glm::dot(
			glm::cross(glm::vec3(xMesh.m_pxNormals[i]), glm::vec3(xMesh.m_pxTangents[i])),
			glm::vec3(xMesh.m_pxBitangents[i])
		) > 0.0f ? 1.0f : -1.0f;

		uint32_t* pTangent = reinterpret_cast<uint32_t*>(pVertex + uOffset);
		*pTangent = Flux_PackSnorm10_10_10_2(
			xMesh.m_pxTangents[i].x,
			xMesh.m_pxTangents[i].y,
			xMesh.m_pxTangents[i].z,
			fBitangentSign
		);
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

//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO world units per heightmap pixel. 1.0 => a 4096px heightmap bakes a
// 4096-unit-wide terrain, which is what Flux_TerrainConfig (CHUNK_SIZE_WORLD=64
// * CHUNK_GRID_SIZE=64 = 4096) and the games assume. Was 0.1f, a long-standing
// bug that produced a 409.6-unit terrain mismatched with the config/LOD/docs.
#define TERRAIN_SCALE 1.0f

// ★ THE SOURCE GRID CARRIES ONE MORE SAMPLE PER EDGE THAN THE HEIGHTMAP HAS
// COLUMNS, AND THAT CLOSING SAMPLE IS LOAD-BEARING.
//
// ExportChunkBatch below splits this mesh into uNumSplits x uNumSplits chunks of
// uCells quads each, and every chunk closes itself by stitching the FIRST
// column/row of its +X / +Z neighbour. The grid therefore needs
// uNumSplits * uCells + 1 samples per edge -- exactly one more than
// (heightmap width * density). Without it the final chunk column (x == 63) and
// row (z == 63) have no neighbour to stitch from, and used to bake incomplete:
// their stitch vertices stayed UNINITIALISED and their stitch index triples
// stayed (0,0,0), which Zenith_TerrainComponent's chunk-topology validator
// rejects outright -- 127 of 4096 chunks silently dropped from LOW LOD *and*
// physics, i.e. no always-resident geometry and no collision along the outer
// +X/+Z strip of every full-grid terrain.
//
// The closing sample is taken with its heightmap coordinate CLAMPED to the last
// texel -- the same clamp the bilinear tap already applies -- so it costs no new
// sampling policy and lands the terrain's outer boundary exactly on
// TERRAIN_SIZE * uNumSplits world units (4096). Before this the terrain stopped
// one sample short of its own configured extent.
void GenerateFullTerrain(const Zenith_Image& xHeightmapImage, Flux_MeshGeometry& xMesh, u_int uDensityDivisor)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	u_int uWidth = xHeightmapImage.GetWidth();
	u_int uHeight = xHeightmapImage.GetHeight();

	const u_int uSamplesX = Flux_TerrainSourceGrid::SampleCountForCells(static_cast<u_int>(uWidth * fDensity));
	const u_int uSamplesZ = Flux_TerrainSourceGrid::SampleCountForCells(static_cast<u_int>(uHeight * fDensity));

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
			glm::vec2 xUV = { static_cast<double>(x) / fDensity , static_cast<double>(z) / fDensity };
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
			}

			xMesh.m_pxPositions[offset] = glm::highp_vec3((double)x / fDensity, dHeight * MAX_TERRAIN_HEIGHT, (double)z / fDensity) * static_cast<float>(TERRAIN_SCALE);
			glm::vec2 fUV = glm::vec2(x, z);
			xMesh.m_pxUVs[offset] = fUV / fDensity;
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
	float fDensity;
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

static void ValidateHighChunkCounts(float fDensity, const Flux_MeshGeometry& xMesh)
{
	if (fDensity != 1.0f)
	{
		return;
	}

	Zenith_Assert(xMesh.m_uNumVerts == Zenith_TerrainChunkLayout::uHIGH_CHUNK_VERTEX_COUNT,
		"HIGH terrain exporter vertex count must match the canonical terrain contract");
	Zenith_Assert(xMesh.m_uNumIndices == Zenith_TerrainChunkLayout::uHIGH_CHUNK_INDEX_COUNT,
		"HIGH terrain exporter index count must match the canonical terrain contract");
	(void)xMesh;
}

static void ExportChunkBatch(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	const ChunkExportData* pxData = static_cast<const ChunkExportData*>(pData);
	const Flux_MeshGeometry& xFullMesh = *pxData->pxFullMesh;
	const u_int uTotalChunks = pxData->uTotalChunks;
	const float fDensity = pxData->fDensity;

	// Quads per chunk edge, and the row stride of the source grid
	// GenerateFullTerrain produced -- one MORE than the heightmap's column count
	// at this density (see the note there). That closing column/row is what lets
	// EVERY chunk, the last one included, stitch its +X / +Z edge; there is no
	// border special case below and there must never be one again.
	const u_int uCells = static_cast<u_int>(TERRAIN_SIZE * fDensity);
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
	ValidateHighChunkCounts(fDensity, xSubMesh);
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

	GenerateTerrainLayoutAndVertexData(xSubMesh);
	xSubMesh.Export((pxData->strOutputDir + pxData->strName + std::string("_") + std::to_string(x) + std::string("_") + std::to_string(z) + std::string(ZENITH_MESH_EXT)).c_str());

	Zenith_MemoryManagement::Deallocate(puRightEdgeIndices);
	Zenith_MemoryManagement::Deallocate(puTopEdgeIndices);
	} // end for uChunkIndex
}

static bool ExportMeshInternal(u_int uDensityDivisor, const std::string& strName,
	const Zenith_Image& xHeightmap, const std::string& strOutputDir,
	const Flux_TerrainExportRect* pxRect)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor-1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	Zenith_Assert(!xHeightmap.IsEmpty(), "Invalid heightmap image");

	u_int uImageWidth = xHeightmap.GetWidth();
	u_int uImageHeight = xHeightmap.GetHeight();

	Zenith_Assert(static_cast<u_int>(uImageWidth * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain width");
	Zenith_Assert(static_cast<u_int>(uImageHeight * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain height");

	u_int uNumSplitsX = uImageWidth / TERRAIN_SIZE;
	u_int uNumSplitsZ = uImageHeight / TERRAIN_SIZE;
	if (pxRect != nullptr && (!pxRect->IsValid() ||
		static_cast<u_int>(pxRect->GetMaxX()) >= uNumSplitsX ||
		static_cast<u_int>(pxRect->GetMaxY()) >= uNumSplitsZ))
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"Terrain export rectangle exceeds the heightmap chunk grid (%ux%u)",
			uNumSplitsX, uNumSplitsZ);
		return false;
	}

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xFullMesh, uDensityDivisor);

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
	xChunkData.fDensity = fDensity;
	xChunkData.strOutputDir = strOutputDir;
	xChunkData.strName = strName;

	u_int uNumInvocations = std::min(static_cast<u_int>(64), uTotalChunks);
	Zenith_DataParallelTask xChunkTask(ZENITH_PROFILE_ZONE("Flux Terrain"), ExportChunkBatch, &xChunkData, uNumInvocations, true);
	g_xEngine.Tasks().SubmitDataParallelTask(&xChunkTask);
	xChunkTask.WaitUntilComplete();
	return true;
}

void ExportMesh(u_int uDensityDivisor, std::string strName,
	const Zenith_Image& xHeightmap, const std::string& strOutputDir)
{
	(void)ExportMeshInternal(uDensityDivisor, strName, xHeightmap, strOutputDir, nullptr);
}

static void ExportHeightmapInternal(const Zenith_Image& xHeightmap, const std::string& strOutputDir)
{
	Zenith_Assert(!xHeightmap.IsEmpty(), "Invalid heightmap");

	// Export HIGH detail render meshes (density divisor 1, streamed dynamically)
	ExportMesh(1, "Render", xHeightmap, strOutputDir);

	// Export LOW detail render meshes (density divisor 4, always resident)
	ExportMesh(4, "Render_LOW", xHeightmap, strOutputDir);

	// Export physics mesh (density divisor 4). This remains lower-poly than the
	// HIGH render mesh, but gives characters a four-metre collision surface rather
	// than the visibly coarse eight-metre one.
	ExportMesh(4, "Physics", xHeightmap, strOutputDir);
}

static bool ExportHeightmapRectInternal(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Flux_TerrainExportRect& xRect)
{
	if (xHeightmap.IsEmpty() || !xRect.IsValid())
	{
		Zenith_Warning(LOG_CATEGORY_TOOLS,
			"Rect terrain export requires a valid heightmap and export rectangle");
		return false;
	}

	return ExportMeshInternal(1, "Render", xHeightmap, strOutputDir, &xRect) &&
		ExportMeshInternal(4, "Render_LOW", xHeightmap, strOutputDir, &xRect) &&
		ExportMeshInternal(4, "Physics", xHeightmap, strOutputDir, &xRect);
}

void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Heightmap=%s, Output=%s",
		strHeightmapPath.c_str(), strOutputDir.c_str());

	Zenith_Image xHeightmap = LoadHeightmapAuto(strHeightmapPath);
	ExportHeightmapInternal(xHeightmap, strOutputDir);

	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Export complete");
}

void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Output=%s", strOutputDir.c_str());
	ExportHeightmapInternal(xHeightmap, strOutputDir);
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Export complete");
}

bool ExportHeightmapFromMatRect(const Zenith_Image& xHeightmap,
	const std::string& strOutputDir, const Flux_TerrainExportRect& xRect)
{
	Zenith_Log(LOG_CATEGORY_TOOLS,
		"ExportHeightmapFromMatRect: Output=%s Bounds=[%d,%d]-[%d,%d] Chunks=%u Files=%u",
		strOutputDir.c_str(), xRect.GetMinX(), xRect.GetMinY(),
		xRect.GetMaxX(), xRect.GetMaxY(), xRect.ChunkCount(), xRect.ChunkCount() * 3);
	const bool bExported = ExportHeightmapRectInternal(xHeightmap, strOutputDir, xRect);
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMatRect: Export %s",
		bExported ? "complete" : "failed");
	return bExported;
}
