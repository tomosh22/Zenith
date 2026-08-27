#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "AssetHandling/Zenith_AssetHandle.h"	// MeshHandle/MaterialHandle typedefs + Zenith_Mesh/MaterialAsset fwd-decls (were pulled in transitively via the now-removed Flux includes)
#include "Physics/Zenith_Physics_Fwd.h"	// Zenith_PhysicsBodyID (the per-instance body ledger) -- fwd header, no Jolt
#include "Collections/Zenith_Vector.h"
#include <string>
#include <vector>

class Zenith_MeshAsset;
class Flux_InstanceGroup;
class Flux_AnimationTexture;
class Flux_MeshInstance;

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

//=============================================================================
// Per-instance collider authored on the component and serialized with it (v5).
// NONE (the default) preserves today's behaviour exactly: no bodies, no physics
// access, byte-identical v4 semantics.
//=============================================================================
enum InstanceColliderType : uint32_t
{
	INSTANCE_COLLIDER_TYPE_NONE = 0,
	INSTANCE_COLLIDER_TYPE_CAPSULE = 1,
};

struct Zenith_InstanceColliderConfig
{
	InstanceColliderType m_eType = INSTANCE_COLLIDER_TYPE_NONE;
	float m_fRadius = 0.3f;              // local (pre-scale)
	float m_fCylinderHalfHeight = 3.2f;  // Jolt convention: total half-extent = this + radius
	float m_fLocalYOffset = 3.5f;        // capsule centre above the instance origin (pre-scale)
};

//=============================================================================
// Zenith_InstancedMeshComponent
// Component for rendering 100k+ mesh instances with GPU culling and VAT animation.
//
// Usage:
//   1. Call SetMesh() and SetMaterial() to configure shared geometry
//   2. Optionally call SetAnimationTexture() for animated instances
//   3. Call SpawnInstance() to create instances
//   4. Use SetInstance*() methods to configure individual instances
//   5. Call Update() each frame to advance animations
//=============================================================================
class Zenith_InstancedMeshComponent
{
public:
	Zenith_InstancedMeshComponent(Zenith_Entity& xEntity);
	~Zenith_InstancedMeshComponent();

	// Move semantics - required for component pool operations
	Zenith_InstancedMeshComponent(Zenith_InstancedMeshComponent&& xOther) noexcept;
	Zenith_InstancedMeshComponent& operator=(Zenith_InstancedMeshComponent&& xOther) noexcept;

	// Disable copy semantics - component should only be moved
	Zenith_InstancedMeshComponent(const Zenith_InstancedMeshComponent&) = delete;
	Zenith_InstancedMeshComponent& operator=(const Zenith_InstancedMeshComponent&) = delete;

	//-------------------------------------------------------------------------
	// Configuration (call before spawning instances)
	//-------------------------------------------------------------------------

	// Set the mesh to instance (required)
	void SetMesh(Flux_MeshInstance* pxMesh);

	// Set the material for all instances (required)
	// Note: For file-based materials, use LoadMaterial() to ensure proper serialization
	void SetMaterial(Zenith_MaterialAsset* pxMaterial);

	// Load material from path (ensures proper serialization)
	void LoadMaterial(const std::string& strPath);

	// Set the vertex animation texture for skeletal animation (optional)
	void SetAnimationTexture(Flux_AnimationTexture* pxAnimTex);

	// Set bounding sphere for frustum culling
	void SetBounds(const Zenith_Maths::Vector3& xCenter, float fRadius);

	// Load mesh from .zmesh file
	void LoadMesh(const std::string& strPath);

	// Load animation texture from .zanmt file
	void LoadAnimationTexture(const std::string& strPath);

	//-------------------------------------------------------------------------
	// Instance Spawning
	//-------------------------------------------------------------------------

	// Spawn instance at position with optional rotation and scale
	// Returns instance ID for future manipulation
	uint32_t SpawnInstance(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		const Zenith_Maths::Vector3& xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)
	);

	// Spawn instance with full transform matrix
	uint32_t SpawnInstanceWithMatrix(const Zenith_Maths::Matrix4& xMatrix);

	// Remove an instance
	void DespawnInstance(uint32_t uInstanceID);

	// Remove all instances
	void ClearInstances();

	// Reserve capacity for expected instance count (avoids reallocation)
	void Reserve(uint32_t uCapacity);

	//-------------------------------------------------------------------------
	// Per-Instance Control
	//-------------------------------------------------------------------------

	// Set instance world transform
	void SetInstanceTransform(
		uint32_t uInstanceID,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		const Zenith_Maths::Vector3& xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)
	);

	// Set instance world transform from matrix
	void SetInstanceMatrix(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix);

	// Set animation for an instance by name (requires AnimationTexture)
	void SetInstanceAnimation(uint32_t uInstanceID, const std::string& strAnimName, float fNormalizedTime = 0.0f);

	// Set animation for an instance by index
	void SetInstanceAnimationByIndex(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime = 0.0f);

	// Set animation time (0-1 normalized)
	void SetInstanceAnimationTime(uint32_t uInstanceID, float fNormalizedTime);

	// Set color tint (RGBA, 0-1 range)
	void SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor);

	// Enable/disable instance visibility
	void SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled);

	//-------------------------------------------------------------------------
	// Instance Colliders (one static physics body per live instance)
	//-------------------------------------------------------------------------
	// A group of 2500 trees becomes 2500 static Jolt capsules owned by THIS
	// component -- deliberately not 2500 entities with ColliderComponents, which
	// would pay the per-frame Zenith_SyncPhysicsTransforms sweep and become 2500
	// navmesh obstruction boxes. Contacts and raycasts against an instance body
	// therefore attribute to the GROUP entity, not to an individual instance.
	//
	// Enable a per-instance static capsule. Bodies are created for every
	// currently-enabled instance AND for every instance spawned afterwards;
	// despawn / disable / clear destroys them. Idempotent for an identical config.
	void SetInstanceColliderCapsule(float fRadius, float fCylinderHalfHeight, float fLocalYOffset);
	// Destroy every instance body and reset the config to NONE.
	void ClearInstanceColliderConfig();
	const Zenith_InstanceColliderConfig& GetInstanceColliderConfig() const { return m_xInstanceColliderConfig; }
	bool HasInstanceColliders() const;      // any live instance body
	uint32_t GetInstanceBodyCount() const;  // number of live instance bodies
	// The body for one instance slot, or an INVALID id when that slot has none
	// (out of range, disabled, despawned, or the config is NONE). Lets a caller
	// query a specific tree's pose / shape through Zenith_Physics without this
	// component re-exporting the whole physics surface.
	Zenith_PhysicsBodyID GetInstanceBodyID(uint32_t uSlot) const;

	//-------------------------------------------------------------------------
	// Animation Playback
	//-------------------------------------------------------------------------

	// Set animation duration (used for time advancement)
	void SetAnimationDuration(float fDuration) { m_fAnimationDuration = fDuration; }
	float GetAnimationDuration() const { return m_fAnimationDuration; }

	// Set playback speed multiplier
	void SetAnimationSpeed(float fSpeed) { m_fAnimationSpeed = fSpeed; }
	float GetAnimationSpeed() const { return m_fAnimationSpeed; }

	// Pause/resume animation
	void SetAnimationsPaused(bool bPaused) { m_bAnimationsPaused = bPaused; }
	bool AreAnimationsPaused() const { return m_bAnimationsPaused; }

	//-------------------------------------------------------------------------
	// Per-Frame Update
	//-------------------------------------------------------------------------

	// Call each frame to advance animations
	void Update(float fDt);

	// ECS lifecycle hook (detected by name through the component-meta system):
	// advances VAT animation time every Playing-mode frame so wind-swayed
	// instanced foliage animates without a game-side ticker. Editor Stopped
	// mode doesn't dispatch component OnUpdate — the terrain editor services
	// its tree components there instead.
	void OnUpdate(float fDt) { Update(fDt); }

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------

	uint32_t GetInstanceCount() const;
	uint32_t GetVisibleCount() const;
	bool IsEmpty() const;

	Flux_InstanceGroup* GetInstanceGroup() { return m_pxInstanceGroup; }
	const Flux_InstanceGroup* GetInstanceGroup() const { return m_pxInstanceGroup; }

	Flux_MeshInstance* GetMesh() const;
	Zenith_MaterialAsset* GetMaterial() const;
	Flux_AnimationTexture* GetAnimationTexture() const;

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	//-------------------------------------------------------------------------
	// Serialization
	//-------------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	//-------------------------------------------------------------------------
	// Editor UI
	//-------------------------------------------------------------------------
	void RenderPropertiesPanel();
#endif

private:
	//-------------------------------------------------------------------------
	// Helper functions
	//-------------------------------------------------------------------------
	Zenith_Maths::Matrix4 BuildMatrix(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xScale
	) const;

	void EnsureInstanceGroupCreated();

	// Instance-collider internals. Every one is a no-op when the config is NONE,
	// so the collider-free path costs a single enum compare.
	void CreateInstanceBody(uint32_t uSlot);    // early-returns when the slot already has a body
	void DestroyInstanceBody(uint32_t uSlot);   // no-op when no body; always invalidates the ledger slot
	void CreateAllInstanceBodies();             // sweep over ComputeVisibleIndices (the enabled slots)
	void DestroyAllInstanceBodies();            // sweep over the LEDGER (robust vs stale group flags)
	void RefreshInstanceBody(uint32_t uSlot);   // destroy + recreate (pose / scale change)
	void RebuildInstanceBodies();               // destroy-all + create-all + broadphase re-optimise

	//-------------------------------------------------------------------------
	// Data
	//-------------------------------------------------------------------------
	Zenith_Entity m_xParentEntity;

	// The instance group (owned)
	Flux_InstanceGroup* m_pxInstanceGroup = nullptr;

	// Asset handles (handles manage ref counting)
	MeshHandle m_xMeshAsset;
	MaterialHandle m_xMaterial;

	// Non-registry resources (still using raw pointers)
	Flux_MeshInstance* m_pxOwnedMeshInstance = nullptr;  // Not a registry asset
	Flux_AnimationTexture* m_pxOwnedAnimTexture = nullptr;  // Not a registry asset
	std::string m_strAnimTexturePath;  // Path for animation texture (not a registry asset)

	// Animation playback settings
	float m_fAnimationDuration = 1.0f;
	float m_fAnimationSpeed = 1.0f;
	bool m_bAnimationsPaused = false;

	// Authored collider config (serialized, v5) + the slot-indexed body ledger.
	// Ledger index == instance slot id; an INVALID id means "no body for that
	// slot". Slot-indexed rather than packed because Flux_InstanceGroup recycles
	// slots through a free list, so live slots are NOT contiguous after removals.
	Zenith_InstanceColliderConfig m_xInstanceColliderConfig;
	Zenith_Vector<Zenith_PhysicsBodyID> m_axInstanceBodyIDs;
};
