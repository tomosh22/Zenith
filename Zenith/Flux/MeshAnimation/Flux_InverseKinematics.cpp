#include "Zenith.h"
#include "Flux_InverseKinematics.h"

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
void Flux_IKChain::ResolveBoneIndices(const Zenith_SkeletonAsset& xSkeleton)
{
	m_xBoneIndices.Clear();
	m_xBoneIndices.Reserve(m_xBoneNames.GetSize());

	for (const std::string& strName : m_xBoneNames)
	{
		const int32_t iIdx = xSkeleton.GetBoneIndex(strName);
		if (iIdx != Zenith_SkeletonAsset::INVALID_BONE_INDEX)
		{
			m_xBoneIndices.PushBack(static_cast<uint32_t>(iIdx));
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[IK] Warning: Bone '%s' not found in skeleton", strName.c_str());
			m_xBoneIndices.PushBack(~0u);  // Invalid index
		}
	}
}

void Flux_IKChain::ComputeBoneLengths(const Flux_SkeletonPose& xPose)
{
	m_xBoneLengths.Clear();
	m_fTotalLength = 0.0f;

	if (m_xBoneIndices.GetSize() < 2)
		return;

	m_xBoneLengths.Reserve(m_xBoneIndices.GetSize() - 1);

	for (u_int i = 0; i < m_xBoneIndices.GetSize() - 1; ++i)
	{
		uint32_t uCurrent = m_xBoneIndices.Get(i);
		uint32_t uNext = m_xBoneIndices.Get(i + 1);

		if (uCurrent == ~0u || uNext == ~0u)
		{
			m_xBoneLengths.PushBack(0.0f);
			continue;
		}

		Zenith_Maths::Vector3 xCurrentPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uCurrent)[3]);
		Zenith_Maths::Vector3 xNextPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uNext)[3]);

		float fLength = glm::length(xNextPos - xCurrentPos);
		m_xBoneLengths.PushBack(fLength);
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

	uint32_t uNumBones = static_cast<uint32_t>(m_xBoneNames.GetSize());
	xStream << uNumBones;
	for (const auto& strName : m_xBoneNames)
	{
		xStream << strName;
	}

	uint32_t uNumConstraints = static_cast<uint32_t>(m_xJointConstraints.GetSize());
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
	// Migration path: pre-2026-05 saves stored m_xPoleVector as a world-space
	// position. The new convention is a unit direction. If we read a vector that
	// isn't approximately unit length, normalize it — for the common case of
	// (0, 0, ±1) this is a no-op, and for stale position values it produces a
	// best-effort direction rather than letting the solver consume garbage.
	{
		// "Approximately unit length" = squared length within [0.9, 1.1]^2 =
		// [0.81, 1.21]. Outside that band (and not near-zero) the value is a stale
		// world-space position from an old save, so normalize to a best-effort dir.
		constexpr float fPOLE_MIN_NONZERO_LENSQ = 0.0001f;
		constexpr float fPOLE_UNIT_LENSQ_MIN    = 0.81f; // 0.9^2
		constexpr float fPOLE_UNIT_LENSQ_MAX    = 1.21f; // 1.1^2
		const float fLenSq = glm::dot(m_xPoleVector, m_xPoleVector);
		if (fLenSq > fPOLE_MIN_NONZERO_LENSQ && (fLenSq < fPOLE_UNIT_LENSQ_MIN || fLenSq > fPOLE_UNIT_LENSQ_MAX))
		{
			m_xPoleVector = m_xPoleVector / std::sqrt(fLenSq);
		}
	}
	xStream >> m_strPoleTargetBone;

	uint32_t uNumBones = 0;
	xStream >> uNumBones;
	m_xBoneNames.Clear();
	m_xBoneNames.Reserve(uNumBones);
	for (uint32_t i = 0; i < uNumBones; ++i)
	{
		std::string strName;
		xStream >> strName;
		m_xBoneNames.PushBack(std::move(strName));
	}

	uint32_t uNumConstraints = 0;
	xStream >> uNumConstraints;
	m_xJointConstraints.Clear();
	m_xJointConstraints.Reserve(uNumConstraints);
	for (uint32_t i = 0; i < uNumConstraints; ++i)
	{
		Flux_JointConstraint xConstraint;
		xConstraint.ReadFromDataStream(xStream);
		m_xJointConstraints.PushBack(xConstraint);
	}

	m_xBoneIndices.Clear();
	m_xBoneLengths.Clear();
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
	m_xChains.Remove(strName);
	m_xTargets.Remove(strName);
}

Flux_IKChain* Flux_IKSolver::GetChain(const std::string& strName)
{
	return m_xChains.TryGet(strName);
}

const Flux_IKChain* Flux_IKSolver::GetChain(const std::string& strName) const
{
	return m_xChains.TryGet(strName);
}

bool Flux_IKSolver::HasChain(const std::string& strName) const
{
	return m_xChains.Contains(strName);
}

void Flux_IKSolver::SetTarget(const std::string& strChainName, const Flux_IKTarget& xTarget)
{
	m_xTargets[strChainName] = xTarget;
}

void Flux_IKSolver::ClearTarget(const std::string& strChainName)
{
	m_xTargets.Remove(strChainName);
}

const Flux_IKTarget* Flux_IKSolver::GetTarget(const std::string& strChainName) const
{
	return m_xTargets.TryGet(strChainName);
}

bool Flux_IKSolver::HasTarget(const std::string& strChainName) const
{
	return m_xTargets.Contains(strChainName);
}

//=============================================================================
// Static Helpers
//=============================================================================
Zenith_Maths::Vector3 Flux_IKSolver::SafeNormalize(const Zenith_Maths::Vector3& xVec,
	const Zenith_Maths::Vector3& xFallback,
	float fEpsilon)
{
	float fLen = glm::length(xVec);
	if (fLen > fEpsilon)
	{
		return xVec / fLen;
	}
	return xFallback;
}

Zenith_Maths::Vector3 Flux_IKSolver::FindPerpendicularAxis(const Zenith_Maths::Vector3& xVec)
{
	Zenith_Maths::Vector3 xAxis = glm::cross(Zenith_Maths::Vector3(1, 0, 0), xVec);
	if (glm::length(xAxis) < 0.0001f)
	{
		xAxis = glm::cross(Zenith_Maths::Vector3(0, 1, 0), xVec);
	}
	return glm::normalize(xAxis);
}

Zenith_Maths::Vector3 Flux_IKSolver::ConstrainBoneLength(const Zenith_Maths::Vector3& xChildPos,
	const Zenith_Maths::Vector3& xParentPos,
	float fLength)
{
	Zenith_Maths::Vector3 xDir = xChildPos - xParentPos;
	Zenith_Maths::Vector3 xNormDir = SafeNormalize(xDir, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	return xParentPos + xNormDir * fLength;
}

void Flux_IKSolver::Solve(Flux_SkeletonPose& xPose,
	const Zenith_SkeletonAsset& xSkeleton,
	const Zenith_Maths::Matrix4& xWorldMatrix)
{
	for (Zenith_HashMap<std::string, Flux_IKChain>::Iterator xIt(m_xChains); !xIt.Done(); xIt.Next())
	{
		Flux_IKChain& xChain = xIt.GetValueMutable();
		const std::string& strChainName = xIt.GetKey();

		// Check if chain has active target
		const Flux_IKTarget* pxTarget = m_xTargets.TryGet(strChainName);
		if (pxTarget == nullptr || !pxTarget->m_bEnabled)
			continue;

		// E3d: skip solver work entirely when target weight is non-positive — the
		// weighted slerp inside ConvertPositionsToRotations would produce identity
		// anyway, so the FABRIK iterations are pure waste.
		if (pxTarget->m_fWeight <= 0.0f)
			continue;

		// Resolve bone indices if needed
		if (xChain.m_xBoneIndices.GetSize() == 0)
			xChain.ResolveBoneIndices(xSkeleton);

		// Compute bone lengths if needed
		if (xChain.m_xBoneLengths.GetSize() == 0)
			xChain.ComputeBoneLengths(xPose);

		// Transform target to model space (unless caller already did the conversion)
		Flux_IKTarget xModelSpaceTarget = *pxTarget;
		if (!xModelSpaceTarget.m_bIsModelSpace)
		{
			Zenith_Maths::Matrix4 xInvWorld = glm::inverse(xWorldMatrix);
			Zenith_Maths::Vector4 xTargetWorld = Zenith_Maths::Vector4(xModelSpaceTarget.m_xPosition, 1.0f);
			xModelSpaceTarget.m_xPosition = Zenith_Maths::Vector3(xInvWorld * xTargetWorld);

			// Mirror the position's world->model conversion for the end-effector
			// orientation so a world-space target rotation lands in skeleton space
			// (assumes a roughly-orthonormal world matrix; entities driving IK are
			// unit-scaled).
			if (xModelSpaceTarget.m_bUseRotation)
			{
				const Zenith_Maths::Quat xWorldRot = glm::quat_cast(Zenith_Maths::Matrix3(xWorldMatrix));
				xModelSpaceTarget.m_xRotation = glm::normalize(glm::inverse(xWorldRot) * xModelSpaceTarget.m_xRotation);
			}
		}

		// Solve the chain
		SolveChain(xPose, xChain, xModelSpaceTarget, xSkeleton);
	}
}

void Flux_IKSolver::SolveChain(Flux_SkeletonPose& xPose,
	const Flux_IKChain& xChain,
	const Flux_IKTarget& xTarget,
	const Zenith_SkeletonAsset& xSkeleton)
{
	if (xChain.m_xBoneIndices.GetSize() < 2 || xChain.m_xBoneLengths.GetSize() == 0)
		return;

	const uint32_t uNumBones = static_cast<uint32_t>(xChain.m_xBoneIndices.GetSize());

	// Extract bone positions from current pose (model space)
	Zenith_Vector<Zenith_Maths::Vector3> xBonePositions(uNumBones);
	for (uint32_t i = 0; i < uNumBones; ++i)
	{
		xBonePositions.PushBack(Zenith_Maths::Vector3(0.0f));
	}
	for (uint32_t i = 0; i < uNumBones; ++i)
	{
		uint32_t uBoneIndex = xChain.m_xBoneIndices.Get(i);
		if (uBoneIndex != ~0u)
		{
			xBonePositions.Get(i) = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uBoneIndex)[3]);
		}
	}

	const Zenith_Maths::Vector3 xRootPos = xBonePositions.Get(0);
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
			fAccumLength += xChain.m_xBoneLengths.Get(i - 1);
			xBonePositions.Get(i) = xRootPos + xDirection * fAccumLength;
		}
	}
	else
	{
		// Pre-FABRIK collinearity bias. When the initial chain is collinear with
		// the target direction (foot hanging straight down, target straight below
		// player — exactly the foot-IK case), FABRIK has no preferred bend
		// direction and oscillates between collapsed states. The pole-vector
		// constraint can't recover because once the chain collapses (root==end)
		// it computes a zero main axis and early-outs. Fix: nudge the middle
		// joints slightly toward a chosen perpendicular direction BEFORE iteration
		// starts so FABRIK has a non-degenerate frame to converge from.
		if (uNumBones >= 3)
		{
			const Zenith_Maths::Vector3 xChainAxis = SafeNormalize(xBonePositions.Get(uNumBones - 1) - xRootPos);
			const Zenith_Maths::Vector3 xTargetAxis = SafeNormalize(xTargetPos - xRootPos);
			// Detect collinearity of initial chain with target axis. dot ~ ±1 means parallel.
			const float fCollinearity = std::abs(glm::dot(xChainAxis, xTargetAxis));
			if (fCollinearity > 0.999f && glm::length(xChainAxis) > 0.0001f)
			{
				// Choose bias direction. Use the pole vector AS A DIRECTION here
				// (not "pole-relative-to-root" the way ApplyPoleVectorConstraint
				// treats it). For pre-bias purposes we just need any side to bend
				// toward; using the pole as a direction avoids introducing a stray
				// X component that would conflict with a knee hinge constraint.
				Zenith_Maths::Vector3 xBiasDir(0.0f);
				if (xChain.m_bUsePoleVector)
				{
					Zenith_Maths::Vector3 xPole = xChain.m_xPoleVector;
					xPole -= xChainAxis * glm::dot(xPole, xChainAxis);
					xBiasDir = SafeNormalize(xPole);
				}
				if (glm::length(xBiasDir) < 0.0001f)
				{
					xBiasDir = FindPerpendicularAxis(xChainAxis);
				}
				// Nudge middle joints by a small amount (5% of bone length).
				// FABRIK's length-constraint passes will repair bone lengths but
				// the non-collinear configuration will persist as a hint to FABRIK
				// (and the pole-vector constraint) about which side to bend toward.
				for (uint32_t i = 1; i < uNumBones - 1; ++i)
				{
					const float fPerturb = 0.05f * xChain.m_xBoneLengths.Get(i - 1);
					xBonePositions.Get(i) += xBiasDir * fPerturb;
				}
			}
		}

		// FABRIK iterations
		for (uint32_t iter = 0; iter < xChain.m_uMaxIterations; ++iter)
		{
			// Forward reaching: from end effector to root
			ForwardReaching(xBonePositions, xChain.m_xBoneLengths, xTargetPos);

			// Backward reaching: from root to end effector
			BackwardReaching(xBonePositions, xChain.m_xBoneLengths, xRootPos);

			// Apply joint constraints if any
			if (xChain.m_xJointConstraints.GetSize() != 0)
			{
				ApplyConstraints(xBonePositions, xChain, xPose);
			}

			// Apply pole vector constraint
			if (xChain.m_bUsePoleVector && uNumBones >= 3)
			{
				ApplyPoleVectorConstraint(xBonePositions, xChain, xChain.m_xPoleVector);
			}

			// Check convergence
			float fError = glm::length(xBonePositions.Get(uNumBones - 1) - xTargetPos);
			if (fError < xChain.m_fTolerance)
				break;
		}
	}

	// Convert positions back to bone rotations
	ConvertPositionsToRotations(xPose, xChain, xBonePositions, xSkeleton, xTarget);
}

void Flux_IKSolver::ForwardReaching(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Zenith_Vector<float>& xBoneLengths,
	const Zenith_Maths::Vector3& xTargetPos)
{
	const u_int uNumBones = xPositions.GetSize();
	if (uNumBones < 2)
		return;

	// Set end effector to target
	xPositions.Get(uNumBones - 1) = xTargetPos;

	// Work backward to root
	for (int32_t i = static_cast<int32_t>(uNumBones) - 2; i >= 0; --i)
	{
		const u_int uI = static_cast<u_int>(i);
		xPositions.Get(uI) = ConstrainBoneLength(xPositions.Get(uI), xPositions.Get(uI + 1), xBoneLengths.Get(uI));
	}
}

void Flux_IKSolver::BackwardReaching(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Zenith_Vector<float>& xBoneLengths,
	const Zenith_Maths::Vector3& xRootPos)
{
	const u_int uNumBones = xPositions.GetSize();
	if (uNumBones < 2)
		return;

	// Fix root position
	xPositions.Get(0) = xRootPos;

	// Work forward to end effector
	for (u_int i = 0; i < uNumBones - 1; ++i)
	{
		xPositions.Get(i + 1) = ConstrainBoneLength(xPositions.Get(i + 1), xPositions.Get(i), xBoneLengths.Get(i));
	}
}

void Flux_IKSolver::ApplyHingeConstraint(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	u_int uJointIndex,
	const Flux_JointConstraint& xConstraint,
	float fBoneLength)
{
	if (uJointIndex == 0 || uJointIndex + 1 >= xPositions.GetSize())
		return;

	// Project movement onto plane perpendicular to hinge axis
	Zenith_Maths::Vector3 xBoneDir = xPositions.Get(uJointIndex + 1) - xPositions.Get(uJointIndex);
	const Zenith_Maths::Vector3& xAxis = xConstraint.m_xHingeAxis;

	// Remove component along hinge axis
	float fDot = glm::dot(xBoneDir, xAxis);
	xBoneDir = xBoneDir - xAxis * fDot;

	Zenith_Maths::Vector3 xNormDir = SafeNormalize(xBoneDir);
	if (glm::length(xNormDir) > 0.0001f)
	{
		xPositions.Get(uJointIndex + 1) = xPositions.Get(uJointIndex) + xNormDir * fBoneLength;
	}
}

void Flux_IKSolver::ApplyBallSocketConstraint(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	u_int uJointIndex,
	const Flux_JointConstraint& xConstraint,
	const Flux_IKChain& xChain,
	const Flux_SkeletonPose& xOriginalPose)
{
	if (uJointIndex == 0 || uJointIndex + 1 >= xPositions.GetSize() || uJointIndex >= xChain.m_xBoneIndices.GetSize())
		return;

	uint32_t uBoneIdx = xChain.m_xBoneIndices.Get(uJointIndex);
	if (uBoneIdx == ~0u)
		return;

	// Original bone direction from the pose
	Zenith_Maths::Vector3 xOrigDir = Zenith_Maths::Vector3(
		xOriginalPose.GetModelSpaceMatrix(uBoneIdx) *
		Zenith_Maths::Vector4(0, 1, 0, 0)
	);

	Zenith_Maths::Vector3 xNewDir = glm::normalize(xPositions.Get(uJointIndex + 1) - xPositions.Get(uJointIndex));
	float fAngle = std::acos(glm::clamp(glm::dot(xOrigDir, xNewDir), -1.0f, 1.0f));

	if (fAngle > xConstraint.m_fConeAngle)
	{
		// Clamp to cone boundary
		Zenith_Maths::Vector3 xRotAxis = glm::cross(xOrigDir, xNewDir);
		Zenith_Maths::Vector3 xNormAxis = SafeNormalize(xRotAxis);
		if (glm::length(xNormAxis) > 0.0001f)
		{
			Zenith_Maths::Quat xRotation = glm::angleAxis(xConstraint.m_fConeAngle, xNormAxis);
			Zenith_Maths::Vector3 xClampedDir = xRotation * xOrigDir;
			xPositions.Get(uJointIndex + 1) = xPositions.Get(uJointIndex) + xClampedDir * xChain.m_xBoneLengths.Get(uJointIndex);
		}
	}
}

void Flux_IKSolver::ApplyConstraints(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Flux_IKChain& xChain,
	const Flux_SkeletonPose& xOriginalPose)
{
	// Loop must also check bone lengths bounds (m_xBoneLengths.GetSize() == m_xBoneIndices.GetSize() - 1)
	const u_int uMaxIndex = std::min({xChain.m_xJointConstraints.GetSize(), xPositions.GetSize(), xChain.m_xBoneLengths.GetSize()});
	for (u_int i = 0; i < uMaxIndex; ++i)
	{
		const Flux_JointConstraint& xConstraint = xChain.m_xJointConstraints.Get(i);

		switch (xConstraint.m_eType)
		{
		case Flux_JointConstraint::ConstraintType::Hinge:
			ApplyHingeConstraint(xPositions, i, xConstraint, xChain.m_xBoneLengths.Get(i));
			break;

		case Flux_JointConstraint::ConstraintType::BallSocket:
			ApplyBallSocketConstraint(xPositions, i, xConstraint, xChain, xOriginalPose);
			break;

		default:
			break;
		}
	}
}

void Flux_IKSolver::ApplyPoleVectorConstraint(Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Flux_IKChain& xChain,
	const Zenith_Maths::Vector3& xPoleVector)
{
	if (xPositions.GetSize() < 3)
		return;

	// For a 3-bone chain (like arm or leg), rotate the middle joint to point
	// toward the pole DIRECTION. Treating m_xPoleVector as a direction (not a
	// world-space position) avoids spurious lateral pulls on the middle joint
	// when the chain root is offset from the model origin (the leg case: upper-leg
	// is at -0.15m X, pole at (0,0,1) — if pole were treated as a position, the
	// constraint would pull the knee toward +X by 0.15m, which conflicts with a
	// hinge constraint on the same axis). CreateLegChain comments confirm this:
	// the pole is documented as "Forward" — a direction, not a coordinate.
	const Zenith_Maths::Vector3& xRoot = xPositions.Get(0);
	const Zenith_Maths::Vector3& xEnd = xPositions.Get(xPositions.GetSize() - 1);

	// Main axis from root to end
	Zenith_Maths::Vector3 xMainAxis = SafeNormalize(xEnd - xRoot);
	if (glm::length(xMainAxis) < 0.0001f)
		return;

	// Project pole DIRECTION onto plane perpendicular to main axis.
	Zenith_Maths::Vector3 xToPole = xPoleVector;
	xToPole = xToPole - xMainAxis * glm::dot(xToPole, xMainAxis);

	xToPole = SafeNormalize(xToPole);
	if (glm::length(xToPole) < 0.0001f)
		return;

	// For each middle joint, ensure it's on the pole side
	for (u_int i = 1; i < xPositions.GetSize() - 1; ++i)
	{
		Zenith_Maths::Vector3 xToJoint = xPositions.Get(i) - xRoot;
		xToJoint = xToJoint - xMainAxis * glm::dot(xToJoint, xMainAxis);

		if (glm::length(xToJoint) < 0.0001f)
			continue;

		float fCurrentDist = glm::length(xToJoint);
		Zenith_Maths::Vector3 xNewJointOffset = xToPole * fCurrentDist;

		// Project root position along main axis
		float fAlongMain = glm::dot(xPositions.Get(i) - xRoot, xMainAxis);
		xPositions.Get(i) = xRoot + xMainAxis * fAlongMain + xNewJointOffset;
	}

	// Re-apply bone length constraints after pole adjustment
	for (u_int i = 0; i < xPositions.GetSize() - 1; ++i)
	{
		xPositions.Get(i + 1) = ConstrainBoneLength(xPositions.Get(i + 1), xPositions.Get(i), xChain.m_xBoneLengths.Get(i));
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
		Zenith_Maths::Vector3 xAxis = Flux_IKSolver::FindPerpendicularAxis(xFromNorm);
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
		Zenith_Maths::Vector3 xFallbackAxis = Flux_IKSolver::FindPerpendicularAxis(xFromNorm);
		return glm::angleAxis(3.14159265f, xFallbackAxis);
	}

	float invS = 1.0f / s;

	return Zenith_Maths::Quat(s * 0.5f, xAxis.x * invS, xAxis.y * invS, xAxis.z * invS);
}

void Flux_IKSolver::ConvertPositionsToRotations(Flux_SkeletonPose& xPose,
	const Flux_IKChain& xChain,
	const Zenith_Vector<Zenith_Maths::Vector3>& xPositions,
	const Zenith_SkeletonAsset& xSkeleton,
	const Flux_IKTarget& xTarget)
{
	if (xChain.m_xBoneIndices.GetSize() < 2)
		return;

	const float fWeight = xTarget.m_fWeight;
	const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);

	// For each bone (except the last), compute the rotation needed to point
	// toward the next bone in the chain. Two correctness requirements:
	//   1. The model-space rotation delta must be converted to parent-local
	//      space before being applied to m_xRotation, otherwise non-root bones
	//      with rotated parents end up rotating in the wrong frame.
	//   2. After updating bone i's local rotation, model-space matrices for
	//      bone i and its descendants are stale. Recompute the whole pose
	//      between iterations so the next bone's "current" direction reads
	//      from the just-updated hierarchy.
	for (u_int i = 0; i < xChain.m_xBoneIndices.GetSize() - 1; ++i)
	{
		uint32_t uBoneIndex = xChain.m_xBoneIndices.Get(i);
		uint32_t uChildIndex = xChain.m_xBoneIndices.Get(i + 1);

		if (uBoneIndex == ~0u || uChildIndex == ~0u)
			continue;

		// Current model-space child direction. Fresh on first iteration thanks
		// to the pre-solve recompute in Flux_AnimationController; fresh on later
		// iterations thanks to the per-bone recompute at the end of this loop.
		Zenith_Maths::Vector3 xCurrentChildPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uChildIndex)[3]);
		Zenith_Maths::Vector3 xCurrentPos = Zenith_Maths::Vector3(xPose.GetModelSpaceMatrix(uBoneIndex)[3]);
		Zenith_Maths::Vector3 xCurrentDir = SafeNormalize(xCurrentChildPos - xCurrentPos);

		if (glm::length(xCurrentDir) < 0.0001f)
			continue;

		// Target direction from FABRIK position buffer
		Zenith_Maths::Vector3 xTargetDir = SafeNormalize(xPositions.Get(i + 1) - xPositions.Get(i));
		if (glm::length(xTargetDir) < 0.0001f)
			continue;

		// Model-space delta rotation
		Zenith_Maths::Quat xDeltaModel = RotationBetweenVectors(xCurrentDir, xTargetDir);

		// Resolve the bone's parent in the skeleton hierarchy. For root chain bones
		// (or bones whose parent is outside the resolved skeleton) the parent rotation
		// is identity, so model-space and parent-local-space deltas coincide.
		Zenith_Maths::Quat xParentModelRotation = xIdentity;
		if (uBoneIndex < xSkeleton.GetNumBones())
		{
			const int32_t iParent = xSkeleton.GetBone(uBoneIndex).m_iParentIndex;
			if (iParent != Zenith_SkeletonAsset::INVALID_BONE_INDEX
				&& static_cast<uint32_t>(iParent) < xPose.GetNumBones())
			{
				xParentModelRotation = glm::quat_cast(
					Zenith_Maths::Matrix3(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(iParent))));
			}
		}

		// Convert the model-space delta into parent-local space via conjugation.
		const Zenith_Maths::Quat xParentInv = glm::inverse(xParentModelRotation);
		Zenith_Maths::Quat xDeltaLocal = xParentInv * xDeltaModel * xParentModelRotation;

		// Apply weighted delta to local rotation
		Flux_BoneLocalPose& xLocalPose = xPose.GetLocalPose(uBoneIndex);
		Zenith_Maths::Quat xWeightedDelta = glm::slerp(xIdentity, xDeltaLocal, fWeight);
		xLocalPose.m_xRotation = xWeightedDelta * xLocalPose.m_xRotation;

		// Refresh the whole pose's model-space matrices so the next iteration's
		// model-space reads (and the children's positions) reflect the just-applied
		// rotation. Cheap on small skeletons; trivial relative to the FABRIK iters.
		xPose.ComputeModelSpaceMatricesFromSkeleton(xSkeleton);
	}

	// End-effector orientation. The loop above only orients each bone toward the
	// NEXT joint, so the tip bone (hand/wrist) keeps whatever orientation the
	// animation left it with. When the target requests an explicit end-effector
	// orientation, drive the tip bone's local rotation so its model-space
	// orientation matches the (model-space) target rotation, blended by the same
	// weight. This is what lets a rigidly-attached tool (e.g. a racket blade) be
	// squared to a chosen direction rather than wherever the swing clip left the
	// wrist. Applied after the position pass so the parent chain is already posed.
	if (xTarget.m_bUseRotation && fWeight > 0.0f)
	{
		const uint32_t uEndIndex = xChain.m_xBoneIndices.Get(xChain.m_xBoneIndices.GetSize() - 1);
		if (uEndIndex != ~0u && uEndIndex < xPose.GetNumBones())
		{
			// Parent model-space rotation (freshly recomputed by the loop above).
			Zenith_Maths::Quat xParentModelRotation = xIdentity;
			if (uEndIndex < xSkeleton.GetNumBones())
			{
				const int32_t iParent = xSkeleton.GetBone(uEndIndex).m_iParentIndex;
				if (iParent != Zenith_SkeletonAsset::INVALID_BONE_INDEX
					&& static_cast<uint32_t>(iParent) < xPose.GetNumBones())
				{
					xParentModelRotation = glm::quat_cast(
						Zenith_Maths::Matrix3(xPose.GetModelSpaceMatrix(static_cast<uint32_t>(iParent))));
				}
			}

			// Local rotation that yields the desired model-space orientation:
			//   parentModel * localDesired = targetModel
			//   => localDesired = inverse(parentModel) * targetModel
			const Zenith_Maths::Quat xDesiredLocal =
				glm::normalize(glm::inverse(xParentModelRotation) * xTarget.m_xRotation);

			Flux_BoneLocalPose& xEndLocal = xPose.GetLocalPose(uEndIndex);
			xEndLocal.m_xRotation = glm::slerp(xEndLocal.m_xRotation, xDesiredLocal, fWeight);
			xPose.ComputeModelSpaceMatricesFromSkeleton(xSkeleton);
		}
	}
}

Flux_IKChain Flux_IKSolver::CreateLegChain(const std::string& strName,
	const std::string& strHipBone,
	const std::string& strKneeBone,
	const std::string& strAnkleBone)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames.PushBack(strHipBone);
	xChain.m_xBoneNames.PushBack(strKneeBone);
	xChain.m_xBoneNames.PushBack(strAnkleBone);
	xChain.m_bUsePoleVector = true;
	xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);  // Forward

	// Knee hinge constraint
	Flux_JointConstraint xKneeConstraint;
	xKneeConstraint.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
	xKneeConstraint.m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);  // Side axis
	xKneeConstraint.m_fMinAngle = 0.0f;
	xKneeConstraint.m_fMaxAngle = 2.5f;  // ~143 degrees

	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());  // Hip - no constraint for now
	xChain.m_xJointConstraints.PushBack(xKneeConstraint);
	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());  // Ankle

	return xChain;
}

Flux_IKChain Flux_IKSolver::CreateArmChain(const std::string& strName,
	const std::string& strShoulderBone,
	const std::string& strElbowBone,
	const std::string& strWristBone)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames.PushBack(strShoulderBone);
	xChain.m_xBoneNames.PushBack(strElbowBone);
	xChain.m_xBoneNames.PushBack(strWristBone);
	xChain.m_bUsePoleVector = true;
	xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);  // Behind

	// Elbow hinge constraint
	Flux_JointConstraint xElbowConstraint;
	xElbowConstraint.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
	xElbowConstraint.m_xHingeAxis = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);  // Up axis
	xElbowConstraint.m_fMinAngle = 0.0f;
	xElbowConstraint.m_fMaxAngle = 2.7f;  // ~155 degrees

	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());  // Shoulder
	xChain.m_xJointConstraints.PushBack(xElbowConstraint);
	xChain.m_xJointConstraints.PushBack(Flux_JointConstraint());  // Wrist

	return xChain;
}

Flux_IKChain Flux_IKSolver::CreateSpineChain(const std::string& strName,
	const Zenith_Vector<std::string>& xSpineBones)
{
	Flux_IKChain xChain;
	xChain.m_strName = strName;
	xChain.m_xBoneNames = xSpineBones;
	xChain.m_bUsePoleVector = false;

	// Ball-socket constraints for each spine bone
	for (u_int i = 0; i < xSpineBones.GetSize(); ++i)
	{
		Flux_JointConstraint xConstraint;
		xConstraint.m_eType = Flux_JointConstraint::ConstraintType::BallSocket;
		xConstraint.m_fConeAngle = 0.35f;  // ~20 degrees per vertebra
		xChain.m_xJointConstraints.PushBack(xConstraint);
	}

	return xChain;
}

void Flux_IKSolver::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumChains = static_cast<uint32_t>(m_xChains.GetSize());
	xStream << uNumChains;

	for (Zenith_HashMap<std::string, Flux_IKChain>::Iterator xIt(m_xChains); !xIt.Done(); xIt.Next())
	{
		xIt.GetValue().WriteToDataStream(xStream);
	}
}

void Flux_IKSolver::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_xChains.Clear();
	m_xTargets.Clear();

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

	// Project pole DIRECTION onto plane perpendicular to root-to-target axis.
	// xPoleVector is treated as a direction (matching ApplyPoleVectorConstraint
	// and the CreateLegChain/CreateArmChain factories).
	Zenith_Maths::Vector3 xToPole = xPoleVector;
	xToPole = xToPole - xToTargetDir * glm::dot(xToPole, xToTargetDir);
	xToPole = Flux_IKSolver::SafeNormalize(xToPole, Zenith_Maths::Vector3(0, 0, 1));

	// Compute rotation axis (perpendicular to plane containing root, target, and pole)
	Zenith_Maths::Vector3 xRotAxis = glm::cross(xToTargetDir, xToPole);
	xRotAxis = Flux_IKSolver::SafeNormalize(xRotAxis, Zenith_Maths::Vector3(1, 0, 0));

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
	Zenith_Maths::Vector3 xToTarget = Flux_IKSolver::SafeNormalize(xTargetPos - xBonePos);
	if (glm::length(xToTarget) < 0.0001f)
	{
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	Zenith_Maths::Vector3 xForward = glm::normalize(xForwardDir);

	// Compute angle between current forward and target
	float fDot = glm::dot(xForward, xToTarget);
	float fAngle = std::acos(glm::clamp(fDot, -1.0f, 1.0f));

	// Clamp angle
	if (fAngle > fMaxAngle)
	{
		// Limit rotation
		Zenith_Maths::Vector3 xAxis = Flux_IKSolver::SafeNormalize(glm::cross(xForward, xToTarget));
		if (glm::length(xAxis) < 0.0001f)
		{
			return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		}
		return glm::angleAxis(fMaxAngle, xAxis);
	}

	return RotationBetweenVectors(xForward, xToTarget);
}
