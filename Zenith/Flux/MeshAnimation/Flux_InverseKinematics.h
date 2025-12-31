#pragma once
#include "Flux_BonePose.h"
#include "DataStream/Zenith_DataStream.h"
#include <vector>
#include <unordered_map>
#include <string>

// Forward declarations
class Flux_MeshGeometry;

//=============================================================================
// Flux_IKTarget
// Represents a target position/rotation for an IK chain to reach
//=============================================================================
struct Flux_IKTarget
{
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Quat m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	float m_fWeight = 1.0f;           // Blend weight with animation [0-1]
	bool m_bUseRotation = false;       // Apply rotation constraint on end effector
	bool m_bEnabled = true;            // Is this target active?
};

//=============================================================================
// Flux_JointConstraint
// Limits the rotation of a joint in an IK chain
//=============================================================================
struct Flux_JointConstraint
{
	enum class ConstraintType : uint8_t
	{
		None,           // No constraint
		Hinge,          // Single axis rotation (e.g., elbow, knee)
		BallSocket,     // Cone constraint (e.g., shoulder, hip)
		Twist           // Rotation around bone axis
	};

	ConstraintType m_eType = ConstraintType::None;

	// For Hinge constraint
	Zenith_Maths::Vector3 m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	float m_fMinAngle = -3.14159f;
	float m_fMaxAngle = 3.14159f;

	// For BallSocket constraint (cone)
	float m_fConeAngle = 3.14159f;    // Maximum angle from rest direction

	// For Twist constraint
	float m_fMinTwist = -3.14159f;
	float m_fMaxTwist = 3.14159f;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Flux_IKChain
// Defines a chain of bones for IK solving
//=============================================================================
struct Flux_IKChain
{
	std::string m_strName;                    // "LeftLeg", "RightArm", etc.
	std::vector<std::string> m_xBoneNames;   // Bone names from root to tip
	std::vector<uint32_t> m_xBoneIndices;    // Resolved bone indices (runtime)

	// FABRIK parameters
	uint32_t m_uMaxIterations = 10;
	float m_fTolerance = 0.001f;              // Distance threshold for convergence

	// Pole vector for elbow/knee direction control
	Zenith_Maths::Vector3 m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	bool m_bUsePoleVector = false;
	std::string m_strPoleTargetBone;          // Optional: bone to use as pole target

	// Per-joint constraints
	std::vector<Flux_JointConstraint> m_xJointConstraints;

	// Chain properties (computed at runtime)
	float m_fTotalLength = 0.0f;
	std::vector<float> m_xBoneLengths;

	// Resolve bone names to indices
	void ResolveBoneIndices(const Flux_MeshGeometry& xGeometry);

	// Compute bone lengths from bind pose
	void ComputeBoneLengths(const Flux_SkeletonPose& xPose);

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Flux_IKSolver
// FABRIK-based IK solver for skeletal animation
//=============================================================================
class Flux_IKSolver
{
public:
	Flux_IKSolver() = default;
	~Flux_IKSolver() = default;

	//=========================================================================
	// Chain Management
	//=========================================================================

	// Add a new IK chain
	void AddChain(const Flux_IKChain& xChain);

	// Remove a chain by name
	void RemoveChain(const std::string& strName);

	// Get chain by name
	Flux_IKChain* GetChain(const std::string& strName);
	const Flux_IKChain* GetChain(const std::string& strName) const;

	// Check if chain exists
	bool HasChain(const std::string& strName) const;

	// Get all chains
	const std::unordered_map<std::string, Flux_IKChain>& GetChains() const { return m_xChains; }

	//=========================================================================
	// Target Management
	//=========================================================================

	// Set IK target for a chain
	void SetTarget(const std::string& strChainName, const Flux_IKTarget& xTarget);

	// Clear target for a chain
	void ClearTarget(const std::string& strChainName);

	// Get target for a chain
	const Flux_IKTarget* GetTarget(const std::string& strChainName) const;

	// Check if chain has active target
	bool HasTarget(const std::string& strChainName) const;

	//=========================================================================
	// Solving
	//=========================================================================

	// Apply IK to a skeleton pose
	// Call AFTER animation blending, BEFORE computing final matrices
	void Solve(Flux_SkeletonPose& xPose,
		const Flux_MeshGeometry& xGeometry,
		const Zenith_Maths::Matrix4& xWorldMatrix);

	// Solve a single chain (internal use or for debugging)
	void SolveChain(Flux_SkeletonPose& xPose,
		const Flux_IKChain& xChain,
		const Flux_IKTarget& xTarget,
		const Flux_MeshGeometry& xGeometry);

	//=========================================================================
	// Helper Functions
	//=========================================================================

	// Create common IK chain configurations
	static Flux_IKChain CreateLegChain(const std::string& strName,
		const std::string& strHipBone,
		const std::string& strKneeBone,
		const std::string& strAnkleBone);

	static Flux_IKChain CreateArmChain(const std::string& strName,
		const std::string& strShoulderBone,
		const std::string& strElbowBone,
		const std::string& strWristBone);

	static Flux_IKChain CreateSpineChain(const std::string& strName,
		const std::vector<std::string>& xSpineBones);

	//=========================================================================
	// Serialization
	//=========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	//=========================================================================
	// FABRIK Algorithm Implementation
	//=========================================================================

	// Forward reaching: from end effector to root
	void ForwardReaching(std::vector<Zenith_Maths::Vector3>& xPositions,
		const std::vector<float>& xBoneLengths,
		const Zenith_Maths::Vector3& xTargetPos);

	// Backward reaching: from root to end effector
	void BackwardReaching(std::vector<Zenith_Maths::Vector3>& xPositions,
		const std::vector<float>& xBoneLengths,
		const Zenith_Maths::Vector3& xRootPos);

	// Apply joint constraints
	void ApplyConstraints(std::vector<Zenith_Maths::Vector3>& xPositions,
		const Flux_IKChain& xChain,
		const Flux_SkeletonPose& xOriginalPose);

	// Apply pole vector constraint (for elbow/knee direction)
	void ApplyPoleVectorConstraint(std::vector<Zenith_Maths::Vector3>& xPositions,
		const Flux_IKChain& xChain,
		const Zenith_Maths::Vector3& xPolePosition);

	// Convert final positions back to bone rotations
	void ConvertPositionsToRotations(Flux_SkeletonPose& xPose,
		const Flux_IKChain& xChain,
		const std::vector<Zenith_Maths::Vector3>& xPositions,
		const Flux_MeshGeometry& xGeometry,
		float fWeight);

	//=========================================================================
	// Data
	//=========================================================================

	std::unordered_map<std::string, Flux_IKChain> m_xChains;
	std::unordered_map<std::string, Flux_IKTarget> m_xTargets;
};

//=============================================================================
// Specialized IK Helpers
//=============================================================================

// Compute rotation from one direction to another
Zenith_Maths::Quat RotationBetweenVectors(const Zenith_Maths::Vector3& xFrom,
	const Zenith_Maths::Vector3& xTo);

// Two-bone analytical IK (for simple arm/leg setups)
// Returns true if solution found
bool SolveTwoBoneIK(const Zenith_Maths::Vector3& xRootPos,
	const Zenith_Maths::Vector3& xMidPos,
	const Zenith_Maths::Vector3& xEndPos,
	const Zenith_Maths::Vector3& xTargetPos,
	const Zenith_Maths::Vector3& xPoleVector,
	float fUpperLength,
	float fLowerLength,
	Zenith_Maths::Quat& xOutRootRotation,
	Zenith_Maths::Quat& xOutMidRotation);

// Look-at IK for head/eyes
Zenith_Maths::Quat SolveLookAtIK(const Zenith_Maths::Vector3& xBonePos,
	const Zenith_Maths::Vector3& xForwardDir,
	const Zenith_Maths::Vector3& xUpDir,
	const Zenith_Maths::Vector3& xTargetPos,
	float fMaxAngle = 1.57f);  // 90 degrees
