#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Serpent -- the S4 SC3 SERPENT clip builder: it authors
// the 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint) against the
// SERPENT skeleton ZM_CreatureArchetype_Serpent emits. Mirrors the QUADRUPED /
// AVIAN reference builders exactly -- rotation-only, fixed channel-insert order,
// key0==keyN looping (via ZM_AnimAddRotCurve with fn(0)==fn(1)), one-shot clips
// ending on identity except Faint (which holds the collapsed pose).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Serpent.cpp):
// a SIX-node root spine chain Spine00..Spine05 (Spine00 is the single root =
// coil-base at low Y, Spine05 = neck at high Y), Head, a 3-node tail
// Tail00..Tail02, and two brow horns HornL / HornR -- 12 bones, IDENTICAL across
// every evolution stage and every serpent species, which is why a clip authored
// against these NAMES transfers to all of them. There are NO limbs. A channel
// references ONLY these exact names.
//
// GEOMETRY (from the archetype builder): the serpent is posed as an UPRIGHT /
// rearing column of rings swept along WORLD Y -- coil-base (tail end) at low Y,
// neck at high Y; "front" is +Z; the head sits above the neck with its snout
// forward (+Z); the tail sweeps DOWN (-Y) from the coil-base to a fine point; the
// two brow horns sit on the head. As in the quadruped/avian kit +RotX pitches a
// bone forward/down, -RotX pitches it back/up, and -- the load-bearing choice for a
// vertical column -- RotZ tilts a vertebra side-to-side in X, so RotZ is the
// LATERAL undulation axis (the classic snake S-curve); RotY yaws the head/look.
//
// TRAVELLING WAVE (the serpent signature): Idle and Walk are one lateral (RotZ)
// sine wave travelling head->tail, authored per vertebra with a fixed PHASE LAG
// baked into each channel's lambda (no new kit primitive -- just phase in the
// closed form). Every curve is a pure sin(2*pi*f*t + phase) so fn(0)==fn(1) and
// the loop never pops.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Lateral slither,
// strike, and collapse are all ROTATION -- never root translation. One-shot
// Attack / Special / Hit end on ~bind (identity) so the clip-end clamp holds
// neutral; Faint is MONOTONIC and its LAST key IS the settled downed pose (it does
// NOT return to bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Serpent
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf

namespace
{
	// ---- The EXACT frozen serpent bone-name strings (emit order). ----
	const char* const szZM_SERP_SPINE[6] = { "Spine00", "Spine01", "Spine02", "Spine03", "Spine04", "Spine05" };
	const char* const szZM_SERP_HEAD     = "Head";
	const char* const szZM_SERP_TAIL[3]  = { "Tail00", "Tail01", "Tail02" };
	const char* const szZM_SERP_HORN[2]  = { "HornL", "HornR" };   // brow-horn pair (Special threat display)

	constexpr float fZM_SERP_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic). Walk
	// runs at a higher temporal frequency so it is sampled more densely.
	constexpr u_int uZM_SERP_IDLE_KEYS = 17u;
	constexpr u_int uZM_SERP_WALK_KEYS = 21u;

	// Per-vertebra spatial phase lag for the travelling wave (radians); the wave
	// propagates head(neck)->tail (and continues into the tail chain).
	constexpr float fZM_SERP_PHASE_STEP = 0.70f;

	inline Zenith_Maths::Quat ZM_SerpId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// The shared travelling-undulation authoring used by BOTH Idle and Walk: a
	// lateral (RotZ) sine wave down the spine (amplitude growing toward the free
	// neck end) whose per-vertebra phase lag makes it travel head->tail, continuing
	// into the tail and a gentle head sway. Spine00 (the planted coil-base root) is
	// keyed ONLY when bKeyRoot (the crawl drives the base; idle keeps it planted).
	// fFreq is the temporal cycle count over the loop (integer -> fn(0)==fn(1)).
	// -------------------------------------------------------------------------
	void ZM_SerpBuildUndulation(Flux_AnimationClip& xOut, u_int uKeys, float fFreq,
		float fSpineAmpBase, float fSpineAmpStep, float fTailAmpBase, float fTailAmpStep,
		float fHeadAmp, bool bKeyRoot)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());

		// Spine vertebrae -- lateral RotZ sine, phase-lagged head->tail. neck (highest
		// index) leads with zero lag; amplitude grows toward that free neck end.
		const u_int uStart = bKeyRoot ? 0u : 1u;
		for (u_int i = uStart; i < 6u; ++i)
		{
			const float fAmp   = fSpineAmpBase + fSpineAmpStep * static_cast<float>(i);
			const float fPhase = fZM_SERP_PHASE_STEP * static_cast<float>(5u - i);
			ZM_AnimAddRotCurve(xOut, szZM_SERP_SPINE[i], fTicks, uKeys, [fAmp, fPhase, fFreq](float fT)
			{
				return ZM_AnimRotZ(fAmp * sinf(fT * fZM_SERP_TWO_PI * fFreq + fPhase));
			});
		}

		// Head: gentle lateral sway continuing the wave (near-zero lag) + a slow look drift.
		ZM_AnimAddRotCurve(xOut, szZM_SERP_HEAD, fTicks, uKeys, [fHeadAmp, fFreq](float fT)
		{
			const float fP = fT * fZM_SERP_TWO_PI * fFreq;
			return ZM_AnimRotCompose(ZM_AnimRotZ(fHeadAmp * sinf(fP)), ZM_AnimRotY(0.5f * fHeadAmp * sinf(fP + 0.5f)));
		});

		// Tail: the wave continues past the coil-base, growing amplitude toward the tip.
		for (u_int t = 0; t < 3u; ++t)
		{
			const float fAmp   = fTailAmpBase + fTailAmpStep * static_cast<float>(t);
			const float fPhase = fZM_SERP_PHASE_STEP * static_cast<float>(6u + t);
			ZM_AnimAddRotCurve(xOut, szZM_SERP_TAIL[t], fTicks, uKeys, [fAmp, fPhase, fFreq](float fT)
			{
				return ZM_AnimRotZ(fAmp * sinf(fT * fZM_SERP_TWO_PI * fFreq + fPhase));
			});
		}
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): slow low-amplitude lateral undulation; the planted
	// coil-base (Spine00) stays put, the raised column and tail sway. key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildIdle(Flux_AnimationClip& xOut)
	{
		ZM_SerpBuildUndulation(xOut, uZM_SERP_IDLE_KEYS, 1.0f, 2.0f, 0.7f, 4.0f, 1.5f, 3.0f, false);
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): the SAME travelling undulation as Idle, but higher
	// amplitude AND double the temporal frequency (a serpentine crawl), and the
	// coil-base root (Spine00) now drives too. key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildWalk(Flux_AnimationClip& xOut)
	{
		ZM_SerpBuildUndulation(xOut, uZM_SERP_WALK_KEYS, 2.0f, 5.0f, 1.4f, 9.0f, 2.5f, 7.0f, true);
	}

	// One-shot RotX channel: rear back on -axis (anticipation) -> strike forward/down
	// on +axis -> follow-through -> settle to identity. Front-spine + head strike.
	void ZM_SerpAddStrikeX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fAnticip, float fStrike, float fFollow)
	{
		const Zenith_Maths::Quat xId = ZM_SerpId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.22f, fD), ZM_AnimRotX(fAnticip) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(fStrike)  },
			{ ZM_AnimTicksForT01(0.74f, fD), ZM_AnimRotX(fFollow)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// One-shot RotX channel: rear far back on -axis, HOLD the charge, then lunge
	// forward on +axis and settle to identity. Longer wind-up than a strike.
	void ZM_SerpAddChargeX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fRear, float fHold, float fLunge)
	{
		const Zenith_Maths::Quat xId = ZM_SerpId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(fRear)  },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(fHold)  },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(fLunge) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// One-shot LATERAL RotZ recoil: sharp jolt to one side, overshoot back, settle.
	void ZM_SerpAddRecoilZ(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fJolt, float fOver)
	{
		const Zenith_Maths::Quat xId = ZM_SerpId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotZ(fJolt) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(fOver) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 4u);
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a head/neck strike -- rear the FRONT of the spine
	// (Spine03..Spine05) + Head back, then whip them forward/down; magnitude grows
	// toward the head. No horns (that is the Special display), no tail, no lower
	// spine. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		ZM_SerpAddStrikeX(xOut, szZM_SERP_SPINE[3], fD,  -6.0f, 14.0f,  5.0f);
		ZM_SerpAddStrikeX(xOut, szZM_SERP_SPINE[4], fD,  -9.0f, 20.0f,  7.0f);
		ZM_SerpAddStrikeX(xOut, szZM_SERP_SPINE[5], fD, -12.0f, 26.0f,  9.0f);
		ZM_SerpAddStrikeX(xOut, szZM_SERP_HEAD,     fD, -14.0f, 32.0f, 11.0f);
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT threat tell -- coil/rear the WHOLE front
	// of the body (Spine02..Spine05 + Head) far back with a charged HOLD, the brow
	// horns FLARE wide (opposite RotZ = both splay out), then it lunges. More wind-up
	// AND more of the body than Attack, plus the horn flare (which Attack never
	// keys). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_SerpId();

		ZM_SerpAddChargeX(xOut, szZM_SERP_SPINE[2], fD, -16.0f, -18.0f, 12.0f);
		ZM_SerpAddChargeX(xOut, szZM_SERP_SPINE[3], fD, -20.0f, -22.0f, 14.0f);
		ZM_SerpAddChargeX(xOut, szZM_SERP_SPINE[4], fD, -24.0f, -26.0f, 16.0f);
		ZM_SerpAddChargeX(xOut, szZM_SERP_SPINE[5], fD, -28.0f, -30.0f, 18.0f);
		ZM_SerpAddChargeX(xOut, szZM_SERP_HEAD,     fD, -32.0f, -34.0f, 20.0f);

		// Brow horns flare outward as a threat display (opposite RotZ so both splay
		// out), then settle. This horn-flare is the Special signature.
		const ZM_AnimRotKey axHornL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(28.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(32.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(10.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_SERP_HORN[0], axHornL, 5u);
		const ZM_AnimRotKey axHornR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(-28.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(-32.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(-10.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_SERP_HORN[1], axHornR, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): a sharp LATERAL recoil of the front spine
	// (Spine02..Spine05) + a head snap, then settle. Lateral (RotZ) so it clearly
	// differs from the forward (RotX) Attack/Special; no horns, no tail. LAST key
	// returns to bind.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_SerpId();

		ZM_SerpAddRecoilZ(xOut, szZM_SERP_SPINE[2], fD, -12.0f, 5.0f);
		ZM_SerpAddRecoilZ(xOut, szZM_SERP_SPINE[3], fD, -14.0f, 6.0f);
		ZM_SerpAddRecoilZ(xOut, szZM_SERP_SPINE[4], fD, -16.0f, 7.0f);
		ZM_SerpAddRecoilZ(xOut, szZM_SERP_SPINE[5], fD, -18.0f, 8.0f);

		// Head snap: lateral jolt composed with a small backward pitch, then settle.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotZ(-22.0f), ZM_AnimRotX(-8.0f)) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_SERP_HEAD, axHead, 4u);
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC collapse -- the rearing column goes limp,
	// every vertebra folding forward-down (+RotX) and leaning into a slack coil
	// (+RotZ), the head dropping, the tail sagging. The LAST keyframe IS the settled
	// downed pose (it does NOT return to bind), so the sampler clamps to it and the
	// KO pose holds. The brow horns are left out of the collapse. No root translation.
	// -------------------------------------------------------------------------
	void ZM_SerpBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_SerpId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		// Whole spine chain folds + leans, monotonically; fold grows toward the neck.
		for (u_int i = 0; i < 6u; ++i)
		{
			const float fFold = 12.0f + 5.0f * static_cast<float>(i);
			const float fLean = 6.0f + 2.0f * static_cast<float>(i);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotCompose(ZM_AnimRotX(0.35f * fFold), ZM_AnimRotZ(0.35f * fLean)) },
				{ fT2, ZM_AnimRotCompose(ZM_AnimRotX(0.70f * fFold), ZM_AnimRotZ(0.70f * fLean)) },
				{ fT3, ZM_AnimRotCompose(ZM_AnimRotX(fFold),         ZM_AnimRotZ(fLean))         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_SERP_SPINE[i], ax, 4u);
		}

		// Head drops.
		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(22.0f) }, { fT2, ZM_AnimRotX(44.0f) }, { fT3, ZM_AnimRotX(60.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_SERP_HEAD, axHead, 4u);

		// Tail goes slack, sagging further toward the tip.
		for (u_int t = 0; t < 3u; ++t)
		{
			const float fSag = 14.0f + 6.0f * static_cast<float>(t);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotX(0.35f * fSag) },
				{ fT2, ZM_AnimRotX(0.70f * fSag) },
				{ fT3, ZM_AnimRotX(fSag)         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_SERP_TAIL[t], ax, 4u);
		}
	}
}

// ---------------------------------------------------------------------------
// The SERPENT clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Serpent(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_SerpBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_SerpBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_SerpBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_SerpBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_SerpBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_SerpBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Serpent: bad clip %u", (u_int)eClip);
		break;
	}
}
