#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

static std::string ShaderDataTypeToString(ShaderDataType eType)
{
	switch (eType)
	{
	case SHADER_DATA_TYPE_FLOAT:
		return "Float";
	case SHADER_DATA_TYPE_FLOAT2:
		return "Float2";
	case SHADER_DATA_TYPE_FLOAT3:
		return "Float3";
	case SHADER_DATA_TYPE_FLOAT4:
		return "Float4";
	case SHADER_DATA_TYPE_UINT4:
		return "UInt4";
	default:
		Zenith_Assert(false, "Unknown data type");
		return "";
	}
}

static void Export(Flux_MeshGeometry& xMesh, const char* szFilename)
{
	FILE* pxFile = fopen(szFilename, "wb");
	char cNull = '\0';

	fputs(std::to_string(xMesh.m_xBufferLayout.GetElements().size()).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);
	for (Flux_BufferElement& xElement : xMesh.m_xBufferLayout.GetElements())
	{
		fputs(ShaderDataTypeToString(xElement._Type).c_str(), pxFile);
		fwrite(&cNull, 1, 1, pxFile);
	}


	fputs(std::to_string(xMesh.m_uNumVerts).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(xMesh.m_uNumIndices).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(xMesh.m_uNumVerts * xMesh.m_xBufferLayout.GetStride()).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);


	fputs(std::to_string(xMesh.m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType)).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fwrite(xMesh.m_pVertexData, xMesh.m_uNumVerts * xMesh.m_xBufferLayout.GetStride(), 1, pxFile);

	fwrite(xMesh.m_puIndices, xMesh.m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType), 1, pxFile);

	fwrite(xMesh.m_pxPositions, xMesh.m_uNumVerts * sizeof(xMesh.m_pxPositions[0]), 1, pxFile);

	fwrite(xMesh.m_pxNormals, xMesh.m_uNumVerts * sizeof(xMesh.m_pxNormals[0]), 1, pxFile);


	fclose(pxFile);
}


static void ExportFromObj(std::string& strFilename)
{
	//#TO_TODO: double check this
	const bool bFlipWinding = false;

	Flux_MeshGeometry xMesh;

	Assimp::Importer importer;
	const aiScene* pxScene = importer.ReadFile(strFilename,
		aiProcess_CalcTangentSpace |
		aiProcess_GenSmoothNormals |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType);



	if (!pxScene)
	{
		Zenith_Log("Null mesh scene %s", strFilename);
		return;
	}


	//#TO_TODO: do i care about any other meshes?
	aiMesh* pxAssimpMesh = pxScene->mMeshes[0];

	Zenith_Assert(pxAssimpMesh->mNumBones == 0, "Don't support exporting skinned meshes");

	xMesh.m_uNumVerts = pxAssimpMesh->mNumVertices;
	xMesh.m_uNumIndices = pxAssimpMesh->mNumFaces * 3;

	xMesh.m_puIndices = new Flux_MeshGeometry::IndexType[xMesh.m_uNumIndices];

	bool bHasPositions = &(pxAssimpMesh->mVertices[0]);
	bool bHasUVs = &(pxAssimpMesh->mTextureCoords[0][0]);
	bool bHasNormals = &(pxAssimpMesh->mNormals[0]);
	bool bHasTangents = &(pxAssimpMesh->mTangents[0]);
	bool bHasBitangents = &(pxAssimpMesh->mBitangents[0]);

	if (bHasPositions)
	{
		xMesh.m_pxPositions = new glm::vec3[xMesh.m_uNumVerts];
	}
	if (bHasUVs)
	{
		xMesh.m_pxUVs = new glm::vec2[xMesh.m_uNumVerts];
	}
	if (bHasNormals)
	{
		xMesh.m_pxNormals = new glm::vec3[xMesh.m_uNumVerts];
	}
	if (bHasTangents)
	{
		xMesh.m_pxTangents = new glm::vec3[xMesh.m_uNumVerts];
	}
	if (bHasBitangents)
	{
		xMesh.m_pxBitangents = new glm::vec3[xMesh.m_uNumVerts];
	}

	for (uint32_t i = 0; i < pxAssimpMesh->mNumVertices; i++)
	{
		if (bHasPositions)
		{
			const aiVector3D* pxPos = &(pxAssimpMesh->mVertices[i]);
			xMesh.m_pxPositions[i] = glm::vec3(pxPos->x, pxPos->y, pxPos->z);
		}
		if (bHasUVs)
		{
			const aiVector3D* pxTexCoord = &(pxAssimpMesh->mTextureCoords[0][i]);
			xMesh.m_pxUVs[i] = glm::vec2(pxTexCoord->x, pxTexCoord->y);
		}
		if (bHasNormals)
		{
			const aiVector3D* pxNormal = &(pxAssimpMesh->mNormals[i]);
			xMesh.m_pxNormals[i] = glm::vec3(pxNormal->x, pxNormal->y, pxNormal->z);
		}
		if (bHasTangents)
		{
			const aiVector3D* pxTangent = &(pxAssimpMesh->mTangents[i]);
			xMesh.m_pxTangents[i] = glm::vec3(pxTangent->x, pxTangent->y, pxTangent->z);
		}
		if (bHasBitangents)
		{
			const aiVector3D* pxBitangent = &(pxAssimpMesh->mBitangents[i]);
			xMesh.m_pxBitangents[i] = glm::vec3(pxBitangent->x, pxBitangent->y, pxBitangent->z);
		}
	}

	for (uint32_t i = 0; i < pxAssimpMesh->mNumFaces; i++)
	{
		Zenith_Assert(pxAssimpMesh->mFaces[i].mNumIndices == 3, "Face isn't a triangle");

		xMesh.m_puIndices[i * 3 + 0] = pxAssimpMesh->mFaces[i].mIndices[0];
		xMesh.m_puIndices[i * 3 + 1] = pxAssimpMesh->mFaces[i].mIndices[bFlipWinding ? 1 : 2];
		xMesh.m_puIndices[i * 3 + 2] = pxAssimpMesh->mFaces[i].mIndices[bFlipWinding ? 2 : 1];
	}



	xMesh.GenerateLayoutAndVertexData();

	size_t ulFindPos = strFilename.find("obj");
	Zenith_Assert(ulFindPos != std::string::npos, "How have we managed to get here when this isn't an obj file?");
	strFilename.replace(ulFindPos, strlen("obj"), "zmsh");

	Export(xMesh, strFilename.c_str());
}

void ExportAllMeshes()
{
	for (auto& xFile : std::filesystem::recursive_directory_iterator(ASSETS_DIR))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';

		//is this an obj
		if (!strcmp(szFilename + strlen(szFilename) - strlen("obj"), "obj"))
		{
			std::string strFilename(szFilename);
			ExportFromObj(strFilename);
		}
	}
}
