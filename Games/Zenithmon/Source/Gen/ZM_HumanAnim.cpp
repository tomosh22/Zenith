#include "Zenith.h"

// ============================================================================
// ZM_HumanAnim -- SC4's deterministic shared-human clip authoring. The nine
// clips are pure functions of clip id, use the frozen shared bone names, and
// contain rotation channels only. Looping curves close byte-exactly at t=1.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"

#include <cmath>

namespace
{
	using Zenith_Maths::Quat;

	// Frozen shared-human bone names, grouped in skeleton insertion order.
	const char* const szZM_HUMAN_ROOT = "Root";
	const char* const szZM_HUMAN_SPINE = "Spine";
	const char* const szZM_HUMAN_NECK = "Neck";
	const char* const szZM_HUMAN_HEAD = "Head";
	const char* const szZM_HUMAN_LEFT_UPPER_ARM = "LeftUpperArm";
	const char* const szZM_HUMAN_LEFT_LOWER_ARM = "LeftLowerArm";
	const char* const szZM_HUMAN_LEFT_HAND = "LeftHand";
	const char* const szZM_HUMAN_RIGHT_UPPER_ARM = "RightUpperArm";
	const char* const szZM_HUMAN_RIGHT_LOWER_ARM = "RightLowerArm";
	const char* const szZM_HUMAN_RIGHT_HAND = "RightHand";
	const char* const szZM_HUMAN_LEFT_UPPER_LEG = "LeftUpperLeg";
	const char* const szZM_HUMAN_LEFT_LOWER_LEG = "LeftLowerLeg";
	const char* const szZM_HUMAN_LEFT_FOOT = "LeftFoot";
	const char* const szZM_HUMAN_RIGHT_UPPER_LEG = "RightUpperLeg";
	const char* const szZM_HUMAN_RIGHT_LOWER_LEG = "RightLowerLeg";
	const char* const szZM_HUMAN_RIGHT_FOOT = "RightFoot";

	const char* const aszZM_HUMAN_UPPER_ARMS[2] =
	{
		szZM_HUMAN_LEFT_UPPER_ARM,
		szZM_HUMAN_RIGHT_UPPER_ARM,
	};
	const char* const aszZM_HUMAN_LOWER_ARMS[2] =
	{
		szZM_HUMAN_LEFT_LOWER_ARM,
		szZM_HUMAN_RIGHT_LOWER_ARM,
	};
	const char* const aszZM_HUMAN_UPPER_LEGS[2] =
	{
		szZM_HUMAN_LEFT_UPPER_LEG,
		szZM_HUMAN_RIGHT_UPPER_LEG,
	};
	const char* const aszZM_HUMAN_LOWER_LEGS[2] =
	{
		szZM_HUMAN_LEFT_LOWER_LEG,
		szZM_HUMAN_RIGHT_LOWER_LEG,
	};
	const char* const aszZM_HUMAN_FEET[2] =
	{
		szZM_HUMAN_LEFT_FOOT,
		szZM_HUMAN_RIGHT_FOOT,
	};

	constexpr float fZM_HUMAN_PI = 3.14159265358979323846f;
	constexpr float fZM_HUMAN_TWO_PI = 6.28318530717958647692f;
	constexpr u_int uZM_HUMAN_LOOP_KEYS = 25u;

	Quat ZM_HumanIdentity()
	{
		return ZM_AnimRotX(0.0f);
	}

	float ZM_HumanTick(const Flux_AnimationClip& xClip, float fT01)
	{
		return ZM_AnimTicksForT01(fT01, xClip.GetDuration());
	}

	// The sampled helper supplies t=1 exactly. Map it back to t=0 before every
	// periodic expression so the first and last quaternion bytes are identical.
	float ZM_HumanLoopT(float fT01)
	{
		return (fT01 >= 1.0f) ? 0.0f : fT01;
	}

	float ZM_HumanWrap01(float fValue)
	{
		return fValue - floorf(fValue);
	}

	float ZM_HumanRaisedCosine(float fQ, float fCentre, float fWidth)
	{
		float fDistance = fabsf(fQ - fCentre);
		if (fDistance > 0.5f)
		{
			fDistance = 1.0f - fDistance;
		}
		if (fDistance > fWidth)
		{
			return 0.0f;
		}
		const float fX = fDistance / fWidth;
		return 0.5f + 0.5f * cosf(fZM_HUMAN_PI * fX);
	}

	void ZM_HumanBuildIdle(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_HumanTick(xOut, 1.0f);

		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_ROOT, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotZ(0.9f * sinf(fP));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_SPINE, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotX(1.1f * sinf(fP + 0.3f)),
					ZM_AnimRotZ(0.5f * sinf(fP)));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_NECK, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotX(0.45f * sinf(fP + 0.5f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_HEAD, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(2.4f * sinf(fP)),
					ZM_AnimRotX(-1.0f + 0.9f * sinf(fP + 0.7f)));
			});

		const float afPhase[2] = { 0.9f, 2.1f };
		const float afOut[2] = { -4.0f, 4.0f };
		for (u_int uSide = 0u; uSide < 2u; ++uSide)
		{
			const float fPhase = afPhase[uSide];
			const float fOut = afOut[uSide];
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_UPPER_ARMS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [fPhase, fOut](float fT01)
				{
					const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
					return ZM_AnimRotCompose(
						ZM_AnimRotX(1.6f * sinf(fP + fPhase)),
						ZM_AnimRotZ(fOut));
				});
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_LOWER_ARMS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [fPhase](float fT01)
				{
					const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
					return ZM_AnimRotX(-8.0f - 1.5f * sinf(fP + fPhase));
				});
		}
	}

	void ZM_HumanBuildTalk(Flux_AnimationClip& xOut)
	{
		const float fTicks = ZM_HumanTick(xOut, 1.0f);

		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_SPINE, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(3.0f * sinf(fP)),
					ZM_AnimRotX(1.25f * sinf(2.0f * fP + 0.3f)));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_NECK, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(-1.5f * sinf(fP)),
					ZM_AnimRotX(0.8f * sinf(2.0f * fP + 0.5f)));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_HEAD, fTicks, uZM_HUMAN_LOOP_KEYS,
			[](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(6.0f * sinf(fP)),
					ZM_AnimRotX(-2.0f + 2.5f * sinf(2.0f * fP + 0.4f)));
			});

		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_LEFT_UPPER_ARM, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotX(-12.0f - 8.0f * sinf(fP)),
					ZM_AnimRotZ(-6.0f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_LEFT_LOWER_ARM, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotX(-26.0f - 10.0f * sinf(fP + 0.5f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_LEFT_HAND, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(-10.0f * sinf(2.0f * fP + 0.3f)),
					ZM_AnimRotZ(-4.0f * sinf(fP)));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotX(-12.0f + 8.0f * sinf(fP)),
					ZM_AnimRotZ(6.0f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotX(-26.0f + 10.0f * sinf(fP + 0.5f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_RIGHT_HAND, fTicks,
			uZM_HUMAN_LOOP_KEYS, [](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(10.0f * sinf(2.0f * fP + 0.3f)),
					ZM_AnimRotZ(4.0f * sinf(fP)));
			});
	}

	struct ZM_HumanGait
	{
		float m_fForward;
		float m_fBack;
		float m_fStance;
		float m_fSwing;
		float m_fArm;
		float m_fElbowBase;
		float m_fElbowPump;
		float m_fLean;
		float m_fYaw;
	};

	void ZM_HumanBuildLocomotion(Flux_AnimationClip& xOut, const ZM_HumanGait& xGait)
	{
		const float fTicks = ZM_HumanTick(xOut, 1.0f);

		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_ROOT, fTicks, uZM_HUMAN_LOOP_KEYS,
			[xGait](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotY(xGait.m_fYaw * cosf(fP)),
					ZM_AnimRotZ(1.6f * sinf(fP)));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_SPINE, fTicks, uZM_HUMAN_LOOP_KEYS,
			[xGait](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotX(xGait.m_fLean),
					ZM_AnimRotCompose(
						ZM_AnimRotY(-1.4f * xGait.m_fYaw * cosf(fP)),
						ZM_AnimRotZ(-1.2f * sinf(fP))));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_NECK, fTicks, uZM_HUMAN_LOOP_KEYS,
			[xGait](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotX(-0.25f * xGait.m_fLean
					+ 0.4f * cosf(2.0f * fP + 0.5f));
			});
		ZM_AnimAddRotCurve(xOut, szZM_HUMAN_HEAD, fTicks, uZM_HUMAN_LOOP_KEYS,
			[xGait](float fT01)
			{
				const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
				return ZM_AnimRotCompose(
					ZM_AnimRotX(-0.55f * xGait.m_fLean + 0.8f * cosf(2.0f * fP)),
					ZM_AnimRotY(0.45f * xGait.m_fYaw * cosf(fP)));
			});

		const float afArmSign[2] = { 1.0f, -1.0f };
		const float afArmOut[2] = { -3.5f, 3.5f };
		for (u_int uSide = 0u; uSide < 2u; ++uSide)
		{
			const float fSign = afArmSign[uSide];
			const float fOut = afArmOut[uSide];
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_UPPER_ARMS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [xGait, fSign, fOut](float fT01)
				{
					const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
					return ZM_AnimRotCompose(
						ZM_AnimRotX(-2.0f + fSign * xGait.m_fArm * cosf(fP)),
						ZM_AnimRotZ(fOut));
				});
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_LOWER_ARMS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [xGait, fSign](float fT01)
				{
					const float fP = fZM_HUMAN_TWO_PI * ZM_HumanLoopT(fT01);
					return ZM_AnimRotX(-xGait.m_fElbowBase
						- xGait.m_fElbowPump * 0.5f * (1.0f - fSign * cosf(fP)));
				});
		}

		const float afLegPhase[2] = { 0.0f, 0.5f };
		const float fLegMid = 0.5f * (xGait.m_fBack - xGait.m_fForward);
		const float fLegAmp = 0.5f * (xGait.m_fBack + xGait.m_fForward);
		for (u_int uSide = 0u; uSide < 2u; ++uSide)
		{
			const float fPhase = afLegPhase[uSide];
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_UPPER_LEGS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [fPhase, fLegMid, fLegAmp](float fT01)
				{
					const float fT = ZM_HumanLoopT(fT01);
					const float fQ = ZM_HumanWrap01(fT + fPhase);
					return ZM_AnimRotX(fLegMid - fLegAmp
						* cosf(fZM_HUMAN_TWO_PI * fQ));
				});
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_LOWER_LEGS[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [xGait, fPhase](float fT01)
				{
					const float fT = ZM_HumanLoopT(fT01);
					const float fQ = ZM_HumanWrap01(fT + fPhase);
					return ZM_AnimRotX(4.0f
						+ xGait.m_fStance * ZM_HumanRaisedCosine(fQ, 0.16f, 0.14f)
						+ xGait.m_fSwing * ZM_HumanRaisedCosine(fQ, 0.72f, 0.20f));
				});
			ZM_AnimAddRotCurve(xOut, aszZM_HUMAN_FEET[uSide], fTicks,
				uZM_HUMAN_LOOP_KEYS, [fPhase](float fT01)
				{
					const float fT = ZM_HumanLoopT(fT01);
					const float fQ = ZM_HumanWrap01(fT + fPhase);
					return ZM_AnimRotX(
						-7.0f * ZM_HumanRaisedCosine(fQ, 0.02f, 0.10f)
						+ 14.0f * ZM_HumanRaisedCosine(fQ, 0.52f, 0.14f)
						- 6.0f * ZM_HumanRaisedCosine(fQ, 0.80f, 0.14f));
				});
		}
	}

	void ZM_HumanBuildWalk(Flux_AnimationClip& xOut)
	{
		const ZM_HumanGait xGait =
		{
			28.0f, 18.0f, 13.0f, 52.0f, 21.0f, 16.0f, 13.0f, 3.5f, 4.5f
		};
		ZM_HumanBuildLocomotion(xOut, xGait);
	}

	void ZM_HumanBuildRun(Flux_AnimationClip& xOut)
	{
		const ZM_HumanGait xGait =
		{
			50.0f, 26.0f, 22.0f, 82.0f, 34.0f, 62.0f, 18.0f, 11.0f, 7.5f
		};
		ZM_HumanBuildLocomotion(xOut, xGait);
	}

	void ZM_HumanBuildWave(Flux_AnimationClip& xOut)
	{
		const float afT[] =
		{
			ZM_HumanTick(xOut, 0.0f), ZM_HumanTick(xOut, 0.18f),
			ZM_HumanTick(xOut, 0.35f), ZM_HumanTick(xOut, 0.52f),
			ZM_HumanTick(xOut, 0.69f), ZM_HumanTick(xOut, 0.82f),
			ZM_HumanTick(xOut, 1.0f),
		};
		const Quat xIdentity = ZM_HumanIdentity();
		const ZM_AnimRotKey axSpine[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(3.0f) },
			{ afT[2], ZM_AnimRotY(4.0f) }, { afT[3], ZM_AnimRotY(3.0f) },
			{ afT[4], ZM_AnimRotY(4.0f) }, { afT[5], ZM_AnimRotY(3.0f) },
			{ afT[6], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axNeck[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(4.0f) },
			{ afT[2], ZM_AnimRotY(6.0f) }, { afT[3], ZM_AnimRotY(4.0f) },
			{ afT[4], ZM_AnimRotY(6.0f) }, { afT[5], ZM_AnimRotY(4.0f) },
			{ afT[6], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axHead[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(8.0f) },
			{ afT[2], ZM_AnimRotY(11.0f) }, { afT[3], ZM_AnimRotY(8.0f) },
			{ afT[4], ZM_AnimRotY(11.0f) }, { afT[5], ZM_AnimRotY(8.0f) },
			{ afT[6], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-12.0f), ZM_AnimRotZ(105.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-8.0f), ZM_AnimRotZ(102.0f)) },
			{ afT[3], ZM_AnimRotCompose(ZM_AnimRotX(-12.0f), ZM_AnimRotZ(108.0f)) },
			{ afT[4], ZM_AnimRotCompose(ZM_AnimRotX(-8.0f), ZM_AnimRotZ(102.0f)) },
			{ afT[5], ZM_AnimRotCompose(ZM_AnimRotX(-12.0f), ZM_AnimRotZ(105.0f)) },
			{ afT[6], xIdentity },
		};
		const ZM_AnimRotKey axLower[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-65.0f), ZM_AnimRotY(-15.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-65.0f), ZM_AnimRotY(18.0f)) },
			{ afT[3], ZM_AnimRotCompose(ZM_AnimRotX(-65.0f), ZM_AnimRotY(-18.0f)) },
			{ afT[4], ZM_AnimRotCompose(ZM_AnimRotX(-65.0f), ZM_AnimRotY(18.0f)) },
			{ afT[5], ZM_AnimRotCompose(ZM_AnimRotX(-65.0f), ZM_AnimRotY(-10.0f)) },
			{ afT[6], xIdentity },
		};
		const ZM_AnimRotKey axHand[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(-18.0f) },
			{ afT[2], ZM_AnimRotY(24.0f) }, { afT[3], ZM_AnimRotY(-24.0f) },
			{ afT[4], ZM_AnimRotY(24.0f) }, { afT[5], ZM_AnimRotY(-12.0f) },
			{ afT[6], ZM_AnimRotY(0.0f) },
		};

		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_SPINE, axSpine, 7u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_NECK, axNeck, 7u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_HEAD, axHead, 7u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, axUpper, 7u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, axLower, 7u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_HAND, axHand, 7u);
	}

	void ZM_HumanBuildPoint(Flux_AnimationClip& xOut)
	{
		const float afT[] =
		{
			ZM_HumanTick(xOut, 0.0f), ZM_HumanTick(xOut, 0.20f),
			ZM_HumanTick(xOut, 0.38f), ZM_HumanTick(xOut, 0.72f),
			ZM_HumanTick(xOut, 1.0f),
		};
		const Quat xIdentity = ZM_HumanIdentity();
		const Quat xUpperHold = ZM_AnimRotCompose(ZM_AnimRotX(-92.0f), ZM_AnimRotZ(5.0f));
		const Quat xLowerHold = ZM_AnimRotX(-5.0f);
		const Quat xHandHold = ZM_AnimRotX(-4.0f);
		const ZM_AnimRotKey axSpine[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(6.0f) },
			{ afT[2], ZM_AnimRotY(8.0f) }, { afT[3], ZM_AnimRotY(8.0f) },
			{ afT[4], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axNeck[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(4.0f) },
			{ afT[2], ZM_AnimRotY(6.0f) }, { afT[3], ZM_AnimRotY(6.0f) },
			{ afT[4], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axHead[] =
		{
			{ afT[0], ZM_AnimRotY(0.0f) }, { afT[1], ZM_AnimRotY(10.0f) },
			{ afT[2], ZM_AnimRotY(14.0f) }, { afT[3], ZM_AnimRotY(14.0f) },
			{ afT[4], ZM_AnimRotY(0.0f) },
		};
		const ZM_AnimRotKey axUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-70.0f), ZM_AnimRotZ(8.0f)) },
			{ afT[2], xUpperHold }, { afT[3], xUpperHold }, { afT[4], xIdentity },
		};
		const ZM_AnimRotKey axLower[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-25.0f) },
			{ afT[2], xLowerHold }, { afT[3], xLowerHold },
			{ afT[4], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axHand[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(8.0f) },
			{ afT[2], xHandHold }, { afT[3], xHandHold },
			{ afT[4], ZM_AnimRotX(0.0f) },
		};

		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_SPINE, axSpine, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_NECK, axNeck, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_HEAD, axHead, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, axUpper, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, axLower, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_HAND, axHand, 5u);
	}

	void ZM_HumanBuildCheer(Flux_AnimationClip& xOut)
	{
		const float afT[] =
		{
			ZM_HumanTick(xOut, 0.0f), ZM_HumanTick(xOut, 0.18f),
			ZM_HumanTick(xOut, 0.40f), ZM_HumanTick(xOut, 0.62f),
			ZM_HumanTick(xOut, 0.82f), ZM_HumanTick(xOut, 1.0f),
		};
		const Quat xIdentity = ZM_HumanIdentity();
		const ZM_AnimRotKey axSpine[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(6.0f) },
			{ afT[2], ZM_AnimRotX(-10.0f) }, { afT[3], ZM_AnimRotX(-7.0f) },
			{ afT[4], ZM_AnimRotX(-10.0f) }, { afT[5], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axNeck[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(3.0f) },
			{ afT[2], ZM_AnimRotX(-6.0f) }, { afT[3], ZM_AnimRotX(-4.0f) },
			{ afT[4], ZM_AnimRotX(-6.0f) }, { afT[5], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axHead[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(5.0f) },
			{ afT[2], ZM_AnimRotX(-14.0f) }, { afT[3], ZM_AnimRotX(-10.0f) },
			{ afT[4], ZM_AnimRotX(-14.0f) }, { afT[5], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axLeftUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(8.0f), ZM_AnimRotZ(-20.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-15.0f), ZM_AnimRotZ(-155.0f)) },
			{ afT[3], ZM_AnimRotCompose(ZM_AnimRotX(-10.0f), ZM_AnimRotZ(-145.0f)) },
			{ afT[4], ZM_AnimRotCompose(ZM_AnimRotX(-15.0f), ZM_AnimRotZ(-160.0f)) },
			{ afT[5], xIdentity },
		};
		const ZM_AnimRotKey axLower[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-10.0f) },
			{ afT[2], ZM_AnimRotX(-25.0f) }, { afT[3], ZM_AnimRotX(-15.0f) },
			{ afT[4], ZM_AnimRotX(-25.0f) }, { afT[5], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axLeftHand[] =
		{
			{ afT[0], ZM_AnimRotZ(0.0f) }, { afT[1], ZM_AnimRotZ(-5.0f) },
			{ afT[2], ZM_AnimRotZ(-12.0f) }, { afT[3], ZM_AnimRotZ(-8.0f) },
			{ afT[4], ZM_AnimRotZ(-12.0f) }, { afT[5], ZM_AnimRotZ(0.0f) },
		};
		const ZM_AnimRotKey axRightUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(8.0f), ZM_AnimRotZ(20.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-15.0f), ZM_AnimRotZ(155.0f)) },
			{ afT[3], ZM_AnimRotCompose(ZM_AnimRotX(-10.0f), ZM_AnimRotZ(145.0f)) },
			{ afT[4], ZM_AnimRotCompose(ZM_AnimRotX(-15.0f), ZM_AnimRotZ(160.0f)) },
			{ afT[5], xIdentity },
		};
		const ZM_AnimRotKey axRightHand[] =
		{
			{ afT[0], ZM_AnimRotZ(0.0f) }, { afT[1], ZM_AnimRotZ(5.0f) },
			{ afT[2], ZM_AnimRotZ(12.0f) }, { afT[3], ZM_AnimRotZ(8.0f) },
			{ afT[4], ZM_AnimRotZ(12.0f) }, { afT[5], ZM_AnimRotZ(0.0f) },
		};

		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_SPINE, axSpine, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_NECK, axNeck, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_HEAD, axHead, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_UPPER_ARM, axLeftUpper, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_LOWER_ARM, axLower, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_HAND, axLeftHand, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, axRightUpper, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, axLower, 6u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_HAND, axRightHand, 6u);
	}

	void ZM_HumanBuildHurt(Flux_AnimationClip& xOut)
	{
		const float afT[] =
		{
			ZM_HumanTick(xOut, 0.0f), ZM_HumanTick(xOut, 0.22f),
			ZM_HumanTick(xOut, 0.55f), ZM_HumanTick(xOut, 1.0f),
		};
		const Quat xIdentity = ZM_HumanIdentity();
		const ZM_AnimRotKey axSpine[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-18.0f), ZM_AnimRotY(8.0f)) },
			{ afT[2], ZM_AnimRotX(6.0f) }, { afT[3], xIdentity },
		};
		const ZM_AnimRotKey axNeck[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-12.0f) },
			{ afT[2], ZM_AnimRotX(4.0f) }, { afT[3], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axHead[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-24.0f), ZM_AnimRotZ(8.0f)) },
			{ afT[2], ZM_AnimRotX(6.0f) }, { afT[3], xIdentity },
		};
		const ZM_AnimRotKey axLeftUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-28.0f), ZM_AnimRotZ(-18.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-8.0f), ZM_AnimRotZ(6.0f)) },
			{ afT[3], xIdentity },
		};
		const ZM_AnimRotKey axLower[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-45.0f) },
			{ afT[2], ZM_AnimRotX(-10.0f) }, { afT[3], ZM_AnimRotX(0.0f) },
		};
		const ZM_AnimRotKey axRightUpper[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-28.0f), ZM_AnimRotZ(18.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-8.0f), ZM_AnimRotZ(-6.0f)) },
			{ afT[3], xIdentity },
		};

		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_SPINE, axSpine, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_NECK, axNeck, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_HEAD, axHead, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_UPPER_ARM, axLeftUpper, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_LOWER_ARM, axLower, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, axRightUpper, 4u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, axLower, 4u);
	}

	void ZM_HumanBuildFaint(Flux_AnimationClip& xOut)
	{
		const float afT[] =
		{
			ZM_HumanTick(xOut, 0.0f), ZM_HumanTick(xOut, 0.32f),
			ZM_HumanTick(xOut, 0.62f), ZM_HumanTick(xOut, 0.86f),
			ZM_HumanTick(xOut, 1.0f),
		};
		const Quat xIdentity = ZM_HumanIdentity();

		const Quat xSpineHold = ZM_AnimRotCompose(ZM_AnimRotX(65.0f), ZM_AnimRotZ(8.0f));
		const ZM_AnimRotKey axSpine[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotX(12.0f) },
			{ afT[2], ZM_AnimRotX(38.0f) },
			{ afT[3], xSpineHold }, { afT[4], xSpineHold },
		};
		const Quat xNeckHold = ZM_AnimRotX(42.0f);
		const ZM_AnimRotKey axNeck[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(8.0f) },
			{ afT[2], ZM_AnimRotX(24.0f) },
			{ afT[3], xNeckHold }, { afT[4], xNeckHold },
		};
		const Quat xHeadHold = ZM_AnimRotCompose(ZM_AnimRotX(62.0f), ZM_AnimRotY(10.0f));
		const ZM_AnimRotKey axHead[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotX(18.0f) },
			{ afT[2], ZM_AnimRotX(45.0f) },
			{ afT[3], xHeadHold }, { afT[4], xHeadHold },
		};

		const Quat xLeftUpperArmHold = ZM_AnimRotZ(-55.0f);
		const ZM_AnimRotKey axLeftUpperArm[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-12.0f), ZM_AnimRotZ(-8.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-30.0f), ZM_AnimRotZ(-25.0f)) },
			{ afT[3], xLeftUpperArmHold }, { afT[4], xLeftUpperArmHold },
		};
		const Quat xLeftLowerArmHold = ZM_AnimRotX(-58.0f);
		const ZM_AnimRotKey axLeftLowerArm[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-18.0f) },
			{ afT[2], ZM_AnimRotX(-42.0f) },
			{ afT[3], xLeftLowerArmHold }, { afT[4], xLeftLowerArmHold },
		};
		const Quat xLeftHandHold = ZM_AnimRotCompose(ZM_AnimRotY(-20.0f), ZM_AnimRotZ(-25.0f));
		const ZM_AnimRotKey axLeftHand[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotZ(-5.0f) },
			{ afT[2], ZM_AnimRotZ(-18.0f) },
			{ afT[3], xLeftHandHold }, { afT[4], xLeftHandHold },
		};

		const Quat xRightUpperArmHold = ZM_AnimRotZ(65.0f);
		const ZM_AnimRotKey axRightUpperArm[] =
		{
			{ afT[0], xIdentity },
			{ afT[1], ZM_AnimRotCompose(ZM_AnimRotX(-14.0f), ZM_AnimRotZ(8.0f)) },
			{ afT[2], ZM_AnimRotCompose(ZM_AnimRotX(-34.0f), ZM_AnimRotZ(28.0f)) },
			{ afT[3], xRightUpperArmHold }, { afT[4], xRightUpperArmHold },
		};
		const Quat xRightLowerArmHold = ZM_AnimRotX(-64.0f);
		const ZM_AnimRotKey axRightLowerArm[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-20.0f) },
			{ afT[2], ZM_AnimRotX(-46.0f) },
			{ afT[3], xRightLowerArmHold }, { afT[4], xRightLowerArmHold },
		};
		const Quat xRightHandHold = ZM_AnimRotCompose(ZM_AnimRotY(20.0f), ZM_AnimRotZ(28.0f));
		const ZM_AnimRotKey axRightHand[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotZ(5.0f) },
			{ afT[2], ZM_AnimRotZ(20.0f) },
			{ afT[3], xRightHandHold }, { afT[4], xRightHandHold },
		};

		const Quat xLeftUpperLegHold = ZM_AnimRotCompose(ZM_AnimRotX(-52.0f), ZM_AnimRotZ(-7.0f));
		const ZM_AnimRotKey axLeftUpperLeg[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotX(-12.0f) },
			{ afT[2], ZM_AnimRotX(-32.0f) },
			{ afT[3], xLeftUpperLegHold }, { afT[4], xLeftUpperLegHold },
		};
		const Quat xLeftLowerLegHold = ZM_AnimRotX(82.0f);
		const ZM_AnimRotKey axLeftLowerLeg[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(18.0f) },
			{ afT[2], ZM_AnimRotX(52.0f) },
			{ afT[3], xLeftLowerLegHold }, { afT[4], xLeftLowerLegHold },
		};
		const Quat xLeftFootHold = ZM_AnimRotX(-25.0f);
		const ZM_AnimRotKey axLeftFoot[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-5.0f) },
			{ afT[2], ZM_AnimRotX(-15.0f) },
			{ afT[3], xLeftFootHold }, { afT[4], xLeftFootHold },
		};

		const Quat xRightUpperLegHold = ZM_AnimRotCompose(ZM_AnimRotX(-58.0f), ZM_AnimRotZ(9.0f));
		const ZM_AnimRotKey axRightUpperLeg[] =
		{
			{ afT[0], xIdentity }, { afT[1], ZM_AnimRotX(-14.0f) },
			{ afT[2], ZM_AnimRotX(-36.0f) },
			{ afT[3], xRightUpperLegHold }, { afT[4], xRightUpperLegHold },
		};
		const Quat xRightLowerLegHold = ZM_AnimRotX(88.0f);
		const ZM_AnimRotKey axRightLowerLeg[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(20.0f) },
			{ afT[2], ZM_AnimRotX(58.0f) },
			{ afT[3], xRightLowerLegHold }, { afT[4], xRightLowerLegHold },
		};
		const Quat xRightFootHold = ZM_AnimRotX(-28.0f);
		const ZM_AnimRotKey axRightFoot[] =
		{
			{ afT[0], ZM_AnimRotX(0.0f) }, { afT[1], ZM_AnimRotX(-6.0f) },
			{ afT[2], ZM_AnimRotX(-17.0f) },
			{ afT[3], xRightFootHold }, { afT[4], xRightFootHold },
		};

		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_SPINE, axSpine, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_NECK, axNeck, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_HEAD, axHead, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_UPPER_ARM, axLeftUpperArm, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_LOWER_ARM, axLeftLowerArm, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_HAND, axLeftHand, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_ARM, axRightUpperArm, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_ARM, axRightLowerArm, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_HAND, axRightHand, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_UPPER_LEG, axLeftUpperLeg, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_LOWER_LEG, axLeftLowerLeg, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_LEFT_FOOT, axLeftFoot, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_UPPER_LEG, axRightUpperLeg, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_LOWER_LEG, axRightLowerLeg, 5u);
		ZM_AnimAddRotKeys(xOut, szZM_HUMAN_RIGHT_FOOT, axRightFoot, 5u);
	}
}

void ZM_BuildHumanClip(ZM_HUMAN_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
{
	Zenith_Assert(eClip < ZM_HUMAN_CLIP_COUNT,
		"ZM_BuildHumanClip: bad clip %u", static_cast<u_int>(eClip));
	if (eClip >= ZM_HUMAN_CLIP_COUNT)
	{
		return;
	}

	xOut.SetName(ZM_HumanClipName(eClip));
	xOut.SetDuration(ZM_HumanClipDurationSeconds(eClip));
	xOut.SetTicksPerSecond(uZM_CREATURE_ANIM_TICKS_PER_SECOND);
	xOut.SetLooping(ZM_HumanClipLooping(eClip));

	switch (eClip)
	{
	case ZM_HUMAN_CLIP_IDLE:
		ZM_HumanBuildIdle(xOut);
		break;
	case ZM_HUMAN_CLIP_WALK:
		ZM_HumanBuildWalk(xOut);
		break;
	case ZM_HUMAN_CLIP_RUN:
		ZM_HumanBuildRun(xOut);
		break;
	case ZM_HUMAN_CLIP_TALK:
		ZM_HumanBuildTalk(xOut);
		break;
	case ZM_HUMAN_CLIP_WAVE:
		ZM_HumanBuildWave(xOut);
		break;
	case ZM_HUMAN_CLIP_POINT:
		ZM_HumanBuildPoint(xOut);
		break;
	case ZM_HUMAN_CLIP_CHEER:
		ZM_HumanBuildCheer(xOut);
		break;
	case ZM_HUMAN_CLIP_HURT:
		ZM_HumanBuildHurt(xOut);
		break;
	case ZM_HUMAN_CLIP_FAINT:
		ZM_HumanBuildFaint(xOut);
		break;
	default:
		Zenith_Assert(false, "ZM_BuildHumanClip: bad clip %u",
			static_cast<u_int>(eClip));
		break;
	}
}
