#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"

// Log tag for model component physics mesh operations
static constexpr const char* LOG_TAG_MODEL_PHYSICS = "[ModelPhysics]";

// Helper function to check if we should delete assets in the destructor
// Returns false during scene loading to prevent deleting assets that will be reused
bool Zenith_ModelComponent_ShouldDeleteAssets()
{
	return !Zenith_Scene::IsLoadingScene();
}

void Zenith_ModelComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write the number of mesh entries
	u_int uNumEntries = m_xMeshEntries.GetSize();
	xStream << uNumEntries;

	// Write each mesh entry (as asset name references)
	for (u_int u = 0; u < uNumEntries; u++)
	{
		const MeshEntry& xEntry = m_xMeshEntries.Get(u);

		// Get asset names from pointers
		std::string strMeshName = Zenith_AssetHandler::GetMeshName(xEntry.m_pxGeometry);
		std::string strMaterialName = Zenith_AssetHandler::GetMaterialName(xEntry.m_pxMaterial);

		xStream << strMeshName;
		xStream << strMaterialName;
	}

	// Note: m_xCreatedTextures, m_xCreatedMaterials, m_xCreatedMeshes are not serialized
	// These are runtime tracking arrays for cleanup purposes only
	// The mesh entries themselves contain all necessary asset references
}

void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Clear existing mesh entries
	m_xMeshEntries.Clear();

	// Read the number of mesh entries
	u_int uNumEntries;
	xStream >> uNumEntries;

	// Read and reconstruct each mesh entry
	for (u_int u = 0; u < uNumEntries; u++)
	{
		std::string strMeshName;
		std::string strMaterialName;

		xStream >> strMeshName;
		xStream >> strMaterialName;

		// Look up assets by name (they must already be loaded)
		if (!strMeshName.empty() && !strMaterialName.empty())
		{
			if (Zenith_AssetHandler::MeshExists(strMeshName) &&
				Zenith_AssetHandler::MaterialExists(strMaterialName))
			{
				Flux_MeshGeometry& xMesh = Zenith_AssetHandler::GetMesh(strMeshName);
				Flux_Material& xMaterial = Zenith_AssetHandler::GetMaterial(strMaterialName);
				AddMeshEntry(xMesh, xMaterial);
			}
			else
			{
				// Asset not loaded - this is expected if assets haven't been loaded yet
				// The scene loader should ensure assets are loaded before component deserialization
				Zenith_Assert(false, "Referenced assets not found during ModelComponent deserialization");
			}
		}
	}

	// Generate physics mesh after deserializing if auto-generation is enabled
	if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_xMeshEntries.GetSize() > 0)
	{
		GeneratePhysicsMesh();
	}

	// m_xParentEntity will be set by the entity deserialization system
}

void Zenith_ModelComponent::GeneratePhysicsMesh(PhysicsMeshQuality eQuality)
{
	PhysicsMeshConfig xConfig = g_xPhysicsMeshConfig;
	xConfig.m_eQuality = eQuality;
	GeneratePhysicsMeshWithConfig(xConfig);
}

void Zenith_ModelComponent::GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig)
{
	// Clean up existing physics mesh
	ClearPhysicsMesh();

	if (m_xMeshEntries.GetSize() == 0)
	{
		Zenith_Log("%s Cannot generate physics mesh: no mesh entries", LOG_TAG_MODEL_PHYSICS);
		return;
	}

	// Collect all mesh geometries
	Zenith_Vector<Flux_MeshGeometry*> xMeshGeometries;
	for (uint32_t i = 0; i < m_xMeshEntries.GetSize(); i++)
	{
		if (m_xMeshEntries.Get(i).m_pxGeometry)
		{
			xMeshGeometries.PushBack(m_xMeshEntries.Get(i).m_pxGeometry);
		}
	}

	if (xMeshGeometries.GetSize() == 0)
	{
		Zenith_Log("%s Cannot generate physics mesh: no valid geometries", LOG_TAG_MODEL_PHYSICS);
		return;
	}

	// Log current entity scale
	if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		Zenith_Maths::Vector3 xScale;
		xTransform.GetScale(xScale);
		Zenith_Log("%s Generating physics mesh with entity scale (%.3f, %.3f, %.3f)",
			LOG_TAG_MODEL_PHYSICS, xScale.x, xScale.y, xScale.z);
	}

	// Generate the physics mesh
	m_pxPhysicsMesh = Zenith_PhysicsMeshGenerator::GeneratePhysicsMeshWithConfig(xMeshGeometries, xConfig);

	if (m_pxPhysicsMesh)
	{
		Zenith_Log("%s Generated physics mesh for model: %u verts, %u tris",
			LOG_TAG_MODEL_PHYSICS,
			m_pxPhysicsMesh->GetNumVerts(),
			m_pxPhysicsMesh->GetNumIndices() / 3);
		
		// Log first vertex position for debugging
		if (m_pxPhysicsMesh->GetNumVerts() > 0)
		{
			Zenith_Maths::Vector3& v0 = m_pxPhysicsMesh->m_pxPositions[0];
			Zenith_Log("%s First vertex in model space: (%.3f, %.3f, %.3f)",
				LOG_TAG_MODEL_PHYSICS, v0.x, v0.y, v0.z);
		}
	}
	else
	{
		Zenith_Log("%s Failed to generate physics mesh for model", LOG_TAG_MODEL_PHYSICS);
	}
}

void Zenith_ModelComponent::ClearPhysicsMesh()
{
	if (m_pxPhysicsMesh)
	{
		delete m_pxPhysicsMesh;
		m_pxPhysicsMesh = nullptr;
	}
}

void Zenith_ModelComponent::DebugDrawPhysicsMesh()
{
	if (!m_bDebugDrawPhysicsMesh || !m_pxPhysicsMesh)
	{
		return;
	}

	// Get the transform matrix from the parent entity
	if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	
	Zenith_Maths::Matrix4 xModelMatrix;
	xTransform.BuildModelMatrix(xModelMatrix);

	Zenith_Log("%s DebugDraw: Entity scale (%.3f, %.3f, %.3f), verts=%u",
		LOG_TAG_MODEL_PHYSICS, xScale.x, xScale.y, xScale.z, m_pxPhysicsMesh->GetNumVerts());

	// Draw the physics mesh
	Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(m_pxPhysicsMesh, xModelMatrix, m_xDebugDrawColor);
}