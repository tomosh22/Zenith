#pragma once
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Types.h"
#include "AssetHandling/Zenith_AssetHandle.h"

class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Flux_MeshGeometry;

/**
 * Flux_MeshInstance - GPU-aware runtime representation of a mesh
 *
 * This is the "instance" representation that holds GPU buffers created from a
 * Zenith_MeshAsset. Multiple instances can share the same asset data but each
 * instance owns its own GPU resources.
 *
 * The vertex format matches the static mesh vertex stride (72 bytes):
 * - Position (12 bytes) - Vector3
 * - UV (8 bytes) - Vector2
 * - Normal (12 bytes) - Vector3
 * - Tangent (12 bytes) - Vector3
 * - Bitangent (12 bytes) - Vector3
 * - Color (16 bytes) - Vector4
 */
class Flux_MeshInstance
{
public:
	Flux_MeshInstance() = default;
	~Flux_MeshInstance();

	// Prevent accidental copies
	Flux_MeshInstance(const Flux_MeshInstance&) = delete;
	Flux_MeshInstance& operator=(const Flux_MeshInstance&) = delete;

	// Allow moves
	Flux_MeshInstance(Flux_MeshInstance&& xOther);
	Flux_MeshInstance& operator=(Flux_MeshInstance&& xOther);

	/**
	 * Factory method to create from asset
	 * Creates GPU vertex and index buffers from the asset's CPU data
	 * @param pxAsset The source mesh asset (must not be null)
	 * @return Newly created instance, or nullptr on failure
	 */
	static Flux_MeshInstance* CreateFromAsset(Zenith_MeshAsset* pxAsset);

	/**
	 * Factory method to create from skinned asset with bind pose applied
	 * For skinned meshes, applies the skeleton's bind pose transforms to position
	 * vertices correctly for static rendering
	 * @param pxAsset The source mesh asset (must not be null)
	 * @param pxSkeleton Optional skeleton for skinned meshes (applies bind pose if provided)
	 * @return Newly created instance, or nullptr on failure
	 */
	static Flux_MeshInstance* CreateFromAsset(Zenith_MeshAsset* pxAsset, Zenith_SkeletonAsset* pxSkeleton);

	/**
	 * Factory method to create skinned mesh instance for GPU animation
	 * Creates a 104-byte vertex format that includes bone indices and weights
	 * for real-time GPU skinning in the shader
	 * @param pxAsset The source mesh asset with skinning data (must not be null)
	 * @return Newly created instance, or nullptr on failure
	 */
	static Flux_MeshInstance* CreateSkinnedFromAsset(Zenith_MeshAsset* pxAsset);

	/**
	 * Factory method to create from a procedural Flux_MeshGeometry
	 * The instance holds a non-owning back-reference to the geometry and
	 * proxies its GPU buffers/layout from there. The geometry must outlive
	 * the instance; buffers must already be GPU-uploaded (LoadFromFile's
	 * default, or GenerateUnitCube/FullscreenQuad).
	 * @param pxGeometry Source geometry (must not be null)
	 * @return Newly created instance, or nullptr on failure
	 */
	static Flux_MeshInstance* CreateFromGeometry(Flux_MeshGeometry* pxGeometry);

	/**
	 * Destroy GPU resources
	 * Call this before deleting the instance if you need explicit cleanup timing
	 */
	void Destroy();

	// Accessors
	uint32_t GetNumVerts() const;
	uint32_t GetNumIndices() const;
	const Flux_BufferLayout& GetBufferLayout() const;
	const Flux_VertexBuffer& GetVertexBuffer() const;
	const Flux_IndexBuffer& GetIndexBuffer() const;
	Zenith_MeshAsset* GetSourceAsset() const { return m_xSourceAsset.GetDirect(); }

	/**
	 * Get the procedural geometry back-reference (nullptr if asset-backed)
	 * Used by physics and other systems that need raw CPU geometry data.
	 */
	const Flux_MeshGeometry* GetProceduralGeometry() const { return m_pxProceduralGeometry; }

	/**
	 * Check if this mesh has skinning/bone data
	 */
	bool HasSkinning() const;

private:
	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
	Flux_BufferLayout m_xBufferLayout;

	uint32_t m_uNumVerts = 0;
	uint32_t m_uNumIndices = 0;

	// Source asset handle — keeps the asset alive for the lifetime of this instance.
	// Was a raw pointer with "not owned" comments before; the handle now makes the
	// dependency explicit so UnloadUnused can't free the source while it's in use.
	MeshHandle m_xSourceAsset;
	// Non-owning back-reference for procedural instances (see CreateFromGeometry).
	// When non-null, buffer/layout accessors proxy to the geometry and Destroy()
	// skips buffer teardown (the geometry owns them).
	Flux_MeshGeometry* m_pxProceduralGeometry = nullptr;
	bool m_bInitialized = false;
};
