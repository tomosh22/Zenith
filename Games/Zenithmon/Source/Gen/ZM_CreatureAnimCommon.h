#pragma once

// ============================================================================
// ZM_CreatureAnimCommon -- the ZM-prefixed, LINKABLE analogue of the StickFigure
// procedural-clip helpers (Tools/Zenith_Tools_TestAssetExport.cpp), which live in
// an anonymous namespace in a Tools TU and are therefore un-linkable. Every
// ZM_CreatureAnimGen archetype builder composes this kit to author its rotation
// channels.
//
// UNIT DUALITY (load-bearing): Flux_AnimationClip::SetDuration is in SECONDS but
// keyframe times are in TICKS. Author tick = t01 * durationSeconds * 24. Use
// ZM_AnimTicksForT01 so no builder hand-computes the conversion.
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

// An explicit (tick, rotation) key for the one-shot action clips.
struct ZM_AnimRotKey
{
	float              m_fTick = 0.0f;
	Zenith_Maths::Quat m_xRot  = glm::identity<Zenith_Maths::Quat>();
};

// Single-axis absolute local rotations (degrees). glm quats are WXYZ; angleAxis
// takes radians. These are the atoms every builder composes.
Zenith_Maths::Quat ZM_AnimRotX(float fDeg);
Zenith_Maths::Quat ZM_AnimRotY(float fDeg);
Zenith_Maths::Quat ZM_AnimRotZ(float fDeg);

// Compose two local rotations (quat multiply; applies xB first, then xA).
Zenith_Maths::Quat ZM_AnimRotCompose(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB);

// Convert a normalized clip fraction t01 to a keyframe tick given the clip
// duration in SECONDS: t01 * durationSeconds * ticksPerSecond. THE single unit-
// conversion point (so total ticks == ZM_AnimTicksForT01(1.0f, durationSeconds)).
inline float ZM_AnimTicksForT01(float fT01, float fDurationSeconds)
{
	return fT01 * fDurationSeconds * static_cast<float>(uZM_CREATURE_ANIM_TICKS_PER_SECOND);
}

// Sample a continuous rotation curve into a bone channel: uKeys keys EVENLY
// spaced INCLUSIVE of both t01=0 and t01=1 (key0 AND keyN emitted), so a looping
// builder that authors pfnT01ToQuat with fn(0)==fn(1) never pops at the loop
// seam. pfnT01ToQuat is a template parameter so builders may pass small stateless
// lambdas (with value captures) -- signature Zenith_Maths::Quat(float fT01).
// Builds the channel, adds each (tick, quat), sorts, then moves it into xClip.
// Channels are inserted in a fixed order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
template <typename TFn>
void ZM_AnimAddRotCurve(Flux_AnimationClip& xClip, const char* szBone,
	float fDurationTicks, u_int uKeys, TFn&& pfnT01ToQuat)
{
	Zenith_Assert(uKeys >= 2u, "ZM_AnimAddRotCurve: need >= 2 keys for bone '%s'", szBone);

	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uKeys; ++u)
	{
		const float fT01 = static_cast<float>(u) / static_cast<float>(uKeys - 1u);
		xChannel.AddRotationKeyframe(fT01 * fDurationTicks, pfnT01ToQuat(fT01));
	}
	xChannel.SortKeyframes();
	xClip.AddBoneChannel(szBone, std::move(xChannel));
}

// Author an explicit key-posed rotation channel (anticipation -> strike ->
// recovery for the one-shot clips). Builds the channel, adds each (tick, quat),
// sorts, then moves it into xClip.
void ZM_AnimAddRotKeys(Flux_AnimationClip& xClip, const char* szBone,
	const ZM_AnimRotKey* pxKeys, u_int uCount);
