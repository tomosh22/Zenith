#pragma once
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_HashSet.h"
#include "Collections/Zenith_Vector.h"
#include <string>
#include <functional>
#include <utility>

// Forward declarations
#ifdef ZENITH_TOOLS
struct aiNodeAnim;
struct aiAnimation;
struct aiNode;
#endif
class Flux_MeshGeometry;

//=============================================================================
// Timestamped-keyframe (value, time) vector serialization helpers.
//
// The MeshAnimation system stores keyframes as Zenith_Vector<std::pair<V, float>>
// (V = Vector3 for position/scale, Quat for rotation) and serializes them with the
// recurring "uint32 count, then per key: value components then float time" block.
// These collapse that count+loop (and the Clear/Reserve/PushBack read scaffolding)
// to one call per vector. The on-disk format is unchanged — byte-identical to the
// former hand-rolled loops (Vec3: x,y,z,time; Quat: w,x,y,z,time).
//=============================================================================
void Flux_WriteVec3Keys(Zenith_DataStream& xStream, const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xKeys);
void Flux_ReadVec3Keys (Zenith_DataStream& xStream, Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xKeys);
void Flux_WriteQuatKeys(Zenith_DataStream& xStream, const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys);
void Flux_ReadQuatKeys (Zenith_DataStream& xStream, Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys);

//=============================================================================
// Per-key in/out tangents — RESERVED (decision D17).
//
// These are SERIALIZED and round-tripped now so the on-disk layout does not have
// to move again when curve-interpolated sampling lands; NOTHING SAMPLES THEM YET.
// Every channel carries one entry per keyframe, kept in lockstep by the channel's
// Add*Keyframe / SortKeyframes / read paths, and defaulting to zero (which is the
// "no tangent authored" value a linear sampler would ignore anyway).
//
// ★ A ROTATION TANGENT IS AN ANGULAR VELOCITY, NOT A QUATERNION CONTROL POINT.
// It is a Vector3 in axis * radians-per-second form — the same shape as a position
// or scale tangent's units-per-second — because the natural derivative of a slerped
// rotation curve is a body-frame angular velocity. Storing quaternion Bezier control
// points instead would be four components that only mean anything relative to their
// own segment's endpoints, and could not be blended or retimed.
//=============================================================================
struct Flux_KeyTangents
{
	Zenith_Maths::Vector3 m_xInTangent  = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xOutTangent = Zenith_Maths::Vector3(0.0f);
};

void Flux_WriteKeyTangents(Zenith_DataStream& xStream, const Zenith_Vector<Flux_KeyTangents>& xTangents);
void Flux_ReadKeyTangents (Zenith_DataStream& xStream, Zenith_Vector<Flux_KeyTangents>& xTangents);

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
#ifdef ZENITH_TOOLS
	Flux_BoneChannel(const aiNodeAnim* pxChannel);
#endif

	// Sample the channel at a specific time, returns local bone transform
	Zenith_Maths::Matrix4 Sample(float fTime) const;

	// Sample individual components
	Zenith_Maths::Vector3 SamplePosition(float fTime) const;
	Zenith_Maths::Quat SampleRotation(float fTime) const;
	Zenith_Maths::Vector3 SampleScale(float fTime) const;

	const std::string& GetBoneName() const { return m_strBoneName; }

	// Check if channel has keyframes for each component
	bool HasPositionKeyframes() const { return m_xPositions.GetSize() != 0; }
	bool HasRotationKeyframes() const { return m_xRotations.GetSize() != 0; }
	bool HasScaleKeyframes() const { return m_xScales.GetSize() != 0; }

	// Get keyframe data for export
	const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& GetPositionKeyframes() const { return m_xPositions; }
	const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& GetRotationKeyframes() const { return m_xRotations; }
	const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& GetScaleKeyframes() const { return m_xScales; }

	// RESERVED tangent block (D17). One entry per keyframe of the matching channel,
	// zero by default. Serialized and round-tripped; NOT sampled — Sample*() is still
	// pure lerp/slerp.
	const Zenith_Vector<Flux_KeyTangents>& GetPositionTangents() const { return m_xPositionTangents; }
	const Zenith_Vector<Flux_KeyTangents>& GetRotationTangents() const { return m_xRotationTangents; }
	const Zenith_Vector<Flux_KeyTangents>& GetScaleTangents()    const { return m_xScaleTangents; }

	void SetPositionTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents);
	void SetRotationTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents);
	void SetScaleTangent   (u_int uKeyIndex, const Flux_KeyTangents& xTangents);

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
	Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>> m_xPositions;
	Zenith_Vector<std::pair<Zenith_Maths::Quat, float>> m_xRotations;
	Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>> m_xScales;

	// RESERVED (D17) — parallel to the three keyframe arrays above, same size.
	Zenith_Vector<Flux_KeyTangents> m_xPositionTangents;
	Zenith_Vector<Flux_KeyTangents> m_xRotationTangents;  // ANGULAR velocity (axis * rad/s)
	Zenith_Vector<Flux_KeyTangents> m_xScaleTangents;
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

	// D6: the frame rate the clip was AUTHORED at, in frames per second. This is
	// editorial intent (what a key grid snaps to, what a re-bake should resample to)
	// and is deliberately NOT m_uTicksPerSecond, which is the tick->second divisor the
	// keyframe timestamps are already expressed in.
	uint32_t m_uAuthoredFrameRate = 30;

	// D7: the RIG this clip animates, and a model to preview it on. Both are asset
	// paths normalized through Zenith_AssetRegistry::NormalizeAssetPath on the way in
	// and out of the stream, exactly like Flux_AnimationClip::m_strSourcePath.
	//
	// ★ m_strSourcePath IS NOT THE RIG. It is the .glb / FBX the clip was IMPORTED
	// from — a provenance breadcrumb that is empty for every procedurally generated
	// clip — so overloading it as the skeleton reference would make a generated clip
	// unable to name its own rig and would silently retarget an imported one onto its
	// source file. These are separate fields on purpose.
	std::string m_strSkeletonPath;
	std::string m_strPreviewModelPath;

	// D8: true when the clip was produced by a generator rather than imported from an
	// authored source file. A generated clip is rewritten in full on every tools boot,
	// so this is what tells a consumer that editing it in place is pointless.
	bool m_bGenerated = false;

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
	Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>> m_xPositionDeltas;
	Zenith_Vector<std::pair<Zenith_Maths::Quat, float>> m_xRotationDeltas;

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

#ifdef ZENITH_TOOLS
	// Load from Assimp animation data (use Zenith_AnimationAsset for file loading)
	void LoadFromAssimp(const aiAnimation* pxAnimation, const aiNode* pxRootNode);
#endif

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
	const Zenith_HashMap<std::string, Flux_BoneChannel>& GetBoneChannels() const { return m_xBoneChannels; }

	//-------------------------------------------------------------------------
	// Programmatic clip construction (for procedural animations/tests)
	//-------------------------------------------------------------------------

	void AddBoneChannel(const std::string& strBoneName, Flux_BoneChannel&& xChannel);
	void SetDuration(float fDurationSeconds) { m_xMetadata.m_fDuration = fDurationSeconds; }
	void SetTicksPerSecond(uint32_t uTicksPerSecond) { m_xMetadata.m_uTicksPerSecond = uTicksPerSecond; }

	// Events
	const Zenith_Vector<Flux_AnimationEvent>& GetEvents() const { return m_xEvents; }
	void AddEvent(const Flux_AnimationEvent& xEvent);
	void RemoveEvent(u_int uIndex);

	// Root motion
	const Flux_RootMotion& GetRootMotion() const { return m_xRootMotion; }
	Flux_RootMotion& GetRootMotion() { return m_xRootMotion; }

	// Source path for serialization
	const std::string& GetSourcePath() const { return m_strSourcePath; }
	void SetSourcePath(const std::string& strPath) { m_strSourcePath = Zenith_AssetRegistry::NormalizeAssetPath(strPath); }

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	// A refused read (no envelope / not the current schema) must not leave a
	// half-parsed clip behind — the caller gets an EMPTY clip, the same contract the
	// navmesh reader has.
	void ResetToEmpty();

	Flux_AnimationClipMetadata m_xMetadata;
	Zenith_HashMap<std::string, Flux_BoneChannel> m_xBoneChannels;
	Zenith_Vector<Flux_AnimationEvent> m_xEvents;
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
	const Zenith_Vector<Flux_AnimationClip*>& GetClips() const { return m_xClips; }
	uint32_t GetClipCount() const { return m_xClips.GetSize(); }

#ifdef ZENITH_TOOLS
	// Load all animations from a file (may contain multiple clips)
	void LoadFromFile(const std::string& strPath);
#endif

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_HashMap<std::string, Flux_AnimationClip*> m_xClipsByName;
	Zenith_Vector<Flux_AnimationClip*> m_xClips;  // Ordered list for iteration
	Zenith_HashSet<Flux_AnimationClip*> m_xBorrowedClips;  // Non-owned references
};
