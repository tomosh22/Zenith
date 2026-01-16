#include "Zenith.h"

#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

//=============================================================================
// VkDrawIndexedIndirectCommand structure (matches Vulkan spec)
//=============================================================================
struct Flux_DrawIndexedIndirectCommand
{
	uint32_t m_uIndexCount;
	uint32_t m_uInstanceCount;
	uint32_t m_uFirstIndex;
	int32_t m_iVertexOffset;
	uint32_t m_uFirstInstance;
};
static_assert(sizeof(Flux_DrawIndexedIndirectCommand) == 20, "DrawIndexedIndirectCommand must be 20 bytes");

//=============================================================================
// Constructor / Destructor
//=============================================================================

Flux_InstanceGroup::Flux_InstanceGroup()
{
}

Flux_InstanceGroup::~Flux_InstanceGroup()
{
	DestroyGPUBuffers();
}

//=============================================================================
// Configuration
//=============================================================================

void Flux_InstanceGroup::SetMesh(Flux_MeshInstance* pxMesh)
{
	m_pxMesh = pxMesh;
}

void Flux_InstanceGroup::SetMaterial(Flux_MaterialAsset* pxMaterial)
{
	m_pxMaterial = pxMaterial;
}

void Flux_InstanceGroup::SetAnimationTexture(Flux_AnimationTexture* pxAnimTex)
{
	m_pxAnimationTexture = pxAnimTex;
}

void Flux_InstanceGroup::SetBounds(const Flux_InstanceBounds& xBounds)
{
	m_xBounds = xBounds;
}

//=============================================================================
// Instance Management
//=============================================================================

uint32_t Flux_InstanceGroup::AddInstance()
{
	Zenith_Assert(m_uInstanceCount < uMAX_INSTANCES, "Instance group at maximum capacity");

	uint32_t uID;

	// Reuse a recycled ID if available
	if (!m_auFreeIDs.empty())
	{
		uID = m_auFreeIDs.back();
		m_auFreeIDs.pop_back();
	}
	else
	{
		// Allocate new slot
		uID = m_uInstanceCount;
		if (uID >= m_uCapacity)
		{
			Reserve(m_uCapacity == 0 ? 1024 : m_uCapacity * 2);
		}
	}

	// Initialize instance data
	m_axTransforms[uID] = glm::identity<Zenith_Maths::Matrix4>();

	Flux_InstanceAnimData xAnimData = {};
	xAnimData.m_uAnimationIndex = 0;
	xAnimData.m_uFrameCount = 1;
	xAnimData.m_fAnimTime = 0.0f;
	xAnimData.m_uColorTint = 0xFFFFFFFF;  // White, full alpha
	xAnimData.m_uFlags = 1;               // Enabled
	m_axAnimData[uID] = xAnimData;

	m_abDirty[uID] = true;
	m_uInstanceCount++;
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;

	return uID;
}

void Flux_InstanceGroup::RemoveInstance(uint32_t uInstanceID)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	Zenith_Assert(m_uInstanceCount > 0, "Cannot remove from empty group");

	// Mark as disabled (won't be culled or rendered)
	m_axAnimData[uInstanceID].m_uFlags = 0;
	m_abDirty[uInstanceID] = true;
	m_bAnimDataDirty = true;

	// Add to free list for reuse
	m_auFreeIDs.push_back(uInstanceID);
	m_uInstanceCount--;
}

void Flux_InstanceGroup::Clear()
{
	m_uInstanceCount = 0;
	m_uVisibleCount = 0;
	m_auFreeIDs.clear();

	// Mark all as dirty for next upload
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceTransform(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	m_axTransforms[uInstanceID] = xMatrix;
	MarkDirty(uInstanceID);
	m_bTransformsDirty = true;
}

void Flux_InstanceGroup::SetInstanceAnimation(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime, uint32_t uFrameCount)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	Flux_InstanceAnimData& xData = m_axAnimData[uInstanceID];
	xData.m_uAnimationIndex = static_cast<uint16_t>(uAnimIndex);
	xData.m_uFrameCount = static_cast<uint16_t>(uFrameCount);
	xData.m_fAnimTime = fNormalizedTime;
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	m_axAnimData[uInstanceID].m_uColorTint = PackColorRGBA8(xColor);
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	m_axAnimData[uInstanceID].m_uFlags = bEnabled ? 1 : 0;
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

//=============================================================================
// Bulk Operations
//=============================================================================

void Flux_InstanceGroup::AdvanceAllAnimations(float fDt, float fAnimDuration)
{
	if (fAnimDuration <= 0.0f)
		return;

	float fNormalizedDt = fDt / fAnimDuration;

	for (uint32_t i = 0; i < m_uCapacity; ++i)
	{
		if (m_axAnimData[i].m_uFlags != 0)  // Only active instances
		{
			float fNewTime = m_axAnimData[i].m_fAnimTime + fNormalizedDt;
			m_axAnimData[i].m_fAnimTime = fmod(fNewTime, 1.0f);
		}
	}

	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::Reserve(uint32_t uCapacity)
{
	if (uCapacity <= m_uCapacity)
		return;

	uCapacity = std::min(uCapacity, uMAX_INSTANCES);

	m_axTransforms.resize(uCapacity);
	m_axAnimData.resize(uCapacity);
	m_abDirty.resize(uCapacity);

	// Initialize new slots
	for (uint32_t i = m_uCapacity; i < uCapacity; ++i)
	{
		m_axTransforms[i] = glm::identity<Zenith_Maths::Matrix4>();
		m_axAnimData[i] = {};
		m_abDirty[i] = false;
	}

	m_uCapacity = uCapacity;

	// Recreate GPU buffers with new capacity
	if (m_bBuffersInitialised)
	{
		DestroyGPUBuffers();
	}
	InitialiseGPUBuffers();
}

//=============================================================================
// GPU Buffer Management
//=============================================================================

void Flux_InstanceGroup::InitialiseGPUBuffers()
{
	if (m_uCapacity == 0)
		return;

	// Transform buffer (mat4 per instance)
	const size_t ulTransformSize = m_uCapacity * sizeof(Zenith_Maths::Matrix4);
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(nullptr, ulTransformSize, m_xTransformBuffer);

	// Animation data buffer
	const size_t ulAnimDataSize = m_uCapacity * sizeof(Flux_InstanceAnimData);
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(nullptr, ulAnimDataSize, m_xAnimDataBuffer);

	// Visible index buffer (worst case: all visible)
	const size_t ulVisibleIndexSize = m_uCapacity * sizeof(uint32_t);
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(nullptr, ulVisibleIndexSize, m_xVisibleIndexBuffer);

	// Bounds buffer (single bounding sphere, replicated conceptually but stored once)
	// Actually we store per-instance bounds in case we want per-instance bounds later
	const size_t ulBoundsSize = sizeof(Flux_InstanceBounds);
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(&m_xBounds, ulBoundsSize, m_xBoundsBuffer);

	// Indirect draw command buffer
	Zenith_Vulkan_MemoryManager::InitialiseIndirectBuffer(sizeof(Flux_DrawIndexedIndirectCommand), m_xIndirectBuffer);

	// Visible count buffer (single uint32 for atomic counter)
	uint32_t uZero = 0;
	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(&uZero, sizeof(uint32_t), m_xVisibleCountBuffer);

	m_bBuffersInitialised = true;
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;

	Zenith_Log(LOG_CATEGORY_RENDERER, "[InstanceGroup] Initialised GPU buffers for %u instances", m_uCapacity);
}

void Flux_InstanceGroup::DestroyGPUBuffers()
{
	if (!m_bBuffersInitialised)
		return;

	// Queue buffers for deferred deletion
	if (m_xTransformBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(m_xTransformBuffer);

	if (m_xAnimDataBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(m_xAnimDataBuffer);

	if (m_xVisibleIndexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(m_xVisibleIndexBuffer);

	if (m_xBoundsBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(m_xBoundsBuffer);

	if (m_xIndirectBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyIndirectBuffer(m_xIndirectBuffer);

	if (m_xVisibleCountBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(m_xVisibleCountBuffer);

	m_bBuffersInitialised = false;
}

void Flux_InstanceGroup::UpdateGPUBuffers()
{
	if (!m_bBuffersInitialised || m_uCapacity == 0)
		return;

	// Upload transform data if dirty
	if (m_bTransformsDirty)
	{
		const size_t ulSize = m_uCapacity * sizeof(Zenith_Maths::Matrix4);
		Zenith_Vulkan_MemoryManager::UploadBufferData(
			m_xTransformBuffer.GetBuffer().m_xVRAMHandle,
			m_axTransforms.data(),
			ulSize);
		m_bTransformsDirty = false;
	}

	// Upload animation data if dirty
	if (m_bAnimDataDirty)
	{
		const size_t ulSize = m_uCapacity * sizeof(Flux_InstanceAnimData);
		Zenith_Vulkan_MemoryManager::UploadBufferData(
			m_xAnimDataBuffer.GetBuffer().m_xVRAMHandle,
			m_axAnimData.data(),
			ulSize);
		m_bAnimDataDirty = false;
	}

	// Phase 1: Populate visible index buffer with sequential indices (no GPU culling)
	// This will be replaced by compute shader output in Phase 2
	{
		std::vector<uint32_t> auVisibleIndices;
		auVisibleIndices.reserve(m_uInstanceCount);

		for (uint32_t i = 0; i < m_uCapacity; ++i)
		{
			// Only include enabled instances
			if (m_axAnimData[i].m_uFlags != 0)
			{
				auVisibleIndices.push_back(i);
			}
		}

		if (!auVisibleIndices.empty())
		{
			const size_t ulSize = auVisibleIndices.size() * sizeof(uint32_t);
			Zenith_Vulkan_MemoryManager::UploadBufferData(
				m_xVisibleIndexBuffer.GetBuffer().m_xVRAMHandle,
				auVisibleIndices.data(),
				ulSize);
		}

		m_uVisibleCount = static_cast<uint32_t>(auVisibleIndices.size());
	}

	// Clear dirty flags
	for (uint32_t i = 0; i < m_uCapacity; ++i)
	{
		m_abDirty[i] = false;
	}
}

void Flux_InstanceGroup::ResetVisibleCount()
{
	if (!m_bBuffersInitialised)
		return;

	// Reset the atomic counter to 0 for culling pass
	uint32_t uZero = 0;
	Zenith_Vulkan_MemoryManager::UploadBufferData(
		m_xVisibleCountBuffer.GetBuffer().m_xVRAMHandle,
		&uZero,
		sizeof(uint32_t));

	// Also reset the indirect command instance count
	// The culling shader will write the actual visible count
	if (m_pxMesh)
	{
		Flux_DrawIndexedIndirectCommand xCmd = {};
		xCmd.m_uIndexCount = m_pxMesh->GetNumIndices();
		xCmd.m_uInstanceCount = 0;  // Will be set by culling
		xCmd.m_uFirstIndex = 0;
		xCmd.m_iVertexOffset = 0;
		xCmd.m_uFirstInstance = 0;

		Zenith_Vulkan_MemoryManager::UploadBufferData(
			m_xIndirectBuffer.GetBuffer().m_xVRAMHandle,
			&xCmd,
			sizeof(xCmd));
	}

	m_uVisibleCount = 0;
}

//=============================================================================
// Helper Functions
//=============================================================================

void Flux_InstanceGroup::MarkDirty(uint32_t uInstanceID)
{
	if (uInstanceID < m_uCapacity)
	{
		m_abDirty[uInstanceID] = true;
	}
}

uint32_t Flux_InstanceGroup::PackColorRGBA8(const Zenith_Maths::Vector4& xColor)
{
	uint8_t uR = static_cast<uint8_t>(glm::clamp(xColor.r, 0.0f, 1.0f) * 255.0f);
	uint8_t uG = static_cast<uint8_t>(glm::clamp(xColor.g, 0.0f, 1.0f) * 255.0f);
	uint8_t uB = static_cast<uint8_t>(glm::clamp(xColor.b, 0.0f, 1.0f) * 255.0f);
	uint8_t uA = static_cast<uint8_t>(glm::clamp(xColor.a, 0.0f, 1.0f) * 255.0f);

	return (uA << 24) | (uB << 16) | (uG << 8) | uR;
}
