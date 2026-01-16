#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_AssetRef.h"
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
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
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
	// MeshEntry structure (for procedural mesh construction)
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

	// Move semantics - required for component pool operations
	Zenith_ModelComponent(Zenith_ModelComponent&& xOther) noexcept;
	Zenith_ModelComponent& operator=(Zenith_ModelComponent&& xOther) noexcept;

	// Disable copy semantics - component should only be moved
	Zenith_ModelComponent(const Zenith_ModelComponent&) = delete;
	Zenith_ModelComponent& operator=(const Zenith_ModelComponent&) = delete;

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
	// Rendering Helpers
	//=========================================================================

	/**
	 * Get number of meshes in the model
	 */
	uint32_t GetNumMeshes() const;

	/**
	 * Get mesh instance at index
	 * Returns nullptr if using procedural mesh entries
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
	 * Get skeleton instance
	 * Returns nullptr if using procedural mesh entries or no skeleton
	 */
	Flux_SkeletonInstance* GetSkeletonInstance() const;

	//=========================================================================
	// Animation System Integration
	//=========================================================================

	/**
	 * Get/create animation controller
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
	// Procedural Mesh API
	//=========================================================================

	/**
	 * Add a mesh entry for procedural/runtime mesh construction.
	 * Use this when building models programmatically from already-loaded geometry and materials.
	 * For loading assets from files, use LoadModel() with a .zmodel asset instead.
	 */
	void AddMeshEntry(Flux_MeshGeometry& xGeometry, Flux_MaterialAsset& xMaterial) { m_xMeshEntries.PushBack({ &xGeometry, &xMaterial }); }

	// Mesh entry accessors (for procedural mesh system)
	// Note: These assert that pointers are valid - callers should check entry validity first
	Flux_MeshGeometry& GetMeshGeometryAtIndex(const uint32_t uIndex) const
	{
		Zenith_Assert(uIndex < m_xMeshEntries.GetSize(), "GetMeshGeometryAtIndex: Index %u out of bounds (size=%u)", uIndex, m_xMeshEntries.GetSize());
		Flux_MeshGeometry* pxGeometry = m_xMeshEntries.Get(uIndex).m_pxGeometry;
		Zenith_Assert(pxGeometry != nullptr, "GetMeshGeometryAtIndex: Geometry pointer is null at index %u", uIndex);
		return *pxGeometry;
	}
	const Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex) const
	{
		Zenith_Assert(uIndex < m_xMeshEntries.GetSize(), "GetMaterialAtIndex: Index %u out of bounds (size=%u)", uIndex, m_xMeshEntries.GetSize());
		const Flux_MaterialAsset* pxMaterial = m_xMeshEntries.Get(uIndex).m_pxMaterial;
		Zenith_Assert(pxMaterial != nullptr, "GetMaterialAtIndex: Material pointer is null at index %u", uIndex);
		return *pxMaterial;
	}
	Flux_MaterialAsset& GetMaterialAtIndex(const uint32_t uIndex)
	{
		Zenith_Assert(uIndex < m_xMeshEntries.GetSize(), "GetMaterialAtIndex: Index %u out of bounds (size=%u)", uIndex, m_xMeshEntries.GetSize());
		Flux_MaterialAsset* pxMaterial = m_xMeshEntries.Get(uIndex).m_pxMaterial;
		Zenith_Assert(pxMaterial != nullptr, "GetMaterialAtIndex: Material pointer is null at index %u", uIndex);
		return *pxMaterial;
	}
	uint32_t GetNumMeshEntries() const { return m_xMeshEntries.GetSize(); }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	// Check if using model instance API vs procedural mesh entries
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

	void RenderPropertiesPanel();

private:
	// Helper to load texture and assign to material slot (creates new material instance)
	void AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, Zenith_Editor_MaterialUI::TextureSlotType eSlot);
public:
#endif

//private:
	Zenith_Entity m_xParentEntity;

	//=========================================================================
	// New Model Instance System
	//=========================================================================

	// Single model instance (replaces m_xMeshEntries for new system)
	Flux_ModelInstance* m_pxModelInstance = nullptr;

	// Animation controller (moved from mesh to component level)
	Flux_AnimationController* m_pxAnimController = nullptr;

	// GUID-based reference to the .zmodel asset (primary)
	ModelRef m_xModel;

	// Path to the .zmodel file (cached from GUID resolution)
	std::string m_strModelPath;

	//=========================================================================
	// Procedural Mesh System
	//=========================================================================

	Zenith_Vector<MeshEntry> m_xMeshEntries;
	Flux_MeshGeometry* m_pxPhysicsMesh = nullptr;

	// Debug draw settings
	bool m_bDebugDrawPhysicsMesh = true;
	Zenith_Maths::Vector3 m_xDebugDrawColor = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f); // Green

};
