#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Quadruped -- the SC1 reference clip builder: it
// authors the 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint)
// against the QUADRUPED skeleton ZM_CreatureArchetype_Quadruped emits. This is
// the sole wired anim builder in SC1 (ZM_GetArchetypeAnimBuilder returns it only
// for ZM_ARCHETYPE_QUADRUPED).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Quadruped.cpp
// + the ZM_CreatureArchetypeCommon kit that names the bones): a 4-node spine
// chain Spine00..Spine03 (Spine00 is the single root), Head, eight leg bones
// LegFLUp/Lo LegFRUp/Lo LegHLUp/Lo LegHRUp/Lo, a 3-node tail Tail00..Tail02, and
// two horn bones HornL / HornR -- 18 bones, IDENTICAL across every evolution stage
// and every quadruped species, which is why a clip authored against these NAMES
// transfers to all of them. A channel references ONLY these exact names.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Vertical motion
// (idle sway, faint collapse) is spine-flexion ROTATION, never root translation.
// Looping clips (Idle / Walk) author full-cycle curves so key0 == keyN per channel
// (no loop pop). The one-shot Attack / Special / Hit end on ~bind (identity) so the
// clip-end clamp holds neutral; Faint is MONOTONIC and its LAST key IS the settled
// downed pose (it does NOT return to bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Quadruped
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf, cosf

namespace
{
	// ---- The EXACT frozen quadruped bone-name strings (emit order). ----
	const char* const szZM_QUAD_SPINE[4]  = { "Spine00", "Spine01", "Spine02", "Spine03" };
	const char* const szZM_QUAD_HEAD      = "Head";
	const char* const szZM_QUAD_LEG_UP[4] = { "LegFLUp", "LegFRUp", "LegHLUp", "LegHRUp" };   // FL, FR, HL, HR
	const char* const szZM_QUAD_LEG_LO[4] = { "LegFLLo", "LegFRLo", "LegHLLo", "LegHRLo" };
	const char* const szZM_QUAD_TAIL[3]   = { "Tail00", "Tail01", "Tail02" };
	const char* const szZM_QUAD_HORN[2]   = { "HornL", "HornR" };

	constexpr float fZM_QUAD_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_QUAD_IDLE_KEYS = 17u;
	constexpr u_int uZM_QUAD_WALK_KEYS = 13u;

	// Walk gait shaping (namespace scope so the per-leg curve lambdas read them
	// without capture ambiguity).
	constexpr float fZM_QUAD_WALK_SWING    = 14.0f;   // hip fore-aft swing (deg)
	constexpr float fZM_QUAD_WALK_KNEE_MID = -6.0f;   // knee flex bias (deg)
	constexpr float fZM_QUAD_WALK_KNEE_AMP = -9.0f;   // knee flex amplitude (deg)

	inline Zenith_Maths::Quat ZM_QuadId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): gentle full-cycle counter-phase spine sway + slow head
	// drift + tail micro-sway + leg-'Up' micro-flex. Every curve is a pure
	// sin(2*pi*t + phase) (+ constants) so fn(0) == fn(1) and the loop never pops.
	// -------------------------------------------------------------------------
	void ZM_QuadBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_QUAD_IDLE_KEYS;

		// Spine sway (skip the planted root Spine00).
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_SPINE[1], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_QUAD_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotZ(2.5f * sinf(fP)), ZM_AnimRotX(1.5f * sinf(fP + 0.4f)));
		});
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_SPINE[2], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(-2.0f * sinf(fT * fZM_QUAD_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_SPINE[3], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(1.2f * sinf(fT * fZM_QUAD_TWO_PI + 0.8f));
		});

		// Head: slow attentive drift.
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_QUAD_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(3.0f * sinf(fP)), ZM_AnimRotX(-2.0f + 1.5f * sinf(fP + 0.6f)));
		});

		// Tail: micro-sway, growing toward the tip.
		for (u_int t = 0; t < 3u; ++t)
		{
			const float fAmp   = 3.0f + 1.0f * static_cast<float>(t);
			const float fPhase = 0.2f * static_cast<float>(t + 1u);
			ZM_AnimAddRotCurve(xOut, szZM_QUAD_TAIL[t], fTicks, uKeys, [fAmp, fPhase](float fT)
			{
				return ZM_AnimRotY(fAmp * sinf(fT * fZM_QUAD_TWO_PI + fPhase));
			});
		}

		// Legs: barely-there hip micro-flex, phase-offset per leg.
		for (u_int l = 0; l < 4u; ++l)
		{
			const float fPhase = 1.5f * static_cast<float>(l);
			ZM_AnimAddRotCurve(xOut, szZM_QUAD_LEG_UP[l], fTicks, uKeys, [fPhase](float fT)
			{
				return ZM_AnimRotX(1.0f * sinf(fT * fZM_QUAD_TWO_PI + fPhase));
			});
		}
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): diagonal gait -- diagonal leg pairs (FL,HR) in phase and
	// (FR,HL) counter-phase; knees follow with a phase lag; spine counter-yaw; tail
	// wag; head bob. All curves close (fn(0) == fn(1)).
	// -------------------------------------------------------------------------
	void ZM_QuadBuildWalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_QUAD_WALK_KEYS;

		// FL, FR, HL, HR -> diagonal phase (fraction of a cycle).
		const float afLegPhase[4] = { 0.0f, 0.5f, 0.5f, 0.0f };
		const float fKneeLag = 0.15f * fZM_QUAD_TWO_PI;

		for (u_int l = 0; l < 4u; ++l)
		{
			const float fPh = afLegPhase[l] * fZM_QUAD_TWO_PI;
			ZM_AnimAddRotCurve(xOut, szZM_QUAD_LEG_UP[l], fTicks, uKeys, [fPh](float fT)
			{
				return ZM_AnimRotX(fZM_QUAD_WALK_SWING * sinf(fT * fZM_QUAD_TWO_PI + fPh));
			});
			ZM_AnimAddRotCurve(xOut, szZM_QUAD_LEG_LO[l], fTicks, uKeys, [fPh, fKneeLag](float fT)
			{
				return ZM_AnimRotX(fZM_QUAD_WALK_KNEE_MID + fZM_QUAD_WALK_KNEE_AMP * sinf(fT * fZM_QUAD_TWO_PI + fPh + fKneeLag));
			});
		}

		// Spine counter-yaw (root + chest oppose each other).
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_SPINE[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(-3.0f * sinf(fT * fZM_QUAD_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_SPINE[3], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(3.0f * sinf(fT * fZM_QUAD_TWO_PI));
		});

		// Head bob (twice per stride) + slight sway.
		ZM_AnimAddRotCurve(xOut, szZM_QUAD_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_QUAD_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(2.0f * sinf(fP)), ZM_AnimRotX(-2.5f * sinf(2.0f * fP)));
		});

		// Tail wag, growing toward the tip.
		for (u_int t = 0; t < 3u; ++t)
		{
			const float fAmp   = 6.0f + 2.0f * static_cast<float>(t);
			const float fPhase = 0.3f * static_cast<float>(t);
			ZM_AnimAddRotCurve(xOut, szZM_QUAD_TAIL[t], fTicks, uKeys, [fAmp, fPhase](float fT)
			{
				return ZM_AnimRotY(fAmp * sinf(fT * fZM_QUAD_TWO_PI + fPhase));
			});
		}
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): anticipation -> lunge -> recovery. Spine + head
	// thrust forward, horns lead the gore, forelegs reach; LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_QuadBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_QuadId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-8.0f)  },   // rear (anticipation)
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(16.0f)  },   // thrust forward/down
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(7.0f)   },   // follow-through
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[1], axSpine01, 5u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(20.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(9.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[3], axSpine03, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(24.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(11.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HEAD, axHead, 5u);

		const ZM_AnimRotKey axHorn[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-6.0f)  },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(16.0f)  },   // gore lead
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HORN[0], axHorn, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HORN[1], axHorn, 4u);

		const ZM_AnimRotKey axForeleg[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-14.0f) },   // reach
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(22.0f)  },   // swipe/plant
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[0], axForeleg, 5u);   // LegFLUp
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[1], axForeleg, 5u);   // LegFRUp
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT offensive tell -- rear back (spine pitch
	// UP + head raise), a brief charged HOLD, then release forward. More wind-up
	// than Attack; LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_QuadBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_QuadId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-20.0f) },   // rear up
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-22.0f) },   // charged hold
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(15.0f)  },   // release forward
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[1], axSpine01, 5u);

		const ZM_AnimRotKey axSpine02[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-18.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-20.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(13.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[2], axSpine02, 5u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-24.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-26.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(17.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[3], axSpine03, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-28.0f) },   // head raise
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-30.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(18.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HEAD, axHead, 5u);

		const ZM_AnimRotKey axHindleg[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(12.0f)  },   // crouch/charge
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(14.0f)  },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(-6.0f)  },   // drive
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[2], axHindleg, 5u);   // LegHLUp
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[3], axHindleg, 5u);   // LegHRUp
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): sharp spine recoil BACKWARD (opposite the attack axis)
	// + head snap + a brief twitch flicking tail/leg outward, then settle; LAST key
	// returns to bind.
	// -------------------------------------------------------------------------
	void ZM_QuadBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_QuadId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-14.0f) },   // recoil back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },   // overshoot settle
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(7.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[3], axSpine03, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-20.0f), ZM_AnimRotZ(8.0f)) },   // snap
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(5.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HEAD, axHead, 4u);

		const ZM_AnimRotKey axTailTwitch[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotZ(18.0f) },    // flick outward
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(-6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_TAIL[0], axTailTwitch, 4u);

		const ZM_AnimRotKey axLegTwitch[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotX(-12.0f) },   // kick out
			{ ZM_AnimTicksForT01(0.65f, fD), ZM_AnimRotX(4.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[2], axLegTwitch, 4u);   // LegHLUp
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC fold to a collapsed pose -- spine pitches
	// forward-down, head drops, legs buckle under, tail slack. The LAST keyframe IS
	// the fully-settled downed pose (it does NOT return to bind), so the sampler
	// clamps to it and the KO pose holds. No root translation.
	// -------------------------------------------------------------------------
	void ZM_QuadBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_QuadId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		const ZM_AnimRotKey axSpine01[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(15.0f) }, { fT2, ZM_AnimRotX(30.0f) }, { fT3, ZM_AnimRotX(42.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine02[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(16.0f) }, { fT2, ZM_AnimRotX(32.0f) }, { fT3, ZM_AnimRotX(45.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[2], axSpine02, 4u);

		const ZM_AnimRotKey axSpine03[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(18.0f) }, { fT2, ZM_AnimRotX(34.0f) }, { fT3, ZM_AnimRotX(48.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_SPINE[3], axSpine03, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(25.0f) }, { fT2, ZM_AnimRotX(45.0f) }, { fT3, ZM_AnimRotX(62.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_QUAD_HEAD, axHead, 4u);

		// Legs buckle under: '...Up' fold forward, '...Lo' knees fold back. Identical
		// per-leg values (reused arrays), appended in the fixed FL, FR, HL, HR order.
		const ZM_AnimRotKey axLegUp[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(20.0f) }, { fT2, ZM_AnimRotX(45.0f) }, { fT3, ZM_AnimRotX(70.0f) },
		};
		const ZM_AnimRotKey axLegLo[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(-25.0f) }, { fT2, ZM_AnimRotX(-55.0f) }, { fT3, ZM_AnimRotX(-80.0f) },
		};
		for (u_int l = 0; l < 4u; ++l)
		{
			ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_UP[l], axLegUp, 4u);
			ZM_AnimAddRotKeys(xOut, szZM_QUAD_LEG_LO[l], axLegLo, 4u);
		}

		// Tail slack.
		const ZM_AnimRotKey axTail[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(10.0f) }, { fT2, ZM_AnimRotX(22.0f) }, { fT3, ZM_AnimRotX(35.0f) },
		};
		for (u_int t = 0; t < 3u; ++t)
		{
			ZM_AnimAddRotKeys(xOut, szZM_QUAD_TAIL[t], axTail, 4u);
		}
	}
}

// ---------------------------------------------------------------------------
// The QUADRUPED clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Quadruped(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_QuadBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_QuadBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_QuadBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_QuadBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_QuadBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_QuadBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Quadruped: bad clip %u", (u_int)eClip);
		break;
	}
}
