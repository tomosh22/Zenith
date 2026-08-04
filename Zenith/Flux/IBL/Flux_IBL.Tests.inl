#include "UnitTests/Zenith_UnitTests.h"
#include "Core/Zenith_CommandLine.h"   // ResolveUnderAssetsRoot — shader-source scan below
#include "FileAccess/Zenith_FileAccess.h"  // shader-source read (APK-aware on Android)
#include <string>

ZENITH_TEST(IBLRegeneration, AmortizedCursorConvergesInSixFrames)
{
	bool bDirty = true;
	IBL_RegenState eState = IBL_REGEN_IDLE;
	u_int uFace = 0u;
	u_int uMip = 0u;
	u_int uIrradiancePasses = 0u;
	u_int uPrefilterPasses = 0u;
	u_int uFrames = 0u;

	while (bDirty && uFrames < 20u)
	{
		for (u_int u = 0u; u < IBLConfig::uPASSES_PER_FRAME; u++)
		{
			const Flux_IBLRegen::Pass xPass = Flux_IBLRegen::Next(bDirty, eState, uFace, uMip);
			if (xPass.m_eType == Flux_IBLRegen::PASS_NONE) break;
			if (xPass.m_eType == Flux_IBLRegen::PASS_IRRADIANCE) uIrradiancePasses++;
			if (xPass.m_eType == Flux_IBLRegen::PASS_PREFILTER) uPrefilterPasses++;
		}
		uFrames++;
	}

	ZENITH_ASSERT_EQ(uFrames, 6u);
	ZENITH_ASSERT_EQ(uIrradiancePasses, 6u);
	ZENITH_ASSERT_EQ(uPrefilterPasses, 42u);
	ZENITH_ASSERT_FALSE(bDirty);
	ZENITH_ASSERT_TRUE(eState == IBL_REGEN_IDLE);
}

ZENITH_TEST(IBLRegeneration, DirtyDuringPrefilterRestartsAtIrradianceZero)
{
	bool bDirty = true;
	IBL_RegenState eState = IBL_REGEN_PREFILTER;
	u_int uFace = 4u;
	u_int uMip = 5u;
	Flux_IBLRegen::MarkDirty(false, bDirty, eState, uFace, uMip);
	ZENITH_ASSERT_TRUE(eState == IBL_REGEN_IRRADIANCE);
	ZENITH_ASSERT_EQ(uFace, 0u);
	ZENITH_ASSERT_EQ(uMip, 0u);
	const Flux_IBLRegen::Pass xFirst = Flux_IBLRegen::Next(bDirty, eState, uFace, uMip);
	ZENITH_ASSERT_TRUE(xFirst.m_eType == Flux_IBLRegen::PASS_IRRADIANCE);
	ZENITH_ASSERT_EQ(xFirst.m_uFace, 0u);
}

ZENITH_TEST(IBLRegeneration, RuntimeDirtyKeepsReadyLatched)
{
	Flux_IBLImpl xIBL;
	xIBL.m_bFirstGeneration = false;
	xIBL.m_bIBLReady = true;
	xIBL.m_bSkyIBLDirty = true;
	xIBL.m_eRegenState = IBL_REGEN_PREFILTER;
	xIBL.m_uRegenFace = 3u;
	xIBL.m_uRegenMip = 2u;
	xIBL.MarkAllProbesDirty();
	ZENITH_ASSERT_TRUE(xIBL.IsReady());
	ZENITH_ASSERT_TRUE(xIBL.m_eRegenState == IBL_REGEN_IRRADIANCE);
	ZENITH_ASSERT_EQ(xIBL.m_uRegenFace, 0u);
	ZENITH_ASSERT_EQ(xIBL.m_uRegenMip, 0u);
}

// ============================================================================
// Environment capture: the snapshot the passes read, and the accumulate /
// never-restart / coalesce schedule that replaced the previous-frame compare.
//
// These drive the PRODUCTION functions (RequestEnvironmentUpdate +
// TickRegenerationForFrame + Flux_IBLPassConstants::Build*) on a bare
// Flux_IBLImpl -- no GPU, no render graph -- so the scheduling contract is
// pinned deterministically rather than inferred from a windowed run.
// ============================================================================

namespace
{
	// CityBuilder's day/night geometry, exactly: CB_DayNightCycleComponent maps
	// clock -> Zenith_SunComponent::SetTimeOfDayAngleDegrees((timeOfDay-0.25)*360),
	// and Zenith_SunComponent::GetWorldDirection resolves a 0-azimuth orbit to
	// (-cos(orbit), -sin(orbit), 0). Mirrored here so the simulated day is the
	// real one and not a convenient approximation.
	Zenith_Maths::Vector3 IBLTestSunDirForOrbitDegrees(float fDegrees)
	{
		const float fOrbit = glm::radians(fDegrees);
		return Zenith_Maths::Vector3(-cosf(fOrbit), -sinf(fOrbit), 0.0f);
	}

	Flux_IBLEnvironmentSnapshot IBLTestSnapshotAtOrbit(float fDegrees)
	{
		Flux_IBLEnvironmentSnapshot x;
		x.m_xSunDirection = IBLTestSunDirForOrbitDegrees(fDegrees);
		x.m_fSunIntensity = 7.0f;
		return x;
	}

	// One engine frame's worth of IBL state-machine work, in the production
	// order: Flux_GraphicsImpl::UploadFrameConstants offers the live
	// environment, then Flux_IBLImpl::UpdateGraphPassEnables ticks the machine.
	Flux_IBLRegenFrameWork IBLTestSimulateFrame(Flux_IBLImpl& xIBL,
		const Flux_IBLEnvironmentSnapshot& xLive)
	{
		xIBL.RequestEnvironmentUpdate(xLive);
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
		return xWork;
	}

	// A post-boot IBL: BRDF LUT + first generation already done, ready latched.
	void IBLTestPrimePastFirstGeneration(Flux_IBLImpl& xIBL, float fStartOrbitDegrees)
	{
		xIBL.m_bBRDFLUTGenerated = true;
		IBLTestSimulateFrame(xIBL, IBLTestSnapshotAtOrbit(fStartOrbitDegrees)); // first generation
		IBLTestSimulateFrame(xIBL, IBLTestSnapshotAtOrbit(fStartOrbitDegrees)); // idle + ready
	}

	u_int IBLTestCountWork(const Flux_IBLRegenFrameWork& xWork)
	{
		u_int uCount = 0u;
		for (u_int uFace = 0u; uFace < 6u; uFace++)
			if (xWork.m_abIrradiance[uFace]) uCount++;
		for (u_int uMip = 0u; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
			for (u_int uFace = 0u; uFace < 6u; uFace++)
				if (xWork.m_abPrefilter[uMip][uFace]) uCount++;
		return uCount;
	}
}

ZENITH_TEST(IBLEnvironment, AuthoredAtmosphereReachesBothConvolutionPasses)
{
	// Finding 1: the irradiance AND prefilter pass data must carry the AUTHORED
	// Rayleigh scale, Mie scale and Mie-G -- not Atmosphere.slang's defaults.
	Flux_IBLEnvironmentSnapshot xEnv;
	xEnv.m_xSunDirection  = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	xEnv.m_fRayleighScale = 2.75f;   // all three deliberately NON-default
	xEnv.m_fMieScale      = 0.375f;
	xEnv.m_fMieG          = 0.42f;
	xEnv.m_fSunIntensity  = 7.0f;

	ZENITH_ASSERT_TRUE(xEnv.m_fRayleighScale != Zenith_GetDefaultAtmosphereRayleighScale(), "fixture is non-default");
	ZENITH_ASSERT_TRUE(xEnv.m_fMieScale != Zenith_GetDefaultAtmosphereMieScale(), "fixture is non-default");
	ZENITH_ASSERT_TRUE(xEnv.m_fMieG != Zenith_GetDefaultAtmosphereMieG(), "fixture is non-default");

	const Flux_IBLPassConstants::Irradiance xIrr = Flux_IBLPassConstants::BuildIrradiance(xEnv, 3u);
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fRayleighScale, 2.75f, 0.0f, "irradiance carries the authored Rayleigh scale");
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fMieScale, 0.375f, 0.0f, "irradiance carries the authored Mie scale");
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fMieG, 0.42f, 0.0f, "irradiance carries the authored Mie-G");
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_afSunDirection[1], -1.0f, 0.0f, "irradiance carries the CAPTURED sun direction");
	ZENITH_ASSERT_EQ(xIrr.m_uFaceIndex, 3u);
	ZENITH_ASSERT_EQ(xIrr.m_uUseAtmosphere, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fSunIntensity, 7.0f, 0.0f, "irradiance carries the captured radiometric anchor");

	const Flux_IBLPassConstants::Prefilter xPre = Flux_IBLPassConstants::BuildPrefilter(xEnv, 4u, 2u);
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fRayleighScale, 2.75f, 0.0f, "prefilter carries the authored Rayleigh scale");
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fMieScale, 0.375f, 0.0f, "prefilter carries the authored Mie scale");
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fMieG, 0.42f, 0.0f, "prefilter carries the authored Mie-G");
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_afSunDirection[1], -1.0f, 0.0f, "prefilter carries the CAPTURED sun direction");
	ZENITH_ASSERT_EQ(xPre.m_uFaceIndex, 2u);
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fRoughness, Flux_IBLPassConstants::RoughnessForMip(4u), 0.0f);
}

ZENITH_TEST(IBLEnvironment, EveryPassOfOneGenerationSharesOneSnapshot)
{
	// Finding 2: a generation is amortised over 6 frames. Every pass in it must
	// read the SAME sun + atmosphere, even though the live environment keeps
	// moving underneath (the old shaders read the live per-frame sun out of the
	// VIEW set, so a generation could span several sun positions).
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 20.0f);

	// Move enough to schedule a capture, then let the generation run while the
	// live sun keeps advancing every frame.
	Flux_IBLEnvironmentSnapshot xMoved = IBLTestSnapshotAtOrbit(40.0f);
	xMoved.m_fRayleighScale = 1.6f;
	xMoved.m_fMieScale      = 0.8f;
	xMoved.m_fMieG          = 0.5f;
	xIBL.RequestEnvironmentUpdate(xMoved);
	ZENITH_ASSERT_TRUE(xIBL.IsGenerationInFlight(), "a material change schedules a generation");

	const Flux_IBLEnvironmentSnapshot xFrozen = xIBL.GetActiveEnvironment();
	const u_int uGenerationsAtStart = xIBL.GetCompletedGenerationCount();
	u_int uPassesSeen = 0u;
	u_int uFrames = 0u;
	float fLiveOrbit = 40.0f;
	while (xIBL.GetCompletedGenerationCount() == uGenerationsAtStart && uFrames < 20u)
	{
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);

		// Every pass scheduled this frame builds its constants from the ACTIVE
		// snapshot -- assert those constants against the snapshot frozen before
		// the generation started.
		for (u_int uFace = 0u; uFace < 6u; uFace++)
		{
			if (!xWork.m_abIrradiance[uFace]) continue;
			const Flux_IBLPassConstants::Irradiance x =
				Flux_IBLPassConstants::BuildIrradiance(xIBL.GetActiveEnvironment(), uFace);
			ZENITH_ASSERT_EQ_FLOAT(x.m_afSunDirection[0], xFrozen.m_xSunDirection.x, 0.0f, "irradiance sun frozen");
			ZENITH_ASSERT_EQ_FLOAT(x.m_afSunDirection[1], xFrozen.m_xSunDirection.y, 0.0f, "irradiance sun frozen");
			ZENITH_ASSERT_EQ_FLOAT(x.m_fRayleighScale, xFrozen.m_fRayleighScale, 0.0f, "irradiance medium frozen");
			ZENITH_ASSERT_EQ_FLOAT(x.m_fMieG, xFrozen.m_fMieG, 0.0f, "irradiance Mie-G frozen");
			uPassesSeen++;
		}
		for (u_int uMip = 0u; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		{
			for (u_int uFace = 0u; uFace < 6u; uFace++)
			{
				if (!xWork.m_abPrefilter[uMip][uFace]) continue;
				const Flux_IBLPassConstants::Prefilter x =
					Flux_IBLPassConstants::BuildPrefilter(xIBL.GetActiveEnvironment(), uMip, uFace);
				ZENITH_ASSERT_EQ_FLOAT(x.m_afSunDirection[0], xFrozen.m_xSunDirection.x, 0.0f, "prefilter sun frozen");
				ZENITH_ASSERT_EQ_FLOAT(x.m_afSunDirection[1], xFrozen.m_xSunDirection.y, 0.0f, "prefilter sun frozen");
				ZENITH_ASSERT_EQ_FLOAT(x.m_fMieScale, xFrozen.m_fMieScale, 0.0f, "prefilter medium frozen");
				ZENITH_ASSERT_EQ_FLOAT(x.m_fMieG, xFrozen.m_fMieG, 0.0f, "prefilter Mie-G frozen");
				uPassesSeen++;
			}
		}

		// The live environment keeps moving DURING the generation (it must not
		// disturb the frozen snapshot). Skip the offer on the completing frame:
		// once the generation is done the schedule is free to re-target, and
		// this test is only about the passes INSIDE one generation.
		fLiveOrbit += 3.0f;
		if (xIBL.GetCompletedGenerationCount() == uGenerationsAtStart)
		{
			xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(fLiveOrbit));
		}
		uFrames++;
	}

	ZENITH_ASSERT_EQ(uPassesSeen, 48u, "one complete generation is 6 irradiance + 42 prefilter passes");
	ZENITH_ASSERT_EQ(uFrames, 6u, "48 passes at 8/frame converge in 6 frames");
}

ZENITH_TEST(IBLEnvironment, SmallPerFrameMovementAccumulatesUntilCaptured)
{
	// Finding 2, the core defect: 0.05 deg/frame (CityBuilder's 120 s day at
	// 60 FPS) is below the ~0.081 deg threshold, so a PREVIOUS-FRAME baseline
	// could never trigger. Measured against the last CAPTURED target it must
	// accumulate and fire on the second frame.
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);
	const u_int uBaseGenerations = xIBL.GetCompletedGenerationCount();

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(0.05f));
	ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight(), "one 0.05 deg step is below the capture threshold");

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(0.10f));
	ZENITH_ASSERT_TRUE(xIBL.IsGenerationInFlight(),
		"displacement accumulates against the CAPTURED target, so two steps cross it");
	ZENITH_ASSERT_EQ(xIBL.GetCompletedGenerationCount(), uBaseGenerations,
		"scheduling is not completing");
}

ZENITH_TEST(IBLEnvironment, SimulatedDayAt60FpsKeepsCompletingGenerations)
{
	// CityBuilder's default 120 s day at 60 FPS. Under the old previous-frame
	// compare this produced ZERO IBL updates for the whole day.
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);
	const u_int uBaseGenerations = xIBL.GetCompletedGenerationCount();

	// ★ THE ANGULAR STEP IS THE SCENARIO; THE FRAME COUNT IS ONLY THE HORIZON.
	// The defect this pins depends on 0.05 deg/FRAME (below the ~0.081 deg capture
	// threshold, so a previous-frame baseline could never fire) -- which is fixed by
	// fDt * fDegreesPerSecond and is UNCHANGED here. Simulating a fifth of the day
	// keeps that step exactly and still separates "many generations" from the bug's
	// ZERO by a wide margin; the full 7200 frames cost 13 s, ~10% of the entire
	// engine unit suite. Do NOT "simplify" this by changing the day length or dt --
	// that would change the deg/frame the whole test is about.
	const float fDt = 1.0f / 60.0f;
	const float fDegreesPerSecond = 360.0f / 120.0f;  // one 120 s day
	const u_int uFrames = 1440u;                      // 24 s of that day at 60 FPS
	u_int uMaxIdleRun = 0u;
	u_int uIdleRun = 0u;
	for (u_int uFrame = 0u; uFrame < uFrames; uFrame++)
	{
		const float fOrbit = static_cast<float>(uFrame) * fDt * fDegreesPerSecond;
		const Flux_IBLRegenFrameWork xWork = IBLTestSimulateFrame(xIBL, IBLTestSnapshotAtOrbit(fOrbit));
		if (IBLTestCountWork(xWork) == 0u) { uIdleRun++; uMaxIdleRun = uIdleRun > uMaxIdleRun ? uIdleRun : uMaxIdleRun; }
		else                               { uIdleRun = 0u; }
	}

	// Threshold scaled with the horizon (was >100 over 7200 frames). The bug being
	// pinned produced ZERO, so any lower bound well above zero separates it.
	const u_int uCompleted = xIBL.GetCompletedGenerationCount() - uBaseGenerations;
	ZENITH_ASSERT_GT(uCompleted, 20u, "a moving sun must keep completing IBL generations at 60 FPS");
	ZENITH_ASSERT_LT(uMaxIdleRun, 12u, "regeneration must never stall for long while the sun is moving");
	// The capture must TRACK the sun, not lag behind it. Derived from uFrames, so it
	// follows the horizon automatically.
	const Zenith_Maths::Vector3 xFinalOrbit = IBLTestSunDirForOrbitDegrees(
		static_cast<float>(uFrames - 1u) * fDt * fDegreesPerSecond);
	ZENITH_ASSERT_GT(glm::dot(xIBL.GetActiveEnvironment().m_xSunDirection, xFinalOrbit), 0.9999f,
		"the captured target advanced with the day");
}

ZENITH_TEST(IBLEnvironment, SimulatedDayAt30FpsNeitherRestartsNorStarves)
{
	// 0.1 deg/frame crosses the threshold EVERY frame. Under the old code that
	// re-armed MarkDirty every frame, rewinding the 48-pass cursor to face 0
	// before it could finish -- so a generation never completed. The coalescing
	// schedule must complete them at the same 6-frame cadence as 60 FPS.
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);
	const u_int uBaseGenerations = xIBL.GetCompletedGenerationCount();

	// Same reasoning as the 60 FPS case: 0.1 deg/FRAME is the scenario (it crosses
	// the threshold EVERY frame, which is what used to rewind the 48-pass cursor),
	// and that is set by fDt * fDegreesPerSecond, not by the horizon. The
	// starts == completions assertion below is a per-generation identity, so it
	// holds over any window long enough to contain several generations.
	const float fDt = 1.0f / 30.0f;
	const float fDegreesPerSecond = 360.0f / 120.0f;
	const u_int uFrames = 720u;                        // 24 s of the day at 30 FPS
	u_int uIrradianceFaceZeroStarts = 0u;
	for (u_int uFrame = 0u; uFrame < uFrames; uFrame++)
	{
		const float fOrbit = static_cast<float>(uFrame) * fDt * fDegreesPerSecond;
		const Flux_IBLRegenFrameWork xWork = IBLTestSimulateFrame(xIBL, IBLTestSnapshotAtOrbit(fOrbit));
		if (xWork.m_abIrradiance[0]) uIrradianceFaceZeroStarts++;
	}
	// Drain whatever was still in flight (and the one promotion it may trigger)
	// with no further live offers, so starts and completions are comparable.
	for (u_int u = 0u; u < 40u; u++)
	{
		if (!xIBL.IsGenerationInFlight() && !xIBL.HasPendingEnvironment()) break;
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
		if (xWork.m_abIrradiance[0]) uIrradianceFaceZeroStarts++;
	}
	ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight(), "the schedule drains once the sun stops moving");
	ZENITH_ASSERT_FALSE(xIBL.HasPendingEnvironment(), "nothing is left queued");

	const u_int uCompleted = xIBL.GetCompletedGenerationCount() - uBaseGenerations;
	ZENITH_ASSERT_GT(uCompleted, 20u, "generations must still COMPLETE at 30 FPS");
	// A restart re-runs irradiance face 0 without completing, so starts and
	// completions would diverge. They must match exactly.
	ZENITH_ASSERT_EQ(uIrradianceFaceZeroStarts, uCompleted,
		"every started generation completed -- no restart, no starvation");
}

ZENITH_TEST(IBLEnvironment, ReadyLatchesAndHoldsUnderContinuousMotion)
{
	// IsReady() gates m_bIBLEnabled in DeferredShading + Translucency, so an
	// idle-only latch would mean NO ambient IBL for the whole of a moving day
	// (the schedule is essentially never idle once the sun moves fast enough to
	// re-trigger every frame). It must latch on the first COMPLETE generation
	// and stay latched.
	Flux_IBLImpl xIBL;
	xIBL.m_bBRDFLUTGenerated = true;
	ZENITH_ASSERT_FALSE(xIBL.IsReady(), "not ready before any generation completes");

	// 0.5 deg/frame -- far above the threshold, so a capture is scheduled EVERY
	// frame and the machine is never idle after the first generation.
	const float fDegreesPerFrame = 0.5f;
	for (u_int uFrame = 0u; uFrame < 60u; uFrame++)
	{
		IBLTestSimulateFrame(xIBL, IBLTestSnapshotAtOrbit(static_cast<float>(uFrame) * fDegreesPerFrame));
		if (uFrame >= 1u)
		{
			ZENITH_ASSERT_TRUE(xIBL.IsReady(),
				"ready latches on the first complete generation and never unlatches");
		}
	}
	ZENITH_ASSERT_GT(xIBL.GetCompletedGenerationCount(), 5u,
		"generations keep completing while the sun re-triggers every frame");
}

ZENITH_TEST(IBLEnvironment, MidGenerationChangesCoalesceIntoOneLatestPending)
{
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(10.0f));
	ZENITH_ASSERT_TRUE(xIBL.IsGenerationInFlight());
	const Flux_IBLEnvironmentSnapshot xFrozen = xIBL.GetActiveEnvironment();

	// Advance two frames so the cursor is genuinely mid-generation (8 passes a
	// frame: frame 1 finishes irradiance and starts prefilter). Without this the
	// "cursor was not rewound" assertion below would pass vacuously.
	for (u_int u = 0u; u < 2u; u++)
	{
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
	}
	const IBL_RegenState eStateMid = xIBL.m_eRegenState;
	const u_int uFaceMid = xIBL.m_uRegenFace;
	const u_int uMipMid  = xIBL.m_uRegenMip;
	ZENITH_ASSERT_TRUE(eStateMid == IBL_REGEN_PREFILTER, "two frames of 8 passes reach the prefilter phase");
	ZENITH_ASSERT_TRUE(uFaceMid != 0u || uMipMid != 0u, "the cursor has genuinely advanced");

	// Three further changes arrive mid-generation; only the LATEST is kept, the
	// in-flight snapshot is untouched, and the cursor does NOT rewind.
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(20.0f));
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(30.0f));
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(40.0f));
	ZENITH_ASSERT_TRUE(xIBL.HasPendingEnvironment(), "mid-generation changes are coalesced, not dropped");
	ZENITH_ASSERT_NEAR_VEC3(xIBL.GetActiveEnvironment().m_xSunDirection, xFrozen.m_xSunDirection, 0.0f,
		"the in-flight snapshot is immutable");
	ZENITH_ASSERT_NEAR_VEC3(xIBL.GetPendingEnvironment().m_xSunDirection,
		IBLTestSunDirForOrbitDegrees(40.0f), 0.0001f, "only the LATEST pending target is kept");
	ZENITH_ASSERT_TRUE(xIBL.m_eRegenState == eStateMid, "the phase was not rewound");
	ZENITH_ASSERT_EQ(xIBL.m_uRegenFace, uFaceMid, "the face cursor was not rewound");
	ZENITH_ASSERT_EQ(xIBL.m_uRegenMip, uMipMid, "the mip cursor was not rewound");
}

ZENITH_TEST(IBLEnvironment, CompletionStartsTheLatestPendingGeneration)
{
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(10.0f));
	const u_int uGenerationsAtStart = xIBL.GetCompletedGenerationCount();
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(45.0f));   // coalesced pending

	// Run the generation out with NO further live offers.
	u_int uFrames = 0u;
	while (xIBL.GetCompletedGenerationCount() == uGenerationsAtStart && uFrames < 20u)
	{
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
		uFrames++;
	}
	ZENITH_ASSERT_EQ(uFrames, 6u, "the first generation completed on its own schedule");

	// Publication is deferred by one tick ON PURPOSE: the completing frame's 8
	// passes have not been recorded yet, so promoting the pending target inside
	// that frame would hand them the NEXT generation's sky.
	ZENITH_ASSERT_TRUE(xIBL.HasPendingEnvironment(), "the pending target survives the completing frame");
	ZENITH_ASSERT_NEAR_VEC3(xIBL.GetActiveEnvironment().m_xSunDirection,
		IBLTestSunDirForOrbitDegrees(10.0f), 0.0001f,
		"the completing frame still sees the generation it just finished");

	Flux_IBLRegenFrameWork xPublishTick;
	xIBL.TickRegenerationForFrame(xPublishTick);

	// The next tick promoted the pending target and started the next generation.
	ZENITH_ASSERT_FALSE(xIBL.HasPendingEnvironment(), "the pending target was consumed");
	ZENITH_ASSERT_TRUE(xIBL.IsGenerationInFlight(), "completion started the pending generation");
	ZENITH_ASSERT_NEAR_VEC3(xIBL.GetActiveEnvironment().m_xSunDirection,
		IBLTestSunDirForOrbitDegrees(45.0f), 0.0001f, "the new generation targets the latest pending snapshot");
}

ZENITH_TEST(IBLEnvironment, CompletionWithoutAPendingTargetStopsCleanly)
{
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(10.0f));
	for (u_int u = 0u; u < 6u; u++)
	{
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
	}
	ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight(), "a static environment converges to idle");
	ZENITH_ASSERT_FALSE(xIBL.HasPendingEnvironment());

	// A live offer that matches the captured target schedules nothing.
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(10.0f));
	ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight(), "an unchanged environment never re-captures");
}

ZENITH_TEST(IBLEnvironment, FrontBufferOnlyPublishesCompleteGenerations)
{
	// Coherent publication: consumers sample the FRONT cube every frame while a
	// generation writes the BACK one, so the front index must hold still for the
	// whole generation and flip exactly once per completion.
	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);

	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(10.0f));
	const u_int uFrontAtStart = xIBL.GetFrontBufferIndex();
	const u_int uBackAtStart = xIBL.GetBackBufferIndex();
	ZENITH_ASSERT_TRUE(uFrontAtStart != uBackAtStart, "front and back are distinct buffers");

	for (u_int u = 0u; u < 6u; u++)
	{
		ZENITH_ASSERT_EQ(xIBL.GetFrontBufferIndex(), uFrontAtStart,
			"the front cube never changes mid-generation");
		ZENITH_ASSERT_EQ(xIBL.GetBackBufferIndex(), uBackAtStart,
			"every pass of the generation targets the same back cube");
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
	}
	ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight());

	// The swap is published on the NEXT tick (the completing frame's passes are
	// still targeting the old back index when they record).
	ZENITH_ASSERT_EQ(xIBL.GetFrontBufferIndex(), uFrontAtStart,
		"publication is deferred past the completing frame's recording");
	Flux_IBLRegenFrameWork xNext;
	xIBL.TickRegenerationForFrame(xNext);
	ZENITH_ASSERT_EQ(xIBL.GetFrontBufferIndex(), uBackAtStart,
		"the completed generation is published exactly once");
}

ZENITH_TEST(IBLEnvironment, DiffersCoversTheAuthoredMediumButNotTheAnchor)
{
	// The medium compares exactly (authored floats applied verbatim), INCLUDING
	// the Mie-G phase asymmetry -- it does not affect transmittance (see
	// Flux_AtmosphereTransmittance::TransmittanceLUTChanged) but it does change
	// the sky radiance the cubes integrate. The radiometric anchor is policy,
	// never a runtime look control, so it is deliberately not compared.
	Flux_IBLEnvironmentSnapshot xA = IBLTestSnapshotAtOrbit(45.0f);
	Flux_IBLEnvironmentSnapshot xB = xA;
	ZENITH_ASSERT_FALSE(Flux_IBLEnvironment::Differs(xA, xB), "identical snapshots do not differ");

	xB = xA; xB.m_fRayleighScale += 0.25f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xA, xB), "Rayleigh scale change");
	xB = xA; xB.m_fMieScale += 0.25f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xA, xB), "Mie scale change");
	xB = xA; xB.m_fMieG += 0.04f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xA, xB), "Mie-G change");
	xB = xA; xB.m_fSunIntensity *= 2.0f;
	ZENITH_ASSERT_FALSE(Flux_IBLEnvironment::Differs(xA, xB),
		"the radiometric anchor is not a capture input -- it cannot schedule work");

	xB = IBLTestSnapshotAtOrbit(45.04f);   // below the ~0.081 deg threshold
	ZENITH_ASSERT_FALSE(Flux_IBLEnvironment::Differs(xA, xB), "sub-threshold sun motion");
	xB = IBLTestSnapshotAtOrbit(45.2f);
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xA, xB), "supra-threshold sun motion");
}

// ============================================================================
// Per-game capture budget: the threshold and the passes-per-frame are
// Zenith_GraphicsOptions now, not compile-time constants. One value could never
// serve both a 120 s day (3 deg/s) and a 40-minute day.
// ============================================================================

namespace
{
	// RAII: the graphics options are process-global, so a test that changes them
	// must put them back however it exits.
	struct IBLTestScopedCaptureBudget
	{
		float    m_fPrevDegrees;
		uint32_t m_uPrevPasses;
		IBLTestScopedCaptureBudget(float fDegrees, uint32_t uPasses)
			: m_fPrevDegrees(Zenith_GraphicsOptions::Get().m_fIBLCaptureThresholdDegrees)
			, m_uPrevPasses(Zenith_GraphicsOptions::Get().m_uIBLPassesPerFrame)
		{
			Zenith_GraphicsOptions::Get().m_fIBLCaptureThresholdDegrees = fDegrees;
			Zenith_GraphicsOptions::Get().m_uIBLPassesPerFrame = uPasses;
		}
		~IBLTestScopedCaptureBudget()
		{
			Zenith_GraphicsOptions::Get().m_fIBLCaptureThresholdDegrees = m_fPrevDegrees;
			Zenith_GraphicsOptions::Get().m_uIBLPassesPerFrame = m_uPrevPasses;
		}
	};
}

ZENITH_TEST(IBLEnvironment, CaptureThresholdConversionAndClamping)
{
	namespace E = Flux_IBLEnvironment;
	// The historical constant IS the default, so an opt-out game is unchanged.
	ZENITH_ASSERT_EQ_FLOAT(E::CosThresholdFromDegrees(0.0811f),
		E::fDEFAULT_SUN_DIRECTION_DOT_EPSILON, 0.0000005f,
		"the default degrees reproduce the historical dot epsilon");
	// Larger angle => smaller cosine => less sensitive.
	ZENITH_ASSERT_LT(E::CosThresholdFromDegrees(5.0f), E::CosThresholdFromDegrees(0.5f),
		"a looser threshold is a smaller cosine");
	// Guard rails.
	ZENITH_ASSERT_EQ_FLOAT(E::CosThresholdFromDegrees(-10.0f),
		E::CosThresholdFromDegrees(E::fMIN_THRESHOLD_DEGREES), 0.0f, "clamps at the floor");
	ZENITH_ASSERT_EQ_FLOAT(E::CosThresholdFromDegrees(1000.0f),
		E::CosThresholdFromDegrees(E::fMAX_THRESHOLD_DEGREES), 0.0f, "clamps at the ceiling");
}

ZENITH_TEST(IBLEnvironment, LooseThresholdSuppressesCapturesATightOneSchedules)
{
	// The same sun motion, judged by two budgets. This is the knob a fast-day
	// game reaches for so it is not re-capturing back-to-back forever.
	const Flux_IBLEnvironmentSnapshot xA = IBLTestSnapshotAtOrbit(0.0f);
	const Flux_IBLEnvironmentSnapshot xB = IBLTestSnapshotAtOrbit(1.0f);   // 1 degree

	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xA, xB,
		Flux_IBLEnvironment::CosThresholdFromDegrees(0.5f)), "1 deg crosses a 0.5 deg threshold");
	ZENITH_ASSERT_FALSE(Flux_IBLEnvironment::Differs(xA, xB,
		Flux_IBLEnvironment::CosThresholdFromDegrees(5.0f)), "1 deg does not cross a 5 deg threshold");

	// ... and end to end through the scheduler.
	{
		IBLTestScopedCaptureBudget xBudget(5.0f, 8u);
		Flux_IBLImpl xIBL;
		IBLTestPrimePastFirstGeneration(xIBL, 0.0f);
		xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(1.0f));
		ZENITH_ASSERT_FALSE(xIBL.IsGenerationInFlight(), "a loose budget ignores 1 deg of drift");
		xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(8.0f));
		ZENITH_ASSERT_TRUE(xIBL.IsGenerationInFlight(), "...but still captures once it accumulates past it");
	}
}

ZENITH_TEST(IBLEnvironment, PassBudgetChangesConvergenceRateNotCorrectness)
{
	// 48 passes at N/frame must converge in ceil(48/N) frames for any N, and the
	// generation must stay coherent throughout.
	const u_int auBudgets[3] = { 4u, 8u, 16u };
	const u_int auExpectedFrames[3] = { 12u, 6u, 3u };
	for (u_int u = 0u; u < 3u; u++)
	{
		IBLTestScopedCaptureBudget xBudget(0.0811f, auBudgets[u]);
		Flux_IBLImpl xIBL;
		IBLTestPrimePastFirstGeneration(xIBL, 0.0f);

		xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(30.0f));
		const u_int uGenAtStart = xIBL.GetCompletedGenerationCount();
		u_int uFrames = 0u;
		u_int uPasses = 0u;
		while (xIBL.GetCompletedGenerationCount() == uGenAtStart && uFrames < 100u)
		{
			Flux_IBLRegenFrameWork xWork;
			xIBL.TickRegenerationForFrame(xWork);
			uPasses += IBLTestCountWork(xWork);
			uFrames++;
		}
		ZENITH_ASSERT_EQ(uPasses, 48u, "a generation is always exactly 48 passes");
		ZENITH_ASSERT_EQ(uFrames, auExpectedFrames[u], "budget %u converges on schedule", auBudgets[u]);
	}
}

ZENITH_TEST(IBLEnvironment, ZeroPassBudgetCannotWedgeTheSchedule)
{
	// A budget of 0 would be a generation that can never advance and therefore
	// never completes -- the schedule would never drain and the IBL would be
	// permanently mid-capture. It clamps to 1 instead.
	IBLTestScopedCaptureBudget xBudget(0.0811f, 0u);
	ZENITH_ASSERT_EQ(Flux_IBLImpl::GetPassesPerFrame(), 1u, "a zero budget clamps to one pass");

	Flux_IBLImpl xIBL;
	IBLTestPrimePastFirstGeneration(xIBL, 0.0f);
	xIBL.RequestEnvironmentUpdate(IBLTestSnapshotAtOrbit(30.0f));
	const u_int uGenAtStart = xIBL.GetCompletedGenerationCount();
	for (u_int u = 0u; u < 48u; u++)
	{
		Flux_IBLRegenFrameWork xWork;
		xIBL.TickRegenerationForFrame(xWork);
	}
	ZENITH_ASSERT_GT(xIBL.GetCompletedGenerationCount(), uGenAtStart,
		"one pass per frame still completes in 48 frames");
}

// ============================================================================
// The authored medium + multiple scattering reach the capture.
// ============================================================================

ZENITH_TEST(IBLEnvironment, FullAuthoredMediumReachesBothConvolutionPasses)
{
	// Every v2 medium parameter must land in BOTH pass constant buffers. Ground
	// albedo has no other route to the GPU at all -- the visible sky passes 0.
	Flux_IBLEnvironmentSnapshot xEnv;
	xEnv.m_xSunDirection        = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	xEnv.m_fRayleighScale       = 2.75f;
	xEnv.m_fMieScale            = 0.375f;
	xEnv.m_fMieG                = 0.42f;
	xEnv.m_fRayleighScaleHeight = 10500.0f;
	xEnv.m_fMieScaleHeight      = 450.0f;
	xEnv.m_fGroundAlbedo        = 0.68f;
	xEnv.m_fSunIntensity        = 7.0f;
	xEnv.m_bMultiScattering     = true;

	ZENITH_ASSERT_TRUE(xEnv.m_fRayleighScaleHeight != Zenith_GetDefaultAtmosphereRayleighScaleHeight(), "fixture is non-default");
	ZENITH_ASSERT_TRUE(xEnv.m_fMieScaleHeight != Zenith_GetDefaultAtmosphereMieScaleHeight(), "fixture is non-default");
	ZENITH_ASSERT_TRUE(xEnv.m_fGroundAlbedo != Zenith_GetDefaultAtmosphereGroundAlbedo(), "fixture is non-default");

	const Flux_IBLPassConstants::Irradiance xIrr = Flux_IBLPassConstants::BuildIrradiance(xEnv, 3u);
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fRayleighScaleHeight, 10500.0f, 0.0f, "irradiance carries the authored Rayleigh scale height");
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fMieScaleHeight, 450.0f, 0.0f, "irradiance carries the authored Mie scale height");
	ZENITH_ASSERT_EQ_FLOAT(xIrr.m_fGroundAlbedo, 0.68f, 0.0f, "irradiance carries the authored ground albedo");
	ZENITH_ASSERT_EQ(xIrr.m_uMultiScatteringEnabled, 1u, "irradiance carries the multi-scatter flag");

	const Flux_IBLPassConstants::Prefilter xPre = Flux_IBLPassConstants::BuildPrefilter(xEnv, 4u, 2u);
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fRayleighScaleHeight, 10500.0f, 0.0f, "prefilter carries the authored Rayleigh scale height");
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fMieScaleHeight, 450.0f, 0.0f, "prefilter carries the authored Mie scale height");
	ZENITH_ASSERT_EQ_FLOAT(xPre.m_fGroundAlbedo, 0.68f, 0.0f, "prefilter carries the authored ground albedo");
	ZENITH_ASSERT_EQ(xPre.m_uMultiScatteringEnabled, 1u, "prefilter carries the multi-scatter flag");

	// The LUT bake the capture runs must describe the SAME medium.
	const Flux_AtmosphereTransmittance::MultiScatterConstants xMS =
		Flux_AtmosphereTransmittance::BuildMultiScatterConstants(
			xEnv.m_fRayleighScale, xEnv.m_fMieScale,
			xEnv.m_fRayleighScaleHeight, xEnv.m_fMieScaleHeight, xEnv.m_fGroundAlbedo);
	ZENITH_ASSERT_EQ_FLOAT(xMS.m_afRayleighScatter[3], 10500.0f, 0.0f, "the bake gets the authored scale height");
	ZENITH_ASSERT_EQ_FLOAT(xMS.m_afMieScatter[3], 450.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xMS.m_fGroundAlbedo, 0.68f, 0.0f, "the bake gets the authored ground albedo");
	// Coefficients arrive PRE-SCALED by the authored densities, like the shaders.
	ZENITH_ASSERT_EQ_FLOAT(xMS.m_afRayleighScatter[0],
		AtmosphereConfig::afRAYLEIGH_SCATTER[0] * 2.75f, 0.0f, "coefficients are density-scaled for the bake");
}

ZENITH_TEST(IBLEnvironment, EveryV2MediumParameterSchedulesACapture)
{
	// A parameter that reaches the GPU but not the invalidation predicate is
	// worse than one that reaches neither: the author edits it and nothing
	// happens until something unrelated forces a re-capture.
	const Flux_IBLEnvironmentSnapshot xBase = IBLTestSnapshotAtOrbit(45.0f);

	Flux_IBLEnvironmentSnapshot x = xBase; x.m_fRayleighScaleHeight += 500.0f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xBase, x), "Rayleigh scale height");
	x = xBase; x.m_fMieScaleHeight -= 300.0f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xBase, x), "Mie scale height");
	x = xBase; x.m_fGroundAlbedo = 0.7f;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xBase, x), "ground albedo");
	x = xBase; x.m_bMultiScattering = !xBase.m_bMultiScattering;
	ZENITH_ASSERT_TRUE(Flux_IBLEnvironment::Differs(xBase, x), "multi-scatter toggle re-captures");
}

// ============================================================================
// Source-level lock on the SHADER half of finding 1. The pass-constant tests
// above prove the CPU fills the authored medium in; this proves the .slang
// actually CONSUMES it (and no longer reads the live per-frame sun out of the
// spine), and that the defaults-only convenience overload it used to call no
// longer exists to fall back to. Text-scanning the shader source is the same
// mechanism the spine lint uses.
// ============================================================================

namespace
{
	bool IBLTestReadShaderSource(const char* szRelativePath, std::string& strOut)
	{
		// Baked absolute build-machine path, overridable by --assets-root for a
		// relocated package -- the same resolution Flux_SlangCompiler uses.
		const std::string strRoot = Zenith_CommandLine::ResolveUnderAssetsRoot(
			SHADER_SOURCE_ROOT, Zenith_CommandLine::GetAssetsRoot(), "Zenith/Flux/Shaders/");
		std::string strPath = strRoot;
		if (!strPath.empty() && strPath.back() != '/' && strPath.back() != '\\')
		{
			strPath += '/';
		}
		strPath += szRelativePath;

		// Route through Zenith_FileAccess rather than a raw ifstream. On
		// Android the shader sources ship INSIDE the APK and are reachable
		// only via AAssetManager, which is precisely what this abstraction
		// wraps; SHADER_SOURCE_ROOT is "" there, so strPath is already the
		// APK-relative asset path. A raw ifstream saw nothing, and these two
		// tests were the only failures in the whole suite on Android.
		uint64_t ulSize = 0;
		char* pcData = Zenith_FileAccess::ReadFile(strPath.c_str(), ulSize);
		if (pcData == nullptr)
		{
			return false;
		}
		strOut.assign(pcData, static_cast<size_t>(ulSize));
		Zenith_FileAccess::FreeFileData(pcData);
		return !strOut.empty();
	}

	bool IBLTestContains(const std::string& strHaystack, const char* szNeedle)
	{
		return strHaystack.find(szNeedle) != std::string::npos;
	}
}

ZENITH_TEST(IBLEnvironment, ShadersConsumeTheCapturedEnvironmentNotTheDefaults)
{
	std::string strIrradiance, strPrefilter, strAtmosphere;
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("IBL/Flux_IrradianceConvolution.slang", strIrradiance),
		"the irradiance shader source must be readable under SHADER_SOURCE_ROOT");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("IBL/Flux_PrefilterEnvMap.slang", strPrefilter),
		"the prefilter shader source must be readable under SHADER_SOURCE_ROOT");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("Common/Atmosphere.slang", strAtmosphere),
		"the shared atmosphere module must be readable under SHADER_SOURCE_ROOT");

	// The shared module offers the medium-taking capture entry point, and scales
	// the coefficients by the authored values rather than using them raw.
	ZENITH_ASSERT_TRUE(IBLTestContains(strAtmosphere, "public AtmosphereResult ComputeEnvironmentCaptureScattering"),
		"Atmosphere.slang exposes the environment-capture entry point");
	ZENITH_ASSERT_TRUE(IBLTestContains(strAtmosphere, "DEFAULT_RAYLEIGH_COEFF * xMedium.fRayleighScale"),
		"the capture path scales the Rayleigh coefficients by the authored density");
	ZENITH_ASSERT_TRUE(IBLTestContains(strAtmosphere, "DEFAULT_MIE_COEFF      * xMedium.fMieScale"),
		"the capture path scales the Mie coefficient by the authored density");
	// The defaults-only convenience overload (the one that hard-coded
	// DEFAULT_RAYLEIGH_COEFF / DEFAULT_MIE_COEFF / DEFAULT_MIE_G) is GONE, so no
	// IBL pass can silently fall back to it.
	ZENITH_ASSERT_FALSE(IBLTestContains(strAtmosphere, "AtmosphereResult ComputeAtmosphereScattering"),
		"the defaults-only IBL convenience overload must not exist");

	const char* aszShaders[2] = { "irradiance", "prefilter" };
	const std::string* apxSources[2] = { &strIrradiance, &strPrefilter };
	for (u_int u = 0u; u < 2u; u++)
	{
		const std::string& rSrc = *apxSources[u];
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "ComputeEnvironmentCaptureScattering"),
			"%s convolution calls the medium-taking capture entry point", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fRayleighScale"),
			"%s convolution threads the authored Rayleigh scale", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fMieScale"),
			"%s convolution threads the authored Mie scale", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fMieG"),
			"%s convolution threads the authored Mie-G", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_xSunDirection"),
			"%s convolution reads the CAPTURED sun direction", aszShaders[u]);
		ZENITH_ASSERT_FALSE(IBLTestContains(rSrc, "GetSunDir_Pad"),
			"%s convolution must not read the live per-frame sun out of the spine", aszShaders[u]);
		// v2 medium + multiple scattering.
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fRayleighScaleHeight"),
			"%s convolution threads the authored Rayleigh scale height", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fMieScaleHeight"),
			"%s convolution threads the authored Mie scale height", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_fGroundAlbedo"),
			"%s convolution threads the authored ground albedo", aszShaders[u]);
		ZENITH_ASSERT_TRUE(IBLTestContains(rSrc, "g_xMultiScatterLUT"),
			"%s convolution samples the CAPTURE's multi-scatter LUT", aszShaders[u]);
	}
}

ZENITH_TEST(IBLEnvironment, MultipleScatteringIsSharedMathsWithSeparateBakes)
{
	// The Skybox and the capture MUST bake separate LUTs (live medium vs frozen
	// snapshot) but MUST share the maths, or the visible sky and the ambient it
	// lights the scene with drift apart for the silliest possible reason.
	std::string strShared, strSkyBake, strIBLBake, strSkyView, strAtmosphere;
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("Common/MultiScatter.slang", strShared),
		"the shared multiple-scattering module must exist");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("Skybox/Flux_MultiScatterLUT.slang", strSkyBake),
		"the Skybox bake must exist");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("IBL/Flux_IBLMultiScatterLUT.slang", strIBLBake),
		"the capture bake must exist");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("Skybox/Flux_SkyViewLUT.slang", strSkyView), "sky-view LUT");
	ZENITH_ASSERT_TRUE(IBLTestReadShaderSource("Common/Atmosphere.slang", strAtmosphere), "atmosphere module");

	// One implementation, two thin entry points.
	ZENITH_ASSERT_TRUE(IBLTestContains(strShared, "public float3 ComputeMultiScatterPsi"),
		"the estimate lives in the shared module");
	ZENITH_ASSERT_TRUE(IBLTestContains(strSkyBake, "import Common.MultiScatter"), "the Skybox bake imports it");
	ZENITH_ASSERT_TRUE(IBLTestContains(strIBLBake, "import Common.MultiScatter"), "the capture bake imports it");
	ZENITH_ASSERT_TRUE(IBLTestContains(strSkyBake, "ComputeMultiScatterPsi"), "the Skybox bake calls it");
	ZENITH_ASSERT_TRUE(IBLTestContains(strIBLBake, "ComputeMultiScatterPsi"), "the capture bake calls it");

	// Neither bake may sample a transmittance LUT: the capture's would be a read
	// whose only writer is declared later in the setup walk (IBL is a feature
	// declared before Skybox), and the graph silently drops that barrier.
	ZENITH_ASSERT_FALSE(IBLTestContains(strIBLBake, "g_xTransmittanceLUT"),
		"the capture bake takes no cross-feature LUT dependency");
	ZENITH_ASSERT_FALSE(IBLTestContains(strSkyBake, "g_xTransmittanceLUT"),
		"the Skybox bake integrates the sun ray analytically too, for one code path");

	// Both consumers of the LUT are wired.
	ZENITH_ASSERT_TRUE(IBLTestContains(strSkyView, "g_xMultiScatterLUT"),
		"the visible sky folds in multiple scattering");
	ZENITH_ASSERT_TRUE(IBLTestContains(strAtmosphere, "SampleMultiScatterLut"),
		"the shared solver exposes the sampler");
	ZENITH_ASSERT_TRUE(IBLTestContains(strAtmosphere, "bMultiScatter"),
		"both solvers take the enable flag");

	// The authored ground albedo replaced the hard-coded constant outright.
	ZENITH_ASSERT_FALSE(IBLTestContains(strAtmosphere, "public static const float  IBL_GROUND_ALBEDO"),
		"the hard-coded capture ground albedo is gone -- it is authored now");
}
