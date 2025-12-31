#include "Zenith.h"
#include "Flux_SkeletonInstance.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"

//=============================================================================
// Destructor
//=============================================================================
Flux_SkeletonInstance::~Flux_SkeletonInstance()
{
	Destroy();
}

//=============================================================================
// Move Constructor
//=============================================================================
Flux_SkeletonInstance::Flux_SkeletonInstance(Flux_SkeletonInstance&& xOther) noexcept
	: m_pxSourceSkeleton(xOther.m_pxSourceSkeleton)
	, m_uNumBones(xOther.m_uNumBones)
	, m_xBoneBuffer(std::move(xOther.m_xBoneBuffer))
	, m_bGPUResourcesInitialized(xOther.m_bGPUResourcesInitialized)
{
	// Copy fixed-size arrays
	for (uint32_t i = 0; i < MAX_BONES; ++i)
	{
		m_axLocalPositions[i] = xOther.m_axLocalPositions[i];
		m_axLocalRotations[i] = xOther.m_axLocalRotations[i];
		m_axLocalScales[i] = xOther.m_axLocalScales[i];
		m_axModelSpaceTransforms[i] = xOther.m_axModelSpaceTransforms[i];
		m_axSkinningMatrices[i] = xOther.m_axSkinningMatrices[i];
	}

	// Clear source to prevent double destruction
	xOther.m_pxSourceSkeleton = nullptr;
	xOther.m_uNumBones = 0;
	xOther.m_bGPUResourcesInitialized = false;
}

//=============================================================================
// Move Assignment
//=============================================================================
Flux_SkeletonInstance& Flux_SkeletonInstance::operator=(Flux_SkeletonInstance&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Clean up existing resources
		Destroy();

		// Move data
		m_pxSourceSkeleton = xOther.m_pxSourceSkeleton;
		m_uNumBones = xOther.m_uNumBones;
		m_xBoneBuffer = std::move(xOther.m_xBoneBuffer);
		m_bGPUResourcesInitialized = xOther.m_bGPUResourcesInitialized;

		// Copy fixed-size arrays
		for (uint32_t i = 0; i < MAX_BONES; ++i)
		{
			m_axLocalPositions[i] = xOther.m_axLocalPositions[i];
			m_axLocalRotations[i] = xOther.m_axLocalRotations[i];
			m_axLocalScales[i] = xOther.m_axLocalScales[i];
			m_axModelSpaceTransforms[i] = xOther.m_axModelSpaceTransforms[i];
			m_axSkinningMatrices[i] = xOther.m_axSkinningMatrices[i];
		}

		// Clear source
		xOther.m_pxSourceSkeleton = nullptr;
		xOther.m_uNumBones = 0;
		xOther.m_bGPUResourcesInitialized = false;
	}
	return *this;
}

//=============================================================================
// Factory Method
//=============================================================================
Flux_SkeletonInstance* Flux_SkeletonInstance::CreateFromAsset(Zenith_SkeletonAsset* pxAsset, bool bUploadToGPU)
{
	if (!pxAsset)
	{
		Zenith_Error("[Flux_SkeletonInstance] Cannot create instance from null skeleton asset");
		return nullptr;
	}

	if (pxAsset->GetNumBones() == 0)
	{
		Zenith_Error("[Flux_SkeletonInstance] Cannot create instance from skeleton with 0 bones");
		return nullptr;
	}

	if (pxAsset->GetNumBones() > MAX_BONES)
	{
		Zenith_Error("[Flux_SkeletonInstance] Skeleton has %u bones, max is %u",
			pxAsset->GetNumBones(), MAX_BONES);
		return nullptr;
	}

	Flux_SkeletonInstance* pxInstance = new Flux_SkeletonInstance();
	pxInstance->m_pxSourceSkeleton = pxAsset;
	pxInstance->m_uNumBones = pxAsset->GetNumBones();

	// Initialize all arrays to identity/default values
	for (uint32_t i = 0; i < MAX_BONES; ++i)
	{
		pxInstance->m_axLocalPositions[i] = Zenith_Maths::Vector3(0.0f);
		pxInstance->m_axLocalRotations[i] = glm::identity<Zenith_Maths::Quat>();
		pxInstance->m_axLocalScales[i] = Zenith_Maths::Vector3(1.0f);
		pxInstance->m_axModelSpaceTransforms[i] = glm::identity<Zenith_Maths::Matrix4>();
		pxInstance->m_axSkinningMatrices[i] = glm::identity<Zenith_Maths::Matrix4>();
	}

	// Copy bind pose values from skeleton asset
	pxInstance->SetToBindPose();

	// Create GPU buffer for bone matrices (skip for CPU-only use, e.g. unit tests)
	if (bUploadToGPU)
	{
		Flux_MemoryManager::InitialiseDynamicConstantBuffer(
			nullptr,
			MAX_BONES * sizeof(Zenith_Maths::Matrix4),
			pxInstance->m_xBoneBuffer
		);
		pxInstance->m_bGPUResourcesInitialized = true;

		// Compute initial skinning matrices and upload to ALL frame buffers
		// This prevents flickering by ensuring all triple-buffered copies have valid data
		pxInstance->ComputeSkinningMatrices();
		pxInstance->UploadToAllFrameBuffers();
	}
	else
	{
		pxInstance->m_bGPUResourcesInitialized = false;
		pxInstance->ComputeSkinningMatrices();
	}

	Zenith_Log("[Flux_SkeletonInstance] Created instance with %u bones", pxInstance->m_uNumBones);

	// Debug: log first bone's skinning matrix (should be close to identity for bind pose)
	if (pxInstance->m_uNumBones > 0)
	{
		const Zenith_Maths::Matrix4& xSkinMat = pxInstance->m_axSkinningMatrices[0];
		Zenith_Log("[Flux_SkeletonInstance]   Bone 0 skinning matrix row0: (%.3f, %.3f, %.3f, %.3f)",
			xSkinMat[0][0], xSkinMat[1][0], xSkinMat[2][0], xSkinMat[3][0]);
		Zenith_Log("[Flux_SkeletonInstance]   Bone 0 skinning matrix row3 (translation): (%.3f, %.3f, %.3f, %.3f)",
			xSkinMat[0][3], xSkinMat[1][3], xSkinMat[2][3], xSkinMat[3][3]);
	}

	return pxInstance;
}

//=============================================================================
// Destroy
//=============================================================================
void Flux_SkeletonInstance::Destroy()
{
	if (m_bGPUResourcesInitialized)
	{
		Flux_MemoryManager::DestroyDynamicConstantBuffer(m_xBoneBuffer);
		m_bGPUResourcesInitialized = false;
	}

	m_pxSourceSkeleton = nullptr;
	m_uNumBones = 0;
}

//=============================================================================
// SetToBindPose
//=============================================================================
void Flux_SkeletonInstance::SetToBindPose()
{
	if (!m_pxSourceSkeleton)
	{
		return;
	}

	// Copy bind pose transforms from skeleton asset
	for (uint32_t i = 0; i < m_uNumBones && i < MAX_BONES; ++i)
	{
		const Zenith_SkeletonAsset::Bone& xBone = m_pxSourceSkeleton->GetBone(i);
		m_axLocalPositions[i] = xBone.m_xBindPosition;
		m_axLocalRotations[i] = xBone.m_xBindRotation;
		m_axLocalScales[i] = xBone.m_xBindScale;
	}
}

//=============================================================================
// SetBoneLocalTransform
//=============================================================================
void Flux_SkeletonInstance::SetBoneLocalTransform(uint32_t uBoneIndex,
	const Zenith_Maths::Vector3& xPos,
	const Zenith_Maths::Quat& xRot,
	const Zenith_Maths::Vector3& xScale)
{
	if (uBoneIndex >= m_uNumBones || uBoneIndex >= MAX_BONES)
	{
		Zenith_Warning("[Flux_SkeletonInstance] SetBoneLocalTransform: bone index %u out of range (max %u)",
			uBoneIndex, m_uNumBones);
		return;
	}

	m_axLocalPositions[uBoneIndex] = xPos;
	m_axLocalRotations[uBoneIndex] = xRot;
	m_axLocalScales[uBoneIndex] = xScale;
}

//=============================================================================
// Pose Accessors
//=============================================================================
const Zenith_Maths::Vector3& Flux_SkeletonInstance::GetBoneLocalPosition(uint32_t uBoneIndex) const
{
	static const Zenith_Maths::Vector3 s_xZero(0.0f);
	if (uBoneIndex >= m_uNumBones || uBoneIndex >= MAX_BONES)
	{
		return s_xZero;
	}
	return m_axLocalPositions[uBoneIndex];
}

const Zenith_Maths::Quat& Flux_SkeletonInstance::GetBoneLocalRotation(uint32_t uBoneIndex) const
{
	static const Zenith_Maths::Quat s_xIdentity = glm::identity<Zenith_Maths::Quat>();
	if (uBoneIndex >= m_uNumBones || uBoneIndex >= MAX_BONES)
	{
		return s_xIdentity;
	}
	return m_axLocalRotations[uBoneIndex];
}

const Zenith_Maths::Vector3& Flux_SkeletonInstance::GetBoneLocalScale(uint32_t uBoneIndex) const
{
	static const Zenith_Maths::Vector3 s_xOne(1.0f);
	if (uBoneIndex >= m_uNumBones || uBoneIndex >= MAX_BONES)
	{
		return s_xOne;
	}
	return m_axLocalScales[uBoneIndex];
}

//=============================================================================
// GetNumBones
//=============================================================================
uint32_t Flux_SkeletonInstance::GetNumBones() const
{
	return m_uNumBones;
}

//=============================================================================
// GetBoneModelTransform
//=============================================================================
const Zenith_Maths::Matrix4& Flux_SkeletonInstance::GetBoneModelTransform(uint32_t uBoneIndex) const
{
	static const Zenith_Maths::Matrix4 s_xIdentity = glm::identity<Zenith_Maths::Matrix4>();
	if (uBoneIndex >= MAX_BONES)
	{
		return s_xIdentity;
	}
	return m_axModelSpaceTransforms[uBoneIndex];
}

//=============================================================================
// ComposeTransformMatrix
//=============================================================================
Zenith_Maths::Matrix4 Flux_SkeletonInstance::ComposeTransformMatrix(
	const Zenith_Maths::Vector3& xPos,
	const Zenith_Maths::Quat& xRot,
	const Zenith_Maths::Vector3& xScale)
{
	// TRS order: Translation * Rotation * Scale
	Zenith_Maths::Matrix4 xResult = glm::translate(glm::identity<Zenith_Maths::Matrix4>(), xPos);
	xResult *= glm::mat4_cast(xRot);
	xResult = glm::scale(xResult, xScale);
	return xResult;
}

//=============================================================================
// ComputeBoneModelTransform
//=============================================================================
Zenith_Maths::Matrix4 Flux_SkeletonInstance::ComputeBoneModelTransform(uint32_t uBoneIndex) const
{
	if (!m_pxSourceSkeleton || uBoneIndex >= m_uNumBones)
	{
		return glm::identity<Zenith_Maths::Matrix4>();
	}

	// Get local transform for this bone
	Zenith_Maths::Matrix4 xLocalTransform = ComposeTransformMatrix(
		m_axLocalPositions[uBoneIndex],
		m_axLocalRotations[uBoneIndex],
		m_axLocalScales[uBoneIndex]
	);

	// Walk up the parent chain to accumulate transforms
	const Zenith_SkeletonAsset::Bone& xBone = m_pxSourceSkeleton->GetBone(uBoneIndex);
	int32_t iParentIndex = xBone.m_iParentIndex;

	if (iParentIndex == Zenith_SkeletonAsset::INVALID_BONE_INDEX)
	{
		// Root bone - local transform is the model transform
		return xLocalTransform;
	}

	// Recursively compute parent's model transform
	// Note: This could be optimized by computing in order from roots to leaves
	// and caching results, but this is simpler and works for now
	Zenith_Maths::Matrix4 xParentModelTransform = ComputeBoneModelTransform(static_cast<uint32_t>(iParentIndex));

	return xParentModelTransform * xLocalTransform;
}

//=============================================================================
// ComputeSkinningMatrices
//=============================================================================
void Flux_SkeletonInstance::ComputeSkinningMatrices()
{
	if (!m_pxSourceSkeleton)
	{
		return;
	}

	// Debug: log once during animation playback
	static bool s_bLoggedSkinningDebug = false;

	// Compute model-space transforms for all bones
	// We process bones in order, which works correctly because parent indices
	// are always less than child indices (bones are stored in hierarchical order)
	for (uint32_t i = 0; i < m_uNumBones && i < MAX_BONES; ++i)
	{
		const Zenith_SkeletonAsset::Bone& xBone = m_pxSourceSkeleton->GetBone(i);

		// Get local transform for this bone
		Zenith_Maths::Matrix4 xLocalTransform = ComposeTransformMatrix(
			m_axLocalPositions[i],
			m_axLocalRotations[i],
			m_axLocalScales[i]
		);

		// Compute model-space transform
		if (xBone.m_iParentIndex == Zenith_SkeletonAsset::INVALID_BONE_INDEX)
		{
			// Root bone - local transform is the model transform
			m_axModelSpaceTransforms[i] = xLocalTransform;
		}
		else
		{
			// Child bone - multiply by parent's model transform
			// Parent is guaranteed to be computed already since indices are in hierarchical order
			m_axModelSpaceTransforms[i] = m_axModelSpaceTransforms[xBone.m_iParentIndex] * xLocalTransform;
		}

		// Compute skinning matrix: modelSpace * inverseBindPose
		m_axSkinningMatrices[i] = m_axModelSpaceTransforms[i] * xBone.m_xInverseBindPose;

		// Debug: Log skinning matrix details for first few bones
		if (!s_bLoggedSkinningDebug && i < 3)
		{
			// Extract scale from matrix by computing column lengths
			float fLocalScaleX = glm::length(Zenith_Maths::Vector3(xLocalTransform[0]));
			float fLocalScaleY = glm::length(Zenith_Maths::Vector3(xLocalTransform[1]));
			float fLocalScaleZ = glm::length(Zenith_Maths::Vector3(xLocalTransform[2]));

			float fModelScaleX = glm::length(Zenith_Maths::Vector3(m_axModelSpaceTransforms[i][0]));
			float fModelScaleY = glm::length(Zenith_Maths::Vector3(m_axModelSpaceTransforms[i][1]));
			float fModelScaleZ = glm::length(Zenith_Maths::Vector3(m_axModelSpaceTransforms[i][2]));

			float fInvBindScaleX = glm::length(Zenith_Maths::Vector3(xBone.m_xInverseBindPose[0]));
			float fInvBindScaleY = glm::length(Zenith_Maths::Vector3(xBone.m_xInverseBindPose[1]));
			float fInvBindScaleZ = glm::length(Zenith_Maths::Vector3(xBone.m_xInverseBindPose[2]));

			float fSkinScaleX = glm::length(Zenith_Maths::Vector3(m_axSkinningMatrices[i][0]));
			float fSkinScaleY = glm::length(Zenith_Maths::Vector3(m_axSkinningMatrices[i][1]));
			float fSkinScaleZ = glm::length(Zenith_Maths::Vector3(m_axSkinningMatrices[i][2]));

			Zenith_Log("[ComputeSkinning] Bone %u '%s':", i, xBone.m_strName.c_str());
			Zenith_Log("  LocalScale input: (%.3f, %.3f, %.3f)", m_axLocalScales[i].x, m_axLocalScales[i].y, m_axLocalScales[i].z);
			Zenith_Log("  LocalTrans scale (from cols): (%.3f, %.3f, %.3f)", fLocalScaleX, fLocalScaleY, fLocalScaleZ);
			Zenith_Log("  ModelSpace scale (from cols): (%.3f, %.3f, %.3f)", fModelScaleX, fModelScaleY, fModelScaleZ);
			Zenith_Log("  InvBindPose scale (from cols): (%.3f, %.3f, %.3f)", fInvBindScaleX, fInvBindScaleY, fInvBindScaleZ);
			Zenith_Log("  Skinning scale (from cols): (%.3f, %.3f, %.3f)", fSkinScaleX, fSkinScaleY, fSkinScaleZ);
			Zenith_Log("  Skinning translation: (%.3f, %.3f, %.3f)",
				m_axSkinningMatrices[i][3][0], m_axSkinningMatrices[i][3][1], m_axSkinningMatrices[i][3][2]);
		}
	}

	if (!s_bLoggedSkinningDebug)
	{
		s_bLoggedSkinningDebug = true;
	}

	// Fill remaining slots with identity matrices
	for (uint32_t i = m_uNumBones; i < MAX_BONES; ++i)
	{
		m_axSkinningMatrices[i] = glm::identity<Zenith_Maths::Matrix4>();
	}
}

//=============================================================================
// UploadToGPU
//=============================================================================
void Flux_SkeletonInstance::UploadToGPU()
{
	if (!m_bGPUResourcesInitialized)
	{
		Zenith_Warning("[Flux_SkeletonInstance] UploadToGPU called but GPU resources not initialized");
		return;
	}

	Flux_MemoryManager::UploadBufferData(
		m_xBoneBuffer.GetBuffer().m_xVRAMHandle,
		m_axSkinningMatrices,
		MAX_BONES * sizeof(Zenith_Maths::Matrix4)
	);
}

//=============================================================================
// UploadToAllFrameBuffers
//=============================================================================
void Flux_SkeletonInstance::UploadToAllFrameBuffers()
{
	if (!m_bGPUResourcesInitialized)
	{
		Zenith_Warning("[Flux_SkeletonInstance] UploadToAllFrameBuffers called but GPU resources not initialized");
		return;
	}

	// Upload to all triple-buffered frame copies to prevent flickering
	// This ensures all frame buffers have valid bone data from initialization
	for (uint32_t uFrame = 0; uFrame < MAX_FRAMES_IN_FLIGHT; uFrame++)
	{
		Flux_MemoryManager::UploadBufferData(
			m_xBoneBuffer.GetBufferForFrameInFlight(uFrame).m_xVRAMHandle,
			m_axSkinningMatrices,
			MAX_BONES * sizeof(Zenith_Maths::Matrix4)
		);
	}
}
