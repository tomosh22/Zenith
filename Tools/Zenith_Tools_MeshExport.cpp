#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Zenith_Tools_TextureExport.h"

#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb/stb_image.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

static void ExportAssimpMesh(aiMesh* pxAssimpMesh, std::string strOutFilename)
{
	//#TO_TODO: double check this
	const bool bFlipWinding = false;

	Flux_MeshGeometry xMesh;

	xMesh.m_uNumVerts = pxAssimpMesh->mNumVertices;
	xMesh.m_uNumIndices = pxAssimpMesh->mNumFaces * 3;

	xMesh.m_puIndices = new Flux_MeshGeometry::IndexType[xMesh.m_uNumIndices];

	const bool bHasPositions = &(pxAssimpMesh->mVertices[0]);
	const bool bHasUVs = &(pxAssimpMesh->mTextureCoords[0][0]);
	const bool bHasNormals = &(pxAssimpMesh->mNormals[0]);
	const bool bHasTangents = &(pxAssimpMesh->mTangents[0]);
	const bool bHasBitangents = &(pxAssimpMesh->mBitangents[0]);
	const bool bHasBones = pxAssimpMesh->mNumBones;

	if (bHasBones)
	{
		xMesh.m_uNumBones = pxAssimpMesh->mNumBones;

		for (uint32_t u = 0; u < xMesh.m_uNumBones; u++)
		{
			const aiMatrix4x4& xAssimpMat = pxAssimpMesh->mBones[u]->mOffsetMatrix;
			Zenith_Maths::Matrix4 xMat;
			xMat[0][0] = xAssimpMat.a1;
			xMat[1][0] = xAssimpMat.a2;
			xMat[2][0] = xAssimpMat.a3;
			xMat[3][0] = xAssimpMat.a4;
			xMat[0][1] = xAssimpMat.b1;
			xMat[1][1] = xAssimpMat.b2;
			xMat[2][1] = xAssimpMat.b3;
			xMat[3][1] = xAssimpMat.b4;
			xMat[0][2] = xAssimpMat.c1;
			xMat[1][2] = xAssimpMat.c2;
			xMat[2][2] = xAssimpMat.c3;
			xMat[3][2] = xAssimpMat.c4;
			xMat[0][3] = xAssimpMat.d1;
			xMat[1][3] = xAssimpMat.d2;
			xMat[2][3] = xAssimpMat.d3;
			xMat[3][3] = xAssimpMat.d4;

			Zenith_Assert(xMesh.m_xBoneNameToIdAndOffset.find(pxAssimpMesh->mBones[u]->mName.C_Str()) == xMesh.m_xBoneNameToIdAndOffset.end(), "Bone name already exists");
			xMesh.m_xBoneNameToIdAndOffset.insert({ pxAssimpMesh->mBones[u]->mName.C_Str(), {u, xMat } });
		}
	}

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
	if (bHasBones)
	{
		xMesh.m_puBoneIDs = new uint32_t[xMesh.m_uNumVerts * MAX_BONES_PER_VERTEX];
		xMesh.m_pfBoneWeights = new float[xMesh.m_uNumVerts * MAX_BONES_PER_VERTEX];
		memset(xMesh.m_puBoneIDs, ~0u, sizeof(uint32_t) * xMesh.m_uNumVerts * MAX_BONES_PER_VERTEX);
		memset(xMesh.m_pfBoneWeights, 0u, sizeof(float) * xMesh.m_uNumVerts * MAX_BONES_PER_VERTEX);
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

	if (bHasBones)
	{
		for (uint32_t uBoneIndex = 0; uBoneIndex < pxAssimpMesh->mNumBones; uBoneIndex++)
		{
			const aiBone* pxBone = pxAssimpMesh->mBones[uBoneIndex];
			const aiVertexWeight* pxWeights = pxBone->mWeights;

			//Zenith_Assert(xMesh.m_xBoneNameToID.find(pxBone->mName.C_Str()) == xMesh.m_xBoneNameToID.end(), "Already found this bone name");
			//Zenith_Assert(xMesh.m_xBoneIDToName.find(uBoneIndex) == xMesh.m_xBoneIDToName.end(), "Already found this bone id");

			//xMesh.m_xBoneNameToID.insert({ pxBone->mName.C_Str(), uBoneIndex });
			//xMesh.m_xBoneIDToName.insert({ uBoneIndex, pxBone->mName.C_Str() });

			for (uint32_t uWeightIndex = 0; uWeightIndex < pxBone->mNumWeights; uWeightIndex++)
			{
				const aiVertexWeight& xWeight = pxWeights[uWeightIndex];
				uint32_t uVertexID = xWeight.mVertexId;
				float fWeight = xWeight.mWeight;
				
				uint32_t* puFirstIndexForThisVertex = xMesh.m_puBoneIDs + uVertexID * MAX_BONES_PER_VERTEX;
				float* puFirstWeightForThisVertex = xMesh.m_pfBoneWeights + uVertexID * MAX_BONES_PER_VERTEX;
				for (uint32_t u = 0; u < MAX_BONES_PER_VERTEX; u++)
				{
					if (*(puFirstIndexForThisVertex + u) == ~0u)
					{
						*(puFirstIndexForThisVertex + u) = uBoneIndex;
						Zenith_Assert(*(puFirstWeightForThisVertex + u) == 0, "There is already a bone weight here");
						*(puFirstWeightForThisVertex + u) = fWeight;
						break;
					}
					Zenith_Assert(u < MAX_BONES_PER_VERTEX - 1, "Failed to assign vertex to bone");
				}
			}
		}

#ifdef ZENITH_ASSERT
		for (uint32_t uVert = 0; uVert < xMesh.m_uNumVerts; uVert++)
		{
			float fTotalWeight = 0;
			float* puFirstWeightForThisVertex  = xMesh.m_pfBoneWeights + uVert * MAX_BONES_PER_VERTEX;
			for (uint32_t uWeight = 0; uWeight < MAX_BONES_PER_VERTEX; uWeight++)
			{
				fTotalWeight += *(puFirstWeightForThisVertex + uWeight);
			}
			Zenith_Assert(std::fabsf(1.f - fTotalWeight) < 0.1f, "Vertex weights don't add to 1");
		}
#endif
	}

	for (uint32_t i = 0; i < pxAssimpMesh->mNumFaces; i++)
	{
		Zenith_Assert(pxAssimpMesh->mFaces[i].mNumIndices == 3, "Face isn't a triangle");

		xMesh.m_puIndices[i * 3 + 0] = pxAssimpMesh->mFaces[i].mIndices[0];
		xMesh.m_puIndices[i * 3 + 1] = pxAssimpMesh->mFaces[i].mIndices[bFlipWinding ? 1 : 2];
		xMesh.m_puIndices[i * 3 + 2] = pxAssimpMesh->mFaces[i].mIndices[bFlipWinding ? 2 : 1];
	}

	xMesh.GenerateLayoutAndVertexData();

	xMesh.Export(strOutFilename.c_str());
}

static void ProcessNode(aiNode* pxNode, const aiScene* pxScene, const std::string& strExtension, const std::string& strFilename, uint32_t& uIndex, const char* szExportFilenameOverride = nullptr)
{
	for (uint32_t u = 0; u < pxNode->mNumMeshes; u++)
	{
		aiMesh* pxAssimpMesh = pxScene->mMeshes[pxNode->mMeshes[u]];
		std::string strExportFilename = szExportFilenameOverride ? szExportFilenameOverride : strFilename;
		size_t ulFindPos = strExportFilename.find(strExtension.c_str());
		Zenith_Assert(ulFindPos != std::string::npos, "");
		strExportFilename.replace(ulFindPos, strlen(strExtension.c_str()), "_Mesh" + std::to_string(uIndex++) + "_Mat" + std::to_string(pxAssimpMesh->mMaterialIndex) + ".zmsh");

		ExportAssimpMesh(pxAssimpMesh, strExportFilename);
	}

	for (uint32_t u = 0; u < pxNode->mNumChildren; u++)
	{
		ProcessNode(pxNode->mChildren[u], pxScene, strExtension, strFilename, uIndex, szExportFilenameOverride);
	}
}

static const char* s_aszMaterialTypeToName[]
{
	"None",				//0
	"Diffuse",			//1
	"Specular",			//2
	"Ambient",			//3
	"Emissive",			//4
	"Height",			//5
	"Normals",			//6
	"Shininess",		//7
	"Opacity",			//8
	"Displacement",		//9
	"Lightmap",			//10
	"Reflection",		//11
	"Base_Colour",		//12
	"Normal_Camera",	//13
	"Emission_Colour",	//14
	"Metalness",		//15
	"Diffuse_Roughness",//16
	"Ambient_Occlusion",//17
};

static void ExportMaterialTextures(const aiMaterial* pxMat, const aiScene* pxScene, const std::string& strFilename, const uint32_t uIndex)
{
	for (uint32_t uType = aiTextureType_NONE; uType <= aiTextureType_AMBIENT_OCCLUSION; uType++)
	{
		aiString str;
		pxMat->GetTexture((aiTextureType)uType, 0, &str);
		const aiTexture* pxTex = pxScene->GetEmbeddedTexture(str.C_Str());
		if (pxTex == nullptr) continue;

		Zenith_Assert(pxTex->mHeight == 0, "Need to add support for non compressed textures");

		const uint32_t uCompressedDataSize = pxTex->mWidth;
		const void* pCompressedData = pxTex->pcData;

		int32_t iWidth, iHeight, iNumChannels;
		uint8_t* pData = stbi_load_from_memory((uint8_t*)pCompressedData, uCompressedDataSize, &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);

		std::string strExportFile(strFilename);
		size_t ulDotPos = strExportFile.find(".");
		strExportFile = strExportFile.substr(0, ulDotPos);
		strExportFile += std::string("_") + s_aszMaterialTypeToName[uType];
		strExportFile += std::string("_") + std::to_string(uIndex);
		strExportFile += ".ztx";

		//#TO_TODO: do this properly
		ColourFormat eFormat;
		if (iNumChannels == 3)
		{
			eFormat = COLOUR_FORMAT_RGB8_UNORM;
		}
		else if (iNumChannels == 4)
		{
			eFormat = COLOUR_FORMAT_RGBA8_UNORM;
		}
		else
		{
			Zenith_Assert(false, "What format is this?");
		}

		Zenith_Tools_TextureExport::ExportFromData(pData, strExportFile, iWidth, iHeight, eFormat);
	}
}

static void Export(const std::string& strFilename, const std::string& strExtension, const char* szExportFilenameOverride = nullptr)
{
	Assimp::Importer importer;
	const aiScene* pxScene = importer.ReadFile(strFilename,
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_FlipUVs);

	//#TO_TODO: move this for loop inside the function
	for (uint32_t u = 0; u < pxScene->mNumMaterials; u++)
	{
		ExportMaterialTextures(pxScene->mMaterials[u], pxScene, strFilename, u);
	}

	if (!pxScene)
	{
		Zenith_Log("Null mesh scene %s", strFilename.c_str());
		Zenith_Log("Assimp error %s", importer.GetErrorString() ? importer.GetErrorString() : "no error");
		return;
	}

	uint32_t uRootIndex = 0;
	ProcessNode(pxScene->mRootNode, pxScene, strExtension, strFilename, uRootIndex, szExportFilenameOverride);
}

//#TO export twice and ensure both are identical
static void ExportDeterminismCheck(const std::string& strFilename, const std::string& strExtension)
{
	std::string strCopy0 = strFilename + "0";
	std::string strCopy1 = strFilename + "1";
	Export(strFilename, strExtension, strCopy0.c_str());
	//Export(strFilename, strExtension, strCopy1.c_str());

	Zenith_Entity xDummyEntity0, xDummyEntity1;
	Zenith_ModelComponent xModel0(xDummyEntity0), xModel1(xDummyEntity1);

	std::string strDir = std::filesystem::directory_entry(strFilename).path().parent_path().string();
	xModel0.LoadMeshesFromDir(strDir, nullptr, true, false);
	xModel1.LoadMeshesFromDir(strDir, nullptr, true, false);

	Zenith_Assert(xModel0.GetNumMeshEntires() == xModel1.GetNumMeshEntires());

	for (u_int u = 0; u < xModel0.GetNumMeshEntires(); u++)
	{
		Flux_MeshGeometry& xMesh0 = xModel0.GetMeshGeometryAtIndex(u);
		Flux_MeshGeometry& xMesh1 = xModel1.GetMeshGeometryAtIndex(u);
		//buffer layout
		Zenith_Assert(xMesh0.m_xBufferLayout.GetStride() == xMesh1.m_xBufferLayout.GetStride());

		Zenith_Assert(xMesh0.GetNumVerts() == xMesh1.GetNumVerts());
		Zenith_Assert(xMesh0.GetNumIndices() == xMesh1.GetNumIndices());
		Zenith_Assert(xMesh0.GetNumBones() == xMesh1.GetNumBones());
		Zenith_Assert(xMesh0.m_xBoneNameToIdAndOffset == xMesh1.m_xBoneNameToIdAndOffset);

		const u_int uVertexDataSize = xMesh0.m_uNumVerts * xMesh0.m_xBufferLayout.GetStride();
		const u_int uIndexDataSize = xMesh0.m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType);

		Zenith_Assert(!memcmp(xMesh0.m_pVertexData, xMesh1.m_pVertexData, uVertexDataSize));
		Zenith_Assert(!memcmp(xMesh0.m_puIndices, xMesh1.m_puIndices, uIndexDataSize));
		Zenith_Assert(!memcmp(xMesh0.m_pxPositions, xMesh1.m_pxPositions, xMesh0.m_uNumVerts * sizeof(xMesh0.m_pxPositions[0])));
		Zenith_Assert(!memcmp(xMesh0.m_pxNormals, xMesh1.m_pxNormals, xMesh0.m_uNumVerts * sizeof(xMesh0.m_pxNormals[0])));
	}

	volatile bool a = false;
	//std::filesystem::remove(strCopy0);
	//std::filesystem::remove(strCopy1);
}

void ExportAllMeshes()
{
	for (auto& xFile : std::filesystem::recursive_directory_iterator(GAME_ASSETS_DIR))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';

		//#TO_TODO: fix asset export pipeline, we need this to avoid trying to export C++ IR files (.obj)
		if (!strstr(szFilename, "Assets"))
		{
			continue;
		}

		//is this a gltf
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".gltf"), ".gltf"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".gltf");
		}

		//is this an fbx
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".fbx"), ".fbx"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".fbx");

			if(strFilename.find("stickymcstickface") != std::string::npos)
			{
				//ExportDeterminismCheck(strFilename, ".fbx");
			}
		}

		//is this an obj
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".obj"), ".obj"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".obj");
		}
	}
}