#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Enums.h"
#include "FileAccess/Zenith_FileAccess.h"

// Extern function that must be implemented by game projects - returns just the project name (e.g., "Test")
// Paths are constructed using ZENITH_ROOT (defined by build system) + project name
extern const char* Project_GetName();

// Helper function to construct game assets path from project name
static std::string GetGameAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/";
}
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <cstring>
#include <cmath>

#include "TaskSystem/Zenith_TaskSystem.h"

#define MAX_TERRAIN_HEIGHT 4096

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

// Convert float32 to float16 (IEEE 754 half-precision)
static uint16_t FloatToHalf(float fValue)
{
	uint32_t uBits;
	memcpy(&uBits, &fValue, sizeof(uint32_t));

	uint32_t uSign = (uBits >> 16) & 0x8000;
	int32_t iExponent = static_cast<int32_t>((uBits >> 23) & 0xFF) - 127;
	uint32_t uMantissa = uBits & 0x7FFFFF;

	// Handle special cases
	if (iExponent == 128) // Inf/NaN
		return static_cast<uint16_t>(uSign | 0x7C00 | (uMantissa ? 0x200 : 0));
	if (iExponent < -24) // Too small, underflow to zero
		return static_cast<uint16_t>(uSign);

	int32_t iHalfExp = iExponent + 15;
	if (iHalfExp <= 0)
	{
		// Denormalized half
		uMantissa = (uMantissa | 0x800000) >> (1 - iHalfExp);
		return static_cast<uint16_t>(uSign | (uMantissa >> 13));
	}
	if (iHalfExp >= 31) // Overflow to infinity
		return static_cast<uint16_t>(uSign | 0x7C00);

	return static_cast<uint16_t>(uSign | (iHalfExp << 10) | (uMantissa >> 13));
}

// Generate packed terrain vertex data (24 bytes/vertex)
// Layout: Position(FLOAT3,12) + UV(HALF2,4) + Normal(SNORM10,4) + Tangent+Sign(SNORM10,4)
static void GenerateTerrainLayoutAndVertexData(Flux_MeshGeometry& xMesh)
{
	xMesh.m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
	xMesh.m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_HALF2 });
	xMesh.m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_SNORM10_10_10_2 });
	xMesh.m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_SNORM10_10_10_2 });
	xMesh.m_xBufferLayout.CalculateOffsetsAndStrides();

	const uint32_t uStride = xMesh.m_xBufferLayout.GetStride();
	Zenith_Assert(uStride == 24, "Terrain vertex stride should be 24 bytes");

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
		uOffset += 12;

		// UV: half2 (4 bytes)
		uint16_t* pUV = reinterpret_cast<uint16_t*>(pVertex + uOffset);
		pUV[0] = FloatToHalf(xMesh.m_pxUVs[i].x);
		pUV[1] = FloatToHalf(xMesh.m_pxUVs[i].y);
		uOffset += 4;

		// Normal: SNORM10:10:10:2 (4 bytes), w=0
		uint32_t* pNormal = reinterpret_cast<uint32_t*>(pVertex + uOffset);
		*pNormal = PackSNORM10_10_10_2(
			xMesh.m_pxNormals[i].x,
			xMesh.m_pxNormals[i].y,
			xMesh.m_pxNormals[i].z,
			0.0f
		);
		uOffset += 4;

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
// Load heightmap from .ztxtr file and return as cv::Mat in CV_32FC1 format
//-----------------------------------------------------------------------------
static cv::Mat LoadHeightmapFromZtxtr(const std::string& strPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load .ztxtr file: %s", strPath.c_str());
		return cv::Mat();
	}

	int32_t iWidth, iHeight, iDepth;
	TextureFormat eFormat;
	uint64_t ulDataSize;

	xStream >> iWidth;
	xStream >> iHeight;
	xStream >> iDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	Zenith_Log(LOG_CATEGORY_TOOLS, "Loading .ztxtr heightmap: %dx%d, format=%d, size=%llu",
		iWidth, iHeight, static_cast<int>(eFormat), ulDataSize);

	// Allocate and read data
	void* pData = Zenith_MemoryManagement::Allocate(ulDataSize);
	xStream.ReadData(pData, ulDataSize);

	// Create cv::Mat based on format
	cv::Mat xResult;
	if (eFormat == TEXTURE_FORMAT_R32_SFLOAT)
	{
		// 32-bit float single channel - use directly
		cv::Mat xTemp(iHeight, iWidth, CV_32FC1, pData);
		xResult = xTemp.clone();
	}
	else if (eFormat == TEXTURE_FORMAT_R16_UNORM)
	{
		// 16-bit unsigned single channel - convert to float normalized [0,1]
		cv::Mat xTemp(iHeight, iWidth, CV_16UC1, pData);
		xTemp.convertTo(xResult, CV_32FC1, 1.0 / 65535.0);
	}
	else if (eFormat == TEXTURE_FORMAT_RGBA8_UNORM)
	{
		// RGBA8 - use red channel, convert to float
		cv::Mat xTemp(iHeight, iWidth, CV_8UC4, pData);
		std::vector<cv::Mat> xChannels;
		cv::split(xTemp, xChannels);
		xChannels[0].convertTo(xResult, CV_32FC1, 1.0 / 255.0);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "Unsupported texture format for heightmap: %d", static_cast<int>(eFormat));
		Zenith_MemoryManagement::Deallocate(pData);
		return cv::Mat();
	}

	Zenith_MemoryManagement::Deallocate(pData);
	return xResult;
}

//-----------------------------------------------------------------------------
// Load heightmap from either .ztxtr or .tif based on file extension
//-----------------------------------------------------------------------------
static cv::Mat LoadHeightmapAuto(const std::string& strPath)
{
	// Get file extension
	std::string strExt = strPath.substr(strPath.rfind('.'));

	if (strExt == ZENITH_TEXTURE_EXT)
	{
		return LoadHeightmapFromZtxtr(strPath);
	}
	else
	{
		// Use OpenCV for .tif and other formats
		cv::Mat xImage = cv::imread(strPath, cv::IMREAD_ANYDEPTH);
		if (xImage.empty())
		{
			Zenith_Log(LOG_CATEGORY_TOOLS, "Failed to load heightmap: %s", strPath.c_str());
			return cv::Mat();
		}

		// Convert to 32-bit float if needed
		if (xImage.depth() != CV_32F)
		{
			cv::Mat xConverted;
			if (xImage.depth() == CV_16U)
			{
				xImage.convertTo(xConverted, CV_32FC1, 1.0 / 65535.0);
			}
			else if (xImage.depth() == CV_8U)
			{
				xImage.convertTo(xConverted, CV_32FC1, 1.0 / 255.0);
			}
			else
			{
				xImage.convertTo(xConverted, CV_32FC1);
			}
			return xConverted;
		}
		return xImage;
	}
}

//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 1

void GenerateFullTerrain(const cv::Mat& xHeightmapImage, Flux_MeshGeometry& xMesh, u_int uDensityDivisor)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	u_int uWidth = xHeightmapImage.cols;
	u_int uHeight = xHeightmapImage.rows;

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
				float fTopLeft = xHeightmapImage.at<cv::Vec<float, 1>>(y0, x0).val[0];
				float fTopRight = xHeightmapImage.at<cv::Vec<float, 1>>(y0, x1).val[0];
				float fBottomLeft = xHeightmapImage.at<cv::Vec<float, 1>>(y1, x0).val[0];
				float fBottomRight = xHeightmapImage.at<cv::Vec<float, 1>>(y1, x1).val[0];

				double dWeightX = xUV.x - x0;
				double dWeightY = xUV.y - y0;

				double dTop = fTopRight * dWeightX + fTopLeft * (1.f - dWeightX);
				double dBottom = fBottomRight * dWeightX + fBottomLeft * (1.f - dWeightX);

				dHeight = dBottom * dWeightY + dTop * (1.f - dWeightY);
			}

			xMesh.m_pxPositions[offset] = glm::highp_vec3((double)x / fDensity, dHeight * MAX_TERRAIN_HEIGHT - 1000, (double)z / fDensity) * static_cast<float>(TERRAIN_SCALE);
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
	float fDensity;
	u_int uImageWidth;
	std::string strOutputDir;
	std::string strName;
};

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
	const u_int x = uChunkIndex % uNumSplitsX;
	const u_int z = uChunkIndex / uNumSplitsX;

	Flux_MeshGeometry xSubMesh;
	xSubMesh.m_uNumVerts = static_cast<u_int>((TERRAIN_SIZE * fDensity + 1) * (TERRAIN_SIZE * fDensity + 1));
	xSubMesh.m_uNumIndices = static_cast<u_int>((((TERRAIN_SIZE * fDensity + 1)) - 1) * (((TERRAIN_SIZE * fDensity + 1)) - 1) * 6);
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

void ExportMesh(u_int uDensityDivisor, std::string strName, const cv::Mat& xHeightmap, const std::string& strOutputDir)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor-1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	Zenith_Assert(!xHeightmap.empty(), "Invalid heightmap image");

	u_int uImageWidth = xHeightmap.cols;
	u_int uImageHeight = xHeightmap.rows;

	Zenith_Assert(static_cast<u_int>(uImageWidth * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain width");
	Zenith_Assert(static_cast<u_int>(uImageHeight * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain height");

	u_int uNumSplitsX = uImageWidth / TERRAIN_SIZE;
	u_int uNumSplitsZ = uImageHeight / TERRAIN_SIZE;

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xFullMesh, uDensityDivisor);

	u_int uTotalChunks = uNumSplitsX * uNumSplitsZ;

	ChunkExportData xChunkData;
	xChunkData.pxFullMesh = &xFullMesh;
	xChunkData.uNumSplitsX = uNumSplitsX;
	xChunkData.uNumSplitsZ = uNumSplitsZ;
	xChunkData.uTotalChunks = uTotalChunks;
	xChunkData.fDensity = fDensity;
	xChunkData.uImageWidth = uImageWidth;
	xChunkData.strOutputDir = strOutputDir;
	xChunkData.strName = strName;

	u_int uNumInvocations = std::min(static_cast<u_int>(64), uTotalChunks);
	Zenith_TaskArray xChunkTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, ExportChunkBatch, &xChunkData, uNumInvocations, true);
	Zenith_TaskSystem::SubmitTaskArray(&xChunkTask);
	xChunkTask.WaitUntilComplete();
}

static void ExportHeightmapInternal(const cv::Mat& xHeightmap, const std::string& strOutputDir)
{
	Zenith_Assert(!xHeightmap.empty(), "Invalid heightmap");

	// Export HIGH detail render meshes (density divisor 1, streamed dynamically)
	ExportMesh(1, "Render", xHeightmap, strOutputDir);

	// Export LOW detail render meshes (density divisor 4, always resident)
	ExportMesh(4, "Render_LOW", xHeightmap, strOutputDir);

	// Export physics mesh (density divisor 8)
	ExportMesh(8, "Physics", xHeightmap, strOutputDir);
}

void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Heightmap=%s, Output=%s",
		strHeightmapPath.c_str(), strOutputDir.c_str());

	cv::Mat xHeightmap = LoadHeightmapAuto(strHeightmapPath);
	ExportHeightmapInternal(xHeightmap, strOutputDir);

	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Export complete");
}

void ExportHeightmapFromMat(const cv::Mat& xHeightmap, const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Output=%s", strOutputDir.c_str());
	ExportHeightmapInternal(xHeightmap, strOutputDir);
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromMat: Export complete");
}

void ExportHeightmap()
{
	// Use default paths for backward compatibility
	std::string strAssetsDir = GetGameAssetsDirectory();
	std::string strHeightmapPath = strAssetsDir + "Textures/Heightmaps/Test/gaeaHeight.tif";
	std::string strOutputDir = strAssetsDir + "Terrain/";

	ExportHeightmapFromPaths(strHeightmapPath, strOutputDir);
}