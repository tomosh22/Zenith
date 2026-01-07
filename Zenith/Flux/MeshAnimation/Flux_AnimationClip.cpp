#include "Zenith.h"
#include "Flux_AnimationClip.h"
#include "Core/Zenith_Core.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
Zenith_Maths::Vector3 Flux_RootMotion::SamplePositionDelta(float fTime) const
{
	if (!m_bEnabled || m_xPositionDeltas.empty())
		return Zenith_Maths::Vector3(0.0f);

	if (m_xPositionDeltas.size() == 1)
		return m_xPositionDeltas[0].first;

	// Find keyframes to interpolate between
	for (size_t i = 0; i < m_xPositionDeltas.size() - 1; ++i)
	{
		if (fTime < m_xPositionDeltas[i + 1].second)
		{
			float fTimeDelta = m_xPositionDeltas[i + 1].second - m_xPositionDeltas[i].second;
			// Guard against division by zero (identical keyframe timestamps)
			if (fTimeDelta <= 0.0f)
				return m_xPositionDeltas[i].first;
			float t = (fTime - m_xPositionDeltas[i].second) / fTimeDelta;
			return glm::mix(m_xPositionDeltas[i].first, m_xPositionDeltas[i + 1].first, t);
		}
	}

	return m_xPositionDeltas.back().first;
}

Zenith_Maths::Quat Flux_RootMotion::SampleRotationDelta(float fTime) const
{
	if (!m_bEnabled || m_xRotationDeltas.empty())
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	if (m_xRotationDeltas.size() == 1)
		return m_xRotationDeltas[0].first;

	// Find keyframes to interpolate between
	for (size_t i = 0; i < m_xRotationDeltas.size() - 1; ++i)
	{
		if (fTime < m_xRotationDeltas[i + 1].second)
		{
			float fTimeDelta = m_xRotationDeltas[i + 1].second - m_xRotationDeltas[i].second;
			// Guard against division by zero (identical keyframe timestamps)
			if (fTimeDelta <= 0.0f)
				return m_xRotationDeltas[i].first;
			float t = (fTime - m_xRotationDeltas[i].second) / fTimeDelta;
			return glm::slerp(m_xRotationDeltas[i].first, m_xRotationDeltas[i + 1].first, t);
		}
	}

	return m_xRotationDeltas.back().first;
}

void Flux_RootMotion::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_bEnabled;

	uint32_t uNumPosDelta = static_cast<uint32_t>(m_xPositionDeltas.size());
	xStream << uNumPosDelta;
	for (const auto& xDelta : m_xPositionDeltas)
	{
		xStream << xDelta.first.x;
		xStream << xDelta.first.y;
		xStream << xDelta.first.z;
		xStream << xDelta.second;
	}

	uint32_t uNumRotDelta = static_cast<uint32_t>(m_xRotationDeltas.size());
	xStream << uNumRotDelta;
	for (const auto& xDelta : m_xRotationDeltas)
	{
		xStream << xDelta.first.w;
		xStream << xDelta.first.x;
		xStream << xDelta.first.y;
		xStream << xDelta.first.z;
		xStream << xDelta.second;
	}
}

void Flux_RootMotion::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_bEnabled;

	uint32_t uNumPosDelta = 0;
	xStream >> uNumPosDelta;
	m_xPositionDeltas.resize(uNumPosDelta);
	for (uint32_t i = 0; i < uNumPosDelta; ++i)
	{
		xStream >> m_xPositionDeltas[i].first.x;
		xStream >> m_xPositionDeltas[i].first.y;
		xStream >> m_xPositionDeltas[i].first.z;
		xStream >> m_xPositionDeltas[i].second;
	}

	uint32_t uNumRotDelta = 0;
	xStream >> uNumRotDelta;
	m_xRotationDeltas.resize(uNumRotDelta);
	for (uint32_t i = 0; i < uNumRotDelta; ++i)
	{
		xStream >> m_xRotationDeltas[i].first.w;
		xStream >> m_xRotationDeltas[i].first.x;
		xStream >> m_xRotationDeltas[i].first.y;
		xStream >> m_xRotationDeltas[i].first.z;
		xStream >> m_xRotationDeltas[i].second;
	}
}

//=============================================================================
// Flux_BoneChannel
//=============================================================================
Flux_BoneChannel::Flux_BoneChannel(const aiNodeAnim* pxChannel)
{
	m_strBoneName = pxChannel->mNodeName.data;

	// Load position keyframes
	m_xPositions.reserve(pxChannel->mNumPositionKeys);
	for (uint32_t i = 0; i < pxChannel->mNumPositionKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mPositionKeys[i];
		m_xPositions.emplace_back(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}

	// Load rotation keyframes
	m_xRotations.reserve(pxChannel->mNumRotationKeys);
	for (uint32_t i = 0; i < pxChannel->mNumRotationKeys; ++i)
	{
		const aiQuatKey& xKey = pxChannel->mRotationKeys[i];
		// Assimp uses WXYZ order for quaternions
		m_xRotations.emplace_back(
			Zenith_Maths::Quat(xKey.mValue.w, xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}

	// Load scale keyframes
	m_xScales.reserve(pxChannel->mNumScalingKeys);
	for (uint32_t i = 0; i < pxChannel->mNumScalingKeys; ++i)
	{
		const aiVectorKey& xKey = pxChannel->mScalingKeys[i];
		m_xScales.emplace_back(
			Zenith_Maths::Vector3(xKey.mValue.x, xKey.mValue.y, xKey.mValue.z),
			static_cast<float>(xKey.mTime)
		);
	}
}

uint32_t Flux_BoneChannel::GetPositionIndex(float fTime) const
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_xPositions.size()) - 1; ++i)
	{
		if (fTime < m_xPositions[i + 1].second)
			return i;
	}
	return 0;
}

uint32_t Flux_BoneChannel::GetRotationIndex(float fTime) const
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_xRotations.size()) - 1; ++i)
	{
		if (fTime < m_xRotations[i + 1].second)
			return i;
	}
	return 0;
}

uint32_t Flux_BoneChannel::GetScaleIndex(float fTime) const
{
	for (uint32_t i = 0; i < static_cast<uint32_t>(m_xScales.size()) - 1; ++i)
	{
		if (fTime < m_xScales[i + 1].second)
			return i;
	}
	return 0;
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
	if (m_xPositions.empty())
		return Zenith_Maths::Vector3(0.0f);

	if (m_xPositions.size() == 1)
		return m_xPositions[0].first;

	uint32_t p0Index = GetPositionIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xPositions.size())
		return m_xPositions[p0Index].first;

	float fScaleFactor = GetScaleFactor(
		m_xPositions[p0Index].second,
		m_xPositions[p1Index].second,
		fTime
	);

	return glm::mix(m_xPositions[p0Index].first, m_xPositions[p1Index].first, fScaleFactor);
}

Zenith_Maths::Quat Flux_BoneChannel::SampleRotation(float fTime) const
{
	if (m_xRotations.empty())
		return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	if (m_xRotations.size() == 1)
		return glm::normalize(m_xRotations[0].first);

	uint32_t p0Index = GetRotationIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xRotations.size())
		return glm::normalize(m_xRotations[p0Index].first);

	float fScaleFactor = GetScaleFactor(
		m_xRotations[p0Index].second,
		m_xRotations[p1Index].second,
		fTime
	);

	Zenith_Maths::Quat xResult = glm::slerp(
		m_xRotations[p0Index].first,
		m_xRotations[p1Index].first,
		fScaleFactor
	);

	return glm::normalize(xResult);
}

Zenith_Maths::Vector3 Flux_BoneChannel::SampleScale(float fTime) const
{
	if (m_xScales.empty())
		return Zenith_Maths::Vector3(1.0f);

	if (m_xScales.size() == 1)
		return m_xScales[0].first;

	uint32_t p0Index = GetScaleIndex(fTime);
	uint32_t p1Index = p0Index + 1;

	if (p1Index >= m_xScales.size())
		return m_xScales[p0Index].first;

	float fScaleFactor = GetScaleFactor(
		m_xScales[p0Index].second,
		m_xScales[p1Index].second,
		fTime
	);

	return glm::mix(m_xScales[p0Index].first, m_xScales[p1Index].first, fScaleFactor);
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

	// Positions
	uint32_t uNumPositions = static_cast<uint32_t>(m_xPositions.size());
	xStream << uNumPositions;
	for (const auto& xKey : m_xPositions)
	{
		xStream << xKey.first.x;
		xStream << xKey.first.y;
		xStream << xKey.first.z;
		xStream << xKey.second;
	}

	// Rotations
	uint32_t uNumRotations = static_cast<uint32_t>(m_xRotations.size());
	xStream << uNumRotations;
	for (const auto& xKey : m_xRotations)
	{
		xStream << xKey.first.w;
		xStream << xKey.first.x;
		xStream << xKey.first.y;
		xStream << xKey.first.z;
		xStream << xKey.second;
	}

	// Scales
	uint32_t uNumScales = static_cast<uint32_t>(m_xScales.size());
	xStream << uNumScales;
	for (const auto& xKey : m_xScales)
	{
		xStream << xKey.first.x;
		xStream << xKey.first.y;
		xStream << xKey.first.z;
		xStream << xKey.second;
	}
}

void Flux_BoneChannel::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strBoneName;

	// Positions
	uint32_t uNumPositions = 0;
	xStream >> uNumPositions;
	m_xPositions.resize(uNumPositions);
	for (uint32_t i = 0; i < uNumPositions; ++i)
	{
		xStream >> m_xPositions[i].first.x;
		xStream >> m_xPositions[i].first.y;
		xStream >> m_xPositions[i].first.z;
		xStream >> m_xPositions[i].second;
	}

	// Rotations
	uint32_t uNumRotations = 0;
	xStream >> uNumRotations;
	m_xRotations.resize(uNumRotations);
	for (uint32_t i = 0; i < uNumRotations; ++i)
	{
		xStream >> m_xRotations[i].first.w;
		xStream >> m_xRotations[i].first.x;
		xStream >> m_xRotations[i].first.y;
		xStream >> m_xRotations[i].first.z;
		xStream >> m_xRotations[i].second;
	}

	// Scales
	uint32_t uNumScales = 0;
	xStream >> uNumScales;
	m_xScales.resize(uNumScales);
	for (uint32_t i = 0; i < uNumScales; ++i)
	{
		xStream >> m_xScales[i].first.x;
		xStream >> m_xScales[i].first.y;
		xStream >> m_xScales[i].first.z;
		xStream >> m_xScales[i].second;
	}
}

//=============================================================================
// Flux_AnimationClip
//=============================================================================
void Flux_AnimationClip::LoadFromAssimp(const aiAnimation* pxAnimation, const aiNode* pxRootNode)
{
	// Extract metadata
	m_xMetadata.m_strName = pxAnimation->mName.data;
	m_xMetadata.m_fDuration = static_cast<float>(pxAnimation->mDuration / pxAnimation->mTicksPerSecond);
	m_xMetadata.m_uTicksPerSecond = static_cast<uint32_t>(pxAnimation->mTicksPerSecond);

	// If ticks per second is 0, default to 24
	if (m_xMetadata.m_uTicksPerSecond == 0)
		m_xMetadata.m_uTicksPerSecond = 24;

	// Load bone channels
	m_xBoneChannels.clear();
	for (uint32_t i = 0; i < pxAnimation->mNumChannels; ++i)
	{
		const aiNodeAnim* pxChannel = pxAnimation->mChannels[i];
		std::string strBoneName = pxChannel->mNodeName.data;
		m_xBoneChannels.emplace(strBoneName, Flux_BoneChannel(pxChannel));
	}
}

Flux_AnimationClip* Flux_AnimationClip::LoadFromFile(const std::string& strPath)
{
	Assimp::Importer xImporter;
	const aiScene* pxScene = xImporter.ReadFile(strPath,
		aiProcess_Triangulate |
		aiProcess_LimitBoneWeights |
		aiProcess_ValidateDataStructure
	);

	if (!pxScene || !pxScene->mRootNode)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClip] Failed to load animation from: %s", strPath.c_str());
		return nullptr;
	}

	if (pxScene->mNumAnimations == 0)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClip] No animations found in: %s", strPath.c_str());
		return nullptr;
	}

	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->LoadFromAssimp(pxScene->mAnimations[0], pxScene->mRootNode);
	pxClip->m_strSourcePath = strPath;

	return pxClip;
}

Flux_AnimationClip* Flux_AnimationClip::LoadFromZanimFile(const std::string& strPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());

	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->ReadFromDataStream(xStream);

	return pxClip;
}

void Flux_AnimationClip::Export(const std::string& strPath) const
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(strPath.c_str());

	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimationClip] Exported animation '%s' to: %s", m_xMetadata.m_strName.c_str(), strPath.c_str());
}

const Flux_BoneChannel* Flux_AnimationClip::GetBoneChannel(const std::string& strBoneName) const
{
	auto it = m_xBoneChannels.find(strBoneName);
	if (it != m_xBoneChannels.end())
		return &it->second;
	return nullptr;
}

bool Flux_AnimationClip::HasBoneChannel(const std::string& strBoneName) const
{
	return m_xBoneChannels.find(strBoneName) != m_xBoneChannels.end();
}

void Flux_AnimationClip::AddEvent(const Flux_AnimationEvent& xEvent)
{
	m_xEvents.push_back(xEvent);
	// Keep events sorted by time
	std::sort(m_xEvents.begin(), m_xEvents.end(),
		[](const Flux_AnimationEvent& a, const Flux_AnimationEvent& b) {
			return a.m_fNormalizedTime < b.m_fNormalizedTime;
		});
}

void Flux_AnimationClip::RemoveEvent(size_t uIndex)
{
	if (uIndex < m_xEvents.size())
		m_xEvents.erase(m_xEvents.begin() + uIndex);
}

void Flux_AnimationClip::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Metadata
	m_xMetadata.WriteToDataStream(xStream);

	// Source path
	xStream << m_strSourcePath;

	// Bone channels
	uint32_t uNumChannels = static_cast<uint32_t>(m_xBoneChannels.size());
	xStream << uNumChannels;
	for (const auto& xPair : m_xBoneChannels)
	{
		xPair.second.WriteToDataStream(xStream);
	}

	// Events
	uint32_t uNumEvents = static_cast<uint32_t>(m_xEvents.size());
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

	// Bone channels
	uint32_t uNumChannels = 0;
	xStream >> uNumChannels;
	m_xBoneChannels.clear();
	for (uint32_t i = 0; i < uNumChannels; ++i)
	{
		Flux_BoneChannel xChannel;
		xChannel.ReadFromDataStream(xStream);
		m_xBoneChannels.emplace(xChannel.GetBoneName(), std::move(xChannel));
	}

	// Events
	uint32_t uNumEvents = 0;
	xStream >> uNumEvents;
	m_xEvents.resize(uNumEvents);
	for (uint32_t i = 0; i < uNumEvents; ++i)
	{
		m_xEvents[i].ReadFromDataStream(xStream);
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

void Flux_AnimationClipCollection::AddClip(Flux_AnimationClip* pxClip)
{
	if (!pxClip)
		return;

	const std::string& strName = pxClip->GetName();

	// Remove existing clip with same name
	if (HasClip(strName))
		RemoveClip(strName);

	m_xClipsByName[strName] = pxClip;
	m_xClips.push_back(pxClip);
}

void Flux_AnimationClipCollection::RemoveClip(const std::string& strName)
{
	auto it = m_xClipsByName.find(strName);
	if (it != m_xClipsByName.end())
	{
		Flux_AnimationClip* pxClip = it->second;

		// Remove from ordered list
		auto vecIt = std::find(m_xClips.begin(), m_xClips.end(), pxClip);
		if (vecIt != m_xClips.end())
			m_xClips.erase(vecIt);

		// Remove from map
		m_xClipsByName.erase(it);

		// Delete clip
		delete pxClip;
	}
}

void Flux_AnimationClipCollection::Clear()
{
	for (Flux_AnimationClip* pxClip : m_xClips)
		delete pxClip;

	m_xClips.clear();
	m_xClipsByName.clear();
}

Flux_AnimationClip* Flux_AnimationClipCollection::GetClip(const std::string& strName)
{
	auto it = m_xClipsByName.find(strName);
	if (it != m_xClipsByName.end())
		return it->second;
	return nullptr;
}

const Flux_AnimationClip* Flux_AnimationClipCollection::GetClip(const std::string& strName) const
{
	auto it = m_xClipsByName.find(strName);
	if (it != m_xClipsByName.end())
		return it->second;
	return nullptr;
}

bool Flux_AnimationClipCollection::HasClip(const std::string& strName) const
{
	return m_xClipsByName.find(strName) != m_xClipsByName.end();
}

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

void Flux_AnimationClipCollection::WriteToDataStream(Zenith_DataStream& xStream) const
{
	uint32_t uNumClips = static_cast<uint32_t>(m_xClips.size());
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
