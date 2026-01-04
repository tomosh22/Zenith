#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

// Extern function that must be implemented by game projects - returns just the project name (e.g., "Test")
// Paths are constructed using ZENITH_ROOT (defined by build system) + project name
extern const char* Project_GetName();

// Helper function to construct game assets path from project name
static std::string GetGameAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/";
}
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <opencv2/opencv.hpp>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#define MAX_TERRAIN_HEIGHT 4096

//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 1

void GenerateFullTerrain(cv::Mat& xHeightmapImage, cv::Mat& xMaterialImage, Flux_MeshGeometry& xMesh, u_int uDensityDivisor)
{
	Zenith_Assert((uDensityDivisor & (uDensityDivisor - 1)) == 0, "Density divisor must be a power of 2");

	float fDensity = 1.f / uDensityDivisor;

	uint32_t uWidth = xHeightmapImage.cols;
	uint32_t uHeight = xHeightmapImage.rows;

	xMesh.m_uNumVerts = uWidth * uHeight * fDensity * fDensity;
	xMesh.m_uNumIndices = ((uWidth * fDensity) - 1) * ((uHeight * fDensity) - 1) * 6;
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

	for (uint32_t z = 0; z < uHeight * fDensity; ++z)
	{
		for (uint32_t x = 0; x < uWidth * fDensity; ++x)
		{
			glm::vec2 xUV = { (double)x / fDensity , (double)z / fDensity };
			uint32_t offset = (z * uWidth * fDensity) + x;

			uint32_t x0 = std::floor(xUV.x);
			uint32_t x1 = std::min((uint32_t)std::ceil(xUV.x), uWidth - 1);
			uint32_t y0 = std::floor(xUV.y);
			uint32_t y1 = std::min((uint32_t)std::ceil(xUV.y), uHeight - 1);

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
			xMesh.m_pfMaterialLerps[offset] = dMaterialLerp;
		}
	}

	size_t i = 0;
	for (int z = 0; z < (uHeight * fDensity) - 1; ++z)
	{
		for (int x = 0; x < (uWidth * fDensity) - 1; ++x)
		{
			int a = (z * (uWidth * fDensity)) + x;
			int b = (z * (uWidth * fDensity)) + x + 1;
			int c = ((z + 1) * (uWidth * fDensity)) + x + 1;
			int d = ((z + 1) * (uWidth * fDensity)) + x;
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

	cv::Mat xHeightmap = cv::imread(strHeightmapPath, cv::IMREAD_ANYDEPTH);
	cv::Mat xMaterialLerpMap = cv::imread(strMaterialPath, cv::IMREAD_ANYDEPTH);

	Zenith_Assert(!xHeightmap.empty(), "Invalid image");

	uint32_t uImageWidth = xHeightmap.cols;
	uint32_t uImageHeight = xHeightmap.rows;

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

	Zenith_Assert(uint32_t(uImageWidth * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain width");
	Zenith_Assert(uint32_t(uImageHeight * fDensity) % TERRAIN_SIZE == 0, "Invalid terrain height");

	uint32_t uNumSplitsX = uImageWidth / TERRAIN_SIZE;
	uint32_t uNumSplitsZ = uImageHeight / TERRAIN_SIZE;

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xMaterialLerpMap, xFullMesh, uDensityDivisor);

	for (uint32_t z = 0; z < uNumSplitsZ; z++)
	{
		for (uint32_t x = 0; x < uNumSplitsX; x++)
		{
			Flux_MeshGeometry xSubMesh;
			xSubMesh.m_uNumVerts = (TERRAIN_SIZE * fDensity + 1) * (TERRAIN_SIZE * fDensity + 1);
			xSubMesh.m_uNumIndices = (((TERRAIN_SIZE * fDensity + 1)) - 1) * (((TERRAIN_SIZE * fDensity + 1)) - 1) * 6;
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
			xSubMesh.m_puIndices = new uint32_t[xSubMesh.m_uNumIndices]{ 0 };
			xSubMesh.m_pfMaterialLerps = new float[xSubMesh.m_uNumIndices];

			uint32_t uHeighestNewOffset = 0;
#ifdef ZENITH_ASSERT
			std::set<uint32_t> xFoundOldIndices;
			std::set<uint32_t> xFoundNewIndices;
#endif
			uint32_t* puRightEdgeIndices = new uint32_t[TERRAIN_SIZE * fDensity];
			uint32_t* puTopEdgeIndices = new uint32_t[TERRAIN_SIZE * fDensity];
			uint32_t uTopRightFromBoth = 0;

			std::vector<glm::vec3> xFoliagePositions;

			glm::highp_vec3 xOrigin = { x, 0, z };
			for (uint32_t subZ = 0; subZ < TERRAIN_SIZE * fDensity; subZ++)
			{
				for (uint32_t subX = 0; subX < TERRAIN_SIZE * fDensity; subX++)
				{
					uint32_t uNewOffset = (subZ * TERRAIN_SIZE * fDensity) + subX;

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * fDensity;
					Zenith_Assert(uIndexIntoRow < fDensity * uImageWidth, "Gone past end of row");
					uint32_t uOldOffset = uStartOfRow + uIndexIntoRow;

					Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

					Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
					Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

					xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
					xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
					xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
					xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
					xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

					if (subX == TERRAIN_SIZE * fDensity - 1)
						puRightEdgeIndices[subZ] = uNewOffset;
					if (subZ == TERRAIN_SIZE * fDensity - 1)
						puTopEdgeIndices[subX] = uNewOffset;
					if (subX == TERRAIN_SIZE * fDensity - 1 && subZ == TERRAIN_SIZE * fDensity - 1)
						uTopRightFromBoth = uNewOffset;

#ifdef ZENITH_ASSERT
					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
#endif

					if (rand() / (float)RAND_MAX < 0.2f && glm::dot(xFullMesh.m_pxNormals[uOldOffset], { 0,1,0 }) > 0.95f)
						xFoliagePositions.push_back(xFullMesh.m_pxPositions[uOldOffset]);
				}
			}

			size_t indexIndex = 0;
			for (uint32_t indexZ = 0; indexZ < TERRAIN_SIZE * fDensity - 1; indexZ++)
			{
				for (uint32_t indexX = 0; indexX < TERRAIN_SIZE * fDensity - 1; indexX++)
				{
					uint32_t a = (indexZ * TERRAIN_SIZE * fDensity) + indexX;
					uint32_t b = (indexZ * TERRAIN_SIZE * fDensity) + indexX + 1;
					uint32_t c = ((indexZ + 1) * TERRAIN_SIZE * fDensity) + indexX + 1;
					uint32_t d = ((indexZ + 1) * TERRAIN_SIZE * fDensity) + indexX;
					xSubMesh.m_puIndices[indexIndex++] = a;
					xSubMesh.m_puIndices[indexIndex++] = c;
					xSubMesh.m_puIndices[indexIndex++] = b;
					xSubMesh.m_puIndices[indexIndex++] = c;
					xSubMesh.m_puIndices[indexIndex++] = a;
					xSubMesh.m_puIndices[indexIndex++] = d;
					Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
				}
			}

			uint32_t uTopRightFromX = 0;
			if (x < uNumSplitsX - 1)
			{
				uint32_t subX = TERRAIN_SIZE * fDensity;
				for (uint32_t subZ = 0; subZ < TERRAIN_SIZE * fDensity; subZ++)
				{
					uint32_t uNewOffset = ++uHeighestNewOffset;

					Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * fDensity;
					Zenith_Assert(uIndexIntoRow < fDensity * uImageWidth, "Gone past end of row");
					uint32_t uOldOffset = uStartOfRow + uIndexIntoRow;

					Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

					Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
					Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

					xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
					xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
					xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
					xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
					xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

					if (subZ == TERRAIN_SIZE * fDensity - 1)
						uTopRightFromX = uNewOffset;

					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
					//xPositionsSet.insert(pxSubMesh->m_pxVertexPositions[uNewOffset]);
#endif
				}

				uHeighestNewOffset -= TERRAIN_SIZE * fDensity - 1;

				uint32_t indexX = TERRAIN_SIZE * fDensity - 1;
				for (uint32_t indexZ = 0; indexZ < TERRAIN_SIZE * fDensity - 1; indexZ++)
				{
					uint32_t a = puRightEdgeIndices[indexZ + 1];
					uint32_t c = uHeighestNewOffset++;
					uint32_t b = puRightEdgeIndices[indexZ];
					uint32_t d = uHeighestNewOffset;

					xSubMesh.m_puIndices[indexIndex++] = a;
					xSubMesh.m_puIndices[indexIndex++] = c;
					xSubMesh.m_puIndices[indexIndex++] = b;
					xSubMesh.m_puIndices[indexIndex++] = c;
					xSubMesh.m_puIndices[indexIndex++] = a;
					xSubMesh.m_puIndices[indexIndex++] = d;
					Zenith_Assert(indexIndex <= xSubMesh.m_uNumIndices, "Index index too big");
				}
			}

			uint32_t uTopRightFromZ = 0;

			if (z < uNumSplitsZ - 1)
			{
				uint32_t subZ = TERRAIN_SIZE * fDensity;
				for (uint32_t subX = 0; subX < TERRAIN_SIZE * fDensity; subX++)
				{
					uint32_t uNewOffset = ++uHeighestNewOffset;

					Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * fDensity;
					Zenith_Assert(uIndexIntoRow < fDensity* uImageWidth, "Gone past end of row");
					uint32_t uOldOffset = uStartOfRow + uIndexIntoRow;

					Zenith_Assert(uOldOffset < xFullMesh.m_uNumVerts, "Incorrect index somewhere");

					Zenith_Assert(xFoundOldIndices.find(uOldOffset) == xFoundOldIndices.end(), "Duplicate old index");
					Zenith_Assert(xFoundNewIndices.find(uNewOffset) == xFoundNewIndices.end(), "Duplicate new index");

					xSubMesh.m_pxPositions[uNewOffset] = xFullMesh.m_pxPositions[uOldOffset];
					xSubMesh.m_pxUVs[uNewOffset] = xFullMesh.m_pxUVs[uOldOffset];
					xSubMesh.m_pxNormals[uNewOffset] = xFullMesh.m_pxNormals[uOldOffset];
					xSubMesh.m_pxTangents[uNewOffset] = xFullMesh.m_pxTangents[uOldOffset];
					xSubMesh.m_pxBitangents[uNewOffset] = xFullMesh.m_pxBitangents[uOldOffset];
					xSubMesh.m_pfMaterialLerps[uNewOffset] = xFullMesh.m_pfMaterialLerps[uOldOffset];

					if (subX == TERRAIN_SIZE * fDensity - 1)
						uTopRightFromZ = uNewOffset;

					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
#endif
				}

				uHeighestNewOffset -= TERRAIN_SIZE * fDensity - 1;

				uint32_t indexZ = TERRAIN_SIZE * fDensity - 1;
				for (uint32_t indexX = 0; indexX < TERRAIN_SIZE * fDensity - 1; indexX++)
				{
					uint32_t c = puTopEdgeIndices[indexX + 1];
					uint32_t a = uHeighestNewOffset++;
					uint32_t b = puTopEdgeIndices[indexX];
					uint32_t d = uHeighestNewOffset;

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
				uint32_t subZ = TERRAIN_SIZE * fDensity;
				uint32_t subX = TERRAIN_SIZE * fDensity;
				uint32_t uNewOffset = ++uHeighestNewOffset;

				Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

				uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * fDensity * uNumSplitsZ) + (z * uImageWidth * fDensity * TERRAIN_SIZE * fDensity);
				Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
				uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * fDensity;
				Zenith_Assert(uIndexIntoRow < fDensity* uImageWidth, "Gone past end of row");
				uint32_t uOldOffset = uStartOfRow + uIndexIntoRow;

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

				uint32_t indexZ = TERRAIN_SIZE * fDensity - 1;
				uint32_t indexX = TERRAIN_SIZE * fDensity - 1;
				uint32_t a = uTopRightFromX;
				uint32_t d = uTopRightFromBoth;
				uint32_t c = uTopRightFromZ;
				uint32_t b = uHeighestNewOffset;

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
	
	// Export full detail render meshes (LOD0)
	ExportMesh(1, "Render", strHeightmapPath, strMaterialPath, strOutputDir);
	
	// Export LOD1 meshes (2x downsampled - skip every other vertex)
	ExportMesh(2, "Render_LOD1", strHeightmapPath, strMaterialPath, strOutputDir);
	
	// Export LOD2 meshes (4x downsampled - skip 3 out of 4 vertices)
	ExportMesh(4, "Render_LOD2", strHeightmapPath, strMaterialPath, strOutputDir);
	
	// Export LOD3 meshes (8x downsampled - skip 7 out of 8 vertices)
	ExportMesh(8, "Render_LOD3", strHeightmapPath, strMaterialPath, strOutputDir);
	
	// Export physics mesh (8x downsampled, already existed)
	// Rename it to avoid confusion
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