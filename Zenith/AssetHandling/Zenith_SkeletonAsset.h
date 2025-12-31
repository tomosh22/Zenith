#pragma once
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <unordered_map>

#define ZENITH_SKELETON_ASSET_VERSION 2
#define ZENITH_SKELETON_EXT ".zskel"

/**
 * Zenith_SkeletonAsset - Bone hierarchy and bind pose data
 *
 * Represents a skeleton that can be shared between multiple meshes and animations.
 * Separating the skeleton from the mesh allows:
 * - Different meshes to use the same skeleton (e.g., LODs, cosmetic variants)
 * - Animation retargeting between compatible skeletons
 * - Clear separation of concerns between mesh geometry and bone hierarchy
 */
class Zenith_SkeletonAsset
{
public:
	static constexpr int32_t INVALID_BONE_INDEX = -1;
	static constexpr uint32_t MAX_BONES = 128;

	/**
	 * Bone - A single bone in the skeleton hierarchy
	 */
	struct Bone
	{
		std::string m_strName;
		int32_t m_iParentIndex = INVALID_BONE_INDEX;  // -1 for root bones

		// Bind pose: Local transform relative to parent (or world for roots)
		Zenith_Maths::Vector3 m_xBindPosition = Zenith_Maths::Vector3(0);
		Zenith_Maths::Quat m_xBindRotation = glm::identity<Zenith_Maths::Quat>();
		Zenith_Maths::Vector3 m_xBindScale = Zenith_Maths::Vector3(1);

		// Precomputed matrices
		Zenith_Maths::Matrix4 m_xBindPoseLocal = glm::identity<Zenith_Maths::Matrix4>();
		Zenith_Maths::Matrix4 m_xBindPoseModel = glm::identity<Zenith_Maths::Matrix4>();  // Local * parent chain
		Zenith_Maths::Matrix4 m_xInverseBindPose = glm::identity<Zenith_Maths::Matrix4>();

		// True if inverse bind pose was set from Assimp's mOffsetMatrix (should not be overwritten)
		bool m_bHasAssimpInverseBindPose = false;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion);
	};

	Zenith_SkeletonAsset() = default;
	~Zenith_SkeletonAsset() = default;

	// Prevent accidental copies
	Zenith_SkeletonAsset(const Zenith_SkeletonAsset&) = delete;
	Zenith_SkeletonAsset& operator=(const Zenith_SkeletonAsset&) = delete;

	// Allow moves
	Zenith_SkeletonAsset(Zenith_SkeletonAsset&& xOther) noexcept;
	Zenith_SkeletonAsset& operator=(Zenith_SkeletonAsset&& xOther) noexcept;

	//--------------------------------------------------------------------------
	// Loading and Saving
	//--------------------------------------------------------------------------

	/**
	 * Load a skeleton asset from file
	 * @param szPath Path to .zskel file
	 * @return Loaded asset, or nullptr on failure
	 */
	static Zenith_SkeletonAsset* LoadFromFile(const char* szPath);

	/**
	 * Export this skeleton to a file
	 * @param szPath Output path
	 */
	void Export(const char* szPath) const;

	/**
	 * Serialization for scene save/load
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	uint32_t GetNumBones() const { return static_cast<uint32_t>(m_xBones.GetSize()); }

	const Bone& GetBone(uint32_t uIndex) const { return m_xBones.Get(uIndex); }
	Bone& GetBone(uint32_t uIndex) { return m_xBones.Get(uIndex); }

	/**
	 * Get bone index by name
	 * @return Bone index, or INVALID_BONE_INDEX if not found
	 */
	int32_t GetBoneIndex(const std::string& strName) const;

	/**
	 * Check if a bone exists by name
	 */
	bool HasBone(const std::string& strName) const;

	const std::string& GetSourcePath() const { return m_strSourcePath; }

	/**
	 * Get root bone indices (bones with no parent)
	 */
	Zenith_Vector<uint32_t> GetRootBones() const;

	/**
	 * Get child bone indices for a given parent
	 */
	Zenith_Vector<uint32_t> GetChildBones(uint32_t uParentIndex) const;

	//--------------------------------------------------------------------------
	// Skeleton Building (for tools/import)
	//--------------------------------------------------------------------------

	/**
	 * Add a bone to the skeleton
	 * @return Index of the added bone
	 */
	uint32_t AddBone(
		const std::string& strName,
		int32_t iParentIndex,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xScale
	);

	/**
	 * Set the inverse bind pose matrix directly (for importing from Assimp)
	 */
	void SetInverseBindPose(uint32_t uBoneIndex, const Zenith_Maths::Matrix4& xInvBindPose);

	/**
	 * Compute all bind pose matrices from local transforms
	 * Call this after adding all bones and setting parent relationships
	 */
	void ComputeBindPoseMatrices();

	/**
	 * Clear all bones
	 */
	void Reset();

	//--------------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------------

	Zenith_Vector<Bone> m_xBones;
	std::unordered_map<std::string, uint32_t> m_xBoneNameToIndex;
	std::string m_strSourcePath;

private:
	/**
	 * Recursively compute model-space bind pose for a bone and its children
	 */
	void ComputeBindPoseRecursive(uint32_t uBoneIndex, const Zenith_Maths::Matrix4& xParentModelPose);
};
