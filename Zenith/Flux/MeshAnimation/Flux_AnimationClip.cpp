#include "Zenith.h"
#include "Flux_AnimationClip.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

#ifdef ZENITH_TOOLS
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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
// Reserved per-key tangent block (D17). Same count+loop shape as the keyframe
// helpers above: uint32 count, then per entry the in-tangent xyz then the
// out-tangent xyz. A rotation channel's entries are ANGULAR velocities
// (axis * rad/s), which is why one Vector3 pair serves all three channel types.
//=============================================================================
void Flux_WriteKeyTangents(Zenith_DataStream& xStream, const Zenith_Vector<Flux_KeyTangents>& xTangents)
{
	xStream << static_cast<uint32_t>(xTangents.GetSize());
	for (const Flux_KeyTangents& xTangent : xTangents)
	{
		xStream << xTangent.m_xInTangent.x;
		xStream << xTangent.m_xInTangent.y;
		xStream << xTangent.m_xInTangent.z;
		xStream << xTangent.m_xOutTangent.x;
		xStream << xTangent.m_xOutTangent.y;
		xStream << xTangent.m_xOutTangent.z;
	}
}

void Flux_ReadKeyTangents(Zenith_DataStream& xStream, Zenith_Vector<Flux_KeyTangents>& xTangents)
{
	uint32_t uCount = 0;
	xStream >> uCount;
	xTangents.Clear();
	xTangents.Reserve(uCount);
	for (u_int i = 0; i < uCount; ++i)
	{
		Flux_KeyTangents xTangent;
		xStream >> xTangent.m_xInTangent.x;
		xStream >> xTangent.m_xInTangent.y;
		xStream >> xTangent.m_xInTangent.z;
		xStream >> xTangent.m_xOutTangent.x;
		xStream >> xTangent.m_xOutTangent.y;
		xStream >> xTangent.m_xOutTangent.z;
		xTangents.PushBack(xTangent);
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

	xStream << m_uAuthoredFrameRate;
	// Normalized on the way out, exactly like Flux_AnimationClip::m_strSourcePath, so
	// an absolute authoring-machine path never reaches the file.
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strSkeletonPath);
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strPreviewModelPath);
	xStream << m_bGenerated;
}

void Flux_AnimationClipMetadata::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strName;
	xStream >> m_fDuration;
	xStream >> m_uTicksPerSecond;
	xStream >> m_bLooping;
	xStream >> m_fBlendInTime;
	xStream >> m_fBlendOutTime;

	xStream >> m_uAuthoredFrameRate;
	xStream >> m_strSkeletonPath;
	m_strSkeletonPath = Zenith_AssetRegistry::NormalizeAssetPath(m_strSkeletonPath);
	xStream >> m_strPreviewModelPath;
	m_strPreviewModelPath = Zenith_AssetRegistry::NormalizeAssetPath(m_strPreviewModelPath);
	xStream >> m_bGenerated;
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
Flux_BoneChannel::Flux_BoneChannel(const aiNodeAnim* pxChannel, double dSourceTicksPerSecond)
{
	m_strBoneName = pxChannel->mNodeName.data;

	// ★ THE IMPORT IS WHERE TICKS BECOME SECONDS (D3). aiVectorKey::mTime is a tick
	// count on the source file's own grid; the channel stores seconds. Dividing here
	// — once, at the only place a tick ever enters the engine — is what lets every
	// generator, sampler and test downstream hold one unit.
	//
	// A zero or negative rate would silently produce infinities, so it is refused and
	// treated as 1 (key times pass through unscaled) rather than guessed at. The
	// caller has already applied the "the file said 0, use 24" default; a rate that is
	// still bad here means the aiAnimation itself is malformed.
	Zenith_Assert(dSourceTicksPerSecond > 0.0,
		"Flux_BoneChannel('%s'): source ticks-per-second is %f — key times cannot be converted to seconds",
		m_strBoneName.c_str(), dSourceTicksPerSecond);
	const double dToSeconds = (dSourceTicksPerSecond > 0.0) ? (1.0 / dSourceTicksPerSecond) : 1.0;

	// Load position keyframes
	m_xPositions.Reserve(pxChannel->mNumPositionKeys);
	for (uint32_t i = 0; i < pxChannel->mNumPositionKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mPositionKeys[i];
		m_xPositions.EmplaceBack(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime * dToSeconds)
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
			static_cast<float>(xKey.mTime * dToSeconds)
		);
	}

	// Load scale keyframes
	m_xScales.Reserve(pxChannel->mNumScalingKeys);
	for (uint32_t i = 0; i < pxChannel->mNumScalingKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mScalingKeys[i];
		m_xScales.EmplaceBack(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime * dToSeconds)
		);
	}

	// Assimp carries no tangents, so the reserved block comes in at its zero default
	// — but it must still be the same length as the keys it parallels.
	m_xPositionTangents.Resize(m_xPositions.GetSize(), Flux_KeyTangents());
	m_xRotationTangents.Resize(m_xRotations.GetSize(), Flux_KeyTangents());
	m_xScaleTangents.Resize(m_xScales.GetSize(), Flux_KeyTangents());
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
uint32_t Flux_BoneChannel::GetPositionIndex(float fTimeSeconds) const
{
	for (u_int i = 0; i < m_xPositions.GetSize() - 1; ++i)
	{
		if (fTimeSeconds < m_xPositions.Get(i + 1).second)
			return i;
	}
	return m_xPositions.GetSize() - 1;
}

uint32_t Flux_BoneChannel::GetRotationIndex(float fTimeSeconds) const
{
	for (u_int i = 0; i < m_xRotations.GetSize() - 1; ++i)
	{
		if (fTimeSeconds < m_xRotations.Get(i + 1).second)
			return i;
	}
	return m_xRotations.GetSize() - 1;
}

uint32_t Flux_BoneChannel::GetScaleIndex(float fTimeSeconds) const
{
	for (u_int i = 0; i < m_xScales.GetSize() - 1; ++i)
	{
		if (fTimeSeconds < m_xScales.Get(i + 1).second)
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

Zenith_Maths::Vector3 Flux_BoneChannel::SamplePosition(float fTimeSeconds) const
{
	if (m_xPositions.GetSize() == 0)
		return Zenith_Maths::Vector3(0.0f);

	if (m_xPositions.GetSize() == 1)
		return m_xPositions.Get(0).first;

	uint32_t p0Index = GetPositionIndex(fTimeSeconds);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xPositions.GetSize())
		return m_xPositions.Get(p0Index).first;

	float fScaleFactor = GetScaleFactor(
		m_xPositions.Get(p0Index).second,
		m_xPositions.Get(p1Index).second,
		fTimeSeconds
	);

	return glm::mix(m_xPositions.Get(p0Index).first, m_xPositions.Get(p1Index).first, fScaleFactor);
}

Zenith_Maths::Quat Flux_BoneChannel::SampleRotation(float fTimeSeconds) const
{
	if (m_xRotations.GetSize() == 0)
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	if (m_xRotations.GetSize() == 1)
		return glm::normalize(m_xRotations.Get(0).first);

	uint32_t p0Index = GetRotationIndex(fTimeSeconds);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xRotations.GetSize())
		return glm::normalize(m_xRotations.Get(p0Index).first);

	float fScaleFactor = GetScaleFactor(
		m_xRotations.Get(p0Index).second,
		m_xRotations.Get(p1Index).second,
		fTimeSeconds
	);

	Zenith_Maths::Quat xResult = glm::slerp(
		m_xRotations.Get(p0Index).first,
		m_xRotations.Get(p1Index).first,
		fScaleFactor
	);

	return glm::normalize(xResult);
}

Zenith_Maths::Vector3 Flux_BoneChannel::SampleScale(float fTimeSeconds) const
{
	if (m_xScales.GetSize() == 0)
		return Zenith_Maths::Vector3(1.0f);

	if (m_xScales.GetSize() == 1)
		return m_xScales.Get(0).first;

	uint32_t p0Index = GetScaleIndex(fTimeSeconds);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xScales.GetSize())
		return m_xScales.Get(p0Index).first;

	float fScaleFactor = GetScaleFactor(
		m_xScales.Get(p0Index).second,
		m_xScales.Get(p1Index).second,
		fTimeSeconds
	);

	return glm::mix(m_xScales.Get(p0Index).first, m_xScales.Get(p1Index).first, fScaleFactor);
}

Zenith_Maths::Matrix4 Flux_BoneChannel::Sample(float fTimeSeconds) const
{
	Zenith_Maths::Vector3 xPosition = SamplePosition(fTimeSeconds);
	Zenith_Maths::Quat xRotation = SampleRotation(fTimeSeconds);
	Zenith_Maths::Vector3 xScale = SampleScale(fTimeSeconds);

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
	// Reserved tangent block (D17) — trails the keys so a reader that already knows
	// the key counts can check the parallel arrays against them.
	Flux_WriteKeyTangents(xStream, m_xPositionTangents);
	Flux_WriteKeyTangents(xStream, m_xRotationTangents);
	Flux_WriteKeyTangents(xStream, m_xScaleTangents);
}

void Flux_BoneChannel::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strBoneName;
	Flux_ReadVec3Keys(xStream, m_xPositions);
	Flux_ReadQuatKeys(xStream, m_xRotations);
	Flux_ReadVec3Keys(xStream, m_xScales);
	Flux_ReadKeyTangents(xStream, m_xPositionTangents);
	Flux_ReadKeyTangents(xStream, m_xRotationTangents);
	Flux_ReadKeyTangents(xStream, m_xScaleTangents);

	// The writer can only ever emit matched lengths, so a mismatch here is a corrupt
	// or mis-cut stream. Say so, then restore the invariant rather than leaving the
	// channel with arrays that index differently.
	Zenith_Assert(m_xPositionTangents.GetSize() == m_xPositions.GetSize()
		&& m_xRotationTangents.GetSize() == m_xRotations.GetSize()
		&& m_xScaleTangents.GetSize() == m_xScales.GetSize(),
		"Flux_BoneChannel '%s': reserved tangent block does not parallel the keyframes", m_strBoneName.c_str());
	m_xPositionTangents.Resize(m_xPositions.GetSize(), Flux_KeyTangents());
	m_xRotationTangents.Resize(m_xRotations.GetSize(), Flux_KeyTangents());
	m_xScaleTangents.Resize(m_xScales.GetSize(), Flux_KeyTangents());
}

void Flux_BoneChannel::AddPositionKeyframe(float fTimeSeconds, const Zenith_Maths::Vector3& xPosition)
{
	m_xPositions.EmplaceBack(xPosition, fTimeSeconds);
	m_xPositionTangents.PushBack(Flux_KeyTangents());
}

void Flux_BoneChannel::AddRotationKeyframe(float fTimeSeconds, const Zenith_Maths::Quat& xRotation)
{
	m_xRotations.EmplaceBack(xRotation, fTimeSeconds);
	m_xRotationTangents.PushBack(Flux_KeyTangents());
}

void Flux_BoneChannel::AddScaleKeyframe(float fTimeSeconds, const Zenith_Maths::Vector3& xScale)
{
	m_xScales.EmplaceBack(xScale, fTimeSeconds);
	m_xScaleTangents.PushBack(Flux_KeyTangents());
}

float Flux_BoneChannel::GetLastKeyTimeSeconds() const
{
	// MAX, not "the back of each array": a channel is legitimately inspected before
	// SortKeyframes has run (that is precisely when a generator wants to check its
	// own work), and a back-of-array read would then report an interior key.
	float fLast = 0.0f;
	for (const auto& xKey : m_xPositions) { if (xKey.second > fLast) { fLast = xKey.second; } }
	for (const auto& xKey : m_xRotations) { if (xKey.second > fLast) { fLast = xKey.second; } }
	for (const auto& xKey : m_xScales)    { if (xKey.second > fLast) { fLast = xKey.second; } }
	return fLast;
}

void Flux_BoneChannel::SetPositionTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents)
{
	Zenith_Assert(uKeyIndex < m_xPositionTangents.GetSize(), "SetPositionTangent: key index %u out of range (%u keys)", uKeyIndex, m_xPositionTangents.GetSize());
	if (uKeyIndex < m_xPositionTangents.GetSize())
		m_xPositionTangents.Get(uKeyIndex) = xTangents;
}

void Flux_BoneChannel::SetRotationTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents)
{
	Zenith_Assert(uKeyIndex < m_xRotationTangents.GetSize(), "SetRotationTangent: key index %u out of range (%u keys)", uKeyIndex, m_xRotationTangents.GetSize());
	if (uKeyIndex < m_xRotationTangents.GetSize())
		m_xRotationTangents.Get(uKeyIndex) = xTangents;
}

void Flux_BoneChannel::SetScaleTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents)
{
	Zenith_Assert(uKeyIndex < m_xScaleTangents.GetSize(), "SetScaleTangent: key index %u out of range (%u keys)", uKeyIndex, m_xScaleTangents.GetSize());
	if (uKeyIndex < m_xScaleTangents.GetSize())
		m_xScaleTangents.Get(uKeyIndex) = xTangents;
}

// Sorting a keyframe array on its own would silently un-pair it from the reserved
// tangent array that parallels it, so the two are permuted together. The sort is
// STABLE: two keys sharing a timestamp keep their authored order, which is what
// keeps a re-serialized clip byte-identical (D5) rather than dependent on
// std::sort's introsort pivot choices.
template<typename V>
static void Flux_SortKeysWithTangents(Zenith_Vector<std::pair<V, float>>& xKeys, Zenith_Vector<Flux_KeyTangents>& xTangents)
{
	const u_int uCount = xKeys.GetSize();
	if (xTangents.GetSize() != uCount)
	{
		xTangents.Resize(uCount, Flux_KeyTangents());
	}
	if (uCount < 2)
	{
		return;
	}

	Zenith_Vector<u_int> auOrder;
	auOrder.Reserve(uCount);
	for (u_int u = 0; u < uCount; ++u)
	{
		auOrder.PushBack(u);
	}
	std::stable_sort(auOrder.begin(), auOrder.end(),
		[&xKeys](u_int uA, u_int uB) { return xKeys.Get(uA).second < xKeys.Get(uB).second; });

	Zenith_Vector<std::pair<V, float>> xSortedKeys;
	Zenith_Vector<Flux_KeyTangents> xSortedTangents;
	xSortedKeys.Reserve(uCount);
	xSortedTangents.Reserve(uCount);
	for (u_int u = 0; u < uCount; ++u)
	{
		xSortedKeys.PushBack(xKeys.Get(auOrder.Get(u)));
		xSortedTangents.PushBack(xTangents.Get(auOrder.Get(u)));
	}
	for (u_int u = 0; u < uCount; ++u)
	{
		xKeys.Get(u) = xSortedKeys.Get(u);
		xTangents.Get(u) = xSortedTangents.Get(u);
	}
}

void Flux_BoneChannel::SortKeyframes()
{
	Flux_SortKeysWithTangents(m_xPositions, m_xPositionTangents);
	Flux_SortKeysWithTangents(m_xRotations, m_xRotationTangents);
	Flux_SortKeysWithTangents(m_xScales,    m_xScaleTangents);
}

//=============================================================================
// Flux_AnimationClip
//=============================================================================
#ifdef ZENITH_TOOLS
void Flux_AnimationClip::LoadFromAssimp(const aiAnimation* pxAnimation, const aiNode*)
{
	// Extract metadata
	m_xMetadata.m_strName = pxAnimation->mName.data;
	m_xMetadata.m_uTicksPerSecond = static_cast<uint32_t>(pxAnimation->mTicksPerSecond);

	// If ticks per second is 0, default to 24
	if (m_xMetadata.m_uTicksPerSecond == 0)
		m_xMetadata.m_uTicksPerSecond = 24;

	// ★ ONE divisor, resolved BEFORE it is used, and used for BOTH the duration and
	// every key time — otherwise the clip's length and its keys land on different
	// clocks, which is the defect D3 removes. It used to divide the duration by the
	// RAW mTicksPerSecond on the line above the zero-default, so a file declaring 0
	// produced an infinite duration and the default never reached it.
	const double dSourceTicksPerSecond = static_cast<double>(m_xMetadata.m_uTicksPerSecond);
	m_xMetadata.m_fDuration = static_cast<float>(pxAnimation->mDuration / dSourceTicksPerSecond);

	// Load bone channels
	m_xBoneChannels.Clear();
	for (uint32_t i = 0; i < pxAnimation->mNumChannels; ++i)
	{
		const aiNodeAnim* pxChannel = pxAnimation->mChannels[i];
		std::string strBoneName = pxChannel->mNodeName.data;
		m_xBoneChannels.Emplace(strBoneName, Flux_BoneChannel(pxChannel, dSourceTicksPerSecond));
	}
}
#endif // ZENITH_TOOLS

// D19: Export is PUBLIC and NOT tools-gated — a procedural generator running in a
// runtime build writes .zanim through it. It is WriteToDataStream + WriteToFile and
// nothing else, so it inherits the stream envelope from WriteToDataStream; there is
// no second write path that could emit a headerless file.
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
	// D1: the shared stream envelope leads every typed asset payload. Flux_AnimationClip
	// is the write half of BOTH .zanim paths — Export() is WriteToDataStream +
	// WriteToFile — so putting it here is what gives Export the envelope.
	Zenith_WriteStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID, uZENITH_ANIMATION_SCHEMA_CURRENT);

	// Metadata
	m_xMetadata.WriteToDataStream(xStream);

	// Source path
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strSourcePath);

	// ★ D5: CHANNELS GO OUT IN BONE-NAME ORDER, NOT HASH ORDER.
	// Walking m_xBoneChannels directly put the channels on disk in Zenith_HashMap
	// bucket order, so the same clip built by two different insertion sequences — or
	// by a build whose hashing or capacity growth differs — serialized to different
	// bytes for identical animation data. Every .zanim is gitignored bake output
	// today, which is the only reason that has cost nothing so far; the day one is
	// committed it becomes churn on every re-bake. This is the same defect .zscen had
	// with process-global slot indices, and it gets the same fix: impose a total order
	// the data itself defines.
	Zenith_Vector<const Flux_BoneChannel*> apxOrderedChannels;
	apxOrderedChannels.Reserve(m_xBoneChannels.GetSize());
	for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(m_xBoneChannels); !xIt.Done(); xIt.Next())
	{
		apxOrderedChannels.PushBack(&xIt.GetValue());
	}
	std::sort(apxOrderedChannels.begin(), apxOrderedChannels.end(),
		[](const Flux_BoneChannel* pxA, const Flux_BoneChannel* pxB)
		{ return pxA->GetBoneName() < pxB->GetBoneName(); });

	// Bone channels
	uint32_t uNumChannels = static_cast<uint32_t>(apxOrderedChannels.GetSize());
	xStream << uNumChannels;
	for (const Flux_BoneChannel* pxChannel : apxOrderedChannels)
	{
		pxChannel->WriteToDataStream(xStream);
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

void Flux_AnimationClip::ResetToEmpty()
{
	m_xMetadata = Flux_AnimationClipMetadata();
	m_xBoneChannels.Clear();
	m_xEvents.Clear();
	m_xRootMotion.m_bEnabled = false;
	m_xRootMotion.m_xPositionDeltas.Clear();
	m_xRootMotion.m_xRotationDeltas.Clear();
	m_strSourcePath.clear();
}

void Flux_AnimationClip::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// D1/D2: the envelope is MANDATORY and the schema must be EXACTLY current. There
	// is no headerless branch and no "read it as current anyway" branch — a .zanim is
	// bake output, so an older or unrecognised layout is a stale bake to be deleted
	// and rewritten, not a file to guess at. Zenith_ReadStreamHeader restores the
	// cursor on every failure path, so a refused stream is handed back untouched.
	Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID);
	if (!xHeader.IsOk())
	{
		Zenith_Assert(false, "Flux_AnimationClip::ReadFromDataStream: stream carries no valid .zanim envelope");
		ResetToEmpty();
		return;
	}
	if (xHeader.Value().m_uSchemaVersion != uZENITH_ANIMATION_SCHEMA_CURRENT)
	{
		Zenith_Assert(false, "Flux_AnimationClip::ReadFromDataStream: .zanim schema %u is not the current %u — stale bake, delete it and re-run the tools boot",
			xHeader.Value().m_uSchemaVersion, uZENITH_ANIMATION_SCHEMA_CURRENT);
		ResetToEmpty();
		return;
	}

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
// Key-time / duration agreement (D3). See the header for why this is only a
// checkable property now that both sides are seconds.
//=============================================================================
float Flux_ClipLastKeyTimeSeconds(const Flux_AnimationClip& xClip)
{
	float fLast = 0.0f;
	for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xClip.GetBoneChannels()); !xIt.Done(); xIt.Next())
	{
		const float fChannelLast = xIt.GetValue().GetLastKeyTimeSeconds();
		if (fChannelLast > fLast)
		{
			fLast = fChannelLast;
		}
	}
	return fLast;
}

bool Flux_ClipKeyTimesFitDuration(const Flux_AnimationClip& xClip, float fEpsilonSeconds)
{
	for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xClip.GetBoneChannels()); !xIt.Done(); xIt.Next())
	{
		const Flux_BoneChannel& xChannel = xIt.GetValue();

		// A NEGATIVE key time is checked too. It cannot arise from an honest authoring
		// pass, but it is exactly what a "divide the tick literal" conversion produces
		// from a sign slip, and it would otherwise sail past a last-key-only check.
		auto CheckKeys = [fEpsilonSeconds, &xClip](const auto& xKeys) -> bool
		{
			for (const auto& xKey : xKeys)
			{
				if (xKey.second < -fEpsilonSeconds) { return false; }
				if (xKey.second > xClip.GetDuration() + fEpsilonSeconds) { return false; }
			}
			return true;
		};

		if (!CheckKeys(xChannel.GetPositionKeyframes())) { return false; }
		if (!CheckKeys(xChannel.GetRotationKeyframes())) { return false; }
		if (!CheckKeys(xChannel.GetScaleKeyframes()))    { return false; }
	}
	return true;
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
