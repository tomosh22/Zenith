#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Flux_MeshGeometry;
class Flux_GraphicsImpl;
class Flux_HDRImpl;
class Zenith_Vulkan_MemoryManager;
class FrameContext;

enum Grass_DebugMode : u_int
{
	GRASS_DEBUG_NONE,
	GRASS_DEBUG_LOD_COLORS,
	GRASS_DEBUG_CHUNK_BOUNDS,
	GRASS_DEBUG_DENSITY_HEAT,
	GRASS_DEBUG_WIND_VECTORS,
	GRASS_DEBUG_CULLING_RESULT,
	GRASS_DEBUG_BLADE_NORMALS,
	GRASS_DEBUG_HEIGHT_VARIATION,
	GRASS_DEBUG_PLACEMENT_MASK,
	GRASS_DEBUG_BUFFER_USAGE,
	GRASS_DEBUG_COUNT
};

namespace GrassConfig
{
	constexpr u_int uBLADES_PER_SQM = 50;
	constexpr float fLOD0_DISTANCE = 20.0f;
	constexpr float fLOD1_DISTANCE = 50.0f;
	constexpr float fLOD2_DISTANCE = 100.0f;
	constexpr float fMAX_DISTANCE = 200.0f;
	constexpr float fCHUNK_SIZE = 64.0f;
	constexpr u_int uMAX_INSTANCES_PER_CHUNK = 65536;
	constexpr u_int uMAX_VISIBLE_CHUNKS = 64;
	constexpr u_int uMAX_TOTAL_INSTANCES = 2000000;
}

// Per-blade instance data (GPU buffer layout)
struct GrassBladeInstance
{
	Zenith_Maths::Vector3 m_xPosition;
	float m_fRotation;
	float m_fHeight;
	float m_fWidth;
	float m_fBend;
	u_int m_uColorTint;
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

// Phase 9: state + behaviour for Grass subsystem.
class Flux_GrassImpl
{
public:
	Flux_GrassImpl() = default;
	~Flux_GrassImpl() = default;

	Flux_GrassImpl(const Flux_GrassImpl&) = delete;
	Flux_GrassImpl& operator=(const Flux_GrassImpl&) = delete;

	// Injected engine-infra deps (de-globalisation, aggressive pass):
	//   xVulkanMemory owns the grass instance/constants/blade-mesh GPU buffers,
	//   xFrame supplies the wind animation time, xGraphics provides the camera /
	//   frame-constants / depth attachment, xHDR provides the HDR scene target.
	void Initialise(Zenith_Vulkan_MemoryManager& xVulkanMemory, FrameContext& xFrame,
		Flux_GraphicsImpl& xGraphics, Flux_HDRImpl& xHDR);
	void BuildPipelines();
	void Shutdown();
	void Reset();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void SetDensityScale(float fScale);
	void SetMaxDistance(float fDistance);
	void SetWindStrength(float fStrength);
	void SetWindDirection(const Zenith_Maths::Vector2& xDirection);

	bool IsEnabled() const;
	float GetDensityScale() const                                        { return m_fDensityScale; }
	float GetMaxDistance() const                                         { return m_fMaxDistance; }
	bool IsWindEnabled() const;
	float GetWindStrength() const                                        { return m_fWindStrength; }
	const Zenith_Maths::Vector2& GetWindDirection() const                { return m_xWindDirection; }

	u_int GetVisibleBladeCount() const                                   { return m_uVisibleBladeCount; }
	u_int GetActiveChunkCount() const                                    { return m_uActiveChunkCount; }
	float GetBufferUsageMB() const;

	void GenerateFromTerrain(const Flux_MeshGeometry& xTerrainMesh);

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();
#endif

public:
	void GenerateGrassForChunk(GrassChunk& xChunk, const Zenith_Maths::Vector3& xCenter);
	void UpdateVisibleChunks();
	void UploadInstanceData();
	void CreateBuffers();
	void DestroyBuffers();
	// Promoted from a file-static free function so its VulkanMemory reach-in
	// routes through the injected member instead of g_xEngine.
	void CreateGrassBladeMesh();

public:
	Flux_Pipeline                     m_xGrassPipeline;
	Flux_Shader                       m_xGrassShader;

	Flux_ReadWriteBuffer              m_xInstanceBuffer;
	u_int                             m_uAllocatedInstances = 0;

	Zenith_Vector<GrassChunk>         m_axChunks;
	u_int                             m_uVisibleBladeCount = 0;
	u_int                             m_uActiveChunkCount  = 0;

	Zenith_Vector<GrassBladeInstance> m_axAllInstances;
	bool                              m_bInstancesGenerated = false;
	bool                              m_bInstancesUploaded  = false;

	float                             m_fDensityScale  = 1.0f;
	float                             m_fMaxDistance   = GrassConfig::fMAX_DISTANCE;
	float                             m_fWindStrength  = 1.0f;
	Zenith_Maths::Vector2             m_xWindDirection = glm::normalize(Zenith_Maths::Vector2(1.0f, 0.2f));

	Flux_DynamicConstantBuffer        m_xGrassConstantsBuffer;

	// Injected engine-infra deps (stored in Initialise, nulled in Shutdown).
	// Public so the static graph trampoline can route through them after it
	// recovers the subsystem via g_xEngine.Grass().
	Zenith_Vulkan_MemoryManager*      m_pxVulkanMemory = nullptr;
	FrameContext*                     m_pxFrame        = nullptr;
	Flux_GraphicsImpl*                m_pxGraphics     = nullptr;
	Flux_HDRImpl*                     m_pxHDR          = nullptr;
};
