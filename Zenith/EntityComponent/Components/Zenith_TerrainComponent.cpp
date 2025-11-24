#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"

Zenith_TerrainComponent::Zenith_TerrainComponent(Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity)
	: m_pxMaterial0(&xMaterial0)
	, m_pxMaterial1(&xMaterial1)
	, m_xParentEntity(xEntity)
{
#pragma region Render
{
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);

			Zenith_AssetHandler::AddMesh("Terrain_Render" + strSuffix, std::string(ASSETS_ROOT"Terrain/Render_" + strSuffix + ".zmsh").c_str());
		}
	}

	Flux_MeshGeometry& xRenderGeometry = Zenith_AssetHandler::GetMesh("Terrain_Render0_0");

	const u_int64 ulTotalVertexDataSize = xRenderGeometry.GetVertexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalIndexDataSize = xRenderGeometry.GetIndexDataSize() * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;
	const u_int64 ulTotalPositionDataSize = xRenderGeometry.m_uNumVerts * sizeof(Zenith_Maths::Vector3) * TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

	xRenderGeometry.m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_pVertexData, ulTotalVertexDataSize));
	xRenderGeometry.m_ulReservedVertexDataSize = ulTotalVertexDataSize;

	xRenderGeometry.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xRenderGeometry.m_puIndices, ulTotalIndexDataSize));
	xRenderGeometry.m_ulReservedIndexDataSize = ulTotalIndexDataSize;

	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; x++)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; y++)
		{
			if(x == 0 && y == 0) continue;

			std::string strRenderMeshName = "Terrain_Render" + std::to_string(x) + "_" + std::to_string(y);
			Flux_MeshGeometry& xTerrainRenderMesh = Zenith_AssetHandler::GetMesh(strRenderMeshName);

			Flux_MeshGeometry::Combine(xRenderGeometry, xTerrainRenderMesh);
			Zenith_Log("Combined %u %u", x, y);

			Zenith_AssetHandler::DeleteMesh(strRenderMeshName);
		}
	}

	Flux_MemoryManager::InitialiseVertexBuffer(xRenderGeometry.GetVertexData(), xRenderGeometry.GetVertexDataSize(), xRenderGeometry.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xRenderGeometry.GetIndexData(), xRenderGeometry.GetIndexDataSize(), xRenderGeometry.m_xIndexBuffer);

	m_pxRenderGeometry = &xRenderGeometry;
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
