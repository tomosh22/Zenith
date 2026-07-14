#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_FloaterPlantoid -- the S4 SC5 FLOATER-PLANTOID clip
// builder (the LAST archetype -- with this wired the anim-builder dispatch becomes
// TOTAL): it authors the 6 archetype clips (Idle / Walk / Attack / Special / Hit /
// Faint) against the FLOATER-PLANTOID skeleton ZM_CreatureArchetype_FloaterPlantoid
// emits. Mirrors the SERPENT / AQUATIC reference builders exactly -- rotation-only,
// fixed channel-insert order, key0==keyN looping (via ZM_AnimAddRotCurve with
// fn(0)==fn(1)), one-shot clips ending on identity except Faint (which holds the
// wilted downed pose).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_FloaterPlantoid.cpp):
// a THREE-node root spine chain Spine00..Spine02 (Spine00 is the single root =
// bulb belly at low Y, Spine02 = bulb top / crown-mount at high Y) that IS the
// buoyant bulb, one crown "Head", and a symmetric RADIAL SKIRT of SIX tendrils
// Tendril0..Tendril5 -- 10 bones, IDENTICAL across every evolution stage and every
// floater species, which is why a clip authored against these NAMES transfers to
// all of them. There are NO legs (a floater drifts). A channel references ONLY
// these exact names.
//
// GEOMETRY (from the archetype builder): the bulb is a VERTICAL egg swept along
// WORLD Y (belly low, crown high) that FLOATS above the ground; "front" is +Z. The
// crown caps the top; SIX thin round tendrils hang from the bulb's underside at
// FIXED even angular steps (k * 360/6) around Y, so the skirt is radially symmetric.
// Each tendril bone is placed at its radial angle but with IDENTITY local rotation
// (in the BULB frame), so an axis-aligned RotX/Y/Z rotates every tendril the SAME
// way regardless of its compass placement. As in the serpent/aquatic kit +RotX
// pitches a bone forward/down and -RotX back/up (the bulb bob / crown thrust / droop
// axis), RotY yaws (a slow drift), RotZ is lateral. The load-bearing floater-local
// choice: a RADIAL tendril tilt about each tendril's own TANGENTIAL axis
// (-sin,0,cos at its fixed placement angle) swings its tip radially OUTWARD (+) or
// INWARD (-) -- so a per-tendril radial sine gives the jellyfish-bell PULSE (Idle /
// Walk), a positive radial hold gives the symmetric FLARE (Special), and a negative
// radial fold gives the INWARD flick / WILT (Hit / Faint). This radial axis is the
// SAME fixed constant the archetype builder used to place the skirt (never a random
// draw), so the motion stays radially symmetric and byte-identical across species.
//
// ATTACK vs SPECIAL (the required pairwise separation): Attack is a UNIFORM forward
// LASH -- every tendril whips toward +Z on the shared RotX axis (a directional
// strike) + a crown thrust; it keys ONLY Spine02 + Head + the tendrils. Special is
// a symmetric BLOOM -- every tendril flares radially OUTWARD about its OWN axis + the
// crown raises + the bulb (Spine01 + Spine02) expands. Spine01 is keyed by Special
// but NOT by Attack -- the structural lock separating the two -- and the tendril axis
// (uniform-forward vs per-tendril-radial) differs, so the content hashes diverge.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Bob, drift, lash,
// bloom, flick, and wilt are all ROTATION -- never root translation. One-shot Attack
// / Special / Hit end on ~bind (identity) so the clip-end clamp holds neutral; Faint
// is MONOTONIC and its LAST key IS the settled wilted pose (it does NOT return to
// bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order -- is
// byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_FloaterPlantoid
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf, cosf

namespace
{
	// ---- The EXACT frozen floater-plantoid bone-name strings (emit order). ----
	const char* const szZM_FLOAT_SPINE[3]   = { "Spine00", "Spine01", "Spine02" };   // the buoyant bulb
	const char* const szZM_FLOAT_HEAD       = "Head";                                 // crown cap
	const char* const szZM_FLOAT_TENDRIL[6] = { "Tendril0", "Tendril1", "Tendril2",   // radial skirt
	                                            "Tendril3", "Tendril4", "Tendril5" };

	// The FIXED radial appendage count (golden-fixed, mirrors the archetype builder's
	// uFLOATER_TENDRIL_COUNT).
	constexpr u_int uZM_FLOAT_TENDRIL_COUNT = 6u;

	constexpr float fZM_FLOAT_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic). The drift
	// runs at a higher temporal frequency so it is sampled more densely.
	constexpr u_int uZM_FLOAT_IDLE_KEYS  = 17u;
	constexpr u_int uZM_FLOAT_DRIFT_KEYS = 13u;

	// Per-tendril spatial phase lag (radians) for the pulse wave, so the skirt breathes
	// out of phase and the ripple travels around the ring.
	constexpr float fZM_FLOAT_TENDRIL_PHASE_STEP = 0.85f;

	inline Zenith_Maths::Quat ZM_FloatId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// The FIXED radial (tangential-axis) tilt for tendril uT: +fDeg swings the tip
	// radially OUTWARD (flare), -fDeg curls it INWARD (wilt / flick). The axis is
	// (-sin,0,cos) at the tendril's fixed placement angle uT*(2*pi/6) -- the SAME
	// deterministic constant the archetype builder used to place the skirt (never a
	// random draw), so the motion is radially symmetric. angleAxis yields a unit quat.
	// -------------------------------------------------------------------------
	Zenith_Maths::Quat ZM_FloatTendrilRadial(u_int uT, float fDeg)
	{
		const float fAngle = static_cast<float>(uT) * (fZM_FLOAT_TWO_PI / static_cast<float>(uZM_FLOAT_TENDRIL_COUNT));
		const Zenith_Maths::Vector3 xAxis(-sinf(fAngle), 0.0f, cosf(fAngle));
		return glm::angleAxis(glm::radians(fDeg), xAxis);
	}

	inline float ZM_FloatTendrilPhase(u_int uT)
	{
		return fZM_FLOAT_TENDRIL_PHASE_STEP * static_cast<float>(uT);
	}

	// -------------------------------------------------------------------------
	// The shared drift authoring used by BOTH Idle and Walk: a vertical BOB of the
	// bulb (RotX flexion growing toward the crown, with a soft travelling phase up the
	// spine) + a crown bob-and-yaw + a per-tendril RADIAL pulse (the skirt breathing
	// in/out about each tendril's own radial axis, phase-lagged around the ring so it
	// ripples like a jellyfish bell). Spine00 (the bulb base) is keyed ONLY when
	// bKeyBase (the stronger drift drives the base; idle keeps it steady). fFreq is the
	// temporal cycle count over the loop (integer -> fn(0)==fn(1)).
	// -------------------------------------------------------------------------
	void ZM_FloatBuildDrift(Flux_AnimationClip& xOut, u_int uKeys, float fFreq,
		float fBobBase, float fBobStep, float fHeadBob, float fTendrilAmp, bool bKeyBase)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());

		// Bulb bob: RotX flexion, amplitude growing toward the crown, phase up the spine.
		const u_int uStart = bKeyBase ? 0u : 1u;
		for (u_int i = uStart; i < 3u; ++i)
		{
			const float fAmp   = fBobBase + fBobStep * static_cast<float>(i);
			const float fPhase = 0.6f * static_cast<float>(i);
			ZM_AnimAddRotCurve(xOut, szZM_FLOAT_SPINE[i], fTicks, uKeys, [fAmp, fPhase, fFreq](float fT)
			{
				return ZM_AnimRotX(fAmp * sinf(fT * fZM_FLOAT_TWO_PI * fFreq + fPhase));
			});
		}

		// Crown gently bobs (RotX) + a slow yaw drift (RotY).
		ZM_AnimAddRotCurve(xOut, szZM_FLOAT_HEAD, fTicks, uKeys, [fHeadBob, fFreq](float fT)
		{
			const float fP = fT * fZM_FLOAT_TWO_PI * fFreq;
			return ZM_AnimRotCompose(ZM_AnimRotX(fHeadBob * sinf(fP)), ZM_AnimRotY(0.4f * fHeadBob * sinf(fP + 0.5f)));
		});

		// Radial tendril skirt: each tendril pulses in/out about its OWN radial axis,
		// phase-lagged around the ring so the whole skirt ripples.
		for (u_int t = 0; t < uZM_FLOAT_TENDRIL_COUNT; ++t)
		{
			const float fPhase = ZM_FloatTendrilPhase(t);
			ZM_AnimAddRotCurve(xOut, szZM_FLOAT_TENDRIL[t], fTicks, uKeys, [t, fTendrilAmp, fPhase, fFreq](float fT)
			{
				return ZM_FloatTendrilRadial(t, fTendrilAmp * sinf(fT * fZM_FLOAT_TWO_PI * fFreq + fPhase));
			});
		}
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): a gentle hover -- a slow low-amplitude bulb bob (the base
	// stays steady) + crown bob + a soft radial skirt pulse. key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildIdle(Flux_AnimationClip& xOut)
	{
		ZM_FloatBuildDrift(xOut, uZM_FLOAT_IDLE_KEYS, 1.0f, 3.0f, 1.5f, 2.5f, 10.0f, false);
	}

	// -------------------------------------------------------------------------
	// WALK == DRIFT (1.0s, loop): the SAME bob + skirt pulse but the bulb base drives
	// too, at DOUBLE the temporal frequency and higher amplitude -- the tendrils PADDLE
	// propulsively (a stronger radial pulse). key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildWalk(Flux_AnimationClip& xOut)
	{
		ZM_FloatBuildDrift(xOut, uZM_FLOAT_DRIFT_KEYS, 2.0f, 5.0f, 2.5f, 4.0f, 18.0f, true);
	}

	// One-shot RotX strike: rear back on -axis (anticipation) -> whip forward/down on
	// +axis -> follow-through -> settle to identity.
	void ZM_FloatAddStrikeX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fAnticip, float fStrike, float fFollow)
	{
		const Zenith_Maths::Quat xId = ZM_FloatId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.22f, fD), ZM_AnimRotX(fAnticip) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(fStrike)  },
			{ ZM_AnimTicksForT01(0.74f, fD), ZM_AnimRotX(fFollow)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a forward LASH -- the crown (Head) thrusts and the bulb
	// TOP (Spine02) leans forward, while EVERY tendril whips forward together on the
	// shared RotX axis (a directional strike, NOT the radial bloom). Keys Spine02 +
	// Head + all tendrils; it does NOT key Spine00 or Spine01. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_FloatId();

		ZM_FloatAddStrikeX(xOut, szZM_FLOAT_SPINE[2], fD,  -8.0f, 18.0f, 6.0f);
		ZM_FloatAddStrikeX(xOut, szZM_FLOAT_HEAD,     fD, -12.0f, 26.0f, 9.0f);

		// Tendrils LASH forward together (uniform RotX whip), settle to identity.
		for (u_int t = 0; t < uZM_FLOAT_TENDRIL_COUNT; ++t)
		{
			const ZM_AnimRotKey ax[] = {
				{ ZM_AnimTicksForT01(0.00f, fD), xId },
				{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-10.0f) },
				{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotX(34.0f)  },
				{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(10.0f)  },
				{ ZM_AnimTicksForT01(1.00f, fD), xId },
			};
			ZM_AnimAddRotKeys(xOut, szZM_FLOAT_TENDRIL[t], ax, 5u);
		}
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT bloom/display -- the crown RAISES (-RotX)
	// and the bulb (Spine01 + Spine02) EXPANDS while EVERY tendril FLARES radially
	// OUTWARD about its own axis (a symmetric bloom, unlike the Attack's uniform
	// forward lash), then it settles. Spine01 is keyed here but NOT by Attack -- the
	// structural lock separating Special from Attack. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_FloatId();

		// Bulb expands/raises (crown-ward nodes lift = -RotX), charged hold, settle.
		const ZM_AnimRotKey axSpine1[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-12.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(-4.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_SPINE[1], axSpine1, 5u);
		const ZM_AnimRotKey axSpine2[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-14.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(-6.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_SPINE[2], axSpine2, 5u);

		// Crown raises up as it blooms.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-22.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-26.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(-8.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_HEAD, axHead, 5u);

		// Tendrils FLARE wide -- each swings radially OUTWARD about its OWN axis (the
		// bloom signature), hold, settle. This radial axis is what separates the Special
		// bloom from the Attack's uniform forward lash.
		for (u_int t = 0; t < uZM_FLOAT_TENDRIL_COUNT; ++t)
		{
			const ZM_AnimRotKey ax[] = {
				{ ZM_AnimTicksForT01(0.00f, fD), xId },
				{ ZM_AnimTicksForT01(0.35f, fD), ZM_FloatTendrilRadial(t, 40.0f) },
				{ ZM_AnimTicksForT01(0.55f, fD), ZM_FloatTendrilRadial(t, 46.0f) },
				{ ZM_AnimTicksForT01(0.80f, fD), ZM_FloatTendrilRadial(t, 16.0f) },
				{ ZM_AnimTicksForT01(1.00f, fD), xId },
			};
			ZM_AnimAddRotKeys(xOut, szZM_FLOAT_TENDRIL[t], ax, 5u);
		}
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): a sharp recoil -- the bulb top (Spine02) + crown (Head)
	// snap back/up (-RotX, the head with a small lateral jolt), and every tendril
	// FLICKS radially INWARD (negative radial = opposite of the Special flare), then
	// settle. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_FloatId();

		const ZM_AnimRotKey axSpine2[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_SPINE[2], axSpine2, 4u);

		// Crown snap: backward recoil composed with a small lateral jolt, then settle.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-20.0f), ZM_AnimRotZ(8.0f)) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_HEAD, axHead, 4u);

		// Tendrils flick INWARD (negative radial), then settle.
		for (u_int t = 0; t < uZM_FLOAT_TENDRIL_COUNT; ++t)
		{
			const ZM_AnimRotKey ax[] = {
				{ ZM_AnimTicksForT01(0.00f, fD), xId },
				{ ZM_AnimTicksForT01(0.28f, fD), ZM_FloatTendrilRadial(t, -24.0f) },
				{ ZM_AnimTicksForT01(0.60f, fD), ZM_FloatTendrilRadial(t, 6.0f)   },
				{ ZM_AnimTicksForT01(1.00f, fD), xId },
			};
			ZM_AnimAddRotKeys(xOut, szZM_FLOAT_TENDRIL[t], ax, 4u);
		}
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC collapse -- the buoyant bulb loses its lift,
	// the whole spine chain sagging forward-down (+RotX, fold growing toward the
	// crown), the crown drooping, and every tendril WILTING slack -- curling radially
	// INWARD (negative radial) as the skirt goes limp. The LAST keyframe IS the settled
	// wilted pose (it does NOT return to bind), so the sampler clamps to it and the KO
	// pose holds. No root translation.
	// -------------------------------------------------------------------------
	void ZM_FloatBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_FloatId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		// The whole bulb sags forward-down (+RotX), monotonically; fold grows crown-ward.
		for (u_int i = 0; i < 3u; ++i)
		{
			const float fFold = 14.0f + 6.0f * static_cast<float>(i);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotX(0.35f * fFold) },
				{ fT2, ZM_AnimRotX(0.70f * fFold) },
				{ fT3, ZM_AnimRotX(fFold)         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_FLOAT_SPINE[i], ax, 4u);
		}

		// Crown droops.
		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(20.0f) }, { fT2, ZM_AnimRotX(42.0f) }, { fT3, ZM_AnimRotX(58.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_FLOAT_HEAD, axHead, 4u);

		// Every tendril wilts slack -- curling INWARD (negative radial) monotonically,
		// with a small per-tendril spread so the collapse is not perfectly rigid.
		for (u_int t = 0; t < uZM_FLOAT_TENDRIL_COUNT; ++t)
		{
			const float fWilt = 30.0f + 4.0f * static_cast<float>(t);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_FloatTendrilRadial(t, -0.35f * fWilt) },
				{ fT2, ZM_FloatTendrilRadial(t, -0.70f * fWilt) },
				{ fT3, ZM_FloatTendrilRadial(t, -fWilt)         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_FLOAT_TENDRIL[t], ax, 4u);
		}
	}
}

// ---------------------------------------------------------------------------
// The FLOATER-PLANTOID clip builder. Signature matches the frozen
// ZM_CreatureAnimGen.h declaration exactly. The driver has already set the clip's
// golden metadata (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_FloaterPlantoid(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_FloatBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_FloatBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_FloatBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_FloatBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_FloatBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_FloatBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_FloaterPlantoid: bad clip %u", (u_int)eClip);
		break;
	}
}
