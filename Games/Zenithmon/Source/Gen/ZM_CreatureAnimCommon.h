#pragma once

// ============================================================================
// ZM_CreatureAnimCommon -- the ZM-prefixed, LINKABLE analogue of the StickFigure
// procedural-clip helpers (Tools/Zenith_Tools_TestAssetExport.cpp), which live in
// an anonymous namespace in a Tools TU and are therefore un-linkable. Every
// ZM_CreatureAnimGen archetype builder composes this kit to author its rotation
// channels.
//
// ONE UNIT (D3): Flux_AnimationClip::SetDuration and every keyframe time are both
// SECONDS. Author time = t01 * durationSeconds. Use ZM_AnimTimeForT01 so no builder
// hand-computes it.
//
// ★ THIS USED TO BE A UNIT DUALITY, and that is why the helper is still here. The
// duration was seconds while key times were TICKS (t01 * durationSeconds * 24), and
// Flux_SkeletonPose::SampleFromClip multiplied wall-clock seconds by the clip's
// ticks-per-second to meet them. ZM_AnimTicksForT01 was RENAMED to ZM_AnimTimeForT01
// rather than quietly changed, so a builder that still spells the old name fails to
// compile instead of authoring a clip 24x too long.
//
// ROTATION-ONLY: this kit builds rotation channels only (the sampler REPLACES a
// bone's bind-local TRS, so an absolute local rotation is meaningful and identical
// for every species; a position channel would break cross-species purity). There
// is deliberately NO position/scale helper here.
// ============================================================================

#include "Flux/MeshAnimation/Flux_AnimationClip.h"   // Flux_AnimationClip, Flux_BoneChannel
#include "Maths/Zenith_Maths.h"                       // Zenith_Maths::Quat, angleAxis path
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"  // uZM_CREATURE_ANIM_TICKS_PER_SECOND

#include <utility>   // std::move

// An explicit (time-in-seconds, rotation) key for the one-shot action clips.
struct ZM_AnimRotKey
{
	float              m_fTimeSeconds = 0.0f;
	Zenith_Maths::Quat m_xRot         = glm::identity<Zenith_Maths::Quat>();
};

// Single-axis absolute local rotations (degrees). glm quats are WXYZ; angleAxis
// takes radians. These are the atoms every builder composes.
Zenith_Maths::Quat ZM_AnimRotX(float fDeg);
Zenith_Maths::Quat ZM_AnimRotY(float fDeg);
Zenith_Maths::Quat ZM_AnimRotZ(float fDeg);

// Compose two local rotations (quat multiply; applies xB first, then xA).
Zenith_Maths::Quat ZM_AnimRotCompose(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB);

// Convert a normalized clip fraction t01 to a keyframe TIME IN SECONDS, given the
// clip duration in seconds. Key times and duration share one clock (D3), so this is
// a plain scale — and ZM_AnimTimeForT01(1.0f, d) == d exactly, which is what makes
// "the last key lands on the duration" checkable (Flux_ClipKeyTimesFitDuration).
//
// It survives as a named helper rather than being inlined at ~250 call sites so the
// clip's time base stays one edit away, and so the rename above forces a recompile
// of every builder.
inline float ZM_AnimTimeForT01(float fT01, float fDurationSeconds)
{
	return fT01 * fDurationSeconds;
}

// Sample a continuous rotation curve into a bone channel: uKeys keys EVENLY
// spaced INCLUSIVE of both t01=0 and t01=1 (key0 AND keyN emitted), so a looping
// builder that authors pfnT01ToQuat with fn(0)==fn(1) never pops at the loop
// seam. pfnT01ToQuat is a template parameter so builders may pass small stateless
// lambdas (with value captures) -- signature Zenith_Maths::Quat(float fT01).
// Builds the channel, adds each (timeSeconds, quat), sorts, then moves it into
// xClip. fDurationSeconds is the clip's own duration, in SECONDS (D3).
// Channels are inserted in a fixed order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
template <typename TFn>
void ZM_AnimAddRotCurve(Flux_AnimationClip& xClip, const char* szBone,
	float fDurationSeconds, u_int uKeys, TFn&& pfnT01ToQuat)
{
	Zenith_Assert(uKeys >= 2u, "ZM_AnimAddRotCurve: need >= 2 keys for bone '%s'", szBone);

	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uKeys; ++u)
	{
		const float fT01 = static_cast<float>(u) / static_cast<float>(uKeys - 1u);
		xChannel.AddRotationKeyframe(fT01 * fDurationSeconds, pfnT01ToQuat(fT01));
	}
	xChannel.SortKeyframes();
	xClip.AddBoneChannel(szBone, std::move(xChannel));
}

// Author an explicit key-posed rotation channel (anticipation -> strike ->
// recovery for the one-shot clips). Builds the channel, adds each
// (timeSeconds, quat), sorts, then moves it into xClip.
void ZM_AnimAddRotKeys(Flux_AnimationClip& xClip, const char* szBone,
	const ZM_AnimRotKey* pxKeys, u_int uCount);
