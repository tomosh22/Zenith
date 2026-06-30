#include "Zenith.h"
#include "Flux_AnimationClip.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

//=============================================================================
// Timestamped-keyframe vector serialization helpers (see Flux_AnimationClip.h).
// On-disk format unchanged: uint32 count, then per key the value components then the
// float time. Byte-identical to the former hand-rolled count+loop blocks.
//=============================================================================
void Flux_WriteVec3Keys(Zenith_DataStream& xStream, const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xKeys)
{
	xStream << static_cast<uint32_t>(xKeys.GetSize());
	for (const auto& xKey : xKeys)
	{
		xStream << xKey.first.x;
		xStream << xKey.first.y;
		xStream << xKey.first.z;
		xStream << xKey.second;
	}
}

void Flux_ReadVec3Keys(Zenith_DataStream& xStream, Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xKeys)
{
	uint32_t uCount = 0;
	xStream >> uCount;
	xKeys.Clear();
	xKeys.Reserve(uCount);
	for (u_int i = 0; i < uCount; ++i)
	{
		std::pair<Zenith_Maths::Vector3, float> xKey;
		xStream >> xKey.first.x;
		xStream >> xKey.first.y;
		xStream >> xKey.first.z;
		xStream >> xKey.second;
		xKeys.PushBack(xKey);
	}
}

void Flux_WriteQuatKeys(Zenith_DataStream& xStream, const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys)
{
	xStream << static_cast<uint32_t>(xKeys.GetSize());
	for (const auto& xKey : xKeys)
	{
		xStream << xKey.first.w;
		xStream << xKey.first.x;
		xStream << xKey.first.y;
		xStream << xKey.first.z;
		xStream << xKey.second;
	}
}

void Flux_ReadQuatKeys(Zenith_DataStream& xStream, Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys)
{
	uint32_t uCount = 0;
	xStream >> uCount;
	xKeys.Clear();
	xKeys.Reserve(uCount);
	for (u_int i = 0; i < uCount; ++i)
	{
		std::pair<Zenith_Maths::Quat, float> xKey;
		xStream >> xKey.first.w;
		xStream >> xKey.first.x;
		xStream >> xKey.first.y;
		xStream >> xKey.first.z;
		xStream >> xKey.second;
		xKeys.PushBack(xKey);
	}
}

//=============================================================================
// Flux_AnimationEvent
//=============================================================================
void Flux_AnimationEvent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_fNormalizedTime;
	xStream << m_strEventName;
	xStream << m_xData.x;
	xStream << m_xData.y;
	xStream << m_xData.z;
	xStream << m_xData.w;
}

void Flux_AnimationEvent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_fNormalizedTime;
	xStream >> m_strEventName;
	xStream >> m_xData.x;
	xStream >> m_xData.y;
	xStream >> m_xData.z;
	xStream >> m_xData.w;
}

//=============================================================================
// Flux_AnimationClipMetadata
//=============================================================================
void Flux_AnimationClipMetadata::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_fDuration;
	xStream << m_uTicksPerSecond;
	xStream << m_bLooping;
	xStream << m_fBlendInTime;
	xStream << m_fBlendOutTime;
}

void Flux_AnimationClipMetadata::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strName;
	xStream >> m_fDuration;
	xStream >> m_uTicksPerSecond;
	xStream >> m_bLooping;
	xStream >> m_fBlendInTime;
	xStream >> m_fBlendOutTime;
}

//=============================================================================
// Flux_RootMotion
//=============================================================================

// Walk the keyframe list to find the bracket containing fTime, then call the
// caller-supplied per-channel interpolator. Empty / single-keyframe / past-end
// cases short-circuit before invoking the interpolator. fnInterp must accept
// (a, b, t) and return the interpolated value (mix for vectors, slerp for
// quaternions).
template<typename T, typename InterpFn>
static T SampleRootMotionDeltas(const Zenith_Vector<std::pair<T, float>>& xKeys,
								float fTime, bool bEnabled, const T& xIdentity,
								InterpFn fnInterp)
{
	if (!bEnabled || xKeys.GetSize() == 0) return xIdentity;
	if (xKeys.GetSize() == 1)              return xKeys.Get(0).first;

	for (u_int i = 0; i < xKeys.GetSize() - 1; ++i)
	{
		if (fTime < xKeys.Get(i + 1).second)
		{
			float fTimeDelta = xKeys.Get(i + 1).second - xKeys.Get(i).second;
			// Guard against division by zero (identical keyframe timestamps)
			if (fTimeDelta <= 0.0f) return xKeys.Get(i).first;
			float t = (fTime - xKeys.Get(i).second) / fTimeDelta;
			return fnInterp(xKeys.Get(i).first, xKeys.Get(i + 1).first, t);
		}
	}
	return xKeys.GetBack().first;
}

Zenith_Maths::Vector3 Flux_RootMotion::SamplePositionDelta(float fTime) const
{
	return SampleRootMotionDeltas(m_xPositionDeltas, fTime, m_bEnabled,
		Zenith_Maths::Vector3(0.0f),
		[](const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float t)
		{ return glm::mix(a, b, t); });
}

Zenith_Maths::Quat Flux_RootMotion::SampleRotationDelta(float fTime) const
{
	return SampleRootMotionDeltas(m_xRotationDeltas, fTime, m_bEnabled,
		Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
		[](const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float t)
		{ return glm::slerp(a, b, t); });
}

void Flux_RootMotion::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_bEnabled;
	Flux_WriteVec3Keys(xStream, m_xPositionDeltas);
	Flux_WriteQuatKeys(xStream, m_xRotationDeltas);
}

void Flux_RootMotion::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_bEnabled;
	Flux_ReadVec3Keys(xStream, m_xPositionDeltas);
	Flux_ReadQuatKeys(xStream, m_xRotationDeltas);
}

//=============================================================================
// Flux_BoneChannel
//=============================================================================
#ifdef ZENITH_TOOLS
Flux_BoneChannel::Flux_BoneChannel(const aiNodeAnim* pxChannel)
{
	m_strBoneName = pxChannel->mNodeName.data;

	// Load position keyframes
	m_xPositions.Reserve(pxChannel->mNumPositionKeys);
	for (uint32_t i = 0; i < pxChannel->mNumPositionKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mPositionKeys[i];
		m_xPositions.EmplaceBack(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}

	// Load rotation keyframes
	m_xRotations.Reserve(pxChannel->mNumRotationKeys);
	for (uint32_t i = 0; i < pxChannel->mNumRotationKeys; ++i)
	{
		const aiQuatKey& xKey = pxChannel->mRotationKeys[i];
		// Assimp uses WXYZ order for quaternions
		m_xRotations.EmplaceBack(
			Zenith_Maths::Quat(xKey.mValue.w, xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}

	// Load scale keyframes
	m_xScales.Reserve(pxChannel->mNumScalingKeys);
	for (uint32_t i = 0; i < pxChannel->mNumScalingKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mScalingKeys[i];
		m_xScales.EmplaceBack(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}
}
#endif // ZENITH_TOOLS

// When fTime is at/after the LAST keyframe the loop finds no segment. It must return
// the LAST keyframe index (size-1) so Sample*() CLAMPS to the last keyframe (its
// p1Index>=size guard returns that keyframe). The old `return 0` returned the FIRST
// segment, so Sample*() computed scaleFactor = fTime/firstSegLen (huge) and
// EXTRAPOLATED the first segment far past it — a wildly wrong pose at the clip end.
// A VAT bake samples its final frame at exactly t=duration (the last keyframe time),
// so that corrupted the last baked frame, making instanced trees lurch for one frame
// at every loop wrap. (Sample*() handle the size 0/1 cases before calling these, so
// size>=2 here and size-1>=1.)
uint32_t Flux_BoneChannel::GetPositionIndex(float fTime) const
{
	for (u_int i = 0; i < m_xPositions.GetSize() - 1; ++i)
	{
		if (fTime < m_xPositions.Get(i + 1).second)
			return i;
	}
	return m_xPositions.GetSize() - 1;
}

uint32_t Flux_BoneChannel::GetRotationIndex(float fTime) const
{
	for (u_int i = 0; i < m_xRotations.GetSize() - 1; ++i)
	{
		if (fTime < m_xRotations.Get(i + 1).second)
			return i;
	}
	return m_xRotations.GetSize() - 1;
}

uint32_t Flux_BoneChannel::GetScaleIndex(float fTime) const
{
	for (u_int i = 0; i < m_xScales.GetSize() - 1; ++i)
	{
		if (fTime < m_xScales.Get(i + 1).second)
			return i;
	}
	return m_xScales.GetSize() - 1;
}

float Flux_BoneChannel::GetScaleFactor(float fLastTime, float fNextTime, float fAnimTime) const
{
	const float fMidWayLength = fAnimTime - fLastTime;
	const float fFramesDiff = fNextTime - fLastTime;
	if (fFramesDiff <= 0.0f)
		return 0.0f;
	return fMidWayLength / fFramesDiff;
}

Zenith_Maths::Vector3 Flux_BoneChannel::SamplePosition(float fTime) const
{
	if (m_xPositions.GetSize() == 0)
		return Zenith_Maths::Vector3(0.0f);

	if (m_xPositions.GetSize() == 1)
		return m_xPositions.Get(0).first;

	uint32_t p0Index = GetPositionIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xPositions.GetSize())
		return m_xPositions.Get(p0Index).first;

	float fScaleFactor = GetScaleFactor(
		m_xPositions.Get(p0Index).second,
		m_xPositions.Get(p1Index).second,
		fTime
	);

	return glm::mix(m_xPositions.Get(p0Index).first, m_xPositions.Get(p1Index).first, fScaleFactor);
}

Zenith_Maths::Quat Flux_BoneChannel::SampleRotation(float fTime) const
{
	if (m_xRotations.GetSize() == 0)
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	if (m_xRotations.GetSize() == 1)
		return glm::normalize(m_xRotations.Get(0).first);

	uint32_t p0Index = GetRotationIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xRotations.GetSize())
		return glm::normalize(m_xRotations.Get(p0Index).first);

	float fScaleFactor = GetScaleFactor(
		m_xRotations.Get(p0Index).second,
		m_xRotations.Get(p1Index).second,
		fTime
	);

	Zenith_Maths::Quat xResult = glm::slerp(
		m_xRotations.Get(p0Index).first,
		m_xRotations.Get(p1Index).first,
		fScaleFactor
	);

	return glm::normalize(xResult);
}

Zenith_Maths::Vector3 Flux_BoneChannel::SampleScale(float fTime) const
{
	if (m_xScales.GetSize() == 0)
		return Zenith_Maths::Vector3(1.0f);

	if (m_xScales.GetSize() == 1)
		return m_xScales.Get(0).first;

	uint32_t p0Index = GetScaleIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xScales.GetSize())
		return m_xScales.Get(p0Index).first;

	float fScaleFactor = GetScaleFactor(
		m_xScales.Get(p0Index).second,
		m_xScales.Get(p1Index).second,
		fTime
	);

	return glm::mix(m_xScales.Get(p0Index).first, m_xScales.Get(p1Index).first, fScaleFactor);
}

Zenith_Maths::Matrix4 Flux_BoneChannel::Sample(float fTime) const
{
	Zenith_Maths::Vector3 xPosition = SamplePosition(fTime);
	Zenith_Maths::Quat xRotation = SampleRotation(fTime);
	Zenith_Maths::Vector3 xScale = SampleScale(fTime);

	Zenith_Maths::Matrix4 xTranslation = glm::translate(glm::mat4(1.0f), xPosition);
	Zenith_Maths::Matrix4 xRotationMat = glm::toMat4(xRotation);
	Zenith_Maths::Matrix4 xScaleMat = glm::scale(glm::mat4(1.0f), xScale);

	return xTranslation * xRotationMat * xScaleMat;
}

void Flux_BoneChannel::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strBoneName;
	Flux_WriteVec3Keys(xStream, m_xPositions);
	Flux_WriteQuatKeys(xStream, m_xRotations);
	Flux_WriteVec3Keys(xStream, m_xScales);
}

void Flux_BoneChannel::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strBoneName;
	Flux_ReadVec3Keys(xStream, m_xPositions);
	Flux_ReadQuatKeys(xStream, m_xRotations);
	Flux_ReadVec3Keys(xStream, m_xScales);
}

void Flux_BoneChannel::AddPositionKeyframe(float fTimeTicks, const Zenith_Maths::Vector3& xPosition)
{
	m_xPositions.EmplaceBack(xPosition, fTimeTicks);
}

void Flux_BoneChannel::AddRotationKeyframe(float fTimeTicks, const Zenith_Maths::Quat& xRotation)
{
	m_xRotations.EmplaceBack(xRotation, fTimeTicks);
}

void Flux_BoneChannel::AddScaleKeyframe(float fTimeTicks, const Zenith_Maths::Vector3& xScale)
{
	m_xScales.EmplaceBack(xScale, fTimeTicks);
}

void Flux_BoneChannel::SortKeyframes()
{
	auto sortByTime = [](const auto& a, const auto& b) { return a.second < b.second; };
	std::sort(m_xPositions.begin(), m_xPositions.end(), sortByTime);
	std::sort(m_xRotations.begin(), m_xRotations.end(), sortByTime);
	std::sort(m_xScales.begin(), m_xScales.end(), sortByTime);
}

//=============================================================================
// Flux_AnimationClip
//=============================================================================
#ifdef ZENITH_TOOLS
void Flux_AnimationClip::LoadFromAssimp(const aiAnimation* pxAnimation, const aiNode*)
{
	// Extract metadata
	m_xMetadata.m_strName = pxAnimation->mName.data;
	m_xMetadata.m_fDuration = static_cast<float>(pxAnimation->mDuration / pxAnimation->mTicksPerSecond);
	m_xMetadata.m_uTicksPerSecond = static_cast<uint32_t>(pxAnimation->mTicksPerSecond);

	// If ticks per second is 0, default to 24
	if (m_xMetadata.m_uTicksPerSecond == 0)
		m_xMetadata.m_uTicksPerSecond = 24;

	// Load bone channels
	m_xBoneChannels.Clear();
	for (uint32_t i = 0; i < pxAnimation->mNumChannels; ++i)
	{
		const aiNodeAnim* pxChannel = pxAnimation->mChannels[i];
		std::string strBoneName = pxChannel->mNodeName.data;
		m_xBoneChannels.Emplace(strBoneName, Flux_BoneChannel(pxChannel));
	}
}
#endif // ZENITH_TOOLS

void Flux_AnimationClip::Export(const std::string& strPath) const
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(strPath.c_str());

	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClip] Exported animation '%s' to: %s", m_xMetadata.m_strName.c_str(), strPath.c_str());
}

const Flux_BoneChannel* Flux_AnimationClip::GetBoneChannel(const std::string& strBoneName) const
{
	return m_xBoneChannels.TryGet(strBoneName);
}

bool Flux_AnimationClip::HasBoneChannel(const std::string& strBoneName) const
{
	return m_xBoneChannels.Contains(strBoneName);
}

void Flux_AnimationClip::AddEvent(const Flux_AnimationEvent& xEvent)
{
	m_xEvents.PushBack(xEvent);
	// Keep events sorted by time
	std::sort(m_xEvents.begin(), m_xEvents.end(),
		[](const Flux_AnimationEvent& a, const Flux_AnimationEvent& b) {
			return a.m_fNormalizedTime < b.m_fNormalizedTime;
		});
}

void Flux_AnimationClip::RemoveEvent(u_int uIndex)
{
	if (uIndex < m_xEvents.GetSize())
		m_xEvents.Remove(uIndex);
}

void Flux_AnimationClip::AddBoneChannel(const std::string& strBoneName, Flux_BoneChannel&& xChannel)
{
	xChannel.SetBoneName(strBoneName);
	m_xBoneChannels.Emplace(strBoneName, std::move(xChannel));
}

void Flux_AnimationClip::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Metadata
	m_xMetadata.WriteToDataStream(xStream);

	// Source path
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strSourcePath);

	// Bone channels
	uint32_t uNumChannels = static_cast<uint32_t>(m_xBoneChannels.GetSize());
	xStream << uNumChannels;
	for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(m_xBoneChannels); !xIt.Done(); xIt.Next())
	{
		xIt.GetValue().WriteToDataStream(xStream);
	}

	// Events
	uint32_t uNumEvents = static_cast<uint32_t>(m_xEvents.GetSize());
	xStream << uNumEvents;
	for (const auto& xEvent : m_xEvents)
	{
		xEvent.WriteToDataStream(xStream);
	}

	// Root motion
	m_xRootMotion.WriteToDataStream(xStream);
}

void Flux_AnimationClip::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Metadata
	m_xMetadata.ReadFromDataStream(xStream);

	// Source path
	xStream >> m_strSourcePath;
	m_strSourcePath = Zenith_AssetRegistry::NormalizeAssetPath(m_strSourcePath);

	// Bone channels
	uint32_t uNumChannels = 0;
	xStream >> uNumChannels;
	m_xBoneChannels.Clear();
	for (uint32_t i = 0; i < uNumChannels; ++i)
	{
		Flux_BoneChannel xChannel;
		xChannel.ReadFromDataStream(xStream);
		m_xBoneChannels.Emplace(xChannel.GetBoneName(), std::move(xChannel));
	}

	// Events
	uint32_t uNumEvents = 0;
	xStream >> uNumEvents;
	m_xEvents.Clear();
	m_xEvents.Reserve(uNumEvents);
	for (u_int i = 0; i < uNumEvents; ++i)
	{
		Flux_AnimationEvent xEvent;
		xEvent.ReadFromDataStream(xStream);
		m_xEvents.PushBack(xEvent);
	}

	// Root motion
	m_xRootMotion.ReadFromDataStream(xStream);
}

//=============================================================================
// Flux_AnimationClipCollection
//=============================================================================
Flux_AnimationClipCollection::~Flux_AnimationClipCollection()
{
	Clear();
}

Flux_AnimationClipCollection::Flux_AnimationClipCollection(Flux_AnimationClipCollection&& xOther) noexcept
	: m_xClipsByName(std::move(xOther.m_xClipsByName))
	, m_xClips(std::move(xOther.m_xClips))
	, m_xBorrowedClips(std::move(xOther.m_xBorrowedClips))
{
	// Clear the moved-from object's containers to prevent double-delete
	xOther.m_xClipsByName.Clear();
	xOther.m_xClips.Clear();
	xOther.m_xBorrowedClips.Clear();
}

Flux_AnimationClipCollection& Flux_AnimationClipCollection::operator=(Flux_AnimationClipCollection&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Delete our existing clips
		Clear();

		// Take ownership of the other's clips
		m_xClipsByName = std::move(xOther.m_xClipsByName);
		m_xClips = std::move(xOther.m_xClips);
		m_xBorrowedClips = std::move(xOther.m_xBorrowedClips);

		// Clear the moved-from object's containers to prevent double-delete
		xOther.m_xClipsByName.Clear();
		xOther.m_xClips.Clear();
		xOther.m_xBorrowedClips.Clear();
	}
	return *this;
}

void Flux_AnimationClipCollection::AddClip(Flux_AnimationClip* pxClip)
{
	if (!pxClip)
		return;

	const std::string& strName = pxClip->GetName();

	// Remove existing clip with same name
	if (HasClip(strName))
		RemoveClip(strName);

	m_xClipsByName[strName] = pxClip;
	m_xClips.PushBack(pxClip);
}

void Flux_AnimationClipCollection::AddClipReference(Flux_AnimationClip* pxClip)
{
	if (!pxClip)
		return;

	const std::string& strName = pxClip->GetName();

	// Remove existing clip with same name
	if (HasClip(strName))
		RemoveClip(strName);

	m_xClipsByName[strName] = pxClip;
	m_xClips.PushBack(pxClip);
	m_xBorrowedClips.Insert(pxClip);  // Mark as borrowed (not owned)
}

void Flux_AnimationClipCollection::RemoveClip(const std::string& strName)
{
	Flux_AnimationClip** ppxClip = m_xClipsByName.TryGet(strName);
	if (ppxClip != nullptr)
	{
		Flux_AnimationClip* pxClip = *ppxClip;

		// Remove from ordered list (order-preserving)
		m_xClips.EraseValue(pxClip);

		// Remove from map
		m_xClipsByName.Remove(strName);

		// Only delete if we own it (not borrowed)
		if (!m_xBorrowedClips.Contains(pxClip))
		{
			delete pxClip;
		}
		else
		{
			m_xBorrowedClips.Remove(pxClip);
		}
	}
}

void Flux_AnimationClipCollection::Clear()
{
	// Only delete clips we own (not borrowed)
	for (Flux_AnimationClip* pxClip : m_xClips)
	{
		if (!m_xBorrowedClips.Contains(pxClip))
		{
			delete pxClip;
		}
	}

	m_xClips.Clear();
	m_xClipsByName.Clear();
	m_xBorrowedClips.Clear();
}

Flux_AnimationClip* Flux_AnimationClipCollection::GetClip(const std::string& strName)
{
	Flux_AnimationClip** ppxClip = m_xClipsByName.TryGet(strName);
	if (ppxClip != nullptr)
		return *ppxClip;
	return nullptr;
}

const Flux_AnimationClip* Flux_AnimationClipCollection::GetClip(const std::string& strName) const
{
	Flux_AnimationClip* const* ppxClip = m_xClipsByName.TryGet(strName);
	if (ppxClip != nullptr)
		return *ppxClip;
	return nullptr;
}

bool Flux_AnimationClipCollection::HasClip(const std::string& strName) const
{
	return m_xClipsByName.Contains(strName);
}

#ifdef ZENITH_TOOLS
void Flux_AnimationClipCollection::LoadFromFile(const std::string& strPath)
{
	Assimp::Importer xImporter;
	const aiScene* pxScene = xImporter.ReadFile(strPath,
		aiProcess_Triangulate |
		aiProcess_LimitBoneWeights |
		aiProcess_ValidateDataStructure
	);

	if (!pxScene || !pxScene->mRootNode)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClipCollection] Failed to load file: %s", strPath.c_str());
		return;
	}

	// Load all animations from the file
	for (uint32_t i = 0; i < pxScene->mNumAnimations; ++i)
	{
		Flux_AnimationClip* pxClip = new Flux_AnimationClip();
		pxClip->LoadFromAssimp(pxScene->mAnimations[i], pxScene->mRootNode);
		pxClip->SetSourcePath(strPath);

		// If clip name is empty, generate one
		if (pxClip->GetName().empty())
		{
			pxClip->SetName("Animation_" + std::to_string(i));
		}

		AddClip(pxClip);
	}

	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClipCollection] Loaded %u animations from: %s",
		pxScene->mNumAnimations, strPath.c_str());
}
#endif // ZENITH_TOOLS

void Flux_AnimationClipCollection::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumClips = static_cast<uint32_t>(m_xClips.GetSize());
	xStream << uNumClips;

	for (const Flux_AnimationClip* pxClip : m_xClips)
	{
		pxClip->WriteToDataStream(xStream);
	}
}

void Flux_AnimationClipCollection::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Clear();

	uint32_t uNumClips = 0;
	xStream >> uNumClips;

	for (uint32_t i = 0; i < uNumClips; ++i)
	{
		Flux_AnimationClip* pxClip = new Flux_AnimationClip();
		pxClip->ReadFromDataStream(xStream);
		AddClip(pxClip);
	}
}

#include "Flux/MeshAnimation/Flux_AnimationClip.Tests.inl"
