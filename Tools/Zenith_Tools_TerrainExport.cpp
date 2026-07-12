#include "Zenith.h"
#include "Zenith_Tools_TerrainExport.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainVertexLayout.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

// Helper function to construct game assets path from project name
static std::string GetGameAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/";
}
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

//-----------------------------------------------------------------------------
// Packing helpers for terrain vertex format optimization
//-----------------------------------------------------------------------------

// Pack 3 floats (xyz) + 1 float (w) into A2B10G10R10 SNORM format (4 bytes)
// R,G,B = 10-bit signed normalized [-1,1], A = 2-bit signed normalized [-1,1]
static uint32_t PackSNORM10_10_10_2(float fX, float fY, float fZ, float fW)
{
	auto Clamp = [](float f) { return std::max(-1.0f, std::min(1.0f, f)); };
	fX = Clamp(fX);
	fY = Clamp(fY);
	fZ = Clamp(fZ);
	fW = Clamp(fW);

	// 10-bit snorm: [-1.0, 1.0] -> [-511, 511]
	int32_t iR = static_cast<int32_t>(std::round(fX * 511.0f));
	int32_t iG = static_cast<int32_t>(std::round(fY * 511.0f));
	int32_t iB = static_cast<int32_t>(std::round(fZ * 511.0f));
	// 2-bit snorm: [-1.0, 1.0] -> [-1, 1]
	int32_t iA = static_cast<int32_t>(std::round(fW * 1.0f));

	uint32_t uResult = 0;
	uResult |= (static_cast<uint32_t>(iR) & 0x3FF);
	uResult |= (static_cast<uint32_t>(iG) & 0x3FF) << 10;
	uResult |= (static_cast<uint32_t>(iB) & 0x3FF) << 20;
	uResult |= (static_cast<uint32_t>(iA) & 0x3) << 30;
	return uResult;
}

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
	for (uint32_t uElement = 0; uElement < Flux_TerrainVertexLayout::uELEMENT_COUNT; uElement++)
	{
		xMesh.m_xBufferLayout.GetElements().PushBack(
			{ Flux_TerrainVertexLayout::axELEMENTS[uElement].m_eType });
	}
	xMesh.m_xBufferLayout.CalculateOffsetsAndStrides();

	const uint32_t uCalculatedStride = xMesh.m_xBufferLayout.GetStride();
	Zenith_Assert(uCalculatedStride == Flux_TerrainVertexLayout::uVERTEX_STRIDE,
		"Terrain exporter layout must match the canonical HIGH terrain vertex stride");
	(void)uCalculatedStride;
	const uint32_t uStride = Flux_TerrainVertexLayout::uVERTEX_STRIDE;

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
		uOffset += Flux_TerrainVertexLayout::axELEMENTS[0].m_uSize;

		// UV: float2 (8 bytes) — full 32-bit precision so heightmap-pixel-scale
		// UVs don't collapse on the upper half of the terrain.
		float* pUV = reinterpret_cast<float*>(pVertex + uOffset);
		pUV[0] = xMesh.m_pxUVs[i].x;
		pUV[1] = xMesh.m_pxUVs[i].y;
		uOffset += Flux_TerrainVertexLayout::axELEMENTS[1].m_uSize;

		// Normal: SNORM10:10:10:2 (4 bytes), w=0
		uint32_t* pNormal = reinterpret_cast<uint32_t*>(pVertex + uOffset);
		*pNormal = PackSNORM10_10_10_2(
			xMesh.m_pxNormals[i].x,
			xMesh.m_pxNormals[i].y,
			xMesh.m_pxNormals[i].z,
			0.0f
		);
		uOffset += Flux_TerrainVertexLayout::axELEMENTS[2].m_uSize;

		// Tangent + BitangentSign: SNORM10:10:10:2 (4 bytes)
		float fBitangentSign = glm::dot(
			glm::cross(glm::vec3(xMesh.m_pxNormals[i]), glm::vec3(xMesh.m_pxTangents[i])),
			glm::vec3(xMesh.m_pxBitangents[i])
		) > 0.0f ? 1.0f : -1.0f;

		uint32_t* pTangent = reinterpret_cast<uint32_t*>(pVertex + uOffset);
		*pTangent = PackSNORM10_10_10_2(
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

void GenerateFullTerrain(const Zenith_Image& xHeightmapImage, Flux_MeshGeometry& xMesh, u_int uDensityDivisor)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	u_int uWidth = xHeightmapImage.GetWidth();
	u_int uHeight = xHeightmapImage.GetHeight();

	xMesh.m_uNumVerts = static_cast<u_int>(uWidth * uHeight * fDensity * fDensity);
	xMesh.m_uNumIndices = static_cast<u_int>(((uWidth * fDensity) - 1) * ((uHeight * fDensity) - 1) * 6);
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

	for (u_int z = 0; z < static_cast<u_int>(uHeight * fDensity); ++z)
	{
		for (u_int x = 0; x < static_cast<u_int>(uWidth * fDensity); ++x)
		{
			glm::vec2 xUV = { static_cast<double>(x) / fDensity , static_cast<double>(z) / fDensity };
			u_int offset = static_cast<u_int>((z * uWidth * fDensity) + x);

			u_int x0 = static_cast<u_int>(std::floor(xUV.x));
			u_int x1 = std::min(static_cast<u_int>(std::ceil(xUV.x)), uWidth - 1);
			u_int y0 = static_cast<u_int>(std::floor(xUV.y));
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
	for (int z = 0; z < static_cast<int>((uHeight * fDensity) - 1); ++z)
	{
		for (int x = 0; x < static_cast<int>((uWidth * fDensity) - 1); ++x)
		{
			int a = static_cast<int>((z * (uWidth * fDensity)) + x);
			int b = static_cast<int>((z * (uWidth * fDensity)) + x + 1);
			int c = static_cast<int>(((z + 1) * (uWidth * fDensity)) + x + 1);
			int d = static_cast<int>(((z + 1) * (uWidth * fDensity)) + x);
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
	u_int uImageWidth;
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

	Zenith_Assert(xMesh.m_uNumVerts == Flux_TerrainVertexLayout::uHIGH_CHUNK_VERTEX_COUNT,
		"HIGH terrain exporter vertex count must match the canonical terrain contract");
	Zenith_Assert(xMesh.m_uNumIndices == Flux_TerrainVertexLayout::uHIGH_CHUNK_INDEX_COUNT,
		"HIGH terrain exporter index count must match the canonical terrain contract");
	(void)xMesh;
}

static void ExportChunkBatch(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	const ChunkExportData* pxData = static_cast<const ChunkExportData*>(pData);
	const Flux_MeshGeometry& xFullMesh = *pxData->pxFullMesh;
	const u_int uNumSplitsX = pxData->uNumSplitsX;
	const u_int uNumSplitsZ = pxData->uNumSplitsZ;
	const u_int uTotalChunks = pxData->uTotalChunks;
	const float fDensity = pxData->fDensity;
	const u_int uImageWidth = pxData->uImageWidth;

	u_int uChunksPerInvocation = (uTotalChunks + uNumInvocations - 1) / uNumInvocations;
	u_int uStartChunk = uInvocationIndex * uChunksPerInvocation;
	u_int uEndChunk = std::min(uStartChunk + uChunksPerInvocation, uTotalChunks);

	for (u_int uChunkIndex = uStartChunk; uChunkIndex < uEndChunk; uChunkIndex++)
	{
	u_int x = 0;
	u_int z = 0;
	ResolveChunkExportCoordinates(*pxData, uChunkIndex, x, z);

	Flux_MeshGeometry xSubMesh;
	xSubMesh.m_uNumVerts = static_cast<u_int>((TERRAIN_SIZE * fDensity + 1) * (TERRAIN_SIZE * fDensity + 1));
	xSubMesh.m_uNumIndices = static_cast<u_int>((((TERRAIN_SIZE * fDensity + 1)) - 1) * (((TERRAIN_SIZE * fDensity + 1)) - 1) * 6);
	ValidateHighChunkCounts(fDensity, xSubMesh);
	xSubMesh.m_pxPositions = static_cast<glm::highp_vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::highp_vec3)));
	xSubMesh.m_pxUVs = static_cast<glm::vec2*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec2)));
	xSubMesh.m_pxNormals = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	xSubMesh.m_pxTangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	xSubMesh.m_pxBitangents = static_cast<glm::vec3*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumVerts * sizeof(glm::vec3)));
	for (size_t i = 0; i < xSubMesh.m_uNumVerts; i++)
	{
		xSubMesh.m_pxNormals[i] = { 0,0,0 };
		xSubMesh.m_pxTangents[i] = { 0,0,0 };
	}
	xSubMesh.m_puIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(xSubMesh.m_uNumIndices * sizeof(u_int)));
	memset(xSubMesh.m_puIndices, 0, xSubMesh.m_uNumIndices * sizeof(u_int));

	u_int uHeighestNewOffset = 0;
#ifdef ZENITH_ASSERT
	std::set<u_int> xFoundOldIndices;
	std::set<u_int> xFoundNewIndices;
#endif
	u_int* puRightEdgeIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(static_cast<u_int>(TERRAIN_SIZE * fDensity) * sizeof(u_int)));
	u_int* puTopEdgeIndices = static_cast<u_int*>(Zenith_MemoryManagement::Allocate(static_cast<u_int>(TERRAIN_SIZE * fDensity) * sizeof(u_int)));
	u_int uTopRightFromBoth = 0;

	for (u_int subZ = 0; subZ < static_cast<u_int>(TERRAIN_SIZE * fDensity); subZ++)
	{
		for (u_int subX = 0; subX < static_cast<u_int>(TERRAIN_SIZE * fDensity); subX++)
		{
			u_int uNewOffset = static_cast<u_int>((subZ * TERRAIN_SIZE * fDensity) + subX);

			u_int uStartOfRow = static_cast<u_int>((subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity));
			Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
			u_int uIndexIntoRow = static_cast<u_int>(subX + x * TERRAIN_SIZE * fDensity);
			Zenith_Assert(uIndexIntoRow < fDensity * uImageWidth, "Gone past end of row");
			u_int uOldOffset = uStartOfRow + uIndexIntoRow;

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subX == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
				puRightEdgeIndices[subZ] = uNewOffset;
			if (subZ == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
				puTopEdgeIndices[subX] = uNewOffset;
			if (subX == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1) && subZ == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
				uTopRightFromBoth = uNewOffset;

#ifdef ZENITH_ASSERT
			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}
	}

	size_t indexIndex = 0;
	for (u_int indexZ = 0; indexZ < static_cast<u_int>(TERRAIN_SIZE * fDensity - 1); indexZ++)
	{
		for (u_int indexX = 0; indexX < static_cast<u_int>(TERRAIN_SIZE * fDensity - 1); indexX++)
		{
			u_int a = static_cast<u_int>((indexZ * TERRAIN_SIZE * fDensity) + indexX);
			u_int b = static_cast<u_int>((indexZ * TERRAIN_SIZE * fDensity) + indexX + 1);
			u_int c = static_cast<u_int>(((indexZ + 1) * TERRAIN_SIZE * fDensity) + indexX + 1);
			u_int d = static_cast<u_int>(((indexZ + 1) * TERRAIN_SIZE * fDensity) + indexX);
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = b;
			xSubMesh.m_puIndices[indexIndex++] = c;
			xSubMesh.m_puIndices[indexIndex++] = a;
			xSubMesh.m_puIndices[indexIndex++] = d;
			Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
		}
	}

	u_int uTopRightFromX = 0;
	if (x < uNumSplitsX - 1)
	{
		u_int subX = static_cast<u_int>(TERRAIN_SIZE * fDensity);
		for (u_int subZ = 0; subZ < static_cast<u_int>(TERRAIN_SIZE * fDensity); subZ++)
		{
			u_int uNewOffset = ++uHeighestNewOffset;

			Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

			u_int uStartOfRow = static_cast<u_int>((subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity));
			Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
			u_int uIndexIntoRow = static_cast<u_int>(subX + x * TERRAIN_SIZE * fDensity);
			Zenith_Assert(uIndexIntoRow < fDensity * uImageWidth, "Gone past end of row");
			u_int uOldOffset = uStartOfRow + uIndexIntoRow;

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subZ == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
				uTopRightFromX = uNewOffset;

			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}

		uHeighestNewOffset -= static_cast<u_int>(TERRAIN_SIZE * fDensity - 1);

		for (u_int indexZ = 0; indexZ < static_cast<u_int>(TERRAIN_SIZE * fDensity - 1); indexZ++)
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

	u_int uTopRightFromZ = 0;

	if (z < uNumSplitsZ - 1)
	{
		u_int subZ = static_cast<u_int>(TERRAIN_SIZE * fDensity);
		for (u_int subX = 0; subX < static_cast<u_int>(TERRAIN_SIZE * fDensity); subX++)
		{
			u_int uNewOffset = ++uHeighestNewOffset;

			Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

			u_int uStartOfRow = static_cast<u_int>((subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity));
			Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
			u_int uIndexIntoRow = static_cast<u_int>(subX + x * TERRAIN_SIZE * fDensity);
			Zenith_Assert(uIndexIntoRow < fDensity* uImageWidth, "Gone past end of row");
			u_int uOldOffset = uStartOfRow + uIndexIntoRow;

			Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

			Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
			Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

			xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
			xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
			xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
			xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
			xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];

			if (subX == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
				uTopRightFromZ = uNewOffset;

			uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
			xFoundOldIndices.insert(uOldOffset);
			xFoundNewIndices.insert(uNewOffset);
#endif
		}

		uHeighestNewOffset -= static_cast<u_int>(TERRAIN_SIZE * fDensity - 1);

		for (u_int indexX = 0; indexX < static_cast<u_int>(TERRAIN_SIZE * fDensity - 1); indexX++)
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

	if (x < uNumSplitsX - 1 && z < uNumSplitsZ - 1)
	{
		u_int subZ = static_cast<u_int>(TERRAIN_SIZE * fDensity);
		u_int subX = static_cast<u_int>(TERRAIN_SIZE * fDensity);
		u_int uNewOffset = ++uHeighestNewOffset;

		Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

		u_int uStartOfRow = static_cast<u_int>((subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity));
		Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
		u_int uIndexIntoRow = static_cast<u_int>(subX + x * TERRAIN_SIZE * fDensity);
		Zenith_Assert(uIndexIntoRow < fDensity* uImageWidth, "Gone past end of row");
		u_int uOldOffset = uStartOfRow + uIndexIntoRow;

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
	xChunkData.uImageWidth = uImageWidth;
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

	// Export physics mesh (density divisor 8)
	ExportMesh(8, "Physics", xHeightmap, strOutputDir);
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
		ExportMeshInternal(8, "Physics", xHeightmap, strOutputDir, &xRect);
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

void ExportHeightmap()
{
	// Use default paths for backward compatibility
	std::string strAssetsDir = GetGameAssetsDirectory();
	std::string strHeightmapPath = strAssetsDir + "Textures/Heightmaps/Test/gaeaHeight" ZENITH_TEXTURE_EXT;
	std::string strOutputDir = strAssetsDir + "Terrain/";

	ExportHeightmapFromPaths(strHeightmapPath, strOutputDir);
}
