#include "Zenith.h"
#include "Flux_BonePose.h"
#include "Flux_MeshAnimation.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Core/Zenith_Core.h"

//=============================================================================
// Flux_BoneLocalPose
//=============================================================================
Zenith_Maths::Matrix4 Flux_BoneLocalPose::ToMatrix() const
{
	Zenith_Maths::Matrix4 xTranslation = glm::translate(glm::mat4(1.0f), m_xPosition);
	Zenith_Maths::Matrix4 xRotation = glm::toMat4(m_xRotation);
	Zenith_Maths::Matrix4 xScale = glm::scale(glm::mat4(1.0f), m_xScale);

	return xTranslation * xRotation * xScale;
}

void Flux_BoneLocalPose::FromMatrix(const Zenith_Maths::Matrix4& xMatrix)
{
	// Extract translation from column 3
	m_xPosition = Zenith_Maths::Vector3(xMatrix[3]);

	// Extract scale from column lengths
	m_xScale.x = glm::length(Zenith_Maths::Vector3(xMatrix[0]));
	m_xScale.y = glm::length(Zenith_Maths::Vector3(xMatrix[1]));
	m_xScale.z = glm::length(Zenith_Maths::Vector3(xMatrix[2]));

	// Minimum scale to prevent division by zero and NaN propagation
	constexpr float fMinScale = 1e-6f;

	// Extract rotation by removing scale
	// Use safe division to prevent NaN from degenerate matrices
	Zenith_Maths::Matrix3 xRotMat;
	xRotMat[0] = Zenith_Maths::Vector3(xMatrix[0]) / std::max(m_xScale.x, fMinScale);
	xRotMat[1] = Zenith_Maths::Vector3(xMatrix[1]) / std::max(m_xScale.y, fMinScale);
	xRotMat[2] = Zenith_Maths::Vector3(xMatrix[2]) / std::max(m_xScale.z, fMinScale);

	m_xRotation = glm::quat_cast(xRotMat);
}

Flux_BoneLocalPose Flux_BoneLocalPose::Blend(const Flux_BoneLocalPose& xA,
	const Flux_BoneLocalPose& xB,
	float fBlendFactor)
{
	fBlendFactor = glm::clamp(fBlendFactor, 0.0f, 1.0f);

	Flux_BoneLocalPose xResult;
	xResult.m_xPosition = glm::mix(xA.m_xPosition, xB.m_xPosition, fBlendFactor);
	xResult.m_xRotation = glm::slerp(xA.m_xRotation, xB.m_xRotation, fBlendFactor);
	xResult.m_xScale = glm::mix(xA.m_xScale, xB.m_xScale, fBlendFactor);

	return xResult;
}

Flux_BoneLocalPose Flux_BoneLocalPose::AdditiveBlend(const Flux_BoneLocalPose& xBase,
	const Flux_BoneLocalPose& xAdditive,
	float fWeight)
{
	// Additive blend assumes additive pose is relative to identity
	// Result = Base + (Additive - Identity) * Weight
	//        = Base + Additive * Weight (for position)
	//        = Base * slerp(identity, Additive, Weight) (for rotation)

	Flux_BoneLocalPose xResult;

	// Position: add weighted offset
	xResult.m_xPosition = xBase.m_xPosition + xAdditive.m_xPosition * fWeight;

	// Rotation: multiply by weighted additive rotation
	Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat xWeightedRot = glm::slerp(xIdentity, xAdditive.m_xRotation, fWeight);
	xResult.m_xRotation = xBase.m_xRotation * xWeightedRot;

	// Scale: multiplicative blend
	Zenith_Maths::Vector3 xScaleOffset = xAdditive.m_xScale - Zenith_Maths::Vector3(1.0f);
	xResult.m_xScale = xBase.m_xScale + xScaleOffset * fWeight;

	return xResult;
}

Flux_BoneLocalPose Flux_BoneLocalPose::AdditiveBlendWithReference(const Flux_BoneLocalPose& xBase,
	const Flux_BoneLocalPose& xAdditive,
	const Flux_BoneLocalPose& xReference,
	float fWeight)
{
	// Result = Base + (Additive - Reference) * Weight
	Flux_BoneLocalPose xResult;

	// Position
	Zenith_Maths::Vector3 xPosOffset = xAdditive.m_xPosition - xReference.m_xPosition;
	xResult.m_xPosition = xBase.m_xPosition + xPosOffset * fWeight;

	// Rotation: compute delta rotation (additive * inverse(reference))
	Zenith_Maths::Quat xInvRef = glm::inverse(xReference.m_xRotation);
	Zenith_Maths::Quat xDeltaRot = xAdditive.m_xRotation * xInvRef;

	Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat xWeightedDelta = glm::slerp(xIdentity, xDeltaRot, fWeight);
	xResult.m_xRotation = xBase.m_xRotation * xWeightedDelta;

	// Scale
	Zenith_Maths::Vector3 xScaleOffset = xAdditive.m_xScale - xReference.m_xScale;
	xResult.m_xScale = xBase.m_xScale + xScaleOffset * fWeight;

	return xResult;
}

//=============================================================================
// Flux_SkeletonPose
//=============================================================================
Flux_SkeletonPose::Flux_SkeletonPose()
{
	Reset();
}

void Flux_SkeletonPose::Initialize(uint32_t uNumBones)
{
	Zenith_Assert(uNumBones <= FLUX_MAX_BONES, "Too many bones");
	m_uNumBones = uNumBones;

	// Initialize all poses to identity
	for (uint32_t i = 0; i < m_uNumBones; ++i)
	{
		m_axLocalPoses[i] = Flux_BoneLocalPose::Identity();
		m_axModelSpaceMatrices[i] = glm::mat4(1.0f);
		m_axSkinningMatrices[i] = glm::mat4(1.0f);
	}
}

void Flux_SkeletonPose::Reset()
{
	for (uint32_t i = 0; i < FLUX_MAX_BONES; ++i)
	{
		m_axLocalPoses[i] = Flux_BoneLocalPose::Identity();
		m_axModelSpaceMatrices[i] = glm::mat4(1.0f);
		m_axSkinningMatrices[i] = glm::mat4(1.0f);
	}
}

Flux_BoneLocalPose& Flux_SkeletonPose::GetLocalPose(uint32_t uBoneIndex)
{
	Zenith_Assert(uBoneIndex < FLUX_MAX_BONES, "Bone index out of range");
	return m_axLocalPoses[uBoneIndex];
}

const Flux_BoneLocalPose& Flux_SkeletonPose::GetLocalPose(uint32_t uBoneIndex) const
{
	Zenith_Assert(uBoneIndex < FLUX_MAX_BONES, "Bone index out of range");
	return m_axLocalPoses[uBoneIndex];
}

const Zenith_Maths::Matrix4& Flux_SkeletonPose::GetModelSpaceMatrix(uint32_t uBoneIndex) const
{
	Zenith_Assert(uBoneIndex < FLUX_MAX_BONES, "Bone index out of range");
	return m_axModelSpaceMatrices[uBoneIndex];
}

const Zenith_Maths::Matrix4& Flux_SkeletonPose::GetSkinningMatrix(uint32_t uBoneIndex) const
{
	Zenith_Assert(uBoneIndex < FLUX_MAX_BONES, "Bone index out of range");
	return m_axSkinningMatrices[uBoneIndex];
}

void Flux_SkeletonPose::SampleFromClip(const Flux_AnimationClip& xClip,
	float fTime,
	const Flux_MeshGeometry& xGeometry)
{
	// Convert time to ticks
	float fTimeInTicks = fTime * xClip.GetTicksPerSecond();

	// Sample each bone channel
	for (const auto& xPair : xClip.GetBoneChannels())
	{
		const std::string& strBoneName = xPair.first;
		const Flux_BoneChannel& xChannel = xPair.second;

		// Find bone index in geometry
		std::unordered_map<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>>::const_iterator it = xGeometry.m_xBoneNameToIdAndOffset.find(strBoneName);
		if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
		{
			uint32_t uBoneIndex = it->second.first;
			if (uBoneIndex < FLUX_MAX_BONES)
			{
				m_axLocalPoses[uBoneIndex].m_xPosition = xChannel.SamplePosition(fTimeInTicks);
				m_axLocalPoses[uBoneIndex].m_xRotation = xChannel.SampleRotation(fTimeInTicks);
				m_axLocalPoses[uBoneIndex].m_xScale = xChannel.SampleScale(fTimeInTicks);
			}
		}
	}
}

void Flux_SkeletonPose::SampleFromClip(const Flux_AnimationClip& xClip,
	float fTime,
	const Zenith_SkeletonAsset& xSkeleton)
{
	// Convert time to ticks
	float fTimeInTicks = fTime * xClip.GetTicksPerSecond();

	// Debug: Log bone name matching once
	static bool s_bLoggedBoneNames = false;
	if (!s_bLoggedBoneNames)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip] Animation '%s' has %zu bone channels, skeleton has %u bones",
			xClip.GetName().c_str(),
			xClip.GetBoneChannels().size(),
			xSkeleton.GetNumBones());

		uint32_t uMatchCount = 0;
		for (const std::pair<const std::string, Flux_BoneChannel>& xPair : xClip.GetBoneChannels())
		{
			const std::string& strBoneName = xPair.first;
			std::unordered_map<std::string, uint32_t>::const_iterator it = xSkeleton.m_xBoneNameToIndex.find(strBoneName);
			if (it != xSkeleton.m_xBoneNameToIndex.end())
			{
				uMatchCount++;
				Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip]   MATCH: '%s' -> bone %u", strBoneName.c_str(), it->second);
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip]   NO MATCH: '%s'", strBoneName.c_str());
			}
		}
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip] Total matches: %u/%zu", uMatchCount, xClip.GetBoneChannels().size());

		// Also log skeleton bone names for comparison
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip] Skeleton bone names:");
		for (uint32_t u = 0; u < xSkeleton.GetNumBones(); u++)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[SampleFromClip]   [%u] '%s'", u, xSkeleton.GetBone(u).m_strName.c_str());
		}

		s_bLoggedBoneNames = true;
	}

	// Sample each bone channel
	for (const std::pair<const std::string, Flux_BoneChannel>& xPair : xClip.GetBoneChannels())
	{
		const std::string& strBoneName = xPair.first;
		const Flux_BoneChannel& xChannel = xPair.second;

		// Find bone index in skeleton asset
		std::unordered_map<std::string, uint32_t>::const_iterator it = xSkeleton.m_xBoneNameToIndex.find(strBoneName);
		if (it != xSkeleton.m_xBoneNameToIndex.end())
		{
			uint32_t uBoneIndex = it->second;
			if (uBoneIndex < FLUX_MAX_BONES)
			{
				m_axLocalPoses[uBoneIndex].m_xPosition = xChannel.SamplePosition(fTimeInTicks);
				m_axLocalPoses[uBoneIndex].m_xRotation = xChannel.SampleRotation(fTimeInTicks);
				m_axLocalPoses[uBoneIndex].m_xScale = xChannel.SampleScale(fTimeInTicks);
			}
		}
	}
}

void Flux_SkeletonPose::ComputeModelSpaceMatricesRecursive(const Flux_MeshAnimation::Node* pxNode,
	const Zenith_Maths::Matrix4& xParentTransform,
	const Flux_MeshGeometry& xGeometry)
{
	const std::string& strNodeName = pxNode->m_strName;

	// Start with node's default transform
	Zenith_Maths::Matrix4 xNodeTransform = pxNode->m_xTrans;

	// If this node corresponds to a bone, use the sampled local pose
	auto it = xGeometry.m_xBoneNameToIdAndOffset.find(strNodeName);
	if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
	{
		uint32_t uBoneIndex = it->second.first;
		if (uBoneIndex < FLUX_MAX_BONES)
		{
			xNodeTransform = m_axLocalPoses[uBoneIndex].ToMatrix();
		}
	}

	// Compute global transform
	Zenith_Maths::Matrix4 xGlobalTransform = xParentTransform * xNodeTransform;

	// Store model-space matrix for this bone
	if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
	{
		uint32_t uBoneIndex = it->second.first;
		if (uBoneIndex < FLUX_MAX_BONES)
		{
			m_axModelSpaceMatrices[uBoneIndex] = xGlobalTransform;
		}
	}

	// Recurse to children
	for (uint32_t i = 0; i < pxNode->m_uChildCount; ++i)
	{
		ComputeModelSpaceMatricesRecursive(&pxNode->m_xChildren[i], xGlobalTransform, xGeometry);
	}
}

void Flux_SkeletonPose::ComputeModelSpaceMatrices(const Flux_MeshAnimation::Node& xRootNode,
	const Flux_MeshGeometry& xGeometry)
{
	ComputeModelSpaceMatricesRecursive(&xRootNode, glm::mat4(1.0f), xGeometry);
}

void Flux_SkeletonPose::ComputeModelSpaceMatricesFlat(const Flux_MeshGeometry& xGeometry)
{
	// Simple flat hierarchy: each bone's model space = its local pose
	// This is a fallback when no node tree is available
	for (uint32_t i = 0; i < m_uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		m_axModelSpaceMatrices[i] = m_axLocalPoses[i].ToMatrix();
	}
}

void Flux_SkeletonPose::ComputeSkinningMatrices(const Flux_MeshGeometry& xGeometry)
{
	// skinning = modelSpace * inverseBindPose
	for (const auto& xPair : xGeometry.m_xBoneNameToIdAndOffset)
	{
		uint32_t uBoneIndex = xPair.second.first;
		const Zenith_Maths::Matrix4& xOffsetMatrix = xPair.second.second;

		if (uBoneIndex < FLUX_MAX_BONES)
		{
			m_axSkinningMatrices[uBoneIndex] = m_axModelSpaceMatrices[uBoneIndex] * xOffsetMatrix;
		}
	}
}

void Flux_SkeletonPose::Blend(Flux_SkeletonPose& xOut,
	const Flux_SkeletonPose& xA,
	const Flux_SkeletonPose& xB,
	float fBlendFactor)
{
	uint32_t uNumBones = glm::max(xA.m_uNumBones, xB.m_uNumBones);
	xOut.m_uNumBones = uNumBones;

	for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		xOut.m_axLocalPoses[i] = Flux_BoneLocalPose::Blend(
			xA.m_axLocalPoses[i],
			xB.m_axLocalPoses[i],
			fBlendFactor
		);
	}
}

void Flux_SkeletonPose::AdditiveBlend(Flux_SkeletonPose& xOut,
	const Flux_SkeletonPose& xBase,
	const Flux_SkeletonPose& xAdditive,
	float fWeight)
{
	xOut.m_uNumBones = xBase.m_uNumBones;

	for (uint32_t i = 0; i < xBase.m_uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		xOut.m_axLocalPoses[i] = Flux_BoneLocalPose::AdditiveBlend(
			xBase.m_axLocalPoses[i],
			xAdditive.m_axLocalPoses[i],
			fWeight
		);
	}
}

void Flux_SkeletonPose::MaskedBlend(Flux_SkeletonPose& xOut,
	const Flux_SkeletonPose& xLower,
	const Flux_SkeletonPose& xUpper,
	const std::vector<float>& xBoneMask)
{
	uint32_t uNumBones = glm::max(xLower.m_uNumBones, xUpper.m_uNumBones);
	xOut.m_uNumBones = uNumBones;

	for (uint32_t i = 0; i < uNumBones && i < FLUX_MAX_BONES; ++i)
	{
		float fMask = (i < xBoneMask.size()) ? xBoneMask[i] : 0.0f;
		xOut.m_axLocalPoses[i] = Flux_BoneLocalPose::Blend(
			xLower.m_axLocalPoses[i],
			xUpper.m_axLocalPoses[i],
			fMask
		);
	}
}

void Flux_SkeletonPose::CopyFrom(const Flux_SkeletonPose& xOther)
{
	m_uNumBones = xOther.m_uNumBones;

	for (uint32_t i = 0; i < FLUX_MAX_BONES; ++i)
	{
		m_axLocalPoses[i] = xOther.m_axLocalPoses[i];
		m_axModelSpaceMatrices[i] = xOther.m_axModelSpaceMatrices[i];
		m_axSkinningMatrices[i] = xOther.m_axSkinningMatrices[i];
	}
}

//=============================================================================
// Flux_BoneMask
//=============================================================================
Flux_BoneMask::Flux_BoneMask()
{
	m_xWeights.resize(FLUX_MAX_BONES, 0.0f);
}

void Flux_BoneMask::SetFromBoneNames(const std::vector<std::string>& xBoneNames,
	const Flux_MeshGeometry& xGeometry)
{
	// Reset all weights to 0
	std::fill(m_xWeights.begin(), m_xWeights.end(), 0.0f);

	// Set weight 1.0 for specified bones
	for (const std::string& strName : xBoneNames)
	{
		auto it = xGeometry.m_xBoneNameToIdAndOffset.find(strName);
		if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
		{
			uint32_t uBoneIndex = it->second.first;
			if (uBoneIndex < m_xWeights.size())
			{
				m_xWeights[uBoneIndex] = 1.0f;
			}
		}
	}
}

void Flux_BoneMask::SetBoneWeight(uint32_t uBoneIndex, float fWeight)
{
	if (uBoneIndex < m_xWeights.size())
	{
		m_xWeights[uBoneIndex] = glm::clamp(fWeight, 0.0f, 1.0f);
	}
}

float Flux_BoneMask::GetBoneWeight(uint32_t uBoneIndex) const
{
	if (uBoneIndex < m_xWeights.size())
	{
		return m_xWeights[uBoneIndex];
	}
	return 0.0f;
}

Flux_BoneMask Flux_BoneMask::CreateUpperBodyMask(const Flux_MeshGeometry& xGeometry,
	const std::string& strSpineBoneName)
{
	Flux_BoneMask xMask;

	// Find spine bone and set all bones from spine upward to 1.0
	// This requires traversing the skeleton hierarchy
	// For now, simple implementation: set spine and all bones with higher indices to 1.0
	auto it = xGeometry.m_xBoneNameToIdAndOffset.find(strSpineBoneName);
	if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
	{
		uint32_t uSpineIndex = it->second.first;
		// Mark spine and all bones above it
		// Note: This is a simplistic approach; proper implementation would use hierarchy
		for (const auto& xPair : xGeometry.m_xBoneNameToIdAndOffset)
		{
			const std::string& strBoneName = xPair.first;
			uint32_t uBoneIndex = xPair.second.first;

			// Check if bone name suggests upper body
			// (spine, chest, neck, head, arm, hand, shoulder, clavicle)
			if (strBoneName.find("spine") != std::string::npos ||
				strBoneName.find("Spine") != std::string::npos ||
				strBoneName.find("chest") != std::string::npos ||
				strBoneName.find("Chest") != std::string::npos ||
				strBoneName.find("neck") != std::string::npos ||
				strBoneName.find("Neck") != std::string::npos ||
				strBoneName.find("head") != std::string::npos ||
				strBoneName.find("Head") != std::string::npos ||
				strBoneName.find("arm") != std::string::npos ||
				strBoneName.find("Arm") != std::string::npos ||
				strBoneName.find("hand") != std::string::npos ||
				strBoneName.find("Hand") != std::string::npos ||
				strBoneName.find("shoulder") != std::string::npos ||
				strBoneName.find("Shoulder") != std::string::npos ||
				strBoneName.find("clavicle") != std::string::npos ||
				strBoneName.find("Clavicle") != std::string::npos)
			{
				xMask.SetBoneWeight(uBoneIndex, 1.0f);
			}
		}
	}

	return xMask;
}

Flux_BoneMask Flux_BoneMask::CreateLowerBodyMask(const Flux_MeshGeometry& xGeometry,
	const std::string& strSpineBoneName)
{
	Flux_BoneMask xMask;

	// Set lower body bones to 1.0
	for (const auto& xPair : xGeometry.m_xBoneNameToIdAndOffset)
	{
		const std::string& strBoneName = xPair.first;
		uint32_t uBoneIndex = xPair.second.first;

		// Check if bone name suggests lower body
		// (hip, pelvis, leg, thigh, knee, foot, toe)
		if (strBoneName.find("hip") != std::string::npos ||
			strBoneName.find("Hip") != std::string::npos ||
			strBoneName.find("pelvis") != std::string::npos ||
			strBoneName.find("Pelvis") != std::string::npos ||
			strBoneName.find("leg") != std::string::npos ||
			strBoneName.find("Leg") != std::string::npos ||
			strBoneName.find("thigh") != std::string::npos ||
			strBoneName.find("Thigh") != std::string::npos ||
			strBoneName.find("knee") != std::string::npos ||
			strBoneName.find("Knee") != std::string::npos ||
			strBoneName.find("foot") != std::string::npos ||
			strBoneName.find("Foot") != std::string::npos ||
			strBoneName.find("toe") != std::string::npos ||
			strBoneName.find("Toe") != std::string::npos)
		{
			xMask.SetBoneWeight(uBoneIndex, 1.0f);
		}
	}

	return xMask;
}

void Flux_BoneMask::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumWeights = static_cast<uint32_t>(m_xWeights.size());
	xStream << uNumWeights;
	for (float fWeight : m_xWeights)
	{
		xStream << fWeight;
	}
}

void Flux_BoneMask::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uNumWeights = 0;
	xStream >> uNumWeights;
	m_xWeights.resize(uNumWeights);
	for (uint32_t i = 0; i < uNumWeights; ++i)
	{
		xStream >> m_xWeights[i];
	}
}

//=============================================================================
// Flux_CrossFadeTransition
//=============================================================================
void Flux_CrossFadeTransition::Start(const Flux_SkeletonPose& xFromPose, float fDuration)
{
	m_xFromPose.CopyFrom(xFromPose);
	m_fDuration = fDuration;
	m_fElapsedTime = 0.0f;
}

bool Flux_CrossFadeTransition::Update(float fDt)
{
	m_fElapsedTime += fDt;
	return !IsComplete();
}

float Flux_CrossFadeTransition::ApplyEasing(float t) const
{
	switch (m_eEasing)
	{
	case EasingType::Linear:
		return t;

	case EasingType::EaseInOut:
		// Smoothstep
		return t * t * (3.0f - 2.0f * t);

	case EasingType::EaseIn:
		// Quadratic ease in
		return t * t;

	case EasingType::EaseOut:
		// Quadratic ease out
		return t * (2.0f - t);

	default:
		return t;
	}
}

float Flux_CrossFadeTransition::GetBlendWeight() const
{
	if (m_fDuration <= 0.0f)
		return 1.0f;

	float t = glm::clamp(m_fElapsedTime / m_fDuration, 0.0f, 1.0f);
	return ApplyEasing(t);
}

void Flux_CrossFadeTransition::Blend(Flux_SkeletonPose& xOut, const Flux_SkeletonPose& xTarget) const
{
	float fBlendWeight = GetBlendWeight();
	Flux_SkeletonPose::Blend(xOut, m_xFromPose, xTarget, fBlendWeight);
}
