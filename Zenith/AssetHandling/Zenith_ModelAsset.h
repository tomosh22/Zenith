#pragma once
#include "AssetHandling/Zenith_Asset.h"
#include "Collections/Zenith_Vector.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include <string>

// .zmodel schema version now lives in AssetHandling/Zenith_AssetTypeIds.h
// (uZENITH_MODEL_SCHEMA_CURRENT). The envelope's BAD_MAGIC rewind covers
// pre-envelope files; a version mismatch is rejected (re-export required).

// Forward declarations
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_MaterialAsset;

/**
 * Zenith_ModelAsset - Bundle asset combining meshes, materials, skeleton, and animations
 *
 * This is the top-level asset for 3D models. It references:
 * - One or more mesh assets (for LODs or multi-part models)
 * - Materials for each mesh/submesh
 * - Optional skeleton for animated models
 * - Optional animation clips
 *
 * "Model" or "FBX import" produces
 * a bundle that can be instantiated multiple times in the scene.
 *
 * Usage:
 *   // Load an owning handle (GetView<T>() for a raw transient view)
 *   ModelHandle xModel = Zenith_AssetRegistry::Acquire<Zenith_ModelAsset>("Assets/model.zmodel");
 *
 */
class Zenith_ModelAsset : public Zenith_Asset
{
public:
	/**
	 * MeshMaterialBinding - Associates a mesh with its materials
	 */
	struct MeshMaterialBinding
	{
		MeshHandle m_xMesh;                          // Reference to mesh geometry
		Zenith_Vector<MaterialHandle> m_xMaterials;  // One per submesh

		// Path accessors
		std::string GetMeshPath() const { return m_xMesh.GetPath(); }
		std::string GetMaterialPath(uint32_t uIndex) const;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	Zenith_ModelAsset() = default;
	virtual ~Zenith_ModelAsset() = default;

	// Prevent accidental copies
	Zenith_ModelAsset(const Zenith_ModelAsset&) = delete;
	Zenith_ModelAsset& operator=(const Zenith_ModelAsset&) = delete;

	// Allow moves
	Zenith_ModelAsset(Zenith_ModelAsset&& xOther);
	Zenith_ModelAsset& operator=(Zenith_ModelAsset&& xOther);

	//--------------------------------------------------------------------------
	// Loading and Saving
	//--------------------------------------------------------------------------

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

	// Envelope-aware, status-returning parse of an in-memory .zmodel stream — the
	// file-load error contract. The static LoadFromFile is ReadFromFile + ParseStream;
	// the void ReadFromDataStream above delegates here. Public for stream-only tests.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

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
	void AddMesh(const MeshHandle& xMesh, const Zenith_Vector<MaterialHandle>& xMaterials);

	/**
	 * Add a mesh with its materials (path-based, for tools)
	 */
	void AddMeshByPath(const std::string& strMeshPath, const Zenith_Vector<std::string>& xMaterialPaths);

	/**
	 * Set the skeleton path (for animated models)
	 */
	void SetSkeletonPath(const std::string& strPath) { m_strSkeletonPath = Zenith_AssetRegistry::NormalizeAssetPath(strPath); }

	/**
	 * Add an animation path
	 */
	void AddAnimationPath(const std::string& strPath) { m_xAnimationPaths.PushBack(Zenith_AssetRegistry::NormalizeAssetPath(strPath)); }

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

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend Zenith_Result<Zenith_Asset*> LoadAssetViaStaticFactory(const std::string&);

	/**
	 * Load a model asset from file (private - use Zenith_AssetRegistry::Get)
	 * @param szPath Path to .zmodel file
	 * @return Loaded asset, or an error code on failure
	 */
	static Zenith_Result<Zenith_ModelAsset*> LoadFromFile(const char* szPath);
};
