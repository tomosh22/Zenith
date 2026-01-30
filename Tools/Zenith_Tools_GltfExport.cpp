#include "Zenith.h"
#include "Zenith_Tools_GltfExport.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// Disable Zenith memory management for the entire file.
// All Assimp object allocations must use standard C++ allocator
// so that Assimp's destructors can properly free them.
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Zenith_Tools_AssimpConvert.h"

#include <filesystem>

namespace Zenith_Tools_GltfExport
{

//-----------------------------------------------------------------------------
// Helper: Build complete aiScene from Zenith assets
//-----------------------------------------------------------------------------
static aiScene* BuildScene(
	const Zenith_MeshAsset* pxMesh,
	const Zenith_SkeletonAsset* pxSkeleton,
	const std::vector<const Flux_AnimationClip*>& axAnimations)
{
	aiScene* pxScene = new aiScene();
	pxScene->mFlags = 0;

	// Create root node
	aiNode* pxRootNode = new aiNode();
	pxRootNode->mName = aiString("RootNode");
	pxRootNode->mTransformation = aiMatrix4x4();
	pxScene->mRootNode = pxRootNode;

	// Create default material
	pxScene->mNumMaterials = 1;
	pxScene->mMaterials = new aiMaterial*[1];
	aiMaterial* pxMat = new aiMaterial();
	aiString strMatName("DefaultMaterial");
	pxMat->AddProperty(&strMatName, AI_MATKEY_NAME);

	// Set material color from mesh
	aiColor4D xDiffuse(
		pxMesh->m_xMaterialColor.r,
		pxMesh->m_xMaterialColor.g,
		pxMesh->m_xMaterialColor.b,
		pxMesh->m_xMaterialColor.a
	);
	pxMat->AddProperty(&xDiffuse, 1, AI_MATKEY_COLOR_DIFFUSE);
	pxScene->mMaterials[0] = pxMat;

	// Create mesh
	pxScene->mNumMeshes = 1;
	pxScene->mMeshes = new aiMesh*[1];
	pxScene->mMeshes[0] = Zenith_AssimpConvert::ZenithToAssimp(pxMesh, pxSkeleton);
	pxScene->mMeshes[0]->mMaterialIndex = 0;

	// Build skeleton node hierarchy if we have a skeleton
	if (pxSkeleton && pxSkeleton->GetNumBones() > 0)
	{
		aiNode* pxSkeletonRoot = Zenith_AssimpConvert::ZenithToAssimp(pxSkeleton);
		if (pxSkeletonRoot)
		{
			// Create mesh node that references the mesh
			aiNode* pxMeshNode = new aiNode();
			pxMeshNode->mName = aiString("MeshNode");
			pxMeshNode->mTransformation = aiMatrix4x4();
			pxMeshNode->mNumMeshes = 1;
			pxMeshNode->mMeshes = new unsigned int[1];
			pxMeshNode->mMeshes[0] = 0;
			pxMeshNode->mParent = pxRootNode;

			// Attach skeleton and mesh to root
			pxSkeletonRoot->mParent = pxRootNode;

			pxRootNode->mNumChildren = 2;
			pxRootNode->mChildren = new aiNode*[2];
			pxRootNode->mChildren[0] = pxSkeletonRoot;
			pxRootNode->mChildren[1] = pxMeshNode;
		}
	}
	else
	{
		// No skeleton - just attach mesh directly to root
		pxRootNode->mNumMeshes = 1;
		pxRootNode->mMeshes = new unsigned int[1];
		pxRootNode->mMeshes[0] = 0;
	}

	// Add animations
	if (!axAnimations.empty())
	{
		pxScene->mNumAnimations = static_cast<uint32_t>(axAnimations.size());
		pxScene->mAnimations = new aiAnimation*[pxScene->mNumAnimations];

		for (uint32_t i = 0; i < pxScene->mNumAnimations; i++)
		{
			pxScene->mAnimations[i] = Zenith_AssimpConvert::ZenithToAssimp(axAnimations[i]);
		}
	}

	return pxScene;
}

//-----------------------------------------------------------------------------
// Helper: Determine format ID from file extension
//-----------------------------------------------------------------------------
static const char* GetFormatFromPath(const char* szPath)
{
	std::filesystem::path xPath(szPath);
	std::string strExt = xPath.extension().string();

	// Convert to lowercase
	for (char& c : strExt)
	{
		c = static_cast<char>(tolower(c));
	}

	if (strExt == ".glb")
	{
		return "glb2";  // Binary glTF 2.0
	}
	else
	{
		return "gltf2";  // JSON glTF 2.0
	}
}

//-----------------------------------------------------------------------------
// Helper: Clean up aiScene
//-----------------------------------------------------------------------------
static void FreeScene(aiScene* pxScene)
{
	// Note: Assimp's Exporter doesn't take ownership of the scene,
	// so we need to clean it up ourselves.
	// IMPORTANT: Assimp types have destructors that delete their members.
	// We must null out pointers after deleting to prevent double-free.
	if (!pxScene)
	{
		return;
	}

	// Free meshes
	if (pxScene->mMeshes)
	{
		for (uint32_t i = 0; i < pxScene->mNumMeshes; i++)
		{
			aiMesh* pxMesh = pxScene->mMeshes[i];
			if (pxMesh)
			{
				// aiMesh destructor deletes these, so null them after manual delete
				delete[] pxMesh->mVertices; pxMesh->mVertices = nullptr;
				delete[] pxMesh->mNormals; pxMesh->mNormals = nullptr;
				delete[] pxMesh->mTangents; pxMesh->mTangents = nullptr;
				delete[] pxMesh->mBitangents; pxMesh->mBitangents = nullptr;

				for (uint32_t j = 0; j < AI_MAX_NUMBER_OF_TEXTURECOORDS; j++)
				{
					delete[] pxMesh->mTextureCoords[j];
					pxMesh->mTextureCoords[j] = nullptr;
				}

				for (uint32_t j = 0; j < AI_MAX_NUMBER_OF_COLOR_SETS; j++)
				{
					delete[] pxMesh->mColors[j];
					pxMesh->mColors[j] = nullptr;
				}

				// aiFace::~aiFace() handles deleting mIndices
				// aiMesh::~aiMesh() will delete[] mFaces
				// Just null it so destructor doesn't double-free
				if (pxMesh->mFaces)
				{
					// Must delete faces manually because aiFace dtor deletes mIndices
					// but we allocated them, so let aiFace dtor run
					delete[] pxMesh->mFaces;
					pxMesh->mFaces = nullptr;
				}

				// aiBone::~aiBone() handles deleting mWeights
				// aiMesh::~aiMesh() will delete[] mBones
				if (pxMesh->mBones)
				{
					for (uint32_t j = 0; j < pxMesh->mNumBones; j++)
					{
						delete pxMesh->mBones[j];
					}
					delete[] pxMesh->mBones;
					pxMesh->mBones = nullptr;
				}

				delete pxMesh;
			}
		}
		delete[] pxScene->mMeshes;
		pxScene->mMeshes = nullptr;
	}

	// Free materials - aiMaterial handles its own cleanup
	if (pxScene->mMaterials)
	{
		for (uint32_t i = 0; i < pxScene->mNumMaterials; i++)
		{
			delete pxScene->mMaterials[i];
		}
		delete[] pxScene->mMaterials;
		pxScene->mMaterials = nullptr;
	}

	// Free animations
	if (pxScene->mAnimations)
	{
		for (uint32_t i = 0; i < pxScene->mNumAnimations; i++)
		{
			aiAnimation* pxAnim = pxScene->mAnimations[i];
			if (pxAnim)
			{
				// aiNodeAnim::~aiNodeAnim() handles deleting keys
				if (pxAnim->mChannels)
				{
					for (uint32_t j = 0; j < pxAnim->mNumChannels; j++)
					{
						delete pxAnim->mChannels[j];
					}
					delete[] pxAnim->mChannels;
					pxAnim->mChannels = nullptr;
				}
				delete pxAnim;
			}
		}
		delete[] pxScene->mAnimations;
		pxScene->mAnimations = nullptr;
	}

	// Free node hierarchy (recursive)
	std::function<void(aiNode*)> FreeNode = [&FreeNode](aiNode* pxNode)
	{
		if (!pxNode)
		{
			return;
		}
		if (pxNode->mChildren)
		{
			for (uint32_t i = 0; i < pxNode->mNumChildren; i++)
			{
				FreeNode(pxNode->mChildren[i]);
			}
			delete[] pxNode->mChildren;
			pxNode->mChildren = nullptr;
		}
		if (pxNode->mMeshes)
		{
			delete[] pxNode->mMeshes;
			pxNode->mMeshes = nullptr;
		}
		delete pxNode;
	};
	FreeNode(pxScene->mRootNode);
	pxScene->mRootNode = nullptr;

	delete pxScene;
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------
bool ExportToGltf(
	const char* szOutputPath,
	const Zenith_MeshAsset* pxMesh,
	const Zenith_SkeletonAsset* pxSkeleton,
	const std::vector<const Flux_AnimationClip*>& axAnimations)
{
	if (!pxMesh || !szOutputPath)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "GLTF_EXPORT: Invalid mesh or path");
		return false;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "GLTF_EXPORT: Exporting to %s", szOutputPath);
	Zenith_Log(LOG_CATEGORY_TOOLS, "  Mesh: %u verts, %u indices",
		pxMesh->GetNumVerts(), pxMesh->GetNumIndices());
	if (pxSkeleton)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "  Skeleton: %u bones", pxSkeleton->GetNumBones());
	}
	Zenith_Log(LOG_CATEGORY_TOOLS, "  Animations: %zu", axAnimations.size());

	// Determine format from file extension
	const char* szFormat = GetFormatFromPath(szOutputPath);
	Zenith_Log(LOG_CATEGORY_TOOLS, "  Format: %s", szFormat);

	// Build aiScene from Zenith data
	aiScene* pxScene = BuildScene(pxMesh, pxSkeleton, axAnimations);
	if (!pxScene)
	{
		Zenith_Log(LOG_CATEGORY_TOOLS, "GLTF_EXPORT: Failed to build scene");
		return false;
	}

	// Export using Assimp
	Assimp::Exporter xExporter;
	aiReturn eResult = xExporter.Export(pxScene, szFormat, szOutputPath);

	if (eResult != aiReturn_SUCCESS)
	{
		const char* szError = xExporter.GetErrorString();
		Zenith_Log(LOG_CATEGORY_TOOLS, "GLTF_EXPORT: Export failed - %s",
			szError ? szError : "Unknown error");
		FreeScene(pxScene);
		return false;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS, "GLTF_EXPORT: Successfully exported to %s", szOutputPath);

	// Clean up
	FreeScene(pxScene);
	return true;
}

bool ExportStaticMeshToGltf(
	const char* szOutputPath,
	const Zenith_MeshAsset* pxMesh)
{
	std::vector<const Flux_AnimationClip*> xEmpty;
	return ExportToGltf(szOutputPath, pxMesh, nullptr, xEmpty);
}

} // namespace Zenith_Tools_GltfExport
