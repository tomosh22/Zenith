#pragma once
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <string>

#define ZENITH_MODEL_ASSET_VERSION 1
#define ZENITH_MODEL_EXT ".zmodel"

// Forward declarations
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Flux_MaterialAsset;

/**
 * Zenith_ModelAsset - Bundle asset combining meshes, materials, skeleton, and animations
 *
 * This is the top-level asset for 3D models. It references:
 * - One or more mesh assets (for LODs or multi-part models)
 * - Materials for each mesh/submesh
 * - Optional skeleton for animated models
 * - Optional animation clips
 *
 * This follows the Unity/Unreal pattern where a "Model" or "FBX import" produces
 * a bundle that can be instantiated multiple times in the scene.
 *
 * File path references are used (not GUIDs) for simplicity.
 */
class Zenith_ModelAsset
{
public:
	/**
	 * MeshMaterialBinding - Associates a mesh with its materials
	 */
	struct MeshMaterialBinding
	{
		std::string m_strMeshPath;
		Zenith_Vector<std::string> m_strMaterialPaths;  // One per submesh

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	Zenith_ModelAsset() = default;
	~Zenith_ModelAsset() = default;

	// Prevent accidental copies
	Zenith_ModelAsset(const Zenith_ModelAsset&) = delete;
	Zenith_ModelAsset& operator=(const Zenith_ModelAsset&) = delete;

	// Allow moves
	Zenith_ModelAsset(Zenith_ModelAsset&& xOther) noexcept;
	Zenith_ModelAsset& operator=(Zenith_ModelAsset&& xOther) noexcept;

	//--------------------------------------------------------------------------
	// Loading and Saving
	//--------------------------------------------------------------------------

	/**
	 * Load a model asset from file
	 * @param szPath Path to .zmodel file
	 * @return Loaded asset, or nullptr on failure
	 */
	static Zenith_ModelAsset* LoadFromFile(const char* szPath);

	/**
	 * Export this model to a file
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

	uint32_t GetNumMeshes() const { return static_cast<uint32_t>(m_xMeshBindings.GetSize()); }
	const MeshMaterialBinding& GetMeshBinding(uint32_t uIndex) const { return m_xMeshBindings.Get(uIndex); }

	bool HasSkeleton() const { return !m_strSkeletonPath.empty(); }
	const std::string& GetSkeletonPath() const { return m_strSkeletonPath; }

	uint32_t GetNumAnimations() const { return static_cast<uint32_t>(m_xAnimationPaths.GetSize()); }
	const std::string& GetAnimationPath(uint32_t uIndex) const { return m_xAnimationPaths.Get(uIndex); }

	const std::string& GetSourcePath() const { return m_strSourcePath; }
	const std::string& GetName() const { return m_strName; }

	//--------------------------------------------------------------------------
	// Model Building (for tools/import)
	//--------------------------------------------------------------------------

	/**
	 * Set the model name (usually derived from source filename)
	 */
	void SetName(const std::string& strName) { m_strName = strName; }

	/**
	 * Add a mesh with its materials
	 */
	void AddMesh(const std::string& strMeshPath, const Zenith_Vector<std::string>& xMaterialPaths);

	/**
	 * Set the skeleton path (for animated models)
	 */
	void SetSkeletonPath(const std::string& strPath) { m_strSkeletonPath = strPath; }

	/**
	 * Add an animation path
	 */
	void AddAnimationPath(const std::string& strPath) { m_xAnimationPaths.PushBack(strPath); }

	/**
	 * Clear all data
	 */
	void Reset();

	//--------------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------------

	std::string m_strName;
	Zenith_Vector<MeshMaterialBinding> m_xMeshBindings;
	std::string m_strSkeletonPath;
	Zenith_Vector<std::string> m_xAnimationPaths;
	std::string m_strSourcePath;
};
