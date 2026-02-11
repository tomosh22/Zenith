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

#define MAX_TERRAIN_HEIGHT 4096

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

void GenerateFullTerrain(cv::Mat& xHeightmapImage, cv::Mat& xMaterialImage, Flux_MeshGeometry& xMesh, u_int uDensityDivisor)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	u_int uWidth = xHeightmapImage.cols;
	u_int uHeight = xHeightmapImage.rows;

	xMesh.m_uNumVerts = static_cast<u_int>(uWidth * uHeight * fDensity * fDensity);
	xMesh.m_uNumIndices = static_cast<u_int>(((uWidth * fDensity) - 1) * ((uHeight * fDensity) - 1) * 6);
	xMesh.m_pxPositions = new glm::highp_vec3[xMesh.m_uNumVerts];
	xMesh.m_pxUVs = new glm::vec2[xMesh.m_uNumVerts];
	xMesh.m_pxNormals = new glm::vec3[xMesh.m_uNumVerts];
	xMesh.m_pxTangents = new glm::vec3[xMesh.m_uNumVerts];
	xMesh.m_pxBitangents = new glm::vec3[xMesh.m_uNumVerts];
	xMesh.m_pfMaterialLerps = new float[xMesh.m_uNumIndices];
	for (size_t i = 0; i < xMesh.m_uNumVerts; i++)
	{
		xMesh.m_pxPositions[i] = { 0,0,0 };
		xMesh.m_pxUVs[i] = { 0,0 };
		xMesh.m_pxNormals[i] = { 0,0,0 };
		xMesh.m_pxTangents[i] = { 0,0,0 };
		xMesh.m_pxBitangents[i] = { 0,0,0 };
		xMesh.m_pfMaterialLerps[i] = 0;
	}
	xMesh.m_puIndices = new Flux_MeshGeometry::IndexType[xMesh.m_uNumIndices];

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

			double dMaterialLerp;
			{
				float fTopLeft = xMaterialImage.at<cv::Vec<float, 1>>(y0, x0).val[0];
				float fTopRight = xMaterialImage.at<cv::Vec<float, 1>>(y0, x1).val[0];
				float fBottomLeft = xMaterialImage.at<cv::Vec<float, 1>>(y1, x0).val[0];
				float fBottomRight = xMaterialImage.at<cv::Vec<float, 1>>(y1, x1).val[0];

				double dWeightX = xUV.x - x0;
				double dWeightY = xUV.y - y0;

				double dTop = fTopRight * dWeightX + fTopLeft * (1.f - dWeightX);
				double dBottom = fBottomRight * dWeightX + fBottomLeft * (1.f - dWeightX);

				dMaterialLerp = dBottom * dWeightY + dTop * (1.f - dWeightY);
			}

			xMesh.m_pxPositions[offset] = glm::highp_vec3((double)x / fDensity, dHeight * MAX_TERRAIN_HEIGHT - 1000, (double)z / fDensity) * static_cast<float>(TERRAIN_SCALE);
			glm::vec2 fUV = glm::vec2(x, z);
			xMesh.m_pxUVs[offset] = fUV / fDensity;
			xMesh.m_pfMaterialLerps[offset] = static_cast<float>(dMaterialLerp);
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

void ExportMesh(u_int uDensityDivisor, std::string strName, const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor-1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	// Load heightmap and material textures (supports both .ztxtr and .tif)
	cv::Mat xHeightmap = LoadHeightmapAuto(strHeightmapPath);
	cv::Mat xMaterialLerpMap = LoadHeightmapAuto(strMaterialPath);

	Zenith_Assert(!xHeightmap.empty(), "Invalid heightmap image");

	u_int uImageWidth = xHeightmap.cols;
	u_int uImageHeight = xHeightmap.rows;

#if 0
	GUID xFoliageMaterialGUID;
	GUID xFoliageAlbedoGUID;
	GUID xFoliageNormalGUID;
	GUID xFoliageRoughnessGUID;
	GUID xFoliageHeightmapGUID;
	GUID xFoliageAlphaGUID;
	GUID xFoliageTranslucencyGUID;
	WriteFoliageMaterialAsset(xFoliageMaterialGUID, xFoliageAlbedoGUID, xFoliageNormalGUID, xFoliageRoughnessGUID, xFoliageHeightmapGUID, xFoliageAlphaGUID, xFoliageTranslucencyGUID);
#endif

	Zenith_Assert(static_cast<u_int>(uImageWidth * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain width");
	Zenith_Assert(static_cast<u_int>(uImageHeight * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain height");

	u_int uNumSplitsX = uImageWidth / TERRAIN_SIZE;
	u_int uNumSplitsZ = uImageHeight / TERRAIN_SIZE;

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xMaterialLerpMap, xFullMesh, uDensityDivisor);

	for (u_int z = 0; z < uNumSplitsZ; z++)
	{
		for (u_int x = 0; x < uNumSplitsX; x++)
		{
			Flux_MeshGeometry xSubMesh;
			xSubMesh.m_uNumVerts = static_cast<u_int>((TERRAIN_SIZE * fDensity + 1) * (TERRAIN_SIZE * fDensity + 1));
			xSubMesh.m_uNumIndices = static_cast<u_int>((((TERRAIN_SIZE * fDensity + 1)) - 1) * (((TERRAIN_SIZE * fDensity + 1)) - 1) * 6);
			xSubMesh.m_pxPositions = new glm::highp_vec3[xSubMesh.m_uNumVerts];
			xSubMesh.m_pxUVs = new glm::vec2[xSubMesh.m_uNumVerts];
			xSubMesh.m_pxNormals = new glm::vec3[xSubMesh.m_uNumVerts];
			xSubMesh.m_pxTangents = new glm::vec3[xSubMesh.m_uNumVerts];
			xSubMesh.m_pxBitangents = new glm::vec3[xSubMesh.m_uNumVerts];
			for (size_t i = 0; i < xSubMesh.m_uNumVerts; i++)
			{
				xSubMesh.m_pxNormals[i] = { 0,0,0 };
				xSubMesh.m_pxTangents[i] = { 0,0,0 };
			}
			xSubMesh.m_puIndices = new u_int[xSubMesh.m_uNumIndices]{ 0 };
			xSubMesh.m_pfMaterialLerps = new float[xSubMesh.m_uNumIndices];

			u_int uHeighestNewOffset = 0;
#ifdef ZENITH_ASSERT
			std::set<u_int> xFoundOldIndices;
			std::set<u_int> xFoundNewIndices;
#endif
			u_int* puRightEdgeIndices = new u_int[static_cast<u_int>(TERRAIN_SIZE * fDensity)];
			u_int* puTopEdgeIndices = new u_int[static_cast<u_int>(TERRAIN_SIZE * fDensity)];
			u_int uTopRightFromBoth = 0;

			std::vector<glm::vec3> xFoliagePositions;

			glm::highp_vec3 xOrigin = { x, 0, z };
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
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

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

					if (rand() / static_cast<float>(RAND_MAX) < 0.2f && glm::dot(xFullMesh.m_pxNormals[uOldOffset], { 0,1,0 }) > 0.95f)
						xFoliagePositions.push_back(xFullMesh.m_pxPositions[uOldOffset]);
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
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

					if (subZ == static_cast<u_int>(TERRAIN_SIZE * fDensity - 1))
						uTopRightFromX = uNewOffset;

					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
					//xPositionsSet.insert(pxSubMesh->m_pxVertexPositions[uNewOffset]);
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
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

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
				xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

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

			xSubMesh.GenerateLayoutAndVertexData();
			xSubMesh.Export((strOutputDir + strName + std::string("_") + std::to_string(x) + std::string("_") + std::to_string(z) + std::string(ZENITH_MESH_EXT)).c_str());

			delete[] puRightEdgeIndices;
			delete[] puTopEdgeIndices;
		}
	}
}

void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir)
{
	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Heightmap=%s, Material=%s, Output=%s",
		strHeightmapPath.c_str(), strMaterialPath.c_str(), strOutputDir.c_str());

	// Export HIGH detail render meshes (density divisor 1, streamed dynamically)
	ExportMesh(1, "Render", strHeightmapPath, strMaterialPath, strOutputDir);

	// Export LOW detail render meshes (density divisor 4, always resident)
	ExportMesh(4, "Render_LOW", strHeightmapPath, strMaterialPath, strOutputDir);

	// Export physics mesh (density divisor 8)
	ExportMesh(8, "Physics", strHeightmapPath, strMaterialPath, strOutputDir);

	Zenith_Log(LOG_CATEGORY_TOOLS, "ExportHeightmapFromPaths: Export complete");
}

void ExportHeightmap()
{
	// Use default paths for backward compatibility
	std::string strAssetsDir = GetGameAssetsDirectory();
	std::string strHeightmapPath = strAssetsDir + "Textures/Heightmaps/Test/gaeaHeight.tif";
	std::string strMaterialPath = strAssetsDir + "Textures/Heightmaps/Test/gaeaMaterial.tif";
	std::string strOutputDir = strAssetsDir + "Terrain/";

	ExportHeightmapFromPaths(strHeightmapPath, strMaterialPath, strOutputDir);
}