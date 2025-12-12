#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

class Zenith_ModelComponent
{
public:

	struct MeshEntry
	{
		Flux_MeshGeometry* m_pxGeometry;
		Flux_Material* m_pxMaterial;
	};

	Zenith_ModelComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent()
	{
		// CRITICAL: Do NOT delete assets if we're loading a scene!
		// When LoadFromFile calls Reset(), it destroys all existing components.
		// If we delete assets here, they won't be available when deserializing
		// the new components that reference the same assets.
		//
		// Forward declaration used here to avoid circular dependency with Zenith_Scene.h
		// Implementation is in Zenith_ModelComponent.cpp where we can include Zenith_Scene.h
		extern bool Zenith_ModelComponent_ShouldDeleteAssets();

		if (Zenith_ModelComponent_ShouldDeleteAssets())
		{
			// Clean up any textures and materials that were created by LoadMeshesFromDir
			for (uint32_t u = 0; u < m_xCreatedTextures.GetSize(); u++)
			{
				Zenith_AssetHandler::DeleteTexture(m_xCreatedTextures.Get(u));
			}
			for (uint32_t u = 0; u < m_xCreatedMaterials.GetSize(); u++)
			{
				Zenith_AssetHandler::DeleteMaterial(m_xCreatedMaterials.Get(u));
			}
			for (uint32_t u = 0; u < m_xCreatedMeshes.GetSize(); u++)
			{
				Zenith_AssetHandler::DeleteMesh(m_xCreatedMeshes.Get(u));
			}
		}

		// Clean up physics mesh if it was generated
		ClearPhysicsMesh();
	}

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//#TO not the cleanest code in the world
	//takes a filename in the form meshname_texturetype_materialindex (no extension)
	//and returns materialindex, for example Assets/Meshes/foo_bar_5 would return 5
	static uint32_t GetMaterialIndexFromTextureName(const std::string& strFilename)
	{
		std::string strFileCopy(strFilename);
		const uint32_t uLength = strFileCopy.size();
		char* szFileCopy = new char[uLength+1];
		strncpy(szFileCopy, strFileCopy.c_str(), uLength);
		szFileCopy[uLength] = '\0';

		std::string strTruncated(szFileCopy);
		size_t ulUnderscorePos = strTruncated.find("_");
		Zenith_Assert(ulUnderscorePos != std::string::npos, "Should have found an underscore");
		while (ulUnderscorePos != std::string::npos)
		{
			strTruncated = strTruncated.substr(ulUnderscorePos + 1, strTruncated.size());
			ulUnderscorePos = strTruncated.find("_");
		}

		delete[] szFileCopy;
		return std::stoi(strTruncated.c_str());
	}

	//#TO does a similar thing to above, returns N from a filename in the format meshname_Mesh?_MatN
	static uint32_t GetMaterialIndexFromMeshName(const std::string& strFilename)
	{
		std::string strSubstr = strFilename.substr(strFilename.find("Mat") + 3);
		const uint32_t uLength = strSubstr.size();
		char* szFileCopy = new char[uLength+1];
		strncpy(szFileCopy, strSubstr.c_str(), uLength);
		szFileCopy[uLength] = '\0';

		uint32_t uRet = std::atoi(szFileCopy);
		delete[] szFileCopy;
		return uRet;
	}

	void LoadMeshesFromDir(const std::filesystem::path& strPath, Flux_Material* const pxOverrideMaterial = nullptr, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true)
	{
		static u_int ls_uCount = 0;
		ls_uCount++;
		const std::string strLeaf = strPath.stem().string();

		// If physics mesh auto-generation is enabled, ensure position data is retained
		if (g_xPhysicsMeshConfig.m_bAutoGenerate)
		{
			uRetainAttributeBits |= (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
		}

		//#TO iterate over textures first to create materials
		if (!pxOverrideMaterial)
		{
			for (auto& xFile : std::filesystem::directory_iterator(strPath))
			{
				if (xFile.path().extension() == ".ztx")
				{
					const std::string strFilepath = xFile.path().string();
					const std::string strFilename = xFile.path().stem().string();
					Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile(strFilepath.c_str());
					Flux_Texture xTexture = Zenith_AssetHandler::AddTexture(strFilename, xTexData);
					xTexData.FreeAllocatedData();
					
					m_xCreatedTextures.PushBack(strFilename);
					
					const uint32_t uMatIndex = GetMaterialIndexFromTextureName(strFilename);
					const std::string strMatName = strLeaf + std::to_string(uMatIndex) + std::to_string(ls_uCount);
					if (!Zenith_AssetHandler::MaterialExists(strMatName))
					{
						Zenith_AssetHandler::AddMaterial(strMatName);
						m_xCreatedMaterials.PushBack(strMatName);
					}
					Flux_Material& xMat = Zenith_AssetHandler::GetMaterial(strMatName);

					if (strFilename.find("Diffuse") != std::string::npos)
					{
						xMat.SetDiffuse(xTexture);
					}
					else if (strFilename.find("Specular") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Ambient") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Emissive") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Height") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Normals") != std::string::npos)
					{
						xMat.SetNormal(xTexture);
					}
					else if (strFilename.find("Shininess") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Opacity") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Displacement") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Lightmap") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Reflection") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("BaseColor") != std::string::npos)
					{
						xMat.SetDiffuse(xTexture);
					}
					else if (strFilename.find("Normal_Camera") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Emissive") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Metallic") != std::string::npos)
					{
						xMat.SetRoughnessMetallic(xTexture);
					}
					else if (strFilename.find("Roughness") != std::string::npos)
					{
						xMat.SetRoughnessMetallic(xTexture);
					}
					else if (strFilename.find("Occlusion") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
				}
			}
		}

		//#TO then iterate over meshes
		for (auto& xFile : std::filesystem::directory_iterator(strPath))
		{
			if (xFile.path().extension() == ".zmsh")
			{
				if (!Zenith_AssetHandler::MeshExists(xFile.path().stem().string()))
				{
					Zenith_AssetHandler::AddMesh(xFile.path().stem().string(), xFile.path().string().c_str(), uRetainAttributeBits, bUploadToGPU);
					m_xCreatedMeshes.PushBack(xFile.path().stem().string());
				}
				const uint32_t uMatIndex = GetMaterialIndexFromMeshName(xFile.path().stem().string());
				const std::string strMatName = strLeaf + std::to_string(uMatIndex) + std::to_string(ls_uCount);
				if (pxOverrideMaterial)
				{
					AddMeshEntry(Zenith_AssetHandler::GetMesh(xFile.path().stem().string()), *pxOverrideMaterial);
				}
				else
				{
					Flux_Material& xMat = Zenith_AssetHandler::TryGetMaterial(strMatName);
					
					// Set the base color from the mesh's material color
					Flux_MeshGeometry& xMeshGeometry = Zenith_AssetHandler::GetMesh(xFile.path().stem().string());
					xMat.SetBaseColor(xMeshGeometry.m_xMaterialColor);
					
					AddMeshEntry(xMeshGeometry, xMat);
				}
			}
		}

		// Generate physics mesh from loaded meshes if auto-generation is enabled
		if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_xMeshEntries.GetSize() > 0)
		{
			GeneratePhysicsMesh();
		}
	}

	void AddMeshEntry(Flux_MeshGeometry& xGeometry, Flux_Material& xMaterial) { m_xMeshEntries.PushBack({ &xGeometry, &xMaterial }); }

	Flux_MeshGeometry& GetMeshGeometryAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxGeometry; }
	const Flux_Material& GetMaterialAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }
	Flux_Material& GetMaterialAtIndex(const uint32_t uIndex) { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }

	const uint32_t GetNumMeshEntries() const { return m_xMeshEntries.GetSize(); }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	// Physics mesh generation and access
	void GeneratePhysicsMesh(PhysicsMeshQuality eQuality = PHYSICS_MESH_QUALITY_MEDIUM);
	void GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig);
	Flux_MeshGeometry* GetPhysicsMesh() const { return m_pxPhysicsMesh; }
	bool HasPhysicsMesh() const { return m_pxPhysicsMesh != nullptr; }
	void ClearPhysicsMesh();

	// Debug drawing control
	void SetDebugDrawPhysicsMesh(bool bEnable) { m_bDebugDrawPhysicsMesh = bEnable; }
	bool GetDebugDrawPhysicsMesh() const { return m_bDebugDrawPhysicsMesh; }
	void SetDebugDrawColor(const Zenith_Maths::Vector3& xColor) { m_xDebugDrawColor = xColor; }
	const Zenith_Maths::Vector3& GetDebugDrawColor() const { return m_xDebugDrawColor; }

	// Call this to render debug physics mesh visualization (call each frame when enabled)
	void DebugDrawPhysicsMesh();

#ifdef ZENITH_VULKAN
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Draw Physics Mesh", &m_bDebugDrawPhysicsMesh);
		}
	}
#endif

//private:
	Zenith_Entity m_xParentEntity;

	Zenith_Vector<MeshEntry> m_xMeshEntries;
	Flux_MeshGeometry* m_pxPhysicsMesh = nullptr;
	
	// Debug draw settings
	bool m_bDebugDrawPhysicsMesh = true;
	Zenith_Maths::Vector3 m_xDebugDrawColor = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Green
	
	// Track assets created by LoadMeshesFromDir so we can delete them in the destructor
	Zenith_Vector<std::string> m_xCreatedTextures;
	Zenith_Vector<std::string> m_xCreatedMaterials;
	Zenith_Vector<std::string> m_xCreatedMeshes;

public:
#if defined(ZENITH_TOOLS) && defined(ZENITH_VULKAN)
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_ModelComponent>("Model");
	}
#endif
};
