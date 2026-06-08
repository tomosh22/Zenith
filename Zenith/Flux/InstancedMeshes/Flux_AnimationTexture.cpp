#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include <cmath>
#include <fstream>

//=============================================================================
// Constants
//=============================================================================

static constexpr uint32_t ZANT_MAGIC = 0x544E415A;  // 'ZANT'
static constexpr uint32_t ZANT_VERSION = 1;

//=============================================================================
// Helper Functions
//=============================================================================

uint32_t Flux_AnimationTexture::NextPowerOfTwo(uint32_t v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

// Convert float to half-float (IEEE 754 binary16)
static uint16_t FloatToHalf(float fValue)
{
	// Simple conversion - handle special cases
	if (fValue == 0.0f)
	{
		return 0;
	}

	uint32_t uBits = *reinterpret_cast<uint32_t*>(&fValue);
	uint32_t uSign = (uBits >> 16) & 0x8000;
	int32_t iExponent = ((uBits >> 23) & 0xFF) - 127 + 15;
	uint32_t uMantissa = uBits & 0x7FFFFF;

	if (iExponent <= 0)
	{
		// Denormalized or zero
		return static_cast<uint16_t>(uSign);
	}
	else if (iExponent >= 31)
	{
		// Infinity or NaN
		return static_cast<uint16_t>(uSign | 0x7C00);
	}

	return static_cast<uint16_t>(uSign | (iExponent << 10) | (uMantissa >> 13));
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

Flux_AnimationTexture::Flux_AnimationTexture()
{
}

Flux_AnimationTexture::~Flux_AnimationTexture()
{
	DestroyGPUResources();
}

//=============================================================================
// Baking
//=============================================================================

void Flux_AnimationTexture::EvaluateAnimationFrame(
	const Flux_MeshGeometry* pxMesh,
	const Zenith_SkeletonAsset* pxSkeleton,
	const Flux_AnimationClip* pxAnimation,
	float fTime,
	Zenith_Vector<Zenith_Maths::Vector4>& axOutPositions) const
{
	const uint32_t uNumVerts = pxMesh->GetNumVerts();
	const uint32_t uNumBones = pxSkeleton->GetNumBones();

	// Sample animation pose
	Flux_SkeletonPose xPose;
	xPose.Initialize(uNumBones);
	xPose.InitFromBindPose(*pxSkeleton);
	xPose.SampleFromClip(*pxAnimation, fTime, *pxSkeleton);

	// Compute model-space bone matrices from skeleton hierarchy
	Zenith_Maths::Matrix4 axModelSpaceMatrices[FLUX_MAX_BONES];

	// Iterate bones in hierarchy order (parent before children)
	for (uint32_t uBone = 0; uBone < uNumBones; ++uBone)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		Zenith_Maths::Matrix4 xLocalMatrix = xPose.GetLocalPose(uBone).ToMatrix();

		if (xBone.m_iParentIndex >= 0)
		{
			axModelSpaceMatrices[uBone] = axModelSpaceMatrices[xBone.m_iParentIndex] * xLocalMatrix;
		}
		else
		{
			axModelSpaceMatrices[uBone] = xLocalMatrix;
		}
	}

	// Compute skinning matrices (model space × inverse bind pose)
	Zenith_Maths::Matrix4 axSkinningMatrices[FLUX_MAX_BONES];
	for (uint32_t uBone = 0; uBone < uNumBones; ++uBone)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		axSkinningMatrices[uBone] = axModelSpaceMatrices[uBone] * xBone.m_xInverseBindPose;
	}

	// Transform each vertex using bone weights
	// Zenith_Vector has no resize-to-N-default; clear + reserve + PushBack each computed value.
	axOutPositions.Clear();
	axOutPositions.Reserve(uNumVerts);

	for (uint32_t uVert = 0; uVert < uNumVerts; ++uVert)
	{
		Zenith_Maths::Vector3 xOriginalPos = pxMesh->m_pxPositions[uVert];
		Zenith_Maths::Vector4 xSkinnedPos(0.0f);

		// Get bone weights for this vertex (4 bones max)
		const uint32_t* puBoneIDs = &pxMesh->m_puBoneIDs[uVert * MAX_BONES_PER_VERTEX];
		const float* pfWeights = &pxMesh->m_pfBoneWeights[uVert * MAX_BONES_PER_VERTEX];

		for (uint32_t i = 0; i < MAX_BONES_PER_VERTEX; ++i)
		{
			uint32_t uBoneID = puBoneIDs[i];
			float fWeight = pfWeights[i];

			if (fWeight > 0.0f && uBoneID < uNumBones)
			{
				Zenith_Maths::Vector4 xTransformed = axSkinningMatrices[uBoneID] * Zenith_Maths::Vector4(xOriginalPos, 1.0f);
				xSkinnedPos += xTransformed * fWeight;
			}
		}

		axOutPositions.PushBack(xSkinnedPos);
	}
}

bool Flux_AnimationTexture::BakeFromAnimations(
	const Flux_MeshGeometry* pxMesh,
	const Zenith_SkeletonAsset* pxSkeleton,
	const Zenith_Vector<Flux_AnimationClip*>& axAnimations,
	uint32_t uFramesPerSecond)
{
	if (!pxMesh || !pxSkeleton || axAnimations.GetSize() == 0)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Flux_AnimationTexture::BakeFromAnimations - Invalid input");
		return false;
	}

	if (!pxMesh->m_puBoneIDs || !pxMesh->m_pfBoneWeights)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Flux_AnimationTexture::BakeFromAnimations - Mesh has no bone weights");
		return false;
	}

	const uint32_t uNumVerts = pxMesh->GetNumVerts();
	const uint32_t uNumAnimations = axAnimations.GetSize();

	// Calculate total frames needed
	uint32_t uTotalFrames = 0;
	uint32_t uMaxFramesPerAnimation = 0;

	// Zenith_Vector has no resize-to-N-default; clear + reserve + PushBack each entry.
	m_axAnimations.Clear();
	m_axAnimations.Reserve(uNumAnimations);

	for (uint32_t uAnim = 0; uAnim < uNumAnimations; ++uAnim)
	{
		const Flux_AnimationClip* pxClip = axAnimations.Get(uAnim);
		float fDuration = pxClip->GetDuration();
		uint32_t uFrameCount = static_cast<uint32_t>(fDuration * uFramesPerSecond) + 1;

		AnimationInfo xInfo;
		xInfo.m_strName = pxClip->GetName();
		xInfo.m_uFirstFrame = uTotalFrames;
		xInfo.m_uFrameCount = uFrameCount;
		xInfo.m_fDuration = fDuration;
		xInfo.m_bLooping = pxClip->IsLooping();
		m_axAnimations.PushBack(xInfo);

		uTotalFrames += uFrameCount;
		uMaxFramesPerAnimation = std::max(uMaxFramesPerAnimation, uFrameCount);
	}

	// Setup header
	m_xHeader.m_uMagic = ZANT_MAGIC;
	m_xHeader.m_uVersion = ZANT_VERSION;
	m_xHeader.m_uVertexCount = uNumVerts;
	m_xHeader.m_uTextureWidth = NextPowerOfTwo(uNumVerts);
	m_xHeader.m_uTextureHeight = uTotalFrames;
	m_xHeader.m_uNumAnimations = uNumAnimations;
	m_xHeader.m_uFramesPerAnimation = uMaxFramesPerAnimation;
	m_xHeader.m_fFrameDuration = 1.0f / static_cast<float>(uFramesPerSecond);

	// Allocate texture data (RGBA16F = 4 × uint16 per pixel)
	// Zenith_Vector has no resize-to-N-default; reserve + PushBack(0) to grow with zero-fill.
	const uint32_t uPixelsTotal = m_xHeader.m_uTextureWidth * m_xHeader.m_uTextureHeight;
	const uint32_t uTexelCount = uPixelsTotal * 4;
	m_axTextureData.Clear();
	m_axTextureData.Reserve(uTexelCount);
	for (uint32_t i = 0; i < uTexelCount; ++i)
	{
		m_axTextureData.PushBack(0);
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture] Baking %u animations, %u total frames, texture %u x %u",
		uNumAnimations, uTotalFrames, m_xHeader.m_uTextureWidth, m_xHeader.m_uTextureHeight);

	// Bake each animation frame
	uint32_t uCurrentFrame = 0;
	Zenith_Vector<Zenith_Maths::Vector4> axFramePositions;

	for (uint32_t uAnim = 0; uAnim < uNumAnimations; ++uAnim)
	{
		const Flux_AnimationClip* pxClip = axAnimations.Get(uAnim);
		const AnimationInfo& xInfo = m_axAnimations.Get(uAnim);

		for (uint32_t uFrame = 0; uFrame < xInfo.m_uFrameCount; ++uFrame)
		{
			float fTime = (uFrame * m_xHeader.m_fFrameDuration);

			// Evaluate animation at this time
			EvaluateAnimationFrame(pxMesh, pxSkeleton, pxClip, fTime, axFramePositions);

			// Store positions in texture row
			uint32_t uRowOffset = uCurrentFrame * m_xHeader.m_uTextureWidth * 4;

			for (uint32_t uVert = 0; uVert < uNumVerts; ++uVert)
			{
				const Zenith_Maths::Vector4& xPos = axFramePositions.Get(uVert);
				uint32_t uPixelOffset = uRowOffset + uVert * 4;

				m_axTextureData.Get(uPixelOffset + 0) = FloatToHalf(xPos.x);
				m_axTextureData.Get(uPixelOffset + 1) = FloatToHalf(xPos.y);
				m_axTextureData.Get(uPixelOffset + 2) = FloatToHalf(xPos.z);
				m_axTextureData.Get(uPixelOffset + 3) = FloatToHalf(1.0f);  // W = 1 (or could store normal.x)
			}

			++uCurrentFrame;
		}

		Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture]   Baked '%s': %u frames (%.2fs)",
			xInfo.m_strName.c_str(), xInfo.m_uFrameCount, xInfo.m_fDuration);
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture] Baking complete. Texture size: %u KB",
		static_cast<uint32_t>((m_axTextureData.GetSize() * sizeof(uint16_t)) / 1024));

	return true;
}

//=============================================================================
// File I/O
//=============================================================================

bool Flux_AnimationTexture::Export(const std::string& strPath) const
{
	Zenith_DataStream xStream;

	// Write header using operator<<
	xStream << m_xHeader.m_uMagic;
	xStream << m_xHeader.m_uVersion;
	xStream << m_xHeader.m_uVertexCount;
	xStream << m_xHeader.m_uTextureWidth;
	xStream << m_xHeader.m_uTextureHeight;
	xStream << m_xHeader.m_uNumAnimations;
	xStream << m_xHeader.m_uFramesPerAnimation;
	xStream << m_xHeader.m_fFrameDuration;

	// Write animation info
	for (uint32_t i = 0; i < m_xHeader.m_uNumAnimations; ++i)
	{
		const AnimationInfo& xInfo = m_axAnimations.Get(i);
		xStream << xInfo.m_strName;
		xStream << xInfo.m_uFirstFrame;
		xStream << xInfo.m_uFrameCount;
		xStream << xInfo.m_fDuration;
		xStream << xInfo.m_bLooping;
	}

	// Write texture data size and raw data
	uint32_t uDataSize = m_axTextureData.GetSize();
	xStream << uDataSize;
	xStream.WriteData(m_axTextureData.GetDataPointer(), uDataSize * sizeof(uint16_t));

	// Save to file
	xStream.WriteToFile(strPath.c_str());

	Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture] Exported to %s", strPath.c_str());
	return true;
}

Flux_AnimationTexture* Flux_AnimationTexture::LoadFromFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());

	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[AnimationTexture] Failed to load: %s", strPath.c_str());
		return nullptr;
	}

	Flux_AnimationTexture* pxTexture = new Flux_AnimationTexture();

	// Read header using operator>>
	xStream >> pxTexture->m_xHeader.m_uMagic;
	if (pxTexture->m_xHeader.m_uMagic != ZANT_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[AnimationTexture] Invalid file format: %s", strPath.c_str());
		delete pxTexture;
		return nullptr;
	}

	xStream >> pxTexture->m_xHeader.m_uVersion;
	xStream >> pxTexture->m_xHeader.m_uVertexCount;
	xStream >> pxTexture->m_xHeader.m_uTextureWidth;
	xStream >> pxTexture->m_xHeader.m_uTextureHeight;
	xStream >> pxTexture->m_xHeader.m_uNumAnimations;
	xStream >> pxTexture->m_xHeader.m_uFramesPerAnimation;
	xStream >> pxTexture->m_xHeader.m_fFrameDuration;

	// Read animation info
	// Zenith_Vector has no resize-to-N-default; reserve + PushBack each parsed entry.
	pxTexture->m_axAnimations.Clear();
	pxTexture->m_axAnimations.Reserve(pxTexture->m_xHeader.m_uNumAnimations);
	for (uint32_t i = 0; i < pxTexture->m_xHeader.m_uNumAnimations; ++i)
	{
		AnimationInfo xInfo;
		xStream >> xInfo.m_strName;
		xStream >> xInfo.m_uFirstFrame;
		xStream >> xInfo.m_uFrameCount;
		xStream >> xInfo.m_fDuration;
		xStream >> xInfo.m_bLooping;
		pxTexture->m_axAnimations.PushBack(xInfo);
	}

	// Read texture data
	// Grow the blob to uDataSize (no resize-to-N-default), then read the raw bytes into it.
	uint32_t uDataSize;
	xStream >> uDataSize;
	pxTexture->m_axTextureData.Clear();
	pxTexture->m_axTextureData.Reserve(uDataSize);
	for (uint32_t i = 0; i < uDataSize; ++i)
	{
		pxTexture->m_axTextureData.PushBack(0);
	}
	xStream.ReadData(pxTexture->m_axTextureData.GetDataPointer(), uDataSize * sizeof(uint16_t));

	Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture] Loaded from %s (%u animations, %u x %u texture)",
		strPath.c_str(), pxTexture->m_xHeader.m_uNumAnimations,
		pxTexture->m_xHeader.m_uTextureWidth, pxTexture->m_xHeader.m_uTextureHeight);

	return pxTexture;
}

//=============================================================================
// Accessors
//=============================================================================

const Flux_AnimationTexture::AnimationInfo* Flux_AnimationTexture::GetAnimationInfo(uint32_t uIndex) const
{
	if (uIndex >= m_axAnimations.GetSize())
	{
		return nullptr;
	}
	return &m_axAnimations.Get(uIndex);
}

const Flux_AnimationTexture::AnimationInfo* Flux_AnimationTexture::FindAnimation(const std::string& strName) const
{
	for (uint32_t i = 0; i < m_axAnimations.GetSize(); ++i)
	{
		if (m_axAnimations.Get(i).m_strName == strName)
		{
			return &m_axAnimations.Get(i);
		}
	}
	return nullptr;
}

uint32_t Flux_AnimationTexture::GetFrameIndex(uint32_t uAnimIndex, float fNormalizedTime) const
{
	if (uAnimIndex >= m_axAnimations.GetSize())
	{
		return 0;
	}

	const AnimationInfo& xInfo = m_axAnimations.Get(uAnimIndex);

	// Clamp normalized time to [0, 1]
	fNormalizedTime = glm::clamp(fNormalizedTime, 0.0f, 1.0f);

	// Calculate frame index within this animation
	uint32_t uLocalFrame = static_cast<uint32_t>(fNormalizedTime * (xInfo.m_uFrameCount - 1));
	uLocalFrame = std::min(uLocalFrame, xInfo.m_uFrameCount - 1);

	return xInfo.m_uFirstFrame + uLocalFrame;
}

//=============================================================================
// GPU Resource Management
//=============================================================================

void Flux_AnimationTexture::CreateGPUResources()
{
	if (m_bGPUResourcesCreated || m_axTextureData.GetSize() == 0)
	{
		return;
	}

	// Setup surface info for RGBA16F texture
	Flux_SurfaceInfo xSurfaceInfo;
	xSurfaceInfo.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
	xSurfaceInfo.m_uWidth = m_xHeader.m_uTextureWidth;
	xSurfaceInfo.m_uHeight = m_xHeader.m_uTextureHeight;
	xSurfaceInfo.m_uDepth = 1;
	xSurfaceInfo.m_uNumMips = 1;
	xSurfaceInfo.m_uNumLayers = 1;
	xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;  // Required for shader sampling

	// Create VRAM and upload texture data
	m_xPositionTexture.m_xVRAMHandle = g_xEngine.VulkanMemory().CreateTextureVRAM(
		m_axTextureData.GetDataPointer(),
		xSurfaceInfo,
		false  // No mipmaps
	);

	// Create shader resource view for sampling
	m_xPositionTexture.m_xSRV = g_xEngine.VulkanMemory().CreateShaderResourceView(
		m_xPositionTexture.m_xVRAMHandle,
		xSurfaceInfo
	);

	m_xPositionTexture.m_xSurfaceInfo = xSurfaceInfo;
	m_bGPUResourcesCreated = true;

	Zenith_Log(LOG_CATEGORY_MESH, "[AnimationTexture] Created GPU texture (%u x %u)",
		m_xHeader.m_uTextureWidth, m_xHeader.m_uTextureHeight);
}

void Flux_AnimationTexture::DestroyGPUResources()
{
	if (!m_bGPUResourcesCreated)
	{
		return;
	}

	if (m_xPositionTexture.m_xVRAMHandle.IsValid())
	{
		Flux_VRAM* pxVRAM = g_xEngine.Vulkan().GetVRAM(m_xPositionTexture.m_xVRAMHandle);
		g_xEngine.VulkanMemory().QueueVRAMDeletion(pxVRAM, m_xPositionTexture.m_xVRAMHandle, m_xPositionTexture.m_xSRV.m_xImageViewHandle);
	}

	m_xPositionTexture = Flux_Texture();
	m_bGPUResourcesCreated = false;
}
