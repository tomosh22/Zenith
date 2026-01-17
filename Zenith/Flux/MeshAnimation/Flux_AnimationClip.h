#pragma once
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <functional>

#define ZENITH_ANIMATION_EXT ".zanim"

// Forward declarations
struct aiNodeAnim;
struct aiAnimation;
struct aiNode;
class Flux_MeshGeometry;

//=============================================================================
// Animation Event
// Callback triggered at specific times during animation playback
//=============================================================================
struct Flux_AnimationEvent
{
	float m_fNormalizedTime = 0.0f;  // Time in [0-1] range
	std::string m_strEventName;       // "FootstepLeft", "SwingStart", etc.
	Zenith_Maths::Vector4 m_xData;    // Optional event parameters

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Bone Channel
// Keyframe data for a single bone in an animation clip
//=============================================================================
class Flux_BoneChannel
{
public:
	Flux_BoneChannel() = default;
	Flux_BoneChannel(const aiNodeAnim* pxChannel);

	// Sample the channel at a specific time, returns local bone transform
	Zenith_Maths::Matrix4 Sample(float fTime) const;

	// Sample individual components
	Zenith_Maths::Vector3 SamplePosition(float fTime) const;
	Zenith_Maths::Quat SampleRotation(float fTime) const;
	Zenith_Maths::Vector3 SampleScale(float fTime) const;

	const std::string& GetBoneName() const { return m_strBoneName; }

	// Check if channel has keyframes for each component
	bool HasPositionKeyframes() const { return !m_xPositions.empty(); }
	bool HasRotationKeyframes() const { return !m_xRotations.empty(); }
	bool HasScaleKeyframes() const { return !m_xScales.empty(); }

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//-------------------------------------------------------------------------
	// Programmatic keyframe construction (for procedural animations/tests)
	//-------------------------------------------------------------------------

	void SetBoneName(const std::string& strName) { m_strBoneName = strName; }
	void AddPositionKeyframe(float fTimeTicks, const Zenith_Maths::Vector3& xPosition);
	void AddRotationKeyframe(float fTimeTicks, const Zenith_Maths::Quat& xRotation);
	void AddScaleKeyframe(float fTimeTicks, const Zenith_Maths::Vector3& xScale);
	void SortKeyframes();

private:
	friend class Flux_AnimationClip;

	// Find keyframe indices for interpolation
	uint32_t GetPositionIndex(float fTime) const;
	uint32_t GetRotationIndex(float fTime) const;
	uint32_t GetScaleIndex(float fTime) const;

	// Calculate interpolation factor between keyframes
	float GetScaleFactor(float fLastTime, float fNextTime, float fAnimTime) const;

	std::string m_strBoneName;

	// Keyframes stored as (value, timestamp) pairs
	std::vector<std::pair<Zenith_Maths::Vector3, float>> m_xPositions;
	std::vector<std::pair<Zenith_Maths::Quat, float>> m_xRotations;
	std::vector<std::pair<Zenith_Maths::Vector3, float>> m_xScales;
};

//=============================================================================
// Animation Clip Metadata
//=============================================================================
struct Flux_AnimationClipMetadata
{
	std::string m_strName;           // "Run", "Walk", "Idle", etc.
	float m_fDuration = 0.0f;        // Total duration in seconds
	uint32_t m_uTicksPerSecond = 24; // Animation sample rate
	bool m_bLooping = true;          // Does this clip loop?
	float m_fBlendInTime = 0.15f;    // Default blend-in duration
	float m_fBlendOutTime = 0.15f;   // Default blend-out duration

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Root Motion Data (optional)
// Extracts movement from root bone for gameplay integration
//=============================================================================
struct Flux_RootMotion
{
	bool m_bEnabled = false;
	std::vector<std::pair<Zenith_Maths::Vector3, float>> m_xPositionDeltas;
	std::vector<std::pair<Zenith_Maths::Quat, float>> m_xRotationDeltas;

	// Sample root motion delta at time
	Zenith_Maths::Vector3 SamplePositionDelta(float fTime) const;
	Zenith_Maths::Quat SampleRotationDelta(float fTime) const;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

//=============================================================================
// Animation Clip
// Complete animation data for one animation (e.g., "Walk", "Run", "Attack")
//=============================================================================
class Flux_AnimationClip
{
public:
	Flux_AnimationClip() = default;
	~Flux_AnimationClip() = default;

	// Load from Assimp animation data (use Zenith_AnimationAsset for file loading)
	void LoadFromAssimp(const aiAnimation* pxAnimation, const aiNode* pxRootNode);

	// Export to .zanim file
	void Export(const std::string& strPath) const;

	// Accessors
	const Flux_AnimationClipMetadata& GetMetadata() const { return m_xMetadata; }
	Flux_AnimationClipMetadata& GetMetadata() { return m_xMetadata; }

	const std::string& GetName() const { return m_xMetadata.m_strName; }
	void SetName(const std::string& strName) { m_xMetadata.m_strName = strName; }

	float GetDuration() const { return m_xMetadata.m_fDuration; }
	float GetDurationInTicks() const { return m_xMetadata.m_fDuration * m_xMetadata.m_uTicksPerSecond; }
	uint32_t GetTicksPerSecond() const { return m_xMetadata.m_uTicksPerSecond; }
	bool IsLooping() const { return m_xMetadata.m_bLooping; }
	void SetLooping(bool bLooping) { m_xMetadata.m_bLooping = bLooping; }

	// Bone channel access
	const Flux_BoneChannel* GetBoneChannel(const std::string& strBoneName) const;
	bool HasBoneChannel(const std::string& strBoneName) const;
	const std::unordered_map<std::string, Flux_BoneChannel>& GetBoneChannels() const { return m_xBoneChannels; }

	//-------------------------------------------------------------------------
	// Programmatic clip construction (for procedural animations/tests)
	//-------------------------------------------------------------------------

	void AddBoneChannel(const std::string& strBoneName, Flux_BoneChannel&& xChannel);
	void SetDuration(float fDurationSeconds) { m_xMetadata.m_fDuration = fDurationSeconds; }
	void SetTicksPerSecond(uint32_t uTicksPerSecond) { m_xMetadata.m_uTicksPerSecond = uTicksPerSecond; }

	// Events
	const std::vector<Flux_AnimationEvent>& GetEvents() const { return m_xEvents; }
	void AddEvent(const Flux_AnimationEvent& xEvent);
	void RemoveEvent(size_t uIndex);

	// Root motion
	const Flux_RootMotion& GetRootMotion() const { return m_xRootMotion; }
	Flux_RootMotion& GetRootMotion() { return m_xRootMotion; }

	// Source path for serialization
	const std::string& GetSourcePath() const { return m_strSourcePath; }
	void SetSourcePath(const std::string& strPath) { m_strSourcePath = strPath; }

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Flux_AnimationClipMetadata m_xMetadata;
	std::unordered_map<std::string, Flux_BoneChannel> m_xBoneChannels;
	std::vector<Flux_AnimationEvent> m_xEvents;
	Flux_RootMotion m_xRootMotion;
	std::string m_strSourcePath;
};

//=============================================================================
// Animation Clip Collection
// Manages multiple clips for a single mesh/skeleton
//=============================================================================
class Flux_AnimationClipCollection
{
public:
	Flux_AnimationClipCollection() = default;
	~Flux_AnimationClipCollection();

	// Non-copyable - owns dynamically allocated clips
	Flux_AnimationClipCollection(const Flux_AnimationClipCollection&) = delete;
	Flux_AnimationClipCollection& operator=(const Flux_AnimationClipCollection&) = delete;

	// Moveable - transfers ownership of clips
	Flux_AnimationClipCollection(Flux_AnimationClipCollection&& xOther) noexcept;
	Flux_AnimationClipCollection& operator=(Flux_AnimationClipCollection&& xOther) noexcept;

	// Add/remove clips
	void AddClip(Flux_AnimationClip* pxClip);  // Takes ownership
	void AddClipReference(Flux_AnimationClip* pxClip);  // Non-owning reference
	void RemoveClip(const std::string& strName);
	void Clear();

	// Lookup
	Flux_AnimationClip* GetClip(const std::string& strName);
	const Flux_AnimationClip* GetClip(const std::string& strName) const;
	bool HasClip(const std::string& strName) const;

	// Iteration
	const std::vector<Flux_AnimationClip*>& GetClips() const { return m_xClips; }
	uint32_t GetClipCount() const { return static_cast<uint32_t>(m_xClips.size()); }

	// Load all animations from a file (may contain multiple clips)
	void LoadFromFile(const std::string& strPath);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::unordered_map<std::string, Flux_AnimationClip*> m_xClipsByName;
	std::vector<Flux_AnimationClip*> m_xClips;  // Ordered list for iteration
	std::unordered_set<Flux_AnimationClip*> m_xBorrowedClips;  // Non-owned references
};
