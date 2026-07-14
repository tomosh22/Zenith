#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Avian -- the SC2 AVIAN clip builder: it authors the
// 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint) against the
// AVIAN skeleton ZM_CreatureArchetype_Avian emits. Mirrors the QUADRUPED
// reference builder (ZM_CreatureAnimArchetype_Quadruped.cpp) exactly -- same
// rotation-only, fixed channel-insert order, key0==keyN looping, one-shot
// clips ending on identity except Faint (which holds the collapsed pose).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Avian.cpp
// + the ZM_CreatureArchetypeCommon kit that names the bones): a 3-node spine
// chain Spine00..Spine02 (belly -> chest; Spine00 is the single root), Head, a
// forward Beak, two single-bone Wings WingL / WingR, four leg bones LegLUp/Lo
// LegRUp/Lo, and a 2-node tail Tail00 / Tail01 -- 13 bones, IDENTICAL across
// every evolution stage and every avian species, which is why a clip authored
// against these NAMES transfers to all of them. A channel references ONLY these
// exact names.
//
// GEOMETRY (from the archetype builder): the torso is a VERTICAL egg (belly low
// Y, chest high Y); "front" is +Z. The head sits above/forward of the chest; the
// beak juts FORWARD (+Z); the two wings drape DOWN the flanks as single-bone
// blades at +/-X; the legs drop DOWN to the ground; the tail sweeps back (-Z) and
// down. As in the quadruped kit +RotX pitches a bone forward/down (spine flexion,
// peck thrust, limb reach), -RotX pitches it back/up, RotY yaws (tail/head sway),
// RotZ rolls -- used here as the WING up/down flap: L and R take OPPOSITE RotZ
// signs so both wingtips rise/fall together (a symmetric flap/flare rather than a
// body roll), the same way the quadruped applies mirrored motion to its paired
// limbs.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Vertical motion
// (idle bob, faint collapse) is spine-flexion ROTATION, never root translation.
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

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Avian
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf

namespace
{
	// ---- The EXACT frozen avian bone-name strings (emit order). ----
	const char* const szZM_AVIAN_SPINE[3]  = { "Spine00", "Spine01", "Spine02" };
	const char* const szZM_AVIAN_HEAD      = "Head";
	const char* const szZM_AVIAN_BEAK      = "Beak";
	const char* const szZM_AVIAN_WING[2]   = { "WingL", "WingR" };   // L, R (single bone each)
	const char* const szZM_AVIAN_LEG_UP[2] = { "LegLUp", "LegRUp" };
	const char* const szZM_AVIAN_LEG_LO[2] = { "LegLLo", "LegRLo" };
	const char* const szZM_AVIAN_TAIL[2]   = { "Tail00", "Tail01" };

	constexpr float fZM_AVIAN_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_AVIAN_IDLE_KEYS = 17u;
	constexpr u_int uZM_AVIAN_WALK_KEYS = 13u;

	// Walk gait shaping (namespace scope so the per-limb curve lambdas read them
	// without capture ambiguity).
	constexpr float fZM_AVIAN_WALK_HIP_SWING = 20.0f;   // leg hip fore-aft swing (deg)
	constexpr float fZM_AVIAN_WALK_KNEE_MID  = -8.0f;   // knee flex bias (deg)
	constexpr float fZM_AVIAN_WALK_KNEE_AMP  = -12.0f;  // knee flex amplitude (deg)

	inline Zenith_Maths::Quat ZM_AvianId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): wing settle micro-flap + head bob + tail balance sway.
	// Every curve is a pure sin(2*pi*t + phase) (+ constants) so fn(0) == fn(1) and
	// the loop never pops. The wings take OPPOSITE RotZ so both tips flap together;
	// legs stay planted and the Beak stays still (Beak is the ATTACK peck lead).
	// -------------------------------------------------------------------------
	void ZM_AvianBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_AVIAN_IDLE_KEYS;

		// Spine breathing (skip the planted pelvis root Spine00).
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_SPINE[1], fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_AVIAN_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotX(1.5f * sinf(fP)), ZM_AnimRotZ(1.0f * sinf(fP + 0.3f)));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_SPINE[2], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotX(1.2f * sinf(fT * fZM_AVIAN_TWO_PI + 0.5f));
		});

		// Head: shallow bob + attentive look drift.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_AVIAN_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotX(-2.0f + 2.0f * sinf(fP)), ZM_AnimRotY(2.5f * sinf(fP + 0.4f)));
		});

		// Wings: micro settle-flap. Opposite RotZ so both wingtips rise/fall together.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_WING[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(4.0f * sinf(fT * fZM_AVIAN_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_WING[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(-4.0f * sinf(fT * fZM_AVIAN_TWO_PI));
		});

		// Tail: gentle balance sway, growing toward the tip.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_TAIL[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(3.0f * sinf(fT * fZM_AVIAN_TWO_PI + 0.2f));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_TAIL[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(4.0f * sinf(fT * fZM_AVIAN_TWO_PI + 0.4f));
		});
	}

	// -------------------------------------------------------------------------
	// WALK (1.0s, loop): stepping gait (LegL vs LegR 180deg out of phase), knees
	// follow with a phase lag, wings hold with a micro-settle, spine/pelvis counter-
	// yaw, tail counterbalances the body sway, head bobs. All curves close.
	// -------------------------------------------------------------------------
	void ZM_AvianBuildWalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());
		const u_int uKeys  = uZM_AVIAN_WALK_KEYS;

		const float afLegPhase[2] = { 0.0f, 0.5f };            // L, R -> opposed
		const float fKneeLag = 0.15f * fZM_AVIAN_TWO_PI;

		for (u_int l = 0; l < 2u; ++l)
		{
			const float fPh = afLegPhase[l] * fZM_AVIAN_TWO_PI;
			ZM_AnimAddRotCurve(xOut, szZM_AVIAN_LEG_UP[l], fTicks, uKeys, [fPh](float fT)
			{
				return ZM_AnimRotX(fZM_AVIAN_WALK_HIP_SWING * sinf(fT * fZM_AVIAN_TWO_PI + fPh));
			});
			ZM_AnimAddRotCurve(xOut, szZM_AVIAN_LEG_LO[l], fTicks, uKeys, [fPh, fKneeLag](float fT)
			{
				return ZM_AnimRotX(fZM_AVIAN_WALK_KNEE_MID + fZM_AVIAN_WALK_KNEE_AMP * sinf(fT * fZM_AVIAN_TWO_PI + fPh + fKneeLag));
			});
		}

		// Wings: small settle sway with each step (opposite RotZ = symmetric).
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_WING[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(3.0f * sinf(fT * fZM_AVIAN_TWO_PI));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_WING[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotZ(-3.0f * sinf(fT * fZM_AVIAN_TWO_PI));
		});

		// Pelvis counter-yaw with the stride.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_SPINE[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(-4.0f * sinf(fT * fZM_AVIAN_TWO_PI));
		});

		// Tail counterbalances the body yaw (opposite phase), growing toward the tip.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_TAIL[0], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(5.0f * sinf(fT * fZM_AVIAN_TWO_PI + fZM_AVIAN_TWO_PI * 0.5f));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_TAIL[1], fTicks, uKeys, [](float fT)
		{
			return ZM_AnimRotY(6.0f * sinf(fT * fZM_AVIAN_TWO_PI + fZM_AVIAN_TWO_PI * 0.5f));
		});

		// Head bob (twice per stride) + slight turn.
		ZM_AnimAddRotCurve(xOut, szZM_AVIAN_HEAD, fTicks, uKeys, [](float fT)
		{
			const float fP = fT * fZM_AVIAN_TWO_PI;
			return ZM_AnimRotCompose(ZM_AnimRotY(2.0f * sinf(fP)), ZM_AnimRotX(-3.0f * sinf(2.0f * fP)));
		});
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a peck -- the neck (Spine02) + Head cock back then
	// thrust forward/down, the Beak leads the jab, and the wings tuck in tight.
	// Beak is the peck signature (the ONLY clip that keys it). LAST key returns to
	// bind.
	// -------------------------------------------------------------------------
	void ZM_AvianBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AvianId();

		const ZM_AnimRotKey axSpine02[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.18f, fD), ZM_AnimRotX(-8.0f)  },   // rear (anticipation)
			{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotX(16.0f)  },   // thrust forward
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[2], axSpine02, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.18f, fD), ZM_AnimRotX(-12.0f) },   // cock back
			{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotX(28.0f)  },   // peck down/forward
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(10.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_HEAD, axHead, 5u);

		const ZM_AnimRotKey axBeak[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.18f, fD), ZM_AnimRotX(-6.0f)  },
			{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotX(18.0f)  },   // jab lead
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_BEAK, axBeak, 5u);

		// Wings tuck IN (opposite RotZ = symmetric fold against the flanks).
		const ZM_AnimRotKey axWingL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotZ(-10.0f) },   // fold in
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(-6.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[0], axWingL, 4u);
		const ZM_AnimRotKey axWingR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotZ(10.0f)  },
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[1], axWingR, 4u);
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT screech tell -- the wings FLARE wide, the
	// spine puffs back, the head raises, and the tail fans up; then it settles. The
	// wing flare (opposite RotZ, large) and the raised head clearly differ from the
	// peck, and it NEVER keys the Beak. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_AvianBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AvianId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-12.0f) },   // puff back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-14.0f) },   // charged hold
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[1], axSpine01, 5u);

		const ZM_AnimRotKey axSpine02[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-18.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[2], axSpine02, 5u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-30.0f) },   // screech up
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-34.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(-6.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_HEAD, axHead, 5u);

		// Wings FLARE wide (opposite RotZ = both spread outward), then settle.
		const ZM_AnimRotKey axWingL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(55.0f) },    // spread out
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(60.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotZ(20.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[0], axWingL, 5u);
		const ZM_AnimRotKey axWingR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(-55.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(-60.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotZ(-20.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[1], axWingR, 5u);

		// Tail fans UP as part of the display.
		const ZM_AnimRotKey axTail[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-14.0f) },   // fan up
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.78f, fD), ZM_AnimRotX(-4.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_TAIL[0], axTail, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): sharp spine + head recoil BACKWARD + a brief wing flinch
	// throwing both wings outward, then settle; LAST key returns to bind. It keys no
	// Tail (which distinguishes it from the tail-fanning Special).
	// -------------------------------------------------------------------------
	void ZM_AvianBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AvianId();

		const ZM_AnimRotKey axSpine01[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-12.0f) },   // recoil back
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(5.0f)   },   // overshoot settle
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine02[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-14.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[2], axSpine02, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-18.0f), ZM_AnimRotZ(7.0f)) },   // snap
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(4.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_HEAD, axHead, 4u);

		// Wings flinch outward on impact (opposite RotZ = symmetric), then settle.
		const ZM_AnimRotKey axWingL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.28f, fD), ZM_AnimRotZ(25.0f) },    // flinch out
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(-6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[0], axWingL, 4u);
		const ZM_AnimRotKey axWingR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.28f, fD), ZM_AnimRotZ(-25.0f) },
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[1], axWingR, 4u);
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC fold to a collapsed pose -- spine pitches
	// forward-down, head drops, wings go SLACK and droop open, legs buckle under,
	// tail sags. The LAST keyframe IS the fully-settled downed pose (it does NOT
	// return to bind), so the sampler clamps to it and the KO pose holds. No root
	// translation. The Beak is left out of the collapse.
	// -------------------------------------------------------------------------
	void ZM_AvianBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD  = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AvianId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		const ZM_AnimRotKey axSpine01[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(16.0f) }, { fT2, ZM_AnimRotX(32.0f) }, { fT3, ZM_AnimRotX(45.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[1], axSpine01, 4u);

		const ZM_AnimRotKey axSpine02[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(18.0f) }, { fT2, ZM_AnimRotX(35.0f) }, { fT3, ZM_AnimRotX(50.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_SPINE[2], axSpine02, 4u);

		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(24.0f) }, { fT2, ZM_AnimRotX(46.0f) }, { fT3, ZM_AnimRotX(64.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_HEAD, axHead, 4u);

		// Wings droop open and slack (opposite RotZ = both sag outward/down).
		const ZM_AnimRotKey axWingL[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(15.0f) }, { fT2, ZM_AnimRotZ(30.0f) }, { fT3, ZM_AnimRotZ(42.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[0], axWingL, 4u);
		const ZM_AnimRotKey axWingR[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(-15.0f) }, { fT2, ZM_AnimRotZ(-30.0f) }, { fT3, ZM_AnimRotZ(-42.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_WING[1], axWingR, 4u);

		// Legs buckle under: '...Up' fold forward, '...Lo' knees fold back.
		const ZM_AnimRotKey axLegUp[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(22.0f) }, { fT2, ZM_AnimRotX(48.0f) }, { fT3, ZM_AnimRotX(72.0f) },
		};
		const ZM_AnimRotKey axLegLo[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(-25.0f) }, { fT2, ZM_AnimRotX(-55.0f) }, { fT3, ZM_AnimRotX(-82.0f) },
		};
		for (u_int l = 0; l < 2u; ++l)
		{
			ZM_AnimAddRotKeys(xOut, szZM_AVIAN_LEG_UP[l], axLegUp, 4u);
			ZM_AnimAddRotKeys(xOut, szZM_AVIAN_LEG_LO[l], axLegLo, 4u);
		}

		// Tail sags down.
		const ZM_AnimRotKey axTail0[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(10.0f) }, { fT2, ZM_AnimRotX(22.0f) }, { fT3, ZM_AnimRotX(34.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_TAIL[0], axTail0, 4u);
		const ZM_AnimRotKey axTail1[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotX(12.0f) }, { fT2, ZM_AnimRotX(26.0f) }, { fT3, ZM_AnimRotX(40.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AVIAN_TAIL[1], axTail1, 4u);
	}
}

// ---------------------------------------------------------------------------
// The AVIAN clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Avian(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_AvianBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_AvianBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_AvianBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_AvianBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_AvianBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_AvianBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Avian: bad clip %u", (u_int)eClip);
		break;
	}
}
