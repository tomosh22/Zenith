#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Insectoid -- the S4 SC4 INSECTOID clip builder: it
// authors the 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint)
// against the INSECTOID skeleton ZM_CreatureArchetype_Insectoid emits. Mirrors
// the QUADRUPED / SERPENT reference builders exactly -- rotation-only, fixed
// channel-insert order, key0==keyN looping (via ZM_AnimAddRotCurve with
// fn(0)==fn(1)), one-shot clips ending on identity except Faint (which holds the
// collapsed pose). This is the HIGHEST-limb-count archetype (~19 bones).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Insectoid.cpp
// + the ZM_CreatureArchetypeCommon kit that names the bones): a FOUR-node body
// spine chain Spine00..Spine03 (Spine00 is the single root = abdomen at low Y,
// Spine03 = thorax/head-end at high Y), Head, SIX legs each on an '...Up'/'...Lo'
// pair -- LegL0Up/Lo LegL1Up/Lo LegL2Up/Lo LegR0Up/Lo LegR1Up/Lo LegR2Up/Lo
// (L0/R0 = the FRONT row, L1/R1 = MID, L2/R2 = REAR) -- and two antennae
// AntennaL / AntennaR: 4 + 1 + 12 + 2 = 19 bones, IDENTICAL across every evolution
// stage and every insectoid species, which is why a clip authored against these
// NAMES transfers to all of them. A channel references ONLY these exact names.
//
// METACHRONAL TRIPOD (the insect signature): the six legs split into two
// alternating tripods driven 180deg out of phase -- tripod A = {front-left LegL0,
// rear-left LegL2, mid-right LegR1}, tripod B = {mid-left LegL1, front-right LegR0,
// rear-right LegR2}. Because the leg emit order is L0,L1,L2,R0,R1,R2, the per-leg
// phase is simply {0, .5, 0, .5, 0, .5} of a cycle -- each successive emitted leg
// swaps tripod, which is exactly the diagonal-tripod pattern. No new kit primitive
// is needed: the phase is baked into each per-leg ZM_AnimAddRotCurve lambda.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Body bob, gait,
// strike, and collapse are all ROTATION -- never root translation. One-shot Attack /
// Special / Hit end on ~bind (identity) so the clip-end clamp holds neutral; Faint
// is MONOTONIC and its LAST key IS the settled downed pose (it does NOT return to
// bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Insectoid
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf, cosf

namespace
{
	// ---- The EXACT frozen insectoid bone-name strings (emit order). ----
	const char* const szZM_INS_SPINE[4]  = { "Spine00", "Spine01", "Spine02", "Spine03" };
	const char* const szZM_INS_HEAD      = "Head";
	// Legs, in the archetype's fixed emit order L0,L1,L2,R0,R1,R2 (front/mid/rear
	// per side). '...Up' = the hip/upper segment, '...Lo' = the knee/lower segment.
	const char* const szZM_INS_LEG_UP[6] = { "LegL0Up", "LegL1Up", "LegL2Up", "LegR0Up", "LegR1Up", "LegR2Up" };
	const char* const szZM_INS_LEG_LO[6] = { "LegL0Lo", "LegL1Lo", "LegL2Lo", "LegR0Lo", "LegR1Lo", "LegR2Lo" };
	const char* const szZM_INS_ANT[2]    = { "AntennaL", "AntennaR" };

	// Metachronal-tripod per-leg phase (fraction of a cycle) in the fixed emit order
	// L0,L1,L2,R0,R1,R2: successive emitted legs alternate tripod, so this is just
	// {0,.5} repeated -> tripod A = {L0,L2,R1}, tripod B = {L1,R0,R2}, 180deg apart.
	const float afZM_INS_TRIPOD[6] = { 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f };

	// Front-row / rear-row leg indices (into szZM_INS_LEG_*), used by the action clips.
	const u_int auZM_INS_FRONT_LEG[2] = { 0u, 3u };   // LegL0, LegR0
	const u_int auZM_INS_REAR_LEG[2]  = { 2u, 5u };   // LegL2, LegR2

	constexpr float fZM_INS_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_INS_IDLE_KEYS = 17u;
	constexpr u_int uZM_INS_WALK_KEYS = 13u;

	// Walk gait shaping (namespace scope so the per-leg curve lambdas read them
	// without capture ambiguity).
	constexpr float fZM_INS_WALK_SWING    = 16.0f;   // hip fore-aft swing (deg)
	constexpr float fZM_INS_WALK_KNEE_MID = -8.0f;   // knee flex bias (deg)
	constexpr float fZM_INS_WALK_KNEE_AMP = -11.0f;  // knee flex amplitude (deg)

	inline Zenith_Maths::Quat ZM_InsId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// One-shot RotX channel: rear back on -axis (anticipation) -> strike forward/down
	// on +axis -> follow-through -> settle to identity.
	void ZM_InsAddStrikeX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fAnticip, float fStrike, float fFollow)
	{
		const Zenith_Maths::Quat xId = ZM_InsId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(fAnticip) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(fStrike)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(fFollow)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// One-shot RotX channel: rear far back on -axis, HOLD the charge, then release
	// forward on +axis and settle to identity. Longer wind-up than a strike.
	void ZM_InsAddChargeX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fRear, float fHold, float fRelease)
	{
		const Zenith_Maths::Quat xId = ZM_InsId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(fRear)    },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(fHold)    },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(fRelease) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): antennae sway (the signature idle motion) + slow head
	// drift + a subtle body bob (spine pitch) + a barely-there per-leg hip
	// micro-flex. Every curve is a pure sin(2*pi*t + phase) (+ constants) so
	// fn(0) == fn(1) and the loop never pops.
	// -------------------------------------------------------------------------
	void ZM_InsBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_INS_IDLE_KEYS;

		// Body: subtle pitch bob on the two upper segments (breathing).
		ZM_AnimAddRotCurve(xOut, szZM_INS_SPINE[2], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(1.5f * sinf(fT * fZM_INS_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_INS_SPINE[3], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_INS_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotX(1.2f * sinf(fP + 0.6f)), ZM_AnimRotZ(1.0f * sinf(fP)));
		});

		// Head: slow attentive drift.
		ZM_AnimAddRotCurve(xOut, szZM_INS_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_INS_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(3.0f * sinf(fP)), ZM_AnimRotX(-1.5f + 1.2f * sinf(fP + 0.5f)));
		});

		// Legs: barely-there hip micro-flex, phase-offset per leg (fixed emit order).
		for (u_int l = 0; l < 6u; ++l)
		{
			const float fPhase = 1.1f * static_cast<float>(l);
			ZM_AnimAddRotCurve(xOut, szZM_INS_LEG_UP[l], fTicks, uKeys, [fPhase](float fT)
			{
				return ZM_AnimRotX(1.5f * sinf(fT * fZM_INS_TWO_PI + fPhase));
			});
		}

		// Antennae: the signature idle sway -- gentle lateral RotZ + a slow forward
		// nod, the two antennae sweeping in opposite lateral directions.
		for (u_int a = 0; a < 2u; ++a)
		{
			const float fSide = (a == 0u) ? 1.0f : -1.0f;
			ZM_AnimAddRotCurve(xOut, szZM_INS_ANT[a], fTicks, uKeys, [fSide](float fT)
			{
				const float fP = fT * fZM_INS_TWO_PI;
				return ZM_AnimRotCompose(ZM_AnimRotZ(fSide * 7.0f * sinf(fP)), ZM_AnimRotX(3.0f * sinf(fP + 0.9f)));
			});
		}
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): the metachronal TRIPOD gait -- the six hips swing fore-aft
	// (RotX) split into two alternating tripods 180deg out of phase; the knees
	// follow with a phase lag; a slight body counter-yaw sways the thorax; a head
	// bob and antennae bob add life. All curves close (fn(0) == fn(1)).
	// -------------------------------------------------------------------------
	void ZM_InsBuildWalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_INS_WALK_KEYS;

		const float fKneeLag = 0.15f * fZM_INS_TWO_PI;

		for (u_int l = 0; l < 6u; ++l)
		{
			const float fPh = afZM_INS_TRIPOD[l] * fZM_INS_TWO_PI;
			// Hip: fore-aft swing at the tripod phase.
			ZM_AnimAddRotCurve(xOut, szZM_INS_LEG_UP[l], fTicks, uKeys, [fPh](float fT)
			{
				return ZM_AnimRotX(fZM_INS_WALK_SWING * sinf(fT * fZM_INS_TWO_PI + fPh));
			});
			// Knee: flex following the hip with a lag.
			ZM_AnimAddRotCurve(xOut, szZM_INS_LEG_LO[l], fTicks, uKeys, [fPh, fKneeLag](float fT)
			{
				return ZM_AnimRotX(fZM_INS_WALK_KNEE_MID + fZM_INS_WALK_KNEE_AMP * sinf(fT * fZM_INS_TWO_PI + fPh + fKneeLag));
			});
		}

		// Body sway: abdomen + thorax counter-yaw (RotY oppose each other).
		ZM_AnimAddRotCurve(xOut, szZM_INS_SPINE[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(-3.5f * sinf(fT * fZM_INS_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_INS_SPINE[3], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(3.5f * sinf(fT * fZM_INS_TWO_PI));
		});

		// Head bob (twice per stride) + slight sway.
		ZM_AnimAddRotCurve(xOut, szZM_INS_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_INS_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(2.0f * sinf(fP)), ZM_AnimRotX(-2.0f * sinf(2.0f * fP)));
		});

		// Antennae bob with the gait (twice per stride), opposite lateral phase.
		for (u_int a = 0; a < 2u; ++a)
		{
			const float fSide = (a == 0u) ? 1.0f : -1.0f;
			ZM_AnimAddRotCurve(xOut, szZM_INS_ANT[a], fTicks, uKeys, [fSide](float fT)
			{
				return ZM_AnimRotZ(fSide * 5.0f * sinf(2.0f * fT * fZM_INS_TWO_PI));
			});
		}
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a forward LUNGE -- the front spine + head thrust
	// forward and the FRONT legs (LegL0/LegR0) reach/swipe. No antennae (that is the
	// Special display), no mid/rear legs. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_InsBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();

		// Front spine + head thrust forward (magnitude grows toward the head).
		ZM_InsAddStrikeX(xOut, szZM_INS_SPINE[2], fD,  -6.0f, 14.0f,  6.0f);
		ZM_InsAddStrikeX(xOut, szZM_INS_SPINE[3], fD,  -8.0f, 18.0f,  8.0f);
		ZM_InsAddStrikeX(xOut, szZM_INS_HEAD,     fD, -10.0f, 24.0f, 10.0f);

		// Front legs reach forward then plant.
		for (u_int i = 0; i < 2u; ++i)
		{
			const u_int l = auZM_INS_FRONT_LEG[i];
			ZM_InsAddStrikeX(xOut, szZM_INS_LEG_UP[l], fD, -16.0f, 24.0f, 8.0f);
		}
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT threat display -- the bug REARS UP on its
	// hind legs (the front spine + head pitch back with a charged HOLD, the HIND legs
	// LegL2/LegR2 crouch/push, the FORElegs LegL0/LegR0 raise), and the antennae
	// FLARE wide (opposite RotZ). The antennae flare + hind-leg rear are the Special
	// signature (Attack keys neither). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_InsBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_InsId();

		// Front spine + head rear back (pitch up) with a charged hold, then release.
		ZM_InsAddChargeX(xOut, szZM_INS_SPINE[1], fD, -16.0f, -18.0f, 10.0f);
		ZM_InsAddChargeX(xOut, szZM_INS_SPINE[2], fD, -20.0f, -22.0f, 12.0f);
		ZM_InsAddChargeX(xOut, szZM_INS_SPINE[3], fD, -24.0f, -26.0f, 14.0f);
		ZM_InsAddChargeX(xOut, szZM_INS_HEAD,     fD, -28.0f, -30.0f, 16.0f);

		// Hind legs crouch/push to drive the rear-up (+RotX), then settle.
		for (u_int i = 0; i < 2u; ++i)
		{
			const u_int l = auZM_INS_REAR_LEG[i];
			ZM_InsAddChargeX(xOut, szZM_INS_LEG_UP[l], fD, 12.0f, 14.0f, -6.0f);
		}

		// Forelegs raise/reach out as part of the threat pose (-RotX = lift), settle.
		for (u_int i = 0; i < 2u; ++i)
		{
			const u_int l = auZM_INS_FRONT_LEG[i];
			ZM_InsAddChargeX(xOut, szZM_INS_LEG_UP[l], fD, -22.0f, -24.0f, 8.0f);
		}

		// Antennae FLARE wide (opposite RotZ so both splay out), then settle. This
		// flare is the Special signature -- Attack never keys the antennae.
		const ZM_AnimRotKey axAntL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(34.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(38.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(12.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_INS_ANT[0], axAntL, 5u);
		const ZM_AnimRotKey axAntR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(-34.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(-38.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(-12.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_INS_ANT[1], axAntR, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): a sharp body recoil BACKWARD (front spine + head snap
	// back) and ALL six legs flinch (a quick kick-out twitch), then settle. LAST key
	// returns to bind. (Backward recoil + all-legs flinch differs from Attack's
	// forward front-leg lunge.)
	// -------------------------------------------------------------------------
	void ZM_InsBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_InsId();

		// Spine recoil back + overshoot settle.
		const ZM_AnimRotKey axSpine[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-14.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_INS_SPINE[1], axSpine, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_INS_SPINE[2], axSpine, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_INS_SPINE[3], axSpine, 4u);

		// Head snap: lateral jolt composed with a backward pitch, then settle.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-18.0f), ZM_AnimRotZ(8.0f)) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(5.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_INS_HEAD, axHead, 4u);

		// All six hips flinch outward (quick kick), then settle -- fixed emit order.
		const ZM_AnimRotKey axLegTwitch[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.65f, fD), ZM_AnimRotX(5.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		for (u_int l = 0; l < 6u; ++l)
		{
			ZM_AnimAddRotKeys(xOut, szZM_INS_LEG_UP[l], axLegTwitch, 4u);
		}
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC collapse of a dead insect -- the body folds
	// forward-down, the head drops, ALL six legs curl UNDER (hips fold up, knees curl
	// tight in the opposite sense), and the antennae droop. The LAST keyframe IS the
	// settled downed pose (it does NOT return to bind), so the sampler clamps to it
	// and the KO pose holds. No root translation.
	// -------------------------------------------------------------------------
	void ZM_InsBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_InsId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		// Body folds forward-down, monotonically; fold grows toward the head end.
		for (u_int i = 1; i < 4u; ++i)
		{
			const float fFold = 16.0f + 6.0f * static_cast<float>(i);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotX(0.35f * fFold) },
				{ fT2, ZM_AnimRotX(0.70f * fFold) },
				{ fT3, ZM_AnimRotX(fFold)         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_INS_SPINE[i], ax, 4u);
		}

		// Head drops.
		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(22.0f) }, { fT2, ZM_AnimRotX(44.0f) }, { fT3, ZM_AnimRotX(60.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_INS_HEAD, axHead, 4u);

		// Legs curl under: '...Up' hips fold up (+RotX), '...Lo' knees curl tight
		// (-RotX). Identical per-leg arrays, appended in the fixed emit order.
		const ZM_AnimRotKey axLegUp[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(24.0f) }, { fT2, ZM_AnimRotX(52.0f) }, { fT3, ZM_AnimRotX(78.0f) },
		};
		const ZM_AnimRotKey axLegLo[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(-30.0f) }, { fT2, ZM_AnimRotX(-64.0f) }, { fT3, ZM_AnimRotX(-92.0f) },
		};
		for (u_int l = 0; l < 6u; ++l)
		{
			ZM_AnimAddRotKeys(xOut, szZM_INS_LEG_UP[l], axLegUp, 4u);
			ZM_AnimAddRotKeys(xOut, szZM_INS_LEG_LO[l], axLegLo, 4u);
		}

		// Antennae droop (fold forward-down, opposite lateral lean), monotonic.
		for (u_int a = 0; a < 2u; ++a)
		{
			const float fSide = (a == 0u) ? 1.0f : -1.0f;
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotCompose(ZM_AnimRotX(10.0f), ZM_AnimRotZ(fSide * 5.0f))  },
				{ fT2, ZM_AnimRotCompose(ZM_AnimRotX(22.0f), ZM_AnimRotZ(fSide * 11.0f)) },
				{ fT3, ZM_AnimRotCompose(ZM_AnimRotX(32.0f), ZM_AnimRotZ(fSide * 16.0f)) },
			};
			ZM_AnimAddRotKeys(xOut, szZM_INS_ANT[a], ax, 4u);
		}
	}
}

// ---------------------------------------------------------------------------
// The INSECTOID clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Insectoid(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_InsBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_InsBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_InsBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_InsBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_InsBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_InsBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Insectoid: bad clip %u", (u_int)eClip);
		break;
	}
}
