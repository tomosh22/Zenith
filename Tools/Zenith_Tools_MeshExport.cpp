#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Zenith.h"
#include "Zenith_Tools_TextureExport.h"

// Extern function that must be implemented by game projects - returns just the project name (e.g., "Test")
// Paths are constructed using ZENITH_ROOT (defined by build system) + project name
extern const char* Project_GetName();

// Helper functions to construct asset paths from project name
static std::string GetGameAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Games/" + Project_GetName() + "/Assets/";
}

static std::string GetEngineAssetsDirectory()
{
	return std::string(ZENITH_ROOT) + "Zenith/Assets/";
}

#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb/stb_image.h"
#include <vector>
#include <unordered_map>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

//------------------------------------------------------------------------------
// Forward declarations for helper functions
//------------------------------------------------------------------------------
static void BuildBoneHierarchyFromNode(
	aiNode* pxNode,
	const aiScene* pxScene,
	Zenith_SkeletonAsset* pxSkelAsset,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToMeshBoneIndex,
	const std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose,
	int32_t iParentIndex
);

static bool NodeIsBone(
	aiNode* pxNode,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToIndex
);

//------------------------------------------------------------------------------
// Helper: Convert Assimp matrix to GLM matrix
//------------------------------------------------------------------------------
static Zenith_Maths::Matrix4 AssimpToGLM(const aiMatrix4x4& xAssimpMat)
{
	Zenith_Maths::Matrix4 xMat;
	xMat[0][0] = xAssimpMat.a1; xMat[1][0] = xAssimpMat.a2; xMat[2][0] = xAssimpMat.a3; xMat[3][0] = xAssimpMat.a4;
	xMat[0][1] = xAssimpMat.b1; xMat[1][1] = xAssimpMat.b2; xMat[2][1] = xAssimpMat.b3; xMat[3][1] = xAssimpMat.b4;
	xMat[0][2] = xAssimpMat.c1; xMat[1][2] = xAssimpMat.c2; xMat[2][2] = xAssimpMat.c3; xMat[3][2] = xAssimpMat.c4;
	xMat[0][3] = xAssimpMat.d1; xMat[1][3] = xAssimpMat.d2; xMat[2][3] = xAssimpMat.d3; xMat[3][3] = xAssimpMat.d4;
	return xMat;
}

//------------------------------------------------------------------------------
// Helper: Calculate world transform for a node by walking up the parent chain
//------------------------------------------------------------------------------
static Zenith_Maths::Matrix4 CalculateNodeWorldTransform(aiNode* pxNode)
{
	Zenith_Maths::Matrix4 xWorldTransform = AssimpToGLM(pxNode->mTransformation);

	aiNode* pxParent = pxNode->mParent;
	while (pxParent != nullptr)
	{
		xWorldTransform = AssimpToGLM(pxParent->mTransformation) * xWorldTransform;
		pxParent = pxParent->mParent;
	}

	return xWorldTransform;
}

//------------------------------------------------------------------------------
// Export a single mesh from Assimp to Zenith_MeshAsset
// xMeshNodeWorldTransform: The world transform of the mesh node, used to bake
// vertex positions into world space. For skinned meshes, this ensures vertices
// are positioned correctly at bind pose even when the mesh node has a transform.
//------------------------------------------------------------------------------
static void ExportAssimpMesh(
	aiMesh* pxAssimpMesh,
	const aiScene* pxScene,
	const std::string& strOutFilename,
	const std::string& strSkeletonPath,
	std::unordered_map<std::string, uint32_t>& xBoneNameToIndex,
	std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose,
	const Zenith_Maths::Matrix4& xMeshNodeWorldTransform)
{
	const bool bFlipWinding = false;

	Zenith_MeshAsset xMeshAsset;

	const uint32_t uNumVerts = pxAssimpMesh->mNumVertices;
	const uint32_t uNumIndices = pxAssimpMesh->mNumFaces * 3;

	const bool bHasPositions = pxAssimpMesh->mVertices != nullptr;
	const bool bHasUVs = pxAssimpMesh->mTextureCoords[0] != nullptr;
	const bool bHasNormals = pxAssimpMesh->mNormals != nullptr;
	const bool bHasTangents = pxAssimpMesh->mTangents != nullptr;
	const bool bHasBitangents = pxAssimpMesh->mBitangents != nullptr;
	const bool bHasBones = pxAssimpMesh->mNumBones > 0;
	const bool bHasVertexColors = pxAssimpMesh->mColors[0] != nullptr;

	// Extract material base color
	glm::vec4 xMaterialColor(1.0f, 1.0f, 1.0f, 1.0f);
	if (pxAssimpMesh->mMaterialIndex < pxScene->mNumMaterials)
	{
		const aiMaterial* pxMat = pxScene->mMaterials[pxAssimpMesh->mMaterialIndex];
		aiColor4D xDiffuseColor;
		if (AI_SUCCESS == pxMat->Get(AI_MATKEY_COLOR_DIFFUSE, xDiffuseColor))
		{
			xMaterialColor = glm::vec4(xDiffuseColor.r, xDiffuseColor.g, xDiffuseColor.b, xDiffuseColor.a);
		}
	}
	xMeshAsset.m_xMaterialColor = xMaterialColor;

	Zenith_Log("MESH_EXPORT: Exporting mesh to %s (Verts: %u, Indices: %u, Bones: %u, VertexColors: %s, MaterialColor: %.2f,%.2f,%.2f)",
		strOutFilename.c_str(), uNumVerts, uNumIndices, pxAssimpMesh->mNumBones,
		bHasVertexColors ? "Yes" : "No", xMaterialColor.r, xMaterialColor.g, xMaterialColor.b);

	// Reserve space for vertex data
	xMeshAsset.Reserve(uNumVerts, uNumIndices);

	// Build bone mapping from mesh bones to skeleton bones
	std::unordered_map<uint32_t, uint32_t> xMeshBoneToSkeletonBone; // mesh bone index -> skeleton bone index

	// We need the inverse of the mesh node world transform to adjust the inverse bind pose
	Zenith_Maths::Matrix4 xInverseMeshNodeWorldTransform = glm::inverse(xMeshNodeWorldTransform);

	if (bHasBones)
	{
		// First pass: validate bone count per vertex and collect bone data
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

			// Store inverse bind pose for this bone
			// The original mOffsetMatrix transforms from mesh-local space to bone-local space.
			// Since we're baking the mesh node transform into vertices (putting them in world space),
			// we need to adjust the inverse bind pose to transform from world space to bone-local space.
			// adjustedInvBindPose = originalInvBindPose * inverse(meshNodeWorldTransform)
			const aiMatrix4x4& xAssimpMat = pxBone->mOffsetMatrix;
			Zenith_Maths::Matrix4 xOriginalInvBindPose;
			xOriginalInvBindPose[0][0] = xAssimpMat.a1; xOriginalInvBindPose[1][0] = xAssimpMat.a2; xOriginalInvBindPose[2][0] = xAssimpMat.a3; xOriginalInvBindPose[3][0] = xAssimpMat.a4;
			xOriginalInvBindPose[0][1] = xAssimpMat.b1; xOriginalInvBindPose[1][1] = xAssimpMat.b2; xOriginalInvBindPose[2][1] = xAssimpMat.b3; xOriginalInvBindPose[3][1] = xAssimpMat.b4;
			xOriginalInvBindPose[0][2] = xAssimpMat.c1; xOriginalInvBindPose[1][2] = xAssimpMat.c2; xOriginalInvBindPose[2][2] = xAssimpMat.c3; xOriginalInvBindPose[3][2] = xAssimpMat.c4;
			xOriginalInvBindPose[0][3] = xAssimpMat.d1; xOriginalInvBindPose[1][3] = xAssimpMat.d2; xOriginalInvBindPose[2][3] = xAssimpMat.d3; xOriginalInvBindPose[3][3] = xAssimpMat.d4;

			// Adjust inverse bind pose: transforms from world space (baked) to bone-local space
			Zenith_Maths::Matrix4 xAdjustedInvBindPose = xOriginalInvBindPose * xInverseMeshNodeWorldTransform;
			xBoneNameToInvBindPose[pxBone->mName.C_Str()] = xAdjustedInvBindPose;

			Zenith_Log("MESH_EXPORT:   Bone '%s' inverse bind pose adjusted for baked mesh transform", pxBone->mName.C_Str());

			// Register bone name for skeleton extraction
			if (xBoneNameToIndex.find(pxBone->mName.C_Str()) == xBoneNameToIndex.end())
			{
				uint32_t uNewIndex = static_cast<uint32_t>(xBoneNameToIndex.size());
				xBoneNameToIndex[pxBone->mName.C_Str()] = uNewIndex;
			}
			xMeshBoneToSkeletonBone[uBoneIndex] = xBoneNameToIndex[pxBone->mName.C_Str()];
		}

		constexpr uint32_t kBonesPerVertexLimit = 4; // Same as Zenith_MeshAsset::BONES_PER_VERTEX_LIMIT
		for (u_int uVert = 0; uVert < uNumVerts; uVert++)
		{
			if (puVertexBoneCount[uVert] > kBonesPerVertexLimit)
			{
				Zenith_Log("Mesh has vertices with more than BONES_PER_VERTEX_LIMIT bone influences");
				delete[] puVertexBoneCount;
				return;
			}
		}

		delete[] puVertexBoneCount;

		// Set skeleton path for skinned mesh
		xMeshAsset.SetSkeletonPath(strSkeletonPath);
	}

	// Always bake mesh node transform into vertices
	// For skinned meshes, we compensate by adjusting the inverse bind pose (see above)
	// This ensures vertices are always in world space at bind pose
	bool bBakeTransform = true;

	// Calculate normal matrix (inverse transpose of upper 3x3) for transforming normals
	Zenith_Maths::Matrix3 xNormalMatrix = glm::transpose(glm::inverse(Zenith_Maths::Matrix3(xMeshNodeWorldTransform)));

	Zenith_Log("MESH_EXPORT:   Baking mesh node transform into vertices");

	// Add vertex data
	for (u_int i = 0; i < pxAssimpMesh->mNumVertices; i++)
	{
		Zenith_Maths::Vector3 xPosition(0);
		Zenith_Maths::Vector3 xNormal(0, 1, 0);
		Zenith_Maths::Vector2 xUV(0);
		Zenith_Maths::Vector3 xTangent(1, 0, 0);
		Zenith_Maths::Vector4 xColor = xMaterialColor;

		if (bHasPositions)
		{
			const aiVector3D& xPos = pxAssimpMesh->mVertices[i];
			Zenith_Maths::Vector3 xLocalPos(xPos.x, xPos.y, xPos.z);

			if (bBakeTransform)
			{
				// Transform vertex from mesh-local space to world space (for non-skinned meshes)
				Zenith_Maths::Vector4 xWorldPos = xMeshNodeWorldTransform * Zenith_Maths::Vector4(xLocalPos, 1.0f);
				xPosition = Zenith_Maths::Vector3(xWorldPos);
			}
			else
			{
				// Keep mesh-local position (for skinned meshes)
				xPosition = xLocalPos;
			}
		}

		if (bHasUVs)
		{
			const aiVector3D& xUVData = pxAssimpMesh->mTextureCoords[0][i];
			xUV = glm::vec2(xUVData.x, xUVData.y);
		}

		if (bHasNormals)
		{
			const aiVector3D& xNormalData = pxAssimpMesh->mNormals[i];
			Zenith_Maths::Vector3 xLocalNormal(xNormalData.x, xNormalData.y, xNormalData.z);
			if (bBakeTransform)
			{
				// Transform normal by normal matrix (handles non-uniform scaling correctly)
				xNormal = glm::normalize(xNormalMatrix * xLocalNormal);
			}
			else
			{
				xNormal = glm::normalize(xLocalNormal);
			}
		}

		if (bHasTangents)
		{
			const aiVector3D& xTangentData = pxAssimpMesh->mTangents[i];
			Zenith_Maths::Vector3 xLocalTangent(xTangentData.x, xTangentData.y, xTangentData.z);
			if (bBakeTransform)
			{
				// Transform tangent by normal matrix
				xTangent = glm::normalize(xNormalMatrix * xLocalTangent);
			}
			else
			{
				xTangent = glm::normalize(xLocalTangent);
			}
		}

		if (bHasVertexColors)
		{
			const aiColor4D& xColorData = pxAssimpMesh->mColors[0][i];
			xColor = glm::vec4(xColorData.r, xColorData.g, xColorData.b, xColorData.a);
		}

		xMeshAsset.AddVertex(xPosition, xNormal, xUV, xTangent, xColor);
	}

	// Add bitangents if present (must be done after AddVertex since it's separate from the main call)
	if (bHasBitangents || bHasBones)
	{
		for (u_int i = 0; i < pxAssimpMesh->mNumVertices; i++)
		{
			Zenith_Maths::Vector3 xBitangent(0, 1, 0);
			if (bHasBitangents)
			{
				const aiVector3D& xBitangentData = pxAssimpMesh->mBitangents[i];
				Zenith_Maths::Vector3 xLocalBitangent(xBitangentData.x, xBitangentData.y, xBitangentData.z);
				if (bBakeTransform)
				{
					// Transform bitangent by normal matrix
					xBitangent = glm::normalize(xNormalMatrix * xLocalBitangent);
				}
				else
				{
					xBitangent = glm::normalize(xLocalBitangent);
				}
			}
			xMeshAsset.m_xBitangents.PushBack(xBitangent);
		}
	}

	// Add skinning data
	if (bHasBones)
	{
		constexpr uint32_t kBonesPerVertexLimit = 4; // Same as Zenith_MeshAsset::BONES_PER_VERTEX_LIMIT

		// Initialize bone data for all vertices
		for (u_int i = 0; i < uNumVerts; i++)
		{
			xMeshAsset.SetVertexSkinning(i, glm::uvec4(0), glm::vec4(0.0f));
		}

		// Temporary storage for bone weights per vertex
		std::vector<std::vector<std::pair<uint32_t, float>>> xVertexBoneData(uNumVerts);

		for (u_int uBoneIndex = 0; uBoneIndex < pxAssimpMesh->mNumBones; uBoneIndex++)
		{
			const aiBone* pxBone = pxAssimpMesh->mBones[uBoneIndex];
			uint32_t uSkeletonBoneIndex = xMeshBoneToSkeletonBone[uBoneIndex];

			for (u_int uWeightIndex = 0; uWeightIndex < pxBone->mNumWeights; uWeightIndex++)
			{
				const aiVertexWeight& xWeight = pxBone->mWeights[uWeightIndex];
				xVertexBoneData[xWeight.mVertexId].push_back({ uSkeletonBoneIndex, xWeight.mWeight });
			}
		}

		// Set bone data for each vertex
		for (u_int uVert = 0; uVert < uNumVerts; uVert++)
		{
			glm::uvec4 xBoneIndices(0);
			glm::vec4 xBoneWeights(0.0f);

			const auto& xBones = xVertexBoneData[uVert];
			float fTotalWeight = 0.0f;

			for (size_t uSlot = 0; uSlot < (std::min)(xBones.size(), static_cast<size_t>(kBonesPerVertexLimit)); uSlot++)
			{
				xBoneIndices[static_cast<int>(uSlot)] = xBones[uSlot].first;
				xBoneWeights[static_cast<int>(uSlot)] = xBones[uSlot].second;
				fTotalWeight += xBones[uSlot].second;
			}

			// Normalize weights
			if (fTotalWeight > 0.0001f)
			{
				xBoneWeights /= fTotalWeight;
			}

			xMeshAsset.SetVertexSkinning(uVert, xBoneIndices, xBoneWeights);
		}
	}

	// Add indices
	for (u_int i = 0; i < pxAssimpMesh->mNumFaces; i++)
	{
		const aiFace& xFace = pxAssimpMesh->mFaces[i];
		if (xFace.mNumIndices != 3)
		{
			Zenith_Log("Face is not a triangle, aborting");
			return;
		}

		uint32_t uIdx0 = xFace.mIndices[0];
		uint32_t uIdx1 = xFace.mIndices[bFlipWinding ? 1 : 2];
		uint32_t uIdx2 = xFace.mIndices[bFlipWinding ? 2 : 1];
		xMeshAsset.AddTriangle(uIdx0, uIdx1, uIdx2);
	}

	// Add a single submesh covering all indices with this mesh's material
	xMeshAsset.AddSubmesh(0, uNumIndices, pxAssimpMesh->mMaterialIndex);

	// Compute bounds
	xMeshAsset.ComputeBounds();

	// Export to file
	xMeshAsset.Export(strOutFilename.c_str());

	Zenith_Log("MESH_EXPORT: Successfully exported %s", strOutFilename.c_str());
}

//------------------------------------------------------------------------------
// Helper: Check if a node is a bone (referenced by any mesh in the scene)
//------------------------------------------------------------------------------
static bool NodeIsBone(
	aiNode* pxNode,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToIndex)
{
	return xBoneNameToIndex.find(pxNode->mName.C_Str()) != xBoneNameToIndex.end();
}

//------------------------------------------------------------------------------
// Helper: Check if a node or any descendant is a bone
//------------------------------------------------------------------------------
static bool NodeOrDescendantIsBone(
	aiNode* pxNode,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToIndex)
{
	if (NodeIsBone(pxNode, xBoneNameToIndex))
		return true;

	for (uint32_t i = 0; i < pxNode->mNumChildren; i++)
	{
		if (NodeOrDescendantIsBone(pxNode->mChildren[i], xBoneNameToIndex))
			return true;
	}
	return false;
}

//------------------------------------------------------------------------------
// Build bone hierarchy from Assimp scene graph
// IMPORTANT: Only includes actual bones (nodes with mOffsetMatrix from Assimp).
// Non-bone ancestors (like Armature) are skipped because:
// - Assimp's mOffsetMatrix is relative to mesh space, not scene space
// - Including Armature's transform would cause a coordinate space mismatch
// - The mOffsetMatrix already accounts for getting from mesh space to bone space
//
// CRITICAL: Bones must be added in the same order as xBoneNameToMeshBoneIndex
// because mesh vertex skinning data uses those indices. We use a two-pass approach:
// 1. First pass: pre-allocate all bones with placeholder data in mesh index order
// 2. Second pass: fill in actual bone data (parent, transforms) from scene graph
//------------------------------------------------------------------------------

// Helper struct for second pass bone data collection
struct BoneNodeData
{
	std::string strName;
	int32_t iParentIndex = Zenith_SkeletonAsset::INVALID_BONE_INDEX;
	Zenith_Maths::Vector3 xPosition = Zenith_Maths::Vector3(0);
	Zenith_Maths::Quat xRotation = glm::identity<Zenith_Maths::Quat>();
	Zenith_Maths::Vector3 xScale = Zenith_Maths::Vector3(1);
};

// Helper: Calculate the accumulated transform from non-bone ancestors
// This is used when a bone becomes a skeleton root but has non-bone ancestors
static Zenith_Maths::Matrix4 CalculateNonBoneAncestorTransform(
	aiNode* pxNode,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToMeshBoneIndex)
{
	Zenith_Maths::Matrix4 xAccumulated = glm::identity<Zenith_Maths::Matrix4>();

	aiNode* pxParent = pxNode->mParent;
	while (pxParent != nullptr)
	{
		// Stop if we hit a bone - we only want non-bone ancestors
		if (xBoneNameToMeshBoneIndex.find(pxParent->mName.C_Str()) != xBoneNameToMeshBoneIndex.end())
			break;

		// Accumulate this non-bone ancestor's transform
		xAccumulated = AssimpToGLM(pxParent->mTransformation) * xAccumulated;
		pxParent = pxParent->mParent;
	}

	return xAccumulated;
}

// Recursive function to collect bone data from scene graph
static void CollectBoneDataFromNode(
	aiNode* pxNode,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToMeshBoneIndex,
	std::vector<BoneNodeData>& xBoneDataArray,
	int32_t iParentSkeletonIndex)
{
	std::string strNodeName = pxNode->mName.C_Str();

	// Check if this node is an actual bone
	auto it = xBoneNameToMeshBoneIndex.find(strNodeName);
	bool bIsBone = (it != xBoneNameToMeshBoneIndex.end());
	bool bHasBoneDescendant = NodeOrDescendantIsBone(pxNode, xBoneNameToMeshBoneIndex);

	// Skip nodes that aren't bones and don't have bone descendants
	if (!bIsBone && !bHasBoneDescendant)
		return;

	int32_t iChildParentIndex = iParentSkeletonIndex;

	if (bIsBone)
	{
		uint32_t uMeshBoneIndex = it->second;

		// Use local transform relative to scene graph parent for ALL bones
		// We do NOT bake non-bone ancestor transforms into root bone TRS because:
		// 1. Mesh vertices are already baked into world space (with adjusted inverse bind pose)
		// 2. Animation keyframes are relative to scene graph parents (including non-bone ancestors)
		// 3. If we baked ancestor transforms here, animations would "unbake" them when replacing TRS
		Zenith_Maths::Matrix4 xBoneTransform = AssimpToGLM(pxNode->mTransformation);

		if (iParentSkeletonIndex == Zenith_SkeletonAsset::INVALID_BONE_INDEX)
		{
			Zenith_Log("SKELETON_BUILD: Root bone '%s' - using local transform (no ancestor baking)", strNodeName.c_str());
		}

		// Decompose to TRS
		// Note: GLM decompose is more complex, so we do a simple decomposition
		Zenith_Maths::Vector3 xPosition = Zenith_Maths::Vector3(xBoneTransform[3]);

		// Extract scale from column lengths
		Zenith_Maths::Vector3 xCol0(xBoneTransform[0]);
		Zenith_Maths::Vector3 xCol1(xBoneTransform[1]);
		Zenith_Maths::Vector3 xCol2(xBoneTransform[2]);
		float fScaleX = glm::length(xCol0);
		float fScaleY = glm::length(xCol1);
		float fScaleZ = glm::length(xCol2);
		if (fScaleX < 0.0001f) fScaleX = 1.0f;
		if (fScaleY < 0.0001f) fScaleY = 1.0f;
		if (fScaleZ < 0.0001f) fScaleZ = 1.0f;
		Zenith_Maths::Vector3 xScale(fScaleX, fScaleY, fScaleZ);

		// Extract rotation by normalizing basis vectors
		Zenith_Maths::Matrix3 xRotMat(
			xCol0 / fScaleX,
			xCol1 / fScaleY,
			xCol2 / fScaleZ
		);
		Zenith_Maths::Quat xRotation = glm::quat_cast(xRotMat);

		// Store bone data at the correct index (matching mesh bone index)
		BoneNodeData& xData = xBoneDataArray[uMeshBoneIndex];
		xData.strName = strNodeName;
		xData.iParentIndex = iParentSkeletonIndex;
		xData.xPosition = xPosition;
		xData.xRotation = xRotation;
		xData.xScale = xScale;

		// Children of this bone should have this bone as their parent
		// Use the MESH bone index as the skeleton index (they must match!)
		iChildParentIndex = static_cast<int32_t>(uMeshBoneIndex);
	}

	// Process children
	for (uint32_t i = 0; i < pxNode->mNumChildren; i++)
	{
		CollectBoneDataFromNode(
			pxNode->mChildren[i],
			xBoneNameToMeshBoneIndex,
			xBoneDataArray,
			iChildParentIndex
		);
	}
}

static void BuildBoneHierarchyFromNode(
	aiNode* pxNode,
	const aiScene* pxScene,
	Zenith_SkeletonAsset* pxSkelAsset,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToMeshBoneIndex,
	const std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose,
	int32_t /*iParentIndex*/)
{
	// Two-pass approach to ensure skeleton bone indices match mesh bone indices

	// Pass 1: Collect all bone data from scene graph into array indexed by mesh bone index
	uint32_t uNumBones = static_cast<uint32_t>(xBoneNameToMeshBoneIndex.size());
	std::vector<BoneNodeData> xBoneDataArray(uNumBones);

	CollectBoneDataFromNode(pxNode, xBoneNameToMeshBoneIndex, xBoneDataArray, Zenith_SkeletonAsset::INVALID_BONE_INDEX);

	// Pass 2: Add bones to skeleton in mesh index order (0, 1, 2, ...)
	for (uint32_t u = 0; u < uNumBones; u++)
	{
		const BoneNodeData& xData = xBoneDataArray[u];

		// Add bone - it will get index u (matching mesh bone index)
		uint32_t uBoneIndex = pxSkelAsset->AddBone(xData.strName, xData.iParentIndex, xData.xPosition, xData.xRotation, xData.xScale);
		Zenith_Assert(uBoneIndex == u, "Skeleton bone index mismatch! Expected %u, got %u", u, uBoneIndex);

		// Set inverse bind pose from Assimp's mOffsetMatrix
		auto invBindIt = xBoneNameToInvBindPose.find(xData.strName);
		if (invBindIt != xBoneNameToInvBindPose.end())
		{
			pxSkelAsset->SetInverseBindPose(uBoneIndex, invBindIt->second);
		}
	}

	Zenith_Log("SKELETON_BUILD: Built skeleton with %u bones in mesh index order", uNumBones);
}

//------------------------------------------------------------------------------
// Extract skeleton from scene if meshes have bones
//------------------------------------------------------------------------------
static void ExtractSkeleton(
	const aiScene* pxScene,
	const std::string& strSkeletonPath,
	const std::unordered_map<std::string, uint32_t>& xBoneNameToIndex,
	const std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose)
{
	if (xBoneNameToIndex.empty())
		return;

	Zenith_SkeletonAsset xSkelAsset;

	// Build skeleton hierarchy from scene graph
	BuildBoneHierarchyFromNode(
		pxScene->mRootNode,
		pxScene,
		&xSkelAsset,
		xBoneNameToIndex,
		xBoneNameToInvBindPose,
		Zenith_SkeletonAsset::INVALID_BONE_INDEX
	);

	// Compute bind pose matrices
	xSkelAsset.ComputeBindPoseMatrices();

	// Export
	xSkelAsset.Export(strSkeletonPath.c_str());

	Zenith_Log("SKELETON_EXPORT: Successfully exported %s (%u bones)",
		strSkeletonPath.c_str(), xSkelAsset.GetNumBones());
}

//------------------------------------------------------------------------------
// Process node and its children to export meshes
//------------------------------------------------------------------------------
struct MeshExportInfo
{
	std::string m_strMeshPath;
	uint32_t m_uMaterialIndex;
};

static void ProcessNode(
	aiNode* pxNode,
	const aiScene* pxScene,
	const std::string& strExtension,
	const std::string& strBaseFilename,
	uint32_t& uMeshIndex,
	const std::string& strSkeletonPath,
	std::unordered_map<std::string, uint32_t>& xBoneNameToIndex,
	std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose,
	std::vector<MeshExportInfo>& xExportedMeshes,
	const char* szExportFilenameOverride = nullptr)
{
	// Calculate this node's world transform for baking into mesh vertices
	Zenith_Maths::Matrix4 xNodeWorldTransform = CalculateNodeWorldTransform(pxNode);

	for (uint32_t u = 0; u < pxNode->mNumMeshes; u++)
	{
		aiMesh* pxAssimpMesh = pxScene->mMeshes[pxNode->mMeshes[u]];
		std::string strExportFilename = szExportFilenameOverride ? szExportFilenameOverride : strBaseFilename;
		size_t ulFindPos = strExportFilename.find(strExtension.c_str());
		Zenith_Assert(ulFindPos != std::string::npos, "Extension not found in filename");
		strExportFilename.replace(ulFindPos, strlen(strExtension.c_str()),
			"_Mesh" + std::to_string(uMeshIndex++) + "_Mat" + std::to_string(pxAssimpMesh->mMaterialIndex) + ZENITH_MESH_EXT);

		// Pass node's world transform to bake vertex positions into world space
		ExportAssimpMesh(pxAssimpMesh, pxScene, strExportFilename, strSkeletonPath, xBoneNameToIndex, xBoneNameToInvBindPose, xNodeWorldTransform);

		// Track exported mesh for model asset
		MeshExportInfo xInfo;
		xInfo.m_strMeshPath = strExportFilename;
		xInfo.m_uMaterialIndex = pxAssimpMesh->mMaterialIndex;
		xExportedMeshes.push_back(xInfo);
	}

	for (uint32_t u = 0; u < pxNode->mNumChildren; u++)
	{
		ProcessNode(pxNode->mChildren[u], pxScene, strExtension, strBaseFilename, uMeshIndex,
			strSkeletonPath, xBoneNameToIndex, xBoneNameToInvBindPose, xExportedMeshes, szExportFilenameOverride);
	}
}

//------------------------------------------------------------------------------
// Material type names for texture export
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// Export material textures
//------------------------------------------------------------------------------
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
		strExportFile += ZENITH_TEXTURE_EXT;

		// Use BC1 compression for better GPU performance
		Zenith_Tools_TextureExport::ExportFromDataCompressed(pData, strExportFile, iWidth, iHeight, TextureCompressionMode::BC1);
		stbi_image_free(pData);
	}
}

//------------------------------------------------------------------------------
// Collect bone info from all meshes for skeleton extraction
//------------------------------------------------------------------------------
static void CollectBoneInfo(
	const aiScene* pxScene,
	std::unordered_map<std::string, uint32_t>& xBoneNameToIndex,
	std::unordered_map<std::string, Zenith_Maths::Matrix4>& xBoneNameToInvBindPose)
{
	for (uint32_t uMesh = 0; uMesh < pxScene->mNumMeshes; uMesh++)
	{
		const aiMesh* pxMesh = pxScene->mMeshes[uMesh];
		for (uint32_t uBone = 0; uBone < pxMesh->mNumBones; uBone++)
		{
			const aiBone* pxBone = pxMesh->mBones[uBone];
			std::string strBoneName = pxBone->mName.C_Str();

			if (xBoneNameToIndex.find(strBoneName) == xBoneNameToIndex.end())
			{
				uint32_t uIndex = static_cast<uint32_t>(xBoneNameToIndex.size());
				xBoneNameToIndex[strBoneName] = uIndex;

				// Store inverse bind pose
				const aiMatrix4x4& xAssimpMat = pxBone->mOffsetMatrix;
				Zenith_Maths::Matrix4 xMat;
				xMat[0][0] = xAssimpMat.a1; xMat[1][0] = xAssimpMat.a2; xMat[2][0] = xAssimpMat.a3; xMat[3][0] = xAssimpMat.a4;
				xMat[0][1] = xAssimpMat.b1; xMat[1][1] = xAssimpMat.b2; xMat[2][1] = xAssimpMat.b3; xMat[3][1] = xAssimpMat.b4;
				xMat[0][2] = xAssimpMat.c1; xMat[1][2] = xAssimpMat.c2; xMat[2][2] = xAssimpMat.c3; xMat[3][2] = xAssimpMat.c4;
				xMat[0][3] = xAssimpMat.d1; xMat[1][3] = xAssimpMat.d2; xMat[2][3] = xAssimpMat.d3; xMat[3][3] = xAssimpMat.d4;
				xBoneNameToInvBindPose[strBoneName] = xMat;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Extract and export animations from scene
//------------------------------------------------------------------------------
static void ExtractAnimations(
	const aiScene* pxScene,
	const std::string& strBaseName)
{
	if (pxScene->mNumAnimations == 0)
	{
		Zenith_Log("ANIM_EXPORT: No animations found in scene");
		return;
	}

	Zenith_Log("ANIM_EXPORT: Found %u animations", pxScene->mNumAnimations);

	for (uint32_t uAnim = 0; uAnim < pxScene->mNumAnimations; uAnim++)
	{
		const aiAnimation* pxAnim = pxScene->mAnimations[uAnim];

		// Create animation clip
		Flux_AnimationClip xClip;
		xClip.LoadFromAssimp(pxAnim, pxScene->mRootNode);

		// Generate output filename
		std::string strAnimName = pxAnim->mName.C_Str();
		if (strAnimName.empty())
		{
			strAnimName = "Animation_" + std::to_string(uAnim);
		}

		// Sanitize animation name for use in filename
		for (size_t i = 0; i < strAnimName.size(); i++)
		{
			char c = strAnimName[i];
			if (!isalnum(c) && c != '_' && c != '-')
			{
				strAnimName[i] = '_';
			}
		}

		std::string strAnimPath = strBaseName + "_" + strAnimName + ZENITH_ANIMATION_EXT;

		// Export the animation
		xClip.Export(strAnimPath);

		Zenith_Log("ANIM_EXPORT: Exported '%s' to %s (Duration: %.2fs, Channels: %u)",
			pxAnim->mName.C_Str(),
			strAnimPath.c_str(),
			xClip.GetDuration(),
			static_cast<uint32_t>(xClip.GetBoneChannels().size()));
	}
}

//------------------------------------------------------------------------------
// Main export function
//------------------------------------------------------------------------------
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

	// Derive base name for outputs
	std::string strBaseName = strFilename;
	size_t ulDotPos = strBaseName.rfind('.');
	if (ulDotPos != std::string::npos)
	{
		strBaseName = strBaseName.substr(0, ulDotPos);
	}

	// Model name (just the filename without path and extension)
	std::filesystem::path xPath(strFilename);
	std::string strModelName = xPath.stem().string();

	// Export material textures
	for (uint32_t u = 0; u < pxScene->mNumMaterials; u++)
	{
		ExportMaterialTextures(pxScene->mMaterials[u], pxScene, strFilename, u);
	}

	// Maps to be populated by mesh processing
	// ProcessNode will populate both xBoneNameToIndex and xBoneNameToInvBindPose
	// with ADJUSTED inverse bind poses that account for baked mesh transforms
	std::unordered_map<std::string, uint32_t> xBoneNameToIndex;
	std::unordered_map<std::string, Zenith_Maths::Matrix4> xBoneNameToInvBindPose;

	// Determine skeleton path (we'll set it even if we don't know if there are bones yet,
	// ProcessNode will use it for skinned meshes)
	std::string strSkeletonPath = strBaseName + ZENITH_SKELETON_EXT;

	// Export meshes FIRST - this populates xBoneNameToIndex and xBoneNameToInvBindPose
	// with the adjusted inverse bind poses that account for baked mesh transforms
	std::vector<MeshExportInfo> xExportedMeshes;
	uint32_t uRootIndex = 0;
	ProcessNode(pxScene->mRootNode, pxScene, strExtension, strFilename, uRootIndex,
		strSkeletonPath, xBoneNameToIndex, xBoneNameToInvBindPose, xExportedMeshes, szExportFilenameOverride);

	// Now we know if there are bones
	bool bHasSkeleton = !xBoneNameToIndex.empty();

	// Export skeleton if we have bones (using the adjusted inverse bind poses from mesh export)
	if (bHasSkeleton)
	{
		ExtractSkeleton(pxScene, strSkeletonPath, xBoneNameToIndex, xBoneNameToInvBindPose);
	}

	// Export animations
	ExtractAnimations(pxScene, strBaseName);

	// Create and export model asset
	Zenith_ModelAsset xModelAsset;
	xModelAsset.SetName(strModelName);

	if (bHasSkeleton)
	{
		xModelAsset.SetSkeletonPath(strSkeletonPath);
	}

	// Add mesh bindings
	for (const auto& xMeshInfo : xExportedMeshes)
	{
		Zenith_Vector<std::string> xMaterialPaths;
		// For now, we don't export material files (keep existing texture export logic)
		// Material paths would be added here when material export is implemented
		xModelAsset.AddMesh(xMeshInfo.m_strMeshPath, xMaterialPaths);
	}

	// Export model asset
	std::string strModelPath = strBaseName + ZENITH_MODEL_EXT;
	xModelAsset.Export(strModelPath.c_str());

	Zenith_Log("MODEL_EXPORT: Successfully exported %s (Meshes: %zu, Skeleton: %s)",
		strModelPath.c_str(), xExportedMeshes.size(), bHasSkeleton ? "Yes" : "No");
}

//------------------------------------------------------------------------------
// Export all meshes from game assets directory
//------------------------------------------------------------------------------
void ExportAllMeshes()
{
	for (auto& xFile : std::filesystem::recursive_directory_iterator(GetGameAssetsDirectory()))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';

		// Avoid trying to export C++ IR files (.obj)
		if (!strstr(szFilename, "Assets"))
		{
			delete[] szFilename;
			continue;
		}

		// Is this a gltf
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".gltf"), ".gltf"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".gltf");
		}

		// Is this an fbx
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".fbx"), ".fbx"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".fbx");
		}

		// Is this an obj
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".obj"), ".obj"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".obj");
		}

		delete[] szFilename;
	}

	for (auto& xFile : std::filesystem::recursive_directory_iterator(GetEngineAssetsDirectory()))
	{
		const wchar_t* wszFilename = xFile.path().c_str();
		size_t ulLength = wcslen(wszFilename);
		char* szFilename = new char[ulLength + 1];
		wcstombs(szFilename, wszFilename, ulLength);
		szFilename[ulLength] = '\0';

		// Avoid trying to export C++ IR files (.obj)
		if (!strstr(szFilename, "Assets"))
		{
			delete[] szFilename;
			continue;
		}

		// Is this a gltf
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".gltf"), ".gltf"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".gltf");
		}

		// Is this an fbx
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".fbx"), ".fbx"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".fbx");
		}

		// Is this an obj
		if (!strcmp(szFilename + strlen(szFilename) - strlen(".obj"), ".obj"))
		{
			std::string strFilename(szFilename);
			Export(strFilename, ".obj");
		}

		delete[] szFilename;
	}
}
