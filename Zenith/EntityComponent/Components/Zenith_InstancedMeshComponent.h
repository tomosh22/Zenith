#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include <string>
#include <vector>

class Zenith_MeshAsset;

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

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
};
