#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"

// Forward declarations for new asset/instance system
class Zenith_ModelAsset;
class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_SkeletonInstance;

// Forward declarations for animation system
class Flux_AnimationController;
class Flux_AnimationStateMachine;
class Flux_IKSolver;

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

	//=========================================================================
	// Legacy MeshEntry structure (kept for backward compatibility)
	//=========================================================================
	struct MeshEntry
	{
		Flux_MeshGeometry* m_pxGeometry;
		Flux_MaterialAsset* m_pxMaterial;
	};

	Zenith_ModelComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent();

	//=========================================================================
	// New Model Instance API (primary interface)
	//=========================================================================

	/**
	 * Load model from .zmodel file
	 * Creates a Flux_ModelInstance from the asset
	 * @param strPath Path to .zmodel file
	 */
	void LoadModel(const std::string& strPath);

	/**
	 * Clear the current model and release resources
	 */
	void ClearModel();

	/**
	 * Check if a model is loaded
	 */
	bool HasModel() const { return m_pxModelInstance != nullptr; }

	/**
	 * Get the model instance
	 */
	Flux_ModelInstance* GetModelInstance() const { return m_pxModelInstance; }

	/**
	 * Get the path to the loaded .zmodel file
	 */
	const std::string& GetModelPath() const { return m_strModelPath; }

	//=========================================================================
	// Rendering Helpers (work with both new and legacy systems)
	//=========================================================================

	/**
	 * Get number of meshes in the model
	 */
	uint32_t GetNumMeshes() const;

	/**
	 * Get mesh instance at index (new system)
	 * Returns nullptr if using legacy system
	 */
	Flux_MeshInstance* GetMeshInstance(uint32_t uIndex) const;

	/**
	 * Get material at index
	 */
	Flux_MaterialAsset* GetMaterial(uint32_t uIndex) const;

	/**
	 * Check if model has skeleton (is animated)
	 */
	bool HasSkeleton() const;

	/**
	 * Get skeleton instance (new system)
	 * Returns nullptr if using legacy system or no skeleton
	 */
	Flux_SkeletonInstance* GetSkeletonInstance() const;

	//=========================================================================
	// Animation System Integration
	//=========================================================================

	/**
	 * Get/create animation controller
	 * Works with both new skeleton instance and legacy mesh geometry
	 */
	Flux_AnimationController* GetOrCreateAnimationController();

	/**
	 * Get animation controller (returns nullptr if not created)
	 */
	Flux_AnimationController* GetAnimationController() const { return m_pxAnimController; }

	/**
	 * Update animation (call each frame)
	 */
	void Update(float fDt);

	// Animation convenience methods
	void PlayAnimation(const std::string& strClipName, float fBlendTime = 0.15f);
	void StopAnimations();
	void SetAnimationsPaused(bool bPaused);
	bool AreAnimationsPaused() const;
	void SetAnimationPlaybackSpeed(float fSpeed);
	float GetAnimationPlaybackSpeed() const;

	// State machine parameter shortcuts
	void SetAnimationFloat(const std::string& strName, float fValue);
	void SetAnimationInt(const std::string& strName, int32_t iValue);
	void SetAnimationBool(const std::string& strName, bool bValue);
	void SetAnimationTrigger(const std::string& strName);

	// IK target shortcuts
	void SetIKTarget(const std::string& strChainName, const Zenith_Maths::Vector3& xPosition, float fWeight = 1.0f);
	void ClearIKTarget(const std::string& strChainName);

	// Set world matrix for IK (called automatically if TransformComponent exists)
	void UpdateAnimationWorldMatrix();

	//=========================================================================
	// Serialization
	//=========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//=========================================================================
	// Legacy API (kept for backward compatibility)
	//=========================================================================

	//#TO not the cleanest code in the world
	//takes a filename in the form meshname_texturetype_materialindex (no extension)
	//and returns materialindex, for example Assets/Meshes/foo_bar_5 would return 5
	[[deprecated("Use LoadModel() instead")]]
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
	[[deprecated("Use LoadModel() instead")]]
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

	[[deprecated("Use LoadModel() instead")]]
	void LoadMeshesFromDir(const std::filesystem::path& strPath, Flux_MaterialAsset* const pxOverrideMaterial = nullptr, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true);

	[[deprecated("Use new model instance API instead")]]
	void AddMeshEntry(Flux_MeshGeometry& xGeometry, Flux_MaterialAsset& xMaterial) { m_xMeshEntries.PushBack({ &xGeometry, &xMaterial }); }

	[[deprecated("Use GetMeshInstance() instead")]]
	Flux_MeshGeometry& GetMeshGeometryAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxGeometry; }

	[[deprecated("Use GetMaterial() instead")]]
	const Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex) const { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }

	[[deprecated("Use GetMaterial() instead")]]
	Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex) { return *m_xMeshEntries.Get(uIndex).m_pxMaterial; }

	[[deprecated("Use GetNumMeshes() instead")]]
	const uint32_t GetNumMeshEntries() const { return m_xMeshEntries.GetSize(); }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	// Check if using new model instance API vs legacy mesh entries
	bool IsUsingModelInstance() const { return m_pxModelInstance != nullptr; }

	//=========================================================================
	// Physics Mesh
	//=========================================================================

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
	void RenderTextureSlot(const char* szLabel, Flux_MaterialAsset& xMaterial, uint32_t uMeshIdx, TextureSlotType eSlot);

	// Helper to load texture and assign to material slot
	void AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, TextureSlotType eSlot);
public:
#endif

private:
	// Helper to read legacy mesh entries from DataStream (used by both tools and non-tools builds)
	void ReadLegacyMeshEntries(Zenith_DataStream& xStream, uint32_t uVersion);

public:

//private:
	Zenith_Entity m_xParentEntity;

	//=========================================================================
	// New Model Instance System
	//=========================================================================

	// Single model instance (replaces m_xMeshEntries for new system)
	Flux_ModelInstance* m_pxModelInstance = nullptr;

	// Animation controller (moved from mesh to component level)
	Flux_AnimationController* m_pxAnimController = nullptr;

	// Path to the .zmodel file (for serialization)
	std::string m_strModelPath;

	//=========================================================================
	// Legacy System (kept for backward compatibility)
	//=========================================================================

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

};
