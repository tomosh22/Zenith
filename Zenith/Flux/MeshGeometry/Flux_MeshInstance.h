#pragma once
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_FrustumCulling.h"   // Zenith_AABB (Phase 3 culling bounds)

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
 * The vertex format is the reflected static-mesh layout (24 bytes: half4 position,
 * half2 UV, snorm10 normal, snorm10 tangent whose w carries the bitangent SIGN, and
 * unorm8x4 colour) — declared once by Flux_DeclareMeshVertexLayout below rather than
 * spelled out here, because the shader's VsIn is what decides it.
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
	 * Creates the skin-INPUT vertex format, which appends bone indices and weights
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

	// Phase 3 (scene-graph culling): the mesh's LOCAL-space AABB, captured at creation
	// and cached. Asset-backed instances read the baked asset bounds (Zenith_MeshAsset::
	// GetBounds{Min,Max}); procedural instances compute it from the geometry's positions
	// (GenerateAABBFromVertices). Call InvalidateLocalBounds() if the procedural geometry
	// mutates (it is immutable for asset-backed instances).
	const Zenith_AABB& GetLocalBounds() const;
	void InvalidateLocalBounds() { m_bLocalBoundsValid = false; }

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

	// Phase 3: lazily-computed, cached local-space bounds (mutable so GetLocalBounds()
	// stays const). m_bLocalBoundsValid == false forces a recompute on next access.
	mutable Zenith_AABB m_xLocalBounds;
	mutable bool m_bLocalBoundsValid = false;
};

// Declare the MESH-FAMILY interleaved vertex layout into xLayoutOut (Reset first):
// the reflected static-mesh table every mesh drawn by the unified opaque pipeline
// (and the forward translucent pass) is fetched with, plus — when bSkinned — the two
// compute-skinning bone lanes the packed skin-input vertex appends.
//
// It exists because four places used to declare that layout by hand, element by
// element, each with its own `stride == 72` assert: they agreed with the shader by
// convention alone, and a storage-format change had to be applied to all four in the
// same edit or a buffer would be uploaded at the wrong size. The one caller-visible
// job the layout still has is SIZE (GetVertexDataSize / the upload), so it is derived
// from the same table the writers pack through rather than re-stated.
void Flux_DeclareMeshVertexLayout(Flux_BufferLayout& xLayoutOut, bool bSkinned);

// True when xLayout is — element-type-for-element-type, and stride — the reflected
// static-mesh pipeline table (the form Flux_DeclareMeshVertexLayout(.., false)
// installs). Two tripwires share it: CreateFromGeometry asserts a drawn geometry IS
// this table (the unified pipelines fetch whatever bytes are bound at the reflected
// stride, so a float32 or stale-layout stream decodes as garbage rather than fail),
// and Flux_MeshGeometry::Export asserts a serialized geometry is NOT (the .zmesh
// file format has no version field; its one stability guarantee is its element
// table, and the reflected table moves whenever a shader annotation does).
bool Flux_LayoutIsReflectedMeshPipelineTable(const Flux_BufferLayout& xLayout);

// Pack uNumVerts of a mesh asset's CPU attributes into the engine's standard
// STATIC vertex, shaped by
// Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout — the table
// reflected straight out of the shader that fetches it. Nothing here names an
// offset, a stride or a storage format: the generated table does, so a layout edit
// moves this writer and its reader together.
//
// The attribute -> semantic mapping is the asset's whole float vocabulary
// (positions POSITION, UVs TEXCOORD0, normals NORMAL, tangents TANGENT,
// bitangents BINORMAL, colours COLOR). An array shorter than uNumVerts is not
// offered to the packer at all, which routes that attribute onto the canonical
// default (uv 0, normal +Y, tangent +X, bitangent +Z, white colour) — the same
// rule, and the same values, the hand-written loops applied. The BITANGENT no
// longer has an element of its own: it feeds the 4-lane TANGENT's handedness sign,
// which is what every consumer rebuilds the vector from.
//
// pxPositionOverride, when non-null, replaces the asset's positions with a
// caller-owned tight float3 stream of at least uNumVerts entries: the bind-pose
// baked path pre-skins the positions and hands the RESULT over, so the packer
// stays a pure interleaver with no skeleton in its contract.
//
// pDst must hold at least uNumVerts vertices of the layout
// Flux_DeclareMeshVertexLayout(.., false) declares. Pure — no GPU, no engine
// singleton — so the whole path is headlessly unit-tested.
void Flux_PackStaticMeshVertices(
	void* pDst,
	const Zenith_MeshAsset& xAsset,
	uint32_t uNumVerts,
	const Zenith_Maths::Vector3* pxPositionOverride = nullptr);
