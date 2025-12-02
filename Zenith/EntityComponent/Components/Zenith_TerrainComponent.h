#pragma once

#define TERRAIN_EXPORT_DIMS 64

//#TO_TODO: these need to be in a header file for tools terrain export

#define MAX_TERRAIN_HEIGHT 2048
//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8

#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
#include "Maths/Zenith_FrustumCulling.h"

// Forward declarations
class Flux_CommandList;

// Number of LOD levels per terrain chunk
#define TERRAIN_LOD_COUNT 4

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
	Zenith_Maths::Vector4 m_xAABBMin;                      // xyz = min corner, w = padding
	Zenith_Maths::Vector4 m_xAABBMax;                      // xyz = max corner, w = padding
	Zenith_TerrainLODData m_axLODs[TERRAIN_LOD_COUNT];     // LOD mesh data (LOD0=highest detail)
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
		, m_pxMaterial0(nullptr)
		, m_pxMaterial1(nullptr)
		, m_bCullingResourcesInitialized(false)
	{
	};

	// Full constructor for runtime creation
	Zenith_TerrainComponent(Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity);

	~Zenith_TerrainComponent();

	const Flux_MeshGeometry& GetRenderMeshGeometry() const { return m_xRenderGeometryFacade; }
	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const { return *m_pxPhysicsGeometry; }
	const Flux_Material& GetMaterial0() const { return *m_pxMaterial0; }
	Flux_Material& GetMaterial0() { return *m_pxMaterial0; }
	const Flux_Material& GetMaterial1() const { return *m_pxMaterial1; }
	Flux_Material& GetMaterial1() { return *m_pxMaterial1; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	const bool IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const;

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
	 * Get the maximum number of draw commands (= 4096, the theoretical maximum)
	 */
	uint32_t GetMaxDrawCount() const { return TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS; }

	/**
	 * Get the LOD level buffer for visualization
	 */
	Flux_ReadWriteBuffer& GetLODLevelBuffer() { return m_xLODLevelBuffer; }

	/**
	 * Update chunk LOD allocations in GPU buffer based on current streaming manager state
	 * Called each frame after streaming manager updates
	 */
	void UpdateChunkLODAllocations();

private:
	Zenith_Entity m_xParentEntity;

	//#TO not owning - just references to materials and physics geometry
	// Render geometry now comes from Flux_TerrainStreamingManager
	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	Flux_Material* m_pxMaterial0 = nullptr;
	Flux_Material* m_pxMaterial1 = nullptr;

	// Facade mesh geometry that references streaming manager's buffers
	// This allows existing rendering code to work without changes
	Flux_MeshGeometry m_xRenderGeometryFacade;

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
};
