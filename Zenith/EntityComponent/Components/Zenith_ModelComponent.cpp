#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

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

	// Write each mesh entry using source paths for meshes
	// Materials are recreated from mesh data on load
	for (u_int u = 0; u < uNumEntries; u++)
	{
		const MeshEntry& xEntry = m_xMeshEntries.Get(u);

		// Get mesh source path (set when loaded from file via AddMeshFromFile)
		std::string strMeshPath = xEntry.m_pxGeometry ? xEntry.m_pxGeometry->m_strSourcePath : "";
		xStream << strMeshPath;

		// Serialize material base color (materials are recreated, textures must be loaded separately)
		Zenith_Maths::Vector4 xBaseColor = xEntry.m_pxMaterial ? xEntry.m_pxMaterial->GetBaseColor() : Zenith_Maths::Vector4(1.f);
		xStream << xBaseColor.x;
		xStream << xBaseColor.y;
		xStream << xBaseColor.z;
		xStream << xBaseColor.w;

		// Serialize animation path if animation exists
		std::string strAnimPath = "";
		if (xEntry.m_pxGeometry && xEntry.m_pxGeometry->m_pxAnimation)
		{
			strAnimPath = xEntry.m_pxGeometry->m_pxAnimation->GetSourcePath();
		}
		xStream << strAnimPath;
	}

	// Note: m_xCreatedTextures, m_xCreatedMaterials, m_xCreatedMeshes are not serialized
	// These are runtime tracking arrays for cleanup purposes only
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
		std::string strMeshPath;
		xStream >> strMeshPath;

		Zenith_Maths::Vector4 xBaseColor;
		xStream >> xBaseColor.x;
		xStream >> xBaseColor.y;
		xStream >> xBaseColor.z;
		xStream >> xBaseColor.w;

		// Read animation path (may be empty)
		std::string strAnimPath;
		xStream >> strAnimPath;

		// Load mesh from source path
		if (!strMeshPath.empty())
		{
			Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(strMeshPath.c_str());
			if (pxMesh)
			{
				m_xCreatedMeshes.PushBack(pxMesh);

				// Create a new material with the serialized base color
				Flux_Material* pxMaterial = Zenith_AssetHandler::AddMaterial();
				if (pxMaterial)
				{
					m_xCreatedMaterials.PushBack(pxMaterial);
					pxMaterial->SetBaseColor(xBaseColor);
					AddMeshEntry(*pxMesh, *pxMaterial);
				}
				else
				{
					Zenith_Log("[ModelComponent] Failed to create material during deserialization");
					AddMeshEntry(*pxMesh, *Flux_Graphics::s_pxBlankMaterial);
				}

				// Recreate animation if path was serialized
				if (!strAnimPath.empty() && pxMesh->GetNumBones() > 0)
				{
					Zenith_Log("[ModelComponent] Recreating animation from: %s", strAnimPath.c_str());
					pxMesh->m_pxAnimation = new Flux_MeshAnimation(strAnimPath, *pxMesh);
				}
			}
			else
			{
				Zenith_Log("[ModelComponent] Failed to load mesh from path: %s", strMeshPath.c_str());
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