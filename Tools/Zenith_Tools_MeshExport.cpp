#include "Zenith.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Zenith_Tools_TextureExport.h"

#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb/stb_image.h"
#include <vector>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

static void ExportAssimpMesh(aiMesh* pxAssimpMesh, std::string strOutFilename)
{
	const bool bFlipWinding = false;

	Flux_MeshGeometry xMesh;

	const uint32_t uNumVerts = pxAssimpMesh->mNumVertices;
	const uint32_t uNumIndices = pxAssimpMesh->mNumFaces * 3;
	xMesh.m_uNumVerts = uNumVerts;
	xMesh.m_uNumIndices = uNumIndices;
	xMesh.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	const bool bHasPositions = pxAssimpMesh->mVertices != nullptr;
	const bool bHasUVs = pxAssimpMesh->mTextureCoords[0] != nullptr;
	const bool bHasNormals = pxAssimpMesh->mNormals != nullptr;
	const bool bHasTangents = pxAssimpMesh->mTangents != nullptr;
	const bool bHasBitangents = pxAssimpMesh->mBitangents != nullptr;
	const bool bHasBones = pxAssimpMesh->mNumBones > 0;

	Zenith_Log("MESH_EXPORT: Exporting mesh to %s (Verts: %u, Indices: %u, Bones: %u)",
		strOutFilename.c_str(), uNumVerts, uNumIndices, pxAssimpMesh->mNumBones);

	xMesh.SetNumBones(pxAssimpMesh->mNumBones);

	if (bHasBones)
	{
		u_int* puVertexBoneCount = new u_int[uNumVerts];
		memset(puVertexBoneCount, 0, sizeof(u_int) * uNumVerts);

		for (u_int uBoneIndex = 0; uBoneIndex < pxAssimpMesh->mNumBones; uBoneIndex++)
		{
			const aiBone* pxBone = pxAssimpMesh->mBones[uBoneIndex];

			for (u_int uWeightIndex = 0; uWeightIndex < pxBone->mNumWeights; uWeightIndex++)
			{
				u_int uVertexID = pxBone->mWeights[uWeightIndex].mVertexId;
				puVertexBoneCount[uVertexID]++;
			}
		}

		for (u_int uVert = 0; uVert < uNumVerts; uVert++)
		{
			if (puVertexBoneCount[uVert] > MAX_BONES_PER_VERTEX)
			{
				Zenith_Log("Mesh has vertices with more than MAX_BONES_PER_VERTEX bone influences");
				return;
			}
		}

		delete[] puVertexBoneCount;

		for (u_int uBoneIndex = 0; uBoneIndex < pxAssimpMesh->mNumBones; uBoneIndex++)
		{
			const aiBone* pxBone = pxAssimpMesh->mBones[uBoneIndex];
			const aiMatrix4x4& xAssimpMat = pxBone->mOffsetMatrix;

			Zenith_Maths::Matrix4 xMat;
			xMat[0][0] = xAssimpMat.a1; xMat[1][0] = xAssimpMat.a2; xMat[2][0] = xAssimpMat.a3; xMat[3][0] = xAssimpMat.a4;
			xMat[0][1] = xAssimpMat.b1; xMat[1][1] = xAssimpMat.b2; xMat[2][1] = xAssimpMat.b3; xMat[3][1] = xAssimpMat.b4;
			xMat[0][2] = xAssimpMat.c1; xMat[1][2] = xAssimpMat.c2; xMat[2][2] = xAssimpMat.c3; xMat[3][2] = xAssimpMat.c4;
			xMat[0][3] = xAssimpMat.d1; xMat[1][3] = xAssimpMat.d2; xMat[2][3] = xAssimpMat.d3; xMat[3][3] = xAssimpMat.d4;

			Zenith_Assert(xMesh.m_xBoneNameToIdAndOffset.find(pxBone->mName.C_Str()) == xMesh.m_xBoneNameToIdAndOffset.end(),
				"Duplicate bone name found");
			xMesh.m_xBoneNameToIdAndOffset.insert({ pxBone->mName.C_Str(), {uBoneIndex, xMat} });
		}
	}

	if (bHasPositions)
		xMesh.m_pxPositions = new glm::vec3[uNumVerts];
	
	// Always allocate UVs for animated meshes to ensure consistent vertex layout
	if (bHasUVs || bHasBones)
		xMesh.m_pxUVs = new glm::vec2[uNumVerts];

	if (bHasNormals)
		xMesh.m_pxNormals = new glm::vec3[uNumVerts];

	// Always allocate tangents and bitangents for animated meshes to ensure consistent vertex layout
	if (bHasTangents || bHasBones)
		xMesh.m_pxTangents = new glm::vec3[uNumVerts];

	if (bHasBitangents || bHasBones)
		xMesh.m_pxBitangents = new glm::vec3[uNumVerts];

	if (bHasBones)
	{
		xMesh.m_puBoneIDs = new u_int[uNumVerts * MAX_BONES_PER_VERTEX];
		xMesh.m_pfBoneWeights = new float[uNumVerts * MAX_BONES_PER_VERTEX];

		for (u_int i = 0; i < uNumVerts * MAX_BONES_PER_VERTEX; i++)
		{
			xMesh.m_puBoneIDs[i] = ~0u;
			xMesh.m_pfBoneWeights[i] = 0.0f;
		}
	}

	for (u_int i = 0; i < pxAssimpMesh->mNumVertices; i++)
	{
		if (bHasPositions)
		{
			const aiVector3D& xPos = pxAssimpMesh->mVertices[i];
			xMesh.m_pxPositions[i] = glm::vec3(xPos.x, xPos.y, xPos.z);
		}

		if (bHasUVs)
		{
			const aiVector3D& xUV = pxAssimpMesh->mTextureCoords[0][i];
			xMesh.m_pxUVs[i] = glm::vec2(xUV.x, xUV.y);
		}
		else if (bHasBones)
		{
			// Set dummy UVs for skinned meshes without UVs to maintain vertex layout
			xMesh.m_pxUVs[i] = glm::vec2(0.0f, 0.0f);
		}

		if (bHasNormals)
		{
			const aiVector3D& xNormal = pxAssimpMesh->mNormals[i];
			xMesh.m_pxNormals[i] = glm::normalize(glm::vec3(xNormal.x, xNormal.y, xNormal.z));
		}

		if (bHasTangents)
		{
			const aiVector3D& xTangent = pxAssimpMesh->mTangents[i];
			xMesh.m_pxTangents[i] = glm::normalize(glm::vec3(xTangent.x, xTangent.y, xTangent.z));
		}
		else if (bHasBones)
		{
			// Set dummy tangent for skinned meshes without tangents
			xMesh.m_pxTangents[i] = glm::vec3(1.0f, 0.0f, 0.0f);
		}

		if (bHasBitangents)
		{
			const aiVector3D& xBitangent = pxAssimpMesh->mBitangents[i];
			xMesh.m_pxBitangents[i] = glm::normalize(glm::vec3(xBitangent.x, xBitangent.y, xBitangent.z));
		}
		else if (bHasBones)
		{
			// Set dummy bitangent for skinned meshes without bitangents
			xMesh.m_pxBitangents[i] = glm::vec3(0.0f, 1.0f, 0.0f);
		}
	}

	if (bHasBones)
	{
		for (u_int uBoneIndex = 0; uBoneIndex < pxAssimpMesh->mNumBones; uBoneIndex++)
		{
			const aiBone* pxBone = pxAssimpMesh->mBones[uBoneIndex];

			for (u_int uWeightIndex = 0; uWeightIndex < pxBone->mNumWeights; uWeightIndex++)
			{
				const aiVertexWeight& xWeight = pxBone->mWeights[uWeightIndex];
				u_int uVertexID = xWeight.mVertexId;
				float fWeight = xWeight.mWeight;

				Zenith_Assert(uVertexID < uNumVerts, "Vertex ID out of range");

				u_int* puBoneIDs = xMesh.m_puBoneIDs + (uVertexID * MAX_BONES_PER_VERTEX);
				float* pfBoneWeights = xMesh.m_pfBoneWeights + (uVertexID * MAX_BONES_PER_VERTEX);

				bool bAssigned = false;
				for (u_int uSlot = 0; uSlot < MAX_BONES_PER_VERTEX; uSlot++)
				{
					if (puBoneIDs[uSlot] == ~0u)
					{
						puBoneIDs[uSlot] = uBoneIndex;
						pfBoneWeights[uSlot] = fWeight;
						bAssigned = true;
						break;
					}
				}

				Zenith_Assert(bAssigned, "Failed to assign bone weight");
			}
		}

		for (u_int uVert = 0; uVert < uNumVerts; uVert++)
		{
			float* pfBoneWeights = xMesh.m_pfBoneWeights + (uVert * MAX_BONES_PER_VERTEX);

			float fTotalWeight = 0.0f;
			for (u_int uSlot = 0; uSlot < MAX_BONES_PER_VERTEX; uSlot++)
			{
				fTotalWeight += pfBoneWeights[uSlot];
			}

			if (fTotalWeight > 0.0001f)
			{
				float fInvTotalWeight = 1.0f / fTotalWeight;
				for (u_int uSlot = 0; uSlot < MAX_BONES_PER_VERTEX; uSlot++)
				{
					pfBoneWeights[uSlot] *= fInvTotalWeight;
				}
			}
		}

		for (u_int uVert = 0; uVert < uNumVerts; uVert++)
		{
			float* pfBoneWeights = xMesh.m_pfBoneWeights + (uVert * MAX_BONES_PER_VERTEX);

			float fTotalWeight = 0.0f;
			for (u_int uSlot = 0; uSlot < MAX_BONES_PER_VERTEX; uSlot++)
			{
				fTotalWeight += pfBoneWeights[uSlot];
			}

			if (fTotalWeight > 0.0001f)
			{
				Zenith_Assert(std::fabsf(1.0f - fTotalWeight) < 0.01f,
					"Vertex bone weights don't sum to 1.0 after normalization");
			}
		}
	}

	for (u_int i = 0; i < pxAssimpMesh->mNumFaces; i++)
	{
		const aiFace& xFace = pxAssimpMesh->mFaces[i];
		if (xFace.mNumIndices != 3)
		{
			Zenith_Log("Face is not a triangle, aborting");
			return;
		}

		xMesh.m_puIndices[i * 3 + 0] = xFace.mIndices[0];
		xMesh.m_puIndices[i * 3 + 1] = xFace.mIndices[bFlipWinding ? 1 : 2];
		xMesh.m_puIndices[i * 3 + 2] = xFace.mIndices[bFlipWinding ? 2 : 1];
	}

	xMesh.GenerateLayoutAndVertexData();
	xMesh.Export(strOutFilename.c_str());

	Zenith_Log("MESH_EXPORT: Successfully exported %s", strOutFilename.c_str());
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
	"BaseColor",		//12 - glTF base color texture
	"Normal_Camera",	//13
	"Emissive",			//14 - glTF emissive texture
	"Metallic",			//15 - glTF metallic texture
	"Roughness",		//16 - glTF roughness or combined MetallicRoughness
	"Occlusion",		//17 - glTF ambient occlusion texture
};

static void ExportMaterialTextures(const aiMaterial* pxMat, const aiScene* pxScene, const std::string& strFilename, const uint32_t uIndex)
{
	for (uint32_t uType = aiTextureType_NONE; uType <= aiTextureType_AMBIENT_OCCLUSION; uType++)
	{
		aiString str;
		pxMat->GetTexture((aiTextureType)uType, 0, &str);
		const aiTexture* pxTex = pxScene->GetEmbeddedTexture(str.C_Str());

		int32_t iWidth = 0, iHeight = 0, iNumChannels = 0;
		uint8_t* pData = nullptr;

		if (pxTex)
		{
			Zenith_Assert(pxTex->mHeight == 0, "Need to add support for non compressed textures");
			const uint32_t uCompressedDataSize = pxTex->mWidth;
			const void* pCompressedData = pxTex->pcData;
			pData = stbi_load_from_memory((uint8_t*)pCompressedData, uCompressedDataSize, &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);
		}
		else
		{
			if (str.length == 0)
			{
				continue;
			}
			std::filesystem::path xModelPath = std::filesystem::directory_entry(strFilename).path().parent_path();
			std::filesystem::path xTexRelPath = std::filesystem::path(str.C_Str());
			std::filesystem::path xTexPath = xTexRelPath.is_absolute() ? xTexRelPath : (xModelPath / xTexRelPath);
			std::string strTexPath = xTexPath.string();
			pData = stbi_load(strTexPath.c_str(), &iWidth, &iHeight, &iNumChannels, STBI_rgb_alpha);
			if (!pData)
			{
				continue;
			}
		}

		std::string strExportFile(strFilename);
		size_t ulDotPos = strExportFile.find(".");
		strExportFile = strExportFile.substr(0, ulDotPos);
		strExportFile += std::string("_") + s_aszMaterialTypeToName[uType];
		strExportFile += std::string("_") + std::to_string(uIndex);
		strExportFile += ".ztx";

		TextureFormat eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
		Zenith_Tools_TextureExport::ExportFromData(pData, strExportFile, iWidth, iHeight, eFormat);
		stbi_image_free(pData);
	}
}

static void Export(const std::string& strFilename, const std::string& strExtension, const char* szExportFilenameOverride = nullptr)
{
	Assimp::Importer importer;
	const aiScene* pxScene = importer.ReadFile(strFilename,
		aiProcess_CalcTangentSpace |
		aiProcess_LimitBoneWeights |
		aiProcess_Triangulate |
		aiProcess_FlipUVs);

	if (!pxScene)
	{
		Zenith_Log("Null mesh scene %s", strFilename.c_str());
		Zenith_Log("Assimp error %s", importer.GetErrorString() ? importer.GetErrorString() : "no error");
		return;
	}

	//#TO_TODO: move this for loop inside the function
	for (uint32_t u = 0; u < pxScene->mNumMaterials; u++)
	{
		ExportMaterialTextures(pxScene->mMaterials[u], pxScene, strFilename, u);
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
	xModel0.LoadMeshesFromDir(strDir, nullptr, -1, false);
	xModel1.LoadMeshesFromDir(strDir, nullptr, -1, false);

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

			if (strFilename.find("stickymcstickface") != std::string::npos)
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