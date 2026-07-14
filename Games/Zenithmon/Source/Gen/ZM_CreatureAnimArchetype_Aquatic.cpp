#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimArchetype_Aquatic -- the S4 SC3 AQUATIC clip builder: it authors
// the 6 archetype clips (Idle / Walk / Attack / Special / Hit / Faint) against the
// AQUATIC skeleton ZM_CreatureArchetype_Aquatic emits. Mirrors the QUADRUPED /
// AVIAN reference builders exactly -- rotation-only, fixed channel-insert order,
// key0==keyN looping (via ZM_AnimAddRotCurve with fn(0)==fn(1)), one-shot clips
// ending on identity except Faint (which holds the collapsed pose).
//
// BONE NAMES (frozen -- extracted verbatim from ZM_CreatureArchetype_Aquatic.cpp):
// a 3-node root spine chain Spine00..Spine02 (Spine00 is the single root = pelvis/
// belly at low Y, Spine02 = back ridge at high Y), Head, a top FinDorsal, two side
// FinPecL / FinPecR pectorals, and a rear FinCaudal (tail fin) -- 8 bones,
// IDENTICAL across every evolution stage and every aquatic species, which is why a
// clip authored against these NAMES transfers to all of them. There are NO legs. A
// channel references ONLY these exact names.
//
// GEOMETRY (from the archetype builder): the body is a VERTICAL egg swept along
// WORLD Y (belly low, dorsal ridge high), THIN in X (laterally compressed) and
// ELONGATED in Z (front-back); "front" is +Z. The head projects forward (+Z); the
// dorsal fin rises UP (+Y) from the back; the caudal fin sweeps UP and BACK (-Z)
// from the rear; the two pectorals drape DOWN the flanks at +/-X. The load-bearing
// axis choice: a fish swims by yawing its rear/tail side-to-side, which for this
// vertical-swept body is rotation about WORLD Y -- so RotY is the SWIM (yaw) axis
// (body sway + caudal propulsion sweep); RotX pitches (ram / arch / nose-sag); RotZ
// rolls (pectoral flap/flare, faint body-roll). Pectoral L/R take OPPOSITE RotZ so
// both fin tips rise/fall together (a symmetric flap), the same way the AVIAN
// builder mirrors its wings.
//
// ROTATION-ONLY (v1): every clip authors rotation channels only. Swim sway, fin
// sweep, ram, and collapse are all ROTATION -- never root translation. One-shot
// Attack / Special / Hit end on ~bind (identity) so the clip-end clamp holds
// neutral; Faint is MONOTONIC and its LAST key IS the settled downed pose (it does
// NOT return to bind), so the KO pose holds.
//
// DETERMINISM: no RNG, no clock, no address-dependent data -- a pure function of
// (clip). Channels are inserted in a FIXED order every build, so the bone-channel
// hashmap's bucket layout -- and thus WriteToDataStream's serialization order --
// is byte-stable across builds and across species.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"       // ZM_ANIM_CLIP, ZM_BuildAnim_Aquatic
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"    // the rotation-clip kit

#include <cmath>   // sinf

namespace
{
	// ---- The EXACT frozen aquatic bone-name strings (emit order). ----
	const char* const szZM_AQUA_SPINE[3]     = { "Spine00", "Spine01", "Spine02" };
	const char* const szZM_AQUA_HEAD         = "Head";
	const char* const szZM_AQUA_FIN_DORSAL   = "FinDorsal";                      // top sail (Special/Faint)
	const char* const szZM_AQUA_FIN_PEC[2]   = { "FinPecL", "FinPecR" };         // side paddles
	const char* const szZM_AQUA_FIN_CAUDAL   = "FinCaudal";                      // rear propulsion fin

	constexpr float fZM_AQUA_TWO_PI = 6.28318530717958647692f;

	// Curve key counts (more keys -> smoother; any value is deterministic).
	constexpr u_int uZM_AQUA_IDLE_KEYS = 17u;
	constexpr u_int uZM_AQUA_SWIM_KEYS = 13u;

	inline Zenith_Maths::Quat ZM_AquaId()
	{
		return glm::identity<Zenith_Maths::Quat>();
	}

	// -------------------------------------------------------------------------
	// The shared swim authoring used by BOTH Idle and Walk (swim): a gentle body
	// YAW wave (RotY) down the spine, a counter-yawing head, a caudal fin trailing
	// (phase-lagged) yaw sweep, and a slow pectoral paddle (fore-aft RotX + a
	// symmetric up/down RotZ flap, opposite signs L/R). Spine00 (pelvis root) is
	// keyed ONLY when bKeyPelvis (the stronger swim drives the base; idle keeps it
	// steady). Every curve is a pure sin(2*pi*f*t + phase) so fn(0)==fn(1).
	// -------------------------------------------------------------------------
	void ZM_AquaBuildSwim(Flux_AnimationClip& xOut, u_int uKeys, float fFreq,
		float fBodyAmpBase, float fBodyAmpStep, float fCaudalAmp,
		float fPecPaddle, float fPecFlap, bool bKeyPelvis)
	{
		const float fTicks = ZM_AnimTicksForT01(1.0f, xOut.GetDuration());

		// Body yaw (RotY = the swim side-to-side axis), a soft wave up the spine.
		const u_int uStart = bKeyPelvis ? 0u : 1u;
		for (u_int i = uStart; i < 3u; ++i)
		{
			const float fAmp   = fBodyAmpBase + fBodyAmpStep * static_cast<float>(i);
			const float fPhase = 0.5f * static_cast<float>(i);
			ZM_AnimAddRotCurve(xOut, szZM_AQUA_SPINE[i], fTicks, uKeys, [fAmp, fPhase, fFreq](float fT)
			{
				return ZM_AnimRotY(fAmp * sinf(fT * fZM_AQUA_TWO_PI * fFreq + fPhase));
			});
		}

		// Head counter-yaws (opposite sign) + a tiny pitch bob.
		ZM_AnimAddRotCurve(xOut, szZM_AQUA_HEAD, fTicks, uKeys, [fBodyAmpBase, fFreq](float fT)
		{
			const float fP = fT * fZM_AQUA_TWO_PI * fFreq;
			return ZM_AnimRotCompose(ZM_AnimRotY(-fBodyAmpBase * sinf(fP)), ZM_AnimRotX(0.4f * fBodyAmpBase * sinf(2.0f * fP)));
		});

		// Caudal fin: side-to-side propulsion sweep (RotY), trailing the body (phase lag).
		ZM_AnimAddRotCurve(xOut, szZM_AQUA_FIN_CAUDAL, fTicks, uKeys, [fCaudalAmp, fFreq](float fT)
		{
			return ZM_AnimRotY(fCaudalAmp * sinf(fT * fZM_AQUA_TWO_PI * fFreq - 0.6f));
		});

		// Pectoral fins: slow paddle -- fore-aft RotX (in sync) + a symmetric up/down
		// RotZ flap with OPPOSITE signs so both fin tips rise/fall together.
		ZM_AnimAddRotCurve(xOut, szZM_AQUA_FIN_PEC[0], fTicks, uKeys, [fPecPaddle, fPecFlap, fFreq](float fT)
		{
			const float fP = fT * fZM_AQUA_TWO_PI * fFreq;
			return ZM_AnimRotCompose(ZM_AnimRotX(fPecPaddle * sinf(fP)), ZM_AnimRotZ(fPecFlap * sinf(fP + 0.3f)));
		});
		ZM_AnimAddRotCurve(xOut, szZM_AQUA_FIN_PEC[1], fTicks, uKeys, [fPecPaddle, fPecFlap, fFreq](float fT)
		{
			const float fP = fT * fZM_AQUA_TWO_PI * fFreq;
			return ZM_AnimRotCompose(ZM_AnimRotX(fPecPaddle * sinf(fP)), ZM_AnimRotZ(-fPecFlap * sinf(fP + 0.3f)));
		});
	}

	// -------------------------------------------------------------------------
	// IDLE (2.0s, loop): a gentle hover -- body-yaw sway (pelvis steady) + caudal fin
	// trailing sway + pectoral slow paddle. The dorsal sail stays still (it is the
	// Special display signature). key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildIdle(Flux_AnimationClip& xOut)
	{
		ZM_AquaBuildSwim(xOut, uZM_AQUA_IDLE_KEYS, 1.0f, 2.0f, 1.0f, 5.0f, 4.0f, 4.0f, false);
	}

	// -------------------------------------------------------------------------
	// WALK == SWIM (1.0s, loop): the SAME body-yaw wave but the pelvis root drives
	// too and the CAUDAL fin sweeps far harder (side-to-side propulsion), pectorals
	// paddling. key0==keyN.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildWalk(Flux_AnimationClip& xOut)
	{
		ZM_AquaBuildSwim(xOut, uZM_AQUA_SWIM_KEYS, 1.0f, 4.0f, 1.5f, 14.0f, 6.0f, 5.0f, true);
	}

	// One-shot RotX ram channel: pitch back (anticipation) -> thrust forward (+RotX)
	// -> follow-through -> settle to identity.
	void ZM_AquaAddRamX(Flux_AnimationClip& xOut, const char* szBone, float fD,
		float fAnticip, float fThrust, float fFollow)
	{
		const Zenith_Maths::Quat xId = ZM_AquaId();
		const ZM_AnimRotKey ax[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotX(fAnticip) },
			{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotX(fThrust)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotX(fFollow)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szBone, ax, 5u);
	}

	// -------------------------------------------------------------------------
	// ATTACK (0.7s, one-shot): a body ram -- the torso + head pitch back then thrust
	// forward (+RotX), driven by a CAUDAL power stroke (RotY to one side). No
	// pectorals, no dorsal (those are the Special display). LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildAttack(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AquaId();

		ZM_AquaAddRamX(xOut, szZM_AQUA_SPINE[0], fD,  -6.0f, 12.0f, 4.0f);
		ZM_AquaAddRamX(xOut, szZM_AQUA_SPINE[1], fD,  -8.0f, 16.0f, 6.0f);
		ZM_AquaAddRamX(xOut, szZM_AQUA_SPINE[2], fD, -10.0f, 20.0f, 7.0f);
		ZM_AquaAddRamX(xOut, szZM_AQUA_HEAD,     fD, -12.0f, 24.0f, 9.0f);

		// Caudal power stroke drives the ram (RotY load -> thrust -> settle to identity).
		const ZM_AnimRotKey axCaudal[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.20f, fD), ZM_AnimRotY(-14.0f) },
			{ ZM_AnimTicksForT01(0.48f, fD), ZM_AnimRotY(26.0f)  },
			{ ZM_AnimTicksForT01(0.72f, fD), ZM_AnimRotY(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_CAUDAL, axCaudal, 5u);
	}

	// -------------------------------------------------------------------------
	// SPECIAL (0.9s, one-shot): a DISTINCT display tell -- the pectoral fins FLARE
	// wide (opposite RotZ = both spread), the body ARCHES back (-RotX charged hold),
	// the dorsal sail ERECTS, the head raises, then it settles. It NEVER keys the
	// caudal (the caudal thrust is the Attack ram), so the pectoral+dorsal flare
	// clearly separates it from Attack. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildSpecial(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AquaId();

		// Body arches back (-RotX) with a charged hold, then releases.
		const ZM_AnimRotKey axSpine1[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-14.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-16.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_SPINE[1], axSpine1, 5u);
		const ZM_AnimRotKey axSpine2[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-18.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-20.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(8.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_SPINE[2], axSpine2, 5u);

		// Head raises.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-24.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-26.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(-6.0f)  },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_HEAD, axHead, 5u);

		// Dorsal sail erects (pitch the blade up via RotX) -- the display signature.
		const ZM_AnimRotKey axDorsal[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotX(-30.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(-34.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotX(-10.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_DORSAL, axDorsal, 5u);

		// Pectorals FLARE wide (opposite RotZ = both spread out), hold, settle.
		const ZM_AnimRotKey axPecL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(45.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(50.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(18.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[0], axPecL, 5u);
		const ZM_AnimRotKey axPecR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.35f, fD), ZM_AnimRotZ(-45.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotZ(-50.0f) },
			{ ZM_AnimTicksForT01(0.80f, fD), ZM_AnimRotZ(-18.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[1], axPecR, 5u);
	}

	// -------------------------------------------------------------------------
	// HIT (0.4s, one-shot): a body recoil (spine + head snap back) plus a caudal +
	// pectoral fin flick, then settle. No dorsal. LAST key returns to bind.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildHit(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AquaId();

		const ZM_AnimRotKey axSpine1[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-12.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(5.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_SPINE[1], axSpine1, 4u);
		const ZM_AnimRotKey axSpine2[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotX(-14.0f) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_SPINE[2], axSpine2, 4u);

		// Head snap: lateral jolt composed with a small backward pitch, then settle.
		const ZM_AnimRotKey axHead[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.25f, fD), ZM_AnimRotCompose(ZM_AnimRotX(-16.0f), ZM_AnimRotZ(7.0f)) },
			{ ZM_AnimTicksForT01(0.55f, fD), ZM_AnimRotX(4.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_HEAD, axHead, 4u);

		// Caudal fin flick (RotY) then settle.
		const ZM_AnimRotKey axCaudal[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.30f, fD), ZM_AnimRotY(20.0f) },
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotY(-6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_CAUDAL, axCaudal, 4u);

		// Pectorals flinch outward (opposite RotZ = symmetric) then settle.
		const ZM_AnimRotKey axPecL[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.28f, fD), ZM_AnimRotZ(22.0f) },
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(-6.0f) },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[0], axPecL, 4u);
		const ZM_AnimRotKey axPecR[] = {
			{ ZM_AnimTicksForT01(0.00f, fD), xId },
			{ ZM_AnimTicksForT01(0.28f, fD), ZM_AnimRotZ(-22.0f) },
			{ ZM_AnimTicksForT01(0.60f, fD), ZM_AnimRotZ(6.0f)   },
			{ ZM_AnimTicksForT01(1.00f, fD), xId },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[1], axPecR, 4u);
	}

	// -------------------------------------------------------------------------
	// FAINT (1.2s, one-shot): MONOTONIC collapse -- the body ROLLS onto its side
	// (+RotZ) with the nose sagging (+RotX), the head drooping, and every fin going
	// slack. The LAST keyframe IS the settled downed tilt (it does NOT return to
	// bind), so the sampler clamps to it and the belly-up KO pose holds. No root
	// translation.
	// -------------------------------------------------------------------------
	void ZM_AquaBuildFaint(Flux_AnimationClip& xOut)
	{
		const float fD = xOut.GetDuration();
		const Zenith_Maths::Quat xId = ZM_AquaId();

		const float fT0 = ZM_AnimTicksForT01(0.00f, fD);
		const float fT1 = ZM_AnimTicksForT01(0.40f, fD);
		const float fT2 = ZM_AnimTicksForT01(0.70f, fD);
		const float fT3 = ZM_AnimTicksForT01(1.00f, fD);

		// Whole body rolls onto its side + nose sags, monotonically.
		for (u_int i = 0; i < 3u; ++i)
		{
			const float fRoll = 30.0f + 8.0f * static_cast<float>(i);
			const float fSag  = 6.0f + 3.0f * static_cast<float>(i);
			const ZM_AnimRotKey ax[] = {
				{ fT0, xId },
				{ fT1, ZM_AnimRotCompose(ZM_AnimRotZ(0.35f * fRoll), ZM_AnimRotX(0.35f * fSag)) },
				{ fT2, ZM_AnimRotCompose(ZM_AnimRotZ(0.70f * fRoll), ZM_AnimRotX(0.70f * fSag)) },
				{ fT3, ZM_AnimRotCompose(ZM_AnimRotZ(fRoll),         ZM_AnimRotX(fSag))         },
			};
			ZM_AnimAddRotKeys(xOut, szZM_AQUA_SPINE[i], ax, 4u);
		}

		// Head droops to the side.
		const ZM_AnimRotKey axHead[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(14.0f) }, { fT2, ZM_AnimRotZ(30.0f) }, { fT3, ZM_AnimRotZ(44.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_HEAD, axHead, 4u);

		// Dorsal sail flops slack to the side.
		const ZM_AnimRotKey axDorsal[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(16.0f) }, { fT2, ZM_AnimRotZ(34.0f) }, { fT3, ZM_AnimRotZ(50.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_DORSAL, axDorsal, 4u);

		// Pectorals go slack (opposite RotZ so both sag down/out).
		const ZM_AnimRotKey axPecL[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(12.0f) }, { fT2, ZM_AnimRotZ(26.0f) }, { fT3, ZM_AnimRotZ(38.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[0], axPecL, 4u);
		const ZM_AnimRotKey axPecR[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotZ(-12.0f) }, { fT2, ZM_AnimRotZ(-26.0f) }, { fT3, ZM_AnimRotZ(-38.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_PEC[1], axPecR, 4u);

		// Caudal fin trails slack (RotY droop to one side).
		const ZM_AnimRotKey axCaudal[] = {
			{ fT0, xId }, { fT1, ZM_AnimRotY(10.0f) }, { fT2, ZM_AnimRotY(22.0f) }, { fT3, ZM_AnimRotY(32.0f) },
		};
		ZM_AnimAddRotKeys(xOut, szZM_AQUA_FIN_CAUDAL, axCaudal, 4u);
	}
}

// ---------------------------------------------------------------------------
// The AQUATIC clip builder. Signature matches the frozen ZM_CreatureAnimGen.h
// declaration exactly. The driver has already set the clip's golden metadata
// (name / duration / ticks-per-second / looping) before this runs.
// ---------------------------------------------------------------------------
void ZM_BuildAnim_Aquatic(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	switch (eClip)
	{
	case ZM_ANIM_CLIP_IDLE:    ZM_AquaBuildIdle(xOut);    break;
	case ZM_ANIM_CLIP_WALK:    ZM_AquaBuildWalk(xOut);    break;
	case ZM_ANIM_CLIP_ATTACK:  ZM_AquaBuildAttack(xOut);  break;
	case ZM_ANIM_CLIP_SPECIAL: ZM_AquaBuildSpecial(xOut); break;
	case ZM_ANIM_CLIP_HIT:     ZM_AquaBuildHit(xOut);     break;
	case ZM_ANIM_CLIP_FAINT:   ZM_AquaBuildFaint(xOut);   break;
	default:
		Zenith_Assert(false, "ZM_BuildAnim_Aquatic: bad clip %u", (u_int)eClip);
		break;
	}
}
