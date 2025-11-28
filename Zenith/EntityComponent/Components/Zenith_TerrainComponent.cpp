#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Terrain/Flux_TerrainCulling.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include <fstream>

Zenith_TerrainComponent::Zenith_TerrainComponent(Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity)
	: m_pxMaterial0(&xMaterial0)
	, m_pxMaterial1(&xMaterial1)
	, m_xParentEntity(xEntity)
{
#pragma region Render
{
	// Define LOD suffixes
	const char* LOD_SUFFIXES[4] = { "", "_LOD1", "_LOD2", "_LOD3" };
	
	// Load all chunks for all LOD levels
	// IMPORTANT: Load with POSITION attribute retained for AABB calculation
	for (uint32_t uLOD = 0; uLOD < 4; ++uLOD)
	{
		for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
		{
			for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
			{
				std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
				std::string strLODMeshName = std::string("Terrain_Render") + LOD_SUFFIXES[uLOD] + strSuffix;
				std::string strLODPath = std::string(ASSETS_ROOT"Terrain/Render") + LOD_SUFFIXES[uLOD] + "_" + strSuffix + ".zmsh";
				
				// Check if LOD file exists, fallback to LOD0 if not
				std::ifstream lodFile(strLODPath);
				if (!lodFile.good() && uLOD > 0)
				{
					// Use LOD0 as fallback
					strLODPath = std::string(ASSETS_ROOT"Terrain/Render_") + strSuffix + ".zmsh";
					Zenith_Log("WARNING: LOD%u not found for chunk (%u,%u), using LOD0 as fallback", uLOD, x, y);
					Zenith_Assert(false, "");
				}
				
				// Load mesh with POSITION attribute retained for AABB calculation
				Zenith_AssetHandler::AddMesh(strLODMeshName, strLODPath.c_str(), 
					1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
			}
		}
	}

	// Start with LOD0 chunk 0,0 as base
	Flux_MeshGeometry& xRenderGeometry = Zenith_AssetHandler::GetMesh("Terrain_Render0_0");

	// Calculate EXACT total size needed for ALL chunks × ALL LOD levels
	// Based on terrain export formulas from Zenith_Tools_TerrainExport.cpp
	//
	// Each chunk has:
	// - Base vertices: (TERRAIN_SIZE * density + 1)^2
	// - Right edge stitching (if not rightmost chunk): TERRAIN_SIZE * density verts
	// - Top edge stitching (if not topmost chunk): TERRAIN_SIZE * density verts
	// - Top-right corner (if has both edges): 1 vert
	//
	// Indices follow a similar pattern with extra triangles for stitching
	
	const uint32_t uNumChunks = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const uint32_t uChunksX = TERRAIN_EXPORT_DIMS;
	const uint32_t uChunksZ = TERRAIN_EXPORT_DIMS;
	
	// Calculate total for all chunks at all LOD levels
	uint32_t uTotalVerts = 0;
	uint32_t uTotalIndices = 0;
	
	const float densities[4] = { 1.0f, 0.5f, 0.25f, 0.125f }; // LOD0, LOD1, LOD2, LOD3
	
	for (uint32_t lodLevel = 0; lodLevel < 4; ++lodLevel)
	{
		float density = densities[lodLevel];
		
		for (uint32_t z = 0; z < uChunksZ; ++z)
		{
			for (uint32_t x = 0; x < uChunksX; ++x)
			{
				bool hasRightEdge = (x < uChunksX - 1);
				bool hasTopEdge = (z < uChunksZ - 1);
				
				// Base vertices and indices
				uint32_t verts = (uint32_t)((TERRAIN_SIZE * density + 1) * (TERRAIN_SIZE * density + 1));
				uint32_t indices = (uint32_t)((TERRAIN_SIZE * density) * (TERRAIN_SIZE * density) * 6);
				
				// Add edge stitching vertices and indices
				if (hasRightEdge)
				{
					verts += (uint32_t)(TERRAIN_SIZE * density);
					indices += (uint32_t)((TERRAIN_SIZE * density - 1) * 6); // Right edge triangles
				}
				if (hasTopEdge)
				{
					verts += (uint32_t)(TERRAIN_SIZE * density);
					indices += (uint32_t)((TERRAIN_SIZE * density - 1) * 6); // Top edge triangles
				}
				if (hasRightEdge && hasTopEdge)
				{
					verts += 1; // Top-right corner vertex
					indices += 6; // Corner triangles
				}
				
				uTotalVerts += verts;
				uTotalIndices += indices;
			}
		}
	}
	
	// Convert to byte sizes
	const u_int64 ulTotalVertexDataSize = static_cast<u_int64>(uTotalVerts) * xRenderGeometry.m_xBufferLayout.GetStride();
	const u_int64 ulTotalIndexDataSize = static_cast<u_int64>(uTotalIndices) * sizeof(Flux_MeshGeometry::IndexType);
	const u_int64 ulTotalPositionDataSize = static_cast<u_int64>(uTotalVerts) * sizeof(Zenith_Maths::Vector3);

	Zenith_Log("Terrain EXACT pre-allocation (with edge stitching): %u total verts, %u total indices across all LODs", uTotalVerts, uTotalIndices);
	Zenith_Log("Terrain EXACT pre-allocation: Vertex=%llu MB, Index=%llu MB, Position=%llu MB",
		ulTotalVertexDataSize / (1024*1024), ulTotalIndexDataSize / (1024*1024), ulTotalPositionDataSize / (1024*1024));
	
	// Pre-allocate buffers and set reserved sizes BEFORE combining to avoid reallocations
	xRenderGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_pVertexData, ulTotalVertexDataSize));
	xRenderGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xRenderGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_puIndices, ulTotalIndexDataSize));
	xRenderGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	// Also pre-allocate position buffer (needed for AABB calculations later)
	if (xRenderGeometry.m_pxPositions)
	{
		xRenderGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_pxPositions, ulTotalPositionDataSize));
		xRenderGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;
	}
	
	// Combine all chunks for all LOD levels
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			for (uint32_t uLOD = 0; uLOD < 4; ++uLOD)
			{
				// Skip the first chunk's LOD0 (already loaded as base)
				if (x == 0 && y == 0 && uLOD == 0) continue;

				std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
				std::string strLODMeshName = std::string("Terrain_Render") + LOD_SUFFIXES[uLOD] + strSuffix;
				Flux_MeshGeometry& xTerrainRenderMesh = Zenith_AssetHandler::GetMesh(strLODMeshName);

				Flux_MeshGeometry::Combine(xRenderGeometry, xTerrainRenderMesh);
				
				if ((x * TERRAIN_EXPORT_DIMS + y) % 256 == 0 || uLOD == 0)
				{
					Zenith_Log("Combined LOD%u chunk (%u,%u)", uLOD, x, y);
				}

				// Don't delete yet - we'll need these for BuildCullingData()
			}
		}
	}

	Zenith_Log("Terrain: Combined %u chunks × 4 LOD levels into unified vertex/index buffers", TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS);
	Zenith_Log("Terrain: Total vertices: %u, Total indices: %u", xRenderGeometry.m_uNumVerts, xRenderGeometry.m_uNumIndices);

	Flux_MemoryManager::InitialiseVertexBuffer(xRenderGeometry.GetVertexData(), xRenderGeometry.GetVertexDataSize(), xRenderGeometry.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xRenderGeometry.GetIndexData(), xRenderGeometry.GetIndexDataSize(), xRenderGeometry.m_xIndexBuffer);

	m_pxRenderGeometry = &xRenderGeometry;

	// Build GPU culling data now that all terrain meshes are loaded
	// This extracts AABBs and LOD index ranges from the meshes we just loaded
	Flux_TerrainCulling::BuildChunkData();
}
#pragma endregion

#pragma region Physics
{
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);

			Zenith_AssetHandler::AddMesh("Terrain_Physics" + strSuffix, std::string(ASSETS_ROOT"Terrain/Physics_" + strSuffix + ".zmsh").c_str(), 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION | 1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__NORMAL);
		}
	}

	Flux_MeshGeometry& xPhysicsGeometry = Zenith_AssetHandler::GetMesh("Terrain_Physics0_0");

	const u_int64 ulTotalVertexDataSize = xPhysicsGeometry.GetVertexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalIndexDataSize = xPhysicsGeometry.GetIndexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalPositionDataSize = xPhysicsGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3) * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	xPhysicsGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pVertexData, ulTotalVertexDataSize));
	xPhysicsGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xPhysicsGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_puIndices, ulTotalIndexDataSize));
xPhysicsGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	xPhysicsGeometry.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xPhysicsGeometry.m_pxPositions, ulTotalPositionDataSize));
	xPhysicsGeometry.m_ulReservedPositionDataSize = ulTotalPositionDataSize;

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			if (x == 0 && y == 0) continue;

			std::string strPhysicsMeshName = "Terrain_Physics" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainPhysicsMesh = Zenith_AssetHandler::GetMesh(strPhysicsMeshName);

			Flux_MeshGeometry::Combine(xPhysicsGeometry, xTerrainPhysicsMesh);
			Zenith_Log("Combined %u %u", x, y);

			Zenith_AssetHandler::DeleteMesh(strPhysicsMeshName);
		}
	}

	m_pxPhysicsGeometry = &xPhysicsGeometry;
}
#pragma endregion
}

Zenith_TerrainComponent::~Zenith_TerrainComponent()
{
	Zenith_AssetHandler::DeleteMesh("Terrain_Render0_0");
	Zenith_AssetHandler::DeleteMesh("Terrain_Physics0_0");
}

const bool Zenith_TerrainComponent::IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	//#TO_TODO: this should be a camera frustum check against the terrain's encapsulating AABB
	Zenith_Maths::Vector3 xCamPos;
	xCam.GetPosition(xCamPos);
	const Zenith_Maths::Vector2 xCamPos_2D(xCamPos.x, xCamPos.z);

	bool bRet = true;//(glm::length(xCamPos_2D - GetPosition_2D()) < xCam.GetFarPlane() * 2 * fVisibilityMultiplier);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	return bRet;
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Get asset names from pointers
	std::string strRenderGeometryName = Zenith_AssetHandler::GetMeshName(m_pxRenderGeometry);
	std::string strPhysicsGeometryName = Zenith_AssetHandler::GetMeshName(m_pxPhysicsGeometry);
	std::string strMaterial0Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial0);
	std::string strMaterial1Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial1);

	// Write asset names
	xStream << strRenderGeometryName;
	xStream << strPhysicsGeometryName;
	xStream << strMaterial0Name;
	xStream << strMaterial1Name;

	// m_xParentEntity reference is not serialized - will be restored during deserialization
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read asset names
	std::string strRenderGeometryName;
	std::string strPhysicsGeometryName;
	std::string strMaterial0Name;
	std::string strMaterial1Name;

	xStream >> strRenderGeometryName;
	xStream >> strPhysicsGeometryName;
 xStream >> strMaterial0Name;
	xStream >> strMaterial1Name;

	// Look up assets by name (they must already be loaded)
	if (!strRenderGeometryName.empty() && !strPhysicsGeometryName.empty() && !strMaterial0Name.empty() && !strMaterial1Name.empty())
	{
		if (Zenith_AssetHandler::MeshExists(strRenderGeometryName) &&
			Zenith_AssetHandler::MeshExists(strPhysicsGeometryName) &&
			Zenith_AssetHandler::MaterialExists(strMaterial0Name) &&
			Zenith_AssetHandler::MaterialExists(strMaterial1Name))
		{
			m_pxRenderGeometry = &Zenith_AssetHandler::GetMesh(strRenderGeometryName);
			m_pxPhysicsGeometry = &Zenith_AssetHandler::GetMesh(strPhysicsGeometryName);
			m_pxMaterial0 = &Zenith_AssetHandler::GetMaterial(strMaterial0Name);
			m_pxMaterial1 = &Zenith_AssetHandler::GetMaterial(strMaterial1Name);
		}
		else
		{
			// Asset not loaded - this is expected if assets haven't been loaded yet
			Zenith_Assert(false, "Referenced assets not found during TerrainComponent deserialization");
		}
	}

	// m_xParentEntity will be set by the entity deserialization system
}

