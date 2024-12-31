#include "Zenith.h"
#include "Flux_MeshAnimation.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include <assimp/anim.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Flux_MeshAnimation::AnimBone::AnimBone(const std::string& strName, uint32_t uID, const aiNodeAnim* pxChannel)
	: m_strName(strName)
	, m_uID(uID)
{
	m_uNumPositions = pxChannel->mNumPositionKeys;
	m_xPositions.resize(m_uNumPositions);
	for (uint32_t u = 0; u < m_uNumPositions; u++)
	{
		const aiVector3D& xPosition = pxChannel->mPositionKeys[u].mValue;
		const float fTimestamp = pxChannel->mPositionKeys[u].mTime;
		m_xPositions.at(u) = { {xPosition.x, xPosition.y, xPosition.z}, fTimestamp };
	}

	m_uNumRotations = pxChannel->mNumRotationKeys;
	m_xRotations.resize(m_uNumRotations);
	for (uint32_t u = 0; u < m_uNumRotations; u++)
	{
		const aiQuaternion& xRotation = pxChannel->mRotationKeys[u].mValue;
		const float fTimestamp = pxChannel->mRotationKeys[u].mTime;
		m_xRotations.at(u) = { {xRotation.w, xRotation.x, xRotation.y, xRotation.z}, fTimestamp };
	}

	m_uNumScales = pxChannel->mNumScalingKeys;
	m_xScales.resize(m_uNumScales);
	for (uint32_t u = 0; u < m_uNumScales; u++)
	{
		const aiVector3D& xScale = pxChannel->mScalingKeys[u].mValue;
		const float fTimestamp = pxChannel->mScalingKeys[u].mTime;
		m_xScales.at(u) = { {xScale.x, xScale.y, xScale.z}, fTimestamp };
	}
}

static void ReadHierarchy(Flux_MeshAnimation::Node& xDst, const aiNode& xSrc, Flux_MeshGeometry& xParentGeometry)
{
	xDst.m_xTrans[0][0] = xSrc.mTransformation.a1;
	xDst.m_xTrans[1][0] = xSrc.mTransformation.a2;
	xDst.m_xTrans[2][0] = xSrc.mTransformation.a3;
	xDst.m_xTrans[3][0] = xSrc.mTransformation.a4;
	xDst.m_xTrans[0][1] = xSrc.mTransformation.b1;
	xDst.m_xTrans[1][1] = xSrc.mTransformation.b2;
	xDst.m_xTrans[2][1] = xSrc.mTransformation.b3;
	xDst.m_xTrans[3][1] = xSrc.mTransformation.b4;
	xDst.m_xTrans[0][2] = xSrc.mTransformation.c1;
	xDst.m_xTrans[1][2] = xSrc.mTransformation.c2;
	xDst.m_xTrans[2][2] = xSrc.mTransformation.c3;
	xDst.m_xTrans[3][2] = xSrc.mTransformation.c4;
	xDst.m_xTrans[0][3] = xSrc.mTransformation.d1;
	xDst.m_xTrans[1][3] = xSrc.mTransformation.d2;
	xDst.m_xTrans[2][3] = xSrc.mTransformation.d3;
	xDst.m_xTrans[3][3] = xSrc.mTransformation.d4;

	xDst.m_uChildCount = xSrc.mNumChildren;

	xDst.m_strName = std::string(xSrc.mName.data);

	for (uint32_t u = 0; u < xSrc.mNumChildren; u++)
	{
		Flux_MeshAnimation::Node xNewNode;
		ReadHierarchy(xNewNode, *xSrc.mChildren[u], xParentGeometry);
		xDst.m_xChildren.push_back(xNewNode);
	}
}

void Flux_MeshAnimation::CalculateBoneTransform(const Node* node, glm::mat4 parentTransform)
{
	std::string nodeName = node->m_strName;
	Zenith_Maths::Matrix4 nodeTransform = node->m_xTrans;

	AnimBone* Bone = nullptr;
	for (AnimBone& xBone : m_xBones)
	{
		if (xBone.m_strName == nodeName)
		{
			Bone = &xBone;
		}
	}


	if (Bone)
	{
		Bone->Update(m_fCurrentTimestamp);
		nodeTransform = Bone->m_xLocalTransform;
	}

	glm::mat4 globalTransformation = parentTransform * nodeTransform;

	auto boneInfoMap = m_xParentGeometry.m_xBoneNameToIdAndOffset;
	if (boneInfoMap.find(nodeName) != boneInfoMap.end())
	{
		int index = boneInfoMap.at(nodeName).first;
		glm::mat4 offset = boneInfoMap[nodeName].second;
		m_axAnimMatrices[index] = globalTransformation * offset;
	}

	for (int i = 0; i < node->m_uChildCount; i++)
		CalculateBoneTransform(&node->m_xChildren[i], globalTransformation);

	Flux_MemoryManager::UploadBufferData(m_xBoneBuffer.GetBuffer(), m_axAnimMatrices, sizeof(m_axAnimMatrices));
}

Flux_MeshAnimation::Flux_MeshAnimation(const std::string& strPath, Flux_MeshGeometry& xParentGeometry)
	: m_xParentGeometry(xParentGeometry)
{

	for (auto& xIt : m_axAnimMatrices)
	{
		xIt = glm::identity<Zenith_Maths::Matrix4>();
	}


	Assimp::Importer xImporter;
	const aiScene* pxScene = xImporter.ReadFile(strPath, aiProcess_Triangulate);
	aiAnimation* pxAnimation = pxScene->mAnimations[0];
	m_fDuration = pxAnimation->mDuration;
	m_uTicksPerSecond = pxAnimation->mTicksPerSecond;
	

	



	int size = pxAnimation->mNumChannels;

	std::unordered_map<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>>& boneInfoMap = xParentGeometry.m_xBoneNameToIdAndOffset;//getting m_BoneInfoMap from Model class
	uint32_t boneCount = xParentGeometry.GetNumBones(); //getting the m_BoneCounter from Model class

	//reading channels(bones engaged in an animation and their keyframes)
	for (int i = 0; i < size; i++)
	{
		auto channel = pxAnimation->mChannels[i];
		std::string boneName = channel->mNodeName.data;

		std::string strName(channel->mNodeName.data);

		if (boneInfoMap.find(boneName) == boneInfoMap.end())
		{
			boneInfoMap.insert({ strName, {boneCount++, glm::identity<Zenith_Maths::Matrix4>()}});
			xParentGeometry.SetNumBones(boneCount);
		}

		m_xBones.push_back(AnimBone(channel->mNodeName.data,
			boneInfoMap.at(strName).first, channel));
	}

	ReadHierarchy(m_xRootNode, *pxScene->mRootNode, xParentGeometry);

	Flux_MemoryManager::InitialiseConstantBuffer(nullptr, sizeof(m_axAnimMatrices), m_xBoneBuffer);
}
