#pragma once
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Core/Zenith_Result.h"        // Zenith_Status — the status-returning ParseStream
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
// Per-key in/out tangents (decision D17), SAMPLED since WU-8.1.
//
// Every channel carries one entry per keyframe of each of its three tracks — same
// size, zero by default, kept in lockstep by the channel's Add*Keyframe /
// SortKeyframes / mutation / read paths. They were serialized and round-tripped
// from D17 onward specifically so the on-disk layout would not have to move when
// curve-interpolated sampling landed. It did not: Sample*() now interpolates
// through them and THE WIRE FORMAT IS UNCHANGED — still schema 2, still the same
// bytes, because in/out per key and "a rotation tangent is an angular velocity"
// were the meanings D17 reserved.
//
// ★ A ROTATION TANGENT IS AN ANGULAR VELOCITY, NOT A QUATERNION CONTROL POINT.
// It is a Vector3 in axis * radians-per-second form — the same shape as a position
// or scale tangent's units-per-second — because the natural derivative of a slerped
// rotation curve is a body-frame angular velocity. Storing quaternion Bezier control
// points instead would be four components that only mean anything relative to their
// own segment's endpoints, and could not be blended or retimed.
//
// ★ AN ANGULAR VELOCITY IS IN ITS OWN KEY'S BODY FRAME. Key k's OUT tangent is
// expressed in the frame key k's own rotation defines, and key k+1's IN tangent in
// key k+1's — which is exactly what the sampler's endpoint-derivative identities
// produce (see "HOW A SEGMENT IS INTERPOLATED" below), and what lets an authored
// tangent survive an edit to the OTHER end of its segment.
//=============================================================================
struct Flux_KeyTangents
{
	Zenith_Maths::Vector3 m_xInTangent  = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xOutTangent = Zenith_Maths::Vector3(0.0f);
};

void Flux_WriteKeyTangents(Zenith_DataStream& xStream, const Zenith_Vector<Flux_KeyTangents>& xTangents);
void Flux_ReadKeyTangents (Zenith_DataStream& xStream, Zenith_Vector<Flux_KeyTangents>& xTangents);

//=============================================================================
// ★ AN UNSET (EXACTLY ZERO) TANGENT IS THE **LINEAR** TANGENT, NOT A FLAT ONE,
// AND THAT IS THE WHOLE REASON THIS BLOCK COULD BE TURNED ON WITHOUT MOVING A
// SINGLE CLIP IN THE TREE.
//
// The classical cubic-Hermite reading of a zero derivative is a FLAT (horizontal)
// tangent, which turns a straight segment into a smoothstep. Every generated clip
// in the tree, and every clip Assimp ever imported, carries zero in every tangent
// slot — so that reading would have silently re-timed every animation in the
// repository into an ease-in/ease-out: a change no unit, no byte comparison and no
// bake hash could see, because none of the DATA would have moved.
//
// So an unset tangent means "this end of this segment has no authored derivative",
// and the sampler substitutes the segment's own LINEAR slope there (for rotation,
// slerp's own constant angular velocity). A segment whose two bounding tangents are
// BOTH unset is evaluated by the pre-WU-8.1 code path verbatim — glm::mix /
// glm::slerp, with no Hermite arithmetic executed at all — so it is bit-identical
// rather than merely close, and that is also the branch every clip in the tree
// takes on every sample today.
//
// ★ THE TRADE, STATED OUT LOUD: a genuinely FLAT tangent (zero derivative — the
// "ease" handle) is NOT authorable, because the zero vector is spoken for.
// Flux_BoneChannel::ComputeFlatTangents writes zeroes and therefore produces LINEAR
// segments, which is today's behaviour and is what WU-8.1 specifies "flat" mode to
// mean. Telling flat from linear needs a per-key tangent MODE, and a mode is a
// field that is not on the wire — a schema move, deliberately not made here.
//
// The comparison is EXACT on purpose. The value being tested is a
// default-constructed sentinel that round-trips through a .zanim as exact zero
// bits, not a measurement, so there is no tolerance to choose — and any tolerance
// would swallow a deliberately tiny authored tangent instead.
//=============================================================================
inline bool Flux_TangentIsUnset(const Zenith_Maths::Vector3& xTangent)
{
	return xTangent.x == 0.0f && xTangent.y == 0.0f && xTangent.z == 0.0f;
}

//=============================================================================
// ★ ONE TIME-COMPARISON TOLERANCE FOR EVERY KEYFRAME MUTATION (D9).
//
// In SECONDS, on the same clock as every key time and the clip duration. Two key
// times within fANIM_TIME_EPSILON of each other ARE THE SAME TIME. Nothing below
// ever compares two key times with `==`: a time that arrives from a pointer drag,
// from a snap-to-frame and from a serialization round-trip is the same instant
// expressed three ways, and only one of the three lands on the same bit pattern.
//
// 1e-5 s sits ~420x below the finest authorable frame grid (1/240 s ~= 4.2e-3 s),
// so it can never merge two keys a user placed on adjacent frames; and ~10x ABOVE
// the float spacing at a key time in a clip of realistic length (2^-20 ~= 9.5e-7 s
// at t = 10 s), so it can never fail to recognise a key that has been through a
// file. The lower margin is the tight one, and it is the one to re-derive if a
// clip ever needs to be minutes long. This is the constant the dope sheet will share —
// a UI that picked its own hit-test tolerance would disagree with the mutator
// about whether a slot is occupied, which is the one disagreement that can
// destroy a key.
//=============================================================================
constexpr float fANIM_TIME_EPSILON = 1.0e-5f;

// The shortest quaternion a rotation mutator will accept (D15). A shorter one —
// the zero quaternion above all — is REFUSED, not normalized: glm::normalize of a
// zero-length quaternion is NaN, and one NaN rotation key poisons every pose the
// clip can produce, at every time, through the slerp.
constexpr float fANIM_MIN_QUAT_LENGTH = 1.0e-6f;

//=============================================================================
// Which of a channel's key arrays a mutation addresses.
//
// ★ ONE SELECTOR ENUM RATHER THAN THREE OVERLOAD FAMILIES, because the layer
// above addresses a key as (bone, TRACK, keyIndex) and has to store that triple
// in an undo record. Three families — RemovePositionKeyframe /
// RemoveRotationKeyframe / RemoveScaleKeyframe — would push a switch over the
// same three cases into every undo command, at every call site, once per verb.
//
// The VALUE, in contrast, is an overload: a position/scale key is a Vector3 and a
// rotation key is a Quat, and those are different types with no conversion
// between them, so the compiler picks. Passing a Vector3 with
// FLUX_ANIM_TRACK_ROTATION (or a Quat with POSITION/SCALE) is refused at runtime
// with an assert rather than silently reinterpreted.
//
// Flux_RootMotion takes the SAME enum (D16) and refuses FLUX_ANIM_TRACK_SCALE —
// it has a position and a rotation delta track and no third one.
//=============================================================================
enum Flux_AnimTrack
{
	FLUX_ANIM_TRACK_POSITION,
	FLUX_ANIM_TRACK_ROTATION,
	FLUX_ANIM_TRACK_SCALE,
};

//=============================================================================
// ★ KEY TIMES ARE SECONDS (D3). EVERY key time in this file — every
// std::pair<V, float>'s .second, every Add*Keyframe argument, every Sample*()
// argument — is a time in SECONDS on the same clock as
// Flux_AnimationClipMetadata::m_fDuration.
//
// They used to be TICKS: the channel stored aiVectorKey::mTime unconverted and
// Flux_SkeletonPose::SampleFromClip multiplied the incoming wall-clock seconds by
// the clip's ticks-per-second on the way in. That made a clip carry two clocks —
// a duration in seconds beside keys in ticks — and every generator, test and
// consumer had to remember which one it was holding. m_uTicksPerSecond survives
// as IMPORT PROVENANCE only (below); nothing multiplies or divides by it while
// sampling.
//
// Flux_AnimationEvent::m_fNormalizedTime is NOT part of this (D4) — an event time
// is a [0,1] fraction of the clip and stays one.
//=============================================================================

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
// HOW A SEGMENT IS INTERPOLATED (WU-8.1).
//
// Both formulas below reduce EXACTLY to the pre-tangent lerp / slerp when the two
// tangents bounding the segment are unset, and both are evaluated in place: no
// allocation, no virtual dispatch, no per-sample state. Shared setup for the
// segment between keys k and k+1:
//
//   dt = t_{k+1} - t_k        the segment duration, in seconds (dt <= 0 -> key k)
//   u  = (t - t_k) / dt       the normalized position inside it, in [0,1]
//   m0 = OUT tangent of key k     (unset -> the segment's own slope, see above)
//   m1 = IN  tangent of key k+1   (unset -> the same)
//
// POSITION and SCALE — cubic Hermite, tangents in UNITS PER SECOND:
//
//   P(u) = h00(u)*P0 + h10(u)*dt*m0 + h01(u)*P1 + h11(u)*dt*m1
//   h00 = 2u^3-3u^2+1   h10 = u^3-2u^2+u   h01 = -2u^3+3u^2   h11 = u^3-u^2
//
//   The dt factors are what make a tangent a VELOCITY rather than a per-segment
//   handle length: RETIME a key and the curve through it keeps the same physical
//   slope instead of silently changing shape. With m0 = m1 = s = (P1-P0)/dt the
//   four terms collapse algebraically to (1-u)*P0 + u*P1 — that identity is what
//   the "unset means linear" rule is built on.
//
// ROTATION — a cumulative-Bezier quaternion Hermite (Kim/Kim/Shin 1995), tangents
// in BODY-FRAME ANGULAR VELOCITY (axis * rad/s), each in its own key's frame:
//
//   qEnd = dot(q0,q1) < 0 ? -q1 : q1     shortest arc, the flip glm::slerp makes
//   r    = q0^-1 * qEnd                  the segment's relative rotation
//   v    = RotVec(r)                     ...as an axis*angle vector
//   w0, w1                               the two angular velocities
//                                        (unset -> v/dt, which IS slerp's own)
//   v1 = dt*w0/3                         v3 = dt*w1/3
//   v2 = RotVec( E(v1)^-1 * r * E(v3)^-1 )
//   q(u) = q0 * E(v1*B1(u)) * E(v2*B2(u)) * E(v3*B3(u))
//   B1 = 1-(1-u)^3   B2 = 3u^2-2u^3   B3 = u^3     (cumulative Bernstein basis)
//   E(x)      = angleAxis(|x|, x/|x|), identity as |x| -> 0
//   RotVec(q) = 2*atan2(|q.xyz|, q.w) * normalize(q.xyz), shortest arc
//
//   ★ WHY THE CUMULATIVE FORM AND NOT "SLERP PLUS A CORRECTION". This one is
//   EXACT AT BOTH ENDS, which is the only property that makes C1 across a key mean
//   anything:
//     • q(0) = q0 and q(1) = q1 by construction (every B is 0 at u=0, 1 at u=1);
//     • B1'(0) = 3 with B2'(0) = B3'(0) = 0, so the body-frame angular velocity at
//       u=0 is exactly 3*v1/dt = w0; symmetrically B3'(1) = 3 with B1'(1) =
//       B2'(1) = 0, so it is exactly w1 at u=1. A single-chart log-space Hermite
//       (q0 * E(hermite(u))) is exact at u=0 ONLY — the differential of the
//       exponential map is the identity at the origin and not at v — so its
//       end-of-segment velocity is wrong by O(|v|). That error is invisible in any
//       single pose and shows up as a hitch at every keyframe of a long segment.
//     • w0 = w1 = v/dt makes v1 = v2 = v3 = v/3, all about one axis, so they
//       commute and add; B1+B2+B3 = 3u, so q(u) = q0 * E(u*v) — slerp, exactly.
//       (The sampler short-circuits to glm::slerp in that case anyway; the
//       identity is what makes the two agree rather than merely nearly agree.)
//=============================================================================

//=============================================================================
// Bone Channel
// Keyframe data for a single bone in an animation clip
//=============================================================================
class Flux_BoneChannel
{
public:
	Flux_BoneChannel() = default;
#ifdef ZENITH_TOOLS
	// dSourceTicksPerSecond is the SOURCE file's tick rate (aiAnimation::mTicksPerSecond,
	// already defaulted by the caller when the file said 0). Assimp key times are ticks;
	// this constructor DIVIDES by it so what lands in the channel is seconds. It is a
	// required argument rather than a default so an import path cannot forget it.
	Flux_BoneChannel(const aiNodeAnim* pxChannel, double dSourceTicksPerSecond);
#endif

	// Sample the channel at a specific time IN SECONDS, returns local bone transform
	Zenith_Maths::Matrix4 Sample(float fTimeSeconds) const;

	// Sample individual components at a time IN SECONDS.
	//
	// Curve-interpolated through the per-key tangents since WU-8.1 — cubic Hermite
	// for position/scale, a cumulative-Bezier quaternion Hermite for rotation; see
	// "HOW A SEGMENT IS INTERPOLATED" above for both formulas. A segment bounded by
	// two UNSET tangents runs the pre-WU-8.1 glm::mix / glm::slerp code verbatim, so
	// a clip that authored none (which is every generated and every imported clip)
	// samples bit-identically to before and pays one Vector3 zero-compare for it.
	//
	// The empty / single-key / at-or-past-the-last-key behaviour is unchanged: the
	// origin, identity and unit scale for an empty track (see D14 on why an empty
	// channel must not survive inside a clip), the sole key's value for a single-key
	// track, and a CLAMP to the last key past the end — never an extrapolation.
	Zenith_Maths::Vector3 SamplePosition(float fTimeSeconds) const;
	Zenith_Maths::Quat SampleRotation(float fTimeSeconds) const;
	Zenith_Maths::Vector3 SampleScale(float fTimeSeconds) const;

	const std::string& GetBoneName() const { return m_strBoneName; }

	// Check if channel has keyframes for each component
	bool HasPositionKeyframes() const { return m_xPositions.GetSize() != 0; }
	bool HasRotationKeyframes() const { return m_xRotations.GetSize() != 0; }
	bool HasScaleKeyframes() const { return m_xScales.GetSize() != 0; }

	// Get keyframe data for export
	const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& GetPositionKeyframes() const { return m_xPositions; }
	const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& GetRotationKeyframes() const { return m_xRotations; }
	const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& GetScaleKeyframes() const { return m_xScales; }

	// The tangent block (D17). One entry per keyframe of the matching track, zero by
	// default, serialized and round-tripped, and SAMPLED since WU-8.1. A rotation
	// tangent is an angular velocity; an exactly-zero tangent is the LINEAR one —
	// see Flux_KeyTangents and Flux_TangentIsUnset above.
	const Zenith_Vector<Flux_KeyTangents>& GetPositionTangents() const { return m_xPositionTangents; }
	const Zenith_Vector<Flux_KeyTangents>& GetRotationTangents() const { return m_xRotationTangents; }
	const Zenith_Vector<Flux_KeyTangents>& GetScaleTangents()    const { return m_xScaleTangents; }

	void SetPositionTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents);
	void SetRotationTangent(u_int uKeyIndex, const Flux_KeyTangents& xTangents);
	void SetScaleTangent   (u_int uKeyIndex, const Flux_KeyTangents& xTangents);

	//-------------------------------------------------------------------------
	// Tangent PRESETS (WU-8.1). Two pure, allocation-free whole-track rewrites,
	// both of which go through the Set*Tangent setters above so there is exactly
	// one write path into the parallel arrays.
	//
	// ★ A TANGENT MODE IS AN EDITOR CONCEPT AND IS NOT STORED. The clip carries
	// numbers, never "this key is auto" — a mode is not on the wire (D17 reserved
	// two vectors per key and nothing else) and putting one there is a schema move.
	// So a curve editor owns the mode, and calls one of these to REALISE it as
	// numbers; re-applying after a key moves is the editor's job, because the clip
	// cannot know whether a stored tangent was authored by hand or computed.
	//
	// ComputeAutoTangents — Catmull-Rom style. For an interior key k, in = out =
	// the centred slope (v_{k+1} - v_{k-1}) / (t_{k+1} - t_{k-1}); the two endpoint
	// keys get the one-sided slope of the single segment they bound. A track with
	// fewer than two keys, and any key whose neighbour span is non-positive, gets
	// zero — which is the LINEAR tangent, the honest answer when there is no slope
	// to measure.
	//
	// For ROTATION the same construction in angular-velocity terms: the rotation
	// vector of q_{k-1}^-1 * q_{k+1} over (t_{k+1} - t_{k-1}), then rotated into key
	// k's OWN body frame by q_k^-1 * q_{k-1} — because that is the frame the
	// sampler reads a tangent in, and the raw relative rotation comes out in the
	// EARLIER key's frame. On a sweep about a fixed body axis the two frames agree
	// and the result is a constant angular velocity, which is the property the units
	// pin.
	//
	// ComputeFlatTangents — zero on every key of the track. ★ THAT IS *LINEAR*, NOT
	// a flat/eased handle: read the Flux_TangentIsUnset block above before wiring a
	// UI label to it. It is exactly "put this track back the way every clip in the
	// tree already is".
	//-------------------------------------------------------------------------
	void ComputeAutoTangents(Flux_AnimTrack eTrack);
	void ComputeFlatTangents(Flux_AnimTrack eTrack);

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	//-------------------------------------------------------------------------
	// Programmatic keyframe construction (for procedural animations/tests)
	//-------------------------------------------------------------------------

	// fTimeSeconds is SECONDS on the clip's own clock — the same clock
	// Flux_AnimationClip::SetDuration takes. NOT frames, NOT ticks.
	void SetBoneName(const std::string& strName) { m_strBoneName = strName; }
	void AddPositionKeyframe(float fTimeSeconds, const Zenith_Maths::Vector3& xPosition);
	void AddRotationKeyframe(float fTimeSeconds, const Zenith_Maths::Quat& xRotation);
	void AddScaleKeyframe(float fTimeSeconds, const Zenith_Maths::Vector3& xScale);
	void SortKeyframes();

	// The LATEST authored key time across all three channels, in seconds, or 0 when
	// the channel is empty. Keys need not be sorted — this takes the max, so it is
	// usable straight after authoring.
	float GetLastKeyTimeSeconds() const;

	//-------------------------------------------------------------------------
	// Keyframe MUTATION (WU-1.3).
	//
	// Before these existed the clip was APPEND-ONLY — Add*Keyframe plus
	// SortKeyframes, three private vectors and const-only getters — so nothing
	// could remove, retime or revalue a key. An editor needs all three.
	//
	// Shared contract, and it holds for every entry point here and on
	// Flux_RootMotion:
	//
	//  • Each returns TRUE only when the clip actually changed. A refusal changes
	//    NOTHING — no clamp, no merge, no partial edit — so a caller may retry or
	//    abandon the edit without first reading the state back.
	//  • The tangent array (D17) is kept exactly parallel: a removal drops
	//    the matching tangent, an insert adds a zero one at the same index, a retime
	//    carries the tangent with its key, and a value replace leaves it alone.
	//  • The track is left TIME-SORTED. There is no SortKeyframes() to remember
	//    afterwards, and the input is asserted to be sorted going in (a channel
	//    built with Add*Keyframe and never sorted has no defined insert position).
	//  • The clip DURATION is never touched (D12), and a key past the duration is
	//    permitted (D13) — the panel warns about it, the mutator does not veto it.
	//  • Write order stays deterministic for free (D5): channels are serialized in
	//    bone-name order by Flux_AnimationClip::WriteToDataStream, so a mutation
	//    only has to keep the hash map coherent, never an ordering.
	//-------------------------------------------------------------------------

	u_int GetKeyframeCount(Flux_AnimTrack eTrack) const;

	// The time of one key, in seconds. False (and fOutTimeSeconds untouched) when
	// the index is out of range.
	bool GetKeyframeTime(Flux_AnimTrack eTrack, u_int uKeyIndex, float& fOutTimeSeconds) const;

	// The index of the key AT fTimeSeconds — within fANIM_TIME_EPSILON, never an
	// exact compare (D9) — or GetKeyframeCount(eTrack) when the time is free.
	u_int FindKeyframeAtTime(Flux_AnimTrack eTrack, float fTimeSeconds) const;

	// Out-of-range index: false + assert. NOTE this does NOT drop an emptied channel
	// from its clip (a channel cannot reach the map that owns it) — that is D14, and
	// it lives on Flux_AnimationClip::RemoveKeyframe / PruneEmptyChannel.
	bool RemoveKeyframe(Flux_AnimTrack eTrack, u_int uKeyIndex);

	// Retime one key. Refused (false, nothing changed) when another key already sits
	// within fANIM_TIME_EPSILON of fNewTimeSeconds — NO SILENT MERGE (D11/D25), a
	// merge destroys a key during a drag, which is exactly when a user is least able
	// to notice. Otherwise the key and its tangent move together to their new sorted
	// position, reported through puOutKeyIndex.
	bool SetKeyframeTime(Flux_AnimTrack eTrack, u_int uKeyIndex, float fNewTimeSeconds, u_int* puOutKeyIndex = nullptr);

	// Revalue one key in place; its time, its slot and its tangent are untouched.
	bool SetKeyframeValue(Flux_AnimTrack eTrack, u_int uKeyIndex, const Zenith_Maths::Vector3& xValue);
	// The rotation overload NORMALIZES on write and refuses a quaternion shorter
	// than fANIM_MIN_QUAT_LENGTH (D15).
	bool SetKeyframeValue(Flux_AnimTrack eTrack, u_int uKeyIndex, const Zenith_Maths::Quat& xRotation);

	// On an OCCUPIED time (within epsilon) this REPLACES that key's value in place,
	// keeping its index slot, its stored time and its tangent entry; on a free time
	// it inserts at the sorted position with a zero tangent. Either way the
	// resulting index comes back through puOutKeyIndex.
	bool InsertKeyframeAt(Flux_AnimTrack eTrack, float fTimeSeconds, const Zenith_Maths::Vector3& xValue, u_int* puOutKeyIndex = nullptr);
	bool InsertKeyframeAt(Flux_AnimTrack eTrack, float fTimeSeconds, const Zenith_Maths::Quat& xRotation, u_int* puOutKeyIndex = nullptr);

	// No keys at all, on any of the three tracks — the state D14 says must not survive
	// inside a clip. See Flux_AnimationClip::RemoveKeyframe for what an empty channel
	// actually does to each of the two samplers; they do NOT agree, which is half the
	// reason for getting rid of it.
	bool IsEmpty() const { return !HasPositionKeyframes() && !HasRotationKeyframes() && !HasScaleKeyframes(); }

private:
	friend class Flux_AnimationClip;

	// Find keyframe indices for interpolation
	uint32_t GetPositionIndex(float fTimeSeconds) const;
	uint32_t GetRotationIndex(float fTimeSeconds) const;
	uint32_t GetScaleIndex(float fTimeSeconds) const;

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

	// ★ IMPORT PROVENANCE ONLY (D3). The tick rate of the FILE this clip was imported
	// from — what aiAnimation::mTicksPerSecond said, so a re-export to Assimp/glTF can
	// put the key times back on the source's own grid. NOTHING SAMPLES THROUGH IT:
	// key times are already seconds by the time they reach a channel, and
	// Flux_SkeletonPose::SampleFromClip no longer multiplies by it. A procedurally
	// generated clip may set it to whatever grid it authored on, or leave it at 24 —
	// the pose it produces is identical either way, which is exactly the property the
	// Null-backend units pin.
	uint32_t m_uTicksPerSecond = 24;

	bool m_bLooping = true;          // Does this clip loop?
	float m_fBlendInTime = 0.15f;    // Default blend-in duration
	float m_fBlendOutTime = 0.15f;   // Default blend-out duration

	// D6: the frame rate the clip was AUTHORED at, in frames per second. This is
	// editorial intent (what a key grid snaps to, what a re-bake should resample to)
	// and is deliberately NOT m_uTicksPerSecond, which is the SOURCE FILE's tick rate
	// kept as import provenance (see above). Neither one is applied to a key time.
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

	//-------------------------------------------------------------------------
	// Keyframe MUTATION (WU-1.3 / D16) — the SAME verbs, the SAME selector enum
	// and the SAME policy as Flux_BoneChannel's, on the two delta tracks.
	// FLUX_ANIM_TRACK_SCALE is refused (false + assert): there is no scale track.
	//
	// ★ ROOT MOTION CARRIES NO TANGENTS, and deliberately gains none here. Its two
	// delta arrays are the same Zenith_Vector<std::pair<V, float>> shape a bone
	// channel's are, but there is no parallel Flux_KeyTangents array beside them and
	// none is added — adding one would move the .zanim layout, and this unit changes
	// no on-disk bytes. The mutators share the bone channel's implementation and
	// simply pass no tangent array, so the lockstep rule is vacuous here rather than
	// re-implemented (and therefore cannot drift from it).
	//
	// ★ AND SO ROOT MOTION IS STILL SAMPLED LINEARLY, deliberately, after WU-8.1
	// turned the bone channel's tangents on. SamplePositionDelta /
	// SampleRotationDelta are unchanged lerp / slerp because there is no tangent
	// array here to read. A root-motion delta is integrated by gameplay into a
	// world transform, so a curve through it would need the same treatment on the
	// consumer side; giving it tangents is a wire change (WU-1.3 declined it for
	// that reason) and a separate decision.
	//-------------------------------------------------------------------------
	u_int GetKeyframeCount(Flux_AnimTrack eTrack) const;
	bool  GetKeyframeTime(Flux_AnimTrack eTrack, u_int uKeyIndex, float& fOutTimeSeconds) const;
	u_int FindKeyframeAtTime(Flux_AnimTrack eTrack, float fTimeSeconds) const;
	bool  RemoveKeyframe(Flux_AnimTrack eTrack, u_int uKeyIndex);
	bool  SetKeyframeTime(Flux_AnimTrack eTrack, u_int uKeyIndex, float fNewTimeSeconds, u_int* puOutKeyIndex = nullptr);
	bool  SetKeyframeValue(Flux_AnimTrack eTrack, u_int uKeyIndex, const Zenith_Maths::Vector3& xValue);
	bool  SetKeyframeValue(Flux_AnimTrack eTrack, u_int uKeyIndex, const Zenith_Maths::Quat& xRotation);
	bool  InsertKeyframeAt(Flux_AnimTrack eTrack, float fTimeSeconds, const Zenith_Maths::Vector3& xValue, u_int* puOutKeyIndex = nullptr);
	bool  InsertKeyframeAt(Flux_AnimTrack eTrack, float fTimeSeconds, const Zenith_Maths::Quat& xRotation, u_int* puOutKeyIndex = nullptr);

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

	// ★ COPYABLE BY DESIGN, AND DECLARED SO EXPLICITLY. ReplaceContentsFrom (below)
	// is a whole-object copy assignment — that is what makes it impossible to forget
	// a member — so the copy operations are part of this class's contract rather than
	// an accident. They are spelled out because an IMPLICITLY defined copy assignment
	// on a class with a user-declared destructor is deprecated in C++20, and a clang
	// build with that warning enabled would fail on it; an explicitly defaulted one is
	// not. The clip owns no raw pointers and no handles, so the default is a deep copy.
	Flux_AnimationClip(const Flux_AnimationClip&) = default;
	Flux_AnimationClip& operator=(const Flux_AnimationClip&) = default;

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

	// ★ THERE IS NO GetDurationInTicks(). It was a SECOND authority on the clip's
	// length, expressed in the one unit no key time is in any more; every caller of
	// it was sampling with a tick number. Sample with GetDuration() — seconds.
	//
	// The one legitimate consumer is a re-export to a tick-based file format, which
	// wants `GetDuration() * GetTicksPerSecond()` written at the call site where the
	// tick grid is visible (see Tools/Zenith_Tools_AssimpConvert.cpp).
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

	//-------------------------------------------------------------------------
	// Channel MUTATION (WU-1.3). The clip owns the bone-name -> channel map, so
	// anything that changes WHICH channels exist has to happen here; the per-key
	// verbs live on Flux_BoneChannel and are reached through the mutable accessor.
	//-------------------------------------------------------------------------

	// The channel for a bone, WRITABLE, or nullptr when the bone has none. This is
	// how the document/undo layer reaches Flux_BoneChannel's mutators; before it
	// existed the only non-const route into a channel was `friend class
	// Flux_AnimationClip`, which no caller outside this class can use.
	Flux_BoneChannel* GetBoneChannelMutable(const std::string& strBoneName);

	// As above, creating an empty channel (with its bone name already set) when the
	// bone has none. ★ The returned channel is EMPTY, which is the one state D14
	// forbids inside a clip — so a caller that takes this reference and then fails
	// to add a key has left the clip in that state. Add the key, or call
	// PruneEmptyChannel on the way out of the edit.
	Flux_BoneChannel& GetOrAddBoneChannel(const std::string& strBoneName);

	// Drop a bone's channel outright. False when the bone had none.
	bool RemoveBoneChannel(const std::string& strBoneName);

	// ★ THE D14 ENTRY POINT. Removes one key and, when that leaves the channel with
	// zero position AND zero rotation AND zero scale keys, removes the CHANNEL from
	// the clip.
	//
	// ★ AN EMPTY CHANNEL IS NOT NEUTRAL, AND THE TWO SAMPLERS DISAGREE ABOUT IT.
	// Flux_SkeletonPose::SampleFromClip guards each track with Has*Keyframes() and so
	// leaves the bind pose untouched (Flux_BonePose.cpp) — but the DIRECT channel API,
	// Flux_BoneChannel::Sample() / SamplePosition() / SampleRotation() / SampleScale(),
	// has no such guard and returns the origin, the identity rotation and unit scale.
	// So the same empty channel is invisible through one entry point and an authored
	// origin pose through the other. Removing it is what makes "this bone is not
	// animated" have exactly ONE representation — an absent channel — instead of two
	// that behave differently.
	//
	// A caller that mutates through GetBoneChannelMutable() instead bypasses this
	// (the channel cannot reach the map that owns it) and must call
	// PruneEmptyChannel itself once the edit is finished.
	bool RemoveKeyframe(const std::string& strBoneName, Flux_AnimTrack eTrack, u_int uKeyIndex);

	// Remove the named channel IF it is empty. True only when it existed AND was
	// empty AND has now been removed — so it is safe to call unconditionally after
	// any edit, and says whether it did anything.
	bool PruneEmptyChannel(const std::string& strBoneName);

	// Records the grid this clip was imported from / authored on. It does NOT
	// reinterpret any key time — see m_uTicksPerSecond. Calling it changes nothing a
	// sampler can observe.
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

	//-------------------------------------------------------------------------
	// IN-PLACE CONTENT REPLACEMENT (WU-2.1 / D26).
	//
	// ★ A RELOAD MUST NOT MOVE THE CLIP. A controller borrows the clip POINTER
	// (Flux_AnimationClipCollection::AddClipReference) and a state machine resolves
	// its clip references THROUGH that collection by name
	// (Flux_AnimationStateMachine::ResolveClipReferences). Freeing the clip and
	// acquiring a new one — which is all `ForceUnload` + a fresh acquire can do —
	// hands back a DIFFERENT address, so every borrowed pointer and every resolved
	// blend-tree node is left pointing at freed memory. This copies xSource's
	// contents INTO this object instead, so the address a controller holds keeps
	// working and starts observing the new content on the very next sample.
	//
	// Everything the clip owns moves: metadata (name, duration, ticks-per-second,
	// looping, blend times, authored frame rate, skeleton/preview paths, generated
	// flag), the bone-channel map, the event list, the root motion block and the
	// source path. It is implemented as a whole-object copy assignment rather than a
	// field list ON PURPOSE — a member added to this class later is part of the
	// replace automatically instead of being silently left behind.
	//
	// ★ THE NAME IS IMMUTABLE ACROSS A REPLACE (D28). The collection is name-keyed,
	// AddClip on a name collision DELETES the clip that was already there, and
	// ResolveClipReferences resolves through the same map — so a rename underneath a
	// live clip corrupts two lookups at once and neither one reports it. A source
	// whose name differs is REFUSED: it asserts and returns false, and this clip is
	// left EXACTLY as it was. The one exception is a destination with no name yet —
	// a freshly constructed clip being populated is not a rename. Renaming a clip is
	// Save As, and is a different operation.
	//
	// ★ THIS IS NOT A SYNCHRONISATION POINT AND DOES NOT CREATE ONE (D27). It is a
	// plain non-atomic write over live data: the CALLER must guarantee no animation
	// update is in flight against this clip (the editor calls it from the main
	// thread, between frames). Load and validate into a TEMPORARY clip first and
	// only then call this — Zenith_AnimationAsset::ReloadFromDisk is the worked
	// example — so a file that fails to parse never reaches a live clip at all.
	bool ReplaceContentsFrom(const Flux_AnimationClip& xSource);

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;

	// ★ THE READER REPORTS FAILURE, AND THIS IS THE ENTRY POINT THAT SAYS SO.
	// ParseStream is the status-returning parse — the same shape the sibling asset
	// Zenith_SkeletonAsset uses — and it is what any load path must call if it wants
	// to know whether the bytes were a .zanim at all:
	//   • no envelope (or a stream too short to hold one) -> BAD_MAGIC
	//   • an envelope carrying another asset's type id    -> INVALID_ARGUMENT
	//   • a newer envelope, or any schema that is not
	//     uZENITH_ANIMATION_SCHEMA_CURRENT                -> VERSION_MISMATCH
	// Every refusal also asserts EXACTLY ONCE and leaves this clip EMPTY
	// (ResetToEmpty), never half-parsed.
	//
	// It used to be that ReadFromDataStream was `void` and Zenith_AnimationAsset::
	// LoadFromFile returned true unconditionally, so a refused .zanim produced an
	// assert, an empty clip AND a "successful" asset load — three signals, none of
	// which reached the caller.
	//
	// The void ReadFromDataStream below is kept only for Zenith_DataStream's <</>>
	// dispatch (operator>> discards the return value); it is NOT the load contract.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

	//-------------------------------------------------------------------------
	// ★ THE PAYLOAD READER, AND IT EXISTS SPLIT OUT FOR EXACTLY ONE CALLER
	// BESIDES ParseStream: THE AUTHORED-CLIP MIGRATOR (WU-2.5 / D22 / D23,
	// Tools/Zenith_Tools_AnimMigrate.cpp).
	//
	// Reads the clip BODY — metadata, source path, channels, events, root motion
	// — from a cursor already positioned immediately after the envelope. It does
	// NOT read, validate or write a header; ParseStream owns that.
	//
	// ★ THE RUNTIME NEVER CALLS THIS WITH A NON-CURRENT SCHEMA, AND IN A
	// NON-TOOLS BUILD IT CANNOT: the acceptance test below narrows to
	// `== uZENITH_ANIMATION_SCHEMA_CURRENT` outside ZENITH_TOOLS, so the D2
	// ruling ("the envelope is mandatory and the schema must be current, there is
	// no legacy branch in the runtime reader") is enforced by the compiler and
	// not by convention. ParseStream is unchanged: read header -> refuse anything
	// that is not the current schema -> ParsePayload(current).
	//
	// Schemas 1 and 2 share a byte layout EXACTLY — schema 2 reinterpreted the
	// key-time floats as SECONDS where schema 1 meant TICKS, and no field moved —
	// so there is one body reader here and the unit conversion is a migrator STEP,
	// not a branch in this function. The schema argument exists so that a future
	// layout change has somewhere to branch that only tools-called code reaches.
	//
	// Every refusal asserts exactly once and leaves this clip EMPTY, the same
	// contract ParseStream has.
	//-------------------------------------------------------------------------
	Zenith_Status ParsePayload(Zenith_DataStream& xStream, u_int uSchemaVersion);

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
// Key-time / duration agreement (D3).
//
// Now that a key time and a duration are in the SAME unit, "the last key lands at
// or before the end of the clip" is a checkable property — and it is exactly the
// property a generator loses when it authors on one grid and states its length on
// another. Before D3 the two were incomparable, which is why nothing checked it
// and why a 24x error could sit in a generator with every unit green.
//
// Both are pure and allocation-free, so a generator may assert with them at bake
// time and a headless unit may assert with them on the same clip.
//=============================================================================

// The latest key time in the clip, across every channel and all three key arrays,
// in seconds. Zero for a clip with no keys.
float Flux_ClipLastKeyTimeSeconds(const Flux_AnimationClip& xClip);

// True when every authored key time lies in [-fEpsilonSeconds, duration + fEpsilonSeconds].
// A clip with no channels is vacuously true; a clip with a non-positive duration is
// FALSE when it carries any key past the epsilon, because that is the shape a
// forgotten SetDuration leaves behind.
bool Flux_ClipKeyTimesFitDuration(const Flux_AnimationClip& xClip, float fEpsilonSeconds = 1.0e-4f);

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

	// Add/remove clips.
	//
	// ★ BOTH REQUIRE A NON-EMPTY CLIP NAME, and assert on one that is empty. This
	// collection is keyed by name, so an unnamed clip keys on "" — two of them evict
	// each other from m_xClipsByName (AddClip DELETES the one already there) while
	// BOTH stay in m_xClips, leaving the ordered list and the map disagreeing about
	// what the collection contains and a freed pointer in one of them. There is no
	// useful lookup for an unnamed clip either: GetClip("") is not something a state
	// machine or a blend tree ever asks for. Name the clip before adding it.
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
