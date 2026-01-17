#pragma once

#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Maths/Zenith_Maths.h"
#include <vector>

class Flux_AnimationTexture;

//=============================================================================
// Per-instance animation and color data (16 bytes, GPU-aligned)
//=============================================================================
struct Flux_InstanceAnimData
{
	uint16_t m_uAnimationIndex;    // Which animation clip (0-65535)
	uint16_t m_uFrameCount;        // Frames in this animation
	float m_fAnimTime;             // 0-1 normalized time within animation
	uint32_t m_uColorTint;         // RGBA8 packed color (premultiplied alpha)
	uint32_t m_uFlags;             // Visibility/active flags
};
static_assert(sizeof(Flux_InstanceAnimData) == 16, "Flux_InstanceAnimData must be 16 bytes");

//=============================================================================
// Bounding sphere for culling (16 bytes)
//=============================================================================
struct Flux_InstanceBounds
{
	Zenith_Maths::Vector3 m_xCenter = Zenith_Maths::Vector3(0.0f);  // Local-space center
	float m_fRadius = 10.0f;                                         // Local-space radius (default is generous to avoid culling)
};
static_assert(sizeof(Flux_InstanceBounds) == 16, "Flux_InstanceBounds must be 16 bytes");

//=============================================================================
// Flux_InstanceGroup
// Manages a collection of mesh instances that share geometry and material.
// Supports GPU frustum culling and indirect drawing for 100k+ instances.
//=============================================================================
class Flux_InstanceGroup
{
public:
	static constexpr uint32_t uMAX_INSTANCES = 131072;  // 128K max instances

	Flux_InstanceGroup();
	~Flux_InstanceGroup();

	// Non-copyable (owns GPU resources)
	Flux_InstanceGroup(const Flux_InstanceGroup&) = delete;
	Flux_InstanceGroup& operator=(const Flux_InstanceGroup&) = delete;

	//-------------------------------------------------------------------------
	// Configuration (call before adding instances)
	//-------------------------------------------------------------------------
	void SetMesh(Flux_MeshInstance* pxMesh);
	void SetMaterial(Zenith_MaterialAsset* pxMaterial);
	void SetAnimationTexture(Flux_AnimationTexture* pxAnimTex);
	void SetBounds(const Flux_InstanceBounds& xBounds);

	//-------------------------------------------------------------------------
	// Instance Management
	//-------------------------------------------------------------------------

	// Add a new instance, returns instance ID (0 to MAX_INSTANCES-1)
	uint32_t AddInstance();

	// Remove an instance by ID (uses swap-and-pop, so IDs may change)
	void RemoveInstance(uint32_t uInstanceID);

	// Remove all instances
	void Clear();

	// Set transform for an instance
	void SetInstanceTransform(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix);

	// Set animation state for an instance
	void SetInstanceAnimation(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime, uint32_t uFrameCount);

	// Set color tint for an instance (RGBA, 0-1 range)
	void SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor);

	// Enable/disable an instance (disabled instances are not rendered)
	void SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled);

	//-------------------------------------------------------------------------
	// Bulk Operations (more efficient for large updates)
	//-------------------------------------------------------------------------

	// Advance all instance animations by dt seconds
	void AdvanceAllAnimations(float fDt, float fAnimDuration);

	// Reserve capacity for expected instance count
	void Reserve(uint32_t uCapacity);

	//-------------------------------------------------------------------------
	// Per-Frame GPU Update
	//-------------------------------------------------------------------------

	// Upload dirty instance data to GPU buffers
	void UpdateGPUBuffers();

	// Reset the visible count for a new frame (called before culling)
	void ResetVisibleCount();

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------

	uint32_t GetInstanceCount() const { return m_uInstanceCount; }
	uint32_t GetVisibleCount() const { return m_uVisibleCount; }
	bool IsEmpty() const { return m_uInstanceCount == 0; }

	Flux_MeshInstance* GetMesh() const { return m_pxMesh; }
	Zenith_MaterialAsset* GetMaterial() const { return m_pxMaterial; }
	Flux_AnimationTexture* GetAnimationTexture() const { return m_pxAnimationTexture; }
	const Flux_InstanceBounds& GetBounds() const { return m_xBounds; }

	// GPU buffer access for rendering
	const Flux_ReadWriteBuffer& GetTransformBuffer() const { return m_xTransformBuffer; }
	const Flux_ReadWriteBuffer& GetAnimDataBuffer() const { return m_xAnimDataBuffer; }
	const Flux_ReadWriteBuffer& GetVisibleIndexBuffer() const { return m_xVisibleIndexBuffer; }
	const Flux_ReadWriteBuffer& GetBoundsBuffer() const { return m_xBoundsBuffer; }
	const Flux_IndirectBuffer& GetIndirectBuffer() const { return m_xIndirectBuffer; }
	const Flux_ReadWriteBuffer& GetVisibleCountBuffer() const { return m_xVisibleCountBuffer; }

	Flux_ReadWriteBuffer& GetTransformBuffer() { return m_xTransformBuffer; }
	Flux_ReadWriteBuffer& GetAnimDataBuffer() { return m_xAnimDataBuffer; }
	Flux_ReadWriteBuffer& GetVisibleIndexBuffer() { return m_xVisibleIndexBuffer; }
	Flux_ReadWriteBuffer& GetBoundsBuffer() { return m_xBoundsBuffer; }
	Flux_IndirectBuffer& GetIndirectBuffer() { return m_xIndirectBuffer; }
	Flux_ReadWriteBuffer& GetVisibleCountBuffer() { return m_xVisibleCountBuffer; }

private:
	//-------------------------------------------------------------------------
	// Helper functions
	//-------------------------------------------------------------------------
	void InitialiseGPUBuffers();
	void DestroyGPUBuffers();
	void MarkDirty(uint32_t uInstanceID);

	static uint32_t PackColorRGBA8(const Zenith_Maths::Vector4& xColor);

	//-------------------------------------------------------------------------
	// CPU-side instance data (Structure of Arrays for cache efficiency)
	//-------------------------------------------------------------------------
	std::vector<Zenith_Maths::Matrix4> m_axTransforms;
	std::vector<Flux_InstanceAnimData> m_axAnimData;
	std::vector<bool> m_abDirty;              // Per-instance dirty flags
	std::vector<uint32_t> m_auFreeIDs;        // Recycled instance IDs

	uint32_t m_uInstanceCount = 0;
	uint32_t m_uVisibleCount = 0;
	uint32_t m_uCapacity = 0;
	bool m_bBuffersInitialised = false;
	bool m_bTransformsDirty = false;
	bool m_bAnimDataDirty = false;

	//-------------------------------------------------------------------------
	// GPU Buffers
	//-------------------------------------------------------------------------
	Flux_ReadWriteBuffer m_xTransformBuffer;      // mat4[] - per-instance transforms
	Flux_ReadWriteBuffer m_xAnimDataBuffer;       // Flux_InstanceAnimData[]
	Flux_ReadWriteBuffer m_xVisibleIndexBuffer;   // uint32[] - indices of visible instances
	Flux_ReadWriteBuffer m_xBoundsBuffer;         // vec4[] - bounding spheres for culling (constant)
	Flux_IndirectBuffer m_xIndirectBuffer;        // VkDrawIndexedIndirectCommand
	Flux_ReadWriteBuffer m_xVisibleCountBuffer;   // uint32 - atomic counter for culling

	//-------------------------------------------------------------------------
	// References (not owned)
	//-------------------------------------------------------------------------
	Flux_MeshInstance* m_pxMesh = nullptr;
	Zenith_MaterialAsset* m_pxMaterial = nullptr;
	Flux_AnimationTexture* m_pxAnimationTexture = nullptr;
	Flux_InstanceBounds m_xBounds = {};
};
