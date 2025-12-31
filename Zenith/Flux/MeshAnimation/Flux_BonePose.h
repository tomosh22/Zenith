#pragma once
#include "Maths/Zenith_Maths.h"
#include "Flux_AnimationClip.h"
#include "Flux_MeshAnimation.h"
#include <vector>
#include <string>

// Forward declarations
class Flux_MeshGeometry;
class Zenith_SkeletonAsset;

//=============================================================================
// Constants
//=============================================================================
static constexpr uint32_t FLUX_MAX_BONES = 100;

//=============================================================================
// Flux_BoneLocalPose
// Represents the local transform of a single bone (position, rotation, scale)
//=============================================================================
struct Flux_BoneLocalPose
{
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Quat m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
	Zenith_Maths::Vector3 m_xScale = Zenith_Maths::Vector3(1.0f);

	// Default constructor
	Flux_BoneLocalPose() = default;

	// Constructor with values
	Flux_BoneLocalPose(const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Quat& xRot,
		const Zenith_Maths::Vector3& xScale)
		: m_xPosition(xPos), m_xRotation(xRot), m_xScale(xScale) {}

	// Create identity pose
	static Flux_BoneLocalPose Identity()
	{
		return Flux_BoneLocalPose(
			Zenith_Maths::Vector3(0.0f),
			Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
			Zenith_Maths::Vector3(1.0f)
		);
	}

	// Convert to 4x4 transformation matrix
	Zenith_Maths::Matrix4 ToMatrix() const;

	// Set from a 4x4 transformation matrix (decompose)
	void FromMatrix(const Zenith_Maths::Matrix4& xMatrix);

	//=========================================================================
	// Blending Operations
	//=========================================================================

	// Linear blend between two poses (t=0 returns A, t=1 returns B)
	static Flux_BoneLocalPose Blend(const Flux_BoneLocalPose& xA,
		const Flux_BoneLocalPose& xB,
		float fBlendFactor);

	// Additive blend: result = base + (additive - reference) * weight
	// Use when layering animations (e.g., hit reaction on top of locomotion)
	static Flux_BoneLocalPose AdditiveBlend(const Flux_BoneLocalPose& xBase,
		const Flux_BoneLocalPose& xAdditive,
		float fWeight);

	// Additive blend with explicit reference pose
	static Flux_BoneLocalPose AdditiveBlendWithReference(const Flux_BoneLocalPose& xBase,
		const Flux_BoneLocalPose& xAdditive,
		const Flux_BoneLocalPose& xReference,
		float fWeight);
};

//=============================================================================
// Flux_SkeletonPose
// Complete pose for an entire skeleton (all bones)
//=============================================================================
class Flux_SkeletonPose
{
public:
	Flux_SkeletonPose();

	// Initialize for a specific skeleton
	void Initialize(uint32_t uNumBones);
	void Reset();

	// Accessors
	uint32_t GetNumBones() const { return m_uNumBones; }

	Flux_BoneLocalPose& GetLocalPose(uint32_t uBoneIndex);
	const Flux_BoneLocalPose& GetLocalPose(uint32_t uBoneIndex) const;

	const Zenith_Maths::Matrix4& GetModelSpaceMatrix(uint32_t uBoneIndex) const;
	const Zenith_Maths::Matrix4& GetSkinningMatrix(uint32_t uBoneIndex) const;

	// Get pointer to skinning matrices for GPU upload
	const Zenith_Maths::Matrix4* GetSkinningMatrices() const { return m_axSkinningMatrices; }

	//=========================================================================
	// Pose Computation
	//=========================================================================

	// Sample a pose from an animation clip at a specific time (legacy mesh geometry)
	void SampleFromClip(const Flux_AnimationClip& xClip,
		float fTime,
		const Flux_MeshGeometry& xGeometry);

	// Sample a pose from an animation clip using skeleton asset (new model instance system)
	void SampleFromClip(const Flux_AnimationClip& xClip,
		float fTime,
		const Zenith_SkeletonAsset& xSkeleton);

	// Compute model-space matrices from local poses using skeleton hierarchy
	// Must be called after setting local poses and before computing skinning matrices
	void ComputeModelSpaceMatrices(const struct Flux_MeshAnimation::Node& xRootNode,
		const Flux_MeshGeometry& xGeometry);

	// Alternative: compute model space from a flat bone hierarchy (for clips without node tree)
	void ComputeModelSpaceMatricesFlat(const Flux_MeshGeometry& xGeometry);

	// Compute final skinning matrices from model-space matrices
	// skinning = modelSpace * inverseBindPose
	void ComputeSkinningMatrices(const Flux_MeshGeometry& xGeometry);

	//=========================================================================
	// Blending Operations (operate on entire poses)
	//=========================================================================

	// Linear blend between two poses
	static void Blend(Flux_SkeletonPose& xOut,
		const Flux_SkeletonPose& xA,
		const Flux_SkeletonPose& xB,
		float fBlendFactor);

	// Additive blend: out = base + (additive - identity) * weight
	static void AdditiveBlend(Flux_SkeletonPose& xOut,
		const Flux_SkeletonPose& xBase,
		const Flux_SkeletonPose& xAdditive,
		float fWeight);

	// Masked blend using per-bone weights
	// boneMask[i] = 0 means use xLower, boneMask[i] = 1 means use xUpper
	static void MaskedBlend(Flux_SkeletonPose& xOut,
		const Flux_SkeletonPose& xLower,
		const Flux_SkeletonPose& xUpper,
		const std::vector<float>& xBoneMask);

	// Copy pose data
	void CopyFrom(const Flux_SkeletonPose& xOther);

private:
	// Recursive helper for computing model-space matrices
	void ComputeModelSpaceMatricesRecursive(const struct Flux_MeshAnimation::Node* pxNode,
		const Zenith_Maths::Matrix4& xParentTransform,
		const Flux_MeshGeometry& xGeometry);

	uint32_t m_uNumBones = 0;

	// Local space poses (one per bone, indexed by bone ID)
	Flux_BoneLocalPose m_axLocalPoses[FLUX_MAX_BONES];

	// Model space matrices (accumulated from root to each bone)
	Zenith_Maths::Matrix4 m_axModelSpaceMatrices[FLUX_MAX_BONES];

	// Final skinning matrices (model space * inverse bind pose)
	// This is what gets uploaded to the GPU
	Zenith_Maths::Matrix4 m_axSkinningMatrices[FLUX_MAX_BONES];
};

//=============================================================================
// Flux_BoneMask
// Defines which bones are affected by certain operations (e.g., upper body mask)
//=============================================================================
class Flux_BoneMask
{
public:
	Flux_BoneMask();

	// Initialize from bone names (weight 1.0 for named bones, 0.0 for others)
	void SetFromBoneNames(const std::vector<std::string>& xBoneNames,
		const Flux_MeshGeometry& xGeometry);

	// Set weight for specific bone
	void SetBoneWeight(uint32_t uBoneIndex, float fWeight);

	// Get weight for specific bone
	float GetBoneWeight(uint32_t uBoneIndex) const;

	// Get all weights for use in MaskedBlend
	const std::vector<float>& GetWeights() const { return m_xWeights; }

	// Common masks
	static Flux_BoneMask CreateUpperBodyMask(const Flux_MeshGeometry& xGeometry,
		const std::string& strSpineBoneName);
	static Flux_BoneMask CreateLowerBodyMask(const Flux_MeshGeometry& xGeometry,
		const std::string& strSpineBoneName);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::vector<float> m_xWeights;
};

//=============================================================================
// Flux_CrossFadeTransition
// Manages smooth blending between two poses over time
//=============================================================================
class Flux_CrossFadeTransition
{
public:
	Flux_CrossFadeTransition() = default;

	// Start a new transition
	void Start(const Flux_SkeletonPose& xFromPose, float fDuration);

	// Update the transition (call each frame)
	// Returns true if transition is still active
	bool Update(float fDt);

	// Blend with a target pose
	// target is the pose being transitioned TO (from the new state's blend tree)
	void Blend(Flux_SkeletonPose& xOut, const Flux_SkeletonPose& xTarget) const;

	// Check if transition is complete
	bool IsComplete() const { return m_fElapsedTime >= m_fDuration; }

	// Get current blend weight (0 = from pose, 1 = target pose)
	float GetBlendWeight() const;

	// Optional: set easing function
	enum class EasingType
	{
		Linear,
		EaseInOut,
		EaseIn,
		EaseOut
	};
	void SetEasing(EasingType eType) { m_eEasing = eType; }

private:
	float ApplyEasing(float t) const;

	Flux_SkeletonPose m_xFromPose;  // Snapshot of pose when transition started
	float m_fDuration = 0.0f;
	float m_fElapsedTime = 0.0f;
	EasingType m_eEasing = EasingType::EaseInOut;
};
