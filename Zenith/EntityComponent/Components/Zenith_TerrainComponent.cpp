#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Profiling/Zenith_Profiling.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetHandler.h"

const bool Zenith_TerrainComponent::IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const
{
	Zenith_Profiling::BeginProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	//#TO_TODO: this should be a camera frustum check against the terrain's encapsulating AABB
	Zenith_Maths::Vector3 xCamPos;
	xCam.GetPosition(xCamPos);
	const Zenith_Maths::Vector2 xCamPos_2D(xCamPos.x, xCamPos.z);

	bool bRet = (glm::length(xCamPos_2D - GetPosition_2D()) < xCam.GetFarPlane() * 2 * fVisibilityMultiplier);
	Zenith_Profiling::EndProfile(ZENITH_PROFILE_INDEX__VISIBILITY_CHECK);
	return bRet;
}

void Zenith_TerrainComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Get asset names from pointers
	std::string strRenderGeometryName = Zenith_AssetHandler::GetMeshName(m_pxRenderGeometry);
	std::string strPhysicsGeometryName = Zenith_AssetHandler::GetMeshName(m_pxPhysicsGeometry);
	std::string strWaterGeometryName = Zenith_AssetHandler::GetMeshName(m_pxWaterGeometry);
	std::string strMaterial0Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial0);
	std::string strMaterial1Name = Zenith_AssetHandler::GetMaterialName(m_pxMaterial1);

	// Write asset names
	xStream << strRenderGeometryName;
	xStream << strPhysicsGeometryName;
	xStream << strWaterGeometryName;
	xStream << strMaterial0Name;
	xStream << strMaterial1Name;

	// Write 2D position
	xStream << m_xPosition_2D;

	// m_xParentEntity reference is not serialized - will be restored during deserialization
}

void Zenith_TerrainComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read asset names
	std::string strRenderGeometryName;
	std::string strPhysicsGeometryName;
	std::string strWaterGeometryName;
	std::string strMaterial0Name;
	std::string strMaterial1Name;

	xStream >> strRenderGeometryName;
	xStream >> strPhysicsGeometryName;
	xStream >> strWaterGeometryName;
	xStream >> strMaterial0Name;
	xStream >> strMaterial1Name;

	// Look up assets by name (they must already be loaded)
	if (!strRenderGeometryName.empty() && !strPhysicsGeometryName.empty() &&
		!strWaterGeometryName.empty() && !strMaterial0Name.empty() && !strMaterial1Name.empty())
	{
		if (Zenith_AssetHandler::MeshExists(strRenderGeometryName) &&
			Zenith_AssetHandler::MeshExists(strPhysicsGeometryName) &&
			Zenith_AssetHandler::MeshExists(strWaterGeometryName) &&
			Zenith_AssetHandler::MaterialExists(strMaterial0Name) &&
			Zenith_AssetHandler::MaterialExists(strMaterial1Name))
		{
			m_pxRenderGeometry = &Zenith_AssetHandler::GetMesh(strRenderGeometryName);
			m_pxPhysicsGeometry = &Zenith_AssetHandler::GetMesh(strPhysicsGeometryName);
			m_pxWaterGeometry = &Zenith_AssetHandler::GetMesh(strWaterGeometryName);
			m_pxMaterial0 = &Zenith_AssetHandler::GetMaterial(strMaterial0Name);
			m_pxMaterial1 = &Zenith_AssetHandler::GetMaterial(strMaterial1Name);
		}
		else
		{
			// Asset not loaded - this is expected if assets haven't been loaded yet
			Zenith_Assert(false, "Referenced assets not found during TerrainComponent deserialization");
		}
	}

	// Read 2D position
	xStream >> m_xPosition_2D;

	// m_xParentEntity will be set by the entity deserialization system
}
