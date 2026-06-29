#pragma once
#include "Collections/Zenith_Vector.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_FrustumCulling.h"   // Zenith_AABB (Phase 3 culling bounds)

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

	// Phase 3 (scene-graph culling): the model's LOCAL-space AABB = the union of its mesh
	// instances' local bounds, lazily computed + cached. InvalidateLocalBounds() dirties
	// the union (e.g. after AppendProceduralMesh grows the mesh set); the per-mesh bounds
	// are themselves immutable for asset-backed meshes. The entity WORLD AABB is then
	// TransformAABB(GetLocalBounds(), worldMatrix) — computed at snapshot-build time.
	const Zenith_AABB& GetLocalBounds() const;
	void InvalidateLocalBounds() { m_bLocalBoundsValid = false; }

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

	// True iff this is a skinned-animated model: it has a skeleton AND at least one
	// skinned mesh instance. The unified static-mesh walk skips these (they're drawn by
	// the unified compute-skinning path); a skeleton with NO skinned mesh still renders
	// as static. Single source for the static/animated split (the snapshot fill stamps the
	// advisory flag from this; the unified gather's static filter calls it too).
	bool IsAnimatedSkinned() const
	{
		if (!HasSkeleton()) return false;
		for (uint32_t u = 0; u < GetNumMeshes(); u++)
		{
			if (GetSkinnedMeshInstance(u) != nullptr) return true;
		}
		return false;
	}

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
	// Phase 3: lazily-computed, cached local-space union bounds (mutable so GetLocalBounds()
	// stays const). Invalidated by InvalidateLocalBounds() when the mesh set changes.
	mutable Zenith_AABB m_xLocalBounds;
	mutable bool m_bLocalBoundsValid = false;

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

// Phase 2 (scene graph): one entry of the engine-owned Flux_RenderSceneSnapshot (the
// uncullled master list, rebuilt once per frame — see Flux/SceneGraph/Flux_RenderSceneSnapshot.h).
// The item struct + fill seam live HERE, in the Flux header Zenith_ModelComponent.cpp already
// includes, so the EC-side fill fn (g_pfnZenithSceneSnapshotFill, defined in
// Zenith_ModelComponent.cpp) populates a Zenith_Vector<Flux_RenderSceneItem> WITHOUT EC ever
// including the snapshot header — i.e. no new EntityComponent->Flux edge. The render consumers
// dereference m_pxModelInstance + read m_xWorldMatrix (only after the authoritative rebuild);
// the remaining fields are pointer-free diagnostics the tools panel reads even when stale.
struct Flux_RenderSceneItem
{
	Flux_ModelInstance*    m_pxModelInstance = nullptr;
	Zenith_Maths::Matrix4  m_xWorldMatrix = Zenith_Maths::Matrix4(1.0f);

	// Phase 3: the entity's world-space AABB (TransformAABB of the model's local union by
	// the world matrix), computed at snapshot build. Camera-cull consumers test this; it is
	// also pointer-free so the diagnostics overlays read it directly.
	Zenith_AABB            m_xWorldAABB;

	// --- pointer-free diagnostics (safe to read when stale) ---
	// EntityID stored PACKED (uint64) rather than as Zenith_EntityID: the established
	// Flux<->ECS convention forward-declares Zenith_EntityID and never #includes
	// Zenith_Entity.h into a widely-included Flux header (cycle). A by-value member needs
	// the complete type, so the packed form keeps this struct a pure ECS-free POD; the
	// tools panel unpacks with Zenith_EntityID::FromPacked. 0 == none.
	uint64_t               m_ulEntityIDPacked = 0;
	uint32_t               m_uMeshCount = 0;
	bool                   m_bAnimatedSkinned = false;   // advisory/debug only; consumers still apply their own predicate
};

// EC -> Flux fill seam (replaces the former g_pfnZenithModelGather two-vector form). The EC
// side pushes one item per renderable into the snapshot's item vector; the snapshot's Rebuild
// drives it. The typedef names only Flux + ECS-leaf types (no EntityComponent type), and lives
// in this already-included header, so it adds no new EntityComponent->Flux edge. DEFINED in
// Zenith_ModelComponent.cpp.
using Zenith_SceneSnapshotFillFn = void (*)(Zenith_Vector<Flux_RenderSceneItem>& xOutItems);
extern Zenith_SceneSnapshotFillFn g_pfnZenithSceneSnapshotFill;
