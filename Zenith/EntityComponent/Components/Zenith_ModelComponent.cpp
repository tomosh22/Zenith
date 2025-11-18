#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"

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

	// m_xParentEntity will be set by the entity deserialization system
}