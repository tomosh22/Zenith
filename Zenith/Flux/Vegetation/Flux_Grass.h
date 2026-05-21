#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

enum Grass_DebugMode : u_int
{
	GRASS_DEBUG_NONE,
	GRASS_DEBUG_LOD_COLORS,         // Color by LOD level
	GRASS_DEBUG_CHUNK_BOUNDS,       // Wireframe chunk boundaries
	GRASS_DEBUG_DENSITY_HEAT,       // Heatmap of blade density
	GRASS_DEBUG_WIND_VECTORS,       // Visualize wind field
	GRASS_DEBUG_CULLING_RESULT,     // Show what's culled vs visible
	GRASS_DEBUG_BLADE_NORMALS,      // Visualize terrain normals used
	GRASS_DEBUG_HEIGHT_VARIATION,   // Color by blade height
	GRASS_DEBUG_PLACEMENT_MASK,     // Show terrain mask (where grass grows)
	GRASS_DEBUG_BUFFER_USAGE,       // Instance buffer utilization
	GRASS_DEBUG_COUNT
};

// Grass configuration constants
namespace GrassConfig
{
	constexpr u_int uBLADES_PER_SQM = 50;              // Density at LOD0
	constexpr float fLOD0_DISTANCE = 20.0f;            // Full geometry
	constexpr float fLOD1_DISTANCE = 50.0f;            // Reduced density
	constexpr float fLOD2_DISTANCE = 100.0f;           // Billboard/simplified
	constexpr float fMAX_DISTANCE = 200.0f;            // Culled beyond this
	constexpr float fCHUNK_SIZE = 64.0f;               // Matches terrain chunk
	constexpr u_int uMAX_INSTANCES_PER_CHUNK = 65536;  // Per-chunk limit
	constexpr u_int uMAX_VISIBLE_CHUNKS = 64;          // Chunks in view
	constexpr u_int uMAX_TOTAL_INSTANCES = 2000000;    // 2M blades max
}

// Per-blade instance data (GPU buffer layout)
struct GrassBladeInstance
{
	Zenith_Maths::Vector3 m_xPosition;
	float m_fRotation;         // Y-axis rotation (radians)
	float m_fHeight;           // Blade height
	float m_fWidth;            // Blade width
	float m_fBend;             // Initial bend amount
	u_int m_uColorTint;        // Packed RGBA8 color variation
};

// Per-chunk data
struct GrassChunk
{
	Zenith_Maths::Vector3 m_xCenter;
	float m_fRadius;
	u_int m_uInstanceOffset;
	u_int m_uInstanceCount;
	u_int m_uLOD;
	bool m_bVisible;
};

class Flux_Grass
{
public:
	Flux_Grass() = delete;
	~Flux_Grass() = delete;

	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();
	static void Reset();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Configuration (continuous parameters; on/off toggles live in Zenith_GraphicsOptions)
	static void SetDensityScale(float fScale);
	static void SetMaxDistance(float fDistance);
	static void SetWindStrength(float fStrength);
	static void SetWindDirection(const Zenith_Maths::Vector2& xDirection);

	// Getters
	static bool IsEnabled();
	static float GetDensityScale();
	static float GetMaxDistance();
	static bool IsWindEnabled();
	static float GetWindStrength();
	static const Zenith_Maths::Vector2& GetWindDirection();

	// Stats
	static u_int GetVisibleBladeCount();
	static u_int GetActiveChunkCount();
	static float GetBufferUsageMB();

	// Generate grass from terrain mesh data
	// Call this during initialization after terrain is loaded
	static void GenerateFromTerrain(const class Flux_MeshGeometry& xTerrainMesh);

#ifdef ZENITH_TOOLS
	static void RegisterDebugVariables();
#endif

private:
	static void ExecuteRender(Flux_CommandList* pxCmdList, void* pUserData);
	static void GenerateGrassForChunk(GrassChunk& xChunk, const Zenith_Maths::Vector3& xCenter);
	static void UpdateVisibleChunks();
	static void UploadInstanceData();
	static void CreateBuffers();
	static void DestroyBuffers();

	// Phase 7g: pipelines, instance buffer, chunk state and configuration moved
	// to Flux_GrassImpl held by Zenith_Engine. Reach them via
	// g_xEngine.Grass().m_xGrassPipeline etc.
};
