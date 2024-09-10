#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include <opencv2/opencv.hpp>

#define HEIGHTMAP_MESH_DENSITY 1

#define MAX_TERRAIN_HEIGHT 2048

//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8

void GenerateFullTerrain(cv::Mat& xHeightmapImage, cv::Mat& xMaterialImage, Flux_MeshGeometry& xMesh) {
	uint32_t uWidth = xHeightmapImage.cols;
	uint32_t uHeight = xHeightmapImage.rows;

	xMesh.m_uNumVerts = uWidth * uHeight * HEIGHTMAP_MESH_DENSITY * HEIGHTMAP_MESH_DENSITY;
	xMesh.m_uNumIndices = ((uWidth * HEIGHTMAP_MESH_DENSITY) - 1) * ((uHeight * HEIGHTMAP_MESH_DENSITY) - 1) * 6;
	xMesh.m_pxPositions = new glm::highp_vec3[xMesh.m_uNumVerts];
	xMesh.m_pxUVs = new glm::vec2[xMesh.m_uNumVerts];
	xMesh.m_pxNormals = new glm::vec3[xMesh.m_uNumVerts];
	xMesh.m_pxTangents = new glm::vec3[xMesh.m_uNumVerts];
	xMesh.m_pxBitangents = new glm::vec3[xMesh.m_uNumVerts];
	for (size_t i = 0; i < xMesh.m_uNumVerts; i++)
	{
		xMesh.m_pxNormals[i] = { 0,0,0 };
		xMesh.m_pxTangents[i] = { 0,0,0 };
	}
	xMesh.m_puIndices = new Flux_MeshGeometry::IndexType[xMesh.m_uNumIndices];
	xMesh.m_pfMaterialLerps = new float[xMesh.m_uNumIndices];

	for (uint32_t z = 0; z < uHeight * HEIGHTMAP_MESH_DENSITY; ++z)
	{
		for (uint32_t x = 0; x < uWidth * HEIGHTMAP_MESH_DENSITY; ++x)
		{
			glm::vec2 xUV = { (double)x / HEIGHTMAP_MESH_DENSITY , (double)z / HEIGHTMAP_MESH_DENSITY };
			uint32_t offset = (z * uWidth * HEIGHTMAP_MESH_DENSITY) + x;

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

			xMesh.m_pxPositions[offset] = glm::highp_vec3((double)x / HEIGHTMAP_MESH_DENSITY, dHeight * MAX_TERRAIN_HEIGHT, (double)z / HEIGHTMAP_MESH_DENSITY) * static_cast<float>(TERRAIN_SCALE);
			glm::vec2 fUV = glm::vec2(x, z);
			xMesh.m_pxUVs[offset] = fUV / (float)HEIGHTMAP_MESH_DENSITY;
			xMesh.m_pfMaterialLerps[offset] = dMaterialLerp;
		}
	}

	size_t i = 0;
	for (int z = 0; z < (uHeight * HEIGHTMAP_MESH_DENSITY) - 1; ++z)
	{
		for (int x = 0; x < (uWidth * HEIGHTMAP_MESH_DENSITY) - 1; ++x)
		{
			int a = (z * (uWidth * HEIGHTMAP_MESH_DENSITY)) + x;
			int b = (z * (uWidth * HEIGHTMAP_MESH_DENSITY)) + x + 1;
			int c = ((z + 1) * (uWidth * HEIGHTMAP_MESH_DENSITY)) + x + 1;
			int d = ((z + 1) * (uWidth * HEIGHTMAP_MESH_DENSITY)) + x;
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

void ExportHeightmap()
{
	cv::Mat xHeightmap = cv::imread(std::string(GAME_ASSETS_DIR) + "Textures/Heightmaps/Test/gaeaHeight.tif", cv::IMREAD_ANYDEPTH);
	cv::Mat xMaterialLerpMap = cv::imread(std::string(GAME_ASSETS_DIR) + "Textures/Heightmaps/Test/gaeaMaterial.tif", cv::IMREAD_ANYDEPTH);

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

	Zenith_Assert((uImageWidth * HEIGHTMAP_MESH_DENSITY) % TERRAIN_SIZE == 0, "Invalid terrain width");
	Zenith_Assert((uImageHeight * HEIGHTMAP_MESH_DENSITY) % TERRAIN_SIZE == 0, "Invalid terrain height");

	uint32_t uNumSplitsX = uImageWidth / TERRAIN_SIZE;
	uint32_t uNumSplitsZ = uImageHeight / TERRAIN_SIZE;

	std::ofstream xAssetsOut("heightmap.vceassets");
	std::ofstream xSceneOut("heightmap.vcescene");

	Flux_MeshGeometry xFullMesh;
	GenerateFullTerrain(xHeightmap, xMaterialLerpMap, xFullMesh);

	for (uint32_t z = 0; z < uNumSplitsZ; z++)
	{
		for (uint32_t x = 0; x < uNumSplitsX; x++)
		{
			Flux_MeshGeometry xSubMesh;
			xSubMesh.m_uNumVerts = (TERRAIN_SIZE + 1) * (TERRAIN_SIZE + 1) * HEIGHTMAP_MESH_DENSITY * HEIGHTMAP_MESH_DENSITY;
			xSubMesh.m_uNumIndices = (((TERRAIN_SIZE + 1) * HEIGHTMAP_MESH_DENSITY) - 1) * (((TERRAIN_SIZE + 1) * HEIGHTMAP_MESH_DENSITY) - 1) * 6;
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
			std::array<uint32_t, TERRAIN_SIZE* HEIGHTMAP_MESH_DENSITY> xRightEdgeIndices;
			std::array<uint32_t, TERRAIN_SIZE* HEIGHTMAP_MESH_DENSITY> xTopEdgeIndices;
			uint32_t uTopRightFromBoth = 0;

			std::vector<glm::vec3> xFoliagePositions;

			glm::highp_vec3 xOrigin = { x, 0, z };
			for (uint32_t subZ = 0; subZ < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY; subZ++)
			{
				for (uint32_t subX = 0; subX < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY; subX++)
				{
					uint32_t uNewOffset = (subZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY) + subX;

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY * uNumSplitsZ) + (z * uImageWidth * HEIGHTMAP_MESH_DENSITY * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
					Zenith_Assert(uIndexIntoRow < HEIGHTMAP_MESH_DENSITY * uImageWidth, "Gone past end of row");
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

					if (subX == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1)
						xRightEdgeIndices.at(subZ) = uNewOffset;
					if (subZ == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1)
						xTopEdgeIndices.at(subX) = uNewOffset;
					if (subX == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1 && subZ == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1)
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
			for (uint32_t indexZ = 0; indexZ < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1; indexZ++)
			{
				for (uint32_t indexX = 0; indexX < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1; indexX++)
				{
					uint32_t a = (indexZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY) + indexX;
					uint32_t b = (indexZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY) + indexX + 1;
					uint32_t c = ((indexZ + 1) * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY) + indexX + 1;
					uint32_t d = ((indexZ + 1) * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY) + indexX;
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
				uint32_t subX = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
				for (uint32_t subZ = 0; subZ < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY; subZ++)
				{
					uint32_t uNewOffset = ++uHeighestNewOffset;

					Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY * uNumSplitsZ) + (z * uImageWidth * HEIGHTMAP_MESH_DENSITY * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
					Zenith_Assert(uIndexIntoRow < HEIGHTMAP_MESH_DENSITY * uImageWidth, "Gone past end of row");
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

					if (subZ == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1)
						uTopRightFromX = uNewOffset;

					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
					//xPositionsSet.insert(pxSubMesh->m_pxVertexPositions[uNewOffset]);
#endif
				}

				uHeighestNewOffset -= TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;

				uint32_t indexX = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;
				for (uint32_t indexZ = 0; indexZ < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1; indexZ++)
				{
					uint32_t a = xRightEdgeIndices.at(indexZ + 1);
					uint32_t c = uHeighestNewOffset++;
					uint32_t b = xRightEdgeIndices.at(indexZ);
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
				uint32_t subZ = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
				for (uint32_t subX = 0; subX < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY; subX++)
				{
					uint32_t uNewOffset = ++uHeighestNewOffset;

					Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

					uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY * uNumSplitsZ) + (z * uImageWidth * HEIGHTMAP_MESH_DENSITY * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY);
					Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
					uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
					Zenith_Assert(uIndexIntoRow < HEIGHTMAP_MESH_DENSITY * uImageWidth, "Gone past end of row");
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

					if (subX == TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1)
						uTopRightFromZ = uNewOffset;

					uHeighestNewOffset = std::max(uHeighestNewOffset, uNewOffset);
#ifdef ZENITH_ASSERT
					xFoundOldIndices.insert(uOldOffset);
					xFoundNewIndices.insert(uNewOffset);
#endif
				}

				uHeighestNewOffset -= TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;

				uint32_t indexZ = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;
				for (uint32_t indexX = 0; indexX < TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1; indexX++)
				{
					uint32_t c = xTopEdgeIndices.at(indexX + 1);
					uint32_t a = uHeighestNewOffset++;
					uint32_t b = xTopEdgeIndices.at(indexX);
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
				uint32_t subZ = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
				uint32_t subX = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
				uint32_t uNewOffset = ++uHeighestNewOffset;

				Zenith_Assert(uNewOffset < xSubMesh.m_uNumVerts, "Offset too big for submesh");

				uint32_t uStartOfRow = (subZ * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY * uNumSplitsZ) + (z * uImageWidth * HEIGHTMAP_MESH_DENSITY * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY);
				Zenith_Assert(uStartOfRow < xFullMesh.m_uNumVerts, "Start of row has gone past end of mesh");
				uint32_t uIndexIntoRow = subX + x * TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY;
				Zenith_Assert(uIndexIntoRow < HEIGHTMAP_MESH_DENSITY * uImageWidth, "Gone past end of row");
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

				uint32_t indexZ = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;
				uint32_t indexX = TERRAIN_SIZE * HEIGHTMAP_MESH_DENSITY - 1;
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
			xSubMesh.Export((std::string(GAME_ASSETS_DIR) + std::string("Terrain\\") + std::to_string(x) + std::string("_") + std::to_string(z) + std::string(".zmsh")).c_str());
			Sleep(10);

#if 0
			GUID xAssetGUID;
			GUID xSceneGUID;
			xAssetsOut << "Mesh\n" << xAssetGUID.m_uGuid << '\n' << "Terrain/" << std::to_string(x) + "_" + std::to_string(z) + "." + VCE_MESH_FILE_EXTENSION + "\n";
			xSceneOut << "Entity\n" << xSceneGUID.m_uGuid << '\n' << "0\n" << "Terrain" << std::to_string(x) + "_" + std::to_string(z) << '\n';

			xSceneOut << "TransformComponent" << '\n' << "0 0 0" << '\n' << "1 0 0 0" << '\n' << "1 1 1" << '\n';

			xSceneOut << "TerrainComponent\n" << xAssetGUID.m_uGuid << "\n1538048126\n" << x << ' ' << z << '\n';
			xSceneOut << "ColliderComponent" << '\n' << "Terrain" << '\n' << "Static" << '\n';

			WriteFoliageComponent(xSceneGUID, xFoliageMaterialGUID, xFoliagePositions, xSceneOut);
			xSceneOut << "EndEntity\n";
#endif
		}
	}

	xAssetsOut.close();
	xSceneOut.close();
}