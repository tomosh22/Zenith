#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "Editor/Zenith_Editor.h"
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "Shell32.lib")
#endif
#endif

class Zenith_ModelComponent
{
public:

	struct MeshEntry
	{
		Flux_MeshGeometry* m_pxGeometry;
		Flux_MaterialAsset* m_pxMaterial;
	};

	Zenith_ModelComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent()
	{
		// Always clean up assets we created - they will be recreated fresh on scene reload
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
				// NOTE: Flux_MaterialAsset manages its own lifecycle
				// Materials will be unloaded when the registry is cleared
				// We just need to delete the material instance itself
				delete m_xCreatedMaterials.Get(u);
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

	void LoadMeshesFromDir(const std::filesystem::path& strPath, Flux_MaterialAsset* const pxOverrideMaterial = nullptr, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true)
	{
		static u_int ls_uCount = 0;
		ls_uCount++;
		const std::string strLeaf = strPath.stem().string();

		// If physics mesh auto-generation is enabled, ensure position data is retained
		if (g_xPhysicsMeshConfig.m_bAutoGenerate)
		{
			uRetainAttributeBits |= (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
		}

		// Track materials created in this call (indexed by material index from filenames)
		std::unordered_map<uint32_t, Flux_MaterialAsset*> xMaterialMap;
		
		// Track texture paths per material index for serialization
		std::unordered_map<uint32_t, std::string> xDiffusePathMap;
		std::unordered_map<uint32_t, std::string> xNormalPathMap;
		std::unordered_map<uint32_t, std::string> xRoughnessMetallicPathMap;

		//#TO iterate over textures first to create materials
		if (!pxOverrideMaterial)
		{
			for (auto& xFile : std::filesystem::directory_iterator(strPath))
			{
				if (xFile.path().extension() == ZENITH_TEXTURE_EXT)
				{
					const std::string strFilepath = xFile.path().string();
					const std::string strFilename = xFile.path().stem().string();
					Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile(strFilepath.c_str());
					Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
					xTexData.FreeAllocatedData();

					if (!pxTexture)
					{
						Zenith_Log("[ModelComponent] Failed to load texture: %s", strFilepath.c_str());
						continue;
					}
					
					// Store source path in texture for reference
					pxTexture->m_strSourcePath = strFilepath;

					m_xCreatedTextures.PushBack(pxTexture);

					const uint32_t uMatIndex = GetMaterialIndexFromTextureName(strFilename);

					// Create material if not already created for this index
					if (xMaterialMap.find(uMatIndex) == xMaterialMap.end())
					{
						Flux_MaterialAsset* pxMat = Flux_MaterialAsset::Create("Material_" + std::to_string(uMatIndex));
						if (pxMat)
						{
							xMaterialMap[uMatIndex] = pxMat;
							m_xCreatedMaterials.PushBack(pxMat);
						}
					}

					Flux_MaterialAsset* pxMat = xMaterialMap[uMatIndex];
					if (!pxMat) continue;

					// Set texture paths (Flux_MaterialAsset loads textures on demand)
					if (strFilename.find("Diffuse") != std::string::npos)
					{
						pxMat->SetDiffuseTexturePath(strFilepath);
						xDiffusePathMap[uMatIndex] = strFilepath;
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
						pxMat->SetEmissiveTexturePath(strFilepath);
					}
					else if (strFilename.find("Height") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Normals") != std::string::npos)
					{
						pxMat->SetNormalTexturePath(strFilepath);
						xNormalPathMap[uMatIndex] = strFilepath;
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
						pxMat->SetDiffuseTexturePath(strFilepath);
						xDiffusePathMap[uMatIndex] = strFilepath;
					}
					else if (strFilename.find("Normal_Camera") != std::string::npos)
					{
						Zenith_Assert(false, "Unhandled texture type");
					}
					else if (strFilename.find("Metallic") != std::string::npos)
					{
						pxMat->SetRoughnessMetallicTexturePath(strFilepath);
						xRoughnessMetallicPathMap[uMatIndex] = strFilepath;
					}
					else if (strFilename.find("Roughness") != std::string::npos)
					{
						pxMat->SetRoughnessMetallicTexturePath(strFilepath);
						xRoughnessMetallicPathMap[uMatIndex] = strFilepath;
					}
					else if (strFilename.find("Occlusion") != std::string::npos)
					{
						pxMat->SetOcclusionTexturePath(strFilepath);
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
				Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(
					xFile.path().string().c_str(), uRetainAttributeBits, bUploadToGPU);

				if (!pxMesh)
				{
					Zenith_Log("[ModelComponent] Failed to load mesh: %s", xFile.path().string().c_str());
					continue;
				}

				m_xCreatedMeshes.PushBack(pxMesh);

				const uint32_t uMatIndex = GetMaterialIndexFromMeshName(xFile.path().stem().string());

				if (pxOverrideMaterial)
				{
					AddMeshEntry(*pxMesh, *pxOverrideMaterial);
				}
				else
				{
					// Find material by index
					auto it = xMaterialMap.find(uMatIndex);
					if (it != xMaterialMap.end() && it->second)
					{
						Flux_MaterialAsset* pxMat = it->second;
						// Set the base color from the mesh's material color
						pxMat->SetBaseColor(pxMesh->m_xMaterialColor);
						AddMeshEntry(*pxMesh, *pxMat);
					}
					else
					{
						// No material found, use blank material
						AddMeshEntry(*pxMesh, *Flux_Graphics::s_pxBlankMaterial);
					}
				}
			}
		}

		// Generate physics mesh from loaded meshes if auto-generation is enabled
		if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_xMeshEntries.GetSize() > 0)
		{
			GeneratePhysicsMesh();
		}
	}

	void AddMeshEntry(Flux_MeshGeometry& xGeometry, Flux_MaterialAsset& xMaterial) { m_xMeshEntries.PushBack({ &xGeometry, &xMaterial }); }

	Flux_MeshGeometry& GetMeshGeometryAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxGeometry; }
	const Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }
	Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex) { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }

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

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------

	// Texture slot identifiers for material editing
	enum TextureSlotType
	{
		TEXTURE_SLOT_DIFFUSE,
		TEXTURE_SLOT_NORMAL,
		TEXTURE_SLOT_ROUGHNESS_METALLIC,
		TEXTURE_SLOT_OCCLUSION,
		TEXTURE_SLOT_EMISSIVE
	};

	void RenderPropertiesPanel();

private:
	// Helper to render a single texture slot with drag-drop target
	void RenderTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, uint32_t uMeshIdx, TextureSlotType eSlot)
	{
		ImGui::PushID(szLabel);

		// Get current texture (if any) and its path
		const Flux_Texture* pxCurrentTexture = nullptr;
		std::string strCurrentPath;
		switch (eSlot)
		{
		case TEXTURE_SLOT_DIFFUSE:
			pxCurrentTexture = xMaterial.GetDiffuseTexture();
			strCurrentPath = xMaterial.GetDiffuseTexturePath();
			break;
		case TEXTURE_SLOT_NORMAL:
			pxCurrentTexture = xMaterial.GetNormalTexture();
			strCurrentPath = xMaterial.GetNormalTexturePath();
			break;
		case TEXTURE_SLOT_ROUGHNESS_METALLIC:
			pxCurrentTexture = xMaterial.GetRoughnessMetallicTexture();
			strCurrentPath = xMaterial.GetRoughnessMetallicTexturePath();
			break;
		case TEXTURE_SLOT_OCCLUSION:
			pxCurrentTexture = xMaterial.GetOcclusionTexture();
			strCurrentPath = xMaterial.GetOcclusionTexturePath();
			break;
		case TEXTURE_SLOT_EMISSIVE:
			pxCurrentTexture = xMaterial.GetEmissiveTexture();
			strCurrentPath = xMaterial.GetEmissiveTexturePath();
			break;
		}

		std::string strTextureName = "(none)";
		if (pxCurrentTexture && pxCurrentTexture->m_xVRAMHandle.IsValid())
		{
			// Show filename from path if available
			if (!strCurrentPath.empty())
			{
				std::filesystem::path xPath(strCurrentPath);
				strTextureName = xPath.filename().string();
			}
			else
			{
				strTextureName = "(loaded)";
			}
		}

		// Display slot label and current texture
		ImGui::Text("%s:", szLabel);
		ImGui::SameLine();

		// Drop zone button
		ImVec2 xButtonSize(150, 20);
		ImGui::Button(strTextureName.c_str(), xButtonSize);

		// Visual feedback when hovering with payload
		if (ImGui::BeginDragDropTarget())
		{
			// Accept texture payloads
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);

				Zenith_Log("[ModelComponent] Texture dropped on %s: %s",
					szLabel, pFilePayload->m_szFilePath);

				// Load and assign texture - creates new material instance to avoid modifying shared materials
				AssignTextureToSlot(pFilePayload->m_szFilePath, uMeshIdx, eSlot);
			}

			ImGui::EndDragDropTarget();
		}

		// Tooltip showing full path
		if (ImGui::IsItemHovered())
		{
			if (!strCurrentPath.empty())
			{
				ImGui::SetTooltip("Drop a .ztxtr texture here\nPath: %s", strCurrentPath.c_str());
			}
			else
			{
				ImGui::SetTooltip("Drop a .ztxtr texture here\nCurrent: %s", strTextureName.c_str());
			}
		}

		ImGui::PopID();
	}

	// Helper to load texture and assign to material slot
	// Note: This creates a new material instance for this mesh entry to avoid modifying shared materials
	void AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, TextureSlotType eSlot)
	{
		// Load texture from file
		Zenith_AssetHandler::TextureData xTexData =
			Zenith_AssetHandler::LoadTexture2DFromFile(szFilePath);
		Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
		xTexData.FreeAllocatedData();

		if (!pxTexture)
		{
			Zenith_Log("[ModelComponent] Failed to load texture: %s", szFilePath);
			return;
		}
		
		// Store the source path in the texture
		pxTexture->m_strSourcePath = szFilePath;

		m_xCreatedTextures.PushBack(pxTexture);
		Zenith_Log("[ModelComponent] Loaded texture from: %s", szFilePath);

		// Get current material to copy its properties
		Flux_MaterialAsset* pxOldMaterial = m_xMeshEntries.Get(uMeshIdx).m_pxMaterial;

		// Create new material instance
		Flux_MaterialAsset* pxNewMaterial = Flux_MaterialAsset::Create("Material_" + std::to_string(uMeshIdx));
		if (!pxNewMaterial)
		{
			Zenith_Log("[ModelComponent] Failed to create new material instance");
			return;
		}

		m_xCreatedMaterials.PushBack(pxNewMaterial);
		Zenith_Log("[ModelComponent] Created new material instance");

		// Copy existing texture paths from old material to new material
		if (pxOldMaterial)
		{
			if (!pxOldMaterial->GetDiffuseTexturePath().empty())
				pxNewMaterial->SetDiffuseTexturePath(pxOldMaterial->GetDiffuseTexturePath());
			if (!pxOldMaterial->GetNormalTexturePath().empty())
				pxNewMaterial->SetNormalTexturePath(pxOldMaterial->GetNormalTexturePath());
			if (!pxOldMaterial->GetRoughnessMetallicTexturePath().empty())
				pxNewMaterial->SetRoughnessMetallicTexturePath(pxOldMaterial->GetRoughnessMetallicTexturePath());
			if (!pxOldMaterial->GetOcclusionTexturePath().empty())
				pxNewMaterial->SetOcclusionTexturePath(pxOldMaterial->GetOcclusionTexturePath());
			if (!pxOldMaterial->GetEmissiveTexturePath().empty())
				pxNewMaterial->SetEmissiveTexturePath(pxOldMaterial->GetEmissiveTexturePath());

			pxNewMaterial->SetBaseColor(pxOldMaterial->GetBaseColor());
		}

		// Now set the new texture on the appropriate slot WITH the path
		std::string strPath(szFilePath);
		switch (eSlot)
		{
		case TEXTURE_SLOT_DIFFUSE:
			pxNewMaterial->SetDiffuseTexturePath(strPath);
			Zenith_Log("[ModelComponent] Set diffuse texture");
			break;
		case TEXTURE_SLOT_NORMAL:
			pxNewMaterial->SetNormalTexturePath(strPath);
			Zenith_Log("[ModelComponent] Set normal texture");
			break;
		case TEXTURE_SLOT_ROUGHNESS_METALLIC:
			pxNewMaterial->SetRoughnessMetallicTexturePath(strPath);
			Zenith_Log("[ModelComponent] Set roughness/metallic texture");
			break;
		case TEXTURE_SLOT_OCCLUSION:
			pxNewMaterial->SetOcclusionTexturePath(strPath);
			Zenith_Log("[ModelComponent] Set occlusion texture");
			break;
		case TEXTURE_SLOT_EMISSIVE:
			pxNewMaterial->SetEmissiveTexturePath(strPath);
			Zenith_Log("[ModelComponent] Set emissive texture");
			break;
		}

		// Update the mesh entry to use the new material
		m_xMeshEntries.Get(uMeshIdx).m_pxMaterial = pxNewMaterial;
	}
public:
#endif

//private:
	Zenith_Entity m_xParentEntity;

	Zenith_Vector<MeshEntry> m_xMeshEntries;
	Flux_MeshGeometry* m_pxPhysicsMesh = nullptr;

	// Debug draw settings
	bool m_bDebugDrawPhysicsMesh = true;
	Zenith_Maths::Vector3 m_xDebugDrawColor = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Green

	// Track assets created by LoadMeshesFromDir so we can delete them in the destructor
	// Uses raw pointers for direct lifecycle management
	Zenith_Vector<Flux_Texture*> m_xCreatedTextures;
	Zenith_Vector<Flux_MaterialAsset*> m_xCreatedMaterials;
	Zenith_Vector<Flux_MeshGeometry*> m_xCreatedMeshes;

public:
#if defined(ZENITH_TOOLS) && defined(ZENITH_VULKAN)
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_ModelComponent>("Model");
	}
#endif
};
