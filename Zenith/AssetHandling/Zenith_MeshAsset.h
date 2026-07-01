#pragma once
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux_Buffers.h"

// .zmesh schema version now lives in AssetHandling/Zenith_AssetTypeIds.h
// (uZENITH_MESH_SCHEMA_CURRENT). Bumping it needs a legacy read branch keyed on
// the old value; the envelope's BAD_MAGIC rewind covers pre-envelope files.

// Forward declarations
class Zenith_SkeletonAsset;

/**
 * Zenith_MeshAsset - Geometry asset with CPU data and GPU buffers
 *
 * This class contains both CPU geometry data (for serialization/physics) and
 * GPU buffers (for rendering). It replaces the old split between Zenith_MeshAsset,
 * Flux_MeshGeometry, and Flux_MeshInstance.
 *
 * Usage:
 *   // Load from file
 *   Zenith_MeshAsset* pMesh = Zenith_AssetRegistry::Get<Zenith_MeshAsset>("Assets/mesh.zmesh");
 *
 *   // Get GPU buffers for rendering (uploads if needed)
 *   pMesh->EnsureGPUBuffers();
 *   const Flux_VertexBuffer& xVB = pMesh->GetVertexBuffer();
 */
class Zenith_MeshAsset : public Zenith_Asset
{
public:
	static constexpr uint32_t BONES_PER_VERTEX_LIMIT = 4;

	/**
	 * Submesh - A contiguous range of indices that share a material
	 */
	struct Submesh
	{
		uint32_t m_uStartIndex = 0;
		uint32_t m_uIndexCount = 0;
		uint32_t m_uMaterialIndex = 0;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	Zenith_MeshAsset() = default;
	~Zenith_MeshAsset();

	// Prevent accidental copies (use pointers for sharing)
	Zenith_MeshAsset(const Zenith_MeshAsset&) = delete;
	Zenith_MeshAsset& operator=(const Zenith_MeshAsset&) = delete;

	// Allow moves
	Zenith_MeshAsset(Zenith_MeshAsset&& xOther);
	Zenith_MeshAsset& operator=(Zenith_MeshAsset&& xOther);

	//--------------------------------------------------------------------------
	// Loading and Saving
	//--------------------------------------------------------------------------

	/**
	 * Export this mesh to a file
	 * @param szPath Output path
	 */
	void Export(const char* szPath) const;

	/**
	 * Serialization for scene save/load
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Envelope-aware, status-returning parse of an in-memory .zmesh stream — the
	// file-load error contract. The static LoadFromFile is ReadFromFile + ParseStream;
	// the void ReadFromDataStream above delegates here. Public for stream-only tests.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	uint32_t GetNumVerts() const { return m_uNumVerts; }
	uint32_t GetNumIndices() const { return m_uNumIndices; }
	uint32_t GetNumSubmeshes() const { return static_cast<uint32_t>(m_xSubmeshes.GetSize()); }
	bool HasSkinning() const { return !m_strSkeletonPath.empty() && m_xBoneIndices.GetSize() > 0; }

	const Zenith_Maths::Vector3& GetBoundsMin() const { return m_xBoundsMin; }
	const Zenith_Maths::Vector3& GetBoundsMax() const { return m_xBoundsMax; }
	Zenith_Maths::Vector3 GetBoundsCenter() const { return (m_xBoundsMin + m_xBoundsMax) * 0.5f; }
	Zenith_Maths::Vector3 GetBoundsExtents() const { return (m_xBoundsMax - m_xBoundsMin) * 0.5f; }

	const std::string& GetSourcePath() const { return m_strSourcePath; }
	const std::string& GetSkeletonPath() const { return m_strSkeletonPath; }

	//--------------------------------------------------------------------------
	// Mesh Building (for tools/procedural generation)
	//--------------------------------------------------------------------------

	/**
	 * Reserve capacity for vertex data
	 */
	void Reserve(uint32_t uNumVerts, uint32_t uNumIndices);

	/**
	 * Add a vertex with all attributes
	 */
	void AddVertex(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xNormal,
		const Zenith_Maths::Vector2& xUV,
		const Zenith_Maths::Vector3& xTangent = Zenith_Maths::Vector3(1, 0, 0),
		const Zenith_Maths::Vector4& xColor = Zenith_Maths::Vector4(1, 1, 1, 1)
	);

	/**
	 * Add a triangle (3 indices)
	 */
	void AddTriangle(uint32_t uA, uint32_t uB, uint32_t uC);

	/**
	 * Add a submesh definition
	 */
	void AddSubmesh(uint32_t uStartIndex, uint32_t uIndexCount, uint32_t uMaterialIndex);

	/**
	 * Set skinning data for a vertex
	 */
	void SetVertexSkinning(
		uint32_t uVertexIndex,
		const glm::uvec4& xBoneIndices,
		const glm::vec4& xBoneWeights
	);

	/**
	 * Set the skeleton path for skinned meshes
	 */
	void SetSkeletonPath(const std::string& strPath) { m_strSkeletonPath = Zenith_AssetRegistry::NormalizeAssetPath(strPath); }

	/**
	 * Compute bounding box from vertex positions
	 */
	void ComputeBounds();

	/**
	 * Generate normals from face topology (if not provided)
	 */
	void GenerateNormals();

	/**
	 * Generate tangents from UVs (if not provided)
	 */
	void GenerateTangents();

	/**
	 * Clear all data
	 */
	void Reset();

	//--------------------------------------------------------------------------
	// Static Mesh Generation Utilities
	//--------------------------------------------------------------------------

	/**
	 * Generate a fullscreen quad (for post-processing)
	 * Creates a 2-triangle quad from -1 to 1 in X/Y, with Z=0
	 * @param xMeshOut Output mesh asset (will be reset first)
	 */
	static void GenerateFullscreenQuad(Zenith_MeshAsset& xMeshOut);

	/**
	 * Generate a fullscreen quad with a transform applied
	 * @param xMeshOut Output mesh asset (will be reset first)
	 * @param xTransform Transform matrix to apply to positions
	 */
	static void GenerateFullscreenQuad(Zenith_MeshAsset& xMeshOut, const Zenith_Maths::Matrix4& xTransform);

	/**
	 * Generate a unit cube (from -0.5 to 0.5 on each axis)
	 * Creates a 24-vertex cube with proper normals per face
	 * @param xMeshOut Output mesh asset (will be reset first)
	 */
	static void GenerateUnitCube(Zenith_MeshAsset& xMeshOut);

	/**
	 * Generate a unit sphere (radius 0.5, centred at the origin; bounds exactly
	 * [-0.5, 0.5]). Matches the runtime Zenith_MeshGeometryAsset::CreateUnitSphere
	 * convention (latitude = uSegments, longitude = uSegments*2) so a sphere-collider
	 * sized from the same scale lines up with the mesh.
	 *
	 * uSegments is the latitude (stack) count and MUST be EVEN (asserted): an even
	 * count puts a ring exactly on the equator, so |x| and |z| reach exactly 0.5.
	 *
	 * Writes ANALYTIC attributes — normal = radial (unit), tangent = the longitude
	 * derivative {-sinφ, 0, cosφ}, bitangent = {cosθcosφ, -sinθ, cosθsinφ} — rather
	 * than GenerateNormals/GenerateTangents, which average across the duplicated
	 * pole/seam vertices and corrupt shading there.
	 *
	 * CPU-only: does NOT create GPU buffers (suits offline asset export). Call
	 * EnsureGPUBuffers() yourself if you need to render the result directly.
	 *
	 * @param xMeshOut Output mesh asset (will be reset first)
	 * @param uSegments Latitude segment count (even, >= 2)
	 */
	static void GenerateUnitSphere(Zenith_MeshAsset& xMeshOut, uint32_t uSegments = 16);

	//--------------------------------------------------------------------------
	// Vertex Data (public for direct access during export/building)
	//--------------------------------------------------------------------------

	// Core geometry
	Zenith_Vector<Zenith_Maths::Vector3> m_xPositions;
	Zenith_Vector<Zenith_Maths::Vector3> m_xNormals;
	Zenith_Vector<Zenith_Maths::Vector2> m_xUVs;
	Zenith_Vector<Zenith_Maths::Vector3> m_xTangents;
	Zenith_Vector<Zenith_Maths::Vector3> m_xBitangents;
	Zenith_Vector<Zenith_Maths::Vector4> m_xColors;

	// Index data
	Zenith_Vector<uint32_t> m_xIndices;

	// Submesh definitions
	Zenith_Vector<Submesh> m_xSubmeshes;

	// Skinning data (optional - only for animated meshes)
	std::string m_strSkeletonPath;
	Zenith_Vector<glm::uvec4> m_xBoneIndices;  // 4 bone indices per vertex
	Zenith_Vector<glm::vec4> m_xBoneWeights;   // 4 bone weights per vertex

	// Bounds
	Zenith_Maths::Vector3 m_xBoundsMin = Zenith_Maths::Vector3(0);
	Zenith_Maths::Vector3 m_xBoundsMax = Zenith_Maths::Vector3(0);

	// Source information
	std::string m_strSourcePath;

	// Material base color (from source file, if any)
	Zenith_Maths::Vector4 m_xMaterialColor = Zenith_Maths::Vector4(1, 1, 1, 1);

	//--------------------------------------------------------------------------
	// GPU Resources
	//--------------------------------------------------------------------------

	/**
	 * Ensure GPU buffers are created and uploaded
	 * Call this before rendering. Does nothing if already uploaded.
	 * @param bSkinned If true, creates skinned vertex format (104 bytes per vert)
	 */
	void EnsureGPUBuffers(bool bSkinned = false);

	/**
	 * Ensure GPU buffers with skeleton for bind pose transformation
	 * @param pxSkeleton Skeleton for bind pose (can be null)
	 */
	void EnsureGPUBuffers(Zenith_SkeletonAsset* pxSkeleton);

	/**
	 * Release GPU resources (call before destroying or to free VRAM)
	 */
	void ReleaseGPU();

	/**
	 * Check if GPU buffers are ready
	 */
	bool HasGPUBuffers() const { return m_bGPUBuffersReady; }

	// Uniform GPU-lifetime vocabulary (Zenith_Asset). Mesh is LAZY — Ensure builds the
	// (unskinned) GPU buffers on demand; Release frees them (also auto-run via the
	// destructor's Reset()). The skinned path still uses EnsureGPUBuffers(skeleton)
	// directly, since skinned-ness is only known at the use site.
	void EnsureGPUResources() override { EnsureGPUBuffers(false); }
	bool IsGPUReady() const override { return m_bGPUBuffersReady; }
	void ReleaseGPUResources() override { ReleaseGPU(); }

	/**
	 * Get vertex buffer for rendering
	 */
	const Flux_VertexBuffer& GetVertexBuffer() const { return m_xVertexBuffer; }

	/**
	 * Get index buffer for rendering
	 */
	const Flux_IndexBuffer& GetIndexBuffer() const { return m_xIndexBuffer; }

	/**
	 * Get buffer layout
	 */
	const Flux_BufferLayout& GetBufferLayout() const { return m_xBufferLayout; }

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend Zenith_Result<Zenith_Asset*> LoadAssetViaStaticFactory(const std::string&);

	/**
	 * Load a mesh asset from file (private - use Zenith_AssetRegistry::Get)
	 * @param szPath Path to .zmesh file
	 * @return Loaded asset, or an error code on failure
	 */
	static Zenith_Result<Zenith_MeshAsset*> LoadFromFile(const char* szPath);

	uint32_t m_uNumVerts = 0;
	uint32_t m_uNumIndices = 0;

	// GPU resources
	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
	Flux_BufferLayout m_xBufferLayout;
	bool m_bGPUBuffersReady = false;
	bool m_bIsSkinned = false;
};
