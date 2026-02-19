#pragma once

// Unified terrain configuration - single source of truth for all terrain constants
#include "Flux/Terrain/Flux_TerrainConfig.h"
using namespace Flux_TerrainConfig;

#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_FrustumCulling.h"

// Forward declarations
class Flux_CommandList;

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

// LOD data for a single level
struct Zenith_TerrainLODData
{
	uint32_t m_uFirstIndex;    // Starting index in the index buffer for this LOD
	uint32_t m_uIndexCount;    // Number of indices to draw for this LOD
	uint32_t m_uVertexOffset;  // Base vertex offset (always 0 for combined mesh)
	float m_fMaxDistance;      // Maximum distance (squared) at which this LOD is used
};

// Chunk data structure that gets uploaded to GPU
// Must match the GLSL struct in Flux_TerrainCulling.comp
struct Zenith_TerrainChunkData
{
	Zenith_Maths::Vector4 m_xAABBMin;                   // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;                   // xyz = max corner, w = padding
	Zenith_TerrainLODData m_axLODs[LOD_COUNT];          // LOD mesh data (HIGH=0, LOW=1)
};

// Frustum plane structure for GPU upload
struct Zenith_FrustumPlaneGPU
{
	Zenith_Maths::Vector4 m_xNormalAndDistance;  // xyz = normal, w = distance
};

// Camera culling data structure for GPU upload
struct Zenith_CameraDataGPU
{
	Zenith_FrustumPlaneGPU m_axFrustumPlanes[6];  // 6 frustum planes
	Zenith_Maths::Vector4 m_xCameraPosition;      // xyz = camera position, w = padding
};

class Zenith_TerrainComponent
{
public:
	// Default constructor for deserialization
	// ReadFromDataStream will populate all members from saved data
	Zenith_TerrainComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
		, m_pxPhysicsGeometry(nullptr)
		, m_bCullingResourcesInitialized(false)
	{
		IncrementInstanceCount();
	};

	// Full constructor for runtime creation
	Zenith_TerrainComponent(Zenith_MaterialAsset& xMaterial0, Zenith_MaterialAsset& xMaterial1, Zenith_Entity& xEntity);

	~Zenith_TerrainComponent();

private:
	static uint32_t s_uInstanceCount;
	static void IncrementInstanceCount();
	static void DecrementInstanceCount();

public:
	const Flux_VertexBuffer& GetUnifiedVertexBuffer() const { return m_xUnifiedVertexBuffer; }
	const Flux_IndexBuffer& GetUnifiedIndexBuffer() const { return m_xUnifiedIndexBuffer; }

	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const { return *m_pxPhysicsGeometry; }
	// Material accessors (4-material palette)
	static constexpr u_int TERRAIN_MATERIAL_COUNT = 4;
	Zenith_MaterialAsset* GetMaterial(u_int uIndex) const { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex].Get(); }
	MaterialHandle& GetMaterialHandle(u_int uIndex) { Zenith_Assert(uIndex < TERRAIN_MATERIAL_COUNT, "Invalid material index"); return m_axMaterials[uIndex]; }

	// Splatmap texture (RGBA8, weights for 4 materials)
	Zenith_TextureAsset* GetSplatmapTexture() const { return m_xSplatmap.Get(); }
	TextureHandle& GetSplatmapHandle() { return m_xSplatmap; }

	// Backward compatibility wrappers
	Zenith_MaterialAsset* GetMaterial0() const { return m_axMaterials[0].Get(); }
	Zenith_MaterialAsset* GetMaterial1() const { return m_axMaterials[1].Get(); }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// ========== GPU-Driven Culling API ==========

	/**
	 * Initialize GPU culling resources for this terrain component
	 * Allocates GPU buffers and builds chunk AABB + LOD metadata
	 * Called automatically by the full constructor after mesh loading
	 */
	void InitializeCullingResources();

	/**
	 * Destroy GPU culling resources
	 * Called automatically by destructor
	 */
	void DestroyCullingResources();

	/**
	 * Update culling and LOD selection for this terrain component
	 * Records a compute dispatch to the provided command list
	 *
	 * IMPORTANT: Assumes the terrain culling compute pipeline is already bound by Flux_Terrain
	 * before calling this method. This method only records dispatch commands and buffer bindings.
	 *
	 * @param xCmdList Command list to record dispatch commands into
	 * @param xViewProjMatrix Camera view-projection matrix for frustum extraction
	 */
	void UpdateCullingAndLod(Flux_CommandList& xCmdList, const Zenith_Maths::Matrix4& xViewProjMatrix);

	/**
	 * Get the indirect draw buffer for rendering
	 * Contains VkDrawIndexedIndirectCommand structs written by the compute shader
	 */
	const Flux_IndirectBuffer& GetIndirectDrawBuffer() const { return m_xIndirectDrawBuffer; }

	/**
	 * Get the visible chunk count buffer (for indirect draw count)
	 */
	const Flux_IndirectBuffer& GetVisibleCountBuffer() const { return m_xVisibleCountBuffer; }

	/**
	 * Get the maximum number of draw commands (= total chunks)
	 */
	uint32_t GetMaxDrawCount() const { return TOTAL_CHUNKS; }

	/**
	 * Get the LOD level buffer for visualization
	 */
	Flux_ReadWriteBuffer& GetLODLevelBuffer() { return m_xLODLevelBuffer; }

	/**
	 * Update chunk LOD allocations in GPU buffer based on current streaming manager state
	 * Called each frame after streaming manager updates
	 */
	void UpdateChunkLODAllocations();

//private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	MaterialHandle m_axMaterials[4];
	TextureHandle m_xSplatmap;

	// ========== Unified Terrain Buffers (owned by this component) ==========
	// Contains LOW LOD (always-resident) data at the beginning, followed by streaming space for HIGH LOD
	// These buffers are registered with Flux_TerrainStreamingManager for LOD streaming
	Flux_VertexBuffer m_xUnifiedVertexBuffer;
	Flux_IndexBuffer m_xUnifiedIndexBuffer;

	// Buffer sizes and layout information
	uint64_t m_ulUnifiedVertexBufferSize = 0;
	uint64_t m_ulUnifiedIndexBufferSize = 0;
	uint32_t m_uVertexStride = 0;
	uint32_t m_uLowLODVertexCount = 0;   // Vertices reserved for LOW LOD at buffer start
	uint32_t m_uLowLODIndexCount = 0;    // Indices reserved for LOW LOD at buffer start

	// ========== GPU-Driven Culling State ==========
	bool m_bCullingResourcesInitialized = false;

	// GPU buffers for culling
	Flux_ReadWriteBuffer m_xChunkDataBuffer;        // Chunk AABB + LOD metadata (read-only in compute)
	Flux_IndirectBuffer m_xIndirectDrawBuffer;      // Indirect draw commands (written by compute)
	Flux_DynamicConstantBuffer m_xFrustumPlanesBuffer; // Camera frustum + position (read-only in compute)
	Flux_IndirectBuffer m_xVisibleCountBuffer;      // Atomic counter for visible chunks
	Flux_ReadWriteBuffer m_xLODLevelBuffer;         // LOD level for each draw call (visualization)

	// Helper methods for culling
	void BuildChunkData();
	void ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Zenith_FrustumPlaneGPU* pxOutPlanes);

	// Helper method to initialize render resources (called by constructor and deserialization)
	void InitializeRenderResources();

	// Helper method to load and combine all physics chunks
	void LoadCombinedPhysicsGeometry();

#ifdef ZENITH_TOOLS
	// Editor UI
	void RenderPropertiesPanel();
#endif

};
