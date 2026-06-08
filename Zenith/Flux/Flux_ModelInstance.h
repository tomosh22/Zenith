#pragma once
#include "Collections/Zenith_Vector.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Zenith_ModelAsset;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_MaterialAsset;
class Flux_MeshInstance;
class Flux_MeshGeometry;
class Flux_SkeletonInstance;

/**
 * Flux_ModelInstance - A complete renderable model combining meshes, materials, and optional skeleton
 *
 * This is the top-level runtime instance created from a Zenith_ModelAsset.
 * It aggregates:
 * - One or more mesh instances (GPU-ready mesh data)
 * - Materials for each mesh
 * - Optional skeleton instance for animated models
 *
 * Asset (data definition) and Instance (runtime state)
 * Assets are shared data and instances are per-entity state.
 *
 * Usage:
 *   // Create from asset
 *   Zenith_ModelAsset* pxAsset = Zenith_ModelAsset::LoadFromFile("Models/Character.zmodel");
 *   Flux_ModelInstance* pxInstance = Flux_ModelInstance::CreateFromAsset(pxAsset);
 *
 *   // Access for rendering
 *   for (uint32_t u = 0; u < pxInstance->GetNumMeshes(); u++)
 *   {
 *       Flux_MeshInstance* pxMesh = pxInstance->GetMeshInstance(u);
 *       Zenith_MaterialAsset* pxMat = pxInstance->GetMaterial(u);
 *       // Submit draw call...
 *   }
 *
 *   // For animated models
 *   if (pxInstance->HasSkeleton())
 *   {
 *       pxInstance->UpdateAnimation();
 *   }
 *
 *   // Cleanup
 *   pxInstance->Destroy();
 *   delete pxInstance;
 */
class Flux_ModelInstance
{
public:
	Flux_ModelInstance() = default;
	~Flux_ModelInstance();

	// Prevent copying (instances own resources)
	Flux_ModelInstance(const Flux_ModelInstance&) = delete;
	Flux_ModelInstance& operator=(const Flux_ModelInstance&) = delete;

	//--------------------------------------------------------------------------
	// Factory Methods
	//--------------------------------------------------------------------------

	/**
	 * Create a model instance from a model asset
	 * Loads all referenced meshes, materials, and skeleton from disk
	 * @param pxAsset The model asset to instantiate
	 * @return New model instance, or nullptr on failure
	 */
	static Flux_ModelInstance* CreateFromAsset(Zenith_ModelAsset* pxAsset);

	/**
	 * Create a procedural model instance that wraps raw Flux_MeshGeometry.
	 * Used by code paths that build meshes at runtime (e.g. generated cubes,
	 * sprites, per-game procedural content) without going through a .zmodel asset.
	 * The geometry must outlive this instance (the instance does not own it).
	 * @param xGeometry Procedural geometry (must have GPU buffers initialized)
	 * @param xMaterial Material to use for the single sub-mesh
	 * @return New model instance, or nullptr on failure
	 */
	static Flux_ModelInstance* CreateProcedural(Flux_MeshGeometry& xGeometry, Zenith_MaterialAsset& xMaterial);

	/**
	 * Append another procedural sub-mesh to this instance.
	 * Produces a multi-sub-mesh model built from raw geometry.
	 * @param xGeometry Procedural geometry (must have GPU buffers initialized)
	 * @param xMaterial Material to use for this sub-mesh
	 */
	void AppendProceduralMesh(Flux_MeshGeometry& xGeometry, Zenith_MaterialAsset& xMaterial);

	//--------------------------------------------------------------------------
	// Lifecycle
	//--------------------------------------------------------------------------

	/**
	 * Destroy all owned resources
	 * Must be called before deleting the instance to properly clean up GPU resources
	 */
	void Destroy();

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	/**
	 * Get the source asset this instance was created from
	 */
	Zenith_ModelAsset* GetSourceAsset() const { return m_xSourceAsset.GetDirect(); }

	/**
	 * Get the number of meshes in this model
	 */
	uint32_t GetNumMeshes() const { return static_cast<uint32_t>(m_xMeshInstances.GetSize()); }

	/**
	 * Get mesh instance at the specified index (static 72-byte format)
	 * These are used for static mesh rendering
	 * @param uIndex Mesh index (0 to GetNumMeshes()-1)
	 * @return Mesh instance, or nullptr if index out of range
	 */
	Flux_MeshInstance* GetMeshInstance(uint32_t uIndex) const;

	/**
	 * Get skinned mesh instance at the specified index (104-byte format with bone data)
	 * These are used for animated mesh rendering with GPU skinning
	 * @param uIndex Mesh index (0 to GetNumMeshes()-1)
	 * @return Skinned mesh instance, or nullptr if index out of range or model has no skeleton
	 */
	Flux_MeshInstance* GetSkinnedMeshInstance(uint32_t uIndex) const;

	/**
	 * Get the number of materials in this model
	 */
	uint32_t GetNumMaterials() const { return static_cast<uint32_t>(m_xMaterials.GetSize()); }

	/**
	 * Get material at the specified index
	 * @param uIndex Material index (0 to GetNumMaterials()-1)
	 * @return Material asset, or nullptr if index out of range
	 */
	Zenith_MaterialAsset* GetMaterial(uint32_t uIndex) const;

	/**
	 * Override material at the specified index
	 * Useful for runtime material changes like procedural/colored materials
	 * @param uIndex Material index (0 to GetNumMaterials()-1)
	 * @param pxMaterial Material to set (not owned by model instance)
	 */
	void SetMaterial(uint32_t uIndex, Zenith_MaterialAsset* pxMaterial);

	/**
	 * Check if this model has a skeleton (is animated)
	 */
	bool HasSkeleton() const { return m_pxSkeleton != nullptr; }

	/**
	 * Get the skeleton instance
	 * Returns nullptr if model has no skeleton
	 */
	Flux_SkeletonInstance* GetSkeletonInstance() const { return m_pxSkeleton; }

	//--------------------------------------------------------------------------
	// Animation
	//--------------------------------------------------------------------------

	/**
	 * Update animation for this model
	 * For animated models - computes skinning matrices and uploads to GPU
	 */
	void UpdateAnimation();

private:
	// Source asset handle — keeps the asset alive for the lifetime of this instance.
	ModelHandle m_xSourceAsset;

	// Runtime mesh instances (GPU-ready, owned by this instance)
	// Static 72-byte format for static rendering or bind-pose rendering
	Zenith_Vector<Flux_MeshInstance*> m_xMeshInstances;

	// Skinned mesh instances (104-byte format with bone indices/weights)
	// Used for animated rendering with GPU skinning - only populated if model has skeleton
	Zenith_Vector<Flux_MeshInstance*> m_xSkinnedMeshInstances;

	// Materials for each mesh (handles manage ref counting automatically)
	Zenith_Vector<MaterialHandle> m_xMaterials;

	// Skeleton instance (owned by this instance - runtime state, not a registry asset)
	Flux_SkeletonInstance* m_pxSkeleton = nullptr;

	// Track mesh assets that were loaded (handles manage ref counting)
	Zenith_Vector<MeshHandle> m_xLoadedMeshAssets;

	// Track skeleton asset that was loaded (handle manages ref counting)
	SkeletonHandle m_xLoadedSkeletonAsset;

	// Called once per sub-mesh from CreateFromAsset — loads the mesh, creates static
	// and (if applicable) skinned GPU instances, and resolves its material bindings.
	void BuildSubMeshInstance(uint32_t uMeshIdx, Zenith_ModelAsset* pxAsset);
};

// Wave 3: model render-gather. The EC side queries Zenith_ModelComponent + the entity
// transform and produces a flat list of (model instance, world matrix) pairs so the
// renderers (Flux_StaticMeshes / Flux_AnimatedMeshes) no longer #include the components.
// The fn-ptr type lives here (not in Core/Zenith_RenderGather.h) because it names
// Flux_ModelInstance, a Flux type; Zenith_ModelComponent.cpp already includes this header
// (an existing forward EC->Flux edge), so hosting it here adds no new Flux<->EC edge.
// Both renderers consume the full list and filter (static keeps non-skinned; animated
// keeps skinned) exactly as before.
using Zenith_ModelGatherFn = void (*)(Zenith_Vector<Flux_ModelInstance*>& xOutInstances,
	Zenith_Vector<Zenith_Maths::Matrix4>& xOutMatrices);
extern Zenith_ModelGatherFn g_pfnZenithModelGather;
