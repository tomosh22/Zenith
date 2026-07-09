#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

// Forward declarations for new asset/instance system
class Zenith_ModelAsset;
class Flux_MeshGeometry;
class Flux_ModelInstance;
class Flux_MeshInstance;
class Flux_SkeletonInstance;

// Forward declarations for RegisterProperties (cycle-avoidance — see TransformComponent.h).
template<typename T> class Zenith_Vector;
struct Zenith_PropertyDescriptor;

// Owns a renderable Flux_ModelInstance and nothing else. The instance is populated
// by one of two paths — LoadModel() (from a .zmodel asset) or AddMeshEntry()
// (procedural geometry built at runtime) — and the component then exposes it plus a
// few read-only rendering/skeleton queries used by the render snapshot fill, the
// AnimatorComponent, and the AttachmentComponent. It is a render-instance owner: NOT
// an asset loader (loading is one population path) and NOT a physics component (the
// collision mesh derived from this geometry is owned by Zenith_ColliderComponent).
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

	/**
	 * Resolve a named bone's current model-space matrix from the posed skeleton
	 * instance. Returns false if there is no skeleton or the bone is unknown.
	 * Lets non-Flux callers (e.g. Zenith_AttachmentComponent) read a posed bone
	 * without including the Flux skeleton headers themselves.
	 */
	bool GetBoneModelMatrix(const std::string& strBoneName, Zenith_Maths::Matrix4& xOut) const;

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

	// Monotonic counter bumped whenever the model's geometry changes (LoadModel /
	// ClearModel / AddMeshEntry). Zenith_ColliderComponent caches the collision mesh
	// it derives from this geometry against this value, regenerating when it changes.
	uint32_t GetGeometryRevision() const { return m_uGeometryRevision; }

#ifdef ZENITH_TOOLS
	// Editor UI — renders this component's block in the Properties panel. The
	// implementation and all its helpers live in Zenith_ModelComponent_Editor.cpp.
	void RenderPropertiesPanel();
#endif

private:
	Zenith_Entity m_xParentEntity;

	// Single owning model instance, populated by LoadModel (asset) or AddMeshEntry
	// (procedural); null until one of those runs.
	Flux_ModelInstance* m_pxModelInstance = nullptr;

	// Path-based reference to the .zmodel asset (primary) + its resolved path.
	ModelHandle m_xModel;
	std::string m_strModelPath;

	// Bumped on every geometry mutation (LoadModel / ClearModel / AddMeshEntry) so
	// derived caches (Zenith_ColliderComponent's collision mesh) detect staleness.
	uint32_t m_uGeometryRevision = 0;

};
