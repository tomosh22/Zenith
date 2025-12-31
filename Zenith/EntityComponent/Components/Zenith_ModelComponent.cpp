#include "Zenith.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Core/Zenith_Core.h"

// Log tag for model component operations
static constexpr const char* LOG_TAG_MODEL = "[ModelComponent]";
static constexpr const char* LOG_TAG_MODEL_PHYSICS = "[ModelPhysics]";

// Serialization version for ModelComponent
// Version 3: New model instance system with .zmodel path
static constexpr uint32_t MODEL_COMPONENT_SERIALIZE_VERSION = 3;

// Helper function to check if we should delete assets in the destructor
// Always returns true - assets should be properly cleaned up and recreated fresh on scene reload
bool Zenith_ModelComponent_ShouldDeleteAssets()
{
	return true;
}

//=============================================================================
// Destructor
//=============================================================================
Zenith_ModelComponent::~Zenith_ModelComponent()
{
	// Clean up new model instance system
	ClearModel();

	// Clean up legacy system assets
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

//=============================================================================
// New Model Instance API
//=============================================================================

void Zenith_ModelComponent::LoadModel(const std::string& strPath)
{
	Zenith_Log("%s LoadModel called with path: %s", LOG_TAG_MODEL, strPath.c_str());

	// Clear any existing model
	ClearModel();

	// Load model asset from file
	Zenith_ModelAsset* pxAsset = Zenith_ModelAsset::LoadFromFile(strPath.c_str());
	if (!pxAsset)
	{
		Zenith_Log("%s ERROR: Failed to load model asset from: %s", LOG_TAG_MODEL, strPath.c_str());
		return;
	}

	Zenith_Log("%s Model asset loaded: %s (meshes: %u, has skeleton: %s)",
		LOG_TAG_MODEL, pxAsset->GetName().c_str(), pxAsset->GetNumMeshes(),
		pxAsset->HasSkeleton() ? "yes" : "no");

	// Create model instance from asset
	m_pxModelInstance = Flux_ModelInstance::CreateFromAsset(pxAsset);
	if (!m_pxModelInstance)
	{
		Zenith_Log("%s ERROR: Failed to create model instance from asset: %s", LOG_TAG_MODEL, strPath.c_str());
		delete pxAsset;
		return;
	}

	// Store path for serialization
	m_strModelPath = strPath;

	// Detailed logging for debugging
	Zenith_Log("%s SUCCESS: Loaded model from: %s", LOG_TAG_MODEL, strPath.c_str());
	Zenith_Log("%s   Meshes: %u", LOG_TAG_MODEL, m_pxModelInstance->GetNumMeshes());
	Zenith_Log("%s   Materials: %u", LOG_TAG_MODEL, m_pxModelInstance->GetNumMaterials());
	Zenith_Log("%s   Has Skeleton: %s", LOG_TAG_MODEL, m_pxModelInstance->HasSkeleton() ? "yes (animated mesh renderer)" : "no (static mesh renderer)");

	for (uint32_t u = 0; u < m_pxModelInstance->GetNumMeshes(); u++)
	{
		Flux_MeshInstance* pxMesh = m_pxModelInstance->GetMeshInstance(u);
		if (pxMesh)
		{
			Zenith_Log("%s   Mesh %u: %u verts, %u indices", LOG_TAG_MODEL, u, pxMesh->GetNumVerts(), pxMesh->GetNumIndices());
		}
		else
		{
			Zenith_Log("%s   Mesh %u: NULL", LOG_TAG_MODEL, u);
		}
	}

	// Generate physics mesh if auto-generation is enabled
	if (g_xPhysicsMeshConfig.m_bAutoGenerate && m_pxModelInstance->GetNumMeshes() > 0)
	{
		GeneratePhysicsMesh();
	}
}

void Zenith_ModelComponent::ClearModel()
{
	// Delete animation controller (owned by component)
	if (m_pxAnimController)
	{
		delete m_pxAnimController;
		m_pxAnimController = nullptr;
	}

	// Delete model instance (handles cleanup of mesh instances, skeleton instance, etc.)
	if (m_pxModelInstance)
	{
		m_pxModelInstance->Destroy();
		delete m_pxModelInstance;
		m_pxModelInstance = nullptr;
	}

	// Clear path
	m_strModelPath.clear();
}

//=============================================================================
// Rendering Helpers
//=============================================================================

uint32_t Zenith_ModelComponent::GetNumMeshes() const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetNumMeshes();
	}
	// Fall back to legacy system
	return m_xMeshEntries.GetSize();
}

Flux_MeshInstance* Zenith_ModelComponent::GetMeshInstance(uint32_t uIndex) const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetMeshInstance(uIndex);
	}
	// Legacy system doesn't use Flux_MeshInstance
	return nullptr;
}

Flux_MaterialAsset* Zenith_ModelComponent::GetMaterial(uint32_t uIndex) const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetMaterial(uIndex);
	}
	// Fall back to legacy system
	if (uIndex < m_xMeshEntries.GetSize())
	{
		return m_xMeshEntries.Get(uIndex).m_pxMaterial;
	}
	return nullptr;
}

bool Zenith_ModelComponent::HasSkeleton() const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->HasSkeleton();
	}
	return false;
}

Flux_SkeletonInstance* Zenith_ModelComponent::GetSkeletonInstance() const
{
	if (m_pxModelInstance)
	{
		return m_pxModelInstance->GetSkeletonInstance();
	}
	return nullptr;
}

//=============================================================================
// Animation System Integration
//=============================================================================

Flux_AnimationController* Zenith_ModelComponent::GetOrCreateAnimationController()
{
	if (m_pxAnimController)
	{
		return m_pxAnimController;
	}

	// Create new controller at component level
	m_pxAnimController = new Flux_AnimationController();

	if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
	{
		// Initialize with skeleton instance for new model instance system
		Flux_SkeletonInstance* pxSkeleton = m_pxModelInstance->GetSkeletonInstance();
		if (pxSkeleton)
		{
			m_pxAnimController->Initialize(pxSkeleton);
			Zenith_Log("%s Created animation controller for model instance (bones: %u)",
				LOG_TAG_MODEL, pxSkeleton->GetNumBones());
		}
		else
		{
			Zenith_Log("%s Model has skeleton but GetSkeletonInstance() returned null", LOG_TAG_MODEL);
		}
	}
	else if (m_xMeshEntries.GetSize() > 0)
	{
		// Legacy: Initialize with first mesh that has bones
		for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
		{
			Flux_MeshGeometry* pxGeometry = m_xMeshEntries.Get(u).m_pxGeometry;
			if (pxGeometry && pxGeometry->GetNumBones() > 0)
			{
				m_pxAnimController->Initialize(pxGeometry);
				Zenith_Log("%s Created animation controller for legacy mesh (bones: %u)",
					LOG_TAG_MODEL, pxGeometry->GetNumBones());
				break;
			}
		}
	}

	return m_pxAnimController;
}

void Zenith_ModelComponent::Update(float fDt)
{
	// Update world matrices for animation controller
	UpdateAnimationWorldMatrix();

	// Update component-level animation controller
	if (m_pxAnimController)
	{
		m_pxAnimController->Update(fDt);
	}

	// If using new model instance with skeleton, update animation
	if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
	{
		m_pxModelInstance->UpdateAnimation();
	}
}

void Zenith_ModelComponent::PlayAnimation(const std::string& strClipName, float fBlendTime)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->PlayClip(strClipName, fBlendTime);
	}
}

void Zenith_ModelComponent::StopAnimations()
{
	if (m_pxAnimController)
	{
		m_pxAnimController->Stop();
	}
}

void Zenith_ModelComponent::SetAnimationsPaused(bool bPaused)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetPaused(bPaused);
	}
}

bool Zenith_ModelComponent::AreAnimationsPaused() const
{
	if (m_pxAnimController)
	{
		return m_pxAnimController->IsPaused();
	}
	return false;
}

void Zenith_ModelComponent::SetAnimationPlaybackSpeed(float fSpeed)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetPlaybackSpeed(fSpeed);
	}
}

float Zenith_ModelComponent::GetAnimationPlaybackSpeed() const
{
	if (m_pxAnimController)
	{
		return m_pxAnimController->GetPlaybackSpeed();
	}
	return 1.0f;
}

void Zenith_ModelComponent::SetAnimationFloat(const std::string& strName, float fValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetFloat(strName, fValue);
	}
}

void Zenith_ModelComponent::SetAnimationInt(const std::string& strName, int32_t iValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetInt(strName, iValue);
	}
}

void Zenith_ModelComponent::SetAnimationBool(const std::string& strName, bool bValue)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetBool(strName, bValue);
	}
}

void Zenith_ModelComponent::SetAnimationTrigger(const std::string& strName)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetTrigger(strName);
	}
}

void Zenith_ModelComponent::SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPosition, float fWeight)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->SetIKTarget(strChainName, xPosition, fWeight);
	}
}

void Zenith_ModelComponent::ClearIKTarget(const std::string& strChainName)
{
	if (m_pxAnimController)
	{
		m_pxAnimController->ClearIKTarget(strChainName);
	}
}

void Zenith_ModelComponent::UpdateAnimationWorldMatrix()
{
	if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xWorldMatrix;
	xTransform.BuildModelMatrix(xWorldMatrix);

	// Update component-level controller
	if (m_pxAnimController)
	{
		m_pxAnimController->SetWorldMatrix(xWorldMatrix);
	}
}

//=============================================================================
// Serialization
//=============================================================================

void Zenith_ModelComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write serialization version
	xStream << MODEL_COMPONENT_SERIALIZE_VERSION;

	// Check if using new model instance system
	bool bUsingModelInstance = (m_pxModelInstance != nullptr);
	xStream << bUsingModelInstance;

	if (bUsingModelInstance)
	{
		// New system: Write model path
		xStream << m_strModelPath;

		// TODO: Serialize animation controller state if needed
	}
	else
	{
		// Legacy system: Write mesh entries
		u_int uNumEntries = m_xMeshEntries.GetSize();
		xStream << uNumEntries;

		for (u_int u = 0; u < uNumEntries; u++)
		{
			const MeshEntry& xEntry = m_xMeshEntries.Get(u);

			// Get mesh source path
			std::string strMeshPath = xEntry.m_pxGeometry ? xEntry.m_pxGeometry->m_strSourcePath : "";
			xStream << strMeshPath;

			// Serialize the entire material
			if (xEntry.m_pxMaterial)
			{
				xEntry.m_pxMaterial->WriteToDataStream(xStream);
			}
			else
			{
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
	}
}

void Zenith_ModelComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Clear existing data
	ClearModel();
	m_xMeshEntries.Clear();

	// Read serialization version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion >= 3)
	{
		// Version 3+: Check which system is being used
		bool bUsingModelInstance;
		xStream >> bUsingModelInstance;

		if (bUsingModelInstance)
		{
			// New system: Read and load model path
			std::string strModelPath;
			xStream >> strModelPath;

			if (!strModelPath.empty())
			{
				LoadModel(strModelPath);
			}

			// TODO: Deserialize animation controller state if needed
		}
		else
		{
			// Legacy system: Read mesh entries
			ReadLegacyMeshEntries(xStream, uVersion);
		}
	}
	else
	{
		// Versions 1-2: Always legacy format
		ReadLegacyMeshEntries(xStream, uVersion);
	}
}

void Zenith_ModelComponent::ReadLegacyMeshEntries(Zenith_DataStream& xStream, uint32_t uVersion)
{
	u_int uNumEntries;
	xStream >> uNumEntries;

	for (u_int u = 0; u < uNumEntries; u++)
	{
		std::string strMeshPath;
		xStream >> strMeshPath;

		if (uVersion >= 2)
		{
			// Version 2+: Read full material with texture paths
			if (!strMeshPath.empty())
			{
				u_int uRetainFlags = (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
				Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(strMeshPath.c_str(), uRetainFlags, true);
				if (pxMesh)
				{
					m_xCreatedMeshes.PushBack(pxMesh);

					std::string strEntityName = m_xParentEntity.m_strName.empty() ?
						("Entity_" + std::to_string(m_xParentEntity.GetEntityID())) : m_xParentEntity.m_strName;
					std::filesystem::path xMeshPath(strMeshPath);
					std::string strMatName = strEntityName + "_Model_" + xMeshPath.stem().string();
					Flux_MaterialAsset* pxMaterial = Flux_MaterialAsset::Create(strMatName);
					if (pxMaterial)
					{
						m_xCreatedMaterials.PushBack(pxMaterial);
						pxMaterial->ReadFromDataStream(xStream);
						AddMeshEntry(*pxMesh, *pxMaterial);
					}
					else
					{
						Zenith_Log("%s Failed to create material during deserialization", LOG_TAG_MODEL);
						Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
						pxTempMat->ReadFromDataStream(xStream);
						delete pxTempMat;
						AddMeshEntry(*pxMesh, *Flux_Graphics::s_pxBlankMaterial);
					}
				}
				else
				{
					Zenith_Log("%s Failed to load mesh from path: %s", LOG_TAG_MODEL, strMeshPath.c_str());
					Flux_MaterialAsset* pxTempMat = Flux_MaterialAsset::Create("Temp");
					pxTempMat->ReadFromDataStream(xStream);
					delete pxTempMat;
				}
			}
			else
			{
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

			if (!strMeshPath.empty())
			{
				u_int uRetainFlags = (1 << Flux_MeshGeometry::FLUX_VERTEX_ATTRIBUTE__POSITION);
				Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(strMeshPath.c_str(), uRetainFlags, true);
				if (pxMesh)
				{
					m_xCreatedMeshes.PushBack(pxMesh);

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
						Zenith_Log("%s Failed to create material during deserialization", LOG_TAG_MODEL);
						AddMeshEntry(*pxMesh, *Flux_Graphics::s_pxBlankMaterial);
					}
				}
				else
				{
					Zenith_Log("%s Failed to load mesh from path: %s", LOG_TAG_MODEL, strMeshPath.c_str());
				}
			}
		}

		// Read animation path (common to all versions)
		std::string strAnimPath;
		xStream >> strAnimPath;

		if (!strAnimPath.empty() && m_xMeshEntries.GetSize() > 0)
		{
			MeshEntry& xEntry = m_xMeshEntries.Get(m_xMeshEntries.GetSize() - 1);
			if (xEntry.m_pxGeometry && xEntry.m_pxGeometry->GetNumBones() > 0)
			{
				Zenith_Log("%s Recreating animation from: %s", LOG_TAG_MODEL, strAnimPath.c_str());
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
}

//=============================================================================
// Physics Mesh
//=============================================================================

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

	// Collect mesh geometries from either new or legacy system
	Zenith_Vector<Flux_MeshGeometry*> xMeshGeometries;

	if (m_pxModelInstance)
	{
		// New system: Get geometries from mesh instances
		// TODO: Flux_MeshInstance needs to provide access to geometry or position data
		// For now, physics mesh generation is not supported with new system
		Zenith_Log("%s Physics mesh generation not yet implemented for new model instance system", LOG_TAG_MODEL_PHYSICS);
		return;
	}
	else
	{
		// Legacy system
		if (m_xMeshEntries.GetSize() == 0)
		{
			Zenith_Log("%s Cannot generate physics mesh: no mesh entries", LOG_TAG_MODEL_PHYSICS);
			return;
		}

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

	Zenith_PhysicsMeshGenerator::DebugDrawPhysicsMesh(m_pxPhysicsMesh, xModelMatrix, m_xDebugDrawColor);
}

//=============================================================================
// Legacy LoadMeshesFromDir
//=============================================================================

void Zenith_ModelComponent::LoadMeshesFromDir(const std::filesystem::path& strPath, Flux_MaterialAsset* const pxOverrideMaterial, u_int uRetainAttributeBits, const bool bUploadToGPU)
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
					Zenith_Log("%s Failed to load texture: %s", LOG_TAG_MODEL, strFilepath.c_str());
					continue;
				}

				pxTexture->m_strSourcePath = strFilepath;
				m_xCreatedTextures.PushBack(pxTexture);

				const uint32_t uMatIndex = GetMaterialIndexFromTextureName(strFilename);

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
		if (xFile.path().extension() == ZENITH_MESH_EXT)
		{
			Flux_MeshGeometry* pxMesh = Zenith_AssetHandler::AddMeshFromFile(
				xFile.path().string().c_str(), uRetainAttributeBits, bUploadToGPU);

			if (!pxMesh)
			{
				Zenith_Log("%s Failed to load mesh: %s", LOG_TAG_MODEL, xFile.path().string().c_str());
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
				auto it = xMaterialMap.find(uMatIndex);
				if (it != xMaterialMap.end() && it->second)
				{
					Flux_MaterialAsset* pxMat = it->second;
					pxMat->SetBaseColor(pxMesh->m_xMaterialColor);
					AddMeshEntry(*pxMesh, *pxMat);
				}
				else
				{
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

//=============================================================================
// Editor UI
//=============================================================================

#ifdef ZENITH_TOOLS
void Zenith_ModelComponent::RenderTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	ImGui::PushID(szLabel);

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

	ImGui::Text("%s:", szLabel);
	ImGui::SameLine();

	ImVec2 xButtonSize(150, 20);
	ImGui::Button(strTextureName.c_str(), xButtonSize);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);

			Zenith_Log("%s Texture dropped on %s: %s",
				LOG_TAG_MODEL, szLabel, pFilePayload->m_szFilePath);

			AssignTextureToSlot(pFilePayload->m_szFilePath, uMeshIdx, eSlot);
		}

		ImGui::EndDragDropTarget();
	}

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

void Zenith_ModelComponent::AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, TextureSlotType eSlot)
{
	Zenith_AssetHandler::TextureData xTexData =
		Zenith_AssetHandler::LoadTexture2DFromFile(szFilePath);
	Flux_Texture* pxTexture = Zenith_AssetHandler::AddTexture(xTexData);
	xTexData.FreeAllocatedData();

	if (!pxTexture)
	{
		Zenith_Log("%s Failed to load texture: %s", LOG_TAG_MODEL, szFilePath);
		return;
	}

	pxTexture->m_strSourcePath = szFilePath;
	m_xCreatedTextures.PushBack(pxTexture);
	Zenith_Log("%s Loaded texture from: %s", LOG_TAG_MODEL, szFilePath);

	Flux_MaterialAsset* pxOldMaterial = m_xMeshEntries.Get(uMeshIdx).m_pxMaterial;

	Flux_MaterialAsset* pxNewMaterial = Flux_MaterialAsset::Create("Material_" + std::to_string(uMeshIdx));
	if (!pxNewMaterial)
	{
		Zenith_Log("%s Failed to create new material instance", LOG_TAG_MODEL);
		return;
	}

	m_xCreatedMaterials.PushBack(pxNewMaterial);
	Zenith_Log("%s Created new material instance", LOG_TAG_MODEL);

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

	std::string strPath(szFilePath);
	switch (eSlot)
	{
	case TEXTURE_SLOT_DIFFUSE:
		pxNewMaterial->SetDiffuseTexturePath(strPath);
		Zenith_Log("%s Set diffuse texture", LOG_TAG_MODEL);
		break;
	case TEXTURE_SLOT_NORMAL:
		pxNewMaterial->SetNormalTexturePath(strPath);
		Zenith_Log("%s Set normal texture", LOG_TAG_MODEL);
		break;
	case TEXTURE_SLOT_ROUGHNESS_METALLIC:
		pxNewMaterial->SetRoughnessMetallicTexturePath(strPath);
		Zenith_Log("%s Set roughness/metallic texture", LOG_TAG_MODEL);
		break;
	case TEXTURE_SLOT_OCCLUSION:
		pxNewMaterial->SetOcclusionTexturePath(strPath);
		Zenith_Log("%s Set occlusion texture", LOG_TAG_MODEL);
		break;
	case TEXTURE_SLOT_EMISSIVE:
		pxNewMaterial->SetEmissiveTexturePath(strPath);
		Zenith_Log("%s Set emissive texture", LOG_TAG_MODEL);
		break;
	}

	m_xMeshEntries.Get(uMeshIdx).m_pxMaterial = pxNewMaterial;
}

void Zenith_ModelComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Draw Physics Mesh", &m_bDebugDrawPhysicsMesh);

		ImGui::Separator();

		// Show which system is being used
		if (m_pxModelInstance)
		{
			ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Using: New Model Instance System");
			ImGui::Text("Model Path: %s", m_strModelPath.c_str());
			ImGui::Text("Meshes: %u", m_pxModelInstance->GetNumMeshes());
			ImGui::Text("Has Skeleton: %s", m_pxModelInstance->HasSkeleton() ? "Yes" : "No");
		}
		else if (m_xMeshEntries.GetSize() > 0)
		{
			ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "Using: Legacy Mesh Entry System");
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No model loaded");
		}

		ImGui::Separator();

		// Model drop target - drag .zmodel files here
		{
			ImGui::Text("Model:");
			ImGui::SameLine();

			std::string strModelName = "(none)";
			if (m_pxModelInstance && !m_strModelPath.empty())
			{
				std::filesystem::path xPath(m_strModelPath);
				strModelName = xPath.filename().string();
			}

			ImVec2 xButtonSize(200, 20);
			ImGui::Button(strModelName.c_str(), xButtonSize);

			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MODEL))
				{
					const DragDropFilePayload* pFilePayload =
						static_cast<const DragDropFilePayload*>(pPayload->Data);

					Zenith_Log("%s Model dropped: %s", LOG_TAG_MODEL, pFilePayload->m_szFilePath);
					LoadModel(pFilePayload->m_szFilePath);
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Drop a .zmodel file here to load it");
			}

			ImGui::SameLine();
			if (ImGui::Button("Clear##ClearModel"))
			{
				ClearModel();
			}
		}

		// Load model from .zmodel file (new system) - manual path entry
		if (ImGui::TreeNode("Load Model (Manual Path)"))
		{
			static char s_szModelPath[512] = "";
			ImGui::InputText("Model Path", s_szModelPath, sizeof(s_szModelPath));

			if (ImGui::Button("Load Model"))
			{
				if (strlen(s_szModelPath) > 0)
				{
					LoadModel(s_szModelPath);
				}
			}

			ImGui::TreePop();
		}

		// Animation section for new model instance system
		if (m_pxModelInstance && m_pxModelInstance->HasSkeleton() && ImGui::TreeNode("Animations (.zanim)"))
		{
			// Animation drop target - drag .zanim files here
			{
				ImVec2 xDropZoneSize(ImGui::GetContentRegionAvail().x, 30);
				ImGui::Button("Drop .zanim file here to add animation", xDropZoneSize);

				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_ANIMATION))
					{
						const DragDropFilePayload* pFilePayload =
							static_cast<const DragDropFilePayload*>(pPayload->Data);

						Zenith_Log("%s Animation dropped: %s", LOG_TAG_MODEL, pFilePayload->m_szFilePath);

						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(pFilePayload->m_szFilePath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log("%s Loaded animation: %s", LOG_TAG_MODEL, pxClip->GetName().c_str());
							}
							else
							{
								Zenith_Log("%s Failed to load animation from: %s", LOG_TAG_MODEL, pFilePayload->m_szFilePath);
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			ImGui::Separator();

			// Manual path entry section
			if (ImGui::TreeNode("Load Animation (Manual Path)"))
			{
				static char s_szAnimPath[512] = "";
				ImGui::InputText("Animation Path", s_szAnimPath, sizeof(s_szAnimPath));

				if (ImGui::Button("Load .zanim"))
				{
					if (strlen(s_szAnimPath) > 0)
					{
						Flux_AnimationController* pxController = GetOrCreateAnimationController();
						if (pxController)
						{
							// Load from .zanim file using our new loader
							Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(s_szAnimPath);
							if (pxClip)
							{
								pxController->GetClipCollection().AddClip(pxClip);
								Zenith_Log("[ModelComponent] Loaded animation from: %s", s_szAnimPath);
							}
							else
							{
								Zenith_Log("[ModelComponent] Failed to load animation from: %s", s_szAnimPath);
							}
						}
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Browse...##AnimBrowse"))
				{
#ifdef _WIN32
					OPENFILENAMEA ofn = { 0 };
					char szFile[512] = "";
					ofn.lStructSize = sizeof(ofn);
					ofn.lpstrFilter = "Animation Files (*.zanim)\0*.zanim\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
					if (GetOpenFileNameA(&ofn))
					{
						strncpy(s_szAnimPath, szFile, sizeof(s_szAnimPath) - 1);
					}
#endif
				}

				ImGui::TreePop();
			}

			// Show animation controller info
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Loaded Clips: %u", xClips.GetClipCount());

				// List clips and allow playing them
				if (xClips.GetClipCount() > 0)
				{
					for (const Flux_AnimationClip* pxClip : xClips.GetClips())
					{
						if (pxClip)
						{
							ImGui::PushID(pxClip);
							if (ImGui::Button(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
								Zenith_Log("[ModelComponent] Playing animation: %s", pxClip->GetName().c_str());
							}
							ImGui::SameLine();
							ImGui::Text("(%.2fs)", pxClip->GetDuration());
							ImGui::PopID();
						}
					}
				}

				ImGui::Separator();

				// Playback controls
				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused##NewModel", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Speed##NewModel", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				if (ImGui::Button("Stop"))
				{
					pxController->Stop();
				}

				// Editor preview: Update animation even when not in Play mode
				// This allows previewing animations in the editor
				if (pxController->HasAnimationContent())
				{
					float fPreviewDt = Zenith_Core::GetDt();
					pxController->Update(fPreviewDt);

					// Also update the model instance skeleton if using new system
					if (m_pxModelInstance && m_pxModelInstance->HasSkeleton())
					{
						m_pxModelInstance->UpdateAnimation();
					}
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Load an animation to create controller");
			}

			ImGui::TreePop();
		}

		// Load meshes from directory (legacy system)
		if (ImGui::TreeNode("Load Mesh (Legacy)"))
		{
			static char s_szMeshDirPath[512] = "";
			ImGui::InputText("Directory Path", s_szMeshDirPath, sizeof(s_szMeshDirPath));
			ImGui::SameLine();
			if (ImGui::Button("Browse..."))
			{
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
					m_xMeshEntries.Clear();
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
					Zenith_Log("%s Loaded meshes from: %s", LOG_TAG_MODEL, s_szMeshDirPath);
				}
			}

			ImGui::TreePop();
		}

		// Animation loading section (only for legacy system with mesh entries)
		if (m_xMeshEntries.GetSize() > 0 && ImGui::TreeNode("Animations"))
		{
			static char s_szAnimFilePath[512] = "";
			ImGui::InputText("Animation File (.fbx/.gltf)", s_szAnimFilePath, sizeof(s_szAnimFilePath));

			static int s_iTargetMeshIndex = 0;
			int iMaxIndex = static_cast<int>(m_xMeshEntries.GetSize()) - 1;
			ImGui::SliderInt("Target Mesh Index", &s_iTargetMeshIndex, 0, iMaxIndex);

			if (ImGui::Button("Load Animation"))
			{
				if (strlen(s_szAnimFilePath) > 0 && s_iTargetMeshIndex >= 0 && s_iTargetMeshIndex < static_cast<int>(m_xMeshEntries.GetSize()))
				{
					Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(s_iTargetMeshIndex);
					if (xMesh.GetNumBones() > 0)
					{
						if (xMesh.m_pxAnimation)
						{
							delete xMesh.m_pxAnimation;
						}
						xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
						Zenith_Log("%s Loaded animation from: %s for mesh %d", LOG_TAG_MODEL, s_szAnimFilePath, s_iTargetMeshIndex);
					}
					else
					{
						Zenith_Log("%s Cannot load animation: mesh %d has no bones", LOG_TAG_MODEL, s_iTargetMeshIndex);
					}
				}
			}

			if (ImGui::Button("Load Animation for All Meshes"))
			{
				if (strlen(s_szAnimFilePath) > 0)
				{
					for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
					{
						Flux_MeshGeometry& xMesh = GetMeshGeometryAtIndex(u);
						if (xMesh.GetNumBones() > 0)
						{
							if (xMesh.m_pxAnimation)
							{
								delete xMesh.m_pxAnimation;
							}
							xMesh.m_pxAnimation = new Flux_MeshAnimation(s_szAnimFilePath, xMesh);
							Zenith_Log("%s Loaded animation for mesh %u", LOG_TAG_MODEL, u);
						}
					}
				}
			}

			ImGui::Separator();
			for (uint32_t u = 0; u < m_xMeshEntries.GetSize(); u++)
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

			// Animation Controller section
			Flux_AnimationController* pxController = GetAnimationController();
			if (pxController)
			{
				ImGui::Separator();
				ImGui::Text("Animation Controller");

				bool bPaused = pxController->IsPaused();
				if (ImGui::Checkbox("Paused", &bPaused))
				{
					pxController->SetPaused(bPaused);
				}

				float fSpeed = pxController->GetPlaybackSpeed();
				if (ImGui::SliderFloat("Playback Speed", &fSpeed, 0.0f, 2.0f))
				{
					pxController->SetPlaybackSpeed(fSpeed);
				}

				const Flux_AnimationClipCollection& xClips = pxController->GetClipCollection();
				ImGui::Text("Clips loaded: %u", static_cast<uint32_t>(xClips.GetClips().size()));

				if (!xClips.GetClips().empty())
				{
					if (ImGui::TreeNode("Clip List"))
					{
						for (const Flux_AnimationClip* pxClip : xClips.GetClips())
						{
							if (pxClip && ImGui::Selectable(pxClip->GetName().c_str()))
							{
								pxController->PlayClip(pxClip->GetName());
							}
						}
						ImGui::TreePop();
					}
				}
			}

			ImGui::TreePop();
		}

		ImGui::Separator();
		ImGui::Text("Mesh Entries: %u", GetNumMeshes());

		// Display each mesh entry with its material (legacy system only)
		for (uint32_t uMeshIdx = 0; uMeshIdx < m_xMeshEntries.GetSize(); ++uMeshIdx)
		{
			ImGui::PushID(uMeshIdx);

			Flux_MaterialAsset& xMaterial = GetMaterialAtIndex(uMeshIdx);

			if (ImGui::TreeNode("MeshEntry", "Mesh Entry %u", uMeshIdx))
			{
				Flux_MeshGeometry& xGeom = GetMeshGeometryAtIndex(uMeshIdx);
				if (!xGeom.m_strSourcePath.empty())
				{
					ImGui::TextWrapped("Source: %s", xGeom.m_strSourcePath.c_str());
				}

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
