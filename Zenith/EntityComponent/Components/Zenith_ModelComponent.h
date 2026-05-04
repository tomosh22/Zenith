#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"

// Forward declarations for new asset/instance system
class Zenith_ModelAsset;
class Zenith_MeshGeometryAsset;
class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_SkeletonInstance;

// Forward declarations for RegisterProperties (cycle-avoidance — see TransformComponent.h).
template<typename T> class Zenith_Vector;
struct Zenith_PropertyDescriptor;

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

	Zenith_ModelComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent();

	// Property registration for prefab-variant overrides. The reflection layer
	// in Zenith_ComponentMeta calls this once at component-type registration.
	static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties);

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
	 */
	Flux_MeshInstance* GetMeshInstance(uint32_t uIndex) const;

	/**
	 * Get material at index
	 */
	Zenith_MaterialAsset* GetMaterial(uint32_t uIndex) const;

	/**
	 * Check if model has skeleton (is animated)
	 */
	bool HasSkeleton() const;

	/**
	 * Get skeleton instance
	 * Returns nullptr if model has no skeleton
	 */
	Flux_SkeletonInstance* GetSkeletonInstance() const;

	//=========================================================================
	// Serialization
	//=========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	// ReadFromDataStream sub-path — GUID-based model + per-override materials.
	void ReadModelInstanceWithMaterials(Zenith_DataStream& xStream, uint32_t uVersion);
public:

	//=========================================================================
	// Procedural Mesh API
	//=========================================================================

	/**
	 * Add a procedural mesh to this component's model.
	 * Used by code paths that build meshes at runtime (generated cubes, sprites,
	 * per-game procedural content) without going through a .zmodel asset.
	 * The first call constructs a procedural Flux_ModelInstance; subsequent
	 * calls append additional sub-meshes. The caller owns the geometry and
	 * must keep it alive for the lifetime of the component.
	 */
	void AddMeshEntry(Flux_MeshGeometry& xGeometry, Zenith_MaterialAsset& xMaterial);

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	//=========================================================================
	// Physics Mesh
	//=========================================================================

	void GeneratePhysicsMesh(PhysicsMeshQuality eQuality = PHYSICS_MESH_QUALITY_MEDIUM);
	void GeneratePhysicsMeshWithConfig(const PhysicsMeshConfig& xConfig);
	Flux_MeshGeometry* GetPhysicsMesh() const;
	bool HasPhysicsMesh() const { return m_xPhysicsMeshAsset.GetDirect() != nullptr; }
	void SetDebugDrawPhysicsMesh(bool bEnable) { m_bDebugDrawPhysicsMesh = bEnable; }
	bool GetDebugDrawPhysicsMesh() const { return m_bDebugDrawPhysicsMesh; }
	void QueueDebugDrawPhysicsMesh(const Zenith_Maths::Vector3& xColor) const;
	void ClearPhysicsMesh();

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------

	void RenderPropertiesPanel();

private:
	// Helper to load texture and assign to material slot (creates new material instance)
	void AssignTextureToSlot(const char* szFilePath, uint32_t uMeshIdx, Zenith_Editor_MaterialUI::TextureSlotType eSlot);

	// Per-section helpers that RenderPropertiesPanel dispatches to. Each owns
	// one logical block of the properties panel (status, drop targets, material
	// lists, etc.) so the top-level function stays a thin dispatcher.
	void RenderModelStatusSection();
	void RenderModelDropTargetSection();
	void RenderManualLoadSection();
	void RenderModelInstanceMaterialsSection();
	void RenderMeshMaterialSlots(uint32_t uMeshIdx, Zenith_MaterialAsset& xMaterial);
public:
#endif

//private:
	Zenith_Entity m_xParentEntity;

	//=========================================================================
	// New Model Instance System
	//=========================================================================

	// Single model instance (replaces m_xMeshEntries for new system)
	Flux_ModelInstance* m_pxModelInstance = nullptr;

	// Path-based reference to the .zmodel asset (primary)
	ModelHandle m_xModel;

	// Path to the .zmodel file (cached from GUID resolution)
	std::string m_strModelPath;

	//=========================================================================
	// Physics Mesh
	//=========================================================================

	MeshGeometryHandle m_xPhysicsMeshAsset;
	bool m_bDebugDrawPhysicsMesh = false;

};
