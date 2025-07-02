#include "Zenith.h"
#include "Flux_MeshAnimation.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include <assimp/anim.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Flux_MeshAnimation::AnimBone::AnimBone(const aiNodeAnim* pxChannel)
	: m_strName(pxChannel->mNodeName.data)
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

static void ReadHierarchy(Flux_MeshAnimation::Node& xDst, const aiNode& xSrc)
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
		ReadHierarchy(xNewNode, *xSrc.mChildren[u]);
		xDst.m_xChildren.push_back(xNewNode);
	}
}

void Flux_MeshAnimation::CalculateBoneTransform(const Node* const pxNode, const glm::mat4& xParentTransform, bool bDebug /*= false*/)
{
	const std::string& strNodeName = pxNode->m_strName;
	Zenith_Maths::Matrix4 xNodeTransform = pxNode->m_xTrans;

	if (m_xBones.find(strNodeName) != m_xBones.end())
	{
		AnimBone& xBone = m_xBones.at(strNodeName);
		xBone.Update(m_fCurrentTimestamp);
		xNodeTransform = xBone.m_xLocalTransform;
	}

	if (bDebug)
	{
		printf("Animation Bone %s, local transform\n", strNodeName.c_str());
		for (u_int u = 0; u < 4; u++)
		{
			for (u_int v = 0; v < 4; v++)
			{
				printf(" %f ", xNodeTransform[u][v]);
			}
			printf("\n");
		}
		printf("\n");
	}

	glm::mat4 xGlobalTransformation = xParentTransform * xNodeTransform;

	const std::unordered_map<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>>& xBoneInfoMap = m_xParentGeometry.m_xBoneNameToIdAndOffset;
	if (xBoneInfoMap.find(strNodeName) != xBoneInfoMap.end())
	{
		const std::pair<uint32_t, Zenith_Maths::Matrix4>& xIdAndOffset = xBoneInfoMap.at(strNodeName);
		const u_int uIndex = xIdAndOffset.first;
		const glm::mat4& xOffset = xIdAndOffset.second;
		m_axAnimMatrices[uIndex] = xGlobalTransformation * xOffset;
	}

	for (u_int u = 0; u < pxNode->m_uChildCount; u++)
	{
		CalculateBoneTransform(&pxNode->m_xChildren[u], xGlobalTransformation, bDebug);
	}
}

Flux_MeshAnimation::Flux_MeshAnimation(const std::string& strPath, Flux_MeshGeometry& xParentGeometry)
	: m_xParentGeometry(xParentGeometry)
{
	for (u_int u = 0; u < uMAX_BONES_PER_ANIM; u++)
	{
		m_axAnimMatrices[u] = glm::identity<Zenith_Maths::Matrix4>();
	}

	Assimp::Importer xImporter;
	const aiScene* pxScene = xImporter.ReadFile(strPath, aiProcess_Triangulate);

	const aiAnimation* pxAnimation = pxScene->mAnimations[0];
	m_fDuration = pxAnimation->mDuration;
	m_uTicksPerSecond = pxAnimation->mTicksPerSecond;
	

	ReadHierarchy(m_xRootNode, *pxScene->mRootNode);

	const std::unordered_map<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>>& xBoneInfoMap = xParentGeometry.m_xBoneNameToIdAndOffset;

	//#TO convert each channel into an AnimBone and register against its name
	for (u_int u = 0; u < pxAnimation->mNumChannels; u++)
	{
		const aiNodeAnim* pxChannel = pxAnimation->mChannels[u];
		const std::string& strBoneName = pxChannel->mNodeName.data;

		if (xBoneInfoMap.find(strBoneName) == xBoneInfoMap.end())
		{
			continue;
		}

		m_xBones.insert({ strBoneName, AnimBone(pxChannel) });
	}

	

	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(m_axAnimMatrices), m_xBoneBuffer);
}
