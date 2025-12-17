#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"

// Log tag for model component physics mesh operations
static constexpr const char* LOG_TAG_MODEL_PHYSICS = "[ModelPhysics]";

// Serialization version for ModelComponent
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 2;

// Helper function to check if we should delete assets in the destructor
// Always returns true - assets should be properly cleaned up and recreated fresh on scene reload
bool Zenith_ModelComponent_ShouldDeleteAssets()
{
	return true;
}

void Zenith_ModelComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write serialization version
	xStream << MODEL_COMPONENT_SERIALIZE_VERSION;
	
	// Write the number of mesh entries
	u_int uNumEntries = m_xMeshEntries.GetSize();
	xStream << uNumEntries;

	// Write each mesh entry using source paths for meshes AND textures
	for (u_int u = 0; u < uNumEntries; u++)
	{
		const MeshEntry& xEntry = m_xMeshEntries.Get(u);

		// Get mesh source path (set when loaded from file via AddMeshFromFile)
		std::string strMeshPath = xEntry.m_pxGeometry ? xEntry.m_pxGeometry->m_strSourcePath : "";
		xStream << strMeshPath;

		// Serialize the entire material (Flux_MaterialAsset has WriteToDataStream)
		if (xEntry.m_pxMaterial)
		{
			xEntry.m_pxMaterial->WriteToDataStream(xStream);
		}
		else
		{
			// Write empty material - create a temporary one
			Flux_MaterialAsset* pxEmptyMat = Flux_MaterialAsset::Create("Empty");
			pxEmptyMat->WriteToDataStream(xStream);
			delete pxEmptyMat;
		}

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

	// Read serialization version
	uint32_t uVersion;
	xStream >> uVersion;
	
	// Read the number of mesh entries
	u_int uNumEntries;
	xStream >> uNumEntries;

	// Read and reconstruct each mesh entry
	for (u_int u = 0; u < uNumEntries; u++)
	{
		std::string strMeshPath;
		xStream >> strMeshPath;

		// Version 2+: Read full material with texture paths
		if (uVersion >= 2)
		{
			// Load mesh from source path
			if (!strMeshPath.empty())
			{
				// CRITICAL: Retain position data for physics mesh generation
				u_int uRetainFlags = (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
				Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(strMeshPath.c_str(), uRetainFlags, true);
				if (pxMesh)
				{
					m_xCreatedMeshes.PushBack(pxMesh);

					// Create a new material with descriptive name including entity and mesh info
					std::string strEntityName = m_xParentEntity.m_strName.empty() ?
						("Entity_" + std::to_string(m_xParentEntity.GetEntityID())) : m_xParentEntity.m_strName;
					std::filesystem::path xMeshPath(strMeshPath);
					std::string strMatName = strEntityName + "_Model_" + xMeshPath.stem().string();
					Flux_MaterialAsset* pxMaterial = Flux_MaterialAsset::Create(strMatName);
					if (pxMaterial)
					{
						m_xCreatedMaterials.PushBack(pxMaterial);
						pxMaterial->ReadFromDataStream(xStream);  // This loads textures from paths
						AddMeshEntry(*pxMesh, *pxMaterial);
					}
					else
					{
						Zenith_Log("[ModelComponent] Failed to create material during deserialization");
						// Still need to read material data to advance stream
						Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
						pxTempMat->ReadFromDataStream(xStream);
						delete pxTempMat;
						AddMeshEntry(*pxMesh, *Flux_Graphics::s_pxBlankMaterial);
					}
				}
				else
				{
					Zenith_Log("[ModelComponent] Failed to load mesh from path: %s", strMeshPath.c_str());
					// Still need to read material data to advance stream
					Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
					pxTempMat->ReadFromDataStream(xStream);
					delete pxTempMat;
				}
			}
			else
			{
				// Empty mesh path, still need to read material data
				Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
				pxTempMat->ReadFromDataStream(xStream);
				delete pxTempMat;
			}
		}
		else
		{
			// Version 1: Legacy format with only base color
			Zenith_Maths::Vector4 xBaseColor;
			xStream >> xBaseColor.x;
			xStream >> xBaseColor.y;
			xStream >> xBaseColor.z;
			xStream >> xBaseColor.w;

			// Load mesh from source path
			if (!strMeshPath.empty())
			{
				// CRITICAL: Retain position data for physics mesh generation
				u_int uRetainFlags = (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
				Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(strMeshPath.c_str(), uRetainFlags, true);
				if (pxMesh)
				{
					m_xCreatedMeshes.PushBack(pxMesh);

					// Create a new material with descriptive name including entity and mesh info
					std::string strEntityName = m_xParentEntity.m_strName.empty() ?
						("Entity_" + std::to_string(m_xParentEntity.GetEntityID())) : m_xParentEntity.m_strName;
					std::filesystem::path xMeshPath(strMeshPath);
					std::string strMatName = strEntityName + "_Model_" + xMeshPath.stem().string() + "_Legacy";
					Flux_MaterialAsset* pxMaterial = Flux_MaterialAsset::Create(strMatName);
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
				}
				else
				{
					Zenith_Log("[ModelComponent] Failed to load mesh from path: %s", strMeshPath.c_str());
				}
			}
		}

		// Read animation path (common to all versions)
		std::string strAnimPath;
		xStream >> strAnimPath;

		// Recreate animation if path was serialized (and mesh was loaded successfully)
		if (!strAnimPath.empty() && m_xMeshEntries.GetSize() > 0)
		{
			MeshEntry& xEntry = m_xMeshEntries.Get(m_xMeshEntries.GetSize() - 1);
			if (xEntry.m_pxGeometry && xEntry.m_pxGeometry->GetNumBones() > 0)
			{
				Zenith_Log("[ModelComponent] Recreating animation from: %s", strAnimPath.c_str());
				xEntry.m_pxGeometry->m_pxAnimation = new Flux_MeshAnimation(strAnimPath, *xEntry.m_pxGeometry);
			}
		}
	}

	// Generate physics mesh after deserializing if auto-generation is enabled
	if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_xMeshEntries.GetSize() > 0)
	{
		Zenith_Log("%s Auto-generating physics mesh for deserialized ModelComponent (entity: %s, meshes: %u)",
			LOG_TAG_MODEL_PHYSICS, m_xParentEntity.m_strName.c_str(), m_xMeshEntries.GetSize());
		GeneratePhysicsMesh();

		if (m_pxPhysicsMesh)
		{
			Zenith_Log("%s Physics mesh generated successfully: %u verts, %u tris",
				LOG_TAG_MODEL_PHYSICS, m_pxPhysicsMesh->GetNumVerts(), m_pxPhysicsMesh->GetNumIndices() / 3);
		}
		else
		{
			Zenith_Log("%s WARNING: Physics mesh generation failed!", LOG_TAG_MODEL_PHYSICS);
		}
	}
	else
	{
		if (!g_xPhysicsMeshConfig.m_bAutoGenerate)
		{
			Zenith_Log("%s Physics mesh auto-generation is DISABLED", LOG_TAG_MODEL_PHYSICS);
		}
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

#ifdef ZENITH_TOOLS
void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Draw Physics Mesh", &m_bDebugDrawPhysicsMesh);

		ImGui::Separator();

		// Load meshes from directory
		if (ImGui::TreeNode("Load Mesh"))
		{
			static char s_szMeshDirPath[512] = "";
			ImGui::InputText("Directory Path", s_szMeshDirPath, sizeof(s_szMeshDirPath));
			ImGui::SameLine();
			if (ImGui::Button("Browse..."))
			{
				// Use Windows folder dialog
#ifdef _WIN32
				BROWSEINFOA bi = { 0 };
				bi.lpszTitle = "Select Mesh Directory";
				bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
				LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
				if (pidl != nullptr)
				{
					char szPath[MAX_PATH];
					if (SHGetPathFromIDListA(pidl, szPath))
					{
						strncpy(s_szMeshDirPath, szPath, sizeof(s_szMeshDirPath) - 1);
					}
					CoTaskMemFree(pidl);
				}
#endif
			}

			if (ImGui::Button("Load Meshes from Directory"))
			{
				if (strlen(s_szMeshDirPath) > 0)
				{
					// Clear existing mesh entries
					m_xMeshEntries.Clear();
					// Clear created assets tracking
					for (uint32_t u = 0; u < m_xCreatedTextures.GetSize(); u++)
					{
						Zenith_AssetHandler::DeleteTexture(m_xCreatedTextures.Get(u));
					}
					m_xCreatedTextures.Clear();
					for (uint32_t u = 0; u < m_xCreatedMaterials.GetSize(); u++)
					{
						delete m_xCreatedMaterials.Get(u);
					}
					m_xCreatedMaterials.Clear();
					for (uint32_t u = 0; u < m_xCreatedMeshes.GetSize(); u++)
					{
						Zenith_AssetHandler::DeleteMesh(m_xCreatedMeshes.Get(u));
					}
					m_xCreatedMeshes.Clear();

					LoadMeshesFromDir(s_szMeshDirPath);
					Zenith_Log("[ModelComponent] Loaded meshes from: %s", s_szMeshDirPath);
				}
			}

			ImGui::TreePop();
		}

		// Animation loading section
		if (GetNumMeshEntries() > 0 && ImGui::TreeNode("Animations"))
		{
			static char s_szAnimFilePath[512] = "";
			ImGui::InputText("Animation File (.fbx/.gltf)", s_szAnimFilePath, sizeof(s_szAnimFilePath));

			static int s_iTargetMeshIndex = 0;
			int iMaxIndex = static_cast<int>(GetNumMeshEntries()) - 1;
			ImGui::SliderInt("Target Mesh Index", &s_iTargetMeshIndex, 0, iMaxIndex);

			if (ImGui::Button("Load Animation"))
			{
				if (strlen(s_szAnimFilePath) > 0 && s_iTargetMeshIndex >= 0 && s_iTargetMeshIndex < static_cast<int>(GetNumMeshEntries()))
				{
					Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(s_iTargetMeshIndex);
					if (xMesh.GetNumBones() > 0)
					{
						if (xMesh.m_pxAnimation)
						{
							delete xMesh.m_pxAnimation;
						}
						xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
						Zenith_Log("[ModelComponent] Loaded animation from: %s for mesh %d", s_szAnimFilePath, s_iTargetMeshIndex);
					}
					else
					{
						Zenith_Log("[ModelComponent] Cannot load animation: mesh %d has no bones", s_iTargetMeshIndex);
					}
				}
			}

			// Load animation for all meshes with bones
			if (ImGui::Button("Load Animation for All Meshes"))
			{
				if (strlen(s_szAnimFilePath) > 0)
				{
					for (uint32_t u = 0; u < GetNumMeshEntries(); u++)
					{
						Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
						if (xMesh.GetNumBones() > 0)
						{
							if (xMesh.m_pxAnimation)
							{
								delete xMesh.m_pxAnimation;
							}
							xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
							Zenith_Log("[ModelComponent] Loaded animation for mesh %u", u);
						}
					}
				}
			}

			// Display current animation status per mesh
			ImGui::Separator();
			for (uint32_t u = 0; u < GetNumMeshEntries(); u++)
			{
				Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
				if (xMesh.m_pxAnimation)
				{
					ImGui::Text("Mesh %u: Animation loaded (%s)", u, xMesh.m_pxAnimation->GetSourcePath().c_str());
				}
				else if (xMesh.GetNumBones() > 0)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Mesh %u: Has %u bones, no animation", u, xMesh.GetNumBones());
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::Text("Mesh Entries: %u", GetNumMeshEntries());

		// Display each mesh entry with its material
		for (uint32_t uMeshIdx = 0; uMeshIdx < GetNumMeshEntries(); ++uMeshIdx)
		{
			ImGui::PushID(uMeshIdx);

			Flux_MaterialAsset& xMaterial = GetMaterialAtIndex(uMeshIdx);

			if (ImGui::TreeNode("MeshEntry", "Mesh Entry %u", uMeshIdx))
			{
				// Display mesh source path
				Flux_MeshGeometry& xGeom = GetMeshGeometryAtIndex(uMeshIdx);
				if (!xGeom.m_strSourcePath.empty())
				{
					ImGui::TextWrapped("Source: %s", xGeom.m_strSourcePath.c_str());
				}

				// Material texture slots - pass mesh index for material instancing
				RenderTextureSlot("Diffuse", xMaterial, uMeshIdx, TEXTURE_SLOT_DIFFUSE);
				RenderTextureSlot("Normal", xMaterial, uMeshIdx, TEXTURE_SLOT_NORMAL);
				RenderTextureSlot("Roughness/Metallic", xMaterial, uMeshIdx, TEXTURE_SLOT_ROUGHNESS_METALLIC);
				RenderTextureSlot("Occlusion", xMaterial, uMeshIdx, TEXTURE_SLOT_OCCLUSION);
				RenderTextureSlot("Emissive", xMaterial, uMeshIdx, TEXTURE_SLOT_EMISSIVE);

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}
}
#endif
