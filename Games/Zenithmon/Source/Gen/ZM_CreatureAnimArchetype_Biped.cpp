#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Biped -- the SC2 BIPED clip builder: it authors the
// 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint) against the
// BIPED skeleton ZM_CreatureArchetype_Biped emits. Mirrors the QUADRUPED
// reference builder (ZM_CreatureAnimArchetype_Quadruped.cpp) exactly -- same
// rotation-only, fixed channel-insert order, key0==keyN looping, one-shot
// clips ending on identity except Faint (which holds the collapsed pose).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Biped.cpp
// + the ZM_CreatureArchetypeCommon kit that names the bones): a 4-node spine
// chain Spine00..Spine03 (pelvis -> shoulders; Spine00 is the single root),
// Head, four arm bones ArmLUp/Lo ArmRUp/Lo, four leg bones LegLUp/Lo LegRUp/Lo,
// and a single dorsal Crest -- 14 bones, IDENTICAL across every evolution stage
// and every biped species, which is why a clip authored against these NAMES
// transfers to all of them. A channel references ONLY these exact names.
//
// GEOMETRY (from the archetype builder): the torso is a VERTICAL egg standing on
// end (pelvis low Y, shoulders high Y); "front" is +Z. Arms hang DOWN (-Y) from
// the shoulders at +/-X; legs drop DOWN to the ground from the pelvis at +/-X.
// As in the quadruped kit +RotX pitches a bone forward/down (spine flexion, limb
// reach), -RotX pitches it back/up, RotY yaws, RotZ rolls/side-sways.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Vertical motion
// (idle sway, faint collapse) is spine-flexion ROTATION, never root translation.
// Looping clips (Idle / Walk) author full-cycle curves so key0 == keyN per
// channel (no loop pop). The one-shot Attack / Special / Hit end on ~bind
// (identity) so the clip-end clamp holds neutral; Faint is MONOTONIC and its LAST
// key IS the settled downed pose (it does NOT return to bind), so the KO pose
// holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Biped
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf

namespace
{
	// ---- The EXACT frozen biped bone-name strings (emit order). ----
	const char* const szZM_BIPED_SPINE[4]  = { "Spine00", "Spine01", "Spine02", "Spine03" };
	const char* const szZM_BIPED_HEAD      = "Head";
	const char* const szZM_BIPED_ARM_UP[2] = { "ArmLUp", "ArmRUp" };   // L, R
	const char* const szZM_BIPED_ARM_LO[2] = { "ArmLLo", "ArmRLo" };
	const char* const szZM_BIPED_LEG_UP[2] = { "LegLUp", "LegRUp" };
	const char* const szZM_BIPED_LEG_LO[2] = { "LegLLo", "LegRLo" };
	const char* const szZM_BIPED_CREST     = "Crest";

	constexpr float fZM_BIPED_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_BIPED_IDLE_KEYS = 17u;
	constexpr u_int uZM_BIPED_WALK_KEYS = 13u;

	// Walk gait shaping (namespace scope so the per-limb curve lambdas read them
	// without capture ambiguity).
	constexpr float fZM_BIPED_WALK_HIP_SWING = 16.0f;   // leg hip fore-aft swing (deg)
	constexpr float fZM_BIPED_WALK_KNEE_MID  = -6.0f;   // knee flex bias (deg)
	constexpr float fZM_BIPED_WALK_KNEE_AMP  = -10.0f;  // knee flex amplitude (deg)
	constexpr float fZM_BIPED_WALK_ARM_SWING = 18.0f;   // arm counter-swing amplitude (deg)
	constexpr float fZM_BIPED_WALK_ELB_MID   = -10.0f;  // elbow flex bias (deg)
	constexpr float fZM_BIPED_WALK_ELB_AMP   = -6.0f;   // elbow flex amplitude (deg)

	inline Zenith_Maths::Quat ZM_BipedId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): gentle spine/head sway + arm micro-sway. Every curve is a
	// pure sin(2*pi*t + phase) (+ constants) so fn(0) == fn(1) and the loop never
	// pops. Legs stay planted and the Crest stays still (Crest is the SPECIAL tell).
	// -------------------------------------------------------------------------
	void ZM_BipedBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_BIPED_IDLE_KEYS;

		// Spine sway (skip the planted pelvis root Spine00).
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_SPINE[1], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BIPED_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotZ(2.0f * sinf(fP)), ZM_AnimRotX(1.2f * sinf(fP + 0.4f)));
		});
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_SPINE[2], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(-1.5f * sinf(fT * fZM_BIPED_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_SPINE[3], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BIPED_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(1.5f * sinf(fP)), ZM_AnimRotX(1.0f * sinf(fP + 0.8f)));
		});

		// Head: slow attentive drift + shallow nod.
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BIPED_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(3.0f * sinf(fP)), ZM_AnimRotX(-1.5f + 1.5f * sinf(fP + 0.6f)));
		});

		// Arms: barely-there counter-phase sway at the shoulders (weight shift).
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_ARM_UP[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(2.0f * sinf(fT * fZM_BIPED_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_ARM_UP[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(2.0f * sinf(fT * fZM_BIPED_TWO_PI + fZM_BIPED_TWO_PI * 0.5f));
		});
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): opposed leg gait (LegL vs LegR 180deg out of phase), knees
	// follow with a phase lag, arms counter-swing CONTRALATERAL to the legs (left
	// arm with right leg), spine counter-yaw (pelvis vs shoulders), head bob. All
	// curves close (fn(0) == fn(1)).
	// -------------------------------------------------------------------------
	void ZM_BipedBuildWalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_BIPED_WALK_KEYS;

		const float afLegPhase[2] = { 0.0f, 0.5f };            // L, R -> opposed
		const float fKneeLag = 0.15f * fZM_BIPED_TWO_PI;
		const float fElbowLag = 0.10f * fZM_BIPED_TWO_PI;

		for (u_int l = 0; l < 2u; ++l)
		{
			const float fPh = afLegPhase[l] * fZM_BIPED_TWO_PI;
			ZM_AnimAddRotCurve(xOut, szZM_BIPED_LEG_UP[l], fTicks, uKeys, [fPh](float fT)
			{
				return ZM_AnimRotX(fZM_BIPED_WALK_HIP_SWING * sinf(fT * fZM_BIPED_TWO_PI + fPh));
			});
			ZM_AnimAddRotCurve(xOut, szZM_BIPED_LEG_LO[l], fTicks, uKeys, [fPh, fKneeLag](float fT)
			{
				return ZM_AnimRotX(fZM_BIPED_WALK_KNEE_MID + fZM_BIPED_WALK_KNEE_AMP * sinf(fT * fZM_BIPED_TWO_PI + fPh + fKneeLag));
			});
		}

		// Arms swing CONTRALATERAL: left arm opposes the left leg (phase + 0.5), so
		// it moves in sync with the right leg -- natural counter-swing.
		for (u_int a = 0; a < 2u; ++a)
		{
			const float fPh = (afLegPhase[a] + 0.5f) * fZM_BIPED_TWO_PI;
			ZM_AnimAddRotCurve(xOut, szZM_BIPED_ARM_UP[a], fTicks, uKeys, [fPh](float fT)
			{
				return ZM_AnimRotX(fZM_BIPED_WALK_ARM_SWING * sinf(fT * fZM_BIPED_TWO_PI + fPh));
			});
			ZM_AnimAddRotCurve(xOut, szZM_BIPED_ARM_LO[a], fTicks, uKeys, [fPh, fElbowLag](float fT)
			{
				return ZM_AnimRotX(fZM_BIPED_WALK_ELB_MID + fZM_BIPED_WALK_ELB_AMP * sinf(fT * fZM_BIPED_TWO_PI + fPh + fElbowLag));
			});
		}

		// Spine counter-yaw (pelvis root + shoulders oppose each other).
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_SPINE[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(-3.0f * sinf(fT * fZM_BIPED_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_SPINE[3], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(3.0f * sinf(fT * fZM_BIPED_TWO_PI));
		});

		// Head bob (twice per stride) + slight turn.
		ZM_AnimAddRotCurve(xOut, szZM_BIPED_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_BIPED_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(2.0f * sinf(fP)), ZM_AnimRotX(-2.0f * sinf(2.0f * fP)));
		});
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): anticipation -> strike -> recovery. The RIGHT arm
	// drives a forward melee swing while the spine + head thrust forward; the LEFT
	// arm stays out of it (this is what distinguishes Attack from the two-arm
	// Special). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BipedBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BipedId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-8.0f)  },   // rear (anticipation)
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(14.0f)  },   // thrust forward
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(6.0f)   },   // follow-through
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[1], axSpine01, 5u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(18.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(7.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[3], axSpine03, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(-8.0f)  },
			{ ZM_AnimTicksForT01(0.50f, fD), ZM_AnimRotX(16.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_HEAD, axHead, 5u);

		// Right arm: cock BACK (up), then swing hard FORWARD/down into the strike.
		const ZM_AnimRotKey axArmUp[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.18f, fD), ZM_AnimRotX(-35.0f) },   // windup (arm back/up)
			{ ZM_AnimTicksForT01(0.45f, fD), ZM_AnimRotX(55.0f)  },   // strike forward
			{ ZM_AnimTicksForT01(0.70f, fD), ZM_AnimRotX(20.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[1], axArmUp, 5u);   // ArmRUp

		// Right forearm: elbow cocked on windup, extends into the strike.
		const ZM_AnimRotKey axArmLo[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.18f, fD), ZM_AnimRotX(-20.0f) },
			{ ZM_AnimTicksForT01(0.45f, fD), ZM_AnimRotX(30.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_LO[1], axArmLo, 5u);   // ArmRLo
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT offensive tell -- BOTH arms raise while
	// the spine leans back and the head lifts (charge), the dorsal Crest flares
	// back, then the whole thing releases forward. More wind-up than Attack, drives
	// BOTH arms (never one), and is the ONLY clip that keys the Crest. LAST key
	// returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BipedBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BipedId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-18.0f) },   // lean back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-20.0f) },   // charged hold
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(14.0f)  },   // release forward
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[1], axSpine01, 5u);

		const ZM_AnimRotKey axSpine02[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-18.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(12.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[2], axSpine02, 5u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-22.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-24.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(16.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[3], axSpine03, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-26.0f) },   // head raise
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-28.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(10.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_HEAD, axHead, 5u);

		// BOTH arms raise forward/up during the charge, then thrust forward. Same
		// array applied to L and R (the mirrored bind makes the raise symmetric).
		const ZM_AnimRotKey axArmUp[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(85.0f)  },   // raised (charge)
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(90.0f)  },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(45.0f)  },   // thrust forward
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[0], axArmUp, 5u);   // ArmLUp
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[1], axArmUp, 5u);   // ArmRUp

		const ZM_AnimRotKey axArmLo[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-25.0f) },   // elbows cocked
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-28.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_LO[0], axArmLo, 5u);   // ArmLLo
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_LO[1], axArmLo, 5u);   // ArmRLo

		// Dorsal Crest flares BACK during the charge (the SPECIAL-only tell).
		const ZM_AnimRotKey axCrest[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-25.0f) },   // flare back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-30.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(-8.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_CREST, axCrest, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): sharp spine recoil BACKWARD + head snap + a brief flail
	// throwing BOTH arms outward, then settle; LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_BipedBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BipedId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-14.0f) },   // recoil back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },   // overshoot settle
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine03[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(7.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[3], axSpine03, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-20.0f), ZM_AnimRotZ(8.0f)) },   // snap
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(5.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_HEAD, axHead, 4u);

		// Both arms fly outward on impact (RotZ), then settle. Same array applied to
		// L and R (the mirrored bind makes the flail symmetric).
		const ZM_AnimRotKey axArmFlail[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.28f, fD), ZM_AnimRotZ(22.0f) },    // flail outward
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(-8.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[0], axArmFlail, 4u);   // ArmLUp
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[1], axArmFlail, 4u);   // ArmRUp
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC fold to a collapsed pose -- spine pitches
	// forward-down, head drops, arms flop forward with elbows folding, legs buckle
	// under. The LAST keyframe IS the fully-settled downed pose (it does NOT return
	// to bind), so the sampler clamps to it and the KO pose holds. No root
	// translation. The rigid Crest is left out of the collapse.
	// -------------------------------------------------------------------------
	void ZM_BipedBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_BipedId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		const ZM_AnimRotKey axSpine01[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(15.0f) }, { fT2, ZM_AnimRotX(30.0f) }, { fT3, ZM_AnimRotX(42.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine02[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(16.0f) }, { fT2, ZM_AnimRotX(32.0f) }, { fT3, ZM_AnimRotX(45.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[2], axSpine02, 4u);

		const ZM_AnimRotKey axSpine03[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(18.0f) }, { fT2, ZM_AnimRotX(34.0f) }, { fT3, ZM_AnimRotX(48.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_SPINE[3], axSpine03, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(25.0f) }, { fT2, ZM_AnimRotX(45.0f) }, { fT3, ZM_AnimRotX(62.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_BIPED_HEAD, axHead, 4u);

		// Arms flop forward ('...Up') with elbows folding ('...Lo'); identical values
		// per side (reused arrays), appended in the fixed L, R order.
		const ZM_AnimRotKey axArmUp[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(20.0f) }, { fT2, ZM_AnimRotX(45.0f) }, { fT3, ZM_AnimRotX(70.0f) },
		};
		const ZM_AnimRotKey axArmLo[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(-20.0f) }, { fT2, ZM_AnimRotX(-45.0f) }, { fT3, ZM_AnimRotX(-70.0f) },
		};
		for (u_int a = 0; a < 2u; ++a)
		{
			ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_UP[a], axArmUp, 4u);
			ZM_AnimAddRotKeys(xOut, szZM_BIPED_ARM_LO[a], axArmLo, 4u);
		}

		// Legs buckle under: '...Up' fold forward, '...Lo' knees fold back.
		const ZM_AnimRotKey axLegUp[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(22.0f) }, { fT2, ZM_AnimRotX(48.0f) }, { fT3, ZM_AnimRotX(72.0f) },
		};
		const ZM_AnimRotKey axLegLo[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(-25.0f) }, { fT2, ZM_AnimRotX(-55.0f) }, { fT3, ZM_AnimRotX(-82.0f) },
		};
		for (u_int l = 0; l < 2u; ++l)
		{
			ZM_AnimAddRotKeys(xOut, szZM_BIPED_LEG_UP[l], axLegUp, 4u);
			ZM_AnimAddRotKeys(xOut, szZM_BIPED_LEG_LO[l], axLegLo, 4u);
		}
	}
}

// ---------------------------------------------------------------------------
// The BIPED clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Biped(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_BipedBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_BipedBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_BipedBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_BipedBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_BipedBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_BipedBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Biped: bad clip %u", (u_int)eClip);
		break;
	}
}
