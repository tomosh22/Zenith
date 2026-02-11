#include "Zenith.h"
#include "Flux_InverseKinematics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Core/Zenith_Core.h"

//=============================================================================
// Flux_JointConstraint
//=============================================================================
void Flux_JointConstraint::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << static_cast<uint8_t>(m_eType);
	xStream << m_xHingeAxis.x;
	xStream << m_xHingeAxis.y;
	xStream << m_xHingeAxis.z;
	xStream << m_fMinAngle;
	xStream << m_fMaxAngle;
	xStream << m_fConeAngle;
	xStream << m_fMinTwist;
	xStream << m_fMaxTwist;
}

void Flux_JointConstraint::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint8_t uType = 0;
	xStream >> uType;
	m_eType = static_cast<ConstraintType>(uType);

	xStream >> m_xHingeAxis.x;
	xStream >> m_xHingeAxis.y;
	xStream >> m_xHingeAxis.z;
	xStream >> m_fMinAngle;
	xStream >> m_fMaxAngle;
	xStream >> m_fConeAngle;
	xStream >> m_fMinTwist;
	xStream >> m_fMaxTwist;
}

//=============================================================================
// Flux_IKChain
//=============================================================================
void Flux_IKChain::ResolveBoneIndices(const Flux_MeshGeometry& xGeometry)
{
	m_xBoneIndices.clear();
	m_xBoneIndices.reserve(m_xBoneNames.size());

	for (const std::string& strName : m_xBoneNames)
	{
		auto it = xGeometry.m_xBoneNameToIdAndOffset.find(strName);
		if (it != xGeometry.m_xBoneNameToIdAndOffset.end())
		{
			m_xBoneIndices.push_back(it->second.first);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[IK] Warning: Bone '%s' not found in skeleton", strName.c_str());
			m_xBoneIndices.push_back(~0u);  // Invalid index
		}
	}
}

void Flux_IKChain::ComputeBoneLengths(const Flux_SkeletonPose& xPose)
{
	m_xBoneLengths.clear();
	m_fTotalLength = 0.0f;

	if (m_xBoneIndices.size() < 2)
		return;

	m_xBoneLengths.reserve(m_xBoneIndices.size() - 1);

	for (size_t i = 0; i < m_xBoneIndices.size() - 1; ++i)
	{
		uint32_t uCurrent = m_xBoneIndices[i];
		uint32_t uNext = m_xBoneIndices[i + 1];

		if (uCurrent == ~0u || uNext == ~0u)
		{
			m_xBoneLengths.push_back(0.0f);
			continue;
		}

		Zenith_Maths::Vector3 xCurrentPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uCurrent)[3]);
		Zenith_Maths::Vector3 xNextPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uNext)[3]);

		float fLength = glm::length(xNextPos - xCurrentPos);
		m_xBoneLengths.push_back(fLength);
		m_fTotalLength += fLength;
	}
}

void Flux_IKChain::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_uMaxIterations;
	xStream << m_fTolerance;
	xStream << m_bUsePoleVector;
	xStream << m_xPoleVector.x;
	xStream << m_xPoleVector.y;
	xStream << m_xPoleVector.z;
	xStream << m_strPoleTargetBone;

	uint32_t uNumBones = static_cast<uint32_t>(m_xBoneNames.size());
	xStream << uNumBones;
	for (const auto& strName : m_xBoneNames)
	{
		xStream << strName;
	}

	uint32_t uNumConstraints = static_cast<uint32_t>(m_xJointConstraints.size());
	xStream << uNumConstraints;
	for (const auto& xConstraint : m_xJointConstraints)
	{
		xConstraint.WriteToDataStream(xStream);
	}
}

void Flux_IKChain::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strName;
	xStream >> m_uMaxIterations;
	xStream >> m_fTolerance;
	xStream >> m_bUsePoleVector;
	xStream >> m_xPoleVector.x;
	xStream >> m_xPoleVector.y;
	xStream >> m_xPoleVector.z;
	xStream >> m_strPoleTargetBone;

	uint32_t uNumBones = 0;
	xStream >> uNumBones;
	m_xBoneNames.resize(uNumBones);
	for (uint32_t i = 0; i < uNumBones; ++i)
	{
		xStream >> m_xBoneNames[i];
	}

	uint32_t uNumConstraints = 0;
	xStream >> uNumConstraints;
	m_xJointConstraints.resize(uNumConstraints);
	for (uint32_t i = 0; i < uNumConstraints; ++i)
	{
		m_xJointConstraints[i].ReadFromDataStream(xStream);
	}

	m_xBoneIndices.clear();
	m_xBoneLengths.clear();
	m_fTotalLength = 0.0f;
}

//=============================================================================
// Flux_IKSolver
//=============================================================================
void Flux_IKSolver::AddChain(const Flux_IKChain& xChain)
{
	m_xChains[xChain.m_strName] = xChain;
}

void Flux_IKSolver::RemoveChain(const std::string& strName)
{
	m_xChains.erase(strName);
	m_xTargets.erase(strName);
}

Flux_IKChain* Flux_IKSolver::GetChain(const std::string& strName)
{
	auto it = m_xChains.find(strName);
	return (it != m_xChains.end()) ? &it->second : nullptr;
}

const Flux_IKChain* Flux_IKSolver::GetChain(const std::string& strName) const
{
	auto it = m_xChains.find(strName);
	return (it != m_xChains.end()) ? &it->second : nullptr;
}

bool Flux_IKSolver::HasChain(const std::string& strName) const
{
	return m_xChains.find(strName) != m_xChains.end();
}

void Flux_IKSolver::SetTarget(const std::string& strChainName, const Flux_IKTarget& xTarget)
{
	m_xTargets[strChainName] = xTarget;
}

void Flux_IKSolver::ClearTarget(const std::string& strChainName)
{
	m_xTargets.erase(strChainName);
}

const Flux_IKTarget* Flux_IKSolver::GetTarget(const std::string& strChainName) const
{
	auto it = m_xTargets.find(strChainName);
	return (it != m_xTargets.end()) ? &it->second : nullptr;
}

bool Flux_IKSolver::HasTarget(const std::string& strChainName) const
{
	return m_xTargets.find(strChainName) != m_xTargets.end();
}

void Flux_IKSolver::Solve(Flux_SkeletonPose& xPose,
	const Flux_MeshGeometry& xGeometry,
	const Zenith_Maths::Matrix4& xWorldMatrix)
{
	for (auto& xPair : m_xChains)
	{
		Flux_IKChain& xChain = xPair.second;
		const std::string& strChainName = xPair.first;

		// Check if chain has active target
		auto itTarget = m_xTargets.find(strChainName);
		if (itTarget == m_xTargets.end() || !itTarget->second.m_bEnabled)
			continue;

		// Resolve bone indices if needed
		if (xChain.m_xBoneIndices.empty())
			xChain.ResolveBoneIndices(xGeometry);

		// Compute bone lengths if needed
		if (xChain.m_xBoneLengths.empty())
			xChain.ComputeBoneLengths(xPose);

		// Transform target to model space
		Flux_IKTarget xModelSpaceTarget = itTarget->second;
		Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorldMatrix);
		Zenith_Maths::Vector4 xTargetWorld = Zenith_Maths::Vector4(xModelSpaceTarget.m_xPosition, 1.0f);
		xModelSpaceTarget.m_xPosition = Zenith_Maths::Vector3(xInvWorld * xTargetWorld);

		// Solve the chain
		SolveChain(xPose, xChain, xModelSpaceTarget, xGeometry);
	}
}

void Flux_IKSolver::SolveChain(Flux_SkeletonPose& xPose,
	const Flux_IKChain& xChain,
	const Flux_IKTarget& xTarget,
	const Flux_MeshGeometry& xGeometry)
{
	if (xChain.m_xBoneIndices.size() < 2 || xChain.m_xBoneLengths.empty())
		return;

	const uint32_t uNumBones = static_cast<uint32_t>(xChain.m_xBoneIndices.size());

	// Extract bone positions from current pose (model space)
	std::vector<Zenith_Maths::Vector3> xBonePositions(uNumBones);
	for (uint32_t i = 0; i < uNumBones; ++i)
	{
		uint32_t uBoneIndex = xChain.m_xBoneIndices[i];
		if (uBoneIndex != ~0u)
		{
			xBonePositions[i] = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uBoneIndex)[3]);
		}
	}

	const Zenith_Maths::Vector3 xRootPos = xBonePositions[0];
	const Zenith_Maths::Vector3 xTargetPos = xTarget.m_xPosition;
	const float fDistToTarget = glm::length(xTargetPos - xRootPos);

	// Check if target is reachable
	if (fDistToTarget > xChain.m_fTotalLength)
	{
		// Target unreachable - stretch towards it
		Zenith_Maths::Vector3 xDirection = glm::normalize(xTargetPos - xRootPos);
		float fAccumLength = 0.0f;

		for (uint32_t i = 1; i < uNumBones; ++i)
		{
			fAccumLength += xChain.m_xBoneLengths[i - 1];
			xBonePositions[i] = xRootPos + xDirection * fAccumLength;
		}
	}
	else
	{
		// FABRIK iterations
		for (uint32_t iter = 0; iter < xChain.m_uMaxIterations; ++iter)
		{
			// Forward reaching: from end effector to root
			ForwardReaching(xBonePositions, xChain.m_xBoneLengths, xTargetPos);

			// Backward reaching: from root to end effector
			BackwardReaching(xBonePositions, xChain.m_xBoneLengths, xRootPos);

			// Apply joint constraints if any
			if (!xChain.m_xJointConstraints.empty())
			{
				ApplyConstraints(xBonePositions, xChain, xPose);
			}

			// Apply pole vector constraint
			if (xChain.m_bUsePoleVector && uNumBones >= 3)
			{
				ApplyPoleVectorConstraint(xBonePositions, xChain, xChain.m_xPoleVector);
			}

			// Check convergence
			float fError = glm::length(xBonePositions[uNumBones - 1] - xTargetPos);
			if (fError < xChain.m_fTolerance)
				break;
		}
	}

	// Convert positions back to bone rotations
	ConvertPositionsToRotations(xPose, xChain, xBonePositions, xGeometry, xTarget.m_fWeight);
}

void Flux_IKSolver::ForwardReaching(std::vector<Zenith_Maths::Vector3>& xPositions,
	const std::vector<float>& xBoneLengths,
	const Zenith_Maths::Vector3& xTargetPos)
{
	const size_t uNumBones = xPositions.size();
	if (uNumBones < 2)
		return;

	// Set end effector to target
	xPositions[uNumBones - 1] = xTargetPos;

	// Work backward to root
	for (int32_t i = static_cast<int32_t>(uNumBones) - 2; i >= 0; --i)
	{
		Zenith_Maths::Vector3 xDir = xPositions[i] - xPositions[i + 1];
		float fLen = glm::length(xDir);

		if (fLen > 0.0001f)
		{
			xDir = xDir / fLen;
			xPositions[i] = xPositions[i + 1] + xDir * xBoneLengths[i];
		}
	}
}

void Flux_IKSolver::BackwardReaching(std::vector<Zenith_Maths::Vector3>& xPositions,
	const std::vector<float>& xBoneLengths,
	const Zenith_Maths::Vector3& xRootPos)
{
	const size_t uNumBones = xPositions.size();
	if (uNumBones < 2)
		return;

	// Fix root position
	xPositions[0] = xRootPos;

	// Work forward to end effector
	for (size_t i = 0; i < uNumBones - 1; ++i)
	{
		Zenith_Maths::Vector3 xDir = xPositions[i + 1] - xPositions[i];
		float fLen = glm::length(xDir);

		if (fLen > 0.0001f)
		{
			xDir = xDir / fLen;
			xPositions[i + 1] = xPositions[i] + xDir * xBoneLengths[i];
		}
	}
}

void Flux_IKSolver::ApplyConstraints(std::vector<Zenith_Maths::Vector3>& xPositions,
	const Flux_IKChain& xChain,
	const Flux_SkeletonPose& xOriginalPose)
{
	// Apply joint constraints
	// For now, this is a simplified implementation
	// A full implementation would properly handle each constraint type

	// Loop must also check bone lengths bounds (m_xBoneLengths.size() == m_xBoneIndices.size() - 1)
	const size_t uMaxIndex = std::min({xChain.m_xJointConstraints.size(), xPositions.size(), xChain.m_xBoneLengths.size()});
	for (size_t i = 0; i < uMaxIndex; ++i)
	{
		const Flux_JointConstraint& xConstraint = xChain.m_xJointConstraints[i];

		switch (xConstraint.m_eType)
		{
		case Flux_JointConstraint::ConstraintType::Hinge:
		{
			// Project movement onto plane perpendicular to hinge axis
			if (i > 0 && i + 1 < xPositions.size())
			{
				Zenith_Maths::Vector3 xBoneDir = xPositions[i + 1] - xPositions[i];
				Zenith_Maths::Vector3 xAxis = xConstraint.m_xHingeAxis;

				// Remove component along hinge axis
				float fDot = glm::dot(xBoneDir, xAxis);
				xBoneDir = xBoneDir - xAxis * fDot;

				if (glm::length(xBoneDir) > 0.0001f)
				{
					xBoneDir = glm::normalize(xBoneDir) * xChain.m_xBoneLengths[i];
					xPositions[i + 1] = xPositions[i] + xBoneDir;
				}
			}
			break;
		}

		case Flux_JointConstraint::ConstraintType::BallSocket:
		{
			// Limit angle from original direction
			if (i > 0 && i + 1 < xPositions.size() && i < xChain.m_xBoneIndices.size())
			{
				uint32_t uBoneIdx = xChain.m_xBoneIndices[i];
				if (uBoneIdx != ~0u)
				{
					Zenith_Maths::Vector3 xOrigDir = Zenith_Maths::Vector3(
						xOriginalPose.GetModelSpaceMatrix(uBoneIdx) *
						Zenith_Maths::Vector4(0, 1, 0, 0)
					);

					Zenith_Maths::Vector3 xNewDir = glm::normalize(xPositions[i + 1] - xPositions[i]);
					float fAngle = std::acos(glm::clamp(glm::dot(xOrigDir, xNewDir), -1.0f, 1.0f));

					if (fAngle > xConstraint.m_fConeAngle)
					{
						// Clamp to cone
						Zenith_Maths::Vector3 xAxis = glm::cross(xOrigDir, xNewDir);
						if (glm::length(xAxis) > 0.0001f)
						{
							xAxis = glm::normalize(xAxis);
							Zenith_Maths::Quat xRotation = glm::angleAxis(xConstraint.m_fConeAngle, xAxis);
							Zenith_Maths::Vector3 xClampedDir = xRotation * xOrigDir;
							xPositions[i + 1] = xPositions[i] + xClampedDir * xChain.m_xBoneLengths[i];
						}
					}
				}
			}
			break;
		}

		default:
			break;
		}
	}
}

void Flux_IKSolver::ApplyPoleVectorConstraint(std::vector<Zenith_Maths::Vector3>& xPositions,
	const Flux_IKChain& xChain,
	const Zenith_Maths::Vector3& xPolePosition)
{
	if (xPositions.size() < 3)
		return;

	// For a 3-bone chain (like arm or leg), rotate the middle joint
	// to point toward the pole vector
	const Zenith_Maths::Vector3& xRoot = xPositions[0];
	const Zenith_Maths::Vector3& xEnd = xPositions[xPositions.size() - 1];

	// Main axis from root to end
	Zenith_Maths::Vector3 xMainAxis = xEnd - xRoot;
	float fMainLength = glm::length(xMainAxis);
	if (fMainLength < 0.0001f)
		return;

	xMainAxis = xMainAxis / fMainLength;

	// Project pole onto plane perpendicular to main axis
	Zenith_Maths::Vector3 xToPole = xPolePosition - xRoot;
	xToPole = xToPole - xMainAxis * glm::dot(xToPole, xMainAxis);

	if (glm::length(xToPole) < 0.0001f)
		return;

	xToPole = glm::normalize(xToPole);

	// For each middle joint, ensure it's on the pole side
	for (size_t i = 1; i < xPositions.size() - 1; ++i)
	{
		Zenith_Maths::Vector3 xToJoint = xPositions[i] - xRoot;
		xToJoint = xToJoint - xMainAxis * glm::dot(xToJoint, xMainAxis);

		if (glm::length(xToJoint) < 0.0001f)
			continue;

		float fCurrentDist = glm::length(xToJoint);
		Zenith_Maths::Vector3 xNewJointOffset = xToPole * fCurrentDist;

		// Project root position along main axis
		float fAlongMain = glm::dot(xPositions[i] - xRoot, xMainAxis);
		xPositions[i] = xRoot + xMainAxis * fAlongMain + xNewJointOffset;
	}

	// Re-apply bone length constraints after pole adjustment
	for (size_t i = 0; i < xPositions.size() - 1; ++i)
	{
		Zenith_Maths::Vector3 xDir = xPositions[i + 1] - xPositions[i];
		float fLen = glm::length(xDir);
		if (fLen > 0.0001f)
		{
			xDir = xDir / fLen;
			xPositions[i + 1] = xPositions[i] + xDir * xChain.m_xBoneLengths[i];
		}
	}
}

Zenith_Maths::Quat RotationBetweenVectors(const Zenith_Maths::Vector3& xFrom,
	const Zenith_Maths::Vector3& xTo)
{
	Zenith_Maths::Vector3 xFromNorm = glm::normalize(xFrom);
	Zenith_Maths::Vector3 xToNorm = glm::normalize(xTo);

	float fDot = glm::dot(xFromNorm, xToNorm);

	if (fDot > 0.9999f)
	{
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity
	}

	if (fDot < -0.9999f)
	{
		// Opposite directions - find perpendicular axis
		Zenith_Maths::Vector3 xAxis = glm::cross(Zenith_Maths::Vector3(1, 0, 0), xFromNorm);
		if (glm::length(xAxis) < 0.0001f)
		{
			xAxis = glm::cross(Zenith_Maths::Vector3(0, 1, 0), xFromNorm);
		}
		xAxis = glm::normalize(xAxis);
		return glm::angleAxis(3.14159265f, xAxis);
	}

	Zenith_Maths::Vector3 xAxis = glm::cross(xFromNorm, xToNorm);
	float s = std::sqrt((1.0f + fDot) * 2.0f);

	// Prevent division by zero/near-zero which causes Inf quaternion components
	// This handles edge cases where fDot is very close to -1.0 but above threshold
	constexpr float fMinS = 1e-5f;
	if (s < fMinS)
	{
		// Fallback to 180-degree rotation around an arbitrary axis
		Zenith_Maths::Vector3 xFallbackAxis = glm::cross(Zenith_Maths::Vector3(1, 0, 0), xFromNorm);
		if (glm::length(xFallbackAxis) < 0.0001f)
		{
			xFallbackAxis = glm::cross(Zenith_Maths::Vector3(0, 1, 0), xFromNorm);
		}
		xFallbackAxis = glm::normalize(xFallbackAxis);
		return glm::angleAxis(3.14159265f, xFallbackAxis);
	}

	float invS = 1.0f / s;

	return Zenith_Maths::Quat(s * 0.5f, xAxis.x * invS, xAxis.y * invS, xAxis.z * invS);
}

void Flux_IKSolver::ConvertPositionsToRotations(Flux_SkeletonPose& xPose,
	const Flux_IKChain& xChain,
	const std::vector<Zenith_Maths::Vector3>& xPositions,
	const Flux_MeshGeometry&,
	float fWeight)
{
	if (xChain.m_xBoneIndices.size() < 2)
		return;

	// For each bone (except the last), compute the rotation needed
	// to point toward the next bone in the chain
	for (size_t i = 0; i < xChain.m_xBoneIndices.size() - 1; ++i)
	{
		uint32_t uBoneIndex = xChain.m_xBoneIndices[i];
		uint32_t uChildIndex = xChain.m_xBoneIndices[i + 1];

		if (uBoneIndex == ~0u || uChildIndex == ~0u)
			continue;

		// Current direction to child in model space
		Zenith_Maths::Vector3 xCurrentChildPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uChildIndex)[3]);
		Zenith_Maths::Vector3 xCurrentPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uBoneIndex)[3]);
		Zenith_Maths::Vector3 xCurrentDir = xCurrentChildPos - xCurrentPos;

		if (glm::length(xCurrentDir) < 0.0001f)
			continue;

		xCurrentDir = glm::normalize(xCurrentDir);

		// Target direction
		Zenith_Maths::Vector3 xTargetDir = xPositions[i + 1] - xPositions[i];
		if (glm::length(xTargetDir) < 0.0001f)
			continue;

		xTargetDir = glm::normalize(xTargetDir);

		// Compute rotation from current to target
		Zenith_Maths::Quat xDeltaRotation = RotationBetweenVectors(xCurrentDir, xTargetDir);

		// Apply with weight
		Flux_BoneLocalPose& xLocalPose = xPose.GetLocalPose(uBoneIndex);
		Zenith_Maths::Quat xWeightedDelta = glm::slerp(
			Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
			xDeltaRotation,
			fWeight
		);

		xLocalPose.m_xRotation = xWeightedDelta * xLocalPose.m_xRotation;
	}
}

Flux_IKChain Flux_IKSolver::CreateLegChain(const std::string& strName,
	const std::string& strHipBone,
	const std::string& strKneeBone,
	const std::string& strAnkleBone)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames = { strHipBone, strKneeBone, strAnkleBone };
	xChain.m_bUsePoleVector = true;
	xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);  // Forward

	// Knee hinge constraint
	Flux_JointConstraint xKneeConstraint;
	xKneeConstraint.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
	xKneeConstraint.m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);  // Side axis
	xKneeConstraint.m_fMinAngle = 0.0f;
	xKneeConstraint.m_fMaxAngle = 2.5f;  // ~143 degrees

	xChain.m_xJointConstraints.push_back(Flux_JointConstraint());  // Hip - no constraint for now
	xChain.m_xJointConstraints.push_back(xKneeConstraint);
	xChain.m_xJointConstraints.push_back(Flux_JointConstraint());  // Ankle

	return xChain;
}

Flux_IKChain Flux_IKSolver::CreateArmChain(const std::string& strName,
	const std::string& strShoulderBone,
	const std::string& strElbowBone,
	const std::string& strWristBone)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames = { strShoulderBone, strElbowBone, strWristBone };
	xChain.m_bUsePoleVector = true;
	xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Behind

	// Elbow hinge constraint
	Flux_JointConstraint xElbowConstraint;
	xElbowConstraint.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
	xElbowConstraint.m_xHingeAxis = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);  // Up axis
	xElbowConstraint.m_fMinAngle = 0.0f;
	xElbowConstraint.m_fMaxAngle = 2.7f;  // ~155 degrees

	xChain.m_xJointConstraints.push_back(Flux_JointConstraint());  // Shoulder
	xChain.m_xJointConstraints.push_back(xElbowConstraint);
	xChain.m_xJointConstraints.push_back(Flux_JointConstraint());  // Wrist

	return xChain;
}

Flux_IKChain Flux_IKSolver::CreateSpineChain(const std::string& strName,
	const std::vector<std::string>& xSpineBones)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames = xSpineBones;
	xChain.m_bUsePoleVector = false;

	// Ball-socket constraints for each spine bone
	for (size_t i = 0; i < xSpineBones.size(); ++i)
	{
		Flux_JointConstraint xConstraint;
		xConstraint.m_eType = Flux_JointConstraint::ConstraintType::BallSocket;
		xConstraint.m_fConeAngle = 0.35f;  // ~20 degrees per vertebra
		xChain.m_xJointConstraints.push_back(xConstraint);
	}

	return xChain;
}

void Flux_IKSolver::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumChains = static_cast<uint32_t>(m_xChains.size());
	xStream << uNumChains;

	for (const auto& xPair : m_xChains)
	{
		xPair.second.WriteToDataStream(xStream);
	}
}

void Flux_IKSolver::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xChains.clear();
	m_xTargets.clear();

	uint32_t uNumChains = 0;
	xStream >> uNumChains;

	for (uint32_t i = 0; i < uNumChains; ++i)
	{
		Flux_IKChain xChain;
		xChain.ReadFromDataStream(xStream);
		m_xChains[xChain.m_strName] = xChain;
	}
}

//=============================================================================
// Specialized IK Helpers
//=============================================================================
bool SolveTwoBoneIK(const Zenith_Maths::Vector3& xRootPos,
	const Zenith_Maths::Vector3& xMidPos,
	const Zenith_Maths::Vector3& xEndPos,
	const Zenith_Maths::Vector3& xTargetPos,
	const Zenith_Maths::Vector3& xPoleVector,
	float fUpperLength,
	float fLowerLength,
	Zenith_Maths::Quat& xOutRootRotation,
	Zenith_Maths::Quat& xOutMidRotation)
{
	Zenith_Maths::Vector3 xToTarget = xTargetPos - xRootPos;
	float fTargetDist = glm::length(xToTarget);

	// Check reachability
	float fTotalLength = fUpperLength + fLowerLength;
	if (fTargetDist > fTotalLength * 0.999f)
	{
		// Target too far - stretch toward it
		fTargetDist = fTotalLength * 0.999f;
	}
	if (fTargetDist < std::abs(fUpperLength - fLowerLength) * 1.001f)
	{
		// Target too close
		fTargetDist = std::abs(fUpperLength - fLowerLength) * 1.001f;
	}

	// Calculate mid position using law of cosines
	// a = upper, b = lower, c = target distance
	// cos(A) = (b^2 + c^2 - a^2) / (2bc)  -- angle at root
	float fCosAngleRoot = (fUpperLength * fUpperLength + fTargetDist * fTargetDist -
		fLowerLength * fLowerLength) / (2.0f * fUpperLength * fTargetDist);
	fCosAngleRoot = glm::clamp(fCosAngleRoot, -1.0f, 1.0f);
	float fAngleRoot = std::acos(fCosAngleRoot);

	// Direction to target
	Zenith_Maths::Vector3 xToTargetDir = glm::normalize(xToTarget);

	// Initial upper bone direction
	Zenith_Maths::Vector3 xUpperDir = glm::normalize(xMidPos - xRootPos);

	// Compute plane normal from pole vector
	Zenith_Maths::Vector3 xToPole = xPoleVector - xRootPos;
	xToPole = xToPole - xToTargetDir * glm::dot(xToPole, xToTargetDir);
	if (glm::length(xToPole) < 0.0001f)
	{
		xToPole = Zenith_Maths::Vector3(0, 0, 1);
	}
	xToPole = glm::normalize(xToPole);

	// Compute rotation axis (perpendicular to plane containing root, target, and pole)
	Zenith_Maths::Vector3 xRotAxis = glm::cross(xToTargetDir, xToPole);
	if (glm::length(xRotAxis) < 0.0001f)
	{
		xRotAxis = Zenith_Maths::Vector3(1, 0, 0);
	}
	xRotAxis = glm::normalize(xRotAxis);

	// Rotate target direction by angle to get upper bone direction
	Zenith_Maths::Quat xRootRot = glm::angleAxis(fAngleRoot, xRotAxis);
	Zenith_Maths::Vector3 xNewUpperDir = xRootRot * xToTargetDir;

	// Compute rotation from current upper direction to new direction
	xOutRootRotation = RotationBetweenVectors(xUpperDir, xNewUpperDir);

	// Compute new mid position
	Zenith_Maths::Vector3 xNewMidPos = xRootPos + xNewUpperDir * fUpperLength;

	// Compute mid joint rotation
	Zenith_Maths::Vector3 xLowerDir = glm::normalize(xEndPos - xMidPos);
	Zenith_Maths::Vector3 xNewLowerDir = glm::normalize(xTargetPos - xNewMidPos);
	xOutMidRotation = RotationBetweenVectors(xLowerDir, xNewLowerDir);

	return true;
}

Zenith_Maths::Quat SolveLookAtIK(const Zenith_Maths::Vector3& xBonePos,
	const Zenith_Maths::Vector3& xForwardDir,
	const Zenith_Maths::Vector3&,
	const Zenith_Maths::Vector3& xTargetPos,
	float fMaxAngle)
{
	Zenith_Maths::Vector3 xToTarget = xTargetPos - xBonePos;
	if (glm::length(xToTarget) < 0.0001f)
	{
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	xToTarget = glm::normalize(xToTarget);
	Zenith_Maths::Vector3 xForward = glm::normalize(xForwardDir);

	// Compute angle between current forward and target
	float fDot = glm::dot(xForward, xToTarget);
	float fAngle = std::acos(glm::clamp(fDot, -1.0f, 1.0f));

	// Clamp angle
	if (fAngle > fMaxAngle)
	{
		// Limit rotation
		Zenith_Maths::Vector3 xAxis = glm::cross(xForward, xToTarget);
		if (glm::length(xAxis) < 0.0001f)
		{
			return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		}
		xAxis = glm::normalize(xAxis);
		return glm::angleAxis(fMaxAngle, xAxis);
	}

	return RotationBetweenVectors(xForward, xToTarget);
}
