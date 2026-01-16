#pragma once
/**
 * Exploration_TerrainExplorer.h - Terrain interaction and observation
 *
 * Demonstrates:
 * - Terrain height sampling for player placement
 * - Streaming state observation for debug display
 * - Chunk position tracking
 * - LOD distance visualization
 *
 * Engine APIs used:
 * - Zenith_TerrainComponent
 * - Flux_TerrainStreamingManager
 * - Flux_TerrainConfig
 */

#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <algorithm>

namespace Exploration_TerrainExplorer
{
	using namespace Flux_TerrainConfig;

	// ========================================================================
	// Terrain Info Structure
	// ========================================================================
	struct TerrainInfo
	{
		float m_fHeight = 0.0f;               // Height at current position
		int32_t m_iChunkX = 0;                // Current chunk X coordinate
		int32_t m_iChunkY = 0;                // Current chunk Y coordinate
		uint32_t m_uCurrentLOD = 3;           // LOD level at current position
		bool m_bOnTerrain = true;             // Whether position is within terrain bounds
	};

	// ========================================================================
	// Streaming Stats Structure
	// ========================================================================
	struct StreamingStats
	{
		uint32_t m_uHighLODChunksResident = 0;
		uint32_t m_uStreamsThisFrame = 0;
		uint32_t m_uEvictionsThisFrame = 0;
		float m_fVertexBufferUsageMB = 0.0f;
		float m_fVertexBufferTotalMB = 0.0f;
		float m_fIndexBufferUsageMB = 0.0f;
		float m_fIndexBufferTotalMB = 0.0f;
	};

	/**
	 * Convert mesh position to chunk coordinates
	 * Terrain mesh goes from (0, 0) to (TERRAIN_SIZE, TERRAIN_SIZE)
	 */
	inline void WorldPosToChunkCoords(const Zenith_Maths::Vector3& xMeshPos, int32_t& iChunkX, int32_t& iChunkY)
	{
		// Mesh coordinates are 0 to TERRAIN_SIZE, divide by chunk size to get chunk index
		iChunkX = static_cast<int32_t>(std::floor(xMeshPos.x / CHUNK_SIZE_WORLD));
		iChunkY = static_cast<int32_t>(std::floor(xMeshPos.z / CHUNK_SIZE_WORLD));
	}

	/**
	 * Check if chunk coordinates are valid
	 */
	inline bool IsChunkValid(int32_t iChunkX, int32_t iChunkY)
	{
		return iChunkX >= 0 && iChunkX < static_cast<int32_t>(CHUNK_GRID_SIZE) &&
		       iChunkY >= 0 && iChunkY < static_cast<int32_t>(CHUNK_GRID_SIZE);
	}

	/**
	 * Get terrain height at a mesh XZ position
	 * Converts mesh coordinates to procedural world coordinates, calculates
	 * procedural height, then scales to match terrain mesh Y coordinates.
	 *
	 * @param fMeshX Mesh X coordinate (0 to TERRAIN_SIZE)
	 * @param fMeshZ Mesh Z coordinate (0 to TERRAIN_SIZE)
	 * @return Terrain mesh Y coordinate
	 */
	inline float GetTerrainHeightAt(float fMeshX, float fMeshZ)
	{
		// Convert mesh coordinates to procedural world coordinates
		// Mesh coords: 0 to TERRAIN_SIZE
		// Procedural coords: -TERRAIN_SIZE/2 to +TERRAIN_SIZE/2
		// NOTE: Z is negated because the heightmap was flipped vertically during generation
		float fProcX = fMeshX - TERRAIN_SIZE * 0.5f;
		float fProcZ = TERRAIN_SIZE * 0.5f - fMeshZ;  // Negated due to heightmap flip

		// Simple multi-octave noise approximation (same as heightmap generation)
		float fProceduralHeight = 0.0f;

		// Large hills
		float fFreq1 = 0.001f;
		fProceduralHeight += std::sin(fProcX * fFreq1) * std::cos(fProcZ * fFreq1) * 50.0f;

		// Medium features
		float fFreq2 = 0.005f;
		fProceduralHeight += std::sin(fProcX * fFreq2 + 1.3f) * std::cos(fProcZ * fFreq2 + 0.7f) * 20.0f;

		// Small details
		float fFreq3 = 0.02f;
		fProceduralHeight += std::sin(fProcX * fFreq3 + 2.1f) * std::cos(fProcZ * fFreq3 + 1.4f) * 5.0f;

		// Add base height to keep most terrain above water level
		fProceduralHeight += 30.0f;

		// Clamp to reasonable terrain bounds
		fProceduralHeight = std::max(0.0f, fProceduralHeight);

		// Convert procedural height (0-100) to terrain mesh Y scale
		// Terrain export uses: meshY = normalizedHeight * 4096 - 1000
		float fNormalized = fProceduralHeight / 100.0f;
		float fMeshY = fNormalized * 4096.0f - 1000.0f;

		return fMeshY;
	}

	/**
	 * Get comprehensive terrain information at a position
	 */
	inline TerrainInfo GetTerrainInfo(const Zenith_Maths::Vector3& xWorldPos)
	{
		TerrainInfo xInfo;

		// Get chunk coordinates
		WorldPosToChunkCoords(xWorldPos, xInfo.m_iChunkX, xInfo.m_iChunkY);

		// Check if on terrain
		xInfo.m_bOnTerrain = IsChunkValid(xInfo.m_iChunkX, xInfo.m_iChunkY);

		// Get height
		xInfo.m_fHeight = GetTerrainHeightAt(xWorldPos.x, xWorldPos.z);

		// Calculate LOD level based on distance from terrain center
		// (This is a simplification - actual LOD is per-chunk based on camera distance)
		float fDistFromCenterSq = xWorldPos.x * xWorldPos.x + xWorldPos.z * xWorldPos.z;
		xInfo.m_uCurrentLOD = SelectLOD(fDistFromCenterSq);

		return xInfo;
	}

	/**
	 * Get streaming statistics from the terrain system
	 */
	inline StreamingStats GetStreamingStats()
	{
		StreamingStats xStats;

		if (Flux_TerrainStreamingManager::IsInitialized())
		{
			const auto& xEngineStats = Flux_TerrainStreamingManager::GetStats();
			xStats.m_uHighLODChunksResident = xEngineStats.m_uHighLODChunksResident;
			xStats.m_uStreamsThisFrame = xEngineStats.m_uStreamsThisFrame;
			xStats.m_uEvictionsThisFrame = xEngineStats.m_uEvictionsThisFrame;
			xStats.m_fVertexBufferUsageMB = static_cast<float>(xEngineStats.m_uVertexBufferUsedMB);
			xStats.m_fVertexBufferTotalMB = static_cast<float>(xEngineStats.m_uVertexBufferTotalMB);
			xStats.m_fIndexBufferUsageMB = static_cast<float>(xEngineStats.m_uIndexBufferUsedMB);
			xStats.m_fIndexBufferTotalMB = static_cast<float>(xEngineStats.m_uIndexBufferTotalMB);
		}

		return xStats;
	}

	/**
	 * Get LOD residency state for a specific chunk
	 */
	inline uint32_t GetChunkResidentLOD(int32_t iChunkX, int32_t iChunkY)
	{
		if (!IsChunkValid(iChunkX, iChunkY))
			return LOD_ALWAYS_RESIDENT;

		if (!Flux_TerrainStreamingManager::IsInitialized())
			return LOD_ALWAYS_RESIDENT;

		// Check which LODs are resident, return highest quality
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; ++uLOD)
		{
			if (Flux_TerrainStreamingManager::GetResidencyState(
					static_cast<uint32_t>(iChunkX),
					static_cast<uint32_t>(iChunkY),
					uLOD) == Flux_TerrainLODResidencyState::RESIDENT)
			{
				return uLOD;
			}
		}

		return LOD_ALWAYS_RESIDENT;
	}

	/**
	 * Calculate distance to chunk center (mesh coordinates)
	 */
	inline float GetDistanceToChunk(const Zenith_Maths::Vector3& xMeshPos, int32_t iChunkX, int32_t iChunkY)
	{
		// Calculate chunk center in mesh coordinates (0 to TERRAIN_SIZE)
		float fChunkCenterX = (static_cast<float>(iChunkX) + 0.5f) * CHUNK_SIZE_WORLD;
		float fChunkCenterZ = (static_cast<float>(iChunkY) + 0.5f) * CHUNK_SIZE_WORLD;

		float fDx = xMeshPos.x - fChunkCenterX;
		float fDz = xMeshPos.z - fChunkCenterZ;

		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	/**
	 * Get terrain bounds (mesh coordinates)
	 */
	inline void GetTerrainBounds(Zenith_Maths::Vector3& xMin, Zenith_Maths::Vector3& xMax)
	{
		// Terrain mesh goes from (0, 0) to (TERRAIN_SIZE, TERRAIN_SIZE)
		// Y range is from the mesh export: -1000 to 3096
		xMin = Zenith_Maths::Vector3(0.0f, -1000.0f, 0.0f);
		xMax = Zenith_Maths::Vector3(TERRAIN_SIZE, 3096.0f, TERRAIN_SIZE);
	}

	/**
	 * Clamp position to terrain bounds (mesh coordinates)
	 */
	inline Zenith_Maths::Vector3 ClampToTerrainBounds(const Zenith_Maths::Vector3& xPos)
	{
		float fMargin = 50.0f;  // Keep player slightly inside bounds

		Zenith_Maths::Vector3 xClamped = xPos;
		xClamped.x = std::clamp(xClamped.x, fMargin, TERRAIN_SIZE - fMargin);
		xClamped.z = std::clamp(xClamped.z, fMargin, TERRAIN_SIZE - fMargin);

		return xClamped;
	}

	/**
	 * Get LOD name string for display
	 */
	inline const char* GetLODDisplayName(uint32_t uLOD)
	{
		return GetLODName(uLOD);
	}

	/**
	 * Get total terrain chunk count
	 */
	inline uint32_t GetTotalChunkCount()
	{
		return TOTAL_CHUNKS;
	}

	/**
	 * Get terrain size in world units
	 */
	inline float GetTerrainSize()
	{
		return TERRAIN_SIZE;
	}

} // namespace Exploration_TerrainExplorer
