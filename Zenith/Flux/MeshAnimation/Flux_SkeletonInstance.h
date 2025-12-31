#pragma once
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

class Zenith_SkeletonAsset;

/**
 * Flux_SkeletonInstance - Runtime skeleton for animation playback
 *
 * This class represents a runtime instance of a skeleton asset. It manages:
 * - Current bone pose (position, rotation, scale per bone)
 * - Skinning matrix computation (model space * inverse bind pose)
 * - GPU buffer upload for shader access
 *
 * Created from a Zenith_SkeletonAsset which provides the bone hierarchy
 * and bind pose data. Multiple instances can share the same skeleton asset.
 */
class Flux_SkeletonInstance
{
public:
	static constexpr uint32_t MAX_BONES = 100;  // Must match shader's g_xBones[100] array size

	Flux_SkeletonInstance() = default;
	~Flux_SkeletonInstance();

	// Prevent accidental copies
	Flux_SkeletonInstance(const Flux_SkeletonInstance&) = delete;
	Flux_SkeletonInstance& operator=(const Flux_SkeletonInstance&) = delete;

	// Allow moves
	Flux_SkeletonInstance(Flux_SkeletonInstance&& xOther) noexcept;
	Flux_SkeletonInstance& operator=(Flux_SkeletonInstance&& xOther) noexcept;

	/**
	 * Factory method - create instance from skeleton asset
	 * @param pxAsset Source skeleton asset (must remain valid for lifetime of instance)
	 * @param bUploadToGPU If true, creates GPU buffer for skinning matrices. Set to false for CPU-only use (e.g., unit tests)
	 * @return New skeleton instance, or nullptr on failure
	 */
	static Flux_SkeletonInstance* CreateFromAsset(Zenith_SkeletonAsset* pxAsset, bool bUploadToGPU = true);

	/**
	 * Destroy GPU resources
	 * Called automatically by destructor, but can be called manually for early cleanup
	 */
	void Destroy();

	//=========================================================================
	// Pose Management
	//=========================================================================

	/**
	 * Reset pose to bind pose from skeleton asset
	 */
	void SetToBindPose();

	/**
	 * Set local transform for a specific bone
	 * @param uBoneIndex Index of bone to modify
	 * @param xPos Local position relative to parent
	 * @param xRot Local rotation relative to parent
	 * @param xScale Local scale
	 */
	void SetBoneLocalTransform(uint32_t uBoneIndex,
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Quat& xRot,
		const Zenith_Maths::Vector3& xScale);

	/**
	 * Get local position for a bone
	 */
	const Zenith_Maths::Vector3& GetBoneLocalPosition(uint32_t uBoneIndex) const;

	/**
	 * Get local rotation for a bone
	 */
	const Zenith_Maths::Quat& GetBoneLocalRotation(uint32_t uBoneIndex) const;

	/**
	 * Get local scale for a bone
	 */
	const Zenith_Maths::Vector3& GetBoneLocalScale(uint32_t uBoneIndex) const;

	//=========================================================================
	// Skinning Matrix Computation
	//=========================================================================

	/**
	 * Compute final skinning matrices from current pose
	 * Must be called after updating bone transforms and before UploadToGPU()
	 *
	 * For each bone: skinningMatrix = modelSpaceTransform * inverseBindPose
	 * where modelSpaceTransform is computed by walking up the parent chain
	 */
	void ComputeSkinningMatrices();

	/**
	 * Upload skinning matrices to GPU buffer for current frame
	 * Call after ComputeSkinningMatrices()
	 */
	void UploadToGPU();

	/**
	 * Upload skinning matrices to ALL frame buffers
	 * Used during initialization to ensure all triple-buffered copies have valid data
	 */
	void UploadToAllFrameBuffers();

	//=========================================================================
	// Accessors
	//=========================================================================

	/**
	 * Get source skeleton asset
	 */
	Zenith_SkeletonAsset* GetSourceSkeleton() const { return m_pxSourceSkeleton; }

	/**
	 * Get number of bones in skeleton
	 */
	uint32_t GetNumBones() const;

	/**
	 * Get GPU buffer containing bone matrices
	 * Used for binding to shaders during rendering
	 */
	const Flux_DynamicConstantBuffer& GetBoneBuffer() const { return m_xBoneBuffer; }
	Flux_DynamicConstantBuffer& GetBoneBuffer() { return m_xBoneBuffer; }

	/**
	 * Get pointer to skinning matrices array
	 * Can be used for CPU-side operations or debug visualization
	 */
	const Zenith_Maths::Matrix4* GetSkinningMatrices() const { return m_axSkinningMatrices; }

	/**
	 * Get model-space transform for a bone (computed during ComputeSkinningMatrices)
	 */
	const Zenith_Maths::Matrix4& GetBoneModelTransform(uint32_t uBoneIndex) const;

private:
	// Source skeleton asset (not owned)
	Zenith_SkeletonAsset* m_pxSourceSkeleton = nullptr;

	// Number of bones (cached from skeleton asset)
	uint32_t m_uNumBones = 0;

	// Current local pose (position, rotation, scale per bone)
	Zenith_Maths::Vector3 m_axLocalPositions[MAX_BONES];
	Zenith_Maths::Quat m_axLocalRotations[MAX_BONES];
	Zenith_Maths::Vector3 m_axLocalScales[MAX_BONES];

	// Cached model-space transforms (computed during ComputeSkinningMatrices)
	Zenith_Maths::Matrix4 m_axModelSpaceTransforms[MAX_BONES];

	// Final skinning matrices (model space * inverse bind pose)
	Zenith_Maths::Matrix4 m_axSkinningMatrices[MAX_BONES];

	// GPU buffer for bone matrices
	Flux_DynamicConstantBuffer m_xBoneBuffer;

	// Flag to track if GPU resources are initialized
	bool m_bGPUResourcesInitialized = false;

	/**
	 * Helper to compute model-space transform for a bone
	 * Walks up the parent chain to accumulate transforms
	 */
	Zenith_Maths::Matrix4 ComputeBoneModelTransform(uint32_t uBoneIndex) const;

	/**
	 * Convert local pose components to a transformation matrix
	 */
	static Zenith_Maths::Matrix4 ComposeTransformMatrix(
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Quat& xRot,
		const Zenith_Maths::Vector3& xScale);
};
