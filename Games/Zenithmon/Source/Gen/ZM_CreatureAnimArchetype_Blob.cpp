#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Blob -- the S4 SC4 BLOB clip builder: it authors the 6
// archetype clips (Idle / Walk / Attack / Special / Hit / Faint) against the BLOB
// skeleton ZM_CreatureArchetype_Blob emits. Mirrors the QUADRUPED / SERPENT
// reference builders' structure exactly -- rotation-only, fixed channel-insert
// order, key0==keyN looping (via ZM_AnimAddRotCurve with fn(0)==fn(1)), one-shot
// clips ending on identity except Faint (which holds the collapsed pose). This is
// the SPARSEST archetype (~4 bones), the low-bone extreme of the roster.
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Blob.cpp + the
// ZM_CreatureArchetypeCommon kit that names the bones): a THREE-node body spine
// chain Spine00..Spine02 (Spine00 is the single root = belly at low Y, Spine02 =
// crown at high Y) plus ONE crown nub bone "Nub" (a tiny eye-stalk cone) -- 3 + 1 =
// 4 bones, and NOTHING else (a blob has NO head, NO legs, NO tail). The set is
// IDENTICAL across every evolution stage and every blob species, which is why a
// clip authored against these NAMES transfers to all of them. A channel references
// ONLY these four exact names.
//
// EXPRESSING MOTION WITH FOUR BONES: with no limbs, EVERY motion is body-spine
// flexion ROTATION plus a Nub rotation. The blob "walks" by a bouncing spine PULSE
// (a hop expressed as spine flexion, never translation), "attacks" by a forward
// spine lunge + Nub jab, does its "special" by coiling/rearing the whole body BACK
// then releasing, is "hit" by a lateral spine wobble, and "faints" by folding /
// deflating down. Because there are only four bones, the six clips are made
// PAIRWISE DISTINCT by varying which nodes are keyed (Idle/Attack/Hit leave the
// planted base Spine00 alone; Walk/Special/Faint drive it too), the rotation AXIS
// (forward RotX vs lateral RotZ vs breathing sway), the frequency, the amplitude,
// and the key-pose sequence -- so no two clips serialize identically (the generic
// harness asserts Idle!=Walk, Attack!=Special, Attack!=Hit, Hit!=Faint by hash).
//
// ROTATION-ONLY (v1): every clip authors rotation channels only -- never root
// translation. One-shot Attack / Special / Hit end on ~bind (identity) so the
// clip-end clamp holds neutral; Faint is MONOTONIC and its LAST key IS the settled
// downed pose (it does NOT return to bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Blob
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf, cosf

namespace
{
	// ---- The EXACT frozen blob bone-name strings (emit order). ----
	const char* const szZM_BLOB_SPINE[3] = { "Spine00", "Spine01", "Spine02" };   // belly -> crown
	const char* const szZM_BLOB_NUB      = "Nub";                                 // crown eye-stalk

	constexpr float fZM_BLOB_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_BLOB_IDLE_KEYS = 17u;
	constexpr u_int uZM_BLOB_WALK_KEYS = 13u;

	// Walk bounce shaping (namespace scope so the per-node curve lambdas read them
	// without capture ambiguity).
	constexpr float fZM_BLOB_WALK_FLEX_BASE = 6.0f;    // base-node compression amplitude (deg)
	constexpr float fZM_BLOB_WALK_FLEX_STEP = 5.0f;    // per-node growth toward the crown (deg)
	constexpr float fZM_BLOB_WALK_NUB_AMP   = 12.0f;   // crown-nub bob amplitude (deg)

	inline Zenith_Maths::Quat ZM_BlobId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): a gentle low-amplitude gelatinous wobble -- the mid + crown
	// nodes sway laterally (RotZ) and breathe (RotX) out of phase, and the crown nub
	// micro-bobs. The planted base (Spine00) stays put. Every curve is a pure
	// sin(2*pi*t + phase) so fn(0) == fn(1) and the loop never pops.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_BLOB_IDLE_KEYS;

		// Mid node: gentle lateral sway + slow breathe.
		ZM_AnimAddRotCurve(xOut, szZM_BLOB_SPINE[1], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BLOB_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotZ(2.5f * sinf(fP)), ZM_AnimRotX(1.5f * sinf(fP + 0.5f)));
		});
		// Crown node: a touch more sway, counter-phase to the mid so the dome jiggles.
		ZM_AnimAddRotCurve(xOut, szZM_BLOB_SPINE[2], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BLOB_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotZ(-3.5f * sinf(fP)), ZM_AnimRotX(2.0f * sinf(fP + 0.9f)));
		});
		// Crown nub: slow lazy sway.
		ZM_AnimAddRotCurve(xOut, szZM_BLOB_NUB, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BLOB_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(4.0f * sinf(fP)), ZM_AnimRotX(2.5f * sinf(fP + 0.3f)));
		});
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): a bouncing PULSE -- the whole body (the planted base too)
	// rhythmically COMPRESSES then extends (a hop expressed as spine RotX flexion,
	// NOT translation), amplitude growing toward the crown so the dome squashes and
	// stretches most; the nub bobs counter to the body. Two hops per cycle. A
	// raised-cosine pulse 0.5*(1 - cos) is >= 0 (a compress-and-release) and closes:
	// pulse(0) == pulse(1) == 0. UNLIKE Idle, the base (Spine00) is driven.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildWalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_BLOB_WALK_KEYS;

		for (u_int i = 0; i < 3u; ++i)
		{
			const float fAmp = fZM_BLOB_WALK_FLEX_BASE + fZM_BLOB_WALK_FLEX_STEP * static_cast<float>(i);
			ZM_AnimAddRotCurve(xOut, szZM_BLOB_SPINE[i], fTicks, uKeys, [fAmp](float fT)
			{
				// Raised-cosine compression pulse (two hops/cycle); pulse(0)==pulse(1)==0.
				const float fPulse = 0.5f - 0.5f * cosf(fT * fZM_BLOB_TWO_PI * 2.0f);
				return ZM_AnimRotX(fAmp * fPulse);
			});
		}

		// Nub bobs counter to the body hop (RotX, two per cycle).
		ZM_AnimAddRotCurve(xOut, szZM_BLOB_NUB, fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(-fZM_BLOB_WALK_NUB_AMP * sinf(fT * fZM_BLOB_TWO_PI * 2.0f));
		});
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a forward LUNGE -- the mid + crown spine thrust
	// forward (+RotX) and the Nub jabs forward; anticipation back -> lunge ->
	// follow-through -> settle to identity. The planted base (Spine00) is NOT
	// engaged (that whole-body coil is the Special). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BlobId();

		const ZM_AnimRotKey axSpine1[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-8.0f)  },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(20.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(9.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_SPINE[1], axSpine1, 5u);

		const ZM_AnimRotKey axSpine2[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-11.0f) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(26.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(12.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_SPINE[2], axSpine2, 5u);

		const ZM_AnimRotKey axNub[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(30.0f)  },   // Nub jabs forward
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(12.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_NUB, axNub, 5u);
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT wind-up -- the WHOLE body (the base too)
	// COILS/REARS BACK (-RotX) with a charged HOLD, then releases forward; the Nub
	// whips back then forward. More wind-up AND more of the body than Attack (it
	// engages the base Spine00, which Attack never does). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BlobId();

		// The whole body coils back (magnitude grows toward the crown), holds, releases.
		const float afRear[3]    = { -14.0f, -18.0f, -22.0f };
		const float afHold[3]    = { -16.0f, -20.0f, -24.0f };
		const float afRelease[3] = {  10.0f,  11.0f,  12.0f };
		for (u_int i = 0; i < 3u; ++i)
		{
			const ZM_AnimRotKey ax[] = {
				{ ZM_AnimTicksForT01(0.00f, fD), xId },
				{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(afRear[i])    },
				{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(afHold[i])    },
				{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(afRelease[i]) },
				{ ZM_AnimTicksForT01(1.00f, fD), xId },
			};
			ZM_AnimAddRotKeys(xOut, szZM_BLOB_SPINE[i], ax, 5u);
		}

		// Nub whips back with the coil then snaps forward, then settles.
		const ZM_AnimRotKey axNub[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-26.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-28.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(16.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_NUB, axNub, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): a sharp LATERAL wobble -- the mid + crown spine jolt to
	// one side (RotZ) and overshoot back, the Nub flicking with them, then settle.
	// Lateral (RotZ) clearly differs from the forward (RotX) Attack, and the base
	// (Spine00) stays planted. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BlobId();

		const float afJolt[2] = { -14.0f, -16.0f };   // Spine01, Spine02 side jolt
		const float afOver[2] = {   6.0f,   7.0f };
		for (u_int i = 0; i < 2u; ++i)
		{
			const ZM_AnimRotKey ax[] = {
				{ ZM_AnimTicksForT01(0.00f, fD), xId },
				{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotZ(afJolt[i]) },
				{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(afOver[i]) },
				{ ZM_AnimTicksForT01(1.00f, fD), xId },
			};
			ZM_AnimAddRotKeys(xOut, szZM_BLOB_SPINE[i + 1u], ax, 4u);
		}

		// Nub flicks laterally with the wobble, then settles.
		const ZM_AnimRotKey axNub[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotZ(-20.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_NUB, axNub, 4u);
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC deflate -- the whole body (the base too)
	// folds/flattens forward-down (+RotX), the crown nub droops. The LAST keyframe IS
	// the settled downed pose (it does NOT return to bind), so the sampler clamps to
	// it and the KO pose holds. No root translation.
	// -------------------------------------------------------------------------
	void ZM_BlobBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BlobId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		// Body flattens forward-down, monotonically; fold grows toward the crown.
		const float afFold[3] = { 20.0f, 30.0f, 42.0f };   // Spine00, Spine01, Spine02
		for (u_int i = 0; i < 3u; ++i)
		{
			const float fFold = afFold[i];
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotX(0.35f * fFold) },
				{ fT2, ZM_AnimRotX(0.70f * fFold) },
				{ fT3, ZM_AnimRotX(fFold)         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_BLOB_SPINE[i], ax, 4u);
		}

		// Nub droops over (monotonic forward-down).
		const ZM_AnimRotKey axNub[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(16.0f) }, { fT2, ZM_AnimRotX(32.0f) }, { fT3, ZM_AnimRotX(46.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BLOB_NUB, axNub, 4u);
	}
}

// ---------------------------------------------------------------------------
// The BLOB clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Blob(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_BlobBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_BlobBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_BlobBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_BlobBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_BlobBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_BlobBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Blob: bad clip %u", (u_int)eClip);
		break;
	}
}
