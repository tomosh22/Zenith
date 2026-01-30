#include "Zenith.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

// Disable Zenith memory management for the entire file.
// All Assimp object allocations must use standard C++ allocator
// so that Assimp's destructors can properly free them.
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/scene.h>
#include <assimp/anim.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Zenith_Tools_AssimpConvert.h"

namespace Zenith_AssimpConvert
{

//=============================================================================
// Matrix Conversions
//=============================================================================
Zenith_Maths::Matrix4 AssimpToZenith(const aiMatrix4x4& xMat)
{
	Zenith_Maths::Matrix4 xOut;
	xOut[0][0] = xMat.a1; xOut[1][0] = xMat.a2; xOut[2][0] = xMat.a3; xOut[3][0] = xMat.a4;
	xOut[0][1] = xMat.b1; xOut[1][1] = xMat.b2; xOut[2][1] = xMat.b3; xOut[3][1] = xMat.b4;
	xOut[0][2] = xMat.c1; xOut[1][2] = xMat.c2; xOut[2][2] = xMat.c3; xOut[3][2] = xMat.c4;
	xOut[0][3] = xMat.d1; xOut[1][3] = xMat.d2; xOut[2][3] = xMat.d3; xOut[3][3] = xMat.d4;
	return xOut;
}

aiMatrix4x4 ZenithToAssimp(const Zenith_Maths::Matrix4& xMat)
{
	aiMatrix4x4 xOut;
	xOut.a1 = xMat[0][0]; xOut.a2 = xMat[1][0]; xOut.a3 = xMat[2][0]; xOut.a4 = xMat[3][0];
	xOut.b1 = xMat[0][1]; xOut.b2 = xMat[1][1]; xOut.b3 = xMat[2][1]; xOut.b4 = xMat[3][1];
	xOut.c1 = xMat[0][2]; xOut.c2 = xMat[1][2]; xOut.c3 = xMat[2][2]; xOut.c4 = xMat[3][2];
	xOut.d1 = xMat[0][3]; xOut.d2 = xMat[1][3]; xOut.d3 = xMat[2][3]; xOut.d4 = xMat[3][3];
	return xOut;
}

//=============================================================================
// Vector Conversions
//=============================================================================
Zenith_Maths::Vector3 AssimpToZenith(const aiVector3D& xVec)
{
	return Zenith_Maths::Vector3(xVec.x, xVec.y, xVec.z);
}

aiVector3D ZenithToAssimp(const Zenith_Maths::Vector3& xVec)
{
	return aiVector3D(xVec.x, xVec.y, xVec.z);
}

Zenith_Maths::Vector2 AssimpToZenith2D(const aiVector3D& xVec)
{
	return Zenith_Maths::Vector2(xVec.x, xVec.y);
}

aiVector3D ZenithToAssimp2D(const Zenith_Maths::Vector2& xVec)
{
	return aiVector3D(xVec.x, xVec.y, 0.0f);
}

//=============================================================================
// Color Conversions
//=============================================================================
Zenith_Maths::Vector4 AssimpToZenith(const aiColor4D& xColor)
{
	return Zenith_Maths::Vector4(xColor.r, xColor.g, xColor.b, xColor.a);
}

aiColor4D ZenithToAssimp(const Zenith_Maths::Vector4& xColor)
{
	return aiColor4D(xColor.r, xColor.g, xColor.b, xColor.a);
}

//=============================================================================
// Quaternion Conversions
//=============================================================================
Zenith_Maths::Quat AssimpToZenith(const aiQuaternion& xQuat)
{
	// GLM quaternion constructor order is (w, x, y, z)
	return Zenith_Maths::Quat(xQuat.w, xQuat.x, xQuat.y, xQuat.z);
}

aiQuaternion ZenithToAssimp(const Zenith_Maths::Quat& xQuat)
{
	// aiQuaternion constructor order is (w, x, y, z)
	return aiQuaternion(xQuat.w, xQuat.x, xQuat.y, xQuat.z);
}

//=============================================================================
// Helper: Calculate world transform
//=============================================================================
Zenith_Maths::Matrix4 CalculateNodeWorldTransform(aiNode* pxNode)
{
	Zenith_Maths::Matrix4 xWorldTransform = AssimpToZenith(pxNode->mTransformation);

	aiNode* pxParent = pxNode->mParent;
	while (pxParent != nullptr)
	{
		xWorldTransform = AssimpToZenith(pxParent->mTransformation) * xWorldTransform;
		pxParent = pxParent->mParent;
	}

	return xWorldTransform;
}

//=============================================================================
// Mesh: ZenithToAssimp
//=============================================================================
aiMesh* ZenithToAssimp(const Zenith_MeshAsset* pxMesh, const Zenith_SkeletonAsset* pxSkeleton)
{
	aiMesh* pxOut = new aiMesh();
	pxOut->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

	const uint32_t uNumVerts = pxMesh->GetNumVerts();
	pxOut->mNumVertices = uNumVerts;

	// Positions (required)
	pxOut->mVertices = new aiVector3D[uNumVerts];
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxOut->mVertices[i] = ZenithToAssimp(pxMesh->m_xPositions.Get(i));
	}

	// Normals
	if (pxMesh->m_xNormals.GetSize() > 0)
	{
		pxOut->mNormals = new aiVector3D[uNumVerts];
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxOut->mNormals[i] = ZenithToAssimp(pxMesh->m_xNormals.Get(i));
		}
	}

	// NOTE: Tangents/Bitangents skipped for glTF export.
	// glTF requires TANGENT to be VEC4 (xyz + handedness), but Assimp's aiMesh
	// only supports VEC3 tangents. This causes glTF validation to fail.
	// Blender can recalculate tangents on import if needed.

	// UVs
	if (pxMesh->m_xUVs.GetSize() > 0)
	{
		pxOut->mTextureCoords[0] = new aiVector3D[uNumVerts];
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			const Zenith_Maths::Vector2& xUV = pxMesh->m_xUVs.Get(i);
			pxOut->mTextureCoords[0][i] = aiVector3D(xUV.x, xUV.y, 0.0f);
		}
		pxOut->mNumUVComponents[0] = 2;
	}

	// Vertex Colors
	if (pxMesh->m_xColors.GetSize() > 0)
	{
		pxOut->mColors[0] = new aiColor4D[uNumVerts];
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxOut->mColors[0][i] = ZenithToAssimp(pxMesh->m_xColors.Get(i));
		}
	}

	// Faces (indices)
	const uint32_t uNumFaces = pxMesh->GetNumIndices() / 3;
	pxOut->mNumFaces = uNumFaces;
	pxOut->mFaces = new aiFace[uNumFaces];
	for (uint32_t i = 0; i < uNumFaces; i++)
	{
		aiFace& xFace = pxOut->mFaces[i];
		xFace.mNumIndices = 3;
		xFace.mIndices = new unsigned int[3];
		xFace.mIndices[0] = pxMesh->m_xIndices.Get(i * 3 + 0);
		xFace.mIndices[1] = pxMesh->m_xIndices.Get(i * 3 + 1);
		xFace.mIndices[2] = pxMesh->m_xIndices.Get(i * 3 + 2);
	}

	// Bones (if skeleton provided and mesh has skinning data)
	if (pxSkeleton && pxMesh->m_xBoneIndices.GetSize() > 0)
	{
		const uint32_t uNumBones = pxSkeleton->GetNumBones();
		pxOut->mNumBones = uNumBones;
		pxOut->mBones = new aiBone*[uNumBones];

		// Build per-bone vertex weight lists from per-vertex data
		std::vector<std::vector<aiVertexWeight>> axBoneWeights(uNumBones);

		for (uint32_t uVert = 0; uVert < uNumVerts; uVert++)
		{
			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(uVert);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(uVert);

			for (int j = 0; j < 4; j++)
			{
				if (xBoneWgt[j] > 0.0001f && xBoneIdx[j] < uNumBones)
				{
					aiVertexWeight xWeight;
					xWeight.mVertexId = uVert;
					xWeight.mWeight = xBoneWgt[j];
					axBoneWeights[xBoneIdx[j]].push_back(xWeight);
				}
			}
		}

		// Create aiBone for each bone
		for (uint32_t uBone = 0; uBone < uNumBones; uBone++)
		{
			const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);

			aiBone* pxAiBone = new aiBone();
			pxAiBone->mName = aiString(xBone.m_strName.c_str());
			pxAiBone->mOffsetMatrix = ZenithToAssimp(xBone.m_xInverseBindPose);

			// Copy vertex weights
			const auto& xWeights = axBoneWeights[uBone];
			pxAiBone->mNumWeights = static_cast<uint32_t>(xWeights.size());
			if (pxAiBone->mNumWeights > 0)
			{
				pxAiBone->mWeights = new aiVertexWeight[pxAiBone->mNumWeights];
				for (uint32_t w = 0; w < pxAiBone->mNumWeights; w++)
				{
					pxAiBone->mWeights[w] = xWeights[w];
				}
			}

			pxOut->mBones[uBone] = pxAiBone;
		}
	}

	return pxOut;
}

//=============================================================================
// Skeleton: ZenithToAssimp
//=============================================================================
aiNode* ZenithToAssimp(const Zenith_SkeletonAsset* pxSkeleton)
{
	const uint32_t uNumBones = pxSkeleton->GetNumBones();
	if (uNumBones == 0)
	{
		return nullptr;
	}

	// Create nodes for all bones
	std::vector<aiNode*> axNodes(uNumBones);
	for (uint32_t i = 0; i < uNumBones; i++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(i);

		aiNode* pxNode = new aiNode();
		pxNode->mName = aiString(xBone.m_strName.c_str());
		pxNode->mTransformation = ZenithToAssimp(xBone.m_xBindPoseLocal);
		axNodes[i] = pxNode;
	}

	// Count children per bone
	std::vector<uint32_t> auChildCounts(uNumBones, 0);
	for (uint32_t i = 0; i < uNumBones; i++)
	{
		int32_t iParent = pxSkeleton->GetBone(i).m_iParentIndex;
		if (iParent >= 0 && iParent < static_cast<int32_t>(uNumBones))
		{
			auChildCounts[iParent]++;
		}
	}

	// Allocate child arrays and link hierarchy
	std::vector<uint32_t> auChildIdx(uNumBones, 0);
	for (uint32_t i = 0; i < uNumBones; i++)
	{
		uint32_t uCount = auChildCounts[i];
		if (uCount > 0)
		{
			axNodes[i]->mNumChildren = uCount;
			axNodes[i]->mChildren = new aiNode*[uCount];
		}
	}

	// Link children to parents
	aiNode* pxRoot = nullptr;
	for (uint32_t i = 0; i < uNumBones; i++)
	{
		int32_t iParent = pxSkeleton->GetBone(i).m_iParentIndex;
		if (iParent >= 0 && iParent < static_cast<int32_t>(uNumBones))
		{
			uint32_t uIdx = auChildIdx[iParent]++;
			axNodes[iParent]->mChildren[uIdx] = axNodes[i];
			axNodes[i]->mParent = axNodes[iParent];
		}
		else
		{
			// Root bone - keep track of first root
			if (pxRoot == nullptr)
			{
				pxRoot = axNodes[i];
			}
		}
	}

	// If we have multiple roots, create a wrapper node
	Zenith_Vector<uint32_t> xRootBones = pxSkeleton->GetRootBones();
	if (xRootBones.GetSize() > 1)
	{
		aiNode* pxArmature = new aiNode();
		pxArmature->mName = aiString("Armature");
		pxArmature->mTransformation = aiMatrix4x4();
		pxArmature->mNumChildren = static_cast<uint32_t>(xRootBones.GetSize());
		pxArmature->mChildren = new aiNode*[xRootBones.GetSize()];

		for (uint32_t i = 0; i < xRootBones.GetSize(); i++)
		{
			pxArmature->mChildren[i] = axNodes[xRootBones.Get(i)];
			axNodes[xRootBones.Get(i)]->mParent = pxArmature;
		}
		return pxArmature;
	}

	return pxRoot;
}

//=============================================================================
// Bone Channel: ZenithToAssimp
//=============================================================================
aiNodeAnim* ZenithToAssimp(const Flux_BoneChannel& xChannel, const std::string& strBoneName)
{
	aiNodeAnim* pxOut = new aiNodeAnim();
	pxOut->mNodeName = aiString(strBoneName.c_str());

	// Position keyframes
	const auto& xPositions = xChannel.GetPositionKeyframes();
	pxOut->mNumPositionKeys = static_cast<uint32_t>(xPositions.size());
	if (pxOut->mNumPositionKeys > 0)
	{
		pxOut->mPositionKeys = new aiVectorKey[pxOut->mNumPositionKeys];
		for (uint32_t i = 0; i < pxOut->mNumPositionKeys; i++)
		{
			pxOut->mPositionKeys[i].mTime = xPositions[i].second;
			pxOut->mPositionKeys[i].mValue = ZenithToAssimp(xPositions[i].first);
		}
	}

	// Rotation keyframes
	const auto& xRotations = xChannel.GetRotationKeyframes();
	pxOut->mNumRotationKeys = static_cast<uint32_t>(xRotations.size());
	if (pxOut->mNumRotationKeys > 0)
	{
		pxOut->mRotationKeys = new aiQuatKey[pxOut->mNumRotationKeys];
		for (uint32_t i = 0; i < pxOut->mNumRotationKeys; i++)
		{
			pxOut->mRotationKeys[i].mTime = xRotations[i].second;
			pxOut->mRotationKeys[i].mValue = ZenithToAssimp(xRotations[i].first);
		}
	}

	// Scale keyframes
	const auto& xScales = xChannel.GetScaleKeyframes();
	pxOut->mNumScalingKeys = static_cast<uint32_t>(xScales.size());
	if (pxOut->mNumScalingKeys > 0)
	{
		pxOut->mScalingKeys = new aiVectorKey[pxOut->mNumScalingKeys];
		for (uint32_t i = 0; i < pxOut->mNumScalingKeys; i++)
		{
			pxOut->mScalingKeys[i].mTime = xScales[i].second;
			pxOut->mScalingKeys[i].mValue = ZenithToAssimp(xScales[i].first);
		}
	}

	return pxOut;
}

//=============================================================================
// Animation: ZenithToAssimp
//=============================================================================
aiAnimation* ZenithToAssimp(const Flux_AnimationClip* pxClip)
{
	aiAnimation* pxOut = new aiAnimation();
	pxOut->mName = aiString(pxClip->GetName().c_str());
	pxOut->mDuration = pxClip->GetDurationInTicks();
	pxOut->mTicksPerSecond = pxClip->GetTicksPerSecond();

	const auto& xChannels = pxClip->GetBoneChannels();
	pxOut->mNumChannels = static_cast<uint32_t>(xChannels.size());

	if (pxOut->mNumChannels > 0)
	{
		pxOut->mChannels = new aiNodeAnim*[pxOut->mNumChannels];

		uint32_t uIdx = 0;
		for (const auto& [strName, xChannel] : xChannels)
		{
			pxOut->mChannels[uIdx++] = ZenithToAssimp(xChannel, strName);
		}
	}

	return pxOut;
}

} // namespace Zenith_AssimpConvert
