#pragma once
/**
 * Runner_TerrainManager.h - Terrain entity setup and management
 *
 * Demonstrates:
 * - Zenith_TerrainComponent - GPU-driven terrain with LOD streaming
 * - Procedural terrain chunk generation
 * - Infinite scrolling terrain for endless runner
 *
 * For this demo, we use a simplified procedural ground plane since
 * full terrain requires heightmap assets. The patterns shown here
 * demonstrate how you would set up Zenith_TerrainComponent.
 *
 * Terrain System Overview:
 * - Zenith_TerrainComponent manages GPU buffers for terrain mesh
 * - Flux_Terrain handles rendering with LOD streaming
 * - Chunks are culled on GPU via compute shader
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Maths/Zenith_Maths.h"
#include "Prefab/Zenith_Prefab.h"
#include <vector>

// Forward declaration
class Zenith_Prefab;

/**
 * Runner_TerrainManager - Manages ground/terrain for the runner
 *
 * In a full implementation with Zenith_TerrainComponent:
 * 1. Create terrain entity with TerrainComponent
 * 2. Load heightmap data
 * 3. TerrainComponent handles GPU streaming and LOD
 * 4. Flux_Terrain renders with frustum culling
 *
 * For this demo:
 * - Uses procedural ground chunks (cubes)
 * - Chunks spawn ahead and despawn behind player
 * - Simulates infinite scrolling terrain
 */
class Runner_TerrainManager
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================
	struct Config
	{
		float m_fChunkLength = 100.0f;
		float m_fChunkWidth = 20.0f;
		uint32_t m_uActiveChunkCount = 5;
		float m_fHeightVariation = 2.0f;
	};

	// ========================================================================
	// Chunk tracking
	// ========================================================================
	struct TerrainChunk
	{
		Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
		float m_fStartZ = 0.0f;
		float m_fEndZ = 0.0f;
		float m_fHeight = 0.0f;  // Y offset for terrain variation
	};

	// ========================================================================
	// Initialization
	// ========================================================================
	static void Initialize(
		const Config& xConfig,
		Zenith_Prefab* pxGroundPrefab,
		Flux_MeshGeometry* pxCubeGeometry,
		Zenith_MaterialAsset* pxGroundMaterial)
	{
		s_xConfig = xConfig;
		s_pxGroundPrefab = pxGroundPrefab;
		s_pxCubeGeometry = pxCubeGeometry;
		s_pxGroundMaterial = pxGroundMaterial;

		Reset();
	}

	static void Reset()
	{
		// Destroy existing chunks
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		for (auto& xChunk : s_axChunks)
		{
			if (xChunk.m_uEntityID.IsValid() && xScene.EntityExists(xChunk.m_uEntityID))
			{
				Zenith_Scene::Destroy(xChunk.m_uEntityID);
			}
		}
		s_axChunks.clear();

		// Create initial chunks
		float fChunkZ = -s_xConfig.m_fChunkLength;  // Start one chunk behind origin
		for (uint32_t i = 0; i < s_xConfig.m_uActiveChunkCount + 1; i++)
		{
			CreateChunk(fChunkZ);
			fChunkZ += s_xConfig.m_fChunkLength;
		}
	}

	// ========================================================================
	// Update
	// ========================================================================
	static void Update(float fPlayerZ)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

		// Check if we need to spawn new chunks ahead
		if (!s_axChunks.empty())
		{
			float fFarthestZ = s_axChunks.back().m_fEndZ;
			float fSpawnThreshold = fPlayerZ + s_xConfig.m_fChunkLength * (s_xConfig.m_uActiveChunkCount - 1);

			while (fFarthestZ < fSpawnThreshold)
			{
				CreateChunk(fFarthestZ);
				fFarthestZ += s_xConfig.m_fChunkLength;
			}
		}

		// Remove chunks that are too far behind
		float fDespawnThreshold = fPlayerZ - s_xConfig.m_fChunkLength * 2.0f;
		while (!s_axChunks.empty() && s_axChunks.front().m_fEndZ < fDespawnThreshold)
		{
			if (s_axChunks.front().m_uEntityID.IsValid() && xScene.EntityExists(s_axChunks.front().m_uEntityID))
			{
				Zenith_Scene::Destroy(s_axChunks.front().m_uEntityID);
			}
			s_axChunks.erase(s_axChunks.begin());
		}
	}

	// ========================================================================
	// Terrain Queries
	// ========================================================================
	static float GetTerrainHeightAt(float fZ)
	{
		// Find which chunk contains this Z position
		for (const auto& xChunk : s_axChunks)
		{
			if (fZ >= xChunk.m_fStartZ && fZ < xChunk.m_fEndZ)
			{
				// Simple: return chunk height (flat chunk)
				// For full terrain: would sample heightmap
				return xChunk.m_fHeight;
			}
		}
		return 0.0f;  // Default ground level
	}

	static const std::vector<TerrainChunk>& GetChunks() { return s_axChunks; }

	/*
	// ========================================================================
	// EXAMPLE: Real Zenith_TerrainComponent usage
	// ========================================================================
	// This is how you would set up actual GPU-driven terrain:

	static void InitializeRealTerrain(Zenith_Scene& xScene)
	{
		// Create terrain entity
		Zenith_Entity xTerrainEntity(&xScene, "Terrain");

		// Create materials for terrain texture blending
		// Material 0 = grass, Material 1 = dirt
		Zenith_MaterialAsset& xGrassMat = *Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
		xGrassMat.SetName("TerrainGrass");
		xGrassMat.SetDiffuseTexturePath("Textures/grass_diffuse.ztex");

		Zenith_MaterialAsset& xDirtMat = *Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
		xDirtMat.SetName("TerrainDirt");
		xDirtMat.SetDiffuseTexturePath("Textures/dirt_diffuse.ztex");

		// Add TerrainComponent - this handles:
		// 1. Loading terrain mesh data (heightmap -> mesh)
		// 2. Setting up unified GPU buffers
		// 3. GPU culling compute shader dispatch
		// 4. LOD streaming management
		// Note: AddComponent automatically passes entity as last arg
		Zenith_TerrainComponent& xTerrain = xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			xGrassMat, xDirtMat);

		// After construction, TerrainComponent:
		// - Has 4096 chunks (64x64 grid)
		// - LOD3 always resident in buffer
		// - LOD0-2 streamed based on camera distance
		// - GPU compute culling via Flux_Terrain

		// The terrain renders automatically via Flux_Terrain::RenderToGBuffer()
		// which is called in the render pipeline
	}

	// Each frame, Flux_Terrain:
	// 1. Updates streaming based on camera position
	// 2. Dispatches compute shader for frustum culling
	// 3. Issues indirect draw call for visible chunks
	// 4. Each chunk renders with appropriate LOD
	*/

private:
	// ========================================================================
	// Chunk Creation
	// ========================================================================
	static void CreateChunk(float fStartZ)
	{
		if (s_pxGroundPrefab == nullptr || s_pxCubeGeometry == nullptr || s_pxGroundMaterial == nullptr)
		{
			return;
		}

		TerrainChunk xChunk;
		xChunk.m_fStartZ = fStartZ;
		xChunk.m_fEndZ = fStartZ + s_xConfig.m_fChunkLength;

		// Calculate height variation (simple sine wave for demo)
		float fProgress = fStartZ / (s_xConfig.m_fChunkLength * 10.0f);
		xChunk.m_fHeight = sin(fProgress * 3.14159f * 2.0f) * s_xConfig.m_fHeightVariation;

		// Create ground entity
		Zenith_Entity xGround = s_pxGroundPrefab->Instantiate(&Zenith_Scene::GetCurrentScene(), "Ground");

		// Position: center of chunk, below player level
		Zenith_Maths::Vector3 xPos(
			0.0f,
			xChunk.m_fHeight - 0.5f,  // Slightly below ground level
			fStartZ + s_xConfig.m_fChunkLength * 0.5f
		);

		// Scale: flat wide chunk
		Zenith_Maths::Vector3 xScale(
			s_xConfig.m_fChunkWidth,
			1.0f,  // Thin ground plane
			s_xConfig.m_fChunkLength
		);

		Zenith_TransformComponent& xTransform = xGround.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPos);
		xTransform.SetScale(xScale);

		// Add model
		Zenith_ModelComponent& xModel = xGround.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*s_pxCubeGeometry, *s_pxGroundMaterial);

		xChunk.m_uEntityID = xGround.GetEntityID();
		s_axChunks.push_back(xChunk);
	}

	// ========================================================================
	// Static State
	// ========================================================================
	static inline Config s_xConfig;
	static inline std::vector<TerrainChunk> s_axChunks;
	static inline Zenith_Prefab* s_pxGroundPrefab = nullptr;
	static inline Flux_MeshGeometry* s_pxCubeGeometry = nullptr;
	static inline Zenith_MaterialAsset* s_pxGroundMaterial = nullptr;
};
