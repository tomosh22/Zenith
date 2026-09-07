#pragma once
#include "Maths/Zenith_Maths.h"
#include "Flux_AnimationClip.h"
#include "Collections/Zenith_Vector.h"
#include <string>

// Forward declarations
class Flux_MeshGeometry;

#include "AssetHandling/Zenith_SkeletonAsset.h"

//=============================================================================
// Constants
//=============================================================================
static constexpr uint32_t FLUX_MAX_BONES = Zenith_SkeletonAsset::MAX_BONES;

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

	// Initialize local poses from skeleton's bind pose
	// Call this before SampleFromClip to ensure non-animated bones have correct bind pose values
	void InitFromBindPose(const Zenith_SkeletonAsset& xSkeleton);

	// Alternative: compute model space from a flat bone hierarchy (for clips without node tree)
	void ComputeModelSpaceMatricesFlat(const Flux_MeshGeometry& xGeometry);

	// Hierarchy-aware model-space matrix computation using the skeleton asset's parent chain.
	// Walks bones in storage order (parents always precede children — same invariant
	// Flux_SkeletonInstance::ComputeSkinningMatrices relies on). Used by IK solving where
	// reading model-space bone positions during the solve requires the local poses to be
	// composed with their ancestors.
	void ComputeModelSpaceMatricesFromSkeleton(const Zenith_SkeletonAsset& xSkeleton);

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
		const Zenith_Vector<float>& xBoneMask);

	// Copy pose data
	void CopyFrom(const Flux_SkeletonPose& xOther);

private:
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
//
// ★ IT IS A RESOLVED, INDEX-BASED WEIGHT ARRAY AND CARRIES NO PROVENANCE. It
// does not know which skeleton produced the indices, let alone which file — so
// it is never the thing that gets authored or persisted. `.zanimmask`
// (Zenith_BoneMaskAsset) holds the weights BY BONE NAME and resolves them onto
// one specific rig; this is the resolution's OUTPUT.
//
// ★ THE CONSTRUCTOR FILLS FLUX_MAX_BONES ZEROES, BUT ReadFromDataStream SIZES
// TO WHATEVER COUNT THE STREAM CARRIED — which may be FEWER. Every accessor
// below is therefore bounds-checked against the STORED count rather than
// against FLUX_MAX_BONES, and a query past it answers 0 (which is also the
// answer Flux_SkeletonPose::MaskedBlend already substitutes for a short mask).
// Reading past the end would be reading another object's bytes for a bone the
// mask simply does not describe.
//=============================================================================
class Flux_BoneMask
{
public:
	Flux_BoneMask();

	// Initialize from bone names (weight 1.0 for named bones, 0.0 for others)
	void SetFromBoneNames(const Zenith_Vector<std::string>& xBoneNames,
		const Flux_MeshGeometry& xGeometry);

	//=========================================================================
	// WU-7.1 — the SKELETON-ASSET overload.
	//
	// ★ THE WHOLE RUNTIME RESOLVES AGAINST A Zenith_SkeletonAsset AND THIS CLASS
	// DID NOT. Flux_SkeletonPose::SampleFromClip (the overload the controller,
	// the layers and every state machine reach), Flux_SkeletonInstance and
	// Zenith_BoneMaskAsset::ResolveTo all key on
	// Zenith_SkeletonAsset::m_xBoneNameToIndex, while the overload above keys on
	// Flux_MeshGeometry::m_xBoneNameToIdAndOffset — a DIFFERENT map, filled by a
	// different importer path, and one the editor never has in hand at all. A
	// mask authored against the geometry's numbering and applied to a pose posed
	// against the skeleton's is a mask on the wrong bones, with nothing to
	// observe but an animation that looks wrong.
	//
	// ★ NAMES ARE THE ONLY THING THAT TRAVELS BETWEEN TWO RIGS. Two skeletons
	// carrying the same bone NAMES in a different ORDER resolve the same mask to
	// different INDICES and to the same per-bone meaning, which is precisely why
	// the asset stores names.
	//
	// xWeights is either the SAME LENGTH as xBoneNames — one weight per name — or
	// EMPTY, which means "weight 1.0 for every named bone" and matches the
	// mesh-geometry overload's behaviour. A length mismatch is refused whole
	// (assert + false, the mask left fully zeroed) rather than resolved for the
	// prefix the two agree on: a weight silently attached to the wrong bone is
	// worse than no mask.
	//
	// Every weight not named is left at 0, so the result is a complete mask
	// whatever subset was passed. Returns TRUE only when EVERY name resolved; a
	// name the rig does not carry is appended to pxOutUnresolvedNames (when one
	// is supplied) and its weight is DROPPED. The reporting is a list rather than
	// a log line because this layer has no idea which asset the names came from —
	// Zenith_BoneMaskAsset::ResolveTo does, and is what names them.
	//=========================================================================
	bool SetFromBoneNames(const Zenith_SkeletonAsset& xSkeleton,
		const Zenith_Vector<std::string>& xBoneNames,
		const Zenith_Vector<float>& xWeights,
		Zenith_Vector<std::string>* pxOutUnresolvedNames = nullptr);

	// Set weight for specific bone
	void SetBoneWeight(uint32_t uBoneIndex, float fWeight);

	// Get weight for specific bone. 0.0f for an index past the STORED count —
	// see the class note; never an out-of-range read.
	float GetBoneWeight(uint32_t uBoneIndex) const;

	// How many weights this mask actually stores. FLUX_MAX_BONES for a freshly
	// constructed or freshly resolved one; whatever a stream carried after a
	// ReadFromDataStream.
	u_int GetWeightCount() const { return m_xWeights.GetSize(); }

	// Does any bone carry a non-zero weight?
	//
	// ★ THIS IS THE DERIVATION D47 IS ABOUT, NAMED. Flux_AnimationLayer::
	// ReadFromDataStream hand-inlines exactly this scan to decide "this layer has
	// an avatar mask" — so an ALL-ZERO MASK, which is a perfectly meaningful one
	// ("this layer overrides nothing yet"), comes back from a scene as NO MASK,
	// and an OVERRIDE layer with no mask replaces the WHOLE skeleton. Losing a
	// mask makes a layer do MORE, not less.
	//
	// It stays a derivation there because that stream payload reaches committed
	// .zscen bytes and may not gain a field. The `.zanimmask` asset carries the
	// flag EXPLICITLY instead, and Flux_AnimationController::BuildFromControllerDef
	// gates SetAvatarMask on the asset's flag rather than on this predicate. This
	// accessor exists so the scan is nameable and testable rather than only ever
	// appearing as an anonymous loop.
	bool HasAnyNonZeroWeight() const;

	// Get all weights for use in MaskedBlend
	const Zenith_Vector<float>& GetWeights() const { return m_xWeights; }

	// Common masks
	static Flux_BoneMask CreateUpperBodyMask(const Flux_MeshGeometry& xGeometry,
		const std::string& strSpineBoneName);
	static Flux_BoneMask CreateLowerBodyMask(const Flux_MeshGeometry& xGeometry,
		const std::string& strSpineBoneName);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_Vector<float> m_xWeights;
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
